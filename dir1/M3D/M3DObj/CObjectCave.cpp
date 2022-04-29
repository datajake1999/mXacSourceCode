/************************************************************************
CObjectCave.cpp - Draws a box.

begun 9/4/05 by Mike Rozak
Copyright 2005 Mike Rozak. All rights reserved
*/

#include <windows.h>
#include "escarpment.h"
#include "..\M3D.h"
#include "resource.h"

#define METABOUNDARY    0.5         // if >= this then inside the metaball
#define     TREEID            400

typedef struct {
   DWORD          dwMetaball; // metaball
   DWORD          dwMetaSurface; // meta surface index that this was created from
   DWORD          dwCanopy;   // canopy it's in
   DWORD          dwType;     // index into m_lCANOPYTREE in CCaveCanopy
   CMatrix        mMatrix;    // location, rotation, and scale matrix
} CAVEOBJECTINFO, *PCAVEOBJECTINFO;


// CCaveCanopy - For handling stalagtites/mites, rocks, stuff on walls
class DLLEXPORT CCaveCanopy {
public:
   ESCNEWDELETE;

   CCaveCanopy();
   ~CCaveCanopy();

   PCMMLNode2 MMLTo (void);
   BOOL MMLFrom (PCMMLNode2 pNode);
   CCaveCanopy *Clone (void);
   void DirtySet (void);

   BOOL Dialog (DWORD dwRenderShard, PCEscWindow pWindow, PCObjectCave pCave, BOOL fShowDistance);
   fp MaxTreeSize (DWORD dwRenderShard);
   //BOOL EnumTree (int x, int y, DWORD dwType, PCMatrix pMatrix, PCPoint pLoc, BYTE *pScore);
   //void TreesInRange (PCPoint pCorner1, PCPoint pCorner2, RECT *pr);
   PCObjectClone TreeClone (DWORD dwRenderShard, DWORD dwType);
   BOOL TextureQuery (DWORD dwRenderShard, PCListFixed plText);
   BOOL ColorQuery (DWORD dwRenderShard, PCListFixed plColor);
   BOOL ObjectClassQuery (DWORD dwRenderShard, PCListFixed plObj);
   BOOL AreEqual (PCCaveCanopy pOther);

   // set by user - note: After changing any of these DirtySet() must be called
   CListFixed        m_lCANOPYTREE;       // list of CANOPYTREE to use
   fp                m_fDistance;         // average distance between objects
   //TEXTUREPOINT      m_tpSeparation;      // typical X and Y separation of trees in this canopy
   //TEXTUREPOINT      m_tpSeparationVar;   // variation in separation. From 0 (exactly on grid) to 1 (full variation)
   CPoint            m_pRotationVar;      // variation in rotation. From 0 (no variation) to 1 (maximum variation)
   CPoint            m_pScaleVar;         // variation in scale. From 0 (no variation) to 1 (max variation).
                                          // note: p[4] is used for scaling xyz with one value so keep same proportions
   BOOL              m_fRotateDown;       // if true then automatically rotate down
   //DWORD             m_dwRepeatX;         // number of trees in X before pattern repeats. 1+
   //DWORD             m_dwRepeatY;         // number of trees in Y before pattern repeats. 1+
   //DWORD             m_dwSeed;            // random seed used to generate canopy
   //BOOL              m_fNoShadows;        // if TRUE then dont draw shadows
   DWORD             m_dwCount;           // count of number of different objects

   // scratch used to display bitmaps
   CListFixed        m_lDialogHBITMAP;    // list of bitmaps for thumbnails
   CListFixed        m_lDialogCOLORREF;   // list colorrefs for transparency
   PCObjectCave      m_pCave;             // cave this is in, only used for the dialog
   BOOL              m_fShowDistance;     // if TRUE show distance option


private:
   BOOL CalcTrees (DWORD dwRenderShard);

   // calculated
   BOOL              m_fDirty;            // set to TRUE if data has changed since last calc
   CListFixed        m_lPCObjectClone;    // list of clone objects, corresponding to m_lCANOPYTREE
   //CMem              m_memTREEINFO;       // location of trees and matrices
   fp                m_fMaxSize;          // maximum size of a tree - used as buffer to make sure all trees in bounding box
};
typedef CCaveCanopy *PCCaveCanopy;



// METAPOLY - polygon information for metaballs
typedef struct {
   WORD           awVert[4];     // verticies, index into m_lPoint, if only tringle last value is -1
   CPoint         p1;            // normal for entire surface for some calcs. Changed to min XYZ
   CPoint         p2;            // once the the normals have all been calculated, changed to max XYZ
} METAPOLY, *PMETAPOLY;

// CMetaball - for storing metaball information
class CMetaball;
typedef CMetaball *PCMetaball;
class CMetaSurface;
typedef CMetaSurface *PCMetaSurface;

class CMetaball {
public:
   ESCNEWDELETE;

   CMetaball (void);
   ~CMetaball (void);

   PCMMLNode2 MMLTo (void);
   BOOL MMLFrom (PCMMLNode2 pNode);
   BOOL CloneTo (CMetaball *pTo);
   CMetaball *Clone (void);

   void ShapeSet (PCPoint pCenter, PCPoint pLLT, PCPoint pSize, PCPoint pPower, fp fLOD,
      fp fStrength);
   void ShapeGet (PCPoint pCenter, PCPoint pLLT, PCPoint pSize, PCPoint pPower, fp *pfLOD,
      fp *pfStrength);
   void BoundingBoxGet (short *paiMin, short *paiMax);
   BOOL BoundingBoxGet (PCPoint pMin, PCPoint pMax);

   void ShapeClear (void);
   BOOL PolyAdd (DWORD dwNum, __int64 *paiID, PCPoint papLoc, BOOL fFlip);

   void CalcPolygons (DWORD dwNumMeta, PCMetaball *papMeta, DWORD dwThis,
      BOOL fSeenFromInside);
   void CalcNoise (DWORD dwRenderShard, DWORD dwNoisePoints, PCNoise3D paNoise, PCPoint pNoiseStrength, PCPoint pNoiseDetail,
      PCMetaSurface pms, BOOL fSeenFromInside);
   void CalcPolygonNormals (void);
   void CalcVertexNormals (DWORD dwNumMeta, PCMetaball *papMeta);
   void CalcPolyMinMax (void);
   void CalcObjects (DWORD dwRenderShard, PCMetaSurface pms, DWORD dwSurface, BOOL fSeenFromInside,
      DWORD dwMetaball, PCObjectCave pCave);
   void CalcObjectsSort (void);
   void ClearObjects (void);
   void ClearObjectsFromMetaballs (DWORD *padw, DWORD dwNum);
   void Render (POBJECTRENDER pr, PCRenderSurface prs, BOOL fBackface,
      BOOL fBoxes, PCMetaSurface pms,
      PCObjectCave pCave, fp fDetail);
   BOOL IsValidGet (void);

   void SphereToObject (PCPoint pLoc);
   void ObjectToSphere (PCPoint pLoc);
   fp Distance (PCPoint pLoc);

   BOOL DialogShow (PCEscWindow pWindow, PCObjectCave pCave);

   DWORD          m_dwMetaSurface;     // metasurface index to use for rendering
   BOOL           m_fInvisible;        // determines if the metaball is invisible

private:
   BOOL IntersectStalagtite (PCPoint pBottom, DWORD dwExcludePoly, PCPoint pInter);
   void FindIntersectingMetaballs (DWORD dwNumMeta, PCMetaball *papMeta,
                                           PCListFixed plPCMetaball, PCListFixed plMetaRange);
   BOOL FindPolyWithPoint (DWORD dwIndex, PCPoint pPoint, __int64 *piID, PCListFixed plPMETAPOLY);
   DWORD PointAdd (__int64 iID, PCPoint pLoc, BOOL *pfAdded);
   fp Score (PCPoint pLoc);
   void FillInScore (DWORD dwNumMeta, PCMetaball *papMeta, short *paiRange,
                             int iZ, fp *pafScore, DWORD *padwBestMatch);
   BOOL MarchingCube (DWORD dwNumMeta, PCMetaball *papMeta, DWORD dwThis,
                              BOOL fFlip, DWORD dwPoints,
                              BOOL *pafInside, DWORD *padwBest, __int64 *paID, PCPoint papLoc, fp *pafScore);
   BOOL MarchingCubeDontCompliment (DWORD dwNumMeta, PCMetaball *papMeta, DWORD dwThis,
                              BOOL fFlip, DWORD dwPoints,
                              BOOL *pafInside, DWORD *padwBest, __int64 *paID, PCPoint papLoc, fp *pafScore,
                              BOOL fComplimented);
   BOOL MarchingCube1 (BOOL fFlip,
                              BOOL *pafInside, DWORD *padwBest, __int64 *paID, PCPoint papLoc, fp *pafScore,
                              DWORD dwAdjacent);
   BOOL MarchingCube2 (BOOL fFlip,
                              BOOL *pafInside, DWORD *padwBest, __int64 *paID, PCPoint papLoc, fp *pafScore,
                              DWORD *padwAdjacent);
   BOOL MarchingCube2x (BOOL fFlip,
                              BOOL *pafInside, DWORD *padwBest, __int64 *paID, PCPoint papLoc, fp *pafScore,
                              DWORD dwAdjacent1, DWORD dwAdjacent2);
   BOOL MarchingCube3 (BOOL fFlip,
                              BOOL *pafInside, DWORD *padwBest, __int64 *paID, PCPoint papLoc, fp *pafScore,
                              DWORD *padwAdjacent);
   BOOL MarchingCube4 (BOOL fFlip,
                              BOOL *pafInside, DWORD *padwBest, __int64 *paID, PCPoint papLoc, fp *pafScore,
                              DWORD *padwAdjacent);
   void ObjectAdd (PCMetaSurface pms, DWORD dwMetaball, DWORD dwSurface, DWORD dwCanopy,
                           PCPoint pLoc, PCPoint pNorm);

   // stored in MML
   CPoint         m_pCenter;     // center
   CPoint         m_pLLT;        // latitude, longitude, tilt
   CPoint         m_pSize;       // size in meters
   CPoint         m_pPower;      // power to raise xyz to. 1 = spehere, <1 = squarish
   fp             m_fLOD;        // level of detail, in meters
   fp             m_fStrength;   // strength effect of the metaball

   // calculated by MML stored
   CMatrix        m_mSphereToObject;   // converts from sphere space, -1 to 1, to object coords
   CMatrix        m_mObjectToSphere;   // converts from object space to sphere space
   short          m_aiBoundMin[3];  // bounding box in LOD units
   short          m_aiBoundMax[3];  // bounding box, in LOD units

   // shape calculatons
   BOOL           m_fIsValid;    // set to TRUE if the data in the shape is valid
   CListFixed     m_lPoint;      // list of Cpoints for vertices
   CListFixed     m_lPointID;    // list of IDs (__int64) for vertices
   CListFixed     m_lMETAPOLY;   // list of polyons
   CListFixed     m_lNorm;       // list of CPoint normals for vertices
   CListFixed     m_lText;       // list of TEXTPOINT5 textures for vertices
   CListFixed     m_lVERTEX;     // list of vertex information
   CPoint         m_apBound[2];  // bounding box given points
   CListFixed     m_lCAVEOBJECTINFO;   // objects to draw
};
typedef CMetaball *PCMetaball;


// CMetaSurface - Used to store texture information about a surface

class CMetaSurface {
public:
   ESCNEWDELETE;

   CMetaSurface (void);
   ~CMetaSurface (void);

   PCMMLNode2 MMLTo (void);
   BOOL MMLFrom (PCMMLNode2 pNode);
   BOOL CloneTo (CMetaSurface *pTo);
   CMetaSurface *Clone (void);

   BOOL AreEqual (PCObjectCave pCaveThis, PCMetaSurface pMetaSurfaceComp, PCObjectCave pCaveComp);
   BOOL DialogShow (PCEscWindow pWindow, PCObjectCave pCave, DWORD dwIndex);
   void DirtyUsers (PCObjectCave pCave, DWORD dwIndex);

   DWORD          m_dwRendSurface;     // render surface number to use
   WCHAR          m_szName[64];        // name of the surface
   DWORD          m_dwAxis;            // axis, 0 = x, 1 = y, 2 = z
   DWORD          m_adwMethod[2];      // method for wrap, [0] = x, [1] = y
                                       // 0 = around axis w/repeat, 1 = along axis w/repeat,
                                       // 10 = around axis no repeat, 11 = along axis, no repeat
                                       // 12..14 = EW, NS, UD straight
   DWORD          m_adwRepeat[2];      // number of times to repeat (if have set in m_adwMethod)
   PCCaveCanopy   m_apCaveCanopy[NUMCAVECANOPY];  // array of cave canopies

private:
};
typedef CMetaSurface *PCMetaSurface;


// CIS - passed to dialog
typedef struct {
   PCObjectCave         pv;         // this
   PCMetaball           pmb;        // metaball clicked on
   PCMetaSurface        pms;        // metasurface
   DWORD                dwIndex;    // index used for metasurface
} CIS, *PCIS;





/*************************************************************************
CCaveCanopy::Constructor and destructor */
CCaveCanopy::CCaveCanopy()
{
   m_fDirty = TRUE;
   m_lCANOPYTREE.Init (sizeof(CANOPYTREE));
   m_pRotationVar.Zero();
   m_fRotateDown = FALSE;
   m_pRotationVar.p[2] = 1;   // any angle
   m_pRotationVar.p[0] = m_pRotationVar.p[1] = .05;
   m_pScaleVar.Zero4();
   m_pScaleVar.p[3] = .3;
   m_fDistance = 1;
   m_dwCount = 0;
   m_lPCObjectClone.Init (sizeof(PCObjectClone));
}

CCaveCanopy::~CCaveCanopy()
{
   // free up the clones
   PCObjectClone *ppc = (PCObjectClone*) m_lPCObjectClone.Get(0);
   DWORD i;
   for (i = 0; i < m_lPCObjectClone.Num(); i++)
      if (ppc[i])
         ppc[i]->Release();
   m_lPCObjectClone.Clear();
}

/*************************************************************************
CCaveCanopy::Clone - Creates a copy of the Cave canopy.
*/
CCaveCanopy *CCaveCanopy::Clone (void)
{
   PCCaveCanopy pNew = new CCaveCanopy;
   if (!pNew)
      return NULL;

   pNew->m_lCANOPYTREE.Init (sizeof(CANOPYTREE), m_lCANOPYTREE.Get(0), m_lCANOPYTREE.Num());
   pNew->m_pRotationVar.Copy (&m_pRotationVar);
   pNew->m_pScaleVar.Copy (&m_pScaleVar);
   pNew->m_fDistance = m_fDistance;
   pNew->m_fRotateDown = m_fRotateDown;
   pNew->DirtySet();
   // dont bother with automatically calculated stuff
   return pNew;

}


/*************************************************************************
CCaveCanopy::TextureQuery - From CObjectSocket
*/
BOOL CCaveCanopy::TextureQuery (DWORD dwRenderShard, PCListFixed plText)
{
   CalcTrees(dwRenderShard);

   DWORD i;
   BOOL fRet = FALSE;
   PCObjectClone *ppc = (PCObjectClone*) m_lPCObjectClone.Get(0);
   for (i = 0; i < m_lPCObjectClone.Num(); i++)
      fRet |= ppc[i]->TextureQuery (plText);
   return fRet;
}

/*************************************************************************
CCaveCanopy::ColorQuery - From CObjectSocket
*/
BOOL CCaveCanopy::ColorQuery (DWORD dwRenderShard, PCListFixed plColor)
{
   CalcTrees(dwRenderShard);

   DWORD i;
   BOOL fRet = FALSE;
   PCObjectClone *ppc = (PCObjectClone*) m_lPCObjectClone.Get(0);
   for (i = 0; i < m_lPCObjectClone.Num(); i++)
      fRet |= ppc[i]->ColorQuery (plColor);
   return fRet;
}

/*************************************************************************
CCaveCanopy::ObjectClassQuery - From CObjectSocket
*/
BOOL CCaveCanopy::ObjectClassQuery (DWORD dwRenderShard, PCListFixed plObj)
{
   CalcTrees(dwRenderShard);

   DWORD i;
   BOOL fRet = FALSE;
   PCObjectClone *ppc = (PCObjectClone*) m_lPCObjectClone.Get(0);
   for (i = 0; i < m_lPCObjectClone.Num(); i++)
      fRet |= ppc[i]->ObjectClassQuery (plObj);
   return fRet;
}

/*************************************************************************
CCaveCanopy::DirtySet - Call this after any of the public member variables
of the canopy have been changed so that internal calculations can be redone.
*/
void CCaveCanopy::DirtySet (void)
{
   m_fDirty = TRUE;
}


/*************************************************************************
CCaveCanopy::TreeClone - Returns a pointer to the PCObjectClone for the
tree of given type (from 0..m_lCANOPYTREE.Num()-1).

inputs
   DWORD       dwType - 0 .. m_lCANOPYTREE.Num()-1
returns
   PCObjectClone - Clone object. Do not bother calling Release() because no
   extra addref was called.
*/
PCObjectClone CCaveCanopy::TreeClone (DWORD dwRenderShard, DWORD dwType)
{
   if (!CalcTrees (dwRenderShard))
      return NULL;

   if (dwType >= m_lPCObjectClone.Num())
      return NULL;

   PCObjectClone *pcc;
   pcc = (PCObjectClone*) m_lPCObjectClone.Get(0);
   return pcc[dwType];
}


/*************************************************************************
CCaveCanopy::CalcTrees - If the dirty flag is set this will relcalcualte
the locations of all the trees.

returns
   BOOL - TRUE if success. FALSE if error, such as no trees
*/
BOOL CCaveCanopy::CalcTrees (DWORD dwRenderShard)
{
   if (!m_fDirty)
      return TRUE;

   if (!m_lCANOPYTREE.Num())
      return FALSE;  // no trees so doesnt matter

   // free up old tree clones and load new ones
   CListFixed lOld;
   lOld.Init (sizeof(PCObjectClone), m_lPCObjectClone.Get(0), m_lPCObjectClone.Num());

   m_lPCObjectClone.Clear();
   DWORD i;
   PCANOPYTREE pc;
   PCObjectClone pClone;
   pc = (PCANOPYTREE) m_lCANOPYTREE.Get(0);
   for (i = 0; i < m_lCANOPYTREE.Num(); i++, pc++) {
      pClone = ObjectCloneGet (dwRenderShard, &pc->gCode, &pc->gSub);
      m_lPCObjectClone.Add (&pClone);
   }

   // free up old
   PCObjectClone *pcc;
   pcc = (PCObjectClone*) lOld.Get(0);
   for (i = 0; i < lOld.Num(); i++)
      if (pcc[i])
         pcc[i]->Release();

   // make sure values are within limits
   for (i = 0; i < 4; i++) {
      m_pRotationVar.p[i] = max(m_pRotationVar.p[i], 0);
      m_pRotationVar.p[i] = min(m_pRotationVar.p[i], 1);

      m_pScaleVar.p[i] = max(m_pScaleVar.p[i], 0);
      m_pScaleVar.p[i] = min(m_pScaleVar.p[i], .99);  // since 1 would cause extremes
   }

   // total count of # of trees
   DWORD dwCount;
   dwCount = 0;
   pc = (PCANOPYTREE) m_lCANOPYTREE.Get(0);
   for (i = 0; i < m_lCANOPYTREE.Num(); i++) {
      pc[i].dwWeight = max(pc[i].dwWeight,1);
      dwCount += pc[i].dwWeight;
   }
   m_dwCount = dwCount; // to store away for later

#if 0 // not used
   // seed the random
   srand (m_dwSeed);

   // allocate enough memory
   if (!m_memTREEINFO.Required (m_dwRepeatX * m_dwRepeatY * sizeof(TREEINFO)))
      return FALSE;  // error
   PTREEINFO pti;
   pti = (PTREEINFO) m_memTREEINFO.p;

   // fill it in
   DWORD x,y, dwWant;
   TEXTUREPOINT tpSep;
   CPoint pRot;
   tpSep.h = m_tpSeparationVar.h / 2;
   tpSep.v = m_tpSeparationVar.v / 2;
   pRot.Copy (&m_pRotationVar);
   pRot.Scale (PI);
   for (y = 0; y < m_dwRepeatY; y++) for (x = 0; x < m_dwRepeatX; x++, pti++) {
      dwWant = (DWORD) rand() % dwCount;
      for (i = 0; i < m_lCANOPYTREE.Num(); i++) {
         if (dwWant < pc[i].dwWeight)
            break;
         dwWant -= pc[i].dwWeight;
      }
      pti->dwType = i;

      pti->bScore = (BYTE)rand();

      // what's the default location... note only includes delta, not offset due to x and y
      pti->tpLoc.h = randf(tpSep.h, -tpSep.h) * m_tpSeparation.h;
      pti->tpLoc.v = randf(tpSep.v, -tpSep.v) * m_tpSeparation.v;

      // figure out rotation, etc.
      CMatrix mScale, mRot;
      fp fTotal;
      fTotal = randf(1 - m_pScaleVar.p[3], 1 + m_pScaleVar.p[3]);
      mScale.Scale (
         fTotal * randf(1 - m_pScaleVar.p[0], 1 + m_pScaleVar.p[0]),
         fTotal * randf(1 - m_pScaleVar.p[1], 1 + m_pScaleVar.p[1]),
         fTotal * randf(1 - m_pScaleVar.p[2], 1 + m_pScaleVar.p[2]) );

      mRot.Rotation (
         randf(-pRot.p[0], pRot.p[0]),
         randf(-pRot.p[1], pRot.p[1]),
         randf(-pRot.p[2], pRot.p[2]));


      // combine
      pti->mMatrix.Multiply (&mRot, &mScale);

      // NOTE: Not putting trnanslation into mMatrix since will be added later by caller
      // to EnumTree ()
   }
#endif // 0

   // calculate the maximum size that a tree will be so can use this when calculating
   // bounding boxes
   m_fMaxSize = 0;
   pcc = (PCObjectClone*) m_lPCObjectClone.Get(0);
   for (i = 0; i < m_lCANOPYTREE.Num(); i++) {
      pClone = pcc[i];
      m_fMaxSize = max(m_fMaxSize, pClone->MaxSize());
   }
   // include scale in max size
   m_fMaxSize *= (1.0 + m_pScaleVar.p[3]) *
      (1.0 + max(max(m_pScaleVar.p[0], m_pScaleVar.p[1]), m_pScaleVar.p[2]) );

   m_fDirty = FALSE;
   return TRUE;
}
/*************************************************************************
CCaveCanopy::MaxTreeSize - Returns the largest tree size that will have.
Use this to put limits on the bounding box for sub-sections of the ground.
*/
fp CCaveCanopy::MaxTreeSize (DWORD dwRenderShard)
{
   if (!CalcTrees(dwRenderShard))
      return 0;

   return m_fMaxSize;
}



static PWSTR gpszCanopy = L"Canopy";
static PWSTR gpszSeparation = L"Separation";
static PWSTR gpszSeparationVar = L"SeparationVar";
static PWSTR gpszRotationVar = L"RotationVar";
static PWSTR gpszRotateDown = L"RotateDown";
static PWSTR gpszScaleVar = L"ScaleVar";
static PWSTR gpszRepeatX = L"RepeatX";
static PWSTR gpszRepeatY = L"RepeatY";
// static PWSTR gpszSeed = L"Seed";
static PWSTR gpszTree = L"Tree";
static PWSTR gpszCode = L"Code";
static PWSTR gpszSub = L"Sub";
static PWSTR gpszWeight = L"Weight";
static PWSTR gpszNoShadows = L"NoShadows";
static PWSTR gpszDistance = L"Distance";

/*************************************************************************
CCaveCanopy::MMLTo - Fills a MMLNode describing the canopy
*/
PCMMLNode2 CCaveCanopy::MMLTo (void)
{
   PCMMLNode2 pNode = new CMMLNode2;
   if (!pNode)
      return NULL;
   pNode->NameSet (gpszCanopy);


   MMLValueSet (pNode, gpszRotationVar, &m_pRotationVar);
   MMLValueSet (pNode, gpszScaleVar, &m_pScaleVar);
   MMLValueSet (pNode, gpszDistance, m_fDistance);
   MMLValueSet (pNode, gpszRotateDown, (int)m_fRotateDown);

   DWORD i;
   PCANOPYTREE pt;
   pt = (PCANOPYTREE) m_lCANOPYTREE.Get(0);
   for (i = 0; i < m_lCANOPYTREE.Num(); i++, pt++) {
      PCMMLNode2 pSub = new CMMLNode2;
      if (!pSub)
         continue;
      pSub->NameSet (gpszTree);

      MMLValueSet (pSub, gpszCode, (PBYTE) &pt->gCode, sizeof(pt->gCode));
      MMLValueSet (pSub, gpszSub, (PBYTE) &pt->gSub, sizeof(pt->gSub));
      MMLValueSet (pSub, gpszWeight, (int)pt->dwWeight);

      pNode->ContentAdd (pSub);
   }


   return pNode;
}

/*************************************************************************
CCaveCanopy::MMLFrom - Fills in a canopy based on the mml
*/
BOOL CCaveCanopy::MMLFrom (PCMMLNode2 pNode)
{
   m_lCANOPYTREE.Clear();


   MMLValueGetPoint (pNode, gpszRotationVar, &m_pRotationVar);
   MMLValueGetPoint (pNode, gpszScaleVar, &m_pScaleVar);
   m_fRotateDown = (BOOL) MMLValueGetInt (pNode, gpszRotateDown, FALSE);
   m_fDistance = MMLValueGetDouble (pNode, gpszDistance, 0.1);
   m_fDistance = max(m_fDistance, 0.01);

   DWORD i;
   PCMMLNode2 pSub;
   PWSTR psz;
   CANOPYTREE ct;
   for (i = 0; i < pNode->ContentNum(); i++) {
      pSub = NULL;
      pNode->ContentEnum (i, &psz, &pSub);
      if (!pSub)
         continue;
      psz = pSub->NameGet();
      if (!psz)
         continue;

      if (!_wcsicmp(psz, gpszTree)) {
         MMLValueGetBinary (pSub, gpszCode, (PBYTE) &ct.gCode, sizeof(ct.gCode));
         MMLValueGetBinary (pSub, gpszSub, (PBYTE) &ct.gSub, sizeof(ct.gSub));
         ct.dwWeight = (DWORD)MMLValueGetInt (pSub, gpszWeight, 1);

         m_lCANOPYTREE.Add (&ct);
      }
   }

   DirtySet();
   return TRUE;
}



/****************************************************************************
CaveCanopyPage
*/
BOOL CaveCanopyPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCCaveCanopy pv = (PCCaveCanopy)pPage->m_pUserData;
   DWORD dwRenderShard = pv->m_pCave->m_OSINFO.dwRenderShard;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         PCEscControl pControl;

         WCHAR szTemp[32];
         DWORD i;
         for (i = 0; i < 4; i++) {
            swprintf (szTemp, L"scalevar%d", (int) i);
            pControl = pPage->ControlFind (szTemp);
            if (pControl)
               pControl->AttribSetInt (Pos(), (int)(pv->m_pScaleVar.p[i] * 100));
            if (i >= 3)
               break;   // dont do rotation var=3

            swprintf (szTemp, L"rotationvar%d", (int) i);
            pControl = pPage->ControlFind (szTemp);
            if (pControl)
               pControl->AttribSetInt (Pos(), (int)(pv->m_pRotationVar.p[i] * 100));
         }

         if (pControl = pPage->ControlFind (L"RotateDown"))
            pControl->AttribSetBOOL (Checked(), pv->m_fRotateDown);

         MeasureToString (pPage, L"distance", pv->m_fDistance, TRUE);
      }
      break;

   case ESCN_SCROLL:
   // take out because too slow - case ESCN_SCROLLING:
      {
         PESCNSCROLL p = (PESCNSCROLL) pParam;

         // only do one scroll bar
         if (!p->pControl->m_pszName)
            break;

         // just get all the values
         PCEscControl pControl;

         pv->m_pCave->m_pWorld->ObjectAboutToChange (pv->m_pCave);

         WCHAR szTemp[32];
         DWORD i;
         for (i = 0; i < 4; i++) {
            swprintf (szTemp, L"scalevar%d", (int) i);
            pControl = pPage->ControlFind (szTemp);
            if (pControl)
               pv->m_pScaleVar.p[i] = (fp) pControl->AttribGetInt (Pos()) / 100.0;
            if (i >= 3)
               break;   // dont do rotation var=3

            swprintf (szTemp, L"rotationvar%d", (int) i);
            pControl = pPage->ControlFind (szTemp);
            if (pControl)
               pv->m_pRotationVar.p[i] = (fp) pControl->AttribGetInt (Pos()) / 100.0;
         }

         pv->DirtySet();
         pv->m_pCave->ChangedCanopy (pv);
         pv->m_pCave->m_pWorld->ObjectChanged (pv->m_pCave);
      }
      break;

   case ESCM_LINK:
      {
         PESCMLINK p = (PESCMLINK) pParam;
         if (!p->psz)
            break;

         if ((p->psz[0] == L'c') && (p->psz[1] == L'p') && (p->psz[2] == L':')) {
            pv->m_pCave->m_pWorld->ObjectAboutToChange (pv->m_pCave);

            DWORD dwNum = _wtoi(p->psz + 3);
            PCANOPYTREE pt = (PCANOPYTREE) pv->m_lCANOPYTREE.Get(dwNum);
            pv->DirtySet();
            if (pt->dwWeight >= 2)
               pt->dwWeight--;
            else
               pv->m_lCANOPYTREE.Remove (dwNum);

            pv->m_pCave->ChangedCanopy (pv);
            pv->m_pCave->m_pWorld->ObjectChanged (pv->m_pCave);

            pPage->Exit (RedoSamePage());
            return TRUE;
         }
      }
      break;

   case ESCN_EDITCHANGE:
      {
         PESCNEDITCHANGE p = (PESCNEDITCHANGE) pParam;

         if (!p->pControl || !p->pControl->m_pszName)
            break;

         pv->m_pCave->m_pWorld->ObjectAboutToChange (pv->m_pCave);

         MeasureParseString (pPage, L"distance", &pv->m_fDistance);
         pv->m_fDistance = max(pv->m_fDistance, 0.01);

         // since all out edit controls

         pv->DirtySet();
         pv->m_pCave->ChangedCanopy (pv);
         pv->m_pCave->m_pWorld->ObjectChanged (pv->m_pCave);
      }
      break;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;
         PWSTR psz;
         psz = p->pControl->m_pszName;

         if (!_wcsicmp(psz, L"add")) {
            CANOPYTREE ct;
            memset (&ct,0 ,sizeof(ct));
            ct.dwWeight = 1;
            if (!ObjectCFNewDialog (dwRenderShard, pPage->m_pWindow->m_hWnd, &ct.gCode, &ct.gSub))
               return TRUE;

            pv->m_pCave->m_pWorld->ObjectAboutToChange (pv->m_pCave);

            // see if there's a match already
            PCANOPYTREE pct;
            pct = (PCANOPYTREE) pv->m_lCANOPYTREE.Get(0);
            DWORD i;
            for (i = 0; i < pv->m_lCANOPYTREE.Num(); i++, pct++)
               if (IsEqualGUID(ct.gCode, pct->gCode) && IsEqualGUID(ct.gSub, pct->gSub))
                  break;
            if (i < pv->m_lCANOPYTREE.Num())
               pct->dwWeight++;
            else
               pv->m_lCANOPYTREE.Add (&ct);
            pv->DirtySet();

            pv->m_pCave->ChangedCanopy (pv);
            pv->m_pCave->m_pWorld->ObjectChanged (pv->m_pCave);

            pPage->Exit (RedoSamePage());
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"RotateDown")) {
            pv->m_pCave->m_pWorld->ObjectAboutToChange (pv->m_pCave);


            pv->m_fRotateDown = p->pControl->AttribGetBOOL (Checked());

            pv->DirtySet();
            pv->m_pCave->ChangedCanopy (pv);
            pv->m_pCave->m_pWorld->ObjectChanged (pv->m_pCave);
         }
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Canopy";
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"IFDISTANCE")) {
            p->pszSubString = pv->m_fShowDistance ? L"" : L"<comment>";
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"ENDIFDISTANCE")) {
            p->pszSubString = pv->m_fShowDistance ? L"" : L"</comment>";
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"CANOPY")) {
            MemZero (&gMemTemp);

            DWORD i;
            PCANOPYTREE pct;
            HBITMAP *ph;
            COLORREF *pcr;
            pct = (PCANOPYTREE) pv->m_lCANOPYTREE.Get(0);
            ph = (HBITMAP*) pv->m_lDialogHBITMAP.Get(0);
            pcr = (COLORREF*) pv->m_lDialogCOLORREF.Get(0);

            MemCat (&gMemTemp, L"<table width=100%");
            MemCat (&gMemTemp, L" border=0 innerlines=0>");
            BOOL fNeedTr;
            fNeedTr = FALSE;
            for (i = 0; i < pv->m_lCANOPYTREE.Num(); i++, pct++, ph++, pcr++) {
               // get the name
               WCHAR szMajor[128], szMinor[128], szName[128];
               szName[0] = 0;
               ObjectCFNameFromGUIDs (dwRenderShard, &pct->gCode, &pct->gSub, szMajor, szMinor, szName);

               if (!(i % 2))
                  MemCat (&gMemTemp, L"<tr>");
               MemCat (&gMemTemp, L"<td>");
               MemCat (&gMemTemp, L"<p align=center>");

               // bitmap
               MemCat (&gMemTemp, L"<image hbitmap=");
               WCHAR szTemp[32];
               swprintf (szTemp, L"%lx", (__int64)ph[0]);
               MemCat (&gMemTemp, szTemp);
               if (pcr[0] != -1) {
                  MemCat (&gMemTemp, L" transparent=true transparentdistance=0 transparentcolor=");
                  ColorToAttrib (szTemp, pcr[0]);
                  MemCat (&gMemTemp, szTemp);
               }

               //MemCat (&gMemTemp, L" name=bitmap");
               //MemCat (&gMemTemp, (int) i+j);
               MemCat (&gMemTemp, L" border=0 href=cp:");
               MemCat (&gMemTemp, (int) i);
               MemCat (&gMemTemp, L"/>");

               MemCat (&gMemTemp, L"<br/>");

               MemCat (&gMemTemp, L"<a href=cp:");
               MemCat (&gMemTemp, (int) i);
               MemCat (&gMemTemp, L">");
               MemCatSanitize (&gMemTemp, szName);
               MemCat (&gMemTemp, L"</a>");
               if (pct->dwWeight > 1) {
                  MemCat (&gMemTemp, L" <italic>(x ");
                  MemCat (&gMemTemp, (int) pct->dwWeight);
                  MemCat (&gMemTemp, L")</italic>");
               }

               MemCat (&gMemTemp, L"</p>");
               MemCat (&gMemTemp, L"</td>");
               if (i%2) {
                  MemCat (&gMemTemp, L"</tr>");
                  fNeedTr = FALSE;
               }
               else
                  fNeedTr = TRUE;
            }
            if (fNeedTr) {
               MemCat (&gMemTemp, L"<td/>");
               MemCat (&gMemTemp, L"</tr>");
            }
            MemCat (&gMemTemp, L"</table>");

            p->pszSubString = (PWSTR) gMemTemp.p;
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}



/*************************************************************************
CCaveCanopy::Dialog - Brings up a dialog that allows a user to modify
the canopy.

inputs
   PCEscWindow       pWindow - Window to display it in
   PCObjectCave      pCave - Cave that it's in, notified when object changes
   BOOL              fShowDistance - If TRUE then show distance and user can change
returns
   BOOL - TRUE if the user pressed back, FALSE if closed window
*/
BOOL CCaveCanopy::Dialog (DWORD dwRenderShard, PCEscWindow pWindow, PCObjectCave pCave,
                          BOOL fShowDistance)
{
   PWSTR pszRet;
   m_lDialogHBITMAP.Init (sizeof(HBITMAP));
   m_lDialogCOLORREF.Init (sizeof(COLORREF));
   m_pCave = pCave;
   m_fShowDistance = fShowDistance;

redo:
   // create all the bitmaps
   m_lDialogHBITMAP.Clear();
   m_lDialogCOLORREF.Clear();
   PCANOPYTREE pct;
   pct = (PCANOPYTREE) m_lCANOPYTREE.Get(0);
   DWORD i;
   HBITMAP hBit;
   COLORREF cr;
   m_lDialogHBITMAP.Required (m_lCANOPYTREE.Num());
   m_lDialogCOLORREF.Required (m_lCANOPYTREE.Num());
   for (i = 0; i < m_lCANOPYTREE.Num(); i++, pct++) {
      hBit = Thumbnail (dwRenderShard, &pct->gCode, &pct->gSub, pWindow->m_hWnd, &cr);
      m_lDialogHBITMAP.Add (&hBit);
      m_lDialogCOLORREF.Add (&cr);
   }

   pszRet = pWindow->PageDialog (ghInstance, IDR_MMLCAVECANOPY, CaveCanopyPage, this);

   // free up bitmaps
   HBITMAP *ph;
   ph = (HBITMAP*) m_lDialogHBITMAP.Get(0);
   for (i = 0; i < m_lDialogHBITMAP.Num(); i++, ph++)
      DeleteObject (*ph);

   if (!pszRet)
      return FALSE;
   if (!_wcsicmp(pszRet, RedoSamePage()))
      goto redo;
   return (!_wcsicmp(pszRet, Back()));
}


/*************************************************************************
CCaveCanopy::AreEqual - Returns TRUE if the two canopies are equivalent

inputs
   PCCaveCanopy      pOther - Other to test
reutrns
   BOOL - TRUE if they're the same
*/
BOOL CCaveCanopy::AreEqual (PCCaveCanopy pOther)
{
   if (m_lCANOPYTREE.Num() != pOther->m_lCANOPYTREE.Num())
      return FALSE;
   if (memcmp (m_lCANOPYTREE.Get(0), pOther->m_lCANOPYTREE.Get(0), sizeof(CANOPYTREE) * pOther->m_lCANOPYTREE.Num()))
      return FALSE;

   if (!pOther->m_pRotationVar.AreClose (&m_pRotationVar))
      return FALSE;
   if (!pOther->m_pScaleVar.AreClose (&m_pScaleVar))
      return FALSE;

   if (pOther->m_fDistance != m_fDistance)
      return FALSE;

   if (pOther->m_fRotateDown != m_fRotateDown)
      return FALSE;

   return TRUE;
}



/**********************************************************************************
CMetaSurface::Constructor and destructur
*/
CMetaSurface::CMetaSurface (void)
{
   m_dwRendSurface = 10;
   m_szName[0] = 0;

   m_dwAxis = 0;
   m_adwMethod[0] = 0;
   m_adwMethod[1] = 11;
   m_adwRepeat[0] = m_adwRepeat[1] = 3;

   DWORD i;
   for (i = 0; i < NUMCAVECANOPY; i++)
      m_apCaveCanopy[i] = new CCaveCanopy;

}

CMetaSurface::~CMetaSurface (void)
{
   // clean out the canopy
   DWORD i;
   for (i = 0; i < NUMCAVECANOPY; i++)
      delete m_apCaveCanopy[i];
}


static PWSTR gpszMetaSurface = L"MetaSurface";
static PWSTR gpszRendSurface = L"RendSurface";
static PWSTR gpszName = L"Name";
static PWSTR gpszAxis = L"Axis";

/**********************************************************************************
CMetaSurface::MMLTo - Standard API
*/
PCMMLNode2 CMetaSurface::MMLTo (void)
{
   PCMMLNode2 pNode = new CMMLNode2;
   if (!pNode)
      return NULL;
   pNode->NameSet (gpszMetaSurface);

   MMLValueSet (pNode, gpszRendSurface, (int)m_dwRendSurface);
   if (m_szName[0])
      MMLValueSet (pNode, gpszName, m_szName);

   MMLValueSet (pNode, gpszAxis, (int)m_dwAxis);
   DWORD i;
   WCHAR szTemp[64];
   for (i = 0; i < 2; i++) {
      swprintf (szTemp, L"Method%d", (int)i);
      MMLValueSet (pNode, szTemp, (int)m_adwMethod[i]);

      swprintf (szTemp, L"Repeat%d", (int)i);
      MMLValueSet (pNode, szTemp, (int)m_adwRepeat[i]);
   } // i

   // write out the canopy
   for (i = 0; i < NUMCAVECANOPY; i++) {
      PCMMLNode2 pSub = m_apCaveCanopy[i]->MMLTo ();
      if (pSub)
         pNode->ContentAdd (pSub);
   } // i

   return pNode;
}


/**********************************************************************************
CMetaSurface::MMLFrom - Standard API
*/
BOOL CMetaSurface::MMLFrom (PCMMLNode2 pNode)
{
   m_dwRendSurface = (DWORD) MMLValueGetInt (pNode, gpszRendSurface, 10);

   PWSTR psz = MMLValueGet (pNode, gpszName);
   if (psz && (wcslen(psz)+1 < sizeof(m_szName)/sizeof(WCHAR)))
      wcscpy (m_szName, psz);
   else
      m_szName[0] = 0;

   m_dwAxis = (DWORD) MMLValueGetInt (pNode, gpszAxis, (int)0);
   DWORD i;
   WCHAR szTemp[64];
   for (i = 0; i < 2; i++) {
      swprintf (szTemp, L"Method%d", (int)i);
      m_adwMethod[i] = (DWORD) MMLValueGetInt (pNode, szTemp, (int)0);

      swprintf (szTemp, L"Repeat%d", (int)i);
      m_adwRepeat[i] = (DWORD) MMLValueGetInt (pNode, szTemp, (int)1);
   } // i

   // load in metaballs
   PCMMLNode2 pSub;
   // PWSTR psz;
   DWORD dwCurCanopy = 0;
   for (i = 0; i < pNode->ContentNum(); i++) {
      pSub = NULL;
      pNode->ContentEnum (i, &psz, &pSub);
      if (!pSub)
         continue;
      psz = pSub->NameGet();
      if (!psz)
         continue;

      if (!_wcsicmp (psz, gpszCanopy) && (dwCurCanopy < NUMCAVECANOPY)) {
         m_apCaveCanopy[dwCurCanopy]->MMLFrom (pSub);

         dwCurCanopy++;
      } // if
   } // i
   return TRUE;
}



/**********************************************************************************
CMetaSurface::CloneTo - Standard API
*/
BOOL CMetaSurface::CloneTo (CMetaSurface *pTo)
{
   pTo->m_dwRendSurface = m_dwRendSurface;
   wcscpy (pTo->m_szName, m_szName);

   pTo->m_dwAxis = m_dwAxis;
   memcpy (pTo->m_adwMethod, m_adwMethod, sizeof(m_adwMethod));
   memcpy (pTo->m_adwRepeat, m_adwRepeat, sizeof(m_adwRepeat));

   DWORD i;
   for (i = 0; i < NUMCAVECANOPY; i++) {
      delete pTo->m_apCaveCanopy[i];
      pTo->m_apCaveCanopy[i] = m_apCaveCanopy[i]->Clone();
   } // i

   return TRUE;
}


/**********************************************************************************
CMetaSurface::Clone - Standard API
*/
CMetaSurface *CMetaSurface::Clone (void)
{
   PCMetaSurface pNew = new CMetaSurface;
   if (!pNew)
      return NULL;
   if (!CloneTo (pNew)) {
      delete pNew;
      return NULL;
   }

   return pNew;
}




/**********************************************************************************
CMetaSurface::AreEqual - Tests to see if two metasurfaces (and the textures they
link to) are the same. If they are then returns TRUE.

inputs
   PCObjectCave        pCaveThis - Cave that this surface is part of
   PCMetaSurface     pMetaSurfaceComp - Metasurface to compare with
   PCObjectCave        pCaveComp - Cave that pMetaSurfaceComp is in
returns
   BOOL - TRUE if same, FALSE if different
*/
BOOL CMetaSurface::AreEqual (PCObjectCave pCaveThis, PCMetaSurface pMetaSurfaceComp, PCObjectCave pCaveComp)
{
   // check through values
   if (_wcsicmp(m_szName, pMetaSurfaceComp->m_szName))
      return FALSE;

   if (m_dwAxis != pMetaSurfaceComp->m_dwAxis)
      return FALSE;

   DWORD i;
   for (i = 0; i < 2; i++) {
      if (m_adwMethod[i] != pMetaSurfaceComp->m_adwMethod[i])
         return FALSE;

      if ((m_adwMethod[i] < 10) && (m_adwRepeat[i] != pMetaSurfaceComp->m_adwRepeat[i]))
         return FALSE;
   } // i

   // check to see if canopies are the same
   for (i = 0; i < NUMCAVECANOPY; i++)
      if (!m_apCaveCanopy[i]->AreEqual (pMetaSurfaceComp->m_apCaveCanopy[i]))
         return FALSE;

   // get the other info
   PCObjectSurface pThis = pCaveThis->ObjectSurfaceFind (m_dwRendSurface);
   PCObjectSurface pComp = pCaveComp->ObjectSurfaceFind (pMetaSurfaceComp->m_dwRendSurface);

   return pThis->AreTheSame (pComp);
}


/**********************************************************************************
CMetaSurface::DirtyUsers - Dirty all metaballs that use this surface.

inputs
   PCObjectCave      pCave - Cave object
   DWORD             dwIndex - This one's index into the cave object
returns
   none
*/
void CMetaSurface::DirtyUsers (PCObjectCave pCave, DWORD dwIndex)
{
   PCMetaball *ppmb = (PCMetaball*) pCave->m_lPCMetaball.Get(0);
   DWORD i;
   for (i = 0; i < pCave->m_lPCMetaball.Num(); i++)
      if (ppmb[i]->m_dwMetaSurface == dwIndex)
         ppmb[i]->ShapeClear();
}


/****************************************************************************
CaveMetaSurfacePage
*/
BOOL CaveMetaSurfacePage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCIS pcis = (PCIS)pPage->m_pUserData;
   PCObjectCave pv = pcis->pv;
   PCMetaSurface pms = pcis->pms;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         PCEscControl pControl;

         if (pControl = pPage->ControlFind (L"name"))
            pControl->AttribSet (Text(), pms->m_szName);

         ComboBoxSet (pPage, L"axis", pms->m_dwAxis);
         DWORD i;
         WCHAR szTemp[64];
         for (i = 0; i < 2; i++) {
            swprintf (szTemp, L"method%d", (int)i);
            ComboBoxSet (pPage, szTemp, pms->m_adwMethod[i]);

            swprintf (szTemp, L"repeat%d", (int)i);
            DoubleToControl (pPage, szTemp, pms->m_adwRepeat[i]);
         } // i
      }
      break;

   case ESCN_EDITCHANGE:
      {
         PESCNEDITCHANGE p = (PESCNEDITCHANGE) pParam;
         PWSTR psz = p->pControl->m_pszName;
         if (!psz)
            break;   // doesnt have name

         PCMetaball *ppm = (PCMetaball*)pv->m_lPCMetaball.Get(0);
         pv->m_pWorld->ObjectAboutToChange (pv);

         PCEscControl pControl;
         DWORD dwNeed;
         if (pControl = pPage->ControlFind (L"name"))
            pControl->AttribGet (Text(), pms->m_szName, sizeof(pms->m_szName), &dwNeed);

         DWORD i;
         WCHAR szTemp[64];
         BOOL fChanged = FALSE;
         for (i = 0; i < 2; i++) {
            swprintf (szTemp, L"repeat%d", (int)i);
            DWORD dwOld = pms->m_adwRepeat[i];
            pms->m_adwRepeat[i] = (DWORD) DoubleFromControl (pPage, szTemp);

            if (dwOld != pms->m_adwRepeat[i])
               fChanged = TRUE;
         } // i

         // if fChanged then invalidate all objects that use this
         // BUGFIX - but not if changed name
         if (_wcsicmp(psz, L"name"))
            pms->DirtyUsers (pv, pcis->dwIndex);

         pv->m_pWorld->ObjectChanged (pv);
      }
      break;

   case ESCN_COMBOBOXSELCHANGE:
      {
         PESCNCOMBOBOXSELCHANGE p = (PESCNCOMBOBOXSELCHANGE) pParam;

         if (!p->pControl || !p->pControl->m_pszName)
            break;
         PWSTR psz = p->pControl->m_pszName;
         DWORD dwNum = p->pszName ? _wtoi(p->pszName) : 0;


         PWSTR pszMethod = L"method";
         DWORD dwMethodLen = (DWORD)wcslen(pszMethod);
         if (!wcsncmp (psz, pszMethod, dwMethodLen)) {
            DWORD dwIndex = _wtoi(psz + dwMethodLen);
            if (dwIndex >= 2)
               return TRUE;    //error
            if (pms->m_adwMethod[dwIndex] == dwNum)
               return TRUE;   // no change

            // else, changed texture
            pv->m_pWorld->ObjectAboutToChange (pv);
            pms->m_adwMethod[dwIndex] = dwNum;
            pms->DirtyUsers (pv, pcis->dwIndex);
            pv->m_pWorld->ObjectChanged (pv);

            return TRUE;
         }
         else if (!_wcsicmp(psz, L"axis")) {
            if (pms->m_dwAxis == dwNum)
               return TRUE;

            // else, changed texture
            pv->m_pWorld->ObjectAboutToChange (pv);
            pms->m_dwAxis = dwNum;
            pms->DirtyUsers (pv, pcis->dwIndex);
            pv->m_pWorld->ObjectChanged (pv);

            return TRUE;
         }

      }
      break;


   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Texture settings";
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}



/**********************************************************************************
CMetaSurface::DialogShow - Shows a dialog to edit this particular metaball as well
as the cave settings.

inputs
   PCEscWindow       pWindow - Window
   PCObjectCave      pCave - Cave that this is part of
   DWORD             dwIndex - Surface index, for easier refresh
returns
   BOOL - TRUE if user pressed back, FALSE if pressed cancel
*/
BOOL CMetaSurface::DialogShow (PCEscWindow pWindow, PCObjectCave pCave, DWORD dwIndex)
{
   PWSTR pszRet = NULL;
   CIS cis;
   memset (&cis, 0, sizeof(cis));
   cis.pms = this;
   cis.dwIndex = dwIndex;
   cis.pv = pCave;

redo:
   pszRet = pWindow->PageDialog (ghInstance, IDR_MMLCAVEMETASURFACE, CaveMetaSurfacePage, &cis);
   if (!pszRet)
      return FALSE;
   PWSTR pszCanopy = L"canopy";
   DWORD dwCanopyLen = (DWORD)wcslen(pszCanopy);
   if (!wcsncmp (pszRet, pszCanopy, dwCanopyLen)) {
      DWORD dwNum = _wtoi(pszRet + dwCanopyLen);
      dwNum = min(dwNum, NUMCAVECANOPY-1);
      if (m_apCaveCanopy[dwNum]->Dialog (pCave->m_OSINFO.dwRenderShard, pWindow, pCave, dwNum != CC_STALAGMITE))
         goto redo;
      else
         return FALSE;
   }
   else if (!_wcsicmp(pszRet, RedoSamePage()))
      goto redo;

   return !_wcsicmp(pszRet, Back());
}







/**********************************************************************************
CMetaball::Constructor and destructur
*/
CMetaball::CMetaball (void)
{
   m_dwMetaSurface = 0;      // default surface to use
   m_fInvisible = FALSE;

   CPoint pCenter, pLLT, pScale, pPower;
   pCenter.Zero();
   pLLT.Zero();
   pScale.Zero();
   pPower.Zero();
   pScale.p[0] = pScale.p[1] = pScale.p[2] = 4;
   pPower.p[0] = pPower.p[1] = pPower.p[2] = 1;
   ShapeSet (&pCenter, &pLLT, &pScale, &pPower, 0.25, 1); // default level of detail and strength

   ShapeClear ();
}

CMetaball::~CMetaball (void)
{
   // do nothing for now
}


static PWSTR gpszCenter = L"Center";
static PWSTR gpszLLT = L"LLT";
static PWSTR gpszSize = L"Size";
static PWSTR gpszPower = L"Power";
static PWSTR gpszLOD = L"LOD";
static PWSTR gpszStrength = L"Strength";
static PWSTR gpszMetaball = L"Metaball";
static PWSTR gpszInvisible = L"Inbvisible";

/**********************************************************************************
CMetaball::MMLTo - Standard call
*/
PCMMLNode2 CMetaball::MMLTo (void)
{
   PCMMLNode2 pNode = new CMMLNode2;
   if (!pNode)
      return NULL;
   pNode->NameSet (gpszMetaball);

   MMLValueSet (pNode, gpszCenter, &m_pCenter);
   MMLValueSet (pNode, gpszLLT, &m_pLLT);
   MMLValueSet (pNode, gpszSize, &m_pSize);
   MMLValueSet (pNode, gpszPower, &m_pPower);
   MMLValueSet (pNode, gpszLOD, m_fLOD);
   MMLValueSet (pNode, gpszStrength, m_fStrength);

   MMLValueSet (pNode, gpszMetaSurface, (int) m_dwMetaSurface);
   MMLValueSet (pNode, gpszInvisible, (int) m_fInvisible);

   return pNode;
}


/**********************************************************************************
CMetaball::MMLFrom - Standard call
*/
BOOL CMetaball::MMLFrom (PCMMLNode2 pNode)
{
   CPoint pCenter, pLLT, pSize, pPower;
   fp fLOD = 0.1;
   fp fStrength = 1;
   pCenter.Zero();
   pLLT.Zero();
   pSize.Zero();
   pPower.Zero();
   pSize.p[0] = pSize.p[1] = pSize.p[2] = 1;
   pPower.p[0] = pPower.p[1] = pPower.p[2] = 1;

   MMLValueGetPoint (pNode, gpszCenter, &pCenter);
   MMLValueGetPoint (pNode, gpszLLT, &pLLT);
   MMLValueGetPoint (pNode, gpszSize, &pSize);
   MMLValueGetPoint (pNode, gpszPower, &pPower);
   fLOD = MMLValueGetDouble (pNode, gpszLOD, fLOD);
   fStrength = MMLValueGetDouble (pNode, gpszStrength, fStrength);

   m_dwMetaSurface = (int)MMLValueGetInt (pNode, gpszMetaSurface, 10);
   m_fInvisible = (BOOL)MMLValueGetInt (pNode, gpszInvisible, FALSE);

   ShapeSet (&pCenter, &pLLT, &pSize, &pPower, fLOD, fStrength);
   ShapeClear ();

   return TRUE;
}



/**********************************************************************************
CMetaball::CloneTo - Standard call
*/
BOOL CMetaball::CloneTo (CMetaball *pTo)
{
   pTo->m_pCenter.Copy (&m_pCenter);
   pTo->m_pLLT.Copy (&m_pLLT);
   pTo->m_pSize.Copy (&m_pSize);
   pTo->m_pPower.Copy (&m_pPower);
   pTo->m_fLOD = m_fLOD;
   pTo->m_fStrength = m_fStrength;
   pTo->m_dwMetaSurface = m_dwMetaSurface;
   pTo->m_fInvisible = m_fInvisible;

   memcpy (pTo->m_aiBoundMax, m_aiBoundMax, sizeof(m_aiBoundMax));
   memcpy (pTo->m_aiBoundMin, m_aiBoundMin, sizeof(m_aiBoundMin));
   pTo->m_mSphereToObject.Copy (&m_mSphereToObject);
   pTo->m_mObjectToSphere.Copy (&m_mObjectToSphere);

   pTo->m_fIsValid = m_fIsValid;
   pTo->m_lPoint.Init (sizeof(CPoint), m_lPoint.Get(0), m_lPoint.Num());
   pTo->m_lPointID.Init (sizeof(__int64), m_lPointID.Get(0), m_lPointID.Num());
   pTo->m_lMETAPOLY.Init (sizeof(METAPOLY), m_lMETAPOLY.Get(0), m_lMETAPOLY.Num());
   pTo->m_lNorm.Init (sizeof(CPoint), m_lNorm.Get(0), m_lNorm.Num());
   pTo->m_lText.Init (sizeof(TEXTPOINT5), m_lText.Get(0), m_lText.Num());
   pTo->m_lVERTEX.Init (sizeof(VERTEX), m_lVERTEX.Get(0), m_lVERTEX.Num());
   pTo->m_lPoint.Init (sizeof(CPoint), m_lPoint.Get(0), m_lPoint.Num());
   pTo->m_apBound[0].Copy (&m_apBound[0]);
   pTo->m_apBound[1].Copy (&m_apBound[1]);
   pTo->m_lCAVEOBJECTINFO.Init (sizeof(CAVEOBJECTINFO), m_lCAVEOBJECTINFO.Get(0),m_lCAVEOBJECTINFO.Num());

   return TRUE;
}



/**********************************************************************************
CMetaball::Clone - Standard call
*/
CMetaball *CMetaball::Clone (void)
{
   PCMetaball pNew = new CMetaball;
   if (!pNew)
      return NULL;

   if (!CloneTo (pNew)) {
      delete pNew;
      return NULL;
   }

   return pNew;
}


/**********************************************************************************
CMetaball::IntersectStalagtite - Intersects a line running from pBottom down with
all the polygons in the metaball, and returns the one it intersects with

inputs
   PCPoint        pBottom - Where to start looking from
   DWORD          dwExcludePoly - If not -1, then exclude this polygon index. Used
                  to make sure doesn't intersect with self.
   PCPoint        pInter - Where it intersects. Only valid if returns TRUE.
                     This should initially be filled with an intersection point
                     of the current lowest point, or one with a very low (negative) z
                     value, since the z value is used as an optimizaiton for intersection
                     checks
returns
   BOOL - TRUE if found intersection, and fill in pInter.
*/
BOOL CMetaball::IntersectStalagtite (PCPoint pBottom, DWORD dwExcludePoly, PCPoint pInter)
{
   // get the bounding box for the entire metaball. If doesn't intersect then dont bother
   DWORD i, j;
   for (i = 0; i < 2; i++) {
      if (pBottom->p[i] <= m_apBound[0].p[i])
         return FALSE;
      if (pBottom->p[i] >= m_apBound[1].p[i])
         return FALSE;
   } // i
   if (pBottom->p[2] <= m_apBound[0].p[2])
      return FALSE;  // since going down, will never intersect
   if (pInter->p[2] >= m_apBound[1].p[2])
      return FALSE;  // intersection is already closer

   BOOL fRet = FALSE;

   // go through all polygons
   PMETAPOLY pmp = (PMETAPOLY)m_lMETAPOLY.Get(0);
   PCPoint paPoint = (PCPoint)m_lPoint.Get(0);
   PVERTEX paVert = (PVERTEX)m_lVERTEX.Get(0);
   CPoint pDown, pTempInter;
   pDown.Zero();
   pDown.p[2] = -1;
   for (i = 0; i < m_lMETAPOLY.Num(); i++, pmp++) {
      // dont intersect with self
      if (i == dwExcludePoly)
         continue;

      // not intersect with triangle bounding box
      for (j = 0; j < 2; j++) {
         if (pBottom->p[j] <= pmp->p1.p[j])
            break;
         if (pBottom->p[j] >= pmp->p2.p[j])
            break;
      }
      if (j < 2)
         continue;
      if (pBottom->p[2] <= pmp->p1.p[2])
         continue;    // too low
      if (pInter->p[2] >= pmp->p2.p[2])
         continue;   // already have a better intersection point

      // else, might intersect... try it
      if (pmp->awVert[3] == (WORD)-1) {
         // with polygon
         if (!IntersectLineTriangle (pBottom, &pDown,
            &paPoint[ paVert[pmp->awVert[0]].dwPoint ],
            &paPoint[ paVert[pmp->awVert[1]].dwPoint ],
            &paPoint[ paVert[pmp->awVert[2]].dwPoint ],
            &pTempInter))
            continue;   // miss

      }
      else {
         // with quad
         if (!IntersectLineQuad (pBottom, &pDown,
            &paPoint[ paVert[pmp->awVert[0]].dwPoint ],
            &paPoint[ paVert[pmp->awVert[1]].dwPoint ],
            &paPoint[ paVert[pmp->awVert[2]].dwPoint ],
            &paPoint[ paVert[pmp->awVert[3]].dwPoint ],
            &pTempInter))
            continue;   // miss

      }

      // else intersect, but only take if better
      if ((pTempInter.p[2] < pBottom->p[2]) && (pTempInter.p[2] > pInter->p[2])) {
         pInter->Copy (&pTempInter);
         fRet = TRUE;
      }
   } // i

   return fRet;
}

/**********************************************************************************
CMetaball::Distance - Returns the distance the point is away from the closest
metaball point.

NOTE: This is slow.

inputs
   PCPoint        pLoc - To test
returns
   fp - Distance
*/
fp CMetaball::Distance (PCPoint pLoc)
{
   PCPoint pap = (PCPoint)m_lPoint.Get(0);
   DWORD i;
   fp fBest;
   fp fCur;
   CPoint p;

   // first compare against distance from center... allows user to click
   // on central control point if it's invisible
   p.Subtract (&m_pCenter, pLoc);
   fBest = p.Length();

   for (i = 0; i < m_lPoint.Num(); i++, pap++) {
      p.Subtract (pap, pLoc);
      fCur = p.Length();
      fBest = min(fBest, fCur);
   } // i

   return fBest;
}

/**********************************************************************************
CMetaball::SphereToObject - Converts a point from sphere coords to the object-space
coords
*/
void CMetaball::SphereToObject (PCPoint pLoc)
{
   pLoc->p[3] = 1;
   pLoc->MultiplyLeft (&m_mSphereToObject);
}

/**********************************************************************************
CMetaball::ObjectToSphere - Converts a point from object coords to the sphere-space
coords
*/
void CMetaball::ObjectToSphere (PCPoint pLoc)
{
   pLoc->p[3] = 1;
   pLoc->MultiplyLeft (&m_mObjectToSphere);
}


/**********************************************************************************
CMetaball::BoundingBoxGet - Returns the bounding box of the metaball in terms of
LOD units.

inputs
   short       *paiMin - Filled with 3 shorts for xyz bound (inclusive)
   short       *paiMax - Max. Exclusive
returns
   none
*/
void CMetaball::BoundingBoxGet (short *paiMin, short *paiMax)
{
   memcpy (paiMin, m_aiBoundMin, sizeof(m_aiBoundMin));
   memcpy (paiMax, m_aiBoundMax, sizeof(m_aiBoundMax));
}

/**********************************************************************************
CMetaball::BoundingBoxGet - Returns the bounding box for the shape in terms of 
meters. This is used for rendering and takes into account stuff like stalagtites
and stalgmites stickount out.

inputs
   PCPoint        pMin - filled with minimum
   PCPoint        pMax - filled with maximum
returns
   BOOL - TRUE if success, FALSE if metaball has no points
*/
BOOL CMetaball::BoundingBoxGet (PCPoint pMin, PCPoint pMax)
{
   // if no points then false
   if (!m_lPoint.Num()) {
      // wing it and assume a sphere of half size, since may draw an ellipsoid at
      // an angle

      CPoint p;
      DWORD i;
      for (i = 0; i < 8; i++) {
         p.Zero();
         p.p[0] = (i & 0x01) ? 1 : -1;
         p.p[1] = (i & 0x02) ? 1 : -1;
         p.p[2] = (i & 0x04) ? 1 : -1;
         p.Scale (0.5); // 1/2 size

         p.MultiplyLeft (&m_mSphereToObject);

         if (i) {
            pMin->Min (&p);
            pMax->Max (&p);
         }
         else {
            pMin->Copy (&p);
            pMax->Copy (&p);
         }
      }

      return TRUE;
   }

   pMin->Copy (&m_apBound[0]);
   pMax->Copy (&m_apBound[1]);
   return TRUE;
}



/**********************************************************************************
CMetaball::ShapeClear - Clears all the shape information stored away
*/
void CMetaball::ShapeClear (void)
{
   m_fIsValid = FALSE;
   m_lPoint.Init (sizeof(CPoint));
   m_lPointID.Init (sizeof(__int64));
   m_lMETAPOLY.Init (sizeof(METAPOLY));
   m_lNorm.Init (sizeof(CPoint));
   m_lText.Init (sizeof(TEXTPOINT5));
   m_lVERTEX.Init (sizeof(VERTEX));
   m_lCAVEOBJECTINFO.Init (sizeof(CAVEOBJECTINFO));

   m_apBound[0].Zero();
   m_apBound[1].Zero();
}


/**********************************************************************************
CMetaball::ShapeSet - Changes the shape of the metaball.

inputs
   PCPoint        pCenter - Center of the metaball, in meters
   PCPoint        pLLT - Latitude, longitute, tilt of metball, in radians
   PCPoint        pSize - XYZ radius of the metaball, in meters
   PCPoint        pPower - Power of the metaball. 1.0 = round, < 1.0 = squarish
   fp             fLOD - Level of detail to use
   fp             fStrength - Multiplier for strength of metaball
*/
void CMetaball::ShapeSet (PCPoint pCenter, PCPoint pLLT, PCPoint pSize, PCPoint pPower, fp fLOD,
                          fp fStrength)
{
   m_pCenter.Copy (pCenter);
   m_pLLT.Copy (pLLT);
   m_pSize.Copy (pSize);
   m_pPower.Copy (pPower);
   m_fLOD = fLOD;
   m_fStrength = fStrength;

   // calculate sphere to object space
   CMatrix mScale;
   mScale.Scale (m_pSize.p[0], m_pSize.p[1], m_pSize.p[2]);
   m_mSphereToObject.FromXYZLLT (&m_pCenter, m_pLLT.p[2], m_pLLT.p[0], m_pLLT.p[1]);
   m_mSphereToObject.MultiplyLeft (&mScale); // so scale first

   // and invert
   m_mSphereToObject.Invert4 (&m_mObjectToSphere);

   // matrix for sphere space to LOD spcace
   CMatrix mSphereToLOD;
   mSphereToLOD.Scale (1.0 / m_fLOD, 1.0 / m_fLOD, 1.0 / m_fLOD);
   mSphereToLOD.MultiplyLeft (&m_mSphereToObject);


   // determine bounding box, int amount
   CPoint p;
   DWORD i, j;
   short iVal;
   for (i = 0; i < 8; i++) {
      p.Zero();
      p.p[0] = (i & 0x01) ? 1 : -1;
      p.p[1] = (i & 0x02) ? 1 : -1;
      p.p[2] = (i & 0x04) ? 1 : -1;

      p.MultiplyLeft (&mSphereToLOD);
      for (j = 0; j < 3; j++) {
         // round off
         if (p.p[j] >= -0.5)
            iVal = (short) floor(p.p[j] + 0.5);
         else
            iVal = (short) ceil(p.p[j] - 0.5);

         if (i) {
            m_aiBoundMin[j] = min(m_aiBoundMin[j], iVal);
            m_aiBoundMax[j] = max(m_aiBoundMax[j], iVal);
         }
         else
            m_aiBoundMin[j] = m_aiBoundMax[j] = iVal;
      } // i
   } // i

   // apply buffer
#define LODBUF       2
   for (i = 0; i < 3; i++) {
      m_aiBoundMin[i] -= LODBUF;
      m_aiBoundMax[i] += LODBUF + 1;
   }

   // clear out the shape
   ShapeClear();
}


/**********************************************************************************
CMetaball::ShapeGet - Returns the shape of the metaball.

inputs
   PCPoint        pCenter - Filled with Center of the metaball, in meters
   PCPoint        pLLT - Filled with Latitude, longitute, tilt of metball, in radians
   PCPoint        pSize - Filled with XYZ radius of the metaball, in meters
   PCPoint        pPower - Filled with Power of the metaball. 1.0 = round, < 1.0 = squarish
   fp             *pfLOD - Filled with Level of detail to use
   fp             *pfStrength - Filled in with Multiplier for strength of metaball
*/
void CMetaball::ShapeGet (PCPoint pCenter, PCPoint pLLT, PCPoint pSize, PCPoint pPower, fp *pfLOD,
                          fp *pfStrength)
{
   if (pCenter)
      pCenter->Copy (&m_pCenter);

   if (pLLT)
      pLLT->Copy (&m_pLLT);

   if (pSize)
      pSize->Copy (&m_pSize);

   if (pPower)
      pPower->Copy (&m_pPower);

   if (pfLOD)
      *pfLOD = m_fLOD;

   if (pfStrength)
      *pfStrength = m_fStrength;
}


/**********************************************************************************
CMetaball::PointAdd - Adds a point to the metaball. This checks for duplicates.

inputs
   __int64        iID - ID of the point, combination of xyz LOD address
   PCPoint        pLoc - location. Only one location per iID
   BOOL           *pfAdded - If TRUE then added, set to FALSE if already existsed
returns
   DWORD - Index of the point, -1 if error
*/
DWORD CMetaball::PointAdd (__int64 iID, PCPoint pLoc, BOOL *pfAdded)
{
   DWORD dwNum = m_lPointID.Num();
   PCPoint paPoints = (PCPoint)m_lPoint.Get(0);
   __int64 *paID = (__int64*)m_lPointID.Get(0);
   DWORD i;
   for (i = dwNum-1; i < dwNum; i--) {  // BUGFIX - work backwards

#if 0 // BUGFIX - This isn't accurate. Intead, use actual point location
      if (paID[i] == iID) {
         *pfAdded = FALSE;
         return i; // already found
      }
#endif // 0

      // make sure that don't have identical points with different names
      if (pLoc->AreClose (paPoints+i)) {
         *pfAdded = FALSE;
         return i; // already found
      }
   }

   // if too many points then return error
   if (m_lPoint.Num() > 10000) { // BUGFIX - was 60000
      *pfAdded = FALSE;
      return (DWORD)-1; // error
   }

   // else, add
   m_lPointID.Add (&iID);
   m_lPoint.Add (pLoc);

   // min/max loc
   if (dwNum) {
      m_apBound[0].Min (pLoc);
      m_apBound[1].Max (pLoc);
   }
   else {
      m_apBound[0].Copy (pLoc);
      m_apBound[1].Copy (pLoc);
   }

   // NOTE: Not include stalgtites and mites

   *pfAdded = TRUE;
   return dwNum;  // ID
}


/**********************************************************************************
CMetaball::PolyAdd - Adds a polygon to the metaball. If the polygon already exists
then don't bother adding it.

inputs
   DWORD          dwNum - Number of points.. 3 or 4
   __int64        *paiID - Array of dwNum ID's for the points
   PCPoint        papLoc - Array of dwNum points
   BOOL           fFlip - If TRUE then flip the order of the vertices
returns
   BOOL - TRUE if added or already existed, FALSE if cant add because too large
*/
BOOL CMetaball::PolyAdd (DWORD dwNum, __int64 *paiID, PCPoint papLoc, BOOL fFlip)
{
   BOOL fAddedNewPoint = FALSE;
   DWORD i, j, k;
   WORD awPoint[4];
   BOOL fAdded;
   for (i = 0; i < dwNum; i++) {
      awPoint[i] = (WORD) PointAdd (paiID[i], papLoc + i, &fAdded);
      if (awPoint[i] == (WORD)-1)
         return FALSE;  // too many points
      if (fAdded)
         fAddedNewPoint = TRUE;
   } // i
   if (dwNum < 4)
      awPoint[3] = (WORD) -1; // to triangle

   // if we didn't add any new points, see if the polygon already exists
   if (!fAddedNewPoint) {
      PMETAPOLY pmp = (PMETAPOLY)m_lMETAPOLY.Get(0);
      for (i = 0; i < m_lMETAPOLY.Num(); i++, pmp++) {
         DWORD dwMatch = 0;
         for (j = 0; j < 4; j++) {
            for (k = 0; k < 4; k++)
               if (awPoint[j] == pmp->awVert[k]) {
                  dwMatch++;
                  break;
               }
            if (k >= 4)
               break;   // dont bother with any more points since at least one didnt match
         } // j, check to see if new points in poly
         if (dwMatch == 4)
            return TRUE;  // polygon already exists
      } // i, over all polygons
   } // if added new point

   // if the polygon is degenerate then dont add
   for (i = 0; i < dwNum; i++)
      for (j = i+1; j < dwNum; j++)
         if (awPoint[i] == awPoint[j])
            return TRUE;

   // else, add polygon
   METAPOLY mp;
   memset (&mp, 0, sizeof(mp));
   memcpy (mp.awVert, awPoint, sizeof(mp.awVert));

   if (fFlip) for (i = 0; i < dwNum/2; i++) {
      WORD wTemp = mp.awVert[i];
      mp.awVert[i] = mp.awVert[dwNum-i-1];
      mp.awVert[dwNum-i-1] = wTemp;
   }

   m_lMETAPOLY.Add (&mp);

   return TRUE;
}


/**********************************************************************************
CMetaball::Score - Calculates the metaball score based on pLoc (object space).

inputs
   PCPoint        pLoc - Location in object space
returns
   fp - Score usually ranging from 0.0 to 1.0. 0.5 is the boundary between the metaball being
      visible and not.
*/
fp CMetaball::Score (PCPoint pLoc)
{
   // convert to speher space
   CPoint p;
   p.Copy (pLoc);
   p.p[3] = 1; // just to make sure
   p.MultiplyLeft (&m_mObjectToSphere);

   // if outside -1 to 1 range then always 0
   DWORD i;
   for (i = 0; i < 3; i++) {
      p.p[i] = fabs(p.p[i]);
      if (p.p[i] >= 1.0)
         return 0;
   } // i

   // apply power
   for (i = 0; i < 3; i++) {
      if (m_pPower.p[i] != 1.0)
         p.p[i] = pow(p.p[i], (fp)(1.0 / m_pPower.p[i]));
   } // i

   // distance
   fp fDist = p.Length();
   if (fDist >= 1.0)
      return 0;   // outside sphere

   // BUGFIX: Was fDist = sqrt (fDist);
   fDist = 2 * fDist * fDist * fDist - 3 * fDist * fDist + 1;

   // scaling for fDist so can control strength
   return fDist * m_fStrength;
}



/**********************************************************************************
CMetaball::FillInScore - Internal function that fills in all the polygon mesh
scores for the given Z level.

inputs
   DWORD       dwNumMeta - Number of metaballs
   PCMetaball  *papMeta - Pointer to an array of dwNumMeta metaballs. This one is
                  one of the metaballs. A metaball can be NULL.
   short       *paiRange - Array of dwNumMeta x 2 x 3 ranges for each of the metaballs
                  so the metaball's value will only be calculated if within range.
   int         iZ - Z value to use.
   fp          *pafScore - Array of width x height scores to fill in
   DWORD       *padwBestMatch - Array of width x height best-match indecies (into metaballs)
                  to fill in. Use -1 if no best match.
returns
   none
*/
void CMetaball::FillInScore (DWORD dwNumMeta, PCMetaball *papMeta, short *paiRange,
                             int iZ, fp *pafScore, DWORD *padwBestMatch)
{
   // figure out width and height
   DWORD dwWidth = (DWORD)((int)m_aiBoundMax[0] - (int)m_aiBoundMin[0]);
   DWORD dwHeight = (DWORD)((int)m_aiBoundMax[1] - (int)m_aiBoundMin[1]);

   CMem mem;
   if (!mem.Required (sizeof(PCMetaball) * dwNumMeta))
      return;
   PCMetaball *papMetaTemp = (PCMetaball*)mem.p;

   // loop over all points
   DWORD dwX, dwY, j;
   int iX, iY;
   CPoint pLoc;
   pLoc.Zero();
   pLoc.p[2] = (fp)iZ * m_fLOD;
   for (dwY = 0; dwY < dwHeight; dwY++) {
      iY = (int)m_aiBoundMin[1] + (int)dwY;
      pLoc.p[1] = (fp)iY * m_fLOD;

      // do a quick elimination of metaballs
      memcpy (papMetaTemp, papMeta, sizeof(PCMetaball)*dwNumMeta);
      for (j = 0; j < dwNumMeta; j++) {
         PCMetaball pMeta = papMetaTemp[j];
         short *pai = paiRange + j*6;

         // if no metaball then continue
         if (!pMeta)
            continue;

         // if not within range then continue
         if (iY < pai[0*3 + 1]) {
            // outside metaball
            pMeta = NULL;
            continue;
         }
         if (iY >= pai[1*3 + 1]) {
            // outside metaball
            pMeta = NULL;
            continue;
         }
         if (iZ < pai[0*3 + 2]) {
            // outside metaball
            pMeta = NULL;
            continue;
         }
         if (iZ >= pai[1*3 + 2]) {
            // outside metaball
            pMeta = NULL;
            continue;
         }

      } // j

      for (dwX = 0; dwX < dwWidth; dwX++, pafScore++, padwBestMatch++) {
         iX = (int)m_aiBoundMin[0] + (int)dwX;
         pLoc.p[0] = (fp)iX * m_fLOD;

         fp fScore = 0;
         DWORD dwBest = (DWORD)-1;
         fp fScoreBest = 0;

         // loop thorugh all metaballs
         for (j = 0; j < dwNumMeta; j++) {
            PCMetaball pMeta = papMetaTemp[j];
            short *pai = paiRange + j*6;

            // if no metaball then continue
            if (!pMeta)
               continue;

            // if not within range then continue
            if (iX < pai[0*3 + 0])
               continue;   // outside bounding box
            if (iX >= pai[1*3 + 0])
               continue;   // outside bounding box

            // get the score
            fp fScoreThis = pMeta->Score (&pLoc);
            fScore += fScoreThis;
            if (fScoreThis > fScoreBest) {
               // remember this as best one
               dwBest = j;
               fScoreBest = fScoreThis;
            }
         } // j

         // write this info out
         *pafScore = fScore;
         *padwBestMatch = dwBest;


      } // dwX++
   } // dwY
}



/**********************************************************************************
CMetaball::CalcPolygons - This calculates the polygons of the metaball. It does NOT
calculate the normals or textures, since those must be done in a later pass.

Also: The app should have called ShapeClear() before calling this. All of the metaballs
should have had their locations set.

inputs
   DWORD       dwNumMeta - Number of metaballs
   PCMetaball  *papMeta - Pointer to an array of dwNumMeta metaballs. This one is
                  one of the metaballs.
   DWORD       dwThis - This on'e index into papMeta.
   BOOL        fSeenFromInside - Set to TRUE if the metaball usually seen from the inside
returns
   none
*/
void CMetaball::CalcPolygons (DWORD dwNumMeta, PCMetaball *papMeta, DWORD dwThis,
                              BOOL fSeenFromInside)
{
   // figure out all the metaballs in this range
   DWORD i;
   CListFixed  lPCMetaball, lMetaRange;
   FindIntersectingMetaballs (dwNumMeta, papMeta, &lPCMetaball, &lMetaRange);

   // if this area is too large then just abort here since would consume too much memory
   int aiSize[3];
   __int64 iVolume = 1;
   for (i = 0; i < 3; i++) {
      aiSize[i] = (int)m_aiBoundMax[i] - (int)m_aiBoundMin[i];
      iVolume *= aiSize[i];
   }
   if (iVolume >= 256 * 256 * 256)
      return;  // dont do because too large

   // allocate memory for the area to cover
   DWORD dwArea = aiSize[0] * aiSize[1];
   DWORD dwNeed = dwArea * 2;
   CMem mem;
   if (!mem.Required (dwNeed * (sizeof(fp) * sizeof(DWORD))))
      return;  // not enough memory
   fp *pafScore = (fp*) mem.p;
   DWORD *padwBestMatch = (DWORD*) (pafScore + dwNeed);

   // loop over all z
   int iZ;
   BOOL fToggle = FALSE;
   CPoint apPoints[8];  // points in the cube
   BOOL afInside[8]; // set to TRUE if inside
   __int64 aiID[8];  // IDs for each of the points
   DWORD adwBest[8]; // best metaball for each
   fp afScore[8]; // scores for points
   for (iZ = (int)m_aiBoundMin[2]; iZ+1 < (int)m_aiBoundMax[2]; iZ++, fToggle = !fToggle) {
      fp *pafTop = pafScore + (fToggle ? 0 : dwArea);
      fp *pafBottom = pafScore + (fToggle ? dwArea : 0);
      DWORD *padwTop = padwBestMatch + (fToggle ? 0 : dwArea);
      DWORD *padwBottom = padwBestMatch + (fToggle ? dwArea : 0);

      // if this is the first time then caclulate the top row
      if (iZ == (int)m_aiBoundMin[2])
         FillInScore (dwNumMeta, (PCMetaball*)lPCMetaball.Get(0), (short*)lMetaRange.Get(0),
            iZ, pafTop, padwTop);

      // fill in the bottom
      FillInScore (dwNumMeta, (PCMetaball*)lPCMetaball.Get(0), (short*)lMetaRange.Get(0),
         iZ+1, pafBottom, padwBottom);

      // march
      int iX, iY;
      DWORD dwX, dwY;
      for (iY = (int)m_aiBoundMin[1], dwY=0; iY+1 < (int)m_aiBoundMax[1]; iY++, dwY++) {
         for (iX = (int)m_aiBoundMin[0], dwX=0; iX+1 < (int)m_aiBoundMax[0]; iX++, dwX++) {
            // wrap this up into a cube to be marched

            // figure out if meet the boundary condition
            DWORD dwPoints = 0;
            BOOL fBestThis = FALSE;
            for (i = 0; i < 8; i++) {
               DWORD dwIndex = (DWORD)(dwY + ((i & 0x02) ? 1 : 0)) * aiSize[0] +
                  (DWORD)(dwX + ((i & 0x01) ? 1 : 0));
               afScore[i] = ((i & 0x04) ? pafBottom : pafTop)[dwIndex];
               afInside[i] = (afScore[i] >= METABOUNDARY);
               if (fSeenFromInside)
                  afInside[i] = !afInside[i];
               adwBest[i] = ((i & 0x04) ? padwBottom : padwTop)[dwIndex];

               // if have a point that's inside then note
               if (afInside[i]) {
                  dwPoints++;
                  if (adwBest[i] == dwThis)
                     fBestThis = TRUE;
               }
            } // i

            // if 0 or 8 points then trivial reject for polygon
            if (!dwPoints || (dwPoints == 8))
               continue;

            // if none of the points have this as best then trivial reject
            if (!fBestThis)
               continue;

            // wrap up all the points
            for (i = 0; i < 8; i++) {
               int aiVal[3];
               aiVal[0] = iX + ((i&0x01) ? 1 : 0);
               aiVal[1] = iY + ((i&0x02) ? 1 : 0);
               aiVal[2] = iZ + ((i&0x04) ? 1 : 0);

               apPoints[i].p[0] = (fp)aiVal[0] * m_fLOD;
               apPoints[i].p[1] = (fp)aiVal[1] * m_fLOD;
               apPoints[i].p[2] = (fp)aiVal[2] * m_fLOD;
               apPoints[i].p[3] = 1.0;

               short *pai = (short*) (&aiID[i]);
               pai[0] = (short) aiVal[0];
               pai[1] = (short) aiVal[1];
               pai[2] = (short) aiVal[2];
               pai[3] = 0;
            } // i

            if (!MarchingCube (dwNumMeta, (PCMetaball*)lPCMetaball.Get(0), dwThis,
               FALSE, dwPoints,
               &afInside[0], &adwBest[0], &aiID[0], &apPoints[0], &afScore[0]))
               return;
         } // iX
      } // iY
   } // iZ

   // done
}




/**********************************************************************************
CMetaball::CalcNoise - This calculates the noise for the metaball, based on the
current point location. Call this before normals are calculated.

inputs
   DWORD          dwNoisePoints - X,Y,Z size that the CNoise3D objects were initialized to
   PCNoise3D      paNoise - Pointer to an array of 3 3D noise objects that are used.
                     [0] = x noise, [1] = y noise, [2] = z noise
   PCPoint        pNoiseStrength - 4 noise strengths
   PCPoint        pNoiseDetail - 4 noise details
   PCMetaSurface  pms - Metasurface
   BOOL           fSeenFromInside - Set to TRUE if metaball usually seen from sinide
returns
   none
*/
// WRAPAROUND - Used to store wrap around info
typedef struct {
   fp                atpRadians[2];  // texture point in radians
   fp                atpAltHV[2];    // alternative HV to use if wrap around
   DWORD             dwVertex;   // new vertex to use if need wrap around, -1 if no vertex has been created
} WRAPAROUND, *PWRAPAROUND;

void CMetaball::CalcNoise (DWORD dwRenderShard, DWORD dwNoisePoints, PCNoise3D paNoise, PCPoint pNoiseStrength, PCPoint pNoiseDetail,
                           PCMetaSurface pms, BOOL fSeenFromInside)
{
   PCPoint paPoints = (PCPoint)m_lPoint.Get(0);
   DWORD dwNum = m_lPoint.Num();

   // will be filling in vertecies
   m_lVERTEX.Init (sizeof(VERTEX));

   // do the textures
   TEXTPOINT5 tp;
   DWORD dwNoise, i, j;
   memset (&tp, 0, sizeof(tp));
   m_lText.Init (sizeof(TEXTPOINT5));

   fp afScale[2];
   for (j = 0; j < 2; j++) {
      pms->m_adwRepeat[j] = max (1, pms->m_adwRepeat[j]);   // just so dont get divide
      afScale[j] = (pms->m_adwMethod[j] < 10) ? (fp)pms->m_adwRepeat[j] : 1.0;

      // if around axis (0 or 10), then scale by 1.0 / 2*PI
      if ((pms->m_adwMethod[j] == 0) || (pms->m_adwMethod[j] == 10)) {
         afScale[j] /= (2.0 * PI);
         if (fSeenFromInside)
            afScale[j] *= -1;
      }
      else if (pms->m_adwMethod[j] == 11)
         afScale[j] = 1;   // no scaling
      else // will be 1 or 11
         afScale[j] /= 2.0;   // since points go from -1 to 1 i nspace
   }

   CPoint p;
   DWORD dwAxis = pms->m_dwAxis % 3;
   DWORD dwAxis2 = (dwAxis + 1) % 3;
   DWORD dwAxis3 = (dwAxis + 2) % 3;
   VERTEX v;
   WRAPAROUND wa;
   CListFixed lWRAPAROUND;
   lWRAPAROUND.Init (sizeof(wa));
   memset (&v, 0, sizeof(v));
   memset (&wa, 0, sizeof(wa));
   wa.dwVertex = -1; // since not set up yet
   m_lText.Required (dwNum);
   m_lVERTEX.Required (dwNum);
   lWRAPAROUND.Required (dwNum);
   for (i = 0; i < dwNum; i++) {
      p.Copy (&paPoints[i]);
      p.p[3] = 1; // just to be sure
      p.MultiplyLeft (&m_mObjectToSphere);

      for (j = 0; j < 2; j++) switch (pms->m_adwMethod[j]) {
         case 0:  // rotate around axis, repeat
         case 10:  // rotate around axis, no repeat
            wa.atpRadians[j] = atan2(p.p[dwAxis2], p.p[dwAxis3]);
            tp.hv[j] = wa.atpRadians[j] * afScale[j];
            wa.atpAltHV[j] = (wa.atpRadians[j] + 2.0 * PI) * afScale[j];
            break;

         case 1: // along axis, repeat
         case 11: // along axis, no repeat
            tp.hv[j] = p.p[dwAxis] * afScale[j];
            wa.atpAltHV[j] = tp.hv[j];
            break;
         // default: - will calculate later
      } // j

      m_lText.Add (&tp);

      lWRAPAROUND.Add (&wa);

      v.dwNormal = v.dwPoint = v.dwTexture = i;
      m_lVERTEX.Add (&v);
   } // i

   CPoint pAffect;
   for (dwNoise = 0; dwNoise < 4; dwNoise++) {
      if (pNoiseStrength->p[dwNoise] <= 0)
         continue;   // no noise

      // figure out the scaling
      pNoiseDetail->p[dwNoise] = max(pNoiseDetail->p[dwNoise], CLOSE);
      fp fScale = 1.0 / (fp)dwNoisePoints / pNoiseDetail->p[dwNoise];
      for (j = 0; j < 4; j++)
         pAffect.p[j] = pNoiseStrength->p[j] * pNoiseDetail->p[j];

      for (i = 0; i < dwNum; i++) {
         p.Copy (&paPoints[i]);
         p.Scale (fScale);

         for (j = 0; j < 3; j++)
            paPoints[i].p[j] += paNoise[j].Value (p.p[0], p.p[1], p.p[2]) * pAffect.p[dwNoise];
      } // i
   } // dwNoise

   // finish with the texture bits done after the noise
   PTEXTPOINT5 ptp = (PTEXTPOINT5) m_lText.Get(0);
   PWRAPAROUND pwa = (PWRAPAROUND) lWRAPAROUND.Get(0);
   for (i = 0; i < m_lText.Num(); i++) {
      for (j = 0; j < 2; j++) {
         if ((pms->m_adwMethod[j] >= 12) /* not needed && (pms->m_adwMethod[j] <= 14)*/) {
            ptp[i].hv[j] = paPoints[i].p[pms->m_adwMethod[j] - 12];
            pwa[i].atpAltHV[j] = ptp[i].hv[j];
         }
      } // j

      // for 3D points
      ptp[i].xyz[0] = paPoints[i].p[0];
      ptp[i].xyz[1] = paPoints[i].p[1];
      ptp[i].xyz[2] = paPoints[i].p[2];
   } // i

   // loop through all the polygons and see if any include wrap-around triangles
   if ((pms->m_adwMethod[0] == 0) || (pms->m_adwMethod[0] == 10) || (pms->m_adwMethod[1] == 0) || (pms->m_adwMethod[1] == 10)) {
      PMETAPOLY pmp = (PMETAPOLY)m_lMETAPOLY.Get(0);
      for (i = 0; i < m_lMETAPOLY.Num(); i++, pmp++) {
         DWORD dwPositive = 0;
         DWORD dwNegative = 0;
         for (j = 0; j < 4; j++) {
            WORD wVert = pmp->awVert[j];
            if (wVert == (WORD)-1)
               continue;   // not a point

            if ((pwa[wVert].atpRadians[0] > PI/2) || (pwa[wVert].atpRadians[0] > PI/2))
               dwPositive++;
            else if ((pwa[wVert].atpRadians[0] < -PI/2) || (pwa[wVert].atpRadians[0] < -PI/2))
               dwNegative++;
         } // j

         if (!dwPositive || !dwNegative)
            continue;   // no problems with this since one of postive or negative are 0

         // else, need to wrap around on the nagative side
         for (j = 0; j < 4; j++) {
            WORD wVert = pmp->awVert[j];
            if (wVert == (WORD)-1)
               continue;   // not a point
            if (! ((pwa[wVert].atpRadians[0] < -PI/2) || (pwa[wVert].atpRadians[0] < -PI/2)) )
               continue; // only looking for negative

            // if no alternative vertex then create one
            if (pwa[wVert].dwVertex == (DWORD)-1) {
               // new texture
               tp.hv[0] = pwa[wVert].atpAltHV[0];
               tp.hv[1] = pwa[wVert].atpAltHV[1];
               ptp = (PTEXTPOINT5) m_lText.Get(wVert);
               memcpy (tp.xyz, ptp->xyz, sizeof(tp.xyz));
               m_lText.Add (&tp);

               // new vertex
               v.dwNormal = pmp->awVert[j];
               v.dwPoint = pmp->awVert[j];
               v.dwTexture = m_lText.Num()-1;
               m_lVERTEX.Add (&v);

               pwa[wVert].dwVertex = m_lVERTEX.Num()-1;
            }

            // adjust this
            pmp->awVert[j] = (WORD)pwa[wVert].dwVertex;
         } // j
      } // i
   } // if have wraparound


   // re-calculate the bounding box
   if (dwNum) {
      m_apBound[0].Copy (&paPoints[0]);
      m_apBound[1].Copy (&paPoints[0]);
   }
   for (i = 1; i < dwNum; i++) {
      m_apBound[0].Min (&paPoints[i]);
      m_apBound[1].Max (&paPoints[i]);
   }

   // extra for contents
   fp fMax = 0, f;
   for (i = 0; i < NUMCAVECANOPY; i++) {
      f = pms->m_apCaveCanopy[i]->MaxTreeSize (dwRenderShard);
      fMax = max(f, fMax);
   }
   if (fMax) for (i = 0; i < 3; i++) {
      m_apBound[0].p[i] -= fMax;
      m_apBound[1].p[i] += fMax;
   } // i
}


/**********************************************************************************
CMetaball::ClearObjects - Just clears the objects associated with the metaball.
*/
void CMetaball::ClearObjects (void)
{
   m_lCAVEOBJECTINFO.Init (sizeof(CAVEOBJECTINFO));
}


/**********************************************************************************
CMetaball::CalcObjects - Calculates where all the objects go.

NOTE: Should have called ClearObjects() before since some metaball install objects
onto other metaballs.

inputs
   PCMetaSurface        pms - Surface
   DWORD                dwSurface - Meta surface inex used for pms
   BOOL                 fSeenFromInside - Set to true if polygon seen from inside
   DWORD                dwMetaball - Specific metaball index for this metaball
   PCObjectCave         pCave - Cave object
returns
   none
*/
void CMetaball::CalcObjects (DWORD dwRenderShard, PCMetaSurface pms, DWORD dwSurface, BOOL fSeenFromInside,
                             DWORD dwMetaball, PCObjectCave pCave)
{
   // if there aren't any items for this surface then dont bother
   DWORD i, j;
   BOOL fFound = FALSE;
   // CPoint apRot[NUMCAVECANOPY];
   for (i = 0; i < NUMCAVECANOPY; i++) {
      pms->m_apCaveCanopy[i]->MaxTreeSize(dwRenderShard); // to make sure calculated
      if (!pms->m_apCaveCanopy[i]->m_dwCount)
         continue;
      else
         fFound = TRUE;

      pms->m_apCaveCanopy[i]->m_fDistance = max(pms->m_apCaveCanopy[i]->m_fDistance, 0.01);
      //apRot[i].Copy (&pms->m_apCaveCanopy[i]->m_pRotationVar);
      //apRot[i].Scale (PI);
   }
   if (!fFound)
      return;  // no info

   // loop through all the polygons
   PMETAPOLY pmp = (PMETAPOLY)m_lMETAPOLY.Get(0);
   PCPoint paPoint = (PCPoint)m_lPoint.Get(0);
   PCPoint paNorm = (PCPoint)m_lNorm.Get(0);
   PVERTEX paVert = (PVERTEX)m_lVERTEX.Get(0);
   DWORD dwNumVert = m_lVERTEX.Num();
   PCPoint pap[4], pan[4];
   CPoint pA, pB, pN;

   for (i = 0; i < m_lMETAPOLY.Num(); i++, pmp++) {
      for (j = 0; j < 4; j++) {
         if (pmp->awVert[j] >= (WORD)dwNumVert) {
            pap[j] = pan[j] = NULL;
            continue;
         }

         // else, get the point and normal
         pap[j] = &paPoint[paVert[pmp->awVert[j]].dwPoint];
         pan[j] = &paNorm[paVert[pmp->awVert[j]].dwNormal];
      } // j

      // calculate the area
      fp fArea;
      pA.Subtract (pap[1], pap[0]);
      pB.Subtract (pap[2], pap[0]);
      pN.CrossProd (&pA, &pB);
      fArea = pN.Length() / 2.0;
      if (pap[3]) {
         pA.Subtract (pap[3], pap[0]);
         // already set pB.Subtract (pap[2], pap[0]);
         pN.CrossProd (&pA, &pB);
         fArea += pN.Length() / 2.0;
      }
      if (!fArea)
         continue;

      // figure out the average normal for the polygon
      pN.Add (pan[0], pan[1]);
      pN.Add (pan[2]);
      if (pan[3])
         pN.Add (pan[3]);
      pN.Normalize();

      // determine if on ground, wall, or floor
      fp fScoreGround = (pN.p[2] > 0.5) ? ((pN.p[2] - 0.5) * 4) : 0;
      fp fScoreCeiling = (pN.p[2] < -0.5) ? ((-0.5 - pN.p[2])*4 ) : 0;
         // BUGFIX - multiply by 4 instead of 2, so only a small transition area
      DWORD dwCanopy;
      if (fScoreGround)
         dwCanopy = (randf(0, 1) < fScoreGround) ? CC_GROUND : CC_WALL;
      else if (fScoreCeiling) {
         if (randf(0, 1) < fScoreCeiling) {
            DWORD dwSum = pms->m_apCaveCanopy[CC_CEILING]->m_dwCount + pms->m_apCaveCanopy[CC_STALAGTITE]->m_dwCount;
            if (dwSum)
               dwCanopy = ((rand()%dwSum) < pms->m_apCaveCanopy[CC_CEILING]->m_dwCount) ?
                  CC_CEILING : CC_STALAGTITE;
            else
               dwCanopy = CC_CEILING;  // wont matter since neither is possible

            // BUGFIX - If have stalagtite, then lower probability if on flat ceiling
            fp fScoreStalagtite = (pN.p[2] < -1.0) ? 0 : ((pN.p[2] + 1.0)*4 );
            if ((dwCanopy == CC_STALAGTITE) && (randf(0, 1) >= fScoreStalagtite))
               dwCanopy = CC_CEILING;
         } else
            dwCanopy = CC_WALL;
      }
      else
         dwCanopy = CC_WALL;

      // what's the probability?
      PCCaveCanopy pcc = pms->m_apCaveCanopy[dwCanopy];
      if (randf(0, 1) >= fArea / (pcc->m_fDistance * pcc->m_fDistance))
         continue;   // not enough chance

      // translation to the center
      pA.Add (pap[0], pap[1]);
      pA.Add (pap[2]);
      if (pap[3]) {
         pA.Add (pap[3]);
         pA.Scale (1.0 / 4.0);
      }
      else
         pA.Scale (1.0 / 3.0);

      ObjectAdd (pms, dwMetaball, dwSurface, dwCanopy, &pA, &pN);

      // if apply stalagtite then see about stalagtite below
      if (dwCanopy == CC_STALAGTITE) {
         // find an intersection
         CPoint pInter;
         pInter.Copy (&pA);
         pInter.p[2] -= 1000000; // long distance
         DWORD dwInterMeta = (DWORD)-1;

         PCMetaball *ppmb = (PCMetaball*) pCave->m_lPCMetaball.Get(0);
         for (j = 0; j < pCave->m_lPCMetaball.Num(); j++)
            if (ppmb[j]->IntersectStalagtite (&pA, (ppmb[j] == this) ? i : (DWORD)-1, &pInter))
               dwInterMeta = j;

         // if found a metaball that intersected then add stalagtite
         if (dwInterMeta != (DWORD)-1)
            ppmb[dwInterMeta]->ObjectAdd (pms, dwMetaball, dwSurface, CC_STALAGMITE, &pInter, &pN);


      }

   } // i, all polygons
}


/**********************************************************************************
CMetaball::ClearObjectsFromMetaballs - This clears all objects that were created
by a specific metaball... used to ensure that stalagtites aren't repeated infinitely

inputs
   DWORD       *padw - Array of metaball iDs
   DWORD       dwNum - Number in array
returns
   none
*/
void CMetaball::ClearObjectsFromMetaballs (DWORD *padw, DWORD dwNum)
{
   DWORD i, j;
   PCAVEOBJECTINFO pcoi = (PCAVEOBJECTINFO) m_lCAVEOBJECTINFO.Get(0);
   for (i = m_lCAVEOBJECTINFO.Num()-1; i < m_lCAVEOBJECTINFO.Num(); i--) {
      for (j = 0; j < dwNum; j++)
         if (pcoi[i].dwMetaball == padw[j])
            break;
      if (j >= dwNum)
         continue;   // not found

      // else, remove this
      m_lCAVEOBJECTINFO.Remove (i);
      pcoi = (PCAVEOBJECTINFO) m_lCAVEOBJECTINFO.Get(0);
   } // i
}

/**********************************************************************************
CMetaball::ObjectAdd - Add an object to the list

inputs
   PCMetaSurface  pms - Metasurface
   DWORD          dwMetaball - Metaball that created this
   DWORD          dwSurface - Surface type
   DWORD          dwCanopy - CC_XXX type
   PCPoint        pLoc - Location where the object should be plced
   PCPoint        pNorm - Normal of the surface to place it on. Only used for CC_WALL
returns
   none
*/
void CMetaball::ObjectAdd (PCMetaSurface pms, DWORD dwMetaball, DWORD dwSurface, DWORD dwCanopy,
                           PCPoint pLoc, PCPoint pNorm)
{
   // if no entries in the canopy then dont bother
   PCCaveCanopy pcc = pms->m_apCaveCanopy[dwCanopy];
   if (!pcc->m_dwCount)
      return;

   CAVEOBJECTINFO coi;
   CMatrix mScale, mRot, mTrans;
   memset (&coi, 0, sizeof(coi));

   DWORD j;
   DWORD dwWant =(DWORD) rand() % pcc->m_dwCount;
   PCANOPYTREE pc;
   pc = (PCANOPYTREE) pcc->m_lCANOPYTREE.Get(0);
   for (j = 0; j < pcc->m_lCANOPYTREE.Num(); j++) {
      if (dwWant < pc[j].dwWeight)
         break;
      dwWant -= pc[j].dwWeight;
   }
   coi.dwType = j;
   coi.dwCanopy = dwCanopy;

   // figure out rotation, etc.
   fp fTotal;
   fTotal = randf(1 - pcc->m_pScaleVar.p[3], 1 + pcc->m_pScaleVar.p[3]);
   mScale.Scale (
      fTotal * randf(1 - pcc->m_pScaleVar.p[0], 1 + pcc->m_pScaleVar.p[0]),
      fTotal * randf(1 - pcc->m_pScaleVar.p[1], 1 + pcc->m_pScaleVar.p[1]),
      fTotal * randf(1 - pcc->m_pScaleVar.p[2], 1 + pcc->m_pScaleVar.p[2]) );

   PCPoint pRot = &pcc->m_pRotationVar;

   mRot.Rotation (
      randf(-pRot->p[0], pRot->p[0]) * PI,
      randf(-pRot->p[1], pRot->p[1]) * PI,
      randf(-pRot->p[2], pRot->p[2]) * PI);

   // if this is the wall then extra rotation so that the forward-facing Y
   // is always inside
   CPoint pA, pB, pN;
   if (dwCanopy == CC_WALL) {
      pN.Copy (pNorm);
      pN.p[2] = 0;
      pN.Normalize();
      pN.Scale (-1);
      pA.Zero();
      pA.p[2] = 1;
      pB.CrossProd (&pN, &pA);
      pB.Normalize();

      mTrans.RotationFromVectors (&pB, &pN, &pA);
      mRot.MultiplyLeft (&mTrans);
   }

   if (pcc->m_fRotateDown) {
      // rotate down
      mTrans.RotationX (PI);
      mRot.MultiplyLeft (&mTrans);
   }

   mTrans.Translation (pLoc->p[0], pLoc->p[1], pLoc->p[2]);

   // combine
   coi.mMatrix.Multiply (&mRot, &mScale);
   coi.mMatrix.MultiplyRight (&mTrans);

   coi.dwMetaSurface = dwSurface;
   coi.dwMetaball = dwMetaball;

   m_lCAVEOBJECTINFO.Add (&coi);
   
}



/**********************************************************************************
CMetaball::CalcObjectSort - Call this after all the objects have been added.
This sorts the metaball objects so that they're in order and make for a faster render
*/

static int _cdecl CAVEOBJECTINFOCompare (const void *elem1, const void *elem2)
{
   CAVEOBJECTINFO *pdw1, *pdw2;
   pdw1 = (CAVEOBJECTINFO*) elem1;
   pdw2 = (CAVEOBJECTINFO*) elem2;

   if (pdw1->dwMetaSurface != pdw2->dwMetaSurface)
      return (int)pdw1->dwMetaSurface - (int)pdw2->dwMetaSurface;
   if (pdw1->dwCanopy != pdw2->dwCanopy)
      return (int)pdw1->dwCanopy - (int)pdw2->dwCanopy;
   return (int)pdw1->dwType - (int)pdw2->dwType;
}

void CMetaball::CalcObjectsSort (void)
{
   qsort (m_lCAVEOBJECTINFO.Get(0), m_lCAVEOBJECTINFO.Num(), sizeof(CAVEOBJECTINFO), CAVEOBJECTINFOCompare);
}


/**********************************************************************************
CMetaball::MarchingCubeDontCompliment - Has the metaball do one cube in the marching cubes algorithm.

NOTE: This WONT compliment the cube. If the solution can't be found without a compliment
returns FALSE;

inputs
   DWORD       dwNumMeta - Number of metaballs
   PCMetaball  *papMeta - Pointer to an array of dwNumMeta metaballs. This one is
                  one of the metaballs.
   DWORD       dwThis - This on'e index into papMeta.
   BOOL        fFlip - If TRUE then flip the vertices of the triangle when add
   DWORD       dwPoints - Number of points in this cube
   BOOL        *pafInside - Array of 8 bools that checked if point inside metaball. Bit 0 = x, 1 = y, 2 = z
   DWORD       *padwBest - Array of 8 Index to the metaball with the most influence, -1 if none
   __int64     *paID - Array of 8 IDs for the points
   PCPoint     papLoc - Array of 8 point locations
   fp          *pafScore - Array of 8 scores for the locations
   BOOL        fComplimented - If TRUE then this has been complimented
returns
   BOOL - TRUE if success, FALSE if not solution without a compliment
*/
BOOL CMetaball::MarchingCubeDontCompliment (DWORD dwNumMeta, PCMetaball *papMeta, DWORD dwThis,
                              BOOL fFlip, DWORD dwPoints,
                              BOOL *pafInside, DWORD *padwBest, __int64 *paID, PCPoint papLoc, fp *pafScore,
                              BOOL fComplimented)
{
   // trivial reject
   if (!dwPoints || (dwPoints == 8))
      return TRUE;


   DWORD i, j, k;
   // copy the inside list fo afInside to use as a working list
   BOOL afInside[8];
   memcpy (afInside, pafInside, sizeof(afInside));

   // loop through all the points in afInside
   DWORD adwAdjacent[8][8];   // adjacent points
   DWORD adwNumAdjacent[8];
   DWORD dwGroup = 0;   // current group
   for (i = 0; i < 8; i++) {
      if (!afInside[i])
         continue;   // not a valid point
      adwAdjacent[dwGroup][0] = i;
      adwNumAdjacent[dwGroup] = 1;
      afInside[i] = FALSE; // so no longer consider

      for (k = 0; k < adwNumAdjacent[dwGroup]; k++) {
         // find any adjacent points
         for (j = 0; j < 8; j++) {
            // this checks makes sure that looking in opposite corners only for adjacency
            DWORD dwXOr = (adwAdjacent[dwGroup][k] ^ j);
            if (!dwXOr || (dwXOr & (dwXOr-1)))  // make sure only one bit
               continue;

            // if not longer a point that's in play then ignore
            if (!afInside[j])
               continue;

            // else, in play and adjacent, so use
            adwAdjacent[dwGroup][adwNumAdjacent[dwGroup]++] = j;
            afInside[j] = FALSE; // since no longer in play
         } // j
      } // k

      // if this would create too many adjacent points then fail...
      if (adwNumAdjacent[dwGroup] > 4)
         return FALSE;

      dwGroup++;
   } // i


   // special case to prevent holes... probably one of many
   // if this is a compliment, and there are two groups, each of one point, and they're
   // on the fame face, then special call
   // I think the correct solution is to generate lots of hypothesis and pick the
   // one with all sides connecting to other polygons
   if (fComplimented && (dwGroup == 2) && (adwNumAdjacent[0] == 1) && (adwNumAdjacent[1] == 1)) {
      DWORD dwXOr = adwAdjacent[0][0] ^ adwAdjacent[1][0];
      dwXOr = dwXOr ^ 0x07;   // to see if there's one dimension where they don't cross

      if (dwXOr && !(dwXOr & (dwXOr-1)))
         return MarchingCube2x (fFlip, pafInside, padwBest, paID, papLoc, pafScore, adwAdjacent[0][0], adwAdjacent[1][0]);
   } // special case
   
   // draw all these
   for (i = 0; i < dwGroup; i++) {
      // when get here, have all the points adjacent to the point that started
      // with, and have filled in adwAdjacent

      // only draw this polygon if dwThis is in the best list, and it's the lowest
      // member of the best. That way, ensure each polygon only drawn once
      BOOL fFoundThis = FALSE;
      DWORD dwLowest = 0xffffffff;
      for (j = 0; j < 8; j++) {
         // see if this is one of the points
         for (k = 0; k < adwNumAdjacent[i]; k++)
            if (j == adwAdjacent[i][k])
               break;
         BOOL fAdjacent = (k < adwNumAdjacent[i]);
         if (fFlip)
            fAdjacent = !fAdjacent; // flipping meaning
         if (!fAdjacent)
            continue;   // ignore this

         if (padwBest[j] == dwThis)
            fFoundThis = TRUE;
         dwLowest = min(dwLowest, padwBest[j]);
      } // j
      if (!fFoundThis || (dwLowest < dwThis))
         continue;   // the polygon wont appear in this, so dont bother to calculate

      // else, calculate this division
      switch (adwNumAdjacent[i]) {
         case 1:  // single corner
            if (!MarchingCube1 (fFlip, pafInside, padwBest, paID, papLoc, pafScore, adwAdjacent[i][0]))
               return FALSE;
            break;
         case 2:  // edge
            if (!MarchingCube2 (fFlip, pafInside, padwBest, paID, papLoc, pafScore, &adwAdjacent[i][0]))
               return FALSE;
            break;
         case 3:  // 3 points
            if (!MarchingCube3 (fFlip, pafInside, padwBest, paID, papLoc, pafScore, &adwAdjacent[i][0]))
               return FALSE;
            break;
         case 4:  // 4 points
            if (!MarchingCube4 (fFlip, pafInside, padwBest, paID, papLoc, pafScore, &adwAdjacent[i][0]))
               return FALSE;
            break;
      }
   } // i

   return TRUE;
}


/**********************************************************************************
CMetaball::MarchingCube - Has the metaball do one cube in the marching cubes algorithm.

inputs
   DWORD       dwNumMeta - Number of metaballs
   PCMetaball  *papMeta - Pointer to an array of dwNumMeta metaballs. This one is
                  one of the metaballs.
   DWORD       dwThis - This on'e index into papMeta.
   BOOL        fFlip - If TRUE then flip the vertices of the triangle when add
   DWORD       dwPoints - Number of points in this cube
   BOOL        *pafInside - Array of 8 bools that checked if point inside metaball. Bit 0 = x, 1 = y, 2 = z
   DWORD       *padwBest - Array of 8 Index to the metaball with the most influence, -1 if none
   __int64     *paID - Array of 8 IDs for the points
   PCPoint     papLoc - Array of 8 point locations
   fp          *pafScore - Array of 8 scores for the locations
returns
   BOOL - TRUE if success
*/
BOOL CMetaball::MarchingCube (DWORD dwNumMeta, PCMetaball *papMeta, DWORD dwThis,
                              BOOL fFlip, DWORD dwPoints,
                              BOOL *pafInside, DWORD *padwBest, __int64 *paID, PCPoint papLoc, fp *pafScore)
{
   // see if can do this without inverting
   if (MarchingCubeDontCompliment (dwNumMeta, papMeta, dwThis, fFlip, dwPoints,
      pafInside, padwBest, paID, papLoc, pafScore, FALSE))
      return TRUE;

   // if more than 4 points, then "invert" the image and use this, pretending the
   // inside is the outside. Makes algorithm much simpler
   DWORD i;
   BOOL afInside[8];
   if (dwPoints > 4) {
      for (i = 0; i < 8; i++)
         afInside[i] = !pafInside[i];
      return MarchingCubeDontCompliment (dwNumMeta, papMeta, dwThis, !fFlip, 8 - dwPoints,
         afInside, padwBest, paID, papLoc, pafScore, TRUE);
   }

   // else, fail
   return FALSE;
}

/**********************************************************************************
MetaFindCrossing - Finds the crossing for the metaball.

inputs
   fp          fA - A value, from 0 to 1
   fp          fB - B value, from 0 to 1
returns
   fp - Alpha, from 0..1 where it crosses METABOUNDARY. 1.0 => crosses at A, 0.0 => crosses at B
*/
fp MetaFindCrossing (fp fA, fp fB)
{
   fp fDelta = fA - fB;
   if (!fDelta)
      return 0.5; // error
   fDelta = (METABOUNDARY - fB) / fDelta;

#if 0 // to test
   if ((fDelta < -0.1) || (fDelta > 1.1))
      fDelta *= 1;   // so can break here
#endif

   return fDelta;
}


/**********************************************************************************
MetaNewID - Given an ID for A and B points (which are only off by 1, on one axis),
this returns the ID to use.

inputs
   __int64        *piA - A point ID
   __int64        *piB - B point ID
   __int64        *piNew - New point
returns
   none
*/
void MetaNewID (const __int64 *piA, const __int64 *piB, __int64 *piNew)
{
   const short *psA = (const short*)piA;
   const short *psB = (const short*)piB;
   short *psNew = (short*)piNew;

   psNew[3] = 0;  // default dimensionality

   DWORD i;
   for (i = 0; i < 3; i++)
      if (psA[i] != psB[i]) {
         psNew[i] = min(psA[i], psB[i]);
         psNew[3] = (short) i;   //dimensionality
      }
      else
         psNew[i] = psA[i];
   // NOTE - may need to verify alpha != 0.0 or 1, but hasnt been a problem so far
}

/**********************************************************************************
MetaLeftRight - Give two points in the cubes, figures out what "left" and "right"
are.

inputs
   DWORD       dwStart - Value from 0..7 for start, 0x01 = x, 0x02 = y, 0x04 = z
   DWORD       dwEnd - Value for end (facing this direction). Only ONE bit in difference from dwStart
   DWORD       *pdwLeft - Filled with left bit
   DWORD       *pdwRight - Filled with right bit
*/
void MetaLeftRight (DWORD dwStart, DWORD dwEnd, DWORD *pdwLeft, DWORD *pdwRight)
{
   // determine which direction facing
   DWORD dwFace = dwStart ^ dwEnd;
   BOOL fPositive = TRUE;
   if (dwStart & 0x01)
      fPositive = !fPositive;
   if (dwStart & 0x02)
      fPositive = !fPositive;
   if (dwStart & 0x04)
      fPositive = !fPositive;
   switch (dwFace) {
   case 0x01:
      dwFace = 0;
      break;
   case 0x02:
      dwFace = 1;
      break;
   case 0x04:
      dwFace = 2;
      break;
   default:
      return; // error, shouldnt happen
   }

   if (fPositive) {
      *pdwLeft = 1 << ((dwFace + 3 - 1) % 3);
      *pdwRight = 1 << ((dwFace +1) % 3);
   }
   else {
      *pdwRight = 1 << ((dwFace + 3 - 1) % 3);
      *pdwLeft = 1 << ((dwFace +1) % 3);
   }
}

/**********************************************************************************
CMetaball::MarchingCube2 - Does the marching cube with two vertices.

inputs
   BOOL        fFlip - If TRUE then flip the vertices of the triangle when add
   BOOL        *pafInside - Array of 8 bools that checked if point inside metaball. Bit 0 = x, 1 = y, 2 = z
   DWORD       *padwBest - Array of 8 Index to the metaball with the most influence, -1 if none
   __int64     *paID - Array of 8 IDs for the points
   PCPoint     papLoc - Array of 8 point locations
   fp          *pafScore - Array of 8 scores for the locations
   DWORD       *padwAdjacent - Single adjacent point
returns
   BOOL - TRUE if success
*/
BOOL CMetaball::MarchingCube2 (BOOL fFlip,
                              BOOL *pafInside, DWORD *padwBest, __int64 *paID, PCPoint papLoc, fp *pafScore,
                              DWORD *padwAdjacent)
{
   CPoint ap[4];
   __int64 aID[4];
   DWORD i;

   // figure out left and right
   DWORD dwLeft, dwRight;
   MetaLeftRight (padwAdjacent[0], padwAdjacent[1], &dwLeft, &dwRight);

   for (i = 0; i < 4; i++) {
      DWORD dwStart = padwAdjacent[i / 2];
      DWORD dwDir = ((i == 1) || (i == 2)) ? dwLeft : dwRight;
      DWORD dwEnd = dwStart ^ dwDir;

      fp fAlpha = MetaFindCrossing(pafScore[dwStart], pafScore[dwEnd]);

      ap[i].Average (papLoc + dwStart, papLoc + dwEnd, fAlpha);
      MetaNewID (paID + dwStart, paID + dwEnd, &aID[i]);
   } // i

   if (!PolyAdd (4, &aID[0], &ap[0], !fFlip))
      return FALSE;
      // BUGFIX - Use !fFlip

   return TRUE;
}



/**********************************************************************************
CMetaball::MarchingCube2x - Does the marching cube with two vertices. These vertices
must be on opposite ends of the same side of the cube. It creates a long connection.

inputs
   BOOL        fFlip - If TRUE then flip the vertices of the triangle when add
   BOOL        *pafInside - Array of 8 bools that checked if point inside metaball. Bit 0 = x, 1 = y, 2 = z
   DWORD       *padwBest - Array of 8 Index to the metaball with the most influence, -1 if none
   __int64     *paID - Array of 8 IDs for the points
   PCPoint     papLoc - Array of 8 point locations
   fp          *pafScore - Array of 8 scores for the locations
   DWORD       dwAdjacent1 - Single adjacent point
   DWORD       dwAdjacent2 - Single adjacent point
returns
   BOOL - TRUE if success
*/
BOOL CMetaball::MarchingCube2x (BOOL fFlip,
                              BOOL *pafInside, DWORD *padwBest, __int64 *paID, PCPoint papLoc, fp *pafScore,
                              DWORD dwAdjacent1, DWORD dwAdjacent2)
{
   // figure out which side they're on
   DWORD dwSide = dwAdjacent1 ^ dwAdjacent2;
   dwSide = dwSide ^ 0x07; // since only 1 bit should be set after this
   DWORD dwSideVal = (dwAdjacent1 & dwSide) ? 1 : 0;

   CPoint ap[6];
   __int64 aID[6];
   DWORD i;

   DWORD dwStart, dwEnd, dwLeft, dwRight;
   fp fAlpha;

   dwStart = dwAdjacent1;
   dwEnd = dwStart ^ dwSide;
   fAlpha = MetaFindCrossing(pafScore[dwStart], pafScore[dwEnd]);
   ap[0].Average (papLoc + dwStart, papLoc + dwEnd, fAlpha);
   MetaNewID (paID + dwStart, paID + dwEnd, &aID[0]);

   MetaLeftRight (dwEnd, dwStart, &dwLeft, &dwRight);
   dwStart = dwAdjacent1;
   dwEnd = dwStart ^ dwLeft;
   fAlpha = MetaFindCrossing(pafScore[dwStart], pafScore[dwEnd]);
   ap[1].Average (papLoc + dwStart, papLoc + dwEnd, fAlpha);
   MetaNewID (paID + dwStart, paID + dwEnd, &aID[1]);

   dwEnd = dwStart ^ dwRight;
   fAlpha = MetaFindCrossing(pafScore[dwStart], pafScore[dwEnd]);
   ap[5].Average (papLoc + dwStart, papLoc + dwEnd, fAlpha);
   MetaNewID (paID + dwStart, paID + dwEnd, &aID[5]);

   dwStart = dwAdjacent2;
   dwEnd = dwStart ^ dwLeft;
   fAlpha = MetaFindCrossing(pafScore[dwStart], pafScore[dwEnd]);
   ap[4].Average (papLoc + dwStart, papLoc + dwEnd, fAlpha);
   MetaNewID (paID + dwStart, paID + dwEnd, &aID[4]);

   dwEnd = dwStart ^ dwRight;
   fAlpha = MetaFindCrossing(pafScore[dwStart], pafScore[dwEnd]);
   ap[2].Average (papLoc + dwStart, papLoc + dwEnd, fAlpha);
   MetaNewID (paID + dwStart, paID + dwEnd, &aID[2]);

   dwStart = dwAdjacent2;
   dwEnd = dwStart ^ dwSide;
   fAlpha = MetaFindCrossing(pafScore[dwStart], pafScore[dwEnd]);
   ap[3].Average (papLoc + dwStart, papLoc + dwEnd, fAlpha);
   MetaNewID (paID + dwStart, paID + dwEnd, &aID[3]);




   CPoint apTri[4];
   __int64 aiIDTri[4];
   DWORD adwReorder[8] = {0, 1, 2, 3,    0, 3, 4, 5};
   DWORD j;
   for (i = 0; i < 2; i++) {
      for (j = 0; j < 4; j++) {
         apTri[j].Copy (&ap[adwReorder[i*4+j]]);
         aiIDTri[j] = aID[adwReorder[i*4+j]];
      } // j

      if (!PolyAdd (4, &aiIDTri[0], &apTri[0], !fFlip))
         return FALSE;
   } // i


   return TRUE;
}

/**********************************************************************************
CMetaball::MarchingCube3 - Does the marching cube with three vertices.

inputs
   BOOL        fFlip - If TRUE then flip the vertices of the triangle when add
   BOOL        *pafInside - Array of 8 bools that checked if point inside metaball. Bit 0 = x, 1 = y, 2 = z
   DWORD       *padwBest - Array of 8 Index to the metaball with the most influence, -1 if none
   __int64     *paID - Array of 8 IDs for the points
   PCPoint     papLoc - Array of 8 point locations
   fp          *pafScore - Array of 8 scores for the locations
   DWORD       *padwAdjacent - Single adjacent point
returns
   BOOL - TRUE if success
*/
BOOL CMetaball::MarchingCube3 (BOOL fFlip,
                              BOOL *pafInside, DWORD *padwBest, __int64 *paID, PCPoint papLoc, fp *pafScore,
                              DWORD *padwAdjacent)
{
   CPoint ap[5];
   __int64 aID[5];
   DWORD i;

   DWORD dwXOr;

   // find the central point, that's adjacent to two others...
   for (i = 0; i < 3; i++) {
      dwXOr = (padwAdjacent[i] ^ padwAdjacent[(i+1)%3]);
      if (!dwXOr || (dwXOr & (dwXOr-1)))
         continue;   // more than one bit changed

      dwXOr = (padwAdjacent[i] ^ padwAdjacent[(i+2)%3]);
      if (!dwXOr || (dwXOr & (dwXOr-1)))
         continue;   // more than one bit changed

      // else, this one
      break;
   } // i

   // swap so center at the center
   DWORD dwLeft, dwRight, dwStart, dwEnd;
   fp fAlpha;
   dwXOr = padwAdjacent[i];
   padwAdjacent[i] = padwAdjacent[1];
   padwAdjacent[1] = dwXOr;

   // if left of center is the last point then swap first and left
   MetaLeftRight (padwAdjacent[0], padwAdjacent[1], &dwLeft, &dwRight);
   if ((padwAdjacent[1] ^ dwLeft) == padwAdjacent[2]) {
      dwXOr = padwAdjacent[0];
      padwAdjacent[0] = padwAdjacent[2];
      padwAdjacent[2] = dwXOr;
   }

   // make the points
   MetaLeftRight (padwAdjacent[0], padwAdjacent[1], &dwLeft, &dwRight);

   dwStart = padwAdjacent[0];
   dwEnd = dwStart ^ dwRight;
   fAlpha = MetaFindCrossing(pafScore[dwStart], pafScore[dwEnd]);
   ap[4].Average (papLoc + dwStart, papLoc + dwEnd, fAlpha);
   MetaNewID (paID + dwStart, paID + dwEnd, &aID[4]);

   dwStart = padwAdjacent[0];
   dwEnd = dwStart ^ dwLeft;
   fAlpha = MetaFindCrossing(pafScore[dwStart], pafScore[dwEnd]);
   ap[0].Average (papLoc + dwStart, papLoc + dwEnd, fAlpha);
   MetaNewID (paID + dwStart, paID + dwEnd, &aID[0]);

   dwStart = padwAdjacent[1];
   dwEnd = dwStart ^ dwLeft;
   fAlpha = MetaFindCrossing(pafScore[dwStart], pafScore[dwEnd]);
   ap[1].Average (papLoc + dwStart, papLoc + dwEnd, fAlpha);
   MetaNewID (paID + dwStart, paID + dwEnd, &aID[1]);

   MetaLeftRight (padwAdjacent[1], padwAdjacent[2], &dwLeft, &dwRight);

   dwStart = padwAdjacent[2];
   dwEnd = dwStart ^ dwLeft;
   fAlpha = MetaFindCrossing(pafScore[dwStart], pafScore[dwEnd]);
   ap[2].Average (papLoc + dwStart, papLoc + dwEnd, fAlpha);
   MetaNewID (paID + dwStart, paID + dwEnd, &aID[2]);

   dwEnd = dwStart ^ dwRight;
   fAlpha = MetaFindCrossing(pafScore[dwStart], pafScore[dwEnd]);
   ap[3].Average (papLoc + dwStart, papLoc + dwEnd, fAlpha);
   MetaNewID (paID + dwStart, paID + dwEnd, &aID[3]);

   CPoint apTri[3];
   __int64 aiIDTri[3];
   DWORD adwReorder[9] = {1, 4, 0,     1, 3, 4,    1, 2, 3};
   DWORD j;
   for (i = 0; i < 3; i++) {
      for (j = 0; j < 3; j++) {
         apTri[j].Copy (&ap[adwReorder[i*3+j]]);
         aiIDTri[j] = aID[adwReorder[i*3+j]];
      } // j

      if (!PolyAdd (3, &aiIDTri[0], &apTri[0], !fFlip))
         return FALSE;
   } // i

   return TRUE;
}



/**********************************************************************************
CMetaball::MarchingCube4 - Does the marching cube with four vertices.

inputs
   BOOL        fFlip - If TRUE then flip the vertices of the triangle when add
   BOOL        *pafInside - Array of 8 bools that checked if point inside metaball. Bit 0 = x, 1 = y, 2 = z
   DWORD       *padwBest - Array of 8 Index to the metaball with the most influence, -1 if none
   __int64     *paID - Array of 8 IDs for the points
   PCPoint     papLoc - Array of 8 point locations
   fp          *pafScore - Array of 8 scores for the locations
   DWORD       *padwAdjacent - Single adjacent point
returns
   BOOL - TRUE if success
*/
BOOL CMetaball::MarchingCube4 (BOOL fFlip,
                              BOOL *pafInside, DWORD *padwBest, __int64 *paID, PCPoint papLoc, fp *pafScore,
                              DWORD *padwAdjacent)
{
   CPoint ap[6];
   __int64 aID[6];
   DWORD i, j;

   // figure out the plane with the most points on it
   DWORD adwPlaneCount[3][2];
   memset (adwPlaneCount, 0, sizeof(adwPlaneCount));
   for (i = 0; i < 4; i++) {
      if (padwAdjacent[i] & 0x01)
         adwPlaneCount[0][1]++;
      else
         adwPlaneCount[0][0]++;

      if (padwAdjacent[i] & 0x02)
         adwPlaneCount[1][1]++;
      else
         adwPlaneCount[1][0]++;

      if (padwAdjacent[i] & 0x04)
         adwPlaneCount[2][1]++;
      else
         adwPlaneCount[2][0]++;
   } // i
   DWORD dwPlane = 0, dwPlaneVal = 0;
   for (i = 0; i < 3; i++) for (j = 0; j < 2; j++)
      if (adwPlaneCount[i][j] > adwPlaneCount[dwPlane][dwPlaneVal]) {
         dwPlane = i;
         dwPlaneVal = j;
      }
   DWORD dwPlaneNum = dwPlane;
   dwPlane = 1 << dwPlane; // so have a bit wise

   // move all the points on the main plane into one bin, and the others into
   // a second. It should either be 3 points + 1 point, or 4 points
   DWORD adwPlane[4], adwOther[1];
   DWORD dwNumOnPlane = 0, dwNumOnOther = 0;
   for (i = 0; i < 4; i++) {
      DWORD dwVal = padwAdjacent[i] & dwPlane;
      BOOL fOnPlane = (padwAdjacent[i] & dwVal) ? TRUE : FALSE;
      if (!dwPlaneVal)
         fOnPlane = !fOnPlane;

      if (fOnPlane)
         adwPlane[dwNumOnPlane++] = padwAdjacent[i];
      else
         adwOther[dwNumOnOther++] = padwAdjacent[i];
   } // i

   // if they're all on a plane then just a quad
   DWORD dwStart, dwEnd;
   fp fAlpha;
   if (!dwNumOnOther) {
      // pretend for modelling that all on X=0 plane
      DWORD dwPlaneY = 1 << ((dwPlaneNum + 1) % 3);
      DWORD dwPlaneZ = 1 << ((dwPlaneNum + 2) % 3);

      for (i = 0; i < 4; i++) {
         dwStart = dwEnd = ((i < 2) ? 0 : dwPlaneY) | (((i == 1) || (i == 2)) ? dwPlaneZ : 0);
         if (dwPlaneVal)
            dwStart = dwStart ^ dwPlane;
         else
            dwEnd = dwEnd ^ dwPlane;

         fAlpha = MetaFindCrossing(pafScore[dwStart], pafScore[dwEnd]);
         ap[i].Average (papLoc + dwStart, papLoc + dwEnd, fAlpha);
         MetaNewID (paID + dwStart, paID + dwEnd, &aID[i]);
      } // i

      if (dwPlaneVal)
         fFlip = !fFlip;

      if (!PolyAdd (4, &aID[0], &ap[0], fFlip))
         return FALSE;

      return TRUE;
   }

   // if get here, dwNumOnOther = 1, dwNumOnPlane =3

   // find the central point, that's adjacent to two others...
   DWORD dwXOr;
   for (i = 0; i < 3; i++) {
      dwXOr = (adwPlane[i] ^ adwPlane[(i+1)%3]);
      if (!dwXOr || (dwXOr & (dwXOr-1)))
         continue;   // more than one bit changed

      dwXOr = (adwPlane[i] ^ adwPlane[(i+2)%3]);
      if (!dwXOr || (dwXOr & (dwXOr-1)))
         continue;   // more than one bit changed

      // else, this one
      break;
   } // i

   // swap so center at the center
   DWORD dwLeft, dwRight;
   dwXOr = adwPlane[i];
   adwPlane[i] = adwPlane[1];
   adwPlane[1] = dwXOr;

   // if left of center is the last point then swap first and left
   MetaLeftRight (adwPlane[0], adwPlane[1], &dwLeft, &dwRight);
   if ((adwPlane[1] ^ dwLeft) == adwPlane[2]) {
      dwXOr = adwPlane[0];
      adwPlane[0] = adwPlane[2];
      adwPlane[2] = dwXOr;
   }

   // points to use
   DWORD *padwTri1 = NULL, *padwTri2 = NULL;
   DWORD *padwQuad1 = NULL;

   DWORD adwTri1Center[] = {1, 5, 0};
   DWORD adwTri2Center[] = {2, 3, 4};
   DWORD adwQuad1Center[] = {1, 2, 4, 5};

   // see if the extra point is off the central bit
   dwXOr = adwPlane[1] ^ adwOther[0];
   if (dwXOr && !(dwXOr & (dwXOr-1))) {
      MetaLeftRight (adwPlane[0], adwPlane[1], &dwLeft, &dwRight);
      dwStart = adwPlane[0];
      dwEnd = dwStart ^ dwLeft;
      fAlpha = MetaFindCrossing(pafScore[dwStart], pafScore[dwEnd]);
      ap[0].Average (papLoc + dwStart, papLoc + dwEnd, fAlpha);
      MetaNewID (paID + dwStart, paID + dwEnd, &aID[0]);

      dwEnd = dwStart ^ dwRight;
      fAlpha = MetaFindCrossing(pafScore[dwStart], pafScore[dwEnd]);
      ap[5].Average (papLoc + dwStart, papLoc + dwEnd, fAlpha);
      MetaNewID (paID + dwStart, paID + dwEnd, &aID[5]);

      MetaLeftRight (adwPlane[1], adwOther[0], &dwLeft, &dwRight);
      dwStart = adwOther[0];
      dwEnd = dwStart ^ dwLeft;
      fAlpha = MetaFindCrossing(pafScore[dwStart], pafScore[dwEnd]);
      ap[1].Average (papLoc + dwStart, papLoc + dwEnd, fAlpha);
      MetaNewID (paID + dwStart, paID + dwEnd, &aID[1]);

      dwEnd = dwStart ^ dwRight;
      fAlpha = MetaFindCrossing(pafScore[dwStart], pafScore[dwEnd]);
      ap[2].Average (papLoc + dwStart, papLoc + dwEnd, fAlpha);
      MetaNewID (paID + dwStart, paID + dwEnd, &aID[2]);

      MetaLeftRight (adwPlane[1], adwPlane[2], &dwLeft, &dwRight);
      dwStart = adwPlane[2];
      dwEnd = dwStart ^ dwLeft;
      fAlpha = MetaFindCrossing(pafScore[dwStart], pafScore[dwEnd]);
      ap[3].Average (papLoc + dwStart, papLoc + dwEnd, fAlpha);
      MetaNewID (paID + dwStart, paID + dwEnd, &aID[3]);

      dwEnd = dwStart ^ dwRight;
      fAlpha = MetaFindCrossing(pafScore[dwStart], pafScore[dwEnd]);
      ap[4].Average (papLoc + dwStart, papLoc + dwEnd, fAlpha);
      MetaNewID (paID + dwStart, paID + dwEnd, &aID[4]);

      padwTri1 = adwTri1Center;
      padwTri2 = adwTri2Center;
      padwQuad1 = adwQuad1Center;
   }

   // see if it's off the last one
   DWORD adwTri1Last[] = {0, 5, 1};
   DWORD adwTri2Last[] = {2, 4, 3};
   DWORD adwQuad1Last[] = {1, 5, 4, 2};
   dwXOr = adwPlane[2] ^ adwOther[0];
   if (dwXOr && !(dwXOr & (dwXOr-1))) {
      MetaLeftRight (adwPlane[0], adwPlane[1], &dwLeft, &dwRight);
      dwStart = adwPlane[0];
      dwEnd = dwStart ^ dwLeft;
      fAlpha = MetaFindCrossing(pafScore[dwStart], pafScore[dwEnd]);
      ap[0].Average (papLoc + dwStart, papLoc + dwEnd, fAlpha);
      MetaNewID (paID + dwStart, paID + dwEnd, &aID[0]);

      dwEnd = dwStart ^ dwRight;
      fAlpha = MetaFindCrossing(pafScore[dwStart], pafScore[dwEnd]);
      ap[1].Average (papLoc + dwStart, papLoc + dwEnd, fAlpha);
      MetaNewID (paID + dwStart, paID + dwEnd, &aID[1]);

      dwStart = adwPlane[1];
      dwEnd = dwStart ^ dwLeft;
      fAlpha = MetaFindCrossing(pafScore[dwStart], pafScore[dwEnd]);
      ap[5].Average (papLoc + dwStart, papLoc + dwEnd, fAlpha);
      MetaNewID (paID + dwStart, paID + dwEnd, &aID[5]);

      MetaLeftRight (adwPlane[2], adwOther[0], &dwLeft, &dwRight);
      dwStart = adwPlane[2];
      dwEnd = dwStart ^ dwRight;
      fAlpha = MetaFindCrossing(pafScore[dwStart], pafScore[dwEnd]);
      ap[2].Average (papLoc + dwStart, papLoc + dwEnd, fAlpha);
      MetaNewID (paID + dwStart, paID + dwEnd, &aID[2]);

      dwStart = adwOther[0];
      dwEnd = dwStart ^ dwLeft;
      fAlpha = MetaFindCrossing(pafScore[dwStart], pafScore[dwEnd]);
      ap[4].Average (papLoc + dwStart, papLoc + dwEnd, fAlpha);
      MetaNewID (paID + dwStart, paID + dwEnd, &aID[4]);

      dwEnd = dwStart ^ dwRight;
      fAlpha = MetaFindCrossing(pafScore[dwStart], pafScore[dwEnd]);
      ap[3].Average (papLoc + dwStart, papLoc + dwEnd, fAlpha);
      MetaNewID (paID + dwStart, paID + dwEnd, &aID[3]);

      padwTri1 = adwTri1Last;
      padwTri2 = adwTri2Last;
      padwQuad1 = adwQuad1Last;
   }


   // if extra off the first one
   DWORD adwTri1First[] = {3, 5, 4};
   DWORD adwTri2First[] = {2, 1, 0};
   DWORD adwQuad1First[] = {3, 2, 0, 5};
   dwXOr = adwPlane[0] ^ adwOther[0];
   if (dwXOr && !(dwXOr & (dwXOr-1))) {
      MetaLeftRight (adwOther[0], adwPlane[0], &dwLeft, &dwRight);
      dwStart = adwOther[0];
      dwEnd = dwStart ^ dwLeft;
      fAlpha = MetaFindCrossing(pafScore[dwStart], pafScore[dwEnd]);
      ap[3].Average (papLoc + dwStart, papLoc + dwEnd, fAlpha);
      MetaNewID (paID + dwStart, paID + dwEnd, &aID[3]);

      dwEnd = dwStart ^ dwRight;
      fAlpha = MetaFindCrossing(pafScore[dwStart], pafScore[dwEnd]);
      ap[4].Average (papLoc + dwStart, papLoc + dwEnd, fAlpha);
      MetaNewID (paID + dwStart, paID + dwEnd, &aID[4]);

      dwStart = adwPlane[0];
      dwEnd = dwStart ^ dwRight;
      fAlpha = MetaFindCrossing(pafScore[dwStart], pafScore[dwEnd]);
      ap[5].Average (papLoc + dwStart, papLoc + dwEnd, fAlpha);
      MetaNewID (paID + dwStart, paID + dwEnd, &aID[5]);

      MetaLeftRight (adwPlane[1], adwPlane[2], &dwLeft, &dwRight);
      dwStart = adwPlane[1];
      dwEnd = dwStart ^ dwLeft;
      fAlpha = MetaFindCrossing(pafScore[dwStart], pafScore[dwEnd]);
      ap[2].Average (papLoc + dwStart, papLoc + dwEnd, fAlpha);
      MetaNewID (paID + dwStart, paID + dwEnd, &aID[2]);

      dwStart = adwPlane[2];
      dwEnd = dwStart ^ dwLeft;
      fAlpha = MetaFindCrossing(pafScore[dwStart], pafScore[dwEnd]);
      ap[1].Average (papLoc + dwStart, papLoc + dwEnd, fAlpha);
      MetaNewID (paID + dwStart, paID + dwEnd, &aID[1]);

      dwEnd = dwStart ^ dwRight;
      fAlpha = MetaFindCrossing(pafScore[dwStart], pafScore[dwEnd]);
      ap[0].Average (papLoc + dwStart, papLoc + dwEnd, fAlpha);
      MetaNewID (paID + dwStart, paID + dwEnd, &aID[0]);

      padwTri1 = adwTri1First;
      padwTri2 = adwTri2First;
      padwQuad1 = adwQuad1First;
   }

   CPoint apTri[4];
   __int64 aiIDTri[4];
   for (i = 0; i < 3; i++) {
      DWORD *pdw;
      DWORD dwNum;
      switch (i) {
      case 0:
         pdw = padwTri1;
         dwNum = 3;
         break;
      case 1:
         pdw = padwTri2;
         dwNum = 3;
         break;
      case 2:
         pdw = padwQuad1;
         dwNum = 4;
         break;
      }

      if (!pdw)
         continue;   // no entry

      for (j = 0; j < dwNum; j++) {
         apTri[j].Copy (&ap[pdw[j]]);
         aiIDTri[j] = aID[pdw[j]];
      } // j

      if (!PolyAdd (dwNum, &aiIDTri[0], &apTri[0], !fFlip))
         return FALSE;
   } // i


   return TRUE;
}

/**********************************************************************************
CMetaball::MarchingCube1 - Does the marching cube with an isolated vertex.

inputs
   BOOL        fFlip - If TRUE then flip the vertices of the triangle when add
   BOOL        *pafInside - Array of 8 bools that checked if point inside metaball. Bit 0 = x, 1 = y, 2 = z
   DWORD       *padwBest - Array of 8 Index to the metaball with the most influence, -1 if none
   __int64     *paID - Array of 8 IDs for the points
   PCPoint     papLoc - Array of 8 point locations
   fp          *pafScore - Array of 8 scores for the locations
   DWORD       dwAdjacent - Single adjacent point
returns
   BOOL - TRUE if success
*/
BOOL CMetaball::MarchingCube1 (BOOL fFlip,
                              BOOL *pafInside, DWORD *padwBest, __int64 *paID, PCPoint papLoc, fp *pafScore,
                              DWORD dwAdjacent)
{
   CPoint ap[3];
   __int64 aID[3];
   DWORD i;

   for (i = 0; i < 3; i++) {
      DWORD dwB;
      dwB = dwAdjacent ^ (1 << i);   // other XYZ value

      fp fAlpha = MetaFindCrossing(pafScore[dwAdjacent], pafScore[dwB]);

      ap[i].Average (papLoc + dwAdjacent, papLoc + dwB, fAlpha);
      MetaNewID (paID + dwAdjacent, paID + dwB, &aID[i]);
   } // i

   // flip for ever XYZ where not on 0,0,0
   if (dwAdjacent & 0x01)
      fFlip = !fFlip;
   if (dwAdjacent & 0x02)
      fFlip = !fFlip;
   if (dwAdjacent & 0x04)
      fFlip = !fFlip;

   if (!PolyAdd (3, &aID[0], &ap[0], !fFlip))
      return FALSE;
      // BUGFIX - For triangle need to flip whatever calculated

   return TRUE;
}



/**********************************************************************************
CMetaball::IsValidGet - Returns the state of the m_fIsValid flag, indicating
if the metaball is valid to render.
*/
BOOL CMetaball::IsValidGet (void)
{
   return m_fIsValid;
}


/**********************************************************************************
CMetaball::CalcPolygonNormals - Calculates the normals of the polygons, NOT
the verticles. Call this after perterbring the points with a noise function.
*/
void CMetaball::CalcPolygonNormals (void)
{
   DWORD dwNum = m_lMETAPOLY.Num();
   PMETAPOLY pmp = (PMETAPOLY) m_lMETAPOLY.Get (0);
   PCPoint papPoints = (PCPoint) m_lPoint.Get(0);
   PVERTEX pv = (PVERTEX) m_lVERTEX.Get(0);

   DWORD i, j;
   CPoint p1, p2, pN;
   for (i = 0; i < dwNum; i++, pmp++) {
      pmp->p1.Zero();

      // loop over all the points and average the normal. However, for
      // a triangle only need one vertex
      DWORD dwVert = (pmp->awVert[3] == (WORD)-1) ? 3 : 4;
      DWORD dwLoop = (dwVert == 4) ? 4 : 1;
      for (j = 0; j < dwLoop; j++) {
         PVERTEX pv0 = &pv[pmp->awVert[j]];
         PVERTEX pv1 = &pv[pmp->awVert[(j+1)%dwVert]];
         PVERTEX pv2 = &pv[pmp->awVert[(j+dwVert-1)%dwVert]];
         p1.Subtract (papPoints + pv1->dwPoint, papPoints + pv0->dwPoint);
         p2.Subtract (papPoints + pv2->dwPoint, papPoints + pv0->dwPoint);
         //p1.Subtract (papPoints + pmp->awVert[(j+1)%dwVert], papPoints + pmp->awVert[j]);
         //p2.Subtract (papPoints + pmp->awVert[(j+dwVert-1)%dwVert], papPoints + pmp->awVert[j]);
         pN.CrossProd (&p2, &p1);
         pN.Normalize();
         pmp->p1.Add (&pN);
      } // j

      // renormalize if have quad
      if (dwLoop > 1)
         pmp->p1.Normalize();
   } // i
}


/**********************************************************************************
CMetaball::FindPolyWithPoint - This finds all the polygons using the given
point.

inputs
   DWORD          dwIndex - if looking at self, then pass a point index in
                  so don't need to search. Otherwise, pass a -1 in, and will
                  use pPoint
   PCPoint        pPoint - Point to use. (BUGFIX - Was using the ID, but not realiable)
   __int64        *pID - Point ID. Mainly used for bounding box purposes
   PCListFixed    plPMETAPOLY - Must be pre-initialized to PMETAPOLY. THis
                  will append with PMETAPOLY with
                  the vertex indecies that match.
returns
   BOOL - TRUE if filled in plIndex. FALSE if trivially rejected by bouding box
*/
BOOL CMetaball::FindPolyWithPoint (DWORD dwIndex, PCPoint pPoint, __int64 *piID, PCListFixed plPMETAPOLY)
{
   // make sure in bounding box
   short *psID = (short*)piID;
   DWORD i, j;
   for (i = 0; i < 3; i++) {
      if ((psID[i] < m_aiBoundMin[i]-1) || (psID[i] >= m_aiBoundMax[i]+1))
         return FALSE;
   } // i

   // look for matches
   if (dwIndex == (DWORD) -1) {
      PCPoint paPoints = (PCPoint)m_lPoint.Get(0);
      DWORD dwNum = m_lPoint.Num();

      for (i = 0; i < dwNum; i++, paPoints++)
         if (paPoints->AreClose (pPoint)) // BUGFIX - was comparing point IDs, but not reliable
            break;
      if (i >= dwNum)
         return FALSE;  // nothing
   }
   else
      i = dwIndex;

   // else found, see if can find vertics
   WORD wPoint = (WORD)i;
   PMETAPOLY pmp = (PMETAPOLY) m_lMETAPOLY.Get(0);
   DWORD dwNum = m_lMETAPOLY.Num();
   for (i = 0; i < dwNum; i++, pmp++) {
      for (j = 0; j < 4; j++)
         if (pmp->awVert[j] == wPoint) {
            // found a match
            plPMETAPOLY->Add (&pmp);
            break;
         }
   } // i

   return TRUE;
}


/**********************************************************************************
CMetaball::FindIntersectingMetaballs - Find other metaballs that intersect with
this one.

inputs
   DWORD       dwNumMeta - Number of metaballs
   PCMetaball  *papMeta - Pointer to an array of dwNumMeta metaballs. This one is
                  one of the metaballs.
   PCListFixed plPCMetaball - Initialized to PCMetaball and filled in with intersecting
                  metaballs. If it doesnt intserect then filled with NULL.
   PCListFixed plMetaRange - Filled with metaball range (2 x 3 shorts). This
                  parameter can be NULL.
returns
   none
*/
void CMetaball::FindIntersectingMetaballs (DWORD dwNumMeta, PCMetaball *papMeta,
                                           PCListFixed plPCMetaball, PCListFixed plMetaRange)
{
   DWORD i, j;
   short aiRange[2][3];
   plPCMetaball->Init (sizeof(PCMetaball));
   if (plMetaRange)
      plMetaRange->Init (sizeof(aiRange));
   PCMetaball pMeta;
   for (i = 0; i < dwNumMeta; i++) {
      papMeta[i]->BoundingBoxGet (&aiRange[0][0], &aiRange[1][0]);

      // see if out of bounds
      for (j = 0; j < 3; j++) {
         if (aiRange[0][j] >= m_aiBoundMax[j])
            break;
         if (aiRange[1][j] <= m_aiBoundMin[j])
            break;
      } // j
      if ((j < 3) && (papMeta[i] != this)) {
         pMeta = NULL;
         plPCMetaball->Add (&pMeta);
         if (plMetaRange)
            plMetaRange->Add (aiRange);
         continue;   // not doesnt overlap
      }

      // remember this since will intersect
      pMeta = papMeta[i];
      plPCMetaball->Add (&pMeta);
      if (plMetaRange)
         plMetaRange->Add (aiRange);
   } // i

}

/**********************************************************************************
CMetaball::CalcVertexNormals - Calculates the vertex normals. Call this AFTER
all the polygon normals have been calculated for this metaball and all adjacent ones.

This also calculates the texture points.

Once all the normals and textures are properly calculated this is marked
as valid, with m_fIsValid = TRUE.

inputs
   DWORD       dwNumMeta - Number of metaballs
   PCMetaball  *papMeta - Pointer to an array of dwNumMeta metaballs. This one is
                  one of the metaballs.
*/
void CMetaball::CalcVertexNormals (DWORD dwNumMeta, PCMetaball *papMeta)
{
   // find all metaballs that intersect with this
   CListFixed lPCMetaball;
   FindIntersectingMetaballs (dwNumMeta, papMeta, &lPCMetaball, NULL);
   papMeta = (PCMetaball*)lPCMetaball.Get(0);

   // go through all the points and calculate the normals
   CListFixed lPMETAPOLY;
   lPMETAPOLY.Init (sizeof(PMETAPOLY));
   DWORD dwNum = m_lPointID.Num();
   __int64 *paiID = (__int64*)m_lPointID.Get(0);
   PCPoint paPoints = (PCPoint)m_lPoint.Get(0);
   m_lNorm.Init (sizeof(CPoint));
   DWORD i, j;
   CPoint pN;
   PMETAPOLY *ppmp;
   for (i = 0; i < dwNum; i++, paiID++, paPoints++) {
      lPMETAPOLY.Clear();

      // go through all metaballs and ask for normals
      for (j = 0; j < dwNumMeta; j++)
         if (papMeta[j])
            papMeta[j]->FindPolyWithPoint ((papMeta[j] == this) ? i : (DWORD)-1,
               paPoints, paiID, &lPMETAPOLY);

      // average the normals
      pN.Zero();
      ppmp = (PMETAPOLY*) lPMETAPOLY.Get(0);
      DWORD dwNumPoly = lPMETAPOLY.Num();
      for (j = 0; j < dwNumPoly; j++)
         pN.Add (&ppmp[j]->p1);
      if (!dwNumPoly)
         pN.p[0] = 1;   // shouldnt happen
      else if (dwNumPoly > 1)
         pN.Normalize();

      // add this
      m_lNorm.Add (&pN);
   } // i


   // mark as valid
   m_fIsValid = TRUE;
}


/**********************************************************************************
CMetaball::CalcPolyMinMax - Calculates the polygon min and max, so have fast
intersections.
*/
void CMetaball::CalcPolyMinMax (void)
{
   PMETAPOLY pmp = (PMETAPOLY)m_lMETAPOLY.Get(0);
   PCPoint paPoint = (PCPoint)m_lPoint.Get(0);
   PCPoint paNorm = (PCPoint)m_lNorm.Get(0);
   PVERTEX paVert = (PVERTEX)m_lVERTEX.Get(0);
   DWORD dwNumVert = m_lVERTEX.Num();
   PCPoint pap[4];

   DWORD i, j;

   for (i = 0; i < m_lMETAPOLY.Num(); i++, pmp++) {
      for (j = 0; j < 4; j++) {
         if (pmp->awVert[j] >= (WORD)dwNumVert) {
            pap[j] = NULL;
            continue;
         }

         // else, get the point and normal
         pap[j] = &paPoint[paVert[pmp->awVert[j]].dwPoint];
      } // j

      // min and max
      pmp->p1.Copy (pap[0]);
      pmp->p2.Copy (pap[0]);
      for (j = 1; j < 3; j++)
         if (pap[j]) {
            pmp->p1.Min(pap[j]);
            pmp->p2.Max(pap[j]);
         }
   } // i
}

/**********************************************************************************
CMetaball::Render - Draws the metaball

inputs
   POBJECTRENDER     pr - Render information
   PCRenderSurface   prs - Render surface
   BOOL              fBackface - Set to TRUE if backface culling OK
   BOOL              fBoxes - If TRUE, draw sub-objects as boxes
   PCMetaSurface     pms - Surface used
   PCObjectCave      pCave - Cave info
   fp                fDetail - Amount of detail in the object ,so can pair out
                        very small objects
*/
void CMetaball::Render (POBJECTRENDER pr, PCRenderSurface prs, BOOL fBackface,
                        BOOL fBoxes, PCMetaSurface pms,
                        PCObjectCave pCave, fp fDetail)
{
   DWORD dwRenderShard = pCave->m_OSINFO.dwRenderShard;

   DWORD dwPoints = m_lPoint.Num();
   DWORD dwPoly = m_lMETAPOLY.Num();
   if (!dwPoints || !dwPoly) {
      // only care if this is a temporary render
      if (pr->dwReason != ORREASON_WORKING)
         return;   // dont draw for final

      // draw a sphere for a fill-in

      PRENDERSURFACE prsOld = prs->GetDefSurface ();

      // else, set red
      RENDERSURFACE ren;
      memset (&ren, 0, sizeof(ren));
      ren.Material.InitFromID (MATERIAL_PAINTMATTE);
      ren.wMinorID = (WORD)(prsOld ? prsOld->wMinorID : 0);
      prs->SetDefMaterial (&ren);
      prs->SetDefColor (RGB(0, 0, 0xff));

      prs->Push();
      CMatrix mRot;
      mRot.FromXYZLLT (&m_pCenter, m_pLLT.p[2], m_pLLT.p[0], m_pLLT.p[1]);
      prs->Multiply (&mRot);
      prs->ShapeEllipsoid (m_pSize.p[0]/4, m_pSize.p[1]/4, m_pSize.p[2]/4);
      prs->Pop();

      return;  // nothing to render
   }

   // points
   DWORD dwPointIndex;
   PCPoint paPoints = prs->NewPoints (&dwPointIndex, dwPoints);
   if (!paPoints)
      return;
   memcpy (paPoints, m_lPoint.Get(0), dwPoints * sizeof(CPoint));

   // normals
   DWORD dwNormIndex;
   PCPoint paNorms = prs->NewNormals (TRUE, &dwNormIndex, dwPoints);
   if (paNorms)
      memcpy (paNorms, m_lNorm.Get(0), dwPoints * sizeof(CPoint));

   // texutres
   DWORD dwTextIndex;
   DWORD dwTexts = m_lText.Num();
   PTEXTPOINT5 paText = prs->NewTextures (&dwTextIndex, dwTexts);
   if (paText) {
      memcpy (paText, m_lText.Get(0), dwTexts * sizeof(TEXTPOINT5));
      prs->ApplyTextureRotation (paText, dwTexts);
      
      // undo scaling for some textures
      DWORD j, k;
      PTEXTPOINT5 paOrig = (PTEXTPOINT5)m_lText.Get(0);
      for (j = 0; j < 2; j++) {
         if (pms->m_adwMethod[j] > 10)
            continue;

         for (k = 0; k < dwTexts; k++)
            paText[k].hv[j] = paOrig[k].hv[j];
      } // j
   }

   // vertices
   DWORD dwVertIndex;
   PVERTEX pv = prs->NewVertices (&dwVertIndex, m_lVERTEX.Num());
   PVERTEX pvOrig = (PVERTEX) m_lVERTEX.Get(0);
   DWORD i;
   for (i = 0; i < m_lVERTEX.Num(); i++, pv++, pvOrig++) {
      pv->dwNormal = paNorms ? (dwNormIndex + pvOrig->dwNormal) : 0;
      pv->dwTexture = paText ? (dwTextIndex + pvOrig->dwTexture) : 0;
      pv->dwPoint = dwPointIndex + pvOrig->dwPoint;
   } // i

   // polygons
   PMETAPOLY pmp = (PMETAPOLY)m_lMETAPOLY.Get(0);
   for (i = 0; i < dwPoly; i++, pmp++) {
      if (pmp->awVert[3] == (WORD)-1)
         prs->NewTriangle (
            dwVertIndex + (DWORD)pmp->awVert[0],
            dwVertIndex + (DWORD)pmp->awVert[1],
            dwVertIndex + (DWORD)pmp->awVert[2],
            fBackface);
      else
         prs->NewQuad (
            dwVertIndex + (DWORD)pmp->awVert[0],
            dwVertIndex + (DWORD)pmp->awVert[1],
            dwVertIndex + (DWORD)pmp->awVert[2],
            dwVertIndex + (DWORD)pmp->awVert[3],
            fBackface);
   } // i

   // find out if the renderer supports clones
   BOOL fCloneRender;
   CListFixed lCloneMatrix;
   CMatrix mMatrixObject;
   pCave->ObjectMatrixGet (&mMatrixObject);
   fCloneRender = pr->pRS->QueryCloneRender();
   lCloneMatrix.Init (sizeof(CMatrix));

   // draw all the objects
   PCAVEOBJECTINFO pcoi = (PCAVEOBJECTINFO) m_lCAVEOBJECTINFO.Get (0);
   DWORD dwNum = m_lCAVEOBJECTINFO.Num();
   PCMetaSurface *ppms = (PCMetaSurface*) pCave->m_lPCMetaSurface.Get(0);
   DWORD dwNumSurface = pCave->m_lPCMetaSurface.Num();
   DWORD dwStart, dwEnd;
   PCCaveCanopy pcc;
   CMatrix mLoc;

   if (fBoxes && dwNum) {
      CObjectSurface os;
      os.m_dwID = TREEID;
      os.m_Material.InitFromID (MATERIAL_FLAT);
      os.m_cColor = RGB(0,0xff,0xff);

      prs->SetDefMaterial (pCave->m_OSINFO.dwRenderShard, &os, pCave->m_pWorld);
   }

   for (dwStart = 0; dwStart < dwNum; dwStart = dwEnd) {
      // find the end
      for (dwEnd = dwStart+1; dwEnd < dwNum; dwEnd++)
         if ((pcoi[dwStart].dwMetaSurface != pcoi[dwEnd].dwMetaSurface) ||
            (pcoi[dwStart].dwCanopy != pcoi[dwEnd].dwCanopy) ||
            (pcoi[dwStart].dwType != pcoi[dwEnd].dwType))
            break;

      // get pointer
      if (pcoi[dwStart].dwMetaSurface >= dwNumSurface)
         continue;   // shouldn happen
      PCMetaSurface pms = ppms[pcoi[dwStart].dwMetaSurface];
      pcc = pms->m_apCaveCanopy[pcoi[dwStart].dwCanopy];
      if (pcoi[dwStart].dwType >= pcc->m_lCANOPYTREE.Num())
         continue; // shouldnt happen
      if (pcc->MaxTreeSize(dwRenderShard) < fDetail)
         continue;

      // get the object
      PCObjectClone pClone = pcc->TreeClone(dwRenderShard, pcoi[dwStart].dwType);
      if (!pClone)
         continue;
      if (pClone->MaxSize() < fDetail)
         continue;   // too small

      // create the matrices
      lCloneMatrix.Clear();
      for (i = dwStart; i < dwEnd; i++) {
         if (fBoxes)
            lCloneMatrix.Add (&pcoi[i].mMatrix);
         else {
            mLoc.Multiply (&mMatrixObject, &pcoi[i].mMatrix);
            lCloneMatrix.Add (&mLoc);
         }
      }

      // draw
      PCMatrix pm = (PCMatrix)lCloneMatrix.Get(0);
      if (fCloneRender)
         pr->pRS->CloneRender (&pClone->m_gCode, &pClone->m_gSub,
            lCloneMatrix.Num(), (PCMatrix)lCloneMatrix.Get(0));
      else if (fBoxes) for (i = 0; i < lCloneMatrix.Num(); i++) {
         prs->Push();

         // get the box for the tree
         CPoint pC[2];
         CMatrix m;
         pClone->BoundingBoxGet (&m, &pC[0], &pC[1]);
         m.MultiplyRight (&pm[i]);
         prs->Multiply (&m);

         // draw box
         //prs->ShapeBox (pC[0].p[0], pC[0].p[1], pC[0].p[2],
         //   pC[1].p[0], pC[1].p[1], pC[1].p[2]);

         // BUGFIX - Draw a diamond so the trees take up less space
         // draw as diamond
         PCPoint pap;
         DWORD dwPoints;
         fp fHalf;
         pap = prs->NewPoints (&dwPoints, 6);
         if (!pap)
            goto pop;
         fHalf = (pC[0].p[2] + pC[1].p[2]) / 2.0;
         pap[0].Zero();
         pap[0].p[0] = (pC[0].p[0] + pC[1].p[0]) / 2;
         pap[0].p[1] = pC[0].p[1];
         pap[0].p[2] = fHalf;
         pap[2].Copy (&pap[0]);
         pap[2].p[1] = pC[1].p[1];

         pap[1].Zero();
         pap[1].p[0] = pC[0].p[0];
         pap[1].p[1] = (pC[0].p[1] + pC[1].p[1]) / 2;
         pap[1].p[2] = fHalf;
         pap[3].Copy (&pap[1]);
         pap[3].p[0] = pC[1].p[0];

         pap[4].Zero();
         pap[4].p[0] = pap[0].p[0];
         pap[4].p[1] = pap[1].p[1];
         pap[4].p[2] = pC[0].p[2];
         pap[5].Copy (&pap[4]);
         pap[5].p[2] = pC[1].p[2];

         // textures
         DWORD dwText;
         dwText = prs->NewTexture (0, 0, 0, 0, 0);

         DWORD dwNorm;
         PCPoint paNorm;
         dwNorm = 0;
         paNorm = prs->NewNormals (TRUE, &dwNorm, 3);
         if (paNorm) {
            paNorm[0].Zero();
            paNorm[0].p[0] = 1;
            paNorm[1].Zero();
            paNorm[1].p[1] = 1;
            paNorm[2].Zero();
            paNorm[2].p[2] = 1;
         }

         DWORD dwVert, dw;
         PVERTEX pv;
         for (dw = 0; dw < 3; dw++) {
            pv = prs->NewVertices (&dwVert, 4);
            if (!pv)
               continue;

            pv[0].dwColor = pv[1].dwColor = pv[2].dwColor = pv[3].dwColor = prs->DefColor ();
            pv[0].dwTexture = pv[1].dwTexture = pv[2].dwTexture = pv[3].dwTexture = dwText;
            pv[0].dwNormal = pv[1].dwNormal = pv[2].dwNormal = pv[3].dwNormal =
               paNorm ? (dwNorm + dw) : 0;

            switch (dw) {
            case 0:  // NS
               pv[0].dwPoint = dwPoints + 0;
               pv[1].dwPoint = dwPoints + 4;
               pv[2].dwPoint = dwPoints + 2;
               pv[3].dwPoint = dwPoints + 5;
               break;
            case 1: // ew
               pv[0].dwPoint = dwPoints + 1;
               pv[1].dwPoint = dwPoints + 4;
               pv[2].dwPoint = dwPoints + 3;
               pv[3].dwPoint = dwPoints + 5;
               break;
            case 2:  // level
               pv[0].dwPoint = dwPoints + 0;
               pv[1].dwPoint = dwPoints + 1;
               pv[2].dwPoint = dwPoints + 2;
               pv[3].dwPoint = dwPoints + 3;
               break;
            }

            // add quad
            prs->NewQuad (dwVert + 0, dwVert + 1, dwVert + 2, dwVert + 3, FALSE);
         }
pop:
         prs->Pop();
      }
      else {
         // draw original objects
         for (i = 0; i < lCloneMatrix.Num(); i++)
            pClone->Render (pr, TREEID, &pm[i], fDetail);
      }
   } // dwStart

   // call commit so that each metaball will be its own object for ray-tracing
   prs->Commit();

}



/****************************************************************************
CavePage
*/
BOOL CavePage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCIS pcis = (PCIS)pPage->m_pUserData;
   PCObjectCave pv = pcis->pv;
   PCMetaball pmb = pcis->pmb;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         PCEscControl pControl;

         WCHAR szTemp[64];

         CPoint pCenter, pLLT, pSize, pPower;
         fp fLOD, fStrength;
         pmb->ShapeGet (&pCenter, &pLLT, &pSize, &pPower, &fLOD, &fStrength);

         DWORD i;
         for (i = 0; i < 3; i++) {
            swprintf (szTemp, L"center%d", (int)i);
            MeasureToString (pPage, szTemp, pCenter.p[i], TRUE);

            swprintf (szTemp, L"size%d", (int)i);
            MeasureToString (pPage, szTemp, pSize.p[i], TRUE);

            swprintf (szTemp, L"llt%d", (int)i);
            AngleToControl (pPage, szTemp, pLLT.p[i], TRUE);

            swprintf (szTemp, L"power%d", (int)i);
            pControl = pPage->ControlFind (szTemp);
            if (pControl)
               pControl->AttribSetInt (Pos(), (int)(pPower.p[i] * 100.0));
         } // i

         for (i = 0; i < 4; i++) {
            swprintf (szTemp, L"noisedetail%d", (int)i);
            MeasureToString (pPage, szTemp, pv->m_pNoiseDetail.p[i], TRUE);

            swprintf (szTemp, L"noisestrength%d", (int)i);
            pControl = pPage->ControlFind (szTemp);
            if (pControl)
               pControl->AttribSetInt (Pos(), (int)(pv->m_pNoiseStrength.p[i] * 100.0));
         } // i

         MeasureToString (pPage, L"lod", fLOD, TRUE);
         DoubleToControl (pPage, L"seed", pv->m_dwSeed);
         DoubleToControl (pPage, L"strength", fStrength);

         if (pControl = pPage->ControlFind (L"seenfrominside"))
            pControl->AttribSetBOOL (Checked(), pv->m_fSeenFromInside);
         if (pControl = pPage->ControlFind (L"backface"))
            pControl->AttribSetBOOL (Checked(), pv->m_fBackface);
         if (pControl = pPage->ControlFind (L"boxes"))
            pControl->AttribSetBOOL (Checked(), pv->m_fBoxes);
         if (pControl = pPage->ControlFind (L"invisible"))
            pControl->AttribSetBOOL (Checked(), pmb->m_fInvisible);

         // disable deconstruct and delete buttons if thsi is the only one
         if (pv->m_lPCMetaball.Num() <= 1) {
            if (pControl = pPage->ControlFind (L"deconstruct"))
               pControl->Enable (FALSE);
            if (pControl = pPage->ControlFind (L"delete"))
               pControl->Enable (FALSE);
         }

         // disable delete texture if only one texture
         if (pv->m_lPCMetaSurface.Num() <= 1) {
            if (pControl = pPage->ControlFind (L"texturedelete"))
               pControl->Enable (FALSE);
         }

         ComboBoxSet (pPage, L"texture", pmb->m_dwMetaSurface);
      }
      break;


   case ESCN_SCROLL:
   case ESCN_SCROLLING:
      {
         PESCNSCROLL p = (PESCNSCROLL) pParam;

         // only do one scroll bar
         if (!p->pControl->m_pszName)
            break;

         fp fVal;
         fVal = (fp) p->pControl->AttribGetInt (Pos()) / 100.0;

         // power
         PWSTR pszPower = L"power", pszNoiseStrength = L"noisestrength";
         DWORD dwPowerLen = (DWORD)wcslen(pszPower), dwNoiseStrengthLen = (DWORD)wcslen(pszNoiseStrength);
         if (!wcsncmp (p->pControl->m_pszName, pszPower, dwPowerLen)) {
            DWORD dwDim = _wtoi (p->pControl->m_pszName + dwPowerLen);
            if (dwDim >= 3)
               break;   // dont know this one

            CPoint pCenter, pLLT, pSize, pPower;
            fp fLOD, fStrength;
            pmb->ShapeGet (&pCenter, &pLLT, &pSize, &pPower, &fLOD, &fStrength);

            pPower.p[dwDim] = fVal;

            pv->m_pWorld->ObjectAboutToChange (pv);
            pv->Dirty (pmb);
            pmb->ShapeSet (&pCenter, &pLLT, &pSize, &pPower, fLOD, fStrength);
            pv->Dirty (pmb);
            pv->m_pWorld->ObjectChanged (pv);

            return TRUE;
         } // power
         else if (!wcsncmp (p->pControl->m_pszName, pszNoiseStrength, dwNoiseStrengthLen)) {
            DWORD dwDim = _wtoi (p->pControl->m_pszName + dwNoiseStrengthLen);
            if (dwDim >= 4)
               break;   // dont know this one

            pv->m_pWorld->ObjectAboutToChange (pv);
            pv->m_pNoiseStrength.p[dwDim] = fVal;
            pv->Dirty (NULL);
            pv->m_pWorld->ObjectChanged (pv);

            return TRUE;
         } // power

      }
      break;

   case ESCN_COMBOBOXSELCHANGE:
      {
         PESCNCOMBOBOXSELCHANGE p = (PESCNCOMBOBOXSELCHANGE) pParam;

         if (!p->pControl || !p->pControl->m_pszName)
            break;
         if (_wcsicmp (p->pControl->m_pszName, L"texture"))
            break;   // only care about texture

         DWORD dwNum = p->pszName ? _wtoi(p->pszName) : 0;
         if (dwNum == pmb->m_dwMetaSurface)
            return TRUE;   // nothing changed

         // else, changed texture
         pv->m_pWorld->ObjectAboutToChange (pv);
         pmb->m_dwMetaSurface = dwNum;
         pv->Dirty (pmb);
         pv->m_pWorld->ObjectChanged (pv);
         return TRUE;

      }
      break;

   case ESCN_EDITCHANGE:
      {
         PESCNEDITCHANGE p = (PESCNEDITCHANGE) pParam;
         PWSTR psz = p->pControl->m_pszName;
         if (!psz)
            break;   // doesnt have name

         PCMetaball *ppm = (PCMetaball*)pv->m_lPCMetaball.Get(0);
         pv->m_pWorld->ObjectAboutToChange (pv);

         // always get the values for the specific metaball and change
         CPoint pCenter, pLLT, pSize, pPower;
         fp fLOD, fStrength;
         pmb->ShapeGet (&pCenter, &pLLT, &pSize, &pPower, &fLOD, &fStrength);

         WCHAR szTemp[64];

         DWORD i;
         for (i = 0; i < 3; i++) {
            swprintf (szTemp, L"center%d", (int)i);
            MeasureParseString (pPage, szTemp, &pCenter.p[i]);

            swprintf (szTemp, L"size%d", (int)i);
            MeasureParseString (pPage, szTemp, &pSize.p[i]);
            pSize.p[i] = max(pSize.p[i], CLOSE);

            swprintf (szTemp, L"llt%d", (int)i);
            pLLT.p[i] = AngleFromControl (pPage, szTemp);
         } // i
         fStrength = DoubleFromControl (pPage, L"strength");

         pv->Dirty (pmb);
         pmb->ShapeSet (&pCenter, &pLLT, &pSize, &pPower, fLOD, fStrength);
         pv->Dirty (pmb);

         // if the level of detail changed then change all LODs in all polygons
         PWSTR pszNoiseDetail = L"noisedetail";
         DWORD dwNoiseDetailLen = (DWORD)wcslen(pszNoiseDetail);
         if (!wcsncmp (psz, pszNoiseDetail, dwNoiseDetailLen)) {
            DWORD dwDim = _wtoi(psz + dwNoiseDetailLen);
            MeasureParseString (pPage, psz, &pv->m_pNoiseDetail.p[dwDim]);
            pv->m_pNoiseDetail.p[dwDim] = max(pv->m_pNoiseDetail.p[dwDim], CLOSE);

            pv->Dirty (NULL); // everything invalid
         }
         else if (!_wcsicmp(psz, L"lod")) {
            MeasureParseString (pPage, L"lod", &fLOD);
            fLOD = max(fLOD, 0.001);

            for (i = 0; i < pv->m_lPCMetaball.Num(); i++) {
               ppm[i]->ShapeGet (&pCenter, &pLLT, &pSize, &pPower, NULL, &fStrength);
               ppm[i]->ShapeSet (&pCenter, &pLLT, &pSize, &pPower, fLOD, fStrength);
            } // i
         }
         else if (!_wcsicmp(psz, L"seed")) {
            pv->m_dwSeed = (DWORD) DoubleFromControl (pPage, L"seed");

            pv->Dirty (NULL); // everything invalid
         }

         pv->m_pWorld->ObjectChanged (pv);
      }
      break;


   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;
         PWSTR psz;
         psz = p->pControl->m_pszName;

         if (!_wcsicmp (psz, L"seenfrominside")) {
            pv->m_pWorld->ObjectAboutToChange (pv);
            pv->m_fSeenFromInside = p->pControl->AttribGetBOOL (Checked());
            pv->Dirty (NULL);
            pv->m_pWorld->ObjectChanged (pv);
            return TRUE;
         }
         else if (!_wcsicmp (psz, L"backface")) {
            pv->m_pWorld->ObjectAboutToChange (pv);
            pv->m_fBackface = p->pControl->AttribGetBOOL (Checked());
            pv->m_pWorld->ObjectChanged (pv);
            return TRUE;
         }
         else if (!_wcsicmp (psz, L"boxes")) {
            pv->m_pWorld->ObjectAboutToChange (pv);
            pv->m_fBoxes = p->pControl->AttribGetBOOL (Checked());
            pv->m_pWorld->ObjectChanged (pv);
            return TRUE;
         }
         else if (!_wcsicmp (psz, L"invisible")) {
            pv->m_pWorld->ObjectAboutToChange (pv);
            pmb->m_fInvisible = p->pControl->AttribGetBOOL (Checked());
            pv->m_pWorld->ObjectChanged (pv);
            return TRUE;
         }
         else if (!_wcsicmp (psz, L"deconstruct") || !_wcsicmp (psz, L"delete")) {
            // find the index
            DWORD dwIndex;
            PCMetaball *ppmb = (PCMetaball*)pv->m_lPCMetaball.Get(0);
            for (dwIndex = 0; dwIndex < pv->m_lPCMetaball.Num(); dwIndex++)
               if (ppmb[dwIndex] == pmb)
                  break;
            if (dwIndex >= pv->m_lPCMetaball.Num())
               return TRUE;   // error, shouldnt happen

            if (!_wcsicmp (psz, L"deconstruct"))
               pv->DeconstructIndividual (pmb);

            // delete
            pv->m_pWorld->ObjectAboutToChange (pv);
            pv->DeleteMetaball (dwIndex);
            pv->m_pWorld->ObjectChanged (pv);

            // exit since metaball no longer exists
            pPage->Exit (Back());
            return TRUE;
         }
         else if (!_wcsicmp (psz, L"texturedelete")) {
            DWORD dwSurface = pmb->m_dwMetaSurface;
            PCMetaSurface *ppms = (PCMetaSurface*) pv->m_lPCMetaSurface.Get(0);
            if (dwSurface >= pv->m_lPCMetaSurface.Num())
               return TRUE;   // cant deelte this

            pv->m_pWorld->ObjectAboutToChange (pv);

            pv->ObjectSurfaceRemove (ppms[dwSurface]->m_dwRendSurface);
            delete ppms[dwSurface];
            pv->m_lPCMetaSurface.Remove (dwSurface);

            DWORD i;
            PCMetaball *ppmb = (PCMetaball*) pv->m_lPCMetaball.Get(0);
            for (i = 0; i < pv->m_lPCMetaball.Num(); i++) {
               if (ppmb[i]->m_dwMetaSurface < dwSurface)
                  continue;   // no change

               if (ppmb[i]->m_dwMetaSurface > dwSurface)
                  ppmb[i]->m_dwMetaSurface--;
               else
                  ppmb[i]->m_dwMetaSurface = 0; // default
               ppmb[i]->ShapeClear();  // since will need to recalc
            } // i
            pv->m_pWorld->ObjectChanged (pv);

            pPage->Exit (RedoSamePage());
            return TRUE;
         }
         else if (!_wcsicmp (psz, L"texturenew")) {
            // find a render surface that works
            DWORD dwRend;
            for (dwRend = 1; ; dwRend++)
               if (!pv->ObjectSurfaceFind (dwRend))
                  break;

            // create a new one
            PCMetaSurface pms = new CMetaSurface;
            if (!pms)
               return TRUE;

            pv->m_pWorld->ObjectAboutToChange (pv);
            pms->m_dwRendSurface = dwRend;
            pv->ObjectSurfaceAdd (dwRend, RGB(0xff,0xff,0), MATERIAL_PAINTSEMIGLOSS, NULL,
               &GTEXTURECODE_FlooringTiles, &GTEXTURESUB_FlooringTiles);
               // NOTE: Using brick so can see repeat
            pv->m_lPCMetaSurface.Add (&pms);

            // change this surface
            pmb->m_dwMetaSurface = pv->m_lPCMetaSurface.Num() - 1;
            pmb->ShapeClear();

            pv->m_pWorld->ObjectChanged (pv);

            pPage->Exit (RedoSamePage());
            return TRUE;
         }

      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Cave settings";
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"TEXTURE")) {
            MemZero (&gMemTemp);

            DWORD i;
            PCMetaSurface *ppms = (PCMetaSurface*)pv->m_lPCMetaSurface.Get(0);
            for (i = 0; i < pv->m_lPCMetaSurface.Num(); i++) {
               PCMetaSurface pms = ppms[i];

               PWSTR psz = (pms->m_szName[0]) ? pms->m_szName : L"Unnamed";

               MemCat (&gMemTemp, L"<elem name=");
               MemCat (&gMemTemp, (int)i);
               MemCat (&gMemTemp, L">");
               MemCatSanitize (&gMemTemp, psz);
               MemCat (&gMemTemp, L"</elem>");
            } // i

            p->pszSubString = (PWSTR)gMemTemp.p;
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}


/**********************************************************************************
CMetaball::DialogShow - Shows a dialog to edit this particular metaball as well
as the cave settings.

inputs
   PCEscWindow       pWindow - Window
   PCObjectCave      pCave - Cave that this is part of
returns
   BOOL - TRUE if user pressed back, FALSE if pressed cancel
*/
BOOL CMetaball::DialogShow (PCEscWindow pWindow, PCObjectCave pCave)
{
   PWSTR pszRet = NULL;
   CIS cis;
   memset (&cis, 0, sizeof(cis));
   cis.pmb = this;
   cis.pv = pCave;

redo:
   pszRet = pWindow->PageDialog (ghInstance, IDR_MMLCAVE, CavePage, &cis);
   if (!pszRet)
      return FALSE;
   if (!_wcsicmp(pszRet, RedoSamePage()))
      goto redo;
   else if (!_wcsicmp(pszRet, L"textureedit")) {
      if (m_dwMetaSurface >= pCave->m_lPCMetaSurface.Num())
         goto redo;  // just in case
      PCMetaSurface *ppms = (PCMetaSurface*) pCave->m_lPCMetaSurface.Get(0);
      
      if (ppms[m_dwMetaSurface]->DialogShow (pWindow, pCave, m_dwMetaSurface))
         goto redo;
      else
         return FALSE;
   }

   return !_wcsicmp(pszRet, Back());
}



/**********************************************************************************
CObjectCave::Constructor and destructor */
CObjectCave::CObjectCave (PVOID pParams, POSINFO pInfo)
{
   m_OSINFO = *pInfo;

   m_dwRenderShow = RENDERSHOW_WALLS;  // use walls, since not sure what else to use

   m_dwMetaSel = 0;
   m_dwSeed = 1234;
   m_fSeenFromInside = TRUE;
   m_fBackface = TRUE;
   m_fBoxes = TRUE;

   m_pNoiseStrength.Zero4();
   m_pNoiseDetail.p[0] = 2.0;
   m_pNoiseDetail.p[1] = 1;
   m_pNoiseDetail.p[2] = 0.5;
   m_pNoiseDetail.p[3] = 0.25;

   // surfaces
   m_lPCMetaSurface.Init (sizeof(PCMetaSurface));
   PCMetaSurface pms = new CMetaSurface;
   pms->m_dwRendSurface = 10; // default surface used below
   m_lPCMetaSurface.Add (&pms);

   // add one metaball
   m_lPCMetaball.Init (sizeof(PCMetaball));
   PCMetaball pNew = new CMetaball;
   pNew->m_dwMetaSurface = 0;
   m_lPCMetaball.Add (&pNew);

   // color for the box
   ObjectSurfaceAdd (10, RGB(0xc0,0xc0,0xc0), MATERIAL_PAINTSEMIGLOSS, NULL,
      &GTEXTURECODE_Rock, &GTEXTURESUB_Rock);
      // ID #10 correlates to value in CMetaSurface
}


CObjectCave::~CObjectCave (void)
{
   // clear out the metaballs
   PCMetaball *ppm = (PCMetaball*)m_lPCMetaball.Get(0);
   DWORD i;
   for (i = 0; i < m_lPCMetaball.Num(); i++)
      delete ppm[i];
   m_lPCMetaball.Clear();

   // clear out the metasurfaces
   PCMetaSurface *ppms = (PCMetaSurface*) m_lPCMetaSurface.Get(0);
   for (i = 0; i < m_lPCMetaSurface.Num(); i++)
      delete ppms[i];
   m_lPCMetaSurface.Clear();
}


/**********************************************************************************
CObjectCave::Delete - Called to delete this object
*/
void CObjectCave::Delete (void)
{
   delete this;
}


// METLOC - For sorting metaball location
typedef struct {
   fp             fDist;      // distance from the camera
   PCMetaball     pmb;        // metaball
} METLOC, *PMETLOC;

static int _cdecl METLOCCompare (const void *elem1, const void *elem2)
{
   METLOC *pdw1, *pdw2;
   pdw1 = (METLOC*) elem1;
   pdw2 = (METLOC*) elem2;

   if (pdw1->fDist < pdw2->fDist)
      return -1;
   else if (pdw1->fDist > pdw2->fDist)
      return 1;
   else
      return 0;
}

/**********************************************************************************
CObjectCave::Render - Draws a test box.

inputs
   POBJECTRENDER     pr - Render information
*/
void CObjectCave::Render (POBJECTRENDER pr, DWORD dwSubObject)
{
   // create the surface render object and draw
   // CRenderSurface rs;
   m_Renderrs.ClearAll();

   CMatrix mObject;
   m_Renderrs.Init (pr->pRS);
   ObjectMatrixGet (&mObject);
   m_Renderrs.Multiply (&mObject);



   // make sure all the metaballs are calculated
   CalcIfNecessary();

   // get the surfaces
   PCMetaSurface *ppms = (PCMetaSurface*)m_lPCMetaSurface.Get(0);
   DWORD dwNumMetaSurface = m_lPCMetaSurface.Num();
   PCMetaball *papMeta = (PCMetaball*) m_lPCMetaball.Get(0);
   DWORD dwNumMeta = m_lPCMetaball.Num();

   // create a list of metaballs with their distances so draw the closest metaballs first
   CListFixed lMETLOC;
   DWORD i;
   lMETLOC.Init (sizeof(METLOC));
   METLOC ml;
   CPoint pDist;
   CPoint pCamera;
   if (pr->fCameraValid) {
      // start with camera in world
      pCamera.Copy (&pr->pCamera);
      pCamera.p[3] = 1;

      // convert to object
      CMatrix mWorldToObject;
      m_MatrixObject.Invert4 (&mWorldToObject);
      pCamera.MultiplyLeft (&mWorldToObject);
   }
   else
      pCamera.Zero();

   if (dwSubObject != (DWORD)-1) {
      if (dwSubObject < dwNumMeta) {
         ml.fDist = 0;
         ml.pmb = papMeta[dwSubObject];
         lMETLOC.Add (&ml);
      }
   }
   else // add all subobjects
      lMETLOC.Required (dwNumMeta);
      for (i = 0; i < dwNumMeta; i++) {
         if (pr->fCameraValid) {
            papMeta[i]->ShapeGet (&pDist, NULL, NULL, NULL, NULL, NULL);
            pDist.Subtract (&pCamera);
            ml.fDist = pDist.Length();
         }
         else
            ml.fDist = 0;
         ml.pmb = papMeta[i];

         lMETLOC.Add (&ml);
      } // i
   qsort (lMETLOC.Get(0), lMETLOC.Num(), sizeof(METLOC), METLOCCompare);


   // draw the trees
   BOOL fForestBoxes;
   fForestBoxes = m_fBoxes && (pr->dwReason != ORREASON_FINAL) && (pr->dwReason != ORREASON_SHADOWS);
      // BUGFIX - If calculating shadows then DONT want forest boxes

   // render
   PMETLOC pml = (PMETLOC)lMETLOC.Get(0);
   CPoint pMin, pMax;
   fp fDetail;
   for (i = 0; i < lMETLOC.Num(); i++) {
      PCMetaball pmb = pml[i].pmb;

      if (dwSubObject == (DWORD)-1) {
         // See if should be clipped straiged out
         pmb->BoundingBoxGet (&pMin, &pMax);
         if (!pr->pRS->QuerySubDetail (&m_MatrixObject, &pMin, &pMax, &fDetail))
            continue;
      }
      else
         fDetail = pr->pRS->QueryDetail ();

      DWORD dwSurface = pmb->m_dwMetaSurface;

      // if it's invisible don't bother
      if (pmb->m_fInvisible) {
         if (pr->dwReason != ORREASON_WORKING)
            continue;   // dont draw for final

         // else, set red
         RENDERSURFACE ren;
         memset (&ren, 0, sizeof(ren));
         ren.Material.InitFromID (MATERIAL_PAINTMATTE);
         ren.wMinorID = (WORD)dwSurface;
         m_Renderrs.SetDefMaterial (&ren);
         m_Renderrs.SetDefColor (RGB(0xff, 0, 0));
      }
      else {
         // get the surface to use
         if (dwSurface < dwNumMetaSurface)
            m_Renderrs.SetDefMaterial (m_OSINFO.dwRenderShard, ObjectSurfaceFind (ppms[dwSurface]->m_dwRendSurface), m_pWorld);
      }

      pmb->Render (pr, &m_Renderrs,
         (pr->dwReason == ORREASON_SHADOWS) ? FALSE : m_fBackface,   // BUGFIX - dont backface for shadows
         fForestBoxes, ppms[dwSurface], this, fDetail);
   }

   m_Renderrs.Commit();
}


/**********************************************************************************
CObjectCave::CalcIfNecessary - Sees if any elements are the metaball are dirty.
If so, they're recalculated
*/
#define NOISEPOINTS     32       // noise is 32 x 32 x 32 values
void CObjectCave::CalcIfNecessary (void)
{
   // see if anything is dirty
   PCMetaball *papMeta = (PCMetaball*) m_lPCMetaball.Get(0);
   DWORD dwNumMeta = m_lPCMetaball.Num();
   CListFixed lMetaToCalc;
   lMetaToCalc.Init (sizeof(DWORD));
   DWORD i;
   lMetaToCalc.Required (dwNumMeta);
   for (i = 0; i < dwNumMeta; i++)
      if (!papMeta[i]->IsValidGet())
         lMetaToCalc.Add (&i);
   if (!lMetaToCalc.Num())
      return;  // nothing dirty

   // initialize noise
   CNoise3D aNoise[3];
   for (i = 0; i < 4; i++)
      if (m_pNoiseStrength.p[i])
         break;
   if (i < 4) {
      // have noise, need to initialize
      srand (m_dwSeed);
      for (i = 0; i < 3; i++)
         aNoise[i].Init (NOISEPOINTS, NOISEPOINTS, NOISEPOINTS);
   }


   DWORD dwRenderShard = m_OSINFO.dwRenderShard;

   DWORD *padw = (DWORD*)lMetaToCalc.Get(0);
   PCMetaSurface *ppms = (PCMetaSurface*)m_lPCMetaSurface.Get(0);
   for (i = 0; i < lMetaToCalc.Num(); i++) {
      // calculate the polygons
      DWORD dwIndex = padw[i];
      papMeta[dwIndex]->CalcPolygons (dwNumMeta, papMeta, dwIndex, m_fSeenFromInside);

      // perterb location based on noise functions
      papMeta[dwIndex]->CalcNoise (dwRenderShard, NOISEPOINTS, aNoise, &m_pNoiseStrength, &m_pNoiseDetail,
         ppms[(papMeta[dwIndex]->m_dwMetaSurface < m_lPCMetaSurface.Num()) ? papMeta[dwIndex]->m_dwMetaSurface : 0],
         m_fSeenFromInside);

      // calculate the normals for each polygon
      papMeta[dwIndex]->CalcPolygonNormals ();

      // clear objects
      papMeta[dwIndex]->ClearObjects ();
   }

   // go through all the other metaballs and clear any references to stalagmites
   // from this one
   for (i = 0; i < m_lPCMetaball.Num(); i++)
      papMeta[i]->ClearObjectsFromMetaballs (padw, lMetaToCalc.Num());

   for (i = 0; i < lMetaToCalc.Num(); i++) {
      // calculate the vertex normals
      DWORD dwIndex = padw[i];
      papMeta[dwIndex]->CalcVertexNormals (dwNumMeta, papMeta);
   }

   for (i = 0; i < lMetaToCalc.Num(); i++) {
      DWORD dwIndex = padw[i];

      // calculate the min and max locations of the polygons
      papMeta[dwIndex]->CalcPolyMinMax ();
   }

   // BUGFIX - make sure cloned objects are created before call the srand
   DWORD j;
   for (i = 0; i < m_lPCMetaSurface.Num(); i++)
      for (j = 0; j < NUMCAVECANOPY; j++)
         ppms[i]->m_apCaveCanopy[j]->MaxTreeSize (dwRenderShard);

   for (i = 0; i < lMetaToCalc.Num(); i++) {
      // calculate the vertex normals
      DWORD dwIndex = padw[i];

      // reset the random
      srand (m_dwSeed + i*100);

      // calculate the objcts
      papMeta[dwIndex]->CalcObjects (dwRenderShard,
         ppms[(papMeta[dwIndex]->m_dwMetaSurface < m_lPCMetaSurface.Num()) ? papMeta[dwIndex]->m_dwMetaSurface : 0],
         papMeta[dwIndex]->m_dwMetaSurface,
         m_fSeenFromInside,
         dwIndex, this);
   }

   // go back through and sort the sub-objects for faster render
   for (i = 0; i < lMetaToCalc.Num(); i++) {
      DWORD dwIndex = padw[i];
      papMeta[dwIndex]->CalcObjectsSort ();
   }

}

/**********************************************************************************
CObjectCave::Clone - Clones this

inputs
   none
returns
   PCObjectSocket - Clone
*/
PCObjectSocket CObjectCave::Clone (void)
{
   PCObjectCave pNew;

   pNew = new CObjectCave(NULL, &m_OSINFO);

   // clone template info
   CloneTemplate(pNew);

   // Clone member variables
   pNew->m_dwMetaSel = m_dwMetaSel;
   pNew->m_pNoiseStrength.Copy (&m_pNoiseStrength);
   pNew->m_pNoiseDetail.Copy (&m_pNoiseDetail);
   pNew->m_dwSeed = m_dwSeed;
   pNew->m_fSeenFromInside = m_fSeenFromInside;
   pNew->m_fBackface = m_fBackface;
   pNew->m_fBoxes = m_fBoxes;

   DWORD i;

   // clone the metaballs
   // clear out the metaballs
   PCMetaball *ppm = (PCMetaball*)pNew->m_lPCMetaball.Get(0);
   for (i = 0; i < pNew->m_lPCMetaball.Num(); i++)
      delete ppm[i];
   pNew->m_lPCMetaball.Clear();
   pNew->m_lPCMetaball.Init (sizeof(PCMetaball), m_lPCMetaball.Get(0), m_lPCMetaball.Num());
   ppm = (PCMetaball*)pNew->m_lPCMetaball.Get(0);
   for (i = 0; i < pNew->m_lPCMetaball.Num(); i++)
      ppm[i] = ppm[i]->Clone();

   // clear out the metasurfaces
   PCMetaSurface *ppms = (PCMetaSurface*) pNew->m_lPCMetaSurface.Get(0);
   for (i = 0; i < pNew->m_lPCMetaSurface.Num(); i++)
      delete ppms[i];
   pNew->m_lPCMetaSurface.Init (sizeof(PCMetaSurface), m_lPCMetaSurface.Get(0), m_lPCMetaSurface.Num());
   ppms = (PCMetaSurface*)pNew->m_lPCMetaSurface.Get(0);
   for (i = 0; i < pNew->m_lPCMetaSurface.Num(); i++)
      ppms[i] = ppms[i]->Clone();

   return pNew;
}



static PWSTR gpszMetaSel = L"MetaSel";
static PWSTR gpszSeed = L"Seed";
static PWSTR gpszSeenFromInside = L"SeenFromInside";
static PWSTR gpszBackface = L"Backface";
static PWSTR gpszBoxes = L"Boxes";
static PWSTR gpszNoiseStrength = L"NoiseStrength";
static PWSTR gpszNoiseDetail = L"NoiseDetail";

PCMMLNode2 CObjectCave::MMLTo (void)
{
   // call into the template first
   PCMMLNode2 pNode = MMLToTemplate();
   if (!pNode)
      return NULL;

   // member variables go here
   MMLValueSet (pNode, gpszMetaSel, (int)m_dwMetaSel);
   MMLValueSet (pNode, gpszSeed, (int)m_dwSeed);
   MMLValueSet (pNode, gpszSeenFromInside, (int)m_fSeenFromInside);
   MMLValueSet (pNode, gpszBackface, (int)m_fBackface);
   MMLValueSet (pNode, gpszBoxes, (int)m_fBoxes);
   MMLValueSet (pNode, gpszNoiseStrength, &m_pNoiseStrength);
   MMLValueSet (pNode, gpszNoiseDetail, &m_pNoiseDetail);

   // write all the metaballs
   PCMetaball *ppm = (PCMetaball*)m_lPCMetaball.Get(0);
   DWORD i;
   for (i = 0; i < m_lPCMetaball.Num(); i++) {
      PCMMLNode2 pSub = ppm[i]->MMLTo ();
      if (pSub)
         pNode->ContentAdd (pSub);
   } // i

   // write out the metasurfaces
   PCMetaSurface *ppms = (PCMetaSurface*)m_lPCMetaSurface.Get(0);
   for (i = 0; i < m_lPCMetaSurface.Num(); i++) {
      PCMMLNode2 pSub = ppms[i]->MMLTo ();
      if (pSub)
         pNode->ContentAdd (pSub);
   } // i

   return pNode;
}

BOOL CObjectCave::MMLFrom (PCMMLNode2 pNode)
{
   // call into the template first
   if (!MMLFromTemplate (pNode))
      return FALSE;

   // clear out the metaballs
   PCMetaball *ppm = (PCMetaball*)m_lPCMetaball.Get(0);
   DWORD i;
   for (i = 0; i < m_lPCMetaball.Num(); i++)
      delete ppm[i];
   m_lPCMetaball.Clear();

   // clear out the metasurfaces
   PCMetaSurface *ppms = (PCMetaSurface*) m_lPCMetaSurface.Get(0);
   for (i = 0; i < m_lPCMetaSurface.Num(); i++)
      delete ppms[i];
   m_lPCMetaSurface.Clear();

   // member variables go here
   m_dwMetaSel = (DWORD)MMLValueGetInt (pNode, gpszMetaSel, 0);
   m_dwSeed = (DWORD)MMLValueGetInt (pNode, gpszSeed, 1234);
   m_fSeenFromInside = (BOOL)MMLValueGetInt (pNode, gpszSeenFromInside, TRUE);
   m_fBackface = (BOOL)MMLValueGetInt (pNode, gpszBackface, TRUE);
   m_fBoxes = (BOOL)MMLValueGetInt (pNode, gpszBoxes, TRUE);
   MMLValueGetPoint (pNode, gpszNoiseStrength, &m_pNoiseStrength);
   MMLValueGetPoint (pNode, gpszNoiseDetail, &m_pNoiseDetail);

   // load in metaballs
   PCMMLNode2 pSub;
   PWSTR psz;
   DWORD dwCurCanopy = 0;
   for (i = 0; i < pNode->ContentNum(); i++) {
      pSub = NULL;
      pNode->ContentEnum (i, &psz, &pSub);
      if (!pSub)
         continue;
      psz = pSub->NameGet();
      if (!psz)
         continue;

      if (!_wcsicmp (psz, gpszMetaball)) {
         PCMetaball pNew = new CMetaball;
         if (!pNew)
            continue;
         if (!pNew->MMLFrom (pSub)) {
            delete pNew;
            continue;
         }
         m_lPCMetaball.Add (&pNew);
      } // i
      else if (!_wcsicmp (psz, gpszMetaSurface)) {
         PCMetaSurface pNew = new CMetaSurface;
         if (!pNew)
            continue;
         if (!pNew->MMLFrom (pSub)) {
            delete pNew;
            continue;
         }
         m_lPCMetaSurface.Add (&pNew);
      } // i
   } // i

   return TRUE;
}

/*************************************************************************************
CObjectCave::ControlPointQuery - Called to query the information about a control
point identified by dwID.

inputs
   DWORD       dwID - ID
   POSCONTROL  pInfo - Filled in with the infomration
returns
   BOOL - TRUE if successful
*/
BOOL CObjectCave::ControlPointQuery (DWORD dwID, POSCONTROL pInfo)
{
   if (! ((dwID < 5) || ((dwID >= 10) && (dwID < 10 + m_lPCMetaball.Num()))) )
      return FALSE;

   // make sure selection within range
   if (m_dwMetaSel >= m_lPCMetaball.Num())
      m_dwMetaSel = 0;
   PCMetaball *ppm = (PCMetaball*)m_lPCMetaball.Get(0);
   PCMetaball pm;

   memset (pInfo,0, sizeof(*pInfo));

   pInfo->dwID = dwID;

   if ((dwID >= 10) && (dwID < 10 + m_lPCMetaball.Num())) {
      pm = ppm[dwID-10];
      pInfo->dwStyle = CPSTYLE_POINTER;
      pInfo->cColor = RGB(0xff,0xff,0);

      pInfo->pLocation.Zero();
      pm->SphereToObject (&pInfo->pLocation);

      wcscpy (pInfo->szName, L"Click to select");
      pInfo->fButton = TRUE;
   }
   else {
      pm = ppm[m_dwMetaSel];

      switch (dwID) {
         case 0:  // move location
            pInfo->dwStyle = CPSTYLE_SPHERE;
            pInfo->cColor = RGB(0xff,0,0xff);

            pInfo->pLocation.Zero();
            pm->SphereToObject (&pInfo->pLocation);

            wcscpy (pInfo->szName, L"Move");
            break;

         case 1:  // overall size
            pInfo->dwStyle = CPSTYLE_CUBE;
            pInfo->cColor = RGB(0,0xff,0);

            pInfo->pLocation.Zero();
            pInfo->pLocation.p[0] = pInfo->pLocation.p[1] = pInfo->pLocation.p[2] = 1;
            pm->SphereToObject (&pInfo->pLocation);

            wcscpy (pInfo->szName, L"Size");
            break;

         case 2:  // orientation X
         case 3:  // orientation Y
         case 4:  // orientation Z
            pInfo->dwStyle = CPSTYLE_POINTER;
            pInfo->cColor = RGB(0,0,0xff);

            pInfo->pLocation.Zero();
            pInfo->pLocation.p[dwID-2] = 1;
            pm->SphereToObject (&pInfo->pLocation);

            wcscpy (pInfo->szName, L"Orientation");
            break;
      }
   }



   // get size
   CPoint pSize;
   pm->ShapeGet (NULL, NULL, &pSize, NULL, NULL, NULL);
   pInfo->fSize = pSize.Length() / 40;


   return TRUE;
}


/*************************************************************************************
CObjectCave::ControlPointEnum - Called to enumerate a list of control point IDs
into the list.

inputs
   PCListFixed       plDWORD - Should be filled in with control points
returns
   none
*/
void CObjectCave::ControlPointEnum (PCListFixed plDWORD)
{
   // make sure selection within range
   if (m_dwMetaSel >= m_lPCMetaball.Num())
      m_dwMetaSel = 0;

   // 1 control points starting at 10
   DWORD i;
   // only have central location if more than one metaball
   if (m_lPCMetaball.Num() > 1) {
      i = 0;
      plDWORD->Add(&i);
   }
   for (i = 1; i < 5; i++)
      plDWORD->Add (&i);

   // other metaballs
   for (i = 10; i < 10 + m_lPCMetaball.Num(); i++)
      if (i != 10 + m_dwMetaSel)
         plDWORD->Add (&i);

}


/*************************************************************************************
CObjectCave::ControlPointSet - Called to change the valud of a control point.

inputs
   DWORD       dwID - ID
   PCPoint     pVal - Contains the new location, in object coordinates
returns
   BOOL - TRUE if successful
*/
BOOL CObjectCave::ControlPointSet (DWORD dwID, PCPoint pVal, PCPoint pViewer)
{
   if (! ((dwID < 5) || ((dwID >= 10) && (dwID < 10 + m_lPCMetaball.Num()))) )
      return FALSE;

   // make sure selection within range
   if (m_dwMetaSel >= m_lPCMetaball.Num())
      m_dwMetaSel = 0;
   PCMetaball *ppm = (PCMetaball*)m_lPCMetaball.Get(0);
   PCMetaball pm;

   // tell the world we're about to change
   if (m_pWorld)
      m_pWorld->ObjectAboutToChange (this);

   DWORD i;

   if ((dwID >= 10) && (dwID < 10 + m_lPCMetaball.Num())) {
      pm = ppm[dwID-10];
      m_dwMetaSel = dwID - 10;
   }
   else {
      pm = ppm[m_dwMetaSel];
      CPoint pCenter, pLLT, pSize, pPower;
      fp fLOD, fStrength;
      short aiBoundary[2][3];
      pm->ShapeGet (&pCenter, &pLLT, &pSize, &pPower, &fLOD, &fStrength);

      // set this to dirty
      Dirty (pm);

      DWORD dwDim;
      CMatrix mRot;
      CPoint pTemp;
      switch (dwID) {
         case 0:  // move location
            pCenter.Copy (pVal);
            break;

         case 1:  // overall size
            pTemp.Copy (pVal);
            pm->ObjectToSphere (&pTemp);

            for (i = 0; i < 3; i++)
               pSize.p[i] = max(pSize.p[i] * pTemp.p[i], CLOSE);
            break;

         case 2:  // orientation X
         case 3:  // orientation Y
         case 4:  // orientation Z
            CPoint ap[3];

            dwDim = dwID - 2;

            for (i = 0; i < 3; i++) {
               if (i != dwDim) {
                  ap[i].Zero();
                  ap[i].p[i] = 1;
                  pm->SphereToObject (&ap[i]);
               }
               else
                  ap[i].Copy (pVal);

               ap[i].Subtract (&pCenter);
               ap[i].Normalize();
               if (ap[i].Length() < 0.5)
                  goto err;
            }

            // make square
            DWORD dwA = (dwDim + 1) % 3;
            DWORD dwB = (dwDim + 2) % 3;
            ap[dwB].CrossProd (&ap[dwDim], &ap[dwA]);
            ap[dwB].Normalize();
            if (ap[dwB].Length() < 0.5)
               goto err;

            ap[dwA].CrossProd (&ap[dwB], &ap[dwDim]);
            ap[dwA].Normalize();
            if (ap[dwA].Length() < 0.5)
               goto err;

            mRot.RotationFromVectors (&ap[0], &ap[1], &ap[2]);
            mRot.ToXYZLLT (&pTemp, &pLLT.p[2], &pLLT.p[0], &pLLT.p[1]);

            break;
      } // switch

      // set 
      pm->ShapeSet (&pCenter, &pLLT, &pSize, &pPower, fLOD, fStrength);

err:
      // set new location to dirty
      pm->BoundingBoxGet (&aiBoundary[0][0], &aiBoundary[1][0]);
      Dirty (pm);

   }


   // tell the world we've changed
   if (m_pWorld)
      m_pWorld->ObjectChanged (this);

   return TRUE;
}


/*************************************************************************************
CObjectCave::Dirty - This dirties a region of the metaball.

inputs
   short       *paiMin - Minimum area to dirty, array of 3 shorts. If both NULL then dirty everything
   short       *paiMax - Maximum area to dirty, array of 3 shorts. If both NULL then dirty everything
*/
void CObjectCave::Dirty (short *paiMin, short *paiMax)
{
   PCMetaball *ppm = (PCMetaball*)m_lPCMetaball.Get(0);
   DWORD i, j;
   short aiRange[2][3];
   for (i = 0; i < m_lPCMetaball.Num(); i++) {
      if (!paiMin || !paiMax) {
         ppm[i]->ShapeClear ();
         continue;
      }

      ppm[i]->BoundingBoxGet (&aiRange[0][0], &aiRange[1][0]);

      for (j = 0; j < 3; j++) {
         if (paiMin[j] >= aiRange[1][j])
            break;
         if (paiMax[j] <= aiRange[0][j])
            break;
      }
      if (j >= 3)
         ppm[i]->ShapeClear ();
   } // i
}

/*************************************************************************************
CObjectCave::Dirty - This dirties a region of the metaball.

inputs
   PCMetaball        pmb - Calls dirty based on a metaball. If NULL then clear everything
*/
void CObjectCave::Dirty (PCMetaball pmb)
{
   if (!pmb) {
      Dirty (NULL, NULL);
      return;
   }

   short as[2][3];
   pmb->BoundingBoxGet (&as[0][0], &as[1][0]);
   Dirty (&as[0][0], &as[1][0]);
}



/**********************************************************************************
CObjectCave::DialogShow - Causes the object to show a dialog box that allows
different attributes to be changed.

inputs
   DWORD             dwSuface - Surface that was clicked on. 0 if unknown.
   PCEscWindow       pWindow - Escarpment window to draw it in
returns
   BOOL - TRUE if user pressed "Back", FALSE if user pressed "Close"
*/
BOOL CObjectCave::DialogShow (DWORD dwSurface, PCEscWindow pWindow, PCPoint pClick)
{
   // find the closest metaball
   DWORD dwClosest = 0;
   fp fClosestDist = 1000000000;
   fp f;
   PCMetaball *ppm = (PCMetaball*)m_lPCMetaball.Get(0);
   DWORD i;
   if (pClick) for (i = 0; i < m_lPCMetaball.Num(); i++) {
      f = ppm[i]->Distance (pClick);
      if (f < fClosestDist) {
         dwClosest = i;
         fClosestDist = f;
      }
   }

   return ppm[dwClosest]->DialogShow (pWindow, this);
}


/**********************************************************************************
CObjectCave::DialogQuery - Returns TRUE if the object supports a custom
dialog box, FALSE if it doesn't. The default object doesn't
*/
BOOL CObjectCave::DialogQuery (void)
{
   return TRUE;
}

/**********************************************************************************
CObjectCave::QuerySubObjects - Standard API
*/
DWORD CObjectCave::QuerySubObjects (void)
{
   CalcIfNecessary();

   return m_lPCMetaball.Num();
}

/**********************************************************************************
CObjectCave::QueryBoundingBox - Standard API
*/
void CObjectCave::QueryBoundingBox (PCPoint pCorner1, PCPoint pCorner2, DWORD dwSubObject)
{
   // override this so bounding box is quicker
   CalcIfNecessary();

   PCMetaball *ppm = (PCMetaball*)m_lPCMetaball.Get(0);

   if (dwSubObject != (DWORD)-1) {
      if (dwSubObject >= m_lPCMetaball.Num())
         return;  // error

      if (!ppm[dwSubObject]->BoundingBoxGet (pCorner1, pCorner2)) {
         pCorner1->Zero();
         pCorner2->Zero();
      }

      return;
   }

   // else, usual
   DWORD i;
   CPoint pMin, pMax;
   BOOL fValueSet = FALSE;
   pCorner1->Zero();
   pCorner2->Zero();
   for (i = 0; i < m_lPCMetaball.Num(); i++) {
      if (!ppm[i]->BoundingBoxGet (&pMin, &pMax))
         continue;   // no bounding box

      if (fValueSet) {
         pCorner1->Min (&pMin);
         pCorner2->Max (&pMax);
      }
      else {
         pCorner1->Copy (&pMin);
         pCorner2->Copy (&pMax);
         fValueSet = TRUE;
      }
   }
}



/*************************************************************************************
CObjectCave::DeleteMetaball - Deletes a metaball from this object.

inputs
   DWORD          dwIndex - Metaball index
returns
   BOOL - TRUE if success
*/
BOOL CObjectCave::DeleteMetaball (DWORD dwIndex)
{
   // make sure index is ok
   if (dwIndex >= m_lPCMetaball.Num())
      return FALSE;

   // see if it's the only one that uses the surface
   PCMetaball *ppm = (PCMetaball*)m_lPCMetaball.Get(0);
   PCMetaball pmb = ppm[dwIndex];
   DWORD dwSurface = pmb->m_dwMetaSurface;

   // mark the other bits dirty
   Dirty (pmb);

   DWORD i;
   for (i = 0; i < m_lPCMetaball.Num(); i++) {
      if (i == dwIndex)
         continue;
      if (ppm[i]->m_dwMetaSurface == dwSurface)
         break;
   }
   if (i >= m_lPCMetaball.Num()) {
      // delete the surface
      PCMetaSurface *ppms = (PCMetaSurface*)m_lPCMetaSurface.Get(0);
      PCMetaSurface pms = ppms[pmb->m_dwMetaSurface];

      ObjectSurfaceRemove (pms->m_dwRendSurface);
      delete pms;
      m_lPCMetaSurface.Remove (pmb->m_dwMetaSurface);
         // BUGFIX - was dwIndex

      for (i = 0; i < m_lPCMetaball.Num(); i++)
         if (ppm[i]->m_dwMetaSurface >= dwSurface)
            ppm[i]->m_dwMetaSurface--;
   }

   delete pmb;
   m_lPCMetaball.Remove (dwIndex);

   return TRUE;
}


/*************************************************************************************
CObjectCave::DeconstructIndividual - The creates a copy of an individual metaball.

inputs
   PCMetaball     pmb - Invididual metaball
returns
   BOOL - TRUE if created
*/
BOOL CObjectCave::DeconstructIndividual (PCMetaball pmb)
{
   // create a new one
   PCObjectCave pCave;
   OSINFO OI;
   DWORD i;
   memset (&OI, 0, sizeof(OI));
   InfoGet (&OI);
   pCave = new CObjectCave((PVOID) 0, &OI);
   if (!pCave)
      return FALSE;

   // BUGFIX - Set a name
   PWSTR psz = StringGet (OSSTRING_NAME);
   if (psz)
      pCave->StringSet (OSSTRING_NAME, psz);

   // make sure to copy over some settings
   pCave->m_pNoiseStrength.Copy (&m_pNoiseStrength);
   pCave->m_pNoiseDetail.Copy (&m_pNoiseDetail);
   pCave->m_dwSeed = m_dwSeed;
   pCave->m_fSeenFromInside = m_fSeenFromInside;
   pCave->m_fBackface = m_fBackface;
   pCave->m_fBoxes = m_fBoxes;

   m_pWorld->ObjectAdd (pCave);

   m_pWorld->ObjectAboutToChange (pCave);

   // remove all the metaballs and surfaces in the cave
   PCMetaball *ppm = (PCMetaball*)pCave->m_lPCMetaball.Get(0);
   for (i = 0; i < pCave->m_lPCMetaball.Num(); i++)
      delete ppm[i];
   pCave->m_lPCMetaball.Clear();
   PCMetaSurface *ppms = (PCMetaSurface*)pCave->m_lPCMetaSurface.Get(0);
   for (i = 0; i < pCave->m_lPCMetaSurface.Num(); i++) {
      pCave->ObjectSurfaceRemove (ppms[i]->m_dwRendSurface);
      delete ppms[i];
   }
   pCave->m_lPCMetaSurface.Clear();

   // copy over the metaball
   pmb = pmb->Clone();
   if (!pmb)
      return FALSE;
   pCave->m_lPCMetaball.Add (&pmb);

   // copy over the metaball's surface
   ppms = (PCMetaSurface*) m_lPCMetaSurface.Get(0);
   PCMetaSurface pms = ppms[pmb->m_dwMetaSurface];
   pms = pms->Clone();
   pCave->m_lPCMetaSurface.Add (&pms);
   pmb->m_dwMetaSurface = 0;

   // copy over the texture
   pCave->ObjectSurfaceAdd (ObjectSurfaceFind (pms->m_dwRendSurface));

   // set the matrix to the old object's matrix, plus the new
   // translation and rotation
   // NOTE: This may cause problems with stalagmites hanging down at the wrong angle
   CMatrix mTransRot;
   CPoint pCenter, pLLT, pSize, pPower;
   fp fLOD, fStrength;
   pmb->ShapeGet (&pCenter, &pLLT, &pSize, &pPower, &fLOD, &fStrength);
   mTransRot.FromXYZLLT (&pCenter, pLLT.p[2], pLLT.p[0], pLLT.p[1]);
   pCenter.Zero();
   pLLT.Zero();
   pmb->ShapeSet (&pCenter, &pLLT, &pSize, &pPower, fLOD, fStrength);
   mTransRot.MultiplyRight (&m_MatrixObject);
   pCave->ObjectMatrixSet (&mTransRot);


   // note changed
   m_pWorld->ObjectChanged (pCave);

   return TRUE;
}


/*************************************************************************************
CObjectCave::Deconstruct -
Tells the object to deconstruct itself into sub-objects.
Basically, new objects will be added that exactly mimic this object,
and any embedeeding objects will be moved to the new ones.
NOTE: This old one will not be deleted - the called of Deconstruct()
will need to call Delete()
If fAct is FALSE the function is just a query, that returns
TRUE if the object cares about adjustment and can try, FALSE if it can't.

NOTE: Often overridden.
*/
BOOL CObjectCave::Deconstruct (BOOL fAct)
{
   // if there is only one metaball then can't deconstruct
   if (m_lPCMetaball.Num() <= 1)
      return FALSE;

   if (!m_pWorld)
      return FALSE;
   if (!fAct)
      return TRUE;

   // loop through all the metaballs... create them and clone them
   DWORD i;
   PCMetaball *ppm = (PCMetaball*)m_lPCMetaball.Get(0);
   for (i = 0; i < m_lPCMetaball.Num(); i++)
      DeconstructIndividual (ppm[i]);
   
   return TRUE;
}



/*****************************************************************************************
CObjectCave::MergeIndividual - Merges the metaballs from an individual into this object.
It doesn't delete the other object.

inputs
   PCObjectCave         pMerge - Cave to merge in with this
returns
   BOOL - TRUE if success
*/
BOOL CObjectCave::MergeIndividual (PCObjectCave pMerge)
{
   // inform that changed
   m_pWorld->ObjectAboutToChange (this);

   // if this is a single metaball, the reorient the object so that the m_ObejectMatrix
   // is level, while the metaball is angled
   if (m_lPCMetaball.Num() == 1) {
      PCMetaball *ppmb = (PCMetaball*)m_lPCMetaball.Get(0);
      PCMetaball pmb = ppmb[0];

      CMatrix mTransRot;
      CPoint pCenter, pLLT, pSize, pPower;
      fp fLOD, fStrength;
      pmb->ShapeGet (&pCenter, &pLLT, &pSize, &pPower, &fLOD, &fStrength);
      mTransRot.FromXYZLLT (&pCenter, pLLT.p[2], pLLT.p[0], pLLT.p[1]);
      mTransRot.MultiplyRight (&m_MatrixObject);
      mTransRot.ToXYZLLT (&pCenter, &pLLT.p[2], &pLLT.p[0], &pLLT.p[1]);
      mTransRot.Translation (pCenter.p[0], pCenter.p[1], pCenter.p[2]); // so no rotation in object
      ObjectMatrixSet (&mTransRot);
      pCenter.Zero();
      pmb->ShapeSet (&pCenter, &pLLT, &pSize, &pPower, fLOD, fStrength);
   }
   
   // go through all the surfaces in the merged-in metaball and make sure have an
   // analogy, or add
   CListFixed lRemap;
   lRemap.Init (sizeof(DWORD));
   PCMetaSurface *ppmsMerge = (PCMetaSurface*) pMerge->m_lPCMetaSurface.Get(0);
   DWORD i, j, dwSurface;
   for (i = 0; i < pMerge->m_lPCMetaSurface.Num(); i++) {
      PCMetaSurface pmsMerge = ppmsMerge[i];

      // find a match
      PCMetaSurface *ppmsThis = (PCMetaSurface*) m_lPCMetaSurface.Get(0);
      for (j = 0; j < m_lPCMetaSurface.Num(); j++)
         if (ppmsThis[j]->AreEqual (this, pmsMerge, pMerge))
            break;
      if (j < m_lPCMetaSurface.Num()) {
         // found a match
         lRemap.Add (&j);
         break;
      }

      // else, add a new one

      // first, find a surface that can clone to
      for (dwSurface = 1; ; dwSurface++)
         if (!ObjectSurfaceFind (dwSurface))
            break;

      // add it to the coloration
      PCObjectSurface pos = pMerge->ObjectSurfaceFind (pmsMerge->m_dwRendSurface);
      DWORD dwTemp = pos->m_dwID;
      pos->m_dwID = dwSurface;   // temporarily change
      ObjectSurfaceAdd (pos);
      pos->m_dwID = dwTemp;   // to restore

      // clone metasurface and add
      pmsMerge = pmsMerge->Clone();
      pmsMerge->m_dwRendSurface = dwSurface;
      dwTemp = m_lPCMetaSurface.Num();
      m_lPCMetaSurface.Add (&pmsMerge);
      lRemap.Add (&dwTemp);   // so have remap number
   } // i
   DWORD *padwRemap = (DWORD*)lRemap.Get(0);

   // figure out the matrix to remap the metaballs
   CMatrix mThis, mMerge, mConvert;
   ObjectMatrixGet (&mThis);
   pMerge->ObjectMatrixGet (&mMerge);
   mThis.Invert4 (&mConvert);
   mConvert.MultiplyLeft (&mMerge);

   // remember the LOD for the first metaball
   PCMetaball *ppmb = (PCMetaball*)m_lPCMetaball.Get(0);
   CPoint pCenter, pLLT, pSize, pPower;
   fp fLOD, fStrength;
   ppmb[0]->ShapeGet (NULL, NULL, NULL, NULL, &fLOD, NULL);

   // go through all the metaballs and merge
   ppmb = (PCMetaball*)pMerge->m_lPCMetaball.Get(0);
   for (i = 0; i < pMerge->m_lPCMetaball.Num(); i++) {
      PCMetaball pmb = ppmb[i];
      pmb->ShapeGet (&pCenter, &pLLT, &pSize, &pPower, NULL, &fStrength);

      // adjust scaling and location
      CMatrix m;
      m.FromXYZLLT (&pCenter, pLLT.p[2], pLLT.p[0], pLLT.p[1]);
      m.MultiplyRight (&mConvert);
      m.ToXYZLLT (&pCenter, &pLLT.p[2], &pLLT.p[0], &pLLT.p[1]);
      
      pmb = pmb->Clone();
      if (!pmb)
         continue;
      pmb->ShapeSet (&pCenter, &pLLT, &pSize, &pPower, fLOD, fStrength);
      if (pmb->m_dwMetaSurface < lRemap.Num())
         pmb->m_dwMetaSurface = padwRemap[pmb->m_dwMetaSurface];
      else
         pmb->m_dwMetaSurface = 0;

      // add it
      m_lPCMetaball.Add (&pmb);

      // make the area it touches dirty
      Dirty (pmb);
   } // i

   // inform that changed
   m_pWorld->ObjectChanged (this);

   return TRUE;
}


/**********************************************************************************
CObjectCave::Message -
sends a message to the object. The interpretation of the message depends upon
dwMessage, which is OSM_XXX. If the function understands and handles the
message it returns TRUE, otherwise FALE.
*/
BOOL CObjectCave::Message (DWORD dwMessage, PVOID pParam)
{
   switch (dwMessage) {
   case OSM_CAVE:
      {
         POSMCAVE p = (POSMCAVE) pParam;
         p->pCave = this;
      }
      return TRUE;

   }

   return FALSE;
}



/*****************************************************************************************
CObjectCave::Merge -
asks the object to merge with the list of objects (identified by GUID) in pagWith.
dwNum is the number of objects in the list. The object should see if it can
merge with any of the ones in the list (some of which may no longer exist and
one of which may be itself). If it does merge with any then it return TRUE.
if no merges take place it returns false.
*/
BOOL CObjectCave::Merge (GUID *pagWith, DWORD dwNum)
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

      // send a message to see if it is another struct surface
      OSMCAVE os;
      memset (&os, 0, sizeof(os));
      if (!pos->Message (OSM_CAVE, &os))
         continue;
      if (!os.pCave)
         continue;

      // dont merge with self
      if (os.pCave == this)
         continue;

      if (MergeIndividual (os.pCave)) {
         // delete the other object
         m_pWorld->ObjectRemove (dwFind);
         fRet = TRUE;
      }
   }

   return fRet;
}


/**********************************************************************************
CObjectCave::TextureQuery -
asks the object what textures it uses. This allows the save-function
to save custom textures into the file. The object just ADDS (doesn't
clear or remove) elements, which are two guids in a row: the
gCode followed by the gSub of the object. Of course, it may add more
than one texture
*/
BOOL CObjectCave::TextureQuery (PCListFixed plText)
{
   // call into template code
   CObjectTemplate::TextureQuery (plText);


   DWORD dwRenderShard = m_OSINFO.dwRenderShard;

   // Will need to call texture query for all the forests
   DWORD i, j;
   PCMetaSurface *ppms = (PCMetaSurface*)m_lPCMetaSurface.Get(0);
   for (i = 0; i < m_lPCMetaSurface.Num(); i++)
      for (j = 0; j < NUMCAVECANOPY; j++)
         ppms[i]->m_apCaveCanopy[j]->TextureQuery (dwRenderShard, plText);

   return TRUE;
}



/**********************************************************************************
CObjectCave::ColorQuery -
asks the object what colors it uses (exclusive of textures).
It adds elements to plColor, which is a list of COLORREF. It may
add more than one color
*/
BOOL CObjectCave::ColorQuery (PCListFixed plColor)
{
   // call into template code
   CObjectTemplate::ColorQuery (plColor);

   DWORD dwRenderShard = m_OSINFO.dwRenderShard;

   // Will need to call colorquery for all the forests
   DWORD i, j;
   PCMetaSurface *ppms = (PCMetaSurface*)m_lPCMetaSurface.Get(0);
   for (i = 0; i < m_lPCMetaSurface.Num(); i++)
      for (j = 0; j < NUMCAVECANOPY; j++)
         ppms[i]->m_apCaveCanopy[j]->ColorQuery (dwRenderShard, plColor);

   return TRUE;
}


/**********************************************************************************
CObjectCave::ObjectClassQuery
asks the curent object what other objects (including itself) it requires
so that when a file is saved, all user objects will be saved along with
the file, so people on other machines can load them in.
The object just ADDS (doesn't clear or remove) elements, which are two
guids in a row: gCode followed by gSub of the object. All objects
must add at least on (their own). Some, like CObjectEditor, will add
all its sub-objects too
*/
BOOL CObjectCave::ObjectClassQuery (PCListFixed plObj)
{
   // call into template
   CObjectTemplate::ObjectClassQuery (plObj);


   DWORD dwRenderShard = m_OSINFO.dwRenderShard;

   // Will need to call colorquery for all the forests
   DWORD i, j;
   PCMetaSurface *ppms = (PCMetaSurface*)m_lPCMetaSurface.Get(0);
   for (i = 0; i < m_lPCMetaSurface.Num(); i++)
      for (j = 0; j < NUMCAVECANOPY; j++)
         ppms[i]->m_apCaveCanopy[j]->ObjectClassQuery (dwRenderShard, plObj);

   return TRUE;
}


/**********************************************************************************
CObjectCave::ChangedCanopy - Called when the canopy is changed. This goes through
and dirties all the metaballs dealing with the canopy

inputs
   PCCaveCanopy       pCanopy - Canopy that changed
returns
   none
*/
void CObjectCave::ChangedCanopy (PCCaveCanopy pCanopy)
{
   // figure out which texture
   DWORD i, j, dwSurface;
   PCMetaSurface *ppms = (PCMetaSurface*)m_lPCMetaSurface.Get(0);
   for (i = 0; i < m_lPCMetaSurface.Num(); i++) {
      for (j = 0; j < NUMCAVECANOPY; j++)
         if (ppms[i]->m_apCaveCanopy[j] == pCanopy)
            break;
      if (j < NUMCAVECANOPY)
         break;
   }
   if (i < m_lPCMetaSurface.Num())
      dwSurface = i;
   else
      return;  // shouldnt happen

   // go through and dirty all the metaballs using this surfaec
   PCMetaball *ppmb = (PCMetaball*)m_lPCMetaball.Get(0);
   CListFixed lDirty;
   lDirty.Init (sizeof(DWORD));
   for (i = 0; i < m_lPCMetaball.Num(); i++)
      if (ppmb[i]->m_dwMetaSurface == dwSurface) {
         ppmb[i]->ShapeClear();
         lDirty.Add (&i);
      }

   DWORD *padw = (DWORD*)lDirty.Get(0);
   for (i = 0; i < m_lPCMetaball.Num(); i++)
      // clear any objects that are stalagmites from this canopy
      ppmb[i]->ClearObjectsFromMetaballs (padw, lDirty.Num());

}


