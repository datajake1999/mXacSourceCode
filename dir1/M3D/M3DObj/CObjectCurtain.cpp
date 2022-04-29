/************************************************************************
CObjectCurtain.cpp - Draws a Curtain.

begun 6/9/02 by Mike Rozak
Copyright 2002 Mike Rozak. All rights reserved
*/

#include <windows.h>
#include "escarpment.h"
#include "..\M3D.h"
#include "resource.h"
#include <crtdbg.h>


typedef struct {
   PWSTR       pszName;    // name of the move point
   int         iX, iY; // -1 if for left/top, 1 for right/bottom
} SPLINEMOVEP, *PSPLINEMOVEP;

static SPLINEMOVEP   gaCurtainMove[] = {
   L"Center", 0, 0
};

#define RIPPLEDETAIL    8

/**********************************************************************************
CObjectCurtain::Constructor and destructor */
CObjectCurtain::CObjectCurtain (PVOID pParams, POSINFO pInfo)
{
   m_dwRenderShow = RENDERSHOW_FURNITURE;
   m_OSINFO = *pInfo;

   m_dwShape = 10;
   m_pSize.Zero();
   m_pSize.p[0] = 1;
   m_pSize.p[1] = 1.5;
   m_fRippleSize = 0.10;

   m_fFromWall = .1;
   m_fClothExtra = 1.5;
   m_fCompressability = .3;
   m_fTieV = .5;
   m_fTieH = .2;
   m_fHangSag = .2;
   m_dwHangDivide = 1;
   m_fBladeSize = .05;
   m_fBladeAngle = PI/4;

   m_dwNumPoints = m_dwNumNormals = m_dwNumText = 0;
   m_dwNumVertex = m_dwNumPoly = 0;

   m_fCanBeEmbedded = TRUE;
   m_fOpen = .8;

   // calculate
   CalcInfo();

   // color for the Curtain
   ObjectSurfaceAdd (42, RGB(0xff,0xff,0xff), MATERIAL_CLOTHSMOOTH, L"Curtains");
   ObjectSurfaceAdd (43, RGB(0x80,0x80,0xa0), MATERIAL_CLOTHSMOOTH, L"Blind rails");
}


CObjectCurtain::~CObjectCurtain (void)
{
   // do nothing for now
}


/**********************************************************************************
CObjectCurtain::Delete - Called to delete this object
*/
void CObjectCurtain::Delete (void)
{
   delete this;
}

/**********************************************************************************
CObjectCurtain::Render - Draws a test box.

inputs
   POBJECTRENDER     pr - Render information
*/
void CObjectCurtain::Render (POBJECTRENDER pr, DWORD dwSubObject)
{
   // create the surface render object and draw
   CRenderSurface rs;
   CMatrix mObject;
   rs.Init (pr->pRS);
   ObjectMatrixGet (&mObject);
   rs.Multiply (&mObject);

   // translate out so the curtain rod is away from the wall
   rs.Translate (0, -m_fFromWall, 0);

   DWORD dwSides;
   CPoint   pOffset;
   fp fRotate;
   dwSides = 3;   // 0x01 = left, 0x02 = right, 0x03 = both
   pOffset.Zero();
   fRotate = 0;

   switch (m_dwShape) {
   default:
   case 10: // curtain, both sides
   case 20: // drapes, both sides
   case 50: // blinds, vertical, both sides
   case 60: // blinds, retractable, both sides
      pOffset.p[0] = -m_pSize.p[0]/2;
      break;

   case 11: // curtian, left
   case 21: // drapes, left
   case 51: // blinds, vertical, left
   case 61: // blinds, retractable, left
      pOffset.p[0] = -m_pSize.p[0]/2;
      dwSides = 1;
      break;

   case 12: // curtain right
   case 22: // drapes, right
   case 52: // blinds, vertical, right
   case 62: // blinds, retractable, right
      pOffset.p[0] = -m_pSize.p[0]/2;
      dwSides = 2;
      break;

   case 30: // drapes - hanging
      dwSides = 1;
      break;

   case 40: // blinds, venetian
   case 42: // blinds, roman
   case 63: // blinds, retractable, hanging
      pOffset.p[0] = m_pSize.p[0] / 2; 
      fRotate = PI/2;
      dwSides = 1;
      break;

   case 41: // blinds, cloth, hanging
      dwSides = 1;
      break;
   }

   // draw them
   DWORD i;
   for (i = 0; i < 2; i++) {
      if (!(dwSides & (1 << i)))
         continue;

      rs.Push();
      if (i)
         pOffset.Scale (-1);
      rs.Translate (pOffset.p[0], pOffset.p[1], pOffset.p[2]);
      if (fRotate)
         rs.Rotate (fRotate, 2);
      RenderCurtain (pr, &rs, i);
      rs.Pop();
   }

}


/**********************************************************************************
CObjectCurtain::QueryBoundingBox - Standard API
*/
void CObjectCurtain::QueryBoundingBox (PCPoint pCorner1, PCPoint pCorner2, DWORD dwSubObject)
{
   pCorner1->p[0] = -m_pSize.p[0]/2 * 1.2;
   pCorner1->p[1] = -m_fFromWall;
   pCorner1->p[2] = -m_pSize.p[1] * 1.02;
   pCorner2->p[0] = m_pSize.p[0]/2 * 1.2;
   pCorner2->p[1] = -m_fFromWall;
   pCorner2->p[2] = m_pSize.p[1] * 0.02;

   switch (m_dwShape) {
   case 40: // blinds, venetian
   case 50: // blinds, vertical, both sides
   case 51: // blinds, vertical, left
   case 52: // blinds, vertical, right
      pCorner1->p[0] -= m_fBladeSize/2;
      pCorner2->p[0] += m_fBladeSize/2;
      pCorner1->p[1] -= m_fBladeSize/2;
      pCorner2->p[1] += m_fBladeSize/2;
      break;

   default:
      pCorner1->p[1] -= m_fRippleSize/2 * 1.3;
      pCorner2->p[1] += m_fRippleSize/2 * 1.3;
      break;
   } // switch shape

#ifdef _DEBUG
   // test, make sure bounding box not too small
   CPoint p1,p2;
   DWORD i;
   CObjectTemplate::QueryBoundingBox (&p1, &p2, dwSubObject);
   for (i = 0; i < 3; i++)
      if ((p1.p[i] < pCorner1->p[i] - CLOSE) || (p2.p[i] > pCorner2->p[i] + CLOSE))
         break;
   if (i < 3)
      OutputDebugString ("\r\nCObjectCurtain::QueryBoundingBox too small.");
#endif
}


/**********************************************************************************
CObjectCurtain::Clone - Clones this

inputs
   none
returns
   PCObjectSocket - Clone
*/
PCObjectSocket CObjectCurtain::Clone (void)
{
   PCObjectCurtain pNew;

   pNew = new CObjectCurtain(NULL, &m_OSINFO);

   // clone template info
   CloneTemplate(pNew);

   pNew->m_dwShape = m_dwShape;
   pNew->m_pSize.Copy (&m_pSize);
   pNew->m_fRippleSize = m_fRippleSize;

   pNew->m_dwNumPoints = m_dwNumPoints;
   pNew->m_dwNumNormals = m_dwNumNormals;
   pNew->m_dwNumText = m_dwNumText;
   pNew->m_dwNumVertex = m_dwNumVertex;
   pNew->m_dwNumPoly = m_dwNumPoly;
   pNew->m_fOpen = m_fOpen;

   pNew->m_fFromWall = m_fFromWall;
   pNew->m_fClothExtra = m_fClothExtra;
   pNew->m_fCompressability = m_fCompressability;
   pNew->m_fTieV = m_fTieV;
   pNew->m_fTieH = m_fTieH;
   pNew->m_fHangSag = m_fHangSag;
   pNew->m_dwHangDivide = m_dwHangDivide;
   pNew->m_fBladeSize = m_fBladeSize;
   pNew->m_fBladeAngle = m_fBladeAngle;

   DWORD dwNeed;
   if (pNew->m_memPoints.Required(dwNeed = m_dwNumPoints * sizeof(CPoint)))
      memcpy (pNew->m_memPoints.p, m_memPoints.p, dwNeed);
   if (pNew->m_memNormals.Required(dwNeed = m_dwNumNormals * sizeof(CPoint)))
      memcpy (pNew->m_memNormals.p, m_memNormals.p, dwNeed);
   if (pNew->m_memText.Required(dwNeed = m_dwNumText * sizeof(TEXTPOINT5)))
      memcpy (pNew->m_memText.p, m_memText.p, dwNeed);
   if (pNew->m_memVertex.Required(dwNeed = m_dwNumVertex * sizeof(VERTEX)))
      memcpy (pNew->m_memVertex.p, m_memVertex.p, dwNeed);
   if (pNew->m_memPoly.Required(dwNeed = m_dwNumPoly * sizeof(DWORD)*4))
      memcpy (pNew->m_memPoly.p, m_memPoly.p, dwNeed);
   m_NoodleCircle.CloneTo (&pNew->m_NoodleCircle);
   m_NoodleSquare.CloneTo (&pNew->m_NoodleSquare);
   return pNew;
}

static PWSTR gpszShape = L"Shape";
static PWSTR gpszSize = L"Size";
static PWSTR gpszRippleSize = L"RippleSize";
static PWSTR gpszOpen = L"Open";
static PWSTR gpszFromWall = L"FromWall";
static PWSTR gpszClothExtra = L"ClothExtra";
static PWSTR gpszCompressability = L"Compressability";
static PWSTR gpszTieV = L"TieV";
static PWSTR gpszTieH = L"TieH";
static PWSTR gpszHangSag = L"HangSag";
static PWSTR gpszHangDivide = L"HangDivide";
static PWSTR gpszBladeSize = L"BladeSize";
static PWSTR gpszBladeAngle = L"BladeAngle";

PCMMLNode2 CObjectCurtain::MMLTo (void)
{
   // call into the template first
   PCMMLNode2 pNode = MMLToTemplate();
   if (!pNode)
      return NULL;

   MMLValueSet (pNode, gpszShape, (int) m_dwShape);
   MMLValueSet (pNode, gpszSize, &m_pSize);
   MMLValueSet (pNode, gpszRippleSize, m_fRippleSize);
   MMLValueSet (pNode, gpszOpen, m_fOpen);

   MMLValueSet (pNode, gpszFromWall, m_fFromWall);
   MMLValueSet (pNode, gpszClothExtra, m_fClothExtra);
   MMLValueSet (pNode, gpszCompressability, m_fCompressability);
   MMLValueSet (pNode, gpszTieV, m_fTieV);
   MMLValueSet (pNode, gpszTieH, m_fTieH);
   MMLValueSet (pNode, gpszHangSag, m_fHangSag);
   MMLValueSet (pNode, gpszHangDivide, (int) m_dwHangDivide);
   MMLValueSet (pNode, gpszBladeSize, m_fBladeSize);
   MMLValueSet (pNode, gpszBladeAngle, m_fBladeAngle);
   return pNode;
}

BOOL CObjectCurtain::MMLFrom (PCMMLNode2 pNode)
{
   // call into the template first
   if (!MMLFromTemplate (pNode))
      return FALSE;

   m_dwShape = (DWORD) MMLValueGetInt (pNode, gpszShape, (int) 0);
   MMLValueGetPoint (pNode, gpszSize, &m_pSize);
   m_fOpen = MMLValueGetDouble (pNode, gpszOpen, 0);

   m_fRippleSize = MMLValueGetDouble (pNode, gpszRippleSize, 0);
   m_fFromWall = MMLValueGetDouble (pNode, gpszFromWall, 0);
   m_fClothExtra = MMLValueGetDouble (pNode, gpszClothExtra, 1);
   m_fCompressability = MMLValueGetDouble (pNode, gpszCompressability, .1);
   m_fTieV = MMLValueGetDouble (pNode, gpszTieV, .1);
   m_fTieH = MMLValueGetDouble (pNode, gpszTieH, .1);
   m_fHangSag = MMLValueGetDouble (pNode, gpszHangSag, .1);
   m_dwHangDivide = (DWORD) MMLValueGetInt (pNode, gpszHangDivide, 1);
   m_fBladeSize = MMLValueGetDouble (pNode, gpszBladeSize, 1);
   m_fBladeAngle = MMLValueGetDouble (pNode, gpszBladeAngle, 0);

   CalcInfo ();
   return TRUE;
}

/*********************************************************************************
CObjectCurtain::CalcInfo - Calculate the polygons needed to draw the table cloth.
*/
void CObjectCurtain::CalcInfo ()
{
   switch (m_dwShape) {
   default:
   case 10: // curtain, both sides
      GenerateHangingCurtain (m_fOpen, m_pSize.p[0] / 2 * m_fClothExtra,
         m_pSize.p[1], m_fRippleSize, m_pSize.p[0] / 2, m_fRippleSize * m_fCompressability);
      break;

   case 11: // curtian, left
   case 12: // curtain right
      GenerateHangingCurtain (m_fOpen, m_pSize.p[0] * m_fClothExtra,
         m_pSize.p[1], m_fRippleSize, m_pSize.p[0], m_fRippleSize * m_fCompressability);
      break;

   case 20: // drapes, both sides
      GenerateTiedCurtain (m_fOpen, m_pSize.p[0] * m_fClothExtra,
         m_pSize.p[1], m_fRippleSize, m_pSize.p[0]/2, m_fRippleSize * m_fCompressability,
         m_pSize.p[1] * m_fTieV, .05, m_pSize.p[0] / 2 * m_fTieH);
      break;

   case 21: // drapes, left
   case 22: // drapes, right
      GenerateTiedCurtain (m_fOpen, m_pSize.p[0] * m_fClothExtra,
         m_pSize.p[1], m_fRippleSize, m_pSize.p[0], m_fRippleSize * m_fCompressability,
         m_pSize.p[1] * m_fTieV, .05, m_pSize.p[0] * m_fTieH);
      break;

   case 30: // drapes - hanging
      GenerateFoldingCurtain (m_fOpen, m_pSize.p[0], m_pSize.p[1],
         m_fRippleSize, m_pSize.p[1] * m_fTieH, m_pSize.p[1] * m_fHangSag, m_dwHangDivide);
      break;

   case 40: // blinds, venetian
      GenerateLouvers (m_fOpen, m_pSize.p[1], m_pSize.p[0], m_fBladeSize, -m_fBladeAngle);
      break;

   case 41: // blinds, cloth, hanging
      GenerateFolding2Curtain (m_fOpen, m_pSize.p[0], m_pSize.p[1], m_fRippleSize);
      break;

   case 42: // blinds, roman
      GenerateBlind (m_fOpen, m_pSize.p[1], m_pSize.p[0], FALSE);
      break;

   case 50: // blinds, vertical, both sides
      GenerateLouvers (m_fOpen, m_pSize.p[0]/2, m_pSize.p[1], m_fBladeSize, m_fBladeAngle);
      break;

   case 51: // blinds, vertical, left
   case 52: // blinds, vertical, right
      GenerateLouvers (m_fOpen, m_pSize.p[0], m_pSize.p[1], m_fBladeSize, m_fBladeAngle);
      break;

   case 60: // blinds, retractable, both sides
      GenerateBlind (m_fOpen, m_pSize.p[0]/2, m_pSize.p[1], TRUE);
      break;

   case 61: // blinds, retractable, left
   case 62: // blinds, retractable, right
      GenerateBlind (m_fOpen, m_pSize.p[0], m_pSize.p[1], TRUE);
      break;

   case 63: // blinds, retractable, hanging
      GenerateBlind (m_fOpen, m_pSize.p[1], m_pSize.p[0], TRUE);
      break;
   }

}


/**************************************************************************************
CObjectCurtain::MoveReferencePointQuery - 
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
BOOL CObjectCurtain::MoveReferencePointQuery (DWORD dwIndex, PCPoint pp)
{
   PSPLINEMOVEP ps;
   DWORD dwDataSize;
   ps = gaCurtainMove;
   dwDataSize = sizeof(gaCurtainMove);

   if (dwIndex >= dwDataSize / sizeof(SPLINEMOVEP))
      return FALSE;

   // always at 0,0 in Curtains
   pp->Zero();
   return TRUE;
}

/**************************************************************************************
CObjectCurtain::MoveReferenceStringQuery -
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
BOOL CObjectCurtain::MoveReferenceStringQuery (DWORD dwIndex, PWSTR psz, DWORD dwSize, DWORD *pdwNeeded)
{
   PSPLINEMOVEP ps;
   DWORD dwDataSize;
   ps = gaCurtainMove;
   dwDataSize = sizeof(gaCurtainMove);
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


/* CurtainDialogPage
*/
BOOL CurtainDialogPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCObjectCurtain pv = (PCObjectCurtain) pPage->m_pUserData;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         ComboBoxSet (pPage, L"shape", pv->m_dwShape);

         MeasureToString (pPage, L"ripplesize", pv->m_fRippleSize);
         MeasureToString (pPage, L"fromwall", pv->m_fFromWall);
         MeasureToString (pPage, L"bladesize", pv->m_fBladeSize);
         AngleToControl (pPage, L"bladeangle", pv->m_fBladeAngle, TRUE);
         DoubleToControl (pPage, L"hangdivide", pv->m_dwHangDivide);

         // scrolling
         PCEscControl pControl;
         pControl = pPage->ControlFind (L"tiev");
         if (pControl)
            pControl->AttribSetInt (Pos(), (int)(pv->m_fTieV * 100));
         pControl = pPage->ControlFind (L"tieh");
         if (pControl)
            pControl->AttribSetInt (Pos(), (int)(pv->m_fTieH * 100));
         pControl = pPage->ControlFind (L"hangsag");
         if (pControl)
            pControl->AttribSetInt (Pos(), (int)(pv->m_fHangSag * 100));
         pControl = pPage->ControlFind (L"clothextra");
         if (pControl)
            pControl->AttribSetInt (Pos(), (int)(pv->m_fClothExtra * 100));
         pControl = pPage->ControlFind (L"compressability");
         if (pControl)
            pControl->AttribSetInt (Pos(), (int)(pv->m_fCompressability * 100));

         // enable/disable controls
         pPage->Message (ESCM_USER+86);
      }
      break;

   case ESCM_USER+86:   // enable/disable controls
      {
         BOOL fRippleSize, fBlades, fHangDivide, fTieV, fTieH, fClothExtra, fCompressability;
         PCEscControl pControl;
         fRippleSize = fBlades = fHangDivide = fTieV = fTieH = fClothExtra = fCompressability = FALSE;

         switch (pv->m_dwShape) {
         default:
         case 10: // curtain, both sides
         case 11: // curtian, left
         case 12: // curtain right
            fClothExtra = fRippleSize = fCompressability = TRUE;
            break;

         case 20: // drapes, both sides
         case 21: // drapes, left
         case 22: // drapes, right
            fClothExtra = fRippleSize = fCompressability = fTieV = fTieH = TRUE;
            break;

         case 30: // drapes - hanging
            fTieH = fHangDivide = fRippleSize = TRUE;
            break;

         case 40: // blinds, venetian
         case 50: // blinds, vertical, both sides
         case 51: // blinds, vertical, left
         case 52: // blinds, vertical, right
            fBlades = TRUE;
            break;

         case 41: // blinds, cloth, hanging
            fRippleSize = TRUE;
            break;

         case 42: // blinds, roman
         case 60: // blinds, retractable, both sides
         case 61: // blinds, retractable, left
         case 62: // blinds, retractable, right
         case 63: // blinds, retractable, hanging
            break;
         }

         pControl = pPage->ControlFind (L"ripplesize");
         if (pControl)
            pControl->Enable (fRippleSize);
         pControl = pPage->ControlFind (L"bladesize");
         if (pControl)
            pControl->Enable (fBlades);
         pControl = pPage->ControlFind (L"bladeangle");
         if (pControl)
            pControl->Enable (fBlades);
         pControl = pPage->ControlFind (L"hangdivide");
         if (pControl)
            pControl->Enable (fHangDivide);
         pControl = pPage->ControlFind (L"tiev");
         if (pControl)
            pControl->Enable (fTieV);
         pControl = pPage->ControlFind (L"tieh");
         if (pControl)
            pControl->Enable (fTieH);
         pControl = pPage->ControlFind (L"hangsag");
         if (pControl)
            pControl->Enable (fHangDivide);
         pControl = pPage->ControlFind (L"clothextra");
         if (pControl)
            pControl->Enable (fClothExtra);
         pControl = pPage->ControlFind (L"compressability");
         if (pControl)
            pControl->Enable (fCompressability);
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

         if (!_wcsicmp(psz, L"tiev"))
            pf = &pv->m_fTieV;
         else if (!_wcsicmp(psz, L"tieh"))
            pf = &pv->m_fTieH;
         else if (!_wcsicmp(psz, L"hangsag"))
            pf = &pv->m_fHangSag;
         else if (!_wcsicmp(psz, L"clothextra"))
            pf = &pv->m_fClothExtra;
         else if (!_wcsicmp(psz, L"compressability"))
            pf = &pv->m_fCompressability;

         if (!pf || (*pf == fVal))
            break;   // no change

         pv->m_pWorld->ObjectAboutToChange (pv);
         *pf = fVal;
         pv->CalcInfo();
         pv->m_pWorld->ObjectChanged (pv);
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

         if (!_wcsicmp(psz, L"shape")) {
            if (dwVal == pv->m_dwShape)
               break;

            pv->m_pWorld->ObjectAboutToChange (pv);
            pv->m_dwShape = dwVal;
            pv->CalcInfo();
            pv->m_pWorld->ObjectChanged (pv);

            // enable/disable controls
            pPage->Message (ESCM_USER+86);
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

         MeasureParseString (pPage, L"ripplesize", &pv->m_fRippleSize);
         pv->m_fRippleSize = max (pv->m_fRippleSize, .01);
         MeasureParseString (pPage, L"fromwall", &pv->m_fFromWall);
         MeasureParseString (pPage, L"bladesize", &pv->m_fBladeSize);
         pv->m_fBladeSize = max(pv->m_fBladeSize, .01);
         pv->m_fBladeAngle = AngleFromControl (pPage, L"bladeangle");
         pv->m_dwHangDivide = (DWORD) DoubleFromControl (pPage, L"hangdivide");

         pv->CalcInfo ();
         pv->m_pWorld->ObjectChanged (pv);


         break;
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Curtain/blind settings";
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}


/**********************************************************************************
CObjectCurtain::DialogShow - Causes the object to show a dialog box that allows
different attributes to be changed.

inputs
   DWORD             dwSuface - Surface that was clicked on. 0 if unknown.
   PCEscWindow       pWindow - Escarpment window to draw it in
returns
   BOOL - TRUE if user pressed "Back", FALSE if user pressed "Close"
*/
BOOL CObjectCurtain::DialogShow (DWORD dwSurface, PCEscWindow pWindow, PCPoint pClick)
{
   PWSTR pszRet;
   pszRet = pWindow->PageDialog (ghInstance, IDR_MMLCURTAINDIALOG, CurtainDialogPage, this);
   if (!pszRet)
      return FALSE;

   return !_wcsicmp(pszRet, Back());
}


/**********************************************************************************
CObjectCurtain::DialogQuery - Returns TRUE if the object supports a custom
dialog box, FALSE if it doesn't. The default object doesn't
*/
BOOL CObjectCurtain::DialogQuery (void)
{
   return TRUE;
}

/**********************************************************************************
CObjectCurtain::GenerateLouvers - Fills the m_dwNumPoints, m_memPoints, etc.
with information to draw the louvers hanging down

The curtain's UL corner is 0,0,0 (centered Y-wisze). It extends (and closes) to
the right. It falls down to -Z values.

inputs
   fp       fOpen - Amount that open, from 0 (closed) to 1 (open)
   fp       fWidth - Width of the louvers (when filly closed)
   fp       fHeight - Height of the louvers
   fp       fLouverWidth - Width of an individual louver
   fp       fAngle - Angle of the blades - 0 = perp to wall, PI/2 = parallel to wall
returns
   BOOL - TRUE if success
*/
BOOL CObjectCurtain::GenerateLouvers (fp fOpen, fp fWidth, fp fHeight,
                                      fp fLouverWidth, fp fAngle)
{
   // some limits
   fOpen = max(0,fOpen);
   fOpen = min(1, fOpen);
   fLouverWidth = max(.01, fLouverWidth);
   fWidth = max(fLouverWidth, fWidth);
   fHeight = max (fHeight, fLouverWidth);


   // zero out
   m_dwNumPoints = m_dwNumNormals = m_dwNumText = 0;
   m_dwNumVertex = m_dwNumPoly = 0;

   // calculate
   DWORD dwNumBlades;
   dwNumBlades = (DWORD) ceil(fWidth / fLouverWidth);
   dwNumBlades = max(dwNumBlades, 1);

   // how many points?
   DWORD dwNum;
   dwNum = dwNumBlades * 4;

   // allocate the points
   // so, we know how many points, etc. we need
   m_dwNumPoints = dwNum;
   m_dwNumNormals = 1; // since all the same direction, only 1
   m_dwNumText = dwNum; // so can include a picture on the louvers
   m_dwNumVertex = dwNum;
   m_dwNumPoly = dwNumBlades;

   // make sure have enough memory
   if (!m_memPoints.Required (m_dwNumPoints * sizeof(CPoint)))
      return FALSE;
   if (!m_memNormals.Required (m_dwNumNormals * sizeof(CPoint)))
      return FALSE;
   if (!m_memText.Required (m_dwNumText * sizeof(TEXTPOINT5)))
      return FALSE;
   if (!m_memVertex.Required (m_dwNumVertex * sizeof(VERTEX)))
      return FALSE;
   if (!m_memPoly.Required (m_dwNumPoly * sizeof(DWORD) * 4))
      return FALSE;

   // pointers to these
   PCPoint pPoint, pNormal;
   PTEXTPOINT5 pText;
   PVERTEX pVert;
   DWORD *pPoly;
   pPoint = (PCPoint) m_memPoints.p;
   memset (pPoint, 0 ,sizeof (CPoint) * m_dwNumPoints);
   pNormal = (PCPoint) m_memNormals.p;
   memset (pNormal, 0, sizeof(CPoint) * m_dwNumNormals);
   pText = (PTEXTPOINT5) m_memText.p;
   memset (pText, 0, sizeof(TEXTPOINT5) * m_dwNumText);
   pVert = (PVERTEX) m_memVertex.p;
   memset (pVert, 0, sizeof(VERTEX) * m_dwNumVertex);
   pPoly = (DWORD*) m_memPoly.p;
   memset (pPoly, 0, m_dwNumPoly * 4 * sizeof(DWORD));


   // calculate the points and TEXTPOINT5s
   PCPoint p;
   PTEXTPOINT5 pt;
   DWORD i, j;
   p = pPoint;
   pt = pText;
   for (j = 0; j < dwNumBlades; j++) { // louver blades
      fp fC, fT;
      fT = (fp) j / (fp)(dwNumBlades-1) * fWidth;
      fC = fT - fOpen * fWidth;
      fC = max(fC, (fp) j * fLouverWidth / 25);  // can only go so far to left

      for (i = 0; i < 4; i++, p++, pt++) {   // 4 corners: UL, UR, LR, LL
         // texture
         pt->hv[0] = fT + fLouverWidth / 2 * ( ((i == 0) || (i == 3)) ? -1 : 1);
         pt->hv[1] = ((i < 2) ? 0 : fHeight);

         // not really sure how to do volumetric curtains so linking to flat version
         pt->xyz[0] = pt->hv[0];
         pt->xyz[1] = -pt->hv[1];
         pt->xyz[2] = 0;

         p->p[0] = sin(fAngle) * fLouverWidth/2 * ( ((i == 0) || (i == 3)) ? -1 : 1) + fC;
         p->p[1] = cos(fAngle) * fLouverWidth/2 * ( ((i == 0) || (i == 3)) ? -1 : 1);
         p->p[2] = -((i < 2) ? 0 : fHeight);
      }
   }


   
   // calculate the normals
   pNormal->p[0] = cos(fAngle);
   pNormal->p[1] = -sin(fAngle);
   pNormal->p[2] = 0;
   
   // all the vertices
   PVERTEX pv;
   pv = pVert;
   
   // on the top
   for (j = 0; j < dwNumBlades; j++)
      for (i = 0; i < 4; i++, pv++) {
         pv->dwNormal = 0;
         pv->dwPoint = j*4 + i;
         pv->dwTexture = j*4 + i;
      }


   // and the polygons
   DWORD *padw;
   padw = pPoly;

   for (i = 0; i < dwNumBlades; i++, padw += 4) {
      for (j = 0; j < 4; j++)
         padw[j] = i*4+j;
   }

   return TRUE;
}



/**********************************************************************************
CObjectCurtain::GenerateBlind - Fills the m_dwNumPoints, m_memPoints, etc.
with information to draw the blind coming from the left.

The curtain's UL corner is 0,0,0 (centered Y-wisze). It extends (and closes) to
the right. It falls down to -Z values.

inputs
   fp       fOpen - Amount that open, from 0 (closed) to 1 (open)
   fp       fWidth - Width of the blind (when filly closed)
   fp       fHeight - Height of the blind
   BOOL     fFromLeft - if TRUE, the blind is pulled from the left, so the
            texture is aligned with right edge. if FALSE, rolls from right,
            so texture aligned with left edge.
returns
   BOOL - TRUE if success
*/
BOOL CObjectCurtain::GenerateBlind (fp fOpen, fp fWidth, fp fHeight, BOOL fFromLeft)
{
   // some limits
   fOpen = max(0,fOpen);
   fOpen = min(1, fOpen);
   fWidth = max(CLOSE, fWidth);
   fHeight = max (CLOSE, fHeight);

   // fill in the noodles
   CPoint pStart, pEnd, pScale;
   pStart.Zero();
   pEnd.Zero();
   pScale.Zero();
   pStart.p[2] = -fHeight;
   if (fFromLeft) {
      pScale.p[0] = pScale.p[1] = fWidth / 50;
   }
   else {
      pScale.p[0] = pScale.p[1] = fWidth*(fOpen + .1) / 50;
      pStart.p[0] = pEnd.p[0] = fWidth * (1.0 - fOpen);
   }
   pStart.p[1] = pEnd.p[1] = -pScale.p[0] / 2.0;
   m_NoodleCircle.BackfaceCullSet (TRUE);
   m_NoodleCircle.DrawEndsSet (TRUE);
   m_NoodleCircle.PathLinear (&pStart, &pEnd);
   m_NoodleCircle.ScaleVector (&pScale);
   m_NoodleCircle.ShapeDefault (NS_CIRCLEFAST);

   pStart.Zero();
   pEnd.Zero();
   pScale.Zero();
   pStart.p[2] = -fHeight;
   pScale.p[0] = pScale.p[1] = fWidth / 100;
   if (fFromLeft) {
      pStart.p[0] = pEnd.p[0] = fWidth * (1.0 - fOpen);
   }
   m_NoodleSquare.BackfaceCullSet (TRUE);
   m_NoodleSquare.DrawEndsSet (TRUE);
   m_NoodleSquare.PathLinear (&pStart, &pEnd);
   m_NoodleSquare.ScaleVector (&pScale);
   m_NoodleSquare.ShapeDefault (NS_RECTANGLE);

   // zero out
   m_dwNumPoints = m_dwNumNormals = m_dwNumText = 0;
   m_dwNumVertex = m_dwNumPoly = 0;

   // how many points?
   DWORD dwNum, j;
   dwNum = 4;

   // allocate the points
   // so, we know how many points, etc. we need
   m_dwNumPoints = dwNum;
   m_dwNumNormals = 1; // since all the same direction, only 1
   m_dwNumText = dwNum; // so can include a picture on the louvers
   m_dwNumVertex = dwNum;
   m_dwNumPoly = 1;

   // make sure have enough memory
   if (!m_memPoints.Required (m_dwNumPoints * sizeof(CPoint)))
      return FALSE;
   if (!m_memNormals.Required (m_dwNumNormals * sizeof(CPoint)))
      return FALSE;
   if (!m_memText.Required (m_dwNumText * sizeof(TEXTPOINT5)))
      return FALSE;
   if (!m_memVertex.Required (m_dwNumVertex * sizeof(VERTEX)))
      return FALSE;
   if (!m_memPoly.Required (m_dwNumPoly * sizeof(DWORD) * 4))
      return FALSE;

   // pointers to these
   PCPoint pPoint, pNormal;
   PTEXTPOINT5 pText;
   PVERTEX pVert;
   DWORD *pPoly;
   pPoint = (PCPoint) m_memPoints.p;
   memset (pPoint, 0 ,sizeof (CPoint) * m_dwNumPoints);
   pNormal = (PCPoint) m_memNormals.p;
   memset (pNormal, 0, sizeof(CPoint) * m_dwNumNormals);
   pText = (PTEXTPOINT5) m_memText.p;
   memset (pText, 0, sizeof(TEXTPOINT5) * m_dwNumText);
   pVert = (PVERTEX) m_memVertex.p;
   memset (pVert, 0, sizeof(VERTEX) * m_dwNumVertex);
   pPoly = (DWORD*) m_memPoly.p;
   memset (pPoly, 0, m_dwNumPoly * 4 * sizeof(DWORD));


   // calculate the points and TEXTPOINT5s
   PCPoint p;
   PTEXTPOINT5 pt;
   DWORD i;
   p = pPoint;
   pt = pText;
   for (i = 0; i < 4; i++, p++, pt++) {   // 4 corners: UL, UR, LR, LL
      p->p[0] = fWidth * (1-fOpen) * ( ((i == 0) || (i == 3)) ? 0 : 1);
      p->p[1] = 0;
      p->p[2] = -((i < 2) ? 0 : fHeight);

      // texture
      pt->hv[0] = p->p[0];
      if (fFromLeft)
         pt->hv[0] -= fWidth * (1-fOpen);
      pt->hv[1] = -p->p[2];

      // not really sure how to do volumetric curtains so linking to flat version
      pt->xyz[0] = pt->hv[0];
      pt->xyz[1] = -pt->hv[1];
      pt->xyz[2] = 0;

   }


   
   // calculate the normals
   pNormal->p[0] = 0;
   pNormal->p[1] = -1;
   pNormal->p[2] = 0;
   
   // all the vertices
   PVERTEX pv;
   pv = pVert;
   for (i = 0; i < 4; i++, pv++) {
      pv->dwNormal = 0;
      pv->dwPoint = i;
      pv->dwTexture = i;
   }


   // and the polygons
   DWORD *padw;
   padw = pPoly;
   for (j = 0; j < 4; j++)
      padw[j] = j;

   return TRUE;
}


/**********************************************************************************
CObjectCurtain::GenerateHangingCurtain - Fills the m_dwNumPoints, m_memPoints, etc.
with information to draw the hanging curtain.

The curtain's UL corner is 0,0,0 (centered Y-wisze). It extends (and closes) to
the right. It falls down to -Z values.

inputs
   fp       fOpen - Amount that open, from 0 (closed) to 1 (open)
   fp       fFabricWidth - Width of the fabric (when fully flattened out)
   fp       fFabricHeight - Height of the fabric
   fp       fRippleLen - Length of each ripple in the curtain
   fp       fClosedWidth - Amount of space allotted when curtain totally closed
   fp       fCompress - Minimium width per ripple, when curtain entirely open
returns
   BOOL - TRUE if success
*/
BOOL CObjectCurtain::GenerateHangingCurtain (fp fOpen, fp fFabricWidth, fp fFabricHeight,
                                             fp fRippleLen, fp fClosedWidth, fp fCompress)
{
   // some limits
   fOpen = max(0,fOpen);
   fOpen = min(1, fOpen);
   fFabricWidth = max(CLOSE, fFabricWidth);
   fFabricHeight = max(CLOSE, fFabricHeight);
   fRippleLen = max(.01, fRippleLen);
   fClosedWidth = max(CLOSE, fClosedWidth);
   fClosedWidth = min (fClosedWidth, fFabricWidth); // cant extend more than the width
   fCompress = max(CLOSE, fCompress);


   // zero out
   m_dwNumPoints = m_dwNumNormals = m_dwNumText = 0;
   m_dwNumVertex = m_dwNumPoly = 0;

   // calculate
   DWORD dwNumRipples;
   fp fOpenWidth, fWidth;
   dwNumRipples = (DWORD)ceil(fClosedWidth / fRippleLen);
   dwNumRipples = max(1,dwNumRipples);
   fOpenWidth = (fp) dwNumRipples * fCompress;
   fOpenWidth = min(fOpenWidth, fClosedWidth-CLOSE);
   fWidth = fOpen * (fOpenWidth - fClosedWidth) + fClosedWidth;

   // how many points?
   DWORD dwNum;
   dwNum = dwNumRipples * RIPPLEDETAIL + 1;

   // allocate the points
   // so, we know how many points, etc. we need
   m_dwNumPoints = dwNum /* top*/ + dwNum /*bottom*/;
   m_dwNumNormals = dwNum; // could optimize down to just 8, but minimize bugs
   m_dwNumText = m_dwNumPoints;
   m_dwNumVertex = m_dwNumPoints;
   m_dwNumPoly = dwNum-1;

   // make sure have enough memory
   if (!m_memPoints.Required (m_dwNumPoints * sizeof(CPoint)))
      return FALSE;
   if (!m_memNormals.Required (m_dwNumNormals * sizeof(CPoint)))
      return FALSE;
   if (!m_memText.Required (m_dwNumText * sizeof(TEXTPOINT5)))
      return FALSE;
   if (!m_memVertex.Required (m_dwNumVertex * sizeof(VERTEX)))
      return FALSE;
   if (!m_memPoly.Required (m_dwNumPoly * sizeof(DWORD) * 4))
      return FALSE;

   // pointers to these
   PCPoint pPoint, pNormal;
   PTEXTPOINT5 pText;
   PVERTEX pVert;
   DWORD *pPoly;
   pPoint = (PCPoint) m_memPoints.p;
   memset (pPoint, 0 ,sizeof (CPoint) * m_dwNumPoints);
   pNormal = (PCPoint) m_memNormals.p;
   memset (pNormal, 0, sizeof(CPoint) * m_dwNumNormals);
   pText = (PTEXTPOINT5) m_memText.p;
   memset (pText, 0, sizeof(TEXTPOINT5) * m_dwNumText);
   pVert = (PVERTEX) m_memVertex.p;
   memset (pVert, 0, sizeof(VERTEX) * m_dwNumVertex);
   pPoly = (DWORD*) m_memPoly.p;
   memset (pPoly, 0, m_dwNumPoly * 4 * sizeof(DWORD));


   // BUGFIX - fYOffset so ends are connected to rod
   fp fYOffset;
   fYOffset = -(fFabricWidth - fWidth) / (fp) dwNumRipples / 4.0 * .8;

   // calculate the points and TEXTPOINT5s
   PCPoint p;
   PTEXTPOINT5 pt;
   DWORD i;
   p = pPoint;
   pt = pText;
   // around edge of table
   for (i = 0; i < dwNum; i++, p++, pt++) {
      fp f = (fp)i / (fp)(dwNum-1);
      fp fAngle = (fp) (i % RIPPLEDETAIL) / (fp)RIPPLEDETAIL * 2 * PI;

      // texture, for top and bottom
      pt->hv[0] = f * fFabricWidth;
      pt->hv[1] = 0;

      // not really sure how to do volumetric curtains so linking to flat version
      pt->xyz[0] = pt->hv[0];
      pt->xyz[1] = -pt->hv[1];
      pt->xyz[2] = 0;

      pt[dwNum].hv[0] = pt->hv[0];
      pt[dwNum].hv[0] = fFabricHeight;
      // not really sure how to do volumetric curtains so linking to flat version
      pt[dwNum].xyz[0] = pt[dwNum].hv[0];
      pt[dwNum].xyz[1] = -pt[dwNum].hv[1];
      pt[dwNum].xyz[2] = 0;


      // fabric for top and bottom
      p->p[0] = f * fWidth +
         sin(-fAngle*2) * (1.0 - (fWidth - fOpenWidth) / (fFabricWidth - fOpenWidth)) *
         fCompress / 2.0 * .8;
         // the last term above makes it ripple back and forth
         // the .8 is a fudge factor
      p->p[1] = fYOffset + sin (fAngle) *
         (fFabricWidth - fWidth) / (fp) dwNumRipples / 4.0 * .8;
         // the .8 is a fudge factor to account for the back and forth in X
      p->p[2] = 0;
      p[dwNum].Copy (p);
      p[dwNum].p[2] = -fFabricHeight;
   }


   
   // calculate the normals
   CPoint p1, pUp;
   pUp.Zero();
   pUp.p[2] = 1;
   p = pNormal;
   // around the edge of the table
   for (i = 0; i < dwNum; i++, p++) {
      if (i == 0)
         p1.Subtract (&pPoint[1], &pPoint[0]);
      else if (i >= dwNum-1)
         p1.Subtract (&pPoint[i], &pPoint[i-1]);
      else
         p1.Subtract (&pPoint[i+1], &pPoint[i-1]); // more accurate

      p->CrossProd (&p1, &pUp);
      p->Normalize();
   }


   // all the vertices
   PVERTEX pv;
   pv = pVert;
   
   // on the top
   for (i = 0; i < dwNum; i++, pv++) {
      pv->dwNormal = i;
      pv->dwPoint = i;
      pv->dwTexture = i;
   }

   // on the bottom
   for (i = 0; i < dwNum; i++, pv++) {
      pv->dwNormal = i;
      pv->dwPoint = i+dwNum;
      pv->dwTexture = i+dwNum;
   }


   // and the polygons
   DWORD *padw;
   padw = pPoly;

   for (i = 0; i < dwNum-1; i++, padw += 4) {
      padw[0] = i;   // UL
      padw[1] = i+1;  // UR
      padw[2] = i+1+dwNum; // LR
      padw[3] = i+dwNum; // LL
   }

   return TRUE;
}


/**********************************************************************************
CObjectCurtain::GenerateTiedCurtain - Fills the m_dwNumPoints, m_memPoints, etc.
with information to draw the tied curtain.

The curtain's UL corner is 0,0,0 (centered Y-wisze). It extends (and closes) to
the right. It falls down to -Z values.

inputs
   fp       fOpen - Amount that open, from 0 (closed) to 1 (open)
   fp       fFabricWidth - Width of the fabric (when fully flattened out)
   fp       fFabricHeight - Height of the fabric
   fp       fRippleLen - Length of each ripple in the curtain
   fp       fClosedWidth - Amount of space allotted when curtain totally closed
   fp       fCompress - Minimium width per ripple, when curtain entirely open
   fp       fTieV - Tie down this many meters
   fp       fTieHeight - Height of the tie strang - such as .05m
   fp       fTieH - When fully tied down, this is the with of the tie area
returns
   BOOL - TRUE if success
*/
BOOL CObjectCurtain::GenerateTiedCurtain (fp fOpen, fp fFabricWidth, fp fFabricHeight,
                                             fp fRippleLen, fp fClosedWidth, fp fCompress,
                                             fp fTieV, fp fTieHeight, fp fTieH)
{
   // some limits
   fOpen = max(0,fOpen);
   fOpen = min(1, fOpen);
   fFabricWidth = max(CLOSE, fFabricWidth);
   fFabricHeight = max(CLOSE, fFabricHeight);
   fRippleLen = max(.01, fRippleLen);
   fClosedWidth = max(CLOSE, fClosedWidth);
   fClosedWidth = min (fClosedWidth, fFabricWidth); // cant extend more than the width
   fCompress = max(CLOSE, fCompress);
   fTieHeight = max (.01, fTieHeight);
   fTieHeight = min (fFabricHeight/2, fTieHeight);
   fTieV = min (fTieV, fFabricHeight - fTieHeight);
   fTieV = max (fTieV, fTieHeight);
   fTieH = max (CLOSE, fTieH);



   // zero out
   m_dwNumPoints = m_dwNumNormals = m_dwNumText = 0;
   m_dwNumVertex = m_dwNumPoly = 0;

   // calculate
   DWORD dwNumRipples;
   fp fMinWidth;
   dwNumRipples = (DWORD)ceil(fClosedWidth / fRippleLen);
   dwNumRipples = max(1,dwNumRipples);
   fMinWidth = (fp) dwNumRipples * fCompress;
   fMinWidth = min(fMinWidth, fClosedWidth-CLOSE);

   // adjust the tie H depending on open/close
   fTieH = fOpen * (fTieH - fClosedWidth) + fClosedWidth;

   // how many points horizontal?
   DWORD dwNumH;
   dwNumH = dwNumRipples * RIPPLEDETAIL + 1;

#define VERTTIE         8     // vertical points

   // allocate the points
   // so, we know how many points, etc. we need
   m_dwNumPoints = dwNumH * VERTTIE;
   m_dwNumNormals = m_dwNumPoints;
   m_dwNumText = m_dwNumPoints;
   m_dwNumVertex = m_dwNumPoints;
   m_dwNumPoly = (dwNumH-1) * (VERTTIE-1);

   // make sure have enough memory
   if (!m_memPoints.Required (m_dwNumPoints * sizeof(CPoint)))
      return FALSE;
   if (!m_memNormals.Required (m_dwNumNormals * sizeof(CPoint)))
      return FALSE;
   if (!m_memText.Required (m_dwNumText * sizeof(TEXTPOINT5)))
      return FALSE;
   if (!m_memVertex.Required (m_dwNumVertex * sizeof(VERTEX)))
      return FALSE;
   if (!m_memPoly.Required (m_dwNumPoly * sizeof(DWORD) * 4))
      return FALSE;

   // pointers to these
   PCPoint pPoint, pNormal;
   PTEXTPOINT5 pText;
   PVERTEX pVert;
   DWORD *pPoly;
   pPoint = (PCPoint) m_memPoints.p;
   memset (pPoint, 0 ,sizeof (CPoint) * m_dwNumPoints);
   pNormal = (PCPoint) m_memNormals.p;
   memset (pNormal, 0, sizeof(CPoint) * m_dwNumNormals);
   pText = (PTEXTPOINT5) m_memText.p;
   memset (pText, 0, sizeof(TEXTPOINT5) * m_dwNumText);
   pVert = (PVERTEX) m_memVertex.p;
   memset (pVert, 0, sizeof(VERTEX) * m_dwNumVertex);
   pPoly = (DWORD*) m_memPoly.p;
   memset (pPoly, 0, m_dwNumPoly * 4 * sizeof(DWORD));

   // BUGFIX - So hand out from curtain rod, not in and out
   fp fYOffset;
   fp fWidth;
   fWidth = fClosedWidth;
   fYOffset = -(fFabricWidth - fWidth) / (fp) dwNumRipples / 4.0 * .8;

   // calculate the points and TEXTPOINT5s
   PCPoint p;
   PTEXTPOINT5 pt;
   DWORD i, j;
   p = pPoint;
   pt = pText;
   for (j = 0; j < VERTTIE; j++) {
      for (i = 0; i < dwNumH; i++, p++, pt++) {
         fp f = (fp)i / (fp)(dwNumH-1);
         fp fAngle = (fp) (i % RIPPLEDETAIL) / (fp)RIPPLEDETAIL * 2 * PI;
         fp fV;

         if (j <= VERTTIE - 3)
            fWidth = (1 - cos ((fp) j / (fp) (VERTTIE-3) * PI/2)) * (fTieH-fClosedWidth) + fClosedWidth;
         else if (j == VERTTIE-2)
            fWidth = fTieH;
         else
            fWidth = max(fMinWidth, fTieH);

         // fV from 0 to fFabricHeight (0 at top of curtain, fFabricHeight at bottom)
         if (j <= VERTTIE - 3)
            fV = (fp) j / (fp) (VERTTIE - 3) * (fTieV - fTieHeight/2.0);
         else if (j == VERTTIE-2)
            fV = fTieV + fTieHeight/2.0;
         else
            fV = fFabricHeight;

         // need to pull up on the open end
         fp fPullUp;
         fPullUp = (fClosedWidth - fTieH) * (fClosedWidth - fTieH) + fTieV * fTieV;
         fPullUp = sqrt(fPullUp);
         fPullUp -= fTieV;
         fPullUp = max(fPullUp, 0);
         fPullUp *= pow((fp) i / (fp)(dwNumH-1) * PI / 2.0, .5); // so pull up more on the outside

         fPullUp *= (fp) min(j,VERTTIE-3) / (fp) (VERTTIE-3);

         // texture, for top and bottom
         pt->hv[0] = f * fFabricWidth;
         pt->hv[1] = fV + fPullUp;  // to simulate pulling up cloth

         // not really sure how to do volumetric curtains so linking to flat version
         pt->xyz[0] = pt->hv[0];
         pt->xyz[1] = -pt->hv[1];
         pt->xyz[2] = 0;

         // fabric for top and bottom
         // Need to max out move back-and forth so that when drawn real-tight
         // together sheet wont go through itself
         fp fBackForth;
         fBackForth = (1.0 - (fWidth - fMinWidth) / (fFabricWidth - fMinWidth));
         fBackForth = max(0,fBackForth);
         fBackForth = min(1,fBackForth);

         p->p[0] = f * fWidth +
            sin(-fAngle*2) * fBackForth * fCompress / 2.0 * .8;
            // the last term above makes it ripple back and forth
            // the .8 is a fudge factor

         p->p[1] = fYOffset + sin (fAngle) *
            (fFabricWidth - fWidth) / (fp) dwNumRipples / 4.0 * .8;
            // the .8 is a fudge factor to account for the back and forth in X
         p->p[2] = -fV;
         if (j == VERTTIE-1) {
            if (fTieH < fMinWidth)
               p->p[0] += (fTieH - fMinWidth) / 3;
            p->p[2] += fPullUp;
         }

      }
   }


   
   // calculate the normals
   CPoint p1, p2;
   p = pNormal;
   for (j = 0; j < VERTTIE; j++) {
      for (i = 0; i < dwNumH; i++, p++) {
         // right vs. left
         DWORD dw1, dw2;
         dw1 = (i > 0) ? (i-1) : 0;
         dw2 = (i < dwNumH-1) ? (i+1) : i;
         p1.Subtract (&pPoint[dw1 + j * dwNumH], &pPoint[dw2 + j * dwNumH]);

         // top vs down
         dw1 = (j > 0) ? (j - 1) : 0;
         dw2 = (j < VERTTIE-1) ? (j+1) : j;
         p2.Subtract (&pPoint[i + dw1 * dwNumH], &pPoint[i + dw2 * dwNumH]);

         p->CrossProd (&p2, &p1);
         p->Normalize();
      }
   }


   // all the vertices
   PVERTEX pv;
   pv = pVert;
   for (i = 0; i < m_dwNumVertex; i++, pv++) {
      pv->dwNormal = i;
      pv->dwPoint = i;
      pv->dwTexture = i;
   }



   // and the polygons
   DWORD *padw;
   padw = pPoly;

   for (j = 0; j < VERTTIE-1; j++) {
      for (i = 0; i < dwNumH-1; i++, padw+=4) {
         padw[0] = i + j * dwNumH;   // UL
         padw[1] = i+1 + j * dwNumH;  // UR
         padw[2] = i+1+ (j+1)*dwNumH; // LR
         padw[3] = i+ (j+1)*dwNumH; // LL
      }
   }

   return TRUE;
}


/**********************************************************************************
CObjectCurtain::GenerateFoldingCurtain - Fills the m_dwNumPoints, m_memPoints, etc.
with information to draw the hanging curtain.

The curtain's UL corner is -fFabricWidth/2,0,0 (centered Y-wisze). It extends (and closes) to
the right. It falls down to -Z values.

inputs
   fp       fOpen - Amount that open, from 0 (closed) to 1 (open)
   fp       fFabricWidth - Width of the fabric
   fp       fFabricHeight - Height of the fabric  (when fully flattened out)
   fp       fRippleLen - Length of each ripple in the curtain
   fp       fMinHeight - Minimim height - when fully open
   fp       fSag - Amount (in meters) the curtain sages
   DWORD    dwTies - Number of ties (excluding the edges). 0 => sag entirely in
            middle, 1 => divided in half, etc.
returns
   BOOL - TRUE if success
*/
BOOL CObjectCurtain::GenerateFoldingCurtain (fp fOpen, fp fFabricWidth, fp fFabricHeight,
                                             fp fRippleLen, fp fMinHeight, fp fSag,
                                             DWORD dwTies)
{
   // some limits
   fOpen = max(0,fOpen);
   fOpen = min(1, fOpen);
   fFabricWidth = max(CLOSE, fFabricWidth);
   fFabricHeight = max(CLOSE, fFabricHeight);
   fRippleLen = max(.01, fRippleLen);
   fMinHeight = max(CLOSE, fMinHeight);
   fMinHeight = min(fFabricHeight, fMinHeight);
   fSag = max( CLOSE, fSag);
   fSag = min(fFabricHeight,fSag);

   // zero out
   m_dwNumPoints = m_dwNumNormals = m_dwNumText = 0;
   m_dwNumVertex = m_dwNumPoly = 0;

   // calculate
   DWORD dwNumRipples;
   dwNumRipples = (DWORD)ceil(fFabricHeight / fRippleLen);
   dwNumRipples = max(1,dwNumRipples);

   // how many points horizontal?
   DWORD dwNumH, dwNumV;
   dwNumH = (dwTies+1) * RIPPLEDETAIL + 1;
   dwNumV = dwNumRipples * RIPPLEDETAIL + 1;

   // allocate the points
   // so, we know how many points, etc. we need
   m_dwNumPoints = dwNumH * dwNumV;
   m_dwNumNormals = m_dwNumPoints;
   m_dwNumText = m_dwNumPoints;
   m_dwNumVertex = m_dwNumPoints;
   m_dwNumPoly = (dwNumH-1) * (dwNumV-1);

   // make sure have enough memory
   if (!m_memPoints.Required (m_dwNumPoints * sizeof(CPoint)))
      return FALSE;
   if (!m_memNormals.Required (m_dwNumNormals * sizeof(CPoint)))
      return FALSE;
   if (!m_memText.Required (m_dwNumText * sizeof(TEXTPOINT5)))
      return FALSE;
   if (!m_memVertex.Required (m_dwNumVertex * sizeof(VERTEX)))
      return FALSE;
   if (!m_memPoly.Required (m_dwNumPoly * sizeof(DWORD) * 4))
      return FALSE;

   // pointers to these
   PCPoint pPoint, pNormal;
   PTEXTPOINT5 pText;
   PVERTEX pVert;
   DWORD *pPoly;
   pPoint = (PCPoint) m_memPoints.p;
   memset (pPoint, 0 ,sizeof (CPoint) * m_dwNumPoints);
   pNormal = (PCPoint) m_memNormals.p;
   memset (pNormal, 0, sizeof(CPoint) * m_dwNumNormals);
   pText = (PTEXTPOINT5) m_memText.p;
   memset (pText, 0, sizeof(TEXTPOINT5) * m_dwNumText);
   pVert = (PVERTEX) m_memVertex.p;
   memset (pVert, 0, sizeof(VERTEX) * m_dwNumVertex);
   pPoly = (DWORD*) m_memPoly.p;
   memset (pPoly, 0, m_dwNumPoly * 4 * sizeof(DWORD));

   // calculate the points and TEXTPOINT5s
   PCPoint p;
   PTEXTPOINT5 pt;
   p = pPoint;
   pt = pText;
   DWORD i, j;
   for (j = 0; j < dwNumV; j++) {
      for (i = 0; i < dwNumH; i++, p++, pt++) {
         // how high is this section
         fp fHeight;
         fHeight = fabs(sin((fp) i / (fp) RIPPLEDETAIL * PI)) * fSag;
         fHeight += (1.0 - fOpen) * (fFabricHeight - fMinHeight) + fMinHeight;
         fHeight = min(fHeight, fFabricHeight); // cant be longer than this

         // how much is bunched up
         fp fBunch, fBunchPer, fUnBunchPer;
         fBunch = fFabricHeight - fHeight;
         fBunch /= 2;   // can easily fit 4m of fabric length into 1m of bunching
         fBunch *= (fabs(sin((fp) i / (fp) RIPPLEDETAIL * PI)) +2) / 3.0;
         fBunch = min(fBunch, fHeight);   // cant have more bunched up than height
         fUnBunchPer = (fHeight - fBunch) / fFabricHeight;
         fBunchPer = 1.0 - fUnBunchPer;
         fBunchPer = max(CLOSE, fBunchPer);

         // how far down the chain
         fp f = (fp)j / (fp)(dwNumV-1);

         // texture, for top and bottom
         pt->hv[0] = (fp) i / (fp) (dwNumH-1) * fFabricWidth - fFabricWidth / 2;
         if (f < fUnBunchPer)
            pt->hv[1] = f * fFabricHeight;
         else
            pt->hv[1] = (pow((fp)((f - fUnBunchPer)/fBunchPer), (fp).5) * fBunchPer + fUnBunchPer) * fFabricHeight;

         // not really sure how to do volumetric curtains so linking to flat version
         pt->xyz[0] = pt->hv[0];
         pt->xyz[1] = -pt->hv[1];
         pt->xyz[2] = 0;

         p->p[0] = pt->hv[0];  // note - not stretching left and right, but shouldnt
            // be too visible

         if (f < fUnBunchPer)
            p->p[1] = 0;
         else
            p->p[1] = sin((fp) j / (fp) RIPPLEDETAIL * 2 * PI) *
               fBunchPer * (fFabricHeight / (fp) dwNumRipples) / 4.0 * .8;;
            // the .8 is a fudge factor to account for the back and forth in X

         if (f < fUnBunchPer)
            p->p[2] = -f * fFabricHeight;
         else
            p->p[2] = -pow((fp)((f - fUnBunchPer)/fBunchPer), (fp).5) * fBunch -
               fUnBunchPer * fFabricHeight;
      }
   }


   
   // calculate the normals
   CPoint p1, p2;
   p = pNormal;
   for (j = 0; j < dwNumV; j++) {
      for (i = 0; i < dwNumH; i++, p++) {
         // right vs. left
         DWORD dw1, dw2;
         dw1 = (i > 0) ? (i-1) : 0;
         dw2 = (i < dwNumH-1) ? (i+1) : i;
         p1.Subtract (&pPoint[dw1 + j * dwNumH], &pPoint[dw2 + j * dwNumH]);

         // top vs down
         dw1 = (j > 0) ? (j - 1) : 0;
         dw2 = (j < dwNumV-1) ? (j+1) : j;
         p2.Subtract (&pPoint[i + dw1 * dwNumH], &pPoint[i + dw2 * dwNumH]);

         p->CrossProd (&p2, &p1);
         p->Normalize();
      }
   }


   // all the vertices
   PVERTEX pv;
   pv = pVert;
   for (i = 0; i < m_dwNumVertex; i++, pv++) {
      pv->dwNormal = i;
      pv->dwPoint = i;
      pv->dwTexture = i;
   }



   // and the polygons
   DWORD *padw;
   padw = pPoly;

   for (j = 0; j < dwNumV-1; j++) {
      for (i = 0; i < dwNumH-1; i++, padw+=4) {
         padw[0] = i + j * dwNumH;   // UL
         padw[1] = i+1 + j * dwNumH;  // UR
         padw[2] = i+1+ (j+1)*dwNumH; // LR
         padw[3] = i+ (j+1)*dwNumH; // LL
      }
   }

   return TRUE;
}


/**********************************************************************************
CObjectCurtain::GenerateFolding2Curtain - Fills the m_dwNumPoints, m_memPoints, etc.
with information to draw the hanging curtain.

The curtain's UL corner is -fFabricWidth/2,0,0 (centered Y-wisze). It extends (and closes) to
the right. It falls down to -Z values.

inputs
   fp       fOpen - Amount that open, from 0 (closed) to 1 (open)
   fp       fFabricWidth - Width of the fabric
   fp       fFabricHeight - Height of the fabric  (when fully flattened out)
   fp       fRippleLen - Length of each ripple in the curtain
returns
   BOOL - TRUE if success
*/
BOOL CObjectCurtain::GenerateFolding2Curtain (fp fOpen, fp fFabricWidth, fp fFabricHeight,
                                             fp fRippleLen)
{
   // some limits
   fOpen = max(0,fOpen);
   fOpen = min(1, fOpen);
   fFabricWidth = max(CLOSE, fFabricWidth);
   fFabricHeight = max(CLOSE, fFabricHeight);
   fRippleLen = max(.01, fRippleLen);

   // zero out
   m_dwNumPoints = m_dwNumNormals = m_dwNumText = 0;
   m_dwNumVertex = m_dwNumPoly = 0;

   // calculate
   DWORD dwNumRipples;
   dwNumRipples = (DWORD)ceil(fFabricHeight / fRippleLen);
   dwNumRipples = max(1,dwNumRipples);
   fRippleLen = fFabricHeight / (fp) dwNumRipples;

   // how many points horizontal?
   DWORD dwNumH, dwNumV;
   dwNumH = 2;
   dwNumV = dwNumRipples * RIPPLEDETAIL + 1;

   // allocate the points
   // so, we know how many points, etc. we need
   m_dwNumPoints = dwNumH * dwNumV;
   m_dwNumNormals = m_dwNumPoints;
   m_dwNumText = m_dwNumPoints;
   m_dwNumVertex = m_dwNumPoints;
   m_dwNumPoly = (dwNumH-1) * (dwNumV-1);

   // make sure have enough memory
   if (!m_memPoints.Required (m_dwNumPoints * sizeof(CPoint)))
      return FALSE;
   if (!m_memNormals.Required (m_dwNumNormals * sizeof(CPoint)))
      return FALSE;
   if (!m_memText.Required (m_dwNumText * sizeof(TEXTPOINT5)))
      return FALSE;
   if (!m_memVertex.Required (m_dwNumVertex * sizeof(VERTEX)))
      return FALSE;
   if (!m_memPoly.Required (m_dwNumPoly * sizeof(DWORD) * 4))
      return FALSE;

   // pointers to these
   PCPoint pPoint, pNormal;
   PTEXTPOINT5 pText;
   PVERTEX pVert;
   DWORD *pPoly;
   pPoint = (PCPoint) m_memPoints.p;
   memset (pPoint, 0 ,sizeof (CPoint) * m_dwNumPoints);
   pNormal = (PCPoint) m_memNormals.p;
   memset (pNormal, 0, sizeof(CPoint) * m_dwNumNormals);
   pText = (PTEXTPOINT5) m_memText.p;
   memset (pText, 0, sizeof(TEXTPOINT5) * m_dwNumText);
   pVert = (PVERTEX) m_memVertex.p;
   memset (pVert, 0, sizeof(VERTEX) * m_dwNumVertex);
   pPoly = (DWORD*) m_memPoly.p;
   memset (pPoly, 0, m_dwNumPoly * 4 * sizeof(DWORD));

   // calculate the points and TEXTPOINT5s
   PCPoint p;
   PTEXTPOINT5 pt;
   p = pPoint;
   pt = pText;
   DWORD i, j;
   for (j = 0; j < dwNumV; j++) {
      for (i = 0; i < dwNumH; i++, p++, pt++) {
         // how far down the chain?
         fp f = (fp)j / (fp)(dwNumV-1);

         // what angle
         fp fMod = (fp) (j % RIPPLEDETAIL) / (fp) RIPPLEDETAIL;
         fp fAngle = ((fMod < 2.0 / 3.0) ? (fMod * 3.0 / 4.0) : ((fMod - 2.0 / 3.0) * 3.0 / 2.0 + .5)) * PI;

         // texture, for top and bottom
         pt->hv[0] = (fp) i / (fp) (dwNumH-1) * fFabricWidth - fFabricWidth / 2;
         pt->hv[1] = f * fFabricHeight;

         // not really sure how to do volumetric curtains so linking to flat version
         pt->xyz[0] = pt->hv[0];
         pt->xyz[1] = -pt->hv[1];
         pt->xyz[2] = 0;

#define STANDOUT     .25

         fp fVert, fBendZ, fBendZ2;

         p->p[0] = pt->hv[0];


         fp fAtOneThird = fRippleLen * sqrt((fp)3) / 3.0;
         fp fAtZero = fRippleLen * 2.0 / 3.0;
         fp fAtOne = fRippleLen / 3.0;
         fp fC;
         if (fOpen < 1.0 / 3.0) {
            fC = fOpen * 3;
            fBendZ = sqrt (4 - fC * fC);
            fBendZ2 = fBendZ + sqrt (1 - fC * fC);
         }
         else {
            fC = 1.0 - (fOpen * 3 - 1) / 2.0;
            fBendZ = sqrt (4 - fC * fC);
            fBendZ2 = fBendZ - sqrt (1 - fC * fC);
         }
         fBendZ *= fRippleLen / 3.0;
         fBendZ2 *= fRippleLen / 3.0;
         fC = (fC + fOpen * .8) / (1 + fOpen * .8);   // just so isn't completely flat
         fC *= fRippleLen / 3.0;

         p->p[1] = -sin(fAngle) * fC;

         //fBendZ2 = fOpen * fRippleLen / 3 + (1.0 - fOpen) * fRippleLen;;
         fVert = floor((fp)(j / RIPPLEDETAIL)) * fBendZ2;
         if (fMod < 2.0 / 3.0)
            fVert += fMod * 3.0 / 2.0 * fBendZ;
         else {
            fp fAmt;
            fAmt = (1.0 - fMod) * 3;   // if 2/3 then = 1, if 1 then 0
            fVert += fAmt * fBendZ + (1.0 - fAmt) * fBendZ2;
         }

         p->p[2] = -fVert;

      }
   }


   
   // calculate the normals
   CPoint p1, p2;
   p = pNormal;
   for (j = 0; j < dwNumV; j++) {
      for (i = 0; i < dwNumH; i++, p++) {
         // right vs. left
         DWORD dw1, dw2;
         dw1 = (i > 0) ? (i-1) : 0;
         dw2 = (i < dwNumH-1) ? (i+1) : i;
         p1.Subtract (&pPoint[dw1 + j * dwNumH], &pPoint[dw2 + j * dwNumH]);

         // top vs down
         dw1 = (j > 0) ? (j - 1) : 0;
         dw2 = (j < dwNumV-1) ? (j+1) : j;
         p2.Subtract (&pPoint[i + dw1 * dwNumH], &pPoint[i + dw2 * dwNumH]);

         p->CrossProd (&p2, &p1);
         p->Normalize();
      }
   }


   // all the vertices
   PVERTEX pv;
   pv = pVert;
   for (i = 0; i < m_dwNumVertex; i++, pv++) {
      pv->dwNormal = i;
      pv->dwPoint = i;
      pv->dwTexture = i;
   }



   // and the polygons
   DWORD *padw;
   padw = pPoly;

   for (j = 0; j < dwNumV-1; j++) {
      for (i = 0; i < dwNumH-1; i++, padw+=4) {
         padw[0] = i + j * dwNumH;   // UL
         padw[1] = i+1 + j * dwNumH;  // UR
         padw[2] = i+1+ (j+1)*dwNumH; // LR
         padw[3] = i+ (j+1)*dwNumH; // LL
      }
   }

   return TRUE;
}

/**********************************************************************************
CObjectCurtain::RenderCurtain - Draws the curtain in m_memPoints, etc.

inputs
   POBJECTRENDER     pr - Render information
   PCRenderSurface   prs - Render surface
   BOOL              fFlip - if TRUE, flip curtain on X axis
*/
void CObjectCurtain::RenderCurtain (POBJECTRENDER pr, PCRenderSurface prs, BOOL fFlip)
{
   PCPoint pPoint, pNormal;
   PTEXTPOINT5 pText;
   PVERTEX pVert;
   DWORD dwPointIndex, dwNormalIndex, dwTextIndex, dwVertIndex;

   // The following code is for blinds where draws the end bits
   switch (m_dwShape) {
   case 60: // blinds, retractable, both sides
   case 61: // blinds, retractable, left
   case 62: // blinds, retractable, right
   case 42: // blinds, roman
   case 63: // blinds, retractable, hanging
      {
         if (fFlip) {
            prs->Push();

            PCSpline ps;
            PCPoint p;
            ps = m_NoodleCircle.PathSplineGet();
            p = ps->LocationGet (0);
            prs->Translate (0, 0, p->p[2]);
            prs->Rotate (PI, 2);
         }

         // object specific
         prs->SetDefMaterial (m_OSINFO.dwRenderShard, ObjectSurfaceFind (43), m_pWorld);

         m_NoodleCircle.Render (pr, prs);
         m_NoodleSquare.Render (pr, prs);

         if (fFlip)
            prs->Pop();
      }
      break;
   }

   // object specific
   prs->SetDefMaterial (m_OSINFO.dwRenderShard, ObjectSurfaceFind (42), m_pWorld);

   DWORD i;
   pPoint = prs->NewPoints (&dwPointIndex, m_dwNumPoints);
   if (pPoint) {
      memcpy (pPoint, m_memPoints.p, m_dwNumPoints * sizeof(CPoint));

      if (fFlip) for (i = 0; i < m_dwNumPoints; i++)
         pPoint[i].p[0] *= -1;
   }

   pNormal = prs->NewNormals (TRUE, &dwNormalIndex, m_dwNumNormals);
   if (pNormal) {
      memcpy (pNormal, m_memNormals.p, m_dwNumNormals * sizeof(CPoint));

      if (fFlip) for (i = 0; i < m_dwNumNormals; i++)
         pNormal[i].p[0] *= -1;
   }

   pText = prs->NewTextures (&dwTextIndex, m_dwNumText);
   if (pText) {
      memcpy (pText, m_memText.p, m_dwNumText * sizeof(TEXTPOINT5));

      if (fFlip) for (i = 0; i < m_dwNumText; i++)
         pText[i].hv[0] *= -1;

      prs->ApplyTextureRotation (pText, m_dwNumText);
   }

   pVert = prs->NewVertices (&dwVertIndex, m_dwNumVertex);
   if (pVert) {
      PVERTEX pvs, pvd;
      pvs = (PVERTEX) m_memVertex.p;
      pvd = pVert;
      for (i = 0; i < m_dwNumVertex; i++, pvs++, pvd++) {
         pvd->dwNormal = pNormal ? (pvs->dwNormal + dwNormalIndex) : 0;
         pvd->dwPoint = pvs->dwPoint + dwPointIndex;
         pvd->dwTexture = pText ? (pvs->dwTexture + dwTextIndex) : 0;
      }
   }

   DWORD *padw;
   padw = (DWORD*) m_memPoly.p;
   for (i = 0; i < m_dwNumPoly; i++, padw += 4) {
      if (padw[3] == -1) {
         if (fFlip)
            prs->NewTriangle (dwVertIndex + padw[2], dwVertIndex + padw[1],
               dwVertIndex + padw[0], FALSE);
         else
            prs->NewTriangle (dwVertIndex + padw[0], dwVertIndex + padw[1],
               dwVertIndex + padw[2], FALSE);
      }
      else {
         if (fFlip)
            prs->NewQuad (dwVertIndex + padw[3], dwVertIndex + padw[2],
               dwVertIndex + padw[1], dwVertIndex + padw[0], FALSE);
         else
            prs->NewQuad (dwVertIndex + padw[0], dwVertIndex + padw[1],
               dwVertIndex + padw[2], dwVertIndex + padw[3], FALSE);
      }
   }
   
}



/**********************************************************************************
CObjectCurtain::OpenGet - 
returns how Open the object is, from 0 (closed) to 1.0 (Open), or
< 0 for an object that can't be Opened
*/
fp CObjectCurtain::OpenGet (void)
{
   return m_fOpen;
}

/**********************************************************************************
CObjectCurtain::OpenSet - 
Opens/closes the object. if fOpen==0 it's close, 1.0 = Open, and
values in between are partially Opened closed. Returne TRUE if success
*/
BOOL CObjectCurtain::OpenSet (fp fOpen)
{
   fOpen = max(0,fOpen);
   fOpen = min(1,fOpen);
   if (fOpen == m_fOpen)
      return TRUE;

   m_pWorld->ObjectAboutToChange (this);
   m_fOpen = fOpen;
   CalcInfo ();
   m_pWorld->ObjectChanged (this);

   return TRUE;
}



/*************************************************************************************
CObjectCurtain::ControlPointQuery - Called to query the information about a control
point identified by dwID.

inputs
   DWORD       dwID - ID
   POSCONTROL  pInfo - Filled in with the infomration
returns
   BOOL - TRUE if successful
*/
BOOL CObjectCurtain::ControlPointQuery (DWORD dwID, POSCONTROL pInfo)
{
   fp fKnobSize = max(m_pSize.p[0], m_pSize.p[1]) / 50;

   if (dwID >= 2)
      return FALSE;

   memset (pInfo,0, sizeof(*pInfo));

   pInfo->dwID = dwID;
//   pInfo->dwFreedom = 0;   // any direction
   pInfo->dwStyle = CPSTYLE_CUBE;
   pInfo->fSize = fKnobSize;
   pInfo->cColor = RGB(0,0xff,0xff);
   switch (dwID) {
   case 0:
   default:
      wcscpy (pInfo->szName, L"Width");
      break;
   case 1:
      wcscpy (pInfo->szName, L"Height");
      break;
   }
   MeasureToString (m_pSize.p[dwID], pInfo->szMeasurement);

   pInfo->pLocation.Zero();
   pInfo->pLocation.p[2] = -m_pSize.p[1]/2;
   if (dwID)
      pInfo->pLocation.p[2] = -m_pSize.p[1];
   else
      pInfo->pLocation.p[0] = m_pSize.p[0] / 2.0;

   return TRUE;
}

/*************************************************************************************
CObjectCurtain::ControlPointSet - Called to change the valud of a control point.

inputs
   DWORD       dwID - ID
   PCPoint     pVal - Contains the new location, in object coordinates
returns
   BOOL - TRUE if successful
*/
BOOL CObjectCurtain::ControlPointSet (DWORD dwID, PCPoint pVal, PCPoint pViewer)
{
   if (dwID >= 2)
      return FALSE;

   CPoint pNew;
   pNew.Copy (pVal);
   pNew.p[0] = max(CLOSE, pNew.p[0]);
   pNew.p[2] = max(CLOSE, -pNew.p[2]);
   // tell the world we're about to change
   if (m_pWorld)
      m_pWorld->ObjectAboutToChange (this);

   if (dwID)
      m_pSize.p[1] = pNew.p[2];
   else
      m_pSize.p[0] = pNew.p[0]*2;

   CalcInfo ();

   // tell the world we've changed
   if (m_pWorld)
      m_pWorld->ObjectChanged (this);

   return TRUE;
}

/*************************************************************************************
CObjectCurtain::ControlPointEnum - Called to enumerate a list of control point IDs
into the list.

inputs
   PCListFixed       plDWORD - Should be filled in with control points
returns
   none
*/
void CObjectCurtain::ControlPointEnum (PCListFixed plDWORD)
{
   DWORD i;

   DWORD dwNum = 2;
   for (i = 0; i < dwNum; i++)
      plDWORD->Add (&i);
}


