/************************************************************************
CObjectClone.cpp - Code for handling cloned objects - which are created in
bulk.

begun 7/3/03 by Mike Rozak
Copyright 2003 Mike Rozak. All rights reserved
*/

#include <windows.h>
#include "escarpment.h"
#include "..\M3D.h"
#include "resource.h"





/* globals */
static CListFixed    galClones[MAXRENDERSHARDS];         // list of PCObjectClone
static BOOL          gafClonesValid[MAXRENDERSHARDS] = {FALSE, FALSE, FALSE, FALSE};    // set to true if the clones are valid


/******************************************************************************
ObjectCloneGet - Given the guids that identify the object class, this returns
a PCObjectClone object that is a clone - used for minimizing memory resource.
NOTE: Do NOT delete the clone. Call Release(). Other functions might be using the
same one.

inputs
   GUID        *pgCode - Code looking for
   GUID        *pgSub - Subcode looking for
   BOOL        fCreateIfNotExist - If TRUE (default) the clone will be created if it's
                  not already in the database.
retuyrns
   PCObjectClone - Clone. NULL if error
*/
PCObjectClone ObjectCloneGet (DWORD dwRenderShard, GUID *pgCode, GUID *pgSub, BOOL fCreateIfNotExist)
{
   PCObjectClone *ppc = (PCObjectClone*) galClones[dwRenderShard].Get(0);
   DWORD i;
   for (i = 0; i < galClones[dwRenderShard].Num(); i++) {
      if (IsEqualGUID (*pgSub, ppc[i]->m_gSub) && IsEqualGUID (*pgCode, ppc[i]->m_gCode)) {
         ppc[i]->AddRef();
         return ppc[i];
      }
   }

   //else cant find
   if (!fCreateIfNotExist)
      return NULL;

   // create
   PCObjectClone pc;
   pc = new CObjectClone;
   if (!pc)
      return NULL;
   if (!pc->Init (dwRenderShard, pgCode, pgSub)) {
      delete pc;
      return NULL;
   }

   return pc;
}

/******************************************************************************
CObjectClone::Constructor and destructor

NOTE: Do NOT call the destructor directly. Call release
*/
CObjectClone::CObjectClone (void)
{
   memset (&m_gCode, 0, sizeof(m_gCode));
   memset (&m_gSub, 0, sizeof(m_gSub));
   m_dwRenderShard = (DWORD)-1;  // so has to set
   m_dwRefCount = 0;
   m_World.m_fKeepUndo = FALSE;
   m_fDetail = 0;
   m_pmApply = NULL;
   m_pRender = NULL;
   m_fInLoop = FALSE;
   m_dwID = 0;
   m_fMaxSize = 0;
}

CObjectClone::~CObjectClone (void)
{
   if (m_dwRenderShard == (DWORD)-1)
      return;  // not initialized

   // remove self from the list
   PCObjectClone *ppc = (PCObjectClone*) galClones[m_dwRenderShard].Get(0);
   DWORD i;
   for (i = 0; i < galClones[m_dwRenderShard].Num(); i++)
      if (ppc[i] == this) {
         galClones[m_dwRenderShard].Remove (i);
         break;
      }
}

/******************************************************************************
CObjectClone::Init - initalizes the clone to use the code and sub-code. If this
fails it returns false and the caller should DELETE the object. If it successed
the caller should call Release() to delete.

inputs
   GUID        *pgCode - Major code of object
   GUID        *pgSub - Sub-code of object
returns
   BOOL - True if success
*/
BOOL CObjectClone::Init (DWORD dwRenderShard, GUID *pgCode, GUID *pgSub)
{
   // cant call init a second time
   if (m_World.ObjectNum())
      return FALSE;

   m_dwRenderShard = dwRenderShard;
   m_World.RenderShardSet (dwRenderShard);
   m_gCode = *pgCode;
   m_gSub = *pgSub;

   if (!UpdateToNewObject(TRUE))
      return FALSE;


   // add self to list
   PCObjectClone pThis;
   pThis = this;
   if (!gafClonesValid[dwRenderShard]) {
      gafClonesValid[dwRenderShard] = TRUE;
      galClones[dwRenderShard].Init (sizeof(PCObjectClone));
   }
   galClones[dwRenderShard].Add (&pThis);

   return TRUE;
}

/******************************************************************************
CObjectClone::BoundingBoxGet - Returns the bounding box for this object.

inptus
   PCMatrix       pm - Filled with the matrix that converts the corners into "world space"...
                  Although since the clone does not really know where it is, another matrix
                  multiply will be needed
   PCPoint        pCorner1, pCorner2 - Filled with two opposite corners.
returns
   BOOL - TRUE if a bounding box is gotten and the values are filled in
*/
BOOL CObjectClone::BoundingBoxGet (CMatrix *pm, PCPoint pCorner1, PCPoint pCorner2)
{
   return m_World.BoundingBoxGet (0, pm, pCorner1, pCorner2);
}

/******************************************************************************
CObjectClone::AddRef - Increases the reference count by 1. Returns the
new refrence count
*/
DWORD CObjectClone::AddRef (void)
{
   m_dwRefCount++;
   return m_dwRefCount;
}


/******************************************************************************
CObjectClone::Release - Decreases the reference count. If this goes to 0 then it
deletes the object. Returns the reference count after the release
*/
DWORD CObjectClone::Release (void)
{
   if (m_dwRefCount)
      m_dwRefCount--;
   if (!m_dwRefCount) {
      delete this;
      return 0;
   }
   else
      return m_dwRefCount;
}

/******************************************************************************
CObjectClone::Render - Causes the object (and only one object) in the world to
draw itself into the information defined by pr.

inputs
   POBJECTRENDER        pr - Information about rendering
   DWORD                dwID - Use this ID to replace the ID for the surfaces - so won't
                           be able to click on this surface and change the color
   PCMatrix             pmApply - Matrix to apply to all matrixset calls from the obect.
                        If this is NULL then an identity matrix is used.
   fp                   fDetail - Detail level to return to the object when it asks for detail.
                        If this is less than 0 then just uses the detail from the calling render.
*/
void CObjectClone::Render (POBJECTRENDER pr, DWORD dwID, PCMatrix pmApply, fp fDetail)
{
   if (m_fInLoop)
      return;

   m_fInLoop = TRUE;
   m_pmApply = pmApply;
   m_dwID = dwID;
   if (fDetail < 0)
      m_fDetail = pr->pRS->QueryDetail ();
   else
      m_fDetail = fDetail;
   
   // duplicate by modify render info
   OBJECTRENDER or;
   or = *pr;
   or.pRS = this;
   m_pRender = pr->pRS;

   DWORD i;
   PCObjectSocket pos;
   for (i = 0; i < m_World.ObjectNum(); i++) {
      pos = m_World.ObjectGet (i);
      if (!pos)
         continue;

      // if not rendering this then ignore
      if (!(pr->dwShow & pos->CategoryQuery()))
         continue;

      // pass in the matrix just in case callee doesnt
      if (m_pmApply)
         m_pRender->MatrixSet (m_pmApply);
      else {
         CMatrix mIdent;
         mIdent.Identity();
         m_pRender->MatrixSet (&mIdent);
      }

      pos->Render (&or, (DWORD)-1);
   }

   // finished
   m_pmApply = NULL;
   m_pRender = NULL;
   m_fInLoop = FALSE;
}

/******************************************************************************
CObjectClone::TextureQuery - Just like in CObjectSocket
*/
BOOL CObjectClone::TextureQuery (PCListFixed plText)
{
   BOOL fRet = FALSE;
   if (m_fInLoop)
      return FALSE;
   m_fInLoop = TRUE;

   DWORD i;
   PCObjectSocket pos;
   for (i = 0; i < m_World.ObjectNum(); i++) {
      pos = m_World.ObjectGet (i);
      if (!pos)
         continue;

      fRet |= pos->TextureQuery (plText);
   }

   m_fInLoop = FALSE;
   return fRet;
}

/******************************************************************************
CObjectClone::ColorQuery - Just like in CObjectSocket
*/
BOOL CObjectClone::ColorQuery (PCListFixed plColor)
{
   BOOL fRet = FALSE;
   if (m_fInLoop)
      return FALSE;
   m_fInLoop = TRUE;

   DWORD i;
   PCObjectSocket pos;
   for (i = 0; i < m_World.ObjectNum(); i++) {
      pos = m_World.ObjectGet (i);
      if (!pos)
         continue;

      fRet |= pos->ColorQuery (plColor);
   }

   m_fInLoop = FALSE;
   return fRet;
}

/******************************************************************************
CObjectClone::ObjectClassQuery - Just like in CObjectSocket
*/
BOOL CObjectClone::ObjectClassQuery (PCListFixed plObj)
{
   BOOL fRet = FALSE;
   if (m_fInLoop)
      return FALSE;
   m_fInLoop = TRUE;

   DWORD i;
   PCObjectSocket pos;
   for (i = 0; i < m_World.ObjectNum(); i++) {
      pos = m_World.ObjectGet (i);
      if (!pos)
         continue;

      fRet |= pos->ObjectClassQuery (plObj);
   }

   m_fInLoop = FALSE;
   return fRet;
}



/******************************************************************************
CObjectClone::UpdateToNewObject - Call this the object was saved in the object
editor. This will causes the object clone to update

inputs
   BOOL        fReloadObject - If TRUE the world is clear and the object is realoaded.
                  This be set to FALSE if updating because object in editor has changed,
                  so that it only recalcs the bounding box
returns
   BOOL - TRUE if success, FALSE if fail
*/
BOOL CObjectClone::UpdateToNewObject (BOOL fReloadObject)
{
   if (fReloadObject) {
      if (m_World.ObjectNum())
         m_World.Clear();

      PCObjectSocket pNew;
      pNew = ObjectCFCreate (m_dwRenderShard, &m_gCode, &m_gSub);
      if (!pNew)
         return FALSE;  // error that shouldnt happen
      m_World.ObjectAdd (pNew);
      pNew->WorldSetFinished();  // do this now since wont contain anything

      // make sure at 0,0,0
      CMatrix mIdent;
      mIdent.Identity();
      pNew->ObjectMatrixSet (&mIdent);

      // increase reference count
      m_dwRefCount = 1;
   }

   // get the maximum size
   CMatrix m;
   CPoint apc[2], p;
   DWORD x,y,z;
   BoundingBoxGet (&m, &apc[0], &apc[1]);
   fp fLen;
   m_fMaxSize = 0;
   for (x = 0; x < 2; x++) for (y = 0; y < 2; y++) for (z = 0; z < 2; z++) {
      p.p[0] = apc[x].p[0];
      p.p[1] = apc[y].p[1];
      p.p[2] = apc[z].p[2];
      p.p[3] = 1;
      p.MultiplyLeft (&m);
      fLen = p.Length();
      m_fMaxSize = max(m_fMaxSize, fLen);
   }

   return TRUE;
}

/******************************************************************************
CObjectClone::MaxSize - Returns the maximum distance from 0,0,0 for the clone.
This is a quick way of including trees in the bounding box size
*/
fp CObjectClone::MaxSize (void)
{
   return m_fMaxSize;
}


/******************************************************************************
CObjectClone::QueryWantNormals - Pass through to the renderer
*/
BOOL CObjectClone::QueryWantNormals (void)
{
   return m_pRender->QueryWantNormals();
}

/******************************************************************************
CObjectClone::QueryWantTextures - Pass through to the renderer
*/
BOOL CObjectClone::QueryWantTextures (void)
{
   return m_pRender->QueryWantTextures();
}

/******************************************************************************
CObjectClone::QuerySubDetail - From CRenderSocket. Basically end up ignoring
*/
BOOL CObjectClone::QuerySubDetail (PCMatrix pMatrix, PCPoint pBound1, PCPoint pBound2, fp *pfDetail)
{
   *pfDetail = QueryDetail();
   return TRUE;
}

/******************************************************************************
CObjectClone::QueryCloneRender - From CRenderSocket
*/
BOOL CObjectClone::QueryCloneRender (void)
{
   return m_pRender->QueryCloneRender();
}


/******************************************************************************
CObjectClone::CloneRender - From CRenderSocket
*/
BOOL CObjectClone::CloneRender (GUID *pgCode, GUID *pgSub, DWORD dwNum, PCMatrix pamMatrix)
{
   if (!m_pmApply) {
      return m_pRender->CloneRender (pgCode, pgSub, dwNum, pamMatrix);
   }

   // create scratch matricies
   CListFixed lm;
   DWORD i;
   PCMatrix pm;
   lm.Init (sizeof(CMatrix), pamMatrix, dwNum);
   pm = (PCMatrix) lm.Get(0);

   // else apply own matrix
   for (i = 0; i < lm.Num(); i++, pm++)
      pm->MultiplyRight (m_pmApply);

   return m_pRender->CloneRender (pgCode, pgSub, lm.Num(), (PCMatrix)lm.Get(0));
}

/******************************************************************************
CObjectClone::QueryDetail - Use m_fDetail
*/
fp CObjectClone::QueryDetail (void)
{
   return m_fDetail;
}

/******************************************************************************
CObjectClone::MatrixSet - Pass this through to the renderer, including own matrix.
*/
void CObjectClone::MatrixSet (PCMatrix pm)
{
   if (!m_pmApply) {
      m_pRender->MatrixSet (pm);
      return;
   }

   // else apply own matrix
   CMatrix m;
   m.Multiply (m_pmApply, pm);
   m_pRender->MatrixSet (&m);
}

/******************************************************************************
CObjectClone::Render - Pass through to the renderer
*/
void CObjectClone::PolyRender (PPOLYRENDERINFO pInfo)
{
   // change the ID's
   DWORD i;
   for (i = 0; i < pInfo->dwNumSurfaces; i++)
      pInfo->paSurfaces[i].wMinorID = (WORD) m_dwID;

   m_pRender->PolyRender (pInfo);
}



