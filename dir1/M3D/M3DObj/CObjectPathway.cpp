/************************************************************************
CObjectPathway.cpp - Draws a Pathway.

begun 14/9/01 by Mike Rozak
Copyright 2001 Mike Rozak. All rights reserved
*/

#include <windows.h>
#include "escarpment.h"
#include "..\M3D.h"
#include "resource.h"

/**********************************************************************************
CObjectPathway::Constructor and destructor */
CObjectPathway::CObjectPathway (PVOID pParams, POSINFO pInfo)
{
   // BUGFIX - Allow to use as river
   DWORD dwType = (DWORD)(size_t) pParams;
   BOOL fRiver = (dwType == 1000);

   m_dwRenderShow = RENDERSHOW_LANDSCAPING;
   m_OSINFO = *pInfo;

   CPoint ap[3];
   TEXTUREPOINT tp[3];
   memset (ap,0, sizeof(ap));
   memset (tp, 0, sizeof(tp));
   tp[0].h = tp[1].h = tp[2].h = fRiver ? 2 : 1;
   ap[1].p[1] = 10;
   m_sPath.Init (FALSE, 2, ap, tp, (DWORD*)(size_t) (fRiver ? SEGCURVE_CUBIC : SEGCURVE_LINEAR), 2, 2, .1);
   m_pWidth.p[0] = 0;   // not used
   m_pWidth.p[1] = fRiver ? 0.01 : .1;
   m_pWidth.p[2] = fRiver ? 1 : 0;
   m_pWidth.p[3] = 0;
   m_fKeepSameWidth = TRUE; // BUGFIX - Default to same width, but user will eventually vary.. !fRiver;
   m_fKeepLevel = TRUE;

   m_dwNumPoints = m_dwNumNormals = m_dwNumText = 0;
   m_dwNumVertex = m_dwNumPoly = 0;
   CalcPath();

   // color for the Pathway
   if (fRiver)
      ObjectSurfaceAdd (42, RGB(0x80, 0x80, 0xff), MATERIAL_TILEGLAZED, L"River water",
         &GTEXTURECODE_PoolWater, &GTEXTURESUB_PoolWater);
   else
      ObjectSurfaceAdd (42, RGB(0xff,0xff,0xe0), MATERIAL_TILEGLAZED, L"Pathway");
}


CObjectPathway::~CObjectPathway (void)
{
   // do nothing for now
}


/**********************************************************************************
CObjectPathway::Delete - Called to delete this object
*/
void CObjectPathway::Delete (void)
{
   delete this;
}

/**********************************************************************************
CObjectPathway::Render - Draws a test box.

inputs
   POBJECTRENDER     pr - Render information
*/
void CObjectPathway::Render (POBJECTRENDER pr, DWORD dwSubObject)
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
CObjectPathway::QueryBoundingBox - Standard API
*/
void CObjectPathway::QueryBoundingBox (PCPoint pCorner1, PCPoint pCorner2, DWORD dwSubObject)
{
   DWORD i;
   PCPoint pp = (PCPoint) m_memPoints.p;
   if(m_dwNumPoints) {
      pCorner1->Copy (pp);
      pCorner2->Copy (pp);
   }
   for (i = 1, pp++; i < m_dwNumPoints; i++, pp++) {
      pCorner1->Min (pp);
      pCorner2->Max (pp);
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
      OutputDebugString ("\r\nCObjectPathway::QueryBoundingBox too small.");
#endif
}


/**********************************************************************************
CObjectPathway::Clone - Clones this

inputs
   none
returns
   PCObjectSocket - Clone
*/
PCObjectSocket CObjectPathway::Clone (void)
{
   PCObjectPathway pNew;

   pNew = new CObjectPathway(NULL, &m_OSINFO);

   // clone template info
   CloneTemplate(pNew);

   m_sPath.CloneTo (&pNew->m_sPath);
   pNew->m_pWidth.Copy (&m_pWidth);

   pNew->m_fKeepSameWidth = m_fKeepSameWidth;
   pNew->m_fKeepLevel = m_fKeepLevel;
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

static PWSTR gpszPath = L"Path";
static PWSTR gpszWidth = L"Width";
static PWSTR gpszKeepSameWidth = L"KeepSameWidth";
static PWSTR gpszKeepLevel = L"KeepLevel";

PCMMLNode2 CObjectPathway::MMLTo (void)
{
   // call into the template first
   PCMMLNode2 pNode = MMLToTemplate();
   if (!pNode)
      return NULL;


   MMLValueSet (pNode, gpszWidth, &m_pWidth);
   MMLValueSet (pNode, gpszKeepSameWidth, (int) m_fKeepSameWidth);
   MMLValueSet (pNode, gpszKeepLevel, (int) m_fKeepLevel);

   PCMMLNode2 pSub;
   pSub = m_sPath.MMLTo();
   if (pSub) {
      pSub->NameSet (gpszPath);
      pNode->ContentAdd (pSub);
   }

   return pNode;
}

BOOL CObjectPathway::MMLFrom (PCMMLNode2 pNode)
{
   // call into the template first
   if (!MMLFromTemplate (pNode))
      return FALSE;

   MMLValueGetPoint (pNode, gpszWidth, &m_pWidth);
   m_fKeepSameWidth = (BOOL) MMLValueGetInt (pNode, gpszKeepSameWidth, (int) 0);
   m_fKeepLevel = (BOOL) MMLValueGetInt (pNode, gpszKeepLevel, (int) 0);

   PCMMLNode2 pSub;
   PWSTR psz;
   pSub = NULL;
   pNode->ContentEnum (pNode->ContentFind (gpszPath), &psz, &pSub);
   if (pSub)
      m_sPath.MMLFrom (pSub);

   CalcPath();
   return TRUE;
}




/**********************************************************************************
CObjectPathway::CalcPath - Calculates the path polygons given all the settings.
*/
BOOL CObjectPathway::CalcPath (void)
{
   m_pWidth.p[1] = max(CLOSE, m_pWidth.p[1]);
   m_pWidth.p[2] = max(0, m_pWidth.p[2]);
   m_pWidth.p[3] = max(0, m_pWidth.p[3]);
   m_pWidth.p[2] = min(1, m_pWidth.p[2]);
   m_pWidth.p[3] = min(1, m_pWidth.p[3]);

   // zero out
   m_dwNumPoints = m_dwNumNormals = m_dwNumText = 0;
   m_dwNumVertex = m_dwNumPoly = 0;

#define SCCURVE      9
#define ENDCURVE     2

   // calculate
   DWORD dwNumY, dwNumX, dwEnd, dwQuery;
   BOOL fRect, fLooped;
   fLooped = m_sPath.LoopedGet();
   dwQuery = m_sPath.QueryNodes();
   dwEnd = (fLooped ? 0 : ENDCURVE);
   fRect = (m_pWidth.p[2] < CLOSE); // if rectangular then don't curve in the ends
   dwNumY = dwQuery + dwEnd*2;   // number vertical nodes, plus two on either end
   dwNumX = fRect ? 7 : SCCURVE; // number of points along perp of path

   // allocate the points
   // so, we know how many points, etc. we need
   m_dwNumPoints = dwNumY * dwNumX;
   m_dwNumNormals = dwNumY * dwNumX;
   m_dwNumText = (dwNumY + (fLooped ? 1 : 0)) * dwNumX;
   m_dwNumVertex = (dwNumY + (fLooped ? 1 : 0)) * dwNumX;
   m_dwNumPoly = (dwNumY - (fLooped ? 0 : 1)) * (dwNumX - 1);;

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


   // first of all, create a temporary spline for the center that is devoid
   // of Z values. Do this that curvature will be along 2d
   BOOL fPathLooped;
   DWORD i;
   DWORD dwPathPoints, dwPathMin, dwPathMax;
   fp fPathDetail;
   CMem memPathPoints, memPathSegCurve, memPathTextures;
   CSpline sNoZ;
   m_sPath.ToMem (&fPathLooped, &dwPathPoints, &memPathPoints, &memPathTextures, &memPathSegCurve,
      &dwPathMin, &dwPathMax, &fPathDetail);
   if (!memPathTextures.m_dwCurPosn) {
      DWORD dwNeed = (dwPathPoints+1) * sizeof(TEXTUREPOINT);
      if (!memPathTextures.Required (dwNeed))
         return FALSE;  // error
      if (memPathTextures.p)
         memset (memPathTextures.p, 0, dwNeed);
   }
   for (i = 0; i < dwPathPoints; i++)
      ((PCPoint) memPathPoints.p)[i].p[2] = 0;  // clear out Z value
   sNoZ.Init (fPathLooped, dwPathPoints, (PCPoint) memPathPoints.p, (PTEXTUREPOINT) memPathTextures.p,
      (DWORD*) memPathSegCurve.p, dwPathMin, dwPathMax, fPathDetail);

   // create a left and right path
   PCSpline pLeft, pRight;
   CPoint pUp;
   fp fAverageWidth;
   PTEXTUREPOINT ptt;
   ptt = (PTEXTUREPOINT) memPathTextures.p;
   fAverageWidth = 0;
   for (i = 0; i < dwPathPoints; i++)
      fAverageWidth += ptt[i].h;
   fAverageWidth /= (fp) dwPathPoints;
   pUp.Zero();
   pUp.p[2] = 1;

   pLeft = sNoZ.Expand (fAverageWidth/2, &pUp);
   if (!pLeft)
      return FALSE;
   pRight = sNoZ.Expand (-fAverageWidth/2, &pUp);
   if (!pRight) {
      delete pLeft;
      return FALSE;
   }

   // precalculate the sin/cos curve
   TEXTUREPOINT   atpSC[SCCURVE];
   BOOL fNeg;
   fp fPow;
   fPow = (fRect ? .01 : m_pWidth.p[2]);
   for (i = 0; i < dwNumX; i++) {
      atpSC[i].h = -cos((fp) i / (fp) (dwNumX-1) * PI);
      if (atpSC[i].h < 0) {
         fNeg = TRUE;
         atpSC[i].h *= -1;
      }
      else
         fNeg = FALSE;
      atpSC[i].v = sin((fp) i / (fp)(dwNumX-1) * PI);

      // power
      atpSC[i].h = pow((fp)atpSC[i].h, (fp)fPow);
      atpSC[i].v = pow((fp)atpSC[i].v, (fp)fPow);

      // take care of roundoff error
      if (i == (dwNumX-1)/2)
         atpSC[i].h = 0;
      else if ((i == 0) || (i == dwNumX-1)) {
         atpSC[i].h = 1;
         atpSC[i].v = 0;
      }

      // reapply sign
      if (fNeg)
         atpSC[i].h *= -1;
      atpSC[i].h = (atpSC[i].h + 1) / 2.0;
   }

   // calculate the points and texturepoints
   PCPoint p;
   DWORD x, y;
   fp fCurV;
   PTEXTPOINT5 pt;
   fCurV = 0;
   p = pPoint;
   pt = pText;
   for (y = 0; y < dwNumY; y++) {
      // get the left and right points
      PCPoint pL, pR;
      DWORD dwNode;
      dwNode = (y < dwEnd) ? 0 : (y - dwEnd);
      dwNode = min(dwQuery-1, dwNode);
      pL = pLeft->LocationGet (dwNode);
      pR = pRight->LocationGet (dwNode);

      // vector going from left to right
      CPoint pAcross, pHeight;
      pAcross.Subtract (pR, pL);

      // calculate Z - special work since must make it linear
      DWORD dwScale, dwLoc, dwMod;
      fp fZ;
      if (fLooped)
         dwScale = dwQuery / m_sPath.OrigNumPointsGet();
      else
         dwScale = (dwQuery - 1) / (m_sPath.OrigNumPointsGet() - 1);
      dwLoc = dwNode / dwScale;
      dwMod = dwNode % dwScale;
      CPoint pPrev, pNext;
      m_sPath.OrigPointGet (dwLoc, &pPrev);
      m_sPath.OrigPointGet ((dwLoc+1) % m_sPath.OrigNumPointsGet(), &pNext);
      fZ = (fp) dwMod / (fp) dwScale;
      fZ = (1.0 - fZ) * pPrev.p[2] + fZ * pNext.p[2];

      // real left (so have altitude)
      CPoint pRealLeft;
      pRealLeft.Copy (pL);
      pRealLeft.p[2] = fZ;

      // adjust based on the difference of the averag width and the current width
      CPoint pT;
      fp fPrev, fNext;
      fPrev = ptt[dwLoc].h;
      fNext = ptt[(dwLoc+1) % m_sPath.OrigNumPointsGet()].h;
      fZ = (fp) dwMod / (fp) dwScale;
      fZ = (1.0 - fZ) * fPrev + fZ * fNext;
      pT.Copy (&pAcross);
      pT.Scale ((1 - fZ / fAverageWidth) / 2.0);
      pRealLeft.Add (&pT);
      pAcross.Scale (fZ / fAverageWidth);

      // rotate - this keeps the same profile from above, but skews the path
      // by the rotation angle so it conforms to the landscape
      fPrev = ptt[dwLoc].v;
      fNext = ptt[(dwLoc+1) % m_sPath.OrigNumPointsGet()].v;
      fZ = (fp) dwMod / (fp) dwScale;
      fZ = (1.0 - fZ) * fPrev + fZ * fNext;
      if (fabs(fZ) > CLOSE) {
         fZ = max(-PI/2*.9, fZ);
         fZ = min(PI/2*.9, fZ);

         // rotate
         pT.Copy (&pUp);
         pT.Scale (tan(fZ) * pAcross.Length() / 2.0);
         pRealLeft.Add (&pT);
         pT.Scale (2.0);
         pAcross.Subtract (&pT);
      }

      // if this is the beginning or end, then need to add curavature
      DWORD dwDist;
      fp fThisHeight;
      fThisHeight = m_pWidth.p[1];
      dwDist = 0;
      if (y < dwEnd)
         dwDist = dwEnd - y;
      else if (y >= dwEnd + dwQuery)
         dwDist = y - dwEnd - dwQuery + 1;
      if (dwDist) {
         fp fRound = max(.01, m_pWidth.p[3]);
         fp fJutOut = pow ((fp)sin((fp)dwDist / (fp)ENDCURVE * PI / 2.0), (fp)fRound) * (fRound+CLOSE);
         fp fElev = (dwDist < ENDCURVE) ? pow((fp)cos((fp)dwDist / (fp)ENDCURVE * PI / 2.0), (fp)fRound) : .01;
         fp fWidth = fElev * .9 + .1;  // so not in a complete point

         // find direction that heading in
         CPoint pF, pP;
         pF.Copy (sNoZ.TangentGet (dwNode, !dwNode));
         pF.Normalize();
         pP.Copy (&pAcross);
         pP.Normalize();

         // change the height
         fThisHeight *= fElev;

         // change the real left
         pT.Copy (&pF);
         pT.Scale (fJutOut * pAcross.Length() / 2.0 * ((y < dwEnd) ? -1 : 1));
         pRealLeft.Add (&pT);
         pT.Copy (&pAcross);
         pT.Scale ((1.0 - fWidth) / 2.0);
         pRealLeft.Add (&pT);

         // adjust across
         pAcross.Scale (fWidth);
      }

      // if approaching the end then scale down slightly so clean intersections
      if ( (y <= dwEnd) || (y+1 >= dwEnd + dwQuery))
         fThisHeight *= .99;


      // adjust the curent Y
      if (y) {
         // figure out where center point will be
         pT.Copy (&pAcross);
         pT.Scale (atpSC[(dwNumX-1)/2].h);
         pT.Add (&pRealLeft);
         pT.Subtract (&pPoint[(dwNumX-1)/2 + (y-1) * dwNumX]);
         pT.p[2] = 0;
         fCurV += pT.Length();
      }

      fp fLenAcross;
      fLenAcross = pAcross.Length();

      for (x = 0; x < dwNumX; x++, p++, pt++) {
         p->Copy (&pAcross);
         p->Scale (atpSC[x].h);
         p->Add (&pRealLeft);
         pHeight.Copy (&pUp);
         pHeight.Scale (fThisHeight * atpSC[x].v);
         p->Add (&pHeight);
         
         pt->hv[0] = (atpSC[x].h - .5) * fLenAcross;
         pt->hv[1] = -fCurV;
         pt->xyz[0] = p->p[0];
         pt->xyz[1] = p->p[1];
         pt->xyz[2] = p->p[2];
      }

   }

   // last texture points
   if (fLooped) {
      // update texture point
      PCPoint pCur, pNext;
      CPoint pRealLeft;
      pCur = sNoZ.LocationGet (dwNumY-1);
      pNext = sNoZ.LocationGet (0);
      pRealLeft.Subtract (pCur, pNext);
      fCurV += pRealLeft.Length();

      for (x = 0; x < dwNumX; x++, p++, pt++) {
         *pt = pText[x];  // keep same h's and other locs
         pt->hv[1] = -fCurV;
      }
   }

   
   // calculate the normals
   p = pNormal;
   // From above: m_dwNumNormals = dwNumY * dwNumX;
   for (y = 0; y < dwNumY; y++) {
      DWORD dwYPrev, dwYNext;
      if (fLooped) {
         dwYPrev = (y + dwNumY - 1) % dwNumY;
         dwYNext = (y + 1) % dwNumY;
      }
      else {
         dwYPrev = y ? (y-1) : 0;
         dwYNext = (y+1 < dwNumY) ? (y+1) : y;
      }

      for (x = 0; x < dwNumX; x++, p++) {
         DWORD dwXLeft, dwXRight;
         dwXLeft = x ? (x-1) : 0;
         dwXRight = (x+1 < dwNumX) ? (x+1) : x;

         // forward and right vectors
         CPoint pF, pR;
         pF.Subtract (&pPoint[x + dwYNext * dwNumX], &pPoint[x + dwYPrev * dwNumX]);
         pR.Subtract (&pPoint[dwXRight + y * dwNumX], &pPoint[dwXLeft + y * dwNumX]);
         p->CrossProd (&pR, &pF);
         p->Normalize();
      }
   }
   
   // all the vertices
   PVERTEX pv;
   pv = pVert;
   // From above: m_dwNumVertex = (dwNumY + (fLooped ? 1 : 0)) * dwNumX;
   for (y = 0; y < (dwNumY + (fLooped ? 1 : 0)); y++)
      for (x = 0; x < dwNumX; x++, pv++) {
         pv->dwNormal = (y % dwNumY) * dwNumX + x;
         pv->dwPoint = (y % dwNumY) * dwNumX + x;
         pv->dwTexture = y * dwNumX + x;
      }


   // and the polygons
   DWORD *padw;
   padw = pPoly;
   // From above: m_dwNumPoly = (dwNumY - (fLooped ? 0 : 1)) * (dwNumX - 1);;

   for (y = 0; y < dwNumY - (fLooped ? 0 : 1); y++)
      for (x = 0; x < dwNumX-1; x++, padw += 4) {
         padw[0] = x + y * dwNumX;
         padw[1] = x + (y+1) * dwNumX;
         padw[2] = x+1 + (y+1) * dwNumX;
         padw[3] = x+1 + y * dwNumX;
      }


   delete pLeft;
   delete pRight;

   return TRUE;
}


#if 0 // DEAD CODE - Attempt to fix expansion bug/quirk - but wont really solve
// problem, so go back to old code
/**********************************************************************************
CObjectPathway::CalcPath - Calculates the path polygons given all the settings.
*/
BOOL CObjectPathway::CalcPath (void)
{
   m_pWidth.p[1] = max(CLOSE, m_pWidth.p[1]);
   m_pWidth.p[2] = max(0, m_pWidth.p[2]);
   m_pWidth.p[3] = max(0, m_pWidth.p[3]);
   m_pWidth.p[2] = min(1, m_pWidth.p[2]);
   m_pWidth.p[3] = min(1, m_pWidth.p[3]);

   // first of all, create a temporary spline for the center that is devoid
   // of Z values. Do this that curvature will be along 2d
   CSpline sNoZ;
   DWORD i;
   {
      BOOL fPathLooped;
      DWORD dwPathPoints, dwPathMin, dwPathMax;
      fp fPathDetail;
      CMem memPathPoints, memPathSegCurve, memPathTextures;
      m_sPath.ToMem (&fPathLooped, &dwPathPoints, &memPathPoints, &memPathTextures, &memPathSegCurve,
         &dwPathMin, &dwPathMax, &fPathDetail);
      if (!memPathTextures.m_dwCurPosn) {
         DWORD dwNeed = (dwPathPoints+1) * sizeof(TEXTUREPOINT);
         if (!memPathTextures.Required (dwNeed))
            return FALSE;  // error
         if (memPathTextures.p)
            memset (memPathTextures.p, 0, dwNeed);
      }
      for (i = 0; i < dwPathPoints; i++)
         ((PCPoint) memPathPoints.p)[i].p[2] = 0;  // clear out Z value
      sNoZ.Init (fPathLooped, dwPathPoints, (PCPoint) memPathPoints.p, (PTEXTUREPOINT) memPathTextures.p,
         (DWORD*) memPathSegCurve.p, dwPathMin, dwPathMax, fPathDetail);
   }

   // create a linear one from the main setup
   CListFixed lPoints, lText;
   CPoint pT;
   lPoints.Init (sizeof(CPoint));
   lText.Init (sizeof(TEXTUREPOINT));
   DWORD dwScale, dwLoc, dwMod;

   if (sNoZ.LoopedGet ())
      dwScale = sNoZ.QueryNodes() / sNoZ.OrigNumPointsGet();
   else
      dwScale = (sNoZ.QueryNodes() - 1) / (sNoZ.OrigNumPointsGet() - 1);
   for (i = 0; i < sNoZ.QueryNodes(); i++) {
      pT.Copy (sNoZ.LocationGet (i));

      // restore the Z value
      pT.p[2] = (m_sPath.LocationGet(i))->p[2];

      // get the texture points
      TEXTUREPOINT tPrev, tNext;
      fp fZ;
      dwLoc = i / dwScale;
      dwMod = i % dwScale;
      sNoZ.OrigTextureGet (dwLoc, &tPrev);
      sNoZ.OrigTextureGet ((dwLoc+1) % sNoZ.OrigNumPointsGet(), &tNext);
      fZ = (fp) dwMod / (fp) dwScale;
      tPrev.h = (1.0 - fZ) * tPrev.h + fZ * tNext.h;
      tPrev.v = (1.0 - fZ) * tPrev.v + fZ * tNext.v;

      // see if it's linear with the last one
      if (lPoints.Num() >= 2) {
         PCPoint p1, p2;
         CPoint pSub, pSub2;
         p1 = (PCPoint) lPoints.Get(lPoints.Num()-2);
         p2 = p1+1;
         pSub.Subtract (p2, p1);
         pSub.Normalize();
         pSub2.Subtract (&pT, p1);
         pSub2.Normalize();
         if (pSub.DotProd (&pSub2) >= (1 - CLOSE)) {
            // it's linear
            p2->Copy (&pT);

            PTEXTUREPOINT ptp;
            ptp = (PTEXTUREPOINT) lText.Get (lText.Num()-1);
            *ptp = tPrev;
            continue;
         }
      }

      // add it
      lPoints.Add (&pT);
      lText.Add (&tPrev);
   }

   // create the spline without Z
   PCPoint pLinearPoint;
   PTEXTUREPOINT pLinearText;
   DWORD dwNumLinear;
   pLinearPoint = (PCPoint) lPoints.Get(0);
   pLinearText = (PTEXTUREPOINT) lText.Get(0);
   dwNumLinear = lPoints.Num();
   for (i = 0; i < dwNumLinear; i++) {
      // hide away
      pLinearPoint[i].p[3] = pLinearPoint[i].p[2];
      pLinearPoint[i].p[2]= 0;
   }

   sNoZ.Init (sNoZ.LoopedGet(), dwNumLinear, pLinearPoint, NULL, (DWORD*) SEGCURVE_LINEAR,0,0,.1);

   for (i = 0; i < dwNumLinear; i++) {
      // restore
      pLinearPoint[i].p[2] = pLinearPoint[i].p[3];
   }

   // create a left and right path
   PCSpline pLeft, pRight;
   CPoint pUp;
   fp *pf;
   CMem memExpand;
   if (!memExpand.Required ((dwNumLinear+1) * sizeof(fp)))
      return FALSE;
   pf = (fp *) memExpand.p;
   for (i = 0; i < dwNumLinear; i++)
      pf[i] = pLinearText[(i+1)%dwNumLinear].h / 2;  // half the width
      //NOTE: Adding one because of some slighty strange behaviour in expand
   pUp.Zero();
   pUp.p[2] = 1;

   pLeft = sNoZ.Expand (pf, &pUp);
   if (!pLeft)
      return FALSE;
   for (i = 0; i < dwNumLinear; i++)
      pf[i] *= -1;
   pRight = sNoZ.Expand (pf, &pUp);
   if (!pRight) {
      delete pLeft;
      return FALSE;
   }




   // zero out
   m_dwNumPoints = m_dwNumNormals = m_dwNumText = 0;
   m_dwNumVertex = m_dwNumPoly = 0;

#define SCCURVE      9
#define ENDCURVE     2

   // calculate
   DWORD dwNumY, dwNumX, dwEnd, dwQuery;
   BOOL fRect, fLooped;
   fLooped = m_sPath.LoopedGet();
   dwQuery = dwNumLinear;
   dwEnd = (fLooped ? 0 : ENDCURVE);
   fRect = (m_pWidth.p[2] < CLOSE); // if rectangular then don't curve in the ends
   dwNumY = dwQuery + dwEnd*2;   // number vertical nodes, plus two on either end
   dwNumX = fRect ? 7 : SCCURVE; // number of points along perp of path

   // allocate the points
   // so, we know how many points, etc. we need
   m_dwNumPoints = dwNumY * dwNumX;
   m_dwNumNormals = dwNumY * dwNumX;
   m_dwNumText = (dwNumY + (fLooped ? 1 : 0)) * dwNumX;
   m_dwNumVertex = (dwNumY + (fLooped ? 1 : 0)) * dwNumX;
   m_dwNumPoly = (dwNumY - (fLooped ? 0 : 1)) * (dwNumX - 1);;

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
   PTEXTUREPOINT pText;
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


   // precalculate the sin/cos curve
   TEXTUREPOINT   atpSC[SCCURVE];
   BOOL fNeg;
   fp fPow;
   fPow = (fRect ? .01 : m_pWidth.p[2]);
   for (i = 0; i < dwNumX; i++) {
      atpSC[i].h = -cos((fp) i / (fp) (dwNumX-1) * PI);
      if (atpSC[i].h < 0) {
         fNeg = TRUE;
         atpSC[i].h *= -1;
      }
      else
         fNeg = FALSE;
      atpSC[i].v = sin((fp) i / (fp)(dwNumX-1) * PI);

      // power
      atpSC[i].h = pow(atpSC[i].h, fPow);
      atpSC[i].v = pow(atpSC[i].v, fPow);

      // take care of roundoff error
      if (i == (dwNumX-1)/2)
         atpSC[i].h = 0;
      else if ((i == 0) || (i == dwNumX-1)) {
         atpSC[i].h = 1;
         atpSC[i].v = 0;
      }

      // reapply sign
      if (fNeg)
         atpSC[i].h *= -1;
      atpSC[i].h = (atpSC[i].h + 1) / 2.0;
   }

   // calculate the points and texturepoints
   PCPoint p;
   DWORD x, y;
   fp fCurV;
   PTEXTUREPOINT pt;
   fCurV = 0;
   p = pPoint;
   pt = pText;
   for (y = 0; y < dwNumY; y++) {
      // get the left and right points
      PCPoint pL, pR;
      DWORD dwNode;
      dwNode = (y < dwEnd) ? 0 : (y - dwEnd);
      dwNode = min(dwQuery-1, dwNode);
      pL = pLeft->LocationGet (dwNode);
      pR = pRight->LocationGet (dwNode);

      // vector going from left to right
      CPoint pAcross, pHeight;
      pAcross.Subtract (pR, pL);

      // calculate Z - special work since must make it linear
      fp fZ;
      fZ = pLinearPoint[dwNode].p[2];

      // real left (so have altitude)
      CPoint pRealLeft;
      pRealLeft.Copy (pL);
      pRealLeft.p[2] = fZ;

      // rotate - this keeps the same profile from above, but skews the path
      // by the rotation angle so it conforms to the landscape
      fZ = pLinearText[dwNode].v;
      if (fabs(fZ) > CLOSE) {
         fZ = max(-PI/2*.9, fZ);
         fZ = min(PI/2*.9, fZ);

         // rotate
         pT.Copy (&pUp);
         pT.Scale (tan(fZ) * pAcross.Length() / 2.0);
         pRealLeft.Add (&pT);
         pT.Scale (2.0);
         pAcross.Subtract (&pT);
      }

      // if this is the beginning or end, then need to add curavature
      DWORD dwDist;
      fp fThisHeight;
      fThisHeight = m_pWidth.p[1];
      dwDist = 0;
      if (y < dwEnd)
         dwDist = dwEnd - y;
      else if (y >= dwEnd + dwQuery)
         dwDist = y - dwEnd - dwQuery + 1;
      if (dwDist) {
         fp fRound = max(.01, m_pWidth.p[3]);
         fp fJutOut = pow (sin((fp)dwDist / (fp)ENDCURVE * PI / 2.0), fRound) * (fRound+CLOSE);
         fp fElev = (dwDist < ENDCURVE) ? pow(cos((fp)dwDist / (fp)ENDCURVE * PI / 2.0), fRound) : .01;
         fp fWidth = fElev * .9 + .1;  // so not in a complete point

         // find direction that heading in
         CPoint pF, pP;
         pF.Copy (sNoZ.TangentGet (dwNode, !dwNode));
         pF.Normalize();
         pP.Copy (&pAcross);
         pP.Normalize();

         // change the height
         fThisHeight *= fElev;

         // change the real left
         pT.Copy (&pF);
         pT.Scale (fJutOut * pAcross.Length() / 2.0 * ((y < dwEnd) ? -1 : 1));
         pRealLeft.Add (&pT);
         pT.Copy (&pAcross);
         pT.Scale ((1.0 - fWidth) / 2.0);
         pRealLeft.Add (&pT);

         // adjust across
         pAcross.Scale (fWidth);
      }

      // if approaching the end then scale down slightly so clean intersections
      if ( (y <= dwEnd) || (y+1 >= dwEnd + dwQuery))
         fThisHeight *= .99;


      // adjust the curent Y
      if (y) {
         // figure out where center point will be
         pT.Copy (&pAcross);
         pT.Scale (atpSC[(dwNumX-1)/2].h);
         pT.Add (&pRealLeft);
         pT.Subtract (&pPoint[(dwNumX-1)/2 + (y-1) * dwNumX]);
         pT.p[2] = 0;
         fCurV += pT.Length();
      }

      fp fLenAcross;
      fLenAcross = pAcross.Length();

      for (x = 0; x < dwNumX; x++, p++, pt++) {
         p->Copy (&pAcross);
         p->Scale (atpSC[x].h);
         p->Add (&pRealLeft);
         pHeight.Copy (&pUp);
         pHeight.Scale (fThisHeight * atpSC[x].v);
         p->Add (&pHeight);
         
         pt->h = (atpSC[x].h - .5) * fLenAcross;
         pt->v = -fCurV;
      }

   }

   // last texture points
   if (fLooped) {
      // update texture point
      PCPoint pCur, pNext;
      CPoint pRealLeft;
      pCur = sNoZ.LocationGet (dwNumY-1);
      pNext = sNoZ.LocationGet (0);
      pRealLeft.Subtract (pCur, pNext);
      fCurV += pRealLeft.Length();

      for (x = 0; x < dwNumX; x++, p++, pt++) {
         pt->h = pText[x].h;  // keep same h's
         pt->v = -fCurV;
      }
   }

   
   // calculate the normals
   p = pNormal;
   // From above: m_dwNumNormals = dwNumY * dwNumX;
   for (y = 0; y < dwNumY; y++) {
      DWORD dwYPrev, dwYNext;
      if (fLooped) {
         dwYPrev = (y + dwNumY - 1) % dwNumY;
         dwYNext = (y + 1) % dwNumY;
      }
      else {
         dwYPrev = y ? (y-1) : 0;
         dwYNext = (y+1 < dwNumY) ? (y+1) : y;
      }

      for (x = 0; x < dwNumX; x++, p++) {
         DWORD dwXLeft, dwXRight;
         dwXLeft = x ? (x-1) : 0;
         dwXRight = (x+1 < dwNumX) ? (x+1) : x;

         // forward and right vectors
         CPoint pF, pR;
         pF.Subtract (&pPoint[x + dwYNext * dwNumX], &pPoint[x + dwYPrev * dwNumX]);
         pR.Subtract (&pPoint[dwXRight + y * dwNumX], &pPoint[dwXLeft + y * dwNumX]);
         p->CrossProd (&pR, &pF);
         p->Normalize();
      }
   }
   
   // all the vertices
   PVERTEX pv;
   pv = pVert;
   // From above: m_dwNumVertex = (dwNumY + (fLooped ? 1 : 0)) * dwNumX;
   for (y = 0; y < (dwNumY + (fLooped ? 1 : 0)); y++)
      for (x = 0; x < dwNumX; x++, pv++) {
         pv->dwNormal = (y % dwNumY) * dwNumX + x;
         pv->dwPoint = (y % dwNumY) * dwNumX + x;
         pv->dwTexture = y * dwNumX + x;
      }


   // and the polygons
   DWORD *padw;
   padw = pPoly;
   // From above: m_dwNumPoly = (dwNumY - (fLooped ? 0 : 1)) * (dwNumX - 1);;

   for (y = 0; y < dwNumY - (fLooped ? 0 : 1); y++)
      for (x = 0; x < dwNumX-1; x++, padw += 4) {
         padw[0] = x + y * dwNumX;
         padw[1] = x + (y+1) * dwNumX;
         padw[2] = x+1 + (y+1) * dwNumX;
         padw[3] = x+1 + y * dwNumX;
      }


   delete pLeft;
   delete pRight;

   return TRUE;
}
#endif // 0 - DEAD CODE


/*************************************************************************************
CObjectPathway::ControlPointQuery - Called to query the information about a control
point identified by dwID.

inputs
   DWORD       dwID - ID
   POSCONTROL  pInfo - Filled in with the infomration
returns
   BOOL - TRUE if successful
*/
BOOL CObjectPathway::ControlPointQuery (DWORD dwID, POSCONTROL pInfo)
{
   fp fKnobSize = max(m_pWidth.p[1], CLOSE);

   DWORD dwNode, dwType;
   dwNode = dwID % m_sPath.OrigNumPointsGet();
   dwType = dwID / m_sPath.OrigNumPointsGet();
   if (dwType >= 3)
      return FALSE;

   memset (pInfo,0, sizeof(*pInfo));

   pInfo->dwID = dwID;
   //pInfo->dwFreedom = 0;   // any direction
   pInfo->fSize = fKnobSize;

   // get the info
   CPoint pCenter;
   TEXTUREPOINT tp;
   DWORD dwScale;
   if (m_sPath.LoopedGet ())
      dwScale = m_sPath.QueryNodes() / m_sPath.OrigNumPointsGet();
   else
      dwScale = (m_sPath.QueryNodes() - 1) / (m_sPath.OrigNumPointsGet() - 1);
   m_sPath.OrigPointGet (dwNode, &pCenter);
   pCenter.p[2] += m_pWidth.p[1];   // add the thickness
   m_sPath.OrigTextureGet (dwNode, &tp);

   PCPoint pLTan, pRTan;
   CPoint pTan, pPerp, pUp;
   pLTan = (dwNode+1 < m_sPath.OrigNumPointsGet()) ? m_sPath.TangentGet (dwNode * dwScale, TRUE) : NULL;
   pRTan = dwNode ? m_sPath.TangentGet (dwNode * dwScale, FALSE) : NULL;
   if (pLTan && pRTan)
      pTan.Average (pLTan, pRTan);
   else if (pLTan)
      pTan.Copy (pLTan);
   else if (pRTan)
      pTan.Copy (pRTan);
   else {
      pTan.Zero();
      pTan.p[0] = 1;
   }
   pUp.Zero();
   pUp.p[2] = 1;
   pPerp.CrossProd (&pUp, &pTan);
   pPerp.Normalize();

   // rotate the tangent
   if (tp.v) {
      tp.v = max(-PI/2*.9, tp.v);
      tp.v = min(PI/2*.9, tp.v);
      pPerp.p[2] += tan(tp.v);
      pPerp.Normalize();
   }

   switch (dwType) {
   case 0:  // location
   default:
      pInfo->dwStyle = CPSTYLE_CUBE;
      pInfo->cColor = RGB(0,0xff,0xff);
      wcscpy (pInfo->szName, L"Curve");
      pInfo->pLocation.Copy (&pCenter);
      break;
   case 1:  // size
      pInfo->dwStyle = CPSTYLE_SPHERE;
      pInfo->cColor = RGB(0xff,0, 0xff);
      wcscpy (pInfo->szName, L"Width");
      MeasureToString (tp.h, pInfo->szMeasurement);
      pInfo->pLocation.Copy (&pPerp);
      pInfo->pLocation.Scale (tp.h / 2.0);
      pInfo->pLocation.Add (&pCenter);
      break;
   case 2:  // rotation
      pInfo->dwStyle = CPSTYLE_POINTER;
      pInfo->cColor = RGB(0xff,0xff,0);
      wcscpy (pInfo->szName, L"Angle");
      char szTemp[32];
      AngleToString (szTemp, tp.v, TRUE);
      MultiByteToWideChar (CP_ACP, 0, szTemp, -1, pInfo->szMeasurement, sizeof(pInfo->szMeasurement));
      pInfo->pLocation.Copy (&pPerp);
      pInfo->pLocation.Scale (-tp.h / 2.0);
      pInfo->pLocation.Add (&pCenter);
      //pInfo->pDirection.Copy (&pPerp);
      break;
   }

   return TRUE;
}

/*************************************************************************************
CObjectPathway::ControlPointSet - Called to change the valud of a control point.

inputs
   DWORD       dwID - ID
   PCPoint     pVal - Contains the new location, in object coordinates
returns
   BOOL - TRUE if successful
*/
BOOL CObjectPathway::ControlPointSet (DWORD dwID, PCPoint pVal, PCPoint pViewer)
{
   DWORD dwNode, dwType;
   dwNode = dwID % m_sPath.OrigNumPointsGet();
   dwType = dwID / m_sPath.OrigNumPointsGet();
   if (dwType >= 3)
      return FALSE;

   // get all the info
   DWORD dwPoints, dwMinDivide, dwMaxDivide;
   fp fDetail;
   BOOL fLooped;
   CMem memPoints, memTextures, memSegCurve;
   if (!m_sPath.ToMem (&fLooped, &dwPoints, &memPoints, &memTextures, &memSegCurve,
      &dwMinDivide, &dwMaxDivide, &fDetail))
      return FALSE;

   // where info located
   PCPoint pc;
   PTEXTUREPOINT pt;
   pc = ((PCPoint) memPoints.p) + dwNode;
   pt = ((PTEXTUREPOINT) memTextures.p) + dwNode;

   // calculate
   CPoint p;
   p.Copy (pVal);
   p.p[2] -= m_pWidth.p[1];   // take out thickness

   switch (dwType) {
   case 0:  // location
   default:
      pc->Copy (&p);
      break;
   case 1:  // size
      p.Subtract (pc);
      pt->h = p.Length()*2;
      pt->h = max(CLOSE, pt->h);

      // if keep same width then adjust all of them
      if (m_fKeepSameWidth) {
         DWORD i;
         for (i = 0; i < dwPoints; i++)
            ((PTEXTUREPOINT) memTextures.p)[i].h = pt->h;
      }
      break;
   case 2:  // rotation
      p.Subtract (pc);
      pt->v = -atan2 (p.p[2], sqrt(p.p[0] * p.p[0] + p.p[1] * p.p[1]));
      pt->v = max(-PI/2*.9, pt->v);
      pt->v = min(PI/2*.9, pt->v);
      break;
   }

   // tell the world we're about to change
   if (m_pWorld)
      m_pWorld->ObjectAboutToChange (this);

   m_sPath.Init (fLooped, dwPoints, (PCPoint) memPoints.p, (PTEXTUREPOINT) memTextures.p,
      (DWORD*) memSegCurve.p, dwMinDivide, dwMaxDivide, fDetail);

   CalcPath();

   // tell the world we've changed
   if (m_pWorld)
      m_pWorld->ObjectChanged (this);
   return TRUE;
}

/*************************************************************************************
CObjectPathway::ControlPointEnum - Called to enumerate a list of control point IDs
into the list.

inputs
   PCListFixed       plDWORD - Should be filled in with control points
returns
   none
*/
void CObjectPathway::ControlPointEnum (PCListFixed plDWORD)
{
   DWORD i;

   DWORD dwNum = m_sPath.OrigNumPointsGet();
   if (m_fKeepLevel)
      dwNum *= 2;
   else
      dwNum *= 3;
   for (i = 0; i < dwNum; i++)
      plDWORD->Add (&i);
}



/* PathwayDialogPage
*/
BOOL PathwayDialogPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCObjectPathway pv = (PCObjectPathway) pPage->m_pUserData;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         PCEscControl pControl;

         MeasureToString (pPage, L"width1", pv->m_pWidth.p[1]);

         pControl = pPage->ControlFind (L"keepsamewidth");
         if (pControl)
            pControl->AttribSetBOOL (Checked(), pv->m_fKeepSameWidth);
         pControl = pPage->ControlFind (L"keeplevel");
         if (pControl)
            pControl->AttribSetBOOL (Checked(), pv->m_fKeepLevel);

         // scrolling
         pControl = pPage->ControlFind (L"width2");
         if (pControl)
            pControl->AttribSetInt (Pos(), (int)(pv->m_pWidth.p[2] * 100));
         pControl = pPage->ControlFind (L"width3");
         if (pControl)
            pControl->AttribSetInt (Pos(), (int)(pv->m_pWidth.p[3] * 100));
      }
      break;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;
         PWSTR psz;
         psz = p->pControl->m_pszName;

         if (!_wcsicmp(psz, L"keepsamewidth") || !_wcsicmp(psz, L"keeplevel")) {
            BOOL fSameWidth = !_wcsicmp(psz, L"keepsamewidth");

            pv->m_pWorld->ObjectAboutToChange (pv);

            if (fSameWidth)
               pv->m_fKeepSameWidth = p->pControl->AttribGetBOOL (Checked());
            else
               pv->m_fKeepLevel = p->pControl->AttribGetBOOL (Checked());

            if (p->pControl->AttribGetBOOL (Checked())) {
               // reset params
               DWORD dwPoints, dwMinDivide, dwMaxDivide, i;
               fp fDetail;
               BOOL fLooped;
               CMem memPoints, memTextures, memSegCurve;
               if (!pv->m_sPath.ToMem (&fLooped, &dwPoints, &memPoints, &memTextures, &memSegCurve,
                  &dwMinDivide, &dwMaxDivide, &fDetail))
                  return FALSE;

               for (i = 0; i < dwPoints; i++) {
                  if (fSameWidth)
                     ((PTEXTUREPOINT)(memTextures.p))[i].h = ((PTEXTUREPOINT)(memTextures.p))[0].h;
                  else
                     ((PTEXTUREPOINT)(memTextures.p))[i].v = 0;
               }

               pv->m_sPath.Init (fLooped, dwPoints, (PCPoint) memPoints.p,
                  (PTEXTUREPOINT) memTextures.p, (DWORD*) memSegCurve.p,
                  dwMinDivide, dwMaxDivide, fDetail);
            }

            pv->CalcPath();
            pv->m_pWorld->ObjectChanged (pv);
            return TRUE;
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

         if (!_wcsicmp(psz, L"width2"))
            pf = &pv->m_pWidth.p[2];
         else if (!_wcsicmp(psz, L"width3"))
            pf = &pv->m_pWidth.p[3];

         if (!pf || (*pf == fVal))
            break;   // no change

         pv->m_pWorld->ObjectAboutToChange (pv);
         *pf = fVal;
         pv->CalcPath();
         pv->m_pWorld->ObjectChanged (pv);
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

         MeasureParseString (pPage, L"width1", &pv->m_pWidth.p[1]);
         pv->m_pWidth.p[1] = max(CLOSE,pv->m_pWidth.p[1]);

         pv->CalcPath ();
         pv->m_pWorld->ObjectChanged (pv);


         break;
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Pathway settings";
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}


/* PathwayCurvePage
*/
BOOL PathwayCurvePage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCObjectPathway pv = (PCObjectPathway)pPage->m_pUserData;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         PCEscControl pControl;

         // draw images
         PCSpline ps;
         ps = &pv->m_sPath;
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
            ps = &pv->m_sPath;
            if (!ps)
               return TRUE;
            DWORD dwMin, dwMax;
            fp fDetail;
            ps->DivideGet (&dwMin, &dwMax, &fDetail);
            if (dwVal == dwMax)
               return TRUE;   // nothing to change

            // get all the points
            CMem  memPoints, memSegCurve, memTextures;
            DWORD dwOrig;
            BOOL fLooped;
            if (!ps->ToMem (&fLooped, &dwOrig, &memPoints, &memTextures, &memSegCurve, &dwMin, &dwMax, &fDetail))
               return FALSE;

            pv->m_pWorld->ObjectAboutToChange (pv);
            pv->m_sPath.Init (fLooped, dwOrig,
               (PCPoint) memPoints.p, (PTEXTUREPOINT) memTextures.p, (DWORD*) memSegCurve.p,
               dwVal, dwVal, fDetail);
            pv->CalcPath();
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
         CMem  memPoints, memSegCurve, memTextures;
         DWORD dwMinDivide, dwMaxDivide, dwOrig;
         fp fDetail;
         BOOL fLooped;
         PCSpline ps;
         ps = &pv->m_sPath;
         if (!ps)
            return FALSE;
         if (!ps->ToMem (&fLooped, &dwOrig, &memPoints, &memTextures, &memSegCurve, &dwMinDivide, &dwMaxDivide, &fDetail))
            return FALSE;

         // make sure have enough memory for extra
         if (!memPoints.Required ((dwOrig+1) * sizeof(CPoint)))
            return TRUE;
         if (!memSegCurve.Required ((dwOrig+1) * sizeof(DWORD)))
            return TRUE;
         if (!memTextures.Required ((dwOrig+2) * sizeof(TEXTUREPOINT)))
            return TRUE;

         // load it in
         PCPoint paPoints;
         DWORD *padw;
         PTEXTUREPOINT pt;
         paPoints = (PCPoint) memPoints.p;
         padw = (DWORD*) memSegCurve.p;
         pt = (PTEXTUREPOINT) memTextures.p;

         if (fCol) {
            if (dwMode == 1) {
               // inserting
               memmove (paPoints + (x+1), paPoints + x, sizeof(CPoint) * (dwOrig-x));
               paPoints[x+1].Add (paPoints + ((x+2) % (dwOrig+1)));
               paPoints[x+1].Scale (.5);
               memmove (padw + (x+1), padw + x, sizeof(DWORD) * (dwOrig - x));
               memmove (pt + (x+1), pt + x, sizeof(TEXTUREPOINT) * (dwOrig - x + 1));  // add extra just in case
               dwOrig++;
            }
            else if (dwMode == 2) {
               // deleting
               memmove (paPoints + x, paPoints + (x+1), sizeof(CPoint) * (dwOrig-x-1));
               memmove (padw + x, padw + (x+1), sizeof(DWORD) * (dwOrig - x - 1));
               memmove (pt + x, pt + (x+1), sizeof(TEXTUREPOINT) * (dwOrig - x));   // add extra one just in case
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
         pv->m_sPath.Init (fLooped, dwOrig, paPoints, pt, padw, dwMinDivide, dwMaxDivide, fDetail);
         pv->CalcPath();
         pv->m_pWorld->ObjectChanged (pv);

         // redraw the shapes
         ps = &pv->m_sPath;
         if (ps) {
            NoodGenerateThreeDFromSpline (L"edgeaddremove", pPage, ps, ps, 0, FALSE);
            NoodGenerateThreeDFromSpline (L"edgecurve", pPage, ps, ps, 1, FALSE);
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

         if (!_wcsicmp(psz, L"looped")) {
            // get all the points
            CMem  memPoints, memSegCurve, memTextures;
            DWORD dwMinDivide, dwMaxDivide, dwOrig;
            fp fDetail;
            BOOL fLooped;
            PCSpline ps;
            ps = &pv->m_sPath;
            if (!ps)
               return FALSE;
            if (!ps->ToMem (&fLooped, &dwOrig, &memPoints, &memTextures, &memSegCurve, &dwMinDivide, &dwMaxDivide, &fDetail))
               return FALSE;

            if (!memTextures.Required (sizeof(TEXTUREPOINT) * (dwOrig+2)))
               return FALSE;
            if (!memSegCurve.Required (sizeof(DWORD) * (dwOrig+1)))
               return FALSE;
            ((PTEXTUREPOINT)memTextures.p)[dwOrig] = ((PTEXTUREPOINT)memTextures.p)[0];   // just in case looped
            ((DWORD*)memSegCurve.p)[dwOrig-1] = ((DWORD*)memSegCurve.p)[0];

            pv->m_pWorld->ObjectAboutToChange (pv);
            pv->m_sPath.Init (p->pControl->AttribGetBOOL(Checked()), dwOrig,
               (PCPoint) memPoints.p, (PTEXTUREPOINT) memTextures.p,
               (DWORD*) memSegCurve.p, dwMinDivide, dwMaxDivide, fDetail);
            pv->CalcPath();
            pv->m_pWorld->ObjectChanged (pv);

            // redraw the shapes
            if (ps) {
               NoodGenerateThreeDFromSpline (L"edgeaddremove", pPage, ps, ps, 0, FALSE);
               NoodGenerateThreeDFromSpline (L"edgecurve", pPage, ps, ps, 1, FALSE);
            }
            return TRUE;
         }
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Pathway curve";
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}




/**********************************************************************************
CObjectPathway::DialogShow - Causes the object to show a dialog box that allows
different attributes to be changed.

inputs
   DWORD             dwSuface - Surface that was clicked on. 0 if unknown.
   PCEscWindow       pWindow - Escarpment window to draw it in
returns
   BOOL - TRUE if user pressed "Back", FALSE if user pressed "Close"
*/
BOOL CObjectPathway::DialogShow (DWORD dwSurface, PCEscWindow pWindow, PCPoint pClick)
{
   PWSTR pszRet;
mainpage:
   pszRet = pWindow->PageDialog (ghInstance, IDR_MMLPATHWAYDIALOG, PathwayDialogPage, this);
   if (!pszRet)
      return FALSE;
   if (!_wcsicmp(pszRet, L"custom")) {
      pszRet = pWindow->PageDialog (ghInstance, IDR_MMLNOODLECURVE, PathwayCurvePage, this);
      if (!pszRet)
         return FALSE;
      if (!_wcsicmp(pszRet, Back()))
         goto mainpage;
      // else fall through
   }

   return !_wcsicmp(pszRet, Back());
}


/**********************************************************************************
CObjectPathway::DialogQuery - Returns TRUE if the object supports a custom
dialog box, FALSE if it doesn't. The default object doesn't
*/
BOOL CObjectPathway::DialogQuery (void)
{
   return TRUE;
}


/**************************************************************************************
CObjectPathway::MoveReferencePointQuery - 
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
BOOL CObjectPathway::MoveReferencePointQuery (DWORD dwIndex, PCPoint pp)
{
   if (dwIndex >= m_sPath.OrigNumPointsGet())
      return FALSE;

   m_sPath.OrigPointGet (dwIndex, pp);

   return TRUE;
}

/**************************************************************************************
CObjectPathway::MoveReferenceStringQuery -
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
BOOL CObjectPathway::MoveReferenceStringQuery (DWORD dwIndex, PWSTR psz, DWORD dwSize, DWORD *pdwNeeded)
{
   if (dwIndex >= m_sPath.OrigNumPointsGet())
      return FALSE;

   WCHAR szTemp[32];
   swprintf (szTemp, L"Control point %d", (int) dwIndex+1);

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
CObjectPathway::IntelligentAdjust
Tells the object to intelligently adjust itself based on nearby objects.
For walls, this means triming to the roof line, for floors, different
textures, etc. If fAct is FALSE the function is just a query, that returns
TRUE if the object cares about adjustment and can try, FALSE if it can't.

NOTE: Often overridden
*/
BOOL CObjectPathway::IntelligentAdjust (BOOL fAct)
{
   if (!fAct)
      return TRUE;

   if (!m_pWorld)
      return FALSE;

   // matrix that's the inteverse of the location info
   CMatrix m, mInv;
   ObjectMatrixGet(&m);
   m.Invert4(&mInv);

   // find all the points below/above us
   DWORD dwObj;
   CMatrix mBound;
   CPoint pc1, pc2, apBound[2][2][2], pMin, pMax;
   DWORD x,y,z;
   dwObj = m_pWorld->ObjectFind (&m_gGUID);
   m_pWorld->BoundingBoxGet (dwObj, &mBound, &pc1, &pc2);
   for (x = 0; x < 2; x++) for (y = 0; y < 2; y++) for (z = 0; z < 2; z++) {
      apBound[x][y][z].p[0] = x ? pc2.p[0] : pc1.p[0];
      apBound[x][y][z].p[1] = y ? pc2.p[1] : pc1.p[1];
      apBound[x][y][z].p[2] = z ? pc2.p[2] : pc1.p[2];
      apBound[x][y][z].p[3] = 1;

      apBound[x][y][z].MultiplyLeft (&mBound);

      if (x+y+z == 0) {
         pMin.Copy (&apBound[x][y][z]);
         pMax.Copy (&apBound[x][y][z]);
      }
      else {
         pMin.Min (&apBound[x][y][z]);
         pMax.Max (&apBound[x][y][z]);
      }
   }
   pMin.p[2] -=10000;  // large number
   pMax.p[2] += 10000;   // large number
   CMatrix mIdent;
   CListFixed lIndex;
   DWORD *padwIndex;
   mIdent.Identity();
   lIndex.Init (sizeof(DWORD));
   m_pWorld->IntersectBoundingBox (&mIdent, &pMin, &pMax, &lIndex);
   if (!lIndex.Num())
      return TRUE;   // all done
   padwIndex =(DWORD*) lIndex.Get(0);

   // get the points
   BOOL fPathLooped;
   DWORD dwPathPoints, dwPathMin, dwPathMax;
   fp fPathDetail;
   CMem memPathPoints, memPathSegCurve, memPathTextures;
   CSpline sNoZ;
   m_sPath.ToMem (&fPathLooped, &dwPathPoints, &memPathPoints, &memPathTextures, &memPathSegCurve,
      &dwPathMin, &dwPathMax, &fPathDetail);
   PCPoint pap;
   PTEXTUREPOINT pat;
   pap = (PCPoint) memPathPoints.p;
   pat = (PTEXTUREPOINT) memPathTextures.p;

   CPoint pUp;
   pUp.Zero();
   pUp.p[2] = 1;
   DWORD dwScale;
   if (m_sPath.LoopedGet ())
      dwScale = m_sPath.QueryNodes() / m_sPath.OrigNumPointsGet();
   else
      dwScale = (m_sPath.QueryNodes() - 1) / (m_sPath.OrigNumPointsGet() - 1);

   // loop through all the posts that we care about
   DWORD i;
   for (i = 0; i < dwPathPoints; i++) {
      // convert to world coords
      CPoint pStartW, pEndW;
      pStartW.Copy (&pap[i]);
      pStartW.p[3] = 1;
      pStartW.MultiplyLeft (&m);
      pEndW.Copy (&pStartW);
      pEndW.p[2] -= 100;


      // find out where intersects
      OSMINTERSECTLINE il;
      memset (&il, 0, sizeof(il));
      il.pStart.Copy (&pStartW);
      il.pEnd.Copy (&pEndW);
      DWORD j;
      for (j = 0; j < lIndex.Num(); j++) {
         PCObjectSocket pos = m_pWorld->ObjectGet(j);

         pos->Message (OSM_INTERSECTLINE, &il);

         if (il.fIntersect)
            break;
      }
      if (il.fIntersect) {
         // distance
         il.pIntersect.p[3] = 1;
         il.pIntersect.MultiplyLeft (&mInv);
         pap[i].Copy (&il.pIntersect);
      }


      // if always want to keep level then dont angle
      if (m_fKeepLevel) {
         pat[i].v = 0;
         continue;
      }

      // intersect to find the banking
      PCPoint pLTan, pRTan;
      CPoint pTan, pPerp;
      pLTan = (i+1 < m_sPath.OrigNumPointsGet()) ? m_sPath.TangentGet (i * dwScale, TRUE) : NULL;
      pRTan = i ? m_sPath.TangentGet (i * dwScale, FALSE) : NULL;
      if (pLTan && pRTan)
         pTan.Average (pLTan, pRTan);  // NOTE: Basically a hack, but should be close enough
      else if (pLTan)
         pTan.Copy (pLTan);
      else if (pRTan)
         pTan.Copy (pRTan);
      else {
         pTan.Zero();
         pTan.p[0] = 1;
      }
      pPerp.CrossProd (&pUp, &pTan);
      pPerp.Normalize();
      pPerp.Scale (.1);

      pStartW.Add (&pap[i], &pPerp);
      pStartW.p[3] = 1;
      pStartW.MultiplyLeft (&m);
      pEndW.Copy (&pStartW);
      pEndW.p[2] -= 100;
      memset (&il, 0, sizeof(il));
      il.pStart.Copy (&pStartW);
      il.pEnd.Copy (&pEndW);
      for (j = 0; j < lIndex.Num(); j++) {
         PCObjectSocket pos = m_pWorld->ObjectGet(j);

         pos->Message (OSM_INTERSECTLINE, &il);

         if (il.fIntersect)
            break;
      }
      if (il.fIntersect) {
         // distance
         il.pIntersect.p[3] = 1;
         il.pIntersect.MultiplyLeft (&mInv);
         il.pIntersect.Subtract (&pap[i]);

         pat[i].v = atan2(il.pIntersect.p[2],
            sqrt(il.pIntersect.p[0] * il.pIntersect.p[0] + il.pIntersect.p[1] * il.pIntersect.p[1]));
      }
      else
         pat[i].v = 0;

   } // i

   if (m_pWorld)
      m_pWorld->ObjectAboutToChange (this);

   m_sPath.Init (fPathLooped, dwPathPoints, (PCPoint) memPathPoints.p, (PTEXTUREPOINT) memPathTextures.p,
      (DWORD*) memPathSegCurve.p, dwPathMin, dwPathMax, fPathDetail);
   CalcPath();

   // tell the world we've changed
   if (m_pWorld)
      m_pWorld->ObjectChanged (this);

   return TRUE;
}

/*************************************************************************************
CObjectPathway::IntelligentPositionDrag - The template object is unable to chose an intelligent
position so it just retunrs FALSE.

inputs
   POSINTELLIPOS     pInfo - Information that might be useful to chose the position
returns
   BOOL - TRUE if the object has moved, rotated, scaled, itself to an intelligent
      location. FALSE if doesn't know and its up to the application.
*/
typedef struct {
   fp      fZ;      // height
   DWORD       dwCount; // number
} IPD, *PIPD;
BOOL CObjectPathway::IntelligentPositionDrag (CWorldSocket *pWorld, POSINTELLIPOSDRAG pInfo,  POSINTELLIPOSRESULT pResult)
{

   BOOL fFence = TRUE;

   if (m_sPath.OrigNumPointsGet() != 2)
      return FALSE;

   // walls can be dragged
   if (!pWorld && !pInfo && !pResult)
      return TRUE;

   // some parameter checks
   if (!pWorld || !pInfo || !pResult || !pInfo->paWorldCoord || !pInfo->dwNumWorldCoord)
      return FALSE;

   // get info about the current path
   BOOL fPathLooped;
   DWORD dwPathPoints, dwPathMin, dwPathMax;
   fp fPathDetail;
   CMem memPathPoints, memPathSegCurve, memPathTextures;
   CSpline sNoZ;
   m_sPath.ToMem (&fPathLooped, &dwPathPoints, &memPathPoints, &memPathTextures, &memPathSegCurve,
      &dwPathMin, &dwPathMax, &fPathDetail);
   DWORD dwCurve = *((DWORD*)memPathSegCurve.p);
   fp fAverageWidth;
   PTEXTUREPOINT ptt;
   ptt = (PTEXTUREPOINT) memPathTextures.p;
   fAverageWidth = 0;
   DWORD i, j;
   for (i = 0; i < dwPathPoints; i++)
      fAverageWidth += ptt[i].h;
   fAverageWidth /= (fp) dwPathPoints;

   // create a list of all the points that dragged over to determine height - which
   // is all care about with path
   CListFixed lHeight;
   lHeight.Init (sizeof(IPD));
   PIPD pi;
   for (i = 0; i < pInfo->dwNumWorldCoord; i++) {
      fp fZ = pInfo->paWorldCoord[i].p[2];

      // loop through the current list and see if can find same height
      pi = (PIPD)lHeight.Get(0);
      for (j = 0; j < lHeight.Num(); j++) {
         if (fabs(fZ - pi[j].fZ) < .01)
            break;
      }
      if (j < lHeight.Num()) {
         // average and increment the counter
         pi[j].fZ = ((pi[j].fZ * pi[j].dwCount) + fZ) / (fp)(pi[j].dwCount+1);
         pi[j].dwCount++;
      }
      else {
         // add it
         IPD id;
         id.dwCount = 1;
         id.fZ = fZ;
         lHeight.Add (&id);
      }
   }

   // look at what we have
   if (!lHeight.Num())
      return FALSE;
   pi = (PIPD) lHeight.Get(0);
   DWORD dwMax;
   dwMax = 0;
   for (i = 1; i < lHeight.Num(); i++)
      if (pi[i].dwCount > pi[dwMax].dwCount)
         dwMax = i;

   // have a height
   fp fHeight;
   fHeight = pi[dwMax].fZ;

   // now, look at intersect a point with the plane, running from
   // the eye to the plane
   CPoint apBoundary[2];
   CPoint pPlane, pPlaneN;
   pPlane.Zero();
   pPlane.p[2] = fHeight;
   pPlaneN.Zero();
   pPlaneN.p[2] = 1;
   for (i = 0; i < 2; i++) {
      CPoint pTo;
      pTo.Copy (&pInfo->paWorldCoord[i ? (pInfo->dwNumWorldCoord-1) : 0]);

      // if its a fence, just take the start/stop location of where clicked
      if (fFence) {
         apBoundary[i].Copy (&pTo);
         continue;
      }

      // look from
      CPoint pFrom;
      if (pInfo->fViewFlat) {
         pFrom.Subtract (&pInfo->pCamera, &pInfo->pLookAt);
         pFrom.Normalize();
         pFrom.Add (&pTo);
      }
      else {
         pFrom.Copy (&pInfo->pCamera);
      }

      // intersect
      if (!IntersectLinePlane (&pFrom, &pTo, &pPlane, &pPlaneN, &apBoundary[i]))
         return FALSE;  // for some reason not intersecting with plane we chose
   }

   // create matrix that rotates and centers the points
   CMatrix mRot;
   CMatrix mInv;
   CPoint pX, pY, pZ;
   pZ.Zero();
   pZ.p[2] = 1;
   pX.Subtract (&apBoundary[1], &apBoundary[0]);
   pX.p[2] = 0;
   pX.Normalize();
   pY.CrossProd (&pZ, &pX);
   pY.Normalize();
   pZ.CrossProd (&pX, &pY);
   pZ.Normalize();
   if (pZ.p[2] < 0.0) {
      pX.Scale(-1);
      pY.Scale(-1);
      pZ.Scale(-1);
   }
   mRot.RotationFromVectors (&pX, &pY, &pZ);

   // half way in between should go to 0
   CPoint pHalf;
   CMatrix mTrans;
   pHalf.Average (&apBoundary[0], &apBoundary[1]);
   mTrans.Translation (pHalf.p[0], pHalf.p[1], pHalf.p[2]);
   mRot.MultiplyRight (&mTrans);

   // invert
   mRot.Invert4 (&mInv);

   // modify the two boundaries - should be (-flen,0,0) to (flen,0,0);
   for (i = 0; i < 2; i++) {
      apBoundary[i].p[3] = 1;
      apBoundary[i].MultiplyLeft (&mInv);
   }

   // create the splines
   CSpline sBottom;
   if (fFence) {
      // find the topography of the land by dividing into lengths
      fp fLen;
      pX.Subtract (&apBoundary[1], &apBoundary[0]);
      fLen = pX.Length();
      DWORD dwDivide;
      dwDivide = max ((DWORD) (fLen / 4.0)+1, 2);
      // BUGFIX - No more than 5 divide segments
      dwDivide = min (5, dwDivide);

      // find topography height
      CListFixed lh;
      lh.Init (sizeof(CPoint));
      lh.Required (dwDivide);
      for (i = 0; i < dwDivide; i++) {
         CPoint pStart, pEnd;
         pStart.Average (&apBoundary[1], &apBoundary[0], (fp)i / (fp)(dwDivide-1));
         pStart.p[3] = 1;
         pEnd.Copy (&pStart);
         pEnd.p[2] += 1.0;

         // convert to world space
         pStart.MultiplyLeft (&mRot);
         pEnd.MultiplyLeft (&mRot);

         DWORD j;
         OSMINTERSECTLINE il;
         // find out where intersects with ground
         memset (&il, 0, sizeof(il));
         il.pStart.Copy (&pStart);
         il.pEnd.Copy (&pEnd);
         for (j = 0; j < pWorld->ObjectNum(); j++) {
            PCObjectSocket pos = pWorld->ObjectGet(j);

            pos->Message (OSM_INTERSECTLINE, &il);

            if (il.fIntersect)
               break;
         }
         if (j >= pWorld->ObjectNum())
            lh.Add (&pStart);
         else
            lh.Add (&il.pIntersect);
         
      
      }

      // the above path needs to be converted into object space
      PCPoint pap;
      pap = (PCPoint) lh.Get(0);
      for (i = 0; i < dwDivide; i++) {
         pap[i].p[3] = 1;
         pap[i].MultiplyLeft (&mInv);
      }

      // create enough points
      if (!memPathTextures.Required (dwDivide * sizeof(TEXTUREPOINT)))
         return FALSE;
      for (i = 0; i < dwDivide; i++) {
         ((PTEXTUREPOINT)memPathTextures.p)[i].h = fAverageWidth;
         ((PTEXTUREPOINT)memPathTextures.p)[i].v = 0;
      }
      // init
      m_sPath.Init (FALSE, dwDivide, pap, (PTEXTUREPOINT) memPathTextures.p,
         (DWORD*)(size_t) dwCurve /*SEGCURVE_LINEAR*/, 2, 2, .1);
         // BUGFIX - switch to dwCurve so can do rivers, with curves
   }
   else {
      // can leave width untouched since only two textures
      m_sPath.Init (FALSE, 2, apBoundary, (PTEXTUREPOINT) memPathTextures.p,
         (DWORD*)(size_t) dwCurve /*SEGCURVE_LINEAR*/, 2, 2, .1);
         // BUGFIX - switch to dwCurve so can do rivers, with curves
   }

   CalcPath();

   // use the calculated matrix
   memset (pResult, 0, sizeof(*pResult));
   pResult->mObject.Copy (&mRot);

   return TRUE;
}



