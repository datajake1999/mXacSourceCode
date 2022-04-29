/*************************************************************************************
CDatabase.cpp - Code for the database object.

begun 30/3/04 by Mike Rozak.
Copyright 2004 by Mike Rozak. All rights reserved
*/

#include <windows.h>
#include <objbase.h>
#include "escarpment.h"
#include "..\..\m3d\M3D.h"
#include "..\..\m3d\mifl.h"
#include "..\mif.h"
#include "..\buildnum.h"
#include "mifserver.h"
#include "resource.h"

static PWSTR gpszContainedInProp = L"containedin"; // for caching and finding out what contained in
static PWSTR gpszContainedIn = L"ContainedIn";     // MML name


#define MAXELEMCACHE             100        // max elements cached before clear out

/*************************************************************************************
CDatabaseCat::Constructor and destructor
*/
CDatabaseCat::CDatabaseCat (void)
{
   m_szName[0] = 0;
   m_hDBITEM.Init (sizeof (DBITEM));
   m_hQuick.Init (0);
   m_pVM = NULL;
   m_fDirtyQuick = FALSE;
   m_dwCached = 0;
   m_dwCheckedOut = 0;
}

CDatabaseCat::~CDatabaseCat (void)
{
   // save everything, just in case
   SaveAll ();

   // free it up
   DWORD i, j;
   for (i = 0; i < m_hDBITEM.Num(); i++) {
      PDBITEM pdi = (PDBITEM) m_hDBITEM.Get (i);
      if (pdi->pNode)
         delete pdi->pNode;   // shouldn't happen

#ifdef _DEBUG
      if (pdi->dwCheckedOut)
         OutputDebugString ("\r\nDBItem checked out!");
#endif // _DEBUG

      // free up everything it points to
      PCMIFLVar pv = (PCMIFLVar) pdi->plCMIFLVar->Get(0);
      for (j = 0; j < pdi->plCMIFLVar->Num(); j++, pv++)
         pv->SetUndefined();
      delete pdi->plCMIFLVar;

      // done
   } // i
}



/*************************************************************************************
CDatabaseCat::Init - Call this to initialize the database category and
open (or created) the database file.

inputs
   PWSTR          pszName - Database name, such as "players"
   PWSTR          pszFile - Database file
returns
   BOOL - TRUE if success
*/
static PWSTR gpszQuick = L"Quick";
static PWSTR gpszColumn = L"Columns";
static PWSTR gpszElem = L"Elem";
static PWSTR gpszStrings = L"Strings";
static PWSTR gpszLists = L"Lists";
static PWSTR gpszName = L"Name";
static PWSTR gpszID = L"ID";
static PWSTR gpszCache = L"Cache";

BOOL CDatabaseCat::Init (PWSTR pszName, PWSTR pszFile, PCMIFLVM pVM)
{
   if (m_szName[0] || !pszName[0])
      return FALSE;
   wcscpy (m_szName, pszName);
   m_pVM = pVM;

   if (!m_MegaFileInThread.Init (pszFile, &GUID_DatabaseCat, TRUE))
      return FALSE;

   // read in the columns...
   PCMMLNode2 pNode = MMLLoad (TRUE, gpszQuick);
   DWORD i, j;
   if (!pNode)
      return TRUE;   // no quick-info, so done

   PCMMLNode2 pSub, pVarNode;
   PWSTR psz;
   PCHashDWORD phString = NULL, phList = NULL;
   CHashGUID hObjectRemap;
   CMIFLVar var;
   BOOL fRet = TRUE;

   pSub = NULL;
   pNode->ContentEnum (pNode->ContentFind (gpszStrings), &psz, &pSub);
   if (pSub)
      phString = CMIFLVarStringMMLFrom (pSub);
   if (!phString) {
      fRet = FALSE;
      goto done;
   }

   // get the lists
   pSub = NULL;
   pNode->ContentEnum (pNode->ContentFind (gpszLists), &psz, &pSub);
   if (pSub)
      phList = CMIFLVarListMMLFrom (pSub, m_pVM, phString, &hObjectRemap);
   if (!phList) {
      fRet = FALSE;
      goto done;
   }

   for (i = 0; i < pNode->ContentNum(); i++) {
      pSub = NULL;
      pNode->ContentEnum (i, &psz, &pSub);
      if (!pSub)
         continue;
      psz = pSub->NameGet();
      if (!psz)
         continue;
      if (!_wcsicmp(psz, gpszColumn)) {
         psz = MMLValueGet (pSub, gpszName);
         if (psz)
            m_hQuick.Add (psz, 0);

         continue;
      }
      else if (!_wcsicmp(psz, gpszElem)) {
         DBITEM dbi;
         memset (&dbi, 0, sizeof(dbi));
         dbi.plCMIFLVar = new CListFixed;
         dbi.plCMIFLVar->Init (sizeof(CMIFLVar));
         dbi.fDirty = TRUE;   // used so will know if had entry saved for it too

         MMLValueGetBinary (pSub, gpszID, (PBYTE)&dbi.gID, sizeof(dbi.gID));
         for (j = 0; j < pSub->ContentNum(); j++) {
            pVarNode = NULL;
            pSub->ContentEnum (j, &psz, &pVarNode);
            if (!pVarNode)
               continue;
            psz = pVarNode->NameGet();
            if (!psz || _wcsicmp(psz, gpszCache))
               continue;

            var.MMLFrom (pVarNode, m_pVM, phString, phList, &hObjectRemap);

            var.AddRef();  // since will be copying directly
            dbi.plCMIFLVar->Add (&var);
         }; // j

         m_hDBITEM.Add (&dbi.gID, &dbi);
         continue;
      } // if gpszElem
   } // i

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
   delete pNode;
   if (!fRet)
      return FALSE;

   // enumerate all the items...
   CListVariable lName;
   CListFixed lMFFILEINFO;
   lMFFILEINFO.Init (sizeof(MFFILEINFO));
   m_MegaFileInThread.Enum (&lName, &lMFFILEINFO);
   PMFFILEINFO pmf = (PMFFILEINFO)lMFFILEINFO.Get(0);
   GUID g;
   PDBITEM pdi;
   for (i = 0; i < lName.Num(); i++, pmf++) {
      g = GUID_NULL;
      if (sizeof(g) != MMLBinaryFromString ((PWSTR)lName.Get(i), (PBYTE)&g, sizeof(g)))
         continue;   // not a guid

      // see if can find this in the hash
      pdi = (PDBITEM) m_hDBITEM.Find (&g);
      if (!pdi) {
         // couldnt find the item, so add it, since may have meant a corrupted file system
         // NOTE: Not tested
         DBITEM dbi;
         memset (&dbi, 0, sizeof(dbi));
         dbi.gID = g;
         dbi.plCMIFLVar = new CListFixed;
         dbi.plCMIFLVar->Init (sizeof(CMIFLVar));
         // note: List or variables will be blank...

         m_hDBITEM.Add (&g, &dbi);
         pdi = (PDBITEM) m_hDBITEM.Find (&g);
         if (!pdi)
            continue;
      }

      // remember times
      pdi->ftAccess = pmf->iTimeAccess;
      pdi->ftCreate = pmf->iTimeCreate;
      pdi->ftModify = pmf->iTimeModify;
      pdi->fDirty = FALSE;
   } // i

   // look through the database and remove items with pdi->fDirty==TRUE,
   // which indicates a corrupted database
   for (i = m_hDBITEM.Num()-1; i < m_hDBITEM.Num(); i--) {
      pdi = (PDBITEM) m_hDBITEM.Get (i);
      if (!pdi || !pdi->fDirty)
         continue;

      // else dirty
      // NOTE: Not tested
      if (pdi->pNode)
         delete pdi->pNode;   // shouldn't happen

      // free up everything it points to
      PCMIFLVar pv = (PCMIFLVar) pdi->plCMIFLVar->Get(0);
      for (j = 0; j < pdi->plCMIFLVar->Num(); j++, pv++)
         pv->SetUndefined();
      delete pdi->plCMIFLVar;

      m_hDBITEM.Remove (i);
   } // i

   // done
   return TRUE;
}



/*************************************************************************************
CDatabaseCat::SaveQuick - This saves the quick-access information so that
the database can be loaded later
*/
BOOL CDatabaseCat::SaveQuick (void)
{
   // if not dirty dont save
   if (!m_fDirtyQuick)
      return TRUE;

   // create node
   PCMMLNode2 pNode = new CMMLNode2;
   if (!pNode)
      return FALSE;
   pNode->NameSet (gpszQuick);

   // write out all the columns
   DWORD i;
   PWSTR psz;
   PCMMLNode2 pSub;
   for (i = 0; i < m_hQuick.Num(); i++) {
      psz = m_hQuick.GetString (i);

      pSub = pNode->ContentAddNewNode ();
      if (!pSub)
         break;
      pSub->NameSet (gpszColumn);
      MMLValueSet (pSub, gpszName, psz);
   }

   // make up some hashes to store the strings that need...
   CHashPVOID hString, hList;
   hString.Init (0);
   hList.Init (0);


   // loop through all the items
   PCMMLNode2 pVarNode;
   DWORD j;
   for (i = 0; i < m_hDBITEM.Num(); i++) {
      PDBITEM pdi = (PDBITEM)m_hDBITEM.Get(i);
      if (!pdi)
         continue;

      pSub = pNode->ContentAddNewNode();
      if (!pSub)
         continue;
      pSub->NameSet (gpszElem);

      MMLValueSet (pSub, gpszID, (PBYTE)&pdi->gID, sizeof(pdi->gID));


      PCMIFLVar pv = (PCMIFLVar)pdi->plCMIFLVar->Get(0);
      for (j = 0; j < pdi->plCMIFLVar->Num(); j++, pv++) {
         pVarNode = pv->MMLTo (m_pVM, &hString, &hList);
         if (!pVarNode)
            continue;   // shouldnt happen
         pVarNode->NameSet (gpszCache);

         pSub->ContentAdd (pVarNode);
      }; // j
   } // i



   // write out all the strings and lists
   pSub = CMIFLVarListMMLTo (&hList, m_pVM, &hString);
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
   m_fDirtyQuick = FALSE;
   BOOL fRet;
   fRet = MMLSave (TRUE, gpszQuick, pNode);
   // NOTE: No longer need to delete pNode... delete pNode;
   return fRet;
}


/*************************************************************************************
CDatabaseCat::SaveElem - Saves a specific element, if it's dirty.

inputs
   DWORD          dwIndex - Index
returns
   BOOL - TRUE if saved OK, FALSE if error
*/
BOOL CDatabaseCat::SaveElem (DWORD dwIndex)
{
   PDBITEM pdi = (PDBITEM)m_hDBITEM.Get(dwIndex);
   if (!pdi)
      return FALSE;
   if (!pdi->fDirty ||!pdi->pNode)
      return TRUE;   // not dirty

   // make up the name
   WCHAR szTemp[64];
   MMLBinaryToString ((PBYTE)&pdi->gID, sizeof(pdi->gID), szTemp);

   // save
   PCMMLNode2 pClone = pdi->pNode->Clone();
   if (!pClone)
      return FALSE;
   if (!MMLSave (TRUE, szTemp, pClone, &pdi->ftCreate, &pdi->ftModify, &pdi->ftAccess))
      return FALSE;
      // BUGFIX - Save elements as compressed

   // done
   pdi->fDirty = FALSE;
   return TRUE;
}


/*************************************************************************************
CDatabaseCat::SaveAll - Saves everything in the database that's dirty

returns
   BOOL - TRUE if success, FALSE if error
*/
BOOL CDatabaseCat::SaveAll (void)
{
   // save the quick-access
   if (!SaveQuick())
      return FALSE;

   // save individual items
   DWORD i;
   for (i = 0; i < m_hDBITEM.Num(); i++)
      if (!SaveElem (i))
         return FALSE;

   return TRUE;
}


/*************************************************************************************
CDatabaseCat::SavePartial - This is used for a piece-meal save that will
save to disk slowly over time. What this does is:
- One out of 1024 calls, this saves the full index
- Increments through all the objects and tries saving one each go

inputs
   DWORD       dwNumber - This is a value that should increate by 1 every
                     time it's called
returns
   BOOL - TRUE if the save was usccessful
*/
BOOL CDatabaseCat::SavePartial (DWORD dwNumber)
{
   if (!(dwNumber % 1024))
      if (!SaveQuick())
         return FALSE;

   DWORD dwNum = m_hDBITEM.Num();
   if (!dwNum)
      return TRUE;   // nothing to save

   return SaveElem (dwNumber % dwNum);
}


/*************************************************************************************
CDatabaseCat::MMLLoad - Loads an element (or other info) from the megafile.

inputs
   BOOL        fCompressed - TRUE if the info should be compressed, FALSE if not
   PWSTR       pszName - File name
returns
   PCMMLNode2 - Node, or NULL if error
*/
PCMMLNode2 CDatabaseCat::MMLLoad (BOOL fCompressed, PWSTR pszName)
{
   return m_MegaFileInThread.LoadMML (pszName, fCompressed);

#if 0 // olf code, pre thread megafile
   if (fCompressed)
      return MMLFileOpen (&m_MegaFile, pszName, &GUID_DatabaseCompressed);

   // else, raw
   __int64 iSize;
   PWSTR psz = (PWSTR) m_MegaFile.Load (pszName, &iSize);
   if (!psz)
      return NULL;
   PCMMLNode2 pNode = MMLFromMem(psz);
   pNode->NameSet (L"MMLLoad");
   MegaFileFree (psz);

   return pNode;
#endif // 0
}


/*************************************************************************************
CDatabaseCat::MMLSave - Saves the information to the megafile

inputs
   BOOL        fCompressed - TRUE if want to compress the data, FALSE if nont
   PWSTR       pszName - File name
   PCMMLNode2   pNode - Node. NOTE: This is DELTEED by the call.
   PFILETIME   pftCreate - Creation time. Can be NULL. Only used if !fCompressed
   PFILETIME   pftModify - Modify time. Can be NULL. Only used if !fCompressed
   PFILETIME   pftAccess - Access time. Can be NULL. Only used if !fCompressed
returns
   BOOl - TRUE if write out
*/
BOOL CDatabaseCat::MMLSave (BOOL fCompressed, PWSTR pszName, PCMMLNode2 pNode, FILETIME *pftCreate, FILETIME *pftModify,
   FILETIME *pftAccess)
{
   return m_MegaFileInThread.SaveMML (pszName, pNode, fCompressed, pftCreate, pftModify, pftAccess);

#if 0 // old code before mefagile in thread
   BOOL fRet;
   if (fCompressed) {
      fRet = MMLFileSave (&m_MegaFile, pszName, &GUID_DatabaseCompressed, pNode);
      delete pNode;
      return fRet;
   }

   // else, raw text
   CMem mem;
   if (!MMLToMem (pNode, &mem, TRUE)) {
      delete pNode;
      return FALSE;
   }
   mem.CharCat (0);

   fRet = m_MegaFile.Save (pszName, mem.p, mem.m_dwCurPosn, pftCreate, pftModify, pftAccess);
   delete pNode;
   return fRet;
#endif
}


/*************************************************************************************
CDatabaseCat::ElemCache - Caches as element if it isn't already

inputs
   DWORD       dwIndex - Index number
returns
   BOOL - TRUE if success
*/
BOOL CDatabaseCat::ElemCache (DWORD dwIndex)
{
   // flush elements if too many checked out
   ElemFlush ();

   PDBITEM pdi = (PDBITEM)m_hDBITEM.Get(dwIndex);
   if (!pdi)
      return FALSE;
   if (pdi->pNode)
      return TRUE;   // already cached

   // try to load

   // make up the name
   WCHAR szTemp[64];
   MMLBinaryToString ((PBYTE)&pdi->gID, sizeof(pdi->gID), szTemp);

   pdi->pNode = MMLLoad (TRUE, szTemp);
      // BUGFIX - Save elements as compressed
   if (!pdi->pNode)
      return FALSE;

   pdi->fDirty = FALSE; // just in case was set
   m_dwCached++;

   return TRUE;
}

/*************************************************************************************
CDatabaseCat::ElemUnCache - Writes out a cached element, and frees up the caching.

inputs
   DWORD       dwIndex - Index number
returns
   BOOL - TRUE if success
*/
BOOL CDatabaseCat::ElemUnCache (DWORD dwIndex)
{
   PDBITEM pdi = (PDBITEM)m_hDBITEM.Get(dwIndex);
   if (!pdi || pdi->dwCheckedOut)   // cant uncache chcecked out
      return FALSE;
   if (!pdi->pNode)
      return TRUE;   // already uncached

   // if dirty then save if
   if (pdi->fDirty)
      if (!SaveElem (dwIndex))
         return FALSE;

   // can delete
   delete pdi->pNode;
   pdi->pNode = NULL;
   pdi->fDirty = FALSE;
   m_dwCached--;

   return TRUE;
}



/*************************************************************************************
CDatabaseCat::ElemDelete - Deletes an element.

inputs
   DWORD       dwIndex - Index
returns
   BOOL - TRUE if success, FALSE if error. May error out if checked out
*/
BOOL CDatabaseCat::ElemDelete (DWORD dwIndex)
{
   PDBITEM pdi = (PDBITEM)m_hDBITEM.Get(dwIndex);
   if (!pdi)
      return FALSE;

   // make sure it isn't cached
   if (!ElemUnCache (dwIndex))
      return FALSE;

   // delete the file
   WCHAR szTemp[64];
   MMLBinaryToString ((PBYTE)&pdi->gID, sizeof(pdi->gID), szTemp);
   m_MegaFileInThread.Delete (szTemp);

   // free up the list elements
   DWORD j;
   PCMIFLVar pv = (PCMIFLVar) pdi->plCMIFLVar->Get(0);
   for (j = 0; j < pdi->plCMIFLVar->Num(); j++, pv++)
      pv->SetUndefined();
   delete pdi->plCMIFLVar;

   m_hDBITEM.Remove (dwIndex);
   m_fDirtyQuick = TRUE;   // note that have dirtied quick list

   return TRUE;
}


/*************************************************************************************
CDatabaseCat::ElemSyncFromObject - Given an object, this synchronizes all the
properties from the object.

inputs
   PCMIFLVMObject pObj - object
   DWORD          dwIndex - Index to element
returns
   BOOL - TRUE if success
*/
BOOL CDatabaseCat::ElemSyncFromObject (PCMIFLVMObject pObj, DWORD dwIndex)
{
   PDBITEM pdi = (PDBITEM)m_hDBITEM.Get(dwIndex);
   if (!pdi)
      return FALSE;

   // if there aren't enough entries then add
   CMIFLVarLValue var;  // which will be initialized to undefined
   while (pdi->plCMIFLVar->Num() < m_hQuick.Num()) {
      pdi->plCMIFLVar->Add (&var);
      m_fDirtyQuick = TRUE;
   };

   // loop through all the attributes and get
   PCMIFLVar pv = (PCMIFLVar) pdi->plCMIFLVar->Get(0);
   DWORD i;
   for (i = 0; i < m_hQuick.Num(); i++, pv++) {
      PWSTR psz = m_hQuick.GetString (i);

      // if it's a "containedin" property then look at what it's contained in
      if (!_wcsicmp(psz, gpszContainedInProp)) {
         switch (pv->TypeGet()) {
         case MV_OBJECT:
            {
               GUID g;
               g = pv->GetGUID();
               if (IsEqualGUID (g, pObj->m_gContainedIn))
                  continue;   // no change
            }
            break;
         case MV_NULL:
               if (IsEqualGUID (GUID_NULL, pObj->m_gContainedIn))
                  continue;   // no change
            break;
         }

         // else differnt
         if (IsEqualGUID (pObj->m_gContainedIn, GUID_NULL))
            pv->SetNULL();
         else
            pv->SetObject (&pObj->m_gContainedIn);
         m_fDirtyQuick = TRUE;
         continue;
      }

      DWORD dwID = m_pVM->ToPropertyID (psz, FALSE);
      var.m_Var.SetUndefined();
      if (dwID != -1)
         m_pVM->PropertyGet (dwID, pObj, TRUE, &var);

      // if they're the same then no change
      if (!var.m_Var.Compare (pv, TRUE, m_pVM))
         continue;

      // else changed...
      pv->Set (&var.m_Var);
      pv->Fracture ();
      m_fDirtyQuick = TRUE;
   }  // i, all strings

   return TRUE;
}




/*************************************************************************************
CDatabaseCat::ObjectAdd - Adds a new object to the database. THis object
will automatically be checked out.

inputs
   PCMIFLVMObject    pObj - Object
returns
   BOOL - TRUE if was sucessfully added
*/
BOOL CDatabaseCat::ObjectAdd (PCMIFLVMObject pObj)
{
   // make sure doens't alreayd exist
   DWORD dwIndex = m_hDBITEM.FindIndex (&pObj->m_gID);
   if (dwIndex != -1)
      return FALSE;

   // create the entry...
   DBITEM dbi;
   memset (&dbi, 0, sizeof(dbi));
   dbi.dwCheckedOut = TRUE;
   dbi.fDirty = TRUE;
   GetSystemTimeAsFileTime (&dbi.ftCreate);
   dbi.ftAccess = dbi.ftCreate;
   dbi.ftModify = dbi.ftCreate;
   dbi.gID = pObj->m_gID;
   dbi.plCMIFLVar = new CListFixed;
   dbi.plCMIFLVar->Init (sizeof(CMIFLVar));
   dbi.pNode = m_pVM->MMLTo (FALSE, FALSE, &pObj->m_gID, 1, FALSE);
   if (!dbi.pNode) {
      delete dbi.plCMIFLVar;
      return FALSE;
   }

   // add it
   m_hDBITEM.Add (&dbi.gID, &dbi);
   dwIndex = m_hDBITEM.FindIndex (&pObj->m_gID);
   m_dwCached++;
   m_dwCheckedOut++;

   // BUGFIX - Note that quick-refernce is dirty
   m_fDirtyQuick = TRUE;

   // syncronize for cache
   return ElemSyncFromObject (pObj, dwIndex);
}


/*************************************************************************************
CDatabaseCat::ObjectSave - Saves a checked-out object. This does NOT check
it in though. It does set the dirty flag.

inputs
   PCMIFLVMObject    pObj - Object
returns
   BOOL - TRUE if was sucessfully saved
*/
BOOL CDatabaseCat::ObjectSave (PCMIFLVMObject pObj)
{
   DWORD dwIndex = m_hDBITEM.FindIndex (&pObj->m_gID);
   PDBITEM pdi = (PDBITEM)m_hDBITEM.Get (dwIndex);
   if (!pdi || !pdi->dwCheckedOut)
      return FALSE;

   // get the new node...
   PCMMLNode2 pNode = m_pVM->MMLTo (FALSE, FALSE, &pObj->m_gID, 1, FALSE);
   if (!pNode)
      return FALSE;

   // update
   pdi->fDirty = TRUE;
   if (pdi->pNode)
      delete pdi->pNode;
   pdi->pNode = pNode;
   GetSystemTimeAsFileTime (&pdi->ftModify);
   pdi->ftAccess = pdi->ftModify;

   // update the variables
   return ElemSyncFromObject (pObj, dwIndex);
}


/*************************************************************************************
CDatabaseCat::ObjectCheckIn - Checks in a checked out object.

inputs
   PCMIFLVMObject    pObj - Object
   BOOL              fNoSave - If TRUE then checks it in without updating the changed
   BOOL              fDelete - If TRUE then delete the object after it's checked in
   PCMIFLVM          pVM - VM used for deleteing
returns
   BOOL - TRUE if was sucessfully saved
*/
BOOL CDatabaseCat::ObjectCheckIn (PCMIFLVMObject pObj, BOOL fNoSave, BOOL fDelete, PCMIFLVM pVM)
{
   // save it first, in case it's dirty
   if (!fNoSave)
      if (!ObjectSave (pObj))
         return FALSE;

   PDBITEM pdi = (PDBITEM)m_hDBITEM.Find (&pObj->m_gID);
   if (!pdi || !pdi->dwCheckedOut)
      return FALSE;

   // since has been saved and all, just check in
   pdi->dwCheckedOut = 0;

   // flush elements if too many out
   m_dwCheckedOut--;
   ElemFlush ();

   // delete
   if (fDelete)
      pVM->ObjectDelete (&pObj->m_gID);

   return TRUE;
}
   

/*************************************************************************************
CDatabaseCat::ObjectCheckOut - Checks out an object (which baically loads it
into the VM). After an object is checked out, it MUST be checked in when done
using it.

inputs
   GUID        *pgID - Object ID to check out
returns
   BOOL - TRUE if successfull checked out
*/
BOOL CDatabaseCat::ObjectCheckOut (GUID *pgID)
{
   // if it already exists in the VM then can't
   if (m_pVM->ObjectFind (pgID))
      return FALSE;

   // find the index
   DWORD dwIndex = m_hDBITEM.FindIndex (pgID);
   if (dwIndex == -1)
      return FALSE;  // cant check out because doesnt exist
   PDBITEM pdi = (PDBITEM)m_hDBITEM.Get(dwIndex);
   if (!pdi || pdi->dwCheckedOut)
      return FALSE;  // cant check out because already checked out elsewhere

   // cache this
   if (!ElemCache (dwIndex)) {
      // BUGFIX - If it's in the index, but can't actually cache it, then delete it
      // Do this in case database gets corrupted
      ObjectDelete (pgID);
      return FALSE;  // error, cant seem to load it in
   }

   // add to the VM
   if (!m_pVM->MMLFrom (FALSE, FALSE, FALSE, TRUE, pdi->pNode, NULL))
      return FALSE;

   // if any fields in cache are empty (because of database corruption)
   // then when check out fill them in
   if (pdi->plCMIFLVar->Num() != m_hQuick.Num()) {
      PCMIFLVMObject pObj = m_pVM->ObjectFind (pgID);
      if (pObj)
         ElemSyncFromObject (pObj, dwIndex);
   }

   // else, it worked...
   pdi->dwCheckedOut = TRUE;
   GetSystemTimeAsFileTime (&pdi->ftAccess); // so know when access
   m_dwCheckedOut++;
   return TRUE;
}



/*************************************************************************************
CDatabaseCat::ObjectQueryCheckOut - Returns 2 if the object is checked out by
the current VM, 1 if checked out by another VM, 0 if it isn't checked out,
-1 if it doesn't exist

inputs
   GUID        *pgID - Object ID to check out
   BOOL        fTestForOtherVM - If TRUE this tests to see if its checekd out
               on another VM. (Slower)
returns
   int - Check out value. Above
*/
int CDatabaseCat::ObjectQueryCheckOut (GUID *pgID, BOOL fTestForOtherVM)
{
   // find the index
   DWORD dwIndex = m_hDBITEM.FindIndex (pgID);
   if (dwIndex == -1)
      return -1;  // cant check out because doesnt exist
   PDBITEM pdi = (PDBITEM)m_hDBITEM.Get(dwIndex);
   if (!pdi)
      return -1;
   if (pdi->dwCheckedOut)
      return 2;  // cant check out because already checked out elsewhere

   // NOTE: Since databases can't be shared across processes, a value
   // of 1 won't be returned

   // else, not chcked out
   return 0;
}

/*************************************************************************************
CDatabaseCat::ObjectDelete - Deletes an object.

inputs
   GUID        *pgID - Object ID to delete
returns
   BOOL - TRUE if successfull checked out
*/
BOOL CDatabaseCat::ObjectDelete (GUID *pgID)
{
   DWORD dwIndex = m_hDBITEM.FindIndex (pgID);
   if (dwIndex == -1)
      return FALSE;

   PDBITEM pdi = (PDBITEM)m_hDBITEM.Get (dwIndex);
   if (!pdi)
      return FALSE;

   if (pdi->dwCheckedOut) {
      pdi->dwCheckedOut = 0;
      m_dwCheckedOut--;
   }

   return ElemDelete (dwIndex);
}



/*************************************************************************************
CDatabaseCat::ObjectDelete - Deletes an object.

inputs
   PCMIFLVar      pVar - This is either a single object, or a list of objects.
returns
   BOOL - TRUE if successfully deleted. FALSE if cant delete one. Even
            if one object cant be deleted, as many will be deleted as possible
*/
BOOL CDatabaseCat::ObjectDelete (PCMIFLVar pVar)
{
   switch (pVar->TypeGet()) {
   case MV_OBJECT:
      {
         GUID g = pVar->GetGUID();
         return ObjectDelete (&g);
      }
   case MV_LIST:
      break;   // below
   default:
      return FALSE;
   }

   // it's a list
   BOOL fRet = TRUE;
   PCMIFLVarList pl = pVar->GetList();
   DWORD i;
   GUID g;
   for (i = 0; i < pl->Num(); i++) {
      PCMIFLVar pv = pl->Get(i);
      if (pv->TypeGet() != MV_OBJECT) {
         fRet = FALSE;
         continue;
      }

      g = pv->GetGUID();
      fRet &= ObjectDelete (&g);
   } // i
   pl->Release();

   return fRet;
}

/*************************************************************************************
CDatabaseCat::ObjectQuery - Querries info about the object.

inputs
   GUID        *pgID - Object ID
   PDCOQ       pdcoq - Filled with info if valid object
returns
   BOOL - TRUE if finds object, FALSE if object doesn't exist
*/
BOOL CDatabaseCat::ObjectQuery (GUID *pgID, PDCOQ pdcoq)
{
   // NOTE: Not tested
   PDBITEM pdi = (PDBITEM)m_hDBITEM.Find (pgID);
   if (!pdi)
      return FALSE;

   pdcoq->dwCheckedOut = pdi->dwCheckedOut;
   pdcoq->ftAccess = pdi->ftAccess;
   pdcoq->ftModify = pdi->ftModify;
   pdcoq->ftCreate = pdi->ftCreate;

   return TRUE;
}


/*************************************************************************************
CDatabaseCat::CacheAdd - Adds a property that needs to be cached.

NOTE: If this is called AFTER items are already in the database then the
cached attributes for those will be undefined.

inputs
   PWSTR       pszProp - Property that needs to be cached.
returns
   BOOL - TRUE if success (or it's already there). FALSE if error
*/
BOOL CDatabaseCat::CacheAdd (PWSTR pszProp)
{
   if (!MIFLIsNameValid(pszProp))
      return FALSE;  // must be valid name

   if (-1 != m_hQuick.FindIndex (pszProp))
      return TRUE;   // already there

   // else, add
   m_hQuick.Add (pszProp, NULL);

   // note: dirty
   m_fDirtyQuick = TRUE;
   return TRUE;
}


/*************************************************************************************
CDatabaseCat::CacheRemove - Removes a property from the list of properties
to be cached by the database.

inputs
   PWSTR          pszProp - Property to be removed
returns
   BOOL - TRUE if success, FALSE if couldnt find
*/
BOOL CDatabaseCat::CacheRemove (PWSTR pszProp)
{
   DWORD dwIndex = m_hQuick.FindIndex (pszProp);
   if (dwIndex == -1)
      return FALSE;

   // loop through all the objects and remove
   DWORD i;
   for (i = 0; i < m_hDBITEM.Num(); i++) {
      PDBITEM pdi = (PDBITEM)m_hDBITEM.Get(i);

      PCMIFLVar pv = (PCMIFLVar) pdi->plCMIFLVar->Get(dwIndex);
      if (pv)
         pv->SetUndefined();  // so will delete contents
      pdi->plCMIFLVar->Remove (dwIndex);
   } // i

   // done
   m_hQuick.Remove (dwIndex);
   m_fDirtyQuick = TRUE;
   return TRUE;
}


/*************************************************************************************
CDatabaseCat::CacheEnum - Fills in a CMIFLVar with a list of all the cached
properties.

inputs
   PCMIFLVar      pVar - Filled in
returns
   BOOL - TRUE if success
*/
BOOL CDatabaseCat::CacheEnum (PCMIFLVar pVar)
{
   PCMIFLVarList pl = new CMIFLVarList;
   if (!pl) {
      pVar->SetBOOL (FALSE);
      return FALSE;
   }

   DWORD i;
   CMIFLVar var;
   for (i = 0; i < m_hQuick.Num(); i++) {
      PWSTR psz = m_hQuick.GetString (i);
      var.SetString (psz);
      pl->Add (&var, TRUE);
   }

   pVar->SetList (pl);
   pl->Release();
   return TRUE;
}


/*************************************************************************************
CDatabaseCat::ElemFlush - Flushes out elements if too many are cached.
*/
void CDatabaseCat::ElemFlush (void)
{
   // allow up to 100 objects cached beyond what's checked out
   while (m_dwCheckedOut + MAXELEMCACHE < m_dwCached) {
      // loop through and find best one to flush
      PDBITEM pdiLastUsed = NULL;
      DWORD i;
      PDBITEM pdi;
      DWORD dwLastUsed = -1;
      m_dwCheckedOut = m_dwCached = 0; // might as well recalc
      for (i = 0; i < m_hDBITEM.Num(); i++) {
         pdi = (PDBITEM)m_hDBITEM.Get(i);

         if (pdi->pNode) {
            m_dwCached++;
            if (pdi->dwCheckedOut)
               m_dwCheckedOut++;
         }

         if (!pdi->pNode || pdi->dwCheckedOut)
            continue;   // dont care about this

         if (!pdiLastUsed || (CompareFileTime (&pdi->ftAccess, &pdiLastUsed->ftAccess) < 0)) {
            pdiLastUsed = pdi;   // remember this as lowest
            dwLastUsed = i;
         }
      } // i

      // remove lowest
      if (dwLastUsed != -1)
         ElemUnCache (dwLastUsed);

   } // while too many cached
}



/*************************************************************************************
CDatabaseCat::FreeStringList - Frees the phString and phList from GetObjectMML.

inputs
   PCHashDWORD phString - Filled in with the string hash, which must be freed
   PCHashDWORD phList - Filled in with the list hash, which must be freed
returns
   none
*/
void CDatabaseCat::FreeStringList (PCHashDWORD phString, PCHashDWORD phList)
{
   DWORD i;

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

}
   

/*************************************************************************************
CDatabaseCat::GetObjectMML - This caches an objects and returns a PCMMLNode2 for
the Object's MML. This can then be searched through for the given variable.

inputs
   DWORD       dwIndex - Index for the object
   PCHashDWORD *pphString - Filled in with the string hash, which must be freed
   PCHashDWORD *pphList - Filled in with the list hash, which must be freed
returns
   PCMMLNode2 - The object MML, or NULL if error
*/
static PWSTR gpszVMObjects = L"VMObjects";
static PWSTR gpszObject = L"Object";

PCMMLNode2 CDatabaseCat::GetObjectMML (DWORD dwIndex, PCHashDWORD *pphString, PCHashDWORD *pphList)
{
   *pphString = *pphList = NULL;

   PCHashDWORD phString = NULL, phList = NULL;
   PCHashGUID phObjectRemap = NULL;

   // loading in
   if (!ElemCache (dwIndex))
      return NULL;

   PDBITEM pdi = (PDBITEM) m_hDBITEM.Get(dwIndex);
   PCMMLNode2 pSub = NULL;
   PCMMLNode2 pNode = pdi->pNode;
   PWSTR psz;
   pNode->ContentEnum (pNode->ContentFind (gpszStrings), &psz, &pSub);
   if (pSub)
      phString = CMIFLVarStringMMLFrom (pSub);

   phObjectRemap = new CHashGUID;
   if (!phString || !phObjectRemap)
      goto err;

   // get the lists
   pSub = NULL;
   pNode->ContentEnum (pNode->ContentFind (gpszLists), &psz, &pSub);
   if (pSub)
      phList = CMIFLVarListMMLFrom (pSub, m_pVM, phString, phObjectRemap);
   if (!phList)
      goto err;

   // find the objects... look into MML created by CMIFLVM::MMLTo
   PCMMLNode2 pObjects = NULL;
   pNode->ContentEnum (pNode->ContentFind (gpszVMObjects), &psz, &pObjects);
   if (!pObjects)
      goto err;

   // read in object created by CMIFLVM::ObjectMMLTo
   PCMMLNode2 pObject = NULL;
   pObjects->ContentEnum (pObjects->ContentFind (gpszObject), &psz, &pObject);
   if (!pObject)
      goto err;

   // read in object created by CMIFLVMObject::MMLTo
   GUID gID;
   MMLValueGetBinary (pObject, gpszID, (PBYTE)&gID, sizeof(gID));
   if (!IsEqualGUID (gID, pdi->gID))
      goto err;

   *pphString = phString;
   *pphList = phList;
   delete phObjectRemap;
   return pObject;


err:
   FreeStringList (phString, phList);
   if (phObjectRemap)
      delete phObjectRemap;
   return NULL;
}


/*************************************************************************************
CDatabaseCat::GetPropMML - This searches through the object node returned
by GetObjectMML for the given property. If it's found then the index
for the property is returned. Otherwise, -1 is returned.

inputs
   PCMMLNode2      pObject - From GetObjectMML()
   PWSTR          pszProp - Property name to find
returns
   DWORD - Index into pObject, or -1 if error
*/
static PWSTR gpszProp = L"Prop";
static PWSTR gpszVar = L"Var";
static PWSTR gpszDeleted = L"Deleted";
DWORD CDatabaseCat::GetPropMML (PCMMLNode2 pObject, PWSTR pszProp)
{
   PCMMLNode2 pSub;
   PWSTR psz;
   DWORD i;
   for (i = 0; i < pObject->ContentNum(); i++) {
      pSub = NULL;
      pObject->ContentEnum (i, &psz, &pSub);
      if (!pSub)
         continue;
      psz = pSub->NameGet();
      if (!psz || _wcsicmp(psz, gpszProp))
         continue;   // only looking for prop

      if (MMLValueGet (pSub, gpszObject))
         continue;   // no private properties
      PWSTR pszName = MMLValueGet (pSub, gpszName);
      if (!pszName || _wcsicmp(pszName, pszProp))
         continue;   // wrong name

      // else, found it
      return i;
   } // i

   // cant find
   return -1;
}


/*************************************************************************************
CDatabaseCat::ObjectAttribGet - Get the attributes of an object.

DOCUMENT: This also handles attributes for:
   - DataIsCheckedOut - bool, TRUE for checked out
   - DataDateCreated - Date created. same as MIFL, number of days since jan 1 2000
   - DataDateModified - Date modified.
   - DataDateAccessed - Date accessed

inputs
   GUID        *pgID - Object ID
   DWORD       dwNum - number of attributes
   PWSTR       *ppszAttrib - Pointer to list of dwNum PWSTR with attribute names
   PCMIFLVar   paVar - Pointer to an array of dwNum paVar that are filled in
   BOOL        *pfCheckedOut - Filled with TRUE if it's checked out (so data may not be up
                  to date)
returns
   BOOL - TRUE if success, FALSE if can't find object
*/
BOOL CDatabaseCat::ObjectAttribGet (GUID *pgID, DWORD dwNum, PWSTR *ppszAttrib,
                                    PCMIFLVar paVar, BOOL *pfCheckedOut)
{
   // find the object
   DWORD dwIndex = m_hDBITEM.FindIndex (pgID);
   if (dwIndex == -1)
      return FALSE;
   PDBITEM pdi = (PDBITEM)m_hDBITEM.Get (dwIndex);
   if (!pdi)
      return FALSE;

   if (pfCheckedOut)
      *pfCheckedOut = pdi->dwCheckedOut ? TRUE : FALSE;

   PCHashDWORD phString = NULL, phList = NULL;
   PCMMLNode2 pObject = NULL;

   // loop through all the attributes
   DWORD i;
   PWSTR psz;
   for (i = 0; i < dwNum; i++) {
      // see if it's a cached attribute
      DWORD dwFind = m_hQuick.FindIndex (ppszAttrib[i]);
      if (dwFind != -1) {
         // cached
         PCMIFLVar pv = (PCMIFLVar) pdi->plCMIFLVar->Get(dwFind);
         if (pv) {
            paVar[i].Set (pv);
            paVar[i].Fracture();
         }
         else
            paVar[i].SetUndefined();   // not cached
         continue;
      }

      // see if it's a database item
      if (!_wcsnicmp(ppszAttrib[i], L"data", 4)) {
         if (!_wcsicmp(ppszAttrib[i] + 4, L"ischeckedout")) {
            paVar[i].SetBOOL (pdi->dwCheckedOut ? TRUE : FALSE);
            continue;
         }
         else if (!_wcsicmp(ppszAttrib[i] + 4, L"datecreated")) {
            paVar[i].SetDouble (MIFLFileTimeToDouble(&pdi->ftCreate));
            continue;
         }
         else if (!_wcsicmp(ppszAttrib[i] + 4, L"datemodified")) {
            paVar[i].SetDouble (MIFLFileTimeToDouble(&pdi->ftModify));
            continue;
         }
         else if (!_wcsicmp(ppszAttrib[i] + 4, L"dateaccessed")) {
            paVar[i].SetDouble (MIFLFileTimeToDouble(&pdi->ftAccess));
            continue;
         }
      } // if object info

      // if get here, try to get the attribute from the object.

      // do we need to load in?
      if (!pObject)
         pObject = GetObjectMML (dwIndex, &phString, &phList);
      if (!pObject) {
         paVar[i].SetUndefined();
         continue;
      }

      // if it's a contained-in value then need different way of getting
      if (!_wcsicmp(ppszAttrib[i], gpszContainedInProp)) {
         GUID g;
         if (sizeof(g) != MMLValueGetBinary (pObject, gpszContainedIn, (PBYTE)&g, sizeof(g)))
            g = GUID_NULL;

         if (IsEqualGUID (g, GUID_NULL))
            paVar[i].SetNULL();
         else
            paVar[i].SetObject (&g);
         continue;
      }

      // find this property
      DWORD dwProp = GetPropMML (pObject, ppszAttrib[i]);
      if (dwProp == -1) {
         paVar[i].SetUndefined();
         continue;
      }

      // get the value
      PCMMLNode2 pProp, pVar;
      pProp = pVar = NULL;
      pObject->ContentEnum (dwProp, &psz, &pProp);
      pProp->ContentEnum (pProp->ContentFind (gpszVar), &psz, &pVar);
      if (!pVar) {
         // could happen if was deleted
         paVar[i].SetUndefined();
         continue;
      }

      // else, have value
      paVar[i].MMLFrom (pVar, m_pVM, phString, phList, NULL);
   } // i

   // free up the strings and lists
   FreeStringList (phString, phList);

   // done
   return TRUE;
}



/*************************************************************************************
CDatabaseCat::ObjectAttribSet - Set the attributes of an object.

NOTE: If the object is checked out this will fail.

inputs
   GUID        *pgID - Object ID
   DWORD       dwNum - number of attributes
   PWSTR       *ppszAttrib - Pointer to list of dwNum PWSTR with attribute names
   PCMIFLVar   paVar - Pointer to an array of dwNum paVar that have the attribute contents
returns
   BOOL - TRUE if success, FALSE if can't find object or if checked out
*/
BOOL CDatabaseCat::ObjectAttribSet (GUID *pgID, DWORD dwNum, PWSTR *ppszAttrib,
                                    PCMIFLVar paVar)
{
   // find the object
   BOOL fRet = TRUE;
   DWORD dwIndex = m_hDBITEM.FindIndex (pgID);
   if (dwIndex == -1)
      return FALSE;
   PDBITEM pdi = (PDBITEM)m_hDBITEM.Get (dwIndex);
   if (!pdi || pdi->dwCheckedOut)
      return FALSE;

   PCHashDWORD phString = NULL, phList = NULL;
   CHashPVOID hStringVoid, hListVoid;
   PCMMLNode2 pObject = NULL;

   pdi->fDirty = TRUE;

   // loop through all the attributes
   DWORD i;
   CMem memLower;
   for (i = 0; i < dwNum; i++) {
      // update cached attributes
      DWORD dwFind = m_hQuick.FindIndex (ppszAttrib[i]);
      if (dwFind != -1) {
         m_fDirtyQuick = TRUE; // so know that changed index

         // add undefined until long enough
         while (pdi->plCMIFLVar->Num() <= dwFind) {
            CMIFLVar vUndef;
            pdi->plCMIFLVar->Add (&vUndef);
         }

         PCMIFLVar pv = (PCMIFLVar) pdi->plCMIFLVar->Get(dwFind);
         if (pv) {
            pv->Set (&paVar[i]);
            pv->Fracture();
         }
      }  // if on cache list

      // do we need to load in?
      if (!pObject) {
         pObject = GetObjectMML (dwIndex, &phString, &phList);

         if (!pObject) {
            fRet = FALSE;
            break;
         }

         // keep a list of pointers for strings and lists
         DWORD j;
         hStringVoid.Init (0);
         hListVoid.Init (0);
         for (j = 0; j < phString->Num(); j++) {
            PCMIFLVarString pvs = *((PCMIFLVarString*)phString->Get(j));
            hStringVoid.Add (pvs, NULL);
         } // j
         for (j = 0; j < phList->Num(); j++) {
            PCMIFLVarList pvs = *((PCMIFLVarList*)phList->Get(j));
            hListVoid.Add (pvs, NULL);
         } // j
      }


      // if it's a contained-in value then need different way of getting
      if (!_wcsicmp(ppszAttrib[i], gpszContainedInProp)) {
         // delete the old one
         DWORD dwIndex = pObject->ContentFind (gpszContainedIn);
         if (dwIndex != -1)
            pObject->ContentRemove (dwIndex);

         // add a new one
         if (paVar[i].TypeGet() == MV_OBJECT) {
            GUID g;
            g = paVar[i].GetGUID();
            if (!IsEqualGUID(g, GUID_NULL))
               MMLValueSet (pObject, gpszContainedIn, (PBYTE)&g, sizeof(g));
         }
         continue;
      }

      // find this property, and delete it
      DWORD dwProp = GetPropMML (pObject, ppszAttrib[i]);
      if (dwProp != -1)
         pObject->ContentRemove (dwProp);

      // create a new property
      PCMMLNode2 pProp = pObject->ContentAddNewNode();
      if (!pProp)
         continue;
      pProp->NameSet (gpszProp);

      // lower case attribute name
      MemZero (&memLower);
      MemCat (&memLower, ppszAttrib[i]);
      _wcslwr ((PWSTR)memLower.p);
      MMLValueSet (pProp, gpszName, (PWSTR)memLower.p);

      // write in the value
      PCMMLNode2 pVar = paVar[i].MMLTo (m_pVM,&hStringVoid, &hListVoid);
      if (pVar) {
         pVar->NameSet (gpszVar);
         pProp->ContentAdd (pVar);
      }
   } // i

   // rewrite the list of strings and lists for the VM
   PCMMLNode2 pNode = pdi->pNode;
   if (pNode) {
      // remove previous entries
      DWORD dwFind = pNode->ContentFind (gpszLists);
      if (dwFind != -1)
         pNode->ContentRemove (dwFind);
      dwFind = pNode->ContentFind (gpszStrings);
      if (dwFind != -1)
         pNode->ContentRemove (dwFind);

      // add new ones
      PCMMLNode2 pSub;

      // write out all the strings and lists
      pSub = CMIFLVarListMMLTo (&hListVoid, m_pVM, &hStringVoid);
      if (pSub) {
         pSub->NameSet (gpszLists);
         pNode->ContentAdd (pSub);
      }

      // BUGFIX - write out strings after lists
      pSub = CMIFLVarStringMMLTo (&hStringVoid);
      if (pSub) {
         pSub->NameSet (gpszStrings);
         pNode->ContentAdd (pSub);
      }
   }

   // free up the strings and lists
   FreeStringList (phString, phList);

   // done
   return fRet;
}


/*************************************************************************************
CDatabaseCat::ObjectAttribGet - This gets the attribute(s) for one or more objects.
It's a MIFLVar version of ObjectAttribGet().

NOTE: The attributes can also be stuff like DataIsCheckedOut, etc. See
the other ObjectAttribGet()

inputs
   PCMIFLVar      pvObject - This is either an object, or a list of objects.
   PCMIFLVar      pvProp - This is either a single property, or a list of properties.
   PCMIFLVar      pvResult - If pvObject is a single object and pvProp is a single
                     property, then pvResult is a single result. If prObject is multiple
                     and pvProp is single, it's a list of properties, one per object.
                     If pvObject is single and pvProp is multuple, it's a list
                     of properties. If both a multiple, the return is a list of lists
                     of properties.
   BOOL           *pfCheckedOut - Filled in with TRUE if any objects checked out
returns
   BOOL - TRUE if success, FALSE if fail (one of the objects requested is not an object)
*/
BOOL CDatabaseCat::ObjectAttribGet (PCMIFLVar pvObject, PCMIFLVar pvProp, PCMIFLVar pvResult,
                                    BOOL *pfCheckedOut)
{
   CMem memProp;
   DWORD i, j;
   CMIFLVar vUndef, vProps;
   BOOL fRet = TRUE;
   PWSTR *ppsz = NULL;
   PCMIFLVarString *papvs = NULL;
   PCMIFLVar pv = NULL;
   BOOL fObjectMulti = (pvObject->TypeGet() == MV_LIST);
   BOOL fPropMulti = (pvProp->TypeGet() == MV_LIST);
   PCMIFLVarList pvlObject = fObjectMulti ? pvObject->GetList() : NULL;
   PCMIFLVarList pvlProp = fPropMulti ? pvProp->GetList() : NULL;
   PCMIFLVarList pvlMaster = NULL;
   if (pfCheckedOut)
      *pfCheckedOut = FALSE;

   // create a list for the strings...
   DWORD dwProp = pvlProp ? pvlProp->Num() : 1;
   if (!dwProp) {
      fRet = FALSE;
      goto done;
   }
   DWORD dwNeed = dwProp * (sizeof(PWSTR) + sizeof(PCMIFLVarString) + sizeof(CMIFLVar));
   if (!memProp.Required (dwNeed))
      goto done;
   ppsz = (PWSTR*)memProp.p;
   papvs = (PCMIFLVarString*) (ppsz + dwProp);
   pv = (PCMIFLVar) (papvs + dwProp);
   for (i = 0; i < dwProp; i++) {
      papvs[i] = (pvlProp ? pvlProp->Get(i) : pvProp)->GetString(m_pVM);
      ppsz[i] = papvs[i]->Get();

      // fill with undefined at first
      memcpy (pv + i, &vUndef, sizeof(vUndef));
   } // i

   // how many objects...
   DWORD dwObject = pvlObject ? pvlObject->Num() : 1;
   if (!dwObject) {
      fRet = FALSE;
      goto done;
   }

   // if have list of objects then create list
   if (pvlObject) {
      pvlMaster = new CMIFLVarList;
      if (!pvlMaster) {
         fRet = FALSE;
         goto done;
      }
      pvResult->SetList (pvlMaster);
   }

   // loop over all the objects
   for (i = 0; i < dwObject; i++) {
      PCMIFLVar pvo = (pvlObject ? pvlObject->Get(i) : pvObject);
      if (pvo->TypeGet() != MV_OBJECT) {
         fRet = FALSE;
         goto done;
      }

      GUID g = pvo->GetGUID();

      BOOL fCheckedOut;
      if (!ObjectAttribGet (&g, dwProp, ppsz, pv, &fCheckedOut)) {
         fRet = FALSE;
         goto done;
      }
      if (pfCheckedOut)
         *pfCheckedOut = *pfCheckedOut | fCheckedOut;

      // if there's no object list and no property list, then just fill in
      if (!pvlObject && !pvlProp) {
         pvResult->Set (&pv[0]);
         continue;
      }

      // create a list for elements?
      if (pvlProp) {
         PCMIFLVarList pvl = new CMIFLVarList;
         if (!pvl) {
            fRet = FALSE;
            goto done;
         }

         for (j = 0; j < dwProp; j++)
            pvl->Add (&pv[j], TRUE);

         vProps.SetList (pvl);
         pvl->Release();
      }
      else
         vProps.Set (&pv[0]);

      // add this to the master
      if (pvlMaster)
         pvlMaster->Add (&vProps, TRUE);
      else
         pvResult->Set (&vProps);
   } // i

done:
   if (papvs)
      for (i = 0; i < dwProp; i++)
         papvs[i]->Release();
   if (pv)
      for (i = 0; i < dwProp; i++)
         pv[i].SetUndefined();

   if (pvlObject)
      pvlObject->Release();
   if (pvlProp)
      pvlProp->Release();
   if (pvlMaster)
      pvlMaster->Release();

   if (!fRet)
      pvResult->SetUndefined();

   return fRet;
}



/*************************************************************************************
CDatabaseCat::ObjectAttribSet - This sets the attribute(s) for one or more objects.
It's a MIFLVar version of ObjectAttribGet().

inputs
   PCMIFLVar      pvObject - This is either an object, or a list of objects.
   PCMIFLVar      pvProp - This is either a single property, or a list of properties.
   PCMIFLVar      pvResult - This must contain the data to set.
                     If pvObject is a single object and pvProp is a single
                     property, then pvResult is a single result.
                     
                     If prObject is multiple
                     and pvProp is single, it's a list of properties, one per object.
                     
                     If pvObject is single and pvProp is multuple, it's a list
                     of properties.
                     
                     If both a multiple, the return is a list of lists
                     of properties.
returns
   BOOL - TRUE if success, FALSE if fail (one of the objects is checked out)
*/
BOOL CDatabaseCat::ObjectAttribSet (PCMIFLVar pvObject, PCMIFLVar pvProp, PCMIFLVar pvResult)
{
   CMem memProp;
   DWORD i, j;
   CMIFLVar vUndef, vProps;
   BOOL fRet = TRUE;
   PWSTR *ppsz = NULL;
   PCMIFLVarString *papvs = NULL;
   PCMIFLVar pv = NULL;
   BOOL fObjectMulti = (pvObject->TypeGet() == MV_LIST);
   BOOL fPropMulti = (pvProp->TypeGet() == MV_LIST);
   PCMIFLVarList pvlObject = fObjectMulti ? pvObject->GetList() : NULL;
   PCMIFLVarList pvlProp = fPropMulti ? pvProp->GetList() : NULL;
   PCMIFLVarList pvlMaster = NULL;
   if (fObjectMulti) {
      pvlMaster = pvResult->GetList();
      if (!pvlMaster) {
         fRet = FALSE;
         goto done;
      }
   }

   // create a list for the strings...
   DWORD dwProp = pvlProp ? pvlProp->Num() : 1;
   if (!dwProp) {
      fRet = FALSE;
      goto done;
   }
   DWORD dwNeed = dwProp * (sizeof(PWSTR) + sizeof(PCMIFLVarString) + sizeof(CMIFLVar));
   if (!memProp.Required (dwNeed))
      goto done;
   ppsz = (PWSTR*)memProp.p;
   papvs = (PCMIFLVarString*) (ppsz + dwProp);
   pv = (PCMIFLVar) (papvs + dwProp);
   for (i = 0; i < dwProp; i++) {
      papvs[i] = (pvlProp ? pvlProp->Get(i) : pvProp)->GetString(m_pVM);
      ppsz[i] = papvs[i]->Get();

      // fill with undefined at first
      memcpy (pv + i, &vUndef, sizeof(vUndef));
   } // i

   // how many objects...
   DWORD dwObject = pvlObject ? pvlObject->Num() : 1;
   if (!dwObject) {
      fRet = FALSE;
      goto done;
   }

   // loop over all the objects
   for (i = 0; i < dwObject; i++) {
      PCMIFLVar pvo = (pvlObject ? pvlObject->Get(i) : pvObject);
      if (pvo->TypeGet() != MV_OBJECT) {
         fRet = FALSE;
         goto done;
      }

      GUID g = pvo->GetGUID();

      // attribute object
      PCMIFLVar pva = pvlMaster ? pvlMaster->Get(i) : pvResult;
      if (!pva) {
         fRet = FALSE;
         goto done;
      }
      PCMIFLVarList pvlPropList = NULL;
      if (pvlProp) {
         if (pva->TypeGet() == MV_LIST)
            pvlPropList = pva->GetList();
         if (!pvlPropList) {
            fRet = FALSE;
            goto done;
         }
      }

      // fill in values to set
      for (j = 0; j < dwProp; j++) {
         // get the specific property value
         PCMIFLVar pvProp = pvlPropList ? pvlPropList->Get(j) : pva;
         if (!pvProp) {
            if (pvlPropList)
               pvlPropList->Release();
            fRet = FALSE;
            goto done;
         }

         // copy over
         pv[j].Set (pvProp);
      } // j

      // reeleast property list
      if (pvlPropList)
         pvlPropList->Release();


      // set if
      if (!ObjectAttribSet (&g, dwProp, ppsz, pv)) {
         fRet = FALSE;
         goto done;
      }
   } // i

done:
   if (papvs)
      for (i = 0; i < dwProp; i++)
         papvs[i]->Release();
   if (pv)
      for (i = 0; i < dwProp; i++)
         pv[i].SetUndefined();

   if (pvlObject)
      pvlObject->Release();
   if (pvlProp)
      pvlProp->Release();
   if (pvlMaster)
      pvlMaster->Release();

   if (!fRet)
      pvResult->SetUndefined();

   return fRet;
}


/*************************************************************************************
CDatabaseCat::EnumFindProps - Look through the pvConst item and determine which
properties need to be loaded.

inputs
   PCMIFLVar      pvConst - List (or item)
   WCHAR          cPropDisambig - Disambiguation character
   PCHashString   phProp - Filled with the properties.
returns
   none
*/
void CDatabaseCat::EnumFindProps (PCMIFLVar pvConst, WCHAR cPropDisambig, PCHashString phProp)
{
   switch (pvConst->TypeGet()) {
   case MV_STRING:
      {
         PCMIFLVarString ps = pvConst->GetString(m_pVM);
         PWSTR psz = ps->Get();
         if (cPropDisambig) {
            if (psz[0] != cPropDisambig)
               psz = NULL; // normal string
            else
               psz++;   // skip first char
         }

         if (psz && (-1 == phProp->FindIndex (psz)))
            phProp->Add (psz, 0);

         ps->Release();
      }
      break;

   case MV_LIST:
      {
         // loop through all the sub-elements, except for 0
         PCMIFLVarList pl = pvConst->GetList();
         DWORD i;
         for (i = 1; i < pl->Num(); i++)
            EnumFindProps (pl->Get(i), cPropDisambig, phProp);

         pl->Release();
      }
      break;
   }
}



/*************************************************************************************
CDatabaseCat::EnumEval - Evaluates the list pvConst passed into ObjectEnum().

inputs
   PCMIFLVar      pvConst - Passed into ObjectEnum()
   WCHAR          cPropDisambig - Disambiguation character
   PCHashString   phProp - List of properties generated by EnumFindProps()
   PCMIFLVar      paVar - Values for the properties in phProp
   PCMIFLVar      pvResult - Filled with the result
returns
   none
*/
void CDatabaseCat::EnumEval (PCMIFLVar pvConst, WCHAR cPropDisambig, PCHashString phProp, PCMIFLVar paVar,
                             PCMIFLVar pvResult)
{
   CMIFLVar v1, v2;
   // make sure have something
   if (!pvConst) {
      pvResult->SetUndefined();
      return;
   }

   switch (pvConst->TypeGet()) {
   case MV_STRING:
      {
         PCMIFLVarString ps = pvConst->GetString(m_pVM);
         PWSTR psz = ps->Get();
         if (cPropDisambig) {
            if (psz[0] != cPropDisambig)
               psz = NULL; // normal string
            else
               psz++;   // skip first char
         }

         DWORD dwIndex = -1;
         if (psz)
            dwIndex = phProp->FindIndex (psz);
         ps->Release();

         // if it's a string then use directly, else evaluate to property
         if (dwIndex == -1)
            pvResult->Set (pvConst);
         else
            pvResult->Set (&paVar[dwIndex]);
      }
      return;

   case MV_LIST:
      // exit the switch
      break;

   default:
      // pass this up
      pvResult->Set (pvConst);
      return;
   }

   // if get here have a list
   PCMIFLVarList pl = pvConst->GetList();
   PCMIFLVarString psOper = NULL;
   DWORD dwNum = pl->Num();
   if (!dwNum)
      goto err;  // shouldnt happen
   psOper = pl->Get(0)->GetString(m_pVM);
   PWSTR psz = psOper->Get();
   DWORD dwLen = (DWORD)wcslen(psz);
   DWORD i;

   BOOL fBitwise;
   switch (psz[0]) {
   case L'-': // negation
      if (dwLen != 1)
         goto err;
      if (dwNum == 2) {
         EnumEval (pl->Get(1), cPropDisambig, phProp, paVar, pvResult);
         pvResult->OperNegation(m_pVM);
      }
      else {
         // subtraction
         pvResult->SetUndefined();
         EnumEval (pl->Get(1), cPropDisambig, phProp, paVar, &v1);
         for (i = 2; i < dwNum; i++) {
            EnumEval (pl->Get(i), cPropDisambig, phProp, paVar, &v2);
            pvResult->OperSubtract (m_pVM, &v1, &v2);
            v1.Set (pvResult);
         }
      }
      break;

   case L'~': // bitwise not
      if (dwLen != 1)
         goto err;
      EnumEval (pl->Get(1), cPropDisambig, phProp, paVar, pvResult);
      pvResult->OperBitwiseNot (m_pVM);
      break;

   case L'!': // logical not
      if (dwLen == 1) {
         EnumEval (pl->Get(1), cPropDisambig, phProp, paVar, pvResult);
         pvResult->OperLogicalNot (m_pVM);
         break;
      }
      else if ((dwLen == 2) && (psz[1] == L'=')) {
         EnumEval (pl->Get(1), cPropDisambig, phProp, paVar, &v1);
         EnumEval (pl->Get(2), cPropDisambig, phProp, paVar, &v2);
         pvResult->SetBOOL (v1.Compare (&v2, FALSE, m_pVM) != 0);
      }
      else if ((dwLen == 3) && (psz[1] == L'=') && (psz[2] == L'=')) {
         EnumEval (pl->Get(1), cPropDisambig, phProp, paVar, &v1);
         EnumEval (pl->Get(2), cPropDisambig, phProp, paVar, &v2);
         pvResult->SetBOOL (v1.Compare (&v2, TRUE, m_pVM) != 0);
      }
      else if ((dwLen == 3) && (psz[1] == L'=') && (towlower(psz[2]) == L'c')) {
         PCMIFLVarString ps1, ps2;
         PWSTR psz1, psz2;
         EnumEval (pl->Get(1), cPropDisambig, phProp, paVar, &v1);
         EnumEval (pl->Get(2), cPropDisambig, phProp, paVar, &v2);
         ps1 = v1.GetString(m_pVM);
         ps2 = v2.GetString(m_pVM);
         psz1 = ps1->Get();
         psz2 = ps2->Get();

         pvResult->SetBOOL (!(!_wcsicmp(psz1, psz2)));

         ps1->Release();
         ps2->Release();
      }
      else
         goto err;
      break;

   case L'%': // modulo
      if (dwLen != 1)
         goto err;
      EnumEval (pl->Get(1), cPropDisambig, phProp, paVar, &v1);
      EnumEval (pl->Get(2), cPropDisambig, phProp, paVar, &v2);
      pvResult->OperModulo (m_pVM, &v1, &v2);
      break;

   case L'<': // left
      // since always two params for these
      EnumEval (pl->Get(1), cPropDisambig, phProp, paVar, &v1);
      EnumEval (pl->Get(2), cPropDisambig, phProp, paVar, &v2);

      if (dwLen == 1) {
         pvResult->SetBOOL (v1.Compare (&v2, FALSE, m_pVM) < 0);
      }
      else if (dwLen == 2) {
         if (psz[1] == L'<')
            pvResult->OperBitwiseLeft (m_pVM, &v1, &v2);
         else if (psz[1] == L'=')
            pvResult->SetBOOL (v1.Compare (&v2, FALSE, m_pVM) <= 0);
         else
            goto err;
      }
      else
         goto err;
      break;

   case L'>': // right
      // since always two params for these
      EnumEval (pl->Get(1), cPropDisambig, phProp, paVar, &v1);
      EnumEval (pl->Get(2), cPropDisambig, phProp, paVar, &v2);

      if (dwLen == 1) {
         pvResult->SetBOOL (v1.Compare (&v2, FALSE, m_pVM) > 0);
      }
      else if (dwLen == 2) {
         if (psz[1] == L'<')
            pvResult->OperBitwiseRight (m_pVM, &v1, &v2);
         else if (psz[1] == L'=')
            pvResult->SetBOOL (v1.Compare (&v2, FALSE, m_pVM) >= 0);
         else
            goto err;
      }
      else
         goto err;
      break;

   case L'=': // equals
      if (psz[1] != L'=')
         goto err;

      EnumEval (pl->Get(1), cPropDisambig, phProp, paVar, &v1);
      EnumEval (pl->Get(2), cPropDisambig, phProp, paVar, &v2);
      if (dwLen == 2)
         pvResult->SetBOOL (v1.Compare (&v2, FALSE, m_pVM) == 0);
      else if ((dwLen == 3) && (psz[2] == L'='))
         pvResult->SetBOOL (v1.Compare (&v2, TRUE, m_pVM) == 0);
      else if ((dwLen == 3) && (towlower(psz[2]) == L'c')) {
         PCMIFLVarString ps1, ps2;
         PWSTR psz1, psz2;
         ps1 = v1.GetString(m_pVM);
         ps2 = v2.GetString(m_pVM);
         psz1 = ps1->Get();
         psz2 = ps2->Get();

         pvResult->SetBOOL (!_wcsicmp(psz1, psz2));

         ps1->Release();
         ps2->Release();
      }
      else
         goto err;
      break;

   case L'*': // multiplication
      if (dwLen != 1)
         goto err;
      pvResult->SetUndefined();
      EnumEval (pl->Get(1), cPropDisambig, phProp, paVar, &v1);
      for (i = 2; i < dwNum; i++) {
         EnumEval (pl->Get(i), cPropDisambig, phProp, paVar, &v2);
         pvResult->OperMultiply (m_pVM, &v1, &v2);
         v1.Set (pvResult);
      }
      break;

   case L'/': // division
      if (dwLen != 1)
         goto err;
      pvResult->SetUndefined();
      EnumEval (pl->Get(1), cPropDisambig, phProp, paVar, &v1);
      for (i = 2; i < dwNum; i++) {
         EnumEval (pl->Get(i), cPropDisambig, phProp, paVar, &v2);
         pvResult->OperDivide (m_pVM, &v1, &v2);
         v1.Set (pvResult);
      }
      break;

   case L'+': // addition
      if (dwLen != 1)
         goto err;
      pvResult->SetUndefined();
      EnumEval (pl->Get(1), cPropDisambig, phProp, paVar, &v1);
      for (i = 2; i < dwNum; i++) {
         EnumEval (pl->Get(i), cPropDisambig, phProp, paVar, &v2);
         pvResult->OperAdd (m_pVM, &v1, &v2);
         v1.Set (pvResult);
      }
      break;

   case L'&': // bitwise or logical and
      if (dwLen == 1)
         fBitwise = TRUE;
      else if ((dwLen == 2) && (psz[1] == L'&'))
         fBitwise = FALSE;
      else
         goto err;

      pvResult->SetUndefined();
      EnumEval (pl->Get(1), cPropDisambig, phProp, paVar, &v1);
      for (i = 2; i < dwNum; i++) {
         EnumEval (pl->Get(i), cPropDisambig, phProp, paVar, &v2);
         if (fBitwise)
            pvResult->OperBitwiseAnd (m_pVM, &v1, &v2);
         else
            pvResult->OperLogicalAnd (m_pVM, &v1, &v2);
         v1.Set (pvResult);
      }
      break;

   case L'|': // bitwise or logical or
      if (dwLen == 1)
         fBitwise = TRUE;
      else if ((dwLen == 2) && (psz[1] == L'|'))
         fBitwise = FALSE;
      else
         goto err;

      pvResult->SetUndefined();
      EnumEval (pl->Get(1), cPropDisambig, phProp, paVar, &v1);
      for (i = 2; i < dwNum; i++) {
         EnumEval (pl->Get(i), cPropDisambig, phProp, paVar, &v2);
         if (fBitwise)
            pvResult->OperBitwiseOr (m_pVM, &v1, &v2);
         else
            pvResult->OperLogicalOr (m_pVM, &v1, &v2);
         v1.Set (pvResult);
      }
      break;

   case L'^': // bitwise xor
      if (dwLen != 1)
         goto err;

      pvResult->SetUndefined();
      EnumEval (pl->Get(1), cPropDisambig, phProp, paVar, &v1);
      for (i = 2; i < dwNum; i++) {
         EnumEval (pl->Get(i), cPropDisambig, phProp, paVar, &v2);
         pvResult->OperBitwiseXOr (m_pVM, &v1, &v2);
         v1.Set (pvResult);
      }
      break;

   case L'c':
   case L'C':
      {
         BOOL fCaseSens;
         if (!_wcsicmp(psz, L"contains"))
            fCaseSens = TRUE;
         else if (!_wcsicmp(psz, L"containsc"))
            fCaseSens = FALSE;
         else
            goto err;

         PCMIFLVarString ps1, ps2;
         PWSTR psz1, psz2;
         EnumEval (pl->Get(1), cPropDisambig, phProp, paVar, &v1);
         EnumEval (pl->Get(2), cPropDisambig, phProp, paVar, &v2);
         ps1 = v1.GetString(m_pVM);
         ps2 = v2.GetString(m_pVM);
         psz1 = ps1->Get();
         psz2 = ps2->Get();

         PWSTR pszFind;
         if (fCaseSens)
            pszFind = wcsstr (psz1, psz2);
         else
            pszFind = (PWSTR) MyStrIStr (psz1, psz2);

         ps1->Release();
         ps2->Release();

         pvResult->SetBOOL (pszFind ? TRUE : FALSE);
      }
      break;

   default:
      goto err;
   } // switch

   if (psOper)
      psOper->Release();
   pl->Release();
   return;

err:
   pvResult->SetUndefined();
   if (psOper)
      psOper->Release();
   pl->Release();
   return;
}

   

/*************************************************************************************
CDatabaseCat::ObjectEnum - Looks through all the objects and enumerates the
ones that match the given criteria.


inputs
   PCMIFLVar      pvConst - Only pass through objects that meet these
                     constraints. See below.
   WCHAR          cPropDisambig - If this character appears at the start of a string
                  in pvConst (of sublists) then the entry gets the value of the
                  property in the database. If the string starts with any other
                  character then the string is used. If pvConst == 0, then the
                  property name is always used.
                  Ex: If cPropDisambig = '|' then "|Name" refers to the "Name"
                  property, and "Name" is just a string.
   PCMIFLVar      pvResult - Filled in with the results (in a list), or FALSE if error.
                     The list is a list of objects.
returns
   BOOL - TRUE if success, FALSE if error

DOCUMENT: Need to document the parameters for the search.
If pvConst is NULL then no constraints. If pvConst is
a list, the first element is a string indicating what kind of comparison to
make, while the next parameters are operands. The operands can often be
lists in themselves, which indicate more constraints.

The following contraints are possible:

Single operand: Elem 1 is either an attribute string that's gotten and negated, or another list.
"-" - Negation.
"~" - Bitwise not.
"!" - Loglical not.

Two operands:
"%" - Modulo. Two elements, either attribute as string or list.
"<<" - Bitwise left
">>" - Bitwise right
"<" - Less than
"<=" - Less than equals
">" - Greater than
">=" - Greater than equals
"==" - Equality
"!=" - Not equal
"===" - Strict equality
"!==" - Not equal, strict equality
"==c" - Case insentative equals
"!=c" - Case insentative not-equals
"contains" - If element 2's string contained in element 1
"containsc" - Case insensative contains

Any number of operands: Any number of elements. They's attribute strings or sub-lists.
"*" - Multiplication.
"/" - Division
"+" - Addition
"-" - Subtraction
"&" - Bitwise and
"^" - BItwise xor
"|" - Bitwise or
"&&" - Logical and
"||" - Logical or

Disambiguation of a string vs. property name: Property names are assumed
to start with the character, cPropDisambig. If it doesn't it's assumed
to be a string.

*/
BOOL CDatabaseCat::ObjectEnum (PCMIFLVar pvConst, WCHAR cPropDisambig, PCMIFLVar pvResult)
{
   // figure out what properties are used
   CHashString hProp;
   hProp.Init (0);
   EnumFindProps (pvConst, cPropDisambig, &hProp);

   // create a list of this
   CMem mem;
   CMIFLVar vUndef, vResult;
   DWORD i;
   BOOL fRet= TRUE;
   DWORD dwProp = hProp.Num();
   DWORD dwNeed = dwProp * (sizeof(PWSTR) + sizeof(CMIFLVar));
   if (!mem.Required (dwNeed)) {
      pvResult->SetBOOL (FALSE);
      return FALSE;  // error
   }
   PWSTR *ppsz = (PWSTR*)mem.p;
   PCMIFLVar pv = (PCMIFLVar) (ppsz + dwProp);
   for (i = 0; i < dwProp; i++) {
      ppsz[i] = hProp.GetString (i);
      memcpy (pv + i, &vUndef, sizeof(vUndef));
   } // i

   // loop through all the objects...
   BOOL fIsNull = (pvConst->TypeGet() == MV_NULL);
   PCMIFLVarList pl = new CMIFLVarList;
   if (!pl) {
      pvResult->SetBOOL (FALSE);
      return FALSE;  // error
   }
   for (i = 0; i < m_hDBITEM.Num(); i++) {
      PDBITEM pdi = (PDBITEM)m_hDBITEM.Get(i);

      if (dwProp)
         if (!ObjectAttribGet (&pdi->gID, dwProp, ppsz, pv, NULL))
            continue;   // some sort of error, shouldnt happen

      // do comparisons
      if (!fIsNull) {
         EnumEval (pvConst, cPropDisambig, &hProp, pv, &vResult);
         if (!vResult.GetBOOL(m_pVM))
            continue;   // not accepted
      }

      // else, add
      vResult.SetObject (&pdi->gID);
      pl->Add (&vResult, TRUE);
   } // i

   pvResult->SetList (pl);
   pl->Release();

   // when done, free up all the values
   for (i = 0; i < dwProp; i++)
      pv[i].SetUndefined();

   return fRet;
}



   

/*************************************************************************************
CDatabaseCat::ObjectCheckOut - Returns a list of all the objects checked out.


inputs
   PCMIFLVar      pvResult - Filled in with the results (in a list), or FALSE if error.
                     The list is a list of objects.
returns
   BOOL - TRUE if success, FALSE if error
*/
BOOL CDatabaseCat::ObjectEnumCheckOut (PCMIFLVar pvResult)
{
   BOOL fRet = TRUE;

   // loop through all the objects...
   PCMIFLVarList pl = new CMIFLVarList;
   DWORD i;
   CMIFLVar vResult;
   if (!pl) {
      pvResult->SetBOOL (FALSE);
      return FALSE;  // error
   }
   for (i = 0; i < m_hDBITEM.Num(); i++) {
      PDBITEM pdi = (PDBITEM)m_hDBITEM.Get(i);

      if (!pdi->dwCheckedOut)
         continue;

      // else, add
      vResult.SetObject (&pdi->gID);
      pl->Add (&vResult, TRUE);
   } // i

   pvResult->SetList (pl);
   pl->Release();

   return fRet;
}



/*************************************************************************************
CDatabaseCat::ObjectNum - Returns the number of objects checked out.


inputs
   BOOL        fCheckOutOnly - If TRUE return only the checked out number
returns
   DWORD - NUmber of objects checked out
*/
DWORD CDatabaseCat::ObjectNum (BOOL fCheckOutOnly)
{
   return fCheckOutOnly ? m_dwCheckedOut : m_hDBITEM.Num();
}


/*************************************************************************************
CDatabaseCat::ObjectGet - Gets the N'th object. If fCheckOutOnly then this
   is the Nth checked out object


inputs
   DWORD    dwIndex - Index to get
   BOOL     fCheckOutOnly - If TRUE the index is looking through checked out objects,
               as opposed to all objects
   GUID     *pgID - Filled with the GUID of the object if success
returns
   BOOL - TRUE if success, FALSE if not
*/
BOOL CDatabaseCat::ObjectGet (DWORD dwIndex, BOOL fCheckOutOnly, GUID *pgID)
{
   if (!fCheckOutOnly) {
      PDBITEM pdi = (PDBITEM)m_hDBITEM.Get(dwIndex);
      if (!pdi)
         return FALSE;

      *pgID = pdi->gID;
      return TRUE;
   }

   // else, only look for chedk out items
   DWORD i;
   for (i = 0; i < m_hDBITEM.Num(); i++) {
      PDBITEM pdi = (PDBITEM)m_hDBITEM.Get(i);

      if (!pdi->dwCheckedOut)
         continue;
      
      if (!dwIndex) {
         *pgID = pdi->gID;
         return TRUE;
      }

      dwIndex--;
   }

   // if get here can't find
   return FALSE;

}




/*************************************************************************************
CDatabase::Constructor and destructor
*/
CDatabase::CDatabase (void)
{
   m_szDir[0] = 0;
   m_pVM = NULL;
   m_hPCDatabaseCat.Init (sizeof(PCDatabaseCat));
}

CDatabase::~CDatabase (void)
{
   // free up all the databased
   DWORD i;
   for (i = 0; i < m_hPCDatabaseCat.Num(); i++) {
      PCDatabaseCat pdc = *((PCDatabaseCat*)m_hPCDatabaseCat.Get(i));
      delete pdc;
   } // i
}


/*************************************************************************************
CDatabase::Init - This initializes the database object.

inputs
   PWSTR          pszDir - Directory to store any database files in. This INCLUDES
                     the final '\', such as "c:\hello\"
   PCMIFLVM       pVM - VM to use
returns
   BOOL - TRUE if success
*/
BOOL CDatabase::Init (PWSTR pszDir, PCMIFLVM pVM)
{
   if (m_pVM || m_szDir[0])
      return FALSE;

   wcscpy (m_szDir, pszDir);
   m_pVM = pVM;

   // create the directory if it doesn't already exist
   char szTemp[512];
   WideCharToMultiByte (CP_ACP, 0, m_szDir, -1, szTemp, sizeof(szTemp), 0,0);
   DWORD dwLen = (DWORD)strlen(szTemp);
   if (dwLen && (szTemp[dwLen-1] == '\\'))
      szTemp[dwLen-1] = 0;
   CreateDirectory (szTemp, NULL);

   return TRUE;
}




/*************************************************************************************
CDatabase::Get - Gets a CDatabaseCat object based on the category. (This category
is translated into a file name.

inputs
   PWSTR       pszCategory - Category name. This must be a valid name, passing
                     MIFLIsNameValid() - all alphanum or underscores, can't start with number
returns
   PCDatabaseCat - Database. Use this, but don't delete it. NULL if can't create
*/
PCDatabaseCat CDatabase::Get (PWSTR pszCategory)
{
   // see if it's already there
   PCDatabaseCat *ppdb = (PCDatabaseCat*)m_hPCDatabaseCat.Find (pszCategory);
   if (ppdb)
      return *ppdb;


   // else, need to create
   if (!MIFLIsNameValid(pszCategory))
      return NULL;

   // make sure name not too long
   WCHAR szTemp[256];
   DWORD dwLen = (DWORD)wcslen(m_szDir) + (DWORD)wcslen(pszCategory) + 5;
   if (dwLen >= sizeof(szTemp)/sizeof(WCHAR))
      return NULL;
   wcscpy (szTemp, m_szDir);
   wcscat (szTemp, pszCategory);
   wcscat (szTemp, L".mdb");

   PCDatabaseCat pdb;
   pdb = new CDatabaseCat;
   if (!pdb)
      return NULL;
   if (!pdb->Init (pszCategory, szTemp, m_pVM)) {
      delete pdb;
      return NULL;
   }

   // else, works
   m_hPCDatabaseCat.Add (pszCategory, &pdb);
   return pdb;
}


/*************************************************************************************
CDatabase::ObjectQueryCheckOut - Sees if the object is checked out by any of the
databases.
inputs
   GUID        *pgID - Object ID to check out
   BOOL        fTestForOtherVM - If TRUE this tests to see if its checekd out
               on another VM. (Slower)
returns
   PWSTR - Database name, or NULL if not checked out
*/
PWSTR CDatabase::ObjectQueryCheckOut (GUID *pgID, BOOL fTestForOtherVM)
{
   DWORD i;
   int iRet;
   for (i = 0; i < m_hPCDatabaseCat.Num(); i++) {
      PCDatabaseCat pdc = *((PCDatabaseCat*)m_hPCDatabaseCat.Get(i));

      iRet = pdc->ObjectQueryCheckOut (pgID, fTestForOtherVM);
      if (iRet < 0)
         continue;   // didn't find it

      // else found
      return m_hPCDatabaseCat.GetString (i);
   }

   // else didn't find
   return NULL;
}

/*************************************************************************************
CDatabase::SaveAll - Saves all the data cached by the databased.

returns
   BOOL - TRUE if success
*/
BOOL CDatabase::SaveAll (void)
{
   DWORD i;
   for (i = 0; i < m_hPCDatabaseCat.Num(); i++) {
      PCDatabaseCat pdc = *((PCDatabaseCat*)m_hPCDatabaseCat.Get(i));
      if (!pdc->SaveAll ())
         return FALSE;
   } // i

   return TRUE;
}

/*************************************************************************************
CDatabase::SavePartial - This is used for a piece-meal save that will
save to disk slowly over time. What this does is:
- One out of 1024 calls, this saves the full index
- Increments through all the objects and tries saving one each go

inputs
   DWORD       dwNumber - This is a value that should increate by 1 every
                     time it's called
returns
   BOOL - TRUE if the save was usccessful
*/
BOOL CDatabase::SavePartial (DWORD dwNumber)
{
   DWORD dwNum = m_hPCDatabaseCat.Num();
   if (!dwNum)
      return TRUE;   // nothing to save

   // alternate, saving different bits
   PCDatabaseCat pdc = *((PCDatabaseCat*)m_hPCDatabaseCat.Get(dwNumber % dwNum));

   return pdc->SavePartial (dwNumber / dwNum);
}


/*************************************************************************************
CDatabase::SaveBackup -  This saves all then copies all of the database files to
a backup directory.

inputs
   PWSTR          pszDir - Backup directory. This INCLUDES the final '\', such
                     as "c:\test\".
returns
   BOOL - TRUE if success
*/
BOOL CDatabase::SaveBackup (PWSTR pszDir)
{
   if (!SaveAll())
      return FALSE;

   // create the directory if it doesn't already exist
   char szOrigDir[512], szNewDir[512];
   WideCharToMultiByte (CP_ACP, 0, m_szDir, -1, szOrigDir, sizeof(szOrigDir), 0,0);
   WideCharToMultiByte (CP_ACP, 0, pszDir, -1, szNewDir, sizeof(szNewDir), 0,0);
   DWORD dwLen = (DWORD)strlen(szNewDir);
   if (dwLen && (szNewDir[dwLen-1] == '\\'))
      szNewDir[dwLen-1] = 0;
   CreateDirectory (szNewDir, NULL);
   strcat (szNewDir, "\\");

   DWORD i;
   char szOrig[512], szNew[1024];
   for (i = 0; i < m_hPCDatabaseCat.Num(); i++) {
      PCDatabaseCat pdc = *((PCDatabaseCat*)m_hPCDatabaseCat.Get(i));

      // original file
      strcpy (szOrig, szOrigDir);
      dwLen = (DWORD)strlen(szOrig);
      WideCharToMultiByte (CP_ACP, 0, pdc->m_szName, -1, szOrig+dwLen, sizeof(szOrig)-dwLen, 0, 0);
      strcat (szOrig, ".mdb");

      // new file
      strcpy (szNew, szNewDir);
      dwLen = (DWORD)strlen(szNew);
      WideCharToMultiByte (CP_ACP, 0, pdc->m_szName, -1, szNew+dwLen, sizeof(szNew)-dwLen, 0, 0);
      strcat (szNew, ".mdb");

      // copy
      if (!CopyFile (szOrig, szNew, FALSE))
         return FALSE;
   } // i

   return TRUE;
}


/*************************************************************************************
CDatabase::ObjectDelete - Called when the object has been deleted by the VM (except
for shutdown), and it should therefore be deleted from the database.

inputs
   GUID           *pgID - Object ID
   BOOL           fOnlyIfCheckedOut - If TRUE, only delete the object if it's checked out
returns
   BOOL - TRUE if found, FALSE if not
*/
BOOL CDatabase::ObjectDelete (GUID *pgID, BOOL fOnlyIfCheckedOut)
{
   DWORD i;
   int iRet;
   for (i = 0; i < m_hPCDatabaseCat.Num(); i++) {
      PCDatabaseCat pdc = *((PCDatabaseCat*)m_hPCDatabaseCat.Get(i));

      if (fOnlyIfCheckedOut) {
         iRet = pdc->ObjectQueryCheckOut (pgID, FALSE);
         if (iRet < 0)
            continue;   // didn't find it
         if (iRet != 2)
            return FALSE;  // exists, but not checked out

         // else, it's checked out, so delete
      }

      if (pdc->ObjectDelete (pgID))
         return TRUE;
   }

   // else didn't find
   return FALSE;
}


// BUGBUG - CDatabaseCat refers to m_pVM. It may be better to pass m_pVM
// into all the function calls so that the database can be called by
// multiple shards.
