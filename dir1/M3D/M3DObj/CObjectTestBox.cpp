/************************************************************************
CObjectTestBox.cpp - Draws a box.

begun 12/9/01 by Mike Rozak
Copyright 2001 Mike Rozak. All rights reserved
*/

#include <windows.h>
#include "escarpment.h"
#include "..\M3D.h"
#include "resource.h"

static PWSTR gszCorner1 = L"corner1";
static PWSTR gszCorner2 = L"corner2";

/**********************************************************************************
CObjectTestBox::Constructor and destructor */
CObjectTestBox::CObjectTestBox (PVOID pParams, POSINFO pInfo)
{
   m_OSINFO = *pInfo;

   m_dwRenderShow = RENDERSHOW_MISC;

   m_pCorner[0].Zero();
   m_pCorner[1].Zero();

   m_pCorner[0].p[0] = -2;
   m_pCorner[0].p[1] = -.1;
   m_pCorner[0].p[2] = -2;
   m_pCorner[1].p[0] = 2;
   m_pCorner[1].p[1] = .1;
   m_pCorner[1].p[2] = 2;

   // color for the box
   ObjectSurfaceAdd (10, RGB(0x40,0xff,0x40), MATERIAL_METALSMOOTH, L"Box colors");
//      &GTEXTURECODE_Algorithmic, &GTEXTURESUB_Checkerboard);
}


CObjectTestBox::~CObjectTestBox (void)
{
   // do nothing for now
}


/**********************************************************************************
CObjectTestBox::Delete - Called to delete this object
*/
void CObjectTestBox::Delete (void)
{
   delete this;
}

/**********************************************************************************
CObjectTestBox::Render - Draws a test box.

inputs
   POBJECTRENDER     pr - Render information
*/
void CObjectTestBox::Render (POBJECTRENDER pr, DWORD dwSubObject)
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

   // transparency
   //RENDERSURFACE r;
   //memset (&r, 0, sizeof(r));
   //r.wTransparency = 0xe000;
   //m_Renderrs.SetDefMaterial (&r);

   m_Renderrs.ShapeBox (m_pCorner[0].p[0], m_pCorner[0].p[1], m_pCorner[0].p[2],
      m_pCorner[1].p[0], m_pCorner[1].p[1], m_pCorner[1].p[2]);

   m_Renderrs.Commit();
}

/**********************************************************************************
CObjectTestBox::QueryBoundingBox - Standard API
*/
void CObjectTestBox::QueryBoundingBox (PCPoint pCorner1, PCPoint pCorner2, DWORD dwSubObject)
{
   DWORD i;
   for (i = 0; i < 3; i++) {
      pCorner1->p[i] = min (m_pCorner[0].p[i], m_pCorner[1].p[i]);
      pCorner2->p[i] = max (m_pCorner[0].p[i], m_pCorner[1].p[i]);
   } // i
}

/**********************************************************************************
CObjectTestBox::Clone - Clones this

inputs
   none
returns
   PCObjectSocket - Clone
*/
PCObjectSocket CObjectTestBox::Clone (void)
{
   PCObjectTestBox pNew;

   pNew = new CObjectTestBox(NULL, &m_OSINFO);

   // clone template info
   CloneTemplate(pNew);

   // Clone member variables
   pNew->m_pCorner[0].Copy (&m_pCorner[0]);
   pNew->m_pCorner[1].Copy (&m_pCorner[1]);

   return pNew;
}




PCMMLNode2 CObjectTestBox::MMLTo (void)
{
   // call into the template first
   PCMMLNode2 pNode = MMLToTemplate();
   if (!pNode)
      return NULL;

   // member variables go here
   MMLValueSet (pNode, gszCorner1, &m_pCorner[0]);
   MMLValueSet (pNode, gszCorner2, &m_pCorner[1]);

   return pNode;
}

BOOL CObjectTestBox::MMLFrom (PCMMLNode2 pNode)
{
   // call into the template first
   if (!MMLFromTemplate (pNode))
      return FALSE;

   // member variables go here
   MMLValueGetPoint (pNode, gszCorner1, &m_pCorner[0]);
   MMLValueSet (pNode, gszCorner2, &m_pCorner[1]);

   return TRUE;
}

/*************************************************************************************
CObjectTestBox::ControlPointQuery - Called to query the information about a control
point identified by dwID.

inputs
   DWORD       dwID - ID
   POSCONTROL  pInfo - Filled in with the infomration
returns
   BOOL - TRUE if successful
*/
BOOL CObjectTestBox::ControlPointQuery (DWORD dwID, POSCONTROL pInfo)
{
   if ((dwID < 10) || (dwID >= 16))
      return FALSE;

   memset (pInfo,0, sizeof(*pInfo));

   CPoint pCenter;
   pCenter.Add (&m_pCorner[0], &m_pCorner[1]);
   pCenter.Scale (.5);

   pInfo->dwID = dwID;
   // BUGFIX - Since freedom != 0 causes problems, use freedom=0: pInfo->dwFreedom = 2;   // line
   pInfo->dwStyle = CPSTYLE_POINTER;
   pInfo->fSize = DistancePointToPoint(&m_pCorner[0], &m_pCorner[1]) / 20;
   pInfo->cColor = RGB(0xff,0,0xff);

   // start location in mid point
   pInfo->pLocation.Copy (&pCenter);
   //pInfo->pV1.Zero();

   // from the ID calculate the dimension and corner
   DWORD dwCorner, dwDim;
   dwCorner = (dwID - 10) / 3;
   dwDim = (dwID - 10) % 3;

   switch (dwDim) {
   case 0:
      wcscpy (pInfo->szName, L"Width");
      break;
   case 1:
      wcscpy (pInfo->szName, L"Depth");
      break;
   case 2:
      wcscpy (pInfo->szName, L"Height");
      break;
   default:
      return FALSE;
   }

   MeasureToString (m_pCorner[1].p[dwDim] - m_pCorner[0].p[dwDim], pInfo->szMeasurement);
   pInfo->pLocation.p[dwDim] = m_pCorner[dwCorner].p[dwDim];
   //pInfo->pV1.p[dwDim] = 1;

   // direction is the point minus the center
   //pInfo->pDirection.Subtract (&pInfo->pLocation, &pCenter);
   
   return TRUE;
}

/*************************************************************************************
CObjectTestBox::ControlPointSet - Called to change the valud of a control point.

inputs
   DWORD       dwID - ID
   PCPoint     pVal - Contains the new location, in object coordinates
returns
   BOOL - TRUE if successful
*/
BOOL CObjectTestBox::ControlPointSet (DWORD dwID, PCPoint pVal, PCPoint pViewer)
{
   if ((dwID < 10) || (dwID >= 16))
      return FALSE;

   // tell the world we're about to change
   if (m_pWorld)
      m_pWorld->ObjectAboutToChange (this);

   // from the ID calculate the dimension and corner
   DWORD dwCorner, dwDim;
   dwCorner = (dwID - 10) / 3;
   dwDim = (dwID - 10) % 3;

   if (dwCorner == 0) {
      m_pCorner[dwCorner].p[dwDim] = min(m_pCorner[1].p[dwDim], pVal->p[dwDim]);
   }
   else {
      m_pCorner[dwCorner].p[dwDim] = max(m_pCorner[0].p[dwDim], pVal->p[dwDim]);
   }


   // tell the world we've changed
   if (m_pWorld)
      m_pWorld->ObjectChanged (this);

   return TRUE;
}

/*************************************************************************************
CObjectTestBox::ControlPointEnum - Called to enumerate a list of control point IDs
into the list.

inputs
   PCListFixed       plDWORD - Should be filled in with control points
returns
   none
*/
void CObjectTestBox::ControlPointEnum (PCListFixed plDWORD)
{
   // 6 control points starting at 10
   DWORD i;
   for (i = 10; i < 16; i++)
      plDWORD->Add (&i);
}

