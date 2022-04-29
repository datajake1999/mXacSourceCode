/**********************************************************************************
CDoorOpening.cpp - File for an doorway opening.

begun 27/4/02 by Mike Rozak
Copyright 2002 Mike Rozak. All rights reserved
*/

#include <windows.h>
#include "escarpment.h"
#include "..\M3D.h"
#include "resource.h"

/*********************************************************************************
CDoorOpening::Constructor and destructor*/
CDoorOpening::CDoorOpening (void)
{
   m_fOutside = FALSE;
   m_pYWallFrame.Zero4();
   m_apExtend[0].Zero4();
   m_apExtend[1].Zero4();
   m_fDirty = TRUE;
   memset (m_apDoorSet, 0, sizeof(m_apDoorSet));
}

CDoorOpening::~CDoorOpening (void)
{
   // free the door sets
   DWORD i;
   for (i = 0; i < 6; i++) {
      if (m_apDoorSet[i])
         delete m_apDoorSet[i];
      m_apDoorSet[i] = 0;
   }
}


/*********************************************************************************
CDoorOpening::ShapeSet - Sets the shape of the opening.

inputs
   PCSpline       psShape - Shape. X and Z are used. Y ignored. Clockwise direction.
   BOOL           fOutside - Set to TRUE if Y==-1 is outside, FALSE if it's inside.
   fp         fYWallOutside - Y value of the outside wall
   fp         fYWallInside - Y value of the inside wall
   fp         fYFrameOutside - Y value of the outside frame
   fp         fYFrameInside - Y value fo the inside frame
returns
   BOOL - TRUE if succes
*/
BOOL CDoorOpening::ShapeSet (PCSpline psShape, BOOL fOutside,
                             fp fYWallOutside, fp fYWallInside,
                             fp fYFrameOutside, fp fYFrameInside)
{
   if (!psShape->LoopedGet())
      return FALSE;

   m_fDirty = TRUE;
   psShape->CloneTo (&m_sShape);
   m_fOutside = fOutside;
   m_pYWallFrame.p[0] = fYWallOutside;
   m_pYWallFrame.p[1] = fYWallInside;
   m_pYWallFrame.p[2] = fYFrameOutside;
   m_pYWallFrame.p[3] = fYFrameInside;

   return TRUE;
}

/*********************************************************************************
CDoorOpening::ShapeGet - Gets the shape of the opening.

inputs
   BOOL           *pfOutside - Set to TRUE if Y==-1 is outside, FALSE if it's inside.
   fp         *pfYWallOutside - Y value of the outside wall
   fp         *pfYWallInside - Y value of the inside wall
   fp         *pfYFrameOutside - Y value of the outside frame
   fp         *pfYFrameInside - Y value fo the inside frame
returns
   PCSpline - Shape. Don't change this. only valid while the object is around.
*/
PCSpline CDoorOpening::ShapeGet (BOOL *pfOutside,
                             fp *pfYWallOutside, fp *pfYWallInside,
                             fp *pfYFrameOutside, fp *pfYFrameInside)
{
   if (pfOutside)
      *pfOutside = m_fOutside;
   if (pfYWallOutside)
      *pfYWallOutside = m_pYWallFrame.p[0];
   if (pfYWallInside)
      *pfYWallInside = m_pYWallFrame.p[1];
   if (pfYFrameOutside)
      *pfYFrameOutside = m_pYWallFrame.p[2];
   if (pfYFrameInside)
      *pfYFrameInside = m_pYWallFrame.p[3];
   return &m_sShape;
}

/*********************************************************************************
CDoorOpening::StyleInit - Once ShapeSet() is called, ths initializes all the
   CDoorSet objects and ExtendSet() info according to a style paramter number
   from DSSTYLE_XXX.

   Use this when creating a particular style directly from the object list.

inputs
   DWORD    dwStyle - style to use. Combination of DSSTYLE_XXX and DS_XXX, to
   specify how it opens and the type of door used. Same info as passed into
   CDoorSet::StyleSet();
returns
   BOOL - TRUE if success
*/
BOOL CDoorOpening::StyleInit (DWORD dwStyle)
{
   // set dirty
   m_fDirty = TRUE;

   // free the door sets
   DWORD i;
   for (i = 0; i < 6; i++) {
      if (m_apDoorSet[i])
         delete m_apDoorSet[i];
      m_apDoorSet[i] = 0;
   }

   // by default, create a door set on the inside flush with frame
   DWORD dwLoc;
   BOOL  fWindow, fCabinet, fNeedScreen;
   fWindow = (dwStyle & DSSTYLE_WINDOW) ? TRUE : FALSE;
   fCabinet = (dwStyle & DSSTYLE_CABINET) ? TRUE : FALSE;
   fNeedScreen = FALSE;
   dwLoc = 4;  // default to being on the inside, flush with frame
   switch (dwStyle & DSSTYLE_BITS) {
   case DSSTYLE_FIXED:   // fixed
      dwLoc = 3;  // center this
      break;
   case DSSTYLE_HINGEL:   // hinged, left, 1 division
   case DSSTYLE_HINGEL2:   // hinged, left, 1 division, extrior
   case DSSTYLE_HINGER:   // hinged, right, 1 division
   case DSSTYLE_HINGELR:   // hinged, left and right, 2 divisions
   case DSSTYLE_HINGEU:   // hinged, up, 1 dicision
   case DSSTYLE_HINGELR2:   // hinged, left/right, 2 divisions, custom shape - saloon door
      if (fWindow || fCabinet)
         dwLoc = 1;  // open out
      break;
   case DSSTYLE_HINGELO:   // hinged, left, 1 division
   case DSSTYLE_HINGERO:   // hinged, right, 1 division
   case DSSTYLE_HINGELRO:   // hinged, left and right, 2 divisions
      fp fExtendAmt;
      dwLoc = 0;  // all the way on the outside
      fExtendAmt = fCabinet ? .02 : .05;
      m_apExtend[0].p[0] = m_apExtend[0].p[1] = m_apExtend[0].p[2] = fExtendAmt;
      m_apExtend[0].p[3] = (fWindow || fCabinet) ? fExtendAmt : 0;
      m_apExtend[1].p[0] = m_apExtend[1].p[1] = m_apExtend[1].p[2] = fExtendAmt;
      m_apExtend[1].p[3] = (fWindow || fCabinet) ? fExtendAmt : 0;
      break;
   case DSSTYLE_BIL:   // bifold, left 4 divisions
   case DSSTYLE_BILR:   // bifold, left/right 4 divisions
      break;
   case DSSTYLE_SLIDEL:   // slide, left, 2 divisions
   case DSSTYLE_SLIDEU:   // slide, up, 2 divisions
      if (fWindow)
         fNeedScreen = TRUE;
      break;
   case DSSTYLE_POCKLR:   // pocket, left/right, 2 divisions
   case DSSTYLE_POCKU:   // pocket up, 1 division
   case DSSTYLE_POCKL:   // pocket, left, 1 division
      dwLoc = 3;  // centered
      if (fWindow)
         fNeedScreen = TRUE;
      break;
   case DSSTYLE_POCKLR2:  // pocket, left/right, 2 divisions, on outside
      dwLoc = 0;  // all the way on the outside
      break;
   case DSSTYLE_GARAGEU:   // garage, up, 4 divisions
   case DSSTYLE_GARAGEU2:   // garage up, 1 division
   case DSSTYLE_ROLLERU:   // roller up, 8 divisions
      dwLoc = 5;  // all the way on the inside
      if (fWindow)
         fNeedScreen = TRUE;
      break;
   case DSSTYLE_LOUVER:   // pivot, up(?), division for louver height
      dwLoc = 4;  // towards inside
      if (fWindow)
         fNeedScreen = TRUE;
      break;
   }
   DoorSetAdd (dwLoc, dwStyle);

   // create screen windows?
   if (fNeedScreen) {
      DoorSetAdd (1, fWindow ? (DSSTYLE_WINDOW | DSSTYLE_FIXED | DS_WINDOWSCREEN) :
         (DSSTYLE_FIXED | DS_DOORSCREEN));
   }


   return TRUE;
}

/*********************************************************************************
CDoorOpening::ExtendSet - Those door sets completely outside the frame can be
extended beyond the frame to cover it. For example: Cabinet doors that cover the
framed opening, or sliding barn doors that cover the opening.

inputs
   BOOL        fOutside - TRUE if it's the outside expanded, FALSE if it's inside
   fp      fLeft, fRight, fTop, fBottom - Amount to extend the door on these ends.
returns
   none
*/
void CDoorOpening::ExtendSet (BOOL fOutside, fp fLeft, fp fRight, fp fTop, fp fBottom)
{
   m_fDirty = TRUE;
   m_apExtend[!fOutside].p[0] = fLeft;
   m_apExtend[!fOutside].p[1] = fRight;
   m_apExtend[!fOutside].p[2] = fTop;
   m_apExtend[!fOutside].p[3] = fBottom;
}

/*********************************************************************************
CDoorOpening::ExtendGet - Gets the amount the door is extended on the two outside locs.

inputs
   BOOL        fOutside - TRUE if it's the outside expanded, FALSE if it's inside
   fp      *pfLeft, *pfRight, *pfTop, *pfBottom - Amount to extend the door on these ends.
returns
   none
*/
void CDoorOpening::ExtendGet (BOOL fOutside, fp *pfLeft, fp *pfRight, fp *pfTop, fp *pfBottom)
{
   if (pfLeft)
      *pfLeft = m_apExtend[!fOutside].p[0];
   if (pfRight)
      *pfRight = m_apExtend[!fOutside].p[1];
   if (pfTop)
      *pfTop = m_apExtend[!fOutside].p[2];
   if (pfBottom)
      *pfBottom = m_apExtend[!fOutside].p[3];
}


/**********************************************************************************
CDoorOpening::Clone - Clones the door.

returns
   CDoor *- CLone
*/
CDoorOpening * CDoorOpening::Clone (void)
{
   PCDoorOpening pNew = new CDoorOpening;
   if (!pNew)
      return NULL;

   // free the door sets
   DWORD i;
   for (i = 0; i < 6; i++) {
      if (pNew->m_apDoorSet[i])
         delete pNew->m_apDoorSet[i];
      pNew->m_apDoorSet[i] = 0;
   }

   m_sShape.CloneTo (&pNew->m_sShape);
   pNew->m_fOutside = m_fOutside;
   pNew->m_pYWallFrame.Copy (&m_pYWallFrame);
   pNew->m_apExtend[0].Copy (&m_apExtend[0]);
   pNew->m_apExtend[1].Copy (&m_apExtend[1]);
   pNew->m_fDirty = m_fDirty;

   for (i = 0; i < 6; i++) {
      if (!m_apDoorSet[i])
         continue;
      pNew->m_apDoorSet[i] = m_apDoorSet[i]->Clone();
   }

   return pNew;
}

static PWSTR gpszDoorOpening = L"DoorOpening";
static PWSTR gpszShape = L"Shape";
static PWSTR gpszYWallFrame = L"YWallFrame";
static PWSTR gpszExtend0 = L"Extend0";
static PWSTR gpszExtend1 = L"Extend1";
static PWSTR gpszOutside = L"Outside";

/**********************************************************************************
CDoorOpening::MMLTo - Writes the door to MML.

returns
   PCMMLNode2 - Node that contains the door information
*/
PCMMLNode2 CDoorOpening::MMLTo (void)
{
   PCMMLNode2 pNode = new CMMLNode2;
   if (!pNode)
      return NULL;
   pNode->NameSet (gpszDoorOpening);

   PCMMLNode2 pSub;
   pSub = m_sShape.MMLTo();
   if (pSub) {
      pSub->NameSet (gpszShape);
      pNode->ContentAdd (pSub);
   }

   DWORD i;
   for (i = 0; i < 6; i++) {
      if (!m_apDoorSet[i])
         continue;

      pSub = m_apDoorSet[i]->MMLTo ();
      if (!pSub)
         continue;

      WCHAR szTemp[32];
      swprintf (szTemp, L"DoorSet%d", (int) i);
      pSub->NameSet (szTemp);
      pNode->ContentAdd (pSub);
   }

   MMLValueSet (pNode, gpszOutside, (int)m_fOutside);
   MMLValueSet (pNode, gpszYWallFrame, &m_pYWallFrame);
   MMLValueSet (pNode, gpszExtend0, &m_apExtend[0]);
   MMLValueSet (pNode, gpszExtend1, &m_apExtend[1]);
   // note: not setting dirty

   return pNode;
}

/**********************************************************************************
CDoorOpening::MMLFrom - Reads in information about the door.

inputs
   PCMMLNode2      pNode - TO read from
returns
   BOOL - TRUE if success
*/
BOOL CDoorOpening::MMLFrom (PCMMLNode2 pNode)
{
   // free the door sets
   DWORD i;
   for (i = 0; i < 6; i++) {
      if (m_apDoorSet[i])
         delete m_apDoorSet[i];
      m_apDoorSet[i] = 0;
   }

   PWSTR psz;
   PCMMLNode2 pSub;
   for (i = 0; i < pNode->ContentNum(); i++) {
      pSub = NULL;
      if (!pNode->ContentEnum (i, &psz, &pSub))
         continue;
      if (!pSub)
         continue;
      psz = pSub->NameGet();
      if (!psz)
         continue;

      PWSTR pszDoorSet = L"DoorSet";
      DWORD dwLen = (DWORD)wcslen(pszDoorSet);
      if (!_wcsicmp(psz, gpszShape)) {
         m_sShape.MMLFrom (pSub);
      }
      else if (!wcsncmp(psz, pszDoorSet, dwLen)) {
         DWORD dwSet = _wtoi(psz + dwLen);
         if (dwSet >= 6)
            continue;
         m_apDoorSet[dwSet] = new CDoorSet;
         if (!m_apDoorSet[dwSet])
            continue;
         m_apDoorSet[dwSet]->MMLFrom (pSub);
      }
   }

   m_fOutside = (BOOL) MMLValueGetInt (pNode, gpszOutside, FALSE);
   CPoint pZero;
   pZero.Zero4();
   MMLValueGetPoint (pNode, gpszYWallFrame, &m_pYWallFrame, &pZero);
   MMLValueGetPoint (pNode, gpszExtend0, &m_apExtend[0], &pZero);
   MMLValueGetPoint (pNode, gpszExtend1, &m_apExtend[1], &pZero);

   // note: not reading dirty
   m_fDirty = TRUE;

   return NULL;
}

/**********************************************************************************
CDoorOpening::SurfaceQuery - Returns a DWORD with a bitfield indicating what types
of surfaces are on the door (glass, framing, etc.). This is used to determine what
colors are needed for the object calling.

returns
   DWORD - bitfield of DSURF_XXX types
*/
DWORD CDoorOpening::SurfaceQuery (void)
{
   DWORD dwSurface, i, dw;
   dwSurface = 0;

   CalcIfNecessary();

   for (i = 0; i < 6; i++)
      if (m_apDoorSet[i]) {
         dw = m_apDoorSet[i]->SurfaceQuery ();

         // flip external and internal since the opening doesnt really know this
         // information, but assumes external is out
         if (i >= 3) {
            DWORD dwIntF, dwExtF, dwExtP, dwIntP;
            dwIntF = dw & DSURF_INTFRAME;
            dwExtF = dw & DSURF_EXTFRAME;
            dwIntP = dw & DSURF_INTPANEL;
            dwExtP = dw & DSURF_EXTPANEL;
            dw = dw & ~(DSURF_INTFRAME | DSURF_EXTFRAME | DSURF_INTPANEL | DSURF_EXTPANEL) |
               (dwIntF ? DSURF_EXTFRAME : 0) | (dwExtF ? DSURF_INTFRAME : 0) |
               (dwIntP ? DSURF_EXTPANEL : 0) | (dwExtP ? DSURF_INTPANEL : 0);
         }

         dwSurface |= dw;
      }

   return dwSurface;
}


/**********************************************************************************
CDoorOpening::CalcIfNecessary - If the dirty flag is set then this recalculates all the
necessary doors.
*/
void CDoorOpening::CalcIfNecessary (void)
{
   if (!m_fDirty)
      return;

   // loop through all the door sets
   DWORD i, j;
   CMem memPoints, memCurve;
   DWORD dwNum, dwMin, dwMax;
   fp fDetail;
   dwNum = m_sShape.OrigNumPointsGet();
   m_sShape.DivideGet (&dwMin, &dwMax, &fDetail);
   if (!memPoints.Required(dwNum * sizeof(CPoint)) || !memCurve.Required(dwNum * sizeof(DWORD)))
      return;   // error with memory
   PCPoint pap;
   DWORD *padw;
   pap = (PCPoint) memPoints.p;
   padw = (DWORD*) memCurve.p;

   for (i = 0; i < 6; i++) {
      if (!m_apDoorSet[i])
         continue;

      // get the contents of the spline because may need to muck with
      for (j = 0; j < dwNum; j++) {
         m_sShape.OrigPointGet (j, &pap[j]);
         m_sShape.OrigSegCurveGet (j, &padw[j]);
      }

      // expand on edges
      if ((i == 0) || (i == 5)) {
         DWORD dw = (i == 0) ? 0 : 1;

         // find the center
         CPoint pMin, pMax, pC;
         for (j = 0; j < dwNum; j++) {
            if (j) {
               pMin.Min (&pap[j]);
               pMax.Max (&pap[j]);
            }
            else {
               pMin.Copy (&pap[j]);
               pMax.Copy (&pap[j]);
            }
         }
         pC.Average (&pMin, &pMax);

         // expand
         for (j = 0; j < dwNum; j++) {
            // left and right
            if (pap[j].p[0] > pC.p[0] + CLOSE)
               pap[j].p[0] += (pap[j].p[0] - pC.p[0]) / (pMax.p[0]-pC.p[0]) * m_apExtend[dw].p[1];
            else if (pap[j].p[0] < pC.p[0] - CLOSE)
               pap[j].p[0] += (pap[j].p[0] - pC.p[0]) / (pMax.p[0]-pC.p[0]) * m_apExtend[dw].p[0];

            // top and bottom
            if (pap[j].p[2] > pC.p[2] + CLOSE)
               pap[j].p[2] += (pap[j].p[2] - pC.p[2]) / (pMax.p[2]-pC.p[2]) * m_apExtend[dw].p[2];
            else if (pap[j].p[2] < pC.p[2] - CLOSE)
               pap[j].p[2] += (pap[j].p[2] - pC.p[2]) / (pMax.p[2]-pC.p[2]) * m_apExtend[dw].p[3];
         }
      }

      // do we need to reverse
      if ( (m_fOutside && (i >= 3)) || (!m_fOutside && (i < 3)) ) {
         // reverse the spline
         CPoint t;
         for (j = 0; j < dwNum/2; j++) {
            t.Copy (&pap[j]);
            pap[j].Copy (&pap[dwNum-j-1]);
            pap[dwNum-j-1].Copy (&t);
         }

         // flip sign of X
         for (j = 0; j < dwNum; j++)
            pap[j].p[0] *= -1;

         DWORD dwVal;
         for (j = 0; j < dwNum; j++) {
            dwVal = padw[j];
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
            padw[j] = dwVal;
         }

         for (j = 0; j < dwNum/2; j++) {
            dwVal = padw[j];
            padw[j] = padw[dwNum-j-1];
            padw[dwNum-j-1] = dwVal;
         }

         // BUGFIX - If looped need to rotate around 1...
         dwVal = padw[0];
         memmove (padw + 0, padw + 1, (dwNum-1) * sizeof(DWORD));
         padw[dwNum-1] = dwVal;

      }  // need to reverse

      // convert to spline
      CSpline s;
      s.Init (TRUE, dwNum, pap, NULL, padw, dwMin, dwMax, fDetail);

      // set the size
      m_apDoorSet[i]->ShapeSet (&s);

   } // over all openings

   m_fDirty = FALSE;
}

/**********************************************************************************
CDoorOpening::Render - Tells the door to render itself. It will be within the original X and Z
coordinates given, but will go from 0 to m_fTHickness

inputs
   POBJECTRENDER        pr - Rendering information
   PCRenderSurface      *prs - Render to.
   DWORD                dwSurface - Which surfaces to render. From DSURF_XXX
   fp               fOpen - Amount that it's open. 0 for closed. 1 for opened as much as possible
returns
   none
*/
void CDoorOpening::Render (POBJECTRENDER pr, CRenderSurface *prs, DWORD dwSurface, fp fOpen)
{
   CalcIfNecessary();

   // loop over all the openings
   DWORD i;
   for (i = 0; i < 6; i++) {
      if (!m_apDoorSet[i])
         continue;

      // push matrix
      prs->Push();

      // find out where this goes
      fp fPos, fThickness, fOutside;
      fThickness = m_apDoorSet[i]->ThicknessGet();
      fOutside = (m_fOutside ? -1 : 1);   // points in direction of outside

      switch (i) {
      case 0:  // beyond the outside frame
         fPos = (max(fOutside * m_pYWallFrame.p[0], fOutside * m_pYWallFrame.p[2]) + fThickness) * fOutside;
         break;
      case 1:  // just in the outide frame
         fPos = m_pYWallFrame.p[2];
         break;
      case 2:  // center, outside facing
         fPos = (m_pYWallFrame.p[2] + m_pYWallFrame.p[3]) / 2 + fThickness * fOutside / 2.0;
         break;
      case 3:  // center, inside facing
         fPos = (m_pYWallFrame.p[2] + m_pYWallFrame.p[3]) / 2 - fThickness * fOutside / 2.0;
         break;
      case 4:  // just in the inside frame
         fPos = m_pYWallFrame.p[3];
         break;
      case 5:  // beyond the inside frame
         fPos = (min(fOutside * m_pYWallFrame.p[1], fOutside * m_pYWallFrame.p[3]) - fThickness) * fOutside;
         break;
      }


      // flip external and internal since the opening doesnt really know this
      // information, but assumes external is out
      DWORD dw;
      dw = dwSurface;
      if (i >= 3) {
         DWORD dwIntF, dwExtF, dwExtP, dwIntP;
         dwIntF = dw & DSURF_INTFRAME;
         dwExtF = dw & DSURF_EXTFRAME;
         dwIntP = dw & DSURF_INTPANEL;
         dwExtP = dw & DSURF_EXTPANEL;
         dw = dw & ~(DSURF_INTFRAME | DSURF_EXTFRAME | DSURF_INTPANEL | DSURF_EXTPANEL) |
            (dwIntF ? DSURF_EXTFRAME : 0) | (dwExtF ? DSURF_INTFRAME : 0) |
            (dwIntP ? DSURF_EXTPANEL : 0) | (dwExtP ? DSURF_INTPANEL : 0);
      }

      // translate so the Y value will be at the correct location
      prs->Translate (0, fPos, 0);

      // rotate 180 degrees
      if ( (m_fOutside && (i >= 3)) || (!m_fOutside && (i < 3)) )
         prs->Rotate (PI, 3);

      // draw it
      m_apDoorSet[i]->Render (pr, prs, dw, fOpen);
      // pop
      prs->Pop();
   }
}

/**********************************************************************************
CDoorOpening::DoorSetAdd - Adds a new opening

inputs
   DWORD    dwPosn - 0 to 6
   DWORD    dwStyle - Passed down to DoorSet::StyleSet()
returns
   BOOL - TRUE if success, FALSE if error, like bay already occupied
*/
BOOL CDoorOpening::DoorSetAdd (DWORD dwPosn, DWORD dwStyle)
{
   if (dwPosn >= 6)
      return FALSE;
   if (m_apDoorSet[dwPosn])
      return FALSE;

   m_apDoorSet[dwPosn] = new CDoorSet;
   if (!m_apDoorSet[dwPosn])
      return FALSE;
   // NOTE: Not setting the location yet because will do that in CalcIfNecessary()

   m_apDoorSet[dwPosn]->StyleSet (dwStyle);
   return TRUE;
}



/**********************************************************************************
CDoorOpening::DoorSetRemove - Removes an opening.

inputs
   DWORD    dwPosn - 0 to 6
returns
   BOOL - TRUE if success, FALSE if error, like bay doesnt exist
*/
BOOL CDoorOpening::DoorSetRemove (DWORD dwPosn)
{
   if (dwPosn >= 6)
      return FALSE;
   if (!m_apDoorSet[dwPosn])
      return FALSE;
   delete m_apDoorSet[dwPosn];
   m_apDoorSet[dwPosn] = NULL;
   m_fDirty = TRUE;
   return TRUE;
}


/**********************************************************************************
CDoorOpening::DoorSetGet - Retursn the PCDoorSet object. NOTE: Do NOT delete.

inputs
   DWORD    dwPosn - 0 to 6
returns
   PCDoorSet - door set. NULL if error
*/
PCDoorSet CDoorOpening::DoorSetGet (DWORD dwPosn)
{
   if (dwPosn >= 6)
      return FALSE;
   return m_apDoorSet[dwPosn];
}





/**********************************************************************************
CDoorOpening::DoorSetFlip - Flips the door front to back

inputs
   DWORD    dwPosn - 0 to 6
returns
   BOOL - TRUE if success, FALSE if error, like bay doesnt exist
*/
BOOL CDoorOpening::DoorSetFlip (DWORD dwPosn)
{
   if (dwPosn >= 6)
      return FALSE;
   PCDoorSet p;
   p = m_apDoorSet[dwPosn];
   m_apDoorSet[dwPosn] = m_apDoorSet[5-dwPosn];
   m_apDoorSet[5-dwPosn] = p;
   m_fDirty = TRUE;
   return TRUE;
}

// FUTURERELEASE - Way of also creating shutters on window?

// FUTURERELEASE - Way of having screen door?
   
