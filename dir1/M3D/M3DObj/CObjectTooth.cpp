/************************************************************************
CObjectTooth.cpp - Draws a Tooth.

begun 14/9/01 by Mike Rozak
Copyright 2001 Mike Rozak. All rights reserved
*/

#include <windows.h>
#include "escarpment.h"
#include "..\M3D.h"
#include "resource.h"




typedef struct {
   PWSTR       pszName;    // name of the move point
   int         iX, iY; // -1 if for left/top, 1 for right/bottom
} SPLINEMOVEP, *PSPLINEMOVEP;

static SPLINEMOVEP   gaToothMove[] = {
   L"Root", 0, 0
};


/**********************************************************************************
CObjectTooth::Constructor and destructor */
CObjectTooth::CObjectTooth (PVOID pParams, POSINFO pInfo)
{
   m_dwRenderShow = RENDERSHOW_CHARACTER;
   m_OSINFO = *pInfo;


   // color for the Tooth
   ObjectSurfaceAdd (42, RGB(0xff,0xff,0xf0), MATERIAL_TILEGLAZED);
}


CObjectTooth::~CObjectTooth (void)
{
   // do nothing for now
}


/**********************************************************************************
CObjectTooth::Delete - Called to delete this object
*/
void CObjectTooth::Delete (void)
{
   delete this;
}

/**********************************************************************************
CObjectTooth::Render - Draws a test box.

inputs
   POBJECTRENDER     pr - Render information
*/
void CObjectTooth::Render (POBJECTRENDER pr, DWORD dwSubObject)
{
   // create the surface render object and draw
   // CRenderSurface rs;
   m_Renderrs.ClearAll();

   CMatrix mObject;
   m_Renderrs.Init (pr->pRS);
   ObjectMatrixGet (&mObject);
   m_Renderrs.Multiply (&mObject);

   // object specific
   m_Renderrs.SetDefMaterial (m_OSINFO.dwRenderShard, ObjectSurfaceFind (42), m_pWorld);

   m_Tooth.Render (pr, &m_Renderrs);

   m_Renderrs.Commit();
}


/**********************************************************************************
CObjectTooth::QueryBoundingBox - Standard API
*/
void CObjectTooth::QueryBoundingBox (PCPoint pCorner1, PCPoint pCorner2, DWORD dwSubObject)
{
   m_Tooth.QueryBoundingBox (pCorner1, pCorner2);

#ifdef _DEBUG
   // test, make sure bounding box not too small
   CPoint p1,p2;
   DWORD i;
   CObjectTemplate::QueryBoundingBox (&p1, &p2, dwSubObject);
   for (i = 0; i < 3; i++)
      if ((p1.p[i] < pCorner1->p[i]) || (p2.p[i] > pCorner2->p[i]))
         break;
   if (i < 3)
      OutputDebugString ("\r\nCObjectTooth::QueryBoundingBox too small.");
#endif
}


/**********************************************************************************
CObjectTooth::Clone - Clones this

inputs
   none
returns
   PCObjectSocket - Clone
*/
PCObjectSocket CObjectTooth::Clone (void)
{
   PCObjectTooth pNew;

   pNew = new CObjectTooth(NULL, &m_OSINFO);

   // clone template info
   CloneTemplate(pNew);

   m_Tooth.CloneTo (&pNew->m_Tooth);

   return pNew;
}

static PWSTR gpszTooth = L"Tooth";

PCMMLNode2 CObjectTooth::MMLTo (void)
{
   // call into the template first
   PCMMLNode2 pNode = MMLToTemplate();
   if (!pNode)
      return NULL;

   PCMMLNode2 pSub;
   pSub = m_Tooth.MMLTo();
   if (pSub) {
      pSub->NameSet (gpszTooth);
      pNode->ContentAdd (pSub);
   }

   return pNode;
}

BOOL CObjectTooth::MMLFrom (PCMMLNode2 pNode)
{
   // call into the template first
   if (!MMLFromTemplate (pNode))
      return FALSE;

   PCMMLNode2 pSub;
   PWSTR psz;
   pSub = NULL;
   pNode->ContentEnum (pNode->ContentFind (gpszTooth), &psz, &pSub);
   if (pSub)
      m_Tooth.MMLFrom (pSub);

   return TRUE;
}




/**************************************************************************************
CObjectTooth::MoveReferencePointQuery - 
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
BOOL CObjectTooth::MoveReferencePointQuery (DWORD dwIndex, PCPoint pp)
{
   PSPLINEMOVEP ps;
   DWORD dwDataSize;
   ps = gaToothMove;
   dwDataSize = sizeof(gaToothMove);

   if (dwIndex >= dwDataSize / sizeof(SPLINEMOVEP))
      return FALSE;

   // always at 0,0 in Tooths
   pp->Zero();

   return TRUE;
}

/**************************************************************************************
CObjectTooth::MoveReferenceStringQuery -
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
BOOL CObjectTooth::MoveReferenceStringQuery (DWORD dwIndex, PWSTR psz, DWORD dwSize, DWORD *pdwNeeded)
{
   PSPLINEMOVEP ps;
   DWORD dwDataSize;
   ps = gaToothMove;
   dwDataSize = sizeof(gaToothMove);
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
CObjectTooth::DialogShow - Causes the object to show a dialog box that allows
different attributes to be changed.

inputs
   DWORD             dwSuface - Surface that was clicked on. 0 if unknown.
   PCEscWindow       pWindow - Escarpment window to draw it in
returns
   BOOL - TRUE if user pressed "Back", FALSE if user pressed "Close"
*/
BOOL CObjectTooth::DialogShow (DWORD dwSurface, PCEscWindow pWindow, PCPoint pClick)
{
   return m_Tooth.Dialog (this, NULL, pWindow);
}

/**********************************************************************************
CObjectTooth::DialogQuery - Returns TRUE if the object supports a custom
dialog box, FALSE if it doesn't. The default object doesn't
*/
BOOL CObjectTooth::DialogQuery (void)
{
   return TRUE;
}


