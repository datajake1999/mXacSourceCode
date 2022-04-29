/************************************************************************
CObjectPool.cpp - Draws a Pool.

begun 27/9/02 by Mike Rozak
Copyright 2002 Mike Rozak. All rights reserved
*/

#include <windows.h>
#include "escarpment.h"
#include "..\M3D.h"
#include "resource.h"

/**********************************************************************************
CObjectPool::Constructor and destructor */
CObjectPool::CObjectPool (PVOID pParams, POSINFO pInfo)
{
   m_dwRenderShow = RENDERSHOW_PLUMBING;
   m_OSINFO = *pInfo;
   m_fCanBeEmbedded = TRUE;

   m_pLip.Zero();
   m_pLip.p[0] = .05;
   m_pLip.p[1] = .025;
   m_pCenter.Zero();
   m_pCenter.p[0] = -.40;
   m_pCenter.p[2] = -.3;
   m_pBottomNorm.Zero();
   m_pBottomNorm.p[2] = 1;
   m_pBottomNorm.p[0] = -.025;
   m_pBottomNorm.Normalize();
   m_fFitToEmbed = FALSE;
   m_fRectangular = TRUE;
   m_fWallSlope = .3;
   m_fWallRounded = .5;
   m_fFilled = .8;
   m_fHoleSize = .05;

   m_dwShape = 2;
   m_pShapeSize.Zero();
   m_pShapeSize.p[0] = 1.3;
   m_pShapeSize.p[1] = .6;
   m_pShapeSize.p[2] = .2;

   m_fNeedRecalcEmbed = TRUE;
   m_pScaleMin.Zero();
   m_pScaleMax.Zero();
   m_lLipOutside.Init (sizeof(CPoint));
   m_lPoolEdge.Init (sizeof(CPoint));
   m_lPoolIn.Init (sizeof(CPoint));
   m_lPoolHole.Init (sizeof(CPoint));
   m_lWater.Init (sizeof(CPoint));

   m_dwNumPoints = m_dwNumNormals = m_dwNumText = 0;
   m_dwNumVertex = m_dwNumPoly = 0;
   CalcPool();

   // color for the Pool
   ObjectSurfaceAdd (1, RGB(0xff,0xff,0xff), MATERIAL_TILEGLAZED);   // bottom of pool
   ObjectSurfaceAdd (2, RGB(0xff,0xff,0xff), MATERIAL_TILEGLAZED);   // walls of pool
   ObjectSurfaceAdd (3, RGB(0xff,0xff,0xff), MATERIAL_TILEGLAZED);   // lip of pool
   ObjectSurfaceAdd (4, RGB(0x80, 0x80, 0xff), MATERIAL_GLASSCLEAR, L"Pool water",
      &GTEXTURECODE_PoolWater, &GTEXTURESUB_PoolWater);
}


CObjectPool::~CObjectPool (void)
{
   // do nothing for now
}


/**********************************************************************************
CObjectPool::Delete - Called to delete this object
*/
void CObjectPool::Delete (void)
{
   delete this;
}

/**********************************************************************************
CObjectPool::Render - Draws a test box.

inputs
   POBJECTRENDER     pr - Render information
*/
void CObjectPool::Render (POBJECTRENDER pr, DWORD dwSubObject)
{
   // create the surface render object and draw
   // CRenderSurface rs;
   m_Renderrs.ClearAll();

   CMatrix mObject;
   m_Renderrs.Init (pr->pRS);
   ObjectMatrixGet (&mObject);
   m_Renderrs.Multiply (&mObject);
   DWORD i;
   PCPoint p;

   // If embeded will rotate 90 degrees
   GUID gContain;
   if (EmbedContainerGet(&gContain)) {
     m_Renderrs.Rotate (PI/2, 1);

     // may need to recalc - if just drawin this for the first time and it's embedded
     // then need to get info from parent
     if (m_fNeedRecalcEmbed && m_fFitToEmbed)
        CalcPool();
   }
   m_fNeedRecalcEmbed = FALSE;

   // draw the water and the base
   DWORD dwWater;
   PCPoint pPoint, pNormal;
   PTEXTPOINT5 pText;
   PVERTEX pVert;
   DWORD dwPointIndex, dwNormalIndex, dwTextIndex, dwVertIndex;
   for (dwWater = 0; dwWater < 2; dwWater++) {
      // if want wanter, but no list then continue
      if (dwWater && !m_lWater.Num())
         continue;

      // pointer to list for the edge
      PCListFixed plEdge, plIn;
      plEdge = dwWater ? &m_lWater : &m_lPoolIn;
      plIn = dwWater ? NULL : &m_lPoolHole;
      if (plIn && !plIn->Num())
         plIn = FALSE;  // if all the same then clear

      // set the color
      m_Renderrs.SetDefMaterial (m_OSINFO.dwRenderShard, ObjectSurfaceFind (dwWater ? 4 : 1), m_pWorld);

      // create all the points
      DWORD dwNumP;
      dwNumP = plEdge->Num() + (plIn ? plIn->Num() : 1);
      pPoint = m_Renderrs.NewPoints (&dwPointIndex, dwNumP);
      if (!pPoint)
         continue;
      p = pPoint;
      memcpy (p, (PCPoint) plEdge->Get(0), plEdge->Num() * sizeof(CPoint));
      p += plEdge->Num();

      if (plIn)
         memcpy (p, (PCPoint) plIn->Get(0), plIn->Num() * sizeof(CPoint));
      else {
         p->Copy (&m_pCenter);
         if (dwWater)
            p->p[2] = pPoint->p[2];
      }

      // normals
      pNormal = m_Renderrs.NewNormals (TRUE, &dwNormalIndex, 1);
      if (pNormal) {
         if (dwWater) {
            pNormal->Zero();
            pNormal->p[2] =1;
         }
         else {
            pNormal->Copy (&m_pBottomNorm);
            pNormal->Normalize();
         }
      }

      // textures
      pText = m_Renderrs.NewTextures (&dwTextIndex, dwNumP);
      if (pText) {
         for (i = 0; i < dwNumP; i++) {
            pText[i].hv[0] = pPoint[i].p[0];
            pText[i].hv[1] = -pPoint[i].p[1];
            pText[i].xyz[0] = pPoint[i].p[0];
            pText[i].xyz[1] = pPoint[i].p[1];
            pText[i].xyz[2] = pPoint[i].p[2];
         }
         m_Renderrs.ApplyTextureRotation (pText, dwNumP);
      }

      // vertices
      pVert = m_Renderrs.NewVertices (&dwVertIndex, dwNumP);
      if (!pVert)
         continue;
      for (i = 0; i < dwNumP; i++) {
         pVert[i].dwNormal = dwNormalIndex;
         pVert[i].dwPoint = dwPointIndex + i;
         pVert[i].dwTexture = pText ? (dwTextIndex + i) : 0;
      }

      // polygons
      DWORD dwEN;
      dwEN = plEdge->Num();
      for (i = 0; i < dwEN; i++) {
         if (plIn)
            m_Renderrs.NewQuad (
               dwVertIndex + dwEN + i,
               dwVertIndex + i,
               dwVertIndex + ((i+1)%dwEN),
               dwVertIndex + dwEN + ((i+1)%dwEN),
               FALSE);  // dont allow backface culling since may look from underneath
         else
            m_Renderrs.NewTriangle (
               dwVertIndex + dwEN,
               dwVertIndex + i,
               dwVertIndex + ((i+1)%dwEN),
               FALSE);  // dont allow backface culling since may look from underneath
      }
   }


   // make sure enough memory for XYZHV
   CMem mem;
   if (!mem.Required (m_lPoolEdge.Num() * sizeof(HVXYZ) * 2)) {
      m_Renderrs.Commit();
      return;
   }
   PHVXYZ pl1, pl2;
   memset (mem.p, 0, mem.m_dwAllocated);
   pl1 = (PHVXYZ) mem.p;
   pl2 = pl1 + m_lPoolEdge.Num();


   // lip's rim
   if (m_lLipOutside.Num()) {
      m_Renderrs.SetDefMaterial (m_OSINFO.dwRenderShard, ObjectSurfaceFind (3), m_pWorld);

      p = (PCPoint) m_lLipOutside.Get(0);
      DWORD dwNum;
      dwNum = m_lLipOutside.Num();
      for (i = 0; i < dwNum; i++) {
         pl1[i].p.Copy (&p[dwNum - i - 1]);
         pl2[i].p.Copy (&pl1[i].p);
         pl2[i].p.p[2] = 0;
      }

      if (m_pLip.p[1] > CLOSE)
         m_Renderrs.ShapeZipper (pl2, dwNum, pl1, dwNum, TRUE,
            0, 1, FALSE);

      // and inside
      p = (PCPoint) m_lPoolEdge.Get(0);
      dwNum = m_lPoolEdge.Num();
      for (i = 0; i < dwNum; i++)
         pl2[i].p.Copy (&p[dwNum - i - 1]);
      m_Renderrs.ShapeZipper (pl1, m_lLipOutside.Num(), pl2, dwNum, TRUE,
         0, 1, FALSE);
   }


   // pool wall
   m_Renderrs.SetDefMaterial (m_OSINFO.dwRenderShard, ObjectSurfaceFind (2), m_pWorld);

   // test use of zipper for wall
   //p = (PCPoint) m_lPoolEdge.Get(0);
   //for (i = 0; i < m_lPoolEdge.Num(); i++)
   //   pl1[i].p.Copy (&p[i]);
   //p = (PCPoint) m_lPoolIn.Get(0);
   //for (i = 0; i < m_lPoolIn.Num(); i++)
   //   pl2[i].p.Copy (&p[i]);
   //m_Renderrs.ShapeZipper (pl2, m_lPoolIn.Num(), pl1, m_lPoolEdge.Num(), TRUE,
   //   0, 1, FALSE);




   pPoint = m_Renderrs.NewPoints (&dwPointIndex, m_dwNumPoints);
   if (pPoint) {
      memcpy (pPoint, m_memPoints.p, m_dwNumPoints * sizeof(CPoint));
   }

   pNormal = m_Renderrs.NewNormals (TRUE, &dwNormalIndex, m_dwNumNormals);
   if (pNormal) {
      memcpy (pNormal, m_memNormals.p, m_dwNumNormals * sizeof(CPoint));
   }

   pText = m_Renderrs.NewTextures (&dwTextIndex, m_dwNumText);
   if (pText) {
      memcpy (pText, m_memText.p, m_dwNumText * sizeof(TEXTPOINT5));

      m_Renderrs.ApplyTextureRotation (pText, m_dwNumText);
   }

   pVert = m_Renderrs.NewVertices (&dwVertIndex, m_dwNumVertex);
   if (pVert) {
      PVERTEX pvs, pvd;
      pvs = (PVERTEX) m_memVertex.p;
      pvd = pVert;
      for (i = 0; i < m_dwNumVertex; i++, pvs++, pvd++) {
         pvd->dwNormal = pNormal ? (pvs->dwNormal + dwNormalIndex) : 0;
         pvd->dwPoint = pvs->dwPoint + dwPointIndex;
         pvd->dwTexture = pText ? (pvs->dwTexture + dwTextIndex) : 0;
      }
   }

   DWORD *padw;
   padw = (DWORD*) m_memPoly.p;
   for (i = 0; i < m_dwNumPoly; i++, padw += 4) {
      if (padw[3] == -1) {
         m_Renderrs.NewTriangle (dwVertIndex + padw[0], dwVertIndex + padw[1],
            dwVertIndex + padw[2], FALSE);
      }
      else {
         m_Renderrs.NewQuad (dwVertIndex + padw[0], dwVertIndex + padw[1],
            dwVertIndex + padw[2], dwVertIndex + padw[3], FALSE);
      }
   }
 
   m_Renderrs.Commit();
}

// NOTE: Not doing QueryBoundingBox() because somewhat complex and likely
// to introduce bugs

/**********************************************************************************
CObjectPool::Clone - Clones this

inputs
   none
returns
   PCObjectSocket - Clone
*/
PCObjectSocket CObjectPool::Clone (void)
{
   PCObjectPool pNew;

   pNew = new CObjectPool(NULL, &m_OSINFO);

   // clone template info
   CloneTemplate(pNew);

   m_sShape.CloneTo (&pNew->m_sShape);
   pNew->m_pLip.Copy (&m_pLip);
   pNew->m_pCenter.Copy (&m_pCenter);
   pNew->m_pBottomNorm.Copy (&m_pBottomNorm);
   pNew->m_fFitToEmbed = m_fFitToEmbed;
   pNew->m_fRectangular = m_fRectangular;
   pNew->m_fWallSlope = m_fWallSlope;
   pNew->m_fWallRounded = m_fWallRounded;
   pNew->m_fFilled = m_fFilled;
   pNew->m_fHoleSize = m_fHoleSize;

   pNew->m_dwShape = m_dwShape;
   pNew->m_pShapeSize.Copy (&m_pShapeSize);

   // clone polygon pieces
   pNew->m_dwNumPoints = m_dwNumPoints;
   pNew->m_dwNumNormals = m_dwNumNormals;
   pNew->m_dwNumText = m_dwNumText;
   pNew->m_dwNumVertex = m_dwNumVertex;
   pNew->m_dwNumPoly = m_dwNumPoly;
   DWORD dwNeed;
   if (pNew->m_memPoints.Required(dwNeed = m_dwNumPoints * sizeof(CPoint)))
      memcpy (pNew->m_memPoints.p, m_memPoints.p, dwNeed);
   if (pNew->m_memNormals.Required(dwNeed = m_dwNumNormals * sizeof(CPoint)))
      memcpy (pNew->m_memNormals.p, m_memNormals.p, dwNeed);
   if (pNew->m_memText.Required(dwNeed = m_dwNumText * sizeof(TEXTPOINT5)))
      memcpy (pNew->m_memText.p, m_memText.p, dwNeed);
   if (pNew->m_memVertex.Required(dwNeed = m_dwNumVertex * sizeof(VERTEX)))
      memcpy (pNew->m_memVertex.p, m_memVertex.p, dwNeed);
   if (pNew->m_memPoly.Required(dwNeed = m_dwNumPoly * sizeof(DWORD)*4))
      memcpy (pNew->m_memPoly.p, m_memPoly.p, dwNeed);

   pNew->m_lLipOutside.Init (sizeof(CPoint), m_lLipOutside.Get(0), m_lLipOutside.Num());
   pNew->m_lPoolEdge.Init (sizeof(CPoint), m_lPoolEdge.Get(0), m_lPoolEdge.Num());
   pNew->m_lPoolIn.Init (sizeof(CPoint), m_lPoolIn.Get(0), m_lPoolIn.Num());
   pNew->m_lPoolHole.Init (sizeof(CPoint), m_lPoolHole.Get(0), m_lPoolHole.Num());
   pNew->m_lWater.Init (sizeof(CPoint), m_lWater.Get(0), m_lWater.Num());

   pNew->m_pScaleMin.Copy (&m_pScaleMin);
   pNew->m_pScaleMax.Copy (&m_pScaleMax);
   pNew->m_fNeedRecalcEmbed = m_fNeedRecalcEmbed;

   return pNew;
}

static PWSTR gpszShape = L"Shape";
static PWSTR gpszLip = L"Lip";
static PWSTR gpszCenter = L"Center";
static PWSTR gpszBottomNorm = L"BottomNorm";
static PWSTR gpszFitToEmbed = L"FitToEmbed";
static PWSTR gpszRectangular = L"Rectangular";
static PWSTR gpszWallSlope = L"WallSlope";
static PWSTR gpszWallRounded = L"WallRounded";
static PWSTR gpszFilled = L"Filled";
static PWSTR gpszHoleSize = L"HoleSize";
static PWSTR gpszShapeNum = L"ShapeNum";
static PWSTR gpszShapeSize = L"ShapeSize";

PCMMLNode2 CObjectPool::MMLTo (void)
{
   // call into the template first
   PCMMLNode2 pNode = MMLToTemplate();
   if (!pNode)
      return NULL;

   PCMMLNode2 pSub;
   pSub = m_dwShape ? NULL : m_sShape.MMLTo();
   if (pSub) {
      pSub->NameSet (gpszShape);
      pNode->ContentAdd (pSub);
   }

   MMLValueSet (pNode, gpszLip, &m_pLip);
   MMLValueSet (pNode, gpszCenter, &m_pCenter);
   MMLValueSet (pNode, gpszBottomNorm, &m_pBottomNorm);
   MMLValueSet (pNode, gpszFitToEmbed, (int)m_fFitToEmbed);
   MMLValueSet (pNode, gpszRectangular, (int)m_fRectangular);
   MMLValueSet (pNode, gpszWallSlope, m_fWallSlope);
   MMLValueSet (pNode, gpszWallRounded, m_fWallRounded);
   MMLValueSet (pNode, gpszFilled, m_fFilled);
   MMLValueSet (pNode, gpszHoleSize, m_fHoleSize);

   MMLValueSet (pNode, gpszShapeNum, (int) m_dwShape);
   MMLValueSet (pNode, gpszShapeSize, &m_pShapeSize);

   return pNode;
}

BOOL CObjectPool::MMLFrom (PCMMLNode2 pNode)
{
   // call into the template first
   if (!MMLFromTemplate (pNode))
      return FALSE;

   MMLValueGetPoint (pNode, gpszLip, &m_pLip);
   MMLValueGetPoint (pNode, gpszCenter, &m_pCenter);
   MMLValueGetPoint (pNode, gpszBottomNorm, &m_pBottomNorm);
   m_fFitToEmbed = (BOOL) MMLValueGetInt (pNode, gpszFitToEmbed, (int)0);
   m_fRectangular = (BOOL) MMLValueGetInt (pNode, gpszRectangular, (int)0);
   m_fWallSlope = MMLValueGetDouble (pNode, gpszWallSlope, 0);
   m_fWallRounded = MMLValueGetDouble (pNode, gpszWallRounded, 0);
   m_fFilled = MMLValueGetDouble (pNode, gpszFilled, 0);
   m_fHoleSize = MMLValueGetDouble (pNode, gpszHoleSize, 0);

   m_dwShape = (DWORD) MMLValueGetInt (pNode, gpszShapeNum, (int) 1);
   MMLValueGetPoint (pNode, gpszShapeSize, &m_pShapeSize);

   if (!m_dwShape) {
      PCMMLNode2 pSub;
      PWSTR psz;
      pSub = NULL;
      pNode->ContentEnum (pNode->ContentFind (gpszShape), &psz, &pSub);
      if (pSub)
         m_sShape.MMLFrom (pSub);
   }


   // Need to reconstruct info from this
   CalcPool();

   return TRUE;
}


/**********************************************************************************
CObjectPool::CalcPool - Calculates the path polygons given all the settings.
*/
BOOL CObjectPool::CalcPool (void)
{
   BOOL fSecondPass = FALSE;

   // zero out
   m_lLipOutside.Clear();
   m_lPoolEdge.Clear();
   m_lPoolIn.Clear();
   m_lPoolHole.Clear();
   m_lWater.Clear();
   m_dwNumPoints = m_dwNumNormals = m_dwNumText = 0;
   m_dwNumVertex = m_dwNumPoly = 0;

   // rebuild the shape
   ShapeToSpline (&m_sShape);

   // figure out the pool edge
   DWORD i;
   CPoint pT;
   m_lPoolEdge.Required (m_sShape.QueryNodes());
   for (i = 0; i < m_sShape.QueryNodes(); i++) {
      pT.Copy (m_sShape.LocationGet (i));
      if (!m_fFitToEmbed)
         pT.p[2] = m_pLip.p[1];
      m_lPoolEdge.Add (&pT);
   }

tryagain:
   // calculate the inside
   CPoint pEnd;
   PCPoint p;
   m_lPoolIn.Init (sizeof(CPoint), m_lPoolEdge.Get(0), m_lPoolEdge.Num());
   p = (PCPoint) m_lPoolIn.Get(0);
   for (i = 0; i < m_lPoolIn.Num(); i++) {
      p[i].Average (&m_pCenter, m_fWallSlope);
      pEnd.Copy (&p[i]);
      pEnd.p[2] -= 1;   // so goes down

      if (IntersectLinePlane (&p[i], &pEnd, &m_pCenter, &m_pBottomNorm, &pT))
         p[i].Copy (&pT);
   }

   // if this fits to the embedded object then do what can to calculate how this
   // intersects
   GUID gContainer;
   PCObjectSocket pos;
   if (!fSecondPass && m_fFitToEmbed && EmbedContainerGet (&gContainer) && m_pWorld) {
      // first thing is to calculate the cutout because rely on that
      MyEmbedDoCutout();

      pos = m_pWorld->ObjectGet (m_pWorld->ObjectFind(&gContainer));
      if (!pos)
         goto drawit;

      CListFixed l1, l2;
      l1.Init (sizeof(HVXYZ));
      l2.Init (sizeof(HVXYZ));
      pos->ContCutoutToZipper (&m_gGUID, &l1, &l2);
      if (!l1.Num())
         goto drawit;

      // figure out matrix to convert from the containr's space to this object's space
      CMatrix mCont, mInv, mTemp;
      // account for roation
      mTemp.RotationX (PI/2);
      mTemp.MultiplyRight (&m_MatrixObject);
      mTemp.Invert4 (&mInv);
      pos->ObjectMatrixGet (&mCont);
      mInv.MultiplyLeft (&mCont);

      // clear out pool edge since will recalculate
      m_lPoolEdge.Clear();

      DWORD i;
      PHVXYZ p;
      m_lPoolEdge.Required (l1.Num());
      for (i = 0; i < l1.Num(); i++) {
         // BUGFIX - Had to reverse the order since apparantly for the cutout getting it
         // back counterclockwise...
         // NOTE: In future may need to run code to see if is clockwise or not
         p = (PHVXYZ) l1.Get(l1.Num() - i - 1);
         p->p.p[3] = 1;
         p->p.MultiplyLeft (&mInv);

         m_lPoolEdge.Add (&p->p);
      }


      fSecondPass = TRUE;
      goto tryagain;

   }

drawit:
   // figure out the edge
   p = (PCPoint) m_lPoolEdge.Get(0);
   for (i = 0; i < m_lPoolEdge.Num(); i++) {
      if (i) {
         m_pScaleMin.Min (&p[i]);
         m_pScaleMax.Max (&p[i]);
      }
      else {
         m_pScaleMin.Copy (&p[i]);
         m_pScaleMax.Copy (&p[i]);
      }
   }
   m_pScaleMax.p[0] += m_pLip.p[0];
   m_pScaleMax.p[1] += m_pLip.p[0];
   m_pScaleMin.p[0] -= m_pLip.p[0];
   m_pScaleMin.p[1] -= m_pLip.p[0];
   m_pScaleMin.p[2] = m_pScaleMax.p[2] = 0;


   // loip outside
   if (!m_fFitToEmbed && (m_pLip.p[0] > CLOSE)) {
      if (m_fRectangular || (m_dwShape == 1)) {
         // make a rectangle
         CListFixed lTemp;
         lTemp.Init (sizeof(CPoint));

         // make about the same number of points as have on the pool edge
         DWORD dwPerSide = max(m_lPoolEdge.Num()/4,1);


         // create all the points
         for (i = 0; i < 4; i++) {
            CPoint pStart, pEnd;
            pStart.Zero();
            pEnd.Zero();
            switch (i) {
            case 0:  // bottom
               pStart.p[0] = m_pScaleMax.p[0];
               pStart.p[1] = m_pScaleMin.p[1];
               pEnd.Copy (&m_pScaleMin);
               break;
            case 1:  // left
               pStart.Copy (&m_pScaleMin);
               pEnd.p[0] = m_pScaleMin.p[0];
               pEnd.p[1] = m_pScaleMax.p[1];
               break;
            case 2:  // top
               pStart.p[0] = m_pScaleMin.p[0];
               pStart.p[1] = m_pScaleMax.p[1];
               pEnd.Copy (&m_pScaleMax);
               break;
            case 3:  // right
               pStart.Copy (&m_pScaleMax);
               pEnd.p[0] = m_pScaleMax.p[0];
               pEnd.p[1] = m_pScaleMin.p[1];
               break;
            }

            DWORD j;
            lTemp.Required (dwPerSide);
            for (j = 0; j < dwPerSide; j++) {
               pT.Average (&pEnd, &pStart, (fp) j / (fp)dwPerSide);
               pT.p[2] = m_pLip.p[1];
               lTemp.Add (&pT);
            }
         }

         // find the closest one
         p = (PCPoint) lTemp.Get(0);
         DWORD dwClosest;
         fp fClosest, fLen;
         for (i = 0; i < lTemp.Num(); i++) {
            pT.Subtract (&p[i], (PCPoint) m_lPoolEdge.Get(0));
            fLen = pT.Length();
            if (!i || (fLen < fClosest)) {
               dwClosest = i;
               fClosest = fLen;
            }
         }

         // add from closet to the end on
         m_lLipOutside.Init (sizeof(CPoint), p + dwClosest, lTemp.Num() - dwClosest);

         // add from 0 to closest onto the end
         m_lLipOutside.Required (dwClosest);
         for (i = 0; i < dwClosest; i++)
            m_lLipOutside.Add (p + i);
      }
      else {   // not rectangular
         PCSpline pNew = m_sShape.Expand (m_pLip.p[0]);
         if (!pNew)
            return FALSE;

         m_lLipOutside.Init (sizeof(CPoint));

         m_lLipOutside.Required (pNew->QueryNodes());
         for (i = 0; i < pNew->QueryNodes(); i++) {
            pT.Copy (pNew->LocationGet (i));
            if (!m_fFitToEmbed)
               pT.p[2] = m_pLip.p[1];
            m_lLipOutside.Add (&pT);
         }

         delete pNew;
      }
   }
   // calculate the hole
   if (m_fHoleSize > CLOSE) {
      m_lPoolHole.Init (sizeof(CPoint), m_lPoolEdge.Get(0), m_lPoolEdge.Num());
      p = (PCPoint) m_lPoolHole.Get(0);
      for (i = 0; i < m_lPoolHole.Num(); i++) {
         p[i].Subtract (&m_pCenter);
         p[i].p[2] = 0; // so no Z component
         p[i].Normalize();
         p[i].Scale (m_fHoleSize/2);
         p[i].Add (&m_pCenter);

         // NOTE: Don't bother intersecting with bottom because a) hole should be
         // small enough it doesn't matter, and b) usually have the hole flat anyway
      }
   }

#define WALLDETAIL      5  // number of points in the wall

   // so, we know how many points, etc. we need
   DWORD dwNum;
   dwNum = m_lPoolEdge.Num();
   m_dwNumPoints = dwNum * WALLDETAIL;
   m_dwNumNormals = m_dwNumPoints;
   m_dwNumText = (dwNum+1) * WALLDETAIL;
   m_dwNumVertex = (dwNum+1) * WALLDETAIL;
   m_dwNumPoly = dwNum * (WALLDETAIL-1);

   // make sure have enough memory
   if (!m_memPoints.Required (m_dwNumPoints * sizeof(CPoint)))
      return FALSE;
   if (!m_memNormals.Required (m_dwNumNormals * sizeof(CPoint)))
      return FALSE;
   if (!m_memText.Required (m_dwNumText * sizeof(TEXTPOINT5)))
      return FALSE;
   if (!m_memVertex.Required (m_dwNumVertex * sizeof(VERTEX)))
      return FALSE;
   if (!m_memPoly.Required (m_dwNumPoly * sizeof(DWORD) * 4))
      return FALSE;

   // pointers to these
   PCPoint pPoint, pNormal;
   PTEXTPOINT5 pText;
   PVERTEX pVert;
   DWORD *pPoly;
   pPoint = (PCPoint) m_memPoints.p;
   memset (pPoint, 0 ,sizeof (CPoint) * m_dwNumPoints);
   pNormal = (PCPoint) m_memNormals.p;
   memset (pNormal, 0, sizeof(CPoint) * m_dwNumNormals);
   pText = (PTEXTPOINT5) m_memText.p;
   memset (pText, 0, sizeof(TEXTPOINT5) * m_dwNumText);
   pVert = (PVERTEX) m_memVertex.p;
   memset (pVert, 0, sizeof(VERTEX) * m_dwNumVertex);
   pPoly = (DWORD*) m_memPoly.p;
   memset (pPoly, 0, m_dwNumPoly * 4 * sizeof(DWORD));


   // precalculate the sin
   fp afSin[WALLDETAIL];
   for (i = 0; i < WALLDETAIL; i++) {
      if (i == 0) {
         afSin[i] = 0;
         continue;
      }
      else if (i == WALLDETAIL-1) {
         afSin[i] = 1;
         continue;
      }

      afSin[i] = (sin( ((fp)i / (fp)(WALLDETAIL-1) - .5) * PI) + 1.0) / 2.0;
      afSin[i] = (1 - m_fWallRounded) * (fp) i / (fp)(WALLDETAIL-1) +
         m_fWallRounded * afSin[i]; // average with linear
   }

   // calculate the points and TEXTPOINT5s
   DWORD x, y;
   fp fCurV;
   PTEXTPOINT5 pt, ptLastSet;
   fCurV = 0;
   p = pPoint;
   pt = pText;
   PCPoint p1, p2;
   fp fCurTextH, fCurTextV;
   fCurTextH = 0;
   ptLastSet = NULL;
   for (y = 0; y < dwNum; y++) {
      // udpate the currnet texture
      if (y) {
         pT.Subtract ((PCPoint) m_lPoolEdge.Get(y), (PCPoint) m_lPoolEdge.Get(y-1));
         fCurTextH += pT.Length();
      }

      p1 = (PCPoint) m_lPoolEdge.Get(y);
      p2 = (PCPoint) m_lPoolIn.Get(y);

      fCurTextV = 0;
      ptLastSet = pt;
      for (x = 0; x < WALLDETAIL; x++, p++, pt++) {
         // point
         pT.Average (p2, p1, (fp)x / (fp) (WALLDETAIL-1));  // goes from wall edge in
         pT.p[2] = afSin[x] * p2->p[2] + (1-afSin[x]) * p1->p[2];
         p->Copy (&pT);

         // texture
         if (x) {
            pT.Subtract (p, p-1);
            fCurTextV += pT.Length();
         }

         pt->hv[0] = fCurTextH;
         pt->hv[1] = fCurTextV;
         pt->xyz[0] = pT.p[0];
         pt->xyz[1] = pT.p[1];
         pt->xyz[2] = pT.p[2];
      }
   }

   // last textures
   pT.Subtract ((PCPoint) m_lPoolEdge.Get(dwNum-1), (PCPoint) m_lPoolEdge.Get(0));
   fCurTextH += pT.Length();
   for (x = 0; x < WALLDETAIL; x++, pt++, ptLastSet++) {
      *pt = *ptLastSet;
      pt->hv[0] = fCurTextH;
      pt->hv[1] = pText[x].hv[1];
   }

   
   // calculate the normals
   PCPoint pp2;
   PCPoint pLeft, pRight, pAbove, pBelow;
   CPoint pUp;
   m_pBottomNorm.Normalize();
   pUp.Zero();
   pUp.p[2] = 1;

   p = pNormal;
   pp2 = pPoint;
   for (y = 0; y < dwNum; y++) {
      for (x = 0; x < WALLDETAIL; x++, p++, pp2++) {
         // only do autmatic normal on top/bottom if the walls are rounded and non-zero slope
         if ((m_fWallSlope > CLOSE) && (m_fWallRounded > CLOSE)) {
            if (x == 0) {
               p->Copy (&pUp);
               continue;
            }
            else if (x == WALLDETAIL-1) {
               p->Copy (&m_pBottomNorm);
               p->p[3] = 1;
               continue;
            }
         }

         pAbove = pPoint + (x + ((y+1) % dwNum) * WALLDETAIL);
         pBelow = pPoint + (x + ((y+dwNum-1) % dwNum) * WALLDETAIL);
         pRight = (x+1 < WALLDETAIL) ? (pp2+1) : pp2;
         pLeft = x ? (pp2 - 1) : pp2;

         CPoint pR, pU;
         pR.Subtract (pRight, pLeft);
         pU.Subtract (pAbove, pBelow);
         p->CrossProd (&pR, &pU);
         p->Normalize();
      }
   } // over y

   
   // all the vertices
   PVERTEX pv;
   pv = pVert;
   for (y = 0; y < dwNum+1; y++)
      for (x = 0; x < WALLDETAIL; x++, pv++) {
         pv->dwNormal = pv->dwPoint = x + (y % dwNum) * WALLDETAIL;
         pv->dwTexture = x + y * WALLDETAIL;
      }


   // and the polygons
   DWORD *padw;
   DWORD dwVert;
   padw = pPoly;
   dwVert = 0;
   for (y = 0; y < dwNum; y++) { // bottom to top
      for (x = 0; x < WALLDETAIL-1; x++, padw += 4) { // left to right
         padw[0] = x+1 + y*WALLDETAIL;
         padw[1] = x + y*WALLDETAIL;
         padw[2] = x + (y+1)*WALLDETAIL;
         padw[3] = x+1 + (y+1)*WALLDETAIL;
      }
   }

   // calculate the water level
   if (m_fFilled > CLOSE) {
      m_lWater.Init (sizeof(CPoint), m_lPoolEdge.Get(0), m_lPoolEdge.Num());
      p = (PCPoint) m_lWater.Get(0);

      // plane...
      CPoint pUp, pLevel;
      fp fLevel;
      pUp.Zero();
      pUp.p[2] = 1;
      pLevel.Zero();
      pLevel.Average (&p[0], &m_pCenter, m_fFilled);
      fLevel = pLevel.p[2];

      // intersect all points
      for (y = 0; y < m_lWater.Num(); y++) {
         p1 = &pPoint[y * WALLDETAIL];

         for (x = 0; x < WALLDETAIL-1; x++) {
            if (fLevel < p1[x+1].p[2])
               continue;   // too low

            if (IntersectLinePlane (&p1[x], &p1[x+1], &pLevel, &pUp, &pT)) {
               p[y].Copy (&pT);
               break;
            }
         }
         if (x >= WALLDETAIL-1)
            if (IntersectLinePlane (&p1[WALLDETAIL-2], &p1[WALLDETAIL-1],
               &pLevel, &pUp, &pT))
                  p[y].Copy (&pT);
      }
   }


   // redo the cutout
   MyEmbedDoCutout ();

   return TRUE;
}


/**************************************************************************************
CObjectPool::MoveReferencePointQuery - 
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
BOOL CObjectPool::MoveReferencePointQuery (DWORD dwIndex, PCPoint pp)
{
   if (dwIndex >= 1)
      return FALSE;

   pp->Zero();

   return TRUE;
}

/**************************************************************************************
CObjectPool::MoveReferenceStringQuery -
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
BOOL CObjectPool::MoveReferenceStringQuery (DWORD dwIndex, PWSTR psz, DWORD dwSize, DWORD *pdwNeeded)
{
   if (dwIndex >= 1)
      return FALSE;

   PWSTR szTemp = L"Center";

   DWORD dwNeeded;
   dwNeeded = ((DWORD)wcslen (szTemp) + 1) * 2;
   if (pdwNeeded)
      *pdwNeeded = dwNeeded;
   if (dwNeeded <= dwSize) {
      wcscpy (psz, szTemp);
      return TRUE;
   }
   else
      return FALSE;
}



/*************************************************************************************
CObjectPool::ControlPointQuery - Called to query the information about a control
point identified by dwID.

inputs
   DWORD       dwID - ID
   POSCONTROL  pInfo - Filled in with the infomration
returns
   BOOL - TRUE if successful
*/
BOOL CObjectPool::ControlPointQuery (DWORD dwID, POSCONTROL pInfo)
{
   if ( ((dwID >= 4) && (dwID < 10)) || (dwID >= 10+m_sShape.OrigNumPointsGet()))
      return FALSE;

   // find the center and scale
   CPoint pCenter, pScale;
   DWORD i;
   pCenter.Average (&m_pScaleMax, &m_pScaleMin);
   pScale.Subtract (&m_pScaleMax, &m_pScaleMin);
   for (i = 0; i < 3; i++)
      pScale.p[i] = max(pScale.p[i], CLOSE);

   fp fKnobSize;
   fKnobSize = max(pScale.p[0], pScale.p[1]) * .05;


   memset (pInfo,0, sizeof(*pInfo));
   pInfo->dwID = dwID;
   //pInfo->dwFreedom = 0;   // any direction
   pInfo->fSize = fKnobSize;

   switch (dwID) {
   case 0:  // base
      pInfo->dwStyle = CPSTYLE_SPHERE;
      pInfo->cColor = RGB(0xff,0,0xff);
      wcscpy (pInfo->szName, L"Center");
      pInfo->pLocation.Copy (&m_pCenter);
      MeasureToString (-m_pCenter.p[2], pInfo->szMeasurement);
      break;

   case 1:  // normal
      pInfo->dwStyle = CPSTYLE_POINTER;
      pInfo->cColor = RGB(0xff,0xff, 0);
      wcscpy (pInfo->szName, L"Bottom tilt");
      //pInfo->pDirection.Copy (&m_pBottomNorm);
      pInfo->pLocation.Copy (&m_pBottomNorm);
      pInfo->pLocation.Normalize();
      pInfo->pLocation.Scale (.5);
      pInfo->pLocation.Add (&m_pCenter);
      break;

   case 2:  // width
   case 3:  // length
      pInfo->dwStyle = CPSTYLE_SPHERE;
      pInfo->cColor = RGB(0xff,0,0xff);
      wcscpy (pInfo->szName, (dwID == 2) ? L"Width" : L"Length");
      pInfo->pLocation.Copy (&pCenter);
      if (m_dwShape) {
         pInfo->pLocation.p[dwID-2] += m_pShapeSize.p[dwID-2]/2.0;
         MeasureToString (m_pShapeSize.p[dwID-2], pInfo->szMeasurement);
      }
      else {
         pInfo->pLocation.p[dwID-2] = m_pScaleMax.p[dwID-2];
         MeasureToString (m_pScaleMax.p[dwID-2], pInfo->szMeasurement);
      }
      pInfo->pLocation.p[2] = m_pLip.p[1];
      break;

   default: // edge control points
      pInfo->dwStyle = CPSTYLE_CUBE;
      pInfo->cColor = RGB(0,0xff,0xff);
      wcscpy (pInfo->szName, L"Shape");
      m_sShape.OrigPointGet (dwID - 10, &pInfo->pLocation);
      pInfo->pLocation.p[2] = m_pLip.p[1];
      break;
   }


   // rotate 90 degrees
   GUID gContain;
   CPoint pV;
   pV.Copy (&pInfo->pLocation);
   // NOTE: Ignoring the direction and other settings since not really used
   if (EmbedContainerGet(&gContain)) {
      // rotate 90 degrees
      pInfo->pLocation.p[0] = pV.p[0];
      pInfo->pLocation.p[1] = -pV.p[2];
      pInfo->pLocation.p[2] = pV.p[1];
      pInfo->pLocation.p[3] = 1;
   }

   return TRUE;
}

/*************************************************************************************
CObjectPool::ControlPointSet - Called to change the valud of a control point.

inputs
   DWORD       dwID - ID
   PCPoint     pVal - Contains the new location, in object coordinates
returns
   BOOL - TRUE if successful
*/
BOOL CObjectPool::ControlPointSet (DWORD dwID, PCPoint pVal, PCPoint pViewer)
{
   if ( ((dwID >= 4) && (dwID < 10)) || (dwID >= 10+m_sShape.OrigNumPointsGet()))
      return FALSE;

   // find the center and scale
   CPoint pCenter, pScale;
   DWORD i;
   pCenter.Average (&m_pScaleMax, &m_pScaleMin);
   pScale.Subtract (&m_pScaleMax, &m_pScaleMin);
   for (i = 0; i < 3; i++)
      pScale.p[i] = max(pScale.p[i], CLOSE);

   // tell the world we're about to change
   if (m_pWorld)
      m_pWorld->ObjectAboutToChange (this);

   GUID gContain;
   CPoint pV;
   if (EmbedContainerGet(&gContain)) {
      // rotate 90 degrees
      pV.p[0] = pVal->p[0];
      pV.p[1] = pVal->p[2];
      pV.p[2] = -pVal->p[1];
      pV.p[3] = 1;
   }
   else
      pV.Copy (pVal);


   switch (dwID) {
   case 0:  // base
      m_pCenter.Copy (&pV);
      break;

   case 1:  // normal
      m_pBottomNorm.Copy (&pV);
      m_pBottomNorm.Subtract (&m_pCenter);
      m_pBottomNorm.Normalize();
      if (m_pBottomNorm.Length()<.5)
         m_pBottomNorm.p[2] = 1;
      break;

   case 2:  // width
   case 3:  // length
      if (m_dwShape) {
         DWORD dwDim = dwID - 2;
         fp fScale = pV.p[dwDim] - pCenter.p[dwDim];
         fScale = max(fScale, CLOSE);
         fScale *= 2;
         m_pShapeSize.p[dwDim] = fScale;
      }
      else {   // any shape
         DWORD dwDim = dwID - 2;
         fp fScale = pV.p[dwDim] - pCenter.p[dwDim];
         fScale = max(fScale, CLOSE);
         fScale *= 2;
         fScale /= pScale.p[dwDim];

         DWORD dwPoints, dwMinDivide, dwMaxDivide;
         BOOL fLooped;
         CMem memPoints, memSegCurve;
         fp fDetail;
         m_sShape.ToMem (&fLooped, &dwPoints, &memPoints, NULL, &memSegCurve,
            &dwMinDivide, &dwMaxDivide, &fDetail);
         PCPoint p;
         p = (PCPoint) memPoints.p;
         if (p) {
            for (i = 0; i < dwPoints; i++)
               p[i].p[dwDim] = (p[i].p[dwDim] - pCenter.p[dwDim]) * fScale + pCenter.p[dwDim];
            m_sShape.Init (fLooped, dwPoints, p, NULL, (DWORD*) memSegCurve.p,
               dwMinDivide, dwMaxDivide, fDetail);
         }

         // also scale the lip
         m_pLip.p[0] *= fScale;
      }
      break;

   default: // edge control points
      CPoint pT;
      pT.Copy (&pV);
      pT.p[2] = 0;
      m_sShape.OrigPointReplace (dwID - 10, &pT);
      break;
   }

   CalcPool();

   // tell the world we've changed
   if (m_pWorld)
      m_pWorld->ObjectChanged (this);
   return TRUE;
}

/*************************************************************************************
CObjectPool::ControlPointEnum - Called to enumerate a list of control point IDs
into the list.

inputs
   PCListFixed       plDWORD - Should be filled in with control points
returns
   none
*/
void CObjectPool::ControlPointEnum (PCListFixed plDWORD)
{
   DWORD i;

   DWORD dwNum;
   
   dwNum = 4;  // 0=base, 1=angle, 2=xscale, 3=yscale
   for (i = 0; i < dwNum; i++)
      plDWORD->Add (&i);

   // If rounded rectangle then different number of control points
   if (!m_dwShape) {
      dwNum = m_sShape.OrigNumPointsGet();
      for (i = 10; i < 10+dwNum; i++)
         plDWORD->Add (&i);
   }
}

/*********************************************************************************
CObjectPool::ShapeToSpline - Looks at the shape and other member variables
and creates the noodle object.
*/
void CObjectPool::ShapeToSpline (PCSpline ps)
{
   if (!m_dwShape)
      return;

   // figure out the path
   CPoint apLoc[16];
   DWORD dwSegCurve[16];
   DWORD dwNum, dwDivide;
   dwNum = 0;
   dwDivide = 3;
   memset (apLoc, 0, sizeof(apLoc));
   memset (dwSegCurve, 0, sizeof(dwSegCurve));

   fp fWidth, fHeight, fCurve;
   fWidth = m_pShapeSize.p[0];
   fHeight = m_pShapeSize.p[1];
   fCurve = min(min(fWidth,fHeight)/2, m_pShapeSize.p[2]);
   fCurve = max(CLOSE,fCurve);

   DWORD i;

   switch (m_dwShape) {
   case 1:  // rectangle
      apLoc[0].p[0] = -fWidth/2;
      apLoc[0].p[1] = -fHeight/2;

      apLoc[1].Copy (&apLoc[0]);
      apLoc[1].p[1] = fHeight/2;

      apLoc[2].Copy (&apLoc[1]);
      apLoc[2].p[0] = fWidth/2;

      apLoc[3].Copy (&apLoc[2]);
      apLoc[3].p[1] = -fHeight/2;

      dwNum = 4;
      dwDivide = 2;
      break;

   case 2:  // rectangle with rounded edges
      apLoc[0].p[0] = -fWidth/2 + fCurve;
      apLoc[0].p[1] = fHeight/2;

      apLoc[1].p[0] = fWidth/2 - fCurve;
      apLoc[1].p[1] = apLoc[0].p[1];
      dwSegCurve[1] = SEGCURVE_ELLIPSENEXT;

      apLoc[2].p[0] = fWidth/2;
      apLoc[2].p[1] = apLoc[1].p[1];
      dwSegCurve[2] = SEGCURVE_ELLIPSEPREV;

      apLoc[3].p[0] = apLoc[2].p[0];
      apLoc[3].p[1] = fHeight/2 - fCurve;
      
      apLoc[4].p[0] = apLoc[3].p[0];
      apLoc[4].p[1] = -fHeight/2 + fCurve;
      dwSegCurve[4] = SEGCURVE_ELLIPSENEXT;

      apLoc[5].p[0] = apLoc[4].p[0];
      apLoc[5].p[1] = -fHeight/2;
      dwSegCurve[5] = SEGCURVE_ELLIPSEPREV;
      
      dwNum = 6;
      dwDivide = 1;

      for (i = 0; i < dwNum; i++) {
         apLoc[6+i].Copy (&apLoc[i]);
         apLoc[6+i].Scale (-1);
         dwSegCurve[6+i] = dwSegCurve[i];
      }
      dwNum *= 2;
      break;

   case 3:  // oval
      apLoc[0].p[0] = -fWidth/2;
      apLoc[0].p[1] = 0;
      dwSegCurve[0] = SEGCURVE_ELLIPSENEXT;

      apLoc[1].Copy (&apLoc[0]);
      apLoc[1].p[1] = fHeight/2;
      dwSegCurve[1] = SEGCURVE_ELLIPSEPREV;

      apLoc[2].Copy (&apLoc[1]);
      apLoc[2].p[0] = 0;
      dwSegCurve[2] = SEGCURVE_ELLIPSENEXT;

      apLoc[3].Copy (&apLoc[2]);
      apLoc[3].p[0] = fWidth/2;
      dwSegCurve[3] = SEGCURVE_ELLIPSEPREV;

      for (i = 4; i < 8; i++) {
         apLoc[i].Copy (&apLoc[i-4]);
         apLoc[i].Scale (-1);
         dwSegCurve[i] = dwSegCurve[i-4];
      }

      dwNum = 8;

      break;

   case 4:  // rounded
      if (fWidth > fHeight+CLOSE) {
         // curved on right and left
         apLoc[0].p[0] = -fWidth/2 + fHeight/2;
         apLoc[0].p[1] = -fHeight/2;
         dwSegCurve[0] = SEGCURVE_CIRCLENEXT;

         apLoc[1].p[0] = -fWidth/2;
         apLoc[1].p[1] = 0;
         dwSegCurve[1] = SEGCURVE_CIRCLEPREV;

         apLoc[2].p[0] = apLoc[0].p[0];
         apLoc[2].p[1] = -apLoc[0].p[1];
         dwSegCurve[2] = SEGCURVE_LINEAR;

         dwNum = 3;
      }
      else if (fHeight > fWidth+CLOSE) {
         // curved on top and bottom
         apLoc[0].p[1] = fHeight/2 - fWidth/2;
         apLoc[0].p[0] = -fWidth/2;
         dwSegCurve[0] = SEGCURVE_CIRCLENEXT;

         apLoc[1].p[1] = fHeight/2;
         apLoc[1].p[0] = 0;
         dwSegCurve[1] = SEGCURVE_CIRCLEPREV;

         apLoc[2].p[1] = apLoc[0].p[1];
         apLoc[2].p[0] = -apLoc[0].p[0];
         dwSegCurve[2] = SEGCURVE_LINEAR;

         dwNum = 3;
      }
      else {
         // completely round
         apLoc[0].p[0] = -fWidth/2;
         apLoc[0].p[1] = 0;
         dwSegCurve[0] = SEGCURVE_CIRCLENEXT;

         apLoc[1].p[1] = fHeight/2;
         apLoc[1].p[0] = 0;
         dwSegCurve[1] = SEGCURVE_CIRCLEPREV;

         dwNum = 2;
      }

      for (i = dwNum; i < dwNum*2; i++) {
         apLoc[i].Copy (&apLoc[i-dwNum]);
         apLoc[i].Scale (-1);
         dwSegCurve[i] = dwSegCurve[i-dwNum];
      }
      dwNum *= 2;
      break;
   }


   // set the curve
   ps->Init (TRUE, dwNum, apLoc, NULL, dwSegCurve, dwDivide, dwDivide, .1);
}


/**********************************************************************************
CObjectPool::TurnOnGet - 
returns how TurnOn the object is, from 0 (closed) to 1.0 (TurnOn), or
< 0 for an object that can't be TurnOned
*/
fp CObjectPool::TurnOnGet (void)
{
   return m_fFilled;
}

/**********************************************************************************
CObjectPool::TurnOnSet - 
TurnOns/closes the object. if fTurnOn==0 it's close, 1.0 = TurnOn, and
values in between are partially TurnOned closed. Returne TRUE if success
*/
BOOL CObjectPool::TurnOnSet (fp fTurnOn)
{
   fTurnOn = max(0,fTurnOn);
   fTurnOn = min(1,fTurnOn);

   m_pWorld->ObjectAboutToChange (this);
   m_fFilled = fTurnOn;
   CalcPool();
   m_pWorld->ObjectChanged (this);

   return TRUE;
}


/* PoolDialogPage
*/
BOOL PoolDialogPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCObjectPool pv = (PCObjectPool) pPage->m_pUserData;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         ComboBoxSet (pPage, L"shape", pv->m_dwShape);

         // enable/disable modify custom shape
         PCEscControl pControl;
         pControl = pPage->ControlFind (L"custom");
         if (pControl)
            pControl->Enable (!pv->m_dwShape);

         MeasureToString (pPage, L"shapesize3", pv->m_pShapeSize.p[2]);
         MeasureToString (pPage, L"holesize", pv->m_fHoleSize);
         MeasureToString (pPage, L"lip0", pv->m_pLip.p[0]);
         MeasureToString (pPage, L"lip1", pv->m_pLip.p[1]);

         // scrolling
         pControl = pPage->ControlFind (L"wallslope");
         if (pControl)
            pControl->AttribSetInt (Pos(), (int)(pv->m_fWallSlope * 100));
         pControl = pPage->ControlFind (L"wallrounded");
         if (pControl)
            pControl->AttribSetInt (Pos(), (int)(pv->m_fWallRounded * 100));

         // radio button
         if (pv->m_fFitToEmbed)
            pControl = pPage->ControlFind (L"lipnone");
         else if (pv->m_fRectangular)
            pControl = pPage->ControlFind (L"liprect");
         else
            pControl = pPage->ControlFind (L"liparound");
         if (pControl)
            pControl->AttribSetBOOL (Checked(), TRUE);
      }
      break;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;

         if (!p->pControl->m_pszName)
            break;
         PWSTR psz;
         psz = p->pControl->m_pszName;

         BOOL fNewFit, fNewRect;
         fNewFit = pv->m_fFitToEmbed;
         fNewRect = pv->m_fRectangular;

         PCEscControl pControl;
         pControl = pPage->ControlFind (L"lipnone");
         if (pControl && pControl->AttribGetBOOL (Checked()))
            fNewFit = TRUE;
         pControl = pPage->ControlFind (L"liprect");
         if (pControl && pControl->AttribGetBOOL (Checked())) {
            fNewFit = FALSE;
            fNewRect = TRUE;
         }
         pControl = pPage->ControlFind (L"liparound");
         if (pControl && pControl->AttribGetBOOL (Checked())) {
            fNewFit = FALSE;
            fNewRect = FALSE;
         }

         if ((fNewFit != pv->m_fFitToEmbed) || (fNewRect != pv->m_fRectangular)) {
            pv->m_pWorld->ObjectAboutToChange (pv);
            pv->m_fFitToEmbed = fNewFit;
            pv->m_fRectangular = fNewRect;
            pv->CalcPool();
            pv->m_pWorld->ObjectChanged (pv);
         }
      }
      break;

   case ESCN_SCROLL:
   case ESCN_SCROLLING:
      {
         PESCNSCROLL p = (PESCNSCROLL) pParam;

         // only do one scroll bar
         PWSTR psz;
         psz = p->pControl->m_pszName;
         if (!psz)
            break;

         // get value
         fp fVal, *pf;
         fVal = p->pControl->AttribGetInt (Pos()) / 100.0;
         pf = NULL;

         if (!_wcsicmp(psz, L"wallslope"))
            pf = &pv->m_fWallSlope;
         else if (!_wcsicmp(psz, L"wallrounded"))
            pf = &pv->m_fWallRounded;

         if (!pf || (*pf == fVal))
            break;   // no change

         pv->m_pWorld->ObjectAboutToChange (pv);
         *pf = fVal;
         pv->CalcPool();
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
         DWORD dwVal;
         dwVal = p->pszName ? (DWORD) _wtoi(p->pszName) : 0;

         if (!_wcsicmp(psz, L"shape")) {
            if (dwVal == pv->m_dwShape)
               break;

            pv->m_pWorld->ObjectAboutToChange (pv);
            pv->m_dwShape = dwVal;
            pv->CalcPool();
            pv->m_pWorld->ObjectChanged (pv);

            // enable/disable modify custom shape
            PCEscControl pControl;
            pControl = pPage->ControlFind (L"custom");
            if (pControl)
               pControl->Enable (!pv->m_dwShape);

            return TRUE;
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

         // since any edit change will result in redraw, get them all
         pv->m_pWorld->ObjectAboutToChange (pv);

         MeasureParseString (pPage, L"shapesize3", &pv->m_pShapeSize.p[2]);
         pv->m_pShapeSize.p[2] = max(CLOSE,pv->m_pShapeSize.p[2]);
         MeasureParseString (pPage, L"holesize", &pv->m_fHoleSize);
         pv->m_fHoleSize = max(0,pv->m_fHoleSize);
         MeasureParseString (pPage, L"lip0", &pv->m_pLip.p[0]);
         pv->m_pLip.p[0] = max(CLOSE, pv->m_pLip.p[0]);
         MeasureParseString (pPage, L"lip1", &pv->m_pLip.p[1]);
         pv->m_pLip.p[1] = max(0, pv->m_pLip.p[1]);

         pv->CalcPool ();
         pv->m_pWorld->ObjectChanged (pv);


         break;
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Pool settings";
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}


/* PoolCurvePage
*/
BOOL PoolCurvePage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCObjectPool pv = (PCObjectPool)pPage->m_pUserData;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         PCEscControl pControl;

         // draw images
         PCSpline ps;
         ps = &pv->m_sShape;
         if (ps) {
            NoodGenerateThreeDFromSpline (L"edgeaddremove", pPage, ps, ps, 0, FALSE);
            NoodGenerateThreeDFromSpline (L"edgecurve", pPage, ps, ps, 1, FALSE);

            pControl = pPage->ControlFind (L"looped");
            if (pControl)
               pControl->AttribSetBOOL (Checked(), ps->LoopedGet());

            DWORD dwMin, dwMax;
            fp fDetail;
            ps->DivideGet (&dwMin, &dwMax, &fDetail);
            ComboBoxSet (pPage, L"divide", dwMax);
         }
      }
      break;

   case ESCN_COMBOBOXSELCHANGE:
      {
         PESCNCOMBOBOXSELCHANGE p = (PESCNCOMBOBOXSELCHANGE) pParam;

         if (!p->pControl || !p->pControl->m_pszName)
            break;
         DWORD dwVal;
         dwVal = p->pszName ? (DWORD) _wtoi(p->pszName) : 0;
         PWSTR psz;
         psz = p->pControl->m_pszName;

         if (!_wcsicmp(psz, L"divide")) {
            PCSpline ps;
            ps = &pv->m_sShape;
            if (!ps)
               return TRUE;
            DWORD dwMin, dwMax;
            fp fDetail;
            ps->DivideGet (&dwMin, &dwMax, &fDetail);
            if (dwVal == dwMax)
               return TRUE;   // nothing to change

            // get all the points
            CMem  memPoints, memSegCurve;
            DWORD dwOrig;
            BOOL fLooped;
            if (!ps->ToMem (&fLooped, &dwOrig, &memPoints, NULL, &memSegCurve, &dwMin, &dwMax, &fDetail))
               return FALSE;

            pv->m_pWorld->ObjectAboutToChange (pv);
            pv->m_sShape.Init (TRUE, dwOrig,
               (PCPoint) memPoints.p, NULL, (DWORD*) memSegCurve.p, dwVal, dwVal, .1);
            pv->CalcPool();
            pv->m_pWorld->ObjectChanged (pv);
            return TRUE;
         }
      }

   case ESCN_THREEDCLICK:
      {
         PESCNTHREEDCLICK p = (PESCNTHREEDCLICK) pParam;
         if (!p->pControl->m_pszName)
            break;

         BOOL  fCol;
         if (!_wcsicmp(p->pControl->m_pszName, L"edgeaddremove"))
            fCol = TRUE;
         else if (!_wcsicmp(p->pControl->m_pszName, L"edgecurve"))
            fCol = FALSE;
         else
            break;

         // figure out x, y, and what clicked on
         DWORD x, dwMode;
         dwMode = (BYTE)(p->dwMajor >> 16);
         x = (BYTE) p->dwMajor;
         if ((dwMode < 1) || (dwMode > 2))
            break;

         // get all the points
         CMem  memPoints, memSegCurve;
         DWORD dwMinDivide, dwMaxDivide, dwOrig;
         fp fDetail;
         BOOL fLooped;
         PCSpline ps;
         ps = &pv->m_sShape;
         if (!ps)
            return FALSE;
         if (!ps->ToMem (&fLooped, &dwOrig, &memPoints, NULL, &memSegCurve, &dwMinDivide, &dwMaxDivide, &fDetail))
            return FALSE;

         // make sure have enough memory for extra
         if (!memPoints.Required ((dwOrig+1) * sizeof(CPoint)))
            return TRUE;
         if (!memSegCurve.Required ((dwOrig+1) * sizeof(DWORD)))
            return TRUE;

         // load it in
         PCPoint paPoints;
         DWORD *padw;
         paPoints = (PCPoint) memPoints.p;
         padw = (DWORD*) memSegCurve.p;

         if (fCol) {
            if (dwMode == 1) {
               // inserting
               memmove (paPoints + (x+1), paPoints + x, sizeof(CPoint) * (dwOrig-x));
               paPoints[x+1].Add (paPoints + ((x+2) % (dwOrig+1)));
               paPoints[x+1].Scale (.5);
               memmove (padw + (x+1), padw + x, sizeof(DWORD) * (dwOrig - x));
               dwOrig++;
            }
            else if (dwMode == 2) {
               // deleting
               memmove (paPoints + x, paPoints + (x+1), sizeof(CPoint) * (dwOrig-x-1));
               memmove (padw + x, padw + (x+1), sizeof(DWORD) * (dwOrig - x - 1));
               dwOrig--;
            }
         }
         else {
            // setting curvature
            if (dwMode == 1) {
               padw[x] = (padw[x] + 1) % (SEGCURVE_MAX+1);
            }
         }

         pv->m_pWorld->ObjectAboutToChange (pv);
         //pv->m_dwDisplayControl = 3;   // front
         pv->m_sShape.Init (TRUE, dwOrig, paPoints, NULL, padw, dwMaxDivide, dwMaxDivide, .1);
         pv->CalcPool();
         pv->m_pWorld->ObjectChanged (pv);

         // redraw the shapes
         ps = &pv->m_sShape;
         if (ps) {
            NoodGenerateThreeDFromSpline (L"edgeaddremove", pPage, ps, ps, 0, FALSE);
            NoodGenerateThreeDFromSpline (L"edgecurve", pPage, ps, ps, 1, FALSE);
         }
      }
      break;


   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Pool curve";
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"IFUSELOOP")) {
            p->pszSubString = L"<comment>";
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"ENDIFUSELOOP")) {
            p->pszSubString = L"</comment>";
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}


/**********************************************************************************
CObjectPool::DialogShow - Causes the object to show a dialog box that allows
different attributes to be changed.

inputs
   DWORD             dwSuface - Surface that was clicked on. 0 if unknown.
   PCEscWindow       pWindow - Escarpment window to draw it in
returns
   BOOL - TRUE if user pressed "Back", FALSE if user pressed "Close"
*/
BOOL CObjectPool::DialogShow (DWORD dwSurface, PCEscWindow pWindow, PCPoint pClick)
{
   PWSTR pszRet;
mainpage:
   pszRet = pWindow->PageDialog (ghInstance, IDR_MMLPOOLDIALOG, PoolDialogPage, this);
   if (!pszRet)
      return FALSE;
   if (!_wcsicmp(pszRet, L"custom")) {
      pszRet = pWindow->PageDialog (ghInstance, IDR_MMLNOODLECURVE, PoolCurvePage, this);
      if (!pszRet)
         return FALSE;
      if (!_wcsicmp(pszRet, Back()))
         goto mainpage;
      // else fall through
   }

   return !_wcsicmp(pszRet, Back());
}


/**********************************************************************************
CObjectPool::DialogQuery - Returns TRUE if the object supports a custom
dialog box, FALSE if it doesn't. The default object doesn't
*/
BOOL CObjectPool::DialogQuery (void)
{
   return TRUE;
}


/**********************************************************************************
CObjectPool::EmbedDoCutout - Member function specific to the template. Called
when the object has moved within the surface. This enables the super-class for
the embedded object to pass a cutout into the container. (Basically, specify the
hole for the window or door)
*/
BOOL CObjectPool::EmbedDoCutout (void)
{
   GUID gCont;
   if (!m_pWorld || !EmbedContainerGet (&gCont))
      return FALSE;
   if (m_fFitToEmbed)
      CalcPool(); // need to recalc all
   else
      return MyEmbedDoCutout();  // only recalcuate the cutout

   return TRUE;
}


/**********************************************************************************
CObjectPool::MyEmbedDoCutout - Member function specific to the template. Called
when the object has moved within the surface. This enables the super-class for
the embedded object to pass a cutout into the container. (Basically, specify the
hole for the window or door)
*/
BOOL CObjectPool::MyEmbedDoCutout (void)
{
   // find the surface
   GUID gCont;
   PCObjectSocket pos;
   if (!m_pWorld || !EmbedContainerGet (&gCont))
      return FALSE;
   pos = m_pWorld->ObjectGet (m_pWorld->ObjectFind (&gCont));
   if (!pos)
      return FALSE;

   // will need to transform from this object space into the container space
   CMatrix mCont, mTrans;
   pos->ObjectMatrixGet (&mCont);
   mCont.Invert4 (&mTrans);
   mTrans.MultiplyLeft (&m_MatrixObject);
   mCont.RotationX (PI/2);
   mTrans.MultiplyLeft (&mCont); // rotate by PI/2 since embedded

   CListFixed lFront, lBack;
   lFront.Init (sizeof(CPoint));
   lBack.Init (sizeof(CPoint));
   DWORD i;
   CPoint pt;
   lFront.Required (m_sShape.QueryNodes());
   lBack.Required (m_sShape.QueryNodes());
   for (i = 0; i < m_sShape.QueryNodes(); i++) {
      pt.Copy (m_sShape.LocationGet(i));
      pt.p[3] = 1;
      pt.MultiplyLeft (&mTrans);
      lFront.Add (&pt);

      pt.Copy (m_sShape.LocationGet(i));
      pt.p[3] = 1;
      pt.p[2] -= m_fContainerDepth + .01;
      pt.MultiplyLeft (&mTrans);
      lBack.Add (&pt);
   }
   //lFront.Init (sizeof(CPoint), m_lPoolEdge.Get(0), m_lPoolEdge.Num());
   //lBack.Init (sizeof(CPoint), m_lPoolEdge.Get(0), m_lPoolEdge.Num());
   //p = ((PCPoint)lFront.Get(0));
   //for (i = 0; i < lFront.Num(); i++) {
   //   p[i].p[3] = 1;
   //   p[i].MultiplyLeft (&mTrans);
   // }
   //p = ((PCPoint)lBack.Get(0));
   //for (i = 0; i < lBack.Num(); i++) {
   //   p[i].p[3] = 1;
   //   p[i].MultiplyLeft (&mTrans);
   //}
   pos->ContCutout (&m_gGUID, lFront.Num(), (PCPoint) lFront.Get(0), (PCPoint) lBack.Get(0), TRUE);

   return TRUE;
}



// BUGBUG - Pond normals seem to be wrong when drawn in the ground.
// also, ground seems not to draw properly around when not in RT mode

// BUGBUG - when ponds drawn in RT can see the polygonization of water
// maybe because using quads instead of triangles?

