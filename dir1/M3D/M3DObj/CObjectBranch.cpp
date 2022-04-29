/************************************************************************
CObjectBranch.cpp - Draws a Branch.

begun 14/9/01 by Mike Rozak
Copyright 2001 Mike Rozak. All rights reserved
*/

#include <windows.h>
#include "escarpment.h"
#include "..\M3D.h"
#include "resource.h"



typedef struct {
   PWSTR       pszName;    // name of the move point
   int         iX, iY; // -1 if for left/top, 1 for right/bottom
} SPLINEMOVEP, *PSPLINEMOVEP;

static SPLINEMOVEP   gaBranchMove[] = {
   L"Attach", 0, 0
};


/* CBranchNode - For storing branch information */
class CBranchNode;
typedef CBranchNode *PCBranchNode;
typedef struct {
   CPoint            pLoc;       // offset location from the start of the current branch
   PCBranchNode      pNode;      // new node
} BRANCHINFO, *PBRANCHINFO;

typedef struct {
   CPoint            pX;         // vector pointing in x=1 direction for leaf (normalized)
   CPoint            pY;         // vector pointing in y=1 direction for leaf (from the node). Normalized
   CPoint            pZ;         // normalized Up (z=1) vector
   CMatrix           mLeafToObject; // converts from leaf space to object space, includes translation, rotation, and scale
   fp                fScale;     // amount to scale. 1.0 = default size
   DWORD             dwID;       // leaf ID - index into list of leaves supported
} LEAFINFO, *PLEAFINFO;

// BRANCHCP - Maintain list of CP supported by branch
typedef struct {
   PCBranchNode      pBranch;    // branch that it's in
   BOOL              fLeaf;      // if true, it's a leaf index, FALSE it's a sub-branch
   DWORD             dwIndex;    // index into the leaf or sub-branch
   DWORD             dwAttrib;   // attribute...
                                 // 0 for CP to select the object as one being modified
                                 // 1 for CP to move end
                                 // 2 for CP to split
                                 // 3 for CP to delete
                                 // 4 for CP to add new node (dwIndex not used)
                                 // 5 for CP to add new leaf (dwIndex not used)
                                 // 6 for rotate and scale leaf
                                 // 7 for definition of up on leaf
                                 // 8 to change leaf type
   CPoint            pLoc;       // location of the control point
   fp                fSize;      // size of the control point
} BRANCHCP, *PBRANCHCP;

class CBranchNode {
public:
   ESCNEWDELETE;

   CBranchNode (void);
   ~CBranchNode (void);

   CBranchNode *Clone (void);
   PCMMLNode2 MMLTo (void);
   BOOL MMLFrom (PCMMLNode2 pNode);

   fp CalcThickness (PCListFixed plLEAFTYPE);
   void ScaleThick (fp fScale);
   void CalcLoc (PCPoint pLoc, PCPoint pUp);
   //void SortByThickness (void);
   void FillNoodle (fp fThickScale, PCListFixed plLoc, PCListFixed plUp, PCListFixed plThick,
      PCListFixed plBRANCHNOODLE, DWORD dwDivide, BOOL fRound, DWORD dwTextureWrap, BOOL fCap);
   void EnumCP (PCListFixed plBRANCHCP, PCBranchNode pParent,
      PCBranchNode pCurModify, BOOL fCurModifyLeaf, PVOID pCurModifyIndex,
      PCListFixed plLEAFTYPE);
   void RemoveLeaf (DWORD dwID);
   void LeafMatricies (PCListFixed *paplCMatrix);

//private:
   CListFixed           m_lBRANCHINFO;    // list of branches
   CListFixed           m_lLEAFINFO;      // list of leaves

   // calculate
   fp                   m_fThickness;     // how thick it is
   fp                   m_fThickDist;     // thickness in meters
   CPoint               m_pLoc;           // location
   CPoint               m_pUp;            // up vector, so noodle has proper up vector
   BOOL                 m_fIsRoot;        // set to TRUE if it's shte start of a root
};


/**********************************************************************************
LEAFTYPEFill - Given a leaf type with an object ID, this gets the clone (which must
later be released) and fills in some size information

inputs
   PLEAFTYPE      plt - Leaf
*/
void LEAFTYPEFill (DWORD dwRenderShard, PLEAFTYPE plt)
{
   plt->pClone = ObjectCloneGet (dwRenderShard, &plt->gCode, &plt->gSub, TRUE);
   if (!plt->pClone)
      return;

   // Fill in other leaf info
   CMatrix m;
   CPoint   pc[2];
   plt->pClone->BoundingBoxGet (&m, &pc[0], &pc[1]);
   // NOTE: Assuming that m is identity
   DWORD i, j;
   plt->fDimLength = -1;
   for (i = 0; i < 2; i++) for (j = 0; j < 2; j++) {
      if (fabs(pc[i].p[j]) <= plt->fDimLength)
         continue;   // not best match

      // else, found longest side
      plt->fDimLength = max(fabs(pc[i].p[j]), CLOSE);
      plt->dwDim = j;
      plt->fNegative = (pc[i].p[j] < 0);
   }
}

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

static PWSTR gpszBranchNode = L"BranchNode";
static PWSTR gpszLeaf = L"Leaf";
static PWSTR gpszY = L"Y";
static PWSTR gpszZ = L"Z";
static PWSTR gpszScale = L"Scale";
static PWSTR gpszID = L"ID";
static PWSTR gpszLoc = L"Loc";

/**********************************************************************************
CBranchNode::MMLTo - Standard MMLTo
*/
PCMMLNode2 CBranchNode::MMLTo (void)
{
   PCMMLNode2 pNode = new CMMLNode2;
   if (!pNode)
      return NULL;
   pNode->NameSet (gpszBranchNode);

   PCMMLNode2 pSub;
   DWORD i;
   PBRANCHINFO pbi = (PBRANCHINFO) m_lBRANCHINFO.Get(0);
   PLEAFINFO pli = (PLEAFINFO) m_lLEAFINFO.Get(0);
   for (i = 0; i < m_lLEAFINFO.Num(); i++, pli++) {
      pSub = pNode->ContentAddNewNode ();
      if (!pSub)
         continue;
      pSub->NameSet (gpszLeaf);
      MMLValueSet (pSub, gpszY, &pli->pY);
      MMLValueSet (pSub, gpszZ, &pli->pZ);
      MMLValueSet (pSub, gpszScale, pli->fScale);
      MMLValueSet (pSub, gpszID, (int)pli->dwID);
   }

   for (i = 0; i < m_lBRANCHINFO.Num(); i++, pbi++) {
      pSub = pbi->pNode->MMLTo();
      if (!pSub)
         continue;
      MMLValueSet (pSub, gpszLoc, &pbi->pLoc);
      pNode->ContentAdd (pSub);
   }
   return pNode;
}

/**********************************************************************************
CBranchNode::MMLFrom - Standard */
BOOL CBranchNode::MMLFrom (PCMMLNode2 pNode)
{
   // clear out bits
   DWORD i;
   PBRANCHINFO pbi = (PBRANCHINFO) m_lBRANCHINFO.Get(0);
   for (i = 0; i < m_lBRANCHINFO.Num(); i++)
      delete pbi[i].pNode;
   m_lBRANCHINFO.Clear();
   m_lLEAFINFO.Clear();

   // look through nodes
   PWSTR psz;
   PCMMLNode2 pSub;
   for (i = 0; i < pNode->ContentNum(); i++) {
      pSub = NULL;
      pNode->ContentEnum (i, &psz, &pSub);
      if (!pSub)
         continue;
      psz = pSub->NameGet();
      if (!psz)
         continue;

      if (!_wcsicmp(psz, gpszBranchNode)) {
         BRANCHINFO bi;
         memset (&bi, 0, sizeof(bi));
         MMLValueGetPoint (pSub, gpszLoc, &bi.pLoc);
         bi.pNode = new CBranchNode;
         if (!bi.pNode)
            continue;
         if (!bi.pNode->MMLFrom (pSub)) {
            delete bi.pNode;
            continue;
         }
         m_lBRANCHINFO.Add (&bi);
         continue;
      }
      else if (!_wcsicmp(psz, gpszLeaf)) {
         LEAFINFO li;
         memset (&li, 0, sizeof(li));
         MMLValueGetPoint (pSub, gpszY, &li.pY);
         MMLValueGetPoint (pSub, gpszZ, &li.pZ);
         li.fScale = MMLValueGetDouble (pSub, gpszScale, 1);
         li.dwID = (DWORD) MMLValueGetInt (pSub, gpszID, (int)0);
         m_lLEAFINFO.Add (&li);
         continue;
      }
   }

   return TRUE;
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


#if 0 // DEAD code
/**********************************************************************************
CBranchNode::SortByThickness - Sorts all of the BRANCHINFO structures by the
thickness of the branch they point to. Thickest are first in the list
*/
static int _cdecl BRANCHINFOSort (const void *elem1, const void *elem2)
{
   BRANCHINFO *pdw1, *pdw2;
   pdw1 = (BRANCHINFO*) elem1;
   pdw2 = (BRANCHINFO*) elem2;

   // if it's the start of a root and the other isn't then put roots last
   if (pdw1->pNode->m_fIsRoot && !pdw2->pNode->m_fIsRoot)
      return 1;
   else if (!pdw1->pNode->m_fIsRoot && pdw2->pNode->m_fIsRoot)
      return -1;

   if (pdw1->pNode->m_fThickness < pdw2->pNode->m_fThickness)
      return 1;
   else if (pdw1->pNode->m_fThickness > pdw2->pNode->m_fThickness)
      return -1;
   else
      return (int)pdw1 - (int)pdw2; // so have some decision
}

void CBranchNode::SortByThickness (void)
{
   // sort
   qsort (m_lBRANCHINFO.Get(0), m_lBRANCHINFO.Num(), sizeof(BRANCHINFO), BRANCHINFOSort);

   // loop through the sub branches
   DWORD i;
   PBRANCHINFO pbi = (PBRANCHINFO) m_lBRANCHINFO.Get(0);
   for (i = 0; i < m_lBRANCHINFO.Num(); i++, pbi++)
      pbi->pNode->SortByThickness();
}
#endif // 0 - dead code

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
CBranchNode::EnumCP - Enumerates control points supported by the branch into the
plBRANCHCP list.

inputs
   PCListFixed       plBRANCHCP - Filled with control points that should be supported
   PCBranchNode      pParent - Parent of the current branch so can move either end if select
   PCBranchNode      pCurModify - Current branch that is selected for modification
   BOOL              fCurModifyLeaf - Flag indicating if modifying leaf or sub-branch on current branch
   PVOID             pCurModifyIndex - Index of currently modified branch if leaf, else pointer to
                        next node if branch
   PCListFixed       plLEAFTYPE - Leaf type info... determines if can add a new type of leaf
*/
void CBranchNode::EnumCP (PCListFixed plBRANCHCP, PCBranchNode pParent,
                          PCBranchNode pCurModify, BOOL fCurModifyLeaf, PVOID pCurModifyIndex,
                          PCListFixed plLEAFTYPE)
{
#define CPDIST       1.0
   // do sub-branches
   DWORD i, j;
   PBRANCHINFO pbi = (PBRANCHINFO) m_lBRANCHINFO.Get(0);
   BRANCHCP bc;
   memset (&bc, 0, sizeof(bc));
   bc.pBranch = this;
   //bc.pParent = pParent;
   for (i = 0; i < m_lBRANCHINFO.Num(); i++, pbi++) {
      pbi->pNode->EnumCP (plBRANCHCP, this, pCurModify, fCurModifyLeaf, pCurModifyIndex,
         plLEAFTYPE);

      bc.dwIndex = i;
      bc.fLeaf = FALSE;

      // add CP to select object
      if ((pCurModify != this) || (fCurModifyLeaf != FALSE) || (pCurModifyIndex != (PVOID) pbi->pNode)) {
         bc.dwAttrib = 0;  // select this object
         bc.fSize = (m_fThickDist + pbi->pNode->m_fThickDist) / 2;
         bc.pLoc.Average (&m_pLoc, &pbi->pNode->m_pLoc);
         plBRANCHCP->Add (&bc);
         continue;
      }

      // else, object selected
      // CP to move end
      bc.dwAttrib = 1;
      bc.fSize = pbi->pNode->m_fThickDist;
      bc.pLoc.Copy (&pbi->pNode->m_pLoc);
      plBRANCHCP->Add (&bc);

      // CP to split
      bc.dwAttrib = 2;
      bc.fSize = (m_fThickDist + pbi->pNode->m_fThickDist) / 2;
      bc.pLoc.Average (&m_pLoc, &pbi->pNode->m_pLoc);
      plBRANCHCP->Add (&bc);

      // CP to delete
      if (pParent || (m_lBRANCHINFO.Num() >= 2) ) {
         bc.dwAttrib = 3;
         bc.fSize = m_fThickDist *.25 + pbi->pNode->m_fThickDist * .75;
         bc.pLoc.Average (&m_pLoc, &pbi->pNode->m_pLoc, .25);
         plBRANCHCP->Add (&bc);
      }

      // add a new node
      bc.dwAttrib = 4;
      bc.fLeaf = FALSE;
      bc.fSize = pbi->pNode->m_fThickDist;
      bc.pLoc.Copy (&pbi->pNode->m_pLoc);
      bc.pLoc.p[0] += pbi->pNode->m_fThickDist * CPDIST;
      bc.pBranch = pbi->pNode;
      plBRANCHCP->Add (&bc);
      bc.pBranch = this;   // to restore

      // add a new leaf
      if (plLEAFTYPE->Num()) {
         bc.dwAttrib = 5;
         bc.fLeaf = FALSE;
         bc.fSize = pbi->pNode->m_fThickDist;
         bc.pLoc.Copy (&pbi->pNode->m_pLoc);
         bc.pLoc.p[0] -= pbi->pNode->m_fThickDist * CPDIST;
         bc.pBranch = pbi->pNode;
         plBRANCHCP->Add (&bc);
         bc.pBranch = this;   // to restore
      }
   } // i - over branchinfo

   // CP for leaf
   PLEAFINFO pli;
   PLEAFTYPE plt, pCur;
   plt = (PLEAFTYPE) plLEAFTYPE->Get(0);
   pli = (PLEAFINFO) m_lLEAFINFO.Get(0);
   CPoint pLocTip, pLocUp;
   for (i = 0; i < m_lLEAFINFO.Num(); i++, pli++) {
      pCur = &plt[pli->dwID];
      bc.dwIndex = i;
      bc.fLeaf = TRUE;
      bc.fSize = pCur->fDimLength * pli->fScale / 10;
      pLocTip.Copy (pCur->dwDim ? &pli->pY : &pli->pX);
      pLocTip.Scale (pCur->fDimLength * pli->fScale * (pCur->fNegative ? -1 : 1));
      pLocTip.Add (&m_pLoc);
      pLocUp.Copy (&pli->pZ);
      pLocUp.Scale (pCur->fDimLength * pli->fScale);
      pLocUp.Add (&m_pLoc);

      // add CP to select object
      if ((pCurModify != this) || (fCurModifyLeaf != TRUE) || (pCurModifyIndex != (PVOID)(size_t) i)) {
         bc.dwAttrib = 0;  // select this object
         bc.pLoc.Average (&m_pLoc, &pLocTip);
         plBRANCHCP->Add (&bc);
         continue;
      }

      // allow to be rotated and scaled
      bc.dwAttrib = 6;
      bc.pLoc.Copy (&pLocTip);
      plBRANCHCP->Add (&bc);

      // up
      bc.dwAttrib = 7;
      bc.pLoc.Copy (&pLocUp);
      plBRANCHCP->Add (&bc);

      // delete
      bc.dwAttrib = 3;
      bc.pLoc.Average (&m_pLoc, &pLocTip, .25);
      plBRANCHCP->Add (&bc);

      // change to new type
      if (plLEAFTYPE->Num() >= 2) {
         bc.dwAttrib = 8;
         bc.pLoc.Average (&m_pLoc, &pLocTip, .75);
         plBRANCHCP->Add (&bc);
      }
   }


   // can move this one
   if (pParent && (pCurModify == this)) {
      PBRANCHINFO pbi2 = (PBRANCHINFO) pParent->m_lBRANCHINFO.Get(0);
      for (j = 0; j < pParent->m_lBRANCHINFO.Num(); j++, pbi2++)
         if (pbi2->pNode == this) {
            BRANCHCP bc2;
            //bc2.pParent = pParent;
            bc2.dwAttrib = 1;
            bc2.dwIndex = j;
            bc2.fLeaf = FALSE;
            bc2.fSize = m_fThickDist;
            bc2.pBranch = pParent;
            bc2.pLoc.Copy (&m_pLoc);
            plBRANCHCP->Add (&bc2);
         } // j
   }  // if have parent

   // CP to add new node
   //if ((pCurModify == this) || (pParent && (pCurModify == pParent)) ) {
   if (pCurModify == this) {
      bc.dwAttrib = 4;
      bc.fLeaf = FALSE;
      bc.fSize = m_fThickDist;
      bc.pLoc.Copy (&m_pLoc);
      bc.pLoc.p[0] += m_fThickDist * CPDIST;
      plBRANCHCP->Add (&bc);

      // cp to adda  new leaf
      if (plLEAFTYPE->Num()) {
         bc.dwAttrib = 5;
         bc.pLoc.Copy (&m_pLoc);
         bc.pLoc.p[0] -= m_fThickDist * CPDIST;
         plBRANCHCP->Add (&bc);
      }
   }

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

/**********************************************************************************
CBranchNode::LeafMatricies - Fills in CListFixed objects with matricies that
indicate where the leaf objects are to be drawn. These can then be passed into
the clone render functions.

inputs
   PCListFixed    paplCMatrix - Pointer to an array of PCListFixed. The size of the
                  array is the number of leaftype's. Each PCListFixed should be
                  initialized with sizeof(CMatrix). They will have all the leaf
                  matricies added
returns
   none
*/
void CBranchNode::LeafMatricies (PCListFixed *paplCMatrix)
{
   // loop through the sub branches
   DWORD i;
   PBRANCHINFO pbi = (PBRANCHINFO) m_lBRANCHINFO.Get(0);
   for (i = 0; i < m_lBRANCHINFO.Num(); i++, pbi++)
      pbi->pNode->LeafMatricies(paplCMatrix);

   PLEAFINFO pli = (PLEAFINFO) m_lLEAFINFO.Get(0);
   // BUGFIX - Don't use paplCMatrix[pli->dwID]->Required (m_lLEAFINFO.Num());
   for (i = 0; i < m_lLEAFINFO.Num(); i++, pli++)
      paplCMatrix[pli->dwID]->Add (&pli->mLeafToObject);
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

inputs
   PCListFixed    plLEAFTYPE - Pointer to a list of leaf inforamtion so can tell
                     how much each leaf adds to the weight
*/
fp CBranchNode::CalcThickness (PCListFixed plLEAFTYPE)
{
   fp fThick = 1; // always start at 1 thickness for the branch

   m_fIsRoot = FALSE;   // assume it's not a root

   // loop through the sub branches
   DWORD i;
   PBRANCHINFO pbi = (PBRANCHINFO) m_lBRANCHINFO.Get(0);
   for (i = 0; i < m_lBRANCHINFO.Num(); i++, pbi++)
      fThick += pbi->pNode->CalcThickness(plLEAFTYPE);


   PLEAFINFO pli = (PLEAFINFO) m_lLEAFINFO.Get(0);
   PLEAFTYPE plt = (PLEAFTYPE) plLEAFTYPE->Get(0);
   for (i = 0; i < m_lLEAFINFO.Num(); i++, pli++)
      fThick += plt[pli->dwID].fWeight;

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


/**********************************************************************************
CObjectBranch::Constructor and destructor */
CObjectBranch::CObjectBranch (PVOID pParams, POSINFO pInfo)
{
   m_dwRenderShow = RENDERSHOW_MISC;
   m_OSINFO = *pInfo;

   m_fThickScale = .01;
   m_dwDivide = 2;
   m_dwTextureWrap = 0;
   m_fRound = FALSE;
   m_fDispLowDetail = FALSE;
   m_fCap = FALSE;
   m_pRevFarAway = new CRevolution;
   m_fLowDetail = 0;
   m_fRoots = FALSE;
   m_fBaseThick = .5;
   m_fRootThick = .25;
   m_fTapRoot = .5;
   m_pAutoBranch = NULL;

   m_pRoot = new CBranchNode;
   BRANCHINFO bi;
   memset (&bi, 0, sizeof(bi));
   bi.pLoc.p[2] = 1;
   bi.pNode = new CBranchNode;
   m_pRoot->m_lBRANCHINFO.Add (&bi);

   m_pCurModify = m_pRoot;
   m_fCurModifyLeaf = FALSE;
   m_pCurModifyIndex = (PVOID) bi.pNode;

   m_lBRANCHNOODLE.Init (sizeof(BRANCHNOODLE));
   m_lLEAFTYPE.Init (sizeof(LEAFTYPE));
   m_lBRANCHCP.Init (sizeof(BRANCHCP));

   m_fDirty = TRUE;

   m_lRenderListBack.Init (sizeof(PCListFixed));

   // color for the Branch
   ObjectSurfaceAdd (42, RGB(0x80,0x80,0x0), MATERIAL_FLAT);   // color of trunk
   ObjectSurfaceAdd (41, RGB(0x20,0xc0,0x30), MATERIAL_FLAT);  // color of leaves far away
}


CObjectBranch::~CObjectBranch (void)
{
   if (m_pRoot)
      delete m_pRoot;
   if (m_pRevFarAway)
      delete m_pRevFarAway;
   if (m_pAutoBranch)
      delete m_pAutoBranch;

   // clear the branch noodle
   DWORD i;
   PBRANCHNOODLE pbn = (PBRANCHNOODLE) m_lBRANCHNOODLE.Get(0);
   for (i = 0; i < m_lBRANCHNOODLE.Num(); i++, pbn++)
      delete pbn->pNoodle;
   m_lBRANCHNOODLE.Clear();

   // free away the leaf types
   PLEAFTYPE plt;
   plt = (PLEAFTYPE) m_lLEAFTYPE.Get(0);
   for (i = 0; i < m_lLEAFTYPE.Num(); i++, plt++)
      if (plt->pClone)
         plt->pClone->Release();
   m_lLEAFTYPE.Clear();

   PCListFixed *ppl;
   ppl = (PCListFixed*) m_lRenderListBack.Get(0);
   for (i = 0; i < m_lRenderListBack.Num(); i++)
      if (ppl[i])
         delete ppl[i];
}


/**********************************************************************************
CObjectBranch::Delete - Called to delete this object
*/
void CObjectBranch::Delete (void)
{
   delete this;
}

/**********************************************************************************
CObjectBranch::Render - Draws a test box.

inputs
   POBJECTRENDER     pr - Render information
*/
void CObjectBranch::Render (POBJECTRENDER pr, DWORD dwSubObject)
{
	MALLOCOPT_INIT;
   CalcBranches();

   // BUGFIX - Multiply QueryDetail() * 2.0 so that trees stop drawing leaves
   // a little bit sooner.
   // BUGFIX - Disable (fDetail * 3.0 >= m_fLowDetail) so don't draw balloon branches
   fp fDetail = pr->pRS->QueryDetail();
   if ((pr->dwReason == ORREASON_BOUNDINGBOX) || m_fDispLowDetail /* || (fDetail * 3.0 >= m_fLowDetail) */ ) {
      // far away, so drawing low detail
      // CRenderSurface rs;
      m_Renderrs.ClearAll();
      CMatrix mObject;
      m_Renderrs.Init (pr->pRS);
      ObjectMatrixGet (&mObject);
      m_Renderrs.Multiply (&mObject);

      // object specific
      m_Renderrs.SetDefMaterial (m_OSINFO.dwRenderShard, ObjectSurfaceFind (41), m_pWorld);

      // if drawing for bounding box then just draw a cube
      if (pr->dwReason == ORREASON_BOUNDINGBOX) {
         // note: don't return
         CPoint pScale, pBottom, pAround, pLeft, pMin, pMax;
         m_pRevFarAway->ScaleGet (&pScale);
         m_pRevFarAway->DirectionGet (&pBottom, &pAround, &pLeft);
         pMin.Zero();
         pMin.Copy (&pBottom);
         pMin.p[0] -= pScale.p[0] / 2.0;
         pMin.p[1] -= pScale.p[1] / 2.0;
         pMax.Add (&pMin, &pScale);
         m_Renderrs.ShapeBox (pMin.p[0], pMin.p[1], pMin.p[2],
            pMax.p[0], pMax.p[1], pMax.p[2]);
      }
      else {
         m_pRevFarAway->Render (pr, &m_Renderrs);
         m_Renderrs.Commit();
         return;
      }
      m_Renderrs.Commit();
   }

   // draw the branches
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

      PBRANCHNOODLE pbn = (PBRANCHNOODLE) m_lBRANCHNOODLE.Get(0);
      DWORD i;
      for (i = 0; i < m_lBRANCHNOODLE.Num(); i++, pbn++) {
         if (pbn->fLength < m_Renderrs.m_fDetail * 3.0)
            continue;   // too small
                  // BUGFIX - Branches aren't drawn based on m_fDetail*2 instead of m_fDetail*1
                  // since not as important

         pbn->pNoodle->Render (pr, &m_Renderrs);
      }
      m_Renderrs.Commit();
   }

   // if bounding box then return before draw leaves
   //if (pr->dwReason == ORREASON_BOUNDINGBOX)
   //   return;

   // draw the leaves
   // CListFixed lList;
	MALLOCOPT_OKTOMALLOC;
   m_lRenderList.Init (sizeof(PCListFixed));
	MALLOCOPT_RESTORE;
   DWORD i,j;
   PCListFixed *ppl;
   m_lRenderList.Required (m_lLEAFTYPE.Num());
   ppl = (PCListFixed*) m_lRenderListBack.Get(0);
   for (i = 0; i < m_lLEAFTYPE.Num(); i++) {
      PCListFixed pl;
      if (i < m_lRenderListBack.Num())
         pl = ppl[i];
      else {
         pl = new CListFixed;
         m_lRenderListBack.Add (&pl);  // so have copy to save reallocing all the time
      }
      pl->Init (sizeof(CMatrix));
      m_lRenderList.Add (&pl);
   }
   ppl = (PCListFixed*) m_lRenderList.Get(0);
   m_pRoot->LeafMatricies (ppl);

   // see if the renderer supports clonerender
   BOOL fCloneRender;
   fCloneRender = pr->pRS->QueryCloneRender();

   PLEAFTYPE plt;
   plt = (PLEAFTYPE) m_lLEAFTYPE.Get(0);
   for (i = 0; i < m_lRenderList.Num(); i++, plt++) {
      PCListFixed pl = ppl[i];
      PCMatrix pm = (PCMatrix)pl->Get(0);

      if (plt->pClone) {
         for (j = 0; j < pl->Num(); j++, pm++) {
            // BUGFIX - If bounding box then only draw one leaf, which will be
            // enough that color information can be transmitted up
            if (j && (pr->dwReason == ORREASON_BOUNDINGBOX))
               break;

            pm->MultiplyRight (&m_MatrixObject);

            if (!fCloneRender)
               plt->pClone->Render (pr, 44, pm, fDetail);
         }

         if (fCloneRender)
            pr->pRS->CloneRender (&plt->pClone->m_gCode, &plt->pClone->m_gSub,
               pl->Num(), (PCMatrix) pl->Get(0));
      }

      // delete it
      // BUGFIX - Dont since kept in m_lrenderlistback - delete pl;
   }
}




/**********************************************************************************
CObjectBranch::QueryBoundingBox - Standard API
*/
void CObjectBranch::QueryBoundingBox (PCPoint pCorner1, PCPoint pCorner2, DWORD dwSubObject)
{
   CalcBranches();

   // if drawing for bounding box then just draw a cube
   // note: don't return
   CPoint pScale, pBottom, pAround, pLeft, pMin, pMax;
   m_pRevFarAway->ScaleGet (&pScale);
   m_pRevFarAway->DirectionGet (&pBottom, &pAround, &pLeft);
   pMin.Zero();
   pMin.Copy (&pBottom);
   pMin.p[0] -= pScale.p[0] / 2.0;
   pMin.p[1] -= pScale.p[1] / 2.0;
   pMax.Add (&pMin, &pScale);
   pCorner1->Copy (&pMin);
   pCorner2->Copy (&pMax);

   // draw the branches
   PBRANCHNOODLE pbn = (PBRANCHNOODLE) m_lBRANCHNOODLE.Get(0);
   DWORD i;
   CPoint p1, p2;
   for (i = 0; i < m_lBRANCHNOODLE.Num(); i++, pbn++) {
      pbn->pNoodle->QueryBoundingBox (&p1, &p2);

      pCorner1->Min(&p1);
      pCorner2->Max(&p2);
   }

   // NOTE: In version of Render, even if just checking for a bounding box
   // makes sure to call into leaf to get color... not sure why
   // but the code doesn't make sense in boundingboxquery()


#ifdef _DEBUG
   // test, make sure bounding box not too small
   //CPoint p1,p2;
   //DWORD i;
   CObjectTemplate::QueryBoundingBox (&p1, &p2, dwSubObject);
   for (i = 0; i < 3; i++)
      if ((p1.p[i] < pCorner1->p[i] - CLOSE) || (p2.p[i] > pCorner2->p[i] + CLOSE))
         break;
   if (i < 3)
      OutputDebugString ("\r\nCObjectBranch::QueryBoundingBox too small.");
#endif
}

/**********************************************************************************
CObjectBranch::Clone - Clones this

inputs
   none
returns
   PCObjectSocket - Clone
*/
PCObjectSocket CObjectBranch::Clone (void)
{
   PCObjectBranch pNew;

   pNew = new CObjectBranch(NULL, &m_OSINFO);

   // clone template info
   CloneTemplate(pNew);

   if (pNew->m_pRoot)
      delete pNew->m_pRoot;
   pNew->m_pRoot = m_pRoot->Clone();

   // clear the old ones away
   DWORD i;
   PLEAFTYPE plt;
   plt = (PLEAFTYPE) pNew->m_lLEAFTYPE.Get(0);
   for (i = 0; i < pNew->m_lLEAFTYPE.Num(); i++, plt++)
      if (plt->pClone)
         plt->pClone->Release();
   pNew->m_lLEAFTYPE.Init (sizeof(LEAFTYPE), m_lLEAFTYPE.Get(0), m_lLEAFTYPE.Num());
   plt = (PLEAFTYPE) pNew->m_lLEAFTYPE.Get(0);
   for (i = 0; i < pNew->m_lLEAFTYPE.Num(); i++, plt++)
      if (plt->pClone)
         plt->pClone->AddRef ();

   pNew->m_fThickScale = m_fThickScale;
   pNew->m_dwDivide = m_dwDivide;
   pNew->m_dwTextureWrap = m_dwTextureWrap;
   pNew->m_fRound = m_fRound;
   pNew->m_fDispLowDetail = m_fDispLowDetail;
   pNew->m_fCap = m_fCap;
   pNew->m_pCurModify = pNew->m_pRoot;
   pNew->m_fCurModifyLeaf = FALSE;
   pNew->m_pCurModifyIndex = 0;
   pNew->m_fRoots = m_fRoots;
   pNew->m_fBaseThick = m_fBaseThick;
   pNew->m_fRootThick = m_fRootThick;
   pNew->m_fTapRoot = m_fTapRoot;

   if (pNew->m_pAutoBranch) {
      delete pNew->m_pAutoBranch;
      pNew->m_pAutoBranch = NULL;
   }
   if (m_pAutoBranch) {
      pNew->m_pAutoBranch = new AUTOBRANCH;
      if (pNew->m_pAutoBranch)
         memcpy (pNew->m_pAutoBranch, m_pAutoBranch, sizeof(AUTOBRANCH));
   }
   pNew->m_fDirty = TRUE;

   return pNew;
}

static PWSTR gpszLeafType = L"LeafType";
static PWSTR gpszSub = L"Sub";
static PWSTR gpszCode = L"Code";
static PWSTR gpszWeight = L"Weight";
static PWSTR gpszThickScale = L"ThickScale";
static PWSTR gpszDivide = L"Divide";
static PWSTR gpszRound = L"Round";
static PWSTR gpszTextureWrap = L"TextureWrap";
static PWSTR gpszCap = L"Cap";
static PWSTR gpszRoots = L"Roots";
static PWSTR gpszBaseThick = L"BaseThick";
static PWSTR gpszRootThick = L"RootThick";
static PWSTR gpszTapRoot = L"TapRoot";

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

// misc
static PWSTR gpszSeed = L"Seed";                 // seed to use
static PWSTR gpszMaxGen = L"MaxGen";               // maximum generations

PCMMLNode2 CObjectBranch::MMLTo (void)
{
   // call into the template first
   PCMMLNode2 pNode = MMLToTemplate();
   if (!pNode)
      return NULL;


   MMLValueSet (pNode, gpszRoots, (int) m_fRoots);
   MMLValueSet (pNode, gpszBaseThick, m_fBaseThick);
   MMLValueSet (pNode, gpszRootThick, m_fRootThick);
   MMLValueSet (pNode, gpszTapRoot, m_fTapRoot);
   MMLValueSet (pNode, gpszThickScale, m_fThickScale);
   MMLValueSet (pNode, gpszDivide, (int)m_dwDivide);
   MMLValueSet (pNode, gpszRound, (int)m_fRound);
   MMLValueSet (pNode, gpszCap, (int)m_fCap);
   MMLValueSet (pNode, gpszTextureWrap, (int)m_dwTextureWrap);
   // NOTE: Specifically NOT saving m_fDispLowDetail

   PCMMLNode2 pSub;
   pSub = m_pRoot->MMLTo();
   if (pSub)
      pNode->ContentAdd (pSub);

   DWORD i;
   PLEAFTYPE plt;
   plt =  (PLEAFTYPE) m_lLEAFTYPE.Get(0);
   for (i = 0; i < m_lLEAFTYPE.Num(); i++, plt++) {
      pSub = pNode->ContentAddNewNode ();
      if (!pSub)
         continue;
      pSub->NameSet (gpszLeafType);
      MMLValueSet (pSub, gpszCode, (PBYTE) &plt->gCode, sizeof(plt->gCode));
      MMLValueSet (pSub, gpszSub, (PBYTE) &plt->gSub, sizeof(plt->gSub));
      MMLValueSet (pSub, gpszWeight, plt->fWeight);
   }


   // info for autogen
   if (m_pAutoBranch) {
      MMLValueSet (pNode, gpszBranchInitialAngle, m_pAutoBranch->fBranchInitialAngle); // initial angle. 0..PI. 0 = up
      MMLValueSet (pNode, gpszBranchLength, m_pAutoBranch->fBranchLength); // typical length, meters
      MMLValueSet (pNode, gpszBranchLengthVar, m_pAutoBranch->fBranchLengthVar); // variation in lenght. 0..1
      MMLValueSet (pNode, gpszBranchShorten, m_pAutoBranch->fBranchShorten);   // amount shortens per generation. 0..1

      // branch extension
      MMLValueSet (pNode, gpszBranchExtendProb, m_pAutoBranch->fBranchExtendProb);      // probability that will extend branch, 0..1
      MMLValueSet (pNode, gpszBranchExtendProbGen, m_pAutoBranch->fBranchExtendProbGen);   // change in fBranchExtendProb per generation, 0..1
      MMLValueSet (pNode, gpszBranchDirVar, m_pAutoBranch->fBranchDirVar);          // variation in direction, 0..1

      // branch forking
      MMLValueSet (pNode, gpszBranchForkProb, m_pAutoBranch->fBranchForkProb);        // probability of branch forking, 0..1
      MMLValueSet (pNode, gpszBranchForkProbGen, m_pAutoBranch->fBranchForkProbGen);     // chance of forking increasing per generation, 0..1
      MMLValueSet (pNode, gpszBranchForkNum, m_pAutoBranch->fBranchForkNum);         // number of forks, 0..1.
      MMLValueSet (pNode, gpszBranchForkForwards, m_pAutoBranch->fBranchForkForwards);    // how forwards. 1 = forwards, 0 = backwards
      MMLValueSet (pNode, gpszBranchForkUpDown, m_pAutoBranch->fBranchForkUpDown);      // how up/down. 1=up, 0=down
      MMLValueSet (pNode, gpszBranchForkVar, m_pAutoBranch->fBranchForkVar);         // variation in forking, 0..1.

      // branch gravity
      MMLValueSet (pNode, gpszBranchGrav, m_pAutoBranch->fBranchGrav);            // 1=branchs will want to go upwards, 0=branch go downwards
      MMLValueSet (pNode, gpszBranchGravGen, m_pAutoBranch->fBranchGravGen);         // how much fBranchGrav changes each generation. 1 = increase, 0 = decrease

      // leaves along branch
      MMLValueSet (pNode, gpszLeafExtendProb, m_pAutoBranch->fLeafExtendProb);        // 0..1, probability of leaf being along extension
      MMLValueSet (pNode, gpszLeafExtendProbGen, m_pAutoBranch->fLeafExtendProbGen);     // 0..1, likelihood of leaves increasing
      MMLValueSet (pNode, gpszLeafExtendNum, m_pAutoBranch->fLeafExtendNum);         // 0..1, number of leaves
      MMLValueSet (pNode, gpszLeafExtendForwards, m_pAutoBranch->fLeafExtendForwards);    // 0..1, 1 = forwards, 0 = backwards
      MMLValueSet (pNode, gpszLeafExtendVar, m_pAutoBranch->fLeafExtendVar);         // 0..1, variation in leaf direction
      MMLValueSet (pNode, gpszLeafExtendScale, m_pAutoBranch->fLeafExtendScale);       // 0..1, .5 = default scale
      MMLValueSet (pNode, gpszLeafExtendScaleVar, m_pAutoBranch->fLeafExtendScaleVar);    // 0..1, amount leaf size varies
      MMLValueSet (pNode, gpszLeafExtendScaleGen, m_pAutoBranch->fLeafExtendScaleGen);    // 0..1, how much leaf size changes, .5 = none, 1 = larger

      // leaf at terminal node
      MMLValueSet (pNode, gpszLeafEndProb, m_pAutoBranch->fLeafEndProb);           // 0..1, likelihood of leaf at end
      MMLValueSet (pNode, gpszLeafEndNum, m_pAutoBranch->fLeafEndNum);            // 0..1, number of leaves at the end
      MMLValueSet (pNode, gpszLeafEndNumVar, m_pAutoBranch->fLeafEndNumVar);         // 0..1, variation in the number of leaves at the end
      MMLValueSet (pNode, gpszLeafEndSymmetry, m_pAutoBranch->fLeafEndSymmetry);       // 0..1, amount of symmetry in end leaves
      MMLValueSet (pNode, gpszLeafEndVar, m_pAutoBranch->fLeafEndVar);            // 0..1, variation in leaf location at end
      MMLValueSet (pNode, gpszLeafEndScale, m_pAutoBranch->fLeafEndScale);          // 0..1, .5 = default scale
      MMLValueSet (pNode, gpszLeafEndScaleVar, m_pAutoBranch->fLeafEndScaleVar);       // 0..1, amount leaf size varies

      // misc
      MMLValueSet (pNode, gpszSeed, (int) m_pAutoBranch->dwSeed);                 // seed to use
      MMLValueSet (pNode, gpszMaxGen, (int) m_pAutoBranch->dwMaxGen);               // maximum generations
   }

   return pNode;
}

BOOL CObjectBranch::MMLFrom (PCMMLNode2 pNode)
{
   DWORD i;
   PLEAFTYPE plt;
   plt = (PLEAFTYPE) m_lLEAFTYPE.Get(0);
   for (i = 0; i < m_lLEAFTYPE.Num(); i++, plt++)
      if (plt->pClone)
         plt->pClone->Release();
   m_lLEAFTYPE.Clear();

   // call into the template first
   if (!MMLFromTemplate (pNode))
      return FALSE;

   m_fRoots = (BOOL) MMLValueGetInt (pNode, gpszRoots, (int) FALSE);
   m_fBaseThick = MMLValueGetDouble (pNode, gpszBaseThick, .5);
   m_fRootThick = MMLValueGetDouble (pNode, gpszRootThick, .5);
   m_fTapRoot = MMLValueGetDouble (pNode, gpszTapRoot, .5);
   m_fThickScale = MMLValueGetDouble (pNode, gpszThickScale, .01);
   m_dwDivide = (DWORD) MMLValueGetInt (pNode, gpszDivide, 0);
   m_fRound = (BOOL) MMLValueGetInt (pNode, gpszRound, FALSE);
   m_fCap = (BOOL) MMLValueGetInt (pNode, gpszCap, FALSE);
   m_dwTextureWrap = (int) MMLValueGetInt (pNode, gpszTextureWrap, 1);
   // NOTE: Specifically not reading m_fDispLowDetail

   PCMMLNode2 pSub;
   PWSTR psz;
   pSub = NULL;
   pNode->ContentEnum (pNode->ContentFind (gpszBranchNode), &psz, &pSub);
   if (pSub)
      m_pRoot->MMLFrom (pSub);

   DWORD dwRenderShard = m_OSINFO.dwRenderShard;

   for (i = 0; i < pNode->ContentNum(); i++) {
      pSub = NULL;
      pNode->ContentEnum (i, &psz, &pSub);
      if (!pSub)
         continue;
      psz = pSub->NameGet();
      if (!psz)
         continue;

      if (!_wcsicmp(psz, gpszLeafType)) {
         LEAFTYPE lt;
         memset (&lt, 0, sizeof(lt));
         MMLValueGetBinary (pSub, gpszCode, (PBYTE) &lt.gCode, sizeof(lt.gCode));
         MMLValueGetBinary (pSub, gpszSub, (PBYTE) &lt.gSub, sizeof(lt.gSub));
         lt.fWeight = MMLValueGetDouble (pSub, gpszWeight, 1);

         LEAFTYPEFill (dwRenderShard, &lt);

         m_lLEAFTYPE.Add (&lt);
         continue;
      }
   }

   if (m_pAutoBranch)
      delete m_pAutoBranch;
   m_pAutoBranch = NULL;
   if (-10 != MMLValueGetDouble (pNode, gpszBranchInitialAngle, -10))
      AutoBranchFill ();   // so have something

   if (m_pAutoBranch) {
      m_pAutoBranch->fBranchInitialAngle = MMLValueGetDouble (pNode, gpszBranchInitialAngle, m_pAutoBranch->fBranchInitialAngle); // initial angle. 0..PI. 0 = up
      m_pAutoBranch->fBranchLength = MMLValueGetDouble (pNode, gpszBranchLength, m_pAutoBranch->fBranchLength); // typical length, meters
      m_pAutoBranch->fBranchLengthVar = MMLValueGetDouble (pNode, gpszBranchLengthVar, m_pAutoBranch->fBranchLengthVar); // variation in lenght. 0..1
      m_pAutoBranch->fBranchShorten = MMLValueGetDouble (pNode, gpszBranchShorten, m_pAutoBranch->fBranchShorten);   // amount shortens per generation. 0..1

      // branch extension
      m_pAutoBranch->fBranchExtendProb = MMLValueGetDouble (pNode, gpszBranchExtendProb, m_pAutoBranch->fBranchExtendProb);      // probability that will extend branch, 0..1
      m_pAutoBranch->fBranchExtendProbGen = MMLValueGetDouble (pNode, gpszBranchExtendProbGen, m_pAutoBranch->fBranchExtendProbGen);   // change in fBranchExtendProb per generation, 0..1
      m_pAutoBranch->fBranchDirVar = MMLValueGetDouble (pNode, gpszBranchDirVar, m_pAutoBranch->fBranchDirVar);          // variation in direction, 0..1

      // branch forking
      m_pAutoBranch->fBranchForkProb = MMLValueGetDouble (pNode, gpszBranchForkProb, m_pAutoBranch->fBranchForkProb);        // probability of branch forking, 0..1
      m_pAutoBranch->fBranchForkProbGen = MMLValueGetDouble (pNode, gpszBranchForkProbGen, m_pAutoBranch->fBranchForkProbGen);     // chance of forking increasing per generation, 0..1
      m_pAutoBranch->fBranchForkNum = MMLValueGetDouble (pNode, gpszBranchForkNum, m_pAutoBranch->fBranchForkNum);         // number of forks, 0..1.
      m_pAutoBranch->fBranchForkForwards = MMLValueGetDouble (pNode, gpszBranchForkForwards, m_pAutoBranch->fBranchForkForwards);    // how forwards. 1 = forwards, 0 = backwards
      m_pAutoBranch->fBranchForkUpDown = MMLValueGetDouble (pNode, gpszBranchForkUpDown, m_pAutoBranch->fBranchForkUpDown);      // how up/down. 1=up, 0=down
      m_pAutoBranch->fBranchForkVar = MMLValueGetDouble (pNode, gpszBranchForkVar, m_pAutoBranch->fBranchForkVar);         // variation in forking, 0..1.

      // branch gravity
      m_pAutoBranch->fBranchGrav = MMLValueGetDouble (pNode, gpszBranchGrav, m_pAutoBranch->fBranchGrav);            // 1=branchs will want to go upwards, 0=branch go downwards
      m_pAutoBranch->fBranchGravGen = MMLValueGetDouble (pNode, gpszBranchGravGen, m_pAutoBranch->fBranchGravGen);         // how much fBranchGrav changes each generation. 1 = increase, 0 = decrease

      // leaves along branch
      m_pAutoBranch->fLeafExtendProb = MMLValueGetDouble (pNode, gpszLeafExtendProb, m_pAutoBranch->fLeafExtendProb);        // 0..1, probability of leaf being along extension
      m_pAutoBranch->fLeafExtendProbGen = MMLValueGetDouble (pNode, gpszLeafExtendProbGen, m_pAutoBranch->fLeafExtendProbGen);     // 0..1, likelihood of leaves increasing
      m_pAutoBranch->fLeafExtendNum = MMLValueGetDouble (pNode, gpszLeafExtendNum, m_pAutoBranch->fLeafExtendNum);         // 0..1, number of leaves
      m_pAutoBranch->fLeafExtendForwards= MMLValueGetDouble (pNode, gpszLeafExtendForwards, m_pAutoBranch->fLeafExtendForwards);    // 0..1, 1 = forwards, 0 = backwards
      m_pAutoBranch->fLeafExtendVar = MMLValueGetDouble (pNode, gpszLeafExtendVar, m_pAutoBranch->fLeafExtendVar);         // 0..1, variation in leaf direction
      m_pAutoBranch->fLeafExtendScale = MMLValueGetDouble (pNode, gpszLeafExtendScale, m_pAutoBranch->fLeafExtendScale);       // 0..1, .5 = default scale
      m_pAutoBranch->fLeafExtendScaleVar = MMLValueGetDouble (pNode, gpszLeafExtendScaleVar, m_pAutoBranch->fLeafExtendScaleVar);    // 0..1, amount leaf size varies
      m_pAutoBranch->fLeafExtendScaleGen = MMLValueGetDouble (pNode, gpszLeafExtendScaleGen, m_pAutoBranch->fLeafExtendScaleGen);    // 0..1, how much leaf size changes, .5 = none, 1 = larger

      // leaf at terminal node
      m_pAutoBranch->fLeafEndProb = MMLValueGetDouble (pNode, gpszLeafEndProb, m_pAutoBranch->fLeafEndProb);           // 0..1, likelihood of leaf at end
      m_pAutoBranch->fLeafEndNum = MMLValueGetDouble (pNode, gpszLeafEndNum, m_pAutoBranch->fLeafEndNum);            // 0..1, number of leaves at the end
      m_pAutoBranch->fLeafEndNumVar = MMLValueGetDouble (pNode, gpszLeafEndNumVar, m_pAutoBranch->fLeafEndNumVar);         // 0..1, variation in the number of leaves at the end
      m_pAutoBranch->fLeafEndSymmetry = MMLValueGetDouble (pNode, gpszLeafEndSymmetry, m_pAutoBranch->fLeafEndSymmetry);       // 0..1, amount of symmetry in end leaves
      m_pAutoBranch->fLeafEndVar = MMLValueGetDouble (pNode, gpszLeafEndVar, m_pAutoBranch->fLeafEndVar);            // 0..1, variation in leaf location at end
      m_pAutoBranch->fLeafEndScale = MMLValueGetDouble (pNode, gpszLeafEndScale, m_pAutoBranch->fLeafEndScale);          // 0..1, .5 = default scale
      m_pAutoBranch->fLeafEndScaleVar = MMLValueGetDouble (pNode, gpszLeafEndScaleVar, m_pAutoBranch->fLeafEndScaleVar);       // 0..1, amount leaf size varies

      // misc
      m_pAutoBranch->dwSeed = (DWORD) MMLValueGetInt (pNode, gpszSeed, (int) m_pAutoBranch->dwSeed);                 // seed to use
      m_pAutoBranch->dwMaxGen = (DWORD) MMLValueGetInt (pNode, gpszMaxGen, (int) m_pAutoBranch->dwMaxGen);               // maximum generations
   }
   m_fDirty = TRUE;

   return TRUE;
}



/**************************************************************************************
CObjectBranch::MoveReferencePointQuery - 
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
BOOL CObjectBranch::MoveReferencePointQuery (DWORD dwIndex, PCPoint pp)
{
   PSPLINEMOVEP ps;
   DWORD dwDataSize;
   ps = gaBranchMove;
   dwDataSize = sizeof(gaBranchMove);

   if (dwIndex >= dwDataSize / sizeof(SPLINEMOVEP))
      return FALSE;

   // always at 0,0 in Branchs
   pp->Zero();
   return TRUE;
}

/**************************************************************************************
CObjectBranch::MoveReferenceStringQuery -
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
BOOL CObjectBranch::MoveReferenceStringQuery (DWORD dwIndex, PWSTR psz, DWORD dwSize, DWORD *pdwNeeded)
{
   PSPLINEMOVEP ps;
   DWORD dwDataSize;
   ps = gaBranchMove;
   dwDataSize = sizeof(gaBranchMove);
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
CObjectBranch::CalcBranches - Call this to calculte all the branch information,
control points, and noodles
*/
void CObjectBranch::CalcBranches (void)
{
   if (!m_fDirty)
      return;
   m_fDirty = FALSE;

   // clear the branch noodle
   PBRANCHNOODLE pbn = (PBRANCHNOODLE) m_lBRANCHNOODLE.Get(0);
   DWORD i;
   for (i = 0; i < m_lBRANCHNOODLE.Num(); i++, pbn++)
      delete pbn->pNoodle;
   m_lBRANCHNOODLE.Clear();

   // calculate the thickness
   m_pRoot->CalcThickness (&m_lLEAFTYPE);

   // make the base thicker if root
   if (m_fRoots)
      m_pRoot->m_fThickness *= (1.0 + m_fBaseThick) * (1.0 + m_fBaseThick);
      // using the square of the thickness since calc to thickdist uses the sqert

   // figure out which ones are the roots...
   PBRANCHINFO pbi;
   if (m_fRoots) {
      fp fSumThick = 0;
      pbi = (PBRANCHINFO) m_pRoot->m_lBRANCHINFO.Get(0);
      for (i = 0; i < m_pRoot->m_lBRANCHINFO.Num(); i++, pbi++) {
         fp fLen = sqrt(pbi->pLoc.p[0] * pbi->pLoc.p[0] + pbi->pLoc.p[1] * pbi->pLoc.p[1]);
         if (pbi->pLoc.p[2] > fLen / 5.0)
            continue;   // not a root

         pbi->pNode->m_fIsRoot = TRUE;
         fSumThick += pbi->pNode->m_fThickness;
      }

      // scale up all the thicknesses
      if (m_pRoot->m_fThickness)
         fSumThick = m_pRoot->m_fThickness / fSumThick * m_fRootThick;

      // scale all the root thicknesses
      pbi = (PBRANCHINFO) m_pRoot->m_lBRANCHINFO.Get(0);
      for (i = 0; i < m_pRoot->m_lBRANCHINFO.Num(); i++, pbi++)
         if (pbi->pNode->m_fIsRoot)
            pbi->pNode->ScaleThick (fSumThick);
   }

   // m_pRoot->SortByThickness ();

   // calculate the location of all the nodes
   CPoint pLoc, pUp;
   pLoc.Zero();
   pUp.Zero();
   pUp.p[0] = .5;
   pUp.p[1] = .5;
   pUp.p[2] = .5;
   m_pRoot->CalcLoc (&pLoc, &pUp);

   // calulate all the noodle
   CListFixed lLoc, lThick, lUp;
   lLoc.Init (sizeof(CPoint));
   lThick.Init (sizeof(CPoint));
   lUp.Init (sizeof(CPoint));
   if (m_fRoots) {
      // tap root
      CPoint p;
      p.Zero();
      p.p[2] = -m_fTapRoot;
      lLoc.Add (&p);
      lThick.Add (&p);
      lUp.Add (&p);
   }
   m_pRoot->FillNoodle (m_fThickScale, &lLoc, &lUp, &lThick, &m_lBRANCHNOODLE,
      m_dwDivide, m_fRound, m_dwTextureWrap, m_fCap);
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
      m_dwDivide, m_fRound, m_dwTextureWrap, m_fCap);

   // figure out where cp go
   m_lBRANCHCP.Clear();
   m_pRoot->EnumCP (&m_lBRANCHCP, NULL, m_pCurModify, m_fCurModifyLeaf, m_pCurModifyIndex,
      &m_lLEAFTYPE);

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
   m_pRoot->LeafMatricies (ppl);

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
   m_fLowDetail = pScale.Length();
   m_pRevFarAway->BackfaceCullSet (TRUE);
   m_pRevFarAway->DirectionSet (&pBottom, &pUp, &pLeft);
   m_pRevFarAway->ProfileSet (RPROF_CIRCLE, 0);
   m_pRevFarAway->RevolutionSet (RREV_CIRCLE, 0);
   m_pRevFarAway->ScaleSet (&pScale);

   // loop through all the branches and include the longest in the detail
   // BUGFIX - Make this 1/2 longest
   pbn = (PBRANCHNOODLE) m_lBRANCHNOODLE.Get(0);
   for (i = 0; i < m_lBRANCHNOODLE.Num(); i++, pbn++)
      m_fLowDetail = max(m_fLowDetail, pbn->fLength / 2.0);

}



/*************************************************************************************
CObjectBranch::ControlPointQuery - Called to query the information about a control
point identified by dwID.

inputs
   DWORD       dwID - ID
   POSCONTROL  pInfo - Filled in with the infomration
returns
   BOOL - TRUE if successful
*/
BOOL CObjectBranch::ControlPointQuery (DWORD dwID, POSCONTROL pInfo)
{
   CalcBranches();
   if (dwID >= m_lBRANCHCP.Num())
      return FALSE;  // only have one control point
   
   PBRANCHCP pb;
   pb = (PBRANCHCP) m_lBRANCHCP.Get(dwID);

   // CP for stem location
   memset (pInfo,0, sizeof(*pInfo));
   pInfo->dwID = dwID;
   pInfo->fSize = pb->fSize * 1.5;
   pInfo->pLocation.Copy (&pb->pLoc);

   switch (pb->dwAttrib) {
      case 0:   // CP to select object
         pInfo->dwStyle = CPSTYLE_POINTER;
         pInfo->cColor = RGB(0xff,0xff,0xff);
         pInfo->fButton = TRUE;
         wcscpy (pInfo->szName, L"Click to select");
         break;
      case 1:  // CP to move end
         pInfo->dwStyle = CPSTYLE_SPHERE;
         pInfo->cColor = RGB(0xff,0,0xff);
         wcscpy (pInfo->szName, L"Move");
         break;
      case 2:  // CP to split
         pInfo->dwStyle = CPSTYLE_POINTER;
         pInfo->cColor = RGB(0xff,0xff,0);
         pInfo->fButton = TRUE;
         wcscpy (pInfo->szName, L"Click to split");
         break;
      case 3:  // CP to delete
         pInfo->dwStyle = CPSTYLE_POINTER;
         pInfo->cColor = RGB(0xff,0,0);
         pInfo->fButton = TRUE;
         wcscpy (pInfo->szName, L"Click to delete");
         break;
      case 4:  // CP to add new node
         pInfo->dwStyle = CPSTYLE_POINTER;
         pInfo->cColor = RGB(0x40,0x40,0);
         pInfo->fButton = TRUE;
         wcscpy (pInfo->szName, L"Click to add new branch");
         break;
      case 5:  // CP to add a new leaf
         pInfo->dwStyle = CPSTYLE_POINTER;
         pInfo->cColor = RGB(0,0xff,0);
         pInfo->fButton = TRUE;
         wcscpy (pInfo->szName, L"Click to add new leaf");
         break;
      case 6:  // CP to move and scale leaf
         pInfo->dwStyle = CPSTYLE_SPHERE;
         pInfo->cColor = RGB(0,0xff,0);
         wcscpy (pInfo->szName, L"Rotation and size");
         break;
      case 7:  // CP to move up definition
         pInfo->dwStyle = CPSTYLE_CUBE;
         pInfo->cColor = RGB(0,0,0xff);
         wcscpy (pInfo->szName, L"Leaf's Up direction");
         break;
      case 8:  // CP to add a new type
         pInfo->dwStyle = CPSTYLE_POINTER;
         pInfo->cColor = RGB(0,0xff,0xff);
         pInfo->fButton = TRUE;
         wcscpy (pInfo->szName, L"Click to switch leaves");
         break;
   };

   return TRUE;
}

/*************************************************************************************
CObjectBranch::ControlPointSet - Called to change the valud of a control point.

inputs
   DWORD       dwID - ID
   PCPoint     pVal - Contains the new location, in object coordinates
returns
   BOOL - TRUE if successful
*/
BOOL CObjectBranch::ControlPointSet (DWORD dwID, PCPoint pVal, PCPoint pViewer)
{
   if (dwID >= m_lBRANCHCP.Num())
      return FALSE;  // only one CP

   PBRANCHCP pb;
   pb = (PBRANCHCP) m_lBRANCHCP.Get(dwID);

   // hit CP for stem
   // tell the world we're about to change
   if (m_pWorld)
      m_pWorld->ObjectAboutToChange (this);

   switch (pb->dwAttrib) {
      case 0:   // CP to select object
         {
            m_pCurModify = pb->pBranch;
            m_fCurModifyLeaf = pb->fLeaf;
            if (pb->fLeaf)
               m_pCurModifyIndex = (PVOID)(size_t) pb->dwIndex;
            else {
               PBRANCHINFO pbi = (PBRANCHINFO) pb->pBranch->m_lBRANCHINFO.Get(pb->dwIndex);
               m_pCurModifyIndex = (PVOID) pbi->pNode;
            }
         }
         break;
      case 1:  // CP to move end
         {
            // find out wher epoint is
            PBRANCHINFO pbi = (PBRANCHINFO) pb->pBranch->m_lBRANCHINFO.Get(pb->dwIndex);
            pbi->pLoc.Subtract (pVal, &pb->pBranch->m_pLoc);
         }
         break;
      case 2:  // CP to split
         {
            PBRANCHINFO pbi = (PBRANCHINFO) pb->pBranch->m_lBRANCHINFO.Get(pb->dwIndex);

            // create new one
            PCBranchNode pNew = new CBranchNode;
            if (!pNew)
               return TRUE;
            
            // halve the distance
            pbi->pLoc.Scale (.5);

            BRANCHINFO bi;
            memset (&bi, 0, sizeof(bi));
            bi.pLoc.Copy (&pbi->pLoc);
            bi.pNode = pbi->pNode;
            pNew->m_lBRANCHINFO.Add (&bi);

            pbi->pNode = pNew;

            m_pCurModifyIndex = (PVOID) pNew;
            m_fCurModifyLeaf = FALSE;
            m_pCurModify = pb->pBranch;
         }
         break;
      case 3:  // CP to delete
         if (pb->fLeaf) {
            pb->pBranch->m_lLEAFINFO.Remove (pb->dwIndex);
         }
         else {
            // remove from the list
            PBRANCHINFO pbi = (PBRANCHINFO) pb->pBranch->m_lBRANCHINFO.Get(pb->dwIndex);
            PCBranchNode pOld = pbi->pNode;
            pb->pBranch->m_lBRANCHINFO.Remove (pb->dwIndex);

            // add the items on
            pbi = (PBRANCHINFO) pOld->m_lBRANCHINFO.Get(0);
            DWORD i;
            for (i = 0; i < pOld->m_lBRANCHINFO.Num(); i++, pbi++)
               pb->pBranch->m_lBRANCHINFO.Add (pbi);
            pOld->m_lBRANCHINFO.Clear();
            PLEAFINFO pli;
            pli = (PLEAFINFO) pOld->m_lLEAFINFO.Get(0);
            for (i = 0; i < pOld->m_lLEAFINFO.Num(); i++, pli++)
               pb->pBranch->m_lLEAFINFO.Add (pli);
            pOld->m_lLEAFINFO.Clear();

            // finally, dlete what's left
            delete pOld;
         }
         break;
      case 4:  // CP to add new node
         {
            PBRANCHINFO pbi = (PBRANCHINFO) pb->pBranch->m_lBRANCHINFO.Get(0);
            BRANCHINFO bi;
            fp fLen;
            memset (&bi, 0, sizeof(bi));
            bi.pLoc.Copy (&pb->pBranch->m_pUp);
            fLen = 1;
            if (pb->pBranch->m_lBRANCHINFO.Num())
               fLen = pbi->pLoc.Length(); // so same length as existing
            bi.pNode = new CBranchNode;
            if (!bi.pNode)
               return TRUE;

            bi.pLoc.Scale (fLen);
            pb->pBranch->m_lBRANCHINFO.Add (&bi);
            m_pCurModifyIndex = (PVOID) bi.pNode;
            m_fCurModifyLeaf = FALSE;
            m_pCurModify = pb->pBranch;
         }
         break;

      case 5:   // cp to add a new leaf
         {
            LEAFINFO li;
            memset (&li, 0, sizeof(li));
            li.dwID = rand() % m_lLEAFTYPE.Num();   // start with random leaf
            li.fScale = 1.0;  // default size
            li.pZ.Zero();
            li.pZ.p[2] = 1;
            li.pY.Zero();
            fp fAngle;
            fAngle = randf(0, 2.0 * PI);
            li.pY.p[0] = sin(fAngle);
            li.pY.p[1] = cos(fAngle);
            pb->pBranch->m_lLEAFINFO.Add (&li);
         }
         break;

      case 6:  // rotation and size
         {
            PLEAFINFO pli = (PLEAFINFO) pb->pBranch->m_lLEAFINFO.Get(pb->dwIndex);
            PLEAFTYPE plt = (PLEAFTYPE) m_lLEAFTYPE.Get(pli->dwID);

            // convert the point to new coords
            CPoint p;
            fp fLen;
            p.Subtract (pVal, &pb->pBranch->m_pLoc);
            fLen = p.Length();
            if (fLen < CLOSE)
               break;   // too close to change
            p.Normalize();
            if (plt->fNegative)
               p.Scale(-1);
            if (plt->dwDim == 0) {
               // have an X value. convert to Y
               CPoint pY;
               pY.CrossProd (&pli->pZ, &p);
               pY.Normalize();
               if (pY.Length() < CLOSE)
                  break;
               p.Copy (&pY);
            }

            // scale the length
            fLen /= plt->fDimLength;
            fLen = max(fLen, CLOSE);

            pli->fScale = fLen;
            pli->pY.Copy (&p);
            pli->pX.CrossProd (&pli->pY, &pli->pZ);
            pli->pZ.CrossProd (&pli->pX, &pli->pY);
            pli->pZ.Normalize();
            if (pli->pZ.Length() < CLOSE)
               pli->pZ.p[2] = 1; // always have something
         }
         break;
      case 7:  // leaf's up direction
         {
            PLEAFINFO pli = (PLEAFINFO) pb->pBranch->m_lLEAFINFO.Get(pb->dwIndex);
            PLEAFTYPE plt = (PLEAFTYPE) m_lLEAFTYPE.Get(pli->dwID);

            // convert the point to new coords
            CPoint p;
            fp fLen;
            p.Subtract (pVal, &pb->pBranch->m_pLoc);
            fLen = p.Length();
            if (fLen < CLOSE)
               break;   // too close to change
            p.Normalize();

            pli->pZ.Copy (&p);
            pli->pY.CrossProd (&pli->pZ, &pli->pX);
            pli->pY.Normalize();
            if (pli->pY.Length() < CLOSE)
               pli->pY.p[1] = 1; // so always have soemthing
         }
         break;
      case 8:  // leaf's type
         {
            PLEAFINFO pli = (PLEAFINFO) pb->pBranch->m_lLEAFINFO.Get(pb->dwIndex);

            pli->dwID = (pli->dwID+1) % m_lLEAFTYPE.Num();
         }
         break;
   };

   m_fDirty = TRUE;

   // tell the world we've changed
   if (m_pWorld)
      m_pWorld->ObjectChanged (this);
   return TRUE;
}

/*************************************************************************************
CObjectBranch::ControlPointEnum - Called to enumerate a list of control point IDs
into the list.

inputs
   PCListFixed       plDWORD - Should be filled in with control points
returns
   none
*/
void CObjectBranch::ControlPointEnum (PCListFixed plDWORD)
{
   DWORD i;

   CalcBranches();

   plDWORD->Required (m_lBRANCHCP.Num());
   for (i = 0; i < m_lBRANCHCP.Num(); i++)
      plDWORD->Add (&i);
}

/**********************************************************************************
CObjectBranch::DialogQuery - Standard function to ask if object supports dialog.
*/
BOOL CObjectBranch::DialogQuery (void)
{
   return TRUE;
}


/****************************************************************************
BranchAutoPage
*/
BOOL BranchAutoPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCObjectBranch pv = (PCObjectBranch)pPage->m_pUserData;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      PCEscControl pControl;

      MeasureToString (pPage, L"branchlength", pv->m_pAutoBranch->fBranchLength);
      DoubleToControl (pPage, L"seed", (fp) pv->m_pAutoBranch->dwSeed);
      DoubleToControl (pPage, L"maxgen", (fp) pv->m_pAutoBranch->dwMaxGen);

      if (pControl = pPage->ControlFind (L"branchinitialangle"))
         pControl->AttribSetInt (Pos(), (int) (pv->m_pAutoBranch->fBranchInitialAngle * 100.0));
      if (pControl = pPage->ControlFind (L"branchinitialangle"))
         pControl->AttribSetInt (Pos(), (int) (pv->m_pAutoBranch->fBranchInitialAngle * 100.0));
      if (pControl = pPage->ControlFind (L"branchlengthvar"))
         pControl->AttribSetInt (Pos(), (int) (pv->m_pAutoBranch->fBranchLengthVar * 100.0));
      if (pControl = pPage->ControlFind (L"branchshorten"))
         pControl->AttribSetInt (Pos(), (int) (pv->m_pAutoBranch->fBranchShorten * 100.0));

      if (pControl = pPage->ControlFind (L"branchextendprob"))
         pControl->AttribSetInt (Pos(), (int) (pv->m_pAutoBranch->fBranchExtendProb * 100.0));
      if (pControl = pPage->ControlFind (L"branchextendprobgen"))
         pControl->AttribSetInt (Pos(), (int) (pv->m_pAutoBranch->fBranchExtendProbGen * 100.0));
      if (pControl = pPage->ControlFind (L"branchdirvar"))
         pControl->AttribSetInt (Pos(), (int) (pv->m_pAutoBranch->fBranchDirVar * 100.0));

      if (pControl = pPage->ControlFind (L"branchforkprob"))
         pControl->AttribSetInt (Pos(), (int) (pv->m_pAutoBranch->fBranchForkProb * 100.0));
      if (pControl = pPage->ControlFind (L"branchforkprobgen"))
         pControl->AttribSetInt (Pos(), (int) (pv->m_pAutoBranch->fBranchForkProbGen * 100.0));
      if (pControl = pPage->ControlFind (L"branchforknum"))
         pControl->AttribSetInt (Pos(), (int) (pv->m_pAutoBranch->fBranchForkNum * 100.0));
      if (pControl = pPage->ControlFind (L"branchforkforwards"))
         pControl->AttribSetInt (Pos(), (int) (pv->m_pAutoBranch->fBranchForkForwards * 100.0));
      if (pControl = pPage->ControlFind (L"branchforkUpDown"))
         pControl->AttribSetInt (Pos(), (int) (pv->m_pAutoBranch->fBranchForkUpDown * 100.0));
      if (pControl = pPage->ControlFind (L"branchforkvar"))
         pControl->AttribSetInt (Pos(), (int) (pv->m_pAutoBranch->fBranchForkVar * 100.0));

      if (pControl = pPage->ControlFind (L"branchgrav"))
         pControl->AttribSetInt (Pos(), (int) (pv->m_pAutoBranch->fBranchGrav * 100.0));
      if (pControl = pPage->ControlFind (L"branchgravgen"))
         pControl->AttribSetInt (Pos(), (int) (pv->m_pAutoBranch->fBranchGravGen * 100.0));

      if (pControl = pPage->ControlFind (L"leafextendprob"))
         pControl->AttribSetInt (Pos(), (int) (pv->m_pAutoBranch->fLeafExtendProb * 100.0));
      if (pControl = pPage->ControlFind (L"leafextendprobgen"))
         pControl->AttribSetInt (Pos(), (int) (pv->m_pAutoBranch->fLeafExtendProbGen * 100.0));
      if (pControl = pPage->ControlFind (L"leafextendnum"))
         pControl->AttribSetInt (Pos(), (int) (pv->m_pAutoBranch->fLeafExtendNum * 100.0));
      if (pControl = pPage->ControlFind (L"leafextendforwards"))
         pControl->AttribSetInt (Pos(), (int) (pv->m_pAutoBranch->fLeafExtendForwards * 100.0));
      if (pControl = pPage->ControlFind (L"leafextendvar"))
         pControl->AttribSetInt (Pos(), (int) (pv->m_pAutoBranch->fLeafExtendVar * 100.0));
      if (pControl = pPage->ControlFind (L"leafextendscale"))
         pControl->AttribSetInt (Pos(), (int) (pv->m_pAutoBranch->fLeafExtendScale * 100.0));
      if (pControl = pPage->ControlFind (L"leafextendscalevar"))
         pControl->AttribSetInt (Pos(), (int) (pv->m_pAutoBranch->fLeafExtendScaleVar * 100.0));
      if (pControl = pPage->ControlFind (L"leafextendscalegen"))
         pControl->AttribSetInt (Pos(), (int) (pv->m_pAutoBranch->fLeafExtendScaleGen * 100.0));

      if (pControl = pPage->ControlFind (L"leafendprob"))
         pControl->AttribSetInt (Pos(), (int) (pv->m_pAutoBranch->fLeafEndProb * 100.0));
      if (pControl = pPage->ControlFind (L"leafendnum"))
         pControl->AttribSetInt (Pos(), (int) (pv->m_pAutoBranch->fLeafEndNum * 100.0));
      if (pControl = pPage->ControlFind (L"leafendnumvar"))
         pControl->AttribSetInt (Pos(), (int) (pv->m_pAutoBranch->fLeafEndNumVar * 100.0));
      if (pControl = pPage->ControlFind (L"leafendsymmetry"))
         pControl->AttribSetInt (Pos(), (int) (pv->m_pAutoBranch->fLeafEndSymmetry * 100.0));
      if (pControl = pPage->ControlFind (L"leafendvar"))
         pControl->AttribSetInt (Pos(), (int) (pv->m_pAutoBranch->fLeafEndVar * 100.0));
      if (pControl = pPage->ControlFind (L"leafendscale"))
         pControl->AttribSetInt (Pos(), (int) (pv->m_pAutoBranch->fLeafEndScale * 100.0));
      if (pControl = pPage->ControlFind (L"leafendscalevar"))
         pControl->AttribSetInt (Pos(), (int) (pv->m_pAutoBranch->fLeafEndScaleVar * 100.0));

      fp f;
      f = log10 (pv->m_fThickScale * 1000.0) * 50.0;
      f = max(0, f);
      f = min(99, f);
      pControl = pPage->ControlFind (L"thickscale");
      if (pControl)
         pControl->AttribSetInt (Pos(), (int)f);

      if (pv->m_pWorld)
         pv->m_pWorld->ObjectAboutToChange (pv);
      pv->AutoBranchGen ();
      if (pv->m_pWorld)
         pv->m_pWorld->ObjectChanged (pv);
      break;

   case ESCN_EDITCHANGE:
      {
         PESCNEDITCHANGE p = (PESCNEDITCHANGE) pParam;

         if (!p->pControl || !p->pControl->m_pszName)
            break;

         if (pv->m_pWorld)
            pv->m_pWorld->ObjectAboutToChange (pv);

         MeasureParseString (pPage, L"branchlength", &pv->m_pAutoBranch->fBranchLength);
         pv->m_pAutoBranch->fBranchLength = max(pv->m_pAutoBranch->fBranchLength, CLOSE);

         pv->m_pAutoBranch->dwSeed = (DWORD) DoubleFromControl (pPage, L"seed");
         pv->m_pAutoBranch->dwMaxGen = (DWORD) DoubleFromControl (pPage, L"maxgen");
         pv->m_pAutoBranch->dwMaxGen = max(1, pv->m_pAutoBranch->dwMaxGen);

         pv->AutoBranchGen ();

         if (pv->m_pWorld)
            pv->m_pWorld->ObjectChanged (pv);
      }
      break;

   case ESCN_SCROLL:
   // disable this because too slow: case ESCN_SCROLLING:
      {
         PESCNSCROLL p = (PESCNSCROLL) pParam;

         // only do one scroll bar
         if (!p->pControl->m_pszName)
            break;

         if (!_wcsicmp(p->pControl->m_pszName, L"thickscale")) {
            fp fVal = (fp) p->iPos;
            fVal =  pow (10.0, fVal / 50.0) / 1000.0;
            // inverse: f = log10 (pv->m_fThickScale * 1000.0) * 20.0;
            if (fVal == pv->m_fThickScale)
               return TRUE; // no change

            if (pv->m_pWorld)
               pv->m_pWorld->ObjectAboutToChange (pv);
            pv->m_fThickScale = fVal;
            pv->m_fDirty = TRUE;
            if (pv->m_pWorld)
               pv->m_pWorld->ObjectChanged (pv);
            return TRUE;
         }

         fp fVal = (fp) p->iPos / 100.0;
         fp *pf = NULL;

         if (!_wcsicmp(p->pControl->m_pszName, L"branchinitialangle"))
            pf = &pv->m_pAutoBranch->fBranchInitialAngle;
         else if (!_wcsicmp(p->pControl->m_pszName, L"branchinitialangle"))
            pf = &pv->m_pAutoBranch->fBranchInitialAngle;
         else if (!_wcsicmp(p->pControl->m_pszName, L"branchlengthvar"))
            pf = &pv->m_pAutoBranch->fBranchLengthVar;
         else if (!_wcsicmp(p->pControl->m_pszName, L"branchshorten"))
            pf = &pv->m_pAutoBranch->fBranchShorten;

         else if (!_wcsicmp(p->pControl->m_pszName, L"branchextendprob"))
            pf = &pv->m_pAutoBranch->fBranchExtendProb;
         else if (!_wcsicmp(p->pControl->m_pszName, L"branchextendprobgen"))
            pf = &pv->m_pAutoBranch->fBranchExtendProbGen;
         else if (!_wcsicmp(p->pControl->m_pszName, L"branchdirvar"))
            pf = &pv->m_pAutoBranch->fBranchDirVar;

         else if (!_wcsicmp(p->pControl->m_pszName, L"branchforkprob"))
            pf = &pv->m_pAutoBranch->fBranchForkProb;
         else if (!_wcsicmp(p->pControl->m_pszName, L"branchforkprobgen"))
            pf = &pv->m_pAutoBranch->fBranchForkProbGen;
         else if (!_wcsicmp(p->pControl->m_pszName, L"branchforknum"))
            pf = &pv->m_pAutoBranch->fBranchForkNum;
         else if (!_wcsicmp(p->pControl->m_pszName, L"branchforkforwards"))
            pf = &pv->m_pAutoBranch->fBranchForkForwards;
         else if (!_wcsicmp(p->pControl->m_pszName, L"branchforkUpDown"))
            pf = &pv->m_pAutoBranch->fBranchForkUpDown;
         else if (!_wcsicmp(p->pControl->m_pszName, L"branchforkvar"))
            pf = &pv->m_pAutoBranch->fBranchForkVar;

         else if (!_wcsicmp(p->pControl->m_pszName, L"branchgrav"))
            pf = &pv->m_pAutoBranch->fBranchGrav;
         else if (!_wcsicmp(p->pControl->m_pszName, L"branchgravgen"))
            pf = &pv->m_pAutoBranch->fBranchGravGen;

         else if (!_wcsicmp(p->pControl->m_pszName, L"leafextendprob"))
            pf = &pv->m_pAutoBranch->fLeafExtendProb;
         else if (!_wcsicmp(p->pControl->m_pszName, L"leafextendprobgen"))
            pf = &pv->m_pAutoBranch->fLeafExtendProbGen;
         else if (!_wcsicmp(p->pControl->m_pszName, L"leafextendnum"))
            pf = &pv->m_pAutoBranch->fLeafExtendNum;
         else if (!_wcsicmp(p->pControl->m_pszName, L"leafextendforwards"))
            pf = &pv->m_pAutoBranch->fLeafExtendForwards;
         else if (!_wcsicmp(p->pControl->m_pszName, L"leafextendvar"))
            pf = &pv->m_pAutoBranch->fLeafExtendVar;
         else if (!_wcsicmp(p->pControl->m_pszName, L"leafextendscale"))
            pf = &pv->m_pAutoBranch->fLeafExtendScale;
         else if (!_wcsicmp(p->pControl->m_pszName, L"leafextendscalevar"))
            pf = &pv->m_pAutoBranch->fLeafExtendScaleVar;
         else if (!_wcsicmp(p->pControl->m_pszName, L"leafextendscalegen"))
            pf = &pv->m_pAutoBranch->fLeafExtendScaleGen;

         else if (!_wcsicmp(p->pControl->m_pszName, L"leafendprob"))
            pf = &pv->m_pAutoBranch->fLeafEndProb;
         else if (!_wcsicmp(p->pControl->m_pszName, L"leafendnum"))
            pf = &pv->m_pAutoBranch->fLeafEndNum;
         else if (!_wcsicmp(p->pControl->m_pszName, L"leafendnumvar"))
            pf = &pv->m_pAutoBranch->fLeafEndNumVar;
         else if (!_wcsicmp(p->pControl->m_pszName, L"leafendsymmetry"))
            pf = &pv->m_pAutoBranch->fLeafEndSymmetry;
         else if (!_wcsicmp(p->pControl->m_pszName, L"leafendvar"))
            pf = &pv->m_pAutoBranch->fLeafEndVar;
         else if (!_wcsicmp(p->pControl->m_pszName, L"leafendscale"))
            pf = &pv->m_pAutoBranch->fLeafEndScale;
         else if (!_wcsicmp(p->pControl->m_pszName, L"leafendscalevar"))
            pf = &pv->m_pAutoBranch->fLeafEndScaleVar;

         if (!pf)
            break;   // not on of ours
         if (*pf == fVal)
            break;   // no change

         if (pv->m_pWorld)
            pv->m_pWorld->ObjectAboutToChange (pv);
         *pf = fVal;
         pv->AutoBranchGen ();
         if (pv->m_pWorld)
            pv->m_pWorld->ObjectChanged (pv);
         return TRUE;
      }
      break;


   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Procedural branch/tree generator";
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}

/****************************************************************************
BranchPage
*/
BOOL BranchPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCObjectBranch pv = (PCObjectBranch)pPage->m_pUserData;
   DWORD dwRenderShard = pv->m_OSINFO.dwRenderShard;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         // set all the edit windows
         WCHAR szTemp[32];
         DWORD i;
         PLEAFTYPE plt  = (PLEAFTYPE) pv->m_lLEAFTYPE.Get(0);
         for (i = 0; i < pv->m_lLEAFTYPE.Num(); i++, plt++) {
            swprintf (szTemp, L"ew:%d", (int) i);
            DoubleToControl (pPage, szTemp, plt->fWeight);
         }

         PCEscControl pControl;
         fp f;
         f = log10 (pv->m_fThickScale * 1000.0) * 50.0;
         f = max(0, f);
         f = min(99, f);
         pControl = pPage->ControlFind (L"thickscale");
         if (pControl)
            pControl->AttribSetInt (Pos(), (int)f);
         pControl = pPage->ControlFind (L"basethick");
         if (pControl)
            pControl->AttribSetInt (Pos(), (int)(pv->m_fBaseThick * 100.0));
         pControl = pPage->ControlFind (L"rootthick");
         if (pControl)
            pControl->AttribSetInt (Pos(), (int)(pv->m_fRootThick * 100.0));

         ComboBoxSet (pPage, L"divide", pv->m_dwDivide);

         pControl = pPage->ControlFind (L"round");
         if (pControl)
            pControl->AttribSetBOOL (Checked(), pv->m_fRound);
         pControl = pPage->ControlFind (L"cap");
         if (pControl)
            pControl->AttribSetBOOL (Checked(), pv->m_fCap);
         pControl = pPage->ControlFind (L"displowdetail");
         if (pControl)
            pControl->AttribSetBOOL (Checked(), pv->m_fDispLowDetail);
         pControl = pPage->ControlFind (L"roots");
         if (pControl)
            pControl->AttribSetBOOL (Checked(), pv->m_fRoots);

         DoubleToControl (pPage, L"texturewrap", (fp) pv->m_dwTextureWrap);
         MeasureToString (pPage, L"tapRoot", pv->m_fTapRoot);
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
         if (!_wcsicmp(psz, L"divide")) {
            DWORD dwVal = p->pszName ? (DWORD)_wtoi(p->pszName) : 0;
            if (dwVal == pv->m_dwDivide)
               return TRUE;

            if (pv->m_pWorld)
               pv->m_pWorld->ObjectAboutToChange (pv);
            pv->m_dwDivide = dwVal;
            pv->m_fDirty = TRUE;
            if (pv->m_pWorld)
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
         if (!p->pControl->m_pszName)
            break;

         if (!_wcsicmp(p->pControl->m_pszName, L"thickscale")) {
            fp fVal = (fp) p->iPos;
            fVal =  pow (10.0, fVal / 50.0) / 1000.0;
            // inverse: f = log10 (pv->m_fThickScale * 1000.0) * 20.0;
            if (fVal == pv->m_fThickScale)
               return TRUE; // no change

            if (pv->m_pWorld)
               pv->m_pWorld->ObjectAboutToChange (pv);
            pv->m_fThickScale = fVal;
            pv->m_fDirty = TRUE;
            if (pv->m_pWorld)
               pv->m_pWorld->ObjectChanged (pv);
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"basethick")) {
            fp fVal = (fp) p->iPos / 100.0;
            if (fVal == pv->m_fBaseThick)
               return TRUE; // no change

            if (pv->m_pWorld)
               pv->m_pWorld->ObjectAboutToChange (pv);
            pv->m_fBaseThick = fVal;
            pv->m_fDirty = TRUE;
            if (pv->m_pWorld)
               pv->m_pWorld->ObjectChanged (pv);
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"rootthick")) {
            fp fVal = (fp) p->iPos / 100.0;
            if (fVal == pv->m_fRootThick)
               return TRUE; // no change

            if (pv->m_pWorld)
               pv->m_pWorld->ObjectAboutToChange (pv);
            pv->m_fRootThick = fVal;
            pv->m_fDirty = TRUE;
            if (pv->m_pWorld)
               pv->m_pWorld->ObjectChanged (pv);
            return TRUE;
         }
      }
      break;


   case ESCM_LINK:
      {
         PESCMLINK p = (PESCMLINK) pParam;
         if (!p->psz)
            break;

         if ((p->psz[0] == L'c') && (p->psz[1] == L'p') && (p->psz[2] == L':')) {
            DWORD dwNum = _wtoi(p->psz + 3);
            PLEAFTYPE plt = (PLEAFTYPE) pv->m_lLEAFTYPE.Get(dwNum);

            if (pv->m_pWorld)
               pv->m_pWorld->ObjectAboutToChange (pv);
            if (plt->pClone)
               plt->pClone->Release();
            pv->m_lLEAFTYPE.Remove (dwNum);
            pv->m_pRoot->RemoveLeaf (dwNum);
            pv->m_fDirty = TRUE;
            if (pv->m_pWorld)
               pv->m_pWorld->ObjectChanged (pv);
            pPage->Exit (RedoSamePage());
            return TRUE;
         }
      }
      break;

   case ESCN_EDITCHANGE:
      {
         PESCNEDITCHANGE p = (PESCNEDITCHANGE) pParam;

         if (!p->pControl || !p->pControl->m_pszName)
            break;

         if (pv->m_pWorld)
            pv->m_pWorld->ObjectAboutToChange (pv);
         // since all out edit controls
         DWORD i;
         WCHAR szTemp[32];
         PLEAFTYPE plt  = (PLEAFTYPE) pv->m_lLEAFTYPE.Get(0);
         for (i = 0; i < pv->m_lLEAFTYPE.Num(); i++, plt++) {
            swprintf (szTemp, L"ew:%d", (int) i);
            plt->fWeight = DoubleFromControl (pPage, szTemp);
            plt->fWeight = max(plt->fWeight, 0);
         }
         pv->m_dwTextureWrap = (DWORD) DoubleFromControl (pPage, L"texturewrap");
         MeasureParseString (pPage, L"tapRoot", &pv->m_fTapRoot);
         pv->m_fTapRoot = max(pv->m_fTapRoot, CLOSE);
         pv->m_fDirty = TRUE;

         if (pv->m_pWorld)
            pv->m_pWorld->ObjectChanged (pv);
      }
      break;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;
         PWSTR psz;
         psz = p->pControl->m_pszName;
         if (!psz)
            break;

         if (!_wcsicmp(psz, L"round")) {
            if (pv->m_pWorld)
               pv->m_pWorld->ObjectAboutToChange (pv);
            pv->m_fRound = p->pControl->AttribGetBOOL (Checked());
            pv->m_fDirty = TRUE;
            if (pv->m_pWorld)
               pv->m_pWorld->ObjectChanged (pv);
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"roots")) {
            if (pv->m_pWorld)
               pv->m_pWorld->ObjectAboutToChange (pv);
            pv->m_fRoots = p->pControl->AttribGetBOOL (Checked());
            pv->m_fDirty = TRUE;
            if (pv->m_pWorld)
               pv->m_pWorld->ObjectChanged (pv);
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"displowdetail")) {
            if (pv->m_pWorld)
               pv->m_pWorld->ObjectAboutToChange (pv);
            pv->m_fDispLowDetail = p->pControl->AttribGetBOOL (Checked());
            pv->m_fDirty = TRUE;
            if (pv->m_pWorld)
               pv->m_pWorld->ObjectChanged (pv);
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"cap")) {
            if (pv->m_pWorld)
               pv->m_pWorld->ObjectAboutToChange (pv);
            pv->m_fCap = p->pControl->AttribGetBOOL (Checked());
            pv->m_fDirty = TRUE;
            if (pv->m_pWorld)
               pv->m_pWorld->ObjectChanged (pv);
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"add")) {
            LEAFTYPE ct;
            memset (&ct,0 ,sizeof(ct));
            ct.fWeight = 1;
            if (!ObjectCFNewDialog (dwRenderShard, pPage->m_pWindow->m_hWnd, &ct.gCode, &ct.gSub))
               return TRUE;

            // see if there's a match already
            PLEAFTYPE pct;
            pct = (PLEAFTYPE) pv->m_lLEAFTYPE.Get(0);
            DWORD i;
            for (i = 0; i < pv->m_lLEAFTYPE.Num(); i++, pct++)
               if (IsEqualGUID(ct.gCode, pct->gCode) && IsEqualGUID(ct.gSub, pct->gSub))
                  break;
            if (i < pv->m_lLEAFTYPE.Num()) {
               pPage->MBInformation (L"The leaf or sub-branch is already in the list.");
               return TRUE;
            }

            
            if (pv->m_pWorld)
               pv->m_pWorld->ObjectAboutToChange (pv);

            LEAFTYPEFill (dwRenderShard, &ct);

            pv->m_lLEAFTYPE.Add (&ct);

            pv->m_fDirty = TRUE;
            if (pv->m_pWorld)
               pv->m_pWorld->ObjectChanged (pv);

            pPage->Exit (RedoSamePage());
            return TRUE;
         }
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Branch settings";
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"CANOPY")) {
            MemZero (&gMemTemp);

            DWORD i;
            PLEAFTYPE pct;
            HBITMAP *ph;
            COLORREF *pcr;
            pct = (PLEAFTYPE) pv->m_lLEAFTYPE.Get(0);
            ph = (HBITMAP*) pv->m_lDialogHBITMAP.Get(0);
            pcr = (COLORREF*) pv->m_lDialogCOLORREF.Get(0);

            MemCat (&gMemTemp, L"<table width=100%");
            MemCat (&gMemTemp, L" border=0 innerlines=0>");
            BOOL fNeedTr;
            fNeedTr = FALSE;
            for (i = 0; i < pv->m_lLEAFTYPE.Num(); i++, pct++, ph++, pcr++) {
               // get the name
               WCHAR szMajor[128], szMinor[128], szName[128];
               szName[0] = 0;
               ObjectCFNameFromGUIDs (dwRenderShard, &pct->gCode, &pct->gSub, szMajor, szMinor, szName);

               if (!(i % 2))
                  MemCat (&gMemTemp, L"<tr>");
               MemCat (&gMemTemp, L"<td>");
               MemCat (&gMemTemp, L"<p align=center>");

               // bitmap
               MemCat (&gMemTemp, L"<image hbitmap=");
               WCHAR szTemp[32];
               swprintf (szTemp, L"%lx", (__int64)ph[0]);
               MemCat (&gMemTemp, szTemp);
               if (pcr[0] != -1) {
                  MemCat (&gMemTemp, L" transparent=true transparentdistance=0 transparentcolor=");
                  ColorToAttrib (szTemp, pcr[0]);
                  MemCat (&gMemTemp, szTemp);
               }

               //MemCat (&gMemTemp, L" name=bitmap");
               //MemCat (&gMemTemp, (int) i+j);
               MemCat (&gMemTemp, L" border=0 href=cp:");
               MemCat (&gMemTemp, (int) i);
               MemCat (&gMemTemp, L"/>");

               MemCat (&gMemTemp, L"<br/>");

               MemCat (&gMemTemp, L"<a href=cp:");
               MemCat (&gMemTemp, (int) i);
               MemCat (&gMemTemp, L">");
               MemCatSanitize (&gMemTemp, szName);
               MemCat (&gMemTemp, L"</a>");

               MemCat (&gMemTemp, L"<br/><edit maxchars=32 width=50% name=ew:");
               MemCat (&gMemTemp, (int) i);
               MemCat (&gMemTemp, L"/>");

               MemCat (&gMemTemp, L"</p>");
               MemCat (&gMemTemp, L"</td>");
               if (i%2) {
                  MemCat (&gMemTemp, L"</tr>");
                  fNeedTr = FALSE;
               }
               else
                  fNeedTr = TRUE;
            }
            if (fNeedTr) {
               MemCat (&gMemTemp, L"<td/>");
               MemCat (&gMemTemp, L"</tr>");
            }
            MemCat (&gMemTemp, L"</table>");

            p->pszSubString = (PWSTR) gMemTemp.p;
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}

/**********************************************************************************
CObjectBranch::DialogShow - Standard function to ask for dialog to pop up.
*/
BOOL CObjectBranch::DialogShow (DWORD dwSurface, PCEscWindow pWindow, PCPoint pClick)
{
   PWSTR pszRet;
   m_lDialogHBITMAP.Init (sizeof(HBITMAP));
   m_lDialogCOLORREF.Init (sizeof(COLORREF));

redo:
   // create all the bitmaps
   m_lDialogHBITMAP.Clear();
   m_lDialogCOLORREF.Clear();
   PLEAFTYPE plt;
   plt = (PLEAFTYPE) m_lLEAFTYPE.Get(0);
   DWORD i;
   HBITMAP hBit;
   COLORREF cr;
   m_lDialogHBITMAP.Required (m_lLEAFTYPE.Num());
   m_lDialogCOLORREF.Required (m_lLEAFTYPE.Num());
   for (i = 0; i < m_lLEAFTYPE.Num(); i++, plt++) {
      hBit = Thumbnail (m_OSINFO.dwRenderShard, &plt->gCode, &plt->gSub, pWindow->m_hWnd, &cr);
      m_lDialogHBITMAP.Add (&hBit);
      m_lDialogCOLORREF.Add (&cr);
   }

   pszRet = pWindow->PageDialog (ghInstance, IDR_MMLBRANCH, BranchPage, this);

   // free up bitmaps
   HBITMAP *ph;
   ph = (HBITMAP*) m_lDialogHBITMAP.Get(0);
   for (i = 0; i < m_lDialogHBITMAP.Num(); i++, ph++)
      DeleteObject (*ph);

   if (!pszRet)
      return FALSE;
   if (!_wcsicmp(pszRet, RedoSamePage()))
      goto redo;
   if (!_wcsicmp(pszRet, L"autobranch")) {
redo2:
      // make sure have branch info
      AutoBranchFill();

      pszRet = pWindow->PageDialog (ghInstance, IDR_MMLBRANCHAUTO, BranchAutoPage, this);
      if (!pszRet)
         return FALSE;
      if (!_wcsicmp(pszRet, RedoSamePage()))
         goto redo2;
      if (!_wcsicmp(pszRet, Back()))
         goto redo;
      // fall through
   }

   return (!_wcsicmp(pszRet, Back()));
}


/**********************************************************************************
CObjectBranch::TextureQuery -
asks the object what textures it uses. This allows the save-function
to save custom textures into the file. The object just ADDS (doesn't
clear or remove) elements, which are two guids in a row: the
gCode followed by the gSub of the object. Of course, it may add more
than one texture
*/
BOOL CObjectBranch::TextureQuery (PCListFixed plText)
{
   // template
   BOOL fRet;
   fRet = CObjectTemplate::TextureQuery (plText);

   // each of sub objects
   DWORD i;
   PLEAFTYPE plt = (PLEAFTYPE) m_lLEAFTYPE.Get(0);
   for (i = 0; i < m_lLEAFTYPE.Num(); i++, plt++)
      if (plt->pClone)
         fRet |= plt->pClone->TextureQuery(plText);

   return fRet;
}



/**********************************************************************************
CObjectBranch::ColorQuery -
asks the object what colors it uses (exclusive of textures).
It adds elements to plColor, which is a list of COLORREF. It may
add more than one color
*/
BOOL CObjectBranch::ColorQuery (PCListFixed plColor)
{
   // template
   BOOL fRet;
   fRet = CObjectTemplate::ColorQuery (plColor);

   // each of sub objects
   DWORD i;
   PLEAFTYPE plt = (PLEAFTYPE) m_lLEAFTYPE.Get(0);
   for (i = 0; i < m_lLEAFTYPE.Num(); i++, plt++)
      if (plt->pClone)
         fRet |= plt->pClone->ColorQuery(plColor);

   return fRet;
}


/**********************************************************************************
CObjectBranch::ObjectClassQuery
asks the curent object what other objects (including itself) it requires
so that when a file is saved, all user objects will be saved along with
the file, so people on other machines can load them in.
The object just ADDS (doesn't clear or remove) elements, which are two
guids in a row: gCode followed by gSub of the object. All objects
must add at least on (their own). Some, like CObjectEditor, will add
all its sub-objects too
*/
BOOL CObjectBranch::ObjectClassQuery (PCListFixed plObj)
{
   // template
   BOOL fRet;
   fRet = CObjectTemplate::ObjectClassQuery (plObj);

   // each of sub objects
   DWORD i;
   PLEAFTYPE plt = (PLEAFTYPE) m_lLEAFTYPE.Get(0);
   for (i = 0; i < m_lLEAFTYPE.Num(); i++, plt++)
      if (plt->pClone)
         fRet |= plt->pClone->ObjectClassQuery(plObj);

   return fRet;
}

/**********************************************************************************
CObjectBranch::AutoBranchFill - If there's no m_pAutoBranch, this allocates the memory
for it and fils it. Otherwise it does nothing
*/
void CObjectBranch::AutoBranchFill (void)
{
   if (m_pAutoBranch)
      return;
   m_pAutoBranch = new AUTOBRANCH;
   if (!m_pAutoBranch)
      return;

   memset (m_pAutoBranch, 0, sizeof(AUTOBRANCH));

   m_pAutoBranch->fBranchInitialAngle = 0; // initial angle. 0..PI. 0 = up
   m_pAutoBranch->fBranchLength = .5; // typical length, meters
   m_pAutoBranch->fBranchLengthVar = .5; // variation in lenght. 0..1
   m_pAutoBranch->fBranchShorten = .8;   // amount shortens per generation. 0..1

   // branch extension
   m_pAutoBranch->fBranchExtendProb = 1;      // probability that will extend branch, 0..1
   m_pAutoBranch->fBranchExtendProbGen = .2;   // change in fBranchExtendProb per generation, 0..1
   m_pAutoBranch->fBranchDirVar = .2;          // variation in direction, 0..1

   // branch forking
   m_pAutoBranch->fBranchForkProb = 0;        // probability of branch forking, 0..1
   m_pAutoBranch->fBranchForkProbGen = .4;     // chance of forking increasing per generation, 0..1
   m_pAutoBranch->fBranchForkNum = .1;         // number of forks, 0..1.
   m_pAutoBranch->fBranchForkForwards = .7;    // how forwards. 1 = forwards, 0 = backwards
   m_pAutoBranch->fBranchForkUpDown = .7;      // how up/down. 1=up, 0=down
   m_pAutoBranch->fBranchForkVar = .3;         // variation in forking, 0..1.

   // branch gravity
   m_pAutoBranch->fBranchGrav = .6;            // 1=branchs will want to go upwards, 0=branch go downwards
   m_pAutoBranch->fBranchGravGen = .4;         // how much fBranchGrav changes each generation. 1 = increase, 0 = decrease

   // leaves along branch
   m_pAutoBranch->fLeafExtendProb = 0;        // 0..1, probability of leaf being along extension
   m_pAutoBranch->fLeafExtendProbGen = .2;     // 0..1, likelihood of leaves increasing
   m_pAutoBranch->fLeafExtendNum = .5;         // 0..1, number of leaves
   m_pAutoBranch->fLeafExtendForwards = .6;    // 0..1, 1 = forwards, 0 = backwards
   m_pAutoBranch->fLeafExtendVar = .25;         // 0..1, variation in leaf direction
   m_pAutoBranch->fLeafExtendScale = .5;       // 0..1, .5 = default scale
   m_pAutoBranch->fLeafExtendScaleVar = .2;    // 0..1, amount leaf size varies
   m_pAutoBranch->fLeafExtendScaleGen = .5;    // 0..1, how much leaf size changes, .5 = none, 1 = larger

   // leaf at terminal node
   m_pAutoBranch->fLeafEndProb = 1;           // 0..1, likelihood of leaf at end
   m_pAutoBranch->fLeafEndNum = .2;            // 0..1, number of leaves at the end
   m_pAutoBranch->fLeafEndNumVar = .5;         // 0..1, number of leaves at the end
   m_pAutoBranch->fLeafEndSymmetry = .3;       // 0..1, amount of symmetry in end leaves
   m_pAutoBranch->fLeafEndVar = .25;           // 0..1, variation in leaf location at end
   m_pAutoBranch->fLeafEndScale = .5;          // 0..1, .5 = default scale
   m_pAutoBranch->fLeafEndScaleVar = .2;       // 0..1, amount leaf size varies

   // misc
   m_pAutoBranch->dwSeed = 101;                 // seed to use
   m_pAutoBranch->dwMaxGen = 10;               // maximum generations
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

   pAB->fBranchLength *= pow((fp)pAB->fBranchShorten, (fp).25);
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
CObjectBranch::AutoBranchGenIndiv - Generates an individual branch.

inputs
   PCPoint        pDir - Direction vector. (Normalized)
   DWORD          dwMaxGen - Current maximum generations
   PAUTOBRANCH    pAB - Branch information to use. This is modified on a per-generation bases
returns
   PCBranchNode - New node to add. Returns NULL if error, or if meet dwMaxGen
*/
PCBranchNode CObjectBranch::AutoBranchGenIndiv (PCPoint pDir, DWORD dwMaxGen, PAUTOBRANCH pAB)
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
   CPoint pUp, pRight, pNewDir;
   CMatrix mToObject, mRot;
   pUp.Zero();
   pUp.p[2] = 1;
   pRight.CrossProd (pDir, &pUp);
   if (pRight.Length() < CLOSE) {
      pUp.Zero();
      pUp.p[0] = 1;  // another directon
      pRight.CrossProd (pDir, &pUp);
   }
   pUp.CrossProd (&pRight, pDir);
   pUp.Normalize();
   pRight.CrossProd (pDir, &pUp);
   pRight.Normalize();
   mToObject.RotationFromVectors (&pRight, pDir, &pUp);

   // create the forward-pointing branch
   fp f;
   BRANCHINFO bi;
   memset (&bi, 0, sizeof(bi));
   BOOL fBranchExtend;
   fBranchExtend = FALSE;
   if (randf(0,1) < pAB->fBranchExtendProb) {
      mRot.Rotation (
         randf(-pAB->fBranchDirVar * PI/2, pAB->fBranchDirVar * PI/2),
         0, // no point since just rotates right and left
         randf(-pAB->fBranchDirVar * PI/2, pAB->fBranchDirVar * PI/2)
         );
      mRot.MultiplyRight (&mToObject);
      pNewDir.Zero();
      pNewDir.p[1] = 1;
      pNewDir.MultiplyLeft (&mRot);
      pNewDir.p[2] += (pAB->fBranchGrav - .5);
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
            randf(-pAB->fBranchForkVar * PI/2, pAB->fBranchForkVar * PI/2),
            randf(-pAB->fBranchForkVar * PI/2, pAB->fBranchForkVar * PI/2),
            randf(-pAB->fBranchForkVar * PI/2, pAB->fBranchForkVar * PI/2)
            );
         mRot.MultiplyRight (&mToObject);
         pNewDir.MultiplyLeft (&mRot);

         // forking likely to up or down
         pNewDir.p[2] += (pAB->fBranchForkUpDown - .5);
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
            randf(-pAB->fLeafExtendVar * PI/2, pAB->fLeafExtendVar * PI/2),
            randf(-pAB->fLeafExtendVar * PI/2, pAB->fLeafExtendVar * PI/2),
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
         if (!m_lLEAFTYPE.Num())
            continue;   // no leaves
         li.dwID = rand() % m_lLEAFTYPE.Num();
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
            randf(-pAB->fLeafEndVar * PI/2, pAB->fLeafEndVar * PI/2),
            randf(-pAB->fLeafEndVar * PI/2, pAB->fLeafEndVar * PI/2),
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
         if (!m_lLEAFTYPE.Num())
            continue;   // no leaves
         li.dwID = rand() % m_lLEAFTYPE.Num();
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
CObjectBranch::AutoBranchGen - Generates a tree/branch automatically from
the parameters in m_pAutoBranch
*/
void CObjectBranch::AutoBranchGen (void)
{
   // seed
   srand (m_pAutoBranch->dwSeed);
   m_pAutoBranch->dwMaxGen = max(m_pAutoBranch->dwMaxGen, 1);

   // figure out the initial direction
   CPoint pDir;
   pDir.Zero();
   pDir.p[2] = cos (m_pAutoBranch->fBranchInitialAngle);
   pDir.p[1] = sin (m_pAutoBranch->fBranchInitialAngle);

   // delete existing branches
   if (m_pRoot)
      delete m_pRoot;

   // go for it
   m_pRoot = AutoBranchGenIndiv (&pDir, m_pAutoBranch->dwMaxGen + 1, m_pAutoBranch);
   
   m_fDirty = TRUE;  // set dirty
}




