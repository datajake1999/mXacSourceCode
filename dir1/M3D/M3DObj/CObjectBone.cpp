/************************************************************************
CObjectBone.cpp - For drawing bones.

begun 2/1/03 by Mike Rozak
Copyright 2003 Mike Rozak. All rights reserved
*/

#include <windows.h>
#include "escarpment.h"
#include "..\M3D.h"
#include "resource.h"

typedef struct {
   WCHAR       szName[64];    // name
   PCBone      pBone;      // bone it affects
   DWORD       dwAttrib;   // attribute it affects, same as index into m_pEnvStart, 4=fixed amt
} OBATTRIB, *POBATTRIB;


typedef struct {
   PWSTR       pszName;    // name of the move point
   int         iX, iY; // -1 if for left/top, 1 for right/bottom
} SPLINEMOVEP, *PSPLINEMOVEP;

static SPLINEMOVEP   gaObjectMove[] = {
   L"Bone base", 0, 0
};

/**********************************************************************************
CBone::Constructor and destructor */
CBone::CBone (void)
{
   wcscpy (m_szName, L"New bone");
   m_pEnd.Zero();
   m_pEnd.p[2] = 1;  // BUGFIX - Max default bone pointing up and hinge outward
   m_pUp.Zero();
   m_pUp.p[1] = -1;
   m_pMotionMin.Zero();
   m_pMotionMin.p[0] = -PI/4;
   m_pMotionMin.p[3] = 1;
   m_pMotionMax.Copy (&m_pMotionMin);
   m_pMotionMax.p[0] *= -1;
   m_pMotionDef.Average (&m_pMotionMin, &m_pMotionMax);
   m_pMotionCur.Copy (&m_pMotionDef);
   m_fUseLimits = TRUE;
   m_fFlipLR = FALSE;
   m_fCanBeFixed = FALSE;
   m_fFixedAmt = 0;
   m_pIKWant.Zero();
   m_fDefLowRank = TRUE;   // BUGFIX - Was FALSE, but usually move bones through IK
   m_fUseEnvelope = TRUE;
   m_fDefPassUp = TRUE;
   m_fDrawThick = .1;
   m_pEnvStart.p[0] = m_pEnvStart.p[1] = m_pEnvStart.p[2] = m_pEnvStart.p[3] = .33;
   m_pEnvEnd.Copy (&m_pEnvStart);
   m_pEnvLen.Zero();
   m_pEnvLen.p[0] = m_pEnvLen.p[1] = m_pEnvStart.p[0];
   m_lPCBone.Init (sizeof(PCBone));
   m_lObjRigid.Init (sizeof(GUID));
   m_lObjIgnore.Init (sizeof(GUID));

   m_pParent = NULL;

   CalcMatrix ();

   m_pStartOS.Zero();
   m_pEndOS.Zero();
   m_pUpOS.Zero();
   m_pStartOF.Zero();
   m_pEndOF.Zero();
   m_pUpOF.Zero();
}

CBone::~CBone (void)
{
   DWORD i;
   PCBone *ppb;
   ppb = (PCBone*) m_lPCBone.Get(0);
   for (i = 0; i < m_lPCBone.Num(); i++)
      delete ppb[i];
   m_lPCBone.Clear();
}



/**********************************************************************************
CBone::Split - Divides the bone in half.

inputs
   PWSTR    pszSplitName - Name to use for the split part. NOTE: No testing is
               done to make sure this name is unique.
returns
   BOOL - TRUE if success
*/
BOOL CBone::Split (PWSTR pszSplitName)
{
   PCBone pNew = new CBone;
   if (!pNew)
      return FALSE;

   if (!CloneTo (pNew, FALSE)) {
      delete pNew;
      return FALSE;
   }

   // set the name
   wcscpy (pNew->m_szName, pszSplitName);

   // move the children to the end of the split one
   pNew->m_lPCBone.Init (sizeof(PCBone), m_lPCBone.Get(0), m_lPCBone.Num());

   // set this as the only child
   m_lPCBone.Init (sizeof(PCBone), &pNew, 1);

   // halve
   m_pEnd.Scale (.5);
   pNew->m_pEnd.Scale (.5);

   // movement is restricted in the split one
   pNew->m_pMotionMin.Zero();
   pNew->m_pMotionMin.p[3] = 1;
   pNew->m_pMotionMax.Copy (&pNew->m_pMotionMin);
   pNew->m_pMotionDef.Copy (&pNew->m_pMotionMin);
   pNew->m_pMotionCur.Copy (&pNew->m_pMotionDef);

   // thicken then envelope
   DWORD i;
   for (i = 0; i < 4; i++) {
      pNew->m_pEnvStart.p[i] *= 2;
      pNew->m_pEnvEnd.p[i] *= 2;
      m_pEnvStart.p[i] *= 2;
      m_pEnvEnd.p[i] *= 2;
   }
   pNew->CalcMatrix();
   CalcMatrix();
   return TRUE;
}

/**********************************************************************************
CBone::Scale - Scales the bone and all its children. Invalidates calcualted
parameters in the process

inputs
   PCPoint     pScale - Amount to scale in xyz
*/
void CBone::Scale (PCPoint pScale)
{
   // scale
   m_pEnd.p[0] *= pScale->p[0];
   m_pEnd.p[1] *= pScale->p[1];
   m_pEnd.p[2] *= pScale->p[2];

   // adjust up, picking new normal if old one doesn't match
   CPoint pB, pA, pC;
   pA.Copy (&m_pEnd);
   pA.Normalize();
   DWORD j;
   for (j = 0; j < 4; j++) {
      if (!j)
         pC.Copy (&m_pUp);
      else {
         pC.Zero();
         pC.p[j-1] = 1;
      }
      pB.CrossProd (&pC, &pA);
      if (pB.Length() > CLOSE)
         break;
   }
   pB.Normalize();
   pC.CrossProd (&pA, &pB);
   pC.Normalize();
   m_pUp.Copy (&pC);

   // update matrices
   CalcMatrix();

   // loop through children
   PCBone *ppb;
   DWORD i;
   ppb = (PCBone*) m_lPCBone.Get(0);
   for (i = 0; i < m_lPCBone.Num(); i++)
      ppb[i]->Scale (pScale);
}

/**********************************************************************************
CBone::DeleteChild - Looks through the children (and so on) and sees if finds a child
pFind. If found, that child is deleted. NOTE: This will NOT delete itself.

inputs
   CBone       *pFind - Child ot find
returns
   BOOL - TRUE if found child and deleted
*/ 
BOOL CBone::DeleteChild (CBone *pFind)
{
   DWORD i;
   PCBone *ppb;
   ppb = (PCBone*) m_lPCBone.Get(0);
   for (i = 0; i < m_lPCBone.Num(); i++) {
      if (ppb[i] == pFind) {
         delete ppb[i];
         m_lPCBone.Remove (i);
         return TRUE;
      }

      if (ppb[i]->DeleteChild (pFind))
         return TRUE;
   }

   return FALSE;
}

/**********************************************************************************
CBone::Reorient - Called when merging one skeleton onto the end of another bone.
This reorients all the added bones are looped through and have their orientation changes
accoridng to the matrix pm.

inputs
   PCMatrix    pm - Apply this to the start and end (this is smart enough to remove
                  translations in pm). Also changes the normal
returns
   BOOL - TRUE if success
*/
BOOL CBone::Reorient (PCMatrix pm)
{
   // get all the bones within this
   CListFixed lBones;
   lBones.Init (sizeof(PCBone));
   AddToList (&lBones);

   DWORD i;
   PCBone *ppb;
   ppb = (PCBone*) lBones.Get(0);
   CPoint pStart;
   pStart.Zero();
   pStart.p[3] = 1;
   pStart.MultiplyLeft (pm);
   for (i = 0; i < lBones.Num(); i++) {
      // convert the end
      ppb[i]->m_pEnd.p[3] = 1;
      ppb[i]->m_pEnd.MultiplyLeft (pm);
      ppb[i]->m_pEnd.Subtract (&pStart);

      // convert the normal
      ppb[i]->m_pUp.p[3] = 1;
      ppb[i]->m_pUp.MultiplyLeft (pm);
      ppb[i]->m_pUp.Subtract (&pStart);
      ppb[i]->m_pUp.Normalize(); // shouldnt need to, but paranoid

      ppb[i]->CalcMatrix();
   }

   return TRUE;
}

/**********************************************************************************
CBone::AddToList - This bone (and all its children) will add themselves to the PCListFixed.
Used to determine all the bones in an object.

inputs
   PCListFixed    pl - Must be initialized to sizeof(PCBone), since a list of pointers will
                     be added. The order is not sorted, but is consistent.
returns
   none
*/
void CBone::AddToList (PCListFixed pl)
{
   PCBone pb = this;
   pl->Add (&pb);

   // children
   DWORD i;
   PCBone *ppb;
   ppb = (PCBone*) m_lPCBone.Get(0);
   for (i = 0; i < m_lPCBone.Num(); i++)
      ppb[i]->AddToList(pl);
}


/**********************************************************************************
CBone::Render - Draws the bone and all its childen.

NOTE: Assumes that CalcMatrix() has been called.

inputs
   CBone    *pEnvelope - If the this == pEnvelope then draw the envelope
   PCListFixed plBoneList - Pointer to a list of PCBone pointers. Look
               for the current bone in this list to determine what ID
               to use for the surface's minor ID.
*/
void CBone::Render (POBJECTRENDER pr, PCRenderSurface prs, CBone *pEnvelope,
                    PCListFixed plBoneList)
{
   // draw the bone
   prs->Push();
   prs->Multiply (&m_mRender);

   // figure out the length
   fp fLen, fThick, fNub, fLenOrig;
   fLen = fLenOrig = m_pEnd.Length();
   fThick = fLen * m_fDrawThick / 2.0; // take into account half
   if (m_pMotionDef.p[3] > CLOSE)
      fLen = fLen * m_pMotionCur.p[3] / m_pMotionDef.p[3];
   fNub = fLen / 4;  // thickest part of bone, along length

   // default material is boring
   DWORD i, dwColor;
   RENDERSURFACE Mat;
   memset (&Mat, 0, sizeof(Mat));
   Mat.Material.InitFromID (MATERIAL_PAINTMATTE);
   // BUGFIX - Set the minor ID for the bone based on the bone's index into bonelist
   PCBone* ppb = (PCBone*) plBoneList->Get(0);
   for (i = 0; i < plBoneList->Num(); i++)
      if (ppb[i] == this) {
         Mat.wMinorID = (WORD)i;
         break;
      }
   prs->SetDefMaterial (&Mat);

   // 4 sides
   COLORREF c;
   prs->Push();
   for (i = 0; i < 4; i++) {
      if (i)
         prs->Rotate (PI/2, 1);

      // color based on the side
      dwColor = i;
      if (m_fFlipLR) {
         if (i == 1)
            dwColor = 3;
         else if (i == 3)
            dwColor = 1;
      }
      switch (dwColor) {
         case 0:  // top
            c = RGB(0x0,0x0,0xff);
            break;
         case 1: // right
            c = RGB(0xff,0,0);
            break;
         case 2:  // bottom
            c = RGB(0x0,0x0,0x0);
            break;
         case 3:  // left
            c = RGB(0,0xff,0);
            break;
      }
      prs->SetDefColor (c);

      // 4 points
      DWORD dwPoint;
      PCPoint p;
      p = prs->NewPoints (&dwPoint, 4);
      if (!p)
         continue;
      p[0].Zero();
      p[1].Zero();
      p[1].p[0] = fNub;
      p[1].p[1] = p[1].p[2] = fThick;
      p[2].Copy (&p[1]);
      p[2].p[1] *= -1;
      p[3].Zero();
      p[3].p[0] = fLen;

      // 2 normals
      DWORD dwNorm;
      PCPoint pN;
      pN = prs->NewNormals (TRUE, &dwNorm, 2);
      if (pN) {
         CPoint pA, pB;
         pA.Subtract (&p[2], &p[1]);
         pB.Subtract (&p[0], &p[1]);
         pN[0].CrossProd (&pB, &pA);
         pN[0].Normalize();
         pB.Subtract (&p[3], &p[1]);
         pN[1].CrossProd (&pA, &pB);
         pN[1].Normalize();
      }

      // textures?
      DWORD dwText;
      dwText = prs->NewTexture (0,0,0,0,0);

      // triangles
      prs->NewTriangle (
         prs->NewVertex (dwPoint+0, dwNorm+0, dwText),
         prs->NewVertex (dwPoint+1, dwNorm+0, dwText),
         prs->NewVertex (dwPoint+2, dwNorm+0, dwText));
      prs->NewTriangle (
         prs->NewVertex (dwPoint+3, dwNorm+1, dwText),
         prs->NewVertex (dwPoint+2, dwNorm+1, dwText),
         prs->NewVertex (dwPoint+1, dwNorm+1, dwText));
      
   }
   prs->Pop();

   // draw the envelope
   CNoodle n;
   CPoint pFront, pScale;
   CPoint ap[10];
   DWORD adwCurve[10];
   if (pEnvelope == this) {
      prs->SetDefColor (RGB(0xff,0,0xff));
      memset (&ap, 0, sizeof(ap));
      n.BackfaceCullSet (FALSE);
      n.DrawEndsSet (FALSE);
      n.ShapeDefault (NS_CIRCLEFAST);
      pScale.Zero();
      pScale.p[0] = pScale.p[1] = fLenOrig / 100;
      n.ScaleVector (&pScale);

      if (m_fUseEnvelope) {
         DWORD dwStart;
         fp f;
         PCPoint pEnv;
         for (dwStart = 0; dwStart < 2; dwStart++) {
            pEnv = dwStart ? &m_pEnvStart : &m_pEnvEnd;

            // loop for the start and end
            for (i = 0; i < 8; i++) {
               f = (fp) i / (fp) 8 * 2.0 * PI;
               ap[i].p[0] = dwStart ? 0 : fLen;
               ap[i].p[1] = -sin(f) * pEnv->p[(i < 5) ? 1 : 3] * fLenOrig;
               if (m_fFlipLR)
                  ap[i].p[1] *= -1;
               ap[i].p[2] = cos(f) * pEnv->p[((i > 1) && (i < 7)) ? 2 : 0] * fLenOrig;
            }

            pFront.Zero();
            pFront.p[0] = -1;
            n.FrontVector (&pFront);
            n.PathSpline (TRUE, 8, ap, (DWORD*)SEGCURVE_CUBIC, 1);
            n.Render (pr, prs);
         }
         // loop over length of bone
         DWORD dwSide;
         for (dwStart = 0; dwStart < 2; dwStart++) {
            for (i = 0; i < 10; i++) {
               fp fSin, fCos;
               if (i <= 4)
                  f = (fp) i / 8.0 * 2.0 * PI;
               else
                  f = (fp) (i-1) / 8.0 * 2.0 * PI;
               fSin = sin(f);
               if (fSin > CLOSE)
                  fSin = 1.0;
               else if (fSin < -CLOSE)
                  fSin = -1.0;
               fCos = cos(f);
               if (fCos > CLOSE)
                  fCos = 1.0;
               else if (fCos < -CLOSE)
                  fCos = -1.0;

               if (i < 4)
                  adwCurve[i] = (i%2) ? SEGCURVE_ELLIPSEPREV : SEGCURVE_ELLIPSENEXT;
               else if ((i == 4) || (i == 9))
                  adwCurve[i] = SEGCURVE_LINEAR;
               else
                  adwCurve[i] = (i%2) ? SEGCURVE_ELLIPSENEXT : SEGCURVE_ELLIPSEPREV;

               ap[i].p[0] = fSin * m_pEnvLen.p[(i >= 5) ? 0 : 1] * fLenOrig + ((i <= 4) ? fLen : 0);
               ap[i].p[1] = ap[i].p[2] = 0;
               if (dwStart)
                  dwSide = ((i >= 2) && (i <= 7)) ? 1 : 3;
               else
                  dwSide = ((i >= 2) && (i <= 7)) ? 2 : 0;
               ap[i].p[dwStart ? 1 : 2] = fCos * ((i <= 4) ? m_pEnvEnd.p[dwSide] : m_pEnvStart.p[dwSide]) * fLenOrig;

               if (m_fFlipLR)
                  ap[i].p[1] *= -1;
            }

            pFront.Zero();
            pFront.p[dwStart ? 2 : 1] = 1;
            n.FrontVector (&pFront);
            n.PathSpline (TRUE, 10, ap, adwCurve, 1);
            n.Render (pr, prs);
         }
      }  // useenvelope
   }

   prs->Pop();

   if (pEnvelope == this) {
      // draw a box around the possible movement
      if ((m_pMotionMin.p[0] != m_pMotionMax.p[0]) || (m_pMotionMin.p[1] != m_pMotionMax.p[1])) {
         CPoint pCur;
         pCur.Copy (&m_pMotionCur);
         prs->SetDefColor (RGB(0xff,0xff,0));
         pScale.p[0] = pScale.p[1] = fLenOrig / 50;
         n.ScaleVector (&pScale);

         // matrix on 4 sides
         for (i = 0; i < 4; i++) {
            m_pMotionCur.Copy (&m_pMotionDef);
            m_pMotionCur.p[0] = (i < 2) ? m_pMotionMax.p[0] : m_pMotionMin.p[0];
            m_pMotionCur.p[1] = ((i == 1) || (i == 2)) ? m_pMotionMax.p[1] : m_pMotionMin.p[1];

            CalcMatrix ();

            // figure out the points
            ap[i].Zero();
            ap[i].p[0] = fLen / 2;
            ap[i].p[3] = 1;
            ap[i].MultiplyLeft (&m_mRender);
         }

         // draw the box
         pFront.Zero();
         pFront.p[0] = 1;
         n.FrontVector (&pFront);
         if ((m_pMotionMin.p[0] != m_pMotionMax.p[0]) && (m_pMotionMin.p[1] != m_pMotionMax.p[1]))
            n.PathSpline (TRUE, 4, ap, (DWORD*)SEGCURVE_LINEAR, 0);
         else if (m_pMotionMin.p[0] != m_pMotionMax.p[0])
            n.PathLinear (&ap[0], &ap[2]);
         else
            n.PathLinear (&ap[1], &ap[3]);

         n.Render (pr, prs);

         // lines to form cone
         ap[4].Zero();
         for (i = 0; i < 4; i++) {
            n.PathLinear (&ap[4], &ap[i]);
            n.Render (pr, prs);
         }

         // return to normal
         m_pMotionCur.Copy (&pCur);
         CalcMatrix ();
      }
   }

   // draw the children
   if (m_lPCBone.Num()) {
      prs->Push ();
      prs->Multiply (&m_mChildCur);

      PCBone *ppb;
      ppb = (PCBone*) m_lPCBone.Get(0);
      for (i = 0; i < m_lPCBone.Num(); i++)
         ppb[i]->Render (pr, prs, pEnvelope, plBoneList);

      prs->Pop();
   }
}

// NOTE: Not bothering to to QueryBoundingBox() since usually only rendered in working version


/**********************************************************************************
CBone::FillInParent - The bone will set it's parent (m_pParent) to the CBone
passed in. It then recurses and tells all the children to fill in their parents with
itself,and so on recursing until pParent is properly fileld in.

inputs
   CBone          *pParent - Parent to use for this
returns
   none
*/
void CBone::FillInParent (CBone *pParent)
{
   m_pParent = pParent;


   // recurse
   DWORD i;
   PCBone *ppb;
   ppb = (PCBone*) m_lPCBone.Get(0);
   for (i = 0; i < m_lPCBone.Num(); i++)
      ppb[i]->FillInParent (this);
}


/**********************************************************************************
CBone::CalcIKMass - Calculates the mass for this and fills it into fIKMass.
Recurses, filling in all the masses.

returns
   fp - Mass of this and its children
*/
fp CBone::CalcIKMass (void)
{
   fp f = 0;

   // recurse
   PCBone *ppb;
   DWORD i;
   ppb = (PCBone*) m_lPCBone.Get(0);
   for (i = 0; i < m_lPCBone.Num(); i++)
      f += ppb[i]->CalcIKMass ();

   f += CalcMass (TRUE);

   m_fIKMass = f;
   return f;
}

/**********************************************************************************
CBone::CalcMass - Returns a mass for the bone. This includes the bone and all its
children. Mass is based on the area of the bone's envelope.

inputs
   BOOL        fJustThis - If TRUE then only mass of this bone, else all children also

returns
   fp - Mass. Volume in m3
*/
fp CBone::CalcMass (BOOL fJustThis)
{
   // mass for this?
   fp f;
   DWORD i;
   f = 0;
   if (m_fUseEnvelope) for (i = 0; i < 4; i++) {
      fp fAreaStart, fAreaEnd;
      fAreaStart = m_pEnvStart.p[i] * m_pEnvStart.p[(i+1)%4];
      fAreaEnd = m_pEnvEnd.p[i] * m_pEnvEnd.p[(i+1)%4];

      f += fAreaStart * m_pEnvLen.p[0] / 2 + // approx voluume of end of sphere
         fAreaEnd * m_pEnvLen.p[1] / 2 +  // appeox volume of end of sphere
         (fAreaStart + fAreaEnd) / 2;  // approx area of cylindrical portion
   }
   fp fLen;
   fLen = m_pEnd.Length();
   f *= (fLen * fLen * fLen);

   if (fJustThis)
      return f;

   // recurse
   PCBone *ppb;
   ppb = (PCBone*) m_lPCBone.Get(0);
   for (i = 0; i < m_lPCBone.Num(); i++)
      f += ppb[i]->CalcMass ();

   return f;
}

/**********************************************************************************
CBone::CalcObjectSpace - Calculates the locations of the bones (and Up vector)
in object space. Fills in m_pStartOS, m_pEndOS, m_pUpOS, or
it fills in m_pStartOF, m_pEndOF, m_pUpOF.

NOTE: Must have called CalcMatrix() before this for every bone for the non-fixed.

inputs
   PCMatrix       pBoneToObject - Matrix that converts from bone space into object
                  space
   BOOL           fFixed - If TRYE use m_pXXXOF. Use, use m_pXXXOS and need to have CalcMatrix() called.
   PCMatrix       pmNextInLine - If not NULL this is filled with the next matrix to pass into
                  children as pBoneToObject; no recursion is done. If NULL, then recursion is done
                  and children area also affected.
   BOOL           fForIK - Normally set to FALSE. If set to TRUE then calculated object space
                  for IK, so ignore those bits outside of IK chain. Thus, if !m_fIKChain just return
returns
   none
*/
void CBone::CalcObjectSpace (PCMatrix pBoneToObject, BOOL fFixed, PCMatrix pmNextInLine,
                             BOOL fForIK)
{
   // if it's just for IK and this isn't in the IK chain then ignore
   if (fForIK && !m_fIKChain)
      return;

   PCPoint pStart, pUp, pEnd;
   PCMatrix pm;
   CMatrix m;
   if (fFixed) {
      pStart = &m_pStartOF;
      pEnd = &m_pEndOF;
      pUp = &m_pUpOF;
      pm = &m;
      m.Translation (m_pEnd.p[0], m_pEnd.p[1], m_pEnd.p[2]);
      // matrix is simple if it's in the fixed location
   }
   else {
      // store the bone to object away so can use for deform in CObjectEditor
      m_mCalcOS.Copy (pBoneToObject);

      pStart = &m_pStartOS;
      pEnd = &m_pEndOS;
      pUp = &m_pUpOS;
      pm = &m_mChildCur;
   }

   // location of start
   pStart->Zero();
   pStart->p[3] = 1;
   pStart->MultiplyLeft (pBoneToObject);

   // normal
   pUp->Copy (&m_pUp);
   pUp->p[3] = 1;
   pUp->MultiplyLeft (pBoneToObject);
   pUp->Subtract (pStart);

   // location of end
   CMatrix mCombo;
   mCombo.Multiply (pBoneToObject, pm);
   pEnd->Zero();
   pEnd->p[3] = 1;
   pEnd->MultiplyLeft (&mCombo);  // since children start at the end

   if (pmNextInLine)
      pmNextInLine->Copy (&mCombo);
   else {
      // recurse
      DWORD i;
      PCBone *ppb;
      ppb = (PCBone*) m_lPCBone.Get(0);
      for (i = 0; i < m_lPCBone.Num(); i++)
         ppb[i]->CalcObjectSpace (&mCombo, fFixed, NULL, fForIK);
   }
}

/**********************************************************************************
CBone::CalcMatrix - Tells the bone that there's a new current location (bend, side to side,
twist, and length) for the bone, or new m_pCur, or new m_pUp.
his procedes to calculate the "Cur" attributes.

inputs
   None
returns
   none
*/
void CBone::CalcMatrix (void)
{
   // length
   fp fLen;
   fLen = m_pEnd.Length();

   // if limit motion then do so
   DWORD i;
   if (m_fUseLimits) {
      for (i = 0; i < 4; i++) {
         m_pMotionCur.p[i] = max(m_pMotionCur.p[i], m_pMotionMin.p[i]);
         m_pMotionCur.p[i] = min(m_pMotionCur.p[i], m_pMotionMax.p[i]);
      }
   }

   // create the matrix that takes assumes pEnd = (1,0,0) and pUp = (0,0,1), and
   // rotates into the given position
   CMatrix m, m2, mTrans;
   mTrans.Identity();
   m.Identity();
   if (fabs( m_pMotionDef.p[3]) > EPSILON)
      mTrans.Translation (fLen * m_pMotionCur.p[3] / m_pMotionDef.p[3], 0, 0);

   // twist
   if (fabs(m_pMotionCur.p[2] - m_pMotionDef.p[2]) > CLOSE) {
      m2.RotationX ((m_fFlipLR ? -1 : 1) * (m_pMotionCur.p[2] - m_pMotionDef.p[2]));
      m.MultiplyRight (&m2);
   }

   // side to side
   if (fabs(m_pMotionCur.p[1] - m_pMotionDef.p[1]) > CLOSE) {
      m2.RotationZ ((m_fFlipLR ? -1 : 1) * (m_pMotionCur.p[1] - m_pMotionDef.p[1]));
      m.MultiplyRight (&m2);
   }

   // bend
   if (fabs(m_pMotionCur.p[0] - m_pMotionDef.p[0]) > CLOSE) {
      m2.RotationY (m_pMotionCur.p[0] - m_pMotionDef.p[0]);
      m.MultiplyRight (&m2);
   }

   // create a matrix that takes 1,0,0 to m_pEnd/fLen, and 0,0,1 to m_pUp.
   CMatrix mXToEnd, mEndToX;
   CPoint pA, pB;
   pA.Copy (&m_pEnd);
   if (fLen > EPSILON)
      pA.Scale (1.0  / fLen);
   pB.CrossProd (&m_pUp, &pA);
   pB.Normalize();   // should be length of 1, but just to make sure
   mXToEnd.RotationFromVectors (&pA, &pB, &m_pUp);
   mXToEnd.Invert (&mEndToX); // since only rotation, use normal invert

   // create the full matrix
   m_mXToEnd.Copy (&mXToEnd);
   m_mRender.Multiply (&mXToEnd, &m);
   m_mChildCur.Multiply (&m_mRender, &mTrans);
   m_mChildCur.MultiplyLeft (&mEndToX);
}

static PWSTR gpszBone = L"Bone";
static PWSTR gpszDesc = L"Desc";
static PWSTR gpszName = L"Name";
static PWSTR gpszEnd = L"End";
static PWSTR gpszUp = L"Up";
static PWSTR gpszMotionMin = L"MotionMin";
static PWSTR gpszMotionMax = L"MotionMax";
static PWSTR gpszMotionDef = L"MotionDef";
static PWSTR gpszMotionCur = L"MotionCur";
static PWSTR gpszDefLowRank = L"DefLowRank";
static PWSTR gpszUseLimits = L"UseLimits";
static PWSTR gpszDefPassUp = L"DefPassUp";
static PWSTR gpszDrawThick = L"DrawThick";
static PWSTR gpszEnvStart = L"EnvStart";
static PWSTR gpszEnvEnd = L"EnvEnd";
static PWSTR gpszEnvLen = L"EnvLen";
static PWSTR gpszFlipLR = L"FlipLR";
static PWSTR gpszUseEnvelope = L"UseEnvelope";
static PWSTR gpszCanBeFixed = L"CanBeFixed";
static PWSTR gpszFixedAmt = L"FixedAmt";

/**********************************************************************************
CBone::MMLTo - Standard MMLTo */
PCMMLNode2 CBone::MMLTo (void)
{
   PCMMLNode2 pNode = new CMMLNode2;
   if (!pNode)
      return NULL;
   pNode->NameSet (gpszBone);

   if (m_szName[0])
      MMLValueSet (pNode, gpszName, m_szName);
   if (m_memDesc.p && ((PWSTR)m_memDesc.p)[0])
      MMLValueSet (pNode, gpszDesc, (PWSTR) m_memDesc.p);
   MMLValueSet (pNode, gpszEnd, &m_pEnd);
   MMLValueSet (pNode, gpszUp, &m_pUp);
   MMLValueSet (pNode, gpszMotionMin, &m_pMotionMin);
   MMLValueSet (pNode, gpszMotionMax, &m_pMotionMax);
   MMLValueSet (pNode, gpszMotionDef, &m_pMotionDef);
   MMLValueSet (pNode, gpszMotionCur, &m_pMotionCur);
   MMLValueSet (pNode, gpszUseLimits, (int) m_fUseLimits);
   MMLValueSet (pNode, gpszFlipLR, (int) m_fFlipLR);
   MMLValueSet (pNode, gpszCanBeFixed, (int) m_fCanBeFixed);
   if (m_fCanBeFixed)
      MMLValueSet (pNode, gpszFixedAmt, m_fFixedAmt);
   MMLValueSet (pNode, gpszDefLowRank, (int) m_fDefLowRank);
   MMLValueSet (pNode, gpszUseEnvelope, (int) m_fUseEnvelope);
   MMLValueSet (pNode, gpszDefPassUp, (int) m_fDefPassUp);
   MMLValueSet (pNode, gpszDrawThick, m_fDrawThick);
   MMLValueSet (pNode, gpszEnvStart, &m_pEnvStart);
   MMLValueSet (pNode, gpszEnvEnd, &m_pEnvEnd);
   MMLValueSet (pNode, gpszEnvLen, &m_pEnvLen);

   // ones to ignore
   DWORD i;
   WCHAR szTemp[32];
   GUID *pg;
   pg = (GUID*) m_lObjRigid.Get(0);
   for (i = 0; i < m_lObjRigid.Num(); i++) {
      swprintf (szTemp, L"ObjRigid%d", (int) i);
      MMLValueSet (pNode, szTemp, (PBYTE) &pg[i], sizeof(pg[i]));
   }
   pg = (GUID*) m_lObjIgnore.Get(0);
   for (i = 0; i < m_lObjIgnore.Num(); i++) {
      swprintf (szTemp, L"ObjIgnore%d", (int) i);
      MMLValueSet (pNode, szTemp, (PBYTE) &pg[i], sizeof(pg[i]));
   }

   // all the sub bones
   PCBone *ppb;
   ppb = (PCBone*) m_lPCBone.Get(0);
   for (i = 0; i < m_lPCBone.Num(); i++) {
      PCMMLNode2 pSub = ppb[i]->MMLTo();
      if (pSub)
         pNode->ContentAdd (pSub); // dont need to set name
   }

   return pNode;
}

/**********************************************************************************
CBone::MMLFrom - Standard MML from
*/
BOOL CBone::MMLFrom (PCMMLNode2 pNode)
{
   // clear out some stuff
   DWORD i;
   PCBone *ppb;
   ppb = (PCBone*) m_lPCBone.Get(0);
   for (i = 0; i < m_lPCBone.Num(); i++)
      delete ppb[i];
   m_lPCBone.Clear();
   MemZero (&m_memDesc);
   m_szName[0] = 0;
   m_lObjRigid.Clear();
   m_lObjIgnore.Clear();


   PWSTR psz;
   psz = MMLValueGet (pNode, gpszName);
   if (psz)
      wcscpy (m_szName, psz);
   psz = MMLValueGet (pNode, gpszDesc);
   if (psz && m_memDesc.Required((wcslen(psz)+1)*2))
      wcscpy ((PWSTR) m_memDesc.p, psz);
   MMLValueGetPoint (pNode, gpszEnd, &m_pEnd);
   MMLValueGetPoint (pNode, gpszUp, &m_pUp);
   MMLValueGetPoint (pNode, gpszMotionMin, &m_pMotionMin);
   MMLValueGetPoint (pNode, gpszMotionMax, &m_pMotionMax);
   MMLValueGetPoint (pNode, gpszMotionDef, &m_pMotionDef);
   m_pMotionCur.Copy (&m_pMotionDef);
   MMLValueGetPoint (pNode, gpszMotionCur, &m_pMotionCur);
   m_fUseLimits = (int) MMLValueGetInt (pNode, gpszUseLimits, TRUE);
   m_fFlipLR = (int) MMLValueGetInt (pNode, gpszFlipLR, TRUE);
   m_fCanBeFixed = (BOOL) MMLValueGetInt (pNode, gpszCanBeFixed, FALSE);
   if (m_fCanBeFixed)
      m_fFixedAmt = MMLValueGetDouble (pNode, gpszFixedAmt, 0);
   else
      m_fFixedAmt = 0;
   m_fDefLowRank = (int) MMLValueGetInt (pNode, gpszDefLowRank, FALSE);
   m_fUseEnvelope = (int) MMLValueGetInt (pNode, gpszUseEnvelope, TRUE);
   m_fDefPassUp = (int) MMLValueGetInt (pNode, gpszDefPassUp, (int) TRUE);
   m_fDrawThick = MMLValueGetDouble (pNode, gpszDrawThick, .1);
   MMLValueGetPoint (pNode, gpszEnvStart, &m_pEnvStart);
   MMLValueGetPoint (pNode, gpszEnvEnd, &m_pEnvEnd);
   MMLValueGetPoint (pNode, gpszEnvLen, &m_pEnvLen);

   // ones to ignore
   WCHAR szTemp[32];
   GUID g;
   for (i = 0; ; i++) {
      swprintf (szTemp, L"ObjRigid%d", (int) i);
      if (sizeof(g) != MMLValueGetBinary (pNode, szTemp, (PBYTE) &g, sizeof(g)))
         break;
      m_lObjRigid.Add (&g);
   }
   for (i = 0; ; i++) {
      swprintf (szTemp, L"ObjIgnore%d", (int) i);
      if (sizeof(g) != MMLValueGetBinary (pNode, szTemp, (PBYTE) &g, sizeof(g)))
         break;
      m_lObjIgnore.Add (&g);
   }

   // all the sub bones
   PCMMLNode2 pSub;
   for (i = 0; i < pNode->ContentNum(); i++) {
      pSub = NULL;
      pNode->ContentEnum (i, &psz, &pSub);
      if (!pSub)
         continue;
      psz = pSub->NameGet();
      if (psz && !_wcsicmp(psz, gpszBone)) {
         PCBone pb = new CBone;
         if (!pb)
            continue;
         if (!pb->MMLFrom(pSub)) {
            delete pb;
            continue;
         }
         m_lPCBone.Add (&pb);
      }
   }

   // Calc
   CalcMatrix ();


   return TRUE;
}

/**********************************************************************************
CBone::CloneTo - Copies all the information from this bone to the pTo.

inputs
   CBone       *pTo - Copy stuff to
   BOOL        fIncludeChildren - If TRUE also clone the children. Else, just leave
               new one without children.
*/
BOOL CBone::CloneTo (CBone *pTo, BOOL fIncludeChildren)
{
   // clear out some stuff
   DWORD i;
   PCBone *ppb;
   ppb = (PCBone*) pTo->m_lPCBone.Get(0);
   for (i = 0; i < pTo->m_lPCBone.Num(); i++)
      delete ppb[i];
   pTo->m_lPCBone.Clear();
   if (pTo->m_memDesc.p)
      MemZero (&pTo->m_memDesc);

   // to ignore
   pTo->m_lObjRigid.Init (sizeof(GUID), m_lObjRigid.Get(0), m_lObjRigid.Num());
   pTo->m_lObjIgnore.Init (sizeof(GUID), m_lObjIgnore.Get(0), m_lObjIgnore.Num());

   wcscpy (pTo->m_szName, m_szName);
   if (m_memDesc.p && ((PWSTR)m_memDesc.p)[0]) {
      MemZero (&pTo->m_memDesc);
      MemCat (&pTo->m_memDesc, (PWSTR) m_memDesc.p);
   }
   pTo->m_pEnd.Copy (&m_pEnd);
   pTo->m_pUp.Copy (&m_pUp);
   pTo->m_pMotionMin.Copy (&m_pMotionMin);
   pTo->m_pMotionMax.Copy (&m_pMotionMax);
   pTo->m_pMotionDef.Copy (&m_pMotionDef);
   pTo->m_pMotionCur.Copy (&m_pMotionCur);
   pTo->m_fUseLimits = m_fUseLimits;
   pTo->m_fFlipLR = m_fFlipLR;
   pTo->m_fCanBeFixed = m_fCanBeFixed;
   pTo->m_fFixedAmt = m_fFixedAmt;
   pTo->m_pIKWant.Copy (&m_pIKWant);
   pTo->m_fDefLowRank = m_fDefLowRank;
   pTo->m_fUseEnvelope = m_fUseEnvelope;
   pTo->m_fDefPassUp = m_fDefPassUp;
   pTo->m_fDrawThick = m_fDrawThick;
   pTo->m_pEnvStart.Copy (&m_pEnvStart);
   pTo->m_pEnvEnd.Copy (&m_pEnvEnd);
   pTo->m_pEnvLen.Copy (&m_pEnvLen);

   pTo->m_mChildCur.Copy (&m_mChildCur);
   pTo->m_mRender.Copy (&m_mRender);
   pTo->m_mXToEnd.Copy (&m_mXToEnd);
   pTo->m_mCalcOS.Copy (&m_mCalcOS);
   pTo->m_pStartOS.Copy (&m_pStartOS);
   pTo->m_pEndOS.Copy (&m_pEndOS);
   pTo->m_pUpOS.Copy (&m_pUpOS);
   pTo->m_pStartOF.Copy (&m_pStartOF);
   pTo->m_pParent = m_pParent;
   pTo->m_pEndOF.Copy (&m_pEndOF);
   pTo->m_pUpOF.Copy (&m_pUpOF);

   if (fIncludeChildren) {
      // clone the children
      ppb = (PCBone*) m_lPCBone.Get(0);
      pTo->m_lPCBone.Required (m_lPCBone.Num());
      for (i = 0; i < m_lPCBone.Num(); i++) {
         PCBone pNew = new CBone;
         if (!pNew)
            return FALSE;
         if (!ppb[i]->CloneTo (pNew)) {
            delete pNew;
            return FALSE;
         }
         pTo->m_lPCBone.Add (&pNew);
      }
   }

   return TRUE;
}


/**********************************************************************************
CObjectBone::Constructor and destructor */
CObjectBone::CObjectBone (PVOID pParams, POSINFO pInfo)
{
   m_dwRenderShow = RENDERSHOW_MISC;
   m_OSINFO = *pInfo;
   m_dwSymmetry = 1; // BUGFIX - Default to x symmetry
   m_dwCurBone = 0;
   m_fBoneDefault = TRUE;
   m_fSkeltonForOE = FALSE;
   m_pIKTrans.Zero();
   memset (m_apBound, 0, sizeof(m_apBound));
   m_lBoneList.Init (sizeof(PCBone));
   m_lOBATTRIB.Init (sizeof(OBATTRIB));

   // for doing drawing of bone
   m_plOEBONE = m_plOERIGIDBONE = m_plBoneMove = m_plBoneAffect = m_plOEBW = NULL;
   m_fBoneAffectUniform = FALSE;
   m_mBoneExtraRot.Identity();

   m_lPCBone.Init (sizeof(PCBone));
   PCBone pNew;
   pNew = new CBone;
   CMatrix mIdent;
   mIdent.Identity();
   if (pNew)
      m_lPCBone.Add (&pNew);

   CalcObjectSpace ();
   CalcAttribList ();
}


CObjectBone::~CObjectBone (void)
{
   DWORD i;
   PCBone *ppb;
   ppb = (PCBone*) m_lPCBone.Get(0);
   for (i = 0; i < m_lPCBone.Num(); i++)
      delete ppb[i];
   m_lPCBone.Clear();

   ObjEditBonesClear();
}


/**********************************************************************************
CObjectBone::Delete - Called to delete this object
*/
void CObjectBone::Delete (void)
{
   delete this;
}

/**********************************************************************************
CObjectBone::Render - Draws a test box.

inputs
   POBJECTRENDER     pr - Render information
*/
void CObjectBone::Render (POBJECTRENDER pr, DWORD dwSubObject)
{
   // create the surface render object and draw
   // CRenderSurface rs;
   m_Renderrs.ClearAll();

   CMatrix mObject;
   m_Renderrs.Init (pr->pRS);
   ObjectMatrixGet (&mObject);
   m_Renderrs.Multiply (&mObject);

   // figure out which one gets the envelope
   PCBone pEnvelope;
   pEnvelope = NULL;
   if (m_dwCurBone < m_lBoneList.Num())
      pEnvelope = *((PCBone*) m_lBoneList.Get(m_dwCurBone));

   // BUGFIX - If not selected (and not calculating bounding box) then
   // dont bother with envelope
   if (!pr->fSelected && (pr->dwReason != ORREASON_BOUNDINGBOX))
      pEnvelope = NULL;

   // draw the bone
   DWORD i;
   PCBone *ppb;
   ppb = (PCBone*) m_lPCBone.Get(0);
   for (i = 0; i < m_lPCBone.Num(); i++)
      ppb[i]->Render (pr, &m_Renderrs, pEnvelope, &m_lBoneList);

   m_Renderrs.Commit();
}


/**********************************************************************************
CObjectBone::Clone - Clones this

inputs
   none
returns
   PCObjectSocket - Clone
*/
PCObjectSocket CObjectBone::Clone (void)
{
   PCObjectBone pNew;

   pNew = new CObjectBone(NULL, &m_OSINFO);

   // clone template info
   CloneTemplate(pNew);

   pNew->m_dwSymmetry =m_dwSymmetry;
   pNew->m_dwCurBone = m_dwCurBone;
   pNew->m_fSkeltonForOE = m_fSkeltonForOE;
   pNew->m_mBoneExtraRot.Copy (&m_mBoneExtraRot);
   DWORD i;
   PCBone *ppb;
   ppb = (PCBone*) pNew->m_lPCBone.Get(0);
   for (i = 0; i < pNew->m_lPCBone.Num(); i++)
      delete ppb[i];
   pNew->m_lPCBone.Clear();
   ppb = (PCBone*) m_lPCBone.Get(0);
   pNew->m_lPCBone.Required (m_lPCBone.Num());
   for (i = 0; i < m_lPCBone.Num(); i++) {
      PCBone pb = new CBone;
      if (!pb)
         continue;
      ppb[i]->CloneTo (pb);
      pNew->m_lPCBone.Add (&pb);
   }

   // recalc the object space because this also updates the m_lBoneList
   pNew->CalcObjectSpace ();
   pNew->CalcAttribList();
   // NOTE: Not copying over bone info
   pNew->ObjEditBonesClear();

   return pNew;
}

static PWSTR gpszSymmetry = L"Symmetry";
static PWSTR gpszCurBone = L"CurBone";

PCMMLNode2 CObjectBone::MMLTo (void)
{
   // call into the template first
   PCMMLNode2 pNode = MMLToTemplate();
   if (!pNode)
      return NULL;

   MMLValueSet (pNode, gpszSymmetry, (int) m_dwSymmetry);
   MMLValueSet (pNode, gpszCurBone, (int) m_dwCurBone);

   PCMMLNode2 pSub;
   DWORD i;
   PCBone *ppb;
   ppb = (PCBone*) m_lPCBone.Get(0);
   for (i = 0; i < m_lPCBone.Num(); i++) {
      pSub = ppb[i]->MMLTo ();
      if (!pSub) {
         delete pNode;
         return NULL;
      }
      // dont need to set name because is gpszBone
      pNode->ContentAdd (pSub);
   }

   return pNode;
}

BOOL CObjectBone::MMLFrom (PCMMLNode2 pNode)
{
   // call into the template first
   if (!MMLFromTemplate (pNode))
      return FALSE;

   m_dwSymmetry = (DWORD) MMLValueGetInt (pNode, gpszSymmetry, 0);
   m_dwCurBone = (DWORD) MMLValueGetInt (pNode, gpszCurBone, 0);

   DWORD i;
   PCBone *ppb;
   ppb = (PCBone*) m_lPCBone.Get(0);
   for (i = 0; i < m_lPCBone.Num(); i++)
      delete ppb[i];
   m_lPCBone.Clear();

   PWSTR psz;
   PCMMLNode2 pSub;
   for (i = 0; i < pNode->ContentNum(); i++) {
      pSub= NULL;
      pNode->ContentEnum (i, &psz, &pSub);
      if (!pSub)
         continue;
      psz = pSub->NameGet();
      if (!psz)
         continue;
      if (!_wcsicmp(psz, gpszBone)) {
         PCBone pb = new CBone;
         if (!pb)
            continue;
         pb->MMLFrom (pSub);
         m_lPCBone.Add (&pb);
      }
   }

   // calculate all the locations
   CalcObjectSpace ();
   CalcAttribList();
   ResetIKWant ();
   ObjEditBonesClear();

   return TRUE;
}

/**********************************************************************************
OBATTRIBCompare - Used for sorting attributes in m_lPCOEAttrib
*/
static int _cdecl OBATTRIBCompare (const void *elem1, const void *elem2)
{
   OBATTRIB *pdw1, *pdw2;
   pdw1 = (OBATTRIB*) elem1;
   pdw2 = (OBATTRIB*) elem2;

   return _wcsicmp(pdw1->szName, pdw2->szName);
}


/**********************************************************************************
CObjectBone::CalcAttribList - Creates the attribute list from m_lBoneList.
Hence, this MUST be called after CalcObjectSpace() - assuming that bones have been
added or removed. This fills in m_lOBATTRIB. It then sorts it alphabetically.
*/
static PWSTR gpszBend = L" (Bend)";
static PWSTR gpszPivot = L" (Side-to-side)";
static PWSTR gpszTwist = L" (Twist)";
static PWSTR gpszExtend = L" (Extend)";
static PWSTR gpszFixed = L" (Fixed)";
static DWORD gdwBendLen = (DWORD)wcslen(gpszBend);
static DWORD gdwPivotLen = (DWORD)wcslen(gpszPivot);
static DWORD gdwTwistLen = (DWORD)wcslen(gpszTwist);
static DWORD gdwExtendLen = (DWORD)wcslen(gpszExtend);
static DWORD gdwFixedLen = (DWORD)wcslen(gpszFixed);

void CObjectBone::CalcAttribList (void)
{
   m_lOBATTRIB.Clear ();

   OBATTRIB ob;
   memset (&ob, 0, sizeof(ob));

   DWORD dwNum, i, j, dwCount;
   PCBone *ppb;
   PCBone pb;
   ppb = (PCBone*) m_lBoneList.Get(0);
   dwNum = m_lBoneList.Num();
   for (i = 0; i < dwNum; i++) {
      pb = ppb[i];

      // see if want fixed attribute
      if (pb->m_fCanBeFixed) {
         ob.pBone = pb;
         ob.dwAttrib = 4;  // can be fixed
         wcscpy (ob.szName, pb->m_szName);

         // if more than one attribute then append name
         PWSTR psz;
         DWORD dwLen;
         psz = gpszFixed;
         dwLen = gdwFixedLen;

         ob.szName[63-dwLen] = 0;
         wcscat (ob.szName, psz);

         // add this
         m_lOBATTRIB.Add (&ob);
      }

      // find out how many attributes there are (from motion)
      dwCount = 0;
      for (j = 0; j < 4; j++)
         if (pb->m_pMotionMin.p[j] != pb->m_pMotionMax.p[j])
            dwCount++;
      if (!dwCount)
         continue;   // no attributes for this

      for (j = 0; j < 4; j++) {
         // if the same then not attribute
         if (pb->m_pMotionMin.p[j] == pb->m_pMotionMax.p[j])
            continue;

         // else, match
         ob.pBone = pb;
         ob.dwAttrib = j;
         wcscpy (ob.szName, pb->m_szName);

         // if more than one attribute then append name
         if (dwCount > 1) {
            PWSTR psz;
            DWORD dwLen;
            switch (j) {
               case 0:
                  psz = gpszBend;
                  dwLen = gdwBendLen;
                  break;
               case 1:
                  psz = gpszPivot;
                  dwLen = gdwPivotLen;
                  break;
               case 2:
                  psz = gpszTwist;
                  dwLen = gdwTwistLen;
                  break;
               case 3:
                  psz = gpszExtend;
                  dwLen = gdwExtendLen;
                  break;
            }

            ob.szName[63-dwLen] = 0;
            wcscat (ob.szName, psz);
         }  // if dwCOunt > 1

         // add this
         m_lOBATTRIB.Add (&ob);
      } // over j
   } // over i

   // sort so it's alphabetical
   qsort (m_lOBATTRIB.Get(0), m_lOBATTRIB.Num(), sizeof(OBATTRIB), OBATTRIBCompare);

   // done
}

/**********************************************************************************
CObjectBone::CalcObjectSpace - Calculate where all the bones are in object space
*/
void CObjectBone::CalcObjectSpace (void)
{
   // calculate all the locations
   CMatrix mCur;
   PCBone *ppb;
   DWORD i;
   mCur.Identity();
   ppb = (PCBone*) m_lPCBone.Get(0);
   m_apBound[0].Zero();
   m_apBound[1].Zero();
   for (i = 0; i < m_lPCBone.Num(); i++) {
      // calculate both fixed and moved locations. Don't need to recalc fixed location
      // all the time but might as well because it's fast
      ppb[i]->CalcObjectSpace (&mCur, TRUE);
      ppb[i]->CalcObjectSpace (&mCur, FALSE);

      // fill in parents while at it
      ppb[i]->FillInParent (NULL);
   }

   // create the bone lis
   m_lBoneList.Clear();
   for (i = 0; i < m_lPCBone.Num(); i++)
      ppb[i]->AddToList (&m_lBoneList);

   // loop through all the bones and make sure they are in default position
   m_fBoneDefault = TRUE;
   ppb = (PCBone*) m_lBoneList.Get(0);
   DWORD j;
   for (i = 0; i < m_lBoneList.Num(); i++) {
      // keep track of minimum and maximum location in space
      // only do end since start of one is always end of another, except for 0,0,0
      // which is already accounted for
      m_apBound[0].Min (&ppb[i]->m_pEndOF);
      m_apBound[1].Max (&ppb[i]->m_pEndOF);

      if (m_fBoneDefault) for (j = 0; j < 4; j++)
         if (ppb[i]->m_pMotionCur.p[j] != ppb[i]->m_pMotionDef.p[j])
            m_fBoneDefault = FALSE;
   }
}


/**********************************************************************************
CObjectBone::Message - Standard message handling call
*/
BOOL CObjectBone::Message (DWORD dwMessage, PVOID pParam)
{
   switch (dwMessage) {
   case OSM_BONE:
      {
         POSMBONE p = (POSMBONE) pParam;
         p->pb = this;
      }
      return TRUE;

   }

   return FALSE;
}

/**********************************************************************************
CObjectBone::Merge - Standard merge call.
*/
BOOL CObjectBone::Merge (GUID *pagWith, DWORD dwNum)
{
   BOOL fRet = FALSE;

   // must be in default position to merge
   if (!m_fBoneDefault)
      return FALSE;

   DWORD i;
   for (i = 0; i < dwNum; i++) {
      DWORD dwFind;

      // make sure it's not this object
      if (IsEqualIID (pagWith[i], m_gGUID))
         continue;

      dwFind = m_pWorld->ObjectFind (&pagWith[i]);
      if (dwFind == -1)
         continue;
      PCObjectSocket pos;
      pos = m_pWorld->ObjectGet (dwFind);
      if (!pos)
         continue;

      // send a message to see if it is another struct surface
      OSMBONE os;
      memset (&os, 0, sizeof(os));
      if (!pos->Message (OSM_BONE, &os))
         continue;
      if (!os.pb)
         continue;

      // dont merge with self
      if (os.pb == this)
         continue;

      // inform that about to change
      m_pWorld->ObjectAboutToChange (this);

      // figure out transformation
      CMatrix mTrans, mInv;
      pos->ObjectMatrixGet (&mTrans);
      m_MatrixObject.Invert4 (&mInv);
      mTrans.MultiplyRight (&mInv);

      // merge it
      fRet = TRUE;
      if (Merge (os.pb, &mTrans)) {
         // delete the other object
         m_pWorld->ObjectRemove (dwFind);
      }

      // reset bone info
      ObjEditBonesClear ();

      // inform that changed
      m_pWorld->ObjectChanged (this);
   }

   return fRet;
}

/**********************************************************************************
CObjectBone::MakeNameUnique - Makes a bone name unique.

inputs
   PCBone      pUnique - Make this one unique
returns
   none
*/
void CObjectBone::MakeNameUnique (PCBone pUnique)
{
   // get all the bones
   CListFixed lBones;
   lBones.Init (sizeof(PCBone));
   PCBone *ppb;
   DWORD i;
   ppb = (PCBone*) m_lPCBone.Get(0);
   for (i = 0; i < m_lPCBone.Num(); i++)
      ppb[i]->AddToList (&lBones);
   ppb = (PCBone*) lBones.Get(0);

   // start with case 0
   DWORD dwIndex;
   for (dwIndex = 0;  ; dwIndex++) {
      if (dwIndex) {
         // remove old numbers and add new one
         DWORD dwLen = (DWORD)wcslen(pUnique->m_szName);
         while (dwLen) {
            if ( ((pUnique->m_szName[dwLen-1] >= L'0') && (pUnique->m_szName[dwLen-1] <= L'9')) ||
               (pUnique->m_szName[dwLen-1] == L' ') ) {
                  pUnique->m_szName[dwLen-1] = 0;
                  dwLen--;
               }
            else
               break;   // found end
         }

         // make sure enough space for number
         if (dwLen > 60)
            pUnique->m_szName[60] = 0;

         swprintf (pUnique->m_szName + wcslen(pUnique->m_szName), L" %d", (int) dwIndex+1);
      }

      // if empty string redo
      if (!pUnique->m_szName[0])
         continue;   // not unique

      // see if it's unique
      for (i = 0; i < lBones.Num(); i++) {
         if (ppb[i] == pUnique)
            continue;   // dont check against itself

         // if same then break
         if (!_wcsicmp(ppb[i]->m_szName, pUnique->m_szName))
            break;
      }
      if (i >= lBones.Num())
         return;  // have unique
   }  // loop to find unique
}


/**********************************************************************************
CObjectBone::Merge - Given another bone object, this merges it into this object.

inputs
   PCObjectBone      pWith - The other object
   PCMatrix          pToObject - Convert from with coords into this object's coords
returns
   none
*/
BOOL CObjectBone::Merge (PCObjectBone pWith, PCMatrix mToObject)
{
   // find the start of the object mergin in
   CPoint pStart;
   pStart.Zero();
   pStart.p[3] = 1;
   pStart.MultiplyLeft (mToObject);

   // create a list of all the nodes in this one so can find out which is closest to
   DWORD i;
   CListFixed lThis;
   PCBone *ppb;
   lThis.Init (sizeof(PCBone));
   ppb = (PCBone*) m_lPCBone.Get(0);
   for (i = 0; i < m_lPCBone.Num(); i++)
      ppb[i]->AddToList (&lThis);

   // find which it's closest to
   DWORD dwClosest;
   CPoint pSub, pClosest;
   fp fClosest, fLen;
   pClosest.Zero();
   dwClosest = -1;   // closest to the root
   fClosest = pStart.Length();
   ppb = (PCBone*) lThis.Get(0);
   // NOTE: Using m_pEndOF (not OS), so when merge, will merge against default skeleton
   // position, not modified
   for (i = 0; i < lThis.Num(); i++) {
      pSub.Subtract (&ppb[i]->m_pEndOF, &pStart);
      fLen = pSub.Length();
      if (fLen < fClosest) {
         dwClosest = i;
         fClosest = fLen;
         pClosest.Copy (&ppb[i]->m_pEndOF);
      }
   }

   // find out what the delta is between the closest point and
   CPoint pDelta;
   pDelta.Subtract (&pClosest, &pStart);

   // new matrix adds the delta so have a perfect match
   CMatrix mTrans;
   mTrans.Translation (pDelta.p[0], pDelta.p[1], pDelta.p[2]);
   mTrans.MultiplyLeft (mToObject);

   // Get all points in bone adding to can see if bounding box makes it
   // a candidate for symmetry
   CListFixed lWith;
   PCBone *ppbWith;
   lWith.Init (sizeof(PCBone));
   ppbWith = (PCBone*) pWith->m_lPCBone.Get(0);
   for (i = 0; i < pWith->m_lPCBone.Num(); i++)
      ppbWith[i]->AddToList (&lWith);
   CPoint pMin, pMax;
   pMin.Copy (&pClosest);
   pMax.Copy (&pClosest);
   ppbWith = (PCBone*) lWith.Get(0);
   for (i = 0; i < lWith.Num(); i++) {
      CPoint pEnd;
      pEnd.Copy (&(ppbWith[i]->m_pEndOF));
      pEnd.p[3] = 1;
      pEnd.MultiplyLeft (&mTrans);
      pMin.Min (&pEnd);
      pMax.Max (&pEnd);
   }
   // zero out symmetry bits if would end up with conflict
   DWORD dwSym;
   dwSym = 0;
   for (i = 0; i < 3; i++) {
      if (!(m_dwSymmetry & (1 << i)))
         continue;   // only care if symmetry
      if ((fabs(pMin.p[i]) < CLOSE) && (fabs(pMax.p[i]) < CLOSE))
         continue;   // long straight line, so dont flip

      if (pMin.p[i] * pMax.p[i] < -CLOSE)
         continue;  // flipped object would overlap with itself so cant flip

      dwSym |= (1 << i);
   }

   DWORD x, y, z;
   CMatrix mScale;
   WCHAR szAppend[32];
   CPoint pOrigClosest;
   pOrigClosest.Copy (&pClosest);
   BOOL fFlipLR;
   for (x = 0; x < (DWORD) ((dwSym & 0x01) ? 2 : 1); x++)
   for (y = 0; y < (DWORD) ((dwSym & 0x02) ? 2 : 1); y++)
   for (z = 0; z < (DWORD) ((dwSym & 0x04) ? 2 : 1); z++) {
      // is scaling necessary
      mScale.Scale (x ? -1 : 1, y ? -1 : 1, z ? -1 : 1);
      mScale.MultiplyLeft (&mTrans);

      // what's the new start?
      pStart.Copy (&pOrigClosest);
      fFlipLR = FALSE;
      if (x) {
         pStart.p[0] *= -1;
         fFlipLR = !fFlipLR;
      }
      if (y) {
         pStart.p[1] *= -1;
         fFlipLR = !fFlipLR;
      }
      if (z) {
         pStart.p[2] *= -2;
         fFlipLR = !fFlipLR;
      }

      // find the new closest, since for each symmetry need to do this
      pClosest.Zero();
      dwClosest = -1;   // closest to the root
      fClosest = pStart.Length();
      ppb = (PCBone*) lThis.Get(0);
      for (i = 0; i < lThis.Num(); i++) {
         pSub.Subtract (&ppb[i]->m_pEndOF, &pStart);
         fLen = pSub.Length();
         if (fLen < fClosest) {
            dwClosest = i;
            fClosest = fLen;
            pClosest.Copy (&ppb[i]->m_pEndOF);
         }
      }

      // symmetry string?
      szAppend[0] = 0;
      if (dwSym) {
         if (dwSym & 0x01) {
            if ((pMin.p[0] + pMax.p[0]) * (x ? -1 : 1) >= 0)
               wcscat (szAppend, L"R");
            else
               wcscat (szAppend, L"L");
         }
         if (dwSym & 0x02) {
            if ((pMin.p[1] + pMax.p[1]) * (y ? -1 : 1) >= 0)
               wcscat (szAppend, L"B");
            else
               wcscat (szAppend, L"F");
         }
         if (dwSym & 0x04) {
            if ((pMin.p[2] + pMax.p[2]) * (z ? -1 : 1) >= 0)
               wcscat (szAppend, L"T");
            else
               wcscat (szAppend, L"B");
         }
         wcscat (szAppend, L" ");
      }
      DWORD dwAppendLen;
      dwAppendLen = (DWORD)wcslen(szAppend);


      DWORD dwWith;
      for (dwWith = 0; dwWith < pWith->m_lPCBone.Num(); dwWith++) {
         PCBone pbWith = *((PCBone*) pWith->m_lPCBone.Get(dwWith));

         // clone
         PCBone pNew;
         pNew = new CBone;
         if (!pNew)
            return FALSE;
         if (!pbWith->CloneTo (pNew)) {
            delete pNew;
            return FALSE;
         }
         
         // rotate it around
         pNew->Reorient (&mScale);

         // merge this in
         if (dwClosest == -1)
            m_lPCBone.Add (&pNew);
         else
            ppb[dwClosest]->m_lPCBone.Add (&pNew);

         // rename
         lWith.Clear();
         pNew->AddToList (&lWith);
         ppbWith = (PCBone*) lWith.Get(0);
         for (i = 0; i < lWith.Num(); i++) {
            // flip LR/definition?
            if (fFlipLR)
               ppbWith[i]->m_fFlipLR = !ppbWith[i]->m_fFlipLR;

            // prepend
            if (dwAppendLen) {
               ppbWith[i]->m_szName[63 - dwAppendLen] = 0;  // so can move
               memmove (ppbWith[i]->m_szName + dwAppendLen, ppbWith[i]->m_szName,
                  wcslen(ppbWith[i]->m_szName)*2+2);
               memcpy (ppbWith[i]->m_szName, szAppend, dwAppendLen * 2);
            }

            MakeNameUnique (ppbWith[i]);
         }

      }  // over with
   }  // over symmetry

   // recalc location in space
   CalcObjectSpace ();
   CalcAttribList();
   ResetIKWant ();
   ObjEditBonesClear();

   return TRUE;
}


/**********************************************************************************
CObjectBone::ControlPointQuery - Standard CP Query
*/
BOOL CObjectBone::ControlPointQuery (DWORD dwID, POSCONTROL pInfo)
{
   // if in the object editor and drawing, then export the bone ends to act as IK handles
   if (m_fSkeltonForOE) {
      if (dwID >= m_lBoneList.Num())
         return FALSE;

      fp fLen;
      PCBone pb;
      pb = *((PCBone*) m_lBoneList.Get(dwID));
      fLen = pb->m_pEnd.Length();
      memset (pInfo, 0, sizeof(*pInfo));
      pInfo->cColor = RGB(0xff,0,0xff);
      pInfo->dwID = dwID;
      pInfo->fPassUp = TRUE;
      pInfo->dwStyle = CPSTYLE_SPHERE;
      pInfo->fSize = fLen * pb->m_fDrawThick;
      pInfo->pLocation.Copy (&pb->m_pEndOS);
      wcscpy (pInfo->szName, pb->m_szName);

      return TRUE;
   }

   if (!m_fBoneDefault)
      return FALSE;

   PCBone pb;
   fp fLen;
   if (dwID < m_lBoneList.Num()) {
      pb = *((PCBone*) m_lBoneList.Get(dwID));
      fLen = pb->m_pEnd.Length();
      memset (pInfo, 0, sizeof(*pInfo));
      pInfo->cColor = RGB(0,0xff,0xff);
      pInfo->dwID = dwID;
      pInfo->dwStyle = CPSTYLE_SPHERE;
      pInfo->fSize = fLen * pb->m_fDrawThick;
      pInfo->pLocation.Copy (&pb->m_pEndOF);
      wcscpy (pInfo->szName, pb->m_szName);
      MeasureToString (fLen, pInfo->szMeasurement);

      return TRUE;
   }
   else if ((dwID >= 1000) && (dwID < 1010)) {
      if (m_dwCurBone >= m_lBoneList.Num())
         return FALSE;
      pb = *((PCBone*) m_lBoneList.Get(m_dwCurBone));
      fLen = pb->m_pEnd.Length();

      // calculate the vectors
      CPoint pY, pX, pZ;
      pX.Subtract (&pb->m_pEndOF, &pb->m_pStartOF);
      pX.Normalize();
      pZ.Copy (&pb->m_pUpOF);
      pY.CrossProd (&pZ, &pX);
      pY.Normalize();
      if (pb->m_fFlipLR)
         pY.Scale (-1);

      memset (pInfo, 0, sizeof(*pInfo));
      pInfo->cColor = RGB(0xff,0x0,0xff);
      pInfo->dwID = dwID;
      dwID -= 1000;  // so easier to work with
      pInfo->dwStyle = CPSTYLE_SPHERE;
      pInfo->fSize = fLen * pb->m_fDrawThick;
      PCPoint pEnv;
      if (dwID < 5) {
         pEnv = &pb->m_pEnvStart;
         pX.Scale (-1); // so pointing in opposite direction
      }
      else
         pEnv = &pb->m_pEnvEnd;
      pInfo->pLocation.Copy ((dwID < 5) ? &pb->m_pStartOF : &pb->m_pEndOF);
      switch (dwID % 5) {
      case 0:  // top
         pZ.Scale (fLen * pEnv->p[dwID%5]);
         pInfo->pLocation.Add (&pZ);
         break;
      case 1:  // right
         pY.Scale (-fLen * pEnv->p[dwID%5]);
         pInfo->pLocation.Add (&pY);
         break;
      case 2:  // bottom
         pZ.Scale (-fLen * pEnv->p[dwID%5]);
         pInfo->pLocation.Add (&pZ);
         break;
      case 3:  // left
         pY.Scale (fLen * pEnv->p[dwID%5]);
         pInfo->pLocation.Add (&pY);
         break;
      case 4:  // end point
         pX.Scale (fLen * pb->m_pEnvLen.p[(dwID < 5) ? 0 : 1]);
         pInfo->pLocation.Add (&pX);
         break;
      }
      wcscpy (pInfo->szName, L"Envelope");
      return TRUE;
   }
   return FALSE;

}

/**********************************************************************************
CObjectBone::ControlPointSet - Standard CP Query
*/
BOOL CObjectBone::ControlPointSet (DWORD dwID, PCPoint pVal, PCPoint pViewer)
{
   // if in the object editor and drawing, then export the bone ends to act as IK handles
   if (m_fSkeltonForOE) {
      m_pIKTrans.Zero();

      if (dwID >= m_lBoneList.Num())
         return FALSE;

      PCBone pBone;
      pBone = *((PCBone*) m_lBoneList.Get(dwID));
      if (pBone->m_pEndOS.AreClose (pVal))
         return TRUE;

      // use iK to move this
      m_pWorld->ObjectAboutToChange (this);
      IKMove (dwID, pVal, pViewer, &m_pIKTrans);

      // BUGFIX - Update the bone info
      if (m_plOEBONE)
         ObjEditBonesCalcPostBend ();

      m_pWorld->ObjectChanged (this);
      return TRUE;
   }

   if (!m_fBoneDefault)
      return FALSE;

   fp fLen;
   PCBone pb;
   if (dwID < m_lBoneList.Num()) {
      PCBone *ppb;
      ppb = (PCBone*) m_lBoneList.Get(0);
      pb = ppb[dwID];

      // If have symmetry on and move a point on an axis then don't allow it
      // to be moved
      CPoint pWant;
      DWORD i;
      pWant.Copy (pVal);
      if (m_dwSymmetry) {
         for (i = 0; i < 3; i++) {
            if (!(m_dwSymmetry & (1 << i)))
               continue;   // not looking for symmetry here

            if (fabs(pb->m_pEndOF.p[i]) < CLOSE)
               pWant.p[i] = 0;   // maintain the symmetry
         }  // i
      }
      
      // NOTE: Assuming drawn in default position, which is usually OK

      // find out how much it moved
      CPoint pDelta;
      pDelta.Subtract (&pWant, &pb->m_pEndOF);

      m_pWorld->ObjectAboutToChange (this);

      // what kind of symmetry? if move a point right on the edge then dont allow symmetry fixes there
      DWORD dwSym;
      dwSym = m_dwSymmetry;
      for (i = 0; i < 3; i++)
         if (fabs(pb->m_pEndOF.p[i]) < CLOSE)
            dwSym &= ~(1 << i);

      // move all points
      DWORD x, y, z;
      CPoint pComp, pDelta2;
      for (x = 0; x < (DWORD) ((dwSym & 0x01) ? 2 : 1); x++)
      for (y = 0; y < (DWORD) ((dwSym & 0x02) ? 2 : 1); y++)
      for (z = 0; z < (DWORD) ((dwSym & 0x04) ? 2 : 1); z++) {
         // which symmetry
         pComp.Copy (&pb->m_pEndOF);
         pDelta2.Copy (&pDelta);
         if (x) {
            pComp.p[0] *= -1;
            pDelta2.p[0] *= -1;
         }
         if (y) {
            pComp.p[1] *= -1;
            pDelta2.p[1] *= -1;
         }
         if (z) {
            pComp.p[2] *= -1;
            pDelta2.p[2] *= -1;
         }

         // find match
         for (i = 0; i < m_lBoneList.Num(); i++)
            if (ppb[i]->m_pEndOF.AreClose (&pComp))
               break;
         if (i >= m_lBoneList.Num())
            continue;   // not found

         // else found
         ppb[i]->m_pEnd.Add (&pDelta2);   // since no rotation involved, just add to pend

         // adjust up, picking new normal if old one doesn't match
         CPoint pB, pA, pC;
         pA.Copy (&ppb[i]->m_pEnd);
         pA.Normalize();
         DWORD j;
         for (j = 0; j < 4; j++) {
            if (!j)
               pC.Copy (&ppb[i]->m_pUp);
            else {
               pC.Zero();
               pC.p[j-1] = 1;
            }
            pB.CrossProd (&pC, &pA);
            if (pB.Length() > CLOSE)
               break;
         }
         pB.Normalize();
         pC.CrossProd (&pA, &pB);
         pC.Normalize();
         ppb[i]->m_pUp.Copy (&pC);

         // update matrices
         ppb[i]->CalcMatrix();
      }


      // change
      CalcObjectSpace();
      ResetIKWant ();

      // reset bone info
      ObjEditBonesClear ();

      m_pWorld->ObjectChanged (this);  // BUGFIX - aded this
      return TRUE;
   }
   else if ((dwID >= 1000) && (dwID < 1010)) {
      if (m_dwCurBone >= m_lBoneList.Num())
         return FALSE;
      pb = *((PCBone*) m_lBoneList.Get(m_dwCurBone));
      fLen = pb->m_pEnd.Length();
      fLen = max(fLen, CLOSE);
      dwID -= 1000;  // to make easier

      // which side from
      PCPoint pFrom;
      pFrom = (dwID < 5) ? &pb->m_pStartOF : &pb->m_pEndOF;

      // distance of new point
      CPoint pDist;
      fp fDist;
      pDist.Subtract (pVal, pFrom);
      fDist = pDist.Length() / fLen;
      fDist = max(CLOSE, fDist); // cant make exactly 0

      // set it
      m_pWorld->ObjectAboutToChange (this);
      if ((dwID % 5) == 4) {
         pb->m_pEnvLen.p[dwID / 5] = fDist;
      }
      else {
         if (dwID / 5)
            pb->m_pEnvEnd.p[dwID%5] = fDist;
         else
            pb->m_pEnvStart.p[dwID%5] = fDist;
      }

      // Mirroring
      CListFixed lPCBone, lDWORD;
      FindMirrors (pb, &lPCBone, &lDWORD);
      PCBone *ppb;
      ppb = (PCBone*) lPCBone.Get(0);
      DWORD i;
      DWORD *padw;
      padw = (DWORD*) lDWORD.Get(0);
      for (i = 0; i < lPCBone.Num(); i++) {
         ppb[i]->m_pEnvEnd.Copy (&pb->m_pEnvEnd);
         ppb[i]->m_pEnvStart.Copy (&pb->m_pEnvStart);
         ppb[i]->m_pEnvLen.Copy (&pb->m_pEnvLen);
         ppb[i]->CalcMatrix();
      }

      // reset bone info
      ObjEditBonesClear ();

      m_pWorld->ObjectChanged (this);
      return TRUE;
   }

   return FALSE;
}



/**********************************************************************************
CObjectBone::ControlPointEnum - Standard CP Query
*/
void CObjectBone::ControlPointEnum (PCListFixed plDWORD)
{
   DWORD i;

   // if in the object editor and drawing, then export the bone ends to act as IK handles
   if (m_fSkeltonForOE) {
      for (i = 0; i < m_lBoneList.Num(); i++)
         plDWORD->Add (&i);
      return;
   }

   if (!m_fBoneDefault)
      return;

   plDWORD->Required (plDWORD->Num() + m_lBoneList.Num());
   for (i = 0; i < m_lBoneList.Num(); i++)
      plDWORD->Add (&i);

   if (m_dwCurBone < m_lBoneList.Num())
      for (i = 1000; i < 1010; i++) // BUGFIX - i had started at 0
         plDWORD->Add (&i);
}


typedef struct {
   PCObjectBone      pv;   // object
   DWORD             dwCurBone;  // index into pv->m_lBoneList
   PCListFixed       plSymPCBone; // list of PCBones that are used because of symmetry
   PCListFixed       plSymDWORD;  // accompanying DWORD flag to lSymPCBone for what type of symmetry
} OBPAGE, *POBPAGE;


#if 0 // old code
/********************************************************************************
GenerateThreeDForBones - Given a list of bones, this sets a threeD
control with the spline.

inputs
   PWSTR       pszControl - Control name.
   PCEscPage   pPage - Page
   PCListFixed plBoneList - Bone list. They must have their OS values filled in
   PCBone      pRoot - Root bone. If NULL, draws all bones in bone list.
returns
   BOOl - TRUE if success

NOTE: The ID's are such:
   LOWORD = bone ID+1 (bone ID as it appears on plBoneList)
*/
static BOOL GenerateThreeDForBones (PWSTR pszControl, PCEscPage pPage, PCListFixed plBoneList,
                                    PCBone pRoot)
{
   PCEscControl pControl = pPage->ControlFind (pszControl);
   if (!pControl)
      return FALSE;

   // bone list
   CListFixed lBones;
   if (pRoot) {
      lBones.Init (sizeof(PCBone));
      pRoot->AddToList (&lBones);
   }
   else
      lBones.Init (sizeof(PCBone), plBoneList->Get(0), plBoneList->Num());
   PCBone *ppb, *ppbMain;
   DWORD dwNum, dwNumMain;
   ppb = (PCBone*) lBones.Get(0);
   dwNum = lBones.Num();
   ppbMain = (PCBone*) plBoneList->Get(0);
   dwNumMain = plBoneList->Num();

   // figure out the center
   CPoint pCenter, pMin, pMax;
   DWORD i;
   pCenter.Zero();
   pMin.Zero();
   pMax.Zero();
   DWORD x;
   for (x = 0; x < dwNum; x++) {
      if (x == 0) {
         pMin.Copy (&ppb[x]->m_pStartOF);
         pMax.Copy (&pMin);
      }
      else {
         pMin.Min (&ppb[x]->m_pStartOF);
         pMax.Max (&ppb[x]->m_pStartOF);
      }

      // include the end
      pMin.Min (&ppb[x]->m_pEndOF);
      pMax.Max (&ppb[x]->m_pEndOF);
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
   MemCat (&gMemTemp, L"<rotatex val=-90/>");

   // draw the outline
   for (x = 0; x < dwNum; x++) {
      CPoint p1, p2;
      p1.Copy (&ppb[x]->m_pStartOF);
      p2.Copy (&ppb[x]->m_pEndOF);

      // convert from HV to object space
      p1.Subtract (&pCenter);
      p2.Subtract (&pCenter);
      p1.Scale (1.0 / fMax);
      p2.Scale (1.0 / fMax);


      MemCat (&gMemTemp, (ppb[x] == pRoot) ? L"<colordefault color=#ff0000/>" : L"<colordefault color=#4040ff/>");

      // find the id
      for (i = 0; i < dwNumMain; i++)
         if (ppbMain[i] == ppb[x])
            break;
      i++;

      // set the ID
      MemCat (&gMemTemp, L"<id val=");
      MemCat (&gMemTemp, (int)i);
      MemCat (&gMemTemp, L"/>");

      MemCat (&gMemTemp, L"<shapearrow tip=false width=.1");

      WCHAR szTemp[128];
      swprintf (szTemp, L" p1=%g,%g,%g p2=%g,%g,%g/>",
         (double)p1.p[0], (double)p1.p[1], (double)p1.p[2], (double)p2.p[0], (double)p2.p[1], (double)p2.p[2]);
      MemCat (&gMemTemp, szTemp);
   }

   // set the threeD control
   ESCMTHREEDCHANGE tc;
   memset (&tc, 0, sizeof(tc));
   tc.pszMML = (PWSTR) gMemTemp.p;
   pControl->Message (ESCM_THREEDCHANGE, &tc);

   return TRUE;
}
#endif // 0 - old code

/* BoneDialogPage
*/
BOOL BoneDialogPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   POBPAGE po = (POBPAGE)pPage->m_pUserData;
   PCObjectBone pv = po->pv;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         // do nothing for now
      }
      break;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;

         if (!_wcsicmp(p->pControl->m_pszName, L"default")) {
            pv->DefaultPosition();
            return TRUE;
         }

         return TRUE;

      }
      break;   // default

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Bone settings";
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}





/* BoneEditPage
*/
BOOL BoneEditPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   POBPAGE po = (POBPAGE)pPage->m_pUserData;
   PCObjectBone pv = po->pv;
   PCBone pEdit = (po->dwCurBone < pv->m_lBoneList.Num()) ?
      *((PCBone*) pv->m_lBoneList.Get(po->dwCurBone)) : NULL;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         // Set up other attributes
         PCEscControl pControl;
         pControl = pPage->ControlFind (L"name");
         if (pControl)
            pControl->AttribSet (Text(), pEdit->m_szName);
         pControl = pPage->ControlFind (L"desc");
         if (pControl && pEdit->m_memDesc.p)
            pControl->AttribSet (Text(), (PWSTR) pEdit->m_memDesc.p);

         DWORD i;
         WCHAR szTemp[32];
         for (i = 0; i < 4; i++) {
            swprintf (szTemp, L"min%d", (int)i);
            if (i < 3)
               AngleToControl (pPage, szTemp, pEdit->m_pMotionMin.p[i], TRUE);
            else
               DoubleToControl (pPage, szTemp, pEdit->m_pMotionMin.p[i]);

            swprintf (szTemp, L"max%d", (int)i);
            if (i < 3)
               AngleToControl (pPage, szTemp, pEdit->m_pMotionMax.p[i], TRUE);
            else
               DoubleToControl (pPage, szTemp, pEdit->m_pMotionMax.p[i]);

            swprintf (szTemp, L"def%d", (int)i);
            if (i < 3)
               AngleToControl (pPage, szTemp, pEdit->m_pMotionDef.p[i], TRUE);
            else
               DoubleToControl (pPage, szTemp, pEdit->m_pMotionDef.p[i]);
         }

         pControl = pPage->ControlFind (L"uselimits");
         if (pControl)
            pControl->AttribSetBOOL (Checked(), pEdit->m_fUseLimits);
         pControl = pPage->ControlFind (L"fliplr");
         if (pControl)
            pControl->AttribSetBOOL (Checked(), pEdit->m_fFlipLR);
         pControl = pPage->ControlFind (L"canbefixed");
         if (pControl)
            pControl->AttribSetBOOL (Checked(), pEdit->m_fCanBeFixed);
         pControl = pPage->ControlFind (L"deflowrank");
         if (pControl)
            pControl->AttribSetBOOL (Checked(), pEdit->m_fDefLowRank);
         pControl = pPage->ControlFind (L"useenvelope");
         if (pControl)
            pControl->AttribSetBOOL (Checked(), pEdit->m_fUseEnvelope);
         pControl = pPage->ControlFind (L"defpassup");
         if (pControl)
            pControl->AttribSetBOOL (Checked(), pEdit->m_fDefPassUp);

         pControl = pPage->ControlFind (L"drawthick");
         if (pControl)
            pControl->AttribSetInt (Pos(), (int) (pEdit->m_fDrawThick * 100.0));


         // fill in list of objects for ignore and rigid
         pPage->Message (ESCM_USER+82);
      }
      break;

   case ESCM_USER+82:   // refresh list of rigid and objects to ignore
      {
         // fill in the list box
         MemZero (&gMemTemp);
         DWORD i, dwNum;
         GUID *pg;
         DWORD dwRigid;
         PCListFixed pl;
         for (dwRigid = 0; dwRigid < 2; dwRigid++) {
            if (dwRigid)
               pl = &pEdit->m_lObjRigid;
            else
               pl = &pEdit->m_lObjIgnore;

            dwNum = pl->Num();
            pg = (GUID*) pl->Get(0);

            for (i = 0; i < dwNum; i++) {
               // get the name
               DWORD dwFind;
               PCObjectSocket pos;
               pos = NULL;
               dwFind = pv->m_pWorld->ObjectFind (&pg[i]);
               if (dwFind != -1)
                  pos = pv->m_pWorld->ObjectGet (dwFind);
               if (!pos) {
                  // delete this and try again
                  pl->Remove (i);
                  dwNum--;
                  i--;
                  continue;
               }

               MemCat (&gMemTemp, L"<elem name=");
               MemCat (&gMemTemp, dwRigid ? L"r" : L"i");
               MemCat (&gMemTemp, (int) i);
               MemCat (&gMemTemp, L">");

               // name
               MemCat (&gMemTemp, L"<bold>");
               PWSTR psz;
               psz = pos->StringGet (OSSTRING_NAME);
               if (!psz || !psz[0])
                  psz = L"Unknown";
               MemCatSanitize (&gMemTemp, psz);
               MemCat (&gMemTemp, L"</bold><br/>&tab;<italic>");
               MemCat (&gMemTemp, dwRigid ? L"Affect uniformly" : L"Don't affect");
               MemCat (&gMemTemp, L"</italic><br/>");


               MemCat (&gMemTemp, L"</elem>");
            }
         }  // dwRigid

         // add
         ESCMLISTBOXADD la;
         PCEscControl pControl;
         pControl = pPage->ControlFind (L"envelope");
         memset (&la, 0, sizeof(la));
         la.dwInsertBefore = -1;
         la.pszMML = (PWSTR) gMemTemp.p;
         if (pControl)
            pControl->Message (ESCM_LISTBOXRESETCONTENT, NULL);
         if (la.pszMML[0] && pControl) {
            pControl->Message (ESCM_LISTBOXADD, &la);
            pControl->AttribSet (CurSel(), 0);
         }
      }
      return TRUE;

   case ESCN_EDITCHANGE:
      {
         PESCNEDITCHANGE p = (PESCNEDITCHANGE) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;
         PWSTR psz;
         psz = p->pControl->m_pszName;

         if (!_wcsicmp (psz, L"name")) {
            pv->m_pWorld->ObjectAboutToChange (pv);

            DWORD dwNeed;
            p->pControl->AttribGet (Text(), pEdit->m_szName, sizeof(pEdit->m_szName), &dwNeed);
            pv->MakeNameUnique (pEdit);
            pv->CalcAttribList();

            pv->m_pWorld->ObjectChanged (pv);
            return TRUE;
         }
         else if (!_wcsicmp (psz, L"desc")) {
            pv->m_pWorld->ObjectAboutToChange (pv);

            if (!pEdit->m_memDesc.Required (256 * 2))
               return TRUE;

            DWORD dwNeed;
            PWSTR psz;
            psz = (PWSTR) pEdit->m_memDesc.p;
            psz[0] = 0;
            p->pControl->AttribGet (Text(), psz, 250*2, &dwNeed);

            pv->m_pWorld->ObjectChanged (pv);
            return TRUE;
         }
         
         // else, must be min/max
         pv->m_pWorld->ObjectAboutToChange (pv);
         DWORD i;
         WCHAR szTemp[32];
         for (i = 0; i < 4; i++) {
            swprintf (szTemp, L"min%d", (int)i);
            if (i < 3)
               pEdit->m_pMotionMin.p[i] = AngleFromControl (pPage, szTemp);
            else {
               pEdit->m_pMotionMin.p[i] = DoubleFromControl (pPage, szTemp);
               pEdit->m_pMotionMin.p[i] = max(pEdit->m_pMotionMin.p[i], CLOSE);
            }

            swprintf (szTemp, L"max%d", (int)i);
            if (i < 3)
               pEdit->m_pMotionMax.p[i] = AngleFromControl (pPage, szTemp);
            else
               pEdit->m_pMotionMax.p[i] = DoubleFromControl (pPage, szTemp);
            pEdit->m_pMotionMax.p[i] = max(pEdit->m_pMotionMax.p[i], pEdit->m_pMotionMin.p[i]);

            swprintf (szTemp, L"def%d", (int)i);
            if (i < 3)
               pEdit->m_pMotionDef.p[i] = AngleFromControl (pPage, szTemp);
            else
               pEdit->m_pMotionDef.p[i] = DoubleFromControl (pPage, szTemp);
            pEdit->m_pMotionDef.p[i] = max(pEdit->m_pMotionDef.p[i], pEdit->m_pMotionMin.p[i]);
            pEdit->m_pMotionDef.p[i] = min(pEdit->m_pMotionDef.p[i], pEdit->m_pMotionMax.p[i]);
         }
         pEdit->CalcMatrix();

         // apply this to any mirrors
         PCBone *ppb = (PCBone*) po->plSymPCBone->Get(0);
         for (i = 0; i < po->plSymPCBone->Num(); i++) {
            ppb[i]->m_pMotionMin.Copy (&pEdit->m_pMotionMin);
            ppb[i]->m_pMotionMax.Copy (&pEdit->m_pMotionMax);
            ppb[i]->m_pMotionDef.Copy (&pEdit->m_pMotionDef);
            ppb[i]->CalcMatrix();
         }

         pv->CalcObjectSpace ();
         pv->ResetIKWant();
         pv->CalcAttribList();

         // reset bone info
         pv->ObjEditBonesClear ();

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

         if (!_wcsicmp(psz, L"envremove")) {
            // get the one
            PCEscControl pControl = pPage->ControlFind (L"envelope");
            if (!pControl)
               return TRUE;
            DWORD dwIndex;
            dwIndex = pControl->AttribGetInt (CurSel());

            // get the value
            ESCMLISTBOXGETITEM lgi;
            memset (&lgi, 0, sizeof(lgi));
            lgi.dwIndex = dwIndex;
            pControl->Message (ESCM_LISTBOXGETITEM, &lgi);
            if (!lgi.pszName) {
               pPage->MBWarning (L"You must select an object in the list box.");
               return TRUE;
            }
            DWORD dwRigid;
            PCListFixed pl;
            dwRigid = (lgi.pszName[0] == L'r');
            pl = dwRigid ? &pEdit->m_lObjRigid : &pEdit->m_lObjIgnore;
            dwIndex = _wtoi (lgi.pszName+1);
            if (!pl->Num())
               return TRUE;
            dwIndex = min(dwIndex, pl->Num()-1);

            // remove it
            pv->m_pWorld->ObjectAboutToChange (pv);
            pl->Remove (dwIndex);
            // NOTE: Not doing symmetrical because I don't think will be warranted
            pv->m_pWorld->ObjectChanged (pv);

            // refresh display
            pPage->Message (ESCM_USER+82);
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"envaddrigid") || !_wcsicmp(psz, L"envaddignore")) {
            DWORD dwRigid = !_wcsicmp(psz, L"envaddrigid");

            // make sure there's a selection
            DWORD dwNum;
            DWORD *padwSel;
            padwSel = pv->m_pWorld->SelectionEnum (&dwNum);
            if (!dwNum) {
               pPage->MBWarning (L"Use must select an object first.",
                  L"Close this dialog. Use the \"Select invidival object\" tool to "
                  L"select the object(s) you wish to add. Return to this dialog.");
               return TRUE;
            }

            PCListFixed pl;
            pl = dwRigid ? &pEdit->m_lObjRigid : &pEdit->m_lObjIgnore;

            // add it
            pv->m_pWorld->ObjectAboutToChange (pv);
            GUID g;
            DWORD i;
            PCObjectSocket pos;
            for (i = 0; i < dwNum; i++) {
               pos = pv->m_pWorld->ObjectGet (padwSel[i]);
               if (!pos)
                  continue;
               pos->GUIDGet (&g);
               pl->Add (&g);
            }
            // NOTE: Not doing symmetrical because I don't think will be warranted
            pv->m_pWorld->ObjectChanged (pv);

            // refresh display
            pPage->Message (ESCM_USER+82);
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"uselimits")) {
            pv->m_pWorld->ObjectAboutToChange (pv);

            pEdit->m_fUseLimits = p->pControl->AttribGetBOOL(Checked());

            // apply to symmetrical bones
            PCBone *ppb = (PCBone*) po->plSymPCBone->Get(0);
            DWORD i;
            for (i = 0; i < po->plSymPCBone->Num(); i++)
               ppb[i]->m_fUseLimits = pEdit->m_fUseLimits;

            pv->m_pWorld->ObjectChanged (pv);
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"fliplr")) {
            BOOL f = p->pControl->AttribGetBOOL(Checked());
            if (f == pEdit->m_fFlipLR)
               return TRUE;   // no change

            pv->m_pWorld->ObjectAboutToChange (pv);

            pEdit->m_fFlipLR = f;

            // apply to symmetrical bones
            PCBone *ppb = (PCBone*) po->plSymPCBone->Get(0);
            DWORD i, j;
            DWORD *padw;
            padw = (DWORD*) po->plSymDWORD->Get(0);
            for (i = 0; i < po->plSymPCBone->Num(); i++) {
               // count the bits
               for (j = padw[i]; j; j /= 2)
                  if (j & 0x01)
                     ppb[i]->m_fFlipLR = !ppb[i]->m_fFlipLR;   // flip along symmetry
            }

            pv->m_pWorld->ObjectChanged (pv);
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"canbefixed")) {
            BOOL f = p->pControl->AttribGetBOOL(Checked());
            if (f == pEdit->m_fCanBeFixed)
               return TRUE;   // no change

            pv->m_pWorld->ObjectAboutToChange (pv);

            pEdit->m_fCanBeFixed = f;

            // if just turning feature on then set attribute to 1 so will act as
            // fixed
            if (f) {
               pEdit->m_fFixedAmt = 1;
               pEdit->m_pIKWant.Copy (&pEdit->m_pEndOS); // since turned on update this
            }
            else
               pEdit->m_fFixedAmt = 0;

            // apply to symmetrical bones
            PCBone *ppb = (PCBone*) po->plSymPCBone->Get(0);
            DWORD i;
            DWORD *padw;
            padw = (DWORD*) po->plSymDWORD->Get(0);
            for (i = 0; i < po->plSymPCBone->Num(); i++) {
               ppb[i]->m_fCanBeFixed = pEdit->m_fCanBeFixed;
               ppb[i]->m_fFixedAmt = pEdit->m_fFixedAmt;
               ppb[i]->m_pIKWant.Copy (&ppb[i]->m_pEndOS); // since turned on update this
            }

            pv->CalcAttribList();
            pv->m_pWorld->ObjectChanged (pv);
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"deflowrank")) {
            pv->m_pWorld->ObjectAboutToChange (pv);

            pEdit->m_fDefLowRank = p->pControl->AttribGetBOOL(Checked());

            // apply to symmetrical bones
            PCBone *ppb = (PCBone*) po->plSymPCBone->Get(0);
            DWORD i;
            for (i = 0; i < po->plSymPCBone->Num(); i++)
               ppb[i]->m_fDefLowRank = pEdit->m_fDefLowRank;

            pv->m_pWorld->ObjectChanged (pv);
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"useenvelope")) {
            pv->m_pWorld->ObjectAboutToChange (pv);

            pEdit->m_fUseEnvelope = p->pControl->AttribGetBOOL(Checked());

            // apply to symmetrical bones
            PCBone *ppb = (PCBone*) po->plSymPCBone->Get(0);
            DWORD i;
            for (i = 0; i < po->plSymPCBone->Num(); i++)
               ppb[i]->m_fUseEnvelope = pEdit->m_fUseEnvelope;

            pv->m_pWorld->ObjectChanged (pv);
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"defpassup")) {
            pv->m_pWorld->ObjectAboutToChange (pv);

            pEdit->m_fDefPassUp = p->pControl->AttribGetBOOL(Checked());

            // apply to symmetrical bones
            PCBone *ppb = (PCBone*) po->plSymPCBone->Get(0);
            DWORD i;
            for (i = 0; i < po->plSymPCBone->Num(); i++)
               ppb[i]->m_fDefPassUp = pEdit->m_fDefPassUp;

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

         if (!_wcsicmp(p->pControl->m_pszName, L"drawthick")) {
            fp fVal = (fp)p->pControl->AttribGetInt (Pos()) / 100.0;
            if (fVal != pEdit->m_fDrawThick) {
               pv->m_pWorld->ObjectAboutToChange (pv);

               pEdit->m_fDrawThick = fVal;

               // apply to symmetrical bones
               PCBone *ppb = (PCBone*) po->plSymPCBone->Get(0);
               DWORD i;
               for (i = 0; i < po->plSymPCBone->Num(); i++)
                  ppb[i]->m_fDrawThick = pEdit->m_fDrawThick;

               pv->m_pWorld->ObjectChanged (pv);
            }
            return TRUE;
         }
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Modify a bone";
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}


/**********************************************************************************
CObjectBone::FindMirrors - Given a PCBone, this finds all the mirrors of it.

inputs
   PCBone      pRoot - To find the mirror of
   PCListFixed plPCBone - Initialize to sizeof (PCBone) and filled with list of mirrors
   PCListFixed plDWORD - Initialized to sizeof(DWORD) and filled with what type of mirror
returns
   none
*/
void CObjectBone::FindMirrors (PCBone pRoot, PCListFixed plPCBone, PCListFixed plDWORD)
{
   // calculate the symmetry
   PCBone *ppb;
   DWORD dwNum, x, y, z, i;
   ppb = (PCBone*) m_lBoneList.Get(0);
   dwNum = m_lBoneList.Num();
   plPCBone->Init (sizeof(PCBone));
   plPCBone->Clear();
   plDWORD->Init (sizeof(DWORD));
   plDWORD->Clear();
   for (x = 0; x < (DWORD) ((m_dwSymmetry & 0x01) ? 2 : 1); x++)
   for (y = 0; y < (DWORD) ((m_dwSymmetry & 0x02) ? 2 : 1); y++)
   for (z = 0; z < (DWORD) ((m_dwSymmetry & 0x04) ? 2 : 1); z++) {
      if (!x && !y && !z)
         continue;   // already did 0,0,0 case

      // figure out what values looking for
      CPoint pStart, pEnd;
      pStart.Copy (&pRoot->m_pStartOF);
      pEnd.Copy (&pRoot->m_pEndOF);
      if (x) {
         pStart.p[0] *= -1;
         pEnd.p[0] *= -1;
      }
      if (y) {
         pStart.p[1] *= -1;
         pEnd.p[1] *= -1;
      }
      if (z) {
         pStart.p[1] *= -1;
         pEnd.p[1] *= -1;
      }

      // find a match
      for (i = 0; i < dwNum; i++)
         if (pStart.AreClose (&ppb[i]->m_pStartOF) && pEnd.AreClose (&ppb[i]->m_pEndOF))
            break;
      if (i >= dwNum)
         continue;

      // two pointers
      PCBone pFind;
      DWORD dwSym;
      pFind = ppb[i];
      dwSym = (x ? 0x01 : 0) | (y ? 0x02 : 0) | (z ? 0x04 : 0);
      if (pFind == pRoot)
         continue;   // match

      // make sure not already there
      PCBone *ppb2;
      ppb2 = (PCBone*) plPCBone->Get(0);
      for (i = 0; i < plPCBone->Num(); i++)
         if (pFind == ppb2[i])
            break;
      if (i >= plPCBone->Num()) {
         // didnt find
         plPCBone->Add (&pFind);
         plDWORD->Add (&dwSym);
      }
   }
}

/**********************************************************************************
CObjectBone::DialogShow - Causes the object to show a dialog box that allows
different attributes to be changed.

inputs
   DWORD             dwSuface - Surface that was clicked on. 0 if unknown.
   PCEscWindow       pWindow - Escarpment window to draw it in
returns
   BOOL - TRUE if user pressed "Back", FALSE if user pressed "Close"
*/
BOOL CObjectBone::DialogShow (DWORD dwSurface, PCEscWindow pWindow, PCPoint pClick)
{
   PWSTR pszRet;
   OBPAGE oe;
   CListFixed lSymPCBone, lSymDWORD;
   lSymPCBone.Init (sizeof(PCBone));
   lSymDWORD.Init (sizeof(DWORD));
   memset (&oe, 0, sizeof(oe));
   oe.pv = this;
   oe.plSymDWORD = &lSymDWORD;
   oe.plSymPCBone = &lSymPCBone;
main:
   pszRet = pWindow->PageDialog (ghInstance, IDR_MMLBONEDIALOG, BoneDialogPage, &oe);
test:
   if (!pszRet)
      return FALSE;
   if (!_wcsicmp(pszRet, L"editbone")) {
      // calculate the symmetry
      PCBone pRoot;
      DWORD dwNum;
      PCBone *ppb;
      ppb = (PCBone*) m_lBoneList.Get(0);
      dwNum = m_lBoneList.Num();
      if (oe.dwCurBone >= dwNum)
         return FALSE; // error
      pRoot = ppb[oe.dwCurBone];
      FindMirrors (pRoot, oe.plSymPCBone, oe.plSymDWORD);

      pszRet = pWindow->PageDialog (ghInstance, IDR_MMLBONEEDIT, BoneEditPage, &oe);
      if (pszRet && !_wcsicmp(pszRet, Back()))
         goto main;
      goto test;
   }
   return !_wcsicmp(pszRet, Back());
}

/**********************************************************************************
CObjectBone::DialogBoneEdit - Causes the object to show a dialog box that allows
a specific bone to be edited

inputs
   DWORD             dwBone - Bone number clicked on.
   PCEscWindow       pWindow - Escarpment window to draw it in
returns
   BOOL - TRUE if user pressed "Back", FALSE if user pressed "Close"
*/
BOOL CObjectBone::DialogBoneEdit (DWORD dwBone, PCEscWindow pWindow)
{
   PWSTR pszRet;
   OBPAGE oe;
   CListFixed lSymPCBone, lSymDWORD;
   lSymPCBone.Init (sizeof(PCBone));
   lSymDWORD.Init (sizeof(DWORD));
   memset (&oe, 0, sizeof(oe));
   oe.pv = this;
   oe.plSymDWORD = &lSymDWORD;
   oe.plSymPCBone = &lSymPCBone;
   oe.dwCurBone = dwBone;

   // calculate the symmetry
   PCBone pRoot;
   DWORD dwNum;
   PCBone *ppb;
   ppb = (PCBone*) m_lBoneList.Get(0);
   dwNum = m_lBoneList.Num();
   if (oe.dwCurBone >= dwNum)
      return FALSE; // error
   pRoot = ppb[oe.dwCurBone];
   FindMirrors (pRoot, oe.plSymPCBone, oe.plSymDWORD);

   pszRet = pWindow->PageDialog (ghInstance, IDR_MMLBONEEDIT, BoneEditPage, &oe);
   if (!pszRet)
      return FALSE;
   return (!_wcsicmp(pszRet, Back()));
}



/**********************************************************************************
CObjectBone::DialogQuery - Returns TRUE if the object supports a custom
dialog box, FALSE if it doesn't. The default object doesn't
*/
BOOL CObjectBone::DialogQuery (void)
{
   return TRUE;
}



/*****************************************************************************************
CObjectBone::AttribGetIntern - OVERRIDE THIS

Like AttribGet() except that only called if default attributes not handled.
*/
BOOL CObjectBone::AttribGetIntern (PWSTR pszName, fp *pfValue)
{
   POBATTRIB pob;
   OBATTRIB ob;
   wcscpy (ob.szName, pszName);
   pob = (POBATTRIB) bsearch (&ob, (POBATTRIB) m_lOBATTRIB.Get(0), m_lOBATTRIB.Num(),
      sizeof(OBATTRIB), OBATTRIBCompare);
   if (!pob)
      return FALSE;

   if (pob->dwAttrib == 4) // fixed amt
      *pfValue = pob->pBone->m_fFixedAmt;
   else // one of the the 4 m_pMotionCur
      *pfValue = pob->pBone->m_pMotionCur.p[pob->dwAttrib];

   return TRUE;
}


/*****************************************************************************************
CObjectBone::AttribGetAllIntern - OVERRIDE THIS

Like AttribGetAllIntern() EXCEPT plATTRIBVAL is already initialized and filled with
some parameters (default to the object template)
*/
void CObjectBone::AttribGetAllIntern (PCListFixed plATTRIBVAL)
{
   POBATTRIB pob = (POBATTRIB) m_lOBATTRIB.Get(0);
   DWORD dwNum = m_lOBATTRIB.Num();
   DWORD i;
   if (!dwNum)
      return;

   ATTRIBVAL av;
   memset (&av, 0, sizeof(av));
   plATTRIBVAL->Required (plATTRIBVAL->Num() + dwNum);
   for (i = 0; i < dwNum; i++) {
      wcscpy (av.szName, pob[i].szName);
      if (pob[i].dwAttrib == 4)
         av.fValue = pob[i].pBone->m_fFixedAmt;
      else // m_pMotionCur
         av.fValue = pob[i].pBone->m_pMotionCur.p[pob[i].dwAttrib];
      plATTRIBVAL->Add (&av);
   }
}


/*****************************************************************************************
CObjectBone::AttribSetGroupIntern - OVERRIDE THIS

Like AttribSetGroup() except passing on non-template attributes.
*/
void CObjectBone::AttribSetGroupIntern (DWORD dwNum, PATTRIBVAL paAttrib)
{
   BOOL fSentChanged = FALSE;
   BOOL fUpdateIKWant = FALSE;

   DWORD i;
   POBATTRIB pob = (POBATTRIB) m_lOBATTRIB.Get(0);
   DWORD dwNumOB = m_lOBATTRIB.Num();
   OBATTRIB ob;
   POBATTRIB pp;
   fp *pf, fPre;
   for (i = 0; i < dwNum; i++) {
      wcscpy (ob.szName, paAttrib[i].szName);
      pp = (POBATTRIB) bsearch (&ob, pob, dwNumOB, sizeof(OBATTRIB), OBATTRIBCompare);
      if (!pp)
         continue;   // not known

      if (pp->dwAttrib == 4) // fixed amt
         pf = &pp->pBone->m_fFixedAmt;
      else { // m_pMotionCur
         pf = &pp->pBone->m_pMotionCur.p[pp->dwAttrib];
      }
      if ((*pf == paAttrib[i].fValue) || (fabs(*pf - paAttrib[i].fValue) < CLOSE))
         continue;   // no change

      fPre = *pf;

      // else, attribute
      if (!fSentChanged) {
         fSentChanged = TRUE;
         m_pWorld->ObjectAboutToChange (this);
      }
      *pf = paAttrib[i].fValue;

      // if changed want of the angles then need to update the IKWant params
      // because either user changed param or switched between objects
      if (pp->dwAttrib < 4)
         fUpdateIKWant = TRUE;


      // check for min and max
      if (pp->dwAttrib == 4) {
         *pf = max (*pf, 0);

         // BUGFIX - If turned on wanting the bone to be locked, then
         // also update the want-locked stage to the current value. If also
         // changed an angle, then will update later too when check fUpdateIKWant
         if (!fPre && *pf)
            pp->pBone->m_pIKWant.Copy (&pp->pBone->m_pEndOS);
      }
      else if (pp->pBone->m_fUseLimits) {
         *pf = max(*pf, pp->pBone->m_pMotionMin.p[pp->dwAttrib]);
         *pf = min(*pf, pp->pBone->m_pMotionMax.p[pp->dwAttrib]);

         // recalc matrix
         pp->pBone->CalcMatrix ();
      }
   }

   if (fSentChanged) {
      CalcObjectSpace ();

      // update m_pIKWant
      if (fUpdateIKWant)
         ResetIKWant ();

      // BUGFIX - Update the bone info
      if (m_plOEBONE)
         ObjEditBonesCalcPostBend ();

      m_pWorld->ObjectChanged (this);
   }
}

/*****************************************************************************************
CObjectBone::ResetIKWant - Resets all the m_pIKWant parameters to whatever the
bone is now. So that IK doesn't go crazy when start up or merge
*/
void CObjectBone::ResetIKWant (void)
{
   PCBone *ppb, pBone;
   DWORD i;
   ppb = (PCBone*) m_lBoneList.Get(0);
   for (i = 0; i < m_lBoneList.Num(); i++) {
      pBone = ppb[i];
      if (!pBone->m_fCanBeFixed)
         continue;   // dont care about this

      pBone->m_pIKWant.Copy (&pBone->m_pEndOS);
   }
}


/*****************************************************************************************
CObjectBone::AttribInfoIntern - OVERRIDE THIS

Like AttribInfo() except called if attribute is not for template.
*/
BOOL CObjectBone::AttribInfoIntern (PWSTR pszName, PATTRIBINFO pInfo)
{
   POBATTRIB pob;
   OBATTRIB ob;
   wcscpy (ob.szName, pszName);
   pob = (POBATTRIB) bsearch (&ob, (POBATTRIB) m_lOBATTRIB.Get(0), m_lOBATTRIB.Num(),
      sizeof(OBATTRIB), OBATTRIBCompare);
   if (!pob)
      return FALSE;

   memset (pInfo, 0, sizeof(*pInfo));
   pInfo->dwType = (pob->dwAttrib < 3) ? AIT_ANGLE : AIT_NUMBER;
   pInfo->fDefLowRank = pob->pBone->m_fDefLowRank;
   pInfo->fDefPassUp = pob->pBone->m_fDefPassUp;

   if (pob->dwAttrib == 4) {
      pInfo->fMax = 2;
      pInfo->fMin = 0;
      pInfo->pszDescription = L"The higher the number, the less this will move when "
         L"pulled by IK. 1.0 is good for arms, 2.0 for feet on the ground. "
         L"Use 0.0 to make it free floating.";
      pInfo->pLoc.Copy (&pob->pBone->m_pEndOS);
   }
   else {
      pInfo->fMax = pob->pBone->m_pMotionMax.p[pob->dwAttrib];
      pInfo->fMin = pob->pBone->m_pMotionMin.p[pob->dwAttrib];
      pInfo->pszDescription = ((pob->pBone->m_memDesc.p) && ((PWSTR)pob->pBone->m_memDesc.p)[0]) ?
         (PWSTR)pob->pBone->m_memDesc.p : NULL;
      pInfo->pLoc.Copy (&pob->pBone->m_pStartOS);
   }

   return TRUE;
}




// IKCHAIN - Used to indicate which members of the IK chain are fixed and how
typedef struct {
   PCBone      pBone;   // bone that is fixed
   CPoint      pLoc;    // location that it's fixed to
   BOOL        fUseViewer; // if TRUE pViewer is valid
   CPoint      pViewer; // if fUseViewer is true, then the bone can be on the line between pLoc and pViewer
   fp          fFixedAmt;  // amount that it's fixed, from EPSILON to 2. 1.0 being default
} IKCHAIN, *PIKCHAIN;

/*****************************************************************************************
CObjectBone::IKDetermineChain - Given a bone index, this determines the IK chain.
It fills in a CListFixed with IKCHAIN sturctures. The 0th element is the bone being
manipuldated directly.

inputs
   DWORD       dwBone - Bone index
   PCPoint     pLoc - Location where want the bone
   PCPoint     pViewer - If not NULL, then bone can be on the line between pLoc and pViewer
   PCListFixed plIKCHAIN - Initialized to sizeof(IKCHAIN) and filled in with the chain
returns
   BOOL - TRUE if success
*/
BOOL CObjectBone::IKDetermineChain (DWORD dwBone, PCPoint pLoc, PCPoint pViewer, PCListFixed plIKCHAIN)
{
   plIKCHAIN->Init (sizeof(IKCHAIN));
   plIKCHAIN->Clear();

   // calculate all the masseses
   DWORD i;
   PCBone *ppb;
   ppb = (PCBone*) m_lPCBone.Get(0);
   for (i = 0; i < m_lPCBone.Num(); i++)
      ppb[i]->CalcIKMass ();

   // set all the flags to false at first indicating dont need to worry about
   // for IK
   ppb = (PCBone*) m_lBoneList.Get(0);
   for (i = 0; i < m_lBoneList.Num(); i++)
      ppb[i]->m_fIKChain = FALSE;


   // get the bone
   if (dwBone >= m_lBoneList.Num())
      return FALSE;  // out of range

   // start out with the first bone
   PCBone pBone;
   IKCHAIN ik;
   pBone = *((PCBone*) m_lBoneList.Get(dwBone));
   memset (&ik, 0, sizeof(ik));
   ik.fFixedAmt = 1.0;  // do this for bone being moved
   ik.fUseViewer = (pViewer ? TRUE : FALSE);
   ik.pBone = pBone;
   ik.pLoc.Copy (pLoc);
   if (pViewer)
      ik.pViewer.Copy (pViewer);
   plIKCHAIN->Add (&ik);
   ik.fUseViewer = FALSE;  // so wont use with others

   // set the flags indicating that need to worry about for IK
   PCBone pCur;
   for (pCur = ik.pBone; pCur; pCur = pCur->m_pParent)
      pCur->m_fIKChain = TRUE;

   // loop through all the other bones and see which ones are fixed
   // if they are then include in IK chain
   ppb = (PCBone*) m_lBoneList.Get(0);
   for (i = 0; i < m_lBoneList.Num(); i++) {
      if (ppb[i] == pBone)
         continue;   // dont bother with this one since already added
      if (!ppb[i]->m_fCanBeFixed || !ppb[i]->m_fFixedAmt)
         continue;   // cant be fixed

      // add it
      ik.fFixedAmt = ppb[i]->m_fFixedAmt;
      ik.fFixedAmt *= ik.fFixedAmt; // to make sure that if set to 2, realy firm
      ik.fFixedAmt *= ik.fFixedAmt; // so if set to 2, goes to 16, very firm
      ik.pBone = ppb[i];
      // BUGFIX - The location is now sent to the want location
      ik.pLoc.Copy (&ppb[i]->m_pIKWant);
      //ik.pLoc.Copy (&ppb[i]->m_pEndOS);
      plIKCHAIN->Add (&ik);

      // set flags indicating that need to look at for IK chain
      for (pCur = ik.pBone; pCur; pCur = pCur->m_pParent)
         pCur->m_fIKChain = TRUE;
   }

   return TRUE;
}



/*****************************************************************************************
CObjectBone::IKEvaluateChain - Given a chain, and assuming all the pBone::CalcMatrix() have
been called, this repeatedly calls pBone::CalcObjectSpace() from the start of the chain
to the end (bottom of list to top) and determines where the top of the list ends up.
This is compared to the ideal, and a distance is returned.

NOTE: This dirties up the matricies and other values calculated by ::CalcObjectSpace()

inputs
   PCListFixed    plIKCHAIN - Pointer to an IKCHAIN list generated by IKDetermineChain.
   CPoint         pNewTranslate - Filled with the new translation that's used to minimize
                  the IK error. Keep this if decide to keep the IK chain. This is the amount
                  that need to translate the entire object to minimize IK error.
   PCBone         pChanged - If NULL then re-evaluates all bones in the chain. Else,
                  will just re-evlauate the pChanged and it's children
returns
   fp - Distance/error
*/
fp CObjectBone::IKEvaluateChain (PCListFixed plIKCHAIN, PCPoint pNewTranslate, PCBone pChanged)
{
   PIKCHAIN pik = (PIKCHAIN) plIKCHAIN->Get(0);
   DWORD dwNum = plIKCHAIN->Num();

   // calculate where all the bones end up, except ignore those that don't affect IK
   DWORD i;
   PCBone *ppb;
   CMatrix mNew;
   if (pChanged) {
      mNew.Copy (&pChanged->m_mCalcOS);
      pChanged->CalcObjectSpace (&mNew, FALSE, NULL, TRUE);
   }
   else {
      ppb = (PCBone*) m_lPCBone.Get(0);
      mNew.Identity ();
      for (i = 0; i < m_lPCBone.Num(); i++)
         ppb[i]->CalcObjectSpace (&mNew, FALSE, NULL, TRUE);
   }

   // using only the IKCHAIN elements fixed to a point, determine the COM before
   // and the COM now
   fp fSum;
   DWORD dwCount;
   CPoint pCOMBefore, pCOMNow, pTemp;
   fSum = 0;
   dwCount = 0;
   pCOMBefore.Zero();
   pCOMNow.Zero();
   for (i = 0; i < dwNum; i++) {
      fSum += pik[i].fFixedAmt;
      dwCount++;
      pTemp.Copy (&pik[i].pLoc);
      pTemp.Scale (pik[i].fFixedAmt);
      pCOMBefore.Add (&pTemp);
      pTemp.Copy (&pik[i].pBone->m_pEndOS);
      pTemp.Scale (pik[i].fFixedAmt);
      pCOMNow.Add (&pTemp);
   }

   // if there is a count and a sum, and there's more than one IKCHAIN then
   // move the COM
   if (fSum && (dwCount >= 2) && (dwNum >= 2)) {
      pCOMBefore.Scale (1.0 / fSum);
      pCOMNow.Scale (1.0 / fSum);
      pNewTranslate->Subtract (&pCOMBefore, &pCOMNow);
   }
   else
      pNewTranslate->Zero();  // shouldn move

   // detrmine the error
   fp fError, fCur;
   fError = 0;
   for (i = 0; i < dwNum; i++) {
      // location given translation
      pTemp.Subtract (&pik[i].pLoc, pNewTranslate);

      if (pik[i].fUseViewer) {
         CPoint pInter, pDelta;
         pDelta.Subtract (&pik[i].pLoc, &pik[i].pViewer);
         fCur = DistancePointToLine (&pik[i].pBone->m_pEndOS, &pTemp, &pDelta, &pInter);
      }
      else {
         CPoint pSub;
         pSub.Subtract (&pTemp, &pik[i].pBone->m_pEndOS);
         fCur = pSub.Length();
      }

      fCur *= pik[i].fFixedAmt;
      fError += fCur;
   }

   return fError;
}

/*****************************************************************************************
CObjectBone::IKTwiddleLink - Moves one of the links in the chain a very small amount,
and then re-evaluates the chain. From this it determines which motion it thinks is ideal
and returns a number indicating the ideal motion.

NOTE: This dirties up the matricies and other values calculated by ::CalcObjectSpace()

inputs
   PCListFixed    plIKCHAIN - Pointer to an IKCHAIN list generated by IKDetermineChain.
   DWORD          dwLink - Index into m_lBoneList to twiddle with.
   fp             fPreTwiddleError - Error calculated pre-twiddle, just call IKEvaluateChain
   fp             fStrength - Amount to twiddle. Defaults to 1.0 = about 1 degree.
returns
   int - 0 if nothing should be changed, 1..4 if settings in m_pMotionCur.p[0..3] should
      be increased positively. -1..-4 is the settings should be decreased.
*/
int CObjectBone::IKTwiddleLink (PCListFixed plIKCHAIN, DWORD dwLink, fp fPreTwiddleError,
                                fp fStrength)
{
   PIKCHAIN pik = (PIKCHAIN) plIKCHAIN->Get(0);
   DWORD dwNum = plIKCHAIN->Num();

   int iClosest = 0;
   fp fClosest, f, fAmt;
   fClosest = fPreTwiddleError;

   PCBone *ppb;
   ppb = (PCBone*) m_lBoneList.Get(0);
   PCBone pBone = ppb[dwLink];
   DWORD i, j;
   CPoint pOrig;
   pOrig.Copy (&pBone->m_pMotionCur);
   for (i = 0; i < 4; i++) {
      // if no motion allowed quick exit
      if (pBone->m_pMotionMax.p[i] <= pBone->m_pMotionMin.p[i])
         continue;
      fAmt = (i < 3) ? (PI / 180) : ((pBone->m_pMotionMax.p[i] - pBone->m_pMotionMin.p[i])/100);
      fAmt *= fStrength;

      // two directions
      for (j = 0; j < 2; j++) {
         pBone->m_pMotionCur.Copy (&pOrig);
         if (j) {
            pBone->m_pMotionCur.p[i] += fAmt;
            if (pBone->m_pMotionCur.p[i] > pBone->m_pMotionMax.p[i])
               continue;   // cant go in that direction
         }
         else {
            pBone->m_pMotionCur.p[i] -= fAmt;
            if (pBone->m_pMotionCur.p[i] < pBone->m_pMotionMin.p[i])
               continue;   // cant go in that direction
         }

         // calculate matricies
         pBone->CalcMatrix ();

         // see how it works
         CPoint pNewTrans;
         f = IKEvaluateChain (plIKCHAIN, &pNewTrans, pBone);
         if (f < fClosest) {
            fClosest = f;
            iClosest = (int) (i+1) * (j ? 1 : -1);
         }

      } // j - positive and negative
   }  // i - all possible directions


   // restore before leaving
   pBone->m_pMotionCur.Copy (&pOrig);
   pBone->CalcMatrix();
   return iClosest;
}


/*****************************************************************************************
CObjectBone::IKTwiddleAll - Calculated iTwiddle for all the elements in m_lBonelist that
have m_fIKChain set.

NOTE: This dirties up the matricies and other values calculated by ::CalcObjectSpace()

inputs
   PCListFixed    plIKCHAIN - Pointer to an IKCHAIN list generated by IKDetermineChain.
   fp             fPreTwiddleError - Error before twiddle. Just call IKEvanluateChain() to get
returns
   BOOL - TRUE if at least one twiddle returned non-zero, else FALSE means that
      no more twiddling will improve the situation
*/
BOOL CObjectBone::IKTwiddleAll (PCListFixed plIKCHAIN, fp fPreTwiddleError)
{
   PIKCHAIN pik = (PIKCHAIN) plIKCHAIN->Get(0);
   DWORD dwNum = plIKCHAIN->Num();
   DWORD i;
   BOOL fRet;
   fRet = FALSE;

   // calculate the pre-twiddle error
   fp fError;
   fError = fPreTwiddleError;

   PCBone *ppb;
   PCBone pBone;
   ppb = (PCBone*) m_lBoneList.Get(0);
   for (i = 0; i < m_lBoneList.Num(); i++) {
      pBone = ppb[i];
      if (!pBone->m_fIKChain) {
         pBone->m_iIKTwiddle = 0;
         continue;   // dont want to twiddle with this
      }

      pBone->m_iIKTwiddle = IKTwiddleLink (plIKCHAIN, i, fError);
      fRet |= (pBone->m_iIKTwiddle != 0);

#if 0//def _DEBUG
      char szTemp[128], szTemp2[64];
      WideCharToMultiByte (CP_ACP, 0, pBone->m_szName, -1, szTemp2, sizeof(szTemp2), 0, 0);
      sprintf (szTemp, "Twiddle %s = %d\r\n", szTemp2, pBone->m_iIKTwiddle);
      OutputDebugString (szTemp);
#endif
   }

#if 0//def _DEBUG
   OutputDebugString ("*** End twiddle\r\n");
#endif
   return fRet;
}


/*****************************************************************************************
CObjectBone::IKApplyTwiddle - Looks at the iTwiddle in the IKCHAIN list. Based on this,
it sets the m_pMotionCur in every bone to pBone->m_pIKOrig + direction twiddle points in *
fStrength / mass division.

Make sure to call IKTwiddleAll() before this.

NOTE: This dirties up the matricies and other values calculated by ::CalcObjectSpace().
   It also dirties m_pMotionCur in all the bones.

inputs
   PCListFixed    plIKCHAIN - Pointer to an IKCHAIN list generated by IKDetermineChain.
   fp             fStrength - Start out at 1.0 and double each time calling. 1.0 is only
                     a very small movement.
   DWORD          *padwSpecific - If not NULL then apply the twiddle to a specific
                     index into m_lBoneList as specified by *padwSPecific. If NULL
                     then apply to all bones with non-zero ikTwiddle
returns
   BOOL - TRUE if success, FALSE if nothing to twiddle
*/
BOOL CObjectBone::IKApplyTwiddle (PCListFixed plIKCHAIN, fp fStrength, DWORD *padwSpecific)
{
   PIKCHAIN pik = (PIKCHAIN) plIKCHAIN->Get(0);
   DWORD dwNum = plIKCHAIN->Num();
   DWORD i;

   PCBone *ppb;
   ppb = (PCBone*) m_lBoneList.Get(0);

   // figure out the lowest mass
   fp fFirstMass;
   BOOL fFound;
   if (!padwSpecific) {
      fFound = FALSE;
      for (i = (padwSpecific ? *padwSpecific : 0); i < (padwSpecific ? (*padwSpecific + 1) : m_lBoneList.Num()); i++) {
         if (!ppb[i]->m_iIKTwiddle)
            continue;

         if (!fFound || (ppb[i]->m_fIKMass < fFirstMass)) {
            fFirstMass = ppb[i]->m_fIKMass;
            fFound = TRUE;
         }
      }

      // adjust the strength by the first mass
      if (!fFound)
         return FALSE;
      fStrength *= fFirstMass * fFirstMass;
      // BUGFIX - Use the square of masses because moving  too much of object
   }

   // apply the twiddle
   PCBone pBone;
   fp fAmt;
   DWORD dwDim;
   for (i = (padwSpecific ? *padwSpecific : 0); i < (padwSpecific ? (*padwSpecific + 1) : m_lBoneList.Num()); i++) {
      pBone = ppb[i];

      pBone->m_pMotionCur.Copy (&pBone->m_pIKOrig);

      if (pBone->m_iIKTwiddle) {
         dwDim = (DWORD)abs(pBone->m_iIKTwiddle) - 1;
         fAmt = (dwDim < 3) ? (PI / 180) : ((pBone->m_pMotionMax.p[dwDim] - pBone->m_pMotionMin.p[dwDim])/100);
         if (pBone->m_iIKTwiddle < 0)
            fAmt *= -1;
         fAmt *= fStrength;
         if (!padwSpecific)
            fAmt /= (pBone->m_fIKMass * pBone->m_fIKMass); // BUGFIX - Use square of masses

         pBone->m_pMotionCur.p[dwDim] += fAmt;
         pBone->m_pMotionCur.p[dwDim] = max(pBone->m_pMotionCur.p[dwDim], pBone->m_pMotionMin.p[dwDim]);
         pBone->m_pMotionCur.p[dwDim] = min(pBone->m_pMotionCur.p[dwDim], pBone->m_pMotionMax.p[dwDim]);
      }

      pBone->CalcMatrix ();
   }

   return TRUE;
}


/*****************************************************************************************
CObjectBone::IKBestStrength - Loops through all the bones in the chain and remembers
their current m_pMotionCur into m_pMotionOrig. This then figures out which twiddling to
try, and then applies the twiddle at varrying strengths until the closes twiddle is met.
The new m_pMotionCur are set.

inputs
   PCListFixed    plIKCHAIN - Pointer to an IKCHAIN list generated by IKDetermineChain.
   PCPoint        pNewTrans - Filled in with the new translation for the best choice
returns
   BOOL - TRUE if success, FALSE if no more twiddling can be done because any more
      twiddling just causes a larger error than already have.
*/
BOOL CObjectBone::IKBestStrength (PCListFixed plIKCHAIN, PCPoint pNewTrans)
{
   PIKCHAIN pik = (PIKCHAIN) plIKCHAIN->Get(0);
   DWORD dwNum = plIKCHAIN->Num();
   DWORD i;

   // store values away
   PCBone *ppb;
   ppb = (PCBone*) m_lBoneList.Get(0);
   for (i = 0; i < m_lBoneList.Num(); i++)
      ppb[i]->m_pIKOrig.Copy (&ppb[i]->m_pMotionCur);

   // see how far the best distance is if do nothing
   fp fBestStrength;
   fp fClosest;
   CPoint pBestTrans, pTemp;
   fBestStrength = 0;
   fClosest = IKEvaluateChain (plIKCHAIN, &pBestTrans);

   // find twiddle bits
   if (!IKTwiddleAll (plIKCHAIN, fClosest))
      return FALSE;  // twiddling makes no difference so dont try

   // loop, seeing which has the best distance
   fp fCur, fLen;
   for (fCur = 1; fCur < 360; fCur *= 2) {
      if (!IKApplyTwiddle (plIKCHAIN, fCur))
         break;   // errored out

      fLen = IKEvaluateChain (plIKCHAIN, &pTemp);
      if (fLen > fClosest)
         break;   // not getting any closer so give up

      // else, got closer
      fClosest = fLen;
      fBestStrength = fCur;
      pBestTrans.Copy (&pTemp);
   }

   // restore the best strength, which may be 0
   IKApplyTwiddle (plIKCHAIN, fBestStrength);

   pNewTrans->Copy (&pBestTrans);
   return (fBestStrength ? TRUE : FALSE); // if best is no change then return FALSE
}

/*****************************************************************************************
CObjectBone::IKSlowConverge - This function is an attempt to do better IK, by doing
a slower convergence process.

inputs
   PCListFixed    plIKCHAIN - Pointer to an IKCHAIN list generated by IKDetermineChain.
   PCPoint        pNewTrans - Filled in with the new translation for the best choice
   fp             fStrength - Strength to twiddle with.
returns
   BOOL - TRUE if success, FALSE if no more twiddling can be done because any more
      twiddling just causes a larger error than already have.
*/
BOOL CObjectBone::IKSlowConverge (PCListFixed plIKCHAIN, PCPoint pNewTrans, fp fStrength)
{
   PIKCHAIN pik = (PIKCHAIN) plIKCHAIN->Get(0);
   DWORD dwNum = plIKCHAIN->Num();
   DWORD dwTwiddled = 0;

   // find the smallest mass that will move
   PCBone *ppb;
   PCBone pBone;
   ppb = (PCBone*) m_lBoneList.Get(0);
   DWORD i;
   BOOL fFound;
   fFound = FALSE;
   fp fMass;
   fMass = 0;
   for (i = 0; i < m_lBoneList.Num(); i++) {
      pBone = ppb[i];
      if (!pBone->m_fIKChain)
         continue;   // not in the chain so don't care

      if (!fFound || (pBone->m_fIKMass < fMass)) {
         fMass = pBone->m_fIKMass;
         fFound = TRUE;
      }
   }
   if (!fFound && !fMass)
      return FALSE;  // shouldnt happen

   // crrent error
   fp fCurError, fCurStrength;
   fCurError = IKEvaluateChain (plIKCHAIN, pNewTrans);

   // loop through all the bones
   for (i = 0; i < m_lBoneList.Num(); i++) {
      pBone = ppb[i];
      if (!pBone->m_fIKChain)
         continue;   // not in the chain so don't care

      fCurStrength = fMass / pBone->m_fIKMass;

      // see if twiddling workings
      pBone->m_iIKTwiddle = IKTwiddleLink (plIKCHAIN, i, fCurError, fCurStrength);
      if (pBone->m_iIKTwiddle) {
         // twiddle
         pBone->m_pIKOrig.Copy (&pBone->m_pMotionCur);
         if (IKApplyTwiddle (plIKCHAIN, fCurStrength, &i))
            dwTwiddled++;
      }

      // recalc chain for next twiddle
      fCurError = IKEvaluateChain (plIKCHAIN, pNewTrans, pBone);
   }

   return (dwTwiddled ? TRUE : FALSE);
}

/*****************************************************************************************
CObjectBone::IKMove - Uses IK to move a bone from its current location to the end point.

NOTE: This does NOT call m_pWorld->ObjectAboutToChange()

inputs
   DWORD          dwBone - Bone index into m_lBoneList
   PCPoint        pWant - What want the point to be
   PCPoint        pViewer - If NULL, finds closest match to pWant. If NOT NULL then
                     closest match of end bone to the line between pWant and pViewer
   PCPoint        pNewTrans - Filled with the amount that need to translate the object
                     after the IK move. This ensures the object moves along properly.
returns
   none
*/
void CObjectBone::IKMove (DWORD dwBone, PCPoint pWant, PCPoint pViewer, PCPoint pNewTrans)
{
   CListFixed lChain;
   CPoint pBestTrans;//, pTemp;
   pBestTrans.Zero();
   if (!IKDetermineChain (dwBone, pWant, pViewer, &lChain))
      return;  // do nothing

   // repeat trying to pull the bone in various directions. Max tries = 20
   DWORD i;
   fp fStrength;
   for (fStrength = 10; fStrength >= .1; fStrength /= 2) {
      for (i = 0; i < 100; i++) {
         if (!IKSlowConverge (&lChain, &pBestTrans, fStrength))
            break;
      }
#ifdef _DEBUG
   char szTemp[64];
   sprintf (szTemp, "IK tries at strength %f = %d\r\n", (double) fStrength, (int) i);
   OutputDebugString (szTemp);
#endif
   }
#if 0 // old code
   for (i = 0; i < 10; i++) {
      if (!IKBestStrength (&lChain, &pTemp))
         break;

      pBestTrans.Copy (&pTemp);
   }
#endif // 0

   // recalc the object info
   CalcObjectSpace ();
   pNewTrans->Copy (&pBestTrans);

   // BUGFIX - Go through all the IK's that are fixed and update their pIKWant location
   // by pBestTrans. That way they'll still want effectively the same location
   PCBone *ppb, pBone;
   ppb = (PCBone*) m_lBoneList.Get(0);
   for (i = 0; i < m_lBoneList.Num(); i++) {
      pBone = ppb[i];
      if (!pBone->m_fCanBeFixed || !pBone->m_fFixedAmt)
         continue;   // dont care about this
      
      // if this is the one that was modifying then just set want to current value
      if (dwBone == i) {
         pBone->m_pIKWant.Copy (&pBone->m_pEndOS);
         continue;
      }

      // else, move by best trans
      pBone->m_pIKWant.Subtract (pNewTrans);
   }

}


#if 0 // OLD IK CODE - Only handled one IK point, so not used anymore

/* IKCHAIN - Store information about the IK chain */
typedef struct {
   PCBone         pBone;         // bone that's used
   CPoint         pMotionOrig;   // original motion, used for testing
   fp             fMass;         // mass of the bone MINUS the mass of the child in the IK chain
   int            iTwiddle;      // twiddle direction... 0 for none. 1..4 for m_pMotionCur.p[0..3], -1..-4 for negative values
} IKCHAIN, *PIKCHAIN;



/*****************************************************************************************
CObjectBone::IKMove - Uses IK to move a bone from its current location to the end point.

NOTE: This does NOT call m_pWorld->ObjectAboutToChange()

inputs
   DWORD          dwBone - Bone index into m_lBoneList
   PCPoint        pWant - What want the point to be
   PCPoint        pViewer - If NULL, finds closest match to pWant. If NOT NULL then
                     closest match of end bone to the line between pWant and pViewer
returns
   none
*/
void CObjectBone::IKMove (DWORD dwBone, PCPoint pWant, PCPoint pViewer)
{
   CListFixed lChain;
   if (!IKDetermineChain (dwBone, &lChain))
      return;  // do nothing

   // repeat trying to pull the bone in various directions. Max tries = 20
   DWORD i;
   for (i = 0; i < 10; i++) {
      if (!IKBestStrength (&lChain, pWant, pViewer))
         break;
   }
#ifdef _DEBUG
   char szTemp[64];
   sprintf (szTemp, "IK tries = %d\r\n", (int) i);
   OutputDebugString (szTemp);
#endif

   // recalc the object info
   CalcObjectSpace ();
}

/*****************************************************************************************
CObjectBone::IKBestStrength - Loops through all the bones in the chain and remembers
their current m_pMotionCur into m_pMotionOrig. This then figures out which twiddling to
try, and then applies the twiddle at varrying strengths until the closes twiddle is met.
The new m_pMotionCur are set.

inputs
   PCListFixed    plIKCHAIN - Pointer to an IKCHAIN list generated by IKDetermineChain.
   PCPoint        pWant - Value that want for the end point
   PCPoint        pViewer - If NULL, finds closest match to pWant. If NOT NULL then
                     closest match of end bone to the line between pWant and pViewer
returns
   BOOL - TRUE if success, FALSE if no more twiddling can be done because any more
      twiddling just causes a larger error than already have.
*/
BOOL CObjectBone::IKBestStrength (PCListFixed plIKCHAIN, PCPoint pWant, PCPoint pViewer)
{
   PIKCHAIN pik = (PIKCHAIN) plIKCHAIN->Get(0);
   DWORD dwNum = plIKCHAIN->Num();
   DWORD i;

   // store values away
   for (i = 0; i < dwNum; i++)
      pik[i].pMotionOrig.Copy (&pik[i].pBone->m_pMotionCur);

   // see how far the best distance is if do nothing
   fp fBestStrength;
   fp fClosest;
   fBestStrength = 0;
   fClosest = IKEvaluateChain (plIKCHAIN, pWant, pViewer);

   // find twiddle bits
   if (!IKTwiddleAll (plIKCHAIN, pWant, pViewer))
      return FALSE;  // twiddling makes no difference so dont try

   // loop, seeing which has the best distance
   fp fCur, fLen;
   for (fCur = 1; fCur < 360; fCur *= 2) {
      if (!IKApplyTwiddle (plIKCHAIN, fCur))
         break;   // errored out

      fLen = IKEvaluateChain (plIKCHAIN, pWant, pViewer);
      if (fLen >= fClosest)
         break;   // not getting any closer so give up

      // else, got closer
      fClosest = fLen;
      fBestStrength = fCur;
   }

   // restore the best strength, which may be 0
   IKApplyTwiddle (plIKCHAIN, fBestStrength);

   return (fBestStrength ? TRUE : FALSE); // if best is no change then return FALSE
}

/*****************************************************************************************
CObjectBone::IKApplyTwiddle - Looks at the iTwiddle in the IKCHAIN list. Based on this,
it sets the m_pMotionCur in every bone to IKCHAIN.pMotionOrig + direction twiddle points in *
fStrength / mass division.

Make sure to call IKTwiddleAll() before this.

NOTE: This dirties up the matricies and other values calculated by ::CalcObjectSpace().
   It also dirties m_pMotionCur in all the bones.

inputs
   PCListFixed    plIKCHAIN - Pointer to an IKCHAIN list generated by IKDetermineChain.
   fp             fStrength - Start out at 1.0 and double each time calling. 1.0 is only
                     a very small movement.
returns
   BOOL - TRUE if success, FALSE if nothing to twiddle
*/
BOOL CObjectBone::IKApplyTwiddle (PCListFixed plIKCHAIN, fp fStrength)
{
   PIKCHAIN pik = (PIKCHAIN) plIKCHAIN->Get(0);
   DWORD dwNum = plIKCHAIN->Num();
   DWORD i;

   // figureout out the first mass in the chain because other masses (furhter down)
   // will move less
   fp fFirstMass;
   fFirstMass = 0;
   for (i = 0; i < dwNum; i++)
      if (pik[i].iTwiddle) {
         fFirstMass = pik[i].fMass;
         break;
      }

   // adjust the strength by the first mass
   if (!fFirstMass)
      return FALSE;
   fStrength *= fFirstMass * fFirstMass;
   // BUGFIX - Use the square of masses because moving  too much of object

   // apply the twiddle
   PCBone pBone;
   fp fAmt;
   DWORD dwDim;
   for (i = 0; i < dwNum; i++) {
      pBone = pik[i].pBone;

      pBone->m_pMotionCur.Copy (&pik[i].pMotionOrig);

      if (pik[i].iTwiddle) {
         dwDim = (DWORD)abs(pik[i].iTwiddle) - 1;
         fAmt = (dwDim < 3) ? (PI / 180) : ((pBone->m_pMotionMax.p[dwDim] - pBone->m_pMotionMin.p[dwDim])/100);
         if (pik[i].iTwiddle < 0)
            fAmt *= -1;
         fAmt *= fStrength / (pik[i].fMass * pik[i].fMass); // BUGFIX - Use square of masses

         pBone->m_pMotionCur.p[dwDim] += fAmt;
         pBone->m_pMotionCur.p[dwDim] = max(pBone->m_pMotionCur.p[dwDim], pBone->m_pMotionMin.p[dwDim]);
         pBone->m_pMotionCur.p[dwDim] = min(pBone->m_pMotionCur.p[dwDim], pBone->m_pMotionMax.p[dwDim]);
      }

      pBone->CalcMatrix ();
   }

   return TRUE;
}

/*****************************************************************************************
CObjectBone::IKTwiddleAll - Calculated iTwiddle for all the elements in IKCHAIN

NOTE: This dirties up the matricies and other values calculated by ::CalcObjectSpace()

inputs
   PCListFixed    plIKCHAIN - Pointer to an IKCHAIN list generated by IKDetermineChain.
   PCPoint        pWant - Value that want for the end point
   PCPoint        pViewer - If NULL, finds closest match to pWant. If NOT NULL then
                     closest match of end bone to the line between pWant and pViewer
returns
   BOOL - TRUE if at least one twiddle returned non-zero, else FALSE means that
      no more twiddling will improve the situation
*/
BOOL CObjectBone::IKTwiddleAll (PCListFixed plIKCHAIN, PCPoint pWant, PCPoint pViewer)
{
   PIKCHAIN pik = (PIKCHAIN) plIKCHAIN->Get(0);
   DWORD dwNum = plIKCHAIN->Num();
   DWORD i;
   BOOL fRet;
   fRet = FALSE;

   for (i = 0; i < dwNum; i++) {
      pik[i].iTwiddle = IKTwiddleLink (plIKCHAIN, pWant, pViewer, i);
      fRet |= (pik[i].iTwiddle != 0);
   }

   return fRet;
}

/*****************************************************************************************
CObjectBone::IKTwiddleLink - Moves one of the links in the chain a very small amount,
and then re-evaluates the chain. From this it determines which motion it thinks is ideal
and returns a number indicating the ideal motion.

NOTE: This dirties up the matricies and other values calculated by ::CalcObjectSpace()

inputs
   PCListFixed    plIKCHAIN - Pointer to an IKCHAIN list generated by IKDetermineChain.
   PCPoint        pWant - Value that want for the end point
   PCPoint        pViewer - If NULL, finds closest match to pWant. If NOT NULL then
                     closest match of end bone to the line between pWant and pViewer
   DWORD          dwLink - Index into plIKCHAIN to twiddle with.
returns
   int - 0 if nothing should be changed, 1..4 if settings in m_pMotionCur.p[0..3] should
      be increased positively. -1..-4 is the settings should be decreased.
*/
int CObjectBone::IKTwiddleLink (PCListFixed plIKCHAIN, PCPoint pWant, PCPoint pViewer, DWORD dwLink)
{
   PIKCHAIN pik = (PIKCHAIN) plIKCHAIN->Get(0);
   DWORD dwNum = plIKCHAIN->Num();

   int iClosest = 0;
   fp fClosest, f, fAmt;
   fClosest = IKEvaluateChain (plIKCHAIN, pWant, pViewer);  // must be closer than what currently have
   PCBone pBone = pik[dwLink].pBone;
   DWORD i, j;
   CPoint pOrig;
   pOrig.Copy (&pBone->m_pMotionCur);
   for (i = 0; i < 4; i++) {
      // if no motion allowed quick exit
      if (pBone->m_pMotionMax.p[i] <= pBone->m_pMotionMin.p[i])
         continue;
      fAmt = (i < 3) ? (PI / 180) : ((pBone->m_pMotionMax.p[i] - pBone->m_pMotionMin.p[i])/100);

      // two directions
      for (j = 0; j < 2; j++) {
         pBone->m_pMotionCur.Copy (&pOrig);
         if (j) {
            pBone->m_pMotionCur.p[i] += fAmt;
            if (pBone->m_pMotionCur.p[i] > pBone->m_pMotionMax.p[i])
               continue;   // cant go in that direction
         }
         else {
            pBone->m_pMotionCur.p[i] -= fAmt;
            if (pBone->m_pMotionCur.p[i] < pBone->m_pMotionMin.p[i])
               continue;   // cant go in that direction
         }

         // calculate matricies
         pBone->CalcMatrix ();

         // see how it works
         f = IKEvaluateChain (plIKCHAIN, pWant, pViewer);
         if (f < fClosest) {
            fClosest = f;
            iClosest = (int) (i+1) * (j ? 1 : -1);
         }

      } // j - positive and negative
   }  // i - all possible directions


   // restore before leaving
   pBone->m_pMotionCur.Copy (&pOrig);
   pBone->CalcMatrix();
   return iClosest;
}

/*****************************************************************************************
CObjectBone::IKEvaluateChain - Given a chain, and assuming all the pBone::CalcMatrix() have
been called, this repeatedly calls pBone::CalcObjectSpace() from the start of the chain
to the end (bottom of list to top) and determines where the top of the list ends up.
This is compared to the ideal, and a distance is returned.

NOTE: This dirties up the matricies and other values calculated by ::CalcObjectSpace()

inputs
   PCListFixed    plIKCHAIN - Pointer to an IKCHAIN list generated by IKDetermineChain.
   PCPoint        pWant - Value that want for the end point
   PCPoint        pViewer - If NULL, finds closest match to pWant. If NOT NULL then
                     closest match of end bone to the line between pWant and pViewer
returns
   fp - Distance
*/
fp CObjectBone::IKEvaluateChain (PCListFixed plIKCHAIN, PCPoint pWant, PCPoint pViewer)
{
   PIKCHAIN pik = (PIKCHAIN) plIKCHAIN->Get(0);
   DWORD dwNum = plIKCHAIN->Num();

   // loop backwards
   CMatrix mCur, mNew;
   DWORD i;
   mCur.Copy (&pik[dwNum-1].pBone->m_mCalcOS);   // start out at last point
   for (i = dwNum-1; i < dwNum; i--) {
      // figure out what mNew is
      pik[i].pBone->CalcObjectSpace (&mCur, FALSE, &mNew);

      // copy mNew to mCur
      mCur.Copy (&mNew);
   }

   // see where the end point is
   if (pViewer) {
      CPoint pInter, pDelta;
      pDelta.Subtract (pWant, pViewer);
      return DistancePointToLine (&pik[0].pBone->m_pEndOS, pWant, &pDelta, &pInter);
   }
   else {
      CPoint pSub;
      pSub.Subtract (pWant, &pik[0].pBone->m_pEndOS);
      return pSub.Length();
   }
}


/*****************************************************************************************
CObjectBone::IKDetermineChain - Given a bone index, this determines the IK chain.
It fills in a CListFixed with IKCHAIN sturctures. The 0th element is the bone being
manipuldated directly.

inputs
   DWORD       dwBone - Bone index
   PCListFixed plIKCHAIN - Initialized to sizeof(IKCHAIN) and filled in with the chain
returns
   BOOL - TRUE if success
*/
BOOL CObjectBone::IKDetermineChain (DWORD dwBone, PCListFixed plIKCHAIN)
{
   plIKCHAIN->Init (sizeof(IKCHAIN));
   plIKCHAIN->Clear();

   // get the bone
   if (dwBone >= m_lBoneList.Num())
      return FALSE;  // out of range

   // start out with the first bone
   PCBone pBone;
   IKCHAIN ik;
   pBone = *((PCBone*) m_lBoneList.Get(dwBone));
   fp fTotalMass, fMass;
   fTotalMass = 0;
   while (pBone) {
      // fill it in
      memset (&ik, 0, sizeof(ik));
      ik.pBone = pBone;
      ik.pMotionOrig.Copy (&pBone->m_pMotionCur);

      // calculate the mass
      fMass = pBone->CalcMass();
      ik.fMass = fMass; // dont think want to subtract - fTotalMass;   // dont include the mass from before
      fTotalMass += fMass;

      plIKCHAIN->Add (&ik);

      // move up
      pBone = pBone->m_pParent;
   }

   return TRUE;
}

#endif // 0 - OLD IK CHAIN



/**************************************************************************************
CObjectBone::MoveReferencePointQuery - 
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
BOOL CObjectBone::MoveReferencePointQuery (DWORD dwIndex, PCPoint pp)
{
   PSPLINEMOVEP ps;
   DWORD dwDataSize;
   ps = gaObjectMove;
   dwDataSize = sizeof(gaObjectMove);

   if (dwIndex >= dwDataSize / sizeof(SPLINEMOVEP))
      return FALSE;

   // always at 0,0 in ObjectEditor
   pp->Zero();
   return TRUE;
}

/**************************************************************************************
CObjectBone::MoveReferenceStringQuery -
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
BOOL CObjectBone::MoveReferenceStringQuery (DWORD dwIndex, PWSTR psz, DWORD dwSize, DWORD *pdwNeeded)
{
   PSPLINEMOVEP ps;
   DWORD dwDataSize;
   DWORD dwNeeded;
   ps = gaObjectMove;
   dwDataSize = sizeof(gaObjectMove);
   if (dwIndex >= dwDataSize / sizeof(SPLINEMOVEP)) {
      if (pdwNeeded)
         *pdwNeeded = 0;
      return FALSE;
   }

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

/*********************************************************************************
CObjectBone::ObjEditBonePointConvert - Takes a point in world space, and outputs it after the
bones have deformed it.

inputs
   PCPoint        pPointWorld - Point in world space.
   PCPoint        pNormWorld - Normal in world space, associated with point. Can be NULL.
   PCPoint        pPointDeform - Filled in with the point as it gets deforms
   PCPoint        pNormDeform - Filled in with the normal as it gets deformed. Can be null.
   DWORD          dwNumExistWeight - Normally 0, but if non-zero then the bone weights
                  are not calculated based on the points location relative to the bone,
                  but on the weights provided.
   PCBone         *papExistWeightBone - Pointer to an array of dwNumExistWeight PCBone.
                  These are the bones that should have their weight set
   fp             *pafExistWeight - Pointer to an array of dwNumExistWeight fp's.
                  These are the weights to set.
   
   Uses the plBoneAffect list to figure out what bones will
   Also assumes that ObjEditBonesTestObject() have already been called).
   Also modified plOEBW.

   NOTE: Modifies m_plOEBW in this process. This is used by the polygon mesh when
   building default bone dependences.
returns
   none
*/
void CObjectBone::ObjEditBonePointConvert (PCPoint pPointWorld, PCPoint pNormWorld,
                              PCPoint pPointDeform, PCPoint pNormDeform,
                              DWORD dwNumExistWeight, PCBone *papExistWeightBone, fp *pafExistWeight)
{
   // set all weights to one
   DWORD dwNum = m_plBoneAffect->Num();
   DWORD dwNumLeft = dwNum;
   POEBONE *ppb = (POEBONE*) m_plBoneAffect->Get(0);
   POEBW pbw = (POEBW) m_plOEBW->Get(0);
   DWORD i, j;
   CMatrix mTwist;
   if (dwNumExistWeight) {
      for (i = 0; i < dwNum; i++)
         pbw[i].fWeight = 0;  // default

      for (i = 0; i < dwNumExistWeight; i++) {
         for (j = 0; j < dwNum; j++)
            if (ppb[j]->pBone == papExistWeightBone[i])
               break;
         if (j >= dwNum)
            continue;   // not found

         pbw[j].fWeight = pafExistWeight[i];
      } // i
   }
   else {
      // detect weights
      for (i = 0; i < dwNum; i++)
         pbw[i].fWeight = 1;

      // if there's more than one bone then do a bounding box test for rejection
      if (dwNum > 1) {
         for (i = 0; i < dwNum; i++) {
            for (j = 0; j < 3; j++)
               if ((pPointWorld->p[j] <= ppb[i]->apBound[0].p[j]) || (pPointWorld->p[j] >= ppb[i]->apBound[1].p[j]))
                  break;
            if (j < 3) {
               // dont need to test since outside the bounding box
               pbw[i].fWeight = 0;
               dwNumLeft--;
            }
         }  // i
      }  // dwNum > 1

      // if nothing left then just copy over
      if (!dwNumLeft) {
   nochange:
         pPointDeform->Copy (pPointWorld);
         if (pNormDeform)
            pNormDeform->Copy (pNormWorld);
         return;
      }
   }

   // loop through all the points and convert them to object space
   for (i = 0; i < dwNum; i++) {
      // ignore ones already eliminated
      if (!pbw[i].fWeight)
         continue;

      // conver from world to bone space
      pbw[i].pBone.Copy (pPointWorld);
      pbw[i].pBone.p[3] = 1;
      pbw[i].pBone.MultiplyLeft (&ppb[i]->mPreToBone);

      // calculate how far along x
      fp fDistX;
      BOOL fTwist;
      fTwist = (ppb[i]->pBone->m_pMotionCur.p[2] != ppb[i]->pBone->m_pMotionDef.p[2]);
      if (((dwNumLeft > 1) && !dwNumExistWeight) || fTwist) {
         fDistX = pbw[i].pBone.p[0] / ppb[i]->fPreBoneLen;
         fDistX = max(0, fDistX);
         fDistX = min(1, fDistX);

         if (m_fBoneAffectUniform)
            fDistX = 1; // used for twist, so assume twisted all the way?
      }

      // if there's more than one bone then make sure this is in the envelope.
      // NOTE: If dwNumExistWeight then dont do
      if ((dwNumLeft > 1) && !dwNumExistWeight)  {
         CPoint pDist;
         pDist.Copy (&pbw[i].pBone);
         pDist.Scale (1.0 / ppb[i]->fPreBoneLen);

         // x
         if (fDistX <= 0)   // hemicircle before start
            pDist.p[0] /= ppb[i]->pBone->m_pEnvLen.p[0];
         else if (fDistX >= 1) {
            pDist.p[0] -= 1;  // since at end
            pDist.p[0] /= ppb[i]->pBone->m_pEnvLen.p[1];
         }
         else
            pDist.p[0] = 0;

         // y
         DWORD dwSide;
         fp fLen;
         dwSide = (pDist.p[1] <= 0) ? 1 : 3;
         fLen = (1.0 - fDistX) * ppb[i]->pBone->m_pEnvStart.p[dwSide] +
            fDistX * ppb[i]->pBone->m_pEnvEnd.p[dwSide];
         pDist.p[1] /= fLen;

         // z
         dwSide = (pDist.p[2] <= 0) ? 2 : 0;
         fLen = (1.0 - fDistX) * ppb[i]->pBone->m_pEnvStart.p[dwSide] +
            fDistX * ppb[i]->pBone->m_pEnvEnd.p[dwSide];
         pDist.p[2] /= fLen;

         fLen = pDist.Length();
         fLen = 1.0 - fLen;
         // if ((fDistX <= 0) || (fDistX >= 1)) fLen = 1;   // hack for test

         // If not, zero the score and decrease dwNumLeft
         if (fLen <= 0) {
            pbw[i].fWeight = 0;
            dwNumLeft--;
            continue;
         }
         // BUGFIX - Take out and just use fLen - fLen *= fLen;  // square
         //fLen *= fLen;  // 4th power
         //fLen *= fLen;  // 16th power

         // else, determine the score by distance from line, or point
         pbw[i].fWeight = fLen;
      }

      // if the point is not in the main tube region of the evenlope and extending
      // then will do some extra scaling
      // NOTE: If uniform scaling then affect no matter what the location
      if (((pbw[i].pBone.p[0] <= 0) || (pbw[i].pBone.p[0] >= ppb[i]->fPreBoneLen)) &&
         (ppb[i]->pBone->m_pMotionCur.p[3] != ppb[i]->pBone->m_pMotionDef.p[3]) &&
         !m_fBoneAffectUniform) {
            BOOL fLong;
            fLong = FALSE;
            if (pbw[i].pBone.p[0] >= ppb[i]->fPreBoneLen) {
               pbw[i].pBone.p[0] -= ppb[i]->fPreBoneLen;
               fLong = TRUE;
            }

            pbw[i].pBone.p[0] *= (ppb[i]->pBone->m_pMotionDef.p[3] / ppb[i]->pBone->m_pMotionCur.p[3]);

            if (fLong)
               pbw[i].pBone.p[0] += ppb[i]->fPreBoneLen;
         }

      // if twist then make rotation around joint
      if (fTwist) {
         mTwist.RotationX ((ppb[i]->pBone->m_pMotionCur.p[2] - ppb[i]->pBone->m_pMotionDef.p[2]) *
            fDistX);
         pbw[i].pBone.MultiplyLeft (&mTwist);
      }

      // convert the normal
      if (pNormWorld) {
         pbw[i].pNormBone.Copy (pNormWorld);
         pbw[i].pNormBone.p[3] = 1;
         pbw[i].pNormBone.MultiplyLeft (&ppb[i]->mPreNormToBone);

         if (fTwist) {
            // NOTE: Because mTwist is just one rotation around X, the inverse/transpose
            // is the same as mTwise
            pbw[i].pNormBone.MultiplyLeft (&mTwist);
         }
      }
   } // figuring out scores

   // sum up all the scores, and convert back to world space
   fp fSum;
   fSum = 0;
   for (i = 0; i < dwNum; i++) {
      if (!pbw[i].fWeight)
         continue;

      fSum += pbw[i].fWeight;

      // convert back and scale by the weight
      pbw[i].pBone.MultiplyLeft (&ppb[i]->mBoneToPost);
      if (pNormWorld)
         pbw[i].pNormBone.MultiplyLeft (&ppb[i]->mBoneNormToPost);
   }
   if (fSum < CLOSE)
      goto nochange; // not enough score to make a change

   // else, average together
   pPointDeform->Zero();
   if (pNormDeform)
      pNormDeform->Zero();
   for (i = 0; i < dwNum; i++) {
      if (!pbw[i].fWeight)
         continue;
      
      // scale and add
      if (pbw[i].fWeight != fSum)
         pbw[i].pBone.Scale (pbw[i].fWeight / fSum);
      pPointDeform->Add (&pbw[i].pBone);

      if (pNormWorld) {
         pbw[i].pNormBone.Scale (pbw[i].fWeight);  // no point doing /fSum because will normalize
         pNormDeform->Add (&pbw[i].pNormBone);
      }
   }

   // if normal then normalize
   if (pNormDeform)
      pNormDeform->Normalize();

   // done

}


/*********************************************************************************
CObjectBone::ObjEditBoneRender - Does the rendering if the object is affected by bones. This
will cause vertices to be modified so they use remapped normals.

inputs
   PPOLYRENDERINFO   pInfo - Render information. The data in pInfo and what it points
      to may be changed
   PCRenderSocket    pRS - Render socket to call into for polygon render info
   PCMatrix          pmToWorld - Apply this matrix to the incoming points to convert them to world space

   NOTE: Bone information must must be filled with info about bones
*/
void CObjectBone::ObjEditBoneRender (PPOLYRENDERINFO pInfo, PCRenderSocket pRS,
                        PCMatrix pmToWorld)
{
   // create memory for normals, basically as large as what's passed in
   BOOL fNeedNorm;
   fNeedNorm = pRS->QueryWantNormals();
   CListFixed lNorm;
   if (fNeedNorm) {
      // do it this was so start out with a large enough chunk of memory that hopefully
      // wont have to ESCREALLOC, or if do, only really once.
      lNorm.Init (sizeof(CPoint), pInfo->paNormals, pInfo->dwNumNormals);
      lNorm.Clear();
   }

   // create memory for the new points
   CListFixed lPoints;
   PCPoint paPoints;
   lPoints.Init (sizeof(CPoint), pInfo->paPoints, pInfo->dwNumPoints);
   paPoints = (PCPoint) lPoints.Get(0);

   // convert all the points and normals to world space
   DWORD i;
   for (i = 0; i < pInfo->dwNumPoints; i++) {
      pInfo->paPoints[i].p[3] = 1;
      pInfo->paPoints[i].MultiplyLeft (pmToWorld);
   }
   // if need normals make one to convert normals
   CMatrix mToWorldNorm;
   if (fNeedNorm) {
      pmToWorld->Invert (&mToWorldNorm);
      mToWorldNorm.Transpose ();

      for (i = 0; i < pInfo->dwNumNormals; i++) {
         pInfo->paNormals[i].p[3] = 1;
         pInfo->paNormals[i].MultiplyLeft (&mToWorldNorm);
      }
   }


   // loop through all the vertices
   PVERTEX pv;
   CPoint pNorm;
   for (i = 0, pv = pInfo->paVertices; i < pInfo->dwNumVertices; i++, pv++) {
      // convert the points
      ObjEditBonePointConvert (&pInfo->paPoints[pv->dwPoint],
         fNeedNorm ? &pInfo->paNormals[pv->dwNormal] : NULL,
         &paPoints[pv->dwPoint],
         fNeedNorm ? &pNorm : NULL);
      if (fNeedNorm) {
         lNorm.Add (&pNorm);
         pv->dwNormal = lNorm.Num()-1;
      }
   }  // over all vertices

   // substitute
   pInfo->paPoints = paPoints;
   pInfo->paNormals = fNeedNorm ? (PCPoint)lNorm.Get(0) : NULL;
   pInfo->dwNumNormals = fNeedNorm ? lNorm.Num() : 0;

   // pass down
   pRS->PolyRender (pInfo);
}


/*********************************************************************************
CObjectBone::ObjEditBonesClear - Frees up all memory allocated for bones
*/
void CObjectBone::ObjEditBonesClear (void)
{
   if (m_plBoneAffect) {
      delete m_plBoneAffect;
      m_plBoneAffect = NULL;
   }
   if (m_plBoneMove) {
      delete m_plBoneMove;
      m_plBoneMove = NULL;
   }
   if (m_plOEBW) {
      delete m_plOEBW;
      m_plOEBW = NULL;
   }
   if (m_plOERIGIDBONE) {
      delete m_plOERIGIDBONE;
      m_plOERIGIDBONE = NULL;
   }

   if (!m_plOEBONE)
      return;

   // if a list exists, go through it and free up the sub-elems of list
   DWORD i;
   POEBONE pob;
   pob = (POEBONE) m_plOEBONE->Get(0);
   for (i = 0; i < m_plOEBONE->Num(); i++, pob++) {
      if (pob->plChildren)
         delete pob->plChildren;
   }

   // free up list
   delete m_plOEBONE;
   m_plOEBONE = NULL;
}


/*********************************************************************************
CObjectBone::ObjEditBonesTestObject - Test all the bones against a given object. This fills
in plBoneAffect with a list of POEBONE indicating which bones can affect the
object.

inputs
   DWORD             dwObject - Object index.
   BOOL              fAllBones - If TRUE then all the bones are used (basically ignoring
                        dwObject). If FALSE then dwObject is intersected with the bones
                        to see which ones affect.
*/
void CObjectBone::ObjEditBonesTestObject (DWORD dwObject, BOOL fAllBones)
{
   m_fBoneAffectUniform = FALSE;
   if (m_plBoneAffect)
      m_plBoneAffect->Clear();
   if (m_plOEBW)
      m_plOEBW->Clear();

   if (!m_plOEBONE)
      return;

   if (fAllBones)
      dwObject = -1;
   else {
      if (dwObject == -1)
         return;  // cant do anything since doesnt exist
   }

   if (!m_plBoneAffect) {
      m_plBoneAffect = new CListFixed;
      if (!m_plBoneAffect)
         return;
      m_plBoneAffect->Init (sizeof(POEBONE));
   }
   if (!m_plOEBW) {
      m_plOEBW = new CListFixed;
      if (!m_plOEBW)
         return;
      m_plOEBW->Init (sizeof(OEBW));
   }

   // get the bounding box of the object
   CPoint apBound[2];
   if (dwObject != -1) {
      CMatrix m;
      CPoint pCorner[2], p;
      DWORD x,y,z;
      // NOTE: Changed from pwn->pWorld to m_pWorld
      if (!m_pWorld->BoundingBoxGet (dwObject, &m, &pCorner[0], &pCorner[1]))
         return;  // error
      for (x = 0; x < 2; x++) for (y = 0; y < 2; y++) for (z = 0; z < 2; z++) {
         p.p[0] = pCorner[x].p[0];
         p.p[1] = pCorner[y].p[1];
         p.p[2] = pCorner[z].p[2];
         p.p[3] = 1;
         p.MultiplyLeft (&m);

         if (!x && !y && !z) {
            apBound[0].Copy (&p);
            apBound[1].Copy (&p);
         }
         else {
            apBound[0].Min (&p);
            apBound[1].Max (&p);
         }
      }
   } // dwObject != -1

   // get the object GUID
   PCObjectSocket pos;
   // NOTE: Changed from pwn->pWorld to m_pWorld
   GUID gGUID;
   if (dwObject != -1) {
      pos = m_pWorld->ObjectGet (dwObject);
      if (!pos)
         return;
      pos->GUIDGet (&gGUID);

      // if it's the bones object then no deformation
      if (IsEqualGUID (gGUID, m_gGUID))
         return;
   }
   else {
      pos = NULL;
      memset (&gGUID, 0, sizeof(gGUID));
   }

   // loop through all the bones
   DWORD i, j;
   POEBONE pob;
   DWORD dwNum, dwNumG;
   GUID *pg;
   OEBW fWeight;
   pob = (POEBONE) m_plOEBONE->Get(0);
   dwNum = m_plOEBONE->Num();
   for (i = 0; i < dwNum; i++, pob++) {
      // if doesn't deform then ignore
      if (!pob->fDeform)
         continue;

      // if always affects this object then just add
      pg = (GUID*) pob->pBone->m_lObjRigid.Get(0);
      dwNumG = pob->pBone->m_lObjRigid.Num();
      for (j = 0; j < dwNumG; j++, pg++)
         if (IsEqualGUID (gGUID, *pg))
            break;
      if (j < dwNumG) {
         m_plBoneAffect->Clear();
         m_plOEBW->Clear();
         m_plBoneAffect->Add (&pob);
         m_plOEBW->Add (&fWeight);
         m_fBoneAffectUniform = TRUE;
         return;  // returning since if bone affects as rigid, then only one bone can affect
      }

      // if always ignore this then continue
      pg = (GUID*) pob->pBone->m_lObjIgnore.Get(0);
      dwNumG = pob->pBone->m_lObjIgnore.Num();
      for (j = 0; j < dwNumG; j++, pg++)
         if (IsEqualGUID (gGUID, *pg))
            break;
      if (j < dwNumG)
         continue;

      // else, make sure intersects bounding box
      if (dwObject != -1) {
         for (j = 0; j < 3; j++)
            if ((apBound[0].p[j] >= pob->apBound[1].p[j]) ||(apBound[1].p[j] <= pob->apBound[0].p[j]))
               break;
         if (j < 3)
            continue;   // no found
      }

      // else, probably intersects, so use
      m_plBoneAffect->Add (&pob);
      m_plOEBW->Add (&fWeight);
   }
}

/*********************************************************************************
CObjectBone::ObjEditBonesCalcPreBend - Calculates all the matricies for all the bones that
convert from inside the OE world's space, pre-bend state. These matricies allow
a point (pre-bend) to be checked against a bounding box and see if it will be
bent. The also convert the point (in internal world space) into the bone's space,
where x=0..N goes along the length of the bone, y=l/r, z=u/d.

*/
void CObjectBone::ObjEditBonesCalcPreBend (void)
{
   if (!m_plOEBONE)
      return;

   // get the conversion of the bone object to world space
   CMatrix mObjectBoneToWorld;
   ObjectMatrixGet (&mObjectBoneToWorld);

   POEBONE pob;
   DWORD dwNum, i;
   pob = (POEBONE) m_plOEBONE->Get(0);
   dwNum = m_plOEBONE->Num();
   for (i = 0; i < dwNum; i++, pob++) {
      pob->fDeform = pob->pBone->m_fUseEnvelope;
      if (!pob->fDeform)
         continue;

      // length
      CPoint pLen;
      pLen.Subtract (&pob->pBone->m_pEndOF, &pob->pBone->m_pStartOF);
      pob->fPreBoneLen = pLen.Length();
      pob->fPreBoneLen = max(pob->fPreBoneLen, CLOSE);   // always some length

      // since know there's no rotation, just a translation
      pob->mBoneToPre.Translation (pob->pBone->m_pStartOF.p[0], 
         pob->pBone->m_pStartOF.p[1], pob->pBone->m_pStartOF.p[2]);
      pob->mBoneToPre.MultiplyLeft (&pob->pBone->m_mXToEnd);   // in case bone rotate
      // if have the flip then include scale
      if (pob->pBone->m_fFlipLR) {
         CMatrix mScale;
         mScale.Scale (1, -1, 1);
         pob->mBoneToPre.MultiplyLeft (&mScale);
      }
      pob->mBoneToPre.MultiplyRight (&mObjectBoneToWorld);

      // normal translation - NOTE: includes the scaling flip, which will need to be undone later
      pob->mPreNormToBone.Copy (&pob->mBoneToPre);
      pob->mPreNormToBone.Transpose();

      // going from pre-bend space into bone space is opposite matrix
      pob->mBoneToPre.Invert4 (&pob->mPreToBone);

      // bounding box
      CPoint p;
      DWORD x,y,z;
      for (x = 0; x < 2; x++) for (y = 0; y < 2; y++) for (z = 0; z < 2; z++) {
         PCPoint pEnd;
         pEnd = x ? &pob->pBone->m_pEnvEnd : &pob->pBone->m_pEnvStart;
         p.Zero();
         p.p[0] = x ? (1 + pob->pBone->m_pEnvLen.p[1]) : (-pob->pBone->m_pEnvLen.p[0]);
         p.p[1] = y ? pEnd->p[3] : -pEnd->p[1];
         p.p[2] = z ? pEnd->p[0] : -pEnd->p[2];
         p.Scale (pob->fPreBoneLen);

         // transform into world sace
         p.p[3] = 1;
         p.MultiplyLeft (&pob->mBoneToPre);

         // min and max
         if (!x && !y && !z) {
            pob->apBound[0].Copy (&p);
            pob->apBound[1].Copy (&p);
         }
         else {
            pob->apBound[0].Min (&p);
            pob->apBound[1].Max (&p);
         }
      } // bounding box edges
   } // over all bones
}


/*********************************************************************************
CObjectBone::ObjEditBonesCalcPostBend - Calculates all the matricies for all the bones that
convert from inside the OE world's space, post-bend state. After a point has been
transformed from pre-bend to bone coords, checked against being bounded, and then
rotated (which none of the matricies do), these matricies converts it back into world space.
*/
void CObjectBone::ObjEditBonesCalcPostBend (void)
{
   if (/*!pwc->pObjectBone ||*/ !m_plOEBONE)
      return;

   // get the conversion of the bone object to world space
   CMatrix mObjectBoneToWorld;
   ObjectMatrixGet (&mObjectBoneToWorld);

   POEBONE pob;
   DWORD dwNum, i;
   pob = (POEBONE) m_plOEBONE->Get(0);
   dwNum = m_plOEBONE->Num();
   for (i = 0; i < dwNum; i++, pob++) {
      if (!pob->fDeform)
         continue;

      // length
      CPoint pLen;
      fp fLen;
      pLen.Subtract (&pob->pBone->m_pEndOS, &pob->pBone->m_pStartOS);
      fLen = pLen.Length();

      // flip LR and scale do stretch
      pob->mBoneToPost.Scale (fLen / pob->fPreBoneLen, pob->pBone->m_fFlipLR ? -1 : 1, 1);

      // NOTE: Would rather get this all from COEBone::MatrixSet(), but because situation
      // is not exactly the same cant do that way

      // side to side
      CMatrix m2;
      if (fabs(pob->pBone->m_pMotionCur.p[1] - pob->pBone->m_pMotionDef.p[1]) > CLOSE) {
         m2.RotationZ ((pob->pBone->m_fFlipLR ? -1 : 1) *(pob->pBone->m_pMotionCur.p[1] - pob->pBone->m_pMotionDef.p[1]));
         pob->mBoneToPost.MultiplyRight (&m2);
      }

      // bend
      if (fabs(pob->pBone->m_pMotionCur.p[0] - pob->pBone->m_pMotionDef.p[0]) > CLOSE) {
         m2.RotationY (pob->pBone->m_pMotionCur.p[0] - pob->pBone->m_pMotionDef.p[0]);
         pob->mBoneToPost.MultiplyRight (&m2);
      }

      pob->mBoneToPost.MultiplyRight (&pob->pBone->m_mXToEnd);   // in case bone rotate

      // translation
      //CMatrix mTrans;
      //mTrans.Translation (pob->pBone->m_pStartOS.p[0],
      //   pob->pBone->m_pStartOS.p[1], pob->pBone->m_pStartOS.p[2]);
      //pob->mBoneToPost.MultiplyRight (&mTrans);
      pob->mBoneToPost.MultiplyRight (&pob->pBone->m_mCalcOS);

      // convert and to world sapce
      pob->mBoneToPost.MultiplyRight (&mObjectBoneToWorld);

      // create normal conversion
      pob->mBoneToPost.Invert (&pob->mBoneNormToPost);
      pob->mBoneNormToPost.Transpose ();
   } // over all bones
}

/*********************************************************************************
CObjectBone::ObjEditBonesRigidCP - Given a PCObjectSocket, this sees if it's one of the objects
that is marked as having rigid bones. If the bones are rigid this fills in mBoneRot
with rotation and translation that would affect a rigid object.

inputs
   PCObjectSocket       pos - Object to check
   PCMatrix             pmBoneRot - If the object is affected, returns a matrix that
                        converts from pre-bone coords to bone-rot coords.
returns
   BOOL - TRUE if pmBoneRot is filled. Else, no rotations exist
*/
BOOL CObjectBone::ObjEditBonesRigidCP (PCObjectSocket pos, PCMatrix pmBoneRot)
{
   // if empty list ignore
   if (!m_plOERIGIDBONE || !m_plOERIGIDBONE->Num())
      return FALSE;

   // get the GUID
   GUID gObject;
   pos->GUIDGet (&gObject);

   // see if it's in the list
   POERIGIDBONE prb;
   prb = (POERIGIDBONE) m_plOERIGIDBONE->Get(0);
   DWORD i;
   for (i = 0; i < m_plOERIGIDBONE->Num(); i++, prb++) {
      if (IsEqualGUID(gObject, prb->gObject))
         break;
   }
   if (i >= m_plOERIGIDBONE->Num())
      return FALSE;

   // if got here found a bone that affects this
   CMatrix mPreBoneToWorld, mWorldToPreBone, mPostBoneToWorld;

   // Copied part of code to InternAttachMatrix()

   // pre-bone
   CMatrix mTrans;
   mTrans.Translation (prb->pBone->m_pEnd.Length()/2, 0, 0);   // go half way up, as is standard with rigid bodies
   mPreBoneToWorld.Multiply (&prb->pBone->m_mXToEnd, &mTrans);
   mTrans.Translation (prb->pBone->m_pStartOF.p[0], prb->pBone->m_pStartOF.p[1], prb->pBone->m_pStartOF.p[2]);
   mPreBoneToWorld.MultiplyRight (&mTrans);
   mPreBoneToWorld.Invert4 (&mWorldToPreBone);

   // post bone
   CPoint pLen;
   pLen.Subtract (&prb->pBone->m_pEndOS, &prb->pBone->m_pStartOS);
   mTrans.Identity();
   if (fabs( prb->pBone->m_pMotionDef.p[3]) > EPSILON)
      mTrans.Translation (pLen.Length() * prb->pBone->m_pMotionCur.p[3] / prb->pBone->m_pMotionDef.p[3]/2, 0, 0);  // go half way out as is standard
   // undo half the twist that's stored in m_mRender
   if (fabs(prb->pBone->m_pMotionCur.p[2] - prb->pBone->m_pMotionDef.p[2]) > CLOSE) {
      CMatrix mRot;
      mRot.RotationX (-0.5 * (prb->pBone->m_fFlipLR ? -1 : 1) * (prb->pBone->m_pMotionCur.p[2] - prb->pBone->m_pMotionDef.p[2]));
      mTrans.MultiplyRight (&mRot);
   }
   mPostBoneToWorld.Multiply (&prb->pBone->m_mRender, &mTrans);
   mPostBoneToWorld.MultiplyRight (&prb->pBone->m_mCalcOS); // so gets to starting point for bone

   // combine the two
   pmBoneRot->Multiply (&mPostBoneToWorld, &mWorldToPreBone);

   // BUGFIX - Need to pre-convert from world space, to bone space
   CMatrix mInv;
   m_MatrixObject.Invert4 (&mInv);
   pmBoneRot->MultiplyLeft (&mInv);
   pmBoneRot->MultiplyRight (&m_MatrixObject);

   return TRUE;
}


/* PCBoneCompare - Used to sort bones so lowest Z is drawn first, for movement */
CRITICAL_SECTION gcsBoneCompare;
static PCMatrix gpmBoneCompare = NULL;
static int _cdecl PCBoneCompare (const void *elem1, const void *elem2)
{
   PCBone *pdw1, *pdw2;
   pdw1 = (PCBone*) elem1;
   pdw2 = (PCBone*) elem2;

   // convert the points into post-object space so have right direction for up
   CPoint p1, p2;
   p1.Copy (&pdw1[0]->m_pEndOF);
   p2.Copy (&pdw2[0]->m_pEndOF);
   p1.p[3] = p2.p[3] = 1;
   p1.MultiplyLeft (gpmBoneCompare);
   p2.MultiplyLeft (gpmBoneCompare);

   fp fDelta;
   fDelta = p1.p[2] - p2.p[2];
   if (fabs(fDelta) > CLOSE)
      return (fDelta > 0) ? 1 : -1;

   return _wcsicmp(pdw1[0]->m_szName, pdw2[0]->m_szName);
}


/*********************************************************************************
CObjectBone::ObjEditBoneSetup - When this object is going to be used for bone
manipulation of points, this will set up the object.

inputs
   BOOL              fForce - If TRUE then always setup the bones, even if already
                     have been set up. If FALSE then don't bother seting up
                     if there's already data.
   PCMatrix          pmExtraRot - Extra rotation of object. Used by the object editor
                     to add extra rotation for embedded objects. If no extra rotation
                     can be NULL
returns
   BOOL - TRUE if success
*/
BOOL CObjectBone::ObjEditBoneSetup (BOOL fForce, PCMatrix pmExtraRot)
{
   // if already created then return
   if (!fForce && m_plOEBONE)
      return TRUE;

   // call function to free up existing bone info
   ObjEditBonesClear ();

   if (pmExtraRot)
      m_mBoneExtraRot.Copy (pmExtraRot);

   // create a new object to contain the bone list
   m_plOEBONE = new CListFixed;
   if (!m_plOEBONE)
      return FALSE;
   m_plOEBONE->Init (sizeof(OEBONE));

   // Create mirror structures
   PCBone *ppb;
   OEBONE   ob;
   DWORD j, k, dwNum;
   ppb = (PCBone*) m_lBoneList.Get(0);
   dwNum = m_lBoneList.Num();
   GUID *pg;
   OERIGIDBONE orb;
   if (m_plOERIGIDBONE)
      m_plOERIGIDBONE->Clear();
   DWORD i;
   m_plOEBONE->Required (dwNum);
   for (i = 0; i < dwNum; i++) {
      memset (&ob, 0, sizeof(ob));
      ob.pBone = ppb[i];

      // allocate memory for children
      ob.plChildren = new CListFixed;
      if (ob.plChildren)
         ob.plChildren->Init (sizeof(POEBONE));

      m_plOEBONE->Add (&ob);

      // if bone has any effect in rigid way then remember this
      pg = (GUID*) ob.pBone->m_lObjRigid.Get(0);
      for (j = 0; j < ob.pBone->m_lObjRigid.Num(); j++, pg++) {
         orb.gObject = *pg;
         orb.pBone = ob.pBone;
         if (!m_plOERIGIDBONE) {
            m_plOERIGIDBONE = new CListFixed;
            if (m_plOERIGIDBONE)
               m_plOERIGIDBONE->Init (sizeof (OERIGIDBONE));
         }
         if (m_plOERIGIDBONE)
            m_plOERIGIDBONE->Add (&orb);
      }
   }

   // set up for bone list
   if (!m_plBoneMove)
      m_plBoneMove = new CListFixed;
   if (m_plBoneMove) {
      m_plBoneMove->Init (sizeof(PCBone), ppb, dwNum);

      EnterCriticalSection (&gcsBoneCompare);
      CMatrix m;
      ObjectMatrixGet (&m);
      m.MultiplyRight (&m_mBoneExtraRot);   // incase orientation different
      gpmBoneCompare = &m;

      qsort (m_plBoneMove->Get(0), m_plBoneMove->Num(), sizeof(PCBone), PCBoneCompare);
      LeaveCriticalSection (&gcsBoneCompare);
   }

   // now that all bones have been added, fill in children and parents
   POEBONE pob;
   pob = (POEBONE) m_plOEBONE->Get(0);
   for (i = 0; i < dwNum; i++) {
      if (!pob[i].plChildren)
         continue;
      PCBone *pChild;
      pChild = (PCBone*) pob[i].pBone->m_lPCBone.Get(0);
      for (j = 0; j < pob[i].pBone->m_lPCBone.Num(); j++) {
         for (k = 0; k < dwNum; k++)
            if (pChild[j] == pob[k].pBone)
               break;
         if (k < dwNum) {
            POEBONE p = pob + k;
            pob[i].plChildren->Add (&p);

            // also, note that this is a child
            pob[k].pParent = &pob[i];
         }
      } // j - all chidren
   } // i - all bones

   // create a list of all the bones
   // calculate them pre-bend
   ObjEditBonesCalcPreBend ();

   // just calculaute post-bend to have something
   ObjEditBonesCalcPostBend ();

   return TRUE;
}

/**********************************************************************************
CObjectBone::ObjectMatrixSet - Capture so can change bone calculations
*/
BOOL CObjectBone::ObjectMatrixSet (CMatrix *pObject)
{
   if (!CObjectTemplate::ObjectMatrixSet (pObject))
      return FALSE;

   // if move object then clear info about bones since will need to recalc
   ObjEditBonesClear ();

   return TRUE;
}

/*****************************************************************************************
CObjectBone::Deconstruct - Standard call
*/
BOOL CObjectBone::Deconstruct (BOOL fAct)
{
   return FALSE;
}


/*****************************************************************************************
CObjectBone::EditorCreate - From CObjectSocket. SOMETIMES OVERRIDDEN
*/
BOOL CObjectBone::EditorCreate (BOOL fAct)
{
   if (!fAct)
      return TRUE;

   return ObjectViewNew (m_pWorld, &m_gGUID, VIEWWHAT_BONE);
}


/*****************************************************************************************
CObjectBone::EditorCreate - From CObjectSocket. SOMETIMES OVERRIDDEN
*/
BOOL CObjectBone::EditorDestroy (void)
{
   return ObjectViewDestroy (m_pWorld, &m_gGUID);
}

/*****************************************************************************************
CObjectBone::EditorCreate - From CObjectSocket. SOMETIMES OVERRIDDEN
*/
BOOL CObjectBone::EditorShowWindow (BOOL fShow)
{
   return ObjectViewShowHide (m_pWorld, &m_gGUID, fShow);
}

/*****************************************************************************************
CObjectBone::CurBoneGet - Returns the index number for the current bone.
This is also the same as the bone's minor ID in the pixel.

returns
   DWORD - current bone, or -1 if no current bone
*/
DWORD CObjectBone::CurBoneGet (void)
{
   if (m_dwCurBone >= m_lBoneList.Num())
      return -1;

   return m_dwCurBone;
}

/*****************************************************************************************
CObjectBone::CurBoneSet - Sets the index for the current bone.

inputs
   DWORD - New bone number, -1 for no current bone
*/
void CObjectBone::CurBoneSet (DWORD dwBone)
{
   if (dwBone >= m_lBoneList.Num())
      dwBone = -1;
   if (dwBone == m_dwCurBone)
      return;

   if (m_pWorld)
      m_pWorld->ObjectAboutToChange (this);
   m_dwCurBone = dwBone;
   if (m_pWorld)
      m_pWorld->ObjectChanged (this);
}


/*****************************************************************************************
CObjectBone::SymmetryGet - Returns the current symmetry state. Bit 0 = x, 1 =y, 2=z
*/
DWORD CObjectBone::SymmetryGet (void)
{
   return m_dwSymmetry;
}

/*****************************************************************************************
CObjectBone::SymmetrySet - Sets the current symmetry state

inputs
   DWORD             dwSymmetry - 0 bit =x, 1bit=y, 2bit=z
returns
   none
*/
void CObjectBone::SymmetrySet (DWORD dwSymmetry)
{
   if (dwSymmetry == m_dwSymmetry)
      return;


   if (m_pWorld)
      m_pWorld->ObjectAboutToChange (this);
   m_dwSymmetry = dwSymmetry;
   if (m_pWorld)
      m_pWorld->ObjectChanged (this);

}

/*****************************************************************************************
CObjectBone::DefaultPosition - Resets the bone to its default position.
*/
void CObjectBone::DefaultPosition (void)
{
   if (m_pWorld)
      m_pWorld->ObjectAboutToChange (this);
   PCBone *ppb;
   DWORD i, j;
   ppb = (PCBone*) m_lBoneList.Get(0);
   for (i = 0; i < m_lBoneList.Num(); i++) {
      // make sure max is more than min, just to be sure
      // make sure the def is within range, just to be sure
      for (j = 0; j < 4; j++) {
         ppb[i]->m_pMotionMax.p[j] = max(ppb[i]->m_pMotionMax.p[j], ppb[i]->m_pMotionMin.p[j]);
         ppb[i]->m_pMotionDef.p[j] = max(ppb[i]->m_pMotionDef.p[j], ppb[i]->m_pMotionMin.p[j]);
         ppb[i]->m_pMotionDef.p[j] = min(ppb[i]->m_pMotionDef.p[j], ppb[i]->m_pMotionMax.p[j]);
         ppb[i]->m_pMotionCur.p[j] = ppb[i]->m_pMotionDef.p[j];
      }

      ppb[i]->CalcMatrix();
   }

   CalcObjectSpace();
   ResetIKWant();
   ObjEditBonesClear();

   if (m_pWorld)
      m_pWorld->ObjectChanged (this);
}


/*****************************************************************************************
CObjectBone::SplitBone - Splits the given bone number. This also splits the mirror.

inputs
   DWORD          dwBone - Bone index to split.
returns
   BOOL - TRUE if acutally split
*/
BOOL CObjectBone::SplitBone (DWORD dwBone)
{
   // find symmetrical bones
   CListFixed lSymPCBone, lSymDWORD;
   lSymPCBone.Init (sizeof(PCBone));
   lSymDWORD.Init (sizeof(DWORD));
   PCBone pRoot;
   DWORD dwNum;
   PCBone *ppb;
   ppb = (PCBone*) m_lBoneList.Get(0);
   dwNum = m_lBoneList.Num();
   if (dwBone >= dwNum)
      return FALSE; // error
   pRoot = ppb[dwBone];
   FindMirrors (pRoot, &lSymPCBone, &lSymDWORD);

   if (m_pWorld)
      m_pWorld->ObjectAboutToChange (this);

   // split it
   DWORD dwDel;
   ppb = (PCBone*) lSymPCBone.Get(0);
   for (dwDel = 0; dwDel <= lSymPCBone.Num(); dwDel++) {
      PCBone pDel;
      pDel = (dwDel < lSymPCBone.Num()) ? ppb[dwDel] : pRoot;

      // make a unique name
      WCHAR szTemp[64];
      CBone bTemp;
      wcscpy (szTemp, pDel->m_szName);
      szTemp[55] = 0;   // so can append split
      wcscat (szTemp, L" split");
      wcscpy (bTemp.m_szName, szTemp);
      MakeNameUnique (&bTemp);

      // split it
      pDel->Split (bTemp.m_szName);
   }  // dwDel

   // update calculations
   CalcObjectSpace ();
   ResetIKWant();
   CalcAttribList();

   // reset bone info
   ObjEditBonesClear ();

   if (m_pWorld)
      m_pWorld->ObjectChanged (this);

   return TRUE;
}


/*****************************************************************************************
CObjectBone::DeleteBone - Deletes the given bone number. This also splits the mirror.

inputs
   DWORD          dwBone - Bone index to split.
returns
   BOOL - TRUE if acutally split
*/
BOOL CObjectBone::DeleteBone (DWORD dwBone)
{
   // find symmetrical bones
   CListFixed lSymPCBone, lSymDWORD;
   lSymPCBone.Init (sizeof(PCBone));
   lSymDWORD.Init (sizeof(DWORD));
   PCBone pRoot;
   DWORD dwNum;
   PCBone *ppb;
   ppb = (PCBone*) m_lBoneList.Get(0);
   dwNum = m_lBoneList.Num();
   if (dwBone >= dwNum)
      return FALSE; // error
   pRoot = ppb[dwBone];
   FindMirrors (pRoot, &lSymPCBone, &lSymDWORD);

   if (m_pWorld)
      m_pWorld->ObjectAboutToChange (this);

   // delete it
   DWORD dwDel;
   BOOL fRet = TRUE;
   ppb = (PCBone*) lSymPCBone.Get(0);
   for (dwDel = 0; dwDel <= lSymPCBone.Num(); dwDel++) {
      PCBone pDel;
      pDel = (dwDel < lSymPCBone.Num()) ? ppb[dwDel] : pRoot;

      // loop through all the root bones
      DWORD i;
      PCBone *ppbl;
      ppbl = (PCBone*) m_lPCBone.Get(0);
      for (i = 0; i < m_lPCBone.Num(); i++) {
         if (ppbl[i] == pDel) {
            // if last one dont delete it
            if (m_lPCBone.Num() == 1) {
               fRet = FALSE;
               break;
            }

            // else, remove it
            delete ppbl[i];
            m_lPCBone.Remove (i);
            break;
         }

         // delete child
         ppbl[i]->DeleteChild (pDel);
      }
   }  // dwDel

   // update calculations
   CalcObjectSpace ();
   ResetIKWant();
   CalcAttribList();

   // reset bone info
   ObjEditBonesClear ();

   if (m_pWorld)
      m_pWorld->ObjectChanged (this);
   return fRet;
}



/*****************************************************************************************
CObjectBone::DisconnectBone - Disconnects the given bone number. This also
disconnects the mirrors.

inputs
   DWORD          dwBone - Bone index to disconnect.
returns
   BOOL - TRUE if acutally split
*/
BOOL CObjectBone::DisconnectBone (DWORD dwBone)
{
   // find symmetrical bones
   CListFixed lSymPCBone, lSymDWORD;
   lSymPCBone.Init (sizeof(PCBone));
   lSymDWORD.Init (sizeof(DWORD));
   PCBone pRoot;
   DWORD dwNum;
   PCBone *ppb;
   ppb = (PCBone*) m_lBoneList.Get(0);
   dwNum = m_lBoneList.Num();
   if (dwBone >= dwNum)
      return FALSE; // error
   pRoot = ppb[dwBone];
   FindMirrors (pRoot, &lSymPCBone, &lSymDWORD);


   // list of objects to add
   CListFixed lAdd;
   lAdd.Init (sizeof(PCObjectSocket));

   // delete it
   DWORD dwDel, i;
   BOOL fRet = TRUE;
   DWORD dwWouldDisconnect = 0;
   ppb = (PCBone*) lSymPCBone.Get(0);
   for (dwDel = 0; dwDel <= lSymPCBone.Num(); dwDel++) {
      PCBone pDel;
      pDel = (dwDel < lSymPCBone.Num()) ? ppb[dwDel] : pRoot;

      // keep track if would disconnect all the root bones
      PCBone *ppbl;
      ppbl = (PCBone*) m_lPCBone.Get(0);
      for (i = 0; i < m_lPCBone.Num(); i++)
         if (ppbl[i] == pDel)
            dwWouldDisconnect++;

      // figure out the location
      CMatrix mLoc;
      mLoc.Translation (pDel->m_pStartOF.p[0], pDel->m_pStartOF.p[1], pDel->m_pStartOF.p[2]);
      mLoc.MultiplyRight (&m_MatrixObject);

      // make new object
      OSINFO Info;
      InfoGet (&Info);
      PCObjectBone pNew = new CObjectBone (0, &Info);
      if (!pNew)
         continue;
      pNew->m_dwSymmetry = 0; // no symmetry
      pNew->ObjectMatrixSet (&mLoc);

      // delete old bones
      PCBone *ppbDel;
      ppbDel = (PCBone*) pNew->m_lPCBone.Get(0);
      for (i = 0; i < pNew->m_lPCBone.Num(); i++)
         delete ppbDel[i];
      pNew->m_lPCBone.Clear();

      // transfer new bone over
      PCBone pNewBone = new CBone;
      if (!pNewBone || !pDel->CloneTo(pNewBone)) {
         if (pNewBone)
            delete pNewBone;
         pNew->Delete();
         continue;
      }
      pNew->m_lPCBone.Add (&pNewBone);

      pNew->ObjEditBonesClear();
      pNew->CalcObjectSpace ();
      pNew->CalcAttribList ();
      pNew->ResetIKWant();

      lAdd.Add (&pNew);
   }  // dwDel

   // if would disconnect all the root bones cant do this
   if (dwWouldDisconnect >= m_lPCBone.Num())
      fRet = FALSE;

   // delete all objects if fail
   PCObjectSocket *ppos = (PCObjectSocket*) lAdd.Get(0);
   if (!fRet) {
      for (i = 0; i < lAdd.Num(); i++)
         ppos[i]->Delete ();
   }
   else {
      for (i = 0; i < lAdd.Num(); i++)
         m_pWorld->ObjectAdd (ppos[i]);

      DeleteBone (dwBone);
   }

   return fRet;
}



/*****************************************************************************************
CObjectBone::ScaleBone - Scale the given bone number. This also scales its children
and mirrors.

inputs
   DWORD          dwBone - Bone index to scale. If -1 then scale everything
   PCPoint        pScale - Amount to scale by in xyz
returns
   BOOL - TRUE if acutally split
*/
BOOL CObjectBone::ScaleBone (DWORD dwBone, PCPoint pScale)
{
   // find symmetrical bones
   CListFixed lSymPCBone, lSymDWORD;
   lSymPCBone.Init (sizeof(PCBone));
   lSymDWORD.Init (sizeof(DWORD));
   PCBone pRoot = NULL;
   DWORD dwNum;
   PCBone *ppb;
   ppb = (PCBone*) m_lBoneList.Get(0);
   dwNum = m_lBoneList.Num();
   if (dwBone < dwNum) {
      pRoot = ppb[dwBone];
      FindMirrors (pRoot, &lSymPCBone, &lSymDWORD);
   }

   if (m_pWorld)
      m_pWorld->ObjectAboutToChange (this);

   // delete it
   DWORD dwDel;
   BOOL fRet = TRUE;
   ppb = (PCBone*) lSymPCBone.Get(0);
   if (pRoot) {
      for (dwDel = 0; dwDel <= lSymPCBone.Num(); dwDel++) {
         PCBone pDel;
         pDel = (dwDel < lSymPCBone.Num()) ? ppb[dwDel] : pRoot;

         pDel->Scale (pScale);
      }  // dwDel
   }
   else {
      ppb = (PCBone*) m_lPCBone.Get(0);
      for (dwDel = 0; dwDel < m_lPCBone.Num(); dwDel++)
         ppb[dwDel]->Scale (pScale);
   }

   // update calculations
   CalcObjectSpace ();
   ResetIKWant();
   CalcAttribList();

   // reset bone info
   ObjEditBonesClear ();

   if (m_pWorld)
      m_pWorld->ObjectChanged (this);
   return fRet;
}



/*****************************************************************************************
CObjectBone::RotateBone - Rtotate the given bone number. This also rotates mirrors.

inputs
   DWORD          dwBone - Bone index to scale. If -1 then scale everything
   fp             fRot - Amount to rotate
returns
   BOOL - TRUE if acutally split
*/
BOOL CObjectBone::RotateBone (DWORD dwBone, fp fRot)
{
   // find symmetrical bones
   CListFixed lSymPCBone, lSymDWORD;
   lSymPCBone.Init (sizeof(PCBone));
   lSymDWORD.Init (sizeof(DWORD));
   PCBone pRoot = NULL;
   DWORD dwNum;
   PCBone *ppb;
   ppb = (PCBone*) m_lBoneList.Get(0);
   dwNum = m_lBoneList.Num();
   if (dwBone >= dwNum)
      return FALSE;
   pRoot = ppb[dwBone];
   FindMirrors (pRoot, &lSymPCBone, &lSymDWORD);

   // create a matrix to rotate
   CMatrix m;
   CPoint pA, pB, pC;
   pA.Subtract (&pRoot->m_pEndOF, &pRoot->m_pStartOF);
   pA.Normalize();
   if (pA.Length() < .5)
      return FALSE;  // error
   pC.Copy (&pRoot->m_pUpOF);
   pB.CrossProd (&pC, &pA);
   if (pB.Length() < .01) {
      pC.Zero();
      pC.p[0] = 1;
      pB.CrossProd (&pC, &pA);
      if (pB.Length() < .01) {
         pC.Zero();
         pC.p[1] = 1;
         pB.CrossProd (&pC, &pA);   // must be ok
      }
   }
   pB.Normalize();
   pC.CrossProd (&pA, &pB);
   pC.Normalize();   // just in case
   m.RotationFromVectors (&pA, &pB, &pC);

   // make a new vector
   CPoint pNew;
   pNew.Zero();
   pNew.p[1] = sin(fRot);
   pNew.p[2] = cos(fRot);
   pNew.MultiplyLeft (&m);

   if (m_pWorld)
      m_pWorld->ObjectAboutToChange (this);

   // copy over
   pRoot->m_pUp.Copy (&pNew);
   pRoot->CalcMatrix();

   // mirrors
   DWORD dwDel;
   DWORD *padw = (DWORD*) lSymDWORD.Get(0);
   ppb = (PCBone*) lSymPCBone.Get(0);
   for (dwDel = 0; dwDel < lSymPCBone.Num(); dwDel++) {
      PCBone pDel;
      pDel = ppb[dwDel];

      pDel->m_pUp.Copy (&pNew);
      if (padw[dwDel] & 0x01)
         pDel->m_pUp.p[0] *= -1;
      if (padw[dwDel] & 0x02)
         pDel->m_pUp.p[1] *= -1;
      if (padw[dwDel] & 0x04)
         pDel->m_pUp.p[2] *= -1;
      pDel->CalcMatrix();
   }  // dwDel

   // update calculations
   CalcObjectSpace ();
   ResetIKWant();
   CalcAttribList();

   // reset bone info
   ObjEditBonesClear ();

   if (m_pWorld)
      m_pWorld->ObjectChanged (this);
   return TRUE;
}


/*****************************************************************************************
CObjectBone::AddBone - Adds a new bone (and symmetrical bones).

inputs
   DWORD       dwAddTo - Bone index to add to, or -1 if add to root
   PCPoint     pDir - Bone length and direction
returns
   BOOL - TRUE if success
*/
BOOL CObjectBone::AddBone (DWORD dwAddTo, PCPoint pDir)
{
   // get the parent
   PCBone *ppb = (PCBone*) m_lBoneList.Get(0);
   DWORD dwNum = m_lBoneList.Num();
   PCBone pBone = (dwAddTo < dwNum) ? ppb[dwAddTo] : NULL;

   // if the direction is nearly straight then make it that way
   DWORD i;
   fp fMax = max(max(fabs(pDir->p[0]), fabs(pDir->p[1])), fabs(pDir->p[2])) / 100;
   for (i = 0; i < 3; i++)
      if (fabs(pDir->p[i]) < fMax)
         pDir->p[i] = 0;

   // figure out the up vector from the bone
   CPoint pA, pUp, pB;
   if (pBone)
      pUp.Copy (&pBone->m_pUp);
   else {
      pUp.Zero();
      pUp.p[1] = -1;
   }
   pA.Copy (pDir);
   pA.Normalize();
   if (pA.Length() < .5)
      return FALSE;
   pB.CrossProd (&pUp, &pA);
   if (pB.Length() < .01) {
      pUp.Zero();
      pUp.p[0] = 1;
      pB.CrossProd (&pUp, &pA);
      if (pB.Length() < .01) {
         pUp.Zero();
         pUp.p[1] = 1;
      }
   }
   pUp.CrossProd (&pA, &pB);
   pUp.Normalize();

   // find the mirrors
   CListFixed lSymPCBone, lSymDWORD;
   lSymPCBone.Init (sizeof(PCBone));
   lSymDWORD.Init (sizeof(DWORD));
   if (pBone)
      FindMirrors (pBone, &lSymPCBone, &lSymDWORD);

   if (m_pWorld)
      m_pWorld->ObjectAboutToChange (this);

   // create
   DWORD j;
   PCBone *ppbMirror = (PCBone*) lSymPCBone.Get(0);
   DWORD *padw = (DWORD*) lSymDWORD.Get(0);
   for (i = 0; i <= lSymPCBone.Num(); i++) {
      PCBone pMirror = (i < lSymPCBone.Num()) ? ppbMirror[i] : pBone;
      CPoint pLoc;

      // if on a symmetrical one then need to apply that mirror
      CPoint pDirMirror, pUpMirror;
      pDirMirror.Copy (pDir);
      pUpMirror.Copy (&pUp);
      DWORD dwMirror;
      dwMirror = (i < lSymDWORD.Num()) ? padw[i] : 0;
      if (i < lSymDWORD.Num()) for (j = 0; j < 3; j++)
         if (dwMirror & (1 << j)) {
            pDirMirror.p[j] *= -1;
            pUpMirror.p[j] *= -1;
            }

      if (pMirror)
         pLoc.Copy (&pMirror->m_pEndOF);
      else
         pLoc.Zero();

      // poentially create mirrors off this...
      DWORD x,y,z,xMax,yMax,zMax;
      xMax = ((m_dwSymmetry & 0x01) && (fabs(pLoc.p[0]) < CLOSE) && (fabs(pDirMirror.p[0]) > CLOSE)) ? 2 : 1;
      yMax = ((m_dwSymmetry & 0x02) && (fabs(pLoc.p[1]) < CLOSE) && (fabs(pDirMirror.p[1]) > CLOSE)) ? 2 : 1;
      zMax = ((m_dwSymmetry & 0x04) && (fabs(pLoc.p[2]) < CLOSE) && (fabs(pDirMirror.p[2]) > CLOSE)) ? 2 : 1;

      for (x = 0; x < xMax; x++) for (y = 0; y < yMax; y++) for (z = 0; z < zMax; z++) {
         PCBone pNew = new CBone;
         if (!pNew)
            continue;

         pNew->m_pEnd.Copy (&pDirMirror);
         pNew->m_pUp.Copy (&pUpMirror);
         pNew->m_pParent = pMirror;

         if (x) {
            pNew->m_pEnd.p[0] *= -1;
            pNew->m_pUp.p[0] *= -1;
         }
         if (y) {
            pNew->m_pEnd.p[1] *= -1;
            pNew->m_pUp.p[1] *= -1;
         }
         if (z) {
            pNew->m_pEnd.p[2] *= -1;
            pNew->m_pUp.p[2] *= -1;
         }

         if ((x+y+z-3 + ((dwMirror & 0x01) ? 0 : 1) + ((dwMirror & 0x02) ? 0 : 1) + ((dwMirror & 0x04) ? 0 : 1)) % 2)
            pNew->m_fFlipLR = TRUE;

         pNew->CalcMatrix ();


         // set the name
         WCHAR szAppend[64];

         // symmetry string?
         szAppend[0] = 0;
         CPoint pMin, pMax, pAvg;
         if (pMirror)
            pMin.Copy (&pMirror->m_pEndOF);
         else
            pMin.Zero();
         pMax.Add (&pMin, &pNew->m_pEnd);
         pAvg.Average (&pMin, &pMax);
         if (m_dwSymmetry) {
            if (m_dwSymmetry & 0x01) {
               if (pAvg.p[0] > CLOSE)
                  wcscat (szAppend, L"R");
               else if (pAvg.p[0] < -CLOSE)
                  wcscat (szAppend, L"L");
            }
            if (m_dwSymmetry & 0x02) {
               if (pAvg.p[1] > CLOSE)
                  wcscat (szAppend, L"B");
               else if (pAvg.p[1] < -CLOSE)
                  wcscat (szAppend, L"F");
            }
            if (m_dwSymmetry & 0x04) {
               if (pAvg.p[2] > CLOSE)
                  wcscat (szAppend, L"T");
               else if (pAvg.p[2] < -CLOSE)
                  wcscat (szAppend, L"B");
            }
            if (szAppend[0])
               wcscat (szAppend, L" ");
         }
         wcscpy (pNew->m_szName, szAppend);
         wcscat (pNew->m_szName, L"New bone");
         MakeNameUnique (pNew);

         // add this onto the list...
         if (pMirror)
            pMirror->m_lPCBone.Add (&pNew);
         else
            m_lPCBone.Add (&pNew);
      } // xyz
   } // i

   // update calculations
   CalcObjectSpace ();
   ResetIKWant();
   CalcAttribList();

   // reset bone info
   ObjEditBonesClear ();

   if (m_pWorld)
      m_pWorld->ObjectChanged (this);
   return TRUE;
}



// BUGBUG - At some point, add partial skeletons to the built-in objects, like
// hands, whole arms, legs, etc.

// BUGBUG - In object editor, created a mapped attribute that maps to
// "lipsync:mouthopen". Checked the button to NOT create the attribute at
// he bottom of the list (so it's at the top), but didn't remember the
// check value the next time that reopend the attribute dialog

// BUGBUG - timeline window keeps going to the background, even though
// shouldnt because is always on top... or maybe because tooltip is
// popping up
