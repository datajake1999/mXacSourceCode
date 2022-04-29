/**********************************************************************************
CBalustrade - Used for drawing balustrades.

begun 22/1/2002 by Mike Rozak
Copyright 2002 Mike Rozak. All rights reserved
*/

#include <windows.h>
#include "escarpment.h"
#include "..\M3D.h"
#include "resource.h"

// appearance info
typedef struct {
   PCBalustrade      pBal;    // balustreade
   PCObjectBuildBlock   pBB;  // Building block, if have one
} APIN, *PAPIN;

// post information, used to keep track of what posts go where
typedef struct {
   fp         fLoc;       // location from 0.0 to .99999 where the post goes
   BOOL           fRight;     // TRUE if balustrade is to the right (postive fLoc)
   BOOL           fLeft;      // TRUE if the balustrade is to the left (negetive fLoc)
   DWORD          dwReason;   // reason for being. 0=corner, 1=cutout, 2=make up distance (big), 3=make up disnace (small)
   BOOL           fWantFullHeight;  // if TRUE, try to make the posts go up to the roof, else, just balustrade height
   PCColumn       pColumn;    // column used to draw the post
} BPOSTINFO, *PBPOSTINFO;

// noodle information - used for handrails
typedef struct {
   WORD           wColor;     // color type to use. 0 for handrail, 1 for top/bottom rail, 2 for brace, 3 horizontal, 4 vertical, 5 flat panel
   PCNoodle       pNoodle;    // noodle
   PCColumn       pColumn;    // column - used for vertical bits so can have point on top
   PCPoint        pPanel;     // points to an array of 4 points if allocated for a panel. Must be freed()
} BNOODLEINFO, *PBNOODLEINFO;

typedef struct {
   TEXTUREPOINT   tp;      // where start and stop
   BOOL           fAuto;      // set to true if automatic cutout, FALSE if user
} BALCUTOUT, *PBALCUTOUT;

// BUGFIX - Changed many references of m_psBottom and m_psTop to m_psOrigBottom and m_psOrigTop

/***********************************************************************************
CBalustrade::DeterminePostLocations - Call this after any cutouts have been made.
It looks at:
   - Cutouts (m_lBALCUTOUT)
   - Spline inflections
   - Distances between above posts
And determines where the posts go. Fills in m_lPosts.
*/
BOOL CBalustrade::DeterminePostLocations (void)
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
   PBALCUTOUT pCut;
   DWORD dwCutNum;
   pCut = (PBALCUTOUT) m_lBALCUTOUT.Get(0);
   dwCutNum = m_lBALCUTOUT.Num();

   // assume posts at inflection points
   DWORD dwNum, dwDivide, j;
   dwNum = m_psBottom->OrigNumPointsGet();
   dwDivide = dwNum - (m_psBottom->LoopedGet() ? 0 : 1);
   BPOSTINFO bpi;
   PBPOSTINFO pbpi2;
   memset (&bpi, 0, sizeof(bpi));
   m_lPosts.Required (dwNum);
   for (i = 0; i < dwNum; i++) {
      bpi.fLoc = (fp)i / (fp)dwDivide;
      bpi.fRight = bpi.fLeft = TRUE;
      bpi.dwReason = 0;
      bpi.fWantFullHeight = TRUE;   // want full height on the corners

      // don't have anything to the left or right if at the ends
      if (!i && !m_psBottom->LoopedGet())
         bpi.fLeft = FALSE;
      if ((i+1 == dwNum) && !m_psBottom->LoopedGet())
         bpi.fRight = FALSE;

      // may not want to add this if it's in a cutout region
      for (j = 0; j < dwCutNum; j++) {
         if ((bpi.fLoc >= pCut[j].tp.h - 2*CLOSE) && (bpi.fLoc <= pCut[j].tp.v + 2*CLOSE))
            break;
      }
      if (j < dwCutNum)
         continue;   // in a cutout

      // NOTE: May want to excluse posts in semicircles? between circlenext and circleprev,
      // and ellipsenext and ellipseprev?

      // else, add it
      m_lPosts.Add (&bpi);
   }

   // add posts at the edge of the cutouts
   DWORD k;
   m_lPosts.Required (dwCutNum*2);
   for (i = 0; i < dwCutNum; i++) {
      for (k = 0; k < 2; k++) {
         bpi.fLoc = (k ? pCut[i].tp.v : pCut[i].tp.h);
         bpi.dwReason = 1;
         bpi.fRight = (k ? TRUE : FALSE);
         bpi.fLeft = !bpi.fRight;
         bpi.fWantFullHeight = m_fPostWantFullHeightCutout;

         // don't allow 1.0 if looped
         if ((bpi.fLoc >= 1.0) && m_psBottom->LoopedGet())
            continue;

         // make sure that cutouts don't overlap
         for (j = 0; j < dwCutNum; j++) {
            if (j == i)
               continue;
            if ((bpi.fLoc >= pCut[j].tp.h - 2*CLOSE) && (bpi.fLoc <= pCut[j].tp.v + 2*CLOSE))
               break;
            // also test for wrap around
            if ((bpi.fLoc+1.0 >= pCut[j].tp.h - 2*CLOSE) && (bpi.fLoc+1.0 <= pCut[j].tp.v + 2*CLOSE))
               break;
         }
         if (j < dwCutNum)
            continue;   // in a cutout

         // else, add it
         for (j = 0; j < m_lPosts.Num(); j++) {
            pbpi = (PBPOSTINFO) m_lPosts.Get(j);
            if (bpi.fLoc <= pbpi->fLoc) {
               // insert before this
               m_lPosts.Insert (j, &bpi);
               break;
            }
         } // m_lPosts.Num()
         if (j >= m_lPosts.Num())
            m_lPosts.Add (&bpi); // else, not less than any in list so add it

      } // k 0 to 1
   } // cutout num

   // start with big posts first and divide by those, then small posts.
   // Big posts try to go all the way to roof.
   DWORD dwBig;
   for (dwBig = 0; dwBig < 2; dwBig++) {
      fp fMaxDist = (dwBig ? m_fMaxPostDistanceSmall : m_fMaxPostDistanceBig);
      DWORD dwReas;
      dwReas = (dwBig ? 2 : 3);

      // user may specify not to build posts all the way up to the ceiling
      if ((dwBig == 0) && !m_fPostDivideIntoFull)
         continue;

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
            fVal = (fp)((fEnd-fStart) / 10.0 * (fp)k + fStart);
            if (m_psBottom->LoopedGet())
               fVal = (fp)fmod((fp)fVal, (fp)1.0);
            else
               fVal = (fp)min(fVal, 1.0);
            TEXTUREPOINT tp;
            m_psBottom->LocationGet(fVal, &pCur);
            m_psBottom->TextureGet (fVal, &tp);
            pCur.p[2] = tp.h;
#ifdef _DEBUG
            // look for problems
            if (fabs(tp.h) > 1000)
               tp.v = tp.h;
#endif
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
            bpi.fWantFullHeight = dwBig ? FALSE : TRUE;

            if (i+1 < m_lPosts.Num())
               m_lPosts.Insert (i+1, &bpi);
            else
               m_lPosts.Add (&bpi);
         }  // j
      } // i - posts in-between
   } // dwBig

   // create the post objects
   for (i = 0; i < m_lPosts.Num(); i++) {
      pbpi = (PBPOSTINFO) m_lPosts.Get(i);

      // BUGFIX - If flag set to not have columsn then convert them to ordinary posts
      if (m_fNoColumns && pbpi->fWantFullHeight)
         pbpi->fWantFullHeight = FALSE;

      // find out the points for the post
      CPoint pBottom, pTop, pDir;
      TEXTUREPOINT tp;
      // BUGFIX - Doing this to ensure balustrades right when rising
      m_psBottom->LocationGet (pbpi->fLoc, &pBottom);
      m_psBottom->TextureGet (pbpi->fLoc, &tp);
      pBottom.p[2] = tp.h;
      m_psTop->LocationGet (pbpi->fLoc, &pTop);
      m_psTop->TextureGet (pbpi->fLoc, &tp);
      pTop.p[2] = tp.h;
      pDir.Subtract (&pTop, &pBottom);
      pDir.Normalize();
      if (pDir.Length() < .1)
         continue;   // on top of each other

      // figure out the front of the post
      fp fLook, fDirection;
      BOOL fFlip;
      CPoint pNext, pFront;
      fDirection = (fp)CLOSE;
      fFlip = FALSE;
      // if this is a post on a corner, then see which way it orients itself
      // based on which length is longer (to right or left). If both the same
      // then it chooses which is more E/W oriented
      if ((pbpi->dwReason == 0) && (pbpi->fLeft) && (pbpi->fRight)) {
         DWORD dwPoint;
         dwPoint = (DWORD) (pbpi->fLoc * (fp)dwDivide + .5) % dwNum;
         CPoint C, R, L;
         TEXTUREPOINT tp;
         m_psBottom->OrigPointGet (dwPoint, &C);
         m_psBottom->OrigTextureGet (dwPoint, &tp);
         C.p[2] = tp.h;
         m_psBottom->OrigPointGet ((dwPoint+1)%dwNum, &R);
         m_psBottom->OrigTextureGet ((dwPoint+1)%dwNum, &tp);
         R.p[2] = tp.h;
         m_psBottom->OrigPointGet ((dwPoint+dwNum-1)%dwNum, &L);
         m_psBottom->OrigTextureGet ((dwPoint+dwNum-1)%dwNum, &tp);
         L.p[2] = tp.h;
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
      if ((fLook >= 1.0) && !m_psBottom->LoopedGet()) {
         fLook = pbpi->fLoc - fDirection;
         fFlip = !fFlip;
      }
      if ((fLook < 0.0) && !m_psBottom->LoopedGet()) {
         fLook = pbpi->fLoc + fDirection;
         fFlip = !fFlip;
      }
      m_psBottom->LocationGet ((fp)fmod(fLook+1,1), &pNext);
      m_psBottom->TextureGet ((fp)fmod(fLook+1,1), &tp);
      pNext.p[2] = tp.h;
      pNext.Subtract (&pBottom);
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
      fHeight = m_fHeight + m_fPostHeightAboveBalustrade;
      CPoint pRealTop;
      pRealTop.Copy (&pDir);
      pRealTop.Scale (fHeight);
      pRealTop.Add (&pBottom);

      // create the column object
      PCColumn pc;
      pbpi->pColumn = pc = new CColumn;
      if (!pbpi->pColumn)
         continue;

      // set the bottom, top, and front
      pc->StartEndFrontSet (&pBottom, &pRealTop, &pFront);

      BOOL fLong;
      fLong = FALSE;
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

   }  // i, m_lPosts.Num()

   // BUGFIX - go through all the posts and eliminate those that are basically in the same location
   for (i = 0; i+1 < m_lPosts.Num(); i++) {
      pbpi = (PBPOSTINFO) m_lPosts.Get(i);
      CPoint ps1, ps2, pe1, pe2;
      if (!pbpi[0].pColumn || !pbpi[1].pColumn)
         continue;
      pbpi[0].pColumn->StartEndFrontGet (&ps1, &pe1, NULL);
      pbpi[1].pColumn->StartEndFrontGet (&ps2, &pe2, NULL);

      // note: Only eliminated if exactly the same. This way can have two posts
      // superimposed on one another if one is taller than the other.
      // may cause problems if have a cap though. Dont worry about that now
      if (ps1.AreClose (&ps2) && pe1.AreClose(&pe2)) {
         pbpi[0].fRight = pbpi[1].fRight;
         delete pbpi[1].pColumn;
         m_lPosts.Remove (i+1);
         i--;  // so repeat this one
         continue;
      }
   }

   if (!BuildNoodleLocations())
      return FALSE;

   // claim the colors 
   ClaimAllNecessaryColors ();

   return TRUE;
}


/***********************************************************************************
CBalustrade::BuildNoodleLocations - Call after calling DeterminePostLocations()
and this will build all the noodles from the posts and other info
*/
BOOL CBalustrade::BuildNoodleLocations (void)
{
   DWORD i;
   // create the bits in-between

   // clear them
   PBNOODLEINFO pbni;
   for (i = 0; i < m_lNoodles.Num(); i++) {
      pbni = (PBNOODLEINFO) m_lNoodles.Get(i);
      if (pbni->pNoodle)
         delete pbni->pNoodle;
      if (pbni->pColumn)
         delete pbni->pColumn;
      if (pbni->pPanel)
         ESCFREE (pbni->pPanel);
   }
   m_lNoodles.Clear();

   // loop until find a post that has something to right
   PBPOSTINFO pbpi;
   DWORD dwNum;
   pbpi = (PBPOSTINFO) m_lPosts.Get(0);
   dwNum = m_lPosts.Num();
   DWORD dwFirstPost;
   for (dwFirstPost = 0; dwFirstPost < dwNum; dwFirstPost++)
      if (pbpi[dwFirstPost].fRight)
         break;
   if (dwFirstPost >= dwNum)
      return TRUE;   // shouldn happen, but might

   // loop until we return back to the first post
   DWORD dwStart, dwEnd;
   CListFixed lTop, lBottom;
   CPoint pUp, pOffset, pScale;
   pUp.Zero();
   pUp.p[2] = 1;
   BNOODLEINFO bni;
   pScale.Zero();
   BOOL fLooped;
   for (dwStart = dwFirstPost; dwStart < dwFirstPost + dwNum; dwStart = dwEnd+1) {

      // find out how far can go until get to a post with no right
      for (dwEnd = dwStart+1; dwEnd < dwFirstPost+dwNum; dwEnd++) {
         if (!pbpi[dwEnd % dwNum].fRight)
            break;

         // BUGFIX if the posts have the same X,Y then break
         if ((dwEnd+1 < dwFirstPost+dwNum) && pbpi[dwEnd%dwNum].pColumn && pbpi[(dwEnd+1)%dwNum].pColumn) {
            CPoint pPost1, pPost2;
            pbpi[dwEnd%dwNum].pColumn->StartEndFrontGet (&pPost1, NULL);
            pbpi[(dwEnd+1)%dwNum].pColumn->StartEndFrontGet (&pPost2, NULL);
            pPost1.p[2] = pPost2.p[2] = 0;
            if (pPost1.AreClose(&pPost2))
               break;
         }

      }

      // this is  aloop if dwStart == dwEnd%dwNum
      fLooped = (dwStart == (dwEnd % dwNum));

      // use this to get the top and bottom
      PostToCoord (dwStart, dwEnd, &lBottom, &lTop);

      // the handrail?
      for (i = 0; i < 3; i++) if (m_afHorzRailUse[i]) {
         memset (&bni, 0, sizeof(bni));
         bni.wColor = (i == 0) ? 0 : 1;
         bni.pNoodle = NoodleFromCoord (&lBottom, &lTop, i < 2, m_apHorzRailOffset[i].p[2],
            0, lTop.Num() - (fLooped ? 2 : 1), fLooped);
         if (!bni.pNoodle)
            continue;
         bni.pNoodle->BackfaceCullSet (TRUE);
         bni.pNoodle->DrawEndsSet ((m_apHorzRailOffset[i].p[1] != 0.0) && !fLooped);
         bni.pNoodle->FrontVector (&pUp);
         if (m_apHorzRailOffset[i].p[1]) {
            pOffset.Zero();
            pOffset.p[0] = (fp)(m_apHorzRailOffset[i].p[1] * (m_fSwapSides ? -1.0 : 1.0));
            bni.pNoodle->OffsetSet (&pOffset);
         }
         pScale.p[0] = m_apHorzRailSize[i].p[1];
         pScale.p[1] = m_apHorzRailSize[i].p[0];
         bni.pNoodle->ScaleVector (&pScale);
         bni.pNoodle->ShapeDefault (m_adwHorzRailShape[i]);

         m_lNoodles.Add (&bni);
      }

      // automatic horizontals
      if (m_afHorzRailUse[3]) {
         fp fNum, fMin, fMax;
         fMin = m_pHorzRailAuto.p[0];
         fMax = m_pHorzRailAuto.p[1] + m_fHeight;
         fNum = fMax - fMin;
         fp fSpacing;
         fSpacing = m_pHorzRailAuto.p[2] + m_apHorzRailSize[3].p[0];
         if (fSpacing > EPSILON)
            fNum /= fSpacing;   // distance between
         else
            fNum = 1;
         fNum = min(100, fNum);  // so not too many
         DWORD dwNum;
         dwNum = (DWORD) (fNum + 1.9999);
         dwNum = max(2,dwNum);   // always at least 2

         fNum = fMax - fMin;
         for (i = 0; i < dwNum; i++) {
            memset (&bni, 0, sizeof(bni));
            bni.wColor = 3;
            bni.pNoodle = NoodleFromCoord (&lBottom, &lTop, FALSE,
               fMin + (fp) i / (fp)(dwNum-1) * fNum,
               0, lTop.Num() - (fLooped ? 2 : 1), fLooped);
            if (!bni.pNoodle)
               continue;
            bni.pNoodle->BackfaceCullSet (TRUE);
            bni.pNoodle->DrawEndsSet ((m_apHorzRailOffset[3].p[1] != 0.0) && !fLooped);
            bni.pNoodle->FrontVector (&pUp);
            if (m_apHorzRailOffset[3].p[1]) {
               pOffset.Zero();
               pOffset.p[0] = (fp)(m_apHorzRailOffset[3].p[1] * (m_fSwapSides ? -1.0 : 1.0));
               bni.pNoodle->OffsetSet (&pOffset);
            }
            pScale.p[0] = m_apHorzRailSize[3].p[1];
            pScale.p[1] = m_apHorzRailSize[3].p[0];
            bni.pNoodle->ScaleVector (&pScale);
            bni.pNoodle->ShapeDefault (m_adwHorzRailShape[3]);

            m_lNoodles.Add (&bni);
         }
      }

      // vertical
      if (m_fVertUse) {
         DWORD dwPost;
         // do a post at a time
         for (dwPost = 0; dwPost + 8 < lTop.Num(); dwPost += 8) {
            // BUGFIX - If they're the same location then skip this lot
            CPoint p1, p2;
            p1.Copy((PCPoint) lBottom.Get (dwPost));
            p2.Copy((PCPoint) lBottom.Get (dwPost+8));
            p1.p[2] = p2.p[2] = 0;
            if (p1.AreClose (&p2))
               continue;

            // find the distances
            fp fTop, fBottom;
            fTop = LengthFromCoord (&lTop, dwPost, dwPost+8);
            fBottom = LengthFromCoord (&lBottom, dwPost, dwPost+8);

            // how much to indent from left and right
            fp fIndent;
            fIndent = (fp)(m_apPostSize[4].p[0] / 2.0 + m_pVertAuto.p[2] + m_apVertSize[0].p[0]/2.0);
            
            // how many?
            fp fNum, fSpacing;
            fNum = (fp)max(.01, max(fTop, fBottom) - 2 * fIndent);
            fSpacing = m_pVertAuto.p[2] + m_apVertSize[0].p[0];
            if (fSpacing > EPSILON)
               fNum /= fSpacing;   // distance between
            else
               fNum = 1;
            fNum = min(1000, fNum);  // so not too many
            DWORD dwNum;
            dwNum = (DWORD) (fNum + 1.9999);
            dwNum = max(2,dwNum);   // always at least 2

            fp fCurTop, fCurBottom, fDeltaTop, fDeltaBottom;
            fCurTop = fCurBottom = fIndent;
            fDeltaTop = max(0, fTop - 2 * fIndent) / (fp) (dwNum-1);
            fDeltaBottom = max(0, fBottom - 2 * fIndent) / (fp) (dwNum-1);
            for (i = 0; i < dwNum; i++, fCurTop += fDeltaTop, fCurBottom += fDeltaBottom) {
               // get the two top and bottom
               CPoint pTop, pBottom;
               DistanceFromPost (&lTop, dwPost, fCurTop, &pTop);
               DistanceFromPost (&lBottom, dwPost, fCurBottom, &pBottom);

               // need to adjust these. Know that distances is m_fHeight
               CPoint pDir1, pDir2, pDir;
               pDir.Subtract (&pTop, &pBottom);
               pDir1.Copy (&pDir);
               pDir2.Copy (&pDir);
               pDir1.Scale (m_pVertAuto.p[0]);
               pDir2.Scale (m_pVertAuto.p[1]);

               // determine front vector
               CPoint pRight, pFront;
               BOOL fRight;
               fRight = (i+1 < dwNum);
               DistanceFromPost (&lBottom, dwPost, (fp)(fCurBottom + (fRight ? CLOSE : -CLOSE)), &pRight);
               pRight.Subtract (&pBottom);
               pFront.CrossProd (&pDir, &pRight);
               pFront.Normalize();
               if (!fRight)
                  pFront.Scale (-1);
               // move front/back in space accordingly
               if (m_fVertOffset) {
                  pFront.Scale ((fp)(-m_fVertOffset * (m_fSwapSides ? -1.0 : 1.0)));
                  pTop.Add (&pFront);
                  pBottom.Add (&pFront);
               }

               memset (&bni, 0, sizeof(bni));
               bni.wColor = 4;
               bni.pColumn = new CColumn;
               if (!bni.pColumn)
                  continue;
               pTop.Add (&pDir2);
               pBottom.Add (&pDir1);
               bni.pColumn->StartEndFrontSet (&pBottom, &pTop, &pFront);
               bni.pColumn->ShapeSet (m_adwVertShape[0]);
               bni.pColumn->SizeSet (&m_apVertSize[0]);

               // point?
               if (m_fVertUsePoint) {
                  CBASEINFO bi;
                  memset (&bi, 0, sizeof(bi));
                  bi.dwBevelMode = 0;
                  bi.dwShape = m_adwVertShape[1];
                  bi.dwTaper = m_dwVertPointTaper;
                  bi.fCapped = TRUE;
                  bi.fUse = TRUE;
                  bi.pSize.Copy (&m_apVertSize[1]);
                  bni.pColumn->BaseInfoSet (FALSE, &bi);
               }

               m_lNoodles.Add (&bni);
            }  // i over dwNum

         } // over all posts
      }  // m_fVertUse

      // panel
      if (m_fPanelUse) {
         // how much to indent from left and right
         fp fIndent;
         fIndent = (fp)(m_apPostSize[4].p[0] / 2.0);

         DWORD dwPost;
         DWORD dwSides, dwSide;
         dwSides = (m_fPanelThickness != 0.0) ? 2 : 1;
         // do a post at a time
         for (dwPost = 0; dwPost + 8 < lTop.Num(); dwPost += 8) {
            // BUGFIX - If they're the same location then skip this lot
            CPoint p1, p2;
            p1.Copy((PCPoint) lBottom.Get (dwPost));
            p2.Copy((PCPoint) lBottom.Get (dwPost+8));
            p1.p[2] = p2.p[2] = 0;
            if (p1.AreClose (&p2))
               continue;

            for (dwSide = 0; dwSide < dwSides; dwSide++) {
               // get at each of the corners
               CPoint pTop[2], pBottom[2];
               DistanceFromPost (&lBottom, dwPost, m_pPanelInfo.p[2] + fIndent, &pBottom[0]);
               DistanceFromPost (&lBottom, dwPost+8, -(m_pPanelInfo.p[2] + fIndent), &pBottom[1]);
               DistanceFromPost (&lTop, dwPost, m_pPanelInfo.p[2] + fIndent, &pTop[0]);
               DistanceFromPost (&lTop, dwPost+8, -(m_pPanelInfo.p[2] + fIndent), &pTop[1]);

               fp fOffset;
               fOffset = (fp)(m_fPanelOffset * (m_fSwapSides ? -1.0 : 1.0) + (dwSide ? m_fPanelThickness : -m_fPanelThickness)/2.0);

               if (fOffset) {
                  CPoint pRight, pUp, pFront;
                  pRight.Subtract (&pBottom[1], &pBottom[0]);
                  pUp.Subtract (&pTop[0], &pBottom[0]);
                  pFront.CrossProd (&pUp, &pRight);
                  pFront.Normalize();
                  pFront.Scale (-fOffset);
                  pBottom[0].Add (&pFront);
                  pBottom[1].Add (&pFront);
                  pTop[0].Add (&pFront);
                  pTop[1].Add (&pFront);
               }

               // allocate memory for the points
               memset (&bni, 0, sizeof(bni));
               bni.wColor = 5;
               bni.pPanel = (PCPoint) ESCMALLOC(sizeof(CPoint)*4);
               if (!bni.pPanel)
                  continue;
         
               bni.pPanel[0].Average (&pTop[0], &pBottom[0], (m_fHeight + m_pPanelInfo.p[1]) / m_fHeight);
               bni.pPanel[1].Average (&pTop[0], &pBottom[0], m_pPanelInfo.p[0] / m_fHeight);
               bni.pPanel[2].Average (&pTop[1], &pBottom[1], m_pPanelInfo.p[0] / m_fHeight);
               bni.pPanel[3].Average (&pTop[1], &pBottom[1], (m_fHeight + m_pPanelInfo.p[1]) / m_fHeight);

               if (dwSide) {
                  // reverse the order
                  DWORD j;
                  CPoint pt;
                  for (j = 0; j < 2; j++) {
                     pt.Copy (&bni.pPanel[j]);
                     bni.pPanel[j].Copy (&bni.pPanel[3-j]);
                     bni.pPanel[3-j].Copy (&pt);
                  }
               }

               m_lNoodles.Add (&bni);
            } // dwSide
         } // dwPost
      }  // m_fPanelUse

      // bracing
      if (m_dwBrace) {
         // how much to indent from left and right
         fp fIndent;
         fIndent = (fp)(m_apPostSize[4].p[0] / 2.0);

         DWORD dwPost;
         // do a post at a time
         for (dwPost = 0; dwPost + 8 < lTop.Num(); dwPost += 8) {
            // BUGFIX - If they're the same location then skip this lot
            CPoint p1, p2;
            p1.Copy((PCPoint) lBottom.Get (dwPost));
            p2.Copy((PCPoint) lBottom.Get (dwPost+8));
            p1.p[2] = p2.p[2] = 0;
            if (p1.AreClose (&p2))
               continue;

            // get at each of the corners
            CPoint pTop[2], pBottom[2];
            DistanceFromPost (&lBottom, dwPost, fIndent, &pBottom[0]);
            DistanceFromPost (&lBottom, dwPost+8, -fIndent, &pBottom[1]);
            DistanceFromPost (&lTop, dwPost, fIndent, &pTop[0]);
            DistanceFromPost (&lTop, dwPost+8, -fIndent, &pTop[1]);

            // move brace top/bottom
            CPoint pDir, pDir1;
            for (i = 0; i < 2; i++) {
               pDir.Subtract (&pTop[i], &pBottom[i]);
               pDir.Normalize();
               pDir1.Copy (&pDir);
               pDir1.Scale (m_pBraceTB.h);
               pBottom[i].Add (&pDir1);
               pDir1.Copy (&pDir);
               pDir1.Scale (m_pBraceTB.v);
               pTop[i].Add (&pDir1);
            }

            // figure out the front
            CPoint pRight, pUp, pFront, pFront2;
            pRight.Subtract (&pBottom[1], &pBottom[0]);
            pUp.Subtract (&pTop[0], &pBottom[0]);
            pFront.CrossProd (&pUp, &pRight);
            pFront.Normalize();
            pFront2.Copy (&pFront);
            pFront2.Scale ((fp)(-m_fBraceOffset * (m_fSwapSides ? -1.0 : 1.0)));
            pBottom[0].Add (&pFront2);
            pBottom[1].Add (&pFront2);
            pTop[0].Add (&pFront2);
            pTop[1].Add (&pFront2);

            // first slash
            if ((m_dwBrace == 1) || ((m_dwBrace == 2) && ((dwPost/8)%2)) ) {
               memset (&bni, 0, sizeof(bni));
               bni.wColor = 2;
               bni.pNoodle = new CNoodle;
               if (!bni.pNoodle)
                  continue;
               bni.pNoodle->BackfaceCullSet (TRUE);
               bni.pNoodle->DrawEndsSet (TRUE);
               bni.pNoodle->FrontVector (&pFront);
               bni.pNoodle->PathLinear (&pTop[0], &pBottom[1]);
               bni.pNoodle->ScaleVector (&m_pBraceSize);
               bni.pNoodle->ShapeDefault (m_dwBraceShape);
               m_lNoodles.Add (&bni);
            }
            // second slash
            if ((m_dwBrace == 1) || ((m_dwBrace == 2) && !((dwPost/8)%2))) {
               memset (&bni, 0, sizeof(bni));
               bni.wColor = 2;
               bni.pNoodle = new CNoodle;
               if (!bni.pNoodle)
                  continue;
               bni.pNoodle->BackfaceCullSet (TRUE);
               bni.pNoodle->DrawEndsSet (TRUE);
               bni.pNoodle->FrontVector (&pFront);
               bni.pNoodle->PathLinear (&pBottom[0], &pTop[1]);
               bni.pNoodle->ScaleVector (&m_pBraceSize);
               bni.pNoodle->ShapeDefault (m_dwBraceShape);
               m_lNoodles.Add (&bni);
            }
            // v
            if ((m_dwBrace == 3) || (m_dwBrace == 4)) {
               CPoint pHalfTop, pHalfBottom;
               PCPoint paTop, paBottom;
               paTop = (m_dwBrace == 3) ? pTop : pBottom;
               paBottom = (m_dwBrace == 3) ? pBottom : pTop;
               pHalfTop.Average (&paTop[0], &paTop[1]);
               pHalfBottom.Average (&paBottom[0], &paBottom[1]);

               memset (&bni, 0, sizeof(bni));
               bni.wColor = 2;
               bni.pNoodle = new CNoodle;
               if (!bni.pNoodle)
                  continue;
               bni.pNoodle->BackfaceCullSet (TRUE);
               bni.pNoodle->DrawEndsSet (TRUE);
               bni.pNoodle->FrontVector (&pFront);
               bni.pNoodle->PathLinear (&paTop[0], &pHalfBottom);
               bni.pNoodle->ScaleVector (&m_pBraceSize);
               bni.pNoodle->ShapeDefault (m_dwBraceShape);
               m_lNoodles.Add (&bni);

               bni.pNoodle = bni.pNoodle->Clone();
               if (!bni.pNoodle)
                  continue;
               bni.pNoodle->PathLinear (&pHalfBottom, &paTop[1]);
               m_lNoodles.Add (&bni);
            }
         } // dwPost
      }  // m_dwBrace

   }


   return TRUE;
}

/***********************************************************************************
CBalustrade::ExtendPostsToRoof - Looks through all the posts again and extends
them to the roof. Call this after DeterminePostLocations() is called, and after
all the roofs are in place.

inputs
   DWORD       dwNum - Number of surfaces that might be of interest to intersect with
   PCSplineSurface *papss - Pointer to an array of dwNum PCSplineSurface with the surfaces
   PCMatrix    pam - Pointer to an array of dwNum PCMatrix that convert from the spline surface into this space
returns
   BOOL - TRUE if success
*/
void CBalustrade::ExtendPostsToRoof (DWORD dwNum, PCSplineSurface *papss, PCMatrix pam)
{
   // create a matrix which is the inverse of pam
   CMatrix m;
   DWORD i;
   CListFixed lInv;
   lInv.Init (sizeof(CMatrix));
   for (i = 0; i < dwNum; i++) {
      pam[i].Invert4 (&m);
      lInv.Add (&m);
   }
   PCMatrix pamInv;
   pamInv = (PCMatrix) lInv.Get(0);

   // loop through all the posts that we care about
   DWORD dwPost;
   for (dwPost = 0; dwPost < m_lPosts.Num(); dwPost++) {
      PBPOSTINFO pbpi = (PBPOSTINFO) m_lPosts.Get(dwPost);
      if (!pbpi->pColumn || !pbpi->fWantFullHeight)
         continue;

      // look at this one
      CPoint pBottom, pTop, pDir, pAlt;
      TEXTUREPOINT tp;
      m_psBottom->LocationGet (pbpi->fLoc, &pBottom);
      m_psBottom->TextureGet (pbpi->fLoc, &tp);
      pBottom.p[2] = tp.h;
      m_psTop->LocationGet (pbpi->fLoc, &pTop);
      m_psTop->TextureGet (pbpi->fLoc, &tp);
      pTop.p[2] = tp.h;
      fp fGet;
      fGet = (fp)((pbpi->fLoc + CLOSE < 1.0) ? (pbpi->fLoc+CLOSE) : (pbpi->fLoc-CLOSE));
      m_psBottom->LocationGet (fGet, &pAlt);
      m_psBottom->TextureGet (fGet, &tp);
      pAlt.p[2] = tp.h;
      pDir.Subtract (&pTop, &pBottom);
      pDir.Normalize();
      if (pDir.Length() < .5)
         continue;   // on top of each other

      // remember if intersected
      BOOL fIntersected;
      fp fBestDist;
      CPoint pBestNorm;
      fIntersected = FALSE;
      fBestDist = 2;

      // loop through all the splines
      DWORD dwSpline;
      for (dwSpline = 0; dwSpline < dwNum; dwSpline++) {
         // convert this to the spline space
         CPoint pBotSpline, pTopSpline, pDirSpline, pAltSpline;
         pBotSpline.Copy (&pBottom);
         pBotSpline.p[3] = 1;
         pBotSpline.MultiplyLeft (&pamInv[dwSpline]);
         pTopSpline.Copy (&pTop);
         pTopSpline.p[3] = 1;
         pTopSpline.MultiplyLeft (&pamInv[dwSpline]);
         pDirSpline.Subtract (&pTopSpline, &pBotSpline);
         pDirSpline.Normalize();

         // find out where this intersects
         PCSplineSurface pss = papss[dwSpline];

         TEXTUREPOINT tp;
         if (!pss->IntersectLine (&pBotSpline, &pDirSpline, &tp, TRUE, FALSE)) {
            // missed. Might be passing right through middle of two intersecting
            // surfaces, so try slightly off
            pAltSpline.Copy (&pAlt);
            pAltSpline.p[3] = 1;
            pAltSpline.MultiplyLeft (&pamInv[dwSpline]);
            if (!pss->IntersectLine (&pAltSpline, &pDirSpline, &tp, TRUE, FALSE))
               continue;
         }

         // if got here, intersected, so get the info
         CPoint pInter, pNorm;
         if (!pss->HVToInfo (tp.h, tp.v, &pInter, &pNorm))
            continue;

         // see how far the intersection is along the length
         pInter.Subtract (&pBotSpline);
         fp fDist;
         fDist = pInter.DotProd (&pDirSpline);
         if (fDist <= .5)
            continue;   // if less than 0, happened below.
         // Plus, give 50cm extra leeway for roundoff error and thickness of floor

         // remember this?
         if (!fIntersected || (fDist < fBestDist)) {
            fIntersected = TRUE;
            fBestDist = fDist;

            // remember the normal
            CMatrix mTrans;
            mTrans.Copy (&pamInv[dwSpline]);
            mTrans.Transpose();
            pBestNorm.Copy (&pNorm);
            pBestNorm.p[3] = 1;
            pBestNorm.MultiplyLeft (&mTrans);
         }

      }  // dwSpline

      // if we intersected then use that distance, else use the default height
      if (!fIntersected)
         fBestDist = m_fHeight + m_fPostHeightAboveBalustrade;

      // find the real top
      CPoint pRealTop;
      pRealTop.Copy (&pDir);
      pRealTop.Scale (fBestDist);
      pRealTop.Add (&pBottom);

      // set the height
      PCColumn pc;
      pc = pbpi->pColumn;
      pc->StartEndFrontSet (&pBottom, &pRealTop);

      // set the normal
      CBASEINFO bi;
      pc->BaseInfoGet (FALSE, &bi);
      if (fIntersected) {
         bi.dwBevelMode = 2;
         bi.pBevelNorm.Copy (&pBestNorm);
      }
      else
         bi.dwBevelMode = 0;
      // if it's intersected then use a different post if it's not
      BOOL fLong;
      fLong = fIntersected;
      bi.dwShape = m_adwPostShape[fLong ? 2 : 1];
      bi.dwTaper = m_adwPostTaper[fLong ? 2 : 1];
      bi.pSize.Copy (&m_apPostSize[fLong ? 2 : 1]);
      bi.fUse = m_afPostUse[fLong ? 2 : 1];
      bi.fCapped = bi.fUse;
      pc->BaseInfoSet (FALSE, &bi);
      pc->BaseInfoSet (FALSE, &bi);

      // clear bracing that's there
      DWORD dwBrace;
      CBRACEINFO bri;
      memset (&bri, 0, sizeof(bri));
      for (dwBrace = 0; dwBrace < COLUMNBRACES; dwBrace++) {
         pc->BraceInfoSet (dwBrace, &bri);
      }



      // create the bracing. But only do so if it intersects and we want bracing
      if (fIntersected && m_afPostUse[3]) {
         pc->BraceStartSet (m_fBraceBelow);
         fp fBraceHeight;
         fBraceHeight = fBestDist - m_fBraceBelow;

         // find out where this is
         CPoint pBrace;
         pBrace.Copy (&pDir);
         pBrace.Scale (fBraceHeight);
         pBrace.Add (&pBottom);

         // determine the right and left of this point and create a brace there
         for (dwBrace = 0; dwBrace < 2; dwBrace++) {
            fp fLoc;
            fLoc = (fp)(pbpi->fLoc + (dwBrace ? CLOSE : (-CLOSE)));  // dwBrace == 1 then to right
            // it we're not looped then don't brace beyond end
            if (!m_psBottom->LoopedGet()) {
               if ((fLoc < 0.0) || (fLoc > 1.0))
                  continue;
            }
            // do modulo loc
            fLoc = (fp)fmod(fLoc + 1.0, 1.0);

            // if there's nothing to left or right then skip
            if (dwBrace && !pbpi->fRight)
               continue;
            if (!dwBrace && !pbpi->fLeft)
               continue;

            CPoint pBraceDir;
            TEXTUREPOINT tp;
            if (!m_psBottom->LocationGet (fLoc, &pBraceDir))
               continue;
            m_psBottom->TextureGet (fLoc, &tp);
            pBraceDir.p[2] = tp.h;

            // subtract this from start and normalize
            pBraceDir.Subtract (&pBottom);
            pBraceDir.Normalize();

            // add in an equal measure of up so that it's 45 degrees
            pBraceDir.Add (&pDir);
            pBraceDir.Normalize();
            CPoint pBraceEnd;
            pBraceEnd.Add (&pBrace, &pBraceDir);

            // loop through all surfaces and find intersection
            // remember if intersected
            BOOL fIntersected;
            fp fBestDist2;
            CPoint pBestNorm;
            fIntersected = FALSE;
            fBestDist2 = 2;

            // loop through all the splines
            DWORD dwSpline;
            for (dwSpline = 0; dwSpline < dwNum; dwSpline++) {
               // convert this to the spline space
               CPoint pBotSpline, pTopSpline, pDirSpline;
               pBotSpline.Copy (&pBrace);
               pBotSpline.p[3] = 1;
               pBotSpline.MultiplyLeft (&pamInv[dwSpline]);
               pTopSpline.Copy (&pBraceEnd);
               pTopSpline.p[3] = 1;
               pTopSpline.MultiplyLeft (&pamInv[dwSpline]);
               pDirSpline.Subtract (&pTopSpline, &pBotSpline);
               pDirSpline.Normalize();

               // find out where this intersects
               PCSplineSurface pss = papss[dwSpline];

               TEXTUREPOINT tp;
               if (!pss->IntersectLine (&pBotSpline, &pDirSpline, &tp, TRUE, FALSE))
                  continue;

               // if got here, intersected, so get the info
               CPoint pInter, pNorm;
               if (!pss->HVToInfo (tp.h, tp.v, &pInter, &pNorm))
                  continue;

               // see how far the intersection is along the length
               pInter.Subtract (&pBotSpline);
               fp fDist;
               fDist = pInter.DotProd (&pDirSpline);
               if (fDist < m_fBraceBelow/4)
                  continue;   // if less than 0, happened below.
               // Plus, if brace is too short ignore it

               // remember this?
               if (!fIntersected || (fDist < fBestDist2)) {
                  fIntersected = TRUE;
                  fBestDist2 = fDist;

                  // remember the normal
                  CMatrix mTrans;
                  mTrans.Copy (&pamInv[dwSpline]);
                  mTrans.Transpose();
                  pBestNorm.Copy (&pNorm);
                  pBestNorm.p[3] = 1;
                  pBestNorm.MultiplyLeft (&mTrans);
               }

            }  // dwSpline

            // if didn't intersect, or brace ends up being really long
            // then dont bother
            if (!fIntersected || (fBestDist2 > m_fBraceBelow*2))
               continue;

            // else, add brace
            memset (&bri, 0, sizeof(bri));
            bri.dwBevelMode = 2;
            bri.dwShape = m_adwPostShape[3];
            bri.fUse = TRUE;
            bri.pBevelNorm.Copy (&pBestNorm);
            bri.pEnd.Copy (&pBraceDir);
            bri.pEnd.Scale (fBestDist2);
            bri.pEnd.Add (&pBrace);
            bri.pSize.Copy (&m_apPostSize[3]);

            pc->BraceInfoSet (dwBrace, &bri);

         }  // dwBrace
      }  // if bracing


   } // dwPost

}


/***********************************************************************************
CBalustrade::Constructor and destructor */
CBalustrade::CBalustrade (void)
{
   m_pTemplate = NULL;
   memset (m_adwBalColor, 0, sizeof(m_adwBalColor));
   m_psBottom = m_psTop = NULL;
   m_psOrigBottom = m_psOrigTop = NULL;
   m_lBALCUTOUT.Init (sizeof(BALCUTOUT));
   m_lPosts.Init (sizeof(BPOSTINFO));
   m_lNoodles.Init (sizeof (BNOODLEINFO));
   m_fMaxPostDistanceBig = 4.0;
   m_fPostDivideIntoFull = TRUE;
   m_fMaxPostDistanceSmall = 1.5;
   m_fHeight = 1.0;
   m_fPostWantFullHeightCutout = TRUE;
   m_pssControlInter = NULL;
   m_dwDisplayControl = 0;
   m_fForceVert = m_fForceLevel = TRUE;
   m_fHideFirst = m_fHideLast = FALSE;
   m_fNoColumns = FALSE;
   m_fSwapSides = FALSE;
   m_fIndent = FALSE;
   m_fFence = FALSE;

   ParamFromStyle (BS_BALVERTWOOD);
   
}

CBalustrade::~CBalustrade (void)
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
   PBNOODLEINFO pbni;
   for (i = 0; i < m_lNoodles.Num(); i++) {
      pbni = (PBNOODLEINFO) m_lNoodles.Get(i);
      if (pbni->pNoodle)
         delete pbni->pNoodle;
      if (pbni->pColumn)
         delete pbni->pColumn;
      if (pbni->pPanel)
         ESCFREE (pbni->pPanel);
   }

   if (m_psBottom)
      delete m_psBottom;
   if (m_psTop)
      delete m_psTop;
   if (m_psOrigBottom)
      delete m_psOrigBottom;
   if (m_psOrigTop)
      delete m_psOrigTop;
}


/***********************************************************************************
CBalustrade::ParamFromStyle - Given a balustrade style, this sets the parameters.

inputs
   DWORD       dwStyle - BS_XXX
returns
   none
*/
void CBalustrade::ParamFromStyle (DWORD dwStyle)
{
   m_dwBalStyle = dwStyle;
   if (dwStyle == BS_CUSTOM)
      return;  // nothing to do

   DWORD i;
   fp fSize;
   for (i = 0; i < 5; i++) {
      m_adwPostShape[i] = NS_RECTANGLE;
      //m_adwPostShape[i] = NS_CIRCLEFAST;

      switch (i) {
      case 0:  //bottom
      case 1:  //top, if short
      case 2:  //top, if tall
         fSize = (fp).2;
         break;
      case 3:  // brace
         fSize = (fp).075;
         break;
      case 4:  // main
         fSize = (fp).1;
         break;
      }
      m_apPostSize[i].Zero();
      m_apPostSize[i].p[0] = m_apPostSize[i].p[1] = m_apPostSize[i].p[2] = fSize;
      // m_apPostSize[i].p[0] *= 2;
      // dont need to set p[2] for main column and brace but do it anyway
   }
   for (i = 0; i < 4; i++)
      m_afPostUse[i] = FALSE; // TRUE
   for (i = 0; i < 3; i++)
      m_adwPostTaper[i] = CT_HALFTAPER;
   m_fBraceBelow = 0.75; // brace 1m below roof
   m_fPostHeightAboveBalustrade = (fp).1;

   for (i = 0; i < 4; i++) {
      switch (i) {
      case 0:  // handrail
      case 2:  // bottom rail
         m_afHorzRailUse[i] = TRUE;
         break;
      default:
         m_afHorzRailUse[i] = FALSE;
      }

      m_adwHorzRailShape[i] = NS_RECTANGLE;
      m_apHorzRailSize[i].Zero();
      m_apHorzRailSize[i].p[0] = (fp).025;
      m_apHorzRailSize[i].p[1] = (fp).05;
      if (!i)
         m_apHorzRailSize[i].Scale(2.0);  // handrail fp
      if (i == 3)
         m_apHorzRailSize[i].Scale(0.5);  // auto members like stainless steel wire

      m_apHorzRailOffset[i].Zero();
      switch (i) {
      case 1:  // top rail
         m_apHorzRailOffset[i].p[2] = (fp)-.1;
         break;
      case 2:  // bottom
         m_apHorzRailOffset[i].p[2] = (fp).1;
      }
   }
   m_pHorzRailAuto.Zero();
   m_pHorzRailAuto.p[0] = .2;
   m_pHorzRailAuto.p[1] = -.2;
   m_pHorzRailAuto.p[2] = 0.12;

   m_fPanelUse = FALSE;
   m_fPanelOffset = 0;
   m_fPanelThickness = 0;
   m_pPanelInfo.Zero();
   m_pPanelInfo.p[2] = .1;
   m_pPanelInfo.p[0] = .2;
   m_pPanelInfo.p[1] = -.1;

   m_dwBrace = 0;
   m_fBraceOffset = 0;
   m_pBraceSize.Zero();
   m_pBraceSize.p[0] = .1;
   m_pBraceSize.p[1] = .025;
   m_pBraceTB.h = .1;
   m_pBraceTB.v = 0;
   m_dwBraceShape = NS_RECTANGLE;

   m_fVertUse = FALSE;
   m_fVertUsePoint = FALSE;
   m_dwVertPointTaper = CT_SPIKE;
   m_adwVertShape[0] = m_adwVertShape[1] = NS_RECTANGLE;
   m_apVertSize[0].Zero();
   m_apVertSize[0].p[0] = .01;
   m_apVertSize[0].p[1] = .01;
   m_apVertSize[1].Zero();
   m_apVertSize[1].p[0] = m_apVertSize[1].p[1] = .03;
   m_apVertSize[1].p[2] = .1;
   m_fVertOffset = 0;
   m_pVertAuto.Zero();
   m_pVertAuto.p[0] = .1;
   m_pVertAuto.p[1] = 0.0; // up to handrail
   m_pVertAuto.p[2] = .12;

   switch (dwStyle) {
   case BS_BALVERTWOOD: // 2x4 rails, 2x2 verticals
   case BS_BALVERTWOOD2:   // LIKE BALVERTWOOD except posts go above
   case BS_BALVERTWOOD3:   // verticals go all the way to the ground
      m_fPostHeightAboveBalustrade = (dwStyle != BS_BALVERTWOOD2) ? -.025 : .1;
      m_apHorzRailSize[0].p[0] = m_apHorzRailSize[2].p[0] = .05;  // handrail and bottom rail
      m_apHorzRailSize[0].p[1] = m_apHorzRailSize[2].p[1] = .1;
      m_fVertUse = TRUE;
      m_apVertSize[0].p[0] = .05;
      m_apVertSize[0].p[1] = .05;
      if (dwStyle == BS_BALVERTWOOD3) {
         m_pVertAuto.p[0] = 0;
         m_afHorzRailUse[2] = FALSE;
      }
      break;

   case BS_BALVERTLOG:  // log cabin style
      m_adwPostShape[3] = m_adwPostShape[4] = NS_CIRCLEFAST;
      m_apPostSize[3].p[0] = m_apPostSize[3].p[1] = .1; // brace
      m_apPostSize[4].p[0] = m_apPostSize[4].p[1] = .15; // main
      m_afPostUse[3] = TRUE;
      m_fPostHeightAboveBalustrade = 0;
      m_adwHorzRailShape[0] = m_adwHorzRailShape[2] = NS_CIRCLEFAST;
      m_apHorzRailSize[0].p[0] = m_apHorzRailSize[0].p[1] = .1;   // handrail
      m_apHorzRailSize[2].p[0] = m_apHorzRailSize[2].p[1] = .075;  // bottom rail
      m_fVertUse = TRUE;
      m_adwVertShape[0] = NS_CIRCLEFAST;
      m_apVertSize[0].p[0] = m_apVertSize[0].p[1] = .075;   // vertcials

      break;

   case BS_BALVERTWROUGHTIRON:
      m_fPostHeightAboveBalustrade = .1;
      m_apHorzRailSize[0].p[0] = .025;
      m_apHorzRailSize[0].p[1] = .05;
      m_apHorzRailSize[2].p[0] = m_apHorzRailSize[2].p[1] = .025;
      m_fVertUse = TRUE;
      m_apVertSize[0].p[0] = m_apVertSize[0].p[1] = 0.0125;
      break;

   case BS_BALVERTSTEEL:
      m_fPostHeightAboveBalustrade = -.1;
      m_apPostSize[4].p[0] = m_apPostSize[4].p[1] = .075;   // standard steel
      m_adwHorzRailShape[0] = NS_CIRCLEFAST;
      m_afHorzRailUse[1] = TRUE;
      m_apHorzRailSize[0].p[0] = m_apHorzRailSize[0].p[1] = .05;
      m_apHorzRailSize[1].p[0] = m_apHorzRailSize[1].p[1] = .025;
      m_apHorzRailSize[2].p[0] = m_apHorzRailSize[2].p[1] = .025;
      m_fVertUse = TRUE;
      m_adwVertShape[0] = NS_CIRCLEFAST;
      m_pVertAuto.p[1] = -.1;
      m_apVertSize[0].p[0] = m_apVertSize[0].p[1] = 0.0125;
      break;

   case BS_BALHORZPANELS:  // horizontal, thick panels
   case BS_BALHORZWOOD: // horozontal with small bits of wood
      m_apPostSize[4].p[0] = m_apPostSize[4].p[1] = .075;
      m_fPostHeightAboveBalustrade = -.025;
      m_apHorzRailSize[0].p[0] = .05;
      m_apHorzRailSize[0].p[1] = .1;
      m_afHorzRailUse[2] = FALSE;
      m_afHorzRailUse[3] = TRUE;
      m_apHorzRailSize[3].p[1] = .025; // 1" thick
      m_apHorzRailSize[3].p[0] = (dwStyle == BS_BALHORZWOOD) ? .05 : .20;  // 9"
      m_apHorzRailOffset[3].p[1] = m_apPostSize[4].p[1] / 2.0; // so on inside
      m_pHorzRailAuto.p[0] = .1 + m_apHorzRailSize[3].p[0]/2.0;
      m_pHorzRailAuto.p[1] = - (m_pHorzRailAuto.p[2] + m_apHorzRailSize[3].p[0]/2.0);
      break;

   case BS_BALHORZWIRE:
   case BS_BALHORZWIRERAIL:
      m_fPostHeightAboveBalustrade = (dwStyle == BS_BALHORZWIRERAIL) ? 0 : .1;
      m_apPostSize[4].p[0] = m_apPostSize[4].p[1] = .075;   // standard steel
      m_adwHorzRailShape[0] = NS_CIRCLEFAST;
      m_afHorzRailUse[0] = (dwStyle == BS_BALHORZWIRERAIL);
      m_afHorzRailUse[2] = FALSE;
      m_apHorzRailSize[0].p[0] = m_apHorzRailSize[0].p[1] = .05;
      m_apHorzRailOffset[0].p[1] = .1; // 10 cm in from center of posts
      m_afHorzRailUse[3] = TRUE;
      m_apHorzRailSize[3].p[1] = .005; // 5mm thick
      m_apHorzRailSize[3].p[0] = .005;
      m_pHorzRailAuto.p[0] = .1;
      m_pHorzRailAuto.p[1] = (dwStyle == BS_BALHORZWIRERAIL) ? -.1 : 0;
      break;

   case BS_BALHORZPOLE:
      m_fPostHeightAboveBalustrade = -.025;
      m_apPostSize[4].p[0] = m_apPostSize[4].p[1] = .075;   // standard steel
      m_afHorzRailUse[0] = FALSE;
      m_afHorzRailUse[2] = FALSE;
      m_afHorzRailUse[3] = TRUE;
      m_adwHorzRailShape[3] = NS_CIRCLEFAST;
      m_apHorzRailSize[3].p[1] = .05; // 5mm thick
      m_apHorzRailSize[3].p[0] = .05;
      m_pHorzRailAuto.p[0] = .1;
      m_pHorzRailAuto.p[1] = 0;
      break;

   case BS_BALPANEL:
      m_fPostHeightAboveBalustrade = 0;
      m_apPostSize[4].p[0] = m_apPostSize[4].p[1] = .075;   // standard steel
      m_apHorzRailSize[0].p[0] = .05;  // handrail and bottom rail
      m_apHorzRailSize[0].p[1] = .1;
      m_apHorzRailSize[2].p[0] = m_apHorzRailSize[2].p[1] = .05;
      m_fPanelUse = TRUE;
      m_pPanelInfo.p[0] = .125;
      m_pPanelInfo.p[1] = -.125;
      m_pPanelInfo.p[2] = 0.025;
      break;

   case BS_BALPANELSOLID:
      m_fPostHeightAboveBalustrade = 0;
      m_apPostSize[4].p[0] = m_apPostSize[4].p[1] = .1;   // 4x4
      m_apHorzRailSize[0].p[0] = .025;  // 1"
      m_apHorzRailSize[0].p[1] = .15;  // 6"
      m_afHorzRailUse[2] = FALSE;
      m_fPanelUse = TRUE;
      m_pPanelInfo.p[0] = 0;
      m_pPanelInfo.p[1] = 0;
      m_pPanelInfo.p[2] = 0;
      m_fPanelThickness = .075;
      break;

   case BS_BALOPEN: // 2x4 rails, no verticlas
   case BS_BALOPENMIDDLE:   //with middle rail
      m_fPostHeightAboveBalustrade = -.025;
      m_apHorzRailSize[0].p[0] = m_apHorzRailSize[2].p[0] = .05;  // handrail and bottom rail
      m_apHorzRailSize[0].p[1] = m_apHorzRailSize[2].p[1] = .1;
      m_apHorzRailOffset[2].p[2] = m_fHeight / 2;
      m_afHorzRailUse[2] = (dwStyle == BS_BALOPENMIDDLE);
      break;

   case BS_BALOPENPOLE: // metal poles
      m_fPostHeightAboveBalustrade = -.025;
      m_apPostSize[4].p[0] = m_apPostSize[4].p[1] = .05;
      m_adwPostShape[4] = NS_CIRCLEFAST;
      m_adwHorzRailShape[0] = NS_CIRCLEFAST;
      m_apHorzRailSize[0].p[0] = m_apHorzRailSize[0].p[1] = .05;
      m_afHorzRailUse[2] = FALSE;
      break;

   case BS_BALBRACEX: // 2x4 rails, X brace in center
      m_fPostHeightAboveBalustrade = .1;
      m_apHorzRailSize[0].p[0] = m_apHorzRailSize[2].p[0] = .05;  // handrail and bottom rail
      m_apHorzRailSize[0].p[1] = m_apHorzRailSize[2].p[1] = .1;
      m_dwBrace = 1;
      break;

   case BS_BALFANCYGREEK:
      m_adwPostShape[4] = NS_CIRCLEFAST;
      m_apPostSize[0].p[0] = m_apPostSize[0].p[1] = .3;
      m_apPostSize[0].p[2] = .10;
      m_apPostSize[1].Copy (&m_apPostSize[0]);
      m_apPostSize[2].Copy (&m_apPostSize[0]);
      m_afPostUse[0] = m_afPostUse[1] = m_afPostUse[2] = TRUE;
      m_apPostSize[4].p[0] = m_apPostSize[4].p[1] = .2;
      m_fPostHeightAboveBalustrade = 0;
      m_afHorzRailUse[2] = m_afHorzRailUse[0] = FALSE;
      break;
   case BS_BALFANCYGREEK2:
      m_adwPostShape[4] = NS_CIRCLEFAST;
      m_apPostSize[0].p[0] = m_apPostSize[0].p[1] = .2;
      m_apPostSize[0].p[2] = .075;
      m_apPostSize[2].Copy (&m_apPostSize[0]);
      m_apPostSize[1].p[0] = m_apPostSize[1].p[1] = .2;
      m_apPostSize[1].p[2] = .2;
      m_afPostUse[0] = m_afPostUse[1] = m_afPostUse[2] = TRUE;
      m_apPostSize[4].p[0] = m_apPostSize[4].p[1] = .15;
      m_adwPostShape[1] = NS_CIRCLEFAST;
      m_adwPostTaper[1] = CT_SPHERE;
      m_fPostHeightAboveBalustrade = .3;
      m_apHorzRailSize[0].p[0] = m_apHorzRailSize[2].p[0] = .05;  // handrail and bottom rail
      m_apHorzRailSize[0].p[1] = m_apHorzRailSize[2].p[1] = .1;
      m_fVertUse = TRUE;
      m_apVertSize[0].p[0] = .05;
      m_apVertSize[0].p[1] = .05;
      break;
   case BS_BALFANCYWOOD:   // LIKE BALVERTWOOD except posts go above
      m_fPostHeightAboveBalustrade = .15;
      m_apPostSize[0].p[0] = m_apPostSize[0].p[1] = .2;
      m_apPostSize[0].p[2] = .05;
      m_apPostSize[2].Copy (&m_apPostSize[0]);
      m_apPostSize[1].p[0] = m_apPostSize[1].p[1] = .15;
      m_apPostSize[1].p[2] = .15;
      m_adwPostTaper[1] = CT_DIAMOND;
      m_afPostUse[0] = m_afPostUse[1] = m_afPostUse[2] = TRUE;

      m_apHorzRailSize[0].p[0] = m_apHorzRailSize[2].p[0] = .05;  // handrail and bottom rail
      m_apHorzRailSize[0].p[1] = m_apHorzRailSize[2].p[1] = .1;
      m_fVertUse = TRUE;
      m_apVertSize[0].p[0] = .05;
      m_apVertSize[0].p[1] = .05;
      break;

   case BS_FENCEVERTPICKET:
      m_fPostHeightAboveBalustrade = 0.0;
      m_apPostSize[1].p[0] = m_apPostSize[1].p[1] = .005;
      m_apPostSize[1].p[2] = .05;
      m_adwPostTaper[1] = CT_SPIKE2;
      m_afPostUse[1] = TRUE;
      m_apHorzRailSize[0].p[0] = m_apHorzRailSize[2].p[0] = .05;
      m_apHorzRailSize[0].p[1] = m_apHorzRailSize[2].p[1] = .1;
      m_apHorzRailOffset[0].p[2] = -.2;
      m_apHorzRailOffset[2].p[2] = .2;
      m_fVertUse = TRUE;
      m_fVertUsePoint = TRUE;
      m_dwVertPointTaper = CT_SPIKE2;
      m_apVertSize[0].p[0] = .1;
      m_apVertSize[0].p[1] = .025;
      m_apVertSize[1].p[0] = .01;
      m_apVertSize[1].p[1] = .025;
      m_apVertSize[1].p[2] = .05;
      m_fVertOffset=-m_apHorzRailSize[0].p[1]/2.0;
      m_pVertAuto.p[2] = .05;
      m_dwBrace = 2;
      m_pBraceSize.p[0] = .05;
      m_pBraceSize.p[1] = .1;
      m_pBraceTB.h = .2;
      m_pBraceTB.v = -.2;
      break;

   case BS_FENCEVERTPICKETSMALL:
      m_fPostHeightAboveBalustrade = 0.0;
      m_apHorzRailSize[0].p[0] = m_apHorzRailSize[2].p[0] = .05;
      m_apHorzRailSize[0].p[1] = m_apHorzRailSize[2].p[1] = .1;
      m_apHorzRailOffset[0].p[2] = -.2;
      m_apHorzRailOffset[2].p[2] = .2;
      m_fVertUse = TRUE;
      m_fVertUsePoint = TRUE;
      m_dwVertPointTaper = CT_SPIKE2;
      m_apVertSize[0].p[0] = .05;
      m_apVertSize[0].p[1] = .025;
      m_apVertSize[1].p[0] = .005;
      m_apVertSize[1].p[1] = .025;
      m_apVertSize[1].p[2] = .05;
      m_fVertOffset=-m_apHorzRailSize[0].p[1]/2.0;
      m_pVertAuto.p[2] = .01;
      m_dwBrace = 2;
      m_pBraceSize.p[0] = .05;
      m_pBraceSize.p[1] = .1;
      m_pBraceTB.h = .2;
      m_pBraceTB.v = -.2;
      break;

   case BS_FENCEVERTSTEEL:
      m_fPostHeightAboveBalustrade = .1;
      m_apPostSize[4].p[0] = m_apPostSize[4].p[1] = .075;   // standard steel
      m_apHorzRailSize[0].p[0] = m_apHorzRailSize[0].p[1] = .025;
      m_apHorzRailSize[2].p[0] = m_apHorzRailSize[2].p[1] = .025;
      m_apHorzRailOffset[0].p[2] = 0;
      m_apHorzRailOffset[2].p[2] = .1;
      m_fVertUse = TRUE;
      m_pVertAuto.p[1] = 0;
      m_pVertAuto.p[0] = .1;
      m_apVertSize[0].p[0] = m_apVertSize[0].p[1] = 0.0125;
      break;

   case BS_FENCEVERTWROUGHTIRON:
      m_fPostHeightAboveBalustrade = .2;
      m_apPostSize[1].p[0] = m_apPostSize[1].p[1] = .005;
      m_apPostSize[1].p[2] = .2;
      m_adwPostTaper[1] = CT_SPIKE2;
      m_afPostUse[1] = TRUE;
      m_apPostSize[4].p[0] = m_apPostSize[4].p[1] = .075;   // standard steel
      m_apHorzRailSize[0].p[0] = m_apHorzRailSize[0].p[1] = .025;
      m_apHorzRailSize[2].p[0] = m_apHorzRailSize[2].p[1] = .025;
      m_apHorzRailOffset[0].p[2] = -.2;
      m_apHorzRailOffset[2].p[2] = .2;
      m_fVertUse = TRUE;
      m_pVertAuto.p[1] = 0;
      m_pVertAuto.p[0] = .1;
      m_apVertSize[0].p[0] = m_apVertSize[0].p[1] = 0.0125;
      m_apVertSize[1].p[0] = m_apVertSize[1].p[1] = 0.025;
      m_apVertSize[1].p[2] = .1;
      m_fVertUsePoint = TRUE;
      m_dwVertPointTaper = CT_SPIKE;
      break;

   case BS_FENCEVERTPANEL:
      m_fPostHeightAboveBalustrade = 0.0;
      m_apHorzRailSize[0].p[0] = m_apHorzRailSize[2].p[0] = .05;
      m_apHorzRailSize[0].p[1] = m_apHorzRailSize[2].p[1] = .1;
      m_apHorzRailOffset[0].p[2] = -.2;
      m_apHorzRailOffset[2].p[2] = .2;
      m_fVertUse = TRUE;
      m_apVertSize[0].p[0] = .15;
      m_apVertSize[0].p[1] = .025;
      m_fVertOffset=-m_apHorzRailSize[0].p[1]/2.0;
      m_pVertAuto.p[2] = .025;
      m_dwBrace = 2;
      m_pBraceSize.p[0] = .05;
      m_pBraceSize.p[1] = .1;
      m_pBraceTB.h = .2;
      m_pBraceTB.v = -.2;
      break;

   case BS_FENCEHORZLOG:
      m_adwPostShape[4] = NS_CIRCLEFAST;
      m_apPostSize[4].p[0] = m_apPostSize[4].p[1] = .1;
      m_fPostHeightAboveBalustrade = .2;
      m_adwHorzRailShape[0] = m_adwHorzRailShape[1] = m_adwHorzRailShape[2] = NS_CIRCLEFAST;
      m_afHorzRailUse[0] = m_afHorzRailUse[1] = m_afHorzRailUse[2] = TRUE;
      m_apHorzRailSize[0].p[0] = m_apHorzRailSize[0].p[1] = .05;
      m_apHorzRailSize[1].Copy (&m_apHorzRailSize[0]);
      m_apHorzRailSize[2].Copy (&m_apHorzRailSize[0]);
      m_apHorzRailOffset[0].p[2] = 0;
      m_apHorzRailOffset[1].p[2] = (m_fHeight+.2)/2.0 - m_fHeight;
      m_apHorzRailOffset[2].p[2] = .2;
      m_dwBrace = 2;
      m_fBraceOffset = .025;
      m_pBraceSize.p[0] = m_pBraceSize.p[1] = .05;
      m_pBraceTB.h = 0;
      m_pBraceTB.v = .2;
      m_dwBraceShape = NS_CIRCLEFAST;
      break;

   case BS_FENCEHORZSTICK:
      m_adwPostShape[4] = NS_CIRCLEFAST;
      m_apPostSize[4].p[0] = m_apPostSize[4].p[1] = .1;
      m_fPostHeightAboveBalustrade = .2;
      m_adwHorzRailShape[3] = NS_CIRCLEFAST;
      m_afHorzRailUse[0] = m_afHorzRailUse[1] = m_afHorzRailUse[2] = FALSE;
      m_afHorzRailUse[3] = TRUE;
      m_apHorzRailOffset[3].p[1] = -.05;
      m_apHorzRailSize[3].p[0] = m_apHorzRailSize[3].p[1] = .05;
      m_pHorzRailAuto.p[0] = .2;
      m_pHorzRailAuto.p[1] = 0;
      m_pHorzRailAuto.p[2] = .2;
      break;

   case BS_FENCEHORZPANELS: // horozontal with panels
      m_fPostHeightAboveBalustrade = .2;
      m_apPostSize[4].p[0] = m_apPostSize[4].p[1] = .1;
      m_afHorzRailUse[0] = m_afHorzRailUse[2] = FALSE;
      m_afHorzRailUse[3] = TRUE;
      m_apHorzRailSize[3].p[1] = .025; // 1" thick
      m_apHorzRailSize[3].p[0] = .20;  // 9"
      m_apHorzRailOffset[3].p[1] = -m_apPostSize[4].p[1] / 2.0; // so on outside
      m_pHorzRailAuto.p[2] = .25;
      m_pHorzRailAuto.p[0] = .1 + m_apHorzRailSize[3].p[0]/2.0;
      m_pHorzRailAuto.p[1] = - (m_apHorzRailSize[3].p[0]/2.0);
      break;

   case BS_FENCEHORZWIRE:
      m_fPostHeightAboveBalustrade = .1;
      m_apPostSize[4].p[0] = m_apPostSize[4].p[1] = .05;   // standard steel
      m_afHorzRailUse[0] = FALSE;
      m_afHorzRailUse[2] = FALSE;
      m_afHorzRailUse[3] = TRUE;
      m_apHorzRailSize[3].p[1] = .005; // 5mm thick
      m_apHorzRailSize[3].p[0] = .005;
      m_pHorzRailAuto.p[0] = .1;
      m_pHorzRailAuto.p[1] = 0;
      m_pHorzRailAuto.p[2] = .25;
      break;

   case BS_FENCEBRACEX:
      m_fPostHeightAboveBalustrade = 0.2;
      m_apHorzRailSize[0].p[0] = m_apHorzRailSize[2].p[0] = .05;
      m_apHorzRailSize[0].p[1] = m_apHorzRailSize[2].p[1] = .1;
      m_apHorzRailOffset[0].p[2] = 0;
      m_apHorzRailOffset[2].p[2] = .2;
      m_dwBrace = 1;
      m_pBraceSize.p[0] = .05;
      m_pBraceSize.p[1] = .1;
      m_pBraceTB.h = .2;
      m_pBraceTB.v = 0;
      break;

   case BS_FENCEPANELSOLID:
      m_fPostHeightAboveBalustrade = .2;
      m_apPostSize[4].p[0] = m_apPostSize[4].p[1] = .3;
      m_apPostSize[1].p[0] = m_apPostSize[1].p[1] = .35;
      m_apPostSize[1].p[2] = .1;
      m_adwPostTaper[1] = CT_CAPPED;
      m_afPostUse[1] = TRUE;
      m_apHorzRailSize[0].p[0] = .05;  // 1"
      m_apHorzRailSize[0].p[1] = .25;  // 6"
      m_afHorzRailUse[2] = FALSE;
      m_fPanelUse = TRUE;
      m_pPanelInfo.p[0] = 0;
      m_pPanelInfo.p[1] = 0;
      m_pPanelInfo.p[2] = 0;
      m_fPanelThickness = .2;
      break;

   case BS_FENCEPANELPOLE:
      m_apPostSize[4].p[0] = m_apPostSize[4].p[1] = .05;   // standard steel
      m_adwPostShape[4] = NS_CIRCLEFAST;
      m_fPostHeightAboveBalustrade = 0;
      m_apHorzRailSize[0].p[0] = .05;
      m_apHorzRailSize[0].p[1] = .05;
      m_adwHorzRailShape[0] = NS_CIRCLEFAST;
      m_afHorzRailUse[2] = FALSE;
      m_fPanelUse = TRUE;
      m_pPanelInfo.p[0] = 0;
      m_pPanelInfo.p[1] = 0;
      m_pPanelInfo.p[2] = 0;
      m_fPanelThickness = 0;
      break;

   }
}



/*************************************************************************************
CBalustrade::InitButDontCreate - Initializes the object half way, but doesn't
actually create surfaces - which is ultimately causing problems in CObjectBuildBlock.
Basically, this just remembers the template and dwType and that's it.
*/
BOOL CBalustrade::InitButDontCreate (DWORD dwRenderShard, DWORD dwType, PCObjectTemplate pTemplate)
{
   m_dwType = dwType;
   m_pTemplate = pTemplate;

   // get information from default world style
   DWORD dwExternalWalls;
   PWSTR psz;
   PCWorldSocket pWorld = WorldGet(dwRenderShard, NULL);
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
      dwStyle = BS_BALVERTSTEEL;
      break;
   case 5:  // stone
      dwStyle = BS_BALFANCYGREEK2;
      break;
   case 8:  // log
      dwStyle = BS_BALVERTLOG;
      break;
   default:
      dwStyle = BS_BALVERTWOOD;
      break;
   }
   else
      dwStyle = LOWORD(dwType);
   ParamFromStyle (dwStyle);

   return TRUE;
}


/***********************************************************************************
CBalustrade::Init - Initialize the ballustrade object.

NOTE: This assumes that the balustrade information (such as height, width, m_fFence, and spacing)
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
               calling CSpline::Expand) so that the balustrade doesn't go over
               the floor.
returns
   BOOL - TRUE if successful
*/
BOOL CBalustrade::Init (DWORD dwRenderShard, DWORD dwType, PCObjectTemplate pTemplate, PCSpline pBottom, PCSpline pTop, BOOL fIndent)
{
   InitButDontCreate (dwRenderShard, dwType, pTemplate);

   m_fIndent = fIndent;

   return NewSplines (pBottom, pTop);
}

/***********************************************************************************
CBalustrade::NewSplines - Change it to use new splines

NOTE: This assumes that the balustrade information (such as height, width, and spacing)
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
BOOL CBalustrade::NewSplines (PCSpline pBottom, PCSpline pTop)
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
      DWORD i;
      for (i = 0; i < 5; i++) {
         if (i == 3)
            continue;   // brace - don't care
         if ((i < 3) && !m_afPostUse[i])
            continue;   // for top and bottom, might not be showing, so ignore
         fWidth = max(fWidth, m_apPostSize[i].p[0]);
         fWidth = max(fWidth, m_apPostSize[i].p[1]);
      }
   }

   // BUGFIX - Because curved splines used think out of the XY planes, and
   // balustrades only think in XY (with Z being offset up), need to pull
   // the Z members out into a TEXTUREPOINT for m_psBottom and m_psTop
   DWORD dws;
   for (dws = 0; dws < 2; dws++) {
      PCSpline ps = (dws ? pTop : pBottom);
      CSpline ns;

      DWORD dwNum;
      dwNum = ps->OrigNumPointsGet();
      CMem memPoints, memCurve, memTexture;
      if (!memPoints.Required(dwNum * sizeof(CPoint)) || !memCurve.Required(dwNum * sizeof(DWORD)) ||
         !memTexture.Required((dwNum+1) * sizeof(TEXTUREPOINT)))
         return FALSE;
      PCPoint pPoint;
      PTEXTUREPOINT pt;
      DWORD *pdw;
      pPoint = (PCPoint) memPoints.p;
      pt = (PTEXTUREPOINT) memTexture.p;
      pdw = (DWORD*) memCurve.p;

      DWORD k;
      for (k = 0; k < dwNum; k++) {
         ps->OrigSegCurveGet (k, pdw+k);
         ps->OrigPointGet (k, pPoint+k);
         pt[k].h = pPoint[k].p[2];
         pt[k].v = 0;
         pPoint[k].p[2] = 0;
      }
      pt[dwNum] = pt[0];   // just in case it's looped


      DWORD dwMax, dwMin;
      fp fDetail;
      ps->DivideGet (&dwMin, &dwMax, &fDetail);

      ns.Init (ps->LoopedGet(), dwNum, pPoint, pt, pdw, dwMin, dwMax, fDetail);

      // clone and store away new splines, but they should be internal by fWidth/2
      if (dws)
         m_psTop = ns.Expand (-fWidth/2);
      else
         m_psBottom = ns.Expand (-fWidth / 2);
   }

   if (!m_psBottom || !m_psTop || !m_psOrigBottom || !m_psOrigTop)
      return FALSE;

   // figure out where posts go
   DeterminePostLocations();

   return TRUE;
}


static PWSTR gpszOrigBottom = L"OrigBottom";
static PWSTR gpszOrigTop = L"OrigTop";
static PWSTR gpszBottom = L"Bottom";
static PWSTR gpszTop = L"Top";
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
static PWSTR gpszPostHeightAboveBalustrade = L"HgtAboveBal";
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
static PWSTR gpszFence = L"Fence";
static PWSTR gpszSwapSides = L"SwapSides";
static PWSTR gpszAuto = L"Auto%d";
static PWSTR gpszNoColumns = L"NoColumns";

/***********************************************************************************
CBalustrade::MMLTo - Creates a MML node (that must be freed by the caller) with
information describing the spline.

inputs
   none
returns
   PCMMLNode2 - node
*/
PCMMLNode2 CBalustrade::MMLTo (void)
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

   // values
   MMLValueSet (pNode, gpszDisplayControl, (int) m_dwDisplayControl);
   MMLValueSet (pNode, gpszIndent, (int)m_fIndent);
   MMLValueSet (pNode, gpszHeight, m_fHeight);
   MMLValueSet (pNode, gpszFence, (int)m_fFence);
   MMLValueSet (pNode, gpszForceVert, (int)m_fForceVert);
   MMLValueSet (pNode, gpszForceLevel, (int)m_fForceLevel);
   MMLValueSet (pNode, gpszHideFirst, (int)m_fHideFirst);
   MMLValueSet (pNode, gpszHideLast, (int)m_fHideLast);
   MMLValueSet (pNode, gpszNoColumns, (int)m_fNoColumns);
   MMLValueSet (pNode, gpszSwapSides, (int)m_fSwapSides);

   DWORD i;
   for (i = 0; i < m_lBALCUTOUT.Num(); i++) {
      PBALCUTOUT pCut = (PBALCUTOUT) m_lBALCUTOUT.Get(i);
      WCHAR szTemp[64];
      swprintf (szTemp, gpszCutouts, i);
      MMLValueSet (pNode, szTemp, &pCut->tp);
      swprintf (szTemp, gpszAuto, i);
      MMLValueSet (pNode, szTemp, (int)pCut->fAuto);
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
      MMLValueSet (pSub, gpszWantFullHeight, (int) pbi->fWantFullHeight);

      PCMMLNode2 pCol;
      if (pbi->pColumn) {
         pCol = pbi->pColumn->MMLTo ();
         if (pCol) {
            pCol->NameSet (gpszColumn);
            pSub->ContentAdd (pCol);
         }
      }
   }  // i < m_lPosts.Num()

   // NOTE: don't need to save m_lNoodles since will be reconstructed
   MMLValueSet (pNode, gpszMaxPostDistanceBig, m_fMaxPostDistanceBig);
   MMLValueSet (pNode, gpszMaxPostDistanceSmall, m_fMaxPostDistanceSmall);
   MMLValueSet (pNode, gpszPostDivideIntoFull, (int) m_fPostDivideIntoFull);
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

   MMLValueSet (pNode, gpszPostWantFullHeightCutout, (int) m_fPostWantFullHeightCutout);
   MMLValueSet (pNode, gpszPostHeightAboveBalustrade, m_fPostHeightAboveBalustrade);

   for (i = 0; i < 4; i++) {
      swprintf (szTemp, L"HorzRailUse%d", (int) i);
      MMLValueSet (pNode, szTemp, (int)m_afHorzRailUse[i]);

      swprintf (szTemp, L"HorzRailShape%d", (int) i);
      MMLValueSet (pNode, szTemp, (int)m_adwHorzRailShape[i]);

      swprintf (szTemp, L"HorzRailSize%d", (int) i);
      MMLValueSet (pNode, szTemp, &m_apHorzRailSize[i]);

      swprintf (szTemp, L"HorzRailOffset%d", (int) i);
      MMLValueSet (pNode, szTemp, &m_apHorzRailOffset[i]);
   }

   MMLValueSet (pNode, gpszHorzRailAuto, &m_pHorzRailAuto);
   MMLValueSet (pNode, gpszVertUse, (int) m_fVertUse);
   MMLValueSet (pNode, gpszVertUsePoint, (int) m_fVertUsePoint);
   MMLValueSet (pNode, gpszVertPointTaper, (int) m_dwVertPointTaper);

   for (i = 0; i < 2; i++) {
      swprintf (szTemp, L"VertShape%d", (int) i);
      MMLValueSet (pNode, szTemp, (int)m_adwVertShape[i]);

      swprintf (szTemp, L"VertSize%d", (int) i);
      MMLValueSet (pNode, szTemp, &m_apVertSize[i]);
   }

   MMLValueSet (pNode, gpszVertOffset, m_fVertOffset);
   MMLValueSet (pNode, gpszVertAuto, &m_pVertAuto);
   MMLValueSet (pNode, gpszPanelUse, (int)m_fPanelUse);
   MMLValueSet (pNode, gpszPanelInfo, &m_pPanelInfo);
   MMLValueSet (pNode, gpszPanelOffset, m_fPanelOffset);
   MMLValueSet (pNode, gpszPanelThickness, m_fPanelThickness);
   MMLValueSet (pNode, gpszBrace, (int)m_dwBrace);
   MMLValueSet (pNode, gpszBraceSize, &m_pBraceSize);
   MMLValueSet (pNode, gpszBraceTB, &m_pBraceTB);
   MMLValueSet (pNode, gpszBraceShape, (int) m_dwBraceShape);
   MMLValueSet (pNode, gpszBraceOffset, m_fBraceOffset);
   MMLValueSet (pNode, gpszBalStyle, (int) m_dwBalStyle);
   MMLValueSet (pNode, gpszType, (int)m_dwType);

   for (i = 0; i < BALCOLOR_MAX; i++) {
      swprintf (szTemp, L"BalColor%d", i);
      MMLValueSet (pNode, szTemp, (int) m_adwBalColor[i]);
   }

   return pNode;

}

/***********************************************************************************
CBalustrade::MMLFrom - Reads in MML and fills in variables base don that.

inputs
   PCMMLNode2      pNode - Node to read from
returns
   BOOL - TRUE if successful
*/
BOOL CBalustrade::MMLFrom (PCMMLNode2 pNode)
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
   m_psBottom = NULL;
   m_psTop = NULL;
   m_psOrigBottom = NULL;
   m_psOrigTop = NULL;

   // clear out data structures
   DWORD i;
   PBPOSTINFO pbpi;
   for (i = 0; i < m_lPosts.Num(); i++) {
      pbpi = (PBPOSTINFO) m_lPosts.Get(i);
      if (pbpi->pColumn)
         delete pbpi->pColumn;
   }
   m_lPosts.Clear();
   PBNOODLEINFO pbni;
   for (i = 0; i < m_lNoodles.Num(); i++) {
      pbni = (PBNOODLEINFO) m_lNoodles.Get(i);
      if (pbni->pNoodle)
         delete pbni->pNoodle;
      if (pbni->pColumn)
         delete pbni->pColumn;
      if (pbni->pPanel)
         ESCFREE (pbni->pPanel);
   }
   m_lNoodles.Clear();

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
      else if (!_wcsicmp(psz, gpszPosts)) {
         BPOSTINFO pi;
         memset (&pi, 0, sizeof(pi));

         pi.fLoc = MMLValueGetDouble (pSub, gpszLoc, 0);
         pi.fRight = (BOOL) MMLValueGetInt (pSub, gpszRight, FALSE);
         pi.fLeft = (BOOL) MMLValueGetInt (pSub, gpszLeft, FALSE);
         pi.dwReason = (DWORD) MMLValueGetInt (pSub, gpszReason, 0);
         pi.fWantFullHeight = (BOOL) MMLValueGetInt (pSub, gpszWantFullHeight, FALSE);

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
   m_fFence = (BOOL) MMLValueGetInt (pNode, gpszFence, FALSE);
   m_dwDisplayControl = (DWORD) MMLValueGetInt (pNode, gpszDisplayControl, 0);
   m_fIndent = (BOOL) MMLValueGetInt (pNode, gpszIndent, 0);
   m_fForceVert = (BOOL) MMLValueGetInt (pNode, gpszForceVert, TRUE);
   m_fForceLevel = (BOOL) MMLValueGetInt (pNode, gpszForceLevel, TRUE);
   m_fHideFirst = (BOOL) MMLValueGetInt (pNode, gpszHideFirst, FALSE);
   m_fHideLast = (BOOL) MMLValueGetInt (pNode, gpszHideLast, FALSE);
   m_fNoColumns = (BOOL) MMLValueGetInt (pNode, gpszNoColumns, FALSE);
   m_fSwapSides = (BOOL) MMLValueGetInt (pNode, gpszSwapSides, FALSE);

   m_lBALCUTOUT.Clear();
   for (i = 0; ; i++) {
      WCHAR szTemp[64];
      swprintf (szTemp, gpszCutouts, i);
      BALCUTOUT bc;
      memset (&bc, 0, sizeof(bc));
      bc.tp.h = bc.tp.v = 0;
      if (!MMLValueGetTEXTUREPOINT (pNode, szTemp, &bc.tp, &bc.tp))
         break;
      swprintf (szTemp, gpszAuto, i);
      bc.fAuto = MMLValueGetInt (pNode, szTemp, 1);
      m_lBALCUTOUT.Add (&bc);
   }

   m_fMaxPostDistanceBig = MMLValueGetDouble (pNode, gpszMaxPostDistanceBig, 1);
   m_fMaxPostDistanceSmall = MMLValueGetDouble (pNode, gpszMaxPostDistanceSmall, 1);
   m_fPostDivideIntoFull = (BOOL) MMLValueGetInt (pNode, gpszPostDivideIntoFull, FALSE);
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

   m_fPostWantFullHeightCutout = (BOOL) MMLValueGetInt (pNode, gpszPostWantFullHeightCutout, FALSE);
   m_fPostHeightAboveBalustrade = MMLValueGetDouble (pNode, gpszPostHeightAboveBalustrade, 0);

   for (i = 0; i < 4; i++) {
      swprintf (szTemp, L"HorzRailUse%d", (int) i);
      m_afHorzRailUse[i] = (BOOL) MMLValueGetInt (pNode, szTemp, FALSE);

      swprintf (szTemp, L"HorzRailShape%d", (int) i);
      m_adwHorzRailShape[i] = (DWORD) MMLValueGetInt (pNode, szTemp, NS_RECTANGLE);

      swprintf (szTemp, L"HorzRailSize%d", (int) i);
      MMLValueGetPoint (pNode, szTemp, &m_apHorzRailSize[i]);

      swprintf (szTemp, L"HorzRailOffset%d", (int) i);
      MMLValueGetPoint (pNode, szTemp, &m_apHorzRailOffset[i]);
   }

   MMLValueGetPoint (pNode, gpszHorzRailAuto, &m_pHorzRailAuto);
   m_fVertUse = (BOOL) MMLValueGetInt (pNode, gpszVertUse, FALSE);
   m_fVertUsePoint = (BOOL) MMLValueGetInt (pNode, gpszVertUsePoint, FALSE);
   m_dwVertPointTaper = (DWORD) MMLValueGetInt (pNode, gpszVertPointTaper, FALSE);

   for (i = 0; i < 2; i++) {
      swprintf (szTemp, L"VertShape%d", (int) i);
      m_adwVertShape[i] = (DWORD) MMLValueGetInt (pNode, szTemp, NS_RECTANGLE);

      swprintf (szTemp, L"VertSize%d", (int) i);
      MMLValueGetPoint (pNode, szTemp, &m_apVertSize[i]);
   }

   m_fVertOffset = MMLValueGetDouble (pNode, gpszVertOffset, 0);
   MMLValueGetPoint (pNode, gpszVertAuto, &m_pVertAuto);
   m_fPanelUse = (BOOL) MMLValueGetInt (pNode, gpszPanelUse, FALSE);
   MMLValueGetPoint (pNode, gpszPanelInfo, &m_pPanelInfo);
   m_fPanelOffset = MMLValueGetDouble (pNode, gpszPanelOffset, 0);
   m_fPanelThickness = MMLValueGetDouble (pNode, gpszPanelThickness, 0);
   m_dwBrace = (DWORD) MMLValueGetInt (pNode, gpszBrace, 0);
   MMLValueGetPoint (pNode, gpszBraceSize, &m_pBraceSize);
   MMLValueGetTEXTUREPOINT (pNode, gpszBraceTB, &m_pBraceTB);
   m_dwBraceShape = (DWORD) MMLValueGetInt (pNode, gpszBraceShape, NS_RECTANGLE);
   m_fBraceOffset = MMLValueGetDouble (pNode, gpszBraceOffset, 0);
   m_dwBalStyle = (DWORD) MMLValueGetInt (pNode, gpszBalStyle, 0);
   m_dwType = (DWORD) MMLValueGetInt (pNode, gpszType, 0);

   for (i = 0; i < BALCOLOR_MAX; i++) {
      swprintf (szTemp, L"BalColor%d", i);
      m_adwBalColor[i] = (DWORD) MMLValueGetInt (pNode, szTemp, 0);
   }

   BuildNoodleLocations();

   return TRUE;
}


/*************************************************************************************
CBalustrade::CloneTo - Clones the current balustrade object and returns a cloned version.


NOTE: This does not actually clone the claimed colors, etc.



inputs
   CBalustrade *pNew
   PCObjecTemplate      pTemplate - New template.
returns
   PCBalustrade - new object. Must be freed by the calelr.
*/
BOOL CBalustrade::CloneTo (CBalustrade *pNew, PCObjectTemplate pTemplate)
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
   pNew->m_psOrigBottom = NULL;
   pNew->m_psOrigTop = NULL;
   pNew->m_psBottom = NULL;
   pNew->m_psTop = NULL;

   // clear out data structures
   DWORD i;
   PBPOSTINFO pbpi;
   for (i = 0; i < pNew->m_lPosts.Num(); i++) {
      pbpi = (PBPOSTINFO) pNew->m_lPosts.Get(i);
      if (pbpi->pColumn)
         delete pbpi->pColumn;
   }
   pNew->m_lPosts.Clear();
   PBNOODLEINFO pbni;
   for (i = 0; i < pNew->m_lNoodles.Num(); i++) {
      pbni = (PBNOODLEINFO) pNew->m_lNoodles.Get(i);
      if (pbni->pNoodle)
         delete pbni->pNoodle;
      if (pbni->pColumn)
         delete pbni->pColumn;
      if (pbni->pPanel)
         ESCFREE (pbni->pPanel);
   }
   pNew->m_lNoodles.Clear();

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
   pNew->m_fHideFirst = m_fHideFirst;
   pNew->m_fHideLast = m_fHideLast;
   pNew->m_fNoColumns = m_fNoColumns;
   pNew->m_fSwapSides = m_fSwapSides;
   pNew->m_fForceLevel = m_fForceLevel;
   pNew->m_fHeight = m_fHeight;
   pNew->m_fFence = m_fFence;
   pNew->m_fMaxPostDistanceBig = m_fMaxPostDistanceBig;
   pNew->m_fPostDivideIntoFull = m_fPostDivideIntoFull;
   pNew->m_fMaxPostDistanceSmall = m_fMaxPostDistanceSmall;
   pNew->m_fBraceBelow = m_fBraceBelow;
   memcpy (pNew->m_adwPostShape, m_adwPostShape, sizeof(m_adwPostShape));
   memcpy (pNew->m_adwPostTaper, m_adwPostTaper, sizeof(m_adwPostTaper));
   memcpy (pNew->m_apPostSize, m_apPostSize, sizeof(m_apPostSize));
   memcpy (pNew->m_afPostUse, m_afPostUse, sizeof(m_afPostUse));
   pNew->m_fPostWantFullHeightCutout = m_fPostWantFullHeightCutout;
   pNew->m_fPostHeightAboveBalustrade = m_fPostHeightAboveBalustrade;

   pNew->m_lBALCUTOUT.Init (sizeof(BALCUTOUT), m_lBALCUTOUT.Get(0), m_lBALCUTOUT.Num());

   // Will need to close post info
   // asume start out with no posts
   pNew->m_lPosts.Init (sizeof(BPOSTINFO), m_lPosts.Get(0), m_lPosts.Num());
   for (i = 0; i < pNew->m_lPosts.Num(); i++) {
      PBPOSTINFO pbi = (PBPOSTINFO) pNew->m_lPosts.Get (i);
      if (pbi->pColumn)
         pbi->pColumn = pbi->pColumn->Clone();
   }
   pNew->m_lNoodles.Init (sizeof(BNOODLEINFO), m_lNoodles.Get(0), m_lNoodles.Num());
   for (i = 0; i < pNew->m_lNoodles.Num(); i++) {
      PBNOODLEINFO pbi = (PBNOODLEINFO) pNew->m_lNoodles.Get (i);
      if (pbi->pNoodle)
         pbi->pNoodle = pbi->pNoodle->Clone();
      if (pbi->pColumn)
         pbi->pColumn = pbi->pColumn->Clone();
      if (pbi->pPanel) {
         PCPoint pTemp = pbi->pPanel;
         pbi->pPanel = (PCPoint) ESCMALLOC(sizeof(CPoint)*4);
         if (pbi->pPanel)
            memcpy (pbi->pPanel, pTemp, sizeof(CPoint)*4);
      }
   }

   // clone rail information
   memcpy (pNew->m_afHorzRailUse, m_afHorzRailUse, sizeof(m_afHorzRailUse));
   memcpy (pNew->m_adwHorzRailShape, m_adwHorzRailShape, sizeof(m_adwHorzRailShape));
   memcpy (pNew->m_apHorzRailSize, m_apHorzRailSize, sizeof(m_apHorzRailSize));
   memcpy (pNew->m_apHorzRailOffset, m_apHorzRailOffset, sizeof(m_apHorzRailOffset));
   pNew->m_pHorzRailAuto.Copy (&m_pHorzRailAuto);
   pNew->m_fVertUse = m_fVertUse;
   pNew->m_fVertUsePoint = m_fVertUsePoint;
   pNew->m_dwVertPointTaper = m_dwVertPointTaper;
   memcpy (pNew->m_adwVertShape, m_adwVertShape, sizeof(m_adwVertShape));
   memcpy (pNew->m_apVertSize, m_apVertSize, sizeof(m_apVertSize));
   pNew->m_fVertOffset = m_fVertOffset;
   pNew->m_pVertAuto.Copy (&m_pVertAuto);
   pNew->m_pPanelInfo.Copy (&m_pPanelInfo);
   pNew->m_fPanelUse = m_fPanelUse;
   pNew->m_fPanelOffset = m_fPanelOffset;
   pNew->m_fPanelThickness = m_fPanelThickness;
   pNew->m_dwBrace = m_dwBrace;
   pNew->m_fBraceOffset = m_fBraceOffset;
   pNew->m_pBraceSize.Copy (&m_pBraceSize);
   pNew->m_pBraceTB = m_pBraceTB;
   pNew->m_dwBraceShape = m_dwBraceShape;
   pNew->m_dwBalStyle = m_dwBalStyle;
   pNew->m_dwType = m_dwType;

   pNew->m_pTemplate = pTemplate;
   memcpy (pNew->m_adwBalColor, m_adwBalColor, sizeof(pNew->m_adwBalColor));

   return TRUE;
}



/*************************************************************************************
CBalustrade::Clone - Clones the current balustrade object and returns a cloned version.


NOTE: This does not actually clone the claimed colors, etc.

inputs
   none
returns
   PCBalustrade - new object. Must be freed by the calelr.
*/
CBalustrade *CBalustrade::Clone (void)
{
   PCBalustrade pNew = new CBalustrade;
   if (!pNew)
      return NULL;

   if (!CloneTo (pNew, m_pTemplate)) {
      delete pNew;
      return NULL;
   }
   return pNew;
}


/********************************************************************************
CBalustrade::Render - Causes the balustrade to draw.

NOTE: Right now just draws the ballistrades in a fixed color
inputs
   POBJECTRENDER        pr - Rendering information
   PCRenderSurface      *prs - Render to.
returns
   none
*/
void CBalustrade::Render (DWORD dwRenderShard, POBJECTRENDER pr, CRenderSurface *prs)
{
   // may not want to draw this
   BOOL fBalustrade;
   fBalustrade = !m_fFence;
   if (fBalustrade) {
      if (!(pr->dwShow & RENDERSHOW_BALUSTRADES))
         return;
   }
   else {   // else piers
      if (!(pr->dwShow & RENDERSHOW_LANDSCAPING))
         return;
   }

   // loop through all the colors
   DWORD dwColor;
   for (dwColor = 0; dwColor < BALCOLOR_MAX; dwColor++) {
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
      case BALCOLOR_POSTMAIN:
         fPaintPost = TRUE;
         dwColumnFlag = 0x01;
         break;
      case BALCOLOR_POSTBASE:
         fPaintPost = TRUE;
         dwColumnFlag = 0x02;
         break;
      case BALCOLOR_POSTTOP:
         fPaintPost = TRUE;
         dwColumnFlag = 0x04;
         break;
      case BALCOLOR_POSTBRACE:
         fPaintPost = TRUE;
         // BUGFIX - Was 0x01
         dwColumnFlag = 0x08;
         break;
      case BALCOLOR_HANDRAIL:
         fPaintNoodle = TRUE;
         wNoodleColor = 0;
         break;
      case BALCOLOR_TOPBOTTOMRAIL:
         fPaintNoodle = TRUE;
         wNoodleColor = 1;
         break;
      case BALCOLOR_AUTOHORZRAIL:
         fPaintNoodle = TRUE;
         wNoodleColor = 3;
         break;
      case BALCOLOR_AUTOVERTRAIL:
         fPaintNoodle = TRUE;
         wNoodleColor = 4;
         break;
      case BALCOLOR_PANEL:
         fPaintNoodle = TRUE;
         wNoodleColor = 5;
         break;
      case BALCOLOR_BRACE:
         fPaintNoodle = TRUE;
         wNoodleColor = 2;
         break;
      }

      // draw all the columns
      DWORD i;
      if (fPaintPost) {
         PBPOSTINFO pbpi;
         for (i = 0; i < m_lPosts.Num(); i++) {
            pbpi = (PBPOSTINFO) m_lPosts.Get(i);
            if (!pbpi->pColumn)
               continue;

            // if we're supposed to hide the first or last one then do so
            if (m_fHideFirst && !i)
               continue;
            if (m_fHideLast && (i+1 == m_lPosts.Num()))
               continue;

            pbpi->pColumn->Render (pr, prs, dwColumnFlag);
         } // posts
      }

      if (fPaintNoodle) {
         PBNOODLEINFO pbni;
         pbni = (PBNOODLEINFO) m_lNoodles.Get(0);
         for (i = 0; i < m_lNoodles.Num(); i++) {
            // only accept the current color
            if (pbni[i].wColor != wNoodleColor)
               continue;

            if (pbni[i].pNoodle)
               pbni[i].pNoodle->Render (pr, prs);
            if (pbni[i].pColumn)
               pbni[i].pColumn->Render (pr, prs);
            if (pbni[i].pPanel) {
               PCPoint p = pbni[i].pPanel;
               DWORD dwIndex;
               PCPoint pn = prs->NewPoints (&dwIndex, 4);
               if (pn) {
                  pn[0*2+1].Copy (&p[0]);
                  pn[1*2+1].Copy (&p[1]);
                  pn[1*2+0].Copy (&p[2]);
                  pn[0*2+0].Copy (&p[3]);
                  prs->ShapeSurface (0, 2, 2, pn, dwIndex, NULL, 0, FALSE);
               }
            }
         }  // noodles
      }

   }  // dwColors

}




/**********************************************************************************
CBalustrade::QueryBoundingBox - Standard API
*/
void CBalustrade::QueryBoundingBox (PCPoint pCorner1, PCPoint pCorner2)
{
   CPoint p1, p2;
   BOOL fSet = FALSE;

   PBPOSTINFO pbpi;
   DWORD i, j;
   for (i = 0; i < m_lPosts.Num(); i++) {
      pbpi = (PBPOSTINFO) m_lPosts.Get(i);
      if (!pbpi->pColumn)
         continue;

      // if we're supposed to hide the first or last one then do so
      if (m_fHideFirst && !i)
         continue;
      if (m_fHideLast && (i+1 == m_lPosts.Num()))
         continue;

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

   PBNOODLEINFO pbni;
   pbni = (PBNOODLEINFO) m_lNoodles.Get(0);
   for (i = 0; i < m_lNoodles.Num(); i++) {
      if (pbni[i].pNoodle) {
         pbni[i].pNoodle->QueryBoundingBox (&p1, &p2);
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
      if (pbni[i].pColumn) {
         pbni[i].pColumn->QueryBoundingBox (&p1, &p2);
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
      if (pbni[i].pPanel) {
         PCPoint p = pbni[i].pPanel;
         for (j = 0; j < 4; j++) {
            if (fSet) {
               pCorner1->Min (&p[j]);
               pCorner2->Max (&p[j]);
            }
            else {
               pCorner1->Copy (&p[j]);
               pCorner2->Copy (&p[j]);
               fSet = TRUE;
            }
         } // j

      }
   }  // noodles

}


/*******************************************************************************
AddAutoCutout - Adds an automatic cutout to the list.

inputs
   PCListFixed    pBALCUTOUT - List of BALCUTOUT to add to
   PBALCUTOUT     pNew - New one to add
*/
void AddAutoCutout (PCListFixed pBALCUTOUT, PBALCUTOUT pNew)
{
   PBALCUTOUT pc = (PBALCUTOUT) pBALCUTOUT->Get(0);

   // insert this
   DWORD i;
   for (i = 0; i < pBALCUTOUT->Num(); i++)
      if (pNew->tp.h < pc[i].tp.h)
         break;
   if (i < pBALCUTOUT->Num())
      pBALCUTOUT->Insert (i, pNew);
   else
      pBALCUTOUT->Add (pNew);

   // merge overlapping ones
   for (i = 0; i+1 < pBALCUTOUT->Num(); ) {
      pc = (PBALCUTOUT) pBALCUTOUT->Get(i);
      if (pc[0].tp.v + CLOSE < pc[1].tp.h) {
         i++;
         continue;   // no overlap
      }

      // else, overlaps
      pc[0].tp.v = max(pc[0].tp.v, pc[1].tp.v);
      pc[0].fAuto &= pc[1].fAuto;   // only keep auto flag if both auto

      // delete future one and redo
      pBALCUTOUT->Remove (i+1);
   }

   // done
}

/********************************************************************************
CBalustrade::CutoutBasedOnSurface - THis takes a surface (pss) and matrix that translates
from the surface into ballistrade space (m), and sees what parts of the spline's bottom
are within the non-clipped/cutout area of the surface. This is used to only draw
ballistades and supports in place where the surface exists.

NOTE: The surface must be rectangular and flat for this to work

inputs
   PCSplienSurface      pss - Surface
   PCMatrix             m - Translates from surface coordinates into balustrade coordinates
returns
   BOOL - TRUE if success
*/
BOOL CBalustrade::CutoutBasedOnSurface (PCSplineSurface pss, PCMatrix m)
{
   // Should clear out current cutouts
   // BUGFIX - Clear only automatic ones
   DWORD i;
   for (i = m_lBALCUTOUT.Num()-1; i < m_lBALCUTOUT.Num(); i--) {
      PBALCUTOUT pc = (PBALCUTOUT) m_lBALCUTOUT.Get(i);
      if (pc->fAuto)
         m_lBALCUTOUT.Remove (i);
   }

   // fail if no bottom spline
   if (!m_psBottom || !pss || !m)
      return FALSE;

   // NOTE: Assuming pSurf is 2x2 and linear
   if ((pss->ControlNumGet (TRUE) != 2) || (pss->ControlNumGet(FALSE) != 2))
      return FALSE;
   if ((pss->SegCurveGet (TRUE, 0) != SEGCURVE_LINEAR) || (pss->SegCurveGet (FALSE, 0) != SEGCURVE_LINEAR))
      return FALSE;
   CPoint pMin, pMax, pCur;
   pMax.Zero();   // BUGFIX - So doesnt complain of runtime error
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
   dwNew = m_psBottom->QueryNodes();
   if (!memPoints.Required ((dwNew+1) * sizeof(CPoint)))
      return TRUE;

   // load it in
   PCPoint paPoints;
   paPoints = (PCPoint) memPoints.p;
   PTEXTUREPOINT ptp;
   for (i = 0; i < dwNew; i++) {
      paPoints[i].Copy (m_psBottom->LocationGet (i));
      ptp = m_psBottom->TextureGet (i);
      // BUGFIX - If no ptp then fail gracefilly
      if (!ptp)
         return TRUE;
      paPoints[i].p[2] = ptp->h;
      paPoints[i].p[3] = 1;
   }
   BOOL fLooped;
   fLooped = m_psBottom->QueryLooped ();


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
            BALCUTOUT bc;
            memset (&bc, 0, sizeof(bc));
            bc.fAuto = TRUE;
            bc.tp.h = afCutout[0];
            bc.tp.v = afCutout[1];
            AddAutoCutout (&m_lBALCUTOUT, &bc);

            tpCS = tpNE;
            fInCutout = FALSE;
         }
         else if (!AreClose (&tpCS, &tpNS)) {
            // surprise surprise, we just skipped some
            afCutout[0] = PointOn2DLine (&tpCS, &tpS, &tpE) * fScale + fOffset;
            afCutout[1] = PointOn2DLine (&tpNS, &tpS, &tpE) * fScale + fOffset;
            BALCUTOUT bc;
            memset (&bc, 0, sizeof(bc));
            bc.fAuto = TRUE;
            bc.tp.h = afCutout[0];
            bc.tp.v = afCutout[1];
            AddAutoCutout (&m_lBALCUTOUT, &bc);

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
      BALCUTOUT bc;
      memset (&bc, 0, sizeof(bc));
      bc.fAuto = TRUE;
      bc.tp.h = afCutout[0];
      bc.tp.v = afCutout[1];
      AddAutoCutout (&m_lBALCUTOUT, &bc);
   }


   // figure out where posts go
   DeterminePostLocations();

   return TRUE;
}


/**************************************************************************************
CBalustrade::PostToCoord - Given two post numbers, this generates 8 top and 8 bottom
points per post, plus 1 of each for the last post, to indicate the positions of the top
(m_fHeight) and bottom (0 = floor) of the railings.

inputs
   DWORD       dwStart - Starting post ID. Uses m_lPosts
   DWORD       dwEnd - Ending post ID. This can be >= m_lPosts.Num() because modulo
               function is used.
   PCListFixed plBottom - Initialized to sizeof(CPoint). Filled with (dwEnd-dwStart)*8+1 entries.
   PCListFixed plTop - Initialize to sizeof(CPoint). Same as plBottom.
returns
   none
*/
void CBalustrade::PostToCoord (DWORD dwStart, DWORD dwEnd, PCListFixed plBottom, PCListFixed plTop)
{
   plBottom->Init (sizeof(CPoint));
   plBottom->Clear();
   plTop->Init (sizeof(CPoint));
   plTop->Clear();

   DWORD i;
   for (i = dwStart; i < dwEnd; i++) {
      PBPOSTINFO pStart = (PBPOSTINFO) m_lPosts.Get(i % m_lPosts.Num());
      PBPOSTINFO pEnd = (PBPOSTINFO) m_lPosts.Get((i+1) % m_lPosts.Num());
      fp fStart = pStart->fLoc;
      fp fEnd = pEnd->fLoc;
      if (fEnd <= fStart)
         fEnd += 1.0;   // modulo

      CPoint pBottom, pTop, pDir;
      DWORD j;
      for (j = 0; j <= 8; j++) {
         // only do the 8th point for the last bit
         if ((j == 8) && (i+1 < dwEnd))
            continue;

         // get the location
         fp fLoc;
         fLoc = (fp)j / 8.0 * (fEnd - fStart) + fStart;
         if (fLoc > 1.0) {
            if (fLoc > 1.0 + EPSILON)
               fLoc = fmod(fLoc, 1);
            else
               fLoc = 1.0;
         }
         TEXTUREPOINT tp;
         m_psBottom->LocationGet(fLoc, &pBottom);
         m_psBottom->TextureGet (fLoc, &tp);
         pBottom.p[2] = tp.h;
         m_psTop->LocationGet (fLoc, &pTop);
         m_psTop->TextureGet (fLoc, &tp);
         pTop.p[2] = tp.h;

         // make the right length
         pDir.Subtract (&pTop, &pBottom);
         pDir.Normalize();
         pDir.Scale (m_fHeight);

         pTop.Add (&pDir, &pBottom);

         // add to the list
         plBottom->Add (&pBottom);
         plTop->Add (&pTop);
      }  // j
   } // i
}

/**************************************************************************************
CBalustrade::LengthFromCoord - Given a pointer to a list of CPoint from ::PostToCoord,
this sums up all the distances and provides a length.

inputs
   PCListFixed    pl - List of CPoint
   DWORD          dwStart - Start index into pl
   DWORD          dwEnd - End index into pl. < pl->Num()
returns
   fp - length
*/
fp CBalustrade::LengthFromCoord (PCListFixed pl, DWORD dwStart, DWORD dwEnd)
{
   fp f = 0;
   DWORD i;
   for (i = dwStart; i < dwEnd; i++) {
      PCPoint ps = (PCPoint) pl->Get(i);
      PCPoint pe = (PCPoint) pl->Get(i+1);
      CPoint p;
      p.Subtract (ps, pe);
      f += p.Length();
   }

   return f;
}


/**************************************************************************************
RemoveLinear - Given a list of points, this removes any points that end up producing
a linear section.

inputs
   PCListFixed       pl - List of CPoint. Will be modified
   BOOL              fLoop - Set to true if looped
returns
   noen
*/
void RemoveLinear (PCListFixed pl, BOOL fLoop)
{
   DWORD i;

   // BUGFIX - remove close points - moved out of other loop
   for (i = 0; i < pl->Num();) {
      if (!fLoop && (i+1 >= pl->Num())) {
         i++;
         continue;
      }
      PCPoint pp = (PCPoint) pl->Get(0);
      if (pp[i].AreClose(&pp[(i+1)%(pl->Num())]))
         pl->Remove((i+1 < pl->Num()) ? (i+1) : i);
      else
         i++;
   }

   for (i = 0; i < pl->Num(); ) {
      PCPoint pp = (PCPoint) pl->Get(0);
      DWORD dwNum = pl->Num();
      PCPoint pp0 = pp + (i);
      PCPoint pp1 = pp + ((i+1)%dwNum);
      PCPoint pp2 = pp + ((i+2)%dwNum);

      // BUGFIX - Linear check around boundary
      if (!fLoop && (i+2 >= dwNum)) {
         i++;
         continue;
      }
      // BUGFIX - if are same then remove next one
      //if (pp0->AreClose (pp1)) {
      //   // linear
      //   pl->Remove (i+1);
      //   continue;
      //}

      CPoint p1, p2;
      p1.Subtract (pp1, pp0);
      p2.Subtract (pp2, pp1); // BUGFIX - Was pp2 - pp0, but caused problems with very long segments
      p1.Normalize();
      p2.Normalize();
      p2.Subtract (&p1);
      if (p2.Length() < .01) {
         // linear
         pl->Remove ((i+1) % dwNum);
      }
      else {
         // not linear
         i++;
      }
   }
}

/**************************************************************************************
CBalustrade::NoodleFromCoord - Given a pointer to a list of CPoint from ::PostToCoord,
this creates a CNoodle whose path is ininitialized based upon the coordinates.

NOTE: It does coordinate minimization, looking for linear segemnts, while its at it.

inputs
   PCListFixed    plBottom - Bottom row, at floor height
   PCListFixed    plTop - Top row, and m_fHeight
   BOOL           fBaseFromTop - If TRUE, height offset is based off top, else bottom
   fp         fOffset - Offset (+ is higher) from the top of bottom, from fBaseFromTop.
   DWORD          dwStart - start in plTop/Bottom to use
   DWORD          dwEnd - end point index in plTop/Bottom to use. inclusive
   BOOL           fLooped - If TRUE then the noodle is looped.
returns
   PCNoodle - New noodle that must be freed by caller. Already has its path set.
*/
PCNoodle CBalustrade::NoodleFromCoord (PCListFixed plBottom, PCListFixed plTop,
                                       BOOL fBaseFromTop, fp fOffset,
                                       DWORD dwStart, DWORD dwEnd, BOOL fLooped)
{
   // fill a list with points
   CListFixed lp;
   lp.Init (sizeof(CPoint));
   DWORD i;
   PCPoint paBot, paTop;
   CPoint pt;
   paBot = (PCPoint) plBottom->Get(0);
   paTop = (PCPoint) plTop->Get(0);
   fp fAlpha;
   fAlpha = fOffset / m_fHeight + (fBaseFromTop ? 1.0 : 0.0);
   for (i = dwStart; i <= dwEnd; i++) {
      pt.Average (&paTop[i], &paBot[i], fAlpha);
      lp.Add (&pt);
   }

   // look for linear portions
   RemoveLinear (&lp, fLooped);

   // create noodle
   PCNoodle pn;
   pn = new CNoodle;
   if (!pn)
      return NULL;
   if (!pn->PathSpline (fLooped, lp.Num(), (PCPoint)lp.Get(0), (DWORD*)SEGCURVE_LINEAR, 0)) {
      delete pn;
      return NULL;
   }
   return pn;
}

/**************************************************************************************
CBalustrade::DistanceFromPost - Given the list generated by ::PostToCoord, this finds
a point N meters to the right (or left if use negative) of the post.

inputs
   PCListFixed    pl - List from ::PostToCoord
   DWORD          dwPost - Post index to use, from pl. Probably a multiple of 8.
   fp         fRight - Distance to the right/clockwise. Use negative values for left.
   PCPoint        pVal - Filled with the point
returns
   none
*/
void CBalustrade::DistanceFromPost (PCListFixed pl, DWORD dwPost, fp fRight, PCPoint pVal)
{
   int   iDir;
   iDir = (fRight >= 0) ? 1 : -1;
   fRight = fabs(fRight);

   while (TRUE) {
      int iNext;
      iNext = (int) dwPost + iDir;
      if ((iNext < 0) || (iNext >= (int)pl->Num())) {
         // beyond the edge, so just return the post
         pVal->Copy ((PCPoint) pl->Get(dwPost));
         return;
      }

      // get the next one
      PCPoint pCur, pNext;
      CPoint pDir;
      pCur = (PCPoint) pl->Get(dwPost);
      pNext = (PCPoint) pl->Get((DWORD)iNext);
      pDir.Subtract (pNext, pCur);

      // length
      fp fLen;
      fLen = pDir.Length();
      if (fLen <= fRight) {
         // need to move along
         fRight -= fLen;
         dwPost = (DWORD) iNext;
         continue;
      }

      // else, interpolate
      pVal->Average (pNext, pCur, fRight/fLen);
      return;
   }
}


/*************************************************************************************
CBalustrade::ClaimSurface - Loops through all the texture surfaces and finds one
that can claim as own. If so, claims it.

inputs
   DWORD       dwColor - One of BALCOLOR_XXX
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
DWORD CBalustrade::ClaimSurface (DWORD dwColor, COLORREF cColor, DWORD dwMaterialID, PWSTR pszScheme,
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
CBalustrade::ClaimClear - Clear all the claimed surfaces. This ends up
gettind rid of colors information and embedding information.

  
NOTE: An object that has a CBalustrade should call this if it's deleting
the furace but NOT itself. THis eliminates the claims for the surfaces

*/
void CBalustrade::ClaimClear (void)
{
   DWORD i;

   // free up all the current claim surfaces
   if (m_pTemplate) {
      for (i = 0; i < BALCOLOR_MAX; i++) {
         if (m_adwBalColor[i])
            m_pTemplate->ObjectSurfaceRemove (m_adwBalColor[i]);
      }
   }
   memset (m_adwBalColor, 0, sizeof(m_adwBalColor));
}


/*************************************************************************************
CBalustrade::ClaimCloneTo - Looks at all the claims used in this object. These
are copied into the new fp surface - keeping the same IDs. Which means that can
onlyu call ClaimCloneTo when cloning to a blank destination object.


inputs
   PCBalustrade      pCloneTo - cloning to this
   PCObjectTemplate     pTemplate - Template. THis is what really is changed.
                        New ObjectSurfaceAdd() and ContainerSurfaceAdd()
returns
   none
*/
void CBalustrade::ClaimCloneTo (CBalustrade *pCloneTo, PCObjectTemplate pTemplate)
{
   if (!m_pTemplate)
      return;

   DWORD i;
   for (i = 0; i < BALCOLOR_MAX; i++) {
      if (!m_adwBalColor[i])
         continue;

      pTemplate->ObjectSurfaceAdd (m_pTemplate->ObjectSurfaceFind(m_adwBalColor[i]));
   }
}

/*************************************************************************************
CBalustrade::ClaimRemove - Remove a claim on a particular ID.

inputs
   DWORD       dwID - to remove the claim on. From m_pTemplate->ObjectSurfaceAdd
returns
   BOOL - TRUE if success
*/
BOOL CBalustrade::ClaimRemove (DWORD dwID)
{
   if (!dwID)
      return FALSE;

   // find i
   DWORD i;
   for (i = 0; i < BALCOLOR_MAX; i++)
      if (dwID ==m_adwBalColor[i])
         break;
   if (i >= BALCOLOR_MAX)
      return FALSE;

   // found it
   m_pTemplate->ObjectSurfaceRemove (dwID);
   m_adwBalColor[i] = 0;

   return TRUE;
}

/*************************************************************************************
CBalustrade::ClaimAllNecessaryColors - Call this after filling in the m_lPostts and
m_lNoodles information. This will claim all the necessary colors, and free up
any claimed colors that aren't used.
*/
void CBalustrade::ClaimAllNecessaryColors (void)
{
   if (!m_pTemplate)
      return;

   // keep track of which ones we have
   BOOL afUse[BALCOLOR_MAX];
   memset (afUse, 0, sizeof(afUse));

   // want colomn colors if use
   afUse[BALCOLOR_POSTMAIN] = TRUE;
   if (m_afPostUse[0])  // buttom
      afUse[BALCOLOR_POSTBASE] = TRUE;
   if (m_afPostUse[1] || m_afPostUse[2])
      afUse[BALCOLOR_POSTTOP] = TRUE;
   if (m_afPostUse[3])
      afUse[BALCOLOR_POSTBRACE] = TRUE;

   if (m_afHorzRailUse[0])
      afUse[BALCOLOR_HANDRAIL] = TRUE;
   if (m_afHorzRailUse[1])
      afUse[BALCOLOR_TOPBOTTOMRAIL] = TRUE;
   if (m_afHorzRailUse[2])
      afUse[BALCOLOR_TOPBOTTOMRAIL] = TRUE;
   if (m_afHorzRailUse[3])
      afUse[BALCOLOR_AUTOHORZRAIL] = TRUE;
   if (m_dwBrace)
      afUse[BALCOLOR_BRACE] = TRUE;
   if (m_fVertUse)
      afUse[BALCOLOR_AUTOVERTRAIL] = TRUE;
   if (m_fPanelUse)
      afUse[BALCOLOR_PANEL] = TRUE;

   // loop through and remove those not using
   DWORD i;
   for (i = 0; i < BALCOLOR_MAX; i++)
      if (m_adwBalColor[i] && !afUse[i])
         ClaimRemove (m_adwBalColor[i]);

   // create ones that using
   if (afUse[BALCOLOR_POSTMAIN] && !m_adwBalColor[BALCOLOR_POSTMAIN])
      ClaimSurface (BALCOLOR_POSTMAIN,
         m_fFence ? RGB(0x40, 0x40, 0x20) : RGB(0x40, 0x40, 0x40),
         MATERIAL_PAINTSEMIGLOSS,
         m_fFence ? L"Fence, main post" : L"Balustrade, main post");
   if (afUse[BALCOLOR_POSTBASE] && !m_adwBalColor[BALCOLOR_POSTBASE])
      ClaimSurface (BALCOLOR_POSTBASE,
         m_fFence ? RGB(0x40, 0x40, 0x20) : RGB(0x40, 0x40, 0x40),
         MATERIAL_PAINTSEMIGLOSS,
         m_fFence ? L"Fence, post base" : L"Balustrade, post base");
   if (afUse[BALCOLOR_POSTTOP] && !m_adwBalColor[BALCOLOR_POSTTOP])
      ClaimSurface (BALCOLOR_POSTTOP,
         m_fFence ? RGB(0x40, 0x40, 0x20) : RGB(0x40, 0x40, 0x40),
         MATERIAL_PAINTSEMIGLOSS,
         m_fFence ? L"Fence, post cap" : L"Balustrade, post cap");
   if (afUse[BALCOLOR_POSTBRACE] && !m_adwBalColor[BALCOLOR_POSTBRACE])
      ClaimSurface (BALCOLOR_POSTBRACE,
         m_fFence ? RGB(0x40, 0x40, 0x20) : RGB(0x40, 0x40, 0x40),
         MATERIAL_PAINTSEMIGLOSS,
         m_fFence ? L"Fence, post brace" : L"Balustrade, post brace");

   if (afUse[BALCOLOR_HANDRAIL] && !m_adwBalColor[BALCOLOR_HANDRAIL])
      ClaimSurface (BALCOLOR_HANDRAIL,
         m_fFence ? RGB(0xc0, 0xc0, 0x40) : RGB(0x40, 0x40, 0xc0),
         MATERIAL_PAINTSEMIGLOSS,
         m_fFence ? L"Fence, handrail" : L"Balustrade, handrail");
   if (afUse[BALCOLOR_TOPBOTTOMRAIL] && !m_adwBalColor[BALCOLOR_TOPBOTTOMRAIL])
      ClaimSurface (BALCOLOR_TOPBOTTOMRAIL,
         m_fFence ? RGB(0xc0, 0xc0, 0x40) : RGB(0x40, 0x40, 0xc0),
         MATERIAL_PAINTSEMIGLOSS,
         m_fFence ? L"Fence, rails" : L"Balustrade, rails");
   if (afUse[BALCOLOR_AUTOHORZRAIL] && !m_adwBalColor[BALCOLOR_AUTOHORZRAIL])
      ClaimSurface (BALCOLOR_AUTOHORZRAIL,
         m_fFence ? RGB(0x80, 0x80, 0x40) : RGB(0x40, 0x40, 0x80),
         MATERIAL_PAINTSEMIGLOSS,
         m_fFence ? L"Fence, horizontals" : L"Balustrade, horizontals");
   if (afUse[BALCOLOR_AUTOVERTRAIL] && !m_adwBalColor[BALCOLOR_AUTOVERTRAIL])
      ClaimSurface (BALCOLOR_AUTOVERTRAIL,
         m_fFence ? RGB(0x80, 0x80, 0x40) : RGB(0x40, 0x40, 0x80),
         MATERIAL_PAINTSEMIGLOSS,
         m_fFence ? L"Fence, verticals" : L"Balustrade, verticals");
   if (afUse[BALCOLOR_PANEL] && !m_adwBalColor[BALCOLOR_PANEL])
      ClaimSurface (BALCOLOR_PANEL,
         m_fFence ? RGB(0x80, 0x80, 0x40) : RGB(0x40, 0x40, 0x80),
         MATERIAL_PAINTSEMIGLOSS,
         m_fFence ? L"Fence, panel" : L"Balustrade, panel");
   if (afUse[BALCOLOR_BRACE] && !m_adwBalColor[BALCOLOR_BRACE])
      ClaimSurface (BALCOLOR_BRACE,
         m_fFence ? RGB(0x80, 0x80, 0x40) : RGB(0x40, 0x40, 0xc0),
         MATERIAL_PAINTSEMIGLOSS,
         m_fFence ? L"Fence, bracing" : L"Balustrade, bracing");
}


/*************************************************************************************
CBalustrade::ClaimFindByID - Given a claim ID, finds the first claim with that ID.

inputs
   DWORD       dwID
returns
   BOOL - TRUE if found.
*/
BOOL CBalustrade::ClaimFindByID (DWORD dwID)
{
   if (!dwID)
      return FALSE;

   DWORD i;
   for (i = 0; i < BALCOLOR_MAX; i++) {
      if (m_adwBalColor[i] == dwID)
         return TRUE;
   }
   return FALSE;
}

/* BalAppearancePage
*/
BOOL BalAppearancePage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PAPIN pa = (PAPIN)pPage->m_pUserData;
   PCBalustrade pv = pa->pBal;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         PCEscControl pControl;
         MeasureToString (pPage, L"height", pv->m_fHeight);
         pControl = pPage->ControlFind (L"enablecolumns");
         if (pControl && pv->m_fPostDivideIntoFull)
            pControl->AttribSetBOOL (Checked(), TRUE);
         pControl = pPage->ControlFind (L"atcutouts");
         if (pControl && pv->m_fPostWantFullHeightCutout)
            pControl->AttribSetBOOL (Checked(), TRUE);
         MeasureToString (pPage, L"maxbigdist", pv->m_fMaxPostDistanceBig);
         MeasureToString (pPage, L"maxsmalldist", pv->m_fMaxPostDistanceSmall);

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
            pControl->Enable (pv->m_dwBalStyle == BS_CUSTOM);

         pControl = pPage->ControlFind (L"hidefirst");
         if (pControl)
            pControl->AttribSetBOOL (Checked(), pv->m_fHideFirst);
         pControl = pPage->ControlFind (L"hidelast");
         if (pControl)
            pControl->AttribSetBOOL (Checked(), pv->m_fHideLast);
         pControl = pPage->ControlFind (L"swapsides");
         if (pControl)
            pControl->AttribSetBOOL (Checked(), pv->m_fSwapSides);
         pControl = pPage->ControlFind (L"nocolumns");
         if (pControl)
            pControl->AttribSetBOOL (Checked(), pv->m_fNoColumns);
      }
      break;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;

         // if any of the buttons are pressed redraw
         if (!_wcsicmp(p->pControl->m_pszName, L"enablecolumns")) {
            if (pv->m_pTemplate->m_pWorld)
               pv->m_pTemplate->m_pWorld->ObjectAboutToChange (pv->m_pTemplate);

            pv->m_fPostDivideIntoFull = p->pControl->AttribGetBOOL(Checked());

            // recalculate
            pv->DeterminePostLocations();
            if (pa->pBB)
               pa->pBB->BalustradeExtendToRoof ();

            if (pv->m_pTemplate->m_pWorld)
               pv->m_pTemplate->m_pWorld->ObjectChanged (pv->m_pTemplate);
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"atcutouts")) {
            if (pv->m_pTemplate->m_pWorld)
               pv->m_pTemplate->m_pWorld->ObjectAboutToChange (pv->m_pTemplate);

            pv->m_fPostWantFullHeightCutout = p->pControl->AttribGetBOOL(Checked());

            // recalculate
            pv->DeterminePostLocations();
            if (pa->pBB)
               pa->pBB->BalustradeExtendToRoof ();

            if (pv->m_pTemplate->m_pWorld)
               pv->m_pTemplate->m_pWorld->ObjectChanged (pv->m_pTemplate);
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"hidefirst") ||!_wcsicmp(p->pControl->m_pszName, L"hidelast")) {
            if (pv->m_pTemplate->m_pWorld)
               pv->m_pTemplate->m_pWorld->ObjectAboutToChange (pv->m_pTemplate);

            if (!_wcsicmp(p->pControl->m_pszName, L"hidefirst"))
               pv->m_fHideFirst = p->pControl->AttribGetBOOL(Checked());
            else
               pv->m_fHideLast = p->pControl->AttribGetBOOL(Checked());

            if (pv->m_pTemplate->m_pWorld)
               pv->m_pTemplate->m_pWorld->ObjectChanged (pv->m_pTemplate);
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"nocolumns")) {
            if (pv->m_pTemplate->m_pWorld)
               pv->m_pTemplate->m_pWorld->ObjectAboutToChange (pv->m_pTemplate);
            pv->m_fNoColumns = p->pControl->AttribGetBOOL(Checked());
            // recalculate
            pv->DeterminePostLocations();
            if (pa->pBB)
               pa->pBB->BalustradeExtendToRoof ();

            if (pv->m_pTemplate->m_pWorld)
               pv->m_pTemplate->m_pWorld->ObjectChanged (pv->m_pTemplate);
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"swapsides")) {
            if (pv->m_pTemplate->m_pWorld)
               pv->m_pTemplate->m_pWorld->ObjectAboutToChange (pv->m_pTemplate);

            pv->m_fSwapSides = p->pControl->AttribGetBOOL(Checked());

            // recalculate
            pv->DeterminePostLocations();
            if (pa->pBB)
               pa->pBB->BalustradeExtendToRoof ();

            if (pv->m_pTemplate->m_pWorld)
               pv->m_pTemplate->m_pWorld->ObjectChanged (pv->m_pTemplate);
            return TRUE;
         }
      }
      break;   // default

   case ESCN_EDITCHANGE:
      {
         if (pv->m_pTemplate->m_pWorld)
            pv->m_pTemplate->m_pWorld->ObjectAboutToChange (pv->m_pTemplate);

         // since any edit box change will cause a change in parameters, get them all
         fp f;
         if (MeasureParseString (pPage, L"height", &f))
            pv->m_fHeight = max(.01, f);
         if (MeasureParseString (pPage, L"maxbigdist", &f))
            pv->m_fMaxPostDistanceBig = max (.1, f);
         if (MeasureParseString (pPage, L"maxsmalldist", &f))
            pv->m_fMaxPostDistanceSmall = max (.1, f);

         // recalculate
         pv->DeterminePostLocations();
         if (pa->pBB)
            pa->pBB->BalustradeExtendToRoof ();

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
            pv->NewSplines (&b, &t);

            pv->DeterminePostLocations();
            if (pa->pBB)
               pa->pBB->BalustradeExtendToRoof ();

            if (pv->m_pTemplate->m_pWorld)
               pv->m_pTemplate->m_pWorld->ObjectChanged (pv->m_pTemplate);

            PCEscControl pControl;
            pControl = pPage->ControlFind (L"modify");
            if (pControl)
               pControl->Enable (pv->m_dwBalStyle == BS_CUSTOM);
            return TRUE;
         }
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Balustrade appearance";
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}


/* BalCustomPage
*/
BOOL BalCustomPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PAPIN pa = (PAPIN)pPage->m_pUserData;
   PCBalustrade pv = pa->pBal;
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
         MeasureToString (pPage, L"postheightabovebalustrade", pv->m_fPostHeightAboveBalustrade);

         // horizontal rails
         for (i = 0; i < 4; i++) {
            swprintf (szTemp, L"horzrailuse%d", (int) i);
            pControl = pPage->ControlFind (szTemp);
            if (pControl)
               pControl->AttribSetBOOL (Checked(), pv->m_afHorzRailUse[i]);

            swprintf (szTemp, L"horzrailshape%d", (int) i);
            swprintf (szTemp2, L"%d", (int) pv->m_adwHorzRailShape[i]);
            lss.fExact = TRUE;
            lss.iStart = -1;
            lss.psz = szTemp2;
            pControl = pPage->ControlFind (szTemp);
            if (pControl)
               pControl->Message (ESCM_COMBOBOXSELECTSTRING, &lss);

            for (j = 0; j < 3; j++) {
               swprintf (szTemp, L"horzrailsize%d%d", i, j);
               MeasureToString (pPage, szTemp, pv->m_apHorzRailSize[i].p[j]);

               swprintf (szTemp, L"horzrailoffset%d%d", i, j);
               MeasureToString (pPage, szTemp, pv->m_apHorzRailOffset[i].p[j]);
            }
         }
         for (j = 0; j < 3; j++) {
            swprintf (szTemp, L"horzrailauto%d", j);
            MeasureToString (pPage, szTemp, pv->m_pHorzRailAuto.p[j]);
         }

         // panel
         pControl = pPage->ControlFind (L"panelused");
         if (pControl)
            pControl->AttribSetBOOL (Checked(), pv->m_fPanelUse);
         MeasureToString (pPage, L"paneloffset", pv->m_fPanelOffset);
         MeasureToString (pPage, L"panelthickness", pv->m_fPanelThickness);
         for (j = 0; j < 3; j++) {
            swprintf (szTemp, L"panelinfo%d", j);
            MeasureToString (pPage, szTemp, pv->m_pPanelInfo.p[j]);
         }

         // brace
         pControl = pPage->ControlFind (L"braceused");
         if (pControl)
            pControl->AttribSetBOOL (Checked(), pv->m_dwBrace ? TRUE : FALSE);
         MeasureToString (pPage, L"braceoffset", pv->m_fBraceOffset);
         for (j = 0; j < 3; j++) {
            swprintf (szTemp, L"bracesize%d", j);
            MeasureToString (pPage, szTemp, pv->m_pBraceSize.p[j]);
         }
         MeasureToString (pPage, L"bracetbh", pv->m_pBraceTB.h);
         MeasureToString (pPage, L"bracetbv", pv->m_pBraceTB.v);

         swprintf (szTemp2, L"%d", (int) (pv->m_dwBrace ? pv->m_dwBrace : 1));
         lss.fExact = TRUE;
         lss.iStart = -1;
         lss.psz = szTemp2;
         pControl = pPage->ControlFind (L"brace");
         if (pControl)
            pControl->Message (ESCM_COMBOBOXSELECTSTRING, &lss);
         swprintf (szTemp2, L"%d", (int) pv->m_dwBraceShape);
         lss.fExact = TRUE;
         lss.iStart = -1;
         lss.psz = szTemp2;
         pControl = pPage->ControlFind (L"braceshape");
         if (pControl)
            pControl->Message (ESCM_COMBOBOXSELECTSTRING, &lss);

         // verticals
         pControl = pPage->ControlFind (L"vertuse");
         if (pControl)
            pControl->AttribSetBOOL (Checked(), pv->m_fVertUse);
         pControl = pPage->ControlFind (L"vertusepoint");
         if (pControl)
            pControl->AttribSetBOOL (Checked(), pv->m_fVertUsePoint);
         swprintf (szTemp2, L"%d", (int) pv->m_dwVertPointTaper);
         lss.fExact = TRUE;
         lss.iStart = -1;
         lss.psz = szTemp2;
         pControl = pPage->ControlFind (L"vertpointtaper");
         if (pControl)
            pControl->Message (ESCM_COMBOBOXSELECTSTRING, &lss);
         for (i = 0; i < 2; i++) {
            swprintf (szTemp, L"vertshape%d", (int) i);
            swprintf (szTemp2, L"%d", (int) pv->m_adwVertShape[i]);
            lss.fExact = TRUE;
            lss.iStart = -1;
            lss.psz = szTemp2;
            pControl = pPage->ControlFind (szTemp);
            if (pControl)
               pControl->Message (ESCM_COMBOBOXSELECTSTRING, &lss);


            for (j = 0; j < 3; j++) {
               swprintf (szTemp, L"vertsize%d%d", i, j);
               MeasureToString (pPage, szTemp, pv->m_apVertSize[i].p[j]);
            }
         }

         MeasureToString (pPage, L"vertoffset", pv->m_fVertOffset);
         for (j = 0; j < 3; j++) {
            swprintf (szTemp, L"vertauto%d", j);
            MeasureToString (pPage, szTemp, pv->m_pVertAuto.p[j]);
         }

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
         MeasureParseString (pPage, L"postheightabovebalustrade", &pv->m_fPostHeightAboveBalustrade);

         // horizontal rails
         for (i = 0; i < 4; i++) {
            swprintf (szTemp, L"horzrailuse%d", (int) i);
            pControl = pPage->ControlFind (szTemp);
            if (pControl)
               pv->m_afHorzRailUse[i] = pControl->AttribGetBOOL (Checked());

            swprintf (szTemp, L"horzrailshape%d", (int) i);
            pControl = pPage->ControlFind (szTemp);
            if (pControl) {
               gi.dwIndex = pControl->AttribGetInt (CurSel());
               pControl->Message (ESCM_COMBOBOXGETITEM, &gi);
               if (gi.pszName)
                  pv->m_adwHorzRailShape[i] = _wtoi(gi.pszName);
            }

            for (j = 0; j < 3; j++) {
               swprintf (szTemp, L"horzrailsize%d%d", i, j);
               MeasureParseString (pPage, szTemp, &pv->m_apHorzRailSize[i].p[j]);
               pv->m_apHorzRailSize[i].p[j] = max(.01, pv->m_apHorzRailSize[i].p[j]);

               swprintf (szTemp, L"horzrailoffset%d%d", i, j);
               MeasureParseString (pPage, szTemp, &pv->m_apHorzRailOffset[i].p[j]);
            }
         }
         for (j = 0; j < 3; j++) {
            swprintf (szTemp, L"horzrailauto%d", j);
            MeasureParseString (pPage, szTemp, &pv->m_pHorzRailAuto.p[j]);
         }
         pv->m_pHorzRailAuto.p[2] = max (.01, pv->m_pHorzRailAuto.p[2]);

         // panel
         pControl = pPage->ControlFind (L"panelused");
         if (pControl)
            pv->m_fPanelUse = pControl->AttribGetBOOL (Checked());
         MeasureParseString (pPage, L"paneloffset", &pv->m_fPanelOffset);
         MeasureParseString (pPage, L"panelthickness", &pv->m_fPanelThickness);
         pv->m_fPanelThickness = max(0.0, pv->m_fPanelThickness);
         for (j = 0; j < 3; j++) {
            swprintf (szTemp, L"panelinfo%d", j);
            MeasureParseString (pPage, szTemp, &pv->m_pPanelInfo.p[j]);
         }

         // brace
         pControl = pPage->ControlFind (L"brace");
         if (pControl) {
            gi.dwIndex = pControl->AttribGetInt (CurSel());
            pControl->Message (ESCM_COMBOBOXGETITEM, &gi);
            if (gi.pszName)
               pv->m_dwBrace = _wtoi(gi.pszName);
         }
         pControl = pPage->ControlFind (L"braceused");
         if (pControl && !pControl->AttribGetBOOL (Checked()))
            pv->m_dwBrace = 0;
         MeasureParseString (pPage, L"braceoffset", &pv->m_fBraceOffset);
         for (j = 0; j < 3; j++) {
            swprintf (szTemp, L"bracesize%d", j);
            MeasureParseString (pPage, szTemp, &pv->m_pBraceSize.p[j]);
            pv->m_pBraceSize.p[j] = max(.01,pv->m_pBraceSize.p[j]);
         }
         fp fTemp;
         MeasureParseString (pPage, L"bracetbh", &fTemp);
         pv->m_pBraceTB.h = fTemp;
         MeasureParseString (pPage, L"bracetbv", &fTemp);
         pv->m_pBraceTB.v = fTemp;

         pControl = pPage->ControlFind (L"braceshape");
         if (pControl) {
            gi.dwIndex = pControl->AttribGetInt (CurSel());
            pControl->Message (ESCM_COMBOBOXGETITEM, &gi);
            if (gi.pszName)
               pv->m_dwBraceShape = _wtoi(gi.pszName);
         }

         // verticals
         pControl = pPage->ControlFind (L"vertuse");
         if (pControl)
            pv->m_fVertUse = pControl->AttribGetBOOL (Checked());
         pControl = pPage->ControlFind (L"vertusepoint");
         if (pControl)
            pv->m_fVertUsePoint = pControl->AttribGetBOOL (Checked());
         pControl = pPage->ControlFind (L"vertpointtaper");
         if (pControl) {
            gi.dwIndex = pControl->AttribGetInt (CurSel());
            pControl->Message (ESCM_COMBOBOXGETITEM, &gi);
            if (gi.pszName)
               pv->m_dwVertPointTaper = _wtoi(gi.pszName);
         }

         for (i = 0; i < 2; i++) {
            swprintf (szTemp, L"vertshape%d", (int) i);
            pControl = pPage->ControlFind (szTemp);
            if (pControl) {
               gi.dwIndex = pControl->AttribGetInt (CurSel());
               pControl->Message (ESCM_COMBOBOXGETITEM, &gi);
               if (gi.pszName)
                  pv->m_adwVertShape[i] = _wtoi(gi.pszName);
            }


            for (j = 0; j < 3; j++) {
               swprintf (szTemp, L"vertsize%d%d", i, j);
               MeasureParseString (pPage, szTemp, &pv->m_apVertSize[i].p[j]);
               pv->m_apVertSize[i].p[j] = max(.01, pv->m_apVertSize[i].p[j]);
            }
         }

         MeasureParseString (pPage, L"vertoffset", &pv->m_fVertOffset);
         for (j = 0; j < 3; j++) {
            swprintf (szTemp, L"vertauto%d", j);
            MeasureParseString (pPage, szTemp, &pv->m_pVertAuto.p[j]);
         }
         pv->m_pVertAuto.p[2] = max (.01, pv->m_pVertAuto.p[2]);


         // recalculate
         pv->DeterminePostLocations();
         if (pa->pBB)
            pa->pBB->BalustradeExtendToRoof ();

         if (pv->m_pTemplate->m_pWorld)
            pv->m_pTemplate->m_pWorld->ObjectChanged (pv->m_pTemplate);
      }
      return TRUE;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Custom balustrade appearance";
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}

/*************************************************************************************
CBalustrade::AppearancePage - Brings up a page that displays the appearance of
the balustrade.

inputs
   PCEscWindow    pWindow - window
   CObjectBuildBlock *pBB - If this balustrade is in a building block, pass in the
                  building block object so that posts can be extended to the roof.
returns
   PWSTR - Return string
*/
PWSTR CBalustrade::AppearancePage (PCEscWindow pWindow, CObjectBuildBlock *pBB)
{
   APIN a;
   memset (&a, 0, sizeof(a));
   a.pBal = this;
   a.pBB = pBB;

   PWSTR psz;
appear:
   psz = pWindow->PageDialog (ghInstance, IDR_MMLBALAPPEARANCE, ::BalAppearancePage, &a);

   if (psz && !_wcsicmp(psz, L"modify")) {
      psz = pWindow->PageDialog (ghInstance, IDR_MMLBALCUSTOM, ::BalCustomPage, &a);
      if (psz && !_wcsicmp(psz, Back()))
         goto appear;

      return psz;
   }
   else
      return psz;
}


/*************************************************************************************
CBalustrade::GenerateIntersect - Generates an intersection spline surface so can tell
when move control points and other bits.

inputs
   noen
returns
   none
*/
BOOL CBalustrade::GenerateIntersect (void)
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
      TEXTUREPOINT tp;
      m_psBottom->OrigPointGet (i, &pap[i + dwNum]);
      m_psBottom->OrigTextureGet (i, &tp);
      pap[i+dwNum].p[2] = tp.h;
      m_psTop->OrigPointGet (i, &pap[i]);
      m_psTop->OrigTextureGet (i, &tp);
      pap[i].p[2] = tp.h;
      m_psBottom->OrigSegCurveGet (i, &padw[i]);

      // extend top
      CPoint p;
      p.Subtract (&pap[i], &pap[i+dwNum]);
      p.Normalize();
      p.Scale (m_fHeight * 2.0);
      p.Add (&pap[i+dwNum]);
      pap[i].Copy (&p);
   }

   // create it
   if (!m_pssControlInter->ControlPointsSet (m_psBottom->LoopedGet(), FALSE, dwNum, 2,
      pap, padw, (DWORD*)SEGCURVE_LINEAR)) {

      delete m_pssControlInter;
      m_pssControlInter = NULL;
      return FALSE;
   }

   // done
   return TRUE;
}

  

/*************************************************************************************
CBalustrade::ControlPointQuery - Called to query the information about a control
point identified by dwID.

inputs
   DWORD       dwID - ID of control point
   POSCONTROL  pInfo - Filled in with the infomration
returns
   BOOL - TRUE if successful
*/
BOOL CBalustrade::ControlPointQuery (DWORD dwID, POSCONTROL pInfo)
{
   fp fKnobSize = .2;

   if (m_dwDisplayControl == 0) {
      // it's a corner
      DWORD dwNum = m_psOrigBottom->OrigNumPointsGet();
      if (dwID >= dwNum*2)
         return FALSE;

      memset (pInfo,0, sizeof(*pInfo));

      pInfo->dwID = dwID;
//      pInfo->dwFreedom = 0;   // any direction
      pInfo->dwStyle = CPSTYLE_CUBE;
      pInfo->fSize = fKnobSize;
      pInfo->cColor = RGB(0x00,0xff,0xff);

      // top and bottom
      CPoint pt, pb;
      m_psOrigBottom->OrigPointGet (dwID % dwNum, &pb);
      m_psOrigTop->OrigPointGet (dwID % dwNum, &pt);
      pt.Subtract (&pb);
      pt.Normalize();
      pt.Scale (m_fHeight * 2);
      pt.Add (&pb);

      pInfo->pLocation.Copy ((dwID < dwNum) ? &pb : &pt);

      // name
      wcscpy (pInfo->szName, L"Balustrade corner");

      return TRUE;
   }
   else {
      // it's an opening
      if (dwID >= m_lBALCUTOUT.Num()*2)
         return FALSE;

      // make sure have spline info
      if (!GenerateIntersect())
         return FALSE;

      memset (pInfo,0, sizeof(*pInfo));

      pInfo->dwID = dwID;
//      pInfo->dwFreedom = 0;   // any direction
      pInfo->dwStyle = CPSTYLE_POINTER;
      pInfo->fSize = fKnobSize;
      pInfo->cColor = RGB(0xff,0x00,0xff);

      // find out where this edge is
      PBALCUTOUT pc;
      float *paf;
      pc = (PBALCUTOUT) m_lBALCUTOUT.Get (dwID / 2);
      paf = &pc->tp.h;

      // convert this to space
      m_pssControlInter->HVToInfo ((fp) paf[dwID%2], .5, &pInfo->pLocation);
//      m_pssControlInter->HVToInfo ((fp) paf[dwID%2] - ((dwID % 2) ? CLOSE : -CLOSE), .5, &pInfo->pDirection);
//      pInfo->pDirection.Subtract (&pInfo->pLocation);
//      pInfo->pDirection.Normalize();

      // name
      wcscpy (pInfo->szName, L"Balustrade opening");

      return TRUE;
   }
}
/*************************************************************************************
CBalustrade::ControlPointSet - Called to change the valud of a control point.

inputs
   DWORD       dwID - ID
   PCPoint     pVal - Contains the new location, in object coordinates
   PCPoint     pViewer - Where the viewer is from
returns
   BOOL - TRUE if successful
*/
BOOL CBalustrade::ControlPointSet (DWORD dwID, PCPoint pVal, PCPoint pViewer,
                                   PCObjectBuildBlock pBB)
{
   if (m_dwDisplayControl == 0) {
      // it's a corner
      DWORD dwNum = m_psOrigBottom->OrigNumPointsGet();
      if (dwID >= dwNum*2)
         return FALSE;

      // get the current points
      CPoint pt, pb;
      m_psOrigBottom->OrigPointGet (dwID % dwNum, &pb);
      m_psOrigTop->OrigPointGet (dwID % dwNum, &pt);
      pt.Subtract (&pb);
      pt.Normalize();
      pt.Scale (m_fHeight * 2);
      pt.Add (&pb);

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
         m_psOrigBottom->OrigSegCurveGet (i, &padw[i]);
      }
      fLooped = m_psOrigBottom->LoopedGet ();
      m_psOrigBottom->DivideGet (&dwMin, &dwMax, &fDetail);

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
         pnb.p[2] = 0;

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
      pnt.Subtract (&pnb);
      pnt.Normalize();
      pnt.Add (&pnb);

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
      NewSplines (&sBottom, &sTop);

      // tell the world we've changed
      if (m_pTemplate->m_pWorld)
         m_pTemplate->m_pWorld->ObjectChanged (m_pTemplate);

      return FALSE;
   }
   else {
      // it's an opening
      if (dwID >= m_lBALCUTOUT.Num()*2)
         return FALSE;

      // make sure have all info that need
      if (!GenerateIntersect())
         return FALSE;

      // find out where this edge is now
      PBALCUTOUT pc;
      float *paf;
      pc = (PBALCUTOUT) m_lBALCUTOUT.Get (dwID / 2);
      paf = &pc->tp.h;
      TEXTUREPOINT pOld;
      pOld.h = paf[dwID%2];
      pOld.v = .5;

      // find new
      TEXTUREPOINT tpNew, tpNew2;
      if (!m_pssControlInter->HVDrag (&pOld, pVal, pViewer, &tpNew))
         return FALSE;  // doesnt intersect

      // try from the opposite direction since can see through balustrades
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
         if (dwID/2+1 < m_lBALCUTOUT.Num())
            paf[1] = min(paf[1], pc[1].tp.h);
      }
      else {
         // moved left
         paf[0] = min(paf[1], paf[0]);

         // overlays souldnt go over one another
         if (dwID/2)
            paf[0] = max(paf[0], paf[-1]);
      }

      // BUGFIX - Set flag saying that used mucked with and shouldnt intel afjust
      pc->fAuto = FALSE;

      // recalc
      DeterminePostLocations();
      if (pBB)
         pBB->BalustradeExtendToRoof ();

      // tell the world we've changed
      if (m_pTemplate->m_pWorld)
         m_pTemplate->m_pWorld->ObjectChanged (m_pTemplate);

      return TRUE;
   }
}


/*************************************************************************************
CBalustrade::ControlPointEnum - Called to enumerate a list of control point IDs
into the list.

inputs
   PCListFixed       plDWORD - Should be filled in with control points
returns
   none
*/
void CBalustrade::ControlPointEnum (PCListFixed plDWORD)
{
   DWORD i;
   DWORD dwPoints;

   if (m_dwDisplayControl == 0) {
      // showing corners
      dwPoints = m_psOrigBottom->OrigNumPointsGet() * 2;
   }
   else {
      // showing openings
      dwPoints = m_lBALCUTOUT.Num() * 2;
   }


   plDWORD->Required (plDWORD->Num() + dwPoints);
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


/* BalDisplayPage
*/
BOOL BalDisplayPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCBalustrade pv = (PCBalustrade)pPage->m_pUserData;

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




/* BalOpeningsPage
*/
BOOL BalOpeningsPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PAPIN pa = (PAPIN)pPage->m_pUserData;
   PCBalustrade pv = pa->pBal;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         // draw images
         GenerateThreeDForOpenings (L"edgeaddremove", pPage, pv->m_psOrigBottom, &pv->m_lPosts);
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
               for (i = 0; i < pv->m_lBALCUTOUT.Num(); i++) {
                  PBALCUTOUT pc = (PBALCUTOUT) pv->m_lBALCUTOUT.Get(i);
                  if (tc.v <= pc->tp.h)
                     break;
               };
               BALCUTOUT bc;
               memset (&bc, 0, sizeof(bc));
               bc.tp = tc;
               bc.fAuto = FALSE;
               if (i < pv->m_lBALCUTOUT.Num())
                  pv->m_lBALCUTOUT.Insert (i, &bc);
               else
                  pv->m_lBALCUTOUT.Add (&bc);
            }
         }
         else {
            // remove this cutout
            for (i = 0; i < pv->m_lBALCUTOUT.Num(); i++) {
               PBALCUTOUT pc = (PBALCUTOUT) pv->m_lBALCUTOUT.Get(i);
               if ((pbi[x].fLoc > pc->tp.h - CLOSE) && (pbi[x].fLoc < pc->tp.v + CLOSE))
                  break;
            }
            if (i < pv->m_lBALCUTOUT.Num())
               pv->m_lBALCUTOUT.Remove (i);
         }

         // recalculate
         pv->DeterminePostLocations();
         if (pa->pBB)
            pa->pBB->BalustradeExtendToRoof ();

         // world to change
         if (pv->m_pTemplate && pv->m_pTemplate->m_pWorld)
            pv->m_pTemplate->m_pWorld->ObjectChanged (pv->m_pTemplate);

         // redraw the shapes
         GenerateThreeDForOpenings (L"edgeaddremove", pPage, pv->m_psOrigBottom, &pv->m_lPosts);
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Balustrade openings";
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
         (double)p1.p[0], (double)p1.p[1], (double)0.0, (double)p2.p[0], (double)p2.p[1], (double)0.0);
      MemCat (&gMemTemp, szTemp);

      // do push point if more than 3 points
      if ((dwUse == 0) && (dwNum > 2)) {
         MemCat (&gMemTemp, L"<matrixpush>");
         swprintf (szTemp, L"<translate point=%g,%g,%g/>",
           (double) p1.p[0], (double)p1.p[1], (double)0.0);
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


/* BalCornersPage
*/
BOOL BalCornersPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCBalustrade pv = (PCBalustrade)pPage->m_pUserData;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         // draw images
         GenerateThreeDFromSpline (L"edgeaddremove", pPage, pv->m_psOrigBottom, 0);
         GenerateThreeDFromSpline (L"edgecurve", pPage, pv->m_psOrigBottom, 1);

         PCEscControl pControl;
         pControl = pPage->ControlFind (L"looped");
         if (pControl) {
            pControl->Enable (pv->m_psOrigBottom->OrigNumPointsGet() >= 3);
            pControl->AttribSetBOOL (Checked(), pv->m_psOrigBottom->LoopedGet());
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
         dwOrig = pv->m_psOrigBottom->OrigNumPointsGet();
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
            pv->m_psOrigBottom->OrigSegCurveGet (i, padw + i);
         DWORD dwMinDivide, dwMaxDivide;
         BOOL fLooped;
         fp fDetail;
         pv->m_psOrigBottom->DivideGet (&dwMinDivide, &dwMaxDivide, &fDetail);
         fLooped = pv->m_psOrigBottom->LoopedGet();

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
         pv->NewSplines (&sBottom, &sTop);
         pv->m_pTemplate->m_pWorld->ObjectChanged (pv->m_pTemplate);

         // redraw the shapes
         GenerateThreeDFromSpline (L"edgeaddremove", pPage, pv->m_psOrigBottom, 0);
         GenerateThreeDFromSpline (L"edgecurve", pPage, pv->m_psOrigBottom, 1);

         PCEscControl pControl;
         pControl = pPage->ControlFind (L"looped");
         if (pControl) {
            pControl->Enable (pv->m_psOrigBottom->OrigNumPointsGet() >= 3);
            pControl->AttribSetBOOL (Checked(), pv->m_psOrigBottom->LoopedGet());
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
            dwOrig = pv->m_psOrigBottom->OrigNumPointsGet();
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
               pv->m_psOrigBottom->OrigSegCurveGet (i, padw + i);
            DWORD dwMinDivide, dwMaxDivide;
            BOOL fLooped;
            fp fDetail;
            pv->m_psOrigBottom->DivideGet (&dwMinDivide, &dwMaxDivide, &fDetail);
            fLooped = p->pControl->AttribGetBOOL (Checked());

            // create the two splines
            CSpline sBottom, sTop;
            if (dwOrig < 3)
               fLooped = FALSE;
            sBottom.Init (fLooped, dwOrig, paPoints, NULL, padw, 0, 3, .1);
            sTop.Init (fLooped, dwOrig, paTop, NULL, padw, 0, 3, .1);

            pv->m_pTemplate->m_pWorld->ObjectAboutToChange (pv->m_pTemplate);
            pv->NewSplines (&sBottom, &sTop);
            pv->m_pTemplate->m_pWorld->ObjectChanged (pv->m_pTemplate);

            // redraw the shapes
            GenerateThreeDFromSpline (L"edgeaddremove", pPage, pv->m_psOrigBottom, 0);
            GenerateThreeDFromSpline (L"edgecurve", pPage, pv->m_psOrigBottom, 1);

            return TRUE;
         }

      }
      break;   // default

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Balustrade corners";
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}

/*************************************************************************************
CBalustrade::OpeningsPage - Brings up a page that displays the appearance of
the balustrade.

inputs
   PCEscWindow    pWindow - window
   CObjectBuildBlock *pBB - If this balustrade is in a building block, pass in the
                  building block object so that posts can be extended to the roof.
returns
   PWSTR - Return string
*/
PWSTR CBalustrade::OpeningsPage (PCEscWindow pWindow, CObjectBuildBlock *pBB)
{
   APIN a;
   memset (&a, 0, sizeof(a));
   a.pBal = this;
   a.pBB = pBB;

   PWSTR psz;
   psz = pWindow->PageDialog (ghInstance, IDR_MMLBALOPENINGS, ::BalOpeningsPage, &a);
   return psz;
}

/*************************************************************************************
CBalustrade::DisplayPage - Brings up a page that specifies what control points are visible.

inputs
   PCEscWindow    pWindow - window
returns
   PWSTR - Return string
*/
PWSTR CBalustrade::DisplayPage (PCEscWindow pWindow)
{
   PWSTR psz;
   psz = pWindow->PageDialog (ghInstance, IDR_MMLBALDISPLAY, ::BalDisplayPage, this);
   return psz;
}
    

/*************************************************************************************
CBalustrade::CornersPage - Modify corners.

inputs
   PCEscWindow    pWindow - window
returns
   PWSTR - Return string
*/
PWSTR CBalustrade::CornersPage (PCEscWindow pWindow)
{
   PWSTR psz;
   psz = pWindow->PageDialog (ghInstance, IDR_MMLBALCORNERS, ::BalCornersPage, this);
   return psz;
}
    
// FUTURERELEASE - Intersect two decks together. What do about posts at the intersections since
// they're not always right

// FUTURERELEASE - Should indent posts occuring at an edge of cutoff so they don't go off
// into the wall?

