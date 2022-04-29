/************************************************************************
CObjectWaterfall.cpp - Draws a box.

begun 19/3/05 by Mike Rozak
Copyright 2005 Mike Rozak. All rights reserved
*/

#include <windows.h>
#include "escarpment.h"
#include "..\M3D.h"
#include "resource.h"

static PWSTR gszCorner = L"corner";


#define PATTERNREPEAT         64    // must be even number

/**********************************************************************************
CObjectWaterfall::Constructor and destructor */
CObjectWaterfall::CObjectWaterfall (PVOID pParams, POSINFO pInfo)
{
   m_OSINFO = *pInfo;

   m_dwRenderShow = RENDERSHOW_PLANTS;
   m_fDirty = TRUE;

   m_pCorner.Zero();
   m_pCorner.p[0] = 3;
   m_pCorner.p[1] = -2;
   m_pCorner.p[2] = -10;

   m_fFlowDepthTop = 0.1;
   m_fFlowDepthBase = 1;

   m_fTimeStart = -0.1;

   m_tpFlowAir.h = 0.05;
   m_tpFlowAir.v = 0.1;
   m_tpFlowWater.h = m_tpFlowAir.h * 4;
   m_tpFlowWater.v = m_tpFlowAir.v * 4;

   m_fTime = 0;   // for animation purposes

   m_fG = 9.8; // meters per sec per sec

   m_dwFlows = 10;

   m_iSeed = 1234;   // random seed

   m_fWidth = 4;

   m_fFlowWidth = 0.5;

   m_lFlowObstruct.Init (sizeof(fp));
   fp f = 1;
   DWORD i;
   m_lFlowObstruct.Required (m_dwFlows);
   for (i = 0; i < m_dwFlows; i++)
      m_lFlowObstruct.Add (&f);

   // color for the box
   ObjectSurfaceAdd (10, RGB(0xff,0xff,0xff), MATERIAL_FLAT, L"Waterfall surface",
      &GTEXTURECODE_Waterfall, &GTEXTURESUB_Waterfall);
}


CObjectWaterfall::~CObjectWaterfall (void)
{
   // do nothing for now
}


/**********************************************************************************
CObjectWaterfall::Delete - Called to delete this object
*/
void CObjectWaterfall::Delete (void)
{
   delete this;
}

/**********************************************************************************
CObjectWaterfall::Render - Draws a test box.

inputs
   POBJECTRENDER     pr - Render information
*/
void CObjectWaterfall::Render (POBJECTRENDER pr, DWORD dwSubObject)
{
   // create the surface render object and draw
   // CRenderSurface rs;
   m_Renderrs.ClearAll();

   CMatrix mObject;
   m_Renderrs.Init (pr->pRS);
   ObjectMatrixGet (&mObject);
   m_Renderrs.Multiply (&mObject);

   // object specific
   m_Renderrs.SetDefMaterial (m_OSINFO.dwRenderShard, ObjectSurfaceFind (10), m_pWorld);

   CalcIfDirty ();

   // loop over the flows
   m_dwFlows = max(m_dwFlows, 1);
   fp fFlowWidth = 1.0 / (fp)m_dwFlows;
   fp *pafFlowObstruct = (fp*) m_lFlowObstruct.Get(0);
   m_fFlowWidth = max(m_fFlowWidth, CLOSE);
   DWORD i;
   for (i = 0; i < m_dwFlows; i++) {
      // determine the time in terms of the flow
      fp fTime = myfmod (m_pafFlowTimeOffset[i] - m_fTime, m_pafFlowTimeLoop[i]);
      PCPoint paPattern = m_paFlows + i * PATTERNREPEAT;

      // skip ahead until get to block that care about
      DWORD dwIndex;
      for (dwIndex = 0; dwIndex < PATTERNREPEAT; dwIndex++)
         if (paPattern[dwIndex].p[0] <= fTime)
            fTime -= paPattern[dwIndex].p[0];
         else
            break;   // got to where looking for

      // repeat
      fp fCurWaterfallTime = m_fTimeStart - fTime;
         // current time = start minus time already into this one
      while (TRUE) {
         if (!(dwIndex % 2)) {
            // draw the water
            fp fFlowCenter = ((fp)i + 0.5) / (fp)m_dwFlows;
            fFlowCenter += paPattern[dwIndex].p[1] * fFlowWidth;

            fp fDepth = paPattern[dwIndex].p[2] / 2.0;

            if (DrawFlow (&m_Renderrs, fFlowCenter,
                  fFlowWidth * paPattern[dwIndex].p[3] * 4.0 * m_fFlowWidth,
                  m_fFlowDepthTop * fDepth,
                  m_fFlowDepthBase * fDepth,
                  fCurWaterfallTime,
                  fCurWaterfallTime + paPattern[dwIndex].p[0],
                  m_fTimeStart,
                  m_pCorner.p[2] * pafFlowObstruct[i]) > 0)
               break;   // clipped beyond end of time
         }

         // move up time
         fCurWaterfallTime += paPattern[dwIndex].p[0];
         dwIndex = (dwIndex+1) % PATTERNREPEAT;

      } // while TRUE, draw parts of flow

   } // i

   m_Renderrs.Commit();
}

/**********************************************************************************
CObjectWaterfall::QueryBoundingBox - Standard API
*/
void CObjectWaterfall::QueryBoundingBox (PCPoint pCorner1, PCPoint pCorner2, DWORD dwSubObject)
{
   fp fExtraWidth = max(m_fWidth, m_pCorner.p[0]*2) * m_fFlowWidth * 4.0 / (fp)max(m_dwFlows,1);

   pCorner1->Copy (&m_pCorner);
   pCorner1->p[0] *= -1;   // left
   pCorner2->Copy (&m_pCorner);
   pCorner2->p[1] *= -1;   // since can be negative in y
   pCorner2->p[2] = 0;

   pCorner1->p[0] = min(pCorner1->p[0], -m_fWidth/2);
   pCorner2->p[0] = max(pCorner2->p[0], m_fWidth/2);
   pCorner1->p[0] -= fExtraWidth;
   pCorner2->p[0] += fExtraWidth;

   pCorner1->p[1] -= max(m_fFlowDepthBase, m_fFlowDepthTop);
   pCorner2->p[1] += max(m_fFlowDepthBase, m_fFlowDepthTop);
   pCorner1->p[2] -= m_fFlowDepthBase;
   pCorner2->p[2] += m_fFlowDepthTop;

#ifdef _DEBUG
   // test, make sure bounding box not too small
   CPoint p1,p2;
   DWORD i;
   CObjectTemplate::QueryBoundingBox (&p1, &p2, dwSubObject);
   for (i = 0; i < 3; i++)
      if ((p1.p[i] < pCorner1->p[i]) || (p2.p[i] > pCorner2->p[i]))
         break;
   if (i < 3)
      OutputDebugString ("\r\nCObjectWaterfall::QueryBoundingBox too small.");
#endif
}

/**********************************************************************************
CObjectWaterfall::Clone - Clones this

inputs
   none
returns
   PCObjectSocket - Clone
*/
PCObjectSocket CObjectWaterfall::Clone (void)
{
   PCObjectWaterfall pNew;

   pNew = new CObjectWaterfall(NULL, &m_OSINFO);

   // clone template info
   CloneTemplate(pNew);

   // Clone member variables
   pNew->m_pCorner.Copy (&m_pCorner);

   pNew->m_fG = m_fG;
   pNew->m_fWidth = m_fWidth;
   pNew->m_iSeed = m_iSeed;
   pNew->m_dwFlows = m_dwFlows;
   pNew->m_fFlowDepthTop = m_fFlowDepthTop;
   pNew->m_fFlowDepthBase = m_fFlowDepthBase;
   pNew->m_fTimeStart = m_fTimeStart;
   pNew->m_tpFlowAir = m_tpFlowAir;
   pNew->m_tpFlowWater = m_tpFlowWater;
   pNew->m_fTime = m_fTime;
   pNew->m_fFlowWidth = m_fFlowWidth;
   pNew->m_lFlowObstruct.Init (sizeof(fp), m_lFlowObstruct.Get(0), m_lFlowObstruct.Num());

   pNew->m_fDirty = TRUE;

   return pNew;
}



static PWSTR gpszG = L"G";
static PWSTR gpszWidth = L"Width";
static PWSTR gpszSeed = L"Seed";
static PWSTR gpszFlowDepthTop = L"FlowDepthTop";
static PWSTR gpszFlowDepthBase = L"FlowDepthBase";
static PWSTR gpszFlows = L"Flows";
static PWSTR gpszTimeStart = L"TimeStart";
static PWSTR gpszFlowAir = L"FlowAir";
static PWSTR gpszFlowWater = L"FlowWater";
static PWSTR gpszFlowWidth = L"FlowWidth";
static PWSTR gpszTime = L"Time";

PCMMLNode2 CObjectWaterfall::MMLTo (void)
{
   // call into the template first
   PCMMLNode2 pNode = MMLToTemplate();
   if (!pNode)
      return NULL;

   // member variables go here
   MMLValueSet (pNode, gszCorner, &m_pCorner);

   MMLValueSet (pNode, gpszG, m_fG);
   MMLValueSet (pNode, gpszWidth, m_fWidth);
   MMLValueSet (pNode, gpszFlowWidth, m_fFlowWidth);
   MMLValueSet (pNode, gpszTime, m_fTime);
   MMLValueSet (pNode, gpszSeed, m_iSeed);
   MMLValueSet (pNode, gpszFlows, (int)m_dwFlows);
   MMLValueSet (pNode, gpszFlowDepthTop, m_fFlowDepthTop);
   MMLValueSet (pNode, gpszFlowDepthBase, m_fFlowDepthBase);
   MMLValueSet (pNode, gpszTimeStart, m_fTimeStart);
   MMLValueSet (pNode, gpszFlowAir, &m_tpFlowAir);
   MMLValueSet (pNode, gpszFlowWater, &m_tpFlowWater);

   DWORD i;
   WCHAR szTemp[64];
   fp *paf = (fp*)m_lFlowObstruct.Get(0);
   for (i = 0; i < m_dwFlows; i++) {
      if (paf[i] == 1.0)
         continue;   // dont bother to write

      swprintf (szTemp, L"FlowObstruct%d", (int)i);
      MMLValueSet (pNode, szTemp, paf[i]);
   }

   return pNode;
}

BOOL CObjectWaterfall::MMLFrom (PCMMLNode2 pNode)
{
   // call into the template first
   if (!MMLFromTemplate (pNode))
      return FALSE;

   m_fDirty = TRUE;

   // member variables go here
   MMLValueGetPoint (pNode, gszCorner, &m_pCorner);

   m_fG = MMLValueGetDouble (pNode, gpszG, 9.8);
   m_fWidth = MMLValueGetDouble (pNode, gpszWidth, 4);
   m_fFlowWidth = MMLValueGetDouble (pNode, gpszFlowWidth, 0.5);
   m_fTime = MMLValueGetDouble (pNode, gpszTime, 0);
   m_iSeed = MMLValueGetInt (pNode, gpszSeed, 1234);
   m_dwFlows = (DWORD) MMLValueGetInt (pNode, gpszFlows, 10);
   m_fFlowDepthTop = MMLValueGetDouble (pNode, gpszFlowDepthTop, .1);
   m_fFlowDepthBase = MMLValueGetDouble (pNode, gpszFlowDepthBase, 2);
   m_fTimeStart = MMLValueGetDouble (pNode, gpszTimeStart, -0.1);
   MMLValueGetTEXTUREPOINT (pNode, gpszFlowAir, &m_tpFlowAir);
   MMLValueGetTEXTUREPOINT (pNode, gpszFlowWater, &m_tpFlowWater);

   m_lFlowObstruct.Init (sizeof(fp));
   DWORD i;
   WCHAR szTemp[64];
   fp f;
   for (i = 0; i < m_dwFlows; i++) {
      swprintf (szTemp, L"FlowObstruct%d", (int)i);
      f = MMLValueGetDouble (pNode, szTemp, 1.0);
      m_lFlowObstruct.Add (&f);
   }
   return TRUE;
}

/*************************************************************************************
CObjectWaterfall::ControlPointQuery - Called to query the information about a control
point identified by dwID.

inputs
   DWORD       dwID - ID
   POSCONTROL  pInfo - Filled in with the infomration
returns
   BOOL - TRUE if successful
*/
BOOL CObjectWaterfall::ControlPointQuery (DWORD dwID, POSCONTROL pInfo)
{
   if ( !((dwID == 1) || (dwID == 2) || ((dwID >= 100) && (dwID < 100+m_dwFlows))))
      return FALSE;

   memset (pInfo,0, sizeof(*pInfo));

   pInfo->dwID = dwID;
   pInfo->dwStyle = CPSTYLE_POINTER;
   pInfo->fSize = m_pCorner.Length() / 20;

   if (dwID == 1) {// width up top
      pInfo->cColor = RGB(0xff,0,0xff);
      pInfo->pLocation.Zero();
      pInfo->pLocation.p[0] = m_fWidth / 2;
      wcscpy (pInfo->szName, L"Width at top");
   }
   else if (dwID == 2) {  // LR corner
      pInfo->cColor = RGB(0xff,0,0xff);
      pInfo->pLocation.Copy (&m_pCorner);
      wcscpy (pInfo->szName, L"Size at bottom");
   }
   else if ((dwID >= 100) && (dwID < 100+m_dwFlows)) {
      DWORD dwFlow = dwID - 100;
      fp f = *((fp*)m_lFlowObstruct.Get(dwFlow));
      fp fX = ((fp)dwFlow+0.5) / (fp)m_dwFlows * 2.0 - 1.0;
      pInfo->cColor = RGB(0,0,0xff);
      wcscpy (pInfo->szName, L"Flow obstructed");

      pInfo->pLocation.Zero();
      pInfo->pLocation.p[0] = fX * ((1.0 - sqrt(f)) * m_fWidth/2 + sqrt(f) * m_pCorner.p[0]);
      pInfo->pLocation.p[1] = sqrt(f) * m_pCorner.p[1];
      pInfo->pLocation.p[2] = f * m_pCorner.p[2];
   }
   else
      return FALSE;

   return TRUE;
}

/*************************************************************************************
CObjectWaterfall::ControlPointSet - Called to change the valud of a control point.

inputs
   DWORD       dwID - ID
   PCPoint     pVal - Contains the new location, in object coordinates
returns
   BOOL - TRUE if successful
*/
BOOL CObjectWaterfall::ControlPointSet (DWORD dwID, PCPoint pVal, PCPoint pViewer)
{
   if ( !((dwID == 1) || (dwID == 2) || ((dwID >= 100) && (dwID < 100+m_dwFlows))))
      return FALSE;


   // tell the world we're about to change
   if (m_pWorld)
      m_pWorld->ObjectAboutToChange (this);

   if (dwID == 1) // width up top
      m_fWidth = max (pVal->p[0] * 2, CLOSE);
   else if (dwID == 2) {  // LR corner
      m_pCorner.Copy (pVal);
      m_pCorner.p[0] = max(m_pCorner.p[0], CLOSE);
      m_pCorner.p[1] = min(m_pCorner.p[1], -CLOSE);
      m_pCorner.p[2] = min(m_pCorner.p[2], -0.01);
   }
   else if ((dwID >= 100) && (dwID < 100+m_dwFlows)) {
      fp *pf = (fp*)m_lFlowObstruct.Get(dwID - 100);
      *pf = pVal->p[2] / m_pCorner.p[2];
      *pf = max(*pf, CLOSE);
      *pf = min(*pf, 1.0);
   }

   m_fDirty = TRUE;

   // tell the world we've changed
   if (m_pWorld)
      m_pWorld->ObjectChanged (this);

   return TRUE;
}

/*************************************************************************************
CObjectWaterfall::ControlPointEnum - Called to enumerate a list of control point IDs
into the list.

inputs
   PCListFixed       plDWORD - Should be filled in with control points
returns
   none
*/
void CObjectWaterfall::ControlPointEnum (PCListFixed plDWORD)
{
   // 1 control points starting at 10
   DWORD i;
   i = 1;   // width up top
   plDWORD->Add (&i);
   i = 2;   // L/R corner
   plDWORD->Add (&i);

   for (i = 100; i < 100 + m_dwFlows; i++)
      plDWORD->Add (&i);   // flow obstruction
}


/*************************************************************************************
CObjectWaterfall::DrawFlow - This draws a waterfall flow.

inputs
   PCRenderSurface prs - Render to.
   fp       fCenter - Center location, from 0 (left) to 1 (right) of the stream
   fp       fWidth - Width, where 1.0 = width of entire waterfall
   fp       fDepthTop - Offset in depth, at top, in meters
   fp       fDepthBase - Offset in depth, at base, in meters
   fp       fTimeStart - Time when the flow start (in sec). 0 = at peak, negative = behind
   fp       fTimeEnd - Time when the flow ends.
   fp       fPreHeight - Lowest "height" before the water starts falling. Since the
               crest of the waterfall is at 0, this is a negative number, like -0.1m.
   fp       fPostHeight - Point at which the flow vanishes, because it hits water or
               a rock. Negative height, like -10m for a 10m-high waterfall
returns
   int - 0 if draws, -1 if didn't draw because occurred too far before preheight. 1 if
      didn't draw because occurred after preheight.
*/
#define WATERFALLDETAIL       4        // number of vertical segments to divide up to
int CObjectWaterfall::DrawFlow (PCRenderSurface prs, fp fCenter, fp fWidth, fp fDepthTop, fp fDepthBase,
                                fp fTimeStart, fp fTimeEnd, fp fPreHeight, fp fPostHeight)
{
   // convert pre and post height to time, inverse of 1/2 gt2
   fPreHeight = -sqrt(-fPreHeight * 2.0 / m_fG);
   fPostHeight = sqrt(-fPostHeight * 2.0 / m_fG);
   if (fTimeEnd <= fPreHeight)
      return -1;  // clip before
   if (fTimeStart >= fPostHeight)
      return 1;   // clip afterwards

   // figure out clip texture point
   fp fLength = fTimeEnd - fTimeStart;
   if (fLength <= CLOSE)
      return 0;   // too small to draw
   fp fAlphaStart = (fPreHeight - fTimeStart) / fLength;
   fAlphaStart = max(fAlphaStart, 0);
   fp fAlphaEnd = (fPostHeight - fTimeStart) / fLength;
   fAlphaEnd = min(fAlphaEnd, 1.0);
   // wont get case where fAlphaEnd <= fAlphaStart

   // figure out forward speed
   fp fMaxTimeFall = sqrt(-m_pCorner.p[2] * 2.0 / m_fG);
   fMaxTimeFall = max(fMaxTimeFall, CLOSE);
   fp fSpeedForward = m_pCorner.p[1] / fMaxTimeFall;

   // figure out L/R speed out
   fCenter = fCenter * 2.0 - 1;     // so goes from -1 to 1
   fp fSpeedOutEdge = (m_pCorner.p[0] - m_fWidth/2.0) / fMaxTimeFall;
   fp fSpeedOutLeft = fSpeedOutEdge * (fCenter - fWidth);
   fp fSpeedOutRight = fSpeedOutEdge * (fCenter + fWidth);
   fp fTopLeft = (fCenter - fWidth) * m_fWidth/2.0;
   fp fTopRight = (fCenter + fWidth) * m_fWidth/2.0;

   // figure out the locations
   DWORD dwPointIndex, dwTextureIndex;
   PCPoint paPoints = prs->NewPoints (&dwPointIndex, WATERFALLDETAIL*2);
   PTEXTPOINT5 ptp = prs->NewTextures (&dwTextureIndex, WATERFALLDETAIL*2);
   DWORD i;
   fp fTime, fTextureV;
   fp afDepth[WATERFALLDETAIL];
   for (i = 0; i < WATERFALLDETAIL; i++) {
      // figure out time
      fp fAlpha = (fp)i / (fp)(WATERFALLDETAIL-1);
      fTextureV = (1.0 - fAlpha) * fAlphaStart + fAlpha * fAlphaEnd;
      fTime = fTimeStart + fTextureV * fLength;

      // figure out the depth
      afDepth[i] = fTime / fMaxTimeFall * (fDepthBase - fDepthTop) + fDepthTop;

      paPoints[i].p[0] = fTopLeft + fTime * fSpeedOutLeft;
      paPoints[i+WATERFALLDETAIL].p[0] = fTopRight + fTime * fSpeedOutRight;

      paPoints[i].p[1] = paPoints[i+WATERFALLDETAIL].p[1] = fTime * fSpeedForward;

      paPoints[i].p[2] = paPoints[i+WATERFALLDETAIL].p[2] = -0.5 * m_fG * fTime * fTime;
      paPoints[i].p[3] = paPoints[i+WATERFALLDETAIL].p[3] = 1;

      if (ptp) {
         ptp[i].hv[0] = 0;
         ptp[i+WATERFALLDETAIL].hv[0] = 1.0;

         ptp[i].hv[1] = ptp[i+WATERFALLDETAIL].hv[1] = fTextureV;

         ptp[i].xyz[0] = paPoints[i].p[0];
         ptp[i].xyz[1] = paPoints[i].p[1];
         ptp[i].xyz[2] = paPoints[i].p[2];
         ptp[i+WATERFALLDETAIL].xyz[0] = paPoints[i+WATERFALLDETAIL].p[0];
         ptp[i+WATERFALLDETAIL].xyz[1] = paPoints[i+WATERFALLDETAIL].p[1];
         ptp[i+WATERFALLDETAIL].xyz[2] = paPoints[i+WATERFALLDETAIL].p[2];
      }

   } // i

   // need to calculate normals
   CPoint apNorm[WATERFALLDETAIL*2];
   DWORD dwNormalIndex;
   PCPoint paNorm = prs->NewNormals (TRUE, &dwNormalIndex, WATERFALLDETAIL*2);
   PCPoint paNormOrig = paNorm;
   if (!paNorm)
      paNorm = apNorm;  // so write normals, since need them to move flow up/down
   CPoint p1, p2;
   DWORD j;
   p2.Zero();
   p2.p[0] = 1;   // vector to right
   for (i = 0; i < WATERFALLDETAIL; i++) {
      DWORD dwAbove = i ? (i-1) : 0;
      DWORD dwBelow = (i+1 < WATERFALLDETAIL) ? (i+1) : i;

      for (j = 0; j < 2; j++) {
         // subtract below from above
         p1.Copy (paPoints + (dwAbove + j*WATERFALLDETAIL));
         p1.Subtract (paPoints + (dwBelow + j*WATERFALLDETAIL));

         // determine normal
         paNorm[i + j*WATERFALLDETAIL].CrossProd (&p2, &p1);
         paNorm[i + j*WATERFALLDETAIL].Normalize();
         paNorm[i + j*WATERFALLDETAIL].p[3] = 1;
      } // j
   } // i


   // will need to expand out by amount so not all streams on the same level
   for (i = 0; i < WATERFALLDETAIL*2; i++) {
      p1.Copy (paNorm+i);
      p1.Scale (afDepth[i%WATERFALLDETAIL]);
      paPoints[i].Add (&p1);
   }

   // add all the vertices
   DWORD dwVertexIndex;
   PVERTEX paVert = prs->NewVertices (&dwVertexIndex, WATERFALLDETAIL*2);
   DWORD dwColor = prs->DefColor ();
   for (i = 0; i < WATERFALLDETAIL*2; i++) {
      paVert[i].dwColor = dwColor;
      paVert[i].dwNormal = paNormOrig ? (dwNormalIndex+i) : 0;
      paVert[i].dwPoint = dwPointIndex + i;
      paVert[i].dwTexture = ptp ? (dwTextureIndex+i) : 0;
   } // i

   // need to draw polygons
   for (i = 0; i < WATERFALLDETAIL-1; i++)
      prs->NewQuad (
         dwVertexIndex + i,
         dwVertexIndex + i + WATERFALLDETAIL,
         dwVertexIndex + i + WATERFALLDETAIL + 1,
         dwVertexIndex + i + 1,
         FALSE);
   
   return 0;
}


/*************************************************************************************
CObjectWaterfall::CalcIfDirty - Calculates the flow information if anything
has changed.
*/
void CObjectWaterfall::CalcIfDirty (void)
{
   if (!m_fDirty)
      return;
   m_fDirty = FALSE;

   m_dwFlows = max(m_dwFlows, 1);

   srand (m_iSeed);

   // how much memory
   DWORD dwNeed = m_dwFlows * PATTERNREPEAT * sizeof(CPoint) + m_dwFlows * sizeof(fp) * 2;
   if (!m_memInfo.Required(dwNeed))
      return;
   m_paFlows = (PCPoint) m_memInfo.p;
   m_pafFlowTimeOffset = (fp*) (m_paFlows + m_dwFlows * PATTERNREPEAT);
   m_pafFlowTimeLoop = m_pafFlowTimeOffset + m_dwFlows;

   // fill in
   DWORD i, j;
   PCPoint pCur = m_paFlows;
   fp *pfCurOffset = m_pafFlowTimeOffset, *pfCurLoop = m_pafFlowTimeLoop;
   for (i = 0; i < m_dwFlows; i++, pfCurOffset++, pfCurLoop++) {
      *pfCurLoop = 0;   // no time

      for (j = 0; j < PATTERNREPEAT; j++, pCur++) {
         if (j % 2)   // blank spot
            pCur->p[0] = MyRand(m_tpFlowAir.h, m_tpFlowAir.v);
         else { // spot with water
            pCur->p[0] = MyRand(m_tpFlowWater.h, m_tpFlowWater.v);
            pCur->p[1] = MyRand(-1, 1);   // loc l/r
            pCur->p[2] = MyRand(-1, 1);   // deptih in/out
            pCur->p[3] = MyRand(0.6, 1.4);   // width variation
         }

         pCur->p[0] = max(pCur->p[0], CLOSE);
         (*pfCurLoop) += pCur->p[0];
      } // j

      // calc random offset
      *pfCurOffset = MyRand (0, *pfCurLoop);
   } // i

}


static PWSTR gpszAttribTime = L"Time";

/*****************************************************************************************
CObjectWaterfall::AttribGetIntern - OVERRIDE THIS

Like AttribGet() except that only called if default attributes not handled.
*/
BOOL CObjectWaterfall::AttribGetIntern (PWSTR pszName, fp *pfValue)
{
   if (!_wcsicmp(pszName, gpszAttribTime)) {
      *pfValue = m_fTime;
      return TRUE;
   }
   else
      return FALSE;
}


/*****************************************************************************************
CObjectPolyMesh::AttribGetAllIntern - OVERRIDE THIS

Like AttribGetAllIntern() EXCEPT plATTRIBVAL is already initialized and filled with
some parameters (default to the object template)
*/
void CObjectWaterfall::AttribGetAllIntern (PCListFixed plATTRIBVAL)
{
   ATTRIBVAL av;
   memset (&av, 0 ,sizeof(av));
   av.fValue = m_fTime;
   wcscpy (av.szName, gpszAttribTime);
   plATTRIBVAL->Add (&av);
}


/*****************************************************************************************
CObjectWaterfall::AttribSetGroupIntern - OVERRIDE THIS

Like AttribSetGroup() except passing on non-template attributes.
*/
void CObjectWaterfall::AttribSetGroupIntern (DWORD dwNum, PATTRIBVAL paAttrib)
{
   DWORD i;
   for (i = 0; i < dwNum; i++, paAttrib++) {
      if (_wcsicmp(paAttrib->szName, gpszAttribTime))
         continue;   // not time

      // else changed
      if (m_pWorld)
         m_pWorld->ObjectAboutToChange (this);

      m_fTime = paAttrib->fValue;
      m_fDirty = TRUE;

      // tell the world we've changed
      if (m_pWorld)
         m_pWorld->ObjectChanged (this);

   }
}


/*****************************************************************************************
CObjectWaterfall::AttribInfoIntern - OVERRIDE THIS

Like AttribInfo() except called if attribute is not for template.
*/
BOOL CObjectWaterfall::AttribInfoIntern (PWSTR pszName, PATTRIBINFO pInfo)
{
   if (!_wcsicmp(pszName, gpszAttribTime)) {
      memset (pInfo, 0, sizeof(pInfo));
      pInfo->pszDescription = L"Increasing this at a rate of 1 per second causes the waterfall to animate properly.";
      pInfo->fMin = 0;
      pInfo->fMax = 1000;
      pInfo->fDefPassUp = FALSE;
      pInfo->fDefLowRank = FALSE;
      pInfo->dwType = AIT_NUMBER;
      pInfo->pLoc.Zero();

      return TRUE;
   }
   else
      return FALSE;
}



// WFIS - passed to dialog
typedef struct {
   PCObjectWaterfall          pv;         // this
   PCObjectTemplate     pTemp;      // template
} WFIS, *PWFIS;




/****************************************************************************
WaterfallPage
*/
BOOL WaterfallPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PWFIS ptis = (PWFIS)pPage->m_pUserData;
   PCObjectWaterfall pv = ptis->pv;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         PCEscControl pControl;

         DoubleToControl (pPage, L"flows", pv->m_dwFlows);
         DoubleToControl (pPage, L"timestart", -pv->m_fTimeStart);
         DoubleToControl (pPage, L"flowwater0", pv->m_tpFlowWater.h);
         DoubleToControl (pPage, L"flowwater1", pv->m_tpFlowWater.v);
         DoubleToControl (pPage, L"flowair0", pv->m_tpFlowAir.h);
         DoubleToControl (pPage, L"flowair1", pv->m_tpFlowAir.v);
         DoubleToControl (pPage, L"seed", pv->m_iSeed);
         MeasureToString (pPage, L"flowdepthtop", pv->m_fFlowDepthTop, TRUE);
         MeasureToString (pPage, L"flowdepthbase", pv->m_fFlowDepthBase, TRUE);
         MeasureToString (pPage, L"g", pv->m_fG, TRUE);

         pControl = pPage->ControlFind (L"flowwidth");
         if (pControl)
            pControl->AttribSetInt (Pos(), (int)(pv->m_fFlowWidth * 100.0));

      }
      break;


   case ESCN_SCROLL:
   case ESCN_SCROLLING:
      {
         PESCNSCROLL p = (PESCNSCROLL) pParam;

         // only do one scroll bar
         if (!p->pControl->m_pszName)
            break;

         fp *pfMod = NULL;
         if (!_wcsicmp(p->pControl->m_pszName, L"flowwidth"))
            pfMod = &pv->m_fFlowWidth;
         else
            break;   // not one of these

         fp fVal;
         fVal = (fp) p->pControl->AttribGetInt (Pos()) / 100.0;

         ptis->pTemp->m_pWorld->ObjectAboutToChange (ptis->pTemp);
         *pfMod = fVal;
         pv->m_fDirty = TRUE;
         ptis->pTemp->m_pWorld->ObjectChanged (ptis->pTemp);
         return TRUE;
      }
      break;

   case ESCN_EDITCHANGE:
      {
         PESCNEDITCHANGE p = (PESCNEDITCHANGE) pParam;

         ptis->pTemp->m_pWorld->ObjectAboutToChange (ptis->pTemp);


         pv->m_dwFlows = (DWORD) DoubleFromControl (pPage, L"flows");
         pv->m_dwFlows = max(pv->m_dwFlows, 1);
         fp f = 1.0;
         while (pv->m_lFlowObstruct.Num() < pv->m_dwFlows)
            pv->m_lFlowObstruct.Add (&f);

         pv->m_fTimeStart = -DoubleFromControl (pPage, L"timestart");
         pv->m_fTimeStart = min(pv->m_fTimeStart, 0);

         pv->m_tpFlowWater.h = DoubleFromControl (pPage, L"flowwater0");
         pv->m_tpFlowWater.h = max(pv->m_tpFlowWater.h, CLOSE);

         pv->m_tpFlowWater.v = DoubleFromControl (pPage, L"flowwater1");
         pv->m_tpFlowWater.v = max(pv->m_tpFlowWater.v, CLOSE);

         pv->m_tpFlowAir.h = DoubleFromControl (pPage, L"flowair0");
         pv->m_tpFlowAir.h = max(pv->m_tpFlowAir.h, CLOSE);

         pv->m_tpFlowAir.v = DoubleFromControl (pPage, L"flowair1");
         pv->m_tpFlowAir.v = max(pv->m_tpFlowAir.v, CLOSE);

         pv->m_iSeed = (int) DoubleFromControl (pPage, L"seed");

         MeasureParseString (pPage, L"flowdepthtop", &pv->m_fFlowDepthTop);
         pv->m_fFlowDepthTop = max(pv->m_fFlowDepthTop, 0);

         MeasureParseString (pPage, L"flowdepthbase", &pv->m_fFlowDepthBase);
         pv->m_fFlowDepthBase = max(pv->m_fFlowDepthBase, 0);

         MeasureParseString (pPage, L"g", &pv->m_fG);
         pv->m_fG = max(pv->m_fG, CLOSE);

         pv->m_fDirty = TRUE;

         ptis->pTemp->m_pWorld->ObjectChanged (ptis->pTemp);
      }
      break;


   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Waterfall settings";
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}


/**********************************************************************************
CObjectWaterfall::DialogShow - Causes the object to show a dialog box that allows
different attributes to be changed.

inputs
   DWORD             dwSuface - Surface that was clicked on. 0 if unknown.
   PCEscWindow       pWindow - Escarpment window to draw it in
returns
   BOOL - TRUE if user pressed "Back", FALSE if user pressed "Close"
*/
BOOL CObjectWaterfall::DialogShow (DWORD dwSurface, PCEscWindow pWindow, PCPoint pClick)
{
   PWSTR pszRet;
   WFIS tis;
   memset (&tis, 0, sizeof(tis));
   tis.pTemp = this;
   tis.pv = this;

   pszRet = pWindow->PageDialog (ghInstance, IDR_MMLWATERFALL, WaterfallPage, &tis);
   if (!pszRet)
      return FALSE;

   return !_wcsicmp(pszRet, Back());
}

/**********************************************************************************
CObjectWaterfall::DialogQuery - Returns TRUE if the object supports a custom
dialog box, FALSE if it doesn't. The default object doesn't
*/
BOOL CObjectWaterfall::DialogQuery (void)
{
   return TRUE;
}



// BUGBUG - at some point may want intelligent adjust that intersects end-flow control
// points with rocks below, but for now, move by hand.
