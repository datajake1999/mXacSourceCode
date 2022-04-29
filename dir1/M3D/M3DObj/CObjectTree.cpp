/************************************************************************
CObjectTree.cpp - Draws a box.

begun 12/9/01 by Mike Rozak
Copyright 2001 Mike Rozak. All rights reserved
*/

#include <windows.h>
#include "escarpment.h"
#include "M3D.h"
#include "resource.h"

typedef struct {
   PCNoodle       pn;      // noodle to draw trunk
   fp         fThick;  // thickness at largest point - so that if tree far away dont draw
   COLORREF       cr;      // color
} TRUNKNOODLE, *PTRUNKNOODLE;

// leaf group - describes where leaf goes
typedef struct {
   CPoint      pLoc;       // location
   CPoint      pRot;       // XYZ rotation
   CPoint      pSize;      // size. [0]=width, [1]=length radiating out, [2]=height
   COLORREF    cColor;     // color
} LEAFGROUP, *PLEAFGROUP;

// flower - describes where flower goes
typedef struct {
   CPoint      pLoc;       // location
   CPoint      pRot;       // XYZ rotation
   fp      fSize;      // size.
} FLOWER, *PFLOWER;

class CTrunkNode;
typedef CTrunkNode *PCTrunkNode;
class CTrunkNode : public CEscObject {
public:
   CTrunkNode (void);
   ~CTrunkNode (void);
   PCTrunkNode Clone (void);
   PCTrunkNode NewBranch ();
   BOOL MergeBranches (DWORD dw1, DWORD dw2);
   BOOL MergeAllBranches (fp fDist);
   BOOL CreateNoodles (PCPoint pStart, fp fStartThick, COLORREF cBranch, PCObjectTree pTree, fp fTooThin);

   fp         m_fThickness;    // sum of the mass of leaves to this point
   CPoint         m_pPosn;         // position of the node right below the splits
   CListFixed     m_lSplit;        // list of pointers to other TrunkNodes, where split
};

typedef struct {
   PWSTR       pszName;    // name of the move point
   int         iX, iY; // -1 if for left/top, 1 for right/bottom
} SPLINEMOVEP, *PSPLINEMOVEP;

static SPLINEMOVEP   gaTreeMove[] = {
   L"Plant base", 0, 0
};

/*********************************************************************************
CTrunkNode::Constructor and destructor */
CTrunkNode::CTrunkNode (void)
{
   m_lSplit.Init (sizeof(PCTrunkNode));
   m_fThickness = 0;
   m_pPosn.Zero();
}
CTrunkNode::~CTrunkNode (void)
{
   // delete contents
   DWORD i;
   for (i = 0; i < m_lSplit.Num(); i++) {
      PCTrunkNode ptn = *((PCTrunkNode*) m_lSplit.Get(i));
      delete ptn;
   }
}

/*********************************************************************************
CTrunkNode::Clone - Clones the node.
*/
PCTrunkNode CTrunkNode::Clone (void)
{
   PCTrunkNode pNew = new CTrunkNode;
   if (!pNew)
      return pNew;

   pNew->m_fThickness = m_fThickness;
   pNew->m_pPosn.Copy (&m_pPosn);
   m_lSplit.Init (sizeof(PCTrunkNode), m_lSplit.Get(0), m_lSplit.Num());
   DWORD i, dwNum;
   PCTrunkNode *ppt;
   ppt = (PCTrunkNode*) pNew->m_lSplit.Get(0);
   dwNum = pNew->m_lSplit.Num();
   for (i = 0; i < dwNum; i++)
      ppt[i] = ppt[i]->Clone();

   return pNew;
}


/*********************************************************************************
CTrunkNode::NewBranch - Creates a new branch added to this node.

returns
   PCTrunkNode - New branch which is atuomatically attached to this one.
*/
PCTrunkNode CTrunkNode::NewBranch (void)
{
   PCTrunkNode pNew = new CTrunkNode;
   if (!pNew)
      return NULL;

   m_lSplit.Add (&pNew);
   return pNew;
}

/*********************************************************************************
CTrunkNode::MergeBranches - This merges two branches together. Int the process it:
   1) Sums their thickness.
   2) Averages positions according to thicknesses.
   2) Combines all their sub-branches into one list.
   3) Deletes branch dw2

inputs
   DWORD       dw1 - Index into m_lSplit of the first branch
   DWORD       dw2 - Index into m_lSplit fo the second branch
returns
   BOOL - TRUE if success
*/
BOOL CTrunkNode::MergeBranches (DWORD dw1, DWORD dw2)
{
   PCTrunkNode *ppt = (PCTrunkNode*) m_lSplit.Get(0);
   DWORD dwNum = m_lSplit.Num();
   if ((dw1 >= dwNum) || (dw2 >= dwNum) || (dw1 == dw2))
      return FALSE;

   // some
   ppt[dw1]->m_pPosn.Average (&ppt[dw2]->m_pPosn, ppt[dw2]->m_fThickness / (ppt[dw2]->m_fThickness+ppt[dw1]->m_fThickness));
   ppt[dw1]->m_fThickness += ppt[dw2]->m_fThickness;
   
   // move elems
   DWORD i;
   for (i = 0; i < ppt[dw2]->m_lSplit.Num(); i++) {
      PCTrunkNode ptn = *((PCTrunkNode*)ppt[dw2]->m_lSplit.Get(i));
      ppt[dw1]->m_lSplit.Add (&ptn);
   }
   ppt[dw2]->m_lSplit.Clear();
   delete ppt[dw2];
   m_lSplit.Remove (dw2);

   return TRUE;
}


/*********************************************************************************
CTrunkNode::MergeAllBranches - Merges all branches that end up being close together.
Then, calls those branches and has them merge their sub branches.

inputs
   fp      fDist - Acceptable distance. If closer than this then merges
returns
   BOOL - TRUE if OK
*/
BOOL CTrunkNode::MergeAllBranches (fp fDist)
{
   DWORD i, j;
   for (i = 0; i < m_lSplit.Num(); i++)
      for (j = m_lSplit.Num()-1; (j < m_lSplit.Num()) && (j > i); j--) {
         PCTrunkNode *ppt = (PCTrunkNode*) m_lSplit.Get(0);

         CPoint pDir;
         pDir.Subtract (&ppt[i]->m_pPosn, &ppt[j]->m_pPosn);
         if (pDir.Length() > fDist)
            continue;

         // merge them
         MergeBranches (i, j);
      }

   // using what's left, call merge all branches
   for (i = 0; i < m_lSplit.Num(); i++) {
      PCTrunkNode *ppt = (PCTrunkNode*) m_lSplit.Get(0);
      ppt[i]->MergeAllBranches (fDist);
   }
   return TRUE;
}


/******************************************************************************
CTrunkNode::CreateNoodles - Given a branch (and subbrances) this creates all
the noodles needed to draw the branch and adds them to pTree->m_lTRUNKNOODLE.

inputs
   PCPoint        pStart - Where this noodle starts
   fp         fStartThick - Starting thickness
   COLORREF       cBranch - Color to use for the branches
   PCObjectTree   pTree - To add to
   fp         fTooThin - If the thickness of the thickest branche (aka fStartThick)
                  is thinner than this then dont bother
returns
   BOOL - TRUE if success
*/
BOOL CTrunkNode::CreateNoodles (PCPoint pStart, fp fStartThick, COLORREF cBranch, PCObjectTree pTree, fp fTooThin)
{
   CListFixed  lLoc, lThick;
   // BUGFIX - Dont bother creating branches if they're too thin and will be eliminated later anyway
   if (fStartThick < fTooThin)
      return TRUE;

   lLoc.Init (sizeof(CPoint)); // list of location points
   lThick.Init (sizeof(CPoint)); // list of thicknesses

   // start this up where we're supposed to start
   CPoint p;
   lLoc.Add (pStart);
   p.Zero();
   p.p[0] = p.p[1] = pow(fStartThick, 1.0 / 2.0) * pTree->m_fTrunkThickness;
   lThick.Add (&p);

   // follow this branch all the way up, taking the longest paths
   PCTrunkNode ptn;
   DWORD i;
   DWORD dwExclude;
   dwExclude = -1;
   for (ptn = this; ptn; ) {
      // find the biggest branch
      DWORD dwBiggest = -1;
      fp fBiggest = 0;
      PCTrunkNode *ppt = (PCTrunkNode*) ptn->m_lSplit.Get(0);
      DWORD dwNum = ptn->m_lSplit.Num();
      for (i = 0; i < dwNum; i++) {
         if (ppt[i]->m_fThickness <= fBiggest)
            continue;
         fBiggest = ppt[i]->m_fThickness;
         dwBiggest = i;
      }

      // if no biggest then exit loop
      if (dwBiggest == -1)
         break;

      // go through all noodles that are not the biggest and tell them to be added
      for (i = 0; i < dwNum; i++) {
         if (i == dwBiggest)
            continue;
         ppt[i]->CreateNoodles (&ptn->m_pPosn, ppt[i]->m_fThickness, cBranch, pTree, fTooThin);
      }

      // store this position away?
      PCPoint pLast;
      pLast = NULL;
      if (lLoc.Num())
         pLast = (PCPoint) lLoc.Get(lLoc.Num()-1);
      if (pLast && !pLast->AreClose (&ptn->m_pPosn)) {
         lLoc.Add (&ptn->m_pPosn);
         p.Zero();
         p.p[0] = p.p[1] = pow(ptn->m_fThickness, 1.0 / 2.0) * pTree->m_fTrunkThickness;
         lThick.Add (&p);
      }

      // BUGFIX - Dont let branches taper to nothing, stop them at some point
      if (ppt[dwBiggest]->m_fThickness < fTooThin)
         break;

      // store this away
      lLoc.Add (&ppt[dwBiggest]->m_pPosn);
      p.Zero();
      p.p[0] = p.p[1] = pow(ppt[dwBiggest]->m_fThickness, 1.0 / 2.0) * pTree->m_fTrunkThickness;
      lThick.Add (&p);

      // move up the chain
      ptn = ppt[dwBiggest];
   }

   // if get here have a noodle going all the way
   if (lLoc.Num() < 2)
      return FALSE;

   PCNoodle pn;
   CPoint pFront;
   pFront.Zero();
   pFront.p[1] = -1;
   //pFront.p[2] = -1;
   pn = new CNoodle;
   if (!pn)
      return FALSE;
   // NOTE: Ignoreing bevelling
   pn->DrawEndsSet (FALSE);
   pn->FrontVector (&pFront);
   pn->PathSpline (FALSE, lLoc.Num(), (PCPoint) lLoc.Get(0), (DWORD*) SEGCURVE_LINEAR, 0);
   DWORD dwNum;
   PCPoint pThick;
   dwNum = lThick.Num();
   pThick = (PCPoint) lThick.Get(0);
   pn->ScaleSpline (dwNum, pThick, (DWORD*)SEGCURVE_LINEAR,0);
   pn->ShapeDefault (NS_CIRCLEFAST);

   // set the bevel to flat with the Tree if it's the bottom one
   if (pStart->p[2] == 0.0) {
      CPoint pBevel;
      pBevel.Zero();
      pBevel.p[2] = 1;
      pn->BevelSet (TRUE, 2, &pBevel);
   }

   TRUNKNOODLE tn;
   memset (&tn, 0, sizeof(tn));
   tn.cr = cBranch;
   tn.fThick = ((PCPoint) lThick.Get(0))->p[0];
   tn.pn = pn;
   pTree->m_lTRUNKNOODLE.Add (&tn);

   // done
   return TRUE;
}


/**********************************************************************************
CObjectTree::Constructor and destructor */
CObjectTree::CObjectTree (PVOID pParams, POSINFO pInfo)
{
   m_OSINFO = *pInfo;

   m_dwRenderShow = RENDERSHOW_PLANTS;



   // per tree
   m_dwStyle = LOWORD((DWORD)pParams);
   ParamFromStyle ();
   m_dwSRandExtra = 0;
   m_dwBirthday = MinutesToDFDATE(DFDATEToMinutes(gdwToday) - (__int64)(m_fDefaultAge * 60 * 24 * 365));

   // pot shape based on full height
   fp   fMax;
   m_dwPotShape = LOBYTE(HIWORD((DWORD)pParams));
   m_pPotSize.Zero();
   m_pPotSize.p[2] = m_afHeight[1] / 5;
   fMax = 0;
   DWORD i;
   for (i = 0; i < TREECAN+1; i++)
      fMax = max(fMax, m_afCanopyW[i]);
   //m_pPotSize.p[2] = max(m_pPotSize.p[2], m_afHeight[1] * fMax);
   m_pPotSize.p[2] = max(.2, m_pPotSize.p[2]);
   m_pPotSize.p[0] = m_pPotSize.p[2];
   m_pPotSize.p[1] = m_pPotSize.p[0] * 2.0 / 3.0;

   // generated
   m_fDirty = TRUE;
   m_lLEAFGROUP.Init (sizeof(LEAFGROUP));
   m_lTRUNKNOODLE.Init (sizeof(TRUNKNOODLE));
   m_lFLOWER.Init (sizeof(FLOWER));

   // color for the pot
   ObjectSurfaceAdd (10, RGB(0x80,0x40,0x0), MATERIAL_TILEMATTE, L"Pot");
}


CObjectTree::~CObjectTree (void)
{
   DWORD i;
   for (i = 0; i < m_lTRUNKNOODLE.Num(); i++) {
      PTRUNKNOODLE ptn = (PTRUNKNOODLE) m_lTRUNKNOODLE.Get(i);
      if (ptn->pn)
         delete ptn->pn;
   }
   m_lTRUNKNOODLE.Clear();
}


/**********************************************************************************
CObjectTree::Delete - Called to delete this object
*/
void CObjectTree::Delete (void)
{
   delete this;
}

/**********************************************************************************
CObjectTree::Render - Draws a test box.

inputs
   POBJECTRENDER     pr - Render information
*/
void CObjectTree::Render (POBJECTRENDER pr)
{
   // calculate tree if necessary
   CalcIfNecessary();

   // create the surface render object and draw
   CRenderSurface rs;
   CMatrix mObject;
   rs.Init (pr->pRS);
   ObjectMatrixGet (&mObject);
   rs.Multiply (&mObject);

   // if pot then will need to draw and translate the leaves
   if (m_dwPotShape) {
#define SOILHEIGHT         .95

      rs.Push();
      rs.SetDefMaterial (ObjectSurfaceFind (10), m_pWorld);

      switch (m_dwPotShape) {
      case 1:  // circular
         // draw the pot
         rs.Push();
         rs.Rotate (PI/2, 1);
         rs.ShapeFunnel (m_pPotSize.p[2], m_pPotSize.p[1], m_pPotSize.p[0], FALSE);
         rs.Pop();
         break;
      default:
      case 2:  // square
         CPoint ap[4];
         DWORD j;
         ap[0].Zero();
         ap[0].p[0] = -m_pPotSize.p[0] / 2;
         ap[0].p[1] = ap[0].p[0];
         ap[0].p[2] = m_pPotSize.p[2];
         ap[1].Copy (&ap[0]);
         ap[1].p[0] *= -1;
         ap[2].Copy (&ap[1]);
         ap[2].p[0] = m_pPotSize.p[1] / 2;
         ap[2].p[1] = -ap[2].p[0];
         ap[2].p[2] = 0;
         ap[3].Copy (&ap[2]);
         ap[3].p[0] *= -1;
         for (j = 0; j < 4; j++) {
            rs.Push();
            rs.Rotate (PI/2 * j, 3);
            rs.ShapeQuad (ap+0, ap+1, ap+2, ap+3, FALSE);
            rs.Pop();
         }
         break;
      }

      rs.Translate (0, 0, m_pPotSize.p[2] * SOILHEIGHT);
      RENDERSURFACE mat;
      memset (&mat, 0, sizeof(mat));
      rs.SetDefMaterial (&mat);

      // draw soild
      rs.SetDefColor (RGB(0,0,0));
      fp fSoil;
      fSoil = m_pPotSize.p[1] * (1.0 - SOILHEIGHT) + m_pPotSize.p[0] * SOILHEIGHT;
      fSoil *= .999; // just so doesnt go over edge
      switch (m_dwPotShape) {
      case 1:  // circular
         // draw the pot
         rs.Push();
         rs.Rotate (PI/2, 1);
         rs.ShapeFunnel (0, fSoil, CLOSE);
         rs.Pop();
      default:
      case 2:  // square
         rs.ShapeBox (-fSoil/2, -fSoil/2, 0, fSoil/2, fSoil/2, 0);
         break;
      }
   }

   // draw the leaves
   DWORD i, dwNum;
   PLEAFGROUP plg;
   plg = (PLEAFGROUP) m_lLEAFGROUP.Get(0);
   dwNum = m_lLEAFGROUP.Num();
   for (i = 0; i < dwNum; i++) {
      rs.Push();

      // move out to leave location
      rs.Translate (plg[i].pLoc.p[0], plg[i].pLoc.p[1], plg[i].pLoc.p[2]);


      // rotations
      rs.Rotate (plg[i].pRot.p[2], 3); // rotate around Z
      rs.Rotate (plg[i].pRot.p[1], 2); // rotate around Y, up and down
      rs.Rotate (plg[i].pRot.p[0], 1); // rotate around X, skew

      // object specific
      rs.SetDefColor (plg[i].cColor);

      CPoint ap[80];
      PCPoint pp, pp2;
      DWORD x,y;
      DWORD stx, sty;
      PCPoint p;
      BOOL fBump;
      DWORD dwIndex;
      fp fx;
      p = &plg[i].pSize;

      switch (m_dwLGShape) {
      case 2:  // leaf 1 - like grass blade
      case 3:  // leaf 2 - grass that's rounded at end
      case 4:  // leaf 3 - circular
      case 5:  // leaf 4 - fan
      case 6:  // stick out
         stx = 3;
         sty = 2;
         fx = max(max(p->p[0], p->p[1]), p->p[2]) / 20;  // BUGFIX - Was / 10;
         if ((m_dwLGShape == 4) || (m_dwLGShape == 5))
            fx *= 2; // need more detail for this
         if (rs.m_fDetail < fx)
            stx *= 2;
         for (x = 0; x < stx; x++) {
            pp = &ap[x];
            pp2 = &ap[stx+x];

            // grass-like leaf
            switch (m_dwLGShape) {
            default:
               pp->p[0] = (fp) x / (fp)(stx-1);
               break;
            case 3:
            case 5:
               pp->p[0] = sqrt((fp) x / (fp)(stx-1));
               break;
            case 4:
               pp->p[0] = (fp) x / (fp)(stx-1);
               break;
            }
            switch (m_dwLGShape) {
            default:
               pp->p[1] = sin(pp->p[0] * PI) + CLOSE;
               break;
            case 3:  // bulbous at end
               pp->p[1] = pp->p[0] * sqrt(1.0 - pp->p[0] * pp->p[0]) + CLOSE;
               break;
            case 4:
               fx = (pp->p[0] - .5) * 1.9;
               pp->p[1] = sqrt(1 - fx * fx);
               break;
            case 5:
               fx = pp->p[0] * .99;
               pp->p[1] = sqrt(1 - fx * fx);
               break;
            case 6:
               pp->p[1] = pp->p[0] + CLOSE;
               break;
            }
            switch (m_dwLGShape) {
            default:
               pp->p[2] = sin(pp->p[0] * PI) + CLOSE;
               break;
            case 5:
               pp->p[2] = sin(pp->p[0] * PI/2) + CLOSE;
               break;
            }

            // scale
            pp->p[0] *= p->p[0];
            pp->p[1] *= p->p[1]/2.0;
            pp->p[2] *= p->p[2];

            pp2->Copy (pp);
            pp2->p[1] *= -1;
         }
         pp = rs.NewPoints (&dwIndex, stx * sty);
         if (pp) {
            memcpy (pp, ap, stx * sty * sizeof(CPoint));
            rs.ShapeSurface (0, stx, sty, pp, dwIndex, NULL, 0, FALSE);
         }
         break;

      default:
      case 0:  // spherical thing
      case 1:  // bumpy
         fBump = (m_dwLGShape == 1);
         stx = 5;
         sty = 4;
         if (rs.m_fDetail < max(max(p->p[0], p->p[1]), p->p[2]) / 20) { // BUGFIX - Was /10
            stx *= 2;
            sty *= 2;
         }
         if (fBump)
            srand(i);
         for (x = 0; x < stx; x++) for (y = 0; y < sty; y++) {
            pp = &ap[y*stx+x];
            fp fRadK, fRadI;
            fRadK = PI/2 * (1.0-CLOSE) - y / (fp) (sty-1) * PI * (1.0-CLOSE);
            fRadI = (fp)x / (fp) stx * 2.0 * PI;
            pp->p[1] = cos(fRadI) * cos(fRadK) * p->p[1]/2;
            pp->p[2] = sin(fRadK) * p->p[2]/2;
            pp->p[0] = -sin(fRadI) * cos(fRadK) * p->p[0]/2;

            if (fBump && (y != 0) && (y != sty-1)) {
               pp->p[0] *= MyRand (.5, 1.5);
               pp->p[1] *= MyRand (.5, 1.5);
               pp->p[2] *= MyRand (.5, 1.5);
            }
         }
         pp = rs.NewPoints (&dwIndex, stx * sty);
         if (pp) {
            memcpy (pp, ap, stx * sty * sizeof(CPoint));
            rs.ShapeSurface (1, stx, sty, pp, dwIndex);
         }
         break;

      }
      rs.Pop();
   }

   // flowers
   PFLOWER pf;
   pf = (PFLOWER) m_lFLOWER.Get(0);
   dwNum = m_lFLOWER.Num();
   for (i = 0; i < dwNum; i++) {
      rs.Push();

      // move out to leave location
      rs.Translate (pf[i].pLoc.p[0], pf[i].pLoc.p[1], pf[i].pLoc.p[2]);


      // rotations
      rs.Rotate (pf[i].pRot.p[2], 3); // rotate around Z
      rs.Rotate (pf[i].pRot.p[1], 2); // rotate around Y, up and down
      // rs.Rotate (pf[i].pRot.p[0], 1); // rotate around X, skew

      // object specific
      rs.SetDefColor (m_cFlower);

      // just draw a box for now
      fp fs;
      fs = pf[i].fSize;

      switch (m_dwFlowerShape) {
      default:
      case 0:  // flattened sphere
         rs.ShapeEllipsoid (fs/4, fs/2, fs/2);
         break;
      case 1:  // sphere
         rs.ShapeEllipsoid (fs/2, fs/2, fs/2);
         break;
      case 2:  // cone
         rs.Rotate (-PI/2, 3);
         rs.ShapeFunnel (fs, fs/20, fs/2, FALSE);
         break;
      case 3:  // cone in cone
         rs.Rotate (-PI/2, 3);
         rs.ShapeFunnel (fs, fs/20, fs/2, FALSE);
         rs.ShapeFunnel (fs/2, fs/20, fs, FALSE);
         break;
      case 4:  // bowl
         CPoint pa[4];
         rs.Rotate (-PI/2, 3);
         memset (pa, 0, sizeof(pa));
         pa[2].p[0] = fs/4;
         pa[1].p[0] = fs/2;
         pa[1].p[1] = fs/2;
         pa[0].p[0] = fs/2;
         pa[0].p[1] = fs;
         rs.ShapeRotation (4, pa, TRUE, FALSE);
         break;
      }


      rs.Pop();
   }


   // do trunk
   for (i = 0; i < m_lTRUNKNOODLE.Num(); i++) {
      PTRUNKNOODLE ptn = (PTRUNKNOODLE) m_lTRUNKNOODLE.Get(i);
      rs.SetDefColor (ptn->cr);
      if (ptn->pn)
         ptn->pn->Render (pr, &rs);;
   }

   // if pot then pop
   if (m_dwPotShape)
      rs.Pop();
}

/**********************************************************************************
CObjectTree::Clone - Clones this

inputs
   none
returns
   PCObjectSocket - Clone
*/
PCObjectSocket CObjectTree::Clone (void)
{
   PCObjectTree pNew;

   pNew = new CObjectTree(NULL, &m_OSINFO);

   // clone template info
   CloneTemplate(pNew);

   // free up trunknoodle is have any
   DWORD i;
   for (i = 0; i < pNew->m_lTRUNKNOODLE.Num(); i++) {
      PTRUNKNOODLE ptn = (PTRUNKNOODLE) pNew->m_lTRUNKNOODLE.Get(i);
      if (ptn->pn)
         delete ptn->pn;
   }
   pNew->m_lTRUNKNOODLE.Clear();

   // Clone member variables
   pNew->m_dwSRandExtra = m_dwSRandExtra;
   pNew->m_dwStyle = m_dwStyle;
   pNew->m_dwBirthday = m_dwBirthday;
   pNew->m_fDefaultAge = m_fDefaultAge;
   pNew->m_fTrunkSkewVar = m_fTrunkSkewVar;
   memcpy (pNew->m_afAges, m_afAges, sizeof(m_afAges));
   memcpy (pNew->m_afHeight, m_afHeight, sizeof(m_afHeight));
   memcpy (pNew->m_afCanopyZ, m_afCanopyZ, sizeof(m_afCanopyZ));
   memcpy (pNew->m_afCanopyW, m_afCanopyW, sizeof(m_afCanopyW));
   pNew->m_dwLGShape = m_dwLGShape;
   memcpy (pNew->m_adwLGNum, m_adwLGNum, sizeof(m_adwLGNum));
   pNew->m_fLGRadius = m_fLGRadius;
   pNew->m_pLGSize.Copy (&m_pLGSize);
   memcpy (pNew->m_afLGSize, m_afLGSize, sizeof(m_afLGSize));
   memcpy (pNew->m_acLGColor, m_acLGColor, sizeof(m_acLGColor));
   memcpy (pNew->m_afLGDensBySeason, m_afLGDensBySeason, sizeof(m_afLGDensBySeason));
   pNew->m_fLGAngleUpLight = m_fLGAngleUpLight;
   pNew->m_fLGAngleUpHeavy = m_fLGAngleUpHeavy;
   pNew->m_fHeightVar = m_fHeightVar;
   pNew->m_fLGNumVar = m_fLGNumVar;
   pNew->m_fLGSizeVar = m_fLGSizeVar;
   pNew->m_fLGOrientVar = m_fLGOrientVar;

   pNew->m_fTrunkThickness = m_fTrunkThickness;
   pNew->m_fTrunkTightness = m_fTrunkTightness;
   pNew->m_fTrunkWobble = m_fTrunkWobble;
   pNew->m_fTrunkStickiness = m_fTrunkStickiness;
   memcpy (pNew->m_acTrunkColor, m_acTrunkColor, sizeof(m_acTrunkColor));

   pNew->m_fFlowerSize = m_fFlowerSize;
   pNew->m_fFlowerSizeVar = m_fFlowerSizeVar;
   pNew->m_fFlowerDist = m_fFlowerDist;
   pNew->m_fFlowersPerLG = m_fFlowersPerLG;
   pNew->m_dwFlowerShape = m_dwFlowerShape;
   pNew->m_cFlower = m_cFlower;
   pNew->m_fFlowerPeak = m_fFlowerPeak;
   pNew->m_fFlowerDuration = m_fFlowerDuration;

   pNew->m_dwPotShape = m_dwPotShape;
   pNew->m_pPotSize = m_pPotSize;

   pNew->m_fDirty = m_fDirty;
   pNew->m_lLEAFGROUP.Init (sizeof(LEAFGROUP), m_lLEAFGROUP.Get(0), m_lLEAFGROUP.Num());
   pNew->m_lFLOWER.Init(sizeof(FLOWER), m_lFLOWER.Get(0), m_lFLOWER.Num());

   pNew->m_lTRUNKNOODLE.Init (sizeof(TRUNKNOODLE), m_lTRUNKNOODLE.Get(0), m_lTRUNKNOODLE.Num());
   for (i = 0; i < pNew->m_lTRUNKNOODLE.Num(); i++) {
      PTRUNKNOODLE ptn = (PTRUNKNOODLE) pNew->m_lTRUNKNOODLE.Get(i);
      if (ptn->pn)
         ptn->pn = ptn->pn->Clone();
   }

   return pNew;
}


static PWSTR gpszSRandExtra = L"SRandExtra";
static PWSTR gpszBirthday = L"Birthday";
static PWSTR gpszPotShape = L"PotShape";
static PWSTR gpszPotSize = L"PotSize";
static PWSTR gpszStyle = L"Style";
static PWSTR gpszDefaultAge = L"DefaultAge";
static PWSTR gpszLGShape = L"LGShape";
static PWSTR gpszLGRadius = L"LGRadius";
static PWSTR gpszLGSize = L"LGSize";
static PWSTR gpszHeightVar = L"HeightVar";
static PWSTR gpszLGNumVar = L"LGNumVar";
static PWSTR gpszLGSizeVar = L"LGSizeVar";
static PWSTR gpszLGOrientVar = L"LGOrientVar";
static PWSTR gpszLGAngleUpLight = L"LGAngleUpLight";
static PWSTR gpszLGAngleUpHeavy = L"LGAngleUpHeavy";
static PWSTR gpszTrunkSkewVar = L"TrunkSkewVar";
static PWSTR gpszTrunkThickness = L"TrunkThickness";
static PWSTR gpszTrunkTightness = L"TrunkTightness";
static PWSTR gpszTrunkWobble = L"TrunkWobble";
static PWSTR gpszTrunkStickiness = L"TrunkStickiness";
static PWSTR gpszFlowersPerLG = L"FlowersPerLG";
static PWSTR gpszFlowerSize = L"FlowerSize";
static PWSTR gpszFlowerSizeVar = L"FlowerSizeVar";
static PWSTR gpszFlowerDist = L"FlowerDist";
static PWSTR gpszFlowerPeak = L"FlowerPeak";
static PWSTR gpszFlowerDuration = L"FlowerDuration";
static PWSTR gpszFlowerShape = L"FlowerShape";
static PWSTR gpszFlower = L"Flower";


PCMMLNode2 CObjectTree::MMLTo (void)
{
   // call into the template first
   PCMMLNode2 pNode = MMLToTemplate();
   if (!pNode)
      return NULL;

   MMLValueSet (pNode, gpszSRandExtra, (int)m_dwSRandExtra);
   MMLValueSet (pNode, gpszBirthday, (int)m_dwBirthday);
   MMLValueSet (pNode, gpszPotShape, (int)m_dwPotShape);
   MMLValueSet (pNode, gpszPotSize, &m_pPotSize);
   MMLValueSet (pNode, gpszStyle, (int)m_dwStyle);

   DWORD i, j;
   WCHAR szTemp[32];
   if (!m_dwStyle) {
      MMLValueSet (pNode, gpszDefaultAge, m_fDefaultAge);
      for (i = 0; i < TREEAGES; i++) {
         swprintf (szTemp, L"ages%d", (int)i);
         MMLValueSet (pNode, szTemp, m_afAges[i]);

         swprintf (szTemp, L"height%d", (int)i);
         MMLValueSet (pNode, szTemp, m_afHeight[i]);
      }
      for (i = 0; i < TREECAN; i++) {
         swprintf (szTemp, L"canopyz%d", (int)i);
         MMLValueSet (pNode, szTemp, m_afCanopyZ[i]);
      }
      for (i = 0; i < TREECAN+1; i++) {
         swprintf (szTemp, L"canopyw%d", (int)i);
         MMLValueSet (pNode, szTemp, m_afCanopyW[i]);
      }
      for (i = 0; i < TREEAGES; i++) for (j = 0; j < TREECAN; j++) {
         swprintf (szTemp, L"lgnum%d%d", (int)i, (int)j);
         MMLValueSet (pNode, szTemp, (int) m_adwLGNum[i][j]);
      }

      MMLValueSet (pNode, gpszLGShape, (int)m_dwLGShape);
      MMLValueSet (pNode, gpszLGRadius, m_fLGRadius);
      MMLValueSet (pNode, gpszLGSize, &m_pLGSize);

      for (i = 0; i < TREEAGES; i++) for (j = 0; j < TREECAN+1; j++) {
         swprintf (szTemp, L"lgsize%d%d", (int)i, (int)j);
         MMLValueSet (pNode, szTemp, m_afLGSize[i][j]);
      }

      for (i = 0; i < TREESEAS; i++) for (j = 0; j < 2; j++) {
         swprintf (szTemp, L"lgcolor%d%d", (int)i, (int)j);
         MMLValueSet (pNode, szTemp, (int) m_acLGColor[i][j]);

         swprintf (szTemp, L"trunkcolor%d%d", (int)i, (int)j);
         MMLValueSet (pNode, szTemp, (int) m_acTrunkColor[i][j]);
      }
      for (i = 0; i < TREESEAS; i++) {
         swprintf (szTemp, L"lgdensbyseason%d", (int)i);
         MMLValueSet (pNode, szTemp, m_afLGDensBySeason[i]);
      }

      MMLValueSet (pNode, gpszHeightVar, m_fHeightVar);
      MMLValueSet (pNode, gpszLGNumVar, m_fLGNumVar);
      MMLValueSet (pNode, gpszLGSizeVar, m_fLGSizeVar);
      MMLValueSet (pNode, gpszLGOrientVar, m_fLGOrientVar);
      MMLValueSet (pNode, gpszLGAngleUpLight, m_fLGAngleUpLight);
      MMLValueSet (pNode, gpszLGAngleUpHeavy, m_fLGAngleUpHeavy);
      MMLValueSet (pNode, gpszTrunkSkewVar, m_fTrunkSkewVar);
      MMLValueSet (pNode, gpszTrunkThickness, m_fTrunkThickness);
      MMLValueSet (pNode, gpszTrunkTightness, m_fTrunkTightness);
      MMLValueSet (pNode, gpszTrunkWobble, m_fTrunkWobble);
      MMLValueSet (pNode, gpszTrunkStickiness, m_fTrunkStickiness);

      MMLValueSet (pNode, gpszFlowersPerLG, m_fFlowersPerLG);
      MMLValueSet (pNode, gpszFlowerSize, m_fFlowerSize);
      MMLValueSet (pNode, gpszFlowerSizeVar, m_fFlowerSizeVar);
      MMLValueSet (pNode, gpszFlowerDist, m_fFlowerDist);
      MMLValueSet (pNode, gpszFlowerPeak, m_fFlowerPeak);
      MMLValueSet (pNode, gpszFlowerDuration, m_fFlowerDuration);
      MMLValueSet (pNode, gpszFlowerShape, (int)m_dwFlowerShape);
      MMLValueSet (pNode, gpszFlower, (int)m_cFlower);
   }

   return pNode;
}

BOOL CObjectTree::MMLFrom (PCMMLNode2 pNode)
{
   // call into the template first
   if (!MMLFromTemplate (pNode))
      return FALSE;

   m_dwSRandExtra = (DWORD) MMLValueGetInt (pNode, gpszSRandExtra, 0);
   m_dwBirthday = MMLValueGetInt (pNode, gpszBirthday, (int)m_dwBirthday);
   m_dwPotShape = MMLValueGetInt (pNode, gpszPotShape, 0);
   MMLValueGetPoint (pNode, gpszPotSize, &m_pPotSize);
   m_dwStyle = (DWORD) MMLValueGetInt (pNode, gpszStyle, TS_POTSUCCULANT);

   DWORD i, j;
   WCHAR szTemp[32];
   if (!m_dwStyle) {
      m_fDefaultAge = MMLValueGetDouble (pNode, gpszDefaultAge, 1);
      for (i = 0; i < TREEAGES; i++) {
         swprintf (szTemp, L"ages%d", (int)i);
         m_afAges[i] = MMLValueGetDouble (pNode, szTemp, i);

         swprintf (szTemp, L"height%d", (int)i);
         m_afHeight[i] = MMLValueGetDouble (pNode, szTemp, i);
      }
      for (i = 0; i < TREECAN; i++) {
         swprintf (szTemp, L"canopyz%d", (int)i);
         m_afCanopyZ[i] = MMLValueGetDouble (pNode, szTemp, i/5.0);
      }
      for (i = 0; i < TREECAN+1; i++) {
         swprintf (szTemp, L"canopyw%d", (int)i);
         m_afCanopyW[i] = MMLValueGetDouble (pNode, szTemp, i);
      }
      for (i = 0; i < TREEAGES; i++) for (j = 0; j < TREECAN; j++) {
         swprintf (szTemp, L"lgnum%d%d", (int)i, (int)j);
         m_adwLGNum[i][j] = MMLValueGetInt (pNode, szTemp, (int) 1);
      }

      m_dwLGShape = MMLValueGetInt (pNode, gpszLGShape, (int)0);
      m_fLGRadius = MMLValueGetDouble (pNode, gpszLGRadius, .1);
      MMLValueGetPoint (pNode, gpszLGSize, &m_pLGSize);

      for (i = 0; i < TREEAGES; i++) for (j = 0; j < TREECAN+1; j++) {
         swprintf (szTemp, L"lgsize%d%d", (int)i, (int)j);
         m_afLGSize[i][j] = MMLValueGetDouble (pNode, szTemp, 1);
      }

      for (i = 0; i < TREESEAS; i++) for (j = 0; j < 2; j++) {
         swprintf (szTemp, L"lgcolor%d%d", (int)i, (int)j);
         m_acLGColor[i][j] = MMLValueGetInt (pNode, szTemp, (int) 0);

         swprintf (szTemp, L"trunkcolor%d%d", (int)i, (int)j);
         m_acTrunkColor[i][j] = MMLValueGetInt (pNode, szTemp, (int) 0);
      }
      for (i = 0; i < TREESEAS; i++) {
         swprintf (szTemp, L"lgdensbyseason%d", (int)i);
         m_afLGDensBySeason[i] = MMLValueGetDouble (pNode, szTemp, 1);
      }

      m_fHeightVar = MMLValueGetDouble (pNode, gpszHeightVar, 0);
      m_fLGNumVar = MMLValueGetDouble (pNode, gpszLGNumVar, 0);
      m_fLGSizeVar = MMLValueGetDouble (pNode, gpszLGSizeVar, 0);
      m_fLGOrientVar = MMLValueGetDouble (pNode, gpszLGOrientVar, 0);
      m_fLGAngleUpLight = MMLValueGetDouble (pNode, gpszLGAngleUpLight, 0);
      m_fLGAngleUpHeavy = MMLValueGetDouble (pNode, gpszLGAngleUpHeavy, 0);
      m_fTrunkSkewVar = MMLValueGetDouble (pNode, gpszTrunkSkewVar, 0);
      m_fTrunkThickness = MMLValueGetDouble (pNode, gpszTrunkThickness, .1);
      m_fTrunkTightness = MMLValueGetDouble (pNode, gpszTrunkTightness, 0);
      m_fTrunkWobble = MMLValueGetDouble (pNode, gpszTrunkWobble, 0);
      m_fTrunkStickiness = MMLValueGetDouble (pNode, gpszTrunkStickiness, 0);

      m_fFlowersPerLG = MMLValueGetDouble (pNode, gpszFlowersPerLG, 0);
      m_fFlowerSize = MMLValueGetDouble (pNode, gpszFlowerSize, .1);
      m_fFlowerSizeVar = MMLValueGetDouble (pNode, gpszFlowerSizeVar, 0);
      m_fFlowerDist = MMLValueGetDouble (pNode, gpszFlowerDist, 0);
      m_fFlowerPeak = MMLValueGetDouble (pNode, gpszFlowerPeak, 0);
      m_fFlowerDuration = MMLValueGetDouble (pNode, gpszFlowerDuration, .1);
      m_dwFlowerShape = MMLValueGetInt (pNode, gpszFlowerShape, 0);
      m_cFlower = MMLValueGetInt (pNode, gpszFlower, (int)0);
   }
   else
      ParamFromStyle();

   // set dirty flag so recalculate
   m_fDirty = TRUE;
   return TRUE;
}

static int _cdecl TRUNKNOODLESort (const void *elem1, const void *elem2)
{
   TRUNKNOODLE *pdw1, *pdw2;
   pdw1 = (TRUNKNOODLE*) elem1;
   pdw2 = (TRUNKNOODLE*) elem2;

   if (pdw1->fThick > pdw2->fThick)
      return -1;
   else if (pdw1->fThick < pdw2->fThick)
      return 1;
   else
      return 0;
}

/**************************************************************************************
CObjectTree::CalcIfNecessary - If the dirty flag is set, this rebuilds the treee.
*/
void CObjectTree::CalcIfNecessary (void)
{
   // dirty flag
   if (!m_fDirty)
      return;

   // clear some stuff
   m_fDirty = FALSE;
   m_lLEAFGROUP.Clear();
   m_lFLOWER.Clear();
   DWORD i;
   for (i = 0; i < m_lTRUNKNOODLE.Num(); i++) {
      PTRUNKNOODLE ptn = (PTRUNKNOODLE) m_lTRUNKNOODLE.Get(i);
      if (ptn->pn)
         delete ptn->pn;
   }
   m_lTRUNKNOODLE.Clear();

   // find today's date
   DFDATE   dwToday;
   PWSTR psz;
   PCWorldSocket pWorld;
   // BUGFIX - So trees use date from current world not globals
   pWorld = m_pWorld ? m_pWorld : gpWorld;
   psz = pWorld->VariableGet (gpszWSDate);
   if (psz)
      dwToday = _wtoi(psz);
   else
      DefaultBuildingSettings (&dwToday);
      //dwToday = gdwToday;

   // calc age of tree
   fp fAge;
   fAge = (fp)(int)((DFDATEToMinutes(dwToday) - DFDATEToMinutes (m_dwBirthday)) / 60 / 24) / 365.0;
   if (fAge <= 0)
      return;  // it's just a seee

   // calc season
   psz = pWorld->VariableGet (gpszWSLatitude);
   BOOL fNorth;
   if (psz)
      fNorth = (psz[0] != L'-');
   else {
      // BUGFIX - Use defualt building
      fp fLat;
      DefaultBuildingSettings (NULL, NULL, &fLat);
      fNorth = (fLat >= 0);
   }
   fp fSeason;
   // BUGFIX - Was 4 : 10, changed to 5:11 so better fit
   fSeason = ((fp)MONTHFROMDFDATE(dwToday) - (fp)(fNorth ? 5 : 11)) / 3.0 +
      ((fp)DAYFROMDFDATE(dwToday) + 15) / 31.0 / 3.0;
   // NOTE: Mid-spring is on April 15
   if (fSeason < 0)
      fSeason += 4;
   int iSeasLow, iSeasHigh;
   fp fSeasLowAlpha, fSeasHighAlpha;
   iSeasLow = (int) floor(fSeason);
   iSeasLow = max(0,iSeasLow);
   iSeasLow = min(TREESEAS,iSeasLow);
   iSeasHigh = (iSeasLow + 1) % TREESEAS;
   fSeasLowAlpha = 1.0 - (fSeason - (fp)iSeasLow);
   fSeasLowAlpha = min(1,fSeasLowAlpha);
   fSeasLowAlpha = max(0,fSeasLowAlpha);
   fSeasHighAlpha = 1.0 - fSeasLowAlpha;

   // seed random based on GUID of tree, additional value
   DWORD dwSeed;
   DWORD *padw;
   DWORD j;
   dwSeed = m_dwSRandExtra;
   padw = (DWORD*) &m_gGUID;
   for (i = 0; i < sizeof(m_gGUID)/sizeof(DWORD); i++)
      dwSeed += padw[i];
   srand (dwSeed);

   // NOTE: It's important that from hereon whenever call rand() or MyRand() it
   // will be in a repeatable order

   // convert the age to a treeage number
   int iAgeLow, iAgeHigh;
   fp fAgeAlphaLow, fAgeAlphaHigh;
   if (fAge < m_afAges[0]) {
      iAgeLow = -1;
      fAgeAlphaHigh = fAge / m_afAges[0];
   }
   else if (fAge < m_afAges[1]) {
      iAgeLow = 0;
      fAgeAlphaHigh = (fAge - m_afAges[0]) / (m_afAges[1] - m_afAges[0]);
   }
   else if (fAge < m_afAges[2]) {
      iAgeLow = 1;
      fAgeAlphaHigh = (fAge - m_afAges[1]) / (m_afAges[2] - m_afAges[1]);
   }
   else {
      // cant grow any more beyond this point
      iAgeLow = 1;
      fAgeAlphaHigh = 1;
   }
   iAgeHigh = iAgeLow+1;
   fAgeAlphaHigh = min(1,fAgeAlphaHigh);
   fAgeAlphaHigh = max(0,fAgeAlphaHigh);
   fAgeAlphaLow = 1.0 - fAgeAlphaHigh;

   // figure out how high tree is
   fp fHeight;
   fHeight = ((iAgeLow >= 0) ? m_afHeight[iAgeLow] * fAgeAlphaLow : 0) + m_afHeight[iAgeHigh] * fAgeAlphaHigh;
   m_fHeightVar = min(.99,m_fHeightVar);
   m_fHeightVar = max(0,m_fHeightVar);
   fHeight = MyRand (fHeight * (1.0 - m_fHeightVar), fHeight * (1.0 + m_fHeightVar));

   // how much is the trunk skewed
   m_fTrunkSkewVar = max(0,m_fTrunkSkewVar);
   m_fTrunkSkewVar = min(1, m_fTrunkSkewVar);   // dont skew too much
   fp fSkewLat, fSkewLong;
   fSkewLong = MyRand(0, PI*2);
   fSkewLat = MyRand(0, m_fTrunkSkewVar) * PI/2;
   CPoint pSkew, pSkewNorm;
   pSkew.Zero();
   pSkew.p[0] = cos(fSkewLong) * sin(fSkewLat);
   pSkew.p[1] = sin(fSkewLong) * sin(fSkewLat);
   pSkew.p[2] = cos(fSkewLat);
   pSkewNorm.Copy (&pSkew);
   pSkewNorm.Scale (1.0 / pSkewNorm.p[2]);  // so that will go up with Z=1 units

   // NOTE: Not varrying canopy height/width

   // figure out how many leaves tree will have
   fp afLGNum[TREECAN], fNumLeaves, fSumLeaves;
   DWORD dwNumLeaves;
   fSumLeaves = 0;
   for (i = 0; i < TREECAN; i++) {
      afLGNum[i] = ((iAgeLow >= 0) ? (fp)m_adwLGNum[iAgeLow][i] * fAgeAlphaLow : 0) +
         (fp)m_adwLGNum[iAgeHigh][i] * fAgeAlphaHigh;
      afLGNum[i] = max(CLOSE, afLGNum[i]);
      fSumLeaves += afLGNum[i];
   }
   m_fLGNumVar = min(.99,m_fLGNumVar);
   m_fLGNumVar = max(0,m_fLGNumVar);
   fNumLeaves = floor (MyRand (fSumLeaves * (1.0 - m_fLGNumVar), fSumLeaves * (1.0 + m_fLGNumVar)) + .5);
   fNumLeaves = max(1,fNumLeaves);
   dwNumLeaves = (DWORD) fNumLeaves;

   // do all the leaves
   LEAFGROUP lg;
   m_fLGSizeVar = min(.99, m_fLGSizeVar);
   m_fLGSizeVar = max(0, m_fLGSizeVar);
   m_fLGOrientVar = min(.99, m_fLGOrientVar);
   m_fLGOrientVar = max(0,m_fLGOrientVar);
   for (i = 0; i < dwNumLeaves; i++) {
      memset (&lg, 0, sizeof(lg));

      // which canopy is it in?
      fp fCanopy;
      fCanopy = MyRand(0, fSumLeaves);
      for (j = 0; j < TREECAN; j++) {
         if (fCanopy <= afLGNum[j]) {
            fCanopy = fCanopy / afLGNum[j] + j;
            break;
         }

         // else next
         fCanopy -= afLGNum[j];
      }
      if (j >= TREECAN)
         fCanopy = TREECAN;

      // convert this a canopy min and max, and alpha
      int iCanLow, iCanHigh;
      fp fCanLowAlpha, fCanHighAlpha;
      iCanLow = (int) floor(fCanopy);
      iCanLow = max(0,iCanLow);
      iCanLow = min(TREECAN-1,iCanLow);
      iCanHigh = iCanLow + 1;
      fCanLowAlpha = 1.0 - (fCanopy - (fp)iCanLow);
      fCanLowAlpha = min(1,fCanLowAlpha);
      fCanLowAlpha = max(0,fCanLowAlpha);
      fCanHighAlpha = 1.0 - fCanLowAlpha;

      // how high is it in absolute terms then?
      lg.pLoc.p[2] = m_afCanopyZ[iCanLow] * fCanLowAlpha +
         ((iCanHigh < TREECAN) ? m_afCanopyZ[iCanHigh] : 1.0) * fCanHighAlpha;
      lg.pLoc.p[2] *= fHeight;

      // how far from center
      fp fRadius1, fRadius;
      fRadius1 = m_afCanopyW[iCanLow] * fCanLowAlpha + m_afCanopyW[iCanHigh] * fCanHighAlpha;
      fRadius1 /= 2.0;
         // can use iCanHigh directly because CanopyW has TREECAN+1 elems
      fRadius = MyRand(m_fLGRadius - 1, m_fLGRadius + 1);
      fRadius = max(0,fRadius);
      fRadius = min(1,fRadius);
      fRadius *= fHeight * fRadius1;

      // what angle
      fp fAngle;
      fAngle = MyRand (0, PI * 2);

      // rest of location
      lg.pLoc.p[0] = cos(fAngle) * fRadius;
      lg.pLoc.p[1] = sin(fAngle) * fRadius;

      // Deal with bend in tree's main trunk in offset?
      lg.pLoc.p[0] += pSkew.p[0] * lg.pLoc.p[2];
      lg.pLoc.p[1] += pSkew.p[1] * lg.pLoc.p[2];
      lg.pLoc.p[2] *= pSkew.p[2];

      // coloration
      COLORREF cr[2];
      for (j = 0; j < 2; j++) {
         fp fr, fg, fb;
         fr = (fp)GetRValue(m_acLGColor[iSeasLow][j]) * fSeasLowAlpha +
            (fp)GetRValue(m_acLGColor[iSeasHigh][j]) * fSeasHighAlpha;
         fg = (fp)GetGValue(m_acLGColor[iSeasLow][j]) * fSeasLowAlpha +
            (fp)GetGValue(m_acLGColor[iSeasHigh][j]) * fSeasHighAlpha;
         fb = (fp)GetBValue(m_acLGColor[iSeasLow][j]) * fSeasLowAlpha +
            (fp)GetBValue(m_acLGColor[iSeasHigh][j]) * fSeasHighAlpha;

         cr[j] = RGB((BYTE)fr, (BYTE)fg, (BYTE)fb);
      }
      fp fBlend;
      fBlend = MyRand(0,1);
      lg.cColor = RGB(
         (BYTE)(fBlend * GetRValue(cr[0]) + (1.0 - fBlend) * GetRValue(cr[1])),
         (BYTE)(fBlend * GetGValue(cr[0]) + (1.0 - fBlend) * GetGValue(cr[1])),
         (BYTE)(fBlend * GetBValue(cr[0]) + (1.0 - fBlend) * GetBValue(cr[1]))
         );

      // figure out the size
      fp fSize, fSizeLow, fSizeHigh;
      int iAgeTemp;
      lg.pSize.p[0] = m_pLGSize.p[0] * MyRand(1.0 - m_fLGSizeVar, 1.0 + m_fLGSizeVar);
      lg.pSize.p[1] = m_pLGSize.p[1] * MyRand(1.0 - m_fLGSizeVar, 1.0 + m_fLGSizeVar);
      lg.pSize.p[2] = m_pLGSize.p[2] * MyRand(1.0 - m_fLGSizeVar, 1.0 + m_fLGSizeVar);
      iAgeTemp = max(0,iAgeLow);
      fSizeLow = m_afLGSize[iAgeTemp][iCanLow] * fCanLowAlpha +
         m_afLGSize[iAgeTemp][iCanHigh] * fCanHighAlpha;
      fSizeHigh = m_afLGSize[iAgeHigh][iCanLow] * fCanLowAlpha +
         m_afLGSize[iAgeHigh][iCanHigh] * fCanHighAlpha;
      fSize = fSizeLow * fAgeAlphaLow + fSizeHigh * fAgeAlphaHigh;
      lg.pSize.Scale (fSize);

      // mass?
      fp fMass, fMassDef;
      fMassDef = max(m_pLGSize.p[0] * m_pLGSize.p[1] * m_pLGSize.p[2], CLOSE);
      fMass = (lg.pSize.p[0] * lg.pSize.p[1] * lg.pSize.p[2]);
      
      // min and max for the age
      fp fMinMass, fMaxMass, fTemp;
      for (j = 0; j <= TREECAN; j++) {
         fTemp = m_afLGSize[iAgeTemp][j] * fAgeAlphaLow + m_afLGSize[iAgeHigh][j] * fAgeAlphaHigh;
         if (j) {
            fMinMass = min(fTemp, fMinMass);
            fMaxMass = max(fTemp, fMaxMass);
         }
         else
            fMinMass = fMaxMass = fTemp;
      }
      fMassDef = pow(fMassDef, 1.0 / 3.0);   // cubed root
      fMinMass *= fMassDef * (1.0 - m_fLGSizeVar);
      fMaxMass *= fMassDef  * (1.0 + m_fLGSizeVar);
      fMaxMass = max(fMaxMass, fMinMass + CLOSE);
      fMass = pow(fMass, 1.0 / 3.0);   // cubed root
      lg.pRot.p[1] = -((fMass - fMinMass) / (fMaxMass - fMinMass) * (m_fLGAngleUpHeavy - m_fLGAngleUpLight) +
         m_fLGAngleUpLight);

      // angle out
      lg.pRot.p[2] = fAngle;

      // random variations
      lg.pRot.p[0] += MyRand(- m_fLGOrientVar, + m_fLGOrientVar) * PI;
      lg.pRot.p[1] += MyRand(-m_fLGOrientVar,m_fLGOrientVar)*PI/2;
      lg.pRot.p[2] += MyRand(-m_fLGOrientVar,m_fLGOrientVar)*PI;

      // add it
      m_lLEAFGROUP.Add (&lg);
   }

   // put all the flowers in
   fp fNumFlowers;
   DWORD dwNumFlowers;
   fNumFlowers = m_lLEAFGROUP.Num() * m_fFlowersPerLG;
   fNumFlowers = MyRand(1.0 - m_fLGNumVar, 1.0 + m_fLGNumVar) * fNumFlowers;
   dwNumFlowers = (DWORD) fNumFlowers;
   m_fFlowerSizeVar = max(0,m_fFlowerSizeVar);
   m_fFlowerSizeVar = min(.99,m_fFlowerSizeVar);
   for (i = 0; i < dwNumFlowers; i++) {
      PLEAFGROUP plg = (PLEAFGROUP) m_lLEAFGROUP.Get (rand() % m_lLEAFGROUP.Num());
      FLOWER f;
      memset (&f, 0, sizeof(f));
      f.fSize = MyRand(1.0 - m_fFlowerSizeVar, 1.0 + m_fFlowerSizeVar) * m_fFlowerSize;

      // location
      f.pLoc.Copy (&plg->pLoc);
      for (j = 0; j < 3; j++)
         f.pLoc.p[j] += MyRand(-plg->pSize.p[j]*m_fFlowerDist, plg->pSize.p[j]*m_fFlowerDist);

      // orientation
      fp fFlowerHeight;
      fFlowerHeight = f.pLoc.p[2];
      fFlowerHeight = min(fHeight, fFlowerHeight);
      fFlowerHeight = max(0,fFlowerHeight);
      f.pRot.p[2] = plg->pRot.p[2];
      f.pRot.p[1] = -((fFlowerHeight / fHeight) - .5) * PI/2;

      m_lFLOWER.Add (&f);
   }

   // calculate the wobble
#define WOBBLEFREQ   4
   fp   afWobble[WOBBLEFREQ][2];

   // make the trunks
   CTrunkNode tn;
#define IDIVIDE      10    // divide tree into 20 vertical segments
   fp fUnit;
   fUnit = fHeight / IDIVIDE;
   for (i = 0; i < m_lLEAFGROUP.Num(); i++) {
      PLEAFGROUP plg = (PLEAFGROUP) m_lLEAFGROUP.Get(i);

      // what's the volume of the leave
      fp fVolume;
      fVolume = max(CLOSE, plg->pSize.p[0]) * max(CLOSE, plg->pSize.p[1]) * max(CLOSE, plg->pSize.p[2]);

      // wobble
      DWORD k;
      for (j = 0; j < WOBBLEFREQ; j++) for (k = 0; k < 2; k++) {
         fp fAmp;
         fAmp = m_fTrunkWobble / (fp) (1 << j);
         fAmp *= plg->pLoc.p[2];
         afWobble[j][k] = MyRand(-fAmp, fAmp);
      }

      // create one trunk per leaf at first and merge them later
      // of course, one trunk made of several smaller bits
      fp fCur;
      PCTrunkNode pCur, pNew;
      for (pCur = &tn, fCur = 0; fCur < plg->pLoc.p[2]; fCur += fUnit) {
         pCur->m_fThickness += fVolume;

         pNew = pCur->NewBranch();
         if (!pNew)
            break;

         // where is the branch
         CPoint pLoc;
         pLoc.Copy (&plg->pLoc);
         pLoc.Scale (fCur / plg->pLoc.p[2]);

         // Put wobble in
         for (j = 0; j < WOBBLEFREQ; j++) for (k = 0; k < 2; k++) {
            pLoc.p[k] += sin(fCur / plg->pLoc.p[2] * PI * (fp)j) * afWobble[j][k];
         }

         // where is the main trunk
         CPoint pTrunk;
         pTrunk.Copy (&pSkewNorm);
         pTrunk.Scale (fCur);

         // weighted average of branch and main trunk location
         fp fWeight, fWeightPow;
         fWeight = fCur / plg->pLoc.p[2];
         fWeightPow = pow(fWeight, m_fTrunkTightness * 5);
         pNew->m_pPosn.Average (&pLoc, &pTrunk, fWeightPow);

         pCur = pNew;
      }

      // add the final branch
      pCur->m_fThickness += fVolume;
      pNew = pCur->NewBranch();
      if (pNew) {
         pNew->m_fThickness = fVolume;
         pNew->m_pPosn.Copy (&plg->pLoc);
      }
   }

   // merge all the branches toegher
   tn.MergeAllBranches (fHeight * m_fTrunkStickiness);

   // Figure out branch color
   COLORREF cBranch;
   COLORREF cr[2];
   for (j = 0; j < 2; j++) {
      fp fr, fg, fb;
      fr = (fp)GetRValue(m_acTrunkColor[iSeasLow][j]) * fSeasLowAlpha +
         (fp)GetRValue(m_acTrunkColor[iSeasHigh][j]) * fSeasHighAlpha;
      fg = (fp)GetGValue(m_acTrunkColor[iSeasLow][j]) * fSeasLowAlpha +
         (fp)GetGValue(m_acTrunkColor[iSeasHigh][j]) * fSeasHighAlpha;
      fb = (fp)GetBValue(m_acTrunkColor[iSeasLow][j]) * fSeasLowAlpha +
         (fp)GetBValue(m_acTrunkColor[iSeasHigh][j]) * fSeasHighAlpha;

      cr[j] = RGB((BYTE)fr, (BYTE)fg, (BYTE)fb);
   }
   fp fBlend;
   fBlend = MyRand(0,1);
   cBranch = RGB(
      (BYTE)(fBlend * GetRValue(cr[0]) + (1.0 - fBlend) * GetRValue(cr[1])),
      (BYTE)(fBlend * GetGValue(cr[0]) + (1.0 - fBlend) * GetGValue(cr[1])),
      (BYTE)(fBlend * GetBValue(cr[0]) + (1.0 - fBlend) * GetBValue(cr[1]))
      );

   // create noodles for those
   // Use /100.0 becausse = 1.0 / (10 ^ 2)
   tn.CreateNoodles (&tn.m_pPosn, tn.m_fThickness, cBranch, this, tn.m_fThickness / 100.0);

   // eliminate any noodles that are much thinner than biggest one, or if there
   // are too many
   DWORD dwNum;
   PTRUNKNOODLE ptn;
   dwNum = m_lTRUNKNOODLE.Num();
   ptn = (PTRUNKNOODLE) m_lTRUNKNOODLE.Get(0);
   qsort (ptn, dwNum, sizeof(TRUNKNOODLE), TRUNKNOODLESort);
   for (i = 0; i < 10; i++) {
      if (ptn[i].fThick < ptn[0].fThick / 10.0)
         break;
   }
   while (i < m_lTRUNKNOODLE.Num()) {
      ptn = (PTRUNKNOODLE) m_lTRUNKNOODLE.Get(m_lTRUNKNOODLE.Num()-1);
      if (ptn->pn)
         delete ptn->pn;
      m_lTRUNKNOODLE.Remove(m_lTRUNKNOODLE.Num()-1);
   }

   // eliminate leaves based on season (last in list)
   fp fLow, fHigh;
   fLow = m_afLGDensBySeason[iSeasLow];
   fHigh = m_afLGDensBySeason[iSeasHigh];
   if (!fLow)  // if it's zero, make sure to push leaves to 0
      fLow = -fHigh;
   if (!fHigh)  // if it's zero, make sure to push leaves to 0
      fHigh = -fLow;
   fNumLeaves = (fNumLeaves+1) * (fLow * fSeasLowAlpha +
      fHigh * fSeasHighAlpha);
   fNumLeaves = max(0,fNumLeaves);
   dwNumLeaves = (DWORD) fNumLeaves;
   while (dwNumLeaves < m_lLEAFGROUP.Num())
      m_lLEAFGROUP.Remove(m_lLEAFGROUP.Num()-1);

   // Remove flowers when not right time of year
   fp fFromPeak;
   fFromPeak = fabs(m_fFlowerPeak - fSeason);
   if (fFromPeak > 2)
      fFromPeak = 4-fFromPeak;
   fNumFlowers = (1.0 - fFromPeak / (m_fFlowerDuration / 2 * 4)) * (fp) m_lFLOWER.Num();
   if (iAgeLow == -1)
      fNumFlowers = 0;  // no flowers when youg
   else if (iAgeLow == 0)
      fNumFlowers *= fAgeAlphaHigh; // only partial
   dwNumFlowers = (fNumFlowers > 0) ? (DWORD)fNumFlowers : 0;
   if (!dwNumFlowers)
      m_lFLOWER.Clear();
   while (dwNumFlowers < m_lFLOWER.Num())
      m_lFLOWER.Remove (m_lFLOWER.Num()-1);
}

/**********************************************************************************
CObjectTree::Message -
sends a message to the object. The interpretation of the message depends upon
dwMessage, which is OSM_XXX. If the function understands and handles the
message it returns TRUE, otherwise FALE.
*/
BOOL CObjectTree::Message (DWORD dwMessage, PVOID pParam)
{
   switch (dwMessage) {
   case OSM_NEWLATITUDE:
      // tell object changing so can be redrawn for new season
      m_pWorld->ObjectAboutToChange (this);
      m_fDirty = TRUE;
      m_pWorld->ObjectChanged (this);
      return TRUE;

   case OSM_IGNOREWORLDBOUNDINGBOXGET:
      // BUGFIX - So wont affect view from NSEW distance
      return TRUE;

   }

   return FALSE;
}


/**********************************************************************************
CObjectTree::ParamFromStyle - Sets the parameters up based upon the style
*/
void CObjectTree::ParamFromStyle (void)
{
   // if it's custom do nothing
   if (!m_dwStyle)
      return;

   DWORD i, j;

   m_fDefaultAge = 15;
   m_afAges[0] = 1;
   m_afAges[1] = 10;
   m_afAges[2] = 30;
   m_afHeight[0] = 1.5;
   m_afHeight[1] = 6.0;
   m_afHeight[2] = 10.0;
   m_afCanopyZ[0] = .5;
   m_afCanopyZ[1] = .6;
   m_afCanopyZ[2] = .7;
   m_afCanopyW[0] = .1 * 2;
   m_afCanopyW[1] = .3 * 2;
   m_afCanopyW[2] = .4 * 2;
   m_afCanopyW[3] = .3 * 2;
   m_dwLGShape = 1;
   m_adwLGNum[0][0] = m_adwLGNum[0][1] = m_adwLGNum[0][2] = 2*5;
   m_adwLGNum[1][0] = m_adwLGNum[1][1] = m_adwLGNum[1][2] = 4*5;
   m_adwLGNum[2][0] = m_adwLGNum[2][1] = m_adwLGNum[2][2] = 6*5;
   m_fLGRadius = 1.5;
   m_pLGSize.Zero();
   m_pLGSize.p[0] = 2;
   m_pLGSize.p[1] = 2;
   m_pLGSize.p[2] = 1;
   for (i = 0; i < TREEAGES; i++) for (j = 0; j <= TREECAN; j++)
      m_afLGSize[i][j] = 1;
      //m_afLGSize[i][j] = (fp)((TREECAN + 1) - j) / (fp)(TREECAN+1) * 2;
   m_acLGColor[0][0] = RGB(0x80,0xff,0x80);
   m_acLGColor[0][1] = RGB(0x40,0xc0,0x40);
   m_acLGColor[1][0] = RGB(0x10, 0xff,0x10);
   m_acLGColor[1][1] = RGB(0x10, 0x80,0x10);
   m_acLGColor[2][0] = RGB(0xff, 0xff, 0x00);
   m_acLGColor[2][1] = RGB(0x80, 0x80, 0x00);
   m_acLGColor[3][0] = RGB(0x80, 0x80,0x00);
   m_acLGColor[3][1] = RGB(0x40, 0x40,0x00);
   m_afLGDensBySeason[0] = m_afLGDensBySeason[2] = 1.0;//.8;
   m_afLGDensBySeason[1] = 1;
   m_afLGDensBySeason[3] = .5;
   m_fLGAngleUpLight = PI/8;
   m_fLGAngleUpHeavy = -PI/8;
   m_fHeightVar = .3;
   m_fLGNumVar = .3;
   m_fLGSizeVar = .3;
   m_fLGOrientVar = .1;
   m_fTrunkSkewVar = .1;

   m_fTrunkThickness = .05;
   m_fTrunkTightness = .05;
   m_fTrunkWobble = .1;
   m_fTrunkStickiness = .2;
   for (i = 0; i < TREESEAS; i++) {
      m_acTrunkColor[i][0] = RGB(0x40,0x40,0x00);
      m_acTrunkColor[i][1] = RGB(0x80,0x80,0x40);
   }

   m_fFlowersPerLG = 2;
   m_fFlowerSize = .1;
   m_fFlowerSizeVar = .2;
   m_fFlowerDist = .1;
   m_dwFlowerShape = 4;
   m_cFlower = RGB(0xff,0,0);
   m_fFlowerPeak = 2;
   m_fFlowerDuration = .1;

   switch (m_dwStyle) {
   default:
   case TS_POTSUCCULANT:      // potted, succulant
      m_fDefaultAge = 3;
      m_afAges[0] = 1;
      m_afHeight[0] = 0.01;
      m_afAges[1] = 3;
      m_afHeight[1] = 0.03;
      m_afAges[2] = 5;
      m_afHeight[2] = 0.05;
      m_afCanopyZ[0] = 0.01;
      m_afCanopyZ[1] = 0.3;
      m_afCanopyZ[2] = 0.6;
      m_afCanopyW[0] = 5;
      m_afCanopyW[1] = 6;
      m_afCanopyW[2] = 6;
      m_afCanopyW[3] = 6;
      m_dwLGShape = 2;
      m_adwLGNum[0][0] = 10;
      m_adwLGNum[0][1] = 10;
      m_adwLGNum[0][2] = 10;
      m_adwLGNum[1][0] = 20;
      m_adwLGNum[1][1] = 20;
      m_adwLGNum[1][2] = 20;
      m_adwLGNum[2][0] = 20;
      m_adwLGNum[2][1] = 20;
      m_adwLGNum[2][2] = 20;
      m_fLGRadius = 0.9;
      m_pLGSize.p[0] = 2;
      m_pLGSize.p[1] = 0.1;
      m_pLGSize.p[2] = 0.1;
      m_afLGSize[0][0] = 0.15;
      m_afLGSize[0][1] = 0.2;
      m_afLGSize[0][2] = 0.25;
      m_afLGSize[0][3] = 0.3;
      m_afLGSize[1][0] = 0.45;
      m_afLGSize[1][1] = 0.5;
      m_afLGSize[1][2] = 0.55;
      m_afLGSize[1][3] = 0.6;
      m_afLGSize[2][0] = 0.7;
      m_afLGSize[2][1] = 0.8;
      m_afLGSize[2][2] = 0.9;
      m_afLGSize[2][3] = 1;
      m_acLGColor[0][0] = 0x4000;
      m_acTrunkColor[0][0] = 0x25405a;
      m_acLGColor[0][1] = 0x74dc74;
      m_acTrunkColor[0][1] = 0x8000;
      m_acLGColor[1][0] = 0x4000;
      m_acTrunkColor[1][0] = 0x25405a;
      m_acLGColor[1][1] = 0x66d966;
      m_acTrunkColor[1][1] = 0x8000;
      m_acLGColor[2][0] = 0x4000;
      m_acTrunkColor[2][0] = 0x25405a;
      m_acLGColor[2][1] = 0x66d966;
      m_acTrunkColor[2][1] = 0x8000;
      m_acLGColor[3][0] = 0x4000;
      m_acTrunkColor[3][0] = 0x25405a;
      m_acLGColor[3][1] = 0xe600;
      m_acTrunkColor[3][1] = 0x8000;
      m_afLGDensBySeason[0] = 0.8;
      m_afLGDensBySeason[1] = 0.8;
      m_afLGDensBySeason[2] = 0.8;
      m_afLGDensBySeason[3] = 0.42;
      m_fLGAngleUpLight = 1;
      m_fLGAngleUpHeavy = 1;
      m_fHeightVar = 0.12;
      m_fLGNumVar = 0.21;
      m_fLGSizeVar = 0.16;
      m_fLGOrientVar = 0.12;
      m_fTrunkSkewVar = 0.13;
      m_fTrunkThickness = 0.0131826;
      m_fTrunkTightness = 0.06;
      m_fTrunkWobble = 0;
      m_fTrunkStickiness = 0;
      m_fFlowersPerLG = 0;
      m_fFlowerSize = 0.03;
      m_fFlowerSizeVar = 0;
      m_fFlowerDist = 0.14;
      m_fFlowerPeak = 3.33333;
      m_fFlowerDuration = 0.04;
      m_dwFlowerShape = 1;
      m_cFlower = 0x4080;
      break;
   case TS_POTCORNPLANT:      // potted, corn plant
      m_fDefaultAge = 5;
      m_afAges[0] = 1;
      m_afHeight[0] = 0.4;
      m_afAges[1] = 5;
      m_afHeight[1] = 1;
      m_afAges[2] = 10;
      m_afHeight[2] = 2;
      m_afCanopyZ[0] = 0.25;
      m_afCanopyZ[1] = 0.5;
      m_afCanopyZ[2] = 0.75;
      m_afCanopyW[0] = 0.3;
      m_afCanopyW[1] = 0.4;
      m_afCanopyW[2] = 0.4;
      m_afCanopyW[3] = 0.3;
      m_dwLGShape = 2;
      m_adwLGNum[0][0] = 5;
      m_adwLGNum[0][1] = 5;
      m_adwLGNum[0][2] = 5;
      m_adwLGNum[1][0] = 10;
      m_adwLGNum[1][1] = 10;
      m_adwLGNum[1][2] = 10;
      m_adwLGNum[2][0] = 15;
      m_adwLGNum[2][1] = 15;
      m_adwLGNum[2][2] = 15;
      m_fLGRadius = -1;
      m_pLGSize.p[0] = 0.6;
      m_pLGSize.p[1] = 0.1;
      m_pLGSize.p[2] = 0.2;
      m_afLGSize[0][0] = 0.3;
      m_afLGSize[0][1] = 0.25;
      m_afLGSize[0][2] = 0.2;
      m_afLGSize[0][3] = 0.25;
      m_afLGSize[1][0] = 0.8;
      m_afLGSize[1][1] = 0.75;
      m_afLGSize[1][2] = 0.7;
      m_afLGSize[1][3] = 0.65;
      m_afLGSize[2][0] = 1;
      m_afLGSize[2][1] = 0.95;
      m_afLGSize[2][2] = 0.9;
      m_afLGSize[2][3] = 0.9;
      m_acLGColor[0][0] = 0x408000;
      m_acTrunkColor[0][0] = 0x4040;
      m_acLGColor[0][1] = 0x4000;
      m_acTrunkColor[0][1] = 0x408080;
      m_acLGColor[1][0] = 0x408000;
      m_acTrunkColor[1][0] = 0x4040;
      m_acLGColor[1][1] = 0x4000;
      m_acTrunkColor[1][1] = 0x408080;
      m_acLGColor[2][0] = 0x408000;
      m_acTrunkColor[2][0] = 0x4040;
      m_acLGColor[2][1] = 0x4000;
      m_acTrunkColor[2][1] = 0x408080;
      m_acLGColor[3][0] = 0x408000;
      m_acTrunkColor[3][0] = 0x4040;
      m_acLGColor[3][1] = 0x4000;
      m_acTrunkColor[3][1] = 0x408080;
      m_afLGDensBySeason[0] = 0.8;
      m_afLGDensBySeason[1] = 0.8;
      m_afLGDensBySeason[2] = 0.8;
      m_afLGDensBySeason[3] = 0.8;
      m_fLGAngleUpLight = 0.4;
      m_fLGAngleUpHeavy = -0.68;
      m_fHeightVar = 0.12;
      m_fLGNumVar = 0;
      m_fLGSizeVar = 0;
      m_fLGOrientVar = 0;
      m_fTrunkSkewVar = 0.01;
      m_fTrunkThickness = 0.057544;
      m_fTrunkTightness = 0.49;
      m_fTrunkWobble = 0.16;
      m_fTrunkStickiness = 0.12;
      m_fFlowersPerLG = 0;
      m_fFlowerSize = 0.1;
      m_fFlowerSizeVar = 0.2;
      m_fFlowerDist = 0.1;
      m_fFlowerPeak = 2;
      m_fFlowerDuration = 0.1;
      m_dwFlowerShape = 4;
      m_cFlower = 0xff;
      break;
   case TS_POTPOINTSETIA:
      m_fDefaultAge = 2;
      m_afAges[0] = 1;
      m_afHeight[0] = 0.2;
      m_afAges[1] = 2;
      m_afHeight[1] = 0.5;
      m_afAges[2] = 3;
      m_afHeight[2] = 0.75;
      m_afCanopyZ[0] = 0.1;
      m_afCanopyZ[1] = 0.4;
      m_afCanopyZ[2] = 0.7;
      m_afCanopyW[0] = 0.9;
      m_afCanopyW[1] = 0.8;
      m_afCanopyW[2] = 0.5;
      m_afCanopyW[3] = 0.2;
      m_dwLGShape = 0;
      m_adwLGNum[0][0] = 5;
      m_adwLGNum[0][1] = 5;
      m_adwLGNum[0][2] = 5;
      m_adwLGNum[1][0] = 20;
      m_adwLGNum[1][1] = 20;
      m_adwLGNum[1][2] = 20;
      m_adwLGNum[2][0] = 30;
      m_adwLGNum[2][1] = 30;
      m_adwLGNum[2][2] = 30;
      m_fLGRadius = 1.5;
      m_pLGSize.p[0] = 0.15;
      m_pLGSize.p[1] = 0.15;
      m_pLGSize.p[2] = 0.15;
      m_afLGSize[0][0] = 0.3;
      m_afLGSize[0][1] = 0.25;
      m_afLGSize[0][2] = 0.2;
      m_afLGSize[0][3] = 0.25;
      m_afLGSize[1][0] = 0.8;
      m_afLGSize[1][1] = 0.75;
      m_afLGSize[1][2] = 0.7;
      m_afLGSize[1][3] = 0.65;
      m_afLGSize[2][0] = 1;
      m_afLGSize[2][1] = 0.95;
      m_afLGSize[2][2] = 0.9;
      m_afLGSize[2][3] = 0.9;
      m_acLGColor[0][0] = 0x8000;
      m_acTrunkColor[0][0] = 0x4040;
      m_acLGColor[0][1] = 0x4000;
      m_acTrunkColor[0][1] = 0x408080;
      m_acLGColor[1][0] = 0x8000;
      m_acTrunkColor[1][0] = 0x4040;
      m_acLGColor[1][1] = 0x4000;
      m_acTrunkColor[1][1] = 0x408080;
      m_acLGColor[2][0] = 0x8000;
      m_acTrunkColor[2][0] = 0x4040;
      m_acLGColor[2][1] = 0x4000;
      m_acTrunkColor[2][1] = 0x408080;
      m_acLGColor[3][0] = 0x8000;
      m_acTrunkColor[3][0] = 0x4040;
      m_acLGColor[3][1] = 0x4000;
      m_acTrunkColor[3][1] = 0x408080;
      m_afLGDensBySeason[0] = 0.8;
      m_afLGDensBySeason[1] = 0.8;
      m_afLGDensBySeason[2] = 0.8;
      m_afLGDensBySeason[3] = 0.8;
      m_fLGAngleUpLight = 0.4;
      m_fLGAngleUpHeavy = -0.68;
      m_fHeightVar = 0.12;
      m_fLGNumVar = 0;
      m_fLGSizeVar = 0;
      m_fLGOrientVar = 0;
      m_fTrunkSkewVar = 0.01;
      m_fTrunkThickness = 0.057544;
      m_fTrunkTightness = 0.49;
      m_fTrunkWobble = 0.16;
      m_fTrunkStickiness = 0.12;
      m_fFlowersPerLG = 0.5;
      m_fFlowerSize = 0.1;
      m_fFlowerSizeVar = 0.2;
      m_fFlowerDist = 0.3;
      m_fFlowerPeak = 3;
      m_fFlowerDuration = 0.1;
      m_dwFlowerShape = 3;
      m_cFlower = 0xff;
      break;
   case TS_POTLEAFY:      // potted, leafy
      m_fDefaultAge = 5;
      m_afAges[0] = 1;
      m_afHeight[0] = 0.2;
      m_afAges[1] = 5;
      m_afHeight[1] = 0.5;
      m_afAges[2] = 10;
      m_afHeight[2] = 1;
      m_afCanopyZ[0] = 0.25;
      m_afCanopyZ[1] = 0.5;
      m_afCanopyZ[2] = 0.75;
      m_afCanopyW[0] = 0.3;
      m_afCanopyW[1] = 0.4;
      m_afCanopyW[2] = 0.4;
      m_afCanopyW[3] = 0.3;
      m_dwLGShape = 3;
      m_adwLGNum[0][0] = 5;
      m_adwLGNum[0][1] = 5;
      m_adwLGNum[0][2] = 5;
      m_adwLGNum[1][0] = 20;
      m_adwLGNum[1][1] = 20;
      m_adwLGNum[1][2] = 20;
      m_adwLGNum[2][0] = 30;
      m_adwLGNum[2][1] = 30;
      m_adwLGNum[2][2] = 30;
      m_fLGRadius = 0.43;
      m_pLGSize.p[0] = 0.3;
      m_pLGSize.p[1] = 0.25;
      m_pLGSize.p[2] = 0.05;
      m_afLGSize[0][0] = 0.3;
      m_afLGSize[0][1] = 0.25;
      m_afLGSize[0][2] = 0.2;
      m_afLGSize[0][3] = 0.25;
      m_afLGSize[1][0] = 0.8;
      m_afLGSize[1][1] = 0.75;
      m_afLGSize[1][2] = 0.7;
      m_afLGSize[1][3] = 0.65;
      m_afLGSize[2][0] = 1;
      m_afLGSize[2][1] = 0.95;
      m_afLGSize[2][2] = 0.9;
      m_afLGSize[2][3] = 0.9;
      m_acLGColor[0][0] = 0x645000;
      m_acTrunkColor[0][0] = 0x4040;
      m_acLGColor[0][1] = 0x4000;
      m_acTrunkColor[0][1] = 0x408080;
      m_acLGColor[1][0] = 0x645000;
      m_acTrunkColor[1][0] = 0x4040;
      m_acLGColor[1][1] = 0x4000;
      m_acTrunkColor[1][1] = 0x408080;
      m_acLGColor[2][0] = 0x645000;
      m_acTrunkColor[2][0] = 0x4040;
      m_acLGColor[2][1] = 0x4000;
      m_acTrunkColor[2][1] = 0x408080;
      m_acLGColor[3][0] = 0x645000;
      m_acTrunkColor[3][0] = 0x4040;
      m_acLGColor[3][1] = 0x4000;
      m_acTrunkColor[3][1] = 0x408080;
      m_afLGDensBySeason[0] = 0.8;
      m_afLGDensBySeason[1] = 0.8;
      m_afLGDensBySeason[2] = 0.8;
      m_afLGDensBySeason[3] = 0.8;
      m_fLGAngleUpLight = 0.4;
      m_fLGAngleUpHeavy = -0.68;
      m_fHeightVar = 0.12;
      m_fLGNumVar = 0;
      m_fLGSizeVar = 0;
      m_fLGOrientVar = 0;
      m_fTrunkSkewVar = 0.01;
      m_fTrunkThickness = 0.057544;
      m_fTrunkTightness = 0.49;
      m_fTrunkWobble = 0.16;
      m_fTrunkStickiness = 0.12;
      m_fFlowersPerLG = 0;
      m_fFlowerSize = 0.1;
      m_fFlowerSizeVar = 0.2;
      m_fFlowerDist = 0.1;
      m_fFlowerPeak = 2;
      m_fFlowerDuration = 0.1;
      m_dwFlowerShape = 4;
      m_cFlower = 0xff;
      break;
   case TS_POTSHAPED2:      // potted, shaped 2
      m_fDefaultAge = 5;
      m_afAges[0] = 1;
      m_afHeight[0] = 0.4;
      m_afAges[1] = 5;
      m_afHeight[1] = 1;
      m_afAges[2] = 10;
      m_afHeight[2] = 2;
      m_afCanopyZ[0] = 0.4;
      m_afCanopyZ[1] = 0.6;
      m_afCanopyZ[2] = 0.8;
      m_afCanopyW[0] = 0.3;
      m_afCanopyW[1] = 0.6;
      m_afCanopyW[2] = 0.6;
      m_afCanopyW[3] = 0.3;
      m_dwLGShape = 0;
      m_adwLGNum[0][0] = 8;
      m_adwLGNum[0][1] = 8;
      m_adwLGNum[0][2] = 8;
      m_adwLGNum[1][0] = 20;
      m_adwLGNum[1][1] = 20;
      m_adwLGNum[1][2] = 20;
      m_adwLGNum[2][0] = 25;
      m_adwLGNum[2][1] = 25;
      m_adwLGNum[2][2] = 25;
      m_fLGRadius = 1.8;
      m_pLGSize.p[0] = 0.45;
      m_pLGSize.p[1] = 0.45;
      m_pLGSize.p[2] = 0.45;
      m_afLGSize[0][0] = 0.2;
      m_afLGSize[0][1] = 0.2;
      m_afLGSize[0][2] = 0.2;
      m_afLGSize[0][3] = 0.2;
      m_afLGSize[1][0] = 0.7;
      m_afLGSize[1][1] = 0.7;
      m_afLGSize[1][2] = 0.7;
      m_afLGSize[1][3] = 0.7;
      m_afLGSize[2][0] = 1;
      m_afLGSize[2][1] = 1;
      m_afLGSize[2][2] = 1;
      m_afLGSize[2][3] = 1;
      m_acLGColor[0][0] = 0x645000;
      m_acTrunkColor[0][0] = 0x4040;
      m_acLGColor[0][1] = 0x88b61d;
      m_acTrunkColor[0][1] = 0x408080;
      m_acLGColor[1][0] = 0x645000;
      m_acTrunkColor[1][0] = 0x4040;
      m_acLGColor[1][1] = 0x608000;
      m_acTrunkColor[1][1] = 0x408080;
      m_acLGColor[2][0] = 0x645000;
      m_acTrunkColor[2][0] = 0x4040;
      m_acLGColor[2][1] = 0x608000;
      m_acTrunkColor[2][1] = 0x408080;
      m_acLGColor[3][0] = 0x645000;
      m_acTrunkColor[3][0] = 0x4040;
      m_acLGColor[3][1] = 0x608000;
      m_acTrunkColor[3][1] = 0x408080;
      m_afLGDensBySeason[0] = 0.8;
      m_afLGDensBySeason[1] = 0.8;
      m_afLGDensBySeason[2] = 0.8;
      m_afLGDensBySeason[3] = 0.8;
      m_fLGAngleUpLight = 0.96;
      m_fLGAngleUpHeavy = -0.6;
      m_fHeightVar = 0.12;
      m_fLGNumVar = 0;
      m_fLGSizeVar = 0;
      m_fLGOrientVar = 0;
      m_fTrunkSkewVar = 0.01;
      m_fTrunkThickness = 0.0331131;
      m_fTrunkTightness = 0.42;
      m_fTrunkWobble = 0.16;
      m_fTrunkStickiness = 0.16;
      m_fFlowersPerLG = 0;
      m_fFlowerSize = 0.1;
      m_fFlowerSizeVar = 0.2;
      m_fFlowerDist = 0.1;
      m_fFlowerPeak = 2;
      m_fFlowerDuration = 0.1;
      m_dwFlowerShape = 4;
      m_cFlower = 0xff;
      break;
   case TS_POTSHAPED1:      // potted, shaped 1
      m_fDefaultAge = 5;
      m_afAges[0] = 1;
      m_afHeight[0] = 0.4;
      m_afAges[1] = 5;
      m_afHeight[1] = 1;
      m_afAges[2] = 10;
      m_afHeight[2] = 2;
      m_afCanopyZ[0] = 0.25;
      m_afCanopyZ[1] = 0.5;
      m_afCanopyZ[2] = 0.75;
      m_afCanopyW[0] = 0.2;
      m_afCanopyW[1] = 0.5;
      m_afCanopyW[2] = 0.6;
      m_afCanopyW[3] = 0.4;
      m_dwLGShape = 0;
      m_adwLGNum[0][0] = 0;
      m_adwLGNum[0][1] = 1;
      m_adwLGNum[0][2] = 1;
      m_adwLGNum[1][0] = 1;
      m_adwLGNum[1][1] = 3;
      m_adwLGNum[1][2] = 2;
      m_adwLGNum[2][0] = 1;
      m_adwLGNum[2][1] = 3;
      m_adwLGNum[2][2] = 2;
      m_fLGRadius = 1.8;
      m_pLGSize.p[0] = 0.5;
      m_pLGSize.p[1] = 0.5;
      m_pLGSize.p[2] = 0.5;
      m_afLGSize[0][0] = 0.2;
      m_afLGSize[0][1] = 0.2;
      m_afLGSize[0][2] = 0.2;
      m_afLGSize[0][3] = 0.2;
      m_afLGSize[1][0] = 0.7;
      m_afLGSize[1][1] = 0.7;
      m_afLGSize[1][2] = 0.7;
      m_afLGSize[1][3] = 0.7;
      m_afLGSize[2][0] = 1;
      m_afLGSize[2][1] = 1;
      m_afLGSize[2][2] = 1;
      m_afLGSize[2][3] = 1;
      m_acLGColor[0][0] = 0x645000;
      m_acTrunkColor[0][0] = 0x4040;
      m_acLGColor[0][1] = 0x88b61d;
      m_acTrunkColor[0][1] = 0x408080;
      m_acLGColor[1][0] = 0x645000;
      m_acTrunkColor[1][0] = 0x4040;
      m_acLGColor[1][1] = 0x608000;
      m_acTrunkColor[1][1] = 0x408080;
      m_acLGColor[2][0] = 0x645000;
      m_acTrunkColor[2][0] = 0x4040;
      m_acLGColor[2][1] = 0x608000;
      m_acTrunkColor[2][1] = 0x408080;
      m_acLGColor[3][0] = 0x645000;
      m_acTrunkColor[3][0] = 0x4040;
      m_acLGColor[3][1] = 0x608000;
      m_acTrunkColor[3][1] = 0x408080;
      m_afLGDensBySeason[0] = 0.8;
      m_afLGDensBySeason[1] = 0.8;
      m_afLGDensBySeason[2] = 0.8;
      m_afLGDensBySeason[3] = 0.8;
      m_fLGAngleUpLight = 0.96;
      m_fLGAngleUpHeavy = -0.6;
      m_fHeightVar = 0.12;
      m_fLGNumVar = 0;
      m_fLGSizeVar = 0;
      m_fLGOrientVar = 0;
      m_fTrunkSkewVar = 0.01;
      m_fTrunkThickness = 0.057544;
      m_fTrunkTightness = 0.42;
      m_fTrunkWobble = 0.16;
      m_fTrunkStickiness = 0.16;
      m_fFlowersPerLG = 0;
      m_fFlowerSize = 0.1;
      m_fFlowerSizeVar = 0.2;
      m_fFlowerDist = 0.1;
      m_fFlowerPeak = 2;
      m_fFlowerDuration = 0.1;
      m_dwFlowerShape = 4;
      m_cFlower = 0xff;
      break;
   case TS_POTCONIFER:      // potted, conifer
      m_fDefaultAge = 5;
      m_afAges[0] = 1;
      m_afHeight[0] = 0.2;
      m_afAges[1] = 5;
      m_afHeight[1] = 0.5;
      m_afAges[2] = 10;
      m_afHeight[2] = 1;
      m_afCanopyZ[0] = 0.1;
      m_afCanopyZ[1] = 0.4;
      m_afCanopyZ[2] = 0.7;
      m_afCanopyW[0] = 0.6;
      m_afCanopyW[1] = 0.5;
      m_afCanopyW[2] = 0.3;
      m_afCanopyW[3] = 0.1;
      m_dwLGShape = 6;
      m_adwLGNum[0][0] = 5;
      m_adwLGNum[0][1] = 5;
      m_adwLGNum[0][2] = 5;
      m_adwLGNum[1][0] = 15;
      m_adwLGNum[1][1] = 15;
      m_adwLGNum[1][2] = 15;
      m_adwLGNum[2][0] = 20;
      m_adwLGNum[2][1] = 20;
      m_adwLGNum[2][2] = 20;
      m_fLGRadius = -0.8;
      m_pLGSize.p[0] = 0.4;
      m_pLGSize.p[1] = 0.3;
      m_pLGSize.p[2] = 0.075;
      m_afLGSize[0][0] = 0.2;
      m_afLGSize[0][1] = 0.15;
      m_afLGSize[0][2] = 0.1;
      m_afLGSize[0][3] = 0.05;
      m_afLGSize[1][0] = 0.7;
      m_afLGSize[1][1] = 0.5;
      m_afLGSize[1][2] = 0.3;
      m_afLGSize[1][3] = 0.1;
      m_afLGSize[2][0] = 1;
      m_afLGSize[2][1] = 0.6;
      m_afLGSize[2][2] = 0.5;
      m_afLGSize[2][3] = 0.1;
      m_acLGColor[0][0] = 0x645000;
      m_acTrunkColor[0][0] = 0x4040;
      m_acLGColor[0][1] = 0x88b61d;
      m_acTrunkColor[0][1] = 0x408080;
      m_acLGColor[1][0] = 0x645000;
      m_acTrunkColor[1][0] = 0x4040;
      m_acLGColor[1][1] = 0x608000;
      m_acTrunkColor[1][1] = 0x408080;
      m_acLGColor[2][0] = 0x645000;
      m_acTrunkColor[2][0] = 0x4040;
      m_acLGColor[2][1] = 0x608000;
      m_acTrunkColor[2][1] = 0x408080;
      m_acLGColor[3][0] = 0x645000;
      m_acTrunkColor[3][0] = 0x4040;
      m_acLGColor[3][1] = 0x608000;
      m_acTrunkColor[3][1] = 0x408080;
      m_afLGDensBySeason[0] = 0.8;
      m_afLGDensBySeason[1] = 0.8;
      m_afLGDensBySeason[2] = 0.8;
      m_afLGDensBySeason[3] = 0.8;
      m_fLGAngleUpLight = 0.96;
      m_fLGAngleUpHeavy = -0.6;
      m_fHeightVar = 0.12;
      m_fLGNumVar = 0.23;
      m_fLGSizeVar = 0.16;
      m_fLGOrientVar = 0.04;
      m_fTrunkSkewVar = 0.01;
      m_fTrunkThickness = 0.057544;
      m_fTrunkTightness = 0.05;
      m_fTrunkWobble = 0;
      m_fTrunkStickiness = 0.2;
      m_fFlowersPerLG = 0;
      m_fFlowerSize = 0.1;
      m_fFlowerSizeVar = 0.2;
      m_fFlowerDist = 0.1;
      m_fFlowerPeak = 2;
      m_fFlowerDuration = 0.1;
      m_dwFlowerShape = 4;
      m_cFlower = 0xff;
      break;
   case TS_ANNUALSPRINGRED:      // peren/an, sprint flower red
      m_fDefaultAge = 2;
      m_afAges[0] = 1;
      m_afHeight[0] = 0.2;
      m_afAges[1] = 2;
      m_afHeight[1] = 0.2;
      m_afAges[2] = 3;
      m_afHeight[2] = 0.2;
      m_afCanopyZ[0] = 0.3;
      m_afCanopyZ[1] = 0.6;
      m_afCanopyZ[2] = 0.8;
      m_afCanopyW[0] = 5;
      m_afCanopyW[1] = 5;
      m_afCanopyW[2] = 5;
      m_afCanopyW[3] = 5;
      m_dwLGShape = 0;
      m_adwLGNum[0][0] = 20;
      m_adwLGNum[0][1] = 20;
      m_adwLGNum[0][2] = 20;
      m_adwLGNum[1][0] = 20;
      m_adwLGNum[1][1] = 20;
      m_adwLGNum[1][2] = 20;
      m_adwLGNum[2][0] = 20;
      m_adwLGNum[2][1] = 20;
      m_adwLGNum[2][2] = 20;
      m_fLGRadius = 0.93;
      m_pLGSize.p[0] = 0.4;
      m_pLGSize.p[1] = 0.4;
      m_pLGSize.p[2] = 0.4;
      m_afLGSize[0][0] = 1;
      m_afLGSize[0][1] = 1;
      m_afLGSize[0][2] = 1;
      m_afLGSize[0][3] = 1;
      m_afLGSize[1][0] = 1;
      m_afLGSize[1][1] = 1;
      m_afLGSize[1][2] = 1;
      m_afLGSize[1][3] = 1;
      m_afLGSize[2][0] = 1;
      m_afLGSize[2][1] = 1;
      m_afLGSize[2][2] = 1;
      m_afLGSize[2][3] = 1;
      m_acLGColor[0][0] = 0xff00;
      m_acTrunkColor[0][0] = 0x8000;
      m_acLGColor[0][1] = 0x4000;
      m_acTrunkColor[0][1] = 0x4080;
      m_acLGColor[1][0] = 0x8000;
      m_acTrunkColor[1][0] = 0x8000;
      m_acLGColor[1][1] = 0x4000;
      m_acTrunkColor[1][1] = 0x4080;
      m_acLGColor[2][0] = 0x8000;
      m_acTrunkColor[2][0] = 0x8000;
      m_acLGColor[2][1] = 0x4000;
      m_acTrunkColor[2][1] = 0x4080;
      m_acLGColor[3][0] = 0x8000;
      m_acTrunkColor[3][0] = 0x8000;
      m_acLGColor[3][1] = 0x4000;
      m_acTrunkColor[3][1] = 0x4080;
      m_afLGDensBySeason[0] = 0.8;
      m_afLGDensBySeason[1] = 0.8;
      m_afLGDensBySeason[2] = 0.8;
      m_afLGDensBySeason[3] = 0;
      m_fLGAngleUpLight = 0.15;
      m_fLGAngleUpHeavy = 0.15;
      m_fHeightVar = 0.1;
      m_fLGNumVar = 0.12;
      m_fLGSizeVar = 0.37;
      m_fLGOrientVar = 0.16;
      m_fTrunkSkewVar = 0.1;
      m_fTrunkThickness = 0.00691831;
      m_fTrunkTightness = 0.11;
      m_fTrunkWobble = 0.15;
      m_fTrunkStickiness = 0.09;
      m_fFlowersPerLG = 3;
      m_fFlowerSize = 0.08;
      m_fFlowerSizeVar = 0.2;
      m_fFlowerDist = 0.55;
      m_fFlowerPeak = 3.66667;
      m_fFlowerDuration = 0.21;
      m_dwFlowerShape = 4;
      m_cFlower = 0x404fb;
      break;
   case TS_ANNUALSUMBLUE:      // pern/an, summer flower blue
      m_fDefaultAge = 2;
      m_afAges[0] = 1;
      m_afHeight[0] = 0.2;
      m_afAges[1] = 2;
      m_afHeight[1] = 0.2;
      m_afAges[2] = 3;
      m_afHeight[2] = 0.2;
      m_afCanopyZ[0] = 0.3;
      m_afCanopyZ[1] = 0.6;
      m_afCanopyZ[2] = 0.8;
      m_afCanopyW[0] = 5;
      m_afCanopyW[1] = 5;
      m_afCanopyW[2] = 5;
      m_afCanopyW[3] = 5;
      m_dwLGShape = 0;
      m_adwLGNum[0][0] = 20;
      m_adwLGNum[0][1] = 20;
      m_adwLGNum[0][2] = 20;
      m_adwLGNum[1][0] = 20;
      m_adwLGNum[1][1] = 20;
      m_adwLGNum[1][2] = 20;
      m_adwLGNum[2][0] = 20;
      m_adwLGNum[2][1] = 20;
      m_adwLGNum[2][2] = 20;
      m_fLGRadius = 0.93;
      m_pLGSize.p[0] = 0.4;
      m_pLGSize.p[1] = 0.4;
      m_pLGSize.p[2] = 0.4;
      m_afLGSize[0][0] = 1;
      m_afLGSize[0][1] = 1;
      m_afLGSize[0][2] = 1;
      m_afLGSize[0][3] = 1;
      m_afLGSize[1][0] = 1;
      m_afLGSize[1][1] = 1;
      m_afLGSize[1][2] = 1;
      m_afLGSize[1][3] = 1;
      m_afLGSize[2][0] = 1;
      m_afLGSize[2][1] = 1;
      m_afLGSize[2][2] = 1;
      m_afLGSize[2][3] = 1;
      m_acLGColor[0][0] = 0xff00;
      m_acTrunkColor[0][0] = 0x8000;
      m_acLGColor[0][1] = 0x4000;
      m_acTrunkColor[0][1] = 0x4080;
      m_acLGColor[1][0] = 0x8000;
      m_acTrunkColor[1][0] = 0x8000;
      m_acLGColor[1][1] = 0x4000;
      m_acTrunkColor[1][1] = 0x4080;
      m_acLGColor[2][0] = 0x8000;
      m_acTrunkColor[2][0] = 0x8000;
      m_acLGColor[2][1] = 0x4000;
      m_acTrunkColor[2][1] = 0x4080;
      m_acLGColor[3][0] = 0x8000;
      m_acTrunkColor[3][0] = 0x8000;
      m_acLGColor[3][1] = 0x4000;
      m_acTrunkColor[3][1] = 0x4080;
      m_afLGDensBySeason[0] = 0.8;
      m_afLGDensBySeason[1] = 0.8;
      m_afLGDensBySeason[2] = 0.8;
      m_afLGDensBySeason[3] = 0;
      m_fLGAngleUpLight = 0.15;
      m_fLGAngleUpHeavy = 0.15;
      m_fHeightVar = 0.1;
      m_fLGNumVar = 0.12;
      m_fLGSizeVar = 0.37;
      m_fLGOrientVar = 0.16;
      m_fTrunkSkewVar = 0.1;
      m_fTrunkThickness = 0.00691831;
      m_fTrunkTightness = 0.11;
      m_fTrunkWobble = 0.15;
      m_fTrunkStickiness = 0.09;
      m_fFlowersPerLG = 3;
      m_fFlowerSize = 0.08;
      m_fFlowerSizeVar = 0.2;
      m_fFlowerDist = 0.55;
      m_fFlowerPeak = 1;
      m_fFlowerDuration = 0.21;
      m_dwFlowerShape = 3;
      m_cFlower = 0xffff80;
      break;
   case TS_SHRUBLILAC:   // shrub, lilac
      m_fDefaultAge = 8;
      m_afAges[0] = 2;
      m_afHeight[0] = 0.5;
      m_afAges[1] = 8;
      m_afHeight[1] = 3;
      m_afAges[2] = 15;
      m_afHeight[2] = 6;
      m_afCanopyZ[0] = 0.2;
      m_afCanopyZ[1] = 0.4;
      m_afCanopyZ[2] = 0.7;
      m_afCanopyW[0] = 0.3;
      m_afCanopyW[1] = 0.5;
      m_afCanopyW[2] = 0.6;
      m_afCanopyW[3] = 0.2;
      m_dwLGShape = 0;
      m_adwLGNum[0][0] = 0;
      m_adwLGNum[0][1] = 1;
      m_adwLGNum[0][2] = 2;
      m_adwLGNum[1][0] = 15;
      m_adwLGNum[1][1] = 15;
      m_adwLGNum[1][2] = 15;
      m_adwLGNum[2][0] = 20;
      m_adwLGNum[2][1] = 20;
      m_adwLGNum[2][2] = 20;
      m_fLGRadius = 1.4;
      m_pLGSize.p[0] = 1.25;
      m_pLGSize.p[1] = 1.25;
      m_pLGSize.p[2] = 1.25;
      m_afLGSize[0][0] = 0.2;
      m_afLGSize[0][1] = 0.2;
      m_afLGSize[0][2] = 0.2;
      m_afLGSize[0][3] = 0.2;
      m_afLGSize[1][0] = 0.6;
      m_afLGSize[1][1] = 0.6;
      m_afLGSize[1][2] = 0.6;
      m_afLGSize[1][3] = 0.6;
      m_afLGSize[2][0] = 1;
      m_afLGSize[2][1] = 1;
      m_afLGSize[2][2] = 1;
      m_afLGSize[2][3] = 1;
      m_acLGColor[0][0] = 0x8000;
      m_acTrunkColor[0][0] = 0x2b55;
      m_acLGColor[0][1] = 0x4000;
      m_acTrunkColor[0][1] = 0x4080;
      m_acLGColor[1][0] = 0x8000;
      m_acTrunkColor[1][0] = 0x2b55;
      m_acLGColor[1][1] = 0x4000;
      m_acTrunkColor[1][1] = 0x4080;
      m_acLGColor[2][0] = 0x8000;
      m_acTrunkColor[2][0] = 0x274f;
      m_acLGColor[2][1] = 0x4000;
      m_acTrunkColor[2][1] = 0x4080;
      m_acLGColor[3][0] = 0x8000;
      m_acTrunkColor[3][0] = 0x274f;
      m_acLGColor[3][1] = 0x4000;
      m_acTrunkColor[3][1] = 0x4080;
      m_afLGDensBySeason[0] = 0.65;
      m_afLGDensBySeason[1] = 0.8;
      m_afLGDensBySeason[2] = 0.67;
      m_afLGDensBySeason[3] = 0;
      m_fLGAngleUpLight = 0.52;
      m_fLGAngleUpHeavy = -0.28;
      m_fHeightVar = 0.1;
      m_fLGNumVar = 0.12;
      m_fLGSizeVar = 0.07;
      m_fLGOrientVar = 0.16;
      m_fTrunkSkewVar = 0.07;
      m_fTrunkThickness = 0.0275423;
      m_fTrunkTightness = 0.03;
      m_fTrunkWobble = 0.12;
      m_fTrunkStickiness = 0.06;
      m_fFlowersPerLG = 3;
      m_fFlowerSize = 0.25;
      m_fFlowerSizeVar = 0.2;
      m_fFlowerDist = 0.45;
      m_fFlowerPeak = 0;
      m_fFlowerDuration = 0.21;
      m_dwFlowerShape = 0;
      m_cFlower = 0xe60bbf;
      break;
   case TS_SHRUBROSE:   // shurbs, rose
      m_fDefaultAge = 6;
      m_afAges[0] = 2;
      m_afHeight[0] = 0.25;
      m_afAges[1] = 6;
      m_afHeight[1] = 0.75;
      m_afAges[2] = 10;
      m_afHeight[2] = 1.5;
      m_afCanopyZ[0] = 0.3;
      m_afCanopyZ[1] = 0.6;
      m_afCanopyZ[2] = 0.8;
      m_afCanopyW[0] = 0.5;
      m_afCanopyW[1] = 0.7;
      m_afCanopyW[2] = 0.7;
      m_afCanopyW[3] = 0.2;
      m_dwLGShape = 1;
      m_adwLGNum[0][0] = 0;
      m_adwLGNum[0][1] = 1;
      m_adwLGNum[0][2] = 2;
      m_adwLGNum[1][0] = 10;
      m_adwLGNum[1][1] = 10;
      m_adwLGNum[1][2] = 10;
      m_adwLGNum[2][0] = 20;
      m_adwLGNum[2][1] = 20;
      m_adwLGNum[2][2] = 20;
      m_fLGRadius = 1.05;
      m_pLGSize.p[0] = 0.2;
      m_pLGSize.p[1] = 0.2;
      m_pLGSize.p[2] = 0.2;
      m_afLGSize[0][0] = 0.3;
      m_afLGSize[0][1] = 0.3;
      m_afLGSize[0][2] = 0.3;
      m_afLGSize[0][3] = 0.3;
      m_afLGSize[1][0] = 0.6;
      m_afLGSize[1][1] = 0.6;
      m_afLGSize[1][2] = 0.6;
      m_afLGSize[1][3] = 0.6;
      m_afLGSize[2][0] = 1;
      m_afLGSize[2][1] = 1;
      m_afLGSize[2][2] = 1;
      m_afLGSize[2][3] = 1;
      m_acLGColor[0][0] = 0x8000;
      m_acTrunkColor[0][0] = 0x8000;
      m_acLGColor[0][1] = 0x4000;
      m_acTrunkColor[0][1] = 0x4080;
      m_acLGColor[1][0] = 0x8000;
      m_acTrunkColor[1][0] = 0x8000;
      m_acLGColor[1][1] = 0x4000;
      m_acTrunkColor[1][1] = 0x4080;
      m_acLGColor[2][0] = 0x8000;
      m_acTrunkColor[2][0] = 0x8000;
      m_acLGColor[2][1] = 0x4000;
      m_acTrunkColor[2][1] = 0x4080;
      m_acLGColor[3][0] = 0x8000;
      m_acTrunkColor[3][0] = 0x8000;
      m_acLGColor[3][1] = 0x4000;
      m_acTrunkColor[3][1] = 0x4080;
      m_afLGDensBySeason[0] = 0.8;
      m_afLGDensBySeason[1] = 0.8;
      m_afLGDensBySeason[2] = 0.8;
      m_afLGDensBySeason[3] = 0;
      m_fLGAngleUpLight = 0.52;
      m_fLGAngleUpHeavy = -0.28;
      m_fHeightVar = 0.1;
      m_fLGNumVar = 0.12;
      m_fLGSizeVar = 0.37;
      m_fLGOrientVar = 0.16;
      m_fTrunkSkewVar = 0.1;
      m_fTrunkThickness = 0.0436516;
      m_fTrunkTightness = 0.11;
      m_fTrunkWobble = 0.15;
      m_fTrunkStickiness = 0.09;
      m_fFlowersPerLG = 0.5;
      m_fFlowerSize = 0.08;
      m_fFlowerSizeVar = 0.2;
      m_fFlowerDist = 0.81;
      m_fFlowerPeak = 0.666667;
      m_fFlowerDuration = 0.21;
      m_dwFlowerShape = 3;
      m_cFlower = 0xc4;
      break;
   case TS_SHRUBRHODODENDRON:   // rhododendron
      m_fDefaultAge = 8;
      m_afAges[0] = 2;
      m_afHeight[0] = 0.5;
      m_afAges[1] = 8;
      m_afHeight[1] = 1.5;
      m_afAges[2] = 15;
      m_afHeight[2] = 3;
      m_afCanopyZ[0] = 0.1;
      m_afCanopyZ[1] = 0.4;
      m_afCanopyZ[2] = 0.7;
      m_afCanopyW[0] = 0.5;
      m_afCanopyW[1] = 0.7;
      m_afCanopyW[2] = 0.7;
      m_afCanopyW[3] = 0.2;
      m_dwLGShape = 0;
      m_adwLGNum[0][0] = 0;
      m_adwLGNum[0][1] = 1;
      m_adwLGNum[0][2] = 2;
      m_adwLGNum[1][0] = 20;
      m_adwLGNum[1][1] = 20;
      m_adwLGNum[1][2] = 20;
      m_adwLGNum[2][0] = 20;
      m_adwLGNum[2][1] = 20;
      m_adwLGNum[2][2] = 20;
      m_fLGRadius = 1.8;
      m_pLGSize.p[0] = 1.85;
      m_pLGSize.p[1] = 1.85;
      m_pLGSize.p[2] = 1.85;
      m_afLGSize[0][0] = 0.3;
      m_afLGSize[0][1] = 0.3;
      m_afLGSize[0][2] = 0.3;
      m_afLGSize[0][3] = 0.3;
      m_afLGSize[1][0] = 0.6;
      m_afLGSize[1][1] = 0.6;
      m_afLGSize[1][2] = 0.6;
      m_afLGSize[1][3] = 0.6;
      m_afLGSize[2][0] = 1;
      m_afLGSize[2][1] = 1;
      m_afLGSize[2][2] = 1;
      m_afLGSize[2][3] = 1;
      m_acLGColor[0][0] = 0x8000;
      m_acTrunkColor[0][0] = 0x505050;
      m_acLGColor[0][1] = 0x4000;
      m_acTrunkColor[0][1] = 0x4080;
      m_acLGColor[1][0] = 0x8000;
      m_acTrunkColor[1][0] = 0x505050;
      m_acLGColor[1][1] = 0x4000;
      m_acTrunkColor[1][1] = 0x4080;
      m_acLGColor[2][0] = 0x8000;
      m_acTrunkColor[2][0] = 0x404040;
      m_acLGColor[2][1] = 0x4000;
      m_acTrunkColor[2][1] = 0x4080;
      m_acLGColor[3][0] = 0x8000;
      m_acTrunkColor[3][0] = 0x505050;
      m_acLGColor[3][1] = 0x4000;
      m_acTrunkColor[3][1] = 0x4080;
      m_afLGDensBySeason[0] = 0.8;
      m_afLGDensBySeason[1] = 0.8;
      m_afLGDensBySeason[2] = 0.8;
      m_afLGDensBySeason[3] = 0.8;
      m_fLGAngleUpLight = 0.52;
      m_fLGAngleUpHeavy = -0.28;
      m_fHeightVar = 0.1;
      m_fLGNumVar = 0.12;
      m_fLGSizeVar = 0.07;
      m_fLGOrientVar = 0.16;
      m_fTrunkSkewVar = 0.07;
      m_fTrunkThickness = 0.0275423;
      m_fTrunkTightness = 0;
      m_fTrunkWobble = 0.04;
      m_fTrunkStickiness = 0.04;
      m_fFlowersPerLG = 3;
      m_fFlowerSize = 0.15;
      m_fFlowerSizeVar = 0.2;
      m_fFlowerDist = 0.45;
      m_fFlowerPeak = 3.66667;
      m_fFlowerDuration = 0.21;
      m_dwFlowerShape = 0;
      m_cFlower = 0x6c00f0;
      break;
   case TS_SHRUBMUGHOPINE:   // shrub, mugho pine
      m_fDefaultAge = 8;
      m_afAges[0] = 2;
      m_afHeight[0] = 0.2;
      m_afAges[1] = 8;
      m_afHeight[1] = 0.5;
      m_afAges[2] = 15;
      m_afHeight[2] = 1;
      m_afCanopyZ[0] = 0.1;
      m_afCanopyZ[1] = 0.4;
      m_afCanopyZ[2] = 0.7;
      m_afCanopyW[0] = 1.4;
      m_afCanopyW[1] = 1.4;
      m_afCanopyW[2] = 1.1;
      m_afCanopyW[3] = 0.4;
      m_dwLGShape = 1;
      m_adwLGNum[0][0] = 10;
      m_adwLGNum[0][1] = 10;
      m_adwLGNum[0][2] = 10;
      m_adwLGNum[1][0] = 20;
      m_adwLGNum[1][1] = 20;
      m_adwLGNum[1][2] = 20;
      m_adwLGNum[2][0] = 30;
      m_adwLGNum[2][1] = 30;
      m_adwLGNum[2][2] = 30;
      m_fLGRadius = 1.55;
      m_pLGSize.p[0] = 0.3;
      m_pLGSize.p[1] = 0.4;
      m_pLGSize.p[2] = 0.8;
      m_afLGSize[0][0] = 0.2;
      m_afLGSize[0][1] = 0.14;
      m_afLGSize[0][2] = 0.12;
      m_afLGSize[0][3] = 0.1;
      m_afLGSize[1][0] = 0.8;
      m_afLGSize[1][1] = 0.7;
      m_afLGSize[1][2] = 0.6;
      m_afLGSize[1][3] = 0.5;
      m_afLGSize[2][0] = 1;
      m_afLGSize[2][1] = 0.9;
      m_afLGSize[2][2] = 0.8;
      m_afLGSize[2][3] = 0.7;
      m_acLGColor[0][0] = 0x404000;
      m_acTrunkColor[0][0] = 0x4040;
      m_acLGColor[0][1] = 0x808000;
      m_acTrunkColor[0][1] = 0x408080;
      m_acLGColor[1][0] = 0x404000;
      m_acTrunkColor[1][0] = 0x4040;
      m_acLGColor[1][1] = 0x506a00;
      m_acTrunkColor[1][1] = 0x408080;
      m_acLGColor[2][0] = 0x404000;
      m_acTrunkColor[2][0] = 0x4040;
      m_acLGColor[2][1] = 0x557100;
      m_acTrunkColor[2][1] = 0x408080;
      m_acLGColor[3][0] = 0x404000;
      m_acTrunkColor[3][0] = 0x4040;
      m_acLGColor[3][1] = 0x506a00;
      m_acTrunkColor[3][1] = 0x408080;
      m_afLGDensBySeason[0] = 0.8;
      m_afLGDensBySeason[1] = 0.8;
      m_afLGDensBySeason[2] = 0.8;
      m_afLGDensBySeason[3] = 0.8;
      m_fLGAngleUpLight = -0.28;
      m_fLGAngleUpHeavy = -0.8;
      m_fHeightVar = 0.18;
      m_fLGNumVar = 0.18;
      m_fLGSizeVar = 0.13;
      m_fLGOrientVar = 0.08;
      m_fTrunkSkewVar = 0;
      m_fTrunkThickness = 0.0275423;
      m_fTrunkTightness = 0.55;
      m_fTrunkWobble = 0;
      m_fTrunkStickiness = 0.55;
      m_fFlowersPerLG = 0;
      m_fFlowerSize = 0.1;
      m_fFlowerSizeVar = 0.2;
      m_fFlowerDist = 0.1;
      m_fFlowerPeak = 2;
      m_fFlowerDuration = 0.1;
      m_dwFlowerShape = 4;
      m_cFlower = 0xff;
      break;
   case TS_SHRUBOLEANDER:   // shrub, oleander
      m_fDefaultAge = 8;
      m_afAges[0] = 2;
      m_afHeight[0] = 0.5;
      m_afAges[1] = 8;
      m_afHeight[1] = 1.5;
      m_afAges[2] = 15;
      m_afHeight[2] = 3;
      m_afCanopyZ[0] = 0.1;
      m_afCanopyZ[1] = 0.3;
      m_afCanopyZ[2] = 0.7;
      m_afCanopyW[0] = 0.7;
      m_afCanopyW[1] = 0.8;
      m_afCanopyW[2] = 0.6;
      m_afCanopyW[3] = 0.3;
      m_dwLGShape = 0;
      m_adwLGNum[0][0] = 10;
      m_adwLGNum[0][1] = 10;
      m_adwLGNum[0][2] = 10;
      m_adwLGNum[1][0] = 20;
      m_adwLGNum[1][1] = 20;
      m_adwLGNum[1][2] = 20;
      m_adwLGNum[2][0] = 30;
      m_adwLGNum[2][1] = 30;
      m_adwLGNum[2][2] = 30;
      m_fLGRadius = 1.35;
      m_pLGSize.p[0] = 1;
      m_pLGSize.p[1] = 1;
      m_pLGSize.p[2] = 0.95;
      m_afLGSize[0][0] = 0.3;
      m_afLGSize[0][1] = 0.3;
      m_afLGSize[0][2] = 0.3;
      m_afLGSize[0][3] = 0.3;
      m_afLGSize[1][0] = 0.7;
      m_afLGSize[1][1] = 0.7;
      m_afLGSize[1][2] = 0.7;
      m_afLGSize[1][3] = 0.7;
      m_afLGSize[2][0] = 1;
      m_afLGSize[2][1] = 1;
      m_afLGSize[2][2] = 1;
      m_afLGSize[2][3] = 1;
      m_acLGColor[0][0] = 0x4000;
      m_acTrunkColor[0][0] = 0x505050;
      m_acLGColor[0][1] = 0x8000;
      m_acTrunkColor[0][1] = 0x4080;
      m_acLGColor[1][0] = 0x4000;
      m_acTrunkColor[1][0] = 0x505050;
      m_acLGColor[1][1] = 0x8000;
      m_acTrunkColor[1][1] = 0x4080;
      m_acLGColor[2][0] = 0x4000;
      m_acTrunkColor[2][0] = 0x404040;
      m_acLGColor[2][1] = 0x8000;
      m_acTrunkColor[2][1] = 0x4080;
      m_acLGColor[3][0] = 0x4000;
      m_acTrunkColor[3][0] = 0x505050;
      m_acLGColor[3][1] = 0x8000;
      m_acTrunkColor[3][1] = 0x4080;
      m_afLGDensBySeason[0] = 0.8;
      m_afLGDensBySeason[1] = 0.8;
      m_afLGDensBySeason[2] = 0.8;
      m_afLGDensBySeason[3] = 0.8;
      m_fLGAngleUpLight = 0.15;
      m_fLGAngleUpHeavy = 0.26;
      m_fHeightVar = 0.1;
      m_fLGNumVar = 0.12;
      m_fLGSizeVar = 0.2;
      m_fLGOrientVar = 0;
      m_fTrunkSkewVar = 0.07;
      m_fTrunkThickness = 0.0229087;
      m_fTrunkTightness = 0.12;
      m_fTrunkWobble = 0.04;
      m_fTrunkStickiness = 0.08;
      m_fFlowersPerLG = 4;
      m_fFlowerSize = 0.15;
      m_fFlowerSizeVar = 0.2;
      m_fFlowerDist = 0.62;
      m_fFlowerPeak = 1;
      m_fFlowerDuration = 0.18;
      m_dwFlowerShape = 1;
      m_cFlower = 0x9707e4;
      break;
   case TS_SHRUBJUNIPER2:   // shrub, juniper 2
      m_fDefaultAge = 8;
      m_afAges[0] = 2;
      m_afHeight[0] = 0.2;
      m_afAges[1] = 8;
      m_afHeight[1] = 1;
      m_afAges[2] = 15;
      m_afHeight[2] = 1.5;
      m_afCanopyZ[0] = 0.1;
      m_afCanopyZ[1] = 0.4;
      m_afCanopyZ[2] = 0.7;
      m_afCanopyW[0] = 0.1;
      m_afCanopyW[1] = 0.3;
      m_afCanopyW[2] = 0.2;
      m_afCanopyW[3] = 0.1;
      m_dwLGShape = 1;
      m_adwLGNum[0][0] = 5;
      m_adwLGNum[0][1] = 5;
      m_adwLGNum[0][2] = 5;
      m_adwLGNum[1][0] = 15;
      m_adwLGNum[1][1] = 15;
      m_adwLGNum[1][2] = 15;
      m_adwLGNum[2][0] = 25;
      m_adwLGNum[2][1] = 25;
      m_adwLGNum[2][2] = 25;
      m_fLGRadius = 1.8;
      m_pLGSize.p[0] = 0.2;
      m_pLGSize.p[1] = 0.3;
      m_pLGSize.p[2] = 0.8;
      m_afLGSize[0][0] = 0.2;
      m_afLGSize[0][1] = 0.14;
      m_afLGSize[0][2] = 0.12;
      m_afLGSize[0][3] = 0.1;
      m_afLGSize[1][0] = 0.8;
      m_afLGSize[1][1] = 0.7;
      m_afLGSize[1][2] = 0.6;
      m_afLGSize[1][3] = 0.5;
      m_afLGSize[2][0] = 1;
      m_afLGSize[2][1] = 0.9;
      m_afLGSize[2][2] = 0.8;
      m_afLGSize[2][3] = 0.7;
      m_acLGColor[0][0] = 0xb9b900;
      m_acTrunkColor[0][0] = 0x4040;
      m_acLGColor[0][1] = 0x808000;
      m_acTrunkColor[0][1] = 0x408080;
      m_acLGColor[1][0] = 0xb9b900;
      m_acTrunkColor[1][0] = 0x4040;
      m_acLGColor[1][1] = 0x506a00;
      m_acTrunkColor[1][1] = 0x408080;
      m_acLGColor[2][0] = 0xb9b900;
      m_acTrunkColor[2][0] = 0x4040;
      m_acLGColor[2][1] = 0x557100;
      m_acTrunkColor[2][1] = 0x408080;
      m_acLGColor[3][0] = 0xb9b900;
      m_acTrunkColor[3][0] = 0x4040;
      m_acLGColor[3][1] = 0x506a00;
      m_acTrunkColor[3][1] = 0x408080;
      m_afLGDensBySeason[0] = 0.8;
      m_afLGDensBySeason[1] = 0.8;
      m_afLGDensBySeason[2] = 0.8;
      m_afLGDensBySeason[3] = 0.8;
      m_fLGAngleUpLight = 0.03;
      m_fLGAngleUpHeavy = -0.73;
      m_fHeightVar = 0.18;
      m_fLGNumVar = 0;
      m_fLGSizeVar = 0.04;
      m_fLGOrientVar = 0;
      m_fTrunkSkewVar = 0;
      m_fTrunkThickness = 0.0275423;
      m_fTrunkTightness = 0.55;
      m_fTrunkWobble = 0;
      m_fTrunkStickiness = 0.55;
      m_fFlowersPerLG = 0;
      m_fFlowerSize = 0.1;
      m_fFlowerSizeVar = 0.2;
      m_fFlowerDist = 0.1;
      m_fFlowerPeak = 2;
      m_fFlowerDuration = 0.1;
      m_dwFlowerShape = 4;
      m_cFlower = 0xff;
      break;
   case TS_SHRUBJUNIPER1:   // shrub, juniper 1
      m_fDefaultAge = 8;
      m_afAges[0] = 2;
      m_afHeight[0] = 0.2;
      m_afAges[1] = 8;
      m_afHeight[1] = 1;
      m_afAges[2] = 15;
      m_afHeight[2] = 1.5;
      m_afCanopyZ[0] = 0.1;
      m_afCanopyZ[1] = 0.4;
      m_afCanopyZ[2] = 0.7;
      m_afCanopyW[0] = 1;
      m_afCanopyW[1] = 1;
      m_afCanopyW[2] = 0.8;
      m_afCanopyW[3] = 0.4;
      m_dwLGShape = 1;
      m_adwLGNum[0][0] = 20;
      m_adwLGNum[0][1] = 20;
      m_adwLGNum[0][2] = 20;
      m_adwLGNum[1][0] = 30;
      m_adwLGNum[1][1] = 30;
      m_adwLGNum[1][2] = 30;
      m_adwLGNum[2][0] = 40;
      m_adwLGNum[2][1] = 40;
      m_adwLGNum[2][2] = 40;
      m_fLGRadius = 1.55;
      m_pLGSize.p[0] = 0.8;
      m_pLGSize.p[1] = 0.4;
      m_pLGSize.p[2] = 0.3;
      m_afLGSize[0][0] = 0.2;
      m_afLGSize[0][1] = 0.14;
      m_afLGSize[0][2] = 0.12;
      m_afLGSize[0][3] = 0.1;
      m_afLGSize[1][0] = 0.8;
      m_afLGSize[1][1] = 0.7;
      m_afLGSize[1][2] = 0.6;
      m_afLGSize[1][3] = 0.5;
      m_afLGSize[2][0] = 1;
      m_afLGSize[2][1] = 0.9;
      m_afLGSize[2][2] = 0.8;
      m_afLGSize[2][3] = 0.7;
      m_acLGColor[0][0] = 0xb9b900;
      m_acTrunkColor[0][0] = 0x4040;
      m_acLGColor[0][1] = 0x808000;
      m_acTrunkColor[0][1] = 0x408080;
      m_acLGColor[1][0] = 0xb9b900;
      m_acTrunkColor[1][0] = 0x4040;
      m_acLGColor[1][1] = 0x506a00;
      m_acTrunkColor[1][1] = 0x408080;
      m_acLGColor[2][0] = 0xb9b900;
      m_acTrunkColor[2][0] = 0x4040;
      m_acLGColor[2][1] = 0x557100;
      m_acTrunkColor[2][1] = 0x408080;
      m_acLGColor[3][0] = 0xb9b900;
      m_acTrunkColor[3][0] = 0x4040;
      m_acLGColor[3][1] = 0x506a00;
      m_acTrunkColor[3][1] = 0x408080;
      m_afLGDensBySeason[0] = 0.8;
      m_afLGDensBySeason[1] = 0.8;
      m_afLGDensBySeason[2] = 0.8;
      m_afLGDensBySeason[3] = 0.8;
      m_fLGAngleUpLight = 1;
      m_fLGAngleUpHeavy = 0.34;
      m_fHeightVar = 0.18;
      m_fLGNumVar = 0.18;
      m_fLGSizeVar = 0.13;
      m_fLGOrientVar = 0.08;
      m_fTrunkSkewVar = 0;
      m_fTrunkThickness = 0.0275423;
      m_fTrunkTightness = 0.55;
      m_fTrunkWobble = 0;
      m_fTrunkStickiness = 0.55;
      m_fFlowersPerLG = 0;
      m_fFlowerSize = 0.1;
      m_fFlowerSizeVar = 0.2;
      m_fFlowerDist = 0.1;
      m_fFlowerPeak = 2;
      m_fFlowerDuration = 0.1;
      m_dwFlowerShape = 4;
      m_cFlower = 0xff;
      break;
   case TS_SHRUBHYDRANGEA:      // shrub, hydrangea
      m_fDefaultAge = 5;
      m_afAges[0] = 2;
      m_afHeight[0] = 0.5;
      m_afAges[1] = 5;
      m_afHeight[1] = 1;
      m_afAges[2] = 10;
      m_afHeight[2] = 2;
      m_afCanopyZ[0] = 0.1;
      m_afCanopyZ[1] = 0.4;
      m_afCanopyZ[2] = 0.7;
      m_afCanopyW[0] = 1.2;
      m_afCanopyW[1] = 1.2;
      m_afCanopyW[2] = 0.8;
      m_afCanopyW[3] = 0.4;
      m_dwLGShape = 0;
      m_adwLGNum[0][0] = 4;
      m_adwLGNum[0][1] = 4;
      m_adwLGNum[0][2] = 4;
      m_adwLGNum[1][0] = 10;
      m_adwLGNum[1][1] = 10;
      m_adwLGNum[1][2] = 10;
      m_adwLGNum[2][0] = 20;
      m_adwLGNum[2][1] = 20;
      m_adwLGNum[2][2] = 20;
      m_fLGRadius = 1.35;
      m_pLGSize.p[0] = 1.2;
      m_pLGSize.p[1] = 1.2;
      m_pLGSize.p[2] = 0.9;
      m_afLGSize[0][0] = 0.3;
      m_afLGSize[0][1] = 0.3;
      m_afLGSize[0][2] = 0.3;
      m_afLGSize[0][3] = 0.3;
      m_afLGSize[1][0] = 0.6;
      m_afLGSize[1][1] = 0.6;
      m_afLGSize[1][2] = 0.6;
      m_afLGSize[1][3] = 0.6;
      m_afLGSize[2][0] = 1;
      m_afLGSize[2][1] = 1;
      m_afLGSize[2][2] = 1;
      m_afLGSize[2][3] = 1;
      m_acLGColor[0][0] = 0xd900;
      m_acTrunkColor[0][0] = 0x505050;
      m_acLGColor[0][1] = 0x8000;
      m_acTrunkColor[0][1] = 0x4080;
      m_acLGColor[1][0] = 0xd900;
      m_acTrunkColor[1][0] = 0x505050;
      m_acLGColor[1][1] = 0x8000;
      m_acTrunkColor[1][1] = 0x4080;
      m_acLGColor[2][0] = 0xcaca;
      m_acTrunkColor[2][0] = 0x404040;
      m_acLGColor[2][1] = 0xb9;
      m_acTrunkColor[2][1] = 0x4080;
      m_acLGColor[3][0] = 0xe1e1;
      m_acTrunkColor[3][0] = 0x505050;
      m_acLGColor[3][1] = 0xc6;
      m_acTrunkColor[3][1] = 0x4080;
      m_afLGDensBySeason[0] = 0.65;
      m_afLGDensBySeason[1] = 0.8;
      m_afLGDensBySeason[2] = 0.7;
      m_afLGDensBySeason[3] = 0;
      m_fLGAngleUpLight = 0.83;
      m_fLGAngleUpHeavy = -0.63;
      m_fHeightVar = 0.1;
      m_fLGNumVar = 0.12;
      m_fLGSizeVar = 0.2;
      m_fLGOrientVar = 0;
      m_fTrunkSkewVar = 0.07;
      m_fTrunkThickness = 0.0131826;
      m_fTrunkTightness = 0;
      m_fTrunkWobble = 0.04;
      m_fTrunkStickiness = 0.04;
      m_fFlowersPerLG = 4;
      m_fFlowerSize = 0.2;
      m_fFlowerSizeVar = 0.2;
      m_fFlowerDist = 0.58;
      m_fFlowerPeak = 1;
      m_fFlowerDuration = 0.18;
      m_dwFlowerShape = 0;
      m_cFlower = 0xffa8f9;
      break;
   case TS_SHRUBHIBISCUS:      // shrub, hibiscus
      m_fDefaultAge = 5;
      m_afAges[0] = 1;
      m_afHeight[0] = 0.5;
      m_afAges[1] = 5;
      m_afHeight[1] = 1.5;
      m_afAges[2] = 10;
      m_afHeight[2] = 3;
      m_afCanopyZ[0] = 0.2;
      m_afCanopyZ[1] = 0.6;
      m_afCanopyZ[2] = 0.8;
      m_afCanopyW[0] = 0.3;
      m_afCanopyW[1] = 0.4;
      m_afCanopyW[2] = 1;
      m_afCanopyW[3] = 0.4;
      m_dwLGShape = 1;
      m_adwLGNum[0][0] = 4;
      m_adwLGNum[0][1] = 4;
      m_adwLGNum[0][2] = 4;
      m_adwLGNum[1][0] = 6;
      m_adwLGNum[1][1] = 6;
      m_adwLGNum[1][2] = 6;
      m_adwLGNum[2][0] = 10;
      m_adwLGNum[2][1] = 10;
      m_adwLGNum[2][2] = 10;
      m_fLGRadius = 1.25;
      m_pLGSize.p[0] = 0.8;
      m_pLGSize.p[1] = 1;
      m_pLGSize.p[2] = 0.65;
      m_afLGSize[0][0] = 0.2;
      m_afLGSize[0][1] = 0.25;
      m_afLGSize[0][2] = 0.3;
      m_afLGSize[0][3] = 0.3;
      m_afLGSize[1][0] = 0.5;
      m_afLGSize[1][1] = 0.6;
      m_afLGSize[1][2] = 0.7;
      m_afLGSize[1][3] = 0.7;
      m_afLGSize[2][0] = 0.8;
      m_afLGSize[2][1] = 0.9;
      m_afLGSize[2][2] = 1;
      m_afLGSize[2][3] = 1;
      m_acLGColor[0][0] = 0xff00;
      m_acTrunkColor[0][0] = 0x505050;
      m_acLGColor[0][1] = 0x8000;
      m_acTrunkColor[0][1] = 0x4080;
      m_acLGColor[1][0] = 0xff00;
      m_acTrunkColor[1][0] = 0x505050;
      m_acLGColor[1][1] = 0x8000;
      m_acTrunkColor[1][1] = 0x4080;
      m_acLGColor[2][0] = 0xff00;
      m_acTrunkColor[2][0] = 0x404040;
      m_acLGColor[2][1] = 0x8000;
      m_acTrunkColor[2][1] = 0x4080;
      m_acLGColor[3][0] = 0xff00;
      m_acTrunkColor[3][0] = 0x505050;
      m_acLGColor[3][1] = 0x8000;
      m_acTrunkColor[3][1] = 0x4080;
      m_afLGDensBySeason[0] = 0.65;
      m_afLGDensBySeason[1] = 0.8;
      m_afLGDensBySeason[2] = 0.7;
      m_afLGDensBySeason[3] = 0.8;
      m_fLGAngleUpLight = 0.83;
      m_fLGAngleUpHeavy = -0.63;
      m_fHeightVar = 0.1;
      m_fLGNumVar = 0.12;
      m_fLGSizeVar = 0.2;
      m_fLGOrientVar = 0;
      m_fTrunkSkewVar = 0.07;
      m_fTrunkThickness = 0.0275423;
      m_fTrunkTightness = 0;
      m_fTrunkWobble = 0.04;
      m_fTrunkStickiness = 0.04;
      m_fFlowersPerLG = 4;
      m_fFlowerSize = 0.1;
      m_fFlowerSizeVar = 0.2;
      m_fFlowerDist = 0.62;
      m_fFlowerPeak = 1;
      m_fFlowerDuration = 0.18;
      m_dwFlowerShape = 3;
      m_cFlower = 0xec00da;
      break;
   case TS_SHRUBJUNIPERFALSE:      // shrub, false juniper
      m_fDefaultAge = 10;
      m_afAges[0] = 2;
      m_afHeight[0] = 0.5;
      m_afAges[1] = 10;
      m_afHeight[1] = 2;
      m_afAges[2] = 20;
      m_afHeight[2] = 3.5;
      m_afCanopyZ[0] = 0.1;
      m_afCanopyZ[1] = 0.4;
      m_afCanopyZ[2] = 0.7;
      m_afCanopyW[0] = 0.8;
      m_afCanopyW[1] = 0.9;
      m_afCanopyW[2] = 0.8;
      m_afCanopyW[3] = 0.4;
      m_dwLGShape = 1;
      m_adwLGNum[0][0] = 25;
      m_adwLGNum[0][1] = 25;
      m_adwLGNum[0][2] = 25;
      m_adwLGNum[1][0] = 30;
      m_adwLGNum[1][1] = 40;
      m_adwLGNum[1][2] = 40;
      m_adwLGNum[2][0] = 40;
      m_adwLGNum[2][1] = 50;
      m_adwLGNum[2][2] = 50;
      m_fLGRadius = 1.25;
      m_pLGSize.p[0] = 1.8;
      m_pLGSize.p[1] = 1;
      m_pLGSize.p[2] = 0.65;
      m_afLGSize[0][0] = 0.3;
      m_afLGSize[0][1] = 0.25;
      m_afLGSize[0][2] = 0.2;
      m_afLGSize[0][3] = 0.15;
      m_afLGSize[1][0] = 0.55;
      m_afLGSize[1][1] = 0.5;
      m_afLGSize[1][2] = 0.45;
      m_afLGSize[1][3] = 0.4;
      m_afLGSize[2][0] = 1;
      m_afLGSize[2][1] = 0.9;
      m_afLGSize[2][2] = 0.8;
      m_afLGSize[2][3] = 0.7;
      m_acLGColor[0][0] = 0x5a8000;
      m_acTrunkColor[0][0] = 0x505050;
      m_acLGColor[0][1] = 0x8000;
      m_acTrunkColor[0][1] = 0x4080;
      m_acLGColor[1][0] = 0x5a8000;
      m_acTrunkColor[1][0] = 0x505050;
      m_acLGColor[1][1] = 0x4000;
      m_acTrunkColor[1][1] = 0x4080;
      m_acLGColor[2][0] = 0x5a8000;
      m_acTrunkColor[2][0] = 0x404040;
      m_acLGColor[2][1] = 0x4000;
      m_acTrunkColor[2][1] = 0x4080;
      m_acLGColor[3][0] = 0x5a8000;
      m_acTrunkColor[3][0] = 0x505050;
      m_acLGColor[3][1] = 0x4000;
      m_acTrunkColor[3][1] = 0x4080;
      m_afLGDensBySeason[0] = 0.65;
      m_afLGDensBySeason[1] = 0.8;
      m_afLGDensBySeason[2] = 0.7;
      m_afLGDensBySeason[3] = 0.8;
      m_fLGAngleUpLight = 0.83;
      m_fLGAngleUpHeavy = -0.63;
      m_fHeightVar = 0.1;
      m_fLGNumVar = 0.12;
      m_fLGSizeVar = 0;
      m_fLGOrientVar = 0;
      m_fTrunkSkewVar = 0.07;
      m_fTrunkThickness = 0.0275423;
      m_fTrunkTightness = 0;
      m_fTrunkWobble = 0.04;
      m_fTrunkStickiness = 0.04;
      m_fFlowersPerLG = 4;
      m_fFlowerSize = 0.05;
      m_fFlowerSizeVar = 0.2;
      m_fFlowerDist = 0.21;
      m_fFlowerPeak = 3.66667;
      m_fFlowerDuration = 0.12;
      m_dwFlowerShape = 3;
      m_cFlower = 0xec;
      break;
   case TS_SHRUBFLOWERINGQUINCE:      // shrub, floweing quince
      m_fDefaultAge = 3;
      m_afAges[0] = 1;
      m_afHeight[0] = 0.5;
      m_afAges[1] = 3;
      m_afHeight[1] = 1.2;
      m_afAges[2] = 5;
      m_afHeight[2] = 2.5;
      m_afCanopyZ[0] = 0.1;
      m_afCanopyZ[1] = 0.4;
      m_afCanopyZ[2] = 0.7;
      m_afCanopyW[0] = 0.5;
      m_afCanopyW[1] = 0.7;
      m_afCanopyW[2] = 0.7;
      m_afCanopyW[3] = 0.2;
      m_dwLGShape = 1;
      m_adwLGNum[0][0] = 4;
      m_adwLGNum[0][1] = 4;
      m_adwLGNum[0][2] = 4;
      m_adwLGNum[1][0] = 15;
      m_adwLGNum[1][1] = 15;
      m_adwLGNum[1][2] = 15;
      m_adwLGNum[2][0] = 20;
      m_adwLGNum[2][1] = 20;
      m_adwLGNum[2][2] = 20;
      m_fLGRadius = 1.8;
      m_pLGSize.p[0] = 0.18;
      m_pLGSize.p[1] = 0.18;
      m_pLGSize.p[2] = 0.95;
      m_afLGSize[0][0] = 0.15;
      m_afLGSize[0][1] = 0.2;
      m_afLGSize[0][2] = 0.25;
      m_afLGSize[0][3] = 0.3;
      m_afLGSize[1][0] = 0.3;
      m_afLGSize[1][1] = 0.4;
      m_afLGSize[1][2] = 0.5;
      m_afLGSize[1][3] = 0.6;
      m_afLGSize[2][0] = 0.7;
      m_afLGSize[2][1] = 0.8;
      m_afLGSize[2][2] = 0.9;
      m_afLGSize[2][3] = 1;
      m_acLGColor[0][0] = 0x8000;
      m_acTrunkColor[0][0] = 0x505050;
      m_acLGColor[0][1] = 0xff00;
      m_acTrunkColor[0][1] = 0x4080;
      m_acLGColor[1][0] = 0x8000;
      m_acTrunkColor[1][0] = 0x505050;
      m_acLGColor[1][1] = 0x4000;
      m_acTrunkColor[1][1] = 0x4080;
      m_acLGColor[2][0] = 0x8000;
      m_acTrunkColor[2][0] = 0x404040;
      m_acLGColor[2][1] = 0x4000;
      m_acTrunkColor[2][1] = 0x4080;
      m_acLGColor[3][0] = 0x8000;
      m_acTrunkColor[3][0] = 0x505050;
      m_acLGColor[3][1] = 0x4000;
      m_acTrunkColor[3][1] = 0x4080;
      m_afLGDensBySeason[0] = 0.65;
      m_afLGDensBySeason[1] = 0.8;
      m_afLGDensBySeason[2] = 0.7;
      m_afLGDensBySeason[3] = 0;
      m_fLGAngleUpLight = -0.8;
      m_fLGAngleUpHeavy = -0.21;
      m_fHeightVar = 0.1;
      m_fLGNumVar = 0.12;
      m_fLGSizeVar = 0;
      m_fLGOrientVar = 0;
      m_fTrunkSkewVar = 0.07;
      m_fTrunkThickness = 0.0275423;
      m_fTrunkTightness = 0;
      m_fTrunkWobble = 0.04;
      m_fTrunkStickiness = 0.04;
      m_fFlowersPerLG = 4;
      m_fFlowerSize = 0.05;
      m_fFlowerSizeVar = 0.2;
      m_fFlowerDist = 0.21;
      m_fFlowerPeak = 3.66667;
      m_fFlowerDuration = 0.12;
      m_dwFlowerShape = 3;
      m_cFlower = 0xec;
      break;
   case TS_SHRUBBOXWOOD:      // shrub, boxwood
      m_fDefaultAge = 10;
      m_afAges[0] = 2;
      m_afHeight[0] = 0.5;
      m_afAges[1] = 10;
      m_afHeight[1] = 3;
      m_afAges[2] = 20;
      m_afHeight[2] = 5;
      m_afCanopyZ[0] = 0.1;
      m_afCanopyZ[1] = 0.4;
      m_afCanopyZ[2] = 0.7;
      m_afCanopyW[0] = 0.5;
      m_afCanopyW[1] = 0.7;
      m_afCanopyW[2] = 0.7;
      m_afCanopyW[3] = 0.2;
      m_dwLGShape = 0;
      m_adwLGNum[0][0] = 0;
      m_adwLGNum[0][1] = 1;
      m_adwLGNum[0][2] = 2;
      m_adwLGNum[1][0] = 20;
      m_adwLGNum[1][1] = 20;
      m_adwLGNum[1][2] = 20;
      m_adwLGNum[2][0] = 20;
      m_adwLGNum[2][1] = 20;
      m_adwLGNum[2][2] = 20;
      m_fLGRadius = 1.8;
      m_pLGSize.p[0] = 1.85;
      m_pLGSize.p[1] = 1.85;
      m_pLGSize.p[2] = 1.85;
      m_afLGSize[0][0] = 0.3;
      m_afLGSize[0][1] = 0.3;
      m_afLGSize[0][2] = 0.3;
      m_afLGSize[0][3] = 0.3;
      m_afLGSize[1][0] = 0.6;
      m_afLGSize[1][1] = 0.6;
      m_afLGSize[1][2] = 0.6;
      m_afLGSize[1][3] = 0.6;
      m_afLGSize[2][0] = 1;
      m_afLGSize[2][1] = 1;
      m_afLGSize[2][2] = 1;
      m_afLGSize[2][3] = 1;
      m_acLGColor[0][0] = 0x8000;
      m_acTrunkColor[0][0] = 0x505050;
      m_acLGColor[0][1] = 0x4000;
      m_acTrunkColor[0][1] = 0x4080;
      m_acLGColor[1][0] = 0x8000;
      m_acTrunkColor[1][0] = 0x505050;
      m_acLGColor[1][1] = 0x4000;
      m_acTrunkColor[1][1] = 0x4080;
      m_acLGColor[2][0] = 0x8000;
      m_acTrunkColor[2][0] = 0x404040;
      m_acLGColor[2][1] = 0x4000;
      m_acTrunkColor[2][1] = 0x4080;
      m_acLGColor[3][0] = 0x8000;
      m_acTrunkColor[3][0] = 0x505050;
      m_acLGColor[3][1] = 0x4000;
      m_acTrunkColor[3][1] = 0x4080;
      m_afLGDensBySeason[0] = 0.65;
      m_afLGDensBySeason[1] = 0.8;
      m_afLGDensBySeason[2] = 0.7;
      m_afLGDensBySeason[3] = 0;
      m_fLGAngleUpLight = 0.52;
      m_fLGAngleUpHeavy = -0.28;
      m_fHeightVar = 0.1;
      m_fLGNumVar = 0.12;
      m_fLGSizeVar = 0.07;
      m_fLGOrientVar = 0.16;
      m_fTrunkSkewVar = 0.07;
      m_fTrunkThickness = 0.0275423;
      m_fTrunkTightness = 0;
      m_fTrunkWobble = 0.04;
      m_fTrunkStickiness = 0.04;
      m_fFlowersPerLG = 0;
      m_fFlowerSize = 0.1;
      m_fFlowerSizeVar = 0.2;
      m_fFlowerDist = 0.21;
      m_fFlowerPeak = 1.33333;
      m_fFlowerDuration = 0.12;
      m_dwFlowerShape = 1;
      m_cFlower = 0xa00000;
      break;
   case TS_SHRUBBLUEBERRY:      // shrubs, blueberry
      m_fDefaultAge = 10;
      m_afAges[0] = 1;
      m_afHeight[0] = 0.5;
      m_afAges[1] = 5;
      m_afHeight[1] = 1.2;
      m_afAges[2] = 10;
      m_afHeight[2] = 2;
      m_afCanopyZ[0] = 0.1;
      m_afCanopyZ[1] = 0.4;
      m_afCanopyZ[2] = 0.6;
      m_afCanopyW[0] = 0.5;
      m_afCanopyW[1] = 0.7;
      m_afCanopyW[2] = 0.5;
      m_afCanopyW[3] = 0.2;
      m_dwLGShape = 1;
      m_adwLGNum[0][0] = 0;
      m_adwLGNum[0][1] = 3;
      m_adwLGNum[0][2] = 4;
      m_adwLGNum[1][0] = 25;
      m_adwLGNum[1][1] = 25;
      m_adwLGNum[1][2] = 20;
      m_adwLGNum[2][0] = 30;
      m_adwLGNum[2][1] = 30;
      m_adwLGNum[2][2] = 25;
      m_fLGRadius = 1.5;
      m_pLGSize.p[0] = 0.35;
      m_pLGSize.p[1] = 0.35;
      m_pLGSize.p[2] = 0.25;
      m_afLGSize[0][0] = 0.1;
      m_afLGSize[0][1] = 0.1;
      m_afLGSize[0][2] = 0.1;
      m_afLGSize[0][3] = 0.1;
      m_afLGSize[1][0] = 0.7;
      m_afLGSize[1][1] = 0.7;
      m_afLGSize[1][2] = 0.7;
      m_afLGSize[1][3] = 0.7;
      m_afLGSize[2][0] = 1;
      m_afLGSize[2][1] = 1;
      m_afLGSize[2][2] = 1;
      m_afLGSize[2][3] = 1;
      m_acLGColor[0][0] = 0xd5d5;
      m_acTrunkColor[0][0] = 0x505050;
      m_acLGColor[0][1] = 0xff00;
      m_acTrunkColor[0][1] = 0x4080;
      m_acLGColor[1][0] = 0x4000;
      m_acTrunkColor[1][0] = 0x505050;
      m_acLGColor[1][1] = 0x8000;
      m_acTrunkColor[1][1] = 0x4080;
      m_acLGColor[2][0] = 0xdae6;
      m_acTrunkColor[2][0] = 0x404040;
      m_acLGColor[2][1] = 0xce;
      m_acTrunkColor[2][1] = 0x4080;
      m_acLGColor[3][0] = 0xcaca;
      m_acTrunkColor[3][0] = 0x505050;
      m_acLGColor[3][1] = 0xff00;
      m_acTrunkColor[3][1] = 0x4080;
      m_afLGDensBySeason[0] = 0.65;
      m_afLGDensBySeason[1] = 0.8;
      m_afLGDensBySeason[2] = 0.7;
      m_afLGDensBySeason[3] = 0;
      m_fLGAngleUpLight = 0.52;
      m_fLGAngleUpHeavy = -0.28;
      m_fHeightVar = 0.1;
      m_fLGNumVar = 0.12;
      m_fLGSizeVar = 0.17;
      m_fLGOrientVar = 0.16;
      m_fTrunkSkewVar = 0.07;
      m_fTrunkThickness = 0.0398107;
      m_fTrunkTightness = 0.08;
      m_fTrunkWobble = 0.04;
      m_fTrunkStickiness = 0.09;
      m_fFlowersPerLG = 1;
      m_fFlowerSize = 0.1;
      m_fFlowerSizeVar = 0.2;
      m_fFlowerDist = 0.21;
      m_fFlowerPeak = 1.33333;
      m_fFlowerDuration = 0.12;
      m_dwFlowerShape = 1;
      m_cFlower = 0xa00000;
      break;
   case TS_MISCGRASS2:   // ornamental grass 2
      m_fDefaultAge = 3;
      m_afAges[0] = 1;
      m_afHeight[0] = 0.01;
      m_afAges[1] = 3;
      m_afHeight[1] = 0.03;
      m_afAges[2] = 5;
      m_afHeight[2] = 0.05;
      m_afCanopyZ[0] = 0.01;
      m_afCanopyZ[1] = 0.3;
      m_afCanopyZ[2] = 0.6;
      m_afCanopyW[0] = 5;
      m_afCanopyW[1] = 6;
      m_afCanopyW[2] = 6;
      m_afCanopyW[3] = 6;
      m_dwLGShape = 2;
      m_adwLGNum[0][0] = 10;
      m_adwLGNum[0][1] = 10;
      m_adwLGNum[0][2] = 30;
      m_adwLGNum[1][0] = 20;
      m_adwLGNum[1][1] = 20;
      m_adwLGNum[1][2] = 60;
      m_adwLGNum[2][0] = 20;
      m_adwLGNum[2][1] = 20;
      m_adwLGNum[2][2] = 80;
      m_fLGRadius = 1.8;
      m_pLGSize.p[0] = 1;
      m_pLGSize.p[1] = 0.05;
      m_pLGSize.p[2] = 0.1;
      m_afLGSize[0][0] = 0.15;
      m_afLGSize[0][1] = 0.2;
      m_afLGSize[0][2] = 0.25;
      m_afLGSize[0][3] = 0.3;
      m_afLGSize[1][0] = 0.3;
      m_afLGSize[1][1] = 0.4;
      m_afLGSize[1][2] = 0.5;
      m_afLGSize[1][3] = 0.6;
      m_afLGSize[2][0] = 0.4;
      m_afLGSize[2][1] = 0.6;
      m_afLGSize[2][2] = 0.8;
      m_afLGSize[2][3] = 1;
      m_acLGColor[0][0] = 0xa4ffa4;
      m_acTrunkColor[0][0] = 0x25405a;
      m_acLGColor[0][1] = 0x74dc74;
      m_acTrunkColor[0][1] = 0x8000;
      m_acLGColor[1][0] = 0x6affb5;
      m_acTrunkColor[1][0] = 0x25405a;
      m_acLGColor[1][1] = 0x66d966;
      m_acTrunkColor[1][1] = 0x8000;
      m_acLGColor[2][0] = 0x71ffb8;
      m_acTrunkColor[2][0] = 0x25405a;
      m_acLGColor[2][1] = 0x66d966;
      m_acTrunkColor[2][1] = 0x8000;
      m_acLGColor[3][0] = 0x62ffb0;
      m_acTrunkColor[3][0] = 0x25405a;
      m_acLGColor[3][1] = 0xa97af;
      m_acTrunkColor[3][1] = 0x8000;
      m_afLGDensBySeason[0] = 0.8;
      m_afLGDensBySeason[1] = 0.8;
      m_afLGDensBySeason[2] = 0.8;
      m_afLGDensBySeason[3] = 0.42;
      m_fLGAngleUpLight = 0.39;
      m_fLGAngleUpHeavy = 1;
      m_fHeightVar = 0.12;
      m_fLGNumVar = 0.21;
      m_fLGSizeVar = 0.22;
      m_fLGOrientVar = 0.12;
      m_fTrunkSkewVar = 0.13;
      m_fTrunkThickness = 0.0131826;
      m_fTrunkTightness = 0.06;
      m_fTrunkWobble = 0;
      m_fTrunkStickiness = 0;
      m_fFlowersPerLG = 0;
      m_fFlowerSize = 0.03;
      m_fFlowerSizeVar = 0;
      m_fFlowerDist = 0.14;
      m_fFlowerPeak = 3.33333;
      m_fFlowerDuration = 0.04;
      m_dwFlowerShape = 1;
      m_cFlower = 0x4080;
      break;
   case TS_MISCGRASS1:   // ornamental grass
      m_fDefaultAge = 3;
      m_afAges[0] = 1;
      m_afHeight[0] = 0.01;
      m_afAges[1] = 3;
      m_afHeight[1] = 0.03;
      m_afAges[2] = 5;
      m_afHeight[2] = 0.05;
      m_afCanopyZ[0] = 0.01;
      m_afCanopyZ[1] = 0.3;
      m_afCanopyZ[2] = 0.6;
      m_afCanopyW[0] = 5;
      m_afCanopyW[1] = 6;
      m_afCanopyW[2] = 6;
      m_afCanopyW[3] = 6;
      m_dwLGShape = 2;
      m_adwLGNum[0][0] = 20;
      m_adwLGNum[0][1] = 20;
      m_adwLGNum[0][2] = 20;
      m_adwLGNum[1][0] = 30;
      m_adwLGNum[1][1] = 30;
      m_adwLGNum[1][2] = 30;
      m_adwLGNum[2][0] = 40;
      m_adwLGNum[2][1] = 40;
      m_adwLGNum[2][2] = 40;
      m_fLGRadius = 1.8;
      m_pLGSize.p[0] = 1.5;
      m_pLGSize.p[1] = 0.04;
      m_pLGSize.p[2] = 0.4;
      m_afLGSize[0][0] = 0.2;
      m_afLGSize[0][1] = 0.18;
      m_afLGSize[0][2] = 0.16;
      m_afLGSize[0][3] = 0.14;
      m_afLGSize[1][0] = 0.6;
      m_afLGSize[1][1] = 0.58;
      m_afLGSize[1][2] = 0.56;
      m_afLGSize[1][3] = 0.52;
      m_afLGSize[2][0] = 1;
      m_afLGSize[2][1] = 0.98;
      m_afLGSize[2][2] = 0.96;
      m_afLGSize[2][3] = 0.94;
      m_acLGColor[0][0] = 0xff00;
      m_acTrunkColor[0][0] = 0x25405a;
      m_acLGColor[0][1] = 0x32c832;
      m_acTrunkColor[0][1] = 0x8000;
      m_acLGColor[1][0] = 0xff80;
      m_acTrunkColor[1][0] = 0x25405a;
      m_acLGColor[1][1] = 0x32c832;
      m_acTrunkColor[1][1] = 0x8000;
      m_acLGColor[2][0] = 0xff80;
      m_acTrunkColor[2][0] = 0x25405a;
      m_acLGColor[2][1] = 0x32c832;
      m_acTrunkColor[2][1] = 0x8000;
      m_acLGColor[3][0] = 0xff80;
      m_acTrunkColor[3][0] = 0x25405a;
      m_acLGColor[3][1] = 0xa97af;
      m_acTrunkColor[3][1] = 0x8000;
      m_afLGDensBySeason[0] = 0.8;
      m_afLGDensBySeason[1] = 0.8;
      m_afLGDensBySeason[2] = 0.8;
      m_afLGDensBySeason[3] = 0.42;
      m_fLGAngleUpLight = 1;
      m_fLGAngleUpHeavy = 0.15;
      m_fHeightVar = 0.12;
      m_fLGNumVar = 0.21;
      m_fLGSizeVar = 0;
      m_fLGOrientVar = 0.12;
      m_fTrunkSkewVar = 0.13;
      m_fTrunkThickness = 0.0131826;
      m_fTrunkTightness = 0.06;
      m_fTrunkWobble = 0;
      m_fTrunkStickiness = 0;
      m_fFlowersPerLG = 0;
      m_fFlowerSize = 0.03;
      m_fFlowerSizeVar = 0;
      m_fFlowerDist = 0.14;
      m_fFlowerPeak = 3.33333;
      m_fFlowerDuration = 0.04;
      m_dwFlowerShape = 1;
      m_cFlower = 0x4080;
      break;
   case TS_MISCBAMBOO:   // bamboo
      m_fDefaultAge = 10;
      m_afAges[0] = 2;
      m_afHeight[0] = 0.5;
      m_afAges[1] = 10;
      m_afHeight[1] = 2;
      m_afAges[2] = 15;
      m_afHeight[2] = 4;
      m_afCanopyZ[0] = 0.4;
      m_afCanopyZ[1] = 0.6;
      m_afCanopyZ[2] = 0.8;
      m_afCanopyW[0] = 0.5;
      m_afCanopyW[1] = 0.6;
      m_afCanopyW[2] = 0.6;
      m_afCanopyW[3] = 0.6;
      m_dwLGShape = 1;
      m_adwLGNum[0][0] = 5;
      m_adwLGNum[0][1] = 5;
      m_adwLGNum[0][2] = 5;
      m_adwLGNum[1][0] = 10;
      m_adwLGNum[1][1] = 10;
      m_adwLGNum[1][2] = 10;
      m_adwLGNum[2][0] = 20;
      m_adwLGNum[2][1] = 20;
      m_adwLGNum[2][2] = 20;
      m_fLGRadius = 1.8;
      m_pLGSize.p[0] = 2;
      m_pLGSize.p[1] = 0.4;
      m_pLGSize.p[2] = 0.4;
      m_afLGSize[0][0] = 0.2;
      m_afLGSize[0][1] = 0.18;
      m_afLGSize[0][2] = 0.16;
      m_afLGSize[0][3] = 0.14;
      m_afLGSize[1][0] = 0.6;
      m_afLGSize[1][1] = 0.58;
      m_afLGSize[1][2] = 0.56;
      m_afLGSize[1][3] = 0.52;
      m_afLGSize[2][0] = 1;
      m_afLGSize[2][1] = 0.98;
      m_afLGSize[2][2] = 0.96;
      m_afLGSize[2][3] = 0.94;
      m_acLGColor[0][0] = 0xff00;
      m_acTrunkColor[0][0] = 0x25405a;
      m_acLGColor[0][1] = 0x32c832;
      m_acTrunkColor[0][1] = 0x8000;
      m_acLGColor[1][0] = 0xff80;
      m_acTrunkColor[1][0] = 0x25405a;
      m_acLGColor[1][1] = 0x32c832;
      m_acTrunkColor[1][1] = 0x8000;
      m_acLGColor[2][0] = 0xff80;
      m_acTrunkColor[2][0] = 0x25405a;
      m_acLGColor[2][1] = 0x32c832;
      m_acTrunkColor[2][1] = 0x8000;
      m_acLGColor[3][0] = 0xff80;
      m_acTrunkColor[3][0] = 0x25405a;
      m_acLGColor[3][1] = 0xa97af;
      m_acTrunkColor[3][1] = 0x8000;
      m_afLGDensBySeason[0] = 0.8;
      m_afLGDensBySeason[1] = 0.8;
      m_afLGDensBySeason[2] = 0.8;
      m_afLGDensBySeason[3] = 0.42;
      m_fLGAngleUpLight = 1;
      m_fLGAngleUpHeavy = 0.53;
      m_fHeightVar = 0.12;
      m_fLGNumVar = 0.21;
      m_fLGSizeVar = 0;
      m_fLGOrientVar = 0.12;
      m_fTrunkSkewVar = 0.13;
      m_fTrunkThickness = 0.0331131;
      m_fTrunkTightness = 0.06;
      m_fTrunkWobble = 0;
      m_fTrunkStickiness = 0;
      m_fFlowersPerLG = 0;
      m_fFlowerSize = 0.03;
      m_fFlowerSizeVar = 0;
      m_fFlowerDist = 0.14;
      m_fFlowerPeak = 3.33333;
      m_fFlowerDuration = 0.04;
      m_dwFlowerShape = 1;
      m_cFlower = 0x4080;
      break;
   case TS_MISCFERN:   // ferns, fern
      m_fDefaultAge = 3;
      m_afAges[0] = 1;
      m_afHeight[0] = 0.05;
      m_afAges[1] = 3;
      m_afHeight[1] = 0.1;
      m_afAges[2] = 5;
      m_afHeight[2] = 0.2;
      m_afCanopyZ[0] = 0.91;
      m_afCanopyZ[1] = 0.94;
      m_afCanopyZ[2] = 0.97;
      m_afCanopyW[0] = 0.6;
      m_afCanopyW[1] = 0.4;
      m_afCanopyW[2] = 0.2;
      m_afCanopyW[3] = 0.02;
      m_dwLGShape = 3;
      m_adwLGNum[0][0] = 5;
      m_adwLGNum[0][1] = 5;
      m_adwLGNum[0][2] = 5;
      m_adwLGNum[1][0] = 10;
      m_adwLGNum[1][1] = 10;
      m_adwLGNum[1][2] = 10;
      m_adwLGNum[2][0] = 15;
      m_adwLGNum[2][1] = 15;
      m_adwLGNum[2][2] = 15;
      m_fLGRadius = -1;
      m_pLGSize.p[0] = 1;
      m_pLGSize.p[1] = 0.5;
      m_pLGSize.p[2] = 0.2;
      m_afLGSize[0][0] = 0.6;
      m_afLGSize[0][1] = 0.58;
      m_afLGSize[0][2] = 0.56;
      m_afLGSize[0][3] = 0.54;
      m_afLGSize[1][0] = 0.8;
      m_afLGSize[1][1] = 0.78;
      m_afLGSize[1][2] = 0.76;
      m_afLGSize[1][3] = 0.72;
      m_afLGSize[2][0] = 1;
      m_afLGSize[2][1] = 0.98;
      m_afLGSize[2][2] = 0.96;
      m_afLGSize[2][3] = 0.94;
      m_acLGColor[0][0] = 0xff00;
      m_acTrunkColor[0][0] = 0x25405a;
      m_acLGColor[0][1] = 0x32c832;
      m_acTrunkColor[0][1] = 0x101010;
      m_acLGColor[1][0] = 0x466400;
      m_acTrunkColor[1][0] = 0x25405a;
      m_acLGColor[1][1] = 0x32c832;
      m_acTrunkColor[1][1] = 0x101010;
      m_acLGColor[2][0] = 0x466400;
      m_acTrunkColor[2][0] = 0x25405a;
      m_acLGColor[2][1] = 0x32c832;
      m_acTrunkColor[2][1] = 0x101010;
      m_acLGColor[3][0] = 0x87dfef;
      m_acTrunkColor[3][0] = 0x25405a;
      m_acLGColor[3][1] = 0xa97af;
      m_acTrunkColor[3][1] = 0x101010;
      m_afLGDensBySeason[0] = 0.8;
      m_afLGDensBySeason[1] = 0.8;
      m_afLGDensBySeason[2] = 0.8;
      m_afLGDensBySeason[3] = 0.42;
      m_fLGAngleUpLight = 0.65;
      m_fLGAngleUpHeavy = -0.47;
      m_fHeightVar = 0.12;
      m_fLGNumVar = 0.21;
      m_fLGSizeVar = 0;
      m_fLGOrientVar = 0.12;
      m_fTrunkSkewVar = 0.13;
      m_fTrunkThickness = 0.0301995;
      m_fTrunkTightness = 0.8;
      m_fTrunkWobble = 0.8;
      m_fTrunkStickiness = 0.8;
      m_fFlowersPerLG = 0;
      m_fFlowerSize = 0.03;
      m_fFlowerSizeVar = 0;
      m_fFlowerDist = 0.14;
      m_fFlowerPeak = 3.33333;
      m_fFlowerDuration = 0.04;
      m_dwFlowerShape = 1;
      m_cFlower = 0x4080;
      break;
   case TS_EVERLEAFCITRUS:   // evergreen, leafy, citrus
      m_fDefaultAge = 15;
      m_afAges[0] = 2;
      m_afHeight[0] = 0.7;
      m_afAges[1] = 15;
      m_afHeight[1] = 4;
      m_afAges[2] = 25;
      m_afHeight[2] = 6;
      m_afCanopyZ[0] = 0.2;
      m_afCanopyZ[1] = 0.5;
      m_afCanopyZ[2] = 0.8;
      m_afCanopyW[0] = 0.5;
      m_afCanopyW[1] = 1;
      m_afCanopyW[2] = 0.8;
      m_afCanopyW[3] = 0.4;
      m_dwLGShape = 1;
      m_adwLGNum[0][0] = 3;
      m_adwLGNum[0][1] = 3;
      m_adwLGNum[0][2] = 3;
      m_adwLGNum[1][0] = 15;
      m_adwLGNum[1][1] = 15;
      m_adwLGNum[1][2] = 15;
      m_adwLGNum[2][0] = 20;
      m_adwLGNum[2][1] = 20;
      m_adwLGNum[2][2] = 20;
      m_fLGRadius = 1.37;
      m_pLGSize.p[0] = 1.5;
      m_pLGSize.p[1] = 1.5;
      m_pLGSize.p[2] = 1.5;
      m_afLGSize[0][0] = 0.1;
      m_afLGSize[0][1] = 0.1;
      m_afLGSize[0][2] = 0.1;
      m_afLGSize[0][3] = 0.1;
      m_afLGSize[1][0] = 0.6;
      m_afLGSize[1][1] = 0.6;
      m_afLGSize[1][2] = 0.6;
      m_afLGSize[1][3] = 0.6;
      m_afLGSize[2][0] = 1;
      m_afLGSize[2][1] = 1;
      m_afLGSize[2][2] = 1;
      m_afLGSize[2][3] = 1;
      m_acLGColor[0][0] = 0xff00;
      m_acTrunkColor[0][0] = 0x505050;
      m_acLGColor[0][1] = 0x4000;
      m_acTrunkColor[0][1] = 0x4080;
      m_acLGColor[1][0] = 0x8000;
      m_acTrunkColor[1][0] = 0x505050;
      m_acLGColor[1][1] = 0x4000;
      m_acTrunkColor[1][1] = 0x4080;
      m_acLGColor[2][0] = 0x8000;
      m_acTrunkColor[2][0] = 0x404040;
      m_acLGColor[2][1] = 0x4000;
      m_acTrunkColor[2][1] = 0x4080;
      m_acLGColor[3][0] = 0x8000;
      m_acTrunkColor[3][0] = 0x505050;
      m_acLGColor[3][1] = 0x4000;
      m_acTrunkColor[3][1] = 0x4080;
      m_afLGDensBySeason[0] = 0.8;
      m_afLGDensBySeason[1] = 0.8;
      m_afLGDensBySeason[2] = 0.8;
      m_afLGDensBySeason[3] = 0.8;
      m_fLGAngleUpLight = 0.52;
      m_fLGAngleUpHeavy = -0.28;
      m_fHeightVar = 0.1;
      m_fLGNumVar = 0.12;
      m_fLGSizeVar = 0.17;
      m_fLGOrientVar = 0.09;
      m_fTrunkSkewVar = 0.07;
      m_fTrunkThickness = 0.020893;
      m_fTrunkTightness = 0.39;
      m_fTrunkWobble = 0.2;
      m_fTrunkStickiness = 0.22;
      m_fFlowersPerLG = 5;
      m_fFlowerSize = 0.1;
      m_fFlowerSizeVar = 0.2;
      m_fFlowerDist = 0.4;
      m_fFlowerPeak = 2;
      m_fFlowerDuration = 0.3;
      m_dwFlowerShape = 1;
      m_cFlower = 0x46e0fb;
      break;
   case TS_DECIDUOUSWILLOW:   // disduous, willow
      m_fDefaultAge = 25;
      m_afAges[0] = 2;
      m_afHeight[0] = 1;
      m_afAges[1] = 25;
      m_afHeight[1] = 8;
      m_afAges[2] = 50;
      m_afHeight[2] = 15;
      m_afCanopyZ[0] = 0.2;
      m_afCanopyZ[1] = 0.4;
      m_afCanopyZ[2] = 0.8;
      m_afCanopyW[0] = 0.8;
      m_afCanopyW[1] = 1;
      m_afCanopyW[2] = 1;
      m_afCanopyW[3] = 0.15;
      m_dwLGShape = 1;
      m_adwLGNum[0][0] = 0;
      m_adwLGNum[0][1] = 3;
      m_adwLGNum[0][2] = 5;
      m_adwLGNum[1][0] = 10;
      m_adwLGNum[1][1] = 30;
      m_adwLGNum[1][2] = 35;
      m_adwLGNum[2][0] = 10;
      m_adwLGNum[2][1] = 30;
      m_adwLGNum[2][2] = 35;
      m_fLGRadius = 1.8;
      m_pLGSize.p[0] = 6.3;
      m_pLGSize.p[1] = 3.3;
      m_pLGSize.p[2] = 3;
      m_afLGSize[0][0] = 0.15;
      m_afLGSize[0][1] = 0.12;
      m_afLGSize[0][2] = 0.1;
      m_afLGSize[0][3] = 0.08;
      m_afLGSize[1][0] = 0.6;
      m_afLGSize[1][1] = 0.5;
      m_afLGSize[1][2] = 0.4;
      m_afLGSize[1][3] = 0.3;
      m_afLGSize[2][0] = 1.2;
      m_afLGSize[2][1] = 1;
      m_afLGSize[2][2] = 0.8;
      m_afLGSize[2][3] = 0.6;
      m_acLGColor[0][0] = 0xea00;
      m_acTrunkColor[0][0] = 0x505050;
      m_acLGColor[0][1] = 0x4ff88;
      m_acTrunkColor[0][1] = 0x2d59;
      m_acLGColor[1][0] = 0xb900;
      m_acTrunkColor[1][0] = 0x505050;
      m_acLGColor[1][1] = 0xd900;
      m_acTrunkColor[1][1] = 0x2851;
      m_acLGColor[2][0] = 0xff80;
      m_acTrunkColor[2][0] = 0x404040;
      m_acLGColor[2][1] = 0xecec;
      m_acTrunkColor[2][1] = 0x356a;
      m_acLGColor[3][0] = 0x64ff80;
      m_acTrunkColor[3][0] = 0x505050;
      m_acLGColor[3][1] = 0xff00;
      m_acTrunkColor[3][1] = 0x3162;
      m_afLGDensBySeason[0] = 0.65;
      m_afLGDensBySeason[1] = 0.8;
      m_afLGDensBySeason[2] = 0.7;
      m_afLGDensBySeason[3] = 0;
      m_fLGAngleUpLight = -0.36;
      m_fLGAngleUpHeavy = -0.8;
      m_fHeightVar = 0.1;
      m_fLGNumVar = 0.12;
      m_fLGSizeVar = 0.04;
      m_fLGOrientVar = 0.07;
      m_fTrunkSkewVar = 0;
      m_fTrunkThickness = 0.017378;
      m_fTrunkTightness = 0.09;
      m_fTrunkWobble = 0.23;
      m_fTrunkStickiness = 0.15;
      m_fFlowersPerLG = 0;
      m_fFlowerSize = 0.15;
      m_fFlowerSizeVar = 0.2;
      m_fFlowerDist = 0.21;
      m_fFlowerPeak = 3.66667;
      m_fFlowerDuration = 0.11;
      m_dwFlowerShape = 2;
      m_cFlower = 0xde46f9;
      break;
   case TS_DECIDUOUSMAGNOLIA:   // disiduous, magnolia
      m_fDefaultAge = 15;
      m_afAges[0] = 1;
      m_afHeight[0] = 0.5;
      m_afAges[1] = 15;
      m_afHeight[1] = 3;
      m_afAges[2] = 25;
      m_afHeight[2] = 6;
      m_afCanopyZ[0] = 0.4;
      m_afCanopyZ[1] = 0.6;
      m_afCanopyZ[2] = 0.8;
      m_afCanopyW[0] = 0.2;
      m_afCanopyW[1] = 0.6;
      m_afCanopyW[2] = 0.5;
      m_afCanopyW[3] = 0.15;
      m_dwLGShape = 1;
      m_adwLGNum[0][0] = 5;
      m_adwLGNum[0][1] = 5;
      m_adwLGNum[0][2] = 3;
      m_adwLGNum[1][0] = 10;
      m_adwLGNum[1][1] = 20;
      m_adwLGNum[1][2] = 10;
      m_adwLGNum[2][0] = 15;
      m_adwLGNum[2][1] = 30;
      m_adwLGNum[2][2] = 15;
      m_fLGRadius = 1.8;
      m_pLGSize.p[0] = 1.3;
      m_pLGSize.p[1] = 1.3;
      m_pLGSize.p[2] = 1;
      m_afLGSize[0][0] = 0.1;
      m_afLGSize[0][1] = 0.1;
      m_afLGSize[0][2] = 0.1;
      m_afLGSize[0][3] = 0.1;
      m_afLGSize[1][0] = 0.5;
      m_afLGSize[1][1] = 0.6;
      m_afLGSize[1][2] = 0.5;
      m_afLGSize[1][3] = 0.4;
      m_afLGSize[2][0] = 1;
      m_afLGSize[2][1] = 1;
      m_afLGSize[2][2] = 0.9;
      m_afLGSize[2][3] = 0.6;
      m_acLGColor[0][0] = 0xea00;
      m_acTrunkColor[0][0] = 0x505050;
      m_acLGColor[0][1] = 0x8000;
      m_acTrunkColor[0][1] = 0x2d59;
      m_acLGColor[1][0] = 0xb900;
      m_acTrunkColor[1][0] = 0x505050;
      m_acLGColor[1][1] = 0x8000;
      m_acTrunkColor[1][1] = 0x2851;
      m_acLGColor[2][0] = 0x64bbea;
      m_acTrunkColor[2][0] = 0x404040;
      m_acLGColor[2][1] = 0xecec;
      m_acTrunkColor[2][1] = 0x356a;
      m_acLGColor[3][0] = 0x64ff80;
      m_acTrunkColor[3][0] = 0x505050;
      m_acLGColor[3][1] = 0xff00;
      m_acTrunkColor[3][1] = 0x3162;
      m_afLGDensBySeason[0] = 0.65;
      m_afLGDensBySeason[1] = 0.8;
      m_afLGDensBySeason[2] = 0.7;
      m_afLGDensBySeason[3] = 0;
      m_fLGAngleUpLight = 0.52;
      m_fLGAngleUpHeavy = -0.28;
      m_fHeightVar = 0.1;
      m_fLGNumVar = 0.12;
      m_fLGSizeVar = 0.17;
      m_fLGOrientVar = 0.09;
      m_fTrunkSkewVar = 0;
      m_fTrunkThickness = 0.0229087;
      m_fTrunkTightness = 0.03;
      m_fTrunkWobble = 0.07;
      m_fTrunkStickiness = 0.06;
      m_fFlowersPerLG = 3;
      m_fFlowerSize = 0.15;
      m_fFlowerSizeVar = 0.2;
      m_fFlowerDist = 0.21;
      m_fFlowerPeak = 3.66667;
      m_fFlowerDuration = 0.11;
      m_dwFlowerShape = 2;
      m_cFlower = 0xde46f9;
      break;
   case TS_DECIDUOUSBEECH:   // disiduous, Beech
      m_fDefaultAge = 35;
      m_afAges[0] = 5;
      m_afHeight[0] = 2;
      m_afAges[1] = 35;
      m_afHeight[1] = 13;
      m_afAges[2] = 75;
      m_afHeight[2] = 25;
      m_afCanopyZ[0] = 0.05;
      m_afCanopyZ[1] = 0.3;
      m_afCanopyZ[2] = 0.7;
      m_afCanopyW[0] = 0.2;
      m_afCanopyW[1] = 0.5;
      m_afCanopyW[2] = 0.25;
      m_afCanopyW[3] = 0.05;
      m_dwLGShape = 1;
      m_adwLGNum[0][0] = 0;
      m_adwLGNum[0][1] = 0;
      m_adwLGNum[0][2] = 3;
      m_adwLGNum[1][0] = 15;
      m_adwLGNum[1][1] = 30;
      m_adwLGNum[1][2] = 12;
      m_adwLGNum[2][0] = 30;
      m_adwLGNum[2][1] = 60;
      m_adwLGNum[2][2] = 25;
      m_fLGRadius = 1.8;
      m_pLGSize.p[0] = 4.3;
      m_pLGSize.p[1] = 4.3;
      m_pLGSize.p[2] = 3;
      m_afLGSize[0][0] = 0.1;
      m_afLGSize[0][1] = 0.1;
      m_afLGSize[0][2] = 0.1;
      m_afLGSize[0][3] = 0.1;
      m_afLGSize[1][0] = 0.7;
      m_afLGSize[1][1] = 0.7;
      m_afLGSize[1][2] = 0.7;
      m_afLGSize[1][3] = 0.4;
      m_afLGSize[2][0] = 1;
      m_afLGSize[2][1] = 1;
      m_afLGSize[2][2] = 0.9;
      m_afLGSize[2][3] = 0.6;
      m_acLGColor[0][0] = 0xea00;
      m_acTrunkColor[0][0] = 0x505050;
      m_acLGColor[0][1] = 0x8000;
      m_acTrunkColor[0][1] = 0x2d59;
      m_acLGColor[1][0] = 0xb900;
      m_acTrunkColor[1][0] = 0x505050;
      m_acLGColor[1][1] = 0x8000;
      m_acTrunkColor[1][1] = 0x2851;
      m_acLGColor[2][0] = 0x64bbea;
      m_acTrunkColor[2][0] = 0x404040;
      m_acLGColor[2][1] = 0xecec;
      m_acTrunkColor[2][1] = 0x356a;
      m_acLGColor[3][0] = 0x64ff80;
      m_acTrunkColor[3][0] = 0x505050;
      m_acLGColor[3][1] = 0xff00;
      m_acTrunkColor[3][1] = 0x3162;
      m_afLGDensBySeason[0] = 0.65;
      m_afLGDensBySeason[1] = 0.8;
      m_afLGDensBySeason[2] = 0.7;
      m_afLGDensBySeason[3] = 0;
      m_fLGAngleUpLight = 0.52;
      m_fLGAngleUpHeavy = -0.28;
      m_fHeightVar = 0.1;
      m_fLGNumVar = 0.12;
      m_fLGSizeVar = 0.17;
      m_fLGOrientVar = 0.09;
      m_fTrunkSkewVar = 0;
      m_fTrunkThickness = 0.01;
      m_fTrunkTightness = 0.23;
      m_fTrunkWobble = 0.2;
      m_fTrunkStickiness = 0.09;
      m_fFlowersPerLG = 0;
      m_fFlowerSize = 0.15;
      m_fFlowerSizeVar = 0.2;
      m_fFlowerDist = 0.21;
      m_fFlowerPeak = 3.33333;
      m_fFlowerDuration = 0.08;
      m_dwFlowerShape = 2;
      m_cFlower = 0xd6d5ff;
      break;
   case TS_DECIDUOUSOAK:   // disidous, Oak
      m_fDefaultAge = 25;
      m_afAges[0] = 2;
      m_afHeight[0] = 0.7;
      m_afAges[1] = 25;
      m_afHeight[1] = 12;
      m_afAges[2] = 50;
      m_afHeight[2] = 20;
      m_afCanopyZ[0] = 0.2;
      m_afCanopyZ[1] = 0.45;
      m_afCanopyZ[2] = 0.8;
      m_afCanopyW[0] = 0.8;
      m_afCanopyW[1] = 1;
      m_afCanopyW[2] = 0.8;
      m_afCanopyW[3] = 0.4;
      m_dwLGShape = 1;
      m_adwLGNum[0][0] = 0;
      m_adwLGNum[0][1] = 0;
      m_adwLGNum[0][2] = 3;
      m_adwLGNum[1][0] = 20;
      m_adwLGNum[1][1] = 30;
      m_adwLGNum[1][2] = 20;
      m_adwLGNum[2][0] = 50;
      m_adwLGNum[2][1] = 60;
      m_adwLGNum[2][2] = 45;
      m_fLGRadius = 1.4;
      m_pLGSize.p[0] = 5.3;
      m_pLGSize.p[1] = 5.3;
      m_pLGSize.p[2] = 3;
      m_afLGSize[0][0] = 0.1;
      m_afLGSize[0][1] = 0.1;
      m_afLGSize[0][2] = 0.1;
      m_afLGSize[0][3] = 0.1;
      m_afLGSize[1][0] = 0.7;
      m_afLGSize[1][1] = 0.7;
      m_afLGSize[1][2] = 0.7;
      m_afLGSize[1][3] = 0.7;
      m_afLGSize[2][0] = 1;
      m_afLGSize[2][1] = 1;
      m_afLGSize[2][2] = 1;
      m_afLGSize[2][3] = 1;
      m_acLGColor[0][0] = 0xea00;
      m_acTrunkColor[0][0] = 0x505050;
      m_acLGColor[0][1] = 0x8000;
      m_acTrunkColor[0][1] = 0x2d59;
      m_acLGColor[1][0] = 0xb900;
      m_acTrunkColor[1][0] = 0x505050;
      m_acLGColor[1][1] = 0x8000;
      m_acTrunkColor[1][1] = 0x2851;
      m_acLGColor[2][0] = 0x64bbea;
      m_acTrunkColor[2][0] = 0x404040;
      m_acLGColor[2][1] = 0xecec;
      m_acTrunkColor[2][1] = 0x356a;
      m_acLGColor[3][0] = 0x64ff80;
      m_acTrunkColor[3][0] = 0x505050;
      m_acLGColor[3][1] = 0xff00;
      m_acTrunkColor[3][1] = 0x3162;
      m_afLGDensBySeason[0] = 0.65;
      m_afLGDensBySeason[1] = 0.8;
      m_afLGDensBySeason[2] = 0.7;
      m_afLGDensBySeason[3] = 0;
      m_fLGAngleUpLight = 0.52;
      m_fLGAngleUpHeavy = -0.28;
      m_fHeightVar = 0.1;
      m_fLGNumVar = 0.12;
      m_fLGSizeVar = 0.17;
      m_fLGOrientVar = 0.09;
      m_fTrunkSkewVar = 0;
      m_fTrunkThickness = 0.01;
      m_fTrunkTightness = 0.23;
      m_fTrunkWobble = 0.2;
      m_fTrunkStickiness = 0.09;
      m_fFlowersPerLG = 0;
      m_fFlowerSize = 0.15;
      m_fFlowerSizeVar = 0.2;
      m_fFlowerDist = 0.21;
      m_fFlowerPeak = 3.33333;
      m_fFlowerDuration = 0.08;
      m_dwFlowerShape = 2;
      m_cFlower = 0xd6d5ff;
      break;
   case TS_DECIDUOUSCHERRY:   // disiduous, Cherry
      m_fDefaultAge = 15;
      m_afAges[0] = 2;
      m_afHeight[0] = 0.7;
      m_afAges[1] = 15;
      m_afHeight[1] = 4;
      m_afAges[2] = 25;
      m_afHeight[2] = 8;
      m_afCanopyZ[0] = 0.3;
      m_afCanopyZ[1] = 0.6;
      m_afCanopyZ[2] = 0.8;
      m_afCanopyW[0] = 0.5;
      m_afCanopyW[1] = 0.8;
      m_afCanopyW[2] = 0.8;
      m_afCanopyW[3] = 0.4;
      m_dwLGShape = 1;
      m_adwLGNum[0][0] = 3;
      m_adwLGNum[0][1] = 3;
      m_adwLGNum[0][2] = 3;
      m_adwLGNum[1][0] = 15;
      m_adwLGNum[1][1] = 20;
      m_adwLGNum[1][2] = 12;
      m_adwLGNum[2][0] = 30;
      m_adwLGNum[2][1] = 30;
      m_adwLGNum[2][2] = 25;
      m_fLGRadius = 1.37;
      m_pLGSize.p[0] = 1.3;
      m_pLGSize.p[1] = 1.3;
      m_pLGSize.p[2] = 1;
      m_afLGSize[0][0] = 0.1;
      m_afLGSize[0][1] = 0.1;
      m_afLGSize[0][2] = 0.1;
      m_afLGSize[0][3] = 0.1;
      m_afLGSize[1][0] = 0.6;
      m_afLGSize[1][1] = 0.6;
      m_afLGSize[1][2] = 0.6;
      m_afLGSize[1][3] = 0.6;
      m_afLGSize[2][0] = 1;
      m_afLGSize[2][1] = 1;
      m_afLGSize[2][2] = 1;
      m_afLGSize[2][3] = 1;
      m_acLGColor[0][0] = 0x64c850;
      m_acTrunkColor[0][0] = 0x505050;
      m_acLGColor[0][1] = 0x8000;
      m_acTrunkColor[0][1] = 0x2d59;
      m_acLGColor[1][0] = 0x40bc00;
      m_acTrunkColor[1][0] = 0x505050;
      m_acLGColor[1][1] = 0x8000;
      m_acTrunkColor[1][1] = 0x2851;
      m_acLGColor[2][0] = 0xec76;
      m_acTrunkColor[2][0] = 0x404040;
      m_acLGColor[2][1] = 0xecec;
      m_acTrunkColor[2][1] = 0x356a;
      m_acLGColor[3][0] = 0x64ff80;
      m_acTrunkColor[3][0] = 0x505050;
      m_acLGColor[3][1] = 0xff00;
      m_acTrunkColor[3][1] = 0x3162;
      m_afLGDensBySeason[0] = 0.65;
      m_afLGDensBySeason[1] = 0.8;
      m_afLGDensBySeason[2] = 0.7;
      m_afLGDensBySeason[3] = 0;
      m_fLGAngleUpLight = 0.52;
      m_fLGAngleUpHeavy = -0.28;
      m_fHeightVar = 0.1;
      m_fLGNumVar = 0.12;
      m_fLGSizeVar = 0.17;
      m_fLGOrientVar = 0.09;
      m_fTrunkSkewVar = 0.07;
      m_fTrunkThickness = 0.0436516;
      m_fTrunkTightness = 0.03;
      m_fTrunkWobble = 0.2;
      m_fTrunkStickiness = 0.09;
      m_fFlowersPerLG = 5;
      m_fFlowerSize = 0.15;
      m_fFlowerSizeVar = 0.2;
      m_fFlowerDist = 0.21;
      m_fFlowerPeak = 3.33333;
      m_fFlowerDuration = 0.08;
      m_dwFlowerShape = 2;
      m_cFlower = 0xd6d5ff;
      break;
   case TS_DECIDUOUSDOGWOOD:   // disiduous, Dogwood
      m_fDefaultAge = 15;
      m_afAges[0] = 2;
      m_afHeight[0] = 0.7;
      m_afAges[1] = 15;
      m_afHeight[1] = 4;
      m_afAges[2] = 25;
      m_afHeight[2] = 8;
      m_afCanopyZ[0] = 0.4;
      m_afCanopyZ[1] = 0.6;
      m_afCanopyZ[2] = 0.8;
      m_afCanopyW[0] = 0.5;
      m_afCanopyW[1] = 0.7;
      m_afCanopyW[2] = 0.7;
      m_afCanopyW[3] = 0.4;
      m_dwLGShape = 1;
      m_adwLGNum[0][0] = 3;
      m_adwLGNum[0][1] = 3;
      m_adwLGNum[0][2] = 3;
      m_adwLGNum[1][0] = 15;
      m_adwLGNum[1][1] = 20;
      m_adwLGNum[1][2] = 12;
      m_adwLGNum[2][0] = 30;
      m_adwLGNum[2][1] = 30;
      m_adwLGNum[2][2] = 25;
      m_fLGRadius = 1.37;
      m_pLGSize.p[0] = 1.5;
      m_pLGSize.p[1] = 1.5;
      m_pLGSize.p[2] = 1.5;
      m_afLGSize[0][0] = 0.1;
      m_afLGSize[0][1] = 0.1;
      m_afLGSize[0][2] = 0.1;
      m_afLGSize[0][3] = 0.1;
      m_afLGSize[1][0] = 0.6;
      m_afLGSize[1][1] = 0.6;
      m_afLGSize[1][2] = 0.6;
      m_afLGSize[1][3] = 0.6;
      m_afLGSize[2][0] = 1;
      m_afLGSize[2][1] = 1;
      m_afLGSize[2][2] = 1;
      m_afLGSize[2][3] = 1;
      m_acLGColor[0][0] = 0x64c850;
      m_acTrunkColor[0][0] = 0x505050;
      m_acLGColor[0][1] = 0x8000;
      m_acTrunkColor[0][1] = 0x4080;
      m_acLGColor[1][0] = 0x40bc00;
      m_acTrunkColor[1][0] = 0x505050;
      m_acLGColor[1][1] = 0x8000;
      m_acTrunkColor[1][1] = 0x4080;
      m_acLGColor[2][0] = 0x80ff;
      m_acTrunkColor[2][0] = 0x404040;
      m_acLGColor[2][1] = 0x80;
      m_acTrunkColor[2][1] = 0x4080;
      m_acLGColor[3][0] = 0x64ff80;
      m_acTrunkColor[3][0] = 0x505050;
      m_acLGColor[3][1] = 0xff00;
      m_acTrunkColor[3][1] = 0x4080;
      m_afLGDensBySeason[0] = 0.65;
      m_afLGDensBySeason[1] = 0.8;
      m_afLGDensBySeason[2] = 0.7;
      m_afLGDensBySeason[3] = 0;
      m_fLGAngleUpLight = 0.52;
      m_fLGAngleUpHeavy = -0.28;
      m_fHeightVar = 0.1;
      m_fLGNumVar = 0.12;
      m_fLGSizeVar = 0.17;
      m_fLGOrientVar = 0.09;
      m_fTrunkSkewVar = 0.07;
      m_fTrunkThickness = 0.020893;
      m_fTrunkTightness = 0.38;
      m_fTrunkWobble = 0.2;
      m_fTrunkStickiness = 0.18;
      m_fFlowersPerLG = 5;
      m_fFlowerSize = 0.15;
      m_fFlowerSizeVar = 0.2;
      m_fFlowerDist = 0.21;
      m_fFlowerPeak = 3.33333;
      m_fFlowerDuration = 0.08;
      m_dwFlowerShape = 2;
      m_cFlower = 0xa8a6ff;
      break;
   case TS_DECIDUOUSBIRCH:   // disiduous, Birch
      m_fDefaultAge = 25;
      m_afAges[0] = 2;
      m_afHeight[0] = 1;
      m_afAges[1] = 25;
      m_afHeight[1] = 10;
      m_afAges[2] = 50;
      m_afHeight[2] = 20;
      m_afCanopyZ[0] = 0.5;
      m_afCanopyZ[1] = 0.7;
      m_afCanopyZ[2] = 0.85;
      m_afCanopyW[0] = 0.5;
      m_afCanopyW[1] = 0.6;
      m_afCanopyW[2] = 0.7;
      m_afCanopyW[3] = 0.4;
      m_dwLGShape = 1;
      m_adwLGNum[0][0] = 3;
      m_adwLGNum[0][1] = 3;
      m_adwLGNum[0][2] = 3;
      m_adwLGNum[1][0] = 15;
      m_adwLGNum[1][1] = 20;
      m_adwLGNum[1][2] = 12;
      m_adwLGNum[2][0] = 30;
      m_adwLGNum[2][1] = 40;
      m_adwLGNum[2][2] = 25;
      m_fLGRadius = 1.37;
      m_pLGSize.p[0] = 2.5;
      m_pLGSize.p[1] = 2.5;
      m_pLGSize.p[2] = 2.5;
      m_afLGSize[0][0] = 0.12;
      m_afLGSize[0][1] = 0.1;
      m_afLGSize[0][2] = 0.09;
      m_afLGSize[0][3] = 0.08;
      m_afLGSize[1][0] = 0.7;
      m_afLGSize[1][1] = 0.6;
      m_afLGSize[1][2] = 0.5;
      m_afLGSize[1][3] = 0.4;
      m_afLGSize[2][0] = 1.2;
      m_afLGSize[2][1] = 1;
      m_afLGSize[2][2] = 0.9;
      m_afLGSize[2][3] = 0.8;
      m_acLGColor[0][0] = 0x64c850;
      m_acTrunkColor[0][0] = 0xf0f0f0;
      m_acLGColor[0][1] = 0x28ff64;
      m_acTrunkColor[0][1] = 0x99cccc;
      m_acLGColor[1][0] = 0x40bc00;
      m_acTrunkColor[1][0] = 0xf0f0f0;
      m_acLGColor[1][1] = 0x649b00;
      m_acTrunkColor[1][1] = 0x99cccc;
      m_acLGColor[2][0] = 0x28ffac;
      m_acTrunkColor[2][0] = 0xf0f0f0;
      m_acLGColor[2][1] = 0xffff;
      m_acTrunkColor[2][1] = 0x8dc7c7;
      m_acLGColor[3][0] = 0x64ff80;
      m_acTrunkColor[3][0] = 0xf0f0f0;
      m_acLGColor[3][1] = 0xffff;
      m_acTrunkColor[3][1] = 0x9fcece;
      m_afLGDensBySeason[0] = 0.65;
      m_afLGDensBySeason[1] = 0.8;
      m_afLGDensBySeason[2] = 0.7;
      m_afLGDensBySeason[3] = 0;
      m_fLGAngleUpLight = 0.11;
      m_fLGAngleUpHeavy = -0.8;
      m_fHeightVar = 0.1;
      m_fLGNumVar = 0.12;
      m_fLGSizeVar = 0.17;
      m_fLGOrientVar = 0.09;
      m_fTrunkSkewVar = 0;
      m_fTrunkThickness = 0.020893;
      m_fTrunkTightness = 0.08;
      m_fTrunkWobble = 0.01;
      m_fTrunkStickiness = 0.07;
      m_fFlowersPerLG = 0;
      m_fFlowerSize = 0.1;
      m_fFlowerSizeVar = 0.2;
      m_fFlowerDist = 0.1;
      m_fFlowerPeak = 2;
      m_fFlowerDuration = 0.1;
      m_dwFlowerShape = 4;
      m_cFlower = 0xff;
      break;
   case TS_DECIDUOUSMIMOSA:   // disidual, Mimosa
      m_fDefaultAge = 15;
      m_afAges[0] = 2;
      m_afHeight[0] = 0.1;
      m_afAges[1] = 15;
      m_afHeight[1] = 5;
      m_afAges[2] = 25;
      m_afHeight[2] = 10;
      m_afCanopyZ[0] = 0.4;
      m_afCanopyZ[1] = 0.6;
      m_afCanopyZ[2] = 0.8;
      m_afCanopyW[0] = 1.5;
      m_afCanopyW[1] = 1.2;
      m_afCanopyW[2] = 1.1;
      m_afCanopyW[3] = 0.4;
      m_dwLGShape = 1;
      m_adwLGNum[0][0] = 3;
      m_adwLGNum[0][1] = 3;
      m_adwLGNum[0][2] = 3;
      m_adwLGNum[1][0] = 20;
      m_adwLGNum[1][1] = 30;
      m_adwLGNum[1][2] = 15;
      m_adwLGNum[2][0] = 30;
      m_adwLGNum[2][1] = 40;
      m_adwLGNum[2][2] = 25;
      m_fLGRadius = 1.37;
      m_pLGSize.p[0] = 3.5;
      m_pLGSize.p[1] = 3.5;
      m_pLGSize.p[2] = 0.75;
      m_afLGSize[0][0] = 0.12;
      m_afLGSize[0][1] = 0.1;
      m_afLGSize[0][2] = 0.09;
      m_afLGSize[0][3] = 0.08;
      m_afLGSize[1][0] = 0.7;
      m_afLGSize[1][1] = 0.6;
      m_afLGSize[1][2] = 0.5;
      m_afLGSize[1][3] = 0.4;
      m_afLGSize[2][0] = 1.2;
      m_afLGSize[2][1] = 1;
      m_afLGSize[2][2] = 0.9;
      m_afLGSize[2][3] = 0.8;
      m_acLGColor[0][0] = 0xc850;
      m_acTrunkColor[0][0] = 0x808080;
      m_acLGColor[0][1] = 0xff64;
      m_acTrunkColor[0][1] = 0x3e7d7d;
      m_acLGColor[1][0] = 0x40bc00;
      m_acTrunkColor[1][0] = 0x808080;
      m_acLGColor[1][1] = 0x9b00;
      m_acTrunkColor[1][1] = 0x3c7777;
      m_acLGColor[2][0] = 0xffac;
      m_acTrunkColor[2][0] = 0x808080;
      m_acLGColor[2][1] = 0x3e9fc;
      m_acTrunkColor[2][1] = 0x3e7d7d;
      m_acLGColor[3][0] = 0xff80;
      m_acTrunkColor[3][0] = 0x808080;
      m_acLGColor[3][1] = 0xffff;
      m_acTrunkColor[3][1] = 0x3c7777;
      m_afLGDensBySeason[0] = 0.65;
      m_afLGDensBySeason[1] = 0.8;
      m_afLGDensBySeason[2] = 0.7;
      m_afLGDensBySeason[3] = 0;
      m_fLGAngleUpLight = 0.11;
      m_fLGAngleUpHeavy = -0.8;
      m_fHeightVar = 0.1;
      m_fLGNumVar = 0.12;
      m_fLGSizeVar = 0.17;
      m_fLGOrientVar = 0.04;
      m_fTrunkSkewVar = 0;
      m_fTrunkThickness = 0.0158489;
      m_fTrunkTightness = 0.48;
      m_fTrunkWobble = 0.19;
      m_fTrunkStickiness = 0.04;
      m_fFlowersPerLG = 0;
      m_fFlowerSize = 0.1;
      m_fFlowerSizeVar = 0.2;
      m_fFlowerDist = 0.1;
      m_fFlowerPeak = 2;
      m_fFlowerDuration = 0.1;
      m_dwFlowerShape = 4;
      m_cFlower = 0xff;
      break;
   case TS_DECIDUOUSMAPLEJAPANESE:   // disiduous, maple, Japanese
      m_fDefaultAge = 15;
      m_afAges[0] = 2;
      m_afHeight[0] = 0.5;
      m_afAges[1] = 15;
      m_afHeight[1] = 3;
      m_afAges[2] = 25;
      m_afHeight[2] = 6;
      m_afCanopyZ[0] = 0.4;
      m_afCanopyZ[1] = 0.5;
      m_afCanopyZ[2] = 0.8;
      m_afCanopyW[0] = 0.4;
      m_afCanopyW[1] = 0.9;
      m_afCanopyW[2] = 0.9;
      m_afCanopyW[3] = 0.4;
      m_dwLGShape = 1;
      m_adwLGNum[0][0] = 0;
      m_adwLGNum[0][1] = 0;
      m_adwLGNum[0][2] = 5;
      m_adwLGNum[1][0] = 5;
      m_adwLGNum[1][1] = 20;
      m_adwLGNum[1][2] = 15;
      m_adwLGNum[2][0] = 10;
      m_adwLGNum[2][1] = 40;
      m_adwLGNum[2][2] = 25;
      m_fLGRadius = 1.37;
      m_pLGSize.p[0] = 1.5;
      m_pLGSize.p[1] = 1.5;
      m_pLGSize.p[2] = 0.75;
      m_afLGSize[0][0] = 0.1;
      m_afLGSize[0][1] = 0.1;
      m_afLGSize[0][2] = 0.1;
      m_afLGSize[0][3] = 0.1;
      m_afLGSize[1][0] = 0.7;
      m_afLGSize[1][1] = 0.7;
      m_afLGSize[1][2] = 0.7;
      m_afLGSize[1][3] = 0.7;
      m_afLGSize[2][0] = 1;
      m_afLGSize[2][1] = 1;
      m_afLGSize[2][2] = 1;
      m_afLGSize[2][3] = 1;
      m_acLGColor[0][0] = 0xc850;
      m_acTrunkColor[0][0] = 0x404040;
      m_acLGColor[0][1] = 0xff64;
      m_acTrunkColor[0][1] = 0x3e7d7d;
      m_acLGColor[1][0] = 0x40bc00;
      m_acTrunkColor[1][0] = 0x404040;
      m_acLGColor[1][1] = 0x9b00;
      m_acTrunkColor[1][1] = 0x3c7777;
      m_acLGColor[2][0] = 0xc8;
      m_acTrunkColor[2][0] = 0x404040;
      m_acLGColor[2][1] = 0x80;
      m_acTrunkColor[2][1] = 0x3e7d7d;
      m_acLGColor[3][0] = 0xff80;
      m_acTrunkColor[3][0] = 0x404040;
      m_acLGColor[3][1] = 0xffff;
      m_acTrunkColor[3][1] = 0x3c7777;
      m_afLGDensBySeason[0] = 0.8;
      m_afLGDensBySeason[1] = 0.8;
      m_afLGDensBySeason[2] = 0.7;
      m_afLGDensBySeason[3] = 0;
      m_fLGAngleUpLight = 0.39;
      m_fLGAngleUpHeavy = -0.26;
      m_fHeightVar = 0.1;
      m_fLGNumVar = 0.12;
      m_fLGSizeVar = 0.17;
      m_fLGOrientVar = 0.22;
      m_fTrunkSkewVar = 0;
      m_fTrunkThickness = 0.0158489;
      m_fTrunkTightness = 0.24;
      m_fTrunkWobble = 0.19;
      m_fTrunkStickiness = 0.04;
      m_fFlowersPerLG = 0;
      m_fFlowerSize = 0.1;
      m_fFlowerSizeVar = 0.2;
      m_fFlowerDist = 0.1;
      m_fFlowerPeak = 2;
      m_fFlowerDuration = 0.1;
      m_dwFlowerShape = 4;
      m_cFlower = 0xff;
      break;
   case TS_DECIDUOUSMAPLESUGAR:   // disiduous, maple, sugar
      m_fDefaultAge = 25;
      m_afAges[0] = 2;
      m_afHeight[0] = 1;
      m_afAges[1] = 25;
      m_afHeight[1] = 15;
      m_afAges[2] = 50;
      m_afHeight[2] = 25;
      m_afCanopyZ[0] = 0.2;
      m_afCanopyZ[1] = 0.3;
      m_afCanopyZ[2] = 0.7;
      m_afCanopyW[0] = 0.4;
      m_afCanopyW[1] = 0.9;
      m_afCanopyW[2] = 0.7;
      m_afCanopyW[3] = 0.4;
      m_dwLGShape = 1;
      m_adwLGNum[0][0] = 0;
      m_adwLGNum[0][1] = 0;
      m_adwLGNum[0][2] = 5;
      m_adwLGNum[1][0] = 5;
      m_adwLGNum[1][1] = 40;
      m_adwLGNum[1][2] = 25;
      m_adwLGNum[2][0] = 10;
      m_adwLGNum[2][1] = 80;
      m_adwLGNum[2][2] = 50;
      m_fLGRadius = 1.37;
      m_pLGSize.p[0] = 5.5;
      m_pLGSize.p[1] = 5.5;
      m_pLGSize.p[2] = 3.5;
      m_afLGSize[0][0] = 0.1;
      m_afLGSize[0][1] = 0.1;
      m_afLGSize[0][2] = 0.1;
      m_afLGSize[0][3] = 0.1;
      m_afLGSize[1][0] = 0.7;
      m_afLGSize[1][1] = 0.7;
      m_afLGSize[1][2] = 0.7;
      m_afLGSize[1][3] = 0.7;
      m_afLGSize[2][0] = 1;
      m_afLGSize[2][1] = 1;
      m_afLGSize[2][2] = 1;
      m_afLGSize[2][3] = 1;
      m_acLGColor[0][0] = 0x8000;
      m_acTrunkColor[0][0] = 0x404040;
      m_acLGColor[0][1] = 0xff00;
      m_acTrunkColor[0][1] = 0x3e7d7d;
      m_acLGColor[1][0] = 0x8000;
      m_acTrunkColor[1][0] = 0x404040;
      m_acLGColor[1][1] = 0x9b00;
      m_acTrunkColor[1][1] = 0x3c7777;
      m_acLGColor[2][0] = 0xff;
      m_acTrunkColor[2][0] = 0x404040;
      m_acLGColor[2][1] = 0xffff;
      m_acTrunkColor[2][1] = 0x3e7d7d;
      m_acLGColor[3][0] = 0xff80;
      m_acTrunkColor[3][0] = 0x404040;
      m_acLGColor[3][1] = 0xffff;
      m_acTrunkColor[3][1] = 0x3c7777;
      m_afLGDensBySeason[0] = 0.8;
      m_afLGDensBySeason[1] = 0.8;
      m_afLGDensBySeason[2] = 0.8;
      m_afLGDensBySeason[3] = 0;
      m_fLGAngleUpLight = 0.39;
      m_fLGAngleUpHeavy = -0.26;
      m_fHeightVar = 0.3;
      m_fLGNumVar = 0.3;
      m_fLGSizeVar = 0.3;
      m_fLGOrientVar = 0.28;
      m_fTrunkSkewVar = 0;
      m_fTrunkThickness = 0.01;  // BUGFIX: was .0131826;
      m_fTrunkTightness = 0.17;
      m_fTrunkWobble = 0.19;
      m_fTrunkStickiness = 0.12;
      m_fFlowersPerLG = 0;
      m_fFlowerSize = 0.1;
      m_fFlowerSizeVar = 0.2;
      m_fFlowerDist = 0.1;
      m_fFlowerPeak = 2;
      m_fFlowerDuration = 0.1;
      m_dwFlowerShape = 4;
      m_cFlower = 0xff;
      break;
   case TS_PALMCYCAD:   // palm, cycad
      m_fDefaultAge = 25;
      m_afAges[0] = 2;
      m_afHeight[0] = 0.05;
      m_afAges[1] = 25;
      m_afHeight[1] = 1;
      m_afAges[2] = 50;
      m_afHeight[2] = 2;
      m_afCanopyZ[0] = 0.91;
      m_afCanopyZ[1] = 0.94;
      m_afCanopyZ[2] = 0.97;
      m_afCanopyW[0] = 0.6;
      m_afCanopyW[1] = 0.4;
      m_afCanopyW[2] = 0.2;
      m_afCanopyW[3] = 0.02;
      m_dwLGShape = 3;
      m_adwLGNum[0][0] = 6;
      m_adwLGNum[0][1] = 6;
      m_adwLGNum[0][2] = 6;
      m_adwLGNum[1][0] = 10;
      m_adwLGNum[1][1] = 10;
      m_adwLGNum[1][2] = 10;
      m_adwLGNum[2][0] = 10;
      m_adwLGNum[2][1] = 10;
      m_adwLGNum[2][2] = 10;
      m_fLGRadius = -1;
      m_pLGSize.p[0] = 1;
      m_pLGSize.p[1] = 0.3;
      m_pLGSize.p[2] = 0.2;
      m_afLGSize[0][0] = 0.7;
      m_afLGSize[0][1] = 0.68;
      m_afLGSize[0][2] = 0.66;
      m_afLGSize[0][3] = 0.64;
      m_afLGSize[1][0] = 1;
      m_afLGSize[1][1] = 0.98;
      m_afLGSize[1][2] = 0.96;
      m_afLGSize[1][3] = 0.94;
      m_afLGSize[2][0] = 1;
      m_afLGSize[2][1] = 0.98;
      m_afLGSize[2][2] = 0.96;
      m_afLGSize[2][3] = 0.94;
      m_acLGColor[0][0] = 0xff00;
      m_acTrunkColor[0][0] = 0x25405a;
      m_acLGColor[0][1] = 0x32c832;
      m_acTrunkColor[0][1] = 0x101010;
      m_acLGColor[1][0] = 0x466400;
      m_acTrunkColor[1][0] = 0x25405a;
      m_acLGColor[1][1] = 0x32c832;
      m_acTrunkColor[1][1] = 0x101010;
      m_acLGColor[2][0] = 0x466400;
      m_acTrunkColor[2][0] = 0x25405a;
      m_acLGColor[2][1] = 0x32c832;
      m_acTrunkColor[2][1] = 0x101010;
      m_acLGColor[3][0] = 0x466400;
      m_acTrunkColor[3][0] = 0x25405a;
      m_acLGColor[3][1] = 0x32c832;
      m_acTrunkColor[3][1] = 0x101010;
      m_afLGDensBySeason[0] = 0.8;
      m_afLGDensBySeason[1] = 0.8;
      m_afLGDensBySeason[2] = 0.8;
      m_afLGDensBySeason[3] = 0;
      m_fLGAngleUpLight = 1;
      m_fLGAngleUpHeavy = -0.36;
      m_fHeightVar = 0.12;
      m_fLGNumVar = 0.21;
      m_fLGSizeVar = 0;
      m_fLGOrientVar = 0.12;
      m_fTrunkSkewVar = 0.13;
      m_fTrunkThickness = 0.0758578;
      m_fTrunkTightness = 0.8;
      m_fTrunkWobble = 0.8;
      m_fTrunkStickiness = 0.8;
      m_fFlowersPerLG = 5;
      m_fFlowerSize = 0.03;
      m_fFlowerSizeVar = 0;
      m_fFlowerDist = 0.14;
      m_fFlowerPeak = 3.33333;
      m_fFlowerDuration = 0.04;
      m_dwFlowerShape = 1;
      m_cFlower = 0x4080;
      break;
   case TS_PALMDATE:   // palm, date
      m_fDefaultAge = 25;
      m_afAges[0] = 2;
      m_afHeight[0] = 0.5;
      m_afAges[1] = 25;
      m_afHeight[1] = 3;
      m_afAges[2] = 50;
      m_afHeight[2] = 6;
      m_afCanopyZ[0] = 0.91;
      m_afCanopyZ[1] = 0.94;
      m_afCanopyZ[2] = 0.97;
      m_afCanopyW[0] = 0.6;
      m_afCanopyW[1] = 0.4;
      m_afCanopyW[2] = 0.2;
      m_afCanopyW[3] = 0.02;
      m_dwLGShape = 2;
      m_adwLGNum[0][0] = 6;
      m_adwLGNum[0][1] = 6;
      m_adwLGNum[0][2] = 6;
      m_adwLGNum[1][0] = 9;
      m_adwLGNum[1][1] = 9;
      m_adwLGNum[1][2] = 9;
      m_adwLGNum[2][0] = 10;
      m_adwLGNum[2][1] = 10;
      m_adwLGNum[2][2] = 10;
      m_fLGRadius = -1;
      m_pLGSize.p[0] = 1.5;
      m_pLGSize.p[1] = 0.75;
      m_pLGSize.p[2] = 0.2;
      m_afLGSize[0][0] = 0.7;
      m_afLGSize[0][1] = 0.68;
      m_afLGSize[0][2] = 0.66;
      m_afLGSize[0][3] = 0.64;
      m_afLGSize[1][0] = 1;
      m_afLGSize[1][1] = 0.98;
      m_afLGSize[1][2] = 0.96;
      m_afLGSize[1][3] = 0.94;
      m_afLGSize[2][0] = 1;
      m_afLGSize[2][1] = 0.98;
      m_afLGSize[2][2] = 0.96;
      m_afLGSize[2][3] = 0.94;
      m_acLGColor[0][0] = 0x466400;
      m_acTrunkColor[0][0] = 0x25405a;
      m_acLGColor[0][1] = 0x8000;
      m_acTrunkColor[0][1] = 0x507070;
      m_acLGColor[1][0] = 0x466400;
      m_acTrunkColor[1][0] = 0x25405a;
      m_acLGColor[1][1] = 0x8000;
      m_acTrunkColor[1][1] = 0x507070;
      m_acLGColor[2][0] = 0x466400;
      m_acTrunkColor[2][0] = 0x25405a;
      m_acLGColor[2][1] = 0x8000;
      m_acTrunkColor[2][1] = 0x507070;
      m_acLGColor[3][0] = 0x466400;
      m_acTrunkColor[3][0] = 0x25405a;
      m_acLGColor[3][1] = 0x8000;
      m_acTrunkColor[3][1] = 0x507070;
      m_afLGDensBySeason[0] = 0.8;
      m_afLGDensBySeason[1] = 0.8;
      m_afLGDensBySeason[2] = 0.8;
      m_afLGDensBySeason[3] = 0.8;
      m_fLGAngleUpLight = 1;
      m_fLGAngleUpHeavy = -0.8;
      m_fHeightVar = 0.12;
      m_fLGNumVar = 0.21;
      m_fLGSizeVar = 0;
      m_fLGOrientVar = 0.12;
      m_fTrunkSkewVar = 0.13;
      m_fTrunkThickness = 0.0758578;
      m_fTrunkTightness = 0.8;
      m_fTrunkWobble = 0.8;
      m_fTrunkStickiness = 0.8;
      m_fFlowersPerLG = 0;
      m_fFlowerSize = 0.3;
      m_fFlowerSizeVar = 0.2;
      m_fFlowerDist = 0.34;
      m_fFlowerPeak = 3.33333;
      m_fFlowerDuration = 0.52;
      m_dwFlowerShape = 1;
      m_cFlower = 0x315e;
      break;
   case TS_PALMLIVISTONA:   // palm, livistona
      m_fDefaultAge = 15;
      m_afAges[0] = 2;
      m_afHeight[0] = 0.5;
      m_afAges[1] = 15;
      m_afHeight[1] = 4;
      m_afAges[2] = 25;
      m_afHeight[2] = 6;
      m_afCanopyZ[0] = 0.75;
      m_afCanopyZ[1] = 0.85;
      m_afCanopyZ[2] = 0.92;
      m_afCanopyW[0] = 0.1;
      m_afCanopyW[1] = 0.2;
      m_afCanopyW[2] = 0.2;
      m_afCanopyW[3] = 0.1;
      m_dwLGShape = 6;
      m_adwLGNum[0][0] = 4;
      m_adwLGNum[0][1] = 4;
      m_adwLGNum[0][2] = 4;
      m_adwLGNum[1][0] = 15;
      m_adwLGNum[1][1] = 15;
      m_adwLGNum[1][2] = 15;
      m_adwLGNum[2][0] = 20;
      m_adwLGNum[2][1] = 20;
      m_adwLGNum[2][2] = 20;
      m_fLGRadius = 1.8;
      m_pLGSize.p[0] = 1;
      m_pLGSize.p[1] = 1;
      m_pLGSize.p[2] = 0.1;
      m_afLGSize[0][0] = 0.25;
      m_afLGSize[0][1] = 0.24;
      m_afLGSize[0][2] = 0.23;
      m_afLGSize[0][3] = 0.21;
      m_afLGSize[1][0] = 1;
      m_afLGSize[1][1] = 0.99;
      m_afLGSize[1][2] = 0.98;
      m_afLGSize[1][3] = 0.95;
      m_afLGSize[2][0] = 1;
      m_afLGSize[2][1] = 0.99;
      m_afLGSize[2][2] = 0.98;
      m_afLGSize[2][3] = 0.95;
      m_acLGColor[0][0] = 0x6a00;
      m_acTrunkColor[0][0] = 0x303030;
      m_acLGColor[0][1] = 0x8000;
      m_acTrunkColor[0][1] = 0x507070;
      m_acLGColor[1][0] = 0x6a00;
      m_acTrunkColor[1][0] = 0x303030;
      m_acLGColor[1][1] = 0x8000;
      m_acTrunkColor[1][1] = 0x507070;
      m_acLGColor[2][0] = 0x7100;
      m_acTrunkColor[2][0] = 0x303030;
      m_acLGColor[2][1] = 0x8000;
      m_acTrunkColor[2][1] = 0x507070;
      m_acLGColor[3][0] = 0x6a00;
      m_acTrunkColor[3][0] = 0x303030;
      m_acLGColor[3][1] = 0x8000;
      m_acTrunkColor[3][1] = 0x507070;
      m_afLGDensBySeason[0] = 0.8;
      m_afLGDensBySeason[1] = 0.8;
      m_afLGDensBySeason[2] = 0.8;
      m_afLGDensBySeason[3] = 0.8;
      m_fLGAngleUpLight = 1;
      m_fLGAngleUpHeavy = -0.8;
      m_fHeightVar = 0.12;
      m_fLGNumVar = 0.21;
      m_fLGSizeVar = 0;
      m_fLGOrientVar = 0.12;
      m_fTrunkSkewVar = 0.13;
      m_fTrunkThickness = 0.0758578;
      m_fTrunkTightness = 0.8;
      m_fTrunkWobble = 0.8;
      m_fTrunkStickiness = 0.8;
      m_fFlowersPerLG = 0;
      m_fFlowerSize = 0.3;
      m_fFlowerSizeVar = 0.2;
      m_fFlowerDist = 0.34;
      m_fFlowerPeak = 3.33333;
      m_fFlowerDuration = 0.52;
      m_dwFlowerShape = 1;
      m_cFlower = 0x315e;
      break;
   case TS_PALMSAND:   // palm, sand
      m_fDefaultAge = 5;
      m_afAges[0] = 1;
      m_afHeight[0] = 0.25;
      m_afAges[1] = 5;
      m_afHeight[1] = 1.2;
      m_afAges[2] = 10;
      m_afHeight[2] = 2.5;
      m_afCanopyZ[0] = 0.75;
      m_afCanopyZ[1] = 0.85;
      m_afCanopyZ[2] = 0.92;
      m_afCanopyW[0] = 0.1;
      m_afCanopyW[1] = 0.2;
      m_afCanopyW[2] = 0.2;
      m_afCanopyW[3] = 0.1;
      m_dwLGShape = 6;
      m_adwLGNum[0][0] = 4;
      m_adwLGNum[0][1] = 4;
      m_adwLGNum[0][2] = 4;
      m_adwLGNum[1][0] = 8;
      m_adwLGNum[1][1] = 8;
      m_adwLGNum[1][2] = 8;
      m_adwLGNum[2][0] = 10;
      m_adwLGNum[2][1] = 10;
      m_adwLGNum[2][2] = 10;
      m_fLGRadius = 1.79;
      m_pLGSize.p[0] = 0.5;
      m_pLGSize.p[1] = 0.5;
      m_pLGSize.p[2] = 0.1;
      m_afLGSize[0][0] = 0.25;
      m_afLGSize[0][1] = 0.24;
      m_afLGSize[0][2] = 0.23;
      m_afLGSize[0][3] = 0.21;
      m_afLGSize[1][0] = 1;
      m_afLGSize[1][1] = 0.99;
      m_afLGSize[1][2] = 0.98;
      m_afLGSize[1][3] = 0.95;
      m_afLGSize[2][0] = 1;
      m_afLGSize[2][1] = 0.99;
      m_afLGSize[2][2] = 0.98;
      m_afLGSize[2][3] = 0.95;
      m_acLGColor[0][0] = 0x6a00;
      m_acTrunkColor[0][0] = 0x303030;
      m_acLGColor[0][1] = 0x8000;
      m_acTrunkColor[0][1] = 0x507070;
      m_acLGColor[1][0] = 0x6a00;
      m_acTrunkColor[1][0] = 0x303030;
      m_acLGColor[1][1] = 0x8000;
      m_acTrunkColor[1][1] = 0x507070;
      m_acLGColor[2][0] = 0x7100;
      m_acTrunkColor[2][0] = 0x303030;
      m_acLGColor[2][1] = 0x8000;
      m_acTrunkColor[2][1] = 0x507070;
      m_acLGColor[3][0] = 0x6a00;
      m_acTrunkColor[3][0] = 0x303030;
      m_acLGColor[3][1] = 0x8000;
      m_acTrunkColor[3][1] = 0x507070;
      m_afLGDensBySeason[0] = 0.8;
      m_afLGDensBySeason[1] = 0.8;
      m_afLGDensBySeason[2] = 0.8;
      m_afLGDensBySeason[3] = 0.8;
      m_fLGAngleUpLight = 1;
      m_fLGAngleUpHeavy = -0.8;
      m_fHeightVar = 0.25;
      m_fLGNumVar = 0.2;
      m_fLGSizeVar = 0;
      m_fLGOrientVar = 0.11;
      m_fTrunkSkewVar = 0.12;
      m_fTrunkThickness = 0.0758578;
      m_fTrunkTightness = 0.8;
      m_fTrunkWobble = 0.8;
      m_fTrunkStickiness = 0.8;
      m_fFlowersPerLG = 0;
      m_fFlowerSize = 0.3;
      m_fFlowerSizeVar = 0.2;
      m_fFlowerDist = 0.34;
      m_fFlowerPeak = 3.33333;
      m_fFlowerDuration = 0.51;
      m_dwFlowerShape = 1;
      m_cFlower = 0x315e;
      break;
   case TS_PALMCARPENTARIA:   // palm, carpentaria
      m_fDefaultAge = 12;
      m_afAges[0] = 2;
      m_afHeight[0] = 1;
      m_afAges[1] = 25;
      m_afHeight[1] = 12;
      m_afAges[2] = 50;
      m_afHeight[2] = 20;
      m_afCanopyZ[0] = 0.94;
      m_afCanopyZ[1] = 0.96;
      m_afCanopyZ[2] = 0.98;
      m_afCanopyW[0] = 0.6;
      m_afCanopyW[1] = 0.4;
      m_afCanopyW[2] = 0.2;
      m_afCanopyW[3] = 0.02;
      m_dwLGShape = 2;
      m_adwLGNum[0][0] = 4;
      m_adwLGNum[0][1] = 4;
      m_adwLGNum[0][2] = 4;
      m_adwLGNum[1][0] = 8;
      m_adwLGNum[1][1] = 8;
      m_adwLGNum[1][2] = 8;
      m_adwLGNum[2][0] = 10;
      m_adwLGNum[2][1] = 10;
      m_adwLGNum[2][2] = 10;
      m_fLGRadius = -1;
      m_pLGSize.p[0] = 5;
      m_pLGSize.p[1] = 2;
      m_pLGSize.p[2] = 2;
      m_afLGSize[0][0] = 0.2;
      m_afLGSize[0][1] = 0.15;
      m_afLGSize[0][2] = 0.1;
      m_afLGSize[0][3] = 0.05;
      m_afLGSize[1][0] = 0.7;
      m_afLGSize[1][1] = 0.8;
      m_afLGSize[1][2] = 0.7;
      m_afLGSize[1][3] = 0.6;
      m_afLGSize[2][0] = 1;
      m_afLGSize[2][1] = 0.9;
      m_afLGSize[2][2] = 0.8;
      m_afLGSize[2][3] = 0.7;
      m_acLGColor[0][0] = 0xb000;
      m_acTrunkColor[0][0] = 0xd0d0d0;
      m_acLGColor[0][1] = 0x8000;
      m_acTrunkColor[0][1] = 0x507070;
      m_acLGColor[1][0] = 0xb000;
      m_acTrunkColor[1][0] = 0xc0c0c0;
      m_acLGColor[1][1] = 0x8000;
      m_acTrunkColor[1][1] = 0x507070;
      m_acLGColor[2][0] = 0x9f00;
      m_acTrunkColor[2][0] = 0xc0c0c0;
      m_acLGColor[2][1] = 0x8000;
      m_acTrunkColor[2][1] = 0x507070;
      m_acLGColor[3][0] = 0xb000;
      m_acTrunkColor[3][0] = 0xc0c0c0;
      m_acLGColor[3][1] = 0x8000;
      m_acTrunkColor[3][1] = 0x507070;
      m_afLGDensBySeason[0] = 0.8;
      m_afLGDensBySeason[1] = 0.8;
      m_afLGDensBySeason[2] = 0.8;
      m_afLGDensBySeason[3] = 0.8;
      m_fLGAngleUpLight = 0.4;
      m_fLGAngleUpHeavy = -0.8;
      m_fHeightVar = 0.12;
      m_fLGNumVar = 0.21;
      m_fLGSizeVar = 0.04;
      m_fLGOrientVar = 0.12;
      m_fTrunkSkewVar = 0.13;
      m_fTrunkThickness = 0.0301995;
      m_fTrunkTightness = 0.8;
      m_fTrunkWobble = 0.8;
      m_fTrunkStickiness = 0.8;
      m_fFlowersPerLG = 0;
      m_fFlowerSize = 0.3;
      m_fFlowerSizeVar = 0.2;
      m_fFlowerDist = 0.34;
      m_fFlowerPeak = 3.33333;
      m_fFlowerDuration = 0.52;
      m_dwFlowerShape = 1;
      m_cFlower = 0x315e;
      break;
   case TS_EVERLEAFBANYAN:   // evergreen, leafy, Banyan
      m_fDefaultAge = 25;
      m_afAges[0] = 2;
      m_afHeight[0] = 1;
      m_afAges[1] = 25;
      m_afHeight[1] = 7;
      m_afAges[2] = 50;
      m_afHeight[2] = 12;
      m_afCanopyZ[0] = 0.2;
      m_afCanopyZ[1] = 0.4;
      m_afCanopyZ[2] = 0.7;
      m_afCanopyW[0] = 0.6;
      m_afCanopyW[1] = 1.2;
      m_afCanopyW[2] = 1.2;
      m_afCanopyW[3] = 0.6;
      m_dwLGShape = 1;
      m_adwLGNum[0][0] = 0;
      m_adwLGNum[0][1] = 5;
      m_adwLGNum[0][2] = 5;
      m_adwLGNum[1][0] = 25;
      m_adwLGNum[1][1] = 25;
      m_adwLGNum[1][2] = 30;
      m_adwLGNum[2][0] = 35;
      m_adwLGNum[2][1] = 35;
      m_adwLGNum[2][2] = 35;
      m_fLGRadius = 1.6;
      m_pLGSize.p[0] = 3;
      m_pLGSize.p[1] = 3;
      m_pLGSize.p[2] = 3;
      m_afLGSize[0][0] = 0.1;
      m_afLGSize[0][1] = 0.1;
      m_afLGSize[0][2] = 0.1;
      m_afLGSize[0][3] = 0.1;
      m_afLGSize[1][0] = 0.6;
      m_afLGSize[1][1] = 0.6;
      m_afLGSize[1][2] = 0.6;
      m_afLGSize[1][3] = 0.6;
      m_afLGSize[2][0] = 1;
      m_afLGSize[2][1] = 1;
      m_afLGSize[2][2] = 1;
      m_afLGSize[2][3] = 1;
      m_acLGColor[0][0] = 0x8000;
      m_acTrunkColor[0][0] = 0xaaaa;
      m_acLGColor[0][1] = 0x4000;
      m_acTrunkColor[0][1] = 0x3c8296;
      m_acLGColor[1][0] = 0x8000;
      m_acTrunkColor[1][0] = 0xaaaa;
      m_acLGColor[1][1] = 0x4000;
      m_acTrunkColor[1][1] = 0x3c8296;
      m_acLGColor[2][0] = 0x8000;
      m_acTrunkColor[2][0] = 0xaaaa;
      m_acLGColor[2][1] = 0x4000;
      m_acTrunkColor[2][1] = 0x3c8296;
      m_acLGColor[3][0] = 0x8000;
      m_acTrunkColor[3][0] = 0xaaaa;
      m_acLGColor[3][1] = 0x4000;
      m_acTrunkColor[3][1] = 0x3c8296;
      m_afLGDensBySeason[0] = 0.8;
      m_afLGDensBySeason[1] = 0.8;
      m_afLGDensBySeason[2] = 0.8;
      m_afLGDensBySeason[3] = 0.8;
      m_fLGAngleUpLight = 0.39;
      m_fLGAngleUpHeavy = -0.26;
      m_fHeightVar = 0.1;
      m_fLGNumVar = 0.3;
      m_fLGSizeVar = 0.3;
      m_fLGOrientVar = 0.28;
      m_fTrunkSkewVar = 0.08;
      m_fTrunkThickness = 0.017378;
      m_fTrunkTightness = 0.2;
      m_fTrunkWobble = 0.24;
      m_fTrunkStickiness = 0.07;
      m_fFlowersPerLG = 0;
      m_fFlowerSize = 0.1;
      m_fFlowerSizeVar = 0.2;
      m_fFlowerDist = 0.1;
      m_fFlowerPeak = 2;
      m_fFlowerDuration = 0.1;
      m_dwFlowerShape = 4;
      m_cFlower = 0xff;
      break;
   case TS_EVERLEAFBOAB:   // evergreen, leafy, boab
      m_fDefaultAge = 25;
      m_afAges[0] = 2;
      m_afHeight[0] = 1;
      m_afAges[1] = 25;
      m_afHeight[1] = 7;
      m_afAges[2] = 50;
      m_afHeight[2] = 12;
      m_afCanopyZ[0] = 0.4;
      m_afCanopyZ[1] = 0.6;
      m_afCanopyZ[2] = 0.8;
      m_afCanopyW[0] = 0.6;
      m_afCanopyW[1] = 1;
      m_afCanopyW[2] = 1;
      m_afCanopyW[3] = 0.6;
      m_dwLGShape = 1;
      m_adwLGNum[0][0] = 0;
      m_adwLGNum[0][1] = 5;
      m_adwLGNum[0][2] = 5;
      m_adwLGNum[1][0] = 25;
      m_adwLGNum[1][1] = 25;
      m_adwLGNum[1][2] = 30;
      m_adwLGNum[2][0] = 25;
      m_adwLGNum[2][1] = 25;
      m_adwLGNum[2][2] = 30;
      m_fLGRadius = 1.8;
      m_pLGSize.p[0] = 2.5;
      m_pLGSize.p[1] = 2.5;
      m_pLGSize.p[2] = 1.5;
      m_afLGSize[0][0] = 0.1;
      m_afLGSize[0][1] = 0.1;
      m_afLGSize[0][2] = 0.1;
      m_afLGSize[0][3] = 0.1;
      m_afLGSize[1][0] = 0.6;
      m_afLGSize[1][1] = 0.6;
      m_afLGSize[1][2] = 0.6;
      m_afLGSize[1][3] = 0.6;
      m_afLGSize[2][0] = 1;
      m_afLGSize[2][1] = 1;
      m_afLGSize[2][2] = 1;
      m_afLGSize[2][3] = 1;
      m_acLGColor[0][0] = 0x8000;
      m_acTrunkColor[0][0] = 0xb0b0b0;
      m_acLGColor[0][1] = 0x406040;
      m_acTrunkColor[0][1] = 0x3c8296;
      m_acLGColor[1][0] = 0x8000;
      m_acTrunkColor[1][0] = 0x808080;
      m_acLGColor[1][1] = 0x40603f;
      m_acTrunkColor[1][1] = 0x3c8296;
      m_acLGColor[2][0] = 0x8000;
      m_acTrunkColor[2][0] = 0xb0b0b0;
      m_acLGColor[2][1] = 0x406040;
      m_acTrunkColor[2][1] = 0x3c8296;
      m_acLGColor[3][0] = 0x8000;
      m_acTrunkColor[3][0] = 0xb0b0b0;
      m_acLGColor[3][1] = 0x406040;
      m_acTrunkColor[3][1] = 0x3c8296;
      m_afLGDensBySeason[0] = 0.8;
      m_afLGDensBySeason[1] = 0.8;
      m_afLGDensBySeason[2] = 0.8;
      m_afLGDensBySeason[3] = 0.65;
      m_fLGAngleUpLight = 0.39;
      m_fLGAngleUpHeavy = -0.26;
      m_fHeightVar = 0.1;
      m_fLGNumVar = 0.3;
      m_fLGSizeVar = 0.3;
      m_fLGOrientVar = 0.28;
      m_fTrunkSkewVar = 0;
      m_fTrunkThickness = 0.0630957;
      m_fTrunkTightness = 0.73;
      m_fTrunkWobble = 0.66;
      m_fTrunkStickiness = 0.01;
      m_fFlowersPerLG = 0;
      m_fFlowerSize = 0.1;
      m_fFlowerSizeVar = 0.2;
      m_fFlowerDist = 0.1;
      m_fFlowerPeak = 2;
      m_fFlowerDuration = 0.1;
      m_dwFlowerShape = 4;
      m_cFlower = 0xff;
      break;
   case TS_EVERLEAFMELALEUCA:   // evergree, leafy, melaleuca
      m_fDefaultAge = 12;
      m_afAges[0] = 1;
      m_afHeight[0] = 1;
      m_afAges[1] = 12;
      m_afHeight[1] = 10;
      m_afAges[2] = 20;
      m_afHeight[2] = 15;
      m_afCanopyZ[0] = 0.2;
      m_afCanopyZ[1] = 0.6;
      m_afCanopyZ[2] = 0.8;
      m_afCanopyW[0] = 0.6;
      m_afCanopyW[1] = 1;
      m_afCanopyW[2] = 1;
      m_afCanopyW[3] = 0.5;
      m_dwLGShape = 1;
      m_adwLGNum[0][0] = 0;
      m_adwLGNum[0][1] = 0;
      m_adwLGNum[0][2] = 5;
      m_adwLGNum[1][0] = 25;
      m_adwLGNum[1][1] = 25;
      m_adwLGNum[1][2] = 25;
      m_adwLGNum[2][0] = 25;
      m_adwLGNum[2][1] = 25;
      m_adwLGNum[2][2] = 25;
      m_fLGRadius = 0.95;
      m_pLGSize.p[0] = 3.5;
      m_pLGSize.p[1] = 3.5;
      m_pLGSize.p[2] = 2;
      m_afLGSize[0][0] = 0.1;
      m_afLGSize[0][1] = 0.1;
      m_afLGSize[0][2] = 0.1;
      m_afLGSize[0][3] = 0.1;
      m_afLGSize[1][0] = 0.6;
      m_afLGSize[1][1] = 0.6;
      m_afLGSize[1][2] = 0.6;
      m_afLGSize[1][3] = 0.6;
      m_afLGSize[2][0] = 1;
      m_afLGSize[2][1] = 1;
      m_afLGSize[2][2] = 1;
      m_afLGSize[2][3] = 1;
      m_acLGColor[0][0] = 0x8000;
      m_acTrunkColor[0][0] = 0xb0b0b0;
      m_acLGColor[0][1] = 0xb900;
      m_acTrunkColor[0][1] = 0x8dc7c7;
      m_acLGColor[1][0] = 0x8000;
      m_acTrunkColor[1][0] = 0x808080;
      m_acLGColor[1][1] = 0xca00;
      m_acTrunkColor[1][1] = 0x85c2c2;
      m_acLGColor[2][0] = 0x8000;
      m_acTrunkColor[2][0] = 0xb0b0b0;
      m_acLGColor[2][1] = 0xa800;
      m_acTrunkColor[2][1] = 0x8dc7c7;
      m_acLGColor[3][0] = 0x8000;
      m_acTrunkColor[3][0] = 0xb0b0b0;
      m_acLGColor[3][1] = 0xb000;
      m_acTrunkColor[3][1] = 0x84c1c1;
      m_afLGDensBySeason[0] = 0.8;
      m_afLGDensBySeason[1] = 0.8;
      m_afLGDensBySeason[2] = 0.8;
      m_afLGDensBySeason[3] = 0.8;
      m_fLGAngleUpLight = 0.39;
      m_fLGAngleUpHeavy = -0.26;
      m_fHeightVar = 0.1;
      m_fLGNumVar = 0.3;
      m_fLGSizeVar = 0.3;
      m_fLGOrientVar = 0.28;
      m_fTrunkSkewVar = 0.13;
      m_fTrunkThickness = 0.0275423;
      m_fTrunkTightness = 0.04;
      m_fTrunkWobble = 0.09;
      m_fTrunkStickiness = 0.11;
      m_fFlowersPerLG = 0;
      m_fFlowerSize = 0.1;
      m_fFlowerSizeVar = 0.2;
      m_fFlowerDist = 0.1;
      m_fFlowerPeak = 2;
      m_fFlowerDuration = 0.1;
      m_dwFlowerShape = 4;
      m_cFlower = 0xff;
      break;
   case TS_EVERLEAFEUCALYPTSALMONGUM:   // evergreen, leafy, eucalypt, salmon gum
      m_fDefaultAge = 8;
      m_afAges[0] = 1;
      m_afHeight[0] = 1;
      m_afAges[1] = 8;
      m_afHeight[1] = 4;
      m_afAges[2] = 15;
      m_afHeight[2] = 8;
      m_afCanopyZ[0] = 0.3;
      m_afCanopyZ[1] = 0.6;
      m_afCanopyZ[2] = 0.8;
      m_afCanopyW[0] = 0.6;
      m_afCanopyW[1] = 0.8;
      m_afCanopyW[2] = 1;
      m_afCanopyW[3] = 0.5;
      m_dwLGShape = 1;
      m_adwLGNum[0][0] = 0;
      m_adwLGNum[0][1] = 5;
      m_adwLGNum[0][2] = 5;
      m_adwLGNum[1][0] = 10;
      m_adwLGNum[1][1] = 18;
      m_adwLGNum[1][2] = 25;
      m_adwLGNum[2][0] = 15;
      m_adwLGNum[2][1] = 25;
      m_adwLGNum[2][2] = 35;
      m_fLGRadius = 0.95;
      m_pLGSize.p[0] = 1.5;
      m_pLGSize.p[1] = 1.5;
      m_pLGSize.p[2] = 1;
      m_afLGSize[0][0] = 0.1;
      m_afLGSize[0][1] = 0.1;
      m_afLGSize[0][2] = 0.1;
      m_afLGSize[0][3] = 0.1;
      m_afLGSize[1][0] = 0.6;
      m_afLGSize[1][1] = 0.6;
      m_afLGSize[1][2] = 0.6;
      m_afLGSize[1][3] = 0.6;
      m_afLGSize[2][0] = 1;
      m_afLGSize[2][1] = 1;
      m_afLGSize[2][2] = 1;
      m_afLGSize[2][3] = 1;
      m_acLGColor[0][0] = 0x8000;
      m_acTrunkColor[0][0] = 0xb0b0b0;
      m_acLGColor[0][1] = 0xb900;
      m_acTrunkColor[0][1] = 0x8dc7c7;
      m_acLGColor[1][0] = 0x8000;
      m_acTrunkColor[1][0] = 0x808080;
      m_acLGColor[1][1] = 0xca00;
      m_acTrunkColor[1][1] = 0x972d0;
      m_acLGColor[2][0] = 0x8000;
      m_acTrunkColor[2][0] = 0xb0b0b0;
      m_acLGColor[2][1] = 0xa800;
      m_acTrunkColor[2][1] = 0x8dc7c7;
      m_acLGColor[3][0] = 0x8040;
      m_acTrunkColor[3][0] = 0xb0b0b0;
      m_acLGColor[3][1] = 0xb000;
      m_acTrunkColor[3][1] = 0x84c1c1;
      m_afLGDensBySeason[0] = 0.8;
      m_afLGDensBySeason[1] = 0.8;
      m_afLGDensBySeason[2] = 0.72;
      m_afLGDensBySeason[3] = 0.65;
      m_fLGAngleUpLight = 0.39;
      m_fLGAngleUpHeavy = -0.26;
      m_fHeightVar = 0.3;
      m_fLGNumVar = 0.3;
      m_fLGSizeVar = 0.3;
      m_fLGOrientVar = 0.28;
      m_fTrunkSkewVar = 0.13;
      m_fTrunkThickness = 0.0251189;
      m_fTrunkTightness = 0.15;
      m_fTrunkWobble = 0.22;
      m_fTrunkStickiness = 0.14;
      m_fFlowersPerLG = 0;
      m_fFlowerSize = 0.1;
      m_fFlowerSizeVar = 0.2;
      m_fFlowerDist = 0.1;
      m_fFlowerPeak = 2;
      m_fFlowerDuration = 0.1;
      m_dwFlowerShape = 4;
      m_cFlower = 0xff;
      break;
   case TS_EVERLEAFEUCALYPTREDRIVER:   // evergreen, leafy, eucalypt, red river gum
      m_fDefaultAge = 15;
      m_afAges[0] = 1;
      m_afHeight[0] = 1.5;
      m_afAges[1] = 15;
      m_afHeight[1] = 10;
      m_afAges[2] = 25;
      m_afHeight[2] = 15;
      m_afCanopyZ[0] = 0.4;
      m_afCanopyZ[1] = 0.6;
      m_afCanopyZ[2] = 0.8;
      m_afCanopyW[0] = 0.4;
      m_afCanopyW[1] = 0.8;
      m_afCanopyW[2] = 1;
      m_afCanopyW[3] = 0.5;
      m_dwLGShape = 1;
      m_adwLGNum[0][0] = 0;
      m_adwLGNum[0][1] = 5;
      m_adwLGNum[0][2] = 10;
      m_adwLGNum[1][0] = 8;
      m_adwLGNum[1][1] = 20;
      m_adwLGNum[1][2] = 40;
      m_adwLGNum[2][0] = 10;
      m_adwLGNum[2][1] = 30;
      m_adwLGNum[2][2] = 50;
      m_fLGRadius = 0.95;
      m_pLGSize.p[0] = 2.5;
      m_pLGSize.p[1] = 2.5;
      m_pLGSize.p[2] = 1.5;
      m_afLGSize[0][0] = 0.1;
      m_afLGSize[0][1] = 0.1;
      m_afLGSize[0][2] = 0.1;
      m_afLGSize[0][3] = 0.1;
      m_afLGSize[1][0] = 0.7;
      m_afLGSize[1][1] = 0.7;
      m_afLGSize[1][2] = 0.7;
      m_afLGSize[1][3] = 0.7;
      m_afLGSize[2][0] = 1;
      m_afLGSize[2][1] = 1;
      m_afLGSize[2][2] = 1;
      m_afLGSize[2][3] = 1;
      m_acLGColor[0][0] = 0x8000;
      m_acTrunkColor[0][0] = 0xb0b0b0;
      m_acLGColor[0][1] = 0xff00;
      m_acTrunkColor[0][1] = 0x8dc7c7;
      m_acLGColor[1][0] = 0x8000;
      m_acTrunkColor[1][0] = 0xb0b0b0;
      m_acLGColor[1][1] = 0xff00;
      m_acTrunkColor[1][1] = 0x84c1c1;
      m_acLGColor[2][0] = 0x8000;
      m_acTrunkColor[2][0] = 0xb0b0b0;
      m_acLGColor[2][1] = 0xff00;
      m_acTrunkColor[2][1] = 0x8dc7c7;
      m_acLGColor[3][0] = 0x8040;
      m_acTrunkColor[3][0] = 0xb0b0b0;
      m_acLGColor[3][1] = 0xff00;
      m_acTrunkColor[3][1] = 0x84c1c1;
      m_afLGDensBySeason[0] = 0.8;
      m_afLGDensBySeason[1] = 0.8;
      m_afLGDensBySeason[2] = 0.72;
      m_afLGDensBySeason[3] = 0.65;
      m_fLGAngleUpLight = 0.39;
      m_fLGAngleUpHeavy = -0.26;
      m_fHeightVar = 0.3;
      m_fLGNumVar = 0.3;
      m_fLGSizeVar = 0.3;
      m_fLGOrientVar = 0.28;
      m_fTrunkSkewVar = 0.13;
      m_fTrunkThickness = 0.0251189;
      m_fTrunkTightness = 0.13;
      m_fTrunkWobble = 0.22;
      m_fTrunkStickiness = 0.12;
      m_fFlowersPerLG = 0;
      m_fFlowerSize = 0.1;
      m_fFlowerSizeVar = 0.2;
      m_fFlowerDist = 0.1;
      m_fFlowerPeak = 2;
      m_fFlowerDuration = 0.1;
      m_dwFlowerShape = 4;
      m_cFlower = 0xff;
      break;
   case TS_EVERLEAFEUCALYPTSMALL:   // evergreen, leafy, eucalypt, generic small
      m_fDefaultAge = 15;
      m_afAges[0] = 1;
      m_afHeight[0] = 1;
      m_afAges[1] = 15;
      m_afHeight[1] = 5;
      m_afAges[2] = 25;
      m_afHeight[2] = 10;
      m_afCanopyZ[0] = 0.4;
      m_afCanopyZ[1] = 0.6;
      m_afCanopyZ[2] = 0.8;
      m_afCanopyW[0] = 0.4;
      m_afCanopyW[1] = 0.8;
      m_afCanopyW[2] = 1;
      m_afCanopyW[3] = 0.5;
      m_dwLGShape = 1;
      m_adwLGNum[0][0] = 0;
      m_adwLGNum[0][1] = 0;
      m_adwLGNum[0][2] = 10;
      m_adwLGNum[1][0] = 0;
      m_adwLGNum[1][1] = 10;
      m_adwLGNum[1][2] = 15;
      m_adwLGNum[2][0] = 8;
      m_adwLGNum[2][1] = 15;
      m_adwLGNum[2][2] = 20;
      m_fLGRadius = 0.95;
      m_pLGSize.p[0] = 2;
      m_pLGSize.p[1] = 2;
      m_pLGSize.p[2] = 1.5;
      m_afLGSize[0][0] = 0.1;
      m_afLGSize[0][1] = 0.1;
      m_afLGSize[0][2] = 0.1;
      m_afLGSize[0][3] = 0.1;
      m_afLGSize[1][0] = 0.7;
      m_afLGSize[1][1] = 0.7;
      m_afLGSize[1][2] = 0.7;
      m_afLGSize[1][3] = 0.7;
      m_afLGSize[2][0] = 1;
      m_afLGSize[2][1] = 1;
      m_afLGSize[2][2] = 1;
      m_afLGSize[2][3] = 1;
      m_acLGColor[0][0] = 0x8000;
      m_acTrunkColor[0][0] = 0xb0b0b0;
      m_acLGColor[0][1] = 0x408000;
      m_acTrunkColor[0][1] = 0x8dc7c7;
      m_acLGColor[1][0] = 0x8000;
      m_acTrunkColor[1][0] = 0xb0b0b0;
      m_acLGColor[1][1] = 0x408000;
      m_acTrunkColor[1][1] = 0x84c1c1;
      m_acLGColor[2][0] = 0x8000;
      m_acTrunkColor[2][0] = 0xb0b0b0;
      m_acLGColor[2][1] = 0x408000;
      m_acTrunkColor[2][1] = 0x8dc7c7;
      m_acLGColor[3][0] = 0x8040;
      m_acTrunkColor[3][0] = 0xb0b0b0;
      m_acLGColor[3][1] = 0x408000;
      m_acTrunkColor[3][1] = 0x84c1c1;
      m_afLGDensBySeason[0] = 0.8;
      m_afLGDensBySeason[1] = 0.8;
      m_afLGDensBySeason[2] = 0.72;
      m_afLGDensBySeason[3] = 0.65;
      m_fLGAngleUpLight = 0.39;
      m_fLGAngleUpHeavy = -0.26;
      m_fHeightVar = 0.17;
      m_fLGNumVar = 0.17;
      m_fLGSizeVar = 0.3;
      m_fLGOrientVar = 0.28;
      m_fTrunkSkewVar = 0.13;
      m_fTrunkThickness = 0.0251189;
      m_fTrunkTightness = 0.13;
      m_fTrunkWobble = 0.22;
      m_fTrunkStickiness = 0.12;
      m_fFlowersPerLG = 0;
      m_fFlowerSize = 0.1;
      m_fFlowerSizeVar = 0.2;
      m_fFlowerDist = 0.1;
      m_fFlowerPeak = 2;
      m_fFlowerDuration = 0.1;
      m_dwFlowerShape = 4;
      m_cFlower = 0xff;
      break;
   case TS_EVERLEAFACACIA:   // evergreen leafy, acacia
      m_fDefaultAge = 10;
      m_afAges[0] = 1;
      m_afHeight[0] = 1.5;
      m_afAges[1] = 10;
      m_afHeight[1] = 6;
      m_afAges[2] = 20;
      m_afHeight[2] = 10;
      m_afCanopyZ[0] = 0.2;
      m_afCanopyZ[1] = 0.5;
      m_afCanopyZ[2] = 0.7;
      m_afCanopyW[0] = 0.4;
      m_afCanopyW[1] = 1;
      m_afCanopyW[2] = 1;
      m_afCanopyW[3] = 0.3;
      m_dwLGShape = 1;
      m_adwLGNum[0][0] = 0;
      m_adwLGNum[0][1] = 5;
      m_adwLGNum[0][2] = 10;
      m_adwLGNum[1][0] = 25;
      m_adwLGNum[1][1] = 25;
      m_adwLGNum[1][2] = 25;
      m_adwLGNum[2][0] = 30;
      m_adwLGNum[2][1] = 30;
      m_adwLGNum[2][2] = 30;
      m_fLGRadius = 1.5;
      m_pLGSize.p[0] = 2.5;
      m_pLGSize.p[1] = 2.5;
      m_pLGSize.p[2] = 1.5;
      m_afLGSize[0][0] = 0.1;
      m_afLGSize[0][1] = 0.1;
      m_afLGSize[0][2] = 0.1;
      m_afLGSize[0][3] = 0.1;
      m_afLGSize[1][0] = 0.6;
      m_afLGSize[1][1] = 0.6;
      m_afLGSize[1][2] = 0.6;
      m_afLGSize[1][3] = 0.6;
      m_afLGSize[2][0] = 1;
      m_afLGSize[2][1] = 1;
      m_afLGSize[2][2] = 1;
      m_afLGSize[2][3] = 1;
      m_acLGColor[0][0] = 0x8000;
      m_acTrunkColor[0][0] = 0x4040;
      m_acLGColor[0][1] = 0x408000;
      m_acTrunkColor[0][1] = 0x408080;
      m_acLGColor[1][0] = 0x8000;
      m_acTrunkColor[1][0] = 0x4040;
      m_acLGColor[1][1] = 0x408000;
      m_acTrunkColor[1][1] = 0x408080;
      m_acLGColor[2][0] = 0xffff;
      m_acTrunkColor[2][0] = 0x4040;
      m_acLGColor[2][1] = 0x408000;
      m_acTrunkColor[2][1] = 0x408080;
      m_acLGColor[3][0] = 0x8000;
      m_acTrunkColor[3][0] = 0x4040;
      m_acLGColor[3][1] = 0x408000;
      m_acTrunkColor[3][1] = 0x408080;
      m_afLGDensBySeason[0] = 0.8;
      m_afLGDensBySeason[1] = 0.8;
      m_afLGDensBySeason[2] = 0.72;
      m_afLGDensBySeason[3] = 0.65;
      m_fLGAngleUpLight = 0.39;
      m_fLGAngleUpHeavy = -0.39;
      m_fHeightVar = 0.3;
      m_fLGNumVar = 0.3;
      m_fLGSizeVar = 0.3;
      m_fLGOrientVar = 0.28;
      m_fTrunkSkewVar = 0.03;
      m_fTrunkThickness = 0.0190546;
      m_fTrunkTightness = 0.06;
      m_fTrunkWobble = 0.1;
      m_fTrunkStickiness = 0.21;
      m_fFlowersPerLG = 0;
      m_fFlowerSize = 0.1;
      m_fFlowerSizeVar = 0.2;
      m_fFlowerDist = 0.1;
      m_fFlowerPeak = 2;
      m_fFlowerDuration = 0.1;
      m_dwFlowerShape = 4;
      m_cFlower = 0xff;
      break;

   case TS_CONIFERSEQUOIA:   // conifer, sequoia
      m_fDefaultAge = 25;
      m_afAges[0] = 2;
      m_afHeight[0] = 2;
      m_afAges[1] = 25;
      m_afHeight[1] = 20;
      m_afAges[2] = 100;
      m_afHeight[2] = 60;
      m_afCanopyZ[0] = 0.4;
      m_afCanopyZ[1] = 0.6;
      m_afCanopyZ[2] = 0.9;
      m_afCanopyW[0] = 0.25;
      m_afCanopyW[1] = 0.2;
      m_afCanopyW[2] = 0.1;
      m_afCanopyW[3] = 0.02;
      m_dwLGShape = 1;
      m_adwLGNum[0][0] = 20;
      m_adwLGNum[0][1] = 20;
      m_adwLGNum[0][2] = 30;
      m_adwLGNum[1][0] = 50;
      m_adwLGNum[1][1] = 50;
      m_adwLGNum[1][2] = 50;
      m_adwLGNum[2][0] = 60;
      m_adwLGNum[2][1] = 60;
      m_adwLGNum[2][2] = 60;
      m_fLGRadius = 1.55;
      m_pLGSize.p[0] = 4;
      m_pLGSize.p[1] = 3;
      m_pLGSize.p[2] = 0.75;
      m_afLGSize[0][0] = 0.15;
      m_afLGSize[0][1] = 0.1;
      m_afLGSize[0][2] = 0.05;
      m_afLGSize[0][3] = 0.02;
      m_afLGSize[1][0] = 0.7;
      m_afLGSize[1][1] = 0.6;
      m_afLGSize[1][2] = 0.4;
      m_afLGSize[1][3] = 0.2;
      m_afLGSize[2][0] = 2;
      m_afLGSize[2][1] = 1.6;
      m_afLGSize[2][2] = 1.2;
      m_afLGSize[2][3] = 0.8;
      m_acLGColor[0][0] = 0x505000;
      m_acTrunkColor[0][0] = 0x4040;
      m_acLGColor[0][1] = 0x88b61d;
      m_acTrunkColor[0][1] = 0x4080;
      m_acLGColor[1][0] = 0x505000;
      m_acTrunkColor[1][0] = 0x4040;
      m_acLGColor[1][1] = 0x608000;
      m_acTrunkColor[1][1] = 0x4080;
      m_acLGColor[2][0] = 0x505000;
      m_acTrunkColor[2][0] = 0x4040;
      m_acLGColor[2][1] = 0x608000;
      m_acTrunkColor[2][1] = 0x4080;
      m_acLGColor[3][0] = 0x505000;
      m_acTrunkColor[3][0] = 0x4040;
      m_acLGColor[3][1] = 0x608000;
      m_acTrunkColor[3][1] = 0x4080;
      m_afLGDensBySeason[0] = 0.8;
      m_afLGDensBySeason[1] = 0.8;
      m_afLGDensBySeason[2] = 0.8;
      m_afLGDensBySeason[3] = 0.8;
      m_fLGAngleUpLight = 0.96;
      m_fLGAngleUpHeavy = -0.42;
      m_fHeightVar = 0.12;
      m_fLGNumVar = 0.18;
      m_fLGSizeVar = 0.13;
      m_fLGOrientVar = 0.02;
      m_fTrunkSkewVar = 0;
      m_fTrunkThickness = 0.0275423;
      m_fTrunkTightness = 0.55;
      m_fTrunkWobble = 0;
      m_fTrunkStickiness = 0.55;
      m_fFlowersPerLG = 0;
      m_fFlowerSize = 0.1;
      m_fFlowerSizeVar = 0.2;
      m_fFlowerDist = 0.1;
      m_fFlowerPeak = 2;
      m_fFlowerDuration = 0.1;
      m_dwFlowerShape = 4;
      m_cFlower = 0xff;
      break;
   case TS_CONIFERDOUGLASFIR:   // conifer, douglas fir
      m_fDefaultAge = 20;
      m_afAges[0] = 5;
      m_afHeight[0] = 6;
      m_afAges[1] = 20;
      m_afHeight[1] = 30;
      m_afAges[2] = 50;
      m_afHeight[2] = 60;
      m_afCanopyZ[0] = 0.4;
      m_afCanopyZ[1] = 0.6;
      m_afCanopyZ[2] = 0.8;
      m_afCanopyW[0] = 0.3 * 2;
      m_afCanopyW[1] = 0.2 * 2;
      m_afCanopyW[2] = 0.1 * 2;
      m_afCanopyW[3] = 0.01 * 2;
      m_dwLGShape = 6;
      m_adwLGNum[0][0] = 15;
      m_adwLGNum[0][1] = 15;
      m_adwLGNum[0][2] = 15;
      m_adwLGNum[1][0] = 30;
      m_adwLGNum[1][1] = 50;
      m_adwLGNum[1][2] = 50;
      m_adwLGNum[2][0] = 0;
      m_adwLGNum[2][1] = 60;
      m_adwLGNum[2][2] = 60;
      m_fLGRadius = -1;
      m_pLGSize.p[0] = 4;
      m_pLGSize.p[1] = 3;
      m_pLGSize.p[2] = 0.75;
      m_afLGSize[0][0] = 0.2;
      m_afLGSize[0][1] = 0.15;
      m_afLGSize[0][2] = 0.1;
      m_afLGSize[0][3] = 0.05;
      m_afLGSize[1][0] = 1;
      m_afLGSize[1][1] = 1;
      m_afLGSize[1][2] = 0.5;
      m_afLGSize[1][3] = 0.1;
      m_afLGSize[2][0] = 1.5;
      m_afLGSize[2][1] = 1.5;
      m_afLGSize[2][2] = 0.8;
      m_afLGSize[2][3] = 0.2;
      m_acLGColor[0][0] = 0x505000;
      m_acTrunkColor[0][0] = 0x4040;
      m_acLGColor[0][1] = 0x88b61d;
      m_acTrunkColor[0][1] = 0x408080;
      m_acLGColor[1][0] = 0x505000;
      m_acTrunkColor[1][0] = 0x4040;
      m_acLGColor[1][1] = 0x608000;
      m_acTrunkColor[1][1] = 0x408080;
      m_acLGColor[2][0] = 0x505000;
      m_acTrunkColor[2][0] = 0x4040;
      m_acLGColor[2][1] = 0x608000;
      m_acTrunkColor[2][1] = 0x408080;
      m_acLGColor[3][0] = 0x505000;
      m_acTrunkColor[3][0] = 0x4040;
      m_acLGColor[3][1] = 0x608000;
      m_acTrunkColor[3][1] = 0x408080;
      m_afLGDensBySeason[0] = 0.8;
      m_afLGDensBySeason[1] = 0.8;
      m_afLGDensBySeason[2] = 0.8;
      m_afLGDensBySeason[3] = 0.8;
      m_fLGAngleUpLight = 0.96;
      m_fLGAngleUpHeavy = -0.42;
      m_fHeightVar = 0.12;
      m_fLGNumVar = 0.12;
      m_fLGSizeVar = 0.02;
      m_fLGOrientVar = 0.04;
      m_fTrunkSkewVar = 0.01;
      m_fTrunkThickness = 0.0275423;
      m_fTrunkTightness = 0.05;
      m_fTrunkWobble = 0;
      m_fTrunkStickiness = 0.2;
      m_fFlowersPerLG = 0;
      m_fFlowerSize = 0.1;
      m_fFlowerSizeVar = 0.2;
      m_fFlowerDist = 0.1;
      m_fFlowerPeak = 2;
      m_fFlowerDuration = 0.1;
      m_dwFlowerShape = 4;
      m_cFlower = 0xff;
      break;
   case TS_CONIFERPINE:   // conifer, pine
      m_fDefaultAge = 7;
      m_afAges[0] = 2;
      m_afHeight[0] = 1;
      m_afAges[1] = 7;
      m_afHeight[1] = 5;
      m_afAges[2] = 15;
      m_afHeight[2] = 10;
      m_afCanopyZ[0] = 0.1;
      m_afCanopyZ[1] = 0.4;
      m_afCanopyZ[2] = 0.7;
      m_afCanopyW[0] = 0.25 * 2;
      m_afCanopyW[1] = 0.25 * 2;
      m_afCanopyW[2] = 0.2 * 2;
      m_afCanopyW[3] = 0.05 * 2;
      m_dwLGShape = 1;
      m_adwLGNum[0][0] = 10;
      m_adwLGNum[0][1] = 10;
      m_adwLGNum[0][2] = 10;
      m_adwLGNum[1][0] = 25;
      m_adwLGNum[1][1] = 25;
      m_adwLGNum[1][2] = 25;
      m_adwLGNum[2][0] = 60;
      m_adwLGNum[2][1] = 60;
      m_adwLGNum[2][2] = 60;
      m_fLGRadius = 1.55;
      m_pLGSize.p[0] = 1.5;
      m_pLGSize.p[1] = 1;
      m_pLGSize.p[2] = 0.55;
      m_afLGSize[0][0] = 0.2;
      m_afLGSize[0][1] = 0.15;
      m_afLGSize[0][2] = 0.1;
      m_afLGSize[0][3] = 0.05;
      m_afLGSize[1][0] = 0.6;
      m_afLGSize[1][1] = 0.5;
      m_afLGSize[1][2] = 0.4;
      m_afLGSize[1][3] = 0.3;
      m_afLGSize[2][0] = 1;
      m_afLGSize[2][1] = 0.8;
      m_afLGSize[2][2] = 0.6;
      m_afLGSize[2][3] = 0.4;
      m_acLGColor[0][0] = 0x8000;
      m_acTrunkColor[0][0] = 0x4040;
      m_acLGColor[0][1] = 0x88b61d;
      m_acTrunkColor[0][1] = 0x303030;
      m_acLGColor[1][0] = 0x8000;
      m_acTrunkColor[1][0] = 0x4040;
      m_acLGColor[1][1] = 0x608000;
      m_acTrunkColor[1][1] = 0x202020;
      m_acLGColor[2][0] = 0x8000;
      m_acTrunkColor[2][0] = 0x4040;
      m_acLGColor[2][1] = 0x608000;
      m_acTrunkColor[2][1] = 0x202020;
      m_acLGColor[3][0] = 0x8000;
      m_acTrunkColor[3][0] = 0x4040;
      m_acLGColor[3][1] = 0x608000;
      m_acTrunkColor[3][1] = 0x202020;
      m_afLGDensBySeason[0] = 0.8;
      m_afLGDensBySeason[1] = 0.8;
      m_afLGDensBySeason[2] = 0.8;
      m_afLGDensBySeason[3] = 0.8;
      m_fLGAngleUpLight = 0.96;
      m_fLGAngleUpHeavy = -0.65;
      m_fHeightVar = 0.18;
      m_fLGNumVar = 0.18;
      m_fLGSizeVar = 0.27;
      m_fLGOrientVar = 0.08;
      m_fTrunkSkewVar = 0;
      m_fTrunkThickness = 0.04;  // BUGFIX - Was .69, but too thick
      m_fTrunkTightness = 0.63;
      m_fTrunkWobble = 0;
      m_fTrunkStickiness = 0.59;
      m_fFlowersPerLG = 0.5;
      m_fFlowerSize = 0.3;
      m_fFlowerSizeVar = 0.2;
      m_fFlowerDist = 0.43;
      m_fFlowerPeak = 2;
      m_fFlowerDuration = 0.2;
      m_dwFlowerShape = 1;
      m_cFlower = 0x55;
      break;
   case TS_CONIFERSPRUCE:   // conifer, spruce
      m_fDefaultAge = 20;
      m_afAges[0] = 5;
      m_afHeight[0] = 2;
      m_afAges[1] = 20;
      m_afHeight[1] = 20;
      m_afAges[2] = 50;
      m_afHeight[2] = 40;
      m_afCanopyZ[0] = 0.2;
      m_afCanopyZ[1] = 0.4;
      m_afCanopyZ[2] = 0.7;
      m_afCanopyW[0] = 0.3 * 2;
      m_afCanopyW[1] = 0.25 * 2;
      m_afCanopyW[2] = 0.15 * 2;
      m_afCanopyW[3] = 0.05 * 2;
      m_dwLGShape = 6;
      m_adwLGNum[0][0] = 20;
      m_adwLGNum[0][1] = 20;
      m_adwLGNum[0][2] = 20;
      m_adwLGNum[1][0] = 30;
      m_adwLGNum[1][1] = 30;
      m_adwLGNum[1][2] = 30;
      m_adwLGNum[2][0] = 40;
      m_adwLGNum[2][1] = 40;
      m_adwLGNum[2][2] = 40;
      m_fLGRadius = -0.8;
      m_pLGSize.p[0] = 4;
      m_pLGSize.p[1] = 3;
      m_pLGSize.p[2] = 0.75;
      m_afLGSize[0][0] = 0.2;
      m_afLGSize[0][1] = 0.15;
      m_afLGSize[0][2] = 0.1;
      m_afLGSize[0][3] = 0.05;
      m_afLGSize[1][0] = 0.7;
      m_afLGSize[1][1] = 0.5;
      m_afLGSize[1][2] = 0.4;
      m_afLGSize[1][3] = 0.1;
      m_afLGSize[2][0] = 1;
      m_afLGSize[2][1] = 0.8;
      m_afLGSize[2][2] = 0.6;
      m_afLGSize[2][3] = 0.2;
      m_acLGColor[0][0] = 0x645000;
      m_acTrunkColor[0][0] = 0x4040;
      m_acLGColor[0][1] = 0x88b61d;
      m_acTrunkColor[0][1] = 0x408080;
      m_acLGColor[1][0] = 0x645000;
      m_acTrunkColor[1][0] = 0x4040;
      m_acLGColor[1][1] = 0x608000;
      m_acTrunkColor[1][1] = 0x408080;
      m_acLGColor[2][0] = 0x645000;
      m_acTrunkColor[2][0] = 0x4040;
      m_acLGColor[2][1] = 0x608000;
      m_acTrunkColor[2][1] = 0x408080;
      m_acLGColor[3][0] = 0x645000;
      m_acTrunkColor[3][0] = 0x4040;
      m_acLGColor[3][1] = 0x608000;
      m_acTrunkColor[3][1] = 0x408080;
      m_afLGDensBySeason[0] = 0.8;
      m_afLGDensBySeason[1] = 0.8;
      m_afLGDensBySeason[2] = 0.8;
      m_afLGDensBySeason[3] = 0.8;
      m_fLGAngleUpLight = 0.96;
      m_fLGAngleUpHeavy = -0.42;
      m_fHeightVar = 0.12;
      m_fLGNumVar = 0.23;
      m_fLGSizeVar = 0.16;
      m_fLGOrientVar = 0.04;
      m_fTrunkSkewVar = 0.01;
      m_fTrunkThickness = 0.0229087;
      m_fTrunkTightness = 0.05;
      m_fTrunkWobble = 0;
      m_fTrunkStickiness = 0.2;
      m_fFlowersPerLG = 0;
      m_fFlowerSize = 0.1;
      m_fFlowerSizeVar = 0.2;
      m_fFlowerDist = 0.1;
      m_fFlowerPeak = 2;
      m_fFlowerDuration = 0.1;
      m_dwFlowerShape = 4;
      m_cFlower = 0xff;
      break;

   case TS_CONIFERJUNIPER:   // conifer, juniper
      m_fDefaultAge = 15;
      m_afAges[0] = 1;
      m_afHeight[0] = 0.5;
      m_afAges[1] = 15;
      m_afHeight[1] = 3;
      m_afAges[2] = 25;
      m_afHeight[2] = 5;
      m_afCanopyZ[0] = 0.2;
      m_afCanopyZ[1] = 0.4;
      m_afCanopyZ[2] = 0.7;
      m_afCanopyW[0] = 0.15 * 2;
      m_afCanopyW[1] = 0.15 * 2;
      m_afCanopyW[2] = 0.1 * 2;
      m_afCanopyW[3] = 0.01 * 2;
      m_dwLGShape = 1;
      m_adwLGNum[0][0] = 20;
      m_adwLGNum[0][1] = 20;
      m_adwLGNum[0][2] = 20;
      m_adwLGNum[1][0] = 30;
      m_adwLGNum[1][1] = 30;
      m_adwLGNum[1][2] = 30;
      m_adwLGNum[2][0] = 40;
      m_adwLGNum[2][1] = 40;
      m_adwLGNum[2][2] = 40;
      m_fLGRadius = 1.55;
      m_pLGSize.p[0] = 1;
      m_pLGSize.p[1] = 0.5;
      m_pLGSize.p[2] = 0.3;
      m_afLGSize[0][0] = 0.1;
      m_afLGSize[0][1] = 0.06;
      m_afLGSize[0][2] = 0.03;
      m_afLGSize[0][3] = 0.01;
      m_afLGSize[1][0] = 0.6;
      m_afLGSize[1][1] = 0.5;
      m_afLGSize[1][2] = 0.4;
      m_afLGSize[1][3] = 0.3;
      m_afLGSize[2][0] = 1;
      m_afLGSize[2][1] = 0.9;
      m_afLGSize[2][2] = 0.8;
      m_afLGSize[2][3] = 0.7;
      m_acLGColor[0][0] = 0xb9b900;
      m_acTrunkColor[0][0] = 0x4040;
      m_acLGColor[0][1] = 0x808000;
      m_acTrunkColor[0][1] = 0x408080;
      m_acLGColor[1][0] = 0xb9b900;
      m_acTrunkColor[1][0] = 0x4040;
      m_acLGColor[1][1] = 0x506a00;
      m_acTrunkColor[1][1] = 0x408080;
      m_acLGColor[2][0] = 0xb9b900;
      m_acTrunkColor[2][0] = 0x4040;
      m_acLGColor[2][1] = 0x557100;
      m_acTrunkColor[2][1] = 0x408080;
      m_acLGColor[3][0] = 0xb9b900;
      m_acTrunkColor[3][0] = 0x4040;
      m_acLGColor[3][1] = 0x506a00;
      m_acTrunkColor[3][1] = 0x408080;
      m_afLGDensBySeason[0] = 0.8;
      m_afLGDensBySeason[1] = 0.8;
      m_afLGDensBySeason[2] = 0.8;
      m_afLGDensBySeason[3] = 0.8;
      m_fLGAngleUpLight = 1;
      m_fLGAngleUpHeavy = 0.92;
      m_fHeightVar = 0.18;
      m_fLGNumVar = 0.18;
      m_fLGSizeVar = 0.13;
      m_fLGOrientVar = 0.08;
      m_fTrunkSkewVar = 0;
      m_fTrunkThickness = 0.0275423;
      m_fTrunkTightness = 0.55;
      m_fTrunkWobble = 0;
      m_fTrunkStickiness = 0.55;
      m_fFlowersPerLG = 0;
      m_fFlowerSize = 0.1;
      m_fFlowerSizeVar = 0.2;
      m_fFlowerDist = 0.1;
      m_fFlowerPeak = 2;
      m_fFlowerDuration = 0.1;
      m_dwFlowerShape = 4;
      m_cFlower = 0xff;
      break;

   case TS_CONIFERITALIANCYPRESS:   // conifer, itialian cypress
      m_fDefaultAge = 15;
      m_afAges[0] = 1;
      m_afHeight[0] = 1;
      m_afAges[1] = 15;
      m_afHeight[1] = 7;
      m_afAges[2] = 25;
      m_afHeight[2] = 15;
      m_afCanopyZ[0] = 0.2;
      m_afCanopyZ[1] = 0.4;
      m_afCanopyZ[2] = 0.7;
      m_afCanopyW[0] = 0.05 * 2;
      m_afCanopyW[1] = 0.15 * 2;
      m_afCanopyW[2] = 0.1 * 2;
      m_afCanopyW[3] = 0.01 * 2;
      m_dwLGShape = 1;
      m_adwLGNum[0][0] = 20;
      m_adwLGNum[0][1] = 20;
      m_adwLGNum[0][2] = 20;
      m_adwLGNum[1][0] = 30;
      m_adwLGNum[1][1] = 30;
      m_adwLGNum[1][2] = 30;
      m_adwLGNum[2][0] = 40;
      m_adwLGNum[2][1] = 40;
      m_adwLGNum[2][2] = 40;
      m_fLGRadius = 1.55;
      m_pLGSize.p[0] = 2.5;
      m_pLGSize.p[1] = 1;
      m_pLGSize.p[2] = 0.75;
      m_afLGSize[0][0] = 0.1;
      m_afLGSize[0][1] = 0.06;
      m_afLGSize[0][2] = 0.03;
      m_afLGSize[0][3] = 0.01;
      m_afLGSize[1][0] = 0.6;
      m_afLGSize[1][1] = 0.5;
      m_afLGSize[1][2] = 0.4;
      m_afLGSize[1][3] = 0.3;
      m_afLGSize[2][0] = 1;
      m_afLGSize[2][1] = 0.9;
      m_afLGSize[2][2] = 0.8;
      m_afLGSize[2][3] = 0.7;
      m_acLGColor[0][0] = 0x404000;
      m_acTrunkColor[0][0] = 0x4040;
      m_acLGColor[0][1] = 0x808000;
      m_acTrunkColor[0][1] = 0x408080;
      m_acLGColor[1][0] = 0x404000;
      m_acTrunkColor[1][0] = 0x4040;
      m_acLGColor[1][1] = 0x506a00;
      m_acTrunkColor[1][1] = 0x408080;
      m_acLGColor[2][0] = 0x404000;
      m_acTrunkColor[2][0] = 0x4040;
      m_acLGColor[2][1] = 0x557100;
      m_acTrunkColor[2][1] = 0x408080;
      m_acLGColor[3][0] = 0x404000;
      m_acTrunkColor[3][0] = 0x4040;
      m_acLGColor[3][1] = 0x506a00;
      m_acTrunkColor[3][1] = 0x408080;
      m_afLGDensBySeason[0] = 0.8;
      m_afLGDensBySeason[1] = 0.8;
      m_afLGDensBySeason[2] = 0.8;
      m_afLGDensBySeason[3] = 0.8;
      m_fLGAngleUpLight = 1;
      m_fLGAngleUpHeavy = 0.92;
      m_fHeightVar = 0.18;
      m_fLGNumVar = 0.18;
      m_fLGSizeVar = 0.13;
      m_fLGOrientVar = 0.08;
      m_fTrunkSkewVar = 0;
      m_fTrunkThickness = 0.0275423;
      m_fTrunkTightness = 0.55;
      m_fTrunkWobble = 0;
      m_fTrunkStickiness = 0.55;
      m_fFlowersPerLG = 0;
      m_fFlowerSize = 0.1;
      m_fFlowerSizeVar = 0.2;
      m_fFlowerDist = 0.1;
      m_fFlowerPeak = 2;
      m_fFlowerDuration = 0.1;
      m_dwFlowerShape = 4;
      m_cFlower = 0xff;
      break;

   case TS_CONIFERLEYLANDCYPRESS:   // conifer, leyland cypress
      m_fDefaultAge = 7;
      m_afAges[0] = 1;
      m_afHeight[0] = 1;
      m_afAges[1] = 7;
      m_afHeight[1] = 7;
      m_afAges[2] = 15;
      m_afHeight[2] = 15;
      m_afCanopyZ[0] = 0.1;
      m_afCanopyZ[1] = 0.4;
      m_afCanopyZ[2] = 0.7;
      m_afCanopyW[0] = 0.25 * 2;
      m_afCanopyW[1] = 0.2 * 2;
      m_afCanopyW[2] = 0.15 * 2;
      m_afCanopyW[3] = 0.01 * 2;
      m_dwLGShape = 1;
      m_adwLGNum[0][0] = 5;
      m_adwLGNum[0][1] = 10;
      m_adwLGNum[0][2] = 10;
      m_adwLGNum[1][0] = 15;
      m_adwLGNum[1][1] = 15;
      m_adwLGNum[1][2] = 15;
      m_adwLGNum[2][0] = 40;
      m_adwLGNum[2][1] = 40;
      m_adwLGNum[2][2] = 40;
      m_fLGRadius = 1.55;
      m_pLGSize.p[0] = 4;
      m_pLGSize.p[1] = 3;
      m_pLGSize.p[2] = 0.75;
      m_afLGSize[0][0] = 0.1;
      m_afLGSize[0][1] = 0.06;
      m_afLGSize[0][2] = 0.03;
      m_afLGSize[0][3] = 0.01;
      m_afLGSize[1][0] = 0.7;
      m_afLGSize[1][1] = 0.5;
      m_afLGSize[1][2] = 0.3;
      m_afLGSize[1][3] = 0.2;
      m_afLGSize[2][0] = 1;
      m_afLGSize[2][1] = 0.8;
      m_afLGSize[2][2] = 0.6;
      m_afLGSize[2][3] = 0.4;
      m_acLGColor[0][0] = 0x8000;
      m_acTrunkColor[0][0] = 0x4040;
      m_acLGColor[0][1] = 0x88b61d;
      m_acTrunkColor[0][1] = 0x408080;
      m_acLGColor[1][0] = 0x8000;
      m_acTrunkColor[1][0] = 0x4040;
      m_acLGColor[1][1] = 0x608000;
      m_acTrunkColor[1][1] = 0x408080;
      m_acLGColor[2][0] = 0x8000;
      m_acTrunkColor[2][0] = 0x4040;
      m_acLGColor[2][1] = 0x608000;
      m_acTrunkColor[2][1] = 0x408080;
      m_acLGColor[3][0] = 0x8000;
      m_acTrunkColor[3][0] = 0x4040;
      m_acLGColor[3][1] = 0x608000;
      m_acTrunkColor[3][1] = 0x408080;
      m_afLGDensBySeason[0] = 0.8;
      m_afLGDensBySeason[1] = 0.8;
      m_afLGDensBySeason[2] = 0.8;
      m_afLGDensBySeason[3] = 0.8;
      m_fLGAngleUpLight = 0.96;
      m_fLGAngleUpHeavy = -0.65;
      m_fHeightVar = 0.18;
      m_fLGNumVar = 0.18;
      m_fLGSizeVar = 0.13;
      m_fLGOrientVar = 0.08;
      m_fTrunkSkewVar = 0;
      m_fTrunkThickness = 0.0275423;
      m_fTrunkTightness = 0.55;
      m_fTrunkWobble = 0;
      m_fTrunkStickiness = 0.55;
      m_fFlowersPerLG = 0;
      m_fFlowerSize = 0.1;
      m_fFlowerSizeVar = 0.2;
      m_fFlowerDist = 0.1;
      m_fFlowerPeak = 2;
      m_fFlowerDuration = 0.1;
      m_dwFlowerShape = 4;
      m_cFlower = 0xff;
      break;
   case TS_CONIFERCEDAR:   // conifer, cedar (cedrus)
      m_fDefaultAge = 20;
      m_afAges[0] = 5;
      m_afHeight[0] = 2;
      m_afAges[1] = 20;
      m_afHeight[1] = 12;
      m_afAges[2] = 50;
      m_afHeight[2] = 20;
      m_afCanopyZ[0] = 0.1;
      m_afCanopyZ[1] = 0.4;
      m_afCanopyZ[2] = 0.7;
      m_afCanopyW[0] = 0.45;
      m_afCanopyW[1] = 0.3;
      m_afCanopyW[2] = 0.15;
      m_afCanopyW[3] = 0.02;
      m_dwLGShape = 1;
      m_adwLGNum[0][0] = 20;
      m_adwLGNum[0][1] = 20;
      m_adwLGNum[0][2] = 30;
      m_adwLGNum[1][0] = 50;
      m_adwLGNum[1][1] = 50;
      m_adwLGNum[1][2] = 50;
      m_adwLGNum[2][0] = 60;
      m_adwLGNum[2][1] = 60;
      m_adwLGNum[2][2] = 60;
      m_fLGRadius = 1.55;
      m_pLGSize.p[0] = 4;
      m_pLGSize.p[1] = 3;
      m_pLGSize.p[2] = 0.75;
      m_afLGSize[0][0] = 0.15;
      m_afLGSize[0][1] = 0.15;
      m_afLGSize[0][2] = 0.15;
      m_afLGSize[0][3] = 0.1;
      m_afLGSize[1][0] = 0.7;
      m_afLGSize[1][1] = 0.6;
      m_afLGSize[1][2] = 0.4;
      m_afLGSize[1][3] = 0.2;
      m_afLGSize[2][0] = 1;
      m_afLGSize[2][1] = 0.8;
      m_afLGSize[2][2] = 0.6;
      m_afLGSize[2][3] = 0.4;
      m_acLGColor[0][0] = 0x505000;
      m_acTrunkColor[0][0] = 0x4040;
      m_acLGColor[0][1] = 0x88b61d;
      m_acTrunkColor[0][1] = 0x408080;
      m_acLGColor[1][0] = 0x505000;
      m_acTrunkColor[1][0] = 0x4040;
      m_acLGColor[1][1] = 0x608000;
      m_acTrunkColor[1][1] = 0x408080;
      m_acLGColor[2][0] = 0x505000;
      m_acTrunkColor[2][0] = 0x4040;
      m_acLGColor[2][1] = 0x608000;
      m_acTrunkColor[2][1] = 0x408080;
      m_acLGColor[3][0] = 0x505000;
      m_acTrunkColor[3][0] = 0x4040;
      m_acLGColor[3][1] = 0x608000;
      m_acTrunkColor[3][1] = 0x408080;
      m_afLGDensBySeason[0] = 0.8;
      m_afLGDensBySeason[1] = 0.8;
      m_afLGDensBySeason[2] = 0.8;
      m_afLGDensBySeason[3] = 0.8;
      m_fLGAngleUpLight = 0.96;
      m_fLGAngleUpHeavy = -0.42;
      m_fHeightVar = 0.12;
      m_fLGNumVar = 0.18;
      m_fLGSizeVar = 0.13;
      m_fLGOrientVar = 0.02;
      m_fTrunkSkewVar = 0;
      m_fTrunkThickness = 0.0275423;
      m_fTrunkTightness = 0.55;
      m_fTrunkWobble = 0;
      m_fTrunkStickiness = 0.55;
      m_fFlowersPerLG = 0;
      m_fFlowerSize = 0.1;
      m_fFlowerSizeVar = 0.2;
      m_fFlowerDist = 0.1;
      m_fFlowerPeak = 2;
      m_fFlowerDuration = 0.1;
      m_dwFlowerShape = 4;
      m_cFlower = 0xff;
      break;
   case TS_CONIFERFIR:   // conifer, fir (abies)
      m_fDefaultAge = 20;
      m_afAges[0] = 5;
      m_afHeight[0] = 2;
      m_afAges[1] = 20;
      m_afHeight[1] = 15;
      m_afAges[2] = 50;
      m_afHeight[2] = 25;
      m_afCanopyZ[0] = 0.1;
      m_afCanopyZ[1] = 0.4;
      m_afCanopyZ[2] = 0.7;
      m_afCanopyW[0] = 0.3 * 2;
      m_afCanopyW[1] = 0.2 * 2;
      m_afCanopyW[2] = 0.1 * 2;
      m_afCanopyW[3] = 0.01 * 2;
      m_dwLGShape = 6;
      m_adwLGNum[0][0] = 20;
      m_adwLGNum[0][1] = 20;
      m_adwLGNum[0][2] = 20;
      m_adwLGNum[1][0] = 50;
      m_adwLGNum[1][1] = 50;
      m_adwLGNum[1][2] = 50;
      m_adwLGNum[2][0] = 60;
      m_adwLGNum[2][1] = 60;
      m_adwLGNum[2][2] = 60;
      m_fLGRadius = -1;
      m_pLGSize.p[0] = 4;
      m_pLGSize.p[1] = 3;
      m_pLGSize.p[2] = 0.75;
      m_afLGSize[0][0] = 0.2;
      m_afLGSize[0][1] = 0.15;
      m_afLGSize[0][2] = 0.1;
      m_afLGSize[0][3] = 0.05;
      m_afLGSize[1][0] = 0.7;
      m_afLGSize[1][1] = 0.5;
      m_afLGSize[1][2] = 0.3;
      m_afLGSize[1][3] = 0.05;
      m_afLGSize[2][0] = 1;
      m_afLGSize[2][1] = 0.66;
      m_afLGSize[2][2] = 0.33;
      m_afLGSize[2][3] = 0.05;
      m_acLGColor[0][0] = 0x505000;
      m_acTrunkColor[0][0] = 0x4040;
      m_acLGColor[0][1] = 0x88b61d;
      m_acTrunkColor[0][1] = 0x408080;
      m_acLGColor[1][0] = 0x505000;
      m_acTrunkColor[1][0] = 0x4040;
      m_acLGColor[1][1] = 0x608000;
      m_acTrunkColor[1][1] = 0x408080;
      m_acLGColor[2][0] = 0x505000;
      m_acTrunkColor[2][0] = 0x4040;
      m_acLGColor[2][1] = 0x608000;
      m_acTrunkColor[2][1] = 0x408080;
      m_acLGColor[3][0] = 0x505000;
      m_acTrunkColor[3][0] = 0x4040;
      m_acLGColor[3][1] = 0x608000;
      m_acTrunkColor[3][1] = 0x408080;
      m_afLGDensBySeason[0] = 0.8;
      m_afLGDensBySeason[1] = 0.8;
      m_afLGDensBySeason[2] = 0.8;
      m_afLGDensBySeason[3] = 0.8;
      m_fLGAngleUpLight = 0.96;
      m_fLGAngleUpHeavy = -0.42;
      m_fHeightVar = 0.12;
      m_fLGNumVar = 0.12;
      m_fLGSizeVar = 0.02;
      m_fLGOrientVar = 0.04;
      m_fTrunkSkewVar = 0.01;
      m_fTrunkThickness = 0.0275423;
      m_fTrunkTightness = 0.05;
      m_fTrunkWobble = 0;
      m_fTrunkStickiness = 0.2;
      m_fFlowersPerLG = 0;
      m_fFlowerSize = 0.1;
      m_fFlowerSizeVar = 0.2;
      m_fFlowerDist = 0.1;
      m_fFlowerPeak = 2;
      m_fFlowerDuration = 0.1;
      m_dwFlowerShape = 4;
      m_cFlower = 0xff;
      break;
   }
}


/**********************************************************************************
CObjectTree::DialogQuery - Standard function to ask if object supports dialog.
*/
BOOL CObjectTree::DialogQuery (void)
{
   return TRUE;
}


typedef struct {
   PCObjectTree      pThis;   // this object
   int               iVScroll;   // scrolling
   __int64           iToday;  // today
} TDP, *PTDP;


/* TreeDialogPage
*/
BOOL TreeDialogPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PTDP ptdp = (PTDP) pPage->m_pUserData;
   PCObjectTree pv = ptdp->pThis;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         // Handle scroll on redosamepage
         if (ptdp->iVScroll >= 0) {
            pPage->VScroll (ptdp->iVScroll);

            // when bring up pop-up dialog often they're scrolled wrong because
            // iVScoll was left as valeu, and they use defpage
            ptdp->iVScroll = 0;

            // BUGFIX - putting this invalidate in to hopefully fix a refresh
            // problem when add or move a task in the ProjectView page
            pPage->Invalidate();
         }

         // set the list box
         ListBoxSet (pPage, L"treetype", pv->m_dwStyle);

         // plant-specific information

         // age
         __int64 iWas;
         iWas = ptdp->iToday - DFDATEToMinutes (pv->m_dwBirthday);
         iWas /= (60 * 24);
         DoubleToControl (pPage, L"age", (fp)iWas / 365.0);

         // show on date
         DWORD dwDate;
         PWSTR psz;
         psz = pv->m_pWorld->VariableGet (gpszWSDate);
         if (psz)
            dwDate = _wtoi(psz);
         else
            DefaultBuildingSettings (&dwDate);  // BUGFIX - Get default building settings
         DateControlSet (pPage, L"sundate", dwDate);


         // pot
         PCEscControl pControl;
         pControl = pPage->ControlFind (L"usepot");
         if (pControl)
            pControl->AttribSetBOOL (gszChecked, pv->m_dwPotShape != 0);
         ComboBoxSet (pPage, L"potshape", max(1,pv->m_dwPotShape));
         MeasureToString (pPage, L"potsize0", pv->m_pPotSize.p[0]);
         MeasureToString (pPage, L"potsize1", pv->m_pPotSize.p[1]);
         MeasureToString (pPage, L"potsize2", pv->m_pPotSize.p[2]);


         // display species settings
         DWORD i, j;
         WCHAR szTemp[32];
         for (i = 0; i < 3; i++) {
            swprintf (szTemp, L"ages%d", (int)i);
            DoubleToControl (pPage, szTemp, pv->m_afAges[i]);

            swprintf (szTemp, L"height%d", (int)i);
            MeasureToString (pPage, szTemp, pv->m_afHeight[i]);

            swprintf (szTemp, L"canopyz%d", (int)i);
            DoubleToControl (pPage, szTemp, pv->m_afCanopyZ[i] * 100.0);

            swprintf (szTemp, L"plgsize%d", (int)i);
            MeasureToString (pPage, szTemp, pv->m_pLGSize.p[i]);
         }

         for (i = 0; i < 4; i++) {
            swprintf (szTemp, L"canopyw%d", (int)i);
            DoubleToControl (pPage, szTemp, pv->m_afCanopyW[i] * 100.0);
         }

         for (i = 0; i < 3; i++) for (j = 0; j < 4; j++) {
            swprintf (szTemp, L"lgsize%d%d", (int)i, (int)j);
            DoubleToControl (pPage, szTemp, pv->m_afLGSize[i][j] * 100.0);
         }
         for (i = 0; i < 3; i++) for (j = 0; j < 3; j++) {
            swprintf (szTemp, L"lgnum%d%d", (int)i, (int)j);
            DoubleToControl (pPage, szTemp, (fp) pv->m_adwLGNum[i][j]);
         }
         DoubleToControl (pPage, L"flowersperlg", pv->m_fFlowersPerLG);
         MeasureToString (pPage, L"flowersize", pv->m_fFlowerSize);


         pControl = pPage->ControlFind (L"heightvar");
         if (pControl)
            pControl->AttribSetInt (gszPos, (int) (pv->m_fHeightVar * 100));
         pControl = pPage->ControlFind (L"lgradius");
         if (pControl)
            pControl->AttribSetInt (gszPos, (int) (pv->m_fLGRadius * 100));
         pControl = pPage->ControlFind (L"lgangleuplight");
         if (pControl)
            pControl->AttribSetInt (gszPos, -(int) (pv->m_fLGAngleUpLight * 100));
         pControl = pPage->ControlFind (L"lgangleupheavy");
         if (pControl)
            pControl->AttribSetInt (gszPos, -(int) (pv->m_fLGAngleUpHeavy * 100));
         pControl = pPage->ControlFind (L"lgsizevar");
         if (pControl)
            pControl->AttribSetInt (gszPos, (int) (pv->m_fLGSizeVar * 100));
         pControl = pPage->ControlFind (L"lgnumvar");
         if (pControl)
            pControl->AttribSetInt (gszPos, (int) (pv->m_fLGNumVar * 100));
         pControl = pPage->ControlFind (L"lgorientvar");
         if (pControl)
            pControl->AttribSetInt (gszPos, (int) (pv->m_fLGOrientVar * 100));
         pControl = pPage->ControlFind (L"trunktightness");
         if (pControl)
            pControl->AttribSetInt (gszPos, (int) (pv->m_fTrunkTightness * 100));
         pControl = pPage->ControlFind (L"trunkstickiness");
         if (pControl)
            pControl->AttribSetInt (gszPos, (int) (pv->m_fTrunkStickiness * 100));
         pControl = pPage->ControlFind (L"trunkwobble");
         if (pControl)
            pControl->AttribSetInt (gszPos, (int) (pv->m_fTrunkWobble * 100));
         pControl = pPage->ControlFind (L"trunkskewvar");
         if (pControl)
            pControl->AttribSetInt (gszPos, (int) (pv->m_fTrunkSkewVar * 100));
         pControl = pPage->ControlFind (L"flowersizevar");
         if (pControl)
            pControl->AttribSetInt (gszPos, (int) (pv->m_fFlowerSizeVar * 100));
         pControl = pPage->ControlFind (L"flowerdist");
         if (pControl)
            pControl->AttribSetInt (gszPos, (int) (pv->m_fFlowerDist * 100));
         pControl = pPage->ControlFind (L"flowerduration");
         if (pControl)
            pControl->AttribSetInt (gszPos, (int) (pv->m_fFlowerDuration * 100));

         fp fThick;
         fThick = log10 (pv->m_fTrunkThickness ? pv->m_fTrunkThickness : .001);
         fThick = (fThick * 25) + 75;
         fThick = max(0, fThick);
         fThick = min(100, fThick);
         pControl = pPage->ControlFind (L"trunkthickness");
         if (pControl)
            pControl->AttribSetInt (gszPos, (int) fThick);

         for (i = 0; i < 4; i++) {
            swprintf (szTemp, L"lgdensbyseason%d", (int)i);
            pControl = pPage->ControlFind (szTemp);
            if (pControl)
               pControl->AttribSetInt (gszPos, (int) (pv->m_afLGDensBySeason[i] * 100));
         }

         ComboBoxSet (pPage, L"lgshape", pv->m_dwLGShape);
         ComboBoxSet (pPage, L"flowershape", pv->m_dwFlowerShape);
         ComboBoxSet (pPage, L"flowerpeak", (DWORD)(pv->m_fFlowerPeak * 3.0));

         WCHAR szTemp2[32];
         for (i = 0; i < 4; i++) for (j = 0; j < 2; j++) {
            swprintf (szTemp, L"lgcolor%d%d", (int)i, (int)j);
            ColorToAttrib (szTemp2, pv->m_acLGColor[i][j]);
            pControl = pPage->ControlFind (szTemp);
            if (pControl)
               pControl->AttribSet (gszColor, szTemp2);

            swprintf (szTemp, L"trunkcolor%d%d", (int)i, (int)j);
            ColorToAttrib (szTemp2, pv->m_acTrunkColor[i][j]);
            pControl = pPage->ControlFind (szTemp);
            if (pControl)
               pControl->AttribSet (gszColor, szTemp2);
         }
         ColorToAttrib (szTemp2, pv->m_cFlower);
         pControl = pPage->ControlFind (L"cflower");
         if (pControl)
            pControl->AttribSet (gszColor, szTemp2);
      }
      break;

   case ESCN_SCROLL:
   //case ESCN_SCROLLING:
      {
         PESCNSCROLL p = (PESCNSCROLL) pParam;

         if (!p->pControl->m_pszName || pv->m_dwStyle)
            break;

         // must be one of our scroll bars, so just get all balues
         DWORD i;
         WCHAR szTemp[32];
         PCEscControl pControl;

         pv->m_pWorld->ObjectAboutToChange (pv);

         pControl = pPage->ControlFind (L"heightvar");
         if (pControl)
            pv->m_fHeightVar = (fp) pControl->AttribGetInt (gszPos) / 100.0;
         pControl = pPage->ControlFind (L"lgradius");
         if (pControl)
            pv->m_fLGRadius = (fp) pControl->AttribGetInt (gszPos) / 100.0;
         pControl = pPage->ControlFind (L"lgangleuplight");
         if (pControl)
            pv->m_fLGAngleUpLight = -(fp) pControl->AttribGetInt (gszPos) / 100.0;
         pControl = pPage->ControlFind (L"lgangleupheavy");
         if (pControl)
            pv->m_fLGAngleUpHeavy = -(fp) pControl->AttribGetInt (gszPos) / 100.0;
         pControl = pPage->ControlFind (L"lgsizevar");
         if (pControl)
            pv->m_fLGSizeVar = (fp) pControl->AttribGetInt (gszPos) / 100.0;
         pControl = pPage->ControlFind (L"lgnumvar");
         if (pControl)
            pv->m_fLGNumVar = (fp) pControl->AttribGetInt (gszPos) / 100.0;
         pControl = pPage->ControlFind (L"lgorientvar");
         if (pControl)
            pv->m_fLGOrientVar = (fp) pControl->AttribGetInt (gszPos) / 100.0;
         pControl = pPage->ControlFind (L"trunktightness");
         if (pControl)
            pv->m_fTrunkTightness = (fp) pControl->AttribGetInt (gszPos) / 100.0;
         pControl = pPage->ControlFind (L"trunkstickiness");
         if (pControl)
            pv->m_fTrunkStickiness = (fp) pControl->AttribGetInt (gszPos) / 100.0;
         pControl = pPage->ControlFind (L"trunkwobble");
         if (pControl)
            pv->m_fTrunkWobble = (fp) pControl->AttribGetInt (gszPos) / 100.0;
         pControl = pPage->ControlFind (L"trunkskewvar");
         if (pControl)
            pv->m_fTrunkSkewVar = (fp) pControl->AttribGetInt (gszPos) / 100.0;
         pControl = pPage->ControlFind (L"flowersizevar");
         if (pControl)
            pv->m_fFlowerSizeVar = (fp) pControl->AttribGetInt (gszPos) / 100.0;
         pControl = pPage->ControlFind (L"flowerdist");
         if (pControl)
            pv->m_fFlowerDist = (fp) pControl->AttribGetInt (gszPos) / 100.0;
         pControl = pPage->ControlFind (L"flowerduration");
         if (pControl)
            pv->m_fFlowerDuration = (fp) pControl->AttribGetInt (gszPos) / 100.0;

         fp fThick;
         pControl = pPage->ControlFind (L"trunkthickness");
         if (pControl)
            fThick = pControl->AttribGetInt (gszPos);
         else
            fThick = 50;
         fThick = (fThick - 75) / 25.0;
         fThick = pow (10.0, fThick);
         pv->m_fTrunkThickness = fThick;

         for (i = 0; i < 4; i++) {
            swprintf (szTemp, L"lgdensbyseason%d", (int)i);
            pControl = pPage->ControlFind (szTemp);
            if (pControl)
               pv->m_afLGDensBySeason[i] = (fp) pControl->AttribGetInt (gszPos) / 100.0;
         }

         pv->m_fDirty = TRUE;
         pv->m_pWorld->ObjectChanged (pv);
      }
      break;

   case ESCM_LINK:
      {
         PESCMLINK p = (PESCMLINK) pParam;

         if (!p->psz)
            break;   // default

         COLORREF *pc;
         pc = NULL;

         // deal with color being changed
         PWSTR pszLG = L"lgcolor";
         DWORD dwLenLG = wcslen(pszLG);
         PWSTR pszTrunk = L"trunkcolor";
         DWORD dwLenTrunk = wcslen(pszTrunk);
         if (!wcsicmp(p->psz, L"cflower"))
            pc = &pv->m_cFlower;
         else if (!wcsncmp (p->psz, pszLG, dwLenLG))
            pc = &pv->m_acLGColor[(p->psz)[dwLenLG] - L'0'][(p->psz)[dwLenLG+1] - L'0'];
         else if (!wcsncmp (p->psz, pszTrunk, dwLenTrunk))
            pc = &pv->m_acTrunkColor[(p->psz)[dwLenTrunk] - L'0'][(p->psz)[dwLenTrunk+1] - L'0'];
         if (!pc)
            break;   // not one that captuing

         COLORREF cNew;
         cNew = AskColor (pPage->m_pWindow->m_hWnd, *pc, NULL, NULL);
         if (cNew == *pc)
            return TRUE;   // no change


         pv->m_pWorld->ObjectAboutToChange (pv);
         *pc = cNew;
         pv->m_fDirty = TRUE;
         pv->m_pWorld->ObjectChanged(pv);

         // set it
         PCEscControl pControl;
         WCHAR szTemp2[32];
         ColorToAttrib (szTemp2, *pc);
         pControl = pPage->ControlFind (p->psz);
         if (pControl) {
            pControl->AttribSet (gszColor, szTemp2);
            pControl->Invalidate(); // to refresh
         }

         return TRUE;
      }
      break;

   case ESCN_LISTBOXSELCHANGE:
      {
         PESCNLISTBOXSELCHANGE p = (PESCNLISTBOXSELCHANGE) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;
         PWSTR psz;
         psz = p->pControl->m_pszName;

         if (!wcsicmp(psz, L"treetype")) {
            DWORD dwOld = pv->m_dwStyle;
            DWORD dwNew = p->pszName ? _wtoi(p->pszName) : 0;
            if (dwOld == dwNew)
               break;   // no change

            // else change
            // else modify this
            if (pv->m_pWorld && ptdp->pThis)
               pv->m_pWorld->ObjectAboutToChange (ptdp->pThis);

            pv->m_dwStyle = dwNew;
            pv->m_fDirty = TRUE;
            pv->ParamFromStyle ();

            if (pv->m_pWorld && ptdp->pThis)
               pv->m_pWorld->ObjectChanged (ptdp->pThis);

            // may want to refresh
            if (!dwOld || !dwNew)
               pPage->Exit (gszRedoSamePage);

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
         if (!psz)
            break;

         if (!wcsicmp(psz, L"potshape")) {
            DWORD dwVal = p->pszName ? _wtoi(p->pszName) : 0;
            if (dwVal == pv->m_dwPotShape)
               break;   // no change

            // if use pot button not checked then ignore
            PCEscControl pControl;
            pControl = pPage->ControlFind (L"usepot");
            if (!pControl || !pControl->AttribGetBOOL(gszChecked))
               break;   // not checked

            pv->m_pWorld->ObjectAboutToChange (pv);
            pv->m_dwPotShape = dwVal;
            pv->m_fDirty = TRUE;
            pv->m_pWorld->ObjectChanged(pv);
         }
         else if (!wcsicmp(psz, L"lgshape")) {
            DWORD dwVal = p->pszName ? _wtoi(p->pszName) : 0;
            if (dwVal == pv->m_dwLGShape)
               break;   // no change

            pv->m_pWorld->ObjectAboutToChange (pv);
            pv->m_dwLGShape = dwVal;
            pv->m_fDirty = TRUE;
            pv->m_pWorld->ObjectChanged(pv);
         }
         else if (!wcsicmp(psz, L"flowershape")) {
            DWORD dwVal = p->pszName ? _wtoi(p->pszName) : 0;
            if (dwVal == pv->m_dwFlowerShape)
               break;   // no change

            pv->m_pWorld->ObjectAboutToChange (pv);
            pv->m_dwFlowerShape = dwVal;
            pv->m_fDirty = TRUE;
            pv->m_pWorld->ObjectChanged(pv);
         }
         else if (!wcsicmp(psz, L"flowerpeak")) {
            DWORD dwVal = p->pszName ? _wtoi(p->pszName) : 0;
            if (dwVal == (DWORD)(pv->m_fFlowerPeak * 3.0))
               break;   // no change

            pv->m_pWorld->ObjectAboutToChange (pv);
            pv->m_fFlowerPeak = (fp)dwVal / 3.0;
            pv->m_fDirty = TRUE;
            pv->m_pWorld->ObjectChanged(pv);
         }
      }
      break;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;

         if (!p->pControl->m_pszName)
            break;
         PWSTR psz;
         psz = p->pControl->m_pszName;

         if (!wcsicmp(psz, L"usepot")) {
            pv->m_pWorld->ObjectAboutToChange (pv);
            if (p->pControl->AttribGetBOOL (gszChecked)) {
               PCEscControl pControl = pPage->ControlFind (L"potshape");
               if (pControl) {
                  ESCMCOMBOBOXGETITEM gi;
                  memset (&gi, 0, sizeof(gi));
                  gi.dwIndex = (DWORD) pControl->AttribGetInt (gszCurSel);
                  pControl->Message (ESCM_COMBOBOXGETITEM, &gi);
                  DWORD j;
                  j = 1;
                  if (gi.pszName)
                     j = _wtoi (gi.pszName);
                  pv->m_dwPotShape = j;
               }
            }
            else
               pv->m_dwPotShape = 0;
            pv->m_fDirty = TRUE;
            pv->m_pWorld->ObjectChanged(pv);
            return TRUE;
         }
         else if (!wcsicmp(psz, L"srandextra")) {
            pv->m_pWorld->ObjectAboutToChange (pv);
            pv->m_dwSRandExtra += GetTickCount() + 1; // a bit of random
            pv->m_fDirty = TRUE;
            pv->m_pWorld->ObjectChanged(pv);
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

         PWSTR pszPotSize = L"potsize";
         DWORD dwPotSizeLen = wcslen(pszPotSize);
         if (!wcsncmp(psz, pszPotSize, dwPotSizeLen)) {
            DWORD dwDim = _wtoi(psz + dwPotSizeLen);
            dwDim = min(2, dwDim);
            pv->m_pWorld->ObjectAboutToChange (pv);
            MeasureParseString (pPage, psz, &pv->m_pPotSize.p[dwDim]);
            pv->m_pPotSize.p[dwDim] = max(.01, pv->m_pPotSize.p[dwDim]);
            pv->m_fDirty = TRUE;
            pv->m_pWorld->ObjectChanged(pv);
            return TRUE;
         }
         else if (!wcsicmp(psz, L"age")) {
            pv->m_pWorld->ObjectAboutToChange (pv);
            fp f;
            f = DoubleFromControl (pPage, psz);
            f *= 365;
            pv->m_dwBirthday = MinutesToDFDATE (ptdp->iToday - (__int64)f * 24 * 60);
            pv->m_fDirty = TRUE;
            pv->m_pWorld->ObjectChanged(pv);
            return TRUE;
         }

         // ***** otherwise, it's one of the custom ones, so get them
         if (pv->m_dwStyle)
            break;   // not really
         pv->m_pWorld->ObjectAboutToChange(pv);
         DWORD i, j;
         WCHAR szTemp[32];
         for (i = 0; i < 3; i++) {
            swprintf (szTemp, L"ages%d", (int)i);
            pv->m_afAges[i] = DoubleFromControl (pPage, szTemp);
            pv->m_afAges[i] = max(.01, pv->m_afAges[i]);
            if (i)
               pv->m_afAges[i] = max(pv->m_afAges[i-1] + .01, pv->m_afAges[i]);

            swprintf (szTemp, L"height%d", (int)i);
            MeasureParseString (pPage, szTemp, &pv->m_afHeight[i]);
            pv->m_afHeight[i] = max (.01, pv->m_afHeight[i]);

            swprintf (szTemp, L"canopyz%d", (int)i);
            pv->m_afCanopyZ[i] = DoubleFromControl (pPage, szTemp) / 100.0;
            pv->m_afCanopyZ[i] = max(.01, pv->m_afCanopyZ[i]);
            pv->m_afCanopyZ[i] = min(1, pv->m_afCanopyZ[i]);
            if (i)
               pv->m_afCanopyZ[i] = max(pv->m_afCanopyZ[i-1] + .01, pv->m_afCanopyZ[i]);

            swprintf (szTemp, L"plgsize%d", (int)i);
            MeasureParseString (pPage, szTemp, &pv->m_pLGSize.p[i]);
            pv->m_pLGSize.p[i] = max(.01, pv->m_pLGSize.p[i]);
         }

         for (i = 0; i < 4; i++) {
            swprintf (szTemp, L"canopyw%d", (int)i);
            pv->m_afCanopyW[i] = DoubleFromControl (pPage, szTemp) / 100.0;
            pv->m_afCanopyW[i] = max(.01, pv->m_afCanopyW[i]);
         }

         for (i = 0; i < 3; i++) for (j = 0; j < 4; j++) {
            swprintf (szTemp, L"lgsize%d%d", (int)i, (int)j);
            pv->m_afLGSize[i][j] = DoubleFromControl (pPage, szTemp) / 100.0;
            pv->m_afLGSize[i][j] = max(.01, pv->m_afLGSize[i][j]);
         }
         for (i = 0; i < 3; i++) for (j = 0; j < 3; j++) {
            swprintf (szTemp, L"lgnum%d%d", (int)i, (int)j);
            pv->m_adwLGNum[i][j] = (DWORD) DoubleFromControl (pPage, szTemp);
         }
         pv->m_fFlowersPerLG = DoubleFromControl (pPage, L"flowersperlg");
         pv->m_fFlowersPerLG = max(0, pv->m_fFlowersPerLG);
         MeasureParseString (pPage, L"flowersize", &pv->m_fFlowerSize);
         pv->m_fFlowerSize = max(0.001, pv->m_fFlowerSize);


         pv->m_fDirty = TRUE;
         pv->m_pWorld->ObjectChanged(pv);
      }
      break;

   case ESCN_DATECHANGE:
      {
         PESCNDATECHANGE p = (PESCNDATECHANGE) pParam;
         if (!p->pControl || !p->pControl->m_pszName || wcsicmp(p->pControl->m_pszName, L"sundate"))
            break;
         
         DFDATE dw;
         DWORD dwDate;
         PWSTR psz;
         dw = DateControlGet (pPage, p->pControl->m_pszName);
         if (!dw)
            dw = gdwToday;
         psz = pv->m_pWorld->VariableGet (gpszWSDate);
         if (psz)
            dwDate = _wtoi(psz);
         else
            DefaultBuildingSettings (&dwDate);  // BUGFIX - Get default building settings
         if (dw != dwDate) {
            WCHAR szTemp[32];
            swprintf (szTemp, L"%d", (int) dw);
            pv->m_pWorld->VariableSet (gpszWSDate, szTemp);
            pv->m_pWorld->NotifySockets (WORLDC_LIGHTCHANGED, NULL);
         }

         // tree change
         pPage->Message (ESCM_USER + 108);
      }
      break;   // default

   case ESCM_USER + 108:   // send OSM_NEWLATITUDE to every object
      {
         DWORD i;
         for (i = 0; i < pv->m_pWorld->ObjectNum(); i++) {
            PCObjectSocket pos = pv->m_pWorld->ObjectGet(i);
            if (pos)
               pos->Message (OSM_NEWLATITUDE, NULL);
         }
      }
      return TRUE;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Plant settings";
            return TRUE;
         }
         else if (!wcsicmp(p->pszSubName, L"IFHELP")) {
            p->pszSubString = pv->m_dwStyle ? L"<comment>" : L"";
            return TRUE;
         }
         else if (!wcsicmp(p->pszSubName, L"ENDIFHELP")) {
            p->pszSubString = pv->m_dwStyle ? L"</comment>" : L"";
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}



/**********************************************************************************
CObjectTree::DialogShow - Standard function to ask for dialog to pop up.
*/
BOOL CObjectTree::DialogShow (DWORD dwSurface, PCEscWindow pWindow)
{
   TDP tdp;
   PWSTR psz;
   memset (&tdp, 0, sizeof(tdp));
   tdp.pThis = this;
   tdp.iToday = DFDATEToMinutes (gdwToday);

firstpage:
   psz = pWindow->PageDialog (IDR_MMLTREEDIALOG, TreeDialogPage, &tdp);
   if (!psz)
      return FALSE;
   if (!wcsicmp(psz, gszRedoSamePage)) {
      tdp.iVScroll = pWindow->m_iExitVScroll;
      goto firstpage;
   }

#ifdef _DEBUG
   // copy all this to the clipboard as C++ code
   char  szTemp[10000];
   szTemp[0] = 0;

   sprintf (szTemp + strlen(szTemp), "m_fDefaultAge = %g;\n",
      (double) ((DFDATEToMinutes (gdwToday) - DFDATEToMinutes(m_dwBirthday)) / 60 / 24) / 365.0);

   DWORD i, j;
   for (i = 0; i < TREEAGES; i++) {
      sprintf (szTemp + strlen(szTemp), "m_afAges[%d] = %g;\n", i, (double)m_afAges[i]);
      sprintf (szTemp + strlen(szTemp), "m_afHeight[%d] = %g;\n", i, (double)m_afHeight[i]);
   }
   for (i = 0; i < TREECAN; i++) {
      sprintf (szTemp + strlen(szTemp), "m_afCanopyZ[%d] = %g;\n", i, (double)m_afCanopyZ[i]);
   }
   for (i = 0; i < TREECAN+1; i++) {
      sprintf (szTemp + strlen(szTemp), "m_afCanopyW[%d] = %g;\n", i, (double)m_afCanopyW[i]);
   }
   sprintf (szTemp + strlen(szTemp), "m_dwLGShape = %d;\n", m_dwLGShape);
   for (i = 0; i < TREEAGES; i++) for (j = 0; j < TREECAN; j++) {
      sprintf (szTemp + strlen(szTemp), "m_adwLGNum[%d][%d] = %d;\n", i, j, m_adwLGNum[i][j]);
   }
   sprintf (szTemp + strlen(szTemp), "m_fLGRadius = %g;\n", (double)m_fLGRadius);

   for (i = 0; i < 3; i++)
      sprintf (szTemp + strlen(szTemp), "m_pLGSize.p[%d] = %g;\n", i, (double)m_pLGSize.p[i]);
   for (i = 0; i < TREEAGES; i++) for (j = 0; j < TREECAN+1; j++) {
      sprintf (szTemp + strlen(szTemp), "m_afLGSize[%d][%d] = %g;\n", i, j, (double)m_afLGSize[i][j]);
   }
   for (i = 0; i < TREESEAS; i++) for (j = 0; j < 2; j++) {
      sprintf (szTemp + strlen(szTemp), "m_acLGColor[%d][%d] = 0x%x;\n", i, j, m_acLGColor[i][j]);
      sprintf (szTemp + strlen(szTemp), "m_acTrunkColor[%d][%d] = 0x%x;\n", i, j, m_acTrunkColor[i][j]);
   }
   for (i = 0; i < TREESEAS; i++)
      sprintf (szTemp + strlen(szTemp), "m_afLGDensBySeason[%d] = %g;\n", i, (double)m_afLGDensBySeason[i]);
   sprintf (szTemp + strlen(szTemp), "m_fLGAngleUpLight = %g;\n", (double)m_fLGAngleUpLight);
   sprintf (szTemp + strlen(szTemp), "m_fLGAngleUpHeavy = %g;\n", (double)m_fLGAngleUpHeavy);
   sprintf (szTemp + strlen(szTemp), "m_fHeightVar = %g;\n", (double)m_fHeightVar);
   sprintf (szTemp + strlen(szTemp), "m_fLGNumVar = %g;\n", (double)m_fLGNumVar);
   sprintf (szTemp + strlen(szTemp), "m_fLGSizeVar = %g;\n", (double)m_fLGSizeVar);
   sprintf (szTemp + strlen(szTemp), "m_fLGOrientVar = %g;\n", (double)m_fLGOrientVar);
   sprintf (szTemp + strlen(szTemp), "m_fTrunkSkewVar = %g;\n", (double)m_fTrunkSkewVar);
   sprintf (szTemp + strlen(szTemp), "m_fTrunkThickness = %g;\n", (double)m_fTrunkThickness);
   sprintf (szTemp + strlen(szTemp), "m_fTrunkTightness = %g;\n", (double)m_fTrunkTightness);
   sprintf (szTemp + strlen(szTemp), "m_fTrunkWobble = %g;\n", (double)m_fTrunkWobble);
   sprintf (szTemp + strlen(szTemp), "m_fTrunkStickiness = %g;\n", (double)m_fTrunkStickiness);
   sprintf (szTemp + strlen(szTemp), "m_fFlowersPerLG = %g;\n", (double)m_fFlowersPerLG);
   sprintf (szTemp + strlen(szTemp), "m_fFlowerSize = %g;\n", (double)m_fFlowerSize);
   sprintf (szTemp + strlen(szTemp), "m_fFlowerSizeVar = %g;\n", (double)m_fFlowerSizeVar);
   sprintf (szTemp + strlen(szTemp), "m_fFlowerDist = %g;\n", (double)m_fFlowerDist);
   sprintf (szTemp + strlen(szTemp), "m_fFlowerPeak = %g;\n", (double)m_fFlowerPeak);
   sprintf (szTemp + strlen(szTemp), "m_fFlowerDuration = %g;\n", (double)m_fFlowerDuration);
   sprintf (szTemp + strlen(szTemp), "m_dwFlowerShape = %d;\n", m_dwFlowerShape);
   sprintf (szTemp + strlen(szTemp), "m_cFlower = 0x%x;\n", m_cFlower);

   FILE *f;
   f = fopen ("c:\\tree.cpp", "wt");
   if (f) {
      fputs (szTemp, f);
      fclose (f);
   }
#endif

   if (!wcsicmp(psz, gszBack))
      return TRUE;

   // else close
   return FALSE;
}


/**************************************************************************************
CObjectTree::MoveReferencePointQuery - 
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
BOOL CObjectTree::MoveReferencePointQuery (DWORD dwIndex, PCPoint pp)
{
   PSPLINEMOVEP ps;
   DWORD dwDataSize;
   ps = gaTreeMove;
   dwDataSize = sizeof(gaTreeMove);

   if (dwIndex >= dwDataSize / sizeof(SPLINEMOVEP))
      return FALSE;

   // always at 0,0 in trees
   pp->Zero();
   return TRUE;
}

/**************************************************************************************
CObjectTree::MoveReferenceStringQuery -
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
BOOL CObjectTree::MoveReferenceStringQuery (DWORD dwIndex, PWSTR psz, DWORD dwSize, DWORD *pdwNeeded)
{
   PSPLINEMOVEP ps;
   DWORD dwDataSize;
   ps = gaTreeMove;
   dwDataSize = sizeof(gaTreeMove);
   if (dwIndex >= dwDataSize / sizeof(SPLINEMOVEP)) {
      if (pdwNeeded)
         *pdwNeeded = 0;
      return FALSE;
   }

   DWORD dwNeeded;
   dwNeeded = (wcslen (ps[dwIndex].pszName) + 1) * 2;
   if (pdwNeeded)
      *pdwNeeded = dwNeeded;
   if (dwNeeded <= dwSize) {
      wcscpy (psz, ps[dwIndex].pszName);
      return TRUE;
   }
   else
      return FALSE;
}


// FUTURERELEASE - If tree very far away only drawn main trunk (if that) and
// single surface for entire canopy

// FUTURERELEASE - Have trees thicker at the base just before go into ground

