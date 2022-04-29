/************************************************************************
CObjectBalustrade.cpp - Draws a box.

begun 12/4/02 by Mike Rozak
Copyright 2002 Mike Rozak. All rights reserved
*/

#include <windows.h>
#include "escarpment.h"
#include "..\M3D.h"
#include "resource.h"

static PWSTR gszPoints = L"points%d-%d";


typedef struct {
   PWSTR       pszName;    // name of the move point
   int         iX, iY; // -1 if for left/top, 1 for right/bottom
} SPLINEMOVEP, *PSPLINEMOVEP;



static SPLINEMOVEP   gaSplineMoveP[] = {
   L"Center bottom", 0, 1,
   L"Bottom left end", -1, 1,
   L"Bottom right end", 1, 1,
   L"Top left end", -1, -1,
   L"Top right end", 1, -1
};



/**********************************************************************************
CObjectBalustrade::Constructor and destructor

(DWORD)pParams is passed into CBalustrade::Init(). See definitions of parameters

 */
CObjectBalustrade::CObjectBalustrade (PVOID pParams, POSINFO pInfo)
{
   m_dwType = (DWORD)(size_t) pParams;
   m_OSINFO = *pInfo;

   // If it's a fence then deal with with differnt rendershow
   m_dwRenderShow = m_ds.m_fFence ? RENDERSHOW_LANDSCAPING : RENDERSHOW_BALUSTRADES;

   // default bottom and top spline
   CSpline sBottom, sTop;
   CPoint ap[2];
   ap[0].Zero();
   ap[0].p[0] = -1;
   ap[1].Zero();
   ap[1].p[0] = 1;
   sBottom.Init (FALSE, 2, ap, NULL, (DWORD*) SEGCURVE_LINEAR, 0, 0, .1);
   ap[0].p[2] = 1;
   ap[1].p[2] = 1;
   sTop.Init (FALSE, 2, ap, NULL, (DWORD*) SEGCURVE_LINEAR, 0, 0, .1);

   // balustrade style
   DWORD dwStyle;
   dwStyle = (DWORD) LOWORD(m_dwType);

   // Have a way of specifying if it's a fence or balustrade to CBalustrade
   m_ds.m_fFence = HIWORD(m_dwType) ? TRUE : FALSE;

   // Different fences will have different default heights
   if (m_ds.m_fFence) {
      // divide with small ones - not full height.
      m_ds.m_fPostDivideIntoFull = FALSE;

      switch (dwStyle) {
         case BS_FENCEVERTPANEL:
         case BS_FENCEVERTWROUGHTIRON:
         case BS_FENCEVERTPICKETSMALL:
         case BS_FENCEPANELSOLID:
         case BS_FENCEPANELPOLE:
            m_ds.m_fHeight = 2.0;   // full height
            break;
      }
   }

   // Will need to create differnt balustrade based on pParam
   m_ds.Init (m_OSINFO.dwRenderShard, dwStyle, (PCObjectTemplate) this, &sBottom, &sTop, FALSE);


}


CObjectBalustrade::~CObjectBalustrade (void)
{
   // do nothing for now
}


/**********************************************************************************
CObjectBalustrade::Delete - Called to delete this object
*/
void CObjectBalustrade::Delete (void)
{
   delete this;
}

/**********************************************************************************
CObjectBalustrade::Render - Draws a test box.

inputs
   POBJECTRENDER     pr - Render information
*/
void CObjectBalustrade::Render (POBJECTRENDER pr, DWORD dwSubObject)
{
   // create the surface render object and draw
   // CRenderSurface rs;
   m_Renderrs.ClearAll();

   CMatrix mObject;
   m_Renderrs.Init (pr->pRS);
   ObjectMatrixGet (&mObject);
   m_Renderrs.Multiply (&mObject);

   m_ds.Render (m_OSINFO.dwRenderShard, pr, &m_Renderrs);

   m_Renderrs.Commit();
}




/**********************************************************************************
CObjectBalustrade::QueryBoundingBox - Standard API
*/
void CObjectBalustrade::QueryBoundingBox (PCPoint pCorner1, PCPoint pCorner2, DWORD dwSubObject)
{
   m_ds.QueryBoundingBox (pCorner1, pCorner2);

#ifdef _DEBUG
   // test, make sure bounding box not too small
   CPoint p1,p2;
   DWORD i;
   CObjectTemplate::QueryBoundingBox (&p1, &p2, dwSubObject);
   for (i = 0; i < 3; i++)
      if ((p1.p[i] < pCorner1->p[i] - CLOSE) || (p2.p[i] > pCorner2->p[i] + CLOSE))
         break;
   if (i < 3)
      OutputDebugString ("\r\nCObjectBalustrade::QueryBoundingBox too small.");
#endif
}


/**********************************************************************************
CObjectBalustrade::Clone - Clones this

inputs
   none
returns
   PCObjectSocket - Clone
*/
PCObjectSocket CObjectBalustrade::Clone (void)
{
   PCObjectBalustrade pNew;

   pNew = new CObjectBalustrade(NULL, &m_OSINFO);

   // clone template info
   CloneTemplate(pNew);
   m_ds.CloneTo (&pNew->m_ds, pNew);


   return pNew;
}



PCMMLNode2 CObjectBalustrade::MMLTo (void)
{
   // call into the template first
   PCMMLNode2 pNode = MMLToTemplate();
   if (!pNode)
      return NULL;

   // member variables go here
   PCMMLNode2 pSub;
   pSub = m_ds.MMLTo();
   if (pSub) {
      pSub->NameSet (L"Balustrade");
      pNode->ContentAdd (pSub);
   }


   return pNode;
}

BOOL CObjectBalustrade::MMLFrom (PCMMLNode2 pNode)
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
      if (!_wcsicmp(psz, L"Balustrade"))
         m_ds.MMLFrom (pSub);
   }

   return TRUE;
}

/*************************************************************************************
CObjectBalustrade::ControlPointQuery - Called to query the information about a control
point identified by dwID.

inputs
   DWORD       dwID - ID
   POSCONTROL  pInfo - Filled in with the infomration
returns
   BOOL - TRUE if successful
*/
BOOL CObjectBalustrade::ControlPointQuery (DWORD dwID, POSCONTROL pInfo)
{
   return m_ds.ControlPointQuery (dwID, pInfo);
}

/*************************************************************************************
CObjectBalustrade::ControlPointSet - Called to change the valud of a control point.

inputs
   DWORD       dwID - ID
   PCPoint     pVal - Contains the new location, in object coordinates
returns
   BOOL - TRUE if successful
*/
BOOL CObjectBalustrade::ControlPointSet (DWORD dwID, PCPoint pVal, PCPoint pViewer)
{
   return m_ds.ControlPointSet (dwID, pVal, pViewer, NULL);
}

/*************************************************************************************
CObjectBalustrade::ControlPointEnum - Called to enumerate a list of control point IDs
into the list.

inputs
   PCListFixed       plDWORD - Should be filled in with control points
returns
   none
*/
void CObjectBalustrade::ControlPointEnum (PCListFixed plDWORD)
{
   m_ds.ControlPointEnum (plDWORD);
}

/**********************************************************************************
CObjectBalustrade::DialogQuery - Returns TRUE if the object supports a custom
dialog box, FALSE if it doesn't. The default object doesn't
*/
BOOL CObjectBalustrade::DialogQuery (void)
{
   return TRUE;
}


/**********************************************************************************
CObjectBalustrade::DialogCPQuery - Returns TRUE if the object supports a custom
dialog box, FALSE if it doesn't. The default object doesn't
*/
BOOL CObjectBalustrade::DialogCPQuery (void)
{
   return TRUE;
}


/* BalDialogPage
*/
BOOL BalDialogPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCObjectBalustrade pv = (PCObjectBalustrade) pPage->m_pUserData;

   switch (dwMessage) {
   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Balustrade/fence settings";
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}

/**********************************************************************************
CObjectBalustrade::DialogShow - Causes the object to show a dialog box that allows
different attributes to be changed.

inputs
   DWORD             dwSuface - Surface that was clicked on. 0 if unknown.
   PCEscWindow       pWindow - Escarpment window to draw it in
returns
   BOOL - TRUE if user pressed "Back", FALSE if user pressed "Close"
*/
BOOL CObjectBalustrade::DialogShow (DWORD dwSurface, PCEscWindow pWindow, PCPoint pClick)
{
   PWSTR pszRet;
firstpage:
   pszRet = pWindow->PageDialog (ghInstance, IDR_MMLBALDIALOG, BalDialogPage, this);
firstpage2:
   if (!pszRet)
      return FALSE;
   if (!_wcsicmp(pszRet, L"balappear")) {
      pszRet = m_ds.AppearancePage (pWindow, NULL);

      if (pszRet && !_wcsicmp(pszRet, Back()))
         goto firstpage;
      else
         goto firstpage2;
   }
   else if (!_wcsicmp(pszRet, L"balopenings")) {
      pszRet = m_ds.OpeningsPage (pWindow, NULL);

      if (pszRet && !_wcsicmp(pszRet, Back()))
         goto firstpage;
      else
         goto firstpage2;
   }
//   else if (!_wcsicmp(pszRet, L"displaycontrol")) {
//      pszRet = m_ds.DisplayPage (pWindow);
//
//      if (pszRet && !_wcsicmp(pszRet, Back()))
//         goto firstpage;
//      else
//         goto firstpage2;
//   }
   else if (!_wcsicmp(pszRet, L"corners")) {
      pszRet = m_ds.CornersPage (pWindow);

      if (pszRet && !_wcsicmp(pszRet, Back()))
         goto firstpage;
      else
         goto firstpage2;
   }

   return !_wcsicmp(pszRet, Back());
}



/**********************************************************************************
CObjectBalustrade::DialogCPShow - Causes the object to show a dialog box that allows
different attributes to be changed.

inputs
   DWORD             dwSuface - Surface that was clicked on. 0 if unknown.
   PCEscWindow       pWindow - Escarpment window to draw it in
returns
   BOOL - TRUE if user pressed "Back", FALSE if user pressed "Close"
*/
BOOL CObjectBalustrade::DialogCPShow (DWORD dwSurface, PCEscWindow pWindow, PCPoint pClick)
{
   PWSTR pszRet;
   pszRet = m_ds.DisplayPage (pWindow);
   if (!pszRet)
      return FALSE;

   return !_wcsicmp(pszRet, Back());
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
BOOL CObjectBalustrade::MoveReferencePointQuery (DWORD dwIndex, PCPoint pp)
{
   PSPLINEMOVEP ps;
   DWORD dwDataSize;
   ps = gaSplineMoveP;
   dwDataSize = sizeof(gaSplineMoveP);

   if (dwIndex >= dwDataSize / sizeof(SPLINEMOVEP))
      return FALSE;

   DWORD dwWidth;
   DWORD dwX, dwY;
   CPoint p2;
   dwWidth = m_ds.m_psOrigBottom->OrigNumPointsGet();
   dwX = (ps[dwIndex].iX < 0) ? 0 : (dwWidth-1);
   dwY = (ps[dwIndex].iY < 0) ? 0 : 1;
   
   // special case for center bottom
   if ((ps[dwIndex].iX == 0) && (ps[dwIndex].iY != 0)) {
      dwX = 0;
      m_ds.m_psOrigBottom->OrigPointGet (dwX, &p2);
      dwX = dwWidth-1;
      m_ds.m_psOrigBottom->OrigPointGet (dwX, pp);
      pp->Add (&p2);
      pp->Scale (.5);
      pp->p[3] = 1.0;
      return TRUE;
   }

   if (dwY)
      return m_ds.m_psOrigBottom->OrigPointGet (dwX, pp);
   else {
      CPoint p2;
      m_ds.m_psOrigBottom->OrigPointGet (dwX, &p2);
      m_ds.m_psOrigTop->OrigPointGet (dwX, pp);
      pp->Subtract (&p2);
      pp->Normalize();
      pp->Scale (m_ds.m_fHeight);
      pp->Add (&p2);
      return TRUE;
   }
}

/**************************************************************************************
CObjectBalustrade::MoveReferenceStringQuery -
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
BOOL CObjectBalustrade::MoveReferenceStringQuery (DWORD dwIndex, PWSTR psz, DWORD dwSize, DWORD *pdwNeeded)
{
   PSPLINEMOVEP ps;
   DWORD dwDataSize;
   ps = gaSplineMoveP;
   dwDataSize = sizeof(gaSplineMoveP);
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

/**********************************************************************************
CObjectBalustrade::Message -
sends a message to the object. The interpretation of the message depends upon
dwMessage, which is OSM_XXX. If the function understands and handles the
message it returns TRUE, otherwise FALE.
*/
BOOL CObjectBalustrade::Message (DWORD dwMessage, PVOID pParam)
{
   return FALSE;
}

/*************************************************************************************
CObjectBalustrade::IntelligentAdjust
Tells the object to intelligently adjust itself based on nearby objects.
For walls, this means triming to the roof line, for floors, different
textures, etc. If fAct is FALSE the function is just a query, that returns
TRUE if the object cares about adjustment and can try, FALSE if it can't.

NOTE: Often overridden
*/
BOOL CObjectBalustrade::IntelligentAdjust (BOOL fAct)
{
   // dont extend fence to roof
   if (m_ds.m_fFence)
      return FALSE;

   if (!fAct)
      return TRUE;
   if (!m_pWorld)
      return FALSE;

   CMatrix mInv;
   DWORD i;
   CListFixed lMatrix, lPSS;
   lMatrix.Init (sizeof(CMatrix));
   lPSS.Init (sizeof(PCSplineSurface));

   // find what parts of the world this intersects with
   // get the bounding box for this. I know that because of tests in render
   // it will return even already clipped stuff
   DWORD dwThis;
   dwThis = m_pWorld->ObjectFind (&m_gGUID);
   CPoint pCorner1, pCorner2;
   CMatrix m;
   m_pWorld->BoundingBoxGet (dwThis, &m, &pCorner1, &pCorner2);
   m.Invert4 (&mInv);

   // extend the bounding box up
   if (pCorner1.p[2] > pCorner2.p[2])
      pCorner1.p[2] += 4.0;
   else
      pCorner2.p[2] += 4.0;

   // find out what objects this intersects with
   CListFixed     lObjects;
   lObjects.Init(sizeof(DWORD));
   m_pWorld->IntersectBoundingBox (&m, &pCorner1, &pCorner2, &lObjects);


   // go through those objects and get the surfaces
   PCObjectSocket pos;
   for (i = 0; i < lObjects.Num(); i++) {
      DWORD dwOn = *((DWORD*)lObjects.Get(i));
      if (dwOn == dwThis)
         continue;
      pos = m_pWorld->ObjectGet (dwOn);
      if (!pos)
         continue;

      OSMSPLINESURFACEGET q;
      memset (&q, 0, sizeof(q));
      q.dwType = 0;
      q.pListMatrix = &lMatrix;
      q.pListPSS = &lPSS;
      pos->Message (OSM_SPLINESURFACEGET, &q);
      // if it liked the message will have added on surface
   }


   // loop through all the matricies and multiply them by the inverse of this object's
   // matrix, so all matrices convert to object space
   m_MatrixObject.Invert4 (&mInv);
   PCMatrix pm;
   pm = (PCMatrix) lMatrix.Get(0);
   for (i = 0; i < lMatrix.Num(); i++)
      pm[i].MultiplyRight (&mInv);

   if (lMatrix.Num()) {
      if (m_pWorld)
         m_pWorld->ObjectAboutToChange (this);
      m_ds.ExtendPostsToRoof (lMatrix.Num(), (PCSplineSurface*) lPSS.Get(0), pm);

      if (m_pWorld)
         m_pWorld->ObjectChanged (this);
   }

   return TRUE;
}

/*************************************************************************************
CObjectBalustrade::IntelligentPositionDrag - The template object is unable to chose an intelligent
position so it just retunrs FALSE.

inputs
   POSINTELLIPOS     pInfo - Information that might be useful to chose the position
returns
   BOOL - TRUE if the object has moved, rotated, scaled, itself to an intelligent
      location. FALSE if doesn't know and its up to the application.
*/
typedef struct {
   fp      fZ;      // height
   DWORD       dwCount; // number
} IPD, *PIPD;
BOOL CObjectBalustrade::IntelligentPositionDrag (CWorldSocket *pWorld, POSINTELLIPOSDRAG pInfo,  POSINTELLIPOSRESULT pResult)
{

   BOOL fFence = m_ds.m_fFence;

   // walls can be dragged
   if (!pWorld && !pInfo && !pResult)
      return TRUE;

   // some parameter checks
   if (!pWorld || !pInfo || !pResult || !pInfo->paWorldCoord || !pInfo->dwNumWorldCoord)
      return FALSE;

   if (m_ds.m_psOrigBottom->OrigNumPointsGet() != 2)
      return FALSE;

   // create a list of all the points that dragged over to determine height - which
   // is all care about with walls
   CListFixed lHeight;
   lHeight.Init (sizeof(IPD));
   DWORD i, j;
   PIPD pi;
   for (i = 0; i < pInfo->dwNumWorldCoord; i++) {
      fp fZ = pInfo->paWorldCoord[i].p[2];

      // loop through the current list and see if can find same height
      pi = (PIPD)lHeight.Get(0);
      for (j = 0; j < lHeight.Num(); j++) {
         if (fabs(fZ - pi[j].fZ) < .01)
            break;
      }
      if (j < lHeight.Num()) {
         // average and increment the counter
         pi[j].fZ = ((pi[j].fZ * pi[j].dwCount) + fZ) / (fp)(pi[j].dwCount+1);
         pi[j].dwCount++;
      }
      else {
         // add it
         IPD id;
         id.dwCount = 1;
         id.fZ = fZ;
         lHeight.Add (&id);
      }
   }

   // look at what we have
   if (!lHeight.Num())
      return FALSE;
   pi = (PIPD) lHeight.Get(0);
   DWORD dwMax;
   dwMax = 0;
   for (i = 1; i < lHeight.Num(); i++)
      if (pi[i].dwCount > pi[dwMax].dwCount)
         dwMax = i;

   // have a height
   fp fHeight;
   fHeight = pi[dwMax].fZ;

   // now, look at intersect a point with the plane, running from
   // the eye to the plane
   CPoint apBoundary[2];
   CPoint pPlane, pPlaneN;
   pPlane.Zero();
   pPlane.p[2] = fHeight;
   pPlaneN.Zero();
   pPlaneN.p[2] = 1;
   for (i = 0; i < 2; i++) {
      CPoint pTo;
      pTo.Copy (&pInfo->paWorldCoord[i ? (pInfo->dwNumWorldCoord-1) : 0]);

      // if its a fence, just take the start/stop location of where clicked
      if (fFence) {
         apBoundary[i].Copy (&pTo);
         continue;
      }

      // look from
      CPoint pFrom;
      if (pInfo->fViewFlat) {
         pFrom.Subtract (&pInfo->pCamera, &pInfo->pLookAt);
         pFrom.Normalize();
         pFrom.Add (&pTo);
      }
      else {
         pFrom.Copy (&pInfo->pCamera);
      }

      // intersect
      if (!IntersectLinePlane (&pFrom, &pTo, &pPlane, &pPlaneN, &apBoundary[i]))
         return FALSE;  // for some reason not intersecting with plane we chose
   }

   // create matrix that rotates and centers the points
   CMatrix mRot;
   CMatrix mInv;
   CPoint pX, pY, pZ;
   pZ.Zero();
   pZ.p[2] = 1;
   pX.Subtract (&apBoundary[1], &apBoundary[0]);
   pX.p[2] = 0;
   pX.Normalize();
   pY.CrossProd (&pZ, &pX);
   pY.Normalize();
   pZ.CrossProd (&pX, &pY);
   pZ.Normalize();
   if (pZ.p[2] < 0.0) {
      pX.Scale(-1);
      pY.Scale(-1);
      pZ.Scale(-1);
   }
   mRot.RotationFromVectors (&pX, &pY, &pZ);

   // half way in between should go to 0
   CPoint pHalf;
   CMatrix mTrans;
   pHalf.Average (&apBoundary[0], &apBoundary[1]);
   mTrans.Translation (pHalf.p[0], pHalf.p[1], pHalf.p[2]);
   mRot.MultiplyRight (&mTrans);

   // invert
   mRot.Invert4 (&mInv);

   // modify the two boundaries - should be (-flen,0,0) to (flen,0,0);
   for (i = 0; i < 2; i++) {
      apBoundary[i].p[3] = 1;
      apBoundary[i].MultiplyLeft (&mInv);
   }

   // create the splines
   CSpline sBottom, sTop;
   if (fFence) {
      // find the topography of the land by dividing into lengths
      fp fLen;
      pX.Subtract (&apBoundary[1], &apBoundary[0]);
      fLen = pX.Length();
      DWORD dwDivide;
      dwDivide = max ((DWORD) (fLen / 4.0)+1, 2);
      // BUGFIX - No more than 5 segments
      dwDivide = min(5, dwDivide);

      // find topography height
      CListFixed lh;
      lh.Init (sizeof(CPoint));
      lh.Required (dwDivide);
      for (i = 0; i < dwDivide; i++) {
         CPoint pStart, pEnd;
         pStart.Average (&apBoundary[1], &apBoundary[0], (fp)i / (fp)(dwDivide-1));
         pStart.p[3] = 1;
         pEnd.Copy (&pStart);
         pEnd.p[2] += 1.0;

         // convert to world space
         pStart.MultiplyLeft (&mRot);
         pEnd.MultiplyLeft (&mRot);

         DWORD j;
         OSMINTERSECTLINE il;
         // find out where intersects with ground
         memset (&il, 0, sizeof(il));
         il.pStart.Copy (&pStart);
         il.pEnd.Copy (&pEnd);
         for (j = 0; j < pWorld->ObjectNum(); j++) {
            PCObjectSocket pos = pWorld->ObjectGet(j);

            pos->Message (OSM_INTERSECTLINE, &il);

            if (il.fIntersect)
               break;
         }
         if (j >= pWorld->ObjectNum())
            lh.Add (&pStart);
         else
            lh.Add (&il.pIntersect);
         
      
      }

      // the above path needs to be converted into object space
      PCPoint pap;
      pap = (PCPoint) lh.Get(0);
      for (i = 0; i < dwDivide; i++) {
         pap[i].p[3] = 1;
         pap[i].MultiplyLeft (&mInv);
      }

      // init
      sBottom.Init (FALSE, dwDivide, pap, NULL, (DWORD*) SEGCURVE_LINEAR, 0, 0, .1);
      for (i = 0; i < dwDivide; i++)
         pap[i].p[2] += 1.0;  // up
      sTop.Init (FALSE, dwDivide, pap, NULL, (DWORD*) SEGCURVE_LINEAR, 0, 0, .1);
   }
   else {
      sBottom.Init (FALSE, 2, apBoundary, NULL, (DWORD*) SEGCURVE_LINEAR, 0, 0, .1);
      apBoundary[0].p[2] += 1.0;
      apBoundary[1].p[2] += 1.0;
      sTop.Init (FALSE, 2, apBoundary, NULL, (DWORD*) SEGCURVE_LINEAR, 0, 0, .1);
   }

   m_ds.NewSplines (&sBottom, &sTop);

   if (fFence)
      m_ds.m_fForceLevel = FALSE;   // since went to ground line

   // use the calculated matrix
   memset (pResult, 0, sizeof(*pResult));
   pResult->mObject.Copy (&mRot);

   return TRUE;
}

// FUTURERELEASE - Have it so balustrades automagically connect to one another if
// one's end is next to another's start, and so that if move connected balustrade
// the other one moves

// FUTURERELEASE - Tried to move control points for balustrade openings from above but couldn't
// it's because of how the intersections are done. Not worth fixing because the solution
// (closest point) is more work than worth.
