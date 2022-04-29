/************************************************************************
CForest.cpp - Code for handling a forest.

begun 6/3/03 by Mike Rozak
Copyright 2003 Mike Rozak. All rights reserved
*/

#include <windows.h>
#include "escarpment.h"
#include "..\M3D.h"
#include "resource.h"


typedef struct {
   DWORD          dwType;     // index into m_lCANOPYTREE
   CMatrix        mMatrix;    // location, rotation, and scale matrix
   TEXTUREPOINT   tpLoc;      // location of the trunk in meters. Used for fast access to enumerator
   BYTE           bScore;     // random number from 0..255 for each tree
} TREEINFO, *PTREEINFO;


/*************************************************************************
CForestCanopy::Constructor and destructor */
CForestCanopy::CForestCanopy()
{
   m_fDirty = TRUE;
   m_lCANOPYTREE.Init (sizeof(CANOPYTREE));
   m_tpSeparation.h = m_tpSeparation.v = 10.0;  // 10m
   m_tpSeparationVar.h = m_tpSeparationVar.v = 1;  // totall random
   m_pRotationVar.Zero();
   m_pRotationVar.p[2] = 1;   // any angle
   m_pRotationVar.p[0] = m_pRotationVar.p[1] = .05;
   m_pScaleVar.Zero4();
   m_pScaleVar.p[3] = .3;
   m_dwRepeatX = m_dwRepeatY = 100;
   m_dwSeed = 1;
   m_fNoShadows = FALSE;
   m_fLowDensity = TRUE;
   m_lPCObjectClone.Init (sizeof(PCObjectClone));
}

CForestCanopy::~CForestCanopy()
{
   // free up the clones
   PCObjectClone *ppc = (PCObjectClone*) m_lPCObjectClone.Get(0);
   DWORD i;
   for (i = 0; i < m_lPCObjectClone.Num(); i++)
      if (ppc[i])
         ppc[i]->Release();
   m_lPCObjectClone.Clear();
}

/*************************************************************************
CForestCanopy::Clone - Creates a copy of the forest canopy.
*/
CForestCanopy *CForestCanopy::Clone (void)
{
   PCForestCanopy pNew = new CForestCanopy;
   if (!pNew)
      return NULL;

   pNew->m_lCANOPYTREE.Init (sizeof(CANOPYTREE), m_lCANOPYTREE.Get(0), m_lCANOPYTREE.Num());
   pNew->m_tpSeparation = m_tpSeparation;
   pNew->m_tpSeparationVar = m_tpSeparationVar;
   pNew->m_pRotationVar.Copy (&m_pRotationVar);
   pNew->m_pScaleVar.Copy (&m_pScaleVar);
   pNew->m_dwRepeatX = m_dwRepeatX;
   pNew->m_dwRepeatY = m_dwRepeatY;
   pNew->m_dwSeed = m_dwSeed;
   pNew->m_fNoShadows = m_fNoShadows;
   pNew->m_fLowDensity = m_fLowDensity;
   pNew->DirtySet();
   // dont bother with automatically calculated stuff
   return pNew;

}


/*************************************************************************
CForestCanopy::TextureQuery - From CObjectSocket
*/
BOOL CForestCanopy::TextureQuery (DWORD dwRenderShard, PCListFixed plText)
{
   CalcTrees(dwRenderShard);

   DWORD i;
   BOOL fRet = FALSE;
   PCObjectClone *ppc = (PCObjectClone*) m_lPCObjectClone.Get(0);
   for (i = 0; i < m_lPCObjectClone.Num(); i++)
      fRet |= ppc[i]->TextureQuery (plText);
   return fRet;
}

/*************************************************************************
CForestCanopy::ColorQuery - From CObjectSocket
*/
BOOL CForestCanopy::ColorQuery (DWORD dwRenderShard, PCListFixed plColor)
{
   CalcTrees(dwRenderShard);

   DWORD i;
   BOOL fRet = FALSE;
   PCObjectClone *ppc = (PCObjectClone*) m_lPCObjectClone.Get(0);
   for (i = 0; i < m_lPCObjectClone.Num(); i++)
      fRet |= ppc[i]->ColorQuery (plColor);
   return fRet;
}

/*************************************************************************
CForestCanopy::ObjectClassQuery - From CObjectSocket
*/
BOOL CForestCanopy::ObjectClassQuery (DWORD dwRenderShard, PCListFixed plObj)
{
   CalcTrees(dwRenderShard);

   DWORD i;
   BOOL fRet = FALSE;
   PCObjectClone *ppc = (PCObjectClone*) m_lPCObjectClone.Get(0);
   for (i = 0; i < m_lPCObjectClone.Num(); i++)
      fRet |= ppc[i]->ObjectClassQuery (plObj);
   return fRet;
}

/*************************************************************************
CForestCanopy::DirtySet - Call this after any of the public member variables
of the canopy have been changed so that internal calculations can be redone.
*/
void CForestCanopy::DirtySet (void)
{
   m_fDirty = TRUE;
}


/*************************************************************************
CForestCanopy::TreeClone - Returns a pointer to the PCObjectClone for the
tree of given type (from 0..m_lCANOPYTREE.Num()-1).

inputs
   DWORD       dwType - 0 .. m_lCANOPYTREE.Num()-1
returns
   PCObjectClone - Clone object. Do not bother calling Release() because no
   extra addref was called.
*/
PCObjectClone CForestCanopy::TreeClone (DWORD dwRenderShard, DWORD dwType)
{
   if (!CalcTrees (dwRenderShard))
      return NULL;

   if (dwType >= m_lPCObjectClone.Num())
      return NULL;

   PCObjectClone *pcc;
   pcc = (PCObjectClone*) m_lPCObjectClone.Get(0);
   return pcc[dwType];
}


/*************************************************************************
CForestCanopy::CalcTrees - If the dirty flag is set this will relcalcualte
the locations of all the trees.

returns
   BOOL - TRUE if success. FALSE if error, such as no trees
*/
BOOL CForestCanopy::CalcTrees (DWORD dwRenderShard)
{
   if (!m_fDirty)
      return TRUE;

   if (!m_lCANOPYTREE.Num())
      return FALSE;  // no trees so doesnt matter

   // free up old tree clones and load new ones
   CListFixed lOld;
   lOld.Init (sizeof(PCObjectClone), m_lPCObjectClone.Get(0), m_lPCObjectClone.Num());

   m_lPCObjectClone.Clear();
   DWORD i;
   PCANOPYTREE pc;
   PCObjectClone pClone;
   pc = (PCANOPYTREE) m_lCANOPYTREE.Get(0);
   m_lPCObjectClone.Required ( m_lCANOPYTREE.Num());
   for (i = 0; i < m_lCANOPYTREE.Num(); i++, pc++) {
      pClone = ObjectCloneGet (dwRenderShard, &pc->gCode, &pc->gSub);
      m_lPCObjectClone.Add (&pClone);
   }

   // free up old
   PCObjectClone *pcc;
   pcc = (PCObjectClone*) lOld.Get(0);
   for (i = 0; i < lOld.Num(); i++)
      if (pcc[i])
         pcc[i]->Release();

   // make sure values are within limits
   m_tpSeparation.h = max(m_tpSeparation.h, CLOSE);
   m_tpSeparation.v = max(m_tpSeparation.v, CLOSE);
   for (i = 0; i < 4; i++) {
      m_pRotationVar.p[i] = max(m_pRotationVar.p[i], 0);
      m_pRotationVar.p[i] = min(m_pRotationVar.p[i], 1);

      m_pScaleVar.p[i] = max(m_pScaleVar.p[i], 0);
      m_pScaleVar.p[i] = min(m_pScaleVar.p[i], .99);  // since 1 would cause extremes
   }
   m_dwRepeatX = max(m_dwRepeatX, 1);
   m_dwRepeatY = max(m_dwRepeatY, 1);

   // allocate enough memory
   if (!m_memTREEINFO.Required (m_dwRepeatX * m_dwRepeatY * sizeof(TREEINFO)))
      return FALSE;  // error
   PTREEINFO pti;
   pti = (PTREEINFO) m_memTREEINFO.p;

   // total count of # of trees
   DWORD dwCount;
   dwCount = 0;
   pc = (PCANOPYTREE) m_lCANOPYTREE.Get(0);
   for (i = 0; i < m_lCANOPYTREE.Num(); i++) {
      pc[i].dwWeight = max(pc[i].dwWeight,1);
      dwCount += pc[i].dwWeight;
   }

   // seed the random
   srand (m_dwSeed);

   // fill it in
   DWORD x,y, dwWant;
   TEXTUREPOINT tpSep;
   CPoint pRot;
   tpSep.h = m_tpSeparationVar.h / 2;
   tpSep.v = m_tpSeparationVar.v / 2;
   pRot.Copy (&m_pRotationVar);
   pRot.Scale (PI);
   for (y = 0; y < m_dwRepeatY; y++) for (x = 0; x < m_dwRepeatX; x++, pti++) {
      dwWant = (DWORD) rand() % dwCount;
      for (i = 0; i < m_lCANOPYTREE.Num(); i++) {
         if (dwWant < pc[i].dwWeight)
            break;
         dwWant -= pc[i].dwWeight;
      }
      pti->dwType = i;

      pti->bScore = (BYTE)rand();

      // what's the default location... note only includes delta, not offset due to x and y
      pti->tpLoc.h = randf(tpSep.h, -tpSep.h) * m_tpSeparation.h;
      pti->tpLoc.v = randf(tpSep.v, -tpSep.v) * m_tpSeparation.v;

      // figure out rotation, etc.
      CMatrix mScale, mRot;
      fp fTotal;
      fTotal = randf(1 - m_pScaleVar.p[3], 1 + m_pScaleVar.p[3]);
      mScale.Scale (
         fTotal * randf(1 - m_pScaleVar.p[0], 1 + m_pScaleVar.p[0]),
         fTotal * randf(1 - m_pScaleVar.p[1], 1 + m_pScaleVar.p[1]),
         fTotal * randf(1 - m_pScaleVar.p[2], 1 + m_pScaleVar.p[2]) );

      mRot.Rotation (
         randf(-pRot.p[0], pRot.p[0]),
         randf(-pRot.p[1], pRot.p[1]),
         randf(-pRot.p[2], pRot.p[2]));


      // combine
      pti->mMatrix.Multiply (&mRot, &mScale);

      // NOTE: Not putting trnanslation into mMatrix since will be added later by caller
      // to EnumTree ()
   }

   // calculate the maximum size that a tree will be so can use this when calculating
   // bounding boxes
   m_fMaxSize = 0;
   pcc = (PCObjectClone*) m_lPCObjectClone.Get(0);
   for (i = 0; i < m_lCANOPYTREE.Num(); i++) {
      pClone = pcc[i];
      m_fMaxSize = max(m_fMaxSize, pClone->MaxSize());
   }
   // include scale in max size
   m_fMaxSize *= (1.0 + m_pScaleVar.p[3]) *
      (1.0 + max(max(m_pScaleVar.p[0], m_pScaleVar.p[1]), m_pScaleVar.p[2]) );

   m_fDirty = FALSE;
   return TRUE;
}

/*************************************************************************
CForestCanopy::TreesInRange - Given two points for opposite corners of a rectangle
in space, this fills in a rectangle with tree x and y indecies that might appear
within the rectangle.

inputs
   PCPoint     pCorner1, pCorner2 - Two opposite corners of rectangle (in meters)
   RECT        *pr - Filled with tree x and y indecies (to be passed in to EnumTree())
                  for the minimum and maximum. Note: Because of dislocations some of
                  these trees might be off the rectangle, so some extra checking should
                  be done.
returns
   none
*/
void CForestCanopy::TreesInRange (DWORD dwRenderShard, PCPoint pCorner1, PCPoint pCorner2, RECT *pr)
{
   if (!CalcTrees(dwRenderShard)) {
      pr->left = pr->right = pr->top = pr->bottom = 0;
      return;
   }

   CPoint pMin, pMax;
   pMin.Copy (pCorner1);
   pMax.Copy (pCorner1);
   pMin.Min (pCorner2);
   pMax.Max (pCorner2);
   pMin.p[0] /= m_tpSeparation.h;
   pMax.p[0] /= m_tpSeparation.h;
   pMin.p[1] /= m_tpSeparation.v;
   pMax.p[1] /= m_tpSeparation.v;

   pr->left = (int)floor(pMin.p[0] - .5);
   pr->top = (int)floor(pMin.p[1] - .5);
   pr->right = (int)ceil(pMax.p[0] + .5);
   pr->bottom = (int)ceil(pMax.p[1] + .5);
}


/*************************************************************************
CForestCanopy::EnumTree - Given an x and y location, this fills in a
matrix which translates from the x and y to the tree.

inputs
   int         x, y - Location in the grid. Note: This can be negative or exceed
                     m_dwRepeatX and m_dwRepeatY because modulo will be used
   DWORD       dwType - Type of tree. Index from 0..m_lCANOPYTREE.Num().
                     If the tree at the given location is not of this type then returns FALSE
   PCMatrix    pMatrix - Filled in with the rotation, and scaling matrix. This does NOT
                  include tranlation information - which is stored in pLoc.
   PCPoint     pLoc - Filled in with the location of the tree.
   BYTE        *pScore - Filled in with the score of the tree
   BOOL        *pfIfDistantEliminate - This is for low density trees, and causes 3/4 of
               all trees to be eliminated if distant
returns
   BOOl - TRUE if found a tree there and matrix is filled in, FALSE if no tree.
*/
BOOL CForestCanopy::EnumTree (DWORD dwRenderShard, int x, int y, DWORD dwType, PCMatrix pMatrix, PCPoint pLoc, BYTE *pScore,
                              BOOL *pfIfDistantEliminate)
{
   int iXOrig = x, iYOrig = y;
   *pfIfDistantEliminate = FALSE;
   if (!CalcTrees(dwRenderShard))
      return FALSE;  // no trees to use

   // modulo
   if (x < 0)  // since mod is broken for negative numbers in c
      x += (1 - x / (int)m_dwRepeatX) * (int)m_dwRepeatX;
   if (y < 0)
      y += (1 - y / (int)m_dwRepeatY) * (int)m_dwRepeatY;
   x = x % (int)m_dwRepeatX;
   y = y % (int)m_dwRepeatY;

   *pfIfDistantEliminate = m_fLowDensity && ((x%2) || (y%2));

   // see what's there
   PTREEINFO pti;
   pti = (PTREEINFO) m_memTREEINFO.p;
   pti += (x + y * (int)m_dwRepeatX);
   if (pti->dwType != dwType)
      return FALSE;

   // else have this
   pMatrix->Copy (&pti->mMatrix);
   pLoc->Zero();
   pLoc->p[0] = (fp)iXOrig * m_tpSeparation.h + pti->tpLoc.h;
   pLoc->p[1] = (fp)iYOrig * m_tpSeparation.v + pti->tpLoc.v;

   *pScore = pti->bScore;

   return TRUE;
}


/*************************************************************************
CForestCanopy::MaxTreeSize - Returns the largest tree size that will have.
Use this to put limits on the bounding box for sub-sections of the ground.
*/
fp CForestCanopy::MaxTreeSize (DWORD dwRenderShard)
{
   if (!CalcTrees(dwRenderShard))
      return 0;

   return m_fMaxSize;
}



static PWSTR gpszCanopy = L"Canopy";
static PWSTR gpszSeparation = L"Separation";
static PWSTR gpszSeparationVar = L"SeparationVar";
static PWSTR gpszRotationVar = L"RotationVar";
static PWSTR gpszScaleVar = L"ScaleVar";
static PWSTR gpszRepeatX = L"RepeatX";
static PWSTR gpszRepeatY = L"RepeatY";
static PWSTR gpszSeed = L"Seed";
static PWSTR gpszTree = L"Tree";
static PWSTR gpszCode = L"Code";
static PWSTR gpszSub = L"Sub";
static PWSTR gpszWeight = L"Weight";
static PWSTR gpszNoShadows = L"NoShadows";
static PWSTR gpszLowDensity = L"LowDensity";

/*************************************************************************
CForestCanopy::MMLTo - Fills a MMLNode describing the canopy
*/
PCMMLNode2 CForestCanopy::MMLTo (void)
{
   PCMMLNode2 pNode = new CMMLNode2;
   if (!pNode)
      return NULL;
   pNode->NameSet (gpszCanopy);


   MMLValueSet (pNode, gpszSeparation, &m_tpSeparation);
   MMLValueSet (pNode, gpszSeparationVar, &m_tpSeparationVar);
   MMLValueSet (pNode, gpszRotationVar, &m_pRotationVar);
   MMLValueSet (pNode, gpszScaleVar, &m_pScaleVar);
   MMLValueSet (pNode, gpszRepeatX, (int) m_dwRepeatX);
   MMLValueSet (pNode, gpszRepeatY, (int) m_dwRepeatY);
   MMLValueSet (pNode, gpszSeed, (int)m_dwSeed);
   MMLValueSet (pNode, gpszNoShadows, (int)m_fNoShadows);
   MMLValueSet (pNode, gpszLowDensity, (int)m_fLowDensity);

   DWORD i;
   PCANOPYTREE pt;
   pt = (PCANOPYTREE) m_lCANOPYTREE.Get(0);
   for (i = 0; i < m_lCANOPYTREE.Num(); i++, pt++) {
      PCMMLNode2 pSub = new CMMLNode2;
      if (!pSub)
         continue;
      pSub->NameSet (gpszTree);

      MMLValueSet (pSub, gpszCode, (PBYTE) &pt->gCode, sizeof(pt->gCode));
      MMLValueSet (pSub, gpszSub, (PBYTE) &pt->gSub, sizeof(pt->gSub));
      MMLValueSet (pSub, gpszWeight, (int)pt->dwWeight);

      pNode->ContentAdd (pSub);
   }


   return pNode;
}

/*************************************************************************
CForestCanopy::MMLFrom - Fills in a canopy based on the mml
*/
BOOL CForestCanopy::MMLFrom (PCMMLNode2 pNode)
{
   m_lCANOPYTREE.Clear();

   MMLValueGetTEXTUREPOINT (pNode, gpszSeparation, &m_tpSeparation);
   MMLValueGetTEXTUREPOINT (pNode, gpszSeparationVar, &m_tpSeparationVar);
   MMLValueGetPoint (pNode, gpszRotationVar, &m_pRotationVar);
   MMLValueGetPoint (pNode, gpszScaleVar, &m_pScaleVar);
   m_dwRepeatX = (DWORD) MMLValueGetInt (pNode, gpszRepeatX, (int) 1);
   m_dwRepeatY = (DWORD) MMLValueGetInt (pNode, gpszRepeatY, (int) 1);
   m_dwSeed = (DWORD) MMLValueGetInt (pNode, gpszSeed, (int)1);
   m_fNoShadows = (BOOL) MMLValueGetInt (pNode, gpszNoShadows, FALSE);
   m_fLowDensity = (BOOL) MMLValueGetInt (pNode, gpszLowDensity, TRUE);

   DWORD i;
   PCMMLNode2 pSub;
   PWSTR psz;
   CANOPYTREE ct;
   for (i = 0; i < pNode->ContentNum(); i++) {
      pSub = NULL;
      pNode->ContentEnum (i, &psz, &pSub);
      if (!pSub)
         continue;
      psz = pSub->NameGet();
      if (!psz)
         continue;

      if (!_wcsicmp(psz, gpszTree)) {
         MMLValueGetBinary (pSub, gpszCode, (PBYTE) &ct.gCode, sizeof(ct.gCode));
         MMLValueGetBinary (pSub, gpszSub, (PBYTE) &ct.gSub, sizeof(ct.gSub));
         ct.dwWeight = (DWORD)MMLValueGetInt (pSub, gpszWeight, 1);

         m_lCANOPYTREE.Add (&ct);
      }
   }

   DirtySet();
   return TRUE;
}



/****************************************************************************
GroundForestCanopyPage
*/
BOOL GroundForestCanopyPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCForestCanopy pv = (PCForestCanopy)pPage->m_pUserData;
   DWORD dwRenderShard = pv->m_dwRenderShardTemp;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         MeasureToString (pPage, L"separation0", pv->m_tpSeparation.h);
         MeasureToString (pPage, L"separation1", pv->m_tpSeparation.v);
         DoubleToControl (pPage, L"repeatx", pv->m_dwRepeatX);
         DoubleToControl (pPage, L"repeaty", pv->m_dwRepeatY);
         DoubleToControl (pPage, L"seed", pv->m_dwSeed);

         PCEscControl pControl;
         pControl = pPage->ControlFind (L"separationvar0");
         if (pControl)
            pControl->AttribSetInt (Pos(), (int)(pv->m_tpSeparationVar.h * 100));
         pControl = pPage->ControlFind (L"separationvar1");
         if (pControl)
            pControl->AttribSetInt (Pos(), (int)(pv->m_tpSeparationVar.v * 100));

         pControl = pPage->ControlFind (L"noshadows");
         if (pControl)
            pControl->AttribSetBOOL (Checked(), pv->m_fNoShadows);

         pControl = pPage->ControlFind (L"lowdensity");
         if (pControl)
            pControl->AttribSetBOOL (Checked(), pv->m_fLowDensity);

         WCHAR szTemp[32];
         DWORD i;
         for (i = 0; i < 4; i++) {
            swprintf (szTemp, L"scalevar%d", (int) i);
            pControl = pPage->ControlFind (szTemp);
            if (pControl)
               pControl->AttribSetInt (Pos(), (int)(pv->m_pScaleVar.p[i] * 100));
            if (i >= 3)
               break;   // dont do rotation var=3

            swprintf (szTemp, L"rotationvar%d", (int) i);
            pControl = pPage->ControlFind (szTemp);
            if (pControl)
               pControl->AttribSetInt (Pos(), (int)(pv->m_pRotationVar.p[i] * 100));
         }
      }
      break;

   case ESCN_SCROLL:
   // take out because too slow - case ESCN_SCROLLING:
      {
         PESCNSCROLL p = (PESCNSCROLL) pParam;

         // only do one scroll bar
         if (!p->pControl->m_pszName)
            break;

         // just get all the values
         PCEscControl pControl;
         pControl = pPage->ControlFind (L"separationvar0");
         if (pControl)
            pv->m_tpSeparationVar.h = (fp) pControl->AttribGetInt (Pos()) / 100.0;
         pControl = pPage->ControlFind (L"separationvar1");
         if (pControl)
            pv->m_tpSeparationVar.v = (fp) pControl->AttribGetInt (Pos()) / 100.0;

         WCHAR szTemp[32];
         DWORD i;
         for (i = 0; i < 4; i++) {
            swprintf (szTemp, L"scalevar%d", (int) i);
            pControl = pPage->ControlFind (szTemp);
            if (pControl)
               pv->m_pScaleVar.p[i] = (fp) pControl->AttribGetInt (Pos()) / 100.0;
            if (i >= 3)
               break;   // dont do rotation var=3

            swprintf (szTemp, L"rotationvar%d", (int) i);
            pControl = pPage->ControlFind (szTemp);
            if (pControl)
               pv->m_pRotationVar.p[i] = (fp) pControl->AttribGetInt (Pos()) / 100.0;
         }

         pv->DirtySet();
      }
      break;

   case ESCM_LINK:
      {
         PESCMLINK p = (PESCMLINK) pParam;
         if (!p->psz)
            break;

         if ((p->psz[0] == L'c') && (p->psz[1] == L'p') && (p->psz[2] == L':')) {
            DWORD dwNum = _wtoi(p->psz + 3);
            PCANOPYTREE pt = (PCANOPYTREE) pv->m_lCANOPYTREE.Get(dwNum);
            pv->DirtySet();
            if (pt->dwWeight >= 2)
               pt->dwWeight--;
            else
               pv->m_lCANOPYTREE.Remove (dwNum);
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

         // since all out edit controls
         pv->DirtySet();
         fp fTemp;
         MeasureParseString (pPage, L"separation0", &fTemp);
         pv->m_tpSeparation.h = fTemp;
         pv->m_tpSeparation.h = max (pv->m_tpSeparation.h, CLOSE);
         MeasureParseString (pPage, L"separation1", &fTemp);
         pv->m_tpSeparation.v = fTemp;
         pv->m_tpSeparation.v = max (pv->m_tpSeparation.v, CLOSE);

         pv->m_dwRepeatX = (DWORD) DoubleFromControl (pPage, L"repeatx");
         pv->m_dwRepeatX = max(pv->m_dwRepeatX, 1);

         pv->m_dwRepeatY = (DWORD) DoubleFromControl (pPage, L"repeaty");
         pv->m_dwRepeatY = max(pv->m_dwRepeatY, 1);

         pv->m_dwSeed = (DWORD) DoubleFromControl (pPage, L"seed");
      }
      break;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;
         PWSTR psz;
         psz = p->pControl->m_pszName;

         if (!_wcsicmp (psz, L"noshadows")) {
            pv->m_fNoShadows = p->pControl->AttribGetBOOL (Checked());
            pv->DirtySet();
            return TRUE;
         }
         else if (!_wcsicmp (psz, L"lowdensity")) {
            pv->m_fLowDensity = p->pControl->AttribGetBOOL (Checked());
            pv->DirtySet();
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"add")) {
            CANOPYTREE ct;
            memset (&ct,0 ,sizeof(ct));
            ct.dwWeight = 1;
            if (!ObjectCFNewDialog (dwRenderShard, pPage->m_pWindow->m_hWnd, &ct.gCode, &ct.gSub))
               return TRUE;

            // see if there's a match already
            PCANOPYTREE pct;
            pct = (PCANOPYTREE) pv->m_lCANOPYTREE.Get(0);
            DWORD i;
            for (i = 0; i < pv->m_lCANOPYTREE.Num(); i++, pct++)
               if (IsEqualGUID(ct.gCode, pct->gCode) && IsEqualGUID(ct.gSub, pct->gSub))
                  break;
            if (i < pv->m_lCANOPYTREE.Num())
               pct->dwWeight++;
            else
               pv->m_lCANOPYTREE.Add (&ct);
            pv->DirtySet();

            pPage->Exit (RedoSamePage());
            return TRUE;
         }
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Canopy";
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"CANOPY")) {
            MemZero (&gMemTemp);

            DWORD i;
            PCANOPYTREE pct;
            HBITMAP *ph;
            COLORREF *pcr;
            pct = (PCANOPYTREE) pv->m_lCANOPYTREE.Get(0);
            ph = (HBITMAP*) pv->m_lDialogHBITMAP.Get(0);
            pcr = (COLORREF*) pv->m_lDialogCOLORREF.Get(0);

            MemCat (&gMemTemp, L"<table width=100%");
            MemCat (&gMemTemp, L" border=0 innerlines=0>");
            BOOL fNeedTr;
            fNeedTr = FALSE;
            for (i = 0; i < pv->m_lCANOPYTREE.Num(); i++, pct++, ph++, pcr++) {
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
               if (pct->dwWeight > 1) {
                  MemCat (&gMemTemp, L" <italic>(x ");
                  MemCat (&gMemTemp, (int) pct->dwWeight);
                  MemCat (&gMemTemp, L")</italic>");
               }

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



/*************************************************************************
CForestCanopy::Dialog - Brings up a dialog that allows a user to modify
the canopy.

inputs
   PCEscWindow       pWindow - Window to display it in
returns
   BOOL - TRUE if the user pressed back, FALSE if closed window
*/
BOOL CForestCanopy::Dialog (DWORD dwRenderShard, PCEscWindow pWindow)
{
   PWSTR pszRet;
   m_lDialogHBITMAP.Init (sizeof(HBITMAP));
   m_lDialogCOLORREF.Init (sizeof(COLORREF));

redo:
   // create all the bitmaps
   m_lDialogHBITMAP.Clear();
   m_lDialogCOLORREF.Clear();
   PCANOPYTREE pct;
   pct = (PCANOPYTREE) m_lCANOPYTREE.Get(0);
   DWORD i;
   HBITMAP hBit;
   COLORREF cr;
   m_lDialogHBITMAP.Required (m_lCANOPYTREE.Num());
   m_lDialogCOLORREF.Required (m_lCANOPYTREE.Num());
   for (i = 0; i < m_lCANOPYTREE.Num(); i++, pct++) {
      hBit = Thumbnail (dwRenderShard, &pct->gCode, &pct->gSub, pWindow->m_hWnd, &cr);
      m_lDialogHBITMAP.Add (&hBit);
      m_lDialogCOLORREF.Add (&cr);
   }

   m_dwRenderShardTemp = dwRenderShard;
   pszRet = pWindow->PageDialog (ghInstance, IDR_MMLGROUNDFORESTCANOPY, GroundForestCanopyPage, this);

   // free up bitmaps
   HBITMAP *ph;
   ph = (HBITMAP*) m_lDialogHBITMAP.Get(0);
   for (i = 0; i < m_lDialogHBITMAP.Num(); i++, ph++)
      DeleteObject (*ph);

   if (!pszRet)
      return FALSE;
   if (!_wcsicmp(pszRet, RedoSamePage()))
      goto redo;
   return (!_wcsicmp(pszRet, Back()));
}



/*************************************************************************
CForest::Constructor and destructor */
CForest::CForest (void)
{
   wcscpy (m_szName, L"Not named");
   m_cColor = RGB(0x40,0xff,0x40);

   DWORD i;
   for (i = 0; i < NUMFORESTCANOPIES; i++) {
      m_apCanopy[i] = new CForestCanopy;
      if (!m_apCanopy[i])
         break;
      m_apCanopy[i]->m_tpSeparation.h = m_apCanopy[i]->m_tpSeparation.v =
         10.0 / pow ((fp)3.1, (fp)i); // use 3.1 so doesn't repeat on larger scale
      m_apCanopy[i]->m_fNoShadows = (i >= NUMFORESTCANOPIES-2);
      m_apCanopy[i]->DirtySet();
   }
}

CForest::~CForest (void)
{
   DWORD i;
   for (i = 0; i < NUMFORESTCANOPIES; i++)
      if (m_apCanopy[i])
         delete m_apCanopy[i];
}


/*************************************************************************
CForest::Clone - Clones a forest
*/
CForest *CForest::Clone (void)
{
   PCForest pNew = new CForest;
   if (!pNew)
      return NULL;

   wcscpy (pNew->m_szName, m_szName);
   pNew->m_cColor = m_cColor;
   
   DWORD i;
   for (i = 0; i < NUMFORESTCANOPIES; i++) {
      if (pNew->m_apCanopy[i])
         delete pNew->m_apCanopy[i];
      pNew->m_apCanopy[i] = m_apCanopy[i]->Clone();
   }

   return pNew;
}

static PWSTR gpszForest = L"Forest";
static PWSTR gpszName = L"Name";
static PWSTR gpszColor = L"Color";

/*************************************************************************
CForest::MMLTo - writes a forest to mml
*/
PCMMLNode2 CForest::MMLTo (void)
{
   PCMMLNode2 pNode = new CMMLNode2;
   if (!pNode)
      return NULL;
   pNode->NameSet (gpszForest);

   if (m_szName[0])
      MMLValueSet (pNode, gpszName, m_szName);
   MMLValueSet (pNode, gpszColor, (int) m_cColor);

   DWORD i;
   for (i = 0; i < NUMFORESTCANOPIES; i++) {
      if (!m_apCanopy[i])
         continue;
      PCMMLNode2 pSub;
      pSub = m_apCanopy[i]->MMLTo ();
      if (pSub)
         pNode->ContentAdd (pSub);
   }

   return pNode;
}

/*************************************************************************
CForest::MMLFrom - Reads forest from mml
*/
BOOL CForest::MMLFrom (PCMMLNode2 pNode)
{
   PWSTR psz;
   psz = MMLValueGet (pNode, gpszName);
   if (psz)
      wcscpy (m_szName, psz);
   else
      m_szName[0] = 0;
   m_cColor = (COLORREF) MMLValueGetInt (pNode, gpszColor, (int) 0);

   DWORD i;
   DWORD dwCur;
   PCMMLNode2 pSub;
   dwCur = 0;
   for (i = 0; i < pNode->ContentNum(); i++) {
      pSub = NULL;
      pNode->ContentEnum (i, &psz, &pSub);
      if (!pSub)
         continue;
      psz = pSub->NameGet();
      if (!psz)
         continue;

      if (!_wcsicmp(psz, gpszCanopy) && (dwCur < NUMFORESTCANOPIES)) {
         m_apCanopy[dwCur]->MMLFrom (pSub);
         dwCur++;
      }
   }

   return TRUE;
}



/*************************************************************************
CForest::TextureQuery - From CObjectSocket
*/
BOOL CForest::TextureQuery (DWORD dwRenderShard, PCListFixed plText)
{
   DWORD i;
   BOOL fRet = FALSE;
   for (i = 0; i < NUMFORESTCANOPIES; i++)
      if (m_apCanopy[i])
         fRet = m_apCanopy[i]->TextureQuery (dwRenderShard, plText);
   return fRet;
}

/*************************************************************************
CForest::ColorQuery - From CObjectSocket
*/
BOOL CForest::ColorQuery (DWORD dwRenderShard, PCListFixed plColor)
{
   DWORD i;
   BOOL fRet = FALSE;
   for (i = 0; i < NUMFORESTCANOPIES; i++)
      if (m_apCanopy[i])
         fRet = m_apCanopy[i]->ColorQuery (dwRenderShard, plColor);
   return fRet;
}

/*************************************************************************
CForest::ObjectClassQuery - From CObjectSocket
*/
BOOL CForest::ObjectClassQuery (DWORD dwRenderShard, PCListFixed plObj)
{
   DWORD i;
   BOOL fRet = FALSE;
   for (i = 0; i < NUMFORESTCANOPIES; i++)
      if (m_apCanopy[i])
         fRet = m_apCanopy[i]->ObjectClassQuery (dwRenderShard, plObj);
   return fRet;
}
