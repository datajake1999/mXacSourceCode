/************************************************************************
CObjectTarp.cpp - Draws a Tarp.

begun 26/9/02 by Mike Rozak
Copyright 2002 Mike Rozak. All rights reserved
*/

#include <windows.h>
#include "escarpment.h"
#include "..\M3D.h"
#include "resource.h"

/**********************************************************************************
CObjectTarp::Constructor and destructor */
CObjectTarp::CObjectTarp (PVOID pParams, POSINFO pInfo)
{
   m_dwRenderShow = RENDERSHOW_MISC;
   m_OSINFO = *pInfo;

   if ((DWORD)(size_t) pParams == 3)
      m_dwNumCorner = 3;
   else
      m_dwNumCorner = 4;
   memset (m_apPoint, 0, sizeof(m_apPoint));

   m_apPoint[0].p[0] = 1;
   m_apPoint[0].p[1] = -1;
   m_apPoint[2].p[0] = -1;
   m_apPoint[2].p[1] = -1;
   if (m_dwNumCorner == 3) {
      m_apPoint[4].p[1] = 1;
   }
   else {
      m_apPoint[4].p[0] = -1;
      m_apPoint[4].p[1] = 1;
      m_apPoint[6].p[0] = 1;
      m_apPoint[6].p[1] = 1;
   }
   DWORD i;
   for (i = 1; i < m_dwNumCorner*2; i += 2) {
      m_apPoint[i].Average (&m_apPoint[i-1], &m_apPoint[(i+1)%(m_dwNumCorner*2)]);
      m_apPoint[i].p[2] = -.5;
   }

   m_dwNumPoints = m_dwNumNormals = m_dwNumText = 0;
   m_dwNumVertex = m_dwNumPoly = 0;
   CalcTarp();

   // color for the Tarp
   ObjectSurfaceAdd (1, RGB(0x40,0x40,0xff), MATERIAL_CLOTHSMOOTH, L"Tarp");
}


CObjectTarp::~CObjectTarp (void)
{
   // do nothing for now
}


/**********************************************************************************
CObjectTarp::Delete - Called to delete this object
*/
void CObjectTarp::Delete (void)
{
   delete this;
}

/**********************************************************************************
CObjectTarp::Render - Draws a test box.

inputs
   POBJECTRENDER     pr - Render information
*/
void CObjectTarp::Render (POBJECTRENDER pr, DWORD dwSubObject)
{
   // create the surface render object and draw
   // CRenderSurface rs;
   m_Renderrs.ClearAll();

   CMatrix mObject;
   m_Renderrs.Init (pr->pRS);
   ObjectMatrixGet (&mObject);
   m_Renderrs.Multiply (&mObject);

   // object specific
   m_Renderrs.SetDefMaterial (m_OSINFO.dwRenderShard, ObjectSurfaceFind (1), m_pWorld);

   PCPoint pPoint, pNormal;
   PTEXTPOINT5 pText;
   PVERTEX pVert;
   DWORD dwPointIndex, dwNormalIndex, dwTextIndex, dwVertIndex;

   DWORD i;
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


/**********************************************************************************
CObjectTarp::QueryBoundingBox - Standard API
*/
void CObjectTarp::QueryBoundingBox (PCPoint pCorner1, PCPoint pCorner2, DWORD dwSubObject)
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
      OutputDebugString ("\r\nCObjectTarp::QueryBoundingBox too small.");
#endif
}

/**********************************************************************************
CObjectTarp::Clone - Clones this

inputs
   none
returns
   PCObjectSocket - Clone
*/
PCObjectSocket CObjectTarp::Clone (void)
{
   PCObjectTarp pNew;

   pNew = new CObjectTarp(NULL, &m_OSINFO);

   // clone template info
   CloneTemplate(pNew);

   pNew->m_dwNumCorner = m_dwNumCorner;
   memcpy (pNew->m_apPoint, m_apPoint, sizeof(m_apPoint));

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

   return pNew;
}

static PWSTR gpszNumCorner = L"NumCorner";
static PWSTR gpszPoint = L"Point%d";

PCMMLNode2 CObjectTarp::MMLTo (void)
{
   // call into the template first
   PCMMLNode2 pNode = MMLToTemplate();
   if (!pNode)
      return NULL;

   MMLValueSet (pNode, gpszNumCorner, (int) m_dwNumCorner);

   DWORD i;
   WCHAR szTemp[32];
   for (i = 0; i < m_dwNumCorner*2; i++) {
      swprintf (szTemp, gpszPoint, (int) i);
      MMLValueSet (pNode, szTemp, &m_apPoint[i]);
   }


   return pNode;
}

BOOL CObjectTarp::MMLFrom (PCMMLNode2 pNode)
{
   // call into the template first
   if (!MMLFromTemplate (pNode))
      return FALSE;

   m_dwNumCorner = (DWORD) MMLValueGetInt (pNode, gpszNumCorner, (int) 3);

   DWORD i;
   WCHAR szTemp[32];
   for (i = 0; i < m_dwNumCorner*2; i++) {
      swprintf (szTemp, gpszPoint, (int) i);
      MMLValueGetPoint (pNode, szTemp, &m_apPoint[i]);
   }

   // Recalc
   CalcTarp();

   return TRUE;
}


/**********************************************************************************
CObjectTarp::CalcTarp - Calculates the path polygons given all the settings.
*/
BOOL CObjectTarp::CalcTarp (void)
{
#define TARPDETAIL      13

   // go through all the edges and calculate the sphere the determines their
   // shape
   CMatrix amFromSC[4];    // conver from a sin() and cos() point. See ThreePointsToCircle()
   BOOL afCircle[4];  // set to TRUE if it's a circle, FALSE if linear
   fp afAngle[4][2]; // two angles
   fp afDist[4];  // distance between this corner and the next one
   CPoint pT;
   DWORD i, j;
   for (i = 0; i < m_dwNumCorner; i++) {
      fp fRadius;
      afCircle[i] = ThreePointsToCircle (&m_apPoint[i*2], &m_apPoint[i*2+1],
         &m_apPoint[(i*2+2)%(m_dwNumCorner*2)], &amFromSC[i],
         &afAngle[i][0], &afAngle[i][1], &fRadius);

      if (afCircle[i]) {
         afDist[i] = fabs(afAngle[i][0] - afAngle[i][1]) * fRadius;
      }
      else {
         pT.Subtract (&m_apPoint[i*2], &m_apPoint[((i+1)%m_dwNumCorner)*2]);
         afDist[i] = pT.Length();
      }
   }

   BOOL fRect;
   fRect = (m_dwNumCorner == 4);

   // precalculate thw width and height of the fabric, so can do textures
   fp fTextWidth, fTextHeight;
   if (fRect) {
      fTextWidth = (afDist[0] + afDist[2]) / 2.0;
      fTextHeight = (afDist[1] + afDist[3]) / 2.0;
   }
   else {
      fTextWidth = afDist[0];
      fTextHeight = (afDist[1] + afDist[2]) / 2.0 / sqrt((fp)2);   // assume triangle even
   }

   // precalculate the points along the edge
   CPoint apPreCalc[4][TARPDETAIL];
   CPoint apPreCalcLinear[4][TARPDETAIL];
   for (i = 0; i < m_dwNumCorner; i++) {
      for (j = 0; j < TARPDETAIL; j++) {
         // BUGFIX - Also calculate the linear amount
         apPreCalcLinear[i][j].Average (&m_apPoint[i*2], &m_apPoint[((i+1)%m_dwNumCorner)*2],
            1.0 - ((fp) j / (fp) (TARPDETAIL-1)));

         // if it's linear then just average
         if (!afCircle[i]) {
            apPreCalc[i][j].Copy (&apPreCalcLinear[i][j]);
            continue;
         }

         // what angle
         fp fAngle;
         fAngle = (fp) j / (fp) (TARPDETAIL-1);
         fAngle = (1.0 - fAngle) * afAngle[i][0] + fAngle * afAngle[i][1];

         // sine and cos
         CPoint p;
         p.Zero();
         p.p[0] = sin(fAngle);
         p.p[1] = cos(fAngle);

         // convert
         p.MultiplyLeft (&amFromSC[i]);
         apPreCalc[i][j].Copy (&p);
      }
   }

   // zero out
   m_dwNumPoints = m_dwNumNormals = m_dwNumText = 0;
   m_dwNumVertex = m_dwNumPoly = 0;

   // so, we know how many points, etc. we need
   m_dwNumPoints = fRect ? (TARPDETAIL * TARPDETAIL) : (TARPDETAIL * (TARPDETAIL+1) / 2);
   m_dwNumNormals = m_dwNumPoints;
   m_dwNumText = m_dwNumPoints;
   m_dwNumVertex = m_dwNumPoints;
   m_dwNumPoly = fRect ? ((TARPDETAIL-1)*(TARPDETAIL-1)) : (TARPDETAIL * (TARPDETAIL-1)/ 2);

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


   // calculate the points and texturepoints
   PCPoint p;
   DWORD x, y;
   fp fCurV;
   PTEXTPOINT5 pt;
   fCurV = 0;
   p = pPoint;
   pt = pText;
   CPoint p1, p2, pTB, pLR;
   for (y = 0; y < TARPDETAIL; y++) // bottom to top
      for (x = 0; x < TARPDETAIL; x++, p++, pt++) { // left to right
         if (!fRect && (x+y >= TARPDETAIL))
            break;

         // textures
         if (fRect) {
            pt->hv[0] = (fp)x / (fp)(TARPDETAIL-1) * fTextWidth;
            pt->hv[1] = -(fp)y / (fp)(TARPDETAIL-1) * fTextHeight;
         }
         else {
            pt->hv[0] = ((fp)x+(fp)y/2.0) / (fp)(TARPDETAIL-1) * fTextWidth;
            pt->hv[1] = -(fp)y / (fp)(TARPDETAIL-1) * fTextHeight;
         }


         // points
         if (fRect) {   // rectangle
            // BUGFIX - To make it sag properly, first calculate the location based on
            // sag in just one direction
            p1.Average (&apPreCalc[1][y], &apPreCalc[3][TARPDETAIL-y-1],
               1.0 - (fp) x / (fp)(TARPDETAIL-1)); // if T&B edges held firm
            p2.Average (&apPreCalcLinear[1][y], &apPreCalcLinear[3][TARPDETAIL-y-1],
               1.0 - (fp) x / (fp)(TARPDETAIL-1)); // if T&B edges held firm
            pTB.Subtract (&p1, &p2);

            p1.Average (&apPreCalc[0][TARPDETAIL-x-1], &apPreCalc[2][x],
               1.0 - (fp) y / (fp)(TARPDETAIL-1)); // if L&R edges helf firm
            p2.Average (&apPreCalcLinear[0][TARPDETAIL-x-1], &apPreCalcLinear[2][x],
               1.0 - (fp) y / (fp)(TARPDETAIL-1)); // if L&R edges helf firm
            pLR.Subtract (&p1, &p2);


            // now, assume that all tight...
            p->Average (&apPreCalcLinear[1][y], &apPreCalcLinear[3][TARPDETAIL-y-1],
               1.0 - (fp) x / (fp)(TARPDETAIL-1)); // if T&B edges held firm

            p->Add (&pTB);
            p->Add (&pLR);

#if 0 // OLDCODE that doesnt sag properly
            p1.Average (&apPreCalc[1][y], &apPreCalc[3][TARPDETAIL-y-1],
               1.0 - (fp) x / (fp)(TARPDETAIL-1)); // if T&B edges held firm
            p2.Average (&apPreCalc[0][TARPDETAIL-x-1], &apPreCalc[2][x],
               1.0 - (fp) y / (fp)(TARPDETAIL-1)); // if L&R edges helf firm

            // figure out a score along each direction
            dwScore1 = (x < TARPDETAIL/2) ? x : (TARPDETAIL - x - 1);
            dwScore2 = (y < TARPDETAIL/2) ? y : (TARPDETAIL - y - 1);
            if (dwScore1 + dwScore2 == 0)
               dwScore1 = dwScore2 = 1;   // always at least some score so can divide

            p->Average (&p1, &p2, (fp) dwScore2 / (fp)(dwScore1 + dwScore2));
#endif // 0
         }
         else {   // triangle
            // BUGFIX - Make sag properly
            p1.Average (&apPreCalc[1][y], &apPreCalc[2][TARPDETAIL-y-1],
               (y+1 < TARPDETAIL) ? (1.0 - (fp) x / (fp)(TARPDETAIL-y-1)) :0) ; // if T&B edges held firm
            p2.Average (&apPreCalcLinear[1][y], &apPreCalcLinear[2][TARPDETAIL-y-1],
               (y+1 < TARPDETAIL) ? (1.0 - (fp) x / (fp)(TARPDETAIL-y-1)) :0) ; // if T&B edges held firm
            pTB.Subtract (&p1, &p2);

            fp fDiv;
            fDiv = TARPDETAIL-x-1;
            if (!fDiv)
               fDiv = 1;
            p1.Average (&apPreCalc[0][TARPDETAIL-x-1], &m_apPoint[4],
               1.0 - (fp) y / fDiv); // if L&R edges helf firm
            p2.Average (&apPreCalcLinear[0][TARPDETAIL-x-1], &m_apPoint[4],
               1.0 - (fp) y / fDiv); // if L&R edges helf firm
            pLR.Subtract (&p1, &p2);

            p->Average (&apPreCalcLinear[1][y], &apPreCalcLinear[2][TARPDETAIL-y-1],
               (y+1 < TARPDETAIL) ? (1.0 - (fp) x / (fp)(TARPDETAIL-y-1)) :0) ; // if T&B edges held firm
            p->Add (&pTB);
            p->Add (&pLR);

#if 0 // OLD CODE - DOesnt sag properly
            p1.Average (&apPreCalc[1][y], &apPreCalc[2][TARPDETAIL-y-1],
               (y+1 < TARPDETAIL) ? (1.0 - (fp) x / (fp)(TARPDETAIL-y-1)) :0) ; // if T&B edges held firm
            p2.Average (&apPreCalc[0][TARPDETAIL-x-1], &m_apPoint[4],
               1.0 - (fp) y / (fp)(TARPDETAIL-1)); // if L&R edges helf firm

            // figure out a score along each direction
            dwScore1 = (x < (TARPDETAIL-y)/2) ? x : (TARPDETAIL - y - x - 1);
            dwScore2 = (y < TARPDETAIL/2) ? y : (TARPDETAIL - y - 1);
            if (dwScore1 + dwScore2 == 0)
               dwScore1 = dwScore2 = 1;   // always at least some score so can divide

            p->Average (&p1, &p2, (fp) dwScore2 / (fp)(dwScore1 + dwScore2));
#endif // 0
         }

         // xyz for texture point
         pt->xyz[0] = p->p[0];
         pt->xyz[1] = p->p[1];
         pt->xyz[2] = p->p[2];

      }  // over y and x

   
   // calculate the normals
   PCPoint pp2;
   p = pNormal;
   pp2 = pPoint;
   for (y = 0; y < TARPDETAIL; y++) // bottom to top
      for (x = 0; x < TARPDETAIL; x++, p++, pp2++) { // left to right
         if (!fRect && (x+y >= TARPDETAIL))
            break;

         PCPoint pLeft, pRight, pAbove, pBelow;
         if (fRect) {
            pLeft = x ? (pp2 - 1) : pp2;
            pRight = (x+1 < TARPDETAIL) ? (pp2+1) : pp2;
            pBelow = y ? (pp2 - TARPDETAIL) : pp2;
            pAbove = (y+1 < TARPDETAIL) ? (pp2 + TARPDETAIL) : pp2;
         }
         else {
            pLeft = x ? (pp2 - 1) : pp2;
            pRight = (x+1+y < TARPDETAIL) ? (pp2+1) : pp2;
            if (pLeft == pRight) // only on the very top of the triangle
               pRight = pp2 - 1;
            pBelow = y ? (pp2 - (TARPDETAIL - y + 1)) : pp2;
            if (y+1 >= TARPDETAIL)  // only on the very top
               pAbove = pp2;
            else
               pAbove = (y+x+1 >= TARPDETAIL) ?
                  (pp2 + (TARPDETAIL - y - 1)) : (pp2 + (TARPDETAIL - y));
         }

         CPoint pR, pU;
         pR.Subtract (pRight, pLeft);
         pU.Subtract (pAbove, pBelow);
         p->CrossProd (&pR, &pU);
         p->Normalize();
      }

   
   // all the vertices
   PVERTEX pv;
   pv = pVert;
   for (i = 0; i < m_dwNumVertex; i++, pv++) {
      pv->dwNormal = pv->dwPoint = pv->dwTexture = i;
   }


   // and the polygons
   DWORD *padw;
   DWORD dwVert;
   padw = pPoly;
   dwVert = 0;
   for (y = 0; y < TARPDETAIL-1; y++) { // bottom to top
      for (x = 0; x < TARPDETAIL-1; x++, padw += 4, dwVert++) { // left to right
         if (!fRect && (x+y+1 >= TARPDETAIL))
            break;

         if (fRect) {
            padw[0] = dwVert+1;
            padw[1] = dwVert;
            padw[2] = dwVert + TARPDETAIL;
            padw[3] = dwVert+1 +  TARPDETAIL;
         }
         else { // triangle
            padw[0] = dwVert+1;
            padw[1] = dwVert;
            padw[2] = dwVert + (TARPDETAIL-y);
            padw[3] = (x+y+2 < TARPDETAIL) ? (dwVert+1 + (TARPDETAIL)-y) : -1;
         }

      }

      // increase vertex once more since didn't quite increase enough above
      dwVert++;
   }

   return TRUE;
}

/**************************************************************************************
CObjectTarp::MoveReferencePointQuery - 
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
BOOL CObjectTarp::MoveReferencePointQuery (DWORD dwIndex, PCPoint pp)
{
   if (dwIndex > m_dwNumCorner)
      return FALSE;

   if (dwIndex)
      pp->Copy (&m_apPoint[(dwIndex-1) * 2]);
   else {
      DWORD i;

      pp->Zero();
      for (i = 0; i < m_dwNumCorner*2; i++)
         pp->Add (&m_apPoint[i]);
      pp->Scale (1.0 / (fp) (m_dwNumCorner*2));
   }

   return TRUE;
}

/**************************************************************************************
CObjectTarp::MoveReferenceStringQuery -
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
BOOL CObjectTarp::MoveReferenceStringQuery (DWORD dwIndex, PWSTR psz, DWORD dwSize, DWORD *pdwNeeded)
{
   if (dwIndex > m_dwNumCorner)
      return FALSE;

   WCHAR szTemp[32];
   if (dwIndex)
      swprintf (szTemp, L"Corner %d", (int) dwIndex);
   else
      swprintf (szTemp, L"Center");

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
CObjectTarp::ControlPointQuery - Called to query the information about a control
point identified by dwID.

inputs
   DWORD       dwID - ID
   POSCONTROL  pInfo - Filled in with the infomration
returns
   BOOL - TRUE if successful
*/
BOOL CObjectTarp::ControlPointQuery (DWORD dwID, POSCONTROL pInfo)
{
   if (dwID >= m_dwNumCorner*2 + 3)
      return FALSE;

   CPoint pMin, pMax, pCenter, pScale, p;
   DWORD i;
   for (i = 0; i < m_dwNumCorner*2; i++) {
      p.Copy (&m_apPoint[i]);
      if (i) {
         pMin.Min (&p);
         pMax.Max (&p);
      }
      else {
         pMin.Copy (&p);
         pMax.Copy (&p);
      }
   }
   pCenter.Average (&pMin, &pMax);
   pScale.Subtract (&pMax, &pMin);
   for (i = 0; i < 3; i++)
      pScale.p[i] = max(pScale.p[i], CLOSE);

   fp fKnobSize;
   fKnobSize = max(max(pScale.p[0], pScale.p[1]), pScale.p[2]) * .02;

   memset (pInfo,0, sizeof(*pInfo));

   pInfo->dwID = dwID;
   //pInfo->dwFreedom = 0;   // any direction
   pInfo->fSize = fKnobSize;

   if (dwID >= m_dwNumCorner*2) {
      dwID -= m_dwNumCorner*2;
      pInfo->dwStyle = CPSTYLE_SPHERE;
      pInfo->cColor = RGB(0xff,0,0xff);
      switch (dwID) {
      default:
      case 0:
         wcscpy (pInfo->szName, L"Width");
         break;
      case 1:
         wcscpy (pInfo->szName, L"Length");
         break;
      case 2:
         wcscpy (pInfo->szName, L"Height");
         break;
      }
      pInfo->pLocation.Copy (&pCenter);
      pInfo->pLocation.p[dwID] += pScale.p[dwID]/2;
   }
   else if (dwID % 2) {
      pInfo->dwStyle = CPSTYLE_POINTER;
      pInfo->cColor = RGB(0xff,0xff, 0);
      wcscpy (pInfo->szName, L"Sag");
      pInfo->pLocation.Copy (&m_apPoint[dwID]);
      //pInfo->pDirection.Zero();
      //pInfo->pDirection.p[2] = -1;
   }
   else {
      pInfo->dwStyle = CPSTYLE_CUBE;
      pInfo->cColor = RGB(0,0xff,0xff);
      wcscpy (pInfo->szName, L"Corner");
      pInfo->pLocation.Copy (&m_apPoint[dwID]);
   }

   return TRUE;
}

/*************************************************************************************
CObjectTarp::ControlPointSet - Called to change the valud of a control point.

inputs
   DWORD       dwID - ID
   PCPoint     pVal - Contains the new location, in object coordinates
returns
   BOOL - TRUE if successful
*/
BOOL CObjectTarp::ControlPointSet (DWORD dwID, PCPoint pVal, PCPoint pViewer)
{
   if (dwID >= m_dwNumCorner*2+3)
      return FALSE;

   CPoint pMin, pMax, pCenter, pScale, p;
   DWORD i;
   for (i = 0; i < m_dwNumCorner*2; i++) {
      p.Copy (&m_apPoint[i]);
      if (i) {
         pMin.Min (&p);
         pMax.Max (&p);
      }
      else {
         pMin.Copy (&p);
         pMax.Copy (&p);
      }
   }
   pCenter.Average (&pMin, &pMax);
   pScale.Subtract (&pMax, &pMin);
   for (i = 0; i < 3; i++)
      pScale.p[i] = max(pScale.p[i], CLOSE);


   // tell the world we're about to change
   if (m_pWorld)
      m_pWorld->ObjectAboutToChange (this);


   // Deal with resizing
   if (dwID >= m_dwNumCorner*2) {
      dwID -= m_dwNumCorner*2;

      fp fVal;
      fVal = (pVal->p[dwID] - pCenter.p[dwID]) * 2;
      fVal = max(CLOSE, fVal);
      fVal = fVal / pScale.p[dwID];

      for (i = 0; i < m_dwNumCorner*2; i++) {
         m_apPoint[i].p[dwID] = (m_apPoint[i].p[dwID] - pCenter.p[dwID]) * fVal + pCenter.p[dwID];
      }
   }
   else
      m_apPoint[dwID].Copy (pVal);

   CalcTarp();

   // tell the world we've changed
   if (m_pWorld)
      m_pWorld->ObjectChanged (this);
   return TRUE;
}

/*************************************************************************************
CObjectTarp::ControlPointEnum - Called to enumerate a list of control point IDs
into the list.

inputs
   PCListFixed       plDWORD - Should be filled in with control points
returns
   none
*/
void CObjectTarp::ControlPointEnum (PCListFixed plDWORD)
{
   DWORD i;

   DWORD dwNum = m_dwNumCorner*2 + 3;
   for (i = 0; i < dwNum; i++)
      plDWORD->Add (&i);
}

// FUTURERELEASE - Message between tarps when move so that two or more tarps
// become glued together

