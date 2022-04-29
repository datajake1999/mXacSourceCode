/************************************************************************
CObjectUniHole.cpp - Draws a box.

begun 16/12/01 by Mike Rozak
Copyright 2001 Mike Rozak. All rights reserved
*/

#include <windows.h>
#include "escarpment.h"
#include "..\M3D.h"
#include "resource.h"


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

/**********************************************************************************
CObjectUniHole::Constructor and destructor */
CObjectUniHole::CObjectUniHole (PVOID pParams, POSINFO pInfo)
{
   m_OSINFO = *pInfo;

   m_dwRenderShow = RENDERSHOW_DOORS;

   // store away the type
   m_dwType = (DWORD)(size_t) pParams;

   // Based on the type use the univeral hole
   CPoint pt;
   CListFixed list;
   list.Init (sizeof(CPoint));
   pt.Zero();
   DWORD dwSegCurve, dwDivide;
   dwSegCurve = SEGCURVE_LINEAR;
   dwDivide = 0;
   switch (m_dwType) {
   case 0:  // rectangular
      list.Required (4);

      // UL
      pt.p[0] = -.5;
      pt.p[2] = .5;
      list.Add (&pt);

      // UR
      pt.p[0] = .5;
      list.Add (&pt);

      // LR
      pt.p[2] = -.5;
      list.Add (&pt);

      // LL
      pt.p[0] = -.5;
      list.Add (&pt);
      break;
   case 1:  // elliptical
      list.Required (8);

      pt.p[0] = -.5;
      list.Add (&pt);

      pt.p[2] = .5;
      list.Add (&pt);

      pt.p[0] = 0;
      list.Add (&pt);

      pt.p[0] = .5;
      list.Add (&pt);

      pt.p[2] = 0;
      list.Add (&pt);

      pt.p[2] = -.5;
      list.Add (&pt);

      pt.p[0] = 0;
      list.Add (&pt);

      pt.p[0] = -.5;
      list.Add (&pt);

      dwSegCurve = SEGCURVE_ELLIPSENEXT;
      dwDivide = 1;
      break;

   case 2:  // any shape
   default:
      list.Required (3);

      pt.p[0] = -.5;
      pt.p[2] = -.5;
      list.Add (&pt);

      pt.p[0] = 0;
      pt.p[2] = .5;
      list.Add (&pt);

      pt.p[0] = .5;
      pt.p[2] = -.5;
      list.Add (&pt);

      dwSegCurve = SEGCURVE_CUBIC;
      dwDivide = 2;
      break;
   }
   m_Spline.Init (TRUE, list.Num(), (PCPoint)list.Get(0), NULL, (DWORD*)(size_t) dwSegCurve,
      dwDivide, dwDivide, .1);

   m_fCanBeEmbedded = TRUE;

   // color for the box
   // BUGFIX - linked to exposed frame category so matches with doors and doorways
   ObjectSurfaceAdd (10, RGB(0xff,0xff,0xff), MATERIAL_PAINTSEMIGLOSS, L"Exposed frame");
   //ObjectSurfaceAdd (10, RGB(0xff,0xff,0x40));
}


CObjectUniHole::~CObjectUniHole (void)
{
   // do nothing for now
}


/**********************************************************************************
CObjectUniHole::Delete - Called to delete this object
*/
void CObjectUniHole::Delete (void)
{
   delete this;
}

/**********************************************************************************
CObjectUniHole::Render - Draws a test box.

inputs
   POBJECTRENDER     pr - Render information
*/
void CObjectUniHole::Render (POBJECTRENDER pr, DWORD dwSubObject)
{
   // create the surface render object and draw
   // CRenderSurface rs;
   m_Renderrs.ClearAll();

   CMatrix mObject;
   m_Renderrs.Init (pr->pRS);
   ObjectMatrixGet (&mObject);
   m_Renderrs.Multiply (&mObject);

   // object specific
   m_Renderrs.SetDefMaterial (m_OSINFO.dwRenderShard, ObjectSurfaceFind (10), m_pWorld);

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
         p->p.p[3] = 1;
         p->p.MultiplyLeft (&mInv);
      }
      for (i = 0; i < l2.Num(); i++) {
         p = (PHVXYZ) l2.Get(i);
         p->p.p[3] = 1;
         p->p.MultiplyLeft (&mInv);
      }


      // zipepr it
      m_Renderrs.ShapeZipper ((PHVXYZ)l1.Get(0), l1.Num(), (PHVXYZ)l2.Get(0), l2.Num(), TRUE);

      m_Renderrs.Commit();
      return;
   }

drawit:
   // Call this in case it's not embedded

   // just draw some polygons. This doesn't have to be so great because
   // the universal hole is supposed to be embedded.

   fp fThickness;
   fThickness = m_fContainerDepth ? m_fContainerDepth : .3;

   // just create the zippers
   CListFixed lz1, lz2;
   lz1.Init (sizeof(HVXYZ));
   lz2.Init (sizeof(HVXYZ));
   HVXYZ hv;
   memset (&hv, 0, sizeof(hv));
   DWORD i;
   for (i = 0; i < m_Spline.QueryNodes(); i++) {
      hv.p.Copy(m_Spline.LocationGet(i));
      hv.p.p[3] = 1;
      lz1.Add (&hv);

      // add depth
      hv.p.p[1] = fThickness;
      lz2.Add (&hv);
   }

   // zipper it
   m_Renderrs.ShapeZipper ((PHVXYZ)lz1.Get(0), lz1.Num(), (PHVXYZ)lz2.Get(0), lz2.Num(), TRUE);

   // reverse so get backface cull
   PHVXYZ pz1, pz2;
   DWORD dwNum;
   pz1 = (PHVXYZ) lz1.Get(0);
   pz2 = (PHVXYZ) lz2.Get(0);
   dwNum = lz1.Num();
   for (i = 0; i < dwNum / 2; i++) {
      hv = pz1[i];
      pz1[i] = pz1[dwNum - i - 1];
      pz1[dwNum-i-1] = hv;

      hv = pz2[i];
      pz2[i] = pz2[dwNum - i - 1];
      pz2[dwNum-i-1] = hv;
   }

   // zipper it
   m_Renderrs.ShapeZipper ((PHVXYZ)lz1.Get(0), lz1.Num(), (PHVXYZ)lz2.Get(0), lz2.Num(), TRUE);

   m_Renderrs.Commit();
}

// NOTE: Not bothering to do QueryBoundingBox()

/**********************************************************************************
CObjectUniHole::Clone - Clones this

inputs
   none
returns
   PCObjectSocket - Clone
*/
PCObjectSocket CObjectUniHole::Clone (void)
{
   PCObjectUniHole pNew;

   pNew = new CObjectUniHole(NULL, &m_OSINFO);

   // clone template info
   CloneTemplate(pNew);

   // Clone spline
   m_Spline.CloneTo (&pNew->m_Spline);

   return pNew;
}




PCMMLNode2 CObjectUniHole::MMLTo (void)
{
   // call into the template first
   PCMMLNode2 pNode = MMLToTemplate();
   if (!pNode)
      return NULL;

   // MML spline
   PCMMLNode2 pSub;
   pSub = m_Spline.MMLTo ();
   if (pSub) {
      pSub->NameSet (L"Edge");
      pNode->ContentAdd (pSub);
   }

   return pNode;
}

BOOL CObjectUniHole::MMLFrom (PCMMLNode2 pNode)
{
   // call into the template first
   if (!MMLFromTemplate (pNode))
      return FALSE;

   // member variables go here
   PCMMLNode2 pSub;
   PWSTR psz;
   DWORD i;
   for (i = 0; i < pNode->ContentNum(); i++) {
      pSub = NULL;
      pNode->ContentEnum (i, &psz, &pSub);
      if (!pSub)
         continue;
      psz = pSub->NameGet();
      if (!psz)
         continue;
      if (!_wcsicmp(psz, L"Edge"))
         m_Spline.MMLFrom (pSub);
   }

   return TRUE;
}

/*************************************************************************************
CObjectUniHole::ControlPointQuery - Called to query the information about a control
point identified by dwID.

inputs
   DWORD       dwID - ID
   POSCONTROL  pInfo - Filled in with the infomration
returns
   BOOL - TRUE if successful
*/
BOOL CObjectUniHole::ControlPointQuery (DWORD dwID, POSCONTROL pInfo)
{
   // find the minimum and maximum of the spline
   CPoint pMin, pMax, pt;
   DWORD i,j;
   for (i = 0; i < m_Spline.OrigNumPointsGet(); i++) {
      m_Spline.OrigPointGet(i, &pt);
      pt.p[3] = 1;

      if (!i) {
         pMin.Copy (&pt);
         pMax.Copy (&pt);
         continue;
      }

      for (j = 0; j < 3; j++) {
         pMin.p[j] = min(pMin.p[j], pt.p[j]);
         pMax.p[j] = max(pMax.p[j], pt.p[j]);
      }
   }

   // and center
   CPoint pCenter;
   pCenter.Add (&pMin, &pMax);
   pCenter.Scale (.5);

   fp fThickness;
   fThickness = m_fContainerDepth ? m_fContainerDepth : .3;

   // pinfo
   memset (pInfo,0, sizeof(*pInfo));

   pInfo->dwID = dwID;
   // BUGFIX - Since freedom != 0 causes problems, use freedom=0: pInfo->dwFreedom = 1;   // plane
   //pInfo->pV1.Zero();
   //pInfo->pV1.p[0] = 1;
   //pInfo->pV2.Zero();
   //pInfo->pV2.p[2] = 1;
   pInfo->dwStyle = CPSTYLE_CUBE;
   pInfo->fSize = max(pMax.p[0] - pMin.p[0], pMax.p[2] - pMin.p[2]) / 10;
   pInfo->cColor = RGB(0xff,0xff,0x00);
   wcscpy (pInfo->szName, L"Edge");

   if (m_dwType < 2) {  // rectangle or sphere
      if (dwID >= 8)
         return FALSE;

      switch (dwID % 4) {
      case 0:  // R
         pInfo->pLocation.Copy (&pMax);
         pInfo->pLocation.p[2] = pCenter.p[2];
         break;
      case 1:  // L
         pInfo->pLocation.Copy (&pMin);
         pInfo->pLocation.p[2] = pCenter.p[2];
         break;
      case 2:  // U
         pInfo->pLocation.Copy (&pMax);
         pInfo->pLocation.p[0] = pCenter.p[0];
         break;
      case 3:  // D
         pInfo->pLocation.Copy (&pMin);
         pInfo->pLocation.p[0] = pCenter.p[0];
         break;
      }

      if (dwID >= 4)
         pInfo->pLocation.p[1] += fThickness;
   }
   else {   // any shape
      if (dwID >= m_Spline.OrigNumPointsGet()*2)
         return FALSE;

      m_Spline.OrigPointGet(dwID % m_Spline.OrigNumPointsGet(), &pInfo->pLocation);
      pInfo->pLocation.p[3] = 1;

      if (dwID >= m_Spline.OrigNumPointsGet())
         pInfo->pLocation.p[1] += fThickness;

   }
  
   return TRUE;
}

/*************************************************************************************
CObjectUniHole::ControlPointSet - Called to change the valud of a control point.

inputs
   DWORD       dwID - ID
   PCPoint     pVal - Contains the new location, in object coordinates
returns
   BOOL - TRUE if successful
*/
BOOL CObjectUniHole::ControlPointSet (DWORD dwID, PCPoint pVal, PCPoint pViewer)
{
   // find the minimum and maximum of the spline
   CPoint pMin, pMax, pt;
   DWORD i,j;
   for (i = 0; i < m_Spline.OrigNumPointsGet(); i++) {
      m_Spline.OrigPointGet(i, &pt);
      pt.p[3] = 1;

      if (!i) {
         pMin.Copy (&pt);
         pMax.Copy (&pt);
         continue;
      }

      for (j = 0; j < 3; j++) {
         pMin.p[j] = min(pMin.p[j], pt.p[j]);
         pMax.p[j] = max(pMax.p[j], pt.p[j]);
      }
   }

   // and center
   CPoint pCenter;
   pCenter.Add (&pMin, &pMax);
   pCenter.Scale (.5);

   fp fThickness;
   fThickness = m_fContainerDepth ? m_fContainerDepth : .3;


   CListFixed list;
   list.Init (sizeof(CPoint));
   if (m_dwType < 2) {  // rectangle or sphere
      if (dwID >= 8)
         return FALSE;

      switch (dwID % 4) {
      case 0:  // R
         pMax.p[0] = max(pMin.p[0], pVal->p[0]);
         break;
      case 1:  // L
         pMin.p[0] = min(pMax.p[0], pVal->p[0]);
         break;
      case 2:  // U
         pMax.p[2] = max(pMin.p[2], pVal->p[2]);
         break;
      case 3:  // D
         pMin.p[2] = min(pMax.p[2], pVal->p[2]);
         break;
      }

      // tell the world we're about to change
      if (m_pWorld)
         m_pWorld->ObjectAboutToChange (this);

      CPoint pt;
      pt.Zero();
      DWORD dwSegCurve, dwDivide;
      dwSegCurve = SEGCURVE_LINEAR;
      dwDivide = 0;
      switch (m_dwType) {
      case 0:  // rectangular
         list.Required (4);

         // UL
         pt.p[0] = pMin.p[0];
         pt.p[2] = pMax.p[2];
         list.Add (&pt);

         // UR
         pt.p[0] = pMax.p[0];
         list.Add (&pt);

         // LR
         pt.p[2] = pMin.p[2];
         list.Add (&pt);

         // LL
         pt.p[0] = pMin.p[0];
         list.Add (&pt);
         break;
      case 1:  // elliptical
         list.Required (8);

         pt.p[0] = pMin.p[0];
         pt.p[2] = pCenter.p[2];
         list.Add (&pt);

         pt.p[2] = pMax.p[2];
         list.Add (&pt);

         pt.p[0] = pCenter.p[0];
         list.Add (&pt);

         pt.p[0] = pMax.p[0];
         list.Add (&pt);

         pt.p[2] = pCenter.p[2];
         list.Add (&pt);

         pt.p[2] = pMin.p[2];
         list.Add (&pt);

         pt.p[0] = pCenter.p[0];
         list.Add (&pt);

         pt.p[0] = pMin.p[0];
         list.Add (&pt);

         dwSegCurve = SEGCURVE_ELLIPSENEXT;
         dwDivide = 1;
         break;
      }
      m_Spline.Init (TRUE, list.Num(), (PCPoint)list.Get(0), NULL, (DWORD*)(size_t) dwSegCurve,
         dwDivide, dwDivide, .1);
      // tell the world we've changed
      if (m_pWorld)
         m_pWorld->ObjectChanged (this);
   }
   else {   // any shape
      if (dwID >= m_Spline.OrigNumPointsGet()*2)
         return FALSE;

      // tell the world we're about to change
      if (m_pWorld)
         m_pWorld->ObjectAboutToChange (this);

      // set the new point
      CMem  memPoints;
      CMem  memSegCurve;
      DWORD dwOrig;
      dwOrig = m_Spline.OrigNumPointsGet();
      if (!memPoints.Required (dwOrig * sizeof(CPoint)))
         return FALSE;
      if (!memSegCurve.Required (dwOrig * sizeof(DWORD)))
         return FALSE;
      PCPoint paPoints;
      DWORD *padw;
      paPoints = (PCPoint) memPoints.p;
      padw = (DWORD*) memSegCurve.p;
      DWORD i;
      for (i = 0; i < dwOrig; i++)
         m_Spline.OrigPointGet (i, paPoints+i);
      paPoints[dwID % dwOrig].p[0] = pVal->p[0];
      paPoints[dwID % dwOrig].p[2] = pVal->p[2];
      for (i = 0; i < dwOrig; i++)
         m_Spline.OrigSegCurveGet (i, padw + i);
      DWORD dwMinDivide, dwMaxDivide;
      fp fDetail;
      m_Spline.DivideGet (&dwMinDivide, &dwMaxDivide, &fDetail);
      m_Spline.Init (TRUE, dwOrig, paPoints, NULL, padw, dwMinDivide, dwMaxDivide, fDetail);

      // tell the world we've changed
      if (m_pWorld)
         m_pWorld->ObjectChanged (this);


   }

   EmbedDoCutout();
  
   return TRUE;
}

/*************************************************************************************
CObjectUniHole::ControlPointEnum - Called to enumerate a list of control point IDs
into the list.

inputs
   PCListFixed       plDWORD - Should be filled in with control points
returns
   none
*/
void CObjectUniHole::ControlPointEnum (PCListFixed plDWORD)
{
   DWORD dwNum;
   if (m_dwType < 2)
      dwNum = 8;
   else
      dwNum = m_Spline.OrigNumPointsGet()*2;

   DWORD i;
   for (i = 0; i < dwNum; i++)
      plDWORD->Add (&i);
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
BOOL CObjectUniHole::MoveReferencePointQuery (DWORD dwIndex, PCPoint pp)
{
   if (dwIndex >= sizeof(gaWindowMoveP) / sizeof(WINDOWMOVEP))
      return FALSE;

   // find the minimum and maximum of the spline
   CPoint pMin, pMax, pt;
   DWORD i,j;
   for (i = 0; i < m_Spline.OrigNumPointsGet(); i++) {
      m_Spline.OrigPointGet(i, &pt);
      pt.p[3] = 1;

      if (!i) {
         pMin.Copy (&pt);
         pMax.Copy (&pt);
         continue;
      }

      for (j = 0; j < 3; j++) {
         pMin.p[j] = min(pMin.p[j], pt.p[j]);
         pMax.p[j] = max(pMax.p[j], pt.p[j]);
      }
   }

   // and center
   CPoint pCenter;
   pCenter.Add (&pMin, &pMax);
   pCenter.Scale (.5);

   // which one
   pp->Zero();
   switch (gaWindowMoveP[dwIndex].iX) {
   case -1:
      pp->p[0] = pMin.p[0];
      break;
   case 1:
      pp->p[0] = pMax.p[0];
      break;
   case 0:
   default:
      pp->p[0] = pCenter.p[0];
   }
   switch (gaWindowMoveP[dwIndex].iY) {
   case -1:
      pp->p[2] = pMin.p[2];
      break;
   case 1:
      pp->p[2] = pMax.p[2];
      break;
   case 0:
   default:
      pp->p[2] = pCenter.p[2];
   }

   return TRUE;
}

/**************************************************************************************
CObjectUniHole::MoveReferenceStringQuery -
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
BOOL CObjectUniHole::MoveReferenceStringQuery (DWORD dwIndex, PWSTR psz, DWORD dwSize, DWORD *pdwNeeded)
{
   if (dwIndex >= sizeof(gaWindowMoveP) / sizeof(WINDOWMOVEP)) {
      if (pdwNeeded)
         *pdwNeeded = 0;
      return FALSE;
   }

   DWORD dwNeeded;
   dwNeeded = ((DWORD)wcslen (gaWindowMoveP[dwIndex].pszName) + 1) * 2;
   if (pdwNeeded)
      *pdwNeeded = dwNeeded;
   if (dwNeeded <= dwSize) {
      wcscpy (psz, gaWindowMoveP[dwIndex].pszName);
      return TRUE;
   }
   else
      return FALSE;
}

/**********************************************************************************
CObjectUniHole::EmbedDoCutout - Member function specific to the template. Called
when the object has moved within the surface. This enables the super-class for
the embedded object to pass a cutout into the container. (Basically, specify the
hole for the window or door)
*/
BOOL CObjectUniHole::EmbedDoCutout (void)
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

   CListFixed lFront, lBack;
   lFront.Init (sizeof(CPoint));
   lBack.Init (sizeof(CPoint));
   CPoint pt;
   DWORD i;
   lFront.Required (m_Spline.QueryNodes());
   lBack.Required (m_Spline.QueryNodes());
   for (i = 0; i < m_Spline.QueryNodes(); i++) {
      pt.Copy (m_Spline.LocationGet(i));
      pt.p[3] = 1;
      pt.MultiplyLeft (&mTrans);
      lFront.Add (&pt);

      pt.Copy (m_Spline.LocationGet(i));
      pt.p[3] = 1;
      pt.p[1] += m_fContainerDepth + .01;
      pt.MultiplyLeft (&mTrans);
      lBack.Add (&pt);
   }
   pos->ContCutout (&m_gGUID, lFront.Num(), (PCPoint) lFront.Get(0), (PCPoint) lBack.Get(0), TRUE);

   return TRUE;
}


/********************************************************************************
GenerateThreeDFromSpline - Given a spline on a surface, this sets a threeD
control with the spline.

inputs
   PWSTR       pszControl - Control name.
   PCEscPage   pPage - Page
   PCSpline    pSpline - Spline to draw
   DWORD       dwUse - If 0 it's for adding/remove splines, else if 1 it's for cycling curves
returns
   BOOl - TRUE if success

NOTE: The ID's are such:
   LOBYTE = x
   3rd lowest byte = 1 for edge, 2 for point
*/
static BOOL GenerateThreeDFromSpline (PWSTR pszControl, PCEscPage pPage, PCSpline pSpline,
                                      DWORD dwUse)
{
   PCEscControl pControl = pPage->ControlFind (pszControl);
   if (!pControl)
      return FALSE;

   // figure out center
   CPoint pMin, pMax, pt;
   DWORD i,j;
   for (i = 0; i < pSpline->OrigNumPointsGet(); i++) {
      pSpline->OrigPointGet(i, &pt);
      pt.p[3] = 1;

      if (!i) {
         pMin.Copy (&pt);
         pMax.Copy (&pt);
         continue;
      }

      for (j = 0; j < 3; j++) {
         pMin.p[j] = min(pMin.p[j], pt.p[j]);
         pMax.p[j] = max(pMax.p[j], pt.p[j]);
      }
   }

   // and center
   CPoint pCenter;
   pCenter.Add (&pMin, &pMax);
   pCenter.Scale (.5);

   // figure out the maximum distance
   fp fMax;
   fMax = max(pMax.p[0] - pMin.p[0], pMax.p[2] - pMin.p[2]);
   // NOTE: assume p[1] == 0
   fMax = max(0.001, fMax);
   fMax /= 5;  // so is larger

   // when draw points, get the point, subtract the center, and divide by fMax

   // use gmemtemp
   MemZero (&gMemTemp);
   MemCat (&gMemTemp, L"<rotatex val=-90/><backculloff/>");   //  so that Z is up (on the screen)


   // draw the outline
   DWORD dwNum;
   dwNum = pSpline->OrigNumPointsGet();
   DWORD x;
   for (x = 0; x < dwNum; x++) {
      CPoint p1, p2;
      pSpline->OrigPointGet (x, &p1);
      p1.p[3] = 1;
      pSpline->OrigPointGet ((x+1) % dwNum, &p2);
      p2.p[3] = 1;

      // center and scale
      p1.Subtract (&pCenter);
      p2.Subtract (&pCenter);
      p1.Scale (1.0 / fMax);
      p2.Scale (1.0 / fMax);


      // draw a line
      if (dwUse == 0)
         MemCat (&gMemTemp, L"<colordefault color=#c0c0c0/>");
      else {
         DWORD dwSeg;
         pSpline->OrigSegCurveGet (x, &dwSeg);
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

      MemCat (&gMemTemp, L"<shapearrow tip=false width=.2");

      WCHAR szTemp[128];
      swprintf (szTemp, L" p1=%g,%g,%g p2=%g,%g,%g/>",
         (double)p1.p[0], (double)p1.p[1], (double)p1.p[2], (double)p2.p[0], (double)p2.p[1], (double)p2.p[2]);
      MemCat (&gMemTemp, szTemp);

      // do push point if more than 3 points
      if ((dwUse == 0) && (dwNum > 3)) {
         MemCat (&gMemTemp, L"<matrixpush>");
         swprintf (szTemp, L"<translate point=%g,%g,%g/>",
            (double)p1.p[0], (double)p1.p[1], (double)p1.p[2]);
         MemCat (&gMemTemp, szTemp);
         MemCat (&gMemTemp, L"<colordefault color=#ff0000/>");
         // set the ID
         MemCat (&gMemTemp, L"<id val=");
         MemCat (&gMemTemp, (int)((2 << 16) | x));
         MemCat (&gMemTemp, L"/>");

         MemCat (&gMemTemp, L"<MeshSphere radius=.4/><shapemeshsurface/>");

         MemCat (&gMemTemp, L"</matrixpush>");
      }
   }

   // set the threeD control
   ESCMTHREEDCHANGE tc;
   memset (&tc, 0, sizeof(tc));
   tc.pszMML = (PWSTR) gMemTemp.p;
   pControl->Message (ESCM_THREEDCHANGE, &tc);

   return TRUE;
}



/* UniHolePage
*/
BOOL UniHolePage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCObjectUniHole pv = (PCObjectUniHole)pPage->m_pUserData;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         // draw images
         GenerateThreeDFromSpline (L"edgeaddremove", pPage, &pv->m_Spline, 0);
         GenerateThreeDFromSpline (L"edgecurve", pPage, &pv->m_Spline, 1);
      }
      break;

   case ESCN_THREEDCLICK:
      {
         PESCNTHREEDCLICK p = (PESCNTHREEDCLICK) pParam;
         if (!p->pControl->m_pszName)
            break;

         BOOL  fCol;
         if (!_wcsicmp(p->pControl->m_pszName, L"edgeaddremove"))
            fCol = TRUE;
         else if (!_wcsicmp(p->pControl->m_pszName, L"edgecurve"))
            fCol = FALSE;
         else
            break;

         // figure out x, y, and what clicked on
         DWORD x, dwMode;
         dwMode = (BYTE)(p->dwMajor >> 16);
         x = (BYTE) p->dwMajor;
         if ((dwMode < 1) || (dwMode > 2))
            break;

         // allocate enough memory so can do the calculations
         CMem  memPoints;
         CMem  memSegCurve;
         DWORD dwOrig;
         dwOrig = pv->m_Spline.OrigNumPointsGet();
         if (!memPoints.Required ((dwOrig+1) * sizeof(CPoint)))
            return TRUE;
         if (!memSegCurve.Required ((dwOrig+1) * sizeof(DWORD)))
            return TRUE;

         // load it in
         PCPoint paPoints;
         DWORD *padw;
         paPoints = (PCPoint) memPoints.p;
         padw = (DWORD*) memSegCurve.p;
         DWORD i;
         for (i = 0; i < dwOrig; i++) {
            pv->m_Spline.OrigPointGet (i, paPoints+i);
         }
         for (i = 0; i < dwOrig; i++)
            pv->m_Spline.OrigSegCurveGet (i, padw + i);
         DWORD dwMinDivide, dwMaxDivide;
         fp fDetail;
         pv->m_Spline.DivideGet (&dwMinDivide, &dwMaxDivide, &fDetail);

         if (fCol) {
            if (dwMode == 1) {
               // inserting
               memmove (paPoints + (x+1), paPoints + x, sizeof(CPoint) * (dwOrig-x));
               paPoints[x+1].Add (paPoints + ((x+2) % (dwOrig+1)));
               paPoints[x+1].Scale (.5);
               memmove (padw + (x+1), padw + x, sizeof(DWORD) * (dwOrig - x));
               dwOrig++;
            }
            else if (dwMode == 2) {
               // deleting
               memmove (paPoints + x, paPoints + (x+1), sizeof(CPoint) * (dwOrig-x-1));
               memmove (padw + x, padw + (x+1), sizeof(DWORD) * (dwOrig - x - 1));
               dwOrig--;
            }
         }
         else {
            // setting curvature
            if (dwMode == 1) {
               padw[x] = (padw[x] + 1) % (SEGCURVE_MAX+1);
            }
         }

         pv->m_pWorld->ObjectAboutToChange (pv);
         pv->m_Spline.Init (TRUE, dwOrig, paPoints, NULL, padw, dwMinDivide, dwMaxDivide, fDetail);
         pv->m_pWorld->ObjectChanged (pv);
         pv->EmbedDoCutout();

         // redraw the shapes
         GenerateThreeDFromSpline (L"edgeaddremove", pPage, &pv->m_Spline, 0);
         GenerateThreeDFromSpline (L"edgecurve", pPage, &pv->m_Spline, 1);
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Universal Hole";
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}


/**********************************************************************************
CObjectUniHole::DialogShow - Causes the object to show a dialog box that allows
different attributes to be changed.

inputs
   DWORD             dwSuface - Surface that was clicked on. 0 if unknown.
   PCEscWindow       pWindow - Escarpment window to draw it in
returns
   BOOL - TRUE if user pressed "Back", FALSE if user pressed "Close"
*/
BOOL CObjectUniHole::DialogShow (DWORD dwSurface, PCEscWindow pWindow, PCPoint pClick)
{
   if (m_dwType < 2)
      return FALSE;

   PWSTR pszRet;
   pszRet = pWindow->PageDialog (ghInstance, IDR_MMLUNIHOLE, UniHolePage, this);

   return pszRet && !_wcsicmp(pszRet, Back());
}


/**********************************************************************************
CObjectTemplate::DialogQuery - Returns TRUE if the object supports a custom
dialog box, FALSE if it doesn't. The default object doesn't
*/
BOOL CObjectUniHole::DialogQuery (void)
{
   if (m_dwType < 2)
      return FALSE;

   return TRUE;
}

