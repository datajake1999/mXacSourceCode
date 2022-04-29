/*************************************************************************************
CMIFLVM.cpp - Code for handling the virtual machine.

begun 30/1/04 by Mike Rozak.
Copyright 2004 by Mike Rozak. All rights reserved
*/

#include <windows.h>
#include <float.h>
#include "escarpment.h"
#include "..\M3D.h"
#include "resource.h"
#include "..\mifl.h"



#define VARSSHOWN             10       // number of variables shown in the list
#define TRUNCATETO            1024     // limit lists and strings to this long
#define MAXDEBUGSIZE          10000    // maximum number of bytes for debug window

// VMTOK_XXX - Internal tokens for task access
#define VMTOK_CONSTRUCTOR        1        // constructor for objects
#define VMTOK_CONSTRUCTOR2       2        // constructor2 is called for objects re-loaded into world
#define VMTOK_DESTRUCTOR         3        // destructor for objects
#define VMTOK_NAME               4        // name of the object... used when converting object to string
            // note: VMTOK_NAME must remain 3 since copied into MIFLMisc.cpp

// string tokens
#define VMTOK_STRINGCONCAT       10       // concat 2 strings
#define VMTOK_STRINGFROMCHAR     11       // create a string from one or more characters of codes
#define VMTOK_STRINGSEARCH       12       // find nth occurance of string in set, case sensative or insensative
#define VMTOK_STRINGLENGTH       14       // string length
#define VMTOK_STRINGSLICE        15       // take from start to end char, use negative values for relative pos
#define VMTOK_STRINGSPLIT        16       // split string into list based on delimeter char
#define VMTOK_STRINGSUBSTRING    17       // extract a sub-string from the string, based on start and end
#define VMTOK_STRINGTOLOWER      18       // convert a string to lower case
#define VMTOK_STRINGTOUPPER      19       // convert a string to upper case
#define VMTOK_STRINGCOMPARE      20       // compares string based on case
#define VMTOK_STRINGINSERT       21       // insert a string at the given location
#define VMTOK_STRINGREPLACE      22       // replace the character range with new sub-string
#define VMTOK_STRINGFORMAT       23       // replace %1, %2, etc. in string
#define VMTOK_STRINGTRIM         24       // trims whitespace to left and right. Optional param for only trim from left or right
#define VMTOK_CLONE              25       // supported by strings and lists.
#define VMTOK_STRINGPREPEND      26       // put at start of existing string
#define VMTOK_STRINGAPPEND       27       // add to end of existing string

// list tokens
#define VMTOK_LISTCONCAT         30       // concatenate many values onto end of list
#define VMTOK_LISTMERGE          31       // join second list onto first
#define VMTOK_LISTNUMBER         32       // returns number of elements
#define VMTOK_LISTREMOVE         33       // removes a specific item or range
#define VMTOK_LISTREVERSE        34       // reverses the list
#define VMTOK_LISTSLICE          35       // slices, with relative values
#define VMTOK_LISTSORT           36       // sorts list. May have comparison function
#define VMTOK_LISTSEARCH         37       // searches list. May have comparison function
#define VMTOK_LISTREPLACE        38       // replace from elem start to finish, with new elem(s)
#define VMTOK_LISTINSERT         39       // insert list elements at
#define VMTOK_LISTRANDOMIZE      40       // randomize order of the list
#define VMTOK_LISTAPPEND         41       // append to list
#define VMTOK_LISTPREPEND        42       // at beginning of list
#define VMTOK_LISTSUBLIST        43       // extract sub-list
#define VMTOK_LISTSEARCHTOINSERT 44       // searches list for place to insert before
// #define VMTOK_CLONE              25       // supported by strings and lists.

// object
#define VMTOK_CONTAINEDINGET     50       // what the object is contained in
#define VMTOK_CONTAINEDINSET     51       // change what the object is contained in
#define VMTOK_CONTAINSGET        52       // returns what it contains
#define VMTOK_CLASSENUM          53       // enumerates all the classes that this is in
#define VMTOK_CLASSQUERY         54       // returns TRUE if it's a subclass of the given class
#define VMTOK_PROPERTYGET        55       // get a property via string
#define VMTOK_PROPERTYSET        56       // set a property via string
#define VMTOK_PROPERTYREMOVE     57       // remove a property via string
#define VMTOK_PROPERTYQUERY      58       // sees if property supported
#define VMTOK_PROPERTYENUM       59       // enumerates properties
#define VMTOK_METHODCALL         60       // calls a public method
#define VMTOK_METHODQUERY        61       // queries to see if a public method exists
#define VMTOK_METHODENUM         62       // enumerates public methods
#define VMTOK_DELETETREE         63       // deletes the object and all objects it contains
#define VMTOK_LAYERNUM           64       // returns the number of layers
#define VMTOK_LAYERGET           65       // gets a layer
#define VMTOK_LAYERADD           66       // adds a layer
#define VMTOK_LAYERREMOVE        67       // removes a layer
#define VMTOK_LAYERMETHODENUM    68       // enumerates the public methods in a layer, from classes or added
#define VMTOK_LAYERMETHODADD     69       // adds a method to a layer
#define VMTOK_LAYERMETHODREMOVE  70       // removes a method from a layer
#define VMTOK_LAYERPROPERTYENUM  71       // enumerates the public properties in a layer, from classes or added
#define VMTOK_LAYERPROPERTYADD   72       // adds a properties to a layer
#define VMTOK_LAYERPROPERTYREMOVE 73       // removes a property from a layer
#define VMTOK_TIMERSUSPENDSET    74       // sets the suspend state of an object's timers
#define VMTOK_TIMERSUSPENDGET    75       // gets the suspend state of an objdcts tmiers
#define VMTOK_TIMERADD           76       // adds a timer
#define VMTOK_TIMERREMOVE        77       // removes a timer
#define VMTOK_TIMERENUM          78       // enumerates a timer
#define VMTOK_TIMERQUERY         79       // sees if a timer exists


// functions exported
#define VMTOK_TRACE              100      // outputdebugstring
#define VMTOK_DELETEGROUP        101      // delete a group of objects (and their children)

#define VMTOK_TOBOOL             110      // convert to a boolean
#define VMTOK_TONUMBER           111      // convert to a number
#define VMTOK_TOCHAR             112      // convert to a character
#define VMTOK_TOSTRING           113      // convert to a string
#define VMTOK_TOMETHOD           114      // convert to a method
#define VMTOK_TOFUNCTION         115      // convert to a function
#define VMTOK_TOSTRINGMML        116      // convert to a string, but sanitize for MML
#define VMTOK_TOOBJECT           117      // convert to an object

#define VMTOK_TYPEOF             120      // convert the type to a string
#define VMTOK_LANGUAGEGET        121      // gets the current language
#define VMTOK_LANGUAGESET        122      // sets the current language
#define VMTOK_TIMEGET            123      // number of days since jan1, 2001
#define VMTOK_TIMETODATETIME     124      // convert from time to a date and time
#define VMTOK_TIMEFROMDATETIME   125      // convert from
#define VMTOK_OBJECTNEW          126      // create a new object based on a name or existing class
#define VMTOK_TIMESINCESTART     127      // time since computer started up
#define VMTOK_TIMEZONE           128      // get time zone information
#define VMTOK_OBJECTCLONE        129      // clones and object or set of objects

#define VMTOK_GLOBALENUM         130      // enumerate globals
#define VMTOK_GLOBALQUERY        131      // query if a global exists
#define VMTOK_GLOBALGET          132      // get a global
#define VMTOK_GLOBALSET          133      // set a global
#define VMTOK_GLOBALREMOVE       134      // remove a global
#define VMTOK_GLOBALGETSET       135      // control get/set code

#define VMTOK_OBJECTENUM         140      // enumerate all objects
#define VMTOK_OBJECTQUERY        141      // see if an object still exists
#define VMTOK_CLASSESENUM        142      // enumerates all the object classes in the language

#define VMTOK_MMLTOLIST          150      // converts from a MML string to a list of elements
#define VMTOK_MMLFROMLIST        151      // converts frin from a list of elements back to MML
#define VMTOK_RESOURCEGET        152      // given a resource string name, gets the resource
#define VMTOK_RESOURCEENUM       153      // enumerates all the resources

#define VMTOK_MATH_ABS           201      // absoluate value
#define VMTOK_MATH_ACOS          202      // arc cos
#define VMTOK_MATH_ASIN          203      // arc sin
#define VMTOK_MATH_ATAN          204      // arc tan
#define VMTOK_MATH_CEIL          205      // ceiling
#define VMTOK_MATH_COS           206      // cos
#define VMTOK_MATH_EXP           207      // exponent
#define VMTOK_MATH_FLOOR         208      // floor
#define VMTOK_MATH_LOG           209      // log
#define VMTOK_MATH_LOG10         210      // log 10
#define VMTOK_MATH_POW           211      // power
#define VMTOK_MATH_SQRT          212      // square root
#define VMTOK_MATH_TAN           213      // tan
#define VMTOK_MATH_ISINFINITE    214      // is # infinit
#define VMTOK_MATH_ISNAN         215      // is Nan
#define VMTOK_MATH_MAX           216
#define VMTOK_MATH_MIN           217
#define VMTOK_MATH_SIN           218      // sine
#define VMTOK_MATH_RANDOM        219
#define VMTOK_MATH_RANDOMSEED    220
#define VMTOK_MATH_COSH          221      // hyperholic cos
#define VMTOK_MATH_SINH          222      // hyperbolic sin
#define VMTOK_MATH_ROUND         223      // round off
#define VMTOK_MATH_TANH          224      // hyperbolic tan
#define VMTOK_MATH_ATAN2         225

#define VMTOK_ISBOOL             230      // returns true if bool
#define VMTOK_ISCHAR             231      // returns true if char
#define VMTOK_ISFUNCTION         232      // returns true if func
#define VMTOK_ISMETHOD           233      // returns true if method
#define VMTOK_ISNUMBER           234      // returns true if number
#define VMTOK_ISSTRING           235      // returns true if string
#define VMTOK_ISOBJECT           236      // returns true if object
#define VMTOK_ISRESOURCE         237      // returns true if resource
#define VMTOK_OBJECTTOGUIDSTRING 238      // converts an object to a guid stirng
#define VMTOK_GUIDSTRINGTOOBJECT 239      // converts a guid stirng to an object
#define VMTOK_ISLIST             240      // returns TRUE if it's a list

#define VMTOK_ISCHARALPHA        250      // returns TRUE if alphabetical char
#define VMTOK_ISCHARALPHANUM     251      // returns TRUE if alpha-numeric
#define VMTOK_ISCHARUPPER        252      // returns TRUE if upper
#define VMTOK_ISCHARLOWER        253      // returns TRUE if lower
#define VMTOK_ISCHARSPACE        254      // returns TRUE if whitespace
#define VMTOK_ISCHARDIGIT        255      // returns TRUE if digit




// VMFII - Import information
typedef struct {
   PWSTR          pszName;       // name of the imported function
   DWORD          dwVMTOK;       // token ID. VMTOK_XXX
   DWORD          dwParam;       // number of paramters
} VMFII, *PVMFII;

// TIDTOVM - So can get a timer ID and find the VM it belongs to
typedef struct {
   UINT_PTR       dwTimerID;     // timer ID from SetTimer()
   PCMIFLVM       pVM;           // VM it's associated with
} TIDTOVM, *PTIDTOVM;

static PWSTR gpszTypedCode = L"TypedCode";
static PWSTR gpszOnlyVar = L"Only variables can be accessed.";
static PWSTR gpszOutOfMemory = L"Out of memory.";
static PWSTR gpszNotEnoughParameters = L"Not enough parameters.";
static CHashString ghFuncImport;
static CListFixed glTIDTOVM;   // list of timer id's to VM


/**********************************************************************************
MMLToList - Given a MMLNode, returns a list with three elements. The first element
is the list name. The second is a list of all the attributes. The third is a list of all
the contents.

Each attribute is a sub-list with [Name, Value].

Each of the contents is a sub-list with either a string, or a sub-list which is
another MMLToList

inputs
   PCMMLNode2        pNode - Node
   PCMIFLVar         pv - Filled with the list.
returns
   none
*/
void MMLToList (PCMMLNode2 pNode, PCMIFLVar pv)
{
   PCMIFLVarList plMain = new CMIFLVarList;
   if (!plMain) {
      pv->SetNULL();
      return;
   }

   // set the name
   PWSTR psz, pszValue;
   psz = pNode->NameGet();
   if (psz)
      pv->SetString (psz);
   else
      pv->SetNULL();
   plMain->Add (pv, TRUE);

   // set the properties
   DWORD dwNum = pNode->AttribNum();
   PCMIFLVarList plSub, plSub2;
   DWORD i;
   if (dwNum) {
      plSub = new CMIFLVarList;
      if (plSub) {
         for (i = 0; i < dwNum; i++) {
            plSub2 = new CMIFLVarList;
            if (!plSub2)
               continue;

            if (!pNode->AttribEnum (i, &psz, &pszValue)) {
               plSub2->Release();
               continue;
            }

            pv->SetString (psz);
            plSub2->Add (pv, TRUE);
            pv->SetString (pszValue);
            plSub2->Add (pv, TRUE);

            // add
            pv->SetList (plSub2);
            plSub->Add (pv, TRUE);
            plSub2->Release();
         } // i

         pv->SetList (plSub);
         plSub->Release();
      }
      else
         pv->SetNULL(); // shouldnt happen
   }
   else
      pv->SetNULL(); // since no attributes, leave as null
   plMain->Add (pv, TRUE);

   // add all the contents
   dwNum = pNode->ContentNum();
   PCMMLNode2 pSub;
   if (dwNum) {
      plSub = new CMIFLVarList;
      if (plSub) {
         for (i = 0; i < dwNum; i++) {
            psz = NULL;
            pSub = NULL;
            pNode->ContentEnum (i, &psz, &pSub);

            if (psz)
               pv->SetString (psz);
            else
               MMLToList (pSub, pv);   // recurse and get sub-list

            plSub->Add (pv, TRUE);
         } // i

         // add list
         pv->SetList (plSub);
         plSub->Release();
      }
      else
         pv->SetNULL(); // error. shouldnt happen
   }
   else
      pv->SetNULL(); // since no contents. leave as null
   plMain->Add (pv, TRUE);

   // set this as the main list
   pv->SetList (plMain);
   plMain->Release();
}



/**********************************************************************************
MMLFromList - Does the inverse of MMLToList.

inputs
   PCMIFLVar      pv - Should be a list. If not, errors out
   PCMIFLVM       pVM - MV
returns
   PCMMLNode2 - New node
*/
PCMMLNode2 MMLFromList (PCMIFLVar pv, PCMIFLVM pVM)
{
   if (pv->TypeGet() != MV_LIST)
      return NULL;
   PCMIFLVarList plMain = pv->GetList ();
   if (!plMain)
      return NULL;
   PCMMLNode2 pNode = new CMMLNode2;
   if (!pNode) {
      plMain->Release();
      return NULL;
   }

   // get the name
   PCMIFLVar pmv, pmv2;
   PCMIFLVarString ps;
   pmv = plMain->Get (0);
   if (!pmv)
      goto getattrib;
   ps = pmv->GetString (pVM);
   if (!ps)
      goto getattrib;
   pNode->NameSet (ps->Get());
   ps->Release();

getattrib:
   // get the attributes
   PCMIFLVarList plSub, plSub2;
   PCMIFLVarString psAttrib1, psAttrib2;
   PCMIFLVar pvAttrib1, pvAttrib2;
   DWORD i;
   pmv = plMain->Get(1);
   if (!pmv)
      goto getcontent;
   if (pmv->TypeGet() != MV_LIST)
      goto getcontent;
   plSub = pmv->GetList ();
   if (!plSub)
      goto getcontent;
   for (i = 0; i < plSub->Num(); i++) {
      pmv2 = plSub->Get(i);
      if (!pmv2)
         continue;
      if (pmv2->TypeGet() != MV_LIST)
         continue;
      plSub2 = pmv2->GetList();
      if (!plSub2)
         continue;

      // get the two strings
      pvAttrib1 = plSub2->Get(0);
      pvAttrib2 = plSub2->Get(1);

      if (pvAttrib1 && pvAttrib2) {
         psAttrib1 = pvAttrib1->GetString(pVM);
         psAttrib2 = pvAttrib2->GetString(pVM);

         if (psAttrib1 && psAttrib2)
            pNode->AttribSetString (psAttrib1->Get(), psAttrib2->Get());

         if (psAttrib1)
            psAttrib1->Release();
         if (psAttrib2)
            psAttrib2->Release();
      }

      plSub2->Release();
   } // i
   plSub->Release();


getcontent:
   PCMMLNode2 pSub;
   pmv = plMain->Get(2);
   if (!pmv)
      goto done;
   if (pmv->TypeGet() != MV_LIST)
      goto done;
   plSub = pmv->GetList ();
   if (!plSub)
      goto done;
   plSub->Release();
   for (i = 0; i < plSub->Num(); i++) {
      pmv2 = plSub->Get(i);
      if (!pmv2)
         continue;

      if (pmv2->TypeGet() == MV_LIST) {
         // it's a sub-list
         pSub = MMLFromList (pmv2, pVM);
         if (pSub)
            pNode->ContentAdd (pSub);
      }
      else {
         // its a string to add
         psAttrib1 = pmv2->GetString (pVM);
         if (psAttrib1) {
            pNode->ContentAdd (psAttrib1->Get());
            psAttrib1->Release();
         }
      }
   } // i

done:
   plMain->Release();
   return pNode;
}


/**********************************************************************************
VMFuncImportInit - Internal function that initializes the import functions if they
aren't already
*/
static void VMFuncImportInit (void)
{
   if (ghFuncImport.Num())
      return;

   ghFuncImport.Init (sizeof(VMFII));

   static VMFII av[] = {
      L"trace", VMTOK_TRACE, 1,
      L"deletegroup", VMTOK_DELETEGROUP, 2,
      L"tobool", VMTOK_TOBOOL, 1,
      L"tonumber", VMTOK_TONUMBER, 1,
      L"toobject", VMTOK_TOOBJECT, 1,
      L"tochar", VMTOK_TOCHAR, 1,
      L"tostring", VMTOK_TOSTRING, 1,
      L"tostringmml", VMTOK_TOSTRINGMML, 1,
      L"tomethod", VMTOK_TOMETHOD, 1,
      L"tofunction", VMTOK_TOFUNCTION, 1,
      L"typeof", VMTOK_TYPEOF, 1,
      L"languageget", VMTOK_LANGUAGEGET, 0,
      L"languageset", VMTOK_LANGUAGESET, 1,
      L"timeget", VMTOK_TIMEGET, 0,
      L"timezone", VMTOK_TIMEZONE, 0,
      L"timetodatetime", VMTOK_TIMETODATETIME, 2,
      L"timefromdatetime", VMTOK_TIMEFROMDATETIME, 8,
      L"objectnew", VMTOK_OBJECTNEW, 1,
      L"objectclone", VMTOK_OBJECTCLONE, 3,
      L"timesincestart", VMTOK_TIMESINCESTART, 0,

      L"globalenum", VMTOK_GLOBALENUM, 0,
      L"globalquery", VMTOK_GLOBALQUERY, 1,
      L"globalget", VMTOK_GLOBALGET, 1,
      L"globalset", VMTOK_GLOBALSET, 2,
      L"globalremove", VMTOK_GLOBALREMOVE, 1,
      L"globalgetset", VMTOK_GLOBALGETSET, -1,
      L"objectenum", VMTOK_OBJECTENUM, 0,
      L"objectquery", VMTOK_OBJECTQUERY, 1,
      L"classesenum", VMTOK_CLASSESENUM, 0,

      L"mmltolist", VMTOK_MMLTOLIST, 2,
      L"mmlfromlist", VMTOK_MMLFROMLIST, 2,
      L"resourceget", VMTOK_RESOURCEGET, 1,
      L"resourceenum", VMTOK_RESOURCEENUM, 0,

      L"math_abs", VMTOK_MATH_ABS, 1,
      L"math_acos", VMTOK_MATH_ACOS, 1,
      L"math_asin", VMTOK_MATH_ASIN, 1,
      L"math_atan", VMTOK_MATH_ATAN, 1,
      L"math_ceil", VMTOK_MATH_CEIL, 1,
      L"math_cos", VMTOK_MATH_COS, 1,
      L"math_exp", VMTOK_MATH_EXP, 1,
      L"math_floor", VMTOK_MATH_FLOOR, 1,
      L"math_log", VMTOK_MATH_LOG, 1,
      L"math_log10", VMTOK_MATH_LOG10, 1,
      L"math_pow", VMTOK_MATH_POW, 2,
      L"math_sqrt", VMTOK_MATH_SQRT, 1,
      L"math_tan", VMTOK_MATH_TAN, 1,
      L"math_isinfinite", VMTOK_MATH_ISINFINITE, 1,
      L"math_isnan", VMTOK_MATH_ISNAN, 1,
      L"math_max", VMTOK_MATH_MAX, 2,
      L"math_min", VMTOK_MATH_MIN, 2,
      L"math_sin", VMTOK_MATH_SIN, 1,
      L"math_random", VMTOK_MATH_RANDOM, (DWORD)-1,
      L"math_randomseed", VMTOK_MATH_RANDOMSEED, -1,
      L"math_cosh", VMTOK_MATH_COSH, 1,
      L"math_sinh", VMTOK_MATH_SINH, 1,
      L"math_round", VMTOK_MATH_ROUND, 1,
      L"math_tanh", VMTOK_MATH_TANH, 1,
      L"math_atan2", VMTOK_MATH_ATAN2, 2,
      
      L"isbool", VMTOK_ISBOOL, 1,
      L"ischar", VMTOK_ISCHAR, 1,
      L"isfunction", VMTOK_ISFUNCTION, 1,
      L"ismethod", VMTOK_ISMETHOD, 1,
      L"isnumber", VMTOK_ISNUMBER, 1,
      L"isstring", VMTOK_ISSTRING, 1,
      L"isobject", VMTOK_ISOBJECT, 1,
      L"isresource", VMTOK_ISRESOURCE, 1,
      L"islist", VMTOK_ISLIST, 1,
      L"objecttoguidstring", VMTOK_OBJECTTOGUIDSTRING, 1,
      L"guidstringtoobject", VMTOK_GUIDSTRINGTOOBJECT, 1,

      L"ischaralpha", VMTOK_ISCHARALPHA, 1,
      L"ischaralphanum", VMTOK_ISCHARALPHANUM, 1,
      L"ischarupper", VMTOK_ISCHARUPPER, 1,
      L"ischarlower", VMTOK_ISCHARLOWER, 1,
      L"ischarspace", VMTOK_ISCHARSPACE, 1,
      L"ischardigit", VMTOK_ISCHARDIGIT, 1,
   };

   DWORD i;
   for (i = 0; i < sizeof(av)/sizeof(VMFII); i++)
      ghFuncImport.Add (av[i].pszName, &av[i], FALSE);


}



/**********************************************************************************
VMFuncImportIsValid - This determines if an imported function is valid.
An application should call it when it's CMIFLAppSocket::FuncImportIsValid()
is called.

inputs
   PWSTR          pszName - Name. Should be all lower case.
   DWORD          *pdwParams - Filled in with the number of parameters
returns
   BOOL - TRUE if found, FALSE if not
*/
BOOL VMFuncImportIsValid (PWSTR pszName, DWORD *pdwParams)
{
   if (!ghFuncImport.Num())
      VMFuncImportInit();

   // find it
   PVMFII pv = (PVMFII)ghFuncImport.Find (pszName, FALSE);
   if (!pv)
      return FALSE;

   *pdwParams = pv->dwParam;
   return TRUE;
}


/**********************************************************************************
VMLibraryEnum - This fills in the PMASLIB information for the standard
library.

An application should call this as part of the library enumeration called
in CMIFLAppSocket::LibraryEnum();

inputs
   PMASLIB           pLib - Filled in
returns
   BOOL - TRUE if success - which is bascially guaranteed
*/
BOOL VMLibraryEnum (PMASLIB pLib)
{
   memset (pLib, 0, sizeof(*pLib));

   pLib->dwResource = IDR_MIFLSTANDARDLIBRARY;
   pLib->fDefaultOn = TRUE;
   pLib->hResourceInst = ghInstance;
   pLib->pszDescShort = L"The standard library provides many general-purposes methods and "
      L"functions. It is necessary for a MIFL program to work properly.";
   pLib->pszName = L"Standard library";

   return TRUE;
}


/**********************************************************************************
MIFLDoubleToFileTime - Converts a double (days since jan 1, 2000) to a filetime

inputs
   double         f - Value
   FILETIME       *pf - Filled with the filetime
returns
   none
*/
void MIFLDoubleToFileTime (double f, FILETIME *pft)
{
   // convert this to a filetime
   f += 146097;  // number of days between jan1,1601 and jan1 2001
   f *= (24.0 * 60.0 * 60.0 * 1000.0); // so know number of days

   // conver this to a __int64
   __int64 i64;
   i64 = (__int64)f;
   i64 *= (1000000000 /*nano*/ / 100 /*hundred*/ / 1000 /*milli*/);

   *((__int64*)pft) = i64;
}


/**********************************************************************************
MIFLFileTimeToDouble - Converts a filetime representation to a double representing
the number of days since Jan 1, 2000.

inputs
   PFILETIME      pft - File time
returns
   double - Time in days
*/
DLLEXPORT double MIFLFileTimeToDouble (PFILETIME pft)
{
   __int64 pi = *((__int64*)pft);

   // convert to milliseconds... since filtime is in 100 nano-second
   pi /= (1000000000 /*nano*/ / 100 /*hundred*/ / 1000 /*milli*/);

   // convert to a double
   double f = (double) pi;
   f /= (24.0 * 60.0 * 60.0 * 1000.0); // so know number of days
   f -= 146097;  // number of days between jan1,1601 and jan1 2001

   return f;
}

/**********************************************************************************
CMIFLVM::FuncImport - An application should call this when it receiveds a
CMIFLAppSocket::FuncImport(), so that functions in the standard library
can be exported.

It has the same parameters as CMIFLAppSocket::FuncImport(). If the function
is not handled by the VM code it returns FALSE (without changing pRet)

NOTE: pVM should be the same as this, so the pVM parameter is not accepted.
*/
BOOL CMIFLVM::FuncImport (PWSTR pszName, PCMIFLVMObject pObject,
   PCMIFLVarList plParams, DWORD dwCharStart, DWORD dwCharEnd, PCMIFLVarLValue pRet)
{
   // make sure lValue is cleared
   pRet->Clear();

   if (!ghFuncImport.Num())
      VMFuncImportInit();

   // find it
   PVMFII pvii = (PVMFII)ghFuncImport.Find (pszName, FALSE);
   if (!pvii)
      return FALSE;

   // DOCUMENT: Make sure to document all imported functions

   // initialize to first params
   PCMIFLVar pv = plParams->Get(0);
   PCMIFLVar pv2;

   switch (pvii->dwVMTOK) {
   case VMTOK_TRACE:
      // DOCUMENT: Trace
      if (!pv)
         return TRUE;   // don't worry about run-time error since compiler should have picked up

      OutputDebugString (pv);
      pRet->m_Var.SetUndefined();
      return TRUE;

   case VMTOK_DELETEGROUP:
      {
         // param 0 = list of objects to delete
         // param 1 = TRUE if delete children of those objects
         if (plParams->Num() != 2) {
            pRet->m_Var.SetUndefined ();
            return FALSE;
         }

         PCMIFLVarList pl = plParams->Get(0)->GetList();
         if (!pl) {
            pRet->m_Var.SetBOOL (FALSE);
            return TRUE;
         }
         BOOL fChildren = plParams->Get(1)->GetBOOL(this);

         DWORD i;
         CHashGUID hObject;
         hObject.Init (0, pl->Num()*3);   // 0 sized
         for (i = 0; i < pl->Num(); i++) {
            PCMIFLVar pv = pl->Get(i);
            if (!pv || (pv->TypeGet() != MV_OBJECT))
               continue;
            GUID g;
            g = pv->GetGUID ();

            if (hObject.FindIndex(&g) == -1)
               hObject.Add (&g, NULL);
         } // i

         // do children
         if (fChildren) for (i = 0; i < hObject.Num(); i++) {
            PCMIFLVMObject po = ObjectFind (hObject.GetGUID(i));
            if (!po)
               continue;

            GUID *pg = (GUID*)po->m_lContains.Get(0);
            DWORD j;
            for (j = 0; j < po->m_lContains.Num(); j++, pg++) {
               if (hObject.FindIndex(pg) == -1)
                  hObject.Add (pg, NULL);
            } // j
         } // i

         pl->Release();


         // delete all of these
         // BUGFIX - Delete backwards so children removed first
         for (i = hObject.Num()-1; i < hObject.Num(); i--)
            ObjectDelete (hObject.GetGUID(i));

         // DOCUMENT: Deletes the object and all its children
         // Return TRUE if success, FALSE if object already deleted
         pRet->m_Var.SetBOOL (TRUE);
      }
      return TRUE;

      // DOCUMENT: ToXXX calls
   case VMTOK_TOBOOL:      // convert to a boolean
      if (pv) {
         pRet->m_Var.Set (pv);
         pRet->m_Var.ToBOOL (this);
      }
      else
         pRet->m_Var.SetUndefined();
      return TRUE;

   case VMTOK_TONUMBER:      // convert to a number
      if (pv) {
         pRet->m_Var.Set (pv);
         pRet->m_Var.ToDouble (this);
      }
      else
         pRet->m_Var.SetUndefined();
      return TRUE;

   case VMTOK_TOOBJECT:      // convert to an object
      if (pv) switch (pv->TypeGet()) {
         case MV_OBJECT:
            pRet->m_Var.Set(pv);
            break;
         case MV_OBJECTMETH:
            GUID gObject;
            gObject = pv->GetGUID();
            pRet->m_Var.SetObject (&gObject);
            break;
         case MV_STRING:
         case MV_STRINGTABLE:
            PCMIFLVarString ps;
            ps = pv->GetString (this);
            PWSTR psz;
            psz = ps->Get();
            if (wcslen(psz) == sizeof(gObject)*2) {
               memset (&gObject, 0, sizeof(gObject));
               if (sizeof(gObject) == MMLBinaryFromString (psz, (PBYTE)&gObject, sizeof(gObject)))
                  pRet->m_Var.SetObject(&gObject);
               else
                  pRet->m_Var.SetUndefined();
            }
            else
               pv->SetUndefined();
            ps->Release();
            break;
         default:
            pRet->m_Var.SetUndefined();
            break;
      }
      else
         pRet->m_Var.SetUndefined();
      return TRUE;

   case VMTOK_TOCHAR:      // convert to a character
      if (pv) {
         pRet->m_Var.Set (pv);
         pRet->m_Var.ToChar (this);
      }
      else
         pRet->m_Var.SetUndefined();
      return TRUE;

   case VMTOK_TOSTRING:      // convert to a string
      if (pv) {
         pRet->m_Var.Set (pv);
         pRet->m_Var.ToString (this);
      }
      else
         pRet->m_Var.SetUndefined();
      return TRUE;

   case VMTOK_TOSTRINGMML:
      {
         // get the connection
         if (!pv) {
            pRet->m_Var.SetUndefined();
            return TRUE;
         }

         // sanitize
         PCMIFLVarString ps = pv->GetString(this);
         CMem mem;
         MemZero (&mem);
         MemCatSanitize (&mem, ps->Get());
         ps->Release();

         // set it
         pRet->m_Var.SetString ((PWSTR)mem.p, (DWORD)-1);
      }
      return TRUE;

   case VMTOK_TOMETHOD:      // convert to a method
      if (pv) {
         pRet->m_Var.Set (pv);
         pRet->m_Var.ToMeth (this);
      }
      else
         pRet->m_Var.SetUndefined();
      return TRUE;

   case VMTOK_TOFUNCTION:      // convert to a function
      if (pv) {
         pRet->m_Var.Set (pv);
         pRet->m_Var.ToFunction (this);
      }
      else
         pRet->m_Var.SetUndefined();
      return TRUE;

   case VMTOK_TYPEOF:      // convert to a function
      if (pv)
         pRet->m_Var.SetString (pv->TypeAsString(), (DWORD)-1);
      else
         pRet->m_Var.SetUndefined();
      return TRUE;

   case VMTOK_LANGUAGEGET:      // gets the current language
      pRet->m_Var.SetDouble ((DWORD)m_LangID);
      return TRUE;

   case VMTOK_LANGUAGESET:      // sets the current language
      if (pv)
         m_LangID = (LANGID)pv->GetDouble(this);
      pRet->m_Var.SetUndefined();
      return TRUE;

   case VMTOK_TIMESINCESTART:
      {
         LARGE_INTEGER iCur;
         QueryPerformanceCounter (&iCur);

         double fElapsed = (double)*((__int64*)&iCur) / (double)*((__int64*)&m_iPerCountFreq);

         pRet->m_Var.SetDouble (fElapsed);
      }
      return TRUE;

   case VMTOK_TIMEGET:      // number of days since jan1, 2001
      {
         // DOCUMENT: Returns the time in the number of days since January 1, 2001, GMT

         // BUGFIX - Optimize this so it's slow the first time but faster after that
         if (m_fTimeCalibrate) {
            // NOTE: This will only be accurate to 1 second, but that should be enough
            LARGE_INTEGER iCur;
            QueryPerformanceCounter (&iCur);
            __int64 iElapsed = ( *((__int64*)&iCur) - *((__int64*)&m_iTimeCalibrate)) /
               *((__int64*)&m_iPerCountFreq);
            double fElapsed = (double) iElapsed / 60.0 / 60.0 / 24.0;
            pRet->m_Var.SetDouble (fElapsed + m_fTimeCalibrate);
         }
         else {
            FILETIME ft;
            GetSystemTimeAsFileTime (&ft);
            QueryPerformanceCounter (&m_iTimeCalibrate);
            m_fTimeCalibrate = MIFLFileTimeToDouble (&ft);
            pRet->m_Var.SetDouble (m_fTimeCalibrate);
         }
      }
      return TRUE;


   case VMTOK_TIMEZONE:      // number of days since jan1, 2001
      {
         TIME_ZONE_INFORMATION tzi;
         memset (&tzi, 0, sizeof(tzi));
         GetTimeZoneInformation (&tzi);

         pRet->m_Var.SetDouble((double)tzi.Bias / (60.0 * 24.0));
      }
      return TRUE;

   case VMTOK_TIMETODATETIME:      // convert from time to a date and time
      {
         // DOCUMENT: This takes a time from TimeGet(), and fills in a list
         // with time information.
         // Param1 = TimeGet()
         // Param2 = TRUE then system time (GMT), FALSE then local time
         // Returns a list with [0] = year, [1]=month, [2]=date, [3]=hour, [4]= minute, [5]=second, [6]=millisecond,
         // [7] = day of week, where sun=0,

         PCMIFLVar pv = plParams->Get(0);
         PCMIFLVar pv2 = plParams->Get(1);

         if (!pv) {
            pv->SetUndefined();
            return TRUE;
         }

         // convert this to a filetime
         double f = pv->GetDouble(this);
         FILETIME ftOrig, ft;
         MIFLDoubleToFileTime (f, &ftOrig);
         //f += 146097;  // number of days between jan1,1601 and jan1 2001
         //f *= (24.0 * 60.0 * 60.0 * 1000.0); // so know number of days

         // conver this to a __int64
         //__int64 i64;
         //i64 = (__int64)f;
         //i64 *= (1000000000 /*nano*/ / 100 /*hundred*/ / 1000 /*milli*/);

         SYSTEMTIME st;
         memset (&st, 0, sizeof(st));
         if (!pv2 || !pv2->GetBOOL(this))
            FileTimeToLocalFileTime (&ftOrig/*(FILETIME*)&i64*/, &ft);
         else
            memcpy (&ft, &ftOrig /*&i64*/, sizeof(ftOrig/*i64*/));
         FileTimeToSystemTime (&ft, &st);

         // create the list
         PCMIFLVarList pl = new CMIFLVarList;
         if (!pl) {
            pRet->m_Var.SetUndefined();
            return TRUE;
         }

         // add
         CMIFLVar v;
         v.SetDouble (st.wYear);
         pl->Add (&v, TRUE);
         v.SetDouble (st.wMonth);
         pl->Add (&v, TRUE);
         v.SetDouble (st.wDay);
         pl->Add (&v, TRUE);
         v.SetDouble (st.wHour);
         pl->Add (&v, TRUE);
         v.SetDouble (st.wMinute);
         pl->Add (&v, TRUE);
         v.SetDouble (st.wSecond);
         pl->Add (&v, TRUE);
         v.SetDouble (st.wMilliseconds);
         pl->Add (&v, TRUE);
         v.SetDouble (st.wDayOfWeek);
         pl->Add (&v, TRUE);

         pRet->m_Var.SetList (pl);
         pl->Release();
      }
      return TRUE;

   case VMTOK_TIMEFROMDATETIME:      // convert from
      {
         // DOCUMENT: This takes the day, month, etc. and returns a TimeGet() time.
         // Param1 = TRUE then system time (GMT), FALSE then local time
         // Param2 = year
         // Praam3 = month
         // Param4 = date
         // param5 = hour
         // Param6 = minuten
         // Param7 = second
         // Param8 = millisecond

         PCMIFLVar apv[8];
         DWORD i;
         for (i = 0; i < 8; i++) {
            apv[i] = plParams->Get(i);
            if (!apv[i]) {
               pv->SetUndefined();
               return TRUE;
            }
         }

         // fill in structure
         SYSTEMTIME st;
         memset (&st, 0, sizeof(st));
         st.wYear = (WORD)apv[1]->GetDouble(this);
         st.wMonth = (WORD)apv[2]->GetDouble(this);
         st.wDay = (WORD)apv[3]->GetDouble(this);
         st.wHour = (WORD)apv[4]->GetDouble(this);
         st.wMinute = (WORD)apv[5]->GetDouble(this);
         st.wSecond = (WORD)apv[6]->GetDouble(this);
         st.wMilliseconds = (WORD)apv[7]->GetDouble(this);

         // convert to filetime
         FILETIME ft1, ft2;
         if (!SystemTimeToFileTime (&st, &ft1)) {
            pv->SetNULL();
            return TRUE;
         }
         if (!apv[0] || !apv[0]->GetBOOL(this)) // BUGFIX - Had passed in pv
            LocalFileTimeToFileTime (&ft1, &ft2);
         else
            ft2 = ft1;

         pRet->m_Var.SetDouble (MIFLFileTimeToDouble (&ft2));
      }
      return TRUE;

   case VMTOK_OBJECTCLONE: // clones an object or several objects
      {
         CListFixed lGUID;
         lGUID.Init (sizeof(GUID));

         // first parameter is the object or a list of objects
         PCMIFLVar pv = plParams->Get(0);
         GUID gTemp;
         DWORD i;
         if (pv) switch (pv->TypeGet()) {
         case MV_OBJECT:
            gTemp = pv->GetGUID();
            lGUID.Add (&gTemp);
            break;

         case MV_LIST:
            {
               PCMIFLVarList pl = pv->GetList ();
               for (i = 0; i < pl->Num(); i++) {
                  PCMIFLVar pv2 = pl->Get(i);
                  if (!pv2 || (pv2->TypeGet() != MV_OBJECT) )
                     continue;

                  gTemp = pv2->GetGUID();
                  lGUID.Add (&gTemp);
               } // i

               pl->Release();
            }
            break;
         } // swtich
         if (!lGUID.Num()) {
            pRet->m_Var.SetUndefined ();
            return TRUE;
         }

         // next parameter is string of property to set in all of them
         pv = plParams->Get(1);
         PCMIFLVarString ps = NULL;
         if (pv && ((pv->TypeGet() == MV_STRING) || (pv->TypeGet() == MV_STRINGTABLE)) )
            ps = pv->GetString (this);

         // get the value to set
         pv = plParams->Get(2);

         // call the function
         PCHashGUID phOrigToNew = ObjectClone ((GUID*)lGUID.Get(0), lGUID.Num(), ps->Get(), pv);

         // done
         if (ps)
            ps->Release();

         // create a list to store
         PCMIFLVarList pl = new CMIFLVarList;
         CMIFLVar var;
         pRet->m_Var.SetList (pl);
         if (phOrigToNew) {
            for (i = 0; i < phOrigToNew->Num(); i++) {
               GUID *pgOrig = phOrigToNew->GetGUID (i);
               GUID *pgNew = (GUID*) phOrigToNew->Get (i);
               if (!pgOrig || !pgNew)
                  continue;

               // new sub-ust
               PCMIFLVarList plSub = new CMIFLVarList;
               if (!plSub)
                  continue;
               var.SetList (plSub);
               pl->Add (&var, TRUE);
               var.SetObject (pgOrig);
               plSub->Add (&var, TRUE);
               var.SetObject (pgNew);
               plSub->Add (&var, TRUE);
               plSub->Release();
            } // i
            delete phOrigToNew;
         } // if phOrigToNew
         pl->Release();
      }
      return TRUE;

   case VMTOK_OBJECTNEW:  // create a new object based on the the string or current object
      {
         // DOCUMENT: creates new object from a string or current object
         PCMIFLVar pv = plParams->Get(0);
         if (!pv) {
            pRet->m_Var.SetUndefined ();
            return TRUE;
         }

         // if it's an object then find the class
         PCMIFLVarString ps = NULL;
         if (pv->TypeGet() == MV_OBJECT) {
            GUID g = pv->GetGUID();
            PCMIFLVMObject pFind = ObjectFind (&g);
            if (!pFind) {
               pRet->m_Var.SetNULL ();
               return TRUE;
            }

            PCMIFLVMLayer pLayer = pFind->LayerGet(pFind->LayerNum()-1);
            if (!pLayer) {
               pRet->m_Var.SetNULL();
               return TRUE;
            }

            ps = new CMIFLVarString;
            if (!ps) {
               pRet->m_Var.SetNULL();
               return TRUE;
            }
            ps->Set ((PWSTR)pLayer->m_memName.p, (DWORD)-1);
         }
         else {
            // get the string
            ps = pv->GetString(this);
            if (!ps) {
               pRet->m_Var.SetNULL();
               return TRUE;
            }
         }

         // find the object
         DWORD dwID = m_pCompiled->m_pLib->ObjectFind (ps->Get(), -1);
         ps->Release();

         // get the object...
         PCMIFLObject pObject = m_pCompiled->m_pLib->ObjectGet (dwID);
         if (!pObject) {
            pRet->m_Var.SetNULL();
            return TRUE;
         }

         // create
         PCMIFLVMObject pNew = new CMIFLVMObject;
         if (!pNew) {
            pRet->m_Var.SetNULL();
            return TRUE;
         }
         if (!pNew->InitAsNew (this, pObject)) {
            pRet->m_Var.SetNULL();
            delete pNew;
            return TRUE;
         }

         // add this
         m_hObjects.Add (&pNew->m_gID, &pNew);

         // call constructor
         DWORD dwRet = MethodCallVMTOK (&pNew->m_gID, VMTOK_CONSTRUCTOR, NULL,
            0, 0, pRet);

         pRet->m_Var.SetObject (&pNew->m_gID);
         return TRUE;
      }
      return TRUE;

   case VMTOK_GLOBALENUM:      // enumerate globals
      {
         // DOCUMENT: Returns a list with all the globals
         
         PCMIFLVarList pl = new CMIFLVarList;
         if (!pl) {
            pRet->m_Var.SetUndefined();
            return TRUE;
         }

         DWORD i;
         CMIFLVar v;
         for (i = 0; i < m_hGlobals.Num(); i++) {
            PCMIFLVarProp pvp = (PCMIFLVarProp)m_hGlobals.Get(i);

            PWSTR psz = NULL;
            if (pvp->m_dwID >= VM_CUSTOMIDRANGE) {
               DWORD dwIndex = m_hUnIdentifiersCustomGlobal.FindIndex (pvp->m_dwID);
               psz = m_hIdentifiersCustomGlobal.GetString (dwIndex);
            }
            else {
               // get the name
               PMIFLIDENT pmi = (PMIFLIDENT) m_pCompiled->m_hGlobals.Find (pvp->m_dwID);
               if (pmi && (pmi->dwType == MIFLI_GLOBAL))
                  psz = (PWSTR)((PCMIFLProp)pmi->pEntity)->m_memName.p;
               else if (pmi && (pmi->dwType == MIFLI_OBJECT))
                  psz = (PWSTR)((PCMIFLObject)pmi->pEntity)->m_memName.p;
            }
            if (!psz)
               continue;

            v.SetString (psz, (DWORD)-1);
            pl->Add (&v, TRUE);
         } // i

         pRet->m_Var.SetList (pl);
         pl->Release();

      }
      return TRUE;

   case VMTOK_GLOBALQUERY:      // query if a global exists
      {
         // DOCUMENT: Queries if a global exists, using a string
         // Param1 = name
         // Returns - TRUE if exists, FALSE if didn't exist
         PCMIFLVar pv = plParams->Get(0);
         if (!pv) {
            pRet->m_Var.SetUndefined ();
            return TRUE;
         }
         DWORD dwID = ToGlobalID (pv, FALSE);
         if (dwID == -1) {
            pRet->m_Var.SetBOOL (FALSE);
            return TRUE;
         }

         // else, get...
         dwID = m_hGlobals.FindIndex (dwID);
         pRet->m_Var.SetBOOL (dwID != -1);
      }
      return TRUE;

   case VMTOK_GLOBALGET:      // get a global
      {
         // DOCUMENT: Gets a global, using a string
         // Param1 - name
         // Returns - Global value, of Undefined if doens't exist
         PCMIFLVar pv = plParams->Get(0);
         if (!pv) {
            pRet->m_Var.SetUndefined ();
            return TRUE;
         }
         DWORD dwID = ToGlobalID (pv, FALSE);
         if (dwID == -1) {
            pRet->m_Var.SetUndefined();
            return TRUE;
         }

         // else, get...
         GlobalGet (dwID, !m_pFCICur->pObject && (m_pFCICur->dwPropIDGetSet == dwID), pRet);
      }
      return TRUE;


   case VMTOK_GLOBALSET:      // set a global
      {
         // DOCUMENT: Sets a global, using a string
         // Param1 - name
         // Param2 - new value
         // If a property doesn't already exist then it's created
         // Returns - Property value, of Undefined if doens't exist

         PCMIFLVar pv = plParams->Get(0);
         PCMIFLVar pv2 = plParams->Get(1);
         if (!pv || !pv2) {
            pRet->m_Var.SetUndefined ();
            return TRUE;
         }
         DWORD dwID = ToGlobalID (pv, TRUE);
         if (dwID == -1) {
            pRet->m_Var.SetUndefined();
            return TRUE;
         }

         // else, set...
         pRet->m_Var.Set (pv2);
         GlobalSet (dwID, !m_pFCICur->pObject && (m_pFCICur->dwPropIDGetSet == dwID), pv2);
      }
      return TRUE;

   case VMTOK_GLOBALREMOVE:      // remove a global
      {
         // DOCUMENT: Removes a global, using a string
         // Param1 = name
         // Returns - TRUE if removed, FALSE if didn't exist
         PCMIFLVar pv = plParams->Get(0);
         if (!pv) {
            pRet->m_Var.SetUndefined ();
            return TRUE;
         }
         DWORD dwID = ToGlobalID (pv, FALSE);
         if (dwID == -1) {
            pRet->m_Var.SetBOOL (FALSE);
            return TRUE;
         }

         // else, get...
         pRet->m_Var.SetBOOL (GlobalRemove (dwID));
      }
      return TRUE;

   case VMTOK_GLOBALGETSET:      // control get/set code
      {
         // DOCUMENT: Adds a property get/set to the list.
         // Param1 - Global name (as a string). This must be a valid property name. It must be a name
         // of an existing global or a unique name; the ID cannot already be used as a method name, etc.
         // Param2 - Function or method that gets called when this global called with a get. Must return parameter
         // Param3 - Function of method that gets called when this global called with a set. Must accept one param.
         // If exclude BOTH param2 and Param3 then wont have a get/set function
         // Returns TRUE if it's added, FALSE if fail

         if (plParams->Num() < 1) {
            pRet->m_Var.SetUndefined ();
            return TRUE;
         }

         MIFLGETSET mgs;
         memset (&mgs, 0, sizeof(mgs));

         // figure out the code for the layer
         PMIFLIDENT pmi = NULL;
         MIFLIDENT miFunc;
         DWORD j;
         for (j = 0; j < 2; j++) {
            pv = plParams->Get(1+j);
            if (!pv)
               continue;

            switch (pv->TypeGet()) {
            case MV_NULL:
            case MV_UNDEFINED:
               continue;   // leave NULL

            case MV_METH:
               // since don't specify object link to current one
               if (!pObject)
                  break;   // so get NULL and error

               pv->SetObjectMeth (&pObject->m_gID, pv->GetValue());
               // fall through
            case MV_OBJECTMETH:
               {
                  GUID g = pv->GetGUID();
                  PCMIFLVMObject po = ObjectFind (&g);
                  if (!po) {
                     // coudlnt find object, so cant create
                     pRet->m_Var.SetBOOL (FALSE);
                     return TRUE;
                  }

                  pmi = (PMIFLIDENT) po->m_hMeth.Find (pv->GetValue());
               }
               break;

            case MV_FUNC:
               {
                  PCMIFLFunc pFunc = m_pCompiled->m_pLib->FuncGet (pv->GetValue());
                  if (pFunc) {
                     memset (&miFunc, 0, sizeof(miFunc));
                     miFunc.dwType = MIFLI_FUNC;
                     miFunc.pEntity = pFunc;
                     pmi = &miFunc;
                  }
               }
               break;
            }
            if (!pmi) {
               // err - inapprpriate type
               pRet->m_Var.SetBOOL (FALSE);
               return TRUE;
            }
            
            // verify right number of params
            PCMIFLFunc pFunc = (PCMIFLFunc)pmi->pEntity;
            if (j)
               mgs.m_pCodeSet = &pFunc->m_Code;
            else
               mgs.m_pCodeGet = &pFunc->m_Code;
         } // j


         // get the name
         pv = plParams->Get(0);
         DWORD dwID = ToGlobalID (pv, FALSE);
         if (dwID == -1) {
            pRet->m_Var.SetBOOL (FALSE);
            return TRUE;
         }
         mgs.dwID = dwID;

         // find the global
         PCMIFLVarProp pvp = (PCMIFLVarProp) m_hGlobals.Find (dwID);
         if (!pvp) {
            // not bound
            pRet->m_Var.SetBOOL (FALSE);
            return TRUE;
         }

         pvp->m_pCodeGet = mgs.m_pCodeGet;
         pvp->m_pCodeSet = mgs.m_pCodeSet;

         pRet->m_Var.SetBOOL (TRUE);
      }
      return TRUE;

   case VMTOK_MMLTOLIST:
      // converts from MML (or a resource) to a list
      // 1st param is the string/resource
      // 2nd param is TRUE if it's only one main entry, FALSE if multiple. Should be TRUE if a resource.
      {
         if (plParams->Num() != 2) {
            pRet->m_Var.SetUndefined ();
            return TRUE;
         }

         BOOL fOneMainEntry = plParams->Get(1)->GetBOOL (this);

         PCMIFLVar pv = plParams->Get(0);
         if (pv->TypeGet() == MV_RESOURCE) {
            if (!fOneMainEntry) {
               // error
               pRet->m_Var.SetNULL();
               return TRUE;
            }
            PCMIFLResource pRes = m_pCompiled->m_pLib->ResourceGet (pv->GetValue());
            if (!pRes) {
               // error. shouldnt happen
               pRet->m_Var.SetNULL();
               return TRUE;
            }

            PCMMLNode2 pNode = pRes->Get (m_LangID);
            if (!pNode) {
               pRet->m_Var.SetNULL();
               return TRUE;
            }

            MMLToList (pNode, &pRet->m_Var);
            return TRUE;
         }
         else {
            // string
            PCMIFLVarString ps = pv->GetString (this);
            if (!ps) {
               pRet->m_Var.SetNULL();
               return TRUE;
            }

            // convert the MML
            CEscError err;
            PCMMLNode pNodeOld = ParseMML (ps->Get(), ghInstance, NULL, NULL, &err, TRUE);
            ps->Release();
            PCMMLNode2 pNode = pNodeOld ? pNodeOld->CloneAsCMMLNode2 () : NULL;
            if (pNodeOld)
               delete pNodeOld;
            if (!pNode) {
               pRet->m_Var.SetNULL();
               return TRUE;
            }

            // potentially take only top node
            if (fOneMainEntry) {
               PWSTR psz;
               PCMMLNode2 pSub;
               pNode->ContentEnum (0, &psz, &pSub);
               if (!pSub) {
                  pRet->m_Var.SetNULL();
                  return TRUE;
               }

               pNode->ContentRemove (0, FALSE);
               delete pNode;
               pNode = pSub;
            } // if fOneMainEntry
            else
               pNode->NameSet (L"Main");   // so have something

            MMLToList (pNode, &pRet->m_Var);
            delete pNode;
            return TRUE;
         } // if not resource
      }
      return TRUE;


   case VMTOK_MMLFROMLIST:
      // converts from a list to MML string
      // 1st param is the list (equivalent to output of MMLToList())
      // 2nd param is TRUE if it's only one main entry, FALSE if multiple.
      {
         if (plParams->Num() != 2) {
            pRet->m_Var.SetUndefined ();
            return TRUE;
         }

         BOOL fOneMainEntry = plParams->Get(1)->GetBOOL (this);

         PCMIFLVar pv = plParams->Get(0);

         PCMMLNode2 pNode = MMLFromList (pv, this);
         if (!pNode) {
            pRet->m_Var.SetNULL ();
            return TRUE;
         }

         // make a string out of this
         CMem mem;
         if (!MMLToMem (pNode, &mem, !fOneMainEntry)) {
            pRet->m_Var.SetNULL ();
            return TRUE;
         }
         mem.CharCat (0);  // null terminate
         delete pNode;

         // set the string
         pRet->m_Var.SetString ((PWSTR)mem.p);
         return TRUE;
      }
      return TRUE;

   case VMTOK_RESOURCEGET:
      // gets a resoruce based on its name
      // 1st param = resource name as string
      {
         if (plParams->Num() != 1) {
            pRet->m_Var.SetUndefined ();
            return TRUE;
         }

         PCMIFLVar pv = plParams->Get(0);
         PCMIFLVarString ps = pv->GetString (this);
         if (!ps) {
            pRet->m_Var.SetNULL ();
            return TRUE;
         }

         // find it
         DWORD dwIndex = m_pCompiled->m_pLib->ResourceFind (ps->Get(), (DWORD)-1);
         ps->Release();
         if (dwIndex == (DWORD)-1) {
            // cant find
            pRet->m_Var.SetNULL ();
            return TRUE;
         }

         pRet->m_Var.SetResource (dwIndex);
      }
      return TRUE;

   case VMTOK_RESOURCEENUM:      // enumerate all the resources
      {
         // DOCUMENT: Returns a list with all the resources, with sub-list of
         // name and type
         
         PCMIFLVarList pl = new CMIFLVarList;
         if (!pl) {
            pRet->m_Var.SetUndefined();
            return TRUE;
         }

         DWORD i;
         CMIFLVar v;
         for (i = 0; i < m_pCompiled->m_pLib->ResourceNum(); i++) {
            PCMIFLResource pRes = m_pCompiled->m_pLib->ResourceGet(i);
            if (!pRet)
               continue;

            PCMIFLVarList pl2 = new CMIFLVarList;
            if (!pl2)
               continue;

            // name
            v.SetString ((PWSTR)pRes->m_memName.p);
            pl2->Add (&v, TRUE);

            // type
            v.SetString ((PWSTR)pRes->m_memType.p);
            pl2->Add (&v, TRUE);

            // add sub-list
            v.SetList (pl2);
            pl2->Release();
            pl->Add (&v, TRUE);
         } // i

         pRet->m_Var.SetList (pl);
         pl->Release();

      }
      return TRUE;

   case VMTOK_OBJECTENUM:      // enumerate all objects
      {
         // DOCUMENT: Returns a list with all the objects
         
         PCMIFLVarList pl = new CMIFLVarList;
         if (!pl) {
            pRet->m_Var.SetUndefined();
            return TRUE;
         }

         DWORD i;
         CMIFLVar v;
         for (i = 0; i < m_hObjects.Num(); i++) {
            PCMIFLVMObject po = *((PCMIFLVMObject*) m_hObjects.Get(i));

            v.SetObject (&po->m_gID);
            pl->Add (&v, TRUE);
         } // i

         pRet->m_Var.SetList (pl);
         pl->Release();

      }
      return TRUE;

case VMTOK_CLASSESENUM:      // enumerate all the classes
      {
         // DOCUMENT: Returns a list with all the class strings
         
         PCMIFLVarList pl = new CMIFLVarList;
         if (!pl) {
            pRet->m_Var.SetUndefined();
            return TRUE;
         }

         DWORD i;
         CMIFLVar v;
         DWORD dwNum = m_pCompiled->m_pLib->ObjectNum();
         for (i = 0; i < dwNum; i++) {
            PCMIFLObject po = m_pCompiled->m_pLib->ObjectGet (i);
            if (!po)
               continue;
            v.SetString ((PWSTR)po->m_memName.p);
            pl->Add (&v, TRUE);
         } // i

         pRet->m_Var.SetList (pl);
         pl->Release();

      }
      return TRUE;

   case VMTOK_OBJECTQUERY:      // see if an object still exists
      {
         // DOCUMENT: Queries if a object exists
         // Param1 = object
         // Returns - TRUE if exists, FALSE if didn't exist
         PCMIFLVar pv = plParams->Get(0);
         if (!pv || (pv->TypeGet() != MV_OBJECT)) {
            pRet->m_Var.SetUndefined ();
            return TRUE;
         }
         GUID g = pv->GetGUID();
         pRet->m_Var.SetBOOL (ObjectFind (&g) ? TRUE : FALSE);
      }
      return TRUE;


      // DOCUMENT: Math functions
   case VMTOK_MATH_ABS:      // absoluate value
      if (pv)
         pRet->m_Var.SetDouble (fabs(pv->GetDouble(this)));
      else
         pRet->m_Var.SetUndefined();
      return TRUE;

   case VMTOK_MATH_ACOS:      // arc cos
      if (pv)
         pRet->m_Var.SetDouble (acos(pv->GetDouble(this)));
      else
         pRet->m_Var.SetUndefined();
      return TRUE;

   case VMTOK_MATH_ASIN:      // arc sin
      if (pv)
         pRet->m_Var.SetDouble (asin(pv->GetDouble(this)));
      else
         pRet->m_Var.SetUndefined();
      return TRUE;

   case VMTOK_MATH_ATAN:      // arc tan
      if (pv)
         pRet->m_Var.SetDouble (atan(pv->GetDouble(this)));
      else
         pRet->m_Var.SetUndefined();
      return TRUE;

   case VMTOK_MATH_CEIL:      // ceiling
      if (pv)
         pRet->m_Var.SetDouble (ceil(pv->GetDouble(this)));
      else
         pRet->m_Var.SetUndefined();
      return TRUE;

   case VMTOK_MATH_COS:      // cos
      if (pv)
         pRet->m_Var.SetDouble (cos(pv->GetDouble(this)));
      else
         pRet->m_Var.SetUndefined();
      return TRUE;

   case VMTOK_MATH_EXP:      // exponent
      if (pv)
         pRet->m_Var.SetDouble (exp(pv->GetDouble(this)));
      else
         pRet->m_Var.SetUndefined();
      return TRUE;

   case VMTOK_MATH_FLOOR:      // floor
      if (pv)
         pRet->m_Var.SetDouble (floor(pv->GetDouble(this)));
      else
         pRet->m_Var.SetUndefined();
      return TRUE;

   case VMTOK_MATH_LOG:      // log
      if (pv)
         pRet->m_Var.SetDouble (log(pv->GetDouble(this)));
      else
         pRet->m_Var.SetUndefined();
      return TRUE;

   case VMTOK_MATH_LOG10:      // log 10
      if (pv)
         pRet->m_Var.SetDouble (log10(pv->GetDouble(this)));
      else
         pRet->m_Var.SetUndefined();
      return TRUE;

   case VMTOK_MATH_POW:      // power
      {
         pv2 = plParams->Get(1);
         if (pv && pv2)
            pRet->m_Var.SetDouble (pow (pv->GetDouble(this), pv2->GetDouble(this)));
         else
            pRet->m_Var.SetUndefined();
      }
      return TRUE;

   case VMTOK_MATH_SQRT:      // square root
      if (pv)
         pRet->m_Var.SetDouble (sqrt(pv->GetDouble(this)));
      else
         pRet->m_Var.SetUndefined();
      return TRUE;

   case VMTOK_MATH_TAN:      // tan
      if (pv)
         pRet->m_Var.SetDouble (tan(pv->GetDouble(this)));
      else
         pRet->m_Var.SetUndefined();
      return TRUE;

   case VMTOK_MATH_ISINFINITE:      // is # infinit
      if (pv)
         pRet->m_Var.SetBOOL (!_finite(pv->GetDouble(this)));
      else
         pRet->m_Var.SetUndefined();
      return TRUE;

   case VMTOK_MATH_ISNAN:      // is Nan
      if (pv)
         pRet->m_Var.SetBOOL (_isnan(pv->GetDouble(this)));
      else
         pRet->m_Var.SetUndefined();
      return TRUE;

   case VMTOK_MATH_MAX:
      {
         pv2 = plParams->Get(1);
         if (pv && pv2)
            pRet->m_Var.Set ((pv->Compare (pv2, FALSE, this) <= 0) ? pv2 : pv);
         else
            pRet->m_Var.SetUndefined();
      }
      return TRUE;

   case VMTOK_MATH_MIN:
      {
         pv2 = plParams->Get(1);
         if (pv && pv2)
            pRet->m_Var.Set ((pv->Compare (pv2, FALSE, this) >= 0) ? pv : pv2);
         else
            pRet->m_Var.SetUndefined();
      }
      return TRUE;

   case VMTOK_MATH_SIN:      // sine
      if (pv)
         pRet->m_Var.SetDouble (sin(pv->GetDouble(this)));
      else
         pRet->m_Var.SetUndefined();
      return TRUE;

   case VMTOK_MATH_COSH:      // hyperholic cos
      if (pv)
         pRet->m_Var.SetDouble (cosh(pv->GetDouble(this)));
      else
         pRet->m_Var.SetUndefined();
      return TRUE;

   case VMTOK_MATH_SINH:      // hyperbolic sin
      if (pv)
         pRet->m_Var.SetDouble (sinh(pv->GetDouble(this)));
      else
         pRet->m_Var.SetUndefined();
      return TRUE;

   case VMTOK_MATH_ROUND:      // round off
      if (pv)
         pRet->m_Var.SetDouble (floor(pv->GetDouble(this) + 0.5));
      else
         pRet->m_Var.SetUndefined();
      return TRUE;

   case VMTOK_MATH_TANH:      // hyperbolic tan
      if (pv)
         pRet->m_Var.SetDouble (tanh(pv->GetDouble(this)));
      else
         pRet->m_Var.SetUndefined();
      return TRUE;

   case VMTOK_MATH_ATAN2:
      {
         pv2 = plParams->Get(1);
         if (pv && pv2)
            pRet->m_Var.SetDouble (atan2 (pv->GetDouble(this), pv2->GetDouble(this)));
         else
            pRet->m_Var.SetUndefined();
      }
      return TRUE;

   case VMTOK_MATH_RANDOM:
      {
         // DOCUMENT: Random - If no parameters then 0..32767
         // If one parameteer...
         //    if list then randomly selecct element from list
         //    if not list then 0..value
         // if two parameters, then from min to max
         if (!pv) {
            pRet->m_Var.SetDouble (Rand());
            return TRUE;
         }

         pv2 = plParams->Get(1);
         if (pv->TypeGet() != MV_LIST) {
            double f1 = pv->GetDouble(this);
            double f2 = pv2 ? pv2->GetDouble(this) : 0;
            pRet->m_Var.SetDouble ((double)Rand() / (double)RAND_MAX * (f2 - f1) + f1);
            return TRUE;
         }

         // else, it's a list
         PCMIFLVarList pl = pv->GetList();
         DWORD dwNum = pl->Num();
         if (dwNum)
            pRet->m_Var.Set (pl->Get(Rand() % dwNum));
         else
            pRet->m_Var.SetUndefined ();
         pl->Release();
      }
      return TRUE;

   case VMTOK_MATH_RANDOMSEED:
      {
         DWORD dwSeed;
         if (pv)
            dwSeed = (DWORD)pv->GetDouble(this);
         else {
            LARGE_INTEGER liCount;
            QueryPerformanceCounter (&liCount);
            dwSeed = liCount.HighPart ^ liCount.LowPart;
         }
         SRand (dwSeed);
         pRet->m_Var.SetUndefined();
      }
      return TRUE;

   case VMTOK_ISBOOL:      // returns true if bool
      if (pv)
         pRet->m_Var.SetBOOL (pv->TypeGet() == MV_BOOL);
      else
         pRet->m_Var.SetUndefined();
      return TRUE;

   case VMTOK_ISCHARALPHA:
      if (pv)
         pRet->m_Var.SetBOOL (iswalpha (pv->GetChar(this)));
      else
         pRet->m_Var.SetUndefined();
      return TRUE;
   case VMTOK_ISCHARALPHANUM:
      if (pv)
         pRet->m_Var.SetBOOL (iswalnum (pv->GetChar(this)));
      else
         pRet->m_Var.SetUndefined();
      return TRUE;
   case VMTOK_ISCHARUPPER:
      if (pv)
         pRet->m_Var.SetBOOL (iswupper (pv->GetChar(this)));
      else
         pRet->m_Var.SetUndefined();
      return TRUE;
   case VMTOK_ISCHARLOWER:
      if (pv)
         pRet->m_Var.SetBOOL (iswlower (pv->GetChar(this)));
      else
         pRet->m_Var.SetUndefined();
      return TRUE;
   case VMTOK_ISCHARSPACE:
      if (pv)
         pRet->m_Var.SetBOOL (iswspace (pv->GetChar(this)));
      else
         pRet->m_Var.SetUndefined();
      return TRUE;
   case VMTOK_ISCHARDIGIT:
      if (pv)
         pRet->m_Var.SetBOOL (iswdigit (pv->GetChar(this)));
      else
         pRet->m_Var.SetUndefined();
      return TRUE;

   case VMTOK_ISCHAR:      // returns true if char
      if (pv)
         pRet->m_Var.SetBOOL (pv->TypeGet() == MV_CHAR);
      else
         pRet->m_Var.SetUndefined();
      return TRUE;

   case VMTOK_ISFUNCTION:      // returns true if func
      if (pv)
         pRet->m_Var.SetBOOL ((pv->TypeGet() == MV_FUNC) || (pv->TypeGet() == MV_OBJECTMETH));
      else
         pRet->m_Var.SetUndefined();
      return TRUE;

   case VMTOK_ISMETHOD:      // returns true if method
      if (pv)
         pRet->m_Var.SetBOOL (pv->TypeGet() == MV_METH);
      else
         pRet->m_Var.SetUndefined();
      return TRUE;

   case VMTOK_ISNUMBER:      // returns true if number
      if (pv)
         pRet->m_Var.SetBOOL (pv->TypeGet() == MV_DOUBLE);
      else
         pRet->m_Var.SetUndefined();
      return TRUE;

   case VMTOK_ISSTRING:      // returns true if string
      if (pv)
         pRet->m_Var.SetBOOL ((pv->TypeGet() == MV_STRING) || (pv->TypeGet() == MV_STRINGTABLE));
      else
         pRet->m_Var.SetUndefined();
      return TRUE;

   case VMTOK_ISOBJECT:      // returns true if object
      if (pv)
         pRet->m_Var.SetBOOL (pv->TypeGet() == MV_OBJECT);
      else
         pRet->m_Var.SetUndefined();
      return TRUE;

   case VMTOK_ISLIST:      // returns true if list
      if (pv)
         pRet->m_Var.SetBOOL (pv->TypeGet() == MV_LIST);
      else
         pRet->m_Var.SetUndefined();
      return TRUE;

   case VMTOK_ISRESOURCE:      // returns true if resource
      if (pv)
         pRet->m_Var.SetBOOL (pv->TypeGet() == MV_RESOURCE);
      else
         pRet->m_Var.SetUndefined();
      return TRUE;

   case VMTOK_OBJECTTOGUIDSTRING:      // converts an object to a guid stirng
      if (pv && (pv->TypeGet() == MV_OBJECT)) {
         WCHAR szTemp[64];
         GUID g = pv->GetGUID ();
         MMLBinaryToString ((PBYTE) &g, sizeof(g), szTemp);
         pRet->m_Var.SetString (szTemp, (DWORD)-1);
      }
      else
         pRet->m_Var.SetUndefined();
      return TRUE;

   case VMTOK_GUIDSTRINGTOOBJECT:      // converts a guid stirng to an object
      {
         PCMIFLVarString ps = pv->GetString (this);
         GUID g;
         memset (&g, 0, sizeof(g));
         if (sizeof(g) == MMLBinaryFromString (ps->Get(), (PBYTE)&g, sizeof(g)))
            pRet->m_Var.SetObject (&g);
         else
            pRet->m_Var.SetUndefined();
         ps->Release();
      }
      return TRUE;

   default:
      // unknown
      return FALSE;
   }

   return TRUE;
}



/**********************************************************************************
CMIFLVM::Constructor and destructor
*/
CMIFLVM::CMIFLVM (void)
{
   m_hObjects.Init (sizeof(PCMIFLVMObject));
   m_hGlobals.Init (sizeof(PCMIFLVarProp));
   m_lObjectsToDel.Init (sizeof(PCMIFLVMObject));
   m_hIDToVMTOK.Init (sizeof(DWORD));
   m_hVMTOKToID.Init (sizeof(DWORD));
   m_lPCMIFLVMTimer.Init (sizeof(PCMIFLVMTimer));
   m_pDebugWindowOwn = NULL;
   m_hWndDebugParent = NULL;
   m_dwTimerAutoID = 0;
   m_fDebugUIIn = FALSE;
   m_pCompiled = NULL;  // BUGFIX - To clear

   m_dwMaxFuncLevelDeep = 100;
   m_dwMaxLoopCount = 1000000;
   m_fTimeCalibrate = 0;
   memset (&m_iTimeCalibrate, 0, sizeof(m_iTimeCalibrate));

   // keep track of the performance counter
   QueryPerformanceFrequency (&m_iPerCountFreq);

   // DOCUMENT: Maximum loop count, so don't get infinitely recursive
   // DOCUMENT: Maximum functon depth

   Clear();
}

CMIFLVM::~CMIFLVM (void)
{
   Clear();
}


/**********************************************************************************
CMIFLVM::Init - Initializes the VM to use the compiled code. It also sets up any
globals and objects (checked as autocreate).

inputs
   PCMIFLCompiled       pCompiled - Compiled object. This must remain valid while
                        the VM is in use. This separation allows several VMs to
                        run off one set of compiled code.
   PCEscWindow          pDebugWindow - Window to debug, same as calling DebugWindowSet().
                        This needs to be called so that constructors will be called
   HWND                 hWndParent - Parent window used in case needs to create own debug window
   DWORD                dwDebugMode - Debug mode to use, same as calling DebugModeSet()
   PCMMLNode2            pNode - If NULL then initialezed based on the default compiled state.
                        If non-NULL, then assume that MMLTo(fAll=TRUE) had been called, and that
                        initializing from this, reloading old state.
returns
   BOOL - TRUE if success
*/
BOOL CMIFLVM::Init (PCMIFLCompiled pCompiled, PCEscWindow pDebugWindow, HWND hWndParent,
                    DWORD dwDebugMode, PCMMLNode2 pNode)
{
   Clear();

   m_pCompiled = pCompiled;
   m_pSocket = pCompiled->m_pSocket;

   // create the table that goes from an ID (from m_pCompiled->m_hIdentifiers) to
   // a VMTOK_XXX, which is used for built-in functions like "Constructor" and "Destructor"
   m_hIDToVMTOK.Clear();
   m_hVMTOKToID.Clear();

   VMTOKAdd (L"constructor", VMTOK_CONSTRUCTOR);
   VMTOKAdd (L"constructor2", VMTOK_CONSTRUCTOR2);
   VMTOKAdd (L"destructor", VMTOK_DESTRUCTOR);
   VMTOKAdd (L"name", VMTOK_NAME);

   // string
   VMTOKAdd (L"stringconcat", VMTOK_STRINGCONCAT);
   VMTOKAdd (L"stringfromchar", VMTOK_STRINGFROMCHAR);
   VMTOKAdd (L"stringsearch", VMTOK_STRINGSEARCH);
   VMTOKAdd (L"stringlength", VMTOK_STRINGLENGTH);
   VMTOKAdd (L"stringslice", VMTOK_STRINGSLICE);
   VMTOKAdd (L"stringsubstring", VMTOK_STRINGSUBSTRING);
   VMTOKAdd (L"stringsplit", VMTOK_STRINGSPLIT);
   VMTOKAdd (L"stringtoupper", VMTOK_STRINGTOUPPER);
   VMTOKAdd (L"stringtolower", VMTOK_STRINGTOLOWER);
   VMTOKAdd (L"stringcompare", VMTOK_STRINGCOMPARE);
   VMTOKAdd (L"stringinsert", VMTOK_STRINGINSERT);
   VMTOKAdd (L"stringprepend", VMTOK_STRINGPREPEND);
   VMTOKAdd (L"stringappend", VMTOK_STRINGAPPEND);
   VMTOKAdd (L"stringreplace", VMTOK_STRINGREPLACE);
   VMTOKAdd (L"stringformat", VMTOK_STRINGFORMAT);
   VMTOKAdd (L"stringtrim", VMTOK_STRINGTRIM);
   VMTOKAdd (L"clone", VMTOK_CLONE);

   // list
   VMTOKAdd (L"listconcat", VMTOK_LISTCONCAT);
   VMTOKAdd (L"listmerge", VMTOK_LISTMERGE);
   VMTOKAdd (L"listnumber", VMTOK_LISTNUMBER);
   VMTOKAdd (L"listremove", VMTOK_LISTREMOVE);
   VMTOKAdd (L"listreverse", VMTOK_LISTREVERSE);
   VMTOKAdd (L"listslice", VMTOK_LISTSLICE);
   VMTOKAdd (L"listsublist", VMTOK_LISTSUBLIST);
   VMTOKAdd (L"listsort", VMTOK_LISTSORT);
   VMTOKAdd (L"listsearch", VMTOK_LISTSEARCH);
   VMTOKAdd (L"listreplace", VMTOK_LISTREPLACE);
   VMTOKAdd (L"listinsert", VMTOK_LISTINSERT);
   VMTOKAdd (L"listappend", VMTOK_LISTAPPEND);
   VMTOKAdd (L"listprepend", VMTOK_LISTPREPEND);
   VMTOKAdd (L"listrandomize", VMTOK_LISTRANDOMIZE);
   VMTOKAdd (L"listsearchtoinsert", VMTOK_LISTSEARCHTOINSERT);

   // automatic to objects
   VMTOKAdd (L"containedinget", VMTOK_CONTAINEDINGET);
   VMTOKAdd (L"containedinset", VMTOK_CONTAINEDINSET);
   VMTOKAdd (L"containsenum", VMTOK_CONTAINSGET);
   VMTOKAdd (L"classenum", VMTOK_CLASSENUM);
   VMTOKAdd (L"classquery", VMTOK_CLASSQUERY);
   VMTOKAdd (L"propertyset", VMTOK_PROPERTYSET);
   VMTOKAdd (L"propertyget", VMTOK_PROPERTYGET);
   VMTOKAdd (L"propertyremove", VMTOK_PROPERTYREMOVE);
   VMTOKAdd (L"propertyquery", VMTOK_PROPERTYQUERY);
   VMTOKAdd (L"propertyenum", VMTOK_PROPERTYENUM);
   VMTOKAdd (L"methodcall", VMTOK_METHODCALL);
   VMTOKAdd (L"methodquery", VMTOK_METHODQUERY);
   VMTOKAdd (L"methodenum", VMTOK_METHODENUM);
   VMTOKAdd (L"deletewithcontents", VMTOK_DELETETREE);
   VMTOKAdd (L"layernumber", VMTOK_LAYERNUM);
   VMTOKAdd (L"layerget", VMTOK_LAYERGET);
   VMTOKAdd (L"layeradd", VMTOK_LAYERADD);
   VMTOKAdd (L"layerremove", VMTOK_LAYERREMOVE);
   VMTOKAdd (L"layermethodenum", VMTOK_LAYERMETHODENUM);
   VMTOKAdd (L"layermethodadd", VMTOK_LAYERMETHODADD);
   VMTOKAdd (L"layermethodremove", VMTOK_LAYERMETHODREMOVE);
   VMTOKAdd (L"layerpropgetsetenum", VMTOK_LAYERPROPERTYENUM);
   VMTOKAdd (L"layerpropgetsetadd", VMTOK_LAYERPROPERTYADD);
   VMTOKAdd (L"layerpropgetsetremove", VMTOK_LAYERPROPERTYREMOVE);
   VMTOKAdd (L"timersuspendset", VMTOK_TIMERSUSPENDSET);
   VMTOKAdd (L"timersuspendget", VMTOK_TIMERSUSPENDGET);
   VMTOKAdd (L"timeradd", VMTOK_TIMERADD);
   VMTOKAdd (L"timerremove", VMTOK_TIMERREMOVE);
   VMTOKAdd (L"timerenum", VMTOK_TIMERENUM);
   VMTOKAdd (L"timerquery", VMTOK_TIMERQUERY);

   // prepare debug window and such
   DebugWindowSet (pDebugWindow);
   DebugWindowSetParent (hWndParent);
   DebugModeSet (dwDebugMode);

   if (pNode) {
      // else, load in
      if (!MMLFrom (TRUE, TRUE, TRUE, TRUE, pNode, NULL))
         return FALSE;
   }
   else {
      // creating from clean slate

      // copy all the variables over, and then fracture them
      m_pCompiled->m_hGlobalsDefault.CloneTo (&m_hGlobals);
      DWORD i;
      for (i = 0; i < m_hGlobals.Num(); i++) {
         PCMIFLVarProp pv = (PCMIFLVarProp)m_hGlobals.Get(i);
         pv->m_Var.Fracture(FALSE);

         // DOCUMENT: Potential problems fracture might cause on initialized variables.
         // If two lists somehow point to the same list they will no longer be
         // pointing to the same list after the fracture call
      } // i

      // go through all the objects and create ones
      m_hObjects.TableResize (m_pCompiled->m_hObjectIDs.Num()*3);
      for (i = 0; i < m_pCompiled->m_hObjectIDs.Num(); i++) {
         PMIFLIDENT pmi = (PMIFLIDENT)m_pCompiled->m_hObjectIDs.Get(i);
         if (pmi->dwType != MIFLI_OBJECT)
            continue;   // shouldnt happen

         // create
         PCMIFLVMObject pNew = new CMIFLVMObject;
         if (!pNew)
            return FALSE;
         if (!pNew->InitAsAutoCreate (this, (PCMIFLObject)pmi->pEntity)) {
            delete pNew;
            return FALSE;
         }

         // add this
         m_hObjects.Add (&pNew->m_gID, &pNew);
      } // i

      // call all the constructors
      CMIFLVarLValue var;
      for (i = 0; i < m_pCompiled->m_hObjectIDs.Num(); i++) {
         PMIFLIDENT pmi = (PMIFLIDENT)m_pCompiled->m_hObjectIDs.Get(i);
         if (pmi->dwType != MIFLI_OBJECT)
            continue;   // shouldnt happen

         // get the GUID
         PCMIFLObject po = (PCMIFLObject)pmi->pEntity;

         // run the constructor...
         MethodCallVMTOK (&po->m_gID, VMTOK_CONSTRUCTOR, NULL, 0, 0, &var);
      } // i
   } // create from clean slate

   // DOCUMENT: Explain how constructors will be called after everything else initialied
   // DOCUMENT: Explain that as much as possible done in init varialbes so that
   //    when save will be able to save the smallest footprint possible

   return TRUE;
}



/**********************************************************************************
CMIFLVM::VMTOKAdd - Adds a token to the list that's used to identify VMTOK_XXX.

NOTE: The whole VMTOK_XXX automatic functions for strings, lists, and objects
assumes that the ID for the strings, lists, and object methods are unique, which
they are. However, if ever start converting global IDs, or others, then the
IDs will not longer be unique

inputs
   PWSTR       pszName - Name of the token, all lower case
   DWORD       dwID - VMTOK_XXX
*/
void CMIFLVM::VMTOKAdd (PWSTR pszName, DWORD dwID)
{
   PMIFLIDENT pmi;

   pmi = (PMIFLIDENT)m_pCompiled->m_hIdentifiers.Find (pszName, FALSE);
   if (pmi && (pmi->dwType == MIFLI_METHDEF)) {
      m_hIDToVMTOK.Add (pmi->dwID, &dwID);
      m_hVMTOKToID.Add (dwID, &pmi->dwID);
   };

}

/**********************************************************************************
CMIFLVM::IDToVMTOK - Given an ID (from the m_pCompiled ID set), this sees if it
matches one of the internal tokens. If it does then the VMTOK_XXX is returned.
Otherwise -1 is returned.
*/
DWORD CMIFLVM::IDToVMTOK (DWORD dwID)
{
   DWORD *pdw = (DWORD*) m_hIDToVMTOK.Find (dwID);
   if (!pdw)
      return -1;
   return pdw[0];
}


/**********************************************************************************
CMIFLVM::VMTOKToID - Given an VMTOK_XXX, this converts to to an ID
(from the m_pCompiled ID set). If it succededs returns the ID,
Otherwise -1 is returned.
*/
DWORD CMIFLVM::VMTOKToID (DWORD dwVMTOK)
{
   DWORD *pdw = (DWORD*) m_hVMTOKToID.Find (dwVMTOK);
   if (!pdw)
      return -1;
   return pdw[0];
}


/**********************************************************************************
CMIFLVM::Clear - Frees up all the memory (objects and globals) used by the VM.
*/
void CMIFLVM::Clear (void)
{
   // kill the automatic timer
   if (m_dwTimerAutoID) {
      KillTimer (NULL, m_dwTimerAutoID);

      // kill it from the list
      PTIDTOVM pt = (PTIDTOVM)glTIDTOVM.Get(0);
      DWORD i;
      for (i = 0; i < glTIDTOVM.Num(); i++, pt++)
         if (pt->dwTimerID == m_dwTimerAutoID) {
            glTIDTOVM.Remove (i);
            break;
         }

      m_dwTimerAutoID = 0;
   }

   // make sure all the objects on the "to-delete" queue are gone
   MaintinenceDeleteAll ();

   // go through all the objects and delete
   DWORD i;
   for (i = 0; i < m_hObjects.Num(); i++) {
      PCMIFLVMObject pv = *((PCMIFLVMObject*) m_hObjects.Get(i));
      delete pv;  // dont need to to clean delete because it's all shutting down
   } // i
   m_hObjects.Clear();

   // go through  all the variables and free
   for (i = 0; i < m_hGlobals.Num(); i++) {
      PCMIFLVarProp pv = (PCMIFLVarProp)m_hGlobals.Get(i);
      pv->m_Var.SetUndefined();
   } // i
   m_hGlobals.Clear();

   // free up timers
   PCMIFLVMTimer *ppt = (PCMIFLVMTimer*)m_lPCMIFLVMTimer.Get(0);
   for (i = 0; i < m_lPCMIFLVMTimer.Num(); i++)
      delete ppt[i];
   m_lPCMIFLVMTimer.Clear();

   m_dwDebugListMajor = 0;
   m_dwDebugListMinor = 0;
   memset (&m_gDebugListGUID, 0, sizeof(m_gDebugListGUID));
   m_lDebugVars.Clear();
   memset (&m_gDebugVarsObject, 0, sizeof(m_gDebugVarsObject));
   m_pDebugVarsObjectLayer = NULL;
   m_phDebugVarsString = NULL;
   m_paDebugVars = NULL;
   m_dwDebugVarsNum = 0;
   MemZero (&m_memDebug);
   m_memDebug.m_dwCurPosn = 0;

   m_dwDebugMode = MDM_IGNOREALL;
   m_pDebugWindowApp = NULL;

   if (m_pDebugWindowOwn)
      delete m_pDebugWindowOwn;
   m_pDebugWindowOwn = NULL;

   m_pFCICur = NULL;

   m_dwFuncLevelDeep = 0;

   // temporary identifiers
   m_hIdentifiersCustomGlobal.Init (sizeof(MIFLIDENT));
   m_hUnIdentifiersCustomGlobal.Init (0);
   m_hIdentifiersCustomMethod.Init (sizeof(MIFLIDENT));
   m_hUnIdentifiersCustomMethod.Init (0);
   m_hIdentifiersCustomProperty.Init (sizeof(MIFLIDENT));
   m_hUnIdentifiersCustomProperty.Init (0);
   m_dwIDMethPropCur = VM_CUSTOMIDRANGE;

   // language
   m_LangID = GetSystemDefaultLangID();

   // BUGFIX - Better seed
   DWORD dwSeed;
   LARGE_INTEGER liCount;
   QueryPerformanceCounter (&liCount);
   dwSeed = liCount.HighPart ^ liCount.LowPart;
   SRand (dwSeed);   // so have some randomness
   // done
}


/**********************************************************************************
CMIFLVM::ObjectFind - Looks for an object with the given GUID

NOTE: Don't delete this or change the guid.

inputs
   GUID           *pgID - ID
*/
PCMIFLVMObject CMIFLVM::ObjectFind (GUID *pgID)
{
   PCMIFLVMObject *ppo = (PCMIFLVMObject*) m_hObjects.Find (pgID);
   if (!ppo)
      return NULL;

   return *ppo;
}


/**********************************************************************************
CMIFLVM::ObjectDelete - Deletes the given object in a nice way. This will
1) call the destructor 
2) Detach it from parent and children
3) Kill timers associated with it
4) Move it to the "to-delete" list so that when MaintinenceDeleteAll() is finally
   called it's safely removed - otherwise might delete it while its code is being run

DOCUMENT: Doc how deleting an object works and process of calls
DOCUMENT: Likewise document creation of an object and process of calls

inputs
   GUID           *pgID - ID
returns
   BOOL - TRUE if success
*/
BOOL CMIFLVM::ObjectDelete (GUID *pgID)
{
   DWORD dwIndex = m_hObjects.FindIndex (pgID);
   if (dwIndex == -1)
      return FALSE;  // not there

   // inform the app it's about to be deleted
   if (m_pSocket)
      m_pSocket->VMObjectDelete (this, pgID);

   PCMIFLVMObject po = *((PCMIFLVMObject*)m_hObjects.Get(dwIndex));

   // call destructor
   CMIFLVarLValue var;
   MethodCallVMTOK (pgID, VMTOK_DESTRUCTOR, NULL, 0, 0, &var);
   // DOCUMENT: object should not delete itself in its destructor
   // DOCUMENT: if sudden shutdown of code then destructors won't be called,
   // so generally, destructors only called when objects deleted one at a time

   // nice detach
   po->ContainDisconnectAll();

   // kill timers
   po->TimerRemoveAll ();

   // move to the "to-delete" list
   // BUGFIX - Because calling the destructor might cause other objects to
   // be deleted, find this object again befor removing it
   dwIndex = m_hObjects.FindIndex (pgID);
   if (dwIndex != -1)
      m_hObjects.Remove (dwIndex);
   m_lObjectsToDel.Add (&po);
   return TRUE;
}




/**********************************************************************************
CMIFLVM::ObjectReplace - Replaces the given object with a new one.
   Deletes the given object in a nice way. This will
1) call the destructor 
2) Detach it from parent and children
3) Kill timers associated with it
4) Move it to the "to-delete" list so that when MaintinenceDeleteAll() is finally
   called it's safely removed - otherwise might delete it while its code is being run

DOCUMENT: Doc how deleting an object works and process of calls
DOCUMENT: Likewise document creation of an object and process of calls

inputs
   GUID           *pgID - ID
   PCMIFLVMObject pNew - New object to put in place of the old. This should
                  have the same GUID as pgID
   PCListFixed    plContainIn - This should be initialized to sizeof(GUID)*2.
                  It may be added to if this object needs to be placed inside
                  another object. This first GUID will be this object,
                  and the second will be the object it should be in
returns
   BOOL - TRUE if success
*/
BOOL CMIFLVM::ObjectReplace (GUID *pgID, PCMIFLVMObject pNew, PCListFixed plContiainIn)
{
   // make sure pNew OK to replace
   if (pNew->m_lContains.Num())
      return FALSE;  // cant replace if already linked

   DWORD dwIndex = m_hObjects.FindIndex (pgID);
   if (dwIndex == -1)
      return FALSE;  // not there

   PCMIFLVMObject *ppo = (PCMIFLVMObject*)m_hObjects.Get(dwIndex);
   PCMIFLVMObject po = *ppo;

   // call destructor
   CMIFLVarLValue var;
   MethodCallVMTOK (pgID, VMTOK_DESTRUCTOR, NULL, 0, 0, &var);
   // DOCUMENT: object should not delete itself in its destructor
   // DOCUMENT: if sudden shutdown of code then destructors won't be called,
   // so generally, destructors only called when objects deleted one at a time

   // nice detach
   //po->ContainDisconnectAll();
   po->ContainedBySet(NULL);
   GUID ag[2];
   ag[1] = pNew->m_gContainedIn;
   pNew->m_gContainedIn = GUID_NULL;
   pNew->m_lContains.Init (sizeof(GUID), po->m_lContains.Get(0), po->m_lContains.Num());

   // kill timers
   po->TimerRemoveAll ();

   // move to the "to-delete" list
   //m_hObjects.Remove (dwIndex);
   *ppo = pNew;   // move the usurper into the list
   m_lObjectsToDel.Add (&po);

   // set new container
   if (!IsEqualGUID (ag[1], GUID_NULL)) {
      ag[0] = pNew->m_gID;
      // ag[1] already set
      plContiainIn->Add (&ag[0]);
   }
   return TRUE;
}



/**********************************************************************************
CMIFLVM::ObjectDeleteFamily - Deletes the object and all its children

inputs
   GUID           *pgID - ID
returns
   BOOL - TRUE if success
*/
BOOL CMIFLVM::ObjectDeleteFamily (GUID *pgID)
{
   DWORD dwIndex = m_hObjects.FindIndex (pgID);
   if (dwIndex == -1)
      return FALSE;  // not there

   PCMIFLVMObject po = *((PCMIFLVMObject*)m_hObjects.Get(dwIndex));

   // make a list of its children
   CListFixed l;
   l.Init (sizeof(GUID), po->m_lContains.Get(0), po->m_lContains.Num());

   // delete all children
   GUID *pg = (GUID*)l.Get(0);
   DWORD i;
   for (i = 0; i < l.Num(); i++, pg++)
      ObjectDeleteFamily (pg);

   // BUGFIX - Moved deleting this object until AFTER deleted children
   // so that would work better for checkin

   // delete this
   ObjectDelete (pgID);

   return TRUE;
}

/**********************************************************************************
CMIFLVM::MaintinenceDeleteAll - This should be called AFTER the VM has returned from
any code it was running. It checks to see if any objects were deleting in the code,
and if so, finally deletes them. This must be done after it returns so an object
doesn't delete itself and cause a crash.
*/
void CMIFLVM::MaintinenceDeleteAll (void)
{
   if (!m_lObjectsToDel.Num())
      return;

   DWORD i;
   PCMIFLVMObject *ppo = (PCMIFLVMObject*)m_lObjectsToDel.Get(0);
   for (i = 0; i < m_lObjectsToDel.Num(); i++)
      ppo[i]->CleanDelete();
   m_lObjectsToDel.Clear();
}



/**********************************************************************************
CMIFLVM::DebugVarToName - Takes a variable in pVar and converts it to a string
name that can be displayed in the debug process.

inputs
   PCMIFLVar         pVar - Variable to convert
   BOOL              fJustName - If TRUE then just the name, ELSE also include string
*/
void CMIFLVM::DebugVarToName (PCMIFLVar pVar, BOOL fJustName)
{
   DWORD dwType = pVar->TypeGet();

   switch (dwType) {
   case MV_DOUBLE:
   case MV_NULL:
   case MV_BOOL:
   default:
      pVar->ToString (this);
      return;

   case MV_LIST:       // memory points to PCMIFLVarList
      {
         pVar->ToString (this);
         PCMIFLVarString ps = pVar->GetString (this);

         ps->Prepend (L"[", (DWORD)-1);
         ps->Append (L"]", (DWORD)-1);

         ps->Truncate (TRUNCATETO, TRUE);  // make sure not too long
         ps->Release();
      }
      return;

   case MV_LISTMETH:
      {
         // store away the value
         DWORD dwID = pVar->GetValue();

         // convert to only a list, no value
         PCMIFLVarList pl = pVar->GetList();
         pVar->SetList (pl);
         pl->Release();

         pVar->ToString(this);

         PCMIFLVarString ps = pVar->GetString(this);

         ps->Prepend (L"[", (DWORD)-1);
         ps->Append (L"]", (DWORD)-1);

         ps->Append (L".", (DWORD)-1);
         PMIFLIDENT pFind = (PMIFLIDENT) m_pCompiled->m_hUnIdentifiers.Find (dwID);
         PWSTR psz = L"Unknown method";
         if (pFind->dwType == MIFLI_METHDEF)
            psz = (PWSTR) ((PCMIFLMeth)pFind->pEntity)->m_memName.p;
         ps->Append (psz, (DWORD)-1);

         ps->Truncate (TRUNCATETO, TRUE);  // make sure not too long
         ps->Release();
      }
      return;

   case MV_UNDEFINED:
      pVar->SetString (L"undefined", (DWORD)-1);
      return;

   case MV_CHAR:
      {
         WCHAR szTemp[32];
         WCHAR c = pVar->GetChar (this);
         szTemp[0] = L'\'';
         szTemp[1] = 0;
         switch (c) {
         case '\0':
            wcscat (szTemp, L"\\0");
            break;
         case '\r':
            wcscat (szTemp, L"\\r");
            break;
         case '\n':
            wcscat (szTemp, L"\\n");
            break;
         default:
            szTemp[1] = c;
            szTemp[2] = 0;
            break;
         }
         wcscat (szTemp, L"'");

         pVar->SetString (szTemp, (DWORD)-1);
      }
      return;

   case MV_STRING:       // memory points to a PCMIFLVarString
   case MV_STRINGMETH:
      {
         PCMIFLVarString ps = pVar->GetStringNoMod ();
         ps->Prepend (L"\"", (DWORD)-1);
         // NOTE: theroetically should change back to "\n" and "\r", but not worth it
         ps->Append (L"\"", (DWORD)-1);

         if (dwType == MV_STRINGMETH) {
            ps->Append (L".", (DWORD)-1);
            PMIFLIDENT pFind = (PMIFLIDENT) m_pCompiled->m_hUnIdentifiers.Find (pVar->GetValue());
            PWSTR psz = L"Unknown method";
            if (pFind->dwType == MIFLI_METHDEF)
               psz = (PWSTR) ((PCMIFLMeth)pFind->pEntity)->m_memName.p;
            ps->Append (psz, (DWORD)-1);

            // set pVar to this
            pVar->SetString (ps);
         }

         ps->Truncate (TRUNCATETO, TRUE);  // make sure not too long
         ps->Release();
      }
      return;


   case MV_STRINGTABLE:       // string table. dwValue is the ID
      {
         PCMIFLString pst = m_pCompiled->m_pLib->StringGet (pVar->GetValue());
         PWSTR psz;
         if (!pst)
            psz = L"Unknown string table";
         else
            psz = (PWSTR) pst->m_memName.p;

         if (fJustName) {
            pVar->SetString (psz, (DWORD)-1);
            return;
         }

         // else, convert to string and prepend
         pVar->ToString (this);

         PCMIFLVarString ps = pVar->GetString (this);
         ps->Prepend (L": \"", (DWORD)-1);
         ps->Prepend (psz, (DWORD)-1);
         // NOTE: theroetically should change back to "\n" and "\r", but not worth it
         ps->Append (L"\"", (DWORD)-1);
         ps->Truncate (TRUNCATETO, TRUE);  // make sure not too long
         ps->Release();
      }
      return;

   case MV_RESOURCE:       // string table. dwValue is the ID
      {
         PCMIFLResource pRes = m_pCompiled->m_pLib->ResourceGet (pVar->GetValue());
         PWSTR psz;
         if (!pRes)
            psz = L"Unknown resource";
         else
            psz = (PWSTR) pRes->m_memName.p;
         pVar->SetString (psz, (DWORD)-1);
      }
      return;

   case MV_FUNC:       // function call. dwValue is the ID
      {
         PCMIFLFunc pFunc = m_pCompiled->m_pLib->FuncGet (pVar->GetValue());
         PWSTR psz;
         if (!pFunc)
            psz = L"Unknown function";
         else
            psz = (PWSTR) pFunc->m_Meth.m_memName.p;
         pVar->SetString (psz, (DWORD)-1);
      }
      return;

   case MV_OBJECT:       // object GUID stored
   case MV_METH:       // public/private method, no object. dwValue is the ID
   case MV_OBJECTMETH:       // public/private method with associated object, dwValue is ID, plus GUID
      {
         // first, find the object
         PCMIFLVMObject pObj = NULL;
         BOOL fDerivedFrom = FALSE;
         WCHAR szGUID[64];
         szGUID[0] = 0;
         if (dwType != MV_METH) {
            GUID gObject = pVar->GetGUID();
            PCMIFLVMObject *pp = (PCMIFLVMObject*)m_hObjects.Find (&gObject);
            if (pp)
               pObj = *pp;
            StringFromGUID2 (gObject, szGUID, sizeof(szGUID)/sizeof(WCHAR));
         }
         PWSTR pszObject;

         // BUGFIX - Set derived from based on if original object
         fDerivedFrom = pObj ? (m_pCompiled->m_hObjectIDs.FindIndex (&pObj->m_gID) == -1) : FALSE;

         if (!pObj)
            pszObject = L"Unknown object";
         else {
            //if (pObj->m_pObject)
            //   pszObject = (PWSTR)pObj->m_pObject->m_memName.p;
            //else {
            //   fDerivedFrom = TRUE;
               PCMIFLVMLayer pl = pObj->LayerGet (pObj->LayerNum()-1);
               if (pl)
                  pszObject = (PWSTR) pl->m_memName.p;
               else
                  pszObject = L"Unknown object";
            //}
         } // if object

         // find the method name
         DWORD dwID = pVar->GetValue();
         PWSTR psz = NULL;
         if (dwID >= VM_CUSTOMIDRANGE) {
            // custom ID, so find
            DWORD dwIndex = m_hUnIdentifiersCustomMethod.FindIndex (dwID);
            psz = m_hIdentifiersCustomMethod.GetString (dwIndex);
         }
         else {
            // string it in the standard library
            PMIFLIDENT pFind = (dwType != MV_OBJECT) ? (PMIFLIDENT) m_pCompiled->m_hUnIdentifiers.Find (dwID) : NULL;
            if (pFind) {
               if (pFind->dwType == MIFLI_METHPRIV)
                  psz = (PWSTR) ((PCMIFLFunc)pFind->pEntity)->m_Meth.m_memName.p;
               else if (pFind->dwType == MIFLI_METHDEF)
                  psz = (PWSTR) ((PCMIFLMeth)pFind->pEntity)->m_memName.p;
            }
         }
         if (!psz)
            psz = L"Unknown method";

         // put it together...
         PCMIFLVarString ps = new CMIFLVarString;
         if ((dwType == MV_OBJECT) || (dwType == MV_OBJECTMETH)) {
            ps->Set (pszObject, (DWORD)-1);
            if (fDerivedFrom) {
               ps->Append (L"* ", (DWORD)-1);
               ps->Append (szGUID, (DWORD)-1);
               }

            if (dwType == MV_OBJECTMETH) {
               ps->Append (L".", (DWORD)-1);
               ps->Append (psz, (DWORD)-1);
            }
         }
         else { // dwType == MV_METH
            ps->Append (psz, (DWORD)-1);
         }
         pVar->SetString (ps);
         ps->Release();
      }
      return;
   } // switch dwType

}


/**********************************************************************************
CMIFLVM::WatchVarShow - Sets the controls in the debug page to show the watch
variables.

inputs
   PCEscPage         pPage - Page
   DWORD             dwNum - Which watch variable
   BOOL              fVarText - If TRUE then set the variable text in addition to
                     the value
returns
   BOOL - TRUE if success
*/
BOOL CMIFLVM::WatchVarShow (PCEscPage pPage, DWORD dwNum, BOOL fVarText)
{
   // get the controls
   PCEscControl pcVar, pcVal;
   WCHAR szTemp[64];
   swprintf (szTemp, L"dvvar:%d", (int)dwNum);
   pcVar = pPage->ControlFind (szTemp);
   swprintf (szTemp, L"dvval:%d", (int)dwNum);
   pcVal = pPage->ControlFind (szTemp);
   if (!pcVar || !pcVal)
      return FALSE;

   // find the value in the list
   PWSTR pszVar = (PWSTR)m_lDebugVars.Get(dwNum);
   if (!pszVar)
      pszVar = L"";

   // set
   if (fVarText)
      pcVar->AttribSet (Text(), pszVar);

   // value
   if (!pszVar[0]) {
      // nothing here
      pcVal->AttribSet (Text(), L"");
      return TRUE;
   }

   // get the object
   PCMIFLVMObject pObject = NULL;
   PCMIFLObject pObjectLayer = NULL;
   if (!IsEqualGUID (m_gDebugVarsObject, GUID_NULL)) {
      pObject = ObjectFind (&m_gDebugVarsObject);
      pObjectLayer = m_pDebugVarsObjectLayer;
   }

   // else, compile
   CMIFLVar var;
   if (!VarAccess (pszVar, pObject, pObjectLayer, m_phDebugVarsString,
      m_paDebugVars, m_dwDebugVarsNum, &var)) {
         // error
         pcVal->AttribSet (Text(), L"Error in variable name/expression.");
         return TRUE;
      }

   // get the text...
   var.Fracture(TRUE);
   DebugVarToName (&var, FALSE);
   PCMIFLVarString ps = var.GetString(this);
   pcVal->AttribSet (Text(), ps->Get());
   ps->Release();

   return TRUE;
}


/**********************************************************************************
CMIFLVM::DebugPage - This traps messages into a page to automatically handle
debug paramters. These inludes things like ESCM_SUBSTITUTION, ESCN_BUTTONPRESS, etc.

Call DebugPage() before the other processing for the page.

Note: pPage->m_pUserData is a pointer to a MIFLPAGE with information about the
current page. The m_pVM should be filled in.

inputs
   PCEscPage         pPage - Page
   DWORD             dwMessage - Message
   PVOID             pParam
   BOOL              fAllowScope - Set to TRUE if allow the user to change the
                     scope of the variable view's using the dropdown.
returns
   BOOL - TRUE if the message was handled and trapped, FALSE if the messages weren't
      specifically for the debug system.
*/
BOOL CMIFLVM::DebugPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam, BOOL fAllowScope)
{
   PMIFLPAGE pmp = (PMIFLPAGE)pPage->m_pUserData;

   switch (dwMessage) {

   case ESCM_INITPAGE:
      {
         // fill in all the variables slots
         DWORD i;
         for (i = 0; i < VARSSHOWN; i++)
            WatchVarShow (pPage, i, TRUE);

         // set scope
         PCEscControl pControl;
         pControl = pPage->ControlFind (L"dv:scope");
         if (pControl) {
            DWORD dwIndex = m_hObjects.FindIndex (&m_gDebugVarsObject);
            if (dwIndex == -1)
               dwIndex = 0;
            else
               dwIndex++;
            pControl->AttribSetInt (CurSel(), (int)dwIndex);
         }

         // set the debug display
         pControl = pPage->ControlFind (L"dbg:edit");
         if (pControl) {
            pControl->AttribSet (Text(), (PWSTR)m_memDebug.p);
            pControl->AttribSetInt (L"selstart", (int)m_memDebug.m_dwCurPosn / sizeof(WCHAR));
            pControl->AttribSetInt (L"selend", (int)m_memDebug.m_dwCurPosn / sizeof(WCHAR));
            pControl->Message (ESCM_EDITSCROLLCARET);
         }

      }
      break;

   case ESCN_COMBOBOXSELCHANGE:
      {
         PESCNCOMBOBOXSELCHANGE p = (PESCNCOMBOBOXSELCHANGE) pParam;
         PWSTR psz = p->pControl->m_pszName;
         if (!psz)
            break;

         if (!_wcsicmp(psz, L"dv:scope")) {
            DWORD dwVal = _wtoi(p->pszName);

            // get current object
            DWORD dwIndex = m_hObjects.FindIndex (&m_gDebugVarsObject);
            if (dwIndex == -1)
               dwIndex = 0;
            else
               dwIndex++;

            if (dwIndex == dwVal)
               return TRUE;   // no change

            // else, changed...
            if (dwVal) {
               PCMIFLVMObject *ppp = (PCMIFLVMObject*)m_hObjects.Get (dwVal-1);
               PCMIFLVMObject p = ppp ? *ppp : NULL;
               if (p) {
                  m_gDebugVarsObject = p->m_gID;
                  m_pDebugVarsObjectLayer = NULL;
               }
               else {
                  m_gDebugVarsObject = GUID_NULL;
                  m_pDebugVarsObjectLayer = NULL;
               }
            }
            else {
               m_gDebugVarsObject = GUID_NULL;
               m_pDebugVarsObjectLayer = NULL;
            }

            // fill in all the variables slots
            DWORD i;
            for (i = 0; i < VARSSHOWN; i++)
               WatchVarShow (pPage, i, TRUE);

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

         PWSTR pszVarVal = L"dvvar:";
         DWORD dwVarValLen = (DWORD)wcslen(pszVarVal);
         if (!wcsncmp(psz, pszVarVal, dwVarValLen)) {
            DWORD dwIndex = _wtoi(psz + dwVarValLen);

            // if not enough spaces in list then add
            WCHAR cBlank = 0;
            while (m_lDebugVars.Num() <= dwIndex)
               m_lDebugVars.Add (&cBlank, sizeof(cBlank));

            // get this string
            WCHAR szTemp[256];
            DWORD dwNeed;
            szTemp[0] = 0;
            p->pControl->AttribGet (Text(), szTemp, sizeof(szTemp), &dwNeed);
            m_lDebugVars.Set (dwIndex, szTemp, (wcslen(szTemp)+1)*sizeof(WCHAR));

            // set display
            WatchVarShow (pPage, dwIndex, FALSE);
            return TRUE;
         }
      }
      break;

   case ESCM_LINK:
      {
         PESCMLINK p = (PESCMLINK)pParam;
         PWSTR psz = p->psz;
         if (!psz)
            break;

         // handle click on specific object
         PWSTR pszObject = L"dpdl:o:";
         DWORD dwLenObject = (DWORD)wcslen(pszObject);
         if (!wcsncmp (psz, pszObject, dwLenObject)) {
            DWORD dwIndex = _wtoi(psz + dwLenObject);
            // BUGFIX - Extra test so dont crash if object list opened
            PCMIFLVMObject *pp = (PCMIFLVMObject*) m_hObjects.Get(dwIndex);
            PCMIFLVMObject p = pp ? *pp : NULL;

            if (p)
               m_gDebugListGUID = p->m_gID;
            pPage->Exit (MIFLRedoSamePage());
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"dpdl:objects")) {
            if (m_dwDebugListMajor == 1)
               m_dwDebugListMajor = 0;
            else
               m_dwDebugListMajor = 1;
            m_gDebugListGUID = GUID_NULL;
            pPage->Exit (MIFLRedoSamePage());
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"dpdl:globals")) {
            if (m_dwDebugListMajor == 2)
               m_dwDebugListMajor = 0;
            else
               m_dwDebugListMajor = 2;
            pPage->Exit (MIFLRedoSamePage());
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"dpdl:timers")) {
            if (m_dwDebugListMajor == 6)
               m_dwDebugListMajor = 0;
            else
               m_dwDebugListMajor = 6;
            pPage->Exit (MIFLRedoSamePage());
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"dpdl:functions")) {
            if (m_dwDebugListMajor == 3)
               m_dwDebugListMajor = 0;
            else
               m_dwDebugListMajor = 3;
            pPage->Exit (MIFLRedoSamePage());
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"dpdl:strings")) {
            if (m_dwDebugListMajor == 4)
               m_dwDebugListMajor = 0;
            else
               m_dwDebugListMajor = 4;
            pPage->Exit (MIFLRedoSamePage());
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"dpdl:resources")) {
            if (m_dwDebugListMajor == 5)
               m_dwDebugListMajor = 0;
            else
               m_dwDebugListMajor = 5;
            pPage->Exit (MIFLRedoSamePage());
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"dpdl:ometh")) {
            m_dwDebugListMinor = 1;
            pPage->Exit (MIFLRedoSamePage());
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"dpdl:otimers")) {
            m_dwDebugListMinor = 2;
            pPage->Exit (MIFLRedoSamePage());
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"dpdl:oprop")) {
            m_dwDebugListMinor = 0;
            pPage->Exit (MIFLRedoSamePage());
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"dbg:cleardebug")) {
            OutputDebugStringClear();
            pPage->Exit (MIFLRedoSamePage());
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"dbg:refreshdebug")) {
            pPage->Exit (MIFLRedoSamePage());
            return TRUE;
         }
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"DEBUGSTRING")) {
            MemZero (&m_memTemp);

            MemCat (&m_memTemp,
               L"<xtablecenter width=100% align=center>"
			      L"<xtrheader>Debug trace</xtrheader>"
			      L"<tr><td>"
				   L"<font face=courier><edit readonly=true multiline=true wordwrap=true width=85% height=40% vscroll=dbg:editscroll name=dbg:edit/></font>"
               L"<scrollbar orient=vert height=40% name=dbg:editscroll/>"
               L"<br/>"
               L"<button href=dbg:refreshdebug><bold>Refresh</bold>"
                  L"<xHoverHelp>"
                     L"Sometimes timers will be running in the background and updating the output. "
                     L"Press this to see the updated debug trace."
                  L"</xHoverHelp>"
               L"</button>"
               L"<button href=dbg:cleardebug><bold>Clear</bold></button>"
			      L"</td></tr>"
		         L"</xtablecenter>");

            p->pszSubString = (PWSTR)m_memTemp.p;
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"DEBUGVARS")) {
            DWORD i;
            MemZero (&m_memTemp);

            MemCat (&m_memTemp,
               L"<xtablecenter width=100% innerlines=0>"
               L"<xtrheader>Watch variables</xtrheader>");

            if (fAllowScope) {
               MemCat (&m_memTemp, 
                  L"<tr>"
                  L"<td width=33%><bold><a>Scope<xHoverHelp>"
                  L"Changing this affects from which object properties are retrieved."
                  L"</xHoverHelp></a></bold></td>"
                  L"<td width=66%>"
                  L"<bold><combobox width=100% cbheight=200 name=dv:scope>"
                  L"<elem name=0><italic>Not in object</italic></elem>");

               CMIFLVar var;
               for (i = 0; i < m_hObjects.Num(); i++) {
                  PCMIFLVMObject p = *((PCMIFLVMObject*) m_hObjects.Get(i));

                  var.SetObject (&p->m_gID);
                  DebugVarToName (&var, TRUE);

                  MemCat (&m_memTemp,
                     L"<elem name=");
                  MemCat (&m_memTemp, (int)i+1);
                  MemCat (&m_memTemp,
                     L">");
                  PCMIFLVarString ps = var.GetString(this);
                  MemCatSanitize (&m_memTemp, ps->Get());
                  ps->Release();
                  MemCat (&m_memTemp,
                     L"</elem>");
               } // i

               MemCat (&m_memTemp, L"</combobox></bold>"
                  L"</td>"
                  L"</tr>");
            }

            for (i = 0; i < VARSSHOWN; i++) {
               MemCat (&m_memTemp, L"<tr><td width=33%><bold><edit maxchars=128 width=100% name=dvvar:");
               MemCat (&m_memTemp, (int)i);
               MemCat (&m_memTemp, L"/></bold></td><td width=66%><edit readonly=true width=100% name=dvval:");
               MemCat (&m_memTemp, (int)i);
               MemCat (&m_memTemp, L"/></td></tr>");
		      } // i

            MemCat (&m_memTemp, L"</xtablecenter>");
            p->pszSubString = (PWSTR)m_memTemp.p;
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"DEBUGLIST")) {
            DWORD i, j;
            CMIFLVar var;

            MemZero (&m_memTemp);
            MemCat (&m_memTemp, L"<align tab=16>");

            MemCat (&m_memTemp,
               L"<a color=#ff00ff href=dpdl:objects>Objects..."
               L"<xHoverHelp>Click on this to see a list of existing objects.</xHoverHelp>"
               L"</a><br/>");
            if (m_dwDebugListMajor == 1) for (i = 0; i < m_hObjects.Num(); i++) {
               PCMIFLVMObject p = *((PCMIFLVMObject*) m_hObjects.Get(i));

               var.SetObject (&p->m_gID);
               DebugVarToName (&var, TRUE);

               MemCat (&m_memTemp,
                  L"&tab;<a color=#d000ff href=dpdl:o:");
               MemCat (&m_memTemp, (int)i);
               MemCat (&m_memTemp,
                  L">");
               PCMIFLVarString ps = var.GetString(this);
               MemCatSanitize (&m_memTemp, ps->Get());
               ps->Release();
               MemCat (&m_memTemp,
                  L"...<xHoverHelp>Click on this to learn more about the specific object.</xHoverHelp>"
                  L"</a><br/>");

               if (!IsEqualGUID (p->m_gID, m_gDebugListGUID))
                  continue;   // not selected

               MemCat (&m_memTemp,
                  L"&tab;&tab;<a color=#b000ff href=dpdl:oprop>Properties..."
                  L"<xHoverHelp>Click on this to see a list of properties supported by the object.</xHoverHelp>"
                  L"</a><br/>");
               if (m_dwDebugListMinor == 0) for (j = 0; j < p->m_hProp.Num(); j++) {
                  PCMIFLVarProp pProp = (PCMIFLVarProp)p->m_hProp.Get(j);

                  // get the name
                  DWORD dwID = pProp->m_dwID;
                  PWSTR pszName = NULL;
                  BOOL fPrivate = FALSE;
                  if (dwID >= VM_CUSTOMIDRANGE) {
                     // custom ID, so find
                     DWORD dwIndex = m_hUnIdentifiersCustomProperty.FindIndex (dwID);
                     pszName = m_hIdentifiersCustomProperty.GetString (dwIndex);
                  }
                  else {
                     PMIFLIDENT pmi = (PMIFLIDENT) m_pCompiled->m_hUnIdentifiers.Find (dwID);
                     if (pmi && (pmi->dwType == MIFLI_PROPDEF))
                        pszName = (PWSTR)((PCMIFLProp)pmi->pEntity)->m_memName.p;
                     else if (pmi && (pmi->dwType == MIFLI_PROPPRIV)) {
                        pszName = (PWSTR)((PCMIFLProp)pmi->pEntity)->m_memName.p;
                        fPrivate = TRUE;
                     }
                  }
                  if (!pszName)
                     pszName = L"Unknown property";;

                  // conver the global's value
                  var.Set (&pProp->m_Var);
                  var.Fracture (TRUE);  // so changing this wont affect global
                  DebugVarToName (&var, FALSE);

                  MemCat (&m_memTemp,
                     L"&tab;&tab;&tab;<a>");
                  if (fPrivate)
                     MemCat (&m_memTemp, L"<italic>");
                  MemCatSanitize (&m_memTemp, pszName);
                  if (fPrivate)
                     MemCat (&m_memTemp, L"</italic>");
                  MemCat (&m_memTemp, L"<xHoverHelp>");
                  PCMIFLVarString ps = var.GetString(this);
                  MemCatSanitize (&m_memTemp, ps->Get());
                  ps->Release();
                  MemCat (&m_memTemp, L"</xHoverHelp></a><br/>");

               } // j

               // show the sub-choices
               MemCat (&m_memTemp,
                  L"&tab;&tab;<a color=#b000ff href=dpdl:ometh>Methods..."
                  L"<xHoverHelp>Click on this to see a list of methods supported by the object.</xHoverHelp>"
                  L"</a><br/>");
               if (m_dwDebugListMinor == 1) for (j = 0; j < p->m_hMeth.Num(); j++) {
                  // get the name
                  PMIFLIDENT pmi = (PMIFLIDENT) p->m_hMeth.Get (j);
                  PWSTR pszName = NULL;
                  BOOL fPrivate = FALSE;
                  PCMIFLMeth pParam = NULL;
                  DWORD dwID = pmi ? pmi->dwID : 0;
                  if (pmi && (dwID >= VM_CUSTOMIDRANGE)) {
                     // custom ID, so find
                     DWORD dwIndex = m_hUnIdentifiersCustomMethod.FindIndex (dwID);
                     pszName = m_hIdentifiersCustomMethod.GetString (dwIndex);

                     // BUGFIX - Since this is a custom method, the name in
                     // ((PCMIFLFunc)pmi->pEntity)->m_Meth is the name of the
                     // original method. Since this is likely to be a public method
                     // search through public method info
                     PCMIFLFunc pFunc = (PCMIFLFunc)pmi->pEntity;
                     pParam = &pFunc->m_Meth;
                     if (pmi->dwType == MIFLI_METHDEF) {
                        PMIFLIDENT p2 = (PMIFLIDENT)m_pCompiled->m_hIdentifiers.Find ((PWSTR)pFunc->m_Meth.m_memName.p, FALSE);
                        if (p2 && (p2->dwType == pmi->dwType))
                           pParam = (PCMIFLMeth)p2->pEntity;   // so have all params right
                     }
                  }
                  else {
                     if (pmi && (pmi->dwType == MIFLI_METHDEF)) {
                        pszName = (PWSTR)((PCMIFLFunc)pmi->pEntity)->m_Meth.m_memName.p;

                        PMIFLIDENT p2 = (PMIFLIDENT)m_pCompiled->m_hUnIdentifiers.Find (dwID);
                        if (p2)
                           pParam = (PCMIFLMeth)p2->pEntity;   // so have all params right
                        else
                           pParam = &((PCMIFLFunc)pmi->pEntity)->m_Meth; // shouldnt happen
                     }
                     else if (pmi && (pmi->dwType == MIFLI_METHPRIV)) {
                        pParam = &((PCMIFLFunc)pmi->pEntity)->m_Meth;
                        pszName = (PWSTR)pParam->m_memName.p;
                        fPrivate = TRUE;
                     }
                  }
                  if (!pszName)
                     pszName = L"Unknown method";

                  MemCat (&m_memTemp,
                     L"&tab;&tab;&tab;<a>");
                  if (fPrivate)
                     MemCat (&m_memTemp, L"<italic>");
                  MemCatSanitize (&m_memTemp, pszName);
                  if (fPrivate)
                     MemCat (&m_memTemp, L"</italic>");
                  MemCat (&m_memTemp, L"<xHoverHelp>");
                  pParam->MemCatParam (&m_memTemp);
                  MemCat (&m_memTemp, L"</xHoverHelp></a><br/>");

               } // j

               // show the sub-choices
               MemCat (&m_memTemp,
                  L"&tab;&tab;<a color=#b000ff href=dpdl:otimers>Timers (suspended)..."
                  L"<xHoverHelp>Click on this to see a list of suspended timers in the object.</xHoverHelp>"
                  L"</a><br/>");
               PCMIFLVMTimer *ppt = (PCMIFLVMTimer*)p->m_lPCMIFLVMTimer.Get(0);
               if (m_dwDebugListMinor == 2) for (j = 0; j < p->m_lPCMIFLVMTimer.Num(); j++) {
                  PCMIFLVMTimer pp = ppt[j];

                  MemCat (&m_memTemp,
                     L"&tab;&tab;&tab;<a>");
                  PCMIFLVarString ps;

                  // name of the timer
                  var.Set (&pp->m_varName);
                  var.Fracture();   // so dont change
                  DebugVarToName (&var, FALSE);
                  ps = var.GetString(this);
                  MemCatSanitize (&m_memTemp, ps->Get());
                  ps->Release();

                  MemCat (&m_memTemp,L"<xHoverHelp>");

                  // does it repeat?
                  MemCat (&m_memTemp, L"Repeats: <bold>");
                  MemCat (&m_memTemp, pp->m_fRepeating ? L"true" : L"false");
                  MemCat (&m_memTemp, L"</bold><br/>");

                  // interval
                  WCHAR szTemp[32];
                  swprintf (szTemp, L"%g", (double)pp->m_fTimeRepeat);
                  MemCat (&m_memTemp, L"Interval: <bold>");
                  MemCat (&m_memTemp, szTemp);
                  MemCat (&m_memTemp, L"</bold> sec.<br/>");

                  // function called
                  if (IsEqualGUID(pp->m_gCall, GUID_NULL))
                     var.SetFunc (pp->m_dwCallID);
                  else
                     var.SetObjectMeth (&pp->m_gCall, pp->m_dwCallID);
                  DebugVarToName (&var, FALSE);
                  ps = var.GetString(this);
                  MemCat (&m_memTemp, L"Call: <bold>");
                  MemCatSanitize (&m_memTemp, ps->Get());
                  MemCat (&m_memTemp, L"</bold><br/>");
                  ps->Release();

                  // parameters
                  var.Set (&pp->m_varParams);
                  var.Fracture();   // so dont change
                  DebugVarToName (&var, FALSE);
                  ps = var.GetString(this);
                  MemCat (&m_memTemp, L"Parameters: <bold>");
                  MemCatSanitize (&m_memTemp, ps->Get());
                  MemCat (&m_memTemp, L"</bold><br/>");
                  ps->Release();

                  // all done
                  MemCat (&m_memTemp, L"</xHoverHelp></a><br/>");
               } // listing objects, j



               // cointains
               MemCat (&m_memTemp,
                  L"&tab;&tab;<a>Contains"
                  L"<xHoverHelp>");

               MemCat (&m_memTemp, L"<bold>Contained by:</bold> ");
               PCMIFLVMObject *ppp;
               PCMIFLVMObject pvo;
               ppp = (PCMIFLVMObject*) m_hObjects.Find (&p->m_gContainedIn);
               pvo = ppp ? ppp[0] : NULL;
               if (pvo) {
                  var.SetObject (&pvo->m_gID);
                  DebugVarToName (&var, FALSE);
                  PCMIFLVarString ps = var.GetString(this);
                  MemCatSanitize (&m_memTemp, ps->Get());
                  ps->Release();
               }
               else
                  MemCat (&m_memTemp, L"Not contained");
               MemCat (&m_memTemp, L"<br/>");

               MemCat (&m_memTemp, L"<bold>Contains:</bold> ");
               GUID *pgCont = (GUID*)p->m_lContains.Get(0);
               for (j = 0; j < p->m_lContains.Num(); j++, pgCont++) {
                  ppp = (PCMIFLVMObject*) m_hObjects.Find (pgCont);
                  pvo = ppp ? ppp[0] : NULL;
                  if (!pvo)
                     continue;

                  if (j)
                     MemCat (&m_memTemp, L", ");

                  var.SetObject (&pvo->m_gID);
                  DebugVarToName (&var, FALSE);
                  PCMIFLVarString ps = var.GetString(this);
                  MemCatSanitize (&m_memTemp, ps->Get());
                  ps->Release();
               } // j
               if (!j)
                  MemCat (&m_memTemp, L"Nothing");

               MemCat (&m_memTemp,
                  L"</xHoverHelp>"
                  L"</a><br/>");


               // layers
               MemCat (&m_memTemp,
                  L"&tab;&tab;<a>Layers"
                  L"<xHoverHelp>");
               for (j = 0; j < p->LayerNum(); j++) {
                  PCMIFLVMLayer pl = p->LayerGet (j);
                  MemCat (&m_memTemp, L"<bold>");
                  MemCatSanitize (&m_memTemp, (PWSTR)pl->m_memName.p);
                  MemCat (&m_memTemp, L"</bold>: (");

                  WCHAR szTemp[32];
                  swprintf (szTemp, L"%g", (double)pl->m_fRank);
                  MemCat (&m_memTemp, szTemp);

                  MemCat (&m_memTemp, L") Based on ");

                  MemCatSanitize (&m_memTemp, (PWSTR)pl->m_pObject->m_memName.p);

                  MemCat (&m_memTemp, L"<br/>");
               } // j
               MemCat (&m_memTemp,
                  L"</xHoverHelp>"
                  L"</a><br/>");
            } // listing objects

            MemCat (&m_memTemp,
               L"<a color=#ff00ff href=dpdl:globals>Global variables..."
               L"<xHoverHelp>Click on this to see a list of global variables.</xHoverHelp>"
               L"</a><br/>");
            if (m_dwDebugListMajor == 2) for (i = 0; i < m_hGlobals.Num(); i++) {
               PCMIFLVarProp p = (PCMIFLVarProp) m_hGlobals.Get(i);

               DWORD dwID = p->m_dwID;
               PWSTR pszName = NULL;
               BOOL fPrivate = FALSE;
               if (dwID >= VM_CUSTOMIDRANGE) {
                  // custom ID, so find
                  DWORD dwIndex = m_hUnIdentifiersCustomGlobal.FindIndex (dwID);
                  pszName = m_hIdentifiersCustomGlobal.GetString (dwIndex);
               }
               else {
                  // get the name
                  PMIFLIDENT pmi = (PMIFLIDENT) m_pCompiled->m_hGlobals.Find (dwID);
                  if (pmi && (pmi->dwType == MIFLI_GLOBAL))
                     pszName = (PWSTR)((PCMIFLProp)pmi->pEntity)->m_memName.p;
                  else if (pmi && (pmi->dwType == MIFLI_OBJECT))
                     pszName = (PWSTR)((PCMIFLObject)pmi->pEntity)->m_memName.p;
               }
               if (!pszName)
                  pszName =L"Unknown global";;

               // conver the global's value
               var.Set (&p->m_Var);
               var.Fracture (TRUE);  // so changing this wont affect global
               DebugVarToName (&var, FALSE);

               MemCat (&m_memTemp,
                  L"&tab;<a>");
               MemCatSanitize (&m_memTemp, pszName);
               MemCat (&m_memTemp,L"<xHoverHelp>");
               PCMIFLVarString ps = var.GetString(this);
               MemCatSanitize (&m_memTemp, ps->Get());
               ps->Release();
               MemCat (&m_memTemp, L"</xHoverHelp></a><br/>");
            } // listing objects

            MemCat (&m_memTemp,
               L"<a color=#ff00ff href=dpdl:functions>Functions..."
               L"<xHoverHelp>Click on this to see a list of functions.</xHoverHelp>"
               L"</a><br/>");
            if (m_dwDebugListMajor == 3) for (i = 0; i < m_pCompiled->m_hIdentifiers.Num(); i++) {
               PMIFLIDENT pmi = (PMIFLIDENT) m_pCompiled->m_hIdentifiers.Get (i);
               if (pmi->dwType != MIFLI_FUNC)
                  continue;   // only care about functions

               PCMIFLFunc pFunc = (PCMIFLFunc)pmi->pEntity;

               MemCat (&m_memTemp,
                  L"&tab;<a>");
               MemCatSanitize (&m_memTemp, (PWSTR)pFunc->m_Meth.m_memName.p);
               MemCat (&m_memTemp,L"<xHoverHelp>");
               pFunc->m_Meth.MemCatParam (&m_memTemp);
               MemCat (&m_memTemp, L"</xHoverHelp></a><br/>");
            } // listing functions

            MemCat (&m_memTemp,
               L"<a color=#ff00ff href=dpdl:strings>Strings..."
               L"<xHoverHelp>Click on this to see a list of strings.</xHoverHelp>"
               L"</a><br/>");
            if (m_dwDebugListMajor == 4) for (i = 0; i < m_pCompiled->m_hIdentifiers.Num(); i++) {
               PMIFLIDENT pmi = (PMIFLIDENT) m_pCompiled->m_hIdentifiers.Get (i);
               if (pmi->dwType != MIFLI_STRING)
                  continue;   // only care about functions

               PCMIFLString pString = (PCMIFLString)pmi->pEntity;

               MemCat (&m_memTemp,
                  L"&tab;<a>");
               MemCatSanitize (&m_memTemp, (PWSTR)pString->m_memName.p);
               MemCat (&m_memTemp,L"<xHoverHelp>");
               PWSTR psz = pString->Get (m_LangID);
               if (psz)
                  MemCatSanitize (&m_memTemp, (PWSTR)pString->m_lString.Get(0));
               MemCat (&m_memTemp, L"</xHoverHelp></a><br/>");
            } // strings

            MemCat (&m_memTemp,
               L"<a color=#ff00ff href=dpdl:resources>Resources..."
               L"<xHoverHelp>Click on this to see a list of resources.</xHoverHelp>"
               L"</a><br/>");
            if (m_dwDebugListMajor == 5) for (i = 0; i < m_pCompiled->m_hIdentifiers.Num(); i++) {
               PMIFLIDENT pmi = (PMIFLIDENT) m_pCompiled->m_hIdentifiers.Get (i);
               if (pmi->dwType != MIFLI_RESOURCE)
                  continue;   // only care about functions

               PCMIFLResource pResource = (PCMIFLResource)pmi->pEntity;

               MemCat (&m_memTemp,
                  L"&tab;");
               MemCatSanitize (&m_memTemp, (PWSTR)pResource->m_memName.p);
               MemCat (&m_memTemp, L"<br/>");
            } // resources

            MemCat (&m_memTemp,
               L"<a color=#ff00ff href=dpdl:timers>Timers (active)..."
               L"<xHoverHelp>Click on this to see a list of active timers.</xHoverHelp>"
               L"</a><br/>");
            PCMIFLVMTimer *ppt = (PCMIFLVMTimer*)m_lPCMIFLVMTimer.Get(0);
            if (m_dwDebugListMajor == 6) for (i = 0; i < m_lPCMIFLVMTimer.Num(); i++) {
               PCMIFLVMTimer pp = ppt[i];

               MemCat (&m_memTemp,
                  L"&tab;<a>");
               PCMIFLVarString ps;

               // name of the object...
               var.SetObject (&pp->m_gBelongsTo);
               DebugVarToName (&var, FALSE);
               ps = var.GetString(this);
               MemCatSanitize (&m_memTemp, ps->Get());
               ps->Release();

               // use a colon
               MemCat (&m_memTemp, L":");

               // name of the timer
               var.Set (&pp->m_varName);
               var.Fracture();   // so dont change
               DebugVarToName (&var, FALSE);
               ps = var.GetString(this);
               MemCatSanitize (&m_memTemp, ps->Get());
               ps->Release();

               MemCat (&m_memTemp,L"<xHoverHelp>");

               // does it repeat?
               MemCat (&m_memTemp, L"Repeats: <bold>");
               MemCat (&m_memTemp, pp->m_fRepeating ? L"true" : L"false");
               MemCat (&m_memTemp, L"</bold><br/>");

               // interval
               WCHAR szTemp[32];
               swprintf (szTemp, L"%g", (double)pp->m_fTimeRepeat);
               MemCat (&m_memTemp, L"Interval: <bold>");
               MemCat (&m_memTemp, szTemp);
               MemCat (&m_memTemp, L"</bold> sec.<br/>");

               // function called
               if (IsEqualGUID(pp->m_gCall, GUID_NULL))
                  var.SetFunc (pp->m_dwCallID);
               else
                  var.SetObjectMeth (&pp->m_gCall, pp->m_dwCallID);
               DebugVarToName (&var, FALSE);
               ps = var.GetString(this);
               MemCat (&m_memTemp, L"Call: <bold>");
               MemCatSanitize (&m_memTemp, ps->Get());
               MemCat (&m_memTemp, L"</bold><br/>");
               ps->Release();

               // parameters
               var.Set (&pp->m_varParams);
               var.Fracture();   // so dont change
               DebugVarToName (&var, FALSE);
               ps = var.GetString(this);
               MemCat (&m_memTemp, L"Parameters: <bold>");
               MemCatSanitize (&m_memTemp, ps->Get());
               MemCat (&m_memTemp, L"</bold><br/>");
               ps->Release();

               // all done
               MemCat (&m_memTemp, L"</xHoverHelp></a><br/>");
            } // listing objects


            MemCat (&m_memTemp, L"</align>");
            p->pszSubString = (PWSTR)m_memTemp.p;
            return TRUE;
         }
      }
      break;
   }

   // default, not handles
   return FALSE;
}



#if 0 // dead code
/*****************************************************************************
CMIFLVM::VarAccess - This internal function takes compiled code (from a variable)
and accesses it, filling in the value of the variable. This is used by
the debugger to see different values.

inputs
   PCMem          pCode - Memory containing the MIFLCOMP code
   DWORD          dwIndex - Index to use
   PCMIFLVMObject pObject - Object that is in, so can access private variables. NULL if not in object
   PCMIFLVar      paVars - Storing the values of the variables within a function/method. NULL if not in function
                  The index meaning has been compiled into pCode. Array of CMIFLVar
   DWORD          dwVarsNum - Number of variables
   PCMIFLVar      pVar - Should be initialized to something, but will be modified by
                  this function and filled in

returns
   BOOL - TRUE if success, FALSE if error
*/
BOOL CMIFLVM::VarAccess (PCMem pCode, DWORD dwIndex, PCMIFLVMObject pObject,
                         PCMIFLVar paVars, DWORD dwVarsNum, PCMIFLVar pVar)
{
   PMIFLCOMP pc = (PMIFLCOMP)((PBYTE)pCode->p + dwIndex);

   if (pc->dwNext)
      return FALSE;  // unexpected token

   switch (pc->dwType) {
   case TOKEN_NOP:
      pVar->SetUndefined();
      return TRUE;

   case TOKEN_DOUBLE:
      {
         double *pf = (double*)((PBYTE)pCode->p + pc->dwValue);
         pVar->SetDouble (*pf);
      }
      return TRUE;

   case TOKEN_CHAR:
      pVar->SetChar ((WCHAR)pc->dwValue);
      return TRUE;

   case TOKEN_STRING:
      {
         PWSTR psz = (PWSTR)((PBYTE)pCode->p + pc->dwValue);
         pVar->SetString (psz);
      }
      return TRUE;

   case TOKEN_NULL:
      pVar->SetNULL ();
      return TRUE;

   case TOKEN_UNDEFINED:
      pVar->SetUndefined ();
      return TRUE;

   case TOKEN_BOOL:
      pVar->SetBOOL (pc->dwValue);
      return TRUE;

   case TOKEN_STRINGTABLE:
      pVar->SetStringTable (pc->dwValue);
      return TRUE;

   case TOKEN_RESOURCE:
      pVar->SetResource (pc->dwValue);
      return TRUE;

   case TOKEN_METHPRIV:
      pVar->SetMeth (pc->dwValue);
      return TRUE;

   case TOKEN_METHPUB:
      pVar->SetMeth (pc->dwValue);
      return TRUE;

   case TOKEN_PROPPUB:
   case TOKEN_PROPPRIV:
      {
         if (!pObject)
            return FALSE;  // not in an object

         // see if can find it
         PCMIFLVarProp pv = (PCMIFLVarProp)pObject->m_hProp.Find (pc->dwValue);
         if (pv)
            pVar->Set (&pv->m_Var);
         else
            pVar->SetUndefined();
      }
      return TRUE;

   case TOKEN_FUNCTION:
      pVar->SetFunc (pc->dwValue);
      return TRUE;

   case TOKEN_GLOBAL:
      {
         PCMIFLVarProp pv = (PCMIFLVarProp)m_hGlobals.Find (pc->dwValue);
         if (pv)
            pVar->Set (&pv->m_Var);
         else
            pVar->SetUndefined();
      }
      return TRUE;

   case TOKEN_VARIABLE:
      {
         if (!plVars)
            return FALSE;

         PCMIFLVar pv = (PCMIFLVar)plVars->Get(pc->dwValue);
         if (!pv)
            return FALSE;  // error, shouldnt happen

         pVar->Set (pv);
      }
      return TRUE;


   case TOKEN_LISTINDEX:
      {
         PMIFLCOMP pcArg1 = pc->dwDown ? (PMIFLCOMP)((PBYTE)pCode->p + pc->dwDown) : NULL;
         PMIFLCOMP pcArg2 = (pcArg1 && pcArg1->dwDown) ? (PMIFLCOMP)((PBYTE)pCode->p + pcArg1->dwDown) : NULL;
         if (!pcArg1 || !pcArg2 || !pcArg1->dwNext || !pcArg2->dwNext)
            return FALSE;  // shouldnt happen

         // evaluate the list...
         CMIFLVar vList, vIndex;
         if (!VarAccess (pCode, pcArg1->dwNext, pObject, plVars, &vList))
            return FALSE;

         // evaluate the index
         if (!VarAccess (pCode, pcArg2->dwNext, pObject, plVars, &vIndex))
            return FALSE;
         DWORD dwIndex = (DWORD) vIndex.GetDouble(m_pCompiled);

         // DOCUMENT: Can access array into string or list
         switch (vList.TypeGet()) {
         case MV_LIST:
            {
               // DOCUMENT: If access beyond end of list then get undefined
               PCMIFLVarList pl = vList.GetList(m_pCompiled);
               PCMIFLVar pFind = pl->Get(dwIndex);
               if (pFind)
                  pVar->Set (pFind);
               else
                  pVar->SetUndefined();
               pl->Release();
            }
            break;

         case MV_STRING:
            {
               // DOCUMENT: If access beyond end of string then get undefined
               PCMIFLVarString ps = vList.GetString(m_pCompiled);
               PWSTR pszFind = ps->CharGet(dwIndex);
               if (pszFind)
                  pVar->SetChar (pszFind[0]);
               else
                  pVar->SetUndefined();
               ps->Release();
            }
            break;
         default:
            // NOTE - if can ever array into object then will need to change this
            return FALSE;
         }
      }
      return TRUE;

   case TOKEN_OPER_DOT:
      {
         PMIFLCOMP pcArg1 = pc->dwDown ? (PMIFLCOMP)((PBYTE)pCode->p + pc->dwDown) : NULL;
         PMIFLCOMP pcArg2 = (pcArg1 && pcArg1->dwDown) ? (PMIFLCOMP)((PBYTE)pCode->p + pcArg1->dwDown) : NULL;
         if (!pcArg1 || !pcArg2 || !pcArg1->dwNext || !pcArg2->dwNext)
            return FALSE;  // shouldnt happen

         // evaluate the list...
         CMIFLVar vObj, vIndex;
         if (!VarAccess (pCode, pcArg1->dwNext, pObject, plVars, &vObj))
            return FALSE;

         // if it's not an object then fail
         if (vObj.TypeGet() != MV_OBJECT)
            return FALSE;
         // NOTE - At some point may want function calls for lists and strings
         // like string.LengthOf()

         // get the object
         GUID gObj = vObj.GetGUID (m_pCompiled);
         PCMIFLVMObject pNewObject = ObjectFind (&gObj);

         PMIFLCOMP pcArg2n = (PMIFLCOMP)((PBYTE)pCode->p + pcArg2->dwNext);
         if ((pcArg2n->dwType == TOKEN_PROPPUB) || (pcArg2n->dwType == TOKEN_PROPPRIV)) {
            // to the right of the token is a private/public method, so get it...
            return VarAccess (pCode, pcArg2->dwNext, pNewObject, plVars, pVar);
         }

         // otherwise, might be a method, so just access it...
         // NOTE: Specifically using existing object space when accessing, NOT pNewObject
         if (!VarAccess (pCode, pcArg2->dwNext, pObject, plVars, &vIndex))
            return FALSE;

         switch (vIndex.TypeGet()) {
         case MV_METH:
            pVar->SetObjectMeth (&gObj, vIndex.GetValue(m_pCompiled));
            return TRUE;
         default:
            // improper access
            return FALSE;
         }
      }
      return TRUE;

   case TOKEN_CLASS:
   case TOKEN_FUNCCALL:
   case TOKEN_LISTDEF:
   default:
      // not acceptable
      return FALSE;

   }

   // default
   pVar->SetUndefined();
}
#endif // 0 - dead code


/*****************************************************************************
CMIFLVM::VarAccess - Access a variable (in the current state), based on text.

inputs

inputs
   PWSTR          pszCode - Code to use, such as "x[25]". Assumes that no semi-colon at end
   PCMIFLVMObject pObject - Object that is in, so can access private variables. NULL if not in object
   PCMIFLObject   pObjectLayer - Where the code is coming from - basically the layer. If this
                  is NULL then it's automatically calculated
   PCHashString   phVars - Goes from variable name to an index into the local variables.
                  Use NULL if not a function. Element size is 0
   PCMIFLVar      paVars - Storing the values of the variables within a function/method. NULL if not in function
                  The index meaning has been compiled into pCode. Array of CMIFLVar
   DWORD          dwVarsNum - Number of variables
   PCMIFLVar      pVar - Should be initialized to something, but will be modified by
                  this function and filled in

returns
   BOOL - TRUE if success, FALSE if error
*/
BOOL CMIFLVM::VarAccess (PWSTR pszCode, PCMIFLVMObject pObject, PCMIFLObject pObjectLayer, PCHashString phVars,
                         PCMIFLVar paVars, DWORD dwVarsNum, PCMIFLVar pVar)
{
   CMIFLErrors err;
   CMem memCodeComp;

   // calculate the layer
   // NOTE: This is a hack, but the best that can do given that we really
   // dont know which layer the code is coming from
   if (!pObjectLayer && pObject) {
      //if (pObject->m_pObject)
      //   pObjectLayer = pObject->m_pObject;
      //else {
         // get the last layer
         PCMIFLVMLayer pLayer = pObject->LayerGet(pObject->LayerNum()-1);
         if (pLayer)
            pObjectLayer = pLayer->m_pObject;
      //}
   }

   m_pCompiled->CompileCode (&err, NULL, phVars, FALSE, pszCode, TRUE,
      &memCodeComp, pObjectLayer, L"err", L"err");

   if (err.m_dwNumError || !memCodeComp.m_dwCurPosn)
      return FALSE;  // error in compile

   MIFLFCI mci;
   
   memset (&mci, 0, sizeof(mci));
   mci.pszCode = pszCode;
   mci.pMemMIFLCOMP = &memCodeComp;
   mci.phVarsString = phVars;
   mci.pObject = pObject;
   mci.pObjectLayer = pObjectLayer;
   mci.pszWhere1 = L"varaccess";
   mci.paVars = paVars;
   mci.dwVarsNum = dwVarsNum;
   mci.pbMIFLCOMP = (PBYTE)mci.pMemMIFLCOMP->p;
   mci.fOnlyVarAccess = TRUE;
   mci.dwPropIDGetSet = -1;


   // swap out old one
   PMIFLFCI pOld = m_pFCICur;
   m_pFCICur = &mci;

   CMIFLVarLValue vVarLValue;
   DWORD dwRet = ExecuteCodeStatement (0, &vVarLValue);
   pVar->Set (&vVarLValue.m_Var);

   m_pFCICur = pOld; // restore

   return (dwRet ? FALSE : TRUE);
   // return VarAccess (&memCodeComp, 0, pObject, plVars, pVar);
}

/*****************************************************************************
CMIFLVM::OutputDebugStringClears - Clears the debug strings
*/
void CMIFLVM::OutputDebugStringClear (void)
{
   MemZero (&m_memDebug);
   m_memDebug.m_dwCurPosn = 0;
}



/*****************************************************************************
CMIFLVM::OutputDebugString - Writes a debug string to the display, m_memDebug,
and also to the log.

inputs
   PWSTR          psz - String to write out
returns
   none
*/
void CMIFLVM::OutputDebugString (PWSTR psz)
{
   DWORD dwLen = ((DWORD)wcslen(psz)+1)*sizeof(WCHAR);

   // allocate enough memory
   if (dwLen + m_memDebug.m_dwCurPosn > m_memDebug.m_dwAllocated) {
      if (!m_memDebug.Required (dwLen + m_memDebug.m_dwCurPosn))
         return;  // error
   }

   // copy over
   memcpy ((PBYTE)m_memDebug.p + m_memDebug.m_dwCurPosn, psz, dwLen);
   m_memDebug.m_dwCurPosn += dwLen - sizeof(WCHAR);

   // pass on to socket to log
   m_pSocket->Log(this, psz);

   // don't let debug get too large
   if (m_memDebug.m_dwCurPosn < MAXDEBUGSIZE)
      return;  // no change
   DWORD dwDel = ((DWORD)m_memDebug.m_dwCurPosn - MAXDEBUGSIZE + MAXDEBUGSIZE/10);
   dwDel = (dwDel / sizeof(WCHAR)) * sizeof(WCHAR);

   memmove ((PBYTE)m_memDebug.p, (PBYTE)m_memDebug.p + dwDel, m_memDebug.m_dwCurPosn + sizeof(WCHAR) - dwDel);
   m_memDebug.m_dwCurPosn -= dwDel;
}

/*****************************************************************************
CMIFLVM::OutputDebugString - Writes a debug string to the display, m_memDebug,
and also to the log.

inputs
   PCMIFLVar         pVar - Variable to output
returns
   none
*/
void CMIFLVM::OutputDebugString (PCMIFLVar pVar)
{
   CMIFLVar v;
   v.Set (pVar);
   v.Fracture(TRUE);

   // BUGFIX - If it's a complete string then don't wrap it up
   if (v.TypeGet() != MV_STRING)
      DebugVarToName (&v, FALSE);

   PCMIFLVarString ps = v.GetString(this);
   OutputDebugString (ps->Get());
   ps->Release();
}
 



/*****************************************************************************
CMIFLVM::RunTimeErr - Report a runtime error.

Note: Requires that m_pFCICur is valid

inputs
   PWSTR          pszErr - Error to report
   BOOL           fError - TRUE if it's an error, FALSE if warning
   DWORD          dwCharStart - Starting character in the code where error occurs
   DWORD          dwCharEnd - Ending chararcter in the code where error occurs

   The following information is used from m_pFCICur:
   PWSTR          pszWhere1 - String indicating where the error is to be found.
   PWSTR          pszWhere2 -  Second string for error loc. Might be NULL
   PWSTR          pszCode - Code that's happening in.
   PCHashString   phVarsString - String reference names for debug display. 0-sized elems.
                     NULL if not occuring in function call
   PCListFixed    plVarsCMIFLVar - Local variable info
                     NULL if not occuring in function call
   PCMIFLVMObject pObject - Object that it's happening in. NULL if not in object
   PCMIFLObject   pObjectLayer - Specific object layer it's happening in. If this is
                  NULL the layer is guessed
returns
   DWORD - One of MFC_XXX
*/
DWORD CMIFLVM::RunTimeErr (PWSTR pszErr, BOOL fError, DWORD dwCharStart, DWORD dwCharEnd)
{
   // if this is only testing for var access then don't display runtime errors
   if (m_pFCICur->fOnlyVarAccess)
      return MFC_REPORTERROR;

   // find the line number where this happens...
   DWORD dwLine = 1;
   DWORD dwLineStart = 0;
   DWORD dwFind = dwCharStart;
   DWORD dwChar = 0;
   PWSTR pCur;
   if (m_pFCICur) for (pCur = m_pFCICur->pszCode; pCur[0] && dwFind; pCur++, dwChar++, dwFind--) {
      if (pCur[0] == L'\n') {
         dwLine++;
         dwLineStart = dwChar+1;
      }
      else if (pCur[0] == L'\r')
         dwLineStart = dwChar+1;
   }
   dwLineStart = min (dwLineStart, dwCharStart);   // just in case
   if (!pCur[0]) {
      // came to end of file
      dwLineStart = min(dwLineStart, dwChar);
      dwCharStart = min(dwCharStart, dwChar);
      dwCharEnd = min(dwCharEnd, dwChar);
   }

   // output this
   WCHAR szTemp[64];
   OutputDebugString (fError ? L"\r\nERROR: " : L"\r\nWarning: ");
   if (m_pFCICur) {
      if (m_pFCICur->pszWhere1)
         OutputDebugString (m_pFCICur->pszWhere1);
      if (m_pFCICur->pszWhere2) {
         OutputDebugString (L".");
         OutputDebugString (m_pFCICur->pszWhere2);
      }
   }
   swprintf (szTemp, L" (line %d): ", (int)dwLine);
   OutputDebugString (szTemp);
   if (pszErr)
      OutputDebugString (pszErr);

   // print the code
   CMem mem;
   if (mem.Required ((dwCharEnd - dwLineStart + 200)*sizeof(WCHAR))) {
      PWSTR psz = (PWSTR)mem.p;

      DWORD dwLen = (dwCharStart - dwLineStart)*sizeof(WCHAR);
      memcpy (psz, m_pFCICur->pszCode + dwLineStart, dwLen);
      psz += (dwLen / sizeof(WCHAR));

      // indicate problem
      wcscpy (psz, L">>>");
      psz += wcslen(psz);

      // code that causes problem
      dwLen = (dwCharEnd - dwCharStart)*sizeof(WCHAR);
      memcpy (psz, m_pFCICur->pszCode + dwCharStart, dwLen);
      psz += (dwLen / sizeof(WCHAR));

      // deindicator
      wcscpy (psz, L"<<<");
      psz += wcslen(psz);

      // go until find end of line...
      for (dwLen = 0; ; dwLen++) {
         WCHAR c = m_pFCICur->pszCode[dwLen + dwCharEnd];
         if (!c || (c == L'\r') || (c == L'\n') || (dwLen > 150))
            break;
      }
      memcpy (psz, m_pFCICur->pszCode + dwCharEnd, dwLen*sizeof(WCHAR));
      psz += dwLen;

      // null terminte
      psz[0] = 0;

      OutputDebugString (L"\r\n");
      OutputDebugString (L"Code: ");
      OutputDebugString ((PWSTR)mem.p);
   }

   OutputDebugString (L"\r\n");

   // if debug mode is set to ignore all errors then skip
   if (m_dwDebugMode == MDM_IGNOREALL)
      return MFC_NONE;

   return DebugUI (dwCharStart, dwCharEnd, pszErr);
}



/*****************************************************************************
CMIFLVM::GlobalGet - Call this to get the value of a global variable. It's
the safest way to call since it also handles the get/set variable code.

DOCUMENT: If overrite global get/set then will go through code when access the
variable, unless it's called from within the global get/set code

inputs
   DWORD          dwID - ID of the global
   BOOL           fIgnoreGetSet - If set the ignore the get/set calls.
   PCMIFLVar      pVar - Will be filled in with the global variable, including
                  the l-value info
returns
   DWORD - MFC_XXX
*/
DWORD CMIFLVM::GlobalGet (DWORD dwID, BOOL fIgnoreGetSet, PCMIFLVarLValue pVar)
{
   // clear the LValue
   pVar->Clear();

   PCMIFLVarProp pv = (PCMIFLVarProp)m_hGlobals.Find (dwID);
   if (!pv) {
      pVar->m_Var.SetUndefined();
      return MFC_NONE;
   }

   if (pv->m_pCodeGet && !fIgnoreGetSet) {
      // if this global has get/set code associated with it, and we're not
      // already in the get/set code, then run it
      MIFLFCI fci;
      PCMIFLVarList plParam = new CMIFLVarList;
      if (!plParam)
         return MFC_NONE;   // error
      fci.pszCode = (PWSTR)pv->m_pCodeGet->m_memCode.p;
      fci.pMemMIFLCOMP = &pv->m_pCodeGet->m_memMIFLCOMP;
      fci.dwParamCount = pv->m_pCodeGet->m_dwParamCount;
      fci.plParam = plParam;
      fci.phVarsString = &pv->m_pCodeGet->m_hVars;
      fci.pObject = NULL;
      fci.pObjectLayer = NULL;
      fci.pszWhere1 = pv->m_pCodeGet->m_pszCodeName;
      fci.pszWhere2 = NULL;
      fci.dwPropIDGetSet = dwID;

      DWORD dwRet = FuncCallInternal (&fci, pVar);

      // release parameters
      plParam->Release();

      // remember where came from
      pVar->m_dwLValue = MLV_GLOBAL;
      pVar->m_dwLValueID = dwID;

      return dwRet;
   }

   // else, no override, so just get
   pVar->m_Var.Set (&pv->m_Var);
   pVar->m_dwLValue = MLV_GLOBAL;
   pVar->m_dwLValueID = dwID;


   return MFC_NONE;
}




/*****************************************************************************
CMIFLVM::GlobalSet - Call this to set the value of a global variable. It's
the safest way to call since it also handles the get/set variable code.

DOCUMENT: If set global that doesn't exist then create it

inputs
   DWORD          dwID - ID of the global
   BOOL           fIgnoreGetSet - If set the ignore the get/set calls.
   PCMIFLVar      pVar - Variable set it as
returns
   DWORD - MFC_XXX
*/
DWORD CMIFLVM::GlobalSet (DWORD dwID, BOOL fIgnoreGetSet, PCMIFLVar pVar)
{
   PCMIFLVarProp pv = (PCMIFLVarProp)m_hGlobals.Find (dwID);
   
   if (!pv) {
      // doesn't exist so add it, start with undefined so less chance of bug
      CMIFLVarProp vp;
      vp.m_dwID = dwID;
      m_hGlobals.Add (dwID, &vp);

      pv = (PCMIFLVarProp)m_hGlobals.Find (dwID);
      if (!pv)
         return MFC_NONE;
   }

   if (pv->m_pCodeSet && !fIgnoreGetSet) {
      // if this global has get/set code associated with it, and we're not
      // already in the get/set code, then run it
      MIFLFCI fci;
      PCMIFLVarList plParam = new CMIFLVarList;
      if (!plParam)
         return MFC_NONE;   // error
      plParam->Add (pVar, FALSE);

      fci.pszCode = (PWSTR)pv->m_pCodeSet->m_memCode.p;
      fci.pMemMIFLCOMP = &pv->m_pCodeSet->m_memMIFLCOMP;
      fci.dwParamCount = pv->m_pCodeSet->m_dwParamCount;
      fci.plParam = plParam;
      fci.phVarsString = &pv->m_pCodeSet->m_hVars;
      fci.pObject = NULL;
      fci.pObjectLayer = NULL;
      fci.pszWhere1 = pv->m_pCodeSet->m_pszCodeName;
      fci.pszWhere2 = NULL;
      fci.dwPropIDGetSet = dwID;

      CMIFLVarLValue v;
      DWORD dwRet = FuncCallInternal (&fci, &v);   // use &v since don't want to affect what had set

      // release parameters
      plParam->Release();

      return dwRet;
   }

   // else set it directly
   pv->m_Var.Set (pVar);

   return MFC_NONE;
}


/*****************************************************************************
CMIFLVM::GlobalRemove - Call this to remove a global.

inputs
   DWORD          dwID - ID of the global
returns
   BOOL - TRUE if success
*/
BOOL CMIFLVM::GlobalRemove (DWORD dwID)
{
   DWORD dwIndex = m_hGlobals.FindIndex (dwID);
   if (dwIndex == -1)
      return FALSE;

   PCMIFLVarProp pv = (PCMIFLVarProp)m_hGlobals.Get (dwIndex);
   if (!pv)
      return FALSE;

   pv->m_Var.SetUndefined();
   m_hGlobals.Remove (dwIndex);
   return TRUE;
   
}




/*****************************************************************************
CMIFLVM::PropertyGet - Call this to get the value of a Property. It's
the safest way to call since it also handles the get/set variable code.

DOCUMENT: If overrite property get/set then will go through code when access the
variable, unless it's called from within the property get/set code

inputs
   DWORD          dwID - ID of the Property
   PCMIFLVMObject pObject - Object to use
   BOOL           fIgnoreGetSet - If set the ignore the get/set calls.
   PCMIFLVar      pVar - Will be filled in with the Property variable, including
                  the l-value info
returns
   DWORD - MFC_XXX
*/
DWORD CMIFLVM::PropertyGet (DWORD dwID, PCMIFLVMObject pObject, BOOL fIgnoreGetSet, PCMIFLVarLValue pVar)
{
   // clear LValue
   pVar->Clear();

   PCMIFLVarProp pv = (PCMIFLVarProp)pObject->m_hProp.Find (dwID);

// #define USEPVPNEW - Dont use because would be too slow.
#ifdef USEPVPNEW
   NOTE: Not tested
   // BUGFIX - If cant find property, still need to quickly look to see
   // if there's "get" code in the layer
   PCMIFLVarProp pvpNew = NULL;
   if (!pv) {
      // see if there's a set/get code
      DWORD i;
      for (i = 0; i < pObject->LayerNum(); i++) {
         PCMIFLVMLayer pl = pObject->LayerGet (i);
         PMIFLGETSET pgs = (PMIFLGETSET)pl->m_hPropGetSet.Find (dwID);
         if (pgs) {
            if (!pgs->m_pCodeGet)
               break;

            // else, found
            pv = pvpNew = new CMIFLVarProp;
            if (!pvpNew)
               break;
            pvpNew->m_dwID = dwID;
            // will init to undefined
            pvpNew->m_pCodeGet = pgs->m_pCodeGet;
            pvpNew->m_pCodeSet = pgs->m_pCodeSet;
            break;
         };

         // else, see if based on the object...
         PCMIFLVarProp pFrom = (PCMIFLVarProp) pl->m_pObject->m_hPropDefaultAllClass.Find (dwID);
         if (pFrom) {
            if (!pFrom->m_pCodeGet)
               break;

            // else found
            pv = pvpNew = new CMIFLVarProp;
            if (!pvpNew)
               break;
            pvpNew->m_dwID = dwID;
            // will init to undefined
            pvpNew->m_pCodeGet = pFrom->m_pCodeGet;
            pvpNew->m_pCodeSet = pFrom->m_pCodeSet;
            break;
         }
      } // i
   }
#endif // USEPVPNEW

   if (!pv) {
      pVar->m_Var.SetUndefined();
#ifdef USEPVPNEW
      if (pvpNew)
         delete pvpNew;
#endif // USEPVPNEW
      return MFC_NONE;
   }

   if (pv->m_pCodeGet && !fIgnoreGetSet) {
      // if this global has get/set code associated with it, and we're not
      // already in the get/set code, then run it
      MIFLFCI fci;
      PCMIFLVarList plParam = new CMIFLVarList;
      if (!plParam) {
#ifdef USEPVPNEW
         if (pvpNew)
            delete pvpNew;
#endif // USEPVPNEW
         return MFC_NONE;   // error
      }
      fci.pszCode = (PWSTR)pv->m_pCodeGet->m_memCode.p;
      fci.pMemMIFLCOMP = &pv->m_pCodeGet->m_memMIFLCOMP;
      fci.dwParamCount = pv->m_pCodeGet->m_dwParamCount;
      fci.plParam = plParam;
      fci.phVarsString = &pv->m_pCodeGet->m_hVars;
      fci.pObject = pObject;
      fci.pObjectLayer = pv->m_pCodeGet->m_pObjectLayer;
      fci.pszWhere1 = fci.pObjectLayer ? (PWSTR) fci.pObjectLayer->m_memName.p : NULL;
      fci.pszWhere2 = pv->m_pCodeGet->m_pszCodeName;
      fci.dwPropIDGetSet = dwID;

      DWORD dwRet = FuncCallInternal (&fci, pVar);

      // release parameters
      plParam->Release();

      // remember where came from
      pVar->m_dwLValue = MLV_PROPERTY;
      pVar->m_dwLValueID = dwID;
      pVar->m_pLValue = pObject;

#ifdef USEPVPNEW
      if (pvpNew)
         delete pvpNew;
#endif // USEPVPNEW
      return dwRet;
   }


   // else, read diretly
   pVar->m_Var.Set (&pv->m_Var);

   pVar->m_dwLValue = MLV_PROPERTY;
   pVar->m_dwLValueID = dwID;
   pVar->m_pLValue = pObject;

#ifdef USEPVPNEW
   if (pvpNew)
      delete pvpNew;
#endif // USEPVPNEW
   return MFC_NONE;
}




/*****************************************************************************
CMIFLVM::PropertySet - Call this to set the value of a Property. It's
the safest way to call since it also handles the get/set variable code.

DOCUMENT: If set Property that doesn't exist then create it

inputs
   DWORD          dwID - ID of the Property
   PCMIFLVMObject pObject - Object to use
   BOOL           fIgnoreGetSet - If set the ignore the get/set calls.
   PCMIFLVar      pVar - Variable set it as
returns
   DWORD - MFC_XXX
*/
DWORD CMIFLVM::PropertySet (DWORD dwID, PCMIFLVMObject pObject, BOOL fIgnoreGetSet, PCMIFLVar pVar)
{
   PCMIFLVarProp pv = (PCMIFLVarProp)pObject->m_hProp.Find (dwID);
   
   if (!pv) {
      // doesn't exist so add it, start with undefined so less chance of bug
      CMIFLVarProp vp;
      vp.m_dwID = dwID;

      // see if there's a set/get code
      DWORD i;
      for (i = 0; i < pObject->LayerNum(); i++) {
         PCMIFLVMLayer pl = pObject->LayerGet (i);
         PMIFLGETSET pgs = (PMIFLGETSET)pl->m_hPropGetSet.Find (dwID);
         if (pgs) {
            // else, found
            vp.m_pCodeGet = pgs->m_pCodeGet;
            vp.m_pCodeSet = pgs->m_pCodeSet;
            break;
         };

         // else, see if based on the object...
         PCMIFLVarProp pFrom = (PCMIFLVarProp) pl->m_pObject->m_hPropDefaultAllClass.Find (dwID);
         if (pFrom) {
            // else found
            vp.m_pCodeGet = pFrom->m_pCodeGet;
            vp.m_pCodeSet = pFrom->m_pCodeSet;
            break;
         }
      } // i
      
      pObject->m_hProp.Add (dwID, &vp);

      pv = (PCMIFLVarProp)pObject->m_hProp.Find (dwID);
      if (!pv)
         return MFC_NONE;
   }

   if (pv->m_pCodeSet && !fIgnoreGetSet) {
      // if this global has get/set code associated with it, and we're not
      // already in the get/set code, then run it
      MIFLFCI fci;
      PCMIFLVarList plParam = new CMIFLVarList;
      if (!plParam)
         return MFC_NONE;   // error
      plParam->Add (pVar, FALSE);

      fci.pszCode = (PWSTR)pv->m_pCodeSet->m_memCode.p;
      fci.pMemMIFLCOMP = &pv->m_pCodeSet->m_memMIFLCOMP;
      fci.dwParamCount = pv->m_pCodeSet->m_dwParamCount;
      fci.plParam = plParam;
      fci.phVarsString = &pv->m_pCodeSet->m_hVars;
      fci.pObject = pObject;
      fci.pObjectLayer = pv->m_pCodeSet->m_pObjectLayer;
      fci.pszWhere1 = fci.pObjectLayer ? (PWSTR)fci.pObjectLayer->m_memName.p : NULL;
      fci.pszWhere2 = pv->m_pCodeSet->m_pszCodeName;
      fci.dwPropIDGetSet = dwID;

      CMIFLVarLValue v;
      DWORD dwRet = FuncCallInternal (&fci, &v);   // use &v since don't want to affect what had set

      // release parameters
      plParam->Release();

      return dwRet;
   }

   // set it directly
   pv->m_Var.Set (pVar);

   return MFC_NONE;
}




/*****************************************************************************
CMIFLVM::PropertyRemove - Call this to remove a Property.

inputs
   DWORD          dwID - ID of the Property
   PCMIFLVMObject pObject - Object to use
returns
   BOOL - TRUE if success
*/
BOOL CMIFLVM::PropertyRemove (DWORD dwID, PCMIFLVMObject pObject)
{
   DWORD dwIndex = pObject->m_hProp.FindIndex (dwID);
   if (dwIndex == -1)
      return FALSE;

   PCMIFLVarProp pv = (PCMIFLVarProp)pObject->m_hProp.Get (dwIndex);
   if (!pv)
      return FALSE;

   pv->m_Var.SetUndefined();
   pObject->m_hProp.Remove (dwIndex);
   return TRUE;
}

/*****************************************************************************
CMIFLVM::FuncCallInternal - Internal function call.

inputs
   PMIFLFCI          pFCI - Function call information
   PCMIFLVar         pVar - Filled with the return value.
returns
   DWORD - MFC_XXX, so know what to do when exits
*/
DWORD CMIFLVM::FuncCallInternal (PMIFLFCI pFCI, PCMIFLVarLValue pVar)
{
   // clear LValue
   pVar->Clear();

   // if too deep then error and return
   if (m_dwFuncLevelDeep > m_dwMaxFuncLevelDeep) {
      RunTimeErr (L"Too many functions deep. Stack overflow.", TRUE, 0,0);
      return MFC_REPORTERROR;
   }

   // BUGFIX - if no code then dont bother
   if (!pFCI->pMemMIFLCOMP->m_dwCurPosn)
      return MFC_NONE;

   // allocate all the variables
   CMem memVar;
   DWORD dwNum = pFCI->phVarsString ? pFCI->phVarsString->Num() : 0;
   if (!memVar.Required (dwNum * sizeof(CMIFLVar))) {
      pVar->m_Var.SetUndefined();
      return MFC_ABORTFUNC;
   }
   CMIFLVar vZero;
   DWORD i;
   PCMIFLVar pAllVar = (PCMIFLVar)memVar.p;
   for (i = 0; i < dwNum; i++)
      memcpy (pAllVar + i, &vZero, sizeof(vZero));

   pFCI->dwVarsNum = dwNum;
   pFCI->paVars = pAllVar;
   pFCI->pbMIFLCOMP = (PBYTE)pFCI->pMemMIFLCOMP->p;
   pFCI->fOnlyVarAccess = FALSE;


   // fill in the this pointer, list, and others..
   if (pFCI->pObject && (dwNum > 0))
      pAllVar[0].SetObject (&pFCI->pObject->m_gID);
   if (dwNum > 1)
      pAllVar[1].SetList (pFCI->plParam);
   for (i = 0; i < pFCI->dwParamCount; i++)
      if (dwNum > i+2) {
         PCMIFLVar p = pFCI->plParam->Get(i);
         if (!p)
            break;
         pAllVar[i+2].Set (p);
      }

   // default return value
   DWORD dwRet = MFC_NONE;

   // swap out old one
   PMIFLFCI pOld = m_pFCICur;
   m_pFCICur = pFCI;

   // remember old mode
   DWORD dwDebugModeOld = m_dwDebugMode;
   switch (dwDebugModeOld) {
   case MDM_STEPIN:
      // if was step-in mode then go to everyline
      m_dwDebugMode = MDM_EVERYLINE;
      break;
   case MDM_EVERYLINE:
      // if was everline then goto skip
      m_dwDebugMode = MDM_STEPOVER;
      break;
   }


   // increase function level depth
   m_dwFuncLevelDeep++;

   // do call into function...
   dwRet = ExecuteCode (0, pVar);

   // decrease depth
   m_dwFuncLevelDeep--;

   // revert the mode
   // if was stepping in originall, then revert back to step
   if ((dwDebugModeOld == MDM_STEPIN) && (m_dwDebugMode >= MDM_EVERYLINE))
      m_dwDebugMode = dwDebugModeOld;
   else if ((dwDebugModeOld == MDM_EVERYLINE) && (m_dwDebugMode >= MDM_STEPOVER))
      m_dwDebugMode = dwDebugModeOld;

   // if ended up returning, breaking, or continuing then just ignore and convert to normal
   if (dwRet < MFC_REPORTERROR) {
      if ((dwRet == MFC_RETURN) || (pFCI->pszWhere1 == gpszTypedCode))
         // BUGFIX - Look for gpszTypedCode so that will pass return value up
         pVar->m_dwLValue = MLV_NONE;    // cant return things with l-values;
      else
         pVar->m_Var.SetUndefined();   // no return called
      dwRet = MFC_NONE;
   }

   // swap in old one
   m_pFCICur = pOld;

   // before return, release reference counts
   for (i = 0; i < dwNum; i++)
      pAllVar[i].SetUndefined();

   // if 0 function levels deep then coming out of recusion
   if (!m_dwFuncLevelDeep) {
      // really delete objects
      MaintinenceDeleteAll ();

      // consider killing the debug window
      if (!m_fDebugUIIn && m_pDebugWindowOwn) {
         delete m_pDebugWindowOwn;
         m_pDebugWindowOwn = NULL;
      }
   }

   return dwRet;
}


/*****************************************************************************
CMIFLVM::FunctionCall - Calls a function.

inputs
   DWORD             dwID - Function ID
   PCMIFLVarList     plParam - List of parameters. The refcount will neither be
                        increased or decreased
   PCMIFLVar         pVar - Filled with the return value.
returns
   DWORD - MFC_XXX, so know what to do when exits. MFC_REPORTERROR if doesnt exist
*/
DWORD CMIFLVM::FunctionCall (DWORD dwID, PCMIFLVarList plParam, PCMIFLVarLValue pVar)
{
   // clera LValue
   pVar->Clear();

   PCMIFLFunc pFunc = m_pCompiled->m_pLib->FuncGet (dwID);
   if (!pFunc) {
      // errro
      pVar->m_Var.SetUndefined();
      return MFC_REPORTERROR;
   }

   MIFLFCI fci;
   fci.pszCode = (PWSTR)pFunc->m_Code.m_memCode.p;
   fci.pMemMIFLCOMP = &pFunc->m_Code.m_memMIFLCOMP;
   fci.dwParamCount = pFunc->m_Code.m_dwParamCount;
   fci.plParam = plParam;
   fci.phVarsString = &pFunc->m_Code.m_hVars;
   fci.pObject = NULL;
   fci.pObjectLayer = NULL;
   fci.pszWhere1 = (PWSTR)pFunc->m_Meth.m_memName.p;
   fci.pszWhere2 = NULL;
   fci.dwPropIDGetSet = -1;

   // call
   return FuncCallInternal (&fci, pVar);
}





/*****************************************************************************
CMIFLVM::StringMethodCall - Calls a method specific to a string.

inputs
   PCMIFLVarString   ps - Strings
   DWORD             dwID - Method ID
   PCMIFLVarList     plParam - List of parameters. The refcount will neither be
                        increased or decreased
   DWORD             dwCharStart, dwCharEnd - So can report run-time error
   PCMIFLVar         pVar - Filled with the return value.
returns
   DWORD - MFC_XXX, so know what to do when exist. MFC_REPORTERROR if doesnt exist
*/
DWORD CMIFLVM::StringMethodCall (PCMIFLVarString ps, DWORD dwID, PCMIFLVarList plParam,
                                 DWORD dwCharStart, DWORD dwCharEnd, PCMIFLVarLValue pVar)
{
   // clera LValue
   pVar->Clear();

   // DOCUMENT: Will need to document methods specific to a string

   dwID = IDToVMTOK (dwID);
   DWORD i;
   switch (dwID) {
   case VMTOK_STRINGCONCAT:       // concat 1 or more strings onto the end
      {
         // this adds one or more strings onto the end of the list. It takes any
         // number of paramters. Returns the same string.
         for (i = 0; i < plParam->Num(); i++) {
            PCMIFLVar pv = plParam->Get(i);
            if (!pv)
               continue;

            PCMIFLVarString ps2;
            ps2 = pv->GetString (this);
            if (!ps2)
               continue;

            ps->Append (ps2);
            ps2->Release();
         } // i

         pVar->m_Var.SetString (ps);
      }
      return MFC_NONE;

   case VMTOK_STRINGFROMCHAR:       // create a string from one or more characters of codes
      {
         // accepts any number of parameters. This creates a string from the characters,
         // or converts numbers to character codes.
         WCHAR szTemp[32];
         CMem memTemp;
         PWSTR psz;
         if (plParam->Num()+1 < sizeof(szTemp)/sizeof(WCHAR))
            psz = szTemp;
         else {
            if (!memTemp.Required ((plParam->Num()+1)*sizeof(WCHAR)))
               return RunTimeErr (gpszOutOfMemory, TRUE, dwCharStart, dwCharEnd);

            psz = (PWSTR)memTemp.p;
         }
         for (i = 0; i < plParam->Num(); i++) {
            PCMIFLVar pv = plParam->Get(i);
            if (!pv) {
               psz[i] = L' ';
               continue;
            }

            psz[i] = pv->GetChar (this);
         } // i
         psz[i] = 0;

         ps->Set (psz, i);
      }
      return MFC_NONE;

   case VMTOK_STRINGSEARCH:       // find nth occurance of string in set, case sensative or insensative
      {
         // 1st param is the string to look for. 2nd param (optional) is the index to
         // start searching from, defaults to 0. 3rd param (optional) is the true to
         // do case sensative (default) or false for case insensative
         // returns - index into string, or -1 if cant find

         if (plParam->Num() < 1) {
            pVar->m_Var.SetDouble(-1);
            return RunTimeErr (gpszNotEnoughParameters, TRUE, dwCharStart, dwCharEnd);
         }

         PCMIFLVarString pFind = plParam->Get(0)->GetString(this);
         PWSTR pszFind = pFind->Get();
         PWSTR pszLookIn = ps->Get();
         PCMIFLVar pv = plParam->Get(1);
         DWORD dwIndex = pv ? (DWORD) pv->GetDouble(this) : 0;
         pv = plParam->Get(2);
         BOOL fCaseSens = pv ? pv->GetBOOL(this) : TRUE;

         if (dwIndex >= wcslen(pszLookIn)) {
            // beyond end of string
            pVar->m_Var.SetDouble (-1);
            pFind->Release();
            return MFC_NONE;
         }

         PWSTR psz = fCaseSens ?
            wcsstr (pszLookIn + dwIndex, pszFind) :
            (PWSTR) MyStrIStr (pszLookIn + dwIndex, pszFind);
         if (!psz) {
            // not found
            pVar->m_Var.SetDouble (-1);
            pFind->Release();
            return MFC_NONE;
            }

         // else found
         pVar->m_Var.SetDouble ( (DWORD)((PBYTE)psz - (PBYTE)pszLookIn) / sizeof(WCHAR));

         pFind->Release();
      }
      return MFC_NONE;

   case VMTOK_STRINGLENGTH:       // string length
      {
         // returns the number of characters in the string
         pVar->m_Var.SetDouble (ps->Length());
      }
      return MFC_NONE;

   case VMTOK_STRINGSLICE:       // take from start to end char, use negative values for relative pos
   case VMTOK_STRINGSUBSTRING:       // extract a sub-string from the string, based on start and end
      {
         // this extracts a substring of the current string (not changing current one)
         // 1st param is the starting index, 0 being the first character. If this is
         // negative then will work backwards from the ength (string.length + startindex)
         // 2nd parameter is optional. It's the end of the section to keep. This is
         // always 0 or negative, since results in string.lenth + endindex
         if (plParam->Num() < 1) {
            pVar->m_Var.SetUndefined();
            return RunTimeErr (gpszNotEnoughParameters, TRUE, dwCharStart, dwCharEnd);
         }

         BOOL fSubString = (dwID == VMTOK_STRINGSUBSTRING);

         PWSTR psz = ps->Get();
         PCMIFLVar pv = plParam->Get(0);
         int iLen = (int)wcslen(psz);
         int iStart = pv ? (int) pv->GetDouble(this) : 0;
         pv = plParam->Get(1);
         int iEnd = pv ? (int) pv->GetDouble(this) : (fSubString ? iLen : 0);
         if (!fSubString) {
            if (iStart < 0)
               iStart += iLen;
            iEnd += iLen;
         }
         iStart = max(0, iStart);
         iStart = min(iLen, iStart);
         iEnd = max(iStart, iEnd);
         iEnd = min(iLen, iEnd);

         // temporarily set end to 0, and extract out
         WCHAR c;
         c = psz[iEnd];
         psz[iEnd] = 0;
         pVar->m_Var.SetString (psz + iStart, (DWORD)(iEnd - iStart));
         psz[iEnd] = c;
      }
      return MFC_NONE;

   case VMTOK_STRINGSPLIT:       // split string into list based on delimeter char
      {
         // this splits up the string whenever the 1st parameter (a string or character)
         // is found. The sub-strings are wrapped into a list and returned.

         if (plParam->Num() != 1) {
            pVar->m_Var.SetUndefined();
            return RunTimeErr (gpszNotEnoughParameters, TRUE, dwCharStart, dwCharEnd);
         }

         PWSTR psz = ps->Get();
         PCMIFLVar pv = plParam->Get(0);
         PCMIFLVarList pList = new CMIFLVarList;
         if (!pList) {
            pVar->m_Var.SetUndefined();
            return RunTimeErr (gpszOutOfMemory, TRUE, dwCharStart, dwCharEnd);
         }
         PCMIFLVarString pFind = pv->GetString(this);
         PWSTR pszFind = pFind->Get();
         DWORD dwLen = (DWORD)wcslen(pszFind);
         if (!dwLen) {
            pVar->m_Var.SetList (pList);
            return RunTimeErr (L"The search string must be at least one character.", TRUE, dwCharStart, dwCharEnd);
         }

         WCHAR c;
         CMIFLVar vAdd;
         while (psz) {
            PWSTR pszNext = wcsstr (psz, pszFind);

            if (pszNext) {
               c = pszNext[0];
               pszNext[0] = 0;
            }

            // add it
            vAdd.SetString (psz, (DWORD)-1);
            pList->Add (&vAdd, TRUE);

            // advance
            if (pszNext) {
               pszNext[0] = c;
               psz = pszNext + dwLen;
            }
            else
               break;
         }


         // set return
         pVar->m_Var.SetList (pList);

         // release when done
         pFind->Release();
         pList->Release();
      }
      return MFC_NONE;

   case VMTOK_STRINGTOLOWER:       // convert a string to lower case
   case VMTOK_STRINGTOUPPER:      // convert a string to upper case
      {
         // Generates a lower-case version of the string (without modifying the
         // existing one).
         // no inputs
         // returns the new string

         ps = ps->Clone();
         if (!ps) {
            pVar->m_Var.SetUndefined();
            return RunTimeErr (gpszOutOfMemory, TRUE, dwCharStart, dwCharEnd);
         }

         PWSTR psz = ps->Get();
         if (dwID == VMTOK_STRINGTOLOWER)
            _wcslwr (psz);
         else
            _wcsupr (psz);

         // set return
         pVar->m_Var.SetString (ps);
         ps->Release(); // since created a clone
      }
      return MFC_NONE;

   case VMTOK_STRINGCOMPARE:       // compares string based on case
      {
         // this compares two strings.
         // param1 is the other string.
         // param2 is true if it's case sensative (default), false if it's case insensative
         // returns - 0 if they're the same, negative number if this comes before
         // param1, positive if this comes after param1 in the list

         if (plParam->Num() < 1) {
            pVar->m_Var.SetUndefined();
            return RunTimeErr (gpszNotEnoughParameters, TRUE, dwCharStart, dwCharEnd);
         }

         PWSTR psz = ps->Get();
         PCMIFLVar pv = plParam->Get(0);
         PCMIFLVarString ps2 = pv->GetString(this);
         PWSTR psz2 = ps2->Get();
         pv = plParam->Get(1);
         BOOL fCaseSens = pv ? pv->GetBOOL (this) : TRUE;


         // compare
         pVar->m_Var.SetDouble (fCaseSens ? wcscmp(psz, psz2) : _wcsicmp(psz, psz2));

         // release
         ps2->Release();
      }
      return MFC_NONE;

   case VMTOK_STRINGREPLACE:       // replace the character range with new sub-string
   case VMTOK_STRINGINSERT:       // insert a string at the given location
   case VMTOK_STRINGPREPEND:
   case VMTOK_STRINGAPPEND:
      {
         // STRINGREPLACE
         // modifies a string in place, replacing the range (start to end) with the
         // new string
         // param1 is the string to replace with. If not specified will be NULL string
         // param2 is the starting character. if not specified will be 0
         // param3 is the ending character. If not specified will be end of original string
         // returns same string as original

         // STRINGINSERT
         // modifies the string in place, inserting at the given location
         // param1 is the string to replace with. If not specified will be NULL string
         // param2 is the character to insert before. if not specified will be 0

         // STRINGAPPEND
         // STRINGPREPENT
         // modifies the stirng in place, prepending or appending at the given location
         // param1 is the string to replace with. If not specified will be NULL string

         PCMIFLVar pv = plParam->Get(0);
         PCMIFLVarString ps2 = pv ? pv->GetString(this) : NULL;
         PWSTR pszWith = ps2 ? ps2->Get() : NULL;
         DWORD dwLengthWith = ps2 ? ps2->Length() : (DWORD)-1;
         int iStart;

         switch (dwID) {
         case VMTOK_STRINGREPLACE:
         case VMTOK_STRINGINSERT:
            pv = plParam->Get(1);
            iStart = pv ? (int)pv->GetDouble(this) : 0;
            break;
         case VMTOK_STRINGAPPEND:
            iStart = (int) ps->Length();
            break;
         case VMTOK_STRINGPREPEND:
         default:
            iStart = 0;
            break;
         }

         int iEnd;
         if (dwID == VMTOK_STRINGREPLACE) {
            pv = plParam->Get(2);
            iEnd = pv ? (int)pv->GetDouble(this) : 1000000000;
         }
         else
            iEnd = iStart;


         ps->Replace (pszWith, dwLengthWith, iStart, iEnd);

         // done
         if (ps2)
            ps2->Release();

         pVar->m_Var.SetString (ps);
      }
      return MFC_NONE;

   case VMTOK_STRINGTRIM:       // trims whitespace to left and right. Optional param for only trim from left or right
      {
         // trims the whitespace off the beginning and end of the EXISTING string.
         // if there's a param1 and it's TRUE then trims only off the left side, if FALSE then off right only
         // returns the existing string

         PCMIFLVar pv = plParam->Get(0);
         int iTrim = pv ? (pv->GetBOOL(this) ? -1 : 1) : 0;

         ps->Trim (iTrim);
         pVar->m_Var.SetString (ps);
      }
      return MFC_NONE;

   case VMTOK_CLONE:       // supported by strings and lists.
      {
         // Clones the existing string (or list) and returns a new one

         PCMIFLVarString ps2 = ps->Clone();
         if (!ps2) {
            pVar->m_Var.SetUndefined();
            return RunTimeErr (gpszOutOfMemory, TRUE, dwCharStart, dwCharEnd);
         }

         pVar->m_Var.SetString (ps2);
         ps2->Release();
      }
      return MFC_NONE;

   case VMTOK_STRINGFORMAT:       // replace %1, %2, etc. in string
      {
         // Looks through a string for %1, %2, etc. and replaces them
         // with the 1st, 2nd, etc. parameter passed in.

         PWSTR psz = ps->Get();
         PCMIFLVarString ps2 = new CMIFLVarString;
         if (!ps2) {
            pVar->m_Var.SetUndefined();
            return RunTimeErr (gpszOutOfMemory, TRUE, dwCharStart, dwCharEnd);
         }

         // loop
         while (psz) {
            PWSTR pszNext = psz;
            for (pszNext = psz; pszNext[0]; pszNext++) {
               if (pszNext[0] != L'%')
                  continue;
               if ((pszNext[1] < L'1') || (pszNext[1] > L'9')) {
                  // wasnt followed by digit
                  continue;
               }
               break;   // else stop
            }
            if (!pszNext[0])
               pszNext = NULL;

            // set pszNext to 0
            WCHAR c;
            PCMIFLVar pv;
            if (pszNext) {
               c = pszNext[0];
               pszNext[0] = 0;

               // what's the index?
               DWORD dwIndex = pszNext[1] - L'1';
               pv = plParam->Get(dwIndex);
            }
            else
               pv = NULL;

            // append the existing string
            ps2->Append (psz, (DWORD)-1);

            // convert the param to a string and append
            if (pv) {
               PCMIFLVarString pSub = pv->GetString(this);
               ps2->Append (pSub);
               pSub->Release();
            } // if pv

            // restore and advance
            if (pszNext) {
               pszNext[0] = c;
               psz = pszNext + 2;
            }
            else
               psz = NULL;
         } // while psz

         pVar->m_Var.SetString (ps2);
         ps2->Release();
      }
      return MFC_NONE;


// list tokens
   default:
      return MFC_REPORTERROR; // unknown
   }

   return MFC_NONE;
}


/*****************************************************************************
CMIFLVM::ListMethodCall - Calls a method specific to a string.

inputs
   PCMIFLVarList     pl - List
   DWORD             dwID - Method ID
   PCMIFLVarList     plParam - List of parameters. The refcount will neither be
                        increased or decreased
   DWORD             dwCharStart, dwCharEnd - So can report run-time error
   PCMIFLVar         pVar - Filled with the return value.
returns
   DWORD - MFC_XXX, so know what to do when exist. MFC_REPORTERROR if doesnt exist
*/
DWORD CMIFLVM::ListMethodCall (PCMIFLVarList ps, DWORD dwID, PCMIFLVarList plParam,
                               DWORD dwCharStart, DWORD dwCharEnd, PCMIFLVarLValue pVar)
{
   // clear LValue
   pVar->Clear();

   // DOCUMENT: Will need to document methods specific to a list

   dwID = IDToVMTOK (dwID);
   DWORD i;
   switch (dwID) {
   case VMTOK_LISTCONCAT:       // concatenate many values onto end of list
      {
         // this adds one or more strings onto the end of the list. It takes any
         // number of paramters. Returns the same string.
         for (i = 0; i < plParam->Num(); i++) {
            PCMIFLVar pv = plParam->Get(i);
            if (!pv)
               continue;

            if (!ps->Add (pv, TRUE)) {
               pVar->m_Var.SetList (ps);
               return RunTimeErr (L"Concatenation failed. You may have tried to put the list into itself.", TRUE, dwCharStart, dwCharEnd);
            }
         } // i

         pVar->m_Var.SetList (ps);
      }
      return MFC_NONE;

   case VMTOK_LISTMERGE:       // join second list onto first
      {
         // this mergest a second list into the existing one. It changes
         // the current list in-place.
         // param1 - 2nd list. If this isn't a list then it's just added as an element
         if (plParam->Num() != 1) {
            pVar->m_Var.SetUndefined();
            return RunTimeErr (gpszNotEnoughParameters, TRUE, dwCharStart, dwCharEnd);
         }

         PCMIFLVar pv = plParam->Get(0);
         BOOL fRet = TRUE;
         if (pv)
            fRet = ps->Add (pv, FALSE);

         pVar->m_Var.SetList (ps);

         if (!fRet)
            return RunTimeErr (L"Merge failed. You may have tried to put the list into itself.", TRUE, dwCharStart, dwCharEnd);
      }
      return MFC_NONE;

   case VMTOK_LISTNUMBER:       // returns number of elements
      pVar->m_Var.SetDouble (ps->Num());
      return MFC_NONE;

   case VMTOK_LISTREMOVE:       // removes a specific item or range
      {
         // this removes the given range from the list.
         // param1 - start element
         // param2 - (optional) end element, exclusive
         // returns - Same list. Modified in place
         if (plParam->Num() < 1) {
            pVar->m_Var.SetUndefined();
            return RunTimeErr (gpszNotEnoughParameters, TRUE, dwCharStart, dwCharEnd);
         }

         PCMIFLVar pv = plParam->Get(0);
         int iStart = pv ? (int)pv->GetDouble(this) : 0;
         pv = plParam->Get(1);
         int iEnd = pv ? (int) pv->GetDouble(this) : (iStart+1);
         iStart = max(iStart, 0);
         iEnd = max(iEnd, 0);

         ps->Remove ((DWORD)iStart, (DWORD)iEnd);

         pVar->m_Var.SetList (ps);
      }
      return MFC_NONE;

   case VMTOK_LISTREVERSE:       // reverses the list
      // this reverses a list in place
      // returns the list
      ps->Reverse ();
      pVar->m_Var.SetList (ps);
      return MFC_NONE;

   case VMTOK_LISTSLICE:       // slices, with relative values
   case VMTOK_LISTSUBLIST:
      {
         // this extracts a sublist of the current string (not changing current one)
         // 1st param is the starting index, 0 being the first character. If this is
         // negative then will work backwards from the ength (string.length + startindex)
         // 2nd parameter is optional. It's the end of the section to keep. This is
         // always 0 or negative, since results in string.lenth + endindex
         if (plParam->Num() < 1) {
            pVar->m_Var.SetUndefined();
            return RunTimeErr (gpszNotEnoughParameters, TRUE, dwCharStart, dwCharEnd);
         }

         BOOL fSubString = (dwID == VMTOK_LISTSUBLIST);

         PCMIFLVar pv = plParam->Get(0);
         int iLen = (int)ps->Num();
         int iStart = pv ? (int) pv->GetDouble(this) : 0;
         pv = plParam->Get(1);
         int iEnd = pv ? (int) pv->GetDouble(this) : (fSubString ? iLen : 0);
         if (!fSubString) {
            if (iStart < 0)
               iStart += iLen;
            iEnd += iLen;
         }
         iStart = max(0, iStart);
         iStart = min(iLen, iStart);
         iEnd = max(iStart, iEnd);
         iEnd = min(iLen, iEnd);

         // create new list from cutout...
         PCMIFLVarList pNew = ps->Sublist ((DWORD)iStart, (DWORD)iEnd);
         if (!pNew) {
            pVar->m_Var.SetUndefined();
            return RunTimeErr (gpszOutOfMemory, TRUE, dwCharStart, dwCharEnd);
         }

         pVar->m_Var.SetList (pNew);
         pNew->Release();
      }
      return MFC_NONE;

   case VMTOK_LISTRANDOMIZE:       // randomize order of the list

      // randomizes the current list.
      // returns smae list
      ps->Randomize(this);
      pVar->m_Var.SetList (ps);
      return MFC_NONE;

   case VMTOK_CLONE:       // supported by strings and lists.
      {
         // Clones the existing string (or list) and returns a new one

         PCMIFLVarList ps2 = ps->Clone();
         if (!ps2) {
            pVar->m_Var.SetUndefined();
            return RunTimeErr (gpszOutOfMemory, TRUE, dwCharStart, dwCharEnd);
         }

         pVar->m_Var.SetList (ps2);
         ps2->Release();
      }
      return MFC_NONE;

   case VMTOK_LISTREPLACE:       // replace from elem start to finish, with new elem(s)
   case VMTOK_LISTINSERT:       // insert list elements at
   case VMTOK_LISTAPPEND:
   case VMTOK_LISTPREPEND:
      {
         // STRINGREPLACE
         // modifies a string in place, replacing the range (start to end) with the
         // elements from the new list. If new is not a list then puts in element.
         // param1 is the string to replace with. If not specified will be NULL string
         // param2 is the starting character. if not specified will be 0
         // param3 is the ending character. If not specified will be end of original string
         // returns same string as original

         // STRINGINSERT
         // modifies the string in place, inserting at the given location
         // param1 is the string to replace with. If not specified will be NULL string
         // param2 is the character to insert before. if not specified will be 0
         // param3 is TRUE then sub-lists added, if FALSE (or undefined) then sublists added as itmes

         // STRINGAPPEND
         // STRINGPREPENT
         // modifies the stirng in place, prepending or appending at the given location
         // param1 is the string to replace with. If not specified will be NULL string

         PCMIFLVar pv, pvWith = plParam->Get(0);
         int iStart;

         BOOL fAddSubLists = FALSE;
         switch (dwID) {
         case VMTOK_LISTREPLACE:
            pv = plParam->Get(1);
            iStart = pv ? (int)pv->GetDouble(this) : 0;
            break;
         case VMTOK_LISTINSERT:
            // BUGFIX - Allow third parameter that is the same as list append
            pv = plParam->Get(2);
            if (pv)
               fAddSubLists = pv->GetBOOL (this);

            pv = plParam->Get(1);
            iStart = pv ? (int)pv->GetDouble(this) : 0;
            break;
         case VMTOK_LISTAPPEND:
            iStart = (int) ps->Num();
            break;
         case VMTOK_LISTPREPEND:
         default:
            iStart = 0;
            break;
         }

         int iEnd;
         if (dwID == VMTOK_LISTREPLACE) {
            pv = plParam->Get(2);
            iEnd = pv ? (int)pv->GetDouble(this) : 1000000000;
         }
         else
            iEnd = iStart;

         iStart = max(iStart, 0);
         iStart = min(iStart, (int)ps->Num());
         iEnd = max(iEnd, iStart);
         iEnd = min(iEnd, (int)ps->Num());

         if (iStart != iEnd)
            ps->Remove ((DWORD)iStart, (DWORD)iEnd);
         BOOL fRet = ps->Insert (pvWith, fAddSubLists, (DWORD)iStart);

         pVar->m_Var.SetList (ps);

         if (!fRet)
            return RunTimeErr (L"Replacement failed. You may have tried to put the list into itself.", TRUE, dwCharStart, dwCharEnd);
      }
      return MFC_NONE;


   case VMTOK_LISTSORT:       // sorts list. May have comparison function
      {
         // This sorts the list in place
         // Parm1 - (Optional) Either a function or method to call that will determine
         // if sorting is correct. This function/method takes two params, an A and B.
         // if A appears before B in the list then return a negative number, if A == B
         // thenr eturn 0, if A appears after B then return positive
         // If no param1 then will do a standard sort
         // Returns - Same list

         pVar->m_Var.SetList (ps); // set same list now

         PCMIFLVar pCallback = plParam->Get(0);
         if (pCallback && !QSortCallbackCleanup (pCallback))
            return RunTimeErr (L"The first parameter must be a function or method.", TRUE, dwCharStart, dwCharEnd);

         // sort it
         return QSortList (ps, 0, (int)ps->Num()-1, pCallback);
      }
      return MFC_NONE;

   case VMTOK_LISTSEARCH:       // searches list. May have comparison function
   case VMTOK_LISTSEARCHTOINSERT:       // searches list. May have comparison function
      {
         // This searches for an item within the list.
         // Param1 - What to search for.
         // Parm2 - (Optional) Either a function or method to call that will determine
         // if sorting is correct. This function/method takes two params, an A and B.
         // if A appears before B in the list then return a negative number, if A == B
         // thenr eturn 0, if A appears after B then return positive
         // If no param1 then will do a standard sort
         // NOTE: Must already be sorted using ListSort(), with same sort function
         // Returns - If VMTOK_LISTSEARCH Index of the item it found, or -1
         // Returns - If VMTOK_LISTSEARCHTOINSERT Index of the item to insert before

         PCMIFLVar pComp = plParam->Get(0);
         if (!pComp) {
            pVar->m_Var.SetDouble (-1);
            return RunTimeErr (gpszNotEnoughParameters, TRUE, dwCharStart, dwCharEnd);
         }

         PCMIFLVar pCallback = plParam->Get(1);
         if (pCallback && !QSortCallbackCleanup (pCallback)) {
            pVar->m_Var.SetDouble (-1);
            return RunTimeErr (L"The first parameter must be a function or method.", TRUE, dwCharStart, dwCharEnd);
         }

         // loop
         DWORD dwCur, dw, dwRet;
         DWORD dwNum = ps->Num();
         for (dwCur = 1; dwCur < dwNum; dwCur *= 2);

         dw = 0;
         double f;
         for (; dwCur; dwCur /= 2) {
            DWORD dwTry = dw + dwCur;
            if (dwTry >= dwNum)
               continue;   // too high

            // see how pComp stacks up
            dwRet = QSortCompare (pComp, ps->Get(dwTry), pCallback, &f);
            if (dwRet) {
               pVar->m_Var.SetDouble (-1);
               return dwRet;  // error
            }
            if (f == 0) {
               // exact match
               dw = dwTry;
               break;
            }
            else if (f > 0)
               dw = dwTry; // take higher end
         } // dwCur

         // if out of range then not valid
         BOOL fToInsert = (dwID == VMTOK_LISTSEARCHTOINSERT);
         if (dw >= dwNum) // beyond end => not found
            pVar->m_Var.SetDouble (fToInsert ? (double)dwNum : (double)-1);
         else {
            // else, might be a match
            dwRet = QSortCompare (pComp, ps->Get(dw), pCallback, &f);
            if (f == 0)
               pVar->m_Var.SetDouble ((fp)dw);
            else if (f > 0)
               pVar->m_Var.SetDouble (fToInsert ? (double)(dw+1) : (double)-1);
            else
               pVar->m_Var.SetDouble (fToInsert ? (double)dw : (double)-1);
         }
      }
      return MFC_NONE;
   default:
      return MFC_REPORTERROR; // unknown
   }

   return MFC_NONE;
}



/*****************************************************************************
CMIFLVM::QSortCallbackCleanup - Takes a PCMIFLVar for the callback, and modifies
it to include the object (if it's merely a method without an object).

inputs
   PCMIFLVar            pVar - Modified in place
returns
   BOOl - True if pVar is a valid callback, FALSE if it isn't
*/
BOOL CMIFLVM::QSortCallbackCleanup (PCMIFLVar pVar)
{
   switch (pVar->TypeGet()) {
   case MV_FUNC:
   case MV_OBJECTMETH:
      return TRUE;   // valid

   case MV_METH:
      if (!m_pFCICur->pObject)
         return FALSE;  // not in object, so cant call method

      pVar->SetObjectMeth (&m_pFCICur->pObject->m_gID, pVar->GetValue());
      return TRUE;

   default:
      return FALSE; // not valid
   }
}

/*****************************************************************************
CMIFLVM::QSortCompare - Compares the low and high
 
inputs
   PCMIFLVar            pLow - 1st value
   PCMIFLVar            pHigh - Second value
   PCMIFLVar            pVarCallback - Describes the callback used to for
                        comparison. If NULL then do standard sort
   double               *pf  -Filled in with value.if pLow before pHigh, 0 if same, + if pLow after pHigh
returns
   DWORD - MFC_XXX
*/
DWORD CMIFLVM::QSortCompare (PCMIFLVar pLow, PCMIFLVar pHigh, PCMIFLVar pVarCallback,
                             double *pf)
{
   if (pVarCallback) {
      // create the params
      PCMIFLVarList plParam = new CMIFLVarList;
      CMIFLVarLValue vComp;
      DWORD dwRet;
      GUID gObject;
      if (!plParam) {
         *pf = 0;
         return MFC_ABORTFUNC;   // out of memory
      }
      plParam->Add (pLow, TRUE);
      plParam->Add (pHigh, TRUE);

      switch (pVarCallback->TypeGet()) {
      case MV_FUNC:
         dwRet = FunctionCall (pVarCallback->GetValue(), plParam, &vComp);
         break;
      case MV_OBJECTMETH:
         gObject = pVarCallback->GetGUID();
         dwRet = MethodCall (&gObject, pVarCallback->GetValue(), plParam, 0, 0, &vComp);
         break;
      default:
         dwRet = MFC_NONE;
         break;
      }

      plParam->Release();
      *pf = vComp.m_Var.GetDouble(this);
      return dwRet;
   }
   else {
      // no callback
      *pf = pLow->Compare (pHigh, TRUE, this);
      return MFC_NONE;
   }
}

/*****************************************************************************
CMIFLVM::QSortList - Quick-sorts a list.

DOCUMENT: Will need to document QSortList callback
DOCUMENT: When sort (or search) in list should be of the same type

inputs
   PCMIFLVMList         pl - List
   int                  iLow - Low index
   int                  iHigh - High index, inclusive
   PCMIFLVar            pVarCallback - Describes the callback used to for
                        comparison. If NULL then do standard sort
returns
   DWORD - MFC_XXX
*/
DWORD CMIFLVM::QSortList (PCMIFLVarList pl, int iLow, int iHigh, PCMIFLVar pVarCallback)
{
   if (iHigh <= iLow)
      return MFC_NONE;  // do nothing

   // get low and high
   PCMIFLVar pLow = pl->Get((DWORD)iLow);
   PCMIFLVar pHigh = pl->Get((DWORD)iHigh);
   BYTE abTemp[sizeof(CMIFLVar)];
   double f;
   DWORD dwRet;

   if (iLow+1 == iHigh) {
      // adjacent to one another... do comparison
      dwRet = QSortCompare (pLow, pHigh, pVarCallback, &f);
      if (f > 0) {
         // swap
         memcpy (abTemp, pLow, sizeof (CMIFLVar));
         memcpy (pLow, pHigh, sizeof(CMIFLVar));
         memcpy (pHigh, abTemp, sizeof(CMIFLVar));
      }

      return dwRet;
   } // if only two long

   // take mid element, and swap it with the last element
   int iMid = (iLow + iHigh) / 2;
   PCMIFLVar pMid = pl->Get((DWORD)iMid);
   memcpy (abTemp, pMid, sizeof (CMIFLVar));
   memcpy (pMid, pHigh, sizeof(CMIFLVar));
   memcpy (pHigh, abTemp, sizeof(CMIFLVar));

   int iBottom = iLow;
   PCMIFLVar pBottom = pLow;
   int iTop = iHigh-1;
   PCMIFLVar pTop = pl->Get(iTop);

   while (TRUE) {
      // increase bottom
      for (; iBottom <= iTop; iBottom++, pBottom++) {
         // compare
         dwRet = QSortCompare (pBottom, pHigh, pVarCallback, &f);
         if (dwRet)
            return dwRet;
         if (f > 0)
            break;   // encounter element greater than partition elem
      } // for iBottom <= iTop

      // decrease top
      for (; iBottom <= iTop; iTop--, pTop--) {
         // compare
         dwRet = QSortCompare (pTop, pHigh, pVarCallback, &f);
         if (dwRet)
            return dwRet;
         if (f < 0)
            break;   // encounter element greater than partition elem
      }

      if (iBottom < iTop) {
         // swap
         memcpy (abTemp, pTop, sizeof (CMIFLVar));
         memcpy (pTop, pBottom, sizeof(CMIFLVar));
         memcpy (pBottom, abTemp, sizeof(CMIFLVar));
         continue;
      }

      // got to point where everything left than mid on one side, and greater on other
      break;
   } // while true

   // swap mid with location where bottom ended up
   memcpy (abTemp, pHigh, sizeof (CMIFLVar));
   memcpy (pHigh, pBottom, sizeof(CMIFLVar));
   memcpy (pBottom, abTemp, sizeof(CMIFLVar));

   // sort the 1st half
   dwRet = QSortList (pl, iLow, iBottom - 1, pVarCallback);
   if (dwRet)
      return dwRet;
   dwRet = QSortList (pl, iBottom+1, iHigh, pVarCallback);
   return dwRet;
}


/*****************************************************************************
CMIFLVM::ToPropertyID - Given a PWSTR, which could be a string, this
returns a property ID for it.

inputs
   PWSTR          pszName - name
   BOOL           fCreateIfNotExist - If TRUE then create ID if doesn't exist.
returns
   DWORD dwID - Property ID, or -1 if cant find
*/
DWORD CMIFLVM::ToPropertyID (PWSTR pszName, BOOL fCreateIfNotExist)
{
   PMIFLIDENT pmi = (PMIFLIDENT) m_pCompiled->m_hIdentifiers.Find (pszName, TRUE);
   if (pmi && (pmi->dwType == MIFLI_PROPDEF))
      return pmi->dwID;
      // else fall through

   pmi = (PMIFLIDENT) m_hIdentifiersCustomProperty.Find (pszName, TRUE);
   if (pmi)
      return pmi->dwID;

   if (!fCreateIfNotExist) {  // BUGFIX - Remove pmi|| If wront property type then create anyway as custom prop
      // found but wront type
      return -1;
   }

   // else, not found and want to create if doesn't exist
   MIFLIDENT mi;
   memset (&mi, 0, sizeof(mi));
   mi.dwID = m_dwIDMethPropCur++;
   mi.dwType = MIFLI_PROPDEF;
   m_hIdentifiersCustomProperty.Add (pszName, &mi, TRUE);
   m_hUnIdentifiersCustomProperty.Add (mi.dwID, NULL);
   return mi.dwID;
}

/*****************************************************************************
CMIFLVM::ToPropertyID - Given a CMIFLVar, which could be a string, this
returns a property ID for it.

inputs
   PCMIFLVar      pVar - Variable
   BOOL           fCreateIfNotExist - If TRUE then create ID if doesn't exist.
returns
   DWORD dwID - Property ID, or -1 if cant find
*/
DWORD CMIFLVM::ToPropertyID (PCMIFLVar pVar, BOOL fCreateIfNotExist)
{
   switch (pVar->TypeGet()) {
   case MV_STRING:       // memory points to a PCMIFLVarString
   case MV_STRINGTABLE:       // string table. dwValue is the ID
      break;   // can handle this
   default:
      return -1;  // not supported
   }

   // convert to string
   PCMIFLVarString ps = pVar->GetString(this);
   DWORD dwRet = ToPropertyID (ps->Get(), fCreateIfNotExist);
   ps->Release();
   return dwRet;
}




/*****************************************************************************
CMIFLVM::ToGlobalID - Given a PWSTR, which could be a string, this
returns a global ID for it.

inputs
   PWSTR          pszName - Name of global
   BOOL           fCreateIfNotExist - If TRUE then create ID if doesn't exist.
returns
   DWORD dwID - Property ID, or -1 if cant find
*/
DWORD CMIFLVM::ToGlobalID (PWSTR pszName, BOOL fCreateIfNotExist)
{
   PMIFLIDENT pmi = (PMIFLIDENT) m_pCompiled->m_hIdentifiers.Find (pszName, TRUE);
   if (pmi &&((pmi->dwType == MIFLI_GLOBAL) || (pmi->dwType ==MIFLI_OBJECT)))
      return pmi->dwID;
   pmi = (PMIFLIDENT) m_hIdentifiersCustomGlobal.Find (pszName, TRUE);
   if (pmi)
      return pmi->dwID;

   if (!fCreateIfNotExist) {  // BUGFIX - Remove pmi|| If wront property type then create anyway as custom prop
      // found but wront type
      return -1;
   }

   // else, not found and want to create if doesn't exist
   MIFLIDENT mi;
   memset (&mi, 0, sizeof(mi));
   mi.dwID = m_dwIDMethPropCur++;
   mi.dwType = MIFLI_GLOBAL;
   m_hIdentifiersCustomGlobal.Add (pszName, &mi, TRUE);
   m_hUnIdentifiersCustomGlobal.Add (mi.dwID, NULL);
   return mi.dwID;
}

/*****************************************************************************
CMIFLVM::ToGlobalID - Given a CMIFLVar, which could be a string, this
returns a global ID for it.

inputs
   PCMIFLVar      pVar - Variable
   BOOL           fCreateIfNotExist - If TRUE then create ID if doesn't exist.
returns
   DWORD dwID - Property ID, or -1 if cant find
*/
DWORD CMIFLVM::ToGlobalID (PCMIFLVar pVar, BOOL fCreateIfNotExist)
{
   switch (pVar->TypeGet()) {
   case MV_STRING:       // memory points to a PCMIFLVarString
   case MV_STRINGTABLE:       // string table. dwValue is the ID
      break;   // can handle this
   default:
      return -1;  // not supported
   }

   // convert to string
   PCMIFLVarString ps = pVar->GetString(this);
   DWORD dwRet = ToGlobalID (ps->Get(), fCreateIfNotExist);
   ps->Release();
   return dwRet;

}

/*****************************************************************************
CMIFLVM::ToMethodID - Given a PWSTR for the name, which could be a string, this
returns a method ID for it.

inputs
   PWSTR          pszName - Name
   PWSTR          pszObjectPrivate - If this is a private method for an object then
                     this is the object name. NULL if not private method
   BOOL           fCreateIfNotExist - If TRUE then create ID if doesn't exist.
returns
   DWORD dwID - Property ID, or -1 if cant find
*/
DWORD CMIFLVM::ToMethodID (PWSTR pszName, PWSTR pszObjectPrivate, BOOL fCreateIfNotExist)
{
   // if it's a private method special code.
   PMIFLIDENT p2;
   if (pszObjectPrivate) {
      // BUGFIX - Put this check in so that if save an object to MML with a value, the
      // private method can be restored. Before this, it wouldn't put in a private
      // method
      p2 = (PMIFLIDENT) m_pCompiled->m_hIdentifiers.Find (pszObjectPrivate, TRUE);
      if (!p2 || (p2->dwType != MIFLI_OBJECT))
         return -1;  // error
      PCMIFLObject po = (PCMIFLObject)p2->pEntity;

      PMIFLIDENT pmi;
      pmi = (PMIFLIDENT)po->m_hPrivIdentity.Find (pszName);
      if (pmi)
         return pmi->dwID;
      else
         return -1;  // NOT: Wont create if not exist
   }

   // does it exist already as a main name?
   p2 = (PMIFLIDENT) m_pCompiled->m_hIdentifiers.Find (pszName, TRUE);
   if (p2 && ((p2->dwType == MIFLI_METHDEF) || (p2->dwType == MIFLI_METHPRIV)))
      return p2->dwID;

   p2 = (PMIFLIDENT) m_hIdentifiersCustomMethod.Find (pszName, TRUE);
   if (p2)
      return p2->dwID;

   if (!fCreateIfNotExist)
      return -1;

   if (!MIFLIsNameValid (pszName)) {
      // bad name
      return -1;
   }
   // note: Not lowercasing here, but later

   // else, not found and want to create if doesn't exist
   MIFLIDENT mi;
   memset (&mi, 0, sizeof(mi));
   mi.dwID = m_dwIDMethPropCur++;
   mi.dwType = MIFLI_METHDEF;
   m_hIdentifiersCustomMethod.Add (pszName, &mi, TRUE);
   m_hUnIdentifiersCustomMethod.Add (mi.dwID, NULL);

   return mi.dwID;
}


/*****************************************************************************
CMIFLVM::ToMethodID - Given a CMIFLVar, which could be a string, this
returns a method ID for it.

inputs
   PCMIFLVar      pVar - Variable
   BOOL           fCreateIfNotExist - If TRUE then create ID if doesn't exist.
returns
   DWORD dwID - Property ID, or -1 if cant find
*/
DWORD CMIFLVM::ToMethodID (PCMIFLVar pVar, BOOL fCreateIfNotExist)
{
   switch (pVar->TypeGet()) {
   case MV_METH:
   case MV_OBJECTMETH:
      return pVar->GetValue();
   default:
      return -1; // not supported

   case MV_STRING:       // memory points to a PCMIFLVarString
   case MV_STRINGTABLE:       // string table. dwValue is the ID
      {
         PCMIFLVarString ps = pVar->GetString(this);
         DWORD dwRet = ToMethodID (ps->Get(), NULL, fCreateIfNotExist);
         ps->Release(); // free up string
         return dwRet;
      }
   } // swith typeget

   return -1;  // shouldn get here
}


/*****************************************************************************
CMIFLVM::MethodCall - Calls a method. Make sure to use this with methods
since it will handle the stacked method calls.

inputs
   GUID              *pgObject - Object ID
   DWORD             dwID - Method ID
   PCMIFLVarList     plParam - List of parameters. The refcount will neither be
                        increased or decreased
   PCMIFLVar         pVar - Filled with the return value.
   DWORD             dwCharStart - Start character in case runtime error
   DWORD             dwCharEnd - End character
returns
   DWORD - MFC_XXX, so know what to do when exist. MFC_REPORTERROR if doesnt exist
*/
DWORD CMIFLVM::MethodCall (GUID *pgObject, DWORD dwID, PCMIFLVarList plParam,
                           DWORD dwCharStart, DWORD dwCharEnd, PCMIFLVarLValue pVar)
{
   // clear LValue
   pVar->Clear();

   // get the object
   PCMIFLVMObject pObject = ObjectFind (pgObject);
   if (!pObject) {
      pVar->m_Var.SetUndefined();
      return MFC_REPORTERROR;
   }

   // find the method
   PMIFLIDENT pmi = (PMIFLIDENT) pObject->m_hMeth.Find (dwID);
   if (!pmi)
      goto automethod;

   PCMIFLFunc pFunc = (PCMIFLFunc)pmi->pEntity;
   MIFLFCI fci;

   if (pFunc->m_Meth.m_dwOverride) {
      // this method can be overriden by different layers, allowing cascading calls.
      // therefore, more complex code...

      // DOCUMENT: Overridden methods. If the method returns undefined it passes
      // the call on. If it returns any other value then the chain is broken.
      // Note: This is only for the "Stop on not-undefined". Other types keep on going

      // DOCUMENT: If modifiy elements in "arguments" list then can affect what
      // parameters will be passed into the next method along the line. Use this
      // (for example) to to a "StrengthGet()". The lowest priority ones fills
      // in arguements[0] with the cahracter's strength, and also returns the strength.
      // THe next priority gets arguement[0] and adds 5, writing it back in, and returning
      // the value. Repeat, each layer modifying upon what came before it.

      int iInc = ((pFunc->m_Meth.m_dwOverride == 1) || (pFunc->m_Meth.m_dwOverride == 3)) ? 1 : -1;
      BOOL fAbort = (pFunc->m_Meth.m_dwOverride <= 2);
      int iNumLayer = (int)pObject->LayerNum();
      int iNumClass;
      int i, j;
      DWORD dwRet;

      pVar->m_Var.SetUndefined();

      // loop through all layers in order
      for (i = (iInc > 0) ? 0 : (iNumLayer-1); (i >= 0) && (i < iNumLayer); i += iInc) {
         PCMIFLVMLayer pLayer = pObject->LayerGet ((DWORD)i);
         PCMIFLObject pClass = pLayer->m_pObject;

         // loop through all classes in order
         iNumClass = (int) pClass->m_lClassSuperAllPCMIFLObject.Num();
         PCMIFLObject *ppo = (PCMIFLObject*) pClass->m_lClassSuperAllPCMIFLObject.Get(0);
         for (j = (iInc > 0) ? -2 : (iNumClass-1); (j >= -2) && (j < iNumClass); j += iInc) {
            if (j == -2)
               // BUGFIX - Put in to run the extra methods first
               pmi = (PMIFLIDENT) pLayer->m_hMeth.Find (dwID);
            else {
               PCMIFLObject pUse = (j >= 0) ? ppo[j] : pClass;

               // see if this method exists
               pmi = (PMIFLIDENT) pUse->m_hMethJustThis.Find (dwID);
            }
            if (!pmi)
               continue;   // doesn't support

            // else, function
            pFunc = (PCMIFLFunc)pmi->pEntity;

            if (dwID >= VM_CUSTOMIDRANGE) {
               // custom ID, so find
               DWORD dwIndex = m_hUnIdentifiersCustomMethod.FindIndex (dwID);
               fci.pszWhere2 = m_hIdentifiersCustomMethod.GetString (dwIndex);
            }
            else {
               fci.pszWhere2 = (PWSTR)pFunc->m_Meth.m_memName.p;
            }
            fci.pszCode = (PWSTR)pFunc->m_Code.m_memCode.p;
            fci.pMemMIFLCOMP = &pFunc->m_Code.m_memMIFLCOMP;
            fci.dwParamCount = pFunc->m_Code.m_dwParamCount;
            fci.plParam = plParam;
            fci.phVarsString = &pFunc->m_Code.m_hVars;
            fci.pObject = pObject;
            fci.pObjectLayer = pFunc->m_Code.m_pObjectLayer;   // BUGFIX - Get from code, so right layer
            fci.pszWhere1 = fci.pObjectLayer ? (PWSTR)fci.pObjectLayer->m_memName.p : NULL;
            fci.dwPropIDGetSet = -1;

            // call
            dwRet = FuncCallInternal (&fci, pVar);
            if (dwRet)
               return dwRet;  // error

            // if pVar is not undefined then done
            if (fAbort && (pVar->m_Var.TypeGet () != MV_UNDEFINED))
               return MFC_NONE;
         } // j, all superclasses
      } // i, all layers

      // done. pVar has already been set properly
      return MFC_NONE;
   }

   if (dwID >= VM_CUSTOMIDRANGE) {
      // custom ID, so find
      DWORD dwIndex = m_hUnIdentifiersCustomMethod.FindIndex (dwID);
      fci.pszWhere2 = m_hIdentifiersCustomMethod.GetString (dwIndex);
   }
   else {
      fci.pszWhere2 = (PWSTR)pFunc->m_Meth.m_memName.p;
   }
   fci.pszCode = (PWSTR)pFunc->m_Code.m_memCode.p;
   fci.pMemMIFLCOMP = &pFunc->m_Code.m_memMIFLCOMP;
   fci.dwParamCount = pFunc->m_Code.m_dwParamCount;
   fci.plParam = plParam;
   fci.phVarsString = &pFunc->m_Code.m_hVars;
   fci.pObject = pObject;
   fci.pObjectLayer = pFunc->m_Code.m_pObjectLayer;   // BUGFIX - Get from code, so right layer
   fci.pszWhere1 = fci.pObjectLayer ? (PWSTR)fci.pObjectLayer->m_memName.p : NULL;
   fci.dwPropIDGetSet = -1;

   // call
   return FuncCallInternal (&fci, pVar);

automethod:
   // called if the method doesn't exist in the code, but may be automatically
   // supported by all objects
   dwID = IDToVMTOK (dwID);
   switch (dwID) {
   case VMTOK_LAYERPROPERTYENUM:       // enumerates the public methods in a layer, from classes or added
      {
         // DOCUMENT: Returns a list with all the property get/set this layer supports.
         // NOTE: Only enumerates public properties (from the class). All individually
         // added properties are automatically public.
         // Param1 - Layer number. 0..LayerNumber()-1
         // Param2 - If TRUE then enumerate from class, FALSE then enumerate individually added

         if (plParam->Num() != 2) {
            pVar->m_Var.SetUndefined ();
            return RunTimeErr (gpszNotEnoughParameters, TRUE, dwCharStart, dwCharEnd);
         }

         // get the layer
         PCMIFLVar pv = plParam->Get(0);
         DWORD dwNum = (DWORD)pv->GetDouble(this);
         PCMIFLVMLayer pLayer = pObject->LayerGet(dwNum);
         if (!pLayer) {
            pVar->m_Var.SetNULL();
            return MFC_NONE;
         }

         // create the list...
         PCMIFLVarList pl = new CMIFLVarList;
         if (!pl) {
            pVar->m_Var.SetUndefined();
            return RunTimeErr (gpszOutOfMemory, TRUE, dwCharStart, dwCharEnd);
         }

         DWORD i;
         CMIFLVar v;
         BOOL fFromClass = plParam->Get(1)->GetBOOL(this);
         if (fFromClass) for (i = 0; i < pLayer->m_pObject->m_hPropDefaultAllClass.Num(); i++) {
            PCMIFLVarProp pvp = (PCMIFLVarProp)pLayer->m_pObject->m_hPropDefaultAllClass.Get(i);

            PMIFLIDENT pmi = (PMIFLIDENT)m_pCompiled->m_hUnIdentifiers.Find (pvp->m_dwID);
            if ((pmi->dwType != MIFLI_PROPDEF))
               continue;
            PCMIFLProp pProp = (PCMIFLProp)pmi->pEntity;

            v.SetString ((PWSTR)pProp->m_memName.p, (DWORD)-1);
            pl->Add (&v, TRUE);
         } // i all class
         else for (i = 0; i < pLayer->m_hPropGetSet.Num(); i++) {
            PMIFLGETSET pgs = (PMIFLGETSET)pLayer->m_hPropGetSet.Get(i);
            
            // get the name
            PWSTR psz = NULL;
            if (pgs->dwID >= VM_CUSTOMIDRANGE) {
               DWORD dwIndex = m_hUnIdentifiersCustomProperty.FindIndex (pgs->dwID);
               psz = m_hIdentifiersCustomProperty.GetString (dwIndex);
            }
            else {
               PMIFLIDENT pmi = (PMIFLIDENT) m_pCompiled->m_hUnIdentifiers.Find (pgs->dwID);
               if (!pmi || (pmi->dwType != MIFLI_PROPDEF))
                  continue;

               psz = (PWSTR) ((PCMIFLProp)pmi->pEntity)->m_memName.p;
            }
            if (!psz)
               continue;

            v.SetString (psz, (DWORD)-1);
            pl->Add (&v, TRUE);
         } // i, propgetset

         pVar->m_Var.SetList (pl);
         pl->Release();
      }
      return MFC_NONE;

   case VMTOK_LAYERPROPERTYADD:       // adds a method to a layer
      {
         // DOCUMENT: Adds a property get/set to the list.
         // Param1 - Layer number. 0..LayerNumber()-1
         // Param2 - Property name (as a string). This must be a valid property name. It must be a name
         // of an existing property or a unique name; the ID cannot already be used as a method name, etc.
         // Param3 - Function or method that gets called when this method called with a get. Must return parameter
         // Param4 - Function of method that gets called when this method called with a set. Must accept one param.
         // If exclude BOTH param3 and Param4 then wont have a get/set function
         // Returns TRUE if it's added, FALSE if fail

         if (plParam->Num() < 2) {
            pVar->m_Var.SetUndefined ();
            return RunTimeErr (gpszNotEnoughParameters, TRUE, dwCharStart, dwCharEnd);
         }

         // get the layer
         PCMIFLVar pv = plParam->Get(0);
         DWORD dwNum = (DWORD)pv->GetDouble(this);
         PCMIFLVMLayer pLayer = pObject->LayerGet(dwNum);
         if (!pLayer) {
            pVar->m_Var.SetBOOL(FALSE);
            return MFC_NONE;
         }

         MIFLGETSET mgs;
         memset (&mgs, 0, sizeof(mgs));

         // figure out the code for the layer
         PMIFLIDENT pmi = NULL;
         MIFLIDENT miFunc;
         DWORD j;
         for (j = 0; j < 2; j++) {
            pv = plParam->Get(2+j);
            if (!pv)
               continue;

            switch (pv->TypeGet()) {
            case MV_NULL:
            case MV_UNDEFINED:
               continue;   // leave NULL

            case MV_METH:
               // since don't specify object link to current one
               pv->SetObjectMeth (&pObject->m_gID, pv->GetValue());
               // fall through
            case MV_OBJECTMETH:
               {
                  GUID g = pv->GetGUID();
                  PCMIFLVMObject po = ObjectFind (&g);
                  if (!po) {
                     // coudlnt find object, so cant create
                     pVar->m_Var.SetBOOL (FALSE);
                     return MFC_NONE;
                  }

                  pmi = (PMIFLIDENT) po->m_hMeth.Find (pv->GetValue());
               }
               break;

            case MV_FUNC:
               {
                  PCMIFLFunc pFunc = m_pCompiled->m_pLib->FuncGet (pv->GetValue());
                  if (pFunc) {
                     memset (&miFunc, 0, sizeof(miFunc));
                     miFunc.dwType = MIFLI_FUNC;
                     miFunc.pEntity = pFunc;
                     pmi = &miFunc;
                  }
               }
               break;
            }
            if (!pmi) {
               // err - inapprpriate type
               pVar->m_Var.SetBOOL (FALSE);
               return MFC_NONE;
            }
            
            // verify right number of params
            PCMIFLFunc pFunc = (PCMIFLFunc)pmi->pEntity;
            if (j)
               mgs.m_pCodeSet = &pFunc->m_Code;
            else
               mgs.m_pCodeGet = &pFunc->m_Code;
         } // j


         // get the name
         pv = plParam->Get(1);
         DWORD dwID = ToPropertyID (pv, TRUE);
         if (dwID == -1) {
            pVar->m_Var.SetBOOL (FALSE);
            return MFC_NONE;
         }
         mgs.dwID = dwID;


         // if it already exists in the layer then dont add
         if (-1 != pLayer->m_hPropGetSet.FindIndex (dwID)) {
            pVar->m_Var.SetBOOL (FALSE);
            return MFC_NONE;
         }

         // else, it doesnt exist, so add it
         pLayer->m_hPropGetSet.Add (dwID, &mgs);
         pObject->LayerMerge (); // remerge

         pVar->m_Var.SetBOOL (TRUE);
      }
      return MFC_NONE;

   case VMTOK_LAYERPROPERTYREMOVE:       // removes a method from a layer
      {
         // DOCUMENT: Removes a property from the list of individual properties.
         // Param1 - Layer number. 0..LayerNumber()-1
         // Param2 - Property name (as a string).

         if (plParam->Num() != 2) {
            pVar->m_Var.SetUndefined ();
            return RunTimeErr (gpszNotEnoughParameters, TRUE, dwCharStart, dwCharEnd);
         }

         // get the layer
         PCMIFLVar pv = plParam->Get(0);
         DWORD dwNum = (DWORD)pv->GetDouble(this);
         PCMIFLVMLayer pLayer = pObject->LayerGet(dwNum);
         if (!pLayer) {
            pVar->m_Var.SetBOOL(FALSE);
            return MFC_NONE;
         }

         // get the name
         pv = plParam->Get(1);
         DWORD dwID = ToPropertyID (pv, FALSE);
         if (dwID == -1) {
            pVar->m_Var.SetBOOL (FALSE);
            return MFC_NONE;
         }


         // find index
         DWORD dwIndex = pLayer->m_hPropGetSet.FindIndex (dwID);
         if (dwIndex == -1) {
            // not there
            pVar->m_Var.SetBOOL (FALSE);
            return MFC_NONE;
         }

         // else, remove it
         pLayer->m_hPropGetSet.Remove (dwIndex);
         pObject->LayerMerge (); // remerge

         pVar->m_Var.SetBOOL (TRUE);
      }
      return MFC_NONE;





   case VMTOK_LAYERMETHODENUM:       // enumerates the public methods in a layer, from classes or added
      {
         // DOCUMENT: Returns a list with all the methods this layer supports.
         // NOTE: Only enumerates public methods (from the class). All individually
         // added methods are automatically public.
         // Param1 - Layer number. 0..LayerNumber()-1
         // Param2 - If TRUE then enumerate from class, FALSE then enumerate individually added

         if (plParam->Num() != 2) {
            pVar->m_Var.SetUndefined ();
            return RunTimeErr (gpszNotEnoughParameters, TRUE, dwCharStart, dwCharEnd);
         }

         // get the layer
         PCMIFLVar pv = plParam->Get(0);
         DWORD dwNum = (DWORD)pv->GetDouble(this);
         PCMIFLVMLayer pLayer = pObject->LayerGet(dwNum);
         if (!pLayer) {
            pVar->m_Var.SetNULL();
            return MFC_NONE;
         }

         // create the list...
         PCMIFLVarList pl = new CMIFLVarList;
         if (!pl) {
            pVar->m_Var.SetUndefined();
            return RunTimeErr (gpszOutOfMemory, TRUE, dwCharStart, dwCharEnd);
         }

         DWORD i;
         CMIFLVar v;
         BOOL fFromClass = plParam->Get(1)->GetBOOL(this);
         PCHashDWORD pHash = fFromClass ? &pLayer->m_pObject->m_hMethAllClass : &pLayer->m_hMeth;
         for (i = 0; i < pHash->Num(); i++) {
            PMIFLIDENT pmi = (PMIFLIDENT)pHash->Get(i);
            if (fFromClass && (pmi->dwType != MIFLI_METHDEF))
               continue;

            v.SetMeth (pmi->dwID);
            pl->Add (&v, TRUE);
         } // i

         pVar->m_Var.SetList (pl);
         pl->Release();
      }
      return MFC_NONE;

   case VMTOK_LAYERMETHODADD:       // adds a method to a layer
      {
         // DOCUMENT: Adds a method to the list. This only adds invividual methods.
         // Param1 - Layer number. 0..LayerNumber()-1
         // Param2 - Method name (as a string or a method). This must be a valid method name. It must be a name
         // of an existing method or a unique name; the ID cannot already be used as a variable name, etc.
         // Param3 - Function or method that gets called when this method called
         // Returns TRUE if it's added, FALSE if fail

         if (plParam->Num() != 3) {
            pVar->m_Var.SetUndefined ();
            return RunTimeErr (gpszNotEnoughParameters, TRUE, dwCharStart, dwCharEnd);
         }

         // get the layer
         PCMIFLVar pv = plParam->Get(0);
         DWORD dwNum = (DWORD)pv->GetDouble(this);
         PCMIFLVMLayer pLayer = pObject->LayerGet(dwNum);
         if (!pLayer) {
            pVar->m_Var.SetBOOL(FALSE);
            return MFC_NONE;
         }

         // figure out the code for the layer
         PMIFLIDENT pmi = NULL;
         MIFLIDENT miFunc;
         pv = plParam->Get(2);
         switch (pv->TypeGet()) {
         case MV_METH:
            // since don't specify object link to current one
            pv->SetObjectMeth (&pObject->m_gID, pv->GetValue());
            // fall through
         case MV_OBJECTMETH:
            {
               GUID g = pv->GetGUID();
               PCMIFLVMObject po = ObjectFind (&g);
               if (!po) {
                  // coudlnt find object, so cant create
                  pVar->m_Var.SetBOOL (FALSE);
                  return MFC_NONE;
               }

               pmi = (PMIFLIDENT) po->m_hMeth.Find (pv->GetValue());
            }
            break;

         case MV_FUNC:
            {
               PCMIFLFunc pFunc = m_pCompiled->m_pLib->FuncGet (pv->GetValue());
               if (pFunc) {
                  memset (&miFunc, 0, sizeof(miFunc));
                  miFunc.dwType = MIFLI_FUNC;
                  miFunc.pEntity = pFunc;
                  pmi = &miFunc;
               }
            }
            break;
         }
         if (!pmi) {
            // err - inapprpriate type
            pVar->m_Var.SetBOOL (FALSE);
            return MFC_NONE;
         }


         // get the name
         pv = plParam->Get(1);
         DWORD dwID = ToMethodID (pv, TRUE);
         if (dwID == -1) {
            pVar->m_Var.SetBOOL (FALSE);
            return MFC_NONE;
         }


         // if it already exists in the layer then dont add
         if (-1 != pLayer->m_hMeth.FindIndex (dwID)) {
            pVar->m_Var.SetBOOL (FALSE);
            return MFC_NONE;
         }

         // else, it doesnt exist, so add it
         MIFLIDENT mi;
         memset (&mi, 0, sizeof(mi));
         mi.dwID = dwID;
         mi.dwType = (pmi->dwType == MIFLI_METHDEF) ? MIFLI_METHDEF : MIFLI_METHPRIV;
            // if was a function dwType set to MethPriv
         mi.pEntity = pmi->pEntity;
         pLayer->m_hMeth.Add (dwID, &mi);
         pObject->LayerMerge (); // remerge

         pVar->m_Var.SetBOOL (TRUE);
      }
      return MFC_NONE;

   case VMTOK_LAYERMETHODREMOVE:       // removes a method from a layer
      {
         // DOCUMENT: Removes a method from the list of individual methods.
         // Param1 - Layer number. 0..LayerNumber()-1
         // Param2 - Method name (as a string or a method). This must be a valid method name. It must be a name
         // of an existing method in the list

         if (plParam->Num() != 2) {
            pVar->m_Var.SetUndefined ();
            return RunTimeErr (gpszNotEnoughParameters, TRUE, dwCharStart, dwCharEnd);
         }

         // get the layer
         PCMIFLVar pv = plParam->Get(0);
         DWORD dwNum = (DWORD)pv->GetDouble(this);
         PCMIFLVMLayer pLayer = pObject->LayerGet(dwNum);
         if (!pLayer) {
            pVar->m_Var.SetBOOL(FALSE);
            return MFC_NONE;
         }

         // get the name
         pv = plParam->Get(1);
         DWORD dwID = ToMethodID (pv, FALSE);
         if (dwID == -1) {
            pVar->m_Var.SetBOOL (FALSE);
            return MFC_NONE;
         }


         // find index
         DWORD dwIndex = pLayer->m_hMeth.FindIndex (dwID);
         if (dwIndex == -1) {
            // not there
            pVar->m_Var.SetBOOL (FALSE);
            return MFC_NONE;
         }

         // else, remove it
         pLayer->m_hMeth.Remove (dwIndex);
         pObject->LayerMerge (); // remerge

         pVar->m_Var.SetBOOL (TRUE);
      }
      return MFC_NONE;



   case VMTOK_LAYERNUM:       // returns the number of layers
      {
         // DOCUMENT: Returns the number of layers in an object
         pVar->m_Var.SetDouble(pObject->LayerNum());
      }
      return MFC_NONE;

   case VMTOK_LAYERGET:       // gets a layer
      {
         // DOCUMENT: Gets information about a specific layer
         // Param1 - Layer number, 0 to LayerNumber()-1
         // Returns a list. List[0] is the name of the layer, list[1] is the class
         // it uses, list[2] is the priority. Returns NULL if invalid layer
         PCMIFLVar pv = plParam->Get(0);
         if (!pv) {
            pVar->m_Var.SetUndefined ();
            return RunTimeErr (gpszNotEnoughParameters, TRUE, dwCharStart, dwCharEnd);
         }
         DWORD dwNum = (DWORD)pv->GetDouble(this);

         PCMIFLVMLayer pLayer = pObject->LayerGet(dwNum);
         if (!pLayer) {
            pVar->m_Var.SetNULL();
            return MFC_NONE;
         }

         PCMIFLVarList pl = new CMIFLVarList;
         if (!pl) {
            pVar->m_Var.SetUndefined();
            return RunTimeErr (gpszOutOfMemory, TRUE, dwCharStart, dwCharEnd);
         }

         CMIFLVar v;
         v.SetString ((PWSTR)pLayer->m_memName.p, (DWORD)-1);
         pl->Add (&v, TRUE);
         v.SetString ((PWSTR)pLayer->m_pObject->m_memName.p, (DWORD)-1);
         pl->Add (&v, TRUE);
         v.SetDouble (pLayer->m_fRank);
         pl->Add (&v, TRUE);

         pVar->m_Var.SetList (pl);
         pl->Release();
      }
      return MFC_NONE;

   case VMTOK_LAYERADD:       // adds a layer
      {
         // DOCUMENT: Adds a new layer to the object. NOTE: When added list will
         // be resorted according to priority
         // Param1 - Layer name. String
         // Param2 - Class to base off of, a string. If not valid will error
         // Param3 - Priority, higher numbers place layer on top (first to be called)
         // Returns TRUE if success, FALSE if fail
         if (plParam->Num() != 3) {
            pVar->m_Var.SetUndefined ();
            return RunTimeErr (gpszNotEnoughParameters, TRUE, dwCharStart, dwCharEnd);
         }

         PCMIFLVarString psName, psClass;
         fp fPriority;
         psName = plParam->Get(0)->GetString(this);
         psClass = plParam->Get(1)->GetString(this);
         fPriority = plParam->Get(2)->GetDouble(this);

         // make sure can get the class
         PCMIFLObject pClass = m_pCompiled->m_pLib->ObjectGet (
            m_pCompiled->m_pLib->ObjectFind (psClass->Get(), -1));
         if (!pClass) {
            psName->Release();
            psClass->Release();
            pVar->m_Var.SetBOOL (FALSE);
            return MFC_NONE;
         }

         // try adding it
         pVar->m_Var.SetBOOL (pObject->LayerAdd (psName->Get(), fPriority, pClass, NULL, NULL));

         // finally
         psName->Release();
         psClass->Release();
      }
      return MFC_NONE;

   case VMTOK_LAYERREMOVE:       // removes a layer
      {
         // DOCUMENT: Removes the a specific layer
         // Param1 - Layer number, 0 to LayerNumber()-1
         // Returns TRUE if success
         PCMIFLVar pv = plParam->Get(0);
         if (!pv) {
            pVar->m_Var.SetUndefined ();
            return RunTimeErr (gpszNotEnoughParameters, TRUE, dwCharStart, dwCharEnd);
         }
         DWORD dwNum = (DWORD)pv->GetDouble(this);

         pVar->m_Var.SetBOOL (pObject->LayerRemove (dwNum));
      }
      return MFC_NONE;


   case VMTOK_TIMERSUSPENDSET:       // sets the suspend state of an object's timers
      {
         // DOCUMENT: Suspends (or restarts) timers
         // Param1 - double for timer scale. 0.0 to suspend, 1.0 to restart. If no param then suspend
         // Param2 - TRUE to do all children, FALSE for only this. If no param then only this
         // Returns nothing
         PCMIFLVar pv = plParam->Get(0);
         PCMIFLVar pv2 = plParam->Get(1);

         pObject->TimerSuspendSet (pv ? pv->GetDouble(this) : 0.0, pv2 ? pv2->GetBOOL(this) : FALSE);
         pVar->m_Var.SetUndefined();
      }
      return MFC_NONE;


   case VMTOK_TIMERSUSPENDGET:       // gets the suspend state of an objdcts tmiers
      {
         // DOCUMENT: Returns TRUE if timers suspended for object, FALSE if not
         pVar->m_Var.SetDouble(pObject->TimerSuspendGet ());
      }
      return MFC_NONE;

   case VMTOK_TIMERADD:       // adds a timer
      {
         // DOCUMENT: Adds a new timer
         // param1 -      pVarName - Name that will use to identify the timer. If
         //                  it's a string then case sensative compare
         // param2 -      fRepeating - TRUE if want timer to repeat, FALSE if only happens once
         //                  and then autoamtically kills itself.
         // param3 -      fTimeRepeat - Number of seconds before it goes off (and/or) repeats.
         // param4 -      Function or method to call.
         // param5+ -     Parameters for the function
         // Returns - TRUE if added, FALSE if not (maybe because timer already exists)
         if (plParam->Num() < 4) {
            pVar->m_Var.SetUndefined ();
            return RunTimeErr (gpszNotEnoughParameters, TRUE, dwCharStart, dwCharEnd);
         }

         // get the parametrs
         CMIFLVar vParams;
         vParams.SetList (plParam);
         vParams.Fracture();
         PCMIFLVarList pl = vParams.GetList();
         pl->Remove (0, 4);
         pl->Release();

         // callback
         GUID gCall;
         DWORD dwCallID;
         PCMIFLVar pv;
         pv = plParam->Get(3);
         switch (pv->TypeGet()) {
         case MV_METH:
            gCall = pObject->m_gID;
            break;
         case MV_OBJECTMETH:
            gCall = pv->GetGUID();
            break;
         case MV_FUNC:
            gCall = GUID_NULL;
            break;
         default:
            pVar->m_Var.SetUndefined ();
            return RunTimeErr (L"Function or method call expected.", TRUE, dwCharStart, dwCharEnd);
            break;
         }
         dwCallID = pv->GetValue();

         pVar->m_Var.SetBOOL (pObject->TimerAdd (plParam->Get(0), plParam->Get(1)->GetBOOL(this),
            plParam->Get(2)->GetDouble(this), &gCall, dwCallID, &vParams));
      }
      return MFC_NONE;


   case VMTOK_TIMERREMOVE:       // removes a timer
      {
         // DOCUMENT: Removes the timer
         // Param1 - Timer ID
         // Returns TRUE if found and removed, FALSE if not
         PCMIFLVar pv = plParam->Get(0);
         if (!pv) {
            pVar->m_Var.SetUndefined ();
            return RunTimeErr (gpszNotEnoughParameters, TRUE, dwCharStart, dwCharEnd);
         }

         pVar->m_Var.SetBOOL (pObject->TimerRemove (pv));
      }
      return MFC_NONE;


   case VMTOK_TIMERENUM:       // enumerates a timer
      {
         // DOCUMENT: Returns list of timers.
         pObject->TimerEnum (&pVar->m_Var);
      }
      return MFC_NONE;

   case VMTOK_TIMERQUERY:       // sees if a timer exists
      {
         // DOCUMENT: Tests to see if timer exists and returns information about it
         // Param1 - Timer ID
         // Returns - List with timer information, as defined in CMIFLVMObject::TimerQuery()
         PCMIFLVar pv = plParam->Get(0);
         if (!pv) {
            pVar->m_Var.SetUndefined ();
            return RunTimeErr (gpszNotEnoughParameters, TRUE, dwCharStart, dwCharEnd);
         }

         pObject->TimerQuery (pv, &pVar->m_Var);
      }
      return MFC_NONE;




   case VMTOK_DELETETREE:
      // DOCUMENT: Deletes the object and all its children
      // Return TRUE if success, FALSE if object already deleted
      pVar->m_Var.SetBOOL (ObjectDeleteFamily (&pObject->m_gID));
      return MFC_NONE;

   case VMTOK_METHODCALL:       // calls a public method
      {
         // DOCUMENT: Calls a public method
         // NOTE: This only handles public methods
         // Param1 - method, either as a typed method or a string
         // Param2+ - Parameters for the method
         // Returns - Whatever the method returns, or undefined if error
         PCMIFLVar pv = plParam->Get(0);
         if (!pv) {
            pVar->m_Var.SetUndefined ();
            return RunTimeErr (gpszNotEnoughParameters, TRUE, dwCharStart, dwCharEnd);
         }

         // make sure it's a proper method
         pv->ToMeth (this);
         if (pv->TypeGet() != MV_METH) {
            // BUGFIX - Had a runtime error, but more convenient to return undefined
            pVar->m_Var.SetUndefined ();
            return MFC_NONE;
            // return RunTimeErr (L"Parameter 1 can't be converted to a method.", TRUE, dwCharStart, dwCharEnd);
         }
         DWORD dwID = pv->GetValue();

         // remove the 1st parameter and call in
         // NOTE: OK to do this since won't be passing around list elsewhere
         plParam->Remove (0, 1);

         return MethodCall (pgObject, dwID, plParam, dwCharStart, dwCharEnd, pVar);
      }
      return MFC_NONE;

   case VMTOK_METHODQUERY:       // queries to see if a public method exists
      {
         // DOCUMENT: Queries if a method is supported by an object, using a string
         // Param1 = name
         // NOTE: This only queries public methods
         // Returns - TRUE if exists, FALSE if didn't exist
         PCMIFLVar pv = plParam->Get(0);
         if (!pv) {
            pVar->m_Var.SetUndefined ();
            return RunTimeErr (gpszNotEnoughParameters, TRUE, dwCharStart, dwCharEnd);
         }

         // make sure it's a proper method
         pv->ToMeth (this);
         if (pv->TypeGet() != MV_METH) {
            // BUGFIX - Had a run-time error, but more conventient to just FALSE
            pVar->m_Var.SetBOOL (FALSE);
            return MFC_NONE;
            //pVar->m_Var.SetUndefined ();
            // return RunTimeErr (L"Parameter 1 can't be converted to a method.", TRUE, dwCharStart, dwCharEnd);
         }

         PMIFLIDENT pmi = (PMIFLIDENT) pObject->m_hMeth.Find (pv->GetValue());
         pVar->m_Var.SetBOOL (pmi && (pmi->dwType == MIFLI_METHDEF));
      }
      return MFC_NONE;

   case VMTOK_METHODENUM:       // enumerates public methods
      {
         // DOCUMENT: Returns a list with all the methods this object supports
         // NOTE: Only enumerates public methods
         
         PCMIFLVarList pl = new CMIFLVarList;
         if (!pl) {
            pVar->m_Var.SetUndefined();
            return RunTimeErr (gpszOutOfMemory, TRUE, dwCharStart, dwCharEnd);
         }

         DWORD i;
         CMIFLVar v;
         for (i = 0; i < pObject->m_hMeth.Num(); i++) {
            PMIFLIDENT pmi = (PMIFLIDENT)pObject->m_hMeth.Get(i);
            if (pmi->dwType != MIFLI_METHDEF)
               continue;

            v.SetMeth (pmi->dwID);
            pl->Add (&v, TRUE);
         } // i

         pVar->m_Var.SetList (pl);
         pl->Release();

      }
      return MFC_NONE;




   case VMTOK_PROPERTYGET:       // get a property via string
      {
         // DOCUMENT: Gets a property from an object, using a string
         // NOTE: This only gets public properties
         // Param1 - name
         // Returns - Property value, of Undefined if doens't exist
         PCMIFLVar pv = plParam->Get(0);
         if (!pv) {
            pVar->m_Var.SetUndefined ();
            return RunTimeErr (gpszNotEnoughParameters, TRUE, dwCharStart, dwCharEnd);
         }
         DWORD dwID = ToPropertyID (pv, FALSE);
         if (dwID == -1) {
            pVar->m_Var.SetUndefined();
            return MFC_NONE;
         }

         // else, get...
         return PropertyGet (dwID, pObject, (m_pFCICur->pObject==pObject) && (m_pFCICur->dwPropIDGetSet == dwID), pVar);
      }
      return MFC_NONE;

   case VMTOK_PROPERTYSET:       // set a property via string
      {
         // DOCUMENT: Sets a property for an object, using a string
         // NOTE: This only sets public properties
         // Param1 - name
         // Param2 - new value
         // If a property doesn't already exist then it's created
         // Returns - Property value, of Undefined if doens't exist
         PCMIFLVar pv = plParam->Get(0);
         PCMIFLVar pv2 = plParam->Get(1);
         if (!pv || !pv2) {
            pVar->m_Var.SetUndefined ();
            return RunTimeErr (gpszNotEnoughParameters, TRUE, dwCharStart, dwCharEnd);
         }
         DWORD dwID = ToPropertyID (pv, TRUE);
         if (dwID == -1) {
            pVar->m_Var.SetUndefined();
            return MFC_NONE;
         }

         // else, set...
         pVar->m_Var.Set (pv2);
         return PropertySet (dwID, pObject, (m_pFCICur->pObject==pObject) && (m_pFCICur->dwPropIDGetSet == dwID), pv2);
      }
      return MFC_NONE;

   case VMTOK_PROPERTYREMOVE:       // remove a property via string
      {
         // DOCUMENT: Removes a property from an object, using a string
         // Param1 = name
         // NOTE: This only removes public properties
         // Returns - TRUE if removed, FALSE if didn't exist
         PCMIFLVar pv = plParam->Get(0);
         if (!pv) {
            pVar->m_Var.SetUndefined ();
            return RunTimeErr (gpszNotEnoughParameters, TRUE, dwCharStart, dwCharEnd);
         }
         DWORD dwID = ToPropertyID (pv, FALSE);
         if (dwID == -1) {
            pVar->m_Var.SetBOOL (FALSE);
            return MFC_NONE;
         }

         // else, get...
         pVar->m_Var.SetBOOL (PropertyRemove (dwID, pObject));
      }
      return MFC_NONE;

   case VMTOK_PROPERTYQUERY:       // sees if property supported
      {
         // DOCUMENT: Queries if a property is supported by an object, using a string
         // Param1 = name
         // NOTE: This only queries public properties
         // Returns - TRUE if exists, FALSE if didn't exist
         PCMIFLVar pv = plParam->Get(0);
         if (!pv) {
            pVar->m_Var.SetUndefined ();
            return RunTimeErr (gpszNotEnoughParameters, TRUE, dwCharStart, dwCharEnd);
         }
         DWORD dwID = ToPropertyID (pv, FALSE);
         if (dwID == -1) {
            pVar->m_Var.SetBOOL (FALSE);
            return MFC_NONE;
         }

         // else, get...
         dwID = pObject->m_hProp.FindIndex (dwID);
         pVar->m_Var.SetBOOL (dwID != -1);
      }
      return MFC_NONE;

   case VMTOK_PROPERTYENUM:       // enumerates properties
      {
         // DOCUMENT: Returns a list with all the properties this object supports
         // NOTE: Only enumerates public properties
         
         PCMIFLVarList pl = new CMIFLVarList;
         if (!pl) {
            pVar->m_Var.SetUndefined();
            return RunTimeErr (gpszOutOfMemory, TRUE, dwCharStart, dwCharEnd);
         }

         DWORD i;
         CMIFLVar v;
         for (i = 0; i < pObject->m_hProp.Num(); i++) {
            PCMIFLVarProp pvp = (PCMIFLVarProp)pObject->m_hProp.Get(i);

            PWSTR psz = NULL;
            if (pvp->m_dwID >= VM_CUSTOMIDRANGE) {
               DWORD dwIndex = m_hUnIdentifiersCustomProperty.FindIndex (pvp->m_dwID);
               psz = m_hIdentifiersCustomProperty.GetString (dwIndex);
            }
            else {
               PMIFLIDENT pmi = (PMIFLIDENT) m_pCompiled->m_hUnIdentifiers.Find (pvp->m_dwID);
               if (!pmi || (pmi->dwType != MIFLI_PROPDEF))
                  continue;

               psz = (PWSTR) ((PCMIFLProp)pmi->pEntity)->m_memName.p;
            }
            if (!psz)
               continue;

            v.SetString (psz, (DWORD)-1);
            pl->Add (&v, TRUE);
         } // i

         pVar->m_Var.SetList (pl);
         pl->Release();

      }
      return MFC_NONE;



   case VMTOK_CLASSENUM:       // enumerates all the classes that this is in
      {
         // DOCUMENT: Returns a list with all the classes its part of
         // the list is sorted by the highest class first
         
         PCMIFLVarList pl = new CMIFLVarList;
         if (!pl) {
            pVar->m_Var.SetUndefined();
            return RunTimeErr (gpszOutOfMemory, TRUE, dwCharStart, dwCharEnd);
         }

         DWORD i, j;
         CMIFLVar v;
         for (i = 0; i < pObject->LayerNum(); i++) {
            PCMIFLVMLayer pLayer = pObject->LayerGet(i);
            PCMIFLObject po = pLayer->m_pObject;

            // set itself as class...
            v.SetString ((PWSTR)po->m_memName.p, (DWORD)-1);
            pl->Add (&v, TRUE);

            for (j = 0; j < po->m_lClassSuperAll.Num(); j++) {
               v.SetString ((PWSTR)po->m_lClassSuperAll.Get(j), (DWORD)-1);
               pl->Add (&v, TRUE);
            }
         } // i

         pVar->m_Var.SetList (pl);
         pl->Release();

      }
      return MFC_NONE;

   case VMTOK_CLASSQUERY:       // returns TRUE if it's a subclass of the given class
      {
         // DOCUMENT: Returns TRUE if an object is a member of the given class.
         // the class MUST be a string

         PCMIFLVar pv = plParam->Get(0);
         if (!pv) {
            pVar->m_Var.SetUndefined ();
            return RunTimeErr (gpszNotEnoughParameters, TRUE, dwCharStart, dwCharEnd);
         }
         PCMIFLVarString ps = pv->GetString (this);

         // convert this to a class
         PCMIFLObject pLookFor = m_pCompiled->m_pLib->ObjectGet (m_pCompiled->m_pLib->ObjectFind(ps->Get(), -1));
         ps->Release();

         DWORD i, j;
         CMIFLVar v;
         if (pLookFor) for (i = 0; i < pObject->LayerNum(); i++) {
            PCMIFLVMLayer pLayer = pObject->LayerGet(i);
            PCMIFLObject po = pLayer->m_pObject;

            // set itself as class...
            if (pLookFor == po) {
               pVar->m_Var.SetBOOL (TRUE);
               return MFC_NONE;
            }

            PCMIFLObject *ppo = (PCMIFLObject*)po->m_lClassSuperAllPCMIFLObject.Get(0);
            for (j = 0; j < po->m_lClassSuperAllPCMIFLObject.Num(); j++)
               if (pLookFor == ppo[j]) {
                  pVar->m_Var.SetBOOL (TRUE);
                  return MFC_NONE;
               }
         } // i

         pVar->m_Var.SetBOOL (FALSE);
      }
      return MFC_NONE;

   case VMTOK_CONTAINEDINGET:       // what the object is contained in
      // DOCUMENT: Returns the object that this is contained in
      // No parameters
      // Returns the object, or NULL if not contained
      if (IsEqualGUID(pObject->m_gContainedIn, GUID_NULL))
         pVar->m_Var.SetNULL();
      else
         pVar->m_Var.SetObject (&pObject->m_gContainedIn);
      return MFC_NONE;

   case VMTOK_CONTAINEDINSET:       // change what the object is contained in
      {
         // DOCUMENT: Changes which object contains this
         // New object
         // Returns TRUE if success, FALSE if fail

         if (plParam->Num() != 1) {
            pVar->m_Var.SetBOOL (FALSE);
            return RunTimeErr (gpszNotEnoughParameters, TRUE, dwCharStart, dwCharEnd);
         }

         // based on type
         PCMIFLVar pv = plParam->Get(0);
         GUID g;
         switch (pv->TypeGet()) {
         case MV_OBJECT:
            g = pv->GetGUID();
            break;
         case MV_UNDEFINED:
         case MV_NULL:
            g = GUID_NULL;
            break;
         default:
            pVar->m_Var.SetBOOL (FALSE);
            return RunTimeErr (L"An object can only be contained in another object or NULL.", TRUE, dwCharStart, dwCharEnd);
         } // switch

         // set it
         if (pObject->ContainedBySet (&g))
            pVar->m_Var.SetBOOL (TRUE);
         else {
            pVar->m_Var.SetBOOL (FALSE);
            return RunTimeErr (L"Tried to contain object in a non-existant object or within itself.", FALSE, dwCharStart, dwCharEnd);
         }
      }
      return MFC_NONE;

   case VMTOK_CONTAINSGET:       // returns what it contains
      {
         // DOCUMENT: Returns a list with all the contained objects

         PCMIFLVarList pl = new CMIFLVarList;
         if (!pl) {
            pVar->m_Var.SetUndefined();
            return RunTimeErr (gpszOutOfMemory, TRUE, dwCharStart, dwCharEnd);
         }

         DWORD i;
         GUID *pg = (GUID*) pObject->m_lContains.Get(0);
         CMIFLVar v;
         for (i = 0; i < pObject->m_lContains.Num(); i++, pg++) {
            v.SetObject (pg);
            pl->Add (&v, TRUE);
         } // i

         pVar->m_Var.SetList (pl);
         pl->Release();
      }
      return MFC_NONE;
   } // switch dwID

   // either falls through or method doesn't exist...
   // method doesn't exist, so don't care
   pVar->m_Var.SetUndefined();
   return MFC_NONE;
}

/*****************************************************************************
CMIFLVM::MethodNameToID - Takes a name for a method and converts it to an ID
that can be passed into method calls.

inputs
   PWSTR          pszName - name
returns
   DWORD - Public method ID, or -1 if error
*/
DWORD CMIFLVM::MethodNameToID (PWSTR pszName)
{
   // see if is main method
   PMIFLIDENT pmi = (PMIFLIDENT) m_pCompiled->m_hIdentifiers.Find (pszName, TRUE);
   // BUGFIX - since changed the way custom methods stored
   if (pmi && ((pmi->dwType != MIFLI_METHDEF) && (pmi->dwType != MIFLI_METHPRIV)))
      pmi = NULL;
   if (!pmi)
      pmi = (PMIFLIDENT) m_hIdentifiersCustomMethod.Find (pszName, TRUE);

   if (pmi)
      return pmi->dwID;
   else
      return -1;

}


/*****************************************************************************
CMIFLVM::FunctionNameToID - Takes a function name and converts it to an ID
that can be passed into function calls.

inputs
   PWSTR          pszName - name
returns
   DWORD - Public method ID, or -1 if error
*/
DWORD CMIFLVM::FunctionNameToID (PWSTR pszName)
{
   // see if is main method
   PMIFLIDENT pmi = (PMIFLIDENT) m_pCompiled->m_hIdentifiers.Find (pszName, TRUE);
   // BUGFIX - since changed the way custom methods stored
   if (pmi && (pmi->dwType != MIFLI_FUNC))
      pmi = NULL;

   if (pmi)
      return pmi->dwID;
   else
      return -1;

}



/*****************************************************************************
CMIFLVM::GlobalnNameToID - Takes a function name and converts it to an ID
that can be passed into global calls.

inputs
   PWSTR          pszName - name
returns
   DWORD - Public method ID, or -1 if error
*/
DWORD CMIFLVM::GlobalNameToID (PWSTR pszName)
{
   PMIFLIDENT pmi = (PMIFLIDENT) m_pCompiled->m_hIdentifiers.Find (pszName, TRUE);
   if (pmi &&((pmi->dwType == MIFLI_GLOBAL) || (pmi->dwType ==MIFLI_OBJECT)))
      return pmi->dwID;
   pmi = (PMIFLIDENT) m_hIdentifiersCustomGlobal.Find (pszName, TRUE);
   if (pmi)
      return pmi->dwID;
   else
      return -1;

}


/*****************************************************************************
CMIFLVM::MethodCallVMTOK - Calls a method, but instead of using the normal
ID from m_pCompiled, it uses VMTOK_XXX. Use this to call the constructor
and destructor.

inputs
   GUID              *pgObject - Object ID
   DWORD             dwVMTOK - VMTOK_XXX
   PCMIFLVarList     plParam - List of parameters. The refcount will neither be
                        increased or decreased. This can be NULL
   DWORD             dwCharStart, dwCharEnd - So can report run-time error
   PCMIFLVar         pVar - Filled with the return value.
returns
   DWORD - MFC_XXX, so know what to do when exist. MFC_REPORTERROR if doesnt exist
*/
DWORD CMIFLVM::MethodCallVMTOK (GUID *pgObject, DWORD dwVMTOK, PCMIFLVarList plParam,
                                DWORD dwCharStart, DWORD dwCharEnd, PCMIFLVarLValue pVar)
{
   // clear LValue
   pVar->Clear();

   // get the ID
   DWORD dwID = VMTOKToID (dwVMTOK);
   if (dwID == -1) {
      pVar->m_Var.SetUndefined();
      return MFC_NONE;
   }

   BOOL fDelVar;
   if (!plParam) {
      fDelVar = TRUE;
      plParam = new CMIFLVarList;
      if (!plParam)
         return MFC_REPORTERROR;
   }
   else
      fDelVar = FALSE;

   DWORD dwRet = MethodCall (pgObject, dwID, plParam, dwCharStart, dwCharEnd, pVar);

   if (fDelVar)
      plParam->Release();
   
   return dwRet;
}

/*****************************************************************************
CMIFLVM::RunTimeCodeCall - This takes a line of code, compiles it on the
spot, and runs it.

inputs
   PWSTR             pszCode - Code
   GUID              *pgObject - Object state that in (for running code within object).
                        GUID_NULL if global
   PCMIFLVar         pVar - Filled with the return value.
returns
   DWORD - MFC_XXX, so know what to do when exits. MFC_REPORTERROR if code doesn't compile
*/
DWORD CMIFLVM::RunTimeCodeCall (PWSTR pszCode, GUID *pgObject, PCMIFLVarLValue pVar)
{
   // get the object
   PCMIFLVMObject pObject = NULL;
   PCMIFLObject pObjectLayer = NULL;
   if (pgObject && !IsEqualGUID (*pgObject, GUID_NULL))
      pObject = ObjectFind (pgObject);

   if (pObject) {
      // NOTE: Semi-hack layer calculation, but user won't easily be able to
      // state which layer
      //if (pObject->m_pObject)
      //   pObjectLayer = pObject->m_pObject;
      //else {
         // get the last layer
         PCMIFLVMLayer pLayer = pObject->LayerGet(pObject->LayerNum()-1);
         if (pLayer)
            pObjectLayer = pLayer->m_pObject;
         else
            return MFC_REPORTERROR; // shouldnt happen
      //}
   }


   // compile this...
   CMIFLErrors err;
   CMem memCodeComp;
   m_pCompiled->CompileCode (&err, NULL, NULL, FALSE, pszCode, TRUE, &memCodeComp,
      pObjectLayer, L"err", L"err");
   if (err.m_dwNumError)
      return MFC_REPORTERROR;

   MIFLFCI fci;
   fci.pszCode = pszCode;
   fci.pMemMIFLCOMP = &memCodeComp;
   fci.dwParamCount = 0;
   fci.plParam = NULL;
   fci.phVarsString = NULL;
   fci.pObject = pObject;
   fci.pObjectLayer = pObjectLayer;
   fci.pszWhere1 = gpszTypedCode;
   fci.pszWhere2 = NULL;
   fci.dwPropIDGetSet = -1;

   // call
   return FuncCallInternal (&fci, pVar);
}


/*****************************************************************************
CMIFLVM::ParseArguments - This takes a block to the right of arg2 (following
paren) and produces a CMIFLVarList with all the arguments.

It uses the m_pFCICur for the pointer to the code.

inputs
   DWORD          dwIndex - Index for the argument to the right of arg2
   PCMIFLVarList  *ppList - Filled in with a new PCMIFLVarList that must be
                  freed by the caller.
returns
   DWORD - MFC_XXX
*/
DWORD CMIFLVM::ParseArguments (DWORD dwIndex, PCMIFLVarList *ppList)
{
   PCMIFLVarList pl;
   *ppList = pl = new CMIFLVarList;
   if (!pl)
      return MFC_ABORTFUNC;

   if (!dwIndex)
      return MFC_NONE;     // assume that empty list

   PMIFLCOMP pc = (PMIFLCOMP)(m_pFCICur->pbMIFLCOMP + dwIndex);

#ifdef _DEBUG
   if (pc->dwNext)
      return RunTimeErr (L"Unexpected token in parameters", TRUE, pc->wCharStart, pc->wCharEnd);
#endif

   CMIFLVarLValue var;
   DWORD dwRet;
   if (pc->dwType != TOKEN_OPER_COMMA) {
      // just one parameter
      dwRet = ExecuteCodeStatement (dwIndex, &var);
      pl->Add (&var.m_Var, TRUE);
      return dwRet;
   };

   // go down
   while (pc->dwDown) {
      pc = (PMIFLCOMP)(PMIFLCOMP)(m_pFCICur->pbMIFLCOMP + pc->dwDown);
      if (!pc->dwNext) {
         // make it a null param
         // NOTE: Not tested with new changes to CMIFLVarLValue
         var.m_Var.SetUndefined();
         pl->Add (&var.m_Var, TRUE);
         continue;
      }

#ifdef _DEBUG
      PMIFLCOMP pc2;
      pc2 =  (PMIFLCOMP)(PMIFLCOMP)(m_pFCICur->pbMIFLCOMP + pc->dwNext);
      if (pc2->dwNext)
         return RunTimeErr (L"Unexpected token in parameters", TRUE, pc->wCharStart, pc->wCharEnd);
#endif
      dwRet = ExecuteCodeStatement (pc->dwNext, &var);
      pl->Add (&var.m_Var, TRUE);
      if (dwRet)
         return dwRet;
   } // while pc->dwDown

   return MFC_NONE;
}


/*****************************************************************************
CMIFLVM::ExecuteCodeStatement - This internal function takes compiled code and
runs it. This only runs the code from the current index; it DOESN'T keep
running through all next values.

It uses the m_pFCICur for the pointer to the code.

inputs
   DWORD          dwIndex - Index to use into m_pFCICur->pMemMIFLCOMP
   PCMIFLVar      pVar - Should be initialized to something, but will be modified by
                  this function and filled in

returns
   DWORD - MFC_XXX
*/
DWORD CMIFLVM::ExecuteCodeStatement (DWORD dwIndex, PCMIFLVarLValue pVar)
{
   // make sure LValue cleared
   pVar->Clear();

   PMIFLCOMP pc = (PMIFLCOMP)(m_pFCICur->pbMIFLCOMP + dwIndex);
   DWORD dwRet;

   switch (pc->dwType) {
   case TOKEN_DOUBLE:
      {
         double *pf = (double*)(m_pFCICur->pbMIFLCOMP + pc->dwValue);
         pVar->m_Var.SetDouble (*pf);
      }
      return MFC_NONE;

   case TOKEN_CHAR:
      pVar->m_Var.SetChar ((WCHAR)pc->dwValue);
      return MFC_NONE;

   case TOKEN_STRING:
      {
         PWSTR psz = (PWSTR)(m_pFCICur->pbMIFLCOMP + pc->dwValue);
         pVar->m_Var.SetString (psz, (DWORD)-1);
      }
      return MFC_NONE;

   case TOKEN_NULL:
      pVar->m_Var.SetNULL ();
      return MFC_NONE;

   case TOKEN_NOP:
   case TOKEN_UNDEFINED:
      pVar->m_Var.SetUndefined ();
      return MFC_NONE;

   case TOKEN_BOOL:
      pVar->m_Var.SetBOOL (pc->dwValue);
      return MFC_NONE;

   case TOKEN_STRINGTABLE:
      pVar->m_Var.SetStringTable (pc->dwValue);
      return MFC_NONE;

   case TOKEN_RESOURCE:
      pVar->m_Var.SetResource (pc->dwValue);
      return MFC_NONE;

   case TOKEN_METHPRIV:
      pVar->m_Var.SetMeth (pc->dwValue);
      return MFC_NONE;

   case TOKEN_METHPUB:
      pVar->m_Var.SetMeth (pc->dwValue);
      return MFC_NONE;

   case TOKEN_FUNCTION:
      pVar->m_Var.SetFunc (pc->dwValue);
      return MFC_NONE;


   case TOKEN_LISTDEF:
      {
         // create a new list
         PCMIFLVarList pList = new CMIFLVarList;
         if (!pList) {
            pVar->m_Var.SetUndefined();
            return RunTimeErr (L"Out of memory when create list.", TRUE, pc->wCharStart, pc->wCharEnd);
         }
         pVar->m_Var.SetList (pList);
         pList->Release(); // since pVar storing... saves from having to release later

         if (!pc->dwDown)
            return MFC_NONE;  // empty list

         PMIFLCOMP pDown;
         DWORD dwLoc = pc->dwDown;
         CMIFLVarLValue var;
         pDown = (PMIFLCOMP)(m_pFCICur->pbMIFLCOMP + dwLoc);

         if ((pDown->dwType == TOKEN_OPER_COMMA) && (pDown->dwDown)) {
            pDown = (PMIFLCOMP)(m_pFCICur->pbMIFLCOMP + pDown->dwDown);
            while (TRUE) {
               if (pDown->dwType == TOKEN_ARG1) {
                  if (pDown->dwNext) {
                     dwRet = ExecuteCodeStatement (pDown->dwNext, &var);
                     if (dwRet)
                        return dwRet;
                  }
                  else
                     var.m_Var.SetUndefined();
               }
               
               // add this to the list
               pList->Add (&var.m_Var, TRUE);

               // move next
               if (!pDown->dwDown)
                  break;
               pDown = (PMIFLCOMP)(m_pFCICur->pbMIFLCOMP + pDown->dwDown);

            } // while TRUE
         } // comma
         else if (pDown->dwType != TOKEN_OPER_COMMA) {
            // one element by itself
            dwRet =  ExecuteCodeStatement (dwLoc, &var);
            if (dwRet)
               return dwRet;
            // add this to the list
            pList->Add (&var.m_Var, TRUE);
         }
      }
      return MFC_NONE;

   case TOKEN_PROPPUB:
   case TOKEN_PROPPRIV:
      {
         if (!m_pFCICur->pObject) {
            return RunTimeErr (
               L"Accessed an object property even though not in an object.",
               TRUE,
               pc->wCharStart, pc->wCharEnd);
         }

         // also doing check for get/set - if currently in code to get/set
         return PropertyGet (pc->dwValue, m_pFCICur->pObject,
            (m_pFCICur->pObject==m_pFCICur->pObject) && (m_pFCICur->dwPropIDGetSet == pc->dwValue), pVar);
      }
      return MFC_NONE;

   case TOKEN_GLOBAL:
      // also doing check for get/set - if currently in code to get/set
      return GlobalGet (pc->dwValue, !m_pFCICur->pObject && (m_pFCICur->dwPropIDGetSet == pc->dwValue), pVar);

   case TOKEN_VARIABLE:
      {
         if (pc->dwValue >= m_pFCICur->dwVarsNum)
            return RunTimeErr (L"Unexpected variable access.", TRUE, pc->wCharStart, pc->wCharEnd);

         pVar->m_Var.Set (m_pFCICur->paVars + pc->dwValue);

         pVar->m_dwLValue = MLV_VARIABLE;
         pVar->m_pLValue = m_pFCICur->paVars + pc->dwValue;
      }
      return MFC_NONE;


   case TOKEN_LISTINDEX:
      {
         PMIFLCOMP pcArg1 = pc->dwDown ? (PMIFLCOMP)(m_pFCICur->pbMIFLCOMP + pc->dwDown) : NULL;
         PMIFLCOMP pcArg2 = (pcArg1 && pcArg1->dwDown) ? (PMIFLCOMP)(m_pFCICur->pbMIFLCOMP + pcArg1->dwDown) : NULL;
         if (!pcArg1 || !pcArg2 || !pcArg1->dwNext || !pcArg2->dwNext)
            return RunTimeErr (L"Error is list indexing.", TRUE, pc->wCharStart, pc->wCharEnd);

         // evaluate the list...
         CMIFLVarLValue vList, vIndex;
         dwRet = ExecuteCodeStatement (pcArg1->dwNext, &vList);
         if (dwRet)
            return dwRet;

         // evaluate the index
         dwRet = ExecuteCodeStatement (pcArg2->dwNext, &vIndex);
         if (dwRet)
            return dwRet;
         DWORD dwIndex = (DWORD) vIndex.m_Var.GetDouble(this);

         // DOCUMENT: Can access array into string or list
         DWORD dwType = vList.m_Var.TypeGet();
         switch (dwType) {
         case MV_LIST:
            {
               // DOCUMENT: If access beyond end of list then get undefined
               PCMIFLVarList pl = vList.m_Var.GetList();
               PCMIFLVar pFind = pl->Get(dwIndex);
               if (pFind)
                  pVar->m_Var.Set (pFind);
               else
                  pVar->m_Var.SetUndefined();

               pVar->m_dwLValue = MLV_LISTINDEX;
               pVar->m_dwLValueID = dwIndex;
               pVar->m_pLValue = pl;

               pl->Release();
            }
            break;
         case MV_STRING:
         case MV_STRINGTABLE: // BUGFIX - ALlow stirng table array
            {
               // DOCUMENT: If access beyond end of string then get undefined
               PCMIFLVarString ps = vList.m_Var.GetString(this);
               PWSTR pszFind = ps->CharGet(dwIndex);
               if (pszFind)
                  pVar->m_Var.SetChar (pszFind[0]);
               else
                  pVar->m_Var.SetUndefined();
               ps->Release();

               if (dwType == MV_STRING) {
                  pVar->m_dwLValue = MLV_STRINGINDEX;
                  pVar->m_dwLValueID = dwIndex;
                  pVar->m_pLValue = ps;
               }

            }
            break;

         default:
            // NOTE - if can ever array into object then will need to change this
            pVar->m_Var.SetUndefined();
            return RunTimeErr (L"Trying to index into something that is not a list or string.",
               TRUE, pc->wCharStart, pc->wCharEnd);
         }
      }
      return MFC_NONE;

   case TOKEN_OPER_DOT:
      {
         PMIFLCOMP pcArg1 = pc->dwDown ? (PMIFLCOMP)(m_pFCICur->pbMIFLCOMP + pc->dwDown) : NULL;
         PMIFLCOMP pcArg2 = (pcArg1 && pcArg1->dwDown) ? (PMIFLCOMP)(m_pFCICur->pbMIFLCOMP + pcArg1->dwDown) : NULL;
         if (!pcArg1 || !pcArg2 || !pcArg1->dwNext || !pcArg2->dwNext)
            return RunTimeErr (L"Error is object dereferencing.", TRUE, pc->wCharStart, pc->wCharEnd);

         // evaluate the list...
         CMIFLVarLValue vObj, vIndex;
         dwRet = ExecuteCodeStatement (pcArg1->dwNext, &vObj);
         if (dwRet)
            return dwRet;

         // if it's not an object then fail
         DWORD dwLeft = vObj.m_Var.TypeGet();
         switch (dwLeft) {
         case MV_OBJECT:
         case MV_STRING:
         case MV_LIST:
         case MV_STRINGTABLE:
            break;
         default:
            return RunTimeErr (L"Object, string, or list expected to left of '.'", TRUE, pc->wCharStart, pc->wCharEnd);
         }

         // get the object
         GUID gObj = vObj.m_Var.GetGUID ();
         PCMIFLVMObject pNewObject = ObjectFind (&gObj);

         PMIFLCOMP pcArg2n = (PMIFLCOMP)(m_pFCICur->pbMIFLCOMP + pcArg2->dwNext);
         if ((pcArg2n->dwType == TOKEN_PROPPUB) || (pcArg2n->dwType == TOKEN_PROPPRIV)) {
            // if it's not an object then error
            if (dwLeft != MV_OBJECT)
               return RunTimeErr (L"Object to left of a property.", TRUE, pc->wCharStart, pc->wCharEnd);

            // BUGFIX - if object not found then error
            if (!pNewObject)
               return RunTimeErr (L"Object does not exist.", TRUE, pc->wCharStart, pc->wCharEnd);

            // also passing in ignore in case getting/setting within own function
            dwRet = PropertyGet (pcArg2n->dwValue, pNewObject,
               (m_pFCICur->pObject==pNewObject) && (m_pFCICur->dwPropIDGetSet == pcArg2n->dwValue), pVar);

            pVar->m_dwLValue = MLV_PROPERTY;
            pVar->m_dwLValueID = pcArg2n->dwValue;
            pVar->m_pLValue = pNewObject;
            return dwRet;
         }

         // otherwise, might be a method, so just access it...
         // NOTE: Specifically using existing object space when accessing, NOT pNewObject
         dwRet = ExecuteCodeStatement (pcArg2->dwNext, &vIndex);
         if (dwRet)
            return dwRet;

         switch (vIndex.m_Var.TypeGet()) {
         case MV_METH:
            switch (dwLeft) {
            case MV_OBJECT:
               pVar->m_Var.SetObjectMeth (&gObj, vIndex.m_Var.GetValue());
               break;
            case MV_STRING:
            case MV_STRINGTABLE:
               // DOCUMENT: Can also reference string table
               {
                  PCMIFLVarString ps = vObj.m_Var.GetString(this);
                  pVar->m_Var.SetStringMeth (ps, vIndex.m_Var.GetValue());
                  ps->Release();
               }
               break;
            case MV_LIST:
               {
                  PCMIFLVarList ps = vObj.m_Var.GetList();
                  pVar->m_Var.SetListMeth (ps, vIndex.m_Var.GetValue());
                  ps->Release();
               }
               break;
            }
            return MFC_NONE;
         default:
            // improper access
            return RunTimeErr (L"Method or propertey expected to the right of '.'", TRUE, pc->wCharStart, pc->wCharEnd);
         }
      }
      return MFC_NONE;  // shouldnt get called

   case TOKEN_FUNCCALL:
      {
         if (m_pFCICur->fOnlyVarAccess)
            return RunTimeErr (gpszOnlyVar, TRUE, pc->wCharStart, pc->wCharEnd);

         PMIFLCOMP pcArg1 = pc->dwDown ? (PMIFLCOMP)(m_pFCICur->pbMIFLCOMP + pc->dwDown) : NULL;
         PMIFLCOMP pcArg2 = (pcArg1 && pcArg1->dwDown) ? (PMIFLCOMP)(m_pFCICur->pbMIFLCOMP + pcArg1->dwDown) : NULL;
         if (!pcArg1 || !pcArg2 || !pcArg1->dwNext /*|| !pcArg2->dwNext*/) {
            pVar->m_Var.SetUndefined();
            return RunTimeErr (L"Error in function call.", TRUE, pc->wCharStart, pc->wCharEnd);
         }

         // evaluate the list...
         CMIFLVarLValue vCaller;
         dwRet = ExecuteCodeStatement (pcArg1->dwNext, &vCaller);
         if (dwRet) {
            pVar->m_Var.SetUndefined();
            return dwRet;
         }

         // get the parameters
         PCMIFLVarList pList;
         dwRet = ParseArguments (pcArg2->dwNext, &pList);
         if (dwRet) {
            pVar->m_Var.SetUndefined();
            pList->Release();
            return dwRet;
         }

         // call into function or method
         switch (vCaller.m_Var.TypeGet()) {
         case MV_METH:
            if (!m_pFCICur->pObject) {
               pList->Release();
               pVar->m_Var.SetUndefined();
               return RunTimeErr (L"You cannot call a method from within a function.", TRUE,
                  pcArg1->wCharStart, pcArg1->wCharEnd);
            }

            dwRet = MethodCall (&m_pFCICur->pObject->m_gID, vCaller.m_Var.GetValue(),
               pList, pc->wCharStart, pc->wCharEnd, pVar);
            break;

         case MV_OBJECTMETH:
            {
               GUID g = vCaller.m_Var.GetGUID ();
               dwRet = MethodCall (&g, vCaller.m_Var.GetValue(), pList,
                  pc->wCharStart, pc->wCharEnd, pVar);
            }
            break;

         case MV_FUNC:
            dwRet = FunctionCall (vCaller.m_Var.GetValue(), pList, pVar);
            break;

         case MV_STRINGMETH:
            {
               PCMIFLVarString ps = vCaller.m_Var.GetStringNoMod();
               dwRet = StringMethodCall (ps, vCaller.m_Var.GetValue(), pList,
                  pcArg1->wCharStart, pcArg2->wCharEnd, pVar);
               ps->Release();
            }
            break;

         case MV_LISTMETH:
            {
               PCMIFLVarList ps = vCaller.m_Var.GetList();
               dwRet = ListMethodCall (ps, vCaller.m_Var.GetValue(), pList,
                  pcArg1->wCharStart, pcArg2->wCharEnd, pVar);
               ps->Release();
            }
            break;

         default:
            pList->Release(); // free up
            pVar->m_Var.SetUndefined();
            return RunTimeErr (L"Function or method name expected.", TRUE,
               pcArg1->wCharStart, pcArg1->wCharEnd);
         }

         // called funciton or method
         if (dwRet == MFC_REPORTERROR)
            dwRet = RunTimeErr (L"Unknown function or method.", TRUE, pcArg1->wCharStart, pcArg2->wCharEnd);

         pList->Release(); // free up
         return dwRet;
      }
      return MFC_NONE;

   case TOKEN_DEBUG:
      return RunTimeErr (L"Debug mode invoked by the application.", FALSE, pc->wCharStart, pc->wCharEnd);

   case TOKEN_RETURN:
      {
         PMIFLCOMP pcArg1 = pc->dwDown ? (PMIFLCOMP)(m_pFCICur->pbMIFLCOMP + pc->dwDown) : NULL;
         if (!pcArg1 || !pcArg1->dwNext) {
            pVar->m_Var.SetUndefined();
            return MFC_RETURN;
         }

         dwRet = ExecuteCodeStatement (pcArg1->dwNext, pVar);
         if (dwRet >= MFC_REPORTERROR)
            return dwRet;
      }
      return MFC_RETURN;

   case TOKEN_DELETE:
      {
         if (m_pFCICur->fOnlyVarAccess) {
            pVar->m_Var.SetUndefined();
            return RunTimeErr (gpszOnlyVar, TRUE, pc->wCharStart, pc->wCharEnd);
         }

         CMIFLVarLValue var;
         // clear
         pVar->m_Var.SetUndefined();

         PMIFLCOMP pcArg1 = pc->dwDown ? (PMIFLCOMP)(m_pFCICur->pbMIFLCOMP + pc->dwDown) : NULL;
         if (!pcArg1 || !pcArg1->dwNext)
            return RunTimeErr (L"Object must be specified.", TRUE, pc->wCharStart, pc->wCharEnd);

         dwRet = ExecuteCodeStatement (pcArg1->dwNext, &var);
         if (dwRet)
            return dwRet;

         // make sure an object
         if (var.m_Var.TypeGet() != MV_OBJECT)
            return RunTimeErr (L"Only objects can be deleted.", TRUE, pcArg1->wCharStart, pcArg1->wCharEnd);

         // delete it
         GUID g = var.m_Var.GetGUID();
         if (!ObjectDelete (&g))
            return RunTimeErr (L"Object doesn't exist.", FALSE, pcArg1->wCharStart, pcArg1->wCharEnd);
      }
      return MFC_NONE;

   case TOKEN_OPER_NEW:
      {
         if (m_pFCICur->fOnlyVarAccess) {
            pVar->m_Var.SetUndefined();
            return RunTimeErr (gpszOnlyVar, TRUE, pc->wCharStart, pc->wCharEnd);
         }

         PMIFLCOMP pcArg1 = pc->dwDown ? (PMIFLCOMP)(m_pFCICur->pbMIFLCOMP + pc->dwDown) : NULL;
         pcArg1 = (pcArg1 && pcArg1->dwNext) ? (PMIFLCOMP)(m_pFCICur->pbMIFLCOMP + pcArg1->dwNext) : NULL;
         if (!pcArg1 || (pcArg1->dwType != TOKEN_CLASS)) {
            pVar->m_Var.SetUndefined();
            return RunTimeErr (L"A class must be specified.", TRUE, pc->wCharStart, pc->wCharEnd);
         }

         // get the object...
         PCMIFLObject pObject = m_pCompiled->m_pLib->ObjectGet (pcArg1->dwValue);
         if (!pObject) {
            pVar->m_Var.SetUndefined();
            return RunTimeErr (L"Unknown class.", TRUE, pc->wCharStart, pc->wCharEnd);
         }

         // create
         PCMIFLVMObject pNew = new CMIFLVMObject;
         if (!pNew) {
            pVar->m_Var.SetUndefined();
            return RunTimeErr (gpszOutOfMemory, TRUE, pc->wCharStart, pc->wCharEnd);
         }
         if (!pNew->InitAsNew (this, pObject)) {
            pVar->m_Var.SetUndefined();
            return RunTimeErr (gpszOutOfMemory, TRUE, pc->wCharStart, pc->wCharEnd);
            delete pNew;
            return FALSE;
         }

         // add this
         m_hObjects.Add (&pNew->m_gID, &pNew);

         // call constructor
         dwRet = MethodCallVMTOK (&pNew->m_gID, VMTOK_CONSTRUCTOR, NULL,
            pc->wCharStart, pc->wCharEnd, pVar);

         pVar->m_Var.SetObject (&pNew->m_gID);

         return dwRet;
      }
      return MFC_NONE;

   case TOKEN_OPER_NEGATION:
      if (dwRet = ParseOperatorUnary (pc, NULL, pVar))
         return dwRet;
      pVar->m_Var.OperNegation(this);
      return MFC_NONE;

   case TOKEN_OPER_BITWISENOT:
      if (dwRet = ParseOperatorUnary (pc, NULL, pVar))
         return dwRet;
      pVar->m_Var.OperBitwiseNot(this);
      return MFC_NONE;

   case TOKEN_OPER_LOGICALNOT:
      if (dwRet = ParseOperatorUnary (pc, NULL, pVar))
         return dwRet;
      pVar->m_Var.OperLogicalNot(this);
      return MFC_NONE;

   case TOKEN_OPER_MULTIPLY:
      {
         CMIFLVarLValue varL, varR;
         if (dwRet = ParseOperatorBinary (FALSE, pc, &varL, &varR)) {
            pVar->m_Var.SetUndefined();
            return dwRet;
         }
         pVar->m_Var.OperMultiply (this, &varL.m_Var, &varR.m_Var);
      }
      return MFC_NONE;

   case TOKEN_OPER_DIVIDE:
      {
         CMIFLVarLValue varL, varR;
         if (dwRet = ParseOperatorBinary (FALSE, pc, &varL, &varR)) {
            pVar->m_Var.SetUndefined();
            return dwRet;
         }
         pVar->m_Var.OperDivide (this, &varL.m_Var, &varR.m_Var);
      }
      return MFC_NONE;

   case TOKEN_OPER_MODULO:
      {
         CMIFLVarLValue varL, varR;
         if (dwRet = ParseOperatorBinary (FALSE, pc, &varL, &varR)) {
            pVar->m_Var.SetUndefined();
            return dwRet;
         }
         pVar->m_Var.OperModulo (this, &varL.m_Var, &varR.m_Var);
      }
      return MFC_NONE;

   case TOKEN_OPER_ADD:
      {
         CMIFLVarLValue varL, varR;
         if (dwRet = ParseOperatorBinary (FALSE, pc, &varL, &varR)) {
            pVar->m_Var.SetUndefined();
            return dwRet;
         }
         pVar->m_Var.OperAdd (this, &varL.m_Var, &varR.m_Var);
      }
      return MFC_NONE;

   case TOKEN_OPER_SUBTRACT:
      {
         CMIFLVarLValue varL, varR;
         if (dwRet = ParseOperatorBinary (FALSE, pc, &varL, &varR)) {
            pVar->m_Var.SetUndefined();
            return dwRet;
         }
         pVar->m_Var.OperSubtract (this, &varL.m_Var, &varR.m_Var);
      }
      return MFC_NONE;

   case TOKEN_OPER_BITWISELEFT:
      {
         CMIFLVarLValue varL, varR;
         if (dwRet = ParseOperatorBinary (FALSE, pc, &varL, &varR)) {
            pVar->m_Var.SetUndefined();
            return dwRet;
         }
         pVar->m_Var.OperBitwiseLeft (this, &varL.m_Var, &varR.m_Var);
      }
      return MFC_NONE;

   case TOKEN_OPER_BITWISERIGHT:
      {
         CMIFLVarLValue varL, varR;
         if (dwRet = ParseOperatorBinary (FALSE, pc, &varL, &varR)) {
            pVar->m_Var.SetUndefined();
            return dwRet;
         }
         pVar->m_Var.OperBitwiseRight (this, &varL.m_Var, &varR.m_Var);
      }
      return MFC_NONE;

   case TOKEN_OPER_BITWISEAND:
      {
         CMIFLVarLValue varL, varR;
         if (dwRet = ParseOperatorBinary (FALSE, pc, &varL, &varR)) {
            pVar->m_Var.SetUndefined();
            return dwRet;
         }
         pVar->m_Var.OperBitwiseAnd (this, &varL.m_Var, &varR.m_Var);
      }
      return MFC_NONE;

   case TOKEN_OPER_BITWISEXOR:
      {
         CMIFLVarLValue varL, varR;
         if (dwRet = ParseOperatorBinary (FALSE, pc, &varL, &varR)) {
            pVar->m_Var.SetUndefined();
            return dwRet;
         }
         pVar->m_Var.OperBitwiseXOr (this, &varL.m_Var, &varR.m_Var);
      }
      return MFC_NONE;

   case TOKEN_OPER_BITWISEOR:
      {
         CMIFLVarLValue varL, varR;
         if (dwRet = ParseOperatorBinary (FALSE, pc, &varL, &varR)) {
            pVar->m_Var.SetUndefined();
            return dwRet;
         }
         pVar->m_Var.OperBitwiseOr (this, &varL.m_Var, &varR.m_Var);
      }
      return MFC_NONE;

   case TOKEN_OPER_LOGICALAND:
      {
         CMIFLVarLValue varL, varR;
         // BUGFIX - Call special case so if left arg is FALSE then don't run the right
         if (dwRet = ParseOperatorBinary (FALSE, pc, &varL, &varR, 2)) {
            pVar->m_Var.SetUndefined();
            return dwRet;
         }
         pVar->m_Var.OperLogicalAnd (this, &varL.m_Var, &varR.m_Var);
      }
      return MFC_NONE;

   case TOKEN_OPER_LOGICALOR:
      {
         CMIFLVarLValue varL, varR;
         // BUGFIX - Call special case so if left arg is TRUE then don't run the right
         if (dwRet = ParseOperatorBinary (FALSE, pc, &varL, &varR, 1)) {
            pVar->m_Var.SetUndefined();
            return dwRet;
         }
         pVar->m_Var.OperLogicalOr (this, &varL.m_Var, &varR.m_Var);
      }
      return MFC_NONE;

      // DOCUMENT: In comparison functions, if comparing oranges to apples returns undefined
      // DOCUMENT: Although, if using == or === will always be true or false
   case TOKEN_OPER_LESSTHAN:
      {
         CMIFLVarLValue varL, varR;
         if (dwRet = ParseOperatorBinary (FALSE, pc, &varL, &varR)) {
            pVar->m_Var.SetUndefined();
            return dwRet;
         }
         switch (varL.m_Var.Compare (&varR.m_Var, FALSE, this)) {
         case -1:
            pVar->m_Var.SetBOOL (TRUE);
            break;
         case 0:
         case 1:
            pVar->m_Var.SetBOOL (FALSE);
            break;

         default: // cant really compare
            pVar->m_Var.SetUndefined();
         }
      }
      return MFC_NONE;

   case TOKEN_OPER_LESSTHANEQUAL:
      {
         CMIFLVarLValue varL, varR;
         if (dwRet = ParseOperatorBinary (FALSE, pc, &varL, &varR)) {
            pVar->m_Var.SetUndefined();
            return dwRet;
         }
         switch (varL.m_Var.Compare (&varR.m_Var, FALSE, this)) {
         case -1:
         case 0:
            pVar->m_Var.SetBOOL (TRUE);
            break;
         case 1:
            pVar->m_Var.SetBOOL (FALSE);
            break;

         default: // cant really compare
            pVar->m_Var.SetUndefined();
         }
      }
      return MFC_NONE;

   case TOKEN_OPER_GREATERTHAN:
      {
         CMIFLVarLValue varL, varR;
         if (dwRet = ParseOperatorBinary (FALSE, pc, &varL, &varR)) {
            pVar->m_Var.SetUndefined();
            return dwRet;
         }
         switch (varL.m_Var.Compare (&varR.m_Var, FALSE, this)) {
         case 1:
            pVar->m_Var.SetBOOL (TRUE);
            break;
         case 0:
         case -1:
            pVar->m_Var.SetBOOL (FALSE);
            break;

         default: // cant really compare
            pVar->m_Var.SetUndefined();
         }
      }
      return MFC_NONE;

   case TOKEN_OPER_GREATERTHANEQUAL:
      {
         CMIFLVarLValue varL, varR;
         if (dwRet = ParseOperatorBinary (FALSE, pc, &varL, &varR)) {
            pVar->m_Var.SetUndefined();
            return dwRet;
         }
         switch (varL.m_Var.Compare (&varR.m_Var, FALSE, this)) {
         case 1:
         case 0:
            pVar->m_Var.SetBOOL (TRUE);
            break;
         case -1:
            pVar->m_Var.SetBOOL (FALSE);
            break;

         default: // cant really compare
            pVar->m_Var.SetUndefined();
         }
      }
      return MFC_NONE;

   case TOKEN_OPER_EQUALITY:
      {
         CMIFLVarLValue varL, varR;
         if (dwRet = ParseOperatorBinary (FALSE, pc, &varL, &varR)) {
            pVar->m_Var.SetUndefined();
            return dwRet;
         }
         pVar->m_Var.SetBOOL (varL.m_Var.Compare (&varR.m_Var, FALSE, this) ? FALSE : TRUE);
      }
      return MFC_NONE;

   case TOKEN_OPER_NOTEQUAL:
      {
         CMIFLVarLValue varL, varR;
         if (dwRet = ParseOperatorBinary (FALSE, pc, &varL, &varR)) {
            pVar->m_Var.SetUndefined();
            return dwRet;
         }
         pVar->m_Var.SetBOOL (varL.m_Var.Compare (&varR.m_Var, FALSE, this) ? TRUE : FALSE);
      }
      return MFC_NONE;

   case TOKEN_OPER_EQUALITYSTRICT:
      {
         // DOCUMENT: Equality, strict
         CMIFLVarLValue varL, varR;
         if (dwRet = ParseOperatorBinary (FALSE, pc, &varL, &varR)) {
            pVar->m_Var.SetUndefined();
            return dwRet;
         }
         pVar->m_Var.SetBOOL (varL.m_Var.Compare (&varR.m_Var, TRUE, this) ? FALSE : TRUE);
      }
      return MFC_NONE;

   case TOKEN_OPER_NOTEQUALSTRICT:
      {
         // DOCUMENT: Not-equality, strict
         CMIFLVarLValue varL, varR;
         if (dwRet = ParseOperatorBinary (FALSE, pc, &varL, &varR)) {
            pVar->m_Var.SetUndefined();
            return dwRet;
         }
         pVar->m_Var.SetBOOL (varL.m_Var.Compare (&varR.m_Var, TRUE, this) ? TRUE : FALSE);
      }
      return MFC_NONE;

   case TOKEN_OPER_CONDITIONAL:
      {
         PMIFLCOMP pcArg1 = pc->dwDown ? (PMIFLCOMP)(m_pFCICur->pbMIFLCOMP + pc->dwDown) : NULL;
         PMIFLCOMP pcArg2 = (pcArg1 && pcArg1->dwDown) ? (PMIFLCOMP)(m_pFCICur->pbMIFLCOMP + pcArg1->dwDown) : NULL;
         PMIFLCOMP pcArg3 = (pcArg2 && pcArg2->dwDown) ? (PMIFLCOMP)(m_pFCICur->pbMIFLCOMP + pcArg2->dwDown) : NULL;

         DWORD dwRet;
         if (!pcArg1 || !pcArg1->dwNext || !pcArg2 || !pcArg2->dwNext || !pcArg3 || !pcArg3->dwNext) {
            pVar->m_Var.SetUndefined();
            return RunTimeErr (L"Three operations for contional expected.", TRUE, pc->wCharStart, pc->wCharEnd);
         }
         dwRet = ExecuteCodeStatement (pcArg1->dwNext, pVar);
         if (dwRet)
            pVar->m_Var.SetUndefined();   // since pVar will be set to undefined

         // if it's true then run arg1, else run arg2
         return ExecuteCodeStatement (pVar->m_Var.GetBOOL(this) ? pcArg2->dwNext : pcArg3->dwNext, pVar);
      }
      return MFC_NONE;

   case TOKEN_OPER_PLUSPLUS:
   case TOKEN_OPER_MINUSMINUS:
      {
         // DOCUMENT: ++, --
         DWORD dwType;
         dwRet = ParseOperatorUnary (pc, &dwType, pVar);

         // if increment
         CMIFLVarLValue vInc;
         CMIFLVar vOne;
         vOne.SetDouble ((pc->dwType == TOKEN_OPER_PLUSPLUS) ? 1 : -1);
         vInc.m_Var.OperAdd (this, &pVar->m_Var, &vOne);

         // set the LValue
         dwRet = LValueSet (pVar, &vInc, pc);
         if (dwRet)
            return dwRet;

         // copy over pVar, depending upon the lParam
         if (dwType == TOKEN_ARG2)
            pVar->m_Var.Set (&vInc.m_Var);
      }
      return MFC_NONE;

   case TOKEN_OPER_ASSIGN:
      {
         // DOCUMENT: assign
         CMIFLVarLValue varL;
         if (dwRet = ParseOperatorBinary (TRUE, pc, &varL, pVar)) {
            pVar->m_Var.SetUndefined();
            return dwRet;
         }

         return LValueSet (&varL, pVar, pc);
      }
      return MFC_NONE;

   case TOKEN_OPER_ASSIGNADD:
      {
         // DOCUMENT: Assign add
         CMIFLVarLValue varL, varR;
         if (dwRet = ParseOperatorBinary (TRUE, pc, &varL, &varR)) {
            pVar->m_Var.SetUndefined();
            return dwRet;
         }
         pVar->m_Var.OperAdd (this, &varL.m_Var, &varR.m_Var);
         return LValueSet (&varL, pVar, pc);
      }
      return MFC_NONE;

   case TOKEN_OPER_ASSIGNSUBTRACT:
      {
         // DOCUMENT: Assign subtract
         CMIFLVarLValue varL, varR;
         if (dwRet = ParseOperatorBinary (TRUE, pc, &varL, &varR)) {
            pVar->m_Var.SetUndefined();
            return dwRet;
         }
         pVar->m_Var.OperSubtract (this, &varL.m_Var, &varR.m_Var);
         return LValueSet (&varL, pVar, pc);
      }
      return MFC_NONE;

   case TOKEN_OPER_ASSIGNMULTIPLY:
      {
         // DOCUMENT: Assign multiply
         CMIFLVarLValue varL, varR;
         if (dwRet = ParseOperatorBinary (TRUE, pc, &varL, &varR)) {
            pVar->m_Var.SetUndefined();
            return dwRet;
         }
         pVar->m_Var.OperMultiply (this, &varL.m_Var, &varR.m_Var);
         return LValueSet (&varL, pVar, pc);
      }
      return MFC_NONE;

   case TOKEN_OPER_ASSIGNDIVIDE:
      {
         // DOCUMENT: Assign divide
         CMIFLVarLValue varL, varR;
         if (dwRet = ParseOperatorBinary (TRUE, pc, &varL, &varR)) {
            pVar->m_Var.SetUndefined();
            return dwRet;
         }
         pVar->m_Var.OperDivide (this, &varL.m_Var, &varR.m_Var);
         return LValueSet (&varL, pVar, pc);
      }
      return MFC_NONE;

   case TOKEN_OPER_ASSIGNMODULO:
      {
         // DOCUMENT: Assign modulo
         CMIFLVarLValue varL, varR;
         if (dwRet = ParseOperatorBinary (TRUE, pc, &varL, &varR)) {
            pVar->m_Var.SetUndefined();
            return dwRet;
         }
         pVar->m_Var.OperModulo (this, &varL.m_Var, &varR.m_Var);
         return LValueSet (&varL, pVar, pc);
      }
      return MFC_NONE;

   case TOKEN_OPER_ASSIGNBITWISELEFT:
      {
         // DOCUMENT: Assign bitwise left
         CMIFLVarLValue varL, varR;
         if (dwRet = ParseOperatorBinary (TRUE, pc, &varL, &varR)) {
            pVar->m_Var.SetUndefined();
            return dwRet;
         }
         pVar->m_Var.OperBitwiseLeft (this, &varL.m_Var, &varR.m_Var);
         return LValueSet (&varL, pVar, pc);
      }
      return MFC_NONE;

   case TOKEN_OPER_ASSIGNBITWISERIGHT:
      {
         // DOCUMENT: Assign bitwise right
         CMIFLVarLValue varL, varR;
         if (dwRet = ParseOperatorBinary (TRUE, pc, &varL, &varR)) {
            pVar->m_Var.SetUndefined();
            return dwRet;
         }
         pVar->m_Var.OperBitwiseRight (this, &varL.m_Var, &varR.m_Var);
         return LValueSet (&varL, pVar, pc);
      }
      return MFC_NONE;

   case TOKEN_OPER_ASSIGNBITWISEAND:
      {
         // DOCUMENT: Assign bitwise and
         CMIFLVarLValue varL, varR;
         if (dwRet = ParseOperatorBinary (TRUE, pc, &varL, &varR)) {
            pVar->m_Var.SetUndefined();
            return dwRet;
         }
         pVar->m_Var.OperBitwiseAnd (this, &varL.m_Var, &varR.m_Var);
         return LValueSet (&varL, pVar, pc);
      }
      return MFC_NONE;

   case TOKEN_OPER_ASSIGNBITWISEXOR:
      {
         // DOCUMENT: Assign bitwise xor
         CMIFLVarLValue varL, varR;
         if (dwRet = ParseOperatorBinary (TRUE, pc, &varL, &varR)) {
            pVar->m_Var.SetUndefined();
            return dwRet;
         }
         pVar->m_Var.OperBitwiseXOr (this, &varL.m_Var, &varR.m_Var);
         return LValueSet (&varL, pVar, pc);
      }
      return MFC_NONE;

   case TOKEN_OPER_ASSIGNBITWISEOR:
      {
         // DOCUMENT: Assign bitwise or
         CMIFLVarLValue varL, varR;
         if (dwRet = ParseOperatorBinary (TRUE, pc, &varL, &varR)) {
            pVar->m_Var.SetUndefined();
            return dwRet;
         }
         pVar->m_Var.OperBitwiseOr (this, &varL.m_Var, &varR.m_Var);
         return LValueSet (&varL, pVar, pc);
      }
      return MFC_NONE;

   case TOKEN_OPER_COMMA:
      {
         PMIFLCOMP pDown = pc;
         pVar->m_Var.SetUndefined();
         while (pDown->dwDown) {
            pDown = (PMIFLCOMP)(m_pFCICur->pbMIFLCOMP + pDown->dwDown);
            if (pDown->dwType == TOKEN_ARG1) {
               if (pDown->dwNext) {
                  dwRet = ExecuteCodeStatement (pDown->dwNext, pVar);
                     // DOCUMENT: When have statements separated by commas then last one is on stack
                  if (dwRet)
                     return dwRet;
               }
               else
                  pVar->m_Var.SetUndefined();
            }
         } // while TRUE
      }
      return MFC_NONE;

   case TOKEN_OPER_BRACEL:
      {
         // if no brace then skip
         if (!pc->dwDown) {
            pVar->m_Var.SetUndefined();
            return MFC_NONE;
         }

         return ExecuteCode (pc->dwDown, pVar);
      }
      return MFC_NONE;

   case TOKEN_BREAK:
      pVar->m_Var.SetUndefined();
      return MFC_BREAK;

   case TOKEN_CONTINUE:
      pVar->m_Var.SetUndefined();
      return MFC_CONTINUE;

   case TOKEN_IF:
      return ExecuteIfThenElse (dwIndex, pVar);

   case TOKEN_WHILE:
      return ExecuteWhile (dwIndex, pVar);

   case TOKEN_DO:
      return ExecuteDoWhile (dwIndex, pVar);

   case TOKEN_FOR:
      return ExecuteFor (dwIndex, pVar);

   case TOKEN_SWITCH:
      return ExecuteSwitch (dwIndex, pVar);

   case TOKEN_OPER_NUMSIGN:
      {
         // get down...
         PMIFLCOMP pcDown = pc->dwDown ? (PMIFLCOMP)(m_pFCICur->pbMIFLCOMP + pc->dwDown) : NULL;
         if (!pcDown || (pcDown->dwType != TOKEN_IDENTIFIER))
            return RunTimeErr (L"Expected import function.", TRUE, pc->wCharStart, pc->wCharEnd);

         // get the string
         PWSTR psz = (PWSTR)(m_pFCICur->pbMIFLCOMP + pcDown->dwValue);

         // import
         if (m_pSocket->FuncImport (psz, this, m_pFCICur->pObject, m_pFCICur->plParam,
            pcDown->wCharStart, pcDown->wCharEnd, &pVar->m_Var))
            return MFC_RETURN;

         // else error
         pVar->m_Var.SetUndefined();
         return RunTimeErr (L"Imported function not supported.", TRUE, pc->wCharStart, pc->wCharEnd);

      }
      return MFC_NONE;

   default:
      break;
   }

   // default.. .shouldn ge
   return RunTimeErr (L"Unexpected token.", TRUE, pc->wCharStart, pc->wCharEnd);
   pVar->m_Var.SetUndefined();
   return MFC_NONE;
}


/*****************************************************************************
CMIFLVM::ExecuteIfThenElse - Does if/then/else statements

inputs
   DWORD          dwIndex - Index
   PCMIFLVar      pVar - Should be initialized to something, but will be modified by
                  this function and filled in

returns
   DWORD - MFC_XXX
*/
DWORD CMIFLVM::ExecuteIfThenElse (DWORD dwIndex, PCMIFLVarLValue pVar)
{
   PMIFLCOMP pc = (PMIFLCOMP)(m_pFCICur->pbMIFLCOMP + dwIndex);
   PMIFLCOMP pcArg1 = pc->dwDown ? (PMIFLCOMP)(m_pFCICur->pbMIFLCOMP + pc->dwDown) : NULL;
   PMIFLCOMP pcArg2 = (pcArg1 && pcArg1->dwDown) ? (PMIFLCOMP)(m_pFCICur->pbMIFLCOMP + pcArg1->dwDown) : NULL;
   PMIFLCOMP pcArg3 = (pcArg2 && pcArg2->dwDown) ? (PMIFLCOMP)(m_pFCICur->pbMIFLCOMP + pcArg2->dwDown) : NULL;

   DWORD dwRet;

   if (pcArg1 && pcArg1->dwNext) {
      // will want to show debug window here if flags right
      if (m_dwDebugMode >= MDM_EVERYLINE) {
         dwRet = DebugUI (pcArg1->wCharStart, pcArg1->wCharEnd, NULL);
         if (dwRet)
            return dwRet;
      }

      dwRet = ExecuteCodeStatement (pcArg1->dwNext, pVar);
      if (dwRet) {
         pVar->m_Var.SetUndefined ();  // since will not have break, et.
         return dwRet;
      }
   }
   else
      pVar->m_Var.SetBOOL (TRUE);

   // depending upon the statement
   if (!pVar->m_Var.GetBOOL(this))
      pcArg2 = pcArg3;

   if (!pcArg2 || !pcArg2->dwNext) {
      // no code
      pVar->m_Var.SetUndefined ();  // since will not have break, et.
      return MFC_NONE;
   }

   // run the code
   return ExecuteCode (pcArg2->dwNext, pVar);
}


/*****************************************************************************
CMIFLVM::ExecuteSwitch - Do switch statements...

inputs
   DWORD          dwIndex - Index
   PCMIFLVar      pVar - Should be initialized to something, but will be modified by
                  this function and filled in

returns
   DWORD - MFC_XXX
*/
DWORD CMIFLVM::ExecuteSwitch (DWORD dwIndex, PCMIFLVarLValue pVar)
{
   PMIFLCOMP pc = (PMIFLCOMP)(m_pFCICur->pbMIFLCOMP + dwIndex);
   PMIFLCOMP pcArg1 = pc->dwDown ? (PMIFLCOMP)(m_pFCICur->pbMIFLCOMP + pc->dwDown) : NULL;
   pc = (pcArg1 && pcArg1->dwDown) ? (PMIFLCOMP)(m_pFCICur->pbMIFLCOMP + pcArg1->dwDown) : NULL;

   DWORD dwRet;
   CMIFLVarLValue vTest;

   if (pcArg1 && pcArg1->dwNext) {
      // will want to show debug window here if flags right
      if (m_dwDebugMode >= MDM_EVERYLINE) {
         dwRet = DebugUI (pcArg1->wCharStart, pcArg1->wCharEnd, NULL);
         if (dwRet)
            return dwRet;
      }

      dwRet = ExecuteCodeStatement (pcArg1->dwNext, &vTest);
      if (dwRet) {
         pVar->m_Var.SetUndefined ();  // since will not have break, et.
         return dwRet;
      }
   }
   else {
      // if nothing in switch then exit
      pVar->m_Var.SetUndefined ();
      return MFC_NONE;
   }

   // remember default loc...
   PMIFLCOMP pDefault = NULL;

   // loop down testing
   for (;pc; pc = (pc->dwDown ? (PMIFLCOMP)(m_pFCICur->pbMIFLCOMP + pc->dwDown) : NULL)) {
      if (pc->dwType == TOKEN_ARG2) {
         pDefault = pc; // remember default
         continue;
      }

      // if it's a block of code then skip because not looking for code now
      if ((pc->dwType != TOKEN_ARG3) || !pc->dwNext)
         continue;

      // else a test
      // will want to show debug window here if flags right
      if (m_dwDebugMode >= MDM_EVERYLINE) {
         dwRet = DebugUI (pc->wCharStart, pc->wCharEnd, NULL);
         if (dwRet)
            return dwRet;
      }

      dwRet = ExecuteCodeStatement (pc->dwNext, pVar);
      if (dwRet) {
         pVar->m_Var.SetUndefined ();  // since will not have break, et.
         return dwRet;
      }

      if (pVar->m_Var.Compare (&vTest.m_Var, FALSE, this))
         continue;   // not equal

      // else, found a match
      goto foundmatch;
   } // for pc

   // if get here then didn't find a match. If don't have a default in the
   // case then just exit
   if (!pDefault) {
      pVar->m_Var.SetUndefined();
      return MFC_NONE;
   }
   pc = pDefault;

foundmatch:
   // repeat until end of code or get a break
   for (;pc; pc = (pc->dwDown ? (PMIFLCOMP)(m_pFCICur->pbMIFLCOMP + pc->dwDown) : NULL)) {
      if ((pc->dwType != TOKEN_ARG4) || !pc->dwNext)
         continue;

      // run the code
      // will want to show debug window here if flags right
      if (m_dwDebugMode >= MDM_EVERYLINE) {
         dwRet = DebugUI (pc->wCharStart, pc->wCharEnd, NULL);
         if (dwRet)
            return dwRet;
      }

      dwRet = ExecuteCode (pc->dwNext, pVar);
      if (dwRet == MFC_BREAK)
         break;
      if (dwRet)
         return dwRet;
   } // for pc

   // no return
   pVar->m_Var.SetUndefined();
   return MFC_NONE;
}



/*****************************************************************************
CMIFLVM::ExecuteFor - Do for(;;) {};

inputs
   DWORD          dwIndex - Index
   PCMIFLVar      pVar - Should be initialized to something, but will be modified by
                  this function and filled in

returns
   DWORD - MFC_XXX
*/
DWORD CMIFLVM::ExecuteFor (DWORD dwIndex, PCMIFLVarLValue pVar)
{
   PMIFLCOMP pc = (PMIFLCOMP)(m_pFCICur->pbMIFLCOMP + dwIndex);
   PMIFLCOMP pcArg1 = pc->dwDown ? (PMIFLCOMP)(m_pFCICur->pbMIFLCOMP + pc->dwDown) : NULL;
   PMIFLCOMP pcArg2 = (pcArg1 && pcArg1->dwDown) ? (PMIFLCOMP)(m_pFCICur->pbMIFLCOMP + pcArg1->dwDown) : NULL;
   PMIFLCOMP pcArg3 = (pcArg2 && pcArg2->dwDown) ? (PMIFLCOMP)(m_pFCICur->pbMIFLCOMP + pcArg2->dwDown) : NULL;
   PMIFLCOMP pcArg4 = (pcArg3 && pcArg3->dwDown) ? (PMIFLCOMP)(m_pFCICur->pbMIFLCOMP + pcArg3->dwDown) : NULL;

   DWORD dwRet;

   // run the initial case
   if (pcArg1 && pcArg1->dwNext) {
      // will want to show debug window here if flags right
      if (m_dwDebugMode >= MDM_EVERYLINE) {
         dwRet = DebugUI (pcArg1->wCharStart, pcArg1->wCharEnd, NULL);
         if (dwRet)
            return dwRet;
      }

      dwRet = ExecuteCodeStatement (pcArg1->dwNext, pVar);
      if (dwRet) {
         pVar->m_Var.SetUndefined ();  // since will not have break, et.
         return dwRet;
      }
   } // if pcArg1

   DWORD dwLoopCount = 0;
   while (TRUE) {
      // prevent infinite loops
      dwLoopCount++;
      if (dwLoopCount > m_dwMaxLoopCount) {
         pVar->m_Var.SetUndefined();
         return RunTimeErr (L"The function has looped too many times. Stopping the loop.", FALSE,
            pc->wCharStart, pc->wCharEnd);
      }


      // run the check
      if (pcArg2 && pcArg2->dwNext) {
         // will want to show debug window here if flags right
         if (m_dwDebugMode >= MDM_EVERYLINE) {
            dwRet = DebugUI (pcArg2->wCharStart, pcArg2->wCharEnd, NULL);
            if (dwRet)
               return dwRet;
         }

         dwRet = ExecuteCodeStatement (pcArg2->dwNext, pVar);
         if (dwRet) {
            pVar->m_Var.SetUndefined ();  // since will not have break, et.
            return dwRet;
         }
      }
      else
         pVar->m_Var.SetBOOL (TRUE);

      // break?
      if (!pVar->m_Var.GetBOOL(this))
         break;


      // run the code
      if (pcArg4 && pcArg4->dwNext) {
         dwRet = ExecuteCode (pcArg4->dwNext, pVar);
         if (dwRet == MFC_CONTINUE)
            dwRet = MFC_NONE; // so continues
         else if (dwRet == MFC_BREAK)
            break;
         if (dwRet)
            return dwRet;
      } // arg3

      // run the increment
      if (pcArg3 && pcArg3->dwNext) {
         // will want to show debug window here if flags right
         if (m_dwDebugMode >= MDM_EVERYLINE) {
            dwRet = DebugUI (pcArg3->wCharStart, pcArg3->wCharEnd, NULL);
            if (dwRet)
               return dwRet;
         }

         dwRet = ExecuteCodeStatement (pcArg3->dwNext, pVar);
         if (dwRet) {
            pVar->m_Var.SetUndefined ();  // since will not have break, et.
            return dwRet;
         }
      } // if pcArg3
   }

   pVar->m_Var.SetUndefined();
   return MFC_NONE;
}



/*****************************************************************************
CMIFLVM::ExecuteWhile - Does while() {};

inputs
   DWORD          dwIndex - Index
   PCMIFLVar      pVar - Should be initialized to something, but will be modified by
                  this function and filled in

returns
   DWORD - MFC_XXX
*/
DWORD CMIFLVM::ExecuteWhile (DWORD dwIndex, PCMIFLVarLValue pVar)
{
   PMIFLCOMP pc = (PMIFLCOMP)(m_pFCICur->pbMIFLCOMP + dwIndex);
   PMIFLCOMP pcArg1 = pc->dwDown ? (PMIFLCOMP)(m_pFCICur->pbMIFLCOMP + pc->dwDown) : NULL;
   PMIFLCOMP pcArg2 = (pcArg1 && pcArg1->dwDown) ? (PMIFLCOMP)(m_pFCICur->pbMIFLCOMP + pcArg1->dwDown) : NULL;

   DWORD dwRet;

   DWORD dwLoopCount = 0;
   while (TRUE) {
      // prevent infinite loops
      dwLoopCount++;
      if (dwLoopCount > m_dwMaxLoopCount) {
         pVar->m_Var.SetUndefined();
         return RunTimeErr (L"The function has looped too many times. Stopping the loop.", FALSE,
            pc->wCharStart, pc->wCharEnd);
      }

      if (pcArg1 && pcArg1->dwNext) {
         // will want to show debug window here if flags right
         if (m_dwDebugMode >= MDM_EVERYLINE) {
            dwRet = DebugUI (pcArg1->wCharStart, pcArg1->wCharEnd, NULL);
            if (dwRet)
               return dwRet;
         }

         dwRet = ExecuteCodeStatement (pcArg1->dwNext, pVar);
         if (dwRet) {
            pVar->m_Var.SetUndefined ();  // since will not have break, et.
            return dwRet;
         }
      }
      else
         pVar->m_Var.SetBOOL (TRUE);

      // break?
      if (!pVar->m_Var.GetBOOL(this))
         break;

      if (!pcArg2 || !pcArg2->dwNext)
         continue;   // repeat while

      // run the code
      dwRet = ExecuteCode (pcArg2->dwNext, pVar);
      if (dwRet == MFC_CONTINUE)
         continue;
      else if (dwRet == MFC_BREAK)
         break;
      if (dwRet)
         return dwRet;
   }

   pVar->m_Var.SetUndefined();
   return MFC_NONE;
}


/*****************************************************************************
CMIFLVM::ExecuteDoWhile - Does Do{} while();

inputs
   DWORD          dwIndex - Index
   PCMIFLVar      pVar - Should be initialized to something, but will be modified by
                  this function and filled in

returns
   DWORD - MFC_XXX
*/
DWORD CMIFLVM::ExecuteDoWhile (DWORD dwIndex, PCMIFLVarLValue pVar)
{
   PMIFLCOMP pc = (PMIFLCOMP)(m_pFCICur->pbMIFLCOMP + dwIndex);
   PMIFLCOMP pcArg1 = pc->dwDown ? (PMIFLCOMP)(m_pFCICur->pbMIFLCOMP + pc->dwDown) : NULL;
   PMIFLCOMP pcArg2 = (pcArg1 && pcArg1->dwDown) ? (PMIFLCOMP)(m_pFCICur->pbMIFLCOMP + pcArg1->dwDown) : NULL;

   DWORD dwRet;

   DWORD dwLoopCount = 0;
   while (TRUE) {
      if (!pcArg1 || !pcArg1->dwNext)
         continue;   // repeat while

      // run the code
      dwRet = ExecuteCode (pcArg1->dwNext, pVar);
      if (dwRet == MFC_CONTINUE)
         continue;
      else if (dwRet == MFC_BREAK)
         break;
      if (dwRet)
         return dwRet;

      // prevent infinite loops
      dwLoopCount++;
      if (dwLoopCount > m_dwMaxLoopCount) {
         pVar->m_Var.SetUndefined();
         return RunTimeErr (L"The function has looped too many times. Stopping the loop.", FALSE,
            pc->wCharStart, pc->wCharEnd);
      }

      if (pcArg2 && pcArg2->dwNext) {
         // will want to show debug window here if flags right
         if (m_dwDebugMode >= MDM_EVERYLINE) {
            dwRet = DebugUI (pcArg2->wCharStart, pcArg2->wCharEnd, NULL);
            if (dwRet)
               return dwRet;
         }

         dwRet = ExecuteCodeStatement (pcArg2->dwNext, pVar);
         if (dwRet) {
            pVar->m_Var.SetUndefined ();  // since will not have break, et.
            return dwRet;
         }
      }
      else
         pVar->m_Var.SetBOOL (TRUE);

      // break?
      if (!pVar->m_Var.GetBOOL(this))
         break;

   }

   pVar->m_Var.SetUndefined();
   return MFC_NONE;
}

/*****************************************************************************
CMIFLVM::ParseOperatorUnary - Gets the variable for a unary operator.

inputs
   PMIFLCOMP         pc - Pointer to the operator that's is TOKEN_OPER_XXX
   DWORD             *pdwType - Filled in with the type, whether it's arg1 or arg2
   PCMIFLVar         pVar - Filled in with the variable for the unary operator
returns
   DWORD - MFC_XXX
*/
DWORD CMIFLVM::ParseOperatorUnary (PMIFLCOMP pc, DWORD *pdwType, PCMIFLVarLValue pVar)
{
   pVar->Clear();

   PMIFLCOMP pcArg1 = pc->dwDown ? (PMIFLCOMP)(m_pFCICur->pbMIFLCOMP + pc->dwDown) : NULL;
   if (!pcArg1 || !pcArg1->dwNext) {
      pVar->m_Var.SetUndefined();
      return RunTimeErr (L"Operand expected.", TRUE, pc->wCharStart, pc->wCharEnd);
   }
   if (pdwType)
      *pdwType = pcArg1->dwType;

   return ExecuteCodeStatement (pcArg1->dwNext, pVar);
}


/*****************************************************************************
CMIFLVM::ParseOperatorBinary - Gets the variable for a binary operator.

inputs
   BOOL              fOnlyVarAccess - If TRUE then check that fOnlyVarAccess is TRUE.
                        If not, then fail
   PMIFLCOMP         pc - Pointer to the operator that's is TOKEN_OPER_XXX
   PCMIFLVar         pVarL - Filled in with the variable for the left operator
   PCMIFLVar         pVarR - Filled with the variable for the right operator
   DWORD             dwSpecial - Normally this is 0. However, this is used to
                        simulate c-style optimizations of || and && so that
                        in (x || y), if x is true, then y is NOT executed.
                        Likewise, in (x && y) if x is FALSE then y is NOT executed.

                        dwSpecial == 1 for the (x || y) case
                        dwSpecial == 2 for the (x && y) case
returns
   DWORD - MFC_XXX
*/
DWORD CMIFLVM::ParseOperatorBinary (BOOL fOnlyVarAccess, PMIFLCOMP pc, PCMIFLVarLValue pVarL, PCMIFLVarLValue pVarR,
                                    DWORD dwSpecial)
{
   pVarL->Clear();
   pVarR->Clear();

   if (fOnlyVarAccess && m_pFCICur->fOnlyVarAccess) {
      pVarL->m_Var.SetUndefined();
      pVarR->m_Var.SetUndefined();
      return RunTimeErr (gpszOnlyVar, TRUE, pc->wCharStart, pc->wCharEnd);
   }

   PMIFLCOMP pcArg1 = pc->dwDown ? (PMIFLCOMP)(m_pFCICur->pbMIFLCOMP + pc->dwDown) : NULL;
   DWORD dwRet;
   if (!pcArg1 || !pcArg1->dwNext) {
      pVarL->m_Var.SetUndefined();
      pVarR->m_Var.SetUndefined();
      return RunTimeErr (L"Left operand expected.", TRUE, pc->wCharStart, pc->wCharEnd);
   }
   dwRet = ExecuteCodeStatement (pcArg1->dwNext, pVarL);
   if (dwRet)
      return dwRet;

   // BUGFIX - if special than act on it
   if (dwSpecial) {
      if ((dwSpecial == 1) && pVarL->m_Var.GetBOOL(this)) { // or case
         pVarR->m_Var.SetBOOL (TRUE);
         return MFC_NONE;
      }
      else if ((dwSpecial == 2) && !pVarL->m_Var.GetBOOL(this)) { // and case
         pVarR->m_Var.SetBOOL (FALSE);
         return MFC_NONE;
      }
   }

   pcArg1 = pcArg1->dwDown ? (PMIFLCOMP)(m_pFCICur->pbMIFLCOMP + pcArg1->dwDown) : NULL;
   if (!pcArg1 || !pcArg1->dwNext) {
      pVarR->m_Var.SetUndefined();
      return RunTimeErr (L"Right operand expected.", TRUE, pc->wCharStart, pc->wCharEnd);
   }
   return  ExecuteCodeStatement (pcArg1->dwNext, pVarR);
}


/*****************************************************************************
CMIFLVM::ExecuteCode - This internal function takes compiled code and
runs it. This keeps on running through all the NEXT statemts.

It uses the m_pFCICur for the pointer to the code.

inputs
   DWORD          dwIndex - Index to use into m_pFCICur->pMemMIFLCOMP
   PCMIFLVar      pVar - Should be initialized to something, but will be modified by
                  this function and filled in

returns
   DWORD - MFC_XXX
*/
DWORD CMIFLVM::ExecuteCode (DWORD dwIndex, PCMIFLVarLValue pVar)
{
   PMIFLCOMP pc;
   DWORD dwRet;

   do {
      pc = (PMIFLCOMP)(m_pFCICur->pbMIFLCOMP + dwIndex);

      // will want to show debug window here if flags right
      if (m_dwDebugMode >= MDM_EVERYLINE) {
         dwRet = DebugUI (pc->wCharStart, pc->wCharEnd, NULL);
         if (dwRet)
            return dwRet;
      }

      dwRet = ExecuteCodeStatement (dwIndex, pVar);
      if (dwRet)
         return dwRet;

      // next statement
      dwIndex = pc->dwNext;
   } while (dwIndex);

   return MFC_NONE;
}



/*****************************************************************************
CMIFLVM::DebugModeSet - Sets the debug mode. Must be one of MDM_XXX

inputs
   DWORD          dwMode
*/
void CMIFLVM::DebugModeSet (DWORD dwMode)
{
   m_dwDebugMode = dwMode;
}


/*****************************************************************************
CMIFLVM::DebugModeGet - returns the current debug mode
*/
DWORD CMIFLVM::DebugModeGet (void)
{
   return m_dwDebugMode;
}



/*****************************************************************************
CMIFLVM::DebugWindowSetParent - The application should call this to tell the VM
what HWND to use as a parent for any debugging windows it creates, ensuring
that other code will be stoppoed while debugging.

NOTE: This is slightly different than the DebugWindowSet(PCEscWindow) call
because that specifies a window to use.

inputs
   HWND        hWndParent - Parent to use
*/
void CMIFLVM::DebugWindowSetParent (HWND hWndParent)
{
   m_hWndDebugParent = hWndParent;
}


/*****************************************************************************
CMIFLVM::DebugWindowSet - Normally, the VM will create its own window if
a debug window is called for. However, if DebugWindowSet() is called first, then
it will use the window passed in. (Which means it's important not to call
any functions that would require debugging if already in the window that
is to be used for the debug window.)

inputs
   PCEscWindow          pWindow - Window to use. Call with NULL to stop using the window
*/
void CMIFLVM::DebugWindowSet (PCEscWindow pWindow)
{
   m_pDebugWindowApp = pWindow;

   // free existing window
   if (pWindow) {
      if (!m_fDebugUIIn && m_pDebugWindowOwn) {
         delete m_pDebugWindowOwn;
         m_pDebugWindowOwn = NULL;
      }
   }
}

/*********************************************************************************
DebugPage - UI
*/

BOOL DebugPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PMIFLPAGE pmp = (PMIFLPAGE) pPage->m_pUserData;
   PCMIFLVM pVM = pmp->pVM;

   if (pmp->pVM)
      if (pmp->pVM->DebugPage (pPage, dwMessage, pParam, FALSE))
         return TRUE;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         PCEscControl pControl = pPage->ControlFind (L"code");
         if (pControl) {
            pControl->AttribSet (Text(), pVM->m_pFCICur->pszCode);
            pControl->AttribSetInt (L"selstart", (int)LOWORD(pmp->dwFlag));
            pControl->AttribSetInt (L"selend", (int)HIWORD(pmp->dwFlag));
            pControl->Message (ESCM_EDITSCROLLCARET);
         }
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"DEBUGERR")) {
            if (!pmp->pszErrLink)
               return FALSE;

            MemZero (&pVM->m_memTemp);

            MemCat (&pVM->m_memTemp,
               L"<xtablecenter width=100%>"
			      L"<xtrheader>Debug log</xtrheader>"
               L"<tr><td><bold>Stopped because of error:</bold> ");
            MemCatSanitize (&pVM->m_memTemp, pmp->pszErrLink);
            MemCat (&pVM->m_memTemp,
			      L"</td></tr>"
		         L"</xtablecenter>");

            p->pszSubString = (PWSTR)pVM->m_memTemp.p;
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            MemZero (&pVM->m_memTemp);

            if (pVM->m_pFCICur->pszWhere1)
               MemCatSanitize (&pVM->m_memTemp, pVM->m_pFCICur->pszWhere1);
            if (pVM->m_pFCICur->pszWhere2) {
               MemCat (&pVM->m_memTemp, L".");
               MemCatSanitize (&pVM->m_memTemp, pVM->m_pFCICur->pszWhere2);
            }


            p->pszSubString = (PWSTR)pVM->m_memTemp.p;
            return TRUE;
         }
      }
      break;
   }

   return FALSE;
}


/*****************************************************************************
CMIFLVM::DebugUI - Brings up the debug UI. In the process, this will set
m_dwDebugMode

Uses m_pFCICur for display information.

inputs
   DWORD          dwCharStart - Starting character
   DWORD          dwCharEnd - Ending character
   PWSTR          pszErr - Error to display
returns
   DWORD - MFC_XXX result
*/
DWORD CMIFLVM::DebugUI (DWORD dwCharStart, DWORD dwCharEnd, PWSTR pszErr)
{
   if (m_fDebugUIIn)
      return MFC_NONE;  // already in debug

   m_fDebugUIIn = TRUE;

   // create the window
   // BUGFIX - Added m_pDebugWindowApp->m_pPage so could edbug off of page
   if ((!m_pDebugWindowApp || m_pDebugWindowApp->m_pPage) && !m_pDebugWindowOwn) {
      m_pDebugWindowOwn = new CEscWindow;
      if (!m_pDebugWindowOwn) {
         m_fDebugUIIn = FALSE;
         return MFC_ABORTFUNC;
      }

      HWND hWndParent = m_hWndDebugParent;
      if (m_pDebugWindowApp && m_pDebugWindowApp->m_pPage && m_pDebugWindowApp->m_hWnd)
         hWndParent = m_pDebugWindowApp->m_hWnd;

      RECT r;
      DialogBoxLocation3 (hWndParent, &r, TRUE);

      m_pDebugWindowOwn->Init (ghInstance, hWndParent, 0 /*EWS_FIXEDSIZE*/, &r);
   }

   MIFLPAGE mp;
   memset (&mp, 0, sizeof(mp));
   mp.pVM = this;
   mp.pszErrLink = pszErr;
   mp.dwFlag = (dwCharEnd << 16) | dwCharStart;

   // set info relevent to debug..
   if (m_pFCICur->pObject) {
      m_gDebugVarsObject = m_pFCICur->pObject->m_gID;
      m_pDebugVarsObjectLayer = m_pFCICur->pObjectLayer;
   }
   else {
      m_gDebugVarsObject = GUID_NULL;
      m_pDebugVarsObjectLayer = NULL;
   }
   m_phDebugVarsString = m_pFCICur->phVarsString;
   m_paDebugVars = m_pFCICur->paVars;
   m_dwDebugVarsNum = m_pFCICur->dwVarsNum;


   PWSTR pszRet;
   DWORD dwRet = MFC_NONE;
   // BUGFIX - Make debugwindowown take precendence
   PCEscWindow pWindow = (m_pDebugWindowOwn ? m_pDebugWindowOwn : m_pDebugWindowApp);
redo:
   pszRet = pWindow->PageDialog (ghInstance, IDR_MMLDEBUG, ::DebugPage, &mp);
   mp.iVScroll = pWindow->m_iExitVScroll;
   if (pszRet && !_wcsicmp(pszRet, MIFLRedoSamePage()))
      goto redo;
   else if (pszRet && !_wcsicmp(pszRet, L"exit"))
      dwRet = MFC_ABORTFUNC;
   else if (pszRet && !_wcsicmp(pszRet, L"run"))
      m_dwDebugMode = MDM_ONRUNTIME;
   else if (pszRet && !_wcsicmp(pszRet, L"stepout"))
      m_dwDebugMode = MDM_STEPOVER;
   else if (pszRet && !_wcsicmp(pszRet, L"stepin"))
      m_dwDebugMode = MDM_STEPIN;
   else if (pszRet && !_wcsicmp(pszRet, L"stepover"))
      m_dwDebugMode = MDM_EVERYLINE;
   else
      dwRet = MFC_ABORTFUNC;  // probably closed window

   // BUGIFX - reset debug display params, so dont accidentally see later
   m_gDebugVarsObject = GUID_NULL;
   m_pDebugVarsObjectLayer = NULL;
   m_phDebugVarsString = NULL;
   m_paDebugVars = NULL;
   m_dwDebugVarsNum = NULL;

   // free up the window if created it...
   // NOTE: Only kill if if < stepover mode since will probably need it shortly
   if (m_pDebugWindowOwn && (m_dwDebugMode <= MDM_ONRUNTIME)) {
      delete m_pDebugWindowOwn;
      m_pDebugWindowOwn = NULL;
   }

   m_fDebugUIIn = FALSE;

   // delete just in case
   MaintinenceDeleteAll ();

   return dwRet;
}



/*****************************************************************************
CMIFLVM::LValueSet - Sets an LValue, erroring out if it doesn't exist.

inputs
   PCMIFLVar         pLValue - Original LValue
   PCMIFLVar         pTo - What to copy over it
   PMIFLCOMP         pc - Used to indicate an error. Tries to get pc->dwDown and
                     use as reference. If that failed uses pc.
returns
   DWORD - MFC_XXX
*/
DWORD CMIFLVM::LValueSet (PCMIFLVarLValue pLValue, PCMIFLVarLValue pTo, PMIFLCOMP pc)
{
   switch (pLValue->m_dwLValue) {
   case MLV_VARIABLE:        // m_pLValue is a pointer to PCMIFLVar
      // NOTE - I'm not resetting the LValue information. It seem to work without it,
      // but at some point I may need to set specific LValue info... although I dont think so
      // If so, would affect all MLV_XXX
      ((PCMIFLVar)pLValue->m_pLValue)->Set (&pTo->m_Var);
      return MFC_NONE;

   case MLV_GLOBAL:        // m_dwLValueID is the global ID
      return GlobalSet (pLValue->m_dwLValueID,
         !m_pFCICur->pObject && (m_pFCICur->dwPropIDGetSet == pLValue->m_dwLValueID), &pTo->m_Var);
         // ignoreset based on whether or not in get/set code for global

   case MLV_PROPERTY:        // m_pLValue is a pointer to PCMILFVMObject, m_dwLValueID is property ID
      return PropertySet (pLValue->m_dwLValueID, (PCMIFLVMObject) pLValue->m_pLValue,
         (m_pFCICur->pObject==(PCMIFLVMObject) pLValue->m_pLValue) && (m_pFCICur->dwPropIDGetSet == pLValue->m_dwLValueID), &pTo->m_Var);
         // ignoreset based on whether or not in get/set code for property

   case MLV_LISTINDEX:        // m_pLValue is a pointer to PCMIFLVarList, m_dwLValueID is the index. NOT addrefed
      {
         // DOCUMENT: Setting list entry
         // DOCUMENT: Setting list entry will extend list
         BOOL fRet = ((PCMIFLVarList)pLValue->m_pLValue)->Set (pLValue->m_dwLValueID, &pTo->m_Var);
         if (!fRet)
            return RunTimeErr (L"Setting the list value failed. You may have tried to make the list a sub-element of itself.",
               TRUE, pc->wCharStart, pc->wCharEnd);
      }
      return MFC_NONE;

   case MLV_STRINGINDEX:        // m_pLValue is a pointer to PCMIFLVarString, m_dwLVValueID is the char. NOT addrefed
      {
         // DOCUMENT: setting string character will work, and will shorten string
         // DOCUMENT: setting beyond end of string will expand with spaces
         BOOL fRet = ((PCMIFLVarString)pLValue->m_pLValue)->Set (pLValue->m_dwLValueID, &pTo->m_Var, this);
         if (!fRet)
            return RunTimeErr (L"Setting the character failed. Out of memory?",
               TRUE, pc->wCharStart, pc->wCharEnd);
      }
      return MFC_NONE;

   } // switch

   // if get here then error
   if (pc->dwDown)
      pc = (PMIFLCOMP)(m_pFCICur->pbMIFLCOMP + pc->dwDown);
   return RunTimeErr (L"This must be an L-Value.", TRUE, pc->wCharStart, pc->wCharEnd);
}




/*****************************************************************************
CMIFLVM::Rand - Random function so each VM has its own random

returns
   int - Random value. 0 to RAND_MAX
*/
int CMIFLVM::Rand (void)
{
   return rand();

#if 0 // dont use because easier to use C++ one
   // NOTE: I know this is not a very good random, but using one that can have a seed
   // this is the same code as used for C
   m_dwRandSeed = m_dwRandSeed * 1103515245 + 12345;
   return (DWORD)(m_dwRandSeed / 65536) % 32768;
#endif
}

/*****************************************************************************
CMIFLVM::SRand - Seed random.
*/
void CMIFLVM::SRand (DWORD dwSeed)
{
   srand (dwSeed);

#if 0 // dont use because easier to use C++ one
   m_dwRandSeed = (dwSeed << 16) + LOWORD(dwSeed);
#endif
}


/*****************************************************************************
CMIFLVM::TimerAdd - Adds a timer to the list.

NOTE: Adds such that sorted by m_fTimeLeft.

inputs
   PCMIFLVMTimer        pTimer - Timer object to add. This is the actual
                        one added. so the caller loses control of the pointer.
*/
void CMIFLVM::TimerAdd (PCMIFLVMTimer pTimer)
{
   // BUGFIX - Since are no longer keeping the timers sorted, since can
   // quickly become unsorted by variable timer rates, don't sort
   m_lPCMIFLVMTimer.Add (&pTimer);

#if 0 // old code
   DWORD dwNum = m_lPCMIFLVMTimer.Num();
   PCMIFLVMTimer *ppt = (PCMIFLVMTimer*)m_lPCMIFLVMTimer.Get(0);
   DWORD dwTree, dw, dwTry;
   for (dwTree = 1; dwTree < dwNum; dwTree *= 2);

   // search in
   for (dw = 0; dwTree; dwTree /= 2) {
      dwTry = dw + dwTree;
      if (dwTry >= dwNum)
         continue;   // out of range

      if (pTimer->m_fTimeLeft >= ppt[dwTry]->m_fTimeLeft)
         dw = dwTry;
   } // dwTree

   if ((dw < dwNum) && (pTimer->m_fTimeLeft >= ppt[dw]->m_fTimeLeft))
      dw++; // add to end

   // add/insert
   if (dw >= dwNum)
      m_lPCMIFLVMTimer.Add (&pTimer);
   else
      m_lPCMIFLVMTimer.Insert (dw, &pTimer);
#endif
}


/*****************************************************************************
CMIFLVM::TimerFind - Finds a timer by owner and identifier.

inputs
   GUID           *pgOwner - Owner
   PCMIFLVar      pVarName - Name. Do exact compare to match
returns
   DWORD - Index into timers, or -1 if can't find
*/
DWORD CMIFLVM::TimerFind (GUID *pgOwner, PCMIFLVar pVarName)
{
   DWORD dwNum = m_lPCMIFLVMTimer.Num();
   PCMIFLVMTimer *ppt = (PCMIFLVMTimer*)m_lPCMIFLVMTimer.Get(0);
   DWORD i;
   for (i = 0; i < dwNum; i++)
      if (IsEqualGUID (*pgOwner, ppt[i]->m_gBelongsTo) && !pVarName->Compare (&ppt[i]->m_varName, TRUE, this))
         return i;

   // else not found
   return -1;
}


/*****************************************************************************
CMIFLVM::TimerRemove - Removes a timer by the index.

inputs
   DWORD          dwIndex - index. From TimerFind()
returns
   PCMIFLVMTimer - The timer object. The caller must free this if it wants the timer delted
*/
PCMIFLVMTimer CMIFLVM::TimerRemove (DWORD dwIndex)
{
   if (dwIndex >= m_lPCMIFLVMTimer.Num())
      return NULL;

   PCMIFLVMTimer *ppt = (PCMIFLVMTimer*)m_lPCMIFLVMTimer.Get(0);
   PCMIFLVMTimer pp = ppt[dwIndex];
   m_lPCMIFLVMTimer.Remove (dwIndex);

   return pp;
}




/*****************************************************************************
CMIFLVM::TimerGet - Gets a timer by index. DO NOT change or delete the timer info.

inputs
   DWORD          dwIndex - index. From TimerFind()
returns
   PCMIFLVMTimer - The timer object. DO NOT change or delete the timeer info
*/
PCMIFLVMTimer CMIFLVM::TimerGet (DWORD dwIndex)
{
   if (dwIndex >= m_lPCMIFLVMTimer.Num())
      return NULL;

   PCMIFLVMTimer *ppt = (PCMIFLVMTimer*)m_lPCMIFLVMTimer.Get(0);
   return ppt[dwIndex];
}


/*****************************************************************************
CMIFLVM::TimerEnum - Given an object, this fills in a list with all the
timers associated with the object and (optionall) removes them.

inputs
   GUID           *pgOwner - Object
   BOOL           fRemove - If TRUE then remove them from the VM's list while at it,
                  otherwise leave them there too
   PCListFixed    plTimers - This should already be initialized to sizeof (PCMIFLVMTimer).
                  It has timers APPENDED to it if they're found
returns
   none
*/
void CMIFLVM::TimerEnum (GUID *pgOwner, BOOL fRemove, PCListFixed plTimers)
{
   DWORD dwNum = m_lPCMIFLVMTimer.Num();
   PCMIFLVMTimer *ppt = (PCMIFLVMTimer*)m_lPCMIFLVMTimer.Get(0);
   DWORD i;
   for (i = dwNum-1; i < dwNum; i--) {
      if (!IsEqualGUID(ppt[i]->m_gBelongsTo, *pgOwner))
         continue;   // not right object

      // else, remove this
      plTimers->Add (&ppt[i]);
      if (fRemove) {
         m_lPCMIFLVMTimer.Remove (i);
         dwNum = m_lPCMIFLVMTimer.Num();
         ppt = (PCMIFLVMTimer*)m_lPCMIFLVMTimer.Get(0);
      }
   } // i
}


/*****************************************************************************
CMIFLVM::AlreadyInFunction - Returns TRUE if we're already in a function
or debugging. Use this to stop re-entrancy problems.
*/
BOOL CMIFLVM::AlreadyInFunction (void)
{
   return (m_dwFuncLevelDeep || m_fDebugUIIn);  // if either TRUE then in function
}



/*****************************************************************************
CMIFLVM::MaintinenceTimer - If an application has not turned on TimerAutomatic (),
then it should call this periodically so that timers in objects get run.

NOTE: If TimerAutomatic() is on the don't bother calling this since it will be
called automatically.

inputs
   double            fTimeSinceLast - The time since the last SUCCESSFUL call
                     to MaintinenceTimer (or 0 if this is the first time that calling).
                     If MaintinenceTimer() previously failed, then need to include
                     aggregate time.
returns
   BOOL - TRUE if success. MaintinenceTimer() can fail (returning FALSE) if this
         would cause re-entry into the VM (checking m_dwFuncLevelDeep || m_fDebugUIIn).
         Could also return an error if user was debugging and then wanted to quit.
*/
BOOL CMIFLVM::MaintinenceTimer (double fTimeSinceLast)
{
   if (AlreadyInFunction())
      return FALSE;

   // loop through and subtract time
   DWORD dwNum = m_lPCMIFLVMTimer.Num();
   if (!dwNum)
      return TRUE;   // no timers

   PCMIFLVMTimer *ppt = (PCMIFLVMTimer*)m_lPCMIFLVMTimer.Get(0);
   DWORD i;
   if (fTimeSinceLast) for (i = 0; i < dwNum; i++)
      ppt[i]->m_fTimeLeft -= fTimeSinceLast * ppt[i]->m_fTimeScale;
         // BUGFIX - Scaling the time, so can slow down timers for rooms the player isn't in

   // repeat while timers should be run
   CMIFLVarLValue varRet;
   BOOL fTimerWentOff = TRUE;
   while (fTimerWentOff) {
      fTimerWentOff = FALSE;

      for (i = 0; i < dwNum; i++) {
         // if the timer hasn't gone off yet then skip
         PCMIFLVMTimer pCur = ppt[i];
         if (pCur->m_fTimeLeft > 0.0)
            continue;

         // note that the timer went off
         fTimerWentOff = TRUE;
         m_lPCMIFLVMTimer.Remove (i);

            // BUGBUG - may also need a function to adjust the timer-speed times for an object

         // pull out the relevent information...
         GUID gCall = pCur->m_gCall;
         DWORD dwCallID = pCur->m_dwCallID;
         PCMIFLVarList plOrig = pCur->m_varParams.GetList();
         PCMIFLVarList pl = plOrig ? plOrig->Clone () : NULL;   // so that if callee changes then done affect
         if (plOrig)
            plOrig->Release();
         if (!pl)
            pCur->m_fRepeating = FALSE;   // error, this will cause to delete

         // add this back in?
         if (pCur->m_fRepeating) {
            pCur->m_fTimeLeft += pCur->m_fTimeRepeat;
            TimerAdd (pCur);
         }
         else {
            // inform the object that it's been deleted
            PCMIFLVMObject po = ObjectFind (&pCur->m_gBelongsTo);
            if (po)
               po->TimerRemovedByVM();

            // delelete
            delete pCur;
         }

         // call into the function/method...
         DWORD dwRet;
         if (pl) {
            // inform the app that the timer is about to be called
            m_pSocket->VMTimerToBeCalled (this);

            // just in case pl == NULL...
            if (IsEqualGUID(gCall, GUID_NULL))
               dwRet = FunctionCall (dwCallID, pl, &varRet);
            else
               dwRet = MethodCall (&gCall, dwCallID, pl, 0, 0, &varRet);

            // release
            pl->Release();
         }

         if (dwRet)
            return FALSE;  // some sort of error

         // since just mucked with timers, get new values
         dwNum = m_lPCMIFLVMTimer.Num();
         ppt = (PCMIFLVMTimer*)m_lPCMIFLVMTimer.Get(0);
      } // i
   } // repeat while timers still need to go off

#if 0 // old code
   while (dwNum && (ppt[0]->m_fTimeLeft <= 0)) {
      // remove this timer from the list...
      PCMIFLVMTimer pCur = ppt[0];
      m_lPCMIFLVMTimer.Remove (0);

      // pull out the relevent information...
      GUID gCall = pCur->m_gCall;
      DWORD dwCallID = pCur->m_dwCallID;
      PCMIFLVarList plOrig = pCur->m_varParams.GetList();
      PCMIFLVarList pl = plOrig ? plOrig->Clone () : NULL;   // so that if callee changes then done affect
      if (plOrig)
         plOrig->Release();
      if (!pl)
         pCur->m_fRepeating = FALSE;   // error, this will cause to delete

      // add this back in?
      if (pCur->m_fRepeating) {
         pCur->m_fTimeLeft += pCur->m_fTimeRepeat;
         TimerAdd (pCur);
      }
      else {
         // inform the object that it's been deleted
         PCMIFLVMObject po = ObjectFind (&pCur->m_gBelongsTo);
         if (po)
            po->TimerRemovedByVM();

         // delelete
         delete pCur;
      }

      // call into the function/method...
      DWORD dwRet;
      if (pl) {
         // inform the app that the timer is about to be called
         m_pSocket->VMTimerToBeCalled (this);

         // just in case pl == NULL...
         if (IsEqualGUID(gCall, GUID_NULL))
            dwRet = FunctionCall (dwCallID, pl, &varRet);
         else
            dwRet = MethodCall (&gCall, dwCallID, pl, 0, 0, &varRet);

         // release
         pl->Release();
      }

      if (dwRet)
         return FALSE;  // some sort of error

      // since just mucked with timers, get new values
      dwNum = m_lPCMIFLVMTimer.Num();
      ppt = (PCMIFLVMTimer*)m_lPCMIFLVMTimer.Get(0);
   }; // while timers
#endif
   // do the cleanup
   MaintinenceDeleteAll ();

   return TRUE;
}



/*****************************************************************************
VMTimer - Receives the timer messages
*/
VOID CALLBACK VMTimer(
  HWND hwnd,         // handle to window
  UINT uMsg,         // WM_TIMER message
  UINT_PTR idEvent,  // timer identifier
  DWORD dwTime       // current system time
)
{
   PTIDTOVM pt = (PTIDTOVM)glTIDTOVM.Get(0);
   PCMIFLVM pVM = NULL;
   DWORD i;
   for (i = 0; i < glTIDTOVM.Num(); i++, pt++)
      if (pt->dwTimerID == idEvent) {
         pVM = pt->pVM;
         break;
      }

   if (pVM)
      pVM->TimerCallback ();
}


/*****************************************************************************
CMIFLVM::TimerAutomatic - Sets the interval for the automatic timer, in
milliseconds. If this is set to 0, the timer is shut down.

This is called by the main app to set the timer. The timer starts out as
being turned off.

inputs
   DWORD          dwInterval - Interval in milliseonds. If 0 then kill timer
returns
   none
*/
void CMIFLVM::TimerAutomatic (DWORD dwInterval)
{
   // kill the automatic timer, no matter what
   if (m_dwTimerAutoID) {
      KillTimer (NULL, m_dwTimerAutoID);

      // kill it from the list
      PTIDTOVM pt = (PTIDTOVM)glTIDTOVM.Get(0);
      DWORD i;
      for (i = 0; i < glTIDTOVM.Num(); i++, pt++)
         if (pt->dwTimerID == m_dwTimerAutoID) {
            glTIDTOVM.Remove (i);
            break;
         }

      m_dwTimerAutoID = 0;
   }
   else
      QueryPerformanceCounter (&m_iPerCountLast);   // since just turned on

   if (!dwInterval)
      return;  // no more timer

   // else, set timer
   TIDTOVM tt;
   m_dwTimerAutoID = tt.dwTimerID = SetTimer (NULL, (UINT_PTR) this, dwInterval, VMTimer);
   tt.pVM = this;

   // write this in the list
   if (!glTIDTOVM.Num())
      glTIDTOVM.Init (sizeof(TIDTOVM));
   glTIDTOVM.Add (&tt);
}


/*****************************************************************************
CMIFLVM::TimerCallback - Called by the VMTimer callback. This should only
be called by the callback
*/
void CMIFLVM::TimerCallback (void)
{
   if (!m_lPCMIFLVMTimer.Num()) {
      QueryPerformanceCounter (&m_iPerCountLast);
      return;  // nothing
   }

   LARGE_INTEGER iCur;
   __int64 iDelta;
   QueryPerformanceCounter (&iCur);

   iDelta = *((__int64*)&iCur) - *((__int64*)&m_iPerCountLast);

   double fElapsed = (double)iDelta / (double)*((__int64*)&m_iPerCountFreq);

   // special check... if there's already a window we're supposed to debug on,
   // don't use this because the application won't be expecting it with a timer
   PCEscWindow pAppWindow = m_pDebugWindowApp;
   if (pAppWindow)
      DebugWindowSet (NULL);

   if (MaintinenceTimer (fElapsed))
      m_iPerCountLast = iCur; // success

   // in case brought up window, free that
   if (!m_fDebugUIIn && m_pDebugWindowOwn) {
      delete m_pDebugWindowOwn;
      m_pDebugWindowOwn = NULL;
   }

   if (pAppWindow)
      DebugWindowSet (pAppWindow);
}


/**********************************************************************************
CMIFLVM::GlobalMMLTo - Writes the global values to a MMLNode

inputs
   PCHashPVOID       phString - Hash of string pointer to ID. 0-length. Will be added to.
   PCHashPVoid       phList - Hash of list pointer to ID. 0-lenght. Will be added to
returns
   PCMMLNode2 - Node
*/
static PWSTR gpszVMGlobal = L"VMGlobal";
static PWSTR gpszProp = L"Prop";
static PWSTR gpszGet = L"Get";
static PWSTR gpszSet = L"Set";
static PWSTR gpszVar = L"Var";
static PWSTR gpszDeleted = L"Deleted";
static PWSTR gpszName = L"Name";

PCMMLNode2 CMIFLVM::GlobalMMLTo (PCHashPVOID phString, PCHashPVOID phList)
{
   PCMMLNode2 pNode = new CMMLNode2;
   if (!pNode)
      return NULL;
   pNode->NameSet (gpszVMGlobal);

   // loop through and find globals that are changed or added
   PCMMLNode2 pSub, pSub2;
   DWORD dwPass, i;
   for (dwPass = 0; dwPass < 2; dwPass++) {
      // if dwPass = 0, looping through all proprties in object and finding what changed
      // if dwPass = 1, looping through all properties in merge and findout out which deleted
      PCHashDWORD phThis = dwPass ? &m_pCompiled->m_hGlobalsDefault : &m_hGlobals;
      PCHashDWORD phFind = dwPass ? &m_hGlobals : &m_pCompiled->m_hGlobalsDefault;

      for (i = 0; i < phThis->Num(); i++) {
         // get them
         PCMIFLVarProp pThis = (PCMIFLVarProp) phThis->Get(i);

         // find it in the delta layer?
         PCMIFLVarProp pFind = (PCMIFLVarProp) phFind->Find (pThis->m_dwID);

         BOOL fGet = FALSE, fSet = FALSE, fVar = FALSE;
         if (dwPass) {
            // only care about ones that aren't in the original list (pFind)
            if (pFind)
               continue;
         }
         else {
            // figure out what's different
            if (!pFind)
               fGet = fSet = fVar = TRUE;
            else {
               fGet = (pThis->m_pCodeGet != pFind->m_pCodeGet);
               fSet = (pThis->m_pCodeSet != pFind->m_pCodeSet);
               fVar = pThis->m_Var.Compare (&pFind->m_Var, TRUE, this) ? TRUE : FALSE;

               if (!fGet && !fSet && !fVar)
                  continue;   // nothing changed
            }
         }

         // write it out
         pSub = new CMMLNode2;
         if (!pSub) {
            delete pNode;
            return NULL;
         }
         pSub->NameSet (gpszProp);
         pNode->ContentAdd (pSub);

         if (fGet) {
            pSub2 = CodeGetSetMMLTo (pThis->m_pCodeGet);
            if (pSub2) {
               pSub2->NameSet (gpszGet);
               pSub->ContentAdd (pSub2);
            }
         }
         if (fSet) {
            pSub2 = CodeGetSetMMLTo (pThis->m_pCodeSet);
            if (pSub2) {
               pSub2->NameSet (gpszSet);
               pSub->ContentAdd (pSub2);
            }
         }
         if (fVar) {
            pSub2 = pThis->m_Var.MMLTo (this, phString, phList);
            if (pSub2) {
               pSub2->NameSet (gpszVar);
               pSub->ContentAdd (pSub2);
            }
         } // if fVar
         if (dwPass)
            MMLValueSet (pSub, gpszDeleted, (int)TRUE);

         // get the name
         PWSTR psz = NULL;
         if (pThis->m_dwID >= VM_CUSTOMIDRANGE) {
            DWORD dwIndex = m_hUnIdentifiersCustomGlobal.FindIndex (pThis->m_dwID);
            psz = m_hIdentifiersCustomGlobal.GetString (dwIndex);
         }
         else {
            PMIFLIDENT pmi = (PMIFLIDENT) m_pCompiled->m_hGlobals.Find (pThis->m_dwID);
            if (pmi && (pmi->dwType == MIFLI_GLOBAL))
               psz = (PWSTR) ((PCMIFLProp)pmi->pEntity)->m_memName.p;
            else if (pmi && (pmi->dwType == MIFLI_OBJECT))
               psz = (PWSTR) ((PCMIFLObject)pmi->pEntity)->m_memName.p;
         }
         if (!psz || !psz[0]) {
            delete pNode;
            return NULL;   // cant add this. shouldn happen
         }

         // write out the name
         MMLValueSet (pSub, gpszName, psz);
      } // i, over m_hProp
   } // dwPass

   return pNode;
}

/**********************************************************************************
CMIFLVM::GlobalMMLFrom - Initializes the global values to create a copy of the
object specified by pObject.

NOTE: This assumes that the list of globals, m_hGlobals, is empty...

inputs
   BOOL              fBlankSlate - if true starting from a blank slate
   PCMMLNode2            pNode - Node to get from
   PCHashDWORD       phString - Hash of string pointer to ID. Each element is
                     a PCMIFLVarString.
   PCHashDWORD       phList - Hash of list pointer to ID. Each element is a pointer
                     to a PCMIFLVarString
   PCHashGUID        phObjectRemap - Hash is of original object ID. Contents are GUID of new ID
returns
   BOOL - TRUE if success
*/
BOOL CMIFLVM::GlobalMMLFrom (BOOL fBlankSlate, PCMMLNode2 pNode, PCHashDWORD phString, PCHashDWORD phList,
                             PCHashGUID phObjectRemap)
{
   // BUGFIX - Would fail if there were any globals. Instead, this will
   // copy the globals from default if there globals, otherwise it just
   // loads them. Made changes so could do better save/load
   DWORD i;
   if (fBlankSlate && !m_hGlobals.Num()) {
      // copy all the variables over, and then fracture them
      m_pCompiled->m_hGlobalsDefault.CloneTo (&m_hGlobals);
      for (i = 0; i < m_hGlobals.Num(); i++) {
         PCMIFLVarProp pv = (PCMIFLVarProp)m_hGlobals.Get(i);
         pv->m_Var.Fracture(FALSE);

         // DOCUMENT: Potential problems fracture might cause on initialized variables.
         // If two lists somehow point to the same list they will no longer be
         // pointing to the same list after the fracture call
      } // i
   }

   PCMMLNode2 pSub, pSub2;
   CMIFLVarProp vDef;
   PWSTR psz;
   for (i = 0; i < pNode->ContentNum(); i++) {
      pSub = NULL;
      pNode->ContentEnum (i, &psz, &pSub);
      if (!pSub)
         continue;
      psz = pSub->NameGet();
      if (!psz)
         continue;

      if (!_wcsicmp(psz, gpszProp)) {
         // get the name, and potentially the object..
         PWSTR pszName = MMLValueGet (pSub, gpszName);
         if (!pszName)
            continue;   // error

         // find the ID
         DWORD dwID = ToGlobalID (pszName, TRUE);
         if (dwID == -1)
            continue;   // shouldnt happen

         // is this to be deleted
         if (MMLValueGetInt (pSub, gpszDeleted, 0)) {
            DWORD dwIndex = m_hGlobals.FindIndex (dwID);
            if (dwIndex != -1) {
               PCMIFLVarProp pvp = (PCMIFLVarProp) m_hGlobals.Get(dwIndex);
               pvp->m_Var.SetUndefined ();   // so ref count ok
               m_hGlobals.Remove (dwIndex);
            }
            continue;
         }

         // if not exist then create
         PCMIFLVarProp pvp = (PCMIFLVarProp) m_hGlobals.Find (dwID);
         if (!pvp) {
            vDef.m_Var.SetUndefined();
            vDef.m_dwID = dwID;
            vDef.m_pCodeGet = vDef.m_pCodeSet = NULL;
            pvp = &vDef;
         }

         // potentially change...

         // get
         pSub2 = NULL;
         pSub->ContentEnum (pSub->ContentFind(gpszGet), &psz, &pSub2);
         if (pSub2)
            // have get code
            pvp->m_pCodeGet = (PCMIFLCode) CodeGetSetMMLFrom (pSub2, this, FALSE);

         // set
         pSub2 = NULL;
         pSub->ContentEnum (pSub->ContentFind(gpszSet), &psz, &pSub2);
         if (pSub2)
            // have get code
            pvp->m_pCodeSet = (PCMIFLCode) CodeGetSetMMLFrom (pSub2, this, FALSE);

         // value
         pSub2 = NULL;
         pSub->ContentEnum (pSub->ContentFind(gpszVar), &psz, &pSub2);
         if (pSub2)
            // have get code
            pvp->m_Var.MMLFrom (pSub2, this, phString, phList, phObjectRemap);


         // add?
         if (pvp == &vDef) {
            vDef.m_Var.AddRef ();   // so extra count
            m_hGlobals.Add (dwID, pvp);
         }
         continue;
      } // variable
   } // i, over content

   return TRUE;
}



/**********************************************************************************
CMIFLVM::ObjectsMMLTo - Writes the objects to a MMLNode

inputs
   BOOL              fAll - If TRUE then start with all objects and exclude those on the list,
                           else if FALSE then start with no objects and add those on the list
   BOOL              fGlobalsAndDel - If TRUE (and if fAll) then save all the globals, AS WELL AS
                           indicators for objects that are deleted
   PCHashGUID        phExclude - Hash of GUIDs to exclude (or include if !fAll)
   PCHashPVOID       phString - Hash of string pointer to ID. 0-length. Will be added to.
   PCHashPVoid       phList - Hash of list pointer to ID. 0-lenght. Will be added to
returns
   PCMMLNode2 - Node
*/
static PWSTR gpszVMObjects = L"VMObjects";
static PWSTR gpszObject = L"Object";

PCMMLNode2 CMIFLVM::ObjectsMMLTo (BOOL fAll, BOOL fGlobalsAndDel, PCHashGUID phExclude, 
                                 PCHashPVOID phString, PCHashPVOID phList)
{
   PCMMLNode2 pNode = new CMMLNode2;
   if (!pNode)
      return NULL;
   pNode->NameSet (gpszVMObjects);

   DWORD i;
   PCMMLNode2 pSub;
   if (fAll) {
      for (i = 0; i < m_hObjects.Num(); i++) {
         PCMIFLVMObject pObject = *((PCMIFLVMObject*) m_hObjects.Get(i));

         // exclude?
         if (-1 != phExclude->FindIndex (&pObject->m_gID))
            continue;

         // write it...
         pSub = pObject->MMLTo (phString, phList);
         if (!pSub) {
            delete pNode;
            return NULL;
         }
         pSub->NameSet (gpszObject);
         pNode->ContentAdd (pSub);
      } // i, all objects

      // loop through all the objects and note which are deleted
      if (fGlobalsAndDel) for (i = 0; i < m_pCompiled->m_hObjectIDs.Num(); i++) {
         PMIFLIDENT pmi = (PMIFLIDENT)m_pCompiled->m_hObjectIDs.Get(i);
         PCMIFLObject pObject = (PCMIFLObject)pmi->pEntity;

         // NOTE: NOT checking to see if a deleted item is on the exlcude list

         // if it still exists then ignore
         if (-1 != m_hObjects.FindIndex (&pObject->m_gID))
            continue;

         // else, was deleted
         pSub = pNode->ContentAddNewNode ();
         if (!pSub) {
            delete pNode;
            return NULL;
         }
         pSub->NameSet (gpszDeleted);
         MMLValueSet (pSub, gpszName, (PBYTE)&pObject->m_gID, sizeof(pObject->m_gID));
      } // i
   } // if fAll
   else {
      // only used the include list to save...
      for (i = 0; i < phExclude->Num(); i++) {
         GUID *pg = phExclude->GetGUID (i);
         PCMIFLVMObject *ppo = (PCMIFLVMObject*) m_hObjects.Find (pg);
         if (!ppo)
            continue;
         PCMIFLVMObject pObject = ppo[0];

         // write it...
         pSub = pObject->MMLTo (phString, phList);
         if (!pSub) {
            delete pNode;
            return NULL;
         }
         pSub->NameSet (gpszObject);
         pNode->ContentAdd (pSub);
      } // i, all include

   } // if !fAll

   // done
   return pNode;
}



/**********************************************************************************
CMIFLVM::ObjectsMMLFromRemap - This scans through the objects and determines remaps
for any. If should be called before ObjectsMMLFrom() so the remap exists.

inputs
   BOOL              fRemap - If TRUE then if an object has a conflicting
                        GUID with an existing object, the GUID will be remapped.
                        If FALSE then it will be replaced.
   BOOL              fRemapErr - If forced to remap (and !fRemap) then error.

   PCMMLNode2            pNode - Node to get from
   PCHashDWORD       phString - Hash of string pointer to ID. Each element is
                     a PCMIFLVarString.
   PCHashDWORD       phList - Hash of list pointer to ID. Each element is a pointer
                     to a PCMIFLVarString
   PCHashGUID        phObjectRemap - Hash is of original object ID. Contents are GUID of new ID
                        NOTE: This initializes and fills in phObjectRemap
returns
   BOOL - TRUE if success
*/
static PWSTR gpszID = L"ID";

BOOL CMIFLVM::ObjectsMMLFromRemap (BOOL fRemap, BOOL fRemapErr,
                              PCMMLNode2 pNode, PCHashGUID phObjectRemap)
{
   phObjectRemap->Init (sizeof(GUID));

   if (!m_hObjects.Num())  // BUGFIX - Was testing for fAll, but no longer use that way
      return TRUE;   // no need to remap since loading in bulk

   // if there's no remapping to be done, and there's no remap error, then allow
   // everything to pass
   if (!fRemap && !fRemapErr)
      return TRUE;

   DWORD i;
   PWSTR psz;
   PCMMLNode2 pSub;

   // loop through all the contents and either delete, or look for remaps, depeindg on fAll
   for (i = 0; i < pNode->ContentNum(); i++) {
      pSub = NULL;
      pNode->ContentEnum (i, &psz, &pSub);
      if (!pSub)
         continue;
      psz = pSub->NameGet();
      if (!psz)
         continue;

      if (!_wcsicmp(psz, gpszObject)) {
         // get the guid...
         GUID g;
         if (sizeof(g) != MMLValueGetBinary (pSub, gpszID, (PBYTE)&g, sizeof(g)))
            continue;

         // see if it already exists
         if (-1 == m_hObjects.FindIndex (&g))
            continue;   // doesnt exist

         // otherwise, it already exists, so make a new object
         if (!fRemap)
            return FALSE;  // dont want to remap, so will cause an error instead
               // fRemapErr guaranteed to be TRUE here

         GUID gNew;
         GUIDGen (&gNew);
         phObjectRemap->Add (&g, &gNew);
      }
   } // i, content

   return TRUE;
}

/**********************************************************************************
CMIFLVM::ObjectsMMLFrom - Loads in several objects saved with ObjectsMMLTo.
If fAll is set then this assumes the list of objects (m_hObjects) is empty.

inputs
   BOOL              fBlankSlate - If blank slate starting from empty list
   BOOL              fRemap - TRUE if remapping objects. If objects are remapped
                        then DONT delete any based on the "Delete" tags.

   PCMMLNode2            pNode - Node to get from
   PCHashDWORD       phString - Hash of string pointer to ID. Each element is
                     a PCMIFLVarString.
   PCHashDWORD       phList - Hash of list pointer to ID. Each element is a pointer
                     to a PCMIFLVarString
   PCHashGUID        phObjectRemap - Hash is of original object ID. Contents are GUID of new ID
                        NOTE: Should already have been created by ObjectsMMLFromRemap()
   PCHashGUID        phObjectAdded - Hash is cleared and filled in with a list of objects that
                        have been added (and which will need to be called by Constructor2)
returns
   BOOL - TRUE if success
*/

BOOL CMIFLVM::ObjectsMMLFrom (BOOL fBlankSlate, BOOL fRemap, PCMMLNode2 pNode, PCHashDWORD phString, PCHashDWORD phList,
                             PCHashGUID phObjectRemap, PCHashGUID phObjectAdded)
{
   // if fAll then initialize all the objects in m_hObjects to their default
   DWORD i;
   PWSTR psz;
   PCMMLNode2 pSub;
   phObjectAdded->Init (0);
   BOOL fJustCreated = FALSE;
   if (fBlankSlate) {
      fJustCreated = TRUE;

      // if get here then loading from a blank VM, so create
      if (m_hObjects.Num())
         return FALSE;

      // go through all the objects and create ones
      m_hObjects.TableResize (m_pCompiled->m_hObjectIDs.Num()*3);
      for (i = 0; i < m_pCompiled->m_hObjectIDs.Num(); i++) {
         PMIFLIDENT pmi = (PMIFLIDENT)m_pCompiled->m_hObjectIDs.Get(i);
         if (pmi->dwType != MIFLI_OBJECT)
            continue;   // shouldnt happen

         // create
         PCMIFLVMObject pNew = new CMIFLVMObject;
         if (!pNew)
            return FALSE;
         if (!pNew->InitAsAutoCreate (this, (PCMIFLObject)pmi->pEntity)) {
            delete pNew;
            return FALSE;
         }

         // add this
         m_hObjects.Add (&pNew->m_gID, &pNew);
      } // i
   } // if fAll


   // loop through all the contents and either delete
   // BUGFIX - Had a check for fAll before, but dont need
   if (!fRemap) for (i = 0; i < pNode->ContentNum(); i++) {
      pSub = NULL;
      pNode->ContentEnum (i, &psz, &pSub);
      if (!pSub)
         continue;
      psz = pSub->NameGet();
      if (!psz)
         continue;

      if (!_wcsicmp(psz, gpszDeleted)) {
         // deleted an object
         GUID g;
         if (sizeof(g) != MMLValueGetBinary (pSub, gpszName, (PBYTE)&g, sizeof(g)))
            continue;

         // delete this
         DWORD dwIndex = m_hObjects.FindIndex (&g);
         if (dwIndex != -1) {
            if (fJustCreated) {
               // quick delete
               PCMIFLVMObject po = *((PCMIFLVMObject*)m_hObjects.Get(dwIndex));
               delete po;  // NOTE: doing dirty delete
               m_hObjects.Remove (dwIndex);
            }
            else
               ObjectDelete (&g);   // slower delete because was preexisting
                  // NOTE: Slower delete not yet tested although should work
         }
      }
   } // i, content

   // read all the objects in...
   CListFixed lContainedIn;
   lContainedIn.Init (sizeof(GUID)*2);
   for (i = 0; i < pNode->ContentNum(); i++) {
      pSub = NULL;
      pNode->ContentEnum (i, &psz, &pSub);
      if (!pSub)
         continue;
      psz = pSub->NameGet();
      if (!psz)
         continue;

      if (!_wcsicmp(psz, gpszObject)) {
         PCMIFLVMObject pObject = new CMIFLVMObject;
         if (!pObject)
            return FALSE;  // error

         if (!pObject->MMLFrom (pSub, this, phString, phList, phObjectRemap)) {
            delete pObject;
            continue;   // ignore the error
         }

         // if the object already exists then delete it. NOTE: Only need to
         // do this if fAll
         DWORD dwIndex = m_hObjects.FindIndex (&pObject->m_gID);
         if (dwIndex != -1) {
            if (fJustCreated) {
               // can do fast delete
               PCMIFLVMObject po = *((PCMIFLVMObject*)m_hObjects.Get(dwIndex));
               delete po;  // NOTE: doing dirty delete
               m_hObjects.Remove (dwIndex);
            }
            else {
               // replace what's already there, moving what's there onto the
               // to-delete list
               if (!ObjectReplace (&pObject->m_gID, pObject, &lContainedIn))
                  return FALSE;  // error
               phObjectAdded->Add (&pObject->m_gID, NULL);  // so know that constructor needs to get called
               continue;
            }
         }

         // add it back in
         m_hObjects.Add (&pObject->m_gID, &pObject);
         phObjectAdded->Add (&pObject->m_gID, NULL);
      } // if object add node
   } // i, content

   // BUGFIX: go through and fill in the contained in
   GUID *pgCont = (GUID*)lContainedIn.Get(0);
   for (i = 0; i < lContainedIn.Num(); i++, pgCont += 2) {
      PCMIFLVMObject *ppo = (PCMIFLVMObject*)m_hObjects.Find (pgCont + 0);
      if (ppo)
         ppo[0]->m_gContainedIn = pgCont[1];  // will revamp this below, so no
                  // need to set everything straight
   } // i

   // go through and rebuild the contained list... need to do this to make
   // sure it never gets broken
   lContainedIn.Init (sizeof(GUID));
   for (i = 0; i < m_hObjects.Num(); i++) {
      PCMIFLVMObject *ppo = (PCMIFLVMObject*)m_hObjects.Get(i);
      if (!ppo)
         continue;   // shouldnt happen
      PCMIFLVMObject po = ppo[0];
      lContainedIn.Add (&po->m_gContainedIn);

      // clear out contained in and contains list
      po->m_gContainedIn = GUID_NULL;
      po->m_lContains.Clear();
   } // i
   pgCont = (GUID*)lContainedIn.Get(0);
   for (i = 0; i < lContainedIn.Num(); i++, pgCont++) {
      PCMIFLVMObject *ppo = (PCMIFLVMObject*)m_hObjects.Get(i);
      if (!ppo)
         continue;   // shouldnt happen
      PCMIFLVMObject po = ppo[0];
      po->ContainedBySet (pgCont);
   } // i, add back in contained

   // done
   return TRUE;
}


/**********************************************************************************
CMIFLVM::ObjectClone - This clones one or more objects, along with all the
children of the objects.

inputs
   GUID              *pag - GUID(s) of objects
   DWORD             dwNum - Number of objects in the list
   PWSTR             pszProperty - Property to set to the value. If NULL no property set.
                        Would set something like "pClone" to pVarProperty so know it's a clone
                        when Constructor2() is called.
   PCMIFLVar         pVarProperty - Value to set to the property.
returns
   PCHashGUID - Hash that converts from original object guids into new object guids.
*/
PCHashGUID CMIFLVM::ObjectClone (GUID *pag, DWORD dwNum, PWSTR pszProperty, PCMIFLVar pVarProperty)
{
   // hash for original to new guid
   PCHashGUID phOrigToNew = new CHashGUID;
   if (!phOrigToNew)
      return NULL;
   phOrigToNew->Init (sizeof(GUID), dwNum * 4);

   // hash for new guid to original
   //CHashGUID hNewToOrig;
   //hNewToOrig.Init (sizeof(GUID), dwNum * 4);

   // figure out all the children
   CListFixed  lLook;
   lLook.Init (sizeof(GUID), pag, dwNum);

   // loop and make sure have children
   DWORD i, j;
   GUID gNew;
   pag = (GUID*)lLook.Get(0);
   for (i = 0; i < lLook.Num(); i++, pag++) {
      PCMIFLVMObject po = ObjectFind (pag);
      if (!po)
         continue;

      // make up a GUID for this and add it
      GUIDGen (&gNew);
      phOrigToNew->Add (pag, &gNew);
      //hNewToOrig.Add (&gNew, pag);

      // all children
      GUID *pagChild = (GUID*)po->m_lContains.Get(0);
      DWORD dwNum = po->m_lContains.Num();
      for (j = 0; j < dwNum; j++, pagChild++) {
         // if child not already on the exclude list then add it
         if (-1 == phOrigToNew->FindIndex (pagChild)) {
            lLook.Add (pagChild);
            pag = (GUID*)lLook.Get(i); // since list length may have changed
         }
      } // j
   } // i

   // loop throug all the objects and clone
   pag = (GUID*)lLook.Get(0);
   for (i = 0; i < lLook.Num(); i++, pag++) {
      PCMIFLVMObject po = ObjectFind (pag);
      if (!po)
         continue;

      // find the new ID
      gNew = *((GUID*) phOrigToNew->Find (&po->m_gID));

      PCMIFLVMObject pNew = po->Clone (&gNew, phOrigToNew);
      if (!pNew)
         continue;   // shouldnt happen

      // add
      m_hObjects.Add (&pNew->m_gID, &pNew);
   }

   // loop through all the timers and duplicate timers for clones
   GUID *pgNew;
   for (i = m_lPCMIFLVMTimer.Num()-1; i < m_lPCMIFLVMTimer.Num(); i--) {
      PCMIFLVMTimer pt = *((PCMIFLVMTimer*)m_lPCMIFLVMTimer.Get(i));

      // see if it belongs to a cloned object
      pgNew = (GUID*)phOrigToNew->Find(&pt->m_gBelongsTo);
      if (!pgNew)
         continue;

      // else, matches, so clone
      pt = pt->Clone();
      if (!pt)
         continue;   // shouldnt happen

      // clone already fractured, but remap
      pt->m_gBelongsTo = *pgNew;
      GUID *pgRemap = (GUID*)phOrigToNew->Find (&pt->m_gCall);
      if (pgRemap)
         pt->m_gCall = *pgRemap;
      pt->m_varName.Remap (phOrigToNew);
      pt->m_varParams.Remap (phOrigToNew);

      // insert this afterwards
      m_lPCMIFLVMTimer.Insert (i+1, &pt);
   } // i


   // set the property so know it's a clone
   DWORD dwID = (DWORD)-1;
   if (pszProperty)
      dwID = ToPropertyID (pszProperty, TRUE);
   if (dwID != -1) for (i = 0; i < phOrigToNew->Num(); i++) {
      pgNew = (GUID*)phOrigToNew->Get (i);
      if (!pgNew)
         continue;   // shouldnt happen

      PCMIFLVMObject po = ObjectFind (pgNew);
      if (!po)
         continue;

      PropertySet (dwID, po, TRUE, pVarProperty);
   } // i

   // call constructo2
   CMIFLVarLValue var;
   for (i = 0; i < phOrigToNew->Num(); i++) {
      pgNew = (GUID*)phOrigToNew->Get (i);
      if (!pgNew)
         continue;   // shouldnt happen

      MethodCallVMTOK (pgNew, VMTOK_CONSTRUCTOR2, NULL, 0, 0, &var);
   } // i

   // done
   return phOrigToNew;
}

/**********************************************************************************
CMIFLVM::MMLTo - Writes the objects to a MMLNode

inputs
   BOOL              fAll - If TRUE then start with all objects and exclude those on the list,
                           else if FALSE then start with no objects and add those on the list
   BOOL              fGlobalsAndDel - If TRUE (and if fAll) then save all the globals, AS WELL AS
                           indicators for objects that are deleted
   GUID              *pagExclude - Pointer to list of GUIDs to exclude (if fAll) or include (!fAll)
                           from the list
   DWORD             dwExcludeNum - Number in the exclude list
   BOOL              fExcludeChildren - If TRUE then include children in the exclusion list
returns
   PCMMLNode2 - Node
*/
static PWSTR gpszVM = L"VM";
static PWSTR gpszAll = L"All";
static PWSTR gpszStrings = L"Strings";
static PWSTR gpszLists = L"Lists";

PCMMLNode2 CMIFLVM::MMLTo (BOOL fAll, BOOL fGlobalsAndDel, GUID *pagExclude, DWORD dwExcludeNum, BOOL fExcludeChildren)
{
   // NOTE: DO NOT change this code without first checking against MIFServer
   // where the database reads into the MML to get the values for the attributes

   PCMMLNode2 pNode = new CMMLNode2;
   if (!pNode)
      return NULL;
   pNode->NameSet (gpszVM);

   // create the exclude list
   CHashGUID hExclude;
   DWORD i, j;
   hExclude.Init (0, dwExcludeNum * 3);
   for (i = 0; i < dwExcludeNum; i++) {
      if (-1 !=m_hObjects.FindIndex (pagExclude + i))
         hExclude.Add (pagExclude + i, 0);
   }

   // if excluding children then do that
   if (fExcludeChildren) {
      for (i = 0; i < hExclude.Num(); i++) {
         PCMIFLVMObject po = ObjectFind (hExclude.GetGUID(i));
         if (!po)
            continue;

         // all children
         GUID *pagChild = (GUID*)po->m_lContains.Get(0);
         DWORD dwNum = po->m_lContains.Num();
         for (j = 0; j < dwNum; j++, pagChild++) {
            // if child not already on the exclude list then add it
            if (-1 == hExclude.FindIndex (pagChild))
               hExclude.Add (pagChild, 0);
         } // j

      } // i, over all hExclude
   } // if exclude children

   // make up some hashes to store the strings that need...
   CHashPVOID hString, hList;
   hString.Init (0);
   hList.Init (0);

   // write value of fAll
   MMLValueSet (pNode, gpszAll, (int)fAll);

   // write all the globals...
   PCMMLNode2 pSub;
   if (fAll && fGlobalsAndDel) {
      pSub = GlobalMMLTo (&hString, &hList);
      if (!pSub) {
         delete pNode;
         return FALSE;
      }
      pNode->ContentAdd (pSub);
   }

   // write all the objects
   pSub = ObjectsMMLTo (fAll, fGlobalsAndDel, &hExclude, &hString, &hList);
   if (!pSub) {
      delete pNode;
      return FALSE;
   }
   pNode->ContentAdd (pSub);

   // write out all the strings and lists
   pSub = CMIFLVarListMMLTo (&hList, this, &hString);
   if (!pSub) {
      delete pNode;
      return FALSE;
   }
   pSub->NameSet (gpszLists);
   pNode->ContentAdd (pSub);

   // BUGFIX - write out strings after lists
   pSub = CMIFLVarStringMMLTo (&hString);
   if (!pSub) {
      delete pNode;
      return FALSE;
   }
   pSub->NameSet (gpszStrings);
   pNode->ContentAdd (pSub);

   // done
   return pNode;
}


/**********************************************************************************
CMIFLVM::MMLFrom - Loads in several objects saved with ObjectsMMLTo.
If fAll is set then this assumes the list of objects (m_hObjects) is empty.

NOTE: Debugging flags should have set before calling this since it will
call the constructors

inputs
   BOOL              fBlankSlate - If TRUE then starting with a blank slate and
                        should initialize as go. If FALSE then have stuff

   BOOL              fAll - If TRUE then loading in all objects AND globals. If FALSE then
                        loading in a group of objects on top of existing ones.

   BOOL              fRemap - If TRUE then if an object has a conflicting
                        GUID with an existing object, the GUID will be remapped.
                        If FALSE then the object will be replaced.
                        
   BOOL              fRemapErr - If has to remap, and this is TRUE then will return
                        an error.
   PCHashGUID        *pphRemap - If this is not NULL, and the MMLFrom is successful.
                        this will be filled with a pointer to a CHashGUID object
                        that maps from the original GUID to the new GUID (as it's loaded
                        in). This PCHashGUID must be freed by the caller.

   PCMMLNode2            pNode - Node to get from
returns
   BOOL - TRUE if success
*/
BOOL CMIFLVM::MMLFrom (BOOL fBlankSlate, BOOL fAll, BOOL fRemap, BOOL fRemapErr, PCMMLNode2 pNode,
                       PCHashGUID *pphRemap)
{
   if (pphRemap)
      *pphRemap = NULL;

   // NOTE: DO NOT change this code without first checking against MIFServer
   // where the database reads into the MML to get the values for the attributes

   // get the all flag to test that expecting th eright type
   // BUGFIX - Dont use fText
   //BOOL fTest = (BOOL)MMLValueGetInt (pNode, gpszAll, 0);
   //if (!fTest != !fAll)
   //   return FALSE;  // wrong types

   // BUGFIX - Dont need to test if no objects because other code handles
   if (fBlankSlate && m_hObjects.Num())
      return FALSE;  // shouldnt happen

   // load in the strings and lists
   BOOL fRet = TRUE;
   DWORD i;
   CMIFLVarLValue var;
   PCHashDWORD phString = NULL, phList = NULL;
   CHashGUID hObjectAdded;
   PCHashGUID phObjectRemap = new CHashGUID;
   if (!phObjectRemap)
      return FALSE;
   PCMMLNode2 pSub;
   PWSTR psz;
   pSub = NULL;
   pNode->ContentEnum (pNode->ContentFind (gpszStrings), &psz, &pSub);
   if (pSub)
      phString = CMIFLVarStringMMLFrom (pSub);
   if (!phString) {
      fRet = FALSE;
      goto done;
   }

   // find the object remap, since will need it when loading in lists
   pSub = NULL;
   pNode->ContentEnum (pNode->ContentFind (gpszVMObjects), &psz, &pSub);
   if (!pSub) {
      fRet = FALSE;
      goto done;
   }
   if (!ObjectsMMLFromRemap (fRemap, fRemapErr, pSub, phObjectRemap)) {
      fRet = FALSE;
      goto done;
   }

   // get the lists
   pSub = NULL;
   pNode->ContentEnum (pNode->ContentFind (gpszLists), &psz, &pSub);
   if (pSub)
      phList = CMIFLVarListMMLFrom (pSub, this, phString, phObjectRemap);
   if (!phList) {
      fRet = FALSE;
      goto done;
   }

   // read in globals
   if (fAll) {
      pSub = NULL;
      pNode->ContentEnum (pNode->ContentFind (gpszVMGlobal), &psz, &pSub);
      // NOTE: Will only have globals if fAll is TRUE
      if (pSub && !GlobalMMLFrom (fBlankSlate, pSub, phString, phList, phObjectRemap)) {
         fRet = FALSE;
         goto done;
      }
   }

   // read in all the objects
   pSub = NULL;
   pNode->ContentEnum (pNode->ContentFind (gpszVMObjects), &psz, &pSub);
   if (!pSub) {
      fRet = FALSE;
      goto done;
   }
   if (!ObjectsMMLFrom (fBlankSlate, fRemap, pSub, phString, phList, phObjectRemap, &hObjectAdded)) {
      fRet = FALSE;
      goto done;
   }

   // loop through all the objects and call constructor or constructor2
   if (fBlankSlate) {
      // keep a list of all the objects, since a constructor might cause one
      // to be deleted, and really mess things up
      CListFixed lObject;
      lObject.Init (sizeof(GUID));
      lObject.Required (m_hObjects.Num());
      for (i = 0; i < m_hObjects.Num(); i++)
         lObject.Add (m_hObjects.GetGUID(i));

      GUID *pg = (GUID*)lObject.Get(0);
      for (i = 0; i < lObject.Num(); i++, pg++) {
         MethodCallVMTOK (pg,
            (-1 == hObjectAdded.FindIndex (pg)) ? VMTOK_CONSTRUCTOR : VMTOK_CONSTRUCTOR2,
            NULL, 0, 0, &var);
      } // i
   }
   else {
      // just loop through the objects in the added list
      for (i = 0; i < hObjectAdded.Num(); i++)
         MethodCallVMTOK (hObjectAdded.GetGUID(i), VMTOK_CONSTRUCTOR2, NULL, 0, 0, &var);
   }


done:
   // free up the strings and lists
   if (phString) {
      for (i = 0; i < phString->Num(); i++) {
         PCMIFLVarString ps = *((PCMIFLVarString*)phString->Get(i));
         ps->Release();
      } // i
      delete phString;
   } // phString
   if (phList) {
      for (i = 0; i < phList->Num(); i++) {
         PCMIFLVarList ps = *((PCMIFLVarList*)phList->Get(i));
         ps->Release();
      } // i
      delete phList;
   } // phString

   if (phObjectRemap) {
      if (fRet && pphRemap)
         *pphRemap = phObjectRemap;
      else
         delete phObjectRemap;
   }

   return fRet;
}


// DOCUMENT: How layers work (in general)

// BUGFIX - using m_pFCICur->dwPropIDGetSet for determining if should set/get
// globals is a problem because IDs can overlap with methods. Changed so
// check for (m_pFCICur->pObject==pObject) && (m_pFCICur->dwPropIDGetSet == dwID)




// BUGBUG - GUIDGen() is NOT thread safe. When start making this multithreaded
// will need to protect GUIDGen() with a critical section.


// BUGBUG - when get multithreaded need to but critical section around access to glTIDTOVM


// BUGBUG - When have just a "property = 123;" in the code, and the property has
// not been initialized for the object, complains of a bad l-value.
// But, if that "this.property = 123;" then it works

// BUGBUG - when have "if ((vIndex = pSkillsRacialInnate[i])[0] == Skill) {"
// get unexpected elements in expression, (when compile) even though valid

// BUGBUG - when have "rHello + "test"", gets 0, since not converting rHello to
// a string first.

// BUGBUG - when debug MIFL code, have an option to "ignore all debug" statements
// so can cleanly exit

// BUGBUG - at some point faster execution of timer loop by not decrementing every
// one, but storing absolute time. However, will need to change code that saves/loads
// timer loop to disk. Also might have problems when do MMLTo of timer loop
// due to large numbers

// BUGBUG - rather than one large switch, have an array with pointers. Will
// make code execution much faster