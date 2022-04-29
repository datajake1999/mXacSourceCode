/************************************************************************
CObjectTemplate.cpp - Template object which is a base class for most of
the objects written for the system.

begun 12/9/01 by Mike Rozak
Copyright 2001 Mike Rozak. All rights reserved
*/

#include <windows.h>
#include <crtdbg.h>
#include "escarpment.h"
#include "..\M3D.h"
#include "resource.h"


// TATTACH - Information about attached object
typedef struct {
   GUID           gID;        // ID of the attached object
   WCHAR          szAttach[64];   // string indicating what bone (or whatever) it's attached to
   WCHAR          szAttrib[64];  // attribute to turn the attachment on/off
   CMatrix        mAttachToBone; // converts from attach object coord's to bone's coords (named by szAttach)
   BOOL           fUse;       // set to TRUE if on. Can be turned on/off by attribute
} TATTACH, *PTATTACH;

// EMBEDINFO - Information about embedded object
typedef struct {
   GUID           gID;        // ID of the embedded object
   TEXTUREPOINT   tp;         // location in HV of the surface
   DWORD          dwSurface;  // surface that it's embedded in
   fp         fRotation;  // rotation (clockwise) in radians
} EMBEDINFO, *PEMBEDINFO;

typedef struct {
   PWSTR       pszName;    // name of the move point
   int         iX, iY, iZ; // -1 if smaller left/bottom/front corner, 0 if mid point, 1 if right/top/back corner
} DEFMOVEP, *PDEFMOVEP;

static DEFMOVEP   gaDefMoveP[] = {
   L"Center, center, bottom", 0, 0, -1,
   L"Left, front, bottom", -1, -1, -1,
   L"Center, front, bottom", 0, -1, -1,
   L"Right, front, bottom", 1, -1, -1,
   L"Left, center, bottom", -1, 0, -1,
   L"Right, center, bottom", 1, 0, -1,
   L"Left, back, bottom", -1, 1, -1,
   L"Center, back, bottom", 0, 1, -1,
   L"Right, back, bottom", 1, 1, -1,

   L"Center, center, center", 0, 0, 0,
   L"Left, front, center", -1, -1, 0,
   L"Center, front, center", 0, -1, 0,
   L"Right, front, center", 1, -1, 0,
   L"Left, center, center", -1, 0, 0,
   L"Right, center, center", 1, 0, 0,
   L"Left, back, center", -1, 1, 0,
   L"Center, back, center", 0, 1, 0,
   L"Right, back, center", 1, 1, 0,

   L"Center, center, top", 0, 0, 1,
   L"Left, front, top", -1, -1, 1,
   L"Center, front, top", 0, -1, 1,
   L"Right, front, top", 1, -1, 1,
   L"Left, center, top", -1, 0, 1,
   L"Right, center, top", 1, 0, 1,
   L"Left, back, top", -1, 1, 1,
   L"Center, back, top", 0, 1, 1,
   L"Right, back, top", 1, 1, 1
};

/**************************************************************************************
CObjectTemplate::Constructor and destructor */
CObjectTemplate::CObjectTemplate (void)
{
   m_fDeleting = FALSE;
   m_MatrixObject.Identity();
   m_pWorld = NULL;
   m_dwMoveReferenceCur = 0;  // always use this as default
   m_fObjShow = TRUE;
   memset (&m_gGUID, 0, sizeof(m_gGUID));
   m_listPCObjectSurface.Init (sizeof(PCObjectSurface));
   m_fObjectSurfacesSorted = TRUE;
   m_fCanBeEmbedded = FALSE;
   m_fEmbedFitToFloor = FALSE;
   m_fEmbedded = 0;
   m_fContainerDepth = 0;
   m_dwContSideInfoThis = m_dwContSideInfoOther = 0;
   memset (&m_gContainer, 0, sizeof(m_gContainer));
   m_listContSurface.Init (sizeof(DWORD));
   m_listEMBEDINFO.Init (sizeof(EMBEDINFO));
   m_lTATTACH.Init (sizeof(TATTACH));
   m_dwType = 0;
   m_dwRenderShow = RENDERSHOW_MISC;
   m_fAttachRecurse = FALSE;
   m_pPolyMesh = NULL;
   memset (&m_OSINFO, 0, sizeof(m_OSINFO));
   m_OSINFO.dwRenderShard = (DWORD)-1; // just to cause problems
}

CObjectTemplate::~CObjectTemplate (void)
{
   DWORD i;
   PCObjectSocket pos;

   m_fDeleting = TRUE;

   // if contained within an object tell it that deleting
   if (m_fCanBeEmbedded && m_fEmbedded && m_pWorld) {
      pos = m_pWorld->ObjectGet (m_pWorld->ObjectFind(&m_gContainer));
      if (pos)
         pos->ContEmbeddedRemove (&m_gGUID);
   }

   // if it's a container then remove all the objects
   while (TRUE) {
      GUID gObject;
      if (!ContEmbeddedEnum(0, &gObject))
         break;
      ContEmbeddedRemove (&gObject);
   }

   // free up the object surfaces
   for (i = 0; i < m_listPCObjectSurface.Num(); i++) {
      PCObjectSurface pos = *((PCObjectSurface*) m_listPCObjectSurface.Get(i));
      delete pos;
   }
}

/***************************************************************************************
CObjectTemplate::CloneTemplate - The super-class of the cObjectTemplate should call
::CloneTemplate() to clone/copy all the template-information (like rotation info) to
the newly created object.

inputs
   PCObjectTemplate     pCloneTo - This will have all the member variables of "this"
                        copies to pCloneTo
returns
   none
*/
void CObjectTemplate::CloneTemplate (CObjectTemplate *pCloneTo)
{
   pCloneTo->m_fObjShow = m_fObjShow;
   pCloneTo->m_dwMoveReferenceCur = m_dwMoveReferenceCur;
   pCloneTo->m_gGUID = m_gGUID;
   pCloneTo->m_MatrixObject.Copy (&m_MatrixObject);
   pCloneTo->m_pWorld = m_pWorld;

   pCloneTo->m_fCanBeEmbedded = m_fCanBeEmbedded;
   pCloneTo->m_fEmbedded = m_fEmbedded;
   pCloneTo->m_gContainer = m_gContainer;
   pCloneTo->m_fContainerDepth = m_fContainerDepth;
   pCloneTo->m_dwContSideInfoThis = m_dwContSideInfoThis;
   pCloneTo->m_dwContSideInfoOther = m_dwContSideInfoOther;
   pCloneTo->m_fEmbedFitToFloor = m_fEmbedFitToFloor;

   pCloneTo->m_dwType = m_dwType;
   pCloneTo->m_dwRenderShow = m_dwRenderShow;
   pCloneTo->m_OSINFO = m_OSINFO;

   if (m_memName.p) {
      if (pCloneTo->m_memName.Required ((wcslen((PWSTR)m_memName.p)+1)*2))
         wcscpy ((PWSTR) pCloneTo->m_memName.p, (PWSTR) m_memName.p);
   }
   if (m_memGroup.p) {
      if (pCloneTo->m_memGroup.Required ((wcslen((PWSTR)m_memGroup.p)+1)*2))
         wcscpy ((PWSTR) pCloneTo->m_memGroup.p, (PWSTR) m_memGroup.p);
   }
   if (m_memAttribRank.p) {
      if (pCloneTo->m_memAttribRank.Required ((wcslen((PWSTR)m_memAttribRank.p)+1)*2))
         wcscpy ((PWSTR) pCloneTo->m_memAttribRank.p, (PWSTR) m_memAttribRank.p);
   }

   // clone the objects
   DWORD i;
   for (i = 0; i < pCloneTo->m_listPCObjectSurface.Num(); i++) {
      PCObjectSurface pos = *((PCObjectSurface*) pCloneTo->m_listPCObjectSurface.Get(i));
      delete pos;
   }
   pCloneTo->m_listPCObjectSurface.Clear();
   pCloneTo->m_listPCObjectSurface.Required (m_listPCObjectSurface.Num());
   for (i = 0; i < m_listPCObjectSurface.Num(); i++) {
      PCObjectSurface pos = *((PCObjectSurface*) m_listPCObjectSurface.Get(i));
      PCObjectSurface pNew = pos->Clone();
      if (pNew)
         pCloneTo->m_listPCObjectSurface.Add (&pNew);
   }
   pCloneTo->m_fObjectSurfacesSorted = m_fObjectSurfacesSorted;

   // clonet the container surfaces
   pCloneTo->m_listContSurface.Init (sizeof(DWORD), m_listContSurface.Get(0), m_listContSurface.Num());
   pCloneTo->m_listEMBEDINFO.Init (sizeof(EMBEDINFO), m_listEMBEDINFO.Get(0), m_listEMBEDINFO.Num());
   pCloneTo->m_lTATTACH.Init (sizeof(TATTACH), m_lTATTACH.Get(0), m_lTATTACH.Num());
}

/**************************************************************************************
CObjectTemplate::WorldSet - Should be called right away when it's added to the world
(by the CWorldSocket object) to tell the object what world it's in. That way the object can
notify the world of any changes to itself.

inputs
   CWorldSocket      *pWorld - world
*/
void CObjectTemplate::WorldSet (CWorldSocket *pWorld)
{
   _ASSERTE (!pWorld || (pWorld->RenderShardGet() == m_OSINFO.dwRenderShard));

   m_pWorld = pWorld;
}

/**************************************************************************************
CObjectTemplate::WorldSetFinished - So object knows that has been moved and is finished
being moved.

  Called after every WorldSet() call. This is called to tell the objects
  they're in the new world and all their embedded objects are in the new
  world so they may want to do some processing
*/
void CObjectTemplate::WorldSetFinished (void)
{
   return;
}

/**************************************************************************************
CObjectTemplate::MatrixSet - Sets the object's translation, rotation, and scaling matrix.

inputs
   PCMatrix    pObject - matrix
*/
BOOL CObjectTemplate::ObjectMatrixSet (CMatrix *pObject)
{
   if (m_fEmbedded)
      return FALSE;

   // call into world object and say that matrix is about to change
   if (m_pWorld)
      m_pWorld->ObjectAboutToChange (this);

   m_MatrixObject.Copy (pObject);

   // call into world object and say that matrix has changed
   if (m_pWorld)
      m_pWorld->ObjectChanged (this);

   // tell all the contained objects about the move
   DWORD i;
   if (m_pWorld) for (i = 0; i < m_listEMBEDINFO.Num(); i++) {
      PEMBEDINFO pei = (PEMBEDINFO) m_listEMBEDINFO.Get(i);

      PCObjectSocket pos;
      pos = m_pWorld->ObjectGet(m_pWorld->ObjectFind(&pei->gID));
      if (!pos)
         continue;

      CMatrix m;
      m.Identity();
      // Calculate new rotation matrix and set to POS
      ContMatrixFromHV (pei->dwSurface, &pei->tp, pei->fRotation, &m);

      pos->EmbedContainerMoved (pei->dwSurface, &pei->tp, pei->fRotation, &m);
   }

   // tell all the attached objects that have moved
   TellAttachThatMoved ();

   return TRUE;
}

/**************************************************************************************
CObjectTemplate::MatrixGet - Gets the object's translation, rotation, and scaling matrix.

inputs
   PCMatrix    pObject - matrix
*/
void CObjectTemplate::ObjectMatrixGet (CMatrix *pObject)
{
   pObject->Copy (&m_MatrixObject);
}

/**************************************************************************************
CObjectTemplate::QueryBoundingBox - This virtual function ends up making a CRenderSocket
and calls into Render() with no normals and low detail. The resulting points are then
converted min-maxed to create a bounding box.

This function always sets the bounding box matrix to the object's matrix.

inputs
   PCPoint     pCorner1, pCorner2 - Two corners of bounding box
   DWORD       dwSubObject - If not -1 then this queries the bounding box for a sub-object
returns
   none
*/
void CObjectTemplate::QueryBoundingBox (PCPoint pCorner1, PCPoint pCorner2, DWORD dwSubObject)
{
   // set up the variables so can accept callbacks for CRenderSocket
   m_MatrixRS.Identity();
   m_MatrixObject.Invert4 (&m_MatrixObjectInv);
   m_MatrixRSTimeInv.Copy (&m_MatrixObjectInv);
   m_fPolyAlreadyRendered = FALSE;
   m_aPoint[0].Zero();
   m_aPoint[1].Zero();

   // call and get it rendered
   OBJECTRENDER or;
   memset (&or, 0, sizeof(or));
   or.pRS = this;
   or.dwReason = ORREASON_BOUNDINGBOX;
   or.dwShow = -1;
   Render (&or, dwSubObject);

   // and the results
   pCorner1->Copy (&m_aPoint[0]);
   pCorner2->Copy (&m_aPoint[1]);
}


/**************************************************************************************
CObjectTemplate::PreRender - This is a semi-hack function to cause the object to render,
so that object-editor objects know which colors are used.

inputs
   none
returns
   none
*/
void CObjectTemplate::PreRender (void)
{
   // set up the variables so can accept callbacks for CRenderSocket
   m_MatrixRS.Identity();
   m_MatrixObject.Invert4 (&m_MatrixObjectInv);
   m_MatrixRSTimeInv.Copy (&m_MatrixObjectInv);
   m_fPolyAlreadyRendered = FALSE;
   m_aPoint[0].Zero();
   m_aPoint[1].Zero();

   // call and get it rendered
   OBJECTRENDER or;
   memset (&or, 0, sizeof(or));
   or.pRS = this;
   or.dwReason = ORREASON_BOUNDINGBOX;
   or.dwShow = -1;
   Render (&or, (DWORD)-1);
}


/**************************************************************************************
CObjectTemplate::QuerySubObjects - returns the number of sub-objects.

The default is 0, which means that the object can't be divided into sub-objects.

returns
   DWORD - Number of sub-objects.
*/
DWORD CObjectTemplate::QuerySubObjects (void)
{
   return 0;
}

/**************************************************************************************
CObjectTemplate::QueryWantNormals - Answer question by object when asking renderer if
it wants normals. For the bounding box, return false.
*/
BOOL CObjectTemplate::QueryWantNormals (void)
{
   return m_pPolyMesh ? TRUE : FALSE;  // will want normals if deconstructing
}

/**************************************************************************************
CObjectTemplate::QueryWantTextures - Answer question by object when asking renderer if
it wants textures. For the bounding box, return false.
*/
BOOL CObjectTemplate::QueryWantTextures (void)
{
   return m_pPolyMesh ? TRUE : FALSE;  // will want textures if deconstructing
}

/******************************************************************************
CObjectTemplate::QueryCloneRender - From CRenderSocket
*/
BOOL CObjectTemplate::QueryCloneRender (void)
{
   return FALSE;
}


/******************************************************************************
CObjectTemplate::CloneRender - From CRenderSocket
*/
BOOL CObjectTemplate::CloneRender (GUID *pgCode, GUID *pgSub, DWORD dwNum, PCMatrix pamMatrix)
{
   return FALSE;
}

/******************************************************************************
CObjectTemplate::QuerySubDetail - From CRenderSocket. Basically end up ignoring
*/
BOOL CObjectTemplate::QuerySubDetail (PCMatrix pMatrix, PCPoint pBound1, PCPoint pBound2, fp *pfDetail)
{
   *pfDetail = QueryDetail();
   return TRUE;
}

/**************************************************************************************
CObjectTemplate::QueryDetails - Called by the object to ask how much detail we want.
For the case of bounding box, return 1.0 for 1 meter.
*/
fp CObjectTemplate::QueryDetail (void)
{
   return 0;   // want full detail for bounding box
}

/**************************************************************************************
CObjectTemplate::MatrixSet - Sets the m_MatrixRS to the given value. It also sets
flags if it's called after a polygon has already been rendered.

inputs
   PCMatrix    pm - matrix to set
*/
void CObjectTemplate::MatrixSet (PCMatrix pm)
{
   m_MatrixRS.Copy (pm);
   m_MatrixRSTimeInv.Multiply (&m_MatrixObjectInv, pm);
}

/**************************************************************************************
CObjectTemplate::PolyRender - Called to render one or more polygons. Because we're looking
for a bounding box, just use the points (forget about the rest) and find the min/max
values.

inputs
   PPOLYRENDERINFO      pInfo - information about rendering
*/
void CObjectTemplate::PolyRender (PPOLYRENDERINFO pInfo)
{
   if (!pInfo->paPoints)
      return;

   if (m_pPolyMesh) {
      // called while deconstructing so call into polygon mesh
      // figure out the matrix that goes from the polygon to the mesh
      CMatrix mObjectToMesh;
      mObjectToMesh.Multiply (&m_MatrixObjectInv, &m_MatrixRS);
      m_pPolyMesh->m_PolyMesh.PolyRenderToMesh (pInfo, &mObjectToMesh, m_pPolyMesh);
      return;
   }

   // loop through all the points finding min/max
   CPoint   apMinMax[2];
   DWORD i, j;
   PCPoint p;
   CPoint pTrans;
   for (i = 0; i < pInfo->dwNumPoints; i++) {
      p = pInfo->paPoints + i;
      pTrans.Copy (p);
      pTrans.p[3] = 1;
      pTrans.MultiplyLeft (&m_MatrixRSTimeInv);

      if (i) {
         for (j = 0; j < 4; j++) {
            apMinMax[0].p[j] = min(apMinMax[0].p[j], pTrans.p[j]);
            apMinMax[1].p[j] = max(apMinMax[1].p[j], pTrans.p[j]);
         }
      }
      else {
         // first time
         apMinMax[0].Copy(&pTrans);
         apMinMax[1].Copy(&pTrans);

      }
   }

   if (!pInfo->dwNumPoints)
      return;  // no points to test

   // if no polygons have been rendered yet then just use these values
   if (!m_fPolyAlreadyRendered) {
      m_aPoint[0].Copy(&apMinMax[0]);
      m_aPoint[1].Copy(&apMinMax[1]);

      // note that we've had polyrender called
      m_fPolyAlreadyRendered = TRUE;
      return;
   }

   // if not then incoporate min max into out current min/max
   for (j = 0; j < 4; j++) {
      m_aPoint[0].p[j] = min(apMinMax[0].p[j], m_aPoint[0].p[j]);
      m_aPoint[1].p[j] = max(apMinMax[1].p[j], m_aPoint[1].p[j]);
   }

}

/**************************************************************************************
CObjectTemplate::MoveRefenrenceCurQuery - returns the index for the current move
reference. Initially, the object should have the most likely movement reference selected.

inputs
   none
returns
   DWORD - index
*/
DWORD CObjectTemplate::MoveReferenceCurQuery (void)
{
   return m_dwMoveReferenceCur;
}

/**************************************************************************************
CObjectTemplate::MoveReferenceCurSet - 
sets the current move reference to a new index. Returns FALSE
f the index is invalid

inputs
   DWORD       dwIndex - index to set to
returns
   BOOL - TRUE if set. FALSE if not valid index
*/
BOOL CObjectTemplate::MoveReferenceCurSet (DWORD dwIndex)
{
   // check to make sure it's ok
   WCHAR szTemp[256];
   if (!MoveReferenceStringQuery (dwIndex, szTemp, sizeof(szTemp), NULL))
      return FALSE;

   m_dwMoveReferenceCur = dwIndex;
   return TRUE;
}


/**************************************************************************************
CObjectTemplate::MoveReferencePointQuery - 
given a move reference index, this fill in pp with the position of
the move reference RELATIVE to ObjectMatrixGet. References are numbers
from 0+. If the index is more than the number of points then the
function returns FALSE

inputs
   DWORD       dwIndex - index.0 .. # ref
   PCPoint     pp - Filled with point relative to ObjectMatrixGet() IF its valid
returns
   BOOL - TRUE if valid index.
*/
BOOL CObjectTemplate::MoveReferencePointQuery (DWORD dwIndex, PCPoint pp)
{
   if (dwIndex >= sizeof(gaDefMoveP) / sizeof(DEFMOVEP))
      return FALSE;

   // get the bounding box
   CPoint p1, p2;
   QueryBoundingBox (&p1, &p2, (DWORD)-1);
   DWORD i;
   pp->p[3] = 1;
   for (i = 0; i < 3; i++) {
      switch ((i == 0) ? gaDefMoveP[dwIndex].iX : ((i == 1) ? gaDefMoveP[dwIndex].iY : gaDefMoveP[dwIndex].iZ) ) {
      case -1:
         pp->p[i] = min(p1.p[i], p2.p[i]);
         break;
      case 1:
         pp->p[i] = max(p1.p[i], p2.p[i]);
         break;
      case 0:
      default:
         pp->p[i] = (p1.p[i] + p2.p[i]) / 2;
         break;
      }
   }

   return TRUE;
}

/**************************************************************************************
CObjectTemplate::MoveReferenceStringQuery -
given a move reference index (numbered from 0 to the number of references)
this fills in a string at psz and dwSize BYTES that names the move reference
to the end user. *pdwNeeded is filled with the number of bytes needed for
the string. Returns FALSE if dwIndex is too high, or dwSize is too small (although
pdwNeeded will be filled in)

inputs
   DWORD       dwIndex - index. 0.. # ref
   PWSTR       psz - To be filled in witht he string
   DWORD       dwSize - # of bytes available in psz
   DWORD       *pdwNeeded - If not NULL, filled with the size needed
returns
   BOOL - TRUE if psz copied.
*/
BOOL CObjectTemplate::MoveReferenceStringQuery (DWORD dwIndex, PWSTR psz, DWORD dwSize, DWORD *pdwNeeded)
{
   if (dwIndex >= sizeof(gaDefMoveP) / sizeof(DEFMOVEP)) {
      if (pdwNeeded)
         *pdwNeeded = 0;
      return FALSE;
   }

   size_t dwNeeded;
   dwNeeded = (wcslen (gaDefMoveP[dwIndex].pszName) + 1) * 2;
   if (pdwNeeded)
      *pdwNeeded = (DWORD) dwNeeded;
   if (dwNeeded <= dwSize) {
      wcscpy (psz, gaDefMoveP[dwIndex].pszName);
      return TRUE;
   }
   else
      return FALSE;
}

/***************************************************************************************
CObjectTemplate::GUIDSet - Sets the object's guid.

inputs
   GUID        *pg - New GUID
returns
   none
*/
void CObjectTemplate::GUIDSet (GUID *pg)
{
   m_gGUID = *pg;
}

/***************************************************************************************
CObjectTemplate::GUIDGet - Gets the object's guid.

inputs
   GUID        *pg - New GUID
returns
   none
*/
void CObjectTemplate::GUIDGet (GUID *pg)
{
   *pg = m_gGUID;
}

static PWSTR gpszType = L"Type";
static PWSTR gpszObjName = L"ObjName";
static PWSTR gpszObjGroup = L"ObjGroup";
static PWSTR gpszObjShow = L"ObjShow";
static PWSTR gpszAttribRank = L"ObjAttribRank";

/***************************************************************************************
CObjectTemplate::MMLToTemplate - The superclass should call this in its version
of MMLTo(). This will write the template's member variables out to the MML. In fact,
it first creates the MML (so the MMLTo() function won't need to) and writes stuff out.

inputs
   none
returns
   PCMMLNode2 - Node that the MMLTo() caller should fill with class-specific infomration.
               NULL if error
*/
PCMMLNode2 CObjectTemplate::MMLToTemplate (void)
{
   PCMMLNode2 pNode;
   pNode = new CMMLNode2;
   if (!pNode)
      return NULL;
   pNode->NameSet (L"Object");

   if (!m_fObjShow)
      MMLValueSet (pNode, gpszObjShow, (int) m_fObjShow);

   MMLValueSet (pNode, gpszType, (int)m_dwType);
   MMLValueSet (pNode, L"RenderShow", (int)m_dwRenderShow);
   // NOTE: Not setting m_OSINFO

   // write out the member variables
   MMLValueSet (pNode, L"TempObjectGUID", (PBYTE)&m_gGUID, sizeof(m_gGUID));
   MMLValueSet (pNode, L"TempMoveReferenceCur", (int) m_dwMoveReferenceCur);
   MMLValueSet (pNode, L"TempMatrixObject", &m_MatrixObject);
   // dont set the world pointer because is useless

   if (m_memName.p)
      MMLValueSet (pNode, gpszObjName, (PWSTR) m_memName.p);
   if (m_memGroup.p)
      MMLValueSet (pNode, gpszObjGroup, (PWSTR) m_memGroup.p);
   if (m_memAttribRank.p && ((PWSTR)m_memAttribRank.p)[0])
      MMLValueSet (pNode, gpszAttribRank, (PWSTR) m_memAttribRank.p);

   DWORD i;
   // write out the object surfaces
   for (i = 0; i < m_listPCObjectSurface.Num(); i++) {
      PCObjectSurface pos = *((PCObjectSurface*) m_listPCObjectSurface.Get(i));
      pNode->ContentAdd (pos->MMLTo());
   }

   // write out embeddeing
   // dont right out the flag to see if can embed though because should
   // always be set by constuctore
   MMLValueSet (pNode, L"Embedded", (int) this->m_fEmbedded);
   MMLValueSet (pNode, L"Container", (PBYTE) &m_gContainer, sizeof(m_gContainer));
   MMLValueSet (pNode, L"ContainerDepth", m_fContainerDepth);
   MMLValueSet (pNode, L"CSIThis", (int) m_dwContSideInfoThis);
   MMLValueSet (pNode, L"CSIOther", (int) m_dwContSideInfoOther);

   // write out list of valid container surfaces
   WCHAR szTemp[64];
   for (i = 0; i < m_listContSurface.Num(); i++) {
      DWORD dwVal = *((DWORD*) m_listContSurface.Get(i));
      swprintf (szTemp, L"ContSurface%d", (int)i);
      MMLValueSet (pNode, szTemp, (int) dwVal);
   }

   // write out embed info
   for (i = 0; i < m_listEMBEDINFO.Num(); i++) {
      PEMBEDINFO pInfo = (PEMBEDINFO) m_listEMBEDINFO.Get(i);
      PCMMLNode2 pSub;
      pSub = pNode->ContentAddNewNode ();
      pSub->NameSet (L"EmbedInfo");

      MMLValueSet (pSub, L"GUIDID", (PBYTE) &pInfo->gID, sizeof(pInfo->gID));
      MMLValueSet (pSub, L"HV", &pInfo->tp);
      MMLValueSet (pSub, L"Rotation", pInfo->fRotation);
      MMLValueSet (pSub, L"Surface", (int) pInfo->dwSurface);
   }

   // write out attached info
   PTATTACH pt;
   pt = (PTATTACH) m_lTATTACH.Get(0);
   for (i = 0; i < m_lTATTACH.Num(); i++, pt++) {
      PCMMLNode2 pSub;
      pSub = pNode->ContentAddNewNode ();
      pSub->NameSet (L"AttachInfo");

      MMLValueSet (pSub, L"GUIDID", (PBYTE) &pt->gID, sizeof(pt->gID));
      if (pt->szAttach[0])
         MMLValueSet (pSub, L"Attach", pt->szAttach);
      MMLValueSet (pSub, L"Attrib", pt->szAttrib);
      MMLValueSet (pSub, L"AttachToBone", &pt->mAttachToBone);
      MMLValueSet (pSub, L"Use", (int) pt->fUse);
   }

   return pNode;
}

/***************************************************************************************
CObjectTemplate::MMLFromTemplate - The superclass should call this in its version
of MMLFrom(). It will get the template's member variables (such as rotation matrix)
from the MML and store them away.

inputs
   PCMMLNode2 pNode
returns
   BOOL - TRUE if succeded
*/
BOOL CObjectTemplate::MMLFromTemplate (PCMMLNode2 pNode)
{
   m_fObjShow = (BOOL) MMLValueGetInt (pNode, gpszObjShow, TRUE);
   MMLValueGetBinary (pNode, L"TempObjectGUID", (PBYTE) &m_gGUID, sizeof(m_gGUID));
   m_dwMoveReferenceCur = (DWORD) MMLValueGetInt (pNode, L"TempMoveReferenceCur", (int) 0);
   MMLValueGetMatrix (pNode, L"TempMatrixObject", &m_MatrixObject, NULL);

   m_dwType = (DWORD) MMLValueGetInt (pNode, gpszType, 0);
   m_dwRenderShow = (DWORD) MMLValueGetInt (pNode, L"RenderShow", RENDERSHOW_MISC);
   // NOTE: Not getting m_OSINFO

   PWSTR psz;
   psz = MMLValueGet (pNode, gpszObjName);
   if (psz) {
      if (m_memName.Required ((wcslen(psz)+1)*2))
         wcscpy ((PWSTR) m_memName.p, psz);
   }
   psz = MMLValueGet (pNode, gpszObjGroup);
   if (psz) {
      if (m_memGroup.Required ((wcslen(psz)+1)*2))
         wcscpy ((PWSTR) m_memGroup.p, psz);
   }
   psz = MMLValueGet (pNode, gpszAttribRank);
   if (psz) {
      if (m_memAttribRank.Required ((wcslen(psz)+1)*2))
         wcscpy ((PWSTR) m_memAttribRank.p, psz);
   }

   // read in the surfaces
   DWORD i;
   // free up the object surfaces
   for (i = 0; i < m_listPCObjectSurface.Num(); i++) {
      PCObjectSurface pos = *((PCObjectSurface*) m_listPCObjectSurface.Get(i));
      delete pos;
   }
   m_listPCObjectSurface.Clear();
   m_listEMBEDINFO.Clear();
   m_lTATTACH.Clear();
   for (i = 0; ; i++) {
      PCMMLNode2 pSub;
      PWSTR psz;
      if (!pNode->ContentEnum(i, &psz, &pSub))
         break;
      if (!pSub)
         continue;
      psz = pSub->NameGet();
      if (!psz)
         continue;
      if (!_wcsicmp (psz, gszObjectSurface)) {
         // else, add it
         PCObjectSurface pos;
         pos = new CObjectSurface;
         if (!pos)
            break;
         pos->MMLFrom (pSub);
         m_listPCObjectSurface.Add (&pos);
      }
      else if (!_wcsicmp(psz, L"EmbedInfo")) {
         EMBEDINFO ei;
         memset (&ei, 0, sizeof(ei));
         MMLValueGetBinary (pSub, L"GUIDID", (PBYTE) &ei.gID, sizeof(ei.gID));
         MMLValueGetTEXTUREPOINT (pSub, L"HV", &ei.tp, NULL);
         ei.fRotation = MMLValueGetDouble (pSub, L"Rotation", 0);
         ei.dwSurface = (DWORD) MMLValueGetInt (pSub, L"Surface", 0);

         m_listEMBEDINFO.Add (&ei);
      }
      else if (!_wcsicmp(psz, L"AttachInfo")) {
         TATTACH ta;
         memset (&ta, 0, sizeof(ta));
         MMLValueGetBinary (pSub, L"GUIDID", (PBYTE) &ta.gID, sizeof(ta.gID));
         PWSTR psz;
         psz = MMLValueGet (pSub, L"Attach");
         if (psz)
            wcscpy (ta.szAttach, psz);
         psz = MMLValueGet (pSub, L"Attrib");
         if (!psz)
            continue;
         wcscpy (ta.szAttrib, psz);
         MMLValueGetMatrix (pSub, L"AttachToBone", &ta.mAttachToBone);
         ta.fUse = (BOOL) MMLValueGetInt (pSub, L"Use", 1);

         m_lTATTACH.Add (&ta);
      }
   }
   m_fObjectSurfacesSorted = FALSE;

   // read in embedding
   // dont read in the flag to see if can embed though because should
   // always be set by constuctore
   m_fEmbedded = (BOOL) MMLValueGetInt (pNode, L"Embedded", FALSE);
   MMLValueGetBinary (pNode, L"Container", (PBYTE) &m_gContainer, sizeof(m_gContainer));
   m_fContainerDepth = MMLValueGetDouble (pNode, L"ContainerDepth", 0);
   m_dwContSideInfoThis = (DWORD) MMLValueGetInt (pNode, L"CSIThis", (int) 0);
   m_dwContSideInfoOther = (DWORD) MMLValueGetInt (pNode, L"CSIOther", (int) 0);

   // read in the list of valid container surfaces
   WCHAR szTemp[64];
   m_listContSurface.Clear();
   for (i = 0; ; i++) {
      DWORD dwVal;
      swprintf (szTemp, L"ContSurface%d", (int)i);
      dwVal = (DWORD) MMLValueGetInt (pNode, szTemp, -1);
      if (dwVal == -1)
         break;
      m_listContSurface.Add (&dwVal);
   }

   return TRUE;
}


/*************************************************************************************
CObjectTemlate::IntelligentPosition - The template object is unable to chose an intelligent
position so it just retunrs FALSE.

inputs
   POSINTELLIPOS     pInfo - Information that might be useful to chose the position
returns
   BOOL - TRUE if the object has moved, rotated, scaled, itself to an intelligent
      location. FALSE if doesn't know and its up to the application.
*/
BOOL CObjectTemplate::IntelligentPosition (PCWorldSocket pWorld, POSINTELLIPOS pInfo, POSINTELLIPOSRESULT pResult)
{
   return FALSE;
}

/*************************************************************************************
CObjectTemlate::IntelligentPositionDrag - The template object is unable to chose an intelligent
position so it just retunrs FALSE.

inputs
   POSINTELLIPOS     pInfo - Information that might be useful to chose the position
returns
   BOOL - TRUE if the object has moved, rotated, scaled, itself to an intelligent
      location. FALSE if doesn't know and its up to the application.
*/
BOOL CObjectTemplate::IntelligentPositionDrag (CWorldSocket *pWorld, POSINTELLIPOSDRAG pInfo,  POSINTELLIPOSRESULT pResult)
{
   return FALSE;
}


/*************************************************************************************
CObjectTemplate::
Tells the object to intelligently adjust itself based on nearby objects.
For walls, this means triming to the roof line, for floors, different
textures, etc. If fAct is FALSE the function is just a query, that returns
TRUE if the object cares about adjustment and can try, FALSE if it can't.

NOTE: Often overridden
*/
BOOL CObjectTemplate::IntelligentAdjust (BOOL fAct)
{
   return FALSE;
}

/*************************************************************************************
CObjectTemplate::Deconstruct -
Tells the object to deconstruct itself into sub-objects.
Basically, new objects will be added that exactly mimic this object,
and any embedeeding objects will be moved to the new ones.
NOTE: This old one will not be deleted - the called of Deconstruct()
will need to call Delete()
If fAct is FALSE the function is just a query, that returns
TRUE if the object cares about adjustment and can try, FALSE if it can't.

NOTE: Often overridden.
*/
BOOL CObjectTemplate::Deconstruct (BOOL fAct)
{
   // since will convert to polymesh, allow to deconstruct
   if (!fAct)
      return TRUE;

   // create the new polygon mesh
   OSINFO info;
   memset (&info, 0, sizeof(info));
   info.gCode = CLSID_PolyMesh;
   info.gSub = CLSID_PolyMesh_Cube;
   info.dwRenderShard = m_OSINFO.dwRenderShard;
   PCObjectPolyMesh pm = new CObjectPolyMesh ((PVOID)7, &info); // default to creating cube
   if (!pm)
      return FALSE;

   // wipe out old stuff
   pm->m_PolyMesh.Clear();
   while (pm->ObjectSurfaceNumIndex())
      pm->ObjectSurfaceRemove (pm->ObjectSurfaceGetIndex(0));
   pm->m_dwSubdivideFinal = pm->m_dwSubdivideWork = 0;
   pm->m_dwDivisions = 0;
   pm->ObjectMatrixSet (&m_MatrixObject);

   // render it all...
   m_pPolyMesh = pm;
   OBJECTRENDER or;
   memset (&or, 0, sizeof(or));
   or.pRS = this;
   or.dwReason = ORREASON_FINAL;
   or.dwShow = -1;
   m_MatrixObject.Invert4 (&m_MatrixObjectInv);
   Render (&or, (DWORD) -1);
   m_pPolyMesh = NULL;


   // if no sides then fail
   if (!pm->m_PolyMesh.SideNum()) {
      pm->Delete();
      return FALSE;
   }

   // add to world
   m_pWorld->ObjectAdd (pm);

   return TRUE;
}


/*************************************************************************************
CObjectTemlate::ControlPointQuery - Called to query the information about a control
point identified by dwID. Because this is the template, we do the default action
of returning FALSE all the time, assuming that by default there are no control points.

inputs
   DWORD       dwID - ID
   POSCONTROL  pInfo - Filled in with the infomration
returns
   BOOL - TRUE if successful
*/
BOOL CObjectTemplate::ControlPointQuery (DWORD dwID, POSCONTROL pInfo)
{
   return FALSE;
}

/*************************************************************************************
CObjectTemlate::ControlPointSet - Called to change the valud of a control point.
Because the template assumes there are no control points always return FALSE.

inputs
   DWORD       dwID - ID
   PCPoint     pVal - Contains the new location, in object coordinates
returns
   BOOL - TRUE if successful
*/
BOOL CObjectTemplate::ControlPointSet (DWORD dwID, PCPoint pVal, PCPoint pViewer)
{
   return FALSE;
}

/*************************************************************************************
CObjectTemlate::ControlPointEnum - Called to enumerate a list of control point IDs
into the list. Since the template assumes no control points, does nothing.

inputs
   PCListFixed       plDWORD - Should be filled in with control points
returns
   none
*/
void CObjectTemplate::ControlPointEnum (PCListFixed plDWORD)
{
   // do nothing
   return;
}

/*************************************************************************************
CObjectTemlate::ObjectSurfaceAdd - This should be called in the super-class's constructor
to tell the object template what surfaces exist on the object. Call it once for each
surface. It can also be called later on to add new surfaces.

inputs
   DWORD       dwID - Surface ID. THis must be unique (or the previous version removed
                        by calling ObjectSurfaceRemove())
   COLORREF    cColor - Color to use if no texture is available.
   PWSTR       pszScheme - Scheme to use. If NULL then no scheme will be used.
   GUID        pgTextureCode - Texture code to use. If this is not NULL then its
                        assumed the surface will use a texture in preference to a color
   GUID        pgTextureSub - Sub-type of the texture in gpTextureCode.
   PTEXTUREMODS pTextureMods - Modifications to the basic texture. Can be NULL
   fp      *padTextureMatrix - 2x2 array of doubles for texture rotation matrix. Can be null
   WORD        wTransparency - 0 for opaque, 0xffff for transparent
returns
   none
*/
void CObjectTemplate::ObjectSurfaceAdd (DWORD dwID, COLORREF cColor,
                                        DWORD dwMaterialID, PWSTR pszScheme,
      const GUID *pgTextureCode, const GUID *pgTextureSub,
      PTEXTUREMODS pTextureMods, fp *pafTextureMatrix)
{
   // the objects will be unsorted after this
   m_fObjectSurfacesSorted = FALSE;

   PCObjectSurface pos;
   pos = new CObjectSurface;
   if (!pos)
      return;

   pos->m_dwID = dwID;
   pos->m_cColor = cColor;
   pos->m_Material.InitFromID (dwMaterialID);
   //pos->m_wTransparency = wTransparency;
   if (pszScheme)
      wcscpy (pos->m_szScheme, pszScheme);
   if (pgTextureCode) {
      pos->m_gTextureCode = *pgTextureCode;
      pos->m_fUseTextureMap = TRUE;
   }
   if (pgTextureSub) {
      pos->m_gTextureSub = *pgTextureSub;
   }
   if (pTextureMods)
      pos->m_TextureMods = *pTextureMods;
   if (pafTextureMatrix)
      memcpy (pos->m_afTextureMatrix, pafTextureMatrix, sizeof(pos->m_afTextureMatrix));
   else if (pgTextureCode && pgTextureSub) {
      RENDERSURFACE rs;
      memset (&rs, 0, sizeof(rs));
      rs.fUseTextureMap = TRUE;
      rs.gTextureCode = pos->m_gTextureCode;
      rs.gTextureSub = pos->m_gTextureSub;
      rs.TextureMods = pos->m_TextureMods;
      memcpy (&rs.afTextureMatrix, &pos->m_afTextureMatrix, sizeof(pos->m_afTextureMatrix));

      PCTextureMapSocket pm;
      pm = TextureCacheGet (m_OSINFO.dwRenderShard, &rs, NULL, NULL);
      if (pm) {
         fp fx, fy;
         pm->DefScaleGet (&fx, &fy);
         pos->m_afTextureMatrix[0][0] = 1.0 / fx;
         pos->m_afTextureMatrix[1][1] = 1.0 / fy;
         pm->MaterialGet (0, &pos->m_Material); // BUGFIX - Get material characteristics from texture
         TextureCacheRelease (m_OSINFO.dwRenderShard, pm);
      }
   }


   m_listPCObjectSurface.Add (&pos);
   return;
}

/*************************************************************************************
CObjectTemlate::ObjectSurfaceAdd - Called to add an object surface.

inputs
   PCObjectSurface      posAdd - This is cloned and then added. NOTE that the iDs stay the same.
returns
   none
*/
void CObjectTemplate::ObjectSurfaceAdd (PCObjectSurface posAdd)
{
   // the objects will be unsorted after this
   m_fObjectSurfacesSorted = FALSE;

   PCObjectSurface pos;
   pos = posAdd->Clone ();
   if (!pos)
      return;


   m_listPCObjectSurface.Add (&pos);
   return;
}
/*************************************************************************************
CObjectTemlate::ObjectSurfaceRemove - Given a surface ID, this removes it from the
templates list. The super-class can call this to clear away any surfaces it no longer
needs (due to reshaping, or whatever). It's not necessary to remove a surface (generally)
since they will be cleared out when the object is deleted.

inputs
   DWORD       dwID - ID
returns
   BOOL - TRUE if removed
*/
BOOL CObjectTemplate::ObjectSurfaceRemove (DWORD dwID)
{
   DWORD dwIndex = ObjectSurfaceFindIndex (dwID);
   if (dwIndex == (DWORD)-1)
      return FALSE;

   PCObjectSurface pos;
   pos = *((PCObjectSurface*) m_listPCObjectSurface.Get(dwIndex));
   delete pos;
   m_listPCObjectSurface.Remove (dwIndex);

   return TRUE;
}

/**********************************************************************************
CObjectTemplate::ObjectSurfaceGetIndex - Given a an index into the list
of container surfaces, returns the surface ID

inputs
   DWORD       dwID - Index into list
returns
   DWORD - Constainer surface ID, or -1 if can't find
*/
DWORD CObjectTemplate::ObjectSurfaceGetIndex (DWORD dwIndex)
{
   PCObjectSurface *ppos, pos;
   ppos = (PCObjectSurface*) m_listPCObjectSurface.Get(dwIndex);
   if (!ppos)
      return (DWORD)-1;
   pos = *ppos;
   return pos->m_dwID;
}


/**********************************************************************************
CObjectTemplate::ObjectSurfaceNumIndex - Returns th number of surface objects
in the object.

returns
   DWORD - Number
*/
DWORD CObjectTemplate::ObjectSurfaceNumIndex (void)
{
   return m_listPCObjectSurface.Num();
}

/*************************************************************************************
CObjectTemlate::ObjectSurfaceFindIndex - Internal function that sorts the object surfaces
if they're not alreay sorted, and then does a bsearch.

inputs
   DWORD       dwID - ID
returns
   DWORD - Index in m_listpCObjectSurface, or -1 if cant find
*/
static int _cdecl BCompare (const void *elem1, const void *elem2)
{
   PCObjectSurface *pdw1, *pdw2;
   pdw1 = (PCObjectSurface*) elem1;
   pdw2 = (PCObjectSurface*) elem2;

   return (int) (*pdw1)->m_dwID - (int)(*pdw2)->m_dwID;
}

DWORD CObjectTemplate::ObjectSurfaceFindIndex (DWORD dwID)
{
   if (!m_fObjectSurfacesSorted) {
      m_fObjectSurfacesSorted = TRUE;
      qsort (m_listPCObjectSurface.Get(0), m_listPCObjectSurface.Num(),
         sizeof(PCObjectSurface), BCompare);
   }

   PCObjectSurface *ppos;
   CObjectSurface os;
   os.m_dwID = dwID;
   PCObjectSurface pos;
   pos = &os;
   ppos = (PCObjectSurface*) bsearch (&pos, m_listPCObjectSurface.Get(0), m_listPCObjectSurface.Num(),
         sizeof(PCObjectSurface), BCompare);
   if (!ppos)
      return (DWORD)-1;

   return (DWORD)(size_t) ((PBYTE) ppos - (PBYTE) (m_listPCObjectSurface.Get(0))) / sizeof(PCObjectSurface);
}

/*************************************************************************************
CObjectTemplate::ObjectSurfaceFind - Function called by the super-class to get the
PCObjectSurface object for a given ID. This can then be passed to CRenderSurface::SetDefMaterial(),
causing the upcoming bits of the object to be drawn using that color.

inputs
   DWORD       dwID - ID
returns
   PCObjectSurface - Object surface object, or NULL if error.
*/
PCObjectSurface CObjectTemplate::ObjectSurfaceFind (DWORD dwID)
{
   DWORD dwIndex = ObjectSurfaceFindIndex (dwID);
   if (dwIndex == (DWORD)-1)
      return NULL;

   PCObjectSurface pos;
   pos = *((PCObjectSurface*) m_listPCObjectSurface.Get(dwIndex));

   // BUGFIX - If it's a scheme, make sure the scheme is loaded in already.
   // if not, load it in
   PCSurfaceSchemeSocket pss;
   if (pos->m_szScheme[0] && m_pWorld && (pss = m_pWorld->SurfaceSchemeGet ())) {
      if (!pss->SurfaceExists (pos->m_szScheme)) {
         PCObjectSurface pNew;

         // BUGFIX - Surface about to change
         pNew = pss->SurfaceGet (pos->m_szScheme, pos, TRUE);
         // since noclone if (pNew)
         // since noclone   delete pNew;
      }
   }
   return pos;
}

/*************************************************************************************
CObjectTemplate::SurfaceGet -  returns a pointer to the PCObjectSurface color/texture object describing
the surface on the object. dwID is from 0 to 1023. Returns NULL if there's
no such surface. The object MUST BE FREED by the caller

inputs
   DWORD       dwID - ID from 0 to 1023
returns
   PCObjectSurface - New surface. Must call delete on this.
*/
PCObjectSurface CObjectTemplate::SurfaceGet (DWORD dwID)
{
   PCObjectSurface pos = ObjectSurfaceFind (dwID);
   if (!pos)
      return NULL;

   return pos->Clone();
}

/*************************************************************************************
CObjectTemplate::SurfaceSet - changes an object's surface to something new. Returns FALSE if there's
no such surface ID.

inputs
   PCObjectSurface         pos - object surface
returns
   BOOL - TRUE if succeded
*/
BOOL CObjectTemplate::SurfaceSet (PCObjectSurface pos)
{
   PCObjectSurface pFind = ObjectSurfaceFind (pos->m_dwID);
   if (!pFind)
      return FALSE;

   // call into world object and say that matrix is about to change
   if (m_pWorld)
      m_pWorld->ObjectAboutToChange (this);

   // copy it over
   memcpy (pFind, pos, sizeof(*pFind));

   // call into world object and say that matrix has changed
   if (m_pWorld)
      m_pWorld->ObjectChanged (this);

   // done
   return TRUE;
}


/**********************************************************************************
CObjectTemplate::DialogQuery - Returns TRUE if the object supports a custom
dialog box, FALSE if it doesn't. The default object doesn't
*/
BOOL CObjectTemplate::DialogQuery (void)
{
   return FALSE;
}

/**********************************************************************************
CObjectTemplate::DialogShow - Causes the object to show a dialog box that allows
different attributes to be changed.

inputs
   DWORD             dwSuface - Surface that was clicked on. 0 if unknown.
   PCEscWindow       pWindow - Escarpment window to draw it in
returns
   BOOL - TRUE if user pressed "Back", FALSE if user pressed "Close"
*/
BOOL CObjectTemplate::DialogShow (DWORD dwSurface, PCEscWindow pWindow, PCPoint pClick)
{
   return FALSE;
}


/**********************************************************************************
CObjectTemplate::DialogCPQuery - Returns TRUE if the object supports a CONTROL POINT SELECTION
dialog box, FALSE if it doesn't. The default object doesn't
*/
BOOL CObjectTemplate::DialogCPQuery (void)
{
   return FALSE;
}

/**********************************************************************************
CObjectTemplate::DialogCPShow - Causes the object to show a dialog box that allows
CONTROL POINTS show to be selected.

inputs
   DWORD             dwSuface - Surface that was clicked on. 0 if unknown.
   PCEscWindow       pWindow - Escarpment window to draw it in
returns
   BOOL - TRUE if user pressed "Back", FALSE if user pressed "Close"
*/
BOOL CObjectTemplate::DialogCPShow (DWORD dwSurface, PCEscWindow pWindow, PCPoint pClick)
{
   return FALSE;
}


/**********************************************************************************
CObjectTemplate::ContainerSurfaceAdd - Called during the constructor of the
object to specify that a surface number (the same numbering system as used for
ObjectSurfaceAdd() can be a container. If it's called then (almost) all the container
functions are automagically handled.

inputs
   DWORD       dwID - Surface ID. Same number as used for ObjectSurfaceAdd().
*/
void CObjectTemplate::ContainerSurfaceAdd (DWORD dwID)
{
   // if already exists don't add
   if (ContainerSurfaceIndex(dwID) != (DWORD)-1)
      return;

   // object about to change
   if (m_pWorld)
      m_pWorld->ObjectAboutToChange (this);

   m_listContSurface.Add (&dwID);

   if (m_pWorld)
      m_pWorld->ObjectChanged (this);

}

/**********************************************************************************
CObjectTemplate::ContainerSurfaceRemove - Called to remove a container surface.

NOTE: This does NOT remove any embedded objects that may have been using that
surface.

inputs
   DWORD       dwID - Surface ID. Same number as used for ObjectSurfaceAdd().
*/
void CObjectTemplate::ContainerSurfaceRemove (DWORD dwID)
{
   // see if exists
   DWORD dwIndex = ContainerSurfaceIndex(dwID);
   if (dwIndex == (DWORD)-1)
      return;

   // object about to change
   if (m_pWorld)
      m_pWorld->ObjectAboutToChange (this);

   m_listContSurface.Remove (dwIndex);

   if (m_pWorld)
      m_pWorld->ObjectChanged (this);

}

/**********************************************************************************
CObjectTemplate::ContainerSurfaceIndex - Given a surface ID, searches through
the list of container surfaces and finds the index into the container list.

inputs
   DWORD       dwID - Surface ID. Same number as used for ObjectSurfaceAdd().
returns
   DWORD - Index into container surface list, or -1 if can't find
*/
DWORD CObjectTemplate::ContainerSurfaceIndex (DWORD dwID)
{
   DWORD i, dwFind;
   for (i = 0; i < m_listContSurface.Num(); i++) {
      dwFind = *((DWORD*) m_listContSurface.Get(i));
      if (dwFind == dwID)
         return i;
   }

   return (DWORD)-1;
}


/**********************************************************************************
CObjectTemplate::ContainerSurfaceGetIndex - Given a an index into the list
of container surfaces, returns the surface ID

inputs
   DWORD       dwID - Index into list
returns
   DWORD - Constainer surface ID, or -1 if can't find
*/
DWORD CObjectTemplate::ContainerSurfaceGetIndex (DWORD dwIndex)
{
   DWORD *pdw;
   pdw = (DWORD*) m_listContSurface.Get(dwIndex);
   if (!pdw)
      return (DWORD)-1;
   return *pdw;
}


/**********************************************************************************
CObjectTemplate::
returns TRUE if the object is capable of being embedded in another
object. FALSE if its only standalone. If it's cabaple, it must
support the other EmbedXXX functions

NOTE: For a super-class to support embedding into containers, it should
set m_fCanBeEmbedded = TRUE in its constuctor, and then most things will magically happen.
*/
BOOL CObjectTemplate::EmbedQueryCanEmbed (void)
{
   return m_fCanBeEmbedded;
}

/**********************************************************************************
CObjectTemplate::
Fills pgCont with the object ID of the container object holding
this object. If this object cannot be embedded, or isn't currently
embedded, this returns FALSE.
*/
BOOL CObjectTemplate::EmbedContainerGet (GUID *pgCont)
{
   if (!m_fCanBeEmbedded || !m_fEmbedded)
      return FALSE;

   *pgCont = m_gContainer;
   return TRUE;
}


/**********************************************************************************
CObjectTemplate::
Tells this object to use pgCont as the new container object.
Generally after this call the object will query the container about
it's location, etc., so the container should already be prepared
for the calls. NOTE: Generally this function is only called by
the container object, and not by the user.
*/
BOOL CObjectTemplate::EmbedContainerSet (const GUID *pgCont)
{
   if (!m_fCanBeEmbedded)
      return FALSE;

   if (m_pWorld && !m_fDeleting)
      m_pWorld->ObjectAboutToChange (this);

   if (pgCont) {
      m_gContainer = *pgCont;
      m_fEmbedded = TRUE;
   }
   else {
      memset (&m_gContainer, 0, sizeof(m_gContainer));
      m_fEmbedded = FALSE;
   }

   if (m_pWorld && !m_fDeleting)
      m_pWorld->ObjectChanged (this);

   return TRUE;
}


/**********************************************************************************
CObjectTemplate::
Called by the container object into each of its embedded object
when the container is moved. Passes in a new translation/rotation
matrix from object space into world space (assuming HV of 0,0 is
the same as object space 0,0,0). The embedded object just needs to
remember this matrix, call pWorld->Objectabouttochange, and changed,
and use it for future reference. NOTE: The embedded object is still
in the same position relative to the container.
*/
BOOL CObjectTemplate::EmbedContainerMoved (DWORD dwSurface, const PTEXTUREPOINT pHV, fp fRotation, PCMatrix pm)
{
   if (!m_fCanBeEmbedded || !m_fEmbedded)
      return FALSE;

   // call into world object and say that matrix is about to change
   if (m_pWorld)
      m_pWorld->ObjectAboutToChange (this);

   GUID gCont;
   PCObjectSocket pos;
   // just take what's given
   m_MatrixObject.Copy (pm);

   // while at it, get the thickness of the surface
   // get the thickness from the object
   fp fThickness;
   fThickness = 0;
   if (m_pWorld && EmbedContainerGet(&gCont)) {
      pos = m_pWorld->ObjectGet(m_pWorld->ObjectFind(&gCont));

      if (pos) {
         TEXTUREPOINT HV;
         HV = *pHV;

         // if embedded object wants to fit to the nearest floor then try it
         if (m_fEmbedFitToFloor) {
            OSMFINDCLOSESTFLOOR cf;
            memset (&cf, 0, sizeof(cf));
            cf.dwSurface = dwSurface;
            cf.pOrig = *pHV;
            if (pos->Message (OSM_FINDCLOSESTFLOOR, &cf)) {
               HV = cf.pClosest;

               // get the matrix for this
               pos->ContMatrixFromHV (dwSurface, &HV, fRotation, &m_MatrixObject);
            }
         }

         fThickness = pos->ContThickness (dwSurface, &HV);
         m_dwContSideInfoThis = pos->ContSideInfo (dwSurface, FALSE);
         m_dwContSideInfoOther = pos->ContSideInfo (dwSurface, TRUE);
      }
   }
   m_fContainerDepth = fThickness;

   // call into world object and say that matrix has changed
   if (m_pWorld)
      m_pWorld->ObjectChanged (this);

   return TRUE;
}


/**********************************************************************************
CObjectTemplate::
Kind of like EmbedContainerMoved, except this time the object has
been moved within the container and may need to readjust its shape/
size, etc., or adjust the cutout it has within the container.
*/
BOOL CObjectTemplate::EmbedMovedWithinContainer (DWORD dwSurface, const PTEXTUREPOINT pHV, fp fRotation, PCMatrix pm)
{
   // Gene
   if (!m_fCanBeEmbedded || !m_fEmbedded)
      return FALSE;

   if (!EmbedContainerMoved (dwSurface, pHV, fRotation, pm))
      return FALSE;

   // Call into super-class replacing function for object to redo cutout arch
   EmbedDoCutout ();

   return TRUE;
}


/**********************************************************************************
CObjectTemplate::
Called to tell the embedded object that it's container has a new ID
GUID, and that that should be used for future reference.
*/
BOOL CObjectTemplate::EmbedContainerRenamed (const GUID *pgCont)
{
   if (!m_fCanBeEmbedded || !m_fEmbedded)
      return FALSE;

   // NOTE: Not doing object changed since this is only supposed to
   // be called during the paste process.
   m_gContainer = *pgCont;

   return TRUE;
}


/**********************************************************************************
CObjectTemplate::
Called to tell the container that one of its contained objects has been renamed.
*/
BOOL CObjectTemplate::ContEmbeddedRenamed (const GUID *pgOld, const GUID *pgNew)
{
   // NOTE: Not doing object changed since this is only supposed to be
   // called during the paste process
   DWORD i;
   PEMBEDINFO pei;
   for (i = 0; i < m_listEMBEDINFO.Num(); i++) {
      pei = (PEMBEDINFO) m_listEMBEDINFO.Get(i);
      if (IsEqualGUID(pei->gID, *pgOld)) {
         pei->gID = *pgNew;
         return TRUE;
      }
   }

   return FALSE;
}


/**********************************************************************************
CObjectTemplate::
Call this to ask an object if it can contain other objects. If
dwSurface == -1, this returns TRUE if (in general) it can, or FALSE
if it can't. If dwSurface != -1, then this is asking for the specific
surface (gotten from LOWORD(IMAGEPIXEL)). If it can contain embedded
objects then this object must support the ContXXX functions
*/
BOOL CObjectTemplate::ContQueryCanContain (DWORD dwSurface)
{
   if (dwSurface == -1) {
      return m_listContSurface.Num() ? TRUE : FALSE;
   }

   // else find
   DWORD dwIndex;
   dwIndex = ContainerSurfaceIndex (dwSurface);
   return (dwIndex == -1) ? FALSE : TRUE;
}


/**********************************************************************************
CObjectTemplate::
Returns the number of objects currently embedded in this container.
*/
DWORD CObjectTemplate::ContEmbeddedNum (void)
{
   if (!m_listContSurface.Num())
      return FALSE;

   return m_listEMBEDINFO.Num();
}


/**********************************************************************************
CObjectTemplate::
Given an index from 0 .. ContEmbeddedNum()-1, this filled in pgEmbed with
the embedded's guid
*/
BOOL CObjectTemplate::ContEmbeddedEnum (DWORD dwIndex, GUID *pgEmbed)
{
   if (!m_listContSurface.Num())
      return FALSE;

   PEMBEDINFO pei;
   pei = (PEMBEDINFO) m_listEMBEDINFO.Get(dwIndex);
   if (!pei)
      return FALSE;

   *pgEmbed = pei->gID;

   return TRUE;
}


/**********************************************************************************
CObjectTemplate::
Given an object GUID (that's embedded), fills in pHV with the location in HV coordinations (0..1,0..1)
in the embedded object's surface. pfRotation is filled with clockwise rotation
in radios. and pdwSurface with the surface ID, LOWORD(IMAGEPIXEL)
*/
BOOL CObjectTemplate::ContEmbeddedLocationGet (const GUID *pgEmbed, PTEXTUREPOINT pHV, fp *pfRotation, DWORD *pdwSurface)
{
   if (!m_listContSurface.Num())
      return FALSE;

   DWORD i;
   PEMBEDINFO pei;
   for (i = 0; i < m_listEMBEDINFO.Num(); i++) {
      pei = (PEMBEDINFO) m_listEMBEDINFO.Get(i);
      if (IsEqualGUID(pei->gID, *pgEmbed))
         break;
   }
   if (i >= m_listEMBEDINFO.Num())
      return FALSE;

   *pHV = pei->tp;
   *pfRotation = pei->fRotation;
   *pdwSurface = pei->dwSurface;

   return TRUE;
}


/**********************************************************************************
CObjectTemplate::
Given an object GUID (that's already embedded), changes it's location with pHV,
rotation with fRotation, or surface with dwSurface.
*/
BOOL CObjectTemplate::ContEmbeddedLocationSet (const GUID *pgEmbed, PTEXTUREPOINT pHV, fp fRotation, DWORD dwSurface)
{
   if (!m_listContSurface.Num())
      return FALSE;

   DWORD i;
   PEMBEDINFO pei;
   for (i = 0; i < m_listEMBEDINFO.Num(); i++) {
      pei = (PEMBEDINFO) m_listEMBEDINFO.Get(i);
      if (IsEqualGUID(pei->gID, *pgEmbed))
         break;
   }
   if (i >= m_listEMBEDINFO.Num())
      return FALSE;

   // make sure hv are valid
   if ((pHV->h < 0) || (pHV->h > 1) || (pHV->v < 0) || (pHV->v > 1))
      return FALSE;
   // make sure object surface is valid
   if (ContainerSurfaceIndex(dwSurface) == (DWORD)-1)
      return FALSE;

   // make sure object can be embedded and isn't already owned
   PCObjectSocket pos;
   pos = NULL;
   if (!m_pWorld)
      return FALSE;
   pos = m_pWorld->ObjectGet (m_pWorld->ObjectFind ((GUID*) pgEmbed));
   if (!pos)
      return FALSE;

   // call into world object and say that matrix is about to change
   if (m_pWorld)
      m_pWorld->ObjectAboutToChange (this);

   pei->tp = *pHV;
   pei->fRotation = fRotation;
   pei->dwSurface = dwSurface;

   // call into world object and say that matrix has changed
   if (m_pWorld)
      m_pWorld->ObjectChanged (this);

   CMatrix m;
   m.Identity();

   // calculate where new object is
   ContMatrixFromHV (pei->dwSurface, &pei->tp, pei->fRotation, &m);

   // tell object that has been moved, EmbedMoveWithinContainer
   pos->EmbedMovedWithinContainer (pei->dwSurface, &pei->tp, pei->fRotation, &m);

   return TRUE;
}


/**********************************************************************************
CObjectTemplate::
Adds an object (which is not embedded) into the list. This function must: Make sure object not
already embedded, that it can be embedded, that dwSurface and pHV area valid, then call
EmbedContainerSet() and EmbedMovedWithinContainer() into the object.
*/
BOOL CObjectTemplate::ContEmbeddedAdd (const GUID *pgEmbed, PTEXTUREPOINT pHV, fp fRotation, DWORD dwSurface)
{
   if (!m_listContSurface.Num())
      return FALSE;

   DWORD i;
   PEMBEDINFO pei;
   for (i = 0; i < m_listEMBEDINFO.Num(); i++) {
      pei = (PEMBEDINFO) m_listEMBEDINFO.Get(i);
      if (IsEqualGUID(pei->gID, *pgEmbed))
         break;
   }
   if (i < m_listEMBEDINFO.Num())
      return FALSE;

   // make sure hv are valid
   if ((pHV->h < 0) || (pHV->h > 1) || (pHV->v < 0) || (pHV->v > 1))
      return FALSE;
   // make sure object surface is valid
   if (ContainerSurfaceIndex(dwSurface) == (DWORD)-1)
      return FALSE;

   // make sure object can be embedded and isn't already owned
   PCObjectSocket pos;
   pos = NULL;
   if (!m_pWorld)
      return FALSE;
   pos = m_pWorld->ObjectGet (m_pWorld->ObjectFind ((GUID*) pgEmbed));
   if (!pos)
      return FALSE;
   if (!pos->EmbedQueryCanEmbed())
      return FALSE;
   GUID gEmbedIn;
   if (pos->EmbedContainerGet (&gEmbedIn))
      return FALSE;

   // call into world object and say that matrix is about to change
   if (m_pWorld)
      m_pWorld->ObjectAboutToChange (this);

   EMBEDINFO ei;
   memset (&ei, 0, sizeof(ei));
   ei.gID = *pgEmbed;
   ei.tp = *pHV;
   ei.fRotation = fRotation;
   ei.dwSurface = dwSurface;
   m_listEMBEDINFO.Add (&ei);

   // call into world object and say that matrix has changed
   if (m_pWorld)
      m_pWorld->ObjectChanged (this);

   CMatrix m;
   m.Identity();

   // calculate the location
   ContMatrixFromHV (ei.dwSurface, &ei.tp, ei.fRotation, &m);

   // Set object's owner
   pos->EmbedContainerSet (&m_gGUID);

   // tell object that has been moved, EmbedMoveWithinContainer
   pos->EmbedMovedWithinContainer (ei.dwSurface, &ei.tp, ei.fRotation, &m);

   return TRUE;
}


/**********************************************************************************
CObjectTemplate::
Removed an object (which is embedded) from the list. This function does not
delete the object, but insteads throws it off into world space. It must call
EmbedContainerSet(), and clean up any internal data structures.
*/
BOOL CObjectTemplate::ContEmbeddedRemove (const GUID *pgEmbed)
{
   if (!m_listContSurface.Num())
      return FALSE;

   DWORD i;
   PEMBEDINFO pei;
   for (i = 0; i < m_listEMBEDINFO.Num(); i++) {
      pei = (PEMBEDINFO) m_listEMBEDINFO.Get(i);
      if (IsEqualGUID(pei->gID, *pgEmbed))
         break;
   }
   if (i >= m_listEMBEDINFO.Num())
      return FALSE;

   // find the object in the world
   PCObjectSocket pos;
   pos = NULL;
   if (m_pWorld) {
      DWORD dwIndex = m_pWorld->ObjectFind ((GUID*) pgEmbed);
      pos = m_pWorld->ObjectGet (dwIndex);
   }

   // tell the object it's removed
   if (pos)
      pos->EmbedContainerSet (NULL);

   if (m_pWorld && !m_fDeleting)
      m_pWorld->ObjectAboutToChange (this);

   // get rid of any cutouts this may have
   ContCutout (pgEmbed, 0, NULL, NULL, TRUE);

   // remove this from the list
   m_listEMBEDINFO.Remove (i);

   if (m_pWorld && !m_fDeleting)
      m_pWorld->ObjectChanged (this);

   return TRUE;
}


/**********************************************************************************
CObjectTemplate::ContHVQuery
Called to see the HV location of what clicked on, or in other words, given a line
from pEye to pClick (object sapce), where does it intersect surface dwSurface (LOWORD(PIMAGEPIXEL.dwID))?
Fills pHV with the point. Returns FALSE if doesn't intersect, or invalid surface, or
can't tell with that surface. NOTE: if pOld is not NULL, then assume a drag from
pOld (converted to world object space) to pClick. Finds best intersection of the
surface then. Useful for dragging embedded objects around surface

NOTE: Should be overridden if can embed.
*/
BOOL CObjectTemplate::ContHVQuery (PCPoint pEye, PCPoint pClick, DWORD dwSurface, PTEXTUREPOINT pOld, PTEXTUREPOINT pHV)
{
   if (!m_listContSurface.Num())
      return FALSE;

   return FALSE;
}


/**********************************************************************************
CObjectTemplate::ContCutout
Called by an embeded object to specify an arch cutout within the surface so the
object can go through the surface (like a window). The container should check
that pgEmbed is a valid object. dwNum is the number of points in the arch,
paFront and paBack are the container-object coordinates. (See CSplineSurface::ArchToCutout)
If dwNum is 0 then arch is simply removed. If fBothSides is true then both sides
of the surface are cleared away, else only the side where the object is embedded
is affected.

NOTE: Should be overridden if can embed
*/
BOOL CObjectTemplate::ContCutout (const GUID *pgEmbed, DWORD dwNum, PCPoint paFront, PCPoint paBack, BOOL fBothSides)
{
   return FALSE;
}

/**********************************************************************************
CObjectTemplate::ContCutoutToZipper -
Assumeing that ContCutout() has already been called for the pgEmbed, this askes the surfaces
for the zippering information (CRenderSufrace::ShapeZipper()) that would perfectly seal up
the cutout. plistXXXHVXYZ must be initialized to sizeof(HVXYZ). plisstFrontHVXYZ is filled with
the points on the side where the object was embedded, and plistBackHVXYZ are the opposite side.
NOTE: These are in the container's object space, NOT the embedded object's object space.
*/
BOOL CObjectTemplate::ContCutoutToZipper (const GUID *pgEmbed, PCListFixed plistFrontHVXYZ, PCListFixed plistBackHVXYZ)
{
   return FALSE;
}

/**********************************************************************************
CObjectTemplate::ContMatrixFromHV - THE SUPERCLASS SHOULD OVERRIDE THIS. Given
a point on a surface (that supports embedding), this returns a matrix that translates
0,0,0 to the same in world space as where pHV is, and also applies fRotation around Y (clockwise)
so that X and Z are (for the most part) still on the surface.

inputs
   DWORD             dwSurface - Surface ID that can be embedded. Should check to make sure is valid
   PTEXTUREPOINT     pHV - HV, 0..1 x 0..1 locaiton within the surface
   fp            fRotation - Rotation in radians, clockwise.
   PCMatrix          pm - Filled with the new rotation matrix
returns
   BOOL - TRUE if success
*/
BOOL CObjectTemplate::ContMatrixFromHV (DWORD dwSurface, PTEXTUREPOINT pHV, fp fRotation, PCMatrix pm)
{
   return FALSE;
}

/**********************************************************************************
CObjectTemplate::ContThickness - 
returns the thickness of the surface (dwSurface) at pHV. Used by embedded
objects like windows to know how deep they should be.

NOTE: usually overridden
*/
fp CObjectTemplate::ContThickness (DWORD dwSurface, PTEXTUREPOINT pHV)
{
   return 0;
}

/**********************************************************************************
CObjectTemplate::ContSideInfo -
returns a DWORD indicating what class of surface it is... right now
0 = internal, 1 = external. dwSurface is the surface. If fOtherSide
then want to know what's on the opposite side. Returns 0 if bad dwSurface.

NOTE: usually overridden
*/
DWORD CObjectTemplate::ContSideInfo (DWORD dwSurface, BOOL fOtherSide)
{
   return 0;
}

/**********************************************************************************
CObjectTemplate::EmbedDoCutout - Member function specific to the template. Called
when the object has moved within the surface. This enables the super-class for
the embedded object to pass a cutout into the container. (Basically, specify the
hole for the window or door)

NOTE: This should be replaced by the superclass
*/
BOOL CObjectTemplate::EmbedDoCutout (void)
{
   return FALSE;
}


/**********************************************************************************
CObjectTemplate::Message -
sends a message to the object. The interpretation of the message depends upon
dwMessage, which is OSM_XXX. If the function understands and handles the
message it returns TRUE, otherwise FALE.

NOTE: Often overridden
*/
BOOL CObjectTemplate::Message (DWORD dwMessage, PVOID pParam)
{
   return FALSE;
}


/**********************************************************************************
CObjectTemplate::Called by the renderer to see if the object should even thought
about being drawn. This returns m_dwRenderShow, which is a collection of
RENDERSHOW_XXXX flags. During ::Render(), the object may want to draw parts
of itself depending on the flags.

returns
   DWORD - Set of flags
*/
DWORD CObjectTemplate::CategoryQuery (void)
{
   return m_dwRenderShow;
}

/**********************************************************************************
CObjectTemplate::OpenGet - 
returns how open the object is, from 0 (closed) to 1.0 (open), or
< 0 for an object that can't be opened
*/
fp CObjectTemplate::OpenGet (void)
{
   return -1;
}

/**********************************************************************************
CObjectTemplate::OpenSet - 
opens/closes the object. if fopen==0 it's close, 1.0 = open, and
values in between are partially opened closed. Returne TRUE if success
*/
BOOL CObjectTemplate::OpenSet (fp fOpen)
{
   return FALSE;
}

/********************************************************************************
CObjectTemplate::LightQuery -
ask the object if it has any lights. If it does, pl is added to (it
is already initialized to sizeof LIGHTINFO) with one or more LIGHTINFO
structures. Coordinates are in OBJECT SPACE.
*/
BOOL CObjectTemplate::LightQuery (PCListFixed pl, DWORD dwShow)
{
   return FALSE;
}

/**********************************************************************************
CObjectTemplate::TurnOnGet - 
returns how open the object is, from 0 (closed) to 1.0 (open), or
< 0 for an object that can't be opened
*/
fp CObjectTemplate::TurnOnGet (void)
{
   return -1;
}

/**********************************************************************************
CObjectTemplate::TurnOnSet - 
opens/closes the object. if fopen==0 it's close, 1.0 = open, and
values in between are partially opened closed. Returne TRUE if success
*/
BOOL CObjectTemplate::TurnOnSet (fp fOpen)
{
   return FALSE;
}

/**********************************************************************************
CObjectTemplate::ObjectClassQuery
asks the curent object what other objects (including itself) it requires
so that when a file is saved, all user objects will be saved along with
the file, so people on other machines can load them in.
The object just ADDS (doesn't clear or remove) elements, which are two
guids in a row: gCode followed by gSub of the object. All objects
must add at least on (their own). Some, like CObjectEditor, will add
all its sub-objects too
*/
BOOL CObjectTemplate::ObjectClassQuery (PCListFixed plObj)
{
   GUID ag[2];
   OSINFO OI;
   InfoGet (&OI);
   ag[0] = OI.gCode;
   ag[1] = OI.gSub;

   plObj->Add (ag);

   return TRUE;
}



/**********************************************************************************
CObjectTemplate::TextureQuery -
asks the object what textures it uses. This allows the save-function
to save custom textures into the file. The object just ADDS (doesn't
clear or remove) elements, which are two guids in a row: the
gCode followed by the gSub of the object. Of course, it may add more
than one texture
*/
BOOL CObjectTemplate::TextureQuery (PCListFixed plText)
{
   DWORD i;
   GUID ag[2];
   for (i = 0; i < m_listPCObjectSurface.Num(); i++) {
      PCObjectSurface *ppos, pos;
      ppos = (PCObjectSurface*) m_listPCObjectSurface.Get(i);
      if (!ppos)
         continue;
      pos = *ppos;

      if (pos->m_szScheme[0] && m_pWorld) {
         PCObjectSurface p2;
         p2 = (m_pWorld->SurfaceSchemeGet())->SurfaceGet (pos->m_szScheme, pos, TRUE);
         if (p2) {
            if (p2->m_fUseTextureMap) {
               ag[0] = p2->m_gTextureCode;
               ag[1] = p2->m_gTextureSub;
               plText->Add (ag);
            }
            // since noclone delete p2;
            continue;
         }
      }
      if (!pos->m_fUseTextureMap)
         continue;

      ag[0] = pos->m_gTextureCode;
      ag[1] = pos->m_gTextureSub;
      plText->Add (ag);
   }

   return TRUE;
}



/**********************************************************************************
CObjectTemplate::ColorQuery -
asks the object what colors it uses (exclusive of textures).
It adds elements to plColor, which is a list of COLORREF. It may
add more than one color
*/
BOOL CObjectTemplate::ColorQuery (PCListFixed plColor)
{
   DWORD i;
   for (i = 0; i < m_listPCObjectSurface.Num(); i++) {
      PCObjectSurface *ppos, pos;
      ppos = (PCObjectSurface*) m_listPCObjectSurface.Get(i);
      if (!ppos)
         continue;
      pos = *ppos;

      if (pos->m_szScheme[0] && m_pWorld) {
         PCObjectSurface p2;
         p2 = (m_pWorld->SurfaceSchemeGet())->SurfaceGet (pos->m_szScheme, pos, TRUE);
         if (p2) {
            if (!p2->m_fUseTextureMap) {
               plColor->Add (&p2->m_cColor);
            }
            // since noclone delete p2;
            continue;
         }
      }
      if (pos->m_fUseTextureMap)
         continue;

      plColor->Add (&pos->m_cColor);
   }

   return TRUE;
}

void CObjectTemplate::InfoGet (POSINFO pInfo)
{
   memcpy (pInfo, &m_OSINFO, sizeof(m_OSINFO));
}


/*****************************************************************************************
CObjectTemplate::Merge -
asks the object to merge with the list of objects (identified by GUID) in pagWith.
dwNum is the number of objects in the list. The object should see if it can
merge with any of the ones in the list (some of which may no longer exist and
one of which may be itself). If it does merge with any then it return TRUE.
if no merges take place it returns false.
*/
BOOL CObjectTemplate::Merge (GUID *pagWith, DWORD dwNum)
{
   return FALSE;
}

/*****************************************************************************************
CObjectTemplate::StringSet - 
Sets one of the prefedined object strings, such as OSSTRING_NAME.
*/
BOOL CObjectTemplate::StringSet (DWORD dwString, PWSTR psz)
{
   if (!psz)
      return FALSE;
   
   PCMem pMem;
   switch (dwString) {
      case OSSTRING_NAME:
         pMem = &m_memName;
         break;
      case OSSTRING_GROUP:
         pMem = &m_memGroup;
         break;
      case OSSTRING_ATTRIBRANK:
         pMem = &m_memAttribRank;
         break;
      default:
         return FALSE;
   }

   if (m_pWorld)
      m_pWorld->ObjectAboutToChange (this);

   if (pMem->Required ((wcslen(psz)+1)*2))
      wcscpy ((PWSTR) pMem->p, psz);

   if (m_pWorld)
      m_pWorld->ObjectChanged (this);

   return TRUE;
}


/*****************************************************************************************
CObjectTemplate::StrinGet - 
Gets the string. DONT change the memory pointed to by the string
*/
PWSTR CObjectTemplate::StringGet (DWORD dwString)
{
   PCMem pMem;
   switch (dwString) {
      case OSSTRING_NAME:
         pMem = &m_memName;
         break;
      case OSSTRING_GROUP:
         pMem = &m_memGroup;
         break;
      case OSSTRING_ATTRIBRANK:
         pMem = &m_memAttribRank;
         break;
      default:
         return FALSE;
   }

   if (pMem->p)
      return (PWSTR) pMem->p;
   else
      return NULL;
}

/*****************************************************************************************
CObjectTemplate::ShowSet - 
If true then show the object. If FALSE then hide the object
*/
void CObjectTemplate::ShowSet (BOOL fShow)
{
   if (fShow == m_fObjShow)
      return;

   if (m_pWorld)
      m_pWorld->ObjectAboutToChange (this);

   m_fObjShow = fShow;

   if (m_pWorld)
      m_pWorld->ObjectChanged (this);
}

/*****************************************************************************************
CObjectTemplate::ShowGet - 
returns TRUE if the object is visible (not hidden), false if hidden
*/
BOOL CObjectTemplate::ShowGet (void)
{
   return m_fObjShow;
}


PWSTR gpszAttribOnOff = L"Turned on";
PWSTR gpszAttribOpenClose = L"Opened";
static PWSTR gpszAttribShowHide = L"Visible";
static PWSTR gpszAttribPosnX = L"Position (X)";
static PWSTR gpszAttribPosnY = L"Position (Y)";
static PWSTR gpszAttribPosnZ = L"Position (Z)";
static PWSTR gpszAttribRotX = L"Rotation (X)";
static PWSTR gpszAttribRotY = L"Rotation (Y)";
static PWSTR gpszAttribRotZ = L"Rotation (Z)";
static PWSTR gpszAttribEmbedPosnH = L"Embedded position (H)";
static PWSTR gpszAttribEmbedPosnV = L"Embedded position (V)";
static PWSTR gpszAttribEmbedRot = L"Embedded rotation";
static DWORD gdwAttribOnOff = (DWORD)wcslen(gpszAttribOnOff);
static DWORD gdwAttribOpenClose = (DWORD)wcslen(gpszAttribOpenClose);
static DWORD gdwAttribShowHide = (DWORD)wcslen(gpszAttribShowHide);
static DWORD gdwAttribPosnX = (DWORD)wcslen(gpszAttribPosnX);
static DWORD gdwAttribPosnY = (DWORD)wcslen(gpszAttribPosnY);
static DWORD gdwAttribPosnZ = (DWORD)wcslen(gpszAttribPosnZ);
static DWORD gdwAttribRotX = (DWORD)wcslen(gpszAttribRotX);
static DWORD gdwAttribRotY = (DWORD)wcslen(gpszAttribRotY);
static DWORD gdwAttribRotZ = (DWORD)wcslen(gpszAttribRotZ);
static DWORD gdwAttribEmbedPosnH = (DWORD)wcslen(gpszAttribEmbedPosnH);
static DWORD gdwAttribEmbedPosnV = (DWORD)wcslen(gpszAttribEmbedPosnV);
static DWORD gdwAttribEmbedRot = (DWORD)wcslen(gpszAttribEmbedRot);

static PWSTR gpszAttribOnOffDesc = L"Turns the object (such as a light) on or off.";
static PWSTR gpszAttribOpenCloseDesc = L"Opens or closes the object.";
static PWSTR gpszAttribShowHideDesc = L"Shows or hides the object.";
static PWSTR gpszAttribPosnDesc = L"Position of the object in the world";
static PWSTR gpszAttribRotDesc = L"Orientation of the object.";
static PWSTR gpszAttribEmbedPosnDesc = L"Position of the object within the container.";
static PWSTR gpszAttribEmbedRotDesc = L"Rotation of the object.";
static PWSTR gpszAttribAttachDesc = L"If turned on, the attached object will move in tandem with the object. If off, the connection is ignored (until turned on.)";


/*****************************************************************************************
Return strings
*/
PWSTR AttribOpenClose (void)
{
   return gpszAttribOpenClose;
}
PWSTR AttribOnOff (void)
{
   return gpszAttribOnOff;
}

/*****************************************************************************************
IsTemplateAttributeLoc - Returns true if the attribute is supported by the template
object AND deals with location.

inputs
   PWSTR       pszName - String to test
   DWORD       dwLen - Length of pszName
returns
   BOOL - TRUE if is attribute
*/
BOOL IsTemplateAttributeLoc (PWSTR pszName, DWORD dwLen)
{
   if ((dwLen == gdwAttribEmbedPosnH) && !_wcsicmp(pszName, gpszAttribEmbedPosnH))
      return TRUE;

   if ((dwLen == gdwAttribEmbedPosnV) && !_wcsicmp(pszName, gpszAttribEmbedPosnV))
      return TRUE;

   if ((dwLen == gdwAttribPosnX) && !_wcsicmp(pszName, gpszAttribPosnX))
      return TRUE;
   if ((dwLen == gdwAttribPosnY) && !_wcsicmp(pszName, gpszAttribPosnY))
      return TRUE;
   if ((dwLen == gdwAttribPosnZ) && !_wcsicmp(pszName, gpszAttribPosnZ))
      return TRUE;

   return FALSE;
}


/*****************************************************************************************
IsTemplateAttributeRot - Returns true if the attribute is supported by the template
object AND deals with rotation.

inputs
   PWSTR       pszName - String to test
   DWORD       dwLen - Length of pszName
returns
   BOOL - TRUE if is attribute
*/
BOOL IsTemplateAttributeRot (PWSTR pszName, DWORD dwLen)
{
   if ((dwLen == gdwAttribEmbedRot) && !_wcsicmp(pszName, gpszAttribEmbedRot))
      return TRUE;


   if ((dwLen == gdwAttribRotX) && !_wcsicmp(pszName, gpszAttribRotX))
      return TRUE;
   if ((dwLen == gdwAttribRotY) && !_wcsicmp(pszName, gpszAttribRotY))
      return TRUE;
   if ((dwLen == gdwAttribRotZ) && !_wcsicmp(pszName, gpszAttribRotZ))
      return TRUE;

   return FALSE;
}

/*****************************************************************************************
IsTemplateAttribute - Returns TRUE if the attribute is supported by the object template
and hence shouldn't be passed up. NOTE: This excludes Open/Close and On/off.

inputs
   PWSTR       pszName - String to test
   BOOL        fIncludeOpenOn - If TRUE, also test for Open/close and on/off.
returns
   BOOL - TRUE if supported
*/
BOOL IsTemplateAttribute (PWSTR pszName, BOOL fIncludeOpenOn)
{
   DWORD dwLen = (DWORD)wcslen(pszName);

   if (fIncludeOpenOn) {
      if ((dwLen == gdwAttribOnOff) && !_wcsicmp(pszName, gpszAttribOnOff))
         return TRUE;

      if ((dwLen == gdwAttribOpenClose) && !_wcsicmp(pszName, gpszAttribOpenClose))
         return TRUE;
   }
   if ((dwLen == gdwAttribShowHide) && !_wcsicmp(pszName, gpszAttribShowHide))
      return TRUE;


   if (IsTemplateAttributeLoc(pszName, dwLen))
      return TRUE;
   if (IsTemplateAttributeRot(pszName, dwLen))
      return TRUE;

   return FALSE;
}

/*****************************************************************************************
CObjectTemplate::AttribGet -
given an attribute name, this fills in pfValue with the value of the attribute.
Returns FALSE if there's an error and cant fill in attribute
do not override - use AttribGetIntern
*/
BOOL CObjectTemplate::AttribGet (PWSTR pszName, fp *pfValue)
{
   DWORD dwLen = (DWORD)wcslen(pszName);

   if ((dwLen == gdwAttribOnOff) && !_wcsicmp(pszName, gpszAttribOnOff) && (TurnOnGet() >= 0)) {
      *pfValue = TurnOnGet ();
      return TRUE;
   }

   if ((dwLen == gdwAttribOpenClose) && !_wcsicmp(pszName, gpszAttribOpenClose) && (OpenGet() >= 0)) {
      *pfValue = OpenGet ();
      return TRUE;
   }

   if ((dwLen == gdwAttribShowHide) && !_wcsicmp(pszName, gpszAttribShowHide)) {
      *pfValue = ShowGet() ? 1 : 0;
      return TRUE;
   }

   if (m_fEmbedded) {
      DWORD dwWant = 0;
      if ((dwLen == gdwAttribEmbedPosnH) && !_wcsicmp(pszName, gpszAttribEmbedPosnH))
         dwWant = 1;
      else if ((dwLen == gdwAttribEmbedPosnV) && !_wcsicmp(pszName, gpszAttribEmbedPosnV))
         dwWant = 2;
      else if ((dwLen == gdwAttribEmbedRot) && !_wcsicmp(pszName, gpszAttribEmbedRot))
         dwWant = 3;
      if (!dwWant)
         goto next;

      PCObjectSocket pos;
      TEXTUREPOINT tp;
      fp fRotation;
      DWORD dwSurface;
      pos = NULL;
      if (m_pWorld)
         pos = m_pWorld->ObjectGet (m_pWorld->ObjectFind (&m_gContainer));
      if (!pos)
         return FALSE;
      if (!pos->ContEmbeddedLocationGet (&m_gGUID, &tp, &fRotation, &dwSurface))
         return FALSE;
      if (dwWant == 1)
         *pfValue = tp.h;
      else if (dwWant == 2)
         *pfValue = tp.v;
      else
         *pfValue = fRotation;
      return TRUE;
   }
   else {
      DWORD dwWant = -1;
      if ((dwLen == gdwAttribPosnX) && !_wcsicmp(pszName, gpszAttribPosnX))
         dwWant = 0;
      else if ((dwLen == gdwAttribPosnY) && !_wcsicmp(pszName, gpszAttribPosnY))
         dwWant = 1;
      else if ((dwLen == gdwAttribPosnZ) && !_wcsicmp(pszName, gpszAttribPosnZ))
         dwWant = 2;
      else if ((dwLen == gdwAttribRotX) && !_wcsicmp(pszName, gpszAttribRotX))
         dwWant = 3;
      else if ((dwLen == gdwAttribRotY) && !_wcsicmp(pszName, gpszAttribRotY))
         dwWant = 4;
      else if ((dwLen == gdwAttribRotZ) && !_wcsicmp(pszName, gpszAttribRotZ))
         dwWant = 5;
      if (dwWant == -1)
         goto next;  // not this

      // get the matrix
      CPoint pLoc, pRot;
      m_MatrixObject.ToXYZLLT (&pLoc, &pRot.p[2], &pRot.p[0], &pRot.p[1]);
      if (dwWant < 3)
         *pfValue = pLoc.p[dwWant];
      else
         *pfValue = pRot.p[dwWant-3];
      return TRUE;
   }
next:

   // see about the attached objects
   PTATTACH pt = (PTATTACH) m_lTATTACH.Get(0);
   DWORD i;
   for (i = 0; i < m_lTATTACH.Num(); i++, pt++)
      if (!_wcsicmp (pszName, pt->szAttrib)) {
         *pfValue = pt->fUse ? 1.0 : 0.0;
         return TRUE;
      }

   return AttribGetIntern (pszName, pfValue);
}

/*****************************************************************************************
CObjectTemplate::AttirbGetAll
clears the list and fills it in with ATTRIBVAL strucutres describing
all the attributes.
do not override - use AttribGetAllIntern
*/
void CObjectTemplate::AttribGetAll (PCListFixed plATTRIBVAL)
{
   plATTRIBVAL->Init (sizeof(ATTRIBVAL));
   ATTRIBVAL av;
   memset (&av, 0, sizeof(av));


   // add the values
   if (TurnOnGet() >= 0) {
      wcscpy (av.szName, gpszAttribOnOff);
      av.fValue = TurnOnGet();
      plATTRIBVAL->Add (&av);
   }
   if (OpenGet() >= 0) {
      wcscpy (av.szName, gpszAttribOpenClose);
      av.fValue = OpenGet();
      plATTRIBVAL->Add (&av);
   }

   // visible
   wcscpy (av.szName, gpszAttribShowHide);
   av.fValue = this->ShowGet() ? 1 : 0;
   plATTRIBVAL->Add (&av);

   if (m_fEmbedded) {
      PCObjectSocket pos;
      TEXTUREPOINT tp;
      fp fRotation;
      DWORD dwSurface;
      pos = NULL;
      if (m_pWorld)
         pos = m_pWorld->ObjectGet (m_pWorld->ObjectFind (&m_gContainer));
      if (!pos)
         goto next;
      if (!pos->ContEmbeddedLocationGet (&m_gGUID, &tp, &fRotation, &dwSurface))
         goto next;

      plATTRIBVAL->Required (plATTRIBVAL->Num() + 3);

      // h
      wcscpy (av.szName, gpszAttribEmbedPosnH);
      av.fValue = tp.h;
      plATTRIBVAL->Add (&av);

      // v
      wcscpy (av.szName, gpszAttribEmbedPosnV);
      av.fValue = tp.v;
      plATTRIBVAL->Add (&av);

      // rotation
      wcscpy (av.szName, gpszAttribEmbedRot);
      av.fValue = fRotation;
      plATTRIBVAL->Add (&av);
   }
   else {
      // get the matrix
      CPoint pLoc, pRot;
      m_MatrixObject.ToXYZLLT (&pLoc, &pRot.p[2], &pRot.p[0], &pRot.p[1]);

      plATTRIBVAL->Required (plATTRIBVAL->Num() + 6);

      wcscpy (av.szName, gpszAttribPosnX);
      av.fValue = pLoc.p[0];
      plATTRIBVAL->Add (&av);

      wcscpy (av.szName, gpszAttribPosnY);
      av.fValue = pLoc.p[1];
      plATTRIBVAL->Add (&av);

      wcscpy (av.szName, gpszAttribPosnZ);
      av.fValue = pLoc.p[2];
      plATTRIBVAL->Add (&av);

      wcscpy (av.szName, gpszAttribRotX);
      av.fValue = pRot.p[0];
      plATTRIBVAL->Add (&av);

      wcscpy (av.szName, gpszAttribRotY);
      av.fValue = pRot.p[1];
      plATTRIBVAL->Add (&av);

      wcscpy (av.szName, gpszAttribRotZ);
      av.fValue = pRot.p[2];
      plATTRIBVAL->Add (&av);
   }
next:

   // see about the attached objects
   PTATTACH pt = (PTATTACH) m_lTATTACH.Get(0);
   DWORD i;
   for (i = 0; i < m_lTATTACH.Num(); i++, pt++) {
      wcscpy (av.szName, pt->szAttrib);
      av.fValue = (pt->fUse ? 1.0 : 0.0);
      plATTRIBVAL->Add (&av);
   }

   // call into sub
   AttribGetAllIntern (plATTRIBVAL);
}

/*****************************************************************************************
CObjectTemplate::AttribSetGroup -
passed an array of ATTRIBVALL structures with new values to pass in
and set. THose attributes that are not known to the object are ignored.
do not override - use AttribSetGroupIntern
*/
void CObjectTemplate::AttribSetGroup (DWORD dwNum, PATTRIBVAL paAttrib)
{
   DWORD dwEmbedBits = 0;  // 0x01 for h, 0x02 for v, 0x03 for rotation
   DWORD dwNonEmbedBits = 0;  // 0x01..0x04 for xyz, 0x08..0x20 for rotxyz
   BOOL fShouldMoveAttach = FALSE;
   fp fRotation, fRotationUse;
   TEXTUREPOINT tp, tpUse;
   CPoint pLoc, pRot, pLocUse, pRotUse;

   DWORD i, dwLen;
   PATTRIBVAL pa;
   PWSTR pszName;
   for (i = 0, pa = paAttrib; i < dwNum; i++, pa++) {
      pszName = pa->szName;
      dwLen = (DWORD)wcslen (pszName);
      if ((dwLen == gdwAttribOnOff) && !_wcsicmp(pszName, gpszAttribOnOff)) {
         TurnOnSet (pa->fValue);
         continue;
      }

      if ((dwLen == gdwAttribOpenClose) && !_wcsicmp(pszName, gpszAttribOpenClose)) {
         OpenSet (pa->fValue);
         continue;
      }

      if ((dwLen == gdwAttribShowHide) && !_wcsicmp(pszName, gpszAttribShowHide)) {
         ShowSet (pa->fValue >= .5);
         continue;
      }

      if (m_fEmbedded) {
         if ((dwLen == gdwAttribEmbedPosnH) && !_wcsicmp(pszName, gpszAttribEmbedPosnH)) {
            dwEmbedBits |= 0x01;
            tpUse.h = pa->fValue;
            continue;
         }
         if ((dwLen == gdwAttribEmbedPosnV) && !_wcsicmp(pszName, gpszAttribEmbedPosnV)) {
            dwEmbedBits |= 0x02;
            tpUse.v = pa->fValue;
            continue;
         }
         if ((dwLen == gdwAttribEmbedRot) && !_wcsicmp(pszName, gpszAttribEmbedRot)) {
            dwEmbedBits |= 0x04;
            fRotationUse = pa->fValue;
            continue;
         }
      }
      else {
         if ((dwLen == gdwAttribPosnX) && !_wcsicmp(pszName, gpszAttribPosnX)) {
            dwNonEmbedBits |= 0x01;
            pLocUse.p[0] = pa->fValue;
            continue;
         }
         if ((dwLen == gdwAttribPosnY) && !_wcsicmp(pszName, gpszAttribPosnY)) {
            dwNonEmbedBits |= 0x02;
            pLocUse.p[1] = pa->fValue;
            continue;
         }
         if ((dwLen == gdwAttribPosnZ) && !_wcsicmp(pszName, gpszAttribPosnZ)) {
            dwNonEmbedBits |= 0x04;
            pLocUse.p[2] = pa->fValue;
            continue;
         }
         if ((dwLen == gdwAttribRotX) && !_wcsicmp(pszName, gpszAttribRotX)) {
            dwNonEmbedBits |= 0x08;
            pRotUse.p[0] = pa->fValue;
            continue;
         }
         if ((dwLen == gdwAttribRotY) && !_wcsicmp(pszName, gpszAttribRotY)) {
            dwNonEmbedBits |= 0x10;
            pRotUse.p[1] = pa->fValue;
            continue;
         }
         if ((dwLen == gdwAttribRotZ) && !_wcsicmp(pszName, gpszAttribRotZ)) {
            dwNonEmbedBits |= 0x20;
            pRotUse.p[2] = pa->fValue;
            continue;
         }
      }

      // see about the attached objects
      PTATTACH pt = (PTATTACH) m_lTATTACH.Get(0);
      DWORD i;
      for (i = 0; i < m_lTATTACH.Num(); i++, pt++) {
         if (_wcsicmp (pa->szName, pt->szAttrib))
            continue;
         BOOL fWant;
         fWant = (pa->fValue >= .5);
         if (pt->fUse == fWant)
            break;   // no change, but found match so dont go futher

         if (m_pWorld)
            m_pWorld->ObjectAboutToChange (this);
         pt->fUse = fWant;
         fShouldMoveAttach |= fWant;
         if (m_pWorld)
            m_pWorld->ObjectChanged (this);
         break;
      }


   } // i, over all pa


   // if the embed info has changed then set that
   if (dwEmbedBits) {
      PCObjectSocket pos;
      DWORD dwSurface;
      pos = NULL;
      if (m_pWorld)
         pos = m_pWorld->ObjectGet (m_pWorld->ObjectFind (&m_gContainer));
      if (!pos)
         goto next;
      if (!pos->ContEmbeddedLocationGet (&m_gGUID, &tp, &fRotation, &dwSurface))
         goto next;
      if (dwEmbedBits & 0x01) {
         tp.h = tpUse.h;
         tp.h = max(0,tp.h);
         tp.h = min(1,tp.h);
      }
      if (dwEmbedBits & 0x02) {
         tp.v = tpUse.v;
         tp.v = max(0,tp.v);
         tp.v = min(1,tp.v);
      }
      if (dwEmbedBits & 0x04)
         fRotation = fRotationUse;

      pos->ContEmbeddedLocationSet (&m_gGUID, &tp, fRotation, dwSurface);
   }
   if (dwNonEmbedBits) {
      m_MatrixObject.ToXYZLLT (&pLoc, &pRot.p[2], &pRot.p[0], &pRot.p[1]);

      if (dwNonEmbedBits & 0x01)
         pLoc.p[0] = pLocUse.p[0];
      if (dwNonEmbedBits & 0x02)
         pLoc.p[1] = pLocUse.p[1];
      if (dwNonEmbedBits & 0x04)
         pLoc.p[2] = pLocUse.p[2];
      if (dwNonEmbedBits & 0x08)
         pRot.p[0] = pRotUse.p[0];
      if (dwNonEmbedBits & 0x10)
         pRot.p[1] = pRotUse.p[1];
      if (dwNonEmbedBits & 0x20)
         pRot.p[2] = pRotUse.p[2];

      // set it
      CMatrix m;
      m.FromXYZLLT (&pLoc, pRot.p[2], pRot.p[0], pRot.p[1]);
      fShouldMoveAttach = FALSE; // will have already moved attached
      ObjectMatrixSet (&m);
   }

   // if changed attached state to true and didn't move outselves, then tell attached
   // objects to move
   if (fShouldMoveAttach)
      TellAttachThatMoved ();

next:
   AttribSetGroupIntern (dwNum, paAttrib);
}

/*****************************************************************************************
CObjectTemplate::AttribInfo -
do not override - use AttribInfoIntern
given an attribute name, this fills in information about the attribute.
returns FALSE if can't find.
*/
BOOL CObjectTemplate::AttribInfo (PWSTR pszName, PATTRIBINFO pInfo)
{
   DWORD dwLen;
   dwLen = (DWORD)wcslen(pszName);
   memset (pInfo, 0, sizeof(*pInfo));

   if ((dwLen == gdwAttribOnOff) && !_wcsicmp(pszName, gpszAttribOnOff)) {
      pInfo->dwType = AIT_NUMBER;
      pInfo->fDefPassUp = TRUE;
      pInfo->fMax = 1;
      pInfo->fMin = 0;
      pInfo->pszDescription = gpszAttribOnOffDesc;
      return TRUE;
   }
   if ((dwLen == gdwAttribOpenClose) && !_wcsicmp(pszName, gpszAttribOpenClose)) {
      pInfo->dwType = AIT_NUMBER;
      pInfo->fDefPassUp = TRUE;
      pInfo->fMax = 1;
      pInfo->fMin = 0;
      pInfo->pszDescription = gpszAttribOpenCloseDesc;
      return TRUE;
   }
   if ((dwLen == gdwAttribShowHide) && !_wcsicmp(pszName, gpszAttribShowHide)) {
      pInfo->dwType = AIT_BOOL;
      pInfo->fDefLowRank = TRUE;
      pInfo->fMax = 1;
      pInfo->fMin = 0;
      pInfo->pszDescription = gpszAttribShowHideDesc;
      return TRUE;
   }

   if ( ((dwLen == gdwAttribPosnX) && !_wcsicmp(pszName, gpszAttribPosnX))  ||
         ((dwLen == gdwAttribPosnY) && !_wcsicmp(pszName, gpszAttribPosnY)) ||
         ((dwLen == gdwAttribPosnZ) && !_wcsicmp(pszName, gpszAttribPosnZ)) ){
      pInfo->dwType = AIT_DISTANCE;
      pInfo->fDefLowRank = TRUE;
      pInfo->fMax = 100;
      pInfo->fMin = -100;
      pInfo->pszDescription = gpszAttribPosnDesc;
      return TRUE;
   }

   if ( ((dwLen == gdwAttribRotX) && !_wcsicmp(pszName, gpszAttribRotX))  ||
         ((dwLen == gdwAttribRotY) && !_wcsicmp(pszName, gpszAttribRotY)) ||
         ((dwLen == gdwAttribRotZ) && !_wcsicmp(pszName, gpszAttribRotZ)) ){
      pInfo->dwType = AIT_ANGLE;
      pInfo->fDefLowRank = TRUE;
      pInfo->fMax = PI;
      pInfo->fMin = -PI;
      // BUGFIX - x is only -90 to + 90
      if (!_wcsicmp(pszName, gpszAttribRotX)) {
         pInfo->fMax /= 2;
         pInfo->fMin /= 2;
      }
      pInfo->pszDescription = gpszAttribRotDesc;
      return TRUE;
   }

   if ( ((dwLen == gdwAttribEmbedPosnH) && !_wcsicmp(pszName, gpszAttribEmbedPosnH))  ||
         ((dwLen == gdwAttribEmbedPosnH) && !_wcsicmp(pszName, gpszAttribEmbedPosnV)) ){
      pInfo->dwType = AIT_NUMBER;
      pInfo->fDefLowRank = TRUE;
      pInfo->fMax = 1;
      pInfo->fMin = 0;
      pInfo->pszDescription = gpszAttribEmbedPosnDesc;
      return TRUE;
   }

   if ((dwLen == gdwAttribEmbedRot) && !_wcsicmp(pszName, gpszAttribEmbedRot)) {
      pInfo->dwType = AIT_ANGLE;
      pInfo->fDefLowRank = TRUE;
      pInfo->fMax = PI;
      pInfo->fMin = -PI;
      pInfo->pszDescription = gpszAttribEmbedRotDesc;
      return TRUE;
   }

   // see about the attached objects
   PTATTACH pt = (PTATTACH) m_lTATTACH.Get(0);
   DWORD i;
   for (i = 0; i < m_lTATTACH.Num(); i++, pt++) {
      if (_wcsicmp (pszName, pt->szAttrib))
         continue;

      pInfo->dwType = AIT_BOOL;
      pInfo->fDefLowRank = TRUE;
      pInfo->fMax = 1;
      pInfo->fMin = 0;
      pInfo->pszDescription = gpszAttribAttachDesc;

      return TRUE;
   }

   // pass on
   return AttribInfoIntern (pszName, pInfo);
}


/*****************************************************************************************
CObjectTemplate::AttribGetIntern - OVERRIDE THIS

Like AttribGet() except that only called if default attributes not handled.
*/
BOOL CObjectTemplate::AttribGetIntern (PWSTR pszName, fp *pfValue)
{
   return FALSE;
}


/*****************************************************************************************
CObjectTemplate::AttribGetAllIntern - OVERRIDE THIS

Like AttribGetAllIntern() EXCEPT plATTRIBVAL is already initialized and filled with
some parameters (default to the object template)
*/
void CObjectTemplate::AttribGetAllIntern (PCListFixed plATTRIBVAL)
{
   return;
}


/*****************************************************************************************
CObjectTemplate::AttribSetGroupIntern - OVERRIDE THIS

Like AttribSetGroup() except passing on non-template attributes.
*/
void CObjectTemplate::AttribSetGroupIntern (DWORD dwNum, PATTRIBVAL paAttrib)
{
   return;
}


/*****************************************************************************************
CObjectTemplate::AttribInfoIntern - OVERRIDE THIS

Like AttribInfo() except called if attribute is not for template.
*/
BOOL CObjectTemplate::AttribInfoIntern (PWSTR pszName, PATTRIBINFO pInfo)
{
   return FALSE;
}


/*****************************************************************************************
CObjectTemplate::AttachNum
Returns the number of objects currently attached to this object.

NOT usually overriden
*/
DWORD CObjectTemplate::AttachNum (void)
{
   return m_lTATTACH.Num();
}

/*****************************************************************************************
CObjectTemplate::AttachEnum
Given an index from 0..AttachNum()-1, this fills in pgAttach with the attached
object's guid. 

NOT usually overridden
*/
BOOL CObjectTemplate::AttachEnum (DWORD dwIndex, GUID *pgAttach)
{
   PTATTACH pt = (PTATTACH) m_lTATTACH.Get(dwIndex);
   if (!pt)
      return FALSE;
   *pgAttach = pt->gID;
   return TRUE;
}



/*****************************************************************************************
CObjectTemplate::AttachAdd
Adds an attachment to the object identified by pgAttach (might already be on this
list, in which case morethan one copy), to the obhect usning the location pszAttach
(gotten from AttachClosest(). pmMatrix is a matrix that converts from the attached object's
space into world space. (Hence, defines the translation and rotation
needed to keep the attached object a constant position to the object.) The object
must also include rotations/translations to accound for pszAttach. NOTE: Assumes
attaching in the current skeleton location, so will also need to account for where
the object would be in the default skelton location

NOT ususally overridden
*/
BOOL CObjectTemplate::AttachAdd (const GUID *pgAttach, PWSTR pszAttach, PCMatrix pmMatrix)
{
   TATTACH ta;
   memset (&ta, 0, sizeof(ta));
   ta.fUse = TRUE;
   ta.gID = *pgAttach;
   wcscpy (ta.szAttach, pszAttach);

   // matrix for going from attached object's space to bone...
   // start out with pmMatrix, which is from attached object space into world
   CMatrix m, mInv;
   m_MatrixObject.Invert4 (&mInv);
   ta.mAttachToBone.Multiply (&mInv, pmMatrix); // now have attached to object's space
   InternAttachMatrix (pszAttach, &m); // m filld with bone into object
   m.Invert4 (&mInv);
   ta.mAttachToBone.MultiplyRight (&mInv);   // now have attach to object to bone

   // find the object attaching to and get the name
   PWSTR pszName;
   pszName = NULL;
   if (m_pWorld) {
      DWORD dwFind = m_pWorld->ObjectFind (&ta.gID);
      PCObjectSocket pos = (dwFind != -1) ? m_pWorld->ObjectGet (dwFind) : NULL;
      if (pos)
         pszName = pos->StringGet (OSSTRING_NAME);
   }
   if (!pszName || !pszName[0])
      pszName = L"Unknown";

   // name for the attribute
   DWORD dwLen, dwLenAttach;
   PWSTR pszAttachString = L"Attach to ";
   dwLenAttach = (DWORD)wcslen(pszAttachString);
   dwLen = (DWORD)wcslen (pszName);
   dwLen = min (58 - dwLenAttach, dwLen); // so can also add numbers
   wcscpy (ta.szAttrib, pszAttachString);
   memcpy (ta.szAttrib + dwLenAttach, pszName, dwLen*2);
   dwLen = dwLenAttach + dwLen;
   ta.szAttrib[dwLen] = 0;

   // make sure unique
   PTATTACH pt;
   pt = (PTATTACH) m_lTATTACH.Get(0);
   DWORD i, dwAdd;
   for (dwAdd = 0; ; dwAdd++) {
      // strip off all #'s an spaces
      ta.szAttrib[dwLen] = 0;

      if (dwAdd)
         swprintf (ta.szAttrib + dwLen, L" %d", (int)dwAdd+1);

      for (i = 0; i < m_lTATTACH.Num(); i++)
         if (!_wcsicmp(pt[i].szAttrib, ta.szAttrib))
            break;
      if (i >= m_lTATTACH.Num())
         break;   // found unique name
   }

   // add it
   if (m_pWorld)
      m_pWorld->ObjectAboutToChange (this);
   m_lTATTACH.Add (&ta);
   if (m_pWorld)
      m_pWorld->ObjectChanged (this);

   return TRUE;
}


/*****************************************************************************************
CObjectTemplate::AttachRemove
Removes an attachment from the list.

NOT usually overridden
*/
BOOL CObjectTemplate::AttachRemove (DWORD dwIndex)
{
   if (dwIndex >= m_lTATTACH.Num())
      return FALSE;

   if (m_pWorld)
      m_pWorld->ObjectAboutToChange (this);
   m_lTATTACH.Remove (dwIndex);
   if (m_pWorld)
      m_pWorld->ObjectChanged (this);

   return TRUE;
}


/*****************************************************************************************
CObjectTemplate::AttachRenamed
Called to tell the object that an attachment has been renamed. Note: pgOld
is not guaranteed to be in the list. If not, just ignore it

NOT usually overridden
*/
BOOL CObjectTemplate::AttachRenamed (const GUID *pgOld, const GUID *pgNew)
{
   PTATTACH pt = (PTATTACH) m_lTATTACH.Get(0);
   DWORD i;
   BOOL fNotified = FALSE;

   for (i = 0; i < m_lTATTACH.Num(); i++, pt++) {
      if (!IsEqualGUID (*pgOld, pt->gID))
         continue;   // not the same

      // else match
      if (!fNotified) {
         fNotified = TRUE;
         if (m_pWorld)
            m_pWorld->ObjectAboutToChange (this);
      }
      pt->gID = *pgNew;

      // tell to move
      TellAttachThatMoved (i);
   }

   if (fNotified && m_pWorld)
      m_pWorld->ObjectChanged (this);

   return fNotified;
}

/*****************************************************************************************
CObjectTemplate::AttachClosest
Given a point (pLoc) in the object's coords, returns a name (max 64 characters incl null)
and stores it in pszClosest. This name can then be used for adding an attachment
In the case of attaching to on object with bones, this will be the bone name

USUALLY OVERRIDDEN
*/
BOOL CObjectTemplate::AttachClosest (PCPoint pLoc, PWSTR pszClosest)
{
   // default behavour is to fill in with an empty string
   pszClosest[0] = 0;
   return TRUE;
}


/*****************************************************************************************
CObjectTemplate::AttachPointsEnum
Filled with the names of all the attachment points to the object. lpEnum is
filled with a list of NULL-terminated strings. The string can be paseed into
AttachAdd(). (AttachClosest() will also return one of these.)
This list might be empty if there are no specific points.

USUALLY OVERRIDDEN
*/
void CObjectTemplate::AttachPointsEnum (PCListVariable plEnum)
{
   // no specific strings
   plEnum->Clear();
}

/*****************************************************************************************
CObjectTemplate::InternAttachMatrix - The superclass object should override this. What it
does is take an attachment name (such as a bone name). This returns a matrix that converts from
bone space into object space, taking into account the movement of the bone.

USUALLY OVERRIDDEN
*/
void CObjectTemplate::InternAttachMatrix (PWSTR pszAttach, PCMatrix pmAttachRot)
{
   // if not overridden just use identity
   pmAttachRot->Identity();
}

/*****************************************************************************************
CObjectTemplate::TellAttachThatMoved - This internal function should be called by the
super-class whenever a bone has moved. (it's also called when the object is moved, but
this is automagic). This, in turn, calls all the attached objects and tells them to
move.

inputs
   DWORD       dwIndex - Use an index number of an attached object if modifying a specific
                     one, else use -1 to modify all
*/
void CObjectTemplate::TellAttachThatMoved (DWORD dwIndex)
{
   PTATTACH pt = (PTATTACH) m_lTATTACH.Get(0);
   DWORD i, dwFind;
   PCObjectSocket pos;
   if (!m_pWorld || m_fAttachRecurse)
      return;  // cant do anything

   // set a flag to make sure dont recurse on this
   m_fAttachRecurse = TRUE;

   for (i = (dwIndex == -1) ? 0 : dwIndex; i < ((dwIndex == -1) ? m_lTATTACH.Num() : (dwIndex+1)); i++, pt++) {
      if (!pt->fUse)
         continue;   // temporarily turned off

      dwFind = m_pWorld->ObjectFind (&pt->gID);
      if (dwFind == -1)
         continue;
      pos = m_pWorld->ObjectGet (dwFind);
      if (!pos)
         continue;

      // calc the matrix
      CMatrix mBoneToObject;
      InternAttachMatrix (pt->szAttach, &mBoneToObject);
      mBoneToObject.MultiplyRight (&m_MatrixObject);  // now have bone to world
      mBoneToObject.MultiplyLeft (&pt->mAttachToBone);   // now have attach to bone

      // if same as what's there ignore
      CMatrix mCur;
      pos->ObjectMatrixGet (&mCur);
      if (!mCur.AreClose (&mBoneToObject))
         pos->ObjectMatrixSet (&mBoneToObject);
   }

   m_fAttachRecurse = FALSE;
}


/*****************************************************************************************
CObjectTemplate::EditorCreate - From CObjectSocket. SOMETIMES OVERRIDDEN
*/
BOOL CObjectTemplate::EditorCreate (BOOL fAct)
{
   return FALSE;
}


/*****************************************************************************************
CObjectTemplate::EditorCreate - From CObjectSocket. SOMETIMES OVERRIDDEN
*/
BOOL CObjectTemplate::EditorDestroy (void)
{
   return TRUE;
}

/*****************************************************************************************
CObjectTemplate::EditorCreate - From CObjectSocket. SOMETIMES OVERRIDDEN
*/
BOOL CObjectTemplate::EditorShowWindow (BOOL fShow)
{
   return FALSE;
}

