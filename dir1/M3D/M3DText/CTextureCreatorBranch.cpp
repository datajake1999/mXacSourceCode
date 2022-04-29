/********************************************************************************
CTextCreatorBranch.cpp - Code for handling grass.

begun 11/1/06 by Mike Rozak
Copyright 2006 Mike Rozak. All rights reserved
*/

#include <windows.h>
#include <math.h>
#include <crtdbg.h>
#include "resource.h"
#include "escarpment.h"
#include "..\M3D.h"
#include "texture.h"




/**********************************************************************************
CBranchNode::Construtor and destructor */
CBranchNode::CBranchNode (void)
{
   m_lBRANCHINFO.Init (sizeof(BRANCHINFO));
   m_lLEAFINFO.Init (sizeof(LEAFINFO));
   m_fThickness = 0;
   m_pLoc.Zero();
   m_pUp.Zero();
}

CBranchNode::~CBranchNode (void)
{
   DWORD i;
   PBRANCHINFO pbi = (PBRANCHINFO) m_lBRANCHINFO.Get(0);
   for (i = 0; i < m_lBRANCHINFO.Num(); i++)
      delete pbi[i].pNode;
   m_lBRANCHINFO.Clear();
}


/**********************************************************************************
CBranchNode::Clone - Clones the branch
*/
CBranchNode *CBranchNode::Clone (void)
{
   PCBranchNode pNew = new CBranchNode;
   if (!pNew)
      return NULL;

   // NOTE: Don't need to clear the branch list
   
   // add leaves and bacnhes
   pNew->m_lLEAFINFO.Init (sizeof(LEAFINFO), m_lLEAFINFO.Get(0), m_lLEAFINFO.Num());
   pNew->m_lBRANCHINFO.Init (sizeof(BRANCHINFO), m_lBRANCHINFO.Get(0), m_lBRANCHINFO.Num());
   DWORD i;
   PBRANCHINFO pbi = (PBRANCHINFO) pNew->m_lBRANCHINFO.Get(0);
   for (i = 0; i < pNew->m_lBRANCHINFO.Num(); i++)
      pbi[i].pNode = pbi[i].pNode->Clone();

   pNew->m_fThickness = m_fThickness;
   pNew->m_pLoc.Copy (&m_pLoc);
   pNew->m_pUp.Copy (&m_pUp);

   return pNew;
}

/**********************************************************************************
CBranchNode::RemoveLeaf - Looks through the nodes recursively removing all leaves
with the given ID. Any leaf with a higher ID has 1 subtracted from it's ID number.

inputs
   DWORD          dwID - leaf type idenitifier
*/
void CBranchNode::RemoveLeaf (DWORD dwID)
{
   // loop through the sub branches
   DWORD i;
   PBRANCHINFO pbi = (PBRANCHINFO) m_lBRANCHINFO.Get(0);
   for (i = 0; i < m_lBRANCHINFO.Num(); i++, pbi++)
      pbi->pNode->RemoveLeaf(dwID);

   // get rif of the branch
   for (i = m_lLEAFINFO.Num()-1; i < m_lLEAFINFO.Num(); i--) {
      PLEAFINFO pli = (PLEAFINFO) m_lLEAFINFO.Get(i);
      if (pli->dwID == dwID) {
         m_lLEAFINFO.Remove (i);
         continue;
      }
      if (pli->dwID > dwID)
         pli->dwID--;
   }
}


/**********************************************************************************
CBranchNode::LeafMatricies - Fills in CListFixed objects with matricies that
indicate where the leaf objects are to be drawn. These can then be passed into
the clone render functions.

inputs
   PCListFixed    plCMatrix - Pointer to an CListFixed.  Should be
                  initialized with sizeof(CMatrix). They will have all the leaf
                  matricies added
returns
   none
*/
void CBranchNode::LeafMatricies (PCListFixed plCMatrix)
{
   // loop through the sub branches
   DWORD i;
   PBRANCHINFO pbi = (PBRANCHINFO) m_lBRANCHINFO.Get(0);
   for (i = 0; i < m_lBRANCHINFO.Num(); i++, pbi++)
      pbi->pNode->LeafMatricies(plCMatrix);

   // NOTE: Ignoring different leaf types
   PLEAFINFO pli = (PLEAFINFO) m_lLEAFINFO.Get(0);
   plCMatrix->Required (m_lLEAFINFO.Num());
   for (i = 0; i < m_lLEAFINFO.Num(); i++, pli++)
      plCMatrix->Add (&pli->mLeafToObject);
}


/**********************************************************************************
CBranchNode::ScaleThick - Scales the thickness of the branch and all its children.

inputs
   fp          fScale - Amount to scale by
*/
void CBranchNode::ScaleThick (fp fScale)
{
   m_fThickness *= fScale;

   // loop through the sub branches
   DWORD i;
   PBRANCHINFO pbi = (PBRANCHINFO) m_lBRANCHINFO.Get(0);
   for (i = 0; i < m_lBRANCHINFO.Num(); i++, pbi++)
      pbi->pNode->ScaleThick(fScale);

}

/**********************************************************************************
CBranchNode::CalcThickness - Tells the node (and its sub-nodes) to calculate their
m_fThickness variable.

*/
fp CBranchNode::CalcThickness (void)
{
   fp fThick = 1; // always start at 1 thickness for the branch

   m_fIsRoot = FALSE;   // assume it's not a root

   // loop through the sub branches
   DWORD i;
   PBRANCHINFO pbi = (PBRANCHINFO) m_lBRANCHINFO.Get(0);
   for (i = 0; i < m_lBRANCHINFO.Num(); i++, pbi++)
      fThick += pbi->pNode->CalcThickness();


   fThick += m_lLEAFINFO.Num() * 0.1;  // thickness needed for leaf
   //PLEAFINFO pli = (PLEAFINFO) m_lLEAFINFO.Get(0);
   //PLEAFTYPE plt = (PLEAFTYPE) plLEAFTYPE->Get(0);
   //for (i = 0; i < m_lLEAFINFO.Num(); i++, pli++)
   //   fThick += plt[pli->dwID].fWeight;

   m_fThickness = fThick;
   return fThick;
}


/**********************************************************************************
CBranchNode::CalcLoc - Has the branch node set it's location to pLoc and it's up
vector to pUp. This then calls into children and has them set their location and
up vecotrs based on offsets.

inputs
   PCPoint        pLoc - Location to use
   PCPoint        pUp - Up vecotor to use (normalized)
*/
void CBranchNode::CalcLoc (PCPoint pLoc, PCPoint pUp)
{
   m_pLoc.Copy (pLoc);
   m_pUp.Copy (pUp);

   // children
   DWORD i, j;
   PBRANCHINFO pbi = (PBRANCHINFO) m_lBRANCHINFO.Get(0);
   CPoint pLocNew, pUpNew, pA, pB, pC;
   for (i = 0; i < m_lBRANCHINFO.Num(); i++, pbi++) {
      pLocNew.Add (&m_pLoc, &pbi->pLoc);

      pA.Copy (&pbi->pLoc);
      pA.Normalize();
      if (pA.Length() < CLOSE) {
         // infinitely short... keep same up
         pUpNew.Copy (&m_pUp);
         goto senddown;
      }
      pC.Copy (pUp);
      pB.CrossProd (&pC, &pA);
      if (pB.Length() < CLOSE) {
         for (j = 0; j < 3; j++) {
            pC.Zero();
            pC.p[j] = 1;
            pB.CrossProd (&pC, &pA);
            if (pB.Length() >= CLOSE)
               break;
         }
      }

      pB.Normalize();
      pUpNew.CrossProd (&pA, &pB);
      pUpNew.Normalize();

senddown:
      pbi->pNode->CalcLoc (&pLocNew, &pUpNew);
   }

   // calculate the pX and matrix for all the leaves
   PLEAFINFO pli;
   pli = (PLEAFINFO) m_lLEAFINFO.Get(0);
   for (i = 0; i < m_lLEAFINFO.Num(); i++, pli++) {
      // assume pY and pZ are already normalized
      pli->pX.CrossProd (&pli->pY, &pli->pZ);
      pli->pX.Normalize();

      // matrix
      CMatrix mRot, mTrans, mScale;
      mScale.Scale (pli->fScale, pli->fScale, pli->fScale);
      mTrans.Translation (m_pLoc.p[0], m_pLoc.p[1], m_pLoc.p[2]);
      mRot.RotationFromVectors (&pli->pX, &pli->pY, &pli->pZ);

      pli->mLeafToObject.Multiply (&mRot, &mScale);
      pli->mLeafToObject.MultiplyRight (&mTrans);
   }
}


/**************************************************************************************
BranchAddNoodle - Given a list of plLoc, plUp, plThick, this creates a new CNoodle
object and adds  the information to plBRANCHNOODLE.

inputs
   PCListFixed       plLoc - Pointer to a list of PCPoint
   PCListFixed       plUp - Pointer to list of PCPoiunt
   PCListFixed       plThick - Pointer to list of PCPoint
   PCListFixed       plBRANCHNOODLE - Pointer to list of BRANCHNOODLE
   DWORD             dwDivide - Number of times to divide
   BOOL              fRound - If TRUE make very round trunk
   BOOL              fCap - If TRUE then cap ends
returns
   none
*/
static void BranchAddNoodle (PCListFixed plLoc, PCListFixed plUp, PCListFixed plThick,
                             PCListFixed plBRANCHNOODLE, DWORD dwDivide, BOOL fRound, DWORD dwTextureWrap,
                             BOOL fCap)
{
   PCPoint pLoc = (PCPoint) plLoc->Get(0);
   PCPoint pUp = (PCPoint) plUp->Get(0);
   PCPoint pThick = (PCPoint) plThick->Get(0);
   BRANCHNOODLE bn;
   memset (&bn, 0, sizeof(bn));
   if (plLoc->Num() < 2)
      return;  // not enough points

   // BUGFIX - Ignoring up values passed in and making up own
   // find the vector for the first part
   CPoint pDir, pDirLast, pC, pA;
   DWORD i,j;
   pC.Zero();
   pC.p[0] = 1;
   // find a slight bend
   for (i = 0; i+2 < plUp->Num(); i++) {
      pDir.Subtract (&pLoc[i+1], &pLoc[i]);
      pDirLast.Subtract (&pLoc[i+2], &pLoc[i+1]);
      pDir.Normalize();
      pDirLast.Normalize();
      pA.CrossProd (&pDir, &pDirLast);
      if (pA.Length() > CLOSE) {
         pA.Normalize();
         pC.Copy (&pA);
         break;
      }
   }
   pDirLast.Zero();
   pDirLast.p[2] = 1;
   for (i = 0; i < plUp->Num(); i++) {
      if (i+1 < plUp->Num())
         pDir.Subtract (&pLoc[i+1], &pLoc[i]);
      else
         pDir.Subtract (&pLoc[i], &pLoc[i-1]);
      pDir.Normalize();
      if (pDir.Length() < CLOSE)
         pDir.Copy (&pDirLast);
      pDirLast.Copy (&pDir);

      pA.CrossProd (&pDir, &pC);
      if (pA.Length() < CLOSE) {
         for (j = 0; i < 3; j++) {
            pC.Zero();
            pC.p[j] = 1;
            pA.CrossProd (&pDir, &pC);
            if (pA.Length() >= CLOSE)
               break;
         }
      }

      pA.Normalize();
      pC.CrossProd (&pA, &pDir);
      pC.Normalize();
      pUp[i].Copy (&pC);
   }

   // figure out the length
   CPoint pDist;
   bn.fLength = 0;
   for (i = 1; i < plLoc->Num(); i++) {
      pDist.Subtract (&pLoc[i], &pLoc[i-1]);
      bn.fLength += pDist.Length();
   }

   // ndole
   bn.pNoodle = new CNoodle;
   if (!bn.pNoodle)
      return;
   bn.pNoodle->BackfaceCullSet (TRUE);
   bn.pNoodle->DrawEndsSet (fCap);
   bn.pNoodle->PathSpline (FALSE, plLoc->Num(), pLoc, (DWORD*) SEGCURVE_CUBIC, dwDivide);
   bn.pNoodle->ScaleSpline (plThick->Num(), pThick, (DWORD*) SEGCURVE_LINEAR, dwDivide);
   bn.pNoodle->ShapeDefault (fRound ? NS_CIRCLE : NS_CIRCLEFAST);
   bn.pNoodle->TextureWrapSet (dwTextureWrap);
   bn.pNoodle->FrontSpline (plUp->Num(), pUp, (DWORD*) SEGCURVE_LINEAR, dwDivide);

   // add noodle
   plBRANCHNOODLE->Add (&bn);
}


/**************************************************************************************
CBranchNode::FillNoodle - This is a somehwat complex funtion that:
1) For the thickest branch node and all it's children, adds their location, up,
   and thickness to plLoc, plUp, and plThick.
2) For all other childnren, creates a noodle and fills in a BRANCHNOODLE structure that
   is used to fill in plBRANCHNOODLE

inputs
   fp                fThickScale - Scale thickness by this much
   PCListFixed       plLoc - Pointer to a list of PCPoint which is filled in with location
   PCListFixed       plUp - Pointer to list of PCPoiunt which is filled in with up vectors
   PCListFixed       plThick - Pointer to list of PCPoint containing scale inforamtion
   PCListFixed       plBRANCHNOODLE - Pointer to list of BRANCHNOODLE, which are added for sub-branches
   DWORD             dwDivide - Number of times to divide noodles
   BOOL              fRound - If TRUE then very rounded trunk
   DWORD             dwTextureWrap - Number of times to wrap texture around
   BOOL              fCap - If TRUE then cap the ends
returns
   none
*/
void CBranchNode::FillNoodle (fp fThickScale, PCListFixed plLoc, PCListFixed plUp, PCListFixed plThick,
   PCListFixed plBRANCHNOODLE, DWORD dwDivide, BOOL fRound, DWORD dwTextureWrap, BOOL fCap)
{
   // fill in for this one
   CPoint pScale;
   pScale.Zero();
   m_fThickDist = sqrt(m_fThickness) * fThickScale;
   pScale.p[0] = pScale.p[1] = m_fThickDist;
   plLoc->Add (&m_pLoc);
   plUp->Add (&m_pUp);
   plThick->Add (&pScale);

   if (!m_lBRANCHINFO.Num())
      return;

   // find the thickest
   PBRANCHINFO pbi = (PBRANCHINFO) m_lBRANCHINFO.Get(0);
   DWORD i;
   fp fThickest, fThick;
   DWORD dwThickest;
   dwThickest = 0;
   fThickest = pbi->pNode->m_fIsRoot ? 0 : pbi->pNode->m_fThickness; // roots are never thickest
   pbi++;
   for (i = 1; i < m_lBRANCHINFO.Num(); i++, pbi++) {
      fThick = pbi->pNode->m_fIsRoot ? 0 : pbi->pNode->m_fThickness; // roots are never thickest
      if (fThick > fThickest) {
         fThickest = fThick;
         dwThickest = i;
      }
   }

   // and child
   pbi = (PBRANCHINFO) m_lBRANCHINFO.Get(dwThickest);
   pbi->pNode->FillNoodle (fThickScale, plLoc, plUp, plThick, plBRANCHNOODLE,
      dwDivide, fRound, dwTextureWrap, fCap);

   pbi = (PBRANCHINFO) m_lBRANCHINFO.Get(0);
   CListFixed lLoc, lUp, lThick;
   lLoc.Init (sizeof(CPoint));
   lUp.Init (sizeof(CPoint));
   lThick.Init (sizeof(CPoint));
   lLoc.Required (m_lBRANCHINFO.Num());
   lUp.Required (m_lBRANCHINFO.Num());
   lThick.Required (m_lBRANCHINFO.Num());
   for (i = 0; i < m_lBRANCHINFO.Num(); i++, pbi++) {
      if (i == dwThickest)
         continue;   // dont do thickest
      lLoc.Clear();
      lUp.Clear();
      lThick.Clear();

      // start out with this
      lLoc.Add (&m_pLoc);
      lUp.Add (&m_pUp);
      lThick.Add (&pScale);

      // call into children
      pbi->pNode->FillNoodle (fThickScale, &lLoc, &lUp, &lThick, plBRANCHNOODLE,
         dwDivide, fRound, dwTextureWrap, fCap);

      // go back and reset the thickness of the first node so that it is
      // the same as the next one, otherwise connecting branches are wrong thickness
      PCPoint p;
      p = (PCPoint) lThick.Get(0);
      if (lThick.Num() >= 2)
         p[0].Copy (&p[1]);

      // if this is the start of a root then double
      if (pbi->pNode->m_fIsRoot)
         p[0].Scale(2);

      // add this to the list
      BranchAddNoodle (&lLoc, &lUp, &lThick, plBRANCHNOODLE, dwDivide, fRound, dwTextureWrap, fCap);
   }
}


/****************************************************************************
BranchPage
*/
BOOL BranchPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PTEXTEDITINFO pt = (PTEXTEDITINFO) pPage->m_pUserData;
   PCTextCreatorBranch pv = (PCTextCreatorBranch) pt->pThis;

   if (TEHelperMessageHook (pPage, dwMessage, pParam))
      return TRUE;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         // set the material
         PCEscControl pControl;
         ComboBoxSet (pPage, L"material", pv->m_Material.m_dwID);
         pControl = pPage->ControlFind (L"editmaterial");
         if (pControl)
            pControl->Enable (pv->m_Material.m_dwID ? FALSE : TRUE);


         
         // ComboBoxSet (pPage, L"tipshape", pv->m_dwTipShape);

         DoubleToControl (pPage, L"patternwidth", pv->m_iWidth);
         DoubleToControl (pPage, L"patternheight", pv->m_iHeight);

         pControl = pPage->ControlFind (L"transbase");
         if (pControl)
            pControl->AttribSetInt (Pos(), (int)(pv->m_fTransBase * 100.0));

         pControl = pPage->ControlFind (L"darkenatbottom");
         if (pControl)
            pControl->AttribSetInt (Pos(), (int)(pv->m_fDarkenAtBottom * 100.0));

         //pControl = pPage->ControlFind (L"tipwidth");
         //if (pControl)
         //   pControl->AttribSetInt (Pos(), (int)(pv->m_fTipWidth * 100.0));

         //pControl = pPage->ControlFind (L"tiplength");
         //if (pControl)
         //   pControl->AttribSetInt (Pos(), (int)(pv->m_fTipLength * 100.0));

         pControl = pPage->ControlFind (L"stemlength");
         if (pControl)
            pControl->AttribSetInt (Pos(), (int)(pv->m_fStemLength * 100.0));

         pControl = pPage->ControlFind (L"lowerdarker");
         if (pControl)
            pControl->AttribSetInt (Pos(), (int)(pv->m_fLowerDarker * 100.0));

         //WCHAR szTemp[64];
         //DWORD i;
         //for (i = 0; i < 3; i++) {
         //   swprintf (szTemp, L"leafcolor%d", (int)i);
         //   FillStatusColor (pPage, szTemp, pv->m_acLeaf[i]);
         //}

         FillStatusColor (pPage, L"stemcolor", pv->m_cStemColor);
         FillStatusColor (pPage, L"branchcolor", pv->m_cBranchColor);

         // DoubleToControl (pPage, L"seed", (fp) pv->m_AutoBranch.dwSeed);
         DoubleToControl (pPage, L"maxgen", (fp) pv->m_AutoBranch.dwMaxGen);

         //if (pControl = pPage->ControlFind (L"branchinitialangle"))
         //   pControl->AttribSetInt (Pos(), (int) (pv->m_AutoBranch.fBranchInitialAngle * 100.0));
         // if (pControl = pPage->ControlFind (L"branchinitialangle"))
         //   pControl->AttribSetInt (Pos(), (int) (pv->m_AutoBranch.fBranchInitialAngle * 100.0));
         if (pControl = pPage->ControlFind (L"branchlength"))
            pControl->AttribSetInt (Pos(), (int) (pv->m_AutoBranch.fBranchLength * 100.0));
         if (pControl = pPage->ControlFind (L"branchlengthvar"))
            pControl->AttribSetInt (Pos(), (int) (pv->m_AutoBranch.fBranchLengthVar * 100.0));
         if (pControl = pPage->ControlFind (L"branchshorten"))
            pControl->AttribSetInt (Pos(), (int) (pv->m_AutoBranch.fBranchShorten * 100.0));

         if (pControl = pPage->ControlFind (L"branchextendprob"))
            pControl->AttribSetInt (Pos(), (int) (pv->m_AutoBranch.fBranchExtendProb * 100.0));
         if (pControl = pPage->ControlFind (L"branchextendprobgen"))
            pControl->AttribSetInt (Pos(), (int) (pv->m_AutoBranch.fBranchExtendProbGen * 100.0));
         if (pControl = pPage->ControlFind (L"branchdirvar"))
            pControl->AttribSetInt (Pos(), (int) (pv->m_AutoBranch.fBranchDirVar * 100.0));

         if (pControl = pPage->ControlFind (L"branchforkprob"))
            pControl->AttribSetInt (Pos(), (int) (pv->m_AutoBranch.fBranchForkProb * 100.0));
         if (pControl = pPage->ControlFind (L"branchforkprobgen"))
            pControl->AttribSetInt (Pos(), (int) (pv->m_AutoBranch.fBranchForkProbGen * 100.0));
         if (pControl = pPage->ControlFind (L"branchforknum"))
            pControl->AttribSetInt (Pos(), (int) (pv->m_AutoBranch.fBranchForkNum * 100.0));
         if (pControl = pPage->ControlFind (L"branchforkforwards"))
            pControl->AttribSetInt (Pos(), (int) (pv->m_AutoBranch.fBranchForkForwards * 100.0));
         if (pControl = pPage->ControlFind (L"branchforkUpDown"))
            pControl->AttribSetInt (Pos(), (int) (pv->m_AutoBranch.fBranchForkUpDown * 100.0));
         if (pControl = pPage->ControlFind (L"branchforkvar"))
            pControl->AttribSetInt (Pos(), (int) (pv->m_AutoBranch.fBranchForkVar * 100.0));

         if (pControl = pPage->ControlFind (L"branchgrav"))
            pControl->AttribSetInt (Pos(), (int) (pv->m_AutoBranch.fBranchGrav * 100.0));
         if (pControl = pPage->ControlFind (L"branchgravgen"))
            pControl->AttribSetInt (Pos(), (int) (pv->m_AutoBranch.fBranchGravGen * 100.0));

         if (pControl = pPage->ControlFind (L"leafextendprob"))
            pControl->AttribSetInt (Pos(), (int) (pv->m_AutoBranch.fLeafExtendProb * 100.0));
         if (pControl = pPage->ControlFind (L"leafextendprobgen"))
            pControl->AttribSetInt (Pos(), (int) (pv->m_AutoBranch.fLeafExtendProbGen * 100.0));
         if (pControl = pPage->ControlFind (L"leafextendnum"))
            pControl->AttribSetInt (Pos(), (int) (pv->m_AutoBranch.fLeafExtendNum * 100.0));
         if (pControl = pPage->ControlFind (L"leafextendforwards"))
            pControl->AttribSetInt (Pos(), (int) (pv->m_AutoBranch.fLeafExtendForwards * 100.0));
         if (pControl = pPage->ControlFind (L"leafextendvar"))
            pControl->AttribSetInt (Pos(), (int) (pv->m_AutoBranch.fLeafExtendVar * 100.0));
         if (pControl = pPage->ControlFind (L"leafextendscale"))
            pControl->AttribSetInt (Pos(), (int) (pv->m_AutoBranch.fLeafExtendScale * 100.0));
         if (pControl = pPage->ControlFind (L"leafextendscalevar"))
            pControl->AttribSetInt (Pos(), (int) (pv->m_AutoBranch.fLeafExtendScaleVar * 100.0));
         if (pControl = pPage->ControlFind (L"leafextendscalegen"))
            pControl->AttribSetInt (Pos(), (int) (pv->m_AutoBranch.fLeafExtendScaleGen * 100.0));

         if (pControl = pPage->ControlFind (L"leafendprob"))
            pControl->AttribSetInt (Pos(), (int) (pv->m_AutoBranch.fLeafEndProb * 100.0));
         if (pControl = pPage->ControlFind (L"leafendnum"))
            pControl->AttribSetInt (Pos(), (int) (pv->m_AutoBranch.fLeafEndNum * 100.0));
         if (pControl = pPage->ControlFind (L"leafendnumvar"))
            pControl->AttribSetInt (Pos(), (int) (pv->m_AutoBranch.fLeafEndNumVar * 100.0));
         if (pControl = pPage->ControlFind (L"leafendsymmetry"))
            pControl->AttribSetInt (Pos(), (int) (pv->m_AutoBranch.fLeafEndSymmetry * 100.0));
         if (pControl = pPage->ControlFind (L"leafendvar"))
            pControl->AttribSetInt (Pos(), (int) (pv->m_AutoBranch.fLeafEndVar * 100.0));
         if (pControl = pPage->ControlFind (L"leafendscale"))
            pControl->AttribSetInt (Pos(), (int) (pv->m_AutoBranch.fLeafEndScale * 100.0));
         if (pControl = pPage->ControlFind (L"leafendscalevar"))
            pControl->AttribSetInt (Pos(), (int) (pv->m_AutoBranch.fLeafEndScaleVar * 100.0));

         if (pControl = pPage->ControlFind (L"darkedge"))
            pControl->AttribSetInt (Pos(), (int) (pv->m_fDarkEdge * 100.0));

         fp f;
         f = log10 (pv->m_fThickScale * 1000.0) * 50.0;
         f = max(0, f);
         f = min(99, f);
         pControl = pPage->ControlFind (L"thickscale");
         if (pControl)
            pControl->AttribSetInt (Pos(), (int)f);
      }
      break;

   case ESCN_SCROLL:
   // dont bother case ESCN_SCROLLING:
      {
         PESCNSCROLL p = (PESCNSCROLL) pParam;

         // only do one scroll bar
         if (!p->pControl->m_pszName)
            break;

         // set value
         int iVal;
         iVal = p->pControl->AttribGetInt (Pos());
         fp fVal = (fp)iVal / 100.0;
         //if (!_wcsicmp(p->pControl->m_pszName, L"tipwidth")) {
         //   pv->m_fTipWidth = fVal;
         //   return TRUE;
         //}
         if (!_wcsicmp(p->pControl->m_pszName, L"transbase")) {
            pv->m_fTransBase = fVal;
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"darkenatbottom")) {
            pv->m_fDarkenAtBottom = fVal;
            return TRUE;
         }
         //else if (!_wcsicmp(p->pControl->m_pszName, L"tiplength")) {
         //   pv->m_fTipLength = fVal;
         //   return TRUE;
         //}
         else if (!_wcsicmp(p->pControl->m_pszName, L"stemlength")) {
            pv->m_fStemLength = fVal;
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"lowerdarker")) {
            pv->m_fLowerDarker = fVal;
            return TRUE;
         }

         if (!_wcsicmp(p->pControl->m_pszName, L"thickscale")) {
            fp fVal = (fp) p->iPos;
            fVal =  pow (10.0, fVal / 50.0) / 1000.0;
            // inverse: f = log10 (pv->m_fThickScale * 1000.0) * 20.0;
            if (fVal == pv->m_fThickScale)
               return TRUE; // no change

            pv->m_fThickScale = fVal;
            return TRUE;
         }

         fp *pf = NULL;

         if (!_wcsicmp(p->pControl->m_pszName, L"branchinitialangle"))
            pf = &pv->m_AutoBranch.fBranchInitialAngle;
         else if (!_wcsicmp(p->pControl->m_pszName, L"branchinitialangle"))
            pf = &pv->m_AutoBranch.fBranchInitialAngle;
         else if (!_wcsicmp(p->pControl->m_pszName, L"branchlengthvar"))
            pf = &pv->m_AutoBranch.fBranchLengthVar;
         else if (!_wcsicmp(p->pControl->m_pszName, L"branchlength"))
            pf = &pv->m_AutoBranch.fBranchLength;
         else if (!_wcsicmp(p->pControl->m_pszName, L"branchshorten"))
            pf = &pv->m_AutoBranch.fBranchShorten;

         else if (!_wcsicmp(p->pControl->m_pszName, L"branchextendprob"))
            pf = &pv->m_AutoBranch.fBranchExtendProb;
         else if (!_wcsicmp(p->pControl->m_pszName, L"branchextendprobgen"))
            pf = &pv->m_AutoBranch.fBranchExtendProbGen;
         else if (!_wcsicmp(p->pControl->m_pszName, L"branchdirvar"))
            pf = &pv->m_AutoBranch.fBranchDirVar;

         else if (!_wcsicmp(p->pControl->m_pszName, L"branchforkprob"))
            pf = &pv->m_AutoBranch.fBranchForkProb;
         else if (!_wcsicmp(p->pControl->m_pszName, L"branchforkprobgen"))
            pf = &pv->m_AutoBranch.fBranchForkProbGen;
         else if (!_wcsicmp(p->pControl->m_pszName, L"branchforknum"))
            pf = &pv->m_AutoBranch.fBranchForkNum;
         else if (!_wcsicmp(p->pControl->m_pszName, L"branchforkforwards"))
            pf = &pv->m_AutoBranch.fBranchForkForwards;
         else if (!_wcsicmp(p->pControl->m_pszName, L"branchforkUpDown"))
            pf = &pv->m_AutoBranch.fBranchForkUpDown;
         else if (!_wcsicmp(p->pControl->m_pszName, L"branchforkvar"))
            pf = &pv->m_AutoBranch.fBranchForkVar;

         else if (!_wcsicmp(p->pControl->m_pszName, L"branchgrav"))
            pf = &pv->m_AutoBranch.fBranchGrav;
         else if (!_wcsicmp(p->pControl->m_pszName, L"branchgravgen"))
            pf = &pv->m_AutoBranch.fBranchGravGen;

         else if (!_wcsicmp(p->pControl->m_pszName, L"leafextendprob"))
            pf = &pv->m_AutoBranch.fLeafExtendProb;
         else if (!_wcsicmp(p->pControl->m_pszName, L"leafextendprobgen"))
            pf = &pv->m_AutoBranch.fLeafExtendProbGen;
         else if (!_wcsicmp(p->pControl->m_pszName, L"leafextendnum"))
            pf = &pv->m_AutoBranch.fLeafExtendNum;
         else if (!_wcsicmp(p->pControl->m_pszName, L"leafextendforwards"))
            pf = &pv->m_AutoBranch.fLeafExtendForwards;
         else if (!_wcsicmp(p->pControl->m_pszName, L"leafextendvar"))
            pf = &pv->m_AutoBranch.fLeafExtendVar;
         else if (!_wcsicmp(p->pControl->m_pszName, L"leafextendscale"))
            pf = &pv->m_AutoBranch.fLeafExtendScale;
         else if (!_wcsicmp(p->pControl->m_pszName, L"leafextendscalevar"))
            pf = &pv->m_AutoBranch.fLeafExtendScaleVar;
         else if (!_wcsicmp(p->pControl->m_pszName, L"leafextendscalegen"))
            pf = &pv->m_AutoBranch.fLeafExtendScaleGen;

         else if (!_wcsicmp(p->pControl->m_pszName, L"leafendprob"))
            pf = &pv->m_AutoBranch.fLeafEndProb;
         else if (!_wcsicmp(p->pControl->m_pszName, L"leafendnum"))
            pf = &pv->m_AutoBranch.fLeafEndNum;
         else if (!_wcsicmp(p->pControl->m_pszName, L"leafendnumvar"))
            pf = &pv->m_AutoBranch.fLeafEndNumVar;
         else if (!_wcsicmp(p->pControl->m_pszName, L"leafendsymmetry"))
            pf = &pv->m_AutoBranch.fLeafEndSymmetry;
         else if (!_wcsicmp(p->pControl->m_pszName, L"leafendvar"))
            pf = &pv->m_AutoBranch.fLeafEndVar;
         else if (!_wcsicmp(p->pControl->m_pszName, L"leafendscale"))
            pf = &pv->m_AutoBranch.fLeafEndScale;
         else if (!_wcsicmp(p->pControl->m_pszName, L"leafendscalevar"))
            pf = &pv->m_AutoBranch.fLeafEndScaleVar;

         else if (!_wcsicmp(p->pControl->m_pszName, L"darkedge"))
            pf = &pv->m_fDarkEdge;

         if (!pf)
            break;   // not on of ours
         if (*pf == fVal)
            break;   // no change

         *pf = fVal;
      }
      break;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;

         if (!p->pControl || !p->pControl->m_pszName)
            break;

         //PWSTR pszLeafButton = L"leafbutton";
         //DWORD dwLeafButtonLen = wcslen(pszLeafButton);

         if (!_wcsicmp(p->pControl->m_pszName, L"seed")) {
            pv->m_iSeed += GetTickCount();
            pPage->MBSpeakInformation (L"New variation created.");
            return TRUE;
         }
         //else if (!wcsncmp(p->pControl->m_pszName, pszLeafButton, dwLeafButtonLen)) {
         //   DWORD dwNum = _wtoi(p->pControl->m_pszName + dwLeafButtonLen);
         //   WCHAR szTemp[64];
         //   swprintf (szTemp, L"leafcolor%d", (int)dwNum);

         //   pv->m_acLeaf[dwNum] = AskColor (pPage->m_pWindow->m_hWnd, pv->m_acLeaf[dwNum],
         //      pPage, szTemp);
         //   return TRUE;
         //}
         else if (!_wcsicmp(p->pControl->m_pszName, L"stembutton")) {
            pv->m_cStemColor = AskColor (pPage->m_pWindow->m_hWnd, pv->m_cStemColor,
               pPage, L"stemcolor");
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"branchbutton")) {
            pv->m_cBranchColor = AskColor (pPage->m_pWindow->m_hWnd, pv->m_cBranchColor,
               pPage, L"branchcolor");
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"editmaterial")) {
            if (!pv->m_Material.Dialog (pPage->m_pWindow->m_hWnd))
               pPage->Exit (L"[close]");
            return TRUE;
         }
      }
      break;

   case ESCN_COMBOBOXSELCHANGE:
      {
         PESCNCOMBOBOXSELCHANGE p = (PESCNCOMBOBOXSELCHANGE) pParam;
         if (!p->pControl->m_pszName)
            break;

         // only care about transparency
         if (!_wcsicmp(p->pControl->m_pszName, L"material")) {
            DWORD dwVal;
            dwVal = p->pszName ? _wtoi(p->pszName) : 0;
            if (dwVal == pv->m_Material.m_dwID)
               break; // unchanged
            if (dwVal)
               pv->m_Material.InitFromID (dwVal);
            else
               pv->m_Material.m_dwID = MATERIAL_CUSTOM;

            // eanble/disable button to edit
            PCEscControl pControl;
            pControl = pPage->ControlFind (L"editmaterial");
            if (pControl)
               pControl->Enable (pv->m_Material.m_dwID ? FALSE : TRUE);

            return TRUE;
         }
         //else if (!_wcsicmp(p->pControl->m_pszName, L"tipshape")) {
         //   DWORD dwVal;
         //   dwVal = p->pszName ? _wtoi(p->pszName) : 0;
         //   if (dwVal == pv->m_dwTipShape)
         //      break; // unchanged

         //   pv->m_dwTipShape = dwVal;
         //   return TRUE;
         //}
      }
      break;

   case ESCM_USER+186:  // get all the control values
      {
         pv->m_iWidth = (int) DoubleFromControl (pPage, L"patternwidth");
         pv->m_iWidth = max(pv->m_iWidth, 1);

         pv->m_iHeight = (int) DoubleFromControl (pPage, L"patternheight");
         pv->m_iHeight = max(pv->m_iHeight, 1);

         // MeasureParseString (pPage, L"branchlength", &pv->m_AutoBranch.fBranchLength);
         // pv->m_AutoBranch.fBranchLength = max(pv->m_AutoBranch.fBranchLength, CLOSE);

         // pv->m_AutoBranch.dwSeed = (DWORD) DoubleFromControl (pPage, L"seed");
         pv->m_AutoBranch.dwMaxGen = (DWORD) DoubleFromControl (pPage, L"maxgen");
         pv->m_AutoBranch.dwMaxGen = max(1, pv->m_AutoBranch.dwMaxGen);
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Branch";
            return TRUE;
         }
      }
      break;
   };

   return DefPage (pPage, dwMessage, pParam);
}


BOOL CTextCreatorBranch::Dialog (PCEscWindow pWindow)
{
   TEXTEDITINFO ti;
   PWSTR pszRet;
   memset (&ti, 0, sizeof(ti));
   ti.hBit = NULL;
   ti.pCreator = this;
   ti.pThis = this;
   ti.fDrawFlat = TRUE;

redo:
   pszRet = pWindow->PageDialog (ghInstance, IDR_MMLTEXTUREBRANCH, BranchPage, &ti);
   if (ti.hBit) {
      DeleteObject (ti.hBit);
      ti.hBit = NULL;
   }
   if (pszRet && !_wcsicmp(pszRet, L"tip")) {
      BOOL fRet = m_Leaf.Dialog (m_dwRenderShard, pWindow);
      if (!fRet)
         return FALSE;
      goto redo;
   }

   return pszRet && !_wcsicmp(pszRet, Back());
}

CTextCreatorBranch::CTextCreatorBranch (DWORD dwRenderShard, DWORD dwType)
{
   m_dwRenderShard = dwRenderShard;
   m_Material.InitFromID (MATERIAL_PAINTSEMIGLOSS);
   m_dwType = dwType;
   m_iSeed = 1234;
   m_iWidth = 512+256;
   m_iHeight = 512;
   m_fAngleVariation = 0.5;
   //m_acLeaf[0] = RGB(0, 0x80, 0x20);
   //m_acLeaf[1] = RGB(0x40, 0xa0, 0);
   //m_acLeaf[2] = RGB(0x40, 0xff, 0);

   m_fTransBase = 0.1;
   m_fDarkenAtBottom = 1;
   //m_dwTipShape = 1;
   m_cStemColor = RGB(0xff, 0xc0, 0x80);
   m_cBranchColor = RGB(0x40, 0x30, 0);
   //m_fTipWidth = 0.2;
   //m_fTipLength = 0.5;
   m_fStemLength = 0.5;
   m_fLowerDarker = 0.75;

   m_fThickScale = 0.01;
   m_fDarkEdge = 0.5;

   AutoBranchFill ();

   m_lBRANCHNOODLE.Init (sizeof(BRANCHNOODLE));
}

CTextCreatorBranch::~CTextCreatorBranch (void)
{
   // clear the branch noodle
   DWORD i;
   PBRANCHNOODLE pbn = (PBRANCHNOODLE) m_lBRANCHNOODLE.Get(0);
   for (i = 0; i < m_lBRANCHNOODLE.Num(); i++, pbn++)
      delete pbn->pNoodle;
   m_lBRANCHNOODLE.Clear();
}


void CTextCreatorBranch::Delete (void)
{
   delete this;
}

static PWSTR gpszNoise = L"Noise";
static PWSTR gpszType = L"Type";
static PWSTR gpszSeed = L"Seed";
static PWSTR gpszWidth = L"Width";
static PWSTR gpszHeight = L"Height";
static PWSTR gpszAngleVariation = L"AngleVariation";
static PWSTR gpszTransBase = L"TransBase";
static PWSTR gpszTipShape = L"TipShape";
static PWSTR gpszTipWidth = L"TipWidth";
static PWSTR gpszTipLength = L"TipLength";
static PWSTR gpszDarkenAtBottom = L"DarkenAtBottom";

static PWSTR gpszStemColor = L"StemColor";
static PWSTR gpszBranchColor = L"BranchColor";
static PWSTR gpszBranchInitialAngle = L"BranchInitialAngle"; // initial angle. 0..PI. 0 = up
static PWSTR gpszBranchLength = L"BranchLength"; // typical length, meters
static PWSTR gpszBranchLengthVar = L"BranchLengthVar"; // variation in lenght. 0..1
static PWSTR gpszBranchShorten = L"BranchShorten";   // amount shortens per generation. 0..1

// branch extension
static PWSTR gpszBranchExtendProb = L"BranchExtendProb";      // probability that will extend branch, 0..1
static PWSTR gpszBranchExtendProbGen = L"BranchExtendProbGen";   // change in fBranchExtendProb per generation, 0..1
static PWSTR gpszBranchDirVar = L"BranchDirVar";          // variation in direction, 0..1

// branch forking
static PWSTR gpszBranchForkProb = L"BranchForkProb";        // probability of branch forking, 0..1
static PWSTR gpszBranchForkProbGen = L"BranchForkProbGen";     // chance of forking increasing per generation, 0..1
static PWSTR gpszBranchForkNum = L"BranchForkNum";         // number of forks, 0..1.
static PWSTR gpszBranchForkForwards = L"BranchForkForwards";    // how forwards. 1 = forwards, 0 = backwards
static PWSTR gpszBranchForkUpDown = L"BranchForkUpDown";      // how up/down. 1=up, 0=down
static PWSTR gpszBranchForkVar = L"BranchForkVar";         // variation in forking, 0..1.

// branch gravity
static PWSTR gpszBranchGrav = L"BranchGrav";            // 1=branchs will want to go upwards, 0=branch go downwards
static PWSTR gpszBranchGravGen = L"BranchGravGen";         // how much fBranchGrav changes each generation. 1 = increase, 0 = decrease

// leaves along branch
static PWSTR gpszLeafExtendProb = L"LeafExtendProb";        // 0..1, probability of leaf being along extension
static PWSTR gpszLeafExtendProbGen = L"LeafExtendProbGen";     // 0..1, likelihood of leaves increasing
static PWSTR gpszLeafExtendNum = L"LeafExtendNum";         // 0..1, number of leaves
static PWSTR gpszLeafExtendForwards = L"LeafExtendForwards";    // 0..1, 1 = forwards, 0 = backwards
static PWSTR gpszLeafExtendVar = L"LeafExtendVar";         // 0..1, variation in leaf direction
static PWSTR gpszLeafExtendScale = L"LeafExtendScale";       // 0..1, .5 = default scale
static PWSTR gpszLeafExtendScaleVar = L"LeafExtendScaleVar";    // 0..1, amount leaf size varies
static PWSTR gpszLeafExtendScaleGen = L"LeafExtendScaleGen";    // 0..1, how much leaf size changes, .5 = none, 1 = larger

// leaf at terminal node
static PWSTR gpszLeafEndProb = L"LeafEndProb";           // 0..1, likelihood of leaf at end
static PWSTR gpszLeafEndNum = L"LeafEndNum";            // 0..1, number of leaves at the end
static PWSTR gpszLeafEndNumVar = L"LeafEndNumVar";         // 0..1, variation in the number of leaves at the end
static PWSTR gpszLeafEndSymmetry = L"LeafEndSymmetry";       // 0..1, amount of symmetry in end leaves
static PWSTR gpszLeafEndVar = L"LeafEndVar";            // 0..1, variation in leaf location at end
static PWSTR gpszLeafEndScale = L"LeafEndScale";          // 0..1, .5 = default scale
static PWSTR gpszLeafEndScaleVar = L"LeafEndScaleVar";       // 0..1, amount leaf size varies

static PWSTR gpszThickScale = L"ThickScale";
static PWSTR gpszStemLength = L"StemLength";
static PWSTR gpszLowerDarker = L"LowerDarker";
static PWSTR gpszDarkEdge = L"DarkEdge";

// misc
static PWSTR gpszMaxGen = L"MaxGen";               // maximum generations
static PWSTR gpszLeafForTexture = L"LeafForTexture";

PCMMLNode2 CTextCreatorBranch::MMLTo (void)
{
   PCMMLNode2 pNode = new CMMLNode2;
   if (!pNode)
      return NULL;
   pNode->NameSet (gpszNoise);

   m_Material.MMLTo(pNode);

   MMLValueSet (pNode, gpszType, (int)m_dwType);
   MMLValueSet (pNode, gpszSeed, (int)m_iSeed);
   MMLValueSet (pNode, gpszWidth, m_iWidth);
   MMLValueSet (pNode, gpszHeight, m_iHeight);
   MMLValueSet (pNode, gpszAngleVariation, m_fAngleVariation);
   MMLValueSet (pNode, gpszTransBase, m_fTransBase);
   MMLValueSet (pNode, gpszDarkenAtBottom, m_fDarkenAtBottom);

   //MMLValueSet (pNode, gpszTipShape, (int)m_dwTipShape);
   MMLValueSet (pNode, gpszStemColor, (int)m_cStemColor);
   MMLValueSet (pNode, gpszBranchColor, (int)m_cBranchColor);
   //MMLValueSet (pNode, gpszTipWidth, m_fTipWidth);
   //MMLValueSet (pNode, gpszTipLength, m_fTipLength);
   MMLValueSet (pNode, gpszStemLength, m_fStemLength);
   MMLValueSet (pNode, gpszLowerDarker, m_fLowerDarker);

   PCMMLNode2 pSub = m_Leaf.MMLTo();
   if (pSub) {
      pSub->NameSet (gpszLeafForTexture);
      pNode->ContentAdd (pSub);
   }

   //WCHAR szTemp[64];
   //DWORD i;
   //for (i = 0; i < 3; i++) {
   //   swprintf (szTemp, L"LeafColor%d", (int)i);
   //   MMLValueSet (pNode, szTemp, (int)m_acLeaf[i]);
   //} // i

   // set all the autobranch info
   MMLValueSet (pNode, gpszBranchInitialAngle, m_AutoBranch.fBranchInitialAngle); // initial angle. 0..PI. 0 = up
   MMLValueSet (pNode, gpszBranchLength, m_AutoBranch.fBranchLength); // typical length, meters
   MMLValueSet (pNode, gpszBranchLengthVar, m_AutoBranch.fBranchLengthVar); // variation in lenght. 0..1
   MMLValueSet (pNode, gpszBranchShorten, m_AutoBranch.fBranchShorten);   // amount shortens per generation. 0..1
   MMLValueSet (pNode, gpszThickScale, m_fThickScale);
   MMLValueSet (pNode, gpszDarkEdge, m_fDarkEdge);

   // branch extension
   MMLValueSet (pNode, gpszBranchExtendProb, m_AutoBranch.fBranchExtendProb);      // probability that will extend branch, 0..1
   MMLValueSet (pNode, gpszBranchExtendProbGen, m_AutoBranch.fBranchExtendProbGen);   // change in fBranchExtendProb per generation, 0..1
   MMLValueSet (pNode, gpszBranchDirVar, m_AutoBranch.fBranchDirVar);          // variation in direction, 0..1

   // branch forking
   MMLValueSet (pNode, gpszBranchForkProb, m_AutoBranch.fBranchForkProb);        // probability of branch forking, 0..1
   MMLValueSet (pNode, gpszBranchForkProbGen, m_AutoBranch.fBranchForkProbGen);     // chance of forking increasing per generation, 0..1
   MMLValueSet (pNode, gpszBranchForkNum, m_AutoBranch.fBranchForkNum);         // number of forks, 0..1.
   MMLValueSet (pNode, gpszBranchForkForwards, m_AutoBranch.fBranchForkForwards);    // how forwards. 1 = forwards, 0 = backwards
   MMLValueSet (pNode, gpszBranchForkUpDown, m_AutoBranch.fBranchForkUpDown);      // how up/down. 1=up, 0=down
   MMLValueSet (pNode, gpszBranchForkVar, m_AutoBranch.fBranchForkVar);         // variation in forking, 0..1.

   // branch gravity
   MMLValueSet (pNode, gpszBranchGrav, m_AutoBranch.fBranchGrav);            // 1=branchs will want to go upwards, 0=branch go downwards
   MMLValueSet (pNode, gpszBranchGravGen, m_AutoBranch.fBranchGravGen);         // how much fBranchGrav changes each generation. 1 = increase, 0 = decrease

   // leaves along branch
   MMLValueSet (pNode, gpszLeafExtendProb, m_AutoBranch.fLeafExtendProb);        // 0..1, probability of leaf being along extension
   MMLValueSet (pNode, gpszLeafExtendProbGen, m_AutoBranch.fLeafExtendProbGen);     // 0..1, likelihood of leaves increasing
   MMLValueSet (pNode, gpszLeafExtendNum, m_AutoBranch.fLeafExtendNum);         // 0..1, number of leaves
   MMLValueSet (pNode, gpszLeafExtendForwards, m_AutoBranch.fLeafExtendForwards);    // 0..1, 1 = forwards, 0 = backwards
   MMLValueSet (pNode, gpszLeafExtendVar, m_AutoBranch.fLeafExtendVar);         // 0..1, variation in leaf direction
   MMLValueSet (pNode, gpszLeafExtendScale, m_AutoBranch.fLeafExtendScale);       // 0..1, .5 = default scale
   MMLValueSet (pNode, gpszLeafExtendScaleVar, m_AutoBranch.fLeafExtendScaleVar);    // 0..1, amount leaf size varies
   MMLValueSet (pNode, gpszLeafExtendScaleGen, m_AutoBranch.fLeafExtendScaleGen);    // 0..1, how much leaf size changes, .5 = none, 1 = larger

   // leaf at terminal node
   MMLValueSet (pNode, gpszLeafEndProb, m_AutoBranch.fLeafEndProb);           // 0..1, likelihood of leaf at end
   MMLValueSet (pNode, gpszLeafEndNum, m_AutoBranch.fLeafEndNum);            // 0..1, number of leaves at the end
   MMLValueSet (pNode, gpszLeafEndNumVar, m_AutoBranch.fLeafEndNumVar);         // 0..1, variation in the number of leaves at the end
   MMLValueSet (pNode, gpszLeafEndSymmetry, m_AutoBranch.fLeafEndSymmetry);       // 0..1, amount of symmetry in end leaves
   MMLValueSet (pNode, gpszLeafEndVar, m_AutoBranch.fLeafEndVar);            // 0..1, variation in leaf location at end
   MMLValueSet (pNode, gpszLeafEndScale, m_AutoBranch.fLeafEndScale);          // 0..1, .5 = default scale
   MMLValueSet (pNode, gpszLeafEndScaleVar, m_AutoBranch.fLeafEndScaleVar);       // 0..1, amount leaf size varies

   // misc
   // MMLValueSet (pNode, gpszSeed, (int) m_AutoBranch.dwSeed);                 // seed to use
   MMLValueSet (pNode, gpszMaxGen, (int) m_AutoBranch.dwMaxGen);               // maximum generations

   return pNode;
}

BOOL CTextCreatorBranch::MMLFrom (PCMMLNode2 pNode)
{
   m_Material.MMLFrom (pNode);

   m_dwType = (DWORD) MMLValueGetInt (pNode, gpszType, (int)0);
   m_iSeed = (int) MMLValueGetInt (pNode, gpszSeed, (int)0);
   m_iWidth = MMLValueGetInt (pNode, gpszWidth, 512);
   m_iHeight = MMLValueGetInt (pNode, gpszHeight, 256);
   m_fAngleVariation = MMLValueGetDouble (pNode, gpszAngleVariation, 0.5);

   //m_dwTipShape = (DWORD)MMLValueGetInt (pNode, gpszTipShape, 1);
   m_cStemColor = (COLORREF)MMLValueGetInt (pNode, gpszStemColor, RGB(0xff, 0xc0, 0x80));
   m_cBranchColor = (COLORREF)MMLValueGetInt (pNode, gpszBranchColor, RGB(0x40, 0x30, 0x0));
   //m_fTipWidth = MMLValueGetDouble (pNode, gpszTipWidth, 0.2);
   //m_fTipLength = MMLValueGetDouble (pNode, gpszTipLength, 0.5);
   m_fStemLength = MMLValueGetDouble (pNode, gpszStemLength, 0.5);
   m_fLowerDarker = MMLValueGetDouble (pNode, gpszLowerDarker, 0.75);
   m_fThickScale = MMLValueGetDouble (pNode, gpszThickScale, .01);
   m_fDarkEdge = MMLValueGetDouble (pNode, gpszDarkEdge, .5);
   m_fTransBase = MMLValueGetDouble (pNode, gpszTransBase, 0.1);
   m_fDarkenAtBottom = MMLValueGetDouble (pNode, gpszDarkenAtBottom, 1);

   //WCHAR szTemp[64];
   //DWORD i;
   //for (i = 0; i < 3; i++) {
   //   swprintf (szTemp, L"LeafColor%d", (int)i);
   //   m_acLeaf[i] = (COLORREF) MMLValueGetInt (pNode, szTemp, (int)0);
   //} // i

   AutoBranchFill();
   m_AutoBranch.fBranchInitialAngle = MMLValueGetDouble (pNode, gpszBranchInitialAngle, m_AutoBranch.fBranchInitialAngle); // initial angle. 0..PI. 0 = up
   m_AutoBranch.fBranchLength = MMLValueGetDouble (pNode, gpszBranchLength, m_AutoBranch.fBranchLength); // typical length, meters
   m_AutoBranch.fBranchLengthVar = MMLValueGetDouble (pNode, gpszBranchLengthVar, m_AutoBranch.fBranchLengthVar); // variation in lenght. 0..1
   m_AutoBranch.fBranchShorten = MMLValueGetDouble (pNode, gpszBranchShorten, m_AutoBranch.fBranchShorten);   // amount shortens per generation. 0..1

   // branch extension
   m_AutoBranch.fBranchExtendProb = MMLValueGetDouble (pNode, gpszBranchExtendProb, m_AutoBranch.fBranchExtendProb);      // probability that will extend branch, 0..1
   m_AutoBranch.fBranchExtendProbGen = MMLValueGetDouble (pNode, gpszBranchExtendProbGen, m_AutoBranch.fBranchExtendProbGen);   // change in fBranchExtendProb per generation, 0..1
   m_AutoBranch.fBranchDirVar = MMLValueGetDouble (pNode, gpszBranchDirVar, m_AutoBranch.fBranchDirVar);          // variation in direction, 0..1

   // branch forking
   m_AutoBranch.fBranchForkProb = MMLValueGetDouble (pNode, gpszBranchForkProb, m_AutoBranch.fBranchForkProb);        // probability of branch forking, 0..1
   m_AutoBranch.fBranchForkProbGen = MMLValueGetDouble (pNode, gpszBranchForkProbGen, m_AutoBranch.fBranchForkProbGen);     // chance of forking increasing per generation, 0..1
   m_AutoBranch.fBranchForkNum = MMLValueGetDouble (pNode, gpszBranchForkNum, m_AutoBranch.fBranchForkNum);         // number of forks, 0..1.
   m_AutoBranch.fBranchForkForwards = MMLValueGetDouble (pNode, gpszBranchForkForwards, m_AutoBranch.fBranchForkForwards);    // how forwards. 1 = forwards, 0 = backwards
   m_AutoBranch.fBranchForkUpDown = MMLValueGetDouble (pNode, gpszBranchForkUpDown, m_AutoBranch.fBranchForkUpDown);      // how up/down. 1=up, 0=down
   m_AutoBranch.fBranchForkVar = MMLValueGetDouble (pNode, gpszBranchForkVar, m_AutoBranch.fBranchForkVar);         // variation in forking, 0..1.

   // branch gravity
   m_AutoBranch.fBranchGrav = MMLValueGetDouble (pNode, gpszBranchGrav, m_AutoBranch.fBranchGrav);            // 1=branchs will want to go upwards, 0=branch go downwards
   m_AutoBranch.fBranchGravGen = MMLValueGetDouble (pNode, gpszBranchGravGen, m_AutoBranch.fBranchGravGen);         // how much fBranchGrav changes each generation. 1 = increase, 0 = decrease

   // leaves along branch
   m_AutoBranch.fLeafExtendProb = MMLValueGetDouble (pNode, gpszLeafExtendProb, m_AutoBranch.fLeafExtendProb);        // 0..1, probability of leaf being along extension
   m_AutoBranch.fLeafExtendProbGen = MMLValueGetDouble (pNode, gpszLeafExtendProbGen, m_AutoBranch.fLeafExtendProbGen);     // 0..1, likelihood of leaves increasing
   m_AutoBranch.fLeafExtendNum = MMLValueGetDouble (pNode, gpszLeafExtendNum, m_AutoBranch.fLeafExtendNum);         // 0..1, number of leaves
   m_AutoBranch.fLeafExtendForwards= MMLValueGetDouble (pNode, gpszLeafExtendForwards, m_AutoBranch.fLeafExtendForwards);    // 0..1, 1 = forwards, 0 = backwards
   m_AutoBranch.fLeafExtendVar = MMLValueGetDouble (pNode, gpszLeafExtendVar, m_AutoBranch.fLeafExtendVar);         // 0..1, variation in leaf direction
   m_AutoBranch.fLeafExtendScale = MMLValueGetDouble (pNode, gpszLeafExtendScale, m_AutoBranch.fLeafExtendScale);       // 0..1, .5 = default scale
   m_AutoBranch.fLeafExtendScaleVar = MMLValueGetDouble (pNode, gpszLeafExtendScaleVar, m_AutoBranch.fLeafExtendScaleVar);    // 0..1, amount leaf size varies
   m_AutoBranch.fLeafExtendScaleGen = MMLValueGetDouble (pNode, gpszLeafExtendScaleGen, m_AutoBranch.fLeafExtendScaleGen);    // 0..1, how much leaf size changes, .5 = none, 1 = larger

   // leaf at terminal node
   m_AutoBranch.fLeafEndProb = MMLValueGetDouble (pNode, gpszLeafEndProb, m_AutoBranch.fLeafEndProb);           // 0..1, likelihood of leaf at end
   m_AutoBranch.fLeafEndNum = MMLValueGetDouble (pNode, gpszLeafEndNum, m_AutoBranch.fLeafEndNum);            // 0..1, number of leaves at the end
   m_AutoBranch.fLeafEndNumVar = MMLValueGetDouble (pNode, gpszLeafEndNumVar, m_AutoBranch.fLeafEndNumVar);         // 0..1, variation in the number of leaves at the end
   m_AutoBranch.fLeafEndSymmetry = MMLValueGetDouble (pNode, gpszLeafEndSymmetry, m_AutoBranch.fLeafEndSymmetry);       // 0..1, amount of symmetry in end leaves
   m_AutoBranch.fLeafEndVar = MMLValueGetDouble (pNode, gpszLeafEndVar, m_AutoBranch.fLeafEndVar);            // 0..1, variation in leaf location at end
   m_AutoBranch.fLeafEndScale = MMLValueGetDouble (pNode, gpszLeafEndScale, m_AutoBranch.fLeafEndScale);          // 0..1, .5 = default scale
   m_AutoBranch.fLeafEndScaleVar = MMLValueGetDouble (pNode, gpszLeafEndScaleVar, m_AutoBranch.fLeafEndScaleVar);       // 0..1, amount leaf size varies

   // misc
   m_AutoBranch.dwSeed = (DWORD) MMLValueGetInt (pNode, gpszSeed, (int) m_AutoBranch.dwSeed);                 // seed to use
   m_AutoBranch.dwMaxGen = (DWORD) MMLValueGetInt (pNode, gpszMaxGen, (int) m_AutoBranch.dwMaxGen);               // maximum generations

   PCMMLNode2 pSub = NULL;
   PWSTR psz;
   m_Leaf.Clear();
   pNode->ContentEnum (pNode->ContentFind (gpszLeafForTexture), &psz, &pSub);
   if (pSub)
      m_Leaf.MMLFrom (pSub);


   return TRUE;
}

#define NUMDARKEDGE     3

BOOL CTextCreatorBranch::Render (PCImage pImage, PCImage pImage2, PCMaterial pMaterial, PTEXTINFO pTextInfo)
{
   srand (m_iSeed);

   // size
   DWORD    dwX, dwY, dwScale;
   TextureDetailApply (m_dwRenderShard, (DWORD)m_iWidth, (DWORD)m_iHeight, &dwX, &dwY);
   dwScale = max(dwX, dwY);
   fp fScaleCounterDetail = (fp)dwX / (fp)max(m_iWidth,1);


   // determine average color
   //WORD awOrig[3];
   //WORD awAverage[3];
   //memset (awAverage, 0, sizeof(awAverage));
   //DWORD i, j;
   //for (i = 0; i < 3; i++) {
   //   Gamma (m_acLeaf[i], awOrig);
   //   
   //   for (j = 0; j < 3; j++)
   //      awAverage[j] += awOrig[j] / 3;
   //} // i

   // create the surface
   pImage->Init (dwX, dwY, BACKCOLORRGB, 0);

   PCBranchNode pRoot = AutoBranchGen (dwY);
   CListFixed lCMatrix;
   if (!pRoot)
      goto done;

   CalcBranches (pRoot);

   // draw all the branches
   CPoint pOffset;
   pOffset.Zero();
   pOffset.p[0] = (fp)dwX/2;
   pOffset.p[1] = (fp)dwY;

   PBRANCHNOODLE pbn = (PBRANCHNOODLE) m_lBRANCHNOODLE.Get(0);
   CPoint pLoc1, pScale1, pLoc2, pScale2;
   DWORD i, j, k;
   for (i = 0; i < m_lBRANCHNOODLE.Num(); i++, pbn++) {
      PCSpline pSpline = pbn->pNoodle->PathSplineGet ();
      DWORD dwNum = pSpline->OrigNumPointsGet ();

      COLORREF cBranch = m_cBranchColor;

      for (j = 0; j+1 < dwNum; j++) {
         pbn->pNoodle->LocationAndDirection ((fp)j / (fp)(dwNum-1), &pLoc1, NULL, NULL, NULL, &pScale1);
         pbn->pNoodle->LocationAndDirection ((fp)(j+1) / (fp)(dwNum-1), &pLoc2, NULL, NULL, NULL, &pScale2);

#ifdef _DEBUG
         if ((fabs(pLoc1.p[2]) > CLOSE) || (fabs(pLoc1.p[2]) > CLOSE))
            pSpline = NULL;      // just to make sure not venturing into z
#endif

         // offsets
         pLoc1.Add (&pOffset);
         pLoc2.Add (&pOffset);

         fp fCenter = (fp)(NUMDARKEDGE-1) / 2.0;
         for (k = 0; k < NUMDARKEDGE; k++) {
            fp fScaleColor = fCenter ? (1.0 - (fp)k / fCenter) : 1.0;
            fScaleColor = fScaleColor * (m_fDarkEdge - 1.0) + 1.0;

            WORD awColor[3];
            Gamma (cBranch, awColor);
            DWORD dwC;
            fp fC;
            for (dwC = 0; dwC < 3; dwC++) {
               fC = (fp)awColor[dwC] * fScaleColor;
               fC = max(fC, 0);
               fC = min (fC, (fp)0xffff);
               awColor[dwC] = (WORD)fC;
            } // dwC
            COLORREF cDark = UnGamma (awColor);

            fp fScaleThick = 1.0 - (fp)k / (fp)NUMDARKEDGE;

            // draw the line...
            DrawLineSegment (pImage, &pLoc1, &pLoc2,
               pScale1.p[0]*(fp)dwY / 4.0 * fScaleThick,
               pScale2.p[0]*(fp)dwY / 4.0 * fScaleThick,
               cDark, -ZINFINITE);
         } // k
      } // j
   } // i


   // ungamma all the colors
   //WORD awStem[3][3];
   //WORD awColorToUse[3];
   //for (i = 0; i < 3; i++)
   //   Gamma (m_acLeaf[i], awStem[i]);

   // draw the leaves
   lCMatrix.Init (sizeof(CMatrix));
   pRoot->LeafMatricies (&lCMatrix);
   PCMatrix ppm = (PCMatrix) lCMatrix.Get(0);
   CPoint pStart, pDir;
   fp fScale;
   m_Leaf.TextureCache(m_dwRenderShard);
   DWORD dwNum = lCMatrix.Num();
   for (i = 0; i < dwNum; i++, ppm++) {
      // pick a color
      //DWORD dwColor = (DWORD)rand()%3;
      //DWORD dwColorNext = (dwColor+1)%3;
      //DWORD dwWeight = rand()%256;
      //for (j = 0; j < 3; j++)
      //   awColorToUse[j] = (WORD) (((256 - dwWeight) * (DWORD)awStem[dwColor][j] +
      //      dwWeight * (DWORD)awStem[dwColorNext][j]) / 256);

      // figure out start and dir
      pStart.Zero();
      pStart.MultiplyLeft (ppm);
      pDir.Zero();
      pDir.p[1] = 1;
      pDir.MultiplyLeft (ppm);
      pDir.Subtract (&pStart);
      fScale = pDir.Length();
      if (!fScale)
         continue;
      pDir.Normalize();
      pStart.Add (&pOffset);

      // leaf width and length
      //fp fWidth = fScale * m_fTipWidth * (fp)dwY / 10.0;
      //fp fLength = fScale * m_fTipLength * (fp)dwY  / 10.0;

      // draw the stem
      if (m_fStemLength) {
         CPoint pStemEnd;
         pStemEnd.Copy (&pDir);
         pStemEnd.Scale (m_fStemLength * fScale * m_Leaf.m_fHeight * fScaleCounterDetail); // fLength);
         pStemEnd.Add (&pStart);

         DrawLineSegment (pImage, &pStart, &pStemEnd,
            m_Leaf.m_fWidth / 20 * fScaleCounterDetail, m_Leaf.m_fWidth / 20 * fScaleCounterDetail,  // BUGFIX - Was fWidth
            m_cStemColor, -ZINFINITE);

         // new start
         pStart.Copy (&pStemEnd);
      }

      // how bright
      fp fBright = ((fp)i / (fp)dwNum - 0.5) * 2.0 * (m_fLowerDarker - 0.5) * 2.0 + 1.0;

      // else, draw tip
      m_Leaf.Render (pImage, fBright, fScale * fScaleCounterDetail, &pStart, &pDir);
      //DrawGrassTip (pImage, UnGamma(awColorToUse), &pStart, &pDir,
      //   fWidth, fLength, m_dwTipShape);
   } // i
   m_Leaf.TextureRelease();

   delete pRoot;

done:
   // final bits
   memcpy (pMaterial, &m_Material, sizeof(m_Material));
   memset (pTextInfo, 0, sizeof(TEXTINFO));
   pTextInfo->fFloor = FALSE;
   pTextInfo->fPixelLen = 1.0 / (fp) max(dwX, dwY);
   // diable since doing transparent: MassageForCreator (pImage, pMaterial, pTextInfo);

   // darken the grass at the bottom
   ColorDarkenAtBottom (pImage, BACKCOLORRGB, m_fDarkenAtBottom);

   // look for the strange color and revert to gree
   ColorToTransparency (pImage, BACKCOLORRGB, 0, pMaterial);
   pTextInfo->dwMap |= (0x04 | 0x01);  // transparency map and specularity map
   //pip = pImage->Pixel(0, 0);
   //for (i = 0; i < dwX * dwY; i++, pip++)
   //   if ((pip->wRed == 0) && (pip->wGreen == 1) && (pip->wBlue == 2)) {
   //      pip->wRed = awAverage[0];
   //      pip->wGreen = awAverage[1];
   //      pip->wBlue = awAverage[2];
   //      pip->dwIDPart |= 0xff;  // lo byte is transparency
   //      pip->dwID &= 0xffff; // turn off speculatity
   //   }
   //pTextInfo->dwMap |= 0x04;   // transparency map

   // transparency at the base
   PIMAGEPIXEL pip;
   DWORD x, y;
   int iTrans;
   fp fTrans;
   if (m_fTransBase >= 0.01) for (y = 0; y < dwY; y++) {
      fTrans = (fp)(dwY - y) / (fp)dwY;   // so 0 at bottom and 1 at top
      fTrans = 1.0 - fTrans / m_fTransBase * 4.0;
         // The * 4.0 ensures can only be transparent quarter way up
      if (fTrans <= 0)
         continue;   // not transparent
      iTrans = (int)(fTrans * 256.0);

      pip = pImage->Pixel(0, y);
      for (x = 0; x < dwX; x++, pip++) {
         DWORD dwTrans = pip->dwIDPart & 0xff;
         dwTrans = (dwTrans * (256 - (DWORD)iTrans) + (DWORD)iTrans * 255) / 256;
         pip->dwIDPart = (pip->dwIDPart & ~0xff) | dwTrans;
      } // x
   } // y

   return TRUE;
}



/**********************************************************************************
CTextCreatorBranch::AutoBranchFill - If there's no m_pAutoBranch, this allocates the memory
for it and fils it. Otherwise it does nothing
*/
void CTextCreatorBranch::AutoBranchFill (void)
{
   memset (&m_AutoBranch, 0, sizeof(AUTOBRANCH));

   m_AutoBranch.fBranchInitialAngle = 0; // initial angle. 0..PI. 0 = up
   m_AutoBranch.fBranchLength = .2; // typical length, meters
   m_AutoBranch.fBranchLengthVar = .5; // variation in lenght. 0..1
   m_AutoBranch.fBranchShorten = .8;   // amount shortens per generation. 0..1

   // branch extension
   m_AutoBranch.fBranchExtendProb = 1;      // probability that will extend branch, 0..1
   m_AutoBranch.fBranchExtendProbGen = .2;   // change in fBranchExtendProb per generation, 0..1
   m_AutoBranch.fBranchDirVar = .1;          // variation in direction, 0..1

   // branch forking
   m_AutoBranch.fBranchForkProb = 0;        // probability of branch forking, 0..1
   m_AutoBranch.fBranchForkProbGen = .4;     // chance of forking increasing per generation, 0..1
   m_AutoBranch.fBranchForkNum = .1;         // number of forks, 0..1.
   m_AutoBranch.fBranchForkForwards = .7;    // how forwards. 1 = forwards, 0 = backwards
   m_AutoBranch.fBranchForkUpDown = .7;      // how up/down. 1=up, 0=down
   m_AutoBranch.fBranchForkVar = .3;         // variation in forking, 0..1.

   // branch gravity
   m_AutoBranch.fBranchGrav = .6;            // 1=branchs will want to go upwards, 0=branch go downwards
   m_AutoBranch.fBranchGravGen = .4;         // how much fBranchGrav changes each generation. 1 = increase, 0 = decrease

   // leaves along branch
   m_AutoBranch.fLeafExtendProb = 0;        // 0..1, probability of leaf being along extension
   m_AutoBranch.fLeafExtendProbGen = .2;     // 0..1, likelihood of leaves increasing
   m_AutoBranch.fLeafExtendNum = .5;         // 0..1, number of leaves
   m_AutoBranch.fLeafExtendForwards = .6;    // 0..1, 1 = forwards, 0 = backwards
   m_AutoBranch.fLeafExtendVar = .25;         // 0..1, variation in leaf direction
   m_AutoBranch.fLeafExtendScale = .5;       // 0..1, .5 = default scale
   m_AutoBranch.fLeafExtendScaleVar = .2;    // 0..1, amount leaf size varies
   m_AutoBranch.fLeafExtendScaleGen = .5;    // 0..1, how much leaf size changes, .5 = none, 1 = larger

   // leaf at terminal node
   m_AutoBranch.fLeafEndProb = 1;           // 0..1, likelihood of leaf at end
   m_AutoBranch.fLeafEndNum = .2;            // 0..1, number of leaves at the end
   m_AutoBranch.fLeafEndNumVar = .5;         // 0..1, number of leaves at the end
   m_AutoBranch.fLeafEndSymmetry = .3;       // 0..1, amount of symmetry in end leaves
   m_AutoBranch.fLeafEndVar = .25;           // 0..1, variation in leaf location at end
   m_AutoBranch.fLeafEndScale = .5;          // 0..1, .5 = default scale
   m_AutoBranch.fLeafEndScaleVar = .2;       // 0..1, amount leaf size varies

   // misc
   m_AutoBranch.dwSeed = 101;                 // seed to use
   m_AutoBranch.dwMaxGen = 8;               // maximum generations
}


/**********************************************************************************
ApplyGeneration - Given a PAUTOBRANCH, this applies modifications based on the
generation.

inputs
   PAUTOBRANCH    pAB - Branch information. Self modifying
*/
static void ApplyGeneration (PAUTOBRANCH pAB)
{
   fp f;

   pAB->fBranchLength *= pow(pAB->fBranchShorten, (fp).25);
   pAB->fBranchLength = max (pAB->fBranchLength, CLOSE);
   pAB->fBranchExtendProb -= pAB->fBranchExtendProbGen * pAB->fBranchExtendProbGen;
   pAB->fBranchForkProb += pAB->fBranchForkProbGen * pAB->fBranchForkProbGen;
   f = pAB->fBranchGravGen - .5;
   if (f >= 0)
      pAB->fBranchGrav += f * f;
   else
      pAB->fBranchGrav -= f * f;

   pAB->fLeafExtendProb += pAB->fLeafExtendProbGen * pAB->fLeafExtendProbGen;
   pAB->fLeafExtendScale *= (0.5 + pAB->fLeafExtendScaleGen);
}

/**********************************************************************************
CTextCreatorBranch::AutoBranchGenIndiv - Generates an individual branch.

inputs
   PCPoint        pDir - Direction vector. (Normalized)
   DWORD          dwMaxGen - Current maximum generations
   PAUTOBRANCH    pAB - Branch information to use. This is modified on a per-generation bases
returns
   PCBranchNode - New node to add. Returns NULL if error, or if meet dwMaxGen
*/
PCBranchNode CTextCreatorBranch::AutoBranchGenIndiv (PCPoint pDir, DWORD dwMaxGen, PAUTOBRANCH pAB)
{
   if (!dwMaxGen)
      return NULL;

   PCBranchNode pb;
   pb = new CBranchNode;
   if (!pb)
      return NULL;

   //figure out what the next generation will be
   AUTOBRANCH abNext;
   abNext = *pAB;
   ApplyGeneration (&abNext);

   // figure out up vector
   CPoint pOut, pRight, pNewDir;
   CMatrix mToObject, mRot;
   pOut.Zero();
   pOut.p[2] = 1;
   pRight.CrossProd (pDir, &pOut);
   if (pRight.Length() < CLOSE) {
      pOut.Zero();
      pOut.p[0] = 1;  // another directon
      pRight.CrossProd (pDir, &pOut);
   }
   pRight.Normalize();
   mToObject.RotationFromVectors (&pRight, pDir, &pOut);

   // create the forward-pointing branch
   fp f;
   BRANCHINFO bi;
   memset (&bi, 0, sizeof(bi));
   BOOL fBranchExtend;
   fBranchExtend = FALSE;
   if (randf(0,1) < pAB->fBranchExtendProb) {
      mRot.Rotation (
         0, // BUGFIX - Disable randf(-pAB->fBranchDirVar * PI/2, pAB->fBranchDirVar * PI/2),
         0, // no point since just rotates right and left
         randf(-pAB->fBranchDirVar * PI/2, pAB->fBranchDirVar * PI/2)
         );
      mRot.MultiplyRight (&mToObject);
      pNewDir.Zero();
      pNewDir.p[1] = 1;
      pNewDir.MultiplyLeft (&mRot);
      pNewDir.p[1] -= (pAB->fBranchGrav - .5);
         // BUGFIX - Was pNewDir.p[2] += (pAB->fBranchGrav - .5);
      pNewDir.Normalize();

      // create new one
      bi.pLoc.Copy (&pNewDir);
      f = pAB->fBranchLength * randf(1.0 - pAB->fBranchLengthVar,
         1.0 + pAB->fBranchLengthVar);
      f = max(f, CLOSE);
      bi.pLoc.Scale (f);
      bi.pNode = AutoBranchGenIndiv (&pNewDir, dwMaxGen-1, &abNext);
      if (bi.pNode) {
         pb->m_lBRANCHINFO.Add (&bi);
         fBranchExtend = TRUE;  
      }
   }

   // create the split branches
   DWORD i;
   BOOL fBranchSplit;
   fBranchSplit = FALSE;
   if (randf(0,1) < pAB->fBranchForkProb) {
      DWORD dwNum = (randf(0, 1) < pAB->fBranchForkNum) ? 2 : 1;
      for (i = 0; i < dwNum; i++) {
         // which direction relative to current branch? If two then one on
         // either side. If only one then pick a side
         pNewDir.Zero();
         if (dwNum == 2)
            pNewDir.p[0] = i ? 1 : -1;
         else
            pNewDir.p[0] = (rand()%2) ? 1 : -1;

         // rotate forwards and backwards
         f = (pAB->fBranchForkForwards - .5) * PI;
         if (pNewDir.p[0] < 0)
            f *= -1; // if on left side rotate in opposite direction
         mRot.RotationZ (f);
         pNewDir.MultiplyLeft (&mRot);

         // rotate variation
         mRot.Rotation (
            0, // BUGFIX - disable randf(-pAB->fBranchForkVar * PI/2, pAB->fBranchForkVar * PI/2),
            0, // BUGFIX - diable randf(-pAB->fBranchForkVar * PI/2, pAB->fBranchForkVar * PI/2),
            randf(-pAB->fBranchForkVar * PI/2, pAB->fBranchForkVar * PI/2)
            );
         mRot.MultiplyRight (&mToObject);
         pNewDir.MultiplyLeft (&mRot);

         // forking likely to up or down
         pNewDir.p[1] -= (pAB->fBranchForkUpDown - .5);
            // BUGFIX - Was 
         pNewDir.Normalize();

         // create new one
         bi.pLoc.Copy (&pNewDir);
         f = pAB->fBranchLength * randf(1.0 - pAB->fBranchLengthVar,
            1.0 + pAB->fBranchLengthVar);
         f = max(f, CLOSE);
         bi.pLoc.Scale (f);
         bi.pNode = AutoBranchGenIndiv (&pNewDir, dwMaxGen-1, &abNext);
         if (bi.pNode) {
            pb->m_lBRANCHINFO.Add (&bi);
            fBranchSplit = TRUE;
         }
      } // i
   } // if split branches

   // leaves off to the side
   LEAFINFO li;
   CPoint pUp;
   memset (&li, 0, sizeof(li));
   if (fBranchExtend && !fBranchSplit && (randf(0,1) < pAB->fLeafExtendProb)) {
      DWORD dwNum = (randf(0, 1) < pAB->fLeafExtendNum) ? 2 : 1;
      for (i = 0; i < dwNum; i++) {
         // which direction relative to current branch? If two then one on
         // either side. If only one then pick a side
         pNewDir.Zero();
         if (dwNum == 2)
            pNewDir.p[0] = i ? 1 : -1;
         else
            pNewDir.p[0] = (rand()%2) ? 1 : -1;

         // rotate forwards and backwards
         f = (pAB->fLeafExtendForwards - .5) * PI;
         if (pNewDir.p[0] < 0)
            f *= -1; // if on left side rotate in opposite direction
         mRot.RotationZ (f);
         pNewDir.MultiplyLeft (&mRot);

         // rotate variation
         mRot.Rotation (
            0, // BUGFIX - not in 2D randf(-pAB->fLeafExtendVar * PI/2, pAB->fLeafExtendVar * PI/2),
            0, // BUGFIX - not in 2D randf(-pAB->fLeafExtendVar * PI/2, pAB->fLeafExtendVar * PI/2),
            randf(-pAB->fLeafExtendVar * PI/2, pAB->fLeafExtendVar * PI/2)
            );

         // create an up-vector with some rotation
         pUp.Zero();
         pUp.p[2] = 1;
         pUp.MultiplyLeft (&mRot);

         // rotate leaf
         mRot.MultiplyRight (&mToObject);
         pNewDir.MultiplyLeft (&mRot);

         // make sure the directions are fairly perp
         pRight.CrossProd (&pNewDir, &pUp);
         if (pRight.Length() < CLOSE)
            continue;   // cant create this leaf because can't face up
         pRight.Normalize();
         pNewDir.CrossProd (&pUp, &pRight);

         // create new leaf
         //if (!m_dwTipShape)
         if (!m_Leaf.m_fUseText && !m_Leaf.m_dwShape)
            continue;   // no leaves
         li.dwID = 0;   // only one leaf type for now
         f = randf(1.0 - pAB->fLeafExtendScaleVar, 1.0 + pAB->fLeafExtendScaleVar);
         f *= (pAB->fLeafExtendScale * 2.0);
         f = max(f, CLOSE);
         li.fScale = f;
         li.pY.Copy (&pNewDir);
         li.pZ.Copy (&pUp);

         pb->m_lLEAFINFO.Add (&li);
      } // i
   }

   // leaf and ends
   if (!fBranchExtend && !fBranchSplit && (randf(0,1) < pAB->fLeafEndProb)) {
      fp fNum = (pAB->fLeafEndNum * pAB->fLeafEndNum * 50) *
         randf(1.0 - pAB->fLeafEndNumVar, 1.0 + pAB->fLeafEndNumVar);
      DWORD dwNum = (DWORD) fNum + 1;

      // figure out forwards direction
      CPoint   pFront;
      pFront.Zero();
      pFront.p[1] = 1;
      pFront.MultiplyLeft (&mToObject);
      pFront.p[2] = 0; // so leaf level
      if (pFront.Length() < CLOSE)
         pFront.p[0] = 1; // no obvious forwards, so pick a direction
      pFront.Normalize();
      pUp.Zero();
      pUp.p[2] = 1;
      pRight.Zero();
      pRight.CrossProd (&pFront, &pUp);
      pRight.Normalize();

      // make a matrix out of this
      CMatrix mLeaf;
      mLeaf.RotationFromVectors (&pRight, &pFront, &pUp);

      // loop through all the leaves
      for (i = 0; i < dwNum; i++) {
         // what angle
         f = (fp) i / (fp) dwNum;
         f += (randf(-(1.0-pAB->fLeafEndSymmetry), (1.0-pAB->fLeafEndSymmetry)) / (fp)dwNum);
         f *= 2.0 * PI;

         pNewDir.Zero();
         pNewDir.p[1] = cos(f);
         pNewDir.p[0] = sin(f);
         pNewDir.MultiplyLeft (&mLeaf);

         // pick a randomly rotated up vector
         mRot.Rotation (
            0, // BUGFIX - not in 2D randf(-pAB->fLeafEndVar * PI/2, pAB->fLeafEndVar * PI/2),
            0, // BUGFIX - not in 2D randf(-pAB->fLeafEndVar * PI/2, pAB->fLeafEndVar * PI/2),
            randf(-pAB->fLeafEndVar * PI/2, pAB->fLeafEndVar * PI/2)
            );
         pUp.Zero();
         pUp.p[2] = 1;
         pUp.MultiplyLeft (&mRot);

         // make sure the directions are fairly perp
         pRight.CrossProd (&pNewDir, &pUp);
         if (pRight.Length() < CLOSE)
            continue;   // cant create this leaf because can't face up
         pRight.Normalize();
         pNewDir.CrossProd (&pUp, &pRight);
         pNewDir.Normalize();

         // create new leaf
         //if (!m_dwTipShape)
         if (!m_Leaf.m_fUseText && !m_Leaf.m_dwShape)
            continue;   // no leaves
         li.dwID = 0; // only one leaf type for now
         f = randf(1.0 - pAB->fLeafEndScaleVar, 1.0 + pAB->fLeafEndScaleVar);
         f *= (pAB->fLeafEndScale * 2.0);
         f = max(f, CLOSE);
         li.fScale = f;
         li.pY.Copy (&pNewDir);
         li.pZ.Copy (&pUp);

         pb->m_lLEAFINFO.Add (&li);
      }
   }

   return pb;
}


/**********************************************************************************
CTextCreatorBranch::AutoBranchGen - Generates a tree/branch automatically from
the parameters in m_pAutoBranch

inputs
   fp          fFullHeight - Full height of the image, in pixels, so that locations are right
*/
PCBranchNode CTextCreatorBranch::AutoBranchGen (fp fFullHeight)
{
   // seed
   srand (m_iSeed);
   m_AutoBranch.dwMaxGen = max(m_AutoBranch.dwMaxGen, 1);

   // figure out the initial direction
   CPoint pDir;
   pDir.Zero();
   pDir.p[1] = -1;    // always default to up, no m_AutoBranch.fBranchInitialAngle

   // go for it
   fp fTemp = m_AutoBranch.fBranchLength;
   m_AutoBranch.fBranchLength *= fFullHeight/2;
   PCBranchNode pRoot = AutoBranchGenIndiv (&pDir, m_AutoBranch.dwMaxGen + 1, &m_AutoBranch);
   m_AutoBranch.fBranchLength = fTemp;

   return pRoot;
}


/**************************************************************************************
CTextCreatorBranch::CalcBranches - Call this to calculte all the branch information,
control points, and noodles
*/
void CTextCreatorBranch::CalcBranches (PCBranchNode pRoot)
{
   // clear the branch noodle
   PBRANCHNOODLE pbn = (PBRANCHNOODLE) m_lBRANCHNOODLE.Get(0);
   DWORD i;
   for (i = 0; i < m_lBRANCHNOODLE.Num(); i++, pbn++)
      delete pbn->pNoodle;
   m_lBRANCHNOODLE.Clear();

   // calculate the thickness
   pRoot->CalcThickness ();

   // make the base thicker if root
   // if (m_fRoots)
   //   pRoot->m_fThickness *= (1.0 + m_fBaseThick) * (1.0 + m_fBaseThick);
      // using the square of the thickness since calc to thickdist uses the sqert

   // figure out which ones are the roots...
   PBRANCHINFO pbi;
   BOOL m_fRoots = FALSE;
   fp m_fRootThick = 1;
   if (m_fRoots) {
      fp fSumThick = 0;
      pbi = (PBRANCHINFO) pRoot->m_lBRANCHINFO.Get(0);
      for (i = 0; i < pRoot->m_lBRANCHINFO.Num(); i++, pbi++) {
         fp fLen = sqrt(pbi->pLoc.p[0] * pbi->pLoc.p[0] + pbi->pLoc.p[1] * pbi->pLoc.p[1]);
         if (pbi->pLoc.p[2] > fLen / 5.0)
            continue;   // not a root

         pbi->pNode->m_fIsRoot = TRUE;
         fSumThick += pbi->pNode->m_fThickness;
      }

      // scale up all the thicknesses
      if (pRoot->m_fThickness)
         fSumThick = pRoot->m_fThickness / fSumThick * m_fRootThick;

      // scale all the root thicknesses
      pbi = (PBRANCHINFO) pRoot->m_lBRANCHINFO.Get(0);
      for (i = 0; i < pRoot->m_lBRANCHINFO.Num(); i++, pbi++)
         if (pbi->pNode->m_fIsRoot)
            pbi->pNode->ScaleThick (fSumThick);
   }

   // pRoot->SortByThickness ();

   // calculate the location of all the nodes
   CPoint pLoc, pUp;
   pLoc.Zero();
   pUp.Zero();
   pUp.p[0] = .5;
   pUp.p[1] = .5;
   pUp.p[2] = .5;
   pRoot->CalcLoc (&pLoc, &pUp);

   // calulate all the noodle
   CListFixed lLoc, lThick, lUp;
   lLoc.Init (sizeof(CPoint));
   lThick.Init (sizeof(CPoint));
   lUp.Init (sizeof(CPoint));
   fp m_fTapRoot = 0;
   if (m_fRoots) {
      // tap root
      CPoint p;
      p.Zero();
      p.p[2] = -m_fTapRoot;
      lLoc.Add (&p);
      lThick.Add (&p);
      lUp.Add (&p);
   }
   pRoot->FillNoodle (m_fThickScale, &lLoc, &lUp, &lThick, &m_lBRANCHNOODLE,
      0 /*m_dwDivide*/, FALSE/*m_fRound*/, 0 /*m_dwTextureWrap*/, FALSE /*m_fCap*/);
   if (m_fRoots) {
      // refix tap root
      PCPoint p = (PCPoint) lUp.Get(0);
      if (lUp.Num() >= 2)
         p[0].Copy (&p[1]);   // so keep same direction
      p = (PCPoint) lThick.Get(0);
      if (lThick.Num() >= 2)
         p[0].Copy (&p[1]);   // so keep same thickness
   }
   BranchAddNoodle (&lLoc, &lUp, &lThick, &m_lBRANCHNOODLE,
      0/*m_dwDivide*/, FALSE/*m_fRound*/, 0/*m_dwTextureWrap*/, FALSE/*m_fCap*/);

   // figure out where cp go
   // m_lBRANCHCP.Clear();
   //  pRoot->EnumCP (&m_lBRANCHCP, NULL, m_pCurModify, m_fCurModifyLeaf, m_pCurModifyIndex,
   //    &m_lLEAFTYPE);

#if 0 // dont need bounding box
   // calculate the bounding box so can draw small version
   CListFixed lList;
   lList.Init (sizeof(PCListFixed));
   DWORD j;
   for (i = 0; i < m_lLEAFTYPE.Num(); i++) {
      PCListFixed pl = new CListFixed;
      pl->Init (sizeof(CMatrix));
      lList.Add (&pl);
   }
   PCListFixed *ppl;
   ppl = (PCListFixed*) lList.Get(0);
   pRoot->LeafMatricies (ppl);

   CPoint pMin, pMax, p, ap[2];
   CMatrix mBound;
   BOOL fFirst;
   fFirst = TRUE;
   PLEAFTYPE plt;
   plt = (PLEAFTYPE) m_lLEAFTYPE.Get(0);
   DWORD x,y,z;
   for (i = 0; i < lList.Num(); i++, plt++) {
      PCListFixed pl = ppl[i];
      PCMatrix pm = (PCMatrix)pl->Get(0);
      if (!plt->pClone) {
         delete pl;
         continue;
      }

      plt->pClone->BoundingBoxGet (&mBound, &ap[0], &ap[1]);

      for (j = 0; j < pl->Num(); j++, pm++) {
         pm->MultiplyLeft (&mBound);

         // convret corner of point to bounding box so can get min and max
         for (x = 0; x < 2; x++) for (y = 0; y < 2; y++) for (z = 0; z < 2; z++) {
            p.p[0] = ap[x].p[0];
            p.p[1] = ap[y].p[1];
            p.p[2] = ap[z].p[2];
            p.p[3] = 1;
            p.MultiplyLeft (pm);

            if (fFirst) {
               pMin.Copy (&p);
               pMax.Copy (&p);
               fFirst = FALSE;
            }
            else {
               pMin.Min (&p);
               pMax.Max (&p);
            }
         } // xyz
      } // j

      // delete it
      delete pl;
   } // i

   // if didn't have any leaves then set min and max to 0
   if (fFirst) {
      pMin.Zero();
      pMax.Zero();
   }

   // fill the revolustion to the far away shape
   CPoint pBottom, pLeft, pScale;
   pBottom.Average (&pMin, &pMax);
   pBottom.p[2] = pMin.p[2];
   pUp.Zero();
   pUp.p[2] = 1;
   pLeft.Zero();
   pLeft.p[0] = -1;
   pScale.Subtract (&pMax, &pMin);
   // m_fLowDetail = pScale.Length();
   m_pRevFarAway->BackfaceCullSet (TRUE);
   m_pRevFarAway->DirectionSet (&pBottom, &pUp, &pLeft);
   m_pRevFarAway->ProfileSet (RPROF_CIRCLE, 0);
   m_pRevFarAway->RevolutionSet (RREV_CIRCLE, 0);
   m_pRevFarAway->ScaleSet (&pScale);

   // loop through all the branches and include the longest in the detail
   // BUGFIX - Make this 1/2 longest
   //pbn = (PBRANCHNOODLE) m_lBRANCHNOODLE.Get(0);
   //for (i = 0; i < m_lBRANCHNOODLE.Num(); i++, pbn++)
   //   m_fLowDetail = max(m_fLowDetail, pbn->fLength / 2.0);
#endif // 0, dont need bounding box
}




/***********************************************************************************
CTextCreatorBranch::TextureQuery - Adds this object's textures (and it's sub-textures)
if it's not already on the list.

inputs
   PCListFixed       plText - List of 2-GUIDs (major & minor) for the textures
                     that are already known. If the texture is already on here,
                     this returns. If not, the texture is added, and sub-textures
                     are also added.
   PCBTree           pTree - Also added to
   GUID              *pagThis - Pointer to an array of 2 guids. pagThis[0] = code, pagThis[1] = sub
returns
   BOOL - TRUE if success
*/
BOOL CTextCreatorBranch::TextureQuery (PCListFixed plText, PCBTree pTree, GUID *pagThis)
{
   WCHAR szTemp[sizeof(GUID)*4+2];
   MMLBinaryToString ((PBYTE)pagThis, sizeof(GUID)*2, szTemp);
   if (pTree->Find (szTemp))
      return TRUE;

   
   // add itself
   plText->Add (pagThis);
   pTree->Add (szTemp, NULL, 0);

   // NEW CODE: go through all sub-textures
   m_Leaf.TextureQuery (m_dwRenderShard, plText, pTree);

   return TRUE;
}


/***********************************************************************************
CTextCreatorBranch::SubTextureNoRecurse - Adds this object's textures (and it's sub-textures)
if it's not already on the list.

inputs
   PCListFixed       plText - List of 2-GUIDs (major & minor) for the textures
                     that are already known. If not, the texture is TEMPORARILY added, and sub-textures
                     are also TEMPORARILY added.
   GUID              *pagThis - Pointer to an array of 2 guids. pagThis[0] = code, pagThis[1] = sub
returns
   BOOL - TRUE if success. FALSE if they recurse
*/
BOOL CTextCreatorBranch::SubTextureNoRecurse (PCListFixed plText, GUID *pagThis)
{
   GUID *pag = (GUID*)plText->Get(0);
   DWORD i;
   for (i = 0; i < plText->Num(); i++, pag += 2)
      if (!memcmp (pag, pagThis, sizeof(GUID)*2))
         return FALSE;  // found itself

   // remember count
   DWORD dwNum = plText->Num();

   // add itself
   plText->Add (pagThis);

   // loop through all sub-texts
   BOOL fRet = TRUE;
   fRet = m_Leaf.SubTextureNoRecurse (m_dwRenderShard, plText);
   if (!fRet)
      goto done;

done:
   // restore list
   plText->Truncate (dwNum);

   return fRet;
}
