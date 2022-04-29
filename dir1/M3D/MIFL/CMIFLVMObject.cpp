/*************************************************************************************
CMIFLVMObject.cpp - Code for handling an object in a virtual machine.

begun 30/1/04 by Mike Rozak.
Copyright 2004 by Mike Rozak. All rights reserved
*/

#include <windows.h>
#include "escarpment.h"
#include "..\M3D.h"
#include "resource.h"
#include "..\mifl.h"





/**********************************************************************************
CMIFLVMTimer::Constructor and destructor
*/
CMIFLVMTimer::CMIFLVMTimer (void)
{
   m_fRepeating = TRUE;
   m_fTimeRepeat = m_fTimeLeft = m_fTimeScale = 1;
   m_gBelongsTo = m_gCall = GUID_NULL;
   m_dwCallID = 0;
   // variables already set
}

CMIFLVMTimer::~CMIFLVMTimer (void)
{
   // do nothing for now
}

/**********************************************************************************
CMIFLVMTimer::CloneTo - Clones to a new timer.
*/
BOOL CMIFLVMTimer::CloneTo (PCMIFLVMTimer pTo)
{
   pTo->m_fRepeating = m_fRepeating;
   pTo->m_fTimeRepeat = m_fTimeRepeat;
   pTo->m_fTimeLeft = m_fTimeLeft;
   pTo->m_fTimeScale = m_fTimeScale;
   pTo->m_gBelongsTo = m_gBelongsTo;
   pTo->m_gCall = m_gCall;
   pTo->m_dwCallID = m_dwCallID;
   pTo->m_varName.Set (&m_varName);
   pTo->m_varName.Fracture();
   pTo->m_varParams.Set (&m_varParams);
   pTo->m_varParams.Fracture();

   return TRUE;
}


/**********************************************************************************
CMIFLVMTimer::Clone - Standard clone
*/
PCMIFLVMTimer CMIFLVMTimer::Clone (void)
{
   PCMIFLVMTimer pNew = new CMIFLVMTimer;
   if (!pNew)
      return NULL;

   if (!CloneTo (pNew)) {
      delete pNew;
      return NULL;
   }

   return pNew;

}

/**********************************************************************************
CMIFLVMTimer::MMLTo - Writes the timer to a MMLNode

NOTE: This doesnt write the object it's assigned to because that's implied.

inputs
   PCMIFLVM          pVM - Virtual machine
   PCHashPVOID       phString - Hash of string pointer to ID. 0-length. Will be added to.
   PCHashPVoid       phList - Hash of list pointer to ID. 0-lenght. Will be added to
returns
   PCMMLNode2 - Node
*/
static PWSTR gpszVMTimer = L"VMTimer";
static PWSTR gpszRepeating = L"Repeating";
static PWSTR gpszTimeRepeat = L"TimeRepeat";
static PWSTR gpszTimeLeft = L"TimeLeft";
static PWSTR gpszCall = L"Call";
static PWSTR gpszCallID = L"CallID";
static PWSTR gpszName = L"Name";
static PWSTR gpszParams = L"Params";
static PWSTR gpszTimeScale = L"TimeScale";

PCMMLNode2 CMIFLVMTimer::MMLTo (PCMIFLVM pVM, PCHashPVOID phString, PCHashPVOID phList)
{
   PCMMLNode2 pNode = new CMMLNode2;
   if (!pNode)
      return NULL;
   pNode->NameSet (gpszVMTimer);

   MMLValueSet (pNode, gpszRepeating, (int)m_fRepeating);
   MMLValueSet (pNode, gpszTimeRepeat, (fp)m_fTimeRepeat);
   MMLValueSet (pNode, gpszTimeLeft, (fp)m_fTimeLeft);
   if (m_fTimeScale != 1.0)
      MMLValueSet (pNode, gpszTimeScale, (fp)m_fTimeScale);

   // BUGFIX - Store the timer pointer away, not the specific call ID
   //MMLValueSet (pNode, gpszCall, (PBYTE)&m_gCall, sizeof(m_gCall));
   //MMLValueSet (pNode, gpszCallID, (int)m_dwCallID); bugbug
   CMIFLVar v;
   PCMMLNode2 pSub;
   if (IsEqualGUID (m_gCall, GUID_NULL))
      v.SetFunc (m_dwCallID);
   else
      v.SetObjectMeth (&m_gCall, m_dwCallID);
   pSub = v.MMLTo (pVM, phString, phList);
   if (pSub) {
      pSub->NameSet (gpszCall);
      pNode->ContentAdd (pSub);
   }

   pSub = m_varName.MMLTo (pVM, phString, phList);
   if (pSub) {
      pSub->NameSet (gpszName);
      pNode->ContentAdd (pSub);
   }
   pSub = m_varParams.MMLTo (pVM, phString, phList);
   if (pSub) {
      pSub->NameSet (gpszParams);
      pNode->ContentAdd (pSub);
   }

   return pNode;
}

/**********************************************************************************
CMIFLVMTimer::MMLFrom - Reads the timer from MMLFrom.

inputs
   PCMMLNode2         pNode - Node
   GUID              *pgObject - Object that this assocates with
   PCMIFLVM          pVM - VM
   PCHashDWORD       phString - Hash of string pointer to ID. Each element is
                     a PCMIFLVarString.
   PCHashDWORD       phList - Hash of list pointer to ID. Each element is a pointer
                     to a PCMIFLVarString
   PCHashGUID        phObjectRemap - Hash is of original object ID. Contents are GUID of new ID
returns
   DWORD - TRUEif success
*/
BOOL CMIFLVMTimer::MMLFrom (PCMMLNode2 pNode, GUID *pgObject, PCMIFLVM pVM, PCHashDWORD phString, PCHashDWORD phList,
                            PCHashGUID phObjectRemap)
{
   m_gBelongsTo = *pgObject;

   m_fRepeating = (BOOL) MMLValueGetInt (pNode, gpszRepeating, (int)0);
   m_fTimeRepeat = MMLValueGetDouble (pNode, gpszTimeRepeat, 1);
   m_fTimeLeft = MMLValueGetDouble (pNode, gpszTimeLeft, 1);
   m_fTimeScale = MMLValueGetDouble (pNode, gpszTimeScale, 1.0);


   // BUGFIX - Store the timer pointer away, not the specific call ID
   //MMLValueSet (pNode, gpszCall, (PBYTE)&m_gCall, sizeof(m_gCall));
   //MMLValueSet (pNode, gpszCallID, (int)m_dwCallID); bugbug
   CMIFLVar v;
   PCMMLNode2 pSub;
   PWSTR psz;
   pNode->ContentEnum (pNode->ContentFind (gpszCall), &psz, &pSub);
   if (!pSub || !v.MMLFrom(pSub, pVM, phString, phList, phObjectRemap))
      return FALSE;
   switch (v.TypeGet()) {
   case MV_FUNC:
      m_gCall = GUID_NULL;
      m_dwCallID = v.GetValue ();
      break;
   case MV_OBJECTMETH:
      m_gCall = v.GetGUID();
      m_dwCallID = v.GetValue ();
      break; // BUGFIX - forgot the break, so tiemrs didnt load properly
   default:
      return FALSE;
   } // switch
   // old code, which broke if callID was changed
   //if (sizeof(m_gCall) != MMLValueGetBinary (pNode, gpszCall, (PBYTE)&m_gCall, sizeof(m_gCall)))
   //   m_gCall = GUID_NULL;
   //GUID *pg = (GUID*) phObjectRemap->Find (&m_gCall);
   //if (pg)
   //   m_gCall = *pg;
   //m_dwCallID = (DWORD) MMLValueGetInt (pNode, gpszCallID, 0); bugbug

   pSub = NULL;
   pNode->ContentEnum (pNode->ContentFind (gpszName), &psz, &pSub);
   if (!pSub || !m_varName.MMLFrom(pSub, pVM, phString, phList, phObjectRemap)) {
      m_varName.SetUndefined();
      return FALSE;
   }
   
   pSub = NULL;
   pNode->ContentEnum (pNode->ContentFind (gpszParams), &psz, &pSub);
   if (!pSub || !m_varParams.MMLFrom(pSub, pVM, phString, phList, phObjectRemap)) {
      m_varParams.SetUndefined();
      return FALSE;
   }

   return TRUE;
}




/**********************************************************************************
CMIFLVMLayer::Constructor and destructor
*/
CMIFLVMLayer::CMIFLVMLayer (void)
{
   MemZero (&m_memName);
   m_fRank = 0;
   m_pObject = NULL;
   m_hPropGetSet.Init (sizeof(MIFLGETSET));
   m_hMeth.Init (sizeof(MIFLIDENT));
}

CMIFLVMLayer::~CMIFLVMLayer (void)
{
   // do nothing for now
}


/**********************************************************************************
CMIFLVMLayer::Clone - Clones a layer

returns
   PCMIFLVMLayer - clone
*/
CMIFLVMLayer *CMIFLVMLayer::Clone (void)
{
   PCMIFLVMLayer pNew = new CMIFLVMLayer;
   if (!pNew)
      return NULL;

   MemCat (&pNew->m_memName, (PWSTR)m_memName.p);
   pNew->m_fRank = m_fRank;
   pNew->m_pObject = m_pObject;

   m_hPropGetSet.CloneTo (&pNew->m_hPropGetSet);
   m_hMeth.CloneTo (&pNew->m_hMeth);

   return pNew;
}


/**********************************************************************************
CMIFLVMLayer::MMLTo - Writes the timer to a MMLNode

inputs
   PCMIFLVM          pVM - Virtual machine
   PCHashPVOID       phString - Hash of string pointer to ID. 0-length. Will be added to.
   PCHashPVoid       phList - Hash of list pointer to ID. 0-lenght. Will be added to
returns
   PCMMLNode2 - Node
*/
static PWSTR gpszVMLayer = L"VMLayer";
static PWSTR gpszRank = L"Rank";
static PWSTR gpszObject = L"Object";
static PWSTR gpszGet = L"Get";
static PWSTR gpszSet = L"Set";
static PWSTR gpszPropGetSet = L"PropGetSet";
static PWSTR gpszMeth = L"Meth";
static PWSTR gpszCode = L"Code";

PCMMLNode2 CMIFLVMLayer::MMLTo (PCMIFLVM pVM, PCHashPVOID phString, PCHashPVOID phList)
{
   PCMMLNode2 pNode = new CMMLNode2;
   if (!pNode)
      return NULL;
   pNode->NameSet (gpszVMLayer);

   PWSTR psz;
   psz = (PWSTR)m_memName.p;
   if (psz)
      MMLValueSet (pNode, gpszName, psz);
   MMLValueSet (pNode, gpszRank, (fp)m_fRank);
   psz = (PWSTR)m_pObject->m_memName.p;
   if (psz)
      MMLValueSet (pNode, gpszObject, psz);

   // write where the property get/set code comes from
   DWORD i;
   PCMMLNode2 pSub, pSub2;
   for (i = 0; i < m_hPropGetSet.Num(); i++) {
      PMIFLGETSET pgs = (PMIFLGETSET)m_hPropGetSet.Get(i);

      pSub = new CMMLNode2;
      if (!pSub) {
         delete pNode;
         return NULL;
      }
      pSub->NameSet (gpszPropGetSet);

      PWSTR psz = NULL;
      if (pgs->dwID >= VM_CUSTOMIDRANGE) {
         DWORD dwIndex = pVM->m_hUnIdentifiersCustomProperty.FindIndex (pgs->dwID);
         psz = pVM->m_hIdentifiersCustomProperty.GetString (dwIndex);
      }
      else {
         PMIFLIDENT pmi = (PMIFLIDENT) pVM->m_pCompiled->m_hUnIdentifiers.Find (pgs->dwID);
         if (!pmi || (pmi->dwType != MIFLI_PROPDEF))
            continue;

         psz = (PWSTR) ((PCMIFLProp)pmi->pEntity)->m_memName.p;
      }
      if (!psz || !psz[0]) {
         delete pNode;
         return NULL;
      }

      MMLValueSet (pSub, gpszName, psz);
      pSub2 = pgs->m_pCodeGet ? CodeGetSetMMLTo (pgs->m_pCodeGet) : NULL;
      if (pSub2) {
         pSub2->NameSet (gpszGet);
         pSub->ContentAdd (pSub2);
      }
      pSub2 = pgs->m_pCodeSet ? CodeGetSetMMLTo (pgs->m_pCodeSet) : NULL;
      if (pSub2) {
         pSub2->NameSet (gpszSet);
         pSub->ContentAdd (pSub2);
      }

      pNode->ContentAdd (pSub);
   } // i, all new properties

   // write the method IDs...
   CMIFLVar var;
   for (i = 0; i < m_hMeth.Num(); i++) {
      PMIFLIDENT pmi = (PMIFLIDENT)m_hMeth.Get(i);

      pSub = new CMMLNode2;
      if (!pSub) {
         delete pNode;
         return NULL;
      }
      pSub->NameSet (gpszMeth);

      // get the name
      var.SetMeth (pmi->dwID);
      PCMIFLVarString ps = var.GetString(pVM);
      PWSTR psz = ps->Get();
      if (psz && psz[0])
         MMLValueSet (pSub, gpszName, psz);
      ps->Release();

      // get the code
      PCMIFLFunc pFunc = (PCMIFLFunc)pmi->pEntity;
      pSub2 = CodeGetSetMMLTo (&pFunc->m_Code);
      if (pSub2) {
         pSub2->NameSet (gpszCode);
         pSub->ContentAdd (pSub2);
      }

      pNode->ContentAdd (pSub);
   } // i, all methods

   return pNode;
}


/**********************************************************************************
CMIFLVMLayer::MMLFrom - Initializes the object to create a copy of the
object specified by pObject.

NOTE: This takes the places of the InitXXX() call

inputs
   PCMMLNode2            pNode - Node to get from
   PCMIFLVM             pVM - Virtual machine to use for the object
   PCHashDWORD       phString - Hash of string pointer to ID. Each element is
                     a PCMIFLVarString.
   PCHashDWORD       phList - Hash of list pointer to ID. Each element is a pointer
                     to a PCMIFLVarString
returns
   BOOL - TRUE if success
*/
BOOL CMIFLVMLayer::MMLFrom (PCMMLNode2 pNode, PCMIFLVM pVM, PCHashDWORD phString, PCHashDWORD phToList)
{
   PWSTR psz;
   psz = MMLValueGet (pNode, gpszName);
   if (!psz)
      return FALSE;
   MemZero (&m_memName);
   MemCat (&m_memName, psz);

   // object
   psz = MMLValueGet (pNode, gpszObject);
   PMIFLIDENT pmi = psz ? (PMIFLIDENT)pVM->m_pCompiled->m_hIdentifiers.Find(psz, TRUE) : NULL;
   if (!pmi || (pmi->dwType != MIFLI_OBJECT))
      return FALSE;
   m_pObject = (PCMIFLObject)pmi->pEntity;

   // rank
   m_fRank = MMLValueGetDouble (pNode, gpszRank, 0);

   // loop through other elements looking for property get/set, etc.
   DWORD i;
   PCMMLNode2 pSub, pSub2;
   for (i = 0; i < pNode->ContentNum(); i++) {
      pSub = NULL;
      pNode->ContentEnum (i, &psz, &pSub);
      if (!pSub)
         continue;
      psz = pSub->NameGet();
      if (!psz)
         continue;

      if (!_wcsicmp(psz,gpszPropGetSet)) {
         MIFLGETSET gs;
         memset (&gs, 0, sizeof(gs));

         psz = MMLValueGet (pSub, gpszName);
         if (!psz)
            return FALSE;   // error
         gs.dwID = pVM->ToPropertyID (psz, TRUE);
         if (gs.dwID == -1)
            return FALSE;   // error

         // get code
         pSub2 = NULL;
         pSub->ContentEnum (pSub->ContentFind (gpszGet), &psz, &pSub2);
         if (pSub2)
            gs.m_pCodeGet = (PCMIFLCode) CodeGetSetMMLFrom (pSub2, pVM, FALSE);

         // set code
         pSub2 = NULL;
         pSub->ContentEnum (pSub->ContentFind (gpszSet), &psz, &pSub2);
         if (pSub2)
            gs.m_pCodeSet = (PCMIFLCode) CodeGetSetMMLFrom (pSub2, pVM, FALSE);

         m_hPropGetSet.Add (gs.dwID, &gs);
         continue;
      }
      else if (!_wcsicmp(psz,gpszMeth)) {
         MIFLIDENT mi;
         memset (&mi, 0, sizeof(mi));
         mi.dwType = MIFLI_METHDEF;

         psz = MMLValueGet (pSub, gpszName);
         if (!psz)
            return FALSE;   // error
         mi.dwID = pVM->ToMethodID (psz, NULL, TRUE);
            // NOTE: Passing in NULL for private ID, but I think this is OK
         if (mi.dwID == -1)
            return FALSE;   // error

         // get the code
         pSub2 = NULL;
         pSub->ContentEnum (pSub->ContentFind (gpszCode), &psz, &pSub2);
         if (pSub2)
            mi.pEntity = (PCMIFLFunc) CodeGetSetMMLFrom (pSub2, pVM, TRUE);
         if (!mi.pEntity)
            continue;   // skip error

         m_hMeth.Add (mi.dwID, &mi);
         continue;
      }
   } // i , all content

   return TRUE;
}



/**********************************************************************************
CMIFLVMObject::Constructor and destructor
*/
CMIFLVMObject::CMIFLVMObject (void)
{
   memset (&m_gID, 0, sizeof(m_gID));
   //m_pObject = NULL;
   m_lPCMIFLVMLayer.Init (sizeof(PCMIFLVMLayer));
   m_hProp.Init (sizeof(CMIFLVarProp));
   m_hMeth.Init (sizeof(MIFLIDENT));

   memset (&m_gContainedIn, 0, sizeof(m_gContainedIn));
   m_lContains.Init (sizeof(GUID));

   m_fTimerSuspended = FALSE;
   m_fTimerTimeScale = 1.0;
   m_dwTimerNum = 0;
   m_lPCMIFLVMTimer.Init (sizeof(PCMIFLVMTimer));
}

CMIFLVMObject::~CMIFLVMObject (void)
{
   // free up the layers
   DWORD i;
   PCMIFLVMLayer *ppl = (PCMIFLVMLayer*)m_lPCMIFLVMLayer.Get(0);
   for (i = 0; i < m_lPCMIFLVMLayer.Num(); i++)
      delete ppl[i];

   // loop through allt he properties and free up
   for (i = 0; i < m_hProp.Num(); i++) {
      PCMIFLVarProp pv = (PCMIFLVarProp)m_hProp.Get(i);
      pv->m_Var.SetUndefined();
   }

   // free up all the suspended timers
   PCMIFLVMTimer *ppt = (PCMIFLVMTimer*)m_lPCMIFLVMTimer.Get(0);
   for (i = 0; i < m_lPCMIFLVMTimer.Num(); i++)
      delete ppt[i];
}


/**********************************************************************************
CMIFLVMObject::CleanDelete - Call this to delete the object so that it detaches
itself from all its containers and frees up all its timers in the main VM. If
you just use the normal delete XXX then the attached containers and timers will
still be left around, which is OK for a total shutdown, but not for deleting the
object while the VM continues
*/
void CMIFLVMObject::CleanDelete (void)
{
   // need to suspend timers
   TimerSuspendSet (0.0, FALSE);

   ContainDisconnectAll();
   delete this;
}



/**********************************************************************************
CMIFLVMObject::InitAsAutoCreate - Initializes the object to resemble the an
object that is specified by the CMIFLObject::m_fAutoCreate parameter. This
will assume the object's GUID and other defaults.

inputs
   PCMIFLVM             pVM - Virtual machine to use for the object
   PCMIFLObject         pObject - Object
*/
BOOL CMIFLVMObject::InitAsAutoCreate (PCMIFLVM pVM, PCMIFLObject pObject)
{
   m_pVM = pVM;
   m_gID = pObject->m_gID;
   //m_pObject = pObject;
   m_gContainedIn = pObject->m_pContainedIn ? pObject->m_pContainedIn->m_gID : GUID_NULL;
   m_lContains.Init (sizeof(GUID), pObject->m_lContainsDefault.Get(0), pObject->m_lContainsDefault.Num());

   LayerClear ();
   if (!LayerAdd ((PWSTR)pObject->m_memName.p, 0, pObject, NULL, NULL))
      return FALSE;

   // clone the vars. then, go through them and facture them all from their original
   pObject->m_hPropDefaultAllClass.CloneTo (&m_hProp);
   DWORD i;
   for (i = 0; i < m_hProp.Num(); i++) {
      PCMIFLVarProp pv = (PCMIFLVarProp)m_hProp.Get(i);
      pv->m_Var.Fracture(FALSE);
   }

   return TRUE;
}


/**********************************************************************************
CMIFLVMObject::InitAsNew - Initializes the object to create a copy of the
object specified by pObject.

DOCUMENT: Wont include contained in or contains information

inputs
   PCMIFLVM             pVM - Virtual machine to use for the object
   PCMIFLObject         pObject - Object
*/
BOOL CMIFLVMObject::InitAsNew (PCMIFLVM pVM, PCMIFLObject pObject)
{
   m_pVM = pVM;
   GUIDGen (&m_gID);
   //m_pObject = NULL;
   m_gContainedIn = GUID_NULL;
   m_lContains.Init (sizeof(GUID));

   LayerClear ();
   if (!LayerAdd ((PWSTR)pObject->m_memName.p, 0, pObject, NULL, NULL))
      return FALSE;


   // clone the vars. then, go through them and facture them all from their original
   pObject->m_hPropDefaultAllClass.CloneTo (&m_hProp);
   DWORD i;
   for (i = 0; i < m_hProp.Num(); i++) {
      PCMIFLVarProp pv = (PCMIFLVarProp)m_hProp.Get(i);
      pv->m_Var.Fracture(FALSE);
   }

   return TRUE;
}

/**********************************************************************************
CMIFLVMObject::Clone - This clones the object and assigns a new GUID to it.
It also fractures all the values.

NOTE: If the object is contianed in an object that's not cloned then the object
will be moved to NULL containship.

NOTE: If the object has timers in the main list, those must be dealt with
elsewhere.

inputs
   GUID        *pgNew - New GUID to use
   PCHashGUID  phOrigToNew - Hash of original GUID IDs to new GUID
returns
   PCMIFLVMObject - New object, or NULL.
*/
CMIFLVMObject *CMIFLVMObject::Clone (GUID *pgNew, PCHashGUID phOrigToNew)
{
   PCMIFLVMObject pNew = new CMIFLVMObject;
   if (!pNew)
      return NULL;

   pNew->m_gID = *pgNew;

   // clone the properties
   DWORD i;
   m_hProp.CloneTo (&pNew->m_hProp);
   for (i = 0; i < pNew->m_hProp.Num(); i++) {
      PCMIFLVarProp pThis = (PCMIFLVarProp) pNew->m_hProp.Get (i);
      pThis->m_Var.Fracture (FALSE);
      pThis->m_Var.Remap (phOrigToNew);
   }

   // clone all the methods
   m_hMeth.CloneTo (&pNew->m_hMeth);

   // contained in
   GUID *pRemap;
   if (!IsEqualGUID(m_gContainedIn, GUID_NULL)) {
      pRemap = (GUID*)phOrigToNew->Find (&m_gContainedIn);
      if (pRemap)
         pNew->m_gContainedIn = *pRemap;
      else
         pNew->m_gContainedIn = GUID_NULL;
   }
   else
      pNew->m_gContainedIn = GUID_NULL;

   // list of what contains
   pNew->m_lContains.Init (sizeof(GUID), m_lContains.Get(0), m_lContains.Num());
   GUID *pag = (GUID*) pNew->m_lContains.Get(0);
   for (i = 0; i < pNew->m_lContains.Num(); i++, pag++) {
      pRemap = (GUID*)phOrigToNew->Find (pag);
      if (pRemap)
         *pag = *pRemap;
      else {
         // shouldnt happen
         pNew->m_lContains.Remove (i);
         pag = (GUID*) pNew->m_lContains.Get(0);
         i--;
         pag += i;
      }
   } //
  
   pNew->m_fTimerSuspended = m_fTimerSuspended;
   pNew->m_fTimerTimeScale = m_fTimerTimeScale;
   pNew->m_dwTimerNum = m_dwTimerNum;

   // timers
   pNew->m_lPCMIFLVMTimer.Init (sizeof(PCMIFLVMTimer), m_lPCMIFLVMTimer.Get(0), m_lPCMIFLVMTimer.Num());
   PCMIFLVMTimer *ppt = (PCMIFLVMTimer*)pNew->m_lPCMIFLVMTimer.Get(0);
   for (i = 0; i < pNew->m_lPCMIFLVMTimer.Num(); i++) {
      ppt[i] = ppt[i]->Clone();
      PCMIFLVMTimer pt = ppt[i];

      // clone already fractured, but remap
      pt->m_gBelongsTo = pNew->m_gID;
      GUID *pgRemap = (GUID*)phOrigToNew->Find (&pt->m_gCall);
      if (pgRemap)
         pt->m_gCall = *pgRemap;
      pt->m_varName.Remap (phOrigToNew);
      pt->m_varParams.Remap (phOrigToNew);
   } // i

   // NOTE: Timers in the list will be dealt with later

   // others
   pNew->m_pVM = m_pVM;

   // layers
   pNew->m_lPCMIFLVMLayer.Init (sizeof(PCMIFLVMLayer), m_lPCMIFLVMLayer.Get(0), m_lPCMIFLVMLayer.Num());
   PCMIFLVMLayer *ppl = (PCMIFLVMLayer*)pNew->m_lPCMIFLVMLayer.Get(0);
   for (i = 0; i < pNew->m_lPCMIFLVMLayer.Num(); i++) {
      ppl[i] = ppl[i]->Clone ();
      if (!ppl[i]) {
         // shouldnt happen
         pNew->m_lPCMIFLVMLayer.Remove (i);
         i--;
      }
   }


   return pNew;
}

/**********************************************************************************
CMIFLVMObject::DeltaLayerVarMerge - This is used for purposes of loading/saving
deltas of VMObjects. It quickly merges all the variables of the layers tegher.
It DOES NOT change the reference counting.

inputs
   PCHashDWORD          phMerge - The layers are merged into this. Anything
                        that was in this list is wiped out in the process
returns
   none
*/
void CMIFLVMObject::DeltaLayerVarMerge (PCHashDWORD phMerge)
{
   DWORD dwNum;
   if (!(dwNum = LayerNum())) {
      // nothing
      phMerge->Init (sizeof(CMIFLVarProp));
      phMerge->Clear();
      return;
   }

   // take the lowest layer automacally
   PCMIFLVMLayer pLayer = LayerGet (dwNum-1);
   pLayer->m_pObject->m_hPropDefaultAllClass.CloneTo (phMerge);

   // DOCUMENT: When have multiple layers and saves, bases off lowest priority layer,
   // assuming that's the one originally created with, and which has the most in common
   // DOesn't assume properties from other layers

#if 0 // dont do this, just assume properties based on lowest layer, because when
      // addLayer() doesnt end up adding properties, so why should this?

   // and other layers
   DWORD i, j;
   if (dwNum >= 2) for (i = 0; i < dwNum-1; i++) {
      pLayer = LayerGet (i);
      PCMIFLObject po = pLayer->m_pObject;

      PCMIFLVarProp pFrom;
      for (j = 0; j < po->m_hPropDefaultAllClass.Num(); j++) {
         pFrom = (PCMIFLVarProp) po->m_hPropDefaultAllClass.Get(j);

         // see if exists already
         DWORD dwIndex = phMerge->FindIndex (pFrom->m_dwID);
         if (dwIndex == -1)
            continue;

         phMerge->Remove (dwIndex);
         phMerge->Add (pFrom->m_dwID, pFrom);
      } // j
   } // i
#endif //0
}


/**********************************************************************************
CMIFLVMObject::MMLTo - Writes the object to a MMLNode

NOTE: This only writes the DELTA of the object, what has changed.

DOCUMENT: When save only writes what has changed from default instantiation.
   And is epecially tricky when change properties. As much intiaalization
   as possible should go in the variable initializes (NOT constructor)
   so that delta more accurate.

inputs
   PCHashPVOID       phString - Hash of string pointer to ID. 0-length. Will be added to.
   PCHashPVoid       phList - Hash of list pointer to ID. 0-lenght. Will be added to
returns
   PCMMLNode2 - Node
*/
static PWSTR gpszVMObject = L"VMObject";
static PWSTR gpszID = L"ID";     // NOTE: If change here much change in CMIFLVM
static PWSTR gpszTimerSuspended = L"TimerSuspended";
static PWSTR gpszTimerTimeScale = L"TimerTimeScale";
static PWSTR gpszContainedIn = L"ContainedIn";
//static PWSTR gpszContains = L"Contains";
static PWSTR gpszProp = L"Prop";
static PWSTR gpszVar = L"Var";
static PWSTR gpszDeleted = L"Deleted";

PCMMLNode2 CMIFLVMObject::MMLTo (PCHashPVOID phString, PCHashPVOID phList)
{
   PCMMLNode2 pNode = new CMMLNode2;
   PCMMLNode2 pSub;
   if (!pNode)
      return NULL;
   pNode->NameSet (gpszVMObject);

   MMLValueSet (pNode, gpszID, (PBYTE)&m_gID, sizeof(m_gID));
   MMLValueSet (pNode, gpszTimerSuspended, (int)m_fTimerSuspended);
   if (m_fTimerTimeScale != 1.0)
      MMLValueSet (pNode, gpszTimerTimeScale, (int)m_fTimerTimeScale);
   if (!IsEqualGUID (m_gContainedIn, GUID_NULL))
      MMLValueSet (pNode, gpszContainedIn, (PBYTE)&m_gContainedIn, sizeof(m_gContainedIn));

   // BUGFIX - Disabling saving contains because end up reconstructing everything in the
   // end, so all really need is containedin
   //if (m_lContains.Num())
   //   MMLValueSet (pNode, gpszContains, (PBYTE)m_lContains.Get(0), sizeof(GUID)*m_lContains.Num());

   // write out all the timers
   CListFixed lTemp;
   PCListFixed plt;
   if (m_fTimerSuspended)
      plt = &m_lPCMIFLVMTimer;
   else {
      plt = &lTemp;
      lTemp.Init (sizeof(PCMIFLVMTimer));
      if (m_dwTimerNum)
         m_pVM->TimerEnum (&m_gID, FALSE, plt);
   }
   PCMIFLVMTimer *ppt = (PCMIFLVMTimer*)plt->Get(0);
   DWORD i;
   for (i = 0; i < plt->Num(); i++) {
      pSub = ppt[i]->MMLTo (m_pVM, phString, phList);
      if (pSub)
         pNode->ContentAdd (pSub);
      else
         return NULL;
   } // i

   // write out all the layers
   PCMIFLVMLayer *ppl = (PCMIFLVMLayer*) m_lPCMIFLVMLayer.Get(0);
   for (i = 0; i < m_lPCMIFLVMLayer.Num(); i++) {
      pSub = ppl[i]->MMLTo (m_pVM, phString, phList);
      if (pSub)
         pNode->ContentAdd(pSub);
      else
         return NULL;
   }

   // write all the variables

   // get a list of variables as they would be if merged
   CHashDWORD hMerge;
   DeltaLayerVarMerge (&hMerge);

   // loop through and find properties that are changed or added
   PCMMLNode2 pSub2;
   DWORD dwPass;
   for (dwPass = 0; dwPass < 2; dwPass++) {
      // if dwPass = 0, looping through all proprties in object and finding what changed
      // if dwPass = 1, looping through all properties in merge and findout out which deleted
      PCHashDWORD phThis = dwPass ? &hMerge : &m_hProp;
      PCHashDWORD phFind = dwPass ? &m_hProp : &hMerge;

      for (i = 0; i < phThis->Num(); i++) {
         // get them
         PCMIFLVarProp pThis = (PCMIFLVarProp) phThis->Get(i);

         // find it in the delta layer?
         PCMIFLVarProp pFind = (PCMIFLVarProp) phFind->Find (pThis->m_dwID);

         // NOTE: Not testing get and set different because those would
         // be stored in the layers
         BOOL /*fGet = FALSE, fSet = FALSE,*/ fVar = FALSE;
         if (dwPass) {
            // only care about ones that aren't in the original list (pFind)
            if (pFind)
               continue;
         }
         else {
            // figure out what's different
            if (!pFind)
               /*fGet = fSet =*/ fVar = TRUE;
            else {
               //fGet = (pThis->m_pCodeGet != pFind->m_pCodeGet);
               //fSet = (pThis->m_pCodeSet != pFind->m_pCodeSet);
               fVar = pThis->m_Var.Compare (&pFind->m_Var, TRUE, m_pVM) ? TRUE : FALSE;

               if (/*!fGet && !fSet &&*/ !fVar)
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

         //if (fGet) {
         //   pSub2 = CodeGetSetMMLTo (pThis->m_pCodeGet);
         //   if (pSub2) {
         //      pSub2->NameSet (gpszGet);
         //      pSub->ContentAdd (pSub2);
         //   }
         //}
         //if (fSet) {
         //   pSub2 = CodeGetSetMMLTo (pThis->m_pCodeSet);
         //   if (pSub2) {
         //      pSub2->NameSet (gpszSet);
         //      pSub->ContentAdd (pSub2);
         //   }
         //}
         if (fVar) {
            pSub2 = pThis->m_Var.MMLTo (m_pVM, phString, phList);
            if (pSub2) {
               pSub2->NameSet (gpszVar);
               pSub->ContentAdd (pSub2);
            }
         } // if fVar
         if (dwPass)
            MMLValueSet (pSub, gpszDeleted, (int)TRUE);

         // get the name
         PWSTR pszPrivate = NULL;
         PWSTR psz = NULL;
         if (pThis->m_dwID >= VM_CUSTOMIDRANGE) {
            DWORD dwIndex = m_pVM->m_hUnIdentifiersCustomProperty.FindIndex (pThis->m_dwID);
            psz = m_pVM->m_hIdentifiersCustomProperty.GetString (dwIndex);
         }
         else {
            PMIFLIDENT pmi = (PMIFLIDENT) m_pVM->m_pCompiled->m_hUnIdentifiers.Find (pThis->m_dwID);
            if (pmi && ((pmi->dwType == MIFLI_PROPDEF) || (pmi->dwType == MIFLI_PROPPRIV))) {
               PCMIFLProp pProp = (PCMIFLProp)pmi->pEntity;
               psz = (PWSTR) pProp->m_memName.p;

               // if it's private need to find from where...
               if (pProp->m_pObjectFrom)
                  pszPrivate = (PWSTR)pProp->m_pObjectFrom->m_memName.p;
            }
         }
         if (!psz || !psz[0]) {
            delete pNode;
            return NULL;   // cant add this. shouldn happen
         }

         // write out the name
         MMLValueSet (pSub, gpszName, psz);

         // write out the private object it's from
         if (pszPrivate)
            MMLValueSet (pSub, gpszObject, pszPrivate);

      } // i, over m_hProp
   } // dwPass

   // done
   return pNode;
}

/**********************************************************************************
CMIFLVMObject::MMLFrom - Initializes the object to create a copy of the
object specified by pObject.

NOTE: This takes the places of the InitXXX() call
NOTE: When returns m_gContainedIn will be valid, but m_lContains will be set to 0.
   Do this because assume will be rebuilding the containership list

NOTE: Does NOT call Constructor2().

DOCUMENT: Problems with built-in objects and reloading them, if change basic class,
won't notice that has changed

inputs
   PCMMLNode2            pNode - Node to get from
   PCMIFLVM             pVM - Virtual machine to use for the object
   PCHashDWORD       phString - Hash of string pointer to ID. Each element is
                     a PCMIFLVarString.
   PCHashDWORD       phList - Hash of list pointer to ID. Each element is a pointer
                     to a PCMIFLVarString
   PCHashGUID        phObjectRemap - Hash is of original object ID. Contents are GUID of new ID
returns
   BOOL - TRUE if success
*/
BOOL CMIFLVMObject::MMLFrom (PCMMLNode2 pNode, PCMIFLVM pVM, PCHashDWORD phString, PCHashDWORD phList,
                             PCHashGUID phObjectRemap)
{
   // NOTE: Don't change this without first consulting MIFServer's database
   // code since it directly access the MML

   m_pVM = pVM;
   if (sizeof(m_gID) != MMLValueGetBinary (pNode, gpszID, (PBYTE)&m_gID, sizeof(m_gID)))
      return FALSE;
   GUID *pg = (GUID*)phObjectRemap->Find (&m_gID);
   if (pg)
      m_gID = *pg;

   m_fTimerSuspended = (BOOL) MMLValueGetInt (pNode, gpszTimerSuspended, (int)0);
   m_fTimerTimeScale = MMLValueGetDouble (pNode, gpszTimerTimeScale, 1.0);

   if (sizeof(m_gContainedIn) != MMLValueGetBinary (pNode, gpszContainedIn, (PBYTE)&m_gContainedIn, sizeof(m_gContainedIn)))
      m_gContainedIn = GUID_NULL;
   else {
      // see if remap
      GUID *pg = (GUID*)phObjectRemap->Find (&m_gContainedIn);
      if (pg)
         m_gContainedIn = *pg;
   }

   PWSTR psz;
   // BUGFIX - Disabling saving contains because end up reconstructing everything in the
   // end, so all really need is containedin
   //psz = MMLValueGet (pNode, gpszContains);
   //if (psz) {
   //   CMem mem;
   //   // allocate enough to load all in
   //   if (!mem.Required (wcslen(psz) / 2))
   //      return FALSE;
   //   DWORD dwRet = MMLBinaryFromString (psz, (PBYTE)mem.p, mem.m_dwAllocated);
   //   m_lContains.Init (sizeof(GUID), mem.p, dwRet / sizeof(GUID));
   //}
   //else
   m_lContains.Clear();

   // load in the layers...
   LayerClear();
   PCMMLNode2 pSub, pSub2;
   CMIFLVarProp vDef;
   DWORD i;
   for (i = 0; i < pNode->ContentNum(); i++) {
      pSub = NULL;
      pNode->ContentEnum (i, &psz, &pSub);
      if (!pSub)
         continue;
      psz = pSub->NameGet();
      if (!psz)
         continue;

      if (!_wcsicmp(psz, gpszVMLayer)) {
         PCMIFLVMLayer pNew = new CMIFLVMLayer;
         if (!pNew)
            return FALSE;
         if (!pNew->MMLFrom (pSub, m_pVM, phString, phList)) {
            // if error, ignore it, loading as much as possible
            delete pNew;

            // BUGFIX - If couldnt load a layer, was ignoring the problem, but
            // cant do this because if object definition no longer exists then
            // shouldnt be able to load
            return FALSE;
         }
         m_lPCMIFLVMLayer.Add (&pNew);
         continue;
      } // layer
   } // i enum over layers

   // NOTE: Assuming that variables list it empty, which it should be
   if (m_hProp.Num())
      return FALSE;
   DeltaLayerVarMerge (&m_hProp);
   for (i = 0; i < m_hProp.Num(); i++) {
      // addref and fracture the resulting variables from the merge
      PCMIFLVarProp pvp = (PCMIFLVarProp)m_hProp.Get(i);
      pvp->m_Var.Fracture(FALSE);
   } // i


   // NOTE: Assuming that timers list is EMPTY, which is ok if initializing
   if (m_lPCMIFLVMTimer.Num())
      return FALSE;
   m_dwTimerNum = 0;
   for (i = 0; i < pNode->ContentNum(); i++) {
      pSub = NULL;
      pNode->ContentEnum (i, &psz, &pSub);
      if (!pSub)
         continue;
      psz = pSub->NameGet();
      if (!psz)
         continue;

      if (!_wcsicmp(psz, gpszVMTimer)) {
         PCMIFLVMTimer pNew = new CMIFLVMTimer;
         if (!pNew)
            return FALSE;
         if (!pNew->MMLFrom (pSub, &m_gID, m_pVM, phString, phList, phObjectRemap)) {
            // if error, ignore it, loading as much as possible
            delete pNew;
            continue;
         }

         // add it...
         m_dwTimerNum++;
         if (m_fTimerSuspended)
            m_lPCMIFLVMTimer.Add (&pNew);
         else {
            pNew->m_fTimeScale = m_fTimerTimeScale;   // just to make sure
            m_pVM->TimerAdd (pNew);
         }
         continue;
      } // timer
      else if (!_wcsicmp(psz, gpszProp)) {
         // get the name, and potentially the object..
         PWSTR pszName = MMLValueGet (pSub, gpszName);
         PWSTR pszObject = MMLValueGet (pSub, gpszObject);
         if (!pszName)
            continue;   // error

         // if object, then find it because it's a private variable...
         PMIFLIDENT pmi = pszObject ? (PMIFLIDENT) m_pVM->m_pCompiled->m_hIdentifiers.Find (pszObject,TRUE) : NULL;
         PCMIFLObject po = (pmi && (pmi->dwType == MIFLI_OBJECT)) ? (PCMIFLObject)pmi->pEntity : NULL;

         // find the ID
         DWORD dwID = -1;
         if (po) {
            pmi = (PMIFLIDENT)po->m_hPrivIdentity.Find (pszName, TRUE);
            if (pmi && (pmi->dwType == MIFLI_PROPPRIV))
               dwID = pmi->dwID;
         }
         if (dwID == -1)
            dwID = m_pVM->ToPropertyID (pszName, TRUE);
         if (dwID == -1)
            continue;   // shouldnt happen

         // is this to be deleted
         if (MMLValueGetInt (pSub, gpszDeleted, 0)) {
            DWORD dwIndex = m_hProp.FindIndex (dwID);
            if (dwIndex != -1) {
               PCMIFLVarProp pvp = (PCMIFLVarProp) m_hProp.Get(dwIndex);
               if (pvp) {
                  pvp->m_Var.SetUndefined ();   // so ref count ok
                  m_hProp.Remove (dwIndex);
               }
            }
            continue;
         }

         // if not exist then create
         PCMIFLVarProp pvp = (PCMIFLVarProp) m_hProp.Find (dwID);
         if (!pvp) {
            vDef.m_Var.SetUndefined();
            vDef.m_dwID = dwID;
            vDef.m_pCodeGet = vDef.m_pCodeSet = NULL;
            pvp = &vDef;
         }

         // potentially change...

         // NOTE: Not changing get/set since layer merge should deal with
         // get
         //pSub2 = NULL;
         //pSub->ContentEnum (pSub->ContentFind(gpszGet), &psz, &pSub2);
         //if (pSub2)
         //   // have get code
         //   pvp->m_pCodeGet = (PCMIFLCode) CodeGetSetMMLFrom (pSub2, m_pVM, FALSE);

         // set
         //pSub2 = NULL;
         //pSub->ContentEnum (pSub->ContentFind(gpszSet), &psz, &pSub2);
         //if (pSub2)
         //   // have get code
         //   pvp->m_pCodeSet = (PCMIFLCode) CodeGetSetMMLFrom (pSub2, m_pVM, FALSE);

         // value
         pSub2 = NULL;
         pSub->ContentEnum (pSub->ContentFind(gpszVar), &psz, &pSub2);
         if (pSub2)
            // have get code
            pvp->m_Var.MMLFrom (pSub2, m_pVM, phString, phList, phObjectRemap);


         // add?
         if (pvp == &vDef) {
            vDef.m_Var.AddRef ();   // so extra count
            m_hProp.Add (dwID, pvp);
         }
         continue;
      } // variable
   } // i, over content

   // rebuild the layers. no need to sort though
   LayerMerge ();

   return TRUE;
}



/**********************************************************************************
CMIFLVMObject::ContainedBySet - Changes the container of this object.

inputs
   GUID           *pgContainer - New container. If NULL then no container
returns
   BOOL - TRUE if successs
*/
BOOL CMIFLVMObject::ContainedBySet (GUID *pgContainer)
{
   GUID gNull;
   if (!pgContainer) {
      memset (&gNull, 0, sizeof(gNull));
      pgContainer = &gNull;
   }

   if (IsEqualGUID (*pgContainer, m_gContainedIn))
      return TRUE;   // no change
   if (IsEqualGUID (*pgContainer, m_gID))
      return FALSE;   // cant contain within itself
   
   // take away from old container
   PCMIFLVMObject pParent;
   DWORD i;
   if (!IsEqualGUID (m_gContainedIn, GUID_NULL)) {
      pParent = m_pVM->ObjectFind (&m_gContainedIn);
      if (pParent) {
         GUID *pag = (GUID*) pParent->m_lContains.Get(0);
         for (i = 0; i < pParent->m_lContains.Num(); i++)
            if (IsEqualGUID(m_gID, pag[i]))
               break;
         if (i < pParent->m_lContains.Num())
            pParent->m_lContains.Remove (i);
      }
   }

   // new parent
   if (!IsEqualGUID (*pgContainer, GUID_NULL)) {
      pParent = m_pVM->ObjectFind (pgContainer);
      if (!pParent) {
         // error, cant find
         m_gContainedIn = GUID_NULL;
         return FALSE;
      }

      // else, set
      m_gContainedIn = *pgContainer;
      pParent->m_lContains.Add (&m_gID);
   }
   else
      m_gContainedIn = GUID_NULL;

   return TRUE;
}


/**********************************************************************************
CMIFLVMObject::ContainDisconnectAll - Disconnects this object from all containers,
bother the parent and the children.

returns
   BOOL - TRUE if successs
*/
BOOL CMIFLVMObject::ContainDisconnectAll (void)
{
   ContainedBySet (NULL);

   // go through everything it contains and clear out
   DWORD i;
   PCMIFLVMObject po;
   GUID *pag = (GUID*)m_lContains.Get(0);
   for (i = 0; i < m_lContains.Num(); i++) {
      po = m_pVM->ObjectFind (&pag[i]);
      if (!po)
         continue;

      po->m_gContainedIn = GUID_NULL;
   } // i
   m_lContains.Clear();
   return TRUE;
}



/**********************************************************************************
CMIFLVMObject::LayerAdd - Adds a new layer to the object. In the process the
layers will be resorted and the appropriate tables rebuilt.

inputs
   PWSTR          pszName - Name of the layer.
   double         fRank - Rank of the layer. Higher ranks have their methods called first
   PCMIFLObject   pObject - Object/class whose methods and property get/set are used.
   PCHashDWORD    phPropGetSet - This can be NULL. It's a hash (by property ID) of
                  MIFLGETSET so know which properties are overridden and how. Use this
                  for extra-class property get/set
   PCHashDWORD    phMeth - This can be NULL. It's a has (by method ID) of
                  MIFLIDENT's for the functions used by the methds. Use this
                  for extra-class methods.
returns
   BOOL - TRUE if success
*/
BOOL CMIFLVMObject::LayerAdd (PWSTR pszName, double fRank, PCMIFLObject pObject,
   PCHashDWORD phPropGetSet, PCHashDWORD phMeth)
{
   PCMIFLVMLayer pNew = new CMIFLVMLayer;
   if (!pNew)
      return FALSE;

   MemZero (&pNew->m_memName);
   MemCat (&pNew->m_memName, pszName);
   pNew->m_fRank = fRank;
   pNew->m_pObject = pObject;
   if (phPropGetSet)
      phPropGetSet->CloneTo (&pNew->m_hPropGetSet);
   if (phMeth)
      phMeth->CloneTo (&pNew->m_hMeth);
   m_lPCMIFLVMLayer.Add (&pNew);

   return LayerSort();  // since will also merge
}



/**********************************************************************************
CMIFLVMObject::LayerRemove - Removes a layer from the object.

inputs
   DWORD          dwIndex - Layer number, from 0..LayerNum()
returns
   BOOL - TRUE if success
*/
BOOL CMIFLVMObject::LayerRemove (DWORD dwIndex)
{
   if (dwIndex >= m_lPCMIFLVMLayer.Num())
      return FALSE;

   PCMIFLVMLayer *ppv = (PCMIFLVMLayer*)m_lPCMIFLVMLayer.Get(0);
   
   delete ppv[dwIndex];
   m_lPCMIFLVMLayer.Remove (dwIndex);

   // no need to sort
   return LayerMerge();
}


/**********************************************************************************
CMIFLVMObject::LayerNum - Returns the number of layers in the object
*/
DWORD CMIFLVMObject::LayerNum (void)
{
   return m_lPCMIFLVMLayer.Num();
}



/**********************************************************************************
CMIFLVMObject::LayerGet - Gets a layer of the object. If the rank is changed then
LayerSort() MUST be called. If any of the method/property information is changed
then LayerMerge() MUST be called.

inputs
   DWORD          dwIndex - Index
returns
   PCMIFLVMLayer - Layer. Do NOT delete
*/
PCMIFLVMLayer CMIFLVMObject::LayerGet (DWORD dwIndex)
{
   if (dwIndex >= m_lPCMIFLVMLayer.Num())
      return FALSE;

   PCMIFLVMLayer *ppv = (PCMIFLVMLayer*)m_lPCMIFLVMLayer.Get(0);

   return ppv[dwIndex];
}


/**********************************************************************************
CMIFLVMObject::LayerClear - Wipes out all the layers
*/
void CMIFLVMObject::LayerClear (void)
{
   // free up the layers
   DWORD i;
   PCMIFLVMLayer *ppl = (PCMIFLVMLayer*)m_lPCMIFLVMLayer.Get(0);
   for (i = 0; i < m_lPCMIFLVMLayer.Num(); i++)
      delete ppl[i];
   m_lPCMIFLVMLayer.Clear();

   LayerMerge();
}


/**********************************************************************************
CMIFLVMObject::LayerSort - Resorts the layers so the highest ranks occur first
in the list
*/

static int _cdecl MIFLVMLayerCompare (const void *elem1, const void *elem2)
{
   PCMIFLVMLayer pdw1, pdw2;
   pdw1 = *((PCMIFLVMLayer*) elem1);
   pdw2 = *((PCMIFLVMLayer*) elem2);

   if (pdw1->m_fRank < pdw2->m_fRank)
      return 1;
   else if (pdw1->m_fRank > pdw2->m_fRank)
      return -1;
   else
      return 0;
}


BOOL CMIFLVMObject::LayerSort (void)
{
   qsort (m_lPCMIFLVMLayer.Get(0), m_lPCMIFLVMLayer.Num(),
      sizeof(PCMIFLVMLayer), MIFLVMLayerCompare);

   return LayerMerge();
}

/**********************************************************************************
CMIFLVMObject::LayerMerge - Merges all the layers of the object togehter into
one layer to produce faster access tables.
*/
BOOL CMIFLVMObject::LayerMerge (void)
{
   // first of all, go through all the properties and set the m_pCodeGet to 1
   // so know if have visited already
   DWORD i, j;
   PCMIFLVarProp pv;
   for (i = 0; i < m_hProp.Num(); i++) {
      pv = (PCMIFLVarProp) m_hProp.Get(i);
      pv->m_pCodeGet = (PCMIFLCode)1;
   } // i

   // clear out the methds
   m_hMeth.Clear();

   // loop through all the layers (assume they're sorted)
   PCMIFLVMLayer *ppl = (PCMIFLVMLayer*)m_lPCMIFLVMLayer.Get(0);
   for (i = 0; i < m_lPCMIFLVMLayer.Num(); i++) {
      PCMIFLVMLayer pl = ppl[i];
      PCMIFLObject po = pl->m_pObject;

      // loop through all the exceptions properties and set get/set
      for (j = 0; j < pl->m_hPropGetSet.Num(); j++) {
         PMIFLGETSET pgs = (PMIFLGETSET)pl->m_hPropGetSet.Get(j);
         pv = (PCMIFLVarProp)m_hProp.Find (pgs->dwID);

         if (!pv) {
            if (!pgs->m_pCodeGet)
               continue;   // doesn't matter if override because variable isn't used

            // else, need to create and fill with unknown so can call special property-get
            // code, even though no default property
            CMIFLVarProp vDef;
            vDef.m_dwID = pgs->dwID;
            vDef.m_Var.SetUndefined();
            vDef.m_pCodeGet = (PCMIFLCode)1;
            m_hProp.Add (pgs->dwID, &vDef);

            pv = (PCMIFLVarProp)m_hProp.Find (pgs->dwID);
            if (!pv)
               continue;   // shouldnt happen
         }

         if (pv->m_pCodeGet != (PCMIFLCode)1)
            continue;   // has already been set

         // else, set
         pv->m_pCodeGet = pgs->m_pCodeGet;
         pv->m_pCodeSet = pgs->m_pCodeSet;
      } // j

      // loop through all the exceptions properties for the object
      PCMIFLVarProp pFrom;
      for (j = 0; j < po->m_hPropDefaultAllClass.Num(); j++) {
         pFrom = (PCMIFLVarProp) po->m_hPropDefaultAllClass.Get(j);
         pv = (PCMIFLVarProp)m_hProp.Find (pFrom->m_dwID);
         if (!pv) {
            if (!pFrom->m_pCodeGet)
               continue;   // doesn't matter if override because variable isn't used

            // else, need to create and fill with unknown so can call special property-get
            // code, even though no default property
            CMIFLVarProp vDef;
            vDef.m_dwID = pFrom->m_dwID;
            vDef.m_Var.SetUndefined();
            vDef.m_pCodeGet = (PCMIFLCode)1;
            m_hProp.Add (pFrom->m_dwID, &vDef);

            pv = (PCMIFLVarProp)m_hProp.Find (pFrom->m_dwID);
            if (!pv)
               continue;   // shouldnt happen
         }

         if (pv->m_pCodeGet != (PCMIFLCode)1)
            continue;   // has already been set

         // else, set
         pv->m_pCodeGet = pFrom->m_pCodeGet;
         pv->m_pCodeSet = pFrom->m_pCodeSet;
      } // j

      // loop through all the extra-class methods
      PMIFLIDENT piFrom, pi;
      for (j = 0; j < pl->m_hMeth.Num(); j++) {
         piFrom = (PMIFLIDENT)pl->m_hMeth.Get(j);
         pi = (PMIFLIDENT)m_hMeth.Find (piFrom->dwID);
         if (pi)
            continue;   // already exists, so use since will be higher priority

         // else, add
         m_hMeth.Add (piFrom->dwID, piFrom);
      } // j

      // loop through all the class's methods
      for (j = 0; j < po->m_hMethAllClass.Num(); j++) {
         piFrom = (PMIFLIDENT)po->m_hMethAllClass.Get(j);
         pi = (PMIFLIDENT)m_hMeth.Find (piFrom->dwID);
         if (pi)
            continue;   // already exists, so use since will be higher priority

         // else, add
         m_hMeth.Add (piFrom->dwID, piFrom);
      } // j
   } // i

   // finally, go through all the properties and clear any m_pCodeGet that
   // are still 1
   for (i = 0; i < m_hProp.Num(); i++) {
      pv = (PCMIFLVarProp) m_hProp.Get(i);
      if (pv->m_pCodeGet == (PCMIFLCode)1)
         pv->m_pCodeGet = pv->m_pCodeSet = NULL;
   } // i

   return TRUE;
}


/**********************************************************************************
CMIFLVMObject::TimerSuspendGet - Returns 0.0 if the timers are suspended, or
a timer-scale value (1.0 = normal) if they aren't.
*/
double CMIFLVMObject::TimerSuspendGet (void)
{
   if (m_fTimerSuspended)
      return 0.0;

   return m_fTimerTimeScale;
}

/**********************************************************************************
CMIFLVMObject::TimerSuspendSet - Suspends (or restarts) the object timers.

inputs
   double         fTimerScale - If 1.0, timer will be active, 0.0 all suspended. In-between, timers slow.
                  Above, timers faster.
   BOOL           fChildren - If TRUE then suspend children(contained) too, else only this
*/
void CMIFLVMObject::TimerSuspendSet (double fTimerScale, BOOL fChildren)
{
   BOOL fSuspend;
   if (fTimerScale <= 0.0) {
      // want to stop
      fTimerScale = 0.0;   // dont go too low
      fSuspend = TRUE;
   }
   else
      fSuspend = FALSE;

   if (fChildren) {
      // calculate a list of objects to suspend...
      CHashGUID hSuspend;
      hSuspend.Init (0, (1 + m_lContains.Num())*3);
      hSuspend.Add (&m_gID, 0);

      DWORD i, j;
      GUID *pg;
      for (i = 0; i < hSuspend.Num(); i++) {
         pg = hSuspend.GetGUID(i);
         PCMIFLVMObject pChild = i ? m_pVM->ObjectFind (pg) : this;
         if (!pChild)
            continue;

         pg = (GUID*)pChild->m_lContains.Get(0);
         DWORD dwNum = pChild->m_lContains.Num();
         for (j = 0; j < dwNum; j++, pg++)
            if (-1 == hSuspend.FindIndex (pg))  // BUGFIX - Only add if not there already, prevent infinite loops
               hSuspend.Add (pg, 0);

         // suspend this one
         pChild->TimerSuspendSet (fTimerScale, FALSE);
      } // i, hSuspended
      return;
   }

   fSuspend = fSuspend ? TRUE : FALSE;
   m_fTimerSuspended = m_fTimerSuspended ? TRUE : FALSE;

   if ((fSuspend == m_fTimerSuspended) && (fTimerScale == m_fTimerTimeScale))
      return;  // no change

   // remember the new scale
   m_fTimerTimeScale = fTimerScale;

   // if changed whether suspended or not then vlip
   DWORD i;
   if (fSuspend != m_fTimerSuspended) {
      // remember new flag
      m_fTimerSuspended = fSuspend;

      if (m_fTimerSuspended) {   // BUGBUG - will need to deal with timer suspended differently
         // just suspended timers so pull them from main list
         if (m_dwTimerNum) {
            m_pVM->TimerEnum (&m_gID, TRUE, &m_lPCMIFLVMTimer);
            m_dwTimerNum = m_lPCMIFLVMTimer.Num(); // just in case was off
         }
      }
      else {
         // resume the timers...
         PCMIFLVMTimer *ppt = (PCMIFLVMTimer*) m_lPCMIFLVMTimer.Get(0);
         m_dwTimerNum = m_lPCMIFLVMTimer.Num(); // just in case was off
         for (i = 0; i < m_dwTimerNum; i++) {
            ppt[i]->m_fTimeScale = m_fTimerTimeScale;   // just to make sure
            m_pVM->TimerAdd (ppt[i]);
         }
         m_lPCMIFLVMTimer.Clear();  // since all in VM now
      }
   }
   else if (!m_fTimerSuspended) {   // and (fTimerScale != m_fTimerTimeScale)
      // if get here, timer flag is the same, so m_fTimerTimeScale must
      // have changed
      CListFixed lTimers;
      lTimers.Init (sizeof(PCMIFLVMTimer));
      m_pVM->TimerEnum (&m_gID, FALSE, &lTimers);
      PCMIFLVMTimer *ppt = (PCMIFLVMTimer*) lTimers.Get(0);
      for (i = 0; i < lTimers.Num(); i++)
         ppt[i]->m_fTimeScale = m_fTimerTimeScale;
   }
   // else, changed m_fTimerTimeScale, but all the timers are suspended, so doesn't matter
}


/*****************************************************************************
CMIFLVMObject::TimerFind - Finds a timer by owner and identifier.

NOTE:If the timers are suspended this looks in the local timers and returns
that index. If they're not suspended it looks in the VM.

inputs
   PCMIFLVar      pVarName - Name. Do exact compare to match
returns
   DWORD - Index into timers, or -1 if can't find
*/
DWORD CMIFLVMObject::TimerFind (PCMIFLVar pVarName)
{
   if (!m_fTimerSuspended)
      return m_pVM->TimerFind (&m_gID, pVarName);

   // else, interal
   DWORD dwNum = m_lPCMIFLVMTimer.Num();
   PCMIFLVMTimer *ppt = (PCMIFLVMTimer*)m_lPCMIFLVMTimer.Get(0);
   DWORD i;
   for (i = 0; i < dwNum; i++)
      if (!pVarName->Compare (&ppt[i]->m_varName, TRUE, m_pVM))
         return i;

   // else not found
   return -1;
}


/**********************************************************************************
CMIFLVMObject::TimerAdd - Adds a timer to the list.

inputs
   PCMIFLVar      pVarName - Name that will use to identify the timer. If
                     it's a string then case sensative compare
   BOOL           fRepeating - TRUE if want timer to repeat, FALSE if only happens once
                     and then autoamtically kills itself.
   double         fTimeRepeat - Number of seconds before it goes off (and/or) repeats.
   GUID           *pgCall - Object to call. If GUID_NULL then will call a function
   DWORD          dwCallID - Mehtod ID (if calling object), or function ID (if not object)
   PCMIFLVar      pVarParams - Parameters to pass into call.
                           NOTE: pVarParams will NOT be fractured, so don't pass
                           the list will merely be addrefed
returns
   BOOL - TRUE if success. FALSE if already exists.
*/
BOOL CMIFLVMObject::TimerAdd (PCMIFLVar pVarName, BOOL fRepeating, double fTimeRepeat,
   GUID *pgCall, DWORD dwCallID, PCMIFLVar pVarParams)
{
   // if too short then error
   if (fTimeRepeat < (fRepeating ? 0.001 : 0))  // BUGFIX - minimum length on timre
      return FALSE;

   // error if already exists
   if (-1 != TimerFind (pVarName))
      return FALSE;

   // create new one
   PCMIFLVMTimer pNew = new CMIFLVMTimer;
   if (!pNew)
      return FALSE;
   pNew->m_fRepeating = fRepeating;
   pNew->m_fTimeLeft = pNew->m_fTimeRepeat = fTimeRepeat;
   pNew->m_fTimeScale = m_fTimerTimeScale;
   pNew->m_gBelongsTo = m_gID;
   pNew->m_gCall = *pgCall;
   pNew->m_dwCallID = dwCallID;
   pNew->m_varName.Set (pVarName);
   pNew->m_varName.Fracture();
   pNew->m_varParams.Set (pVarParams);
   // NOTE: Dont fracture pNew->m_varParams.Fracture();

   // add it
   m_dwTimerNum++;
   if (m_fTimerSuspended)
      m_lPCMIFLVMTimer.Add (&pNew);
   else
      m_pVM->TimerAdd (pNew);

   return TRUE;
}



/**********************************************************************************
CMIFLVMObject::TimerRemove - Removes a timer from the list.

inputs
   PCMIFLVar         pVarName - Name to idenify it
returns
   BOOL - TRUE if found and removed it, FALSE if coultn find
*/
BOOL CMIFLVMObject::TimerRemove (PCMIFLVar pVarName)
{
   DWORD dwIndex = TimerFind (pVarName);
   if (dwIndex == -1)
      return FALSE;

   if (m_fTimerSuspended) {
      PCMIFLVMTimer *ppt = (PCMIFLVMTimer*)m_lPCMIFLVMTimer.Get(0);
      delete ppt[dwIndex];
      m_lPCMIFLVMTimer.Remove (dwIndex);
   }
   else {
      PCMIFLVMTimer pp = m_pVM->TimerRemove (dwIndex);
      delete pp;
   }

   // removed
   m_dwTimerNum--;
   return TRUE;
}


/**********************************************************************************
CMIFLVMObject::TimerRemoveAll - Remove all timers from the object.
*/
void CMIFLVMObject::TimerRemoveAll (void)
{
   // pull timers in from vm list
   if (!m_fTimerSuspended && m_dwTimerNum)
      m_pVM->TimerEnum (&m_gID, TRUE, &m_lPCMIFLVMTimer);

   // free up all the suspended timers
   DWORD i;
   PCMIFLVMTimer *ppt = (PCMIFLVMTimer*)m_lPCMIFLVMTimer.Get(0);
   for (i = 0; i < m_lPCMIFLVMTimer.Num(); i++)
      delete ppt[i];

   m_dwTimerNum = 0;
   m_lPCMIFLVMTimer.Clear();
}


/**********************************************************************************
CMIFLVMObject::TimerRemovedByVM - This is called by the VM to tell the object that
one of it's running timers has automatically been removed because it was called and
then was not repeating, so it was deleted
*/
void CMIFLVMObject::TimerRemovedByVM (void)
{
   m_dwTimerNum--;
}



/**********************************************************************************
CMIFLVMObject::TimerEnum - Enumerates all the timers in the object.

inputs
   PCMIFLVar         pRet - Filled with a list
*/
void CMIFLVMObject::TimerEnum (PCMIFLVar pRet)
{
   CListFixed lTemp;
   PCListFixed plTimers;
   if (m_fTimerSuspended)
      plTimers = &m_lPCMIFLVMTimer;
   else {
      lTemp.Init (sizeof(PCMIFLVMTimer));
      plTimers = &lTemp;
      m_pVM->TimerEnum (&m_gID, FALSE, plTimers);
   }

   PCMIFLVarList pl = new CMIFLVarList;
   if (!pl) {
      pRet->SetUndefined();
      return;
   }

   // loop through the timers
   PCMIFLVMTimer *ppt = (PCMIFLVMTimer*)plTimers->Get(0);
   CMIFLVar v;
   DWORD i;
   for (i = 0; i < plTimers->Num(); i++) {
      v.Set (&ppt[i]->m_varName);
      v.Fracture (); // so user cant change timer name
      pl->Add (&v, TRUE);
   } // i


   // add it
   pRet->SetList (pl);
   pl->Release();
}



/**********************************************************************************
CMIFLVMObject::TimerQuery - Querries information about a timer.

inputs
   PCMIFLVar            pVarName - looking for
   PCMIFLVar            pRet - Filled with NULL if can't find. If the timer
                           is found then it's filled with a list.
                           List[0] = TRUE if repeat, FALSE if not
                           List[1] = repeat time
                           List[2] = Function/method to call
                           List[3] = List of parameters
*/
void CMIFLVMObject::TimerQuery (PCMIFLVar pVarName, PCMIFLVar pRet)
{
   // get the timer
   DWORD dwIndex = TimerFind (pVarName);
   if (dwIndex == -1) {
      pRet->SetNULL();
      return;
   }

   // get it
   PCMIFLVMTimer pp;
   if (m_fTimerSuspended) {
      PCMIFLVMTimer *ppt = (PCMIFLVMTimer*)m_lPCMIFLVMTimer.Get(0);
      pp = ppt[dwIndex];
   }
   else
      pp = m_pVM->TimerGet (dwIndex);

   // create the list
   PCMIFLVarList pl = new CMIFLVarList;
   if (!pl) {
      pRet->SetUndefined();
      return;
   }

   CMIFLVar v;
   v.SetBOOL (pp->m_fRepeating);
   pl->Add (&v, TRUE);
   v.SetDouble (pp->m_fTimeRepeat);
   pl->Add (&v, TRUE);
   if (IsEqualGUID (pp->m_gCall, GUID_NULL))
      v.SetFunc (pp->m_dwCallID);
   else
      v.SetObjectMeth (&pp->m_gCall, pp->m_dwCallID);
   pl->Add (&v, TRUE);
   v.Set (&pp->m_varParams);
   v.Fracture();  // so that user can't modify parameters in place
   pl->Add (&v, TRUE);

   // add it
   pRet->SetList (pl);
   pl->Release();
}

