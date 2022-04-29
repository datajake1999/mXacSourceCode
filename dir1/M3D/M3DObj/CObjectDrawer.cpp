/************************************************************************
CObjectDrawer.cpp - Draws a Drawer.

begun 22/9/02 by Mike Rozak
Copyright 2002 Mike Rozak. All rights reserved
*/

#include <windows.h>
#include "escarpment.h"
#include "..\M3D.h"
#include "resource.h"


typedef struct {
   PWSTR       pszName;    // name of the move point
   int         iX, iY; // -1 if for left/top, 1 for right/bottom
} DRAWERMOVEP, *PDRAWERMOVEP;

static DRAWERMOVEP   gaDrawerMoveP[] = {
   L"Center", 0, 0
};

/* locals */
#define REF_FLIPTEXT          0x0001      // if TRUE, flip the texture because polygon's front is y=1. If FALSE, front is y=-1
#define REF_ROTTEXT           0x0002      // if TRUE, rotate the texture 90 degrees
#define REF_CUTOUT            0x0004      // if TRUE then pPoly is the cutout, and pPanel the edge
#define REF_NOSIDES           0x0008      // if TRUE, then pPoly doesnt draw zipper around the sides

#define BITMORE               .001

typedef struct {
   PCNoodle       pNoodle;       // if not NULL, a noodle to draw
   PCRevolution   pRevolution;   // if not NULL, revolution to draw
   PCListFixed    pPoly;         // if not NULL, a list of CPoint for the polygon
   DWORD          dwFlags;       // one of the REF_XXX
   BOOL           fApplyMatrix;  // if TRUE then apply the rotation matrix
   CMatrix        mRotate;       // rotation matrix
} DRENDELEM, *PDRENDELEM;

/**********************************************************************************
CObjectDrawer::Constructor and destructor */
CObjectDrawer::CObjectDrawer (PVOID pParams, POSINFO pInfo)
{
   m_dwRenderShow = RENDERSHOW_CABINETS;
   m_OSINFO = *pInfo;

   m_fCanBeEmbedded = TRUE;

   m_fOpened = 0;
   m_pOpening.Zero();
   m_pOpening.p[0] = CM_CABINETDOORWIDTH;
   m_pOpening.p[1] = CM_CABINETDOORHEIGHT / 4;
   m_pOpening.p[2] = CM_CABINETDEPTH - CM_THICKCABINETWALL * 2;
   m_pOpening.p[3] = .75;
   m_pExtend.Zero4();
   m_pThick.Zero();
   m_pThick.p[0] = CM_THICKCABINETWALL;
   m_pThick.p[1] = .005;

   m_dwKnobStyle = DKS_CABINET1;
   m_fKnobFromTop = .05;
   m_fKnobRotate = 0;

   m_lDRENDELEM.Init (sizeof(DRENDELEM));
   CalcDoorKnob ();

   // color for the Drawer
   ObjectSurfaceAdd (1, RGB(0x80,0x80,0xff), MATERIAL_PAINTSEMIGLOSS, L"Drawer",
                  &GTEXTURECODE_WoodTrim, &GTEXTURESUB_WoodTrim);
   ObjectSurfaceAdd (2, RGB(0xff,0xff,0xff), MATERIAL_PAINTSEMIGLOSS, L"Drawer tray",
                  &GTEXTURECODE_WoodTrim, &GTEXTURESUB_WoodTrim);
   ObjectSurfaceAdd (3, RGB(0xc0,0xc0,0xc0), MATERIAL_METALSMOOTH, L"Cabinet handle");
   ObjectSurfaceAdd (4, RGB(0xff,0xff,0xff), MATERIAL_PAINTSEMIGLOSS, L"Exposed frame");
}


CObjectDrawer::~CObjectDrawer (void)
{
   // free render elements
   DWORD i;
   for (i = 0; i < m_lDRENDELEM.Num(); i++) {
      PDRENDELEM pr = (PDRENDELEM) m_lDRENDELEM.Get(i);
      if (pr->pNoodle)
         delete pr->pNoodle;
      if (pr->pRevolution)
         delete pr->pRevolution;
      if (pr->pPoly)
         delete pr->pPoly;
   }
   m_lDRENDELEM.Clear();
}


/**********************************************************************************
CObjectDrawer::Delete - Called to delete this object
*/
void CObjectDrawer::Delete (void)
{
   delete this;
}

/**********************************************************************************
CObjectDrawer::Render - Draws a test box.

inputs
   POBJECTRENDER     pr - Render information
*/
void CObjectDrawer::Render (POBJECTRENDER pr, DWORD dwSubObject)
{
   // create the surface render object and draw
   // CRenderSurface rs;
   m_Renderrs.ClearAll();

   CMatrix mObject;
   m_Renderrs.Init (pr->pRS);
   ObjectMatrixGet (&mObject);
   m_Renderrs.Multiply (&mObject);


   // draw the frame
   // try to get it from the container objects first
   GUID gContainer;
   PCObjectSocket pos;
   if (EmbedContainerGet (&gContainer) && m_pWorld) {
      pos = m_pWorld->ObjectGet (m_pWorld->ObjectFind(&gContainer));
      if (!pos)
         goto drawit;

      // CListFixed l1, l2;
      m_RenderL1.Init (sizeof(HVXYZ));
      m_RenderL2.Init (sizeof(HVXYZ));
      pos->ContCutoutToZipper (&m_gGUID, &m_RenderL1, &m_RenderL2);
      if (!m_RenderL1.Num() || !m_RenderL2.Num())
         goto drawit;

      m_Renderrs.SetDefMaterial (m_OSINFO.dwRenderShard, ObjectSurfaceFind (4), m_pWorld);

      // figure out matrix to convert from the containr's space to this object's space
      CMatrix mCont, mInv;
      pos->ObjectMatrixGet (&mCont);
      m_MatrixObject.Invert4 (&mInv);
      mInv.MultiplyLeft (&mCont);

      DWORD i;
      PHVXYZ p;
      for (i = 0; i < m_RenderL1.Num(); i++) {
         p = (PHVXYZ) m_RenderL1.Get(i);
         p->p.MultiplyLeft (&mInv);
      }
      for (i = 0; i < m_RenderL2.Num(); i++) {
         p = (PHVXYZ) m_RenderL2.Get(i);
         p->p.MultiplyLeft (&mInv);
      }


      // zipepr it
      m_Renderrs.ShapeZipper ((PHVXYZ)m_RenderL1.Get(0), m_RenderL1.Num(), (PHVXYZ)m_RenderL2.Get(0), m_RenderL2.Num(),
         TRUE, 0, 1.0, FALSE);
   }

drawit:
   // Deal with whether or not flush
   // Deal with amount open
   fp fTrans;
   fTrans = m_fOpened * m_pOpening.p[2];
   if (m_pExtend.p[0] || m_pExtend.p[1] || m_pExtend.p[2] || m_pExtend.p[3])
      fTrans += m_pThick.p[0];
   m_Renderrs.Translate (0, -fTrans, 0);

   // front
   m_Renderrs.SetDefMaterial (m_OSINFO.dwRenderShard, ObjectSurfaceFind (1), m_pWorld);
   m_Renderrs.TempTextureRotation (PI/2);
   m_Renderrs.ShapeBox (
      -m_pOpening.p[0] / 2 - m_pExtend.p[0],
      0,
      -m_pOpening.p[1]/2 - m_pExtend.p[3],
      m_pOpening.p[0] / 2 + m_pExtend.p[1],
      m_pThick.p[0],
      m_pOpening.p[1]/2 + m_pExtend.p[2]
      );

   // back
   m_Renderrs.SetDefMaterial (m_OSINFO.dwRenderShard, ObjectSurfaceFind (2), m_pWorld);
   m_Renderrs.TempTextureRotation (PI/2);
   m_Renderrs.ShapeBox (
      -m_pOpening.p[0]/2 + m_pThick.p[1],
      m_pOpening.p[2] - m_pThick.p[1],
      -m_pOpening.p[1]/2,
      m_pOpening.p[0]/2 - m_pThick.p[1],
      m_pOpening.p[2],
      -m_pOpening.p[1]/2 + m_pOpening.p[1] * m_pOpening.p[3]
      );

   // left and right
   m_Renderrs.TempTextureRotation (PI/2);
   m_Renderrs.ShapeBox (
      -m_pOpening.p[0]/2,
      m_pThick.p[0],
      -m_pOpening.p[1]/2,
      -m_pOpening.p[0]/2 + m_pThick.p[1],
      m_pOpening.p[2],
      -m_pOpening.p[1]/2 + m_pOpening.p[1] * m_pOpening.p[3]
      );
   m_Renderrs.TempTextureRotation (PI/2);
   m_Renderrs.ShapeBox (
      m_pOpening.p[0]/2,
      m_pThick.p[0],
      -m_pOpening.p[1]/2,
      m_pOpening.p[0]/2 - m_pThick.p[1],
      m_pOpening.p[2],
      -m_pOpening.p[1]/2 + m_pOpening.p[1] * m_pOpening.p[3]
      );

   // shelf
   m_Renderrs.ShapeBox (
      -m_pOpening.p[0]/2 + m_pThick.p[1],
      m_pThick.p[0],
      -m_pOpening.p[1] / 2,
      m_pOpening.p[0]/2 - m_pThick.p[1],
      m_pOpening.p[2] - m_pThick.p[1],
      -m_pOpening.p[1] / 2 + m_pThick.p[1]
      );


   // draw the cabinet handle
   m_Renderrs.SetDefMaterial (m_OSINFO.dwRenderShard, ObjectSurfaceFind (3), m_pWorld);

   // May need to translate back so knob placed properly
   m_Renderrs.Translate (0, 0, m_pOpening.p[1]/2 + m_pExtend.p[2] - m_fKnobFromTop);

   DWORD i, j;
   for (i = 0; i < m_lDRENDELEM.Num(); i++) {
      PDRENDELEM pre = (PDRENDELEM) m_lDRENDELEM.Get(i);

      if (pre->fApplyMatrix) {
         m_Renderrs.Push();
         m_Renderrs.Multiply (&pre->mRotate);
      }

      if (pre->pNoodle)
         pre->pNoodle->Render (pr, &m_Renderrs);
      if (pre->pRevolution)
         pre->pRevolution->Render(pr, &m_Renderrs);

      if (pre->pPoly) {  // just polygon
         // create all the bits for this
         // NOTE: Assuming that normal is y=-1, or if reverse, y=1
         DWORD dwNormal, dwPoints, dwTextures, dwVertices;
         PCPoint pPoints;
         PVERTEX pVertices;
         PTEXTPOINT5 pTextures;
         PPOLYDESCRIPT pPoly;
         DWORD dwNum = pre->pPoly->Num();
         dwNormal = m_Renderrs.NewNormal(0, (pre->dwFlags & REF_FLIPTEXT) ? 1 : -1, 0, TRUE);
         pPoints = m_Renderrs.NewPoints (&dwPoints, dwNum);
         if (pPoints)
            memcpy (pPoints, pre->pPoly->Get(0), dwNum * sizeof(CPoint));

         pTextures = m_Renderrs.NewTextures (&dwTextures, dwNum);
         if (pTextures && pPoints) {
            // copy over
            TEXTPOINT5 pll;
            memset (&pll, 0, sizeof(pll));
            for (j = 0; j < dwNum; j++) {
               pTextures[j].hv[0] = pPoints[j].p[0] * ((pre->dwFlags & REF_FLIPTEXT) ? -1 : 1);
               pTextures[j].hv[1] = -pPoints[j].p[2];  // since textures go in reverse direction

               pTextures[j].xyz[0] = pPoints[j].p[0];
               pTextures[j].xyz[1] = pPoints[j].p[1];
               pTextures[j].xyz[2] = pPoints[j].p[2];


               // keep track of lower left corner
               if (j) {
                  pll.hv[0] = min(pll.hv[0], pTextures[j].hv[0]);
                  pll.hv[1] = max(pll.hv[1], pTextures[j].hv[1]);
               }
               else
                  pll = pTextures[j];
            }

            // loop and adjust by offset
            for (j = 0; j < dwNum; j++) {
               pTextures[j].hv[0] -= pll.hv[0];
               pTextures[j].hv[1] -= pll.hv[1];
            }

            // if have flag for rotation then do so
            if (pre->dwFlags & REF_ROTTEXT) for (j = 0; j < dwNum; j++) {
               fp f;
               f = pTextures[j].hv[0];
               pTextures[j].hv[0] = pTextures[j].hv[1];
               pTextures[j].hv[1] = -f;
            }
            // scale the textures
            m_Renderrs.ApplyTextureRotation (pTextures, dwNum);
         }  // pTextures

         pVertices = m_Renderrs.NewVertices (&dwVertices, dwNum);
         DWORD dwColor;
         dwColor = m_Renderrs.DefColor();
         if (pVertices) {
            for (j = 0; j < dwNum; j++) {
               pVertices[j].dwNormal = dwNormal;
               pVertices[j].dwColor = dwColor;
               pVertices[j].dwPoint = pPoints ? (dwPoints + j) : 0;
               pVertices[j].dwTexture = pTextures ? (dwTextures + j) : 0;
            }
         }

         // polugon
         pPoly = m_Renderrs.NewPolygon (dwNum);
         if (pPoly) {
            // can backface cull if thick (which means Y != 0), but
            // if thin, dont
            pPoly->fCanBackfaceCull = (pPoints[0].p[1] ? TRUE : FALSE);
            pPoly->dwSurface = m_Renderrs.DefSurface();
            pPoly->dwIDPart = m_Renderrs.NewIDPart();

            DWORD *padw;
            padw = (DWORD*) (pPoly + 1);
            for (j = 0; j < dwNum; j++)
               padw[j] = dwVertices + j;
         }


      }  // if pPoly

      if (pre->fApplyMatrix)
         m_Renderrs.Pop();

   } // over render objects

   m_Renderrs.Commit();
}


// NOTE: Not doing QueryBoundingBox() for drawer sice complex code and likely
// to introduce bugs

/**********************************************************************************
CObjectDrawer::Clone - Clones this

inputs
   none
returns
   PCObjectSocket - Clone
*/
PCObjectSocket CObjectDrawer::Clone (void)
{
   PCObjectDrawer pNew;

   pNew = new CObjectDrawer(NULL, &m_OSINFO);

   // clone template info
   CloneTemplate(pNew);

   pNew->m_fOpened = m_fOpened;
   pNew->m_pOpening.Copy (&m_pOpening);
   pNew->m_pExtend.Copy (&m_pExtend);
   pNew->m_pThick.Copy (&m_pThick);

   pNew->m_dwKnobStyle = m_dwKnobStyle;
   pNew->m_fKnobFromTop = m_fKnobFromTop;
   pNew->m_fKnobRotate = m_fKnobRotate;

   // free render elements
   DWORD i;
   for (i = 0; i < pNew->m_lDRENDELEM.Num(); i++) {
      PDRENDELEM pr = (PDRENDELEM) pNew->m_lDRENDELEM.Get(i);
      if (pr->pNoodle)
         delete pr->pNoodle;
      if (pr->pRevolution)
         delete pr->pRevolution;
      if (pr->pPoly)
         delete pr->pPoly;
   }
   pNew->m_lDRENDELEM.Clear();

   pNew->m_lDRENDELEM.Init (sizeof(DRENDELEM), m_lDRENDELEM.Get(0), m_lDRENDELEM.Num());

   for (i = 0; i < pNew->m_lDRENDELEM.Num(); i++) {
      PDRENDELEM pr = (PDRENDELEM) pNew->m_lDRENDELEM.Get(i);
      if (pr->pNoodle)
         pr->pNoodle = pr->pNoodle->Clone();
      if (pr->pRevolution)
         pr->pRevolution = pr->pRevolution->Clone();
      if (pr->pPoly) {
         PCListFixed pOld = pr->pPoly;
         pr->pPoly = new CListFixed;
         if (pr->pPoly)
            pr->pPoly->Init (sizeof(CPoint), pOld->Get(0), pOld->Num());
      }
   }
   return pNew;
}

static PWSTR gpszOpened = L"Opened";
static PWSTR gpszOpening = L"Opening";
static PWSTR gpszExtend = L"Extend";
static PWSTR gpszThick = L"Thick";
static PWSTR gpszKnobStyle = L"KnobStyle";
static PWSTR gpszKnobRotate = L"KnobRotate";
static PWSTR gpszKnobFromTop = L"KnobFromTop";

PCMMLNode2 CObjectDrawer::MMLTo (void)
{
   // call into the template first
   PCMMLNode2 pNode = MMLToTemplate();
   if (!pNode)
      return NULL;

   MMLValueSet (pNode, gpszOpened, m_fOpened);
   MMLValueSet (pNode, gpszOpening, &m_pOpening);
   MMLValueSet (pNode, gpszExtend, &m_pExtend);
   MMLValueSet (pNode, gpszThick, &m_pThick);

   MMLValueSet (pNode, gpszKnobStyle, (int) m_dwKnobStyle);
   MMLValueSet (pNode, gpszKnobFromTop, m_fKnobFromTop);
   MMLValueSet (pNode, gpszKnobRotate, m_fKnobRotate);

   return pNode;
}

BOOL CObjectDrawer::MMLFrom (PCMMLNode2 pNode)
{
   // call into the template first
   if (!MMLFromTemplate (pNode))
      return FALSE;

   m_fOpened = MMLValueGetDouble (pNode, gpszOpened, 0);
   MMLValueGetPoint (pNode, gpszOpening, &m_pOpening);
   MMLValueGetPoint (pNode, gpszExtend, &m_pExtend);
   MMLValueGetPoint (pNode, gpszThick, &m_pThick);

   m_dwKnobStyle = (DWORD) MMLValueGetInt (pNode, gpszKnobStyle, 0);
   m_fKnobFromTop = MMLValueGetDouble (pNode, gpszKnobFromTop, 0);
   m_fKnobRotate = MMLValueGetDouble (pNode, gpszKnobRotate, 0);

   CalcDoorKnob ();
   return TRUE;
}


/**********************************************************************************
CObjectDrawer::OpenGet - 
returns how open the object is, from 0 (closed) to 1.0 (open), or
< 0 for an object that can't be opened
*/
fp CObjectDrawer::OpenGet (void)
{
   return m_fOpened;
}

/**********************************************************************************
CObjectDrawer::OpenSet - 
opens/closes the object. if fopen==0 it's close, 1.0 = open, and
values in between are partially opened closed. Returne TRUE if success
*/
BOOL CObjectDrawer::OpenSet (fp fOpen)
{
   fOpen = max(0,fOpen);
   fOpen = min(1,fOpen);

   m_pWorld->ObjectAboutToChange (this);
   m_fOpened = fOpen;
   m_pWorld->ObjectChanged (this);

   return TRUE;
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
BOOL CObjectDrawer::MoveReferencePointQuery (DWORD dwIndex, PCPoint pp)
{
   PDRAWERMOVEP pwp;

   if (dwIndex >= sizeof(gaDrawerMoveP) / sizeof(DRAWERMOVEP))
      return FALSE;
   pwp = gaDrawerMoveP;

   pp->Zero(); // always at 0,0,0

   return TRUE;
}

/**************************************************************************************
CObjectDrawer::MoveReferenceStringQuery -
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
BOOL CObjectDrawer::MoveReferenceStringQuery (DWORD dwIndex, PWSTR psz, DWORD dwSize, DWORD *pdwNeeded)
{
   PDRAWERMOVEP pwp;

   if (dwIndex >= sizeof(gaDrawerMoveP) / sizeof(DRAWERMOVEP)) {
      if (pdwNeeded)
         *pdwNeeded = 0;
      return FALSE;
   }
   pwp = gaDrawerMoveP;

   DWORD dwNeeded;
   dwNeeded = ((DWORD)wcslen (pwp[dwIndex].pszName) + 1) * 2;
   if (pdwNeeded)
      *pdwNeeded = dwNeeded;
   if (dwNeeded <= dwSize) {
      wcscpy (psz, pwp[dwIndex].pszName);
      return TRUE;
   }
   else
      return FALSE;
}


/**********************************************************************************
CObjectDrawer::EmbedDoCutout - Member function specific to the template. Called
when the object has moved within the surface. This enables the super-class for
the embedded object to pass a cutout into the container. (Basically, specify the
hole for the window or door)
*/
BOOL CObjectDrawer::EmbedDoCutout (void)
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

   // add the points
   CPoint pFront[4], pBack[4];
   DWORD dwNum;
   dwNum = 4;
   pFront[0].Zero();
   pFront[0].p[0] = -m_pOpening.p[0] / 2;
   pFront[0].p[2] = -m_pOpening.p[1] / 2;
   pFront[0].p[3] = 1;
   pFront[1].Copy (&pFront[0]);
   pFront[1].p[2] *= -1;
   pFront[2].Copy (&pFront[1]);
   pFront[2].p[0] *= -1;
   pFront[3].Copy (&pFront[2]);
   pFront[3].p[2] *= -1;


   // convert to object's space
   DWORD i;
   for (i = 0; i < dwNum; i++) {
      pBack[i].Copy (&pFront[i]);
      pBack[i].p[1] = .1;

      pFront[i].MultiplyLeft (&mTrans);
      pBack[i].MultiplyLeft (&mTrans);
   }
   pos->ContCutout (&m_gGUID, dwNum, pFront, pBack, TRUE);

   return TRUE;
}


/**********************************************************************************
CObjectDrawer::CalcDoorKnob - Adds the doorknob to the m_lRENDEELEM list.
*/
void CObjectDrawer::CalcDoorKnob (void)
{
   // doorknob - loop over sides, 0 is front
   DRENDELEM re;


   // free render elements
   DWORD i;
   for (i = 0; i < m_lDRENDELEM.Num(); i++) {
      PDRENDELEM pr = (PDRENDELEM) m_lDRENDELEM.Get(i);
      if (pr->pNoodle)
         delete pr->pNoodle;
      if (pr->pRevolution)
         delete pr->pRevolution;
      if (pr->pPoly)
         delete pr->pPoly;
   }
   m_lDRENDELEM.Clear();

   // clear it out
   memset (&re, 0, sizeof(re));
   re.dwFlags = REF_NOSIDES;

   CPoint pKnob;
   pKnob.Zero();

   // make the matrix to rotate around the knob
   CMatrix  m, m2;
   BOOL fRotate;
   m.Identity();
   fRotate = FALSE;
   if (m_fKnobRotate) {
      fRotate = TRUE;
      m.Translation (-pKnob.p[0], -pKnob.p[1], -pKnob.p[2]);
      m2.RotationY (m_fKnobRotate);
      m.MultiplyRight (&m2);
      m2.Translation (pKnob.p[0], pKnob.p[1], pKnob.p[2]);
      m.MultiplyRight (&m2);
   }

   // direction of the other end
   // pRight = vector to other side. (It's only the right if doorknob on left)
   CPoint pRight, pFront, pUp;
   pFront.Zero();
   pFront.p[1] = -1;
   pRight.Zero();
   pRight.p[2] = 1;
   pUp.CrossProd (&pFront, &pRight);
   pUp.Normalize();
   pUp.p[2] = fabs(pUp.p[2]); // set certain direciton for this
   pUp.p[0] = fabs(pUp.p[0]); // set certain direction for this

   PCNoodle pn;
   PCRevolution pr;
   CPoint pStart, pEnd, pScale, pt;
   CPoint paLoc[7];
   fp fScale;
   pStart.Zero();
   pEnd.Zero();
   pScale.Zero();
   fp m_fThickness;
   m_fThickness = 0; // so that draws right at 0,0,0
   switch (m_dwKnobStyle) {
   case DKS_CABINET1:       // small D handle 1
   case DKS_CABINET2:       // larger D handle
   case DKS_CABINET7:       // classic handle
      pt.Copy (&pUp);
      fScale = (m_dwKnobStyle != DKS_CABINET2) ? .66 : 1;
      pt.Scale (fScale *.08);

      for (i = 0; i < 5; i++) {
         paLoc[i].Copy (&pKnob);
         paLoc[i].p[1] = pFront.p[1] * (m_fThickness/2 + (((i > 0) && (i < 4)) ? (fScale * .04) : 0) );
         if (i < 2)
            paLoc[i].Subtract (&pt);
         if (i > 2)
            paLoc[i].Add (&pt);
      }
      pn = new CNoodle;
      if (!pn)
         break;
      pn->DrawEndsSet (FALSE);
      pn->FrontVector (&pRight);
      pn->PathSpline (FALSE, 5, paLoc, (DWORD*)SEGCURVE_ELLIPSENEXT, 1);
      pScale.p[0] = fScale *.01;
      pScale.p[1] = fScale *.02;
      pn->ScaleVector (&pScale);
      pn->ShapeDefault (NS_CIRCLEFAST);
      if (fRotate)
         pn->MatrixSet (&m);
      re.pNoodle = pn;
      m_lDRENDELEM.Add (&re);

      if (m_dwKnobStyle == DKS_CABINET7) {
         pStart.Copy (&pKnob);
         pStart.p[1] = pFront.p[1] * m_fThickness/2;
         pScale.Zero();
         pScale.p[0] = fScale * .25;
         pScale.p[1] = fScale * .10;
         pScale.p[2] = .003;

         pr = new CRevolution;
         if (!pr)
            break;
         pr->BackfaceCullSet (TRUE);
         pr->DirectionSet (&pStart, &pFront, &pUp);
         pr->ProfileSet (RPROF_C);
         pr->RevolutionSet (RREV_CIRCLE);
         pr->ScaleSet (&pScale);
         re.pNoodle = NULL;
         re.pRevolution = pr;
         if (fRotate) {
            re.fApplyMatrix = TRUE;
            re.mRotate.Copy (&m);
         }
         m_lDRENDELEM.Add (&re);
      }

      break;

   case DKS_CABINET3:       // small F hangle
   case DKS_CABINET4:       // larger F handle
      fScale = (m_dwKnobStyle == DKS_CABINET3) ? .66 : 1;
      pt.Copy (&pUp);
      pt.Scale (fScale *.1);

      CPoint pStart, pEnd;
      pStart.Copy (&pKnob);
      pStart.p[1] = pFront.p[1] * (m_fThickness/2 + fScale * .04);
      pEnd.Copy (&pStart);
      pStart.Add (&pt);
      pEnd.Subtract (&pt);

      pn = new CNoodle;
      if (!pn)
         break;
      pn->DrawEndsSet (TRUE);
      pn->FrontVector (&pRight);
      pn->PathLinear (&pStart, &pEnd);
      pScale.p[0] = pScale.p[1] = fScale *.02;
      pn->ScaleVector (&pScale);
      pn->ShapeDefault (NS_CIRCLEFAST);
      if (fRotate)
         pn->MatrixSet (&m);
      re.pNoodle = pn;
      m_lDRENDELEM.Add (&re);

      // two posts
      for (i = 0; i < 2; i++) {
         pt.Copy (&pUp);
         pt.Scale (fScale *.075 * (i ? -1 : 1));
         pStart.Copy (&pKnob);
         pStart.p[1] = pFront.p[1] * m_fThickness/2;
         pStart.Add (&pt);
         pEnd.Copy (&pStart);
         pEnd.p[1] += pFront.p[1] * fScale * .04;

         pn = new CNoodle;
         if (!pn)
            break;
         pn->DrawEndsSet (FALSE);
         pn->FrontVector (&pRight);
         pn->PathLinear (&pStart, &pEnd);
         pScale.p[0] = pScale.p[1] = fScale *.01;
         pn->ScaleVector (&pScale);
         pn->ShapeDefault (NS_CIRCLEFAST);
         if (fRotate)
            pn->MatrixSet (&m);
         re.pNoodle = pn;
         m_lDRENDELEM.Add (&re);
      }
      
      break;

   case DKS_CABINET5:       // knob
      pStart.Copy (&pKnob);
      pStart.p[1] = pFront.p[1] * m_fThickness/2;
      pScale.Zero();
      pScale.p[0] = pScale.p[1] = .04;
      pScale.p[2] = .05;

      pr = new CRevolution;
      if (!pr)
         break;
      pr->BackfaceCullSet (TRUE);
      pr->DirectionSet (&pStart, &pFront, &pUp);
      pr->ProfileSet (RPROF_LIGHTGLOBE);
      pr->RevolutionSet (RREV_CIRCLE);
      pr->ScaleSet (&pScale);
      re.pRevolution = pr;
      if (fRotate) {
         re.fApplyMatrix = TRUE;
         re.mRotate.Copy (&m);
      }
      m_lDRENDELEM.Add (&re);

      break;

   case DKS_CABINET6:       // loop that put fingers in
      pStart.Copy (&pKnob);
      pStart.p[1] = pFront.p[1] * m_fThickness/2;
      pt.Copy (&pRight);
      pt.Scale (-.025);
      pStart.Add (&pt);
      pScale.Zero();
      pScale.p[0] = .1;
      pScale.p[1] = pScale.p[2] = .05;

      pr = new CRevolution;
      if (!pr)
         break;
      pr->BackfaceCullSet (FALSE);

      // BUGFIX - So handles on opposite sides of double doors appear
      pEnd.CrossProd (&pRight, &pUp);
      if (pEnd.p[1] > 0) {
         pEnd.Copy (&pUp);
         pEnd.Scale (-1);
      }
      else
         pEnd.Copy (&pUp);
      pr->DirectionSet (&pStart, &pRight, &pEnd);
      pr->ProfileSet (PRROF_CIRCLEHEMI);
      pr->RevolutionSet (RREV_CIRCLEHEMI);
      pr->ScaleSet (&pScale);
      re.pRevolution = pr;
      if (fRotate) {
         re.fApplyMatrix = TRUE;
         re.mRotate.Copy (&m);
      }
      m_lDRENDELEM.Add (&re);

      break;


   }
}


/**********************************************************************************
CObjectDrawer::CommitRect - Given a rectangle with LL and UR corner, commits it as a polygon
(just the one side though)

NOTE: pMin and pMax can be any corner since internally takes max and min.

inputs
   PCPoint     pMin - lower left corner (assuming that looking from y=-1 direction)
   PCPoint     pMax - upper right corner (assuming that looking from y=-1 direction)
   fp      fY - Y location. If it's negative then pMin and pMax use as LL and UR corner.
               if positive, pMin is LR and pMax is UR corner
   DWORD       dwColor - Color to use
   DWORD       dwFlags - Flags to or in
   PCMatrix    pMatrix - If NOT null, then rotate by this amount
returns
   BOOL - TRUE if success
*/
BOOL CObjectDrawer::CommitRect (PCPoint ppMin, PCPoint ppMax, fp fY, DWORD dwColor, DWORD dwFlags, PCMatrix pMatrix)
{
   // clear it out
   DRENDELEM re;
   memset (&re, 0, sizeof(re));
   re.dwFlags = dwFlags;
   if (pMatrix) {
      re.fApplyMatrix = TRUE;
      re.mRotate.Copy (pMatrix);
   }

   CPoint pMax, pMin;
   pMax.Copy (ppMin);
   pMax.Max (ppMax);
   pMin.Copy (ppMin);
   pMin.Min (ppMax);

   // new list
   PCListFixed pl;
   pl = new CListFixed;
   if (!pl)
      return FALSE;
   pl->Init (sizeof(CPoint));

   DWORD dws;
   dws = (fY < 0) ? 0 : 1;

   // fill in list
   CPoint p;
   p.Zero();
   pl->Required (4);
   p.p[0] = dws ? pMax.p[0] : pMin.p[0];
   p.p[1] = fY;
   p.p[2] = pMax.p[2];
   pl->Add (&p);
   p.p[0] = dws ? pMin.p[0] : pMax.p[0];
   pl->Add (&p);
   p.p[2] = pMin.p[2];
   pl->Add (&p);
   p.p[0] = dws ? pMax.p[0] : pMin.p[0];
   pl->Add (&p);
   if (dws)
      re.dwFlags |= REF_FLIPTEXT;

   re.pPoly = pl;
   m_lDRENDELEM.Add (&re);

   return TRUE;
}

/**********************************************************************************
CObjectDrawer::DialogQuery - Returns TRUE if the object supports a custom
dialog box, FALSE if it doesn't. The default object doesn't
*/
BOOL CObjectDrawer::DialogQuery (void)
{
   return TRUE;
}


/* DrawerDialogPage
*/
BOOL DrawerDialogPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCObjectDrawer pv = (PCObjectDrawer) pPage->m_pUserData;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         DWORD i;
         WCHAR szTemp[32];
         PCEscControl pControl;

         ComboBoxSet (pPage, L"knobstyle", pv->m_dwKnobStyle);

         MeasureToString (pPage, L"knobfromtop", pv->m_fKnobFromTop);
         AngleToControl (pPage, L"knobrotate", pv->m_fKnobRotate);

         for (i = 0; i < 3; i++) {
            swprintf (szTemp, L"opening%d", (int) i);
            MeasureToString (pPage, szTemp, pv->m_pOpening.p[i]);
         }
         for (i = 0; i < 4; i++) {
            swprintf (szTemp, L"extend%d", (int) i);
            MeasureToString (pPage, szTemp, pv->m_pExtend.p[i]);
         }
         for (i = 0; i < 2; i++) {
            swprintf (szTemp, L"thick%d", (int) i);
            MeasureToString (pPage, szTemp, pv->m_pThick.p[i]);
         }

         pControl = pPage->ControlFind (L"opening3");
         if (pControl)
            pControl->AttribSetInt (Pos(), (int)(pv->m_pOpening.p[3] * 100));
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

         if (!_wcsicmp(psz, L"knobstyle")) {
            if (dwVal == pv->m_dwKnobStyle)
               break;

            pv->m_pWorld->ObjectAboutToChange (pv);
            pv->m_dwKnobStyle = dwVal;
            pv->CalcDoorKnob();
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

         // Get all values
         DWORD i;
         WCHAR szTemp[32];
         MeasureParseString (pPage, L"knobfromtop", &pv->m_fKnobFromTop);
         pv->m_fKnobRotate = AngleFromControl (pPage, L"knobrotate");

         for (i = 0; i < 3; i++) {
            swprintf (szTemp, L"opening%d", (int) i);
            MeasureParseString (pPage, szTemp, &pv->m_pOpening.p[i]);
            pv->m_pOpening.p[i] = max(pv->m_pOpening.p[i], CLOSE);
         }
         for (i = 0; i < 4; i++) {
            swprintf (szTemp, L"extend%d", (int) i);
            MeasureParseString (pPage, szTemp, &pv->m_pExtend.p[i]);
         }
         for (i = 0; i < 2; i++) {
            swprintf (szTemp, L"thick%d", (int) i);
            MeasureParseString (pPage, szTemp, &pv->m_pThick.p[i]);
            pv->m_pThick.p[i] = max (pv->m_pThick.p[i], CLOSE);
         }

         pv->CalcDoorKnob();
         pv->EmbedDoCutout();

         if (pv->m_pWorld)
            pv->m_pWorld->ObjectChanged (pv);
      }
      break;

   case ESCN_SCROLL:
   case ESCN_SCROLLING:
      {
         PESCNSCROLL p = (PESCNSCROLL) pParam;

         // only do one scroll bar
         PWSTR psz;
         psz = p->pControl->m_pszName;
         if (!psz)
            break;

         // get value
         fp fVal, *pf;
         fVal = p->pControl->AttribGetInt (Pos()) / 100.0;
         pf = NULL;

         if (!_wcsicmp(psz, L"opening3"))
            pf = &pv->m_pOpening.p[3];

         if (!pf || (*pf == fVal))
            break;   // no change

         pv->m_pWorld->ObjectAboutToChange (pv);
         *pf = fVal;
         // dont need to - pv->CalcDoorKnob()
         pv->m_pWorld->ObjectChanged (pv);
      }
      break;



   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Drawer settings";
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}



/**********************************************************************************
CObjectDrawer::DialogShow - Causes the object to show a dialog box that allows
different attributes to be changed.

inputs
   DWORD             dwSuface - Surface that was clicked on. 0 if unknown.
   PCEscWindow       pWindow - Escarpment window to draw it in
returns
   BOOL - TRUE if user pressed "Back", FALSE if user pressed "Close"
*/
BOOL CObjectDrawer::DialogShow (DWORD dwSurface, PCEscWindow pWindow, PCPoint pClick)
{
   PWSTR pszRet;
   pszRet = pWindow->PageDialog (ghInstance, IDR_MMLDRAWERDIALOG, DrawerDialogPage, this);
   if (!pszRet)
      return FALSE;
   return !_wcsicmp(pszRet, Back());
}

