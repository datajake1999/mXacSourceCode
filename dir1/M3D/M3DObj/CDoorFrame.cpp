/**********************************************************************************
CDoorFrame.cpp - C++ object that mamanges doors and window frames and draws them.

begun 24/4/2002 by Mike Rozak
Copyright 2002 Mike Rozak. All rights reserved
*/

#include <windows.h>
#include "escarpment.h"
#include "..\M3D.h"
#include "resource.h"


/***********************************************************************************
CDoorFrame::Constructor and destructor */
CDoorFrame::CDoorFrame (void)
{
   m_dwShape = DFS_RECTANGLE;
   m_dwDoor = DFD_DOOR;
   m_fDirty = TRUE;
   m_pFrameSize.Zero();
   m_pFrameSize.p[0] = CM_FRAMEWIDTH;
   m_pFrameSize.p[1] = CM_FRAMEDEPTH;
   m_pSubFrameSize.p[0] = CM_SUBFRAMEWIDTH;
   m_pSubFrameSize.p[1] = CM_SUBFRAMEDEPTH;
   m_lOPENINGINFO.Init (sizeof(OPENINGINFO));
   m_lMeasurement.Init (sizeof(fp));
   DWORD k;
   for (k = 0; k < 2; k++)
      m_alNoodles[k].Init (sizeof(PCNoodle));
}

CDoorFrame::~CDoorFrame (void)
{
   // delete stuff
   DWORD i;
   for (i = 0; i < m_lOPENINGINFO.Num(); i++) {
      POPENINGINFO pi = (POPENINGINFO) m_lOPENINGINFO.Get(i);
      if (pi->pSpline)
         delete pi->pSpline;
   }
   m_lOPENINGINFO.Clear();
   DWORD k;
   for (k = 0; k < 2; k++) {
      for (i = 0; i < m_alNoodles[k].Num(); i++) {
         PCNoodle pn= *((PCNoodle*) m_alNoodles[k].Get(i));
         delete pn;
      }
      m_alNoodles[k].Clear();
   }
}



static PWSTR gpszDoorFrame = L"DoorFrame";
static PWSTR gpszShape = L"Shape";
static PWSTR gpszDoor = L"Door";
static PWSTR gpszFrameSize = L"FrameSize";
static PWSTR gpszSubFrameSize = L"SubFrameSize";

/***********************************************************************************
CDoorFrame::MMLTo - Writes the door frame to MML node and resturns that node

retursn
   PCMMLNode2 - Node with door frame information. NULL if error
*/
PCMMLNode2 CDoorFrame::MMLTo (void)
{
   PCMMLNode2 pNode = new CMMLNode2;
   if (!pNode)
      return NULL;
   pNode->NameSet (gpszDoorFrame);


   MMLValueSet (pNode, gpszShape, (int) m_dwShape);
   MMLValueSet (pNode, gpszDoor, (int) m_dwDoor);
   MMLValueSet (pNode, gpszFrameSize, &m_pFrameSize);
   MMLValueSet (pNode, gpszSubFrameSize, &m_pSubFrameSize);

   WCHAR szTemp[32];
   DWORD i;
   for (i = 0; i < m_lOPENINGINFO.Num(); i++) {
      POPENINGINFO pi = (POPENINGINFO) m_lOPENINGINFO.Get(i);
      swprintf (szTemp, L"OIBase%d", (int) i);
      MMLValueSet (pNode, szTemp, pi->iBase);
   }
   for (i = 0; i < m_lMeasurement.Num(); i++) {
      fp f = *((fp*) m_lMeasurement.Get(i));
      swprintf (szTemp, L"Measure%d", (int) i);
      MMLValueSet (pNode, szTemp, f);
   }

   // dont bother writing m_fDirty
   // dont write sopeninginfo's spline
   // dont write m_lnoodles
   // dont write m_loutside

   return pNode;
}

/***********************************************************************************
CDoorFrame::MMLFrom - Reads the door frame informathin from the MML node

iputs
   PCMMLNode2      pNode - inputs
returns
   BOOL - TRUE if success
*/
BOOL CDoorFrame::MMLFrom (PCMMLNode2 pNode)
{
   // free up all current stuff
   DWORD i;
   for (i = 0; i < m_lOPENINGINFO.Num(); i++) {
      POPENINGINFO pi = (POPENINGINFO) m_lOPENINGINFO.Get(i);
      if (pi->pSpline)
         delete pi->pSpline;
   }
   m_lOPENINGINFO.Clear();
   DWORD k;
   for (k = 0; k < 2; k++) {
      for (i = 0; i < m_alNoodles[k].Num(); i++) {
         PCNoodle pn= *((PCNoodle*) m_alNoodles[k].Get(i));
         delete pn;
      }
      m_alNoodles[k].Clear();
   }
   m_fDirty = TRUE;
   m_lMeasurement.Clear();

   m_dwShape = (DWORD) MMLValueGetInt (pNode, gpszShape, DFS_RECTANGLE);
   m_dwDoor = (DWORD) MMLValueGetInt (pNode, gpszDoor, DFD_WINDOW);
   CPoint pZero;
   pZero.Zero();
   pZero.p[0] = pZero.p[1] = .01;
   MMLValueGetPoint (pNode, gpszFrameSize, &m_pFrameSize, &pZero);
   MMLValueGetPoint (pNode, gpszSubFrameSize, &m_pSubFrameSize, &pZero);

   WCHAR szTemp[32];
   OPENINGINFO oi;
   memset (&oi, 0 ,sizeof(oi));
   for (i = 0;; i++) {
      swprintf (szTemp, L"OIBase%d", (int) i);
      oi.iBase = MMLValueGetInt (pNode, szTemp, -1);
      if (oi.iBase == -1)
         break;

      // else add
      m_lOPENINGINFO.Add (&oi);
   }
   for (i = 0;; i++) {
      fp fVal;
      swprintf (szTemp, L"Measure%d", (int) i);
      fVal = MMLValueGetDouble (pNode, szTemp, -1);
      if (fVal == -1)
         break;

      // else add it
      m_lMeasurement.Add (&fVal);
   }

   // dont bother getting m_fDirty
   // dont write sopeninginfo's spline
   // dont write m_lnoodles
   // dont write m_loutside

   return TRUE;
}


/***********************************************************************************
CDoorFrame::CloneTo - Clones the door frame to another door frame.

inputs
   CDoorFrame     *pNew - Where to clone to
returns
   BOOL - TRUE if success
*/
BOOL CDoorFrame::CloneTo (CDoorFrame *pNew)
{
   // free up all current stuff
   DWORD i;
   for (i = 0; i < pNew->m_lOPENINGINFO.Num(); i++) {
      POPENINGINFO pi = (POPENINGINFO) pNew->m_lOPENINGINFO.Get(i);
      if (pi->pSpline)
         delete pi->pSpline;
   }
   pNew->m_lOPENINGINFO.Clear();
   DWORD k;
   for (k = 0; k < 2; k++) {
      for (i = 0; i < pNew->m_alNoodles[k].Num(); i++) {
         PCNoodle pn= *((PCNoodle*) pNew->m_alNoodles[k].Get(i));
         delete pn;
      }
      pNew->m_alNoodles[k].Clear();
   }
   pNew->m_fDirty = TRUE;
   pNew->m_lMeasurement.Clear();

   pNew->m_dwShape = m_dwShape;
   pNew->m_dwDoor = m_dwDoor;
   pNew->m_fDirty = m_fDirty;
   pNew->m_pFrameSize.Copy (&m_pFrameSize);
   pNew->m_pSubFrameSize.Copy (&m_pSubFrameSize);
   pNew->m_lOPENINGINFO.Init (sizeof(OPENINGINFO), m_lOPENINGINFO.Get(0), m_lOPENINGINFO.Num());
   for (i = 0; i < pNew->m_lOPENINGINFO.Num(); i++) {
      POPENINGINFO pi = (POPENINGINFO) pNew->m_lOPENINGINFO.Get(i);
      if (pi->pSpline) {
         PCSpline ps;
         ps = pi->pSpline;
         pi->pSpline = new CSpline;
         if (pi->pSpline)
            ps->CloneTo (pi->pSpline);
      }
   }
   pNew->m_lMeasurement.Init (sizeof(fp), m_lMeasurement.Get(0), m_lMeasurement.Num());
   for (k = 0; k < 2; k++) {
      pNew->m_alNoodles[k].Required (m_alNoodles[k].Num());
      for (i = 0; i < m_alNoodles[k].Num(); i++) {
         PCNoodle pn, pn2;
         pn = *((PCNoodle*) m_alNoodles[k].Get(i));
         pn2 = pn->Clone();
         pNew->m_alNoodles[k].Add (&pn2);
      }
   }
   m_lOutside.CloneTo (&pNew->m_lOutside);

   return TRUE;
}


/***********************************************************************************
CDoorFrame::Clone - Clones the door frame and returns a new one.

returns
   CDoorFrame* - New one. Must be freed by caller
*/
CDoorFrame *CDoorFrame::Clone (void)
{
   PCDoorFrame pNew = new CDoorFrame;
   if (!pNew)
      return NULL;
   if (!CloneTo (pNew)) {
      delete pNew;
      return NULL;
   }
   return pNew;
}

/***********************************************************************************
CDoorFrame::CheckOpenings - Since the number of openings sometimes depends on the
parameters settings, this is called to make sure they agree.
*/
void CDoorFrame::CheckOpenings (void)
{
   // fill in the default measurements
   DWORD dwNumOpen;
   int   aiOpen[32];
   dwNumOpen = 0;
   fp *paf = (fp*) m_lMeasurement.Get(0);
   DWORD dwNumMeas = m_lMeasurement.Num();
   DWORD i;

   switch (LOWORD(m_dwShape)) {
   case DFS_RECTANGLE:
   case DFS_RECTANGLEPEAK:
   case DFS_ARCH:
   case DFS_ARCHHALF:
   case DFS_ARCHHALF2:
   case DFS_ARCH2:
   case DFS_ARCHCIRCLE:
   case DFS_ARCHPEAK:
   case DFS_ARCHPARTCIRCLE:
   case DFS_ARCHTRIANGLE:
   case DFS_TRIANGLERIGHT:
   case DFS_TRIANGLEEQUI:
   case DFS_PENTAGON:
   case DFS_HEXAGON:
   case DFS_OCTAGON:
      dwNumOpen = 1;
      aiOpen[0] = (m_dwDoor == DFD_DOOR) ? 0 : 2;  // opening
      break;

   case DFS_REPEATNUMTOP:
   case DFS_REPEATNUM:
      {
         BOOL fTopFrames = (LOWORD(m_dwShape) == DFS_REPEATNUMTOP);
         DWORD dwNum;

         dwNumOpen = max(1,(int) ((dwNumMeas > 2) ? paf[2] : 1));
         dwNumOpen = min(16,dwNumOpen);
         dwNum = dwNumOpen;
         if (fTopFrames)
            dwNumOpen *= 2;
         for (i = 0; i < dwNum; i++) {
            aiOpen[i] = (m_dwDoor == DFD_DOOR) ? 0 : 2;

            if (fTopFrames)
               aiOpen[i+dwNum] = 2;
         }
      }
      break;

   case DFS_REPEATMINTOP:
   case DFS_REPEATMIN:
      {
         BOOL fTopFrames = (LOWORD(m_dwShape) == DFS_REPEATMINTOP);
         DWORD dwNum;
         fp fWidth;

         fWidth = ((dwNumMeas > 2) ? paf[2] : 1) + m_pSubFrameSize.p[0];
         fWidth = max(.1, fWidth);

         dwNumOpen = (DWORD) ceil(paf[0] / fWidth);
         dwNumOpen = max(1,dwNumOpen);
         dwNumOpen = min(16,dwNumOpen);

         dwNum = dwNumOpen;
         if (fTopFrames)
            dwNumOpen *= 2;
         for (i = 0; i < dwNum; i++) {
            aiOpen[i] = (m_dwDoor == DFD_DOOR) ? 0 : 2;

            if (fTopFrames)
               aiOpen[i+dwNum] = 2;
         }
      }
      break;

   case DFS_CIRCLE:
      dwNumOpen = 1;
      aiOpen[0] = 2;  // opening
      break;

   case DFS_ARCHSPLIT:
   case DFS_RECTANGLEVENT:
      dwNumOpen = 2;
      aiOpen[0] = (m_dwDoor == DFD_DOOR) ? 0 : 2;  // opening
      aiOpen[1] = 2; // cant open this
      break;

   case DFS_RECTANGLELRLITES:
   case DFS_TRAPEZOID1:
      dwNumOpen = 3;
      aiOpen[0] = (m_dwDoor == DFD_DOOR) ? 0 : 2;  // opening
      aiOpen[1] = aiOpen[2] = (m_dwDoor == DFD_DOOR) ? 1 : 2;  // could be opened, but not usually
      break;

   case DFS_TRAPEZOID2:
      dwNumOpen = 2;
      aiOpen[0] = (m_dwDoor == DFD_DOOR) ? 0 : 2;  // opening
      aiOpen[1] = (m_dwDoor == DFD_DOOR) ? 1 : 2;  // could be opened, but not usually
      break;

   case DFS_RECTANGLELITEVENT:
      dwNumOpen = 3;
      aiOpen[0] = (m_dwDoor == DFD_DOOR) ? 0 : 2;  // opening
      aiOpen[1] = (m_dwDoor == DFD_DOOR) ? 1 : 2;  // could be opened, but not usually
      aiOpen[2]= 2; // cant open this
      break;

   case DFS_RECTANGLELRLITEVENT:
   case DFS_ARCHLRLITES:
   case DFS_ARCHLRLITES2:
   case DFS_ARCHLRLITES3:
      dwNumOpen = 4;
      aiOpen[0] = (m_dwDoor == DFD_DOOR) ? 0 : 2;  // opening
      aiOpen[1] = aiOpen[2] = (m_dwDoor == DFD_DOOR) ? 1 : 2;  // could be opened, but not usually
      aiOpen[3] = 2; // cant open this
      break;

   default:
      return;
   }

   DWORD dwNum;
   dwNum = m_lOPENINGINFO.Num();

   // go through existing openings. If any have changed to state 2 then enforce
   for (i = 0; i < (dwNum) && (i < dwNumOpen); i++) {
      POPENINGINFO pi = (POPENINGINFO) m_lOPENINGINFO.Get(i);
      if ((aiOpen[i] == 2) && (pi->iBase != 2)) {
         pi->iBase = aiOpen[i];
         m_fDirty = TRUE;
      }
      else if ((aiOpen[i] != 2) && (pi->iBase == 2)) {
         pi->iBase = aiOpen[i];
         m_fDirty = TRUE;
      }
   }

   if (dwNum == dwNumOpen)
      return;  // no change

   // it's dirty if gets here
   m_fDirty = TRUE;

   // may need to add
   if (dwNum > dwNumOpen) {
      for (i = dwNum-1; (i < dwNum) && (i >= dwNumOpen); i++) {
         POPENINGINFO pi = (POPENINGINFO) m_lOPENINGINFO.Get(i);
         if (pi->pSpline)
            delete pi->pSpline;
         m_lOPENINGINFO.Remove (i);
      }
   }
   else {
      // add
      // fill in openings
      OPENINGINFO oi;
      memset (&oi, 0, sizeof(oi));
      for (i = dwNum; i < dwNumOpen; i++) {
         oi.iBase = aiOpen[i];
         m_lOPENINGINFO.Add (&oi);
      }
   }
}

/***********************************************************************************
CDoorFrame::Init - Initializes the door frame based on the shape and whether it's
a door or not. (Init() may be called several times.)

inputs
   DWORD       dwShape - Shape of the frame. DFS_XXX
                  (Combine with 0x10000 to reverse along Z axis)
   BOOL        dwDoor - DFD_XXX to indicate if door, window, or frame
               Doors are special because their bottom Z'level is at 0.0.
   DWORD       dwSetStyle - Optional. If non-zero, this is looked at and analyzed for
               what size some of the gaps should be based on the door set going to
               be placed in.
returns
   BOOL - TRUE if success
*/
BOOL CDoorFrame::Init (DWORD dwShape, DWORD dwDoor, DWORD dwSetStyle)
{
   m_fDirty = TRUE;
   m_dwShape = dwShape;
   m_dwDoor = dwDoor;

   // clear out odl info
   DWORD i;
   for (i = 0; i < m_lOPENINGINFO.Num(); i++) {
      POPENINGINFO pi = (POPENINGINFO) m_lOPENINGINFO.Get(i);
      if (pi->pSpline)
         delete pi->pSpline;
      pi->pSpline = NULL;
   }
   m_lOPENINGINFO.Clear();

   // look at the set style and determine if have bifold doors, fp, etc.
   fp fScaleWidth, fScaleHeight;
   fScaleWidth = 1.0;   // default width scale
   fScaleHeight = 1.0;  // default height scale
   switch (dwSetStyle & DSSTYLE_BITS) {
   case DSSTYLE_FIXED:   // fixed
   case DSSTYLE_HINGEL:   // hinged, left, 1 division
   case DSSTYLE_HINGER:   // hinged, right, 1 division
   case DSSTYLE_HINGELO:   // hinged, left, 1 division
   case DSSTYLE_HINGERO:   // hinged, right, 1 division
   case DSSTYLE_HINGELR2:   // hinged, left/right, 2 divisions, custom shape - saloon door
      break;
   case DSSTYLE_HINGEL2:   // hinged, left, 1 division
      if (m_dwDoor == DFD_DOOR)
         fScaleWidth *= CM_EXTERIORDOORWIDTH / CM_DOORWIDTH;
      break;
   case DSSTYLE_HINGELR:   // hinged, left and right, 2 divisions
   case DSSTYLE_HINGELRO:   // hinged, left and right, 2 divisions
      if (m_dwDoor != DFD_WINDOW)
         fScaleWidth *= 2;
      break;
   case DSSTYLE_HINGEU:   // hinged, up, 1 dicision
      break;
   case DSSTYLE_BIL:   // bifold, left 4 divisions
      fScaleWidth *= 2;
      break;
   case DSSTYLE_BILR:   // bifold, left/right 4 divisions
      fScaleWidth *= 3;
      break;
   case DSSTYLE_SLIDEL:   // slide, left, 2 divisions
      if (m_dwDoor == DFD_DOOR)
         fScaleWidth = 1.80 / CM_DOORWIDTH;  // so will be 6' sliding doors
      else
         fScaleWidth *= 2;
      break;
   case DSSTYLE_SLIDEU:   // slide, up, 2 divisions
      if (!m_dwDoor == DFD_DOOR)
         fScaleHeight *= 2;
      break;
   case DSSTYLE_POCKLR:   // pocket, left/right, 2 divisions
      fScaleWidth *= 2;
      break;
   case DSSTYLE_POCKLR2:  // pocket, left/right, 2 divisions, on outside
   case DSSTYLE_GARAGEU:   // garage, up, 4 divisions
   case DSSTYLE_GARAGEU2:   // garage up, 1 division
   case DSSTYLE_ROLLERU:   // roller up, 8 divisions
      if (m_dwDoor == DFD_DOOR) {
         fScaleWidth = 2.40 / CM_DOORWIDTH;  // so will be 8' garage doors
         fScaleHeight = 2.40 / CM_DOORHEIGHT;   // so will be 8' high
      }
      break;
   case DSSTYLE_POCKL:   // pocket, left, 1 division
   case DSSTYLE_POCKU:   // pocket up, 1 division
   case DSSTYLE_LOUVER:   // pivot, up(?), division for louver height
      break;
   }

   // fill in the default measurements
   DWORD dwNumMeas;
   fp afMeas[16];
   dwNumMeas = 0;
   fp fWidth, fHeight;
   switch (m_dwDoor) {
   case DFD_DOOR:
   default:
      fWidth = CM_DOORWIDTH;
      fHeight = CM_DOORHEIGHT;
      break;
   case DFD_WINDOW:
      fWidth = CM_WINDOWWIDTH;
      fHeight = CM_WINDOWHEIGHT;
      break;
   case DFD_CABINET:
      fWidth = CM_CABINETDOORWIDTH;
      fHeight = CM_CABINETDOORHEIGHT;
      break;
   }
   switch (LOWORD(m_dwShape)) {
   case DFS_RECTANGLE:
   case DFS_ARCHCIRCLE:
   case DFS_TRIANGLERIGHT:
      dwNumMeas = 2;
      afMeas[0] = fScaleWidth * fWidth;  // standard width
      afMeas[1] = fScaleHeight * fHeight;   // standard height
      break;

   case DFS_TRIANGLEEQUI:
   case DFS_PENTAGON:
   case DFS_HEXAGON:
   case DFS_OCTAGON:
      dwNumMeas = 2;
      afMeas[0] = fScaleWidth * fHeight;  // standard width
      afMeas[1] = fScaleHeight * fHeight;   // standard height
      // keep them the same
      break;

   case DFS_REPEATNUMTOP:
   case DFS_REPEATNUM:
      dwNumMeas = (LOWORD(m_dwShape) == DFS_REPEATNUM) ? 3 : 5;
      afMeas[0] = fScaleWidth * fWidth;  // standard width
      afMeas[1] = fScaleHeight * fHeight;   // standard height
      afMeas[2] = 3;
      afMeas[3] = afMeas[1] / 4;
      afMeas[4] = 0; // afMeas[3];
      break;

   case DFS_REPEATMINTOP:
   case DFS_REPEATMIN:
      dwNumMeas = (LOWORD(m_dwShape) == DFS_REPEATMIN) ? 3 : 5;
      afMeas[0] = (m_dwDoor != DFD_WINDOW) ? (fWidth*3) : fWidth;  // standard width
      afMeas[1] = fScaleHeight * fHeight;   // standard height
      afMeas[2] = fScaleWidth * ((m_dwDoor != DFD_WINDOW) ? fWidth : .5);
      afMeas[3] = afMeas[1] / 4;
      afMeas[4] = 0; // afMeas[3];
      break;

   case DFS_ARCH2:
      dwNumMeas = 2;
      afMeas[0] = fScaleWidth * ((m_dwDoor == DFD_DOOR) ? fHeight*2 : fWidth);  // standard width
      afMeas[1] = fScaleHeight * fHeight;   // standard height
      // make it circular, not elliptical
      break;

   case DFS_ARCHHALF2:
   case DFS_CIRCLE:
      dwNumMeas = 2;
      afMeas[0] = fScaleWidth * fHeight;  // standard width
      afMeas[1] = fScaleHeight * fHeight;   // standard height
               // intentionally make square
      break;

   case DFS_RECTANGLEPEAK:
      dwNumMeas = 3;
      afMeas[0] = fScaleWidth * fWidth;  // standard width
      afMeas[1] = fScaleHeight * fHeight;   // standard height
      afMeas[2] = afMeas[1] + afMeas[1] / 4.0;  // peak goes above left side
      break;

   case DFS_ARCH:
   case DFS_ARCHHALF:
   case DFS_ARCHPEAK:
   case DFS_ARCHPARTCIRCLE:
   case DFS_ARCHTRIANGLE:
      dwNumMeas = 3;
      afMeas[0] = fScaleWidth * fWidth;  // standard width
      afMeas[1] = fScaleHeight * fHeight;   // standard height
      afMeas[2] = afMeas[1] * .75;  // height at edges
      if (LOWORD(m_dwShape) == DFS_ARCHPARTCIRCLE)
         afMeas[2] = afMeas[1] * .25;
      break;

   case DFS_ARCHSPLIT:
   case DFS_RECTANGLEVENT:
      dwNumMeas = 3;
      afMeas[0] = fScaleWidth * fWidth;  // standard width
      afMeas[1] = fScaleHeight * fHeight;   // standard height
      afMeas[2] = afMeas[1] / 4 / fScaleHeight;  // arch height at edges
      break;

   case DFS_RECTANGLELRLITES:
      dwNumMeas = 3;
      afMeas[0] = fScaleWidth * fWidth;  // standard width
      afMeas[1] = fScaleHeight * fHeight;   // standard height
      afMeas[2] = afMeas[0] / 4 / fScaleWidth;  // lite widths
      break;

   case DFS_TRAPEZOID1:
   case DFS_TRAPEZOID2:
      dwNumMeas = 4;
      afMeas[0] = fScaleWidth * fWidth;  // standard width
      afMeas[1] = fScaleHeight * fHeight;   // standard height
      afMeas[2] = afMeas[0] / 2 / fScaleWidth;  // lite widths
      afMeas[3] = afMeas[2] / 2; // bottom
      break;

   case DFS_RECTANGLELITEVENT:
   case DFS_RECTANGLELRLITEVENT:
   case DFS_ARCHLRLITES:
   case DFS_ARCHLRLITES2:
   case DFS_ARCHLRLITES3:
      dwNumMeas = 4;
      afMeas[0] = fScaleWidth * fWidth;  // standard width
      afMeas[1] = fScaleHeight * fHeight;   // standard height
      afMeas[2] = afMeas[0] / 4 / fScaleWidth;  // lite width
      afMeas[3] = afMeas[1] / 4 / fScaleHeight;  // vent height
      break;

   default:
      return FALSE;
   }

   // fill in measurement
   m_lMeasurement.Init (sizeof(fp), afMeas, dwNumMeas);

   CheckOpenings();

   return TRUE;
}


/***********************************************************************************
CDoorFrame::FrameSizeSet - Sets the frame size. Changing this affects the overall
size of the doorway/window.

inputs
   fp      fWidth - Width of the frame.
   fp      fDepth - Depth of the frame. (The frame's Y component wil range from
                  fDepth/2 to -fDepth/2
returns
   BOOL - TRUE if success
*/
BOOL CDoorFrame::FrameSizeSet (fp fWidth, fp fDepth)
{
   if (m_pFrameSize.p[0] != fWidth) {
      m_pFrameSize.p[0] = fWidth;
      m_fDirty = TRUE;
   }
   if (m_pFrameSize.p[1] != fDepth) {
      m_pFrameSize.p[1] = fDepth;
      m_fDirty = TRUE;
   }
   return TRUE;
}

/***********************************************************************************
CDoorFrame::FrameSizeGet - Gets the frame size.

inputs
   fp      *pfWidth - Width of the frame. Can be null.
   fp      *pfDepth - Depth of the frame. Can be null.
returns
   BOOL - TRUE if success
*/
BOOL CDoorFrame::FrameSizeGet (fp *pfWidth, fp *pfDepth)
{
   if (pfWidth)
      *pfWidth = m_pFrameSize.p[0];
   if (pfDepth)
      *pfDepth = m_pFrameSize.p[1];
   return TRUE;
}


/***********************************************************************************
CDoorFrame::SubFrameSizeSet - Sets the subframe size. Changing this affects the overall
size of the doorway/window.

inputs
   fp      fWidth - Width of the frame.
   fp      fDepth - Depth of the frame. (The frame's Y component wil range from
                  fDepth/2 to -fDepth/2
returns
   BOOL - TRUE if success
*/
BOOL CDoorFrame::SubFrameSizeSet (fp fWidth, fp fDepth)
{
   if (m_pSubFrameSize.p[0] != fWidth) {
      m_pSubFrameSize.p[0] = fWidth;
      m_fDirty = TRUE;
   }
   if (m_pSubFrameSize.p[1] != fDepth) {
      m_pSubFrameSize.p[1] = fDepth;
      m_fDirty = TRUE;
   }
   return TRUE;
}

/***********************************************************************************
CDoorFrame::SubFrameSizeGet - Gets the subframe size.

inputs
   fp      *pfWidth - Width of the frame. Can be null.
   fp      *pfDepth - Depth of the frame. Can be null.
returns
   BOOL - TRUE if success
*/
BOOL CDoorFrame::SubFrameSizeGet (fp *pfWidth, fp *pfDepth)
{
   if (pfWidth)
      *pfWidth = m_pSubFrameSize.p[0];
   if (pfDepth)
      *pfDepth = m_pSubFrameSize.p[1];
   return TRUE;
}


/***********************************************************************************
CDoorFrame::OpeningsNum - Returns the number of openings given the shape.

returns
   DWORD - Number of openings
*/
DWORD CDoorFrame::OpeningsNum (void)
{
   CheckOpenings();

   return m_lOPENINGINFO.Num();
}


/***********************************************************************************
CDoorFrame::OpeningsBaseGet - Returns the whether there's a base portion of the frame
on the opening. (There won't be for doors.)

inputs
   DWORD    dwOpening - Opening to use, 0 ..OpeningsNum()-1
returns
   int - 0 if there isn't, 1 if there is, 2 if there is and it can't be changed
*/
int CDoorFrame::OpeningsBaseGet (DWORD dwOpening)
{
   CheckOpenings();

   POPENINGINFO pi;
   pi = (POPENINGINFO) m_lOPENINGINFO.Get(dwOpening);
   if (!pi)
      return 2;
   return pi->iBase;
}

/***********************************************************************************
CDoorFrame::OpeningsBaseSet - Sets whether there's an opening on the base portion
of the frame. (Can't do this is OpeningsBaseGet() returned 2

iunputs
   DWORD    dwOpening - Opening to use, 0 ..OpeningsNum()-1
   int      iBase - 0 if there isn't, 1 if there is
retursn
   BOOL - TRUE if success
*/
BOOL CDoorFrame::OpeningsBaseSet (DWORD dwOpening, int iBase)
{
   POPENINGINFO pi;
   pi = (POPENINGINFO) m_lOPENINGINFO.Get(dwOpening);
   if (!pi || (pi->iBase < 0) || (pi->iBase > 1) || (iBase < 0) || (iBase > 1))
      return FALSE;
   pi->iBase = iBase;
   m_fDirty = TRUE;
   return TRUE;
}

/***********************************************************************************
CDoorFrame::OpeningsSpline - Returns a pointer to the openings spline. This MUST NOT
be changed and is ONLY VALID until something else changes. The spline is around the
inside of the opening, excluding the thickness of the frame.

inputs
   DWORD    dwOpening - Opening to use, 0 ..OpeningsNum()-1
retursn
   PCSpline - Spline.
*/
PCSpline CDoorFrame::OpeningsSpline (DWORD dwOpening)
{
   CalcIfNecessary();

   POPENINGINFO pi;
   pi = (POPENINGINFO) m_lOPENINGINFO.Get(dwOpening);
   if (!pi)
      return NULL;
   return pi->pSpline;
}

/***********************************************************************************
CDoorFrame::MeasNum - Returns the number of measurements, specific to the frame shape.

retursn 
   DWORD - Number
*/
DWORD CDoorFrame::MeasNum (void)
{
   return m_lMeasurement.Num();
}

/***********************************************************************************
CDoorFrame::MeasGet - Returns the measurement value and s string describing it to
the user.

inputs
   DWORD    dwNum - Measurement number, from 0 .. MeasNum()-1
   fp   *pfVal - Filled with the value
   BOOL     *pfDistance - Filled with true if it's a distance, FALSE if its justa  number
returns
   PCWSTR - MML String with tags that provides info. NULL if error
*/
PCWSTR CDoorFrame::MeasGet (DWORD dwNum, fp *pfVal, BOOL *pfDistance)
{
   fp *pf;
   pf = (fp*) m_lMeasurement.Get(dwNum);
   if (!pf)
      return NULL;
   *pfVal = *pf;

   PCWSTR psz;
   psz = NULL;
   *pfDistance = TRUE;  // assume by default

   PCWSTR gpszWidth = L"<bold>Width</bold>";
   PCWSTR gpszHeight = L"<bold>Height</bold>";
   PCWSTR gpszVentHeight = L"<bold>Vent height</bold>";
   PCWSTR gpszSideLiteWidth = L"<bold>Side lite width</bold>";
   PCWSTR gpszArchHeight = L"<bold>Arch height</bold>";
   PCWSTR gpszNumDiv = L"<bold>Number of divisions</bold>";
   PCWSTR gpszHeightLeft = L"<bold>Height of upper panes on left</bold>";
   PCWSTR gpszHeightRight = L"<bold>Height of upper panes on right</bold>";

   switch (LOWORD(m_dwShape)) {
   case DFS_RECTANGLE:
   case DFS_ARCHHALF2:
   case DFS_ARCH2:
   case DFS_CIRCLE:
   case DFS_ARCHCIRCLE:
   case DFS_TRIANGLERIGHT:
   case DFS_TRIANGLEEQUI:
   case DFS_PENTAGON:
   case DFS_HEXAGON:
   case DFS_OCTAGON:
      if (dwNum == 0)   // width
         psz = gpszWidth;
      else if (dwNum == 1)
         psz = gpszHeight;
      break;

   case DFS_REPEATNUMTOP:
   case DFS_REPEATNUM:
      if (dwNum == 0)   // width
         psz = gpszWidth;
      else if (dwNum == 1)
         psz = gpszHeight;
      else if (dwNum == 2) {
         psz = gpszNumDiv;
         *pfDistance = FALSE;
      }
      else if (dwNum == 3)
         psz = gpszHeightLeft;
      else if (dwNum == 4)
         psz = gpszHeightRight;
      break;

   case DFS_REPEATMINTOP:
   case DFS_REPEATMIN:
      if (dwNum == 0)   // width
         psz = L"<bold>Total width</bold>";
      else if (dwNum == 1)
         psz = gpszHeight;
      else if (dwNum == 2)
         psz = L"<bold>Maximum pane width</bold>";
      else if (dwNum == 3)
         psz = gpszHeightLeft;
      else if (dwNum == 4)
         psz = gpszHeightRight;
      break;

   case DFS_RECTANGLEVENT:
      if (dwNum == 0)   // width
         psz = gpszWidth;
      else if (dwNum == 1)
         psz = gpszHeight;
      else if (dwNum == 2)
         psz = gpszVentHeight;
      break;
   case DFS_RECTANGLELRLITES:
      if (dwNum == 0)   // width
         psz = gpszWidth;
      else if (dwNum == 1)
         psz = gpszHeight;
      else if (dwNum == 2)
         psz = gpszSideLiteWidth;
      break;
   case DFS_TRAPEZOID1:
   case DFS_TRAPEZOID2:
      if (dwNum == 0)   // width
         psz = gpszWidth;
      else if (dwNum == 1)
         psz = gpszHeight;
      else if (dwNum == 2)
         psz = gpszSideLiteWidth;
      else if (dwNum == 3)
         psz = L"<bold>Width of bottom of side lites</bold>";
      break;
   case DFS_RECTANGLELITEVENT:
   case DFS_RECTANGLELRLITEVENT:
      if (dwNum == 0)   // width
         psz = gpszWidth;
      else if (dwNum == 1)
         psz = gpszHeight;
      else if (dwNum == 2)
         psz = gpszSideLiteWidth;
      else if (dwNum == 3)
         psz = gpszVentHeight;
      break;
   case DFS_ARCHLRLITES:
   case DFS_ARCHLRLITES2:
   case DFS_ARCHLRLITES3:
      if (dwNum == 0)   // width
         psz = gpszWidth;
      else if (dwNum == 1)
         psz = gpszHeight;
      else if (dwNum == 2)
         psz = gpszSideLiteWidth;
      else if (dwNum == 3)
         psz = gpszArchHeight;
      break;
   case DFS_RECTANGLEPEAK:
      if (dwNum == 0)   // width
         psz = gpszWidth;
      else if (dwNum == 1)
         psz = L"<bold>Height on the left</bold>";
      else if (dwNum == 2)
         psz = L"<bold>Height on the right</bold>";
      break;
   case DFS_ARCH:
   case DFS_ARCHHALF:
   case DFS_ARCHPEAK:
   case DFS_ARCHPARTCIRCLE:
   case DFS_ARCHTRIANGLE:
      if (dwNum == 0)   // width
         psz = gpszWidth;
      else if (dwNum == 1)
         psz = gpszHeight;
      else if (dwNum == 2)
         psz = L"<bold>Height at edge</bold>";
      break;

   case DFS_ARCHSPLIT:
      if (dwNum == 0)   // width
         psz = gpszWidth;
      else if (dwNum == 1)
         psz = gpszHeight;
      else if (dwNum == 2)
         psz = L"<bold>Arch height</bold>";
      break;

   default:
      return NULL;
   }

   return psz;
}

/***********************************************************************************
CDoorFrame::MeasSet - Sets a measurement value.

inputs
   DWORD    dwNum - MEasurement number, from 0..MeasNum()-1
   fp   fVal - Value
returns
   BOOL - TRUE if success
*/
BOOL CDoorFrame::MeasSet (DWORD dwNum, fp fVal)
{
   fp *pf;
   pf = (fp*) m_lMeasurement.Get(dwNum);
   if (!pf)
      return FALSE;

   *pf = max(0,fVal);   // do min/max checking

   m_fDirty =TRUE;
   return TRUE;
}


/***********************************************************************************
CDoorFrame::OutsideGet - Gets the outside spline. NOTE: This SHOULD NOT be changed
and is only valid until the doorframe object is changed. Y = 0. The spline is on the
very outside of the frame, just beyond the frame width.

returns
   PCSpline - Spline for outside frame. NULL if error
*/
PCSpline CDoorFrame::OutsideGet (void)
{
   CalcIfNecessary();

   return &m_lOutside;
}

/***********************************************************************************
CDoorFrame::Render - Draws the frame, centered on around 0 for X.
   Y ranges from depth/2 to -depth/2. Z is centered around 0 for windows, or is 0+
   for doors

inputs
   POBJECTRENDER     pr - Render information
   PCRenderSurface   prs - Render surface. This is used for drawing, so should
         have any previous rotations already done. Push() and Pop() are used
         to insure the main matrix isn't affected
   DWORD             dwSide - 0 for the front, 1 for the back
*/
void CDoorFrame::Render (POBJECTRENDER pr, PCRenderSurface prs, DWORD dwSide)
{
   CalcIfNecessary();

   DWORD i;
   for (i = 0; i < m_alNoodles[dwSide].Num(); i++) {
      PCNoodle pn = *((PCNoodle*) m_alNoodles[dwSide].Get(i));
      pn->Render (pr, prs);
   }
}

/***********************************************************************************
CDoorFrame::SplineMassage - Called by SplineInit(), this massages some of the data passed in
a) Adjust the Z value away from center if it's marked as a door.
b) If HIWORD(m_dwShape) then it's flipped, so flip x, and change CIRCLENEXT to CIRCLEPREV, etc.

NOTE: This may change some of the parameters in paPointa and padwSegCurve

inputs
   fp      fBottom - Bottom of the whole frame (including width of frame). Only used
               if m_dwDoor is marked, then it will offset by -fBottom.
   BOOL        fLooped - TRUE if the spline loops around on itself, FALSE if it starts
                  and ends at the first and last point.
   DWORD       dwPoints - Number of points in the spline.
   PCPoint     paPoints - Pointer to an array of dwPoints points for the spline control
                  points.
   DWORD       *padwSegCurve - Pointer to anarray of DWORDs describing the curve/spline
                  to use between this point and the next. Must have dwPoints elements.
                  This can also be set directly to one of the SEGCURVE_XXXX values
                  to use that SEGCURVE for all the segments. Has dwPoints unless is NOT
                  looped, then dwPoints-1.
   DWORD       dwDivide - Number of times to divide
*/
void CDoorFrame::SplineMassage (fp fBottom, BOOL fLooped, DWORD dwPoints,
                             PCPoint paPoints, DWORD *padwSegCurve, DWORD dwDivide)
{
   // move up?
   DWORD i;
   if (m_dwDoor == DFD_DOOR) {
      for (i = 0; i < dwPoints; i++)
         paPoints[i].p[2] -= fBottom;
   }

   // swap
   if (HIWORD(m_dwShape)) {
      CPoint t;
      for (i = 0; i < dwPoints/2; i++) {
         t.Copy (&paPoints[i]);
         paPoints[i].Copy (&paPoints[dwPoints-i-1]);
         paPoints[dwPoints-i-1].Copy (&t);
      }

      for (i = 0; i < dwPoints; i++)
         paPoints[i].p[0] *= -1;

      if ((DWORD)(size_t) padwSegCurve > 100) {
         DWORD dwNum = dwPoints - (fLooped ? 0 : 1);
         DWORD dwVal;
         for (i = 0; i < dwNum; i++) {
            dwVal = padwSegCurve[i];
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
            padwSegCurve[i] = dwVal;
         }

         for (i = 0; i < dwNum/2; i++) {
            dwVal = padwSegCurve[i];
            padwSegCurve[i] = padwSegCurve[dwNum-i-1];
            padwSegCurve[dwNum-i-1] = dwVal;
         }
         if (fLooped) {
            // BUGFIX - If looped need to rotate around 1...
            dwVal = padwSegCurve[0];
            memmove (padwSegCurve + 0, padwSegCurve + 1, (dwNum-1) * sizeof(DWORD));
            padwSegCurve[dwNum-1] = dwVal;
         }

      }  // if seg curve
   }  // if flipped
}

/***********************************************************************************
CDoorFrame::SplineInit - Calls spline->init(), but in the process it:
a) Adjust the Z value away from center if it's marked as a door.
b) If HIWORD(m_dwShape) then it's flipped, so flip x, and change CIRCLENEXT to CIRCLEPREV, etc.

NOTE: This may change some of the parameters in paPointa and padwSegCurve

inputs
   fp      fBottom - Bottom of the whole frame (including width of frame). Only used
               if m_dwDoor is marked, then it will offset by -fBottom.
   PCSpline       pSpline - spline
   BOOL        fLooped - TRUE if the spline loops around on itself, FALSE if it starts
                  and ends at the first and last point.
   DWORD       dwPoints - Number of points in the spline.
   PCPoint     paPoints - Pointer to an array of dwPoints points for the spline control
                  points.
   DWORD       *padwSegCurve - Pointer to anarray of DWORDs describing the curve/spline
                  to use between this point and the next. Must have dwPoints elements.
                  This can also be set directly to one of the SEGCURVE_XXXX values
                  to use that SEGCURVE for all the segments. Has dwPoints unless is NOT
                  looped, then dwPoints-1.
   DWORD       dwDivide - Number of times to divide
   fp      fExpand - If TRUE, expands out (or in) this much
*/
BOOL CDoorFrame::SplineInit (fp fBottom, PCSpline pSpline, BOOL fLooped, DWORD dwPoints,
                             PCPoint paPoints, DWORD *padwSegCurve, DWORD dwDivide, fp fExpand)
{
   SplineMassage (fBottom, fLooped, dwPoints, paPoints, padwSegCurve, dwDivide);

   // init
   if (!pSpline->Init (fLooped, dwPoints, paPoints, NULL, padwSegCurve, dwDivide, dwDivide))
      return FALSE;

   if (fExpand) {
      PCSpline pExpand;
      CPoint pFront;
      pFront.Zero();
      pFront.p[1] = -1;
      pExpand = pSpline->Expand (fExpand, &pFront);
      if (pExpand) {
         pExpand->CloneTo (pSpline);
         delete pExpand;
      }
   }

   return TRUE;
}


/***********************************************************************************
CDoorFrame::SplineInit - Calls spline->init(), but in the process it:
a) Adjust the Z value away from center if it's marked as a door.
b) If HIWORD(m_dwShape) then it's flipped, so flip x, and change CIRCLENEXT to CIRCLEPREV, etc.

inputs
   fp      fBottom - Bottom of the whole frame (including width of frame). Only used
               if m_dwDoor is marked, then it will offset by -fBottom.
   fp      ixl, izt, ixr, izb - UL and LR corner
   BOOL        fBase - if TRUE, include a base piece to the spline, else not
*/
BOOL CDoorFrame::SplineInit (fp fBottom, PCSpline pSpline,
                             fp ixl, fp izt, fp ixr, fp izb, BOOL fBase)
{
   CPoint ap[4];
   memset (ap, 0, sizeof(ap));
   ap[0].p[0] = ixl;
   ap[0].p[2] = izb;
   ap[1].p[0] = ixl;
   ap[1].p[2] = izt;
   ap[2].p[0] = ixr;
   ap[2].p[2] = izt;
   ap[3].p[0] = ixr;
   ap[3].p[2] = izb;

   return SplineInit (fBottom, pSpline, fBase, 4, ap, (DWORD*)SEGCURVE_LINEAR, 0);
}

/***********************************************************************************
CDoorFrame::NoodlePath - Calls pNoodle->PathSpline(), but in the process it:
a) Adjust the Z value away from center if it's marked as a door.
b) If HIWORD(m_dwShape) then it's flipped, so flip x, and change CIRCLENEXT to CIRCLEPREV, etc.

Also, initializes noodle.


NOTE: This may change some of the parameters in paPointa and padwSegCurve

inputs
   fp      fBottom - Bottom of the whole frame (including width of frame). Only used
               if m_dwDoor is marked, then it will offset by -fBottom.
   BOOL        fSubFrame - Set to TRUE if it's a sub frame
   BOOL        fLooped - TRUE if the spline loops around on itself, FALSE if it starts
                  and ends at the first and last point.
   DWORD       dwPoints - Number of points in the spline.
   PCPoint     paPoints - Pointer to an array of dwPoints points for the spline control
                  points.
   DWORD       *padwSegCurve - Pointer to anarray of DWORDs describing the curve/spline
                  to use between this point and the next. Must have dwPoints elements.
                  This can also be set directly to one of the SEGCURVE_XXXX values
                  to use that SEGCURVE for all the segments. Has dwPoints unless is NOT
                  looped, then dwPoints-1.
   DWORD       dwDivide - Number of times to divide
   fp      fExpand - If TRUE, expands out (or in) this much
*/
BOOL CDoorFrame::NoodlePath (fp fBottom, BOOL fSubFrame, BOOL fLooped, DWORD dwPoints,
                             PCPoint paPoints, DWORD *padwSegCurve, DWORD dwDivide,
                             fp fExpand)
{
   SplineMassage (fBottom, fLooped, dwPoints, paPoints, padwSegCurve, dwDivide);

   DWORD i, j;
   CPoint pScale, pFront;
   pScale.Copy (fSubFrame ? &m_pSubFrameSize : &m_pFrameSize);
   pScale.p[1] /= 2.0;  // so that two sectons of 1/2

   // if there's nothing to the frame then dont bother with noodle
   if (!pScale.p[0] && !pScale.p[1])
      return TRUE;

   // expand it?
   if (fExpand) {
      CSpline s;
      PCSpline pExpand;
      CPoint pFront;
      pFront.Zero();
      pFront.p[1] = -1;
      s.Init (fLooped, dwPoints, paPoints, NULL, padwSegCurve, dwDivide, dwDivide);
      pExpand = s.Expand (fExpand, &pFront);
      if (pExpand) {
         // get the expanded version
         for (i = 0; i < pExpand->OrigNumPointsGet(); i++)
            pExpand->OrigPointGet (i, &paPoints[i]);
         delete pExpand;
      }
   }

   fp fThick = pScale.p[1] / 2.0;
   for (i = 0; i < 2; i++) {
      PCNoodle pn = new CNoodle;
      if (!pn)
         return FALSE;

      // offset for two sides
      for (j = 0; j < dwPoints; j++)
         paPoints[j].p[1] = (i ? fThick : -fThick);

      pFront.Zero();
      pFront.p[1] = (i ? -1 : 1);

      pn->ShapeDefault (NS_RECTANGLE);
      pn->ScaleVector (&pScale);
      pn->PathSpline (fLooped, dwPoints, paPoints, padwSegCurve, dwDivide);

      m_alNoodles[i].Add (&pn);
   }

   return TRUE;
}


/***********************************************************************************
CDoorFrame::NoodlePath - Calls spline->init(), but in the process it:
a) Adjust the Z value away from center if it's marked as a door.
b) If HIWORD(m_dwShape) then it's flipped, so flip x, and change CIRCLENEXT to CIRCLEPREV, etc.

inputs
   fp      fBottom - Bottom of the whole frame (including width of frame). Only used
               if m_dwDoor is marked, then it will offset by -fBottom.
   fp      ixl, izt, ixr, izb - UL and LR corner
   BOOL        fBase - if TRUE, include a base piece to the spline, else not
*/
BOOL CDoorFrame::NoodlePath (fp fBottom, BOOL fSubFrame,
                             fp ixl, fp izt, fp ixr, fp izb, BOOL fBase)
{
   CPoint ap[4];
   memset (ap, 0, sizeof(ap));
   ap[0].p[0] = ixl;
   ap[0].p[2] = izb;
   ap[1].p[0] = ixl;
   ap[1].p[2] = izt;
   ap[2].p[0] = ixr;
   ap[2].p[2] = izt;
   ap[3].p[0] = ixr;
   ap[3].p[2] = izb;

   return NoodlePath (fBottom, fSubFrame, fBase, 4, ap, (DWORD*)SEGCURVE_LINEAR, 0);
}

/***********************************************************************************
CDoorFrame::NoodleLinear - Initializes a path as linear with pNoodle->PathLinear(). The
only difference is that this also:
a) Adjust the Z value away from center if it's marked as a door.
b) If HIWORD(m_dwShape) then it's flipped, so flip x, and change CIRCLENEXT to CIRCLEPREV, etc.

inputs
   fp      fBottom - Bottom of the whole frame (including width of frame). Only used
               if m_dwDoor is marked, then it will offset by -fBottom.
   PCNoodle    pNoodle - Noodle.
   fp      fx1, fz1, fx2, fz2 - start and stop
*/
BOOL CDoorFrame::NoodleLinear (fp fBottom, BOOL fSubFrame,
                               fp fx1, fp fz1, fp fx2, fp fz2)
{
   CPoint ap[2];
   ap[0].Zero();
   ap[1].Zero();
   ap[0].p[0] = fx1;
   ap[0].p[2] = fz1;
   ap[1].p[0] = fx2;
   ap[1].p[2] = fz2;

   return NoodlePath (fBottom, fSubFrame, FALSE, 2, ap, (DWORD*) SEGCURVE_LINEAR, 0);
}

/***********************************************************************************
CDoorFrame::CalcIfNecessary - If the m_fDirty flag is set, this calulcates the
over-all shape, shape of each of the frames, and noodles.
*/
void CDoorFrame::CalcIfNecessary (void)
{
   CheckOpenings();

   if (!m_fDirty)
      return;

   // clear out old info
   DWORD i, dwOpenNum;
   POPENINGINFO api;
   api = (POPENINGINFO) m_lOPENINGINFO.Get(0);
   dwOpenNum = m_lOPENINGINFO.Num();
   for (i = 0; i < dwOpenNum; i++) {
      POPENINGINFO pi = api + i;
      if (pi->pSpline)
         delete pi->pSpline;
      pi->pSpline = NULL;
   }
   DWORD k;
   for (k = 0; k < 2; k++) {
      for (i = 0; i < m_alNoodles[k].Num(); i++) {
         PCNoodle pn= *((PCNoodle*) m_alNoodles[k].Get(i));
         delete pn;
      }
      m_alNoodles[k].Clear();
   }

   // measuremets
   fp *paf;
   DWORD dwMeasNum;
   paf = (fp*) m_lMeasurement.Get(0);
   dwMeasNum = m_lMeasurement.Num();

   // bottom
   fp fBottom, fHeight, fFrame, fSubFrame;
   fFrame = m_pFrameSize.p[0];
   fSubFrame = m_pSubFrameSize.p[0];
   CSpline sTemp;
   CPoint   apTemp[16];
   DWORD    adwTemp[16];
   memset (apTemp, 0, sizeof(apTemp));
   memset (adwTemp, 0, sizeof(adwTemp));

   // fill in:
   // - Full opening
   // - Insides
   // - Noodles
   switch (LOWORD(m_dwShape)) {
   case DFS_RECTANGLE:
      // bottom
      fBottom = - (paf[1] / 2 + (api[0].iBase ? fFrame : 0));

      // outside outline
      SplineInit (fBottom, &m_lOutside,
         -(paf[0]/2 + fFrame),
         paf[1]/2 + fFrame,
         paf[0]/2 + fFrame,
         fBottom);

      // inside outlsine
      api[0].pSpline = new CSpline;
      if (api[0].pSpline)
         SplineInit (fBottom, api[0].pSpline,
            -paf[0]/2, paf[1]/2, paf[0]/2, -paf[1]/2);

      // noodles
      NoodlePath (fBottom, FALSE,
         -(paf[0]/2 + fFrame/2),
         paf[1]/2 + fFrame/2,
         paf[0]/2 + fFrame/2,
         -(paf[1]/2 + (api[0].iBase ? (fFrame / 2) : 0)),
         api[0].iBase ? TRUE : FALSE);

      break;

   case DFS_REPEATNUMTOP:
   case DFS_REPEATNUM:
   case DFS_REPEATMINTOP:
   case DFS_REPEATMIN:
      {
         fp fWidth, fHeight, fLeft, fRight;
         DWORD iNum;
         BOOL fTopFrames = (LOWORD(m_dwShape) == DFS_REPEATNUMTOP) || (LOWORD(m_dwShape) == DFS_REPEATMINTOP);
         iNum = dwOpenNum;
         if (fTopFrames)
            iNum /= 2;
         switch (LOWORD(m_dwShape)) {
         case DFS_REPEATNUMTOP:
         case DFS_REPEATNUM:
            fWidth = paf[0];
            break;
         default:
         case DFS_REPEATMINTOP:
         case DFS_REPEATMIN:
            fWidth = (paf[0] - 2 * fFrame - (iNum-1) * fSubFrame) / (fp) iNum;
            break;
         }
         fHeight = paf[1];
         fLeft = fTopFrames ? paf[3] : 0;
         fRight = fTopFrames ? paf[4] : 0;

         // are any of these doorways?
         BOOL  fDoorways;
         fDoorways = FALSE;
         for (i = 0; i < dwOpenNum; i++)
            if (!api[i].iBase)
               fDoorways = TRUE;
         fp fNonDoorwayOffset;
         fNonDoorwayOffset = fDoorways ? fFrame : 0;

         // bottom
         fBottom = - (fHeight / 2 + (fDoorways ? 0 : fFrame));

         // checks
         fLeft = max(0, fLeft);
         fRight = max(0, fRight);

         fp fFarLeft, fTotalWidth;
         fTotalWidth = fWidth * iNum + (iNum-1) * fSubFrame;
         fTotalWidth = max(CLOSE, fTotalWidth);
         fFarLeft = -fTotalWidth/2;

         // outside outline
         apTemp[0].p[0] = fFarLeft - fFrame;
         apTemp[0].p[2] = fBottom;
         apTemp[1].p[0] = apTemp[0].p[0];
         apTemp[1].p[2] = fHeight/2 + fTopFrames * fSubFrame + fLeft + fFrame;
         apTemp[2].p[0] = -apTemp[0].p[0];
         apTemp[2].p[2] = fHeight/2 + fTopFrames * fSubFrame + fRight + fFrame;
         apTemp[3].p[0] = apTemp[2].p[0];
         apTemp[3].p[2] = apTemp[0].p[2];

         SplineInit (fBottom, &m_lOutside, TRUE, 4, apTemp, (DWORD*)SEGCURVE_LINEAR, 0);

         // inside outlsine
         for (i = 0; i < iNum; i++) {
            // bottom
            api[i].pSpline = new CSpline;
            if (api[i].pSpline)
               SplineInit (fBottom, api[i].pSpline,
                  fFarLeft + i * (fWidth + fSubFrame), fHeight/2,
                  fFarLeft + i * (fWidth + fSubFrame) + fWidth,
                  -fHeight/2 + (api[i].iBase ? fNonDoorwayOffset : 0));

            // top window
            if (!fTopFrames)
               continue;
            apTemp[0].p[0] = fFarLeft + i * (fWidth + fSubFrame);
            apTemp[0].p[2] = fHeight/2 + fSubFrame;
            apTemp[1].p[0] = apTemp[0].p[0];
            fp fAlpha;
            fAlpha = (apTemp[0].p[0] - fFarLeft) / fTotalWidth;
            apTemp[1].p[2] = fHeight/2 + fSubFrame + (1.0 - fAlpha) * fLeft + fAlpha * fRight;
            apTemp[2].p[0] = apTemp[0].p[0] + fWidth;
            fAlpha = (apTemp[2].p[0] - fFarLeft) / fTotalWidth;
            apTemp[2].p[2] = fHeight/2 + fSubFrame + (1.0 - fAlpha) * fLeft + fAlpha * fRight;
            apTemp[3].p[0] = apTemp[2].p[0];
            apTemp[3].p[2] = apTemp[0].p[2];
            api[i+iNum].pSpline = new CSpline;
            if (api[i+iNum].pSpline)
               SplineInit (fBottom, api[i+iNum].pSpline, TRUE, 4, apTemp, (DWORD*) SEGCURVE_LINEAR, 0);
         }

         // noodles
         apTemp[0].p[0] = fFarLeft - fFrame/2;
         apTemp[0].p[2] = fBottom;
         apTemp[1].p[0] = apTemp[0].p[0];
         apTemp[1].p[2] = fHeight/2 + fSubFrame + fLeft + fFrame/2;
         apTemp[2].p[0] = -apTemp[0].p[0];
         apTemp[2].p[2] = fHeight/2 + fSubFrame + fRight + fFrame/2;
         apTemp[3].p[0] = apTemp[2].p[0];
         apTemp[3].p[2] = apTemp[0].p[2];

         NoodlePath (fBottom, FALSE, !fDoorways, 4, apTemp, (DWORD*) SEGCURVE_LINEAR, 0);

         // lines underneath
         if (fDoorways) {
            for (i = 0; i < iNum; i++) {
               if (!api[i].iBase)
                  continue;
               NoodleLinear (fBottom, FALSE,
                  fFarLeft + i * (fWidth+fSubFrame), -fHeight/2 + fFrame/2,
                  fFarLeft + i * (fWidth+fSubFrame) + fWidth + fSubFrame/2, -fHeight/2 + fFrame/2);
            }
         }

         // across
         if (fTopFrames)
            NoodleLinear (fBottom, TRUE,
               fFarLeft, fHeight/2 + fSubFrame/2,
               -fFarLeft, fHeight/2 + fSubFrame/2);

         // vertical lines
         for (i = 0; i+1 < iNum; i++) {
            fp fX, fAlpha;
            fX = fFarLeft + i * (fWidth + fSubFrame) + fWidth + fSubFrame/2;
            fAlpha = (fX - fFarLeft) / fTotalWidth;
            NoodleLinear (fBottom, TRUE,
               fX, -fHeight/2,
               fX, fHeight/2 + fTopFrames * fSubFrame + (1.0 - fAlpha) * fLeft + fAlpha * fRight);
         }
      }

      break;
   case DFS_RECTANGLELRLITES:
   case DFS_RECTANGLELRLITEVENT:
      {
         BOOL fWithLite = (LOWORD(m_dwShape) == DFS_RECTANGLELRLITEVENT);
         fp fLiteExtra = (fWithLite ? (paf[3] + fSubFrame) : 0);

         // are any of these doorways?
         BOOL  fDoorways;
         fDoorways = FALSE;
         for (i = 0; i < dwOpenNum; i++)
            if (!api[i].iBase)
               fDoorways = TRUE;

         // bottom
         fBottom = - (paf[1] / 2 + (fDoorways ? 0 : fFrame));

         // outside outline
         SplineInit (fBottom, &m_lOutside,
            -(paf[0]/2 + fFrame + fSubFrame + paf[2]),
            paf[1]/2 + fFrame + fLiteExtra,
            paf[0]/2 + fFrame + fSubFrame + paf[2],
            fBottom);

         // inside outlsine
         api[0].pSpline = new CSpline;
         fp fNonDoorwayOffset;
         fNonDoorwayOffset = fDoorways ? fFrame : 0;
         if (api[0].pSpline)
            SplineInit (fBottom, api[0].pSpline,
            -paf[0]/2, paf[1]/2,
            paf[0]/2, -paf[1]/2 + (api[0].iBase ? fNonDoorwayOffset : 0));
         api[1].pSpline = new CSpline;
         if (api[1].pSpline)
            SplineInit (fBottom, api[1].pSpline,
               -paf[0]/2 - fSubFrame - paf[2], paf[1]/2,
               -paf[0]/2 - fSubFrame, -paf[1]/2 + (api[1].iBase ? fNonDoorwayOffset : 0));
         api[2].pSpline = new CSpline;
         if (api[2].pSpline)
            SplineInit (fBottom, api[2].pSpline,
               paf[0]/2 + fSubFrame, paf[1]/2,
               paf[0]/2 + fSubFrame + paf[2], -paf[1]/2 + (api[2].iBase ? fNonDoorwayOffset : 0));
         if (fWithLite) {
            api[3].pSpline = new CSpline;
            if (api[3].pSpline)
               SplineInit (fBottom, api[3].pSpline,
                  -paf[0]/2 - fSubFrame - paf[2], paf[1]/2 + fLiteExtra,
                  paf[0]/2 + fSubFrame + paf[2], paf[1]/2 + fSubFrame);
         }

         // noodles
         NoodlePath (fBottom, FALSE,
            -(paf[0]/2 + fFrame/2 + fSubFrame + paf[2]),
            paf[1]/2 + fFrame/2 + fLiteExtra,
            paf[0]/2 + fFrame/2 + fSubFrame + paf[2],
            -(paf[1]/2 + (fDoorways ? 0 : (fFrame / 2))),
            !fDoorways);
         if (fDoorways) {
            // lines undneath each
            if (api[0].iBase)
               NoodleLinear (fBottom, FALSE,
                  -paf[0]/2, fBottom + fFrame/2,
                  paf[0]/2, fBottom + fFrame/2);
            if (api[1].iBase) // left
               NoodleLinear (fBottom, FALSE,
                  -paf[0]/2 - fSubFrame - paf[2], fBottom + fFrame/2,
                  -paf[0]/2 - fSubFrame, fBottom + fFrame/2);
            if (api[2].iBase) // right
               NoodleLinear (fBottom, FALSE,
                  paf[0]/2 + fSubFrame, fBottom + fFrame/2,
                  paf[0]/2 + fSubFrame + paf[2], fBottom + fFrame/2);
         }
         // verticals
         NoodleLinear (fBottom, TRUE,
            -paf[0]/2 - fSubFrame/2, fBottom,
            -paf[0]/2 - fSubFrame/2, paf[1]/2);
         NoodleLinear (fBottom, TRUE,
            paf[0]/2 + fSubFrame/2, fBottom,
            paf[0]/2 + fSubFrame/2, paf[1]/2);
         if (fWithLite)
            NoodleLinear (fBottom, TRUE,
               -paf[0]/2 - fSubFrame - paf[2], paf[1]/2 + fSubFrame/2,
               paf[0]/2 + fSubFrame + paf[2], paf[1]/2 + fSubFrame/2);
      }
      break;

   case DFS_TRAPEZOID1:
      {
         // are any of these doorways?
         BOOL  fDoorways;
         fDoorways = FALSE;
         for (i = 0; i < dwOpenNum; i++)
            if (!api[i].iBase)
               fDoorways = TRUE;

         // bottom
         fBottom = - (paf[1] / 2 + (fDoorways ? 0 : fFrame));

         // outside outline
         apTemp[0].p[0] = -paf[0]/2 - fSubFrame - paf[3] - fFrame;
         apTemp[0].p[2] = fBottom;
         apTemp[1].p[0] = -paf[0]/2 - fSubFrame - paf[2] - fFrame;
         apTemp[1].p[2] = paf[1]/2 + fFrame;
         apTemp[2].p[0] = -apTemp[1].p[0];
         apTemp[2].p[2] = apTemp[1].p[2];
         apTemp[3].p[0] = -apTemp[0].p[0];
         apTemp[3].p[2] = apTemp[0].p[2];
         SplineInit (fBottom, &m_lOutside, TRUE, 4, apTemp, (DWORD*)SEGCURVE_LINEAR, 0);

         // inside outlsine
         api[0].pSpline = new CSpline;
         fp fNonDoorwayOffset;
         fNonDoorwayOffset = fDoorways ? fFrame : 0;
         if (api[0].pSpline)
            SplineInit (fBottom, api[0].pSpline,
            -paf[0]/2, paf[1]/2,
            paf[0]/2, -paf[1]/2 + (api[0].iBase ? fNonDoorwayOffset : 0));
         api[1].pSpline = new CSpline;
         if (api[1].pSpline) {
            apTemp[0].p[0] = -paf[0]/2 - fSubFrame - paf[3];
            apTemp[0].p[2] = -paf[1]/2 + (api[0].iBase ? fNonDoorwayOffset : 0);
            apTemp[1].p[0] = -paf[0]/2 - fSubFrame - paf[2];
            apTemp[1].p[2] = paf[1]/2;
            apTemp[2].p[0] = -paf[0]/2 - fSubFrame;
            apTemp[2].p[2] = apTemp[1].p[2];
            apTemp[3].p[0] = apTemp[2].p[0];
            apTemp[3].p[2] = apTemp[0].p[2];

            SplineInit (fBottom, api[1].pSpline, TRUE, 4, apTemp, (DWORD*) SEGCURVE_LINEAR, 0);
         }
         api[2].pSpline = new CSpline;
         if (api[2].pSpline) {
            apTemp[0].p[0] = paf[0]/2 + fSubFrame;
            apTemp[0].p[2] = -paf[1]/2 + (api[0].iBase ? fNonDoorwayOffset : 0);
            apTemp[1].p[0] = apTemp[0].p[0];
            apTemp[1].p[2] = paf[1]/2;
            apTemp[2].p[0] = paf[0]/2 + fSubFrame + paf[2];
            apTemp[2].p[2] = apTemp[1].p[2];
            apTemp[3].p[0] = paf[0]/2 + fSubFrame + paf[3];
            apTemp[3].p[2] = apTemp[0].p[2];

            SplineInit (fBottom, api[2].pSpline, TRUE, 4, apTemp, (DWORD*) SEGCURVE_LINEAR, 0);
         }

         // noodles
         apTemp[0].p[0] = -paf[0]/2 - fSubFrame - paf[3] - fFrame/2;
         apTemp[0].p[2] = fBottom;
         apTemp[1].p[0] = -paf[0]/2 - fSubFrame - paf[2] - fFrame/2;
         apTemp[1].p[2] = paf[1]/2 + fFrame/2;
         apTemp[2].p[0] = -apTemp[1].p[0];
         apTemp[2].p[2] = apTemp[1].p[2];
         apTemp[3].p[0] = -apTemp[0].p[0];
         apTemp[3].p[2] = apTemp[0].p[2];
         NoodlePath (fBottom, FALSE, !fDoorways, 4, apTemp, (DWORD*)SEGCURVE_LINEAR, 0);

         if (fDoorways) {
            // lines undneath each
            if (api[0].iBase)
               NoodleLinear (fBottom, FALSE,
                  -paf[0]/2, fBottom + fFrame/2,
                  paf[0]/2, fBottom + fFrame/2);
            if (api[1].iBase) // left
               NoodleLinear (fBottom, FALSE,
                  -paf[0]/2 - fSubFrame - paf[3], fBottom + fFrame/2,
                  -paf[0]/2 - fSubFrame, fBottom + fFrame/2);
            if (api[2].iBase) // right
               NoodleLinear (fBottom, FALSE,
                  paf[0]/2 + fSubFrame, fBottom + fFrame/2,
                  paf[0]/2 + fSubFrame + paf[3], fBottom + fFrame/2);
         }

         // verticals
         NoodleLinear (fBottom, TRUE,
            -paf[0]/2 - fSubFrame/2, fBottom,
            -paf[0]/2 - fSubFrame/2, paf[1]/2);
         NoodleLinear (fBottom, TRUE,
            paf[0]/2 + fSubFrame/2, fBottom,
            paf[0]/2 + fSubFrame/2, paf[1]/2);
      }
      break;

   case DFS_TRAPEZOID2:
      {
         // are any of these doorways?
         BOOL  fDoorways;
         fDoorways = FALSE;
         for (i = 0; i < dwOpenNum; i++)
            if (!api[i].iBase)
               fDoorways = TRUE;

         // bottom
         fBottom = - (paf[1] / 2 + (fDoorways ? 0 : fFrame));

         // outside outline
         apTemp[0].p[0] = -paf[0]/2 - fSubFrame - paf[3] - fFrame;
         apTemp[0].p[2] = fBottom;
         apTemp[1].p[0] = -paf[0]/2 - fSubFrame - paf[2] - fFrame;
         apTemp[1].p[2] = paf[1]/2 + fFrame;
         apTemp[2].p[0] = paf[0]/2 + fFrame;
         apTemp[2].p[2] = apTemp[1].p[2];
         apTemp[3].p[0] = apTemp[2].p[0];
         apTemp[3].p[2] = apTemp[0].p[2];
         SplineInit (fBottom, &m_lOutside, TRUE, 4, apTemp, (DWORD*)SEGCURVE_LINEAR, 0);

         // inside outlsine
         api[0].pSpline = new CSpline;
         fp fNonDoorwayOffset;
         fNonDoorwayOffset = fDoorways ? fFrame : 0;
         if (api[0].pSpline)
            SplineInit (fBottom, api[0].pSpline,
            -paf[0]/2, paf[1]/2,
            paf[0]/2, -paf[1]/2 + (api[0].iBase ? fNonDoorwayOffset : 0));
         api[1].pSpline = new CSpline;
         if (api[1].pSpline) {
            apTemp[0].p[0] = -paf[0]/2 - fSubFrame - paf[3];
            apTemp[0].p[2] = -paf[1]/2 + (api[0].iBase ? fNonDoorwayOffset : 0);
            apTemp[1].p[0] = -paf[0]/2 - fSubFrame - paf[2];
            apTemp[1].p[2] = paf[1]/2;
            apTemp[2].p[0] = -paf[0]/2 - fSubFrame;
            apTemp[2].p[2] = apTemp[1].p[2];
            apTemp[3].p[0] = apTemp[2].p[0];
            apTemp[3].p[2] = apTemp[0].p[2];

            SplineInit (fBottom, api[1].pSpline, TRUE, 4, apTemp, (DWORD*) SEGCURVE_LINEAR, 0);
         }

         // noodles
         apTemp[0].p[0] = -paf[0]/2 - fSubFrame - paf[3] - fFrame/2;
         apTemp[0].p[2] = fBottom;
         apTemp[1].p[0] = -paf[0]/2 - fSubFrame - paf[2] - fFrame/2;
         apTemp[1].p[2] = paf[1]/2 + fFrame/2;
         apTemp[2].p[0] = paf[0]/2 + fFrame/2;
         apTemp[2].p[2] = apTemp[1].p[2];
         apTemp[3].p[0] = apTemp[2].p[0];
         apTemp[3].p[2] = apTemp[0].p[2];
         NoodlePath (fBottom, FALSE, !fDoorways, 4, apTemp, (DWORD*)SEGCURVE_LINEAR, 0);

         if (fDoorways) {
            // lines undneath each
            if (api[0].iBase)
               NoodleLinear (fBottom, FALSE,
                  -paf[0]/2, fBottom + fFrame/2,
                  paf[0]/2, fBottom + fFrame/2);
            if (api[1].iBase) // left
               NoodleLinear (fBottom, FALSE,
                  -paf[0]/2 - fSubFrame - paf[3], fBottom + fFrame/2,
                  -paf[0]/2 - fSubFrame, fBottom + fFrame/2);
         }

         // verticals
         NoodleLinear (fBottom, TRUE,
            -paf[0]/2 - fSubFrame/2, fBottom,
            -paf[0]/2 - fSubFrame/2, paf[1]/2);
      }
      break;

   case DFS_ARCHLRLITES:
      {
         // NOTE: For this one separating arch with full-sized frame
         fp fLiteExtra = paf[3] + fFrame;

         // are any of these doorways?
         BOOL  fDoorways;
         fDoorways = FALSE;
         for (i = 0; i < dwOpenNum; i++)
            if (!api[i].iBase)
               fDoorways = TRUE;

         // bottom
         fBottom = - (paf[1] / 2 + (fDoorways ? 0 : fFrame));

         // outside outline
         apTemp[0].p[0] = -(paf[0]/2 + fFrame + fSubFrame + paf[2]);
         apTemp[0].p[2] = fBottom;
         apTemp[1].p[0] = apTemp[0].p[0];
         apTemp[1].p[2] = paf[1]/2 + fFrame;
         apTemp[2].p[0] = -paf[0]/2 - fFrame;
         apTemp[2].p[2] = apTemp[1].p[2];
         apTemp[3].p[0] = apTemp[2].p[0];
         apTemp[3].p[2] = paf[1]/2 + fFrame + fLiteExtra;
         apTemp[4].p[0] = 0;
         apTemp[4].p[2] = apTemp[3].p[2];
         for (i = 5; i < 9; i++) {
            apTemp[i].Copy (&apTemp[8-i]);
            apTemp[i].p[0] *= -1;
         }
         adwTemp[2] = SEGCURVE_ELLIPSENEXT;
         adwTemp[3] = SEGCURVE_ELLIPSEPREV;
         adwTemp[4] = SEGCURVE_ELLIPSENEXT;
         adwTemp[5] = SEGCURVE_ELLIPSEPREV;

         SplineInit (fBottom, &m_lOutside, TRUE, 9, apTemp, adwTemp, 2);

         // inside outlsine
         api[0].pSpline = new CSpline;
         fp fNonDoorwayOffset;
         fNonDoorwayOffset = fDoorways ? fFrame : 0;
         if (api[0].pSpline)
            SplineInit (fBottom, api[0].pSpline,
            -paf[0]/2, paf[1]/2,
            paf[0]/2, -paf[1]/2 + (api[0].iBase ? fNonDoorwayOffset : 0));
         api[1].pSpline = new CSpline;
         if (api[1].pSpline)
            SplineInit (fBottom, api[1].pSpline,
               -paf[0]/2 - fSubFrame - paf[2], paf[1]/2,
               -paf[0]/2 - fSubFrame, -paf[1]/2 + (api[1].iBase ? fNonDoorwayOffset : 0));
         api[2].pSpline = new CSpline;
         if (api[2].pSpline)
            SplineInit (fBottom, api[2].pSpline,
               paf[0]/2 + fSubFrame, paf[1]/2,
               paf[0]/2 + fSubFrame + paf[2], -paf[1]/2 + (api[2].iBase ? fNonDoorwayOffset : 0));

         // top arch
         apTemp[2].p[0] = -paf[0]/2;
         apTemp[2].p[2] = paf[1]/2 + fFrame;
         apTemp[3].p[0] = apTemp[2].p[0];
         apTemp[3].p[2] = paf[1]/2 + fFrame + paf[3];
         apTemp[4].p[0] = 0;
         apTemp[4].p[2] = apTemp[3].p[2];
         for (i = 5; i < 7; i++) {
            apTemp[i].Copy (&apTemp[8-i]);
            apTemp[i].p[0] *= -1;
         }
         adwTemp[2] = SEGCURVE_ELLIPSENEXT;
         adwTemp[3] = SEGCURVE_ELLIPSEPREV;
         adwTemp[4] = SEGCURVE_ELLIPSENEXT;
         adwTemp[5] = SEGCURVE_ELLIPSEPREV;
         adwTemp[6] = SEGCURVE_LINEAR;

         api[3].pSpline = new CSpline;
         if (api[3].pSpline)
            SplineInit (fBottom, api[3].pSpline, TRUE, 5, apTemp+2, adwTemp+2, 2);

         // noodles
         NoodlePath (fBottom, FALSE,
            -(paf[0]/2 + fFrame/2 + fSubFrame + paf[2]),
            paf[1]/2 + fFrame/2,
            paf[0]/2 + fFrame/2 + fSubFrame + paf[2],
            -(paf[1]/2 + (fDoorways ? 0 : (fFrame / 2))),
            2);   // BUGFIX - Was!fDoorways);
         if (fDoorways) {
            // lines undneath each
            if (api[0].iBase)
               NoodleLinear (fBottom, FALSE,
                  -paf[0]/2, fBottom + fFrame/2,
                  paf[0]/2, fBottom + fFrame/2);
            if (api[1].iBase) // left
               NoodleLinear (fBottom, FALSE,
                  -paf[0]/2 - fSubFrame - paf[2], fBottom + fFrame/2,
                  -paf[0]/2 - fSubFrame, fBottom + fFrame/2);
            if (api[2].iBase) // right
               NoodleLinear (fBottom, FALSE,
                  paf[0]/2 + fSubFrame, fBottom + fFrame/2,
                  paf[0]/2 + fSubFrame + paf[2], fBottom + fFrame/2);
         }
         // verticals
         NoodleLinear (fBottom, TRUE,
            -paf[0]/2 - fSubFrame/2, fBottom,
            -paf[0]/2 - fSubFrame/2, paf[1]/2);
         NoodleLinear (fBottom, TRUE,
            paf[0]/2 + fSubFrame/2, fBottom,
            paf[0]/2 + fSubFrame/2, paf[1]/2);

         // top arch
         apTemp[2].p[0] = -paf[0]/2 - fFrame/2;
         apTemp[2].p[2] = paf[1]/2 + fFrame;
         apTemp[3].p[0] = apTemp[2].p[0];
         apTemp[3].p[2] = paf[1]/2 + fFrame + paf[3] + fFrame/2;
         apTemp[4].p[0] = 0;
         apTemp[4].p[2] = apTemp[3].p[2];
         for (i = 5; i < 7; i++) {
            apTemp[i].Copy (&apTemp[8-i]);
            apTemp[i].p[0] *= -1;
         }
         adwTemp[2] = SEGCURVE_ELLIPSENEXT;
         adwTemp[3] = SEGCURVE_ELLIPSEPREV;
         adwTemp[4] = SEGCURVE_ELLIPSENEXT;
         adwTemp[5] = SEGCURVE_ELLIPSEPREV;
         adwTemp[6] = SEGCURVE_LINEAR;

         NoodlePath (fBottom, FALSE, FALSE, 5, apTemp+2, adwTemp+2, 2);
      }
      break;

   case DFS_ARCHLRLITES3:
      {
         // NOTE: For this one separating arch with full-sized frame
         fp fLiteExtra = paf[3] + fFrame;

         // are any of these doorways?
         BOOL  fDoorways;
         fDoorways = FALSE;
         for (i = 0; i < dwOpenNum; i++)
            if (!api[i].iBase)
               fDoorways = TRUE;

         // bottom
         fBottom = - (paf[1] / 2 + (fDoorways ? 0 : fFrame));

         // outside outline
         apTemp[0].p[0] = -(paf[0]/2 + fFrame + fSubFrame + paf[2]);
         apTemp[0].p[2] = fBottom;
         apTemp[1].p[0] = apTemp[0].p[0];
         apTemp[1].p[2] = paf[1]/2 + fFrame - paf[2];
         apTemp[2].p[0] = apTemp[1].p[0];
         apTemp[2].p[2] = paf[1]/2 + fFrame;
         apTemp[3].p[0] = -paf[0]/2 - fFrame;
         apTemp[3].p[2] = apTemp[2].p[2];
         apTemp[4].p[0] = apTemp[3].p[0];
         apTemp[4].p[2] = paf[1]/2 + fFrame + fLiteExtra;
         apTemp[5].p[0] = 0;
         apTemp[5].p[2] = apTemp[4].p[2];
         for (i = 6; i < 11; i++) {
            apTemp[i].Copy (&apTemp[10-i]);
            apTemp[i].p[0] *= -1;
         }
         adwTemp[1] = SEGCURVE_ELLIPSENEXT;
         adwTemp[2] = SEGCURVE_ELLIPSEPREV;
         adwTemp[3] = SEGCURVE_ELLIPSENEXT;
         adwTemp[4] = SEGCURVE_ELLIPSEPREV;
         adwTemp[5] = SEGCURVE_ELLIPSENEXT;
         adwTemp[6] = SEGCURVE_ELLIPSEPREV;
         adwTemp[7] = SEGCURVE_ELLIPSENEXT;
         adwTemp[8] = SEGCURVE_ELLIPSEPREV;

         SplineInit (fBottom, &m_lOutside, TRUE, 11, apTemp, adwTemp, 2);

         // inside outlsine
         api[0].pSpline = new CSpline;
         fp fNonDoorwayOffset;
         fNonDoorwayOffset = fDoorways ? fFrame : 0;
         if (api[0].pSpline)
            SplineInit (fBottom, api[0].pSpline,
            -paf[0]/2, paf[1]/2,
            paf[0]/2, -paf[1]/2 + (api[0].iBase ? fNonDoorwayOffset : 0));
         api[1].pSpline = new CSpline;
         if (api[1].pSpline) {
            apTemp[0].p[0] = -(paf[0]/2 + fSubFrame + paf[2]);
            apTemp[0].p[2] = -paf[1]/2 + (api[1].iBase ? fNonDoorwayOffset : 0);
            apTemp[1].p[0] = apTemp[0].p[0];
            apTemp[1].p[2] = paf[1]/2 - paf[2];
            apTemp[2].p[0] = apTemp[1].p[0];
            apTemp[2].p[2] = paf[1]/2;
            apTemp[3].p[0] = -paf[0]/2 - fSubFrame;
            apTemp[3].p[2] = apTemp[2].p[2];
            apTemp[4].p[0] = apTemp[3].p[0];
            apTemp[4].p[2] = apTemp[0].p[2];
            memset (adwTemp, 0, sizeof(adwTemp));
            adwTemp[1] = SEGCURVE_ELLIPSENEXT;
            adwTemp[2] = SEGCURVE_ELLIPSEPREV;

            SplineInit (fBottom, api[1].pSpline, TRUE, 5, apTemp, adwTemp, 2);
         }
         api[2].pSpline = new CSpline;
         if (api[2].pSpline) {
            apTemp[0].p[0] = paf[0]/2 + fSubFrame;
            apTemp[0].p[2] = -paf[1]/2 + (api[1].iBase ? fNonDoorwayOffset : 0);
            apTemp[1].p[0] = apTemp[0].p[0];
            apTemp[1].p[2] = paf[1]/2;
            apTemp[2].p[0] = paf[0]/2 + fSubFrame + paf[2];
            apTemp[2].p[2] = apTemp[1].p[2];
            apTemp[3].p[0] = apTemp[2].p[0];
            apTemp[3].p[2] = paf[1]/2 - paf[2];
            apTemp[4].p[0] = apTemp[3].p[0];
            apTemp[4].p[2] = apTemp[0].p[2];
            adwTemp[1] = SEGCURVE_ELLIPSENEXT;
            adwTemp[2] = SEGCURVE_ELLIPSEPREV;

            SplineInit (fBottom, api[2].pSpline, TRUE, 5, apTemp, adwTemp, 2);
         }

         // top arch
         apTemp[2].p[0] = -paf[0]/2;
         apTemp[2].p[2] = paf[1]/2 + fFrame;
         apTemp[3].p[0] = apTemp[2].p[0];
         apTemp[3].p[2] = paf[1]/2 + fFrame + paf[3];
         apTemp[4].p[0] = 0;
         apTemp[4].p[2] = apTemp[3].p[2];
         for (i = 5; i < 7; i++) {
            apTemp[i].Copy (&apTemp[8-i]);
            apTemp[i].p[0] *= -1;
         }
         adwTemp[2] = SEGCURVE_ELLIPSENEXT;
         adwTemp[3] = SEGCURVE_ELLIPSEPREV;
         adwTemp[4] = SEGCURVE_ELLIPSENEXT;
         adwTemp[5] = SEGCURVE_ELLIPSEPREV;
         adwTemp[6] = SEGCURVE_LINEAR;

         api[3].pSpline = new CSpline;
         if (api[3].pSpline)
            SplineInit (fBottom, api[3].pSpline, TRUE, 5, apTemp+2, adwTemp+2, 2);

         // noodles
         apTemp[0].p[0] = -(paf[0]/2 + fFrame/2 + fSubFrame + paf[2]);
         apTemp[0].p[2] = fBottom;
         apTemp[1].p[0] = apTemp[0].p[0];
         apTemp[1].p[2] = paf[1]/2 + fFrame/2 - paf[2];
         apTemp[2].p[0] = apTemp[1].p[0];
         apTemp[2].p[2] = paf[1]/2 + fFrame/2;
         apTemp[3].p[0] = -paf[0]/2 - fFrame/2;
         apTemp[3].p[2] = apTemp[2].p[2];
         apTemp[4].p[0] = apTemp[3].p[0];
         apTemp[4].p[2] = paf[1]/2 + fFrame/2 + fLiteExtra;
         apTemp[5].p[0] = 0;
         apTemp[5].p[2] = apTemp[4].p[2];
         for (i = 6; i < 11; i++) {
            apTemp[i].Copy (&apTemp[10-i]);
            apTemp[i].p[0] *= -1;
         }
         memset (adwTemp, 0 ,sizeof(adwTemp));
         adwTemp[1] = SEGCURVE_ELLIPSENEXT;
         adwTemp[2] = SEGCURVE_ELLIPSEPREV;
         adwTemp[3] = SEGCURVE_ELLIPSENEXT;
         adwTemp[4] = SEGCURVE_ELLIPSEPREV;
         adwTemp[5] = SEGCURVE_ELLIPSENEXT;
         adwTemp[6] = SEGCURVE_ELLIPSEPREV;
         adwTemp[7] = SEGCURVE_ELLIPSENEXT;
         adwTemp[8] = SEGCURVE_ELLIPSEPREV;
         NoodlePath (fBottom, FALSE, !fDoorways, 11, apTemp, adwTemp, 2);
         if (fDoorways) {
            // lines undneath each
            if (api[0].iBase)
               NoodleLinear (fBottom, FALSE,
                  -paf[0]/2, fBottom + fFrame/2,
                  paf[0]/2, fBottom + fFrame/2);
            if (api[1].iBase) // left
               NoodleLinear (fBottom, FALSE,
                  -paf[0]/2 - fSubFrame - paf[2], fBottom + fFrame/2,
                  -paf[0]/2 - fSubFrame, fBottom + fFrame/2);
            if (api[2].iBase) // right
               NoodleLinear (fBottom, FALSE,
                  paf[0]/2 + fSubFrame, fBottom + fFrame/2,
                  paf[0]/2 + fSubFrame + paf[2], fBottom + fFrame/2);
         }
         // verticals
         NoodleLinear (fBottom, TRUE,
            -paf[0]/2 - fSubFrame/2, fBottom,
            -paf[0]/2 - fSubFrame/2, paf[1]/2);
         NoodleLinear (fBottom, TRUE,
            paf[0]/2 + fSubFrame/2, fBottom,
            paf[0]/2 + fSubFrame/2, paf[1]/2);

         // top arch horizontal
         NoodleLinear (fBottom, TRUE,
            -paf[0]/2, paf[1]/2 + fSubFrame/2,
            paf[0] / 2, paf[1]/2 + fSubFrame/2);
      }
      break;

   case DFS_ARCHLRLITES2:
      {
         fp fLiteExtra = paf[3] + fSubFrame;

         // are any of these doorways?
         BOOL  fDoorways;
         fDoorways = FALSE;
         for (i = 0; i < dwOpenNum; i++)
            if (!api[i].iBase)
               fDoorways = TRUE;

         // bottom
         fBottom = - (paf[1] / 2 + (fDoorways ? 0 : fFrame));

         // outside outline
         apTemp[0].p[0] = -(paf[0]/2 + fFrame + fSubFrame + paf[2]);
         apTemp[0].p[2] = fBottom;
         apTemp[1].p[0] = apTemp[0].p[0];
         apTemp[1].p[2] = paf[1]/2 + fSubFrame;
         apTemp[2].p[0] = apTemp[1].p[0];
         apTemp[2].p[2] = paf[1]/2 + fFrame + fLiteExtra;
         apTemp[3].p[0] = 0;
         apTemp[3].p[2] = apTemp[2].p[2];
         for (i = 4; i < 7; i++) {
            apTemp[i].Copy (&apTemp[6-i]);
            apTemp[i].p[0] *= -1;
         }
         adwTemp[1] = SEGCURVE_ELLIPSENEXT;
         adwTemp[2] = SEGCURVE_ELLIPSEPREV;
         adwTemp[3] = SEGCURVE_ELLIPSENEXT;
         adwTemp[4] = SEGCURVE_ELLIPSEPREV;

         SplineInit (fBottom, &m_lOutside, TRUE, 7, apTemp, adwTemp, 2);

         // inside outlsine
         api[0].pSpline = new CSpline;
         fp fNonDoorwayOffset;
         fNonDoorwayOffset = fDoorways ? fFrame : 0;
         if (api[0].pSpline)
            SplineInit (fBottom, api[0].pSpline,
            -paf[0]/2, paf[1]/2,
            paf[0]/2, -paf[1]/2 + (api[0].iBase ? fNonDoorwayOffset : 0));
         api[1].pSpline = new CSpline;
         if (api[1].pSpline)
            SplineInit (fBottom, api[1].pSpline,
               -paf[0]/2 - fSubFrame - paf[2], paf[1]/2,
               -paf[0]/2 - fSubFrame, -paf[1]/2 + (api[1].iBase ? fNonDoorwayOffset : 0));
         api[2].pSpline = new CSpline;
         if (api[2].pSpline)
            SplineInit (fBottom, api[2].pSpline,
               paf[0]/2 + fSubFrame, paf[1]/2,
               paf[0]/2 + fSubFrame + paf[2], -paf[1]/2 + (api[2].iBase ? fNonDoorwayOffset : 0));

         // top arch
         apTemp[1].p[0] = -(paf[0]/2 + fSubFrame + paf[2]);
         apTemp[1].p[2] = paf[1]/2 + fSubFrame;
         apTemp[2].p[0] = apTemp[1].p[0];
         apTemp[2].p[2] = paf[1]/2 + fLiteExtra;
         apTemp[3].p[0] = 0;
         apTemp[3].p[2] = apTemp[2].p[2];
         for (i = 4; i < 6; i++) {
            apTemp[i].Copy (&apTemp[6-i]);
            apTemp[i].p[0] *= -1;
         }
         adwTemp[1] = SEGCURVE_ELLIPSENEXT;
         adwTemp[2] = SEGCURVE_ELLIPSEPREV;
         adwTemp[3] = SEGCURVE_ELLIPSENEXT;
         adwTemp[4] = SEGCURVE_ELLIPSEPREV;
         adwTemp[5] = SEGCURVE_LINEAR;

         api[3].pSpline = new CSpline;
         if (api[3].pSpline)
            SplineInit (fBottom, api[3].pSpline, TRUE, 5, apTemp+1, adwTemp+1, 2);

         // noodles
         apTemp[0].p[0] = -(paf[0]/2 + fFrame/2 + fSubFrame + paf[2]);
         apTemp[0].p[2] = fBottom;
         apTemp[1].p[0] = apTemp[0].p[0];
         apTemp[1].p[2] = paf[1]/2 + fSubFrame;
         apTemp[2].p[0] = apTemp[1].p[0];
         apTemp[2].p[2] = paf[1]/2 + fFrame/2 + fLiteExtra;
         apTemp[3].p[0] = 0;
         apTemp[3].p[2] = apTemp[2].p[2];
         for (i = 4; i < 7; i++) {
            apTemp[i].Copy (&apTemp[6-i]);
            apTemp[i].p[0] *= -1;
         }
         memset (adwTemp, 0, sizeof(adwTemp));
         adwTemp[1] = SEGCURVE_ELLIPSENEXT;
         adwTemp[2] = SEGCURVE_ELLIPSEPREV;
         adwTemp[3] = SEGCURVE_ELLIPSENEXT;
         adwTemp[4] = SEGCURVE_ELLIPSEPREV;
         NoodlePath (fBottom, FALSE, !fDoorways, 7, apTemp, adwTemp, 2);
         if (fDoorways) {
            // lines undneath each
            if (api[0].iBase)
               NoodleLinear (fBottom, FALSE,
                  -paf[0]/2, fBottom + fFrame/2,
                  paf[0]/2, fBottom + fFrame/2);
            if (api[1].iBase) // left
               NoodleLinear (fBottom, FALSE,
                  -paf[0]/2 - fSubFrame - paf[2], fBottom + fFrame/2,
                  -paf[0]/2 - fSubFrame, fBottom + fFrame/2);
            if (api[2].iBase) // right
               NoodleLinear (fBottom, FALSE,
                  paf[0]/2 + fSubFrame, fBottom + fFrame/2,
                  paf[0]/2 + fSubFrame + paf[2], fBottom + fFrame/2);
         }
         // verticals
         NoodleLinear (fBottom, TRUE,
            -paf[0]/2 - fSubFrame/2, fBottom,
            -paf[0]/2 - fSubFrame/2, paf[1]/2);
         NoodleLinear (fBottom, TRUE,
            paf[0]/2 + fSubFrame/2, fBottom,
            paf[0]/2 + fSubFrame/2, paf[1]/2);

         // horizontal
         NoodleLinear (fBottom, TRUE,
            -paf[0]/2 - paf[2] - fSubFrame, paf[1]/2 + fSubFrame/2,
            paf[0]/2 + paf[2] + fSubFrame, paf[1]/2 + fSubFrame/2);
      }
      break;

   case DFS_RECTANGLELITEVENT:
      {
         fp fLiteExtra = (paf[3] + fSubFrame);

         // are any of these doorways?
         BOOL  fDoorways;
         fDoorways = FALSE;
         for (i = 0; i < dwOpenNum; i++)
            if (!api[i].iBase)
               fDoorways = TRUE;

         // bottom
         fBottom = - (paf[1] / 2 + (fDoorways ? 0 : fFrame));

         // outside outline
         SplineInit (fBottom, &m_lOutside,
            -(paf[0]/2 + fFrame + fSubFrame + paf[2]),
            paf[1]/2 + fFrame + fLiteExtra,
            paf[0]/2 + fFrame,
            fBottom);

         // inside outlsine
         api[0].pSpline = new CSpline;
         fp fNonDoorwayOffset;
         fNonDoorwayOffset = fDoorways ? fFrame : 0;
         if (api[0].pSpline)
            SplineInit (fBottom, api[0].pSpline,
            -paf[0]/2, paf[1]/2,
            paf[0]/2, -paf[1]/2 + (api[0].iBase ? fNonDoorwayOffset : 0));
         api[1].pSpline = new CSpline;
         if (api[1].pSpline)
            SplineInit (fBottom, api[1].pSpline,
               -paf[0]/2 - fSubFrame - paf[2], paf[1]/2,
               -paf[0]/2 - fSubFrame, -paf[1]/2 + (api[1].iBase ? fNonDoorwayOffset : 0));
         api[2].pSpline = new CSpline;
         if (api[2].pSpline)
            SplineInit (fBottom, api[2].pSpline,
               -paf[0]/2 - fSubFrame - paf[2], paf[1]/2 + fLiteExtra,
               paf[0]/2, paf[1]/2 + fSubFrame);

         // noodles
         NoodlePath (fBottom, FALSE,
            -(paf[0]/2 + fFrame/2 + fSubFrame + paf[2]),
            paf[1]/2 + fFrame/2 + fLiteExtra,
            paf[0]/2 + fFrame/2,
            -(paf[1]/2 + (fDoorways ? 0 : (fFrame / 2))),
            !fDoorways);
         if (fDoorways) {
            // lines undneath each
            if (api[0].iBase)
               NoodleLinear (fBottom, FALSE,
                  -paf[0]/2, fBottom + fFrame/2,
                  paf[0]/2, fBottom + fFrame/2);
            if (api[1].iBase) // left
               NoodleLinear (fBottom, FALSE,
                  -paf[0]/2 - fSubFrame - paf[2], fBottom + fFrame/2,
                  -paf[0]/2 - fSubFrame, fBottom + fFrame/2);
         }
         // verticals
         NoodleLinear (fBottom, TRUE,
            -paf[0]/2 - fSubFrame/2, fBottom,
            -paf[0]/2 - fSubFrame/2, paf[1]/2);

         // horizontal
         NoodleLinear (fBottom, TRUE,
            -paf[0]/2 - fSubFrame - paf[2], paf[1]/2 + fSubFrame/2,
            paf[0]/2, paf[1]/2 + fSubFrame/2);
      }
      break;
   case DFS_RECTANGLEVENT:
      // bottom
      fBottom = - (paf[1] / 2 + (api[0].iBase ? fFrame : 0));

      // outside outline
      SplineInit (fBottom, &m_lOutside,
         -(paf[0]/2 + fFrame),
         paf[1]/2 + fSubFrame + paf[2] + fFrame,
         paf[0]/2 + fFrame,
         fBottom);

      // inside outline
      api[0].pSpline = new CSpline;
      if (api[0].pSpline)
         SplineInit (fBottom, api[0].pSpline,
            -paf[0]/2, paf[1]/2, paf[0]/2, -paf[1]/2);
      api[1].pSpline = new CSpline;
      if (api[1].pSpline)
         SplineInit (fBottom, api[1].pSpline,
            -paf[0]/2, paf[1]/2 + fSubFrame + paf[2], paf[0]/2, paf[1]/2 + fSubFrame);

      // noodles
      NoodlePath (fBottom, FALSE,
         -(paf[0]/2 + fFrame/2),
         paf[1]/2 + fSubFrame + paf[2] + fFrame/2,
         paf[0]/2 + fFrame/2,
         -(paf[1]/2 + (api[0].iBase ? (fFrame / 2) : 0)),
         api[0].iBase ? TRUE : FALSE);
      NoodleLinear (fBottom, TRUE,
         -paf[0]/2, paf[1]/2+fSubFrame/2, paf[0]/2, paf[1]/2+fSubFrame/2);

      break;

   case DFS_RECTANGLEPEAK:
      {
         // bottom

         fHeight = max(paf[1], paf[2]);
         fBottom = - (fHeight / 2 + (api[0].iBase ? fFrame : 0));

         // outside outline
         CPoint apKeep[4];
         apTemp[0].p[0] = -paf[0]/2;  // LL
         apTemp[0].p[2] = -fHeight/2;
         apTemp[1].p[0] = apTemp[0].p[0]; // UL
         apTemp[1].p[2] = paf[1] - fHeight/2.0;
         apTemp[2].p[0] = -apTemp[1].p[0];   // UR
         apTemp[2].p[2] = paf[2] - fHeight / 2.0;
         apTemp[3].p[0] = apTemp[2].p[0];
         apTemp[3].p[2] = -fHeight/2;
         memcpy (apKeep, apTemp, sizeof(apKeep));
         apTemp[0].p[2] = apTemp[3].p[2] = fBottom + fFrame;   // Do this because will expand in a sec
         SplineInit (fBottom, &m_lOutside, TRUE, 4, apTemp, (DWORD*)SEGCURVE_LINEAR, 0, fFrame);

         // inside outlsine
         memcpy (apTemp, apKeep, sizeof(apKeep));
         api[0].pSpline = new CSpline;
         if (api[0].pSpline)
            SplineInit (fBottom, api[0].pSpline, TRUE, 4, apTemp, (DWORD*)SEGCURVE_LINEAR, 0);

         // noodles
         memcpy (apTemp, apKeep, sizeof(apKeep));
         NoodlePath (fBottom, FALSE, api[0].iBase ? TRUE : FALSE,
            4, apTemp, (DWORD*) SEGCURVE_LINEAR, 0, fFrame/2);
      }
      break;


   case DFS_ARCH:
   case DFS_ARCHPEAK:
      {
         BOOL fPeak = (LOWORD(m_dwShape == DFS_ARCHPEAK));
         // bottom

         fHeight = paf[1];
         fBottom = - (fHeight / 2 + (api[0].iBase ? fFrame : 0));

         // outside outline
         CPoint apKeep[7];
         DWORD adwKeep[7];
         apTemp[0].p[0] = -(paf[0]/2 + fFrame);  // LL
         apTemp[0].p[2] = fBottom;
         apTemp[1].p[0] = apTemp[0].p[0]; // UL
         apTemp[1].p[2] = -paf[1]/2.0 + paf[2];
         if (fPeak) {
            apTemp[2].p[0] = apTemp[0].p[0];
            apTemp[2].p[2] = (apTemp[1].p[2] + (paf[1]/2))/2 + fFrame;
         }
         else {
            apTemp[2].p[0] = apTemp[0].p[0];
            apTemp[2].p[2] = paf[1]/2.0 + fFrame;
         }
         apTemp[3].p[0] = 0;
         apTemp[3].p[2] = paf[1]/2.0 + fFrame;
         for (i = 4; i < 7; i++) {
            apTemp[i].Copy (&apTemp[6-i]);
            apTemp[i].p[0] *= -1;
         }
         memcpy (apKeep, apTemp, sizeof(apKeep));

         //if (fPeak) {
         //   adwTemp[1] = SEGCURVE_CUBIC;
         //   adwTemp[4] = SEGCURVE_CUBIC;
         //}
         //else {
            adwTemp[1] = SEGCURVE_ELLIPSENEXT;
            adwTemp[2] = SEGCURVE_ELLIPSEPREV;
            adwTemp[3] = SEGCURVE_ELLIPSENEXT;
            adwTemp[4] = SEGCURVE_ELLIPSEPREV;
         //}
         memcpy (adwKeep, adwTemp, sizeof(adwKeep));

         SplineInit (fBottom, &m_lOutside, TRUE, sizeof(apKeep)/sizeof(CPoint), apTemp,
            adwTemp, 2);

         // inside outlsine
         memcpy (apTemp, apKeep, sizeof(apKeep));
         memcpy (adwTemp, adwKeep, sizeof(adwKeep));
         for (i = 0; i < 3; i++)
            apTemp[i].p[0] += fFrame;
         for (i = 4; i < 7; i++)
            apTemp[i].p[0] -= fFrame;
         for (i = 2; i < 5; i++)
            apTemp[i].p[2] -= fFrame;
         if (api[0].iBase) {
            apTemp[0].p[2] += fFrame;
            apTemp[6].p[2] += fFrame;
         }
         api[0].pSpline = new CSpline;
         if (api[0].pSpline)
            SplineInit (fBottom, api[0].pSpline, TRUE, sizeof(apKeep)/sizeof(CPoint),
               apTemp, adwTemp, 2);

         // noodles
         memcpy (apTemp, apKeep, sizeof(apKeep));
         memcpy (adwTemp, adwKeep, sizeof(adwKeep));
         for (i = 0; i < 3; i++)
            apTemp[i].p[0] += fFrame/2;
         for (i = 4; i < 7; i++)
            apTemp[i].p[0] -= fFrame/2;
         for (i = 2; i < 5; i++)
            apTemp[i].p[2] -= fFrame/2;
         if (api[0].iBase) {
            apTemp[0].p[2] += fFrame/2;
            apTemp[6].p[2] += fFrame/2;
         }
         NoodlePath (fBottom, FALSE, api[0].iBase ? TRUE : FALSE,
            sizeof(apKeep)/sizeof(CPoint), apTemp, adwTemp, 2);
      }
      break;

   case DFS_ARCH2:
      {
         // bottom

         fHeight = paf[1];
         fBottom = - (fHeight / 2 + (api[0].iBase ? fFrame : 0));

         // outside outline
         CPoint apKeep[5];
         DWORD adwKeep[5];
         apTemp[0].p[0] = -(paf[0]/2 + fFrame);  // LL
         apTemp[0].p[2] = fBottom;
         apTemp[1].p[0] = apTemp[0].p[0];
         apTemp[1].p[2] = paf[1]/2.0 + fFrame;
         apTemp[2].p[0] = 0;
         apTemp[2].p[2] = apTemp[1].p[2];
         apTemp[3].p[0] = paf[0]/2 + fFrame;
         apTemp[3].p[2] = apTemp[2].p[2];
         apTemp[4].p[0] = apTemp[3].p[0];
         apTemp[4].p[2] = fBottom;
         memcpy (apKeep, apTemp, sizeof(apKeep));

         adwTemp[0] = SEGCURVE_ELLIPSENEXT;
         adwTemp[1] = SEGCURVE_ELLIPSEPREV;
         adwTemp[2] = SEGCURVE_ELLIPSENEXT;
         adwTemp[3] = SEGCURVE_ELLIPSEPREV;
         memcpy (adwKeep, adwTemp, sizeof(adwKeep));

         SplineInit (fBottom, &m_lOutside, TRUE, sizeof(apKeep)/sizeof(CPoint), apTemp,
            adwTemp, 2);

         // inside outlsine
         memcpy (apTemp, apKeep, sizeof(apKeep));
         memcpy (adwTemp, adwKeep, sizeof(adwKeep));
         for (i = 0; i < 2; i++)
            apTemp[i].p[0] += fFrame;
         for (i = 3; i < 5; i++)
            apTemp[i].p[0] -= fFrame;
         for (i = 1; i < 4; i++)
            apTemp[i].p[2] -= fFrame;
         if (api[0].iBase) {
            apTemp[0].p[2] += fFrame;
            apTemp[4].p[2] += fFrame;
         }
         api[0].pSpline = new CSpline;
         if (api[0].pSpline)
            SplineInit (fBottom, api[0].pSpline, TRUE, sizeof(apKeep)/sizeof(CPoint),
               apTemp, adwTemp, 2);

         // noodles
         memcpy (apTemp, apKeep, sizeof(apKeep));
         memcpy (adwTemp, adwKeep, sizeof(adwKeep));
         for (i = 0; i < 2; i++)
            apTemp[i].p[0] += fFrame/2;
         for (i = 3; i < 5; i++)
            apTemp[i].p[0] -= fFrame/2;
         for (i = 1; i < 4; i++)
            apTemp[i].p[2] -= fFrame/2;
         if (api[0].iBase) {
            apTemp[0].p[2] += fFrame/2;
            apTemp[4].p[2] += fFrame/2;
         }
         NoodlePath (fBottom, FALSE, api[0].iBase ? TRUE : FALSE,
            sizeof(apKeep)/sizeof(CPoint), apTemp, adwTemp, 2);
      }
      break;

   case DFS_ARCHHALF:
      {
         // bottom

         fHeight = paf[1];
         fBottom = - (fHeight / 2 + (api[0].iBase ? fFrame : 0));

         // outside outline
         CPoint apKeep[5];
         DWORD adwKeep[5];
         apTemp[0].p[0] = -(paf[0]/2 + fFrame);  // LL
         apTemp[0].p[2] = fBottom;
         apTemp[1].p[0] = apTemp[0].p[0]; // UL
         apTemp[1].p[2] = -paf[1]/2.0 + paf[2];
         apTemp[2].p[0] = apTemp[0].p[0];
         apTemp[2].p[2] = paf[1]/2.0 + fFrame;
         apTemp[3].p[0] = paf[0]/2 + fFrame;
         apTemp[3].p[2] = apTemp[2].p[2];
         apTemp[4].p[0] = apTemp[3].p[0];
         apTemp[4].p[2] = fBottom;
         memcpy (apKeep, apTemp, sizeof(apKeep));

         adwTemp[1] = SEGCURVE_ELLIPSENEXT;
         adwTemp[2] = SEGCURVE_ELLIPSEPREV;
         memcpy (adwKeep, adwTemp, sizeof(adwKeep));

         SplineInit (fBottom, &m_lOutside, TRUE, sizeof(apKeep)/sizeof(CPoint), apTemp,
            adwTemp, 2);

         // inside outlsine
         memcpy (apTemp, apKeep, sizeof(apKeep));
         memcpy (adwTemp, adwKeep, sizeof(adwKeep));
         for (i = 0; i < 3; i++)
            apTemp[i].p[0] += fFrame;
         for (i = 3; i < 5; i++)
            apTemp[i].p[0] -= fFrame;
         for (i = 2; i < 4; i++)
            apTemp[i].p[2] -= fFrame;
         if (api[0].iBase) {
            apTemp[0].p[2] += fFrame;
            apTemp[4].p[2] += fFrame;
         }
         api[0].pSpline = new CSpline;
         if (api[0].pSpline)
            SplineInit (fBottom, api[0].pSpline, TRUE, sizeof(apKeep)/sizeof(CPoint),
               apTemp, adwTemp, 2);

         // noodles
         memcpy (apTemp, apKeep, sizeof(apKeep));
         memcpy (adwTemp, adwKeep, sizeof(adwKeep));
         for (i = 0; i < 3; i++)
            apTemp[i].p[0] += fFrame/2;
         for (i = 3; i < 5; i++)
            apTemp[i].p[0] -= fFrame/2;
         for (i = 2; i < 4; i++)
            apTemp[i].p[2] -= fFrame/2;
         if (api[0].iBase) {
            apTemp[0].p[2] += fFrame/2;
            apTemp[4].p[2] += fFrame/2;
         }
         NoodlePath (fBottom, FALSE, api[0].iBase ? TRUE : FALSE,
            sizeof(apKeep)/sizeof(CPoint), apTemp, adwTemp, 2);
      }
      break;

   case DFS_ARCHHALF2:
      {
         // bottom

         fHeight = paf[1];
         fBottom = - (fHeight / 2 + (api[0].iBase ? fFrame : 0));

         // outside outline
         CPoint apKeep[4];
         DWORD adwKeep[4];
         apTemp[0].p[0] = -(paf[0]/2 + fFrame);  // LL
         apTemp[0].p[2] = fBottom;
         apTemp[1].p[0] = apTemp[0].p[0];
         apTemp[1].p[2] = paf[1]/2.0 + fFrame;
         apTemp[2].p[0] = paf[0]/2 + fFrame;
         apTemp[2].p[2] = apTemp[1].p[2];
         apTemp[3].p[0] = apTemp[2].p[0];
         apTemp[3].p[2] = fBottom;
         memcpy (apKeep, apTemp, sizeof(apKeep));

         adwTemp[0] = SEGCURVE_ELLIPSENEXT;
         adwTemp[1] = SEGCURVE_ELLIPSEPREV;
         memcpy (adwKeep, adwTemp, sizeof(adwKeep));

         SplineInit (fBottom, &m_lOutside, TRUE, sizeof(apKeep)/sizeof(CPoint), apTemp,
            adwTemp, 2);

         // inside outlsine
         memcpy (apTemp, apKeep, sizeof(apKeep));
         memcpy (adwTemp, adwKeep, sizeof(adwKeep));
         for (i = 0; i < 2; i++)
            apTemp[i].p[0] += fFrame;
         for (i = 2; i < 4; i++)
            apTemp[i].p[0] -= fFrame;
         for (i = 1; i < 3; i++)
            apTemp[i].p[2] -= fFrame;
         if (api[0].iBase) {
            apTemp[0].p[2] += fFrame;
            apTemp[3].p[2] += fFrame;
         }
         api[0].pSpline = new CSpline;
         if (api[0].pSpline)
            SplineInit (fBottom, api[0].pSpline, TRUE, sizeof(apKeep)/sizeof(CPoint),
               apTemp, adwTemp, 2);

         // noodles
         memcpy (apTemp, apKeep, sizeof(apKeep));
         memcpy (adwTemp, adwKeep, sizeof(adwKeep));
         for (i = 0; i < 2; i++)
            apTemp[i].p[0] += fFrame/2;
         for (i = 2; i < 4; i++)
            apTemp[i].p[0] -= fFrame/2;
         for (i = 1; i < 3; i++)
            apTemp[i].p[2] -= fFrame/2;
         if (api[0].iBase) {
            apTemp[0].p[2] += fFrame/2;
            apTemp[3].p[2] += fFrame/2;
         }
         NoodlePath (fBottom, FALSE, api[0].iBase ? TRUE : FALSE,
            sizeof(apKeep)/sizeof(CPoint), apTemp, adwTemp, 2);
      }
      break;

   case DFS_CIRCLE:
      {
         // bottom
         fHeight = paf[1];
         fBottom = - (fHeight / 2 + (api[0].iBase ? fFrame : 0));

         // outside outline
         CPoint apKeep[8];
         DWORD adwKeep[8];
         apTemp[0].p[0] = -paf[0]/2 - fFrame;
         apTemp[0].p[2] = 0;
         apTemp[1].p[0] = apTemp[0].p[0];
         apTemp[1].p[2] = paf[1]/2 + fFrame;
         apTemp[2].p[0] = 0;
         apTemp[2].p[2] = apTemp[1].p[2];
         apTemp[3].p[0] = -apTemp[0].p[0];
         apTemp[3].p[2] = apTemp[2].p[2];
         apTemp[4].p[0] = apTemp[3].p[0];
         apTemp[4].p[2] = 0;
         for (i = 5; i < 8; i++) {
            apTemp[i].Copy (&apTemp[8-i]);
            apTemp[i].p[2] *= -1;
         }
         memcpy (apKeep, apTemp, sizeof(apKeep));

         for (i = 0; i < 8; i++)
            adwTemp[i] = (i % 2) ? SEGCURVE_ELLIPSEPREV : SEGCURVE_ELLIPSENEXT;
         memcpy (adwKeep, adwTemp, sizeof(adwKeep));

         SplineInit (fBottom, &m_lOutside, TRUE, sizeof(apKeep)/sizeof(CPoint), apTemp,
            adwTemp, 2);

         // inside outlsine
         memcpy (apTemp, apKeep, sizeof(apKeep));
         memcpy (adwTemp, adwKeep, sizeof(adwKeep));
         for (i = 0; i < 2; i++)
            apTemp[i].p[0] += fFrame;
         apTemp[7].p[0] += fFrame;
         for (i = 3; i < 6; i++)
            apTemp[i].p[0] -= fFrame;

         for (i = 1; i < 4; i++)
            apTemp[i].p[2] -= fFrame;
         for (i = 5; i < 8; i++)
            apTemp[i].p[2] += fFrame;

         api[0].pSpline = new CSpline;
         if (api[0].pSpline)
            SplineInit (fBottom, api[0].pSpline, TRUE, sizeof(apKeep)/sizeof(CPoint),
               apTemp, adwTemp, 2);

         // noodles
         memcpy (apTemp, apKeep, sizeof(apKeep));
         memcpy (adwTemp, adwKeep, sizeof(adwKeep));
         for (i = 0; i < 2; i++)
            apTemp[i].p[0] += fFrame/2;
         apTemp[7].p[0] += fFrame/2;
         for (i = 3; i < 6; i++)
            apTemp[i].p[0] -= fFrame/2;

         for (i = 1; i < 4; i++)
            apTemp[i].p[2] -= fFrame/2;
         for (i = 5; i < 8; i++)
            apTemp[i].p[2] += fFrame/2;
         NoodlePath (fBottom, FALSE, api[0].iBase ? TRUE : FALSE,
            sizeof(apKeep)/sizeof(CPoint), apTemp, adwTemp, 2);
      }
      break;

   case DFS_ARCHCIRCLE:
      {
         // bottom
         fHeight = paf[1];
         fBottom = - (fHeight / 2 + (api[0].iBase ? fFrame : 0));

         // outside outline
         CPoint apKeep[3];
         DWORD adwKeep[3];
         apTemp[0].p[0] = -paf[0]/2 - fFrame;
         apTemp[0].p[2] = fBottom;
         apTemp[1].p[0] = 0;
         apTemp[1].p[2] = paf[1]/2 + fFrame;
         apTemp[2].p[0] = -apTemp[0].p[0];
         apTemp[2].p[2] = apTemp[0].p[2];
         memcpy (apKeep, apTemp, sizeof(apKeep));

         adwTemp[0] = SEGCURVE_CIRCLENEXT;
         adwTemp[1] = SEGCURVE_CIRCLEPREV;
         memcpy (adwKeep, adwTemp, sizeof(adwKeep));

         SplineInit (fBottom, &m_lOutside, TRUE, sizeof(apKeep)/sizeof(CPoint), apTemp,
            adwTemp, 3);

         // inside outlsine
         memcpy (apTemp, apKeep, sizeof(apKeep));
         memcpy (adwTemp, adwKeep, sizeof(adwKeep));
         apTemp[0].p[0] += fFrame;
         apTemp[2].p[0] -= fFrame;
         apTemp[1].p[2] -= fFrame;
         if (api[0].iBase) {
            apTemp[0].p[2] += fFrame;
            apTemp[2].p[2] += fFrame;
         }

         api[0].pSpline = new CSpline;
         if (api[0].pSpline)
            SplineInit (fBottom, api[0].pSpline, TRUE, sizeof(apKeep)/sizeof(CPoint),
               apTemp, adwTemp, 3);

         // noodles
         memcpy (apTemp, apKeep, sizeof(apKeep));
         memcpy (adwTemp, adwKeep, sizeof(adwKeep));
         apTemp[0].p[0] += fFrame/2;
         apTemp[2].p[0] -= fFrame/2;
         apTemp[1].p[2] -= fFrame/2;
         if (api[0].iBase) {
            apTemp[0].p[2] += fFrame/2;
            apTemp[2].p[2] += fFrame/2;
         }
         NoodlePath (fBottom, FALSE, api[0].iBase ? TRUE : FALSE,
            sizeof(apKeep)/sizeof(CPoint), apTemp, adwTemp, 3);
      }
      break;

   case DFS_ARCHPARTCIRCLE:
   case DFS_ARCHTRIANGLE:
      {
         // bottom
         fHeight = paf[1];
         fBottom = - (fHeight / 2 + (api[0].iBase ? fFrame : 0));

         // outside outline
         CPoint apKeep[5];
         DWORD adwKeep[5];
         apTemp[0].p[0] = -paf[0]/2 - fFrame;
         apTemp[0].p[2] = fBottom;
         apTemp[1].p[0] = apTemp[0].p[0];
         apTemp[1].p[2] = -paf[1]/2 + paf[2] - fFrame;
         apTemp[2].p[0] = 0;
         apTemp[2].p[2] = paf[1]/2 + fFrame;
         for (i = 3; i < 5; i++) {
            apTemp[i].Copy (&apTemp[4-i]);
            apTemp[i].p[0] *= -1;
         }
         memcpy (apKeep, apTemp, sizeof(apKeep));

         if (LOWORD(m_dwShape) != DFS_ARCHTRIANGLE) {
            adwTemp[1] = SEGCURVE_CIRCLENEXT;
            adwTemp[2] = SEGCURVE_CIRCLEPREV;
         }
         memcpy (adwKeep, adwTemp, sizeof(adwKeep));

         SplineInit (fBottom, &m_lOutside, TRUE, sizeof(apKeep)/sizeof(CPoint), apTemp,
            adwTemp, 3);

         // inside outlsine
         memcpy (apTemp, apKeep, sizeof(apKeep));
         memcpy (adwTemp, adwKeep, sizeof(adwKeep));
         for (i = 0; i < 2; i++)
            apTemp[i].p[0] += fFrame;
         for (i = 3; i < 5; i++)
            apTemp[i].p[0] -= fFrame;
         apTemp[2].p[2] -= fFrame;
         apTemp[1].p[2] += fFrame;
         apTemp[3].p[2] += fFrame;
         if (api[0].iBase) {
            apTemp[0].p[2] += fFrame;
            apTemp[4].p[2] += fFrame;
         }

         api[0].pSpline = new CSpline;
         if (api[0].pSpline)
            SplineInit (fBottom, api[0].pSpline, TRUE, sizeof(apKeep)/sizeof(CPoint),
               apTemp, adwTemp, 3);

         // noodles
         memcpy (apTemp, apKeep, sizeof(apKeep));
         memcpy (adwTemp, adwKeep, sizeof(adwKeep));
         for (i = 0; i < 2; i++)
            apTemp[i].p[0] += fFrame/2;
         for (i = 3; i < 5; i++)
            apTemp[i].p[0] -= fFrame/2;
         apTemp[2].p[2] -= fFrame/2;
         apTemp[1].p[2] += fFrame/2;
         apTemp[3].p[2] += fFrame/2;
         if (api[0].iBase) {
            apTemp[0].p[2] += fFrame/2;
            apTemp[4].p[2] += fFrame/2;
         }
         NoodlePath (fBottom, FALSE, api[0].iBase ? TRUE : FALSE,
            sizeof(apKeep)/sizeof(CPoint), apTemp, adwTemp, 3);
      }
      break;

   case DFS_TRIANGLERIGHT:
      {
         // bottom
         fHeight = paf[1];
         fBottom = - (fHeight / 2 + (api[0].iBase ? fFrame : 0));

         // outside outline
         CPoint apKeep[3];
         apTemp[0].p[0] = -paf[0]/2 - fFrame;
         apTemp[0].p[2] = fBottom;
         apTemp[1].p[0] = apTemp[0].p[0];
         apTemp[1].p[2] = paf[1]/2 + fFrame;
         apTemp[2].p[0] = -apTemp[0].p[0];
         apTemp[2].p[2] = apTemp[0].p[2];
         memcpy (apKeep, apTemp, sizeof(apKeep));

         SplineInit (fBottom, &m_lOutside, TRUE, sizeof(apKeep)/sizeof(CPoint), apTemp,
            (DWORD*) SEGCURVE_LINEAR, 0);

         // inside outlsine
         memcpy (apTemp, apKeep, sizeof(apKeep));
         for (i = 0; i < 2; i++)
            apTemp[i].p[0] += fFrame;
         apTemp[2].p[0] -= fFrame;
         apTemp[1].p[2] -= fFrame;
         if (api[0].iBase) {
            apTemp[0].p[2] += fFrame;
            apTemp[2].p[2] += fFrame;
         }

         api[0].pSpline = new CSpline;
         if (api[0].pSpline)
            SplineInit (fBottom, api[0].pSpline, TRUE, sizeof(apKeep)/sizeof(CPoint),
               apTemp, (DWORD*)SEGCURVE_LINEAR, 0);

         // noodles
         memcpy (apTemp, apKeep, sizeof(apKeep));
         for (i = 0; i < 2; i++)
            apTemp[i].p[0] += fFrame/2;
         apTemp[2].p[0] -= fFrame/2;
         apTemp[1].p[2] -= fFrame/2;
         if (api[0].iBase) {
            apTemp[0].p[2] += fFrame/2;
            apTemp[2].p[2] += fFrame/2;
         }
         NoodlePath (fBottom, FALSE, api[0].iBase ? TRUE : FALSE,
            sizeof(apKeep)/sizeof(CPoint), apTemp, (DWORD*)SEGCURVE_LINEAR, 0);
      }
      break;

   case DFS_TRIANGLEEQUI:
   case DFS_PENTAGON:
   case DFS_HEXAGON:
   case DFS_OCTAGON:
      {
         DWORD dwSides;
         if (LOWORD(m_dwShape) == DFS_PENTAGON)
            dwSides = 5;
         else if (LOWORD(m_dwShape) == DFS_HEXAGON)
            dwSides = 6;
         else if (LOWORD(m_dwShape) == DFS_OCTAGON)
            dwSides = 8;
         else
            dwSides = 3;

         // precalc some angles and stuff
         fp fInterval, fStart;
         fInterval = PI * 2.0 / (fp) dwSides;
         fStart = fInterval / 2;

         // find min and max
         CPoint pMin, pMax, pCur;
         for (i = 0; i < dwSides; i++) {
            pCur.Zero();
            pCur.p[0] = -sin(fStart + fInterval * i);
            pCur.p[2] = -cos(fStart + fInterval * i);

            if (i) {
               pMin.Min (&pCur);
               pMax.Max (&pCur);
            }
            else {
               pMin.Copy (&pCur);
               pMax.Copy (&pCur);
            }
         }
         
         // figure out the scale so get right width and height
         fp fX, fY;
         fX = paf[0] / (pMax.p[0] - pMin.p[0]);
         fY = paf[1] / (pMax.p[2] - pMin.p[2]);
         fX = max(CLOSE, fX);
         fY = max(CLOSE, fY);

         // bottom
         fHeight = -cos(fStart) * fY;
         fBottom = - (fHeight / 2 + (api[0].iBase ? fFrame : 0));

         // outside outline
         for (i = 0; i < dwSides; i++) {
            apTemp[i].Zero();
            apTemp[i].p[0] = -sin(fStart + fInterval * i) * fX;
            apTemp[i].p[2] = -cos(fStart + fInterval * i) * fY;
         }

         SplineInit (fBottom, &m_lOutside, TRUE, dwSides, apTemp,
            (DWORD*) SEGCURVE_LINEAR, 0, fFrame);

         // inside outlsine
         for (i = 0; i < dwSides; i++) {
            apTemp[i].Zero();
            apTemp[i].p[0] = -sin(fStart + fInterval * i) * fX;
            apTemp[i].p[2] = -cos(fStart + fInterval * i) * fY;
         }
         api[0].pSpline = new CSpline;
         if (api[0].pSpline)
            SplineInit (fBottom, api[0].pSpline, TRUE, dwSides,
               apTemp, (DWORD*)SEGCURVE_LINEAR, 0);

         // noodles
         for (i = 0; i < dwSides; i++) {
            apTemp[i].Zero();
            apTemp[i].p[0] = -sin(fStart + fInterval * i) * fX;
            apTemp[i].p[2] = -cos(fStart + fInterval * i) * fY;
         }
         NoodlePath (fBottom, FALSE, api[0].iBase ? TRUE : FALSE,
            dwSides, apTemp, (DWORD*)SEGCURVE_LINEAR, 0, fFrame/2);
      }
      break;

   case DFS_ARCHSPLIT:
      {
         // bottom

         fHeight = paf[1];
         fBottom = - (fHeight / 2 + (api[0].iBase ? fFrame : 0));

         // outside outline
         CPoint apKeep[7];
         DWORD adwKeep[7];
         apTemp[0].p[0] = -(paf[0]/2 + fFrame);  // LL
         apTemp[0].p[2] = fBottom;
         apTemp[1].p[0] = apTemp[0].p[0]; // UL
         apTemp[1].p[2] = paf[1]/2.0 + m_pSubFrameSize.p[0]/2;
         apTemp[2].p[0] = apTemp[0].p[0];
         apTemp[2].p[2] = paf[1]/2.0 + m_pSubFrameSize.p[0] + paf[2] + fFrame;;
         apTemp[3].p[0] = 0;
         apTemp[3].p[2] = apTemp[2].p[2];
         apTemp[4].p[0] = paf[0]/2 + fFrame;
         apTemp[4].p[2] = apTemp[2].p[2];
         apTemp[5].p[0] = apTemp[4].p[0];
         apTemp[5].p[2] = apTemp[1].p[2];
         apTemp[6].p[0] = apTemp[5].p[0];
         apTemp[6].p[2] = fBottom;
         memcpy (apKeep, apTemp, sizeof(apKeep));

         adwTemp[1] = SEGCURVE_ELLIPSENEXT;
         adwTemp[2] = SEGCURVE_ELLIPSEPREV;
         adwTemp[3] = SEGCURVE_ELLIPSENEXT;
         adwTemp[4] = SEGCURVE_ELLIPSEPREV;
         memcpy (adwKeep, adwTemp, sizeof(adwKeep));

         SplineInit (fBottom, &m_lOutside, TRUE, sizeof(apKeep)/sizeof(CPoint), apTemp,
            adwTemp, 2);

         // bottom box
         api[0].pSpline = new CSpline;
         if (api[0].pSpline)
            SplineInit (fBottom, api[0].pSpline,
               -paf[0]/2, paf[1]/2, paf[0]/2, -paf[1]/2, TRUE);   // BUGFIX - Always include bottom

         // inside outlsine
         memcpy (apTemp, apKeep, sizeof(apKeep));
         memcpy (adwTemp, adwKeep, sizeof(adwKeep));
         for (i = 0; i < 3; i++)
            apTemp[i].p[0] += fFrame;
         for (i = 4; i < 7; i++)
            apTemp[i].p[0] -= fFrame;
         for (i = 2; i < 5; i++)
            apTemp[i].p[2] -= fFrame;
         apTemp[1].p[2] += fSubFrame/2;
         apTemp[5].p[2] += fSubFrame/2;
         api[1].pSpline = new CSpline;
         if (api[1].pSpline)
            SplineInit (fBottom, api[1].pSpline, TRUE, 5,
               apTemp + 1, adwTemp + 1, 2);

         // noodles
         memcpy (apTemp, apKeep, sizeof(apKeep));
         memcpy (adwTemp, adwKeep, sizeof(adwKeep));
         for (i = 0; i < 3; i++)
            apTemp[i].p[0] += fFrame/2;
         for (i = 4; i < 7; i++)
            apTemp[i].p[0] -= fFrame/2;
         for (i = 2; i < 5; i++)
            apTemp[i].p[2] -= fFrame/2;
         if (api[0].iBase) {
            apTemp[0].p[2] += fFrame/2;
            apTemp[6].p[2] += fFrame/2;
         }
         NoodlePath (fBottom, FALSE, api[0].iBase ? TRUE : FALSE,
            sizeof(apKeep)/sizeof(CPoint), apTemp, adwTemp, 2);

         // split
         NoodleLinear (fBottom, TRUE,
            -paf[0]/2, paf[1]/2, paf[0]/2, paf[1]/2);
      }
      break;

   default:
      return;
   }

   m_fDirty = FALSE;
   return;
}



/********************************************************************************
GenerateThreeDFromDoorFrame - Given a doorframe, generate a 3D image of it

inputs
   PWSTR       pszControl - Control name.
   PCEscPage   pPage - Page
   PCDoorFrame pDoor - Door frame
returns
   BOOl - TRUE if success

NOTE: The ID's are such:
   LOBYTE = x
   3rd lowest byte = 1 for edge, 2 for point
*/
BOOL GenerateThreeDFromDoorFrame (PWSTR pszControl, PCEscPage pPage, PCDoorFrame pDoor)
{
   PCEscControl pControl = pPage->ControlFind (pszControl);
   if (!pControl)
      return FALSE;

   // figure out center
   CPoint pMin, pMax, pt;
   DWORD i;
   DWORD dwNum;
   PCSpline ps;
   ps = pDoor->OutsideGet();
   if (!ps)
      return FALSE;
   dwNum = ps->OrigNumPointsGet();
   for (i = 0; i < dwNum; i++) {
      ps->OrigPointGet(i % dwNum, &pt);

      if (!i) {
         pMin.Copy (&pt);
         pMax.Copy (&pt);
         continue;
      }

      pMin.Min (&pt);
      pMax.Max (&pt);
   }

   // and center
   CPoint pCenter;
   pCenter.Average (&pMin, &pMax);

   // figure out the maximum distance
   fp fMax;
   fMax = max(pMax.p[0] - pMin.p[0], pMax.p[2] - pMin.p[2]);
   // NOTE: assume p[1] == 0
   fMax = max(0.001, fMax);
   fMax /= 10;  // so is larger
   fMax = 1.0 / fMax;

   // when draw points, get the point, subtract the center, and divide by fMax

   // use gmemtemp
   MemZero (&gMemTemp);
   MemCat (&gMemTemp, L"<rotatex val=-90/><backculloff/>");


   CPoint ap[2];
   WCHAR szTemp[128];

   // outline
   ps = pDoor->OutsideGet();
   if (!ps)
      return FALSE;
   MemCat (&gMemTemp, L"<colordefault color=#404040/>");
   MemCat (&gMemTemp, L"<id val=1/>");
   dwNum = ps->QueryNodes();
   for (i = 0; i <= dwNum; i++) {
      ap[0].Copy (ps->LocationGet(i % dwNum));
      ap[1].Copy (ps->LocationGet ((i+1) % dwNum));
      ap[0].Subtract (&pCenter);
      ap[1].Subtract (&pCenter);
      ap[0].Scale (fMax);
      ap[1].Scale (fMax);

      MemCat (&gMemTemp, L"<shapearrow tip=false width=.2");
      swprintf (szTemp, L" p1=%g,%g,%g p2=%g,%g,%g/>",
         (double)ap[0].p[0], (double)ap[0].p[1], (double)ap[0].p[2], (double)ap[1].p[0], (double)ap[1].p[1], (double)ap[1].p[2]);
      MemCat (&gMemTemp, szTemp);
   }

   // inside bits
   DWORD dwOpen;
   for (dwOpen = 0; dwOpen < pDoor->OpeningsNum(); dwOpen++) {
      ps = pDoor->OpeningsSpline (dwOpen);
      if (!ps)
         continue;

      MemCat (&gMemTemp, L"<id val=");
      MemCat (&gMemTemp, (int) (dwOpen | 0x10000));
      MemCat (&gMemTemp, L"/>");

      MemCat (&gMemTemp, L"<colordefault color=#804040/>");
      dwNum = ps->QueryNodes();
      for (i = 0; i < dwNum; i++) {
         ap[0].Copy (ps->LocationGet(i % dwNum));
         ap[1].Copy (ps->LocationGet ((i+1) % dwNum));
         ap[0].Subtract (&pCenter);
         ap[1].Subtract (&pCenter);
         ap[0].Scale (fMax);
         ap[1].Scale (fMax);

         MemCat (&gMemTemp, L"<shapearrow tip=false width=.1");
         swprintf (szTemp, L" p1=%g,%g,%g p2=%g,%g,%g/>",
            (double)ap[0].p[0], (double)ap[0].p[1], (double)ap[0].p[2], (double)ap[1].p[0], (double)ap[1].p[1], (double)ap[1].p[2]);
         MemCat (&gMemTemp, szTemp);
      }

      // solid inside
      CPoint pCentOpen;
      for (i = 0; i < dwNum; i++) {
         pt.Copy (ps->LocationGet (i));
         if (i) {
            pMin.Min (&pt);
            pMax.Max (&pt);
         }
         else {
            pMin.Copy (&pt);
            pMax.Copy (&pt);
         }
      }
      pCentOpen.Average (&pMin, &pMax);
      pCentOpen.Subtract (&pCenter);
      pCentOpen.Scale (fMax);
      MemCat (&gMemTemp, L"<colordefault color=");
      switch (dwOpen % 6) {
      case 0:
         MemCat (&gMemTemp, L"#c0c0ff");
         break;
      case 1:
         MemCat (&gMemTemp, L"#c0ffc0");
         break;
      case 2:
         MemCat (&gMemTemp, L"#ffc0c0");
         break;
      case 3:
         MemCat (&gMemTemp, L"#c0ffff");
         break;
      case 4:
         MemCat (&gMemTemp, L"#ffffc0");
         break;
      case 5:
         MemCat (&gMemTemp, L"#ffc0ff");
         break;
      }
      MemCat (&gMemTemp, L"/>");
      for (i = 0; i < dwNum; i++) {
         ap[0].Copy (ps->LocationGet(i % dwNum));
         ap[1].Copy (ps->LocationGet ((i+1) % dwNum));
         ap[0].Subtract (&pCenter);
         ap[1].Subtract (&pCenter);
         ap[0].Scale (fMax);
         ap[1].Scale (fMax);

         MemCat (&gMemTemp, L"<shapepolygon");
         swprintf (szTemp, L" p1=%g,%g,%g p2=%g,%g,%g p3=%g,%g,%g/>",
            (double)ap[0].p[0], (double)ap[0].p[1], (double)ap[0].p[2], (double)ap[1].p[0], (double)ap[1].p[1], (double)ap[1].p[2],
            (double)pCentOpen.p[0], (double)pCentOpen.p[1], (double)pCentOpen.p[2]);
         MemCat (&gMemTemp, szTemp);
      }


   }
#if 0
   // draw the bits
   DWORD x;
   WCHAR szTemp[128];
   for (x = 0; x < dwNum; x++) {
      // if it's the last point and we're not looped then quit
      if ((x+1 >= dwNum) && !(pLeft->LoopedGet()))
         continue;

      DWORD h, v;
      CPoint ap[2][2];
      for (h = 0; h < 2; h++) for (v = 0; v < 2; v++) {
         PCSpline ps = h ? pRight : pLeft;
         ps->OrigPointGet ((x + v) % dwNum, &ap[h][v]);
         ps->OrigTextureGet ((x+v)%dwNum, &tp);
         ap[h][v].p[2] = tp.h;
         ap[h][v].p[3] = 1.0;

         // center and scale
         ap[h][v].Subtract (&pCenter);
         ap[h][v].Scale (1.0 / fMax);
      }


      // color
      if (dwUse == 0)
         MemCat (&gMemTemp, L"<colordefault color=#c0c0c0/>");
      else {
         DWORD dwSeg;
         pLeft->OrigSegCurveGet (x, &dwSeg);
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

      MemCat (&gMemTemp, L"<shapepolygon");

      for (i = 0; i < 4; i++) {
         CPoint pTemp;
         switch (i) {
         case 0: // UL
            pTemp.Copy (&ap[0][1]);
            break;
         case 1: // UR
            pTemp.Copy (&ap[1][1]);
            break;
         case 2: // LR
            pTemp.Copy (&ap[1][0]);
            break;
         case 3: // LL
            pTemp.Copy (&ap[0][0]);
            break;
         }

         swprintf (szTemp, L" p%d=%g,%g,%g",
            (int) i+1, (double)pTemp.p[0], (double)pTemp.p[1], (double)pTemp.p[2]);
         MemCat (&gMemTemp, szTemp);
      }
      MemCat (&gMemTemp, L"/>");

      // line
      if ((dwUse == 0) && (dwNum > 2)) {
         MemCat (&gMemTemp, L"<colordefault color=#ff0000/>");
         // set the ID
         MemCat (&gMemTemp, L"<id val=");
         MemCat (&gMemTemp, (int)((2 << 16) | x));
         MemCat (&gMemTemp, L"/>");

         MemCat (&gMemTemp, L"<shapearrow tip=false width=.2");

         WCHAR szTemp[128];
         swprintf (szTemp, L" p1=%g,%g,%g p2=%g,%g,%g/>",
            (double)ap[0][0].p[0], (double)ap[0][0].p[1], (double)ap[0][0].p[2], (double)ap[1][0].p[0], (double)ap[1][0].p[1], (double)ap[1][0].p[2]);
         MemCat (&gMemTemp, szTemp);
      }
   }
#endif // 0

   // set the threeD control
   ESCMTHREEDCHANGE tc;
   memset (&tc, 0, sizeof(tc));
   tc.pszMML = (PWSTR) gMemTemp.p;
   pControl->Message (ESCM_THREEDCHANGE, &tc);

   return TRUE;
}


/* DoorFrameShapePage
*/
BOOL DoorFrameShapePage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCDoorFrame pv = (PCDoorFrame) pPage->m_pUserData;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         // loop through all the controls
         DWORD i;
         PWSTR pszDoor = L"door";
         DWORD dwLen = (DWORD)wcslen(pszDoor);
         CDoorFrame df;
         for (i = 0; i < pPage->m_treeControls.Num(); i++) {
            PWSTR psz = pPage->m_treeControls.Enum(i);
            if (!psz)
               break;
            if (wcsncmp(psz, pszDoor, dwLen))
               continue;   // only looking for doors

            // create a door of the specified type
            df.Init (_wtoi(psz + dwLen), pv->m_dwDoor, 0);

            // draw the door
            GenerateThreeDFromDoorFrame (psz, pPage, &df);
         }
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Framing shape";
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}


/***********************************************************************************
CDoorFrame::DialogShape - Brings up a dialog that allows the user to change the
shape. It returns as soon as the user clicks the shape.

inputs
   PCEscWindow    pWindow - Window to display in
returns
   int - DFS_XXX, or -1 if pressed "Back", or -2 if pressed "Close"
*/
int CDoorFrame::DialogShape (PCEscWindow pWindow)
{
   PWSTR pszRet;
   pszRet = pWindow->PageDialog (ghInstance, IDR_MMLDOORFRAMESHAPE, DoorFrameShapePage, this);

   PWSTR pszDoor = L"door";
   DWORD dwLen = (DWORD)wcslen(pszDoor);

   if (!pszRet)
      return -2;
   if (!_wcsicmp(pszRet, Back()))
      return -1;
   if (!wcsncmp(pszRet, pszDoor, dwLen))
      return (int) _wtoi(pszRet + dwLen);

   // else
   return -2;
}

