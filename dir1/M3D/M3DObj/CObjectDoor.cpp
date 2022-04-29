/************************************************************************
CObjectDoor.cpp - Draws a box.

begun 24/4/02 by Mike Rozak
Copyright 2002 Mike Rozak. All rights reserved
*/

#include <windows.h>
#include "escarpment.h"
#include "..\M3D.h"
#include "resource.h"

#define DOORCOLORSTART  5     // where door colors start

typedef struct {
   PWSTR       pszName;    // name of the move point
   int         iX, iY; // -1 if for left/top, 1 for right/bottom
} WINDOWMOVEP, *PWINDOWMOVEP;

static WINDOWMOVEP   gaWindowMoveP[] = {
   L"Center", 0, 0,
   L"Lower left corner", -1, -1,
   L"Lower right corner", 1, -1,
   L"Upper left corner", -1, 1,
   L"Upper right corner", 1, 1
};
static WINDOWMOVEP   gaDoorMoveP[] = {
   L"Center bottom", 0, -1,
   L"Lower left corner", -1, -1,
   L"Lower right corner", 1, -1,
   L"Upper left corner", -1, 1,
   L"Upper right corner", 1, 1
};

// used for door/window dialog
typedef struct {
   PCObjectDoor      pv;
   DWORD             dwOpening;
   PCDoorOpening     pdo;
} ODD, *PODD;

/**********************************************************************************
CObjectDoor::Constructor and destructor */
CObjectDoor::CObjectDoor (PVOID pParams, POSINFO pInfo)
{
   //m_dwRenderShow = RENDERSHOW_DOORS;
   m_dwType = (DWORD)(size_t) pParams;
   m_OSINFO = *pInfo;


   m_fAutoDepth = TRUE;
   m_fOpened = .25;   // show all doors as partially opened
   m_fAutoAdjust = .05; // 1" to either side
   m_dwBevelSide = 1;
   m_pBevel.Zero();
   m_fDirty = TRUE;
   m_lOpenings.Init (sizeof(PCDoorOpening));

   m_pDoorFrame = new CDoorFrame;
   DWORD dwFrameShape;
   dwFrameShape = HIWORD(m_dwType);
   // translate the 0x1000 and 0x2000 bits to 0x10000 and 0x20000 for frame shape
   if (dwFrameShape & 0x1000)
      dwFrameShape = (dwFrameShape & ~0x1000) | 0x10000;
   DWORD dwDoor;
   dwDoor = DFD_DOOR;
   switch (m_dwType & (DSSTYLE_WINDOW | DSSTYLE_CABINET)) {
   case DSSTYLE_WINDOW:
      dwDoor = DFD_WINDOW;
      break;
   case DSSTYLE_CABINET:
      dwDoor = DFD_CABINET;
      m_fOpened = 0; // default to closed
      break;
   }

   if (dwDoor == DFD_CABINET) {
      m_pDoorFrame->FrameSizeSet (0, 0);
      m_pDoorFrame->SubFrameSizeSet (.04, 0.015);
      m_fAutoAdjust = 0;
   }
   m_pDoorFrame->Init (dwFrameShape, dwDoor, LOWORD(m_dwType));

   m_fCanBeEmbedded = TRUE;
   m_fEmbedFitToFloor = (m_pDoorFrame->m_dwDoor == DFD_DOOR);
   switch (m_pDoorFrame->m_dwDoor) {
   case DFD_DOOR:
   default:
      m_dwRenderShow = RENDERSHOW_DOORS;
      break;
   case DFD_WINDOW:
      m_dwRenderShow = RENDERSHOW_WINDOWS;
      break;
   case DFD_CABINET:
      m_dwRenderShow = RENDERSHOW_CABINETS;
      break;
   }

   // color for the box
   ObjectSurfaceAdd (1, RGB(0xff,0xff,0xc0), MATERIAL_PAINTGLOSS,
      (m_pDoorFrame->m_dwDoor == DFD_CABINET) ? L"Cabinet frame" :
      ((m_pDoorFrame->m_dwDoor == DFD_DOOR) ? L"Door frame (internal)" : L"Window frame (internal)"));
   ObjectSurfaceAdd (2, RGB(0xff,0xff,0xc0), MATERIAL_PAINTGLOSS,
      (m_pDoorFrame->m_dwDoor == DFD_CABINET) ? L"Cabinet frame" :
      ((m_pDoorFrame->m_dwDoor == DFD_DOOR) ? L"Door frame (external)" : L"Window frame (external)"));
   ObjectSurfaceAdd (3, RGB(0xff,0xff,0xff), MATERIAL_PAINTSEMIGLOSS,
      (m_pDoorFrame->m_dwDoor == DFD_CABINET) ? L"Cabinet frame" : L"Exposed frame");

}


CObjectDoor::~CObjectDoor (void)
{
   if (m_pDoorFrame)
      delete m_pDoorFrame;

   // free openings
   DWORD i;
   for (i = 0; i < m_lOpenings.Num(); i++) {
      PCDoorOpening pdo = *((PCDoorOpening*) m_lOpenings.Get(i));
      delete pdo;
   }
   m_lOpenings.Clear();
}


/**********************************************************************************
CObjectDoor::Delete - Called to delete this object
*/
void CObjectDoor::Delete (void)
{
   delete this;
}

/**********************************************************************************
CObjectDoor::Render - Draws a door.

inputs
   POBJECTRENDER     pr - Render information
*/
void CObjectDoor::Render (POBJECTRENDER pr, DWORD dwSubObject)
{
   // create the surface render object and draw
   // CRenderSurface rs;
   m_Renderrs.ClearAll();
   CMatrix mObject;
   m_Renderrs.Init (pr->pRS);
   ObjectMatrixGet (&mObject);
   m_Renderrs.Multiply (&mObject);

   // autoadjust the thicknes
   fp fWidth, fDepth, fWant;
   m_pDoorFrame->FrameSizeGet (&fWidth, &fDepth);
   if (m_fAutoDepth && (m_fContainerDepth > EPSILON)) {
      fWant = m_fContainerDepth + m_fAutoAdjust;
      fWant = max(0,fWant);
      if (fWant != fDepth) {
         m_pDoorFrame->FrameSizeSet (fWidth, fWant);
         m_fDirty = TRUE;  // will need to recalc openings
      }
      fDepth = fWant;
   }

   // which side is outside
   BOOL fThisOutside;
   fThisOutside = (m_dwContSideInfoThis == 1);

   // translate so centered in the wall
   fp fTrans;
   fTrans = m_fContainerDepth/2.0;  // asume larger
   if (fDepth < m_fContainerDepth) {
      switch (this->m_dwBevelSide) {
      case 0:  // outside
         fTrans = fDepth / 2 - CLOSE;  // just a bit of overhang
         break;
      default:
      case 1:  // center
         // leave as is since already centered
         break;
      case 2:  // inside
         fTrans = m_fContainerDepth - fDepth / 2 + CLOSE;   // just a bit of overhang
         break;
      }
   }

   // translate so that centered
   m_Renderrs.Push();
   if (!fThisOutside)
      fTrans = m_fContainerDepth - fTrans;
   m_Renderrs.Translate (0, fTrans, 0);

   // Which color to used depends which sides of the door are internal vs. external

   // front of door frame
   m_Renderrs.SetDefMaterial (m_OSINFO.dwRenderShard, ObjectSurfaceFind ((m_dwContSideInfoThis == 1) ? 2 : 1), m_pWorld);
   m_pDoorFrame->Render (pr, &m_Renderrs, 0);

   // back of door frame
   m_Renderrs.SetDefMaterial (m_OSINFO.dwRenderShard, ObjectSurfaceFind ((m_dwContSideInfoOther == 1) ? 2 : 1), m_pWorld);
   m_pDoorFrame->Render (pr, &m_Renderrs, 1);
   m_Renderrs.Pop();

   // draw the exposed frame
   if (fDepth < m_fContainerDepth) {
      m_Renderrs.SetDefMaterial (m_OSINFO.dwRenderShard, ObjectSurfaceFind (3), m_pWorld);

      // try to get it from the container objects first
      GUID gContainer;
      PCObjectSocket pos;
      if (EmbedContainerGet (&gContainer) && m_pWorld) {
         pos = m_pWorld->ObjectGet (m_pWorld->ObjectFind(&gContainer));
         if (!pos)
            goto drawit;

         CListFixed l1, l2;
         l1.Init (sizeof(HVXYZ));
         l2.Init (sizeof(HVXYZ));
         pos->ContCutoutToZipper (&m_gGUID, &l1, &l2);
         if (!l1.Num() || !l2.Num())
            goto drawit;

         // figure out matrix to convert from the containr's space to this object's space
         CMatrix mCont, mInv;
         pos->ObjectMatrixGet (&mCont);
         m_MatrixObject.Invert4 (&mInv);
         mInv.MultiplyLeft (&mCont);

         DWORD i;
         PHVXYZ p;
         for (i = 0; i < l1.Num(); i++) {
            p = (PHVXYZ) l1.Get(i);
            p->p.MultiplyLeft (&mInv);
         }
         for (i = 0; i < l2.Num(); i++) {
            p = (PHVXYZ) l2.Get(i);
            p->p.MultiplyLeft (&mInv);
         }


         // zipepr it
         m_Renderrs.ShapeZipper ((PHVXYZ)l1.Get(0), l1.Num(), (PHVXYZ)l2.Get(0), l2.Num(),
            TRUE, 0, (m_pDoorFrame->m_dwDoor == DFD_DOOR) ? .999 : 1.0,
            m_pDoorFrame->m_dwDoor == DFD_DOOR);  // use .999 to ensure it doesn't draw the base

      }
   }
drawit:
   // skip over drawing zipper

   // draw the doors
   CalcOpeningsIfNecessary ();

   // loop through all the openings and draw
   DWORD i, dwColor, dwBits;
   for (dwBits = 1, dwColor=DOORCOLORSTART; dwBits < DSURF_COLORSMAX; dwColor++, dwBits = dwBits << 1) {
      PCObjectSurface pos = ObjectSurfaceFind (dwColor);
      if (!pos)
         continue;   // must not be valid

      m_Renderrs.SetDefMaterial (m_OSINFO.dwRenderShard, pos, m_pWorld);

      for (i = 0; i < m_lOpenings.Num(); i++) {
         PCDoorOpening pdo = *((PCDoorOpening*) m_lOpenings.Get(i));
         pdo->Render(pr, &m_Renderrs, dwBits, m_fOpened);
      }
   }

   m_Renderrs.Commit();
}


// NOTE: Not doing QueryBoundingBox() for doors since fairly complex piece
// of code and likely to introduce bugs

/**********************************************************************************
CObjectDoor::Clone - Clones this

inputs
   none
returns
   PCObjectSocket - Clone
*/
PCObjectSocket CObjectDoor::Clone (void)
{
   PCObjectDoor pNew;

   pNew = new CObjectDoor(NULL, &m_OSINFO);
   if (!pNew)
      return NULL;
   // free openings
   DWORD i;
   for (i = 0; i < pNew->m_lOpenings.Num(); i++) {
      PCDoorOpening pdo = *((PCDoorOpening*) pNew->m_lOpenings.Get(i));
      delete pdo;
   }
   pNew->m_lOpenings.Clear();

   // clone template info
   CloneTemplate(pNew);

   m_pDoorFrame->CloneTo (pNew->m_pDoorFrame);
   pNew->m_fAutoDepth = m_fAutoDepth;
   pNew->m_fAutoAdjust = m_fAutoAdjust;
   pNew->m_dwBevelSide = m_dwBevelSide;
   pNew->m_pBevel.Copy (&m_pBevel);
   pNew->m_fDirty = m_fDirty;
   pNew->m_fOpened = m_fOpened;

   // openings
   pNew->m_lOpenings.Init (sizeof(PCDoorOpening), m_lOpenings.Get(0), m_lOpenings.Num());
   for (i = 0; i < pNew->m_lOpenings.Num(); i++) {
      PCDoorOpening *ppdo = (PCDoorOpening*) pNew->m_lOpenings.Get(i);
      (*ppdo) = (*ppdo)->Clone();
   }

   return pNew;
}



static PWSTR gpszDoorFrame = L"DoorFrame";
static PWSTR gpszAutoDepth = L"AutoDepth";
static PWSTR gpszAutoAdjust = L"AutoAdjust";
static PWSTR gpszBevelSide = L"BevelSide";
static PWSTR gpszBevel = L"Bevel";
static PWSTR gpszOpenings = L"Openings";
static PWSTR gpszOpened = L"Opened";

PCMMLNode2 CObjectDoor::MMLTo (void)
{
   // call into the template first
   PCMMLNode2 pNode = MMLToTemplate();
   if (!pNode)
      return NULL;

   PCMMLNode2 pSub;
   pSub = m_pDoorFrame->MMLTo ();
   if (pSub) {
      pSub->NameSet (gpszDoorFrame);
      pNode->ContentAdd (pSub);
   }

   // write out the openings
   DWORD i;
   for (i = 0; i < m_lOpenings.Num(); i++) {
      PCDoorOpening pdo = *((PCDoorOpening*) m_lOpenings.Get(i));
      pSub = pdo->MMLTo();
      if (pSub) {
         pSub->NameSet (gpszOpenings);
         pNode->ContentAdd (pSub);
      }
   }

   MMLValueSet (pNode, gpszAutoDepth, (int) m_fAutoDepth);
   MMLValueSet (pNode, gpszAutoAdjust, m_fAutoAdjust);
   MMLValueSet (pNode, gpszBevelSide, (int) m_dwBevelSide);
   MMLValueSet (pNode, gpszBevel, &m_pBevel);
   MMLValueSet (pNode, gpszOpened, m_fOpened);
   // dont write out m_fDirty

   return pNode;
}

BOOL CObjectDoor::MMLFrom (PCMMLNode2 pNode)
{
   // call into the template first
   if (!MMLFromTemplate (pNode))
      return FALSE;

   // free openings
   DWORD i;
   for (i = 0; i < m_lOpenings.Num(); i++) {
      PCDoorOpening pdo = *((PCDoorOpening*) m_lOpenings.Get(i));
      delete pdo;
   }
   m_lOpenings.Clear();

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

      if (!_wcsicmp(psz, gpszDoorFrame))
         m_pDoorFrame->MMLFrom (pSub);
      else if (!_wcsicmp(psz, gpszOpenings)) {
         PCDoorOpening pNew = new CDoorOpening;
         if (!pNew)
            continue;
         pNew->MMLFrom (pSub);
         m_lOpenings.Add (&pNew);
      }
   }

   m_fEmbedFitToFloor = (m_pDoorFrame->m_dwDoor == DFD_DOOR);
   switch (m_pDoorFrame->m_dwDoor) {
   case DFD_DOOR:
   default:
      m_dwRenderShow = RENDERSHOW_DOORS;
      break;
   case DFD_WINDOW:
      m_dwRenderShow = RENDERSHOW_WINDOWS;
      break;
   case DFD_CABINET:
      m_dwRenderShow = RENDERSHOW_CABINETS;
      break;
   }

   m_fAutoDepth = (BOOL) MMLValueGetInt (pNode, gpszAutoDepth, 0);
   m_fAutoAdjust = MMLValueGetDouble (pNode, gpszAutoAdjust, 0);
   m_dwBevelSide = (DWORD) MMLValueGetInt (pNode, gpszBevelSide, 0);
   m_fOpened = MMLValueGetDouble (pNode, gpszOpened, 0);
   CPoint pZero;
   pZero.Zero();
   MMLValueGetPoint (pNode, gpszBevel, &m_pBevel, &pZero);

   // assume dirty
   m_fDirty = TRUE;

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
BOOL CObjectDoor::MoveReferencePointQuery (DWORD dwIndex, PCPoint pp)
{
   PWINDOWMOVEP pwp;

   if (m_pDoorFrame->m_dwDoor != DFD_WINDOW) {
      if (dwIndex >= sizeof(gaDoorMoveP) / sizeof(WINDOWMOVEP))
         return FALSE;
      pwp = gaDoorMoveP;
   }
   else {
      if (dwIndex >= sizeof(gaWindowMoveP) / sizeof(WINDOWMOVEP))
         return FALSE;
      pwp = gaWindowMoveP;
   }

   // figure out the width and height
   CPoint pMin, pMax;
   DWORD i;
   PCSpline pOut;
   pMin.Zero();
   pMax.Zero();
   pOut = m_pDoorFrame->OutsideGet ();
   if (pOut) {
      for (i = 0; i < pOut->OrigNumPointsGet(); i++) {
         CPoint p;
         pOut->OrigPointGet (i, &p);
         if (i) {
            pMin.Min (&p);
            pMax.Max (&p);
         }
         else {
            pMin.Copy (&p);
            pMax.Copy (&p);
         }
      }
   }

   fp fWidth, fHeight;
   fWidth = pMax.p[0] - pMin.p[0];
   fHeight = pMax.p[2] - pMin.p[2];

   pp->Zero();
   pp->p[0] = pMin.p[0] * (1.0 - pwp[dwIndex].iX) / 2.0 + pMax.p[0] * (pwp[dwIndex].iX + 1.0) / 2.0;
   pp->p[2] = pMin.p[2] * (1.0 - pwp[dwIndex].iY) / 2.0 + pMax.p[2] * (pwp[dwIndex].iY + 1.0) / 2.0;

   return TRUE;
}

/**************************************************************************************
CObjectDoor::MoveReferenceStringQuery -
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
BOOL CObjectDoor::MoveReferenceStringQuery (DWORD dwIndex, PWSTR psz, DWORD dwSize, DWORD *pdwNeeded)
{
   PWINDOWMOVEP pwp;

   if (m_pDoorFrame->m_dwDoor != DFD_WINDOW) {
      if (dwIndex >= sizeof(gaDoorMoveP) / sizeof(WINDOWMOVEP)) {
         if (pdwNeeded)
            *pdwNeeded = 0;
         return FALSE;
      }
      pwp = gaDoorMoveP;
   }
   else {
      if (dwIndex >= sizeof(gaWindowMoveP) / sizeof(WINDOWMOVEP)) {
         if (pdwNeeded)
            *pdwNeeded = 0;
         return FALSE;
      }
      pwp = gaWindowMoveP;
   }

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
CObjectDoor::EmbedDoCutout - Member function specific to the template. Called
when the object has moved within the surface. This enables the super-class for
the embedded object to pass a cutout into the container. (Basically, specify the
hole for the window or door)
*/
BOOL CObjectDoor::EmbedDoCutout (void)
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

   DWORD dwNum;
   PCSpline ps;
   ps = m_pDoorFrame->OutsideGet();
   if (!ps)
      return FALSE;
   dwNum = ps->QueryNodes();
   CMem memFront, memBack;
   if (!memFront.Required(dwNum * sizeof(CPoint)) || !memBack.Required(dwNum * sizeof(CPoint)))
      return FALSE;
   PCPoint pFront, pBack;
   pFront = (PCPoint) memFront.p;
   pBack = (PCPoint) memBack.p;

   DWORD i;
   for (i = 0; i < dwNum; i++) {
      pFront[i].Copy (ps->LocationGet (i));
      pFront[i].p[3] = 1;

      pBack[i].Copy (&pFront[i]);
      pBack[i].p[1] = m_fContainerDepth + CLOSE;
      pBack[i].p[3] = 1;
   }

   // bevel
   BOOL fThisOutside;
   fThisOutside = (m_dwContSideInfoThis == 1);
   fp fWidth, fDepth, fWant;
   m_pDoorFrame->FrameSizeGet (&fWidth, &fDepth);
   if (m_fAutoDepth && (m_fContainerDepth > EPSILON)) {
      fWant = m_fContainerDepth + m_fAutoAdjust;
      fWant = max(0,fWant);
      fDepth = fWant;
   }
   if ((m_dwBevelSide != 1) && (fDepth < m_fContainerDepth)) {
      // find the min and max
      CPoint pMin, pMax;
      for (i = 0; i < dwNum; i++) {
         if (i) {
            pMin.Min (&pFront[i]);
            pMax.Max (&pFront[i]);
         }
         else {
            pMin.Copy (&pFront[i]);
            pMax.Copy (&pFront[i]);
         }
      }

      // center?
      CPoint pCenter;
      pCenter.Average (&pMin, &pMax);

      // which side is bevelled
      BOOL fMoveFront;
      fMoveFront = (m_dwBevelSide == 2);   // assuming front is outside
      if (!fThisOutside)
         fMoveFront = !fMoveFront;

      // loop
      PCPoint pUse;
      fp fRight, fLeft, fTop, fBottom;
      pUse = fMoveFront ? pFront : pBack;
      fRight = tan(m_pBevel.p[0]) * m_fContainerDepth;
      fLeft = -tan(m_pBevel.p[0]) * m_fContainerDepth;
      fTop = tan(m_pBevel.p[1]) * m_fContainerDepth;
      fBottom = -tan(m_pBevel.p[2]) * m_fContainerDepth;

      for (i = 0; i < dwNum; i++) {
         fp fScaleX, fScaleZ;
         fScaleX = (pUse[i].p[0] - pMin.p[0]) / max(CLOSE, pMax.p[0] - pMin.p[0]) * 2 - 1;
         fScaleZ = (pUse[i].p[2] - pMin.p[2]) / max(CLOSE, pMax.p[2] - pMin.p[2]) * 2 - 1;

         if (pUse[i].p[0] < pCenter.p[0])
            pUse[i].p[0] -= fLeft * fScaleX;
         else
            pUse[i].p[0] += fRight * fScaleX;
         if (pUse[i].p[2]  < pCenter.p[2])
            pUse[i].p[2] -= fBottom * fScaleZ;
         else
            pUse[i].p[2] += fTop * fScaleZ;
      }
   }



   // convert to object's space
   for (i = 0; i < dwNum; i++) {
      pFront[i].MultiplyLeft (&mTrans);
      pBack[i].MultiplyLeft (&mTrans);
   }
   pos->ContCutout (&m_gGUID, dwNum, pFront, pBack, TRUE);

   return TRUE;
}


/**********************************************************************************
CObjectDoor::DialogQuery - Returns TRUE if the object supports a custom
dialog box, FALSE if it doesn't. The default object doesn't
*/
BOOL CObjectDoor::DialogQuery (void)
{
   return TRUE;
}


/* DoorDialogPage
*/
BOOL DoorOpeningPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PODD podd = (PODD) pPage->m_pUserData;
   PCObjectDoor pv = podd->pv;
   PCDoorOpening pdo = podd->pdo;
   static BOOL sfIgnoreMessage = FALSE;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         PCEscControl pControl;

         sfIgnoreMessage = TRUE;
         // set openings base
         pControl = pPage->ControlFind (L"openingsbase");
         if (pControl)
            pControl->AttribSetBOOL (Checked(), !pv->m_pDoorFrame->OpeningsBaseGet(podd->dwOpening));

         // set parameters
         DWORD i;
         WCHAR szTemp[32], szTemp2[32];
         ESCMCOMBOBOXSELECTSTRING css;
         memset (&css, 0, sizeof(css));
         css.fExact = TRUE;
         css.iStart = -1;
         css.psz = szTemp2;
         for (i = 0; i < 6; i++) {
            PCDoorSet pds = pdo->DoorSetGet (i);
            if (!pds)
               continue;

            BOOL fFixed;
            fp fDivisions;
            DWORD dwOrient, dwMovement;
            pds->DivisionsGet (&fFixed, &fDivisions, &dwOrient, &dwMovement);

            swprintf (szTemp, L"orient%d", (int) i);
            pControl = pPage->ControlFind (szTemp);
            if (pControl)
               pControl->AttribSetInt (CurSel(), (int) dwOrient);
            
            swprintf (szTemp, fFixed ? L"fixed%d" : L"maxsi%d", (int) i);
            pControl = pPage->ControlFind (szTemp);
            if (pControl)
               pControl->AttribSetBOOL (Checked(), TRUE);
            swprintf (szTemp, L"divisions%d", (int)i);
            if (fFixed)
               DoubleToControl (pPage, szTemp, fDivisions);
            else
               MeasureToString (pPage, szTemp, fDivisions);

            swprintf (szTemp, L"movement%d", (int)i);
            pControl = pPage->ControlFind (szTemp);
            if (pControl) {
               css.psz = szTemp2;
               swprintf (szTemp2, L"%d", (int) dwMovement);
               pControl->Message (ESCM_COMBOBOXSELECTSTRING, &css);
            }

            fp fThick;
            fThick = pds->ThicknessGet();
            swprintf (szTemp, L"thickness%d", (int)i);
            MeasureToString (pPage, szTemp, fThick);

            fp fMin, fMax;
            pds->OpenGet (&fMin, &fMax);
            swprintf (szTemp, L"opens%d", (int)i);
            pControl = pPage->ControlFind (szTemp);
            DWORD dwOpens;
            dwOpens = 0;
            if (fMin > 0)
               dwOpens = 3;
            else if (fMax <= .25)
               dwOpens = 2;
            else if (fMax <= .5)
               dwOpens = 1;
            if (pControl)
               pControl->AttribSetInt (CurSel(), (int) dwOpens);

            DWORD dwShape;
            fp fBottom, fTop;
            BOOL fAlternate;
            pds->CustomShapeGet (&dwShape, &fBottom, &fTop, &fAlternate);
            swprintf (szTemp, L"customuse%d", (int) i);
            pControl = pPage->ControlFind (szTemp);
            if (pControl)
               pControl->AttribSetBOOL (Checked(), dwShape ? TRUE : FALSE);
            swprintf (szTemp, L"customshaoe%d", (int)i);
            pControl = pPage->ControlFind (szTemp);
            if (pControl) {
               css.psz = szTemp2;
               swprintf (szTemp2, L"%d", (int) (dwShape ? dwShape : 1));
               pControl->Message (ESCM_COMBOBOXSELECTSTRING, &css);
            }
            swprintf (szTemp, L"customalt%d", (int) i);
            pControl = pPage->ControlFind (szTemp);
            if (pControl)
               pControl->AttribSetBOOL (Checked(), fAlternate);
            swprintf (szTemp, L"custombot%d", (int) i);
            DoubleToControl (pPage, szTemp, fBottom * 100);
            swprintf (szTemp, L"customtop%d", (int) i);
            DoubleToControl (pPage, szTemp, fTop * 100);

            // extend?
            if ((i == 0) || (i == 5)) {
               BOOL fOutside = (i == 0);
               fp af[4];
               DWORD j;
               pdo->ExtendGet (fOutside, &af[0], &af[1], &af[2], &af[3]);
               for (j = 0; j < 4; j++) {
                  swprintf (szTemp, L"extend%d%d", (int) i, (int) j);
                  MeasureToString (pPage, szTemp, af[j]);
               }
            }
         }

         sfIgnoreMessage = FALSE;
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

         if (!wcsncmp (psz, L"thickness", 9)) {
            PCDoorSet pds;
            pds = pdo->DoorSetGet (_wtoi(psz+9));
            if (pds) {
               pv->m_pWorld->ObjectAboutToChange(pv);
               fp fThick;
               fThick = pds->ThicknessGet();
               MeasureParseString (pPage, psz, &fThick);
               pds->ThicknessSet (fThick);
               pv->m_pWorld->ObjectChanged(pv);
            }
         }
         else if (!wcsncmp(psz, L"extend", 6)) {
            DWORD dwID = _wtoi(psz+6);;
            BOOL fOutside = ((dwID / 10) == 0);
            dwID = dwID % 10;
            dwID = min(4,dwID);
            fp af[4];
            pdo->ExtendGet (fOutside, &af[0], &af[1], &af[2], &af[3]);

            pv->m_pWorld->ObjectAboutToChange(pv);
            MeasureParseString (pPage, psz, &af[dwID]);
            //if (p->pControl->AttribGetBOOL (Checked())) {
            //   fp fWidth, fDepth;
            //   pv->m_pDoorFrame->FrameSizeGet (&fWidth, &fDepth);
            //   fLeft = fRight = fTop = fWidth;
            //   if (pv->m_pDoorFrame->OpeningsBaseGet(podd->dwOpening))
            //      fBottom = fWidth;
            //}
            pdo->ExtendSet (fOutside, af[0], af[1], af[2], af[3]);
            pv->m_pWorld->ObjectChanged(pv);
         }
         else if (!wcsncmp (psz, L"custombot", 9)) {
            PCDoorSet pds;
            pds = pdo->DoorSetGet (_wtoi(psz+9));
            if (pds) {
               pv->m_pWorld->ObjectAboutToChange(pv);
               DWORD dwShape;
               fp fBottom, fTop;
               BOOL fAlt;
               pds->CustomShapeGet (&dwShape, &fBottom, &fTop, &fAlt);
               pds->CustomShapeSet (dwShape, DoubleFromControl(pPage, p->pControl->m_pszName) / 100.0, fTop, fAlt);
               pv->m_pWorld->ObjectChanged(pv);
            }
         }
         else if (!wcsncmp (psz, L"customtop", 9)) {
            PCDoorSet pds;
            pds = pdo->DoorSetGet (_wtoi(psz+9));
            if (pds) {
               pv->m_pWorld->ObjectAboutToChange(pv);
               DWORD dwShape;
               fp fBottom, fTop;
               BOOL fAlt;
               pds->CustomShapeGet (&dwShape, &fBottom, &fTop, &fAlt);
               pds->CustomShapeSet (dwShape, fBottom, DoubleFromControl(pPage, p->pControl->m_pszName) / 100.0, fAlt);
               pv->m_pWorld->ObjectChanged(pv);
            }
         }
         else if (!wcsncmp (psz, L"divisions", 9)) {
            PCDoorSet pds;
            pds = pdo->DoorSetGet (_wtoi(psz+9));
            if (pds) {
               pv->m_pWorld->ObjectAboutToChange(pv);
               BOOL fFixed;
               fp fDivisions;
               DWORD dwOrient, dwMovement;
               pds->DivisionsGet (&fFixed, &fDivisions, &dwOrient, &dwMovement);
               if (fFixed)
                  fDivisions = DoubleFromControl (pPage, psz);
               else
                  MeasureParseString (pPage, psz, &fDivisions);
               pds->DivisionsSet (fFixed, fDivisions, dwOrient, dwMovement);
               pv->m_pWorld->ObjectChanged(pv);
            }
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

         if (!_wcsicmp(psz, L"openingsbase")) {
            pv->m_pWorld->ObjectAboutToChange(pv);
            pv->m_pDoorFrame->OpeningsBaseSet (podd->dwOpening,
               p->pControl->AttribGetBOOL(Checked()) ? 0 : 1);
            pv->m_fDirty = TRUE;
            pv->m_pWorld->ObjectChanged(pv);
            pPage->Exit (RedoSamePage());
            return TRUE;
         }
         else if (!wcsncmp(psz, L"remove", 6)) {
            pv->m_pWorld->ObjectAboutToChange(pv);
            pdo->DoorSetRemove (_wtoi(psz+6));
            pv->m_pWorld->ObjectChanged(pv);
            pPage->Exit (RedoSamePage());
         }
         else if (!wcsncmp(psz, L"adddoorset", 10)) {
            pv->m_pWorld->ObjectAboutToChange(pv);
            pdo->DoorSetAdd (_wtoi(psz+10), LOWORD(pv->m_dwType));
            pv->m_fDirty = TRUE;
            pv->m_pWorld->ObjectChanged(pv);
            pPage->Exit (RedoSamePage());
         }
         else if (!wcsncmp(psz, L"flip", 4)) {
            DWORD dwNum = _wtoi(psz+4);
            pv->m_pWorld->ObjectAboutToChange(pv);
            pdo->DoorSetFlip (dwNum);
            if ((dwNum == 0) || (dwNum == 5)) {
               CPoint p1, p2;
               pdo->ExtendGet (FALSE, &p1.p[0], &p1.p[1], &p1.p[2], &p1.p[3]);
               pdo->ExtendGet (TRUE, &p2.p[0], &p2.p[1], &p2.p[2], &p2.p[3]);
               pdo->ExtendSet (TRUE, p1.p[0], p1.p[1], p1.p[2], p1.p[3]);
               pdo->ExtendSet (FALSE, p2.p[0], p2.p[1], p2.p[2], p2.p[3]);
            }
            pv->m_fDirty = TRUE;
            pv->m_pWorld->ObjectChanged(pv);
            pPage->Exit (RedoSamePage());
         }
         else if (!wcsncmp(psz, L"fixed", 5) || !wcsncmp(psz, L"maxsi", 5)) {
            PCDoorSet pds;
            DWORD dwNum;
            pds = pdo->DoorSetGet (dwNum = (DWORD) _wtoi(psz+5));
            if (pds) {
               pv->m_pWorld->ObjectAboutToChange(pv);
               BOOL fFixed, fNew;
               fp fDivisions;
               DWORD dwOrient, dwMovement;
               pds->DivisionsGet (&fFixed, &fDivisions, &dwOrient, &dwMovement);

               PCEscControl pControl;
               WCHAR szTemp[32];
               swprintf (szTemp, L"fixed%d", (int) dwNum);
               pControl = pPage->ControlFind (szTemp);
               fNew = pControl ? pControl->AttribGetBOOL (Checked()) : FALSE;
               if (fFixed != fNew) {
                  fDivisions = 1;   // use 1 for either of them
                  pds->DivisionsSet (fNew, fDivisions, dwOrient, dwMovement);

                  // set the dividions edit field
                  swprintf (szTemp, L"divisions%d", (int) dwNum);
                  if (!fNew)
                     MeasureToString (pPage, szTemp, fDivisions);
                  else
                     DoubleToControl (pPage, szTemp, fDivisions);
               }
               pv->m_pWorld->ObjectChanged(pv);
            }
         }
         else if (!wcsncmp(psz, L"customuse", 9)) {
            PCDoorSet pds;
            DWORD dwNum;
            pds = pdo->DoorSetGet (dwNum = (DWORD) _wtoi(psz+9));
            if (pds) {
               pv->m_pWorld->ObjectAboutToChange(pv);

               // get current shape
               DWORD dwShape;
               fp fBottom, fTop;
               BOOL fAlt;
               pds->CustomShapeGet (&dwShape, &fBottom, &fTop, &fAlt);

               // get the dropdown settings
               PCEscControl pControl;
               WCHAR szTemp[32];
               dwShape = 0;
               swprintf (szTemp, L"customshape%d", (int) dwNum);
               pControl = pPage->ControlFind (szTemp);
               ESCMCOMBOBOXGETITEM gi;
               memset (&gi, 0, sizeof(gi));
               if (pControl) {
                  gi.dwIndex = pControl->AttribGetInt (CurSel());
                  pControl->Message (ESCM_COMBOBOXGETITEM, &gi);
                  if (gi.pszName)
                     dwShape = _wtoi(gi.pszName);
               }

               // get button settings
               if (!p->pControl->AttribGetBOOL (Checked()))
                  dwShape = 0;

               pds->CustomShapeSet (dwShape, fBottom, fTop, fAlt);
               pv->m_pWorld->ObjectChanged(pv);
            }
         }
         else if (!wcsncmp(psz, L"customalt", 9)) {
            PCDoorSet pds;
            DWORD dwNum;
            pds = pdo->DoorSetGet (dwNum = (DWORD) _wtoi(psz+9));
            if (pds) {
               pv->m_pWorld->ObjectAboutToChange(pv);
               DWORD dwShape;
               fp fBottom, fTop;
               BOOL fAlt;
               pds->CustomShapeGet (&dwShape, &fBottom, &fTop, &fAlt);
               pds->CustomShapeSet (dwShape, fBottom, fTop, p->pControl->AttribGetBOOL(Checked()));
               pv->m_pWorld->ObjectChanged(pv);
            }
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

         if (!wcsncmp (psz, L"orient", 6)) {
            pv->m_pWorld->ObjectAboutToChange (pv);
            PCDoorSet pds;
            pds = pdo->DoorSetGet (_wtoi(psz+6));
            if (pds) {
               BOOL fFixed;
               fp fDivisions;
               DWORD dwOrient, dwMovement;
               pds->DivisionsGet (&fFixed, &fDivisions, &dwOrient, &dwMovement);
               if (p->dwCurSel != dwOrient)
                  pds->DivisionsSet (fFixed, fDivisions, p->dwCurSel, dwMovement);
            }
            pv->m_pWorld->ObjectChanged (pv);
            return TRUE;
         }
         else if (!wcsncmp (psz, L"customshape", 11) && !sfIgnoreMessage) {
            pv->m_pWorld->ObjectAboutToChange (pv);
            PCDoorSet pds;
            pds = pdo->DoorSetGet (_wtoi(psz+11));
            if (pds) {
               DWORD dwShape;
               fp fBottom, fTop;
               BOOL fAlt;
               pds->CustomShapeGet (&dwShape, &fBottom, &fTop, &fAlt);
               if (dwShape && p->pszName)
                  dwShape = _wtoi(p->pszName);
               pds->CustomShapeSet (dwShape, fBottom, fTop, fAlt);
            }
            pv->m_pWorld->ObjectChanged (pv);
            return TRUE;
         }
         else if (!wcsncmp (psz, L"opens", 5) && !sfIgnoreMessage) {
            pv->m_pWorld->ObjectAboutToChange (pv);
            PCDoorSet pds;
            pds = pdo->DoorSetGet (_wtoi(psz+5));
            if (pds) {
               fp fMin, fMax;
               fMin = 0;
               fMax = 1;
               switch (p->dwCurSel) {
               case 1:
                  fMax = .5;
                  break;
               case 2:
                  fMax = .25;
                  break;
               case 3:
                  fMin = 1;
                  break;
               }
               pds->OpenSet (fMin, fMax);
            }
            pv->m_pWorld->ObjectChanged (pv);
            return TRUE;
         }
         else if (!wcsncmp (psz, L"movement", 8)) {
            pv->m_pWorld->ObjectAboutToChange (pv);
            PCDoorSet pds;
            pds = pdo->DoorSetGet (_wtoi(psz+8));
            if (pds && p->pszName) {
               BOOL fFixed;
               fp fDivisions;
               DWORD dwOrient, dwMovement;
               pds->DivisionsGet (&fFixed, &fDivisions, &dwOrient, &dwMovement);
               if ((DWORD) _wtoi(p->pszName) != dwMovement)
                  pds->DivisionsSet (fFixed, fDivisions, dwOrient, _wtoi(p->pszName));
            }
            pv->m_pWorld->ObjectChanged (pv);
            return TRUE;
         }
      }
      return 0;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Door/window opening";
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"THEPAGE")) {
            MemZero (&gMemTemp);

            // openings
            DWORD i;
            PWSTR apszTitle[6] = {
               L"Outside (beyond frame)",
               L"Outer side of frame",
               L"Middle of frame (opening out)",
               L"Middle of frame (opening in)",
               L"Inner side of frame",
               L"Inside (beyond frame)"
            };
            for (i = 0; i < 6; i++) {
               PCDoorSet pds = pdo->DoorSetGet (i);
               if (!pds)
                  continue;

               // title
               MemCat (&gMemTemp, L"<xtablecenter width=100%%><xtrheader>");
               MemCatSanitize (&gMemTemp, apszTitle[i]);
               MemCat (&gMemTemp, L"</xtrheader>");

               // UI to flip
               MemCat (&gMemTemp, L"<tr><td><xchoicebutton name=flip");
               MemCat (&gMemTemp, (int) i);
               MemCat (&gMemTemp, L">"
                  L"<bold>Flip the door front-to-back</bold><br/>"
                  L"Moves the door to the other side of the frame."
                  L"</xchoicebutton></td></tr>");


               // UI for editing door
               MemCat (&gMemTemp, L"<tr><td><xchoicebutton href=custom");
               MemCat (&gMemTemp, (int) i);
               MemCat (&gMemTemp, L":999>"
                  L"<bold>Change the door/window's appearance</bold><br/>"
                  L"Lets you select the style and type of door/window used in the opening."
                  L"</xchoicebutton>");
               if (pds->DoorNum() > 1) {
                  MemCat (&gMemTemp, L"<align align=right>");
                  DWORD j;
                  for (j = 0; j < pds->DoorNum(); j++) {
                     MemCat (&gMemTemp, L"<a href=custom");
                     MemCat (&gMemTemp, (int) i);
                     MemCat (&gMemTemp, L":");
                     MemCat (&gMemTemp, (int) j);
                     MemCat (&gMemTemp, L">");
                     MemCat (&gMemTemp, (int)j+1);
                     MemCat (&gMemTemp, L"<xHoverHelpShort>Press this to modify one door of the set.</xHoverHelpShort></a> ");
                  }
                  MemCat (&gMemTemp, L"</align>");
               }
               MemCat (&gMemTemp, L"</td></tr>");

               // which direction it opens
               MemCat (&gMemTemp, L"<tr><td>"
                  L"<bold>Orientation</bold> - Select the direction which the "
                  L"doors/windows open in."
                  L"</td><td><xcomboorient name=orient");
               MemCat (&gMemTemp, (int) i);
               MemCat (&gMemTemp, L"/></td></tr>");

               // how its divided
               MemCat (&gMemTemp, L"<tr><td>");
               MemCat (&gMemTemp, L"Divide the opening into:<br/>");
               WCHAR szTemp[128];
               swprintf (szTemp, L"fixed%d,maxsi%d", (int) i, (int)i);
               MemCat (&gMemTemp, L"<xchoicebutton style=check radiobutton=true group=");
               MemCat (&gMemTemp, szTemp);
               MemCat (&gMemTemp, L" name=fixed");
               MemCat (&gMemTemp, (int) i);
               MemCat (&gMemTemp, L"><bold>Preset number of doors/windows</bold><br/>"
                  L"Type the number of doors/windows to the right.</xchoicebutton>");
               MemCat (&gMemTemp, L"<xchoicebutton style=check radiobutton=true group=");
               MemCat (&gMemTemp, szTemp);
               MemCat (&gMemTemp, L" name=maxsi");
               MemCat (&gMemTemp, (int) i);
               MemCat (&gMemTemp, L"><bold>Variable number of doors/windows</bold><br/>"
                  L"Type the minimum door/window width on the right.</xchoicebutton>");
               MemCat (&gMemTemp, L"</td><td><edit maxchars=32 width=100%% name=divisions");
               MemCat (&gMemTemp, (int) i);
               MemCat (&gMemTemp, L"/></td></tr>");

               // how it moves
               MemCat (&gMemTemp, L"<tr><td>"
                  L"<bold>Movement</bold> - How the doors are hinged and move in "
                  L"the frame."
                  L"</td><td><xcombomove name=movement");
               MemCat (&gMemTemp, (int) i);
               MemCat (&gMemTemp, L"/></td></tr>");

               // thickness
               MemCat (&gMemTemp, L"<tr><td>"
                  L"<bold>Door/window frame thickness</bold> - How thick "
                  L"the doors/window is."
                  L"</td><td><edit maxchars=32 width=100%% name=thickness");
               MemCat (&gMemTemp, (int) i);
               MemCat (&gMemTemp, L"/></td></tr>");

               // how far it opens
               MemCat (&gMemTemp, L"<tr><td>"
                  L"<bold>Freedom of movement</bold> - How far the door/window can open."
                  L"</td><td><xcomboopens name=opens");
               MemCat (&gMemTemp, (int) i);
               MemCat (&gMemTemp, L"/></td></tr>");

               // extend the door
               if ((i == 0) || (i == 5)) {
                  MemCat (&gMemTemp, L"<tr><td>"
                     L"<bold>Extend the door/window</bold> - Doors/windows are usually "
                     L"limited to the frame size. However, if you enter non-zero "
                     L"numbers to the right, the door will be extended beyond the frame's "
                     L"opening."
                     L"</td><td align=right>");
                  MemCat (&gMemTemp, L"L: <edit maxchars=32 width=80%% name=extend");
                  MemCat (&gMemTemp, (int) i);
                  MemCat (&gMemTemp, L"0/><br/>");
                  MemCat (&gMemTemp, L"R: <edit maxchars=32 width=80%% name=extend");
                  MemCat (&gMemTemp, (int) i);
                  MemCat (&gMemTemp, L"1/><br/>");
                  MemCat (&gMemTemp, L"T: <edit maxchars=32 width=80%% name=extend");
                  MemCat (&gMemTemp, (int) i);
                  MemCat (&gMemTemp, L"2/><br/>");
                  MemCat (&gMemTemp, L"B: <edit maxchars=32 width=80%% name=extend");
                  MemCat (&gMemTemp, (int) i);
                  MemCat (&gMemTemp, L"3/>");
                  MemCat (&gMemTemp, L"</td></tr>");
               }

               // custom shape
               MemCat (&gMemTemp, L"<tr><td>");
               MemCat (&gMemTemp, L"<xchoicebutton checkbox=true style=x name=customuse");
               MemCat (&gMemTemp, (int) i);
               MemCat (&gMemTemp, L">"
                  L"<bold>Door height is less than the frame height</bold><br/>"
                  L"If checked, then the top and bottom or the doors don't occupy "
                  L"the entire frame height. Use this for \"Saloon\" doors.</xchoicebutton>");
               MemCat (&gMemTemp, L"<xtablecenter width=100%%>");
               MemCat (&gMemTemp, L"<xtrheader>Partial-height doors</xtrheader>");
               MemCat (&gMemTemp, L"<tr><td>"
                  L"<bold>Shape</bold>"
                  L"</td><td><xcombocustom name=customshape");
               MemCat (&gMemTemp, (int) i);
               MemCat (&gMemTemp, L"/></td></tr>");

               MemCat (&gMemTemp, L"<tr><td>"
                  L"<bold>Bottom</bold> - Bottom of the doors as a percentage of "
                  L"the possible height. (Leave out the percent sign.)"
                  L"</td><td><edit maxchars=32 width=100%% name=custombot");
               MemCat (&gMemTemp, (int) i);
               MemCat (&gMemTemp, L"/></td></tr>");
               MemCat (&gMemTemp, L"<tr><td>"
                  L"<bold>Top</bold> - Top of the doors as a percentage of "
                  L"the possible height. (Leave out the percent sign.)"
                  L"</td><td><edit maxchars=32 width=100%% name=customtop");
               MemCat (&gMemTemp, (int) i);
               MemCat (&gMemTemp, L"/></td></tr>");

               MemCat (&gMemTemp, L"<tr><td>");
               MemCat (&gMemTemp, L"<xchoicebutton checkbox=true style=x name=customalt");
               MemCat (&gMemTemp, (int) i);
               MemCat (&gMemTemp, L">"
                  L"<bold>Alternate</bold><br/>"
                  L"For asymetric shapes like \"Saloon\" this mirrors every other "
                  L"door.</xchoicebutton>");
               MemCat (&gMemTemp, L"</td></tr>");

               MemCat (&gMemTemp, L"</xtablecenter>");
               MemCat (&gMemTemp, L"</td></tr>");

               // allow to change doorknob
               MemCat (&gMemTemp, L"<tr><td><xchoicebutton href=doorknob");
               MemCat (&gMemTemp, (int) i);
               MemCat (&gMemTemp, L">"
                  L"<bold>Change the doorknob or handle</bold><br/>"
                  L"Brings up a dialog that lets you specify what kind of doorknob to use."
                  L"</xchoicebutton></td></tr>");

               // UI to remove
               MemCat (&gMemTemp, L"<tr><td><xchoicebutton name=remove");
               MemCat (&gMemTemp, (int) i);
               MemCat (&gMemTemp, L">"
                  L"<bold>Remove this door/window set</bold><br/>"
                  L"Deletes this door/window set. The frame and any other door/window sets are untouched."
                  L"</xchoicebutton></td></tr>");

               // end table
               MemCat (&gMemTemp, L"</xtablecenter>");
            }

            // if can have the option of having a bottom frame on this ask here
            if (pv->m_pDoorFrame->OpeningsBaseGet(podd->dwOpening) < 2) {
               MemCat (&gMemTemp, L"<xchoicebutton checkbox=true style=x name=openingsbase>"
                  L"<bold>This is a door</bold><br/>"
                  L"If checked, the opening extends all the way to the ground. "
                  L"Otherwise, the frame runs beneath it.</xchoicebutton>");
            }

            // buttons to add openings
            BOOL  fAddOption;
            fAddOption = FALSE;
            for (i = 0; i < 6; i++) {
               PCDoorSet pds = pdo->DoorSetGet (i);
               if (pds)
                  continue;

               if (!fAddOption) {
                  fAddOption = TRUE;
                  MemCat (&gMemTemp, L"<xbr/><p>"
                     L"Use the following button to add door/window sets to the frame. "
                     L"Having multiple door/window sets allows you to have a solid "
                     L"door on one the inside of the frame and a screen door on "
                     L"the outside of the frame. (Same with windows.)"
                     L"</p>");
               }

               MemCat (&gMemTemp, L"<xchoicebutton name=adddoorset");
               MemCat (&gMemTemp, (int) i);
               MemCat (&gMemTemp, L"><bold>Add a door/window set to the ");
               MemCatSanitize (&gMemTemp, apszTitle[i]);
               MemCat (&gMemTemp, L"</bold><br/>");
               switch (i) {
               case 0:
               case 5:
                  MemCat (&gMemTemp, L"The door set is placed beyond the end of the frame. "
                     L"Use this for sliding barn doors that slide on a rail attached to "
                     L"the frame, or for shutters.");
                  break;
               case 1:
               case 4:
                  MemCat (&gMemTemp, L"The door set's front is flush with the frame, like "
                     L"a normal hinged door.");
                  break;
               case 2:
               case 3:
                  MemCat (&gMemTemp, L"The door set is centered in the frame, such as "
                     L"a pocket door or bi-fold doors.");
                  break;
               }
               MemCat (&gMemTemp, L"</xchoicebutton>");
            }
            p->pszSubString = (PWSTR) gMemTemp.p;
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}

/* DoorDialogPage
*/
BOOL DoorDialogPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCObjectDoor pv = (PCObjectDoor) pPage->m_pUserData;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         GenerateThreeDFromDoorFrame (L"frameshape", pPage, pv->m_pDoorFrame);

         // fill in measurements
         DWORD i;
         PCDoorFrame pdf = pv->m_pDoorFrame;
         WCHAR szTemp[32];

         for (i = 0; i < pdf->MeasNum(); i++) {
            fp f;
            BOOL fDistance;
            PCWSTR psz;
            psz = pdf->MeasGet (i, &f, &fDistance);
            if (!psz)
               break;

            swprintf (szTemp, L"meas%d", (int) i);
            if (fDistance)
               MeasureToString (pPage, szTemp, f);
            else
               DoubleToControl (pPage, szTemp, f);
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

         DWORD i;
         PCDoorFrame pdf = pv->m_pDoorFrame;
         WCHAR szTemp[32];
         for (i = 0; i < pdf->MeasNum(); i++) {
            fp f;
            BOOL fDistance;
            PCWSTR psz;
            psz = pdf->MeasGet (i, &f, &fDistance);
            if (!psz)
               break;

            swprintf (szTemp, L"meas%d", (int) i);
            if (fDistance)
               MeasureParseString (pPage, szTemp, &f);
            else
               f = DoubleFromControl (pPage, szTemp);

            // set it
            pdf->MeasSet (i, f);
            pv->m_fDirty = TRUE; // will need to redo openings
         }

         if (pv->m_pWorld)
            pv->m_pWorld->ObjectChanged (pv);

         pv->EmbedDoCutout();

         // Update the display
         GenerateThreeDFromDoorFrame (L"frameshape", pPage, pv->m_pDoorFrame);
      }
      break;

   case ESCN_THREEDCLICK:
      {
         PESCNTHREEDCLICK p = (PESCNTHREEDCLICK) pParam;
         if (!p->pControl->m_pszName)
            break;
         if (!(p->dwMajor & 0x10000))
            break;
         DWORD dwID;
         dwID = LOWORD(p->dwMajor);
         if (dwID < pv->m_pDoorFrame->OpeningsNum()) {
            WCHAR szTemp[16];
            swprintf (szTemp, L"opening%d", dwID);
            pPage->Exit(szTemp);
            return TRUE;
         }
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Door/window settings";
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"FRAMEMEAS")) {
            MemZero (&gMemTemp);
            DWORD i;
            PCDoorFrame pdf = pv->m_pDoorFrame;

            for (i = 0; i < pdf->MeasNum(); i++) {
               fp f;
               BOOL fDistance;
               PCWSTR psz;
               psz = pdf->MeasGet (i, &f, &fDistance);
               if (!psz)
                  break;

               MemCat (&gMemTemp, L"<tr><td>");
               MemCat (&gMemTemp, (PWSTR) psz);   // NOTE: NOT santiizing
               MemCat (&gMemTemp, L"</td><td><edit maxchars=32 width=100%% name=meas");
               MemCat (&gMemTemp, (int) i);
               MemCat (&gMemTemp, L"/></td></tr>");
            }

            p->pszSubString = (PWSTR) gMemTemp.p;
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}




/* DoorFrameWDPage
*/
BOOL DoorFrameWDPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCObjectDoor pv = (PCObjectDoor) pPage->m_pUserData;
   PCDoorFrame pdf = pv->m_pDoorFrame;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         CPoint pMain, pSub;
         pdf->FrameSizeGet (&pMain.p[0], &pMain.p[1]);
         pdf->SubFrameSizeGet (&pSub.p[0], &pSub.p[1]);
         MeasureToString (pPage, L"framing0", pMain.p[0]);
         MeasureToString (pPage, L"framing1", pMain.p[1]);
         MeasureToString (pPage, L"subframing0", pSub.p[0]);
         MeasureToString (pPage, L"subframing1", pSub.p[1]);

         // Autoinfo and bevelling
         PCEscControl pControl;
         pControl = pPage->ControlFind (L"autodepth");
         if (pControl)
            pControl->AttribSetBOOL (Checked(), pv->m_fAutoDepth);
         MeasureToString (pPage, L"autoadjust", pv->m_fAutoAdjust);
         pControl = pPage->ControlFind (L"side");
         if (pControl)
            pControl->AttribSetInt (CurSel(), (int) pv->m_dwBevelSide);
         AngleToControl (pPage, L"bevel0", pv->m_pBevel.p[0], TRUE);
         AngleToControl (pPage, L"bevel1", pv->m_pBevel.p[1], TRUE);
         AngleToControl (pPage, L"bevel2", pv->m_pBevel.p[2], TRUE);
      }
      break;
   case ESCN_COMBOBOXSELCHANGE:
      {
         PESCNCOMBOBOXSELCHANGE p = (PESCNCOMBOBOXSELCHANGE) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;

         if (_wcsicmp(p->pControl->m_pszName, L"side"))
            break;
         if (p->dwCurSel == pv->m_dwBevelSide)
            break; // no change

         // else change
         if (pv->m_pWorld)
            pv->m_pWorld->ObjectAboutToChange (pv);

         pv->m_dwBevelSide = p->dwCurSel;

         if (pv->m_pWorld)
            pv->m_pWorld->ObjectChanged (pv);

         pv->EmbedDoCutout();

         return TRUE;
      }
      break;


   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;

         if (!_wcsicmp(p->pControl->m_pszName, L"autodepth")) {
            if (pv->m_pWorld)
               pv->m_pWorld->ObjectAboutToChange (pv);

            pv->m_fAutoDepth = p->pControl->AttribGetBOOL (Checked());

            if (pv->m_pWorld)
               pv->m_pWorld->ObjectChanged (pv);

            // dont need to adjust cutout since that won't have changed
            return TRUE;
         }
      }
      break;   // default

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

         // get framing and sub
         fp fWidth, fDepth;
         MeasureParseString (pPage, L"framing0", &fWidth);
         fWidth = max(0, fWidth);
         MeasureParseString (pPage, L"framing1", &fDepth);
         fDepth = max(0, fDepth);
         pdf->FrameSizeSet (fWidth, fDepth);

         MeasureParseString (pPage, L"subframing0", &fWidth);
         fWidth = max(0, fWidth);
         MeasureParseString (pPage, L"subframing1", &fDepth);
         fDepth = max(0, fDepth);
         pdf->SubFrameSizeSet (fWidth, fDepth);

         MeasureParseString (pPage, L"autoadjust", &pv->m_fAutoAdjust);

         pv->m_pBevel.p[0] = AngleFromControl (pPage, L"bevel0");
         pv->m_pBevel.p[0] = max(pv->m_pBevel.p[0], -PI/2+CLOSE);
         pv->m_pBevel.p[0] = min(pv->m_pBevel.p[0], PI/2-CLOSE);

         pv->m_pBevel.p[1] = AngleFromControl (pPage, L"bevel1");
         pv->m_pBevel.p[1] = max(pv->m_pBevel.p[1], -PI/2+CLOSE);
         pv->m_pBevel.p[1] = min(pv->m_pBevel.p[1], PI/2-CLOSE);

         pv->m_pBevel.p[2] = AngleFromControl (pPage, L"bevel2");
         pv->m_pBevel.p[2] = max(pv->m_pBevel.p[2], -PI/2+CLOSE);
         pv->m_pBevel.p[2] = min(pv->m_pBevel.p[2], PI/2-CLOSE);

         pv->m_fDirty = TRUE; // will need to redo openings

         if (pv->m_pWorld)
            pv->m_pWorld->ObjectChanged (pv);

         pv->EmbedDoCutout();
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Frame width and depth";
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}


/**********************************************************************************
CObjectDoor::DialogShow - Causes the object to show a dialog box that allows
different attributes to be changed.

inputs
   DWORD             dwSuface - Surface that was clicked on. 0 if unknown.
   PCEscWindow       pWindow - Escarpment window to draw it in
returns
   BOOL - TRUE if user pressed "Back", FALSE if user pressed "Close"
*/
BOOL CObjectDoor::DialogShow (DWORD dwSurface, PCEscWindow pWindow, PCPoint pClick)
{
   PWSTR pszRet;
firstpage:
   pszRet = pWindow->PageDialog (ghInstance, IDR_MMLDOORDIALOG, DoorDialogPage, this);
firstpage2:
   if (!pszRet)
      return FALSE;
   if (!wcsncmp(pszRet, L"opening", 7)) {
      ODD odd;
      memset (&odd, 0, sizeof(odd));
      odd.pv = this;
      odd.dwOpening = _wtoi(pszRet + 7);
      CalcOpeningsIfNecessary();
      odd.pdo = *((PCDoorOpening*)m_lOpenings.Get(odd.dwOpening));

      PWSTR pszCustom = L"custom";
      DWORD dwLen = (DWORD)wcslen(pszCustom);
      PWSTR pszDoorKnob = L"doorknob";
      DWORD dwDoorKnobLen = (DWORD)wcslen(pszDoorKnob);
openingpage:
      pszRet = pWindow->PageDialog (ghInstance, IDR_MMLDOOROPENING, DoorOpeningPage, &odd);
      if (pszRet && !wcsncmp(pszRet, pszDoorKnob, dwDoorKnobLen)) {
         PCDoorSet pds = odd.pdo->DoorSetGet (_wtoi(pszRet+dwDoorKnobLen));
         if (!pds)
            goto openingpage;

         // else dialog
         BOOL fRet;
         fRet = pds->KnobDialog (pWindow, m_pWorld, this, this);
         if (fRet)
            goto openingpage;
         else
            return FALSE;
      }
      else if (pszRet && !wcsncmp(pszRet, pszCustom, dwLen)) {
         PCDoorSet pds = odd.pdo->DoorSetGet (_wtoi(pszRet+dwLen));
         PWSTR pszColon = wcschr (pszRet + dwLen, L':');
         if (!pds || !pszColon)
            goto openingpage;
         PCDoor pDoor;
         BOOL fAll;
         DWORD dwDoorNum;
         fAll = FALSE;
         dwDoorNum = _wtoi(pszColon+1);
         pDoor = pds->DoorGet (dwDoorNum);
         if (!pDoor) {
            fAll = TRUE;
            pDoor = pds->DoorGet(0);
            dwDoorNum = -1;
         }
         if (!pDoor)
            goto openingpage;

         // else dialog
         BOOL fRet;
         fRet = pDoor->CustomDialog (pWindow, m_pWorld, this, pds, dwDoorNum, this);
         delete pDoor;  // since was clone
         if (fRet)
            goto openingpage;
         else
            return FALSE;
      }
      else if (pszRet && !_wcsicmp(pszRet, Back()))
         goto firstpage;
      if (pszRet && !_wcsicmp(pszRet, RedoSamePage()))
         goto openingpage;
      else
         goto firstpage2;
   }
   else if (!_wcsicmp(pszRet, L"framing")) {
      int iRet = m_pDoorFrame->DialogShape (pWindow);
      if (iRet == -1)
         goto firstpage;
      else if (iRet < 0)
         return FALSE;

      // changed shape
      m_pWorld->ObjectAboutToChange (this);
      if (!m_pDoorFrame->Init ((DWORD)iRet, m_pDoorFrame->m_dwDoor, LOWORD(m_dwType)))
         m_pDoorFrame->Init (DFS_RECTANGLE, m_pDoorFrame->m_dwDoor, LOWORD(m_dwType));
      m_fDirty = TRUE; // will need to redo openings
      m_pWorld->ObjectChanged (this);

      EmbedDoCutout();
      goto firstpage;
   }
   else if (!_wcsicmp(pszRet, L"thick")) {
      pszRet = pWindow->PageDialog (ghInstance, IDR_MMLDOORFRAMEWD, DoorFrameWDPage, this);
      if (pszRet && !_wcsicmp(pszRet, Back()))
         goto firstpage;
      else
         goto firstpage2;
   }
   return !_wcsicmp(pszRet, Back());
}


/**********************************************************************************
CObjectDoor::CalcOpeningsIfNecessary - Recalcs the openings as necessary
*/
void CObjectDoor::CalcOpeningsIfNecessary (void)
{
   if (!m_fDirty)
      return;

   // figure out outisde and inside
   BOOL fOutside;
   fOutside = (m_dwContSideInfoThis == 1);

   // autoadjust the thicknes
   fp fWidth, fDepth;
   m_pDoorFrame->FrameSizeGet (&fWidth, &fDepth);

   // translate so centered in the wall
   fp fTrans;
   fp fYWallOutside, fYWallInside, fYFrameOutside, fYFrameInside;
   fTrans = m_fContainerDepth/2.0;  // asume larger
   if (fDepth < m_fContainerDepth) {
      switch (m_dwBevelSide) {
      case 0:  // outside
         fTrans = fDepth / 2;
         break;
      default:
      case 1:  // center
         // leave as is since already centered
         break;
      case 2:  // inside
         fTrans = m_fContainerDepth - fDepth/2;
         break;
      }
   }
   fYWallOutside = 0;
   fYWallInside = m_fContainerDepth;
   fYFrameOutside = fTrans - fDepth/2;
   fYFrameInside = fTrans + fDepth/2;
   if (!fOutside) {
      fYWallOutside = m_fContainerDepth - fYWallOutside;
      fYWallInside = m_fContainerDepth - fYWallInside;
      fYFrameOutside = m_fContainerDepth - fYFrameOutside;
      fYFrameInside = m_fContainerDepth - fYFrameInside;
   }

   // if there are too many openings remove them
   DWORD dwNum;
   dwNum = m_pDoorFrame->OpeningsNum();
   while (m_lOpenings.Num() > dwNum) {
      PCDoorOpening pdo = *((PCDoorOpening*) m_lOpenings.Get(dwNum));
      delete pdo;
      m_lOpenings.Remove (dwNum);
   }

   // add openings if there aren't any
   while (m_lOpenings.Num() < dwNum) {
      PCDoorOpening pNew;
      PCDoorOpening *ppdo;
      ppdo = (PCDoorOpening*) m_lOpenings.Get(m_lOpenings.Num()-1);
      pNew = NULL;
      // BUGFIX - Take out the clone so it fills in the side windows properly
      //if (ppdo && *ppdo)
      //   pNew = (*ppdo)->Clone();
      if (!pNew) {
         pNew = new CDoorOpening;
         if (!pNew)
            return;

         // depends upon whether this opening is a door and they particular style
         BOOL fShouldBeDoor, fTypeDoor;
         fShouldBeDoor = (m_pDoorFrame->OpeningsBaseGet (m_lOpenings.Num()) == 0);
         fTypeDoor = TRUE;
         switch (m_dwType & (DSSTYLE_WINDOW | DSSTYLE_CABINET)) {
         case DSSTYLE_WINDOW:
            fTypeDoor = FALSE;
            break;
         case DSSTYLE_CABINET:
            fTypeDoor = FALSE;
            break;
         }
         // old code before cabinets - fTypeDoor = !(m_dwType & DSSTYLE_WINDOW);

         DWORD dwStyle;
         dwStyle = LOWORD(m_dwType);
         // if it's a window and should be a door, or vice versa, then send down different style
         if (fShouldBeDoor != fTypeDoor) {
            if (fShouldBeDoor) {
               dwStyle = DSSTYLE_HINGEL | DS_DOORSOLID;
               if (m_dwType & DSSTYLE_CABINET)
                  dwStyle |= DSSTYLE_CABINET;
            }
            else
               dwStyle = DSSTYLE_WINDOW | DSSTYLE_FIXED | DS_WINDOWPLAIN1;
         }

         // Test for a 0-type style, because that would be a doorway
         if (LOWORD(dwStyle))
            pNew->StyleInit (dwStyle);
      }

      m_lOpenings.Add (&pNew);
   }

   // set the size
   DWORD i;
   for (i = 0; i < m_lOpenings.Num(); i++) {
      PCDoorOpening pdo = *((PCDoorOpening*) m_lOpenings.Get(i));
      PCSpline ps = m_pDoorFrame->OpeningsSpline (i);
      if (!ps)
         continue;

      pdo->ShapeSet (ps, fOutside, fYWallOutside, fYWallInside,
         fYFrameOutside, fYFrameInside);
   }

   // update the colors
   DWORD dwSurface;
   dwSurface = 0;
   for (i = 0; i < m_lOpenings.Num(); i++) {
      PCDoorOpening pdo = *((PCDoorOpening*) m_lOpenings.Get(i));
      dwSurface |= pdo->SurfaceQuery();
   }

   // if both internal then don't use external frame or panel
   BOOL fBothInternal;
   fBothInternal = (m_dwContSideInfoThis == 0) && (m_dwContSideInfoOther == 0);

   // create/remove
   for (i = 0; i < 32; i++) {
      DWORD dwBits = (1 << i);
      DWORD dwID = i + DOORCOLORSTART;
      if (dwBits > DSURF_COLORSMAX)
         continue;

      // delete if dont use
      if (!(dwSurface & dwBits) && ObjectSurfaceFind (dwID)) {
         ObjectSurfaceRemove (i + dwID);
         continue;
      }

      if ((dwSurface & dwBits) && !ObjectSurfaceFind (dwID)) {
         switch (dwBits) {
         case DSURF_GLASS:
            ObjectSurfaceAdd (dwID, RGB(0x00,0x40,0xff), MATERIAL_GLASSCLEAR, L"Glass");
            break;
         case DSURF_EXTFRAME:
            // BUGFIX - If totally inside then use all inside colors
            if (fBothInternal)
               ObjectSurfaceAdd (dwID, RGB(0x40,0x40,0xc0), MATERIAL_PAINTGLOSS,
                  (m_pDoorFrame->m_dwDoor == DFD_CABINET) ? L"Cabinet framing" :
                  ((m_pDoorFrame->m_dwDoor == DFD_DOOR) ? L"Door frame, internal" : L"Window frame, internal"),
                  &GTEXTURECODE_WoodTrim, &GTEXTURESUB_WoodTrim);
            else
               ObjectSurfaceAdd (dwID, RGB(0x00,0x00,0x80), MATERIAL_PAINTGLOSS,
                  (m_pDoorFrame->m_dwDoor == DFD_CABINET) ? L"Cabinet framing" :
                  ((m_pDoorFrame->m_dwDoor == DFD_DOOR) ? L"Door frame, external" : L"Window frame, external"),
                  &GTEXTURECODE_WoodTrim, &GTEXTURESUB_WoodTrim);
            break;
         case DSURF_INTFRAME:
            ObjectSurfaceAdd (dwID, RGB(0x40,0x40,0xc0), MATERIAL_PAINTGLOSS,
                  (m_pDoorFrame->m_dwDoor == DFD_CABINET) ? L"Cabinet framing" :
               ((m_pDoorFrame->m_dwDoor == DFD_DOOR) ? L"Door frame, internal" : L"Window frame, internal"),
               &GTEXTURECODE_WoodTrim, &GTEXTURESUB_WoodTrim);
            break;
         case DSURF_EXTPANEL:
            // BUGFIX - If totally inside then use all inside colors
            if (fBothInternal)
               ObjectSurfaceAdd (dwID, RGB(0x80,0x80,0xc0), MATERIAL_PAINTGLOSS,
                  (m_pDoorFrame->m_dwDoor == DFD_CABINET) ? L"Cabinet panel" :
                  ((m_pDoorFrame->m_dwDoor == DFD_DOOR) ? L"Door panel, internal" : L"Window panel, internal"),
                  &GTEXTURECODE_WoodTrim, &GTEXTURESUB_WoodTrim);
            else
               ObjectSurfaceAdd (dwID, RGB(0x40,0x40,0x80), MATERIAL_PAINTGLOSS,
                  (m_pDoorFrame->m_dwDoor == DFD_CABINET) ? L"Cabinet panel" :
                  ((m_pDoorFrame->m_dwDoor == DFD_DOOR) ? L"Door panel, external" : L"Window panel, external"),
                  &GTEXTURECODE_WoodTrim, &GTEXTURESUB_WoodTrim);
            break;
         case DSURF_INTPANEL:
            ObjectSurfaceAdd (dwID, RGB(0x80,0x80,0xc0), MATERIAL_PAINTGLOSS,
               (m_pDoorFrame->m_dwDoor == DFD_CABINET) ? L"Cabinet panel" :
               ((m_pDoorFrame->m_dwDoor == DFD_DOOR) ? L"Door panel, internal" : L"Window panel, internal"),
               &GTEXTURECODE_WoodTrim, &GTEXTURESUB_WoodTrim);
            break;
         case DSURF_SHUTTER:
            ObjectSurfaceAdd (dwID, RGB(0x40,0x40,0x40), MATERIAL_PAINTSEMIGLOSS,
               (m_pDoorFrame->m_dwDoor == DFD_CABINET) ? L"Cabinet shutter" : L"Window shutter");
            break;
         case DSURF_FLYSCREEN:
            ObjectSurfaceAdd (dwID, RGB(0,0,0), MATERIAL_FLYSCREEN, L"Flyscreen",
               NULL, NULL, NULL, NULL);   // BUGFIX - Was 0x40,0x40,0x40 for color
            break;
         case DSURF_LOUVERS:
            ObjectSurfaceAdd (dwID, RGB(0x40,0x40,0x40), MATERIAL_PAINTSEMIGLOSS, L"Window louvers",
               &GTEXTURECODE_WindowShutter, &GTEXTURESUB_WindowShutter);
            break;
         case DSURF_DOORKNOB:
            ObjectSurfaceAdd (dwID, RGB(0xc0,0xc0,0xc0), MATERIAL_METALSMOOTH,
               (m_pDoorFrame->m_dwDoor == DFD_CABINET) ? L"Cabinet handle" :L"Doorknob");
            break;
         case DSURF_BRACING:
            ObjectSurfaceAdd (dwID, RGB(0x40,0x40,0x80), MATERIAL_PAINTGLOSS,
               (m_pDoorFrame->m_dwDoor == DFD_CABINET) ? L"Cabinet bracing" :L"Door bracing",
               &GTEXTURECODE_WoodTrim, &GTEXTURESUB_WoodTrim);
            break;
         case DSURF_FROSTED:
            ObjectSurfaceAdd (dwID, RGB(0xff,0xff,0xff), MATERIAL_GLASSFROSTED, L"Glass, frosted");
            break;
         case DSURF_ALFRAME:
            ObjectSurfaceAdd (dwID, RGB(0x00,0x00,0x00), MATERIAL_PAINTGLOSS, L"Aluminium frame");
            break;
         }
      }
   }


   m_fDirty = FALSE;
}

/**********************************************************************************
CObjectDoor::OpenGet - 
returns how open the object is, from 0 (closed) to 1.0 (open), or
< 0 for an object that can't be opened
*/
fp CObjectDoor::OpenGet (void)
{
   return m_fOpened;
}

/**********************************************************************************
CObjectDoor::OpenSet - 
opens/closes the object. if fopen==0 it's close, 1.0 = open, and
values in between are partially opened closed. Returne TRUE if success
*/
BOOL CObjectDoor::OpenSet (fp fOpen)
{
   fOpen = max(0,fOpen);
   fOpen = min(1,fOpen);

   m_pWorld->ObjectAboutToChange (this);
   m_fOpened = fOpen;
   m_pWorld->ObjectChanged (this);

   return TRUE;
}

