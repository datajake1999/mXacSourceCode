/************************************************************************
CObjectTruss.cpp - Draws a Truss.

begun 14/9/01 by Mike Rozak
Copyright 2001 Mike Rozak. All rights reserved
*/

#include <windows.h>
#include "escarpment.h"
#include "..\M3D.h"
#include "resource.h"

/**********************************************************************************
CObjectTruss::Constructor and destructor */
CObjectTruss::CObjectTruss (PVOID pParams, POSINFO pInfo)
{
   m_dwRenderShow = RENDERSHOW_MISC;
   m_OSINFO = *pInfo;

   m_lPCNoodleMajor.Init (sizeof(PCNoodle));
   m_lPCNoodleMinor.Init (sizeof(PCNoodle));
   m_lMember.Init (sizeof(TEXTUREPOINT));

   CPoint   ap[3];
   switch ((DWORD)(size_t) pParams) {
   case 3:
      m_dwShape = 3;
      break;
   case 4:
      m_dwShape = 4;
      break;
   default:
      m_dwShape = 2;
   }
   memset (ap, 0, sizeof(ap));
   ap[0].p[0] = -2;
   if (m_dwShape == 2)
      ap[1].p[2] = 2;
   else
      ap[1].p[0] = 2;
   ap[2].p[0] = 2;
   m_sTruss.Init (FALSE, (m_dwShape == 2) ? 3: 2, ap, NULL, (DWORD*) SEGCURVE_LINEAR, 3, 3, .1);
   ap[1].Copy (&ap[2]);
   m_sTrussBottom.Init (FALSE, 2, ap, NULL, (DWORD*) SEGCURVE_LINEAR, 2, 2, .1);
   m_dwMembersStyle = 2;
   m_dwAltVerticals = 0;
   m_dwMembers = 5;
   m_fMirrorStyle = FALSE;
   m_dwShowBeam = (DWORD)-1;
   m_adwBeamStyle[0] = m_adwBeamStyle[1] = NS_RECTANGLE;
   m_apBeamSize[0].Zero();
   m_apBeamSize[0].p[0] = m_apBeamSize[0].p[1] = .075;
   m_apBeamSize[1].Zero();
   m_apBeamSize[1].p[0] = m_apBeamSize[1].p[1] = .05;
   m_fMultiSize = 1;

   // calculate the noodles
   CalcInfo();

   // Need two colors for trusses, major and minor beam
   ObjectSurfaceAdd (1, RGB(0xc0,0xc0,0xc0), MATERIAL_PAINTMATTE, L"Truss beams");
   ObjectSurfaceAdd (2, RGB(0x80,0x80,0x80), MATERIAL_PAINTMATTE, L"Truss members");
}


CObjectTruss::~CObjectTruss (void)
{
   DWORD i;
   PCNoodle pn;
   for (i = 0; i < m_lPCNoodleMajor.Num(); i++) {
      pn = *((PCNoodle*) m_lPCNoodleMajor.Get(i));
      delete pn;
   }
   m_lPCNoodleMajor.Clear();
   for (i = 0; i < m_lPCNoodleMinor.Num(); i++) {
      pn = *((PCNoodle*) m_lPCNoodleMinor.Get(i));
      delete pn;
   }
   m_lPCNoodleMinor.Clear();
   m_lMember.Clear();
}


/**********************************************************************************
CObjectTruss::Delete - Called to delete this object
*/
void CObjectTruss::Delete (void)
{
   delete this;
}

/**********************************************************************************
CObjectTruss::Render - Draws a test box.

inputs
   POBJECTRENDER     pr - Render information
*/
void CObjectTruss::Render (POBJECTRENDER pr, DWORD dwSubObject)
{
   // create the surface render object and draw
   // CRenderSurface rs;
   m_Renderrs.ClearAll();

   CMatrix mObject;
   m_Renderrs.Init (pr->pRS);
   ObjectMatrixGet (&mObject);
   m_Renderrs.Multiply (&mObject);

   // object specific

   // draw the old noodles
   DWORD i;
   PCNoodle pn;
   m_Renderrs.SetDefMaterial (m_OSINFO.dwRenderShard, ObjectSurfaceFind (1), m_pWorld);
   for (i = 0; i < m_lPCNoodleMajor.Num(); i++) {
      pn = *((PCNoodle*) m_lPCNoodleMajor.Get(i));
      pn->Render (pr, &m_Renderrs);
   }

   m_Renderrs.SetDefMaterial (m_OSINFO.dwRenderShard, ObjectSurfaceFind (2), m_pWorld);
   for (i = 0; i < m_lPCNoodleMinor.Num(); i++) {
      pn = *((PCNoodle*) m_lPCNoodleMinor.Get(i));
      pn->Render (pr, &m_Renderrs);
   }

   m_Renderrs.Commit();
}


/**********************************************************************************
CObjectTruss::QueryBoundingBox - Standard API
*/
void CObjectTruss::QueryBoundingBox (PCPoint pCorner1, PCPoint pCorner2, DWORD dwSubObject)
{
   BOOL fSet = FALSE;
   CPoint p1, p2;

   DWORD i;
   PCNoodle pn;
   for (i = 0; i < m_lPCNoodleMajor.Num(); i++) {
      pn = *((PCNoodle*) m_lPCNoodleMajor.Get(i));

      if (fSet) {
         pn->QueryBoundingBox (&p1, &p2);
         pCorner1->Min (&p1);
         pCorner2->Max (&p2);
      }
      else {
         pn->QueryBoundingBox (pCorner1, pCorner2);
         fSet = TRUE;
      }
   }

   for (i = 0; i < m_lPCNoodleMinor.Num(); i++) {
      pn = *((PCNoodle*) m_lPCNoodleMinor.Get(i));
      if (fSet) {
         pn->QueryBoundingBox (&p1, &p2);
         pCorner1->Min (&p1);
         pCorner2->Max (&p2);
      }
      else {
         pn->QueryBoundingBox (pCorner1, pCorner2);
         fSet = TRUE;
      }
   }

#ifdef _DEBUG
   // test, make sure bounding box not too small
   //CPoint p1,p2;
   //DWORD i;
   CObjectTemplate::QueryBoundingBox (&p1, &p2, dwSubObject);
   for (i = 0; i < 3; i++)
      if ((p1.p[i] < pCorner1->p[i]) || (p2.p[i] > pCorner2->p[i]))
         break;
   if (i < 3)
      OutputDebugString ("\r\nCObjectTruss::QueryBoundingBox too small.");
#endif
}

/**********************************************************************************
CObjectTruss::Clone - Clones this

inputs
   none
returns
   PCObjectSocket - Clone
*/
PCObjectSocket CObjectTruss::Clone (void)
{
   PCObjectTruss pNew;

   pNew = new CObjectTruss(NULL, &m_OSINFO);
   DWORD i;
   PCNoodle pn;
   for (i = 0; i < pNew->m_lPCNoodleMajor.Num(); i++) {
      pn = *((PCNoodle*) pNew->m_lPCNoodleMajor.Get(i));
      delete pn;
   }
   pNew->m_lPCNoodleMajor.Clear();
   for (i = 0; i < pNew->m_lPCNoodleMinor.Num(); i++) {
      pn = *((PCNoodle*) pNew->m_lPCNoodleMinor.Get(i));
      delete pn;
   }
   pNew->m_lPCNoodleMinor.Clear();
   pNew->m_lMember.Clear();

   // clone template info
   CloneTemplate(pNew);

   m_sTruss.CloneTo (&pNew->m_sTruss);
   m_sTrussBottom.CloneTo (&pNew->m_sTrussBottom);
   pNew->m_dwShape  = m_dwShape;
   pNew->m_dwMembers = m_dwMembers;
   pNew->m_dwMembersStyle = m_dwMembersStyle;
   pNew->m_dwAltVerticals = m_dwAltVerticals;
   pNew->m_fMirrorStyle = m_fMirrorStyle;
   pNew->m_dwShowBeam = m_dwShowBeam;
   memcpy (pNew->m_adwBeamStyle, m_adwBeamStyle, sizeof(m_adwBeamStyle));
   memcpy (pNew->m_apBeamSize, m_apBeamSize, sizeof(m_apBeamSize));
   pNew->m_fMultiSize = m_fMultiSize;

   // clone the noodles
   for (i = 0; i < m_lPCNoodleMajor.Num(); i++) {
      pn = *((PCNoodle*) m_lPCNoodleMajor.Get(i));
      pn = pn->Clone();
      if (pn)
         pNew->m_lPCNoodleMajor.Add (&pn);
   }
   for (i = 0; i < m_lPCNoodleMinor.Num(); i++) {
      pn = *((PCNoodle*) m_lPCNoodleMinor.Get(i));
      pn = pn->Clone();
      if (pn)
         pNew->m_lPCNoodleMinor.Add (&pn);
   }
   pNew->m_lMember.Init (sizeof(TEXTUREPOINT), m_lMember.Get(0), m_lMember.Num());
   return pNew;
}

static PWSTR gpszTruss = L"Truss";
static PWSTR gpszTrussBottom = L"TrussBottom";
static PWSTR gpszShape = L"Shape";
static PWSTR gpszMembersStyle = L"MembersStyle";
static PWSTR gpszAltVerticals = L"AltVerticals";
static PWSTR gpszMembers = L"Members";
static PWSTR gpszMirrorStyle = L"MirrorStyle";
static PWSTR gpszShowBeam = L"ShowBeam";
static PWSTR gpszBeamStyle = L"BeamStyle%d";
static PWSTR gpszBeamSize = L"BeamSize%d";
static PWSTR gpszMultiSize = L"MultiSize";

PCMMLNode2 CObjectTruss::MMLTo (void)
{
   // call into the template first
   PCMMLNode2 pNode = MMLToTemplate();
   if (!pNode)
      return NULL;

   PCMMLNode2 pSub;
   pSub = m_sTruss.MMLTo();
   if (pSub) {
      pSub->NameSet (gpszTruss);
      pNode->ContentAdd (pSub);
   }
   pSub = m_sTrussBottom.MMLTo();
   if (pSub) {
      pSub->NameSet (gpszTrussBottom);
      pNode->ContentAdd (pSub);
   }

   MMLValueSet (pNode, gpszShape, (int) m_dwShape);
   MMLValueSet (pNode, gpszMembersStyle, (int) m_dwMembersStyle);
   MMLValueSet (pNode, gpszMembers, (int) m_dwMembers);
   MMLValueSet (pNode, gpszAltVerticals, (int) m_dwAltVerticals);
   MMLValueSet (pNode, gpszMirrorStyle, (int) m_fMirrorStyle);
   MMLValueSet (pNode, gpszShowBeam, (int)m_dwShowBeam);
   MMLValueSet (pNode, gpszMultiSize, m_fMultiSize);

   DWORD i;
   WCHAR szTemp[32];
   for (i = 0; i < 2; i++) {
      swprintf (szTemp, gpszBeamStyle, (int) i);
      MMLValueSet (pNode, szTemp, (int) m_adwBeamStyle[i]);

      swprintf (szTemp, gpszBeamSize, (int) i);
      MMLValueSet (pNode, szTemp, &m_apBeamSize[i]);
   }

   return pNode;
}

BOOL CObjectTruss::MMLFrom (PCMMLNode2 pNode)
{
   // call into the template first
   if (!MMLFromTemplate (pNode))
      return FALSE;

   PCMMLNode2 pSub;
   PWSTR psz;
   pSub = NULL;
   pNode->ContentEnum (pNode->ContentFind(gpszTruss), &psz, &pSub);
   if (pSub)
      m_sTruss.MMLFrom (pSub);
   pSub = NULL;
   pNode->ContentEnum (pNode->ContentFind(gpszTrussBottom), &psz, &pSub);
   if (pSub)
      m_sTrussBottom.MMLFrom (pSub);


   m_dwShape = (DWORD) MMLValueGetInt (pNode, gpszShape, (int) 2);
   m_dwMembersStyle = (DWORD) MMLValueGetInt (pNode, gpszMembersStyle, (int) 0);
   m_dwAltVerticals = (DWORD) MMLValueGetInt (pNode, gpszAltVerticals, (int) 0);
   m_dwMembers = (DWORD) MMLValueGetInt (pNode, gpszMembers, (int) 1);
   m_fMirrorStyle = (BOOL) MMLValueGetInt (pNode, gpszMirrorStyle, (int) 0);
   m_dwShowBeam = (DWORD) MMLValueGetInt (pNode, gpszShowBeam, (int)0);
   m_fMultiSize = MMLValueGetDouble (pNode, gpszMultiSize, 1);

   DWORD i;
   WCHAR szTemp[32];
   for (i = 0; i < 2; i++) {
      swprintf (szTemp, gpszBeamStyle, (int) i);
      m_adwBeamStyle[i] = MMLValueGetInt (pNode, szTemp, (int) NS_RECTANGLE);

      swprintf (szTemp, gpszBeamSize, (int) i);
      MMLValueGetPoint (pNode, szTemp, &m_apBeamSize[i]);
   }

   // Recalc noodles
   CalcInfo();

   return TRUE;
}

/************************************************************************************
CObjectTruss::CalcInfo - Recalculate the noodles and locaitons for the trusses
*/
void CObjectTruss::CalcInfo (void)
{
   CMem  memPoints, memCurve;
   PCSpline pExtend[4];
   DWORD dwNumExtend = 0;
   memset (pExtend, 0, sizeof(pExtend));

   // clear out the old noodles
   DWORD i;
   PCNoodle pn;
   for (i = 0; i < m_lPCNoodleMajor.Num(); i++) {
      pn = *((PCNoodle*) m_lPCNoodleMajor.Get(i));
      delete pn;
   }
   m_lPCNoodleMajor.Clear();
   for (i = 0; i < m_lPCNoodleMinor.Num(); i++) {
      pn = *((PCNoodle*) m_lPCNoodleMinor.Get(i));
      delete pn;
   }
   m_lPCNoodleMinor.Clear();
   m_lMember.Clear();

   CPoint pFront;
   pFront.Zero();
   pFront.p[1] = -1;

   fp fLeft, fRight;
   fLeft = fRight = 0;
   if (m_dwShape == 2) {
      // Reposition for 2-d truss so evenly spaced along X

      // find furthest left and right that can actually make truss
      fLeft = max(min(m_sTruss.LocationGet(0)->p[0], m_sTruss.LocationGet(m_sTruss.QueryNodes()-1)->p[0]),
         min(m_sTrussBottom.LocationGet(0)->p[0], m_sTrussBottom.LocationGet(m_sTrussBottom.QueryNodes()-1)->p[0]));
      fRight = min(max(m_sTruss.LocationGet(0)->p[0], m_sTruss.LocationGet(m_sTruss.QueryNodes()-1)->p[0]),
         max(m_sTrussBottom.LocationGet(0)->p[0], m_sTrussBottom.LocationGet(m_sTrussBottom.QueryNodes()-1)->p[0]));
      if (fRight - fLeft < CLOSE)
         goto bigtruss;  // error, too close
   }

   // calculate where the new truss memebers go
   // start out evenly spaced
   for (i = 0; i < m_dwMembers; i++) {
      TEXTUREPOINT tp;
      tp.h = tp.v = m_dwMembers ? ((fp) i / (fp) (m_dwMembers - 1)) : .5;

      // reposition so vertical representation
      if (m_dwShape == 2) {
         tp.h = tp.v = (1.0 - tp.h) * fLeft + tp.h * fRight;
         tp.h = m_sTruss.FindCrossing (0, tp.h);
         tp.v = m_sTrussBottom.FindCrossing (0, tp.v);
         if ((tp.h < 0) || (tp.v < 0))
            continue;   // out of bounds
      }

      m_lMember.Add (&tp);
   }
   PTEXTUREPOINT pt;
   pt = (PTEXTUREPOINT) m_lMember.Get(0);

   // create the major noodles
   if (m_dwShape == 3) {
      CPoint pUp;
      dwNumExtend = 3;
      pExtend[0] = &m_sTruss;

      // lower one
      pUp.Zero();
      pUp.p[1] = -1;
      pExtend[2] = pExtend[0]->Expand (-m_fMultiSize * sin(PI/3), &pUp);
      if (!pExtend[2])
         goto alldone;

      pUp.Zero();
      pUp.p[2] = -1;
      pExtend[1] = pExtend[2]->Expand (-m_fMultiSize * cos(PI/3), &pUp);
      delete pExtend[2];
      pExtend[2] = NULL;
      if (!pExtend[1])
         goto alldone;

      // other top one
      pUp.Zero();
      pUp.p[2] = 1;
      pExtend[2] = pExtend[0]->Expand (m_fMultiSize, &pUp);
      if (!pExtend[2])
         goto alldone;
   }
   else if (m_dwShape == 4) {
      CPoint pUp;
      dwNumExtend = 4;
      pExtend[0] = &m_sTruss;

      // lower right one
      pUp.Zero();
      pUp.p[1] = -1;
      pExtend[1] = pExtend[0]->Expand (-m_fMultiSize, &pUp);
      if (!pExtend[1])
         goto alldone;

      // lower left one
      pUp.Zero();
      pUp.p[2] = -1;
      pExtend[2] = pExtend[1]->Expand (-m_fMultiSize, &pUp);
      if (!pExtend[2])
         goto alldone;

      // upper left one
      pUp.Zero();
      pUp.p[2] = 1;
      pExtend[3] = pExtend[0]->Expand (m_fMultiSize, &pUp);
      if (!pExtend[3])
         goto alldone;
   }
   else {   // m_dwShape == 2
      // remember these two
      dwNumExtend = 2;
      pExtend[0] = &m_sTruss;
      pExtend[1] = &m_sTrussBottom;
   }

bigtruss:
   // 2d trusses
   DWORD dwPoints, dwMin, dwMax;
   BOOL fLooped;
   fp fDetail;

   for (i = 0; i < dwNumExtend; i++) {
      // If hidden then dont draw
      if (!(m_dwShowBeam & (1 << i)))
         continue;

      pn = new CNoodle;
      if (!pn)
         goto alldone;
      pn->BackfaceCullSet (TRUE);
      pn->DrawEndsSet (TRUE);
      // NOTE: Not bevelling the ends
      pn->FrontVector (&pFront);
      pn->ScaleVector (&m_apBeamSize[0]);
      pn->ShapeDefault (m_adwBeamStyle[0]);
      pExtend[i]->ToMem (&fLooped, &dwPoints, &memPoints, NULL, &memCurve, &dwMin, &dwMax, &fDetail);
      pn->PathSpline (fLooped, dwPoints, (PCPoint) memPoints.p, (DWORD*) memCurve.p, dwMax);
      m_lPCNoodleMajor.Add (&pn);
   }


   // creat the minor noodles
   DWORD j;
   for (j = 0; j < dwNumExtend; j++) {
      if ((dwNumExtend == 2) && j)
         break;   // only do one pass for 2D

      for (i = 0; i < m_lMember.Num(); i++) {
         // get location of top and bottom
         CPoint pTop, pBottom, pTopTan, pBottomTan, pDiag, pPerp, pNorm;
         pExtend[j]->LocationGet (pt[i].h, &pTop);
         pExtend[(j+1)%dwNumExtend]->LocationGet (pt[i].v, &pBottom);
         pExtend[j]->TangentGet (pt[i].h, &pTopTan);
         pExtend[(j+1)%dwNumExtend]->TangentGet (pt[i].v, &pBottomTan);


         // figure out which diagonals
         BOOL fGoingUp, fGoingDown;
         fGoingUp = fGoingDown = FALSE;
         switch (m_dwMembersStyle) {
         case 0:  // x
            fGoingUp = fGoingDown = TRUE;
            break;
         case 1:  // triangle odd
            fGoingUp = (i%2) ? TRUE : FALSE;
            fGoingDown = (i%2) ? FALSE : TRUE;
            break;
         case 2:  // tiranlg even
            fGoingDown = (i%2) ? TRUE : FALSE;
            fGoingUp = (i%2) ? FALSE : TRUE;
            break;
         case 3:  // saw odd
            fGoingUp = TRUE;
            break;
         case 4:  // saw even
            fGoingDown = TRUE;
            break;
         }

         BOOL fAlt;
         fAlt = m_fMirrorStyle && (pTop.p[0] + CLOSE >= (fLeft+fRight)/2);
         // May want to alternate every dwNumExtend
         if ((dwNumExtend == 4) && (j % 2))
            fAlt = !fAlt;
         if (fAlt) {
            BOOL fTemp;
            fTemp = fGoingUp;
            fGoingUp = fGoingDown;
            fGoingDown = fTemp;
         }

         // FUTURERELEASE - Make top and bottom shorter so just intersect

         // draw diagonals
         if ((fGoingUp || fGoingDown) && (i+1 < m_lMember.Num())) {
            // get points to right
            CPoint pTopRight, pBottomRight, pTopRightTan, pBottomRightTan;
            pExtend[j]->LocationGet (pt[i+1].h, &pTopRight);
            pExtend[(j+1)%dwNumExtend]->LocationGet (pt[i+1].v, &pBottomRight);
            pExtend[j]->TangentGet (pt[i+1].h, &pTopRightTan);
            pExtend[(j+1)%dwNumExtend]->TangentGet (pt[i+1].v, &pBottomRightTan);

            if (fGoingUp && !pBottom.AreClose (&pTopRight)) {
               pn = new CNoodle;
               if (!pn)
                  goto alldone;
               pn->BackfaceCullSet (TRUE);
               pn->DrawEndsSet (FALSE);
               pn->FrontVector (&pFront);
               pn->ScaleVector (&m_apBeamSize[1]);
               pn->ShapeDefault (m_adwBeamStyle[1]);
               pn->PathLinear (&pTopRight, &pBottom);

               // tangent
               pDiag.Subtract (&pTopRight, &pBottom);
               pDiag.Normalize();
               pPerp.CrossProd (&pDiag, &pTopRightTan);
               pNorm.CrossProd (&pPerp, &pTopRightTan);
               if (pPerp.Length() > .1)
                  pn->BevelSet (TRUE, 2, &pNorm);
               pPerp.CrossProd (&pDiag, &pBottomTan);
               pNorm.CrossProd (&pPerp, &pBottomTan);
               if (pPerp.Length() > .1)
                  pn->BevelSet (FALSE, 2, &pNorm);

               m_lPCNoodleMinor.Add (&pn);
            }

            if (fGoingDown && !pTop.AreClose (&pBottomRight)) {
               pn = new CNoodle;
               if (!pn)
                  goto alldone;
               pn->BackfaceCullSet (TRUE);
               pn->DrawEndsSet (FALSE);
               pn->FrontVector (&pFront);
               pn->ScaleVector (&m_apBeamSize[1]);
               pn->ShapeDefault (m_adwBeamStyle[1]);
               pn->PathLinear (&pTop, &pBottomRight);
               m_lPCNoodleMinor.Add (&pn);

               // tangent
               pDiag.Subtract (&pTop, &pBottomRight);
               pDiag.Normalize();
               pPerp.CrossProd (&pDiag, &pTopTan);
               pNorm.CrossProd (&pPerp, &pTopTan);
               if (pPerp.Length() > .1)
                  pn->BevelSet (TRUE, 2, &pNorm);
               pPerp.CrossProd (&pDiag, &pBottomRightTan);
               pNorm.CrossProd (&pPerp, &pBottomRightTan);
               if (pPerp.Length() > .1)
                  pn->BevelSet (FALSE, 2, &pNorm);
            }
         }

         // vertical
         if ((m_dwAltVerticals == 1) && !(i % 2))
            continue;   // doing odd verticals
         if ((m_dwAltVerticals == 2) && (i % 2))
            continue;   // doing even verticals
         if (!pTop.AreClose (&pBottom)) {
            pn = new CNoodle;
            if (!pn)
               goto alldone;
            pn->BackfaceCullSet (TRUE);
            pn->DrawEndsSet (FALSE);
            pn->FrontVector (&pFront);
            pn->ScaleVector (&m_apBeamSize[1]);
            pn->ShapeDefault (m_adwBeamStyle[1]);
            pn->PathLinear (&pTop, &pBottom);

            // tangent
            pDiag.Subtract (&pTop, &pBottom);
            pDiag.Normalize();
            pPerp.CrossProd (&pDiag, &pTopTan);
            pNorm.CrossProd (&pPerp, &pTopTan);
            if (pPerp.Length() > .1)
               pn->BevelSet (TRUE, 2, &pNorm);
            pPerp.CrossProd (&pDiag, &pBottomTan);
            pNorm.CrossProd (&pPerp, &pBottomTan);
            if (pPerp.Length() > .1)
               pn->BevelSet (FALSE, 2, &pNorm);

            m_lPCNoodleMinor.Add (&pn);
         }

      }
   }  // dwNumExtend

alldone:
   for (i = 0; i < 4; i++)
      if (pExtend[i] && (pExtend[i] != &m_sTruss) && (pExtend[i] != &m_sTrussBottom))
         delete pExtend[i];

}


/**************************************************************************************
CObjectTruss::MoveReferencePointQuery - 
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
BOOL CObjectTruss::MoveReferencePointQuery (DWORD dwIndex, PCPoint pp)
{
   if (dwIndex > m_sTruss.OrigNumPointsGet())
      return FALSE;

   if (dwIndex)
      m_sTruss.OrigPointGet (dwIndex-1, pp);
   else {
      DWORD i;
      CPoint p;

      pp->Zero();
      for (i = 0; i < m_sTruss.OrigNumPointsGet(); i++) {
         m_sTruss.OrigPointGet (i, &p);
         pp->Add (&p);
      }
      pp->Scale (1.0 / (fp) m_sTruss.OrigNumPointsGet());
   }

   return TRUE;
}

/**************************************************************************************
CObjectTruss::MoveReferenceStringQuery -
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
BOOL CObjectTruss::MoveReferenceStringQuery (DWORD dwIndex, PWSTR psz, DWORD dwSize, DWORD *pdwNeeded)
{
   if (dwIndex > m_sTruss.OrigNumPointsGet())
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
CObjectTruss::ControlPointQuery - Called to query the information about a control
point identified by dwID.

inputs
   DWORD       dwID - ID
   POSCONTROL  pInfo - Filled in with the infomration
returns
   BOOL - TRUE if successful
*/
BOOL CObjectTruss::ControlPointQuery (DWORD dwID, POSCONTROL pInfo)
{
   DWORD dwNum = dwID;
   PCSpline ps = NULL;
   if (dwNum < m_sTrussBottom.OrigNumPointsGet()) {
      ps = &m_sTrussBottom;
   }
   else {
      dwNum -= m_sTrussBottom.OrigNumPointsGet();

      if (dwNum < m_sTruss.OrigNumPointsGet())
         ps = &m_sTruss;
   }
   if (!ps)
      return FALSE;

   fp fKnobSize;
   fKnobSize = max(max(m_apBeamSize[0].p[0], m_apBeamSize[0].p[1]),
      max(m_apBeamSize[1].p[0], m_apBeamSize[1].p[1])) * 2;


   memset (pInfo,0, sizeof(*pInfo));
   pInfo->dwID = dwID;
   //pInfo->dwFreedom = 0;   // any direction
   pInfo->fSize = fKnobSize;

   pInfo->dwStyle = CPSTYLE_SPHERE;
   pInfo->cColor = RGB(0xff,0,0xff);
   wcscpy (pInfo->szName, (ps == &m_sTruss) ? L"Truss" : L"Truss bottom");
   ps->OrigPointGet (dwNum, &pInfo->pLocation);

   return TRUE;
}

/*************************************************************************************
CObjectTruss::ControlPointSet - Called to change the valud of a control point.

inputs
   DWORD       dwID - ID
   PCPoint     pVal - Contains the new location, in object coordinates
returns
   BOOL - TRUE if successful
*/
BOOL CObjectTruss::ControlPointSet (DWORD dwID, PCPoint pVal, PCPoint pViewer)
{
   DWORD dwNum = dwID;
   PCSpline ps = NULL;
   if (dwNum < m_sTrussBottom.OrigNumPointsGet()) {
      ps = &m_sTrussBottom;
   }
   else {
      dwNum -= m_sTrussBottom.OrigNumPointsGet();

      if (dwNum < m_sTruss.OrigNumPointsGet())
         ps = &m_sTruss;
   }
   if (!ps)
      return FALSE;

   // tell the world we're about to change
   if (m_pWorld)
      m_pWorld->ObjectAboutToChange (this);

   CPoint pV;
   pV.Copy (pVal);
   pV.p[1] = 0;

   ps->OrigPointReplace (dwNum, &pV);

   CalcInfo();

   // tell the world we've changed
   if (m_pWorld)
      m_pWorld->ObjectChanged (this);
   return TRUE;
}

/*************************************************************************************
CObjectTruss::ControlPointEnum - Called to enumerate a list of control point IDs
into the list.

inputs
   PCListFixed       plDWORD - Should be filled in with control points
returns
   none
*/
void CObjectTruss::ControlPointEnum (PCListFixed plDWORD)
{
   DWORD i;

   DWORD dwNum, dwMin;

   dwMin = m_sTrussBottom.OrigNumPointsGet();
   dwNum = dwMin + m_sTruss.OrigNumPointsGet();
   if (m_dwShape == 2)
      dwMin = 0;
   for (i = dwMin; i < dwNum; i++)
      plDWORD->Add (&i);
}



/* TrussDialogPage
*/
BOOL TrussDialogPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCObjectTruss pv = (PCObjectTruss) pPage->m_pUserData;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         PCEscControl pControl;
         ComboBoxSet (pPage, L"beamstyle0", pv->m_adwBeamStyle[0]);
         ComboBoxSet (pPage, L"beamstyle1", pv->m_adwBeamStyle[1]);
         ComboBoxSet (pPage, L"membersstyle", pv->m_dwMembersStyle);
         ComboBoxSet (pPage, L"altverticals", pv->m_dwAltVerticals);

         DWORD i,j;
         WCHAR szTemp[32];
         for (i = 0; i < 2; i++) for (j = 0; j < 2; j++) {
            swprintf (szTemp, L"beamsize%d%d", (int)i, (int)j);
            MeasureToString (pPage, szTemp, pv->m_apBeamSize[i].p[j]);
         }
         MeasureToString (pPage, L"multisize", pv->m_fMultiSize);
         if (pv->m_dwShape == 2) {
            pControl = pPage->ControlFind (L"multisize");
            if (pControl)
               pControl->Enable (FALSE);
         }
         DoubleToControl (pPage, L"members", (fp) pv->m_dwMembers);

         for (i = 0; i < 4; i++) {
            swprintf (szTemp, L"showbeam%d", (int)i);
            pControl = pPage->ControlFind (szTemp);
            if (pControl) {
               pControl->AttribSetBOOL (Checked(), (pv->m_dwShowBeam & (1 << i)) ? TRUE : FALSE);
               pControl->Enable (i < pv->m_dwShape);
            }
         }
         pControl = pPage->ControlFind (L"mirrorstyle");
         if (pControl)
            pControl->AttribSetBOOL (Checked(), pv->m_fMirrorStyle);

         pControl = pPage->ControlFind (L"custom1");
         if (pControl)
            pControl->Enable (pv->m_dwShape == 2);
      }
      break;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;

         if (!p->pControl->m_pszName)
            break;
         PWSTR psz;
         psz = p->pControl->m_pszName;

         PWSTR pszShowBeam = L"showbeam";
         DWORD dwShowBeam = (DWORD)wcslen(pszShowBeam);

         if (!wcsncmp(psz, pszShowBeam, dwShowBeam)) {
            DWORD dw = min(_wtoi(psz + dwShowBeam), 3);
            pv->m_pWorld->ObjectAboutToChange (pv);
            if (p->pControl->AttribGetBOOL (Checked()))
               pv->m_dwShowBeam |= (1 << dw);
            else
               pv->m_dwShowBeam &= ~(1 << dw);
            pv->CalcInfo();
            pv->m_pWorld->ObjectChanged (pv);
            return TRUE;
         }

         if (!_wcsicmp(psz, L"mirrorstyle")) {
            pv->m_pWorld->ObjectAboutToChange (pv);
            pv->m_fMirrorStyle = p->pControl->AttribGetBOOL (Checked());
            pv->CalcInfo();
            pv->m_pWorld->ObjectChanged (pv);
            return TRUE;
         }

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

         if (!_wcsicmp(psz, L"beamstyle0") || !_wcsicmp(psz, L"beamstyle1")) {
            DWORD dw = !_wcsicmp(psz, L"beamstyle1");
            if (dwVal == pv->m_adwBeamStyle[dw])
               break;

            pv->m_pWorld->ObjectAboutToChange (pv);
            pv->m_adwBeamStyle[dw] = dwVal;
            pv->CalcInfo();
            pv->m_pWorld->ObjectChanged (pv);

            return TRUE;
         }
         else if (!_wcsicmp(psz, L"membersstyle")) {
            if (dwVal == pv->m_dwMembersStyle)
               break;

            pv->m_pWorld->ObjectAboutToChange (pv);
            pv->m_dwMembersStyle = dwVal;
            pv->CalcInfo();
            pv->m_pWorld->ObjectChanged (pv);

            return TRUE;
         }
         else if (!_wcsicmp(psz, L"altverticals")) {
            if (dwVal == pv->m_dwAltVerticals)
               break;

            pv->m_pWorld->ObjectAboutToChange (pv);
            pv->m_dwAltVerticals = dwVal;
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

         DWORD i,j;
         WCHAR szTemp[32];
         for (i = 0; i < 2; i++) for (j = 0; j < 2; j++) {
            swprintf (szTemp, L"beamsize%d%d", (int)i, (int)j);
            MeasureParseString (pPage, szTemp, &pv->m_apBeamSize[i].p[j]);
            pv->m_apBeamSize[i].p[j] = max(CLOSE, pv->m_apBeamSize[i].p[j]);
         }
         MeasureParseString (pPage, L"multisize", &pv->m_fMultiSize);
         pv->m_fMultiSize = max(CLOSE, pv->m_fMultiSize);
         pv->m_dwMembers = (DWORD)DoubleFromControl (pPage, L"members");

         pv->CalcInfo ();
         pv->m_pWorld->ObjectChanged (pv);


         break;
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Truss settings";
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}


typedef struct {
   PCObjectTruss     pv;
   PCSpline          ps;
} TCP, *PTCP;

/* TrussCurvePage
*/
BOOL TrussCurvePage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PTCP pt = (PTCP) pPage->m_pUserData;
   PCObjectTruss pv = pt->pv;
   PCSpline ps = pt->ps;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         PCEscControl pControl;

         // draw images
         if (ps) {
            NoodGenerateThreeDFromSpline (L"edgeaddremove", pPage, ps, ps, 0, TRUE);
            NoodGenerateThreeDFromSpline (L"edgecurve", pPage, ps, ps, 1, TRUE);

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
            ps->Init (fLooped, dwOrig,
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
         ps->Init (fLooped, dwOrig, paPoints, NULL, padw, dwMaxDivide, dwMaxDivide, .1);
         pv->CalcInfo();
         pv->m_pWorld->ObjectChanged (pv);

         // redraw the shapes
         if (ps) {
            NoodGenerateThreeDFromSpline (L"edgeaddremove", pPage, ps, ps, 0, TRUE);
            NoodGenerateThreeDFromSpline (L"edgecurve", pPage, ps, ps, 1, TRUE);
         }
      }
      break;


   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Truss curve";
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
CObjectTruss::DialogShow - Causes the object to show a dialog box that allows
different attributes to be changed.

inputs
   DWORD             dwSuface - Surface that was clicked on. 0 if unknown.
   PCEscWindow       pWindow - Escarpment window to draw it in
returns
   BOOL - TRUE if user pressed "Back", FALSE if user pressed "Close"
*/
BOOL CObjectTruss::DialogShow (DWORD dwSurface, PCEscWindow pWindow, PCPoint pClick)
{
   PWSTR pszRet;
mainpage:
   pszRet = pWindow->PageDialog (ghInstance, IDR_MMLTRUSSDIALOG, TrussDialogPage, this);
   if (!pszRet)
      return FALSE;
   if (!_wcsicmp(pszRet, L"custom0") || !_wcsicmp(pszRet, L"custom1")) {
      TCP tcp;
      memset (&tcp, 0, sizeof(tcp));
      tcp.pv = this;
      tcp.ps = (!_wcsicmp(pszRet, L"custom1")) ? &m_sTrussBottom : &m_sTruss;
      pszRet = pWindow->PageDialog (ghInstance, IDR_MMLNOODLECURVE, TrussCurvePage, &tcp);
      if (!pszRet)
         return FALSE;
      if (!_wcsicmp(pszRet, Back()))
         goto mainpage;
      // else fall through
   }

   return !_wcsicmp(pszRet, Back());
}


/**********************************************************************************
CObjectTruss::DialogQuery - Returns TRUE if the object supports a custom
dialog box, FALSE if it doesn't. The default object doesn't
*/
BOOL CObjectTruss::DialogQuery (void)
{
   return TRUE;
}



// FUTURERELEASE - Control point set/get for truss scaling?

