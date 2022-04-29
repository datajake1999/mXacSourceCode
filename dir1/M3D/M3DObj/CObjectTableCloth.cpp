/************************************************************************
CObjectTableCloth.cpp - Draws a TableCloth.

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

static SPLINEMOVEP   gaTableClothMove[] = {
   L"Center", 0, 0
};


/**********************************************************************************
CObjectTableCloth::Constructor and destructor */
CObjectTableCloth::CObjectTableCloth (PVOID pParams, POSINFO pInfo)
{
   m_dwRenderShow = RENDERSHOW_FURNITURE;
   m_OSINFO = *pInfo;

   m_dwShape = 2;
   m_pSizeTable.Zero();
   m_pSizeTable.p[0] = 1.5;
   m_pSizeTable.p[1] = 2;
   m_pSizeCloth.Zero();
   m_pSizeCloth.p[0] = 2;
   m_pSizeCloth.p[1] = 2.5;
   m_fRippleSize = 0.20;

   m_dwNumPoints = m_dwNumNormals = m_dwNumText = 0;
   m_dwNumVertex = m_dwNumPoly = 0;

   // calculate
   CalcInfo();

   // color for the TableCloth
   ObjectSurfaceAdd (42, RGB(0xff,0xff,0xff), MATERIAL_CLOTHSMOOTH);
}


CObjectTableCloth::~CObjectTableCloth (void)
{
   // do nothing for now
}


/**********************************************************************************
CObjectTableCloth::Delete - Called to delete this object
*/
void CObjectTableCloth::Delete (void)
{
   delete this;
}

/**********************************************************************************
CObjectTableCloth::Render - Draws a test box.

inputs
   POBJECTRENDER     pr - Render information
*/
void CObjectTableCloth::Render (POBJECTRENDER pr, DWORD dwSubObject)
{
   // create the surface render object and draw
   // CRenderSurface rs;
   m_Renderrs.ClearAll();

   CMatrix mObject;
   m_Renderrs.Init (pr->pRS);
   ObjectMatrixGet (&mObject);
   m_Renderrs.Multiply (&mObject);

   // object specific
   m_Renderrs.SetDefMaterial (m_OSINFO.dwRenderShard, ObjectSurfaceFind (42), m_pWorld);

   PCPoint pPoint, pNormal;
   PTEXTPOINT5 pText;
   PVERTEX pVert;
   DWORD dwPointIndex, dwNormalIndex, dwTextIndex, dwVertIndex;

   DWORD i;
   pPoint = m_Renderrs.NewPoints (&dwPointIndex, m_dwNumPoints);
   if (pPoint)
      memcpy (pPoint, m_memPoints.p, m_dwNumPoints * sizeof(CPoint));
   pNormal = m_Renderrs.NewNormals (TRUE, &dwNormalIndex, m_dwNumNormals);
   if (pNormal)
      memcpy (pNormal, m_memNormals.p, m_dwNumNormals * sizeof(CPoint));
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
      if (padw[3] == -1)
         m_Renderrs.NewTriangle (dwVertIndex + padw[0], dwVertIndex + padw[1],
            dwVertIndex + padw[2], FALSE);
      else
         m_Renderrs.NewQuad (dwVertIndex + padw[0], dwVertIndex + padw[1],
            dwVertIndex + padw[2], dwVertIndex + padw[3], FALSE);
   }

   m_Renderrs.Commit();
}


/**********************************************************************************
CObjectTableCloth::QueryBoundingBox - Standard API
*/
void CObjectTableCloth::QueryBoundingBox (PCPoint pCorner1, PCPoint pCorner2, DWORD dwSubObject)
{
   DWORD i;
   PCPoint p = (PCPoint)m_memPoints.p;
   pCorner1->Copy (p);
   pCorner2->Copy (p);
   for (i = 1, p++; i < m_dwNumPoints; i++, p++) {
      pCorner1->Min (p);
      pCorner2->Max (p);
   }

#ifdef _DEBUG
   // test, make sure bounding box not too small
   CPoint p1,p2;
   //DWORD i;
   CObjectTemplate::QueryBoundingBox (&p1, &p2, dwSubObject);
   for (i = 0; i < 3; i++)
      if ((p1.p[i] < pCorner1->p[i]) || (p2.p[i] > pCorner2->p[i]))
         break;
   if (i < 3)
      OutputDebugString ("\r\nCObjectTableCloth::QueryBoundingBox too small.");
#endif
}

/**********************************************************************************
CObjectTableCloth::Clone - Clones this

inputs
   none
returns
   PCObjectSocket - Clone
*/
PCObjectSocket CObjectTableCloth::Clone (void)
{
   PCObjectTableCloth pNew;

   pNew = new CObjectTableCloth(NULL, &m_OSINFO);

   // clone template info
   CloneTemplate(pNew);

   pNew->m_dwShape = m_dwShape;
   pNew->m_pSizeTable.Copy (&m_pSizeTable);
   pNew->m_pSizeCloth.Copy (&m_pSizeCloth);
   pNew->m_fRippleSize = m_fRippleSize;

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
   return pNew;
}

static PWSTR gpszShape = L"Shape";
static PWSTR gpszSizeTable = L"SizeTable";
static PWSTR gpszSizeCloth = L"SizeCloth";
static PWSTR gpszRippleSize = L"RippleSize";

PCMMLNode2 CObjectTableCloth::MMLTo (void)
{
   // call into the template first
   PCMMLNode2 pNode = MMLToTemplate();
   if (!pNode)
      return NULL;

   MMLValueSet (pNode, gpszShape, (int) m_dwShape);
   MMLValueSet (pNode, gpszSizeTable, &m_pSizeTable);
   MMLValueSet (pNode, gpszSizeCloth, &m_pSizeCloth);
   MMLValueSet (pNode, gpszRippleSize, m_fRippleSize);

   return pNode;
}

BOOL CObjectTableCloth::MMLFrom (PCMMLNode2 pNode)
{
   // call into the template first
   if (!MMLFromTemplate (pNode))
      return FALSE;

   m_dwShape = (DWORD) MMLValueGetInt (pNode, gpszShape, (int) 0);
   MMLValueGetPoint (pNode, gpszSizeTable, &m_pSizeTable);
   MMLValueGetPoint (pNode, gpszSizeCloth, &m_pSizeCloth);
   m_fRippleSize = MMLValueGetDouble (pNode, gpszRippleSize, 0);

   CalcInfo ();
   return TRUE;
}

/*********************************************************************************
CObjectTableCloth::ShapeToSpline - Looks at the shape and other member variables
and creates the noodle object.
*/
void CObjectTableCloth::ShapeToSpline (PCSpline ps)
{
   // figure out the path
   CPoint apLoc[8];
   DWORD dwSegCurve[8];
   DWORD dwNum, dwDivide;
   dwNum = 0;
   dwDivide = 3;
   memset (apLoc, 0, sizeof(apLoc));
   memset (dwSegCurve, 0, sizeof(dwSegCurve));

   fp fWidth, fHeight;
   fWidth = m_pSizeTable.p[0];
   fHeight = m_pSizeTable.p[1];

   DWORD i;

   switch (m_dwShape) {
   case 0:  // rectangle
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

   case 1:  // oval
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

   case 2:  // rounded
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


/*********************************************************************************
CObjectTableCloth::CalcInfo - Calculate the polygons needed to draw the table cloth.
*/
void CObjectTableCloth::CalcInfo ()
{
   // table cloth is at least as large as the table
   CPoint pSize;
   m_pSizeTable.p[0] = max(m_pSizeTable.p[0], CLOSE);
   m_pSizeTable.p[1] = max(m_pSizeTable.p[1], CLOSE);
   pSize.Copy (&m_pSizeTable);
   pSize.Scale (1.01);
   m_pSizeCloth.Max (&pSize);

   // clear
   m_dwNumPoints = m_dwNumNormals = m_dwNumText = 0;
   m_dwNumVertex = m_dwNumPoly = 0;
   m_memPoints.m_dwCurPosn = 0;
   m_memNormals.m_dwCurPosn = 0;
   m_memText.m_dwCurPosn = 0;
   m_memVertex.m_dwCurPosn = 0;
   m_memPoly.m_dwCurPosn = 0;

   // make the spline
   CSpline s;
   ShapeToSpline (&s);

   // determine the number of ripples
   DWORD i, dwNumRipple, dwOrig, dwLen;
   fp fLen;
   CPoint p1, p2;
   dwOrig = s.OrigNumPointsGet();
   m_fRippleSize = max(m_fRippleSize, .01);  // so not too small
   dwNumRipple = 0;
   for (i = 0; i < dwOrig; i++) {
      s.LocationGet ((fp) i / (fp) dwOrig, &p1);
      s.LocationGet (myfmod((fp) (i+1) / (fp) dwOrig,1), &p2);

      p2.Subtract (&p1);
      fLen = p2.Length();
      fLen = ceil(fLen / m_fRippleSize);
      dwLen = (DWORD) fLen;
      if (!dwLen)
         dwLen = 1;
      dwNumRipple += dwLen;
   }

   // the number of points around is calculated from the number of ripples
   DWORD dwNumAround;
#define RIPPLEDETAIL    8
   dwNumAround = dwNumRipple * RIPPLEDETAIL;

   // so, we know how many points, etc. we need
   m_dwNumPoints = 1 /*center*/ + dwNumAround /* for against table */ + dwNumAround;  // for hanging
   m_dwNumNormals = 1 /*top*/ + dwNumAround /* for edge table */ + dwNumAround; // for edge of fabric
   m_dwNumText = 1 /*center*/ + dwNumAround /* for against table */ + dwNumAround;  // for against edge of fabric
   m_dwNumVertex = 1 /*center*/ + dwNumAround /* table edge, up */ +
      dwNumAround /* table edge horz */ + dwNumAround /*hanging cloth*/;
   m_dwNumPoly = dwNumAround /* on table*/ + dwNumAround;// hanging

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
   p++;
   pt++;
   // around edge of table
   for (i = 0; i < dwNumAround; i++, p++, pt++) {
      s.LocationGet ((fp) i / (fp) dwNumAround, p);
      p->p[2] = 0;

      pt->hv[0] = p->p[0];
      pt->hv[1] = -p->p[1];
      pt->xyz[0] = p->p[0];
      pt->xyz[1] = p->p[1];
      pt->xyz[2] = p->p[2];
   }

   // calculate the end intersection of the table cloth
   CMem mem;
   PCPoint papEnd;
   PCPoint pp1, pp2, pp3, pp4;
   CPoint pStart, pEnd;
   CPoint pUp, pN;
   pUp.Zero();
   pUp.p[2] = 1;
   if (!mem.Required (dwNumAround * sizeof(CPoint)))
      return;
   papEnd = (PCPoint) mem.p;
   for (i = 0; i < dwNumAround; i++) {
      s.LocationGet ((fp) i / (fp) dwNumAround, &pStart);
      pStart.p[2] = 0;   // for now

      // find the normal at the edge of the table
      pp1 = pPoint + (1 + ((i+dwNumAround-1)%dwNumAround));
      pp2 = pPoint + (1 + i);
      pp3 = pPoint + (1 + ((i+1)%dwNumAround));

      p1.Subtract (pp3, pp2);
      p2.Subtract (pp2, pp1);
      p1.Normalize();
      p2.Normalize();
      p1.Average (&p2);
      pN.CrossProd (&pUp, &p1);
      pN.Normalize();
      pNormal[i+1].Copy (&pN);   // save this away

      // make a line from p out beyond the edge of the table cloth
      pEnd.Copy (&pN);
      pEnd.Scale (100); // make it really long
      pEnd.Add (&pStart);

      // convert to so -tablewidth/2 to tablewidth/2 goes to 0..1
      TEXTUREPOINT tpStart, tpEnd, tpnStart, tpnEnd;
      tpStart.h = (pStart.p[0] + m_pSizeCloth.p[0]/2) / m_pSizeCloth.p[0];
      tpStart.v = (pStart.p[1] + m_pSizeCloth.p[1]/2) / m_pSizeCloth.p[1];
      tpEnd.h = (pEnd.p[0] + m_pSizeCloth.p[0]/2) / m_pSizeCloth.p[0];
      tpEnd.v = (pEnd.p[1] + m_pSizeCloth.p[1]/2) / m_pSizeCloth.p[1];
      if (Intersect2DLineWithBox (&tpStart, &tpEnd, &tpnStart, &tpnEnd)) {
         // it's going to be then end that intersects...
         pEnd.p[0] = tpnEnd.h * m_pSizeCloth.p[0] - m_pSizeCloth.p[0]/2;
         pEnd.p[1] = tpnEnd.v * m_pSizeCloth.p[1] - m_pSizeCloth.p[1]/2;
         pEnd.p[2] = 0;

         papEnd[i].Copy (&pEnd);
      }
      else
         papEnd[i].Copy (&pStart);  // better than nothing
   }

   // around edge of table cloth
   for (i = 0; i < dwNumAround; i++, p++, pt++) {
      s.LocationGet ((fp) i / (fp) dwNumAround, p);
      p->p[2] = 0;   // for now

      pt->hv[0] = papEnd[i].p[0];
      pt->hv[1] = -papEnd[i].p[1];
      pt->xyz[0] = p->p[0];
      pt->xyz[1] = p->p[1];
      pt->xyz[2] = p->p[2];

      // find the distance
      fp fLen;
      pEnd.Subtract (&papEnd[i], p);
      fLen = pEnd.Length();

      // take a running average of 4 previous and 4 later points along the
      // inside and outside riim and see the length difference
      fp fLenIn, fLenOut;
      DWORD j;
      fLenIn = fLenOut = 0;
      for (j = dwNumAround-RIPPLEDETAIL*2; j < dwNumAround+RIPPLEDETAIL*2; j++) {
         fp fWeight = RIPPLEDETAIL*2 - fabs((fp) j - dwNumAround);
         // inside
         pp1 = pPoint + (1 + ((j+i) % dwNumAround));
         pp2 = pPoint + (1 + ((j+i+1) % dwNumAround));
         pStart.Subtract (pp2, pp1);
         fLenIn += pStart.Length() * fWeight;

         // outside
         pp1 = papEnd + ((j+i) % dwNumAround);
         pp2 = papEnd + ((j+i+1) % dwNumAround);
         pStart.Subtract (pp2, pp1);
         fLenOut += pStart.Length() * fWeight;
      }

      // the amount to make up is a scale to the sine-wave
      fp fScale;
      fScale = fLenOut/fLenIn;  // this math is only approximate
      fScale *= fScale;
      fScale -= 1;
      fScale = max(fScale, 0);
      fScale = sqrt(fScale);
      fScale *= m_fRippleSize;
      fScale /= 4;

      // multiply by the cos of the angle
      fScale *= cos ((fp) (i % RIPPLEDETAIL) / (fp) RIPPLEDETAIL * 2 * PI);

      // muck with the point
      pN.Copy (&pNormal[i+1]);
      pN.Scale (fScale);   // amount to swing out or in
      if (fLen > fScale)   // amount to hang down
         pN.p[2] = -sqrt(fLen * fLen - fScale * fScale);
      else
         pN.p[2] = 0;

      // add offset
      p->Add (&pN);
   }

   
   // calculate the normals
   p = pNormal;

   // top normal
   p->p[2] = 1;   // point up
   p++;

   // around the edge of the table
   for (i = 0; i < dwNumAround; i++, p++) {
      pp1 = pPoint + (1 + ((i+dwNumAround-1)%dwNumAround));
      pp2 = pPoint + (1 + i);
      pp3 = pPoint + (1 + ((i+1)%dwNumAround));

      p1.Subtract (pp3, pp2);
      p2.Subtract (pp2, pp1);
      p1.Normalize();
      p2.Normalize();
      p1.Average (&p2);
      p2.CrossProd (&pUp, &p1);
      p2.Normalize();

      p->Copy (&p2);
   }

   // around the edge of the sheet
   for (i = 0; i < dwNumAround; i++, p++) {
      pp1 = pPoint + (1 + dwNumAround + ((i+dwNumAround-1)%dwNumAround));
      pp2 = pPoint + (1 + dwNumAround + i);
      pp3 = pPoint + (1 + dwNumAround + ((i+1)%dwNumAround));
      pp4 = pPoint + (1 + i); // up

      p1.Subtract (pp3, pp2);
      p2.Subtract (pp2, pp1);
      p1.Normalize();
      p2.Normalize();
      p1.Average (&p2);
      pUp.Subtract (pp4, pp2);
      p2.CrossProd (&pUp, &p1);
      p2.Normalize();

      p->Copy (&p2);
   }

   // all the vertices
   PVERTEX pv;
   pv = pVert;
   
   // top center
   pv->dwNormal = 0;
   pv->dwPoint = 0;
   pv->dwTexture = 0;
   pv++;

   // around table edge, up
   for (i = 0; i < dwNumAround; i++, pv++) {
      pv->dwNormal = 0; // up
      pv->dwPoint = 1+i;  // around
      pv->dwTexture = 1+i;   // around
   }

   // around the edge of the table, facing out
   for (i = 0; i < dwNumAround; i++, pv++) {
      pv->dwNormal = 1+i; // up
      pv->dwPoint = 1+i;  // around
      pv->dwTexture = 1+i;   // around
   }

   // around the edge of the tablecloth
   for (i = 0; i < dwNumAround; i++, pv++) {
      pv->dwNormal = 1+dwNumAround+i; // up
      pv->dwPoint = 1+dwNumAround+i;  // around
      pv->dwTexture = 1+dwNumAround+i;   // around
   }

   // and the polygons
   DWORD *padw;
   padw = pPoly;

   // top
   for (i = 0; i < dwNumAround; i++, padw += 4) {
      padw[0] = 0;   // center
      padw[1] = 1 + i;  // top
      padw[2] = 1 + ((i+1)%dwNumAround);  // still top
      padw[3] = -1;
   }

   // hanging
   for (i = 0; i < dwNumAround; i++, padw += 4) {
      padw[0] = 1 + dwNumAround + i;
      padw[1] = 1 + dwNumAround*2 + i;
      padw[2] = 1 + dwNumAround*2 + ((i+1)%dwNumAround);
      padw[3] = 1 + dwNumAround + ((i+1)%dwNumAround);
   }

   // done
}


/**************************************************************************************
CObjectTableCloth::MoveReferencePointQuery - 
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
BOOL CObjectTableCloth::MoveReferencePointQuery (DWORD dwIndex, PCPoint pp)
{
   PSPLINEMOVEP ps;
   DWORD dwDataSize;
   ps = gaTableClothMove;
   dwDataSize = sizeof(gaTableClothMove);

   if (dwIndex >= dwDataSize / sizeof(SPLINEMOVEP))
      return FALSE;

   // always at 0,0 in TableCloths
   pp->Zero();
   pp->p[2] = -.001; // BUGFIX - Make tablecloth 1 mm above table so no collision problems
   return TRUE;
}

/**************************************************************************************
CObjectTableCloth::MoveReferenceStringQuery -
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
BOOL CObjectTableCloth::MoveReferenceStringQuery (DWORD dwIndex, PWSTR psz, DWORD dwSize, DWORD *pdwNeeded)
{
   PSPLINEMOVEP ps;
   DWORD dwDataSize;
   ps = gaTableClothMove;
   dwDataSize = sizeof(gaTableClothMove);
   if (dwIndex >= dwDataSize / sizeof(SPLINEMOVEP)) {
      if (pdwNeeded)
         *pdwNeeded = 0;
      return FALSE;
   }

   DWORD dwNeeded;
   dwNeeded = ((DWORD)wcslen (ps[dwIndex].pszName) + 1) * 2;
   if (pdwNeeded)
      *pdwNeeded = dwNeeded;
   if (dwNeeded <= dwSize) {
      wcscpy (psz, ps[dwIndex].pszName);
      return TRUE;
   }
   else
      return FALSE;
}


/* TableClothDialogPage
*/
BOOL TableClothDialogPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCObjectTableCloth pv = (PCObjectTableCloth) pPage->m_pUserData;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         ComboBoxSet (pPage, L"shape", pv->m_dwShape);

         MeasureToString (pPage, L"tablewidth", pv->m_pSizeTable.p[0]);
         MeasureToString (pPage, L"tablelength", pv->m_pSizeTable.p[1]);
         MeasureToString (pPage, L"clothwidth", pv->m_pSizeCloth.p[0]);
         MeasureToString (pPage, L"clothlength", pv->m_pSizeCloth.p[1]);
         MeasureToString (pPage, L"ripplesize", pv->m_fRippleSize);
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

         MeasureParseString (pPage, L"tablewidth", &pv->m_pSizeTable.p[0]);
         pv->m_pSizeTable.p[0] = max(CLOSE, pv->m_pSizeTable.p[0]);
         MeasureParseString (pPage, L"tablelength", &pv->m_pSizeTable.p[1]);
         pv->m_pSizeTable.p[1] = max(CLOSE, pv->m_pSizeTable.p[1]);
         MeasureParseString (pPage, L"clothwidth", &pv->m_pSizeCloth.p[0]);
         pv->m_pSizeCloth.p[0]= max(pv->m_pSizeTable.p[0] * 1.01, pv->m_pSizeCloth.p[0]);
         MeasureParseString (pPage, L"clothlength", &pv->m_pSizeCloth.p[1]);
         pv->m_pSizeCloth.p[1]= max(pv->m_pSizeTable.p[1] * 1.01, pv->m_pSizeCloth.p[1]);
         MeasureParseString (pPage, L"ripplesize", &pv->m_fRippleSize);
         pv->m_fRippleSize = max (pv->m_fRippleSize, .01);

         pv->CalcInfo ();
         pv->m_pWorld->ObjectChanged (pv);


         break;
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Table cloth settings";
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}


/**********************************************************************************
CObjectTableCloth::DialogShow - Causes the object to show a dialog box that allows
different attributes to be changed.

inputs
   DWORD             dwSuface - Surface that was clicked on. 0 if unknown.
   PCEscWindow       pWindow - Escarpment window to draw it in
returns
   BOOL - TRUE if user pressed "Back", FALSE if user pressed "Close"
*/
BOOL CObjectTableCloth::DialogShow (DWORD dwSurface, PCEscWindow pWindow, PCPoint pClick)
{
   PWSTR pszRet;
   pszRet = pWindow->PageDialog (ghInstance, IDR_MMLTABLECLOTHDIALOG, TableClothDialogPage, this);
   if (!pszRet)
      return FALSE;

   return !_wcsicmp(pszRet, Back());
}


/**********************************************************************************
CObjectTableCloth::DialogQuery - Returns TRUE if the object supports a custom
dialog box, FALSE if it doesn't. The default object doesn't
*/
BOOL CObjectTableCloth::DialogQuery (void)
{
   return TRUE;
}

