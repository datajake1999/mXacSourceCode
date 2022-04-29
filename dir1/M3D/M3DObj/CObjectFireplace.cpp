/************************************************************************
CObjectFireplace.cpp - Draws a Fireplace.

begun 1/10/02 by Mike Rozak
Copyright 2002 Mike Rozak. All rights reserved
*/

#include <windows.h>
#include "escarpment.h"
#include "..\M3D.h"
#include "resource.h"

/**********************************************************************************
CObjectFireplace::Constructor and destructor */
CObjectFireplace::CObjectFireplace (PVOID pParams, POSINFO pInfo)
{
   m_dwRenderShow = RENDERSHOW_MISC;
   m_OSINFO = *pInfo;
   m_fCanBeEmbedded = TRUE;
   m_fEmbedFitToFloor = TRUE;

   m_pChimney.Zero();
   m_pChimney.p[0] = .75;  // width at top
   m_pChimney.p[1] = .75;  // depth at top
   m_pChimney.p[2] = 6; // how hight chimney gets
   m_pChimney.p[3] = 4; // narrow at this height

   m_pChimneyCap.Zero();
   m_pChimneyCap.p[0] = m_pChimney.p[0] + .1;
   m_pChimneyCap.p[1] = m_pChimney.p[1] + .1;
   m_pChimneyCap.p[2] = .1;

   m_pFireplace.Zero();
   m_pFireplace.p[1] = .05; // so default bricks behind the wall
   m_pFireplace.p[2] = 2;  // full height to 2 meters
   m_pFireplace.p[3] = -2; // goes 2 meters below floor

   m_pFireBox.p[0] = 2; // 2 meters wide
   m_pFireBox.p[1] = 1; // 1 meter deep
   m_pFireBox.p[2] = 1.5;  // 1.5 m high
   m_pFireBox.p[3] = 0; // curve

   m_pBrickFloor.p[0] = 2; // width
   m_pBrickFloor.p[1] = .5; // depth
   m_pBrickFloor.p[2] = .05;  // height
   m_pBrickFloor.p[3] = 0; // offset of brick from front of mantel

   m_pHearth.p[0] = 1;  // width
   m_pHearth.p[1] = .75; // detph
   m_pHearth.p[2] = 1;  // height
   m_pHearth.p[3] = 0;  // curve

   m_pMantel.p[0] = m_pFireBox.p[0] + .2;
   m_pMantel.p[1] = -.1;
   m_pMantel.p[2] = .1;
   m_pMantel.p[3] = 0;

   m_fNoodleDirty = TRUE;


   // main fireplace, excluding brick
   ObjectSurfaceAdd (1, RGB(0xc0,0x20,0x20), MATERIAL_FLAT, L"Chimney",
      &GTEXTURECODE_SidingBricks, &GTEXTURESUB_SidingBricks);

   // mantel brick - might be painted
   ObjectSurfaceAdd (2, RGB(0xc0,0x20,0x20), MATERIAL_FLAT, L"Fireplace",
      &GTEXTURECODE_SidingBricks, &GTEXTURESUB_SidingBricks);

   // hearth, might be darker
   ObjectSurfaceAdd (3, RGB(0xc0,0x20,0x20), MATERIAL_FLAT, L"Fireplace hearth",
      &GTEXTURECODE_HearthBricks, &GTEXTURESUB_HearthBricks);

   // floor in front of fireplace
   ObjectSurfaceAdd (4, RGB(0xc0,0x40,0x40), MATERIAL_FLAT, L"Fireplace fire barrier",
      &GTEXTURECODE_FlooringTiles, &GTEXTURESUB_FlooringTiles);

   // cap
   ObjectSurfaceAdd (5, RGB(0xc0,0xc0,0xc0), MATERIAL_FLAT, L"Fireplace cap");

   // mantel
   ObjectSurfaceAdd (6, RGB(0xff,0xff,0xff), MATERIAL_PAINTSEMIGLOSS, L"Fireplace mantel");
}


CObjectFireplace::~CObjectFireplace (void)
{
   // do nothing for now
}


/**********************************************************************************
CObjectFireplace::Delete - Called to delete this object
*/
void CObjectFireplace::Delete (void)
{
   delete this;
}

/**********************************************************************************
CObjectFireplace::Render - Draws a test box.

inputs
   POBJECTRENDER     pr - Render information
*/
void CObjectFireplace::Render (POBJECTRENDER pr, DWORD dwSubObject)
{
   // create the surface render object and draw
   // CRenderSurface rs;
   m_Renderrs.ClearAll();

   CMatrix mObject;
   m_Renderrs.Init (pr->pRS);
   ObjectMatrixGet (&mObject);
   m_Renderrs.Multiply (&mObject);

   // hearth depth is fixed
   m_pHearth.p[1] =  m_pFireBox.p[1] * .9;

   // create the normals for left, right, front, back, up, and down
   DWORD dwNormLeft, dwNormRight, dwNormFront, dwNormBack, dwNormUp, dwNormDown;
   dwNormLeft = m_Renderrs.NewNormal (-1, 0, 0, TRUE);
   dwNormRight = m_Renderrs.NewNormal (1, 0, 0, TRUE);
   dwNormFront = m_Renderrs.NewNormal (0, -1, 0, TRUE);
   dwNormBack = m_Renderrs.NewNormal (0, 1, 0, TRUE);
   dwNormUp = m_Renderrs.NewNormal (0, 0, 1, TRUE);
   dwNormDown = m_Renderrs.NewNormal (0, 0, -1, TRUE);

   // points for top LR of hearth and top LR of mantle
   CPoint      apHearthTop[2], apMantelTop[2];
   apHearthTop[0].Zero();
   apHearthTop[0].p[0] = -m_pHearth.p[0] / 2.0;
   apHearthTop[0].p[1] = -(m_pFireBox.p[1] - m_pChimney.p[1]) + m_pFireplace.p[1];
   apHearthTop[0].p[2] = m_pHearth.p[2];
   apHearthTop[1].Copy (&apHearthTop[0]);
   apHearthTop[1].p[0] *= -1;
   apMantelTop[0].Zero();
   apMantelTop[0].p[0] = -m_pFireBox.p[0] / 2.0;
   apMantelTop[0].p[1] = -(m_pFireBox.p[1] - m_pChimney.p[1]) + m_pFireplace.p[1];
   apMantelTop[0].p[2] = m_pFireBox.p[2];
   apMantelTop[1].Copy (&apMantelTop[0]);
   apMantelTop[1].p[0] *= -1;

   // figure out arc for top of hearth and top of mantle
#define ARCDETAIL    12
   CPoint apArcHearth[ARCDETAIL], apArcMantel[ARCDETAIL];
   DWORD i;
   CPoint pCenter, pt;
   BOOL fRet;
   CMatrix m;
   fp fAngle, fAngle1, fAngle2, fRadius;

   pCenter.Average (&apHearthTop[0], &apHearthTop[1]);
   pCenter.p[2] += m_pHearth.p[3];
   fRet = ThreePointsToCircle (&apHearthTop[0], &pCenter, &apHearthTop[1], &m, &fAngle1, &fAngle2, &fRadius);
   for (i = 0; i < ARCDETAIL; i++) {
      if (fRet) {
         pt.Zero();
         fAngle = (fp) i / (fp) (ARCDETAIL-1);
         fAngle = fAngle * fAngle2 + (1.0 - fAngle) * fAngle1;
         pt.p[0] = sin(fAngle);
         pt.p[1] = cos(fAngle);
         pt.MultiplyLeft (&m);
      }
      else
         pt.Average (&apHearthTop[1], &apHearthTop[0], (fp) i / (fp) (ARCDETAIL-1));
      apArcHearth[i].Copy (&pt);
   }

   pCenter.Average (&apMantelTop[0], &apMantelTop[1]);
   pCenter.p[2] += m_pFireBox.p[3];
   fRet = ThreePointsToCircle (&apMantelTop[0], &pCenter, &apMantelTop[1], &m, &fAngle1, &fAngle2, &fRadius);
   for (i = 0; i < ARCDETAIL; i++) {
      if (fRet) {
         pt.Zero();
         fAngle = (fp) i / (fp) (ARCDETAIL-1);
         fAngle = fAngle * fAngle2 + (1.0 - fAngle) * fAngle1;
         pt.p[0] = sin(fAngle);
         pt.p[1] = cos(fAngle);
         pt.MultiplyLeft (&m);
      }
      else
         pt.Average (&apMantelTop[1], &apMantelTop[0], (fp) i / (fp) (ARCDETAIL-1));
      apArcMantel[i].Copy (&pt);
   }


   if (m_fNoodleDirty) {
      // redo the mantel noodle
      m_MantelNood.BackfaceCullSet (TRUE);
      CPoint pBevel;
      pBevel.Zero();
      pBevel.p[0] = 1;
      m_MantelNood.BevelSet (TRUE, 2, &pBevel);
      m_MantelNood.BevelSet (FALSE, 2, &pBevel);
      m_MantelNood.DrawEndsSet (TRUE);
      CPoint pFront;
      pFront.Zero();
      pFront.p[1] = -1;
      m_MantelNood.FrontVector (&pFront);
      CPoint apPath[3];
      apPath[0].Copy (&apArcMantel[0]);
      apPath[1].Copy (&apArcMantel[ARCDETAIL/2]);
      apPath[2].Copy (&apArcMantel[ARCDETAIL-1]);
      for (i = 0; i < 3; i++) {
         apPath[i].p[1] = (m_pMantel.p[3] - (m_pFireBox.p[1] - m_pChimney.p[1]) + m_pMantel.p[1]) / 2.0 + m_pFireplace.p[1];
         apPath[i].p[2] += m_pMantel.p[2]/2;
      }
      
      // extra length
      fp fLength;
      fLength = (m_pMantel.p[0] - m_pFireBox.p[0]) / 2.0;
      CPoint pt;
      pt.Subtract (&apArcMantel[0], &apArcMantel[1]);
      pt.Normalize();
      pt.Scale (fLength);
      apPath[0].Add (&pt);
      pt.Subtract (&apArcMantel[ARCDETAIL-1], &apArcMantel[ARCDETAIL-2]);
      pt.Normalize();
      pt.Scale (fLength);
      apPath[2].Add (&pt);

      m_MantelNood.PathSpline (FALSE, 3, apPath, (DWORD*)SEGCURVE_CIRCLENEXT, 3);
      CPoint pScale;
      pScale.Zero();
      pScale.p[0] = m_pMantel.p[2];
      pScale.p[1] = m_pMantel.p[3] + (m_pFireBox.p[1] - m_pChimney.p[1]) - m_pMantel.p[1];
      m_MantelNood.ScaleVector (&pScale);
      m_MantelNood.ShapeDefault (NS_RECTANGLE);
   }


   // mantel brick
   m_Renderrs.SetDefMaterial (m_OSINFO.dwRenderShard, ObjectSurfaceFind (2), m_pWorld);

   // draw the front of the mantel
   PCPoint pPoint, pNorm;
   PVERTEX pVert;
   PTEXTPOINT5 pText;
   DWORD dwPointIndex, dwTextIndex, dwVertIndex, dwNormIndex;
   pPoint = m_Renderrs.NewPoints (&dwPointIndex, (ARCDETAIL+2)*2);
   if (!pPoint) {
      m_Renderrs.Commit();
      return;
   }

   pText = m_Renderrs.NewTextures (&dwTextIndex, (ARCDETAIL+2)*2);
   for (i = 0; i < ARCDETAIL+2; i++) {
      if (i == 0) {
         // hearth
         pPoint[i*2].Copy (&apHearthTop[0]);
         pPoint[i*2].p[2] = 0;

         // mantel
         pPoint[i*2+1].Copy (&apMantelTop[0]);
         pPoint[i*2+1].p[2] = 0;
      }
      else if (i == ARCDETAIL+1) {
         // hearth
         pPoint[i*2].Copy (&apHearthTop[1]);
         pPoint[i*2].p[2] = 0;

         // mantel
         pPoint[i*2+1].Copy (&apMantelTop[1]);
         pPoint[i*2+1].p[2] = 0;
      }
      else {
         // inside
         pPoint[i*2].Copy (&apArcHearth[i-1]);
         pPoint[i*2+1].Copy (&apArcMantel[i-1]);
      }
   }
   // textures
   if (pText) {
      for (i = 0; i < (ARCDETAIL+2)*2; i++) {
         pText[i].hv[0] = pPoint[i].p[0];
         pText[i].hv[1] = -pPoint[i].p[2];

         pText[i].xyz[0] = pPoint[i].p[0];
         pText[i].xyz[1] = pPoint[i].p[1];
         pText[i].xyz[2] = pPoint[i].p[2];
      }

      m_Renderrs.ApplyTextureRotation (pText, (ARCDETAIL+2)*2);
   }
   // vertecies
   pVert = m_Renderrs.NewVertices (&dwVertIndex, (ARCDETAIL+2)*2);
   if (!pVert) {
      m_Renderrs.Commit();
      return;
   }
   for (i = 0; i < (ARCDETAIL+2)*2; i++) {
      pVert[i].dwNormal = dwNormFront;
      pVert[i].dwPoint = dwPointIndex + i;
      pVert[i].dwTexture = pText ? (dwTextIndex + i) : 0;
   }
   // polygons
   for (i = 0; i < ARCDETAIL+1; i++)
      m_Renderrs.NewQuad (
         dwVertIndex + i*2,
         dwVertIndex + i*2 + 1,
         dwVertIndex + (i+1)*2 + 1,
         dwVertIndex + (i+1)*2);

   // because the hearth and mantel are so central, remember the points and textures
   // for later reference
   CPoint apHearthBack[(ARCDETAIL+2)*2];
   PCPoint pHearthPoint;
   DWORD dwHearthPointIndex, dwHearthTextIndex;
   memcpy (apHearthBack, pPoint, sizeof(apHearthBack));
   pHearthPoint = apHearthBack;
   dwHearthPointIndex = dwPointIndex;
   dwHearthTextIndex = dwTextIndex;


   // top of the mantle
   CPoint pA, pB, pN;
   CPoint pBack[2];
   CPoint pFront[2];
   if (m_pFireBox.p[1] > m_pChimney.p[1] + CLOSE) {
      // top of mantle
      pPoint = m_Renderrs.NewPoints (&dwPointIndex, ARCDETAIL);
      if (!pPoint) {
         m_Renderrs.Commit();
         return;
      }
      pNorm = m_Renderrs.NewNormals (TRUE, &dwNormIndex, ARCDETAIL);
      pText = m_Renderrs.NewTextures (&dwTextIndex, ARCDETAIL*2);
      pVert = m_Renderrs.NewVertices (&dwVertIndex, ARCDETAIL*2);
      if (!pVert) {
         m_Renderrs.Commit();
         return;
      }
      for (i = 0; i < ARCDETAIL; i++) {
         pPoint[i].Copy (&pHearthPoint[(i+1)*2+1]);
         pPoint[i].p[1] += m_pFireBox.p[1] - m_pChimney.p[1];
      }
      if (pText) {
         for (i = 0; i < ARCDETAIL; i++) {
            pText[i*2].hv[0] = 0;
            pText[i*2+1].hv[0] = -(m_pFireBox.p[1] - m_pChimney.p[1]);
            pText[i*2].hv[1] = pText[i*2+1].hv[1] = -pPoint[i].p[0];

            DWORD j;
            for (j = 0; j < 3; j++) {
               pText[i*2+1].xyz[j] = pPoint[i].p[j];
               pText[i*2].xyz[j] = pHearthPoint[(i+1)*2+1].p[j];
            } // j
         } // i
         m_Renderrs.ApplyTextureRotation (pText, ARCDETAIL*2);
      }
      if (pNorm) for (i = 0; i < ARCDETAIL; i++) {
         pA.Subtract (&pPoint[i], &pHearthPoint[(i+1)*2+1]);
         pB.Subtract (&pHearthPoint[((((i+1)<ARCDETAIL) ? (i+1) : i)+1)*2+1],
            &pHearthPoint[((i ? (i-1) : i)+1) * 2+1]);
         pN.CrossProd (&pB, &pA);
         pN.Normalize();
         pNorm[i].Copy (&pN);
      }
      for (i = 0; i < ARCDETAIL; i++) {
         pVert[i*2].dwNormal = pNorm ? (dwNormIndex + i) : 0;
         pVert[i*2].dwPoint = dwHearthPointIndex + (i+1)*2+1;
         pVert[i*2].dwTexture = pText ? (dwTextIndex + i*2) : 0;

         pVert[i*2+1].dwNormal = pNorm ? (dwNormIndex + i) : 0;
         pVert[i*2+1].dwPoint = dwPointIndex + i;
         pVert[i*2+1].dwTexture = pText ? (dwTextIndex + i*2+1) : 0;
      }
      for (i = 0; i < ARCDETAIL-1; i++)
         m_Renderrs.NewQuad (
            dwVertIndex + i*2,
            dwVertIndex + i*2+1,
            dwVertIndex + (i+1)*2+1,
            dwVertIndex + (i+1)*2);

      // L & R sides of hearth
      pFront[0].Copy (&pPoint[0]);
      pFront[1].Copy (&pPoint[ARCDETAIL-1]);
      pBack[0].Copy (&apMantelTop[0]);
      pBack[0].p[1] += (m_pFireBox.p[1] - m_pChimney.p[1]);
      pBack[0].p[2] = 0;
      pBack[1].Copy (&pBack[0]);
      pBack[1].p[0] *= -1;
      m_Renderrs.ShapeQuadQuick (&pFront[0], &pHearthPoint[1*2+1], &pHearthPoint[0*2+1], &pBack[0],
         dwNormLeft, -2, -3);
      m_Renderrs.ShapeQuadQuick (&pHearthPoint[(ARCDETAIL)*2+1], &pFront[1], 
         &pBack[1], &pHearthPoint[(ARCDETAIL+1)*2+1],
         dwNormRight, 2, -3);
   }

   // hearth inside different color
   m_Renderrs.SetDefMaterial (m_OSINFO.dwRenderShard, ObjectSurfaceFind (3), m_pWorld);

   // create the inside of the hearth
   pPoint = m_Renderrs.NewPoints (&dwPointIndex, ARCDETAIL);
   if (!pPoint) {
      m_Renderrs.Commit();
      return;
   }
   pNorm = m_Renderrs.NewNormals (TRUE, &dwNormIndex, ARCDETAIL);
   pText = m_Renderrs.NewTextures (&dwTextIndex, ARCDETAIL*2);
   pVert = m_Renderrs.NewVertices (&dwVertIndex, ARCDETAIL*2);
   if (!pVert) {
      m_Renderrs.Commit();
      return;
   }
   for (i = 0; i < ARCDETAIL; i++) {
      pPoint[i].Copy (&pHearthPoint[(i+1)*2]);
      pPoint[i].p[1] += min(m_pHearth.p[1], m_pFireBox.p[1]);
   }
   if (pText) {
      for (i = 0; i < ARCDETAIL; i++) {
         pText[i*2].hv[0] = 0;
         pText[i*2+1].hv[0] = min(m_pHearth.p[1], m_pFireBox.p[1]);
         pText[i*2].hv[1] = pText[i*2+1].hv[1] = -pPoint[i].p[0];

         DWORD j;
         for (j = 0; j < 3; j++) {
            pText[i*2].xyz[j] = pHearthPoint[(i+1)*2].p[j];
            pText[i*2+1].xyz[j] = pPoint[i].p[j];
         }// j
      } // i
      m_Renderrs.ApplyTextureRotation (pText, ARCDETAIL*2);
   }
   if (pNorm) for (i = 0; i < ARCDETAIL; i++) {
      pA.Subtract (&pPoint[i], &pHearthPoint[(i+1)*2]);
      pB.Subtract (&pHearthPoint[((((i+1)<ARCDETAIL) ? (i+1) : i)+1)*2],
         &pHearthPoint[((i ? (i-1) : i)+1) * 2]);
      pN.CrossProd (&pA, &pB);
      pN.Normalize();
      pNorm[i].Copy (&pN);
   }
   for (i = 0; i < ARCDETAIL; i++) {
      pVert[i*2].dwNormal = pNorm ? (dwNormIndex + i) : 0;
      pVert[i*2].dwPoint = dwHearthPointIndex + (i+1)*2;
      pVert[i*2].dwTexture = pText ? (dwTextIndex + i*2) : 0;

      pVert[i*2+1].dwNormal = pNorm ? (dwNormIndex + i) : 0;
      pVert[i*2+1].dwPoint = dwPointIndex + i;
      pVert[i*2+1].dwTexture = pText ? (dwTextIndex + i*2+1) : 0;
   }
   for (i = 0; i < ARCDETAIL-1; i++)
      m_Renderrs.NewQuad (
         dwVertIndex + i*2 + 1,
         dwVertIndex + i*2,
         dwVertIndex + (i+1)*2,
         dwVertIndex + (i+1)*2 + 1);

   // L & R walls of hearth
   pFront[0].Copy (&pPoint[0]);
   pFront[1].Copy (&pPoint[ARCDETAIL-1]);
   pBack[0].Copy (&apHearthTop[0]);
   pBack[0].p[1] += min(m_pHearth.p[1], m_pFireBox.p[1]);
   pBack[0].p[2] = 0;
   pBack[1].Copy (&pBack[0]);
   pBack[1].p[0] *= -1;
   m_Renderrs.ShapeQuadQuick (&pHearthPoint[1*2], &pFront[0], &pBack[0], &pHearthPoint[0*2],
      dwNormRight, 2, -3);
   m_Renderrs.ShapeQuadQuick (&pFront[1], &pHearthPoint[(ARCDETAIL)*2],
      &pHearthPoint[(ARCDETAIL+1)*2], &pBack[1],
      dwNormLeft, -2, -3);
   
   // back of hearth
   DWORD dwBackL, dwBackR;
   dwBackL = m_Renderrs.NewPoint (pBack[0].p[0], pBack[0].p[1], pBack[0].p[2]);
   dwBackR = m_Renderrs.NewPoint (pBack[1].p[0], pBack[1].p[1], pBack[1].p[2]);
   pVert = m_Renderrs.NewVertices (&dwVertIndex, ARCDETAIL+2);
   if (!pVert) {
      m_Renderrs.Commit();
      return;
   }
   for (i = 0; i < ARCDETAIL+2; i++) {
      pVert[i].dwNormal = dwNormFront;
      if (i == 0)
         pVert[i].dwPoint = dwBackL;
      else if (i == ARCDETAIL+1)
         pVert[i].dwPoint = dwBackR;
      else
         pVert[i].dwPoint = dwPointIndex + (i-1);
      pVert[i].dwTexture = pText ? (dwHearthTextIndex + i*2) : 0;
   }
   PPOLYDESCRIPT pPoly;
   pPoly = m_Renderrs.NewPolygon (ARCDETAIL+2);
   if (pPoly) for (i = 0; i < ARCDETAIL+2; i++)
      ((DWORD*)(pPoly+1))[i] = dwVertIndex + i;

   // Floor of hearth
   pFront[0].Copy (&pHearthPoint[0*2]);
   pFront[1].Copy (&pHearthPoint[(ARCDETAIL+1)*2]);
   pBack[0].p[2] += m_pBrickFloor.p[2];
   pBack[1].p[2] += m_pBrickFloor.p[2];
   pFront[0].p[2] += m_pBrickFloor.p[2];
   pFront[1].p[2] += m_pBrickFloor.p[2];
   m_Renderrs.ShapeQuadQuick (&pBack[0], &pBack[1], &pFront[1], &pFront[0],
      dwNormUp, 1, -2);

   // draw floor bit
   if (m_pBrickFloor.p[2] > CLOSE) {
      // Color for side of brick floor is same as mantel
      m_Renderrs.SetDefMaterial (m_OSINFO.dwRenderShard, ObjectSurfaceFind (2), m_pWorld);

      // front
      pFront[0].p[0] = -m_pBrickFloor.p[0]/2;
      pFront[0].p[1] = m_pBrickFloor.p[3] - m_pBrickFloor.p[1] - (m_pFireBox.p[1] - m_pChimney.p[1]) + m_pFireplace.p[1];
      pFront[0].p[2] = m_pBrickFloor.p[2];
      pFront[1].Copy (&pFront[0]);
      pFront[1].p[0] *= -1;
      pBack[0].Copy (&pFront[0]);
      pBack[0].p[2] = 0;
      pBack[1].Copy (&pBack[0]);
      pBack[1].p[0] *= -1;
      m_Renderrs.ShapeQuadQuick (&pFront[0], &pFront[1], &pBack[1], &pBack[0],
         dwNormFront, 1, -3);

      // left side of brick floor
      pFront[0].p[0] = -m_pBrickFloor.p[0]/2;
      pFront[0].p[1] = m_pBrickFloor.p[3] - (m_pFireBox.p[1] - m_pChimney.p[1]) + m_pFireplace.p[1];
      pFront[0].p[2] = m_pBrickFloor.p[2];
      pFront[1].Copy (&pFront[0]);
      pFront[1].p[1] -= m_pBrickFloor.p[1];
      pBack[0].Copy (&pFront[0]);
      pBack[0].p[2] = 0;
      pBack[1].Copy (&pBack[0]);
      pBack[1].p[1] = pFront[1].p[1];
      m_Renderrs.ShapeQuadQuick (&pFront[0], &pFront[1], &pBack[1], &pBack[0],
         dwNormLeft, -2, -3);

      // right side of brick floor
      pFront[0].p[0] *= -1;
      pFront[1].p[0] *= -1;
      pBack[0].p[0] *= -1;
      pBack[1].p[0] *= -1;
      m_Renderrs.ShapeQuadQuick (&pFront[1], &pFront[0], &pBack[0], &pBack[1],
         dwNormRight, 2, -3);

      // different color for top of brick floor
      m_Renderrs.SetDefMaterial (m_OSINFO.dwRenderShard, ObjectSurfaceFind (4), m_pWorld);

      // top of brick floor
      pBack[0].p[0] = -m_pBrickFloor.p[0]/2;
      pBack[0].p[1] = m_pBrickFloor.p[3] - (m_pFireBox.p[1] - m_pChimney.p[1]) + m_pFireplace.p[1];
      pBack[0].p[2] = m_pBrickFloor.p[2] - CLOSE;  // so will not cover over fire bricks
      pBack[1].Copy (&pBack[0]);
      pBack[1].p[0] *= -1;
      pFront[0].Copy (&pBack[0]);
      pFront[0].p[1] -= m_pBrickFloor.p[1];
      pFront[1].Copy (&pFront[0]);
      pFront[1].p[0] *= -1;
      m_Renderrs.ShapeQuadQuick (&pBack[0], &pBack[1], &pFront[1], &pFront[0],
         dwNormUp, 1, -2);

   }

   // Different color for rest of chimney
   m_Renderrs.SetDefMaterial (m_OSINFO.dwRenderShard, ObjectSurfaceFind (1), m_pWorld);

   // base front of chimney, below mantel
   pFront[0].p[0] = -m_pFireBox.p[0] / 2.0;
   pFront[0].p[1] = -(m_pFireBox.p[1] - m_pChimney.p[1]) + m_pFireplace.p[1];
   pFront[0].p[2] = 0;
   pFront[1].Copy (&pFront[0]);
   pFront[1].p[0] *= -1;
   pBack[0].Copy (&pFront[0]);
   pBack[0].p[2] = m_pFireplace.p[3];
   pBack[1].Copy (&pBack[0]);
   pBack[1].p[0] *= -1;
   m_Renderrs.ShapeQuadQuick (&pFront[0], &pFront[1], &pBack[1], &pBack[0],
      dwNormFront, 1, -3);

   // left side of chimney, below mantel
   pFront[0].p[0] = -m_pFireBox.p[0] / 2.0 + CLOSE;   // BUGFIX - So can have different color for mantel
   pFront[0].p[1] = m_pChimney.p[1] + m_pFireplace.p[1];
   pFront[0].p[2] = m_pFireBox.p[2];
   pFront[1].Copy (&pFront[0]);
   pFront[1].p[1] = -(m_pFireBox.p[1] - m_pChimney.p[1]) + m_pFireplace.p[1];
   pBack[0].Copy (&pFront[0]);
   pBack[0].p[2] = m_pFireplace.p[3];
   pBack[1].Copy (&pBack[0]);
   pBack[1].p[1] = pFront[1].p[1];
   m_Renderrs.ShapeQuadQuick (&pFront[0], &pFront[1], &pBack[1], &pBack[0],
      dwNormLeft, -2, -3);

   // right side of chimney, below mantel
   pFront[0].p[0] *= -1;
   pFront[1].p[0] *= -1;
   pBack[0].p[0] *= -1;
   pBack[1].p[0] *= -1;
   m_Renderrs.ShapeQuadQuick (&pFront[1], &pFront[0], &pBack[0], &pBack[1],
      dwNormRight, 2, -3);

   // back of chimney, full length
   pFront[0].p[0] = m_pFireBox.p[0] / 2;
   pFront[0].p[1] = m_pChimney.p[1] + m_pFireplace.p[1];
   pFront[0].p[2] = m_pFireplace.p[2];
   pFront[1].Copy (&pFront[0]);
   pFront[1].p[0] *= -1;
   pBack[0].Copy (&pFront[0]);
   pBack[0].p[2] = m_pFireplace.p[3];
   pBack[1].Copy (&pBack[0]);
   pBack[1].p[0] *= -1;
   m_Renderrs.ShapeQuadQuick (&pFront[0], &pFront[1], &pBack[1], &pBack[0],
      dwNormBack, -1, -3);

   // front of chimney, above mantel
   pFront[0].p[0] = -m_pFireBox.p[0] / 2.0;
   pFront[0].p[1] = m_pFireplace.p[1];
   pFront[0].p[2] = m_pFireplace.p[2];
   pFront[1].Copy (&pFront[0]);
   pFront[1].p[0] *= -1;
   pBack[0].Copy (&pFront[0]);
   pBack[0].p[2] = m_pFireBox.p[2];
   pBack[1].Copy (&pBack[0]);
   pBack[1].p[0] *= -1;
   m_Renderrs.ShapeQuadQuick (&pFront[0], &pFront[1], &pBack[1], &pBack[0],
      dwNormFront, 1, -3);

   // left side of chimney, above mantel
   pFront[0].p[0] = -m_pFireBox.p[0] / 2.0;
   pFront[0].p[1] = m_pChimney.p[1] + m_pFireplace.p[1];
   pFront[0].p[2] = m_pFireplace.p[2];
   pFront[1].Copy (&pFront[0]);
   pFront[1].p[1] = m_pFireplace.p[1];
   pBack[0].Copy (&pFront[0]);
   pBack[0].p[2] = m_pFireBox.p[2];
   pBack[1].Copy (&pBack[0]);
   pBack[1].p[1] = pFront[1].p[1];
   m_Renderrs.ShapeQuadQuick (&pFront[0], &pFront[1], &pBack[1], &pBack[0],
      dwNormLeft, -2, -3);

   // right side of chimney, above mantel
   pFront[0].p[0] *= -1;
   pFront[1].p[0] *= -1;
   pBack[0].p[0] *= -1;
   pBack[1].p[0] *= -1;
   m_Renderrs.ShapeQuadQuick (&pFront[1], &pFront[0], &pBack[0], &pBack[1],
      dwNormRight, 2, -3);

   // angled bit, front
   pFront[0].p[0] = -m_pChimney.p[0] / 2.0;
   pFront[0].p[1] = m_pFireplace.p[1];
   pFront[0].p[2] = m_pChimney.p[3];
   pFront[1].Copy (&pFront[0]);
   pFront[1].p[0] *= -1;
   pBack[0].Copy (&pFront[0]);
   pBack[0].p[0] = -m_pFireBox.p[0] / 2.0;
   pBack[0].p[2] = m_pFireplace.p[2];
   pBack[1].Copy (&pBack[0]);
   pBack[1].p[0] *= -1;
   m_Renderrs.ShapeQuadQuick (&pFront[0], &pFront[1], &pBack[1], &pBack[0],
      dwNormFront, 1, -3);

   // angled bit, back
   pFront[0].p[1] = m_pChimney.p[1] + m_pFireplace.p[1];
   pFront[1].p[1] = pFront[0].p[1];
   pBack[0].p[1] = pFront[0].p[1];
   pBack[1].p[1] = pFront[0].p[1];
   m_Renderrs.ShapeQuadQuick (&pFront[1], &pFront[0], &pBack[0], &pBack[1],
      dwNormBack, -1, -3);

   // angled bit ,left
   pFront[0].p[0] = -m_pChimney.p[0] / 2.0;
   pFront[0].p[1] = m_pChimney.p[1] + m_pFireplace.p[1];
   pFront[0].p[2] = m_pChimney.p[3];
   pFront[1].Copy (&pFront[0]);
   pFront[1].p[1] = m_pFireplace.p[1];
   pBack[0].Copy (&pFront[0]);
   pBack[0].p[0] = -m_pFireBox.p[0] / 2.0;
   pBack[0].p[2] = m_pFireplace.p[2];
   pBack[1].Copy (&pBack[0]);
   pBack[1].p[1] = pFront[1].p[1];
   m_Renderrs.ShapeQuadQuick (&pFront[0], &pFront[1], &pBack[1], &pBack[0],
      m_Renderrs.NewNormal (&pFront[0], &pFront[1], &pBack[1]), -2, -3);


   // angled bit ,right
   pFront[0].p[0] *= -1;
   pFront[1].p[0] *= -1;
   pBack[0].p[0] *= -1;
   pBack[1].p[0] *= -1;
   m_Renderrs.ShapeQuadQuick (&pFront[1], &pFront[0], &pBack[0], &pBack[1],
      m_Renderrs.NewNormal (&pFront[1], &pFront[0], &pBack[0]), 2, -3);

   // front chimney
   pFront[0].p[0] = -m_pChimney.p[0] / 2.0;
   pFront[0].p[1] = m_pFireplace.p[1];
   pFront[0].p[2] = m_pChimney.p[2] - m_pChimneyCap.p[2];
   pFront[1].Copy (&pFront[0]);
   pFront[1].p[0] *= -1;
   pBack[0].Copy (&pFront[0]);
   pBack[0].p[2] = m_pChimney.p[3];
   pBack[1].Copy (&pBack[0]);
   pBack[1].p[0] *= -1;
   m_Renderrs.ShapeQuadQuick (&pFront[0], &pFront[1], &pBack[1], &pBack[0],
      dwNormFront, 1, -3, FALSE);

   // back of chimney
   pFront[0].p[1] = m_pChimney.p[1] + m_pFireplace.p[1];
   pFront[1].p[1] = pFront[0].p[1];
   pBack[0].p[1] = pFront[0].p[1];
   pBack[1].p[1] = pFront[0].p[1];
   m_Renderrs.ShapeQuadQuick (&pFront[1], &pFront[0], &pBack[0], &pBack[1],
      dwNormBack, -1, -3, FALSE);

   // left of chimney
   pFront[0].p[0] = -m_pChimney.p[0] / 2.0;
   pFront[0].p[1] = m_pChimney.p[1] + m_pFireplace.p[1];
   pFront[0].p[2] = m_pChimney.p[2] - m_pChimneyCap.p[2];
   pFront[1].Copy (&pFront[0]);
   pFront[1].p[1] = m_pFireplace.p[1];
   pBack[0].Copy (&pFront[0]);
   pBack[0].p[2] = m_pChimney.p[3];
   pBack[1].Copy (&pBack[0]);
   pBack[1].p[1] = pFront[1].p[1];
   m_Renderrs.ShapeQuadQuick (&pFront[0], &pFront[1], &pBack[1], &pBack[0],
      dwNormLeft, -2, -3, FALSE);

   // right of chimney
   pFront[0].p[0] *= -1;
   pFront[1].p[0] *= -1;
   pBack[0].p[0] *= -1;
   pBack[1].p[0] *= -1;
   m_Renderrs.ShapeQuadQuick (&pFront[1], &pFront[0], &pBack[0], &pBack[1],
      dwNormRight, 2, -3, FALSE);

   // recalc cap
   if (m_fNoodleDirty) {
      // cap...

      // redo the mantel noodle
      m_CapNood.BackfaceCullSet (TRUE);
      CPoint pFront;
      pFront.Zero();
      pFront.p[2] = 1;
      m_CapNood.FrontVector (&pFront);

      CPoint apPath[4];
      apPath[0].Zero();
      apPath[0].p[0] = -m_pChimneyCap.p[0] / 2;
      apPath[0].p[1] = -m_pChimneyCap.p[1] / 2;
      apPath[0].p[2] = m_pChimney.p[2] -m_pChimneyCap.p[2] / 2;
      apPath[1].Copy (&apPath[0]);
      apPath[1].p[1] *= -1;
      apPath[2].Copy (&apPath[1]);
      apPath[2].p[0] *= -1;
      apPath[3].Copy (&apPath[2]);
      apPath[3].p[1] *= -1;
      for (i = 0; i < 4; i++)
         apPath[i].p[1] += m_pFireplace.p[1] + m_pChimney.p[1] / 2;

      m_CapNood.PathSpline (TRUE, 4, apPath, (DWORD*)SEGCURVE_LINEAR, 0);

      CPoint pScale;
      pScale.Zero();
      pScale.p[0] = min(m_pChimneyCap.p[0], m_pChimneyCap.p[1]) / 3.0;
      pScale.p[1] = m_pChimneyCap.p[2];
      m_CapNood.ScaleVector (&pScale);

      CPoint pOffset;
      pOffset.Zero();
      pOffset.p[0] = pScale.p[0] / 2;
      m_CapNood.OffsetSet (&pOffset);

      m_CapNood.ShapeDefault (NS_RECTANGLE);
   }

   // noodles
   m_fNoodleDirty = FALSE;
   if (m_pMantel.p[2] > CLOSE) {
      m_Renderrs.SetDefMaterial (m_OSINFO.dwRenderShard, ObjectSurfaceFind (6), m_pWorld);
      m_MantelNood.Render (pr, &m_Renderrs);
   }

   if (m_pChimneyCap.p[2] > CLOSE) {
      m_Renderrs.SetDefMaterial (m_OSINFO.dwRenderShard, ObjectSurfaceFind (5), m_pWorld);
      m_CapNood.Render (pr, &m_Renderrs);
   }

   m_Renderrs.Commit();
}

// NOTE: Not doing QueryBoundingBox() for fireplace because would be semi-messy
// code that might be buggy, and fireplaces aren't that common, so not much of a speed saving

/**********************************************************************************
CObjectFireplace::Clone - Clones this

inputs
   none
returns
   PCObjectSocket - Clone
*/
PCObjectSocket CObjectFireplace::Clone (void)
{
   PCObjectFireplace pNew;

   pNew = new CObjectFireplace(NULL, &m_OSINFO);

   // clone template info
   CloneTemplate(pNew);

   pNew->m_pChimney.Copy (&m_pChimney);
   pNew->m_pChimneyCap.Copy (&m_pChimneyCap);
   pNew->m_pFireplace.Copy (&m_pFireplace);
   pNew->m_pFireBox.Copy (&m_pFireBox);
   pNew->m_pBrickFloor.Copy (&m_pBrickFloor);
   pNew->m_pHearth.Copy (&m_pHearth);
   pNew->m_pMantel.Copy (&m_pMantel);
  
   pNew->m_fNoodleDirty = m_fNoodleDirty;
   m_MantelNood.CloneTo (&pNew->m_MantelNood);
   m_CapNood.CloneTo (&pNew->m_CapNood);

   return pNew;
}

static PWSTR gpszChimney = L"Chimney";
static PWSTR gpszChimneyCap = L"ChimneyCap";
static PWSTR gpszFireplace = L"Fireplace";
static PWSTR gpszFireBox = L"FireBox";
static PWSTR gpszBrickFloor = L"BrickFloor";
static PWSTR gpszHearth = L"Hearth";
static PWSTR gpszMantel = L"Mantel";
static PWSTR gpszShowMantel = L"ShowMantel";

PCMMLNode2 CObjectFireplace::MMLTo (void)
{
   // call into the template first
   PCMMLNode2 pNode = MMLToTemplate();
   if (!pNode)
      return NULL;

   MMLValueSet (pNode, gpszChimney, &m_pChimney);
   MMLValueSet (pNode, gpszChimneyCap, &m_pChimneyCap);
   MMLValueSet (pNode, gpszFireplace, &m_pFireplace);
   MMLValueSet (pNode, gpszFireBox, &m_pFireBox);
   MMLValueSet (pNode, gpszBrickFloor, &m_pBrickFloor);
   MMLValueSet (pNode, gpszHearth, &m_pHearth);
   MMLValueSet (pNode, gpszMantel, &m_pMantel);

   return pNode;
}

BOOL CObjectFireplace::MMLFrom (PCMMLNode2 pNode)
{
   // call into the template first
   if (!MMLFromTemplate (pNode))
      return FALSE;

   MMLValueGetPoint (pNode, gpszChimney, &m_pChimney);
   MMLValueGetPoint (pNode, gpszChimneyCap, &m_pChimneyCap);
   MMLValueGetPoint (pNode, gpszFireplace, &m_pFireplace);
   MMLValueGetPoint (pNode, gpszFireBox, &m_pFireBox);
   MMLValueGetPoint (pNode, gpszBrickFloor, &m_pBrickFloor);
   MMLValueGetPoint (pNode, gpszHearth, &m_pHearth);
   MMLValueGetPoint (pNode, gpszMantel, &m_pMantel);

   // Will need to rebuild noodles
   m_fNoodleDirty = TRUE;

   return TRUE;
}


/**************************************************************************************
CObjectFireplace::MoveReferencePointQuery - 
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
BOOL CObjectFireplace::MoveReferencePointQuery (DWORD dwIndex, PCPoint pp)
{
   if (dwIndex >= 1)
      return FALSE;

   pp->Zero();

   return TRUE;
}

/**************************************************************************************
CObjectFireplace::MoveReferenceStringQuery -
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
BOOL CObjectFireplace::MoveReferenceStringQuery (DWORD dwIndex, PWSTR psz, DWORD dwSize, DWORD *pdwNeeded)
{
   if (dwIndex >= 1)
      return FALSE;

   PWSTR szTemp = L"Base";

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


/**********************************************************************************
CObjectFireplace::EmbedDoCutout - Member function specific to the template. Called
when the object has moved within the surface. This enables the super-class for
the embedded object to pass a cutout into the container. (Basically, specify the
hole for the window or door)
*/
BOOL CObjectFireplace::EmbedDoCutout (void)
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

   // points for top LR of hearth and top LR of mantle
   CPoint      apMantelTop[2];
   apMantelTop[0].Zero();
   apMantelTop[0].p[0] = -m_pFireBox.p[0] / 2.0;
   apMantelTop[0].p[1] = -(m_pFireBox.p[1] - m_pChimney.p[1]) + m_pFireplace.p[1];
   apMantelTop[0].p[2] = m_pFireBox.p[2];
   apMantelTop[1].Copy (&apMantelTop[0]);
   apMantelTop[1].p[0] *= -1;

   // figure out arc for top of hearth and top of mantle
   CPoint apArcMantel[ARCDETAIL];
   DWORD i;
   CPoint pCenter, pt;
   BOOL fRet;
   CMatrix m;
   fp fAngle, fAngle1, fAngle2, fRadius;

   pCenter.Average (&apMantelTop[0], &apMantelTop[1]);
   pCenter.p[2] += m_pFireBox.p[3];
   fRet = ThreePointsToCircle (&apMantelTop[0], &pCenter, &apMantelTop[1], &m, &fAngle1, &fAngle2, &fRadius);
   for (i = 0; i < ARCDETAIL; i++) {
      if (fRet) {
         pt.Zero();
         fAngle = (fp) i / (fp) (ARCDETAIL-1);
         fAngle = fAngle * fAngle2 + (1.0 - fAngle) * fAngle1;
         pt.p[0] = sin(fAngle);
         pt.p[1] = cos(fAngle);
         pt.MultiplyLeft (&m);
      }
      else
         pt.Average (&apMantelTop[1], &apMantelTop[0], (fp) i / (fp) (ARCDETAIL-1));
      apArcMantel[i].Copy (&pt);
   }

   // move the mantel tops to 0
   apMantelTop[0].p[2] = 0;
   apMantelTop[1].p[2] = 0;

   CListFixed lFront, lBack;
   lFront.Init (sizeof(CPoint));
   lBack.Init (sizeof(CPoint));
   lFront.Required (ARCDETAIL+2);
   lBack.Required (ARCDETAIL+2);
   for (i = 0; i < ARCDETAIL+2; i++) {
      PCPoint pp;
      if (i == 0)
         pp = &apMantelTop[0];
      else if (i ==  (ARCDETAIL+1))
         pp = &apMantelTop[1];
      else
         pp = &apArcMantel[i-1];

      pt.Copy (pp);
      pt.p[3] = 1;
      pt.MultiplyLeft (&mTrans);
      lFront.Add (&pt);

      pt.Copy (pp);
      pt.p[3] = 1;
      pt.p[1] += m_fContainerDepth + .01;
      pt.MultiplyLeft (&mTrans);
      lBack.Add (&pt);
   }

   pos->ContCutout (&m_gGUID, lFront.Num(), (PCPoint) lFront.Get(0), (PCPoint) lBack.Get(0), TRUE);

   return TRUE;
}



/*************************************************************************************
CObjectFireplace::ControlPointQuery - Called to query the information about a control
point identified by dwID.

inputs
   DWORD       dwID - ID
   POSCONTROL  pInfo - Filled in with the infomration
returns
   BOOL - TRUE if successful
*/
BOOL CObjectFireplace::ControlPointQuery (DWORD dwID, POSCONTROL pInfo)
{
   fp fKnobSize;
   DWORD dwVal;
   fKnobSize = .1;


   memset (pInfo,0, sizeof(*pInfo));
   pInfo->dwID = dwID;
//   pInfo->dwFreedom = 0;   // any direction
   pInfo->fSize = fKnobSize;

   switch (dwID) {
   case 10:  // chimney
   case 11:
   case 12:
   case 13:
   case 14:
   case 15:
   case 16:
   case 17:
      dwVal = dwID - 10;
      pInfo->dwStyle = CPSTYLE_SPHERE;
      pInfo->cColor = RGB(0xff,0,0xff);
      wcscpy (pInfo->szName, L"Chimney");
      pInfo->pLocation.p[0] = m_pChimney.p[0] / 2 * ((dwVal & 0x01) ? 1 : -1);
      pInfo->pLocation.p[1] = m_pFireplace.p[1] + m_pChimney.p[1] * ((dwVal & 0x02) ? 1 : 0);
      pInfo->pLocation.p[2] = (dwVal & 0x04) ? m_pChimney.p[2] : m_pChimney.p[3];
      break;

   case 20: // chimney cap
   case 21:
   case 22:
   case 23:
      dwVal = dwID - 20;
      pInfo->dwStyle = CPSTYLE_CUBE;
      pInfo->cColor = RGB(0xff,0xff,0);
      wcscpy (pInfo->szName, L"Chimney cap");
      pInfo->pLocation.p[0] = m_pChimneyCap.p[0] / 2 * ((dwVal & 0x01) ? 1 : -1);
      pInfo->pLocation.p[1] = m_pFireplace.p[1] + m_pChimney.p[1]/2  + m_pChimneyCap.p[1] / 2.0 * ((dwVal & 0x02) ? 1 : -1);
      pInfo->pLocation.p[2] = m_pChimney.p[2] - m_pChimneyCap.p[2];
      break;

   case 30:  // fireplace
   case 31:
   case 32:
   case 33:
   case 34:
   case 35:
   case 36:
   case 37:
      dwVal = dwID - 30;
      pInfo->dwStyle = CPSTYLE_SPHERE;
      pInfo->cColor = RGB(0xff,0,0xff);
      wcscpy (pInfo->szName, L"Fireplace");
      pInfo->pLocation.p[0] = m_pFireBox.p[0] / 2 * ((dwVal & 0x01) ? 1 : -1);
      pInfo->pLocation.p[1] = m_pFireplace.p[1] + m_pChimney.p[1] * ((dwVal & 0x02) ? 1 : 0);
      pInfo->pLocation.p[2] = (dwVal & 0x04) ? m_pFireplace.p[2] : m_pFireplace.p[3];
      break;

   case 40:  // firebox
   case 41:
   case 42:
   case 43:
      dwVal = dwID - 40;
      pInfo->dwStyle = CPSTYLE_SPHERE;
      pInfo->cColor = RGB(0xff,0,0xff);
      wcscpy (pInfo->szName, L"Firebox");
      pInfo->pLocation.p[0] = m_pFireBox.p[0] / 2 * ((dwVal & 0x01) ? 1 : -1);
      pInfo->pLocation.p[1] = m_pFireplace.p[1] + m_pChimney.p[1] - m_pFireBox.p[1] * ((dwVal & 0x02) ? 0 : 1);
      pInfo->pLocation.p[2] = m_pFireBox.p[2];
      break;
   case 44: // firebox curve
      pInfo->dwStyle = CPSTYLE_SPHERE;
      pInfo->cColor = RGB(0xff,0,0xff);
      wcscpy (pInfo->szName, L"Firebox arch");
      pInfo->pLocation.p[0] = 0;
      pInfo->pLocation.p[1] = m_pFireplace.p[1] + m_pChimney.p[1] - m_pFireBox.p[1];
      pInfo->pLocation.p[2] = m_pFireBox.p[2] + m_pFireBox.p[3];
      break;

   case 50:  // brick floor
   case 51:
   case 52:
   case 53:
      dwVal = dwID - 50;
      pInfo->dwStyle = CPSTYLE_CUBE;
      pInfo->cColor = RGB(0,0xff,0xff);
      wcscpy (pInfo->szName, L"Floor");
      pInfo->pLocation.p[0] = m_pBrickFloor.p[0] / 2 * ((dwVal & 0x01) ? 1 : -1);
      pInfo->pLocation.p[1] = m_pFireplace.p[1] + m_pChimney.p[1] - m_pFireBox.p[1] +
         m_pBrickFloor.p[3] - m_pBrickFloor.p[1] * ((dwVal & 0x02) ? 0 : 1);
      pInfo->pLocation.p[2] = m_pBrickFloor.p[2];
      break;

   case 60:  // hearth
   case 61:
      dwVal = dwID - 60;
      pInfo->dwStyle = CPSTYLE_SPHERE;
      pInfo->cColor = RGB(0xff,0,0xff);
      wcscpy (pInfo->szName, L"Hearth");
      pInfo->pLocation.p[0] = m_pHearth.p[0] / 2 * ((dwVal & 0x01) ? 1 : -1);
      pInfo->pLocation.p[1] = m_pFireplace.p[1] + m_pChimney.p[1] - m_pFireBox.p[1];
      pInfo->pLocation.p[2] = m_pHearth.p[2];
      break;
   case 62: // hearth curve
      pInfo->dwStyle = CPSTYLE_SPHERE;
      pInfo->cColor = RGB(0xff,0,0xff);
      wcscpy (pInfo->szName, L"Hearth arch");
      pInfo->pLocation.p[0] = 0;
      pInfo->pLocation.p[1] = m_pFireplace.p[1] + m_pChimney.p[1] - m_pFireBox.p[1];
      pInfo->pLocation.p[2] = m_pHearth.p[2] + m_pHearth.p[3];
      break;

   case 70:  // mantel
   case 71:
   case 72:
   case 73:
      dwVal = dwID - 70;
      pInfo->dwStyle = CPSTYLE_CUBE;
      pInfo->cColor = RGB(0,0xff,0xff);
      wcscpy (pInfo->szName, L"Mantel");
      pInfo->pLocation.p[0] = m_pMantel.p[0] / 2 * ((dwVal & 0x01) ? 1 : -1);
      if (dwVal & 0x02)
         pInfo->pLocation.p[1] = m_pFireplace.p[1] + m_pChimney.p[1] - m_pFireBox.p[1] +
            m_pMantel.p[1];   // front
      else
         pInfo->pLocation.p[1] = m_pFireplace.p[1] + m_pMantel.p[3];   // back

      pInfo->pLocation.p[2] = m_pMantel.p[2] + m_pFireBox.p[2];

      break;

   default:
      return FALSE;
   }


   return TRUE;
}

/*************************************************************************************
CObjectFireplace::ControlPointSet - Called to change the valud of a control point.

inputs
   DWORD       dwID - ID
   PCPoint     pVal - Contains the new location, in object coordinates
returns
   BOOL - TRUE if successful
*/
BOOL CObjectFireplace::ControlPointSet (DWORD dwID, PCPoint pVal, PCPoint pViewer)
{
   DWORD dwVal;
   fp fOldChimWidth;

   // tell the world we're about to change
   if (m_pWorld)
      m_pWorld->ObjectAboutToChange (this);


   switch (dwID) {
   case 10:  // chimney
   case 11:
   case 12:
   case 13:
   case 14:
   case 15:
   case 16:
   case 17:
      dwVal = dwID - 10;
      fOldChimWidth = m_pChimney.p[0];
      m_pChimney.p[0] = max(fabs (pVal->p[0] * 2), .01);
      m_pChimneyCap.p[0] +=  (m_pChimney.p[0] - fOldChimWidth);
      m_pChimneyCap.p[0] = max(m_pChimneyCap.p[0], .01);

      fOldChimWidth = m_pChimney.p[1];
      if (dwVal & 0x02)
         m_pChimney.p[1] = max(fabs(pVal->p[1] - m_pFireplace.p[1]), .01);
      else {
         m_pChimney.p[1] += m_pFireplace.p[1] - pVal->p[1];
         m_pChimney.p[1] = max(m_pChimney.p[1], .01);
         m_pFireplace.p[1] = pVal->p[1];
      }
      m_pFireBox.p[1] += (m_pChimney.p[1] - fOldChimWidth);
      m_pFireBox.p[1] = max(m_pFireBox.p[1], .01);
      m_pChimneyCap.p[1] +=  (m_pChimney.p[1] - fOldChimWidth);
      m_pChimneyCap.p[1] = max(m_pChimneyCap.p[1], .01);

      if (dwVal & 0x04)
         m_pChimney.p[2] = max(pVal->p[2], m_pChimney.p[3]+.01);
      else {
         m_pChimney.p[3] = min(pVal->p[2], m_pChimney.p[2]-.01);
         m_pChimney.p[3] = max(m_pChimney.p[3], m_pFireplace.p[2] + .01);
      }
      break;

   case 20: // chimney cap
   case 21:
   case 22:
   case 23:
      dwVal = dwID - 20;
      m_pChimneyCap.p[0] = max(fabs (pVal->p[0] * 2), .01);
      m_pChimneyCap.p[1] = max(fabs (pVal->p[1] - (m_pFireplace.p[1] + m_pChimney.p[1]/2))*2, .01);
      m_pChimneyCap.p[2] = max(fabs (pVal->p[2] - m_pChimney.p[2]), .01);
      break;

   case 30:  // fireplace
   case 31:
   case 32:
   case 33:
   case 34:
   case 35:
   case 36:
   case 37:
      dwVal = dwID - 30;
      fOldChimWidth = m_pFireBox.p[0];
      m_pFireBox.p[0] = max(fabs (pVal->p[0] * 2), .01);
      m_pMantel.p[0] += (m_pFireBox.p[0] - fOldChimWidth);
      m_pMantel.p[0] = max(.01,m_pMantel.p[0]);


      fOldChimWidth = m_pChimney.p[1];
      if (dwVal & 0x02)
         m_pChimney.p[1] = max(fabs(pVal->p[1] - m_pFireplace.p[1]), .01);
      else {
         m_pChimney.p[1] += m_pFireplace.p[1] - pVal->p[1];
         m_pChimney.p[1] = max(m_pChimney.p[1], .01);
         m_pFireplace.p[1] = pVal->p[1];
      }
      m_pFireBox.p[1] += (m_pChimney.p[1] - fOldChimWidth);
      m_pFireBox.p[1] = max(m_pFireBox.p[1], .01);
      m_pChimneyCap.p[1] +=  (m_pChimney.p[1] - fOldChimWidth);
      m_pChimneyCap.p[1] = max(m_pChimneyCap.p[1], .01);

      if (dwVal & 0x04) {
         m_pFireplace.p[2] = max(pVal->p[2], m_pFireBox.p[2] + .01);
         m_pFireplace.p[2] = min(m_pFireplace.p[2], m_pChimney.p[3] - .01);
      }
      else
         m_pFireplace.p[3] = min(-.01,pVal->p[2]);
      break;

   case 40:  // firebox
   case 41:
   case 42:
   case 43:
      dwVal = dwID - 40;

      fOldChimWidth = m_pFireBox.p[0];
      m_pFireBox.p[0] = max(fabs (pVal->p[0] * 2), .01);
      m_pMantel.p[0] += (m_pFireBox.p[0] - fOldChimWidth);
      m_pMantel.p[0] = max(.01,m_pMantel.p[0]);

      fOldChimWidth = m_pChimney.p[1];
      if (dwVal & 0x02)
         m_pChimney.p[1] = max(fabs(pVal->p[1] - m_pFireplace.p[1]), .01);
      else {
         m_pFireBox.p[1] = m_pFireplace.p[1] + m_pChimney.p[1] - pVal->p[1];
         m_pFireBox.p[1] = max(m_pFireBox.p[1], m_pChimney.p[1]);
      }
      m_pFireBox.p[1] += (m_pChimney.p[1] - fOldChimWidth);
      m_pFireBox.p[1] = max(m_pFireBox.p[1], .01);
      m_pChimneyCap.p[1] +=  (m_pChimney.p[1] - fOldChimWidth);
      m_pChimneyCap.p[1] = max(m_pChimneyCap.p[1], .01);

      m_pFireBox.p[2] = max(m_pHearth.p[2]+.01, pVal->p[2]);
      m_pFireBox.p[2] = min(m_pFireplace.p[2]-.01, m_pFireBox.p[2]);
      break;

   case 44: // firebox curve
      m_pFireBox.p[3] = max(0,pVal->p[2] - m_pFireBox.p[2]);
      break;

   case 50:  // brick floor
   case 51:
   case 52:
   case 53:
      dwVal = dwID - 50;

      m_pBrickFloor.p[0] = max(fabs (pVal->p[0] * 2), .01);

      if (dwVal & 0x02) {
         fOldChimWidth = m_pBrickFloor.p[3];
         m_pBrickFloor.p[3] = pVal->p[1] - m_pFireplace.p[1] - m_pChimney.p[1] + m_pFireBox.p[1];
         m_pBrickFloor.p[1] += (m_pBrickFloor.p[3] - fOldChimWidth);
      }
      else {
         m_pBrickFloor.p[1] = m_pFireplace.p[1] + m_pChimney.p[1] - m_pFireBox.p[1] +
            m_pBrickFloor.p[3] - pVal->p[1];
      }
      m_pBrickFloor.p[1] = max(m_pBrickFloor.p[1], .01);

      m_pBrickFloor.p[2] = max(0,pVal->p[2]);
      break;

   case 60:  // hearth
   case 61:
      dwVal = dwID - 60;

      m_pHearth.p[0] = max(fabs (pVal->p[0] * 2), .01);

      fOldChimWidth = m_pChimney.p[1];
      m_pFireBox.p[1] = m_pFireplace.p[1] + m_pChimney.p[1] - pVal->p[1];
      m_pFireBox.p[1] = max(m_pFireBox.p[1], m_pChimney.p[1]);
      m_pFireBox.p[1] += (m_pChimney.p[1] - fOldChimWidth);
      m_pFireBox.p[1] = max(m_pFireBox.p[1], .01);
      m_pChimneyCap.p[1] +=  (m_pChimney.p[1] - fOldChimWidth);
      m_pChimneyCap.p[1] = max(m_pChimneyCap.p[1], .01);

      m_pHearth.p[2] = max(m_pBrickFloor.p[2]+.01, pVal->p[2]);
      m_pHearth.p[2] = min(m_pFireBox.p[2]-.01, m_pHearth.p[2]);
      break;

   case 62: // hearth curve
      m_pHearth.p[3] = max(0,pVal->p[2] - m_pHearth.p[2]);
      break;

   case 70:  // mantel
   case 71:
   case 72:
   case 73:
      dwVal = dwID - 70;

      m_pMantel.p[0] = max(fabs (pVal->p[0] * 2), .01);

      if (dwVal & 0x02) {
         m_pMantel.p[1] = pVal->p[1] - m_pFireplace.p[1] - m_pChimney.p[1] + m_pFireBox.p[1];   // front
         m_pMantel.p[1] = min(-.01, m_pMantel.p[1]);
      }
      else {
         m_pMantel.p[3] = pVal->p[1] - m_pFireplace.p[1];
         m_pMantel.p[3] = max(0,m_pMantel.p[3]);
      }

      m_pMantel.p[2] = max(0,pVal->p[2] - m_pFireBox.p[2]);
      break;

   default:
      if (m_pWorld)
         m_pWorld->ObjectChanged (this);
      return FALSE;
   }

   m_fNoodleDirty = TRUE;

   // update the cutout
   EmbedDoCutout();

   // tell the world we've changed
   if (m_pWorld)
      m_pWorld->ObjectChanged (this);
   return TRUE;
}

/*************************************************************************************
CObjectFireplace::ControlPointEnum - Called to enumerate a list of control point IDs
into the list.

inputs
   PCListFixed       plDWORD - Should be filled in with control points
returns
   none
*/
void CObjectFireplace::ControlPointEnum (PCListFixed plDWORD)
{
   DWORD i;

   // chimney
   for (i = 10; i < 18; i++)
      plDWORD->Add (&i);

   // chimney cap
   for (i = 20; i < 24; i++)
      plDWORD->Add (&i);

   // fireplace
   for (i = 30; i < 38; i++)
      plDWORD->Add (&i);

   // firebox
   for (i = 40; i < 45; i++)
      plDWORD->Add (&i);

   // brick floor
   for (i = 50; i < 54; i++)
      plDWORD->Add (&i);

   // hearth
   for (i = 60; i < 63; i++)
      plDWORD->Add (&i);

   // mantel
   for (i = 70; i < 74; i++)
      plDWORD->Add (&i);
}



