/**********************************************************************************
CPiers - Used for drawing Pierss.

begun 14/4/2002 by Mike Rozak
Copyright 2002 Mike Rozak. All rights reserved
*/

#include <windows.h>
#include "escarpment.h"
#include "..\M3D.h"
#include "resource.h"

// appearance info
typedef struct {
   PCPiers      pBal;    // balustreade
   PCObjectBuildBlock   pBB;  // Building block, if have one
   DWORD       dwRenderShard;
} APIN, *PAPIN;

// post information, used to keep track of what posts go where
typedef struct {
   fp         fLoc;       // location from 0.0 to .99999 where the post goes
   BOOL           fRight;     // TRUE if Piers is to the right (postive fLoc)
   BOOL           fLeft;      // TRUE if the Piers is to the left (negetive fLoc)
   DWORD          dwReason;   // reason for being. 0=corner, 1=cutout, 2=make up distance (big)
   PCColumn       pColumn;    // column used to draw the post
} BPOSTINFO, *PBPOSTINFO;

// noodle information - used for handrails
typedef struct {
   WORD           wColor;     // color type to use. 0 for handrail, 1 for top/bottom rail, 2 for brace, 3 horizontal, 4 vertical, 5 flat panel
   PCNoodle       pNoodle;    // noodle
   PCColumn       pColumn;    // column - used for vertical bits so can have point on top
   PCPoint        pPanel;     // points to an array of 4 points if allocated for a panel. Must be freed()
} BNOODLEINFO, *PBNOODLEINFO;

// BUGFIX - Changed many references of m_psBottom and m_psTop to m_psOrigBottom and m_psOrigTop

/***********************************************************************************
CPiers::DeterminePostLocations - Call this after any cutouts have been made.
It looks at:
   - Cutouts (m_lCutouts)
   - Spline inflections
   - Distances between above posts
And determines where the posts go. Fills in m_lPosts.
*/
BOOL CPiers::DeterminePostLocations (DWORD dwRenderShard)
{
   // clear existing info
   // May want to delete existing post objects
   DWORD i;
   PBPOSTINFO pbpi;
   for (i = 0; i < m_lPosts.Num(); i++) {
      pbpi = (PBPOSTINFO) m_lPosts.Get(i);
      if (pbpi->pColumn)
         delete pbpi->pColumn;
   }
   m_lPosts.Clear();

   // list of custouts
   PTEXTUREPOINT pCut;
   DWORD dwCutNum;
   pCut = (PTEXTUREPOINT) m_lCutouts.Get(0);
   dwCutNum = m_lCutouts.Num();

   // assume posts at inflection points
   DWORD dwNum, dwDivide, j;
   dwNum = m_psTop->OrigNumPointsGet();
   dwDivide = dwNum - (m_psTop->LoopedGet() ? 0 : 1);
   BPOSTINFO bpi;
   PBPOSTINFO pbpi2;
   memset (&bpi, 0, sizeof(bpi));
   m_lPosts.Required (dwNum);
   for (i = 0; i < dwNum; i++) {
      bpi.fLoc = (fp)i / (fp)dwDivide;
      bpi.fRight = bpi.fLeft = TRUE;
      bpi.dwReason = 0;

      // don't have anything to the left or right if at the ends
      if (!i && !m_psTop->LoopedGet())
         bpi.fLeft = FALSE;
      if ((i+1 == dwNum) && !m_psTop->LoopedGet())
         bpi.fRight = FALSE;


      // else, add it
      m_lPosts.Add (&bpi);
   }

   // start with big posts first and divide by those, then small posts.
   // Big posts try to go all the way to roof.
   fp fMaxDist = m_fMaxPostDistanceBig;
   DWORD dwReas, k;
   dwReas = 2;

   // go back through each of the posts and see if the distance between them is
   // too great. If so, insert more
   for (i = 0; i < m_lPosts.Num(); i++) {
      pbpi = (PBPOSTINFO) m_lPosts.Get(i);
      if (!pbpi->fRight)
         continue;   // nothing to right of this so don't checl
      if (pbpi->dwReason == dwReas)
         continue;   // dont divide if this is already a divide

      // get the one to the right of this
      pbpi2 = (PBPOSTINFO) m_lPosts.Get((i+1)%m_lPosts.Num());

      // as a not entirely brilliant solution, just divide into 10 bits and
      // sum the distances
      CPoint pLast, pCur, pDist;
      fp fDist, fStart, fEnd;
      fDist = 0;
      fStart = pbpi->fLoc;
      fEnd = pbpi2->fLoc;
      if (fEnd <= fStart)
         fEnd += 1.0;
      for (k = 0; k <= 10; k++, pLast.Copy(&pCur)) {
         // BUGFIX - was wrapping around on non-looped elements
         fp fVal;
         fVal = (fEnd-fStart) / 10.0 * (fp)k + fStart;
         if (m_psTop->LoopedGet())
            fVal = fmod(fVal, (fp)1.0);
         else
            fVal = min(fVal, 1.0);
         m_psTop->LocationGet(fVal, &pCur);
         if (k == 0)
            continue;   // not last for the first point

         pDist.Subtract (&pCur, &pLast);
         fDist += pDist.Length();
      }

      // if get here, need a post in-between
      DWORD dwPosts;
      dwPosts = (DWORD) (fDist / fMaxDist);
      if (!dwPosts)
         continue;   // close enough togehter

      // add them in reverse order
      for (j = 0; j < dwPosts; j++) {
         bpi.fLoc = (fp) (dwPosts - j) / (fp) (dwPosts+1) * (fEnd - fStart) + fStart;
         bpi.dwReason = dwReas;
         bpi.fRight = bpi.fLeft = TRUE;

         if (i+1 < m_lPosts.Num())
            m_lPosts.Insert (i+1, &bpi);
         else
            m_lPosts.Add (&bpi);
      }  // j
   } // i - posts in-between

   // create the post objects
   for (i = 0; i < m_lPosts.Num(); i++) {
      pbpi = (PBPOSTINFO) m_lPosts.Get(i);

      // may not want to add this if it's in a cutout region
      for (j = 0; j < dwCutNum; j++) {
         if ((pbpi->fLoc >= pCut[j].h - 2*CLOSE) && (pbpi->fLoc <= pCut[j].v + 2*CLOSE))
            break;
      }
      if (j < dwCutNum) {
         m_lPosts.Remove (i);
         i--;
         continue;   // in a cutout
      }

      // find out the points for the post
      CPoint pBottom, pTop, pDir;
      m_psBottom->LocationGet (pbpi->fLoc, &pBottom);
      m_psTop->LocationGet (pbpi->fLoc, &pTop);
      pDir.Subtract (&pBottom, &pTop);
      pDir.Normalize();
      if (pDir.Length() < .1)
         continue;   // on top of each other

      // figure out the front of the post
      fp fLook, fDirection;
      BOOL fFlip;
      CPoint pNext, pFront;
      fDirection = CLOSE;
      fFlip = FALSE;
      // if this is a post on a corner, then see which way it orients itself
      // based on which length is longer (to right or left). If both the same
      // then it chooses which is more E/W oriented
      if ((pbpi->dwReason == 0) && (pbpi->fLeft) && (pbpi->fRight)) {
         DWORD dwPoint;
         dwPoint = (DWORD) (pbpi->fLoc * (fp)dwDivide + .5) % dwNum;
         CPoint C, R, L;
         m_psTop->OrigPointGet (dwPoint, &C);
         m_psTop->OrigPointGet ((dwPoint+1)%dwNum, &R);
         m_psTop->OrigPointGet ((dwPoint+dwNum-1)%dwNum, &L);
         R.Subtract (&C);
         L.Subtract (&C);
         fp fDif;
         fDif = R.Length() - L.Length();
         if (fDif < -CLOSE) {
            fDirection *= -1;
            fFlip = !fFlip;
         }
         else if (fDif < CLOSE) {
            if (fabs(L.p[0]) > fabs(R.p[0])) {
               fDirection *= -1;
               fFlip = !fFlip;
            }
         }
         // else do nothing
      }
      fLook = pbpi->fLoc + fDirection;
      if ((fLook >= 1.0) && !m_psTop->LoopedGet()) {
         fLook = pbpi->fLoc - fDirection;
         fFlip = !fFlip;
      }
      if ((fLook < 0.0) && !m_psTop->LoopedGet()) {
         fLook = pbpi->fLoc + fDirection;
         fFlip = !fFlip;
      }
      m_psTop->LocationGet (fmod(fLook+1,1), &pNext);
      pNext.Subtract (&pTop);
      pNext.Normalize();
      if (pNext.Length() < .5)
         continue;   // on top of each other
      if (fFlip)
         pNext.Scale(-1.0);
      pFront.CrossProd (&pDir, &pNext);

      // For this pass, use the small posts. In a later pass
      // may enlarge to go all the way up to the roof

      // determine the height of the post
      fp fHeight;
      fHeight = m_fHeight;
      CPoint pRealBottom;
      pRealBottom.Copy (&pDir);
      pRealBottom.Scale (fHeight);
      pRealBottom.Add (&pTop);

      // create the column object
      PCColumn pc;
      pbpi->pColumn = pc = new CColumn;
      if (!pbpi->pColumn)
         continue;

      // set the bottom, top, and front
      pc->StartEndFrontSet (&pRealBottom, &pTop, &pFront);

      BOOL fLong;
      fLong = TRUE;
      CBASEINFO bi;
      memset (&bi, 0, sizeof(bi));
      bi.dwBevelMode = 0;  // top level
      bi.dwShape = m_adwPostShape[fLong ? 2 : 1];
      bi.dwTaper = m_adwPostTaper[fLong ? 2 : 1];
      bi.pSize.Copy (&m_apPostSize[fLong ? 2 : 1]);
      bi.fUse = m_afPostUse[fLong ? 2 : 1];
      bi.fCapped = !fLong;
      pc->BaseInfoSet (FALSE, &bi);

      bi.dwBevelMode = 2;  // on the bottom bevel accoring to angle out/in
      bi.dwShape = m_adwPostShape[0];
      bi.dwTaper = m_adwPostTaper[0];
      bi.pSize.Copy (&m_apPostSize[0]);
      bi.fUse = m_afPostUse[0];
      bi.fCapped = FALSE;  // never cap this since should be against floor
      bi.pBevelNorm.Zero();
      bi.pBevelNorm.p[2] = 1;
      pc->BaseInfoSet (TRUE, &bi);

      pc->ShapeSet (m_adwPostShape[4]);
      pc->SizeSet (&m_apPostSize[4]);

      // bracing?
      // clear bracing that's there
      DWORD dwBrace;
      CBRACEINFO bri;
      memset (&bri, 0, sizeof(bri));
      for (dwBrace = 0; dwBrace < COLUMNBRACES; dwBrace++) {
         pc->BraceInfoSet (dwBrace, &bri);
      }



      // create the bracing. But only do so if it intersects and we want bracing
      if (m_afPostUse[3]) {
         pc->BraceStartSet (m_fBraceBelow);
         fp fBraceHeight;
         fBraceHeight = m_fBraceBelow;

         // find out where this is
         CPoint pBrace;
         pBrace.Copy (&pDir);
         pBrace.Scale (fBraceHeight);
         pBrace.Add (&pTop);

         // determine the right and left of this point and create a brace there
         for (dwBrace = 0; dwBrace < 2; dwBrace++) {
            fp fLoc;
            fLoc = pbpi->fLoc + (dwBrace ? CLOSE : (-CLOSE));  // dwBrace == 1 then to right
            // it we're not looped then don't brace beyond end
            if (!m_psTop->LoopedGet()) {
               if ((fLoc < 0.0) || (fLoc > 1.0))
                  continue;
            }
            // do modulo loc
            fLoc = fmod(fLoc + 1.0, 1.0);

            // if there's nothing to left or right then skip
            if (dwBrace && !pbpi->fRight)
               continue;
            if (!dwBrace && !pbpi->fLeft)
               continue;

            CPoint pBraceDir;
            if (!m_psTop->LocationGet (fLoc, &pBraceDir))
               continue;

            // subtract this from start and normalize
            pBraceDir.Subtract (&pTop);
            pBraceDir.Normalize();

            // add in an equal measure of up so that it's 45 degrees
            CPoint pDir2;
            pDir2.Subtract (&pBottom, &pTop);
            pDir2.Normalize();
            pBraceDir.Subtract (&pDir2);
            pBraceDir.Normalize();
            CPoint pBraceEnd;
            pBraceEnd.Add (&pBrace, &pBraceDir);

            // loop through all surfaces and find intersection
            BOOL fIntersected;
            CPoint pUp, pInter;
            pUp.Zero();
            pUp.p[2] = 1;
            fIntersected = IntersectLinePlane (&pBrace, &pBraceEnd, &pTop, &pUp, &pInter);

            // if didn't intersect, or brace ends up being really long
            // then dont bother
            if (!fIntersected)
               continue;

            // else, add brace
            memset (&bri, 0, sizeof(bri));
            bri.dwBevelMode = 2;
            bri.dwShape = m_adwPostShape[3];
            bri.fUse = TRUE;
            bri.pBevelNorm.Copy (&pUp);
            bri.pEnd.Copy (&pInter);
            bri.pSize.Copy (&m_apPostSize[3]);

            pc->BraceInfoSet (dwBrace, &bri);

         }  // dwBrace
      }  // if bracing

   }  // i, m_lPosts.Num()

   // claim the colors 
   ClaimAllNecessaryColors ();

   // create the skirting
   UpdateSkirting (dwRenderShard, m_fHeight);

   return TRUE;
}

/***********************************************************************************
CPiers::UpdateSkirting - If the skirting isnt supposed to exist then it's
deleted. If not, it creates it with the desired length.

inputs
   fp      fHeight - Height for the skirting
returns
   none
*/
void CPiers::UpdateSkirting (DWORD dwRenderShard, fp fHeight)
{
   // if doesn't exist then this is easy
   if (!m_fShowSkirting) {
      if (m_pdsSkirting) {
         m_pdsSkirting->ClaimClear();
         delete m_pdsSkirting;
      }
      m_pdsSkirting = NULL;
      return;
   }

   // if not yet made then make
   if (!m_pdsSkirting) {
      m_pdsSkirting = new CDoubleSurface;
      if (!m_pdsSkirting)
         return;
      m_pdsSkirting->Init (dwRenderShard, 0x1000c /*skirting*/, m_pTemplate);
   }

   // if get here have skirting object, so make it
   DWORD dwNum;
   DWORD dwOrigSeg;
   if (m_psOrigTop) {
      dwOrigSeg = dwNum = m_psOrigTop->OrigNumPointsGet();
      if (m_psOrigTop->LoopedGet())
         dwNum++;
      else
         dwOrigSeg--;
   }
   else
      return;  // BUGFIX - Fail gracefully

   CMem memPoints, memCurve;
   PCPoint paTop, paBottom;
   DWORD *padwCurve;
   if (!memPoints.Required (dwNum * 2 * sizeof(CPoint)) || !memCurve.Required(dwNum*sizeof(DWORD))) {
      m_pdsSkirting->ClaimClear();
      delete m_pdsSkirting;
      m_pdsSkirting = NULL;
      return;
   }
   paTop = (PCPoint) memPoints.p;
   paBottom = paTop + dwNum;
   padwCurve = (DWORD*) memCurve.p;

   // get the points from the top and bottom
   DWORD i;
   for (i = 0; i < dwNum-1; i++) {
      // do this wierd thing because flipping backwards AND because will be
      // turning it into a non-looped wall
      DWORD dwVal;
      m_psOrigTop->OrigSegCurveGet (i % dwOrigSeg, &dwVal);
      
      switch (dwVal) {
      case SEGCURVE_CIRCLEPREV:
         dwVal = SEGCURVE_CIRCLENEXT;
         break;
      case SEGCURVE_CIRCLENEXT:
         dwVal = SEGCURVE_CIRCLEPREV;
         break;
      case SEGCURVE_ELLIPSEPREV:
         dwVal = SEGCURVE_ELLIPSENEXT;
         break;
      case SEGCURVE_ELLIPSENEXT:
         dwVal = SEGCURVE_ELLIPSEPREV;
         break;
      }
      padwCurve[dwNum-i-2] = dwVal;
   }
   for (i = 0; i < dwNum; i++) {
      // have to get in reversed order because piers go around clockwise looking from above
      m_psOrigTop->OrigPointGet (i % m_psOrigTop->OrigNumPointsGet(), &paTop[dwNum - i - 1]);
      m_psOrigBottom->OrigPointGet (i % m_psOrigBottom->OrigNumPointsGet(), &paBottom[dwNum - i - 1]);
   }

   // extend the bottom according to the specified length
   for (i = 0; i < dwNum; i++) {
      paBottom[i].Subtract (&paTop[i]);
      paBottom[i].Normalize();
      paBottom[i].Scale (fHeight);
      paBottom[i].Add (&paTop[i]);
   }

   // create the surface
   DWORD dwMin, dwMax;
   fp fDetail;
   m_psOrigTop->DivideGet (&dwMin, &dwMax, &fDetail);
   m_pdsSkirting->ControlPointsSet (dwNum, 2, paTop, padwCurve, (DWORD*)SEGCURVE_LINEAR, dwMax);

   // update cutouts
   UpdateSkirtingCutouts ();

   // done
}

/***********************************************************************************
CPiers::UpdateSkirtingCutouts - Call this whenever the cutouts list has changed
*/
void CPiers::UpdateSkirtingCutouts (void)
{
   if (!m_pdsSkirting)
      return;

   PCSplineSurface ps;
   ps = &m_pdsSkirting->m_SplineA;

   // clear existing cutouts
   DWORD i;
   PTEXTUREPOINT pt;
   for (i = ps->CutoutNum()-1; i < ps->CutoutNum(); i--) {
      PWSTR pszName;
      DWORD dwNum;
      BOOL fClockwise;
      pszName = NULL;
      ps->CutoutEnum (i, &pszName, &pt, &dwNum, &fClockwise);
      if (pszName && !wcsncmp (pszName, L"Opening", 7))
         ps->CutoutRemove (pszName);
   }

   // loop through the overlays and create cutouts
   for (i = 0; i < m_lCutouts.Num(); i++) {
      pt = (PTEXTUREPOINT) m_lCutouts.Get(i);

      WCHAR szTemp[64];
      swprintf (szTemp, L"Opening%d", (int) i);

      TEXTUREPOINT ap[4];
      ap[0].h = 1.0 - pt->v;
      ap[0].v = 0;
      ap[1].h = 1.0 - pt->h;
      ap[1].v = 0;
      ap[2] = ap[1];
      ap[2].v = 1;
      ap[3] = ap[0];
      ap[3].v = 1;

      ps->CutoutSet (szTemp, ap, 4, TRUE);
   };

}

/***********************************************************************************
CPiers::ExtendPostsToGround - Loops through all the posts and extends them to the ground

inputs
   none
returns
   BOOL - TRUE if success
*/
void CPiers::ExtendPostsToGround (DWORD dwRenderShard)
{
   fp fMaxHeight;
   fMaxHeight = .01; // maximum height found for skirting

   // matrix that's the inteverse of the location info
   CMatrix m, mInv;
   if (m_pTemplate)
      m_pTemplate->ObjectMatrixGet(&m);
   else
      m.Identity();
   m.Invert4(&mInv);

   // loop through all the posts that we care about
   DWORD dwPost;
   for (dwPost = 0; dwPost < m_lPosts.Num(); dwPost++) {
      PBPOSTINFO pbpi = (PBPOSTINFO) m_lPosts.Get(dwPost);
      if (!pbpi->pColumn)
         continue;

      // get the start and end
      CPoint pStart, pEnd;
      pbpi->pColumn->StartEndFrontGet (&pStart, &pEnd);

      // convert this to world coords
      CPoint pStartW, pEndW;
      pStartW.Copy (&pStart);
      pEndW.Copy (&pEnd);
      pStartW.p[3] = pEndW.p[3] = 1;
      pStartW.MultiplyLeft (&m);
      pEndW.MultiplyLeft (&m);

      // find out where intersects
      fp fLen, fBelow;
      fLen = m_fHeight; // default
      fBelow = 0.0;

      if (m_pTemplate && m_pTemplate->m_pWorld) {
         DWORD j;
         OSMINTERSECTLINE il;
         PCWorldSocket pWorld = m_pTemplate->m_pWorld;
         // find out where intersects with ground
         memset (&il, 0, sizeof(il));
         il.pStart.Copy (&pStartW);
         il.pEnd.Copy (&pEndW);
         for (j = 0; j < pWorld->ObjectNum(); j++) {
            PCObjectSocket pos = pWorld->ObjectGet(j);

            pos->Message (OSM_INTERSECTLINE, &il);

            if (il.fIntersect)
               break;
         }
         if (il.fIntersect) {
            // distance
            il.pIntersect.Subtract (&pEndW);
            fLen = il.pIntersect.Length();
            fBelow = m_fDepthBelow;
         }
      }

      // remember max height
      fMaxHeight = max(fLen, fMaxHeight);

      // new length
      pStart.Subtract (&pEnd);
      pStart.Normalize();
      pStart.Scale (max(fLen + fBelow, .1));
      pStart.Add (&pEnd);

      // write it
      pbpi->pColumn->StartEndFrontSet (&pStart, &pEnd);

      // extend the base below the gound
      CBASEINFO bi;
      pbpi->pColumn->BaseInfoGet (TRUE, &bi);
      bi.pSize.p[2] = m_apPostSize[0].p[2] + fBelow;
      pbpi->pColumn->BaseInfoSet (TRUE, &bi);

   } // dwPost

   // create the skirting
   UpdateSkirting (dwRenderShard, fMaxHeight);

}


/***********************************************************************************
CPiers::Constructor and destructor */
CPiers::CPiers (void)
{
   m_pTemplate = NULL;
   memset (m_adwBalColor, 0, sizeof(m_adwBalColor));
   m_psBottom = m_psTop = NULL;
   m_psOrigBottom = m_psOrigTop = NULL;
   m_pdsSkirting = NULL;
   m_lCutouts.Init (sizeof(fp)*2);
   m_lPosts.Init (sizeof(BPOSTINFO));
   m_fMaxPostDistanceBig = 2.0;
   m_fHeight = 1.0;
   m_fDepthBelow = 1.0;
   m_pssControlInter = NULL;
   m_dwDisplayControl = 0;
   m_fForceVert = m_fForceLevel = TRUE;
   m_fShowSkirting = TRUE;
   m_fIndent = FALSE;

   ParamFromStyle (PS_STEEL);
   
}

CPiers::~CPiers (void)
{
   //ClaimClear(); - BUGFIX - Don't do this here because confuses things when
   // deleting master object. Instead, have master object call if specially
   // destroying only this surface.

   if (m_pssControlInter)
      delete m_pssControlInter;
   m_pssControlInter = NULL;

   // May want to delete existing post objects
   DWORD i;
   PBPOSTINFO pbpi;
   for (i = 0; i < m_lPosts.Num(); i++) {
      pbpi = (PBPOSTINFO) m_lPosts.Get(i);
      if (pbpi->pColumn)
         delete pbpi->pColumn;
   }

   if (m_psBottom)
      delete m_psBottom;
   if (m_psTop)
      delete m_psTop;
   if (m_psOrigBottom)
      delete m_psOrigBottom;
   if (m_psOrigTop)
      delete m_psOrigTop;
   if (m_pdsSkirting)
      delete m_pdsSkirting;
}


/***********************************************************************************
CPiers::ParamFromStyle - Given a Piers style, this sets the parameters.

inputs
   DWORD       dwStyle - PS_XXX
returns
   none
*/
void CPiers::ParamFromStyle (DWORD dwStyle)
{
   m_dwBalStyle = dwStyle;
   if (dwStyle == PS_CUSTOM)
      return;  // nothing to do

   DWORD i;
   fp fSize;
   for (i = 0; i < 5; i++) {
      m_adwPostShape[i] = NS_RECTANGLE;
      //m_adwPostShape[i] = NS_CIRCLEFAST;

      switch (i) {
      case 0:  //bottom
         fSize = .4;
         break;
      case 1:  //top, if short
      case 2:  //top, if tall
         fSize = .2;
         break;
      case 3:  // brace
         fSize = .075;
         break;
      case 4:  // main
         fSize = .1;
         break;
      }
      m_apPostSize[i].Zero();
      m_apPostSize[i].p[0] = m_apPostSize[i].p[1] = m_apPostSize[i].p[2] = fSize;
      // m_apPostSize[i].p[0] *= 2;
      // dont need to set p[2] for main column and brace but do it anyway
   }
   for (i = 0; i < 4; i++)
      m_afPostUse[i] = FALSE; // TRUE
   m_afPostUse[0] = TRUE;  // bottom cement bit
   m_afPostUse[3] = TRUE;  // bracing
   for (i = 0; i < 3; i++)
      m_adwPostTaper[i] = CT_UPANDTAPER;
   m_fBraceBelow = 0.75; // brace 1m below roof


   switch (dwStyle) {
   case PS_LOGSTUMP:  // log stumps
      m_adwPostShape[0] = m_adwPostShape[3] = m_adwPostShape[4] = NS_CIRCLEFAST;
      m_apPostSize[0].p[0] = m_apPostSize[0].p[1] = .5; // base
      m_apPostSize[0].p[2] = .1;
      m_apPostSize[3].p[0] = m_apPostSize[3].p[1] = .1; // brace
      m_apPostSize[4].p[0] = m_apPostSize[4].p[1] = .2; // main
      m_adwPostTaper[0] = CT_NONE;
      break;

   case PS_STEEL:  // 75mm RHS steel
      m_apPostSize[3].p[0] = m_apPostSize[3].p[1] = .075; // brace
      m_apPostSize[4].p[0] = m_apPostSize[4].p[1] = .075; // main
      m_apPostSize[0].p[0] = m_apPostSize[0].p[1] = m_apPostSize[0].p[2] = .3; // base
      m_adwPostTaper[0] = CT_NONE;
      m_adwPostShape[0] = NS_CIRCLEFAST;
      break;

   case PS_WOOD:  // 100mm wood
      m_apPostSize[3].p[0] = m_apPostSize[3].p[1] = .1; // brace
      m_apPostSize[4].p[0] = m_apPostSize[4].p[1] = .1; // main
      m_apPostSize[0].p[0] = m_apPostSize[0].p[1] = .3; // base
      m_apPostSize[0].p[2] = .1;
      m_adwPostTaper[0] = CT_NONE;
      break;

   case PS_CEMENTSQUARE:  // 300mm cement
      m_afPostUse[0] = FALSE;  // bottom cement bit
      m_afPostUse[3] = FALSE;  // bracing
      m_apPostSize[4].p[0] = m_apPostSize[4].p[1] = .3; // main
      break;

   case PS_CEMENTROUND:  // 300mm round
      m_afPostUse[0] = FALSE;  // bottom cement bit
      m_afPostUse[3] = FALSE;  // bracing
      m_apPostSize[4].p[0] = m_apPostSize[4].p[1] = .3; // main
      m_adwPostShape[4] = NS_CIRCLEFAST;
      break;

   case PS_GREEK:  // greek column
      m_afPostUse[0] = TRUE;  // bottom cement bit
      m_afPostUse[3] = FALSE;  // bracing
      m_afPostUse[1] = m_afPostUse[2] = TRUE;
      m_apPostSize[4].p[0] = m_apPostSize[4].p[1] = .2; // main
      m_adwPostTaper[0] = CT_NONE;
      m_adwPostShape[4] = NS_CIRCLEFAST;
      m_apPostSize[0].p[0] = m_apPostSize[0].p[1] = .3;
      m_apPostSize[0].p[2] = .10;
      m_apPostSize[1].Copy (&m_apPostSize[0]);
      m_apPostSize[2].Copy (&m_apPostSize[0]);
      break;
   }
}



/*************************************************************************************
CPiers::InitButDontCreate - Initializes the object half way, but doesn't
actually create surfaces - which is ultimately causing problems in CObjectBuildBlock.
Basically, this just remembers the template and dwType and that's it.
*/
BOOL CPiers::InitButDontCreate (DWORD dwRenderShard, DWORD dwType, PCObjectTemplate pTemplate)
{
   m_dwType = dwType;
   m_pTemplate = pTemplate;

   // get information from default world style
   DWORD dwExternalWalls;
   PWSTR psz;
   PCWorldSocket pWorld;
   pWorld = WorldGet(dwRenderShard, NULL);
   psz = pWorld ? pWorld->VariableGet (WSExternalWalls()) : NULL;
   dwExternalWalls = 0;
   if (psz)
      dwExternalWalls = (DWORD) _wtoi(psz);
   else
      DefaultBuildingSettings (NULL, NULL, NULL, NULL, NULL, &dwExternalWalls);  // BUGFIX - extenral

   // derive style from this
   DWORD dwStyle;
   if (!dwType) switch (dwExternalWalls) {
   case 0:  // corrogated
      dwStyle = PS_STEEL;
      m_fShowSkirting = FALSE;   // dont hace skirting for steel. Have for the rest
      break;
   case 4:  // brick
   case 5:  // stone
   case 6:  // cement block
      dwStyle = PS_CEMENTSQUARE;
      break;
   case 8:  // log
      dwStyle = PS_LOGSTUMP;
      break;
   default:
      dwStyle = PS_WOOD;
      break;
   }
   else
      dwStyle = LOWORD(dwType);
   ParamFromStyle (dwStyle);

   return TRUE;
}


/***********************************************************************************
CPiers::Init - Initialize the ballustrade object.

NOTE: This assumes that the Piers information (such as height, width, and spacing)
has already been set. The width is particularly important since its the amount
"indented" by.

inputs
   DWORD       dwType - Type of surface.
   PCObjectTemplate     pTemplate - Used for information like getting surfaces available.
   PCSpline    pBottom - Spline circling the bottom (flat end) of the path that
               the ballustrade follows. This is cloned and the clone is kept
               internally.
   PCSpline    pTop - Spline following the top (not flat) end do the path of the
               ballustrade. This is ASSUMED to have the same number of nodes and
               segment curves as pBottom. It better have. This is cloned and the
               clone is kept internally.
   BOOL        fIndent - If TRUE then the balustrading is indented by width/depth of columns (by
               calling CSpline::Expand) so that the Piers doesn't go over
               the floor.
returns
   BOOL - TRUE if successful
*/
BOOL CPiers::Init (DWORD dwRenderShard, DWORD dwType, PCObjectTemplate pTemplate, PCSpline pBottom, PCSpline pTop, BOOL fIndent)
{
   InitButDontCreate (dwRenderShard, dwType, pTemplate);

   m_fIndent = fIndent;

   return NewSplines (dwRenderShard, pBottom, pTop);
}

/***********************************************************************************
CPiers::NewSplines - Change it to use new splines

NOTE: This assumes that the Piers information (such as height, width, and spacing)
has already been set. The width is particularly important since its the amount
"indented" by.

inputs
   PCSpline    pBottom - Spline circling the bottom (flat end) of the path that
               the ballustrade follows. This is cloned and the clone is kept
               internally.
   PCSpline    pTop - Spline following the top (not flat) end do the path of the
               ballustrade. This is ASSUMED to have the same number of nodes and
               segment curves as pBottom. It better have. This is cloned and the
               clone is kept internally.
returns
   BOOL - TRUE if successful
*/
BOOL CPiers::NewSplines (DWORD dwRenderShard, PCSpline pBottom, PCSpline pTop)
{
   // clear the intesection spline surface
   if (m_pssControlInter)
      delete m_pssControlInter;
   m_pssControlInter = NULL;

   if (m_psBottom)
      delete m_psBottom;
   if (m_psTop)
      delete m_psTop;
   if (m_psOrigBottom)
      delete m_psOrigBottom;
   if (m_psOrigTop)
      delete m_psOrigTop;

   m_psOrigBottom = new CSpline;
   m_psOrigTop = new CSpline;
   if (m_psOrigBottom)
      pBottom->CloneTo (m_psOrigBottom);
   if (m_psOrigTop)
      pTop->CloneTo (m_psOrigTop);

   // figure out how much to indent. Take into account the post width
   fp fWidth;
   fWidth = 0;
   if (m_fIndent) {
      // only care about the 4th post - the main column, since that must be
      // right under the load bearing
      fWidth = max(fWidth, m_apPostSize[4].p[0]);
      fWidth = max(fWidth, m_apPostSize[4].p[1]);
   }

   // clone and store away new splines, but they should be internal by fWidth/2
   m_psBottom = pBottom->Expand (-fWidth / 2);
   m_psTop = pTop->Expand (-fWidth/2);
   if (!m_psBottom || !m_psTop || !m_psOrigBottom || !m_psOrigTop)
      return FALSE;

   // figure out where posts go
   DeterminePostLocations(dwRenderShard);

   return TRUE;
}


static PWSTR gpszBottom = L"Bottom";
static PWSTR gpszTop = L"Top";
static PWSTR gpszOrigBottom = L"OrigBottom";
static PWSTR gpszOrigTop = L"OrigTop";
static PWSTR gpszHeight = L"Height";
static PWSTR gpszWidth = L"Width";
static PWSTR gpszSpacing = L"Spacing";
static PWSTR gpszCutouts = L"Cutouts%d";
static PWSTR gpszPosts = L"Posts";
static PWSTR gpszLoc = L"Loc";
static PWSTR gpszRight = L"Right";
static PWSTR gpszLeft = L"Left";
static PWSTR gpszReason = L"Reason";
static PWSTR gpszWantFullHeight = L"WantFullHeight";
static PWSTR gpszColumn = L"Column";
static PWSTR gpszMaxPostDistanceBig = L"MaxPostDistanceBig";
static PWSTR gpszMaxPostDistanceSmall = L"MaxPostDistanceSmall";
static PWSTR gpszPostDivideIntoFull = L"PostDivideIntoFull";
static PWSTR gpszBraceBelow = L"BraceBelow";
static PWSTR gpszPostWantFullHeightCutout = L"WantFullHtCut";
static PWSTR gpszPostHeightAbovePiers = L"HgtAboveBal";
static PWSTR gpszHorzRailAuto = L"HorzRailAuto";
static PWSTR gpszVertUse = L"VertUse";
static PWSTR gpszVertUsePoint = L"VertUsePoint";
static PWSTR gpszVertPointTaper = L"VertPointTaper";
static PWSTR gpszVertOffset = L"VertOffset";
static PWSTR gpszVertAuto = L"VertAuto";
static PWSTR gpszPanelUse = L"PanelUse";
static PWSTR gpszPanelInfo = L"PanelInfo";
static PWSTR gpszPanelOffset = L"PanelOffset";
static PWSTR gpszPanelThickness = L"PanelThickness";
static PWSTR gpszBrace = L"Brace";
static PWSTR gpszBraceSize = L"BraceSize";
static PWSTR gpszBraceTB = L"BraceTB";
static PWSTR gpszBraceShape = L"BraceShape";
static PWSTR gpszBraceOffset = L"BraceOffset";
static PWSTR gpszBalStyle = L"BalStyle";
static PWSTR gpszType = L"Type";
static PWSTR gpszDisplayControl = L"DisplayControl";
static PWSTR gpszIndent = L"Indent";
static PWSTR gpszForceVert = L"ForceVert";
static PWSTR gpszForceLevel = L"ForceLevel";
static PWSTR gpszHideFirst = L"HideFirst";
static PWSTR gpszHideLast = L"HideLast";
static PWSTR gpszDepthBelow = L"DepthBelow";
static PWSTR gpszShowSkirting = L"ShowSkirting";
static PWSTR gpszSkirting = L"Skirting";

/***********************************************************************************
CPiers::MMLTo - Creates a MML node (that must be freed by the caller) with
information describing the spline.

inputs
   none
returns
   PCMMLNode2 - node
*/
PCMMLNode2 CPiers::MMLTo (void)
{
   PCMMLNode2 pNode;
   pNode = new CMMLNode2;
   if (!pNode)
      return NULL;

   // two splines
   PCMMLNode2 pSub;
   if (m_psBottom && (pSub = m_psBottom->MMLTo())) {
      pSub->NameSet (gpszBottom);
      pNode->ContentAdd (pSub);
   }
   if (m_psTop && (pSub = m_psTop->MMLTo())) {
      pSub->NameSet (gpszTop);
      pNode->ContentAdd (pSub);
   }
   if (m_psOrigBottom && (pSub = m_psOrigBottom->MMLTo())) {
      pSub->NameSet (gpszOrigBottom);
      pNode->ContentAdd (pSub);
   }
   if (m_psOrigTop && (pSub = m_psOrigTop->MMLTo())) {
      pSub->NameSet (gpszOrigTop);
      pNode->ContentAdd (pSub);
   }
   if (m_pdsSkirting && (pSub = m_pdsSkirting->MMLTo())) {
      pSub->NameSet (gpszSkirting);
      pNode->ContentAdd (pSub);
   }

   // values
   MMLValueSet (pNode, gpszDisplayControl, (int) m_dwDisplayControl);
   MMLValueSet (pNode, gpszIndent, (int)m_fIndent);
   MMLValueSet (pNode, gpszHeight, m_fHeight);
   MMLValueSet (pNode, gpszDepthBelow, m_fDepthBelow);
   MMLValueSet (pNode, gpszForceVert, (int)m_fForceVert);
   MMLValueSet (pNode, gpszForceLevel, (int)m_fForceLevel);
   MMLValueSet (pNode, gpszShowSkirting, (int)m_fShowSkirting);

   DWORD i;
   for (i = 0; i < m_lCutouts.Num(); i++) {
      WCHAR szTemp[64];
      swprintf (szTemp, gpszCutouts, i);
      MMLValueSet (pNode, szTemp, (PTEXTUREPOINT) m_lCutouts.Get(i));
   }

   for (i = 0; i < m_lPosts.Num(); i++) {
      PBPOSTINFO pbi = (PBPOSTINFO) m_lPosts.Get(i);
      PCMMLNode2 pSub;
      pSub = new CMMLNode2;
      if (!pSub) {
         delete pNode;
         return NULL;
      }
      pNode->ContentAdd (pSub);

      pSub->NameSet (gpszPosts);
      MMLValueSet (pSub, gpszLoc, pbi->fLoc);
      MMLValueSet (pSub, gpszRight, (int) pbi->fRight);
      MMLValueSet (pSub, gpszLeft, (int) pbi->fLeft);
      MMLValueSet (pSub, gpszReason, (int) pbi->dwReason);

      PCMMLNode2 pCol;
      if (pbi->pColumn) {
         pCol = pbi->pColumn->MMLTo ();
         if (pCol) {
            pCol->NameSet (gpszColumn);
            pSub->ContentAdd (pCol);
         }
      }
   }  // i < m_lPosts.Num()

   MMLValueSet (pNode, gpszMaxPostDistanceBig, m_fMaxPostDistanceBig);
   MMLValueSet (pNode, gpszBraceBelow, (int) m_fBraceBelow);


   WCHAR szTemp[64];
   for (i = 0; i < 5; i++) {
      swprintf (szTemp, L"PostShape%d", (int) i);
      MMLValueSet (pNode, szTemp, (int)m_adwPostShape[i]);

      swprintf (szTemp, L"PostSize%d", (int) i);
      MMLValueSet (pNode, szTemp, &m_apPostSize[i]);
   }
   for (i = 0; i < 3; i++) {
      swprintf (szTemp, L"PostTaper%d", (int) i);
      MMLValueSet (pNode, szTemp, (int)m_adwPostTaper[i]);
   }
   for (i = 0; i < 4; i++) {
      swprintf (szTemp, L"PostUse%d", (int) i);
      MMLValueSet (pNode, szTemp, (int)m_afPostUse[i]);
   }

   MMLValueSet (pNode, gpszBalStyle, (int) m_dwBalStyle);
   MMLValueSet (pNode, gpszType, (int)m_dwType);

   for (i = 0; i < PIERCOLOR_MAX; i++) {
      swprintf (szTemp, L"BalColor%d", i);
      MMLValueSet (pNode, szTemp, (int) m_adwBalColor[i]);
   }

   return pNode;

}

/***********************************************************************************
CPiers::MMLFrom - Reads in MML and fills in variables base don that.

inputs
   PCMMLNode2      pNode - Node to read from
returns
   BOOL - TRUE if successful
*/
BOOL CPiers::MMLFrom (PCMMLNode2 pNode)
{
   // clear the intesection spline surface
   if (m_pssControlInter)
      delete m_pssControlInter;
   m_pssControlInter = NULL;

   if (m_psBottom)
      delete m_psBottom;
   if (m_psTop)
      delete m_psTop;
   if (m_psOrigBottom)
      delete m_psOrigBottom;
   if (m_psOrigTop)
      delete m_psOrigTop;
   if (m_pdsSkirting) {
      m_pdsSkirting->ClaimClear();
      delete m_pdsSkirting;
   }
   m_psBottom = NULL;
   m_psTop = NULL;
   m_psOrigBottom = NULL;
   m_psOrigTop = NULL;
   m_pdsSkirting = NULL;

   // clear out data structures
   DWORD i;
   PBPOSTINFO pbpi;
   for (i = 0; i < m_lPosts.Num(); i++) {
      pbpi = (PBPOSTINFO) m_lPosts.Get(i);
      if (pbpi->pColumn)
         delete pbpi->pColumn;
   }
   m_lPosts.Clear();

   PCMMLNode2 pSub;
   PWSTR psz;
   for (i = 0; i < pNode->ContentNum(); i++) {
      pSub = NULL;
      pNode->ContentEnum (i, &psz, &pSub);
      if (!pSub)
         continue;
      psz = pSub->NameGet();
      if (!psz)
         continue;
      if (!_wcsicmp(psz, gpszBottom)) {
         m_psBottom = new CSpline;
         if (!m_psBottom)
            return FALSE;
         m_psBottom->MMLFrom (pSub);
      }
      else if (!_wcsicmp(psz, gpszTop)) {
         m_psTop = new CSpline;
         if (!m_psTop)
            return FALSE;
         m_psTop->MMLFrom (pSub);
      }
      else if (!_wcsicmp(psz, gpszOrigBottom)) {
         m_psOrigBottom = new CSpline;
         if (!m_psOrigBottom)
            return FALSE;
         m_psOrigBottom->MMLFrom (pSub);
      }
      else if (!_wcsicmp(psz, gpszOrigTop)) {
         m_psOrigTop = new CSpline;
         if (!m_psOrigTop)
            return FALSE;
         m_psOrigTop->MMLFrom (pSub);
      }
      else if (!_wcsicmp(psz, gpszSkirting)) {
         m_pdsSkirting = new CDoubleSurface;
         if (!m_pdsSkirting)
            return FALSE;
         m_pdsSkirting->InitButDontCreate (0x1000c /*skrting*/, m_pTemplate);
         m_pdsSkirting->MMLFrom (pSub);
      }
      else if (!_wcsicmp(psz, gpszPosts)) {
         BPOSTINFO pi;
         memset (&pi, 0, sizeof(pi));

         pi.fLoc = MMLValueGetDouble (pSub, gpszLoc, 0);
         pi.fRight = (BOOL) MMLValueGetInt (pSub, gpszRight, FALSE);
         pi.fLeft = (BOOL) MMLValueGetInt (pSub, gpszLeft, FALSE);
         pi.dwReason = (DWORD) MMLValueGetInt (pSub, gpszReason, 0);

         // find the column
         DWORD j;
         PCMMLNode2 pSub2;
         for (j = 0; j < pSub->ContentNum(); j++) {
            pSub2 = NULL;
            pSub->ContentEnum (j, &psz, &pSub2);
            if (!pSub2)
               continue;
            psz = pSub2->NameGet();
            if (!psz)
               continue;
            if (!_wcsicmp(psz, gpszColumn)) {
               pi.pColumn = new CColumn;
               if (!pi.pColumn)
                  return FALSE;
               if (!pi.pColumn->MMLFrom (pSub2)) {
                  delete pi.pColumn;
                  return FALSE;
               }
               break;
            }

         }  // look for column
         // add it
         m_lPosts.Add (&pi);
      }  // if post
   } // look over values

   // values
   m_fHeight = MMLValueGetDouble (pNode, gpszHeight, 1);
   m_fDepthBelow = MMLValueGetDouble (pNode, gpszDepthBelow, 1.0);
   m_dwDisplayControl = (DWORD) MMLValueGetInt (pNode, gpszDisplayControl, 0);
   m_fIndent = (BOOL) MMLValueGetInt (pNode, gpszIndent, 0);
   m_fForceVert = (BOOL) MMLValueGetInt (pNode, gpszForceVert, TRUE);
   m_fForceLevel = (BOOL) MMLValueGetInt (pNode, gpszForceLevel, TRUE);
   m_fShowSkirting = (BOOL) MMLValueGetInt (pNode, gpszShowSkirting, TRUE);

   m_lCutouts.Clear();
   for (i = 0; ; i++) {
      WCHAR szTemp[64];
      swprintf (szTemp, gpszCutouts, i);
      TEXTUREPOINT tp;
      tp.h = tp.v = 0;
      if (!MMLValueGetTEXTUREPOINT (pNode, szTemp, &tp, &tp))
         break;
      m_lCutouts.Add (&tp);
   }

   m_fMaxPostDistanceBig = MMLValueGetDouble (pNode, gpszMaxPostDistanceBig, 1);
   m_fBraceBelow = (BOOL) MMLValueGetInt (pNode, gpszBraceBelow, FALSE);


   WCHAR szTemp[64];
   for (i = 0; i < 5; i++) {
      swprintf (szTemp, L"PostShape%d", (int) i);
      m_adwPostShape[i] = (DWORD) MMLValueGetInt (pNode, szTemp, NS_RECTANGLE);

      swprintf (szTemp, L"PostSize%d", (int) i);
      MMLValueGetPoint (pNode, szTemp, &m_apPostSize[i]);
   }
   for (i = 0; i < 3; i++) {
      swprintf (szTemp, L"PostTaper%d", (int) i);
      m_adwPostTaper[i] = (DWORD) MMLValueGetInt (pNode, szTemp, 0);
   }
   for (i = 0; i < 4; i++) {
      swprintf (szTemp, L"PostUse%d", (int) i);
      m_afPostUse[i] = (BOOL) MMLValueGetInt (pNode, szTemp, FALSE);
   }

   m_dwBalStyle = (DWORD) MMLValueGetInt (pNode, gpszBalStyle, 0);
   m_dwType = (DWORD) MMLValueGetInt (pNode, gpszType, 0);

   for (i = 0; i < PIERCOLOR_MAX; i++) {
      swprintf (szTemp, L"BalColor%d", i);
      m_adwBalColor[i] = (DWORD) MMLValueGetInt (pNode, szTemp, 0);
   }

   return TRUE;
}


/*************************************************************************************
CPiers::CloneTo - Clones the current Piers object and returns a cloned version.


NOTE: This does not actually clone the claimed colors, etc.



inputs
   CPiers *pNew
   PCObjecTemplate      pTemplate - New template.
returns
   PCPiers - new object. Must be freed by the calelr.
*/
BOOL CPiers::CloneTo (CPiers *pNew, PCObjectTemplate pTemplate)
{
   // clear the intesection spline surface
   if (pNew->m_pssControlInter)
      delete pNew->m_pssControlInter;
   pNew->m_pssControlInter = NULL;

   if (pNew->m_psBottom)
      delete pNew->m_psBottom;
   if (pNew->m_psTop)
      delete pNew->m_psTop;
   if (pNew->m_psOrigBottom)
      delete pNew->m_psOrigBottom;
   if (pNew->m_psOrigTop)
      delete pNew->m_psOrigTop;
   if (pNew->m_pdsSkirting)
      delete pNew->m_pdsSkirting;
   pNew->m_psBottom = NULL;
   pNew->m_psTop = NULL;
   pNew->m_psOrigBottom = NULL;
   pNew->m_psOrigTop = NULL;
   pNew->m_pdsSkirting = NULL;

   // clear out data structures
   DWORD i;
   PBPOSTINFO pbpi;
   for (i = 0; i < pNew->m_lPosts.Num(); i++) {
      pbpi = (PBPOSTINFO) pNew->m_lPosts.Get(i);
      if (pbpi->pColumn)
         delete pbpi->pColumn;
   }
   pNew->m_lPosts.Clear();

   // create new ones
   if (m_psBottom) {
      pNew->m_psBottom = new CSpline;
      if (!pNew->m_psBottom) {
         delete pNew;
         return NULL;
      }
      m_psBottom->CloneTo (pNew->m_psBottom);
   }
   if (m_psTop) {
      pNew->m_psTop = new CSpline;
      if (!pNew->m_psTop) {
         delete pNew;
         return NULL;
      }
      m_psTop->CloneTo (pNew->m_psTop);
   }
   if (m_psOrigBottom) {
      pNew->m_psOrigBottom = new CSpline;
      if (!pNew->m_psOrigBottom) {
         delete pNew;
         return NULL;
      }
      m_psOrigBottom->CloneTo (pNew->m_psOrigBottom);
   }
   if (m_pdsSkirting) {
      pNew->m_pdsSkirting = new CDoubleSurface;
      if (!pNew->m_pdsSkirting) {
         delete pNew;
         return NULL;
      }
      pNew->m_pdsSkirting->InitButDontCreate (0x1000c /* skirting*/, pTemplate);
      m_pdsSkirting->CloneTo (pNew->m_pdsSkirting, pTemplate);
   }
   if (m_psOrigTop) {
      pNew->m_psOrigTop = new CSpline;
      if (!pNew->m_psOrigTop) {
         delete pNew;
         return NULL;
      }
      m_psOrigTop->CloneTo (pNew->m_psOrigTop);
   }

   pNew->m_dwDisplayControl = m_dwDisplayControl;
   pNew->m_fForceVert = m_fForceVert;
   pNew->m_fShowSkirting = m_fShowSkirting;
   pNew->m_fForceLevel = m_fForceLevel;
   pNew->m_fHeight = m_fHeight;
   pNew->m_fDepthBelow = m_fDepthBelow;
   pNew->m_fMaxPostDistanceBig = m_fMaxPostDistanceBig;
   pNew->m_fBraceBelow = m_fBraceBelow;
   memcpy (pNew->m_adwPostShape, m_adwPostShape, sizeof(m_adwPostShape));
   memcpy (pNew->m_adwPostTaper, m_adwPostTaper, sizeof(m_adwPostTaper));
   memcpy (pNew->m_apPostSize, m_apPostSize, sizeof(m_apPostSize));
   memcpy (pNew->m_afPostUse, m_afPostUse, sizeof(m_afPostUse));

   pNew->m_lCutouts.Init (sizeof(fp)*2, m_lCutouts.Get(0), m_lCutouts.Num());

   // Will need to close post info
   // asume start out with no posts
   pNew->m_lPosts.Init (sizeof(BPOSTINFO), m_lPosts.Get(0), m_lPosts.Num());
   for (i = 0; i < pNew->m_lPosts.Num(); i++) {
      PBPOSTINFO pbi = (PBPOSTINFO) pNew->m_lPosts.Get (i);
      if (pbi->pColumn)
         pbi->pColumn = pbi->pColumn->Clone();
   }

   // clone rail information
   pNew->m_dwBalStyle = m_dwBalStyle;
   pNew->m_dwType = m_dwType;

   pNew->m_pTemplate = pTemplate;
   memcpy (pNew->m_adwBalColor, m_adwBalColor, sizeof(pNew->m_adwBalColor));

   return TRUE;
}



/*************************************************************************************
CPiers::Clone - Clones the current Piers object and returns a cloned version.


NOTE: This does not actually clone the claimed colors, etc.

inputs
   none
returns
   PCPiers - new object. Must be freed by the calelr.
*/
CPiers *CPiers::Clone (void)
{
   PCPiers pNew = new CPiers;
   if (!pNew)
      return NULL;

   if (!CloneTo (pNew, m_pTemplate)) {
      delete pNew;
      return NULL;
   }
   return pNew;
}


/********************************************************************************
CPiers::Render - Causes the Piers to draw.

NOTE: Right now just draws the ballistrades in a fixed color
inputs
   POBJECTRENDER        pr - Rendering information
   PCRenderSurface      *prs - Render to.
returns
   none
*/
void CPiers::Render (DWORD dwRenderShard, POBJECTRENDER pr, CRenderSurface *prs)
{
   // may not want to draw this
   if (!(pr->dwShow & RENDERSHOW_PIERS))
      return;

   // show the skirting
   if (m_pdsSkirting)
      m_pdsSkirting->Render (dwRenderShard, pr, prs);

   // loop through all the colors
   DWORD dwColor;
   for (dwColor = 0; dwColor < PIERCOLOR_MAX; dwColor++) {
      BOOL fPaintPost, fPaintNoodle;
      DWORD dwColumnFlag;
      WORD wNoodleColor;
      fPaintPost = fPaintNoodle = FALSE;
      dwColumnFlag = 0;
      wNoodleColor = 0;

      if (!m_adwBalColor[dwColor])
         continue;   // no color for this

      if (m_pTemplate)
         prs->SetDefMaterial (dwRenderShard, m_pTemplate->ObjectSurfaceFind (m_adwBalColor[dwColor]), m_pTemplate->m_pWorld);


      switch (dwColor) {
      case PIERCOLOR_POSTMAIN:
         fPaintPost = TRUE;
         dwColumnFlag = 0x01;
         break;
      case PIERCOLOR_POSTBASE:
         fPaintPost = TRUE;
         dwColumnFlag = 0x02;
         break;
      case PIERCOLOR_POSTTOP:
         fPaintPost = TRUE;
         dwColumnFlag = 0x04;
         break;
      case PIERCOLOR_POSTBRACE:
         fPaintPost = TRUE;
         // BUGFIX - Was 0x01
         dwColumnFlag = 0x08;
         break;
      }

      // draw all the columns
      DWORD i;
      if (fPaintPost) {
         PBPOSTINFO pbpi;
         for (i = 0; i < m_lPosts.Num(); i++) {
            pbpi = (PBPOSTINFO) m_lPosts.Get(i);
            if (!pbpi->pColumn)
               break;

            pbpi->pColumn->Render (pr, prs, dwColumnFlag);
         } // posts
      }

   }  // dwColors

}



/**********************************************************************************
CPiers::QueryBoundingBox - Standard API
*/
void CPiers::QueryBoundingBox (PCPoint pCorner1, PCPoint pCorner2)
{
   CPoint p1, p2;
   BOOL fSet = FALSE;

   // show the skirting
   if (m_pdsSkirting) {
      m_pdsSkirting->QueryBoundingBox (&p1, &p2);

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

   // loop through all the colors
   DWORD i;
   PBPOSTINFO pbpi;
   for (i = 0; i < m_lPosts.Num(); i++) {
      pbpi = (PBPOSTINFO) m_lPosts.Get(i);
      if (!pbpi->pColumn)
         break;

      pbpi->pColumn->QueryBoundingBox (&p1, &p2);

      if (fSet) {
         pCorner1->Min (&p1);
         pCorner2->Max (&p2);
      }
      else {
         pCorner1->Copy (&p1);
         pCorner2->Copy (&p2);
         fSet = TRUE;
      }
   } // posts
}



/********************************************************************************
CPiers::CutoutBasedOnSurface - THis takes a surface (pss) and matrix that translates
from the surface into ballistrade space (m), and sees what parts of the spline's bottom
are within the non-clipped/cutout area of the surface. This is used to only draw
ballistades and supports in place where the surface exists.

NOTE: The surface must be rectangular and flat for this to work

inputs
   PCSplienSurface      pss - Surface
   PCMatrix             m - Translates from surface coordinates into Piers coordinates
returns
   BOOL - TRUE if success
*/
BOOL CPiers::CutoutBasedOnSurface (DWORD dwRenderShard, PCSplineSurface pss, PCMatrix m)
{
   // Should clear out current cutouts
   m_lCutouts.Clear();

   // fail if no bottom spline
   if (!m_psBottom || !pss || !m)
      return FALSE;

   // NOTE: Assuming pSurf is 2x2 and linear
   if ((pss->ControlNumGet (TRUE) != 2) || (pss->ControlNumGet(FALSE) != 2))
      return FALSE;
   if ((pss->SegCurveGet (TRUE, 0) != SEGCURVE_LINEAR) || (pss->SegCurveGet (FALSE, 0) != SEGCURVE_LINEAR))
      return FALSE;
   CPoint pMin, pMax, pCur;
   DWORD i;
   pMax.Zero();   // BUGFIX - So doesnt complain about runtime
   pMin.Zero();
   for (i = 0; i < 4; i++) {
      pss->ControlPointGet (i % 2, i / 2, &pCur);
      if (i) {
         pMin.Min (&pCur);
         pMax.Max (&pCur);
      }
      else {
         pMin.Copy (&pCur);
         pMax.Copy (&pCur);
      }
   }
   if (fabs(pMax.p[1] - pMin.p[1]) > EPSILON)
      return FALSE;
   if ((fabs(pMax.p[0] - pMin.p[0]) < EPSILON) ||(fabs(pMax.p[2] - pMin.p[2]) < EPSILON))
      return FALSE;

   // need to get min and max again, defining min and UL corner, and max as LR corner
   pss->ControlPointGet (0, 0, &pMin);
   pss->ControlPointGet (1, 1, &pMax);

   // get the points of the spline
   CMem  memPoints;
   DWORD dwNew;
   dwNew = m_psTop->QueryNodes();
   if (!memPoints.Required ((dwNew+1) * sizeof(CPoint)))
      return TRUE;

   // load it in
   PCPoint paPoints;
   paPoints = (PCPoint) memPoints.p;
   for (i = 0; i < dwNew; i++) {
      paPoints[i].Copy (m_psTop->LocationGet (i));
      paPoints[i].p[3] = 1;
   }
   BOOL fLooped;
   fLooped = m_psTop->QueryLooped ();


   // translate to object space
   CMatrix mInv;
   m->Invert4 (&mInv);

   for (i = 0; i < dwNew; i++)
      paPoints[i].MultiplyLeft (&mInv);

   // convert to HV
   fp fDeltaH, fDeltaV;
   fDeltaH = pMax.p[0] - pMin.p[0];
   fDeltaV = pMax.p[2] - pMin.p[2];
   for (i = 0; i < dwNew; i++) {
      paPoints[i].p[0] = (paPoints[i].p[0] - pMin.p[0]) / fDeltaH;
      paPoints[i].p[1] = ((paPoints[i].p[2] - pMin.p[2]) / fDeltaV);
      paPoints[i].p[2] = 0;

      paPoints[i].p[0] = min(1, paPoints[i].p[0]);
      paPoints[i].p[0] = max(0, paPoints[i].p[0]);
      paPoints[i].p[1] = min(1, paPoints[i].p[1]);
      paPoints[i].p[1] = max(0, paPoints[i].p[1]);

   }

   // loop through all the points looking for a failure to intersect
   DWORD dwMax;
   dwMax = fLooped ? dwNew : (dwNew-1);
   DWORD dwCur;   // current line looking at
   TEXTUREPOINT tpS, tpE;
   fp afCutout[2];
   BOOL  fInCutout;
   fInCutout = FALSE;
   fp fScale;
   fp fOffset;
   fScale = 1.0 / dwMax;
   for (dwCur = 0; dwCur < dwMax; dwCur++) {
      tpS.h = paPoints[dwCur].p[0];
      tpS.v = paPoints[dwCur].p[1];
      tpE.h = paPoints[(dwCur+1)%dwNew].p[0];
      tpE.v = paPoints[(dwCur+1)%dwNew].p[1];
      fOffset = (fp) dwCur / (fp) dwMax;

      // within this, see how far can get
      TEXTUREPOINT tpCS, tpCE;
      tpCS = tpS;
      tpCE = tpE;
      while (TRUE) {
         // stop if the start is close to the end
         if (AreClose (&tpCS, &tpCE))
            break;

         // see how far can get
         TEXTUREPOINT tpNS, tpNE;
         BOOL fRet;
         fRet = pss->FindLongestSegment (&tpCS, &tpCE, &tpNS, &tpNE, FALSE);
         if (!fRet) {
            // if we are already in a cutout then no big deal, just go to the 
            // next segment
            if (fInCutout)
               break;

            // else, we're not in a cutout, so start one
            afCutout[0] = PointOn2DLine (&tpCS, &tpS, &tpE) * fScale + fOffset;
            fInCutout = TRUE;
            break;
         }

         // else if fRet

         if (fInCutout) {
            // cutout ends at tpNS
            afCutout[1] = PointOn2DLine (&tpNS, &tpS, &tpE) * fScale + fOffset;
            m_lCutouts.Add (afCutout);

            tpCS = tpNE;
            fInCutout = FALSE;
         }
         else if (!AreClose (&tpCS, &tpNS)) {
            // surprise surprise, we just skipped some
            afCutout[0] = PointOn2DLine (&tpCS, &tpS, &tpE) * fScale + fOffset;
            afCutout[1] = PointOn2DLine (&tpNS, &tpS, &tpE) * fScale + fOffset;
            m_lCutouts.Add (afCutout);

            // still not in coutous since found some line segment
            // BUGFIX - Took out continue because was causing hang when
            // cloned a building (using ctontrol drag), moved it so it intersected only a bit in the
            // LR and then intellisensed new copy.
            //continue;
         }
         
         // tpNS to tpNE is not in a cutout, see if a cutout starts
         if (!AreClose(&tpNE, &tpCE)) {
            afCutout[0] = PointOn2DLine (&tpNE, &tpS, &tpE) * fScale + fOffset;
            fInCutout = TRUE;
         }

         // go to the end
         tpCS = tpNE;
      }
   }

   if (fInCutout) {
      afCutout[1] = 1;
      m_lCutouts.Add (afCutout);
   }


   // figure out where posts go
   DeterminePostLocations(dwRenderShard);

   return TRUE;
}


/*************************************************************************************
CPiers::ClaimSurface - Loops through all the texture surfaces and finds one
that can claim as own. If so, claims it.

inputs
   DWORD       dwColor - One of PIERCOLOR_XXX
   COLORREF    cColor - Color to use if no texture is available.
   PWSTR       pszScheme - Scheme to use. If NULL then no scheme will be used.
   GUID        pgTextureCode - Texture code to use. If this is not NULL then its
                        assumed the surface will use a texture in preference to a color
   GUID        pgTextureSub - Sub-type of the texture in gpTextureCode.
   PTEXTUREMODS pTextureMods - Modifications to the basic texture. Can be NULL
   fp      *padTextureMatrix - 2x2 array of doubles for texture rotation matrix. Can be null
returns
   DWORD ID - 0 if error
*/
DWORD CPiers::ClaimSurface (DWORD dwColor, COLORREF cColor, DWORD dwMaterialID, PWSTR pszScheme,
      const GUID *pgTextureCode, const GUID *pgTextureSub,
      PTEXTUREMODS pTextureMods, fp *pafTextureMatrix)
{
   if (m_adwBalColor[dwColor])
      return m_adwBalColor[dwColor];

   // loop through surfaces until find one
   DWORD i;
   for (i = 10; i < 512; i++)
      if (-1 == m_pTemplate->ObjectSurfaceFindIndex (i))
         break;
   if (i >= 512)
      return 0;

   // that's the one we want
   m_pTemplate->ObjectSurfaceAdd (i, cColor, dwMaterialID, pszScheme, pgTextureCode, pgTextureSub,
      pTextureMods, pafTextureMatrix);

   // remember this
   m_adwBalColor[dwColor] = i;

   return i;
}


/*************************************************************************************
CPiers::ClaimClear - Clear all the claimed surfaces. This ends up
gettind rid of colors information and embedding information.

  
NOTE: An object that has a CPiers should call this if it's deleting
the furace but NOT itself. THis eliminates the claims for the surfaces

*/
void CPiers::ClaimClear (void)
{
   DWORD i;

   // free up all the current claim surfaces
   if (m_pTemplate) {
      for (i = 0; i < PIERCOLOR_MAX; i++) {
         if (m_adwBalColor[i])
            m_pTemplate->ObjectSurfaceRemove (m_adwBalColor[i]);
      }
   }
   memset (m_adwBalColor, 0, sizeof(m_adwBalColor));
}


/*************************************************************************************
CPiers::ClaimCloneTo - Looks at all the claims used in this object. These
are copied into the new fp surface - keeping the same IDs. Which means that can
onlyu call ClaimCloneTo when cloning to a blank destination object.


inputs
   PCPiers      pCloneTo - cloning to this
   PCObjectTemplate     pTemplate - Template. THis is what really is changed.
                        New ObjectSurfaceAdd() and ContainerSurfaceAdd()
returns
   none
*/
void CPiers::ClaimCloneTo (CPiers *pCloneTo, PCObjectTemplate pTemplate)
{
   if (!m_pTemplate)
      return;

   DWORD i;
   for (i = 0; i < PIERCOLOR_MAX; i++) {
      if (!m_adwBalColor[i])
         continue;

      pTemplate->ObjectSurfaceAdd (m_pTemplate->ObjectSurfaceFind(m_adwBalColor[i]));
   }

   if (m_pdsSkirting)
      m_pdsSkirting->ClaimCloneTo (pCloneTo->m_pdsSkirting, pTemplate);
}

/*************************************************************************************
CPiers::ClaimRemove - Remove a claim on a particular ID.

inputs
   DWORD       dwID - to remove the claim on. From m_pTemplate->ObjectSurfaceAdd
returns
   BOOL - TRUE if success
*/
BOOL CPiers::ClaimRemove (DWORD dwID)
{
   if (!dwID)
      return FALSE;

   // find i
   DWORD i;
   for (i = 0; i < PIERCOLOR_MAX; i++)
      if (dwID ==m_adwBalColor[i])
         break;
   if (i >= PIERCOLOR_MAX)
      return FALSE;

   // found it
   m_pTemplate->ObjectSurfaceRemove (dwID);
   m_adwBalColor[i] = 0;

   return TRUE;
}

/*************************************************************************************
CPiers::ClaimAllNecessaryColors - Call this after filling in the m_lPostts
 information. This will claim all the necessary colors, and free up
any claimed colors that aren't used.
*/
void CPiers::ClaimAllNecessaryColors (void)
{
   if (!m_pTemplate)
      return;

   // keep track of which ones we have
   BOOL afUse[PIERCOLOR_MAX];
   memset (afUse, 0, sizeof(afUse));

   // want colomn colors if use
   afUse[PIERCOLOR_POSTMAIN] = TRUE;
   if (m_afPostUse[0])  // buttom
      afUse[PIERCOLOR_POSTBASE] = TRUE;
   if (m_afPostUse[1] || m_afPostUse[2])
      afUse[PIERCOLOR_POSTTOP] = TRUE;
   if (m_afPostUse[3])
      afUse[PIERCOLOR_POSTBRACE] = TRUE;

   // loop through and remove those not using
   DWORD i;
   for (i = 0; i < PIERCOLOR_MAX; i++)
      if (m_adwBalColor[i] && !afUse[i])
         ClaimRemove (m_adwBalColor[i]);

   // create ones that using
   if (afUse[PIERCOLOR_POSTMAIN] && !m_adwBalColor[PIERCOLOR_POSTMAIN])
      ClaimSurface (PIERCOLOR_POSTMAIN,
         RGB(0x40, 0x40, 0x40),MATERIAL_PAINTSEMIGLOSS,
         L"Piers");
   if (afUse[PIERCOLOR_POSTBASE] && !m_adwBalColor[PIERCOLOR_POSTBASE])
      ClaimSurface (PIERCOLOR_POSTBASE,
         RGB(0xc0, 0xc0, 0xc0),MATERIAL_PAINTSEMIGLOSS,
         L"Piers, base");
   if (afUse[PIERCOLOR_POSTTOP] && !m_adwBalColor[PIERCOLOR_POSTTOP])
      ClaimSurface (PIERCOLOR_POSTTOP,
         RGB(0x40, 0x40, 0x40),MATERIAL_PAINTSEMIGLOSS,
         L"Piers, cap");
   if (afUse[PIERCOLOR_POSTBRACE] && !m_adwBalColor[PIERCOLOR_POSTBRACE])
      ClaimSurface (PIERCOLOR_POSTBRACE,
         RGB(0x40, 0x40, 0x40),MATERIAL_PAINTSEMIGLOSS,
         L"Piers, bracing");

}


/*************************************************************************************
CPiers::ClaimFindByID - Given a claim ID, finds the first claim with that ID.

inputs
   DWORD       dwID
returns
   BOOL - TRUE if found.
*/
BOOL CPiers::ClaimFindByID (DWORD dwID)
{
   if (!dwID)
      return FALSE;

   DWORD i;
   for (i = 0; i < PIERCOLOR_MAX; i++) {
      if (m_adwBalColor[i] == dwID)
         return TRUE;
   }

   if (m_pdsSkirting && m_pdsSkirting->ClaimFindByID (dwID))
      return TRUE;

   return FALSE;
}

/* PierAppearancePage
*/
BOOL PierAppearancePage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PAPIN pa = (PAPIN)pPage->m_pUserData;
   PCPiers pv = pa->pBal;
   DWORD dwRenderShard = pa->dwRenderShard;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         PCEscControl pControl;
         MeasureToString (pPage, L"height", pv->m_fHeight);
         MeasureToString (pPage, L"depthbelow", pv->m_fDepthBelow);
         MeasureToString (pPage, L"maxbigdist", pv->m_fMaxPostDistanceBig);

         WCHAR szTemp[32];
         swprintf (szTemp, L"%d", (int) pv->m_dwBalStyle);
         ESCMLISTBOXSELECTSTRING lss;
         memset (&lss, 0, sizeof(lss));
         lss.fExact = TRUE;
         lss.iStart = -1;
         lss.psz = szTemp;
         pControl = pPage->ControlFind (L"appear");
         if (pControl)
            pControl->Message (ESCM_LISTBOXSELECTSTRING, &lss);

         pControl = pPage->ControlFind (L"modify");
         if (pControl)
            pControl->Enable (pv->m_dwBalStyle == PS_CUSTOM);

         pControl = pPage->ControlFind (L"showskirting");
         if (pControl)
            pControl->AttribSetBOOL (Checked(), pv->m_fShowSkirting);
      }
      break;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;

         // find otu which is checked
         if (!_wcsicmp(p->pControl->m_pszName, L"showskirting")) {

            pv->m_pTemplate->m_pWorld->ObjectAboutToChange (pv->m_pTemplate);
            pv->m_fShowSkirting = p->pControl->AttribGetBOOL (Checked());

            // recalculate
            pv->DeterminePostLocations(dwRenderShard);
            pv->ExtendPostsToGround(dwRenderShard);

            pv->m_pTemplate->m_pWorld->ObjectChanged (pv->m_pTemplate);
            return TRUE;
         }
      }
      break;

   case ESCN_EDITCHANGE:
      {
         if (pv->m_pTemplate->m_pWorld)
            pv->m_pTemplate->m_pWorld->ObjectAboutToChange (pv->m_pTemplate);

         // since any edit box change will cause a change in parameters, get them all
         fp f;
         if (MeasureParseString (pPage, L"height", &f))
            pv->m_fHeight = max(.01, f);
         if (MeasureParseString (pPage, L"depthbelow", &f))
            pv->m_fDepthBelow = max(0, f);
         if (MeasureParseString (pPage, L"maxbigdist", &f))
            pv->m_fMaxPostDistanceBig = max (.1, f);

         // recalculate
         pv->DeterminePostLocations(dwRenderShard);
         pv->ExtendPostsToGround(dwRenderShard);

         if (pv->m_pTemplate->m_pWorld)
            pv->m_pTemplate->m_pWorld->ObjectChanged (pv->m_pTemplate);
      }
      break;

   case ESCN_LISTBOXSELCHANGE:
      {
         PESCNLISTBOXSELCHANGE p = (PESCNLISTBOXSELCHANGE) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;
         if (!_wcsicmp(p->pControl->m_pszName, L"appear")) {
            DWORD dwVal;
            dwVal = _wtoi(p->pszName);
            if (dwVal == pv->m_dwBalStyle)
               return TRUE;   // no change

            if (pv->m_pTemplate->m_pWorld)
               pv->m_pTemplate->m_pWorld->ObjectAboutToChange (pv->m_pTemplate);

            pv->ParamFromStyle (dwVal);

            // BUGFIX - Recalc indent so if go to larger one not sticking out
            // of house boundary
            CSpline b,t;
            if (!pv->m_psOrigBottom || !pv->m_psOrigTop)
               return TRUE;   // BUGFIX - graceful failure
            pv->m_psOrigBottom->CloneTo (&b);
            pv->m_psOrigTop->CloneTo (&t);
            pv->NewSplines (dwRenderShard, &b, &t);

            pv->DeterminePostLocations(dwRenderShard);
            pv->ExtendPostsToGround(dwRenderShard);

            if (pv->m_pTemplate->m_pWorld)
               pv->m_pTemplate->m_pWorld->ObjectChanged (pv->m_pTemplate);

            PCEscControl pControl;
            pControl = pPage->ControlFind (L"modify");
            if (pControl)
               pControl->Enable (pv->m_dwBalStyle == PS_CUSTOM);
            return TRUE;
         }
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Pier appearance";
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}


/* PiersCustomPage
*/
BOOL PiersCustomPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PAPIN pa = (PAPIN)pPage->m_pUserData;
   PCPiers pv = pa->pBal;
   DWORD dwRenderShard = pa->dwRenderShard;
   static BOOL sInitializing = FALSE;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         sInitializing = TRUE;

         PCEscControl pControl;
         WCHAR szTemp[64], szTemp2[32];
         ESCMCOMBOBOXSELECTSTRING lss;
         memset (&lss, 0, sizeof(lss));

         DWORD i, j;
         for (i = 0; i < 5; i++) {
            swprintf (szTemp, L"postshape%d", (int) i);
            swprintf (szTemp2, L"%d", (int) pv->m_adwPostShape[i]);
            lss.fExact = TRUE;
            lss.iStart = -1;
            lss.psz = szTemp2;
            pControl = pPage->ControlFind (szTemp);
            if (pControl)
               pControl->Message (ESCM_COMBOBOXSELECTSTRING, &lss);


            for (j = 0; j < 3; j++) {
               swprintf (szTemp, L"postsize%d%d", i, j);
               MeasureToString (pPage, szTemp, pv->m_apPostSize[i].p[j]);
            }

            // only up to 3
            if (i >= 4)
               continue;
            swprintf (szTemp, L"postuse%d", (int) i);
            pControl = pPage->ControlFind (szTemp);
            if (pControl)
               pControl->AttribSetBOOL (Checked(), pv->m_afPostUse[i]);

            // only up to 2
            if (i >= 3)
               continue;
            swprintf (szTemp, L"posttaper%d", (int) i);
            swprintf (szTemp2, L"%d", (int) pv->m_adwPostTaper[i]);
            lss.fExact = TRUE;
            lss.iStart = -1;
            lss.psz = szTemp2;
            pControl = pPage->ControlFind (szTemp);
            if (pControl)
               pControl->Message (ESCM_COMBOBOXSELECTSTRING, &lss);

         }

         // brace below
         MeasureToString (pPage, L"bracebelow", pv->m_fBraceBelow);


         sInitializing = FALSE;
      }
      break;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;

         // if it's anything other than Back() then get info
         if (_wcsicmp(p->pControl->m_pszName, Back()))
            pPage->Message (ESCM_USER+86);
      }
      break;   // default

   case ESCN_EDITCHANGE:
      // just get new values
      pPage->Message (ESCM_USER+86);
      break;

   case ESCN_COMBOBOXSELCHANGE:
      // just get new values
      if (!sInitializing)
         pPage->Message (ESCM_USER+86);
      break;

   case ESCM_USER+86:   // sent to get new values
      {
         if (pv->m_pTemplate->m_pWorld)
            pv->m_pTemplate->m_pWorld->ObjectAboutToChange (pv->m_pTemplate);


         PCEscControl pControl;
         WCHAR szTemp[64];
         ESCMCOMBOBOXGETITEM gi;
         memset (&gi, 0, sizeof(gi));

         DWORD i, j;
         for (i = 0; i < 5; i++) {
            swprintf (szTemp, L"postshape%d", (int) i);
            pControl = pPage->ControlFind (szTemp);
            if (pControl) {
               gi.dwIndex = pControl->AttribGetInt (CurSel());
               pControl->Message (ESCM_COMBOBOXGETITEM, &gi);
               if (gi.pszName)
                  pv->m_adwPostShape[i] = _wtoi(gi.pszName);
            }


            for (j = 0; j < 3; j++) {
               swprintf (szTemp, L"postsize%d%d", i, j);
               MeasureParseString (pPage, szTemp, &pv->m_apPostSize[i].p[j]);
               pv->m_apPostSize[i].p[j] = max(.01, pv->m_apPostSize[i].p[j]);
            }

            // only up to 3
            if (i >= 4)
               continue;
            swprintf (szTemp, L"postuse%d", (int) i);
            pControl = pPage->ControlFind (szTemp);
            if (pControl)
               pv->m_afPostUse[i] = pControl->AttribGetBOOL (Checked());

            // only up to 2
            if (i >= 3)
               continue;
            swprintf (szTemp, L"posttaper%d", (int) i);
            pControl = pPage->ControlFind (szTemp);
            if (pControl) {
               gi.dwIndex = pControl->AttribGetInt (CurSel());
               pControl->Message (ESCM_COMBOBOXGETITEM, &gi);
               if (gi.pszName)
                  pv->m_adwPostTaper[i] = _wtoi(gi.pszName);
            }

         }

         // brace below
         MeasureParseString (pPage, L"bracebelow", &pv->m_fBraceBelow);
         pv->m_fBraceBelow = max(.01, pv->m_fBraceBelow);



         // recalculate
         pv->DeterminePostLocations(dwRenderShard);
         pv->ExtendPostsToGround(dwRenderShard);

         if (pv->m_pTemplate->m_pWorld)
            pv->m_pTemplate->m_pWorld->ObjectChanged (pv->m_pTemplate);
      }
      return TRUE;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Custom pier appearance";
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}

/*************************************************************************************
CPiers::AppearancePage - Brings up a page that displays the appearance of
the Piers.

inputs
   PCEscWindow    pWindow - window
   CObjectBuildBlock *pBB - If this Piers is in a building block, pass in the
                  building block object so that posts can be extended to the roof.
returns
   PWSTR - Return string
*/
PWSTR CPiers::AppearancePage (DWORD dwRenderShard, PCEscWindow pWindow, CObjectBuildBlock *pBB)
{
   APIN a;
   memset (&a, 0, sizeof(a));
   a.pBal = this;
   a.pBB = pBB;
   a.dwRenderShard = dwRenderShard;

   PWSTR psz;
appear:
   psz = pWindow->PageDialog (ghInstance, IDR_MMLPIERSAPPEARANCE, ::PierAppearancePage, &a);

   if (psz && !_wcsicmp(psz, L"modify")) {
      psz = pWindow->PageDialog (ghInstance, IDR_MMLPIERSCUSTOM, ::PiersCustomPage, &a);
      if (psz && !_wcsicmp(psz, Back()))
         goto appear;

      return psz;
   }
   else
      return psz;
}


/*************************************************************************************
CPiers::GenerateIntersect - Generates an intersection spline surface so can tell
when move control points and other bits.

inputs
   noen
returns
   none
*/
BOOL CPiers::GenerateIntersect (void)
{
   if (m_pssControlInter)
      return TRUE;


   // create a new one
   m_pssControlInter = new CSplineSurface;
   if (!m_pssControlInter)
      return FALSE;

   // create the array's
   CMem memPoints, memCurve;
   DWORD dwNum;
   dwNum = m_psBottom->OrigNumPointsGet ();
   if (!memPoints.Required (dwNum * sizeof(CPoint) * 2))
      return FALSE;
   if (!memCurve.Required ((dwNum+1) * sizeof(DWORD)))
      return FALSE;
   PCPoint pap;
   DWORD *padw;
   pap = (PCPoint) memPoints.p;
   padw = (DWORD*) memCurve.p;

   // fill it in
   DWORD i;
   for (i = 0; i < dwNum; i++) {
      m_psBottom->OrigPointGet (i, &pap[i + dwNum]);
      m_psTop->OrigPointGet (i, &pap[i]);
      m_psTop->OrigSegCurveGet (i, &padw[i]);

      // extend top
      CPoint p;
      p.Subtract (&pap[i+dwNum], &pap[i]);
      p.Normalize();
      p.Scale (m_fHeight * 2.0);
      p.Add (&pap[i]);
      pap[i+dwNum].Copy (&p);
   }

   // create it
   if (!m_pssControlInter->ControlPointsSet (m_psTop->LoopedGet(), FALSE, dwNum, 2,
      pap, padw, (DWORD*)SEGCURVE_LINEAR)) {

      delete m_pssControlInter;
      m_pssControlInter = NULL;
      return FALSE;
   }

   // done
   return TRUE;
}

  

/*************************************************************************************
CPiers::ControlPointQuery - Called to query the information about a control
point identified by dwID.

inputs
   DWORD       dwID - ID of control point
   POSCONTROL  pInfo - Filled in with the infomration
returns
   BOOL - TRUE if successful
*/
BOOL CPiers::ControlPointQuery (DWORD dwID, POSCONTROL pInfo)
{
   fp fKnobSize = .2;

   if (m_dwDisplayControl == 0) {
      // it's a corner
      DWORD dwNum = m_psOrigTop->OrigNumPointsGet();
      if (dwID >= dwNum*2)
         return FALSE;

      memset (pInfo,0, sizeof(*pInfo));

      pInfo->dwID = dwID;
      //pInfo->dwFreedom = 0;   // any direction
      pInfo->dwStyle = CPSTYLE_CUBE;
      pInfo->fSize = fKnobSize;
      pInfo->cColor = RGB(0x00,0xff,0xff);

      // top and bottom
      CPoint pt, pb;
      m_psOrigBottom->OrigPointGet (dwID % dwNum, &pb);
      m_psOrigTop->OrigPointGet (dwID % dwNum, &pt);
      pb.Subtract (&pt);
      pb.Normalize();
      pb.Scale (m_fHeight * 2);
      pb.Add (&pt);

      pInfo->pLocation.Copy ((dwID < dwNum) ? &pb : &pt);

      // name
      wcscpy (pInfo->szName, L"Piers corner");

      return TRUE;
   }
   else {
      // it's an opening
      if (dwID >= m_lCutouts.Num()*2)
         return FALSE;

      // make sure have spline info
      if (!GenerateIntersect())
         return FALSE;

      memset (pInfo,0, sizeof(*pInfo));

      pInfo->dwID = dwID;
      //pInfo->dwFreedom = 0;   // any direction
      pInfo->dwStyle = CPSTYLE_POINTER;
      pInfo->fSize = fKnobSize;
      pInfo->cColor = RGB(0xff,0x00,0xff);

      // find out where this edge is
      fp *paf;
      paf = (fp*) m_lCutouts.Get (dwID / 2);

      // convert this to space
      m_pssControlInter->HVToInfo ((fp) paf[dwID%2], .1, &pInfo->pLocation);
      //m_pssControlInter->HVToInfo ((fp) paf[dwID%2] - ((dwID % 2) ? CLOSE : -CLOSE), .1, &pInfo->pDirection);
      //pInfo->pDirection.Subtract (&pInfo->pLocation);
      //pInfo->pDirection.Normalize();

      // name
      wcscpy (pInfo->szName, L"Piers opening");

      return TRUE;
   }
}
/*************************************************************************************
CPiers::ControlPointSet - Called to change the valud of a control point.

inputs
   DWORD       dwID - ID
   PCPoint     pVal - Contains the new location, in object coordinates
   PCPoint     pViewer - Where the viewer is from
returns
   BOOL - TRUE if successful
*/
BOOL CPiers::ControlPointSet (DWORD dwRenderShard, DWORD dwID, PCPoint pVal, PCPoint pViewer,
                                   PCObjectBuildBlock pBB)
{
   if (m_dwDisplayControl == 0) {
      // it's a corner
      DWORD dwNum = m_psOrigTop->OrigNumPointsGet();
      if (dwID >= dwNum*2)
         return FALSE;

      // get the current points
      CPoint pt, pb;
      m_psOrigBottom->OrigPointGet (dwID % dwNum, &pb);
      m_psOrigTop->OrigPointGet (dwID % dwNum, &pt);
      pb.Subtract (&pt);
      pb.Normalize();
      pb.Scale (m_fHeight * 2);
      pb.Add (&pt);

      // get the old top and bottom and curvature
      CMem memTop, memBottom, memCurve;
      if (!memTop.Required (dwNum * sizeof(CPoint)))
         return FALSE;
      if (!memBottom.Required (dwNum * sizeof(CPoint)))
         return FALSE;
      if (!memCurve.Required (dwNum * sizeof(DWORD)))
         return FALSE;
      PCPoint pTop, pBottom;
      DWORD *padw;
      DWORD dwMin, dwMax;
      fp fDetail;
      BOOL fLooped;
      pTop = (PCPoint) memTop.p;
      pBottom = (PCPoint) memBottom.p;
      padw = (DWORD*) memCurve.p;
      DWORD i;
      for (i = 0; i < dwNum; i++) {
         m_psOrigBottom->OrigPointGet (i, &pBottom[i]);
         m_psOrigTop->OrigPointGet (i, &pTop[i]);
         m_psOrigTop->OrigSegCurveGet (i, &padw[i]);
      }
      fLooped = m_psOrigTop->LoopedGet ();
      m_psOrigTop->DivideGet (&dwMin, &dwMax, &fDetail);

      // new points
      CPoint pnt, pnb;
      pnt.Copy (&pt);
      pnb.Copy (&pb);
      if (dwID < dwNum)
         pnb.Copy (pVal);
      else
         pnt.Copy (pVal);

      // If limit to ground then do that
      if (m_fForceLevel)
         pnt.p[2] = pt.p[2];

      // If limit to being straight up/down then do that
      if (m_fForceVert) {
         if (dwID < dwNum) {  // moved the bottom
            pnt.p[0] = pnb.p[0];
            pnt.p[1] = pnb.p[1];
         }
         else {   // move the top
            pnb.p[0] = pnt.p[0];
            pnb.p[1] = pnt.p[1];
         }
      }

      // make sure top is 1m away, so if raise curves it all works out
      pnb.Subtract (&pnt);
      pnb.Normalize();
      pnb.Add (&pnt);

      // store into list
      pBottom[dwID % dwNum].Copy (&pnb);
      pTop[dwID % dwNum].Copy (&pnt);

      // create new splines
      CSpline sTop, sBottom;
      sTop.Init (fLooped, dwNum, pTop, NULL, padw, dwMin, dwMax, fDetail);
      sBottom.Init (fLooped, dwNum, pBottom, NULL, padw, dwMin, dwMax, fDetail);

      // tell the world we're about to change
      if (m_pTemplate->m_pWorld)
         m_pTemplate->m_pWorld->ObjectAboutToChange (m_pTemplate);

      // set it
      NewSplines (dwRenderShard, &sBottom, &sTop);

      // tell the world we've changed
      if (m_pTemplate->m_pWorld)
         m_pTemplate->m_pWorld->ObjectChanged (m_pTemplate);

      return FALSE;
   }
   else {
      // it's an opening
      if (dwID >= m_lCutouts.Num()*2)
         return FALSE;

      // make sure have all info that need
      if (!GenerateIntersect())
         return FALSE;

      // find out where this edge is now
      fp *paf;
      paf = (fp*) m_lCutouts.Get (dwID / 2);
      TEXTUREPOINT pOld;
      pOld.h = paf[dwID%2];
      pOld.v = .5;

      // find new
      TEXTUREPOINT tpNew, tpNew2;
      if (!m_pssControlInter->HVDrag (&pOld, pVal, pViewer, &tpNew))
         return FALSE;  // doesnt intersect

      // try from the opposite direction since can see through Pierss
      CPoint pv2;
      pv2.Subtract (pVal, pViewer);
      pv2.Add (pVal);
      if (m_pssControlInter->HVDrag (&pOld, pVal, &pv2, &tpNew2)) {
         if (fabs(tpNew2.h - pOld.h) < fabs(tpNew.h - pOld.h))
            tpNew = tpNew2;
      }

      // tell the world we're about to change
      if (m_pTemplate->m_pWorld)
         m_pTemplate->m_pWorld->ObjectAboutToChange (m_pTemplate);

      // move it
      paf[dwID%2] = tpNew.h;
      if (dwID%2) {
         // moved right
         paf[1] = max(paf[1], paf[0]);

         // overlays shouldn't go over one another
         if (dwID/2+1 < m_lCutouts.Num())
            paf[1] = min(paf[1], paf[2]);
      }
      else {
         // moved left
         paf[0] = min(paf[1], paf[0]);

         // overlays souldnt go over one another
         if (dwID/2)
            paf[0] = max(paf[0], paf[-1]);
      }

      // recalc
      DeterminePostLocations(dwRenderShard);
      ExtendPostsToGround(dwRenderShard);

      // tell the world we've changed
      if (m_pTemplate->m_pWorld)
         m_pTemplate->m_pWorld->ObjectChanged (m_pTemplate);

      return TRUE;
   }
}


/*************************************************************************************
CPiers::ControlPointEnum - Called to enumerate a list of control point IDs
into the list.

inputs
   PCListFixed       plDWORD - Should be filled in with control points
returns
   none
*/
void CPiers::ControlPointEnum (PCListFixed plDWORD)
{
   DWORD i;
   DWORD dwPoints;

   if (m_dwDisplayControl == 0) {
      // showing corners
      dwPoints = m_psOrigTop->OrigNumPointsGet() * 2;
   }
   else {
      // showing openings
      dwPoints = m_lCutouts.Num() * 2;
   }


   for (i = 0; i < dwPoints; i++)
      plDWORD->Add (&i);
}

/********************************************************************************
GenerateThreeDForOpenings - Given a spline on a surface, this sets a threeD
control with the spline.

inputs
   PWSTR       pszControl - Control name.
   PCEscPage   pPage - Page
   PCSpline    pSpline - Bottom spline (used for BPOSTINFO)
   PCListFixed plPosts - List of BPOSTINFO
returns
   BOOl - TRUE if success

NOTE: The ID's are such:
   LOWORD = post ID | 0x10000
*/
static BOOL GenerateThreeDForOpenings (PWSTR pszControl, PCEscPage pPage, PCSpline pSpline, PCListFixed plPosts)
{
   PCEscControl pControl = pPage->ControlFind (pszControl);
   if (!pControl)
      return FALSE;

   // figure out the center
   CPoint pCenter, pTemp, pMin, pMax;
   DWORD i;
   pCenter.Zero();
   pMin.Zero();
   pMax.Zero();
   DWORD x, dwNum;
   dwNum = plPosts->Num();
   PBPOSTINFO pbi;
   pbi = (PBPOSTINFO) plPosts->Get(0);
   for (x = 0; x < dwNum; x++) {
      pSpline->LocationGet (pbi[x].fLoc, &pTemp);

      if (x == 0) {
         pMin.Copy (&pTemp);
         pMax.Copy (&pTemp);
         continue;
      }

      // else, do min/max
      for (i = 0; i < 3; i++) {
         pMin.p[i] = min(pMin.p[i], pTemp.p[i]);
         pMax.p[i] = max(pMax.p[i], pTemp.p[i]);
      }
   }
   pCenter.Copy (&pMin);
   pCenter.Add (&pMax);
   pCenter.Scale (.5);

   // figure out the maximum distance
   fp fMax;
   fMax = max(pMax.p[1] - pMin.p[1], pMax.p[0] - pMin.p[0]);
   fMax /= 10;  // so is larger

   // when draw points, get the point, subtract the center, and divide by fMax

   // use gmemtemp
   MemZero (&gMemTemp);

   // draw the outline
   for (x = 0; x < dwNum; x++) {
      if ((x+1 >= dwNum) && !pSpline->LoopedGet())
         continue;

      CPoint p1, p2;
      pSpline->LocationGet (pbi[x].fLoc, &p1);
      pSpline->LocationGet (pbi[(x+1)%dwNum].fLoc, &p2);

      // convert from HV to object space
      p1.Subtract (&pCenter);
      p2.Subtract (&pCenter);
      p1.Scale (1.0 / fMax);
      p2.Scale (1.0 / fMax);


      MemCat (&gMemTemp, pbi[x].fRight ? L"<colordefault color=#000000/>" : L"<colordefault color=#c0c0c0/>");
      // set the ID
      MemCat (&gMemTemp, L"<id val=");
      MemCat (&gMemTemp, (int)x | 0x10000);
      MemCat (&gMemTemp, L"/>");

      MemCat (&gMemTemp, L"<shapearrow tip=false width=.1");

      WCHAR szTemp[128];
      swprintf (szTemp, L" p1=%g,%g,%g p2=%g,%g,%g/>",
         (double)p1.p[0], (double)p1.p[1], (double)0.0, (double)p2.p[0], (double)p2.p[1], (double)0.0);
      MemCat (&gMemTemp, szTemp);

      // draw the post
      MemCat (&gMemTemp, L"<matrixpush>");
      swprintf (szTemp, L"<translate point=%g,%g,%g/>",
         (double)p1.p[0], (double)p1.p[1], (double)0.0);
      MemCat (&gMemTemp, szTemp);
      MemCat (&gMemTemp, L"<colordefault color=#000080/>");
      // set the ID
      MemCat (&gMemTemp, L"<id val=1/>");
      MemCat (&gMemTemp, L"<MeshSphere radius=.3/><shapemeshsurface/>");
      MemCat (&gMemTemp, L"</matrixpush>");
   }

   // set the threeD control
   ESCMTHREEDCHANGE tc;
   memset (&tc, 0, sizeof(tc));
   tc.pszMML = (PWSTR) gMemTemp.p;
   pControl->Message (ESCM_THREEDCHANGE, &tc);

   return TRUE;
}


/* PiersDisplayPage
*/
BOOL PiersDisplayPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCPiers pv = (PCPiers)pPage->m_pUserData;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         PCEscControl pControl;

         // set thecheckboxes
         pControl = NULL;
         switch (pv->m_dwDisplayControl) {
         case 0:  // corner
            pControl = pPage->ControlFind (L"corner");
            break;
         case 1:  // openings
         default:
            pControl = pPage->ControlFind (L"openings");
            break;
         }
         if (pControl)
            pControl->AttribSetBOOL (Checked(), TRUE);

         pControl = pPage->ControlFind (L"forcevert");
         if (pControl)
            pControl->AttribSetBOOL (Checked(), pv->m_fForceVert);
         pControl = pPage->ControlFind (L"forcelevel");
         if (pControl)
            pControl->AttribSetBOOL (Checked(), pv->m_fForceLevel);
      }
      break;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;

         // find otu which is checked
         PCEscControl pControl;
         DWORD dwNew;

         if (!_wcsicmp(p->pControl->m_pszName, L"forcevert") ||
            !_wcsicmp(p->pControl->m_pszName, L"forcelevel")) {

            pv->m_pTemplate->m_pWorld->ObjectAboutToChange (pv->m_pTemplate);
            if (!_wcsicmp(p->pControl->m_pszName, L"forcevert"))
               pv->m_fForceVert = p->pControl->AttribGetBOOL (Checked());
            else
               pv->m_fForceLevel = p->pControl->AttribGetBOOL (Checked());
            pv->m_pTemplate->m_pWorld->ObjectChanged (pv->m_pTemplate);
            return TRUE;
         }

         if ((pControl = pPage->ControlFind (L"corner")) && pControl->AttribGetBOOL(Checked()))
            dwNew = 0;  // curve;
         else if ((pControl = pPage->ControlFind (L"openings")) && pControl->AttribGetBOOL(Checked()))
            dwNew = 1; // roof
         else
            break;   // none of the above
         if (dwNew == pv->m_dwDisplayControl)
            return TRUE;   // no change

         pv->m_pTemplate->m_pWorld->ObjectAboutToChange (pv->m_pTemplate);
         pv->m_dwDisplayControl = dwNew;
         pv->m_pTemplate->m_pWorld->ObjectChanged (pv->m_pTemplate);

         // make sure they're visible
         pv->m_pTemplate->m_pWorld->SelectionClear();
         GUID gObject;
         pv->m_pTemplate->GUIDGet (&gObject);
         pv->m_pTemplate->m_pWorld->SelectionAdd (pv->m_pTemplate->m_pWorld->ObjectFind(&gObject));
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




/* PiersOpeningsPage
*/
BOOL PiersOpeningsPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PAPIN pa = (PAPIN)pPage->m_pUserData;
   PCPiers pv = pa->pBal;
   DWORD dwRenderShard = pa->dwRenderShard;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         // draw images
         GenerateThreeDForOpenings (L"edgeaddremove", pPage, pv->m_psTop, &pv->m_lPosts);
      }
      break;


   case ESCN_THREEDCLICK:
      {
         PESCNTHREEDCLICK p = (PESCNTHREEDCLICK) pParam;
         if (!p->pControl->m_pszName)
            break;

         if (_wcsicmp(p->pControl->m_pszName, L"edgeaddremove"))
            break;

         // figure out x, y, and what clicked on
         DWORD x, dwMode;
         dwMode = (BYTE)(p->dwMajor >> 16);
         x = (WORD) p->dwMajor;
         if (dwMode != 1)
            break;
         if (x > pv->m_lPosts.Num())
            break;

         // find out if clicked on a post that's open or not
         PBPOSTINFO pbi;
         DWORD dwNum;
         dwNum = pv->m_lPosts.Num();
         pbi = (PBPOSTINFO) pv->m_lPosts.Get(0);

         // world to change
         if (pv->m_pTemplate && pv->m_pTemplate->m_pWorld)
            pv->m_pTemplate->m_pWorld->ObjectAboutToChange (pv->m_pTemplate);

         // remove or add
         PTEXTUREPOINT pt;
         DWORD i;
         if (pbi[x].fRight) {
            // add a cutout
            TEXTUREPOINT tc;
            tc.h = pbi[x].fLoc;
            if (x+1 < dwNum)
               tc.v = pbi[x+1].fLoc;
            else
               tc.v = 1.0;
            if (tc.h+CLOSE < tc.v) {
               for (i = 0; i < pv->m_lCutouts.Num(); i++) {
                  pt = (PTEXTUREPOINT) pv->m_lCutouts.Get(i);
                  if (tc.v <= pt->h)
                     break;
               };
               if (i < pv->m_lCutouts.Num())
                  pv->m_lCutouts.Insert (i, &tc);
               else
                  pv->m_lCutouts.Add (&tc);
            }
         }
         else {
            // remove this cutout
            for (i = 0; i < pv->m_lCutouts.Num(); i++) {
               pt = (PTEXTUREPOINT) pv->m_lCutouts.Get(i);
               if ((pbi[x].fLoc > pt->h - CLOSE) && (pbi[x].fLoc < pt->v + CLOSE))
                  break;
            }
            if (i < pv->m_lCutouts.Num())
               pv->m_lCutouts.Remove (i);
         }

         // recalculate
         pv->DeterminePostLocations(dwRenderShard);
         pv->ExtendPostsToGround(dwRenderShard);

         // world to change
         if (pv->m_pTemplate && pv->m_pTemplate->m_pWorld)
            pv->m_pTemplate->m_pWorld->ObjectChanged (pv->m_pTemplate);

         // redraw the shapes
         GenerateThreeDForOpenings (L"edgeaddremove", pPage, pv->m_psTop, &pv->m_lPosts);
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Pier perimeter openings";
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}



/********************************************************************************
GenerateThreeDFromSpline - Given a spline on a surface, this sets a threeD
control with the spline.

inputs
   PWSTR       pszControl - Control name.
   PCEscPage   pPage - Page
   PCSpline    pSpline - Spline to draw
   DWORD       dwUse - If 0 it's for adding/remove splines, else if 1 it's for cycling curves
returns
   BOOl - TRUE if success

NOTE: The ID's are such:
   LOBYTE = x
   3rd lowest byte = 1 for edge, 2 for point
*/
static BOOL GenerateThreeDFromSpline (PWSTR pszControl, PCEscPage pPage, PCSpline pSpline,
                                      DWORD dwUse)
{
   PCEscControl pControl = pPage->ControlFind (pszControl);
   if (!pControl)
      return FALSE;

   // figure out center
   CPoint pMin, pMax, pt;
   DWORD i,j;
   for (i = 0; i < pSpline->OrigNumPointsGet(); i++) {
      pSpline->OrigPointGet(i, &pt);
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
   // NOTE: assume p[1] == 0
   fMax = max(0.001, fMax);
   fMax /= 5;  // so is larger

   // when draw points, get the point, subtract the center, and divide by fMax

   // use gmemtemp
   MemZero (&gMemTemp);
   MemCat (&gMemTemp, L"<backculloff/>");


   // draw the outline
   DWORD dwNum;
   dwNum = pSpline->OrigNumPointsGet();
   DWORD x;
   for (x = 0; x < dwNum; x++) {
      // if it's the last point and we're not looped then quit
      if ((x+1 >= dwNum) && !(pSpline->LoopedGet()))
         continue;

      CPoint p1, p2;
      pSpline->OrigPointGet (x, &p1);
      p1.p[3] = 1;
      pSpline->OrigPointGet ((x+1) % dwNum, &p2);
      p2.p[3] = 1;

      // center and scale
      p1.Subtract (&pCenter);
      p2.Subtract (&pCenter);
      p1.Scale (1.0 / fMax);
      p2.Scale (1.0 / fMax);


      // draw a line
      if (dwUse == 0)
         MemCat (&gMemTemp, L"<colordefault color=#c0c0c0/>");
      else {
         DWORD dwSeg;
         pSpline->OrigSegCurveGet (x, &dwSeg);
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

      MemCat (&gMemTemp, L"<shapearrow tip=false width=.2");

      WCHAR szTemp[128];
      swprintf (szTemp, L" p1=%g,%g,%g p2=%g,%g,%g/>",
         (double)p1.p[0], (double)p1.p[1], (double)0.0, (double)p2.p[0], (double)p2.p[1],(double) 0.0);
      MemCat (&gMemTemp, szTemp);

      // do push point if more than 3 points
      if ((dwUse == 0) && (dwNum > 2)) {
         MemCat (&gMemTemp, L"<matrixpush>");
         swprintf (szTemp, L"<translate point=%g,%g,%g/>",
            (double)p1.p[0], (double)p1.p[1], (double)0.0);
         MemCat (&gMemTemp, szTemp);
         MemCat (&gMemTemp, L"<colordefault color=#ff0000/>");
         // set the ID
         MemCat (&gMemTemp, L"<id val=");
         MemCat (&gMemTemp, (int)((2 << 16) | x));
         MemCat (&gMemTemp, L"/>");

         MemCat (&gMemTemp, L"<MeshSphere radius=.4/><shapemeshsurface/>");

         MemCat (&gMemTemp, L"</matrixpush>");
      }
   }

   // set the threeD control
   ESCMTHREEDCHANGE tc;
   memset (&tc, 0, sizeof(tc));
   tc.pszMML = (PWSTR) gMemTemp.p;
   pControl->Message (ESCM_THREEDCHANGE, &tc);

   return TRUE;
}


/* PiersCornersPage
*/
BOOL PiersCornersPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCPiers pv = (PCPiers)pPage->m_pUserData;
   DWORD dwRenderShard = pv->m_dwRenderShardTemp;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         // draw images
         GenerateThreeDFromSpline (L"edgeaddremove", pPage, pv->m_psTop, 0);
         GenerateThreeDFromSpline (L"edgecurve", pPage, pv->m_psTop, 1);

         PCEscControl pControl;
         pControl = pPage->ControlFind (L"looped");
         if (pControl) {
            pControl->Enable (pv->m_psTop->OrigNumPointsGet() >= 3);
            pControl->AttribSetBOOL (Checked(), pv->m_psTop->LoopedGet());
         }
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
         CMem  memPoints, memTop;
         CMem  memSegCurve;
         DWORD dwOrig;
         dwOrig = pv->m_psTop->OrigNumPointsGet();
         if (!memPoints.Required ((dwOrig+1) * sizeof(CPoint)))
            return TRUE;
         if (!memTop.Required ((dwOrig+1) * sizeof(CPoint)))
            return TRUE;
         if (!memSegCurve.Required ((dwOrig+1) * sizeof(DWORD)))
            return TRUE;

         // load it in
         PCPoint paPoints, paTop;
         DWORD *padw;
         paPoints = (PCPoint) memPoints.p;
         paTop = (PCPoint) memTop.p;
         padw = (DWORD*) memSegCurve.p;
         DWORD i;
         for (i = 0; i < dwOrig; i++) {
            pv->m_psOrigBottom->OrigPointGet (i, paPoints+i);
            pv->m_psOrigTop->OrigPointGet (i, paTop+i);
         }
         for (i = 0; i < dwOrig; i++)
            pv->m_psOrigTop->OrigSegCurveGet (i, padw + i);
         DWORD dwMinDivide, dwMaxDivide;
         BOOL fLooped;
         fp fDetail;
         pv->m_psOrigTop->DivideGet (&dwMinDivide, &dwMaxDivide, &fDetail);
         fLooped = pv->m_psOrigTop->LoopedGet();

         if (fCol) {
            if (dwMode == 1) {
               // inserting
               memmove (paPoints + (x+1), paPoints + x, sizeof(CPoint) * (dwOrig-x));
               paPoints[x+1].Add (paPoints + ((x+2) % (dwOrig+1)));
               paPoints[x+1].Scale (.5);

               memmove (paTop + (x+1), paTop + x, sizeof(CPoint) * (dwOrig-x));
               paTop[x+1].Add (paTop + ((x+2) % (dwOrig+1)));
               paTop[x+1].Scale (.5);

               memmove (padw + (x+1), padw + x, sizeof(DWORD) * (dwOrig - x));
               dwOrig++;
            }
            else if (dwMode == 2) {
               // deleting
               memmove (paPoints + x, paPoints + (x+1), sizeof(CPoint) * (dwOrig-x-1));
               memmove (paTop + x, paTop + (x+1), sizeof(CPoint) * (dwOrig-x-1));
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

         // create the two splines
         CSpline sBottom, sTop;
         if (dwOrig < 3)
            fLooped = FALSE;
         sBottom.Init (fLooped, dwOrig, paPoints, NULL, padw, 0, 3, .1);
         sTop.Init (fLooped, dwOrig, paTop, NULL, padw, 0, 3, .1);

         pv->m_pTemplate->m_pWorld->ObjectAboutToChange (pv->m_pTemplate);
         pv->NewSplines (dwRenderShard, &sBottom, &sTop);
         pv->m_pTemplate->m_pWorld->ObjectChanged (pv->m_pTemplate);

         // redraw the shapes
         GenerateThreeDFromSpline (L"edgeaddremove", pPage, pv->m_psTop, 0);
         GenerateThreeDFromSpline (L"edgecurve", pPage, pv->m_psTop, 1);

         PCEscControl pControl;
         pControl = pPage->ControlFind (L"looped");
         if (pControl) {
            pControl->Enable (pv->m_psTop->OrigNumPointsGet() >= 3);
            pControl->AttribSetBOOL (Checked(), pv->m_psTop->LoopedGet());
         }
      }
      break;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;

         // if any of the buttons are pressed redraw
         if (!_wcsicmp(p->pControl->m_pszName, L"looped")) {
            // allocate enough memory so can do the calculations
            CMem  memPoints, memTop;
            CMem  memSegCurve;
            DWORD dwOrig;
            dwOrig = pv->m_psOrigTop->OrigNumPointsGet();
            if (!memPoints.Required ((dwOrig+1) * sizeof(CPoint)))
               return TRUE;
            if (!memTop.Required ((dwOrig+1) * sizeof(CPoint)))
               return TRUE;
            if (!memSegCurve.Required ((dwOrig+1) * sizeof(DWORD)))
               return TRUE;

            // load it in
            PCPoint paPoints, paTop;
            DWORD *padw;
            paPoints = (PCPoint) memPoints.p;
            paTop = (PCPoint) memTop.p;
            padw = (DWORD*) memSegCurve.p;
            DWORD i;
            for (i = 0; i < dwOrig; i++) {
               pv->m_psOrigBottom->OrigPointGet (i, paPoints+i);
               pv->m_psOrigTop->OrigPointGet (i, paTop+i);
            }
            for (i = 0; i < dwOrig; i++)
               pv->m_psOrigTop->OrigSegCurveGet (i, padw + i);
            DWORD dwMinDivide, dwMaxDivide;
            BOOL fLooped;
            fp fDetail;
            pv->m_psOrigTop->DivideGet (&dwMinDivide, &dwMaxDivide, &fDetail);
            fLooped = p->pControl->AttribGetBOOL (Checked());

            // create the two splines
            CSpline sBottom, sTop;
            if (dwOrig < 3)
               fLooped = FALSE;
            sBottom.Init (fLooped, dwOrig, paPoints, NULL, padw, 0, 3, .1);
            sTop.Init (fLooped, dwOrig, paTop, NULL, padw, 0, 3, .1);

            pv->m_pTemplate->m_pWorld->ObjectAboutToChange (pv->m_pTemplate);
            pv->NewSplines (dwRenderShard, &sBottom, &sTop);
            pv->m_pTemplate->m_pWorld->ObjectChanged (pv->m_pTemplate);

            // redraw the shapes
            GenerateThreeDFromSpline (L"edgeaddremove", pPage, pv->m_psTop, 0);
            GenerateThreeDFromSpline (L"edgecurve", pPage, pv->m_psTop, 1);

            return TRUE;
         }

      }
      break;   // default

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Pier perimeter corners";
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}

/*************************************************************************************
CPiers::OpeningsPage - Brings up a page that displays the appearance of
the Piers.

inputs
   PCEscWindow    pWindow - window
   CObjectBuildBlock *pBB - If this Piers is in a building block, pass in the
                  building block object so that posts can be extended to the roof.
returns
   PWSTR - Return string
*/
PWSTR CPiers::OpeningsPage (DWORD dwRenderShard, PCEscWindow pWindow, CObjectBuildBlock *pBB)
{
   APIN a;
   memset (&a, 0, sizeof(a));
   a.pBal = this;
   a.pBB = pBB;
   a.dwRenderShard = dwRenderShard;

   PWSTR psz;
   psz = pWindow->PageDialog (ghInstance, IDR_MMLPIERSOPENINGS, ::PiersOpeningsPage, &a);
   return psz;
}

/*************************************************************************************
CPiers::DisplayPage - Brings up a page that specifies what control points are visible.

inputs
   PCEscWindow    pWindow - window
returns
   PWSTR - Return string
*/
PWSTR CPiers::DisplayPage (PCEscWindow pWindow)
{
   PWSTR psz;
   psz = pWindow->PageDialog (ghInstance, IDR_MMLPIERSDISPLAY, ::PiersDisplayPage, this);
   return psz;
}


/*************************************************************************************
CPiers::CornersPage - Modify corners.

inputs
   PCEscWindow    pWindow - window
returns
   PWSTR - Return string
*/
PWSTR CPiers::CornersPage (DWORD dwRenderShard, PCEscWindow pWindow)
{
   PWSTR psz;
   m_dwRenderShardTemp = dwRenderShard;
   psz = pWindow->PageDialog (ghInstance, IDR_MMLPIERSCORNERS, ::PiersCornersPage, this);
   return psz;
}


/*************************************************************************************
CPiers::Deconstruct -
Tells the object to deconstruct itself into sub-objects.
Basically, new objects will be added that exactly mimic this object,
and any embedeeding objects will be moved to the new ones.
NOTE: THIS CALL DOES NOT DELETE THE OLD ONE.
If fAct is FALSE the function is just a query, that returns
TRUE if the object cares about adjustment and can try, FALSE if it can't.

NOTE: Often overridden.
*/
BOOL CPiers::Deconstruct (DWORD dwRenderShard, BOOL fAct)
{
   if (!m_pTemplate || !m_pTemplate->m_pWorld)
      return FALSE;
   if (!fAct)
      return TRUE;

   PCWorldSocket pWorld;
   pWorld = m_pTemplate->m_pWorld;

   // loop through all the clumns... create them and clone them
   DWORD i, j;
   for (i = 0; i < m_lPosts.Num(); i++) {
      PBPOSTINFO pbi = (PBPOSTINFO) m_lPosts.Get(i);
      if (!pbi->pColumn)
         return NULL;

      // create a new one
      PCObjectColumn pss;
      OSINFO OI;
      memset (&OI, 0, sizeof(OI));
      OI.gCode = CLSID_Column;
      //OI.gSub = GUID_NULL;
      OI.dwRenderShard = dwRenderShard;
      pss = new CObjectColumn((PVOID) 0, &OI);
      if (!pss)
         continue;

      // BUGFIX - Set a name
      pss->StringSet (OSSTRING_NAME, L"Pier column");

      pWorld->ObjectAdd (pss);

      pWorld->ObjectAboutToChange (pss);

      pbi->pColumn->CloneTo (&pss->m_Column);

      // transfer colors
      for (j = 0; j < PIERCOLOR_MAX; j++) {
         if (!m_adwBalColor[j])
            continue;

         PCObjectSurface pc;
         pc = m_pTemplate->ObjectSurfaceFind (m_adwBalColor[j]);
         if (!pc)
            continue;

         DWORD dwInColumn;
         switch (j) {
         case PIERCOLOR_POSTMAIN:
            dwInColumn = 1;
            break;
         case PIERCOLOR_POSTBASE:
            dwInColumn = 2;
            break;
         case PIERCOLOR_POSTTOP:
            dwInColumn = 3;
            break;
         default:
         case PIERCOLOR_POSTBRACE:
            dwInColumn = 4;
            break;
         }
         pss->ObjectSurfaceRemove (dwInColumn);
         DWORD dwTemp;
         dwTemp = pc->m_dwID;
         pc->m_dwID = dwInColumn;
         pss->ObjectSurfaceAdd (pc);
         pc->m_dwID = dwTemp; // restore
      }

      // set the matrix
      pss->ObjectMatrixSet (&m_pTemplate->m_MatrixObject);
      // note changed
      pWorld->ObjectChanged (pss);
   }

   // Deconstruct skirting
   if (m_pdsSkirting) {
      // create a new one
      PCObjectStructSurface pss;
      OSINFO OI;
      memset (&OI, 0, sizeof(OI));
      OI.gCode = CLSID_StructSurface;
      OI.gSub = CLSID_StructSurfaceExternalStudWall;
      OI.dwRenderShard = dwRenderShard;
      pss = new CObjectStructSurface((PVOID) 0x100c, &OI);
      if (!pss)
         return FALSE;

      // BUGFIX - Set a name
      pss->StringSet (OSSTRING_NAME, L"Pier cladding");

      pWorld->ObjectAdd (pss);

      pWorld->ObjectAboutToChange (pss);

      // remove any colors or links in the new object
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
      m_pdsSkirting->ClaimCloneTo (&pss->m_ds, pss);

      // imbue it with new information
      m_pdsSkirting->CloneTo (&pss->m_ds, pss);

      // set the matrix
      CMatrix m;
      m.Identity();
      pss->m_ds.MatrixSet (&m);
      m_pdsSkirting->MatrixGet(&m);
      m.MultiplyRight (&m_pTemplate->m_MatrixObject);
      pss->ObjectMatrixSet (&m);

      // NOTE: Since cant embed objects from skirting, dont have to worry about this
#if 0
      // move embedded objects over that are embedded in this one
      while (TRUE) {
         for (j = 0; j < ContEmbeddedNum(); j++) {
            GUID gEmbed;
            TEXTUREPOINT pHV;
            fp fRotation;
            DWORD dwSurface;
            if (!ContEmbeddedEnum (j, &gEmbed))
               continue;
            if (!ContEmbeddedLocationGet (&gEmbed, &pHV, &fRotation, &dwSurface))
               continue;

            if (!m_pdsSkirting->ClaimFindByID (dwSurface))
               continue;

            // else, it's on this surface, so remove it from this object and
            // move it to the new one
            ContEmbeddedRemove (&gEmbed);
            pss->ContEmbeddedAdd (&gEmbed, &pHV, fRotation, dwSurface);

            // set j=0 just to make sure repeat
            j = 0;
            break;
         }

         // keep repeating until no more objects embedded in this surface
         if (j >= ContEmbeddedNum())
            break;
      }
#endif

      // eventually need to shorten walls if clones walls are clipped
      // and dont use extra
      // Dont call: pss->m_ds.ShrinkToCutouts (FALSE);

      // note changed
      pWorld->ObjectChanged (pss);
   }

   return TRUE;
}


