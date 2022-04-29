/************************************************************************
CObjectPolyhedron.cpp - Draws a Polyhedron.

begun 1/9/02 by Mike Rozak
Copyright 2002 Mike Rozak. All rights reserved
*/

#include <windows.h>
#include "escarpment.h"
#include "..\M3D.h"
#include "resource.h"


/**********************************************************************************
globals */

// default vertex locations
static CPoint gapVertexCube[] = {
   -1,-1,-1,0,     // point 0
   1,-1,-1,0,     // 1
   -1,1,-1,0,     // 2
   1,1,-1,0,      // 3
   -1,-1,1,0,     // 4
   1,-1,1,0,      // 5
   -1,1,1,0,      // 6
   1,1,1,0,       // 7
   0,0,0,0        // end
};


// vertex to polygons.. go clockwise from lower-right, -1 => no point
static DWORD gadwPolygonCube[] = {
   1, 0, 4, 5, -1, -1,        // front
   0, 2, 6, 4, -1, -1,        // left
   2, 3, 7, 6, -1, -1,        // back
   3, 1, 5, 7, -1, -1,        // right
   5, 4, 6, 7, -1, -1,        // top
   3, 2, 0, 1, -1, -1,        // bottom
   -1,-1,-1,-1,-1,-1          // end
};

// Pyramid
static CPoint gapVertexPyramid[] = {
   -1,-1,-1,0,     // point 0
   1,-1,-1,0,     // 1
   -1,1,-1,0,     // 2
   1,1,-1,0,      // 3
   0,0,-1+sqrt((fp)2),0,     // 4
   0,0,0,0        // end
};

static DWORD gadwPolygonPyramid[] = {
   1, 0, 4, -1, -1, -1,        // front
   0, 2, 4, -1, -1, -1,        // left
   2, 3, 4, -1, -1, -1,        // back
   3, 1, 4, -1, -1, -1,        // right
   3, 2, 0, 1, -1, -1,        // bottom
   -1,-1,-1,-1,-1,-1          // end
};

// double pyramid
static CPoint gapVertexDoublePyramid[] = {
   -1,-1,0,0,     // point 0
   1,-1,0,0,     // 1
   -1,1,0,0,     // 2
   1,1,0,0,      // 3
   0,0,sqrt((fp)2),0,     // 4
   0,0,-sqrt((fp)2),0,     // 5
   0,0,0,0        // end
};

static DWORD gadwPolygonDoublePyramid[] = {
   1, 0, 4, -1, -1, -1,        // front
   0, 2, 4, -1, -1, -1,        // left
   2, 3, 4, -1, -1, -1,        // back
   3, 1, 4, -1, -1, -1,        // right
   0, 1, 5, -1, -1, -1,        // b front
   2, 0, 5, -1, -1, -1,        // b left
   3, 2, 5, -1, -1, -1,        // b back
   1, 3, 5, -1, -1, -1,        // b right
   -1,-1,-1,-1,-1,-1          // end
};


// square
static CPoint gapVertexSquare[] = {
   -1,-1,0,0,     // 4
   1,-1,0,0,      // 5
   -1,1,0,0,      // 6
   1,1,0,0,       // 7
   0,0,0,0        // end
};

static DWORD gadwPolygonSquare[] = {
   1, 0, 2, 3, -1, -1,        // top
   -1,-1,-1,-1,-1,-1          // end
};


// traiangle extruded
static CPoint gapVertexTriExt[] = {
   0,1 - sqrt((fp)2),-1,0,     // point 0
   -1,1,-1,0,     // 1
   1,1,-1,0,      // 2
   0,1 - sqrt((fp)2),1,0,     // point 3
   -1,1,1,0,      // 4
   1,1,1,0,       // 5
   0,0,0,0        // end
};

static DWORD gadwPolygonTriExt[] = {
   0, 1, 4, 3, -1, -1,        // left
   1, 2, 5, 4, -1, -1,        // back
   2, 0, 3, 5, -1, -1,        // right
   4, 5, 3, -1, -1, -1,        // top
   2, 1, 0, -1, -1, -1,        // bottom
   -1,-1,-1,-1,-1,-1          // end
};



// traiangle pyramid
static CPoint gapVertexTriPyramid[] = {
   0,1 - sqrt((fp)2),0,0,     // point 0
   -1,1,0,0,     // 1
   1,1,0,0,      // 2
   0,1 - sqrt((fp)2)/2,sqrt((fp)2),     // point 3
   0,0,0,0        // end
};

static DWORD gadwPolygonTriPyramid[] = {
   0, 1, 3, -1, -1, -1,        // left
   1, 2, 3, -1, -1, -1,        // back
   2, 0, 3, -1, -1, -1,        // right
   2, 1, 0, -1, -1, -1,        // bottom
   -1,-1,-1,-1,-1,-1          // end
};



// traiangle double pyramid
static CPoint gapVertexTriDoublePyramid[] = {
   0,1 - sqrt((fp)2),0,0,     // point 0
   -1,1,0,0,     // 1
   1,1,0,0,      // 2
   0,1 - sqrt((fp)2)/2,sqrt((fp)2),0,     // point 3
   0,1 - sqrt((fp)2)/2,-sqrt((fp)2),0,     // point 4
   0,0,0,0        // end
};

static DWORD gadwPolygonTriDoublePyramid[] = {
   0, 1, 3, -1, -1, -1,        // left
   1, 2, 3, -1, -1, -1,        // back
   2, 0, 3, -1, -1, -1,        // right
   1, 0, 4, -1, -1, -1,        // b,left
   2, 1, 4, -1, -1, -1,        // b,back
   0, 2, 4, -1, -1, -1,        // b,right
   -1,-1,-1,-1,-1,-1          // end
};



// traiangle
static CPoint gapVertexTri[] = {
   0,1 - sqrt((fp)2),0,0,     // point 0
   -1,1,0,0,     // 1
   1,1,0,0,      // 2
   0,0,0,0        // end
};

static DWORD gadwPolygonTri[] = {
   1, 2, 0, -1, -1, -1,        // top
   -1,-1,-1,-1,-1,-1          // end
};


typedef struct {
   CPoint         *papVertex;    // pointer to gapVertexXXX
   DWORD          *padwPoly;     // pointer to gadwPolygonXXX
} PHINFO, *PPHINFO;

static PHINFO gaPHInfo[] = {
   gapVertexCube, gadwPolygonCube,
   gapVertexPyramid, gadwPolygonPyramid,
   gapVertexDoublePyramid, gadwPolygonDoublePyramid,
   gapVertexSquare, gadwPolygonSquare,
   gapVertexTriExt, gadwPolygonTriExt,
   gapVertexTriPyramid, gadwPolygonTriPyramid,
   gapVertexTriDoublePyramid, gadwPolygonTriDoublePyramid,
   gapVertexTri, gadwPolygonTri
};

/**********************************************************************************
CObjectPolyhedron::Constructor and destructor */
CObjectPolyhedron::CObjectPolyhedron (PVOID pParams, POSINFO pInfo)
{
   m_dwRenderShow = RENDERSHOW_MISC;
   m_OSINFO = *pInfo;

   m_dwDisplayControl = 0;    // default to scale
   m_dwDisplayConstraint = 0; // full constraint
   m_dwShape = (DWORD)(size_t) pParams;  // default to shape based on settings
   m_lPoints.Init (sizeof(CPoint));

   // make sure shape is in range
   m_dwShape = m_dwShape % (sizeof(gaPHInfo) / sizeof(PHINFO));

   // Need to fill in m_lPoints
   DWORD i;
   CPoint p;
   PCPoint pap;
   pap = gaPHInfo[m_dwShape].papVertex;
   for (i = 0; ; i++) {
      p.Copy (&pap[i]);

      if ((p.p[0] == 0) && (p.p[1] == 0) && (p.p[2] == 0) && (p.p[3] == 0))
         break;
      p.Scale (.5);  // since going from -1 to 1
      m_lPoints.Add (&p);
   }

   // calculate the scale
   CalcScale ();

   // color for the Polyhedron
   TEXTUREMODS tm;
   memset (&tm, 0, sizeof(tm));
   tm.cTint = RGB(0xff,0xff,0xff);
   tm.wBrightness = 0x1000;
   tm.wContrast = 0x1000;
   tm.wHue = 0x0000;
   tm.wSaturation = 0x1000;
   ObjectSurfaceAdd (42, RGB(0xff,0xff,0x80), MATERIAL_TILEGLAZED);
}


CObjectPolyhedron::~CObjectPolyhedron (void)
{
   // do nothing for now
}


/**********************************************************************************
CObjectPolyhedron::Delete - Called to delete this object
*/
void CObjectPolyhedron::Delete (void)
{
   delete this;
}


/**********************************************************************************
CObjectPolyhedron::RenderFacet - Draws one facet of the polyhedron.

inputs
   PCRenderSurface   prs - Where to render to
   PCPoint           papPoint - Pointer to an array of points to use, as indexed by vertices
   DWORD             *padwVertex - Pointer to an array of 6 vertices. (-1 means none).
                     Starting from lower right, to lower-left, and around clockwise
   DWORD             dwPointIndex - Point index, for all the points, so don't need to add them
                     and share one point among several verticies
returns
   noen
*/
void CObjectPolyhedron::RenderFacet (PCRenderSurface prs,
                                     PCPoint papPoint, DWORD *padwVertex, DWORD dwPointIndex)
{
   DWORD i;

   // figure out how many vertexes
   DWORD dwNum;
   for (dwNum = 0; dwNum < 6; dwNum++)
      if (padwVertex[dwNum] == -1)
         break;
   if (dwNum < 3)
      return;

   // make the textures
   PTEXTPOINT5 ptp;
   DWORD dwText;
   ptp = prs->NewTextures (&dwText, dwNum);
   if (ptp) {
      // rememver point 1
      CPoint p1;
      p1.Copy (&papPoint[padwVertex[1]]);

      // figure out normal to surface, and point 1 (lower-right)
      CPoint pN;
      CPoint pA, pB;
      pA.Subtract (&papPoint[padwVertex[0]], &p1);
      pB.Subtract (&papPoint[padwVertex[2]], &p1);
      pN.CrossProd (&pA, &pB);

      // also know direction to right, pA
      pA.Normalize();

      // so know pB
      pB.CrossProd (&pN, &pA);
      pB.Normalize();

      // and quick refernce center vertex
      for (i = 0; i < dwNum; i++) {
         // set volumetrix texture info
         ptp[i].xyz[0] = papPoint[padwVertex[i]].p[0];
         ptp[i].xyz[1] = papPoint[padwVertex[i]].p[1];
         ptp[i].xyz[2] = papPoint[padwVertex[i]].p[2];

         if (i == 1) {
            // trivial case
            ptp[1].hv[0] = 0;
            ptp[1].hv[1] = -CLOSE;
            continue;
         }

         CPoint p;
         p.Copy (&papPoint[padwVertex[i]]);
         p.Subtract (&p1);

         // how far along h?
         ptp[i].hv[0] = p.DotProd (&pA);
         ptp[i].hv[1] = -CLOSE - p.DotProd(&pB);
      }

      // apply the rotation
      prs->ApplyTextureRotation (ptp, dwNum);
   }

   DWORD dwNormIndex;
   PCPoint paNorm;
   BOOL  fUseSameNormal;
   dwNormIndex = 0;
   paNorm = NULL;
   fUseSameNormal = TRUE;
   if (prs->m_fNeedNormals) {
      // make the normals - Use a different normal per vertex since the user may
      // have twisted the polygon?
      CPoint pN[6];
      CPoint pA, pB;
      for (i = 0; i < dwNum; i++) {
         // if get so far in polygon and still using the same normal then will always
         // use the same normal, so save CPU
         if (fUseSameNormal && (i >= (dwNum-2)))
            break;

         pA.Subtract (&papPoint[padwVertex[(i+dwNum-1)%dwNum]], &papPoint[padwVertex[i]]);
         pB.Subtract (&papPoint[padwVertex[(1+i)%dwNum]], &papPoint[padwVertex[i]]);
         pN[i].CrossProd (&pA, &pB);
         pN[i].Normalize();

         // if not the same then indicate dont use same normal
         if (i && fUseSameNormal && !pN[i].AreClose(&pN[i-1]))
            fUseSameNormal = FALSE;
      }

      paNorm = prs->NewNormals (TRUE, &dwNormIndex, fUseSameNormal ? 1 : dwNum);
      if (paNorm) {
         for (i = 0; i < (fUseSameNormal ? 1 : dwNum); i++)
            paNorm[i].Copy (&pN[i]);
      }
   }

   // add the vertices
   PVERTEX pav;
   DWORD dwVertIndex;
   pav = prs->NewVertices (&dwVertIndex, dwNum);
   if (!pav)
      return;
   for (i = 0; i < dwNum; i++, pav++) {
      pav->dwNormal = dwNormIndex + (fUseSameNormal ? 0 : i);
      pav->dwPoint = dwPointIndex + padwVertex[i];
      pav->dwTexture = ptp ? (dwText + i) : 0;
   }

   // add the polygon
   PPOLYDESCRIPT ppd;
   ppd = prs->NewPolygon (dwNum);
   if (!ppd)
      return;
   // defaults are all correct
   ppd->fCanBackfaceCull = fUseSameNormal;   // so if a side is bent always end up drawing
   if (gaPHInfo[m_dwShape].padwPoly[6] == -1)
      ppd->fCanBackfaceCull = FALSE;   // since only one side
   DWORD *padw;
   padw = (DWORD*)(ppd+1);
   for (i = 0; i < dwNum; i++)
      padw[i] = dwVertIndex + i;
   
   // done
}

/**********************************************************************************
CObjectPolyhedron::Render - Draws a test box.

inputs
   POBJECTRENDER     pr - Render information
*/
void CObjectPolyhedron::Render (POBJECTRENDER pr, DWORD dwSubObject)
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

   // make sure shape is in range
   m_dwShape = m_dwShape % (sizeof(gaPHInfo) / sizeof(PHINFO));


   // get the info
   PCPoint pap;
   DWORD    *padw;
   pap = gaPHInfo[m_dwShape].papVertex;
   padw = gaPHInfo[m_dwShape].padwPoly;

   // add all the points
   PCPoint pNewPoints;
   DWORD dwPointIndex, dwNumPoints;
   DWORD i;
   for (i = 0; ; i++) {
      if ((pap[i].p[0] == 0) && (pap[i].p[1] == 0) && (pap[i].p[2] == 0) && (pap[i].p[3] == 0))
         break;
   }
   dwNumPoints = i;
   if (dwNumPoints != m_lPoints.Num()) {
      m_Renderrs.Commit();
      return;  // error
   }
   pap = (PCPoint) m_lPoints.Get(0);

   pNewPoints = m_Renderrs.NewPoints (&dwPointIndex, dwNumPoints);
   if (!pNewPoints) {
      m_Renderrs.Commit();
      return;
   }
   for (i = 0; i < dwNumPoints; i++) {
      pNewPoints[i].Copy (&pap[i]);
   }

   for (i = 0; ;i++) {
      if (padw[i * 6] == -1)
         break;   // end

      // draw
      RenderFacet (&m_Renderrs, pap, padw + (i*6), dwPointIndex);
   }

   m_Renderrs.Commit();
}


/**********************************************************************************
CObjectPolyhedron::QueryBoundingBox - Standard API
*/
void CObjectPolyhedron::QueryBoundingBox (PCPoint pCorner1, PCPoint pCorner2, DWORD dwSubObject)
{
   BOOL fSet = FALSE;

   // get the info
   PCPoint pap;
   pap = (PCPoint) m_lPoints.Get(0);

   // add all the points
   DWORD i;
   for (i = 0; i < m_lPoints.Num(); i++) {
      if (fSet) {
         pCorner1->Min (&pap[i]);
         pCorner2->Max (&pap[i]);
      }
      else {
         pCorner1->Copy (&pap[i]);
         pCorner2->Copy (&pap[i]);
         fSet = TRUE;
      }
   }

#ifdef _DEBUG
   // test, make sure bounding box not too small
   CPoint p1,p2;
   //DWORD i;
   CObjectTemplate::QueryBoundingBox (&p1, &p2, dwSubObject);
   for (i = 0; i < 3; i++)
      if ((p1.p[i] < pCorner1->p[i] - CLOSE) || (p2.p[i] > pCorner2->p[i] + CLOSE))
         break;
   if (i < 3)
      OutputDebugString ("\r\nCObjectPolyhedron::QueryBoundingBox too small.");
#endif
}



/**********************************************************************************
CObjectPolyhedron::Clone - Clones this

inputs
   none
returns
   PCObjectSocket - Clone
*/
PCObjectSocket CObjectPolyhedron::Clone (void)
{
   PCObjectPolyhedron pNew;

   pNew = new CObjectPolyhedron(NULL, &m_OSINFO);

   // clone template info
   CloneTemplate(pNew);

   pNew->m_dwDisplayConstraint = m_dwDisplayConstraint;
   pNew->m_dwDisplayControl = m_dwDisplayControl;
   pNew->m_dwShape = m_dwShape;
   pNew->m_lPoints.Init (sizeof(CPoint), m_lPoints.Get(0), m_lPoints.Num());
   pNew->m_pScaleMax.Copy (&m_pScaleMax);
   pNew->m_pScaleMin.Copy (&m_pScaleMin);
   pNew->m_pScaleSize.Copy (&m_pScaleSize);
   return pNew;
}

static PWSTR gpszDisplayControl = L"DisplayControl";
static PWSTR gpszDisplayConstraint = L"DisplayConstraint";
static PWSTR gpszShape = L"Shape";

PCMMLNode2 CObjectPolyhedron::MMLTo (void)
{
   // call into the template first
   PCMMLNode2 pNode = MMLToTemplate();
   if (!pNode)
      return NULL;

   MMLValueSet (pNode, gpszDisplayControl, (int) m_dwDisplayControl);
   MMLValueSet (pNode, gpszDisplayConstraint, (int) m_dwDisplayConstraint);
   MMLValueSet (pNode, gpszShape, (int) m_dwShape);

   DWORD i;
   WCHAR szTemp[32];
   for (i = 0; i < m_lPoints.Num(); i++) {
      swprintf (szTemp, L"Vertex%d", (int) i);
      MMLValueSet (pNode, szTemp, (PCPoint) m_lPoints.Get(i));
   }

   return pNode;
}

BOOL CObjectPolyhedron::MMLFrom (PCMMLNode2 pNode)
{
   // call into the template first
   if (!MMLFromTemplate (pNode))
      return FALSE;


   m_dwDisplayControl = (DWORD) MMLValueGetInt (pNode, gpszDisplayControl, 0);
   m_dwDisplayConstraint = (DWORD) MMLValueGetInt (pNode, gpszDisplayConstraint, 0);
   m_dwShape = (DWORD) MMLValueGetInt (pNode, gpszShape, 0);

   m_lPoints.Clear();
   DWORD i;
   WCHAR szTemp[32];
   CPoint p;
   for (i = 0; ; i++) {
      swprintf (szTemp, L"Vertex%d", (int) i);
      if (!MMLValueGetPoint (pNode, szTemp, &p))
         break;
      m_lPoints.Add (&p);
   }

   // calc scale
   CalcScale ();
   return TRUE;
}


/**********************************************************************************
CObjectPolyhedron::DialogCPQuery - Returns TRUE if the object supports a custom
dialog box, FALSE if it doesn't. The default object doesn't
*/
BOOL CObjectPolyhedron::DialogCPQuery (void)
{
   return TRUE;
}



/* PolyhedronDisplayPage
*/
BOOL PolyhedronDisplayPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCObjectPolyhedron pv = (PCObjectPolyhedron)pPage->m_pUserData;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         PCEscControl pControl;

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
         }
         if (pControl)
            pControl->AttribSetBOOL (Checked(), TRUE);

         // contraints
         switch (pv->m_dwDisplayConstraint) {
         case 0:  // full
         default:
            pControl = pPage->ControlFind (L"full");
            break;
         case 1:  // some
            pControl = pPage->ControlFind (L"some");
            break;
         case 2:  // none
            pControl = pPage->ControlFind (L"none");
            break;
         }
         if (pControl)
            pControl->AttribSetBOOL (Checked(), TRUE);
      }
      break;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;

         // find otu which is checked
         PCEscControl pControl;
         DWORD dwNewMove, dwNewConst;
         dwNewMove = pv->m_dwDisplayControl;
         dwNewConst = pv->m_dwDisplayConstraint;
         if ((pControl = pPage->ControlFind (L"shape")) && pControl->AttribGetBOOL(Checked()))
            dwNewMove = 1;  // shape;
         else
            dwNewMove = 0; // scale
         
         if ((pControl = pPage->ControlFind (L"full")) && pControl->AttribGetBOOL(Checked()))
            dwNewConst = 0;
         else if ((pControl = pPage->ControlFind (L"some")) && pControl->AttribGetBOOL(Checked()))
            dwNewConst = 1;
         else
            dwNewConst = 2;
         if ((dwNewMove == pv->m_dwDisplayControl) && (dwNewConst == pv->m_dwDisplayConstraint))
            break;   // no change

         pv->m_pWorld->ObjectAboutToChange (pv);
         pv->m_dwDisplayControl = dwNewMove;
         pv->m_dwDisplayConstraint = dwNewConst;
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

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Which control points are displayed";
            return TRUE;
         }
      }
      break;
   };

   return DefPage (pPage, dwMessage, pParam);
}





/**********************************************************************************
CObjectPolyhedron::DialogCPShow - Causes the object to show a dialog box that allows
different attributes to be changed.

inputs
   DWORD             dwSuface - Surface that was clicked on. 0 if unknown.
   PCEscWindow       pWindow - Escarpment window to draw it in
returns
   BOOL - TRUE if user pressed "Back", FALSE if user pressed "Close"
*/
BOOL CObjectPolyhedron::DialogCPShow (DWORD dwSurface, PCEscWindow pWindow, PCPoint pClick)
{
   PWSTR pszRet;
   pszRet = pWindow->PageDialog (ghInstance, IDR_MMLPOLYHEDRONDISPLAY, PolyhedronDisplayPage, this);
   CalcScale ();  // automatically recalc scale
   if (!pszRet)
      return FALSE;

   return !_wcsicmp(pszRet, Back());
}



/*************************************************************************************
CObjectPolyhedron::ControlPointQuery - Called to query the information about a control
point identified by dwID.

inputs
   DWORD       dwID - ID
   POSCONTROL  pInfo - Filled in with the infomration
returns
   BOOL - TRUE if successful
*/
BOOL CObjectPolyhedron::ControlPointQuery (DWORD dwID, POSCONTROL pInfo)
{
   fp fKnobSize = max(max (m_pScaleSize.p[0], m_pScaleSize.p[1]), m_pScaleSize.p[2]) / 20.0;

   if (m_dwDisplayControl == 1) {
      // control the points
      if (dwID >= m_lPoints.Num())
         return FALSE;

      memset (pInfo,0, sizeof(*pInfo));

      pInfo->dwID = dwID;
      //pInfo->dwFreedom = 0;   // any direction
      pInfo->dwStyle = CPSTYLE_CUBE;
      pInfo->fSize = fKnobSize;
      pInfo->cColor = RGB(0xff,0,0xff);
      wcscpy (pInfo->szName, L"Shape");

      pInfo->pLocation.Copy ((PCPoint) m_lPoints.Get(dwID));

      return TRUE;
   }
   else if (m_dwDisplayControl == 0) { // scale
      if (dwID >= 3)
         return FALSE;

      memset (pInfo,0, sizeof(*pInfo));

      pInfo->dwID = dwID;
      //pInfo->dwFreedom = 0;   // any direction
      pInfo->dwStyle = CPSTYLE_CUBE;
      pInfo->fSize = fKnobSize;
      pInfo->cColor = RGB(0,0xff,0xff);
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
      }
      MeasureToString (m_pScaleSize.p[dwID], pInfo->szMeasurement);

      pInfo->pLocation.Add (&m_pScaleMin, &m_pScaleMax);
      pInfo->pLocation.Scale (.5);
      pInfo->pLocation.p[dwID] = m_pScaleMax.p[dwID];

      return TRUE;
   }

   return FALSE;
}

/*************************************************************************************
CObjectPolyhedron::ControlPointSet - Called to change the valud of a control point.

inputs
   DWORD       dwID - ID
   PCPoint     pVal - Contains the new location, in object coordinates
returns
   BOOL - TRUE if successful
*/
BOOL CObjectPolyhedron::ControlPointSet (DWORD dwID, PCPoint pVal, PCPoint pViewer)
{
   if (m_dwDisplayControl == 1) {
      if (dwID >= m_lPoints.Num())
         return FALSE;

      // get the point
      PCPoint pOld, pList;
      pList = (PCPoint) m_lPoints.Get(0);
      pOld = pList + dwID;

      // tell the world we're about to change
      if (m_pWorld)
         m_pWorld->ObjectAboutToChange (this);

      // use the new value
      pOld->Copy (pVal);

      // Deal with constraints
      if (m_dwDisplayConstraint < 2) {
         // find points which match
         PCPoint pap;
         DWORD i;
         pap = gaPHInfo[m_dwShape].papVertex;
         for (i = 0; i < m_lPoints.Num(); i++) {
            if (i == dwID)
               continue;   // already changing

            if (m_dwDisplayConstraint == 1) {
               // only sync points direct over one another
               if (fabs(pap[i].p[0] - pap[dwID].p[0]) >= CLOSE)
                  continue;
               if (fabs(pap[i].p[1] - pap[dwID].p[1]) >= CLOSE)
                  continue;
            }

            // keep X and Y synced
            if (fabs(pap[i].p[0] - pap[dwID].p[0]) < CLOSE)
               pList[i].p[0] = pOld->p[0];
            if (fabs(pap[i].p[1] - pap[dwID].p[1]) < CLOSE)
               pList[i].p[1] = pOld->p[1];
            // keep Z synced
            if (fabs(pap[i].p[2] - pap[dwID].p[2]) < CLOSE)
               pList[i].p[2] = pOld->p[2];
         }
      }

      // recalc scale
      CalcScale();

      // tell the world we've changed
      if (m_pWorld)
         m_pWorld->ObjectChanged (this);

      return TRUE;
   }
   else if (m_dwDisplayControl == 0) { // scale
      if (dwID >= 3)
         return FALSE;

      // figure out the new scale
      CPoint pNew, pMid;
      pMid.Add (&m_pScaleMax, &m_pScaleMin);
      pMid.Scale (.5);
      pNew.Copy (&m_pScaleSize);
      pNew.p[dwID] = (pVal->p[dwID] - pMid.p[dwID]) * 2.0;
      pNew.p[dwID] = max(CLOSE, pNew.p[dwID]);

      if (pNew.AreClose (&m_pScaleSize))
         return TRUE;   // no change

      // tell the world we're about to change
      if (m_pWorld)
         m_pWorld->ObjectAboutToChange (this);


      // recalc
      DWORD dwNum;
      PCPoint pOld;
      dwNum = m_lPoints.Num();
      pOld = (PCPoint) m_lPoints.Get(0);

      // fill in the new values
      DWORD i, k;
      PCPoint p;
      for (i = 0; i < dwNum; i++) {
         p = pOld + i;

         p->Subtract (&pMid);
         for (k = 0; k < 3; k++)
            p->p[k] = p->p[k] / m_pScaleSize.p[k] * pNew.p[k];
         p->Add (&pMid);
      }



      CalcScale();

      // tell the world we've changed
      if (m_pWorld)
         m_pWorld->ObjectChanged (this);

      return TRUE;
   }

   return FALSE;
}

/*************************************************************************************
CObjectPolyhedron::ControlPointEnum - Called to enumerate a list of control point IDs
into the list.

inputs
   PCListFixed       plDWORD - Should be filled in with control points
returns
   none
*/
void CObjectPolyhedron::ControlPointEnum (PCListFixed plDWORD)
{
   // 6 control points starting at 10
   DWORD i;

   if (m_dwDisplayControl == 1) {
      DWORD dwNum = m_lPoints.Num();
      for (i = 0; i < dwNum; i++)
         plDWORD->Add (&i);
   }
   else if (m_dwDisplayControl == 0) { // scale
      DWORD dwNum = 3;
      for (i = 0; i < dwNum; i++)
         plDWORD->Add (&i);
   }
}


/**********************************************************************************
CObjectPolyhedron::CalcScale - Fills m_pScaleXXX with scale information
*/
void CObjectPolyhedron::CalcScale (void)
{
   // NOTE: Not bothering remember undo and redo since this is automatically calculated

   DWORD dwNum;
   PCPoint pap;
   dwNum = m_lPoints.Num();
   pap = (PCPoint) m_lPoints.Get(0);

   DWORD i;
   for (i = 0; i < dwNum; i++, pap++) {
      if (!i) {
         m_pScaleMin.Copy (pap);
         m_pScaleMax.Copy (pap);
      }
      else {
         m_pScaleMin.Min (pap);
         m_pScaleMax.Max (pap);
      }

   }

   // subtract
   m_pScaleSize.Subtract (&m_pScaleMax, &m_pScaleMin);
   for (i = 0; i < 3; i++)
      m_pScaleSize.p[i] = max(CLOSE, m_pScaleSize.p[i]);
}

