/*************************************************************************************
CMIFLProj.cpp - Code for managing a MIFL project

begun 24/12/03 by Mike Rozak.
Copyright 2003 by Mike Rozak. All rights reserved
*/

#include <windows.h>
#include "escarpment.h"
#include "..\M3D.h"
#include "resource.h"
#include "..\mifl.h"


// HTOCENTRY - Entry in table of contents, for help
typedef struct {
   PWSTR          pszName;       // name of the entry
   PWSTR          pszDescShort;  // short description of the entry
   int            iSlot;         // slot where the help file has ended
} HTOCENTRY, *PHTOCENTRY;

// HTOCNODE - Node in a TOC tree, for help
typedef struct {
   PCBTree        ptHTOCNODE;       // sub-branches of the tree
   PCListFixed    plHTOCENTRY;      // list of HTOCENTRY
} HTOCNODE, *PHTOCNODE;



// HELPSCRATCH - Passed around the help functions
typedef struct {
   int            iSlot;         // current slot to add to. If used, increment by 1
   PHTOCNODE      pTOC;          // node for the TOC
   PCMIFLProj     pProj;         // current project
   PCMIFLLib      pLib;          // library
   PCHashString   phHELPINDEX;   // help index
   PCListVariable plHelp;        // list of help slots
   PCListVariable plHelpIndex;   // list of help slots, but this is what's indexed... missing some extra links
   PCListVariable *papSuper;     // pointer to an array of pLib->ObjectNum() entries. Each enty
                                 // is a PCListVariable containing the exploded list of superclasses

   // lists of slots used by different objects. Each tree entry is an iSlot where it occurs
   PCBTree        ptMethDef;     // meth def name to iSlot
   PCBTree        ptPropDef;
   PCBTree        ptPropGlobal;
   PCBTree        ptFunc;
   PCBTree        ptObject;
} HELPSCRATCH, *PHELPSCRATCH;

// HBI - For sorting
typedef struct {
   PWSTR       pszName;    // name
   PWSTR       pszValue;   // value assiciated
   PVOID       pInfo;      // info associated with name
} HBI, *PHBI;

// HELPINDEX - Storing information for index
#define HIT_METHDEF           0     // method definition
#define HIT_PROPDEF           1     // property definition
#define HIT_PROPGLOBAL        2     // global property
#define HIT_FUNC              3     // function
#define HIT_OBJECT            5     // object
#define HIT_NOTDOCUMENTED     10    // This or above aren't documented
#define HIT_STRING            (HIT_NOTDOCUMENTED+0)    // string table
#define HIT_RESOURCE          (HIT_NOTDOCUMENTED+1)    // resource

typedef struct {
   DWORD       dwType;     // one of HIT_XXX
   int         iSlot;      // slot that this is assocated with. -1 if unknown
   //PCBTree     ptRefersTo; // tree of strings for which other entieis this refers to
   PCBTree     ptReferredFrom;   // tree of strings for which other entities refer to this
   PCListFixed plDOCREFERRED; // document that referred to this
   PCBTree     ptContains; // list of objects that contains
   PCMem       pmemStringText; // if this is a string, pmemStringText will be filled in
} HELPINDEX, *PHELPINDEX;  // index

// DOCREFERRED - Store information about a document that referred to this entity
typedef struct {
   PWSTR       pszDocName;    // document name. Only valid during object construction
   int         iSlot;         // slot that it appears under. -1 if unknown
} DOCREFERRED, *PDOCREFERRED;

void HelpParseStringForReference (PWSTR pszSearch, PCHashString phTokens, PWSTR pszThis, PDOCREFERRED pDoc = NULL);
PWSTR HelpParseStringForToken (PWSTR pszSearch, PCHashString phTokens, BOOL fMustHaveSlot, DWORD *pdwTokenIndex,
                               PWSTR *ppszStringText = NULL);

/**************************************************************************************
CMIFLUndoPacket - Handles undo/redo storage */
typedef struct {
   DWORD             dwObject;            // Type of object.    see below
   DWORD             dwChange;            // chage. 0=changed,1=added,2=removed

   // dwObject == 0, library
   PCMIFLLib         pObject;             // object changed. If Changed/removed this is
                                          // the contents right before changing/removing.
                                          // If Added, this is final result. Flip for Redo buffer

} UNDOCHANGE, *PUNDOCHANGE;

class CMIFLUndoPacket {
public:
   ESCNEWDELETE;

   CMIFLUndoPacket (void);
   ~CMIFLUndoPacket (void);
   void Change (PCMIFLLib pObject, BOOL fBefore, DWORD dwChange);

   CListFixed        m_lUNDOCHANGE;       // list of UNDOCHANGE structures

};
typedef CMIFLUndoPacket *PCMIFLUndoPacket;



// globals

static PWSTR gszPageErrorID = L"r:636";
static PWSTR gszStartingURL = L"r:636";
static PCMIFLProj gpProjHasHelp = NULL;

PWSTR gpszNodeMisc = L"nodemisc";

/**************************************************************************************
NodeAutoLink - Internal function to converts strings with help into links

inputs
   PCMMLNode2     pNode - Node
   PCHashString   phHELPINDEX - Help index
returns
   none
*/
void NodeAutoLink (PCMMLNode2 pNode, PCHashString phHELPINDEX)
{
   // dont do "a" or contents
   PCMMLNode2 pSub;
   PWSTR psz = pNode->NameGet();
   if (psz && !_wcsicmp(psz, L"a"))
      return;

   DWORD i;
   DWORD dwNum = pNode->ContentNum();
   if (!dwNum)
      return;
   CMem memTemp, memString;
   DWORD dwInsertBefore;
   for (i = dwNum-1; i < dwNum; i--) {
      pNode->ContentEnum (i, &psz, &pSub);
      if (pSub) {
         NodeAutoLink (pSub, phHELPINDEX);
         continue;
      }

      // else, have a string, so remove then slowly split up
      MemZero (&memString);
      MemCat (&memString, psz);
      pNode->ContentRemove (i);
      dwInsertBefore = i;

      psz = (PWSTR)memString.p;
      while (psz[0]) {
         // find the next token match
         DWORD dwToken;
         PWSTR pszStringText;
         PWSTR pszMatch = HelpParseStringForToken (psz, phHELPINDEX, TRUE, &dwToken, &pszStringText);

         // if no match then concat the rest and done
         if (!pszMatch) {
            pNode->ContentInsert (dwInsertBefore++, psz);
            break;   // BUGFIX - was return
         }

         PWSTR pszToken = phHELPINDEX->GetString (dwToken);
         DWORD dwLen = (DWORD)wcslen(pszToken);

         // if this is string text then increase pszMatch to AFTER
         // the token
         if (pszStringText)
            pszMatch += dwLen;

         // else, is match, so add to match point
         WCHAR cTemp = pszMatch[0];
         pszMatch[0] = 0;
         if (psz[0])
            pNode->ContentInsert (dwInsertBefore++, psz);
         pszMatch[0] = cTemp;

         if (pszStringText) {
            // show the text in italics
            pSub = new CMMLNode2;
            if (!pSub)
               continue;   // shouldnt happen
            pSub->NameSet (L"italic");

            // create temporary memory to add quotes
            CMem memTemp;
            MemZero (&memTemp);
            MemCat (&memTemp, L" \"");
            MemCat (&memTemp, pszStringText);
            MemCat (&memTemp, L"\"" );
            pSub->ContentAdd ((PWSTR)memTemp.p);
            pNode->ContentInsert (dwInsertBefore++, pSub);

            psz = pszMatch; // not using dwLen since already updated
         }
         else {
            // add link
            pSub = new CMMLNode2;
            if (!pSub)
               continue;   // shouldnt happen
            pSub->NameSet (L"a");

            MemZero (&memTemp);
            MemCat (&memTemp, L"index:");
            MemCat (&memTemp, pszToken);
            pSub->AttribSetString (L"href", (PWSTR)memTemp.p);
            cTemp = pszMatch[dwLen];
            pszMatch[dwLen] = 0;
            pSub->ContentAdd (pszMatch);
            pNode->ContentInsert (dwInsertBefore++, pSub);
            pszMatch[dwLen] = cTemp;

            // continue
            psz = pszMatch + dwLen;
         } // if have link
      } // while (psz)
   } // i
}


/**************************************************************************************
MemCatAutoIndex - Like MemCat(), except autoindexes.

inputs
   PCMem          pMem - Mem to add to
   PWSTR          psz - String to add to. This is MML.
   PCHashString   phHELPINDEX - Help index
returns
   none
*/
void MemCatAutoIndex (PCMem pMem, PWSTR psz, PCHashString phHELPINDEX)
{
   // conver the string to MML
   CEscError err;
   PCMMLNode pNodeOld = ParseMML (psz, ghInstance, NULL, NULL, &err);
   if (!pNodeOld) {
      // error, so add def
      MemCat (pMem, psz);
      return;
   }

   PCMMLNode2 pNode = pNodeOld->CloneAsCMMLNode2();
   delete pNodeOld;
   if (!pNode) {
      // error, so add def
      MemCat (pMem, psz);
      return;
   }

   // repeat conversion
   NodeAutoLink (pNode, phHELPINDEX);

   // write it out
   CMem memTemp;
   if (MMLToMem (pNode, &memTemp, TRUE, 0, FALSE))
      MemCat (pMem, (PWSTR)memTemp.p);

   delete pNode;
}

/**************************************************************************************
MemCatSanitizeAutoIndex - Like MemCatSanitize() except it autoindexes.

inputs
   PCMem          pMem - Mem to cat to
   PWSTR          psz - String to add
   PCHashString   phHELPINDEX - Help index
returns
   none
*/
void MemCatSanitizeAutoIndex (PCMem pMem, PWSTR psz, PCHashString phHELPINDEX)
{
   CMem memTemp;

   // loop over the whole string
   while (psz[0]) {
      // find the next token match
      DWORD dwToken;
      PWSTR pszStringText;
      PWSTR pszMatch = HelpParseStringForToken (psz, phHELPINDEX, TRUE, &dwToken, &pszStringText);

      // if no match then concat the rest and done
      if (!pszMatch) {
         MemCatSanitize (pMem, psz);
         return;
      }

      PWSTR pszToken = phHELPINDEX->GetString (dwToken);
      DWORD dwLen = (DWORD)wcslen(pszToken);

      // if this is pszStringText then increase to after the string
      if (pszStringText)
         pszMatch += dwLen;

      // else, is match, so add to match point
      WCHAR cTemp = pszMatch[0];
      pszMatch[0] = 0;
      if (psz[0])
         MemCatSanitize (pMem, psz);
      pszMatch[0] = cTemp;

      if (pszStringText) {
         // add italics text
         MemZero (&memTemp);
         MemCat (&memTemp, L"<italic> \"");
         MemCatSanitize (&memTemp, pszStringText);
         MemCat (&memTemp, L"\" </italic>");
         MemCat (pMem, (PWSTR)memTemp.p);

         // continue
         psz = pszMatch; // NOTE: No + dwLen;
      }
      else {
         // add link
         MemZero (&memTemp);
         MemCat (&memTemp, L"<a href=\"index:");
         MemCatSanitize (&memTemp, pszToken);
         MemCat (&memTemp, L"\">");
         cTemp = pszMatch[dwLen];
         pszMatch[dwLen] = 0;
         MemCatSanitize (&memTemp, pszMatch);
         pszMatch[dwLen] = cTemp;
         MemCat (&memTemp, L"</a>");
         MemCat (pMem, (PWSTR)memTemp.p);

         // continue
         psz = pszMatch + dwLen;
      } // if adding link

   } // while string
}

/**************************************************************************************
CMIFLUndoPacket::Constructor and destructor. Initializes the object. The destructor
frees all the objects that are there.
*/
CMIFLUndoPacket::CMIFLUndoPacket ()
{
   m_lUNDOCHANGE.Init (sizeof(UNDOCHANGE));
}

CMIFLUndoPacket::~CMIFLUndoPacket ()
{
   DWORD i;
   for (i = 0; i < m_lUNDOCHANGE.Num(); i++) {
      PUNDOCHANGE pu = (PUNDOCHANGE) m_lUNDOCHANGE.Get(i);
      if (pu->pObject)
         delete pu->pObject;
   }
}


/**************************************************************************************
CMIFLUndoPacket::Change - Record a change. As objects are changed by the user this should
be called.

inputs
   PCMIFLLib         pObject - Object that has changed.
   BOOL              fBefore - Set to true if this is the object BEFORE the
                     change, FALSE if it's the object AFTER the operation.
   DWORD             dwChange - 0 => changed, 1=>added, 2=>removed
returns
   none

NOTES:
If dwChange == 0 (changed)
   - If the object doesn't already exist in m_lUNDOCHANGE then
      - If fBefore then clone it and keep it
      - Else ignore the call
   - Else the object exists
      - If its marked as changed or removed then ignore
      - Else its maketed as added, so keep the AFTER change only (!fBefore)
if dwChange == 1 (added)
   - If shouldn't exist. Ignore fBefore. Clone and add
if (dwChange == 2 (removed)
   - Ignore fBefore
   - If doesn't exist then clone and keep
   - If exists:
      - If added, then delete the added from the list because added + removed within one undo
      - If changed then mark as removed, but keep old info
      - Shouldnt be removed twice
*/
void CMIFLUndoPacket::Change (PCMIFLLib pObject, BOOL fBefore, DWORD dwChange)
{
   // find a match
   DWORD i, dwUndo;
   PUNDOCHANGE pUndo;
   for (i = 0; i < m_lUNDOCHANGE.Num(); i++) {
      pUndo = (PUNDOCHANGE) m_lUNDOCHANGE.Get(i);
      if ((pUndo->dwObject == 0) && pUndo->pObject && !_wcsicmp(pObject->m_szFile, pUndo->pObject->m_szFile))
         break;
   }
   if (i >= m_lUNDOCHANGE.Num())
      pUndo = NULL;  // couldnt find
   dwUndo = i;

   UNDOCHANGE uc;
   memset (&uc, 0, sizeof(uc));
   //uc.pObject = pObject;
   uc.dwChange = dwChange;
   uc.dwObject = 0;

   if (dwChange == 0) { // changed
      if (!pUndo) {
         if (fBefore) {
            // only add it if its market as the before event
            uc.pObject = pObject->Clone();
            if (!uc.pObject)
               return;  // didnt clone
            m_lUNDOCHANGE.Add (&uc);
         }
         return;
      }

      // if it was changed after it's already been chnged once then don't care
      // if it was changed after it was added then don't bother remembering
      //       because when do undo will use the latest changes
      // it should never be changed after its removed

      // therefore: do nothing
      return;
   }
   else if (dwChange == 1) { // added
      if (pUndo)
         return;  // shouldnt happen

      //uc.pObject = pObject->Clone();
      //uc.pObject->WorldSet (NULL);  // so doesnt call back in
      uc.pObject = NULL;   // since added it, don't really need to store this away. See undo later
      m_lUNDOCHANGE.Add (&uc);
      return;
   }
   else if (dwChange == 2) {  // removed

      // if it doesnt exist then clone and keep
      if (!pUndo) {
         uc.pObject = pObject->Clone();
         if (!uc.pObject)
            return;
         m_lUNDOCHANGE.Add (&uc);
         return;
      }

      // else, it exists already
      // ignore if already removed
      if (pUndo->dwChange == 2)
         return;

      // if it was marked as changed then convert to removed, and done
      if (pUndo->dwChange == 0) {
         pUndo->dwChange = 2;
         return;
      }

      // else, removing something that was added within the undo collection,
      // so just delete it from the list
      if (pUndo->pObject)
         delete pUndo->pObject;
      m_lUNDOCHANGE.Remove (dwUndo);
      return;
   }
}








/************************************************************************************
CMIFLProj::Constructor and destructor

NOTE: The constructor initializes to no libraries. To put in the default libraries
then use LibraryAddDefault()

inputs
   PCMIFLAppSocket      pSocket - Callback socket to use for the project.
*/
CMIFLProj::CMIFLProj (PCMIFLAppSocket pSocket)
{
   m_fDirty = FALSE;
   m_szFile[0] = 0;
   MemZero (&m_memDescShort);
   MemZero (&m_memDescLong);
   m_lPCMIFLLib.Init (sizeof(PCMIFLLib));
   m_lLANGID.Init (sizeof(LANGID));
   m_pSocket = pSocket;

   m_listPCUndoPacket.Init (sizeof(PCMIFLUndoPacket));
   m_listPCRedoPacket.Init (sizeof(PCMIFLUndoPacket));
   m_pUndoCur = new CMIFLUndoPacket;
   m_dwLibCur = 0;

   m_pHelpWindow = NULL;
   memset (&m_CurHistory, 0, sizeof(m_CurHistory));
   m_dwHelpTimer = 0;
   m_fSmallFont = FALSE;
   m_dwHelpTOC = 0;
   m_pSearch = NULL;
   m_pErrors = NULL;
   m_fErrorsAreSearch = FALSE;
   m_iErrCur = -1;
   m_pCompiled = NULL;
   m_pVM = NULL;
   m_fTestStepDebug = FALSE;
   m_pPageCurrent = NULL;

   m_fTransProsQuick = FALSE;

   MemZero (&m_memDebug);

   m_pNodeMisc = NULL;

   m_hHELPINDEX.Init (sizeof(HELPINDEX));
   m_lPWSTRIndex.Init (sizeof(PWSTR));

   // BUGFIX - Call Clear() so langid's set up
   Clear();
}

CMIFLProj::~CMIFLProj (void)
{
   HelpEnd ();

   if (m_pSearch)
      delete m_pSearch;

   Clear();

   if (m_pNodeMisc)
      delete m_pNodeMisc;

   if (m_pUndoCur)
      delete m_pUndoCur;
}

/************************************************************************************
CMIFLProj::Clone - Stanrdard API
*/
CMIFLProj *CMIFLProj::Clone (void)
{
   PCMIFLProj pNew = new CMIFLProj (m_pSocket);
   if (!pNew)
      return NULL;

   if (!CloneTo (pNew)) {
      delete pNew;
      return NULL;
   }

   return pNew;
}



/************************************************************************************
CMIFLProj::ClearVM - Wipes out information specific to the VM.
*/
void CMIFLProj::ClearVM (void)
{
   if (m_pVM) {
      m_pSocket->TestVMDelete (m_pVM);
      delete m_pVM;
      m_pVM = NULL;
   }

   if (m_pCompiled) {
      delete m_pCompiled;
      m_pCompiled= NULL;
   }
}

/************************************************************************************
CMIFLProj::Clear - Wipes out all the settings
*/
void CMIFLProj::Clear (void)
{
   ClearVM();

   // free elements in m_lPCMIFLLib
   DWORD i;
   PCMIFLLib *ppl = (PCMIFLLib*)m_lPCMIFLLib.Get(0);
   for (i = 0; i < m_lPCMIFLLib.Num(); i++)
      delete ppl[i];
   m_lPCMIFLLib.Clear();

   LANGID lid = 1033;   // BUGFIX - Default to american english when create new: GetSystemDefaultLangID();
   m_lLANGID.Init (sizeof(LANGID), &lid, 1);

   m_fDirty = FALSE;
   // NOTE: Not changing m_szFile

   MemZero (&m_memDescShort);
   MemZero (&m_memDescLong);

   m_dwLibCur = 0;

   m_dwTabFunc = m_dwTabObjEdit = 0;     // what tab is shown with functions

   if (m_pErrors)
      delete m_pErrors;
   m_pErrors = NULL;
   m_iErrCur = -1;
   m_fTestStepDebug = FALSE;
   MemZero (&m_memDebug);

   if (m_pNodeMisc)
      delete m_pNodeMisc;
   m_pNodeMisc = new CMMLNode2;
   m_pNodeMisc->NameSet (gpszNodeMisc);

   UndoClear();
}

/************************************************************************************
CMIFLProj::CloneTo - Stanrdard API
*/
BOOL CMIFLProj::CloneTo (CMIFLProj *pTo)
{
   // NOTE: Clone does NOT clone the undo buffers
   // NOTE: Does NOT clone m_pCompiled and m_pVM

   // free elements in m_lPCMIFLLib
   DWORD i;
   PCMIFLLib *ppl = (PCMIFLLib*)pTo->m_lPCMIFLLib.Get(0);
   for (i = 0; i < pTo->m_lPCMIFLLib.Num(); i++)
      delete ppl[i];
   pTo->m_lPCMIFLLib.Clear();

   pTo->m_dwTabFunc = m_dwTabFunc;
   pTo->m_dwTabObjEdit = m_dwTabObjEdit;


   pTo->m_fDirty = m_fDirty;
   pTo->m_pSocket = m_pSocket;
   pTo->m_dwLibCur = m_dwLibCur;
   wcscpy (pTo->m_szFile, m_szFile);
   MemZero (&pTo->m_memDescShort);
   MemZero (&pTo->m_memDescLong);
   MemCat (&pTo->m_memDescShort, (PWSTR)m_memDescShort.p);
   MemCat (&pTo->m_memDescLong, (PWSTR)m_memDescLong.p);
   
   if (pTo->m_pNodeMisc)
      delete pTo->m_pNodeMisc;
   pTo->m_pNodeMisc = m_pNodeMisc->Clone();

   // NOTE: Not cloning m_pErrors

   pTo->m_lPCMIFLLib.Init (sizeof(PCMIFLLib), m_lPCMIFLLib.Get(0), m_lPCMIFLLib.Num());
   ppl = (PCMIFLLib*)pTo->m_lPCMIFLLib.Get(0);
   for (i = 0; i < pTo->m_lPCMIFLLib.Num(); i++) {
      ppl[i] = ppl[i]->Clone();
      ppl[i]->ProjectSet (pTo);
   }

   pTo->m_lLANGID.Init (sizeof(LANGID), m_lLANGID.Get(0), m_lLANGID.Num());

   return TRUE;
}


static PWSTR gpszMIFLProj = L"MIFLProj";
static PWSTR gpszDescShort = L"DescShort";
static PWSTR gpszDescLong = L"DescLong";
static PWSTR gpszLibrary = L"Library";
static PWSTR gpszFile = L"File";
static PWSTR gpszLibCur = L"LibCur";

/************************************************************************************
CMIFLProj::MMLTo - Stanrdard API
*/
PCMMLNode2 CMIFLProj::MMLTo (void)
{
   PCMMLNode2 pNode = new CMMLNode2;
   if (!pNode)
      return NULL;
   pNode->NameSet (gpszMIFLProj);

   PWSTR psz = (PWSTR)m_memDescShort.p;
   if (psz && psz[0])
      MMLValueSet (pNode, gpszDescShort, psz);

   psz = (PWSTR)m_memDescLong.p;
   if (psz && psz[0])
      MMLValueSet (pNode, gpszDescLong, psz);

   MMLValueSet (pNode, gpszLibCur, (int) m_dwLibCur);

   PCMMLNode2 pSub = m_pNodeMisc->Clone();
   if (pSub) {
      pSub->NameSet (gpszNodeMisc);
      pNode->ContentAdd (pSub);
   }

   // write out library names
   DWORD i;
   PCMIFLLib *ppl = (PCMIFLLib*)m_lPCMIFLLib.Get(0);
   for (i = 0; i < m_lPCMIFLLib.Num(); i++) {
      PCMMLNode2 pSub = pNode->ContentAddNewNode ();
      if (!pSub)
         continue;
      pSub->NameSet (gpszLibrary);

      if (ppl[i]->m_szFile[0])
         MMLValueSet (pSub, gpszFile, ppl[i]->m_szFile);
   } // i

   WCHAR szTemp[64];
   LANGID *pal = (LANGID*) m_lLANGID.Get(0);
   for (i = 0; i < m_lLANGID.Num(); i++) {
      swprintf (szTemp, L"Lang%d", (int)i);
      MMLValueSet (pNode, szTemp, (int) pal[i]);
   }

   return pNode;
}



/************************************************************************************
CMIFLProj::MMLFrom - Stanrdard API

inputs
   pWSTR       pszMaster - File used to open this, in case directories changed
   HWND        hWnd - To bring up load problems with different libraries
*/
BOOL CMIFLProj::MMLFrom (PCMMLNode2 pNode, HWND hWnd, PWSTR pszMaster)
{
   Clear();

   PWSTR psz;

   psz = MMLValueGet (pNode, gpszDescShort);
   if (psz)
      MemCat (&m_memDescShort, psz);
   psz = MMLValueGet (pNode, gpszDescLong);
   if (psz)
      MemCat (&m_memDescLong, psz);

   m_dwLibCur = (DWORD) MMLValueGetInt (pNode, gpszLibCur, 0);

   DWORD i;
   WCHAR szTemp[64];
   for (i = 0; ; i++) {
      swprintf (szTemp, L"Lang%d", (int)i);
      LANGID lid = MMLValueGetInt (pNode, szTemp, -1);
      if (lid == (LANGID)-1)
         break;
      if (!i)
         m_lLANGID.Clear();   // since will have initialized language ID to english
      m_lLANGID.Add (&lid);
   }

   // find libries
   PCMMLNode2 pSub;
   for (i = 0; i < pNode->ContentNum(); i++) {
      pSub = NULL;
      pNode->ContentEnum (i, &psz, &pSub);
      if (!pSub)
         continue;
      psz = pSub->NameGet();
      if (!psz)
         continue;

      if (!_wcsicmp(psz, gpszLibrary)) {
         psz = MMLValueGet (pSub, gpszFile);
         if (!psz || !psz[0])
            continue;

         // take callback into account
         DWORD j;
         MASLIB lib;
         for (j = 0; j < m_pSocket->LibraryNum(); j++) {
            if (!m_pSocket->LibraryEnum (j, &lib))
               continue;
            if (!_wcsicmp (psz, lib.pszName))
               break;
         } // j
         if (j < m_pSocket->LibraryNum()) {
            if (!LibraryAdd (psz, FALSE, TRUE)) {
               WCHAR szTemp[512];
               swprintf (szTemp, L"The built-in library, %s, cannot be added.", psz);

               EscMessageBox (hWnd, ASPString(),
                  szTemp,
                  L"It will automatically be removed from the project.",
                  MB_ICONEXCLAMATION | MB_OK);
               continue;   // continue loading
            }
            continue;
         }

         // resolve the path
         WCHAR szTemp[256];
         wcscpy (szTemp, psz);
         ResolvePathIfNotExist (szTemp, pszMaster);

         // fail if cant load in
         if (!LibraryAdd (szTemp, FALSE, TRUE)) {
            WCHAR sz[512];
            swprintf (sz, L"The library, %s, cannot be found.", szTemp);

            EscMessageBox (hWnd, ASPString(),
               sz,
               L"It will automatically be removed from the project.",
               MB_ICONEXCLAMATION | MB_OK);
            continue;  // continue loading
         }

         continue;
      }
      else if (!_wcsicmp(psz, gpszNodeMisc)) {
         // new node misc
         if (m_pNodeMisc)
            delete m_pNodeMisc;
         m_pNodeMisc = pSub->Clone();
         continue;
      }

   } // i

   return TRUE;
}


/************************************************************************************
CMIFLProj::Save - Saves the project and all the dirty libraries to disk. This
sets the dirty flag to false. It uses the project's m_szFile for the file name.

inputs
   BOOL        fForce - If TRUE save no matter what, otherwise only if dirty
   BOOL        fClearDirty - If TRUE then automatically clear the dirty flag.
               If FALSE then leave it
*/
BOOL CMIFLProj::Save (BOOL fForce, BOOL fClearDirty)
{
   if (fForce || m_fDirty) {
      PCMMLNode2 pNode = MMLTo();
      if (!pNode)
         return FALSE;
      BOOL fRet;
      fRet = MMLFileSave (m_szFile, &GUID_MIFLProj, pNode);
      delete pNode;

      if (!fRet)
         return FALSE;
   }
   if (fClearDirty)
      m_fDirty = FALSE;

   // save libries
   DWORD i;
   PCMIFLLib *ppl = (PCMIFLLib*)m_lPCMIFLLib.Get(0);
   for (i = 0; i < m_lPCMIFLLib.Num(); i++) {
      if (!ppl[i]->m_fReadOnly)
         if (!ppl[i]->Save (fForce, fClearDirty))
            return FALSE;
   }

   return TRUE;
}


/************************************************************************************
CMIFLProj::Open - Opens the file.

inputs
   PWSTR          pszFile - File to open
   HWND           hWnd - To bring up error message if cant load all the libraries
*/
BOOL CMIFLProj::Open (PWSTR pszFile, HWND hWnd)
{
   PCMMLNode2 pNode = MMLFileOpen (pszFile, &GUID_MIFLProj);
   if (!pNode)
      return FALSE;

   if (!MMLFrom (pNode, hWnd, pszFile)) {
      delete pNode;
      return FALSE;
   }

   // rembmeber the file
   wcscpy (m_szFile, pszFile);

   delete pNode;
   return TRUE;
}



/************************************************************************************
CMIFLProj::LibraryPriority - Increases or decreases the library's priority in the
project.

inputs
   DWORD          dwLib - Index, 0 ..LibraryNum()-1
   BOOL           fHigh - If TRUE, increase the priority, which means move it towards
                        the beginning of the list. If FALSE then decrease pri.
returns
   BOOL - TRUE if success
*/
BOOL CMIFLProj::LibraryPriority (DWORD dwLib, BOOL fHigh)
{
   DWORD dwNum = m_lPCMIFLLib.Num();
   if (dwLib >= dwNum)
      return FALSE;

   m_fDirty = TRUE;

   PCMIFLLib *ppl = (PCMIFLLib*) m_lPCMIFLLib.Get(0);
   PCMIFLLib pTemp;
   if (fHigh) {
      if (!dwLib)
         return FALSE;

      pTemp = ppl[dwLib-1];
      ppl[dwLib-1] = ppl[dwLib];
      ppl[dwLib] = pTemp;

      if (m_dwLibCur == dwLib)
         m_dwLibCur--;
      else if (m_dwLibCur == dwLib-1)
         m_dwLibCur++;

      return TRUE;
   }

   // else low
   if (dwLib+1 >= dwNum)
      return FALSE;

   pTemp = ppl[dwLib];
   ppl[dwLib] = ppl[dwLib+1];
   ppl[dwLib+1] = pTemp;

   if (m_dwLibCur == dwLib)
      m_dwLibCur++;
   else if (m_dwLibCur == dwLib+1)
      m_dwLibCur--;

   return TRUE;
}



/************************************************************************************
CMIFLProj::LibraryCurGet - Gets the current library index, from 0..LibraryNum()-1.

Or, -1 if no libraries.
*/
DWORD CMIFLProj::LibraryCurGet (void)
{
   if (m_dwLibCur < m_lPCMIFLLib.Num())
      return m_dwLibCur;

   if (!m_lPCMIFLLib.Num())
      return -1;

   m_dwLibCur = 0;
   return m_dwLibCur;
}

/************************************************************************************
CMIFLProj::LibraryCurSet - Sets the current library index.

inputs
   DWORD          dwLib - Index, 0 ..LibraryNum()-1
*/
BOOL CMIFLProj::LibraryCurSet (DWORD dwLib)
{
   if (dwLib >= m_lPCMIFLLib.Num())
      return FALSE;

   m_dwLibCur = dwLib;
   return TRUE;
}


/************************************************************************************
CMIFLProj::LibraryGet - Returns a pointer to the library object.

inputs
   DWORD          dwLib - Index, 0 ..LibraryNum()-1
*/
PCMIFLLib CMIFLProj::LibraryGet (DWORD dwLib)
{
   if (dwLib >= m_lPCMIFLLib.Num())
      return FALSE;

   PCMIFLLib *ppl = (PCMIFLLib*)m_lPCMIFLLib.Get(0);
   return ppl[dwLib];
}

/************************************************************************************
CMIFLProj::LibraryNum - Returns the number of libraries
*/
DWORD CMIFLProj::LibraryNum (void)
{
   return m_lPCMIFLLib.Num();
}


/************************************************************************************
CMIFLProj::LibraryAddDefault - Loops through the callback looking for libraries
marked as default. These are added.

NOTE: Should only call if the list of libraries is blank.

inputs
   BOOL     fAtTop - If TRUE then at the top of the list
returns
   BOOL - TRUE if success
*/
BOOL CMIFLProj::LibraryAddDefault ()
{
   // look for the defaults
   DWORD i;
   MASLIB lib;
   for (i = 0; i < m_pSocket->LibraryNum(); i++) {
      if (!m_pSocket->LibraryEnum (i, &lib))
         continue;
      if (!lib.fDefaultOn)
         continue;

      // use this
      if (!LibraryAdd (lib.pszName, FALSE, TRUE))
         return FALSE;
   } // i

   return TRUE;
}



/************************************************************************************
CMIFLProj::LibraryRemove - Removes the library with the given name to the list. This does NOT
check for duplicates.

NOTE: This does NOT save the library if it's dirty.

inputs
   PWSTR       pszName - Name. If this matches one of the libraries in the callback's
                  resources that's used. Otherwise, a filename is assumed
   BOOL        fDontRememberForUndo - If TRUE then dont remember this in the undo cache
returns
   BOOL - TRUE if success
*/
BOOL CMIFLProj::LibraryRemove (PWSTR pszName, BOOL fDontRememberForUndo)
{
   // find the index
   DWORD i = LibraryFind (pszName);
   if (i == -1)
      return FALSE;
   PCMIFLLib *ppl = (PCMIFLLib*) m_lPCMIFLLib.Get(0);

   if (!fDontRememberForUndo && m_pUndoCur) {
      m_pUndoCur->Change (ppl[i], TRUE, 2);
      m_fDirty = TRUE;
   }

   delete ppl[i];
   m_lPCMIFLLib.Remove (i);
   return TRUE;
}



/************************************************************************************
CMIFLProj::LibrarySwap - Temporarily swap libraries in/out for puposes of
saving a merged file.

inputs
   PListFixed        plLibs - Should be initialized to a list of PCMIFLLib with
                           libraries to swap in. They will be swapped in, and
                           the list will be filled in with existing libraries.
returns
   none
*/
void CMIFLProj::LibrarySwap (PCListFixed plLibs)
{
   // set all current libraries to NO project
   //DWORD i;
   //PCMIFLLib pLib;
   // dont do for (i = 0; i < LibraryNum(); i++) {
   // dont do    pLib = LibraryGet (i);
   // dont do    pLib->ProjectSet (NULL);
   // dont do }

   // copy current list of libraries
   CListFixed lTemp;
   lTemp.Init (sizeof(PCMIFLLib), m_lPCMIFLLib.Get(0), m_lPCMIFLLib.Num());

   // swap
   m_lPCMIFLLib.Init (sizeof(PCMIFLLib), plLibs->Get(0), plLibs->Num());
   plLibs->Init (sizeof(PCMIFLLib), lTemp.Get(0), lTemp.Num());

   // set project
   // dont do for (i = 0; i < LibraryNum(); i++) {
   // dont do    pLib = LibraryGet (i);
   // dont do    pLib->ProjectSet (this);
   // dont do }
}

/************************************************************************************
CMIFLProj::LibraryFind - Looks for the given library.

inputs
   PWSTR       pszName - Name to find
returns
   DWORD - Index number, or -1 if cant find
*/
DWORD CMIFLProj::LibraryFind (PWSTR pszName)
{
   // find the index
   DWORD i;
   PCMIFLLib *ppl = (PCMIFLLib*) m_lPCMIFLLib.Get(0);
   for (i = 0; i < m_lPCMIFLLib.Num(); i++)
      if (!_wcsicmp(pszName, ppl[i]->m_szFile))
         return i;

   return -1;
}


/************************************************************************************
CMIFLProj::LibraryFind - Looks for the given library.

inputs
   DWORD          dwID - ID looking for
returns
   DWORD - Index number, or -1 if cant find
*/
DWORD CMIFLProj::LibraryFind (DWORD dwID)
{
   // find the index
   DWORD i;
   PCMIFLLib *ppl = (PCMIFLLib*) m_lPCMIFLLib.Get(0);
   for (i = 0; i < m_lPCMIFLLib.Num(); i++)
      if (ppl[i]->m_dwTempID == dwID)
         return i;

   return -1;
}

/************************************************************************************
CMIFLProj::LibraryAdd - Adds a library with the given name to the list. This does NOT
check for duplicates.

inputs
   PWSTR       pszName - Name. If this matches one of the libraries in the callback's
                  resources that's used. Otherwise, a filename is assumed
   BOOL        fAtTop - If TRUE then at the top of the list, FALSE then bottom
   BOOL        fDontRememberForUndo - If TRUE then dont remember this in the undo cache
returns
   BOOL - TRUE if success
*/
BOOL CMIFLProj::LibraryAdd (PWSTR pszName, BOOL fAtTop, BOOL fDontRememberForUndo)
{
   // see if this matches any library names
   // look for the defaults
   DWORD i;
   MASLIB lib;
   PCMIFLLib pLib;
   for (i = 0; i < m_pSocket->LibraryNum(); i++) {
      if (!m_pSocket->LibraryEnum (i, &lib))
         continue;
      if (_wcsicmp(lib.pszName, pszName))
         continue;

      // else, it's a match
      pLib = new CMIFLLib (m_pSocket);
      if (!pLib)
         return FALSE;
      if (!pLib->Open (lib.pszName, lib.hResourceInst, lib.dwResource)) {
         delete pLib;
         return FALSE;
      }

      if (fAtTop)
         m_lPCMIFLLib.Insert (0, &pLib);
      else
         m_lPCMIFLLib.Add (&pLib);
      if (!fDontRememberForUndo && m_pUndoCur) {
         m_pUndoCur->Change (pLib, FALSE, 1);
         m_fDirty = TRUE;
      }

      pLib->ProjectSet (this);
      return TRUE;
   } // i

   // load library
   pLib = new CMIFLLib (m_pSocket);
   if (!pLib)
      return FALSE;
   pLib->ProjectSet (this);
   if (!pLib->Open (pszName)) {
      delete pLib;
      return FALSE;
   }

   if (fAtTop)
      m_lPCMIFLLib.Insert (0, &pLib);
   else
      m_lPCMIFLLib.Add (&pLib);
   if (!fDontRememberForUndo && m_pUndoCur) {
      m_pUndoCur->Change (pLib, FALSE, 1);
      m_fDirty = TRUE;
   }
   return TRUE;
}



/*************************************************************************************
MIFLProjOpenDialog - Dialog box for opening a CMIFLProj

inputs
   HWND           hWnd - To display dialog off of
   PWSTR          pszFile - Pointer to a file name. Must be filled with initial file
   DWORD          dwChars - Number of characters in the file
   BOOL           fSave - If TRUE then saving instead of openeing. if fSave then
                     pszFile contains an initial file name, or empty string
   BOOL           fAllowMIF - If TRUE then the user can also select a "MIF" file.
returns
   BOOL - TRUE if pszFile filled in, FALSE if nothing opened
*/
BOOL MIFLProjOpenDialog (HWND hWnd, PWSTR pszFile, DWORD dwChars, BOOL fSave,
                         BOOL fAllowMIF)
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
   ofn.lpstrFilter = fAllowMIF ?
      "Any MIFL project (*.mfp;*.crf)\0*.mfp;*.crf\0"
      "Uncompiled MIFL Project(*.mfp)\0*.mfp\0"
      "Compiled MIFL Project(*.crf)\0*.crf\0"
      "\0\0":
      "MIFL Project(*.mfp)\0*.mfp\0\0\0";
   ofn.lpstrFile = szTemp;
   ofn.nMaxFile = sizeof(szTemp);
   ofn.lpstrTitle = fSave ? "Save the MIFL Project" :
      "Open MIFL Project";
   ofn.Flags = fSave ? (OFN_PATHMUSTEXIST | OFN_HIDEREADONLY) :
      (OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY);
   ofn.lpstrDefExt = "mfp";
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

/**************************************************************************************
CMIFLProj::UndoClear - Erase all the contents of the undo/redo buffers.

inputs
   BOOL        fUndo - Remove the undo buffers
   BOOL        fRedo - Remove the redo buffers
returns
   none
*/
void CMIFLProj::UndoClear (BOOL fUndo, BOOL fRedo)
{
   PCMIFLUndoPacket pup;
   DWORD i;
   if (fUndo) {
      // current undo
      if (m_pUndoCur && m_pUndoCur->m_lUNDOCHANGE.Num()) {
         delete m_pUndoCur;
         m_pUndoCur = NULL;
      }
      if (!m_pUndoCur)
         m_pUndoCur = new CMIFLUndoPacket;

      // anything in the list
      for (i = 0; i < m_listPCUndoPacket.Num(); i++) {
         pup =* ((PCMIFLUndoPacket*) m_listPCUndoPacket.Get(i));
         delete pup;
      }
      m_listPCUndoPacket.Clear();
   }

   if (fRedo) {
      // anything in the list
      for (i = 0; i < m_listPCRedoPacket.Num(); i++) {
         pup = *((PCMIFLUndoPacket*) m_listPCRedoPacket.Get(i));
         delete pup;
      }
      m_listPCRedoPacket.Clear();
   }
}


/**************************************************************************************
CMIFLProj::UndoRemember - Remember this point in time as an undo point. The current undo
object is added onto the undo list.

inputs
   none
returns
   none
*/
void CMIFLProj::UndoRemember (void)
{
   // if there arent any accumulated changes then ignore
   if (!m_pUndoCur->m_lUNDOCHANGE.Num())
      return;

   // clear the redo list since made changes
   UndoClear (FALSE, TRUE);

   // if the undo list is already too long then remove an item
   if (m_listPCUndoPacket.Num() > 100) {
      PCMIFLUndoPacket p = *((PCMIFLUndoPacket*) m_listPCUndoPacket.Get(0));
      delete p;
      m_listPCUndoPacket.Remove(0);
   }

   // append the current undo
   m_listPCUndoPacket.Add (&m_pUndoCur);

   // create a new current packet
   m_pUndoCur = new CMIFLUndoPacket;
}



/************************************************************************************
CMIFLProj::UndoQuery - Returns TRUE if there's something in the undo buffer. Also
filles in a flag if there's anything in the redo buffer.

inputs
   BOOL        *pfRedo - If not NULL, fills in true if can redo.
*/
BOOL CMIFLProj::UndoQuery (BOOL *pfRedo)
{
   BOOL fUndo;
   fUndo = (m_listPCUndoPacket.Num() ? TRUE : FALSE) || (m_pUndoCur->m_lUNDOCHANGE.Num() ? TRUE : FALSE);

   if (pfRedo)
      *pfRedo = (!m_pUndoCur->m_lUNDOCHANGE.Num()) && (m_listPCRedoPacket.Num() ? TRUE : FALSE);

   return fUndo;
}


/************************************************************************************
CMIFLProj::Undo - Undoes the last changes.

inputs
   BOOL     fUndo - Use TRUE to Undo. FALSE to Redo.
returns
   BOOL - TRUE if succeded. FALSE if error
*/
BOOL CMIFLProj::Undo (BOOL fUndoIt)
{
   // if we can't redo the clear the redo buffer now
   BOOL fUndo, fRedo;
   fUndo = UndoQuery (&fRedo);
   if (!fRedo)
      UndoClear (FALSE, TRUE);
   if (!fRedo && !fUndoIt)
      return FALSE;

   if (fUndoIt) {
      // if the current working undo buffer is empty then pull off the last element
      // in the undo queue
      if (!m_pUndoCur->m_lUNDOCHANGE.Num()) {
         DWORD dwNum;
         dwNum = m_listPCUndoPacket.Num();
         if (!dwNum)
            return FALSE;  // cant undo

         PCMIFLUndoPacket p;
         p = *((PCMIFLUndoPacket*) m_listPCUndoPacket.Get(dwNum-1));
         m_listPCUndoPacket.Remove(dwNum-1);

         delete m_pUndoCur;
         m_pUndoCur = p;
      }
   }
   else {
      // fill the current undo buffer from the redo
      if (m_pUndoCur->m_lUNDOCHANGE.Num())
         return FALSE;  // cant redo if stuff in buffer

      DWORD dwNum;
      dwNum = m_listPCRedoPacket.Num();
      if (!dwNum)
         return FALSE;  // cant undo

      PCMIFLUndoPacket p;
      p = *((PCMIFLUndoPacket*) m_listPCRedoPacket.Get(dwNum-1));
      m_listPCRedoPacket.Remove(dwNum-1);

      delete m_pUndoCur;
      m_pUndoCur = p;
   }

   // remeber the current selection
   CMIFLUndoPacket UPSel;

   // go through all the elements  and doo whatever replacing is necessary.
   // because we're also going to convert this information for the redo
   // buffer, 
   DWORD i;
   DWORD dwAdded, dwRemoved, dwChanged;
   dwAdded = dwRemoved = dwChanged = 0;
   for (i = 0; i < m_pUndoCur->m_lUNDOCHANGE.Num(); i++) {
      PUNDOCHANGE puc = (PUNDOCHANGE) m_pUndoCur->m_lUNDOCHANGE.Get(i);

      if (puc->dwObject == 0) {  // objects
         // find the library on the current list
         DWORD dwIndexCur;
         PCMIFLLib *ppl = (PCMIFLLib*) m_lPCMIFLLib.Get(0);
         for (dwIndexCur = 0; dwIndexCur < m_lPCMIFLLib.Num(); dwIndexCur++)
            if (puc->pObject && !_wcsicmp(puc->pObject->m_szFile, ppl[dwIndexCur]->m_szFile))
               break;
         if (dwIndexCur >= m_lPCMIFLLib.Num())
            dwIndexCur = -1;

         switch (puc->dwChange) {
         case 0:  // changed
            {
               // we had better find it
               if (dwIndexCur == -1)
                  break;

               // this was changed between versions so do a simple swap
               PCMIFLLib pTemp = ppl[dwIndexCur];
               ppl[dwIndexCur] = puc->pObject;
               puc->pObject = pTemp;

               // remember that dirty
               ppl[dwIndexCur]->m_fDirty = puc->pObject->m_fDirty = TRUE;

               dwChanged++;
            }
            break;
         case 1:  // added
            {
               // we had better find it
               if (dwIndexCur == -1)
                  break;

               // remember the object as it is
               if (puc->pObject)
                  delete puc->pObject;
               puc->pObject = ppl[dwIndexCur];
               puc->dwChange = 2;   // we're now a remove for the redo

               m_fDirty = TRUE;

               // we added this object since the last undo. Therefore, remove it.
               m_lPCMIFLLib.Remove(dwIndexCur);

               dwAdded++;
            }
            break;
         case 2:  // removed
            {
               // had better not find it in th elist
               if (dwIndexCur != -1)
                  break;

               puc->pObject->m_fDirty = TRUE;
               m_lPCMIFLLib.Add(&puc->pObject); // NOTE: ignoring order that add in

               // we removed it since the last undo, therefore add it back
               puc->pObject = puc->pObject->Clone();
               puc->dwChange = 1;   // we're now an add for the redo

               m_fDirty = TRUE;
               dwRemoved++;
            }
            break;
         }
      }

   } // i

   // put a new undo packet in
   PCMIFLUndoPacket pOld;
   pOld = m_pUndoCur;
   // make a new m_pUndoCur
   m_pUndoCur = new CMIFLUndoPacket;

   // move pOld into the redo buffer
   if (fUndoIt)
      m_listPCRedoPacket.Add (&pOld);
   else
      m_listPCUndoPacket.Add (&pOld);


   return TRUE;
}


/********************************************************************************
CMIFLProj::ObjectAboutToChange - Called by an object when it's about to change - such
as be moved, rotated, colored, etc. It calls into the world object to warn of the
impending change so the world object has the ability to clone the existing version
of the object for undo reasons. THe object MUST call Changed() soon after
calling this.

inputs
   PCObjectSocket    pObject - object
retursn
   none
*/
void CMIFLProj::ObjectAboutToChange (PCMIFLLib pObject)
{
   m_fDirty = TRUE;

   // cache away for undo/redo
   if (m_pUndoCur)
      m_pUndoCur->Change (pObject, TRUE, 0);
}

/*************************************************************************************
FillLangList - Internal function to fill the language list box

inputs
   PCEscPage         pPage - Page
   PCMIFLProj        pProj - Project
   DWORD             dwSel - One to select
*/
static void FillLangList (PCEscPage pPage, PCMIFLProj pProj, DWORD dwSel)
{
   PCEscControl pControl = pPage->ControlFind (L"langlist");
   if (!pControl)
      return;

   // clear the existing list
   pControl->Message (ESCM_LISTBOXRESETCONTENT);

   MemZero (&gMemTemp);

   DWORD i;
   LANGID *pal = (LANGID*)pProj->m_lLANGID.Get(0);
   for (i = 0; i < pProj->m_lLANGID.Num(); i++) {
      DWORD dwIndex = MIFLLangFind (pal[i]);
      if (dwIndex == -1)
         dwIndex = 0;   // so have something
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

   ESCMLISTBOXADD lba;
   memset (&lba, 0,sizeof(lba));
   lba.pszMML = (PWSTR)gMemTemp.p;

   pControl->Message (ESCM_LISTBOXADD, &lba);

   pControl->AttribSetInt (CurSel(), dwSel);
}


/*********************************************************************************
ProjDescPage - UI
*/

BOOL ProjDescPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PMIFLPAGE pmp = (PMIFLPAGE) pPage->m_pUserData;
   PCMIFLProj pProj = pmp->pProj;

   if (MIFLDefPage (pPage, dwMessage, pParam))
      return TRUE;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         PCEscControl pControl;

         pControl = pPage->ControlFind (L"descshort");
         if (pControl)
            pControl->AttribSet (Text(), (PWSTR) pProj->m_memDescShort.p);

         pControl = pPage->ControlFind (L"desclong");
         if (pControl)
            pControl->AttribSet (Text(), (PWSTR) pProj->m_memDescLong.p);

         FillLangList (pPage, pProj, 0);
      }
      return TRUE;

   case ESCN_EDITCHANGE:
      {
         PESCNEDITCHANGE p = (PESCNEDITCHANGE) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;
         PWSTR psz = p->pControl->m_pszName;

         if (!_wcsicmp(psz, L"descshort")) {
            // NOTE: Not bothering with undo ability since undo doesn't work for project
            DWORD dwNeed = 0;
            p->pControl->AttribGet (Text(), NULL, 0, &dwNeed);
            if (!pProj->m_memDescShort.Required (dwNeed))
               return FALSE;
            p->pControl->AttribGet (Text(), (PWSTR)pProj->m_memDescShort.p, (DWORD)pProj->m_memDescShort.m_dwAllocated, &dwNeed);
            pProj->m_fDirty = TRUE;

            return TRUE;
         }
         else if (!_wcsicmp(psz, L"desclong")) {
            // NOTE: Not bothering with undo ability since undo doesn't work for project
            DWORD dwNeed = 0;
            p->pControl->AttribGet (Text(), NULL, 0, &dwNeed);
            if (!pProj->m_memDescLong.Required (dwNeed))
               return FALSE;
            p->pControl->AttribGet (Text(), (PWSTR)pProj->m_memDescLong.p, (DWORD)pProj->m_memDescLong.m_dwAllocated, &dwNeed);
            pProj->m_fDirty = TRUE;

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
         
         if (!_wcsicmp(psz, L"langadd")) {
            // since the combo-box is in order, just get the index
            PCEscControl pControl = pPage->ControlFind (L"langid");
            if (!pControl)
               return TRUE;
            DWORD dwIndex = pControl->AttribGetInt (CurSel());
            LANGID lid;
            PWSTR psz = MIFLLangGet (dwIndex, &lid);
            if (!psz)
               return TRUE;   // shouldnt happen

            // see if already have the language
            DWORD i;
            LANGID *pl = (LANGID*)pProj->m_lLANGID.Get(0);
            for (i = 0; i < pProj->m_lLANGID.Num(); i++)
               if (pl[i] == lid)
                  break;
            if (i < pProj->m_lLANGID.Num()) {
               pPage->MBInformation (L"The language is already on the list.");
               return TRUE;
            }

            // else, add
            pProj->m_fDirty = TRUE;
            pProj->m_lLANGID.Add (&lid);
            FillLangList (pPage, pProj, pProj->m_lLANGID.Num()-1);
            EscChime (ESCCHIME_INFORMATION);
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"langremove")) {
            PCEscControl pControl = pPage->ControlFind (L"langlist");
            if (!pControl)
               return TRUE;

            // get the current selection
            DWORD dwSel = (DWORD)pControl->AttribGetInt (CurSel());
            if (dwSel >= pProj->m_lLANGID.Num()) {
               pPage->MBWarning (L"You must select the language you wish to remove.");
               return TRUE;
            }

            // figure out the language
            LANGID lid = *((LANGID*)pProj->m_lLANGID.Get(dwSel));

            // ask if wish to remove all the occurances in resource and string table
            int iRet = pPage->MBYesNo (L"Do you wish to remove the language from your resource and string entries?",
               L"If you select \"Yes\" then any entries for the language in your resources or strings will be deleted.", TRUE);
            if (iRet == IDCANCEL)
               return TRUE;   // exit
            if (iRet == IDYES) {
               DWORD i, j;
               for (i = 0; i < pProj->LibraryNum(); i++) {
                  PCMIFLLib pLib = pProj->LibraryGet(i);
                  
                  // BUGFIX - If read-only then skip
                  if (pLib->m_fReadOnly)
                     continue;

                  for (j = 0; j < pLib->StringNum(); j++) {
                     PCMIFLString pString = pLib->StringGet(j);
                     pString->LangRemove (lid, pLib);
                  } // j

                  for (j = 0; j < pLib->ResourceNum(); j++) {
                     PCMIFLResource pResource = pLib->ResourceGet(j);
                     pResource->LangRemove (lid, pLib);
                  } // j
               } // i

            }

            // remove it...
            pProj->m_lLANGID.Remove (dwSel);
            pProj->m_fDirty = TRUE;

            // remove from list
            FillLangList (pPage, pProj, dwSel);
            EscChime (ESCCHIME_INFORMATION);

            return TRUE;
         }

         else if (!_wcsicmp(psz, L"langmoveup") || !_wcsicmp(psz, L"langmovedown")) {
            BOOL fMoveUp = !_wcsicmp(psz, L"langmoveup");
            PCEscControl pControl = pPage->ControlFind (L"langlist");
            if (!pControl)
               return TRUE;

            // get the current selection
            DWORD dwSel = (DWORD)pControl->AttribGetInt (CurSel());
            if (dwSel >= pProj->m_lLANGID.Num()) {
               pPage->MBWarning (L"You must select the language you wish to move.");
               return TRUE;
            }

            if (fMoveUp && !dwSel) {
               pPage->MBWarning (L"You can't move the selected language any higher.");
               return TRUE;
            }
            if (!fMoveUp && (dwSel+1 >= pProj->m_lLANGID.Num())) {
               pPage->MBWarning (L"You can't move the selected language any lower.");
               return TRUE;
            }

            // swap
            pProj->m_fDirty = TRUE;
            LANGID *pl = (LANGID*)pProj->m_lLANGID.Get(0);
            LANGID lTemp;
            if (fMoveUp) {
               lTemp = pl[dwSel];
               pl[dwSel] = pl[dwSel-1];
               pl[dwSel-1] = lTemp;

               dwSel--;
            }
            else {
               lTemp = pl[dwSel];
               pl[dwSel] = pl[dwSel+1];
               pl[dwSel+1] = lTemp;

               dwSel++;
            }


            // remove from list
            FillLangList (pPage, pProj, dwSel);
            EscChime (ESCCHIME_INFORMATION);

            return TRUE;
         }
      }

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Project settings";
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"PROJFILE")) {
            p->pszSubString = pProj->m_szFile;
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"LANGLIST")) {
            MemZero (&gMemTemp);

            DWORD i;
            for (i = 0; i < MIFLLangNum(); i++) {
               LANGID lid;
               PWSTR psz = MIFLLangGet (i, &lid);
               MemCat (&gMemTemp, L"<elem name=");
               MemCat (&gMemTemp, (int)lid);
               MemCat (&gMemTemp, L">");
               MemCatSanitize (&gMemTemp, psz);
               MemCat (&gMemTemp, L"</elem>");
            } // i

            p->pszSubString = (PWSTR)gMemTemp.p;
            return TRUE;
         }
      }
      break;
   }

   return FALSE;
}


/*********************************************************************************
NewLibAskName - Asks the name for a new library and creates it

inputs
   PCMIFLProj        pProj - Project
   PCEscPage         pPage - Page to display stuf on
returns
   DWORD - TempID for the new library, or -1 if didn't create
*/
static DWORD NewLibAskName (PCMIFLProj pProj, PCEscPage pPage)
{
   WCHAR szFile[256];
   szFile[0] = 0;

   BOOL fRet;
   if (pProj->m_pSocket->FileSysSupport())
      fRet = pProj->m_pSocket->FileSysLibOpenDialog (pPage->m_pWindow->m_hWnd, szFile, sizeof(szFile)/sizeof(WCHAR), TRUE);
   else
      fRet = MIFLLibOpenDialog (pPage->m_pWindow->m_hWnd, szFile, sizeof(szFile)/sizeof(WCHAR), TRUE);
   if (!fRet)
      return -1;

   // see if the file exists..
   char sza[512];
   WideCharToMultiByte (CP_ACP, 0, szFile, -1, sza, sizeof(sza), 0, 0);
   FILE *f;
   OUTPUTDEBUGFILE (sza);
   f = fopen (sza, "rb");
   if (f) {
      fclose (f);

      if (IDYES != pPage->MBYesNo (L"The file already exists. Do you wish to overwrite it?"))\
         return -1;
   }

   // see if already in lib list...
   if (pProj->LibraryFind(szFile) != -1) {
      pPage->MBWarning (L"The library is already part of the project.");
      return -1;
   }

   // create
   PCMIFLLib pLib;
   pLib = new CMIFLLib (pProj->m_pSocket);
   if (!pLib)
      return TRUE;
   wcscpy (pLib->m_szFile, szFile);
   pLib->ProjectSet (pProj);  // so that wont crash in save
   pLib->Save (TRUE);
   delete pLib;

   // new library..
   pProj->LibraryAdd (szFile, TRUE, FALSE);

   DWORD dwIndex = pProj->LibraryFind (szFile);
   if (dwIndex == -1)
      return -1;

   return pProj->LibraryGet(dwIndex)->m_dwTempID;
}

/*********************************************************************************
ProjLibAddPage - UI
*/

BOOL ProjLibAddPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PMIFLPAGE pmp = (PMIFLPAGE) pPage->m_pUserData;
   PCMIFLProj pProj = pmp->pProj;

   if (MIFLDefPage (pPage, dwMessage, pParam))
      return TRUE;

   switch (dwMessage) {

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;

         if (!p->pControl || !p->pControl->m_pszName)
            break;
         PWSTR psz = p->pControl->m_pszName;

         if ((psz[0] == L'b') && (psz[1] == L'i') && (psz[2] == L':')) {
            DWORD dwNum = _wtoi(psz+3);
            MASLIB lib;
            if (!pProj->m_pSocket->LibraryEnum (dwNum, &lib))
               return TRUE;

            // add it

            // new library..
            pProj->LibraryAdd (lib.pszName, TRUE, FALSE);

            // link to the library description
            WCHAR szTemp[64];
            DWORD dwIndex = pProj->LibraryFind (lib.pszName);
            if (dwIndex == -1)
               return TRUE;
            swprintf (szTemp, L"lib:%ddesc", (int)pProj->LibraryGet(dwIndex)->m_dwTempID);
            pPage->Exit (szTemp);
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"newlib")) {
            DWORD dwID = NewLibAskName (pProj, pPage);
            if (dwID == -1)
               return TRUE;

            // link to the library description
            WCHAR szTemp[64];
            swprintf (szTemp, L"lib:%ddesc", (int)dwID);
            pPage->Exit (szTemp);
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"existlib")) {
            WCHAR szFile[256];
            szFile[0] = 0;

            BOOL fRet;
            if (pProj->m_pSocket->FileSysSupport())
               fRet = pProj->m_pSocket->FileSysLibOpenDialog (pPage->m_pWindow->m_hWnd, szFile, sizeof(szFile)/sizeof(WCHAR), FALSE);
            else
               fRet = MIFLLibOpenDialog (pPage->m_pWindow->m_hWnd, szFile, sizeof(szFile)/sizeof(WCHAR), FALSE);
            if (!fRet)
               return TRUE;

            // see if already in lib list...
            if (pProj->LibraryFind(szFile) != -1) {
               pPage->MBWarning (L"The library is already part of the project.");
               return TRUE;
            }

            // new library..
            pProj->LibraryAdd (szFile, TRUE, FALSE);

            // link to the library description
            WCHAR szTemp[64];
            DWORD dwIndex = pProj->LibraryFind (szFile);
            if (dwIndex == -1)
               return TRUE;
            swprintf (szTemp, L"lib:%ddesc", (int)pProj->LibraryGet(dwIndex)->m_dwTempID);
            pPage->Exit (szTemp);
            return TRUE;
         }
         else if (!_wcsicmp (psz, L"mergelib")) {
            if (pProj->LibraryNum() < 2) {
               pPage->MBInformation (L"The project must contain at least two libraries.");
               return TRUE;
            }

            // get all the libraries
            CListFixed lpLib;
            lpLib.Init (sizeof(PCMIFLLib));
            DWORD i;
            lpLib.Required (pProj->LibraryNum());
            for (i = 0; i < pProj->LibraryNum(); i++) {
               PCMIFLLib pLib = pProj->LibraryGet(i);
               lpLib.Add (&pLib);
            }

            // create the new library to merge into
            DWORD dwID = NewLibAskName (pProj, pPage);
            if (dwID == -1)
               return TRUE;
            PCMIFLLib pNew = pProj->LibraryGet(pProj->LibraryFind(dwID));
            if (!pNew)
               return TRUE;

            PCMIFLLib *ppl = (PCMIFLLib*)lpLib.Get(0);

            // merge
            if (!pNew->Merge (ppl, lpLib.Num())) {
               delete pNew;
               return TRUE;
            }

            // remove all other libs
            for (i = 0; i < lpLib.Num(); i++)
               pProj->LibraryRemove (ppl[i]->m_szFile);

            // redo same page
            pPage->Exit (MIFLRedoSamePage());
            return TRUE;
         }
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Add a new library";
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"BUILTIN")) {
            MemZero (&gMemTemp);

            DWORD i;
            MASLIB lib;
            BOOL fBuiltIn = FALSE;
            for (i = 0; i < pProj->m_pSocket->LibraryNum(); i++) {
               if (!pProj->m_pSocket->LibraryEnum (i, &lib))
                  continue;

               // if already exists then skip
               if (pProj->LibraryFind(lib.pszName) != -1)
                  continue;

               // if first one then title
               if (!fBuiltIn) {
                  MemCat (&gMemTemp, L"<xbr/>");
                  MemCat (&gMemTemp, L"<xSectionTitle>Built-in libraries</xSectionTitle>");
               }

               // else, show
               MemCat (&gMemTemp, L"<xChoiceButton name=bi:");
               MemCat (&gMemTemp, (int)i);
               MemCat (&gMemTemp, L"><bold>");
               MemCatSanitize (&gMemTemp, lib.pszName);
               MemCat (&gMemTemp, L"</bold>");
               if (lib.fDefaultOn)
                  MemCat (&gMemTemp, L" (Usually used)");
               MemCat (&gMemTemp, L"<br/>");
               MemCatSanitize (&gMemTemp, lib.pszDescShort);
               MemCat (&gMemTemp, L"</xChoiceButton>");
               fBuiltIn = TRUE;
            } // i

            p->pszSubString = (PWSTR)gMemTemp.p;
            return TRUE;
         }
      }
      break;
   }

   return FALSE;
}



/*********************************************************************************
ProjLibListPage - UI
*/

BOOL ProjLibListPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PMIFLPAGE pmp = (PMIFLPAGE) pPage->m_pUserData;
   PCMIFLProj pProj = pmp->pProj;

   if (MIFLDefPage (pPage, dwMessage, pParam))
      return TRUE;

   switch (dwMessage) {

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;

         if (!p->pControl || !p->pControl->m_pszName)
            break;
         PWSTR psz = p->pControl->m_pszName;

         if ((psz[0] == L'l') && ((psz[1] == L'd') || (psz[1] == L'u')) && (psz[2] == L':')) {
            DWORD dwNum = _wtoi(psz+3);

            pProj->LibraryPriority (dwNum, psz[1] == L'u');
            pPage->Exit (MIFLRedoSamePage());
            return TRUE;
         }
         else if ((psz[0] == L'l') && (psz[1] == L'r') && (psz[2] == L':')) {
            DWORD dwNum = _wtoi(psz+3);
            PCMIFLLib pLib = pProj->LibraryGet (dwNum);

            if (IDYES != pPage->MBYesNo (L"Are you sure you wish to remove this library?",
               L"Removing the library will NOT delete the file."))
               return TRUE;

            // if the library is dirty then ask if wish to save
            if (pLib->m_fDirty) {
               int iRet;
               iRet = pPage->MBYesNo (L"The library has changed since it was last saved. Do you wish to save the changes?",
                  L"Press \"Yes\" to save, \"No\" to remove the library without saving, and \"Cancel\" to keep the library.", TRUE);
               if (iRet == IDYES)
                  pLib->Save();
               else if (iRet == IDCANCEL)
                  return TRUE;
            }

            // remove
            pProj->LibraryRemove (pLib->m_szFile);
            pPage->Exit (MIFLRedoSamePage());
            return TRUE;
         }
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"List of libraries";
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"LIBS")) {
            MemZero (&gMemTemp);

            DWORD i;
            for (i = 0; i < pProj->LibraryNum(); i++) {
               PCMIFLLib pLib = pProj->LibraryGet(i);

               MemCat (&gMemTemp, L"<tr><td width=90%><xChoiceButton href=lib:");
               MemCat (&gMemTemp, (int) pLib->m_dwTempID);
               MemCat (&gMemTemp, L"desc><bold>");
               MemCatSanitize (&gMemTemp, LibraryDisplayName(pLib->m_szFile));
               MemCat (&gMemTemp, L"</bold><br/>");
               MemCatSanitize (&gMemTemp, (PWSTR) pLib->m_memDescShort.p);
               MemCat (&gMemTemp, L"</xChoiceButton></td><td width=10% align=center>");
               MemCat (&gMemTemp, L"<button style=uptriangle margintopbottom=2 marginleftright=2 buttonheight=16 buttonwidth=16 name=lu:");
               MemCat (&gMemTemp, (int)i);
               if (!i)
                  MemCat (&gMemTemp, L" enabled=false");
               MemCat (&gMemTemp, L"><xhoverhelpshort>Move up</xhoverhelpshort></button>"
                  L"<button style=righttriangle margintopbottom=2 marginleftright=2 buttonheight=16 buttonwidth=16 color=#c02020 name=lr:");
               MemCat (&gMemTemp, (int)i);
               MemCat (&gMemTemp, L"><xhoverhelpshort>Remove from library</xhoverhelpshort></button>"
                  L"<br/>"
                  L"<button style=downtriangle margintopbottom=2 marginleftright=2 buttonheight=16 buttonwidth=16 name=ld:");
               MemCat (&gMemTemp, (int)i);
               if (i+1 >= pProj->LibraryNum())
                  MemCat (&gMemTemp, L" enabled=false");
               MemCat (&gMemTemp, L"><xhoverhelpshort>Move down</xhoverhelpshort></button><br/>"
                  L"</td></tr>");

            } // i

            // if no libraries where added then say so
            if (!i)
               MemCat (&gMemTemp, L"<tr><td>The project does not contain any libraries.</td></tr>");

            p->pszSubString = (PWSTR)gMemTemp.p;
            return TRUE;
         }
      }
      break;
   }

   return FALSE;
}



/*********************************************************************************
ProjTestNotPage - UI
*/

BOOL ProjTestNotPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PMIFLPAGE pmp = (PMIFLPAGE) pPage->m_pUserData;
   PCMIFLProj pProj = pmp->pProj;

   if (MIFLDefPage (pPage, dwMessage, pParam))
      return TRUE;

   switch (dwMessage) {

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Test compiled code";
            return TRUE;
         }
      }
      break;
   }

   return FALSE;
}


/*********************************************************************************
ProjTestPage - UI
*/

BOOL ProjTestPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PMIFLPAGE pmp = (PMIFLPAGE) pPage->m_pUserData;
   PCMIFLProj pProj = pmp->pProj;
   PCMIFLVM pVM = pmp->pVM;

   if (MIFLDefPage (pPage, dwMessage, pParam))
      return TRUE;

   if (pmp->pVM)
      if (pmp->pVM->DebugPage (pPage, dwMessage, pParam, TRUE))
         return TRUE;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         PCEscControl pControl = pPage->ControlFind (L"stepdebug");
         if (pControl)
            pControl->AttribSetBOOL (Checked(), pProj->m_fTestStepDebug);

         pControl = pPage->ControlFind (L"testcode");
         if (pControl)
            pControl->AttribSet (Text(), (PWSTR)pProj->m_memDebug.p);
      }
      break;

   case ESCN_EDITCHANGE:
      {
         PESCNEDITCHANGE p = (PESCNEDITCHANGE) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;
         PWSTR psz = p->pControl->m_pszName;

         if (!_wcsicmp(psz, L"testcode")) {
            DWORD dwNeed;
            p->pControl->AttribGet (Text(), NULL, 0, &dwNeed);
            if (!pProj->m_memDebug.Required (dwNeed))
               return TRUE;
            p->pControl->AttribGet (Text(), (PWSTR)pProj->m_memDebug.p, (DWORD)pProj->m_memDebug.m_dwAllocated, &dwNeed);

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

         if (!_wcsicmp(psz, L"stepdebug")) {
            pProj->m_fTestStepDebug = p->pControl->AttribGetBOOL (Checked());
            return TRUE;
         }
      }
      break;
   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Test compiled code";
            return TRUE;
         }
      }
      break;
   }

   return FALSE;
}


/********************************************************************************
CMIFLProj::ObjectChanged - Called by an object after it has changed. (Must have called
AboutToChange before this.) The world object uses this to notify the views
that the object has changed.

inputs
   PCObjectSocket    pObject - object
returns
   none
  */
void CMIFLProj::ObjectChanged (PCMIFLLib pObject)
{
   pObject->m_fDirty = TRUE;

   // do nothing else
}


/********************************************************************************
CMIFLProj::DialogEditSingle - Brings up a page (using pWindow) to display the
given link. Once the user exist the page pszLink is filled in with the new link

inputs
   PCEscWindow       pWindow - Window to use
   PWSTR             pszLink - Initialliy filled with a string for the link to start
                     out with. If the link is not found to match any known pages, such as "",
                     then it automatically goes to the main project page. Once the page
                     has been viewed by the user the contents of pszLink are filled in
                     with the new string.
   int               *piVScroll - Should initially fill in with the scroll start, and will be
                     subsequencly filled in with window scroll positionreturns
   BOOL - TRUE if the user pressed a valid link, FALSE if it's a "[close]" or "back"
*/
BOOL CMIFLProj::DialogEditSingle (PCEscWindow pWindow, PWSTR pszLink, int *piVScroll)
{
   // cache undo/redo automatically
   UndoRemember ();

   PWSTR pszLib = L"lib:";
   DWORD dwLibLen = (DWORD)wcslen(pszLib);
   if (!_wcsnicmp(pszLink, pszLib, dwLibLen)) {
      // will need to extract correct library number
      DWORD dwNum;
      PWSTR pszNext = LinkExtractNum (pszLink + dwLibLen, &dwNum);
      if (!pszNext)
         goto defaultpage;

      // find the library
      DWORD i;
      PCMIFLLib *ppl = (PCMIFLLib*)m_lPCMIFLLib.Get(0);
      for (i = 0; i < m_lPCMIFLLib.Num(); i++)
         if (ppl[i]->m_dwTempID == dwNum)
            break;
      if (i >= m_lPCMIFLLib.Num())
         goto defaultpage; // no longer exists

      // if this is not the current library then change the current library
      if (i != LibraryCurGet())
         LibraryCurSet (i);

      if (!ppl[i]->DialogEditSingle (pWindow, pszNext, pszLink, piVScroll)) {
         pszLink[0] = 0;   // so dont repeat same page, if there's an error
         return FALSE;
      }
      goto testlink;
   }

defaultpage:
   PWSTR pszProj = L"proj:";
   DWORD dwProjLen = (DWORD)wcslen(pszProj);

   // what resource to use
   DWORD dwPage = IDR_MMLPROJDESC;
   PESCPAGECALLBACK pPage = ProjDescPage;
   BOOL fEnterAndLeave = FALSE;

   // if it's a project link...
   if (!_wcsnicmp(pszLink, pszProj, dwProjLen)) {
      if (!_wcsicmp (pszLink + dwProjLen, L"desc")) {
         dwPage = IDR_MMLPROJDESC;
         pPage = ProjDescPage;
      }
      else if (!_wcsicmp (pszLink + dwProjLen, L"libadd")) {
         dwPage = IDR_MMLPROJLIBADD;
         pPage = ProjLibAddPage;
      }
      else if (!_wcsicmp (pszLink + dwProjLen, L"liblist")) {
         dwPage = IDR_MMLPROJLIBLIST;
         pPage = ProjLibListPage;
      }
      else if (!_wcsicmp (pszLink + dwProjLen, L"test")) {
refreshtest:
         // create the VM if not created
         if (m_pCompiled && !m_pVM && m_pSocket->TestQuery()) {
            m_pVM = new CMIFLVM;
            if (m_pVM) {
               // BUGFIX - Flipped order, so TestVMNew() is called first, followed
               // by init. Did this because if the constructor in an object called into
               // a function supplied by the main app, that was initialized on a new VM,
               // then it didn't work

               // BUGFIX - Flipped back again because causing crashes
               m_pVM->Init (m_pCompiled, pWindow, pWindow->m_hWnd);
               m_pSocket->TestVMNew (m_pVM);
            }
         }

         dwPage = m_pVM  ? IDR_MMLPROJTEST : IDR_MMLPROJTESTNOT;
         pPage = m_pVM ? ProjTestPage : ProjTestNotPage;
         fEnterAndLeave = m_pVM ? TRUE : FALSE;
      }
   }
   if (!dwPage)
      return FALSE;


   // warn main code that entering and leaving test area
   if (fEnterAndLeave)
      m_pSocket->TestVMPageEnter (m_pVM);

   PWSTR pszRet;
   MIFLPAGE mp;
   memset (&mp, 0, sizeof(mp));
   mp.pProj = this;
   mp.pVM = m_pVM;
   mp.iVScroll = *piVScroll;
redo:

   // if on the test page, then enable the timers
   if (m_pVM && (dwPage == IDR_MMLPROJTEST))
      m_pVM->TimerAutomatic (100);

   pszRet = pWindow->PageDialog (ghInstance, dwPage, pPage, &mp);

   // if on the test page, then disable the timers
   if (m_pVM && (dwPage == IDR_MMLPROJTEST))
      m_pVM->TimerAutomatic (0);

   *piVScroll = mp.iVScroll = pWindow->m_iExitVScroll;
   if (pszRet && !_wcsicmp(pszRet, MIFLRedoSamePage())) {
      if ((dwPage == IDR_MMLPROJTESTNOT) || ((dwPage == IDR_MMLPROJTEST) && !m_pVM))
         goto refreshtest; // recompiled so may have changed
      goto redo;
   }
   else if (pszRet && (dwPage == IDR_MMLPROJTEST) && !_wcsicmp(pszRet, L"runthis")) {
      // output this text:
      m_pVM->OutputDebugString (L"\r\nExecute code: \"");
      m_pVM->OutputDebugString ((PWSTR)m_memDebug.p);
      m_pVM->OutputDebugString (L"\"\r\n");

      // set debugging information inp m_pVM
      m_pVM->DebugModeSet(m_fTestStepDebug ? MDM_STEPIN : MDM_ONRUNTIME);

#ifdef _DEBUG
      PWSTR pszCmd = (PWSTR)m_memDebug.p;
      if (!_wcsicmp(pszCmd, L"loadexclude")) {
         // hack so can test loading
         PCMMLNode2 pNode = MMLFileOpen (L"c:\\test.txt", &GUID_NULL);
         if (pNode) {
            delete m_pVM;
            m_pVM = new CMIFLVM;

            // BUGFIX - Flipped order, so TestVMNew() is called first, followed
            // by init. Did this because if the constructor in an object called into
            // a function supplied by the main app, that was initialized on a new VM,
            // then it didn't work
            // BUGFIX - Flipped back again because causing crashes
            m_pVM->Init (m_pCompiled, pWindow, pWindow->m_hWnd, MDM_ONRUNTIME, pNode);
            m_pSocket->TestVMNew (m_pVM);
            delete pNode;
         }
         goto refreshtest;
      }
      else if (!_wcsicmp(pszCmd, L"loadinclude")) {
         // hack so can test loading
         PCMMLNode2 pNode = MMLFileOpen (L"c:\\test.txt", &GUID_NULL);
         if (pNode) {
            m_pVM->MMLFrom (FALSE, FALSE, TRUE, FALSE, pNode, NULL);
            delete pNode;
         }
         goto refreshtest;
      }
      else if (!_wcsicmp(pszCmd, L"saveexclude")) {
         // hack so can test loading
         PCMMLNode2 pNode = m_pVM->MMLTo (TRUE, TRUE, &m_pVM->m_gDebugVarsObject, 1, TRUE);
            
         if (pNode) {
            MMLFileSave (L"c:\\test.txt", &GUID_NULL, pNode);
            delete pNode;
         }
         goto refreshtest;
      }
      else if (!_wcsicmp(pszCmd, L"saveinclude")) {
         // hack so can test loading
         PCMMLNode2 pNode = m_pVM->MMLTo (FALSE, FALSE, &m_pVM->m_gDebugVarsObject, 1, TRUE);
            
         if (pNode) {
            MMLFileSave (L"c:\\test.txt", &GUID_NULL, pNode);
            delete pNode;
         }
         goto refreshtest;
      }
#endif

      CMIFLVarLValue var;
      m_pVM->DebugWindowSet (pWindow);
      DWORD dwRet = m_pVM->RunTimeCodeCall ((PWSTR)m_memDebug.p, &m_pVM->m_gDebugVarsObject, &var);
      m_pVM->DebugWindowSet (NULL);

      // write out its return value
      m_pVM->OutputDebugString (L"Returns: ");
      m_pVM->OutputDebugString (&var.m_Var);
      m_pVM->OutputDebugString (L"\r\n");


      if (dwRet == MFC_REPORTERROR)
         EscMessageBox (pWindow->m_hWnd, ASPString(),
            L"Compilation of the test-code line failed.",
            NULL,
            MB_ICONEXCLAMATION | MB_OK);
      else
         EscChime (dwRet ? ESCCHIME_ERROR : ESCCHIME_INFORMATION);

      // redo page
      goto refreshtest;
   }

   // warn main code that entering and leaving test area
   if (fEnterAndLeave)
      m_pSocket->TestVMPageLeave (m_pVM);

   if (pszRet)
      wcscpy (pszLink, pszRet);
   else
      pszLink[0] = 0;

testlink:
   if (!_wcsicmp(pszLink, L"[close]") || !_wcsicmp(pszLink, L"exit") || !_wcsicmp(pszLink, Back()))
      return FALSE;

   return TRUE;
}


/********************************************************************************
CMIFLProj::DialogEdit - Brings up the editing UI for the project.

inputs
   PCEscWindow          pWindow - Window to use. If NULL then a new window is created and used
   HWND                 hWndParent - Parent window to use if have to create a new window (when pWindow==NULL)
   PWSTR                pszLink - Initial link to use. Can be NULL
returns
   BOOL - TRUE if the user pressed exit, FALSE if closed window
*/
BOOL CMIFLProj::DialogEdit (PCEscWindow pWindow, HWND hWndParent, PWSTR pszLink)
{
   int iVScroll = 0;
   WCHAR    szLink[256];
   szLink[0] = 0;
   if (pszLink)
      wcscpy (szLink, pszLink);


   // create a history
   CListVariable lHistory;
   CListFixed     lHistoryScroll;   // scroll location
   lHistoryScroll.Init(sizeof(int));

   // create the window
   BOOL fCreated = FALSE;
   BOOL fPressedExit = FALSE;
   if (!pWindow) {
      pWindow = new CEscWindow;
      if (!pWindow)
         return FALSE;

      RECT r;
      DialogBoxLocation3 (hWndParent, &r, TRUE);

      pWindow->Init (ghInstance, hWndParent, 0 /*EWS_FIXEDSIZE*/, &r);

      fCreated = TRUE;
   }

   while (TRUE) {
      // add this link for back
      lHistory.Add (szLink, (wcslen(szLink)+1)*sizeof(WCHAR));
      lHistoryScroll.Add (&iVScroll);

      // not too many levels of back
      while (lHistory.Num() > 50) {
         lHistory.Remove (0);
         lHistoryScroll.Remove (0);
      }

      // pull up the page
      BOOL fRet = DialogEditSingle (pWindow, szLink, &iVScroll);

      // set scroll
      *((int*)lHistoryScroll.Get(lHistoryScroll.Num()-1)) = iVScroll;

      // if hit redo, remove link
      if (!_wcsicmp(szLink, RedoSamePage()) || !_wcsicmp(szLink, MIFLRedoSamePage())) {
         if (lHistory.Num()) {
            wcscpy (szLink, (PWSTR)lHistory.Get(lHistory.Num()-1));
            iVScroll = *((int*)lHistoryScroll.Get(lHistoryScroll.Num()-1));
            lHistory.Remove (lHistory.Num()-1);
            lHistoryScroll.Remove (lHistoryScroll.Num()-1);
         }
         else {
            szLink[0] = 0;
            iVScroll = 0;
         }

         continue;
      }

      if (fRet) {
         iVScroll = 0;
         continue;   // was valid
      }

      // else, see if pressed back
      if (!_wcsicmp(szLink, Back())) {
         if (lHistory.Num()) {
            lHistory.Remove (lHistory.Num()-1);
            lHistoryScroll.Remove (lHistoryScroll.Num()-1);
         }
         if (lHistory.Num()) {
            wcscpy (szLink, (PWSTR)lHistory.Get(lHistory.Num()-1));
            iVScroll = *((int*)lHistoryScroll.Get(lHistoryScroll.Num()-1));
            lHistory.Remove (lHistory.Num()-1);
            lHistoryScroll.Remove (lHistoryScroll.Num()-1);
         }
         else {
            szLink[0] = 0;
            iVScroll = 0;
         }

         continue;
      }

      // set scroll
      iVScroll = 0;

      // check to see if user pressed exit
      // and fill fPressedExit
      if (!_wcsicmp(szLink, L"exit"))
         fPressedExit = TRUE;

      // else, exit
      break;
   }


   if (fCreated)
      delete pWindow;

   return fPressedExit;
}


/************************************************************************************
CMIFLProj::MethDefOverridden - Sees if a method definition is overridden or overrides
one of the same name in another library.

inputs
   DWORD          dwLibID - ID of the library (NOT index) that definition is in
   PWSTR          pszName - Name to see if overridden
   PWSTR          *ppszHigher - Filled in with a pointer to the name of the library
                     with a higher priority. If no defintion is found then this
                     is filled with NULL
   PWSTR          *ppszLower - Fileld with a pointer to a library with lower priority
                     which also defines this. NULL if none found
returns
   none
*/
void CMIFLProj::MethDefOverridden (DWORD dwLibID, PWSTR pszName, PWSTR *ppszHigher, PWSTR *ppszLower)
{
   *ppszHigher = *ppszLower = NULL;

   DWORD i;
   BOOL fFoundLib = FALSE;
   for (i = 0; i < LibraryNum(); i++) {
      PCMIFLLib pLib = LibraryGet(i);

      // if matches library looking for then skip
      if (pLib->m_dwTempID == dwLibID) {
         fFoundLib = TRUE;
         continue;
      }

      PWSTR *ppszLook = (fFoundLib ? ppszLower : ppszHigher);
      if (*ppszLook)
         continue;   // already found

      // see if can find
      if (pLib->MethDefFind (pszName, -1) != -1)
         *ppszLook = pLib->m_szFile;
   } // i
}





/************************************************************************************
CMIFLProj::ObjectOverridden - Sees if a Object definition is overridden or overrides
one of the same name in another library.

inputs
   DWORD          dwLibID - ID of the library (NOT index) that definition is in
   PWSTR          pszName - Name to see if overridden
   PWSTR          *ppszHigher - Filled in with a pointer to the name of the library
                     with a higher priority. If no defintion is found then this
                     is filled with NULL
   PWSTR          *ppszLower - Fileld with a pointer to a library with lower priority
                     which also defines this. NULL if none found
returns
   none
*/
void CMIFLProj::ObjectOverridden (DWORD dwLibID, PWSTR pszName, PWSTR *ppszHigher, PWSTR *ppszLower)
{
   *ppszHigher = *ppszLower = NULL;

   DWORD i;
   BOOL fFoundLib = FALSE;
   for (i = 0; i < LibraryNum(); i++) {
      PCMIFLLib pLib = LibraryGet(i);

      // if matches library looking for then skip
      if (pLib->m_dwTempID == dwLibID) {
         fFoundLib = TRUE;
         continue;
      }

      PWSTR *ppszLook = (fFoundLib ? ppszLower : ppszHigher);
      if (*ppszLook)
         continue;   // already found

      // see if can find
      if (pLib->ObjectFind (pszName, -1) != -1)
         *ppszLook = pLib->m_szFile;
   } // i
}

/************************************************************************************
CMIFLProj::PropDefOverridden - Sees if a Propod definition is overridden or overrides
one of the same name in another library.

inputs
   DWORD          dwLibID - ID of the library (NOT index) that definition is in
   PWSTR          pszName - Name to see if overridden
   PWSTR          *ppszHigher - Filled in with a pointer to the name of the library
                     with a higher priority. If no defintion is found then this
                     is filled with NULL
   PWSTR          *ppszLower - Fileld with a pointer to a library with lower priority
                     which also defines this. NULL if none found
returns
   none
*/
void CMIFLProj::PropDefOverridden (DWORD dwLibID, PWSTR pszName, PWSTR *ppszHigher, PWSTR *ppszLower)
{
   *ppszHigher = *ppszLower = NULL;

   DWORD i;
   BOOL fFoundLib = FALSE;
   for (i = 0; i < LibraryNum(); i++) {
      PCMIFLLib pLib = LibraryGet(i);

      // if matches library looking for then skip
      if (pLib->m_dwTempID == dwLibID) {
         fFoundLib = TRUE;
         continue;
      }

      PWSTR *ppszLook = (fFoundLib ? ppszLower : ppszHigher);
      if (*ppszLook)
         continue;   // already found

      // see if can find
      if (pLib->PropDefFind (pszName, -1) != -1)
         *ppszLook = pLib->m_szFile;
   } // i
}





/************************************************************************************
CMIFLProj::DocOverridden - Sees if a doc is overridden or overrides
one of the same name in another library.

inputs
   DWORD          dwLibID - ID of the library (NOT index) that definition is in
   PWSTR          pszName - Name to see if overridden
   PWSTR          *ppszHigher - Filled in with a pointer to the name of the library
                     with a higher priority. If no defintion is found then this
                     is filled with NULL
   PWSTR          *ppszLower - Fileld with a pointer to a library with lower priority
                     which also defines this. NULL if none found
returns
   none
*/
void CMIFLProj::DocOverridden (DWORD dwLibID, PWSTR pszName, PWSTR *ppszHigher, PWSTR *ppszLower)
{
   *ppszHigher = *ppszLower = NULL;

   DWORD i;
   BOOL fFoundLib = FALSE;
   for (i = 0; i < LibraryNum(); i++) {
      PCMIFLLib pLib = LibraryGet(i);

      // if matches library looking for then skip
      if (pLib->m_dwTempID == dwLibID) {
         fFoundLib = TRUE;
         continue;
      }

      PWSTR *ppszLook = (fFoundLib ? ppszLower : ppszHigher);
      if (*ppszLook)
         continue;   // already found

      // see if can find
      if (pLib->DocFind (pszName, -1) != -1)
         *ppszLook = pLib->m_szFile;
   } // i
}



/************************************************************************************
CMIFLProj::GlobalOverridden - Sees if a global definition is overridden or overrides
one of the same name in another library.

inputs
   DWORD          dwLibID - ID of the library (NOT index) that definition is in
   PWSTR          pszName - Name to see if overridden
   PWSTR          *ppszHigher - Filled in with a pointer to the name of the library
                     with a higher priority. If no defintion is found then this
                     is filled with NULL
   PWSTR          *ppszLower - Fileld with a pointer to a library with lower priority
                     which also defines this. NULL if none found
returns
   none
*/
void CMIFLProj::GlobalOverridden (DWORD dwLibID, PWSTR pszName, PWSTR *ppszHigher, PWSTR *ppszLower)
{
   *ppszHigher = *ppszLower = NULL;

   DWORD i;
   BOOL fFoundLib = FALSE;
   for (i = 0; i < LibraryNum(); i++) {
      PCMIFLLib pLib = LibraryGet(i);

      // if matches library looking for then skip
      if (pLib->m_dwTempID == dwLibID) {
         fFoundLib = TRUE;
         continue;
      }

      PWSTR *ppszLook = (fFoundLib ? ppszLower : ppszHigher);
      if (*ppszLook)
         continue;   // already found

      // see if can find
      if (pLib->GlobalFind (pszName, -1) != -1)
         *ppszLook = pLib->m_szFile;
   } // i
}




/************************************************************************************
CMIFLProj::FuncOverridden - Sees if a Func definition is overridden or overrides
one of the same name in another library.

inputs
   DWORD          dwLibID - ID of the library (NOT index) that definition is in
   PWSTR          pszName - Name to see if overridden
   PWSTR          *ppszHigher - Filled in with a pointer to the name of the library
                     with a higher priority. If no defintion is found then this
                     is filled with NULL
   PWSTR          *ppszLower - Fileld with a pointer to a library with lower priority
                     which also defines this. NULL if none found
returns
   none
*/
void CMIFLProj::FuncOverridden (DWORD dwLibID, PWSTR pszName, PWSTR *ppszHigher, PWSTR *ppszLower)
{
   *ppszHigher = *ppszLower = NULL;

   DWORD i;
   BOOL fFoundLib = FALSE;
   for (i = 0; i < LibraryNum(); i++) {
      PCMIFLLib pLib = LibraryGet(i);

      // if matches library looking for then skip
      if (pLib->m_dwTempID == dwLibID) {
         fFoundLib = TRUE;
         continue;
      }

      PWSTR *ppszLook = (fFoundLib ? ppszLower : ppszHigher);
      if (*ppszLook)
         continue;   // already found

      // see if can find
      if (pLib->FuncFind (pszName, -1) != -1)
         *ppszLook = pLib->m_szFile;
   } // i
}




/************************************************************************************
CMIFLProj::StringOverridden - Sees if a String definition is overridden or overrides
one of the same name in another library.

inputs
   DWORD          dwLibID - ID of the library (NOT index) that definition is in
   PWSTR          pszName - Name to see if overridden
   PWSTR          *ppszHigher - Filled in with a pointer to the name of the library
                     with a higher priority. If no defintion is found then this
                     is filled with NULL
   PWSTR          *ppszLower - Fileld with a pointer to a library with lower priority
                     which also defines this. NULL if none found
returns
   none
*/
void CMIFLProj::StringOverridden (DWORD dwLibID, PWSTR pszName, PWSTR *ppszHigher, PWSTR *ppszLower)
{
   *ppszHigher = *ppszLower = NULL;

   DWORD i;
   BOOL fFoundLib = FALSE;
   for (i = 0; i < LibraryNum(); i++) {
      PCMIFLLib pLib = LibraryGet(i);

      // if matches library looking for then skip
      if (pLib->m_dwTempID == dwLibID) {
         fFoundLib = TRUE;
         continue;
      }

      PWSTR *ppszLook = (fFoundLib ? ppszLower : ppszHigher);
      if (*ppszLook)
         continue;   // already found

      // see if can find
      if (pLib->StringFind (pszName, -1) != -1)
         *ppszLook = pLib->m_szFile;
   } // i
}





/************************************************************************************
CMIFLProj::ResourceOverridden - Sees if a Resource definition is overridden or overrides
one of the same name in another library.

inputs
   DWORD          dwLibID - ID of the library (NOT index) that definition is in
   PWSTR          pszName - Name to see if overridden
   PWSTR          *ppszHigher - Filled in with a pointer to the name of the library
                     with a higher priority. If no defintion is found then this
                     is filled with NULL
   PWSTR          *ppszLower - Fileld with a pointer to a library with lower priority
                     which also defines this. NULL if none found
returns
   none
*/
void CMIFLProj::ResourceOverridden (DWORD dwLibID, PWSTR pszName, PWSTR *ppszHigher, PWSTR *ppszLower)
{
   *ppszHigher = *ppszLower = NULL;

   DWORD i;
   BOOL fFoundLib = FALSE;
   for (i = 0; i < LibraryNum(); i++) {
      PCMIFLLib pLib = LibraryGet(i);

      // if matches library looking for then skip
      if (pLib->m_dwTempID == dwLibID) {
         fFoundLib = TRUE;
         continue;
      }

      PWSTR *ppszLook = (fFoundLib ? ppszLower : ppszHigher);
      if (*ppszLook)
         continue;   // already found

      // see if can find
      if (pLib->ResourceFind (pszName, -1) != -1)
         *ppszLook = pLib->m_szFile;
   } // i
}




/************************************************************************************
CMIFLProj::MethDefUsed - Given the name of a method that's standard, this looks
through all the libraries and finds the highest-ranking one with that definition.
It then returns a pointer to the method defintion.

inputs
   PWSTR          pszName - Name of method to look for
returns
   none
*/
PCMIFLMeth CMIFLProj::MethDefUsed (PWSTR pszName)
{
   DWORD i;
   for (i = 0; i < LibraryNum(); i++) {
      PCMIFLLib pLib = LibraryGet(i);

      DWORD dwFind = pLib->MethDefFind (pszName, -1);
      if (dwFind == -1)
         continue;

      // found...
      return pLib->MethDefGet (dwFind);
   } // i

   return NULL;
}




/************************************************************************************
CMIFLProj::ObjectEnum - Fills in a variable-sized list with the string name for
all the unique objects in all the libraries

inputs
   PCListVariable       pList - Filled with PWSTR of the uniquely named objects.
                        These are sorted.
   BOOL                 fIncludeDesc - If TRUE, then make a double-null terminated
                        string, the first one with the name, the second with a short
                        description.
   BOOL                 fSkipClasses - If TRUE then skip classes and only keep
                        objects marked as being created on startup.
returns
   none
*/
void CMIFLProj::ObjectEnum (PCListVariable pList, BOOL fIncludeDesc, BOOL fSkipClasses)
{
   pList->Clear();

   CListFixed lIndex;
   lIndex.Init (sizeof(DWORD));

   // figure out index into each of the libraries
   DWORD i;
   CMem mem;
   DWORD dwIndex = 0;
   lIndex.Required (LibraryNum());
   for (i = 0; i < LibraryNum(); i++)
      lIndex.Add (&dwIndex);
   DWORD *padwIndex = (DWORD*)lIndex.Get(0);
   DWORD dwNum = lIndex.Num();

   // loop while have indecies
   while (TRUE) {
      DWORD dwBest = -1;
      PCMIFLLib pLibBest = NULL;
      for (i = 0; i < dwNum; i++) {
         PCMIFLLib pLib = LibraryGet (i);
         if (padwIndex[i] >= pLib->ObjectNum())
            continue;   // out of range

         // if no current best then use this
         if (dwBest == -1) {
            dwBest = i;
            pLibBest = pLib;
            continue;
         }

         // else compare
         if (_wcsicmp( (PWSTR) pLib->ObjectGet(padwIndex[i])->m_memName.p,
            (PWSTR)pLibBest->ObjectGet(padwIndex[dwBest])->m_memName.p) < 0) {
               // found a better match
               dwBest = i;
               pLibBest = pLib;
            }
      } // i

      // if didn't find a best match then done
      if (dwBest == -1)
         break;

      // remember this doc index
      DWORD dwBestIndex = padwIndex[dwBest];

      // loop through and incrememnt all indecies that match the best,
      // eliminating dupliucates
      for (i = 0; i < dwNum; i++) {
         if (i == dwBest) {
            padwIndex[dwBest] += 1;
            continue;
         }

         PCMIFLLib pLib = LibraryGet (i);
         if (padwIndex[i] >= pLib->ObjectNum())
            continue;   // out of range

         if (_wcsicmp( (PWSTR) pLib->ObjectGet(padwIndex[i])->m_memName.p,
            (PWSTR)pLibBest->ObjectGet(dwBestIndex)->m_memName.p) == 0) {
               // match
               padwIndex[i] += 1;
               continue;
            }
      } // i, eliminate dups

      // now, create memory to store this
      PCMIFLObject pObject = pLibBest->ObjectGet(dwBestIndex);
      PWSTR psz = (PWSTR)pObject->m_memName.p;

      // dont add empty string
      if (!psz || !psz[0])
         continue;

      // BUGFIX - If want to ignore non-classes then do so
      if (fSkipClasses && !pObject->m_fAutoCreate)
         continue;

      // add
      if (fIncludeDesc) {
         DWORD dwLenName = ((DWORD)wcslen((PWSTR)pObject->m_memName.p)+1)*sizeof(WCHAR);
         DWORD dwLenDesc = ((DWORD)wcslen((PWSTR)pObject->m_memDescShort.p)+1)*sizeof(WCHAR);
         if (!mem.Required (dwLenName + dwLenDesc))
            continue;   // error
         memcpy (mem.p, pObject->m_memName.p, dwLenName);
         memcpy ((PBYTE)mem.p + dwLenName, pObject->m_memDescShort.p, dwLenDesc);

         // add
         pList->Add (mem.p, dwLenName + dwLenDesc);
      }
      else {
         pList->Add (psz, (wcslen(psz)+1)*sizeof(WCHAR));
      }
   } // while true

   // done
}


/************************************************************************************
CMIFLProj::ObjectInLib - Given an object name, this determines which libraries it's
in.

inputs
   PWSTR             pszName - Object name
   PCListFixed       plLib - Initialized to sizeof(PCMIFLLib) and filled with pointers
                     to the library that the object is in. The order is ranked from
                     highest priority to lowest
*/
void CMIFLProj::ObjectInLib (PWSTR pszName, PCListFixed plLib)
{
   plLib->Init (sizeof(PCMIFLLib));

   DWORD i;
   for (i = 0; i < LibraryNum(); i++) {
      PCMIFLLib pLib = LibraryGet(i);
      if (pLib->ObjectFind (pszName, -1) != -1)
         plLib->Add (&pLib);
   } // i
}


/********************************************************************************
CMIFLProj::ObjectAllSuper - Enumerates all superclasses of an object, in order
of priority.

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
BOOL CMIFLProj::ObjectAllSuper (PWSTR pszName, PCListVariable plList, PWSTR pszRecurse)
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

   // first thing, find all libraries that the object is in
   CListFixed lLib;
   ObjectInLib (pszName, &lLib);
   PCMIFLLib *ppl = (PCMIFLLib*) lLib.Get(0);

   // go through all the libraries and enumerate their contents
   for (i = 0; i < lLib.Num(); i++) {
      PCMIFLObject pObj = ppl[i]->ObjectGet(ppl[i]->ObjectFind(pszName, -1));
      if (!pObj)
         continue;

      // loop through all superclass objects
      for (j = 0; j < pObj->m_lClassSuper.Num(); j++)
         fRet &= ObjectAllSuper ((PWSTR)pObj->m_lClassSuper.Get(j), plList, pszRecurse);
   } // i

   return fRet;
}

/*********************************************************************************
MIFLOERListSearch - This searches for a name match in a CListVariable creted
to fill in a filtered list

inputs
   PWSTR             pszName - Name that looking for
   PCListFixed       pList - List of MIFLOER structures
returns
   DWORD - INdex, into pList, or -1 if cant find
*/
static DWORD MIFLOERListSearch (PWSTR pszName, PCListFixed pList)
{
   DWORD dwCur, dwTest;
   DWORD dwNum = pList->Num();
   PMIFLOER poe = (PMIFLOER)pList->Get(0);

   for (dwTest = 1; dwTest < dwNum; dwTest *= 2);
   for (dwCur = 0; dwTest; dwTest /= 2) {
      DWORD dwTry = dwCur + dwTest;
      if (dwTry >= dwNum)
         continue;

      // get it
      PWSTR psz = (PWSTR) poe[dwTry].plSuper->Get(0);
      if (!psz)
         continue;

      // see how compares
      int iRet = _wcsicmp(pszName, psz);
      if (iRet > 0) {
         // string occurs after this
         dwCur = dwTry;
         continue;
      }

      if (iRet == 0)
         return dwTry;
      
      // else, string occurs before. so throw out dwTry
      continue;
   } // dwTest

   if (dwCur >= dwNum)
      return -1;  // cant find at all

   // find match
   PWSTR psz = (PWSTR) poe[dwCur].plSuper->Get(0);
   if (!_wcsicmp(pszName, psz))
      return dwCur;

   return -1;
}



/********************************************************************************
CMIFLProj::ObjectEnumRelationship - Enumerates all objects and provides information
about their relationships with one another too.

inputs
   PCListFixed       plList - Initialized to sizeof(MIFLOER) and then filled with
                     information. Note: the MIFLOER elements point to a CListVariable
                     that MUST be freed by the caller. This list is sorted alphabetically
                     by the object name.
   BOOL              fSortByCount - If TRUE then the outputted list is sorted so the
                     most-used classes are first in the list.
returns
   none
*/


static int _cdecl MIFLOERCompare (const void *elem1, const void *elem2)
{
   PMIFLOER pdw1, pdw2;
   pdw1 = (PMIFLOER) elem1;
   pdw2 = (PMIFLOER) elem2;

   if (pdw2->dwSubCount != pdw1->dwSubCount)
      return (int)pdw2->dwSubCount - (int)pdw1->dwSubCount;

   // else
   return _wcsicmp((PWSTR)pdw1->plSuper->Get(0), (PWSTR)pdw2->plSuper->Get(0));
}


void CMIFLProj::ObjectEnumRelationship (PCListFixed plList, BOOL fSortByCount)
{
   plList->Init (sizeof(MIFLOER));

   CListVariable lObj;
   ObjectEnum (&lObj);

   DWORD i, j;
   MIFLOER oe;
   memset (&oe, 0, sizeof(oe));
   for (i = 0; i < lObj.Num(); i++) {
      oe.dwSubCount = 0;
      oe.plSuper = new CListVariable;
      if (!oe.plSuper)
         continue;
      oe.fSelfRef = !ObjectAllSuper ((PWSTR)lObj.Get(i), oe.plSuper);

      // add name to start of list
      oe.plSuper->Insert (0, lObj.Get(i), lObj.Size(i));

      plList->Add (&oe);
   } // i

   // do reference count
   PMIFLOER poe = (PMIFLOER)plList->Get(0);
   for (i = 0; i < plList->Num(); i++) {
      for (j = 1; j < poe[i].plSuper->Num(); j++) {
         PWSTR psz = (PWSTR) poe[i].plSuper->Get(j);
         DWORD dwIndex = MIFLOERListSearch (psz, plList);
         if (dwIndex == -1)
            continue;

         // else, inc count
         poe[dwIndex].dwSubCount += 1;
      } // j
   } // i

   // sort
   if (fSortByCount)
      qsort (plList->Get(0), plList->Num(), sizeof(MIFLOER), MIFLOERCompare);
}



/*****************************************************************************
HelpTimerFunc - Timer function used to make sure that if user clicks on button
for page-link then actually goes there, if if a dialog is running.
*/
void CALLBACK HelpTimerFunc (HWND hWnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime)
{
   if (gpProjHasHelp)
      gpProjHasHelp->HelpCheckPage();
}

/*****************************************************************************
HelpDefPage - Default page callback. It handles standard operations
*/
BOOL HelpDefPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCMIFLProj pProj = (PCMIFLProj)pPage->m_pUserData;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
      pPage->m_pWindow->IconSet (LoadIcon(ghInstance, MAKEINTRESOURCE(IDI_APPICON)));

      // if we have somplace to scroll to then do so
      if (pProj->m_CurHistory.iVScroll >= 0) {
         pPage->VScroll (pProj->m_CurHistory.iVScroll);

         // when bring up pop-up dialog often they're scrolled wrong because
         // iVScoll was left as valeu, and they use defpage
         pProj->m_CurHistory.iVScroll = 0;

         // BUGFIX - putting this invalidate in to hopefully fix a refresh
         // problem when add or move a task in the ProjectView page
         pPage->Invalidate();
      }
      else {
         // if the current page string has a "#" in it then skip to that
         // section
         if (pProj->m_CurHistory.iSection >= 0)
            pPage->VScrollToSection (pProj->m_CurHistory.szLink + pProj->m_CurHistory.iSection);
      }

      }
      return TRUE;


   case ESCM_LINK:
      {
         PESCMLINK p = (PESCMLINK) pParam;
         if (!p->psz)
            break;

         // invoke code
         PWSTR pszInvoke = L"invoke:";
         DWORD dwInvokeLen = (DWORD)wcslen(pszInvoke);
         if (!wcsncmp (p->psz, pszInvoke, dwInvokeLen)) {
            if (!pProj->HelpInvokeCode (p->psz + dwInvokeLen))
               pPage->m_pWindow->Beep (ESCBEEP_DONTCLICK);
            return TRUE;
         }

         // if user presses option to go back to toc
         if (!_wcsicmp(p->psz, L"gototoc")) {
            WCHAR szTemp[64];
            swprintf (szTemp, L"s:%d", (int)pProj->m_dwHelpTOC);
            pPage->Exit (szTemp);
            return TRUE;
         }
         else if (!_wcsicmp(p->psz, L"rebuildhelp")) {
            pProj->HelpRebuild (pPage->m_pWindow->m_hWnd);

            // BUGFIX - Dont bother pPage->MBInformation (L"Help is rebuilt.");

            WCHAR szTemp[64];
            swprintf (szTemp, L"s:%d", (int)pProj->m_dwHelpTOC);
            pPage->Exit (szTemp);
            return TRUE;
         }
      }
      break;
   };


   return FALSE;
}


/***********************************************************************
CMIFLProj::UpdateSearchList - If this is the search window then it fills the
search listbox with the search items

inputs
   PCEscPage      pPage
*/
void CMIFLProj::UpdateSearchList (PCEscPage pPage)
{
   PCEscControl pc;
   pc = pPage->ControlFind(L"searchlist");
   if (!pc)
      return;  // not the search window

   // clear out the current elemnts
   pc->Message (ESCM_LISTBOXRESETCONTENT);

   // add them in
   WCHAR szHuge[1024];
   WCHAR szTitle[512], szSection[512], szLink[512];
   size_t dwNeeded;

   DWORD i;
   PWSTR psz;
   for (i = 0; i < min(50,m_pSearch->m_listFound.Num()); i++) {
      psz = (PWSTR) m_pSearch->m_listFound.Get(i);
      if (!psz)
         continue;

      // the search info is packed, so unpack and convert so
      // none of the characters interfere with MML

      // scorew
      DWORD dwScore;
      dwScore = *((DWORD*) psz);
      psz += (sizeof(DWORD)/sizeof(WCHAR));

      // document title
      szTitle[0] = 0;
      StringToMMLString (psz, szTitle, sizeof(szTitle), &dwNeeded);
      psz += (wcslen(psz)+1);

      // section title
      szSection[0] = 0;
      StringToMMLString (psz, szSection, sizeof(szSection), &dwNeeded);
      psz += (wcslen(psz)+1);

      // link
      szLink[0] = 0;
      StringToMMLString (psz, szLink, sizeof(szLink), &dwNeeded);

      // combine this into 1 large mml string
      swprintf (szHuge,
         L"<elem name=\"%s\">"
            L"<br><bold>Document: %s</bold></br>"
            L"<br>&tab;Section: %s</br>"
            L"<br>&tab;Score: %g</br>"
         L"</elem>",
         szLink,
         szTitle[0] ? szTitle : L"Unknown",
         szSection[0] ? szSection : L"None",
         (double) (dwScore/100) / 10
         );

      // add to the list
      ESCMLISTBOXADD a;
      memset (&a, 0, sizeof(a));
      a.dwInsertBefore = (DWORD)-1;
      a.pszMML = szHuge;
      pc->Message (ESCM_LISTBOXADD, &a);
   }

   // done
}


/***********************************************************************************
FindPageInfo - Searches through the MML tree looking for page info.

inputs
   PCMMLNode2      pNode - node to search through
returns
   PCMMLNode2 - PageInfo node, or NULL if can't find
*/
PCMMLNode FindPageInfo (PCMMLNode pNode)
{
   // is this it?
   PWSTR pszName;
   pszName = pNode->NameGet();
   if (pszName && !_wcsicmp(pszName, L"PageInfo"))
      return pNode;

   // else children
   DWORD i;
   PWSTR psz;
   PCMMLNode   pSub;
   for (i = 0; i < pNode->ContentNum(); i++) {
      pSub = NULL;
      pNode->ContentEnum(i, &psz, &pSub);
      if (!pSub) continue;

      // must be right type
      if (pSub->m_dwType != MMLCLASS_ELEMENT)
         continue;

      // if it's a control then skip because controls might
      // contain pageinfo
      psz = pSub->NameGet();
      if (psz && EscControlGet (psz))
         continue;

      PCMMLNode   pRet;
      pRet = FindPageInfo (pSub);
      if (pRet)
         return pRet;

      // else continue
   }

   return NULL;   // cant find
}



/*****************************************************************************
SearchCallback - Called by search engine
*/
BOOL __cdecl SearchCallback (CEscSearch *pSearch, DWORD dwDocument, PVOID pUserData)
{
   PCMIFLProj pProj = (PCMIFLProj) pUserData;

   PWSTR psz = (PWSTR)pProj->m_lHelpIndex.Get(dwDocument);
   if (!psz || !psz[0])
      return TRUE;

   // convert the MML
   CEscError err;
   PCMMLNode pNode = ParseMML (psz, ghInstance, NULL, NULL, &err, TRUE);
   if (!pNode)
      return TRUE;

   BOOL fRet;

   // BUGFIX - find the title up here
   PWSTR pszName = L"Document";
   PCMMLNode pSub;
   pSub = FindPageInfo (pNode);
   // find the name
   if (pSub) {
      psz = pSub->AttribGet (L"title");
      if (psz)
         pszName = psz;
   }
   WCHAR szTemp[64];
   swprintf (szTemp, L"s:%d", (int) dwDocument);

   fRet = pSearch->IndexNode (pNode, pszName, szTemp);
   fRet = pSearch->SectionFlush ();

   // finally
   delete pNode;
   return TRUE;

}

/*****************************************************************************
CMIFLProj::SearchIndex - Redoes the search index.

inputs
   PCEscPage pPage - page
*/
void CMIFLProj::SearchIndex (PCEscPage pPage)
{
   ESCINDEX i;
   memset (&i, 0, sizeof(i));
   i.hWndUI = pPage->m_pWindow->m_hWnd;
   i.pCallback = HelpDefPage;
   i.fNotEnumMML = TRUE;
   i.pIndexCallback = SearchCallback;
   i.dwIndexDocuments = m_lHelp.Num();
   i.pIndexUserData = this;

#ifdef _DEBUG
   DWORD dwStart = GetTickCount();
#endif
   m_pSearch->Index (&i);
#ifdef _DEBUG
   char szTemp[64];
   sprintf (szTemp, "\r\nTook %g seconds", (double)(GetTickCount()-dwStart)/1000.0);
   OutputDebugString (szTemp);
#endif
}

/*****************************************************************************
SearchPage - Search page callback. It handles standard operations
*/
BOOL SearchPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCMIFLProj pProj = (PCMIFLProj)pPage->m_pUserData;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         // if this is the seach window then set the search text and fill
         // in the list box
         PCEscControl   pc;
         pc = pPage->ControlFind (L"SearchString");
         if (pc && pProj->m_pSearch->m_pszLastSearch)
            pc->AttribSet (L"text", pProj->m_pSearch->m_pszLastSearch);
         pProj->UpdateSearchList(pPage);

      }
      break;   // so default init happens


      case ESCN_LISTBOXSELCHANGE:
         {
            PESCNLISTBOXSELCHANGE p = (PESCNLISTBOXSELCHANGE) pParam;

            // only care about the search list control
            if (!p->pControl->m_pszName || _wcsicmp(p->pControl->m_pszName, L"searchlist"))
               return TRUE;

            // if no name then ignore
            if (!p->pszName)
               return TRUE;

            // use the name as a link
            pPage->Link(p->pszName);
         }
         return TRUE;

   case ESCM_LINK:
      {
         PESCMLINK p = (PESCMLINK) pParam;

         if (!_wcsicmp(p->psz, L"searchbutton")) {
            // get the text to search for
            WCHAR szTemp[512];
            DWORD dwNeeded;
            szTemp[0] = 0;
            PCEscControl   pc;
            pc = pPage->ControlFind (L"SearchString");
            if (pc)
               pc->AttribGet (L"text", szTemp, sizeof(szTemp), &dwNeeded);

            // index the search if it need reindexing because of a new app version
            if (pProj->m_pSearch->NeedIndexing())
               pProj->SearchIndex (pPage);

            // search
            pProj->m_pSearch->Search (szTemp);

            // update the list
            pProj->UpdateSearchList(pPage);

            // inform user that search complete
            EscChime (ESCCHIME_INFORMATION);
            EscSpeak (L"Search finished.");

            return TRUE;
         }
      }
      break;   // so default search behavior happens


   };


   return HelpDefPage (pPage, dwMessage, pParam);
}




/*****************************************************************************
IndexPage - Search page callback. It handles standard operations
*/
BOOL IndexPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCMIFLProj pProj = (PCMIFLProj)pPage->m_pUserData;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         // update the list
         pPage->Message (ESCM_USER+430);
      }
      break;   // so default init happens

   case ESCM_USER+430: // update the list
      {
         // if this is the seach window then set the search text and fill
         // in the list box
         PCEscControl   pControl;
         WCHAR szTemp[128];
         DWORD dwNeeded;
         pControl = pPage->ControlFind (L"SearchString");
         szTemp[0] = 0;
         if (pControl)
            pControl->AttribGet (Text(), szTemp, sizeof(szTemp), &dwNeeded);
         _wcslwr (szTemp); // since all hash values also lower

         DWORD dwMatchStart, dwMatchEnd;
         if (szTemp[0]) {
            DWORD dwLen = (DWORD)wcslen(szTemp);
            // NOTE: being very lazy and not doing a binary search
            for (dwMatchStart = 0; dwMatchStart < pProj->m_lPWSTRIndex.Num(); dwMatchStart++)
               if (wcsncmp (szTemp, *((PWSTR*)pProj->m_lPWSTRIndex.Get(dwMatchStart)), dwLen) <= 0)
                  break;   // found start

            // find end
            for (dwMatchEnd = dwMatchStart; dwMatchEnd < pProj->m_lPWSTRIndex.Num(); dwMatchEnd++)
               if (wcsncmp (szTemp, *((PWSTR*)pProj->m_lPWSTRIndex.Get(dwMatchEnd)), dwLen) < 0)
                  break;   // found end
         }
         else {
            // show everything
            dwMatchStart = 0;
            dwMatchEnd = pProj->m_lPWSTRIndex.Num();
         }

         // fill in the list
         pControl = pPage->ControlFind (L"searchlist");
         if (!pControl)
            return TRUE;   // error
         pControl->Message (ESCM_LISTBOXRESETCONTENT);

         MemZero (&gMemTemp);
         DWORD i;
         for (i = dwMatchStart; i < dwMatchEnd; i++) {
            PWSTR psz = *((PWSTR*)pProj->m_lPWSTRIndex.Get(i));
            MemCat (&gMemTemp, L"<elem name=\"");
            MemCatSanitize (&gMemTemp, psz);
            MemCat (&gMemTemp, L"\">");
            MemCatSanitize (&gMemTemp, psz);
            MemCat (&gMemTemp, L"</elem>");
         } // i

         ESCMLISTBOXADD lba;
         memset (&lba, 0, sizeof(lba));
         lba.dwInsertBefore = (DWORD)-1;
         lba.pszMML = (PWSTR)gMemTemp.p;
         pControl->Message (ESCM_LISTBOXADD, &lba);

         pControl->AttribSetInt (L"cursel", -1);   // so no selection
      }
      return TRUE;

   case ESCN_EDITCHANGE:
      {
         PESCNEDITCHANGE p = (PESCNEDITCHANGE) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;
         PWSTR psz = p->pControl->m_pszName;

         if (!_wcsicmp(psz, L"SearchString")) {
            // update the list
            pPage->Message (ESCM_USER+430);
            return TRUE;
         }
      }

   case ESCN_LISTBOXSELCHANGE:
      {
         PESCNLISTBOXSELCHANGE p = (PESCNLISTBOXSELCHANGE) pParam;

         // only care about the search list control
         if (!p->pControl->m_pszName || _wcsicmp(p->pControl->m_pszName, L"searchlist"))
            break;

         // if no name then ignore
         if (!p->pszName)
            return TRUE;

         MemZero (&gMemTemp);
         MemCat (&gMemTemp, L"index:");
         MemCat (&gMemTemp, p->pszName);
         pPage->Link ((PWSTR)gMemTemp.p);
      }
      return TRUE;

   };


   return HelpDefPage (pPage, dwMessage, pParam);
}

/*****************************************************************************
CMIFLProj::ParseURL - Parses a URL into a MIFLHISTORY structure.

inputs
   PWSTR    pszURL - url
   PMIFLHISTORY pHistory - filled in
returns
   none
*/
void CMIFLProj::ParseURL (PWSTR pszURL, PMIFLHISTORY pHistory)
{
   memset (pHistory, 0, sizeof(*pHistory));

   if (!pszURL) {
      return;
   }

   pHistory->fResolved = TRUE;

   // copy over the string
   if (wcslen(pszURL) < sizeof(pHistory->szLink)/2)
      wcscpy (pHistory->szLink, pszURL);
   else {
      memcpy (pHistory->szLink, pszURL, sizeof(pHistory->szLink)/2);
      pHistory->szLink[sizeof(pHistory->szLink)/2-1] = 0;
   }

   // see if there's a #
   pHistory->iVScroll = -1;
   pHistory->iSection = -1;
   PWSTR psz;
   psz = wcschr(pszURL, L'#');
   if (psz) {
      pHistory->iSection = (int)((size_t) (PBYTE) psz - (size_t) (PBYTE) pszURL) / 2 + 1;
   }

   // figure out what resource
   psz = pszURL;
   if (!_wcsnicmp (psz, L"s:", 2)) {
      // it's another page
      DWORD dwPage;
      dwPage = _wtoi (psz + 2);

      pHistory->iSlot = (int)dwPage;
   }
   else if (!_wcsnicmp (psz, L"r:", 2)) {
      // it's another page
      DWORD dwPage;
      dwPage = _wtoi (psz + 2);

      pHistory->iSlot = -(int)dwPage;
   }
   else if (!_wcsnicmp (psz, L"index:", 6)) {
      PHELPINDEX phi = (PHELPINDEX) m_hHELPINDEX.Find (psz + 6);
      if (phi && (phi->iSlot >= 0))
         pHistory->iSlot = phi->iSlot;
      else
         pHistory->iSlot = 0; // shouldnt happen, but just in case
   }
   else if (!_wcsicmp (psz, Back())) {
      PMIFLHISTORY pOld;
      pOld = (PMIFLHISTORY) m_ListHistory.Get (0);
      if (!pOld) {
         // cant go back, so go to starting url
         ParseURL (gszStartingURL, pHistory);
         return;
      }


      // else, use that
      *pHistory = *pOld;

      // must do some reparsing if v:
      psz = pHistory->szLink;
      // delete the back
      m_ListHistory.Remove (0);

      return;
   }
   else {
      // quit
      pHistory->fResolved = FALSE;
      return;
   }


   // assume data is -1
   pHistory->dwData = (DWORD)-1;
}


/*****************************************************************************
CMIFLProj::DetermineCallback - Given a resource ID, this returns the pagecallback
to be used.

inputs
   DWORD       dwID - MML resource iD
returns
   PCESCPAGECALLBACK - callback
*/
PESCPAGECALLBACK CMIFLProj::DetermineCallback (DWORD dwID)
{

   switch (dwID) {
   case IDR_HSEARCH:
      return SearchPage;

   case IDR_HINDEX:
      return IndexPage;

   default:
      return HelpDefPage;
   }
}


/**************************************************************************************
CMIFLProj::ShowPage - Given a string, shows that page. NOTE: Only call this if know that no
page is currently running.

inputs
   PWSTR       psz - String
*/
void CMIFLProj::ShowPage (PWSTR psz)
{
   // only one project can have help at a time
   if (gpProjHasHelp && (gpProjHasHelp != this))
      return;

   // create the help window if not there
   if (!m_pHelpWindow) {
      m_pHelpWindow = new CEscWindow;
      if (!m_pHelpWindow)
         return;

      // figure out where should appear
      RECT r;
      DialogBoxLocation3 (NULL, &r, FALSE);

      if (!m_pHelpWindow->Init (ghInstance, NULL, 0, &r)) {
         delete m_pHelpWindow;
         m_pHelpWindow =NULL;
         return;
      }

      // topmost
      // BUGFIX - No SetWindowPos (m_pHelpWindow->m_hWnd, (HWND)HWND_TOPMOST, 0, 0,
      //    0, 0, SWP_NOMOVE | SWP_NOSIZE);

      // create the timer
      if (!m_dwHelpTimer) {
         m_dwHelpTimer = SetTimer (NULL, NULL, 250, HelpTimerFunc);
         gpProjHasHelp = this;
      }
   }

   // BUGFIX - If error in loading go to page error
   if (!psz) {
      psz = gszPageErrorID;
   }

   // as long as we're not back, add the old one to current hisotry
   if (!_wcsicmp (psz, RedoSamePage())) {
      // page requested to redo itself
      m_CurHistory.iVScroll = m_pHelpWindow->m_iExitVScroll;
      goto showit;
   }
   else if (_wcsicmp(psz, Back()) && m_CurHistory.szLink[0]) {
      m_CurHistory.iVScroll = m_pHelpWindow->m_iExitVScroll;
      m_ListHistory.Insert (0, &m_CurHistory);
   }

   // now partse
   ParseURL (psz, &m_CurHistory);

showit:
   if (!m_CurHistory.fResolved) {
      // delete the window, unknown, so we're done
      delete m_pHelpWindow;
      m_pHelpWindow = NULL;
      if (m_dwHelpTimer) {
         KillTimer (NULL, m_dwHelpTimer);
         m_dwHelpTimer = 0;
         gpProjHasHelp = NULL;
      }
      return;
   }

   EscFontScaleSet (m_fSmallFont ? 0xc0 : 0x100);
   BOOL fRet;
   if (m_CurHistory.iSlot < 0)
      fRet = m_pHelpWindow->PageDisplay (ghInstance, (DWORD)(-m_CurHistory.iSlot), DetermineCallback((DWORD)(-m_CurHistory.iSlot)), this);
   else {
      PWSTR psz = (PWSTR)m_lHelp.Get ((DWORD)m_CurHistory.iSlot);
      if (psz)
         fRet = m_pHelpWindow->PageDisplay (ghInstance, psz, HelpDefPage, this);
      else
         fRet = FALSE;
   }
   if (!fRet) {
      // delete the window, unknown, so we're done
      delete m_pHelpWindow;
      m_pHelpWindow = NULL;
      if (m_dwHelpTimer) {
         KillTimer (NULL, m_dwHelpTimer);
         m_dwHelpTimer = 0;
         gpProjHasHelp = NULL;
      }
      return;
   }

   // done
}


/**************************************************************************************
CMIFLProj::HelpInvokeCode - This is called when the help file tells the editor
to bring up the page/code for the object.

inputs
   PWSTR          pszName - Name of the object, method, property, etc.
returns
   BOOL - TRUE if succes, FALSE if failed
*/
BOOL CMIFLProj::HelpInvokeCode (PWSTR pszName)
{
   if (!m_pPageCurrent || !IsWindowEnabled(m_pPageCurrent->m_pWindow->m_hWnd))
      return FALSE;

   // look through the project
   DWORD i;
   WCHAR szLink[128];
   szLink[0] = 0;
   for (i = 0; i < LibraryNum(); i++) {
      PCMIFLLib pLib = LibraryGet(i);
      DWORD dwIndex;

      // object?
      dwIndex = pLib->ObjectFind (pszName, (DWORD)-1);
      if (dwIndex != (DWORD)-1) {
         PCMIFLObject pObj = pLib->ObjectGet(dwIndex);
         swprintf (szLink, L"lib:%dobject:%dedit", (int)pLib->m_dwTempID, (int)pObj->m_dwTempID);
         break;
      }

      dwIndex = pLib->MethDefFind (pszName, (DWORD)-1);
      if (dwIndex != (DWORD)-1) {
         PCMIFLMeth pMeth = pLib->MethDefGet (dwIndex);
         swprintf (szLink, L"lib:%dmethdef:%dedit", (int)pLib->m_dwTempID, (int)pMeth->m_dwTempID);
         break;
      }

      dwIndex = pLib->GlobalFind (pszName, (DWORD)-1);
      if (dwIndex != (DWORD)-1) {
         PCMIFLProp pProp = pLib->GlobalGet (dwIndex);
         swprintf (szLink, L"lib:%dglobal:%dedit", (int)pLib->m_dwTempID, (int)pProp->m_dwTempID);
         break;
      }

      dwIndex = pLib->FuncFind (pszName, (DWORD)-1);
      if (dwIndex != (DWORD)-1) {
         PCMIFLFunc pFunc = pLib->FuncGet (dwIndex);
         swprintf (szLink, L"lib:%dfunc:%dedit", (int)pLib->m_dwTempID, (int)pFunc->m_Meth.m_dwTempID);
         break;
      }

      dwIndex = pLib->PropDefFind (pszName, (DWORD)-1);
      if (dwIndex != (DWORD)-1) {
         PCMIFLProp pProp = pLib->PropDefGet (dwIndex);
         swprintf (szLink, L"lib:%dpropdef:%dedit", (int)pLib->m_dwTempID, (int)pProp->m_dwTempID);
         break;
      }
   } // i

   // make sure can find
   if (!szLink[0])
      return FALSE;

   // found, so tell page to switch
   m_pPageCurrent->Link (szLink);
   return TRUE;
}


/**************************************************************************************
CMIFLProj::HelpCheckPage - Called in the main message loop after every message and checks
to see that page links are parsed correctly.
*/
void CMIFLProj::HelpCheckPage (void)
{
   if (!m_pHelpWindow)
      return;

   if (!m_pHelpWindow->m_pszExitCode)
      return;

   // else, and exit code
   PWSTR psz = m_pHelpWindow->m_pszExitCode;

   // get rid of the page if its still there
   m_pHelpWindow->PageClose ();

   ShowPage (psz);
}



/*****************************************************************************
CMIFLProj::Help - Does the main loop for help

inputs
   PCEscPage      pPage - Page to display messages from
   DWORD    dwRes - Resource ID to pull up. NOTE: ignoring this for now
returns
   none
*/
void CMIFLProj::Help (PCEscPage pPage, DWORD dwRes)
{
   // consider rebuilding help
   if (!m_lHelp.Num()) {
      if (!HelpRebuild (pPage->m_pWindow->m_hWnd))
         m_lHelp.Clear();
      if (!m_lHelp.Num()) {
         pPage->MBInformation (L"You don't have any libraries loaded.",
            L"The help text is produced based on what libraries you have loaded. Since "
            L"there are no libraries, help is unavailable.");
         return;
      }
   }

   dwRes = m_dwHelpTOC;

   WCHAR szStart[32];
   swprintf (szStart, L"s:%d", dwRes);

   // show window if not visible
   if (m_pHelpWindow && IsIconic(m_pHelpWindow->m_hWnd))
      ShowWindow (m_pHelpWindow->m_hWnd, SW_RESTORE);

   if (m_pHelpWindow && m_pHelpWindow->m_pPage) {
      m_pHelpWindow->m_pPage->Exit (szStart);
   }
   else {
      m_ListHistory.Init (sizeof(MIFLHISTORY));
      memset (&m_CurHistory, 0, sizeof(m_CurHistory));

      ShowPage (szStart);
   }
}


/********************************************************************************
CMIFLProj::HelpMML - Returns the MML string to incoporate help,
usually "<a href="index:TOPIC">?</a>".

inputs
   PCMem          pMem - Memory to MemCat to
   PWSTR          pszTopic - Topic string to index
   BOOL           fIndexPrefix - If TRUE then include "index:" as a prefix.
*/
void CMIFLProj::HelpMML (PCMem pMem, PWSTR pszTopic, BOOL fIndexPrefix)
{
   MemCat (pMem, L"<a href=\"");
   if (fIndexPrefix)
      MemCat (pMem, L"index:");
   MemCatSanitize (pMem, pszTopic);
   MemCat (pMem, L"\"><bold>?</bold></a>");
}

/********************************************************************************
CMIFLProj::HelpIndex - Pulls up help, but indexes to a specific topic.

inputs
   PCEscPage      pPage - Page to display messages from
   PWSTR          pszKeyword - Index topic, or might be a paraemeter, in which
                     case will look for an index entry in the parse
   BOOL           fParse - If TRUE, then assume it's code, so look for something
                     indexable. If FALSE, then must match entirely
returns
   BOOL - TRUE if success. FALSE if fail, and display error.
*/
BOOL CMIFLProj::HelpIndex (PCEscPage pPage, PWSTR pszKeyword, BOOL fParse)
{
   // consider rebuilding help
   if (!m_lHelp.Num()) {
      if (!HelpRebuild (pPage->m_pWindow->m_hWnd))
         m_lHelp.Clear();
      if (!m_lHelp.Num()) {
         pPage->MBInformation (L"You don't have any libraries loaded.",
            L"The help text is produced based on what libraries you have loaded. Since "
            L"there are no libraries, help is unavailable.");
         return FALSE;
      }
   }

   // try to find the keyword
   DWORD dwIndex = (DWORD)-1;
   if (fParse) {
      if (!HelpParseStringForToken (pszKeyword, &m_hHELPINDEX, TRUE, &dwIndex))
         dwIndex = (DWORD)-1;
   }
   else
      dwIndex = m_hHELPINDEX.FindIndex (pszKeyword);
   PHELPINDEX phi = (dwIndex != (DWORD)-1) ? (PHELPINDEX)m_hHELPINDEX.Get(dwIndex) : NULL;
   if (!phi || (phi->iSlot < 0)) {
      pPage->MBWarning (L"No appropriate help topic was found.");
      return FALSE;
   }

   // display the help
   WCHAR szStart[32];
   swprintf (szStart, L"s:%d", phi->iSlot);

   // show window if not visible
   if (m_pHelpWindow && IsIconic(m_pHelpWindow->m_hWnd))
      ShowWindow (m_pHelpWindow->m_hWnd, SW_RESTORE);

   if (m_pHelpWindow && m_pHelpWindow->m_pPage) {
      m_pHelpWindow->m_pPage->Exit (szStart);
   }
   else {
      m_ListHistory.Init (sizeof(MIFLHISTORY));
      memset (&m_CurHistory, 0, sizeof(m_CurHistory));

      ShowPage (szStart);
   }

   return TRUE;
}

/********************************************************************************
CMIFLProj::Init - Sets up the globals for search
*/
void CMIFLProj::HelpInit (void)
{
   if (m_pSearch)
      delete m_pSearch;

   m_pSearch = new CEscSearch;
   if (!m_pSearch)
      return;

   // init the search
   WCHAR szFile[256];
   MultiByteToWideChar (CP_ACP, 0, gszAppDir, -1, szFile, sizeof(szFile)/2);
   wcscat (szFile, L"MIFLHelp.xsr");
   // initialize the search, use a hash of the date for the version
   DWORD dwSum;
   dwSum = GetTickCount();
   m_pSearch->Init (ghInstance, dwSum, szFile);
}


/*****************************************************************************************
CMIFLProj::HelpEnd - Shuts down help if it's still around.
*/
void CMIFLProj::HelpEnd (void)
{
   if (m_pHelpWindow) {
      m_pHelpWindow->PageClose();
      delete m_pHelpWindow;
   }
   m_pHelpWindow = NULL;
   if (m_dwHelpTimer) {
      KillTimer (NULL, m_dwHelpTimer);
      m_dwHelpTimer = 0;
      gpProjHasHelp = NULL;
   }

   HelpIndexClear ();
}



/*****************************************************************************************
MIFLHelpHeader - Creates the header for a help MML.

inputs
   PCMem       pMem - Mem to fill in. This clears the mem to 0-length and then appends
   PWSTR       pszTitle - Title of the page
   PWSTR       pszType - Type of information (this can be NULL)
   BOOL        fInvoke - If TRUE, provide a link back to the actual code
*/
static void MIFLHelpHeader (PCMem pMem, PWSTR pszTitle, PWSTR pszType, BOOL fInvoke)
{
   MemZero (pMem);
   MemCat (pMem, L"<?template resource=502?><?Include resource=500?><PageInfo title=\"");
   MemCatSanitize (pMem, pszTitle);
   if (pszType) {
      MemCat (pMem, L" (");
      MemCatSanitize (pMem, pszType);
      MemCat (pMem, L")");
   }
   MemCat (pMem, L"\"/><xPageTitle>");
   MemCatSanitize (pMem, pszTitle);
   if (pszType) {
      MemCat (pMem, L" (");
      MemCatSanitize (pMem, pszType);
      MemCat (pMem, L")");
   }

   if (fInvoke) {
      MemCat (pMem, L"<br/><small><a href=\"invoke:");
      MemCatSanitize (pMem, pszTitle);
      MemCat (pMem, L"\">Modify</a></small>");
   }

   MemCat (pMem, L"</xPageTitle>");
}



/*****************************************************************************************
HelpFillSlot - Fills a slot for the given help topic.

inputs
   PCListVariable plHelp - Help to fill in
   PCListVariable plHelpIndex - Help to fill in for indexing purposes. Shorter, missing some extra links
   PCMem       pMem - Memory containing the string to use
   PCMem       pMemIndex - Memory containing the string to use for the index. Can be NULL.
   int       dwSlot - Slot number, 0+
returns
   BOOL - TRUE if success
*/
static BOOL HelpFillSlot (PCListVariable plHelp, PCListVariable plHelpIndex, PCMem pMem, PCMem pMemIndex, int iSlot)
{
   // fill in blanks until get to slot
   PWSTR pszNULL = L"";
   BOOL fTemp = FALSE;
   while (plHelp->Num() <= (DWORD)iSlot) {
      plHelp->Add (pszNULL, (wcslen(pszNULL)+1)*sizeof(WCHAR));
      plHelpIndex->Add (pszNULL, (wcslen(pszNULL)+1)*sizeof(WCHAR));
   }

   // set it
   plHelp->Set ((DWORD)iSlot, pMem->p, (wcslen((PWSTR)pMem->p)+1)*sizeof(WCHAR));
   if (pMemIndex)
      plHelpIndex->Set ((DWORD)iSlot, pMemIndex->p, (wcslen((PWSTR)pMemIndex->p)+1)*sizeof(WCHAR));
   else
      plHelpIndex->Set ((DWORD)iSlot, pszNULL, (wcslen(pszNULL)+1)*sizeof(WCHAR));

   return TRUE;
}


/*****************************************************************************************
HTOCNODEFree - Frees the HTOCNODE and anything it points to

inputs
   PHTOCNODE         pNode - Node
*/
void HTOCNODEFree (PHTOCNODE pNode)
{
   if (pNode->plHTOCENTRY)
      delete pNode->plHTOCENTRY;
   if (pNode->ptHTOCNODE) {
      // sub elems
      DWORD i;
      for (i = 0; i < pNode->ptHTOCNODE->Num(); i++) {
         PWSTR psz = pNode->ptHTOCNODE->Enum (i);
         PHTOCNODE pSub = (PHTOCNODE) pNode->ptHTOCNODE->Find (psz);
         HTOCNODEFree (pSub);
      } // i

      delete pNode->ptHTOCNODE;
   }
}

/*****************************************************************************************
HTOCNODEAdd - Adds an entry to the TOC.

inputs
   PWSTR          pszHelpCat - Help category. A series of strings divided by slash.
   PWSTR          pszName - Name of the help topic
   PWSTR          pszDescShort - Short descritpion of the help topic
   int            iSlot - Help slot this goes in
   PHTOCNODE      pNode - Node to add to
returns
   BOOL - TRUE if success
*/
BOOL HTOCNODEAdd (PWSTR pszHelpCat, PWSTR pszName, PWSTR pszDescShort, int iSlot, PHTOCNODE pNode)
{
   // if there's still a help category then need to recurce
   if (pszHelpCat[0]) {
      // find the next slash
      PWSTR pszSlash = wcschr (pszHelpCat, L'/');
      if (pszSlash == pszHelpCat+1)
         // slash occurs right away, so skip
         return HTOCNODEAdd (pszHelpCat+1, pszName, pszDescShort, iSlot, pNode);

      // if there's no curent tree then create one
      if (!pNode->ptHTOCNODE)
         pNode->ptHTOCNODE = new CBTree;
      if (!pNode->ptHTOCNODE)
         return FALSE;  // error

      // zero out the slash so have a complete string
      WCHAR cTemp = pszSlash ? pszSlash[0] : 0;
      if (pszSlash)
         pszSlash[0] = 0;

      // find this node
      PHTOCNODE pSub = (PHTOCNODE)pNode->ptHTOCNODE->Find(pszHelpCat);
      if (!pSub) {
         HTOCNODE Zero;
         memset (&Zero, 0, sizeof(Zero));
         pNode->ptHTOCNODE->Add (pszHelpCat, &Zero, sizeof(Zero));
         pSub = (PHTOCNODE)pNode->ptHTOCNODE->Find(pszHelpCat);
      }
      if (!pSub) {
         if (pszSlash)
            pszSlash[0] = cTemp;
         return FALSE;
      }

      // restore the string
      if (pszSlash)
         pszSlash[0] = cTemp;

      // recurse
      return HTOCNODEAdd (pszSlash ? (pszSlash+1) : (pszHelpCat + wcslen(pszHelpCat)),
         pszName, pszDescShort, iSlot, pSub);
   }

   // if get here then there's not category so add to list

   // make sure there is a list
   if (!pNode->plHTOCENTRY) {
      pNode->plHTOCENTRY = new CListFixed;
      if (!pNode->plHTOCENTRY)
         return FALSE;
      pNode->plHTOCENTRY->Init (sizeof(HTOCENTRY));
   }

   // add it
   HTOCENTRY e;
   memset (&e, 0, sizeof(e));
   e.iSlot = iSlot;
   e.pszDescShort = pszDescShort;
   e.pszName = pszName;
   pNode->plHTOCENTRY->Add (&e);

   return TRUE;
}


/*****************************************************************************************
HTOCNODEAdd - Adds an entry to the TOC.

inputs
   PHELPSCRATCH      phs - Information
   PWSTR          pszMainHelpCat - Help category. A series of strings divided by slash.
   PCMem          paMemHelp - Pointer to an array of 2 memory slots that contain more help categories
   PWSTR          pszName - Name of the help topic
   PWSTR          pszDescShort - Short descritpion of the help topic
   int            iSlot - Help slot this goes in
returns
   BOOL - TRUE if success
*/
BOOL HTOCNODEAdd (PHELPSCRATCH phs, PWSTR pszMainHelpCat, PCMem paMemHelp, PWSTR pszName, PWSTR pszDescShort, int iSlot)
{
   BOOL fRet = TRUE;

   if (pszMainHelpCat)
      fRet &= HTOCNODEAdd (pszMainHelpCat, pszName, pszDescShort, iSlot, phs->pTOC);

   DWORD i;
   if (paMemHelp) for (i = 0; i < 2; i++) {
      // note: if !pszMainHelpCat then dealing with a document entry, so add item
      // one even if it's blank
      if (!((PWSTR)paMemHelp[i].p)[0]) {
         if (i || pszMainHelpCat)
            continue;
      }

      // add
      fRet &= HTOCNODEAdd ((PWSTR)paMemHelp[i].p, pszName, pszDescShort, iSlot, phs->pTOC);
   } // i

   return fRet;
}


/*****************************************************************************************
HelpNewSlot - Create a new slot for a given object.

inputs
   PHELPSCRATCH      phs - Information
   DWORD             dwType - 0 = methdef, 1=propdef, 2=propglobal, 3=func, 4=object
   PWSTR             pszName - Name
returns
   int - New slot (or -1 if error)
*/
static int HelpNewSlot (PHELPSCRATCH phs, DWORD dwType, PWSTR pszName)
{
   if (!pszName[0])
      return -1;  // must have a name

   // which tree
   PCBTree pt;
   switch (dwType) {
   case 0:
      pt = phs->ptMethDef;
      break;
   case 1:
      pt = phs->ptPropDef;
      break;
   case 2:
      pt = phs->ptPropGlobal;
      break;
   case 3:
      pt = phs->ptFunc;
      break;
   case 4:
      pt = phs->ptObject;
      break;
   default:
      return -1; //error;
   }

   // see if it already exists
   int *pi = (int*)pt->Find (pszName);
   if (pi)
      return *pi;

   // else, add
   int i;
   i = phs->iSlot;
   phs->iSlot++;
   pt->Add (pszName, &i, sizeof(i));

   return i;
}

static int _cdecl HBICompare (const void *elem1, const void *elem2)
{
   PHBI pdw1, pdw2;
   pdw1 = (PHBI) elem1;
   pdw2 = (PHBI) elem2;

   return _wcsicmp(pdw1->pszName, pdw2->pszName);
}


/*****************************************************************************************
HelpHBIToList - Takes a list of HBI and appends to the memory a <xul><li>...</li></xul>
to list all the elemnts in the HBI.

inputs
   PCMem             pMem - Append to this
   PCListFixed       plHBI - List of HBI. This will be sorted by the call
*/
static void HelpHBIToList (PCMem pMem, PCListFixed plHBI)
{
   qsort (plHBI->Get(0), plHBI->Num(), sizeof(HBI), HBICompare);

   DWORD i;
   PHBI ph = (PHBI)plHBI->Get(0);
   for (i = 0; i < plHBI->Num(); i++, ph++) {
      int iSlot = (int)(size_t)ph->pInfo;

      if (i)
         MemCat (pMem, L", ");

      if (iSlot >= 0) {
         MemCat (pMem, L"<a href=s:");
         MemCat (pMem, iSlot);
         MemCat (pMem, L">");
      }

      MemCatSanitize (pMem, ph->pszName);

      if (iSlot >= 0)
         MemCat (pMem, L"</a>");
   } // i

   if (!plHBI->Num())
      MemCat (pMem, L"<italic>None</italic>");
}


/*****************************************************************************************
HelpShowLibIn - Shows the library it's in.

inputs
   PCMem             pMem - Append to this
   PHELPSCRATCH      phs - Information
   DWORD             dwLibOrig - ID for the original library
retutns
   none
*/
static void HelpShowLibIn (PCMem pMem, PHELPSCRATCH phs, DWORD dwLibOrig)
{
   // find the library
   PCMIFLLib pLib = phs->pProj->LibraryGet(phs->pProj->LibraryFind (dwLibOrig));
   if (!pLib)
      return;

   MemCat (pMem,
      L"<p align=right>(Library: ");
   MemCatSanitize (pMem, LibraryDisplayName(pLib->m_szFile));
   MemCat (pMem,
      L")</p>");
}


static int _cdecl PWSTRIndexCompare (const void *elem1, const void *elem2)
{
   PWSTR pdw1, pdw2;
   pdw1 = *((PWSTR*) elem1);
   pdw2 = *((PWSTR*) elem2);

   return _wcsicmp(pdw1, pdw2);
}




/*****************************************************************************************
HelpShowReferredFrom - Shows a list of objects that referred from this

inputs
   PCMem             pMem - Append to this
   PHELPSCRATCH      phs - Information
   PHELPINDEX        phi - Help index. Can be NULL
retutns
   none
*/
static void HelpShowReferredFrom (PCMem pMem, PHELPSCRATCH phs, PHELPINDEX phi)
{
   // make sure can find an entry for this
   if (!phi || (!phi->ptReferredFrom->Num() && !phi->plDOCREFERRED->Num()))
      return;

   MemCat (pMem,
      L"<xtablecenter width=100%>"
      L"<xtrheader>Other help documents that refer to this page</xtrheader>"
      L"<tr><td><xul>");

   DWORD i;
   PDOCREFERRED pdr = (PDOCREFERRED)phi->plDOCREFERRED->Get(0);
   for (i = 0; i < phi->plDOCREFERRED->Num(); i++, pdr++) {
      if (pdr->iSlot < 0)
         continue;   // shouldnt happen
      MemCat (pMem, L"<li><bold><a href=s:");
      MemCat (pMem, pdr->iSlot);
      MemCat (pMem, L">");
      MemCatSanitize (pMem, pdr->pszDocName);
      MemCat (pMem, L"</a></bold></li>");
   } // i

   // for other entries, create a list that's sorted
   CListFixed lSort;
   PWSTR psz;
   lSort.Init (sizeof(PWSTR));
   lSort.Required (phi->ptReferredFrom->Num());
   for (i = 0; i < phi->ptReferredFrom->Num(); i++) {
      psz = phi->ptReferredFrom->Enum(i);
      lSort.Add (&psz);
   } // i
   qsort (lSort.Get(0), lSort.Num(), sizeof(PWSTR), PWSTRIndexCompare);

   if (lSort.Num()) {
      MemCat (pMem, L"<li><bold></bold>");
      for (i = 0; i < lSort.Num(); i++) {
         if (i)
            MemCat (pMem, L", ");
         MemCatSanitizeAutoIndex (pMem, *((PWSTR*)lSort.Get(i)), phs->phHELPINDEX);
      }
      MemCat (pMem, L"</li>");
   }

   MemCat (pMem,
      L"</xul></td></tr>"
      L"</xtablecenter>");

}

/*****************************************************************************************
HelpBuildMethDef - Build help entry for a method definition.

inputs
   PHELPSCRATCH      phs - Information
   PCMIFLMeth        pMeth - Method to build it for
returns
   BOOL - TRUE if success
*/
static BOOL HelpBuildMethDef (PHELPSCRATCH phs, PCMIFLMeth pMeth)
{
   CMem memSearchIndex;
   MemZero (&memSearchIndex);

   PWSTR pszName = (PWSTR)pMeth->m_memName.p;
   PWSTR pszDescShort = (PWSTR)pMeth->m_memDescShort.p;
   int iSlot = HelpNewSlot (phs, 0, pszName);
   if (iSlot < 0)
      return FALSE;


   // fill in entry
   PHELPINDEX phi = (PHELPINDEX)phs->phHELPINDEX->Find (pszName);
   if (phi)
      phi->iSlot = iSlot;

   MIFLHelpHeader (&gMemTemp, pszName, L"Method def.", TRUE);

   if (((PWSTR)pMeth->m_memDescShort.p)[0]) {
      MemCat (&gMemTemp, L"<p>");
      MemCatSanitizeAutoIndex (&gMemTemp, (PWSTR)pMeth->m_memDescShort.p, phs->phHELPINDEX);
      MemCat (&gMemTemp, L"</p>");
   }

   // parameters
   MemCat (&gMemTemp,
      L"<xtablecenter width=100%>"
      L"<xtrheader>Parameters</xtrheader>");
   DWORD i, j;
   PCMIFLProp *ppp = (PCMIFLProp*)pMeth->m_lPCMIFLProp.Get(0);
   for (i = 0; i < pMeth->m_lPCMIFLProp.Num(); i++) {
      MemCat (&gMemTemp, L"<tr><td width=33%><bold>");
      MemCatSanitize (&gMemTemp, (PWSTR)ppp[i]->m_memName.p);
      MemCat (&gMemTemp, L"</bold></td><td width=66%>");
      MemCatSanitizeAutoIndex (&gMemTemp, (PWSTR)ppp[i]->m_memDescShort.p, phs->phHELPINDEX);
      MemCat (&gMemTemp, L"</td></tr>");
   } // i
   
   // if any number of paramters then indicate
   if (pMeth->m_fParamAnyNum)
      MemCat (&gMemTemp,
         L"<tr><td width=33%/><td width=66%>"
         L"<italic>This method can accept any number of paramters.</italic>"
         L"</td></tr>");

   // return value
   MemCat (&gMemTemp, 
      L"<tr><td width=33%><italic>"
      L"Return value"
      L"</italic></td><td width=66%>");
   MemCatSanitizeAutoIndex (&gMemTemp, (PWSTR)pMeth->m_mpRet.m_memDescShort.p, phs->phHELPINDEX);
   MemCat (&gMemTemp, L"</td></tr>");
   MemCat (&gMemTemp, L"</xtablecenter>");

   if (pMeth->m_dwOverride) {
      MemCat (&gMemTemp,
         L"<xtablecenter width=100%>"
         L"<xtrheader>Overrides</xtrheader>"
         L"<tr><td>");

      switch (pMeth->m_dwOverride) {
      case 1: // all methods from high to low
      case 3: // stops
         MemCat (&gMemTemp,
            L"All instances of the method in an object are called, from "
            L"highest to lowest priority class. ");
         break;
      case 2: // all methods from low to high
      case 4:  // stops
         MemCat (&gMemTemp,
            L"All instances of the method in an object are called, from "
            L"lowest to highest priority class. ");
         break;
      }

      switch (pMeth->m_dwOverride) {
      case 1:
      case 2:
         MemCat (&gMemTemp,
            L"If any instance returns a value other than \"Undefined\" the process stops.");
         break;
      case 3:
      case 4:
         MemCat (&gMemTemp,
            L"The return value is that of the last instance.");
         break;
      }

      MemCat (&gMemTemp,
         L"</td></tr>"
         L"</xtablecenter>");
   }

   // show the long description
   if (((PWSTR)pMeth->m_memDescLong.p)[0]) {
      MemCat (&gMemTemp, L"<p>");
      MemCatSanitizeAutoIndex (&gMemTemp, (PWSTR)pMeth->m_memDescLong.p, phs->phHELPINDEX);
      MemCat (&gMemTemp, L"</p>");
   }
   
   // copy the memory in gMemTemp over to memSearchIndex
   MemCat (&memSearchIndex, (PWSTR)gMemTemp.p);

   MemCat (&gMemTemp,
      L"<xtablecenter width=100%>"
      L"<xtrheader>Objects/classes that use</xtrheader>");

   if (pMeth->m_fCommonAll)
      MemCat (&gMemTemp,
         L"<tr><td>This method is automatically supported by all classes and objects.</td></tr>");
   else {
      CListFixed lHBI;
      lHBI.Init (sizeof(HBI));
      HBI hbi;

      // create list of all classes that support directly...
      lHBI.Clear();
      CBTree tExclude;
      for (i = 0; i < phs->pLib->ObjectNum(); i++) {
         PCMIFLObject pObj = phs->pLib->ObjectGet(i);

         if (-1 == pObj->MethPubFind ((PWSTR)pMeth->m_memName.p, -1))
            continue;   // method not supported

         // else, method supported
         hbi.pszName = (PWSTR)pObj->m_memName.p;
         hbi.pInfo = (PVOID) (size_t) HelpNewSlot (phs, 4, hbi.pszName);
         lHBI.Add (&hbi);

         tExclude.Add (hbi.pszName, NULL, 0);   // so that dont show for indeirect
      }

      // display this
      MemCat (&gMemTemp,
         L"<tr><td>"
         L"<bold>Use the method directly:</bold> ");
      HelpHBIToList (&gMemTemp, &lHBI);
      MemCat (&gMemTemp,
         L"</td></tr>");



      // Show all objects that use indirectly
      lHBI.Clear();
      for (i = 0; i < phs->pLib->ObjectNum(); i++) {
         PCListVariable plSuper = phs->papSuper[i];
         PCMIFLObject pObj = phs->pLib->ObjectGet(i);

         // see if supports itself
         if (-1 != pObj->MethPubFind ((PWSTR)pMeth->m_memName.p, -1))
            goto support;

         for (j = 0; j < plSuper->Num(); j++) {
            DWORD dwIndex = phs->pLib->ObjectFind ((PWSTR)plSuper->Get(j), -1);
            pObj = phs->pLib->ObjectGet(dwIndex);
            if (!pObj)
               continue;

            if (-1 != pObj->MethPubFind ((PWSTR)pMeth->m_memName.p, -1))
               break;   // found occurence
         }
         if (j >= plSuper->Num())
            continue;   // wasnt used by anything

support:
         // else, method supported
         pObj = phs->pLib->ObjectGet(i);
         hbi.pszName = (PWSTR)pObj->m_memName.p;

         // BUGFIX - if this method is already on the list then dont add
         if (tExclude.Find (hbi.pszName))
            continue;

         hbi.pInfo = (PVOID) (size_t) HelpNewSlot (phs, 4, hbi.pszName);
         lHBI.Add (&hbi);
      }

      // display this
      MemCat (&gMemTemp,
         L"<tr><td>"
         L"<bold>Use the method indirectly:</bold> ");
      HelpHBIToList (&gMemTemp, &lHBI);
      MemCat (&gMemTemp,
         L"</td></tr>");
   }

   MemCat (&gMemTemp,
      L"</xtablecenter>");

   // BUGFIX - show what referred to by
   HelpShowReferredFrom (&gMemTemp, phs, phi);
   HelpShowLibIn (&gMemTemp, phs, pMeth->m_dwLibOrigFrom);
   HelpFillSlot (phs->plHelp, phs->plHelpIndex, &gMemTemp, &memSearchIndex, iSlot);
   WCHAR szHelpTopic[] = L"Reference/Method definitions";
   return HTOCNODEAdd (phs, szHelpTopic, pMeth->m_aMemHelp,
      pszName, pszDescShort, iSlot);
}



/*****************************************************************************************
HelpBuildPropDef - Build help entry for a prop definition.

inputs
   PHELPSCRATCH      phs - Information
   PCMIFLProp        pProp - Prop to build it for
returns
   BOOL - TRUE if success
*/
static BOOL HelpBuildPropDef (PHELPSCRATCH phs, PCMIFLProp pProp)
{
   CMem memSearchIndex;
   MemZero (&memSearchIndex);

   PWSTR pszName = (PWSTR)pProp->m_memName.p;
   PWSTR pszDescShort = (PWSTR)pProp->m_memDescShort.p;
   int iSlot = HelpNewSlot (phs, 1, pszName);
   if (iSlot < 0)
      return FALSE;


   // fill in entry
   PHELPINDEX phi = (PHELPINDEX)phs->phHELPINDEX->Find (pszName);
   if (phi)
      phi->iSlot = iSlot;

   MIFLHelpHeader (&gMemTemp, pszName, L"Property def.", TRUE);

   if (((PWSTR)pProp->m_memDescShort.p)[0]) {
      MemCat (&gMemTemp, L"<p>");
      MemCatSanitizeAutoIndex (&gMemTemp, (PWSTR)pProp->m_memDescShort.p, phs->phHELPINDEX);
      MemCat (&gMemTemp, L"</p>");
   }

   // show the long description
   if (((PWSTR)pProp->m_memDescLong.p)[0]) {
      MemCat (&gMemTemp, L"<p>");
      MemCatSanitizeAutoIndex (&gMemTemp, (PWSTR)pProp->m_memDescLong.p, phs->phHELPINDEX);
      MemCat (&gMemTemp, L"</p>");
   }
   
   // copy the memory in gMemTemp over to memSearchIndex
   MemCat (&memSearchIndex, (PWSTR)gMemTemp.p);

   MemCat (&gMemTemp,
      L"<xtablecenter width=100%>"
      L"<xtrheader>Objects/classes that use</xtrheader>");

   CListFixed lHBI;
   lHBI.Init (sizeof(HBI));
   HBI hbi;
   CBTree tClassUseProp;

   // create list of all classes that support directly...
   lHBI.Clear();
   DWORD i, j, k;
   for (i = 0; i < phs->pLib->ObjectNum(); i++) {
      PCMIFLObject pObj = phs->pLib->ObjectGet(i);

      PCMIFLProp *ppp = (PCMIFLProp*)pObj->m_lPCMIFLPropPub.Get(0);
      for (k = 0; k < pObj->m_lPCMIFLPropPub.Num(); k++)
         if (!_wcsicmp((PWSTR)ppp[k]->m_memName.p, (PWSTR)pProp->m_memName.p))
            break;
      if (k >= pObj->m_lPCMIFLPropPub.Num())
         continue;   // Propod not supported

      // else, Propod supported
      hbi.pszName = (PWSTR)pObj->m_memName.p;
      hbi.pInfo = (PVOID) (size_t) HelpNewSlot (phs, 4, hbi.pszName);
      lHBI.Add (&hbi);

      // remeber this in the list of classes that use the property directly
      tClassUseProp.Add (hbi.pszName, &i, sizeof(i));
   }

   // display this
   MemCat (&gMemTemp,
      L"<tr><td>"
      L"<bold>Use the property directly:</bold> ");
   HelpHBIToList (&gMemTemp, &lHBI);
   MemCat (&gMemTemp,
      L"</td></tr>");



   // Show all objects that use indirectly
   lHBI.Clear();
   for (i = 0; i < phs->pLib->ObjectNum(); i++) {
      PCListVariable plSuper = phs->papSuper[i];
      PCMIFLObject pObj = phs->pLib->ObjectGet(i);

      // see if supports itself
      if (tClassUseProp.Find ((PWSTR)pObj->m_memName.p))
         goto support;

      for (j = 0; j < plSuper->Num(); j++) {
         DWORD dwIndex = phs->pLib->ObjectFind ((PWSTR)plSuper->Get(j), -1);
         pObj = phs->pLib->ObjectGet(dwIndex);
         if (!pObj)
            continue;

         if (tClassUseProp.Find ((PWSTR)pObj->m_memName.p))
            break; // found occurence
      }
      if (j >= plSuper->Num())
         continue;   // wasnt used by anything

support:
      // else, Propod supported
      pObj = phs->pLib->ObjectGet(i);
      hbi.pszName = (PWSTR)pObj->m_memName.p;

      // BUGFIX - Don't include used-directly items in indirect lis
      if (tClassUseProp.Find (hbi.pszName))
         continue;

      hbi.pInfo = (PVOID) (size_t) HelpNewSlot (phs, 4, hbi.pszName);
      lHBI.Add (&hbi);
   }

   // display this
   MemCat (&gMemTemp,
      L"<tr><td>"
      L"<bold>Use the property indirectly:</bold> ");
   HelpHBIToList (&gMemTemp, &lHBI);
   MemCat (&gMemTemp,
      L"</td></tr>");

   MemCat (&gMemTemp,
      L"</xtablecenter>");

   // BUGFIX - show what references this

   HelpShowReferredFrom (&gMemTemp, phs, phi);
   HelpShowLibIn (&gMemTemp, phs, pProp->m_dwLibOrigFrom);
   HelpFillSlot (phs->plHelp, phs->plHelpIndex, &gMemTemp, &memSearchIndex, iSlot);
   WCHAR szHelpTopic[] = L"Reference/Property definitions";
   return HTOCNODEAdd (phs, szHelpTopic, pProp->m_aMemHelp,
      pszName, pszDescShort, iSlot);
}




/*****************************************************************************************
HelpBuildGlobal - Build help entry for a prop definition.

inputs
   PHELPSCRATCH      phs - Information
   PCMIFLProp        pProp - Prop to build it for
returns
   BOOL - TRUE if success
*/
static BOOL HelpBuildGlobal (PHELPSCRATCH phs, PCMIFLProp pProp)
{
   CMem memSearchIndex;
   MemZero (&memSearchIndex);

   PWSTR pszName = (PWSTR)pProp->m_memName.p;
   PWSTR pszDescShort = (PWSTR)pProp->m_memDescShort.p;
   int iSlot = HelpNewSlot (phs, 2, pszName);
   if (iSlot < 0)
      return FALSE;


   // fill in entry
   PHELPINDEX phi = (PHELPINDEX)phs->phHELPINDEX->Find (pszName);
   if (phi)
      phi->iSlot = iSlot;

   MIFLHelpHeader (&gMemTemp, pszName, L"Global var.", TRUE);

   if (((PWSTR)pProp->m_memDescShort.p)[0]) {
      MemCat (&gMemTemp, L"<p>");
      MemCatSanitizeAutoIndex (&gMemTemp, (PWSTR)pProp->m_memDescShort.p, phs->phHELPINDEX);
      MemCat (&gMemTemp, L"</p>");
   }

   if (((PWSTR)pProp->m_memInit.p)[0]) {
      MemCat (&gMemTemp,
         L"<xtablecenter width=100%>"
         L"<xtrheader>Initialized to</xtrheader>"
         L"<tr><td><font face=courier>");
      MemCatSanitizeAutoIndex (&gMemTemp, (PWSTR)pProp->m_memInit.p, phs->phHELPINDEX);
      MemCat (&gMemTemp, 
         L"</font></td></tr>"
         L"</xtablecenter>");
   }

   // show the long description
   if (((PWSTR)pProp->m_memDescLong.p)[0]) {
      MemCat (&gMemTemp, L"<p>");
      MemCatSanitizeAutoIndex (&gMemTemp, (PWSTR)pProp->m_memDescLong.p, phs->phHELPINDEX);
      MemCat (&gMemTemp, L"</p>");
   }
   

   // copy the memory in gMemTemp over to memSearchIndex
   MemCat (&memSearchIndex, (PWSTR)gMemTemp.p);

   // BUGFIX - will need to show what references this
   HelpShowReferredFrom (&gMemTemp, phs, phi);
   HelpShowLibIn (&gMemTemp, phs, pProp->m_dwLibOrigFrom);
   HelpFillSlot (phs->plHelp, phs->plHelpIndex, &gMemTemp, &memSearchIndex, iSlot);
   WCHAR szHelpTopic[] = L"Reference/Global variables";
   return HTOCNODEAdd (phs, szHelpTopic, pProp->m_aMemHelp,
      pszName, pszDescShort, iSlot);
}




/*****************************************************************************************
HelpBuildFunc - Build help entry for a Func definition.

inputs
   PHELPSCRATCH      phs - Information
   PCMIFLFunc        pFunc - Func to build it for
returns
   BOOL - TRUE if success
*/
static BOOL HelpBuildFunc (PHELPSCRATCH phs, PCMIFLFunc pFunc)
{
   CMem memSearchIndex;
   MemZero (&memSearchIndex);

   PWSTR pszName = (PWSTR)pFunc->m_Meth.m_memName.p;
   PWSTR pszDescShort = (PWSTR)pFunc->m_Meth.m_memDescShort.p;
   int iSlot = HelpNewSlot (phs, 3, pszName);
   if (iSlot < 0)
      return FALSE;

   // fill in entry
   PHELPINDEX phi = (PHELPINDEX)phs->phHELPINDEX->Find (pszName);
   if (phi)
      phi->iSlot = iSlot;


   MIFLHelpHeader (&gMemTemp, pszName, L"Function", TRUE);

   PCMIFLMeth pMeth = &pFunc->m_Meth;


   if (((PWSTR)pMeth->m_memDescShort.p)[0]) {
      MemCat (&gMemTemp, L"<p>");
      MemCatSanitizeAutoIndex (&gMemTemp, (PWSTR)pMeth->m_memDescShort.p, phs->phHELPINDEX);
      MemCat (&gMemTemp, L"</p>");
   }

   // parameters
   MemCat (&gMemTemp,
      L"<xtablecenter width=100%>"
      L"<xtrheader>Parameters</xtrheader>");
   DWORD i;
   PCMIFLProp *ppp = (PCMIFLProp*)pMeth->m_lPCMIFLProp.Get(0);
   for (i = 0; i < pMeth->m_lPCMIFLProp.Num(); i++) {
      MemCat (&gMemTemp, L"<tr><td width=33%><bold>");
      MemCatSanitize (&gMemTemp, (PWSTR)ppp[i]->m_memName.p);
      MemCat (&gMemTemp, L"</bold></td><td width=66%>");
      MemCatSanitizeAutoIndex (&gMemTemp, (PWSTR)ppp[i]->m_memDescShort.p, phs->phHELPINDEX);
      MemCat (&gMemTemp, L"</td></tr>");
   } // i
   
   // if any number of paramters then indicate
   if (pMeth->m_fParamAnyNum)
      MemCat (&gMemTemp,
         L"<tr><td width=33%/><td width=66%>"
         L"<italic>This function can accept any number of paramters.</italic>"
         L"</td></tr>");

   // return value
   MemCat (&gMemTemp, 
      L"<tr><td width=33%><italic>"
      L"Return value"
      L"</italic></td><td width=66%>");
   MemCatSanitizeAutoIndex (&gMemTemp, (PWSTR)pMeth->m_mpRet.m_memDescShort.p, phs->phHELPINDEX);
   MemCat (&gMemTemp, L"</td></tr>");
   MemCat (&gMemTemp, L"</xtablecenter>");


   // show the long description
   if (((PWSTR)pMeth->m_memDescLong.p)[0]) {
      MemCat (&gMemTemp, L"<p>");
      MemCatSanitizeAutoIndex (&gMemTemp, (PWSTR)pMeth->m_memDescLong.p, phs->phHELPINDEX);
      MemCat (&gMemTemp, L"</p>");
   }
   

   // copy the memory in gMemTemp over to memSearchIndex
   MemCat (&memSearchIndex, (PWSTR)gMemTemp.p);

   // BUGFIX - will need sto show what references this
   HelpShowReferredFrom (&gMemTemp, phs, phi);
   HelpShowLibIn (&gMemTemp, phs, pFunc->m_Meth.m_dwLibOrigFrom);
   HelpFillSlot (phs->plHelp, phs->plHelpIndex, &gMemTemp, &memSearchIndex, iSlot);
   WCHAR szHelpTopic[] = L"Reference/Functions";
   return HTOCNODEAdd (phs, szHelpTopic, pFunc->m_Meth.m_aMemHelp,
      pszName, pszDescShort, iSlot);
}





/*****************************************************************************************
HelpBuildObject - Build help entry for a Object definition.

inputs
   PHELPSCRATCH      phs - Information
   PCMIFLObject        pObject - Object to build it for
   int               iAllObject - Help index for methods supported by all objects
returns
   BOOL - TRUE if success
*/
static BOOL HelpBuildObject (PHELPSCRATCH phs, PCMIFLObject pObject, int iAllObject)
{
   CMem memSearchIndex;
   MemZero (&memSearchIndex);

   DWORD i, j;
   PWSTR pszName = (PWSTR)pObject->m_memName.p;
   PWSTR pszDescShort = (PWSTR)pObject->m_memDescShort.p;
   int iSlot = HelpNewSlot (phs, 4, pszName);
   if (iSlot < 0)
      return FALSE;

   // fill in entry
   PHELPINDEX phi = (PHELPINDEX)phs->phHELPINDEX->Find (pszName);
   if (phi)
      phi->iSlot = iSlot;


   MIFLHelpHeader (&gMemTemp, pszName, L"Object/class", TRUE);

   if (((PWSTR)pObject->m_memDescShort.p)[0]) {
      MemCat (&gMemTemp, L"<p>");
      MemCatSanitizeAutoIndex (&gMemTemp, (PWSTR)pObject->m_memDescShort.p, phs->phHELPINDEX);
      MemCat (&gMemTemp, L"</p>");
   }


   if (pObject->m_fAutoCreate) {
      // show what contained in, and what contains
      MemCat (&gMemTemp,
         L"<xtablecenter width=100%>"
         L"<xtrheader>Contained in and contains</xtrheader>");

      MemCat (&gMemTemp,
         L"<tr><td><bold>Contained in:</bold> ");
      PWSTR psz;
      DWORD dwEntries = 0;
      PCMIFLObject pObjectCur = pObject;
      psz = (PWSTR)pObject->m_memContained.p;
      while (pObjectCur && psz && psz[0]) {
         if (dwEntries)
            MemCat (&gMemTemp, L", which is contained in ");
         MemCatSanitizeAutoIndex (&gMemTemp, psz, phs->phHELPINDEX);
         dwEntries++;

         // go down
         DWORD dwIndex = phs->pLib->ObjectFind (psz, (DWORD)-1);
         pObjectCur = (dwIndex != (DWORD)-1) ? phs->pLib->ObjectGet (dwIndex) : NULL;
         if (pObjectCur)
            psz = (PWSTR)pObjectCur->m_memContained.p;
         else
            psz = NULL;
      }  // while adding object
      if (!dwEntries)
         MemCat (&gMemTemp, L"Not contained");
      MemCat (&gMemTemp,
         L"</td></tr>");

      MemCat (&gMemTemp,
         L"<tr><td><bold>Contains:</bold> ");
      if (phi && phi->ptContains && phi->ptContains->Num()) {
         for (i = 0; i < phi->ptContains->Num(); i++) {
            if (i)
               MemCat (&gMemTemp, L", ");
            MemCatSanitizeAutoIndex (&gMemTemp, phi->ptContains->Enum(i), phs->phHELPINDEX);
         } // i
      }
      else
         MemCat (&gMemTemp, L"Nothing");
      MemCat (&gMemTemp,
         L"</td></tr>");

      MemCat (&gMemTemp,
         L"</xtablecenter>");
   }


   // copy the memory in gMemTemp over to memSearchIndex
   MemCat (&memSearchIndex, (PWSTR)gMemTemp.p);

   // fill in the room's map
   RoomGraphHelpString (phs->pLib, (PWSTR) pObject->m_memName.p, &gMemTemp);

   CListFixed lHBI;
   lHBI.Init (sizeof(HBI));
   HBI hbi;
   CBTree tClassUseProp;

   // show the superclasses
   MemCat (&gMemTemp,
      L"<xtablecenter width=100%>"
      L"<xtrheader>Super-classes and sub-classes</xtrheader>");

   // create list of all classes that support directly...
   lHBI.Clear();
   tClassUseProp.Clear();
   for (i = 0; i < pObject->m_lClassSuper.Num(); i++) {
      PWSTR psz = (PWSTR)pObject->m_lClassSuper.Get(i);

      // else, Propod supported
      hbi.pszName = psz;
      hbi.pInfo = (PVOID) (size_t) HelpNewSlot (phs, 4, hbi.pszName);
      lHBI.Add (&hbi);

      // remeber as a direct
      tClassUseProp.Add (psz, NULL, 0);
   }

   // display this
   MemCat (&gMemTemp,
      L"<tr><td>"
      L"<bold>Direct super-classes:</bold> ");
   HelpHBIToList (&gMemTemp, &lHBI);
   MemCat (&gMemTemp,
      L"</td></tr>");



   // Show all objects that use indirectly
   lHBI.Clear();
   DWORD dwIndex = phs->pLib->ObjectFind ((PWSTR)pObject->m_memName.p, -1);
   if (dwIndex != -1) {
      PCListVariable plSuper = phs->papSuper[dwIndex];

      for (j = 0; j < plSuper->Num(); j++) {
         hbi.pszName = (PWSTR)plSuper->Get(j);

         // if already marked as direct then skip
         if (tClassUseProp.Find (hbi.pszName))
            continue;

         hbi.pInfo = (PVOID) (size_t) HelpNewSlot (phs, 4, hbi.pszName);
         lHBI.Add (&hbi);
      }
   }

   // display this
   MemCat (&gMemTemp,
      L"<tr><td>"
      L"<bold>Indirect super-classes:</bold> ");
   HelpHBIToList (&gMemTemp, &lHBI);
   MemCat (&gMemTemp,
      L"</td></tr>");

   // create list of all classes that support directly...
   lHBI.Clear();
   tClassUseProp.Clear();
   for (i = 0; i < phs->pLib->ObjectNum(); i++) {
      PCMIFLObject pObj = phs->pLib->ObjectGet(i);

      for (j = 0; j < pObj->m_lClassSuper.Num(); j++) {
         PWSTR psz = (PWSTR)pObj->m_lClassSuper.Get(j);

         if (_wcsicmp(psz, (PWSTR)pObject->m_memName.p))
            continue;   // no match

         // else, supported
         hbi.pszName = (PWSTR)pObj->m_memName.p;
         hbi.pInfo = (PVOID) (size_t) HelpNewSlot (phs, 4, hbi.pszName);
         lHBI.Add (&hbi);

         // remeber this in the list of classes that use the property directly
         tClassUseProp.Add (hbi.pszName, &i, sizeof(i));
      }
   } // i, all objects

   // display this
   MemCat (&gMemTemp,
      L"<tr><td>"
      L"<bold>Direct sub-classes:</bold> ");
   HelpHBIToList (&gMemTemp, &lHBI);
   MemCat (&gMemTemp,
      L"</td></tr>");



   // Show all objects that use indirectly
   lHBI.Clear();
   for (i = 0; i < phs->pLib->ObjectNum(); i++) {
      PCListVariable plSuper = phs->papSuper[i];
      PCMIFLObject pObj = phs->pLib->ObjectGet(i);

      // if match up directly then done
      if (tClassUseProp.Find ((PWSTR)pObj->m_memName.p))
         goto supported;

      for (j = 0; j < plSuper->Num(); j++) {
         PWSTR psz = (PWSTR)plSuper->Get(j);

         if (tClassUseProp.Find (psz))
            break;   // found
      }
      if (j >= plSuper->Num())
         continue;   // wasnt used by anything

supported:
      // else, Propod supported
      pObj = phs->pLib->ObjectGet(i);

      // BUGFIX - If is a direct sub-class then dont also
      // list as indirect
      if (tClassUseProp.Find ((PWSTR)pObj->m_memName.p))
         continue;

      hbi.pszName = (PWSTR)pObj->m_memName.p;
      hbi.pInfo = (PVOID) (size_t) HelpNewSlot (phs, 4, hbi.pszName);
      lHBI.Add (&hbi);
   }

   // display this
   MemCat (&gMemTemp,
      L"<tr><td>"
      L"<bold>Indirect sub-classes:</bold> ");
   HelpHBIToList (&gMemTemp, &lHBI);
   MemCat (&gMemTemp,
      L"</td></tr>");

   MemCat (&gMemTemp,
      L"</xtablecenter>");





   // show the long description
   if (((PWSTR)pObject->m_memDescLong.p)[0]) {
      MemCat (&gMemTemp, L"<p>");
      MemCatSanitizeAutoIndex (&gMemTemp, (PWSTR)pObject->m_memDescLong.p, phs->phHELPINDEX);
      MemCat (&gMemTemp, L"</p>");

      // also, for the indexed version
      MemCat (&memSearchIndex, L"<p>");
      // NOTE: Dont bother with autolinks for search index
      MemCatSanitize (&memSearchIndex, (PWSTR)pObject->m_memDescLong.p);
      MemCat (&memSearchIndex, L"</p>");
   }
   



   // show a list of public properites
   MemCat (&gMemTemp,
      L"<xtablecenter width=100%>"
      L"<xtrheader>Public properties</xtrheader>");

   // create list of all classes that support directly...
   lHBI.Clear();
   tClassUseProp.Clear();
   PCMIFLProp *ppp = (PCMIFLProp*) pObject->m_lPCMIFLPropPub.Get(0);
   CBTree tMainProp;
   for (i = 0; i < pObject->m_lPCMIFLPropPub.Num(); i++) {
      hbi.pszName = (PWSTR)ppp[i]->m_memName.p;
      hbi.pszValue = (PWSTR)ppp[i]->m_memInit.p;
      if (!hbi.pszName[0])
         continue;   // no null strings
      hbi.pInfo = (PVOID) (size_t) HelpNewSlot (phs, 1, hbi.pszName);
      lHBI.Add (&hbi);
      tMainProp.Add (hbi.pszName, 0, NULL);
   } // i, all objects

   qsort (lHBI.Get(0), lHBI.Num(), sizeof(HBI), HBICompare);

   // display ones with actual property
   PHBI phbi = (PHBI) lHBI.Get(0);
   CMem memExtra;
   MemZero (&memExtra);
   for (i = 0; i < lHBI.Num(); i++, phbi++) {
      if (!phbi->pszValue || !phbi->pszValue[0]) {
         // properties that don't have a predefined value
         if (((PWSTR)memExtra.p)[0])
            MemCat (&memExtra, L", ");

         MemCatSanitizeAutoIndex (&memExtra, phbi->pszName, phs->phHELPINDEX);
         continue;
      }

      // else, use a table entry
      MemCat (&gMemTemp,
         L"<tr><td>");
      MemCatSanitizeAutoIndex (&gMemTemp, phbi->pszName, phs->phHELPINDEX);
      MemCat (&gMemTemp,
         L"</td><td><font face=courier>");
      DWORD dwLen = (DWORD)wcslen(phbi->pszValue);
#define VALUECUTOFF        100
      WCHAR cTemp;
      if (dwLen > VALUECUTOFF) {
         cTemp = phbi->pszValue[VALUECUTOFF];
         phbi->pszValue[VALUECUTOFF] = 0;
      }
      MemCatSanitizeAutoIndex (&gMemTemp, phbi->pszValue, phs->phHELPINDEX);
      if (dwLen > VALUECUTOFF) {
         MemCat (&gMemTemp, L"...");
         phbi->pszValue[VALUECUTOFF] = cTemp;
      }
      MemCat (&gMemTemp,
         L"</font></td></tr>");
   } // i

   // show all the properties without any value
   if (((PWSTR)memExtra.p)[0]) {
      MemCat (&gMemTemp, L"<tr><td><bold>Properties supported by the object but without any values:</bold> ");
      MemCat (&gMemTemp, (PWSTR)memExtra.p);
      MemCat (&gMemTemp, L"</td></tr>");
   }



   // Show all properties that use indirectly
   lHBI.Clear();
   dwIndex = phs->pLib->ObjectFind ((PWSTR)pObject->m_memName.p, -1);
   if (dwIndex != -1) {
      PCListVariable plSuper = phs->papSuper[dwIndex];

      for (j = 0; j <= plSuper->Num(); j++) {
         PWSTR psz = (j >= plSuper->Num()) ? (PWSTR)pObject->m_memName.p : (PWSTR)plSuper->Get(j);
         PCMIFLObject pObj = phs->pLib->ObjectGet(phs->pLib->ObjectFind (psz,-1));
         if (!pObj)
            continue;

         // loop through all the methods
         ppp = (PCMIFLProp*) pObj->m_lPCMIFLPropPub.Get(0);
         for (i = 0; i < pObj->m_lPCMIFLPropPub.Num(); i++) {
            hbi.pszName = (PWSTR)ppp[i]->m_memName.p;

            if (!hbi.pszName[0])
               continue;   // no null strings
            if (tClassUseProp.Find (hbi.pszName))
               continue;   // already on the list

            // if it's already on the main object's list of properties then dont show
            if (tMainProp.Find (hbi.pszName))
               continue;

            hbi.pInfo = (PVOID) (size_t) HelpNewSlot (phs, 1, hbi.pszName);
            lHBI.Add (&hbi);

            // make sure to add to list, so dont duplicate
            tClassUseProp.Add (hbi.pszName, &i, sizeof(i));
         } // i, all objects
      } // j
   } // if dwIndex

   // sort
   qsort (lHBI.Get(0), lHBI.Num(), sizeof(HBI), HBICompare);
   if (lHBI.Num()) {
      MemCat (&gMemTemp,
         L"<tr><td><bold>Properties supported by the object's superclasses:</bold> ");

      phbi = (PHBI) lHBI.Get(0);
      for (i = 0; i < lHBI.Num(); i++, phbi++) {
         if (i)
            MemCat (&gMemTemp, L", ");
         MemCatSanitizeAutoIndex (&gMemTemp, phbi->pszName, phs->phHELPINDEX);
      } // i
      MemCat (&gMemTemp,
         L"</td></tr>");
   } // i
   
   MemCat (&gMemTemp,
      L"</xtablecenter>");




   



   // show a list of public methods
   MemCat (&gMemTemp,
      L"<xtablecenter width=100%>"
      L"<xtrheader>Public methods</xtrheader>");

   // create list of all classes that support directly...
   lHBI.Clear();
   tClassUseProp.Clear();
   PCMIFLFunc pFunc;
   for (i = 0; i < pObject->MethPubNum(); i++) {
      pFunc = pObject->MethPubGet(i);
      hbi.pszName = (PWSTR)pFunc->m_Meth.m_memName.p;
      if (!hbi.pszName[0])
         continue;   // no null strings
      hbi.pInfo = (PVOID) (size_t) HelpNewSlot (phs, 0, hbi.pszName);
      lHBI.Add (&hbi);

      tClassUseProp.Add (hbi.pszName, NULL, 0);   // BUGFIX - so dont include on indirect
   } // i, all objects

   // display this
   MemCat (&gMemTemp,
      L"<tr><td>"
      L"<bold>Directly supported methods:</bold> ");
   HelpHBIToList (&gMemTemp, &lHBI);
   MemCat (&gMemTemp,
      L"</td></tr>");



   // Show all objects that use indirectly
   lHBI.Clear();
   dwIndex = phs->pLib->ObjectFind ((PWSTR)pObject->m_memName.p, -1);
   if (dwIndex != -1) {
      PCListVariable plSuper = phs->papSuper[dwIndex];

      for (j = 0; j <= plSuper->Num(); j++) {
         PWSTR psz = (j >= plSuper->Num()) ? (PWSTR)pObject->m_memName.p : (PWSTR)plSuper->Get(j);
         PCMIFLObject pObj = phs->pLib->ObjectGet(phs->pLib->ObjectFind (psz,-1));
         if (!pObj)
            continue;

         // loop through all the methods
         for (i = 0; i < pObj->MethPubNum(); i++) {
            pFunc = pObj->MethPubGet(i);
            hbi.pszName = (PWSTR)pFunc->m_Meth.m_memName.p;

            if (!hbi.pszName[0])
               continue;   // no null strings
            if (tClassUseProp.Find (hbi.pszName))
               continue;   // already on the list

            hbi.pInfo = (PVOID) (size_t) HelpNewSlot (phs, 0, hbi.pszName);
            lHBI.Add (&hbi);

            // make sure to add to list, so dont duplicate
            tClassUseProp.Add (hbi.pszName, &i, sizeof(i));
         } // i, all objects
      } // j
   } // if dwIndex

   // display this
   MemCat (&gMemTemp,
      L"<tr><td>"
      L"<bold>Indirectly supported methods:</bold> ");
   HelpHBIToList (&gMemTemp, &lHBI);


   // make link for methods supported by all classes and objects
   MemCat (&gMemTemp, L", <a href=s:");
   MemCat (&gMemTemp, iAllObject);
   MemCat (&gMemTemp, L"><italic>");
   MemCat (&gMemTemp,
      L"(Methods supported by all objects/classes)"
      );
   MemCat (&gMemTemp, L"</italic></a>");

   MemCat (&gMemTemp,
      L"</td></tr>");

   MemCat (&gMemTemp,
      L"</xtablecenter>");




   // BUGFIX - show all help that references this
   HelpShowReferredFrom (&gMemTemp, phs, phi);
   HelpShowLibIn (&gMemTemp, phs, pObject->m_dwLibOrigFrom);
   HelpFillSlot (phs->plHelp, phs->plHelpIndex, &gMemTemp, &memSearchIndex, iSlot);
   WCHAR szHelpTopic[] = L"Reference/Objects and classes";
   return HTOCNODEAdd (phs, szHelpTopic, pObject->m_aMemHelp,
      pszName, pszDescShort, iSlot);
}





/*****************************************************************************************
HelpBuildMethSupportByAll - Build help entry method supported by all objects.

inputs
   PHELPSCRATCH      phs - Information
returns
   int - Slot number.
*/
static BOOL HelpBuildMethSupportByAll (PHELPSCRATCH phs)
{
   PWSTR pszName = L"Methods supported by all objects/classes";
   PWSTR pszDescShort = L"A list of methods that are automatically supported by all "
      L"objects and classes.";
   int iSlot = phs->iSlot;
   phs->iSlot++;


   MIFLHelpHeader (&gMemTemp, pszName, NULL, FALSE);

   MemCat (&gMemTemp,
      L"<p>"
      L"This is a list of methods that are automatically supported by "
      L"all objects and classes."
      L"</p>");

   CListFixed lHBI;
   lHBI.Init (sizeof(HBI));
   HBI hbi;
   DWORD i;

   // show a list of public methods
   MemCat (&gMemTemp,
      L"<xtablecenter width=100%>"
      L"<xtrheader>Methods supported by all objects/classes</xtrheader>"
      L"<tr><td>");

   // show methods supported by all objects
   lHBI.Clear();
   for (i = 0; i < phs->pLib->MethDefNum(); i++) {
      PCMIFLMeth pMeth = phs->pLib->MethDefGet(i);
      if (!pMeth->m_fCommonAll)
         continue;   // not common to all

      hbi.pszName = (PWSTR)pMeth->m_memName.p;

      // add
      hbi.pInfo = (PVOID) (size_t) HelpNewSlot (phs, 0, hbi.pszName);
      lHBI.Add (&hbi);
   } // i
   HelpHBIToList (&gMemTemp, &lHBI);

   MemCat (&gMemTemp,
      L"</td>");

   MemCat (&gMemTemp,
      L"</tr>"
      L"</xtablecenter>");





   HelpFillSlot (phs->plHelp, phs->plHelpIndex, &gMemTemp, NULL, iSlot);
   WCHAR szHelpTopic[] = L"Reference/Method definitions";
   HTOCNODEAdd (phs, szHelpTopic, NULL,
      pszName, pszDescShort, iSlot);
   return iSlot;
}




/*****************************************************************************************
HelpBuildDoc - Build help entry for a Doc definition.

inputs
   PHELPSCRATCH      phs - Information
   PCMIFLDoc        pDoc - Doc to build it for
returns
   BOOL - TRUE if success
*/
static BOOL HelpBuildDoc (PHELPSCRATCH phs, PCMIFLDoc pDoc)
{
   PWSTR pszName = (PWSTR)pDoc->m_memName.p;
   PWSTR pszDescShort = (PWSTR)pDoc->m_memDescShort.p;
   int iSlot = phs->iSlot;
   phs->iSlot++;

   DOCREFERRED dr;
   dr.iSlot = iSlot;
   dr.pszDocName = pszName;

   MIFLHelpHeader (&gMemTemp, pszName, NULL, FALSE);

   if (((PWSTR)pDoc->m_memDescShort.p)[0]) {
      MemCat (&gMemTemp, L"<p>");
      // BUGBGUG - will need to create autolinks
      MemCatSanitize (&gMemTemp, (PWSTR)pDoc->m_memDescShort.p);
      MemCat (&gMemTemp, L"</p>");

      // NOTE: Parsing string for reference here, but only for docs
      HelpParseStringForReference ((PWSTR)pDoc->m_memDescShort.p, phs->phHELPINDEX, NULL, &dr);
   }

   MemCat (&gMemTemp, L"<xbr/>");

   if (((PWSTR)pDoc->m_memDescLong.p)[0]) {
      if (pDoc->m_fUseMML) {
         MemCatAutoIndex (&gMemTemp, (PWSTR)pDoc->m_memDescLong.p, phs->phHELPINDEX);
         MemCat (&gMemTemp, L"<p/>");
      }
      else {
         MemCat (&gMemTemp, L"<p>");
         MemCatSanitizeAutoIndex (&gMemTemp, (PWSTR)pDoc->m_memDescLong.p, phs->phHELPINDEX);
         MemCat (&gMemTemp, L"</p>");
      }

      // NOTE: Parsing string for reference here, but only for docs
      HelpParseStringForReference ((PWSTR)pDoc->m_memDescLong.p, phs->phHELPINDEX, NULL, &dr);
   }

   HelpShowLibIn (&gMemTemp, phs, pDoc->m_dwLibOrigFrom);
   HelpFillSlot (phs->plHelp, phs->plHelpIndex, &gMemTemp, &gMemTemp, iSlot);
   return HTOCNODEAdd (phs, NULL, pDoc->m_aMemHelp,
      pszName, pszDescShort, iSlot);
}




/*****************************************************************************************
HelpBuildTOC - Builds a TOC page

inputs
   PHELPSCRATCH      phs - Information
   PHTOCNODE         pNode - Node to build
   PWSTR             pszName - Name of this section
returns
   int iSlot - Slot of the created node, or -1 if error
*/

static int HelpBuildTOC (PHELPSCRATCH phs, PHTOCNODE pNode, PWSTR pszName)
{
   // create a blank page for this
   int iSlot = phs->iSlot;
   phs->iSlot++;
   CMem mem;
   MIFLHelpHeader (&mem, pszName ? pszName : L"Table of contents", pszName ? L"TOC" : NULL, FALSE);

   // sort the list of topics
   CListFixed l;
   DWORD i;
   HBI hbi;
   PHBI phbi;
   l.Init (sizeof (HBI));
   if (pNode->plHTOCENTRY) {
      l.Clear();

      PHTOCENTRY ph = (PHTOCENTRY)pNode->plHTOCENTRY->Get(0);
      for (i = 0; i < pNode->plHTOCENTRY->Num(); i++) {
         hbi.pszName = ph[i].pszName;
         hbi.pInfo = &ph[i];
         l.Add (&hbi);
      } // i

      qsort (l.Get(0), l.Num(), sizeof(HBI), HBICompare);

      MemCat (&mem, L"<xtablecenter width=100%>"
         L"<xtrheader>Topics</xtrheader>"
         L"<tr><td><xul>");

      // show list of topics
      phbi = (PHBI)l.Get(0);
      for (i = 0; i < l.Num(); i++, phbi++) {
         ph = (PHTOCENTRY)phbi->pInfo;

         MemCat (&mem, L"<li><bold><a href=s:");
         MemCat (&mem, ph->iSlot);
         MemCat (&mem, L">");
         MemCatSanitize (&mem, ph->pszName);
         MemCat (&mem, L"</a></bold>");
         if (ph->pszDescShort[0]) {
            MemCat (&mem, L" - ");
            MemCatSanitize (&mem, ph->pszDescShort);
         }
         MemCat (&mem, L"</li>");
      } // i

      MemCat (&mem, 
         L"</xul></td></tr>"
         L"</xtablecenter>");
   }

   // list of subtopics
   if (pNode->ptHTOCNODE) {
      l.Clear();

      for (i = 0; i < pNode->ptHTOCNODE->Num(); i++) {
         PWSTR psz = pNode->ptHTOCNODE->Enum (i);
         hbi.pszName = psz;
         hbi.pInfo = pNode->ptHTOCNODE->Find(psz);
         l.Add (&hbi);
      } // i

      qsort (l.Get(0), l.Num(), sizeof(HBI), HBICompare);

      MemCat (&mem, L"<xtablecenter width=100%>"
         L"<xtrheader>Sub-topics</xtrheader>"
         L"<tr><td><bold><xul>");

      // show list of topics
      phbi = (PHBI)l.Get(0);
      for (i = 0; i < l.Num(); i++, phbi++) {
         PHTOCNODE pn = (PHTOCNODE)phbi->pInfo;
         
         // create a sub-entry
         int iSub = HelpBuildTOC (phs, pn, phbi->pszName);
         if (iSub < 0)
            continue;

         MemCat (&mem, L"<li><a href=s:");
         MemCat (&mem, iSub);
         MemCat (&mem, L">");
         MemCatSanitize (&mem, phbi->pszName);
         MemCat (&mem, L"</a></li>");
      } // i

      MemCat (&mem, 
         L"</xul></bold></td></tr>"
         L"</xtablecenter>");
   }


   HelpFillSlot (phs->plHelp, phs->plHelpIndex, &mem, NULL, iSlot);
   return iSlot;
}


/*****************************************************************************************
CMIFLProj::HelpIndexClear - Clears the hash index
*/
void CMIFLProj::HelpIndexClear (void)
{
   DWORD i;
   for (i = 0; i < m_hHELPINDEX.Num(); i++) {
      PHELPINDEX phi = (PHELPINDEX)m_hHELPINDEX.Get(i);

      //if (phi->ptRefersTo)
      //   delete phi->ptRefersTo;
      if (phi->ptReferredFrom)
         delete phi->ptReferredFrom;
      if (phi->plDOCREFERRED)
         delete phi->plDOCREFERRED;
      if (phi->ptContains)
         delete phi->ptContains;
      if (phi->pmemStringText)
         delete phi->pmemStringText;

   } // i

   m_hHELPINDEX.Clear();
   m_lPWSTRIndex.Clear();
}


/*****************************************************************************************
HelpParseStringForToken - Parse a string for relevent tokens.

inputs
   PWSTR          pszSearch - String to search through
   PCHashString   phTokens - List of tokens
   BOOL           fMustHaveSlot - Only accept if iSlot >= 0
   DWORD          *pdwTokenIndex - If found a token, the index into phTokens for the token
   PWSTR          *ppszStringText - If not NULL, then if a string is found with a matching name
                  this returns, but with *ppszStringText filled in with the string text
returns
   PWSTR - Pointer to the token found, or NULL if couldnt find one
*/
PWSTR HelpParseStringForToken (PWSTR pszSearch, PCHashString phTokens, BOOL fMustHaveSlot, DWORD *pdwTokenIndex,
                               PWSTR *ppszStringText)
{
   if (ppszStringText)
      *ppszStringText = NULL;

   // loop until NULL
   while (pszSearch[0]) {
      // look for alphas
      while (!iswalpha (pszSearch[0]) && pszSearch[0])
         pszSearch++;
      if (!pszSearch[0])
         break;

      // look for invalid character
      PWSTR pszEnd;
      for (pszEnd = pszSearch+1; pszEnd[0] && (iswalpha(pszEnd[0]) || iswdigit(pszEnd[0]) || (pszEnd[0] == L'_')); pszEnd++);

      // temporarily zero out
      WCHAR cEnd = pszEnd[0];
      pszEnd[0] = 0;
      DWORD dwFind = phTokens->FindIndex (pszSearch);
      pszEnd[0] = cEnd;

      if (dwFind != (DWORD)-1) {
         if (fMustHaveSlot) {
            PHELPINDEX phi = (PHELPINDEX)phTokens->Get(dwFind);

            if (ppszStringText && phi->pmemStringText && (phi->dwType == HIT_STRING)) {
               // found a mactch, kind of
               *pdwTokenIndex = dwFind;
               *ppszStringText = (PWSTR)phi->pmemStringText->p;
               return pszSearch;
            }

            if (phi->dwType >= HIT_NOTDOCUMENTED)
               goto moveon;   // not valid
         } // must have slot

         // found match
         *pdwTokenIndex = dwFind;
         return pszSearch;
      }

moveon:
      // else, move on
      pszSearch = pszEnd;
   }

   return NULL;
}


/*****************************************************************************************
HelpParseStringForReference - This parses a string for all references to other entities.

inputs
   PWSTR          pszSearch - String to parse for the search
   PCHashString   phTokens - List of tokens, sizeof HELPINDEX
   PWSTR          pszThis - Token for this object, or NULL. Used to create back reference.
   PDOCREFERRED   pDoc - Document that's referring. Can be NULL.
returns
   none
*/
void HelpParseStringForReference (PWSTR pszSearch, PCHashString phTokens, PWSTR pszThis, PDOCREFERRED pDoc)
{
   // find the entry for this
   DWORD dwThis = (DWORD)-1;
   PWSTR pszTokenString;
   if (pszThis)
      dwThis = phTokens->FindIndex (pszThis);

   while (TRUE) {
      DWORD dwIndex;
      PWSTR pszMatch = HelpParseStringForToken (pszSearch, phTokens, FALSE, &dwIndex);
      if (!pszMatch)
         break;   // no more matched

      // else, get the entry
      PHELPINDEX phi = (PHELPINDEX) phTokens->Get(dwIndex);
      if (pszThis && !phi->ptReferredFrom->Find(pszThis))
         phi->ptReferredFrom->Add (pszThis, NULL, 0);

      // potential doc entry
      if (pDoc) {
         PDOCREFERRED pdr = (PDOCREFERRED)phi->plDOCREFERRED->Get(0);
         DWORD j;
         for (j = 0; j < phi->plDOCREFERRED->Num(); j++, pdr++)
            if (pdr->iSlot == pDoc->iSlot)
               break;   // already there
         if (j >= phi->plDOCREFERRED->Num())
            phi->plDOCREFERRED->Add (pDoc);
      } // if pDoc

      // and entry for this
      pszTokenString = phTokens->GetString(dwIndex);
      //if (dwThis != (DWORD)-1) {
      //   phi = (PHELPINDEX) phTokens->Get(dwThis);
      //   if (!phi->ptRefersTo->Find(pszTokenString))
      //      phi->ptRefersTo->Add (pszTokenString, 0, NULL);
      //}

      // increment search
      pszSearch = pszMatch + wcslen(pszTokenString);
   } // whole TRUE
}


/*****************************************************************************************
CMIFLProj::HelpIndexGenerate - Generates the index, m_hHELPINDEX, which should have
been cleared before calling this.

inputs
   PCMIFLLib      pLib - Unified library
returns
   none
*/
void CMIFLProj::HelpIndexGenerate (PCMIFLLib pLib)
{
   DWORD i, j, dwNum;
   PWSTR psz;
   PCMIFLMeth pMeth;
   PCMIFLProp pProp;
   PCMIFLFunc pFunc;
   PCMIFLObject pObj;
   PCMIFLString pString;
   PCMIFLResource pResource;

   HELPINDEX hi;
   memset (&hi, 0, sizeof(hi));
   hi.iSlot = -1;

   // fill method defs
   for (hi.dwType = 0; hi.dwType <= HIT_RESOURCE; hi.dwType++) {
      switch (hi.dwType) {
      case HIT_METHDEF:
         dwNum = pLib->MethDefNum();
         break;
      case HIT_PROPDEF:
         dwNum = pLib->PropDefNum();
         break;
      case HIT_PROPGLOBAL:
         dwNum = pLib->GlobalNum();
         break;
      case HIT_FUNC:
         dwNum = pLib->FuncNum();
         break;
      case HIT_OBJECT:
         dwNum = pLib->ObjectNum();
         break;
      case HIT_STRING:
         dwNum = pLib->StringNum();
         break;
      case HIT_RESOURCE:
         dwNum = pLib->ResourceNum();
         break;
      default:
         continue;   // since not valid
      } // switch

      for (i = 0; i < dwNum; i++) {
         hi.pmemStringText = NULL;

         switch (hi.dwType) {
         case HIT_METHDEF:
            pMeth = pLib->MethDefGet(i);
            psz = (PWSTR)pMeth->m_memName.p;
            break;
         case HIT_PROPDEF:
            pProp = pLib->PropDefGet(i);
            psz = (PWSTR)pProp->m_memName.p;
            break;
         case HIT_PROPGLOBAL:
            pProp = pLib->GlobalGet(i);
            psz = (PWSTR)pProp->m_memName.p;
            break;
         case HIT_FUNC:
            pFunc = pLib->FuncGet(i);
            psz = (PWSTR)pFunc->m_Meth.m_memName.p;
            break;
         case HIT_OBJECT:
            pObj = pLib->ObjectGet(i);
            psz = (PWSTR)pObj->m_memName.p;
            break;
         case HIT_STRING:
            pString = pLib->StringGet(i);
            psz = (PWSTR)pString->m_memName.p;

            // keep track of the string
            if (pString->m_lString.Num()) {
               hi.pmemStringText = new CMem;
               if (hi.pmemStringText) {
                  MemZero (hi.pmemStringText);
                  MemCat (hi.pmemStringText, (PWSTR)pString->m_lString.Get(0));
               }
            }
            break;
         case HIT_RESOURCE:
            pResource = pLib->ResourceGet(i);
            psz = (PWSTR)pResource->m_memName.p;
            break;
         default:
            continue;   // since not valid
         } // switch
         if (!psz || !psz[0])
            continue;   // too small
         if (m_hHELPINDEX.Find (psz))
            continue;   // already exists, so ignore this

         hi.plDOCREFERRED = new CListFixed;
         hi.plDOCREFERRED->Init (sizeof(DOCREFERRED));
         hi.ptReferredFrom = new CBTree;
         //hi.ptRefersTo = new CBTree;
         hi.ptContains = NULL;
         m_hHELPINDEX.Add (psz, &hi);
      } // i
   } // j

   // loop through all elements of the help index, getting the string and then sorting it
   m_lPWSTRIndex.Clear();
   for (i = 0; i < m_hHELPINDEX.Num(); i++) {
      PWSTR psz = m_hHELPINDEX.GetString (i);
      m_lPWSTRIndex.Add (&psz);
   }

   qsort (m_lPWSTRIndex.Get(0), m_lPWSTRIndex.Num(), sizeof(PWSTR), PWSTRIndexCompare);
   // loop through again, and find everything that each object refers to

   // methods
   for (i = 0; i < pLib->MethDefNum(); i++) {
      pMeth = pLib->MethDefGet(i);

      HelpParseStringForReference ((PWSTR)pMeth->m_memDescShort.p, &m_hHELPINDEX, (PWSTR)pMeth->m_memName.p);
      HelpParseStringForReference ((PWSTR)pMeth->m_memDescLong.p, &m_hHELPINDEX, (PWSTR)pMeth->m_memName.p);
      HelpParseStringForReference ((PWSTR)pMeth->m_mpRet.m_memDescShort.p, &m_hHELPINDEX, (PWSTR)pMeth->m_memName.p);
      for (j = 0; j < pMeth->m_lPCMIFLProp.Num(); j++) {
         pProp = *((PCMIFLProp*)pMeth->m_lPCMIFLProp.Get(j));
         HelpParseStringForReference ((PWSTR)pProp->m_memDescShort.p, &m_hHELPINDEX, (PWSTR)pMeth->m_memName.p);
      } // j
   } // i

   // fill propdef
   for (i = 0; i < pLib->PropDefNum(); i++) {
      pProp = pLib->PropDefGet(i);

      HelpParseStringForReference ((PWSTR)pProp->m_memDescShort.p, &m_hHELPINDEX, (PWSTR)pProp->m_memName.p);
      HelpParseStringForReference ((PWSTR)pProp->m_memDescLong.p, &m_hHELPINDEX, (PWSTR)pProp->m_memName.p);
   } // i

   // fill globals
   for (i = 0; i < pLib->GlobalNum(); i++) {
      pProp = pLib->GlobalGet(i);

      HelpParseStringForReference ((PWSTR)pProp->m_memDescShort.p, &m_hHELPINDEX, (PWSTR)pProp->m_memName.p);
      HelpParseStringForReference ((PWSTR)pProp->m_memDescLong.p, &m_hHELPINDEX, (PWSTR)pProp->m_memName.p);
      HelpParseStringForReference ((PWSTR)pProp->m_memInit.p, &m_hHELPINDEX, (PWSTR)pProp->m_memName.p);
   } // i

   // fill functions
   for (i = 0; i < pLib->FuncNum(); i++) {
      pFunc = pLib->FuncGet (i);
      pMeth = &pFunc->m_Meth;

      HelpParseStringForReference ((PWSTR)pMeth->m_memDescShort.p, &m_hHELPINDEX, (PWSTR)pMeth->m_memName.p);
      HelpParseStringForReference ((PWSTR)pMeth->m_memDescLong.p, &m_hHELPINDEX, (PWSTR)pMeth->m_memName.p);
      HelpParseStringForReference ((PWSTR)pMeth->m_mpRet.m_memDescShort.p, &m_hHELPINDEX, (PWSTR)pMeth->m_memName.p);
      for (j = 0; j < pMeth->m_lPCMIFLProp.Num(); j++) {
         pProp = *((PCMIFLProp*)pMeth->m_lPCMIFLProp.Get(j));
         HelpParseStringForReference ((PWSTR)pProp->m_memDescShort.p, &m_hHELPINDEX, (PWSTR)pMeth->m_memName.p);
      } // j
   } // i

   // fill objects
   for (i = 0; i < pLib->ObjectNum(); i++) {
      pObj = pLib->ObjectGet(i);

      HelpParseStringForReference ((PWSTR)pObj->m_memDescShort.p, &m_hHELPINDEX, (PWSTR)pObj->m_memName.p);
      HelpParseStringForReference ((PWSTR)pObj->m_memDescLong.p, &m_hHELPINDEX, (PWSTR)pObj->m_memName.p);

      for (j = 0; j < pObj->m_lPCMIFLPropPub.Num(); j++) {
         pProp = *((PCMIFLProp*)pObj->m_lPCMIFLPropPub.Get(j));
         HelpParseStringForReference ((PWSTR)pProp->m_memDescShort.p, &m_hHELPINDEX, (PWSTR)pObj->m_memName.p);
         HelpParseStringForReference ((PWSTR)pProp->m_memInit.p, &m_hHELPINDEX, (PWSTR)pObj->m_memName.p);
      } // j
      for (j = 0; j < pObj->m_lPCMIFLPropPriv.Num(); j++) {
         pProp = *((PCMIFLProp*)pObj->m_lPCMIFLPropPriv.Get(j));
         HelpParseStringForReference ((PWSTR)pProp->m_memDescShort.p, &m_hHELPINDEX, (PWSTR)pObj->m_memName.p);
         HelpParseStringForReference ((PWSTR)pProp->m_memInit.p, &m_hHELPINDEX, (PWSTR)pObj->m_memName.p);
      } // j

      // contained in other object
      PWSTR psz = (PWSTR)pObj->m_memContained.p;
      if (pObj->m_fAutoCreate && psz[0]) {
         PHELPINDEX phi = (PHELPINDEX) m_hHELPINDEX.Find (psz);
         if (phi) {
            if (!phi->ptContains)
               phi->ptContains = new CBTree;
            if (phi->ptContains && !phi->ptContains->Find(psz))
               phi->ptContains->Add ((PWSTR)pObj->m_memName.p, NULL, 0);
         }
      }
   } // i

   // NOTE: Dont bother with strings and methods since dont refer to other objects
}


/*****************************************************************************************
CMIFLProj::HelpRebuild - Rebuilds the help database from the current project information.

This modififies m_lHelp, and m_dwHelpTOC.

inputs
   HWND              hWnd - To show progress off of
returns
   BOOL - TRUE if success
*/
BOOL CMIFLProj::HelpRebuild (HWND hWnd)
{
   CProgress Progress;

   Progress.Start (hWnd, "Building help...");

   HelpInit ();
   m_lHelp.Clear();
   m_lHelpIndex.Clear();
   HelpIndexClear();

   // BUGFIX - clear history
   m_ListHistory.Clear();
   memset (&m_CurHistory, 0, sizeof(m_CurHistory));

   // merge all files into one library
   // get all the libraries
   CListFixed lpLib;
   lpLib.Init (sizeof(PCMIFLLib));
   DWORD i;
   for (i = 0; i < LibraryNum(); i++) {
      PCMIFLLib pLib = LibraryGet(i);
      lpLib.Add (&pLib);
   }
   PCMIFLLib *ppl = (PCMIFLLib*)lpLib.Get(0);

   CMIFLLib lib(m_pSocket);
   lib.Merge (ppl, LibraryNum());
   Progress.Update (0.1);

   // nodes for TOC
   HTOCNODE root;
   memset (&root, 0, sizeof(root));

   // make a list of fully-exploded super-class lists
   CListFixed lSuper;
   lSuper.Init (sizeof(PCListVariable));
   lSuper.Required (lib.ObjectNum());
   for (i = 0; i < lib.ObjectNum(); i++) {
      PCListVariable pl = new CListVariable;
      lSuper.Add (&pl);
      if (!pl)
         continue;

      // expand the list
      PCMIFLObject pObj = lib.ObjectGet(i);
      lib.ObjectAllSuper ((PWSTR)pObj->m_memName.p, pl);
   } // i

   // fill in scratch info
   HELPSCRATCH hs;
   CBTree tFunc, tMethDef, tObject, tPropDef, tPropGlobal;
   memset (&hs, 0, sizeof(hs));
   hs.iSlot = 0;
   hs.ptFunc = &tFunc;
   hs.ptMethDef = &tMethDef;
   hs.ptObject = &tObject;
   hs.ptPropDef = &tPropDef;
   hs.ptPropGlobal = &tPropGlobal;
   hs.pTOC = &root;
   hs.pLib = &lib;
   hs.plHelp = &m_lHelp;
   hs.plHelpIndex = &m_lHelpIndex;
   hs.papSuper = (PCListVariable*)lSuper.Get(0);
   hs.pProj = this;
   hs.phHELPINDEX = &m_hHELPINDEX;

   HelpIndexGenerate (&lib);

   // standard docs
   // BUGFIX - Moved standard docs first
   for (i = 0; i < lib.DocNum(); i++)
      HelpBuildDoc (&hs, lib.DocGet (i));
   Progress.Update (0.25);

   // fill method defs
   for (i = 0; i < lib.MethDefNum(); i++)
      HelpBuildMethDef (&hs, lib.MethDefGet (i));
   Progress.Update (0.4);

   // fill propdef
   for (i = 0; i < lib.PropDefNum(); i++)
      HelpBuildPropDef (&hs, lib.PropDefGet (i));
   Progress.Update (0.44);

   // fill globals
   for (i = 0; i < lib.GlobalNum(); i++)
      HelpBuildGlobal (&hs, lib.GlobalGet (i));
   Progress.Update (0.7);

   // fill functions
   for (i = 0; i < lib.FuncNum(); i++)
      HelpBuildFunc (&hs, lib.FuncGet (i));
   Progress.Update (0.85);

   // fill objects
   int iAllObject = HelpBuildMethSupportByAll (&hs);
   for (i = 0; i < lib.ObjectNum(); i++)
      HelpBuildObject (&hs, lib.ObjectGet (i), iAllObject);
   Progress.Update (0.95);

   // convert TOC into pages
   if (root.plHTOCENTRY || root.ptHTOCNODE)
      m_dwHelpTOC = (DWORD) HelpBuildTOC (&hs, &root, L"Table of contents");
   else
      m_dwHelpTOC = 0;  // since dont have one

   // free
   HTOCNODEFree (&root);
   for (i = 0; i < lSuper.Num(); i++)
      delete hs.papSuper[i];

   return TRUE;
}

/*****************************************************************************************
CMIFLProj::LangClosest - Finds the closest language to one supported in the project.

inputs
   LANGID         lid - Looking for
return
   LANGID - Closest
*/
LANGID CMIFLProj::LangClosest (LANGID lid)
{
   LANGID *pl = (LANGID*)m_lLANGID.Get(0);
   DWORD dwIndex = MIFLLangMatch (&m_lLANGID, lid, FALSE);
   if (dwIndex == -1)
      return lid; // nothing better

   return pl[dwIndex];
}



/*****************************************************************************************
CMIFLProj::VMCreate - This creates a VM from the project. The project should
have already had Compile() called.

inputs
   PCEscWindow          pDebugWindow - Window to debug, same as calling DebugWindowSet().
                        This needs to be called so that constructors will be called
   HWND                 hWndParent - Parent window used in case needs to create own debug window
   DWORD                dwDebugMode - Debug mode to use, same as calling DebugModeSet()
   PCMMLNode2            pNode - If NULL then initialezed based on the default compiled state.
                        If non-NULL, then assume that MMLTo(fAll=TRUE) had been called, and that
                        initializing from this, reloading old state.
returns
   PCMIFLVM - VM that's initialized to using the compiled project.
                        Do NOT delete the project or recompile while the VM is in use
*/
PCMIFLVM CMIFLProj::VMCreate (PCEscWindow pDebugWindow, HWND hWndParent,
                    DWORD dwDebugMode, PCMMLNode2 pNode)
{
   PCMIFLVM pVM;
   pVM = new CMIFLVM;
   if (!pVM)
      return NULL;
   if (!pVM->Init (m_pCompiled, pDebugWindow, hWndParent, dwDebugMode, pNode)) {
      delete pVM;
      return NULL;
   }

   // else finished
   return pVM;
}


/*****************************************************************************************
CMIFLProj::Compile - Tries compiling the project and reports the errors into m_pErrors

inputs
   PCProgress        pProgress - To display progress bar.
   PCMIFLMeth           pMeth - If only want to compile a specific function, then this points to the
                        method definition. If compile all then this is NULL
   PCMIFLCode           pCode - If only want to compile a specific function, then this points to the
                        code definition. If compile all then this is NULL
   PCMIFLObject      pObject - Object that this is in. NULL if not in object
   PWSTR                pszErrLink - If compiling a specific function, then this string is used for
                        links for the error. Not used if pMeth or pCode are NULL.
*/
void CMIFLProj::Compile (PCProgress pProgress,
                         PCMIFLMeth pMeth, PCMIFLCode pCode, PCMIFLObject pObject, PWSTR pszErrLink)
{
   if (m_pErrors)
      delete m_pErrors;

   // wipe out existing compile
   ClearVM ();

   m_pCompiled = new CMIFLCompiled;
   if (!m_pCompiled)
      return;

   m_pErrors = m_pCompiled->Compile (pProgress, this, pMeth, pCode, pObject, pszErrLink);
   m_fErrorsAreSearch = FALSE;
   m_iErrCur = -1;

   // if errors then delete compiled, or if not compiling all
   if (m_pErrors->m_dwNumError || (pMeth && pCode)) {
      delete m_pCompiled;
      m_pCompiled = NULL;
   }
}


/*****************************************************************************************
SearchCode - Look through an individual piece of code for a string.

inputs
   PCMIFLCode        pCode - Code
   PCMIFLErrors      pErr - Error to add to
   PWSTR             pszFind - To look for
   PWSTR             pszLink - Link to use if find
   PWSTR             pszObject - Object that the code is in
   PWSTR             pszDisplay - What to display
returns
   noen
*/
static void SearchCode (PCMIFLCode pCode, PCMIFLErrors pErr, PWSTR pszFind, PWSTR pszLink,
                        PWSTR pszObject, PWSTR pszDisplay)
{
   // if too many then exit
   if (pErr->Num() > 10000)
      return;

   DWORD dwLen = (DWORD) wcslen(pszFind);
   if (!dwLen)
      return;  // 0 length
   PWSTR pszCode = (PWSTR)pCode->m_memCode.p;

   // repeat...
   PWSTR pszCur;
   for (pszCur = pszCode; pszCur; ) {
      pszCur = (PWSTR)MyStrIStr (pszCur, pszFind);
      if (!pszCur)
         break;

      DWORD dwCharStart = (DWORD)((PBYTE)pszCur - (PBYTE)pszCode) / sizeof(WCHAR);

      pErr->Add (pszDisplay, pszLink, pszObject, FALSE, dwCharStart, dwCharStart + dwLen);

      pszCur += dwLen;  // so skip this for next search
   } // pszCur
}


/*****************************************************************************************
CMIFLProj::SearchAllCode - Searches all code in the project for the given code.
THis modifies the error list and uses that to create links to the code

inputs
   PWSTR          pszFind - String to find
returns
   none
*/
void CMIFLProj::SearchAllCode (PWSTR pszFind)
{
   // clear errors
   m_fErrorsAreSearch = TRUE;
   if (m_pErrors)
      m_pErrors->Clear();
   else {
      m_pErrors = new CMIFLErrors;
      if (!m_pErrors)
         return;
   }

   // loop through the libraries
   DWORD dwLib, i;
   WCHAR szTemp[64];
   for (dwLib = 0; dwLib < LibraryNum(); dwLib++) {
      PCMIFLLib pLib = LibraryGet(dwLib);

      // loop through all the globals
      for (i = 0; i < pLib->GlobalNum(); i++) {
         PCMIFLProp pProp = pLib->GlobalGet (i);

         if (pProp->m_pCodeGet) {
            swprintf (szTemp, L"lib:%dglobal:%dget", pLib->m_dwTempID, pProp->m_dwTempID);
            SearchCode (pProp->m_pCodeGet, m_pErrors, pszFind, szTemp,
               L"Global variable get", (PWSTR)pProp->m_memName.p);
         }
         if (pProp->m_pCodeSet) {
            swprintf (szTemp, L"lib:%dglobal:%dset", pLib->m_dwTempID, pProp->m_dwTempID);
            SearchCode (pProp->m_pCodeSet, m_pErrors, pszFind, szTemp,
               L"Global variable set", (PWSTR)pProp->m_memName.p);
         }
      } // i, all globals

      // loop through all the functions
      for (i = 0; i < pLib->FuncNum(); i++) {
         PCMIFLFunc pFunc = pLib->FuncGet (i);

         swprintf (szTemp, L"lib:%dfunc:%dedit", pLib->m_dwTempID, pFunc->m_Meth.m_dwTempID);
         SearchCode (&pFunc->m_Code, m_pErrors, pszFind, szTemp,
            L"Function", (PWSTR)pFunc->m_Meth.m_memName.p);
      } // i

      DWORD dwObj, dwPriv;
      for (dwObj = 0; dwObj < pLib->ObjectNum(); dwObj++) {
         PCMIFLObject pObj = pLib->ObjectGet (dwObj);

         for (dwPriv = 0; dwPriv < 2; dwPriv++) {
            // properties
            PCListFixed pl = dwPriv ? &pObj->m_lPCMIFLPropPriv : &pObj->m_lPCMIFLPropPub;
            PCMIFLProp *ppp = (PCMIFLProp*)pl->Get(0);
            for (i = 0; i < pl->Num(); i++) {
               PCMIFLProp pProp = ppp[i];

               if (pProp->m_pCodeGet) {
                  swprintf (szTemp, L"lib:%dobject:%dprop%s:%dget",
                     pLib->m_dwTempID, pObj->m_dwTempID, dwPriv ? L"priv" : L"pub", pProp->m_dwTempID);
                  SearchCode (pProp->m_pCodeGet, m_pErrors, pszFind, szTemp,
                     (PWSTR)pObj->m_memName.p, (PWSTR)pProp->m_memName.p);
               }
               if (pProp->m_pCodeSet) {
                  swprintf (szTemp, L"lib:%dobject:%dprop%s:%dset",
                     pLib->m_dwTempID, pObj->m_dwTempID, dwPriv ? L"priv" : L"pub", pProp->m_dwTempID);
                  SearchCode (pProp->m_pCodeSet, m_pErrors, pszFind, szTemp,
                     (PWSTR)pObj->m_memName.p, (PWSTR)pProp->m_memName.p);
               }
            } // i
            
         } // dwPriv

         // BUGFIX - Moved this code out of dwPriv loop so search wouldn't find
         // double occurances
         // methods, public
         for (i = 0; i < pObj->MethPubNum(); i++) {
            PCMIFLFunc pFunc = pObj->MethPubGet (i);

            swprintf (szTemp, L"lib:%dobject:%dmethpub:%dedit",
               (int) pLib->m_dwTempID, (int) pObj->m_dwTempID, (int)pFunc->m_Meth.m_dwTempID);
            SearchCode (&pFunc->m_Code, m_pErrors, pszFind, szTemp,
               (PWSTR)pObj->m_memName.p, (PWSTR)pFunc->m_Meth.m_memName.p);
         } // i
         
         // methods, private
         for (i = 0; i < pObj->MethPrivNum(); i++) {
            PCMIFLFunc pFunc = pObj->MethPrivGet (i);

            swprintf (szTemp, L"lib:%dobject:%dmethpriv:%dedit",
               (int) pLib->m_dwTempID, (int) pObj->m_dwTempID, (int)pFunc->m_Meth.m_dwTempID);
            SearchCode (&pFunc->m_Code, m_pErrors, pszFind, szTemp,
               (PWSTR)pObj->m_memName.p, (PWSTR)pFunc->m_Meth.m_memName.p);
         } // i
      } // dwObj

   } // dwLib
}



// BUGBUG - sometimes when search all for "debug", will see the same object method
// listed twice for the same occurance of "debug"


