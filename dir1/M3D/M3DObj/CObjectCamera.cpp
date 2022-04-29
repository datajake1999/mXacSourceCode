/************************************************************************
CObjectCamera.cpp - Draws a box.

begun 26/2/02 by Mike Rozak
Copyright 2001 Mike Rozak. All rights reserved
*/

#include <windows.h>
#include "escarpment.h"
#include "..\M3D.h"
#include "resource.h"

/**********************************************************************************
CObjectCamera::Constructor and destructor */
CObjectCamera::CObjectCamera (PVOID pParams, POSINFO pInfo)
{
   m_OSINFO = *pInfo;

   m_dwRenderShow = RENDERSHOW_VIEWCAMERA;
   m_pView = NULL;
   m_fVisible = FALSE;
}


CObjectCamera::~CObjectCamera (void)
{
   // do nothing for now
}


/**********************************************************************************
CObjectCamera::Delete - Called to delete this object
*/
void CObjectCamera::Delete (void)
{
   if (m_pView)
      m_pView->CameraDeleted();

   delete this;
}

/**********************************************************************************
CObjectCamera::ObjectMatrixSet - Tells the view that it's been moved.
*/
BOOL CObjectCamera::ObjectMatrixSet (CMatrix *pObject)
{
   if (!CObjectTemplate::ObjectMatrixSet (pObject))
      return FALSE;

   if (m_pView)
      m_pView->CameraMoved (&m_MatrixObject);

   return TRUE;
}


/**********************************************************************************
CObjectCamera::Render - Draws a test box.

inputs
   POBJECTRENDER     pr - Render information
*/
void CObjectCamera::Render (POBJECTRENDER pr, DWORD dwSubObject)
{
   // may want to hide
   if ((pr->dwReason != ORREASON_BOUNDINGBOX) && !m_fVisible)
      return;

   // create the surface render object and draw
   // CRenderSurface rs;
   m_Renderrs.ClearAll();

   CMatrix mObject;
   m_Renderrs.Init (pr->pRS);
   ObjectMatrixGet (&mObject);
   m_Renderrs.Multiply (&mObject);

   // cameras are ret
   m_Renderrs.SetDefMaterial (m_OSINFO.dwRenderShard, NULL, NULL);
   m_Renderrs.SetDefColor (RGB(0xff, 0x40, 0x40));

   m_Renderrs.ShapeBox (-.25, -.01, -.25, .25, -.25, .25);
   m_Renderrs.ShapeBox (-.5, -.5, -.3, .5, -.25, .3);

   m_Renderrs.Commit();
}




/**********************************************************************************
CObjectCamera::QueryBoundingBox - Standard API
*/
void CObjectCamera::QueryBoundingBox (PCPoint pCorner1, PCPoint pCorner2, DWORD dwSubObject)
{
   pCorner1->Zero();
   pCorner2->Zero();
   pCorner1->p[0] = -.5;
   pCorner1->p[1] = -.5;
   pCorner1->p[2] = -.3;
   pCorner2->p[0] = .5;
   pCorner2->p[1] = -.01;
   pCorner2->p[2] = .3;

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
CObjectCamera::Clone - Clones this

inputs
   none
returns
   PCObjectSocket - Clone
*/
PCObjectSocket CObjectCamera::Clone (void)
{
   // dont let this be clones
   return NULL;

#if 0
   PCObjectCamera pNew;

   pNew = new CObjectCamera(NULL);

   // clone template info
   CloneTemplate(pNew);

   // Clone member variables
   //pNew->m_pView = m_pView;
   pNew->m_fVisible = m_fVisible;

   return pNew;
#endif

}




PCMMLNode2 CObjectCamera::MMLTo (void)
{
   return NULL;

#if 0 // dont want to save to mml
   // call into the template first
   PCMMLNode2 pNode = MMLToTemplate();
   if (!pNode)
      return NULL;
   return pNode;
#endif // 0
}

BOOL CObjectCamera::MMLFrom (PCMMLNode2 pNode)
{
   return FALSE;

#if 0 // dont want to save to MML
   // call into the template first
   if (!MMLFromTemplate (pNode))
      return FALSE;

   return TRUE;
#endif // 0
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
BOOL CObjectCamera::MoveReferencePointQuery (DWORD dwIndex, PCPoint pp)
{
   if (dwIndex != 0)
      return FALSE;

   pp->Zero();

   return TRUE;
}

/**************************************************************************************
CObjectCamera::MoveReferenceStringQuery -
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
BOOL CObjectCamera::MoveReferenceStringQuery (DWORD dwIndex, PWSTR psz, DWORD dwSize, DWORD *pdwNeeded)
{
   if (dwIndex) {
      if (pdwNeeded)
         *pdwNeeded = 0;
      return FALSE;
   }

   PWSTR pszOrigin = L"Position";

   DWORD dwNeeded;
   dwNeeded = ((DWORD)wcslen (pszOrigin) + 1) * 2;
   if (pdwNeeded)
      *pdwNeeded = dwNeeded;
   if (dwNeeded <= dwSize) {
      wcscpy (psz, pszOrigin);
      return TRUE;
   }
   else
      return FALSE;
}


/**********************************************************************************
CObjectCamera::Message -
sends a message to the object. The interpretation of the message depends upon
dwMessage, which is OSM_XXX. If the function understands and handles the
message it returns TRUE, otherwise FALE.
*/
BOOL CObjectCamera::Message (DWORD dwMessage, PVOID pParam)
{
   switch (dwMessage) {
   case OSM_IGNOREWORLDBOUNDINGBOXGET:
      {
         POSMIGNOREWORLDBOUNDINGBOXGET p= (POSMIGNOREWORLDBOUNDINGBOXGET) pParam;
         p->fIgnoreCompletely = TRUE;
      }
      return TRUE;


   case OSM_DENYCLIP:
      // dont let this be clipped by the floor clipping planes
      return TRUE;
   }

   return FALSE;
}

