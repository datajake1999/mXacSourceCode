/************************************************************************
CObjectStairs.cpp - Draws a box.

begun 17/4/02 by Mike Rozak
Copyright 2002 Mike Rozak. All rights reserved
*/

#include <windows.h>
#include "escarpment.h"
#include "..\M3D.h"
#include "resource.h"

static PWSTR gszPoints = L"points%d-%d";

#define WALLTYPE        0x10001     // to create

// tread information
typedef struct {
   CPoint      aCorner[2][2]; // location of corner. [0 = left,1=right][0=front,1=back]
                              // this is the original one
   CPoint      aExtended[2][2];   // extended tread
   CPoint      aKickboard[2][2]; // kickboard location if have. [0 = left,1=right][0=front,1=back]
   BOOL        fKickboardValid;  // set to true if kickboard is valid. Not valid if treads so close that doesnt matter
   CPoint      aLip[2][2]; // lip location if have. [0 = left,1=right][0=front,1=back]
   BOOL        fFlatSection;  // TRUE if it's a flat section - dont extend here
} TREADINFO, *PTREADINFO;

typedef struct {
   PWSTR       pszName;    // name of the move point
   int         iX, iY; // -1 if for left/top, 1 for right/bottom
} SPLINEMOVEP, *PSPLINEMOVEP;



static SPLINEMOVEP   gaSplineMoveP[] = {
   L"Ground underneath top", 1, 0,
   L"Bottom", 0, -1,
   L"Top", 0, 1
};



/**********************************************************************************
QuadNormal - Given for points (in clockwise order) that form a quadrilateral,
this fills in the normal. (Normalized)

inputs
   PCPoint     pN - Filled in with normal. Normalized
   PCPoint     p1,p2,p3,p4 - Points of quad
returns
   BOOL - TRUE if succeded
*/
BOOL QuadNormal (PCPoint pN, PCPoint p1, PCPoint p2, PCPoint p3, PCPoint p4)
{
   CPoint pA, pB;
   fp fLenA, fLenB;
   pA.Subtract (p2, p1);
   pB.Subtract (p4, p1);
   fLenA = pA.Length();
   fLenB = pB.Length();
   if ((fLenA > EPSILON) && (fLenB > EPSILON)) {
      pN->CrossProd (&pB, &pA);
      pN->Normalize();
      if (pN->Length() > CLOSE)
         return TRUE;
   }

   // else next two
   pA.Subtract (p4, p3);
   pB.Subtract (p2, p3);
   fLenA = pA.Length();
   fLenB = pB.Length();
   if ((fLenA > EPSILON) && (fLenB > EPSILON)) {
      pN->CrossProd (&pB, &pA);
      pN->Normalize();
      if (pN->Length() > CLOSE)
         return TRUE;
   }

   // else none
   pN->Zero();
   pN->p[0] = 1;
   return FALSE;

}

/**********************************************************************************
QuadTextures - Given for points (in clockwise order) that form a quadrilateral,
this fills in the four TEXTPOINT5s, in the same order. p1 is assumed to be UL side.

inputs
   PTEXTPOINT5  pat - Array of 4 texture points
   PCPoint     p1,p2,p3,p4 - Points of quad
returns
   none
*/
void QuadTextures (PTEXTPOINT5 pat, PCPoint p1, PCPoint p2, PCPoint p3, PCPoint p4)
{
   fp fLenH, fLenV;
   CPoint pA, pB;
   pA.Subtract (p2, p1);
   pB.Subtract (p3, p4);
   fLenH = (pA.Length() + pB.Length()) / 2.0;
   pA.Subtract (p3, p2);
   pB.Subtract (p4, p1);
   fLenV = (pA.Length() + pB.Length()) / 2.0;

   pat[0].hv[0] = pat[0].hv[1] = 0;
   pat[1].hv[0] = fLenH;
   pat[1].hv[1] = 0;
   pat[2].hv[0] = fLenH;
   pat[2].hv[1] = fLenV;
   pat[3].hv[0] = 0;
   pat[3].hv[1] = fLenV;

   DWORD i;
   for (i = 0; i < 3; i++) {
      pat[0].xyz[i] = p1->p[i];
      pat[1].xyz[i] = p2->p[i];
      pat[2].xyz[i] = p3->p[i];
      pat[3].xyz[i] = p4->p[i];
   }
}

/**********************************************************************************
DrawBoard - Given four points (in clockwise order around the TOP of the board),
and a thickness, this draws the board.

inputs
   PCPoint     p1..p4 - four points. p1 is UL corner.
   fp      fThickness - THickness
   PCRenderSurface prs - Draw to this
returns
   none
*/
void DrawBoard (PCPoint p1, PCPoint p2, PCPoint p3, PCPoint p4, fp fThickness,
                PCRenderSurface prs)
{
   // find the normal for the top
   CPoint pNTop;
   if (!QuadNormal (&pNTop, p1, p2, p3, p4))
      return;

   // make this an offset for lower points
   CPoint pBelow[4];
   CPoint pSub;
   pSub.Copy (&pNTop);
   pSub.Scale (-fThickness);
   pBelow[0].Add (p1, &pSub);
   pBelow[1].Add (p2, &pSub);
   pBelow[2].Add (p3, &pSub);
   pBelow[3].Add (p4, &pSub);

   // allocate for the normals, vertices, textures, points, colors
   DWORD dwColor, dwNormals, dwPoints, dwTextures, dwVertices;
   PCPoint paNormals, paPoints;
   PVERTEX paVertices;
   PTEXTPOINT5 paTextures;
   dwColor = prs->DefColor();
   paNormals = prs->NewNormals (TRUE, &dwNormals, 6);
   paPoints = prs->NewPoints (&dwPoints, 8);
   paTextures = prs->NewTextures (&dwTextures, 6 * 4);
   paVertices = prs->NewVertices (&dwVertices, 6 * 4);
   if (!paPoints || !paVertices)
      return;

   // store the points away
   paPoints[0].Copy (p1);
   paPoints[1].Copy (p2);
   paPoints[2].Copy (p3);
   paPoints[3].Copy (p4);
   memcpy (paPoints + 4, pBelow, sizeof(pBelow));
   DWORD i;
   for (i = 0; i < 8; i++)
      paPoints[i].p[3] = 1;

   // loop through all the sides
   DWORD dwSide;
   for (dwSide = 0; dwSide < 6; dwSide++) {
      DWORD dwP[4];
      CPoint   pN;
      switch (dwSide) {
      case 0:  // top
         dwP[0] = 0;
         dwP[1] = 1;
         dwP[2] = 2;
         dwP[3] = 3;
         pN.Copy (&pNTop);
         break;
      case 1:  // bottom
         dwP[0] = 1+4;
         dwP[1] = 0+4;
         dwP[2] = 3+4;
         dwP[3] = 2+4;
         pN.Copy (&pNTop);
         pN.Scale(-1);
         break;
      case 2:  // left
         dwP[0] = 0;
         dwP[1] = 3;
         dwP[2] = 3+4;
         dwP[3] = 0+4;
         QuadNormal (&pN, &paPoints[dwP[0]], &paPoints[dwP[1]], &paPoints[dwP[2]], &paPoints[dwP[3]]);
         break;
      case 3:  // right
         dwP[0] = 2;
         dwP[1] = 1;
         dwP[2] = 1+4;
         dwP[3] = 2+4;
         QuadNormal (&pN, &paPoints[dwP[0]], &paPoints[dwP[1]], &paPoints[dwP[2]], &paPoints[dwP[3]]);
         break;
      case 4:  // front
         dwP[0] = 3;
         dwP[1] = 2;
         dwP[2] = 2+4;
         dwP[3] = 3+4;
         QuadNormal (&pN, &paPoints[dwP[0]], &paPoints[dwP[1]], &paPoints[dwP[2]], &paPoints[dwP[3]]);
         break;
      case 5:  // back
         dwP[0] = 1;
         dwP[1] = 0;
         dwP[2] = 0+4;
         dwP[3] = 1+4;
         QuadNormal (&pN, &paPoints[dwP[0]], &paPoints[dwP[1]], &paPoints[dwP[2]], &paPoints[dwP[3]]);
         break;
      }

      if (paNormals) {
         pN.p[3] = 1;
         paNormals[dwSide].Copy (&pN);
      }

      if (paTextures)
         QuadTextures (paTextures + (4 * dwSide), &paPoints[dwP[0]], &paPoints[dwP[1]], &paPoints[dwP[2]], &paPoints[dwP[3]]);
      
      DWORD k;
      for (k = 0; k < 4; k++) {
         paVertices[dwSide*4 + k].dwColor = dwColor;
         paVertices[dwSide*4 + k].dwNormal = dwNormals + dwSide;
         paVertices[dwSide*4 + k].dwPoint = dwPoints + dwP[k];
         paVertices[dwSide*4 + k].dwTexture = dwTextures + dwSide*4 + k;
      }

      // add the polygon
      prs->NewIDPart();
      prs->NewQuad (dwVertices + dwSide*4+0, dwVertices + dwSide*4+1,
         dwVertices + dwSide*4+2, dwVertices + dwSide*4+3);
   }

   // Need to transform all the textures
   if (paTextures)
      prs->ApplyTextureRotation (paTextures, 6*4);

}


/**********************************************************************************
CObjectStairs::Constructor and destructor

(DWORD)pParams is passed into CStairs::Init(). See definitions of parameters

 */
CObjectStairs::CObjectStairs (PVOID pParams, POSINFO pInfo)
{
   m_dwType = (DWORD)(size_t) pParams;
   m_OSINFO = *pInfo;

   
   WORD wH = HIWORD(m_dwType);

   // If it's a fence then deal with with differnt rendershow
   m_dwRenderShow = RENDERSHOW_STAIRS;
   m_pColumn = NULL; // start out empty
   m_lTREADINFO.Init (sizeof(TREADINFO));
   m_psCenter = m_psLeft = m_psRight = NULL;
   m_dwPathType = LOWORD(m_dwType);

   DWORD i;
   for (i = 0; i < 3; i++) {
      m_apBal[i] = NULL;
      m_afBalWant[i] = (i != 1);
      m_apWall[i] = NULL;
      m_afWallWant[i] = FALSE;
      m_apRail[i] = NULL;
      m_afRailWant[i] = (i != 1);
      m_adwRailShape[i] = NS_RECTANGLE;
   }
   m_pRailSize.h = .1;
   m_pRailSize.v = .2;
   m_pRailOffset.h = .1;
   m_pRailOffset.v = 0;
   m_fRailExtend = TRUE;

   fp afElev[NUMLEVELS], fHigher;
   GlobalFloorLevelsGet (WorldGet(m_OSINFO.dwRenderShard, NULL), NULL, afElev, &fHigher);

   // create the center spline
   m_dwRiserLevels = 1;
   m_fLevelHeight = max(fHigher, 2.0);
   m_fStairwellTwoPerLevel = TRUE;
   m_fSpiralRadius = 1.2;
   m_fSpiralRevolutions = 1;   // one complete revolutions
   m_fLandingTop = 0;   // none by default
   m_iLandingTopDir = 0; // straight ahead
   m_fStairwellLength = 3.0;
   m_fStairwellLanding = 1.5;
   m_fWidth = 1.0;
   memset (m_apBottomKick, 0, sizeof(m_apBottomKick));
   m_fTreadThickness = .04;   // 1.5" THICK
   m_fTreadExtendLeft = m_fTreadExtendRight = 0;
   m_fTreadExtendBack = .05;
   m_fTreadMinDepth = CM_TREADDEPTH;
   m_fKickboardIndent = .025; // 1" in
   m_fKickboard = TRUE;
   m_fTreadLip = FALSE;
   m_fTreadLipThickness = .15;
   m_fTreadLipDepth = .15;
   m_fTreadLipOffset = .01;
   if (m_dwPathType == SPATH_STAIRWELL2) {
      m_fStairwellWidth = 2* m_fWidth + m_fLevelHeight / 4.0 / CM_RISERHEIGHT * CM_TREADDEPTH;
      m_fStairwellLength = m_fStairwellWidth;
   }
   else
      m_fStairwellWidth = m_fStairwellLanding*2 + m_fLevelHeight / 2.0 / CM_RISERHEIGHT * CM_TREADDEPTH;

   m_fLRAuto = TRUE;
   m_fTotalRise = m_fLevelHeight;
   m_fWallIndent = 0;
   m_fWallToGround = TRUE;
   m_fWallBelow = .3;

   // if HIWORD(dwType) == 1 it's an entry staircase, which means it's only 1m tall,
   // or however high to ground level
   if (wH) {
      m_fTotalRise = max (.5, afElev[1]);
      m_fLandingTop = 1.5; // bit of a landing on top
      m_fWidth = 1.5;
      if (m_fTotalRise <= 1.0)
         memset (m_afBalWant, 0, sizeof(m_afBalWant));   // no balustrades
      m_afWallWant[0] = m_afWallWant[2] = TRUE;
      memset (m_afRailWant, 0 ,sizeof(m_afRailWant));
   }

   m_fLength = m_fTotalRise / CM_RISERHEIGHT * CM_TREADDEPTH - CM_TREADDEPTH;
   m_fRestLanding = 0;  // none by default
   m_fPathClockwise = TRUE;
   m_fSpiralPost = TRUE;
   m_fSpiralPostDiameter = .15;
   m_fSpiralPostHeight = 2.1;
   // risers height and calculate the treads
   m_fRiser = CM_RISERHEIGHT;

   CalcLRFromPathType (m_dwPathType);
   CalcTread ();

   // BUGFIX - So stair treads have wood oriented correctly
   fp afRot[2][2];
   memset (afRot, 0, sizeof(afRot));
   afRot[1][0] = 1;
   afRot[0][1] = -1;
   ObjectSurfaceAdd (1, RGB(0xff,0xff,0xe0), MATERIAL_PAINTGLOSS, L"Staircase tread",
                  &GTEXTURECODE_WoodTrim, &GTEXTURESUB_WoodTrim, NULL, (fp*) afRot);
   ObjectSurfaceAdd (2, RGB(0x40,0x40,0x40), MATERIAL_PAINTSEMIGLOSS, L"Staircase structure");
   ObjectSurfaceAdd (3, RGB(0xc0,0xc0,0xc0), MATERIAL_PAINTSEMIGLOSS, L"Staircase kickboard");
   ObjectSurfaceAdd (4, RGB(0x40,0x40,0x40), MATERIAL_PAINTSEMIGLOSS, L"Staircase tread lip");


}


CObjectStairs::~CObjectStairs (void)
{
   if (m_pColumn)
      delete m_pColumn;
   if (m_psCenter)
      delete m_psCenter;
   if (m_psLeft)
      delete m_psLeft;
   if (m_psRight)
      delete m_psRight;

   DWORD i;
   for (i = 0; i < 3; i++) {
      if (m_apBal[i]) {
         // dont do here: m_apBal[i]->ClaimClear();
         delete m_apBal[i];
      }
      m_apBal[i] = NULL;
      if (m_apWall[i]) {
         // dont do here: m_apWall[i]->ClaimClear();
         delete m_apWall[i];
      }
      m_apWall[i] = NULL;
      if (m_apRail[i])
         delete m_apRail[i];
      m_apRail[i] = NULL;
   }
}


/**********************************************************************************
CObjectStairs::Delete - Called to delete this object
*/
void CObjectStairs::Delete (void)
{
   delete this;
}

/**********************************************************************************
CObjectStairs::Render - Draws a test box.

inputs
   POBJECTRENDER     pr - Render information
*/
void CObjectStairs::Render (POBJECTRENDER pr, DWORD dwSubObject)
{
   // create the surface render object and draw
   // CRenderSurface rs;
   m_Renderrs.ClearAll();

   CMatrix mObject;
   m_Renderrs.Init (pr->pRS);
   ObjectMatrixGet (&mObject);
   m_Renderrs.Multiply (&mObject);

   // tread color
   m_Renderrs.SetDefMaterial (m_OSINFO.dwRenderShard, ObjectSurfaceFind (1), m_pWorld);

   // draw steps
   DWORD i;
   for (i = 0; i < m_lTREADINFO.Num(); i++) {
      PTREADINFO pti = (PTREADINFO) m_lTREADINFO.Get(i);

      DrawBoard (&pti->aExtended[0][1], &pti->aExtended[1][1],
         &pti->aExtended[1][0], &pti->aExtended[0][0], m_fTreadThickness, &m_Renderrs);

   }

   // draw the kickboard
   if (m_fKickboard) {
      m_Renderrs.SetDefMaterial (m_OSINFO.dwRenderShard, ObjectSurfaceFind (3), m_pWorld);
      for (i = 0; i < m_lTREADINFO.Num(); i++) {
         PTREADINFO pti = (PTREADINFO) m_lTREADINFO.Get(i);
         if (!pti->fKickboardValid)
            continue;
         DrawBoard (&pti->aKickboard[0][1], &pti->aKickboard[1][1],
            &pti->aKickboard[1][0], &pti->aKickboard[0][0], m_fTreadThickness, &m_Renderrs);

         if (!i) {
            // special case - If drawing the bottom tread, draw a kickboard down to
            // ground

            DrawBoard (&m_apBottomKick[0][1], &m_apBottomKick[1][1],
               &m_apBottomKick[1][0], &m_apBottomKick[0][0], m_fTreadThickness, &m_Renderrs);

         }
      }
   } // m_fKickboard

   // draw the tread lip
   if (m_fTreadLip) {
      m_Renderrs.SetDefMaterial (m_OSINFO.dwRenderShard, ObjectSurfaceFind (4), m_pWorld);
      for (i = 0; i < m_lTREADINFO.Num(); i++) {
         PTREADINFO pti = (PTREADINFO) m_lTREADINFO.Get(i);
         // dont traw lip if havne made an elevation change
         if (i && (pti[-1].aExtended[0][0].p[2] >= pti->aExtended[0][0].p[2]))
            continue;
         DrawBoard (&pti->aLip[0][1], &pti->aLip[1][1],
            &pti->aLip[1][0], &pti->aLip[0][0], m_fTreadLipThickness, &m_Renderrs);
      }
   }
   // structure
   m_Renderrs.SetDefMaterial (m_OSINFO.dwRenderShard, ObjectSurfaceFind (2), m_pWorld);

   // draw centeral column
   if (m_pColumn)
      m_pColumn->Render (pr, &m_Renderrs);

   // rails
   for (i = 0; i < 3; i++)
      if (m_apRail[i])
         m_apRail[i]->Render(pr, &m_Renderrs);

   // draw the balustrades and walls
   for (i = 0; i < 3; i++) {
      if (m_apBal[i])
         m_apBal[i]->Render (m_OSINFO.dwRenderShard, pr, &m_Renderrs);
      if (m_apWall[i])
         m_apWall[i]->Render (m_OSINFO.dwRenderShard, pr, &m_Renderrs);
   }

   m_Renderrs.Commit();
}


/**********************************************************************************
CObjectTarp::QueryBoundingBox - Standard API
*/
void CObjectStairs::QueryBoundingBox (PCPoint pCorner1, PCPoint pCorner2, DWORD dwSubObject)
{
   BOOL fSet = FALSE;

   // draw steps
   DWORD i, x, y;
   for (i = 0; i < m_lTREADINFO.Num(); i++) {
      PTREADINFO pti = (PTREADINFO) m_lTREADINFO.Get(i);

      for (x = 0; x < 2; x++) for (y = 0; y < 2; y++) {
         if (fSet) {
            pCorner1->Min (&pti->aExtended[x][y]);
            pCorner2->Max (&pti->aExtended[x][y]);
         }
         else {
            pCorner1->Copy (&pti->aExtended[x][y]);
            pCorner2->Copy (&pti->aExtended[x][y]);
            fSet = TRUE;
         }
      } // x,y 
   }

   // draw the kickboard
   if (m_fKickboard) {
      for (i = 0; i < m_lTREADINFO.Num(); i++) {
         PTREADINFO pti = (PTREADINFO) m_lTREADINFO.Get(i);
         if (!pti->fKickboardValid)
            continue;
         for (x = 0; x < 2; x++) for (y = 0; y < 2; y++) {
            if (fSet) {
               pCorner1->Min (&pti->aKickboard[x][y]);
               pCorner2->Max (&pti->aKickboard[x][y]);
            }
            else {
               pCorner1->Copy (&pti->aKickboard[x][y]);
               pCorner2->Copy (&pti->aKickboard[x][y]);
               fSet = TRUE;
            }
         } // x,y 


         if (!i) {
            // special case - If drawing the bottom tread, draw a kickboard down to
            // ground

            for (x = 0; x < 2; x++) for (y = 0; y < 2; y++) {
               if (fSet) {
                  pCorner1->Min (&m_apBottomKick[x][y]);
                  pCorner2->Max (&m_apBottomKick[x][y]);
               }
               else {
                  pCorner1->Copy (&m_apBottomKick[x][y]);
                  pCorner2->Copy (&m_apBottomKick[x][y]);
                  fSet = TRUE;
               }
            } // x,y 
         }
      }
   } // m_fKickboard

   // draw the tread lip
   if (m_fTreadLip) {
      for (i = 0; i < m_lTREADINFO.Num(); i++) {
         PTREADINFO pti = (PTREADINFO) m_lTREADINFO.Get(i);
         // dont traw lip if havne made an elevation change
         if (i && (pti[-1].aExtended[0][0].p[2] >= pti->aExtended[0][0].p[2]))
            continue;

         for (x = 0; x < 2; x++) for (y = 0; y < 2; y++) {
            if (fSet) {
               pCorner1->Min (&pti->aLip[x][y]);
               pCorner2->Max (&pti->aLip[x][y]);
            }
            else {
               pCorner1->Copy (&pti->aLip[x][y]);
               pCorner2->Copy (&pti->aLip[x][y]);
               fSet = TRUE;
            }
         } // x,y
      }
   }

   // if set, then include tread thickness
   if (fSet) {
      fp fThick = max(m_fTreadThickness, m_fTreadLipThickness);
      CPoint pDelta;
      pDelta.Zero();
      pDelta.p[0] = pDelta.p[1] = pDelta.p[2] = fThick;
      pCorner1->Subtract (&pDelta);
      pCorner2->Add (&pDelta);
   }
   
   // draw centeral column
   CPoint p1,p2;
   if (m_pColumn) {
      m_pColumn->QueryBoundingBox (&p1, &p2);
      if (fSet) {
         pCorner1->Min (&p1);
         pCorner2->Max (&p2);
      }
      else {
         pCorner1->Copy (&p1);
         pCorner2->Copy (&p2);
         fSet = TRUE;
      }
   }

   // rails
   for (i = 0; i < 3; i++)
      if (m_apRail[i]) {
         m_apRail[i]->QueryBoundingBox (&p1, &p2);
         if (fSet) {
            pCorner1->Min (&p1);
            pCorner2->Max (&p2);
         }
         else {
            pCorner1->Copy (&p1);
            pCorner2->Copy (&p2);
            fSet = TRUE;
         }
      }

   // draw the balustrades and walls
   for (i = 0; i < 3; i++) {
      if (m_apBal[i]) {
         m_apBal[i]->QueryBoundingBox (&p1, &p2);
         if (fSet) {
            pCorner1->Min (&p1);
            pCorner2->Max (&p2);
         }
         else {
            pCorner1->Copy (&p1);
            pCorner2->Copy (&p2);
            fSet = TRUE;
         }
      }
      if (m_apWall[i]) {
         m_apWall[i]->QueryBoundingBox (&p1, &p2);
         if (fSet) {
            pCorner1->Min (&p1);
            pCorner2->Max (&p2);
         }
         else {
            pCorner1->Copy (&p1);
            pCorner2->Copy (&p2);
            fSet = TRUE;
         }
      }
   }

#ifdef _DEBUG
   // test, make sure bounding box not too small
   // CPoint p1,p2;
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
CObjectStairs::Clone - Clones this

inputs
   none
returns
   PCObjectSocket - Clone
*/
PCObjectSocket CObjectStairs::Clone (void)
{
   return CloneStairs ();
}



/**********************************************************************************
CObjectStairs::CloneStairs - Clones this

inputs
   none
returns
   PCObjectSocket - Clone
*/
PCObjectStairs CObjectStairs::CloneStairs (void)
{
   PCObjectStairs pNew;

   pNew = new CObjectStairs(NULL, &m_OSINFO);

   // delete balustreads and colors created
   // BUGFIX - Moved before clonetemplate otherwise colors lost
   DWORD i;
   for (i = 0; i < 3; i++) {
      if (pNew->m_apBal[i]) {
         pNew->m_apBal[i]->ClaimClear();
         delete pNew->m_apBal[i];
      }
      pNew->m_apBal[i] = NULL;

      // same for the walls
      if (pNew->m_apWall[i]) {
         pNew->m_apWall[i]->ClaimClear();
         delete pNew->m_apWall[i];
      }
      pNew->m_apWall[i] = NULL;

      if (pNew->m_apRail)
         delete pNew->m_apRail[i];
      pNew->m_apRail[i] = NULL;
   }

   // clone template info
   CloneTemplate(pNew);


   // erase seom stuff from pnew
   if (pNew->m_pColumn)
      delete pNew->m_pColumn;
   if (pNew->m_psCenter)
      delete pNew->m_psCenter;
   if (pNew->m_psLeft)
      delete pNew->m_psLeft;
   if (pNew->m_psRight)
      delete pNew->m_psRight;
   pNew->m_pColumn = NULL;
   pNew->m_psCenter = pNew->m_psLeft = pNew->m_psRight = NULL;

   if (m_psCenter) {
      pNew->m_psCenter = new CSpline;
      if (pNew->m_psCenter)
         m_psCenter->CloneTo (pNew->m_psCenter);
   }
   if (m_psLeft) {
      pNew->m_psLeft = new CSpline;
      if (pNew->m_psLeft)
         m_psLeft->CloneTo (pNew->m_psLeft);
   }
   if (m_psRight) {
      pNew->m_psRight = new CSpline;
      if (pNew->m_psRight)
         m_psRight->CloneTo (pNew->m_psRight);
   }
   for (i = 0; i < 3; i++) {
      if (!m_apBal[i])
         continue;
      pNew->m_apBal[i] = new CBalustrade;
      if (!pNew->m_apBal[i])
         continue;
      pNew->m_apBal[i]->InitButDontCreate (m_OSINFO.dwRenderShard, 0, pNew);
      m_apBal[i]->CloneTo (pNew->m_apBal[i], pNew);
   }
   for (i = 0; i < 3; i++) {
      if (!m_apWall[i])
         continue;
      pNew->m_apWall[i] = new CDoubleSurface;
      if (!pNew->m_apWall[i])
         continue;
      pNew->m_apWall[i]->InitButDontCreate (WALLTYPE, pNew);
      m_apWall[i]->CloneTo (pNew->m_apWall[i], pNew);
   }
   for (i = 0; i < 3; i++) {
      if (!m_apRail[i])
         continue;
      pNew->m_apRail[i] = m_apRail[i]->Clone();
   }
   pNew->m_fLRAuto = m_fLRAuto;
   pNew->m_fWidth = m_fWidth;
   memcpy (pNew->m_apBottomKick, m_apBottomKick, sizeof(m_apBottomKick));
   pNew->m_fTreadThickness = m_fTreadThickness;
   pNew->m_fTreadExtendBack = m_fTreadExtendBack;
   pNew->m_fTreadExtendLeft = m_fTreadExtendLeft;
   pNew->m_fTreadExtendRight = m_fTreadExtendRight;
   pNew->m_fTreadMinDepth = m_fTreadMinDepth;
   pNew->m_fKickboardIndent = m_fKickboardIndent;
   pNew->m_fWallBelow = m_fWallBelow;
   pNew->m_fWallToGround = m_fWallToGround;
   pNew->m_fWallIndent = m_fWallIndent;
   memcpy (pNew->m_afRailWant, m_afRailWant, sizeof(m_afRailWant));
   memcpy (pNew->m_adwRailShape, m_adwRailShape, sizeof(m_adwRailShape));
   pNew->m_pRailSize = m_pRailSize;
   pNew->m_fRailExtend = m_fRailExtend;
   pNew->m_pRailOffset = m_pRailOffset;
   pNew->m_fKickboard = m_fKickboard;
   pNew->m_fTreadLip = m_fTreadLip;
   pNew->m_fTreadLipDepth = m_fTreadLipDepth;
   pNew->m_fTreadLipOffset = m_fTreadLipOffset;
   pNew->m_fTreadLipThickness = m_fTreadLipThickness;
   pNew->m_fPathClockwise = m_fPathClockwise;
   pNew->m_fRestLanding = m_fRestLanding;
   pNew->m_fLength = m_fLength;
   pNew->m_fTotalRise = m_fTotalRise;
   pNew->m_fRiser = m_fRiser;
   pNew->m_dwPathType = m_dwPathType;
   pNew->m_fStairwellWidth = m_fStairwellWidth;
   pNew->m_fStairwellLength = m_fStairwellLength;
   pNew->m_fStairwellLanding = m_fStairwellLanding;
   pNew->m_dwRiserLevels = m_dwRiserLevels;
   pNew->m_fLevelHeight = m_fLevelHeight;
   pNew->m_fLandingTop = m_fLandingTop;
   pNew->m_iLandingTopDir = m_iLandingTopDir;
   pNew->m_fStairwellTwoPerLevel = m_fStairwellTwoPerLevel;
   pNew->m_fSpiralRadius = m_fSpiralRadius;
   pNew->m_fSpiralRevolutions  = m_fSpiralRevolutions;
   pNew->m_fSpiralPost = m_fSpiralPost;
   pNew->m_fSpiralPostDiameter = m_fSpiralPostDiameter;
   pNew->m_fSpiralPostHeight = m_fSpiralPostHeight;

   pNew->m_lTREADINFO.Init (sizeof(TREADINFO), m_lTREADINFO.Get(0), m_lTREADINFO.Num());
   if (m_pColumn)
      pNew->m_pColumn = m_pColumn->Clone();




   return pNew;
}

static PWSTR gpszCenter = L"center";
static PWSTR gpszLeft = L"left";
static PWSTR gpszRight = L"right";
static PWSTR gpszColumn = L"column";
static PWSTR gpszLRAuto = L"LRAuto";
static PWSTR gpszWidth = L"Width";
static PWSTR gpszPathType = L"PathType";
static PWSTR gpszTotalRise = L"TotalRise";
static PWSTR gpszLength = L"Length";
static PWSTR gpszRestLanding = L"RestLanding";
static PWSTR gpszPathClockwise = L"PathClockwise";
static PWSTR gpszStairwellWidth = L"StairwellWidth";
static PWSTR gpszStairwellLanding = L"StairwellLanding";
static PWSTR gpszStairwellLength = L"StairwellLength";
static PWSTR gpszRiserLevels = L"RiserLevels";
static PWSTR gpszLevelHeight = L"LevelHeight";
static PWSTR gpszStairwellTwoPerLevel = L"StairwellTwoPerLevel";
static PWSTR gpszSpiralRadius = L"SpiralRadius";
static PWSTR gpszSpiralRevolutions = L"SpiralReviolutions";
static PWSTR gpszLandingTop = L"LandingTop";
static PWSTR gpszLandingTopDir = L"LandingTopDir";
static PWSTR gpszSpiralPost = L"SprialPost";
static PWSTR gpszSpiralPostDiameter = L"SpiralPostDiameter";
static PWSTR gpszSpiralPostHeight = L"SpiralPostHeight";
static PWSTR gpszTreadThickness = L"TreadThickness";
static PWSTR gpszRiser = L"Riser";
static PWSTR gpszTreadExtendLeft = L"TreadExtendLeft";
static PWSTR gpszTreadExtendRight = L"TreadExtendRight";
static PWSTR gpszTreadExtendBack = L"TreadExtendBack";
static PWSTR gpszTreadMinDepth = L"TreadMinDepth";
static PWSTR gpszKickboardIndent = L"KickboardIndent";
static PWSTR gpszKickboard = L"Kickboard";
static PWSTR gpszTreadLip = L"TreadLip";
static PWSTR gpszTreadLipThickness = L"TreadLipThickness";
static PWSTR gpszTreadLipDepth = L"TreadLipDepth";
static PWSTR gpszTreadLipOffset = L"TreadLipOffset";
static PWSTR gpszWallIndent = L"WallIndent";
static PWSTR gpszWallToGround = L"WallToGround";
static PWSTR gpszWallBelow = L"WallBelow";
static PWSTR gpszRailSize = L"RailSize";
static PWSTR gpszRailOffset = L"RailOffset";
static PWSTR gpszRailExtend = L"RailExtend";

PCMMLNode2 CObjectStairs::MMLTo (void)
{
   // call into the template first
   PCMMLNode2 pNode = MMLToTemplate();
   if (!pNode)
      return NULL;

   // member variables go here
   PCMMLNode2 pSub;
   DWORD i;
   WCHAR szTemp[32];
   for (i = 0; i < 3; i++) {
      if (!m_apBal[i])
         continue;

      swprintf (szTemp, L"BalObject%d", (int) i);
      pSub = m_apBal[i]->MMLTo();
      if (!pSub)
         continue;
      pSub->NameSet (szTemp);
      pNode->ContentAdd (pSub);
   }
   for (i = 0; i < 3; i++) {
      if (!m_apWall[i])
         continue;

      swprintf (szTemp, L"WallObject%d", (int) i);
      pSub = m_apWall[i]->MMLTo();
      if (!pSub)
         continue;
      pSub->NameSet (szTemp);
      pNode->ContentAdd (pSub);
   }
   for (i = 0; i < 3; i++) {
      if (!m_apRail[i])
         continue;

      swprintf (szTemp, L"RailObject%d", (int) i);
      pSub = m_apRail[i]->MMLTo();
      if (!pSub)
         continue;
      pSub->NameSet (szTemp);
      pNode->ContentAdd (pSub);
   }
   
   // center
   if (m_psCenter) {
      pSub = m_psCenter->MMLTo ();
      if (pSub) {
         pSub->NameSet (gpszCenter);
         pNode->ContentAdd (pSub);
      }
   }

   // left
   if (m_psLeft) {
      pSub = m_psLeft->MMLTo ();
      if (pSub) {
         pSub->NameSet (gpszLeft);
         pNode->ContentAdd (pSub);
      }
   }

   // Right
   if (m_psRight) {
      pSub = m_psRight->MMLTo ();
      if (pSub) {
         pSub->NameSet (gpszRight);
         pNode->ContentAdd (pSub);
      }
   }

   // column
   if (m_pColumn) {
      pSub = m_pColumn->MMLTo ();
      if (pSub) {
         pSub->NameSet (gpszColumn);
         pNode->ContentAdd (pSub);
      }
   }

   // Not saved - CListFixed  m_lTREADINFO;  // list of tread info structures
   // Not saved - m_apBottomKick - because calculated automatically

   MMLValueSet (pNode, gpszLRAuto, (int) m_fLRAuto);
   MMLValueSet (pNode, gpszWidth, m_fWidth);
   MMLValueSet (pNode, gpszPathType, (int) m_dwPathType);
   MMLValueSet (pNode, gpszTotalRise, m_fTotalRise);
   MMLValueSet (pNode, gpszLength, m_fLength);
   MMLValueSet (pNode, gpszRestLanding, m_fRestLanding);
   MMLValueSet (pNode, gpszPathClockwise, (int) m_fPathClockwise);
   MMLValueSet (pNode, gpszStairwellWidth, m_fStairwellWidth);
   MMLValueSet (pNode, gpszStairwellLanding, m_fStairwellLanding);
   MMLValueSet (pNode, gpszStairwellLength, m_fStairwellLength);
   MMLValueSet (pNode, gpszRiserLevels, (int) m_dwRiserLevels);
   MMLValueSet (pNode, gpszLevelHeight, m_fLevelHeight);
   MMLValueSet (pNode, gpszStairwellTwoPerLevel, (int) m_fStairwellTwoPerLevel);
   MMLValueSet (pNode, gpszSpiralRadius, m_fSpiralRadius);
   MMLValueSet (pNode, gpszSpiralRevolutions, m_fSpiralRevolutions);
   MMLValueSet (pNode, gpszLandingTop, m_fLandingTop);
   MMLValueSet (pNode, gpszLandingTopDir, (int) m_iLandingTopDir);
   MMLValueSet (pNode, gpszSpiralPost, (int) m_fSpiralPost);
   MMLValueSet (pNode, gpszSpiralPostDiameter, m_fSpiralPostDiameter);
   MMLValueSet (pNode, gpszSpiralPostHeight, m_fSpiralPostHeight);
   MMLValueSet (pNode, gpszTreadThickness, m_fTreadThickness);
   MMLValueSet (pNode, gpszRiser, m_fRiser);
   MMLValueSet (pNode, gpszTreadExtendLeft, m_fTreadExtendLeft);
   MMLValueSet (pNode, gpszTreadExtendRight, m_fTreadExtendRight);
   MMLValueSet (pNode, gpszTreadExtendBack, m_fTreadExtendBack);
   MMLValueSet (pNode, gpszTreadMinDepth, m_fTreadMinDepth);
   MMLValueSet (pNode, gpszKickboardIndent, m_fKickboardIndent);
   MMLValueSet (pNode, gpszKickboard, (int) m_fKickboard);
   MMLValueSet (pNode, gpszTreadLip, (int) m_fTreadLip);
   MMLValueSet (pNode, gpszTreadLipThickness, m_fTreadLipThickness);
   MMLValueSet (pNode, gpszTreadLipDepth, m_fTreadLipDepth);
   MMLValueSet (pNode, gpszTreadLipOffset, m_fTreadLipOffset);
   MMLValueSet (pNode, gpszWallIndent, m_fWallIndent);
   MMLValueSet (pNode, gpszWallToGround, (int) m_fWallToGround);
   MMLValueSet (pNode, gpszWallBelow, m_fWallBelow);
   MMLValueSet (pNode, gpszRailSize, &m_pRailSize);
   MMLValueSet (pNode, gpszRailOffset, &m_pRailOffset);
   MMLValueSet (pNode, gpszRailExtend, (int) m_fRailExtend);


   for (i = 0; i < 3; i++) {
      swprintf (szTemp, L"BalWant%d", (int) i);
      MMLValueSet (pNode, szTemp, (int) m_afBalWant[i]);

      swprintf (szTemp, L"WallWant%d", (int) i);
      MMLValueSet (pNode, szTemp, (int) m_afWallWant[i]);

      swprintf (szTemp, L"RailWant%d", (int) i);
      MMLValueSet (pNode, szTemp, (int) m_afRailWant[i]);

      swprintf (szTemp, L"RailShape%d", (int) i);
      MMLValueSet (pNode, szTemp, (int) m_adwRailShape[i]);
   }

   return pNode;
}

BOOL CObjectStairs::MMLFrom (PCMMLNode2 pNode)
{
   // delete old balustrade objects and stuff
   DWORD i;
   for (i = 0; i < 3; i++) {
      if (m_apBal[i]) {
         m_apBal[i]->ClaimClear();
         delete m_apBal[i];
      }
      m_apBal[i] = NULL;

      // same for the walls
      if (m_apWall[i]) {
         m_apWall[i]->ClaimClear();
         delete m_apWall[i];
      }
      m_apWall[i] = NULL;

      if (m_apRail[i])
         delete m_apRail[i];
      m_apRail[i] = NULL;
   }
   // erase seom stuff from pnew
   if (m_pColumn)
      delete m_pColumn;
   if (m_psCenter)
      delete m_psCenter;
   if (m_psLeft)
      delete m_psLeft;
   if (m_psRight)
      delete m_psRight;
   m_pColumn = NULL;
   m_psCenter = m_psLeft = m_psRight = NULL;

   // call into the template first
   if (!MMLFromTemplate (pNode))
      return FALSE;

   // member variables go here
   PCMMLNode2 pSub;
   PWSTR psz;
   WCHAR szTemp[32];
   for (i = 0; i < pNode->ContentNum(); i++) {
      pSub = NULL;
      pNode->ContentEnum (i, &psz, &pSub);
      if (!pSub)
         continue;
      psz = pSub->NameGet();
      if (!psz)
         continue;

      // see if it's balustrades
      PWSTR pszBalObject = L"BalObject";
      DWORD dwLenBalObject = (DWORD)wcslen(pszBalObject);
      PWSTR pszWallObject = L"WallObject";
      DWORD dwLenWallObject = (DWORD)wcslen(pszWallObject);
      PWSTR pszRailObject = L"RailObject";
      DWORD dwLenRailObject = (DWORD)wcslen(pszRailObject);
      if (!wcsncmp(psz, pszBalObject, dwLenBalObject)) {
         DWORD dw = _wtoi(psz + dwLenBalObject);
         dw = min(2,dw);

         PCBalustrade pds;
         pds = new CBalustrade;
         if (!pds)
            continue;

         // Initializing the object, but then not creating any surfaces to register
         pds->InitButDontCreate (m_OSINFO.dwRenderShard, 0 /*dwType*/, this);
         pds->MMLFrom (pSub);
         m_apBal[dw] = pds;
      }
      else if (!wcsncmp(psz, pszWallObject, dwLenWallObject)) {
         DWORD dw = _wtoi(psz + dwLenWallObject);
         dw = min(2,dw);

         PCDoubleSurface pds;
         pds = new CDoubleSurface;
         if (!pds)
            continue;

         // Initializing the object, but then not creating any surfaces to register
         pds->InitButDontCreate (WALLTYPE /*dwType*/, this);
         pds->MMLFrom (pSub);
         m_apWall[dw] = pds;
      }
      else if (!wcsncmp(psz, pszRailObject, dwLenRailObject)) {
         DWORD dw = _wtoi(psz + dwLenRailObject);
         dw = min(2,dw);

         m_apRail[dw] = new CNoodle;
         if (!m_apRail[dw])
            continue;

         m_apRail[dw]->MMLFrom (pSub);
      }
      else if (!_wcsicmp(psz, gpszCenter)) {
         m_psCenter = new CSpline;
         if (m_psCenter)
            m_psCenter->MMLFrom (pSub);
      }
      else if (!_wcsicmp(psz, gpszLeft)) {
         m_psLeft = new CSpline;
         if (m_psLeft)
            m_psLeft->MMLFrom (pSub);
      }
      else if (!_wcsicmp(psz, gpszRight)) {
         m_psRight = new CSpline;
         if (m_psRight)
            m_psRight->MMLFrom (pSub);
      }
      else if (!_wcsicmp(psz, gpszColumn)) {
         m_pColumn = new CColumn;
         if (m_pColumn)
            m_pColumn->MMLFrom (pSub);
      }
   }


   m_fLRAuto = (BOOL) MMLValueGetInt (pNode, gpszLRAuto, TRUE);
   m_fWidth = MMLValueGetDouble (pNode, gpszWidth, 1.0);
   m_dwPathType = MMLValueGetInt (pNode, gpszPathType, SPATH_STRAIGHT);
   m_fTotalRise = MMLValueGetDouble (pNode, gpszTotalRise, 1);
   m_fLength = MMLValueGetDouble (pNode, gpszLength, 2);
   m_fRestLanding = MMLValueGetDouble (pNode, gpszRestLanding, 0);
   m_fPathClockwise = (BOOL) MMLValueGetInt (pNode, gpszPathClockwise, TRUE);
   m_fStairwellWidth = MMLValueGetDouble (pNode, gpszStairwellWidth, 2);
   m_fStairwellLanding = MMLValueGetDouble (pNode, gpszStairwellLanding, 1);
   m_fStairwellLength = MMLValueGetDouble (pNode, gpszStairwellLength, 3);
   m_dwRiserLevels = (DWORD) MMLValueGetInt (pNode, gpszRiserLevels, 1);
   m_fLevelHeight = MMLValueGetDouble (pNode, gpszLevelHeight, 2);
   m_fStairwellTwoPerLevel = (BOOL) MMLValueGetInt (pNode, gpszStairwellTwoPerLevel, TRUE);
   m_fSpiralRadius = MMLValueGetDouble (pNode, gpszSpiralRadius, 2);
   m_fSpiralRevolutions = MMLValueGetDouble (pNode, gpszSpiralRevolutions, 1);
   m_fLandingTop = MMLValueGetDouble (pNode, gpszLandingTop, 0);
   m_iLandingTopDir = MMLValueGetInt (pNode, gpszLandingTopDir, (int) 0);
   m_fSpiralPost = (BOOL) MMLValueGetInt (pNode, gpszSpiralPost, TRUE);
   m_fSpiralPostDiameter = MMLValueGetDouble (pNode, gpszSpiralPostDiameter, .1);
   m_fSpiralPostHeight = MMLValueGetDouble (pNode, gpszSpiralPostHeight, 0);
   m_fTreadThickness = MMLValueGetDouble (pNode, gpszTreadThickness, .2);
   m_fRiser = MMLValueGetDouble (pNode, gpszRiser, .15);
   m_fTreadExtendLeft = MMLValueGetDouble (pNode, gpszTreadExtendLeft, 0);
   m_fTreadExtendRight = MMLValueGetDouble (pNode, gpszTreadExtendRight, 0);
   m_fTreadExtendBack = MMLValueGetDouble (pNode, gpszTreadExtendBack, 0);
   m_fTreadMinDepth = MMLValueGetDouble (pNode, gpszTreadMinDepth, 0);
   m_fKickboardIndent = MMLValueGetDouble (pNode, gpszKickboardIndent, 0);
   m_fKickboard = (BOOL) MMLValueGetInt (pNode, gpszKickboard, FALSE);
   m_fTreadLip = (BOOL) MMLValueGetInt (pNode, gpszTreadLip, FALSE);
   m_fTreadLipThickness = MMLValueGetDouble (pNode, gpszTreadLipThickness, .1);
   m_fTreadLipDepth = MMLValueGetDouble (pNode, gpszTreadLipDepth, .1);
   m_fTreadLipOffset = MMLValueGetDouble (pNode, gpszTreadLipOffset, 0);
   m_fWallIndent = MMLValueGetDouble (pNode, gpszWallIndent, 0);
   m_fWallToGround = (BOOL) MMLValueGetInt (pNode, gpszWallToGround, TRUE);
   m_fWallBelow = MMLValueGetDouble (pNode, gpszWallBelow, .3);
   TEXTUREPOINT tp;
   tp.h = tp.v = .1;
   MMLValueGetTEXTUREPOINT (pNode, gpszRailSize, &m_pRailSize, &tp);
   MMLValueGetTEXTUREPOINT (pNode, gpszRailOffset, &m_pRailOffset, &tp);
   m_fRailExtend = (BOOL) MMLValueGetInt (pNode, gpszRailExtend, 0);

   for (i = 0; i < 3; i++) {
      swprintf (szTemp, L"BalWant%d", (int) i);
      m_afBalWant[i] = (BOOL) MMLValueGetInt (pNode, szTemp, FALSE);

      swprintf (szTemp, L"WallWant%d", (int) i);
      m_afWallWant[i] = (BOOL) MMLValueGetInt (pNode, szTemp, FALSE);

      swprintf (szTemp, L"RailWant%d", (int) i);
      m_afRailWant[i] = (BOOL) MMLValueGetInt (pNode, szTemp, FALSE);

      swprintf (szTemp, L"RailShape%d", (int) i);
      m_adwRailShape[i] = (DWORD) MMLValueGetInt (pNode, szTemp, NS_CIRCLE);
   }

   // Call CalcTreadInfo() to recalculate the treads
   CalcTread(FALSE);
   return TRUE;
}


/*************************************************************************************
CObjectStairs::ControlPointQuery - Called to query the information about a control
point identified by dwID.

inputs
   DWORD       dwID - ID
   POSCONTROL  pInfo - Filled in with the infomration
returns
   BOOL - TRUE if successful
*/
BOOL CObjectStairs::ControlPointQuery (DWORD dwID, POSCONTROL pInfo)
{
   fp fKnobSize = .2;

   BOOL fCenter;
   switch (m_dwPathType) {
   case SPATH_CUSTOMCENTER:
      fCenter = TRUE;;
      break;
   case SPATH_CUSTOMLR:
      fCenter = FALSE;
      break;
   default:
      return FALSE;
   }

   DWORD dwNum;
   dwNum = m_psCenter->OrigNumPointsGet();
   if (dwID >= dwNum * (fCenter ? 1 : 2))
      return FALSE;

   // get the left, right, and center values
   PCSpline ps;
   if (fCenter)
      ps = m_psCenter;
   else
      ps = ((dwID < dwNum) ? m_psLeft : m_psRight);

   memset (pInfo,0, sizeof(*pInfo));

   pInfo->dwID = dwID;
   //pInfo->dwFreedom = 0;   // any direction
   pInfo->dwStyle = CPSTYLE_CUBE;
   pInfo->fSize = fKnobSize;
   pInfo->cColor = RGB(0x00,0xff,0xff);

   // get it
   ps->OrigPointGet (dwID % dwNum, &pInfo->pLocation);
   TEXTUREPOINT tp;
   ps->OrigTextureGet (dwID % dwNum, &tp);
   pInfo->pLocation.p[2] = tp.h;

   // name
   wcscpy (pInfo->szName, fCenter ? L"Staircase center" :
      ((dwID < dwNum) ? L"Staircase left" : L"Staircase right"));

   return TRUE;
}

/*************************************************************************************
CObjectStairs::ControlPointSet - Called to change the valud of a control point.

inputs
   DWORD       dwID - ID
   PCPoint     pVal - Contains the new location, in object coordinates
returns
   BOOL - TRUE if successful
*/
BOOL CObjectStairs::ControlPointSet (DWORD dwID, PCPoint pVal, PCPoint pViewer)
{
   BOOL fCenter;
   switch (m_dwPathType) {
   case SPATH_CUSTOMCENTER:
      fCenter = TRUE;;
      break;
   case SPATH_CUSTOMLR:
      fCenter = FALSE;
      break;
   default:
      return FALSE;
   }

   DWORD dwNum;
   dwNum = m_psCenter->OrigNumPointsGet();
   if (dwID >= dwNum * (fCenter ? 1 : 2))
      return FALSE;

   // which index to use
   DWORD dwIndex;
   if (fCenter)
      dwIndex = 1;
   else
      dwIndex = (dwID < dwNum) ? 0 : 2;
   DWORD dwc;
   dwc = dwID % dwNum;

   // load in all the spline information
   // allocate enough memory so can do the calculations
   CMem  amemPoints[3];
   CMem  amemText[3];
   CMem  memSegCurve;
   PCPoint paPoints[3];
   PTEXTUREPOINT paText[3];
   DWORD dwOrig;
   dwOrig = m_psCenter->OrigNumPointsGet();
   DWORD k;
   for (k = 0; k < 3; k++) {
      if (!amemPoints[k].Required ((dwOrig+1) * sizeof(CPoint)))
         return TRUE;
      if (!amemText[k].Required ((dwOrig+1) * sizeof(TEXTUREPOINT)))
         return TRUE;
      paPoints[k] = (PCPoint) amemPoints[k].p;
      paText[k] = (PTEXTUREPOINT) amemText[k].p;
   }
   if (!memSegCurve.Required ((dwOrig+1) * sizeof(DWORD)))
      return TRUE;

   // load it in
   DWORD *padw;
   PCSpline ps;
   padw = (DWORD*) memSegCurve.p;
   DWORD i;
   for (i = 0; i < dwOrig; i++) {
      m_psCenter->OrigSegCurveGet (i, padw + i);

      for (k = 0; k < 3; k++) {
         ps = ((k == 0) ? m_psLeft : ((k==1) ? m_psCenter : m_psRight) );
         ps->OrigPointGet (i, paPoints[k] + i);
         ps->OrigTextureGet (i, paText[k] + i);
      }
   }
   DWORD dwMinDivide, dwMaxDivide;
   BOOL fLooped;
   fp fDetail;
   m_psCenter->DivideGet (&dwMinDivide, &dwMaxDivide, &fDetail);
   fLooped = m_psCenter->LoopedGet();

   // remember new point
   CPoint pNew;
   fp fZ;
   fZ = pVal->p[2];
   pNew.Copy (pVal);
   pNew.p[2] = 0;

   // if the height is less than the last one then don't get less
   if (dwc && (fZ < (paText[dwIndex])[dwc-1].h))
      fZ = (paText[dwIndex])[dwc-1].h; 
   if ((dwc+1 < dwNum) && (fZ > (paText[dwIndex])[dwc+1].h))
      fZ = (paText[dwIndex])[dwc+1].h; 

   // move points
   (paPoints[dwIndex])[dwc].Copy (&pNew);
   (paText[dwIndex])[dwc].h = fZ;
   if (!fCenter) {
      // move left and right up/down together
      (paText[0])[dwc].h = fZ;
      (paText[2])[dwc].h = fZ;
   }

   // changed
   m_pWorld->ObjectAboutToChange (this);

   // splines
   for (k = 0; k < 3; k++) {
      ps = ((k == 0) ? m_psLeft : ((k==1) ? m_psCenter : m_psRight) );

      ps->Init (fLooped, dwOrig, paPoints[k], paText[k], padw, dwMinDivide, dwMaxDivide, fDetail);
      // always use 3 so that if custom divide for curvature
   }

   CalcLRFromPathType (m_dwPathType);
   CalcTread ();

   m_pWorld->ObjectChanged (this);
   
   return TRUE;
}

/*************************************************************************************
CObjectStairs::ControlPointEnum - Called to enumerate a list of control point IDs
into the list.

inputs
   PCListFixed       plDWORD - Should be filled in with control points
returns
   none
*/
void CObjectStairs::ControlPointEnum (PCListFixed plDWORD)
{
   DWORD i;
   DWORD dwPoints;

   switch (m_dwPathType) {
   case SPATH_CUSTOMCENTER:
      dwPoints = m_psCenter->OrigNumPointsGet();
      break;
   case SPATH_CUSTOMLR:
      dwPoints = m_psCenter->OrigNumPointsGet() * 2;
      break;
   default:
      return;
   }

   for (i = 0; i < dwPoints; i++)
      plDWORD->Add (&i);
}

/**********************************************************************************
CObjectStairs::DialogQuery - Returns TRUE if the object supports a custom
dialog box, FALSE if it doesn't. The default object doesn't
*/
BOOL CObjectStairs::DialogQuery (void)
{
   return TRUE;
}
/********************************************************************************
GenerateThreeDFromSpline - Given a spline on a surface, this sets a threeD
control with the spline.

inputs
   PWSTR       pszControl - Control name.
   PCEscPage   pPage - Page
   PCSpline    pLeft - Left Spline to draw
   PCSpline    pRight - Right spline to draw
   DWORD       dwUse - If 0 it's for adding/remove splines, else if 1 it's for cycling curves
returns
   BOOl - TRUE if success

NOTE: The ID's are such:
   LOBYTE = x
   3rd lowest byte = 1 for edge, 2 for point
*/
static BOOL GenerateThreeDFromSpline (PWSTR pszControl, PCEscPage pPage, PCSpline pLeft, PCSpline pRight,
                                      DWORD dwUse)
{
   PCEscControl pControl = pPage->ControlFind (pszControl);
   if (!pControl)
      return FALSE;

   // figure out center
   CPoint pMin, pMax, pt;
   DWORD i,j;
   TEXTUREPOINT tp;
   DWORD dwNum;
   dwNum = pLeft->OrigNumPointsGet();
   for (i = 0; i < dwNum*2; i++) {
      if (i >= dwNum) {
         pRight->OrigPointGet(i % dwNum, &pt);
         pRight->OrigTextureGet (i % dwNum, &tp);
      }
      else {
         pLeft->OrigPointGet(i % dwNum, &pt);
         pLeft->OrigTextureGet (i % dwNum, &tp);
      }
      pt.p[2] = tp.h;
      pt.p[3] = 1;

      if (!i) {
         pMin.Copy (&pt);
         pMax.Copy (&pt);
         continue;
      }

      for (j = 0; j < 3; j++) {
         pMin.p[j] = min(pMin.p[j], pt.p[j]);
         pMax.p[j] = max(pMax.p[j], pt.p[j]);
      }
   }

   // and center
   CPoint pCenter;
   pCenter.Add (&pMin, &pMax);
   pCenter.Scale (.5);

   // figure out the maximum distance
   fp fMax;
   fMax = max(pMax.p[0] - pMin.p[0], pMax.p[2] - pMin.p[2]);
   fMax = max(fMax, pMax.p[1] - pMin.p[1]);
   // NOTE: assume p[1] == 0
   fMax = max(0.001, fMax);
   fMax /= 10;  // so is larger

   // when draw points, get the point, subtract the center, and divide by fMax

   // use gmemtemp
   MemZero (&gMemTemp);
   MemCat (&gMemTemp, L"<rotatex val=-90/><backculloff/>");


   // draw the bits
   DWORD x;
   WCHAR szTemp[128];
   for (x = 0; x < dwNum; x++) {
      // if it's the last point and we're not looped then quit
      if ((x+1 >= dwNum) && !(pLeft->LoopedGet()))
         continue;

      DWORD h, v;
      CPoint ap[2][2];
      for (h = 0; h < 2; h++) for (v = 0; v < 2; v++) {
         PCSpline ps = h ? pRight : pLeft;
         ps->OrigPointGet ((x + v) % dwNum, &ap[h][v]);
         ps->OrigTextureGet ((x+v)%dwNum, &tp);
         ap[h][v].p[2] = tp.h;
         ap[h][v].p[3] = 1.0;

         // center and scale
         ap[h][v].Subtract (&pCenter);
         ap[h][v].Scale (1.0 / fMax);
      }


      // color
      if (dwUse == 0)
         MemCat (&gMemTemp, L"<colordefault color=#c0c0c0/>");
      else {
         DWORD dwSeg;
         pLeft->OrigSegCurveGet (x, &dwSeg);
         switch (dwSeg) {
         case SEGCURVE_CUBIC:
            MemCat (&gMemTemp, L"<colordefault color=#8080ff/>");
            break;
         case SEGCURVE_CIRCLENEXT:
            MemCat (&gMemTemp, L"<colordefault color=#ffc0c0/>");
            break;
         case SEGCURVE_CIRCLEPREV:
            MemCat (&gMemTemp, L"<colordefault color=#c04040/>");
            break;
         case SEGCURVE_ELLIPSENEXT:
            MemCat (&gMemTemp, L"<colordefault color=#40c040/>");
            break;
         case SEGCURVE_ELLIPSEPREV:
            MemCat (&gMemTemp, L"<colordefault color=#004000/>");
            break;
         default:
         case SEGCURVE_LINEAR:
            MemCat (&gMemTemp, L"<colordefault color=#c0c0c0/>");
            break;
         }
      }

      // set the ID
      MemCat (&gMemTemp, L"<id val=");
      MemCat (&gMemTemp, (int)((1 << 16) | x));
      MemCat (&gMemTemp, L"/>");

      MemCat (&gMemTemp, L"<shapepolygon");

      for (i = 0; i < 4; i++) {
         CPoint pTemp;
         switch (i) {
         case 0: // UL
            pTemp.Copy (&ap[0][1]);
            break;
         case 1: // UR
            pTemp.Copy (&ap[1][1]);
            break;
         case 2: // LR
            pTemp.Copy (&ap[1][0]);
            break;
         case 3: // LL
            pTemp.Copy (&ap[0][0]);
            break;
         }

         swprintf (szTemp, L" p%d=%g,%g,%g",
            (int) i+1, (double)pTemp.p[0], (double)pTemp.p[1], (double)pTemp.p[2]);
         MemCat (&gMemTemp, szTemp);
      }
      MemCat (&gMemTemp, L"/>");

      // line
      if ((dwUse == 0) && (dwNum > 2)) {
         MemCat (&gMemTemp, L"<colordefault color=#ff0000/>");
         // set the ID
         MemCat (&gMemTemp, L"<id val=");
         MemCat (&gMemTemp, (int)((2 << 16) | x));
         MemCat (&gMemTemp, L"/>");

         MemCat (&gMemTemp, L"<shapearrow tip=false width=.2");

         WCHAR szTemp[128];
         swprintf (szTemp, L" p1=%g,%g,%g p2=%g,%g,%g/>",
            (double)ap[0][0].p[0], (double)ap[0][0].p[1], (double)ap[0][0].p[2], (double)ap[1][0].p[0], (double)ap[1][0].p[1], (double)ap[1][0].p[2]);
         MemCat (&gMemTemp, szTemp);
      }
   }

   // set the threeD control
   ESCMTHREEDCHANGE tc;
   memset (&tc, 0, sizeof(tc));
   tc.pszMML = (PWSTR) gMemTemp.p;
   pControl->Message (ESCM_THREEDCHANGE, &tc);

   return TRUE;
}

/* StairsPathPage
*/
BOOL StairsPathPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCObjectStairs pv = (PCObjectStairs) pPage->m_pUserData;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         PCEscControl pControl;
         WCHAR szTemp[32];
         swprintf (szTemp, L"%d", (int) pv->m_dwPathType);
         ESCMLISTBOXSELECTSTRING lss;
         memset (&lss, 0, sizeof(lss));
         lss.fExact = TRUE;
         lss.iStart = -1;
         lss.psz = szTemp;
         pControl = pPage->ControlFind (L"path");
         if (pControl)
            pControl->Message (ESCM_LISTBOXSELECTSTRING, &lss);

         // set all the parameters
         DoubleToControl (pPage, L"riserlevel", (fp) pv->m_dwRiserLevels);
         MeasureToString (pPage, L"levelheight", pv->m_fLevelHeight);
         MeasureToString (pPage, L"totalrise", pv->m_fTotalRise);
         MeasureToString (pPage, L"width", pv->m_fWidth);
         MeasureToString (pPage, L"length", pv->m_fLength);
         pControl = pPage->ControlFind (pv->m_fPathClockwise ? L"clock" : L"counter");
         if (pControl)
            pControl->AttribSetBOOL (Checked(), TRUE);
         MeasureToString (pPage, L"stairwellwidth", pv->m_fStairwellWidth);
         MeasureToString (pPage, L"stairwelllength", pv->m_fStairwellLength);
         MeasureToString (pPage, L"spiraldiameter", pv->m_fSpiralRadius * 2.0);
         DoubleToControl (pPage, L"spiralrevolutions", pv->m_fSpiralRevolutions);
         MeasureToString (pPage, L"spiralpostdiameter", pv->m_fSpiralPost ? pv->m_fSpiralPostDiameter : 0);
         MeasureToString (pPage, L"spiralpostheight", pv->m_fSpiralPostHeight);
         MeasureToString (pPage, L"stairwelllanding", pv->m_fStairwellLanding);
         DoubleToControl (pPage, L"stairwelltwoperlevel", pv->m_fStairwellTwoPerLevel ? 2 : 1);
         MeasureToString (pPage, L"restlanding", pv->m_fRestLanding);
         MeasureToString (pPage, L"landingtop", pv->m_fLandingTop);
         pControl = pPage->ControlFind ((pv->m_iLandingTopDir == 0) ? L"lstraight" :
            ((pv->m_iLandingTopDir < 0) ? L"lleft" : L"lright"));
         if (pControl)
            pControl->AttribSetBOOL (Checked(), TRUE);

         // draw images
         GenerateThreeDFromSpline (L"edgeaddremove", pPage, pv->m_psLeft, pv->m_psRight, 0);
         GenerateThreeDFromSpline (L"edgecurve", pPage, pv->m_psLeft, pv->m_psRight, 1);
      }
      break;

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

         // allocate enough memory so can do the calculations
         CMem  amemPoints[3];
         CMem  amemText[3];
         CMem  memSegCurve;
         PCPoint paPoints[3];
         PTEXTUREPOINT paText[3];
         DWORD dwOrig;
         dwOrig = pv->m_psCenter->OrigNumPointsGet();
         DWORD k;
         for (k = 0; k < 3; k++) {
            if (!amemPoints[k].Required ((dwOrig+1) * sizeof(CPoint)))
               return TRUE;
            if (!amemText[k].Required ((dwOrig+1) * sizeof(TEXTUREPOINT)))
               return TRUE;
            paPoints[k] = (PCPoint) amemPoints[k].p;
            paText[k] = (PTEXTUREPOINT) amemText[k].p;
         }
         if (!memSegCurve.Required ((dwOrig+1) * sizeof(DWORD)))
            return TRUE;

         // load it in
         DWORD *padw;
         PCSpline ps;
         padw = (DWORD*) memSegCurve.p;
         DWORD i;
         for (i = 0; i < dwOrig; i++) {
            pv->m_psCenter->OrigSegCurveGet (i, padw + i);

            for (k = 0; k < 3; k++) {
               ps = ((k == 0) ? pv->m_psLeft : ((k==1) ? pv->m_psCenter : pv->m_psRight) );
               ps->OrigPointGet (i, paPoints[k] + i);
               ps->OrigTextureGet (i, paText[k] + i);
            }
         }
         DWORD dwMinDivide, dwMaxDivide;
         BOOL fLooped;
         fp fDetail;
         pv->m_psCenter->DivideGet (&dwMinDivide, &dwMaxDivide, &fDetail);
         fLooped = pv->m_psCenter->LoopedGet();

         if (fCol) {
            if (dwMode == 1) {
               // inserting
               for (k = 0; k < 3; k++) {
                  ps = ((k == 0) ? pv->m_psLeft : ((k==1) ? pv->m_psCenter : pv->m_psRight) );

                  // points
                  memmove (paPoints[k] + (x+1), paPoints[k] + x, sizeof(CPoint) * (dwOrig-x));
                  (paPoints[k])[x+1].Add (paPoints[k] + ((x+2) % (dwOrig+1)));
                  (paPoints[k])[x+1].Scale (.5);

                  // textures
                  memmove (paText[k] + (x+1), paText[k] + x, sizeof(TEXTUREPOINT) * (dwOrig-x));
                  (paText[k])[x+1].h = ((paText[k])[x+1].h + (paText[k])[(x+2) % (dwOrig+1)].h) / 2.0;
                  (paText[k])[x+1].v = ((paText[k])[x+1].v + (paText[k])[(x+2) % (dwOrig+1)].v) / 2.0;
               }

               memmove (padw + (x+1), padw + x, sizeof(DWORD) * (dwOrig - x));
               dwOrig++;
            }
            else if (dwMode == 2) {
               // deleting
               for (k = 0; k < 3; k++) {
                  ps = ((k == 0) ? pv->m_psLeft : ((k==1) ? pv->m_psCenter : pv->m_psRight) );

                  memmove (paPoints[k] + x, paPoints[k] + (x+1), sizeof(CPoint) * (dwOrig-x-1));
                  memmove (paText[k] + x, paText[k] + (x+1), sizeof(TEXTUREPOINT) * (dwOrig-x-1));
               }

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

         // changed
         pv->m_pWorld->ObjectAboutToChange (pv);

         // splines
         for (k = 0; k < 3; k++) {
            ps = ((k == 0) ? pv->m_psLeft : ((k==1) ? pv->m_psCenter : pv->m_psRight) );

            ps->Init (fLooped, dwOrig, paPoints[k], paText[k], padw, 0, 3, .1);
            // always use 3 so that if custom divide for curvature
         }

         pv->CalcLRFromPathType (pv->m_dwPathType);
         pv->CalcTread ();

         pv->m_pWorld->ObjectChanged (pv);

         // redraw the shapes
         GenerateThreeDFromSpline (L"edgeaddremove", pPage, pv->m_psLeft, pv->m_psRight, 0);
         GenerateThreeDFromSpline (L"edgecurve", pPage, pv->m_psLeft, pv->m_psRight, 1);
      }
      break;


   case ESCN_EDITCHANGE:
      {
         PESCNEDITCHANGE p = (PESCNEDITCHANGE) pParam;

         // anything that happens here will be one of my edit boxes
         if (!p->pControl || !p->pControl->m_pszName)
            break;
         PWSTR psz;
         psz = p->pControl->m_pszName;

         if (pv->m_pWorld)
            pv->m_pWorld->ObjectAboutToChange (pv);

         if (!_wcsicmp (psz, L"riserlevel")) {
            pv->m_dwRiserLevels = (DWORD) DoubleFromControl (pPage, psz);
            pv->m_dwRiserLevels = max (1, pv->m_dwRiserLevels);
         }
         else if (!_wcsicmp(psz, L"levelheight")) {
            MeasureParseString (pPage, psz, &pv->m_fLevelHeight);
            pv->m_fLevelHeight = max(1, pv->m_fLevelHeight);
         }
         else if (!_wcsicmp(psz, L"totalrise")) {
            MeasureParseString (pPage, psz, &pv->m_fTotalRise);
            pv->m_fTotalRise = max(.01, pv->m_fTotalRise);
         }
         else if (!_wcsicmp(psz, L"width")) {
            MeasureParseString (pPage, psz, &pv->m_fWidth);
            pv->m_fWidth = max(.1, pv->m_fWidth);
         }
         else if (!_wcsicmp(psz, L"length")) {
            MeasureParseString (pPage, psz, &pv->m_fLength);
            pv->m_fLength = max(.1, pv->m_fLength);
         }
         else if (!_wcsicmp(psz, L"stairwellwidth")) {
            MeasureParseString (pPage, psz, &pv->m_fStairwellWidth);
            pv->m_fStairwellWidth = max(1, pv->m_fStairwellWidth);
         }
         else if (!_wcsicmp(psz, L"stairwelllength")) {
            MeasureParseString (pPage, psz, &pv->m_fStairwellLength);
            pv->m_fStairwellLength = max(1, pv->m_fStairwellLength);
         }
         else if (!_wcsicmp(psz, L"spiraldiameter")) {
            if (MeasureParseString (pPage, psz, &pv->m_fSpiralRadius))
               pv->m_fSpiralRadius /= 2.0;
            pv->m_fSpiralRadius = max(.1, pv->m_fSpiralRadius);
         }
         else if (!_wcsicmp (psz, L"spiralrevolutions")) {
            pv->m_fSpiralRevolutions = DoubleFromControl (pPage, psz);
            pv->m_fSpiralRevolutions = max (.1, pv->m_fSpiralRevolutions);
         }
         else if (!_wcsicmp(psz, L"spiralpostdiameter")) {
            MeasureParseString (pPage, psz, &pv->m_fSpiralPostDiameter);
            pv->m_fSpiralPostDiameter = max(0, pv->m_fSpiralPostDiameter);
            pv->m_fSpiralPost = (pv->m_fSpiralPostDiameter > 0.0);
         }
         else if (!_wcsicmp(psz, L"spiralpostheight")) {
            MeasureParseString (pPage, psz, &pv->m_fSpiralPostHeight);
         }
         else if (!_wcsicmp(psz, L"stairwelllanding")) {
            MeasureParseString (pPage, psz, &pv->m_fStairwellLanding);
            pv->m_fStairwellLanding = max(.1, pv->m_fStairwellLanding);
         }
         else if (!_wcsicmp (psz, L"stairwelltwoperlevel")) {
            pv->m_fStairwellTwoPerLevel = (DoubleFromControl (pPage, psz) >= 2);
         }
         else if (!_wcsicmp(psz, L"restlanding")) {
            MeasureParseString (pPage, psz, &pv->m_fRestLanding);
            pv->m_fRestLanding = max(0, pv->m_fRestLanding);
         }
         else if (!_wcsicmp(psz, L"landingtop")) {
            MeasureParseString (pPage, psz, &pv->m_fLandingTop);
            pv->m_fLandingTop = max(0, pv->m_fLandingTop);
         }

         pv->CalcLRFromPathType (pv->m_dwPathType);
         pv->CalcTread ();

         if (pv->m_pWorld)
            pv->m_pWorld->ObjectChanged (pv);
      }
      break;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;

         if (!_wcsicmp(p->pControl->m_pszName, L"clock") || !_wcsicmp(p->pControl->m_pszName, L"counter")) {
            if (pv->m_pWorld)
               pv->m_pWorld->ObjectAboutToChange (pv);

            pv->m_fPathClockwise = !_wcsicmp(p->pControl->m_pszName, L"clock");
            pv->CalcLRFromPathType (pv->m_dwPathType);
            pv->CalcTread ();

            if (pv->m_pWorld)
               pv->m_pWorld->ObjectChanged (pv);
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"lleft") || !_wcsicmp(p->pControl->m_pszName, L"lright")
            || !_wcsicmp(p->pControl->m_pszName, L"lstraight")) {
            if (pv->m_pWorld)
               pv->m_pWorld->ObjectAboutToChange (pv);

            if (!_wcsicmp(p->pControl->m_pszName, L"lleft"))
               pv->m_iLandingTopDir = -1;
            else if (!_wcsicmp(p->pControl->m_pszName, L"lright"))
               pv->m_iLandingTopDir = 1;
            else
               pv->m_iLandingTopDir = 0;
            pv->CalcLRFromPathType (pv->m_dwPathType);
            pv->CalcTread ();

            if (pv->m_pWorld)
               pv->m_pWorld->ObjectChanged (pv);
            return TRUE;
         }

      }
      break;   // default


   case ESCN_LISTBOXSELCHANGE:
      {
         PESCNLISTBOXSELCHANGE p = (PESCNLISTBOXSELCHANGE) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;
         if (!_wcsicmp(p->pControl->m_pszName, L"path")) {
            DWORD dwVal;
            dwVal = _wtoi(p->pszName);
            if (dwVal == pv->m_dwPathType)
               return TRUE;   // no change

            if (pv->m_pWorld)
               pv->m_pWorld->ObjectAboutToChange (pv);

            pv->m_dwPathType = dwVal;
            pv->CalcLRFromPathType (pv->m_dwPathType);
            pv->CalcTread ();

            if (pv->m_pWorld)
               pv->m_pWorld->ObjectChanged (pv);

            // redo same page
            pPage->Exit (RedoSamePage());
            return TRUE;
         }
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         PWSTR pszNone = L"";
         PWSTR pszCommentStart = L"<comment>";
         PWSTR pszCommentEnd = L"</comment>";

         BOOL fStairwell = FALSE, fShowHeight = FALSE, fShowWidth = TRUE;
         BOOL fShowLength = FALSE, fShowClockwise = FALSE, fShowSpiral = FALSE;;
         BOOL fShowPerLevel = FALSE, fShowMidpoint = FALSE, fShowTopLanding = TRUE;
         BOOL fShowControl = FALSE;
         switch (pv->m_dwPathType) {
         case SPATH_STRAIGHT        :     // straight up
            fShowHeight = TRUE;
            fShowLength = TRUE;
            fShowMidpoint = TRUE;
            break;
         case SPATH_LANDINGTURN     :     // up, turn right/left, and up some more
            fShowHeight = TRUE;
            fShowLength = TRUE;
            fShowClockwise = TRUE;
            break;
         case SPATH_STAIRWELL       :     // switches back and forth
            fStairwell = TRUE;
            fShowClockwise = TRUE;
            fShowPerLevel = TRUE;
            fShowMidpoint = TRUE;
            break;
         case SPATH_STAIRWELL2      :     // stairscase inside (or outside) a rectangle
            fStairwell = TRUE;
            fShowClockwise = TRUE;
            break;
         case SPATH_SPIRAL          :     // spiral staircase
            fShowHeight = TRUE;
            fShowClockwise = TRUE;
            fShowSpiral = TRUE;
            break;
         case SPATH_WINDING         :     // just to show that can do winding
            fShowHeight = TRUE;
            fShowLength = TRUE;
            break;
         case SPATH_CUSTOMCENTER    :     // custom path with only the center controled
            fShowTopLanding = FALSE;
            fShowControl = TRUE;
            break;
         case SPATH_CUSTOMLR        :     // custom path with left/right controlled
            fShowWidth = FALSE;
            fShowTopLanding = FALSE;
            fShowControl = TRUE;
            break;
         }

         if (!_wcsicmp(p->pszSubName, L"IFSHOWTOPLANDING")) {
            p->pszSubString = fShowTopLanding ? pszNone : pszCommentStart;
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"ENDIFSHOWTOPLANDING")) {
            p->pszSubString = fShowTopLanding ? pszNone : pszCommentEnd;
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"IFSHOWCONTROL")) {
            p->pszSubString = fShowControl ? pszNone : pszCommentStart;
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"ENDIFSHOWCONTROL")) {
            p->pszSubString = fShowControl ? pszNone : pszCommentEnd;
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"IFSHOWMIDPOINT")) {
            p->pszSubString = fShowMidpoint ? pszNone : pszCommentStart;
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"ENDIFSHOWMIDPOINT")) {
            p->pszSubString = fShowMidpoint ? pszNone : pszCommentEnd;
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"IFSHOWPERLEVEL")) {
            p->pszSubString = fShowPerLevel ? pszNone : pszCommentStart;
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"ENDIFSHOWPERLEVEL")) {
            p->pszSubString = fShowPerLevel ? pszNone : pszCommentEnd;
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"IFSHOWSPIRAL")) {
            p->pszSubString = fShowSpiral ? pszNone : pszCommentStart;
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"ENDIFSHOWSPIRAL")) {
            p->pszSubString = fShowSpiral ? pszNone : pszCommentEnd;
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"IFSHOWCLOCKWISE")) {
            p->pszSubString = fShowClockwise ? pszNone : pszCommentStart;
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"ENDIFSHOWCLOCKWISE")) {
            p->pszSubString = fShowClockwise ? pszNone : pszCommentEnd;
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"IFSHOWLENGTH")) {
            p->pszSubString = fShowLength ? pszNone : pszCommentStart;
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"ENDIFSHOWLENGTH")) {
            p->pszSubString = fShowLength ? pszNone : pszCommentEnd;
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"IFSHOWWIDTH")) {
            p->pszSubString = fShowWidth ? pszNone : pszCommentStart;
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"ENDIFSHOWWIDTH")) {
            p->pszSubString = fShowWidth ? pszNone : pszCommentEnd;
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"IFSHOWHEIGHT")) {
            p->pszSubString = fShowHeight ? pszNone : pszCommentStart;
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"ENDIFSHOWHEIGHT")) {
            p->pszSubString = fShowHeight ? pszNone : pszCommentEnd;
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"IFSTAIRWELL")) {
            p->pszSubString = fStairwell ? pszNone : pszCommentStart;
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"ENDIFSTAIRWELL")) {
            p->pszSubString = fStairwell ? pszNone : pszCommentEnd;
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Staircase path";
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}


/* StairsTreadsPage
*/
BOOL StairsTreadsPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCObjectStairs pv = (PCObjectStairs) pPage->m_pUserData;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         PCEscControl pControl;
         // set all the parameters
         MeasureToString (pPage, L"treadthickness", pv->m_fTreadThickness);
         MeasureToString (pPage, L"treadextendback", pv->m_fTreadExtendBack);
         MeasureToString (pPage, L"treadmindepth", pv->m_fTreadMinDepth);
         MeasureToString (pPage, L"treadextendleft", pv->m_fTreadExtendLeft);
         MeasureToString (pPage, L"treadextendright", pv->m_fTreadExtendRight);
         MeasureToString (pPage, L"riser", pv->m_fRiser);
         MeasureToString (pPage, L"kickboardindent", pv->m_fKickboardIndent);
         MeasureToString (pPage, L"treadlipthickness", pv->m_fTreadLipThickness);
         MeasureToString (pPage, L"treadlipdepth", pv->m_fTreadLipDepth);
         MeasureToString (pPage, L"treadlipoffset", pv->m_fTreadLipOffset);

         pControl = pPage->ControlFind (L"kickboard");
         if (pControl)
            pControl->AttribSetBOOL (Checked(), pv->m_fKickboard);
         pControl = pPage->ControlFind (L"treadlip");
         if (pControl)
            pControl->AttribSetBOOL (Checked(), pv->m_fTreadLip);
      }
      break;


   case ESCN_EDITCHANGE:
      {
         PESCNEDITCHANGE p = (PESCNEDITCHANGE) pParam;

         // anything that happens here will be one of my edit boxes
         if (!p->pControl || !p->pControl->m_pszName)
            break;
         PWSTR psz;
         psz = p->pControl->m_pszName;

         if (pv->m_pWorld)
            pv->m_pWorld->ObjectAboutToChange (pv);

         if (!_wcsicmp(psz, L"treadthickness")) {
            MeasureParseString (pPage, psz, &pv->m_fTreadThickness);
            pv->m_fTreadThickness = max(.01, pv->m_fTreadThickness);
         }
         else if (!_wcsicmp(psz, L"treadextendback")) {
            MeasureParseString (pPage, psz, &pv->m_fTreadExtendBack);
         }
         else if (!_wcsicmp(psz, L"treadmindepth")) {
            MeasureParseString (pPage, psz, &pv->m_fTreadMinDepth);
            pv->m_fTreadMinDepth = max(0, pv->m_fTreadMinDepth);
         }
         else if (!_wcsicmp(psz, L"treadextendleft")) {
            MeasureParseString (pPage, psz, &pv->m_fTreadExtendLeft);
         }
         else if (!_wcsicmp(psz, L"treadextendright")) {
            MeasureParseString (pPage, psz, &pv->m_fTreadExtendRight);
         }
         else if (!_wcsicmp(psz, L"riser")) {
            MeasureParseString (pPage, psz, &pv->m_fRiser);
            pv->m_fRiser = max(.01, pv->m_fRiser);
         }
         else if (!_wcsicmp(psz, L"kickboardindent")) {
            MeasureParseString (pPage, psz, &pv->m_fKickboardIndent);
         }
         else if (!_wcsicmp(psz, L"treadlipthickness")) {
            MeasureParseString (pPage, psz, &pv->m_fTreadLipThickness);
            pv->m_fTreadLipThickness = max(.001, pv->m_fTreadLipThickness);
         }
         else if (!_wcsicmp(psz, L"treadlipdepth")) {
            MeasureParseString (pPage, psz, &pv->m_fTreadLipDepth);
            pv->m_fTreadLipDepth = max(.001, pv->m_fTreadLipDepth);
         }
         else if (!_wcsicmp(psz, L"treadlipoffset")) {
            MeasureParseString (pPage, psz, &pv->m_fTreadLipOffset);
         }

         pv->CalcLRFromPathType (pv->m_dwPathType);
         pv->CalcTread ();

         if (pv->m_pWorld)
            pv->m_pWorld->ObjectChanged (pv);
      }
      break;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;

         if (!_wcsicmp(p->pControl->m_pszName, L"kickboard")
            || !_wcsicmp(p->pControl->m_pszName, L"treadlip")) {
            if (pv->m_pWorld)
               pv->m_pWorld->ObjectAboutToChange (pv);

            BOOL fVal = p->pControl->AttribGetBOOL (Checked());

            if (!_wcsicmp(p->pControl->m_pszName, L"kickboard"))
               pv->m_fKickboard = fVal;
            else  // treadlip
               pv->m_fTreadLip = fVal;

            pv->CalcLRFromPathType (pv->m_dwPathType);
            pv->CalcTread ();

            if (pv->m_pWorld)
               pv->m_pWorld->ObjectChanged (pv);
            return TRUE;
         }

      }
      break;   // default

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Staircase treads";
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}

/* StairsDialogPage
*/
BOOL StairsDialogPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCObjectStairs pv = (PCObjectStairs) pPage->m_pUserData;

   switch (dwMessage) {
   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Staircase settings";
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}


/* StairsBalPage
*/
BOOL StairsBalPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCObjectStairs pv = (PCObjectStairs) pPage->m_pUserData;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         PCEscControl pControl;
         DWORD i;
         WCHAR szTemp[32];

         for (i = 0; i < 3; i++) {
            swprintf (szTemp, L"balon%d", (int) i);
            pControl = pPage->ControlFind (szTemp);
            if (pControl)
               pControl->AttribSetBOOL (Checked(), pv->m_afBalWant[i]);

            swprintf (szTemp, L"edit%d", (int) i);
            pControl = pPage->ControlFind (szTemp);
            if (pControl)
               pControl->Enable (pv->m_apBal[i] ? TRUE : FALSE);
         }
      }
      break;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;

         PWSTR pszBalOn = L"balon";
         if (!wcsncmp(pszBalOn, p->pControl->m_pszName, wcslen(pszBalOn))) {
            if (pv->m_pWorld)
               pv->m_pWorld->ObjectAboutToChange (pv);

            DWORD dw = _wtoi(p->pControl->m_pszName + wcslen(pszBalOn));
            dw = min(2, dw);

            BOOL fVal = p->pControl->AttribGetBOOL (Checked());
            pv->m_afBalWant[dw] = fVal;

            pv->CalcTread ();

            if (pv->m_pWorld)
               pv->m_pWorld->ObjectChanged (pv);

            WCHAR szTemp[32];
            PCEscControl pControl;
            swprintf (szTemp, L"edit%d", (int) dw);
            pControl = pPage->ControlFind (szTemp);
            if (pControl)
               pControl->Enable (pv->m_apBal[dw] ? TRUE : FALSE);

            return TRUE;
         }

      }
      break;   // default

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Staircase balustrades";
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}



/* StairsWallsPage
*/
BOOL StairsWallsPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCObjectStairs pv = (PCObjectStairs) pPage->m_pUserData;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         PCEscControl pControl;
         DWORD i;
         WCHAR szTemp[32];

         for (i = 0; i < 3; i++) {
            swprintf (szTemp, L"balon%d", (int) i);
            pControl = pPage->ControlFind (szTemp);
            if (pControl)
               pControl->AttribSetBOOL (Checked(), pv->m_afWallWant[i]);
         }

         MeasureToString (pPage, L"wallindent", pv->m_fWallIndent);
         MeasureToString (pPage, L"wallbelow", pv->m_fWallToGround ? 0 : pv->m_fWallBelow);
      }
      break;


   case ESCN_EDITCHANGE:
      {
         PESCNEDITCHANGE p = (PESCNEDITCHANGE) pParam;

         // anything that happens here will be one of my edit boxes
         if (!p->pControl || !p->pControl->m_pszName)
            break;
         PWSTR psz;
         psz = p->pControl->m_pszName;

         if (pv->m_pWorld)
            pv->m_pWorld->ObjectAboutToChange (pv);

         if (!_wcsicmp (psz, L"wallindent")) {
            MeasureParseString (pPage, psz, &pv->m_fWallIndent);
         }
         else if (!_wcsicmp(psz, L"wallbelow")) {
            MeasureParseString (pPage, psz, &pv->m_fWallBelow);
            pv->m_fWallToGround = (pv->m_fWallBelow <= 0.0);
         }

         pv->CalcLRFromPathType (pv->m_dwPathType);
         pv->CalcTread ();

         if (pv->m_pWorld)
            pv->m_pWorld->ObjectChanged (pv);
      }
      break;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;

         PWSTR pszBalOn = L"balon";
         if (!wcsncmp(pszBalOn, p->pControl->m_pszName, wcslen(pszBalOn))) {
            if (pv->m_pWorld)
               pv->m_pWorld->ObjectAboutToChange (pv);

            DWORD dw = _wtoi(p->pControl->m_pszName + wcslen(pszBalOn));
            dw = min(2, dw);

            BOOL fVal = p->pControl->AttribGetBOOL (Checked());
            pv->m_afWallWant[dw] = fVal;

            pv->CalcTread ();

            if (pv->m_pWorld)
               pv->m_pWorld->ObjectChanged (pv);
            return TRUE;
         }

      }
      break;   // default

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Staircase walls";
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}

/* StairsRailsPage
*/
BOOL StairsRailsPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCObjectStairs pv = (PCObjectStairs) pPage->m_pUserData;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         PCEscControl pControl;
         DWORD i;
         WCHAR szTemp[32];

         for (i = 0; i < 3; i++) {
            swprintf (szTemp, L"balon%d", (int) i);
            pControl = pPage->ControlFind (szTemp);
            if (pControl)
               pControl->AttribSetBOOL (Checked(), pv->m_afRailWant[i]);
         }

         pControl = pPage->ControlFind (L"railextend");
         if (pControl)
            pControl->AttribSetBOOL (Checked(), pv->m_fRailExtend);

         ESCMCOMBOBOXSELECTSTRING css;
         memset (&css, 0, sizeof(css));
         css.fExact = TRUE;
         css.iStart = -1;

         // main column
         for (i = 0; i< 3; i++) {
            WCHAR sz2[16];
            swprintf (szTemp, L"shape%d", (int) i);
            swprintf (sz2, L"%d", (int) pv->m_adwRailShape[i]);
            pControl = pPage->ControlFind (szTemp);
            css.psz = sz2;
            if (pControl)
               pControl->Message (ESCM_COMBOBOXSELECTSTRING, &css);
         }

         MeasureToString (pPage, L"railsize0", pv->m_pRailSize.h);
         MeasureToString (pPage, L"railsize1", pv->m_pRailSize.v);
         MeasureToString (pPage, L"railoffset0", pv->m_pRailOffset.h);
         MeasureToString (pPage, L"railoffset1", pv->m_pRailOffset.v);
      }
      break;


   case ESCN_COMBOBOXSELCHANGE:
      {
         PESCNCOMBOBOXSELCHANGE p = (PESCNCOMBOBOXSELCHANGE) pParam;
         if (!p->pControl || !p->pControl->m_pszName || !p->pszName)
            break;

         PWSTR pszName = L"shape";
         DWORD dwLen = (DWORD)wcslen(pszName);
         if (!wcsncmp(p->pControl->m_pszName, pszName, dwLen)) {
            DWORD dw = _wtoi(p->pControl->m_pszName + dwLen);
            dw = min(2,dw);

            DWORD dwVal;
            dwVal = _wtoi (p->pszName);

            if (pv->m_adwRailShape[dw] == dwVal)
               break;   // same so dont bother

            if (pv->m_pWorld)
               pv->m_pWorld->ObjectAboutToChange (pv);
            pv->m_adwRailShape[dw] = dwVal;
            pv->CalcLRFromPathType (pv->m_dwPathType);
            pv->CalcTread ();

            if (pv->m_pWorld)
               pv->m_pWorld->ObjectChanged (pv);
            return TRUE;
         }
      }
      break;

   case ESCN_EDITCHANGE:
      {
         PESCNEDITCHANGE p = (PESCNEDITCHANGE) pParam;

         // anything that happens here will be one of my edit boxes
         if (!p->pControl || !p->pControl->m_pszName)
            break;
         PWSTR psz;
         psz = p->pControl->m_pszName;

         if (pv->m_pWorld)
            pv->m_pWorld->ObjectAboutToChange (pv);

         fp fTemp;
         if (!_wcsicmp (psz, L"railoffset0")) {
            MeasureParseString (pPage, psz, &fTemp);
            pv->m_pRailOffset.h = fTemp;
         }
         else if (!_wcsicmp (psz, L"railoffset1")) {
            MeasureParseString (pPage, psz, &fTemp);
            pv->m_pRailOffset.v = fTemp;
         }
         else if (!_wcsicmp(psz, L"railsize0")) {
            MeasureParseString (pPage, psz, &fTemp);
            pv->m_pRailSize.h = fTemp;
            pv->m_pRailSize.h = max(.001, pv->m_pRailSize.h);
         }
         else if (!_wcsicmp(psz, L"railsize1")) {
            MeasureParseString (pPage, psz, &fTemp);
            pv->m_pRailSize.v = fTemp;
            pv->m_pRailSize.v = max(.001, pv->m_pRailSize.v);
         }

         pv->CalcLRFromPathType (pv->m_dwPathType);
         pv->CalcTread ();

         if (pv->m_pWorld)
            pv->m_pWorld->ObjectChanged (pv);
      }
      break;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;

         PWSTR pszBalOn = L"balon";
         if (!wcsncmp(pszBalOn, p->pControl->m_pszName, wcslen(pszBalOn))) {
            if (pv->m_pWorld)
               pv->m_pWorld->ObjectAboutToChange (pv);

            DWORD dw = _wtoi(p->pControl->m_pszName + wcslen(pszBalOn));
            dw = min(2, dw);

            BOOL fVal = p->pControl->AttribGetBOOL (Checked());
            pv->m_afRailWant[dw] = fVal;

            pv->CalcTread ();

            if (pv->m_pWorld)
               pv->m_pWorld->ObjectChanged (pv);
            return TRUE;
         }
         else if (!_wcsicmp (p->pControl->m_pszName, L"railextend")) {
            if (pv->m_pWorld)
               pv->m_pWorld->ObjectAboutToChange (pv);

            pv->m_fRailExtend = p->pControl->AttribGetBOOL (Checked());

            pv->CalcTread ();

            if (pv->m_pWorld)
               pv->m_pWorld->ObjectChanged (pv);
            return TRUE;
         }

      }
      break;   // default

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Staircase rails";
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}

/**********************************************************************************
CObjectStairs::DialogShow - Causes the object to show a dialog box that allows
different attributes to be changed.

inputs
   DWORD             dwSuface - Surface that was clicked on. 0 if unknown.
   PCEscWindow       pWindow - Escarpment window to draw it in
returns
   BOOL - TRUE if user pressed "Back", FALSE if user pressed "Close"
*/
BOOL CObjectStairs::DialogShow (DWORD dwSurface, PCEscWindow pWindow, PCPoint pClick)
{
   PWSTR pszRet;
firstpage:
   pszRet = pWindow->PageDialog (ghInstance, IDR_MMLSTAIRSDIALOG, StairsDialogPage, this);
firstpage2:
   if (!pszRet)
      return FALSE;
   if (!_wcsicmp(pszRet, L"path")) {
redopath:
      pszRet = pWindow->PageDialog (ghInstance, IDR_MMLSTAIRSPATH, StairsPathPage, this);

      if (pszRet && !_wcsicmp(pszRet, Back()))
         goto firstpage;
      else if (pszRet && !_wcsicmp(pszRet, RedoSamePage()))
         goto redopath;
      else
         goto firstpage2;
   }
   else if (!_wcsicmp(pszRet, L"treads")) {
      pszRet = pWindow->PageDialog (ghInstance, IDR_MMLSTAIRSTREADS, StairsTreadsPage, this);

      if (pszRet && !_wcsicmp(pszRet, Back()))
         goto firstpage;
      else
         goto firstpage2;
   }
   else if (!_wcsicmp(pszRet, L"walls")) {
      pszRet = pWindow->PageDialog (ghInstance, IDR_MMLSTAIRSWALLS, StairsWallsPage, this);

      if (pszRet && !_wcsicmp(pszRet, Back()))
         goto firstpage;
      else
         goto firstpage2;
   }
   else if (!_wcsicmp(pszRet, L"rails")) {
      pszRet = pWindow->PageDialog (ghInstance, IDR_MMLSTAIRSRAILS, StairsRailsPage, this);

      if (pszRet && !_wcsicmp(pszRet, Back()))
         goto firstpage;
      else
         goto firstpage2;
   }
   else if (!_wcsicmp(pszRet, L"balustrades")) {
balpage:
      pszRet = pWindow->PageDialog (ghInstance, IDR_MMLSTAIRSBAL, StairsBalPage, this);

      PWSTR pszEdit = L"edit";
      DWORD dwLen = (DWORD)wcslen(pszEdit);

      if (pszRet && !_wcsicmp(pszRet, Back()))
         goto firstpage;
      else if (pszRet && !wcsncmp(pszRet, pszEdit, dwLen)) {
         DWORD dw = _wtoi(pszRet + dwLen);
         dw = min(2,dw);
         if (!m_apBal[dw])
            goto firstpage;
         pszRet = m_apBal[dw]->AppearancePage (pWindow, NULL);
         if (pszRet && !_wcsicmp(pszRet, Back()))
            goto balpage;
         else
            goto firstpage2;
      }
      else
         goto firstpage2;
   }
   return !_wcsicmp(pszRet, Back());
}


/**************************************************************************************
CObjectTemplate::MoveReferencePointQuery - 
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
BOOL CObjectStairs::MoveReferencePointQuery (DWORD dwIndex, PCPoint pp)
{
   PSPLINEMOVEP ps;
   DWORD dwDataSize;
   ps = gaSplineMoveP;
   dwDataSize = sizeof(gaSplineMoveP);

   if (dwIndex >= dwDataSize / sizeof(SPLINEMOVEP))
      return FALSE;

   DWORD dwWidth;
   DWORD dwY;
   dwWidth = m_psCenter->OrigNumPointsGet();
   dwY = (ps[dwIndex].iY < 0) ? 0 : (dwWidth-1);

   TEXTUREPOINT tp;

   if (ps[dwIndex].iX == 1) {
      // special case
      m_psCenter->OrigPointGet (dwWidth-1, pp);
      m_psCenter->OrigTextureGet (0, &tp);
      pp->p[2] = tp.h;
      pp->p[3] = 1;
      return TRUE;
   }

   if (!m_psCenter->OrigTextureGet (dwY, &tp))
      return FALSE;
   
   if (!m_psCenter->OrigPointGet (dwY, pp))
      return FALSE;

   // BUGFIX - Have to use texture's h for the z coord otherwise doesn't do curves properly
   pp->p[2] = tp.h;
   pp->p[3] = 1;
   return TRUE;
}

/**************************************************************************************
CObjectStairs::MoveReferenceStringQuery -
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
BOOL CObjectStairs::MoveReferenceStringQuery (DWORD dwIndex, PWSTR psz, DWORD dwSize, DWORD *pdwNeeded)
{
   PSPLINEMOVEP ps;
   DWORD dwDataSize;
   ps = gaSplineMoveP;
   dwDataSize = sizeof(gaSplineMoveP);
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

/**************************************************************************************
FindValue - Given a Z value, and a spline (assuming it's always going up), this
finds the distance (0..1) into the spline that the elevation occurs.

inputs
   PCSpline    pSpline - to look at
   fp      fZ - Z that looing for
   DWORD       dwStart, dwEnd - Start and end pSpline->OrigPointGet(), dwStart <= x < dwEnd.
               Sets a boundary to look at
retursn  
   fp - 0..1 distance into spline. -1 if cant find
*/
static fp FindValue (PCSpline pSpline, fp fZ, DWORD dwStart, DWORD dwEnd)
{
   // error if looped
   if (pSpline->LoopedGet())
      return -1;

   // find out how many divisions
   DWORD dwNodes, dwPerOrig;
   dwNodes = pSpline->QueryNodes ();
   dwPerOrig = (dwNodes - 1) / (pSpline->OrigNumPointsGet() - 1); // can do this because not looped

   // so the real start and end
   dwStart *= dwPerOrig;
   dwEnd = (dwEnd-1) * dwPerOrig + 1;

   // loop
   DWORD i;
   for (i = dwStart; i+1 < dwEnd; i++) {
      PTEXTUREPOINT ps, pe;
      ps = pSpline->TextureGet (i);
      pe = pSpline->TextureGet (i+1);

      // if out of range ignore
      if ((ps->h - EPSILON > fZ) || (pe->h + EPSILON < fZ))
         continue;

      // interpolate
      fp fAlpha;
      fAlpha = pe->h - ps->h;
      if (fabs(fAlpha) > EPSILON)
         fAlpha = (fZ - ps->h) / fAlpha;
      else
         fAlpha = 0;

      fAlpha = (fAlpha + i) / (fp) (dwNodes - 1);

      return fAlpha;
   }

   // if get here can't find
   return -1;
}

/**************************************************************************************
CObjectStairs::CreateWalls - Called from CalcTread(), this function creates/destroyes
the walls if they're not already created/destroyed. It shapes them and does the cutouts.
*/
void CObjectStairs::CreateWalls (void)
{
   // do the walls now
   DWORD dwWall;
   for (dwWall = 0; dwWall < 3; dwWall++) {
      BOOL fJustCreated = FALSE;

      if (!m_afWallWant[dwWall] && m_apWall[dwWall]) {
         m_apWall[dwWall]->ClaimClear();
         delete m_apWall[dwWall];
         m_apWall[dwWall] = NULL;
         continue;
      }
      if (!m_afWallWant[dwWall])
         continue;

      // if get here want walls
      if (!m_apWall[dwWall]) {
         m_apWall[dwWall] = new CDoubleSurface;
         fJustCreated = TRUE;
         if (!m_apWall[dwWall])
            continue;
         m_apWall[dwWall]->Init (m_OSINFO.dwRenderShard, WALLTYPE, this);

         // Set flag so wall points have no restrictions about height
         m_apWall[dwWall]->m_fConstBottom = FALSE;
         m_apWall[dwWall]->m_fConstRectangle = FALSE;
      }

      PCSpline ps;
      PCDoubleSurface pds;
      pds = m_apWall[dwWall];
      ps = ((dwWall == 0) ? m_psLeft : ((dwWall==1) ? m_psCenter : m_psRight));

      // allocate enough memory for the spline information
      CMem  memPoints, memCurve;
      DWORD dwNum;
      dwNum = ps->OrigNumPointsGet();
      if (!memPoints.Required((dwNum*2) * sizeof(CPoint)) || !memCurve.Required(dwNum * sizeof(DWORD))) {
         delete m_apWall[dwWall];
         m_apWall[dwWall] = NULL;
         continue;
      }
      PCPoint pap;
      DWORD *padw;
      pap = (PCPoint) memPoints.p;
      padw = (DWORD*) memCurve.p;

      DWORD j;
      TEXTUREPOINT tp;
      for (j = 0; j < dwNum; j++) {
         ps->OrigPointGet (j, &pap[j]);
         ps->OrigTextureGet (j, &tp);
         pap[j].p[2] = tp.h;
         pap[j].p[3] = tp.h;  // doing this for a special reason, for min/max, as will see later

         ps->OrigSegCurveGet (j, &padw[j]);
      }

      // want to indent
      fp fWallThick;
      fWallThick = (pds->m_fThickA + pds->m_fThickB + pds->m_fThickStud) *.5
         + m_fWallIndent
         + .01;   // just a bit more than expected
      switch (dwWall) {
      case 0:
         break;
      case 1:
         fWallThick = 0;   // no offset
         break;
      case 2:
         fWallThick *= -1;
         break;
      }
      for (j = 0; j < dwNum; j++) {
         CPoint pRight, pLeft;
         m_psRight->OrigPointGet (j, &pRight);
         m_psLeft->OrigPointGet (j, &pLeft);
         m_psRight->OrigTextureGet (j, &tp);
         pRight.p[2] = pLeft.p[2] = tp.h; // both should be the same height

         // find vector for right
         pRight.Subtract (&pLeft);
         pRight.Normalize ();
         pRight.Scale (fWallThick);
         pap[j].Add (&pRight);
      }

      // get rid of duplicates
      for (j = 0; j+1 < dwNum; j++) {
         CPoint p1, p2;
         p1.Copy (&pap[j]);
         p2.Copy (&pap[j+1]);
         p1.p[2] = 0;
         p2.p[2] = 0;
         if (!p1.AreClose (&p2))
            continue;

         // get min and max
         fp   fMin, fMax;
         fMax = max(pap[j].p[2], pap[j+1].p[2]);
         fMin = (pap[j].p[3], pap[j+1].p[2]); // know that in j+1 no min/max has yet been set

         // if get here they're close
         pap[j].p[3] = pap[j+1].p[2];  // use for min/max
         memmove (pap + j, pap + (j+1), (dwNum - j - 1) * sizeof(CPoint));
         memmove (padw + j, padw + (j+1), (dwNum - j - 1) * sizeof(DWORD));
         dwNum--;

         pap[j].p[2] = fMax;
         pap[j].p[3] = fMin;
      }


      // flip for the right side
      if (ps == m_psLeft) {
         CPoint p;
         for (j = 0; j < dwNum/2; j++) {
            p.Copy (&pap[j]);
            pap[j].Copy (&pap[dwNum-j-1]);
            pap[dwNum-j-1].Copy (&p);
         }

         // curves
         DWORD dw1;
         for (j = 0; j < dwNum-1; j++) {
            dw1 = padw[j];
            switch (dw1) {
            case SEGCURVE_CIRCLEPREV:
               dw1 = SEGCURVE_CIRCLENEXT;
               break;
            case SEGCURVE_CIRCLENEXT:
               dw1 = SEGCURVE_CIRCLEPREV;
               break;
            case SEGCURVE_ELLIPSEPREV:
               dw1 = SEGCURVE_ELLIPSENEXT;
               break;
            case SEGCURVE_ELLIPSENEXT:
               dw1 = SEGCURVE_ELLIPSEPREV;
               break;
            }
            padw[j] = dw1;
         }
         for (j = 0; j < (dwNum-1)/2; j++) {
            dw1 = padw[j];
            padw[j] = padw[dwNum-2-j];
            padw[dwNum-2-j] = dw1;
         }
      }  // flip for right

      // duplicate points but make lower
      TEXTUREPOINT tTop;
      m_psCenter->OrigTextureGet (m_psCenter->OrigNumPointsGet()-1, &tTop);
      for (j = 0; j < dwNum; j++) {
         pap[j+dwNum].Copy (&pap[j]);
         pap[j].p[2] = max(pap[j].p[2], pap[j].p[3]);
         pap[j+dwNum].p[2] = min(pap[j].p[2], pap[j].p[3]);
         pap[j].p[3] = 1;
         pap[j+dwNum].p[3] = 1;

         // make the top a bit higher so clips OK
         pap[j].p[2] += m_fRiser*2;

         // Option for all the way to ground or just down by a distance
         if (m_fWallToGround) {
            pap[j].p[2] = tTop.h;   // so that when apply texture looks good
            pap[j+dwNum].p[2] = 0;
         }
         else
            pap[j+dwNum].p[2] -= m_fWallBelow;

         // make sure doesn't go below ground
         // NOTE: Disabled because looks a bit wierd at
         // bottom - differnet anggles, if do this
         // pap[j+dwNum].p[2] = max(0,pap[j+dwNum].p[2]);
      }

      // set the points
      DWORD dwMin, dwMax;
      fp fDetail;
      ps->DivideGet (&dwMin, &dwMax, &fDetail);
      pds->ControlPointsSet (dwNum, 2, pap, padw, (DWORD*) SEGCURVE_LINEAR, dwMax);

      // Clipping goes here
      DWORD dwSide;
      PWSTR pszStairCutout = L"StairCutout";
      for (dwSide = 0; dwSide < 2; dwSide++) {
         PCSplineSurface pss = (dwSide ? &pds->m_SplineA : &pds->m_SplineB);
         pss->CutoutRemove (pszStairCutout);

         // create the cutout points list
         CListFixed lCutout;
         lCutout.Init (sizeof(TEXTUREPOINT));
         lCutout.Clear();

         // keep track of last in case dont get intersection. this deals
         // with the last cutout
         BOOL fLastValid;
         CPoint pLastStart, pLastDir;
         fLastValid = FALSE;

         // loop up the stairs creating cutouts
         // BUGFIX - Was Num()+1, but this was causing problems with walls (to full bottom
         // and full top) with spiral staircases because was intersecting with itself
         for (j = 0; j < m_lTREADINFO.Num() + (m_fWallToGround ? 0 : 1); j++) {
            PTREADINFO pti = ((PTREADINFO) m_lTREADINFO.Get(0)) + j;

            CPoint pDir, pStart, pN;
            TEXTUREPOINT tp;

            // do the bottom/back of the previous step
            if (!j) {
               // need to intersect bottom kickboard because no previous step
               QuadNormal (&pN, &m_apBottomKick[0][1], &m_apBottomKick[1][1],
                  &m_apBottomKick[1][0], &m_apBottomKick[0][0]);
               pStart.Copy (&m_apBottomKick[0][0]);
               pDir.Copy (&m_apBottomKick[1][0]);
            }
            else {
               pN.Zero();
               pN.p[2] = 1;
               pStart.Copy (&pti[-1].aExtended[0][1]);
               pDir.Copy (&pti[-1].aExtended[1][1]);
            }
            pDir.Subtract(&pStart);
            pN.Scale (-m_fTreadThickness);
            pStart.Add (&pN);
            if (pss->IntersectLine (&pStart, &pDir, &tp, FALSE, FALSE)) {
               lCutout.Add (&tp);
               fLastValid = TRUE;
               pLastStart.Copy (&pStart);
               pLastDir.Copy (&pDir);
            }
            else if (fLastValid) {
               pStart.Average (&pLastStart, .2);
               pDir.Average (&pLastDir, .2);
               if (pss->IntersectLine (&pStart, &pDir, &tp, FALSE, FALSE)) {
                  lCutout.Add (&tp);
                  fLastValid = TRUE;
                  pLastStart.Copy (&pStart);
                  pLastDir.Copy (&pDir);
               }
            }

            // do the top/back of the previous kickboard
            if (!j) {
               // need to intersect bottom kickboard
               // leave pN
               pStart.Copy (&m_apBottomKick[0][1]);
               pDir.Copy (&m_apBottomKick[1][1]);
            }
            else {
               if (pti[-1].fKickboardValid) {
                  QuadNormal (&pN, &pti[-1].aKickboard[0][1], &pti[-1].aKickboard[1][1],
                     &pti[-1].aKickboard[1][0], &pti[-1].aKickboard[0][0]);
                  pN.Scale (-m_fTreadThickness);
                  pStart.Copy (&pti[-1].aKickboard[0][1]);
                  pDir.Copy (&pti[-1].aKickboard[1][1]);
               }
               else {
                  // do the bottom front of this board
                  if (j+1 >= m_lTREADINFO.Num())
                     continue;   // no tread here
                  pN.Zero();
                  pN.p[2] = 1;
                  pN.Scale (-m_fTreadThickness);   // BUGFIX - Was wrong scale
                  pStart.Copy (&pti->aCorner[0][0]);
                  pDir.Copy (&pti->aCorner[1][0]);
               }
            }
            pDir.Subtract(&pStart);
            pStart.Add (&pN);
            if (pss->IntersectLine (&pStart, &pDir, &tp, FALSE, FALSE)) {
               lCutout.Add (&tp);
               fLastValid = TRUE;
               pLastStart.Copy (&pStart);
               pLastDir.Copy (&pDir);
            }
            else if (fLastValid) {
               pStart.Average (&pLastStart, .2);
               pDir.Average (&pLastDir, .2);
               if (pss->IntersectLine (&pStart, &pDir, &tp, FALSE, FALSE)) {
                  lCutout.Add (&tp);
                  fLastValid = TRUE;
                  pLastStart.Copy (&pStart);
                  pLastDir.Copy (&pDir);
               }
            }

         }

         // which direction are we going right now
         BOOL fClockwise;
         fClockwise = (dwSide == 1);
         if (ps == m_psLeft)
            fClockwise = !fClockwise;

         // finish off and make complete loop
         if (!lCutout.Num())
            continue;   // nothing to cutout. shouldnt happen
         TEXTUREPOINT tFirst, tLast;
         tFirst = *((PTEXTUREPOINT)lCutout.Get(0));
         tLast = *((PTEXTUREPOINT)lCutout.Get(lCutout.Num()-1));

         // make loop to the right, and down
         tLast.h = (tLast.h > tFirst.h) ? 1.0 : 0.0;
         lCutout.Add (&tLast);
         tLast.v = 1.0;
         lCutout.Add (&tLast);
         tFirst.v = 1.0;
         lCutout.Add (&tFirst);


         // flip, depending upon which side and if pLeft or not
         PTEXTUREPOINT ptp;
         ptp = (PTEXTUREPOINT) lCutout.Get(0);
         DWORD dwNum;
         dwNum = lCutout.Num();
         if (fClockwise) {
            DWORD k;
            TEXTUREPOINT tp;
            for (k = 0; k < dwNum/2; k++) {
               tp = ptp[k];
               ptp[k] = ptp[dwNum-k-1];
               ptp[dwNum-k-1] = tp;
            }
         }

         // set it
         pss->CutoutSet (pszStairCutout, ptp, dwNum, FALSE);
      }

   } // over walls
}


/**************************************************************************************
CObjectStairs::CreateRails - Called from CalcTread(), this function creates/destroyes
the rails if they're not already created/destroyed. It shapes them and does the cutouts.
*/
void CObjectStairs::CreateRails (void)
{
   // do the Rails now
   DWORD dwRail;
   for (dwRail = 0; dwRail < 3; dwRail++) {
      BOOL fJustCreated = FALSE;

      if (!m_afRailWant[dwRail] && m_apRail[dwRail]) {
         delete m_apRail[dwRail];
         m_apRail[dwRail] = NULL;
         continue;
      }
      if (!m_afRailWant[dwRail])
         continue;

      // if get here want Rails
      if (!m_apRail[dwRail]) {
         m_apRail[dwRail] = new CNoodle;
         if (!m_apRail[dwRail])
            continue;
      }
      PCNoodle pn;
      pn = m_apRail[dwRail];
      CPoint pUp;
      pUp.Zero();
      pUp.p[2] = 1;

      // loop through all the treads and get raill coords from
      DWORD i;
      CListFixed lPath;
      lPath.Init (sizeof(CPoint));
      lPath.Clear();
      for (i = 0; i < m_lTREADINFO.Num(); i++) {
         PTREADINFO pti = (PTREADINFO) m_lTREADINFO.Get(i);
         CPoint p;

         switch (dwRail) {
         case 0:
            p.Copy (&pti->aExtended[0][1]);
            break;
         default:
         case 1:
            p.Average (&pti->aExtended[0][1], &pti->aExtended[1][1]);
            break;
         case 2:
            p.Copy (&pti->aExtended[1][1]);
            break;
         }

         // account for the tread thickness
         p.p[2] -= m_fTreadThickness;

         // offset
         if (dwRail != 1) {
            CPoint pRight;
            pRight.Subtract (&pti->aExtended[1][1], &pti->aExtended[0][1]);
            pRight.Normalize();
            pRight.Scale (m_pRailOffset.h * ((dwRail == 0) ? 1 : -1));
            p.Add (&pRight);
         }

         // add it
         lPath.Add (&p);
      }

      // find a transition to/from flat sections and do an intersections there
      for (i = 0; i+1 < lPath.Num(); i++) {
         PTREADINFO pti = (PTREADINFO) m_lTREADINFO.Get(i);
         PCPoint pap = (PCPoint) lPath.Get(i);

         // if they're the same then do nothing
         if (!pti[0].fFlatSection == !pti[1].fFlatSection)
            continue;

         // else, intersect
         DWORD dwAngle, dwFlat;
         if (pti[0].fFlatSection) {
            dwFlat = 0;
            dwAngle = 1;
         }
         else {
            dwFlat = 1;
            dwAngle = 0;
         }

         // if the flat section is on the inside, no length to the side, then
         // dont do the intersect
         if (dwRail != 1) {
            DWORD dw = ((dwRail == 0) ? 0 : 1);
            if (pti[dwFlat].aExtended[dw][0].AreClose (&pti[dwFlat].aExtended[dw][1]))
               continue;
         }

         // if we dont have enough points to go on then exit, since to continue
         // the angle need another one
         int iNext;
         iNext = (dwAngle ? 2 : -1);
         if ( (((int)i + iNext) < 0) || (((int)i + iNext) >= (int) m_lTREADINFO.Num()) )
            continue;

         // find the line
         CPoint pInter;
         if (!IntersectLinePlane (&pap[iNext], &pap[dwAngle], &pap[dwFlat], &pUp, &pInter))
            continue;   // dont intersect

         // else, they intersect, so mark then angle point as this new value
         pap[pti[0].fFlatSection ? dwFlat : dwAngle].Copy (&pInter);
      }

      // if the length of the tread on this side is 0 then eliminate
      // NOTE: Need to go backwards so can delete properly
      for (i = lPath.Num()-1; i < lPath.Num(); i--) {
         PTREADINFO pti = (PTREADINFO) m_lTREADINFO.Get(i);
         if (!pti->fFlatSection)
            continue;

         CPoint pLeft, pRight;
         pLeft.Subtract (&pti->aExtended[0][1], &pti->aExtended[0][0]);
         pRight.Subtract (&pti->aExtended[1][1], &pti->aExtended[1][0]);

         BOOL fDelete;
         fDelete = FALSE;
         switch (dwRail) {
         case 0:  // left
            fDelete = (pLeft.Length() < CLOSE);
            break;
         case 1:  // cente
            fDelete = (pLeft.Length() < CLOSE) && (pRight.Length() < CLOSE);
            break;
         case 2:  // right
            fDelete = (pRight.Length() < CLOSE);
            break;
         }

         if (fDelete)
            lPath.Remove (i);
      }

      // eliminate linear sections
      for (i = 0; i+2 < lPath.Num(); ) {
         PCPoint pp;
         pp = (PCPoint) lPath.Get(i);

         // find slopes
         CPoint p1, p2;
         p1.Subtract (&pp[1], &pp[0]);
         p2.Subtract (&pp[2], &pp[1]);
         p1.Normalize();
         p2.Normalize();
         if (p1.AreClose (&p2)) {
            lPath.Remove (i+1);
         }
         else
            i++;
      }

      // extend the top and bottom treads
      if (lPath.Num() < 2) {
         delete m_apRail[dwRail];
         m_apRail[dwRail] = NULL;
         continue;
      }
      if (m_fRailExtend) {
         PCPoint p1;
         CPoint pInter, pBottom;
         TEXTUREPOINT tp;
         p1 = (PCPoint) lPath.Get(0);
         pBottom.Zero();
         m_psCenter->OrigTextureGet (0, &tp);
         pBottom.p[2] = tp.h;
         if (IntersectLinePlane (p1 + 1, p1, &pBottom, &pUp, &pInter)) {
            p1->Copy (&pInter);
            pn->BevelSet (TRUE, 2, &pUp);
         }
         else
            pn->BevelSet (TRUE, 0, &pUp);
         p1 = (PCPoint) lPath.Get(lPath.Num()-1);
         pBottom.Zero();
         m_psCenter->OrigTextureGet (m_psCenter->OrigNumPointsGet()-1, &tp);
         pBottom.p[2] = tp.h - m_fTreadThickness;
         if (IntersectLinePlane (p1 - 1, p1, &pBottom, &pUp, &pInter)) {
            p1->Copy (&pInter);
            pn->BevelSet (FALSE, 2, &pUp);
         }
         else
            pn->BevelSet (FALSE, 0, &pUp);
      }
      else {
         pn->BevelSet (TRUE, 0, &pUp);
         pn->BevelSet (FALSE, 0, &pUp);
      }


      // path
      pn->PathSpline (FALSE, lPath.Num(), (PCPoint) lPath.Get(0), (DWORD*)SEGCURVE_LINEAR, 0);

      // set the shape and stuff
      pn->DrawEndsSet (TRUE);
      pn->FrontVector (&pUp);
      CPoint pOffset;
      pOffset.Zero();
      pOffset.p[1] = m_pRailSize.v / 2.0 + m_pRailOffset.v;
      //if (dwRail != 1)
      //   pOffset.p[0] = m_pRailOffset.h * ((dwRail == 0) ? 1 : -1);
      pn->OffsetSet (&pOffset);
      CPoint pScale;
      pScale.Zero();
      pScale.p[0] = m_pRailSize.h;
      pScale.p[1] = m_pRailSize.v;
      pn->ScaleVector (&pScale);
      pn->ShapeDefault (m_adwRailShape[dwRail]);

   } // over walls
}

/**************************************************************************************
CObjectStairs::CalcTread - Looks at m_sCenter, m_sLeft, and m_sRight to calculate
wehre the treads go and fills in m_lTREADINFO.

inputs
   BOOL     fRedoAll - If TRUE then redo the walls, everything. Otherwise, just the treads
*/
void CObjectStairs::CalcTread (BOOL fRedoAll)
{
   m_lTREADINFO.Clear();

   // loop through all the points
   DWORD dwNum;
   dwNum = m_psLeft->OrigNumPointsGet();

   // will raise the balustrade in some places in order to keep level with squished
   // stairs. So, create a raise balustrade list
   CListFixed lRaise;
   lRaise.Init (sizeof(fp));
   fp fTemp;
   DWORD i;
   fTemp = 0.0;   // start out without raising
   lRaise.Required (dwNum);
   for (i = 0; i < dwNum; i++)
      lRaise.Add (&fTemp);
   fp *pfRaise;
   pfRaise = (fp*) lRaise.Get(0);

   // keep track of control points to insert before. Do this so that if
   // there's an end of a landing and a start of another rise, stick an empty
   // set of control points (for the balustrade) there so that get proper jump-up
   // in the handrail
   CListFixed lInsert;
   lInsert.Init (sizeof(BOOL));
   BOOL fbTemp;
   fbTemp = FALSE;
   lInsert.Required (dwNum);
   for (i = 0; i < dwNum; i++)
      lInsert.Add (&fbTemp);
   BOOL *pfInsert;
   pfInsert = (BOOL*) lInsert.Get(0);

   DWORD dwStart;
   TREADINFO ti;
   for (dwStart = 0; dwStart+1 < dwNum; ) {
      // if this is flat then just add one tread
      CPoint p1, p2;
      TEXTUREPOINT tp;
      m_psLeft->OrigPointGet (dwStart, &p1);
      // BUGFIX - Need to use .h as the z coord otherwise doesn't do curves properly
      m_psLeft->OrigTextureGet (dwStart, &tp);
      p1.p[2] = tp.h;
      m_psLeft->OrigPointGet (dwStart+1, &p2);
      m_psLeft->OrigTextureGet (dwStart+1, &tp);
      p2.p[2] = tp.h;
      if (fabs(p1.p[2] - p2.p[2]) < CLOSE) {
         // it's flat
         memset (&ti, 0, sizeof(ti));
         m_psLeft->OrigPointGet (dwStart, &ti.aCorner[0][0]);
         m_psLeft->OrigTextureGet (dwStart, &tp);
         ti.aCorner[0][0].p[2] = tp.h;

         m_psLeft->OrigPointGet (dwStart+1, &ti.aCorner[0][1]);
         m_psLeft->OrigTextureGet (dwStart+1, &tp);
         ti.aCorner[0][1].p[2] = tp.h;

         m_psRight->OrigPointGet (dwStart, &ti.aCorner[1][0]);
         m_psRight->OrigTextureGet (dwStart, &tp);
         ti.aCorner[1][0].p[2] = tp.h;

         m_psRight->OrigPointGet (dwStart+1, &ti.aCorner[1][1]);
         m_psRight->OrigTextureGet (dwStart+1, &tp);
         ti.aCorner[1][1].p[2] = tp.h;


         ti.fFlatSection = TRUE;

         m_lTREADINFO.Add (&ti);
         dwStart++;
         continue;
      }

      // NOTE: Assuming that heights of left and right are the same

      // find out how far go until get to the end or it's flat
      DWORD dwEnd;
      for (dwEnd = dwStart+1; dwEnd+1 < dwNum; dwEnd++) {
         // is it flat?
         m_psLeft->OrigPointGet (dwEnd, &p1);
         m_psLeft->OrigTextureGet (dwEnd, &tp);
         p1.p[2] = tp.h;

         m_psLeft->OrigPointGet (dwEnd+1, &p2);
         m_psLeft->OrigTextureGet (dwEnd+1, &tp);
         p2.p[2] = tp.h;

         if (fabs(p1.p[2] - p2.p[2]) < CLOSE) {
            break;
         }
      }  // dwEnd

      // if we're not at 0, then this is after a riser, so insert extra balustrade
      // bits before this
      if (dwStart)
         pfInsert[dwStart] = TRUE;

      // find the height delta
      fp fStart, fEnd, fSteps;
      m_psLeft->OrigPointGet (dwStart, &p1);
      m_psLeft->OrigTextureGet (dwStart, &tp);
      p1.p[2] = tp.h;
      m_psLeft->OrigPointGet (dwEnd, &p2);
      m_psLeft->OrigTextureGet (dwEnd, &tp);
      p2.p[2] = tp.h;
      fStart = p1.p[2];
      fEnd = p2.p[2];

      // BUGFIX - create a sqush factor so bottom step isn't right on the ground,
      // but is one step up
      fp fOrigStart, fUnsquishMult, fUnsquishAdd;
      fOrigStart = fStart;
      fStart += m_fRiser;
      if (fStart+CLOSE < fEnd) {
         fUnsquishMult = (fEnd - fOrigStart) / (fEnd - fStart);
      }
      else {
         fStart = fEnd;
         fUnsquishMult = 1;
      }
      fUnsquishAdd = fOrigStart - fStart * fUnsquishMult;

      
      // remember the raising
      for (i = dwStart; i < dwEnd; i++)
         pfRaise[i] = m_fRiser * (1.0 - (fp) (i-dwStart) / (fp) (dwEnd - dwStart));

      fSteps = (fEnd - fStart) / max(m_fRiser, .01);
      fSteps = ceil(fSteps);
      if (fSteps <= 0)
         fSteps = 1;
      
      // number of steps
      // no step  on the top, but one on the bottom
      DWORD dwNumSteps;
      dwNumSteps = (DWORD) fSteps;

      // loop over the steps
      for (i = 0; i < dwNumSteps; i++) {
         fp fLook, fNext;
         fLook = fStart + (fp) i / (fp)dwNumSteps * (fEnd - fStart);
         fNext = fStart + (fp) (i+1) / (fp)dwNumSteps * (fEnd - fStart);

         // unquish it
         fp fLookUn, fNextUn;
         fLookUn = fLook * fUnsquishMult + fUnsquishAdd;
         fNextUn = fNext * fUnsquishMult + fUnsquishAdd;

         fp fvl, fvr, fvln, fvrn;
         fvl = FindValue (m_psLeft, fLookUn, dwStart, dwEnd+1);
         fvr = FindValue (m_psRight, fLookUn, dwStart, dwEnd+1);
         fvln = FindValue (m_psLeft, fNextUn, dwStart, dwEnd+1);
         fvrn = FindValue (m_psRight, fNextUn, dwStart, dwEnd+1);

         // BUGFIX - Make sure fvl and fvr in range, so bottom tread OK
         fvl = max(0,fvl);
         fvr = max(0,fvr);

         // BUGFIX - Make sure not > 1
         fvln = min(1,fvln);
         fvrn = min(1,fvrn);

         // fill in the tread
         memset (&ti, 0, sizeof(ti));
         m_psLeft->LocationGet (fvl, &ti.aCorner[0][0]);
         ti.aCorner[0][0].p[2] = fLook;

         m_psLeft->LocationGet (fvln, &ti.aCorner[0][1]);
         ti.aCorner[0][1].p[2] = fLook;

         m_psRight->LocationGet (fvr, &ti.aCorner[1][0]);
         ti.aCorner[1][0].p[2] = fLook;

         m_psRight->LocationGet (fvrn, &ti.aCorner[1][1]);
         ti.aCorner[1][1].p[2] = fLook;


         m_lTREADINFO.Add (&ti);
      }  // dwNumSteps

      // next start
      dwStart = dwEnd;

   }  // dwStart

   // go through all the treads and do extension of whatnot
   for (i = 0; i < m_lTREADINFO.Num(); i++) {
      PTREADINFO pti = (PTREADINFO) m_lTREADINFO.Get(i);

      memcpy (pti->aExtended, pti->aCorner, sizeof(pti->aCorner));

      // tread depth
      if (!pti->fFlatSection && (m_fTreadExtendBack || m_fTreadMinDepth)) {
         CPoint pLeft, pRight;
         fp fLeft, fRight;
         pLeft.Subtract (&pti->aExtended[0][1], &pti->aExtended[0][0]);
         pRight.Subtract (&pti->aExtended[1][1], &pti->aExtended[1][0]);
         fLeft = pLeft.Length();
         fRight = pRight.Length();
         if (fLeft)
            pLeft.Scale (1.0 / fLeft);
         if (fRight)
            pRight.Scale (1.0 / fRight);

         fLeft = max(m_fTreadMinDepth, fLeft + m_fTreadExtendBack);
         fRight = max(m_fTreadMinDepth, fRight + m_fTreadExtendBack);

         // scale again
         pLeft.Scale (fLeft);
         pRight.Scale (fRight);
         pti->aExtended[0][1].Add (&pti->aExtended[0][0], &pLeft);
         pti->aExtended[1][1].Add (&pti->aExtended[1][0], &pRight);
      }

      // calculate the kickboard
      CPoint apNext[2][2]; // where the next tread if
      if (i+1 < m_lTREADINFO.Num())
         memcpy (apNext, &pti[1].aCorner, sizeof(apNext));
      else {
         // make it up by getting the top of the stairs
         DWORD dwNum;
         dwNum = m_psLeft->OrigNumPointsGet();
         TEXTUREPOINT tp;
         m_psLeft->OrigPointGet (dwNum-1, &apNext[0][0]);
         m_psLeft->OrigTextureGet (dwNum-1, &tp);
         apNext[0][0].p[2] = tp.h;
         m_psRight->OrigPointGet (dwNum-1, &apNext[1][0]);
         m_psRight->OrigTextureGet (dwNum-1, &tp);
         apNext[1][0].p[2] = tp.h;

         // know up, so can calc forwards
         CPoint pRight, pUp, pFront;
         pRight.Subtract (&apNext[1][0], &apNext[0][0]);
         pUp.Zero();
         pUp.p[2] = 1;
         pFront.CrossProd (&pUp, &pRight);
         pFront.Normalize();
         apNext[0][1].Add (&apNext[0][0], &pFront);
         apNext[1][1].Add (&apNext[1][0], &pFront);
      }
      // lower by thickness
      DWORD x, y;
      for (x = 0; x < 2; x++) for (y = 0; y < 2; y++)
         apNext[x][y].p[2] -= m_fTreadThickness;
      if (apNext[0][0].p[2] > pti->aExtended[0][0].p[2]) {
         // actually steps up so put in kickboard
         pti->fKickboardValid = TRUE;

         // move next in by specified amount
         CPoint pLeft, pRight;
         pLeft.Subtract (&apNext[0][1], &apNext[0][0]);
         pRight.Subtract (&apNext[1][1], &apNext[1][0]);
         pLeft.Normalize();
         pRight.Normalize();
         pLeft.Scale (m_fKickboardIndent);
         pRight.Scale (m_fKickboardIndent);
         apNext[0][0].Add (&pLeft);
         apNext[1][0].Add (&pRight);

         // use this to generate kickbord
         pti->aKickboard[0][1].Copy (&apNext[0][0]);
         pti->aKickboard[1][1].Copy (&apNext[1][0]);
         pti->aKickboard[0][0].Copy (&pti->aExtended[0][1]);
         pti->aKickboard[1][0].Copy (&pti->aExtended[1][1]);
      }
      else
         pti->fKickboardValid = FALSE;

      // extend to right or left
      // NOTE: Need to extend AFTER the kickboard
      if (!pti->fFlatSection && (m_fTreadExtendLeft || m_fTreadExtendRight)) {
         CPoint pFront, pBack, pExt;
         pFront.Subtract (&pti->aExtended[1][0], &pti->aExtended[0][0]);
         pBack.Subtract (&pti->aExtended[1][1], &pti->aExtended[0][1]);
         pFront.Normalize();
         pBack.Normalize();

         // to right
         pExt.Copy (&pFront);
         pExt.Scale (m_fTreadExtendRight);
         pti->aExtended[1][0].Add (&pExt);
         pExt.Copy (&pBack);
         pExt.Scale (m_fTreadExtendRight);
         pti->aExtended[1][1].Add (&pExt);

         // to left
         pExt.Copy (&pFront);
         pExt.Scale (-m_fTreadExtendLeft);
         pti->aExtended[0][0].Add (&pExt);
         pExt.Copy (&pBack);
         pExt.Scale (-m_fTreadExtendLeft);
         pti->aExtended[0][1].Add (&pExt);
      }

      // tread lip
      memcpy (pti->aLip, pti->aExtended, sizeof(pti->aLip));
      // move up/down
      for (x = 0; x < 2; x++) for (y = 0; y < 2; y++)
         pti->aLip[x][y].p[2] += m_fTreadLipOffset;
      // move next in by specified amount
      CPoint pLeft, pRight;
      pLeft.Subtract (&pti->aLip[0][1], &pti->aLip[0][0]);
      pRight.Subtract (&pti->aLip[1][1], &pti->aLip[1][0]);
      pLeft.Normalize();
      pRight.Normalize();
      // just in case rounding a corner
      if (pLeft.Length() < EPSILON)
         pLeft.Copy (&pRight);
      else if (pRight.Length() < EPSILON)
         pRight.Copy (&pLeft);
      pLeft.Scale (m_fTreadLipDepth);
      pRight.Scale (m_fTreadLipDepth);
      pti->aLip[0][1].Add (&pti->aLip[0][0], &pLeft);
      pti->aLip[1][1].Add (&pti->aLip[1][0], &pRight);

      // move the front out just a bit so some of the tread is hidden
      pLeft.Scale (.01);
      pRight.Scale (.01);
      pti->aLip[0][0].Subtract (&pLeft);
      pti->aLip[1][0].Subtract (&pRight);

   }

   // bottom kickboard
   
   if (m_lTREADINFO.Num()) {
      PTREADINFO pti = (PTREADINFO) m_lTREADINFO.Get(0);

      // find out direction of left and right sides
      CPoint pLeft, pRight, pl, pr;
      pLeft.Subtract (&pti->aCorner[0][1], &pti->aCorner[0][0]);
      pRight.Subtract (&pti->aCorner[1][1], &pti->aCorner[1][0]);
      pLeft.Normalize();
      pRight.Normalize();
      pl.Copy (&pLeft);
      pr.Copy (&pRight);

      // back (aka top) of the kickboard
      m_apBottomKick[0][1].Copy (&pti->aCorner[0][0]);
      m_apBottomKick[1][1].Copy (&pti->aCorner[1][0]);
      m_apBottomKick[0][1].p[2] -= m_fTreadThickness;
      m_apBottomKick[1][1].p[2] -= m_fTreadThickness;
      pLeft.Scale (m_fKickboardIndent);
      pRight.Scale (m_fKickboardIndent);
      m_apBottomKick[0][1].Add (&pLeft);
      m_apBottomKick[1][1].Add (&pRight);

      // front (aka bottom) of the kickboard
      m_apBottomKick[0][0].Copy (&pti->aCorner[0][0]);
      m_apBottomKick[1][0].Copy (&pti->aCorner[1][0]);
      m_apBottomKick[0][0].p[2] = m_apBottomKick[1][0].p[2] = 0;  // go to ground level
      pl.Scale (m_fTreadExtendBack);
      pr.Scale (m_fTreadExtendBack);
      m_apBottomKick[0][0].Add (&pLeft);
      m_apBottomKick[1][0].Add (&pRight);
   }

   if (!fRedoAll)
      return;

   CreateWalls ();
   CreateRails ();

   // do the balustrades now
   for (i = 0; i < 3; i++) {
      BOOL fJustCreated = FALSE;

      if (!m_afBalWant[i] && m_apBal[i]) {
         m_apBal[i]->ClaimClear();
         delete m_apBal[i];
         m_apBal[i] = NULL;
         continue;
      }
      if (!m_afBalWant[i])
         continue;

      // if get here want balustrade
      if (!m_apBal[i]) {
         m_apBal[i] = new CBalustrade;
         fJustCreated = TRUE;
         if (!m_apBal[i])
            continue;
         if (i == 2)
            m_apBal[i]->m_fSwapSides = TRUE;

      }

      PCSpline ps;
      ps = ((i == 0) ? m_psLeft : ((i==1) ? m_psCenter : m_psRight));

      // make the splines
      CListFixed lPoints, lCurve, lRaiseClone;
      lPoints.Init (sizeof(CPoint));
      lCurve.Init (sizeof(DWORD));
      lPoints.Clear();
      lCurve.Clear();
      lRaiseClone.Init (sizeof(fp), lRaise.Get(0), lRaise.Num());

      CSpline sBottom, sTop;
      DWORD dwNum;
      dwNum = ps->OrigNumPointsGet();
      DWORD j;
      CPoint p;
      DWORD dw;
      lCurve.Required (dwNum);
      lPoints.Required (dwNum);
      for (j = 0; j < dwNum; j++) {
         TEXTUREPOINT tp;
         ps->OrigSegCurveGet (j, &dw);
         lCurve.Add (&dw);

         ps->OrigTextureGet (j, &tp);
         ps->OrigPointGet (j, &p);
         p.p[2] = tp.h;
         lPoints.Add (&p);
      }

      // insert balustrade control points if jump up
      // NOTE: Can't insert before the first element, j == 0
      for (j = lInsert.Num()-1; j && (j < lInsert.Num()); j--) {
         if (!pfInsert[j])
            continue;

         // if get here want to insert before this one
         PCPoint pap;
         DWORD *pdw;
         fp f;
         pap = (PCPoint) lPoints.Get(0);
         pdw = (DWORD*) lCurve.Get(0);
         pfRaise = (fp*) lRaiseClone.Get(0);

         p.Copy (&pap[j]);
         dw = SEGCURVE_LINEAR;
         f = pfRaise[j-1]; // note that using the old raise
         lPoints.Insert (j, &p);
         lCurve.Insert (j, &dw);
         lRaiseClone.Insert (j, &f);
      }

      // raise all the points
      PCPoint pap;
      pfRaise = (fp*) lRaiseClone.Get(0);
      pap = (PCPoint) lPoints.Get(0);
      for (j = 0; j < lPoints.Num(); j++)
         pap[j].p[2] += pfRaise[j];

      DWORD dwMin, dwMax;
      fp fDetail;
      ps->DivideGet (&dwMin, &dwMax, &fDetail);
      sBottom.Init (ps->LoopedGet(), lPoints.Num(), (PCPoint)lPoints.Get(0), NULL,
         (DWORD*) lCurve.Get(0), dwMin, dwMax, fDetail);

      // raise it by 1 meter and set that
      for (j = 0; j < lPoints.Num(); j++)
         pap[j].p[2] += 1.0;
      sTop.Init (ps->LoopedGet(), lPoints.Num(), (PCPoint)lPoints.Get(0), NULL,
         (DWORD*) lCurve.Get(0), dwMin, dwMax, fDetail);


      // initalize
      if (fJustCreated)
         m_apBal[i]->Init (m_OSINFO.dwRenderShard, 0, this, &sBottom, &sTop, FALSE);
      else
         m_apBal[i]->NewSplines (&sBottom, &sTop);

      m_apBal[i]->m_fForceLevel = FALSE;  // dont force level when dragging

   } // over balustrades
}

/**************************************************************************************
CObjectStairs::CalcLRC - Based on the m_fLRAuto flag, it caluclates either
m_sLeft and m_sRight (if m_LRAuto true), or m_sCenter (if m_lRAuto false).
*/
void CObjectStairs::CalcLRC (void)
{
   if (m_fLRAuto) {
      if (m_psLeft)
         delete m_psLeft;
      if (m_psRight)
         delete m_psRight;
      m_psLeft = m_psCenter->Expand (m_fWidth/2.0);
      m_psRight = m_psCenter->Expand (-m_fWidth/2.0);
   }
   else {
      if (!m_psCenter)
         m_psCenter = new CSpline;
      if (!m_psCenter)
         return;

      CMem  memPoints, memCurve, memTexture;
      DWORD dwNum;
      dwNum = m_psLeft->OrigNumPointsGet();
      if (!memPoints.Required (dwNum * sizeof(CPoint)))
         return;
      if (!memCurve.Required (dwNum * sizeof(DWORD)))
         return;
      if (!memTexture.Required (dwNum * sizeof(TEXTUREPOINT)))
         return;
      DWORD *padw;
      PCPoint pap;
      PTEXTUREPOINT ptp;
      pap = (PCPoint) memPoints.p;
      padw = (DWORD*) memCurve.p;
      ptp = (PTEXTUREPOINT) memTexture.p;
      DWORD i;
      for (i = 0; i < dwNum; i++) {
         m_psLeft->OrigSegCurveGet (i, padw + i);
         m_psLeft->OrigTextureGet (i, ptp + i);

         CPoint pl, pr;
         m_psLeft->OrigPointGet (i, &pl);
         m_psRight->OrigPointGet (i, &pr);
         pap[i].Average (&pl, &pr);
      }

      DWORD dwMin, dwMax;
      fp fDetail;
      m_psLeft->DivideGet (&dwMin, &dwMax, &fDetail);
      m_psCenter->Init (m_psLeft->LoopedGet(), dwNum, pap, ptp, padw, dwMin, dwMax, fDetail);
   }
}


/**************************************************************************************
CObjectStairs::CalcLRCFromPathType - Given a path type, this sets m_psCenter,
m_psRight, m_psLeft, m_fLRAuto, and m_pColumn.

inputs
   DWORD       dwType - SPATH_XXX
returns
   none
*/
void CObjectStairs::CalcLRFromPathType (DWORD dwType)
{
   CListFixed lLeft, lRight, lCenter, lCurve;
   CListFixed lZ;
   lLeft.Init (sizeof(CPoint));
   lRight.Init (sizeof(CPoint));
   lCenter.Init (sizeof(CPoint));
   lCurve.Init (sizeof(DWORD));

   CPoint pDirAtTop; // direction the stairs are heading when they reach the top- for landing
   pDirAtTop.Zero();

   CPoint p;
   DWORD dwCurve;
   dwCurve = SEGCURVE_LINEAR;

   switch (dwType) {
   default:
   case SPATH_STRAIGHT:     // straight up
      // info
      m_fLRAuto = FALSE;

      // bottom
      p.Zero();
      p.p[0] = -m_fLength/2.0;
      p.p[1] = m_fWidth/2.0;
      lLeft.Add (&p);
      p.p[1] *= -1;
      lRight.Add (&p);
      lCurve.Add (&dwCurve);

      // rest landing?
      if ((m_fRestLanding > 0.0) && (m_fRestLanding < m_fLength)) {
         p.p[0] = -m_fRestLanding/2.0;
         p.p[1] = m_fWidth/2.0;
         p.p[2] = m_fTotalRise / 2.0;
         lLeft.Add (&p);
         p.p[1] *= -1;
         lRight.Add (&p);
         lCurve.Add (&dwCurve);

         p.p[0] = m_fRestLanding/2.0;
         p.p[1] = m_fWidth/2.0;
         p.p[2] = m_fTotalRise / 2.0;
         lLeft.Add (&p);
         p.p[1] *= -1;
         lRight.Add (&p);
         lCurve.Add (&dwCurve);
      }

      // top
      p.p[0] = m_fLength/2.0;
      p.p[1] = m_fWidth/2.0;
      p.p[2] = m_fTotalRise;
      lLeft.Add (&p);
      p.p[1] *= -1;
      lRight.Add (&p);
      lCurve.Add (&dwCurve);

      // direction on top
      pDirAtTop.p[0] = 1;
      break;

   case SPATH_LANDINGTURN:     // up, turn right/left, and up some more
      // info
      m_fLRAuto = FALSE;

      // bottom steps
      p.Zero();
      p.p[0] = -(m_fLength + m_fWidth)/2.0;
      p.p[1] = m_fWidth/2.0;
      lLeft.Add (&p);
      p.p[1] *= -1;
      lRight.Add (&p);
      lCurve.Add (&dwCurve);

      p.p[0] = -m_fWidth/2.0;
      p.p[1] = m_fWidth/2.0;
      p.p[2] = m_fTotalRise/2.0;
      lLeft.Add (&p);
      p.p[1] *= -1;
      lRight.Add (&p);
      lCurve.Add (&dwCurve);

      // landing and rest up
      if (m_fPathClockwise) {
         // landing
         lRight.Add (&p);  // same point as before

         // new left point
         p.p[0] = m_fWidth/2.0;
         p.p[1] = m_fWidth/2.0;
         p.p[2] = m_fTotalRise/2.0;
         lLeft.Add (&p);

         lCurve.Add (&dwCurve);

         // rest of the staircase up
         p.p[0] = m_fWidth / 2.0;
         p.p[1] = -m_fWidth / 2.0;
         p.p[2] = m_fTotalRise/2.0;
         lLeft.Add (&p);
         p.p[0] *= -1;
         lRight.Add (&p);
         lCurve.Add (&dwCurve);

         p.p[0] = m_fWidth / 2.0;
         p.p[1] = -(m_fLength + m_fWidth)/2.0;;
         p.p[2] = m_fTotalRise;
         lLeft.Add (&p);
         p.p[0] *= -1;
         lRight.Add (&p);
         lCurve.Add (&dwCurve);
      }
      else {
         // landing
         lLeft.Add (&p);  // same point as before

         // new right point
         p.p[0] = m_fWidth/2.0;
         p.p[1] = -m_fWidth/2.0;
         p.p[2] = m_fTotalRise/2.0;
         lRight.Add (&p);

         lCurve.Add (&dwCurve);

         // rest of the staircase up
         p.p[0] = -m_fWidth / 2.0;
         p.p[1] = m_fWidth / 2.0;
         p.p[2] = m_fTotalRise/2.0;
         lLeft.Add (&p);
         p.p[0] *= -1;
         lRight.Add (&p);
         lCurve.Add (&dwCurve);

         p.p[0] = -m_fWidth / 2.0;
         p.p[1] = (m_fLength + m_fWidth)/2.0;;
         p.p[2] = m_fTotalRise;
         lLeft.Add (&p);
         p.p[0] *= -1;
         lRight.Add (&p);
         lCurve.Add (&dwCurve);
      }
      // direction on top
      pDirAtTop.p[1] = m_fPathClockwise ? -1 : 1;
      break;

   case SPATH_STAIRWELL:     // switches back and forth
      {
         m_fLRAuto = FALSE;

         // make sure have enough space
         fp fRestLanding = m_fRestLanding;
         fp fStairwellLanding = m_fStairwellLanding;
         fp fWidth = m_fWidth;
         if (m_fStairwellWidth < fStairwellLanding * 2 + fRestLanding)
            fRestLanding = m_fStairwellWidth - fStairwellLanding*2;
         fRestLanding = max(0,fRestLanding);
         if (m_fStairwellWidth < fStairwellLanding * 2 + fRestLanding)
            fStairwellLanding = (m_fStairwellWidth - fRestLanding) / 2;
         fStairwellLanding = max(0, fStairwellLanding);
         if (m_fStairwellLength < 2*fWidth)
            fWidth = m_fStairwellLength / 2.0;

         // how much go up per landing?
         fp fUpPerLanding;
         DWORD dwRiserLevels;
         fUpPerLanding = m_fLevelHeight;
         dwRiserLevels = m_dwRiserLevels;
         if (m_fStairwellTwoPerLevel) {
            fUpPerLanding /= 2.0;
            dwRiserLevels *= 2;
         }

         fp fRiserLen;
         fRiserLen = m_fStairwellWidth - fStairwellLanding * 2;

         DWORD i;
         for (i = 0; i < dwRiserLevels; i++) {
            // riser
            fp fHeight;
            fHeight = fUpPerLanding * i;

            if (i%2) {
               // on the near side, going to leaft
               p.p[0] = fRiserLen/2;
               p.p[1] = -m_fStairwellLength/2;
               p.p[2] = fHeight;
               lLeft.Add (&p);
               p.p[1] += fWidth;
               lRight.Add (&p);
               lCurve.Add (&dwCurve);

               // rest point
               if ((fRestLanding > 0.0) && (fRestLanding < m_fLength)) {
                  p.p[0] = fRestLanding/2.0;
                  p.p[1] = -m_fStairwellLength / 2;
                  p.p[2] = fHeight + fUpPerLanding/2.0;
                  lLeft.Add (&p);
                  p.p[1] += fWidth;
                  lRight.Add (&p);
                  lCurve.Add (&dwCurve);

                  p.p[0] = -fRestLanding/2;
                  p.p[1] = -m_fStairwellLength / 2;
                  p.p[2] = fHeight + fUpPerLanding/2.0;
                  lLeft.Add (&p);
                  p.p[1] += fWidth;
                  lRight.Add (&p);
                  lCurve.Add (&dwCurve);
               }

               // top
               fHeight += fUpPerLanding;
               p.p[0] = -fRiserLen/2;
               p.p[1] = -m_fStairwellLength / 2;
               p.p[2] = fHeight;
               lLeft.Add (&p);
               p.p[1] += fWidth;
               lRight.Add (&p);
               lCurve.Add (&dwCurve);
            }
            else {
               // on the far size going to right

               // bottom part
               p.p[0] = -fRiserLen/2;
               p.p[1] = m_fStairwellLength / 2;
               p.p[2] = fHeight;
               lLeft.Add (&p);
               p.p[1] -= fWidth;
               lRight.Add (&p);
               lCurve.Add (&dwCurve);

               // rest point
               if ((fRestLanding > 0.0) && (fRestLanding < m_fLength)) {
                  p.p[0] = -fRestLanding/2;
                  p.p[1] = m_fStairwellLength / 2;
                  p.p[2] = fHeight + fUpPerLanding/2.0;
                  lLeft.Add (&p);
                  p.p[1] -= fWidth;
                  lRight.Add (&p);
                  lCurve.Add (&dwCurve);

                  p.p[0] = fRestLanding/2;
                  p.p[1] = m_fStairwellLength / 2;
                  p.p[2] = fHeight + fUpPerLanding/2.0;
                  lLeft.Add (&p);
                  p.p[1] -= fWidth;
                  lRight.Add (&p);
                  lCurve.Add (&dwCurve);
               }
               // top
               fHeight += fUpPerLanding;
               p.p[0] = fRiserLen/2;
               p.p[1] = m_fStairwellLength / 2;
               p.p[2] = fHeight;
               lLeft.Add (&p);
               p.p[1] -= fWidth;
               lRight.Add (&p);
               lCurve.Add (&dwCurve);

            }

            // if no more then end here
            if (i + 1 >= dwRiserLevels) {
               pDirAtTop.p[0] = (i%2) ? -1 : 1;
               break;
            }

            if (i%2) {
               // landing on left side
               // put in the landing
               lRight.Add (&p);  // keep right the same
               p.p[0] = -m_fStairwellWidth / 2.0;
               p.p[1] = -m_fStairwellLength / 2.0;
               p.p[2] = fHeight;
               lLeft.Add (&p);
               lCurve.Add (&dwCurve);

               p.p[0] = -fRiserLen/2;
               p.p[1] = (m_fStairwellLength / 2.0) - fWidth;
               p.p[2] = fHeight;
               lRight.Add (&p);
               p.p[0] = -m_fStairwellWidth/2;
               p.p[1] = m_fStairwellLength/2;
               p.p[2] = fHeight;
               lLeft.Add (&p);
               lCurve.Add (&dwCurve);
            }
            else {
               // landing on right side
               // put in the landing
               lRight.Add (&p);  // keep right the same
               p.p[0] = m_fStairwellWidth / 2.0;
               p.p[1] = m_fStairwellLength / 2.0;
               p.p[2] = fHeight;
               lLeft.Add (&p);
               lCurve.Add (&dwCurve);

               p.p[0] = fRiserLen/2;
               p.p[1] = -(m_fStairwellLength / 2.0) + fWidth;
               p.p[2] = fHeight;
               lRight.Add (&p);
               p.p[0] = m_fStairwellWidth/2;
               p.p[1] = -m_fStairwellLength/2;
               p.p[2] = fHeight;
               lLeft.Add (&p);
               lCurve.Add (&dwCurve);
            }

         }  // riserlevels

         // if it's not clockwise then flip
         if (!m_fPathClockwise) {
            pDirAtTop.p[0] *= -1;   // flip

            PCPoint pLeft, pRight;
            pLeft = (PCPoint) lLeft.Get(0);
            pRight = (PCPoint) lRight.Get(0);
            for (i = 0; i < lLeft.Num(); i++) {
               // flip left and right directions
               pLeft[i].p[0] *= -1;
               pRight[i].p[0] *= -1;

               // swap meaning of left and right rail
               CPoint p;
               p.Copy (&pLeft[i]);
               pLeft[i].Copy (&pRight[i]);
               pRight[i].Copy (&p);
            }
         } // counterclockwise check

      }
      break;

   case SPATH_SPIRAL:     // spiral staircase
      {
         m_fLRAuto = FALSE;

         // width limit check
         fp fWidth = m_fWidth;
         if (fWidth + CLOSE > m_fSpiralRadius)
            fWidth = m_fSpiralRadius - CLOSE;

         // how many points split into?
         DWORD dwNum, i;
         dwNum = (DWORD) ceil(m_fSpiralRevolutions * 4.0);
         dwNum = (dwNum / 2) * 2;   // even number
         dwNum++;
         dwNum = max(3,dwNum);

         // loop
         for (i = 0; i < dwNum; i++) {
            fp fAngle = (fp) i / (fp) (dwNum-1) * m_fSpiralRevolutions * 2.0 * PI;
            fp fSin = sin(fAngle);
            fp fCos = cos(fAngle);
            CPoint pL, pR, pU;

            p.p[0] = -fCos * m_fSpiralRadius;
            p.p[1] = fSin * m_fSpiralRadius;
            p.p[2] = (fp)i / (fp)(dwNum-1) * m_fTotalRise;
            lLeft.Add (&p);
            pL.Copy (&p);

            p.p[0] = -fCos * (m_fSpiralRadius - fWidth);
            p.p[1] = fSin * (m_fSpiralRadius - fWidth);
            lRight.Add (&p);
            pR.Copy (&p);

            dwCurve = (i % 2) ? SEGCURVE_CIRCLEPREV : SEGCURVE_CIRCLENEXT;
            lCurve.Add (&dwCurve);

            // which way if forwards
            pL.Subtract (&pR);
            pU.Zero();
            pU.p[2] = 1;
            pDirAtTop.CrossProd (&pL, &pU);
            pDirAtTop.Normalize();
         }

         // if it's not clockwise then flip
         if (!m_fPathClockwise) {
            pDirAtTop.p[0] *= -1;   // flip

            PCPoint pLeft, pRight;
            pLeft = (PCPoint) lLeft.Get(0);
            pRight = (PCPoint) lRight.Get(0);
            for (i = 0; i < lLeft.Num(); i++) {
               // flip left and right directions
               pLeft[i].p[0] *= -1;
               pRight[i].p[0] *= -1;

               // swap meaning of left and right rail
               CPoint p;
               p.Copy (&pLeft[i]);
               pLeft[i].Copy (&pRight[i]);
               pRight[i].Copy (&p);
            }
         } // counterclockwise check
      }
      break;

   case SPATH_STAIRWELL2:     // stairscase inside (or outside) a rectangle
      {
         m_fLRAuto = FALSE;

         // width limit check
         fp fWidth = m_fWidth;
         if (fWidth *2 +CLOSE > m_fStairwellWidth)
            fWidth = m_fStairwellWidth / 2 - CLOSE;
         if (fWidth * 2 +CLOSE> m_fStairwellLength)
            fWidth = m_fStairwellLength / 2 - CLOSE;

         // length EW, and length NS
         fp fLenEW, fLenNS;
         fLenEW = m_fStairwellWidth - fWidth*2;
         fLenNS = m_fStairwellLength - fWidth*2;

         // rise in each EW and NS
         fp fRiseEW, fRiseNS;
         fRiseEW = m_fLevelHeight * fLenEW / (fLenEW + fLenNS) / 2;
         fRiseNS = m_fLevelHeight * fLenNS / (fLenEW + fLenNS) / 2;

         // draw it
         DWORD i;
         p.Zero();
         for (i = 0; i < m_dwRiserLevels; i++) {
            fp fHeight = (fp)i * m_fLevelHeight;

            // far north steps
            p.p[0] = -fLenEW/2;
            p.p[1] = m_fStairwellLength / 2;
            p.p[2] = fHeight;
            lLeft.Add (&p);
            p.p[1] -= fWidth;
            lRight.Add (&p);
            lCurve.Add (&dwCurve);

            fHeight += fRiseEW;
            p.p[0] = fLenEW/2;
            p.p[1] = m_fStairwellLength / 2;
            p.p[2] = fHeight;
            lLeft.Add (&p);
            p.p[1] -= fWidth;
            lRight.Add (&p);
            lCurve.Add (&dwCurve);

            // landing
            lRight.Add (&p);  // keep same
            p.p[0] = m_fStairwellWidth / 2;
            p.p[1] = m_fStairwellLength / 2;
            lLeft.Add (&p);
            lCurve.Add (&dwCurve);

            // far east steps
            p.p[0] = m_fStairwellWidth / 2;
            p.p[1] = fLenNS/2;
            p.p[2] = fHeight;
            lLeft.Add(&p);
            p.p[0] -= fWidth;
            lRight.Add (&p);
            lCurve.Add (&dwCurve);

            fHeight += fRiseNS;
            p.p[0] = m_fStairwellWidth / 2;
            p.p[1] = -fLenNS/2;
            p.p[2] = fHeight;
            lLeft.Add(&p);
            p.p[0] -= fWidth;
            lRight.Add (&p);
            lCurve.Add (&dwCurve);

            // landing
            lRight.Add (&p);  // keep same
            p.p[0] = m_fStairwellWidth / 2;
            p.p[1] = -m_fStairwellLength / 2;
            lLeft.Add (&p);
            lCurve.Add (&dwCurve);

            // south steps
            p.p[0] = fLenEW/2;
            p.p[1] = -m_fStairwellLength / 2;
            p.p[2] = fHeight;
            lLeft.Add (&p);
            p.p[1] += fWidth;
            lRight.Add (&p);
            lCurve.Add (&dwCurve);

            fHeight += fRiseEW;
            p.p[0] = -fLenEW/2;
            p.p[1] = -m_fStairwellLength / 2;
            p.p[2] = fHeight;
            lLeft.Add (&p);
            p.p[1] += fWidth;
            lRight.Add (&p);
            lCurve.Add (&dwCurve);
            
            // SW landing
            lRight.Add (&p);  // keep same
            p.p[0] = -m_fStairwellWidth / 2;
            p.p[1] = -m_fStairwellLength / 2;
            lLeft.Add (&p);
            lCurve.Add (&dwCurve);

            // west steps
            p.p[0] = -m_fStairwellWidth / 2;
            p.p[1] = -fLenNS/2;
            p.p[2] = fHeight;
            lLeft.Add(&p);
            p.p[0] += fWidth;
            lRight.Add (&p);
            lCurve.Add (&dwCurve);

            fHeight += fRiseNS;
            p.p[0] = -m_fStairwellWidth / 2;
            p.p[1] = fLenNS/2;
            p.p[2] = fHeight;
            lLeft.Add(&p);
            p.p[0] += fWidth;
            lRight.Add (&p);
            lCurve.Add (&dwCurve);

            // NW landing
            if (i+1 < m_dwRiserLevels) {
               lRight.Add (&p);  // keep same
               p.p[0] = -m_fStairwellWidth / 2;
               p.p[1] = m_fStairwellLength / 2;
               lLeft.Add (&p);
               lCurve.Add (&dwCurve);
            }
         }

         pDirAtTop.p[1] = 1;

         // if it's not clockwise then flip
         if (!m_fPathClockwise) {
            pDirAtTop.p[0] *= -1;   // flip

            PCPoint pLeft, pRight;
            pLeft = (PCPoint) lLeft.Get(0);
            pRight = (PCPoint) lRight.Get(0);
            for (i = 0; i < lLeft.Num(); i++) {
               // flip left and right directions
               pLeft[i].p[0] *= -1;
               pRight[i].p[0] *= -1;

               // swap meaning of left and right rail
               CPoint p;
               p.Copy (&pLeft[i]);
               pLeft[i].Copy (&pRight[i]);
               pRight[i].Copy (&p);
            }
         } // counterclockwise check
      }
      break;

   case SPATH_WINDING:     // just to show that can do winding
      m_fLRAuto = TRUE;
      pDirAtTop.p[0] = 1;

      p.Zero();
      p.p[0] = -m_fLength/2;
      p.p[2] = m_fTotalRise * 0;
      lCenter.Add (&p);
      dwCurve = SEGCURVE_LINEAR;
      lCurve.Add (&dwCurve);

      p.p[0] = -m_fLength/2 + m_fLength * .1;
      p.p[2] = m_fTotalRise * .1;
      lCenter.Add (&p);
      dwCurve = SEGCURVE_CUBIC;
      lCurve.Add (&dwCurve);

      p.p[0] = -m_fLength/2 + m_fLength * .3;
      p.p[1] = m_fLength / 8;
      p.p[2] = m_fTotalRise * .3;
      lCenter.Add (&p);
      lCurve.Add (&dwCurve);

      p.p[0] = -m_fLength/2 + m_fLength * .7;
      p.p[1] = -m_fLength / 8;
      p.p[2] = m_fTotalRise * .7;
      lCenter.Add (&p);
      lCurve.Add (&dwCurve);

      p.p[0] = -m_fLength/2 + m_fLength * .9;
      p.p[1] = 0;
      p.p[2] = m_fTotalRise * .9;
      lCenter.Add (&p);
      lCurve.Add (&dwCurve);

      p.p[0] = m_fLength/2;
      p.p[2] = m_fTotalRise;
      lCenter.Add (&p);
      dwCurve = SEGCURVE_LINEAR;
      lCurve.Add (&dwCurve);

      break;

   case SPATH_CUSTOMCENTER:     // custom path with only the center controled
      m_fLRAuto = TRUE;
      goto skipstuff;

   case SPATH_CUSTOMLR:     // custom path with left/right controlled
      m_fLRAuto = FALSE;
      goto skipstuff;
   }

   // landing at the top
   if (m_fLandingTop > 0.0) {

      // mark the last curve as being linear
      DWORD *pdw;
      pdw = (DWORD*) lCurve.Get(lCurve.Num()-1);
      *pdw = SEGCURVE_LINEAR;

      p.Zero();
      dwCurve = SEGCURVE_LINEAR;

      // figure out right
      CPoint pRight, pUp;
      pUp.Zero();
      pUp.p[2] = 1;
      pDirAtTop.Normalize();
      pRight.CrossProd (&pDirAtTop, &pUp);
      pRight.Normalize();

      if (m_fLRAuto) {
         PCPoint pLast;
         pLast = (PCPoint)lCenter.Get (lCenter.Num()-1);

         switch (m_iLandingTopDir) {
         case -1: //left
         case 1:  // right
            // straight ahead 1/2 width
            p.Copy (&pDirAtTop);
            p.Scale (.5 * m_fWidth);
            p.Add (pLast);
            lCenter.Add (&p);
            lCurve.Add (&dwCurve);
            pUp.Copy (&p);

            // left
            p.Copy (&pRight);
            p.Scale (m_fLandingTop * (fp) m_iLandingTopDir);
            p.Add (&pUp);
            lCenter.Add (&p);
            lCurve.Add (&dwCurve);
            break;
         default: // straight
            // straight ahead
            p.Copy (&pDirAtTop);
            p.Scale (m_fLandingTop);
            p.Add (pLast);
            lCenter.Add (&p);
            lCurve.Add (&dwCurve);
            break;
         }
      }
      else {
         CPoint pLastLeft, pLastRight;
         pLastLeft.Copy ((PCPoint)lLeft.Get (lLeft.Num()-1));
         pLastRight.Copy ((PCPoint)lRight.Get (lRight.Num()-1));

         switch (m_iLandingTopDir) {
         case -1: //left
            // add the bend
            lLeft.Add (&pLastLeft);
            pUp.Copy (&pDirAtTop);
            pUp.Scale (m_fWidth);
            pLastRight.Add (&pUp);
            lRight.Add (&pLastRight);
            lCurve.Add (&dwCurve);

            // rest of way to left
            pUp.Copy (&pRight);
            pUp.Scale (-m_fLandingTop);
            pLastLeft.Add (&pUp);
            lLeft.Add (&pLastLeft);
            pUp.Copy (&pRight);
            pUp.Scale (-(m_fLandingTop + m_fWidth));
            pLastRight.Add (&pUp);
            lRight.Add (&pLastRight);
            lCurve.Add (&dwCurve);
            break;
         case 1:  // right
            // add the bend
            lRight.Add (&pLastRight);
            pUp.Copy (&pDirAtTop);
            pUp.Scale (m_fWidth);
            pLastLeft.Add (&pUp);
            lLeft.Add (&pLastLeft);
            lCurve.Add (&dwCurve);

            // rest of way to right
            pUp.Copy (&pRight);
            pUp.Scale (m_fLandingTop);
            pLastRight.Add (&pUp);
            lRight.Add (&pLastRight);
            pUp.Copy (&pRight);
            pUp.Scale (m_fLandingTop + m_fWidth);
            pLastLeft.Add (&pUp);
            lLeft.Add (&pLastLeft);
            lCurve.Add (&dwCurve);
            break;
         default: // straight
            // straight ahead
            pUp.Copy (&pDirAtTop);
            pUp.Scale (m_fLandingTop);
            pLastLeft.Add (&pUp);
            pLastRight.Add (&pUp);
            lLeft.Add (&pLastLeft);
            lRight.Add (&pLastRight);
            lCurve.Add (&dwCurve);
            break;
         }
      }
   }

   // allocate the texture points and take the z's
   lZ.Init (sizeof(TEXTUREPOINT));
   PCPoint pLeft, pRight, pCenter;
   DWORD i;
   pLeft = (PCPoint) lLeft.Get(0);
   pRight = (PCPoint) lRight.Get(0);
   pCenter = (PCPoint) lCenter.Get(0);
   TEXTUREPOINT tp;
   for (i = 0; i < lCurve.Num(); i++) {
      // get the Z - which is same for left and right
      tp.h = m_fLRAuto ? pCenter[i].p[2] : pLeft[i].p[2];
      tp.v = 0;
      lZ.Add (&tp);

      // erase z's
      if (m_fLRAuto)
         pCenter[i].p[2] = 0;
      else
         pLeft[i].p[2] = pRight[i].p[2] = 0;
   }

   // which was supplied
   if (m_fLRAuto) {
      // supplied center
      if (!m_psCenter)
         m_psCenter = new CSpline;
      if (!m_psCenter)
         return;
      m_psCenter->Init (FALSE, lCenter.Num(), (PCPoint) lCenter.Get(0), (PTEXTUREPOINT) lZ.Get(0),
         (DWORD*) lCurve.Get(0), 0, 3, .1);
   }
   else {
      // supplied left and right
      if (!m_psRight)
         m_psRight = new CSpline;
      if (!m_psLeft)
         m_psLeft = new CSpline;
      if (!m_psLeft || !m_psRight)
         return;
      m_psLeft->Init (FALSE, lLeft.Num(), (PCPoint) lLeft.Get(0), (PTEXTUREPOINT) lZ.Get(0),
         (DWORD*) lCurve.Get(0), 0, 3, .1);
      m_psRight->Init (FALSE, lRight.Num(), (PCPoint) lRight.Get(0), (PTEXTUREPOINT) lZ.Get(0),
         (DWORD*) lCurve.Get(0), 0, 3, .1);
   }

skipstuff:
   // do the other one
   CalcLRC ();

   // Add/remove column as necessary
   if (m_fSpiralPost && (dwType == SPATH_SPIRAL)) {
      if (!m_pColumn)
         m_pColumn = new CColumn;
      if (m_pColumn) {
         CPoint pBottom, pTop, pFront, pSize;
         pBottom.Zero();
         pTop.Zero();
         pTop.p[2] = m_fSpiralPostHeight + m_fTotalRise;
         pFront.Zero();
         pFront.p[0] = 1;
         m_pColumn->StartEndFrontSet (&pBottom, &pTop, &pFront);
         m_pColumn->ShapeSet (NS_CIRCLE);

         pSize.Zero();
         pSize.p[0] = pSize.p[1] = m_fSpiralPostDiameter;
         m_pColumn->SizeSet (&pSize);
      }
   }
   else {
      if (m_pColumn)
         delete m_pColumn;
      m_pColumn = NULL;
   }
}


/*************************************************************************************
CObjectStairs::Deconstruct -
Tells the object to deconstruct itself into sub-objects.
Basically, new objects will be added that exactly mimic this object,
and any embedeeding objects will be moved to the new ones.
NOTE: THIS CALL DOES NOT DELETE THE OLD ONE.
If fAct is FALSE the function is just a query, that returns
TRUE if the object cares about adjustment and can try, FALSE if it can't.

NOTE: Often overridden.
*/
BOOL CObjectStairs::Deconstruct (BOOL fAct)
{
   if (!m_pWorld)
      return FALSE;
   if (!fAct)
      return TRUE;

   // first off, create a clone of self, BUT get rid of the handrails, columns, and walls
   PCObjectStairs pos;
   pos = CloneStairs ();
   if (pos) {
      m_pWorld->ObjectAdd (pos);

      m_pWorld->ObjectAboutToChange (pos);

      memset (pos->m_afBalWant, 0, sizeof(pos->m_afBalWant));
      pos->m_fSpiralPost = FALSE;
      memset (pos->m_afWallWant, 0, sizeof(pos->m_afWallWant));

      pos->CalcLRFromPathType (pos->m_dwPathType);
      pos->CalcTread();

      m_pWorld->ObjectChanged (pos);
   }

   // clone the post
   PCObjectColumn pop;
   pop = NULL;
   DWORD j;
   if (m_pColumn) {
      OSINFO OI;
      memset (&OI, 0, sizeof(OI));
      OI.gCode = CLSID_Column;
      //OI.gSub = GUID_NULL;
      OI.dwRenderShard = m_OSINFO.dwRenderShard;
      pop = new CObjectColumn((PVOID)0, &OI);
   }
   if (pop) {
      // BUGFIX - Set a name
      pop->StringSet (OSSTRING_NAME, L"Stairway column");

      m_pWorld->ObjectAdd (pop);

      m_pWorld->ObjectAboutToChange (pop);

      m_pColumn->CloneTo (&pop->m_Column);

      // transfer colors
      for (j = 0; j < 5; j++) {
         PCObjectSurface pc;
         pc = ObjectSurfaceFind (2);
         if (!pc)
            continue;

         pop->ObjectSurfaceRemove (j);
         DWORD dwTemp;
         dwTemp = pc->m_dwID;
         pc->m_dwID = j;
         pop->ObjectSurfaceAdd (pc);
         pc->m_dwID = dwTemp; // restore
      }

      // set the matrix
      pop->ObjectMatrixSet (&m_MatrixObject);
      // note changed
      m_pWorld->ObjectChanged (pop);
   }

   // clone the balustrades
   DWORD i;
   for (i = 0; i < 3; i++) {
      if (!m_apBal[i])
         continue;

      // create a new one
      PCObjectBalustrade pss;
      OSINFO OI;
      memset (&OI, 0, sizeof(OI));
      OI.gCode = CLSID_Balustrade;
      OI.gSub = CLSID_BalustradeBalVertWood;
      OI.dwRenderShard = m_OSINFO.dwRenderShard;
      pss = new CObjectBalustrade((PVOID)BS_BALVERTWOOD, &OI);  // need one style just to make sure can recreate (PVOID) ps->pBal->m_dwBalStyle);
      if (!pss)
         continue;

      // BUGFIX - Set a name
      pss->StringSet (OSSTRING_NAME, L"Stairway balustrade");

      m_pWorld->ObjectAdd (pss);

      m_pWorld->ObjectAboutToChange (pss);

      // remove any colors or links in the new object
      pss->m_ds.ClaimClear();

      // clone the surfaces
      m_apBal[i]->ClaimCloneTo (&pss->m_ds, pss);

      // imbue it with new information
      m_apBal[i]->CloneTo (&pss->m_ds, pss);

      // make sure looking at drag points
      pss->m_ds.m_dwDisplayControl = 0;

      // set the matrix
      pss->ObjectMatrixSet (&m_MatrixObject);

      // note changed
      m_pWorld->ObjectChanged (pss);
   }

   // decsontruct the walls
   for (i = 0; i < 3; i++) {
      if (!m_apWall[i])
         continue;

      // create a new one
      PCObjectStructSurface pss;
      OSINFO OI;
      memset (&OI, 0, sizeof(OI));
      OI.gCode = CLSID_StructSurface;
      OI.gSub = CLSID_StructSurfaceInternalStudWall;
      OI.dwRenderShard = m_OSINFO.dwRenderShard;
      pss = new CObjectStructSurface((PVOID)WALLTYPE, &OI);  // need one style just to make sure can recreate (PVOID) ps->pWall->m_dwBalStyle);
      if (!pss)
         continue;

      // BUGFIX - Set a name
      pss->StringSet (OSSTRING_NAME, L"Stairway wall");

      m_pWorld->ObjectAdd (pss);

      m_pWorld->ObjectAboutToChange (pss);

      // remove any colors or links in the new object
      DWORD j;
      while (TRUE) {
         j = pss->ContainerSurfaceGetIndex (0);
         if (j == (DWORD)-1)
            break;
         pss->ContainerSurfaceRemove (j);
      }
      while (TRUE) {
         j = pss->ObjectSurfaceGetIndex (0);
         if (j == (DWORD)-1)
            break;
         pss->ObjectSurfaceRemove (j);
      }

      // clone the surfaces
      m_apWall[i]->ClaimCloneTo (&pss->m_ds, pss);

      // imbue it with new information
      m_apWall[i]->CloneTo (&pss->m_ds, pss);

      // set the matrix
      pss->ObjectMatrixSet (&m_MatrixObject);

      // note changed
      m_pWorld->ObjectChanged (pss);
   }

   return TRUE;
}

// FUTURERELEASE - Support angled balustrade, so angled with stair rise

// FUTURERELEASE - Make sure balstrade verticals go all the way to the treads,
// because right now they hover above.

// FUTURERELEASE - Outdoor steps that drag up a hill and automagically gets the
// right countour and builds outdoor style steps


// FUTURERELEASE - If have landing with a turn in stairs the beams underneath are crooked.
// There's a way to solve this. Do eventually
