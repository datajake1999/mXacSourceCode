/************************************************************************
CSceneSet.cpp - Code for managing a collection of scenes that accompany
a CWorld. This also ends up keeping track of undo and redo
*/

#include <windows.h>
#include <math.h>
#include "escarpment.h"
#include "..\M3D.h"


/************************************************************************
CSceneSet::Constructor and destructor
*/
CSceneSet::CSceneSet (void)
{
   m_pWorld = NULL;
   m_fKeepUndo = TRUE;
   m_fFastRefresh = TRUE;
   m_pCurScene = NULL;
   m_fCurTime = 0;
   m_fCamera = FALSE;
   memset (&m_gCamera, 0, sizeof(m_gCamera));
   m_fIgnoreWorldNotify = FALSE;
   m_fNeedToSyncToWorld = FALSE;
   m_lPCScene.Init (sizeof(PCScene));
   m_pUndoCur = new CUndoScenePacket;
   m_fWorldChangedCompletely = FALSE;
   m_lWorldObjectGUID.Init (sizeof(GUID));
   m_lSSDATTRIB.Init (sizeof(SSDATTRIB));
   m_listViewSceneSocket.Init (sizeof(PCViewSceneSocket));
   m_listPCUndoScenePacket.Init (sizeof(PCUndoScenePacket));
   m_listPCRedoScenePacket.Init (sizeof(PCUndoScenePacket));
   m_listSCENESEL.Init (sizeof(SCENESEL));
   m_dwLastDefault = -1;
}

CSceneSet::~CSceneSet (void)
{
   // unregister from the world
   WorldSet (NULL);

   // delete undoredo
   m_fKeepUndo = FALSE; // BUGFIX - So freeing is faster
   UndoClear (TRUE, TRUE); // must be before the delete m_pUndoCur
   if (m_pUndoCur) {
      delete m_pUndoCur;
      m_pUndoCur = NULL;
   }

   // delete all objects
   DWORD i;
   for (i = 0; i < m_lPCScene.Num(); i++) {
      PCScene ps = *((PCScene*) m_lPCScene.Get(i));
      delete ps;
   }

   // attributes
   PSSDATTRIB pa;
   pa = (PSSDATTRIB) m_lSSDATTRIB.Get(0);
   for (i = 0; i < m_lSSDATTRIB.Num(); i++) {
      delete pa[i].plATTRIBVAL;
   }
}


/********************************************************************************
CSceneSet::WorldSet - Must be called to link up the sceneset with the world.
This will cause the sceneset to register with the world.

inputs
   PCWorld        pWorld - world
*/
void CSceneSet::WorldSet (PCWorld pWorld)
{
   if (m_pWorld)
      m_pWorld->NotifySocketRemove (this);
   m_pWorld = pWorld;
   if (m_pWorld)
      m_pWorld->NotifySocketAdd (this);
}

/********************************************************************************
CSceneSet::WorldGet - Returns the world currently attached to.
*/
PCWorld CSceneSet::WorldGet (void)
{
   return m_pWorld;
}

/********************************************************************************
CSceneSet::DirtyGet - Gets the state of the dirty flag. This just calls into the
world object and asks it.
*/
BOOL CSceneSet::DirtyGet (void)
{
   if (!m_pWorld)
      return FALSE;
   return m_pWorld->DirtyGet();
}

/********************************************************************************
CSceneSet::DirtySet - Sets the state of the dirty flag. This just calls into the
world object and sets it there.
*/
void CSceneSet::DirtySet (BOOL fDirty)
{
   if (m_pWorld)
      m_pWorld->DirtySet (fDirty);
}


/**************************************************************************************
CSceneSet::NotifySocketAdd - Adds a notification socket onto the end of the list. From then
on any changes to the world will get notified through this call.

inputs
   PCViewSceneSocket     pAdd - Socket to add. Duplication testing is NOT done.
returns
   BOOL - TRUE if added
*/
BOOL CSceneSet::NotifySocketAdd (PCViewSceneSocket pAdd)
{
   m_listViewSceneSocket.Add (&pAdd);
   return TRUE;
}

/**************************************************************************************
CSceneSet::NotifySocketRemove - Removes the notification socket from the list.

inputs
   PCViewSceneSocket     pRemove - Socket to remove
returns
   BOOL - TRUE if found and removed
*/
BOOL CSceneSet::NotifySocketRemove (PCViewSceneSocket pRemove)
{
   DWORD i, dwNum;
   PCViewSceneSocket *pp;
   pp = (PCViewSceneSocket*) m_listViewSceneSocket.Get(0);
   dwNum = m_listViewSceneSocket.Num();

   for (i = 0; i < dwNum; i++)
      if (pp[i] == pRemove) {
         // found it
         m_listViewSceneSocket.Remove (i);
         return TRUE;
      }
   return FALSE;
}


/**************************************************************************************
CSceneSet::NotifySockets - An internal function called to notify all the sockets that
something has changed about the world. The views are all tied into this so they automagically
re-render.

inputs
   DWORD    dwChange - Bitfield of WORLDC_XXX
   GUID     *pgScene - If known this points to the scene's guid of object that changed
   GUID     *pgObjectWorld - If known, this points to the CObjectSocket's guid which was changed
   GUID     *pgObjectAnim - If known, this points to the CAnimSocket's guid which changed
*/
void CSceneSet::NotifySockets (DWORD dwChange, GUID *pgScene, GUID *pgObjectWorld, GUID *pgObjectAnim)
{
   DWORD i, dwNum;
   PCViewSceneSocket *pp;
   pp = (PCViewSceneSocket*) m_listViewSceneSocket.Get(0);
   dwNum = m_listViewSceneSocket.Num();

   for (i = 0; i < dwNum; i++)
      (pp[i]->SceneChanged)(dwChange, pgScene, pgObjectWorld, pgObjectAnim);
}


/**************************************************************************************
CSceneSet::NotifySocketsUndo - An internal function called to notify all the sockets that
something has changed about the undo-stat of world. The views are all tied into this so they automagically
re-render.

inputs
   none
*/
void CSceneSet::NotifySocketsUndo (void)
{
   BOOL fUndo, fRedo;
   fUndo = UndoQuery (&fRedo);

   DWORD i, dwNum;
   PCViewSceneSocket *pp;
   pp = (PCViewSceneSocket*) m_listViewSceneSocket.Get(0);
   dwNum = m_listViewSceneSocket.Num();

   for (i = 0; i < dwNum; i++)
      (pp[i]->SceneUndoChanged)(fUndo, fRedo);
}


/**************************************************************************************
CSceneSet::SceneNum - Returns the number of scenes stored away.
*/
DWORD CSceneSet::SceneNum (void)
{
   return m_lPCScene.Num();
}

/**************************************************************************************
CSceneSet::SceneNew - Adds a new scene to the list. NOTE: In the process this will set
the new scene's time-0 values to current world settings.

NOTE: In the process this will sync up to the starting values.
returns
   DWORD - Returns the new index, or -1 if error
*/
DWORD CSceneSet::SceneNew (void)
{
   PCScene pNew = new CScene;
   if (!pNew)
      return -1;

   // sync to because will be doing syncfromworld for starting values of new scene
   SyncToWorld();

   // number
   DWORD dwNum;
   dwNum = m_lPCScene.Num();
   m_lPCScene.Add (&pNew);

   GUID g;
   GUIDGen (&g);
   pNew->GUIDSet (&g);
   pNew->SceneSetSet (this);

   // BUGFIX - If add a new scene, sync it up to the current object locations so
   // that won't lose any object settings

   // set flag so undo not remembered
   BOOL fUndo;
   PCScene pOldScene;
   fp fOldTime;
   fUndo = m_fKeepUndo;
   m_fKeepUndo = FALSE;
   pOldScene = m_pCurScene;
   fOldTime = m_fCurTime;
   m_pCurScene = pNew;  // set current scene so syncing will work
   m_fCurTime = 0;

   // sync up
   m_pCurScene->SyncFromWorld (0, NULL);

   // restore keep undo flag
   m_fCurTime = fOldTime;
   m_pCurScene = pOldScene;
   m_fKeepUndo = fUndo;
   //UndoClear (TRUE, TRUE);

   DirtySet (TRUE);

   return dwNum;
}

/**************************************************************************************
CSceneSet::SceneRemove - Removes the given scene from the index. This also clears out undo
if it's the current scene, and deletes clears out the current scene (if it's this one)

inputs
   DWORD    dwIndex - 0 to SceneNum()-1
returns
   BOOL - TRUE if success
*/
BOOL CSceneSet::SceneRemove (DWORD dwIndex)
{
   if (dwIndex >= m_lPCScene.Num())
      return FALSE;
   PCScene ps;
   ps = *((PCScene*) m_lPCScene.Get(dwIndex));

   if (m_pCurScene == ps)
      StateSet (NULL, 0);

   // delte, but dont keep undo
   BOOL fKeep;
   fKeep = m_fKeepUndo;
   m_fKeepUndo = FALSE;
   delete ps;
   m_fKeepUndo = fKeep;
   DirtySet (TRUE);

   m_lPCScene.Remove (dwIndex);

   return TRUE;
}

/**************************************************************************************
CSceneSet::SceneGet - Gets the give scene.

inputs
   DWORD    dwIndex - 0 to SceneNum()-1
returns
   PCScene - Scene object to use
*/
PCScene CSceneSet::SceneGet (DWORD dwIndex)
{
   if (dwIndex >= m_lPCScene.Num())
      return FALSE;
   PCScene ps;
   ps = *((PCScene*) m_lPCScene.Get(dwIndex));

   return ps;
}

/**************************************************************************************
CSceneSet::SceneClone - Clones the given scene to a new copy and returns the index
to the new copy.

inputs
   DWORD    dwIndex - Index, from 0 .. SceneNum()-1
returns
   DWORD - Index to the new scene, or -1 if error
*/
DWORD CSceneSet::SceneClone (DWORD dwIndex)
{
   if (dwIndex >= m_lPCScene.Num())
      return FALSE;
   PCScene ps;
   ps = *((PCScene*) m_lPCScene.Get(dwIndex));

   // clone, but dont keep undo
   BOOL fKeep;
   PCScene pNew;
   fKeep = m_fKeepUndo;
   m_fKeepUndo = FALSE;
   pNew = ps->Clone ();
   m_fKeepUndo = fKeep;
   DirtySet (TRUE);
   if (!pNew)
      return -1;

   // add it
   DWORD dwNum;
   dwNum = m_lPCScene.Num();
   m_lPCScene.Add (&pNew);

   // set world, etc.
   GUID g;
   GUIDGen (&g);
   pNew->GUIDSet (&g);
   pNew->SceneSetSet (this);

   return dwNum;
}


/**************************************************************************************
CSceneSet::SceneFind - Finds a scene based on its name.

inputs
   PWSTR       pszName - name looking for
returns
   DWORD - Index to the new scene, or -1 if cant find
*/
DWORD CSceneSet::SceneFind (PWSTR pszName)
{
   PCScene *pps = (PCScene*) m_lPCScene.Get(0);
   DWORD i;
   PWSTR psz;

   for (i = 0; i < m_lPCScene.Num(); i++) {
      psz = pps[i]->NameGet();
      if (!psz)
         continue;

      if (!_wcsicmp(pszName, psz))
         return i;
   }

   // not found
   return -1;
}


/**************************************************************************************
CSceneSet::SceneFind - Finds a scene based on its GUID.

inputs
   GUID       *pgScene - Looking for
returns
   DWORD - Index to the new scene, or -1 if cant find
*/
DWORD CSceneSet::SceneFind (GUID *pgScene)
{
   PCScene *pps = (PCScene*) m_lPCScene.Get(0);
   DWORD i;
   GUID g;

   for (i = 0; i < m_lPCScene.Num(); i++) {
      pps[i]->GUIDGet (&g);

      if (IsEqualGUID (*pgScene, g))
         return i;
   }

   // not found
   return -1;
}


/**************************************************************************************
CSceneSet::Clear - Clears everything and returns to original state, except linked to m_pWorld
*/
void CSceneSet::Clear (void)
{
   // go to null state
   StateSet (NULL, 0);
   m_fNeedToSyncToWorld = FALSE;
   m_fWorldChangedCompletely = FALSE;
   m_lWorldObjectGUID.Clear();

   // delete undoredo
   BOOL fKeep = m_fKeepUndo;
   m_fKeepUndo = FALSE; // BUGFIX - So freeing is faster
   UndoClear (TRUE, TRUE); // must be before the delete m_pUndoCur

   // delete all objects
   DWORD i;
   for (i = 0; i < m_lPCScene.Num(); i++) {
      PCScene ps = *((PCScene*) m_lPCScene.Get(i));
      delete ps;
   }
   m_lPCScene.Clear();

   // attributes
   PSSDATTRIB pa;
   pa = (PSSDATTRIB) m_lSSDATTRIB.Get(0);
   for (i = 0; i < m_lSSDATTRIB.Num(); i++) {
      delete pa[i].plATTRIBVAL;
   }
   m_lSSDATTRIB.Clear();



   m_fKeepUndo = fKeep;
}


static PWSTR gpszSceneSet = L"SceneSet";
static PWSTR gpszCurScene = L"CurScene";
static PWSTR gpszCurTime = L"CurTime";
static PWSTR gpszScene = L"Scene";
static PWSTR gpszSSDATTRIB = L"SSDATTRIB";
static PWSTR gpszObjectWorld = L"ObjectWorld";
static PWSTR gpszValue = L"Value";
static PWSTR gpszCamera = L"Camera";

/**************************************************************************************
CSceneSet::MMLTo - Standard MMLTo
*/
PCMMLNode2 CSceneSet::MMLTo (PCProgressSocket pProgress)
{
   PCMMLNode2 pNode = new CMMLNode2;
   if (!pNode)
      return NULL;
   pNode->NameSet (gpszSceneSet);

   // NOTE: Not syncing up - letting the filesave and fileload do that

   // write out current scene
   if (m_pCurScene) {
      GUID g;
      m_pCurScene->GUIDGet (&g);
      MMLValueSet (pNode, gpszCurScene, (PBYTE) &g, sizeof(g));
   }
   MMLValueSet (pNode, gpszCurTime, m_fCurTime);
   if (m_fCamera)
      MMLValueSet (pNode, gpszCamera, (PBYTE) &m_gCamera, sizeof(m_gCamera));

   // write out the scenes
   DWORD i;
   PCScene *pps;
   PCMMLNode2 pSub;
   pps = (PCScene*) m_lPCScene.Get(0);
   for (i = 0; i < m_lPCScene.Num(); i++) {
      if (pProgress)
         pProgress->Update ((fp) i / (fp) (m_lPCScene.Num()+1));

      pSub = pps[i]->MMLTo ();
      if (pSub) {
         pSub->NameSet (gpszScene);
         pNode->ContentAdd (pSub);
      }
   }


   // write out the temporary attributes
   if (pProgress)
      pProgress->Update ((fp) m_lPCScene.Num() / (fp) (m_lPCScene.Num()+1));
   PSSDATTRIB pa;
   pa = (PSSDATTRIB) m_lSSDATTRIB.Get(0);
   for (i = 0; i < m_lSSDATTRIB.Num(); i++, pa++) {
      if (!pa->plATTRIBVAL->Num())
         continue;   // no default attributes so dont bother writing out

      pSub = pNode->ContentAddNewNode ();
      if (!pSub)
         break;
      pSub->NameSet (gpszSSDATTRIB);

      MMLValueSet (pSub, gpszObjectWorld, (PBYTE) &pa->gObjectWorld, sizeof(pa->gObjectWorld));

      DWORD j;
      PCMMLNode2 pSub2;
      PATTRIBVAL pav;
      pav = (PATTRIBVAL) pa->plATTRIBVAL->Get(0);
      for (j = 0; j < pa->plATTRIBVAL->Num(); j++, pav++) {
         if (!pav->szName[0])
            continue;
         pSub2 = pSub->ContentAddNewNode();
         if (!pSub2)
            continue;
         pSub2->NameSet (Attrib());

         MMLValueSet (pSub2, Attrib(), pav->szName);
         MMLValueSet (pSub2, gpszValue, pav->fValue);
      }
   }

   return pNode;
}

/**************************************************************************************
CSceneSet::MMLFrom - Standard MMLFrom
*/
BOOL CSceneSet::MMLFrom (PCMMLNode2 pNode, PCProgressSocket pProgress)
{
   // clear everything here
   Clear ();

   // read in these scenes and the rest
   DWORD i;
   PWSTR psz;
   PCMMLNode2 pSub;
   for (i = 0; i < pNode->ContentNum(); i++) {
      pSub = NULL;
      pNode->ContentEnum (i, &psz, &pSub);
      if (!pSub)
         continue;
      psz = pSub->NameGet();
      if (!psz)
         continue;

      if (pProgress)
         pProgress->Update ((fp) i / (fp) pNode->ContentNum());

      if (!_wcsicmp(psz, gpszScene)) {
         PCScene pNew = new CScene;
         if (!pNew)
            continue;
         pNew->SceneSetSet (this);
         if (!pNew->MMLFrom (pSub)) {
            delete pNew;
            continue;
         }
         m_lPCScene.Add (&pNew);
         continue;
      }
      else if (!_wcsicmp(psz, gpszSSDATTRIB)) {
         SSDATTRIB sa;
         memset (&sa, 0, sizeof(sa));
         
         MMLValueGetBinary (pSub, gpszObjectWorld, (PBYTE) &sa.gObjectWorld, sizeof(sa.gObjectWorld));
         sa.plATTRIBVAL = new CListFixed;
         if (!sa.plATTRIBVAL)
            continue;
         sa.plATTRIBVAL->Init (sizeof(ATTRIBVAL));

         PCMMLNode2 pSub2;
         DWORD j;
         for (j = 0; j < pSub->ContentNum(); j++) {
            pSub2 = NULL;
            pSub->ContentEnum (j, &psz, &pSub2);
            if (!pSub2)
               continue;
            psz = pSub2->NameGet();
            if (!psz || _wcsicmp(psz, Attrib()))
               continue;

            // have attribute
            ATTRIBVAL av;
            memset (&av, 0, sizeof(av));
            psz = MMLValueGet (pSub2, Attrib());
            if (!psz)
               continue;
            wcscpy (av.szName, psz);
            av.fValue = MMLValueGetDouble (pSub2, gpszValue, 0);
            sa.plATTRIBVAL->Add (&av);
         }

         // add it
         m_lSSDATTRIB.Add (&sa); // ok to just add because will retain sort order
         continue;
      }
   }


   // get the scene and the time
   m_fCurTime = MMLValueGetDouble (pNode, gpszCurTime, 0);
   if (MMLValueGetBinary (pNode, gpszCamera, (PBYTE) &m_gCamera, sizeof(m_gCamera)))
      m_fCamera = TRUE;
   else
      m_fCamera = FALSE;

   GUID g;
   if (MMLValueGetBinary (pNode, gpszCurScene, (PBYTE) &g, sizeof(g)))
      m_pCurScene = SceneGet(SceneFind (&g));
   else
      m_pCurScene = NULL;

   DirtySet (FALSE);

   return TRUE;
}


/**************************************************************************************
CSceneSet::SelectionRemove - Removes a specific object from the current selection

inputs
   GUID           *pgScene - Sene that it's in
   GUID           *pgObjectWorld - CObjectSocket that it's in
   GUID           *pgObjectAnim - CAnimSocket that it's in
returns
   BOOL - TRUE if success
*/
BOOL CSceneSet::SelectionRemove (GUID *pgScene, GUID *pgObjectWorld, GUID *pgObjectAnim)
{
   PSCENESEL pss = (PSCENESEL) m_listSCENESEL.Get(0);
   DWORD dwNum = m_listSCENESEL.Num();
   DWORD i;

   for (i = 0; i < dwNum; i++, pss++) {
      if (!IsEqualGUID (*pgObjectAnim, pss->gObjectAnim) || !IsEqualGUID (*pgObjectWorld, pss->gObjectWorld) ||
         !IsEqualGUID (*pgScene, pss->gScene))
         continue;

      // remove this
      m_listSCENESEL.Remove (i);

      // Notify app
      NotifySockets (WORLDC_SELREMOVE, pgScene, pgObjectWorld, pgObjectAnim);

      return TRUE;
   }

   return FALSE;
}

/**************************************************************************************
CSceneSet::SelectionAdd - Adds a specific object from the current selection

inputs
   GUID           *pgScene - Sene that it's in
   GUID           *pgObjectWorld - CObjectSocket that it's in
   GUID           *pgObjectAnim - CAnimSocket that it's in
returns
   BOOL - TRUE if success
*/
BOOL CSceneSet::SelectionAdd (GUID *pgScene, GUID *pgObjectWorld, GUID *pgObjectAnim)
{
   if (SelectionExists (pgScene, pgObjectWorld, pgObjectAnim))
      return TRUE;

   // add it
   SCENESEL ss;
   ss.gObjectAnim = *pgObjectAnim;
   ss.gObjectWorld = *pgObjectWorld;
   ss.gScene = *pgScene;
   m_listSCENESEL.Add (&ss);

   NotifySockets (WORLDC_SELADD, pgScene, pgObjectWorld, pgObjectAnim);
   return TRUE;
}

/**************************************************************************************
CSceneSet::SelectionExsits - Sees if a selection exists

inputs
   GUID           *pgScene - Sene that it's in
   GUID           *pgObjectWorld - CObjectSocket that it's in
   GUID           *pgObjectAnim - CAnimSocket that it's in
returns
   BOOL - TRUE if selection exsits
*/
BOOL CSceneSet::SelectionExists (GUID *pgScene, GUID *pgObjectWorld, GUID *pgObjectAnim)
{
   PSCENESEL pss = (PSCENESEL) m_listSCENESEL.Get(0);
   DWORD dwNum = m_listSCENESEL.Num();
   DWORD i;

   for (i = 0; i < dwNum; i++, pss++) {
      if (!IsEqualGUID (*pgObjectAnim, pss->gObjectAnim) || !IsEqualGUID (*pgObjectWorld, pss->gObjectWorld) ||
         !IsEqualGUID (*pgScene, pss->gScene))
         continue;

      // exists
      return TRUE;
   }

   return FALSE;
}

/**************************************************************************************
CSceneSet::SelectionEnum - Returns a pointer to the list of selections (which is valid
until the selection is changed) and their number. DO NOT change either.

inputs
   DWORD       *pdwNum - Filled with the number of selections
returns
   PSCENESEL - Returns a pointer to the selection info
*/
PSCENESEL CSceneSet::SelectionEnum (DWORD *pdwNum)
{
   if (pdwNum)
      *pdwNum = m_listSCENESEL.Num();
   return (PSCENESEL) m_listSCENESEL.Get(0);
}

/**************************************************************************************
CSceneSet::SelectionClear - Removes all the objects from the selection.

returns
   BOOL - TRUE if there were objects removed. FALSE if it was already empty
*/
BOOL CSceneSet::SelectionClear (void)
{
   // specifically dont change dirty flag

   if (!m_listSCENESEL.Num())
      return FALSE;

   m_listSCENESEL.Clear ();
   NotifySockets (WORLDC_SELREMOVE, NULL, NULL, NULL);
   return TRUE;
}

/**************************************************************************************
CSceneSet::ObjectAboutToChange - Tell the world that an object is about to change.
It calls into the world object to warn of the
impending change so the world object has the ability to clone the existing version
of the object for undo reasons. THe object MUST call ObjectChanged() soon after
calling this.

inputs
   PCAnimSocket      pObject - object that is about to change
*/
void CSceneSet::ObjectAboutToChange (PCAnimSocket pObject)
{
   DirtySet (TRUE);

   // cache away for undo/redo
   if (m_pUndoCur && m_fKeepUndo)
      m_pUndoCur->Change (this, pObject, TRUE, 0);

   // Send a notification that the undo state has changed
   NotifySocketsUndo ();
}


/********************************************************************************
CSceneSet::ObjectChanged - Called by an object after it has changed. (Must have called
ObjectAboutToChange before this.) The world object uses this to notify the views
that the object has changed.

inputs
   PCAnimSocket    pObject - object
returns
   none
  */
void CSceneSet::ObjectChanged (PCAnimSocket pObject)
{
   DirtySet (TRUE);


   // note that spline calculations dirty
   PCScene pScene;
   PCSceneObj pSceneObj;
   GUID gObjectWorld;
   pObject->WorldGet (NULL, &pScene, &gObjectWorld);
   pSceneObj = NULL;
   if (pScene)
      pSceneObj = pScene->ObjectGet (&gObjectWorld);
   if (pSceneObj)
      pSceneObj->AnimAttribDirty ();

   // remember that will need to sync to world
   m_fNeedToSyncToWorld = TRUE;

   // remember this for undo/redo
   if (m_pUndoCur && m_fKeepUndo)
      m_pUndoCur->Change (this, pObject, FALSE, 0);
   // Send a notification that the undo state has changed
   NotifySocketsUndo ();

   // notify sockets
   BOOL fSel;
   GUID gScene, gObjectAnim;
   pObject->GUIDGet (&gObjectAnim);
   pScene->GUIDGet (&gScene);
   fSel = SelectionExists (&gScene, &gObjectWorld, &gObjectAnim);
   NotifySockets (fSel ? WORLDC_OBJECTCHANGESEL : WORLDC_OBJECTCHANGENON,
      &gScene, &gObjectWorld, &gObjectAnim);
}

/**************************************************************************************
CSceneSet::ObjectRemoved - Call this when an object is to be removed. It will delete the
object (call pObject->Delete()) or put it in the undo list. It also changes the selection list (removing it from the selection)
AND notifies all the views that the world has changed.

inputs
   PCAnimSocket      pObject - object
returns
   BOOL - TRUE if deleted
*/
void CSceneSet::ObjectRemoved (PCAnimSocket pObject)
{
   DirtySet (TRUE);

   // get guids
   GUID gScene, gObjectWorld, gObjectAnim;
   PCScene pScene;
   pObject->GUIDGet (&gObjectAnim);
   pObject->WorldGet (NULL, &pScene, &gObjectWorld);
   pScene->GUIDGet (&gScene);

   // note that spline calculations dirty
   PCSceneObj pSceneObj;
   pSceneObj = NULL;
   if (pScene)
      pSceneObj = pScene->ObjectGet (&gObjectWorld);
   if (pSceneObj)
      pSceneObj->AnimAttribDirty ();

   // remember that will have to sync to world
   m_fNeedToSyncToWorld = TRUE;

   // remember that this has been removed
   if (m_pUndoCur && m_fKeepUndo)
      m_pUndoCur->Change (this, pObject, TRUE, 2);
   // Send a notification that the undo state has changed
   NotifySocketsUndo ();

   // remove from the selections list
   SelectionRemove (&gScene, &gObjectWorld, &gObjectAnim);

   // delete
   pObject->Delete();
   
   // Notify application of object deletion
   NotifySockets (WORLDC_OBJECTREMOVE, &gScene, &gObjectWorld, &gObjectAnim);
}


/**************************************************************************************
CSceneSet::ObjectAdded - Call this after a new object is added to a sceneobj.
It updates undo/redo and sends out notification

inputs
   PCAnimSocket         pObject - object to add
*/
void CSceneSet::ObjectAdded (PCAnimSocket pObject)
{
   DirtySet (TRUE);

   // get guids
   GUID gScene, gObjectWorld, gObjectAnim;
   PCScene pScene;
   pObject->GUIDGet (&gObjectAnim);
   pObject->WorldGet (NULL, &pScene, &gObjectWorld);
   pScene->GUIDGet (&gScene);


   // note that spline calculations dirty
   PCSceneObj pSceneObj;
   pSceneObj = NULL;
   if (pScene)
      pSceneObj = pScene->ObjectGet (&gObjectWorld);
   if (pSceneObj)
      pSceneObj->AnimAttribDirty ();

   // remember that will have to sync to world
   m_fNeedToSyncToWorld = TRUE;

   // remember this for undo/redo
   if (m_pUndoCur && m_fKeepUndo)
      m_pUndoCur->Change (this, pObject, FALSE, 1);
   // Send a notification that the undo state has changed
   NotifySocketsUndo ();

   NotifySockets (WORLDC_OBJECTADD, &gScene, &gObjectWorld, &gObjectAnim);

}

/**************************************************************************************
CSceneSet::UndoClear - Erase all the contents of the undo/redo buffers.

inputs
   BOOL        fUndo - Remove the undo buffers
   BOOL        fRedo - Remove the redo buffers
returns
   none
*/
void CSceneSet::UndoClear (BOOL fUndo, BOOL fRedo)
{
   PCUndoScenePacket pup;
   DWORD i;
   if (fUndo) {
      // current undo
      if (m_pUndoCur && m_pUndoCur->m_lUNDOSCENECHANGE.Num()) {
         delete m_pUndoCur;
         m_pUndoCur = NULL;
      }
      if (!m_pUndoCur)
         m_pUndoCur = new CUndoScenePacket;

      // anything in the list
      for (i = 0; i < m_listPCUndoScenePacket.Num(); i++) {
         pup =* ((PCUndoScenePacket*) m_listPCUndoScenePacket.Get(i));
         delete pup;
      }
      m_listPCUndoScenePacket.Clear();
   }

   if (fRedo) {
      // anything in the list
      for (i = 0; i < m_listPCRedoScenePacket.Num(); i++) {
         pup = *((PCUndoScenePacket*) m_listPCRedoScenePacket.Get(i));
         delete pup;
      }
      m_listPCRedoScenePacket.Clear();
   }

   // BUGFIX - Notify sockets of this
   NotifySocketsUndo ();
}


/**************************************************************************************
CSceneSet::UndoRemember - Remember this point in time as an undo point. The current undo
object is added onto the undo list.

inputs
   none
returns
   none
*/
void CSceneSet::UndoRemember (void)
{
   // if there arent any accumulated changes then ignore
   if (!m_pUndoCur->m_lUNDOSCENECHANGE.Num())
      return;

   // clear the redo list since made changes
   UndoClear (FALSE, TRUE);

   // if the undo list is already too long then remove an item
   if (m_listPCUndoScenePacket.Num() > 100) {
      PCUndoScenePacket p = *((PCUndoScenePacket*) m_listPCUndoScenePacket.Get(0));
      delete p;
      m_listPCUndoScenePacket.Remove(0);
   }

   // append the current undo
   m_listPCUndoScenePacket.Add (&m_pUndoCur);

   // create a new current ScenePacket
   m_pUndoCur = new CUndoScenePacket;

   // Send a notification that the undo state has changed
   NotifySocketsUndo ();
}

/************************************************************************************
CSceneSet::UndoQuery - Returns TRUE if there's something in the undo buffer. Also
filles in a flag if there's anything in the redo buffer.

inputs
   BOOL        *pfRedo - If not NULL, fills in true if can redo.
*/
BOOL CSceneSet::UndoQuery (BOOL *pfRedo)
{
   BOOL fUndo;
   fUndo = (m_listPCUndoScenePacket.Num() ? TRUE : FALSE) || (m_pUndoCur->m_lUNDOSCENECHANGE.Num() ? TRUE : FALSE);

   if (pfRedo)
      *pfRedo = (!m_pUndoCur->m_lUNDOSCENECHANGE.Num()) && (m_listPCRedoScenePacket.Num() ? TRUE : FALSE);

   return fUndo;
}


/************************************************************************************
CSceneSet::Undo - Undoes the last changes.

inputs
   BOOL     fUndo - Use TRUE to Undo. FALSE to Redo.
returns
   BOOL - TRUE if succeded. FALSE if error
*/
BOOL CSceneSet::Undo (BOOL fUndoIt)
{
   // NOTE: Not doing a g SyncFromWorld() because then wouldn't make sense to undo
   // However, clearing the sync info. If dont clear then might get a call right
   // away causing a sync and resulting in loss of what was undoing
   m_fWorldChangedCompletely = FALSE;
   m_lWorldObjectGUID.Clear();

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
      if (!m_pUndoCur->m_lUNDOSCENECHANGE.Num()) {
         DWORD dwNum;
         dwNum = m_listPCUndoScenePacket.Num();
         if (!dwNum)
            return FALSE;  // cant undo

         PCUndoScenePacket p;
         p = *((PCUndoScenePacket*) m_listPCUndoScenePacket.Get(dwNum-1));
         m_listPCUndoScenePacket.Remove(dwNum-1);

         delete m_pUndoCur;
         m_pUndoCur = p;
      }
   }
   else {
      // fill the current undo buffer from the redo
      if (m_pUndoCur->m_lUNDOSCENECHANGE.Num())
         return FALSE;  // cant redo if stuff in buffer

      DWORD dwNum;
      dwNum = m_listPCRedoScenePacket.Num();
      if (!dwNum)
         return FALSE;  // cant undo

      PCUndoScenePacket p;
      p = *((PCUndoScenePacket*) m_listPCRedoScenePacket.Get(dwNum-1));
      m_listPCRedoScenePacket.Remove(dwNum-1);

      delete m_pUndoCur;
      m_pUndoCur = p;
   }

   // remeber the current selection
   CUndoScenePacket UPSel;
   UPSel.FillSelectionList (this);

   // go through all the elements  and doo whatever replacing is necessary.
   // because we're also going to convert this information for the redo
   // buffer, 
   DWORD i;
   DWORD dwAdded, dwRemoved, dwChanged;
   dwAdded = dwRemoved = dwChanged = 0;
   for (i = 0; i < m_pUndoCur->m_lUNDOSCENECHANGE.Num(); i++) {
      PUNDOSCENECHANGE puc = (PUNDOSCENECHANGE) m_pUndoCur->m_lUNDOSCENECHANGE.Get(i);

      if (puc->dwObject == 0) {  // objects
         // see if can find he same object in the current list
         PCScene pScene = SceneGet (SceneFind (&puc->gScene));
         PCSceneObj pSceneObj = pScene ? pScene->ObjectGet(pScene->ObjectFind (&puc->gObjectWorld)) : NULL;
         DWORD dwIndex = pSceneObj ? pSceneObj->ObjectFind (&puc->gObjectAnim) : -1;
         PCAnimSocket pAnimObj = (dwIndex != -1)  ? pSceneObj->ObjectGet (dwIndex) : NULL;

         switch (puc->dwChange) {
         case 0:  // changed
            {
               // we had better find it
               if (!pAnimObj)
                  break;

               PCAnimSocket pSwap;
               pSwap = pSceneObj->ObjectSwapForUndo (dwIndex, puc->pObject);
               // dont need to set AnimAttribDirty() because done in ObjectsSwapForUndo
               pSwap->WorldSet (NULL, NULL, &puc->gObjectWorld);
               puc->pObject->WorldSet (WorldGet(), pScene, &puc->gObjectWorld);

               // remember new object
               puc->pObject = pSwap;

               // remember that will have to sync to world
               m_fNeedToSyncToWorld = TRUE;

               dwChanged++;
            }
            break;
         case 1:  // added
            {
               // we had better find it
               if (!pAnimObj)
                  break;

               // remember the object as it is
               if (puc->pObject)
                  puc->pObject->Delete();
               puc->pObject = pAnimObj;
               puc->pObject->WorldSet (NULL, NULL, &puc->gObjectWorld);
               puc->dwChange = 2;   // we're now a remove for the redo

               // we added this object since the last undo. Therefore, remove it.
               pSceneObj->ObjectRemove (dwIndex, TRUE);
               // dont need to callANimAttribDirty because doing in ObjectRemove

               // remember that will have to sync to world
               m_fNeedToSyncToWorld = TRUE;

               dwAdded++;
            }
            break;
         case 2:  // removed
            {
               // had better not find it in th elist
               if (pAnimObj)
                  break;

               // we removed it since the last undo, therefore add it back
               pSceneObj->ObjectAdd (puc->pObject, TRUE, &puc->gObjectAnim);
               // dont need to call AnimAttribDirty because doing in ObjectAdd
               puc->pObject = NULL;
               puc->dwChange = 1;   // we're now an add for the redo

               // remember that will have to sync to world
               m_fNeedToSyncToWorld = TRUE;

               dwRemoved++;
            }
            break;
         }
      } // puc->dwObject == 0

   }

   // put a new undo ScenePacket in
   PCUndoScenePacket pOld;
   pOld = m_pUndoCur;
   // make a new m_pUndoCur
   m_pUndoCur = new CUndoScenePacket;

   // set the selection in the world
   SelectionClear ();
   for (i = 0; i < pOld->m_lSCENESEL.Num(); i++) {
      SCENESEL *pg = (SCENESEL*) pOld->m_lSCENESEL.Get(i);

      PCScene pScene = SceneGet (SceneFind (&pg->gScene));
      PCSceneObj pSceneObj = pScene ? pScene->ObjectGet(pScene->ObjectFind (&pg->gObjectWorld)) : NULL;
      DWORD dwIndex = pSceneObj ? pSceneObj->ObjectFind (&pg->gObjectAnim) : -1;
      PCAnimSocket pAnimObj = (dwIndex != -1)  ? pSceneObj->ObjectGet (dwIndex) : NULL;

      if (pAnimObj)
         SelectionAdd (&pg->gScene, &pg->gObjectWorld, &pg->gObjectAnim);
   }


   // copy the old selection info to overwrite the current selection
   pOld->m_lSCENESEL.Init (sizeof(SCENESEL), UPSel.m_lSCENESEL.Get(0), UPSel.m_lSCENESEL.Num());

   // move pOld into the redo buffer
   if (fUndoIt)
      m_listPCRedoScenePacket.Add (&pOld);
   else
      m_listPCUndoScenePacket.Add (&pOld);


   // remember that dirty
   DirtySet (TRUE);

   // notify the world of new objects and selections
   NotifySockets ((dwAdded ? WORLDC_OBJECTADD : 0) | (dwRemoved ? WORLDC_OBJECTREMOVE : 0) |
      (dwChanged ? (WORLDC_OBJECTCHANGESEL | WORLDC_OBJECTCHANGENON) : 0),
      NULL, NULL, NULL);

   // notify the world that the undo buffer has changed
   NotifySocketsUndo ();

   // sync this to the world
   SyncToWorld ();

   return TRUE;
}


/************************************************************************************
CSceneSet::StateGet - Returns the current scene and time (in the scene) being used.

inputs
   PCScene  *ppScene - Filled with the current scene. MIght be null
   fp       *pfTime - Filled with the current ime
returns
   none
*/
void CSceneSet::StateGet (PCScene *ppScene, fp *pfTime)
{
   if (ppScene)
      *ppScene = m_pCurScene;
   if (pfTime)
      *pfTime = m_fCurTime;
}


/************************************************************************************
CSceneSet::StateSet - Sets the current scene and or time. Changing this may cause
the SyncToWorld to be called.

inputs
   PCScene     pScene - New scene
   fp          fTime - New time
returns
   BOOL - TRUE if success
*/
BOOL CSceneSet::StateSet (PCScene pScene, fp fTime)
{
   BOOL fChangeScene = FALSE, fChangeTime = FALSE;

   if (pScene != m_pCurScene) {
      // if have a scene, make sure it's all synced up with what have in the world
      if (m_pCurScene)
         SyncFromWorld ();

      m_pCurScene = pScene;
      m_fNeedToSyncToWorld = TRUE; // need to sync
      fChangeScene = TRUE;
   }

   if (fTime != m_fCurTime) {
      m_fCurTime = fTime;

      // need to sync
      m_fNeedToSyncToWorld = TRUE;
      fChangeTime = TRUE;
   }

   // sync to this world
   SyncToWorld ();

   // notify
   if (fChangeScene || fChangeTime) {
      DWORD i, dwNum;
      PCViewSceneSocket *pp;
      pp = (PCViewSceneSocket*) m_listViewSceneSocket.Get(0);
      dwNum = m_listViewSceneSocket.Num();

      for (i = 0; i < dwNum; i++)
         (pp[i]->SceneStateChanged)(fChangeScene, fChangeTime);
   }

   return TRUE;
}

/************************************************************************************
CSceneSet::SyncFromWorld - if any object in world have changed since last time was
called will sync them up - changes the scene

This:
1) Makes sure stuff needs to change.
2) Sets flag so that undo wont be remembered
3) Syncs up from the world
4) Clears the sync list
5) Sets flag so undo will be remembered and clears undo
6) Sets flag so wont call SyncToWorld() because of the changes from this - and hence a vicious circle
*/
void CSceneSet::SyncFromWorld (void)
{
   // if no changes then ignore
   if ((!m_fWorldChangedCompletely && !m_lWorldObjectGUID.Num()) || !m_pCurScene)
      return;

   // else, sync

   // set flag so undo not remembered
   BOOL fUndo;
   fUndo = m_fKeepUndo;
   m_fKeepUndo = FALSE;

   // sync up
   m_pCurScene->SyncFromWorld (m_lWorldObjectGUID.Num(),
      m_fWorldChangedCompletely ? NULL : (GUID*) m_lWorldObjectGUID.Get(0));

   // clear up sync list
   m_fWorldChangedCompletely = FALSE;
   m_lWorldObjectGUID.Clear();


   // restore keep undo flag
   m_fKeepUndo = fUndo;
   UndoClear (TRUE, TRUE);

   // dont need to sync to the world now
   m_fNeedToSyncToWorld = FALSE;
}


/************************************************************************************
CSceneSet::CameraSet - Sets the camera that will use when rendering.

inputs
   GUID        *pgCamera - Camera's object GUID. If NULL then no camera
returns
   none
*/
void CSceneSet::CameraSet (GUID *pgCamera)
{
   if (pgCamera) {
      if (m_fCamera && IsEqualGUID(m_gCamera, *pgCamera))
         return;  // no change

      m_fCamera = TRUE;
      m_gCamera = *pgCamera;
   }
   else { // setting to no camera
      if (!m_fCamera)
         return;  // no change
      m_fCamera = FALSE;
   }

   DirtySet (TRUE);
   m_fNeedToSyncToWorld = TRUE;
}


/************************************************************************************
CSceneSet::CameraGet - Fills in pgCamera with the current camera object being
used.

inputs
   GUID        *pgCamera - If a camera object is used will be filled in. Else return false
returns
   BOOL - TRUE if pgCamera filled in. FALSE if no camera
*/
BOOL CSceneSet::CameraGet (GUID *pgCamera)
{
   if (!m_fCamera)
      return FALSE;

   *pgCamera = m_gCamera;
   return TRUE;
}

/************************************************************************************
CSceneSet::SyncToWorld - Given the current scene and time, this sends it to the
world.

It:
1) Tells the world not to remember undo
2) Sets a flag so any notifications from the world about changes will be ignored
3) Has the scene sync up
4) Clears the world's undo/redo buffer, and own internal m_lWorldObjecGUID and m_fWorldChangedCompletely
5) Restores the world's remember undo flag, and ignore notification flag
*/
void CSceneSet::SyncToWorld (void)
{
   if (!m_pWorld || !m_pCurScene || !m_fNeedToSyncToWorld)
      return;

   m_fNeedToSyncToWorld = FALSE;

   // make sure world doesnt remember this
   BOOL fRemember;
   fRemember = m_pWorld->m_fKeepUndo;
   m_fKeepUndo = FALSE;
   m_fIgnoreWorldNotify = TRUE;

   // sync up - might as well do everything
   m_pCurScene->SyncToWorld (0, NULL);

   // clear the wor'd undo/redo buffer and own internal memory of objects that have changed
   m_pWorld->UndoClear (TRUE, TRUE);
   m_lWorldObjectGUID.Clear();
   m_fWorldChangedCompletely = FALSE;

   // restore world's membory
   m_fKeepUndo = fRemember;
   m_fIgnoreWorldNotify = FALSE;

   // force a redraw
   if (m_fFastRefresh) {
      PCHouseView pv;
      pv = FindViewForWorld (m_pWorld);
      if (pv) {
         if (m_fCamera)
            pv->CameraFromObject (&m_gCamera);
         pv->ForcePaint();
      }
   }
}

/******************************************************************************
CSceneSet::WorldAboutToChange - Callback from world
*/
void CSceneSet::WorldAboutToChange (GUID *pgObject)
{
   // may want to ignore this if doing a SyncToWorld
   // BUGFIX - Don't sync unless have a scene and have a scene selected
   if (m_fIgnoreWorldNotify || !pgObject || !m_lPCScene.Num() || !m_pCurScene)
      return;

   // BUGFIX - make sure have attributes cached as defaults
   if (!DefaultAttribExist (pgObject))
      DefaultAttribSet (pgObject, NULL, 0);  // so sync all up
}


/************************************************************************************
CSceneSet::WorldChanged - Callback from m_pWorld. This keeps track of which objects
have changed, so that when syncFromWorld() is called, it knows
*/
void CSceneSet::WorldChanged (DWORD dwChanged, GUID *pgObject)
{
   // may want to ignore this if doing a SyncToWorld
   if (m_fIgnoreWorldNotify)
      return;

   // if didn't change object bits then ignore
   if (!(dwChanged & (WORLDC_OBJECTADD | WORLDC_OBJECTREMOVE | WORLDC_OBJECTCHANGESEL | WORLDC_OBJECTCHANGENON)))
      return;  // NOTE: Ignoring changes of cameras because moved when views move

   // if already marked as having chnaged completley dont do anything
   if (m_fWorldChangedCompletely)
      return;

   // if the object guid is NULL then just set flag that everything has changed and give up
   if (!pgObject) {
      m_fWorldChangedCompletely = TRUE;
      m_lWorldObjectGUID.Clear();
      return;
   }

   // else, add objects
   GUID *pag;
   DWORD dwNum, i;
   pag = (GUID*) m_lWorldObjectGUID.Get(0);
   dwNum = m_lWorldObjectGUID.Num();
   for (i = 0; i < dwNum; i++)
      if (IsEqualGUID (pag[i], *pgObject))
         break;
   if (i >= dwNum)
      m_lWorldObjectGUID.Add (pgObject);
}

/************************************************************************************
CSceneSet::WorldUndoChanged - Ignored
*/
void CSceneSet::WorldUndoChanged (BOOL fUndo, BOOL fRedo)
{
   // may want to ignore this if doing a SyncToWorld
   if (m_fIgnoreWorldNotify)
      return;

   // do nothing
}

/************************************************************************************
CSceneSet::DefaultAttribIndex - internal function that returns the index into
m_lSSDATTRIB for the attribute with the given GUID.

inputs
   GUID        *pgObjectWorld - looking for
   BOOL        fExact - If TRUE want an exact match. Otherwise, return the index to
               the one to insert before
returns
   DWORD - index, or -1 if cant find
*/
DWORD CSceneSet::DefaultAttribIndex (GUID *pgObjectWorld, BOOL fExact)
{
   PSSDATTRIB pa = (PSSDATTRIB) m_lSSDATTRIB.Get(0);
   DWORD dwNum = m_lSSDATTRIB.Num();
   DWORD dwCur, dwTry, dwIndex;
   int iRet;
   if (!dwNum)
      return -1;

   for (dwCur = 1; dwCur < dwNum; dwCur *= 2);
   for (dwIndex = 0; dwCur; dwCur /= 2) {
      dwTry = dwIndex + dwCur;
      if (dwTry >= dwNum)
         continue;

      iRet = memcmp(pgObjectWorld, &pa[dwTry].gObjectWorld, sizeof(*pgObjectWorld));
      if (!iRet)
         return dwTry;  // found exact match

      if (iRet > 0)
         dwIndex = dwTry;
   }

   // if match keep
   iRet = memcmp(pgObjectWorld, &pa[dwIndex].gObjectWorld, sizeof(*pgObjectWorld));
   if (iRet == 0)
      return dwIndex;
   if (fExact)
      return -1; // would have had exact by now

   if (iRet > 0)
      dwIndex++;
   if (dwIndex >= dwNum)
      return -1;
   else
      return dwIndex; // BUGFIX - Was : (dwIndex+1)
}

/************************************************************************************
CSceneSet::DefaultAttribExist - Returns TRUE if the object has some default attributes
set for it.

inputs
   GUID        *pgObjectWorld - Object looking for
returns
   BOOL - TRUE if set for it
*/
BOOL CSceneSet::DefaultAttribExist (GUID *pgObjectWorld)
{
   DWORD dwFind = DefaultAttribIndex (pgObjectWorld);
   return (dwFind != -1);
}

/************************************************************************************
CSceneSet::DefaultAttribGetAll - Gets all the default attributes remembers for the
object and fills them in plATTRIBVAL.

inputs
   GUID        *pgObjectWorld - Object looking for
   PCListFixed plATTRIBVAL - Initialized to sizeof(ATTRIBVAL) and fileld in
   BOOL        fCreateIfNotExist - If TRUE and there's no entry for the object then this
               automagically creates entries for the object.
returns
   BOOL - TRUE if found the object had defaults
*/
BOOL CSceneSet::DefaultAttribGetAll (GUID *pgObjectWorld, PCListFixed plATTRIBVAL)
{
   DWORD dwFind = DefaultAttribIndex (pgObjectWorld);
   plATTRIBVAL->Init (sizeof(ATTRIBVAL));
   plATTRIBVAL->Clear();
   if (dwFind == -1)
      return FALSE;

   PSSDATTRIB pa;
   pa = (PSSDATTRIB) m_lSSDATTRIB.Get(dwFind);
   plATTRIBVAL->Init (sizeof(ATTRIBVAL), pa->plATTRIBVAL->Get(0), pa->plATTRIBVAL->Num());
   return TRUE;
}

/*********************************************************************************
ATTRIBVALSearch - Given a string and a list of ATTRIBVAL, finds the index into
the values where a match is, or the one to insert before.

inputs
   PWSTR          pszFind - Look for this
   PCListFixed    plATTRIBVAL - List of attributes
   BOOL           fExact - If TRUE, return -1 if not an exact match, else return one to insert before
returns
   DWORD - Index into plATTRIBVAL
*/
static DWORD ATTRIBVALSearch (PWSTR pszFind, PCListFixed plATTRIBVAL, BOOL fExact)
{
   PATTRIBVAL pa = (PATTRIBVAL) plATTRIBVAL->Get(0);
   DWORD dwNum = plATTRIBVAL->Num();
   DWORD dwCur, dwTry, dwIndex;
   int iRet;
   if (!dwNum)
      return -1;

   for (dwCur = 1; dwCur < dwNum; dwCur *= 2);
   for (dwIndex = 0; dwCur; dwCur /= 2) {
      dwTry = dwIndex + dwCur;
      if (dwTry >= dwNum)
         continue;

      iRet = _wcsicmp (pszFind, pa[dwTry].szName);
      if (!iRet)
         return dwTry;  // found exact match

      if (iRet > 0)
         dwIndex = dwTry;
   }

   // if match keep
   iRet = _wcsicmp(pszFind, pa[dwIndex].szName);
   if (iRet == 0)
      return dwIndex;

   // if get to here then haven't found exact match
   if (fExact)
      return -1;

   if (iRet > 0)
      dwIndex++;
   if (dwIndex >= dwNum)
      return -1;
   else
      return dwIndex; // BUGFIX - Was : (dwIndex+1)
}

/************************************************************************************
CSceneSet::DefaultAttribGet - Gets a specific attribute from an objec.

inputs
   GUID        *pgObjectWorld - Object looking for
   PWSTR       pszAttrib - Attrib looking for
   fp          *pfValue - Filled in with the default attribute value (if one exists)
   BOOL        fAutoRemember - If FALSE and a default attribute doesnt exist then the function
                  willr eturn FALSE. If TRUE and a default attribute doesnt exist
                  then the current attribute is gotten from the object. This is then
                  stored in the list. Sets dirtyflag too.
returns
   BOOL - TRUE if pfValue has been filled in properly
*/
BOOL CSceneSet::DefaultAttribGet (GUID *pgObjectWorld, PWSTR pszAttrib, fp *pfValue, BOOL fAutoRemember)
{
   // see if happen to use the same object as used last time
   DWORD dwFind = -1;
   PSSDATTRIB pa;
   if (m_dwLastDefault < m_lSSDATTRIB.Num()) {
      pa = (PSSDATTRIB) m_lSSDATTRIB.Get(m_dwLastDefault);
      if (IsEqualGUID(pa->gObjectWorld, *pgObjectWorld))
         dwFind = m_dwLastDefault;
   }

   if (dwFind == -1)
      dwFind = DefaultAttribIndex (pgObjectWorld);
   *pfValue = 0;
   if (dwFind == -1) {
      if (fAutoRemember)
         goto autoremember;
      return FALSE;
   }
   m_dwLastDefault = dwFind;  // remember for faster searches
   pa = (PSSDATTRIB) m_lSSDATTRIB.Get(dwFind);
   dwFind = ATTRIBVALSearch (pszAttrib, pa->plATTRIBVAL, TRUE);
   if (dwFind == -1) {
      if (fAutoRemember)
         goto autoremember;
      return FALSE;
   }

   // else, exact match
   *pfValue = ((PATTRIBVAL) pa->plATTRIBVAL->Get(dwFind))->fValue;
   return TRUE;

autoremember:
   // called if didnt find attribute but automatically want to add it
   if (!m_pWorld)
      return FALSE;
   dwFind = m_pWorld->ObjectFind (pgObjectWorld);
   if (dwFind == -1)
      return FALSE;
   PCObjectSocket pos;
   pos = m_pWorld->ObjectGet (dwFind);
   if (!pos)
      return FALSE;

   if (!pos->AttribGet (pszAttrib, pfValue))
      return FALSE;

   // write it out
   return DefaultAttribSet (pgObjectWorld, pszAttrib, *pfValue);
}

/************************************************************************************
CSceneSet::DefaultAttribSet - Sets a specific attribute from an object. If the
   attribute doesnt exist it's created.

inputs
   GUID        *pgObjectWorld - Object looking for
   PWSTR       pszAttrib - Attrib looking for
                  If pszAttrib is NULL then all attributes are synced up
   fp          fValue - Filled in with the default attribute value (if one exists)
returns
   BOOL - TRUE if succeded
*/
BOOL CSceneSet::DefaultAttribSet (GUID *pgObjectWorld, PWSTR pszAttrib, fp fValue)
{
   // see if happen to use the same object as used last time
   DWORD dwFind = -1;
   PSSDATTRIB pa;
   if (m_dwLastDefault < m_lSSDATTRIB.Num()) {
      pa = (PSSDATTRIB) m_lSSDATTRIB.Get(m_dwLastDefault);
      if (IsEqualGUID(pa->gObjectWorld, *pgObjectWorld))
         dwFind = m_dwLastDefault;
   }

   if (dwFind == -1)
      dwFind = DefaultAttribIndex (pgObjectWorld);
   if (dwFind == -1) {
      // add it
      dwFind = DefaultAttribIndex (pgObjectWorld, FALSE);

      SSDATTRIB ss;
      memset (&ss, 0, sizeof(ss));
      ss.gObjectWorld = *pgObjectWorld;
      ss.plATTRIBVAL = new CListFixed();
      if (!ss.plATTRIBVAL)
         return FALSE;
      ss.plATTRIBVAL->Init (sizeof(ATTRIBVAL));

      if (dwFind == -1) {
         // since searched without exact returned false, add to end
         dwFind = m_lSSDATTRIB.Num();
         m_lSSDATTRIB.Add (&ss);
      }
      else {
         // insert
         m_lSSDATTRIB.Insert (dwFind, &ss);
      }
   }

   // get it
   PSSDATTRIB pss;
   pss = (PSSDATTRIB) m_lSSDATTRIB.Get(dwFind);
   m_dwLastDefault = dwFind;  // remember for faster searches

   if (pszAttrib) {
      // look for the specific attribute
      dwFind = ATTRIBVALSearch (pszAttrib, pss->plATTRIBVAL, TRUE);
      if (dwFind != -1) {
         // found it, so just set
         PATTRIBVAL pv = (PATTRIBVAL) pss->plATTRIBVAL->Get(dwFind);
         pv->fValue = fValue;
         DirtySet (TRUE);
         return TRUE;
      }

      // else, couldnt find, so add
      ATTRIBVAL av;
      memset (&av, 0, sizeof(av));
      wcscpy (av.szName, pszAttrib);
      av.fValue = fValue;
      dwFind = ATTRIBVALSearch (pszAttrib, pss->plATTRIBVAL, FALSE);
      if (dwFind == -1)
         pss->plATTRIBVAL->Add (&av);  // need to add to the end
      else
         pss->plATTRIBVAL->Insert (dwFind, &av);   // insert
   }
   else {
      // just sync up all existing attributes
      PCObjectSocket pos = m_pWorld->ObjectGet(m_pWorld->ObjectFind(pgObjectWorld));
      if (!pos)
         return FALSE;

      // get them
      pss->plATTRIBVAL->Init (sizeof(ATTRIBVAL));
      pss->plATTRIBVAL->Clear();
      pos->AttribGetAll (pss->plATTRIBVAL);

      // sort them
      qsort (pss->plATTRIBVAL->Get(0), pss->plATTRIBVAL->Num(), sizeof(ATTRIBVAL), ATTRIBVALCompare);
   }
   DirtySet (TRUE);

   return TRUE;
}

/************************************************************************************
CSceneSet::DefaultAttribForget - If an attribute is remembered then forget that it
exists.

inputs
   GUID        *pgObjectWorld - Object looking for
   PWSTR       pszAttrib - Attrib looking for. If NULL then clear all
returns
   BOOL - TRUE if succeded
*/
BOOL CSceneSet::DefaultAttribForget (GUID *pgObjectWorld, PWSTR pszAttrib)
{
   DWORD dwFind = DefaultAttribIndex (pgObjectWorld);
   if (dwFind == -1)
      return FALSE;  // never existed in first place

   // get it
   PSSDATTRIB pss;
   pss = (PSSDATTRIB) m_lSSDATTRIB.Get(dwFind);

   // if pszAttrib is NULL may want to forget everything
   if (pszAttrib) {
      // look for the specific attribute
      dwFind = ATTRIBVALSearch (pszAttrib, pss->plATTRIBVAL, TRUE);
      if (dwFind == -1)
         return FALSE;  // never existed in first place

      // else remove
      pss->plATTRIBVAL->Remove (dwFind);
   }
   else
      pss->plATTRIBVAL->Clear();

   DirtySet (TRUE);
   m_fNeedToSyncToWorld = TRUE;  // need to sync to the world
   return TRUE;
}


/**************************************************************************************
CUndoScenePacket::Constructor and destructor. Initializes the object. The destructor
frees all the objects that are there.
*/
CUndoScenePacket::CUndoScenePacket ()
{
   m_lUNDOSCENECHANGE.Init (sizeof(UNDOSCENECHANGE));
   m_lSCENESEL.Init (sizeof(SCENESEL));
}

CUndoScenePacket::~CUndoScenePacket ()
{
   DWORD i;
   for (i = 0; i < m_lUNDOSCENECHANGE.Num(); i++) {
      PUNDOSCENECHANGE pu = (PUNDOSCENECHANGE) m_lUNDOSCENECHANGE.Get(i);
      if (pu->pObject)
         pu->pObject->Delete();
   }
}


/**************************************************************************************
CUndoScenePacket::Change - Record a change. As objects are changed by the user this should
be called.

inputs
   PCSceneSet        pSceneSet - Used to get the selectin from
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
void CUndoScenePacket::Change (PCSceneSet pSceneSet, PCAnimSocket pObject, BOOL fBefore, DWORD dwChange)
{
   // get the object's GUID and see what was there before
   GUID gScene, gObjectWorld, gObjectAnim;
   PCScene pScene;
   pObject->GUIDGet (&gObjectAnim);
   pObject->WorldGet (NULL, &pScene, &gObjectWorld);
   pScene->GUIDGet (&gScene);

   // find a match
   DWORD i, dwUndo;
   PUNDOSCENECHANGE pUndo;
   for (i = 0; i < m_lUNDOSCENECHANGE.Num(); i++) {
      pUndo = (PUNDOSCENECHANGE) m_lUNDOSCENECHANGE.Get(i);
      if ((pUndo->dwObject == 0) && IsEqualGUID(pUndo->gObjectAnim, gObjectAnim) &&
         IsEqualGUID(pUndo->gObjectWorld, gObjectWorld) &&IsEqualGUID(pUndo->gScene, gScene))
         break;
   }
   if (i >= m_lUNDOSCENECHANGE.Num())
      pUndo = NULL;  // couldnt find
   dwUndo = i;

   UNDOSCENECHANGE uc;
   memset (&uc, 0, sizeof(uc));
   uc.gObjectAnim = gObjectAnim;
   uc.gObjectWorld = gObjectWorld;
   uc.gScene = gScene;
   //uc.pObject = pObject;
   uc.dwChange = dwChange;
   uc.dwObject = 0;

   if (dwChange == 0) { // changed
      if (!pUndo) {
         if (fBefore) {
            // consider capturing the selection
            if (!m_lUNDOSCENECHANGE.Num())
               FillSelectionList(pSceneSet);

            // only add it if its market as the before event
            uc.pObject = pObject->Clone();
            if (!uc.pObject)
               return;  // didnt clone
            uc.pObject->WorldSet (NULL, NULL, &gObjectWorld);  // so doesnt call back in
            m_lUNDOSCENECHANGE.Add (&uc);
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
      if (!m_lUNDOSCENECHANGE.Num())
         FillSelectionList(pSceneSet);

      //uc.pObject = pObject->Clone();
      //uc.pObject->WorldSet (NULL);  // so doesnt call back in
      uc.pObject = NULL;   // since added it, don't really need to store this away. See undo later
      m_lUNDOSCENECHANGE.Add (&uc);
      return;
   }
   else if (dwChange == 2) {  // removed

      // if it doesnt exist then clone and keep
      if (!pUndo) {
         // consider capturing the selection
         if (!m_lUNDOSCENECHANGE.Num())
            FillSelectionList(pSceneSet);

         uc.pObject = pObject->Clone();
         if (!uc.pObject)
            return;
         uc.pObject->WorldSet (NULL, pScene, &gObjectWorld);  // so doesnt call back in
         m_lUNDOSCENECHANGE.Add (&uc);
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
      m_lUNDOSCENECHANGE.Remove (dwUndo);
      return;
   }
}



/************************************************************************************
CUndoScenePacket::FillSelectionList - Internal function called the FIRST time something
is really added to the list which Changed(). That way, when all those changes are
undone, the system will know what the selection was like at that point and can restore it.
*/
void CUndoScenePacket::FillSelectionList (PCSceneSet pSceneSet)
{
   DWORD dwNum;
   PSCENESEL ps;
   ps = pSceneSet->SelectionEnum(&dwNum);
   m_lSCENESEL.Init (sizeof(SCENESEL), ps, dwNum);
}


// BUGBUG - In SyncToWorld() - If an object is attached to another object then set
// the attached objects first followed by the non-attached - that way attached
// objects will move more in sync
