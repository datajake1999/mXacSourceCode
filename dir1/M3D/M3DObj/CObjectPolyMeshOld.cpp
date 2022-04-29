/************************************************************************
CObjectPolyMeshOld.cpp - Draws a PolyMesh.

begun 1/9/02 by Mike Rozak
Copyright 2002 Mike Rozak. All rights reserved
*/


/**********************************************************************************
CObjectPolyMeshOld */

// {722D3D0D-8F4B-4a82-B0D3-83D601511755}
DEFINE_GUID(CLSID_PolyMeshOld, 
0x892d3d62, 0x1f4b, 0x4a82, 0xac, 0xd3, 0x83, 0xf0, 0x1, 0x51, 0x21, 0x55);


// CObjectPolyMeshOld is a test object to test the system
class DLLEXPORT CObjectPolyMeshOld : public CObjectTemplate {
public:
   CObjectPolyMeshOld (PVOID pParams, POSINFO pInfo);
   ~CObjectPolyMeshOld (void);

   virtual void Delete (void);
   virtual void Render (POBJECTRENDER pr);
   virtual CObjectSocket *Clone (void);
   virtual PCMMLNode2 MMLTo (void);
   virtual BOOL MMLFrom (PCMMLNode2 pNode);
   virtual BOOL DialogCPQuery (void);
   virtual BOOL DialogCPShow (DWORD dwSurface, PCEscWindow pWindow);
   virtual BOOL DialogQuery (void);
   virtual BOOL DialogShow (DWORD dwSurface, PCEscWindow pWindow);
   virtual BOOL ControlPointQuery (DWORD dwID, POSCONTROL pInfo);
   virtual BOOL ControlPointSet (DWORD dwID, PCPoint pVal, PCPoint pViewer);
   virtual void ControlPointEnum (PCListFixed plDWORD);
   //virtual BOOL Message (DWORD dwMessage, PVOID pParam);
   virtual BOOL Merge (GUID *pagWith, DWORD dwNum);
   virtual BOOL AttribGetIntern (PWSTR pszName, fp *pfValue);
   virtual void AttribGetAllIntern (PCListFixed plATTRIBVAL);
   virtual void AttribSetGroupIntern (DWORD dwNum, PATTRIBVAL paAttrib);
   virtual BOOL AttribInfoIntern (PWSTR pszName, PATTRIBINFO pInfo);

//priviate:
   void CalcScale (void);
   void CreateBasedOnType (void);
   void CreateSphere (DWORD dwNum, PCPoint pScale);
   void CreateCylinder (DWORD dwNum, PCPoint pScale, PCPoint pScaleNegative, BOOL fCap);
   void CreateCylinderRounded (DWORD dwNum, PCPoint pScale, PCPoint pScaleNegative);
   void CreateTaurus (DWORD dwNum, PCPoint pScale, PCPoint pInnerScale);
   void CreateBox (DWORD dwNum, PCPoint pScale);
   void CreatePlane (DWORD dwNum, PCPoint pScale);
   void CreateCone (DWORD dwNum, PCPoint pScale, BOOL fShowTop, BOOL fShowBottom,
                                  fp fTop, fp fBottom);
   void CalcMainNormals (void);
   BOOL MagneticDrag (DWORD dwID, PCPoint pVal);
   void PointToVertices (PCPoint pPoint, fp fDist, PCListFixed plVert);
   void VerticesToSides (DWORD *padwVertex, DWORD dwNumVertex, PCListFixed plSides);
   void SidesAddDetail (DWORD *padwSide, DWORD dwNumSide, BOOL fDivideAll = FALSE);
   void TextureSpherical (void);
   void TextureCylindrical (void);
   void TextureLinear (BOOL fFront);
   void ClearVertSide (BOOL fOnlyDiv = FALSE);
   void EnumEdges (PCListFixed plCPMSide, PCListFixed plPMEDGE, BOOL fBiDi = FALSE);
   void SubdivideMesh (PCListFixed plPMSideOrig, PCListFixed plPMVertOrig,
                                     PCListFixed plPMSideNew, PCListFixed plPMVertNew);
   void SubdivideSide (DWORD dwSide);
   void SubdivideIfNecessary (void);
   void FixWraparoundTextures (void);
   void WipeExtraText(void);
   void CalcVertexGroups (void);
   void CalcVertexSideInGroup (DWORD dwCenter);
   void CalcMorphRemap (void);
   void ShowHideSide (DWORD dwSide);
   void RemoveVertex (DWORD dwVert);
   void RemoveVertexSymmetry (DWORD dwVert);
   BOOL Merge (CObjectPolyMeshOld *pMerge, PCMatrix pmTrans);

   DWORD    m_dwDisplayControl;        // which control points displayed, 0=scale, 1=control points
   fp       m_fDragSize;               // how large an area is to be dragged
   DWORD    m_dwSymmetry;              // bit field. 0=x, 1=y, 2=z
   DWORD    m_dwSubdivide;             // number of times to subdivide - for smoothing
   BOOL     m_fCanBackface;            // set to TRUE if can backface cull, FALSE if can't
   BOOL     m_fMergeSmoothing;         // set to TRUE if do extra smoothing when merge
   DWORD    m_dwDivisions;             // number of divisions. Use 0 to indicate that is in phase II
   DWORD    m_dwVertGroupDist;         // place a vertex group every N vertices
   DWORD    m_dwVertSideGroupDist;     // actual amount of area displayed when click on vertex, usually > m_dwVertGroupDist
   CPoint   m_apParam[2];              // parameters, meaning depends upon base shape in m_dwType

   CListFixed m_lCPMVert;               // list of vertext points for the object
   CListFixed m_lCPMSide;               // list of triangles in the object
   CListFixed m_lPCOEAttrib;            // list of pointers to attributes (morphs) supported
                                        // by polygon mesh

   // calculated
   CPoint   m_pScaleMax;               // highest points in size
   CPoint   m_pScaleMin;               // smallest points in size
   CPoint   m_pScaleSize;              // max - min, at least size of CLOSE
   CPoint   m_pLastMoved;              // location of the last-moved control point
   CListFixed m_lVertexInGroup;        // list of vertex indecies that are to have control points
   CListFixed m_lSideInGroup;          // list of side indecies that are to have control points
   CListFixed m_lVertexGroup;          // list of DWORDs that are vertex indecies indicating where the centers of groups are
                                       // sorted ascending order
   BOOL     m_fVertGroupDirty;         // set to true if the vertex group info is dirty
   DWORD    m_dwCurVertGroup;          // current vertex index that grouped around. -1 if not initialized
   CListFixed  m_lMorphRemapID;        // IDs for morph remapping. DWORD values
   CListFixed  m_lMorphRemapValue;     // values for morph remapping. fp values

   // calculated, divisions
   CListFixed m_lCPMVertDiv;           // divided vertex points, acutally used for rendering
   CListFixed m_lCPMSideDiv;           // divided sides, acutally used for rendering
   BOOL     m_fDivDirty;               // set to true if the division info is dirty

};
typedef CObjectPolyMeshOld *PCObjectPolyMeshOld;




#include <windows.h>
#include "escarpment.h"
#include "..\M3D.h"
#include "resource.h"

#define MAXVERTGROUP 1024        // maximum number of vertices in the group

#define MAXVERT      100000         // maximum verticies allow in a polygon mesh

// PMEDGE - Used by some functions for maintaining a list of edges
typedef struct {
   DWORD       adwVert[2];    // two verticies at either end of edge. adwVert[0] < adwVert[1]
   DWORD       adwSide[2];    // two sides that create the edge.
} PMEDGE, *PPMEDGE;

#if 0
// VERTDEFORM - How much to deform a vertex based on deformation
typedef struct {
   DWORD          dwDeform;   // deform index
   CPoint         pDeform;    // amount to deform
} VERTDEFORM, *PVERTDEFORM;
#endif // 0

// CPMVertOld - C++ object for the vertex of a polymesh
class CPMVertOld : public CEscObject {
public:
   void Zero ();  // call this to zero out memory locations that point to allocated data
   void Release ();  // call this to release allocated data
   void CloneTo (CPMVertOld *pTo, BOOL fCloneDeform = TRUE);  // copied over, and allocates new copies of data
   void HalfWay (CPMVertOld *pVert1, CPMVertOld *pVert2, DWORD dwSide, BOOL fUseDeformed); // figures out half way point, incl normals
   void HalfWay (CPMVertOld *pWith, PCPoint pLoc, BOOL fUseDeformed);
   // old code void MidPoint (CPMVertOld *pVert1, CPMVertOld *pVert2, DWORD dwSide);

   PCMMLNode2 MMLTo (void);
   BOOL MMLFrom (PCMMLNode2 pNode);

   void DeformChanged (DWORD dwDeform, PCPoint pDelta);
   void DeformRename (DWORD dwNum, DWORD *padwOrig, DWORD *padwNew);  // so can rename associations with deformations
   void DeformRemove (DWORD dwRemove);
   void SideRename (DWORD dwNum, DWORD *padwOrig, DWORD *padwNew); // so can rename associations with sides
   void SideRemove (DWORD dwSide);  // called when a side has been removed. Anything above has index reduced by 1
   
   PTEXTUREPOINT TextureGet (DWORD dwSide = -1);     // returns texture based on the side
   void TextureSet (DWORD dwSide, PTEXTUREPOINT pt);     // set the texture based on the side
   void TextureClear (void);  // clears additional texture info if there is any

   void LocationGet (DWORD dwNum, DWORD *padwDeform, fp *pafDeform, PCPoint pLoc);  // get location including deformations
   void DeformCalc (DWORD dwNum, DWORD *padwDeform, fp *pafDeform);  // fill in m_pLocWithDeform based on deformations
   void NormCalc (DWORD dwIndex, PCListFixed plSides); // calculate the m_pNorm. Assumes m_pLocWithjDeform filled in

   void Scale (PCMatrix pScale, PCMatrix pScaleInvTrans);   // scales point and normals

   // variables
   CPoint         m_pLoc;    // location in object space
   TEXTUREPOINT   m_tText;   // texture. Automagically calculated too
   PCListFixed    m_plVERTDEFORM;   // pointer to vertex deformation information
   PCListFixed    m_plSIDETEXT;     // textures specific to sides
   // BUGBUG - Fur info goes here

   // automatically calculated elsewhere
   CPoint         m_pLocWithDeform; // current location based on deformation. Filled in elsewhere
   CPoint         m_pNorm;   // normal, in object space, already normalized. Calculated from pLoc and sides
};
typedef CPMVertOld *PCPMVertOld;


// CPMSideOld - handles all the information about a side
class CPMSideOld : public CEscObject {
public:
   void Zero (void);  // call this to zero out memory locations that point to allocated data
   void Release (void);  // call this to release allocated data
   void CloneTo (CPMSideOld *pTo);  // copied over, and allocates new copies of data

   PCMMLNode2 MMLTo (void);
   BOOL MMLFrom (PCMMLNode2 pNode);

   void VertRename (DWORD dwNum, DWORD *padwOrig, DWORD *padwNew);  // so can rename associations with vertecies
   void VertRemove (DWORD dwVert);  // called when vertex removed
   DWORD Num (void); // returns number of points
   void NormCalc (PCListFixed plVert); // calculate the m_pNorm. Assumes m_pLocWithjDeform filled in
   void Reverse (void);

   // variables
   DWORD          m_adwVert[4];    // vertex ID's in clockwise dir. Last one may be -1 if only triangle
   BOOL           m_fHide;    // if TRUE then the side is hidden and not shown

   // automatic/scratch
   CPoint         m_pNorm;   // scratch space. Normal for the side
   DWORD          m_dwOrigSide; // used when subdividing to keep track of the original side
};
typedef CPMSideOld *PCPMSideOld;


DWORD NumMorphIDs (PCListFixed plPCOEAttrib);


/**********************************************************************************
OECALCompare - Used for sorting attributes in m_lPCOEAttrib
*/
static int _cdecl OECALCompare (const void *elem1, const void *elem2)
{
   PCOEAttrib *pdw1, *pdw2;
   pdw1 = (PCOEAttrib*) elem1;
   pdw2 = (PCOEAttrib*) elem2;

   return wcsicmp(pdw1[0]->m_szName, pdw2[0]->m_szName);
}


/**********************************************************************************
CPMVertOld::Zero - Clears out the memory locations to 0 that are pointers. This enures
that a call to Release() wont gp fault.
*/
void CPMVertOld::Zero (void)
{
   m_plVERTDEFORM = NULL;
   m_plSIDETEXT = NULL;
}


/**********************************************************************************
CPMVertOld::Release - Frees up allocated memory stored in the vertex.
*/
void CPMVertOld::Release (void)
{
   if (m_plVERTDEFORM) {
      delete m_plVERTDEFORM;
      m_plVERTDEFORM = NULL;
   }
   if (m_plSIDETEXT) {
      delete m_plSIDETEXT;
      m_plSIDETEXT = NULL;
   }
}

/**********************************************************************************
CPMVertOld::CloneTo - Copies all the data from the current vertex to the new one.
It ASSUMES the new one has NO memory allocated, since everything is copied over.

inputs
   PCPMVertOld    *pTo - Copies the info to
   BOOL        fCloneDeform - If TRUE (defualt) then clone the deformation info also.
               If false, only clone the texture info.
*/
void CPMVertOld::CloneTo (CPMVertOld *pTo, BOOL fCloneDeform)
{
   pTo->m_pLoc.Copy (&m_pLoc);
   pTo->m_tText = m_tText;
   pTo->m_pNorm.Copy (&m_pNorm);
   pTo->m_pLocWithDeform.Copy (&m_pLocWithDeform);

   if (fCloneDeform && m_plVERTDEFORM) {
      pTo->m_plVERTDEFORM = new CListFixed;
      if (pTo->m_plVERTDEFORM)
         pTo->m_plVERTDEFORM->Init (sizeof(VERTDEFORM), m_plVERTDEFORM->Get(0), m_plVERTDEFORM->Num());
   }
   else
      pTo->m_plVERTDEFORM = NULL;

   if (m_plSIDETEXT) {
      pTo->m_plSIDETEXT = new CListFixed;
      if (pTo->m_plSIDETEXT)
         pTo->m_plSIDETEXT->Init (sizeof(SIDETEXT), m_plSIDETEXT->Get(0), m_plSIDETEXT->Num());
   }
   else
      pTo->m_plSIDETEXT = NULL;
}

/**********************************************************************************
CPMVertOld::HalfWay - Fills this side with a point half way between vertex 1 and vertex 2,
assuming they're both on side dwSide. NOTE: The m_pNorm in both vertecies must be
valid. This will also interpolate all the textures and deformations.

inputs
   PCPMVertOld    pVert1, pVert2 - Two vertices to interpolate between.
   DWORD       dwSide - Side that is common to the two verticies (needed for texture)
   BOOL        fUseDeformed - If TRUE, use already deformed coords. In this case,
                  wont actually store any deformations onto this
returns
   none
*/
void CPMVertOld::HalfWay (PCPMVertOld pVert1, PCPMVertOld pVert2, DWORD dwSide, BOOL fUseDeformed)
{
   // average the points
   pVert1->HalfWay (pVert2, &m_pLoc, fUseDeformed);
   m_pLocWithDeform.Copy (&m_pLoc);  // in case using deformed, and to have something there

   // texture
   PTEXTUREPOINT pt1, pt2;
   m_plSIDETEXT = NULL;
   pt1 = pVert1->TextureGet (dwSide);
   pt2 = pVert2->TextureGet (dwSide);
   m_tText.h = (pt1->h + pt2->h) / 2;
   m_tText.v = (pt1->v + pt2->v) / 2;

   // deformation
   DWORD i, j;
   m_plVERTDEFORM = NULL;
   if (!fUseDeformed && ((pVert1->m_plVERTDEFORM && pVert1->m_plVERTDEFORM->Num()) ||
      (pVert2->m_plVERTDEFORM && pVert2->m_plVERTDEFORM->Num()) ))
      m_plVERTDEFORM = new CListFixed;
   if (m_plVERTDEFORM) {
      m_plVERTDEFORM->Init (sizeof(VERTDEFORM));

      DWORD dwNum1, dwNum2;
      PVERTDEFORM pVD1, pVD2;
      VERTDEFORM VD;
      dwNum1 = pVert1->m_plVERTDEFORM ? pVert1->m_plVERTDEFORM->Num() : 0;
      dwNum2 = pVert2->m_plVERTDEFORM ? pVert2->m_plVERTDEFORM->Num() : 0;
      pVD1 = dwNum1 ? (PVERTDEFORM) pVert1->m_plVERTDEFORM->Get(0) : NULL;
      pVD2 = dwNum2 ? (PVERTDEFORM) pVert2->m_plVERTDEFORM->Get(0) : NULL;
      for (i = 0; i < dwNum1; i++) {
         for (j = 0; j < dwNum2; j++)
            if (pVD1[i].dwDeform == pVD2[j].dwDeform)
               break;
         VD = pVD1[i];
         if (j < dwNum2)
            VD.pDeform.Average (&pVD2[j].pDeform);
         else
            VD.pDeform.Scale (.5);  // since only exists in 1, other is 0
         m_plVERTDEFORM->Add (&VD);
      }

      // other way around
      for (i = 0; i < dwNum2; i++) {
         for (j = 0; j < dwNum1; j++)
            if (pVD1[j].dwDeform == pVD2[i].dwDeform)
               break;
         if (j < dwNum1)
            continue;   // exists in both so has already been added
         VD = pVD2[i];
         VD.pDeform.Scale (.5);  // since only exists in one, scale other
         m_plVERTDEFORM->Add (&VD);
      }
   }

   // zero out other bits
   m_pNorm.Zero();
}


/**********************************************************************************
CPMVertOld::HalfWay - Fills in pLoc with a point that is a midpoint of the arc defined
by this and pWith. The half way takes into account m_pNorm and m_pLocWithDeform, so
they must already have been calculated for both.

inputs
   PCPMVertOld       pWith - Half way location with this one
   PCPoint        pLoc - Fill in location
   BOOL           fUseDeformed - If TRUE, use already deformed coords.
retursn
   none
*/
void CPMVertOld::HalfWay (CPMVertOld *pWith, PCPoint pLoc, BOOL fUseDeformed)
{
   PCPMVertOld pVert1 = this, pVert2 = pWith;
   PCPoint pLoc1 = fUseDeformed ? &pVert1->m_pLocWithDeform : &pVert1->m_pLoc;
   PCPoint pLoc2 = fUseDeformed ? &pVert2->m_pLocWithDeform : &pVert2->m_pLoc;

   // use the norm to determine the direction of the line
   CPoint pDir, pPerp, pDir1, pDir2;
   fp fLen;
   pDir.Subtract (pLoc2, pLoc1);
   fLen = pDir.Length();
   pPerp.CrossProd (&pVert1->m_pNorm, &pDir);
   pDir1.CrossProd (&pPerp, &pVert1->m_pNorm);
   pDir1.Normalize();
   pDir1.Scale (fLen/4);   // basically doing bezier curve averaging
   pDir1.Add (pLoc1);
   pPerp.CrossProd (&pVert2->m_pNorm, &pDir);
   pDir2.CrossProd (&pVert2->m_pNorm, &pPerp);
   pDir2.Normalize();
   pDir2.Scale (fLen/4);
   pDir2.Add (pLoc2);
   pLoc->Average (&pDir1, &pDir2);
}

#if 0 // not used, old code
/**********************************************************************************
CPMVertOld::MidPoint - Used when subdividing polygons. Fills this side with a point half
way between vertex 1 and vertex 2, assuming they're both on side dwSide.
This will also interpolate all the textures. NOTE: It THROWS OUT the deformations.

inputs
   PCPMVertOld    pVert1, pVert2 - Two vertices to interpolate between.
   DWORD       dwSide - Side that is common to the two verticies (needed for texture)
returns
   none
*/
void CPMVertOld::MidPoint (PCPMVertOld pVert1, PCPMVertOld pVert2, DWORD dwSide)
{
   Zero();
   m_pLoc.Average (&pVert1->m_pLoc, &pVert2->m_pLoc);
   m_pLocWithDeform.Average (&pVert1->m_pLocWithDeform, &pVert2->m_pLocWithDeform);

   // texture
   PTEXTUREPOINT pt1, pt2;
   m_plSIDETEXT = NULL;
   pt1 = pVert1->TextureGet (dwSide);
   pt2 = pVert2->TextureGet (dwSide);
   m_tText.h = (pt1->h + pt2->h) / 2;
   m_tText.v = (pt1->v + pt2->v) / 2;


   // zero out other bits
   m_pNorm.Zero();
}
#endif // 0

/**********************************************************************************
CPMVertOld::MMLTo - Returns a new PCMMLNode2 with the information for the vertex
written in. THis must be freed by the caller.

returns
   PCMMLNode2 - Information
*/
static PWSTR gpszVert = L"Vert";
static PWSTR gpszLoc = L"Loc";
static PWSTR gpszText = L"Text";
PCMMLNode2 CPMVertOld::MMLTo (void)
{
   PCMMLNode2 pNode = new CMMLNode2;
   if (!pNode)
      return NULL;

   pNode->NameSet (gpszVert);
   MMLValueSet (pNode, gpszLoc, &m_pLoc);
   MMLValueSet (pNode, gpszText, &m_tText);

   DWORD i;
   WCHAR szTemp[64];
   if (m_plVERTDEFORM) {
      PVERTDEFORM pv = (PVERTDEFORM) m_plVERTDEFORM->Get(0);
      for (i = 0; i < m_plVERTDEFORM->Num(); i++) {
         swprintf (szTemp, L"DefID%d", (int) i);
         MMLValueSet (pNode, szTemp, (int) pv[i].dwDeform);

         swprintf (szTemp, L"DefP%d", (int) i);
         MMLValueSet (pNode, szTemp, &pv[i].pDeform);
      }
   }
   if (m_plSIDETEXT) {
      PSIDETEXT pv = (PSIDETEXT) m_plSIDETEXT->Get(0);
      for (i = 0; i < m_plSIDETEXT->Num(); i++) {
         swprintf (szTemp, L"TextID%d", (int) i);
         MMLValueSet (pNode, szTemp, (int) pv[i].dwSide);

         swprintf (szTemp, L"TextP%d", (int) i);
         MMLValueSet (pNode, szTemp, &pv[i].tp);
      }
   }

   return pNode;
}

/**********************************************************************************
CPMVertOld::MMLFrom - Fills all the information in based on the PCMMLNode2.
ASSUMES that the allocated memory of the vertex has already been freed and can
be overwritten without problem.

inputs
   PCMMLNode2      pNode - read fro
returns
   BOOL - TRUE if success
*/
BOOL CPMVertOld::MMLFrom (PCMMLNode2 pNode)
{
   // make sure pointers are zero
   Zero();


   // fill in some defaults
   m_pNorm.Zero();
   m_pLocWithDeform.Zero();

   MMLValueGetPoint (pNode, gpszLoc, &m_pLoc);
   MMLValueGetTEXTUREPOINT (pNode, gpszText, &m_tText);

   DWORD i;
   WCHAR szTemp[64];
   for (i = 0; ; i++) {
      VERTDEFORM vd;
      
      swprintf (szTemp, L"DefID%d", (int) i);
      vd.dwDeform = (DWORD) MMLValueGetInt (pNode, szTemp, -1);
      if (vd.dwDeform == -1)
         break;

      swprintf (szTemp, L"DefP%d", (int) i);
      MMLValueGetPoint (pNode, szTemp, &vd.pDeform);

      if (!m_plVERTDEFORM) {
         m_plVERTDEFORM = new CListFixed;
         if (m_plVERTDEFORM)
            m_plVERTDEFORM->Init (sizeof(VERTDEFORM));
      }
      if (m_plVERTDEFORM)
         m_plVERTDEFORM->Add (&vd);
   }

   for (i = 0; ; i++) {
      SIDETEXT vd;
      
      swprintf (szTemp, L"TextID%d", (int) i);
      vd.dwSide = (DWORD) MMLValueGetInt (pNode, szTemp, -1);
      if (vd.dwSide == -1)
         break;

      swprintf (szTemp, L"TextP%d", (int) i);
      MMLValueGetTEXTUREPOINT (pNode, szTemp, &vd.tp);

      if (!m_plSIDETEXT) {
         m_plSIDETEXT = new CListFixed;
         if (m_plSIDETEXT)
            m_plSIDETEXT->Init (sizeof(SIDETEXT));
      }
      if (m_plSIDETEXT)
         m_plSIDETEXT->Add (&vd);
   }

   return TRUE;
}

/*************************************************************************************
DWORDSearch - Given a pointer to a CListFixed containing DWORDs, sorted in
ascending order, this finds the index of the matching DWORD. Or, -1 if cant find

inputs
   DWORD       dwFind - Item to find
   DWORD       dwNum - Number of items in the list
   DWORD       *padwElem - Pointer to list of elements. MUST be sorted.
returns
   DWORD - Index of found, -1 if can't find
*/
static int _cdecl BDWORDCompare (const void *elem1, const void *elem2)
{
   DWORD *pdw1, *pdw2;
   pdw1 = (DWORD*) elem1;
   pdw2 = (DWORD*) elem2;

   return (int) (*pdw1) - (int)(*pdw2);
}

#if 0 // old code

DWORD DWORDSearch (DWORD dwFind, DWORD dwNum, DWORD *padwElem)
{
   DWORD *pdw;
   pdw = (DWORD*) bsearch (&dwFind, padwElem, dwNum, sizeof(DWORD), BDWORDCompare);
   if (!pdw)
      return -1;
   return (DWORD) ((PBYTE) pdw - (PBYTE) padwElem) / sizeof(DWORD);
}
#endif // 0

/**********************************************************************************
CPMVertOld::DeformRename - Informs the vertex when deformations have been renamed (change
deform ID 5 to deform ID 28). The vertex changes its internal structures to reflect
this renaming.

inputs
   DWORD       dwNum - Number of deformations
   DWORD       *padwOrig - Pointer to an array of DWORDs with original IDs.
                  This MUST be sorted in ascending order.
   DWORD       *padwNew - New values to replace those found in the original.
                  If the new one is -1 then it's deleted
returns
   none
*/
void CPMVertOld::DeformRename (DWORD dwNum, DWORD *padwOrig, DWORD *padwNew)
{
   if (!m_plVERTDEFORM)
      return;  // nothing to change

   // loop through renaming all
   PVERTDEFORM pvd = (PVERTDEFORM) m_plVERTDEFORM->Get(0);
   DWORD i, dwFind;
   for (i = m_plVERTDEFORM->Num() - 1; i < m_plVERTDEFORM->Num(); i--) {
      dwFind = DWORDSearch (pvd[i].dwDeform, dwNum, padwOrig);
      if (dwFind == -1)
         continue;   // not in list

      // if set to -1 then remove
      if (padwNew[dwFind] == -1) {
         m_plVERTDEFORM->Remove (i);
         pvd = (PVERTDEFORM) m_plVERTDEFORM->Get(0);

         if (!m_plVERTDEFORM->Num()) {
            delete m_plVERTDEFORM;
            m_plVERTDEFORM = NULL;
            return;  // all done
         }

         continue;
      }

      pvd[i].dwDeform = padwNew[dwFind];
   }
}


/**********************************************************************************
CPMVertOld::DeformRemove - Informs the vertex when deformations have been removed.
All numbers above this are decremented one. The vertex changes its internal structures to reflect
this renaming.

inputs
   DWORD       dwRemove - Index removed
returns
   none
*/
void CPMVertOld::DeformRemove (DWORD dwRemove)
{
   if (!m_plVERTDEFORM)
      return;  // nothing to change

   // loop through renaming all
   PVERTDEFORM pvd = (PVERTDEFORM) m_plVERTDEFORM->Get(0);
   DWORD i;
   for (i = m_plVERTDEFORM->Num() - 1; i < m_plVERTDEFORM->Num(); i--) {
      if (pvd[i].dwDeform < dwRemove)
         continue;   // no change
      if (pvd[i].dwDeform > dwRemove) {
         // decrement
         pvd[i].dwDeform--;
         continue;
      }

      // else the same
      m_plVERTDEFORM->Remove (i);
      pvd = (PVERTDEFORM) m_plVERTDEFORM->Get(0);

      if (!m_plVERTDEFORM->Num()) {
         delete m_plVERTDEFORM;
         m_plVERTDEFORM = NULL;
         return;  // all done
      }
   }
}

/**********************************************************************************
CPMVertOld::SideRename - Informs the vertex when sides have been renamed (change
deform ID 5 to deform ID 28). The vertex changes its internal structures to reflect
this renaming.

inputs
   DWORD       dwNum - Number of sides
   DWORD       *padwOrig - Pointer to an array of DWORDs with original IDs.
                  This MUST be sorted in ascending order.
   DWORD       *padwNew - New values to replace those found in the original
                  if the new on is -1 then it's deleted.
returns
   none
*/
void CPMVertOld::SideRename (DWORD dwNum, DWORD *padwOrig, DWORD *padwNew)
{
   if (!m_plSIDETEXT)
      return;  // nothing to change

   // loop through renaming all
   PSIDETEXT pvd = (PSIDETEXT) m_plSIDETEXT->Get(0);
   DWORD i, dwFind;
   for (i = m_plSIDETEXT->Num()-1; i < m_plSIDETEXT->Num(); i--) {
      dwFind = DWORDSearch (pvd[i].dwSide, dwNum, padwOrig);
      if (dwFind == -1)
         continue;   // not in list

      // if set to -1 then remove
      if (padwNew[dwFind] == -1) {
         m_plSIDETEXT->Remove (i);
         pvd = (PSIDETEXT) m_plSIDETEXT->Get(0);
         if (!m_plSIDETEXT->Num()) {
            delete m_plSIDETEXT;
            m_plSIDETEXT = NULL;
            return;  // all deleted
         }
         continue;
      }

      pvd[i].dwSide = padwNew[dwFind];
   }
}

/**********************************************************************************
CPMVertOld::SideRemove - Called when one of the sides has been removed. If any
   texture into refers to the side it is removed. If it refers to an index
   above the side then the index is reduced by 1.

inputs
   DWORD dwSide - Side removed
returns
   none
*/
void CPMVertOld::SideRemove (DWORD dwSide)
{
   if (!m_plSIDETEXT)
      return;  // nothing to change

   // loop through renaming all
   PSIDETEXT pvd = (PSIDETEXT) m_plSIDETEXT->Get(0);
   DWORD i;
   for (i = m_plSIDETEXT->Num()-1; i < m_plSIDETEXT->Num(); i--) {
      if (pvd[i].dwSide < dwSide)
         continue;   // below list, so no change
      else if (pvd[i].dwSide > dwSide)
         pvd[i].dwSide--;  // above, so reduce item
      else {
         m_plSIDETEXT->Remove (i);
         pvd = (PSIDETEXT) m_plSIDETEXT->Get(0);
      }
   }
}


/**********************************************************************************
CPMVertOld::TextureGet - Given a side (or -1 if side unknown), this returns a pointer
to the texture point. This pointer is only valid until textureset is called.

inputs
   DWORD       dwSide - Which side. If the side can't be found in the remap list returns the default text
returns
   PTEXTUREPOINT - Pointer to the texture
*/
PTEXTUREPOINT CPMVertOld::TextureGet (DWORD dwSide)
{
   if (!m_plSIDETEXT || (dwSide == -1))
      return &m_tText;  // only one setting so use

   DWORD i, dwNum;
   PSIDETEXT ps;
   dwNum = m_plSIDETEXT->Num();
   ps = (PSIDETEXT) m_plSIDETEXT->Get(0);
   for (i = 0; i < dwNum; i++)
      if (dwSide == ps[i].dwSide)
         return &ps[i].tp;

   // else not found
   return &m_tText;
}


/**********************************************************************************
CPMVertOld::TextureSet - Sets the texture for the specific side. NOTE: If the side is
new to the vertex it normally adds an entry. However, if the texture point is the
same as what exists nothing is added.

inputs
   DWORD          dwSide - Side to set it for. If the side is not already in the remap
                  list this will add it. If this is -1 then the default side will be set.
   PTEXTUREPOINT  pt - Set to
returns
   none
*/
void CPMVertOld::TextureSet (DWORD dwSide, PTEXTUREPOINT pt)
{
   if (dwSide == -1) {
      m_tText = *pt;
      return;
   }

   // see if already in list
   if (m_plSIDETEXT) {
      DWORD i, dwNum;
      PSIDETEXT ps;
      dwNum = m_plSIDETEXT->Num();
      ps = (PSIDETEXT) m_plSIDETEXT->Get(0);
      for (i = 0; i < dwNum; i++)
         if (dwSide == ps[i].dwSide) {
            ps[i].tp = *pt;
            return;
         }
   }

   // if it's the same then add nothing
   if (AreClose (pt, &m_tText))
      return;  // no change

   // else, add
   SIDETEXT st;
   st.dwSide = dwSide;
   st.tp = *pt;
   if (!m_plSIDETEXT) {
      m_plSIDETEXT = new CListFixed;
      if (m_plSIDETEXT)
         m_plSIDETEXT->Init (sizeof(SIDETEXT));
   }
   if (m_plSIDETEXT)
      m_plSIDETEXT->Add (&st);
}


/**********************************************************************************
CPMVertOld::TextureClear - Clears additional textures if there are any
*/
void CPMVertOld::TextureClear (void)
{
   if (m_plSIDETEXT) {
      delete m_plSIDETEXT;
      m_plSIDETEXT = NULL;
   }
}


/**********************************************************************************
CPMVertOld::LocationGet - Given a set of deformations, this calculates the location of
the vertex given the deformations.

inputs
   DWORD       dwNum - Number of active deformations
   DWORD       *padwDeform - Pointer to an array of deformations. This MUST be sorted in ascending order
   fp          *pafDeform - Pointer to an array of deformation amounts (from 0 to 1)
   PCPoint     pLoc - Filled in with the location for this vertex
returns
   none
*/
void CPMVertOld::LocationGet (DWORD dwNum, DWORD *padwDeform, fp *pafDeform, PCPoint pLoc)
{
   // starting location
   pLoc->Copy (&m_pLoc);
   if (!m_plVERTDEFORM)
      return;  // all done

   // go through all the deformaitons
   DWORD i, dwNumV, dwFind;
   PVERTDEFORM pv;
   CPoint p;
   dwNumV = m_plVERTDEFORM->Num();
   pv = (PVERTDEFORM) m_plVERTDEFORM->Get(0);
   for (i = 0; i < dwNumV; i++) {
      dwFind = DWORDSearch (pv[i].dwDeform, dwNum, padwDeform);
      if (dwFind == -1)
         continue;   // assume 0

      // else value
      p.Copy (&pv[i].pDeform);
      p.Scale (pafDeform[dwFind]);
      pLoc->Add (&p);
   }
}


/**********************************************************************************
CPMVertOld::DeformChanged - Incremennts the vertex's location by pDelta. If dwDeform
==-1 then m_pLoc is changed. Else, the given deformation is changed. Automatically
adds a deformation entry if none exists, or removes it if it's set to 0.

inputs
   DWORD       dwDeform - Deformation number, or -1 if m_pLoc
   CPoint      pDelta - Amount to change by.
returns
   none
*/
void CPMVertOld::DeformChanged (DWORD dwDeform, PCPoint pDelta)
{
   if (dwDeform == -1) {
      m_pLoc.Add (pDelta);
      return;
   }

   CPoint pZero;
   pZero.Zero();
   if (pDelta->AreClose (&pZero))
      return; // no change

   // see if can find it
   if (m_plVERTDEFORM) {
      DWORD i, dwNum;
      PVERTDEFORM pv = (PVERTDEFORM) m_plVERTDEFORM->Get(0);
      dwNum = m_plVERTDEFORM->Num();
      for (i = 0; i < dwNum; i++) {
         if (pv->dwDeform != dwDeform)
            continue;   // no match

         // match
         pv->pDeform.Add (pDelta);
         if (pv->pDeform.AreClose (&pZero)) {
            m_plVERTDEFORM->Remove (i);
            if (!m_plVERTDEFORM->Num()) {
               delete m_plVERTDEFORM;
               m_plVERTDEFORM = NULL;
            }
         }
         return;
      } // i
   } // if verdeform

   // else, cant find, so add
   if (!m_plVERTDEFORM) {
      m_plVERTDEFORM = new CListFixed;
      if (m_plVERTDEFORM)
         m_plVERTDEFORM->Init (sizeof(VERTDEFORM));
   }
   if (m_plVERTDEFORM) {
      VERTDEFORM vd;
      memset (&vd, 0, sizeof(vd));
      vd.dwDeform = dwDeform;
      vd.pDeform.Copy (pDelta);
      m_plVERTDEFORM->Add (&vd);
   }
}


/**********************************************************************************
CPMVertOld::DeformCalc - Filled in the m_pLocWithDeform based on the current deformation.

inputs
   DWORD       dwNum - Number of active deformations
   DWORD       *padwDeform - Pointer to an array of deformations. This MUST be sorted in ascending order
   fp          *pafDeform - Pointer to an array of deformation amounts (from 0 to 1)
returns
   none
*/
void CPMVertOld::DeformCalc (DWORD dwNum, DWORD *padwDeform, fp *pafDeform)
{
   LocationGet (dwNum, padwDeform, pafDeform, &m_pLocWithDeform);
}


/**********************************************************************************
CPMVertOld::NormCalc - Calculates the normal for this vertex. DeformCalc() must
have been called for all vertecies this will use. ALSO, NormCalc() MUST have been
called for all the sides associated with this vertex.

inputs
   DWORD          dwIndex - Index ID for this vertex
   PCListFixed    plSide - Pointer to a CListFixed containing CPMSideOld objects. These
                  are searched through to find out which sides have vertecies in common
                  with this.
*/
void CPMVertOld::NormCalc (DWORD dwIndex, PCListFixed plSides)
{
   m_pNorm.Zero();

   DWORD dwNumS = plSides->Num();
   PCPMSideOld ps = (PCPMSideOld) plSides->Get (0);

   DWORD i, j;
   for (i = 0; i < dwNumS; i++) for (j = 0; j < ps[i].Num(); j++)
      if (ps[i].m_adwVert[j] == dwIndex)
         m_pNorm.Add (&ps[i].m_pNorm);

   // average
   m_pNorm.Normalize ();
}


/**********************************************************************************
CPMVertOld::Scale - Scales the vertex, including all normals.

inputs
   PCMatrix       pScale - Does scaling on the points
   PCMatrix       pScaleInvTrans - Inverse and thentransposed version of pScale. Used on vectors.
*/
void CPMVertOld::Scale (PCMatrix pScale, PCMatrix pScaleInvTrans)
{
   m_pLoc.p[3] = 1;
   m_pLoc.MultiplyLeft (pScale);
   m_pLocWithDeform.p[3] = 1;
   m_pLocWithDeform.MultiplyLeft (pScale);
   m_pNorm.p[3] = 1;
   m_pNorm.MultiplyLeft (pScaleInvTrans);

   // transform all the deformations
   if (m_plVERTDEFORM) {
      DWORD i, dwNum;
      PVERTDEFORM pv = (PVERTDEFORM) m_plVERTDEFORM->Get(0);
      dwNum = m_plVERTDEFORM->Num();
      for (i = 0; i < dwNum; i++) {
         pv[i].pDeform.p[3] =1;
         pv[i].pDeform.MultiplyLeft (pScaleInvTrans);
      }
   }

}





/**********************************************************************************
CPMSideOld::Zero - Frees any memory locations in the side which may point
to memory that would be release by Release()
*/
void CPMSideOld::Zero (void)
{
   // do nothing for now
}

/**********************************************************************************
CPMSideOld::Release - Frees any memory pointed to by the side
*/
void CPMSideOld::Release (void)
{
   // do nothing for now
}

/**********************************************************************************
CPMSideOld::CloneTo - COpies over data to pTo and allocates new copies of any data.
pTo is ASSUMED to be empty, with no pointers to memory that would be lost
when overwritten
*/
void CPMSideOld::CloneTo (CPMSideOld *pTo)
{
   memcpy (pTo->m_adwVert, m_adwVert, sizeof(m_adwVert));
   pTo->m_fHide = m_fHide;
   pTo->m_pNorm.Copy (&m_pNorm);
   pTo->m_dwOrigSide = m_dwOrigSide;
}

/**********************************************************************************
CPMSideOld::MMLTo - Writes the side to MML

returns
   PCMMLNode2 - Node
*/

static PWSTR gpszSide = L"Side";
static PWSTR gpszHide = L"Hide";

PCMMLNode2 CPMSideOld::MMLTo (void)
{
   PCMMLNode2 pNode = new CMMLNode2;
   if (!pNode)
      return NULL;

   pNode->NameSet (gpszSide);
   
   if (m_fHide)
      MMLValueSet (pNode, gpszHide, (int) m_fHide);

   // write vertices in order
   DWORD i;
   WCHAR szTemp[64];
   for (i = 0; i < Num(); i++) {
      swprintf (szTemp, L"V%d", (int) i);
      MMLValueSet (pNode, szTemp, (int) m_adwVert[i]);
   }
   return pNode;
}

/**********************************************************************************
CPMSideOld::MMLFrom - Reads the side from MML. Note: ASSUMES that current data is not
pointing to any memory.

inputs
   PCMMLNode2      pNode - MML node
returns
   BOOL - TRUE if success
*/
BOOL CPMSideOld::MMLFrom (PCMMLNode2 pNode)
{
   Zero();  // free up any memory

   // clear out
   m_pNorm.Zero();

   m_fHide = (BOOL) MMLValueGetInt (pNode, gpszHide, FALSE);

   // read vertices
   DWORD i;
   WCHAR szTemp[64];
   for (i = 0; i < 4; i++) {
      swprintf (szTemp, L"V%d", (int) i);
      m_adwVert[i] = (DWORD) MMLValueGetInt (pNode, szTemp, -1);
   }
   return TRUE;
}

/**********************************************************************************
CPMSideOld::VertRename - Informs the side when vertecies have been renamed (change
vert ID 5 to vert ID 28). The side changes its internal structures to reflect
this renaming.

inputs
   DWORD       dwNum - Number of sides
   DWORD       *padwOrig - Pointer to an array of DWORDs with original IDs.
                  This MUST be sorted in ascending order.
   DWORD       *padwNew - New values to replace those found in the original
                  The values CANNOT be -1.
returns
   none
*/
void CPMSideOld::VertRename (DWORD dwNum, DWORD *padwOrig, DWORD *padwNew)
{
   DWORD i, dwFind;
   for (i = 0; i < 4; i++) {
      if (m_adwVert[i] == -1)
         continue;

      dwFind = DWORDSearch (m_adwVert[i], dwNum, padwOrig);
      if (dwFind == -1)
         continue;   // not in list
      m_adwVert[i] = padwNew[dwFind];
   }
}


/**********************************************************************************
CPMSideOld::VertRemove - Called when a vertex has been removed from the list. If the
side refers to a vertex higher than the one removed its index will be decreased.

NOTE: DOesn't handle case when vertex matches. Shouldnt be called if this happens.

inputs
   DWORD dwVert
returns
   none
*/
void CPMSideOld::VertRemove (DWORD dwVert)
{
   DWORD i;
   for (i = 0; i < 4; i++) {
      if (m_adwVert[i] == -1)
         continue;

      if (m_adwVert[i] >= dwVert)
         m_adwVert[i]--;
   }
}


/**********************************************************************************
CPMSideOld::Num - Returns the number of points. Basically looks at the 4th point - if it's
-1 then have a triangle, else quad.
*/
DWORD CPMSideOld::Num (void)
{
   return (m_adwVert[3] == -1) ? 3 : 4;
}



/**********************************************************************************
CPMSideOld::Reverse - Reverses the order of the points in the polygon.  Clockwise to
counterclockwise, etc.
*/
void CPMSideOld::Reverse (void)
{
   DWORD dwNum = Num();
   DWORD i, dw;
   for (i = 0; i < dwNum/2; i++) {
      dw = m_adwVert[i];
      m_adwVert[i] = m_adwVert[dwNum - i - 1];
      m_adwVert[dwNum - i - 1] = dw;
   }
}


/**********************************************************************************
CPMSideOld::NormCalc - Calculates the normal for this side. DeformCalc() must
have been called for all vertecies this will use.

inputs
   PCListFixed    plVert - Pointer to a CListFixed containing CPMVertOld objects. These
                  are used to get the edges, and calculate the normal
*/
void CPMSideOld::NormCalc (PCListFixed plVert)
{
   DWORD dwNumV = plVert->Num();
   PCPMVertOld pVert = (PCPMVertOld) plVert->Get(0);

   m_pNorm.Zero();

   DWORD dwVert, i;
   dwVert = Num();
   for (i = 0; i < dwVert; i++)
      if (m_adwVert[i] >= dwNumV)
         return;  // can't calculate

   CPoint pA, pB, pN;
   for (i = 0; i < ((dwVert == 3) ? 1 : dwVert); i++) {
      // calculate normal for side
      // BUGFIX - Was m_pLoc. changed to m_pLocWithDeform
      pA.Subtract (&pVert[m_adwVert[(0+i)]].m_pLocWithDeform, &pVert[m_adwVert[(1+i)%dwVert]].m_pLocWithDeform);
      pB.Subtract (&pVert[m_adwVert[(2+i)%dwVert]].m_pLocWithDeform, &pVert[m_adwVert[(1+i)%dwVert]].m_pLocWithDeform);

      pN.CrossProd (&pA, &pB);
      pN.Normalize();
      m_pNorm.Add (&pN);
   }
   if (i > 1)
      m_pNorm.Normalize(); // average al these together
}


/**********************************************************************************
CObjectPolyMeshOld::Constructor and destructor */
CObjectPolyMeshOld::CObjectPolyMeshOld (PVOID pParams, POSINFO pInfo)
{
   m_dwRenderShow = RENDERSHOW_MISC;
   m_OSINFO = *pInfo;
   m_dwType = (DWORD) pParams;

   m_lCPMVert.Init (sizeof(CPMVertOld));
   m_lCPMSide.Init (sizeof(CPMSideOld));
   m_lPCOEAttrib.Init (sizeof(PCOEAttrib));
   m_lVertexGroup.Init (sizeof(DWORD));
   m_lVertexInGroup.Init (sizeof(DWORD));
   m_lSideInGroup.Init (sizeof(DWORD));
   m_fVertGroupDirty = TRUE;
   m_dwCurVertGroup = -1;  // since not initialized
   m_dwVertGroupDist = 5;
   m_dwVertSideGroupDist = 10;

   m_lMorphRemapID.Init (sizeof(DWORD));
   m_lMorphRemapValue.Init (sizeof(fp));

   m_dwDisplayControl = 1;    // BUGFIX: default to other settings, since initially want full control
   m_fCanBackface = TRUE;
   m_fMergeSmoothing = FALSE;
   m_dwSymmetry = 0;
   m_dwSubdivide = 1;   // default
   m_fDragSize = .001; // drag default, BUGFIX - Was .5, but once have smoothing, set to 0.001
   m_pLastMoved.Zero();
   m_pLastMoved.p[3] = -1; // indicator that havent moved yet

   // set up some parameters based on m_dwType
   m_dwDivisions = 8;
   m_apParam[0].Zero();
   m_apParam[1].Zero();
   m_apParam[0].p[0] = m_apParam[0].p[1] = m_apParam[0].p[2] = .5;
   switch (m_dwType) {
   case 3:  // cone, open on tpo
   case 4:  // cone, closed on top
      m_apParam[1].p[0] = 0;
      m_apParam[1].p[1] = -1;
      break;
   case 5:  // disc, also cone
      m_dwDivisions = 16;
      m_apParam[1].p[0] = 0;
      m_apParam[1].p[1] = 0;
      break;
   case 6:  // two cones
      m_apParam[1].p[0] = 1;
      m_apParam[1].p[1] = -1;
      break;
   case 0:  // sphere
   default:
      // no changes to default settings
      break;
   case 1:  // cylinder without cap
   case 2:  // cylinder with cap
      m_apParam[1].Copy (&m_apParam[0]);
      break;
   case 7:  // box
      m_dwDivisions = 3;
      m_dwSubdivide = 0;   // since if subdivide doesn't look like box
      break;
   case 8:  // plane
      m_dwDivisions = 5;
      break;
   case 9:  // taurus
      m_dwDivisions = 16;
      m_apParam[1].p[0] = m_apParam[1].p[1] = .25;
      break;
   case 10: // cylinder with rounded caps
      m_dwDivisions *= 2;
      m_apParam[0].p[3] = .5;
      m_apParam[1].Copy (&m_apParam[0]);
      break;
   }

   CreateBasedOnType();

   // color for the PolyMesh
   ObjectSurfaceAdd (42, RGB(0xff,0x40,0x40), MATERIAL_TILEGLAZED);
}


CObjectPolyMeshOld::~CObjectPolyMeshOld (void)
{
   ClearVertSide();
}


/**********************************************************************************
CObjectPolyMeshOld::ClearVertSide - Clear out the vertex and sides.

inputs
   BOOL     fOnlyDiv - If TRUE only clears the divisions and not the main vertex and sides
returns
   none
*/
void CObjectPolyMeshOld::ClearVertSide (BOOL fOnlyDiv)
{
   PCPMVertOld pv;
   PCPMSideOld ps;
   DWORD dwNum;
   DWORD i;
   if (!fOnlyDiv) {
      pv = (PCPMVertOld) m_lCPMVert.Get(0);
      ps = (PCPMSideOld) m_lCPMSide.Get(0);
      dwNum = m_lCPMVert.Num();
      for (i = 0; i < dwNum; i++)
         pv[i].Release();
      dwNum = m_lCPMSide.Num();
      for (i = 0; i < dwNum; i++)
         ps[i].Release();
      m_lCPMVert.Clear();
      m_lCPMSide.Clear();

      // clear out the morphs
      ObjEditClearPCOEAttribList (&m_lPCOEAttrib);
   }

   // and the divisions
   pv = (PCPMVertOld) m_lCPMVertDiv.Get(0);
   ps = (PCPMSideOld) m_lCPMSideDiv.Get(0);
   dwNum = m_lCPMVertDiv.Num();
   for (i = 0; i < dwNum; i++)
      pv[i].Release();
   dwNum = m_lCPMSideDiv.Num();
   for (i = 0; i < dwNum; i++)
      ps[i].Release();
   m_lCPMVertDiv.Clear();
   m_lCPMSideDiv.Clear();


   m_fDivDirty = TRUE;
   m_fVertGroupDirty = TRUE;
}


/**********************************************************************************
FindMatch - Given a PCListFixed of PMVERT, this finds the other vert that is close
to the point in question.

inputs
   PCListFixed    pl - List of PMVERT
   PCPoint        pPoint - Looking for a match for this
returns
   DWORD - Index into pl, or -1 if can't find
*/
static FindMatch (PCListFixed pl, PCPoint pPoint)
{
   PCPMVertOld pa = (PCPMVertOld) pl->Get(0);
   DWORD dwNum = pl->Num();

   DWORD i;
   for (i = 0; i < dwNum; i++)
      if (pPoint->AreClose (&pa[i].m_pLoc))
         return i;
   return -1;
}

/*************************************************************************************
FillInNormals - Go through a list of vertecies and sides and fill in the normal
for each vertex. (The vertecies must have m_pLocAfterDeform filled in)

inputs
   PCListFixed       plVert - List of CPMVertOld
   PCListFixed       plSide - List of CPMSideOld
returns
   none
*/
void FillInNormals (PCListFixed plVert, PCListFixed plSide)
{
   PCPMVertOld pv = (PCPMVertOld) plVert->Get(0);
   PCPMSideOld ps = (PCPMSideOld) plSide->Get(0);
   DWORD dwNumV = plVert->Num();
   DWORD dwNumS = plSide->Num();

   DWORD i,j;

   // loop through all the vertcies and zero out the normal
   for (i = 0; i < dwNumV; i++)
      pv[i].m_pNorm.Zero();

   // loop through all the triangles and calculate a normal
   for (i = 0; i < dwNumS; i++) {
      ps[i].NormCalc(plVert);

      // add this normal to all the vertecies it attaches to
      for (j = 0; j < ps[i].Num(); j++)
         pv[ps[i].m_adwVert[j]].m_pNorm.Add (&ps[i].m_pNorm);
   }

   // go back through all the normals and normalize
   for (i = 0; i < dwNumV; i++)
      pv[i].m_pNorm.Normalize();
}


/**********************************************************************************
CObjectPolyMeshOld::CalcMainNormals - Calculate the normals if they're not already
calculated.

NOTE: Deformations NOT included in this since used for SideAddDetail
*/
void CObjectPolyMeshOld::CalcMainNormals (void)
{
   PCPMVertOld pv = (PCPMVertOld) m_lCPMVert.Get(0);
   PCPMSideOld ps = (PCPMSideOld) m_lCPMSide.Get(0);
   DWORD dwNumV = m_lCPMVert.Num();
   DWORD dwNumS = m_lCPMSide.Num();

   // loop through and include deformations
   // NOTE: Do NOT include defomrations for this since used in SidesAddDetail
   DWORD i;
   for (i = 0; i < dwNumV; i++)
      pv[i].DeformCalc(0, NULL, NULL);

   FillInNormals (&m_lCPMVert, &m_lCPMSide);

#if 0 // old code
   // loop through all the triangles and calculate a normal
   for (i = 0; i < dwNumS; i++)
      ps[i].NormCalc(&m_lCPMVert);

   // loop through all the vertices and figure out which triangles
   // they're attached to. Once that's done, average the triangles' normals
   for (i = 0; i < dwNumV; i++)
      pv[i].NormCalc(i, &m_lCPMSide);
#endif // 0
}


/**********************************************************************************
CObjectPolyMeshOld::CreateBasedOnType - Fills in all the points based on the type
of object in m_dwType. (Fails if no divisions.)
*/
void CObjectPolyMeshOld::CreateBasedOnType (void)
{
   // Fill in points
   // BUGFIX - Fewer points
   switch (m_dwType) {
   case 0:  // sphere
   default:
      CreateSphere (m_dwDivisions * m_dwDivisions, &m_apParam[0]);
      break;
   case 1:  // cylinder without cap
      CreateCylinder (m_dwDivisions, &m_apParam[0], &m_apParam[1], FALSE);
      break;
   case 2:  // cylinder with cap
      CreateCylinder (m_dwDivisions, &m_apParam[0], &m_apParam[1], TRUE);
      break;
   case 3:  // cone, open on tpo
      CreateCone (m_dwDivisions, &m_apParam[0], FALSE, TRUE, m_apParam[1].p[0], m_apParam[1].p[1]);
      break;
   case 4:  // cone, closed on top
      CreateCone (m_dwDivisions, &m_apParam[0], TRUE, TRUE, m_apParam[1].p[0], m_apParam[1].p[1]);
      break;
   case 5:  // disc
      CreateCone (m_dwDivisions, &m_apParam[0], TRUE, FALSE, m_apParam[1].p[0], m_apParam[1].p[1]);
      break;
   case 6:  // two cones
      CreateCone (m_dwDivisions, &m_apParam[0], TRUE, TRUE, m_apParam[1].p[0], m_apParam[1].p[1]);
      break;
   case 7:  // box
      CreateBox (m_dwDivisions, &m_apParam[0]);
      break;
   case 8:  // plane
      CreatePlane (m_dwDivisions, &m_apParam[0]);
      break;
   case 9:  // taurus
      CreateTaurus (m_dwDivisions, &m_apParam[0], &m_apParam[1]);
      break;
   case 10:  // cylinder without rounded ends
      CreateCylinderRounded (m_dwDivisions, &m_apParam[0], &m_apParam[1]);
      break;
   }

   // calculate the scale
   CalcScale ();

   // do the textures
   switch (m_dwType) {
   case 0:  // sphere
   default:
   case 9:  // taurus
   case 10: // cylinder rounded
      TextureSpherical ();
      break;
   case 1:  // cylinder without cap
   case 2:  // cylinder with cap
   case 3:  // cone, open on tpo
   case 4:  // cone, closed on top
   case 6:  // two cones
      TextureCylindrical ();
      break;
   case 5:  // disc
   case 8:  // plane
      TextureLinear (FALSE);
      break;
   case 7:  // box
      TextureLinear (TRUE);
   }
}

/**********************************************************************************
CObjectPolyMeshOld::CreateSphere - Create a meash that's a sphere. This
erases all existing vertices and sides.

inputs
   DWORD    dwNum - Number of points, approximately, in the sphere
   PCPoint  pScale - Normally -1 to 1 sphere, but multiply bu this
returns
   none
*/
void CObjectPolyMeshOld::CreateSphere (DWORD dwNum, PCPoint pScale)
{
   // scale this down
   dwNum /= 8; // divide into 8 triangles
   dwNum *= 2; // make the triangles into squares
   dwNum = (DWORD) sqrt(dwNum);  // find the number up and down
   dwNum = max(1,dwNum);   // at least two points, for top and bottom

   // clear out old list
   CPMVertOld pv;
   CPMSideOld ps;
   memset (&pv, 0, sizeof(pv));
   memset (&ps, 0, sizeof(ps));
   ps.m_adwVert[3] = -1;   // since dealing with triangles
   ClearVertSide();
   m_fDivDirty = TRUE;   // set dirty flags

   // scratch memory for IDs
   CMem  mem;
   if (!mem.Required ((dwNum+1) * (dwNum+1) * sizeof(DWORD)))
      return;
   DWORD *padwScratch;
   padwScratch = (DWORD*) mem.p;

   // create 8 triangles
   DWORD dwBottom, dwSide;
   for (dwBottom = 0; dwBottom < 2; dwBottom++) {
      for (dwSide = 0; dwSide < 4; dwSide++) {

         // in each triangle, calculate the points...
         DWORD x, y;
         for (x = 0; x <= dwNum; x++) for (y = 0; x+y <= dwNum; y++) {
            fp fLat, fLong;
            if (y < dwNum)
               fLong = ((fp)x / (fp)(dwNum-y) + (fp)dwSide) * PI/2;
            else
               fLong = 0;  // at north/south pole
            fLat = (fp) y / (fp)dwNum * PI/2;

            // if in the southern hemispher then negative latitiude and reverse longitude
            if (dwBottom) {
               fLat *= -1;
               fLong *= -1;
            }

            // calculate the point
            CPoint pLoc;
            pLoc.Zero();
            pLoc.p[0] = cos(fLong) * cos(fLat) * pScale->p[0];
            pLoc.p[1] = sin(fLong) * cos(fLat) * pScale->p[1];
            pLoc.p[2] = sin(fLat) * pScale->p[2];

            // find out which point it's nearest
            DWORD dwNear;
            dwNear = -1;
            // if it's on an edge then may match up elsewhere
            if ((x == 0) || (y == 0) || (x+y == dwNum))
               dwNear = FindMatch (&m_lCPMVert, &pLoc);
            if (dwNear == -1) {
               // not near any, so create it
               dwNear = m_lCPMVert.Num();
               pv.m_pLoc.Copy (&pLoc);
               m_lCPMVert.Add (&pv);
            }
            padwScratch[x + (y * (dwNum+1))] = dwNear;

         }  // x and y


         // now, create all the triangles
         for (x = 0; x < dwNum; x++) for (y = 0; x+y < dwNum; y++) {
            // first one
            ps.m_adwVert[0] = padwScratch[(x+1) + (y * (dwNum+1))];
            ps.m_adwVert[1] = padwScratch[x + (y * (dwNum+1))];
            ps.m_adwVert[2] = padwScratch[x + ((y+1) * (dwNum+1))];
            m_lCPMSide.Add (&ps);

            // might be another side
            if (x+y+2 > dwNum)
               continue;
            ps.m_adwVert[0] = padwScratch[(x+1) + (y * (dwNum+1))];
            ps.m_adwVert[1] = padwScratch[x + ((y+1) * (dwNum+1))];
            ps.m_adwVert[2] = padwScratch[(x+1) + ((y+1) * (dwNum+1))];
            m_lCPMSide.Add (&ps);

         }  // over x and y
      }  // over dwSide
   }  // over dwBottom

   m_fCanBackface = TRUE;
}



/**********************************************************************************
CObjectPolyMeshOld::CreateCone - Create a meash that's a sphere. This
erases all existing vertices and sides.

inputs
   DWORD    dwNum - Number of points around the diameter
   PCPoint   pScale - x and y Scale around diameter. Normally -1 to 1 sphere, but multiply bu this
   BOOL     fShowTop, fShowBottom - If TRUE, show the top/bottom.
   fp       fTop, fBottom - Height of the top and bottom
returns
   none
*/
void CObjectPolyMeshOld::CreateCone (DWORD dwNum, PCPoint pScale, BOOL fShowTop, BOOL fShowBottom,
                                  fp fTop, fp fBottom)
{
   // scale this down
   dwNum /= 4;
   dwNum = max(1,dwNum);   // at least two points, for top and bottom

   // extra boundary pieces at top and two at bottom so that get nice normals
   BOOL fExtraBottom, fExtraTop;
   fExtraBottom = (fShowTop && fShowBottom); // so will merge better
   fExtraTop = (fShowTop && (fTop != 0)) || (fShowBottom && (fBottom != 0));
   if (fExtraBottom)
      dwNum += 1;
   if (fExtraTop)
      dwNum += 1;

   // clear out old list
   CPMVertOld pv;
   CPMSideOld ps;
   memset (&pv, 0, sizeof(pv));
   memset (&ps, 0, sizeof(ps));
   ps.m_adwVert[3] = -1;   // since dealing with triangle
   ClearVertSide();
   m_fDivDirty = TRUE;   // set dirty flags

   // scratch memory for IDs
   CMem  mem;
   if (!mem.Required ((dwNum+1) * (dwNum+1) * sizeof(DWORD)))
      return;
   DWORD *padwScratch;
   padwScratch = (DWORD*) mem.p;

   // create 8 triangles
   DWORD dwBottom, dwSide;
   for (dwBottom = 0; dwBottom < 2; dwBottom++) {
      // show top and bottom
      if (dwBottom && !fShowBottom)
         continue;
      if (!dwBottom && !fShowTop)
         continue;

      for (dwSide = 0; dwSide < 4; dwSide++) {

         // in each triangle, calculate the points...
         DWORD x, y;
         for (x = 0; x <= dwNum; x++) for (y = 0; x+y <= dwNum; y++) {
            fp fLat, fLong, fHeight;
            if (y < dwNum)
               fLong = ((fp)x / (fp)(dwNum-y) + (fp)dwSide) * PI/2;
            else
               fLong = 0;  // at north/south pole

            if (fExtraBottom) {
               fLat = 1.0 - ((fp) y-(fp)1) / (fp)(dwNum - (fExtraTop ? 2 : 1));
               fLat = min(1, fLat);
               fLat = max(0, fLat);
               if (y == 1)
                  fLat *= .99;   // extra bit
               else if (fExtraTop && (y == dwNum-1))
                  fLat = .01;   // extra bit
            }
            else if (fExtraTop) {   // but not extra bottom
               fLat = 1.0 - (fp) y / (fp)(dwNum - 1);
               fLat = min(1, fLat);
               fLat = max(0, fLat);
               if (y == dwNum-1)
                  fLat = .01;   // extra bit
            }
            else {
               fLat = 1.0 - (fp) y / (fp)dwNum;
            }
            fHeight = (dwBottom ? fBottom : fTop) * (1.0 - fLat);

            // if in the southern hemispher then negative latitiude and reverse longitude
            if (dwBottom) {
               fLat *= -1;
               fLong *= -1;
            }

            // calculate the point
            CPoint pLoc;
            pLoc.Zero();
            pLoc.p[0] = cos(fLong) * fLat * pScale->p[0];
            pLoc.p[1] = sin(fLong) * fLat * pScale->p[1];
            pLoc.p[2] = fHeight;

            // find out which point it's nearest
            DWORD dwNear;
            dwNear = -1;
            // if it's on an edge then may match up elsewhere
            if ((x == 0) || (y == 0) || (x+y == dwNum))
               dwNear = FindMatch (&m_lCPMVert, &pLoc);
            if (dwNear == -1) {
               // not near any, so create it
               dwNear = m_lCPMVert.Num();
               pv.m_pLoc.Copy (&pLoc);
               m_lCPMVert.Add (&pv);
            }
            padwScratch[x + (y * (dwNum+1))] = dwNear;

         }  // x and y


         // now, create all the triangles
         for (x = 0; x < dwNum; x++) for (y = 0; x+y < dwNum; y++) {
            // first one
            ps.m_adwVert[0] = padwScratch[(x+1) + (y * (dwNum+1))];
            ps.m_adwVert[1] = padwScratch[x + (y * (dwNum+1))];
            ps.m_adwVert[2] = padwScratch[x + ((y+1) * (dwNum+1))];
            m_lCPMSide.Add (&ps);

            // might be another side
            if (x+y+2 > dwNum)
               continue;
            ps.m_adwVert[0] = padwScratch[(x+1) + (y * (dwNum+1))];
            ps.m_adwVert[1] = padwScratch[x + ((y+1) * (dwNum+1))];
            ps.m_adwVert[2] = padwScratch[(x+1) + ((y+1) * (dwNum+1))];
            m_lCPMSide.Add (&ps);

         }  // over x and y
      }  // over dwSide
   }  // over dwBottom

   m_fCanBackface = fShowTop && fShowBottom;
}



/**********************************************************************************
CObjectPolyMeshOld::CreateCylinder - Create a cylinder without any caps

inputs
   DWORD    dwNum - Number of points, approximately, around the radius of the cylinder.
   PCPoint  pScale - Normally -1 to 1 sphere, but multiply bu this
   PCPoint  pScaleNegative - Scale in the negative end. Only x and y are used
   BOOL     fCap - if TRUE, draw caps on the end, else not
returns
   none
*/
void CObjectPolyMeshOld::CreateCylinder (DWORD dwNum, PCPoint pScale, PCPoint pScaleNegative, BOOL fCap)
{
   dwNum = max(1,dwNum);

   // number of points over length
   DWORD dwNumV;
   fp fAvgRadius, fAvgGridSize;
   fAvgRadius = (pScale->p[0] + pScale->p[1] + pScaleNegative->p[0] + pScaleNegative->p[1]) / 4;
   fAvgGridSize = fAvgRadius * 2 * PI / (fp) dwNum;
   dwNumV = (DWORD) (pScale->p[2] * 3 / fAvgGridSize);   // BUGFIX - Was *2, but not enough divisions
   dwNumV = max(2, dwNumV);
   if (fCap)
      dwNumV += 4;   // since need to extra rows on each end

   // scale this down
   dwNum /= 4; // divide into 4 sections around
   dwNum *= 4;
   dwNum = max(4,dwNum);

   // clear out old list
   CPMVertOld pv;
   CPMSideOld ps;
   memset (&pv, 0, sizeof(pv));
   memset (&ps, 0, sizeof(ps));
   ps.m_adwVert[3] = -1;   //since dealing with triangles
   ClearVertSide();
   m_fDivDirty = TRUE;   // set dirty flags

   // create loop
   // in each triangle, calculate the points...
   DWORD x, y;
   for (y = 0; y < dwNumV; y++) for (x = 0; x < dwNum; x++) {
      fp fLong;
      fLong = (fp)x / (fp)dwNum * 2 * PI;

      // calculate the point
      CPoint pLoc;
      pLoc.Zero();
      pLoc.p[0] = cos(fLong);
      pLoc.p[1] = sin(fLong);
      if (fCap) {
         pLoc.p[2] = (((fp) y-2.0) / (fp) (dwNumV-1-4) * 2.0 - 1.0);
         pLoc.p[2] = min(1, pLoc.p[2]);
         pLoc.p[2] = max(-1, pLoc.p[2]);
         if ((y >= 2) && (y < dwNumV-2))
            pLoc.p[2] *= .99; // so dont go all the way to the top

         if ((y == 0) || (y == dwNumV-1)) {
            // bend the top in
            pLoc.p[0] *= .99;
            pLoc.p[1] *= .99;
         }
      }
      else {
         pLoc.p[2] = ((fp) y / (fp) (dwNumV-1) * 2.0 - 1.0);
      }

      // scale
      CPoint pTempScale;
      pTempScale.Average (pScale, pScaleNegative, (pLoc.p[2] + 1) /2.0);
      pLoc.p[0] *= pTempScale.p[0];
      pLoc.p[1] *= pTempScale.p[1];
      pLoc.p[2] *= pScale->p[2];

      pv.m_pLoc.Copy (&pLoc);
      m_lCPMVert.Add (&pv);
   }

   // now, create all the triangles
   for (y = 0; y+1 < dwNumV; y++) for (x = 0; x < dwNum; x++) {
      // BUGFIX - Make one quad out of it
      // first one
      ps.m_adwVert[0] = ((x+1)%dwNum) + y * dwNum;
      ps.m_adwVert[1] = x + y * dwNum;
      ps.m_adwVert[2] = x + (y+1) * dwNum;
      ps.m_adwVert[3] = ((x+1)%dwNum) + (y+1) * dwNum;
      m_lCPMSide.Add (&ps);
   }  // over x and y
   ps.m_adwVert[3] = -1;


   // make the top and bottom caps
   if (fCap) {
      dwNum /= 4;

      // scratch memory for IDs
      CMem  mem;
      if (!mem.Required ((dwNum+1) * (dwNum+1) * sizeof(DWORD)))
         return;
      DWORD *padwScratch;
      padwScratch = (DWORD*) mem.p;

      // create 8 triangles
      DWORD dwBottom, dwSide;
      for (dwBottom = 0; dwBottom < 2; dwBottom++) {
         for (dwSide = 0; dwSide < 4; dwSide++) {

            // in each triangle, calculate the points...
            DWORD x, y;
            for (x = 0; x <= dwNum; x++) for (y = 0; x+y <= dwNum; y++) {
               fp fLat, fLong;
               if (y < dwNum)
                  fLong = ((fp)x / (fp)(dwNum-y) + (fp)dwSide) * PI/2;
               else
                  fLong = 0;  // at north/south pole
               fLat = 1.0 - (fp) y / (fp)dwNum;

               // if in the southern hemispher then negative latitiude and reverse longitude
               if (dwBottom) {
                  fLat *= -1;
                  fLong *= -1;
               }

               // calculate the point
               CPoint pLoc;
               pLoc.Zero();
               pLoc.p[0] = cos(fLong) * fLat * (dwBottom ? pScaleNegative : pScale)->p[0] * .99;
               pLoc.p[1] = sin(fLong) * fLat * (dwBottom ? pScaleNegative : pScale)->p[1] * .99;
               pLoc.p[2] = (dwBottom ? -1 : 1) * pScale->p[2];

               // find out which point it's nearest
               DWORD dwNear;
               dwNear = -1;
               // if it's on an edge then may match up elsewhere
               if ((x == 0) || (y == 0) || (x+y == dwNum))
                  dwNear = FindMatch (&m_lCPMVert, &pLoc);
               if (dwNear == -1) {
                  // not near any, so create it
                  dwNear = m_lCPMVert.Num();
                  pv.m_pLoc.Copy (&pLoc);
                  m_lCPMVert.Add (&pv);
               }
               padwScratch[x + (y * (dwNum+1))] = dwNear;

            }  // x and y


            // now, create all the triangles
            for (x = 0; x < dwNum; x++) for (y = 0; x+y < dwNum; y++) {
               // first one
               ps.m_adwVert[0] = padwScratch[(x+1) + (y * (dwNum+1))];
               ps.m_adwVert[1] = padwScratch[x + (y * (dwNum+1))];
               ps.m_adwVert[2] = padwScratch[x + ((y+1) * (dwNum+1))];
               m_lCPMSide.Add (&ps);

               // might be another side
               if (x+y+2 > dwNum)
                  continue;
               ps.m_adwVert[0] = padwScratch[(x+1) + (y * (dwNum+1))];
               ps.m_adwVert[1] = padwScratch[x + ((y+1) * (dwNum+1))];
               ps.m_adwVert[2] = padwScratch[(x+1) + ((y+1) * (dwNum+1))];
               m_lCPMSide.Add (&ps);

            }  // over x and y
         }  // over dwSide
      }  // over dwBottom

   }

   m_fCanBackface = fCap;
}



/**********************************************************************************
CObjectPolyMeshOld::CreateCylinderRounded - Create a cylinder with rounded (spherical)
top and bottom.

inputs
   DWORD    dwNum - Number of points, approximately, around the radius of the cylinder.
   PCPoint  pScale - Normally -1 to 1 sphere, but multiply bu this. w is the size of the cylinder
   PCPoint  pScaleNegative - Scale in the negative end.
returns
   none
*/
void CObjectPolyMeshOld::CreateCylinderRounded (DWORD dwNum, PCPoint pScale, PCPoint pScaleNegative)
{
   dwNum = max(1,dwNum);

   // number of points over length
   DWORD dwNumV;
   fp fAvgRadius, fAvgGridSize;
   fAvgRadius = (pScale->p[0] + pScale->p[1] + pScaleNegative->p[0] + pScaleNegative->p[1]) / 4;
   fAvgGridSize = fAvgRadius * 2 * PI / (fp) dwNum;
   dwNumV = (DWORD) (pScale->p[3] * 2 / fAvgGridSize);
   dwNumV = max(2, dwNumV);

   // scale this down
   dwNum /= 4; // divide into 4 sections around
   dwNum *= 4;
   dwNum = max(4,dwNum);

   // clear out old list
   CPMVertOld pv;
   CPMSideOld ps;
   memset (&pv, 0, sizeof(pv));
   memset (&ps, 0, sizeof(ps));
   ps.m_adwVert[3] = -1;   //since dealing with triangles
   ClearVertSide();
   m_fDivDirty = TRUE;   // set dirty flags

   // create loop
   // in each triangle, calculate the points...
   DWORD x, y;
   for (y = 0; y < dwNumV; y++) for (x = 0; x < dwNum; x++) {
      fp fLong;
      fLong = (fp)x / (fp)dwNum * 2 * PI;

      // calculate the point
      CPoint pLoc;
      pLoc.Zero();
      pLoc.p[0] = cos(fLong);
      pLoc.p[1] = sin(fLong);
      pLoc.p[2] = ((fp) y / (fp) (dwNumV-1) * 2.0 - 1.0);

      // scale
      CPoint pTempScale;
      pTempScale.Average (pScale, pScaleNegative, (pLoc.p[2] + 1) /2.0);
      pLoc.p[0] *= pTempScale.p[0];
      pLoc.p[1] *= pTempScale.p[1];
      pLoc.p[2] *= pScale->p[3];

      pv.m_pLoc.Copy (&pLoc);
      m_lCPMVert.Add (&pv);
   }

   // now, create all the triangles
   for (y = 0; y+1 < dwNumV; y++) for (x = 0; x < dwNum; x++) {
      // BUGFIX - Make one quad out of it
      // first one
      ps.m_adwVert[0] = ((x+1)%dwNum) + y * dwNum;
      ps.m_adwVert[1] = x + y * dwNum;
      ps.m_adwVert[2] = x + (y+1) * dwNum;
      ps.m_adwVert[3] = ((x+1)%dwNum) + (y+1) * dwNum;
      m_lCPMSide.Add (&ps);
   }  // over x and y
   ps.m_adwVert[3] = -1;


   // make the top and bottom caps
   dwNum /= 4;

   // scratch memory for IDs
   CMem  mem;
   if (!mem.Required ((dwNum+1) * (dwNum+1) * sizeof(DWORD)))
      return;
   DWORD *padwScratch;
   padwScratch = (DWORD*) mem.p;

   // create 8 triangles
   DWORD dwBottom, dwSide;
   for (dwBottom = 0; dwBottom < 2; dwBottom++) {
      for (dwSide = 0; dwSide < 4; dwSide++) {

         // in each triangle, calculate the points...
         DWORD x, y;
         for (x = 0; x <= dwNum; x++) for (y = 0; x+y <= dwNum; y++) {
            fp fLat, fLong;
            if (y < dwNum)
               fLong = ((fp)x / (fp)(dwNum-y) + (fp)dwSide) * PI/2;
            else
               fLong = 0;  // at north/south pole
            fLat = (fp) y / (fp)dwNum * PI/2;

            // if in the southern hemispher then negative latitiude and reverse longitude
            if (dwBottom) {
               fLat *= -1;
               fLong *= -1;
            }

            // calculate the point
            CPoint pLoc;
            pLoc.Zero();
            pLoc.p[0] = cos(fLong) * cos(fLat) * (dwBottom ? pScaleNegative : pScale)->p[0];
            pLoc.p[1] = sin(fLong) * cos(fLat) * (dwBottom ? pScaleNegative : pScale)->p[1];
            pLoc.p[2] = sin(fLat) * (dwBottom ? pScaleNegative : pScale)->p[2] + (dwBottom ? -1 : 1) * pScale->p[3];

            // find out which point it's nearest
            DWORD dwNear;
            dwNear = -1;
            // if it's on an edge then may match up elsewhere
            if ((x == 0) || (y == 0) || (x+y == dwNum))
               dwNear = FindMatch (&m_lCPMVert, &pLoc);
            if (dwNear == -1) {
               // not near any, so create it
               dwNear = m_lCPMVert.Num();
               pv.m_pLoc.Copy (&pLoc);
               m_lCPMVert.Add (&pv);
            }
            padwScratch[x + (y * (dwNum+1))] = dwNear;

         }  // x and y


         // now, create all the triangles
         for (x = 0; x < dwNum; x++) for (y = 0; x+y < dwNum; y++) {
            // first one
            ps.m_adwVert[0] = padwScratch[(x+1) + (y * (dwNum+1))];
            ps.m_adwVert[1] = padwScratch[x + (y * (dwNum+1))];
            ps.m_adwVert[2] = padwScratch[x + ((y+1) * (dwNum+1))];
            m_lCPMSide.Add (&ps);

            // might be another side
            if (x+y+2 > dwNum)
               continue;
            ps.m_adwVert[0] = padwScratch[(x+1) + (y * (dwNum+1))];
            ps.m_adwVert[1] = padwScratch[x + ((y+1) * (dwNum+1))];
            ps.m_adwVert[2] = padwScratch[(x+1) + ((y+1) * (dwNum+1))];
            m_lCPMSide.Add (&ps);

         }  // over x and y
      }  // over dwSide
   }  // over dwBottom

   m_fCanBackface = TRUE;
}




/**********************************************************************************
CObjectPolyMeshOld::CreateTaurus - Taurus shape

inputs
   DWORD    dwNum - Number of points, approximately, around the radius of the cylinder.
   PCPoint  pScale - x and y are used for the whole taurus
   PCPoint  pInnterScale - x and y are used for the radius of the extrusion part
returns
   none
*/
void CObjectPolyMeshOld::CreateTaurus (DWORD dwNum, PCPoint pScale, PCPoint pInnerScale)
{
   // scale this down
   dwNum /= 4; // divide into 4 sections around
   dwNum *= 4;
   dwNum = max(8,dwNum);

   // number of points over length
   DWORD dwNumV;
   dwNumV = dwNum / 2;
   dwNumV = max(2, dwNumV);

   // clear out old list
   CPMVertOld pv;
   CPMSideOld ps;
   memset (&pv, 0, sizeof(pv));
   memset (&ps, 0, sizeof(ps));
   ps.m_adwVert[3] = -1;   //since dealing with triangles
   ClearVertSide();
   m_fDivDirty = TRUE;   // set dirty flags

   // create loop
   // in each triangle, calculate the points...
   DWORD x, y;
   for (y = 0; y < dwNumV; y++) for (x = 0; x < dwNum; x++) {
      fp fLong, fLat;
      fLong = (fp)x / (fp)dwNum * 2 * PI;
      fLat = ((fp)y / (fp)dwNumV - .5) * 2 * PI;

      // calculate the point
      CPoint pLoc;
      pLoc.Zero();
      pLoc.p[0] = cos(fLong) * (1 + cos (fLat) * pInnerScale->p[0] / pScale->p[0]) * pScale->p[0];
      pLoc.p[1] = sin(fLong) * (1 + cos (fLat) * pInnerScale->p[0] / pScale->p[1]) * pScale->p[1];
      pLoc.p[2] = sin(fLat) * pInnerScale->p[1];

      pv.m_pLoc.Copy (&pLoc);
      m_lCPMVert.Add (&pv);
   }

   // now, create all the triangles
   for (y = 0; y < dwNumV; y++) for (x = 0; x < dwNum; x++) {
      // BUGFIX - Only one quad
      // first one
      ps.m_adwVert[0] = ((x+1)%dwNum) + y * dwNum;
      ps.m_adwVert[1] = x + y * dwNum;
      ps.m_adwVert[2] = x + ((y+1)%dwNumV) * dwNum;
      ps.m_adwVert[3] = ((x+1)%dwNum) + ((y+1)%dwNumV) * dwNum;
      m_lCPMSide.Add (&ps);
   }  // over x and y


   m_fCanBackface = FALSE;
}


/**********************************************************************************
CObjectPolyMeshOld::CreateBox - Create a box

inputs
   DWORD    dwNum - Number of points across on each side
   PCPoint  pScale - Normally -1 to 1 sphere, but multiply bu this
returns
   none
*/
void CObjectPolyMeshOld::CreateBox (DWORD dwNum, PCPoint pScale)
{
   dwNum++;
   dwNum = max(2,dwNum);   // bugfix

   // clear out old list
   CPMVertOld pv;
   CPMSideOld ps;
   memset (&pv, 0, sizeof(pv));
   memset (&ps, 0, sizeof(ps));
   ps.m_adwVert[3] = -1;   //since dealing with triangles
   ClearVertSide();
   m_fDivDirty = TRUE;   // set dirty flags

   // number per side due to edges
   DWORD dwSide;
   dwSide = dwNum + 1;

   // create loop
   // in each triangle, calculate the points...
   DWORD x, y;
   for (y = 0; y < dwNum + 4; y++) for (x = 0; x < dwSide * 4; x++) {
      // distance along it's length
      fp fDist;
      DWORD dwDist = x % dwSide;
      if (dwDist == dwSide-1)
         fDist = 1;
      else
         fDist = ((fp) dwDist / (fp) (dwNum-1) * 2.0 - 1) * .99;

      // calculate the point
      CPoint pLoc;
      pLoc.Zero();

      switch (x / dwSide) {
      case 0:  // front
         pLoc.p[0] = fDist;
         pLoc.p[1] = -1;
         break;
      case 1:  // right
         pLoc.p[1] = fDist;
         pLoc.p[0] = 1;
         break;
      case 2:  // back
         pLoc.p[0] = -fDist;
         pLoc.p[1] = 1;
         break;
      case 3:  // left
         pLoc.p[1] = -fDist;
         pLoc.p[0] = -1;
         break;
      }

      // how high in Y
      pLoc.p[2] = ((fp)y - 2.0) / (fp)(dwNum-1) * 2.0 - 1;
      pLoc.p[2] = max(-1, pLoc.p[2]);
      pLoc.p[2] = min(1, pLoc.p[2]);
      if ((y == 0) || (y == dwNum+3)) {
         pLoc.p[0] *= .99;
         pLoc.p[1] *= .99;
      }
      if ((y == 2) || (y == dwNum+1))
         pLoc.p[2] *= .99;

      pLoc.p[0] *= pScale->p[0];
      pLoc.p[1] *= pScale->p[1];
      pLoc.p[2] *= pScale->p[2];

      pv.m_pLoc.Copy (&pLoc);
      m_lCPMVert.Add (&pv);
   }

   // now, create all the triangles
   DWORD dwAll;
   dwAll = dwSide * 4;
   for (y = 0; y+1 < dwNum+4; y++) for (x = 0; x < dwAll; x++) {
      // BUGFIX - Make only one box
      // first one
      ps.m_adwVert[0] = ((x+1)%dwAll) + y * dwAll;
      ps.m_adwVert[1] = x + y * dwAll;
      ps.m_adwVert[2] = x + (y+1) * dwAll;
      ps.m_adwVert[3] = ((x+1)%dwAll) + (y+1) * dwAll;
      m_lCPMSide.Add (&ps);
   }  // over x and y


   // scratch memory for IDs
   CMem  mem;
   if (!mem.Required ((dwNum+1) * (dwNum+1) * sizeof(DWORD)))
      return;
   DWORD *padwScratch;
   padwScratch = (DWORD*) mem.p;

   // create 8 triangles
   DWORD dwBottom;
   dwSide = dwNum - 1;
   for (dwBottom = 0; dwBottom < 2; dwBottom++) {
      // in each triangle, calculate the points...
      for (x = 0; x <= dwSide; x++) for (y = 0; y <= dwSide; y++) {
         // calculate the point
         CPoint pLoc;
         pLoc.Zero();
         pLoc.p[0] = ((fp) x / (fp) dwSide * 2 - 1) * pScale->p[0] * .99;
         pLoc.p[1] = ((fp) y / (fp) dwSide * 2 - 1) * pScale->p[1] * .99;
         pLoc.p[2] = pScale->p[2];

         if (dwBottom) {
            pLoc.p[0] *= -1;
            pLoc.p[2] *= -1;
         }

         // find out which point it's nearest
         DWORD dwNear;
         dwNear = -1;
         // if it's on an edge then may match up elsewhere
         if ((x == 0) || (y == 0) || (x+y == dwSide))
            dwNear = FindMatch (&m_lCPMVert, &pLoc);
         if (dwNear == -1) {
            // not near any, so create it
            dwNear = m_lCPMVert.Num();
            pv.m_pLoc.Copy (&pLoc);
            m_lCPMVert.Add (&pv);
         }
         padwScratch[x + (y * (dwSide+1))] = dwNear;

      }  // x and y


      // now, create all the triangles
      for (x = 0; x < dwSide; x++) for (y = 0; y < dwSide; y++) {
         // BUGFIX - ONly one quad
         ps.m_adwVert[0] = padwScratch[(x+1) + (y * (dwSide+1))];
         ps.m_adwVert[1] = padwScratch[x + (y * (dwSide+1))];
         ps.m_adwVert[2] = padwScratch[x + ((y+1) * (dwSide+1))];
         ps.m_adwVert[3] = padwScratch[(x+1) + ((y+1) * (dwSide+1))];
         m_lCPMSide.Add (&ps);
      }  // over x and y
   }  // over dwBottom


   m_fCanBackface = TRUE;
}



/**********************************************************************************
CObjectPolyMeshOld::CreatePlane - Create a plane

inputs
   DWORD    dwNum - Number of points across on each side
   PCPoint  pScale - Normally -1 to 1 sphere, but multiply bu this
returns
   none
*/
void CObjectPolyMeshOld::CreatePlane (DWORD dwNum, PCPoint pScale)
{
   dwNum = max(2,dwNum);   // BUGFIX
   // clear out old list
   CPMVertOld pv;
   CPMSideOld ps;
   memset (&pv, 0, sizeof(pv));
   memset (&ps, 0, sizeof(ps));
   ps.m_adwVert[3] = -1;   //since dealing with triangles
   ClearVertSide();
   m_fDivDirty = TRUE;   // set dirty flags

   // number per side due to edges
   DWORD dwSide;
   DWORD x, y;
   // scratch memory for IDs
   CMem  mem;
   if (!mem.Required ((dwNum+1) * (dwNum+1) * sizeof(DWORD)))
      return;
   DWORD *padwScratch;
   padwScratch = (DWORD*) mem.p;

   // create 8 triangles
   dwSide = dwNum - 1;
   // in each triangle, calculate the points...
   for (x = 0; x <= dwSide; x++) for (y = 0; y <= dwSide; y++) {
      // calculate the point
      CPoint pLoc;
      pLoc.Zero();
      pLoc.p[0] = ((fp) x / (fp) dwSide * 2 - 1) * pScale->p[0];
      pLoc.p[1] = ((fp) y / (fp) dwSide * 2 - 1) * pScale->p[1];
      pLoc.p[2] = 0;

      // find out which point it's nearest
      DWORD dwNear;
      // not near any, so create it
      dwNear = m_lCPMVert.Num();
      pv.m_pLoc.Copy (&pLoc);
      m_lCPMVert.Add (&pv);
      padwScratch[x + (y * (dwSide+1))] = dwNear;

   }  // x and y


   // now, create all the triangles
   for (x = 0; x < dwSide; x++) for (y = 0; y < dwSide; y++) {
      // BUGFIX - ONly one quad
      ps.m_adwVert[0] = padwScratch[(x+1) + (y * (dwSide+1))];
      ps.m_adwVert[1] = padwScratch[x + (y * (dwSide+1))];
      ps.m_adwVert[2] = padwScratch[x + ((y+1) * (dwSide+1))];
      ps.m_adwVert[3] = padwScratch[(x+1) + ((y+1) * (dwSide+1))];
      m_lCPMSide.Add (&ps);
   }  // over x and y


   m_fCanBackface = FALSE;
}

/**********************************************************************************
CObjectPolyMeshOld::Delete - Called to delete this object
*/
void CObjectPolyMeshOld::Delete (void)
{
   delete this;
}



/**********************************************************************************
CObjectPolyMeshOld::Render

inputs
   POBJECTRENDER     pr - Render information
*/
void CObjectPolyMeshOld::Render (POBJECTRENDER pr)
{
   PCPMVertOld pv;
   PCPMSideOld ps;
   DWORD dwNumV, dwNumS;

   SubdivideIfNecessary ();

   // pick appropriate database
   pv = (PCPMVertOld) (m_dwSubdivide ? m_lCPMVertDiv.Get(0) : m_lCPMVert.Get(0));
   ps = (PCPMSideOld) (m_dwSubdivide ? m_lCPMSideDiv.Get(0) : m_lCPMSide.Get(0));
   dwNumV = m_dwSubdivide ? m_lCPMVertDiv.Num() : m_lCPMVert.Num();
   dwNumS = m_dwSubdivide ? m_lCPMSideDiv.Num() : m_lCPMSide.Num();

   // create the surface render object and draw
   CRenderSurface rs;
   CMatrix mObject;
   rs.Init (pr->pRS);
   ObjectMatrixGet (&mObject);
   rs.Multiply (&mObject);

   // object specific
   rs.SetDefMaterial (ObjectSurfaceFind (42), m_pWorld);

   // create all the points
   PCPoint pPoints;
   DWORD dwPointIndex;
   pPoints = rs.NewPoints (&dwPointIndex, dwNumV);
   if (!pPoints)
      return;
   DWORD i;
   for (i = 0; i < dwNumV; i++)
      pPoints[i].Copy (&pv[i].m_pLocWithDeform);

   // create all the noramls
   PCPoint pNormals;
   DWORD dwNormalsIndex;
   pNormals = rs.NewNormals (TRUE, &dwNormalsIndex, dwNumV);
   if (pNormals) {
      for (i = 0; i < dwNumV; i++)
         pNormals[i].Copy (&pv[i].m_pNorm);
   }

   // create all the textures
   PTEXTUREPOINT pText;
   DWORD dwTextIndex;
   pText = rs.NewTextures (&dwTextIndex, dwNumV);
   if (pText) {
      for (i = 0; i < dwNumV; i++)
         pText[i] = pv[i].m_tText;
      rs.ApplyTextureRotation (pText, dwNumV);
   }

   // create all the vertices
   PVERTEX pVert;
   DWORD dwVertIndex;
   pVert = rs.NewVertices (&dwVertIndex, dwNumV);
   if (!pVert)
      return;
   for (i = 0; i < dwNumV; i++) {
      pVert[i].dwNormal = dwNormalsIndex + (pNormals ? i : 0);
      pVert[i].dwPoint = dwPointIndex + i;
      pVert[i].dwTexture = dwTextIndex + (pText ? i : 0);
   }

   // create all the triangles
   DWORD adwVert[4];
   DWORD dwNum, j;
   PTEXTUREPOINT ptAll, ptThis;
   DWORD dwTextNew;
   PCPMVertOld pvt;
   for (i = 0; i < dwNumS; i++) {
      // if it's hiden ignore
      if (ps[i].m_fHide)
         continue;

      // figure our the vertices
      dwNum = ps[i].Num();
      for (j = 0; j < dwNum; j++) {
         adwVert[j] = ps[i].m_adwVert[j] + dwVertIndex;

         // if the textures are different need to make a new vertext
         pvt = &pv[ps[i].m_adwVert[j]];
         ptAll = pvt->TextureGet ();
         ptThis = pvt->TextureGet (i);
         PTEXTUREPOINT ptp;
         if (pText && (ptAll != ptThis) && !AreClose(ptAll, ptThis)) {
            ptp = rs.NewTextures (&dwTextNew, 1);
            if (ptp) {
               *ptp = *ptThis;
               rs.ApplyTextureRotation (ptp, 1);
               adwVert[j] = rs.NewVertex (dwPointIndex + ps[i].m_adwVert[j],
                  dwNormalsIndex + (pNormals ? ps[i].m_adwVert[j] : 0),
                  dwTextNew);
            }
         }
      }

      // set the part ID so that can see the divisions between the polygons
      // when draw outlines
      if (pr->fSelected)
         rs.SetIDPart ((WORD) (ps[i].m_dwOrigSide & 0xfff));   // ok if loops a bit

      if (ps[i].Num() == 3)
         rs.NewTriangle (adwVert[0], adwVert[1], adwVert[2], m_fCanBackface);
      else if (ps[i].Num() == 4)
         rs.NewQuad (adwVert[0], adwVert[1], adwVert[2], adwVert[3], m_fCanBackface);
   }
}


/**********************************************************************************
CObjectPolyMeshOld::Clone - Clones this

inputs
   none
returns
   PCObjectSocket - Clone
*/
PCObjectSocket CObjectPolyMeshOld::Clone (void)
{
   PCObjectPolyMeshOld pNew;

   pNew = new CObjectPolyMeshOld(NULL, &m_OSINFO);

   // clone template info
   CloneTemplate(pNew);

   pNew->m_dwDisplayControl = m_dwDisplayControl;
   pNew->m_pLastMoved.Copy (&m_pLastMoved);
   pNew->m_fCanBackface = m_fCanBackface;
   pNew->m_fMergeSmoothing = m_fMergeSmoothing;
   pNew->m_dwSymmetry = m_dwSymmetry;
   pNew->m_dwSubdivide = m_dwSubdivide;
   pNew->m_fDragSize = m_fDragSize;
   pNew->m_dwDivisions = m_dwDivisions;
   pNew->m_dwVertGroupDist = m_dwVertGroupDist;
   pNew->m_dwVertSideGroupDist = m_dwVertSideGroupDist;
   pNew->m_dwCurVertGroup = m_dwCurVertGroup;
   memcpy (pNew->m_apParam, m_apParam, sizeof(m_apParam));

   pNew->m_lVertexGroup.Init (sizeof(DWORD), m_lVertexGroup.Get(0),m_lVertexGroup.Num());
   pNew->m_lVertexInGroup.Init (sizeof(DWORD), m_lVertexInGroup.Get(0), m_lVertexInGroup.Num());
   pNew->m_lSideInGroup.Init(sizeof(DWORD), m_lSideInGroup.Get(0), m_lSideInGroup.Num());

   pNew->ClearVertSide();
   pNew->m_lCPMSide.Init (sizeof(CPMSideOld), m_lCPMSide.Get(0), m_lCPMSide.Num());
   DWORD i, dwNum;
   PCPMSideOld ps;
   ps = (PCPMSideOld) pNew->m_lCPMSide.Get(0);
   dwNum = pNew->m_lCPMSide.Num();
   for (i = 0; i < dwNum; i++) {
      CPMSideOld s;
      ps[i].CloneTo (&s);
      ps[i] = s;
   }
   pNew->m_lCPMVert.Init (sizeof(CPMVertOld), m_lCPMVert.Get(0), m_lCPMVert.Num());
   PCPMVertOld pv;
   pv = (PCPMVertOld) pNew->m_lCPMVert.Get(0);
   dwNum = pNew->m_lCPMVert.Num();
   for (i = 0; i < dwNum; i++) {
      CPMVertOld s;
      pv[i].CloneTo (&s);
      pv[i] = s;
   }

   for (i = 0; i < m_lPCOEAttrib.Num(); i++) {
      PCOEAttrib pa = *((PCOEAttrib*) m_lPCOEAttrib.Get(i));
      PCOEAttrib pNewA = new COEAttrib;
      if (!pNewA)
         continue;
      pa->CloneTo (pNewA);

      pNew->m_lPCOEAttrib.Add (&pNewA);
   }

   pNew->m_lMorphRemapID.Init (sizeof(DWORD), m_lMorphRemapID.Get(0), m_lMorphRemapID.Num());
   pNew->m_lMorphRemapValue.Init (sizeof(fp), m_lMorphRemapValue.Get(0), m_lMorphRemapValue.Num());

   pNew->m_pScaleMax.Copy (&m_pScaleMax);
   pNew->m_pScaleMin.Copy (&m_pScaleMin);
   pNew->m_pScaleSize.Copy (&m_pScaleSize);
   pNew->m_fDivDirty = TRUE;
   return pNew;
}

static PWSTR gpszDisplayControl = L"DisplayControl";
static PWSTR gpszDragSize = L"DragSize";
static PWSTR gpszSymmetry = L"Symmetry";
static PWSTR gpszCanBackface = L"CanBackface";
static PWSTR gpszSubdivide = L"Subdivide";
static PWSTR gpszDivisions = L"Divisions";
static PWSTR gpszVertGroupDist = L"VertGroupDist";
static PWSTR gpszVertSideGroupDist = L"VertSideGroupDist";
static PWSTR gpszMergeSmoothing = L"MergeSmoothing";

PCMMLNode2 CObjectPolyMeshOld::MMLTo (void)
{
   // call into the template first
   PCMMLNode2 pNode = MMLToTemplate();
   if (!pNode)
      return NULL;

   MMLValueSet (pNode, gpszDisplayControl, (int) m_dwDisplayControl);
   MMLValueSet (pNode, gpszCanBackface, (int) m_fCanBackface);
   MMLValueSet (pNode, gpszMergeSmoothing, (int)m_fMergeSmoothing);
   MMLValueSet (pNode, gpszDragSize, m_fDragSize);
   MMLValueSet (pNode, gpszSymmetry, (int)m_dwSymmetry);
   MMLValueSet (pNode, gpszSubdivide, (int)m_dwSubdivide);
   MMLValueSet (pNode, gpszDivisions, (int)m_dwDivisions);
   MMLValueSet (pNode, gpszVertGroupDist, (int)m_dwVertGroupDist);
   MMLValueSet (pNode, gpszVertSideGroupDist, (int)m_dwVertSideGroupDist);

   DWORD i;
   WCHAR szTemp[64];
   for (i = 0; i < 2; i++) {
      swprintf (szTemp, L"Param%d", (int) i);
      MMLValueSet (pNode, szTemp, &m_apParam[i]);
   }

   PCMMLNode2 pSub;
   for (i = 0; i < m_lCPMVert.Num(); i++) {
      PCPMVertOld pv = (PCPMVertOld) m_lCPMVert.Get(i);
      pSub = pv->MMLTo();
      if (pSub)
         pNode->ContentAdd (pSub);
   }
   for (i = 0; i < m_lCPMSide.Num(); i++) {
      PCPMSideOld pv = (PCPMSideOld) m_lCPMSide.Get(i);
      pSub = pv->MMLTo();
      if (pSub)
         pNode->ContentAdd (pSub);
   }

   // morphs
   PCOEAttrib *pap;
   pap = (PCOEAttrib*) m_lPCOEAttrib.Get(0);
   for (i = 0; i < m_lPCOEAttrib.Num(); i++) {
      pap[i]->m_fDefValue = pap[i]->m_fCurValue;   // so writes it out
      pSub = pap[i]->MMLTo ();
      if (pSub)
         pNode->ContentAdd (pSub);
      // NOTE: Dont need to do name set because already have one set by MMLTo()
   }

   return pNode;
}

BOOL CObjectPolyMeshOld::MMLFrom (PCMMLNode2 pNode)
{
   // call into the template first
   if (!MMLFromTemplate (pNode))
      return FALSE;


   m_dwDisplayControl = (DWORD) MMLValueGetInt (pNode, gpszDisplayControl, 0);
   m_fCanBackface= (BOOL) MMLValueGetInt (pNode, gpszCanBackface, 0);
   m_fMergeSmoothing = (BOOL) MMLValueGetInt (pNode, gpszMergeSmoothing, 0);
   m_dwSymmetry = (DWORD) MMLValueGetInt (pNode, gpszSymmetry, 0);
   m_dwSubdivide = (int) MMLValueGetInt (pNode, gpszSubdivide, 0);
   m_fDragSize = MMLValueGetDouble (pNode, gpszDragSize, 0);
   m_dwDivisions = (DWORD) MMLValueGetInt (pNode, gpszDivisions, 16);
   m_dwVertGroupDist = (DWORD) MMLValueGetInt (pNode, gpszVertGroupDist, 8);
   m_dwVertSideGroupDist = (DWORD) MMLValueGetInt (pNode, gpszVertSideGroupDist, 8);


   DWORD i;
   WCHAR szTemp[64];
   for (i = 0; i < 2; i++) {
      swprintf (szTemp, L"Param%d", (int) i);
      MMLValueGetPoint (pNode, szTemp, &m_apParam[i]);
   }

   CPMVertOld pv;
   CPMSideOld ps;
   PWSTR psz;
   PCMMLNode2 pSub;
   ClearVertSide();
   memset (&pv, 0, sizeof(pv));
   memset (&ps, 0, sizeof(ps));

   // find all the vertecies and sides
   for (i = 0; i < pNode->ContentNum(); i++) {
      pSub = NULL;
      pNode->ContentEnum (i, &psz, &pSub);
      if (!pSub)
         continue;
      psz = pSub->NameGet();
      if (!psz)
         continue;

      if (!wcscmp(psz, gpszVert)) {
         if (!pv.MMLFrom (pSub))
            continue;
         m_lCPMVert.Add (&pv);
         pv.Zero();
      }
      else if (!wcscmp(psz, gpszSide)) {
         if (!ps.MMLFrom (pSub))
            continue;
         m_lCPMSide.Add (&ps);
         ps.Zero();
      }
      else if (!wcscmp(psz, Attrib())) {
         PCOEAttrib pa = new COEAttrib;
         if (!pa)
            continue;
         if (!pa->MMLFrom (pSub)) {
            delete pa;
            continue;
         }
         m_lPCOEAttrib.Add (&pa);
      }
   }
   // Sort m_lPCOEAttrib list
   qsort (m_lPCOEAttrib.Get(0), m_lPCOEAttrib.Num(), sizeof(PCOEAttrib), OECALCompare);

   // Normals and textures are dirty
   m_fDivDirty = TRUE;
   m_fVertGroupDirty = TRUE;

   // calc scale
   CalcScale ();
   CalcMorphRemap ();
   return TRUE;
}


/**********************************************************************************
CObjectPolyMeshOld::DialogCPQuery - Returns TRUE if the object supports a custom
dialog box, FALSE if it doesn't. The default object doesn't
*/
BOOL CObjectPolyMeshOld::DialogCPQuery (void)
{
   // BUGFIX - No CP dialog when it's whole
   return m_dwDivisions ? FALSE : TRUE;
}



/* PolyMeshDisplayPage
*/
BOOL PolyMeshDisplayPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCObjectPolyMeshOld pv = (PCObjectPolyMeshOld)pPage->m_pUserData;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         PCEscControl pControl;

         // scrollbars
         pControl = pPage->ControlFind (L"region");
         if (pControl)
            pControl->AttribSetInt (Pos(), pv->m_dwVertSideGroupDist);
         pControl = pPage->ControlFind (L"density");
         if (pControl)
            pControl->AttribSetInt (Pos(), pv->m_dwVertGroupDist);

         // set thecheckboxes
         pControl = NULL;
         switch (pv->m_dwDisplayControl) {
         case 0:  // size
         default:
            pControl = pPage->ControlFind (L"scale");
            break;
         case 1:  // shape
            pControl = pPage->ControlFind (L"shape");
            break;
         case 2:  // divide
            pControl = pPage->ControlFind (L"divide");
            break;
         case 3:  // show/hide
            pControl = pPage->ControlFind (L"hide");
            break;
         case 4:  // merge sides
            pControl = pPage->ControlFind (L"merge");
            break;
         }
         if (pControl)
            pControl->AttribSetBOOL (Checked(), TRUE);

         MeasureToString (pPage, L"dragsize", pv->m_fDragSize);

         // check the symmetry
         if ((pv->m_dwSymmetry & 0x01) && (pControl = pPage->ControlFind (L"sym0")))
            pControl->AttribSetBOOL (Checked(), TRUE);
         if ((pv->m_dwSymmetry & 0x02) && (pControl = pPage->ControlFind (L"sym1")))
            pControl->AttribSetBOOL (Checked(), TRUE);
         if ((pv->m_dwSymmetry & 0x04) && (pControl = pPage->ControlFind (L"sym2")))
            pControl->AttribSetBOOL (Checked(), TRUE);
      }
      break;

   case ESCN_SCROLL:
   case ESCN_SCROLLING:
      {
         PESCNSCROLL p = (PESCNSCROLL) pParam;

         // only do one scroll bar
         if (!p->pControl->m_pszName)
            break;

         DWORD  dwVal;
         if (!wcsicmp(p->pControl->m_pszName, L"region")) {
            dwVal = (DWORD) p->pControl->AttribGetInt (Pos());
            if (dwVal != pv->m_dwVertSideGroupDist) {
               pv->m_pWorld->ObjectAboutToChange (pv);
               pv->m_dwVertSideGroupDist = dwVal;
               pv->CalcVertexSideInGroup (pv->m_dwCurVertGroup);
               pv->m_pWorld->ObjectChanged (pv);
            }
            return TRUE;
         }
         else if (!wcsicmp(p->pControl->m_pszName, L"density")) {
            dwVal = (DWORD) p->pControl->AttribGetInt (Pos());
            if (dwVal != pv->m_dwVertGroupDist) {
               pv->m_pWorld->ObjectAboutToChange (pv);
               pv->m_dwVertGroupDist = dwVal;
               pv->m_fVertGroupDirty = TRUE;
               pv->m_pWorld->ObjectChanged (pv);
            }
            return TRUE;
         }
      }
      break;

   case ESCN_EDITCHANGE:
      {
         PESCNEDITCHANGE p = (PESCNEDITCHANGE) pParam;
         PWSTR psz = p->pControl->m_pszName;
         if (!psz)
            break;

         if (!wcsicmp(psz, L"dragsize")) {
            pv->m_pWorld->ObjectAboutToChange (pv);
            MeasureParseString (pPage, L"dragsize", &pv->m_fDragSize);
            pv->m_pWorld->ObjectChanged (pv);
         }
      }
      break;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;

         // find otu which is checked
         PCEscControl pControl;
         DWORD dwNewMove, dwNewSym;
         dwNewMove = pv->m_dwDisplayControl;
         dwNewSym = pv->m_dwSymmetry;
         if ((pControl = pPage->ControlFind (L"shape")) && pControl->AttribGetBOOL(Checked()))
            dwNewMove = 1;  // shape;
         else if ((pControl = pPage->ControlFind (L"divide")) && pControl->AttribGetBOOL(Checked()))
            dwNewMove = 2;  // shape;
         else if ((pControl = pPage->ControlFind (L"hide")) && pControl->AttribGetBOOL(Checked()))
            dwNewMove = 3;  // show/hide
         else if ((pControl = pPage->ControlFind (L"merge")) && pControl->AttribGetBOOL(Checked()))
            dwNewMove = 4;  // merge
         else
            dwNewMove = 0; // scale

         if ((pControl = pPage->ControlFind (L"sym0")) && pControl->AttribGetBOOL(Checked()))
            dwNewSym = dwNewSym | 0x01;
         else
            dwNewSym = dwNewSym & (~0x01);
         if ((pControl = pPage->ControlFind (L"sym1")) && pControl->AttribGetBOOL(Checked()))
            dwNewSym = dwNewSym | 0x02;
         else
            dwNewSym = dwNewSym & (~0x02);
         if ((pControl = pPage->ControlFind (L"sym2")) && pControl->AttribGetBOOL(Checked()))
            dwNewSym = dwNewSym | 0x04;
         else
            dwNewSym = dwNewSym & (~0x04);
         
         if ((dwNewMove == pv->m_dwDisplayControl) && (dwNewSym == pv->m_dwSymmetry))
            break;   // no change

         pv->m_pWorld->ObjectAboutToChange (pv);
         pv->m_dwDisplayControl = dwNewMove;
         pv->m_dwSymmetry = dwNewSym;
         pv->m_pWorld->ObjectChanged (pv);

         // make sure they're visible
         pv->m_pWorld->SelectionClear();
         GUID gObject;
         pv->GUIDGet (&gObject);
         pv->m_pWorld->SelectionAdd (pv->m_pWorld->ObjectFind(&gObject));
         return TRUE;

      }
      break;   // default


   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Which control points are displayed";
            return TRUE;
         }
      }
      break;
   };

   return DefPage (pPage, dwMessage, pParam);
}





/**********************************************************************************
CObjectPolyMeshOld::DialogCPShow - Causes the object to show a dialog box that allows
different attributes to be changed.

inputs
   DWORD             dwSuface - Surface that was clicked on. 0 if unknown.
   PCEscWindow       pWindow - Escarpment window to draw it in
returns
   BOOL - TRUE if user pressed "Back", FALSE if user pressed "Close"
*/
BOOL CObjectPolyMeshOld::DialogCPShow (DWORD dwSurface, PCEscWindow pWindow)
{
   if (m_dwDivisions)
      return FALSE;

   PWSTR pszRet;
   pszRet = pWindow->PageDialog (ghInstance, IDR_MMLPOLYMESHDISPLAY, PolyMeshDisplayPage, this);
   CalcScale ();  // automatically recalc scale
   if (!pszRet)
      return FALSE;

   return !wcsicmp(pszRet, Back());
}


/*************************************************************************************
CObjectPolyMeshOld::MagneticDrag - Call this function if a control point is moved. This
will magnetically move all the points.

NOTE: Assumes deformations in place with m_pLocWithDeform set

inputs
   DWORD       dwID - ID of the vertex
   CPoint      pVal - New location
retursn
   BOOL - TRUE if success
*/
BOOL CObjectPolyMeshOld::MagneticDrag (DWORD dwID, PCPoint pVal)
{
   if ((dwID >= m_lCPMVert.Num()) || (m_lMorphRemapID.Num() > 1))
      return FALSE;

   // which one deformed?
   DWORD dwDeform;
   fp fDeformAmt;
   dwDeform = -1;
   fDeformAmt = 1;
   if (m_lMorphRemapID.Num()) {
      dwDeform = *((DWORD*) m_lMorphRemapID.Get(0));
      fDeformAmt = *((fp*) m_lMorphRemapValue.Get(0));
   }

   CPoint pvl;
   pvl.Copy (pVal);

   // tell the world we're about to change
   if (m_pWorld)
      m_pWorld->ObjectAboutToChange (this);

   PCPMVertOld pvMove, pv;
   DWORD dwNum;
   dwNum = m_lCPMVert.Num();
   pv = (PCPMVertOld) m_lCPMVert.Get(0);
   pvMove = pv + dwID;

   // BUGFIX - If symmetry is on AND the old point was on the symmetry border then dont
   // move off the symmetry border

   DWORD i, j;
   for (j = 0; j < 3; j++)
      if (m_dwSymmetry & (1 << j)) {
         if (fabs(pvMove->m_pLoc.p[j]) < CLOSE)
            pvl.p[j] = pvMove->m_pLocWithDeform.p[j];   // dont allow to move off symmetry
      }

   // keep track of this new location
   CPoint pDelta, pOrig, pSym, pSymScale;
   m_pLastMoved.Copy (&pvl);
   m_pLastMoved.p[3] = 1;
   pDelta.Subtract (&pvl, &pvMove->m_pLocWithDeform);
   pDelta.Scale (1.0 / fDeformAmt); // so if have set to excess then handles scale properly

   if (m_fDragSize < CLOSE) {
      // use the new value
      pvMove->DeformChanged (dwDeform, &pDelta);
      goto done;
   }

   pOrig.Copy (&pvMove->m_pLocWithDeform);

   // loop over all the points and change them
   CPoint pDist, pScale;
   PCPoint pLoc;
   fp fDist;
   pSym.Copy (&pOrig);
#define DISTSCALE    5.0
   for (i = 0; i < dwNum; i++) {
      pLoc = &pv[i].m_pLocWithDeform;

      // see if need to adjust for symmetry options
      if (m_dwSymmetry) {
         pSym.Copy (&pOrig);
         pSymScale.p[0] = pSymScale.p[1] = pSymScale.p[2] = 1;

         for (j = 0; j < 3; j++)
            if (m_dwSymmetry & (1 << j)) {
               if ((fabs(pLoc->p[j]) < CLOSE) || (fabs(pOrig.p[j]) < CLOSE)) {
                  pSymScale.p[j] = 0;
                  continue;   // either on the zero line then no symmetry adjust
               }

               if (pLoc->p[j] * pOrig.p[j] < 0) {
                  pSym.p[j] *= -1;
                  pSymScale.p[j] = -1;
               }
            }
      }

      pDist.Subtract (pLoc, &pSym);
      fDist = pDist.Length();
      
      if (fDist >= m_fDragSize)
         continue;
      fDist = fDist / m_fDragSize;
      fDist = 1.0 + fDist * fDist * fDist * DISTSCALE;
      fDist = ((1.0 / fDist) - (1.0 / DISTSCALE)) / (1.0 - 1.0 / DISTSCALE);

      pScale.Copy (&pDelta);
      pScale.Scale (fDist);
      if (m_dwSymmetry) {
         pScale.p[0] *= pSymScale.p[0];
         pScale.p[1] *= pSymScale.p[1];
         pScale.p[2] *= pSymScale.p[2];
      }

      pv[i].DeformChanged (dwDeform, &pScale);
   }

done:
   m_fDivDirty = TRUE;

   // recalc scale
   CalcScale();

   // tell the world we've changed
   if (m_pWorld)
      m_pWorld->ObjectChanged (this);

   return TRUE;
}


/*************************************************************************************
CObjectPolyMeshOld::ControlPointQuery - Called to query the information about a control
point identified by dwID.

inputs
   DWORD       dwID - ID
   POSCONTROL  pInfo - Filled in with the infomration
returns
   BOOL - TRUE if successful
*/
BOOL CObjectPolyMeshOld::ControlPointQuery (DWORD dwID, POSCONTROL pInfo)
{
   fp fKnobSize = max(max (m_pScaleSize.p[0], m_pScaleSize.p[1]), m_pScaleSize.p[2]) / 100.0;

   // make sure know vertices
   CalcVertexGroups ();

   if ((m_dwDisplayControl == 1) && m_dwDivisions) {
      memset (pInfo,0, sizeof(*pInfo));

      pInfo->dwID = dwID;
      //pInfo->dwFreedom = 0;   // any direction
      pInfo->dwStyle = CPSTYLE_CUBE;
      pInfo->fSize = fKnobSize;
      pInfo->cColor = RGB(0xff,0,0xff);
      wcscpy (pInfo->szName, L"Shape");

      switch (m_dwType) {
         default:
         case 0:  // sphere
         case 7:  // box
            switch (dwID) {
            case 0:
            case 1:
            case 2:
               pInfo->pLocation.p[dwID] = m_apParam[0].p[dwID];
               break;
            case 3:
               pInfo->cColor = RGB(0xff,0xff,0xff);
               pInfo->pLocation.Copy (&m_apParam[0]);
               break;
            default:
               return FALSE;  // error
            }
            break;
         case 3:  // cone, open on tpo
         case 4:  // cone, closed on top
            switch (dwID) {
            case 0:
            case 1:
               pInfo->pLocation.p[dwID] = m_apParam[0].p[dwID];
               break;
            case 2:
               pInfo->pLocation.p[dwID] = m_apParam[1].p[1];
               break;
            case 3:
               pInfo->cColor = RGB(0xff,0xff,0xff);
               pInfo->pLocation.Copy (&m_apParam[0]);
               pInfo->pLocation.p[2] = m_apParam[1].p[1];
               break;
            default:
               return FALSE;  // error
            }
            break;
         case 5:  // disc, also cone
         case 8:  // plane
            switch (dwID) {
            case 0:
            case 1:
               pInfo->pLocation.p[dwID] = m_apParam[0].p[dwID];
               break;
            case 2:
               pInfo->cColor = RGB(0xff,0xff,0xff);
               pInfo->pLocation.Copy (&m_apParam[0]);
               pInfo->pLocation.p[2] = 0;
               break;
            default:
               return FALSE;  // error
            }
            break;
         case 6:  // two cones
            switch (dwID) {
            case 0:
            case 1:
               pInfo->pLocation.p[dwID] = m_apParam[0].p[dwID];
               break;
            case 2:
               pInfo->pLocation.p[2] = m_apParam[1].p[0];
               break;
            case 3:
               pInfo->pLocation.p[2] = m_apParam[1].p[1];
               break;
            case 4:
               pInfo->cColor = RGB(0xff,0xff,0xff);
               pInfo->pLocation.Copy (&m_apParam[0]);
               pInfo->pLocation.p[2] = m_apParam[1].p[0];
               break;
            default:
               return FALSE;  // error
            }
            break;
         case 1:  // cylinder without cap
         case 2:  // cylinder with cap
            switch (dwID) {
            case 0:
            case 1:
            case 2:
            case 3:
               pInfo->pLocation.p[dwID%2] = m_apParam[dwID/2].p[dwID%2];
               pInfo->pLocation.p[2] = ((dwID/2) ? -1 : 1) * m_apParam[0].p[2];
               break;
            case 4:
               pInfo->pLocation.p[2] = m_apParam[0].p[2];
               break;
            case 5:
               pInfo->cColor = RGB(0xff,0xff,0xff);
               pInfo->pLocation.Copy (&m_apParam[0]);
               break;
            default:
               return FALSE;  // error
            }
            break;
         case 10:  // cylinder rounded
            switch (dwID) {
            case 0:
            case 1:
            case 2:
            case 3:
            case 4:
            case 5:
               pInfo->pLocation.p[dwID%3] = m_apParam[dwID/3].p[dwID%3];
               if (dwID == 5)
                  pInfo->pLocation.p[dwID%3] *= -1;
               pInfo->pLocation.p[2] += ((dwID/3) ? -1 : 1) * m_apParam[0].p[3];
               break;
            case 6:
               pInfo->cColor = RGB(0xff,0xff,0xff);
               pInfo->pLocation.Copy (&m_apParam[0]);
               pInfo->pLocation.p[2] += m_apParam[0].p[3];
               break;
            default:
               return FALSE;  // error
            }
            break;
         case 9:  // taurus
            switch (dwID) {
            case 0:
            case 1:
               pInfo->pLocation.p[dwID] = m_apParam[0].p[dwID] + m_apParam[1].p[0];  // extend by x
               break;
            case 2:
               pInfo->cColor = RGB(0xff,0xff,0);
               pInfo->pLocation.p[0] = -m_apParam[0].p[0] - m_apParam[1].p[0];
               break;
            case 3:
               pInfo->cColor = RGB(0xff,0xff,0);
               pInfo->pLocation.p[0]= -m_apParam[0].p[0];
               pInfo->pLocation.p[2] = m_apParam[1].p[1];
               break;
            case 4:
               pInfo->cColor = RGB(0xff,0xff,0xff);
               pInfo->pLocation.Copy (&m_apParam[0]);
               pInfo->pLocation.p[0] += m_apParam[1].p[0];
               pInfo->pLocation.p[1] += m_apParam[1].p[0];
               pInfo->pLocation.p[2] = 0;
               break;
            default:
               return FALSE;  // error
            }
            break;
      }
      return TRUE;
   }
   else if ((m_dwDisplayControl == 1) && (dwID < MAXVERTGROUP)) {
      // control the points
      if (dwID >= m_lVertexInGroup.Num())
         return FALSE;

      // if more than more morph target active then no CP for dragging edges
      if (m_lMorphRemapID.Num() > 1)
         return FALSE;

      DWORD dwVert;
      dwVert = *((DWORD*) m_lVertexInGroup.Get(dwID));
      if (dwVert >= m_lCPMVert.Num())
         return FALSE;

      memset (pInfo,0, sizeof(*pInfo));

      pInfo->dwID = dwID;
      //pInfo->dwFreedom = 0;   // any direction
      pInfo->dwStyle = CPSTYLE_CUBE;
      pInfo->fSize = fKnobSize;
      pInfo->cColor = RGB(0xff,0,0xff);
      wcscpy (pInfo->szName, L"Shape");

      PCPMVertOld pv;
      pv = (PCPMVertOld) m_lCPMVert.Get(dwVert);
      pInfo->pLocation.Copy (&pv->m_pLocWithDeform);

      return TRUE;
   }
   else if ((m_dwDisplayControl == 4) && (dwID < MAXVERTGROUP)) { // merge sides
      // control the points
      if (dwID >= m_lVertexInGroup.Num())
         return FALSE;

      DWORD dwVert;
      dwVert = *((DWORD*) m_lVertexInGroup.Get(dwID));
      if (dwVert >= m_lCPMVert.Num())
         return FALSE;

      memset (pInfo,0, sizeof(*pInfo));

      pInfo->dwID = dwID;
      pInfo->fButton = TRUE;
      pInfo->dwStyle = CPSTYLE_POINTER;
      pInfo->fSize = fKnobSize;
      pInfo->cColor = RGB(0xff,0x40,0x40);
      wcscpy (pInfo->szName, L"Click to remove");

      PCPMVertOld pv;
      pv = (PCPMVertOld) m_lCPMVert.Get(dwVert);
      pInfo->pLocation.Copy (&pv->m_pLocWithDeform);

      return TRUE;
   }
   else if ( ((m_dwDisplayControl == 2) || (m_dwDisplayControl == 3)) && (dwID < MAXVERTGROUP)) { // divide the side, or show/hide side
      // control the points
      if (dwID >= m_lSideInGroup.Num())
         return FALSE;

      DWORD dwSide;
      dwSide = *((DWORD*) m_lSideInGroup.Get(dwID));
      if (dwSide >= m_lCPMSide.Num())
         return FALSE;

      memset (pInfo,0, sizeof(*pInfo));
      pInfo->dwID = dwID;
      pInfo->fButton = TRUE;
      pInfo->dwStyle = CPSTYLE_POINTER;
      pInfo->fSize = fKnobSize * 4; // do *2 because curvature sometimes obscures
      pInfo->cColor = (m_dwDisplayControl == 2) ? RGB(0x40,0x40,0xff) : RGB(0x40,0xff,0x40);
      wcscpy (pInfo->szName, (m_dwDisplayControl == 2) ? L"Click to subdivide" : L"Clock to show/hide");

      PCPMVertOld pv;
      PCPMSideOld ps;
      pv = (PCPMVertOld) m_lCPMVert.Get(0);
      ps = (PCPMSideOld) m_lCPMSide.Get(dwSide);
      pInfo->pLocation.Zero();
      DWORD i;
      for (i = 0; i < ps->Num(); i++)
         pInfo->pLocation.Add (&pv[ps->m_adwVert[i]].m_pLocWithDeform);
      pInfo->pLocation.Scale (1.0 / (fp) ps->Num());

      return TRUE;
   }
   else if (m_dwDisplayControl == 0) { // scale
      if (dwID >= 4)
         return FALSE;

      memset (pInfo,0, sizeof(*pInfo));

      pInfo->dwID = dwID;
      //pInfo->dwFreedom = 0;   // any direction
      pInfo->dwStyle = CPSTYLE_CUBE;
      pInfo->fSize = fKnobSize;
      pInfo->cColor = (dwID < 3) ? RGB(0,0xff,0xff) : RGB(0xff,0xff,0xff);
      switch (dwID) {
      case 0:
      default:
         wcscpy (pInfo->szName, L"Width");
         break;
      case 1:
         wcscpy (pInfo->szName, L"Depth");
         break;
      case 2:
         wcscpy (pInfo->szName, L"Height");
         break;
      case 3:
         wcscpy (pInfo->szName, L"Scale");
         break;
      }
      if (dwID < 3)
         MeasureToString (m_pScaleSize.p[dwID], pInfo->szMeasurement);

      pInfo->pLocation.Zero();
      if (dwID < 3)
         pInfo->pLocation.p[dwID] = m_pScaleSize.p[dwID] / 2.0;
      else {
         pInfo->pLocation.Copy (&m_pScaleSize);
         pInfo->pLocation.Scale (.5);
      }

      return TRUE;
   }
   else if (dwID >= MAXVERTGROUP) {
      // showing the control points to switch

      // control the points
      if (dwID >= MAXVERTGROUP+m_lVertexGroup.Num())
         return FALSE;

      DWORD dwVert;
      dwVert = *((DWORD*) m_lVertexGroup.Get(dwID-MAXVERTGROUP));
      if (dwVert >= m_lCPMVert.Num())
         return FALSE;

      memset (pInfo,0, sizeof(*pInfo));

      pInfo->dwID = dwID;
      pInfo->fButton = TRUE;
      //pInfo->dwFreedom = 0;   // any direction
      pInfo->dwStyle = CPSTYLE_POINTER;
      pInfo->fSize = fKnobSize;
      pInfo->cColor = RGB(0xff,0xff,0xff);
      wcscpy (pInfo->szName, L"Use this region");

      PCPMVertOld pv;
      pv = (PCPMVertOld) m_lCPMVert.Get(dwVert);
      pInfo->pLocation.Copy (&pv->m_pLocWithDeform);

      return TRUE;
   }
   return FALSE;
}

/*************************************************************************************
CObjectPolyMeshOld::ControlPointSet - Called to change the valud of a control point.

inputs
   DWORD       dwID - ID
   PCPoint     pVal - Contains the new location, in object coordinates
returns
   BOOL - TRUE if successful
*/
BOOL CObjectPolyMeshOld::ControlPointSet (DWORD dwID, PCPoint pVal, PCPoint pViewer)
{
   // make sure know vertices
   CalcVertexGroups ();

   if ((m_dwDisplayControl == 1) && m_dwDivisions) {
      CPoint ap[2];
      fp fLen;
      ap[0].Copy (&m_apParam[0]);
      ap[1].Copy (&m_apParam[1]);

      CPoint pMin;
      CPoint pt;
      pt.Copy (pVal);
      pMin.p[0] = pMin.p[1] = pMin.p[2] = CLOSE;

      switch (m_dwType) {
         default:
         case 0:  // sphere
         case 7:  // box
            switch (dwID) {
            case 0:
            case 1:
            case 2:
               ap[0].p[dwID] = pVal->p[dwID];
               break;
            case 3:
               fLen = pVal->Length();
               fLen /= ap[0].Length();
               ap[0].Scale (fLen);
               break;
            default:
               return FALSE;  // error
            }
            ap[0].Max (&pMin);
            ap[1].Max (&pMin);
            break;
         case 3:  // cone, open on tpo
         case 4:  // cone, closed on top
            switch (dwID) {
            case 0:
            case 1:
               ap[0].p[dwID] = pVal->p[dwID];
               break;
            case 2:
               ap[1].p[1] = pVal->p[dwID];
               break;
            case 3:
               fLen = pVal->Length();
               fLen /= sqrt(ap[0].p[0] * ap[0].p[0] + ap[0].p[1] * ap[0].p[1] + ap[1].p[1] * ap[1].p[1]);
               ap[0].Scale (fLen);
               ap[1].Scale (fLen);
               break;
            default:
               return FALSE;  // error
            }
            ap[0].Max (&pMin);
            break;
         case 5:  // disc, also cone
         case 8:  // plane
            switch (dwID) {
            case 0:
            case 1:
               ap[0].p[dwID] = pVal->p[dwID];
               break;
            case 2:
               fLen = pVal->Length();
               ap[0].p[2] = 0;   // since always 0
               fLen /= ap[0].Length();
               ap[0].Scale (fLen);
               break;
            default:
               return FALSE;  // error
            }
            ap[0].Max (&pMin);
            break;
         case 6:  // two cones
            switch (dwID) {
            case 0:
            case 1:
               ap[0].p[dwID] = pVal->p[dwID];
               break;
            case 2:
               ap[1].p[0] = pVal->p[2];
               break;
            case 3:
               ap[1].p[1] = pVal->p[2];
               break;
            case 4:
               fLen = pVal->Length();
               fLen /= sqrt(ap[0].p[0] * ap[0].p[0] + ap[0].p[1] * ap[0].p[1] + ap[1].p[0] * ap[1].p[0]);
               ap[0].Scale (fLen);
               ap[1].Scale (fLen);
               break;
            default:
               return FALSE;  // error
            }
            ap[0].Max (&pMin);
            break;
         case 1:  // cylinder without cap
         case 2:  // cylinder with cap
            switch (dwID) {
            case 0:
            case 1:
            case 2:
            case 3:
               ap[dwID/2].p[dwID%2] = pVal->p[dwID%2];
               break;
            case 4:
               ap[0].p[2] = pVal->p[2];
               break;
            case 5:
               fLen = pVal->Length();
               fLen /= ap[0].Length();
               ap[0].Scale (fLen);
               ap[1].Scale (fLen);
               break;
            default:
               return FALSE;  // error
            }
            ap[0].Max (&pMin);
            ap[1].Max (&pMin);
            break;
         case 10:  // cylinder, rounded cap
            switch (dwID) {
            case 0:
            case 1:
            case 2:
            case 3:
            case 4:
            case 5:
               if ((dwID%3) == 2)
                  pt.p[2] -= ((dwID/3) ? -1 : 1) * ap[0].p[3];
               if (dwID == 5)
                  pt.p[2] *= -1;
               ap[dwID/3].p[dwID%3] = pt.p[dwID%3];

               if ((dwID % 3) < 2) {
                  ap[0].p[3] = pVal->p[2];
                  if (dwID / 3)
                     ap[0].p[3] *= -1;
                  ap[0].p[3] = max(ap[0].p[3], CLOSE);
               }
               break;
            case 6:
               fLen = pt.Length();
               pt.Copy (&ap[0]);
               pt.p[2] += ap[0].p[3];
               fLen /= pt.Length();
               ap[0].Scale (fLen);
               ap[0].p[3] *= fLen;
               ap[1].Scale (fLen);
               break;
            default:
               return FALSE;  // error
            }
            ap[0].Max (&pMin);
            ap[1].Max (&pMin);
            break;
         case 9:  // taurus
            switch (dwID) {
            case 0:
            case 1:
               ap[0].p[dwID] = pVal->p[dwID] - ap[1].p[dwID];
               break;
            case 2:
               ap[1].p[0] = -(pVal->p[0] + ap[0].p[0]);
               break;
            case 3:
               ap[1].p[1] = pVal->p[2];
               break;
            case 4:
               pt.p[0] -= ap[1].p[0];
               pt.p[1] -= ap[1].p[0];
               pt.p[2] = 0;
               fLen = pt.Length();
               ap[0].p[2] = 0;   // should be zero anyway
               fLen /= ap[0].Length();
               ap[0].Scale (fLen);
               ap[1].Scale (fLen);
               break;
            default:
               return FALSE;  // error
            }
            ap[0].Max (&pMin);
            ap[1].Max (&pMin);
            break;
      }

      // tell the world we're about to change
      if (m_pWorld)
         m_pWorld->ObjectAboutToChange (this);
      m_apParam[0].Copy (&ap[0]);
      m_apParam[1].Copy (&ap[1]);
      m_fDivDirty = TRUE;
      CreateBasedOnType ();
      // tell the world we've changed
      if (m_pWorld)
         m_pWorld->ObjectChanged (this);
      return TRUE;
   }
   else if ((m_dwDisplayControl == 1) && (dwID < MAXVERTGROUP)) {
      // control the points
      if (dwID >= m_lVertexInGroup.Num())
         return FALSE;

      DWORD dwVert;
      dwVert = *((DWORD*) m_lVertexInGroup.Get(dwID));
      if (dwVert >= m_lCPMVert.Num())
         return FALSE;

      // if more than more morph target active then no CP for dragging edges
      if (m_lMorphRemapID.Num() > 1)
         return FALSE;

      return MagneticDrag (dwVert, pVal);
   }
   else if ((m_dwDisplayControl == 4) && (dwID < MAXVERTGROUP)) { // divide side
      // control the points
      if (dwID >= m_lVertexInGroup.Num())
         return FALSE;

      DWORD dwVert;
      dwVert = *((DWORD*) m_lVertexInGroup.Get(dwID));
      if (dwVert >= m_lCPMVert.Num())
         return FALSE;

      // tell the world we're about to change
      if (m_pWorld)
         m_pWorld->ObjectAboutToChange (this);

      RemoveVertexSymmetry (dwVert);
      m_fVertGroupDirty = TRUE;
      m_fDivDirty = TRUE;
      CalcScale();
      CalcVertexSideInGroup (m_dwCurVertGroup);

      // tell the world we've changed
      if (m_pWorld)
         m_pWorld->ObjectChanged (this);

      return TRUE;
   }
   else if ((m_dwDisplayControl == 2) && (dwID < MAXVERTGROUP)) { // divide side
      // control the points
      if (dwID >= m_lSideInGroup.Num())
         return FALSE;

      DWORD dwSide;
      dwSide = *((DWORD*) m_lSideInGroup.Get(dwID));
      if (dwSide >= m_lCPMSide.Num())
         return FALSE;

      // tell the world we're about to change
      if (m_pWorld)
         m_pWorld->ObjectAboutToChange (this);

      SubdivideSide (dwSide);
      CalcVertexSideInGroup (m_dwCurVertGroup);

      // tell the world we've changed
      if (m_pWorld)
         m_pWorld->ObjectChanged (this);

      return TRUE;
   }
   else if ((m_dwDisplayControl == 3) && (dwID < MAXVERTGROUP)) { // show/hide side
      // control the points
      if (dwID >= m_lSideInGroup.Num())
         return FALSE;

      DWORD dwSide;
      dwSide = *((DWORD*) m_lSideInGroup.Get(dwID));
      if (dwSide >= m_lCPMSide.Num())
         return FALSE;

      // tell the world we're about to change
      if (m_pWorld)
         m_pWorld->ObjectAboutToChange (this);

      ShowHideSide (dwSide);

      // tell the world we've changed
      if (m_pWorld)
         m_pWorld->ObjectChanged (this);

      return TRUE;
   }
   else if (m_dwDisplayControl == 0) { // scale
      if (dwID >= 4)
         return FALSE;

      // figure out the new scale
      CPoint pNew, pMid;
      fp fOldArea, fNewArea;
      pMid.Zero();
      pNew.Copy (&m_pScaleSize);
      fOldArea = m_pScaleSize.p[0] * m_pScaleSize.p[1] * m_pScaleSize.p[2];
      if (dwID < 3) {
         pNew.p[dwID] = (pVal->p[dwID] - pMid.p[dwID]) * 2.0;
         pNew.p[dwID] = max(CLOSE, pNew.p[dwID]);
      }
      else {
         fp fLen = pVal->Length();
         fLen /= (m_pScaleSize.Length() / 2);
         pNew.Scale (fLen);
         pNew.p[0] = max(CLOSE, pNew.p[0]);
         pNew.p[1] = max(CLOSE, pNew.p[1]);
         pNew.p[2] = max(CLOSE, pNew.p[2]);
      }

      if (pNew.AreClose (&m_pScaleSize))
         return TRUE;   // no change
      fNewArea = pNew.p[0] * pNew.p[1] * pNew.p[2];

      // tell the world we're about to change
      if (m_pWorld)
         m_pWorld->ObjectAboutToChange (this);

      // adjust drag size to reflect the change in the size of the object
      m_fDragSize *= pow(fNewArea, 1.0 / 3.0) / pow(fOldArea, 1.0 / 3.0);

      // recalc
      DWORD dwNum;
      PCPMVertOld pOld;
      pOld = (PCPMVertOld) m_lCPMVert.Get(0);
      dwNum = m_lCPMVert.Num();

      // create the scale matricies
      CMatrix mTrans, mScale, mScaleInvTrans;
      mScale.Scale (pNew.p[0] / m_pScaleSize.p[0],
         pNew.p[1] / m_pScaleSize.p[1],
         pNew.p[2] / m_pScaleSize.p[2]);
      mTrans.Translation (-pMid.p[0], -pMid.p[1], -pMid.p[2]);
      mScale.MultiplyLeft (&mTrans);   // translate so scales around the center
      mTrans.Translation (pMid.p[0], pMid.p[1], pMid.p[2]);
      mScale.MultiplyRight (&mTrans);   // add back the center
      mScale.Invert (&mScaleInvTrans); // dont use invert4 because don't need positional or translation
      mScaleInvTrans.Transpose();

      // fill in the new values
      DWORD i;
      for (i = 0; i < dwNum; i++)
         pOld[i].Scale (&mScale, &mScaleInvTrans);


      m_fDivDirty = TRUE;
      CalcScale();

      // tell the world we've changed
      if (m_pWorld)
         m_pWorld->ObjectChanged (this);

      return TRUE;
   }
   else if (dwID >= MAXVERTGROUP) {
      // clicked on vertex control point, so change object
      // since this is a button, just ignore val

      // control the points
      if (dwID >= MAXVERTGROUP+m_lVertexGroup.Num())
         return FALSE;

      DWORD dwVert;
      dwVert = *((DWORD*) m_lVertexGroup.Get(dwID-MAXVERTGROUP));
      if (dwVert >= m_lCPMVert.Num())
         return FALSE;
      // tell the world we're about to change
      if (m_pWorld)
         m_pWorld->ObjectAboutToChange (this);

      // keep track of this new location
      PCPMVertOld pv;
      pv = (PCPMVertOld) m_lCPMVert.Get(dwVert);
      m_pLastMoved.Copy (&pv->m_pLoc); // leave as m_pLoc
      m_pLastMoved.p[3] = 1;

      CalcVertexSideInGroup (dwVert);

      // tell the world we've changed
      if (m_pWorld)
         m_pWorld->ObjectChanged (this);

      return TRUE;
   }

   return FALSE;
}

/*************************************************************************************
CObjectPolyMeshOld::ControlPointEnum - Called to enumerate a list of control point IDs
into the list.

inputs
   PCListFixed       plDWORD - Should be filled in with control points
returns
   none
*/
void CObjectPolyMeshOld::ControlPointEnum (PCListFixed plDWORD)
{
   DWORD i;
   DWORD dwNum;

   // make sure know vertices
   CalcVertexGroups ();

   if (m_dwDisplayControl == 1) {
      if (m_dwDivisions) switch (m_dwType){
      case 0:  // sphere
      case 7:  // box
      case 3:  // cone, open on tpo
      case 4:  // cone, closed on top
      default:
         dwNum = 4;  // 0 = x, 1=y, 2=z, 3=combo
         break;
      case 5:  // disc, also cone
      case 8:  // plane
         dwNum = 3;  // 0 = x, 1=y, 2=combo
         break;
      case 6:  // two cones
         dwNum = 5;  // 0 = x, 1=y, 2=top, 3=bottom, 4=combo
         break;
      case 1:  // cylinder without cap
      case 2:  // cylinder with cap
         dwNum = 6;  // 0 = topx, 1=topy, 2=topz, 3=bottomx, 4=bottomy, 5=combo
         break;
      case 9:  // taurus
         dwNum = 5;  // 0 = rad x, 1=rady, 2=extrude x, 3=extrude y, 4=combo
         break;
      case 10: // cylinder, rounded edges
         dwNum = 7;  // 012 = xyz top, 345=xyz bottom, 6=combo
         break;
      }
      else {
         dwNum = m_lVertexInGroup.Num();  // for each control point

         // if more than more morph target active then no CP for dragging edges
         if (m_lMorphRemapID.Num() > 1)
            dwNum = 0;
      }

      for (i = 0; i < dwNum; i++)
         plDWORD->Add (&i);
   }
   else if (m_dwDisplayControl == 4) { // merge sides
      dwNum = m_lVertexInGroup.Num();  // for each control point

      // BUGFIX - If only one side left then cant remove
      if (m_lCPMSide.Num() < 2)
         dwNum = 0;

      for (i = 0; i < dwNum; i++)
         plDWORD->Add (&i);
   }
   else if ((m_dwDisplayControl == 2) || (m_dwDisplayControl == 3)) { // divide the side, or show/hide side
      dwNum = m_lSideInGroup.Num();  // for each control point

      for (i = 0; i < dwNum; i++)
         plDWORD->Add (&i);
   }
   else if (m_dwDisplayControl == 0) { // scale
      dwNum = 4;
      for (i = 0; i < dwNum; i++)
         plDWORD->Add (&i);
   }

   // as long as have no m_dwDivisions (which means custom) AND we're not
   // displaying scale, then also add CP for switching sections
   if (!m_dwDivisions && m_dwDisplayControl) {
      DWORD *padw = (DWORD*) m_lVertexGroup.Get(0);
      dwNum = m_lVertexGroup.Num();
      DWORD dw;
      for (i = 0; i < dwNum; i++) {
         // if it exists in the vertex in group displayed then don't show this
         // since would only clutter stuff up
         if (DWORDSearch (padw[i], m_lVertexInGroup.Num(), (DWORD*)m_lVertexInGroup.Get(0)) != -1)
            continue;

         // use
         dw = i + MAXVERTGROUP;
         plDWORD->Add (&dw);
      }
   }
}


/**********************************************************************************
CObjectPolyMeshOld::CalcScale - Fills m_pScaleXXX with scale information
*/
void CObjectPolyMeshOld::CalcScale (void)
{
   // NOTE: Not bothering remember undo and redo since this is automatically calculated

   DWORD dwNum;
   PCPMVertOld pv;
   dwNum = m_lCPMVert.Num();
   pv = (PCPMVertOld) m_lCPMVert.Get(0);

   DWORD i;
   for (i = 0; i < dwNum; i++, pv++) {
      if (!i) {
         m_pScaleMin.Copy (&pv->m_pLoc);
         m_pScaleMax.Copy (&pv->m_pLoc);
      }
      else {
         m_pScaleMin.Min (&pv->m_pLoc);
         m_pScaleMax.Max (&pv->m_pLoc);
      }

   }

   // Because of symmetry reasons, scale is always from 0,0,0
   m_pScaleSize.Zero();
   m_pScaleSize.Max (&m_pScaleMax);
   m_pScaleMax.Scale(-1);
   m_pScaleSize.Max (&m_pScaleMax);
   m_pScaleSize.Max (&m_pScaleMin);
   m_pScaleMin.Scale(-1);
   m_pScaleSize.Max (&m_pScaleMin);
   m_pScaleSize.Scale (2.0);  // so cover all directions
   for (i = 0; i < 3; i++)
      m_pScaleSize.p[i] = max(CLOSE, m_pScaleSize.p[i]);
}

/**********************************************************************************
CObjectPolyMeshOld::PointToVerteces - Given a point in the object's coordinates,
and an acceptable distance, this finds all the vertecies in that distance from
the point. It ALSO takes into account symmetry.

inputs
   PCPoint     pPoint - Point to look for -  NOTE: affect m_pLoc coords, not m_pLocWithDeform
   fp          fDist - Acceptable distance
   PCListFixed plVert - Initialized to sizeof(DWORD) and filled with vertex indecies
returns
   none
*/
void CObjectPolyMeshOld::PointToVertices (PCPoint pPoint, fp fDist, PCListFixed plVert)
{
   plVert->Init (sizeof(DWORD));
   plVert->Clear();

   // loop through all the verties
   DWORD dwNum = m_lCPMVert.Num();
   PCPMVertOld pv = (PCPMVertOld) m_lCPMVert.Get(0);

   DWORD i, j;
   fp fCalc;
   CPoint pDist, pSym;
   pSym.Copy (pPoint);
   for (i = 0; i < dwNum; i++) {
      // see if need to adjust for symmetry options
      if (m_dwSymmetry) {
         pSym.Copy (pPoint);

         for (j = 0; j < 3; j++)
            if (m_dwSymmetry & (1 << j)) {
               if ((fabs(pv[i].m_pLoc.p[j]) < CLOSE) || (fabs(pPoint->p[j]) < CLOSE)) {
                  continue;   // either on the zero line then no symmetry adjust
               }

               if (pv[i].m_pLoc.p[j] * pPoint->p[j] < 0) {
                  pSym.p[j] *= -1;
               }
            }
      }

      pDist.Subtract (&pv[i].m_pLoc, &pSym);
      fCalc = pDist.Length();

      if (fCalc <= fDist)
         plVert->Add (&i);
   }

   // done
}


/**********************************************************************************
CObjectPolyMeshOld::VertecesToSides - Given a list of verteces, this fills in
a list with a list of sides (polygons) that the verteces occur. NOTE: Duplicates will
be eliminated.

inputs
   DWORD*      padwVertex - Pointer to list of vertex indecies
   DWORD       dwNumVertex - Number of vertecies
   PCListFixed plSides - Initialized to sizeof(DWORD). Filled with list of sides.
returns
   none
*/
void CObjectPolyMeshOld::VerticesToSides (DWORD *padwVertex, DWORD dwNumVertex, PCListFixed plSides)
{
   // init
   plSides->Init (sizeof(DWORD));
   plSides->Clear();

   // create a tree to store all the sides
   CBTree tSides;

   // find out current list of sides
   PCPMSideOld ps = (PCPMSideOld) m_lCPMSide.Get(0);
   DWORD dwNum = m_lCPMSide.Num();

   // loop over all the incoming vertices
   DWORD i,j, k;
   WCHAR szTemp[32];
   for (i = 0; i < dwNumVertex; i++) {
      // loop over all the sides
      for (j = 0; j < dwNum; j++) {
         // loop over all the corners
         for (k = 0; k < 4; k++)
            if (ps[j].m_adwVert[k] == padwVertex[i])
               break;
         if (k >= 4)
            continue;   // not found

         // else, matched a side, so add it
         swprintf (szTemp, L"%d", j);
         if (tSides.Find(szTemp))
            continue;   // already in the tree
         tSides.Add (szTemp, &j, sizeof(j));
      } // j
   } // i

   // enumerate all the sides we found
   for (i = 0; i < tSides.Num(); i++) {
      PWSTR psz = tSides.Enum (i);
      j = (DWORD) _wtoi (psz);
      plSides->Add (&j);
   }

   // done
}


/**********************************************************************************
CObjectPolyMeshOld::SidesAddDetail - Given a list of sides, this splits them into 4
triangles by divide each edge.

NOTE: This does NOT clal ObjectAboutToChange()

inputs
   DWORD          *padwSide - Pointer to a list of side indecies
   DWORD          dwNumSide - Number of sides in the list
   BOOL           fDivideAll - If TRUE, divides all edges of the side. If FALSE,
                     intelligently divides only the longest.
returns
   none
*/
typedef struct {
   DWORD    adwVertex[2];     // two vertices, [0] is lowest value, [1] is highest value
   DWORD    adwSide[2];       // two sides attached to vertex (there should only be two). -1 if empty
   DWORD    adwOppositeVertex[2];   // opposite vertex from each adwSide[x]
   CPoint   pMid;             // where to split
   TEXTUREPOINT tpMid;        // where to split
   DWORD    dwMidVertex;      // new vertex number
} EDGEINFO, *PEDGEINFO;

void CObjectPolyMeshOld::SidesAddDetail (DWORD *padwSide, DWORD dwNumSide, BOOL fDivideAll)
{
   // side into
   PCPMSideOld psPMSIDE = (PCPMSideOld) m_lCPMSide.Get(0);
   DWORD dwPMSIDENum = m_lCPMSide.Num();
   PCPMVertOld psPMVERT = (PCPMVertOld) m_lCPMVert.Get(0);
   DWORD dwPMVERTNum = m_lCPMVert.Num();

   // make sure normals are calculated
   CalcMainNormals ();

   // find the longest side
   DWORD i, j;
   PCPMSideOld ps;
   CPoint pDist;
   fp fLongest, fLen;
   fLongest = 0;
   for (i = 0; i < dwNumSide; i++) {
      ps = &psPMSIDE[padwSide[i]];

      DWORD dwNum;
      dwNum = ps->Num();
      for (j = 0; j < dwNum; j++) {
         pDist.Subtract (&psPMVERT[ps->m_adwVert[j]].m_pLoc, &psPMVERT[ps->m_adwVert[(j+1)%dwNum]].m_pLoc);
         fLen = pDist.Length();
         fLongest = max (fLen, fLongest);
      }
   }
   fLongest /= 2; // add detail if side is 1/2 longest side or greater


   // make a list of edges
   CBTree tEdge;
   PWSTR gpszString = L"%d-%d";
   EDGEINFO ei, *pei;
   WCHAR szTemp[32];
   memset (&ei, 0, sizeof(ei));
   ei.adwSide[0] = ei.adwSide[1] = -1;
   ei.adwOppositeVertex[0] = ei.adwOppositeVertex[1] = -1;
   for (i = 0; i < dwNumSide; i++) {
      ps = &psPMSIDE[padwSide[i]];

      DWORD dwNum;
      dwNum = ps->Num();
      for (j = 0; j < dwNum; j++) {
         // if this is half the length of the longest side then don't split it
         pDist.Subtract (&psPMVERT[ps->m_adwVert[j]].m_pLoc, &psPMVERT[ps->m_adwVert[(j+1)%dwNum]].m_pLoc);
         fLen = pDist.Length();
         if (!fDivideAll && (fLen <= fLongest))
            continue;

         ei.adwVertex[0] = min(ps->m_adwVert[j], ps->m_adwVert[(j+1)%dwNum]);
         ei.adwVertex[1] = max(ps->m_adwVert[j], ps->m_adwVert[(j+1)%dwNum]);

         // see if can find in the list already
         swprintf (szTemp, gpszString, (int)ei.adwVertex[0], (int)ei.adwVertex[1]);
         pei = (PEDGEINFO) tEdge.Find (szTemp);
         if (pei) {
            // already exists, so set the second member of side
            if (pei->adwSide[1] != -1)
               continue;
            pei->adwSide[1] = padwSide[i];
            pei->adwOppositeVertex[1] = (j+dwNum-1)%dwNum;
         }
         else {
            // BUGFIX - Moved this to here because was causing problems where only
            // half an edge was done
            // if there are too many points then skip
            if (tEdge.Num() + m_lCPMVert.Num() > MAXVERT)
               continue;

            // doesn't exist, so set the first member of side and add
            ei.adwSide[0] = padwSide[i];
            ei.adwSide[1] = -1;  // just in case not another side
            ei.adwOppositeVertex[0] = (j+dwNum-1)%dwNum;
            tEdge.Add (szTemp, &ei, sizeof(ei));
         }
      } // over j
   }  // over i

   // go through all the edges and calculate the midpoint
   PWSTR psz;
   for (i = 0; i < tEdge.Num(); i++) {
      psz = tEdge.Enum(i);
      pei = (PEDGEINFO) tEdge.Find (psz);
      if (!pei)
         continue;

#if 0 // old code
      // find the mid point
      CPoint pStart, pEnd, pMid, pNormStart, pNormEnd;
      TEXTUREPOINT tpStart, tpEnd, tpMid;
      pStart.Copy (&psPMVERT[pei->adwVertex[0]].pLoc);
      pNormStart.Copy (&psPMVERT[pei->adwVertex[0]].pNorm);
      tpStart = psPMVERT[pei->adwVertex[0]].tText;
      pEnd.Copy (&psPMVERT[pei->adwVertex[1]].pLoc);
      pNormEnd.Copy (&psPMVERT[pei->adwVertex[1]].pNorm);
      tpEnd = psPMVERT[pei->adwVertex[1]].tText;
      pMid.Average (&pStart, &pEnd);
      tpMid.h = (tpStart.h + tpEnd.h) / 2.0;
      tpMid.v = (tpStart.v + tpEnd.v) / 2.0;

      CPoint pNorm;
      pNorm.Copy (&psPMSIDE[pei->adwSide[0]].pNorm);
      if (pei->adwSide[1] != -1) {
         pNorm.Average (&psPMSIDE[pei->adwSide[1]].pNorm);
      }

      // intersect line from pMid in pNorm direction with the normal of the start
      CPoint pMidNorm, pInterStart, pInterEnd;
      BOOL fInterStart, fInterEnd;
      pMidNorm.Add (&pMid, &pNorm);
      fInterStart = IntersectLinePlane (&pMid, &pMidNorm, &pStart, &pNormStart, &pInterStart);
      fInterEnd = IntersectLinePlane (&pMid, &pMidNorm, &pEnd, &pNormEnd, &pInterEnd);

      // average the two intersection points
      CPoint pInter;
      if (fInterStart && fInterEnd)
         pInter.Average (&pInterStart, &pInterEnd);
      else if (fInterStart)
         pInter.Copy (&pInterStart);
      else if (fInterEnd)
         pInter.Copy (&pInterEnd);
      else
         pInter.Copy (&pMid);

      // average this with the mid point so get some curvature
      pInter.Average (&pMid);

//addmid:
      // use this
      pei->pMid.Copy (&pMid);
      pei->tpMid = tpMid;
#endif // 0 - old code

      // add this to the list of vertices
      pei->dwMidVertex = m_lCPMVert.Num();

      CPMVertOld pv;
      memset (&pv, 0, sizeof(pv));
      pv.HalfWay (&psPMVERT[pei->adwVertex[0]], &psPMVERT[pei->adwVertex[1]], pei->adwSide[0], FALSE);

      // if there's another side then use that one also
      if (pei->adwSide[1] != -1) {
         PTEXTUREPOINT p1, p2;
         TEXTUREPOINT pAvg;
         p1 = psPMVERT[pei->adwVertex[0]].TextureGet (pei->adwSide[1]);
         p2 = psPMVERT[pei->adwVertex[1]].TextureGet (pei->adwSide[1]);
         pAvg.h = (p1->h + p2->h) / 2.0;
         pAvg.v = (p1->v + p2->v) / 2.0;
         if (!AreClose(&pAvg, pv.TextureGet()))
            pv.TextureSet (pei->adwSide[1], &pAvg);
      }


      //pv.pLoc.Copy (&pMid);
      //pv.tText = tpMid;
      m_fDivDirty = TRUE;
      m_lCPMVert.Add (&pv);

      // refresh the vertex pointer info
      psPMVERT = (PCPMVertOld) m_lCPMVert.Get(0);
      dwPMVERTNum = m_lCPMVert.Num();
   }  // over i

   // loop through all the sides and split them up, if necessary.
   // NOTE - Keeping dwPMSIDENum the same even though adding new sides,
   // so that don't bother checking sides that already exist
   for (i = 0; i < dwPMSIDENum; i++) {
      // get psPMSIDE every time since may have changed
      psPMSIDE = (PCPMSideOld) m_lCPMSide.Get(i);

      // see which edges are split
      DWORD adwSplit[4], adwSplitCorner[4], dwNumSplit;
      adwSplit[0] = adwSplit[1] = adwSplit[2] = adwSplit[3] = -1;
      dwNumSplit = 0;
      DWORD dwNum;
      dwNum = psPMSIDE->Num();
      for (j = 0; j < dwNum; j++) {
         DWORD dwMin, dwMax;
         dwMin = min (psPMSIDE->m_adwVert[j], psPMSIDE->m_adwVert[(j+1)%dwNum]);
         dwMax = max (psPMSIDE->m_adwVert[j], psPMSIDE->m_adwVert[(j+1)%dwNum]);
         swprintf (szTemp, gpszString, (int) dwMin, (int)dwMax);
         PEDGEINFO pei;
         pei = (PEDGEINFO) tEdge.Find (szTemp);
         if (!pei)
            continue;

         // found a split
         adwSplit[j] = pei->dwMidVertex;
         adwSplitCorner[dwNumSplit] = j;
         dwNumSplit++;
      }

      if (!dwNumSplit)
         continue;   // no change

      DWORD dwAdd;
      dwAdd = 0;  // number of sides to add

      // split the triangle up
      CPMSideOld as[4];
      memset (as, 0, sizeof(as));
      for (j = 0; j < 4; j++) {
         as[j].m_fHide = psPMSIDE->m_fHide;

         // set last one to -1 since always have at least triangle
         as[j].m_adwVert[3] = -1;
      }

      if (dwNum == 3) { // triangle
         if (dwNumSplit == 1) {
            DWORD dwCorner = adwSplitCorner[0];
            as[0].m_adwVert[0] = psPMSIDE->m_adwVert[(dwCorner+1)%dwNum];
            as[0].m_adwVert[1] = psPMSIDE->m_adwVert[(dwCorner+2)%dwNum];
            as[0].m_adwVert[2] = adwSplit[dwCorner];

            // set it
            psPMSIDE->m_adwVert[(dwCorner+1)%dwNum] = adwSplit[dwCorner];
            dwAdd = 1;
         }
         else if (dwNumSplit == 2) {
            // find the corner that is not split
            DWORD dwCorner;
            for (j = 0; j < 3; j++)
               if (adwSplit[j] == -1)
                  break;
            if (j >= 3)
               continue;   // shouldnt happen
            dwCorner = j;

            // create the first new triangle
            as[0].m_adwVert[0] = psPMSIDE->m_adwVert[dwCorner];
            as[0].m_adwVert[1] = adwSplit[(dwCorner+1)%dwNum];
            as[0].m_adwVert[2] = adwSplit[(dwCorner+2)%dwNum];

            // and the second triangle
            as[1].m_adwVert[0] = adwSplit[(dwCorner+1)%dwNum];
            as[1].m_adwVert[1] = psPMSIDE->m_adwVert[(dwCorner+2)%dwNum];
            as[1].m_adwVert[2] = adwSplit[(dwCorner+2)%dwNum];

            // modify existing triangle
            psPMSIDE->m_adwVert[(dwCorner+2)%dwNum] = adwSplit[(dwCorner+1)%dwNum];

            // add the otehr two
            dwAdd = 2;
         }
         else {   // dwNumSplit == 3
            // all three are split, so it's relatively easy

            // first one
            as[0].m_adwVert[0] = psPMSIDE->m_adwVert[0];
            as[0].m_adwVert[1] = adwSplit[0];
            as[0].m_adwVert[2] = adwSplit[2];

            // second one
            as[1].m_adwVert[0] = adwSplit[0];
            as[1].m_adwVert[1] = psPMSIDE->m_adwVert[1];
            as[1].m_adwVert[2] = adwSplit[1];

            // third one
            as[2].m_adwVert[0] = adwSplit[1];
            as[2].m_adwVert[1] = psPMSIDE->m_adwVert[2];
            as[2].m_adwVert[2] = adwSplit[2];

            // and the central fourth one
            psPMSIDE->m_adwVert[0] = adwSplit[0];
            psPMSIDE->m_adwVert[1] = adwSplit[1];
            psPMSIDE->m_adwVert[2] = adwSplit[2];

            // add the otehr three
            dwAdd = 3;
         }
      } // triangle
      else {   // quad
         // determine if need to calculate center point and add that
         BOOL fNeedCenter;
         fNeedCenter = (dwNumSplit > 2);
         // if have 2 points then need a center IF the points are adjacent
         if (dwNumSplit == 2)
            fNeedCenter = ((adwSplitCorner[0]%2) != (adwSplitCorner[1] % 2));

         // if nedd to create center vertex then do so
         DWORD dwCenter;
         dwCenter = -1;
         if (fNeedCenter) {
            CPMVertOld pLeft, pRight, pCenter;
            psPMVERT = (PCPMVertOld) m_lCPMVert.Get(0);
            dwPMVERTNum = m_lCPMVert.Num();

            // find half-way points on left and right
            pLeft.HalfWay (&psPMVERT[psPMSIDE->m_adwVert[0]],
               &psPMVERT[psPMSIDE->m_adwVert[1]],
               i, FALSE);
            pRight.HalfWay (&psPMVERT[psPMSIDE->m_adwVert[2]],
               &psPMVERT[psPMSIDE->m_adwVert[3]],
               i, FALSE);

            // average normals
            pLeft.m_pNorm.Add (&psPMVERT[psPMSIDE->m_adwVert[0]].m_pNorm,
               &psPMVERT[psPMSIDE->m_adwVert[1]].m_pNorm);
            pRight.m_pNorm.Add (&psPMVERT[psPMSIDE->m_adwVert[2]].m_pNorm,
               &psPMVERT[psPMSIDE->m_adwVert[3]].m_pNorm);
            pLeft.m_pNorm.Normalize();
            pRight.m_pNorm.Normalize();

            // combine those two
            pCenter.HalfWay (&pLeft, &pRight, i, FALSE);
            pLeft.Release();
            pRight.Release();

            // NOTE: Don't need to worry about multiple textures for center point
            // because will all be the same

            // add the center
            dwCenter = dwPMVERTNum;
            m_lCPMVert.Add (&pCenter);

            // refresh the vertex pointer info
            psPMVERT = (PCPMVertOld) m_lCPMVert.Get(0);
            dwPMVERTNum = m_lCPMVert.Num();
         }  // fneedcenter

         // which triangles?
         if (dwNumSplit == 1) {
            DWORD dwCorner = adwSplitCorner[0];
            as[0].m_adwVert[0] = psPMSIDE->m_adwVert[(dwCorner+1)%dwNum];
            as[0].m_adwVert[1] = psPMSIDE->m_adwVert[(dwCorner+2)%dwNum];
            as[0].m_adwVert[2] = adwSplit[dwCorner];

            as[1].m_adwVert[0] = psPMSIDE->m_adwVert[(dwCorner+3)%dwNum];
            as[1].m_adwVert[1] = psPMSIDE->m_adwVert[dwCorner];
            as[1].m_adwVert[2] = adwSplit[dwCorner];

            // set it
            psPMSIDE->m_adwVert[0] = adwSplit[dwCorner];
            psPMSIDE->m_adwVert[1] = as[0].m_adwVert[1];
            psPMSIDE->m_adwVert[2] = as[1].m_adwVert[0];
            psPMSIDE->m_adwVert[3] = -1;

            dwAdd = 2;
         }
         else if (dwNumSplit == 2) {
            if (dwCenter == -1) {   // 2 points on opposite sides, split down middle
               DWORD dwCorner = adwSplitCorner[0];
               as[0].m_adwVert[0] = adwSplit[dwCorner];
               as[0].m_adwVert[1] = psPMSIDE->m_adwVert[(dwCorner+1)%dwNum];
               as[0].m_adwVert[2] = psPMSIDE->m_adwVert[(dwCorner+2)%dwNum];
               as[0].m_adwVert[3] = adwSplit[(dwCorner+2)%dwNum];

               // set it
               psPMSIDE->m_adwVert[(dwCorner+1)%dwNum] = adwSplit[dwCorner];
               psPMSIDE->m_adwVert[(dwCorner+2)%dwNum] = adwSplit[(dwCorner+2)%dwNum];

               dwAdd = 1;
            }
            else {   // two points next to each other
               DWORD dwA = adwSplitCorner[0];
               DWORD dwB = adwSplitCorner[1];
               if (((dwA+1)%dwNum) != dwB) { // so dwSecond is always to right
                  DWORD dw = dwA;
                  dwA = dwB;
                  dwB = dw;
               }

               as[0].m_adwVert[0] = adwSplit[dwB];
               as[0].m_adwVert[1] = psPMSIDE->m_adwVert[(dwB+1)%dwNum];
               as[0].m_adwVert[2] = psPMSIDE->m_adwVert[(dwB+2)%dwNum];
               as[0].m_adwVert[3] = dwCenter;

               as[1].m_adwVert[0] = dwCenter;
               as[1].m_adwVert[1] = psPMSIDE->m_adwVert[(dwB+2)%dwNum];
               as[1].m_adwVert[2] = psPMSIDE->m_adwVert[(dwB+3)%dwNum];
               as[1].m_adwVert[3] = adwSplit[dwA];

               psPMSIDE->m_adwVert[0] = psPMSIDE->m_adwVert[dwB];
               psPMSIDE->m_adwVert[1] = adwSplit[dwB];
               psPMSIDE->m_adwVert[2] = dwCenter;
               psPMSIDE->m_adwVert[3] = adwSplit[dwA];

               dwAdd = 2;
            }
         }
         else if (dwNumSplit == 3) {
            // find out which one not split
            DWORD dwWhole;
            for (dwWhole = 0; dwWhole < 4; dwWhole++)
               if (adwSplit[dwWhole] == -1)
                  break;

            // make several tirangles and quads
            as[0].m_adwVert[0] = adwSplit[(dwWhole+3)%dwNum];
            as[0].m_adwVert[1] = psPMSIDE->m_adwVert[dwWhole];
            as[0].m_adwVert[2] = dwCenter;

            as[1].m_adwVert[0] = psPMSIDE->m_adwVert[dwWhole];
            as[1].m_adwVert[1] = psPMSIDE->m_adwVert[(dwWhole+1)%dwNum];
            as[1].m_adwVert[2] = dwCenter;

            as[2].m_adwVert[0] = psPMSIDE->m_adwVert[(dwWhole+1)%dwNum];
            as[2].m_adwVert[1] = adwSplit[(dwWhole+1)%dwNum];
            as[2].m_adwVert[2] = dwCenter;

            as[3].m_adwVert[0] = adwSplit[(dwWhole+1)%dwNum];
            as[3].m_adwVert[1] = psPMSIDE->m_adwVert[(dwWhole+2)%dwNum];
            as[3].m_adwVert[2] = adwSplit[(dwWhole+2)%dwNum];
            as[3].m_adwVert[3] = dwCenter;

            // change
            psPMSIDE->m_adwVert[0] = psPMSIDE->m_adwVert[(dwWhole+3)%dwNum];
            psPMSIDE->m_adwVert[1] = adwSplit[(dwWhole+3)%dwNum];
            psPMSIDE->m_adwVert[2] = dwCenter;
            psPMSIDE->m_adwVert[3] = adwSplit[(dwWhole+2)%dwNum];

            // add
            dwAdd = 4;
         }
         else {   // dwNumSplit == 4
            as[0].m_adwVert[0] = psPMSIDE->m_adwVert[1];
            as[0].m_adwVert[1] = adwSplit[1];
            as[0].m_adwVert[2] = dwCenter;
            as[0].m_adwVert[3] = adwSplit[0];

            as[1].m_adwVert[0] = psPMSIDE->m_adwVert[2];
            as[1].m_adwVert[1] = adwSplit[2];
            as[1].m_adwVert[2] = dwCenter;
            as[1].m_adwVert[3] = adwSplit[1];

            as[2].m_adwVert[0] = psPMSIDE->m_adwVert[3];
            as[2].m_adwVert[1] = adwSplit[3];
            as[2].m_adwVert[2] = dwCenter;
            as[2].m_adwVert[3] = adwSplit[2];

            psPMSIDE->m_adwVert[1] = adwSplit[0];
            psPMSIDE->m_adwVert[2] = dwCenter;
            psPMSIDE->m_adwVert[3] = adwSplit[3];

            // add
            dwAdd = 3;
         } // dwNumSplit
      }  // quad

      // make sure all the vertices have the right texture associated
      // basically, if find any textures that differ in the current triangle
      // from what will be setting it as then divide up
      PCPMVertOld pvNew;
      DWORD dwWillBe, k;
      PTEXTUREPOINT pAll, pThis;
      pvNew = (PCPMVertOld) m_lCPMVert.Get(0);
      for (j = 0; j < dwAdd; j++) {
         dwWillBe = j + m_lCPMSide.Num();// what side number will this be?
         for (k = 0; k < 4; k++) {
            if (as[j].m_adwVert[k] == -1) {
#ifdef _DEBUG
               if (k < 3)
                  dwWillBe = 1;
#endif
               continue;   // not a point
            }
            pAll = pvNew[as[j].m_adwVert[k]].TextureGet();
            pThis = pvNew[as[j].m_adwVert[k]].TextureGet(i);
            if ((pAll != pThis) && !AreClose(pAll, pThis))
               pvNew[as[j].m_adwVert[k]].TextureSet (dwWillBe, pThis);
         }
      }

      // add
      for (j = 0; j < dwAdd; j++)
         m_lCPMSide.Add (&as[j]);
   }  // over all sides

   // done
}

typedef struct {
   PCObjectPolyMeshOld    pv;
   WCHAR          szAttrib[64];       // filled in by ObjEditorAttribSel with the attribute selected
   DWORD          dwEdit;           // filled in to indicate which one is being edited
   int            iVScroll;         // so ObjEditorAttribEditPage retursn to the right place
} MDPAGE, *PMDPAGE;

/* PolyMeshDialogPage
*/
BOOL PolyMeshDialogPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PMDPAGE po = (PMDPAGE)pPage->m_pUserData;
   PCObjectPolyMeshOld pv = po->pv;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         PCEscControl pControl;

         if (pv->m_pLastMoved.p[3] == -1) {
            pControl = pPage->ControlFind (L"adddetailregion");
            if (pControl)
               pControl->Enable (FALSE);
         }

         pControl = pPage->ControlFind (L"backface");
         if (pControl)
            pControl->AttribSetBOOL (Checked(), pv->m_fCanBackface);

         pControl = pPage->ControlFind (L"mergesmoothing");
         if (pControl)
            pControl->AttribSetBOOL (Checked(), pv->m_fMergeSmoothing);

         ComboBoxSet (pPage, L"smooth", pv->m_dwSubdivide);

         // fill in the list box
         pPage->Message (ESCM_USER+82);
      }
      break;

   case ESCM_USER+82:
      {
         // fill in the list box
         MemZero (&gMemTemp);
         DWORD i, dwNum;
         PCOEAttrib *pap;
         dwNum = pv->m_lPCOEAttrib.Num();
         pap = (PCOEAttrib*) pv->m_lPCOEAttrib.Get(0);
         for (i = 0; i < dwNum; i++) {
            MemCat (&gMemTemp, L"<elem name=");
            MemCat (&gMemTemp, (int) i);
            MemCat (&gMemTemp, L">");

            // name
            MemCat (&gMemTemp, L"<bold>");
            MemCatSanitize (&gMemTemp, pap[i]->m_szName);
            MemCat (&gMemTemp, L"</bold>");
            if (pap[i]->m_dwType == 0)
               MemCat (&gMemTemp, L" <italic>(Combo-morph)</italic>");
            MemCat (&gMemTemp, L"<br/>");

            // description and other bits
            MemCat (&gMemTemp, L"<p parindent=32 wrapindent=32>");
            if ((pap[i]->m_memInfo.p) && ((PWSTR)pap[i]->m_memInfo.p)[0]) {
               MemCatSanitize (&gMemTemp, (PWSTR)pap[i]->m_memInfo.p);
               //MemCat (&gMemTemp, L"<p/>");
            }

#if 0
            // what attributes it effects
            DWORD dwElem;
            PCOEATTRIBCOMBO pc;
            DWORD j;
            pc = (PCOEATTRIBCOMBO) pap[i]->m_lCOEATTRIBCOMBO.Get(0);
            dwElem = pap[i]->m_lCOEATTRIBCOMBO.Num();
            for (j = 0; j < dwElem; j++, pc++) {
               // get the object name
               DWORD dwFind;
               PWSTR pszName;
               PCObjectSocket pos = NULL;
               dwFind = pv->m_pWorld->ObjectFind (&pc->gFromObject);
               if (dwFind != -1)
                  pos = pv->m_pWorld->ObjectGet (dwFind);
               if (!pos)
                  continue;
               pszName = pos->StringGet (OSSTRING_NAME);
               if (!pszName)
                  continue;

               // write name
               MemCatSanitize (&gMemTemp, pszName);
               MemCat (&gMemTemp, L" : ");
               MemCatSanitize (&gMemTemp, pc->szName);
               if (j+1 < dwElem)
                  MemCat (&gMemTemp, L"<br/>");
            }
#endif // 0

            MemCat (&gMemTemp, L"</p>");

            MemCat (&gMemTemp, L"</elem>");
         }

         // add
         ESCMLISTBOXADD la;
         PCEscControl pControl;
         pControl = pPage->ControlFind (L"morph");
         memset (&la, 0, sizeof(la));
         la.dwInsertBefore = -1;
         la.pszMML = (PWSTR) gMemTemp.p;
         if (pControl)
            pControl->Message (ESCM_LISTBOXRESETCONTENT, NULL);
         if (la.pszMML[0] && pControl) {
            pControl->Message (ESCM_LISTBOXADD, &la);
            pControl->AttribSet (CurSel(), 0);
         }
      }
      return TRUE;

   case ESCN_COMBOBOXSELCHANGE:
      {
         PESCNCOMBOBOXSELCHANGE p = (PESCNCOMBOBOXSELCHANGE) pParam;
         if (!p->pControl->m_pszName)
            break;

         // if the major ID changed then pick the first minor and texture that
         // comes to mind
         if (!wcsicmp(p->pControl->m_pszName, L"smooth")) {
            DWORD dwVal = p->pszName ? (DWORD)_wtoi(p->pszName) : 0;

            // if it hasn't reall change then ignore
            if (dwVal == pv->m_dwSubdivide)
               return TRUE;

            pv->m_pWorld->ObjectAboutToChange (pv);
            pv->m_dwSubdivide = dwVal;
            pv->m_fDivDirty = TRUE;
            pv->m_pWorld->ObjectChanged (pv);

            return TRUE;
         }
      }
      break;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;
         PWSTR psz;
         psz = p->pControl->m_pszName;

         if (!wcsicmp(psz, L"addmorphcombo")) {
            // if not morphs added then cant do this
            if (!NumMorphIDs(&pv->m_lPCOEAttrib)) {
               pPage->MBWarning (L"You must first add some morphs before you can add combo-morphs.");
               return TRUE;
            }

            pPage->Exit (L"addmorphcombo");
            return TRUE;
            }
         else if (!wcsicmp(psz, L"removemorph")) {
            // get the one
            PCEscControl pControl = pPage->ControlFind (L"morph");
            if (!pControl)
               return TRUE;
            DWORD dwIndex;
            dwIndex = pControl->AttribGetInt (CurSel());

            // get the value
            ESCMLISTBOXGETITEM lgi;
            memset (&lgi, 0, sizeof(lgi));
            lgi.dwIndex = dwIndex;
            pControl->Message (ESCM_LISTBOXGETITEM, &lgi);
            if (!lgi.pszName) {
               pPage->MBWarning (L"You must select a morph in the list box.");
               return TRUE;
            }
            dwIndex = _wtoi (lgi.pszName);
            dwIndex = min(dwIndex, pv->m_lPCOEAttrib.Num()-1);

            // verify
            if (IDYES != pPage->MBYesNo (L"Are you sure you wish to delete the morph?",
               L"Deleting it will permenantly remove the attribute."))
               return TRUE;

            // remove it
            pv->m_pWorld->ObjectAboutToChange (pv);
            PCOEAttrib pa;
            pa = *((PCOEAttrib*) pv->m_lPCOEAttrib.Get(dwIndex));
            pv->m_lPCOEAttrib.Remove (dwIndex);
            if ((pa->m_dwType == 2) && pa->m_lCOEATTRIBCOMBO.Num()) {
               // which item is removed
               PCOEATTRIBCOMBO pc = (PCOEATTRIBCOMBO) pa->m_lCOEATTRIBCOMBO.Get(0);
               DWORD dwRemove = pc->dwMorphID;

               // remove this from all vertecies
               PCPMVertOld pVert;
               pVert = (PCPMVertOld) pv->m_lCPMVert.Get(0);
               DWORD i;
               for (i = 0; i < pv->m_lCPMVert.Num(); i++, pVert++)
                  pVert->DeformRemove (dwRemove);


               // loop through all entries
               for (i = pv->m_lPCOEAttrib.Num()-1; i < pv->m_lPCOEAttrib.Num(); i--) {
                  PCOEAttrib p = *((PCOEAttrib*) pv->m_lPCOEAttrib.Get(i));
                  DWORD j;
                  for (j = p->m_lCOEATTRIBCOMBO.Num()-1; j < p->m_lCOEATTRIBCOMBO.Num(); j--) {
                     pc = (PCOEATTRIBCOMBO) p->m_lCOEATTRIBCOMBO.Get(j);
                     if (pc->dwMorphID < dwRemove)
                        continue;   // less than, so no change
                     if (pc->dwMorphID > dwRemove) {
                        // greater than so decrease
                        pc->dwMorphID--;
                        continue;
                     }

                     // else, match, so remove this
                     p->m_lCOEATTRIBCOMBO.Remove (j);
                  } // j - over attribcombos

                  // if nothing left in the combo then remove the combo
                  if (!p->m_lCOEATTRIBCOMBO.Num()) {
                     // know that it's a combo so dont have to check that deleing
                     // this will interfere with anything
                     delete p;
                     pv->m_lPCOEAttrib.Remove (i);
                  }
               } // i - over attributes
            }  // if delete non-combo
            delete pa;
            pv->m_fDivDirty = TRUE; // since may change appearance
            pv->CalcMorphRemap(); // redo morph remap since may have changed
            pv->m_pWorld->ObjectChanged (pv);

            // dont need to resort since sort order still ok

            // refresh display
            pPage->Message (ESCM_USER+82);
            return TRUE;
         }
         else if (!wcsicmp(psz, L"addmorphmirrorlr") || !wcsicmp(psz, L"addmorphmirrorfb") || !wcsicmp(psz, L"addmorphmirrortb")) {
            DWORD dwDim;
            if (!wcsicmp(psz, L"addmorphmirrorlr"))
               dwDim = 0;
            else if (!wcsicmp(psz, L"addmorphmirrorfb"))
               dwDim = 1;
            else
               dwDim = 2;

            // get the one
            PCEscControl pControl = pPage->ControlFind (L"morph");
            if (!pControl)
               return TRUE;
            DWORD dwIndex;
            dwIndex = pControl->AttribGetInt (CurSel());

            // get the value
            ESCMLISTBOXGETITEM lgi;
            memset (&lgi, 0, sizeof(lgi));
            lgi.dwIndex = dwIndex;
            pControl->Message (ESCM_LISTBOXGETITEM, &lgi);
            if (!lgi.pszName) {
               pPage->MBWarning (L"You must select a morph in the list box.");
               return TRUE;
            }
            dwIndex = _wtoi (lgi.pszName);
            dwIndex = min(dwIndex, pv->m_lPCOEAttrib.Num()-1);

            // get this morph
            PCOEAttrib pa;
            pa = *((PCOEAttrib*) pv->m_lPCOEAttrib.Get(dwIndex));
            if (pa->m_dwType != 2) {
               pPage->MBWarning (L"You cannot mirror a combo-morph.");
               return TRUE;
            }

            // create a clone
            PCOEAttrib pNew;
            pNew = new COEAttrib;
            if (!pNew)
               return TRUE;
            pa->CloneTo (pNew);

            // unique name
            PWSTR pszMirror = L" (Mirror)";
            DWORD dwLenMirror = wcslen(pszMirror);
            pNew->m_szName[63-dwLenMirror] = 0; // so have enough space to append
            wcscat (pNew->m_szName, pszMirror);
            MakeAttribNameUnique (pNew->m_szName, &pv->m_lPCOEAttrib);

            // find the max
            DWORD dwMax;
            dwMax = NumMorphIDs (&pv->m_lPCOEAttrib);

            // this one will use that morh
            PCOEATTRIBCOMBO pc;
            DWORD dwOld;
            pc = (PCOEATTRIBCOMBO) pNew->m_lCOEATTRIBCOMBO.Get(0);
            dwOld = pc->dwMorphID;
            
            // add new attributes
            pv->m_pWorld->ObjectAboutToChange (pv);
            pc->dwMorphID = dwMax;
            dwMax++;
            pv->m_lPCOEAttrib.Add (&pNew);

            // loop through and do mirroring
            DWORD i,j,k, dwNum, dwNumVD;
            PCPMVertOld pVert;
            PVERTDEFORM pvd;
            VERTDEFORM vd;
            CPoint   pLoc;
            dwNum = pv->m_lCPMVert.Num();
            pVert = (PCPMVertOld) pv->m_lCPMVert.Get(0);
            for (i = 0; i < dwNum; i++) {
               // see if can find a copy of the old morph
               if (!pVert[i].m_plVERTDEFORM)
                  continue;
               pvd = (PVERTDEFORM) pVert[i].m_plVERTDEFORM->Get(0);
               dwNumVD = pVert[i].m_plVERTDEFORM->Num();
               for (k = 0; k < dwNumVD; k++)
                  if (pvd[k].dwDeform == dwOld)
                     break;
               if (k >= dwNumVD)
                  continue;   // didn't have the deformation in it

               // remember this
               vd = pvd[k];
               pLoc.Copy (&pVert[i].m_pLoc);
               vd.pDeform.p[dwDim] *= -1;
               vd.dwDeform = pc->dwMorphID;
               pLoc.p[dwDim] *= -1;

               // now find a match
               for (j = 0; j < dwNum; j++) {
                  if (pVert[j].m_pLoc.AreClose (&pLoc))
                     break;
               }
               if (j >= dwNum)
                  continue;   // no match found

               // else, have a match, so add it
               if (!pVert[j].m_plVERTDEFORM) {
                  pVert[j].m_plVERTDEFORM = new CListFixed;
                  if (pVert[j].m_plVERTDEFORM)
                     pVert[j].m_plVERTDEFORM->Init (sizeof(VERTDEFORM));
               }
               if (pVert[j].m_plVERTDEFORM)
                  pVert[j].m_plVERTDEFORM->Add (&vd);
            }

            // done
            pv->m_fDivDirty = TRUE; // since may change appearance
            qsort (pv->m_lPCOEAttrib.Get(0), pv->m_lPCOEAttrib.Num(), sizeof(PCOEAttrib), OECALCompare);
            pv->CalcMorphRemap(); // redo morph remap since may have changed
            pv->m_pWorld->ObjectChanged (pv);

            // refresh display
            pPage->Message (ESCM_USER+82);
            return TRUE;
         }
         else if (!wcsicmp(psz, L"editmorph")) {
            // get the one
            PCEscControl pControl = pPage->ControlFind (L"morph");
            if (!pControl)
               return TRUE;
            DWORD dwIndex;
            dwIndex = pControl->AttribGetInt (CurSel());

            // get the value
            ESCMLISTBOXGETITEM lgi;
            memset (&lgi, 0, sizeof(lgi));
            lgi.dwIndex = dwIndex;
            pControl->Message (ESCM_LISTBOXGETITEM, &lgi);
            if (!lgi.pszName) {
               pPage->MBWarning (L"You must select a morph in the list box.");
               return TRUE;
            }
            dwIndex = _wtoi (lgi.pszName);
            dwIndex = min(dwIndex, pv->m_lPCOEAttrib.Num()-1);

            po->dwEdit = dwIndex;
            pPage->Exit (L"editmorph");
            return TRUE;
         }
         else if (!wcsicmp(psz, L"adddetailregion")) {
            // how many points in the are
            CListFixed lFoundVert, lFoundSides;
            pv->PointToVertices (&pv->m_pLastMoved, pv->m_fDragSize, &lFoundVert);
            pv->VerticesToSides ((DWORD*) lFoundVert.Get(0), lFoundVert.Num(), &lFoundSides);
            if (!lFoundSides.Num()) {
               pPage->MBInformation (L"No sides were found.",
                  L"None of the control points were close enough. Drag a control point "
                  L"a short distance and try again.");
               return TRUE;
            }

            pv->m_pWorld->ObjectAboutToChange (pv);
            pv->SidesAddDetail ((DWORD*) lFoundSides.Get(0), lFoundSides.Num());
            pv->CalcVertexSideInGroup (pv->m_dwCurVertGroup);
            pv->m_pWorld->ObjectChanged (pv);

            pPage->MBSpeakInformation (L"Detail added.");

            return TRUE;
         }
         else if (!wcsicmp(psz, L"adddetailall")) {
            // create a list with all the sides
            CListFixed lSides;
            lSides.Init (sizeof(DWORD));
            DWORD i;
            for (i = 0; i < pv->m_lCPMSide.Num(); i++)
               lSides.Add (&i);

            pv->m_pWorld->ObjectAboutToChange (pv);
            pv->SidesAddDetail ((DWORD*) lSides.Get(0), lSides.Num());
            pv->CalcVertexSideInGroup (pv->m_dwCurVertGroup);
            pv->m_pWorld->ObjectChanged (pv);

            pPage->MBSpeakInformation (L"Detail added.");
            return TRUE;
         }
         else if (!wcsicmp(psz, L"flipx") || !wcsicmp(psz, L"flipy") || !wcsicmp(psz, L"flipz")) {
            DWORD dwDim;
            if (!wcsicmp(psz, L"flipx"))
               dwDim = 0;
            else if (!wcsicmp(psz, L"flipy"))
               dwDim = 1;
            else
               dwDim = 2;

            pv->m_pWorld->ObjectAboutToChange (pv);

            // create the rotation matrix
            CMatrix mScale, mScaleInvTrans;
            mScale.Scale ((dwDim==0) ? -1 : 1, (dwDim==1) ? -1 : 1, (dwDim==2) ? -1 : 1);
            mScale.Invert (&mScaleInvTrans);
            mScaleInvTrans.Transpose ();

            // rotate all the points
            DWORD i, dwNum;
            PCPMVertOld pVert;
            PCPMSideOld pSide;
            pVert = (PCPMVertOld) pv->m_lCPMVert.Get(0);
            pSide = (PCPMSideOld) pv->m_lCPMSide.Get(0);
            dwNum = pv->m_lCPMVert.Num();
            for (i = 0; i < dwNum; i++)
               pVert[i].Scale (&mScale, &mScaleInvTrans);
            
            // swap the order of the traingles
            dwNum = pv->m_lCPMSide.Num();
            for (i = 0; i < dwNum; i++)
               pSide[i].Reverse();

            // and some other bits
            pv->m_pLastMoved.p[3] = 1;
            pv->m_pLastMoved.MultiplyLeft (&mScale);

            pv->m_fDivDirty = TRUE;
            pv->CalcScale();
            pv->m_pWorld->ObjectChanged (pv);
            return TRUE;
         }
         else if (!wcsicmp(psz, L"textcylinder")) {
            pv->m_pWorld->ObjectAboutToChange (pv);
            pv->TextureCylindrical();
            pv->m_pWorld->ObjectChanged (pv);
            pPage->MBSpeakInformation (L"Texture applied.");
            return TRUE;
         }
         else if (!wcsicmp(psz, L"textsphere")) {
            pv->m_pWorld->ObjectAboutToChange (pv);
            pv->TextureSpherical();
            pv->m_pWorld->ObjectChanged (pv);
            pPage->MBSpeakInformation (L"Texture applied.");
            return TRUE;
         }
         else if (!wcsicmp(psz, L"textlinearfront")) {
            pv->m_pWorld->ObjectAboutToChange (pv);
            pv->TextureLinear (TRUE);
            pv->m_pWorld->ObjectChanged (pv);
            pPage->MBSpeakInformation (L"Texture applied.");
            return TRUE;
         }
         else if (!wcsicmp(psz, L"textlineartop")) {
            pv->m_pWorld->ObjectAboutToChange (pv);
            pv->TextureLinear (FALSE);
            pv->m_pWorld->ObjectChanged (pv);
            pPage->MBSpeakInformation (L"Texture applied.");
            return TRUE;
         }
         else if (!wcsicmp(psz, L"backface")) {
            pv->m_pWorld->ObjectAboutToChange (pv);
            pv->m_fCanBackface = p->pControl->AttribGetBOOL (Checked());
            pv->m_pWorld->ObjectChanged (pv);
            return TRUE;
         }
         else if (!wcsicmp(psz, L"mergesmoothing")) {
            pv->m_pWorld->ObjectAboutToChange (pv);
            pv->m_fMergeSmoothing = p->pControl->AttribGetBOOL (Checked());
            pv->m_pWorld->ObjectChanged (pv);
            return TRUE;
         }
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Polygon mesh (phase II) settings";
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}




/* PolyMeshDialog1Page - phase I
*/
BOOL PolyMeshDialog1Page (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCObjectPolyMeshOld pv = (PCObjectPolyMeshOld) pPage->m_pUserData;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         PCEscControl pControl;
         pControl = pPage->ControlFind (L"points");
         if (pControl)
            pControl->AttribSetInt (Pos(), (int)pv->m_dwDivisions);

         ComboBoxSet (pPage, L"smooth", pv->m_dwSubdivide);
      }
      break;

   case ESCN_SCROLL:
   case ESCN_SCROLLING:
      {
         PESCNSCROLL p = (PESCNSCROLL) pParam;

         // only do one scroll bar
         PWSTR psz;
         psz = p->pControl->m_pszName;
         if (!psz || wcsicmp(psz, L"points"))
            break;

         // get value
         DWORD dw;
         dw = (DWORD) p->pControl->AttribGetInt (Pos());
         dw = max(1,dw);   // always at least 1
         if (dw == pv->m_dwDivisions)
            break;   // no change

         pv->m_pWorld->ObjectAboutToChange (pv);
         pv->m_dwDivisions = dw;
         pv->m_fDivDirty = TRUE;
         pv->CreateBasedOnType();
         pv->m_pWorld->ObjectChanged (pv);
      }
      break;

   case ESCN_COMBOBOXSELCHANGE:
      {
         PESCNCOMBOBOXSELCHANGE p = (PESCNCOMBOBOXSELCHANGE) pParam;
         if (!p->pControl->m_pszName)
            break;

         // if the major ID changed then pick the first minor and texture that
         // comes to mind
         if (!wcsicmp(p->pControl->m_pszName, L"smooth")) {
            DWORD dwVal = p->pszName ? (DWORD)_wtoi(p->pszName) : 0;

            // if it hasn't reall change then ignore
            if (dwVal == pv->m_dwSubdivide)
               return TRUE;

            pv->m_pWorld->ObjectAboutToChange (pv);
            pv->m_dwSubdivide = dwVal;
            pv->m_fDivDirty = TRUE;
            pv->m_pWorld->ObjectChanged (pv);

            return TRUE;
         }
      }
      break;

   case ESCM_LINK:
      {
         PESCMLINK p = (PESCMLINK) pParam;
         if (p->psz && !wcsicmp(p->psz, L"convert")) {
            pv->m_pWorld->ObjectAboutToChange (pv);
            pv->m_dwDivisions = 0;  // so convert over
            pv->m_pWorld->ObjectChanged (pv);
         }
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Polygon mesh (phase I) settings";
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}


/* PolyMeshMorphEditPage
*/
BOOL PolyMeshMorphEditPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PMDPAGE po = (PMDPAGE)pPage->m_pUserData;
   PCObjectPolyMeshOld pv = po->pv;
   PCOEAttrib pa = *((PCOEAttrib*) pv->m_lPCOEAttrib.Get(po->dwEdit));

   switch (dwMessage) {

   case ESCM_INITPAGE:
      {
         PCEscControl pControl;

         // if we have somplace to scroll to then do so
         if (po->iVScroll >= 0) {
            pPage->VScroll (po->iVScroll);

            // when bring up pop-up dialog often they're scrolled wrong because
            // iVScoll was left as valeu, and they use defpage
            po->iVScroll = 0;

            // BUGFIX - putting this invalidate in to hopefully fix a refresh
            // problem when add or move a task in the ProjectView page
            pPage->Invalidate();
         }

         // edit
         pControl = pPage->ControlFind (L"name");
         if (pControl)
            pControl->AttribSet (Text(), pa->m_szName);

         pControl = pPage->ControlFind (L"desc");
         if (pControl && pa->m_memInfo.p && ((PWSTR)pa->m_memInfo.p)[0])
            pControl->AttribSet (Text(), (PWSTR)pa->m_memInfo.p);

         pPage->Message (ESCM_USER+82);   // set the values based on infotype

         // combo
         ComboBoxSet (pPage, L"infotype", pa->m_dwInfoType);

         // bools
         pControl = pPage->ControlFind (L"deflowrank");
         if (pControl)
            pControl->AttribSetBOOL (Checked(), pa->m_fDefLowRank);

         pControl = pPage->ControlFind (L"defpassup");
         if (pControl)
            pControl->AttribSetBOOL (Checked(), pa->m_fDefPassUp);

         // do all the from (t:) values
         WCHAR szTemp[32];
         DWORD i, j;
         DWORD dwNum;
         PCOEATTRIBCOMBO pc;
         pc = (PCOEATTRIBCOMBO) pa->m_lCOEATTRIBCOMBO.Get(0);
         dwNum = pa->m_lCOEATTRIBCOMBO.Num();
         for (i = 0; i < dwNum; i++, pc++) {
            // set the bomxo
            swprintf (szTemp, L"cb:%d", (int) i);
            ComboBoxSet (pPage, szTemp, pc->dwMorphID);

            // set limit controls
            swprintf (szTemp, L"c0:%d", (int) i);
            pControl = pPage->ControlFind (szTemp);
            if (pControl)
               pControl->AttribSetBOOL (Checked(), (pc->dwCapEnds & 0x01) ? TRUE : FALSE);
            swprintf (szTemp, L"c1:%d", (int) i);
            pControl = pPage->ControlFind (szTemp);
            if (pControl)
               pControl->AttribSetBOOL (Checked(), (pc->dwCapEnds & 0x02) ? TRUE : FALSE);

            DWORD dwFind;
            PCObjectSocket pos = NULL;
            ATTRIBINFO ai;
            memset (&ai, 0, sizeof(ai));
            dwFind = pv->m_pWorld->ObjectFind (&pc->gFromObject);
            if (dwFind != -1)
               pos = pv->m_pWorld->ObjectGet (dwFind);
            if (pos)
               pos->AttribInfo (pc->szName, &ai);

            for (j = 0; j < pc->dwNumRemap; j++) {
               swprintf (szTemp, L"t:%d%d", (int)j, (int)i);
               switch (ai.dwType) {
               case AIT_DISTANCE:
                  MeasureToString (pPage, szTemp, pc->afpObjectValue[j]);
                  break;
               case AIT_ANGLE:
                  AngleToControl (pPage, szTemp, pc->afpObjectValue[j]);
                  break;
               default:
                  DoubleToControl (pPage, szTemp, pc->afpObjectValue[j]);
                  break;
               }
            } // j
         } // i
      }
      break;

   case ESCM_USER+82:   // set the values based on m_dwInfoType
      {
         switch (pa->m_dwInfoType) {
         case AIT_DISTANCE:
            MeasureToString (pPage, L"min", pa->m_fMin);
            MeasureToString (pPage, L"max", pa->m_fMax);
            break;
         case AIT_ANGLE:
            AngleToControl (pPage, L"min", pa->m_fMin);
            AngleToControl (pPage, L"max", pa->m_fMax);
            break;
         default:
            DoubleToControl (pPage, L"min", pa->m_fMin);
            DoubleToControl (pPage, L"max", pa->m_fMax);
            break;
         }

         // do all the from (f:) values
         WCHAR szTemp[32];
         DWORD i, j;
         DWORD dwNum;
         PCOEATTRIBCOMBO pc;
         pc = (PCOEATTRIBCOMBO) pa->m_lCOEATTRIBCOMBO.Get(0);
         dwNum = pa->m_lCOEATTRIBCOMBO.Num();
         for (i = 0; i < dwNum; i++, pc++) {
            for (j = 0; j < pc->dwNumRemap; j++) {
               swprintf (szTemp, L"f:%d%d", (int)j, (int)i);
               switch (pa->m_dwInfoType) {
               case AIT_DISTANCE:
                  MeasureToString (pPage, szTemp, pc->afpComboValue[j]);
                  break;
               case AIT_ANGLE:
                  AngleToControl (pPage, szTemp, pc->afpComboValue[j]);
                  break;
               default:
                  DoubleToControl (pPage, szTemp, pc->afpComboValue[j]);
                  break;
               }
            }
         }
      }
      break;

   case ESCN_EDITCHANGE:
      {
         PESCNEDITCHANGE p = (PESCNEDITCHANGE) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;
         PWSTR psz;
         psz = p->pControl->m_pszName;

         if ((psz[0] == L'f') && (psz[1] == L':')) {
            // change the from field
            DWORD i, j;
            PCOEATTRIBCOMBO pc;
            j = psz[2] - L'0';
            i = _wtoi(psz + 3);
            pc = (PCOEATTRIBCOMBO) pa->m_lCOEATTRIBCOMBO.Get(i);
            if (!pc)
               return TRUE;
            if (j >= pc->dwNumRemap)
               return TRUE;

            pv->m_pWorld->ObjectAboutToChange (pv);

            switch (pa->m_dwInfoType) {
            case AIT_DISTANCE:
               MeasureParseString (pPage, p->pControl->m_pszName, &pc->afpComboValue[j]);
               break;
            case AIT_ANGLE:
               pc->afpComboValue[j] = AngleFromControl (pPage, p->pControl->m_pszName);
               break;
            default:
               pc->afpComboValue[j] = DoubleFromControl (pPage, p->pControl->m_pszName);
               break;
            }
            pv->m_pWorld->ObjectChanged (pv);
            return TRUE;
         }
         else if ((psz[0] == L't') && (psz[1] == L':')) {
            // change the from field
            DWORD i, j;
            PCOEATTRIBCOMBO pc;
            j = psz[2] - L'0';
            i = _wtoi(psz + 3);
            pc = (PCOEATTRIBCOMBO) pa->m_lCOEATTRIBCOMBO.Get(i);
            if (!pc)
               return TRUE;
            if (j >= pc->dwNumRemap)
               return TRUE;

            DWORD dwFind;
            PCObjectSocket pos;
            ATTRIBINFO ai;
            pos = NULL;
            memset (&ai, 0, sizeof(ai));
            dwFind = pv->m_pWorld->ObjectFind (&pc->gFromObject);
            if (dwFind != -1)
               pos = pv->m_pWorld->ObjectGet (dwFind);
            if (pos)
               pos->AttribInfo (pc->szName, &ai);

            pv->m_pWorld->ObjectAboutToChange (pv);
            switch (ai.dwType) {
            case AIT_DISTANCE:
               MeasureParseString (pPage, p->pControl->m_pszName, &pc->afpObjectValue[j]);
               break;
            case AIT_ANGLE:
               pc->afpObjectValue[j] = AngleFromControl (pPage, p->pControl->m_pszName);
               break;
            default:
               pc->afpObjectValue[j] = DoubleFromControl (pPage, p->pControl->m_pszName);
               break;
            }
            pv->m_pWorld->ObjectChanged (pv);
            return TRUE;
         }

         // else cheap - since one edit changed just get all the values at once
         PCEscControl pControl;
         DWORD dwNeeded;

         pv->m_pWorld->ObjectAboutToChange (pv);
         // edit
         pControl = pPage->ControlFind (L"name");
         if (pControl) {
            pControl->AttribGet (Text(), pa->m_szName, sizeof(pa->m_szName), &dwNeeded);
         }

         pControl = pPage->ControlFind (L"desc");
         if (pControl && pa->m_memInfo.Required (256 * 2)) {
            ((PWSTR)pa->m_memInfo.p)[0] = 0; // so if attribget fails have null
            pControl->AttribGet (Text(), (PWSTR)pa->m_memInfo.p, pa->m_memInfo.m_dwAllocated, &dwNeeded);
         }

         switch (pa->m_dwInfoType) {
         case AIT_DISTANCE:
            MeasureParseString (pPage, L"min", &pa->m_fMin);
            MeasureParseString (pPage, L"max", &pa->m_fMax);
            break;
         case AIT_ANGLE:
            pa->m_fMin = AngleFromControl (pPage, L"min");
            pa->m_fMax = AngleFromControl (pPage, L"max");
            break;
         default:
            pa->m_fMin = DoubleFromControl (pPage, L"min");
            pa->m_fMax = DoubleFromControl (pPage, L"max");
            break;
         }
         pv->m_pWorld->ObjectChanged (pv);

      }
      break;

   case ESCN_COMBOBOXSELCHANGE:
      {
         PESCNCOMBOBOXSELCHANGE p = (PESCNCOMBOBOXSELCHANGE) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;
         PWSTR psz;
         psz = p->pControl->m_pszName;

         if (!wcsicmp(psz, L"infotype")) {
            DWORD dw;
            dw = p->pszName ? (DWORD)_wtoi(p->pszName) : 0;
            if (dw == pa->m_dwInfoType)
               break;   // no change

            pv->m_pWorld->ObjectAboutToChange (pv);
            pa->m_dwInfoType = dw;
            pv->m_pWorld->ObjectChanged (pv);

            // redisplay the values
            pPage->Message (ESCM_USER+82);   // set the values based on infotype
            return TRUE;
         }
         else if ((psz[0] == L'c') && (psz[1] == L'b') && (psz[2] == L':')) {
            DWORD dw = p->pszName ? (DWORD)_wtoi(p->pszName) : 0;
            DWORD dwElem = _wtoi(psz + 3);
            if (dwElem >= pa->m_lCOEATTRIBCOMBO.Num())
               return FALSE;

            // get the value
            PCOEATTRIBCOMBO pc = (PCOEATTRIBCOMBO) pa->m_lCOEATTRIBCOMBO.Get(dwElem);
            if (pc->dwMorphID == dw)
               return TRUE;   // nothing to do

            pv->m_pWorld->ObjectAboutToChange (pv);
            pc->dwMorphID = dw;
            pv->m_pWorld->ObjectChanged (pv);
            pPage->Exit (RedoSamePage());
            return TRUE;
         }
      }
      break;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;
         PWSTR psz;
         psz = p->pControl->m_pszName;


         if (psz[0] == L'r' && psz[1] == L'c' && psz[2] == L':') {
            DWORD i;
            i = _wtoi(psz + 3);
            PCOEATTRIBCOMBO pc;
            pc = (PCOEATTRIBCOMBO) pa->m_lCOEATTRIBCOMBO.Get(i);
            if (!pc)
               return TRUE;

            if (IDYES != pPage->MBYesNo (L"Are you sure you want to remove the effect?"))
               return TRUE;

            pv->m_pWorld->ObjectAboutToChange (pv);
            pa->m_lCOEATTRIBCOMBO.Remove (i);
            pv->m_pWorld->ObjectChanged (pv);
            pPage->Exit (RedoSamePage());
            return TRUE;
         }
         else if (psz[0] == L'a' && psz[1] == L'm' && psz[2] == L':') {
            DWORD i;
            i = _wtoi(psz + 3);
            PCOEATTRIBCOMBO pc;
            pc = (PCOEATTRIBCOMBO) pa->m_lCOEATTRIBCOMBO.Get(i);
            if (!pc)
               return TRUE;

            pv->m_pWorld->ObjectAboutToChange (pv);
            pc->afpComboValue[pc->dwNumRemap] = pc->afpComboValue[pc->dwNumRemap-1];
            pc->afpObjectValue[pc->dwNumRemap] = pc->afpObjectValue[pc->dwNumRemap-1];
            pc->dwNumRemap++;
            pv->m_pWorld->ObjectChanged (pv);

            pPage->Exit (RedoSamePage());
            return TRUE;
         }
         else if (psz[0] == L'r' && psz[1] == L'm' && psz[2] == L':') {
            DWORD i;
            i = _wtoi(psz + 3);
            PCOEATTRIBCOMBO pc;
            pc = (PCOEATTRIBCOMBO) pa->m_lCOEATTRIBCOMBO.Get(i);
            if (!pc)
               return TRUE;

            pv->m_pWorld->ObjectAboutToChange (pv);
            pc->dwNumRemap--;
            pv->m_pWorld->ObjectChanged (pv);

            pPage->Exit (RedoSamePage());
            return TRUE;
         }
         else if ((psz[0] == L'c') && ((psz[1] == L'0') || (psz[1] == L'1')) && (psz[2] == L':')) {
            DWORD i, j;
            PCOEATTRIBCOMBO pc;
            j = psz[1] - L'0';
            i = _wtoi(psz + 3);
            pc = (PCOEATTRIBCOMBO) pa->m_lCOEATTRIBCOMBO.Get(i);
            if (!pc)
               return TRUE;

            pv->m_pWorld->ObjectAboutToChange (pv);
            if (p->pControl->AttribGetBOOL (Checked())) {
               pc->dwCapEnds |= (1 << j);
            }
            else {
               pc->dwCapEnds &= ~(1<<j);
            }
            pv->m_pWorld->ObjectChanged (pv);

            return TRUE;
         }
         else if (!wcsicmp(psz, L"deflowrank")) {
            pv->m_pWorld->ObjectAboutToChange (pv);
            pa->m_fDefLowRank = p->pControl->AttribGetBOOL (Checked());
            pv->m_pWorld->ObjectChanged (pv);
            return TRUE;
         }
         else if (!wcsicmp(psz, L"defpassup")) {
            pv->m_pWorld->ObjectAboutToChange (pv);
            pa->m_fDefPassUp = p->pControl->AttribGetBOOL (Checked());
            pv->m_pWorld->ObjectChanged (pv);
            return TRUE;
         }
         else if (!wcsicmp(psz, L"addatt")) {
            pv->m_pWorld->ObjectAboutToChange (pv);

            COEATTRIBCOMBO c;
            memset (&c, 0, sizeof(c));
            c.afpComboValue[0] = c.afpObjectValue[0] = 0;
            c.afpComboValue[1] = c.afpObjectValue[1] = 1;
            c.dwNumRemap = 2;
            c.dwMorphID = 0;  // just start out with default value that can change later
            pa->m_lCOEATTRIBCOMBO.Add (&c);
            pv->m_pWorld->ObjectChanged (pv);
            pPage->Exit (RedoSamePage());
            return TRUE;
         }
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Modify a morph";
            return TRUE;
         }
         else if (!wcsicmp(p->pszSubName, L"ATTRIBLIST")) {
            // only do this for combo attributes
            if (pa->m_dwType != 0)
               return FALSE;

            MemZero (&gMemTemp);

            MemCat (&gMemTemp, L"<xbr/><xSectionTitle>Affected sub-morphs</xSectionTitle>"
               L"<p>"
               L"Whenever you adjust the combo-morph's attribute, the polygon mesh will "
               L"in turn adjust one or more \"sub\" morphs within the mesh. You can use to "
               L"to combine two or more morphs into one easy slider. For example: If you "
               L"have a \"left eyebrow\" morp and a \"right eyebrow\" morph, you can make "
               L"a combo-morph called \"both eyebrows\" that uses both the left and right eyebrows as sub-morphs, so "
               L"that both move with the same slider."
               L"</p>"
               );

            // define a macro
            DWORD i, j;
            DWORD dwFind;
            PCOEAttrib *pap = (PCOEAttrib*) pv->m_lPCOEAttrib.Get(0);
            PCOEATTRIBCOMBO pc;
            DWORD dwNum;
            MemCat (&gMemTemp, L"<!xComboMorph>"
               L"<combobox width=50%% cbheight=150 macroattribute=1>");
            for (j = 0; j < pv->m_lPCOEAttrib.Num(); j++) {
               if (pap[j]->m_dwType != 2)
                  continue;   // only care about built in
               dwNum = pap[j]->m_lCOEATTRIBCOMBO.Num();
               pc = (PCOEATTRIBCOMBO) pap[j]->m_lCOEATTRIBCOMBO.Get(0);
               if (!dwNum)
                  continue;   // not enough
               dwFind = pc[0].dwMorphID;
               MemCat (&gMemTemp, L"<elem name=");
               MemCat (&gMemTemp, (int) dwFind);
               MemCat (&gMemTemp, L">");
               MemCatSanitize (&gMemTemp, pap[j]->m_szName);
               MemCat (&gMemTemp, L"</elem>");
            }
            MemCat (&gMemTemp, L"</combobox>"
               L"</xComboMorph>");

            dwNum = pa->m_lCOEATTRIBCOMBO.Num();
            pc = (PCOEATTRIBCOMBO) pa->m_lCOEATTRIBCOMBO.Get(0);
            for (i = 0; i < dwNum; i++, pc++) {
               // get the name
               PWSTR pszName;
               pszName = NULL;
               for (j = 0; j < pv->m_lPCOEAttrib.Num(); j++) {
                  if (pap[j]->m_dwType != 2)
                     continue;   // only care about built in
                  if (!pap[j]->m_lCOEATTRIBCOMBO.Num())
                     continue;   // not enough
                  PCOEATTRIBCOMBO p2;
                  p2 = (PCOEATTRIBCOMBO) pap[j]->m_lCOEATTRIBCOMBO.Get(0);
                  dwFind = p2->dwMorphID;
                  if (dwFind == pc->dwMorphID) {
                     pszName = pap[j]->m_szName;
                     break;
                  }
               }
               if (!pszName) {
                  pszName =  L"Unknown";
                  dwFind = 0;
               }

               MemCat (&gMemTemp, L"<xtablecenter width=100%%>");
               MemCat (&gMemTemp, L"<xtrheader>");
               MemCat (&gMemTemp, L"<xComboMorph name=cb:");
               MemCat (&gMemTemp, (int)i);
               MemCat (&gMemTemp, L"/>");
               MemCat (&gMemTemp, L"</xtrheader>");
               MemCat (&gMemTemp, L"<tr><td>");
               MemCat (&gMemTemp, L"Below is a table that describes how changes to "
                  L"the current attribute morph <italic>");
               MemCatSanitize (&gMemTemp, pszName);
               MemCat (&gMemTemp, L"</italic>. The left column is the value of the edited combo-morph "
                  L"and the right is what the value is changed to for the sub-morph.<p/>");
               MemCat (&gMemTemp, L"<p align=center><table width=66%%>");
               MemCat (&gMemTemp, L"<tr><td><bold>This</bold></td><td><bold>");
               MemCatSanitize (&gMemTemp, pszName);
               MemCat (&gMemTemp, L"</bold></td></tr>");
               for (j = 0; j < pc->dwNumRemap; j++) {
                  MemCat (&gMemTemp, L"<tr>");
                  MemCat (&gMemTemp, L"<td><edit maxchars=32 width=100% selall=true name=f:");
                  MemCat (&gMemTemp, (int) j);  // since only one char
                  MemCat (&gMemTemp, (int) i);
                  MemCat (&gMemTemp, L"/></td>");

                  MemCat (&gMemTemp, L"<td><edit maxchars=32 width=100% selall=true name=t:");
                  MemCat (&gMemTemp, (int) j);  // since only one char
                  MemCat (&gMemTemp, (int) i);
                  MemCat (&gMemTemp, L"/></td>");
                  MemCat (&gMemTemp, L"</tr>");
               }

               // button for add
               if (pc->dwNumRemap + 1 < COEMAXREMAP) {
                  MemCat (&gMemTemp, L"<tr><td><xChoiceButton style=righttriangle name=am:");
                  MemCat (&gMemTemp, (int) i);
                  MemCat (&gMemTemp, L"><bold>Add another row</bold><br/>"
                     L"Adds a row to the end of the list, providing you with more "
                     L"control over how changing this combo-morph affect the sub-morph."
                     L"</xChoiceButton></td></tr>");
               }
               if (pc->dwNumRemap > 2) {
                  MemCat (&gMemTemp, L"<tr><td><xChoiceButton style=righttriangle name=rm:");
                  MemCat (&gMemTemp, (int) i);
                  MemCat (&gMemTemp, L"><bold>Remove last row</bold><br/>"
                     L"Removes the last row."
                     L"</xChoiceButton></td></tr>");
               }

               MemCat (&gMemTemp, L"</table></p>");

               MemCat (&gMemTemp, L"<xChoiceButton checkbox=true style=x name=c0:");
               MemCat (&gMemTemp, (int) i);
               MemCat (&gMemTemp, L"><bold>Limit low values</bold><br/>"
                  L"If checked, then the morph attribute cannot be set below the minimum value "
                  L"(first row, left column). If unchecked, the morph attribute can be set "
                  L"below the minimum value shown."
                  L"</xChoiceButton>");

               MemCat (&gMemTemp, L"<xChoiceButton checkbox=true style=x name=c1:");
               MemCat (&gMemTemp, (int) i);
               MemCat (&gMemTemp, L"><bold>Limit high values</bold><br/>"
                  L"If checked, then the morph attribute cannot be set above the maximum value "
                  L"(last row, left column). If unchecked, the morph attribute can be set "
                  L"above the maximum value shown."
                  L"</xChoiceButton>");

               if (dwNum > 1) {
                  MemCat (&gMemTemp, L"<xChoiceButton style=rightarrow name=rc:");
                  MemCat (&gMemTemp, (int) i);
                  MemCat (&gMemTemp, L"><bold>Delete this effect</bold><br/>"
                     L"If you do this changing this attribute will no longer affect ");
                  MemCatSanitize (&gMemTemp, pszName);
                  MemCat (&gMemTemp, L".</xChoiceButton>");
               }
               MemCat (&gMemTemp, L"</td></tr>");
               MemCat (&gMemTemp, L"</xtablecenter>");
            }

            if (NumMorphIDs(&pv->m_lPCOEAttrib)) {
               MemCat (&gMemTemp, L"<xChoiceButton name=addatt>"
                  L"<bold>Affect an additonal sub-morph</bold><br/>"
                  L"Press this to have this combo-morph affect one or more other "
                  L"sub-morphs.</xChoiceButton>");
            }
            p->pszSubString = (PWSTR) gMemTemp.p;
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}


/**********************************************************************************
CObjectPolyMeshOld::DialogShow - Causes the object to show a dialog box that allows
different attributes to be changed.

inputs
   DWORD             dwSuface - Surface that was clicked on. 0 if unknown.
   PCEscWindow       pWindow - Escarpment window to draw it in
returns
   BOOL - TRUE if user pressed "Back", FALSE if user pressed "Close"
*/
BOOL CObjectPolyMeshOld::DialogShow (DWORD dwSurface, PCEscWindow pWindow)
{
   PWSTR pszRet;
   DWORD i;
   MDPAGE oe;
   memset (&oe, 0, sizeof(oe));
   oe.pv = this;
redo:
   if (m_dwDivisions)
      pszRet = pWindow->PageDialog (ghInstance, IDR_MMLPOLYMESHDIALOG1, PolyMeshDialog1Page, this);
   else
      pszRet = pWindow->PageDialog (ghInstance, IDR_MMLPOLYMESHDIALOG, PolyMeshDialogPage, &oe);
   if (!pszRet)
      return FALSE;
   if (!wcsicmp(pszRet, L"editmorph")) {
      oe.iVScroll = 0;
edit:
      pszRet = pWindow->PageDialog (ghInstance, IDR_MMLPOLYMESHMORPHEDIT, PolyMeshMorphEditPage, &oe);

      m_pWorld->ObjectAboutToChange (this);
      m_fDivDirty = TRUE; // since may change appearance

      // make sure the name is unique
      PCOEAttrib pa;
      pa = *((PCOEAttrib*) m_lPCOEAttrib.Get(oe.dwEdit));
      MakeAttribNameUnique (pa->m_szName, &m_lPCOEAttrib, oe.dwEdit);

      // repick th edit
      PCOEAttrib *pap;
      pap = (PCOEAttrib*) m_lPCOEAttrib.Get(0);
      for (i = 0; i < m_lPCOEAttrib.Num(); i++)
         if (pap[i] == pa)
            break;
      oe.dwEdit = i;

      // make sure the mapped values are sequential
      PCOEATTRIBCOMBO pc;
      DWORD i,j;
      pc = (PCOEATTRIBCOMBO) pa->m_lCOEATTRIBCOMBO.Get(0);
      for (i = 0; i < pa->m_lCOEATTRIBCOMBO.Num(); i++, pc++) {
         for (j = 1; j < pc->dwNumRemap; j++)
            pc->afpComboValue[j] = max(pc->afpComboValue[j], pc->afpComboValue[j-1]);
      }

      m_pWorld->ObjectChanged (this);

      if (!pszRet)
         return FALSE;
      if (!wcsicmp(pszRet, Back()))
         goto redo;
      oe.iVScroll = pWindow->m_iExitVScroll;
      if (pszRet && !wcsicmp(pszRet, RedoSamePage()))
         goto edit;

      
   // else, fall through
   }
   else if (!wcsicmp(pszRet, L"addmorph") || !wcsicmp(pszRet, L"addmorphcombo")) {
      // create a new one
      PCOEAttrib pNew = new COEAttrib;
      if (!pNew)
         return FALSE;
      BOOL fCombo;
      fCombo = !wcsicmp(pszRet, L"addmorphcombo");
      pNew->m_dwInfoType = AIT_NUMBER;
      pNew->m_dwType = fCombo ? 0 : 2;  // just a morph
      pNew->m_fAutomatic = FALSE;
      pNew->m_fCurValue = 0;
      pNew->m_fDefLowRank = FALSE;
      pNew->m_fDefPassUp = TRUE;
      pNew->m_fDefValue = 0;
      pNew->m_fMax = 1;
      pNew->m_fMin = 0;
      wcscpy (pNew->m_szName, fCombo ? L"Combo-morph" : L"New morph");
      MakeAttribNameUnique (pNew->m_szName, &m_lPCOEAttrib);

      COEATTRIBCOMBO ac;
      memset (&ac, 0, sizeof(ac));
      ac.afpComboValue[0] = ac.afpObjectValue[0] = 0;
      ac.afpComboValue[1] = ac.afpObjectValue[1] = 1;
      ac.dwCapEnds = 0;
      ac.dwNumRemap = 2;
      ac.dwMorphID = fCombo ? 0 : NumMorphIDs(&m_lPCOEAttrib);

      pNew->m_lCOEATTRIBCOMBO.Add (&ac);

      // add to list
      m_pWorld->ObjectAboutToChange (this);
      m_lPCOEAttrib.Add (&pNew);

      // Sort m_lPCOEAttrib list
      qsort (m_lPCOEAttrib.Get(0), m_lPCOEAttrib.Num(), sizeof(PCOEAttrib), OECALCompare);

      // world changed
      m_pWorld->ObjectChanged (this);

      PCOEAttrib *pap;
      pap = (PCOEAttrib*) m_lPCOEAttrib.Get(0);
      for (i = 0; i < m_lPCOEAttrib.Num(); i++)
         if (pap[i] == pNew)
            break;
      oe.dwEdit = i;

      oe.iVScroll = 0;
      goto edit;
   }
   else if (!wcsicmp(pszRet, L"convert"))
      goto redo;

   return !wcsicmp(pszRet, Back());
}


/**********************************************************************************
CObjectPolyMeshOld::DialogQuery - Returns TRUE if the object supports a custom
dialog box, FALSE if it doesn't. The default object doesn't
*/
BOOL CObjectPolyMeshOld::DialogQuery (void)
{
   return TRUE;
}

/*************************************************************************************
CObjectPolyMeshOld::WipeExtraText - Wipes out the extra textures stored away and generated
by FixWraparoundTextures()
*/
void CObjectPolyMeshOld::WipeExtraText(void)
{
   DWORD i;
   PCPMVertOld pv = (PCPMVertOld) m_lCPMVert.Get(0);
   DWORD dwNumV = m_lCPMVert.Num();

   for (i = 0; i < dwNumV; i++, pv++)
      pv->TextureClear();
}

/*************************************************************************************
CObjectPolyMeshOld::FixWraparoundTextures - Assumes that a sperical or cylindrical texutre
was placed using X and Y. This then looks for wrap-around segments. If it finds any it
will add extra texture information so it draws properly on the boundaries.

NOTE: This assumes values in H from -PI to PI
*/
void CObjectPolyMeshOld::FixWraparoundTextures (void)
{
   // find all the edges
   CListFixed lEdge;
   EnumEdges (&m_lCPMSide, &lEdge);

   // get list of points
   PCPMVertOld pv, p1, p2, pTemp;
   DWORD dwNumVert;
   pv = (PCPMVertOld) m_lCPMVert.Get(0);
   dwNumVert = m_lCPMVert.Num();

   // loop through all the edges and make sure going in the right direction
   DWORD i, j;
   PPMEDGE ppe;
   BOOL fClockwise;
   CPoint pClock, pN1, pN2;
   DWORD dwV1, dwV2, dw;
   ppe = (PPMEDGE) lEdge.Get(0);
   for (i = 0; i < lEdge.Num(); i++, ppe++) {
      p1 = &pv[dwV1 = ppe->adwVert[0]];
      p2 = &pv[dwV2 = ppe->adwVert[1]];
      pN1.Copy (&p1->m_pLoc);
      pN1.Normalize();
      pN2.Copy (&p2->m_pLoc);
      pN2.Normalize();

      // direction of the vector
      pClock.CrossProd (&pN1, &pN2);
      // NOTE: There is no center to the object other than 0,0,0, so not subtracting center from m_pLoc

      // is it clockwise
      if (pClock.p[2] > CLOSE*10)   // make larger so no problem with roundoff error
         fClockwise = TRUE;
      else if (pClock.p[2] < -CLOSE*10)
         fClockwise = FALSE;
      else
         continue;   // not enough to tell

      // if it doesn't end up being in a clockwise direction flip the order
      // of the points so it is going around clockwise
      if (!fClockwise) {
         pTemp = p1;
         p1 = p2;
         p2 = pTemp;

         dw = dwV1;
         dwV1 = dwV2;
         dwV2 = dw;
      }

      // loop over all sides attached to the edge
      for (j = 0; j < 2; j++) {
         DWORD dwSide = ppe->adwSide[j];
         if (dwSide == -1)
            continue;

         // make sure that angle is positive
         fp fDelta;
         PTEXTUREPOINT pt1, pt2;
         pt1 = p1->TextureGet(dwSide);
         pt2 = p2->TextureGet(dwSide);
         fDelta = pt2->h - pt1->h;
         if (fDelta >= 0.0)
            continue;   // clockwise, so don't change

         // else, need to change one of them. Can either add 2PI to the
         // one on the right, or subtract 2PI from the left
         TEXTUREPOINT t2, t1;
         if (fabs(pt2->h + 2*PI) < fabs(pt1->h - 2 * PI)) {
            // if already modified then don't modify again
            if (pt2 != p2->TextureGet())
               continue;
            t2 = *pt2;
            t2.h += 2 * PI;
            p2->TextureSet (dwSide, &t2);

#ifdef _DEBUG
            char  szTemp[128];
            sprintf (szTemp, "Vert %d, side %d: %g to %g, %g\r\n",
               (int) dwV2, (int) dwSide, (double) pt2->h, (double)t2.h, (double) pt2->v);
            OutputDebugString (szTemp);
#endif
         }
         else {
            // if already modified then don't modify again
            if (pt1 != p1->TextureGet())
               continue;
            t1 = *pt1;
            t1.h -= 2 * PI;
            p1->TextureSet (dwSide, &t1);
         }
      }
   }
}

/*************************************************************************************
CObjectPolyMeshOld::TextureSpherical - Create a spherical texture map.

NOTE: Doesn't call m_pWorld->ObjectChanged
*/
void CObjectPolyMeshOld::TextureSpherical (void)
{
   DWORD i;
   PCPMVertOld pv = (PCPMVertOld) m_lCPMVert.Get(0);
   DWORD dwNumV = m_lCPMVert.Num();

   m_fDivDirty = TRUE;  // BUGFIX - So will cause a redraw

   // figure out radius from scalesize
   fp fRad;
   fRad = (m_pScaleSize.p[0] + m_pScaleSize.p[1]) / 4.0;

   for (i = 0; i < dwNumV; i++, pv++) {
      CPoint p;
      p.Copy (&pv->m_pLoc);

      // use the scale to undo for ellipsoids
      //p.p[0] /= (m_pScaleSize.p[0] / 2.0);
      //p.p[1] /= (m_pScaleSize.p[1] / 2.0);
      //p.p[2] /= (m_pScaleSize.p[2] / 2.0);

      // point
      pv->m_tText.h = atan2 (p.p[0], -p.p[1]);  // leave scaling out for now: * fRad * 2;
      if (pv->m_tText.h >= PI - CLOSE)
         pv->m_tText.h -= 2 * PI;   // so don't get case where have jagged boundary
      pv->m_tText.v = -atan2 (p.p[2], sqrt(p.p[0]*p.p[0] + p.p[1] * p.p[1])) * fRad * 2;
   }

   // fix wrapapound effect
   WipeExtraText ();
   FixWraparoundTextures ();

   // loop back and scale
   pv = (PCPMVertOld) m_lCPMVert.Get(0);
   DWORD j;
   for (i = 0; i < dwNumV; i++, pv++) {
      pv->m_tText.h *= fRad * 2;
      if (pv->m_plSIDETEXT) {
         PSIDETEXT pst = (PSIDETEXT) pv->m_plSIDETEXT->Get(0);
         for (j = 0; j < pv->m_plSIDETEXT->Num(); j++)
            pst[j].tp.h *= fRad * 2;
      }
   }
}



/*************************************************************************************
CObjectPolyMeshOld::TextureCylindrical - Create a spherical texture map.

NOTE: Doesn't call m_pWorld->ObjectChanged
*/
void CObjectPolyMeshOld::TextureCylindrical (void)
{
   DWORD i;
   PCPMVertOld pv = (PCPMVertOld) m_lCPMVert.Get(0);
   DWORD dwNumV = m_lCPMVert.Num();

   m_fDivDirty = TRUE;  // BUGFIX - So will cause a redraw

   // figure out radius from scalesize
   fp fRad;
   fRad = (m_pScaleSize.p[0] + m_pScaleSize.p[1]) / 4.0;

   for (i = 0; i < dwNumV; i++, pv++) {
      CPoint p;
      p.Copy (&pv->m_pLoc);

      // use the scale to undo for ellipsoids
      //p.p[0] /= (m_pScaleSize.p[0] / 2.0);
      //p.p[1] /= (m_pScaleSize.p[1] / 2.0);

      // point
      pv->m_tText.h = atan2 (p.p[0], -p.p[1]);  // leave scaling until later * fRad * 2;
      if (pv->m_tText.h >= PI - CLOSE)
         pv->m_tText.h -= 2 * PI;   // so don't get case where have jagged boundary

      pv->m_tText.v = -p.p[2];
   }

   // fix wrapapound effect
   WipeExtraText ();
   FixWraparoundTextures ();

   // loop back and scale
   pv = (PCPMVertOld) m_lCPMVert.Get(0);
   DWORD j;
   for (i = 0; i < dwNumV; i++, pv++) {
      pv->m_tText.h *= fRad * 2;
      if (pv->m_plSIDETEXT) {
         PSIDETEXT pst = (PSIDETEXT) pv->m_plSIDETEXT->Get(0);
         for (j = 0; j < pv->m_plSIDETEXT->Num(); j++)
            pst[j].tp.h *= fRad * 2;
      }
   }
}

/*************************************************************************************
CObjectPolyMeshOld::TextureLinear - Linear projection from the front of above

inputs
   BOOL     fFront - If TRUE then project from the front, otherwise from above

NOTE: Doesn't call m_pWorld->ObjectChanged
*/
void CObjectPolyMeshOld::TextureLinear (BOOL fFront)
{
   DWORD i;
   PCPMVertOld pv = (PCPMVertOld) m_lCPMVert.Get(0);
   DWORD dwNumV = m_lCPMVert.Num();

   m_fDivDirty = TRUE;  // BUGFIX - So will cause a redraw

   WipeExtraText ();

   for (i = 0; i < dwNumV; i++, pv++) {
      if (fFront) {
         pv->m_tText.h = pv->m_pLoc.p[0];
         pv->m_tText.v = -pv->m_pLoc.p[2];
      }
      else {
         pv->m_tText.h = pv->m_pLoc.p[0];
         pv->m_tText.v = -pv->m_pLoc.p[1];
      }
   }
}


/*************************************************************************************
CObjectPolyMeshOld::EnumEdges - This is passed in a PCListFixed of sides. It fills in
a list of PMEDGE, sorted by start and end vertex. (Where the start vertex < end vertex.)
Used for subdivision of the polygon mesh.

inputs
   PCListFixed       plCPMSide - List of side data to create edges from
   PCListFixed       plPMEDGE - Filled in with edge data
   BOOL              fBiDi - If TRUE the generated list is bi-directional, which means
                     that have two copies of each vertex: one where start < end, the
                     other where end < start. The bi-directional one makes it easier
                     to find all the connections between a given point and all other
                     points.
returns
   none
*/

static int _cdecl PMEDGECompare (const void *elem1, const void *elem2)
{
   PMEDGE *pdw1, *pdw2;
   pdw1 = (PMEDGE*) elem1;
   pdw2 = (PMEDGE*) elem2;

   if (pdw1->adwVert[0] != pdw2->adwVert[0])
      return (int) pdw1->adwVert[0] - (int) pdw2->adwVert[0];

   // else, next in line
   return (int) pdw1->adwVert[1] - (int) pdw2->adwVert[1];
}
void CObjectPolyMeshOld::EnumEdges (PCListFixed plCPMSide, PCListFixed plPMEDGE, BOOL fBiDi)
{
   plPMEDGE->Init (sizeof(PMEDGE));
   plPMEDGE->Clear();

   // loop through all the sides filling in edge info
   DWORD i, j, dwNum, dw1, dw2;
   PCPMSideOld ps;
   PMEDGE pe;
   memset (&pe, 0, sizeof(pe));
   pe.adwSide[1] = -1;  // since wont fill in this go
   dwNum = plCPMSide->Num();
   ps = (PCPMSideOld) plCPMSide->Get(0);
   for (i = 0; i < dwNum; i++, ps++) {
      pe.adwSide[0] = i;
      for (j = 0; j < ps->Num(); j++) {
         dw1 = ps->m_adwVert[j];
         dw2 = ps->m_adwVert[(j+1)%ps->Num()];
         pe.adwVert[0] = min(dw1,dw2);
         pe.adwVert[1] = max(dw1,dw2);
         plPMEDGE->Add (&pe);

         // BUGFIX - Bidirectional
         if (fBiDi) {
            pe.adwVert[0] = max(dw1,dw2);
            pe.adwVert[1] = min(dw1,dw2);
            plPMEDGE->Add (&pe);
         }

      } // j
   }  // i

   // sort this
   qsort (plPMEDGE->Get(0), plPMEDGE->Num(), sizeof(PMEDGE), PMEDGECompare);

   // go through and eliminate duplicates
   PPMEDGE ppe;
   ppe = (PPMEDGE) plPMEDGE->Get(0);
   dwNum = plPMEDGE->Num();
   for (i = j = 0; i < dwNum; i++) {
      // if the current one is the same as the last one added then just note the
      // second edge and continue
      if (j && (ppe[i].adwVert[0] == ppe[j-1].adwVert[0]) && (ppe[i].adwVert[1] == ppe[j-1].adwVert[1])) {
         ppe[j-1].adwSide[1] = ppe[i].adwSide[0];
         continue;
      }

      // else, copy over
      if (i != j)
         ppe[j] = ppe[i];
      j++;
   }

   // remove the last elements
   while ((dwNum = plPMEDGE->Num()) > j)
      plPMEDGE->Remove (dwNum-1);

   // done
}


/*************************************************************************************
CObjectPolyMeshOld::SubdivideMesh - Subdivides the polygon mesh one time. This
subdivision is used for rendering. It is assumed that the original points have valid
m_pLocWithDeform values since the deformation should have taken place. The new
points will have valid m_pLocWithDeform points, but the normals will NOT be calculated
and the deformation information will NOT exist in the new ones.

inputs
   PCListFixed       plPMSideOrig - Original side. Not modified.
   PCListFixed       plPMVertOrig - Original verticies. Not modified.
   PCListFixed       plPMSideNew - Initialized (so make sure no pointers to memory were kept).
                                    Filled with new sides.
   PCListFixed       plPMVertNew - Initialized (so make sure no pointers to memory were kept).
                                    Filled with new sides
returns
   none
*/
void CObjectPolyMeshOld::SubdivideMesh (PCListFixed plPMSideOrig, PCListFixed plPMVertOrig,
                                     PCListFixed plPMSideNew, PCListFixed plPMVertNew)
{
   // fill in the normals for the original
   FillInNormals (plPMVertOrig, plPMSideOrig);

   // determine the edges
   CListFixed lEdge;
   EnumEdges (plPMSideOrig, &lEdge);

   // fill the new list of vertices with all the previous vertex information, ignoring
   // the memory allocated in them. Will deal with this later. Mostly need stuffing
   plPMVertNew->Init (sizeof(CPMVertOld), plPMVertOrig->Get(0), plPMVertOrig->Num());

   // create one point per edge, which is an average of two endpoints
   DWORD i, j, dwNumEdge;
   PPMEDGE ppe;
   ppe = (PPMEDGE) lEdge.Get(0);
   dwNumEdge = lEdge.Num();
   CPMVertOld pv;
   PCPMVertOld pvOrig;
   DWORD dwVertOrig;
   pvOrig = (PCPMVertOld) plPMVertOrig->Get(0);
   dwVertOrig = plPMVertOrig->Num();
   for (i = 0; i < dwNumEdge; i++) {
      // use side 0 first for the texture point
      pv.HalfWay (pvOrig + ppe[i].adwVert[0], pvOrig + ppe[i].adwVert[1], ppe[i].adwSide[0], TRUE);

      // if there's another side then use that one also
      if (ppe[i].adwSide[1] != -1) {
         PTEXTUREPOINT p1, p2;
         TEXTUREPOINT pAvg;
         p1 = pvOrig[ppe[i].adwVert[0]].TextureGet (ppe[i].adwSide[1]);
         p2 = pvOrig[ppe[i].adwVert[1]].TextureGet (ppe[i].adwSide[1]);
         pAvg.h = (p1->h + p2->h) / 2.0;
         pAvg.v = (p1->v + p2->v) / 2.0;
         pv.TextureSet (ppe[i].adwSide[1], &pAvg);
      }

      plPMVertNew->Add (&pv);
   }

   // go back over the initial vertices and average them all in with all their
   // connecting edges
   PCPMVertOld pvNew;
   DWORD dwVertNew;
   pvNew = (PCPMVertOld) plPMVertNew->Get(0);
   dwVertNew = plPMVertNew->Num();
   for (i = 0; i < dwVertOrig; i++) {
      pvOrig[i].CloneTo (&pvNew[i], FALSE);  // clone it over except for the deformation
      pvNew[i].m_pLoc.Copy (&pvNew[i].m_pLocWithDeform);
   }
#if 0 // old code - didn't smooth as nicely
   for (i = 0; i < dwVertOrig; i++) {
      pvNew[i].Zero();  // so no memory allocated for them
      pvNew[i].m_pLoc.Zero(); // used as a temporary sum
      pvNew[i].m_pLoc.p[3] = 0;  // temporary count
   }
   for (i = 0; i < dwNumEdge; i++) for (j = 0; j < 2; j++) {
      pvNew[ppe[i].adwVert[j]].m_pLoc.Add (&pvNew[i + dwVertOrig].m_pLocWithDeform);
      pvNew[ppe[i].adwVert[j]].m_pLoc.p[3] += 1;
   }
   for (i = 0; i < dwVertOrig; i++) {
      // since summed all the surrounding midpoints together, average
      if (pvNew[i].m_pLoc.p[3] != 0)
         pvNew[i].m_pLoc.Scale (1.0 / pvNew[i].m_pLoc.p[3]);

      // convert this to a delta from the original point
      CPoint pDelta;
      pDelta.Subtract (&pvNew[i].m_pLoc, &pvOrig[i].m_pLocWithDeform);

      // the delta can only run along the normal line
      fp fDot;
      fDot = pDelta.DotProd (&pvOrig[i].m_pNorm);
      fDot /= 2;   // only go half way
      pDelta.Copy (&pvOrig[i].m_pNorm);
      pDelta.Scale (fDot);

      // average this with the deformed point
      pvNew[i].m_pLocWithDeform.Add (&pDelta);
      pvNew[i].m_pLoc.Copy (&pvNew[i].m_pLocWithDeform);
   }
#endif //o

   // now that all the verticies are filled in, need to subdivide the edges
   plPMSideNew->Init (sizeof(CPMSideOld), plPMSideOrig->Get(0), plPMSideOrig->Num());
   PCPMSideOld psOrig;
   DWORD dwSideOrig, k;
   PTEXTUREPOINT pAll, pThis;
   psOrig = (PCPMSideOld) plPMSideOrig->Get(0);
   dwSideOrig = plPMSideOrig->Num();
   for (i = 0; i < dwSideOrig; i++) {
      PCPMSideOld pCur = (PCPMSideOld) plPMSideNew->Get(i);
      
      // zero out any allocates
      pCur->Zero();

      // find midpoints of all edges
      DWORD dwMid[4];
      DWORD dwNum, dwWillBe;
      PMEDGE pe, *pFind;
      memset (&pe, 0, sizeof(pe));
      dwNum = pCur->Num();
      for (j = 0; j < dwNum; j++) {
         pe.adwVert[0] = min(pCur->m_adwVert[j], pCur->m_adwVert[(j+1)%dwNum]);
         pe.adwVert[1] = max(pCur->m_adwVert[j], pCur->m_adwVert[(j+1)%dwNum]);
         pFind = (PPMEDGE) bsearch (&pe, ppe, dwNumEdge, sizeof(PMEDGE), PMEDGECompare);
         if (!pFind)
            break;
         dwMid[j] = ((DWORD) (PBYTE) pFind - (DWORD) (PBYTE) ppe) / sizeof(PMEDGE);
         dwMid[j] += dwVertOrig; // since added onto the edge of the original
      }
      if (j < dwNum)
         continue;   // some sort of problem happened

      CPMSideOld  as[4];
      memset (&as, 0, sizeof(as));
      for (j = 0; j < 4; j++) {
         as[j].m_dwOrigSide = pCur->m_dwOrigSide;
         as[j].m_fHide = pCur->m_fHide;   // pass down hide info
      }

      // works for any number of sides

      if (dwNum == 3) { // triangle
         // triangles at edges
         for (j = 0; j < dwNum; j++) {
            as[j].m_adwVert[0] = pCur->m_adwVert[j];
            as[j].m_adwVert[1] = dwMid[j];
            as[j].m_adwVert[2] = dwMid[(j+dwNum-1)%dwNum];
            as[j].m_adwVert[3] = -1;
         }

         // current one is adjusted to just be a loop around the splits
         for (j = 0; j < dwNum; j++)
            pCur->m_adwVert[j] = dwMid[j];
         // dont need to adjust textures for this since already figured out for midpoint,
         // and haven't change side ID
      }
      else {   // quad
         // need to create a center point for the quad
         DWORD dwCenter;
         dwCenter = -1;
         CPMVertOld pLeft, pRight, pCenter;
         pvNew = (PCPMVertOld) plPMVertNew->Get(0);
         dwVertNew = plPMVertNew->Num();

         // find half-way points on left and right
         pLeft.HalfWay (&pvNew[pCur->m_adwVert[0]],
            &pvNew[pCur->m_adwVert[1]],
            i, TRUE);
         pRight.HalfWay (&pvNew[pCur->m_adwVert[2]],
            &pvNew[pCur->m_adwVert[3]],
            i, TRUE);

         // average normals
         pLeft.m_pNorm.Add (&pvNew[pCur->m_adwVert[0]].m_pNorm,
            &pvNew[pCur->m_adwVert[1]].m_pNorm);
         pRight.m_pNorm.Add (&pvNew[pCur->m_adwVert[2]].m_pNorm,
            &pvNew[pCur->m_adwVert[3]].m_pNorm);
         pLeft.m_pNorm.Normalize();
         pRight.m_pNorm.Normalize();

         // combine those two
         pCenter.HalfWay (&pLeft, &pRight, i, TRUE);
         pLeft.Release();
         pRight.Release();

         // add the center
         dwCenter = dwVertNew;
         plPMVertNew->Add (&pCenter);

         // update the pointers in case memory realloced
         pvNew = (PCPMVertOld) plPMVertNew->Get(0);
         dwVertNew = plPMVertNew->Num();

         // NOTE: Don't need to figure out texture point for the center since will
         // only be one alternative, and already figured it out

         // create new quads
         as[0].m_adwVert[0] = pCur->m_adwVert[1];
         as[0].m_adwVert[1] = dwMid[1];
         as[0].m_adwVert[2] = dwCenter;
         as[0].m_adwVert[3] = dwMid[0];

         as[1].m_adwVert[0] = pCur->m_adwVert[2];
         as[1].m_adwVert[1] = dwMid[2];
         as[1].m_adwVert[2] = dwCenter;
         as[1].m_adwVert[3] = dwMid[1];

         as[2].m_adwVert[0] = pCur->m_adwVert[3];
         as[2].m_adwVert[1] = dwMid[3];
         as[2].m_adwVert[2] = dwCenter;
         as[2].m_adwVert[3] = dwMid[2];


         pCur->m_adwVert[1] = dwMid[0];
         pCur->m_adwVert[2] = dwCenter;
         pCur->m_adwVert[3] = dwMid[3];
      }

      // make sure all the vertices have the right texture associated
      // basically, if find any textures that differ in the current triangle
      // from what will be setting it as then divide up
      for (j = 0; j < 3; j++) {
         dwWillBe = j + plPMSideNew->Num();// what side number will this be?
         for (k = 0; k < dwNum; k++) {
            pAll = pvNew[as[j].m_adwVert[k]].TextureGet();
            pThis = pvNew[as[j].m_adwVert[k]].TextureGet(i);
            if ((pAll != pThis) && !AreClose(pAll, pThis))
               pvNew[as[j].m_adwVert[k]].TextureSet (dwWillBe, pThis);
         }
      }

      // add the new ones
      for (j = 0; j < 3; j++) // always add 3 new
         plPMSideNew->Add (&as[j]);
   } // i - original sides

}


/*************************************************************************************
CObjectPolyMeshOld::SubdivideIfNecessary - If the subdivision flag is dirty this
subdivides the main points into smaller ones so get a smoother surfce.
*/
void CObjectPolyMeshOld::SubdivideIfNecessary (void)
{
   if (!m_fDivDirty)
      return;  // not dirty

   // clear out current list
   ClearVertSide (TRUE);
   m_fDivDirty = FALSE;

   // apply the deformations to all the vertecies
   DWORD i;
   PCPMVertOld pv;
   DWORD dwNum;
   pv = (PCPMVertOld) m_lCPMVert.Get(0);
   dwNum = m_lCPMVert.Num();
   for (i = 0; i < dwNum; i++)
      pv[i].DeformCalc(m_lMorphRemapID.Num(), (DWORD*) m_lMorphRemapID.Get(0), (fp*) m_lMorphRemapValue.Get(0));

   // fill all the sides with their original ID
   PCPMSideOld ps;
   dwNum = m_lCPMSide.Num();
   ps = (PCPMSideOld) m_lCPMSide.Get(0);
   for (i = 0; i < dwNum; i++)
      ps[i].m_dwOrigSide = i;

   // subdivide
   CListFixed alVert[2], alSide[2];
   DWORD dwTo;
   dwTo = 0;
   PCListFixed plVertFrom, plSideFrom, plVertTo, plSideTo;
   plVertFrom = &m_lCPMVert;
   plSideFrom = &m_lCPMSide;
   DWORD j;
   for (j = 0; j < m_dwSubdivide; j++) {
      // where convert to? If last one go to built in list
      if (j+1 >= m_dwSubdivide) {
         plVertTo = &m_lCPMVertDiv;
         plSideTo = &m_lCPMSideDiv;
      }
      else {
         // point it to ping pong buffer
         plVertTo = &alVert[dwTo];
         plSideTo = &alSide[dwTo];
         dwTo = !dwTo;
      }

      // clear what pointing to
      pv = (PCPMVertOld) plVertTo->Get(0);
      ps = (PCPMSideOld) plSideTo->Get(0);
      dwNum = plVertTo->Num();
      for (i = 0; i < dwNum; i++)
         pv[i].Release();
      dwNum = plSideTo->Num();
      for (i = 0; i < dwNum; i++)
         ps[i].Release();
      plVertTo->Clear();
      plSideTo->Clear();

      // subdivide
      SubdivideMesh (plSideFrom, plVertFrom, plSideTo, plVertTo);

      plSideFrom = plSideTo;
      plVertFrom = plVertTo;
   }

   // free up ping pong buffer
   for (j = 0; j < 2; j++) {
      plVertTo = &alVert[j];
      plSideTo = &alSide[j];

      pv = (PCPMVertOld) plVertTo->Get(0);
      ps = (PCPMSideOld) plSideTo->Get(0);
      dwNum = plVertTo->Num();
      for (i = 0; i < dwNum; i++)
         pv[i].Release();
      dwNum = plSideTo->Num();
      for (i = 0; i < dwNum; i++)
         ps[i].Release();
      plVertTo->Clear();
      plSideTo->Clear();
   }

   // fill in all the normals for rendering
   FillInNormals (m_dwSubdivide ? &m_lCPMVertDiv : &m_lCPMVert, m_dwSubdivide ? &m_lCPMSideDiv : &m_lCPMSide);

   // done
}


/*************************************************************************************
TouchingVertices - Given a vertex number, this checks its "distance value". If
the distance value is < the current distance, then the new distance is
set. (If >= then returns without doing anything.)
It then increments the distance by one, and assuming it's not more than the
maximum allowed, calls all touching vertices with the TouchingVertecies() call.

Use this to determine how many vertecies within a certain distance of a given vertex.
(Where distance in terms of connections.)

inputs
   DWORD       dwVert - Vertex to modify
   DWORD       *padwVertDist - Pointer to an array of dwNum DWORDs with distances. Initial
               distance should be large number.
   DWORD       dwNum - Number of verticies.
   DWORD       dwCurDist - Current distance to use
   DWORD       dwMaxDist - Maximum distance to use. if dwCurDist would be > dwMaxDist then no more
   PCListFixed  plPMEDGE - Filled in with edge data from EnumEdges (..., TRUE) - make
               sure the BiDi value is set when calling EnumEdges()
returns
   none
*/
void TouchingVertices (DWORD dwVert, DWORD *padwVertDist, DWORD dwNum, DWORD dwCurDist, DWORD dwMaxDist,
                       PCListFixed plPMEDGE)
{
   // see what have
   if (dwVert >= dwNum)
      return;  // out of bounds

   // see if should set
   if (padwVertDist[dwVert] < dwCurDist)
      return;  // already lower

   // set it
   padwVertDist[dwVert] = dwCurDist;

   // increment
   dwCurDist++;
   if (dwCurDist > dwMaxDist)
      return;  // no more

   // find the first edge that matches
   PPMEDGE ppe;
   DWORD dwNumEdge, i;
   ppe = (PPMEDGE) plPMEDGE->Get(0);
   dwNumEdge = plPMEDGE->Num();

   // find the single bit that's >= dwNumEdge
   for (i = 1; i < dwNumEdge; i *= 2);

   // see which to take
   DWORD dwCur;
   for (dwCur = 0; i; i /= 2) {
      if (dwCur + i >= dwNumEdge)
         continue;   // this would be too high, so take lower bit

      // which value to use
      if (ppe[dwCur+i].adwVert[0] < dwVert)
         dwCur += i; // below dwVert so go higher
   }

   // when get here dwCur filled with just below start of list
   for (; dwCur < dwNumEdge; dwCur++) {
      if (ppe[dwCur].adwVert[0] == dwVert)
         break;
      if (ppe[dwCur].adwVert[0] > dwVert)
         return;  // cant find any connections
   }
   if (dwCur >= dwNumEdge)
      return;  // cant find any connections

   // else, if get here then have the first connection
   for (; dwCur < dwNumEdge; dwCur++) {
      if (ppe[dwCur].adwVert[0] != dwVert)
         break;

      // recurse
      TouchingVertices (ppe[dwCur].adwVert[1], padwVertDist, dwNum, dwCurDist, dwMaxDist, plPMEDGE);
   }
}

/*************************************************************************************
NumMorphIDs - Returns the maximum (+1) morph ID that's found. (Ignores combo-morphs)

inputs
   PCListFixed       plPCOEAttrib - Attrib
reutrns
   DWORD - Number of moph IDs, (maximum+1)
*/
static DWORD NumMorphIDs (PCListFixed plPCOEAttrib)
{
   DWORD dwMax, i, j;
   dwMax = -1;
   PCOEAttrib *pap;
   pap = (PCOEAttrib*) plPCOEAttrib->Get(0);
   for (i = 0; i < plPCOEAttrib->Num(); i++) {
      // only allow the default settings
      if (pap[i]->m_dwType != 2)
         continue;

      PCOEATTRIBCOMBO pc = (PCOEATTRIBCOMBO) pap[i]->m_lCOEATTRIBCOMBO.Get(0);
      DWORD dwNum = pap[i]->m_lCOEATTRIBCOMBO.Num();

      for (j = 0; j < dwNum; j++, pc++)
         dwMax = (dwMax == -1) ? pc->dwMorphID : max(dwMax, pc->dwMorphID);
   }
   if (dwMax == -1)
      return 0;  // nothing
   return dwMax + 1;
}

/*************************************************************************************
CObjectPolyMeshOld::CalcMorphRemap - Called when the morph parameters have changed.
This recalculates the values of all the morphs. It fills in m_lMorphRemapXXX
*/
void CObjectPolyMeshOld::CalcMorphRemap (void)
{
   // clear old
   m_lMorphRemapID.Clear();
   m_lMorphRemapValue.Clear();

   // find number of morph IDs
   DWORD dwMax, i, j;
   dwMax = NumMorphIDs (&m_lPCOEAttrib);
   if (dwMax == 0)
      return;  // nothing

   // allocate enoug space
   CMem memMorph;
   if (!memMorph.Required (dwMax * sizeof(fp)))
      return;  // cant go furhter
   fp *pfMorph;
   pfMorph = (fp*) memMorph.p;
restart:
   for (i = 0; i < dwMax; i++)
      pfMorph[i] = 0;

   // loop over attributes and set morph accordingly
   PCOEAttrib *pap;
   pap = (PCOEAttrib*) m_lPCOEAttrib.Get(0);
   for (i = 0; i < m_lPCOEAttrib.Num(); i++) {
      PCOEATTRIBCOMBO pc = (PCOEATTRIBCOMBO) pap[i]->m_lCOEATTRIBCOMBO.Get(0);
      DWORD dwNum = pap[i]->m_lCOEATTRIBCOMBO.Num();

      for (j = 0; j < dwNum; j++, pc++) {
         if (pc->dwMorphID >= dwMax) {
            // out of range, so clear out. Shouldnt happen that ofen
            pap[i]->m_lCOEATTRIBCOMBO.Remove (j);
            goto restart;
         }

         // figure out the ramp...
         fp fIn, fVal, fDelta;
         DWORD k;
         fIn = pap[i]->m_fCurValue;
         for (k = 0; k < pc->dwNumRemap; k++)
            if (fIn < pc->afpComboValue[k])
               break;
         if (k == 0) {
            // before the start
            if ((pc->dwCapEnds & 0x01) || (pc->afpComboValue[0] == pc->afpComboValue[1]))
               fVal = pc->afpObjectValue[0];
            else
               k++;   // so that uses that trend line
         }
         else if (k == pc->dwNumRemap) {
            if ((pc->dwCapEnds & 0x02) || (pc->afpComboValue[pc->dwNumRemap-1] == pc->afpComboValue[pc->dwNumRemap-2]))
               fVal = pc->afpObjectValue[pc->dwNumRemap-1];
            else
               k--;   // so that uses that trend line
         }
         if ((k > 0) && (k < pc->dwNumRemap)) {
            k--;
            fDelta = pc->afpComboValue[k+1] - pc->afpComboValue[k];
            if (fDelta == 0)
               fDelta = 1; // so no divide by zero
            fVal = (fIn - pc->afpComboValue[k]) / fDelta *
               (pc->afpObjectValue[k+1] - pc->afpObjectValue[k]) +
               pc->afpObjectValue[k];
         }

         // include the value in
         pfMorph[pc->dwMorphID] += fVal;

      } // j
   } // i

   // only take non-zero values
   for (i = 0; i < dwMax; i++)
      if (fabs(pfMorph[i]) >= CLOSE) {
         m_lMorphRemapValue.Add (&pfMorph[i]);
         m_lMorphRemapID.Add (&i);
      }

}


/*************************************************************************************
CObjectPolyMeshOld::CalcVertexGroups - Loops through the list of edges and creates
vertex groups based on the current distance. This filles in the m_lVertexGroup list
with a list of DWORDs that are the vertex indecies to be used at the center of
each vertex group.
*/
void CObjectPolyMeshOld::CalcVertexGroups (void)
{
   // if it's not dirty then don't bother
   if (!m_fVertGroupDirty)
      return;
   m_fVertGroupDirty = FALSE;

   // first, calculate edges
   CListFixed lEdge;
   EnumEdges (&m_lCPMSide, &lEdge, TRUE);

   // clear the list of vertex groups
   m_lVertexGroup.Clear();

   // create scratch space for count and use
   CMem memCount, memUse;
   DWORD dwNumVert;
   dwNumVert = m_lCPMVert.Num();
   if (!memCount.Required (dwNumVert * sizeof(DWORD)) || !memUse.Required(dwNumVert * sizeof(DWORD)))
      return;
   DWORD *padwCount, *padwUse;
   padwCount = (DWORD*) memCount.p;
   padwUse = (DWORD*) memUse.p;

largergroup:
   // set the use flag to assume everything is used
   DWORD i, j;
   for (i = 0; i < dwNumVert; i++)
      padwUse[i] = TRUE;

   // vert group distance always at least one
   m_dwVertGroupDist = max(1, m_dwVertGroupDist);

   // loop through all the ones that are marked as "used" and clear out spaces besides them
   for (i = 0; i < dwNumVert; i++) {
      // if were blanked out by earlier control point distance then skip
      if (!padwUse[i])
         continue;

      // set the counts
      for (j = 0; j < dwNumVert; j++)
         padwCount[j] = m_dwVertGroupDist + 1;

      // run the process and find out what's near
      TouchingVertices (i, padwCount, dwNumVert, 0, m_dwVertGroupDist, &lEdge);

      // eliminate any of the nearby ones from the use list
      // only do so for those points beyond current range, since know that if less
      // than current one then already checked them out and passed them
      for (j = i+1; j < dwNumVert; j++)
         if (padwCount[j] < m_dwVertGroupDist + 1)
            padwUse[j] = 0;
   }

   // fill in the list with the remaining elements
   m_lVertexGroup.Clear();
   for (i = 0; i < dwNumVert; i++)
      if (padwUse[i])
         m_lVertexGroup.Add (&i);

   // if there are too many elements then increment the group distance and retry
   if (m_lVertexGroup.Num() > MAXVERTGROUP) {
      // dont bother updating object about to change here. Not necessary
      m_dwVertGroupDist++;
      goto largergroup;
   }

   // if we dont have any vertexes and sides calculated for object, choose vertex
   // 0, which is known to be a group point, and create vertecies around if
   if (m_dwCurVertGroup == -1)
      CalcVertexSideInGroup(0);
}

/*************************************************************************************
CObjectPolyMeshOld::CalcVertexSideInGroup - Calcualtes the vertices surrounding a group
vertex. This fills in m_lVertexInGroup AND m_lSideInGroup.
Sets m_dwCurVertGroup to dwCenter.

inputs
   DWORD    dwCenter - Center vertex
returns
   none
*/
void CObjectPolyMeshOld::CalcVertexSideInGroup (DWORD dwCenter)
{
   m_dwCurVertGroup = dwCenter;

   // first, calculate edges
   CListFixed lEdge;
   EnumEdges (&m_lCPMSide, &lEdge, TRUE);

   // create scratch space for count and use
   CMem memCount;
   DWORD dwNumVert;
   dwNumVert = m_lCPMVert.Num();
   if (!memCount.Required (dwNumVert * sizeof(DWORD)))
      return;
   DWORD *padwCount;
   padwCount = (DWORD*) memCount.p;

smaller:
   // clear out the lists
   m_lVertexInGroup.Clear();
   m_lSideInGroup.Clear();

   // vert group distance always at least one
   m_dwVertSideGroupDist = max(1, m_dwVertSideGroupDist);

   DWORD i, j;

   // set the counts
   for (j = 0; j < dwNumVert; j++)
      padwCount[j] = m_dwVertSideGroupDist + 1;

   // run the process and find out what's near
   TouchingVertices (dwCenter, padwCount, dwNumVert, 0, m_dwVertSideGroupDist, &lEdge);

   // eliminate any of the nearby ones from the use list
   for (j = 0; j < dwNumVert; j++)
      if (padwCount[j] < m_dwVertSideGroupDist + 1)
         m_lVertexInGroup.Add (&j);

   // if there are too many elements then increment the group distance and retry
   if (m_lVertexInGroup.Num() > MAXVERTGROUP) {
      // dont bother updating object about to change here. Not necessary
      m_dwVertSideGroupDist--;
      goto smaller;
   }


   // now get the sides
   PCPMSideOld ps;
   DWORD dwNum;
   ps = (PCPMSideOld) m_lCPMSide.Get(0);
   dwNum = m_lCPMSide.Num();
   for (i = 0; i < dwNum; i++) {
      DWORD dwSides = ps[i].Num();
      for (j = 0; j < dwSides; j++) {
         if (DWORDSearch (ps[i].m_adwVert[j], m_lVertexInGroup.Num(), (DWORD*)m_lVertexInGroup.Get(0)) != -1)
            break;
      }  // j

      // if found a matching side then add
      if (j < dwSides)
         m_lSideInGroup.Add (&i);
   }  // i

   // if there are too many sides then loop
   if (m_lSideInGroup.Num() > MAXVERTGROUP) {
      // dont bother updating object about to change here. Not necessary
      m_dwVertSideGroupDist--;
      goto smaller;
   }

   // else, done
}

/*************************************************************************************
CObjectPolyMeshOld::SubdivideSide - Given a specific side (clicked on as a control point),
this subdivides the side (and any mirrors).

NOTE: This does NOT inform the world of the change

inputs
   DWORD       dwSide - Side to change
returns
   none
*/
void CObjectPolyMeshOld::SubdivideSide (DWORD dwSide)
{
   PCPMSideOld ps = (PCPMSideOld) m_lCPMSide.Get(0);
   PCPMVertOld pv = (PCPMVertOld) m_lCPMVert.Get(0);
   DWORD dwNumSide = m_lCPMSide.Num();
   DWORD dwNumVert = m_lCPMVert.Num();
   DWORD dwNum = ps[dwSide].Num();
   DWORD i, j, k, l;

   // get the point locations
   CPoint apOrig[4];
   CPoint pComp;
   for (i = 0; i < dwNum; i++)
      apOrig[i].Copy (&pv[ps[dwSide].m_adwVert[i]].m_pLoc);

   // add this one
   CListFixed lDivide;
   lDivide.Init (sizeof(DWORD), &dwSide, 1);

   // mirror images
   DWORD x, y, z;
   for (x = 0; x < (DWORD) ((m_dwSymmetry & 0x01) ? 2 : 1); x++)
   for (y = 0; y < (DWORD) ((m_dwSymmetry & 0x02) ? 2 : 1); y++)
   for (z = 0; z < (DWORD) ((m_dwSymmetry & 0x04) ? 2 : 1); z++) {
      // if it's 0,0,0 dont bother bevause already added
      if (!x && !y && !z)
         continue;

      // find a match
      for (j = 0; j < dwNumSide; j++) {
         if (j == dwSide)
            continue;

         // if not same number of points ignore
         if (ps[j].Num() != dwNum)
            continue;

         // look through all the points and make sure the're the same
         for (k = 0; k < dwNum; k++) {
            pComp.Copy (&pv[ps[j].m_adwVert[k]].m_pLoc);
            if (x)
               pComp.p[0] *= -1;
            if (y)
               pComp.p[1] *= -1;
            if (z)
               pComp.p[2] *= -1;

            // must match one of points
            for (l = 0; l < dwNum; l++)
               if (pComp.AreClose (&apOrig[l]))
                  break;
            if (l >= dwNum)
               break;   // not the same
         }
         if (k < dwNum)
            continue;   // not the same

         // else, found a match
         lDivide.Add (&j);
      } // j
   } // xyz

   // have list of sides to divide, so do so
   SidesAddDetail ((DWORD*) lDivide.Get(0), lDivide.Num(), TRUE);
}



/*************************************************************************************
CObjectPolyMeshOld::ShowHideSide - Given a specific side (clicked on as a control point),
shows/hides the side (toggling display)

NOTE: This does NOT inform the world of the change

inputs
   DWORD       dwSide - Side to change
returns
   none
*/
void CObjectPolyMeshOld::ShowHideSide (DWORD dwSide)
{
   PCPMSideOld ps = (PCPMSideOld) m_lCPMSide.Get(0);
   PCPMVertOld pv = (PCPMVertOld) m_lCPMVert.Get(0);
   DWORD dwNumSide = m_lCPMSide.Num();
   DWORD dwNumVert = m_lCPMVert.Num();
   DWORD dwNum = ps[dwSide].Num();
   DWORD i, j, k, l;

   // will it be hidden
   if (dwSide >= dwNumSide)
      return;
   BOOL fHide;
   fHide = !ps[dwSide].m_fHide;

   // get the point locations
   CPoint apOrig[4];
   CPoint pComp;
   for (i = 0; i < dwNum; i++)
      apOrig[i].Copy (&pv[ps[dwSide].m_adwVert[i]].m_pLoc);

   // add this one
   CListFixed lDivide;
   lDivide.Init (sizeof(DWORD), &dwSide, 1);

   // mirror images
   DWORD x, y, z;
   for (x = 0; x < (DWORD) ((m_dwSymmetry & 0x01) ? 2 : 1); x++)
   for (y = 0; y < (DWORD) ((m_dwSymmetry & 0x02) ? 2 : 1); y++)
   for (z = 0; z < (DWORD) ((m_dwSymmetry & 0x04) ? 2 : 1); z++) {
      // if it's 0,0,0 dont bother bevause already added
      if (!x && !y && !z)
         continue;

      // find a match
      for (j = 0; j < dwNumSide; j++) {
         if (j == dwSide)
            continue;

         // if not same number of points ignore
         if (ps[j].Num() != dwNum)
            continue;

         // look through all the points and make sure the're the same
         for (k = 0; k < dwNum; k++) {
            pComp.Copy (&pv[ps[j].m_adwVert[k]].m_pLoc);
            if (x)
               pComp.p[0] *= -1;
            if (y)
               pComp.p[1] *= -1;
            if (z)
               pComp.p[2] *= -1;

            // must match one of points
            for (l = 0; l < dwNum; l++)
               if (pComp.AreClose (&apOrig[l]))
                  break;
            if (l >= dwNum)
               break;   // not the same
         }
         if (k < dwNum)
            continue;   // not the same

         // else, found a match
         lDivide.Add (&j);
      } // j
   } // xyz

   // now have all the sides, set them
   DWORD *padw;
   padw = (DWORD*) lDivide.Get(0);
   dwNum = lDivide.Num();
   for (i = 0; i < dwNum; i++)
      ps[padw[i]].m_fHide = fHide;

   m_fDivDirty = TRUE;

   // set flag to turn off backface culling
   if (fHide)
      m_fCanBackface = FALSE;
}

/*************************************************************************************
CObjectPolyMeshOld::RemoveVertex - Removes a given vertex. In the process it combines all
the attached sides into one large side.

NOTE: This does not tell m_pWorld of the change.

inputs
   DWORD    dwVert - Vertex to remove
returns
   none
*/
void CObjectPolyMeshOld::RemoveVertex (DWORD dwVert)
{
   // find all sides that this vertex is attached to
   CListFixed lSides;
   lSides.Init (sizeof(DWORD));
   DWORD i, j, dwNum;
   PCPMSideOld ps = (PCPMSideOld) m_lCPMSide.Get(0);
   PCPMVertOld pv = (PCPMVertOld) m_lCPMVert.Get(0);
   DWORD dwSideNum = m_lCPMSide.Num();
   DWORD dwVertNum = m_lCPMVert.Num();
   BOOL fHide = FALSE;
   for (i = 0; i < dwSideNum; i++) {
      dwNum = ps[i].Num();
      for (j = 0; j < dwNum; j++)
         if (ps[i].m_adwVert[j] == dwVert)
            break;
      if (j < dwNum) {
         lSides.Add (&i);
         fHide |= ps[i].m_fHide; // if hidden keep this
      }
   }

   // create a copy of sides for late
   CListFixed lSidesOrig;
   lSidesOrig.Init (sizeof(DWORD), lSides.Get(0), lSides.Num());

   // create a list of the vertecies to use, which will be point and texture
   CListFixed lVertex, lText;
   lVertex.Init (sizeof(DWORD));
   lText.Init (sizeof(TEXTUREPOINT));

   // while there are sides left
   while (lSides.Num()) {
      DWORD *padwSides = (DWORD*) lSides.Get(0);
      DWORD dwNumSides = lSides.Num();

      // if there aren't any verticies so far then just pick a side
      DWORD dwBestSide;
      PCPMSideOld pBest;
#if 0 // not needed because wrapped code all into one
      if (!lVertex.Num()) {
         // pick the first one
         dwBestSide = 0;
         pBest = &ps[padwSides[dwBestSide]];

         // find the point just clockwise of then one we're removing
         for (i = 0; i < pBest->Num(); i++)
            if (pBest->m_adwVert[i] == dwVert)
               break;
         i = (i+1) % pBest->Num();

         // increment i one more time since will end up adding that point when finish up
         i = (i+1) % pBest->Num();

         // add all these vertecies (except for the one removing) onto the list
         for (j = 0; j < pBest->Num()-2; j++) {
            DWORD dwAdd;
            dwAdd = pBest->m_adwVert[(j+i) % pBest->Num()];
            lVertex.Add (&dwAdd);
            lText.Add (pv[dwAdd].TextureGet (padwSides[dwBestSide]));
         }

         // remove this and repeat
         lSides.Remove (dwBestSide);
         continue;
      }
#endif // 0
      
      // find the one of the sides that contains the last vertex followed by the last side added
      DWORD dwOrigNum;
      dwOrigNum = lSides.Num();
      for (dwBestSide = 0; dwBestSide < dwOrigNum; dwBestSide++) {
         pBest = &ps[padwSides[dwBestSide]];

         // find the point just clockwise of then one we're removing
         for (i = 0; i < pBest->Num(); i++)
            if (pBest->m_adwVert[i] == dwVert)
               break;
         i = (i+1) % pBest->Num();

         // if this isn't the last vertex added then continue
         if (lVertex.Num() && (pBest->m_adwVert[i] != *((DWORD*)lVertex.Get(lVertex.Num()-1))))
            continue;
         // if there aren't any previous vertecies then no problem. Just tkae this

         // else, have loop
         i = (i+1) % pBest->Num();
         for (j = 0; j < pBest->Num()-2; j++) {
            DWORD dwAdd;
            dwAdd = pBest->m_adwVert[(j+i) % pBest->Num()];
            lVertex.Add (&dwAdd);
            lText.Add (pv[dwAdd].TextureGet (padwSides[dwBestSide]));
         }

         // remove this side
         lSides.Remove (dwBestSide);
         break;
      }

      if (dwBestSide >= dwOrigNum)
         break;   // shouldnt happen but ran out of tirangles to add

   }  // while have sides

   // when get here know a list of verecies that surround the deleted one,
   // which sides are effected

   // delete the vertex
   pv[dwVert].Release();
   m_lCPMVert.Remove (dwVert);
   for (i = 0; i < dwSideNum; i++)
      ps[i].VertRemove (dwVert);
   pv = (PCPMVertOld) m_lCPMVert.Get(0);
   dwVertNum = m_lCPMVert.Num();

   // note the change to the current vertex group
   if (m_dwCurVertGroup > dwVert)
      m_dwCurVertGroup--;
   else if (m_dwCurVertGroup == dwVert)
      m_dwCurVertGroup = -1;

   // sort the sides so delete highest one first - that way no problems with remove
   qsort (lSidesOrig.Get(0), lSidesOrig.Num(), sizeof(DWORD), BDWORDCompare);

   // remove all the sides
   for (i = lSidesOrig.Num()-1; i < lSidesOrig.Num(); i--) {
      DWORD dwSide = *((DWORD*) lSidesOrig.Get(i));

      for (j = 0; j < dwVertNum; j++)
         pv[j].SideRemove (dwSide);

      // remove it
      ps = (PCPMSideOld) m_lCPMSide.Get(dwSide);
      ps->Release();
      m_lCPMSide.Remove (dwSide);
   }

   // if didn't use all the triangles then delete what have because removed an edge
   if (lSides.Num() || !lVertex.Num())
      return;

   // create quads and triangles to refill
   DWORD *padwVert;
   DWORD dwNumVert;
   PTEXTUREPOINT ptText;
   padwVert = (DWORD*) lVertex.Get(0);
   ptText = (PTEXTUREPOINT) lText.Get(0);
   dwNumVert = lVertex.Num();
   CPMSideOld s;
   memset (&s, 0, sizeof(s));
   s.m_fHide = fHide;

   // take into account vertex removed
   for (i = 0; i < dwNumVert; i++)
      if (padwVert[i] > dwVert)
         padwVert[i]--;
   s.m_adwVert[0] = padwVert[0];

   for (i = 1; i+1 < dwNumVert; i += (dwNum-2)) {
      // how many sides on this
      if (dwNumVert - i > 2)
         dwNum = 4;
      else
         dwNum = 3;

      // fill this in
      s.m_adwVert[3] = -1; // just in case
      for (j = 1; j < dwNum; j++)
         s.m_adwVert[j] = padwVert[(i+j-1)%dwNumVert];
      
      // if the vertex group was deleted due to merge chose a point
      if (m_dwCurVertGroup == -1)
         m_dwCurVertGroup = s.m_adwVert[0];

      // add it
      DWORD dwAdded;
      dwAdded = m_lCPMSide.Num();
      m_lCPMSide.Add (&s);

      // modify all the vertices refererd to so they use the right boundary
      for (j = 0; j < dwNum; j++)
         pv[s.m_adwVert[j]].TextureSet (dwAdded, &ptText[j ? ((i+j-1)%dwNumVert) : 0]);
   }

   // done
}

/*************************************************************************************
CObjectPolyMeshOld::RemoveVertexSymmetry - Removes a given vertex. In the process it combines all
the attached sides into one large side. This one also takes into account symmetry and
removes all points in symmtrical locations

NOTE: This does not tell m_pWorld of the change.

inputs
   DWORD    dwVert - Vertex to remove
returns
   none
*/
void CObjectPolyMeshOld::RemoveVertexSymmetry (DWORD dwVert)
{
   if (dwVert > m_lCPMVert.Num())
      return;
   CPoint pLoc;
   pLoc.Copy (&((PCPMVertOld) m_lCPMVert.Get(dwVert))->m_pLoc);

   // remove this
   RemoveVertex (dwVert);

   // mirror images
   DWORD j;
   DWORD x, y, z;
   CPoint pComp;
   for (x = 0; x < (DWORD) ((m_dwSymmetry & 0x01) ? 2 : 1); x++)
   for (y = 0; y < (DWORD) ((m_dwSymmetry & 0x02) ? 2 : 1); y++)
   for (z = 0; z < (DWORD) ((m_dwSymmetry & 0x04) ? 2 : 1); z++) {
      // if it's 0,0,0 dont bother bevause already added
      if (!x && !y && !z)
         continue;

      // mirrored location
      pComp.Copy (&pLoc);
      if (x)
         pComp.p[0] *= -1;
      if (y)
         pComp.p[1] *= -1;
      if (z)
         pComp.p[2] *= -1;

      // find a match
      PCPMVertOld pv = (PCPMVertOld) m_lCPMVert.Get(0);
      DWORD dwNum = m_lCPMVert.Num();
      for (j = 0; j < dwNum; j++)
         if (pv[j].m_pLoc.AreClose (&pComp))
            break;
      if (j < dwNum)
         RemoveVertex (j);
   } // xyz
}


/**********************************************************************************
IntersectAxisWithSurface - This takes information for a surface (CListFixed of
CPMSideOld and CPMVertOld), and interesects each of the triangles and quads against
a line along X, Y, or Z, to test to see if the given point is inside or outside
the surface.

inputs
   PCPoint        pTest - point to test to see if it's inside or outside
   DWORD          dwDim - Line runs parallel with axis line: 0 = x, 1 = y, 2 = z
   PCListFixed    plCPMSide - Pointer to a list of side information
   PCListFixed    plCPMVert - Pointer to a list of vertex information
returns
   fp - Score indicating how "inside" it is. 1.0 is definitely inside, -1.0 is definitely outside.
         Values in-between indicate some undertainty.
*/
typedef struct {
   CPoint      pInter;     // where intersects
   fp          fAlpha;     // alpha that intersect at
   CPoint      pNorm;      // normal of the surface where intersected... tell which side
                           // it intersected on
   fp          fSide;      // 1 if on inside, -1 if outside, 0 if unknown
} IAP, *PIAP;

fp IntersectAxisWithSurface (PCPoint pTest, DWORD dwDim, PCListFixed plCPMSide, PCListFixed plCPMVert)
{
   PCPMSideOld ps = (PCPMSideOld)plCPMSide->Get(0);
   PCPMVertOld pv = (PCPMVertOld)plCPMVert->Get(0);
   DWORD dwNumSide = plCPMSide->Num();
   DWORD dwNumVert = plCPMVert->Num();

   // direction for the line
   CPoint pDir;
   pDir.Zero();
   pDir.p[dwDim] = 1;

   // other two dimensions
   DWORD dwDim1, dwDim2;
   dwDim1 = (dwDim+1)%3;
   dwDim2 = (dwDim+2)%3;

   // keep a list of intersection points, IAP
   IAP iLeft, iRight;   // closest intersection points to the left and right to point
   PIAP pUse;
   iLeft.fAlpha = -1000000;
   iRight.fAlpha = 1000000;

   DWORD i, j, dwNum;
   PCPoint ap[4];
   CPoint pMin, pMax, pInter, pA, pB;
   fp fAlpha;
   BOOL fRet;
   for (i = 0; i < dwNumSide; i++) {
      // get the vertices of the triangle
      dwNum = ps[i].Num();
      for (j = 0; j < dwNum; j++) {
         ap[j] = &pv[ps[i].m_adwVert[j]].m_pLoc;

         // do a min and max
         if (j) {
            pMin.Min(ap[j]);
            pMax.Max(ap[j]);
         }
         else {
            pMin.Copy(ap[j]);
            pMax.Copy(ap[j]);
         }
      }

      // if boundary beyond the min/max then skip
      if ((pTest->p[dwDim1] > pMax.p[dwDim1]+CLOSE) ||
         (pTest->p[dwDim1] < pMin.p[dwDim1]-CLOSE) ||
         (pTest->p[dwDim2] > pMax.p[dwDim2]+CLOSE) ||
         (pTest->p[dwDim2] < pMin.p[dwDim2]-CLOSE))
         continue;   // out of bounds

      // else, intersect with triangle or quad
      if (dwNum == 4)
         fRet = IntersectLineQuad (pTest, &pDir, ap[0], ap[1], ap[2], ap[3], &pInter, &fAlpha);
      else
         fRet = IntersectLineTriangle (pTest, &pDir, ap[0], ap[1], ap[2], &pInter, &fAlpha);
      if (!fRet)
         continue;   // didn't intersect

      // keep this?
      if (fAlpha >= 0) {
         if (fAlpha >= iRight.fAlpha)
            continue;   // already have one closer
         pUse = &iRight;
      }
      else {   // fAlpha < 0
         if (fAlpha <= iLeft.fAlpha)
            continue;   // already have one closer
         pUse = &iLeft;
      }
      pUse->fAlpha = fAlpha;
      pUse->pInter.Copy (&pInter);

      // get the normal for the surface so know which way it's facing
      pA.Subtract (ap[2], ap[1]);
      pB.Subtract (ap[0], ap[1]);
      pUse->pNorm.CrossProd (&pB, &pA);
   }

   // figure out which side it's on
   fp f;
   iLeft.fSide = iRight.fSide = 0;
   if (iLeft.fAlpha > -100000) {
      iLeft.pNorm.Normalize();
      f = -iLeft.pNorm.DotProd (&pDir);
      if (f > CLOSE)
         iLeft.fSide = 1;
      else if (f < -CLOSE)
         iLeft.fSide = -1;
   }
   else
      iLeft.fSide = -.5; // if nothing then pretty sure on outside, but not absolute

   if (iRight.fAlpha < 100000) {
      iRight.pNorm.Normalize();
      f = iRight.pNorm.DotProd (&pDir);
      if (f > CLOSE)
         iRight.fSide = 1;
      else if (f < -CLOSE)
         iRight.fSide = -1;
   }
   else
      iRight.fSide = -.5;   // if nothing pretty sure on otuside but not absolute

   // value is basically sum of which side thinks on
   return (iRight.fSide + iLeft.fSide) / 2.0;
}


/**********************************************************************************
DetermineWhichVerticesInside - Pass in two surface (each with CPMSideOld and CPMVertOld in
the SAME space). This fills a CListFixed in with BOOLs indicating for every point
in surface A whether or not it appears inside surface B

inputs
   PCListFixed       plCPMSideA - Side A
   PCListFixed       plCPMVertA - Vertex A
   PCListFixed       plCPMSideB - Side B
   PCListFixed       plCPMVertB - Side B
   PCListFixed       plCPMInsideA - Iniitalized ot sizeof(BOOL), and filled with one entry
                        per vertex in A. TRUE if the vertex is in side B, false it it's not.
   BOOL              fAddBuffer - If TRUE, the row of points immediately adjacent are also marked
                        as inside
returns
   none
*/
void DetermineWhichVerticesInside (PCListFixed plCPMSideA, PCListFixed plCPMVertA,
                                   PCListFixed plCPMSideB, PCListFixed plCPMVertB,
                                   PCListFixed plCPMInsideA, BOOL fAddBuffer)
{
   // values
   PCPMSideOld psa = (PCPMSideOld) plCPMSideA->Get(0);
   PCPMSideOld psb = (PCPMSideOld) plCPMSideB->Get(0);
   PCPMVertOld pva = (PCPMVertOld) plCPMVertA->Get(0);
   PCPMVertOld pvb = (PCPMVertOld) plCPMVertB->Get(0);
   DWORD dwNumSideA = plCPMSideA->Num();
   DWORD dwNumSideB = plCPMSideB->Num();
   DWORD dwNumVertA = plCPMVertA->Num();
   DWORD dwNumVertB = plCPMVertB->Num();

   plCPMInsideA->Init(sizeof(BOOL));
   plCPMInsideA->Clear();

   // find a bounding box for surface B so can do trivial reject
   DWORD i, j;
   CPoint pMin, pMax;
   PCPoint pLoc;
   for (i = 0; i < dwNumVertB; i++) {
      pLoc = &pvb[i].m_pLoc;
      if (i) {
         pMin.Min (pLoc);
         pMax.Max (pLoc);
      }
      else {
         pMin.Copy (pLoc);
         pMax.Copy (pLoc);
      }
   }

   // loop through all the points
   BOOL fInside;
   fp fSum;
   for (i = 0; i < dwNumVertA; i++) {
      pLoc = &pva[i].m_pLoc;

      // trivial rejects
      fInside = TRUE;
      for (j = 0; fInside && (j < 3); j++)
         if ((pLoc->p[j] < pMin.p[j] - CLOSE) || (pLoc->p[j] > pMax.p[j] + CLOSE))
            fInside = FALSE;
      if (!fInside) {
         // trivially rejected, so it's not inside
         plCPMInsideA->Add (&fInside);
         continue;
         }

      // insersect against 3 axis
      fSum = 0;
      for (j = 0; j < 3; j++)
         fSum += IntersectAxisWithSurface (pLoc, j, plCPMSideB, plCPMVertB);
      fInside = (fSum > 0);

      plCPMInsideA->Add (&fInside);
   }

   // if add a buffer around any points adjacent are also marked as inside
   if (fAddBuffer) {
      BOOL *paf = (BOOL*) plCPMInsideA->Get(0);

      for (i = 0; i < dwNumSideA; i++) {
         for (j = 0; j < psa[i].Num(); j++)
            if (paf[psa[i].m_adwVert[j]] == TRUE)
               break;
         if (j >= psa[i].Num())
            continue;   // no verices inside

         // else, one vertex is inside, so mark the other ones in this
         // tirangle the same way
         for (j = 0; j < psa[i].Num(); j++)
            if (paf[psa[i].m_adwVert[j]] == FALSE)
               paf[psa[i].m_adwVert[j]] = 2; // special tag
      }  // i

      // go bacj over and set all 2's to TRUE
      for (i = 0; i < dwNumVertA; i++)
         if (paf[i] == 2)
            paf[i] = TRUE;
   }

}


/**********************************************************************************
MergeTwoSurfaces - Merges surface B into surface A. Bother surfaces MUST be in the
same coordinate system.

NOTE: the lists for side A will definitely be changed. Those from side B may be changed

inputs
   PCListFixed       plCPMSideA - Side A
   PCListFixed       plCPMVertA - Vertex A
   PCListFixed       plCPMSideB - Side B
   PCListFixed       plCPMVertB - Side B
   BOOL              fSmoothing - if TRUE then do extra smoothing on the joint.
returns
   BOOL - TRUE if success, FALSE if fail
*/
typedef struct {  // info used for the border
   PCListFixed    plBorder;      // pointer ot a list of points
   BOOL           fLoop;         // set to TRUE if looped
   fp             fLen;          // length
} MSBI, *PMSBI;

static int _cdecl MSBICompare (const void *elem1, const void *elem2)
{
   MSBI *pdw1, *pdw2;
   pdw1 = (MSBI*) elem1;
   pdw2 = (MSBI*) elem2;

   if (pdw1->fLoop != pdw2->fLoop)
      return (int) pdw1->fLoop - (int) pdw2->fLoop;
   if (pdw1->fLen > pdw2->fLen)
      return -1;  // so that highest lens on top
   else if (pdw1->fLen < pdw2->fLen)
      return 1;
   else return 0;
   //return (int) (*pdw1) - (int)(*pdw2);
}
BOOL MergeTwoSurfaces (PCListFixed plCPMSideA, PCListFixed plCPMVertA,
                       PCListFixed plCPMSideB, PCListFixed plCPMVertB,
                       BOOL fSmoothing)
{
   PCListFixed aplSide[2], aplVert[2];
   aplSide[0] = plCPMSideA;
   aplSide[1] = plCPMSideB;
   aplVert[0] = plCPMVertA;
   aplVert[1] = plCPMVertB;

   // find out what points are inside
   CListFixed alInside[2];
   BOOL *apafInside[2];
   DetermineWhichVerticesInside (plCPMSideA, plCPMVertA, plCPMSideB, plCPMVertB, &alInside[0], fSmoothing);
   DetermineWhichVerticesInside (plCPMSideB, plCPMVertB, plCPMSideA, plCPMVertA, &alInside[1], fSmoothing);
   apafInside[0] = (BOOL*) alInside[0].Get(0);
   apafInside[1] = (BOOL*) alInside[1].Get(0);

   // determine which sides are kept (outside ones), and which ones are on the boundary
   CListFixed alKeep[2], alBorder[2];
   PCPMSideOld ps;
   PCPMVertOld pv;
   DWORD dwSurf, i, j, k, dwIn, dwOut;
   for (dwSurf = 0; dwSurf < 2; dwSurf++) {
      alKeep[dwSurf].Init (sizeof(DWORD));
      alBorder[dwSurf].Init (sizeof(DWORD));

      ps = (PCPMSideOld) aplSide[dwSurf]->Get(0);
      pv = (PCPMVertOld) aplVert[dwSurf]->Get(0);

      for (i = 0; i < aplSide[dwSurf]->Num(); i++) {
         dwIn = dwOut = 0;

         for (j = 0; j < ps[i].Num(); j++) {
            if ((apafInside[dwSurf])[ps[i].m_adwVert[j]])
               dwIn++;
            else
               dwOut++;
         }

         if (dwOut && !dwIn)
            alKeep[dwSurf].Add (&i);
         else if (dwIn && (dwOut > 1)) // if only one point outside then useful for determining border
            alBorder[dwSurf].Add (&i);
      }
   }

   // will combine all the points and sides together into a new list
   // before do that, create remap lists for what surface numbers are remapped
   // to the other surface numbers, and what vertex numbers are remapped
   CListFixed alRemapSide[2][2], alRemapVert[2][2];
   DWORD dwCurSide, dwCurVert, dwFind;
   dwCurSide = dwCurVert = 0;
   for (dwSurf = 0; dwSurf < 2; dwSurf++) {
      alRemapSide[dwSurf][0].Init (sizeof(DWORD)); // from
      alRemapSide[dwSurf][1].Init (sizeof(DWORD)); // to
      alRemapVert[dwSurf][0].Init (sizeof(DWORD)); // from
      alRemapVert[dwSurf][1].Init (sizeof(DWORD)); // to

      // loop over all the sides
      for (i = 0; i < aplSide[dwSurf]->Num(); i++) {
         alRemapSide[dwSurf][0].Add (&i);

         // does it appear in the list of sides to keep
         dwFind = DWORDSearch (i, alKeep[dwSurf].Num(), (DWORD*) alKeep[dwSurf].Get(0));
         if (dwFind == -1)
            alRemapSide[dwSurf][1].Add (&dwFind);  // since doesnt exist in new one remap to -1
         else {
            alRemapSide[dwSurf][1].Add (&dwCurSide);  // else know will be sequentially added
            dwCurSide++;
         }
      }

      // loop over all the vertexes
      dwFind = -1;
      for (i = 0; i < aplVert[dwSurf]->Num(); i++) {
         alRemapVert[dwSurf][0].Add (&i);

         // does it appear in the list of Verts to keep
         if ((apafInside[dwSurf])[i])
            alRemapVert[dwSurf][1].Add (&dwFind);  // since doesnt exist in new one remap to -1
         else {
            alRemapVert[dwSurf][1].Add (&dwCurVert);  // else know will be sequentially added
            dwCurVert++;
         }
      } // i
   }

   // create new list to store the new points in
   CListFixed lNewVert, lNewSide;
   lNewVert.Init (sizeof(CPMVertOld));
   lNewSide.Init (sizeof(CPMSideOld));
   
   // transfer over
   DWORD *padw;
   CPMVertOld Vert;
   CPMSideOld Side;
   for (dwSurf = 0; dwSurf < 2; dwSurf++) {
      ps = (PCPMSideOld) aplSide[dwSurf]->Get(0);
      pv = (PCPMVertOld) aplVert[dwSurf]->Get(0);

      // transfer over the sides that will keep
      padw = (DWORD*) alKeep[dwSurf].Get(0);
      for (i = 0; i < alKeep[dwSurf].Num(); i++) {
         ps[padw[i]].CloneTo (&Side);
         Side.VertRename (alRemapVert[dwSurf][0].Num(), (DWORD*)alRemapVert[dwSurf][0].Get(0),
            (DWORD*)alRemapVert[dwSurf][1].Get(0));
         lNewSide.Add (&Side);
      }

      // transfer over all the verices that will keep
      for (i = 0; i < aplVert[dwSurf]->Num(); i++) {
         if ((apafInside[dwSurf])[i])
            continue;   // dont include inside ones

         pv[i].CloneTo (&Vert);
         Vert.SideRename (alRemapSide[dwSurf][0].Num(), (DWORD*) alRemapSide[dwSurf][0].Get(0),
            (DWORD*) alRemapSide[dwSurf][1].Get(0));
         lNewVert.Add (&Vert);
      }

   }


   // create a list of borders - each border is a CListFixed
   CListFixed alBorders[2];
   CListFixed alMSBI[2];   // border info
   PCListFixed plBorder;
   for (dwSurf = 0; dwSurf < 2; dwSurf++) {
      alBorders[dwSurf].Init (sizeof(PCListFixed));
      ps = (PCPMSideOld) aplSide[dwSurf]->Get(0);

      while (alBorder[dwSurf].Num()) {   // repeat while there are borders left
         // create a new border
         plBorder = new CListFixed;
         plBorder->Init (sizeof(DWORD));

         // add elements
         while (alBorder[dwSurf].Num()) {
            DWORD *padw = (DWORD*) alBorder[dwSurf].Get(0);
            DWORD dwNumBord = alBorder[dwSurf].Num();
            DWORD dwLookFor = plBorder->Num() ? *((DWORD*)plBorder->Get(plBorder->Num()-1)) : 0;
            DWORD dwUse = 0;
            PCPMSideOld psTemp;
            DWORD dwNext, dwMatch;

            // if there are already points in this border then look for them
            for (dwUse = 0; dwUse < dwNumBord; dwUse++) {
               // see if this point (dwLookFor) is in any of the points
               psTemp = &ps[padw[dwUse]];

               if (plBorder->Num()) {
                  for (j = 0; j < psTemp->Num(); j++)
                     if (psTemp->m_adwVert[j] == dwLookFor)
                        break;
                  if (j >= psTemp->Num())
                     continue;   // couldnt find it here

                  // verify that the next point isn't inside
                  dwNext = psTemp->m_adwVert[(j+1)%psTemp->Num()];
                  //if (dwNext == -1)
                  //   continue;   // already walked through this and is no good
                  if ((apafInside[dwSurf])[dwNext])
                     continue;   // inside
               }
               else {   // no border, so pick one
                  // find the first point after one that's hidden
                  for (j = 0; j < psTemp->Num(); j++) {
                     dwNext = psTemp->m_adwVert[(j+psTemp->Num()-1)%psTemp->Num()];
                     //if (dwNext == -1)
                     //   continue;   // already used this, so skip
                     if (!(apafInside[dwSurf])[dwNext])
                        continue;   // previous one must be inside

                     // this must be outside
                     dwNext = psTemp->m_adwVert[j];
                     //if (dwNext == -1)
                     //   continue;   // already used this, so no good
                     if ((apafInside[dwSurf])[dwNext])
                        continue;   // this one mut be outside

                     // and, there must be a next point that's valid
                     dwNext = psTemp->m_adwVert[(j+1)% psTemp->Num()];
                     //if (dwNext == -1)
                     //   continue;   // already used this, so no good
                     if ((apafInside[dwSurf])[dwNext])
                        continue;   // this one mut be outside

                     // this is the one we want then
                     break;
                  }
                  if (j >= psTemp->Num())
                     continue;   // couldn't find
               }

               // else, found a match
               dwMatch = j;
               break;
            }
            if (dwUse >= dwNumBord)
               break;   // couldnt find a follow-on
            
            // if there aren't any items added the write out the match point
            if (!plBorder->Num())
               plBorder->Add (&psTemp->m_adwVert[dwMatch]);

            // wipe out this value so know that used
            // no longer needed psTemp->m_adwVert[dwMatch] = -1;

            // loop until get a used point or an inside point
            for (dwMatch = dwMatch+1; ; dwMatch++) {
               dwMatch = dwMatch % psTemp->Num();
               dwNext = psTemp->m_adwVert[dwMatch];
               //if (dwNext == -1)
               //   break;   // end of sequence
               if ((apafInside[dwSurf])[dwNext])
                  break;   // this one mut be outside

               // else, add this to list
               plBorder->Add (&dwNext);
               //psTemp->m_adwVert[dwMatch] = -1;
            }

            // remove this side from the list
            alBorder[dwSurf].Remove (dwUse);

            // if got a complete loop then stop here
            if ((plBorder->Num() > 1) &&
               (*((DWORD*) plBorder->Get(0)) == *((DWORD*)plBorder->Get(plBorder->Num()-1))))
               break;
         }

         // if couldnt create a border then exit the entire loop
         if (!plBorder->Num()) {
            delete plBorder;
            break;
         }
         alBorders[dwSurf].Add (&plBorder);
      }  // while alBorder[i]

      // merge unlooped borders togehter
      while (TRUE) {
         BOOL fChanged;
         fChanged = FALSE;
         for (i = 0; i < alBorders[dwSurf].Num(); i++) {
            // if already looped then dont bother
            plBorder = *((PCListFixed*) alBorders[dwSurf].Get(i));
            if ((plBorder->Num() > 1) &&
               (*((DWORD*)plBorder->Get(0)) == *((DWORD*)plBorder->Get(plBorder->Num()-1))) )
               continue;   // looped

            for (j = 0; j < alBorders[dwSurf].Num(); j++) {
               PCListFixed plBorder2 = *((PCListFixed*) alBorders[dwSurf].Get(j));
               if (i == j)
                  continue;

               // if already looped then dont bother
               if ((plBorder2->Num() > 1) &&
                  (*((DWORD*)plBorder2->Get(0)) == *((DWORD*)plBorder2->Get(plBorder2->Num()-1))) )
                  continue;   // looped

               // only care if the end of border (i) is the start of border (j)
               if (*((DWORD*)plBorder2->Get(0)) != *((DWORD*)plBorder->Get(plBorder->Num()-1)))
                  continue;

               // else, append the borders
               for (k = 1; k < plBorder2->Num(); k++)
                  plBorder->Add (plBorder2->Get(k));
               delete plBorder2;
               alBorders[dwSurf].Remove (j);
               fChanged = TRUE;
               break;
            } // j
            if (fChanged)
               break;
          } // i

         // if didn't change anything nothing to merge
         if (!fChanged)
            break;
      }  // loop merging borders

      // info about the borders
      alMSBI[dwSurf].Init (sizeof(MSBI));
      pv = (PCPMVertOld) aplVert[dwSurf]->Get(0);
      for (i = 0; i < alBorders[dwSurf].Num(); i++) {
         DWORD *padw;
         CPoint pDist;
         fp fLen = 0;

         // if already looped then dont bother
         plBorder = *((PCListFixed*) alBorders[dwSurf].Get(i));
         padw = (DWORD*) plBorder->Get(0);

         for (j = 0; j+1 < plBorder->Num(); j++) {
            pDist.Subtract (&pv[padw[j]].m_pLoc, &pv[padw[j+1]].m_pLoc);
            fLen += pDist.Length();
         }

         MSBI ms;
         memset (&ms, 0, sizeof(ms));
         ms.fLen = fLen;
         ms.fLoop = (plBorder->Num() > 1) && (padw[0] == padw[plBorder->Num()-1]);
         ms.plBorder = plBorder;
         alMSBI[dwSurf].Add (&ms);
      }  // find longest border

      // sort this
      qsort (alMSBI[dwSurf].Get(0), alMSBI[dwSurf].Num(), sizeof(MSBI), MSBICompare);

   }  // dwSurf

   // zipper
   DWORD dwA, dwB;
   DWORD dwClosestLoop;
   fp fClosestLen, fLen;
   PMSBI pmA, pmB;
   PCPMVertOld pvA = (PCPMVertOld) aplVert[0]->Get(0);
   PCPMVertOld pvB = (PCPMVertOld) aplVert[1]->Get(0);
   for (dwA = 0; dwA < alMSBI[0].Num(); dwA++) {
      pmA = (PMSBI) alMSBI[0].Get(0);
      // go through all the matching ones and find the one with the least distance

      pmB = (PMSBI) alMSBI[1].Get(0);
      dwClosestLoop = -1;
      for (dwB = 0; dwB < alMSBI[1].Num(); dwB++) {
         // must both be looped the same
         if (pmA[dwA].fLoop != pmB[dwB].fLoop)
            continue;

         fLen = 0;
         // loop through each of the points in a x each of the points in b and
         // take the closest for all - this way find 2 cloest cutouts
         DWORD *padwA = (DWORD*) pmA[dwA].plBorder->Get(0);
         DWORD *padwB = (DWORD*) pmB[dwB].plBorder->Get(0);
         DWORD dwClosePoint;
         fp fClosePoint, fDist;
         CPoint pDist;
         for (i = 0; i < pmA[dwA].plBorder->Num(); i++) {
            dwClosePoint = -1;
            fClosePoint = 0;
            for (j = 0; j < pmB[dwB].plBorder->Num(); j++) {
               pDist.Subtract (&pvA[padwA[i]].m_pLoc, &pvB[padwB[j]].m_pLoc);
               fDist = pDist.Length();
               if ((dwClosePoint == -1) || (fDist < fClosePoint)) {
                  dwClosePoint = j;
                  fClosePoint = fDist;
               }
            }
            // include then closest point in th elength
            fLen += fClosePoint;
         }
         fLen /= (fp) pmA[dwA].plBorder->Num();

         // when get here know the total distance between the two borders
         // take closest
         if ((dwClosestLoop == -1) || (fLen < fClosestLen)) {
            dwClosestLoop = dwB;
            fClosestLen = fLen;
         }
      } // dwB

      // if didn't find a dwB then continue
      if (dwClosestLoop == -1)
         continue;

      // else, use this
      {
         PCListFixed plA = pmA[dwA].plBorder;
         PCListFixed plB = pmB[dwClosestLoop].plBorder;
         DWORD *padwA = (DWORD*) plA->Get(0);
         DWORD *padwB = (DWORD*) plB->Get(0);
         DWORD dwNumA = plA->Num();
         DWORD dwNumB = plB->Num();

         // reverse side A
         DWORD dwTemp;
         for (i = 0; i < dwNumA/2; i++) {
            dwTemp = padwA[i];
            padwA[i] = padwA[dwNumA-i-1];
            padwA[dwNumA-i-1] = dwTemp;
         }

         // looped?
         BOOL fLoopA, fLoopB;
         fLoopA = (dwNumA > 1) && (padwA[0] == padwA[dwNumA-1]);
         if (fLoopA)
            dwNumA--;
         fLoopB = (dwNumB > 1) && (padwB[0] == padwB[dwNumB-1]);
         if (fLoopB)
            dwNumB--;

         // if their loop status isn't the same then no zipper
         if ((fLoopA != fLoopB) || (dwNumA < 2) || (dwNumB < 2))
            goto nozipper;

         // if B is looped then create find the closest point in B to the start of A
         DWORD dwClosestA, dwClosestB;
         fp fClosest, fLen;
         CPoint pDist;
         if (fLoopB) {
            dwClosestA = dwClosestB = -1;
            for (j = 0; j < dwNumA; j++) for (i = 0; i < dwNumB; i++) {
               pDist.Subtract (&pvA[padwA[j]].m_pLoc, &pvB[padwB[i]].m_pLoc);
               fLen = pDist.Length();
               if ((dwClosestA == -1) || (fLen < fClosest)) {
                  dwClosestB = i;
                  dwClosestA = j;
                  fClosest = fLen;
               }
            }
         }
         else
            dwClosestA = dwClosestB = 0;

         // loop through all of point son left and right adding triangles
         DWORD dwCurA, dwCurB, dwLeftA, dwLeftB, dwOrigA, dwOrigB;
         dwCurA = dwClosestA;
         dwCurB = dwClosestB;
         dwLeftA = dwOrigA = (plA->Num() - (fLoopA ? 0 : 1));   // BUGFIX - So will go all the way around if looped
         dwLeftB = dwOrigB = (plB->Num() - (fLoopB ? 0 : 1));
         while (dwLeftA || dwLeftB) {
            // BUGFIX - Calculate the progress as a percentage. Then, use that to scale
            // the distance to encourage loop around
            fp fProgA, fProgB, fAScale, fBScale;
            fProgA = 1 - (fp)dwLeftA / (fp) dwOrigA;
            fProgB = 1 - (fp)dwLeftB / (fp) dwOrigB;
#define KEEPTOLOOP         2       // the higher KEEPTOLOOP the more pressure there is to stay point-wise aligned
            if (fProgA > fProgB)
               fAScale = 1 + KEEPTOLOOP * (fProgA - fProgB);
            else
               fAScale = 1.0 / (1 + KEEPTOLOOP * (fProgB - fProgA));
            fBScale = 1.0 / fAScale;

            // figure out which triangle to use, one of two choices.
            // both triangles use two points from dwCur1 to dwCur2
            // however, third point can eitehr be (dwCur1+1) (tri A) or (dwCur2+1) (triB).
            // use whichever one has the shortest length
            CPoint pt;
            fp fADist, fBDist;
            //pt.Subtract (&p1->p, &p2->p);
            fADist = fBDist = 0; // should include pt.Length(), but makes no different to final comparison
            if (dwLeftA) {
               pt.Subtract (&pvB[padwB[dwCurB%dwNumB]].m_pLoc,&pvA[padwA[(dwCurA+1)%dwNumA]].m_pLoc);
               fADist = pt.Length() * fAScale;
               //pt.Subtract (&pvA[padwA[dwCurA%dwNumA]].m_pLoc,&pvA[padwA[(dwCurA+1)%dwNumA]].m_pLoc);
               //fADist += pt.Length();
            }
            if (dwLeftB) {
               pt.Subtract (&pvA[padwA[dwCurA%dwNumA]].m_pLoc,&pvB[padwB[(dwCurB+1)%dwNumB]].m_pLoc);
               fBDist = pt.Length() * fBScale;
               //pt.Subtract (&pvB[padwB[dwCurB%dwNumB]].m_pLoc,&pvB[padwB[(dwCurB+1)%dwNumB]].m_pLoc);
               //fBDist += pt.Length();
            }

            // take whichever triangle has the smallest perimiter
            BOOL fTakeA;
            if (dwLeftA && dwLeftB)
               fTakeA = (fADist < fBDist);
            else
               fTakeA = (dwLeftA ? TRUE : FALSE);

            CPMSideOld Side;
            memset (&Side, 0, sizeof(Side));
            Side.m_adwVert[3] = -1;
            if (fTakeA)
               Side.m_adwVert[0] = *((DWORD*)alRemapVert[0][1].Get(padwA[(dwCurA+1)%dwNumA]));
            else
               Side.m_adwVert[0] = *((DWORD*)alRemapVert[1][1].Get(padwB[(dwCurB+1)%dwNumB]));
            Side.m_adwVert[1] = *((DWORD*)alRemapVert[0][1].Get(padwA[dwCurA%dwNumA]));
            Side.m_adwVert[2] = *((DWORD*)alRemapVert[1][1].Get(padwB[dwCurB%dwNumB]));

   #ifdef _DEBUG
            for (i = 0; i < 3; i++)
               if (Side.m_adwVert[i] >= lNewVert.Num())
                  Side.m_adwVert[i] = 0;
   #endif

            // add this
            lNewSide.Add (&Side);

            // move up pointers
            if (fTakeA) {
               dwCurA++;
               dwLeftA--;
            }
            else {
               dwCurB++;
               dwLeftB--;
            }
         } // while dwLeftA and dwLeftB
      } // draw ziper
   nozipper:


      // remove it from the list
      alMSBI[1].Remove (dwClosestLoop);

   }  // dwA


   // free up the borders
   for (dwSurf = 0; dwSurf < 2; dwSurf++) {
      for (i = 0; i < alBorders[dwSurf].Num(); i++) {
         plBorder = *((PCListFixed*) alBorders[dwSurf].Get(i));
         delete plBorder;
      }
      alBorders[dwSurf].Clear();
   }

   // free up the old side A, which is what's going to be replaces
   ps = (PCPMSideOld) aplSide[0]->Get(0);
   pv = (PCPMVertOld) aplVert[0]->Get(0);
   for (i = 0; i < aplSide[0]->Num(); i++)
      ps[i].Release ();
   for (i = 0; i < aplVert[0]->Num(); i++)
      pv[i].Release ();

   // clone over the changes
   aplSide[0]->Init (sizeof(CPMSideOld), lNewSide.Get(0), lNewSide.Num());
   aplVert[0]->Init (sizeof(CPMVertOld), lNewVert.Get(0), lNewVert.Num());

   // done
   return TRUE;
}

/**********************************************************************************
MergeTwoSurfacesWithTransform - Merges surface B into surface A. Surface B is copied
and then transformed by the given transformation. The transformed version is merged

NOTE: the lists for side A will definitely be changed. Those from side B may be changed

inputs
   PCListFixed       plCPMSideA - Side A
   PCListFixed       plCPMVertA - Vertex A
   PCListFixed       plCPMSideB - Side B
   PCListFixed       plCPMVertB - Vertex B
   PCMatrix          pmTrans - Transformation
   BOOL              fFlip - If TRUE, rotate the sides from clockwise to counterclockwise
   PCListFixed       plPCOEAttribA - Morphs for side A, added to
   PCListFixed       plPCOEAttribB - Morphs for side B, added to A (unless duplicate)
   PWSTR             pszMorphAppend - If non-null, this string is appended to the end of
                     the morph (on side B) name so can get "(L)" vs "(R)"
   BOOL              fSmoothing - If TRUE cut out one extra level of polygons for extra smoothing
returns
   BOOL - TRUE if success, FALSE if fail
*/
BOOL MergeTwoSurfacesWithTransform (PCListFixed plCPMSideA, PCListFixed plCPMVertA,
                       PCListFixed plCPMSideB, PCListFixed plCPMVertB,
                       PCMatrix pmTrans, BOOL fFlip,
                       PCListFixed plPCOEAttribA, PCListFixed plPCOEAttribB,
                       PWSTR pszMorphAppend, BOOL fSmoothing)
{
   // create matrix to modify normals
   CMatrix mInvTrans;
   pmTrans->Invert (&mInvTrans);
   mInvTrans.Transpose();

   // clone
   CListFixed lNewSide, lNewVert;
   lNewSide.Init (sizeof(CPMSideOld), plCPMSideB->Get(0), plCPMSideB->Num());
   lNewVert.Init (sizeof(CPMVertOld), plCPMVertB->Get(0), plCPMVertB->Num());
   PCPMSideOld psB, psNew;
   PCPMVertOld pvB, pvNew;
   psB = (PCPMSideOld) plCPMSideB->Get(0);
   pvB = (PCPMVertOld) plCPMVertB->Get(0);
   psNew = (PCPMSideOld) lNewSide.Get(0);
   pvNew = (PCPMVertOld) lNewVert.Get(0);
   DWORD i;
   for (i = 0; i < lNewSide.Num(); i++) {
      psB[i].CloneTo (&psNew[i]);
      if (fFlip)
         psNew[i].Reverse();
   }
   for (i = 0; i < lNewVert.Num(); i++) {
      pvB[i].CloneTo (&pvNew[i]);
      pvNew[i].Scale (pmTrans, &mInvTrans);
   }

   // figure out the max # of real morphs on side A and B
   DWORD dwMaxA, dwMaxB;
   dwMaxA = NumMorphIDs (plPCOEAttribA);
   dwMaxB = NumMorphIDs (plPCOEAttribB);

   // create the remap list
   CListFixed lRemapFrom, lRemapTo;
   lRemapFrom.Init (sizeof(DWORD));
   lRemapTo.Init (sizeof(DWORD));

   // loop through all the morphs in side B and find the remap number
   PCOEAttrib *papB = (PCOEAttrib*) plPCOEAttribB->Get(0);
   PCOEAttrib *papA = (PCOEAttrib*) plPCOEAttribA->Get(0);
   PCOEAttrib paFrom;
   DWORD j,k, dwFind;
   for (i = 0; i < dwMaxB; i++) {
      // find a match
      for (j = 0; j < plPCOEAttribB->Num(); j++) {
         if ((papB[j]->m_dwType != 2) || !papB[j]->m_lCOEATTRIBCOMBO.Num())
            continue;   // must be of the right type
         PCOEATTRIBCOMBO pc;
         pc = (PCOEATTRIBCOMBO) papB[j]->m_lCOEATTRIBCOMBO.Get(0);
         dwFind = pc->dwMorphID;
         if (dwFind == i)
            break;
      }
      if (j >= plPCOEAttribB->Num()) {
         // if find this delete it
         lRemapFrom.Add (&i);
         dwFind = -1;
         lRemapTo.Add (&dwFind);
         continue;
      }
      paFrom = papB[j];

      // know where it's from, figre out what it's new name will be
      WCHAR szTemp[64];
      wcscpy (szTemp, paFrom->m_szName);
      if (pszMorphAppend) {
         szTemp[63-wcslen(pszMorphAppend)] = 0; // make sure string doesnt get too long
         wcscat (szTemp, pszMorphAppend);
      }

      // see which one in the original that it matches
      DWORD dwStringMatch;
      BOOL fWasSubMorph;
      dwStringMatch = -1;
      fWasSubMorph = FALSE;
      for (j = 0; j < plPCOEAttribA->Num(); j++) {
         if (wcsicmp(papA[j]->m_szName, szTemp))
            continue;   // strings don't match
         dwStringMatch = j;

         // is it a sub morph, or an combo-morph
         fWasSubMorph = (papA[j]->m_dwType == 2);
         break;
      }

      // if the strings matches BUT it wasn't a sub-morph then need to invent a new
      // name for this and pretend it doesn't match
      if ((dwStringMatch != -1) && !fWasSubMorph) {
         MakeAttribNameUnique (szTemp, plPCOEAttribA);
         dwStringMatch = -1;
      }

      // if the strings match then use that for remap
      if (dwStringMatch != -1) {
         PCOEATTRIBCOMBO pc = (PCOEATTRIBCOMBO)papA[dwStringMatch]->m_lCOEATTRIBCOMBO.Get(0);
         dwFind = pc->dwMorphID;
         lRemapFrom.Add (&i);
         lRemapTo.Add (&dwFind);
         continue;   // go on
      }

      // else, add a new one to A
      PCOEAttrib pNew;
      pNew = new COEAttrib;
      if (!pNew)
         continue;   // error
      lRemapFrom.Add (&i);
      lRemapTo.Add (&dwMaxA);
      paFrom->CloneTo (pNew);
      wcscpy (pNew->m_szName, szTemp);
      PCOEATTRIBCOMBO pc;
      pc = (PCOEATTRIBCOMBO) pNew->m_lCOEATTRIBCOMBO.Get(0);
      if (pc)
         pc->dwMorphID = dwMaxA;
      dwMaxA++;
      plPCOEAttribA->Add (&pNew);
      papA = (PCOEAttrib*) plPCOEAttribA->Get(0);  // reload because adding may cause ESCREALLOC
   }
   DWORD *padwRemapFrom, *padwRemapTo;
   padwRemapFrom = (DWORD*) lRemapFrom.Get(0);
   padwRemapTo = (DWORD*) lRemapTo.Get(0);

   // now, go through and transfer over all the unique combo attributes, with
   // a remap
   for (i = 0; i < plPCOEAttribB->Num(); i++) {
      paFrom = papB[i];
      if (paFrom->m_dwType != 0)
         continue;   // only care about the combo

      WCHAR szTemp[64];
      wcscpy (szTemp, paFrom->m_szName);
      if (pszMorphAppend) {
         szTemp[63-wcslen(pszMorphAppend)] = 0; // make sure string doesnt get too long
         wcscat (szTemp, pszMorphAppend);
      }

      // find a match in A? Basically, tell it to create a unique name for A.
      // if the name changes then it matched
      WCHAR szUnique[64];
      wcscpy (szUnique, szTemp);
      MakeAttribNameUnique (szUnique, plPCOEAttribA);

      // if the name changed then A already has the morph, so don't do anything else
      // to propogate this
      // NOTE: Assuming sub-morphs are the same. This is not necessarily the case.
      if (wcsicmp(szTemp, szUnique))
         continue;

      // else, go on and create a new one
      PCOEAttrib pNew;
      pNew = new COEAttrib;
      if (!pNew)
         continue;
      paFrom->CloneTo (pNew);

      wcscpy (pNew->m_szName, szUnique);

      // need to remap the sub-morphs this references
      PCOEATTRIBCOMBO pc;
      DWORD dwTo;
      pc = (PCOEATTRIBCOMBO) pNew->m_lCOEATTRIBCOMBO.Get(0);
      for (j = 0; j < pNew->m_lCOEATTRIBCOMBO.Num(); j++, pc++) {
         for (k = 0; k < lRemapFrom.Num(); k++) {
            if (pc->dwMorphID == padwRemapFrom[k])
               break;
         } // k
         if (k < lRemapFrom.Num())
            dwTo = padwRemapTo[k];
         else
            dwTo = -1;
         // NOTE: Should be getting any deletion of morphs here, so if do,
         // just set to 0
         if (dwTo == -1)
            dwTo = 0;
         pc->dwMorphID = dwTo;
      }  // j

      // add the new one
      plPCOEAttribA->Add (&pNew);
   } // i - all morphs

   // loop through all the verices remapping morphs
   for (i = 0; i < lNewVert.Num(); i++)
      pvNew[i].DeformRename (lRemapFrom.Num(), padwRemapFrom, padwRemapTo);

   // now that have everything transformed, do the merge
   BOOL fRet;
   fRet = MergeTwoSurfaces (plCPMSideA, plCPMVertA, &lNewSide, &lNewVert, fSmoothing);

   // free up the scratch space
   psNew = (PCPMSideOld) lNewSide.Get(0);
   pvNew = (PCPMVertOld) lNewVert.Get(0);
   for (i = 0; i < lNewSide.Num(); i++)
      psNew[i].Release ();
   for (i = 0; i < lNewVert.Num(); i++)
      pvNew[i].Release ();

   return fRet;
}


/**********************************************************************************
CObjectPolyMeshOld::Merge - Merges in pMerge with this polygon mesh.

inputs
   PCObjectPolyMeshOld     pMerge - Merge this into the current one (without changing it)
   PCMatrix             pmTrans - Transforms the other object's coords into this objects coords
returns
   BOOL - TRUE if success
*/
BOOL CObjectPolyMeshOld::Merge (CObjectPolyMeshOld *pMerge, PCMatrix pmTrans)
{
   // find the bounding box for the other merge object
   CPoint pMin, pMax;
   CPoint pLoc;
   DWORD i;
   BOOL fRet = TRUE;
   PCPMVertOld pv = (PCPMVertOld) pMerge->m_lCPMVert.Get(0);
   for (i = 0; i < pMerge->m_lCPMVert.Num(); i++) {
      pLoc.Copy (&pv[i].m_pLoc);
      pLoc.p[3] = 1;
      pLoc.MultiplyLeft (pmTrans);

      if (i) {
         pMin.Min (&pLoc);
         pMax.Max (&pLoc);
      }
      else {
         pMin.Copy (&pLoc);
         pMax.Copy (&pLoc);
      }
   }

   // zero out symmetry bits if would end up with conflict
   DWORD dwSym;
   dwSym = m_dwSymmetry;
   for (i = 0; i < 3; i++)
      if (pMin.p[i] * pMax.p[i] <= 0)
         dwSym &= ~(1 << i);  // flipped object would overlap with itself so cant flip

   // merge in
   DWORD x, y, z;
   CMatrix mScale;
   BOOL fFlip;
   WCHAR szAppend[32];
   for (x = 0; x < (DWORD) ((dwSym & 0x01) ? 2 : 1); x++)
   for (y = 0; y < (DWORD) ((dwSym & 0x02) ? 2 : 1); y++)
   for (z = 0; z < (DWORD) ((dwSym & 0x04) ? 2 : 1); z++) {
      // is scaling necessary
      mScale.Scale (x ? -1 : 1, y ? -1 : 1, z ? -1 : 1);
      fFlip = ((x + y + z) % 2) ? TRUE : FALSE;
      
      // symmetry string?
      szAppend[0] = 0;
      if (dwSym) {
         wcscpy (szAppend, L" (");
         if (dwSym & 0x01) {
            if ((pMin.p[0] + pMax.p[0]) * (x ? -1 : 1) >= 0)
               wcscat (szAppend, L"R");
            else
               wcscat (szAppend, L"L");
         }
         if (dwSym & 0x02) {
            if ((pMin.p[1] + pMax.p[1]) * (y ? -1 : 1) >= 0)
               wcscat (szAppend, L"B");
            else
               wcscat (szAppend, L"F");
         }
         if (dwSym & 0x04) {
            if ((pMin.p[2] + pMax.p[2]) * (z ? -1 : 1) >= 0)
               wcscat (szAppend, L"T");
            else
               wcscat (szAppend, L"B");
         }
         wcscat (szAppend, L")");
      }

      // include other translation
      mScale.MultiplyLeft (pmTrans);

      // merge
      if (!MergeTwoSurfacesWithTransform (&m_lCPMSide, &m_lCPMVert, &pMerge->m_lCPMSide,
         &pMerge->m_lCPMVert, &mScale, fFlip, &m_lPCOEAttrib, &pMerge->m_lPCOEAttrib, szAppend,
         m_fMergeSmoothing | pMerge->m_fMergeSmoothing))
         fRet = FALSE;
   }

   // Sort m_lPCOEAttrib list
   qsort (m_lPCOEAttrib.Get(0), m_lPCOEAttrib.Num(), sizeof(PCOEAttrib), OECALCompare);

   // set flags so recalc
   m_fDivDirty = TRUE;
   m_fVertGroupDirty = TRUE;
   m_dwCurVertGroup = -1;
   m_dwDivisions = 0;   // so know that this is no longer an automatically generated object
   CalcScale ();
   CalcMorphRemap(); // redo morph remap since may have changed

   return fRet;
}


#if 0
/**********************************************************************************
CObjectPolyMeshOld::Message -
sends a message to the object. The interpretation of the message depends upon
dwMessage, which is OSM_XXX. If the function understands and handles the
message it returns TRUE, otherwise FALE.
*/
BOOL CObjectPolyMeshOld::Message (DWORD dwMessage, PVOID pParam)
{
   switch (dwMessage) {
   case OSM_POLYMESH:
      {
         POSMPOLYMESH p = (POSMPOLYMESH) pParam;
         p->ppm = this;
      }
      return TRUE;

   }

   return FALSE;
}

#endif // 0
/*****************************************************************************************
CObjectPolyMeshOld::Merge -
asks the object to merge with the list of objects (identified by GUID) in pagWith.
dwNum is the number of objects in the list. The object should see if it can
merge with any of the ones in the list (some of which may no longer exist and
one of which may be itself). If it does merge with any then it return TRUE.
if no merges take place it returns false.
*/
BOOL CObjectPolyMeshOld::Merge (GUID *pagWith, DWORD dwNum)
{
   BOOL fRet = FALSE;
   DWORD i;
   for (i = 0; i < dwNum; i++) {
      DWORD dwFind;

      // make sure it's not this object
      if (IsEqualIID (pagWith[i], m_gGUID))
         continue;

      dwFind = m_pWorld->ObjectFind (&pagWith[i]);
      if (dwFind == -1)
         continue;
      PCObjectSocket pos;
      pos = m_pWorld->ObjectGet (dwFind);
      if (!pos)
         continue;

      continue;   // BUGBUG - So kind of workds
#if 0 // BUGBUG - not using
      // send a message to see if it is another struct surface
      OSMPOLYMESH os;
      memset (&os, 0, sizeof(os));
      if (!pos->Message (OSM_POLYMESH, &os))
         continue;
      if (!os.ppm)
         continue;

      // dont merge with self
      if (os.ppm == this)
         continue;

      // inform that about to change
      m_pWorld->ObjectAboutToChange (this);

      // figure out transformation
      CMatrix mTrans, mInv;
      pos->ObjectMatrixGet (&mTrans);
      m_MatrixObject.Invert4 (&mInv);
      mTrans.MultiplyRight (&mInv);

      // merge it
      fRet = TRUE;
      if (Merge (os.ppm, &mTrans)) {
         // delete the other object
         m_pWorld->ObjectRemove (dwFind);
      }

      // inform that changed
      m_pWorld->ObjectChanged (this);
#endif // 0
   }

   return fRet;
}


/*****************************************************************************************
CObjectPolyMeshOld::AttribGetIntern - OVERRIDE THIS

Like AttribGet() except that only called if default attributes not handled.
*/
BOOL CObjectPolyMeshOld::AttribGetIntern (PWSTR pszName, fp *pfValue)
{
   // look through the attributes
   PCOEAttrib *pap = (PCOEAttrib*) m_lPCOEAttrib.Get(0);
   DWORD dwNum = m_lPCOEAttrib.Num();
   if (!dwNum)
      return FALSE;

   COEAttrib aFind, *paFind;
   PCOEAttrib *pp;
   wcscpy (aFind.m_szName, pszName);
   paFind = &aFind;
   pp = (PCOEAttrib*) bsearch (&paFind, pap, dwNum, sizeof (PCOEAttrib*), OECALCompare);
   if (!pp)
      return FALSE;

   // else got it
   *pfValue = pp[0]->m_fCurValue;
   return TRUE;
}


/*****************************************************************************************
CObjectPolyMeshOld::AttribGetAllIntern - OVERRIDE THIS

Like AttribGetAllIntern() EXCEPT plATTRIBVAL is already initialized and filled with
some parameters (default to the object template)
*/
void CObjectPolyMeshOld::AttribGetAllIntern (PCListFixed plATTRIBVAL)
{
   PCOEAttrib *pap = (PCOEAttrib*) m_lPCOEAttrib.Get(0);
   DWORD dwNum = m_lPCOEAttrib.Num();
   DWORD i;
   if (!dwNum)
      return;

   ATTRIBVAL av;
   memset (&av, 0, sizeof(av));
   for (i = 0; i < dwNum; i++) {
      wcscpy (av.szName, pap[i]->m_szName);
      av.fValue = pap[i]->m_fCurValue;
      plATTRIBVAL->Add (&av);
   }
}


/*****************************************************************************************
CObjectPolyMeshOld::AttribSetGroupIntern - OVERRIDE THIS

Like AttribSetGroup() except passing on non-template attributes.
*/
void CObjectPolyMeshOld::AttribSetGroupIntern (DWORD dwNum, PATTRIBVAL paAttrib)
{
   BOOL fSentChanged = FALSE;

   DWORD i;
   COEAttrib aFind, *paFind;
   PCOEAttrib *pp;
   paFind = &aFind;
   PCOEAttrib *pap = (PCOEAttrib*) m_lPCOEAttrib.Get(0);
   DWORD dwNumAttrib = m_lPCOEAttrib.Num();
   for (i = 0; i < dwNum; i++) {
      wcscpy (aFind.m_szName, paAttrib[i].szName);
      pp = (PCOEAttrib*) bsearch (&paFind, pap, dwNumAttrib, sizeof (PCOEAttrib*), OECALCompare);
      if (!pp)
         continue;   // not known

      if (pp[0]->m_fCurValue == paAttrib[i].fValue)
         continue;   // no change

      // else, attribute
      if (!fSentChanged) {
         fSentChanged = TRUE;
         m_pWorld->ObjectAboutToChange (this);
      }
      pp[0]->m_fCurValue = paAttrib[i].fValue;
   }

   if (fSentChanged) {
      m_fDivDirty = TRUE;
      CalcMorphRemap();
      m_pWorld->ObjectChanged (this);
   }
}


/*****************************************************************************************
CObjectPolyMeshOld::AttribInfoIntern - OVERRIDE THIS

Like AttribInfo() except called if attribute is not for template.
*/
BOOL CObjectPolyMeshOld::AttribInfoIntern (PWSTR pszName, PATTRIBINFO pInfo)
{
   // look through the attributes
   PCOEAttrib *pap = (PCOEAttrib*) m_lPCOEAttrib.Get(0);
   DWORD dwNum = m_lPCOEAttrib.Num();
   if (!dwNum)
      return FALSE;

   COEAttrib aFind, *paFind;
   PCOEAttrib *pp;
   wcscpy (aFind.m_szName, pszName);
   paFind = &aFind;
   pp = (PCOEAttrib*) bsearch (&paFind, pap, dwNum, sizeof (PCOEAttrib*), OECALCompare);
   if (!pp)
      return FALSE;

   memset (pInfo, 0, sizeof(*pInfo));
   pInfo->dwType = pp[0]->m_dwInfoType;
   pInfo->fDefLowRank = pp[0]->m_fDefLowRank;
   pInfo->fDefPassUp = pp[0]->m_fDefPassUp;
   pInfo->fMax = pp[0]->m_fMax;
   pInfo->fMin = pp[0]->m_fMin;
   pInfo->pszDescription = ((pp[0]->m_memInfo.p) && ((PWSTR)pp[0]->m_memInfo.p)[0]) ?
      (PWSTR)pp[0]->m_memInfo.p : NULL;

   return TRUE;
}



// BUGBUG - Eventually need to get trid of this old object