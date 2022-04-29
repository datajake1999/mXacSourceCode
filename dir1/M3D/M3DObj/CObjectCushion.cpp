/************************************************************************
CObjectCushion.cpp - Draws a Cushion.

begun 6/9/02 by Mike Rozak
Copyright 2002 Mike Rozak. All rights reserved
*/

#include <windows.h>
#include "escarpment.h"
#include "..\M3D.h"
#include "resource.h"


typedef struct {
   PWSTR       pszName;    // name of the move point
   int         iX, iY; // -1 if for left/top, 1 for right/bottom
} SPLINEMOVEP, *PSPLINEMOVEP;

static SPLINEMOVEP   gaCushionMove[] = {
   L"Center", 0, 0
};


/**********************************************************************************
CObjectCushion::Constructor and destructor */
CObjectCushion::CObjectCushion (PVOID pParams, POSINFO pInfo)
{
   m_dwRenderShow = RENDERSHOW_FURNITURE;
   m_OSINFO = *pInfo;

   m_dwShape = 1;
   m_pCenter.Zero();
   m_pSize.Zero();
   m_pSize.p[0] = .6;
   m_pSize.p[1] = .6;
   m_pSize.p[2] = .2;
   m_fDetail = 0.05;
   m_afBulge[0] = .1;
   m_afBulgeDist[0] = .3;
   m_afBulge[1] = .2;
   m_afBulgeDist[1] = .5;

   m_pCutout.Zero();
   m_pCutout.p[0] = -m_pSize.p[0] / 8;
   m_pCutout.p[1] = -m_pSize.p[1] / 8;
   m_fRounded = .05;

   m_pButtonDist.Zero();
   m_pButtonDist.p[0] = m_pButtonDist.p[1] = .3;
   m_dwButtonNumX = m_dwButtonNumY = 0;
   m_dwButtonStagger = 0;

   m_dwNumPoints = m_dwNumNormals = m_dwNumText = 0;
   m_dwNumVertex = m_dwNumPoly = 0;
   m_dwEdgeNumPoints = m_dwEdgeNumNormals = m_dwEdgeNumText = 0;
   m_dwEdgeNumVertex = m_dwEdgeNumPoly = 0;

   // calculate
   CalcInfo();

   // color for the Cushion
   PWSTR pszScheme = L"Cushions";
   ObjectSurfaceAdd (40, RGB(0xff,0xff,0x40), MATERIAL_CLOTHSMOOTH, pszScheme);
   ObjectSurfaceAdd (41, RGB(0xff,0xff,0x40), MATERIAL_CLOTHSMOOTH, pszScheme);
   ObjectSurfaceAdd (42, RGB(0xff,0xff,0x40), MATERIAL_CLOTHSMOOTH, pszScheme);
}


CObjectCushion::~CObjectCushion (void)
{
   // do nothing for now
}


/**********************************************************************************
CObjectCushion::Delete - Called to delete this object
*/
void CObjectCushion::Delete (void)
{
   delete this;
}

/**********************************************************************************
CObjectCushion::Render - Draws a test box.

inputs
   POBJECTRENDER     pr - Render information
*/
void CObjectCushion::Render (POBJECTRENDER pr, DWORD dwSubObject)
{
   // create the surface render object and draw
   // CRenderSurface rs;
   m_Renderrs.ClearAll();

   CMatrix mObject;
   m_Renderrs.Init (pr->pRS);
   ObjectMatrixGet (&mObject);
   m_Renderrs.Multiply (&mObject);

   // top
   m_Renderrs.Push();
   m_Renderrs.Translate (0, 0, m_pSize.p[2] / 2);
   RenderCushion (pr, &m_Renderrs, FALSE);
   m_Renderrs.Pop();

   // bottom

   m_Renderrs.Push();
   m_Renderrs.Translate (0, 0, -m_pSize.p[2] / 2);
   RenderCushion (pr, &m_Renderrs, TRUE);
   m_Renderrs.Pop();

   if (m_dwEdgeNumPoly) {
      // draw the edge
      m_Renderrs.SetDefMaterial (m_OSINFO.dwRenderShard, ObjectSurfaceFind (42), m_pWorld);

      PCPoint pPoint, pNormal;
      PTEXTPOINT5 pText;
      PVERTEX pVert;
      DWORD dwPointIndex, dwNormalIndex, dwTextIndex, dwVertIndex;

      DWORD i;
      pPoint = m_Renderrs.NewPoints (&dwPointIndex, m_dwEdgeNumPoints);
      if (pPoint)
         memcpy (pPoint, m_memEdgePoints.p, m_dwEdgeNumPoints * sizeof(CPoint));
      pNormal = m_Renderrs.NewNormals (TRUE, &dwNormalIndex, m_dwEdgeNumNormals);
      if (pNormal)
         memcpy (pNormal, m_memEdgeNormals.p, m_dwEdgeNumNormals * sizeof(CPoint));
      pText = m_Renderrs.NewTextures (&dwTextIndex, m_dwEdgeNumText);
      if (pText) {
         memcpy (pText, m_memEdgeText.p, m_dwEdgeNumText * sizeof(TEXTPOINT5));
         m_Renderrs.ApplyTextureRotation (pText, m_dwEdgeNumText);
      }
      pVert = m_Renderrs.NewVertices (&dwVertIndex, m_dwEdgeNumVertex);
      if (pVert) {
         PVERTEX pvs, pvd;
         pvs = (PVERTEX) m_memEdgeVertex.p;
         pvd = pVert;
         for (i = 0; i < m_dwEdgeNumVertex; i++, pvs++, pvd++) {
            pvd->dwNormal = pNormal ? (pvs->dwNormal + dwNormalIndex) : 0;
            pvd->dwPoint = pvs->dwPoint + dwPointIndex;
            pvd->dwTexture = pText ? (pvs->dwTexture + dwTextIndex) : 0;
         }
      }

      DWORD *padw;
      padw = (DWORD*) m_memEdgePoly.p;
      for (i = 0; i < m_dwEdgeNumPoly; i++, padw += 4) {
         if (padw[3] == -1)
            m_Renderrs.NewTriangle (dwVertIndex + padw[0], dwVertIndex + padw[1],
               dwVertIndex + padw[2], TRUE);
         else
            m_Renderrs.NewQuad (dwVertIndex + padw[0], dwVertIndex + padw[1],
               dwVertIndex + padw[2], dwVertIndex + padw[3], TRUE);
      }
   }  // draw edge

   m_Renderrs.Commit();
}



/**********************************************************************************
CObjectCushion::QueryBoundingBox - Standard API
*/
void CObjectCushion::QueryBoundingBox (PCPoint pCorner1, PCPoint pCorner2, DWORD dwSubObject)
{
   pCorner2->Copy (&m_pSize);
   pCorner2->Scale(0.5);
   pCorner2->p[0] += m_afBulge[1] * m_pSize.p[2] / 2;
   pCorner2->p[1] += m_afBulge[1] * m_pSize.p[2] / 2;
   pCorner2->p[2] += m_afBulge[0] * max(m_pSize.p[0], m_pSize.p[1]) / 2;
   pCorner1->Copy (pCorner2);
   pCorner1->Scale(-1);

#ifdef _DEBUG
   // test, make sure bounding box not too small
   CPoint p1,p2;
   DWORD i;
   CObjectTemplate::QueryBoundingBox (&p1, &p2, dwSubObject);
   for (i = 0; i < 3; i++)
      if ((p1.p[i] < pCorner1->p[i] - CLOSE) || (p2.p[i] > pCorner2->p[i] + CLOSE))
         break;
   if (i < 3)
      OutputDebugString ("\r\nCObjectCushion::QueryBoundingBox too small.");
#endif
}

/**********************************************************************************
CObjectCushion::RenderCushion - Draws the curtain in m_memPoints, etc.

inputs
   POBJECTRENDER     pr - Render information
   PCRenderSurface   prs - Render surface
   BOOL              fFlip - if TRUE, flip curtain on X axis
*/
void CObjectCushion::RenderCushion (POBJECTRENDER pr, PCRenderSurface prs, BOOL fFlip)
{
   PCPoint pPoint, pNormal;
   PTEXTPOINT5 pText;
   PVERTEX pVert;
   DWORD dwPointIndex, dwNormalIndex, dwTextIndex, dwVertIndex;

   // object specific
   prs->SetDefMaterial (m_OSINFO.dwRenderShard, ObjectSurfaceFind (fFlip ? 41 : 40), m_pWorld);

   DWORD i;
   pPoint = prs->NewPoints (&dwPointIndex, m_dwNumPoints);
   if (pPoint) {
      memcpy (pPoint, m_memPoints.p, m_dwNumPoints * sizeof(CPoint));

      if (fFlip) for (i = 0; i < m_dwNumPoints; i++)
         pPoint[i].p[2] *= -1;
   }

   pNormal = prs->NewNormals (TRUE, &dwNormalIndex, m_dwNumNormals);
   if (pNormal) {
      memcpy (pNormal, m_memNormals.p, m_dwNumNormals * sizeof(CPoint));

      if (fFlip) for (i = 0; i < m_dwNumNormals; i++)
         pNormal[i].p[2] *= -1;
   }

   pText = prs->NewTextures (&dwTextIndex, m_dwNumText);
   if (pText) {
      memcpy (pText, m_memText.p, m_dwNumText * sizeof(TEXTPOINT5));

      if (fFlip) for (i = 0; i < m_dwNumText; i++)
         pText[i].hv[0] *= -1;

      prs->ApplyTextureRotation (pText, m_dwNumText);
   }

   pVert = prs->NewVertices (&dwVertIndex, m_dwNumVertex);
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
         if (fFlip)
            prs->NewTriangle (dwVertIndex + padw[2], dwVertIndex + padw[1],
               dwVertIndex + padw[0], FALSE);
         else
            prs->NewTriangle (dwVertIndex + padw[0], dwVertIndex + padw[1],
               dwVertIndex + padw[2], FALSE);
      }
      else {
         if (fFlip)
            prs->NewQuad (dwVertIndex + padw[3], dwVertIndex + padw[2],
               dwVertIndex + padw[1], dwVertIndex + padw[0], TRUE);
         else
            prs->NewQuad (dwVertIndex + padw[0], dwVertIndex + padw[1],
               dwVertIndex + padw[2], dwVertIndex + padw[3], TRUE);
      }
   }
   
}


/**********************************************************************************
CObjectCushion::Clone - Clones this

inputs
   none
returns
   PCObjectSocket - Clone
*/
PCObjectSocket CObjectCushion::Clone (void)
{
   PCObjectCushion pNew;

   pNew = new CObjectCushion(NULL, &m_OSINFO);

   // clone template info
   CloneTemplate(pNew);

   pNew->m_dwShape = m_dwShape;
   pNew->m_pSize.Copy (&m_pSize);
   pNew->m_pCenter.Copy (&m_pCenter);
   pNew->m_fDetail = m_fDetail;
   memcpy (pNew->m_afBulge, m_afBulge, sizeof(m_afBulge));
   memcpy (pNew->m_afBulgeDist, m_afBulgeDist, sizeof(m_afBulgeDist));
   pNew->m_pButtonDist.Copy (&m_pButtonDist);
   pNew->m_dwButtonNumX = m_dwButtonNumX;
   pNew->m_dwButtonNumY = m_dwButtonNumY;
   pNew->m_dwButtonStagger = m_dwButtonStagger;

   pNew->m_dwNumPoints = m_dwNumPoints;
   pNew->m_dwNumNormals = m_dwNumNormals;
   pNew->m_dwNumText = m_dwNumText;
   pNew->m_dwNumVertex = m_dwNumVertex;
   pNew->m_dwNumPoly = m_dwNumPoly;

   pNew->m_dwEdgeNumPoints = m_dwEdgeNumPoints;
   pNew->m_dwEdgeNumNormals = m_dwEdgeNumNormals;
   pNew->m_dwEdgeNumText = m_dwEdgeNumText;
   pNew->m_dwEdgeNumVertex = m_dwEdgeNumVertex;
   pNew->m_dwEdgeNumPoly = m_dwEdgeNumPoly;

   pNew->m_pCutout.Copy (&m_pCutout);
   pNew->m_fRounded = m_fRounded;
   m_Spline.CloneTo (&pNew->m_Spline);

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

   if (pNew->m_memEdgePoints.Required(dwNeed = m_dwEdgeNumPoints * sizeof(CPoint)))
      memcpy (pNew->m_memEdgePoints.p, m_memEdgePoints.p, dwNeed);
   if (pNew->m_memEdgeNormals.Required(dwNeed = m_dwEdgeNumNormals * sizeof(CPoint)))
      memcpy (pNew->m_memEdgeNormals.p, m_memEdgeNormals.p, dwNeed);
   if (pNew->m_memEdgeText.Required(dwNeed = m_dwEdgeNumText * sizeof(TEXTPOINT5)))
      memcpy (pNew->m_memEdgeText.p, m_memEdgeText.p, dwNeed);
   if (pNew->m_memEdgeVertex.Required(dwNeed = m_dwEdgeNumVertex * sizeof(VERTEX)))
      memcpy (pNew->m_memEdgeVertex.p, m_memEdgeVertex.p, dwNeed);
   if (pNew->m_memEdgePoly.Required(dwNeed = m_dwEdgeNumPoly * sizeof(DWORD)*4))
      memcpy (pNew->m_memEdgePoly.p, m_memEdgePoly.p, dwNeed);
   return pNew;
}

static PWSTR gpszShape = L"Shape";
static PWSTR gpszSize = L"Size";
static PWSTR gpszDetail = L"Detail";
static PWSTR gpszCenter = L"Center";
static PWSTR gpszBulge0 = L"Bulge0";
static PWSTR gpszBulgeDist0 = L"BulgeDist0";
static PWSTR gpszBulge1 = L"Bulge1";
static PWSTR gpszBulgeDist1 = L"BulgeDist1";
static PWSTR gpszButtonDist = L"ButtonDist";
static PWSTR gpszButtonStagger = L"ButtonStagger";
static PWSTR gpszButtonNumX = L"ButtonNumX";
static PWSTR gpszButtonNumY = L"ButtonNumY";
static PWSTR gpszCutout = L"Cutout";
static PWSTR gpszRounded = L"Rounded";
static PWSTR gpszSpline = L"Spline";

PCMMLNode2 CObjectCushion::MMLTo (void)
{
   // call into the template first
   PCMMLNode2 pNode = MMLToTemplate();
   if (!pNode)
      return NULL;

   MMLValueSet (pNode, gpszCenter, &m_pCenter);
   MMLValueSet (pNode, gpszShape, (int) m_dwShape);
   MMLValueSet (pNode, gpszSize, &m_pSize);
   MMLValueSet (pNode, gpszDetail, m_fDetail);
   MMLValueSet (pNode, gpszBulge0, m_afBulge[0]);
   MMLValueSet (pNode, gpszBulgeDist0, m_afBulgeDist[0]);
   MMLValueSet (pNode, gpszBulge1, m_afBulge[1]);
   MMLValueSet (pNode, gpszBulgeDist1, m_afBulgeDist[1]);

   MMLValueSet (pNode, gpszButtonDist, &m_pButtonDist);
   MMLValueSet (pNode, gpszButtonStagger, (int) m_dwButtonStagger);
   MMLValueSet (pNode, gpszButtonNumX, (int) m_dwButtonNumX);
   MMLValueSet (pNode, gpszButtonNumY, (int) m_dwButtonNumY);

   MMLValueSet (pNode, gpszCutout, &m_pCutout);
   MMLValueSet (pNode, gpszRounded, m_fRounded);
   if (!m_dwShape) {
      PCMMLNode2 pSub = m_Spline.MMLTo();
      if (pSub) {
         pSub->NameSet (gpszSpline);
         pNode->ContentAdd (pSub);
      }
   }

   return pNode;
}

BOOL CObjectCushion::MMLFrom (PCMMLNode2 pNode)
{
   // call into the template first
   if (!MMLFromTemplate (pNode))
      return FALSE;

   m_dwShape = (DWORD) MMLValueGetInt (pNode, gpszShape, (int) 0);
   MMLValueGetPoint (pNode, gpszSize, &m_pSize);
   MMLValueGetPoint (pNode, gpszCenter, &m_pCenter);
   m_fDetail = MMLValueGetDouble (pNode, gpszDetail, 0);
   m_afBulge[0] = MMLValueGetDouble (pNode, gpszBulge0, 0);
   m_afBulgeDist[0] = MMLValueGetDouble (pNode, gpszBulgeDist0, 0);
   m_afBulge[1] = MMLValueGetDouble (pNode, gpszBulge1, 0);
   m_afBulgeDist[1] = MMLValueGetDouble (pNode, gpszBulgeDist1, 0);

   MMLValueGetPoint (pNode, gpszButtonDist, &m_pButtonDist);
   m_dwButtonStagger = (DWORD) MMLValueGetInt (pNode, gpszButtonStagger, (int) 0);
   m_dwButtonNumX = (DWORD) MMLValueGetInt (pNode, gpszButtonNumX, (int) 0);
   m_dwButtonNumY = (DWORD) MMLValueGetInt (pNode, gpszButtonNumY, (int) 0);

   MMLValueGetPoint (pNode, gpszCutout, &m_pCutout);
   m_fRounded = MMLValueGetDouble (pNode, gpszRounded, 0);
   if (!m_dwShape) {
      PCMMLNode2 pSub;
      PWSTR psz;
      pSub = NULL;
      pNode->ContentEnum (pNode->ContentFind (gpszSpline), &psz, &pSub);
      if (pSub)
         m_Spline.MMLFrom (pSub);
   }
   CalcInfo ();
   return TRUE;
}

/*********************************************************************************
CObjectCushion::ShapeToSpline - Looks at the shape and other member variables
and creates the noodle object.
*/
void CObjectCushion::ShapeToSpline (PCSpline ps)
{
   // how much rounded
   DWORD dwDivide;
   fp fRound;
   DWORD i;

   fRound = max(m_pSize.p[0], m_pSize.p[1]) * m_fRounded;

   // skip filling in shape if custom
   if (!m_dwShape) {
      // find out size
      CPoint pMin, pMax, pVal;
      pMin.Zero();
      pMax.Zero();
      for (i = 0; i < m_Spline.OrigNumPointsGet(); i++) {
         m_Spline.OrigPointGet (i, &pVal);
         pMin.Min (&pVal);
         pMax.Max (&pVal);
      }
      m_pSize.p[0] = max(fabs(pMin.p[0]*2), fabs(pMax.p[0]*2));
      m_pSize.p[1] = max(fabs(pMin.p[1]*2), fabs(pMax.p[1]*2));

      // recalc rounded size
      fRound = max(m_pSize.p[0], m_pSize.p[1]) * m_fRounded;
      goto roundit;
   }

   // figure out the path
   CPoint apLoc[32];
   DWORD dwSegCurve[32];
   DWORD dwNum;
   dwNum = 0;
   dwDivide = 3;
   memset (apLoc, 0, sizeof(apLoc));
   memset (dwSegCurve, 0, sizeof(dwSegCurve));

   fp fWidth, fHeight;
   fWidth = m_pSize.p[0];
   fHeight = m_pSize.p[1];

   switch (m_dwShape) {
   case 1:  // rectangle
   default:
      apLoc[0].p[0] = -fWidth/2;
      apLoc[0].p[1] = -fHeight/2;

      apLoc[1].Copy (&apLoc[0]);
      apLoc[1].p[1] = fHeight/2;

      apLoc[2].Copy (&apLoc[1]);
      apLoc[2].p[0] = fWidth/2;

      apLoc[3].Copy (&apLoc[2]);
      apLoc[3].p[1] = -fHeight/2;

      dwNum = 4;
      dwDivide = 0;
      break;

   case 2:  // pinched
      fRound = min(min(fWidth, fHeight)/2, fRound);

      apLoc[0].p[0] = -fWidth/2; // LL corner
      apLoc[0].p[1] = -fHeight/2;
      dwSegCurve[0] = SEGCURVE_ELLIPSENEXT;

      apLoc[1].p[0] = -fWidth/2 + fRound;
      apLoc[1].p[1] = -fHeight/4;
      dwSegCurve[1] = SEGCURVE_ELLIPSEPREV;

      apLoc[2].p[0] = -fWidth/2 + fRound;
      apLoc[2].p[1] = 0;
      dwSegCurve[2] = SEGCURVE_ELLIPSENEXT;

      apLoc[3].Copy (&apLoc[1]);
      apLoc[3].p[1] *= -1;
      dwSegCurve[3] = SEGCURVE_ELLIPSEPREV;

      apLoc[4].Copy (&apLoc[0]); // UL corner
      apLoc[4].p[1] *= -1;
      dwSegCurve[4] = SEGCURVE_ELLIPSENEXT;

      apLoc[5].p[0] = -fWidth/4;
      apLoc[5].p[1] = fHeight/2 - fRound;
      dwSegCurve[5] = SEGCURVE_ELLIPSEPREV;

      apLoc[6].p[0] = 0;
      apLoc[6].p[1] = fHeight/2 - fRound;
      dwSegCurve[6] = SEGCURVE_ELLIPSENEXT;

      apLoc[7].Copy (&apLoc[5]);
      apLoc[7].p[0] *= -1;
      dwSegCurve[7] = SEGCURVE_ELLIPSEPREV;

      for (i = 8; i < 16; i++) {
         apLoc[i].Copy (&apLoc[i-8]);
         apLoc[i].Scale (-1);
         dwSegCurve[i] = dwSegCurve[i-8];
      }

      dwNum = 16;
      break;

   case 3:  // L
      apLoc[0].p[0] = m_pCutout.p[0];
      apLoc[0].p[1] = -fHeight/2;

      apLoc[1].Copy (&m_pCutout);

      apLoc[2].p[0] = -fWidth/2;
      apLoc[2].p[1] = m_pCutout.p[1];

      apLoc[3].p[0] = -fWidth/2;
      apLoc[3].p[1] = fHeight/2;

      apLoc[4].p[0] = fWidth/2;
      apLoc[4].p[1] = fHeight/2;

      apLoc[5].p[0] = fWidth/2;
      apLoc[5].p[1] = -fHeight/2;

      dwNum = 6;
      
      break;

   case 4:  // T
      apLoc[0].p[0] = m_pCutout.p[0];
      apLoc[0].p[1] = -fHeight/2;

      apLoc[1].Copy (&m_pCutout);

      apLoc[2].p[0] = -fWidth/2;
      apLoc[2].p[1] = m_pCutout.p[1];

      apLoc[3].p[0] = -fWidth/2;
      apLoc[3].p[1] = fHeight/2;

      for (i = 4; i < 8; i++) {
         apLoc[i].Copy (&apLoc[7-i]);
         apLoc[i].p[0] *= -1;
         dwSegCurve[i] = dwSegCurve[7-i];
      }

      dwNum = 8;
      
      break;

   case 6:  // armchair side
      apLoc[0].p[0] = m_pCutout.p[0];
      apLoc[0].p[1] = -fHeight/2;

      apLoc[1].Copy (&m_pCutout);
      dwSegCurve[1] = SEGCURVE_ELLIPSENEXT;

      apLoc[2].Copy (&apLoc[1]);
      apLoc[2].p[0] = -fWidth/2;
      dwSegCurve[2] = SEGCURVE_ELLIPSEPREV;

      apLoc[3].Copy (&apLoc[2]);
      apLoc[3].p[1] = (m_pCutout.p[1] + fHeight/2) / 2;
      dwSegCurve[3] = SEGCURVE_ELLIPSENEXT;

      apLoc[4].Copy (&apLoc[3]);
      apLoc[4].p[1] = fHeight/2;
      dwSegCurve[4] = SEGCURVE_ELLIPSEPREV;

      apLoc[5].Copy (&apLoc[4]);
      apLoc[5].p[0] = 0;
      dwSegCurve[5] = SEGCURVE_ELLIPSENEXT;

      apLoc[6].Copy (&apLoc[5]);
      apLoc[6].p[0] = fWidth/2;
      dwSegCurve[6] = SEGCURVE_ELLIPSEPREV;

      apLoc[7].Copy (&apLoc[3]);
      apLoc[7].p[0] *= -1;

      apLoc[8].p[0] = fWidth/2;
      apLoc[8].p[1] = -fHeight/2;

      dwNum = 9;
      break;


   case 5:  // oval
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

   }

   m_Spline.Init (TRUE, dwNum, apLoc, NULL, dwSegCurve, dwDivide, dwDivide, .1);

roundit:
   // round off the edges
   if (fRound > CLOSE) {
      DWORD dwNum = m_Spline.OrigNumPointsGet();
      CListFixed lPoints, lCurve;
      lPoints.Init (sizeof(CPoint));
      lCurve.Init (sizeof(DWORD));

      // BUGFIX - dwDivide was uninitialized
      DWORD dwMin, dwMax;
      fp fDetail;
      m_Spline.DivideGet (&dwMin, &dwMax, &fDetail);

      // reconstruct with curves
      DWORD dwThis, dwPrev, dwNext, dwElNext, dwElPrev;
      CPoint pThis, pNext, pTemp;
      fp fLen;
      dwElNext = SEGCURVE_ELLIPSENEXT;
      dwElPrev = SEGCURVE_ELLIPSEPREV;
      for (i = 0; i < dwNum; i++) {
         // get surrounding points
         m_Spline.OrigSegCurveGet (i, &dwThis);
         m_Spline.OrigSegCurveGet ((i+1)%dwNum, &dwNext);
         m_Spline.OrigSegCurveGet ((i+dwNum-1)%dwNum, &dwPrev);
         m_Spline.OrigPointGet (i, &pThis);
         m_Spline.OrigPointGet ((i+1)%dwNum, &pNext);

         if (dwThis != SEGCURVE_LINEAR) {
            // add it
            lPoints.Add (&pThis);
            lCurve.Add (&dwThis);
            continue;
         }

         if (dwPrev == SEGCURVE_LINEAR) {
            // add current pointer as start of ellipse
            lPoints.Add (&pThis);
            lCurve.Add (&dwElPrev);

            // add another point for start of linear
            pTemp.Subtract (&pNext, &pThis);
            fLen = pTemp.Length() / 2.01;
            fLen = min (fLen, fRound);
            pTemp.Normalize();
            pTemp.Scale (fLen);
            pTemp.Add (&pThis);
            lPoints.Add (&pTemp);
            lCurve.Add (&dwThis);
         }
         else {
            // else, add as a linear segment
            lPoints.Add (&pThis);
            lCurve.Add (&dwThis);
         }

         // if the next one is linear then cut this short and add
         // a curve
         if (dwNext == SEGCURVE_LINEAR) {
            pTemp.Subtract (&pThis, &pNext);
            fLen = pTemp.Length() / 2.01;
            fLen = min (fLen, fRound);
            pTemp.Normalize();
            pTemp.Scale (fLen);
            pTemp.Add (&pNext);
            lPoints.Add (&pTemp);
            lCurve.Add (&dwElNext);
         }
      }

      // Create the spline
      ps->Init (TRUE, lPoints.Num(), (PCPoint) lPoints.Get(0),
         NULL, (DWORD*) lCurve.Get(0), dwMin, dwMax, fDetail);
   }
   else
      m_Spline.CloneTo (ps);

   // done
   return;
}


/*********************************************************************************
CObjectCushion::BulgeCurve - Given a distance from the edge or a point, gives
an amount to bulge. Bulges 0 if distance from edge/curve is 0.

inputs
   fp       fDist
   DWORD    dwSide - 0 = top and bottom, 1 = side
returns
   fp       fBulge - In meters
*/
fp CObjectCushion::BulgeCurve (fp fDist, DWORD dwSide)
{
   fp fBulge, fBulgeDist, fMax;
   fMax = (dwSide ? (m_pSize.p[2]) : max(m_pSize.p[0], m_pSize.p[1])) / 2.0;
   fBulge = m_afBulge[dwSide] * fMax;
   fBulgeDist = m_afBulgeDist[dwSide] * fMax;
   fBulgeDist = max(0,fBulgeDist);

   if (fDist <= CLOSE)
      return 0;
   if (fDist >= fBulgeDist)
      return fBulge;  // max out at this

   // else, use sin
   fp f;
   f = sin(fDist / fBulgeDist * PI/2);
   return f * fBulge;
}


/*********************************************************************************
CObjectCushion::CalcInfo - Calculate the polygons needed to draw the cushion.
*/
void CObjectCushion::CalcInfo ()
{
   // cushion is at least as large as the table
   m_pSize.p[0] = max(m_pSize.p[0], CLOSE);
   m_pSize.p[1] = max(m_pSize.p[1], CLOSE);

   // clear
   m_dwNumPoints = m_dwNumNormals = m_dwNumText = 0;
   m_dwNumVertex = m_dwNumPoly = 0;
   m_dwEdgeNumPoints = m_dwEdgeNumNormals = m_dwEdgeNumText = 0;
   m_dwEdgeNumVertex = m_dwEdgeNumPoly = 0;
   m_memPoints.m_dwCurPosn = 0;
   m_memNormals.m_dwCurPosn = 0;
   m_memText.m_dwCurPosn = 0;
   m_memVertex.m_dwCurPosn = 0;
   m_memPoly.m_dwCurPosn = 0;
   m_memEdgePoints.m_dwCurPosn = 0;
   m_memEdgeNormals.m_dwCurPosn = 0;
   m_memEdgeText.m_dwCurPosn = 0;
   m_memEdgeVertex.m_dwCurPosn = 0;
   m_memEdgePoly.m_dwCurPosn = 0;

   // make the spline
   CSpline s;
   ShapeToSpline (&s);

   // determine the number of points around
   DWORD i, dwNumAround, dwNumRadiate, dwOrig, dwLen;
   fp fLen, fDist;
   CPoint p1, p2;
   dwOrig = s.OrigNumPointsGet();
   m_fDetail = max(m_fDetail, .01);  // so not too small
   dwNumAround = 0;
   fDist = 0;
   for (i = 0; i < dwOrig; i++) {
      // look around
      s.LocationGet ((fp) i / (fp) dwOrig, &p1);
      s.LocationGet (myfmod((fp) (i+1) / (fp) dwOrig,1), &p2);
      p2.Subtract (&p1);
      fLen = p2.Length();
      dwLen = (DWORD) ceil(fLen / m_fDetail);
      dwLen = max(dwLen,1);
      dwNumAround += dwLen;

      // look at the average distance from the center
      p1.Subtract (&m_pCenter);
      p1.p[2] = 0;
      fDist += p1.Length();
   }
   fDist /= (fp) dwOrig;
   dwNumRadiate = (DWORD) ceil(fDist / m_fDetail); // does NOT include the center point
   dwNumRadiate = max(dwNumRadiate,1);

   // so, we know how many points, etc. we need
   m_dwNumPoints = 1 /*center*/ + dwNumAround * dwNumRadiate;
   m_dwNumNormals = 1 /*top*/ + dwNumAround * dwNumRadiate;
   m_dwNumText = 1 /*center*/ + dwNumAround * dwNumRadiate;
   m_dwNumVertex = 1 /*center*/ + dwNumAround * dwNumRadiate;
   m_dwNumPoly = dwNumAround * dwNumRadiate;

   // make sure have enough memory
   if (!m_memPoints.Required (m_dwNumPoints * sizeof(CPoint)))
      return;
   if (!m_memNormals.Required (m_dwNumNormals * sizeof(CPoint)))
      return;
   if (!m_memText.Required (m_dwNumText * sizeof(TEXTPOINT5)))
      return;
   if (!m_memVertex.Required (m_dwNumVertex * sizeof(VERTEX)))
      return;
   if (!m_memPoly.Required (m_dwNumPoly * sizeof(DWORD) * 4))
      return;

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


   // calculate the points and TEXTPOINT5s
   PCPoint p;
   PTEXTPOINT5 pt;
   p = pPoint;
   pt = pText;
   // point [0] (center) is already zeroed out
   p->Copy (&m_pCenter);
   pt->hv[0] = p->p[0];
   pt->hv[1] = -p->p[1];
   pt->xyz[0] = p->p[0];
   pt->xyz[1] = p->p[1];
   pt->xyz[2] = p->p[2];
   p++;
   pt++;

   // loop around the circumfrences
   DWORD dwNode, j;
   for (dwNode = 0; dwNode < dwOrig; dwNode++) {
      // find the distance between these points
      s.LocationGet ((fp) dwNode / (fp) dwOrig, &p1);
      s.LocationGet (myfmod((fp) (dwNode+1) / (fp) dwOrig,1), &p2);
      p2.Subtract (&p1);
      fLen = p2.Length();
      dwLen = (DWORD) ceil(fLen / m_fDetail);
      dwLen = max(dwLen,1);

      // loop over dwLen
      for (i = 0; i < dwLen; i++) {
         s.LocationGet (((fp) dwNode + (fp)i / (fp)dwLen) / (fp) dwOrig, &p1);

         // start at the inside and work out
         for (j = 0; j < dwNumRadiate; j++, p++, pt++) {
            p2.Average (&p1, &m_pCenter, (fp)(j+1) / (fp)dwNumRadiate);
            
            // default to Z=0 - go back later and do bumps
            p2.p[2] = 0;

            p->Copy (&p2);

            // texgture
            pt->hv[0] = p->p[0];
            pt->hv[1] = -p->p[1];
            pt->xyz[0] = p->p[0];
            pt->xyz[1] = p->p[1];
            pt->xyz[2] = p->p[2];
         }  // j
      } // i
   } // dwNode

   // go back over and do Z-height based on distance from edges
   p = pPoint;
   fp fTemp;
   for (i = 0; i < dwNumAround * dwNumRadiate + 1; i++, p++) {
      fDist = 100000;   // a large number

      // find the distance between this and the edges
      for (j = 0; j < dwNumAround; j++) {
         PCPoint pp = &pPoint[1 + j * dwNumRadiate + (dwNumRadiate-1)];  // outside point
         if ((fabs(pp->p[0] - p->p[0]) >= fDist) || (fabs(pp->p[1] - p->p[1]) >= fDist))
            continue;   // quick eliminate
         p1.Subtract (pp, p);
         p1.p[2] = 0;
         fTemp = p1.Length();
         if (fTemp < fDist)
            fDist = fTemp;
      }

      // eventually take buttons int account
      DWORD x,y;
      if (m_dwButtonNumX && m_dwButtonNumY && (fDist > CLOSE)) {
         CPoint pButton;
         DWORD dwX;
         pButton.Zero();
         for (y = 0; y < m_dwButtonNumY; y++) {
            pButton.p[1] = m_pCenter.p[1] + ((fp) y - (fp)(m_dwButtonNumY-1)/2) * m_pButtonDist.p[1];

            dwX = m_dwButtonNumX;
            if ((m_dwButtonNumY > 1) && (m_dwButtonNumX > 1)) {
               switch (m_dwButtonStagger) {
               case 0:  // none
               default:
                  break;
               case 1:  // every even one
                  if (!(y % 2))
                     dwX--;
                  break;
               case 2:  // every odd one
                  if (y%2)
                     dwX--;
                  break;
               }
            }  // if enough buttons to stagger

            // over all x
            for (x = 0; x < dwX; x++) {
               pButton.p[0] = m_pCenter.p[0] + ((fp) x - (fp)(dwX-1)/2) * m_pButtonDist.p[0];

               // distance?
               if ((fabs(pButton.p[0] - p->p[0]) >= fDist) || (fabs(pButton.p[1] - p->p[1]) >= fDist))
                  continue;   // quick eliminate

               p1.Subtract (&pButton, p);
               p1.p[2] = 0;
               fTemp = p1.Length();
               if (fTemp < fDist)
                  fDist = fTemp;
            }

         }
      }  // if have button
      
      // bulge amount
      p->p[2] = BulgeCurve (fDist, 0);
   }


   
   // calculate the normals
   p = pNormal;

   // assume that center point's normal is up
   p->p[2] = 1;   // point up
   p++;

   // normals
   for (i = 0; i < dwNumAround; i++)
      for (j = 0; j < dwNumRadiate; j++, p++) {
         // right vs. left
         DWORD dw1, dw2;
         dw1 = (j > 0) ? (j-1) : -1;
         dw2 = (j < dwNumRadiate-1) ? (j+1) : j;
         p1.Subtract (
            &pPoint[(dw1 == -1) ? 0 : (1 + dw1 + i * dwNumRadiate)],
            &pPoint[1 + dw2 + i * dwNumRadiate]);

         // top vs down
         dw1 = (i + dwNumAround - 1) % dwNumAround;
         dw2 = (i + 1) % dwNumAround;
         p2.Subtract (&pPoint[1 + j + dw1 * dwNumRadiate], &pPoint[1 + j + dw2 * dwNumRadiate]);

         p->CrossProd (&p2, &p1);
         p->Normalize();
      }


   // all the vertices
   PVERTEX pv;
   pv = pVert;
   // around table edge, up
   for (i = 0; i < dwNumAround * dwNumRadiate + 1; i++, pv++) {
      pv->dwNormal = i; // up
      pv->dwPoint = i;  // around
      pv->dwTexture = i;   // around
   }


   // and the polygons
   DWORD *padw;
   padw = pPoly;

   for (i = 0; i < dwNumAround; i++)
      for (j = 0; j < dwNumRadiate; j++, padw+=4) {
         if (j == 0) {  // centeral point triangle
            padw[0] = 0;
            padw[1] = 1 + i * dwNumRadiate;
            padw[2] = 1 + ((i+1)%dwNumAround) * dwNumRadiate;
            padw[3] = -1;
         }
         else {   // edge point quad
            padw[0] = 1 + j-1 + i*dwNumRadiate;
            padw[1] = 1 + j + i*dwNumRadiate;
            padw[2] = 1 + j + ((i+1)%dwNumAround)*dwNumRadiate;
            padw[3] = 1 + j-1 + ((i+1)%dwNumAround)*dwNumRadiate;
         }
      }  // i and j


   //**** Calculate the border
   if (m_pSize.p[2] < CLOSE)
      return;  // all done

   DWORD dwNumVert;
   dwNumVert = ceil(m_pSize.p[2] / m_fDetail) + 1;
   dwNumVert = max(dwNumVert, 2);

   // so, we know how many points, etc. we need
   m_dwEdgeNumPoints = (dwNumAround+1) * dwNumVert;
   m_dwEdgeNumNormals =(dwNumAround+1) * dwNumVert;
   m_dwEdgeNumText = (dwNumAround+1) * dwNumVert;
   m_dwEdgeNumVertex = (dwNumAround+1) * dwNumVert;
   m_dwEdgeNumPoly = dwNumAround * (dwNumVert-1);

   // make sure have enough memory
   if (!m_memEdgePoints.Required (m_dwEdgeNumPoints * sizeof(CPoint)))
      return;
   if (!m_memEdgeNormals.Required (m_dwEdgeNumNormals * sizeof(CPoint)))
      return;
   if (!m_memEdgeText.Required (m_dwEdgeNumText * sizeof(TEXTPOINT5)))
      return;
   if (!m_memEdgeVertex.Required (m_dwEdgeNumVertex * sizeof(VERTEX)))
      return;
   if (!m_memEdgePoly.Required (m_dwEdgeNumPoly * sizeof(DWORD) * 4))
      return;

   // pointers to these
   pPoint = (PCPoint) m_memEdgePoints.p;
   memset (pPoint, 0 ,sizeof (CPoint) * m_dwEdgeNumPoints);
   pNormal = (PCPoint) m_memEdgeNormals.p;
   memset (pNormal, 0, sizeof(CPoint) * m_dwEdgeNumNormals);
   pText = (PTEXTPOINT5) m_memEdgeText.p;
   memset (pText, 0, sizeof(TEXTPOINT5) * m_dwEdgeNumText);
   pVert = (PVERTEX) m_memEdgeVertex.p;
   memset (pVert, 0, sizeof(VERTEX) * m_dwEdgeNumVertex);
   pPoly = (DWORD*) m_memEdgePoly.p;
   memset (pPoly, 0, m_dwEdgeNumPoly * 4 * sizeof(DWORD));

   // points
   p = pPoint;
   pt = pText;
   CPoint pTan, pUp, pOut;
   pUp.Zero();
   pUp.p[2] = 1;
   // loop around the circumfrences
   fp fCurText;
   fCurText = 0;
   CPoint pLastLoc;
   pLastLoc.Zero();
   for (dwNode = 0; dwNode < dwOrig; dwNode++) {
      // find the distance between these points
      s.LocationGet ((fp) dwNode / (fp) dwOrig, &p1);
      s.LocationGet (myfmod((fp) (dwNode+1) / (fp) dwOrig,1), &p2);
      p2.Subtract (&p1);
      fLen = p2.Length();
      dwLen = (DWORD) ceil(fLen / m_fDetail);
      dwLen = max(dwLen,1);

      // loop over dwLen
      for (i = 0; i < dwLen; i++) {
         // get the tangent
         fp fLoc = ((fp) dwNode + (fp)i / (fp)dwLen) / (fp) dwOrig;
         s.LocationGet (myfmod(fLoc-CLOSE, 1), &p1);

         // get the tangent
         s.LocationGet (myfmod(fLoc+CLOSE, 1),&p2);

         pTan.Subtract (&p2, &p1);
         pTan.p[2] = 0;
         pTan.Normalize();

         // find out vector
         pOut.CrossProd (&pUp, &pTan);
         pOut.Normalize();

         // get the location
         s.LocationGet (fLoc, &p1);

         // udpate the textures
         if (i || dwNode) {
            p2.Subtract (&p1, &pLastLoc);
            fCurText += p2.Length();
         }
         pLastLoc.Copy (&p1);

         // start at the inside and work out
         for (j = 0; j < dwNumVert; j++, p++, pt++) {
            p->Copy (&p1);
            p->p[2] = ((fp) j / (fp)(dwNumVert-1) - .5) * m_pSize.p[2];

            // scale the out vector to create bump
            fp fBump;
            fBump = BulgeCurve (
               (1 - fabs(((fp)(dwNumVert-1)/2 - (fp) j) / ((fp)(dwNumVert-1)/2))) * m_pSize.p[2]/2,
               1);
            p2.Copy (&pOut);
            p2.Scale (fBump);
            p->Add (&p2);
            

            // texgture
            pt->hv[0] = p->p[2];
            pt->hv[1] = -fCurText;

            pt->xyz[0] = p->p[0];
            pt->xyz[1] = p->p[1];
            pt->xyz[2] = p->p[2];
         }  // j

      } // i
   } // dwNode

   // duplicate first points but with new textures
   s.LocationGet (0, &p1);
   p2.Subtract (&p1, &pLastLoc);
   fCurText += p2.Length();
   for (j = 0; j < dwNumVert; j++, p++, pt++) {
      p->Copy (&pPoint[j]);

      // texgture
      pt->hv[0] = p->p[2];
      pt->hv[1] = -fCurText;

      pt->xyz[0] = p->p[0];
      pt->xyz[1] = p->p[1];
      pt->xyz[2] = p->p[2];
   }

   // calculate the normals
   p = pNormal;
   for (i = 0; i < (dwNumAround+1); i++)
      for (j = 0; j < dwNumVert; j++, p++) {
         // right vs. left
         DWORD dw1, dw2;
         dw1 = (j > 0) ? (j-1) : 0;
         dw2 = (j < dwNumVert-1) ? (j+1) : j;
         p1.Subtract (
            &pPoint[dw1 + i * dwNumVert],
            &pPoint[dw2 + i * dwNumVert]);

         // top vs down
         dw1 = (i + dwNumAround - 1) % dwNumAround;
         dw2 = (i + 1) % dwNumAround;
         p2.Subtract (&pPoint[j + dw1 * dwNumVert], &pPoint[j + dw2 * dwNumVert]);

         p->CrossProd (&p1, &p2);
         p->Normalize();
      }


   // all the vertices
   pv = pVert;
   // around table edge, up
   for (i = 0; i < (dwNumAround+1) * dwNumVert; i++, pv++) {
      pv->dwNormal = i; // up
      pv->dwPoint = i;  // around
      pv->dwTexture = i;   // around
   }


   // and the polygons
   padw = pPoly;

   for (i = 0; i < dwNumAround; i++)
      for (j = 0; j < (dwNumVert-1); j++, padw+=4) {
         padw[0] = j+1 + i*dwNumVert;
         padw[1] = j + i*dwNumVert;
         padw[2] = j + ((i+1)%(dwNumAround+1))*dwNumVert;
         padw[3] = j+1 + ((i+1)%(dwNumAround+1))*dwNumVert;
      }  // i and j


   // done
}

/* CushionDialogPage
*/
BOOL CushionDialogPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCObjectCushion pv = (PCObjectCushion) pPage->m_pUserData;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         ComboBoxSet (pPage, L"shape", pv->m_dwShape);
         ComboBoxSet (pPage, L"buttonstagger", pv->m_dwButtonStagger);

         // enable/disable modify custom shape
         PCEscControl pControl;
         pControl = pPage->ControlFind (L"custom");
         if (pControl)
            pControl->Enable (!pv->m_dwShape);

         MeasureToString (pPage, L"thick", pv->m_pSize.p[2]);
         MeasureToString (pPage, L"detail", pv->m_fDetail);
         MeasureToString (pPage, L"buttondist0", pv->m_pButtonDist.p[0]);
         MeasureToString (pPage, L"buttondist1", pv->m_pButtonDist.p[1]);

         DoubleToControl (pPage, L"buttonnumx", pv->m_dwButtonNumX);
         DoubleToControl (pPage, L"buttonnumy", pv->m_dwButtonNumY);

         // scrolling
         pControl = pPage->ControlFind (L"bulge0");
         if (pControl)
            pControl->AttribSetInt (Pos(), (int)(pv->m_afBulge[0] * 100));
         pControl = pPage->ControlFind (L"bulge1");
         if (pControl)
            pControl->AttribSetInt (Pos(), (int)(pv->m_afBulge[1] * 100));
         pControl = pPage->ControlFind (L"bulgedist0");
         if (pControl)
            pControl->AttribSetInt (Pos(), (int)(pv->m_afBulgeDist[0] * 100));
         pControl = pPage->ControlFind (L"bulgedist1");
         if (pControl)
            pControl->AttribSetInt (Pos(), (int)(pv->m_afBulgeDist[1] * 100));
         pControl = pPage->ControlFind (L"rounded");
         if (pControl)
            pControl->AttribSetInt (Pos(), (int)(pv->m_fRounded * 100));
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

         if (!_wcsicmp(psz, L"bulge0"))
            pf = &pv->m_afBulge[0];
         else if (!_wcsicmp(psz, L"bulge1"))
            pf = &pv->m_afBulge[1];
         else if (!_wcsicmp(psz, L"bulgedist0"))
            pf = &pv->m_afBulgeDist[0];
         else if (!_wcsicmp(psz, L"bulgedist1"))
            pf = &pv->m_afBulgeDist[1];
         else if (!_wcsicmp(psz, L"rounded"))
            pf = &pv->m_fRounded;

         if (!pf || (*pf == fVal))
            break;   // no change

         pv->m_pWorld->ObjectAboutToChange (pv);
         *pf = fVal;
         pv->CalcInfo();
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
            pv->CalcInfo();
            pv->m_pWorld->ObjectChanged (pv);

            // enable/disable modify custom shape
            PCEscControl pControl;
            pControl = pPage->ControlFind (L"custom");
            if (pControl)
               pControl->Enable (!pv->m_dwShape);

            return TRUE;
         }
         else if (!_wcsicmp(psz, L"buttonstagger")) {
            if (dwVal == pv->m_dwButtonStagger)
               break;

            pv->m_pWorld->ObjectAboutToChange (pv);
            pv->m_dwButtonStagger = dwVal;
            pv->CalcInfo();
            pv->m_pWorld->ObjectChanged (pv);
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

         MeasureParseString (pPage, L"thick", &pv->m_pSize.p[2]);
         pv->m_pSize.p[2] = max(0,pv->m_pSize.p[2]);
         MeasureParseString (pPage, L"detail", &pv->m_fDetail);
         pv->m_fDetail = max(.01, pv->m_fDetail);
         MeasureParseString (pPage, L"buttondist0", &pv->m_pButtonDist.p[0]);
         pv->m_pButtonDist.p[0] = max(.01, pv->m_pButtonDist.p[0]);
         MeasureParseString (pPage, L"buttondist1", &pv->m_pButtonDist.p[1]);
         pv->m_pButtonDist.p[1] = max(.01, pv->m_pButtonDist.p[1]);

         pv->m_dwButtonNumX = (DWORD) DoubleFromControl (pPage, L"buttonnumx");
         pv->m_dwButtonNumY = DoubleFromControl (pPage, L"buttonnumy");

         pv->CalcInfo ();
         pv->m_pWorld->ObjectChanged (pv);


         break;
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Cushion settings";
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}


/* CushionCurvePage
*/
BOOL CushionCurvePage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCObjectCushion pv = (PCObjectCushion)pPage->m_pUserData;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         PCEscControl pControl;

         // draw images
         PCSpline ps;
         ps = &pv->m_Spline;
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
            ps = &pv->m_Spline;
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
            pv->m_Spline.Init (TRUE, dwOrig,
               (PCPoint) memPoints.p, NULL, (DWORD*) memSegCurve.p, dwVal, dwVal, .1);
            pv->CalcInfo();
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
         ps = &pv->m_Spline;
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
         pv->m_Spline.Init (TRUE, dwOrig, paPoints, NULL, padw, dwMaxDivide, dwMaxDivide, .1);
         pv->CalcInfo();
         pv->m_pWorld->ObjectChanged (pv);

         // redraw the shapes
         ps = &pv->m_Spline;
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
            p->pszSubString = L"Cushion curve";
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
CObjectCushion::DialogShow - Causes the object to show a dialog box that allows
different attributes to be changed.

inputs
   DWORD             dwSuface - Surface that was clicked on. 0 if unknown.
   PCEscWindow       pWindow - Escarpment window to draw it in
returns
   BOOL - TRUE if user pressed "Back", FALSE if user pressed "Close"
*/
BOOL CObjectCushion::DialogShow (DWORD dwSurface, PCEscWindow pWindow, PCPoint pClick)
{
   PWSTR pszRet;
mainpage:
   pszRet = pWindow->PageDialog (ghInstance, IDR_MMLCUSHIONDIALOG, CushionDialogPage, this);
   if (!pszRet)
      return FALSE;
   if (!_wcsicmp(pszRet, L"custom")) {
      pszRet = pWindow->PageDialog (ghInstance, IDR_MMLNOODLECURVE, CushionCurvePage, this);
      if (!pszRet)
         return FALSE;
      if (!_wcsicmp(pszRet, Back()))
         goto mainpage;
      // else fall through
   }

   return !_wcsicmp(pszRet, Back());
}


/**********************************************************************************
CObjectCushion::DialogQuery - Returns TRUE if the object supports a custom
dialog box, FALSE if it doesn't. The default object doesn't
*/
BOOL CObjectCushion::DialogQuery (void)
{
   return TRUE;
}


/*************************************************************************************
CObjectCushion::ControlPointQuery - Called to query the information about a control
point identified by dwID.

inputs
   DWORD       dwID - ID
   POSCONTROL  pInfo - Filled in with the infomration
returns
   BOOL - TRUE if successful
*/
BOOL CObjectCushion::ControlPointQuery (DWORD dwID, POSCONTROL pInfo)
{
   fp fKnobSize = max(m_pSize.p[2], max(m_pSize.p[0], m_pSize.p[1])) / 50;

   DWORD dwSide;
   dwSide = dwID % 2;
   dwID /= 2;

   memset (pInfo,0, sizeof(*pInfo));

   pInfo->dwID = dwID * 2 + dwSide;
//   pInfo->dwFreedom = 0;   // any direction
   pInfo->dwStyle = CPSTYLE_CUBE;
   pInfo->fSize = fKnobSize;
   pInfo->cColor = (dwID < 2) ? RGB(0,0xff,0xff) : RGB (0xff,0xff,0);
   pInfo->pLocation.Zero();
   switch (dwID) {
   case 0:
      wcscpy (pInfo->szName, L"Width");
      MeasureToString (m_pSize.p[0], pInfo->szMeasurement);
      pInfo->pLocation.p[0] = m_pSize.p[0]/2;
      break;
   case 1:
      wcscpy (pInfo->szName, L"Length");
      MeasureToString (m_pSize.p[1], pInfo->szMeasurement);
      pInfo->pLocation.p[1] = m_pSize.p[1]/2;
      break;
   case 2:
      wcscpy (pInfo->szName, L"Center");
      pInfo->pLocation.Copy (&m_pCenter);
      break;
   case 3:
      wcscpy (pInfo->szName, L"Cutout");
      pInfo->pLocation.Copy (&m_pCutout);
      break;
   default:
      if ((dwID >= 10) && (dwID < 10+m_Spline.OrigNumPointsGet())) {
         wcscpy (pInfo->szName, L"Shape");
         pInfo->dwStyle = CPSTYLE_SPHERE;
         pInfo->cColor = RGB (0, 0, 0xff);
         m_Spline.OrigPointGet (dwID - 10, &pInfo->pLocation);
      }
      else
         return FALSE;
      break;
   }

   pInfo->pLocation.p[2] = m_pSize.p[2] / 2.0;
   if ((dwID == 2) || (dwID == 3))
      pInfo->pLocation.p[2] += m_afBulge[0]; // add thickness
   pInfo->pLocation.p[2] *= (dwSide ? 1 : -1);

   return TRUE;
}

/*************************************************************************************
CObjectCushion::ControlPointSet - Called to change the valud of a control point.

inputs
   DWORD       dwID - ID
   PCPoint     pVal - Contains the new location, in object coordinates
returns
   BOOL - TRUE if successful
*/
BOOL CObjectCushion::ControlPointSet (DWORD dwID, PCPoint pVal, PCPoint pViewer)
{
   dwID /= 2;  // since will ignore the side information

   // tell the world we're about to change
   if (m_pWorld)
      m_pWorld->ObjectAboutToChange (this);

   CPoint pOld;
   pOld.Copy (&m_pSize);

   switch (dwID) {
   case 0:
      m_pSize.p[0] = max(pVal->p[0]*2, CLOSE);
      break;
   case 1:
      m_pSize.p[1] = max(pVal->p[1]*2, CLOSE);
      break;
   case 2:
      m_pCenter.p[0] = pVal->p[0];
      m_pCenter.p[1] = pVal->p[1];
      break;
   case 3:
      m_pCutout.p[0] = pVal->p[0];
      m_pCutout.p[1] = pVal->p[1];
      break;
   default:
      if ((dwID < 10) || (dwID >= 10 + m_Spline.OrigNumPointsGet())) {
         if (m_pWorld)
            m_pWorld->ObjectChanged (this);
         return FALSE;
      }

      // change the spline
      CPoint pTemp;
      pTemp.Copy (pVal);
      pTemp.p[2] = 0;
      m_Spline.OrigPointReplace (dwID - 10, &pTemp);
      break;
   }

   // rescale
   if (!pOld.AreClose (&m_pSize)) {
      m_pCenter.p[0] *= m_pSize.p[0] / pOld.p[0];
      m_pCenter.p[1] *= m_pSize.p[1] / pOld.p[1];
      m_pCutout.p[0] *= m_pSize.p[0] / pOld.p[0];
      m_pCutout.p[1] *= m_pSize.p[1] / pOld.p[1];

      // Need to rescale the spline also
      if (!m_dwShape) {
         DWORD i;
         for (i = 0; i < m_Spline.OrigNumPointsGet(); i++) {
            CPoint pv;
            m_Spline.OrigPointGet (i, &pv);
            pv.p[0] *= m_pSize.p[0] / pOld.p[0];
            pv.p[1] *= m_pSize.p[1] / pOld.p[1];
            m_Spline.OrigPointReplace (i, &pv);
         }
      }
   }  // if changed size

   // create the info
   CalcInfo ();

   // tell the world we've changed
   if (m_pWorld)
      m_pWorld->ObjectChanged (this);

   return TRUE;
}

/*************************************************************************************
CObjectCushion::ControlPointEnum - Called to enumerate a list of control point IDs
into the list.

inputs
   PCListFixed       plDWORD - Should be filled in with control points
returns
   none
*/
void CObjectCushion::ControlPointEnum (PCListFixed plDWORD)
{
   DWORD i;

   DWORD dwNum;
   
   dwNum = 3;
   switch (m_dwShape) {
   case 3:  // L
   case 4:  // T
   case 6:  // armchair side
      dwNum++;
      break;
   }

   dwNum *= 2; // symmetry on top and bottom
   for (i = 0; i < dwNum; i++)
      plDWORD->Add (&i);

   if (!m_dwShape) {
      dwNum = m_Spline.OrigNumPointsGet();
      for (i = 20; i < 20 + dwNum*2; i++)
         plDWORD->Add (&i);
   }
}

