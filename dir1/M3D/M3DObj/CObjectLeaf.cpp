/************************************************************************
CObjectLeaf.cpp - Draws a Leaf.

begun 13/3/03 by Mike Rozak
Copyright 2003 Mike Rozak. All rights reserved
*/

#include <windows.h>
#include "escarpment.h"
#include "..\M3D.h"
#include "resource.h"


typedef struct {
   PWSTR       pszName;    // name of the move point
   int         iX, iY; // -1 if for left/top, 1 for right/bottom
} SPLINEMOVEP, *PSPLINEMOVEP;

static SPLINEMOVEP   gaLeafMove[] = {
   L"Attach", 0, 0
};


/**********************************************************************************
CObjectLeaf::Constructor and destructor */
CObjectLeaf::CObjectLeaf (PVOID pParams, POSINFO pInfo)
{
   m_dwRenderShow = RENDERSHOW_PLANTS;
   m_OSINFO = *pInfo;


   m_pStem.Zero();
   m_pStem.p[1] = .03;
   m_pStem.p[2] = .03;
   m_fStemDiam = 0.003;
   m_fStemConnect = 0;
   m_tpSize.h = .1;
   m_tpSize.v = .15;
   m_dwDetailH = 2;
   m_dwDetailV = 3;
   m_tpPerpShape.h = PI - PI/8;
   m_tpPerpShape.v = .5;
   m_tpLenShape.h = 0;
   m_tpLenShape.v = .5;
   m_tpRipples.h = 0;
   m_tpRipples.v = .5;
   m_fCutoutTop = m_fCutoutBottom = FALSE;

   m_fDirty = TRUE;

   // color for the Leaf
   ObjectSurfaceAdd (42, RGB(0x00,0xc0,0x00), MATERIAL_PAINTGLOSS);

   // color of stem
   ObjectSurfaceAdd (43, RGB(0x40,0x80,0x40), MATERIAL_PAINTGLOSS);
}


CObjectLeaf::~CObjectLeaf (void)
{
   // do nothing for now
}


/**********************************************************************************
CObjectLeaf::Delete - Called to delete this object
*/
void CObjectLeaf::Delete (void)
{
   delete this;
}

/**********************************************************************************
CObjectLeaf::Render - Draws a test box.

inputs
   POBJECTRENDER     pr - Render information
*/
void CObjectLeaf::Render (POBJECTRENDER pr, DWORD dwSubObject)
{
   CalcLeaf ();

   // create the surface render object and draw
   // CRenderSurface rs;
   m_Renderrs.ClearAll();
   CMatrix mObject;
   m_Renderrs.Init (pr->pRS);
   ObjectMatrixGet (&mObject);
   m_Renderrs.Multiply (&mObject);

   // figure out remap due to scale
   fp fLen;
   DWORD dwWidth, dwHeight, x, y;
   dwWidth = m_dwWidth;
   dwHeight = m_dwHeight;
   // BUGFIX - Was fLen = max(m_tpSize.h, m_tpSiz.v) - which wasn't
   // too accurate as far as determining sizeof leave divisions based on scale
   fLen = max(m_tpSize.h / (fp)m_dwWidth, m_tpSize.v / (fp)m_dwHeight);
   // CMem memMap;
   if (fLen < m_Renderrs.m_fDetail * 0.25) {
      if (dwWidth < dwHeight) {
         dwHeight = dwHeight / (dwWidth+1) + 2;
         dwWidth = 2;
      }
      else {
         dwWidth = dwWidth / (dwHeight+1) + 2;
         dwHeight = 2;
      }
      dwWidth = max(dwWidth, 2);
      dwHeight = max(dwHeight, 2);
      dwWidth = min(dwWidth, m_dwWidth);
      dwHeight = min(dwHeight, m_dwHeight);
   }
   else if (fLen < m_Renderrs.m_fDetail) {
      dwWidth = max(dwWidth/2, 2);
      dwHeight = max(dwHeight/2,2);
   }

   if (!m_memRenderMap.Required (dwWidth * dwHeight * sizeof(DWORD))) {
      m_Renderrs.Commit();
      return;
   }
   DWORD *padwMap;
   padwMap = (DWORD*) m_memRenderMap.p;
   fp fx,fy;
   for (y = 0; y < dwHeight; y++) for (x = 0; x < dwWidth; x++) {
      fx = (fp) x / (fp) (dwWidth-1) * (fp)(m_dwWidth-1) + .5;
      fx = min(fx, (fp)m_dwWidth-1);
      fy = (fp) y / (fp) (dwHeight-1) * (fp)(m_dwHeight-1) + .5;
      fy = min(fy, (fp)m_dwHeight-1);

      padwMap[x + y * dwWidth] = (DWORD)fx + (DWORD)fy * m_dwWidth;
   }

   // object specific
   m_Renderrs.SetDefMaterial (m_OSINFO.dwRenderShard, ObjectSurfaceFind (42), m_pWorld);
   PCPoint paPoints, paNorm;
   PTEXTPOINT5 paText;
   PVERTEX paVert;
   DWORD dwPoint, dwNorm, dwText, dwVert;
   paPoints = m_Renderrs.NewPoints (&dwPoint, dwWidth * dwHeight);
   if (paPoints) {
      PCPoint p = (PCPoint) m_memPoints.p;
      for (y = 0; y < dwHeight; y++) for (x = 0; x < dwWidth; x++)
         paPoints[x + y * dwWidth].Copy (&p[padwMap[x + y * dwWidth]]);
   }

   paNorm = m_Renderrs.NewNormals (TRUE, &dwNorm, dwWidth * dwHeight);
   if (paNorm) {
      PCPoint p = (PCPoint) m_memNormals.p;
      for (y = 0; y < dwHeight; y++) for (x = 0; x < dwWidth; x++)
         paNorm[x + y * dwWidth].Copy (&p[padwMap[x + y * dwWidth]]);
   }

   // NOTE - Wont work well with 3d texture because leaf stretches out texture
   paText = m_Renderrs.NewTextures (&dwText, dwWidth * dwHeight);
   if (paText) {
      for (y = 0; y < dwHeight; y++) for (x = 0; x < dwWidth; x++) {
         paText[x + y * dwWidth].hv[0] = (fp)x / (fp) (dwWidth-1);
         paText[x + y * dwWidth].hv[1] = 1.0 - (fp)y / (fp) (dwHeight-1);
         paText[x + y * dwWidth].xyz[0] = paPoints[x + y * dwWidth].p[0];
         paText[x + y * dwWidth].xyz[1] = paPoints[x + y * dwWidth].p[1];
         paText[x + y * dwWidth].xyz[2] = paPoints[x + y * dwWidth].p[2];
      }
   }

   // vertices
   paVert = NULL;
   if (paPoints)
      paVert = m_Renderrs.NewVertices (&dwVert, dwWidth * dwHeight);
   if (paVert) {
      DWORD i;
      i = 0;
      for (y = 0; y < dwHeight; y++) for (x = 0; x < dwWidth; x++, i++) {
         paVert[i].dwNormal = paNorm ? (i + dwNorm) : 0;
         paVert[i].dwPoint = i + dwNorm;
         paVert[i].dwTexture = paText ? (i + dwText) : 0;
      }
   }

   // draw quads
   DWORD dwCutWidth = dwWidth / 4;
   DWORD dwCutHeight = dwHeight / 4;
   DWORD dwLeafCutRight = dwWidth - 1 - dwCutWidth;
   DWORD dwLeafCutBottom = dwHeight - 1 - dwCutHeight;
   for (y = 0; y < dwHeight-1; y++) for (x = 0; x < dwWidth-1; x++) {
      // BUGFIX - Lead cutouts from edges
      if (m_fCutoutBottom && (y < dwCutHeight) && ( (x < dwCutWidth) || (x >= dwLeafCutRight) ) )
         continue;

      if (m_fCutoutTop && (y >= dwLeafCutBottom) && ( (x < dwCutWidth) || (x >= dwLeafCutRight) ) )
         continue;

      m_Renderrs.NewQuad (
         x + y * dwWidth,
         x+1 + y * dwWidth,
         x+1 + (y+1) * dwWidth,
         x + (y+1) * dwWidth,
         FALSE);  // can't backface cull leaves
   }



   // draw the stem
   // BUGFIX - Drawing stemp uses m_Renderrs.m_fDetail * 2.0 (instead of *1) since stem not as important
   if (m_fStemLen && (m_Renderrs.m_fDetail * 3.0 < m_fStemLen)) {
      m_Renderrs.SetDefMaterial (m_OSINFO.dwRenderShard, ObjectSurfaceFind (43), m_pWorld);
      m_nStem.Render (pr, &m_Renderrs);
   }

   m_Renderrs.Commit();
}





/**********************************************************************************
CObjectLeaf::QueryBoundingBox - Standard API
*/
void CObjectLeaf::QueryBoundingBox (PCPoint pCorner1, PCPoint pCorner2, DWORD dwSubObject)
{
   //CPoint p1, p2;
   BOOL fSet = FALSE;

   CalcLeaf ();

   // draw the stem
   if (m_fStemLen) {
      m_nStem.QueryBoundingBox (pCorner1, pCorner2);
      fSet = TRUE;
   }

   DWORD i;
   PCPoint pp = (PCPoint) m_memPoints.p;
   for (i = 0; i < m_dwWidth * m_dwHeight; i++, pp++) {
      if (fSet) {
         pCorner1->Min (pp);
         pCorner2->Max (pp);
      }
      else {
         pCorner1->Copy (pp);
         pCorner2->Copy (pp);
         fSet = TRUE;
      }
   } // i


#ifdef _DEBUG
   // test, make sure bounding box not too small
   CPoint p1,p2;
   //DWORD i;
   CObjectTemplate::QueryBoundingBox (&p1, &p2, dwSubObject);
   for (i = 0; i < 3; i++)
      if ((p1.p[i] < pCorner1->p[i] - CLOSE) || (p2.p[i] > pCorner2->p[i] + CLOSE))
         break;
   if (i < 3)
      OutputDebugString ("\r\nCObjectLeaf::QueryBoundingBox too small.");
#endif
}



/**********************************************************************************
CObjectLeaf::Clone - Clones this

inputs
   none
returns
   PCObjectSocket - Clone
*/
PCObjectSocket CObjectLeaf::Clone (void)
{
   PCObjectLeaf pNew;

   pNew = new CObjectLeaf(NULL, &m_OSINFO);

   // clone template info
   CloneTemplate(pNew);


   pNew->m_pStem.Copy (&m_pStem);
   pNew->m_fStemDiam = m_fStemDiam;
   pNew->m_fStemConnect = m_fStemConnect;
   pNew->m_tpSize = m_tpSize;
   pNew->m_dwDetailH = m_dwDetailH;
   pNew->m_dwDetailV = m_dwDetailV;
   pNew->m_tpPerpShape = m_tpPerpShape;
   pNew->m_tpLenShape = m_tpLenShape;
   pNew->m_tpRipples = m_tpRipples;
   pNew->m_fCutoutTop = m_fCutoutTop;
   pNew->m_fCutoutBottom = m_fCutoutBottom;

   pNew->m_fDirty = TRUE;  // wont need to copy over other calculated points

   return pNew;
}

static PWSTR gpszStem = L"Stem";
static PWSTR gpszStemDiam = L"StemDiam";
static PWSTR gpszStemConnect = L"StemConnect";
static PWSTR gpszSize = L"Size";
static PWSTR gpszDetailH = L"DetailH";
static PWSTR gpszDetailV = L"DetailV";
static PWSTR gpszPerpShape = L"PerpShape";
static PWSTR gpszLenShape = L"LenShape";
static PWSTR gpszRipples = L"Ripples";
static PWSTR gpszCutoutTop = L"CutoutTop";
static PWSTR gpszCutoutBottom = L"CutoutBottom";

PCMMLNode2 CObjectLeaf::MMLTo (void)
{
   // call into the template first
   PCMMLNode2 pNode = MMLToTemplate();
   if (!pNode)
      return NULL;

   MMLValueSet (pNode, gpszStem, &m_pStem);
   MMLValueSet (pNode, gpszStemDiam, m_fStemDiam);
   MMLValueSet (pNode, gpszStemConnect, m_fStemConnect);
   MMLValueSet (pNode, gpszSize, &m_tpSize);
   MMLValueSet (pNode, gpszDetailH, (int)m_dwDetailH);
   MMLValueSet (pNode, gpszDetailV, (int) m_dwDetailV);
   MMLValueSet (pNode, gpszPerpShape, &m_tpPerpShape);
   MMLValueSet (pNode, gpszLenShape, &m_tpLenShape);
   MMLValueSet (pNode, gpszRipples, &m_tpRipples);
   MMLValueSet (pNode, gpszCutoutTop, (int) m_fCutoutTop);
   MMLValueSet (pNode, gpszCutoutBottom, (int) m_fCutoutBottom);

   return pNode;
}

BOOL CObjectLeaf::MMLFrom (PCMMLNode2 pNode)
{
   // call into the template first
   if (!MMLFromTemplate (pNode))
      return FALSE;

   MMLValueGetPoint (pNode, gpszStem, &m_pStem);
   m_fStemDiam = MMLValueGetDouble (pNode, gpszStemDiam, .001);
   m_fStemConnect = MMLValueGetDouble (pNode, gpszStemConnect, 0);
   MMLValueGetTEXTUREPOINT (pNode, gpszSize, &m_tpSize);
   m_dwDetailH = (DWORD) MMLValueGetInt (pNode, gpszDetailH, 0);
   m_dwDetailV = (DWORD) MMLValueGetInt (pNode, gpszDetailV, 0);
   MMLValueGetTEXTUREPOINT (pNode, gpszPerpShape, &m_tpPerpShape);
   MMLValueGetTEXTUREPOINT (pNode, gpszLenShape, &m_tpLenShape);
   MMLValueGetTEXTUREPOINT (pNode, gpszRipples, &m_tpRipples);
   m_fCutoutTop = (BOOL) MMLValueGetInt (pNode, gpszCutoutTop, FALSE);
   m_fCutoutBottom = (BOOL) MMLValueGetInt (pNode, gpszCutoutBottom, FALSE);

   m_fDirty = TRUE;
   // dont bother with other calculated points because will be recaled with dirty

   return TRUE;
}


/**********************************************************************************
CObjectLeaf::CalcLeaf - Calculates the leaf info if it's dirty
*/
void CObjectLeaf::CalcLeaf (void)
{
   if (!m_fDirty)
      return;
   m_fDirty = FALSE;

   // how large?
   m_dwWidth = 3 + m_dwDetailH * 2;
   m_dwHeight = 3 + m_dwDetailV * 2;
   m_fStemLen = m_pStem.Length();

   // stem
   if (m_fStemLen) {
      CPoint pFront, pStart, pScale;
      pFront.Zero();
      pFront.p[0] = 1;
      pStart.Zero();
      pScale.Zero();
      pScale.p[0] = pScale.p[1] = m_fStemDiam;

      m_nStem.BackfaceCullSet (TRUE);
      m_nStem.DrawEndsSet (FALSE);
      m_nStem.FrontVector (&pFront);
      m_nStem.PathLinear (&pStart, &m_pStem);
      m_nStem.ScaleVector (&pScale);
      m_nStem.ShapeDefault (NS_CIRCLEFAST);
   }

   // leaf shape
   if (!m_memPoints.Required (m_dwWidth * m_dwHeight * sizeof(CPoint))) {
      m_fDirty = TRUE;
      return;
   }
   if (!m_memNormals.Required (m_dwWidth * m_dwHeight * sizeof(CPoint))) {
      m_fDirty = TRUE;
      return;
   }
   PCPoint pP, pN;
   pP = (PCPoint) m_memPoints.p;
   pN = (PCPoint) m_memNormals.p;

   // calculate the basic shape along the length and LR
   CMem memShape;
   DWORD dwHalf;
   dwHalf = (m_dwWidth - 1) / 2 + 1;
   if (!memShape.Required ((dwHalf + 3 * m_dwHeight) * sizeof(CPoint))) {
      m_fDirty = TRUE;
      return;
   }
   PCPoint pW, pL, pPerp, pUp;
   pW = (PCPoint) memShape.p;
   pL = pW + dwHalf;
   pPerp = pL + m_dwHeight;
   pUp = pPerp + m_dwHeight;

   // across
   fp fAngle, fLen;
   DWORD i;
   fAngle = PI / 2 - m_tpPerpShape.h / 2;
   fLen = m_tpSize.h / (fp) (m_dwWidth-1);
   pW[0].Zero();  // start at 0
   pW[1].Zero();
   pW[1].p[0] = cos(fAngle) * fLen;
   pW[1].p[2] = sin(fAngle) * fLen;
   for (i = 2; i < dwHalf; i++) {
      pW[i].Subtract (&pW[i-1], &pW[i-2]);   // slope
      pW[i].p[2] -= fLen * m_tpPerpShape.v * m_tpPerpShape.v;  // gravity
      pW[i].Normalize();
      pW[i].Scale (fLen);
      pW[i].Add (&pW[i-1]);   // add on previous
   }

   // length-wise
   DWORD dwNear;
   CPoint pDir;
   pDir.Copy (&m_pStem);
   pDir.p[2] = 0;
   fLen = m_tpSize.v / (fp) (m_dwHeight-1);
   if (!m_fStemLen) {
      pDir.Zero();
      pDir.p[1] = 1;
   }
   fAngle = atan2 (m_pStem.p[2], pDir.Length());
   pDir.Normalize();
   fAngle += m_tpLenShape.h;
   dwNear = (DWORD) (m_fStemConnect * (fp) m_dwHeight);
   dwNear = min(dwNear, m_dwHeight-1);
   pL[dwNear].Copy (&m_pStem);
   if (dwNear+1 < m_dwHeight) {
      pL[dwNear+1].Copy (&pDir);
      pL[dwNear+1].Scale (cos (fAngle));
      pL[dwNear+1].p[2] = sin(fAngle);
      pL[dwNear+1].Scale (fLen);
      pL[dwNear+1].Add (&pL[dwNear]);

      for (i = dwNear+2; i < m_dwHeight; i++) {
         pL[i].Subtract (&pL[i-1], &pL[i-2]);
         pL[i].p[2] -= fLen * m_tpLenShape.v * m_tpLenShape.v;  // gravity
         pL[i].Normalize();
         pL[i].Scale (fLen);
         pL[i].Add (&pL[i-1]);   // add on previous
      }
   }
   if (dwNear) {
      pL[dwNear-1].Copy (&pDir);
      pL[dwNear-1].Scale (cos (fAngle));
      pL[dwNear-1].p[2] = sin(fAngle);
      pL[dwNear-1].Scale (-fLen);
      pL[dwNear-1].Add (&pL[dwNear]);

      if (dwNear >= 2) for (i = dwNear-2; i < dwNear; i--) {
         pL[i].Subtract (&pL[i+1], &pL[i+2]);
         pL[i].p[2] -= fLen * m_tpLenShape.v * m_tpLenShape.v;  // gravity
         pL[i].Normalize();
         pL[i].Scale (fLen);
         pL[i].Add (&pL[i+1]);   // add on previous
      }
   }

   // perpendicular direction
   CPoint pA, pB, pC;
   for (i = 0; i < m_dwHeight; i++) {
      pB.Subtract (&pL[min(i+1, m_dwHeight-1)], &pL[i ? (i-1) : 0]);
      pA.Zero();
      pA.p[0] = 1;
      pC.CrossProd (&pA, &pB);
      pC.Normalize();
      if (pC.Length() < CLOSE) {
         pC.Zero();
         pC.p[2] = 1;
      }
      pA.CrossProd (&pB, &pC);
      pA.Normalize();
      pPerp[i].Copy (&pA);

      pC.CrossProd (&pA, &pB);
      pC.Normalize();
      pUp[i].Copy (&pC);
   }

   // fill the points...
   DWORD x, y;
   fp f;
   PCPoint pCur;
   for (y = 0; y < m_dwHeight; y++) {
      for (x = 0; x < m_dwWidth; x++) {
         i = (x < dwHalf) ? (dwHalf - x - 1) : (x - (m_dwWidth - dwHalf));
         pCur = pP + (x + y * m_dwWidth);

         // incoporate leaf bend
         pA.Copy (&pPerp[y]);
         pA.Scale (pW[i].p[0] * ((x < dwHalf) ? -1 : 1));
         pC.Copy (&pUp[y]);
         pC.Scale (pW[i].p[2]);

         // incorporate ripple
         pB.Copy (&pUp[y]);
         f = sin((fp)y / (fp)m_dwHeight / max(m_tpRipples.v,CLOSE) * 2.0 * PI);
         f *= (fp) i / (fp)(dwHalf-1);
         f *= m_tpRipples.h * m_tpSize.h / 2; // can ripple up to 1/2 length
         pB.Scale (f);

         // combine these together
         pCur->Copy (&pL[y]);
         pCur->Add (&pA);
         pCur->Add (&pB);
         pCur->Add (&pC);
         pCur->p[3] = 1;
      } // x
   } // y

   // figure out normals
   for (y = 0; y < m_dwHeight; y++) {
      for (x = 0; x < m_dwWidth; x++) {
         pCur = pP + (x + y * m_dwWidth);

         pA.Subtract ((x+1 < m_dwWidth) ? &pCur[1] : pCur, x ? &pCur[-1] : pCur);
         pB.Subtract ((y+1 < m_dwWidth) ? &pCur[m_dwWidth] : pCur, y ? &pCur[-(int)m_dwWidth] : pCur);
         pC.CrossProd (&pB, &pA);
            // BUGFIX - Was pA x pB - but left reversed normals
         pC.Normalize();

         pC.p[3] = 1;

         pN[x + y * m_dwWidth].Copy (&pC);
      }  // x
   } // y
}


/**************************************************************************************
CObjectLeaf::MoveReferencePointQuery - 
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
BOOL CObjectLeaf::MoveReferencePointQuery (DWORD dwIndex, PCPoint pp)
{
   PSPLINEMOVEP ps;
   DWORD dwDataSize;
   ps = gaLeafMove;
   dwDataSize = sizeof(gaLeafMove);

   if (dwIndex >= dwDataSize / sizeof(SPLINEMOVEP))
      return FALSE;

   // always at 0,0 in Leafs
   pp->Zero();
   return TRUE;
}

/**************************************************************************************
CObjectLeaf::MoveReferenceStringQuery -
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
BOOL CObjectLeaf::MoveReferenceStringQuery (DWORD dwIndex, PWSTR psz, DWORD dwSize, DWORD *pdwNeeded)
{
   PSPLINEMOVEP ps;
   DWORD dwDataSize;
   ps = gaLeafMove;
   dwDataSize = sizeof(gaLeafMove);
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


/*************************************************************************************
CObjectLeaf::ControlPointQuery - Called to query the information about a control
point identified by dwID.

inputs
   DWORD       dwID - ID
   POSCONTROL  pInfo - Filled in with the infomration
returns
   BOOL - TRUE if successful
*/
BOOL CObjectLeaf::ControlPointQuery (DWORD dwID, POSCONTROL pInfo)
{
   if (dwID != 0)
      return FALSE;  // only have one control point
   
   // CP for stem location
   memset (pInfo,0, sizeof(*pInfo));
   pInfo->dwID = dwID;
   pInfo->fSize = m_fStemDiam * 2;
   pInfo->dwStyle = CPSTYLE_SPHERE;
   pInfo->cColor = RGB(0xff,0,0xff);
   wcscpy (pInfo->szName, L"Stem connects to leaf");
   pInfo->pLocation.Copy (&m_pStem);
   return TRUE;
}

/*************************************************************************************
CObjectLeaf::ControlPointSet - Called to change the valud of a control point.

inputs
   DWORD       dwID - ID
   PCPoint     pVal - Contains the new location, in object coordinates
returns
   BOOL - TRUE if successful
*/
BOOL CObjectLeaf::ControlPointSet (DWORD dwID, PCPoint pVal, PCPoint pViewer)
{
   if (dwID != 0)
      return FALSE;  // only one CP

   // hit CP for stem
   // tell the world we're about to change
   if (m_pWorld)
      m_pWorld->ObjectAboutToChange (this);

   m_pStem.Copy (pVal);
   m_pStem.p[0] = 0; // so can't move left/right
   m_fDirty = TRUE;

   // tell the world we've changed
   if (m_pWorld)
      m_pWorld->ObjectChanged (this);
   return TRUE;
}

/*************************************************************************************
CObjectLeaf::ControlPointEnum - Called to enumerate a list of control point IDs
into the list.

inputs
   PCListFixed       plDWORD - Should be filled in with control points
returns
   none
*/
void CObjectLeaf::ControlPointEnum (PCListFixed plDWORD)
{
   DWORD i;

   for (i = 0; i < 1; i++)
      plDWORD->Add (&i);
}




/****************************************************************************
LeafPage
*/
BOOL LeafPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCObjectLeaf pv = (PCObjectLeaf)pPage->m_pUserData;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         PCEscControl pControl;

         MeasureToString (pPage, L"size0", pv->m_tpSize.h);
         MeasureToString (pPage, L"size1", pv->m_tpSize.v);
         MeasureToString (pPage, L"stemdiam", pv->m_fStemDiam);

         if (pControl = pPage->ControlFind (L"detailh"))
            pControl->AttribSetInt (Pos(), (int)pv->m_dwDetailH);
         if (pControl = pPage->ControlFind (L"detailv"))
            pControl->AttribSetInt (Pos(), (int)pv->m_dwDetailV);
         if (pControl = pPage->ControlFind (L"perpshape0"))
            pControl->AttribSetInt (Pos(), (int)(pv->m_tpPerpShape.h * 100.0));
         if (pControl = pPage->ControlFind (L"perpshape1"))
            pControl->AttribSetInt (Pos(), (int)(pv->m_tpPerpShape.v * 100.0));
         if (pControl = pPage->ControlFind (L"lenshape0"))
            pControl->AttribSetInt (Pos(), (int)(pv->m_tpLenShape.h * 100.0));
         if (pControl = pPage->ControlFind (L"lenshape1"))
            pControl->AttribSetInt (Pos(), (int)(pv->m_tpLenShape.v * 100.0));
         if (pControl = pPage->ControlFind (L"ripples0"))
            pControl->AttribSetInt (Pos(), (int)(pv->m_tpRipples.h * 100.0));
         if (pControl = pPage->ControlFind (L"ripples1"))
            pControl->AttribSetInt (Pos(), (int)(pv->m_tpRipples.v * 100.0));
         if (pControl = pPage->ControlFind (L"stemconnect"))
            pControl->AttribSetInt (Pos(), (int)(pv->m_fStemConnect * 100.0));

         if (pControl = pPage->ControlFind (L"cutouttop"))
            pControl->AttribSetBOOL (Checked(), pv->m_fCutoutTop);
         if (pControl = pPage->ControlFind (L"cutoutbottom"))
            pControl->AttribSetBOOL (Checked(), pv->m_fCutoutBottom);
      }
      break;


   case ESCN_SCROLL:
   case ESCN_SCROLLING:
      {
         PESCNSCROLL p = (PESCNSCROLL) pParam;

         // only do one scroll bar
         if (!p->pControl->m_pszName)
            break;

         float *pf = NULL;
         fp *pff = NULL;
         DWORD *pdw = NULL;
         if (!_wcsicmp(p->pControl->m_pszName, L"detailh"))
            pdw = &pv->m_dwDetailH;
         else if (!_wcsicmp(p->pControl->m_pszName, L"detailv"))
            pdw = &pv->m_dwDetailV;
         else if (!_wcsicmp(p->pControl->m_pszName, L"perpshape0"))
            pf = &pv->m_tpPerpShape.h;
         else if (!_wcsicmp(p->pControl->m_pszName, L"perpshape1"))
            pf = &pv->m_tpPerpShape.v;
         else if (!_wcsicmp(p->pControl->m_pszName, L"lenshape0"))
            pf = &pv->m_tpLenShape.h;
         else if (!_wcsicmp(p->pControl->m_pszName, L"lenshape1"))
            pf = &pv->m_tpLenShape.v;
         else if (!_wcsicmp(p->pControl->m_pszName, L"ripples0"))
            pf = &pv->m_tpRipples.h;
         else if (!_wcsicmp(p->pControl->m_pszName, L"ripples1"))
            pf = &pv->m_tpRipples.v;
         else if (!_wcsicmp(p->pControl->m_pszName, L"stemconnect"))
            pff = &pv->m_fStemConnect;

         if (!pf && !pff && !pdw)
            break;   // default since not one of ours

         fp fVal;
         DWORD dwVal;
         fVal = p->pControl->AttribGetInt (Pos()) / 100.0;
         dwVal = (DWORD)p->pControl->AttribGetInt (Pos());

         if (pf && (*pf == fVal))
            return TRUE;   // no change
         if (pff && (*pff == fVal))
            return TRUE;   // no change
         if (pdw && (*pdw == dwVal))
            return TRUE;   // no change

         pv->m_pWorld->ObjectAboutToChange (pv);
         if (pf)
            *pf = fVal;
         if (pff)
            *pff = fVal;
         if (pdw)
            *pdw = dwVal;
         pv->m_fDirty = TRUE;
         pv->m_pWorld->ObjectChanged (pv);
         return TRUE;
      }
      break;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;

         if (!_wcsicmp (p->pControl->m_pszName, L"cutouttop")) {
            pv->m_pWorld->ObjectAboutToChange (pv);
            pv->m_fCutoutTop = p->pControl->AttribGetBOOL (Checked());
            pv->m_fDirty = TRUE;
            pv->m_pWorld->ObjectChanged (pv);
            return TRUE;
         }
         else if (!_wcsicmp (p->pControl->m_pszName, L"cutoutbottom")) {
            pv->m_pWorld->ObjectAboutToChange (pv);
            pv->m_fCutoutBottom = p->pControl->AttribGetBOOL (Checked());
            pv->m_fDirty = TRUE;
            pv->m_pWorld->ObjectChanged (pv);
            return TRUE;
         }

      }
      break;

   case ESCN_EDITCHANGE:
      {
         PESCNEDITCHANGE p = (PESCNEDITCHANGE) pParam;

         pv->m_pWorld->ObjectAboutToChange (pv);
         fp fTemp;
         MeasureParseString (pPage, L"size0", &fTemp);
         pv->m_tpSize.h = fTemp;
         pv->m_tpSize.h = max(pv->m_tpSize.h, CLOSE);
         MeasureParseString (pPage, L"size1", &fTemp);
         pv->m_tpSize.v = fTemp;
         pv->m_tpSize.v = max(pv->m_tpSize.v, CLOSE);
         MeasureParseString (pPage, L"stemdiam", &fTemp);
         pv->m_fStemDiam = fTemp;
         pv->m_fStemDiam = max(pv->m_fStemDiam, CLOSE);
         pv->m_fDirty = TRUE;
         pv->m_pWorld->ObjectChanged (pv);
      }
      break;


   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Leaf settings";
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}


/**********************************************************************************
CObjectLeaf::DialogShow - Causes the object to show a dialog box that allows
different attributes to be changed.

inputs
   DWORD             dwSuface - Surface that was clicked on. 0 if unknown.
   PCEscWindow       pWindow - Escarpment window to draw it in
returns
   BOOL - TRUE if user pressed "Back", FALSE if user pressed "Close"
*/
BOOL CObjectLeaf::DialogShow (DWORD dwSurface, PCEscWindow pWindow, PCPoint pClick)
{
   PWSTR pszRet;

   pszRet = pWindow->PageDialog (ghInstance, IDR_MMLLEAF, LeafPage, this);
   if (!pszRet)
      return FALSE;

   return !_wcsicmp(pszRet, Back());
}

/**********************************************************************************
CObjectLeaf::DialogQuery - Returns TRUE if the object supports a custom
dialog box, FALSE if it doesn't. The default object doesn't
*/
BOOL CObjectLeaf::DialogQuery (void)
{
   return TRUE;
}




// BUGBUG - Coloration of leaves seems to dark. See if bug transferring textures over or
// something

// BUGBUG - Cycad leaves need to be thicker

// BUGBUG - Do oak and maple trees
