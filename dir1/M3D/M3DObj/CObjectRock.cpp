/************************************************************************
CObjectRock.cpp - Draws a box.

begun 22/3/05 by Mike Rozak
Copyright 2005 Mike Rozak. All rights reserved
*/

#include <windows.h>
#include "escarpment.h"
#include "..\M3D.h"
#include "resource.h"

static PWSTR gszCorner = L"corner";

typedef struct {
   PWSTR       pszName;    // name of the move point
   int         iX, iY; // -1 if for left/top, 1 for right/bottom
} SPLINEMOVEP, *PSPLINEMOVEP;

static SPLINEMOVEP   gaRockMove[] = {
   L"Center", 0, 0
};


/**********************************************************************************
RockWidth - Returns the number of points on the line of latitude.

inputs
   int      iLat - Latitude. 0 = equator. + = northern hemi, - = southern
   DWORD    dwSize - Number of points in 1/4 of the equator.
returns
   int - Number of points at this latitude, or 0 if error
*/
__inline int RockWidth (int iLat, DWORD dwSize)
{
   iLat = abs(iLat);
   iLat = (int)dwSize - iLat;
   if (iLat >= 1)
      return iLat * 4;
   else if (iLat == 0)
      return 1;   // poles
   else
      return 0;   // error
}

/**********************************************************************************
RockIndexInternal - Given a POSITIVE latitute, this figures out the index into
memory where the rock point is.

inputs
   int      iLong - Longitude. From 0 to RockWidth(iLat, dwSize)
   int      iLat - Latitude. 0 = equator. + = northern hemi, - = southern
   DWORD    dwSize - Number of points in 1/4 of the equator.
returns
   DWORD - Index into array
*/
__inline DWORD RockIndexInternal (int iLong, int iLat, DWORD dwSize)
{
   if (iLat >= (int)dwSize)
      return 0;
   else
      return (DWORD) (((int)dwSize - iLat) * ((int)dwSize - iLat - 1) * (4 / 2) + iLong + 1);
}



/**********************************************************************************
RockIndex - Given a longitude and latitude, determines the index into the memory.

inputs
   int      iLong - Longitude. From 0 to RockWidth(iLat, dwSize)
   int      iLat - Latitude. 0 = equator. + = northern hemi, - = southern
   DWORD    dwSize - Number of points in 1/4 of the equator.
returns
   DWORD - Index into array
*/
__inline DWORD RockIndex (int iLong, int iLat, DWORD dwSize)
{
   if (iLat >= 0)
      return RockIndexInternal (iLong, iLat, dwSize);

   // else, work backwards
   int iWidthEquator = (int)dwSize * 4; // boiling down RockWidth (0, dwSize)
   int iIndexToEquator = (int)(dwSize * (dwSize - 1) * 2 + 1); // boiling down RockIndexInternal (0, 0, dwSize)
   int iTotalPoints = iIndexToEquator * 2 + iWidthEquator;
   int iIndexLineStart = iTotalPoints - RockIndexInternal (0, -iLat-1, dwSize);
   return iIndexLineStart + iLong;
}


/**********************************************************************************
RockNumPoints - Returns the number of points necessary

inputs
   DWORD    dwSize - Number of points in 1/4 of the equator.
returns
   DWORD - Number of points
*/
DWORD RockNumPoints (DWORD dwSize)
{
   return RockIndex (1, -(int)dwSize, dwSize);
}

/**********************************************************************************
RockAdjacentInternal - Returns the adjacent points.

inputs
   int      iLong - Longitude
   int      iLat - Latitude
   DWORD    dwSize - Number of points in 1/4 of the equator.
   BOOL     fAbove - If TRUE then return the points in the positive latitude direction.
            else, negative
   POINT    *papAdjacent - Filled in with 0..3 points. Each one is a DWORD with .x being
            longitude, and .y being latitude
returns
   DWORD - Number of points filled in. Max 3. Will be in a clockwise direction.
*/
DWORD RockAdjacentInternal (int iLong, int iLat, DWORD dwSize, BOOL fAbove, POINT *papAdjacent)
{
   int iLookAt = iLat + (fAbove ? 1 : -1);
   if ((iLookAt < -(int)dwSize) || (iLookAt > (int)dwSize))
      return 0;   // no points

   int iAbsLat = abs(iLat);
   int iAbsLookAt = abs(iLookAt);
   DWORD dwNumLookAt = RockWidth (iLookAt, dwSize) / 4;
   if (!dwNumLookAt) {
      // there's only one point then
      papAdjacent[0].x = 0;
      papAdjacent[0].y = iLookAt;
      return 1;
   }

   DWORD dwNumCur = RockWidth (iLat, dwSize) / 4;
   DWORD dwHemiCur = (DWORD)iLong / dwNumCur;
   DWORD dwModCur = (DWORD)iLong - dwHemiCur * dwNumCur;
   DWORD dwRet;

   BOOL fSwap;

   if (dwNumLookAt < dwNumCur) {
      fSwap = (iLookAt < 0);

      // look at is smaller

      if (dwModCur == 0) {
         // one points
         dwRet = 1;
         papAdjacent[0].x = (int)dwHemiCur * (int)dwNumLookAt;
         papAdjacent[0].y = iLookAt;
      }
      else {
         // two points
         dwRet = 2;
         papAdjacent[0].x = (int)dwHemiCur * (int)dwNumLookAt + (int)dwModCur - 1;
         papAdjacent[1].x = (papAdjacent[0].x + 1) % (int)(dwNumLookAt*4);
         papAdjacent[0].y = papAdjacent[1].y = iLookAt;
      }
   }
   else {
      fSwap = (iLat > 0);

      // look at is larger
      if (dwModCur == 0) {
         // three points
         dwRet = 3;
         papAdjacent[0].x = (int)((dwHemiCur + 4) * (int)dwNumLookAt - 1) % (int)(dwNumLookAt*4);
         papAdjacent[1].x = (papAdjacent[0].x + 1) % (int)(dwNumLookAt*4);
         papAdjacent[2].x = (papAdjacent[1].x + 1) % (int)(dwNumLookAt*4);
         papAdjacent[0].y = papAdjacent[1].y = papAdjacent[2].y = iLookAt;
      }
      else {
         // two points
         dwRet = 2;
         papAdjacent[0].x = (int)dwHemiCur * (int)dwNumLookAt + (int)dwModCur;
         papAdjacent[1].x = papAdjacent[0].x + 1;  // dont need modulo
         papAdjacent[0].y = papAdjacent[1].y = iLookAt;
      }
   }

   DWORD i;
   if (fSwap) for (i = 0; i < dwRet / 2; i++) {
      POINT pTemp = papAdjacent[i];
      papAdjacent[i] = papAdjacent[dwRet - i - 1];
      papAdjacent[dwRet - i - 1] = pTemp;
   } // i

   return dwRet;
}


/**********************************************************************************
RockAdjacent - Returns the adjacent points, in a clockwise direction.

inputs
   int      iLong - Longitude
   int      iLat - Latitude
   DWORD    dwSize - Number of points in 1/4 of the equator.
   POINT    *papAdjacent - Filled in with 0..3 points. Each one is a DWORD with .x being
            longitude, and .y being latitude
returns
   DWORD - Number of points filled in. Max 6. Will be in a clockwise direction.
*/
DWORD RockAdjacent (int iLong, int iLat, DWORD dwSize, POINT *papAdjacent)
{
   DWORD dwRet = 0;
   DWORD dwWidth = RockWidth (iLat, dwSize);
   DWORD i;

   if (dwWidth <= 1) {
      // on the top or the bottom
      if (iLat > 0)
         for (i = 0; i < 4; i++) {
            papAdjacent[i].x = (int) (3-i);
            papAdjacent[i].y = iLat - 1;
         } // i
      else // on the bottom
         for (i = 0; i < 4; i++) {
            papAdjacent[i].x = (int)i;
            papAdjacent[i].y = iLat + 1;
         } // i

      return 4;
   }

   // points above
   dwRet += RockAdjacentInternal (iLong, iLat, dwSize, TRUE, papAdjacent + dwRet);

   // to the right
   papAdjacent[dwRet].x = (iLong + 1) % (int)dwWidth;
   papAdjacent[dwRet].y = iLat;
   dwRet++;

   // points below
   dwRet += RockAdjacentInternal (iLong, iLat, dwSize, FALSE, papAdjacent + dwRet);

   // to the left
   papAdjacent[dwRet].x = (iLong + (int)dwWidth - 1) % (int)dwWidth;
   papAdjacent[dwRet].y = iLat;
   dwRet++;

   return dwRet;
}

/**********************************************************************************
CObjectRock::Constructor and destructor */
CObjectRock::CObjectRock (PVOID pParams, POSINFO pInfo)
{
   m_OSINFO = *pInfo;

   m_dwRenderShow = RENDERSHOW_LANDSCAPING;

   m_pSuperQuad.Zero();
   m_pCorner.Zero();
   m_pCorner.p[0] = m_pCorner.p[1] = m_pCorner.p[2] = 0.5;
   m_fStretchToFit = FALSE;
   m_pNoise.p[0] = m_pNoise.p[1] = m_pNoise.p[2] = m_pNoise.p[3] = 0.5;
   m_iSeed = GetTickCount() & 0xfff;

   m_fDirty = TRUE;
   m_dwSize = 4;

   DWORD i;
   for (i = 0; i < 8; i++) {
      m_apEdge[i].p[0] = (i & 0x01) ? 1 : -1;
      m_apEdge[i].p[1] = (i & 0x02) ? 1 : -1;
      m_apEdge[i].p[2] = (i & 0x04) ? 1 : -1;
      m_apEdge[i].p[3] = 1;
   } // i

   // color for the box
   ObjectSurfaceAdd (10, RGB(0xc0,0xc0,0xc0), MATERIAL_PAINTSEMIGLOSS, L"Rock surface",
      &GTEXTURECODE_Rock, &GTEXTURESUB_Rock);
}


CObjectRock::~CObjectRock (void)
{
   // do nothing for now
}


/**********************************************************************************
CObjectRock::Delete - Called to delete this object
*/
void CObjectRock::Delete (void)
{
   delete this;
}

/**********************************************************************************
CObjectRock::Render - Draws a test box.

inputs
   POBJECTRENDER     pr - Render information
*/
void CObjectRock::Render (POBJECTRENDER pr, DWORD dwSubObject)
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

   // calculae the info
   CalcInfoIfNecessary ();
   DWORD dwPoints = RockNumPoints (m_dwSize);

   // textures
   DWORD dwTextIndex;
   DWORD i;
   PTEXTPOINT5 pText = m_Renderrs.NewTextures (&dwTextIndex, dwPoints);
   if (pText) {
      memcpy (pText, m_patpText, dwPoints * sizeof(TEXTPOINT5));

      if (!m_fStretchToFit)
         m_Renderrs.ApplyTextureRotation (pText, dwPoints);

      // go through and offset .h and .v so top of rock at 0.5, 0.5
      for (i = 0; i < dwPoints; i++) {
         pText[i].hv[0] += 0.5;
         pText[i].hv[1] += 0.5;
      }
   } // if pText

   // facing towards the viewer
   DWORD dwPointIndex, dwNormIndex;
   PCPoint pPoint, pNorm;
   pPoint = m_Renderrs.NewPoints (&dwPointIndex, dwPoints);
   pNorm = m_Renderrs.NewNormals (TRUE, &dwNormIndex, dwPoints);
   if (pPoint)
      memcpy (pPoint, m_paPoint, dwPoints * sizeof(CPoint));
   if (pNorm)
      memcpy (pNorm, m_paNorm, dwPoints * sizeof(CPoint));

   // make verticies
   DWORD dwVertIndex;
   PVERTEX pv;
   DWORD dwColor = m_Renderrs.DefColor ();
   pv = m_Renderrs.NewVertices (&dwVertIndex, dwPoints);
   for (i = 0; i < dwPoints; i++) {
      pv[i].dwColor = dwColor;
      pv[i].dwNormal = pNorm ? (dwNormIndex + i) : 0;
      pv[i].dwPoint = dwPointIndex + i;
      pv[i].dwTexture = pText ? (dwTextIndex + i) : 0;
   }

   int iX, iY, iAbove;
   DWORD dwWidth, dwWidthAbove, dwWidth4, dwWidthAbove4, dwHemi, dwMod;
   DWORD dwV1, dwV2, dwV3, dw;

   // draw the top and bottom halves
   DWORD dwTop, dwY;
   for (dwTop = 0; dwTop < 2; dwTop++) {
      for (dwY = 0; dwY < m_dwSize; dwY++) {
         iY = dwTop ? (int)dwY : -(int)dwY;
         iAbove = iY + (dwTop ? 1 : -1);
         dwWidth = RockWidth (iY, m_dwSize);
         dwWidthAbove = RockWidth (iAbove, m_dwSize);
         dwWidth4 = dwWidth / 4;
         dwWidthAbove4 = dwWidthAbove / 4;

         for (iX = 0; iX < (int) dwWidth; iX++) {
            dwHemi = (DWORD)iX / dwWidth4;
            dwMod = (DWORD)iX - dwHemi * dwWidth4;

            dwV1 = dwVertIndex + RockIndex (iX, iY, m_dwSize);
            dwV2 = dwVertIndex + RockIndex (dwWidthAbove4 ? ((dwHemi * dwWidthAbove4 + dwMod) % dwWidthAbove) : 0, iAbove, m_dwSize);
            dwV3 = dwVertIndex + RockIndex ((iX+1) % (int)dwWidth, iY, m_dwSize);

            if (!dwTop) {
               // swap
               dw = dwV2;
               dwV2 = dwV3;
               dwV3 = dw;
            }

            m_Renderrs.NewTriangle (dwV1, dwV2, dwV3, TRUE);

            // do other triangle
            if (!dwMod || !dwWidthAbove4)
               continue;   // no triangle to the left

            dwV1 = dwVertIndex + RockIndex (iX, iY, m_dwSize);
            dwV2 = dwVertIndex + RockIndex ((dwHemi * dwWidthAbove4 + dwMod - 1) % dwWidthAbove, iAbove, m_dwSize);
            dwV3 = dwVertIndex + RockIndex ((dwHemi * dwWidthAbove4 + dwMod) % dwWidthAbove, iAbove, m_dwSize);

            if (!dwTop) {
               // swap
               dw = dwV2;
               dwV2 = dwV3;
               dwV3 = dw;
            }

            m_Renderrs.NewTriangle (dwV1, dwV2, dwV3, TRUE);

         } // iX
      } // iY
   } // dwTop

   m_Renderrs.Commit();
}



/**********************************************************************************
CObjectRock::QueryBoundingBox - Standard API
*/
void CObjectRock::QueryBoundingBox (PCPoint pCorner1, PCPoint pCorner2, DWORD dwSubObject)
{
   pCorner2->Copy (&m_pCorner);
   pCorner2->Scale (1 + (m_pNoise.p[0] + m_pNoise.p[1] +  m_pNoise.p[2] + m_pNoise.p[3]) * 0.5 /*fNoiseScale*/);
   pCorner1->Copy (pCorner2);
   pCorner1->Scale (-1);

#ifdef _DEBUG
   // test, make sure bounding box not too small
   CPoint p1,p2;
   DWORD i;
   CObjectTemplate::QueryBoundingBox (&p1, &p2, dwSubObject);
   for (i = 0; i < 3; i++)
      if ((p1.p[i] < pCorner1->p[i] - CLOSE) || (p2.p[i] > pCorner2->p[i] + CLOSE))
         break;
   if (i < 3)
      OutputDebugString ("\r\nCCObjectRock::QueryBoundingBox too small.");
#endif
}

/**********************************************************************************
CObjectRock::Clone - Clones this

inputs
   none
returns
   PCObjectSocket - Clone
*/
PCObjectSocket CObjectRock::Clone (void)
{
   PCObjectRock pNew;

   pNew = new CObjectRock(NULL, &m_OSINFO);

   // clone template info
   CloneTemplate(pNew);

   // Clone member variables'
   DWORD i;
   for (i = 0; i < 8; i++)
      m_apEdge[i].Copy (&m_apEdge[i]);
   pNew->m_pSuperQuad.Copy (&m_pSuperQuad);
   pNew->m_pCorner.Copy (&m_pCorner);
   pNew->m_fStretchToFit = m_fStretchToFit;
   pNew->m_dwSize = m_dwSize;
   pNew->m_pNoise.Copy (&m_pNoise);
   pNew->m_iSeed = m_iSeed;

   pNew->m_fDirty = TRUE;

   return pNew;
}



static PWSTR gpszSize = L"Size";
static PWSTR gpszStretchToFit = L"StretchToFit";
static PWSTR gpszNoise = L"Noise";
static PWSTR gpszSeed = L"Seed";
static PWSTR gpszSuperQuad = L"SuperQuad";

PCMMLNode2 CObjectRock::MMLTo (void)
{
   // call into the template first
   PCMMLNode2 pNode = MMLToTemplate();
   if (!pNode)
      return NULL;

   // member variables go here
   MMLValueSet (pNode, gpszSuperQuad, &m_pSuperQuad);
   MMLValueSet (pNode, gszCorner, &m_pCorner);
   MMLValueSet (pNode, gpszSize, (int)m_dwSize);
   MMLValueSet (pNode, gpszStretchToFit, (int)m_fStretchToFit);
   MMLValueSet (pNode, gpszNoise, &m_pNoise);
   MMLValueSet (pNode, gpszSeed, m_iSeed);

   DWORD i;
   WCHAR szTemp[64];
   for (i = 0; i < 8; i++) {
      swprintf (szTemp, L"Edge%d", (int)i);
      MMLValueSet (pNode, szTemp, &m_apEdge[i]);
   } // i

   return pNode;
}

BOOL CObjectRock::MMLFrom (PCMMLNode2 pNode)
{
   // call into the template first
   if (!MMLFromTemplate (pNode))
      return FALSE;

   // member variables go here
   MMLValueGetPoint (pNode, gpszSuperQuad, &m_pSuperQuad);
   MMLValueGetPoint (pNode, gszCorner, &m_pCorner);
   m_dwSize = (DWORD) MMLValueGetInt (pNode, gpszSize, 1);
   m_fStretchToFit = (DWORD) MMLValueGetInt (pNode, gpszStretchToFit, FALSE);
   MMLValueGetPoint (pNode, gpszNoise, &m_pNoise);
   m_iSeed = MMLValueGetInt (pNode, gpszSeed, 1234);

   DWORD i;
   WCHAR szTemp[64];
   for (i = 0; i < 8; i++) {
      swprintf (szTemp, L"Edge%d", (int)i);
      MMLValueGetPoint (pNode, szTemp, &m_apEdge[i]);
   } // i

   m_fDirty = TRUE;

   return TRUE;
}

/*************************************************************************************
CObjectRock::ControlPointQuery - Called to query the information about a control
point identified by dwID.

inputs
   DWORD       dwID - ID
   POSCONTROL  pInfo - Filled in with the infomration
returns
   BOOL - TRUE if successful
*/
#define EDGESCALE       0.9      // so edges dont cover corner
BOOL CObjectRock::ControlPointQuery (DWORD dwID, POSCONTROL pInfo)
{
   if (!(dwID == 10) && !((dwID >= 20) && (dwID < 28)) )
      return FALSE;

   memset (pInfo,0, sizeof(*pInfo));

   pInfo->dwID = dwID;
   // BUGFIX - Since freedom != 0 causes problems, use freedom=0: pInfo->dwFreedom = 2;   // line
   pInfo->dwStyle = (dwID == 10) ? CPSTYLE_POINTER : CPSTYLE_SPHERE;
   pInfo->fSize = m_pCorner.Length() / 20;
   pInfo->cColor = (dwID == 10) ? RGB(0xff,0,0xff) : RGB(0, 0xff, 0);

   // start location in mid point
   if (dwID == 10)
      pInfo->pLocation.Copy (&m_pCorner);
   else {
      // edge
      pInfo->pLocation.Copy (&m_pCorner);
      pInfo->pLocation.Scale (EDGESCALE);

      pInfo->pLocation.p[0] *= m_apEdge[dwID-20].p[0];
      pInfo->pLocation.p[1] *= m_apEdge[dwID-20].p[1];
      pInfo->pLocation.p[2] *= m_apEdge[dwID-20].p[2];
   }
   //pInfo->pV1.Zero();

   wcscpy (pInfo->szName, (dwID == 10) ? L"Size" : L"Skew");

   return TRUE;
}

/*************************************************************************************
CObjectRock::ControlPointSet - Called to change the valud of a control point.

inputs
   DWORD       dwID - ID
   PCPoint     pVal - Contains the new location, in object coordinates
returns
   BOOL - TRUE if successful
*/
BOOL CObjectRock::ControlPointSet (DWORD dwID, PCPoint pVal, PCPoint pViewer)
{
   if (!(dwID == 10) && !((dwID >= 20) && (dwID < 28)) )
      return FALSE;

   // tell the world we're about to change
   if (m_pWorld)
      m_pWorld->ObjectAboutToChange (this);

   if (dwID == 10) {
      m_pCorner.Copy (pVal);
      m_pCorner.p[0] = max(m_pCorner.p[0], CLOSE);
      m_pCorner.p[1] = max(m_pCorner.p[1], CLOSE);
      m_pCorner.p[2] = max(m_pCorner.p[2], CLOSE);
   }
   else {
      // edge
      CPoint p;
      DWORD i;
      p.Copy (pVal);
      for (i = 0; i < 3; i++) {
         p.p[i] = p.p[i] / m_pCorner.p[i] / EDGESCALE;
         p.p[i] = max(p.p[i], -1);
         p.p[i] = min(p.p[i], 1);
      }

      m_apEdge[dwID-20].Copy (&p);
   }

   m_fDirty = TRUE;

   // tell the world we've changed
   if (m_pWorld)
      m_pWorld->ObjectChanged (this);

   return TRUE;
}

/*************************************************************************************
CObjectRock::ControlPointEnum - Called to enumerate a list of control point IDs
into the list.

inputs
   PCListFixed       plDWORD - Should be filled in with control points
returns
   none
*/
void CObjectRock::ControlPointEnum (PCListFixed plDWORD)
{
   // 1 control points starting at 10
   DWORD i;
   i = 10;
   plDWORD->Add (&i);

   for (i = 20; i < 28; i++)
      plDWORD->Add (&i);
}



/**********************************************************************************
DetermineFactors - Determines the factors of a number and adds the factor onto
the list.

inputs
   DWORD             dwNumber - Number to factor
   PCListFixed       plFactor - Filled with a DWORD with the "prime" number factor
                     NOTE: This only factors for 2 or 3
   PCListFixed       plNumber - Filled with this DOWRD number / the factor.
returns
   none
*/
void DetermineFactors (DWORD dwNumber, PCListFixed plFactor, PCListFixed plNumber)
{
   // if already down to 1 then nothing
   if (dwNumber <= 1) {
      plFactor->Add (&dwNumber);
      dwNumber = 1;
      plNumber->Add (&dwNumber);
      return;
   }

   DWORD dwFactor;
   if ((dwNumber / 2) * 2 == dwNumber)
      dwFactor = 2;
   else if ((dwNumber / 3) * 3 == dwNumber)
      dwFactor = 3;
   else
      dwFactor = dwNumber;

   plFactor->Add (&dwFactor);
   dwNumber /= dwFactor;
   plNumber->Add (&dwNumber);

   DetermineFactors (dwNumber, plFactor, plNumber);
}

/**********************************************************************************
NoiseScaleRandom - Returns a random number based on the scale

inputs
   fp          fCurValue - Current value... If not 0 then will just return the current value
   fp          fScale - Number from 0..1
   fp          fA - Value of the A edge of the triangle (original corner)
   fp          fB - Value of the B edge of the triangle (horizontal adjust)
   fp          fC - Value of the C edge of the triangle (vertcal adjust)
   fp          fAlphaX - From 0..1, amount of interpoaltion, 0 = fA
   fp          fAlphaY - From 0..1, amount of interpoaltion. 0 = fA
returns
   fp - New value to use
*/
fp NoiseScaleRandom (fp fCurValue, fp fScale, fp fA, fp fB, fp fC, fp fAlphaX, fp fAlphaY)
{
   if (fCurValue > 0)
      return fCurValue; // no change

   fp f;
   if (fScale) {
      f = MyRand (1.0, 1.0 + fScale);
      if (rand() % 2)
         f = 1.0 / f;
   }
   else
      f = 1;

   // already set, but adjust by randomness
   if (fCurValue < 0)
      return -fCurValue * f;

   if (fAlphaY < 0.99)
      fAlphaX /= (1.0 - fAlphaY);   // since fALphaX + fAlphaY <= 1
   else
      fAlphaX = 1.0;

   fp fLeft = (1.0 - fAlphaY) * fA + fAlphaY * fC;
   fp fRight = (1.0 - fAlphaY) * fB + fAlphaY * fC;

   fp fInterp = (1.0 - fAlphaX) * fLeft + fAlphaX * fRight;

   return f * fInterp;
}

/**********************************************************************************
MoveUpDownAdjust - Adjusts the X based on in the new and old positions

inputs
   int         iX - X offset
   int         iY - Y offset
   DWORD       dwDivisionSize - Divisions along X
   DWORD       dwWidthCur - Width where iX starts from (RockWidth())
   DWORD       dwWidthNew - Width where iX ends up at (RockWidth())
returns
   int - New iX
*/
__inline int MoveUpDownAdjust (int iX, int iY, DWORD dwDivisionSize, DWORD dwWidthCur, DWORD dwWidthNew)
{
   if (dwWidthNew <= 1)
      return 0;   // always go to 0
   if (dwWidthCur <= 1)
      return 0;   // eror, shouldnt happen

   DWORD dwHemi = (DWORD)iX / (dwWidthCur / 4);// BUGFIX - Was / divisionsize, but wrong calc
   // DWORD dwMod = (DWORD)iX - dwHemi * dwDivisionSize;

   if (dwWidthNew < dwWidthCur)
      return iX - (int)dwHemi * abs(iY);
   else if (dwWidthCur < dwWidthNew)
      return iX + (int)dwHemi * abs(iY);
   else
      return iX;  // shouldnt happen

   // return (iX * (int)dwWidthNew) / (int)dwWidthCur;
}

/**********************************************************************************
CObjectRock::FractalNoise - Adds fractal noise to the info

inputs
   int         iLong - Longitude
   int         iLat - LAtitude
   DWORD       dwSize - Size of the triangle... will look at points with iLong+dwSize,
               and iLat +/-dwSize. Will use modulo for iLong.
   BOOL        fAbove - If TRUE then use positive longitude, else negative longitude
   BOOL        fRight - If TRUE then use positive latitude, else negative latitide
   DWORD       dwFactor - Number to divide into, either 2, 3, or another number.
   fp          fNoiseScale - Amount of scale to the noise.
   PCPoint     paScratch - Scratch memory. Modify p[3], if it's not already set, to
               be the scaling
returns
   none
*/
void CObjectRock::FractalNoise (int iLong, int iLat, DWORD dwSize, BOOL fAbove, BOOL fRight,
                                DWORD dwFactor, fp fNoiseScale, PCPoint paScratch)
{
   int iOther = iLat + (int)dwSize * (fAbove ? 1 : -1);
   if (abs(iOther) > (int)m_dwSize)
      return; // shouldnt happen


   DWORD dwWidthCur = RockWidth (iLat, m_dwSize);
   DWORD dwWidthOther = RockWidth (iOther, m_dwSize);

   // triangle
   POINT ap[3];
   fp afValue[3];
   ap[0].x = iLong;
   ap[0].y = iLat;
   ap[1].x = iLong + (int)dwSize * (fRight ? 1 : -1);   // NOTE: No modulo done yet
   ap[1].y = iLat;
   ap[2].x = MoveUpDownAdjust (iLong, iOther - iLat, dwSize, dwWidthCur, dwWidthOther);  // NOTE: no modulo done yet
   ap[2].y = iOther;

   // make sure the triangle corners all have values
   DWORD i, dwIndex, dwWidth;
   for (i = 0; i < 3; i++) {
      dwWidth = RockWidth(ap[i].y, m_dwSize);
      dwIndex = RockIndex ((ap[i].x + (int)dwWidth) % (int)dwWidth, ap[i].y, m_dwSize);
      paScratch[dwIndex].p[3] = NoiseScaleRandom (paScratch[dwIndex].p[3], fNoiseScale,
         1, 1, 1, 0, 0);
      afValue[i] = paScratch[dwIndex].p[3];
   }

   // now, go though in-between points
   DWORD dwSkip = ((dwFactor == 2) || (dwFactor == 3)) ? (dwSize / dwFactor) : 1;
   DWORD dwY, dwX;
   int iX, iY;
   for (dwY = 0; dwY < dwSize; dwY += dwSkip) {
      if (fAbove)
         iY = iLat + (int)dwY;
      else
         iY = iLat - (int)dwY;

      dwWidth = RockWidth (iY, m_dwSize);
      int iOffset = MoveUpDownAdjust (iLong, iY - iLat, dwSize, dwWidthCur, dwWidth);

      for (dwX = 0; dwX <= dwSize - dwY; dwX += (int)dwSkip) {
         if (fRight)
            iX = (iOffset + (int) dwX) % (int)dwWidth;
         else
            iX = ((int)dwWidth + iOffset - (int)dwX) % (int)dwWidth;

         dwIndex = RockIndex (iX, iY, m_dwSize);
         paScratch[dwIndex].p[3] = NoiseScaleRandom (paScratch[dwIndex].p[3], fNoiseScale,
            afValue[0], afValue[1], afValue[2], (fp)dwX / (fp)dwSize, (fp)dwY / (fp)dwSize);
      } // iX
   } // iY

   // BUGBUG - sometimes division by 3 (as in 27 total divisions) doesn't seem to work
   // quite right, but not eactly sure why
}


/**********************************************************************************
CObjectRock::CalcInfoIfNecessary - This calculates the rock information if
the dirty flag is set.
*/
void CObjectRock::CalcInfoIfNecessary (void)
{
   if (!m_fDirty)
      return;
   m_fDirty = FALSE;

   srand (m_iSeed);

   // make sure have some size
   m_dwSize = max(m_dwSize, 1);

   // make sure have enough memory
   DWORD dwPoints = RockNumPoints (m_dwSize);
   DWORD dwNeed = dwPoints * (sizeof(CPoint) + sizeof(CPoint) + sizeof(TEXTPOINT5));
   if (!m_memCalc.Required (dwNeed))
      return;  // error
   m_paPoint = (PCPoint) m_memCalc.p;
   m_paNorm = m_paPoint + dwPoints;
   m_patpText = (PTEXTPOINT5) (m_paNorm + dwPoints);

   // also, make scratch values...
   CMem memScratch;
   if (!memScratch.Required(dwPoints * sizeof(CPoint)))
      return;
   PCPoint paScratch = (PCPoint)memScratch.p;

   // start out with radius of 0.0 so know that need to calculate
   // except, randomly fill 8 points
   DWORD dwWidth, dwIndex, dwSkip;
   int iX, iY;
   for (iY = (int)m_dwSize; iY >= -(int)m_dwSize; iY--) {
      BOOL fOKY = (abs(iY) == (int)m_dwSize) || !iY;
      dwWidth = RockWidth (iY, m_dwSize);
      dwIndex = RockIndex (0, iY, m_dwSize);
      for (iX = 0; iX < (int)dwWidth; iX++, dwIndex++) {
         paScratch[dwIndex].p[3] = 0;

         // if one of the 8 magic points then set to random
         if (fOKY && !(iX % m_dwSize))
            paScratch[dwIndex].p[3] = NoiseScaleRandom (paScratch[dwIndex].p[3], m_pNoise.p[0],
               1, 1, 1, 0, 0);
      }
   } // iY

   // figure out the factors and then sudivide
   CListFixed lFactors, lNumbers;
   lFactors.Init (sizeof(DWORD));
   lNumbers.Init (sizeof(DWORD));
   DetermineFactors (m_dwSize, &lFactors, &lNumbers);
   DWORD *pdwFactor = (DWORD*) lFactors.Get(0);
   DWORD *pdwNumber = (DWORD*) lNumbers.Get(0);
   DWORD i;
   CPoint apInterp4[4], apInterp2[2];

   fp fNoiseScale = 0.5;
   for (i = 0; i < lFactors.Num(); i++, pdwFactor++, pdwNumber++, fNoiseScale /= 2.0) {
      fp fCurNoiseScale = fNoiseScale * m_pNoise.p[min(i+1, 3)];
      dwSkip = *pdwNumber * *pdwFactor;

      // make sure all numbers are 0 (not set) or negative (set, but can be modified)
      for (iY = 0; iY < (int) dwPoints; iY++)
         if (paScratch[iY].p[3] > 0)
            paScratch[iY].p[3] *= -1;

      for (iY = 0; iY < (int)m_dwSize; iY += (int)dwSkip) {
         dwWidth = RockWidth (iY, m_dwSize);
         for (iX = 0; iX < (int)dwWidth; iX += (int)dwSkip) {
            FractalNoise (iX, iY, dwSkip, TRUE, TRUE, *pdwFactor, fCurNoiseScale, paScratch);
            FractalNoise (iX, -iY, dwSkip, FALSE, TRUE, *pdwFactor, fCurNoiseScale, paScratch);

            // also go left points
            if (iY + (int)dwSkip < (int)m_dwSize) {
               DWORD dwWidthCur = RockWidth (iY+(int)dwSkip, m_dwSize);
               FractalNoise (MoveUpDownAdjust(iX, (int)dwSkip, dwSkip, dwWidthCur, dwWidth),
                  iY+(int)dwSkip, dwSkip, FALSE, FALSE, *pdwFactor, fCurNoiseScale, paScratch);
               FractalNoise (MoveUpDownAdjust(iX, -(int)dwSkip, dwSkip, dwWidthCur, dwWidth),
                  -(iY+(int)dwSkip), dwSkip, TRUE, TRUE, *pdwFactor, fCurNoiseScale, paScratch);
            }

         } // iX
      } // iY
   } // i

   // make sure all numbers are positive
   for (iY = 0; iY < (int) dwPoints; iY++) {
      if (paScratch[iY].p[3] < 0)
         paScratch[iY].p[3] *= -1;
      else if (paScratch[iY].p[3] == 0)
         paScratch[iY].p[3] = 1;   // shuldnt happen
   }

   // store away the latitude and longitude, in radians
   fp fLat;
   for (iY = (int)m_dwSize; iY >= -(int)m_dwSize; iY--) {
      fLat = (fp)iY / (fp)m_dwSize * PI/2;

      dwWidth = RockWidth (iY, m_dwSize);
      dwIndex = RockIndex (0, iY, m_dwSize);

      for (iX = 0; iX < (int)dwWidth; iX++, dwIndex++) {
         paScratch[dwIndex].p[0] = -(fp)iX / (fp)dwWidth * 2.0 * PI; // since mentally iX is counterclockwise
         paScratch[dwIndex].p[1] = fLat;
      } // iX
   } // iY


   // determine point locations
   for (iY = (int)m_dwSize; iY >= -(int)m_dwSize; iY--) {
      dwIndex = RockIndex (0, iY, m_dwSize);
      dwWidth = RockWidth (iY, m_dwSize);

      fp fCosY, fSinY;
      for (iX = 0; iX < (int)dwWidth; iX++, dwIndex++) {
         if (!iX) {
            // optimization so only calculate once
            fCosY = cos (paScratch[dwIndex].p[1]);
            fSinY = sin (paScratch[dwIndex].p[1]);
         }

         m_paPoint[dwIndex].p[0] = sin (paScratch[dwIndex].p[0]) * fCosY;
         m_paPoint[dwIndex].p[1] = cos (paScratch[dwIndex].p[0]) * fCosY;
         m_paPoint[dwIndex].p[2] = fSinY;
         m_paPoint[dwIndex].p[3] = 1;

         // scale point
         m_paPoint[dwIndex].Scale (paScratch[dwIndex].p[3]);

         // will need super quadratic
         for (i = 0; i < 3; i++)
            if (m_pSuperQuad.p[i]) {
               BOOL fNeg;
               if (m_paPoint[dwIndex].p[i] < 0) {
                  fNeg = TRUE;
                  m_paPoint[dwIndex].p[i] *= -1;
               }
               else
                  fNeg = FALSE;
               m_paPoint[dwIndex].p[i] = pow((fp)m_paPoint[dwIndex].p[i], (fp)(1.0 - m_pSuperQuad.p[i]));
               if (fNeg)
                  m_paPoint[dwIndex].p[i] *= -1;
            }

         // scale point by 8 corner points that can bend with
         fp f = (m_paPoint[dwIndex].p[2] + 1) / 2;;
         for (i = 0; i < 4; i++)
            apInterp4[i].Average (&m_apEdge[i+4], &m_apEdge[i], f);
         f = (m_paPoint[dwIndex].p[1] + 1) / 2;
         for (i = 0; i < 2; i++)
            apInterp2[i].Average (&apInterp4[i+2], &apInterp4[i], f);
         m_paPoint[dwIndex].Average (&apInterp2[1], &apInterp2[0], (m_paPoint[dwIndex].p[0]+1)/2);

         // scale point by m_pCorner
         m_paPoint[dwIndex].p[0] *= m_pCorner.p[0];
         m_paPoint[dwIndex].p[1] *= m_pCorner.p[1];
         m_paPoint[dwIndex].p[2] *= m_pCorner.p[2];

      } // iX

   } // iY

   // determine the textures
   TEXTUREPOINT tpScaleText, tpDist;
   if (m_fStretchToFit) {
      tpScaleText.h = 0.5 / (fp) (2*m_dwSize);
      tpScaleText.v = 0.5 / (fp) (2*m_dwSize);
   }
   else {
      tpScaleText.h = PI * m_pCorner.p[0] / (fp)(2*m_dwSize);
      tpScaleText.v = PI * m_pCorner.p[1] / (fp)(2*m_dwSize);
   }

   for (iY = (int)m_dwSize; iY >= -(int)m_dwSize; iY--) {
      tpDist.h = tpScaleText.h * (fp)((int)m_dwSize - iY);
      tpDist.v = tpScaleText.v * (fp)((int)m_dwSize - iY);

      dwIndex = RockIndex (0, iY, m_dwSize);
      dwWidth = RockWidth (iY, m_dwSize);
      for (iX = 0; iX < (int)dwWidth; iX++, dwIndex++) {
         m_patpText[dwIndex].hv[0] = sin (paScratch[dwIndex].p[0]) * tpDist.h;
         m_patpText[dwIndex].hv[1] = -cos (paScratch[dwIndex].p[0]) * tpDist.v;

         m_patpText[dwIndex].xyz[0] = m_paPoint[dwIndex].p[0];
         m_patpText[dwIndex].xyz[1] = m_paPoint[dwIndex].p[1];
         m_patpText[dwIndex].xyz[2] = m_paPoint[dwIndex].p[2];
      } // iX
   } // iY

   // determine the normals
   POINT apNear[6];  // nearby points, in clockwise direction
   CPoint apVect[6];
   DWORD dwNum;
   CPoint pSum, pNorm;
   DWORD j;
   for (iY = (int)m_dwSize; iY >= -(int)m_dwSize; iY--) {
      dwIndex = RockIndex (0, iY, m_dwSize);
      dwWidth = RockWidth (iY, m_dwSize);
      for (iX = 0; iX < (int)dwWidth; iX++, dwIndex++) {
         dwNum = RockAdjacent (iX, iY, m_dwSize, apNear);

         for (j = 0; j < dwNum; j++)
            apVect[j].Subtract (m_paPoint + RockIndex (apNear[j].x, apNear[j].y, m_dwSize), m_paPoint + dwIndex);

         pSum.Zero();
         for (j = 0; j < dwNum; j++) {
            pNorm.CrossProd (&apVect[(j+1)%dwNum], &apVect[j]);
            pNorm.Normalize();
            pSum.Add (&pNorm);
         } // j
         pSum.Normalize();
         if (pSum.Length() < 0.5) {
            pSum.Zero();
            pSum.p[0] = 1; // just to have somethin
         }

         m_paNorm[dwIndex].Copy (&pSum);
      } // iX
   } // iY

   // done
}


/**************************************************************************************
CObjectRock::MoveReferencePointQuery - 
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
BOOL CObjectRock::MoveReferencePointQuery (DWORD dwIndex, PCPoint pp)
{
   PSPLINEMOVEP ps;
   DWORD dwDataSize;
   ps = gaRockMove;
   dwDataSize = sizeof(gaRockMove);

   if (dwIndex >= dwDataSize / sizeof(SPLINEMOVEP))
      return FALSE;

   // always at 0,0 in Rocks
   pp->Zero();
   return TRUE;
}

/**************************************************************************************
CObjectRock::MoveReferenceStringQuery -
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
BOOL CObjectRock::MoveReferenceStringQuery (DWORD dwIndex, PWSTR psz, DWORD dwSize, DWORD *pdwNeeded)
{
   PSPLINEMOVEP ps;
   DWORD dwDataSize;
   ps = gaRockMove;
   dwDataSize = sizeof(gaRockMove);
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



/****************************************************************************
RockPage
*/
BOOL RockPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCObjectRock pv = (PCObjectRock)pPage->m_pUserData;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         PCEscControl pControl;

         DoubleToControl (pPage, L"size", pv->m_dwSize);
         DoubleToControl (pPage, L"seed", pv->m_iSeed);

         if (pControl = pPage->ControlFind (L"stretchtofit"))
            pControl->AttribSetBOOL (Checked(), pv->m_fStretchToFit);

         DWORD i;
         WCHAR szTemp[64];
         for (i = 0; i < 4; i++) {
            swprintf (szTemp, L"noise%d", (int)i);

            pControl = pPage->ControlFind (szTemp);
            if (pControl)
               pControl->AttribSetInt (Pos(), (int)(pv->m_pNoise.p[i] * 100.0));
         } // i

         for (i = 0; i < 3; i++) {
            swprintf (szTemp, L"superquad%d", (int)i);

            pControl = pPage->ControlFind (szTemp);
            if (pControl)
               pControl->AttribSetInt (Pos(), (int)(pv->m_pSuperQuad.p[i] * 100.0));
         } // i

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
         if (!_wcsicmp(p->pControl->m_pszName, L"noise0"))
            pfMod = &pv->m_pNoise.p[0];
         else if (!_wcsicmp(p->pControl->m_pszName, L"noise1"))
            pfMod = &pv->m_pNoise.p[1];
         else if (!_wcsicmp(p->pControl->m_pszName, L"noise2"))
            pfMod = &pv->m_pNoise.p[2];
         else if (!_wcsicmp(p->pControl->m_pszName, L"noise3"))
            pfMod = &pv->m_pNoise.p[3];
         else if (!_wcsicmp(p->pControl->m_pszName, L"superquad0"))
            pfMod = &pv->m_pSuperQuad.p[0];
         else if (!_wcsicmp(p->pControl->m_pszName, L"superquad1"))
            pfMod = &pv->m_pSuperQuad.p[1];
         else if (!_wcsicmp(p->pControl->m_pszName, L"superquad2"))
            pfMod = &pv->m_pSuperQuad.p[2];
         else
            break;   // not one of these

         fp fVal;
         fVal = (fp) p->pControl->AttribGetInt (Pos()) / 100.0;

         pv->m_pWorld->ObjectAboutToChange (pv);
         *pfMod = fVal;
         pv->m_fDirty = TRUE;
         pv->m_pWorld->ObjectChanged (pv);
         return TRUE;
      }
      break;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;

         if (!p->pControl->m_pszName)
            break;
         PWSTR psz;
         psz = p->pControl->m_pszName;
         if (!_wcsicmp(psz, L"stretchtofit")) {
            pv->m_pWorld->ObjectAboutToChange (pv);
            pv->m_fStretchToFit = p->pControl->AttribGetBOOL (Checked());
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


         pv->m_dwSize = (DWORD) DoubleFromControl (pPage, L"size");
         pv->m_dwSize = max(pv->m_dwSize, 1);

         pv->m_iSeed = (int) DoubleFromControl (pPage, L"seed");

         pv->m_fDirty = TRUE;

         pv->m_pWorld->ObjectChanged (pv);
      }
      break;


   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Rock settings";
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}


/**********************************************************************************
CObjectRock::DialogShow - Causes the object to show a dialog box that allows
different attributes to be changed.

inputs
   DWORD             dwSuface - Surface that was clicked on. 0 if unknown.
   PCEscWindow       pWindow - Escarpment window to draw it in
returns
   BOOL - TRUE if user pressed "Back", FALSE if user pressed "Close"
*/
BOOL CObjectRock::DialogShow (DWORD dwSurface, PCEscWindow pWindow, PCPoint pClick)
{
   PWSTR pszRet;
   pszRet = pWindow->PageDialog (ghInstance, IDR_MMLROCK, RockPage, this);
   if (!pszRet)
      return FALSE;

   return !_wcsicmp(pszRet, Back());
}

/**********************************************************************************
CObjectRock::DialogQuery - Returns TRUE if the object supports a custom
dialog box, FALSE if it doesn't. The default object doesn't
*/
BOOL CObjectRock::DialogQuery (void)
{
   return TRUE;
}



