/************************************************************************
CObjectPiers.cpp - Draws a box.

begun 16/4/02 by Mike Rozak
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
   L"Center top", 0, -1,
   L"Bottom left end", -1, 1,
   L"Bottom right end", 1, 1,
   L"Top left end", -1, -1,
   L"Top right end", 1, -1
};


/**********************************************************************************
CObjectPiers::Constructor and destructor

(DWORD)pParams is passed into CPiers::Init(). See definitions of parameters

 */
CObjectPiers::CObjectPiers (PVOID pParams, POSINFO pInfo)
{
   m_dwType = (DWORD)(size_t) pParams;
   m_OSINFO = *pInfo;

   // If it's a fence then deal with with differnt rendershow
   m_dwRenderShow = RENDERSHOW_PIERS;

   // default bottom and top spline
   CSpline sBottom, sTop;
   CPoint ap[4];
   fp fDist = 2;
   ap[0].Zero();
   ap[0].p[0] = -fDist;
   ap[0].p[1] = fDist;
   ap[1].Copy (&ap[0]);
   ap[1].p[0] = fDist;
   ap[2].Copy (&ap[1]);
   ap[2].p[1] = -fDist;
   ap[3].Copy (&ap[2]);
   ap[3].p[0] = -fDist;

   sBottom.Init (TRUE, 4, ap, NULL, (DWORD*) SEGCURVE_LINEAR, 0, 0, .1);
   ap[0].p[2] = 1;
   ap[1].p[2] = 1;
   ap[2].p[2] = 1;
   ap[3].p[2] = 1;
   sTop.Init (TRUE, 4, ap, NULL, (DWORD*) SEGCURVE_LINEAR, 0, 0, .1);

   // Piers style
   DWORD dwStyle;
   dwStyle = (DWORD) LOWORD(m_dwType);

   // Will need to create differnt Piers based on pParam
   m_ds.Init (m_OSINFO.dwRenderShard, dwStyle, (PCObjectTemplate) this, &sBottom, &sTop, TRUE);
   // BUGFIX - Changed to indent for self-created so that lattice is drawn around


}


CObjectPiers::~CObjectPiers (void)
{
   // do nothing for now
}


/**********************************************************************************
CObjectPiers::Delete - Called to delete this object
*/
void CObjectPiers::Delete (void)
{
   delete this;
}

/**********************************************************************************
CObjectPiers::Render - Draws a test box.

inputs
   POBJECTRENDER     pr - Render information
*/
void CObjectPiers::Render (POBJECTRENDER pr, DWORD dwSubObject)
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
CObjectPiers::QueryBoundingBox - Standard API
*/
void CObjectPiers::QueryBoundingBox (PCPoint pCorner1, PCPoint pCorner2, DWORD dwSubObject)
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
      OutputDebugString ("\r\nCObjectPiers::QueryBoundingBox too small.");
#endif
}


/**********************************************************************************
CObjectPiers::Clone - Clones this

inputs
   none
returns
   PCObjectSocket - Clone
*/
PCObjectSocket CObjectPiers::Clone (void)
{
   PCObjectPiers pNew;

   pNew = new CObjectPiers(NULL, &m_OSINFO);

   // clone template info
   CloneTemplate(pNew);
   m_ds.CloneTo (&pNew->m_ds, pNew);


   return pNew;
}



PCMMLNode2 CObjectPiers::MMLTo (void)
{
   // call into the template first
   PCMMLNode2 pNode = MMLToTemplate();
   if (!pNode)
      return NULL;

   // member variables go here
   PCMMLNode2 pSub;
   pSub = m_ds.MMLTo();
   if (pSub) {
      pSub->NameSet (L"Piers");
      pNode->ContentAdd (pSub);
   }


   return pNode;
}

BOOL CObjectPiers::MMLFrom (PCMMLNode2 pNode)
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
      if (!_wcsicmp(psz, L"Piers"))
         m_ds.MMLFrom (pSub);
   }

   return TRUE;
}


/*************************************************************************************
CObjectPiers::ControlPointQuery - Called to query the information about a control
point identified by dwID.

inputs
   DWORD       dwID - ID
   POSCONTROL  pInfo - Filled in with the infomration
returns
   BOOL - TRUE if successful
*/
BOOL CObjectPiers::ControlPointQuery (DWORD dwID, POSCONTROL pInfo)
{
   return m_ds.ControlPointQuery (dwID, pInfo);
}

/*************************************************************************************
CObjectPiers::ControlPointSet - Called to change the valud of a control point.

inputs
   DWORD       dwID - ID
   PCPoint     pVal - Contains the new location, in object coordinates
returns
   BOOL - TRUE if successful
*/
BOOL CObjectPiers::ControlPointSet (DWORD dwID, PCPoint pVal, PCPoint pViewer)
{
   return m_ds.ControlPointSet (m_OSINFO.dwRenderShard, dwID, pVal, pViewer, NULL);
}

/*************************************************************************************
CObjectPiers::ControlPointEnum - Called to enumerate a list of control point IDs
into the list.

inputs
   PCListFixed       plDWORD - Should be filled in with control points
returns
   none
*/
void CObjectPiers::ControlPointEnum (PCListFixed plDWORD)
{
   m_ds.ControlPointEnum (plDWORD);
}

/**********************************************************************************
CObjectPiers::DialogQuery - Returns TRUE if the object supports a custom
dialog box, FALSE if it doesn't. The default object doesn't
*/
BOOL CObjectPiers::DialogQuery (void)
{
   return TRUE;
}


/**********************************************************************************
CObjectPiers::DialogCPQuery - Returns TRUE if the object supports a custom
dialog box, FALSE if it doesn't. The default object doesn't
*/
BOOL CObjectPiers::DialogCPQuery (void)
{
   return TRUE;
}


/* PiersDialogPage
*/
BOOL PiersDialogPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCObjectPiers pv = (PCObjectPiers) pPage->m_pUserData;

   switch (dwMessage) {
   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Piers settings";
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}

/**********************************************************************************
CObjectPiers::DialogShow - Causes the object to show a dialog box that allows
different attributes to be changed.

inputs
   DWORD             dwSuface - Surface that was clicked on. 0 if unknown.
   PCEscWindow       pWindow - Escarpment window to draw it in
returns
   BOOL - TRUE if user pressed "Back", FALSE if user pressed "Close"
*/
BOOL CObjectPiers::DialogShow (DWORD dwSurface, PCEscWindow pWindow, PCPoint pClick)
{
   PWSTR pszRet;
firstpage:
   pszRet = pWindow->PageDialog (ghInstance, IDR_MMLPIERSDIALOG, PiersDialogPage, this);
firstpage2:
   if (!pszRet)
      return FALSE;
   if (!_wcsicmp(pszRet, L"balappear")) {
      pszRet = m_ds.AppearancePage (m_OSINFO.dwRenderShard, pWindow, NULL);

      if (pszRet && !_wcsicmp(pszRet, Back()))
         goto firstpage;
      else
         goto firstpage2;
   }
   else if (!_wcsicmp(pszRet, L"balopenings")) {
      pszRet = m_ds.OpeningsPage (m_OSINFO.dwRenderShard, pWindow, NULL);

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
      pszRet = m_ds.CornersPage (m_OSINFO.dwRenderShard, pWindow);

      if (pszRet && !_wcsicmp(pszRet, Back()))
         goto firstpage;
      else
         goto firstpage2;
   }

   return !_wcsicmp(pszRet, Back());
}



/**********************************************************************************
CObjectPiers::DialogCPShow - Causes the object to show a dialog box that allows
different attributes to be changed.

inputs
   DWORD             dwSuface - Surface that was clicked on. 0 if unknown.
   PCEscWindow       pWindow - Escarpment window to draw it in
returns
   BOOL - TRUE if user pressed "Back", FALSE if user pressed "Close"
*/
BOOL CObjectPiers::DialogCPShow (DWORD dwSurface, PCEscWindow pWindow, PCPoint pClick)
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
BOOL CObjectPiers::MoveReferencePointQuery (DWORD dwIndex, PCPoint pp)
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
   dwWidth = m_ds.m_psTop->OrigNumPointsGet();
   dwX = (ps[dwIndex].iX < 0) ? 0 : (dwWidth-1);
   dwY = (ps[dwIndex].iY < 0) ? 0 : 1;
   
   // special case for center bottom
   if ((ps[dwIndex].iX == 0) && (ps[dwIndex].iY != 0)) {
      dwX = 0;
      if (ps[dwIndex].iY < 0)
         m_ds.m_psTop->OrigPointGet (dwX, &p2);
      else
         m_ds.m_psBottom->OrigPointGet (dwX, &p2);
      dwX = dwWidth-1;
      if (ps[dwIndex].iY < 0)
         m_ds.m_psTop->OrigPointGet (dwX, pp);
      else
         m_ds.m_psBottom->OrigPointGet (dwX, pp);
      pp->Add (&p2);
      pp->Scale (.5);
      return TRUE;
   }

   if (dwY)
      return m_ds.m_psBottom->OrigPointGet (dwX, pp);
   else {
      CPoint p2;
      m_ds.m_psBottom->OrigPointGet (dwX, &p2);
      m_ds.m_psTop->OrigPointGet (dwX, pp);
      pp->Subtract (&p2);
      pp->Normalize();
      pp->Scale (m_ds.m_fHeight);
      pp->Add (&p2);
      return TRUE;
   }
}

/**************************************************************************************
CObjectPiers::MoveReferenceStringQuery -
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
BOOL CObjectPiers::MoveReferenceStringQuery (DWORD dwIndex, PWSTR psz, DWORD dwSize, DWORD *pdwNeeded)
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
CObjectPiers::Message -
sends a message to the object. The interpretation of the message depends upon
dwMessage, which is OSM_XXX. If the function understands and handles the
message it returns TRUE, otherwise FALE.
*/
BOOL CObjectPiers::Message (DWORD dwMessage, PVOID pParam)
{
   return FALSE;
}

/*************************************************************************************
CObjectPiers::IntelligentAdjust
Tells the object to intelligently adjust itself based on nearby objects.
For walls, this means triming to the roof line, for floors, different
textures, etc. If fAct is FALSE the function is just a query, that returns
TRUE if the object cares about adjustment and can try, FALSE if it can't.

NOTE: Often overridden
*/
BOOL CObjectPiers::IntelligentAdjust (BOOL fAct)
{
   if (!fAct)
      return TRUE;
   if (!m_pWorld)
      return FALSE;

   m_ds.ExtendPostsToGround(m_OSINFO.dwRenderShard);

   return TRUE;
}


/*************************************************************************************
CObjectPiers::Deconstruct -
Tells the object to deconstruct itself into sub-objects.
Basically, new objects will be added that exactly mimic this object,
and any embedeeding objects will be moved to the new ones.
NOTE: This old one will not be deleted - the called of Deconstruct()
will need to call Delete()
If fAct is FALSE the function is just a query, that returns
TRUE if the object cares about adjustment and can try, FALSE if it can't.

NOTE: Often overridden.
*/
BOOL CObjectPiers::Deconstruct (BOOL fAct)
{
   if (!m_ds.Deconstruct (m_OSINFO.dwRenderShard, FALSE))
      return FALSE;
   if (!fAct)
      return TRUE;

   // actually do it
   if (!m_ds.Deconstruct (m_OSINFO.dwRenderShard, TRUE))
      return FALSE;

   return TRUE;
}

