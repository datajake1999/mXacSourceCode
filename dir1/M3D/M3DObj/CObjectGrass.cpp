/************************************************************************
CObjectGrass.cpp - Draws a box.

begun 10/3/05 by Mike Rozak
Copyright 2005 Mike Rozak. All rights reserved
*/

#include <windows.h>
#include "escarpment.h"
#include "..\M3D.h"
#include "resource.h"

static PWSTR gszCorner = L"corner";

/**********************************************************************************
CObjectGrass::Constructor and destructor */
CObjectGrass::CObjectGrass (PVOID pParams, POSINFO pInfo)
{
   m_OSINFO = *pInfo;

   m_dwRenderShow = RENDERSHOW_PLANTS;

   m_pCorner.Zero();
   m_pCorner.p[0] = m_pCorner.p[1] = 0.5;
   m_pCorner.p[2] = 0.5;

   // color for the box
   ObjectSurfaceAdd (10, RGB(0x40,0xff,0x40), MATERIAL_METALSMOOTH, L"Grass surface");
//      &GTEXTURECODE_Algorithmic, &GTEXTURESUB_Checkerboard);
}


CObjectGrass::~CObjectGrass (void)
{
   // do nothing for now
}


/**********************************************************************************
CObjectGrass::Delete - Called to delete this object
*/
void CObjectGrass::Delete (void)
{
   delete this;
}

/**********************************************************************************
CObjectGrass::Render - Draws a test box.

inputs
   POBJECTRENDER     pr - Render information
*/
void CObjectGrass::Render (POBJECTRENDER pr, DWORD dwSubObject)
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

   // textures
   DWORD dwTextIndex;
   PTEXTPOINT5 pText = m_Renderrs.NewTextures (&dwTextIndex, 4);;
   if (pText) {
      // UL
      pText[0].hv[0] = 0;
      pText[0].hv[1] = 0;
      pText[0].xyz[0] = -m_pCorner.p[0];
      pText[0].xyz[1] = 0;
      pText[0].xyz[2] = m_pCorner.p[2];

      // UR
      pText[1] = pText[0];
      pText[1].hv[0] = 1;
      pText[1].xyz[0] *= -1;

      // LR
      pText[2] = pText[1];
      pText[2].hv[1] = 1;
      pText[2].xyz[2] = 0;

      // LL
      pText[3] = pText[2];
      pText[3].hv[0] = 0;
      pText[3].xyz[0] *= -1;

      // NOTE: specifically not applying texture rotation so texture stretched across
   } // if pText

   // facing towards the viewer
   DWORD dwPointIndex, dwNorm;
   PCPoint pPoint;
   if (m_pCorner.p[0] > 2*CLOSE) {
      // create the points
      pPoint = m_Renderrs.NewPoints (&dwPointIndex, 4);

      // UL
      pPoint[0].p[0] = -m_pCorner.p[0];
      pPoint[0].p[1] = 0;
      pPoint[0].p[2] = m_pCorner.p[2];
      pPoint[0].p[3] = 1;

      // UR
      pPoint[1] = pPoint[0];
      pPoint[1].p[0] *= -1;

      // LR
      pPoint[2] = pPoint[1];
      pPoint[2].p[2] = 0;

      // LL
      pPoint[3] = pPoint[2];
      pPoint[3].p[0] *= -1;

      dwNorm = m_Renderrs.DefNormal (2);

      m_Renderrs.NewQuad (
         m_Renderrs.NewVertex (dwPointIndex + 0, dwNorm, pText ? (dwTextIndex+0) : 0),
         m_Renderrs.NewVertex (dwPointIndex + 1, dwNorm, pText ? (dwTextIndex+1) : 0),
         m_Renderrs.NewVertex (dwPointIndex + 2, dwNorm, pText ? (dwTextIndex+2) : 0),
         m_Renderrs.NewVertex (dwPointIndex + 3, dwNorm, pText ? (dwTextIndex+3) : 0),
         FALSE);  // no backface culling on grass
   }

   // perp to viewer
   if (m_pCorner.p[1] > 2*CLOSE) {
      // create the points
      pPoint = m_Renderrs.NewPoints (&dwPointIndex, 4);

      // UL
      pPoint[0].p[1] = -m_pCorner.p[1];
      pPoint[0].p[0] = 0;
      pPoint[0].p[2] = m_pCorner.p[2];
      pPoint[0].p[3] = 1;

      // UR
      pPoint[1] = pPoint[0];
      pPoint[1].p[1] *= -1;

      // LR
      pPoint[2] = pPoint[1];
      pPoint[2].p[2] = 0;

      // LL
      pPoint[3] = pPoint[2];
      pPoint[3].p[1] *= -1;

      dwNorm = m_Renderrs.DefNormal (0);

      m_Renderrs.NewQuad (
         m_Renderrs.NewVertex (dwPointIndex + 0, dwNorm, pText ? (dwTextIndex+0) : 0),
         m_Renderrs.NewVertex (dwPointIndex + 1, dwNorm, pText ? (dwTextIndex+1) : 0),
         m_Renderrs.NewVertex (dwPointIndex + 2, dwNorm, pText ? (dwTextIndex+2) : 0),
         m_Renderrs.NewVertex (dwPointIndex + 3, dwNorm, pText ? (dwTextIndex+3) : 0),
         FALSE);  // no backface culling on grass
   }

   m_Renderrs.Commit();
}



/**********************************************************************************
CObjectGrass::QueryBoundingBox - Standard API
*/
void CObjectGrass::QueryBoundingBox (PCPoint pCorner1, PCPoint pCorner2, DWORD dwSubObject)
{
   pCorner1->Copy(&m_pCorner);
   pCorner1->Scale(-1);
   pCorner2->Copy(&m_pCorner);

#ifdef _DEBUG
   // test, make sure bounding box not too small
   CPoint p1,p2;
   DWORD i;
   CObjectTemplate::QueryBoundingBox (&p1, &p2, dwSubObject);
   for (i = 0; i < 3; i++)
      if ((p1.p[i] < pCorner1->p[i] - CLOSE) || (p2.p[i] > pCorner2->p[i] + CLOSE))
         break;
   if (i < 3)
      OutputDebugString ("\r\nCObjectGrass::QueryBoundingBox too small.");
#endif
}

/**********************************************************************************
CObjectGrass::Clone - Clones this

inputs
   none
returns
   PCObjectSocket - Clone
*/
PCObjectSocket CObjectGrass::Clone (void)
{
   PCObjectGrass pNew;

   pNew = new CObjectGrass(NULL, &m_OSINFO);

   // clone template info
   CloneTemplate(pNew);

   // Clone member variables
   pNew->m_pCorner.Copy (&m_pCorner);

   return pNew;
}




PCMMLNode2 CObjectGrass::MMLTo (void)
{
   // call into the template first
   PCMMLNode2 pNode = MMLToTemplate();
   if (!pNode)
      return NULL;

   // member variables go here
   MMLValueSet (pNode, gszCorner, &m_pCorner);

   return pNode;
}

BOOL CObjectGrass::MMLFrom (PCMMLNode2 pNode)
{
   // call into the template first
   if (!MMLFromTemplate (pNode))
      return FALSE;

   // member variables go here
   MMLValueGetPoint (pNode, gszCorner, &m_pCorner);

   return TRUE;
}

/*************************************************************************************
CObjectGrass::ControlPointQuery - Called to query the information about a control
point identified by dwID.

inputs
   DWORD       dwID - ID
   POSCONTROL  pInfo - Filled in with the infomration
returns
   BOOL - TRUE if successful
*/
BOOL CObjectGrass::ControlPointQuery (DWORD dwID, POSCONTROL pInfo)
{
   if (dwID != 10)
      return FALSE;

   memset (pInfo,0, sizeof(*pInfo));

   pInfo->dwID = dwID;
   // BUGFIX - Since freedom != 0 causes problems, use freedom=0: pInfo->dwFreedom = 2;   // line
   pInfo->dwStyle = CPSTYLE_POINTER;
   pInfo->fSize = m_pCorner.Length() / 20;
   pInfo->cColor = RGB(0xff,0,0xff);

   // start location in mid point
   pInfo->pLocation.Copy (&m_pCorner);
   //pInfo->pV1.Zero();

   wcscpy (pInfo->szName, L"Size");

   return TRUE;
}

/*************************************************************************************
CObjectGrass::ControlPointSet - Called to change the valud of a control point.

inputs
   DWORD       dwID - ID
   PCPoint     pVal - Contains the new location, in object coordinates
returns
   BOOL - TRUE if successful
*/
BOOL CObjectGrass::ControlPointSet (DWORD dwID, PCPoint pVal, PCPoint pViewer)
{
   if (dwID != 10)
      return FALSE;

   // tell the world we're about to change
   if (m_pWorld)
      m_pWorld->ObjectAboutToChange (this);

   m_pCorner.Copy (pVal);
   m_pCorner.p[0] = max(m_pCorner.p[0], CLOSE);
   m_pCorner.p[1] = max(m_pCorner.p[1], CLOSE);
   m_pCorner.p[2] = max(m_pCorner.p[2], CLOSE);

   // tell the world we've changed
   if (m_pWorld)
      m_pWorld->ObjectChanged (this);

   return TRUE;
}

/*************************************************************************************
CObjectGrass::ControlPointEnum - Called to enumerate a list of control point IDs
into the list.

inputs
   PCListFixed       plDWORD - Should be filled in with control points
returns
   none
*/
void CObjectGrass::ControlPointEnum (PCListFixed plDWORD)
{
   // 1 control points starting at 10
   DWORD i;
   i = 10;
   plDWORD->Add (&i);
}

