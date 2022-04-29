/************************************************************************
CWorld.cpp - Manages the "world", which is all the objects in the world.
Basically, it's a list of pointers to objects.

begun 12/9/01 by Mike Rozak
Copyright 2001 Mike Rozak. All rights reserved
*/

#include <windows.h>
#include <crtdbg.h>
#include "escarpment.h"
#include "..\M3D.h"
#include "resource.h"

static PWSTR gpszGFLLevel = L"GFLLevel%d";
static PWSTR gpszGFLHigher = L"GFLHigher";


// strings used for attributes in objects
/**************************************************************************************
CUndoPacket - Handles undo/redo storage */
typedef struct {
   DWORD             dwObject;            // Type of object.    see below
   DWORD             dwChange;            // chage. 0=changed,1=added,2=removed

   // dwObject == 0, world object
   PCObjectSocket    pObject;             // object changed. If Changed/removed this is
                                          // the contents right before changing/removing.
                                          // If Added, this is final result. Flip for Redo buffer
   GUID              gGUID;               // GUID of the object

   // dwObject = 1, attribute
   PWSTR             pszAttribName;       // attribute name
   PWSTR             pszAttribValue;      // attribute value

   // dwObject == 2, object surface
   PCObjectSurface   pSurf;               // object surface
} UNDOCHANGE, *PUNDOCHANGE;

class CUndoPacket {
public:
   ESCNEWDELETE;

   CUndoPacket (void);
   ~CUndoPacket (void);
   void Change (PCWorld pWorld, PCObjectSocket pObject, BOOL fBefore, DWORD dwChange);
   void Change (PCWorld pWorld, PWSTR pszName, BOOL fBefore, DWORD dwChange);
   void Change (PCWorld pWorld, PCObjectSurface pSurf, BOOL fBefore, DWORD dwChange);
   void FillSelectionList (PCWorld pWorld);

   CListFixed        m_lUNDOCHANGE;       // list of UNDOCHANGE structures
   CListFixed        m_lSelectGUID;       // list of GUIDs indicating which objects are selected

};
typedef CUndoPacket *PCUndoPacket;

/**************************************************************************************
CUndoPacket::Constructor and destructor. Initializes the object. The destructor
frees all the objects that are there.
*/
CUndoPacket::CUndoPacket ()
{
   m_lUNDOCHANGE.Init (sizeof(UNDOCHANGE));
   m_lSelectGUID.Init (sizeof(GUID));
}

CUndoPacket::~CUndoPacket ()
{
   DWORD i;
   for (i = 0; i < m_lUNDOCHANGE.Num(); i++) {
      PUNDOCHANGE pu = (PUNDOCHANGE) m_lUNDOCHANGE.Get(i);
      if (pu->pObject)
         pu->pObject->Delete();
      if (pu->pSurf)
         delete pu->pSurf;
      if (pu->pszAttribName)
         ESCFREE (pu->pszAttribName);
      if (pu->pszAttribValue)
         ESCFREE (pu->pszAttribValue);
   }
}


/**************************************************************************************
CUndoPacket::Change - Record a change. As objects are changed by the user this should
be called.

inputs
   PCWorld           pWorld - Used to get the selectin from
   PCObjectSocket    pObject - Object that has changed.
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
void CUndoPacket::Change (PCWorld pWorld, PCObjectSocket pObject, BOOL fBefore, DWORD dwChange)
{
   // get the object's GUID and see what was there before
   GUID g;
   pObject->GUIDGet (&g);

   // find a match
   DWORD i, dwUndo;
   PUNDOCHANGE pUndo;
   for (i = 0; i < m_lUNDOCHANGE.Num(); i++) {
      pUndo = (PUNDOCHANGE) m_lUNDOCHANGE.Get(i);
      if ((pUndo->dwObject == 0) && IsEqualGUID(pUndo->gGUID, g))
         break;
   }
   if (i >= m_lUNDOCHANGE.Num())
      pUndo = NULL;  // couldnt find
   dwUndo = i;

   UNDOCHANGE uc;
   memset (&uc, 0, sizeof(uc));
   uc.gGUID = g;
   //uc.pObject = pObject;
   uc.dwChange = dwChange;
   uc.dwObject = 0;

   if (dwChange == 0) { // changed
      if (!pUndo) {
         if (fBefore) {
            // consider capturing the selection
            if (!m_lUNDOCHANGE.Num())
               FillSelectionList(pWorld);

            // only add it if its market as the before event
            uc.pObject = pObject->Clone();
            if (!uc.pObject)
               return;  // didnt clone
            uc.pObject->WorldSet (NULL);  // so doesnt call back in
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

      // consider capturing the selection
      if (!m_lUNDOCHANGE.Num())
         FillSelectionList(pWorld);

      //uc.pObject = pObject->Clone();
      //uc.pObject->WorldSet (NULL);  // so doesnt call back in
      uc.pObject = NULL;   // since added it, don't really need to store this away. See undo later
      m_lUNDOCHANGE.Add (&uc);
      return;
   }
   else if (dwChange == 2) {  // removed

      // if it doesnt exist then clone and keep
      if (!pUndo) {
         // consider capturing the selection
         if (!m_lUNDOCHANGE.Num())
            FillSelectionList(pWorld);

         uc.pObject = pObject->Clone();
         if (!uc.pObject)
            return;
         uc.pObject->WorldSet (NULL);  // so doesnt call back in
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
         pUndo->pObject->Delete();
      m_lUNDOCHANGE.Remove (dwUndo);
      return;
   }
}



/**************************************************************************************
CUndoPacket::Change - Record a change. As objects are changed by the user this should
be called.

inputs
   PCWorld           pWorld - Used to get the selectin from
   PCObjectSurface   pSurf - Object that has changed.
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
void CUndoPacket::Change (PCWorld pWorld, PCObjectSurface pSurf, BOOL fBefore, DWORD dwChange)
{
   // get the object's GUID and see what was there before
   PWSTR pszName;
   pszName = pSurf->m_szScheme;
   if (!pszName[0])
      return; // cant tell

   // find a match
   DWORD i, dwUndo;
   PUNDOCHANGE pUndo;
   for (i = 0; i < m_lUNDOCHANGE.Num(); i++) {
      pUndo = (PUNDOCHANGE) m_lUNDOCHANGE.Get(i);
      if ((pUndo->dwObject != 2) || !pUndo->pSurf)
         continue;
      if (!_wcsicmp(pUndo->pSurf->m_szScheme, pSurf->m_szScheme))
         break;
   }
   if (i >= m_lUNDOCHANGE.Num())
      pUndo = NULL;  // couldnt find
   dwUndo = i;

   UNDOCHANGE uc;
   memset (&uc, 0, sizeof(uc));
   //uc.pSurf = pSurf;
   uc.dwChange = dwChange;
   uc.dwObject = 2;

   if (dwChange == 0) { // changed
      if (!pUndo) {
         if (fBefore) {
            // consider capturing the selection
            if (!m_lUNDOCHANGE.Num())
               FillSelectionList(pWorld);

            // only add it if its market as the before event
            uc.pSurf = pSurf->Clone();
            if (!uc.pSurf)
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

      // consider capturing the selection
      if (!m_lUNDOCHANGE.Num())
         FillSelectionList(pWorld);

      //uc.pSurf = pSurf->Clone();
      //uc.pSurf->WorldSet (NULL);  // so doesnt call back in
      uc.pSurf = NULL;   // since added it, don't really need to store this away. See undo later
      m_lUNDOCHANGE.Add (&uc);
      return;
   }
   else if (dwChange == 2) {  // removed

      // if it doesnt exist then clone and keep
      if (!pUndo) {
         // consider capturing the selection
         if (!m_lUNDOCHANGE.Num())
            FillSelectionList(pWorld);

         uc.pSurf = pSurf->Clone();
         if (!uc.pSurf)
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
      if (pUndo->pSurf)
         delete pUndo->pSurf;
      m_lUNDOCHANGE.Remove (dwUndo);
      return;
   }
}

/**************************************************************************************
CUndoPacket::Change - Record an attribute change. As objects are changed by the user this should
be called.

inputs
   PCWorld           pWorld - Used to get the selectin from
   PWSTR             pszName - Name of the attribute. Case sensative
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
void CUndoPacket::Change (PCWorld pWorld, PWSTR pszName, BOOL fBefore, DWORD dwChange)
{
   // find a match
   DWORD i, dwUndo;
   PUNDOCHANGE pUndo;
   for (i = 0; i < m_lUNDOCHANGE.Num(); i++) {
      pUndo = (PUNDOCHANGE) m_lUNDOCHANGE.Get(i);
      if ((pUndo->dwObject == 1) && !wcscmp(pszName, pUndo->pszAttribName))
         break;
   }
   if (i >= m_lUNDOCHANGE.Num())
      pUndo = NULL;  // couldnt find
   dwUndo = i;

   // get the value
   PWSTR pszCurVal;
   pszCurVal = pWorld->VariableGet(pszName);

   UNDOCHANGE uc;
   memset (&uc, 0, sizeof(uc));
   uc.dwChange = dwChange;
   uc.dwObject = 1;

   if (dwChange == 0) { // changed
      if (!pUndo) {
         if (fBefore) {
            uc.pszAttribName = (PWSTR) ESCMALLOC((wcslen(pszName)+1)*2);
            uc.pszAttribValue = (PWSTR) ESCMALLOC((wcslen(pszCurVal)+1)*2);
            if (!uc.pszAttribName || !uc.pszAttribValue) {
               if (uc.pszAttribName)
                  ESCFREE (uc.pszAttribName);
               if (uc.pszAttribValue)
                  ESCFREE (uc.pszAttribValue);
               return;
            }
            wcscpy (uc.pszAttribName, pszName);
            wcscpy (uc.pszAttribValue, pszCurVal);
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

      uc.pszAttribName = (PWSTR) ESCMALLOC((wcslen(pszName)+1)*2);
      uc.pszAttribValue = NULL;  // dont bother saving this since just added
      if (!uc.pszAttribName)
         return;
      wcscpy (uc.pszAttribName, pszName);
      m_lUNDOCHANGE.Add (&uc);
      return;
   }
   else if (dwChange == 2) {  // removed

      // if it doesnt exist then clone and keep
      if (!pUndo) {
         uc.pszAttribName = (PWSTR) ESCMALLOC((wcslen(pszName)+1)*2);
         uc.pszAttribValue = (PWSTR) ESCMALLOC((wcslen(pszCurVal)+1)*2);
         if (!uc.pszAttribName || !uc.pszAttribValue) {
            if (uc.pszAttribName)
               ESCFREE (uc.pszAttribName);
            if (uc.pszAttribValue)
               ESCFREE (uc.pszAttribValue);
            return;
         }
         wcscpy (uc.pszAttribName, pszName);
         wcscpy (uc.pszAttribValue, pszCurVal);
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
      if (pUndo->pszAttribName)
         ESCFREE (pUndo->pszAttribName);
      if (pUndo->pszAttribValue)
         ESCFREE (pUndo->pszAttribValue);
      m_lUNDOCHANGE.Remove (dwUndo);
      return;
   }
}

/************************************************************************************
CUndoPacket::FillSelectionList - Internal function called the FIRST time something
is really added to the list which Changed(). That way, when all those changes are
undone, the system will know what the selection was like at that point and can restore it.
*/
void CUndoPacket::FillSelectionList (PCWorld pWorld)
{
   m_lSelectGUID.Clear();

   DWORD i, dwNum, *pdw;
   pdw = pWorld->SelectionEnum(&dwNum);
   for (i = 0; i < dwNum; i++) {
      PCObjectSocket pObj = pWorld->ObjectGet (pdw[i]);
      GUID g;
      pObj->GUIDGet(&g);
      m_lSelectGUID.Add (&g);
   }
}



/**************************************************************************************
CWorld::Constructor and destructor
*/
CWorld::CWorld (void)
{
   m_dwRenderShard = (DWORD)-1;  // so will cause error if dont set render shard
   m_fKeepUndo = TRUE;
   m_szName[0] = 0;
   m_dwLastFind = -1;
   m_listOBJECTINFO.Init (sizeof(OBJECTINFO));
   m_listSelected.Init (sizeof(DWORD));
   m_listViewSocket.Init (sizeof(PCViewSocket));
   m_listPCUndoPacket.Init (sizeof(PCUndoPacket));
   m_listPCRedoPacket.Init (sizeof(PCUndoPacket));
   m_pUndoCur = new CUndoPacket;
   m_pOrigSurfaceScheme = new CSurfaceScheme;
   m_pSurfaceScheme = m_pOrigSurfaceScheme;
   m_fDirty = FALSE;
   m_pAux = new CMMLNode2;

   // BUGFIX - Setting the world
   m_pSurfaceScheme->WorldSet (this);

   // BUGFIX - call clear so goes global floor levels set
   Clear (FALSE);
}

CWorld::~CWorld (void)
{
   // delete undoredo
   m_fKeepUndo = FALSE; // BUGFIX - So freeing is faster
   UndoClear (TRUE, TRUE); // must be before the delete m_pUndoCur
   if (m_pUndoCur) {
      delete m_pUndoCur;
      m_pUndoCur = NULL;
   }

   // delete all objects
   DWORD i;
   for (i = 0; i < m_listOBJECTINFO.Num(); i++) {
      POBJECTINFO poi = (POBJECTINFO) m_listOBJECTINFO.Get(i);
      if (poi->pObject) {
         poi->pObject->WorldSet(NULL);
         poi->pObject->Delete();
      }
      if (poi->plSubObjectCorners)
         delete poi->plSubObjectCorners;
   }

   // free surface scheme
   if (m_pOrigSurfaceScheme)
      delete m_pOrigSurfaceScheme;

   if (m_pAux)
      delete m_pAux;
   m_pAux = NULL;
}

/**************************************************************************************
CWorld::DirtyGet - Gets the dirty flag. Use this to detect if should save
*/
BOOL CWorld::DirtyGet (void)
{
   return m_fDirty;
}

/**************************************************************************************
CWorld::DirtySet - Sets the dirty flag.
*/
void CWorld::DirtySet (BOOL fDirty)
{
   m_fDirty = fDirty;
}


/**************************************************************************************
CWorld::RenderShardSet - Sets the render shard
*/
void CWorld::RenderShardSet (DWORD dwRenderShard)
{
   m_dwRenderShard = dwRenderShard;
}

/**************************************************************************************
CWorld::RenderShardGet - Gets the render shard.
*/
DWORD CWorld::RenderShardGet (void)
{
   return m_dwRenderShard;
}

/**************************************************************************************
CWorld::NameSet - Sets the name of the world. The name is used for the file name.

inputs
   PWSTR    pszName - name
*/
void CWorld::NameSet (PWSTR pszName)
{
   wcscpy (m_szName, pszName);
}

/**************************************************************************************
CWorld::NameGet - Returns a pointer to the name of the world. DO NOT change this
pointer. If want to change call NameSet ()

returns
   PWSTR pszName
*/
PWSTR CWorld::NameGet (void)
{
   return m_szName;
}

/**************************************************************************************
CWorld::NotifySockets - An internal function called to notify all the sockets that
something has changed about the world. The views are all tied into this so they automagically
re-render.

inputs
   DWORD    dwChange - Bitfield of WORLDC_XXX
   GUID     *pgObject - To pass down to the notification sinks so know specifically what object changed
*/
void CWorld::NotifySockets (DWORD dwChange, GUID *pgObject)
{
   DWORD i, dwNum;
   PCViewSocket *pp;
   pp = (PCViewSocket*) m_listViewSocket.Get(0);
   dwNum = m_listViewSocket.Num();

   for (i = 0; i < dwNum; i++)
      (pp[i]->WorldChanged)(dwChange, pgObject);
}

/**************************************************************************************
CWorld::NotifySocketsUndo - An internal function called to notify all the sockets that
something has changed about the undo-stat of world. The views are all tied into this so they automagically
re-render.

inputs
   none
*/
void CWorld::NotifySocketsUndo (void)
{
   BOOL fUndo, fRedo;
   if (!m_fKeepUndo) // BUGFIX - Dont notify if not saving for undo
      return;

   fUndo = UndoQuery (&fRedo);

   DWORD i, dwNum;
   PCViewSocket *pp;
   pp = (PCViewSocket*) m_listViewSocket.Get(0);
   dwNum = m_listViewSocket.Num();

   for (i = 0; i < dwNum; i++)
      (pp[i]->WorldUndoChanged)(fUndo, fRedo);
}

/**************************************************************************************
CWorld::NotifySocketAdd - Adds a notification socket onto the end of the list. From then
on any changes to the world will get notified through this call.

inputs
   PCViewSocket     pAdd - Socket to add. Duplication testing is NOT done.
returns
   BOOL - TRUE if added
*/
BOOL CWorld::NotifySocketAdd (PCViewSocket pAdd)
{
   m_listViewSocket.Add (&pAdd);
   return TRUE;
}

/**************************************************************************************
CWorld::NotifySocketRemove - Removes the notification socket from the list.

inputs
   PCViewSocket     pRemove - Socket to remove
returns
   BOOL - TRUE if found and removed
*/
BOOL CWorld::NotifySocketRemove (PCViewSocket pRemove)
{
   DWORD i, dwNum;
   PCViewSocket *pp;
   pp = (PCViewSocket*) m_listViewSocket.Get(0);
   dwNum = m_listViewSocket.Num();

   for (i = 0; i < dwNum; i++)
      if (pp[i] == pRemove) {
         // found it
         m_listViewSocket.Remove (i);
         return TRUE;
      }
   return FALSE;
}


/**************************************************************************************
CWorld::ObjectNum - Returnst he number of objects in the world.
*/
DWORD CWorld::ObjectNum (void)
{
   return m_listOBJECTINFO.Num();
}


/**************************************************************************************
CWorld::ObjectGet - Given an object number (an index into the object list), this
returns the object socket. Do NOT delete the object using these. Use ObjectRemove().

inputs
   DWORD       dwObject - object number
returns
   PCObjectSocket - object. NULL if cant get
*/
PCObjectSocket CWorld::ObjectGet (DWORD dwObject)
{
   POBJECTINFO poi = (POBJECTINFO) m_listOBJECTINFO.Get(dwObject);
   if (!poi)
      return NULL;

   return poi->pObject;
}

/**************************************************************************************
CWorld::ObjectRemove - Given an object number (an index into the object list), this
deletes the object. It also changes the selection list (removing it from the selection)
AND notifies all the views that the world has changed.

inputs
   DWORD       dwObject - object number
returns
   BOOL - TRUE if deleted
*/
BOOL CWorld::ObjectRemove (DWORD dwObject)
{
   POBJECTINFO poi = (POBJECTINFO) m_listOBJECTINFO.Get(dwObject);
   if (!poi)
      return FALSE;

   // get the GUID
   GUID g;
   poi->pObject->GUIDGet(&g);


   // BUGFIX - If it's embedded tell the owner that it has been deleted
   GUID gCont;
   if (poi->pObject->EmbedContainerGet (&gCont)) {
      DWORD dwFind = ObjectFind (&gCont);
      PCObjectSocket pos;
      pos = (dwFind != -1) ? ObjectGet (dwFind) : NULL;
      if (pos) {
         GUID gEmbed;
         poi->pObject->GUIDGet (&gEmbed);
         pos->ContEmbeddedRemove (&gEmbed);
      }
   }

   m_fDirty = TRUE;

   // remember that this has been removed
   if (m_pUndoCur && m_fKeepUndo)
      m_pUndoCur->Change (this, poi->pObject, TRUE, 2);
   // Send a notification that the undo state has changed
   NotifySocketsUndo ();

   // remove from the selections list
   SelectionRemove (dwObject);

   // BUGFIX - close the editor window
   poi->pObject->EditorDestroy ();

   // delete
   poi->pObject->Delete();
   if (poi->plSubObjectCorners)
      delete poi->plSubObjectCorners;
   
   // remove from the object list
   m_listOBJECTINFO.Remove (dwObject);

   // because changing the number of objects reduces object numbers above this
   DWORD dwNum, i, *pdw;
   dwNum = m_listSelected.Num();
   pdw = (DWORD*) m_listSelected.Get(0);
   for (i = 0; i < dwNum; i++)
      if (pdw[i] > dwObject)
         pdw[i]--;

   // Notify application of object deletion
   NotifySockets (WORLDC_OBJECTREMOVE, &g);

   return TRUE;
}


/**************************************************************************************
CWorld::SelectionRemove - Removes the object from the selection list. It also notifies
the application that the selection list has changed.

inputs
   DWORD    dwObject - object to remove
returns
   BOOL - TRUE if found and removed
*/
BOOL CWorld::SelectionRemove (DWORD dwObject)
{
   // specifically dont change dirty flag

   // to a linear search for it
   DWORD *pdw, dwNum, i;
   pdw = SelectionEnum (&dwNum);
   for (i = 0; i < dwNum; i++)
      if (pdw[i] == dwObject) {
         m_listSelected.Remove (i);
         // Notify app
         NotifySockets (WORLDC_SELREMOVE, NULL);
         return TRUE;
      }


      // else cant find
   return FALSE;
}

/**************************************************************************************
CWorld::SelectionAdd - Adds the object to the list. If it already exists then nothing
it added.

inputs
   DWORD       dwObject - Object index.
returns
   BOOL - TRUE if added or already exists. FALSE if error
*/
BOOL CWorld::SelectionAdd (DWORD dwObject)
{
   // if not real then error
   if (!ObjectGet (dwObject))
      return FALSE;

   // specifically dont change dirty flag

   // to a linear search for it
   DWORD *pdw, dwNum, i;
   pdw = SelectionEnum (&dwNum);
   for (i = 0; i < dwNum; i++) {
      if (pdw[i] == dwObject)
         return TRUE;
      
      if (pdw[i] < dwObject)
         continue;   // try again
      
      // else, insert before this
      m_listSelected.Insert (i, &dwObject);
      // notify app
      NotifySockets (WORLDC_SELADD, NULL);
      return TRUE;
   }

   // else, add to the end of the list
   m_listSelected.Add (&dwObject);
   // Notify app
   NotifySockets (WORLDC_SELADD, NULL);
   return TRUE;
}

/**************************************************************************************
CWorld::SelectionExists - Returns true if the object is selected

inputs
   DWORD       dwObject - object to look for
returns
   BOOL - TRUE if exists
*/
static int _cdecl BCompare (const void *elem1, const void *elem2)
{
   DWORD *pdw1, *pdw2;
   pdw1 = (DWORD*) elem1;
   pdw2 = (DWORD*) elem2;

   return (int) (*pdw1) - (int)(*pdw2);
}

BOOL CWorld::SelectionExists (DWORD dwObject)
{
   DWORD *pdw, *list, dwNum;
   list = SelectionEnum (&dwNum);
   pdw = (DWORD*) bsearch (&dwObject, list, dwNum, sizeof(DWORD), BCompare);
   return pdw ? TRUE : FALSE;
}


/**************************************************************************************
CWorld::SelectionEnum - Returns a pointer to a sorted array of DWORDs, which are the
object numbers currently selected. Don't change these. It's only valid until the next
call to SelectionXXX (internally or externally).

inputs
   DWORD    *pdwNum - Filled with the number of elements selected
returns
   DWORD * - Pointer to an array of DWORDs. *pdwNum entries.
*/
DWORD *CWorld::SelectionEnum (DWORD *pdwNum)
{
   *pdwNum = m_listSelected.Num();
   return (DWORD*) m_listSelected.Get(0);
}

/**************************************************************************************
CWorld::SelectionClear - Removes all the objects from the selection.

returns
   BOOL - TRUE if there were objects removed. FALSE if it was already empty
*/
BOOL CWorld::SelectionClear (void)
{
   // specifically dont change dirty flag

   if (!m_listSelected.Num())
      return FALSE;

   m_listSelected.Clear ();
   NotifySockets (WORLDC_SELREMOVE, NULL);
   return TRUE;
}



/**************************************************************************************
CWorld::ObjectAdd - Adds a new object to the world list. Once the object is added, its
up to the world object to delete the object.

NOTE: This does NOT call WorldSetFinished() for the object. Whomever calls objectadd
should call WorldSetFinished().

inputs
   PCObjectSocket       pObject - object to add
   BOOL                 fDontRememberForUndo - If TRUE, dont remember this for undo - so
                        wont be deleted if undo. Only use for cameras
   GUID*                pgID - ID to use. Usually NULL, which means automatically generate
returns
   DWORD - Added as this number. Returns -1 if can't add
*/
DWORD CWorld::ObjectAdd (PCObjectSocket pObject, BOOL fDontRememberForUndo, GUID *pgID)
{
   _ASSERTE (m_dwRenderShard != (DWORD)-1);

   if (!fDontRememberForUndo)
      m_fDirty = TRUE;

   OBJECTINFO oi;
   memset (&oi, 0, sizeof(oi));
   oi.pObject = pObject;

   // generate a GIUD
   if (pgID)
      oi.gGUID = *pgID;
   else
      GUIDGen (&oi.gGUID);
   GUID gCur;
   pObject->GUIDGet (&gCur);
   if (!IsEqualGUID (gCur, oi.gGUID))
      pObject->GUIDSet (&oi.gGUID);
   pObject->GUIDGet (&gCur);

   DWORD dw;
   dw = m_listOBJECTINFO.Add (&oi);
   if (dw == (DWORD)-1)
      return dw;

   // notify the object who dealing with
   pObject->WorldSet (this);

   if (!fDontRememberForUndo) {
      // remember this for undo/redo
      if (m_pUndoCur && m_fKeepUndo)
         m_pUndoCur->Change (this, pObject, FALSE, 1);
      // Send a notification that the undo state has changed
      NotifySocketsUndo ();
   }

   NotifySockets (WORLDC_OBJECTADD, &gCur);

   return dw;
}

/**************************************************************************************
CWorld::BoundingBoxGet - Fills in pm, pCorner1, and pCorner2 with the bounding box
of the object.

inputs
   DWORD       dwObject - Object index
   CMatrix     *pm - Filled in with a matrix that translates the object from the object's
                     internal coordinates (aka the coordinates of the box) to world space.
                     The box's 8 corners should be multiplied by this matrix to get
                     a bounding area in world space.
   PCPoint     pCorner1, pCorner2 - Two opposite corners of the box. All 8 corners
                     can be derived by mixing and matching points
   DWORD       dwSubObject - Sub-object number to get. -1 for all sub-objects, or specific sub-object
returns
   BOOL - TRUE if got the bounding box. FALSE if couldnt
*/
BOOL CWorld::BoundingBoxGet (DWORD dwObject, CMatrix *pm, PCPoint pCorner1, PCPoint pCorner2,
                             DWORD dwSubObject)
{
   // get the object
   POBJECTINFO poi = (POBJECTINFO) m_listOBJECTINFO.Get(dwObject);
   if (!poi)
      return FALSE;

   // if the data isn't valid then get some valid data
   if (!poi->fIsBoundingBoxValid) {
      // get some basics
      poi->pObject->ObjectMatrixGet (&poi->mBoundingMatrix);
      poi->fIsBoundingBoxValid = TRUE;

      DWORD dwSubObjects = poi->pObject->QuerySubObjects();
      if (dwSubObjects) {
         if (!poi->plSubObjectCorners)
            poi->plSubObjectCorners = new CListFixed;
         if (!poi->plSubObjectCorners)
            return FALSE;
         poi->plSubObjectCorners->Init (sizeof(CPoint)*2);

         DWORD i;
         CPoint ap[2];
         poi->Corner[0].Zero();
         poi->Corner[1].Zero();
         BOOL fFound = FALSE;
         for (i = 0; i < dwSubObjects; i++) {
            poi->pObject->QueryBoundingBox (&ap[0], &ap[1], i);

            // add
            poi->plSubObjectCorners->Add (&ap[0]);

            // if the points are the same then just ignore. Caves, for example,
            // might return bounding boxes of 0-size
            if ((ap[0].p[0] == ap[1].p[0]) &&
               (ap[0].p[1] == ap[1].p[1]) &&
               (ap[0].p[2] == ap[1].p[2]))
               continue;

            // do min/max... since query bounding box doesnt define which is
            // min/max, make sure
            if (fFound) {
               poi->Corner[0].Min (&ap[0]);
               poi->Corner[0].Min (&ap[1]);
               poi->Corner[1].Max (&ap[0]);
               poi->Corner[1].Max (&ap[1]);
            }
            else {
               poi->Corner[0].Copy (&ap[0]);
               poi->Corner[1].Copy (&ap[0]);
               poi->Corner[0].Min (&ap[1]);
               poi->Corner[1].Max (&ap[1]);
               fFound = TRUE;
            }
         }

      }
      else {
         poi->pObject->QueryBoundingBox (&poi->Corner[0], &poi->Corner[1], (DWORD)-1);
         if (poi->plSubObjectCorners)
            delete poi->plSubObjectCorners;
      }

#ifdef _TIMERS
      // NOTE: DIsable this so it will link easier... gRenderStats.dwBoundingBoxGet++;
#endif
   }

   pm->Copy (&poi->mBoundingMatrix);
   if (dwSubObject == (DWORD)-1) {
      // have valid bounding box
      pCorner1->Copy (&poi->Corner[0]);
      pCorner2->Copy (&poi->Corner[1]);
   }
   else {
      if (!poi->plSubObjectCorners || (dwSubObject >= poi->plSubObjectCorners->Num()))
         return FALSE;

      PCPoint p = (PCPoint)poi->plSubObjectCorners->Get(dwSubObject);
      pCorner1->Copy (&p[0]);
      pCorner2->Copy (&p[1]);
   }

   // done
   return TRUE;

}

/********************************************************************************
CWorld::SurfaceAboutToChange - Call before one of the global surface/texture
settings is about to change.

inputs
   PWSTR    pszScheme - Surface scheme
returns
   none
*/
void CWorld::SurfaceAboutToChange (PWSTR pszScheme)
{
   m_fDirty = TRUE;

   PCObjectSurface pObject;
   pObject = m_pSurfaceScheme->SurfaceGet (pszScheme, NULL, FALSE);

   // cache away for undo/redo
   if (m_pUndoCur && m_fKeepUndo)
      m_pUndoCur->Change (this, pObject, TRUE, 0);

   delete pObject;
   // Send a notification that the undo state has changed
   NotifySocketsUndo ();
}

/********************************************************************************
CWorld::SurfaceChanged - Called by an object after it has changed. (Must have called
ObjectAboutToChange before this.) The world object uses this to notify the views
that the object has changed.

inputs
   PWSTR    pszScheme - Surface scheme
returns
   none
  */
void CWorld::SurfaceChanged (PWSTR pszScheme)
{
   m_fDirty = TRUE;

   PCObjectSurface pObject;
   pObject = m_pSurfaceScheme->SurfaceGet (pszScheme, NULL, FALSE);

   // remember this for undo/redo
   if (m_pUndoCur && m_fKeepUndo)
      m_pUndoCur->Change (this, pObject, FALSE, 0);

   delete pObject;

   // Send a notification that the undo state has changed
   NotifySocketsUndo ();

   // notify sockets
   NotifySockets (WORLDC_SURFACECHANGED, NULL);
}

/********************************************************************************
CWorld::ObjectAboutToChange - Called by an object when it's about to change - such
as be moved, rotated, colored, etc. It calls into the world object to warn of the
impending change so the world object has the ability to clone the existing version
of the object for undo reasons. THe object MUST call ObjectChanged() soon after
calling this.

inputs
   PCObjectSocket    pObject - object
retursn
   none
*/
void CWorld::ObjectAboutToChange (PCObjectSocket pObject)
{
   m_fDirty = TRUE;

   // cache away for undo/redo
   if (m_pUndoCur && m_fKeepUndo)
      m_pUndoCur->Change (this, pObject, TRUE, 0);
   // Send a notification that the undo state has changed
   NotifySocketsUndo ();

   // notify sockets that object is about to change
   GUID g;
   pObject->GUIDGet (&g);
   DWORD i, dwNum;
   PCViewSocket *pp;
   pp = (PCViewSocket*) m_listViewSocket.Get(0);
   dwNum = m_listViewSocket.Num();

   for (i = 0; i < dwNum; i++)
      (pp[i]->WorldAboutToChange)(&g);
}


/********************************************************************************
CWorld::ObjectChanged - Called by an object after it has changed. (Must have called
ObjectAboutToChange before this.) The world object uses this to notify the views
that the object has changed.

inputs
   PCObjectSocket    pObject - object
returns
   none
  */
void CWorld::ObjectChanged (PCObjectSocket pObject)
{
   m_fDirty = TRUE;

   // Find out if the object is selected and send notification based on that
   DWORD i;
   i = ObjectFind (pObject);
   if (i != (DWORD)-1) {
      // note that bounding box is invalid now
      POBJECTINFO p = (POBJECTINFO) m_listOBJECTINFO.Get(i);
      p->fIsBoundingBoxValid = FALSE;

      // remember this for undo/redo
      if (m_pUndoCur && m_fKeepUndo)
         m_pUndoCur->Change (this, pObject, FALSE, 0);
      // Send a notification that the undo state has changed
      NotifySocketsUndo ();

      GUID g;
      pObject->GUIDGet (&g);

      // notify sockets
      // special case if it's a camera so that dont redraw light buffers if cameras move
      OSINFO info;
      p->pObject->InfoGet(&info);
      if (IsEqualGUID(info.gCode, CLSID_Camera))
         NotifySockets (SelectionExists(i) ? WORLDC_CAMERACHANGESEL : WORLDC_CAMERACHANGENON, &g);
      else
         NotifySockets (SelectionExists(i) ? WORLDC_OBJECTCHANGESEL : WORLDC_OBJECTCHANGENON, &g);
   }
}

/********************************************************************************
CWorld::ObjectFind - Find an objects from it's pObject number. Returns an index
into the object list, or -1 if can't find

inputs
   PCObjectSocket    pObject - To look for
returns
   DWORD - Index into object. -1 if cant find
*/
DWORD CWorld::ObjectFind (PCObjectSocket pObject)
{
   DWORD i;
   for (i = 0; i < m_listOBJECTINFO.Num(); i++) {
      POBJECTINFO p = (POBJECTINFO) m_listOBJECTINFO.Get(i);
      if (p->pObject == pObject)
         return i;
   }
   
   return -1;
}

/********************************************************************************
CWorld::ObjectFind - Find an objects from it's GUID number. Returns an index
into the object list, or -1 if can't find

inputs
   GUID     *pg - GUID
returns
   DWORD - Index into object. -1 if cant find
*/
DWORD CWorld::ObjectFind (GUID *pg)
{
   DWORD i;
   // BUGFIX - Speed up object find when called repeatedly for smae object
   if (m_dwLastFind < m_listOBJECTINFO.Num()) {
      POBJECTINFO p = (POBJECTINFO) m_listOBJECTINFO.Get(m_dwLastFind);
      if (IsEqualGUID(p->gGUID,*pg)) {
         return m_dwLastFind;
      }
   }

   for (i = 0; i < m_listOBJECTINFO.Num(); i++) {
      POBJECTINFO p = (POBJECTINFO) m_listOBJECTINFO.Get(i);
      if (IsEqualGUID(p->gGUID,*pg)) {
         m_dwLastFind = i;
         return i;
      }
   }
   
   return -1;
}

/*********************************************************************************
GUIDGen - Generate a unique GUID for the world object.

inputws
   GUID        *pg - GUID
returns
   none
*/
void GUIDGen (GUID *pg)
{
   static FILETIME sft;
   static DWORD sdwTick = 0;
   static DWORD sdwGUIDCount = 0;
   static BOOL fHaveFileTime = FALSE;
   if (!fHaveFileTime) {
      fHaveFileTime = TRUE;
      GetSystemTimeAsFileTime (&sft);

      // BUGFIX - More random tick count
      LARGE_INTEGER liCount;
      QueryPerformanceCounter (&liCount);
      sdwTick = liCount.HighPart ^ liCount.LowPart;

      sdwGUIDCount = Now();
   }

   // clear it out
   memset (pg, 0, sizeof(*pg));
   
   // fill in some values
   DWORD *pdw;
   pdw = (DWORD*) pg;
   *(pdw++) = sdwGUIDCount++;
   *(pdw++) = sdwTick;
   *((LPFILETIME) pdw) = sft;

   // make sure that if if increment the guid count enough that
   // tick is then incremented. Ensure that don't get rollover if create
   // 4 billion objects in a session
   if (!sdwGUIDCount)
      sdwTick++;
}

/**************************************************************************************
CWorld::UndoClear - Erase all the contents of the undo/redo buffers.

inputs
   BOOL        fUndo - Remove the undo buffers
   BOOL        fRedo - Remove the redo buffers
returns
   none
*/
void CWorld::UndoClear (BOOL fUndo, BOOL fRedo)
{
   PCUndoPacket pup;
   DWORD i;
   if (fUndo) {
      // current undo
      if (m_pUndoCur && m_pUndoCur->m_lUNDOCHANGE.Num()) {
         delete m_pUndoCur;
         m_pUndoCur = NULL;
      }
      if (!m_pUndoCur)
         m_pUndoCur = new CUndoPacket;

      // anything in the list
      for (i = 0; i < m_listPCUndoPacket.Num(); i++) {
         pup =* ((PCUndoPacket*) m_listPCUndoPacket.Get(i));
         delete pup;
      }
      m_listPCUndoPacket.Clear();
   }

   if (fRedo) {
      // anything in the list
      for (i = 0; i < m_listPCRedoPacket.Num(); i++) {
         pup = *((PCUndoPacket*) m_listPCRedoPacket.Get(i));
         delete pup;
      }
      m_listPCRedoPacket.Clear();
   }

   // BUGFIX - Notify the sockets of this
   NotifySocketsUndo ();
}


/**************************************************************************************
CWorld::UndoRemember - Remember this point in time as an undo point. The current undo
object is added onto the undo list.

inputs
   none
returns
   none
*/
void CWorld::UndoRemember (void)
{
   // if there arent any accumulated changes then ignore
   if (!m_pUndoCur->m_lUNDOCHANGE.Num())
      return;

   // clear the redo list since made changes
   UndoClear (FALSE, TRUE);

   // if the undo list is already too long then remove an item
   if (m_listPCUndoPacket.Num() > 100) {
      PCUndoPacket p = *((PCUndoPacket*) m_listPCUndoPacket.Get(0));
      delete p;
      m_listPCUndoPacket.Remove(0);
   }

   // append the current undo
   m_listPCUndoPacket.Add (&m_pUndoCur);

   // create a new current packet
   m_pUndoCur = new CUndoPacket;

   // Send a notification that the undo state has changed
   NotifySocketsUndo ();
}



/************************************************************************************
CWorld::UndoQuery - Returns TRUE if there's something in the undo buffer. Also
filles in a flag if there's anything in the redo buffer.

inputs
   BOOL        *pfRedo - If not NULL, fills in true if can redo.
*/
BOOL CWorld::UndoQuery (BOOL *pfRedo)
{
   BOOL fUndo;
   fUndo = (m_listPCUndoPacket.Num() ? TRUE : FALSE) || (m_pUndoCur->m_lUNDOCHANGE.Num() ? TRUE : FALSE);

   if (pfRedo)
      *pfRedo = (!m_pUndoCur->m_lUNDOCHANGE.Num()) && (m_listPCRedoPacket.Num() ? TRUE : FALSE);

   return fUndo;
}


/************************************************************************************
CWorld::Undo - Undoes the last changes.

inputs
   BOOL     fUndo - Use TRUE to Undo. FALSE to Redo.
returns
   BOOL - TRUE if succeded. FALSE if error
*/
BOOL CWorld::Undo (BOOL fUndoIt)
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

         PCUndoPacket p;
         p = *((PCUndoPacket*) m_listPCUndoPacket.Get(dwNum-1));
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

      PCUndoPacket p;
      p = *((PCUndoPacket*) m_listPCRedoPacket.Get(dwNum-1));
      m_listPCRedoPacket.Remove(dwNum-1);

      delete m_pUndoCur;
      m_pUndoCur = p;
   }

   // remeber the current selection
   CUndoPacket UPSel;
   UPSel.FillSelectionList (this);

   // go through all the elements  and doo whatever replacing is necessary.
   // because we're also going to convert this information for the redo
   // buffer, 
   DWORD i;
   DWORD dwAdded, dwRemoved, dwChanged;
   dwAdded = dwRemoved = dwChanged = 0;
   for (i = 0; i < m_pUndoCur->m_lUNDOCHANGE.Num(); i++) {
      PUNDOCHANGE puc = (PUNDOCHANGE) m_pUndoCur->m_lUNDOCHANGE.Get(i);

      if (puc->dwObject == 0) {  // objects
         // see if can find he same object in the current list
         DWORD dwIndexCur;
         POBJECTINFO poi;
         PCObjectSocket psockTemp;
         dwIndexCur = ObjectFind (&puc->gGUID);
         if (dwIndexCur != (DWORD) -1)
            poi = (POBJECTINFO) m_listOBJECTINFO.Get(dwIndexCur);
         else
            poi = NULL;

         switch (puc->dwChange) {
         case 0:  // changed
            {
               // we had better find it
               if (!poi)
                  break;

               // this was changed between versions so do a simple swap
               psockTemp = poi->pObject;
               psockTemp->WorldSet (NULL);

               poi->pObject = puc->pObject;
               puc->pObject = psockTemp;
               poi->fIsBoundingBoxValid = FALSE;   // because changed
               poi->pObject->WorldSet (this);

               dwChanged++;
            }
            break;
         case 1:  // added
            {
               // we had better find it
               if (!poi)
                  break;

               // remember the object as it is
               if (puc->pObject)
                  puc->pObject->Delete();
               puc->pObject = poi->pObject;
               puc->pObject->WorldSet (NULL);
               puc->dwChange = 2;   // we're now a remove for the redo

               if (poi->plSubObjectCorners)
                  delete poi->plSubObjectCorners;

               // we added this object since the last undo. Therefore, remove it.
               m_listOBJECTINFO.Remove(dwIndexCur);

               dwAdded++;
            }
            break;
         case 2:  // removed
            {
               // had better not find it in th elist
               if (poi)
                  break;

               // we removed it since the last undo, therefore add it back
               OBJECTINFO oi;
               memset (&oi, 0, sizeof(oi));
               oi.gGUID = puc->gGUID;
               oi.fIsBoundingBoxValid = FALSE;
               oi.pObject = puc->pObject;
               puc->pObject = NULL;
               puc->dwChange = 1;   // we're now an add for the redo
               oi.pObject->WorldSet (this);
               m_listOBJECTINFO.Add (&oi);

               dwRemoved++;
            }
            break;
         }
      }
      else if (puc->dwObject == 1) {  // variables
         // see if can find he same variable in the current list
         DWORD dwIndexCur, i;
         PWSTR pszName, pszData;
         dwIndexCur = -1;
         pszName = pszData = NULL;
         for (i = 0; i < m_listVarName.Num(); i++) {
            pszName = (PWSTR) m_listVarName.Get(i);
            if (!wcscmp (pszName, puc->pszAttribName)) {
               pszData = (PWSTR) m_listVarData.Get(i);
               dwIndexCur = i;
               break;
            }
         }
         if (dwIndexCur == -1)
            pszName = pszData = NULL;

         switch (puc->dwChange) {
         case 0:  // changed
            {
               // we had better find it
               if (!pszName || !pszData || !puc->pszAttribValue)
                  break;

               // this was changed between versions so do a simple swap
               PWSTR pszNew;
               pszNew = (PWSTR) ESCMALLOC ((wcslen(pszData)+1)*2);
               if (!pszNew)
                  break;
               wcscpy (pszNew, pszData);

               m_listVarData.Set (dwIndexCur, puc->pszAttribValue, (wcslen(puc->pszAttribValue)+1)*2);
               ESCFREE (puc->pszAttribValue);
               puc->pszAttribValue = pszNew;

               dwChanged++;
            }
            break;
         case 1:  // added
            {
               // we had better find it
               if (!pszName || !pszData)
                  break;

               // remember this
               PWSTR pszNew;
               pszNew = (PWSTR) ESCMALLOC ((wcslen(pszData)+1)*2);
               if (!pszNew)
                  break;
               wcscpy (pszNew, pszData);
               if (puc->pszAttribValue)
                  ESCFREE (puc->pszAttribValue);
               puc->pszAttribValue = pszNew;

               puc->dwChange = 2;   // we're now a remove for the redo

               // we added this object since the last undo. Therefore, remove it.
               m_listVarName.Remove (dwIndexCur);
               m_listVarData.Remove (dwIndexCur);

               dwAdded++;
            }
            break;
         case 2:  // removed
            {
               // had better not find it in th elist
               if (pszName || pszData)
                  break;

               // we removed it since the last undo, therefore add it back
               m_listVarName.Add (puc->pszAttribName, (wcslen(puc->pszAttribName)+1)*2);
               m_listVarData.Add (puc->pszAttribValue, (wcslen(puc->pszAttribValue)+1)*2);

               if (puc->pszAttribValue)
                  ESCFREE (puc->pszAttribValue);
               puc->pszAttribValue = NULL;
               puc->dwChange = 1;   // we're now an add for the redo

               dwRemoved++;
            }
            break;
         }  // switch(dwChanged)
      }
      else if (puc->dwObject == 2) {  // textures
         // see if can find he same object in the current list
         PCObjectSurface pExisting;
         if (puc->pSurf)
            pExisting = m_pSurfaceScheme->SurfaceGet (puc->pSurf->m_szScheme, NULL, FALSE);

         switch (puc->dwChange) {
         case 0:  // changed
            {
               // we had better find it
               if (!pExisting)
                  break;

               if (puc->pSurf) {
                  m_pSurfaceScheme->SurfaceSet (puc->pSurf);
                  delete puc->pSurf;
               }
               puc->pSurf = pExisting;

               dwChanged++;
            }
            break;
         case 1:  // added
            {
               // we had better find it
               if (!pExisting)
                  break;

               // remember the object as it is
               if (puc->pSurf)
                  delete puc->pSurf;
               puc->pSurf = pExisting;
               puc->dwChange = 2;   // we're now a remove for the redo

               dwAdded++;
            }
            break;
         case 2:  // removed
            {
               // had better not find it in th elist
               if (pExisting) {
                  delete pExisting;
                  break;
               }


               m_pSurfaceScheme->SurfaceSet (puc->pSurf);
               delete puc->pSurf;
               puc->pSurf = NULL;
               puc->dwChange = 1;   // we're now an add for the redo

               dwRemoved++;
            }
            break;
         }
      }

   }

   // put a new undo packet in
   PCUndoPacket pOld;
   pOld = m_pUndoCur;
   // make a new m_pUndoCur
   m_pUndoCur = new CUndoPacket;

   // set the selection in the world
   SelectionClear ();
   for (i = 0; i < pOld->m_lSelectGUID.Num(); i++) {
      GUID *pg = (GUID*) pOld->m_lSelectGUID.Get(i);
      DWORD dwIndex;
      dwIndex = ObjectFind (pg);
      if (dwIndex != (DWORD) -1)
         SelectionAdd (dwIndex);
   }


   // copy the old selection info to overwrite the current selection
   pOld->m_lSelectGUID.Init (sizeof(GUID), UPSel.m_lSelectGUID.Get(0), UPSel.m_lSelectGUID.Num());

   // move pOld into the redo buffer
   if (fUndoIt)
      m_listPCRedoPacket.Add (&pOld);
   else
      m_listPCUndoPacket.Add (&pOld);


   // remember that dirty
   m_fDirty = TRUE;

   // notify the world of new objects and selections
   NotifySockets ((dwAdded ? WORLDC_OBJECTADD : 0) | (dwRemoved ? WORLDC_OBJECTREMOVE : 0) |
      (dwChanged ? (WORLDC_OBJECTCHANGESEL | WORLDC_OBJECTCHANGENON) : 0), NULL );
      // NOTE: Passing in NULL because too many objects have changed

   // notify the world that the undo buffer has changed
   NotifySocketsUndo ();

   return TRUE;
}



/************************************************************************************
CWorld::Clear - Clears everything out of the world. Usually used to clear purgatory
before it's used.

inputs
   BOOL fOnlyObjects - If TRUE, it clears only the objects. FALSE, wipes everything.
returns
   none
*/
void CWorld::Clear (BOOL fOnlyObjects)
{
   // BUGFIX - Don't keep undo around if going to clear the undo buffer
   BOOL fUndo = m_fKeepUndo;
   if (!fOnlyObjects)
      m_fKeepUndo = FALSE;

   while (ObjectNum()) {
      ObjectRemove (0);
   }

   if (fOnlyObjects)
      return;

   // clear the rest
   UndoClear (TRUE, TRUE); // must be before the delete m_pUndoCur
   m_fDirty = FALSE;

   // surfaces
   m_pSurfaceScheme->Clear();

   // selection
   m_listSelected.Clear();

   // variables
   m_listVarData.Clear();
   m_listVarName.Clear();

   // aux information
   if (m_pAux)
      delete m_pAux;
   m_pAux = new CMMLNode2;



   // BUGFIX - Fix levels to use country code
   // set the levels to some default
   //fp fEl[NUMLEVELS];
   //fEl[0] = 1 - FLOORHEIGHT_TROPICAL;
   //DWORD i;
   //for (i = 1; i < NUMLEVELS; i++)
   //   fEl[i] = fEl[i-1] + FLOORHEIGHT_TROPICAL;
   //GlobalFloorLevelsSet (this, NULL, fEl, FLOORHEIGHT_TROPICAL);
   DWORD dwClimate, dwFoundation;
   DefaultBuildingSettings (NULL, NULL, NULL, &dwClimate, NULL, NULL, &dwFoundation);  // BUGFIX - extenral
   fp fFloorHeight;
   switch (dwClimate) {
   case 0:  // tropical
      fFloorHeight = FLOORHEIGHT_TROPICAL;
      break;
   case 1:  // sub-tropical
   case 5:  // arid
   case 6:  // mediterraneous
      fFloorHeight = (FLOORHEIGHT_TEMPERATE + FLOORHEIGHT_TROPICAL) / 2;
      break;
   default:
      fFloorHeight = FLOORHEIGHT_TEMPERATE;
      break;
   }

   // ground floor is usually at 1m, except when pad, then .1m
   fp fGround;
   fGround = (dwFoundation == 2) ? .1 : 1;

   // make the levels
   fp afLevels[NUMLEVELS];
   afLevels[0] = fGround - fFloorHeight;
   DWORD i;
   for (i = 1; i < NUMLEVELS; i++)
      afLevels[i] = afLevels[i-1] + fFloorHeight;

   // set it out
   GlobalFloorLevelsSet (this, NULL, afLevels, fFloorHeight);

   m_fKeepUndo = fUndo;
}

/***********************************************************************************
CWorld::UsurpObject - Generally not called by anyone except another world.
This removes the object (identified by dwObject) from the world's list without
telling the object itself that it has been removed, and without updating undo/redo
in the object.

inputs
   DWORD       dwObject - Object index to remove
returns
   PCObjectSocket - Object. It's the caller's responsibility to free this.
*/
PCObjectSocket CWorld::UsurpObject (DWORD dwObject)
{
   POBJECTINFO poi = (POBJECTINFO) m_listOBJECTINFO.Get(dwObject);
   if (!poi)
      return NULL;
   PCObjectSocket pObject = poi->pObject;
   if (!pObject)
      return NULL;
   if (poi->plSubObjectCorners)
      delete poi->plSubObjectCorners;
   m_listOBJECTINFO.Remove(dwObject);

   return pObject;
}

/***********************************************************************************
CWorld::UsurpWorld - This takes all the objects from one world and moves them
to the current world. Its used to move objects to be pasted from purgatory into
the real world.

inptus
   CWOrld      *pUsurp - Take from this world
   BOOL        fSelect - If TRUE then select all the usurped objects
retunrs
   none
*/
void CWorld::UsurpWorld (CWorldSocket *pUsurp, BOOL fSelect)
{

   // clear out the selection in the usurped world since they'll all be gone
   pUsurp->SelectionClear();

   // keep track of the objects that have done world set to
   CListFixed lWorldSet;
   lWorldSet.Init (sizeof(PCObjectSocket));


   // go through all the objects
   DWORD dw;
   while (pUsurp->ObjectNum()) {
      // BUGFIX - Changed to use world socket
      OBJECTINFO oi;
      PCObjectSocket pObject;
      memset (&oi, 0, sizeof(oi));
      pObject = pUsurp->UsurpObject (0);
      if (!pObject)
         break;
      pObject->GUIDGet (&oi.gGUID);
      oi.pObject = pObject;
      dw = m_listOBJECTINFO.Add (&oi);
      pObject->WorldSet (this);
      lWorldSet.Add (&pObject);

      // remember this for undo/redo
      if (m_pUndoCur && m_fKeepUndo)
         m_pUndoCur->Change (this, pObject, FALSE, 1);

      // select
      SelectionAdd (dw);
   }

   // go through all the ones that have added and do WorldSetFInished()
   // so they can update
   DWORD i;
   for (i = 0; i < lWorldSet.Num(); i++) {
      PCObjectSocket p = *((PCObjectSocket*) lWorldSet.Get(i));
      p->WorldSetFinished();
   }
}

/**********************************************************************************
CWorld::SurfaceSchemeGet - Returns a pointer to the surface scheme the world is
using. DO NOT delete it. Just use it for reference.
*/
PCSurfaceSchemeSocket CWorld::SurfaceSchemeGet (void)
{
   return m_pSurfaceScheme;
}


/**********************************************************************************
CWorld::SurfaceSchemeSet - Changes the world to use a different surface shceme
than the one it strarts with. NOTE: If this is called then the old scheme will be
removed (maybe deleted if created by this world) and the new one used. The new
one will NOT be deleted by CWorld.

Also, this does NOT call pScheme->WorldSet().

inputs
   PCSurfaceSchemeSocket      pScheme - New scheme
returns
   none
*/
void CWorld::SurfaceSchemeSet (PCSurfaceSchemeSocket pScheme)
{
   // BUGFIX - Dont delete the original scheme since may need later on for
   // reference, esecially in CObjectEditor
   //if (m_pOrigSurfaceScheme)
   //   delete m_pOrigSurfaceScheme;
   //m_pOrigSurfaceScheme = NULL;
   m_pSurfaceScheme = pScheme;

   // BUGFIX - If set to NULL then restore old version. Otherwise crashes
   if (!m_pSurfaceScheme)
      m_pSurfaceScheme = m_pOrigSurfaceScheme;
}


/**********************************************************************************
CWorld::WorldBoundingBoxGet - Gets the bounding box for everything in the world.

inputs
   PCPoint     pCorner1, pCorner2 - Filled with the corners, in world space
   BOOL        fIgnoreGround - If set to TRUE, ignores the ground (and other
               objects that choose to be ignored by this) when calculating bounding
               box.
returns
   BOOL - TRUE if corners filled in
*/
BOOL CWorld::WorldBoundingBoxGet (PCPoint pCorner1, PCPoint pCorner2, BOOL fIgnoreGround)
{
   BOOL fFirstTime = TRUE;
   pCorner1->Zero();
   pCorner2->Zero();

   DWORD i;
   CPoint b[2], p;
   CMatrix m;
   OSMIGNOREWORLDBOUNDINGBOXGET ignore;
   for (i = 0; i < ObjectNum(); i++) {
      ignore.fIgnoreCompletely = FALSE;
      PCObjectSocket pos = ObjectGet(i);
      if (pos && pos->Message (OSM_IGNOREWORLDBOUNDINGBOXGET, &ignore)) {
         if (ignore.fIgnoreCompletely || fIgnoreGround)
            continue;
      }

      BoundingBoxGet (i, &m, &b[0], &b[1]);

      DWORD x,y,z;
      // expand the box's two corners into a full eight points and then rotate
      for (x = 0; x < 2; x++) for (y=0;y<2;y++) for (z=0; z<2;z++) {
         p.p[0] = b[x].p[0];
         p.p[1] = b[y].p[1];
         p.p[2] = b[z].p[2];
         p.p[3] = 1;

         // rotate and see min and max
         p.MultiplyLeft (&m);

         // min/max
         if (fFirstTime) {
            pCorner1->Copy(&p);
            pCorner2->Copy(&p);
            fFirstTime = FALSE;
            continue;
         }

         DWORD k;
         for (k = 0; k < 3; k++) {
            pCorner1->p[k] = min(pCorner1->p[k], p.p[k]);
            pCorner2->p[k] = max(pCorner2->p[k], p.p[k]);
         }
      }
   }

   // done
   return TRUE;
}


/*************************************************************************************
CWorld::IntersectBoundingBox - Given a bounding box, this finds all objects
in the world whose bounding box that intersect its.

inputs
   PCMatrix       mBoundBoxToWorld - Matrix that converts the bounding box to world
                  coordinates
   PCPoint        pCorner1, pCorner2 - Two corners of the bounding box
   PCListFixed    plistIndex - Initialized to sizeof(DWORD), and filled with
                  and objects whose bounding boxes intersect.
returns
   none
*/
void CWorld::IntersectBoundingBox (PCMatrix mBoundingBoxToWorld, PCPoint pCorner1,
                                   PCPoint pCorner2, PCListFixed plistIndex)
{
   plistIndex->Init (sizeof(DWORD));
   plistIndex->Clear();

   // invert mBoundingBoxToWorld so can convert from world to bounding box coords
   CMatrix mInv;
   mBoundingBoxToWorld->Invert4 (&mInv);

   // adjust the corners for min/max
   DWORD i;
   CPoint pMin, pMax;
   for (i = 0; i < 3; i++) {
      pMin.p[i] = min(pCorner1->p[i], pCorner2->p[i]);
      pMax.p[i] = max(pCorner1->p[i], pCorner2->p[i]);
   }

   // loop through all the objects
   CMatrix mObject;
   CPoint pObjCorner[2];
   CPoint pObjMin, pObjMax, pt;
   DWORD x,y,z;
   DWORD k;
   BOOL fFirstTime;
   for (i = 0; i < ObjectNum(); i++) {
      if (!BoundingBoxGet (i, &mObject, &pObjCorner[0], &pObjCorner[1]))
         continue;

      // multiply matrix
      mObject.MultiplyRight (&mInv);

      // convert all the points
      // expand the box's two corners into a full eight points and then rotate
      fFirstTime = TRUE;
      for (x = 0; x < 2; x++) for (y=0;y<2;y++) for (z=0; z<2;z++) {
         pt.p[0] = pObjCorner[x].p[0];
         pt.p[1] = pObjCorner[y].p[1];
         pt.p[2] = pObjCorner[z].p[2];
         pt.p[3] = 1;

         // rotate and see min and max
         pt.MultiplyLeft (&mObject);

         // min/max
         if (fFirstTime) {
            pObjMin.Copy(&pt);
            pObjMax.Copy(&pt);
            fFirstTime = FALSE;
            continue;
         }

         for (k = 0; k < 3; k++) {
            pObjMin.p[k] = min(pObjMin.p[k], pt.p[k]);
            pObjMax.p[k] = max(pObjMax.p[k], pt.p[k]);
         }
      }  // loop over all points

      // trivial reject of bounding box intersect
      for (k = 0; k < 3; k++) {
         if ((pObjMin.p[k] >= pMax.p[k]) || (pObjMax.p[k] <= pMin.p[k]))
            break;
      }
      if (k < 3)
         continue;   // was rejected

      // if got to here there's some sort of overlap
      plistIndex->Add (&i);
   }
}

/***********************************************************************************8
CWorld::VariableSet  - Sets a world variable to a new value. If the variable doens't
exist then it's created.

inputs
   PWSTR       pszName - Case sensative name
   PWSTR       pszData - Data to associate with the variable
returns
   BOOL - TRUE if successful
*/
BOOL CWorld::VariableSet (PWSTR pszName, PWSTR pszData)
{
   m_fDirty = TRUE;

   // see if can find it
   DWORD i;
   PWSTR psz;
   for (i = 0; i < m_listVarName.Num(); i++) {
      psz = (PWSTR) m_listVarName.Get(i);
      if (!wcscmp (psz, pszName)) {
         // cache away for undo/redo
         if (m_pUndoCur && m_fKeepUndo)
            m_pUndoCur->Change (this, pszName, TRUE, 0);

         m_listVarData.Set (i, pszData, (wcslen(pszData)+1)*2);

         if (m_pUndoCur && m_fKeepUndo)
            m_pUndoCur->Change (this, pszName, FALSE, 0);

         // Send a notification that the undo state has changed
         NotifySocketsUndo ();
         return TRUE;
      }
   }
   
   // if got here, wasn't found
   // cache away for undo/redo
   if (m_pUndoCur && m_fKeepUndo)
      m_pUndoCur->Change (this, pszName, TRUE, 1);

   m_listVarName.Add (pszName, (wcslen(pszName)+1)*2);
   m_listVarData.Add (pszData, (wcslen(pszData)+1)*2);

   if (m_pUndoCur && m_fKeepUndo)
      m_pUndoCur->Change (this, pszName, FALSE, 1);

   // Send a notification that the undo state has changed
   NotifySocketsUndo ();

   return TRUE;
}

/***********************************************************************************8
CWorld::VariableRemove - Removed a variable from the list.

inputs
   PWSTR       pszName - Case sensative name
returns
   BOOL - TRUE if successful
*/
BOOL CWorld::VariableRemove (PWSTR pszName)
{
   m_fDirty = TRUE;

   // see if can find it
   DWORD i;
   PWSTR psz;
   for (i = 0; i < m_listVarName.Num(); i++) {
      psz = (PWSTR) m_listVarName.Get(i);
      if (!wcscmp (psz, pszName)) {
         // cache away for undo/redo
         if (m_pUndoCur && m_fKeepUndo)
            m_pUndoCur->Change (this, pszName, TRUE, 2);

         m_listVarData.Remove (i);
         m_listVarName.Remove (i);

         if (m_pUndoCur && m_fKeepUndo)
            m_pUndoCur->Change (this, pszName, FALSE, 2);

         // Send a notification that the undo state has changed
         NotifySocketsUndo ();
         return TRUE;
      }
   }

   return FALSE;
}

/***********************************************************************************8
CWorld::VariableGet - Finds a variable and returns a pointer to the string for the
variable. Do NOT change the string. Use VariableSet() to do this.

inputs
   PWSTR       pszName - Case sensative name
returns
   PWSTR - String. NULL if can't fin
*/
PWSTR CWorld::VariableGet (PWSTR pszName)
{
   // see if can find it
   DWORD i;
   PWSTR psz;
   for (i = 0; i < m_listVarName.Num(); i++) {
      psz = (PWSTR) m_listVarName.Get(i);
      if (!wcscmp (psz, pszName)) {
         return (PWSTR) m_listVarData.Get(i);
      }
   }

   return FALSE;
}


/**********************************************************************************
GlobalFloorLevelsSet - Sets the global floor level and send a message to every
object in the world that
global floor levels have changed.

inputs
   PCWorld           pWorld - world
   PCObjectSocket    pIgnore - Ignore this one
   //DWORD             dwNum - Number of levels. Must be NUMLEVELS
   fp            *pafLevel - Pointer to dwNum doubles which have level values
   fp            fHigher - Pointer to value for that gets higher-floor information
returns
   none
*/
void GlobalFloorLevelsSet (PCWorldSocket pWorld, PCObjectSocket pIgnore,
                           fp *pafLevel, fp fHigher)
{
   DWORD dwNum = NUMLEVELS;

   // change them
   DWORD i;
   WCHAR szTemp[64], szTemp2[64];
   for (i = 0; i < dwNum; i++) {
      swprintf (szTemp, gpszGFLLevel, (int) i);
      swprintf (szTemp2, L"%g", (double)pafLevel[i]);
      pWorld->VariableSet (szTemp, szTemp2);
   }
   swprintf (szTemp2, L"%g", (double)fHigher);
   pWorld->VariableSet (gpszGFLHigher, szTemp2);

   PCObjectSocket pos;
   for (i = 0; i < pWorld->ObjectNum(); i++) {
      pos = pWorld->ObjectGet (i);
      if (pos == pIgnore)
         continue;
      pos->Message (OSM_GLOBALLEVELCHANGED, NULL);
   };
}

/**********************************************************************************
GlobalFloorLevelsGet - Get the current global floor level settings. Returns
true if they actually change one of the values

inputs
   PCWorld           pWorld - world
   PCObjectSocket    pThis - Send an object changed notification if changed
   // DWORD             dwNum - Number of levels. MUST be NUMLEVELS.
   fp            *pafLevel - Pointer to dwNum doubles which have level values
   fp            *pfHigher - Pointer to value for that gets higher-floor information
returns
   BOOL - TRUE if any of the values have actually changed, FALSE if they're the same
*/
BOOL GlobalFloorLevelsGet (PCWorldSocket pWorld, PCObjectSocket pThis, fp *pafLevel, fp *pfHigher)
{
   DWORD dwNum = NUMLEVELS;

   WCHAR szTemp[64];
   DWORD i;
   double fTemp;
   BOOL fChanged;
   fChanged = FALSE;
   PWSTR psz;
   for (i = 0; i < dwNum; i++) {
      swprintf (szTemp, gpszGFLLevel, (int) i);
      psz = pWorld->VariableGet (szTemp);
      if (!psz)
         continue;
      if (!AttribToDouble(psz, &fTemp))
         continue;
      if (fTemp == pafLevel[i])
         continue;

      if (!fChanged && pThis)
         pWorld->ObjectAboutToChange (pThis);

      pafLevel[i] = fTemp;
      fChanged = TRUE;
   }

   psz = pWorld->VariableGet (gpszGFLHigher);
   if (psz && AttribToDouble(psz, &fTemp) && (fTemp != *pfHigher)) {
      if (!fChanged && pThis)
         pWorld->ObjectAboutToChange (pThis);

      *pfHigher = fTemp;
      fChanged = TRUE;
   }

   if (fChanged)
      pWorld->ObjectChanged (pThis);

   return fChanged;
}


static PWSTR gpszVarName = L"VarName%d";
static PWSTR gpszVarData = L"VarData%d";
static PWSTR gpszObjects = L"Objects";
static PWSTR gpszSchemes = L"Schemes";
static PWSTR gpszVariables = L"Variables";
static PWSTR gpszAux = L"Aux";

/******************************************************************************
CWorld::MMLTo - Writes the world and all information about the world to MML
nodes (which must be freed by the caller)

inputs
   PCProgressSocket     pProgress - For progress bar. Can be NULL.
returns
   PCMMLNode2 - Node. NULL if error
*/
PCMMLNode2 CWorld::MMLTo (PCProgressSocket pProgress)
{
   PCMMLNode2 pNode;
   pNode = new CMMLNode2;
   if (!pNode)
      return NULL;

   // write out the objects
   PCMMLNode2 pSub;
   pSub = MMLFromObjects (this, NULL, 0, pProgress);
   if (pSub) {
      pSub->NameSet (gpszObjects);
      pNode->ContentAdd (pSub);
   }

   // write out the surfaces
   pSub = m_pSurfaceScheme->MMLTo ();
   if (pSub) {
      pSub->NameSet (gpszSchemes);
      pNode->ContentAdd (pSub);
   }

   // NOTE: Don't write out the selection since not typical for files

   // attributes
   pSub = new CMMLNode2;
   if (pSub) {
      pNode->ContentAdd (pSub);
      pSub->NameSet (gpszVariables);

      DWORD i;
      WCHAR szTemp[32];
      for (i = 0; i < m_listVarName.Num(); i++) {
         swprintf (szTemp, gpszVarName, i);
         MMLValueSet (pSub, szTemp, (PWSTR) m_listVarName.Get(i));
         swprintf (szTemp, gpszVarData, i);
         MMLValueSet (pSub, szTemp, (PWSTR) m_listVarData.Get(i));
      }
   }

   // write out AUX
   if (m_pAux) {
      m_pAux->NameSet (gpszAux);
      pSub = m_pAux->Clone();
      if (pSub)
         pNode->ContentAdd (pSub);
   }


   // done
   return pNode;
}



/******************************************************************************
CWorld::MMLFrom - Reads the world and all information about the world from MML
nodes.

inputs
   PCMMLNode2      pNode - MML node
   BOOL           *pfFailedToLoad - Filled with TRUE if some objects failed to load
                     because unknown GUID or somrting
   PCProgressSocket     pProgress - Progress bar to use. Can be NULL
returns
   BOOL - TRUE if success
*/
BOOL CWorld::MMLFrom (PCMMLNode2 pNode, BOOL *pfFailedToLoad, PCProgressSocket pProgress)
{
   *pfFailedToLoad = FALSE;

   // BUGFIX - Dont keep undo around
   BOOL fUndo = m_fKeepUndo;
   m_fKeepUndo = FALSE;

   // wipe everything
   Clear (FALSE);

   // find everything except the objects (at first)
   PCMMLNode2 pSub, pObjects;
   pObjects = NULL;
   PWSTR psz;
   CMem memCutout;
   DWORD i;
   for (i = 0; i < pNode->ContentNum(); i++) {
      pSub = NULL;
      pNode->ContentEnum (i, &psz, &pSub);
      if (!pSub)
         continue;
      psz = pSub->NameGet();
      if (!psz)
         continue;

      if (!_wcsicmp(psz, gpszObjects))
         pObjects = pSub;
      else if (!_wcsicmp(psz, gpszSchemes))
         m_pSurfaceScheme->MMLFrom (pSub);
      else if (!_wcsicmp(psz, gpszAux)) {
         if (m_pAux)
            delete m_pAux;
         m_pAux = pSub->Clone();
      }
      else if (!_wcsicmp(psz, gpszVariables)) {
         DWORD i;
         WCHAR szTemp[32];
         PWSTR psz1, psz2;
         for (i = 0; ; i++) {
            swprintf (szTemp, gpszVarName, i);
            psz1 = MMLValueGet (pSub, szTemp);
            if (!psz1)
               break;
            swprintf (szTemp, gpszVarData, i);
            psz2 = MMLValueGet (pSub, szTemp);
            if (!psz2) {
               m_listVarName.Remove (m_listVarName.Num()-1);
               break;
            }
            VariableSet (psz1, psz2);
         }
         
      }
   }

   // get the objects
   if (pObjects)
      *pfFailedToLoad = !MMLToObjects (this, pObjects, FALSE, FALSE, pProgress);

   m_fDirty = FALSE;
   m_fKeepUndo = fUndo;

   // done
   return TRUE;
}


/****************************************************************************************
CWorld::AuxGet - The application can store auxiliary information about the world in
the node return by AuxGet. Should probably create sub-nodes with unique names.
NOTE: Dont delete the main node. Also, undo NOT supported for this.

returns
   PCMMLNode2 - Auxiliary node to use
*/
PCMMLNode2 CWorld::AuxGet (void)
{
   if (!m_pAux)
      m_pAux = new CMMLNode2;

   return m_pAux;
}

/****************************************************************************************
CWorld::ObjectCacheReset - The world object often caches information, such as the bounding
box for its objects. This is called if something system wide has changed that might
require a re-cache of everything. For example: If an object is modified in the object
editor this will be called because the bounding boxes of any objects that use that
object, or indirectly use it, may change.
*/
void CWorld::ObjectCacheReset (void)
{
   DWORD i;
   POBJECTINFO poi;
   for (i = 0; i < m_listOBJECTINFO.Num(); i++) {
      poi = (POBJECTINFO) m_listOBJECTINFO.Get(i);

      poi->fIsBoundingBoxValid = FALSE;
   }

   NotifySockets (WORLDC_NEEDTOREDRAW, NULL);
}

/****************************************************************************************
CWorld::ObjEditorDestory - From CWorldSocket
*/
BOOL CWorld::ObjEditorDestroy (void)
{
   BOOL fDestroy = TRUE;

   // loop
   DWORD i, dwNum;
   PCObjectSocket pos;
   dwNum = ObjectNum();
   for (i = 0; i < dwNum; i++) {
      pos = ObjectGet(i);
      if (!pos)
         continue;
      fDestroy &= pos->EditorDestroy ();
   }

   return fDestroy;
}

/****************************************************************************************
CWorld::ObjEditorShowWindow - From CWorldSocket
*/
BOOL CWorld::ObjEditorShowWindow (BOOL fShow)
{
   // loop
   DWORD i, dwNum;
   PCObjectSocket pos;
   dwNum = ObjectNum();
   for (i = 0; i < dwNum; i++) {
      pos = ObjectGet(i);
      if (!pos)
         continue;
      pos->EditorShowWindow (fShow);
   }

   return TRUE;
}

