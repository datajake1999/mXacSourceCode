/************************************************************************
CObjectTeapot.cpp - Draws a teapot.

begun 14/9/01 by Mike Rozak
Copyright 2001 Mike Rozak. All rights reserved
*/

#include <windows.h>
#include "escarpment.h"
#include "..\M3D.h"
#include "resource.h"

/**********************************************************************************
CObjectTeapot::Constructor and destructor */
CObjectTeapot::CObjectTeapot (PVOID pParams, POSINFO pInfo)
{
   m_dwRenderShow = RENDERSHOW_MISC;
   m_OSINFO = *pInfo;


   // color for the teapot
   TEXTUREMODS tm;
   memset (&tm, 0, sizeof(tm));
   tm.cTint = RGB(0xff,0xff,0xff);
   tm.wBrightness = 0x1000;
   tm.wContrast = 0x1000;
   tm.wHue = 0x0000;
   tm.wSaturation = 0x1000;
   ObjectSurfaceAdd (42, RGB(0xff,0xff,0xe0), MATERIAL_TILEGLAZED, L"Teapot colors");
      //&GTEXTURECODE_Bitmap, &GTEXTURESUB_Printer, &tm);
}


CObjectTeapot::~CObjectTeapot (void)
{
   // do nothing for now
}


/**********************************************************************************
CObjectTeapot::Delete - Called to delete this object
*/
void CObjectTeapot::Delete (void)
{
   delete this;
}

/**********************************************************************************
CObjectTeapot::Render - Draws a test box.

inputs
   POBJECTRENDER     pr - Render information
*/
void CObjectTeapot::Render (POBJECTRENDER pr, DWORD dwSubObject)
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

   // transparency
   //RENDERSURFACE r;
   //memset (&r, 0, sizeof(r));
   //r.wTransparency = 0xe000;
   //m_Renderrs.SetDefMaterial (&r);

   // make embedded teapots smaller
   GUID gCont;
   if (EmbedContainerGet(&gCont))
      m_Renderrs.Scale (.3);

   m_Renderrs.Rotate (PI/2+ 0*PI/6, 1);
   m_Renderrs.ShapeTeapot ();


   //m_Renderrs.ShapeEllipsoid (4,2,1);
   //m_Renderrs.ShapeBox (-4/5.0,-1/5.0,-9/5.0,4/5.0,1/5.0,9/5.0);

   //m_Renderrs.Rotate (PI/4, 1);
   //m_Renderrs.NewIDMinor();
   //m_Renderrs.SetDefColor (RGB(0,0xff,0xff));
   //m_Renderrs.ShapeFunnel (5, .2, .4);

   m_Renderrs.Commit();
}

// NOTE: Not bothiner with QueryBoundingBox

/**********************************************************************************
CObjectTeapot::Clone - Clones this

inputs
   none
returns
   PCObjectSocket - Clone
*/
PCObjectSocket CObjectTeapot::Clone (void)
{
   PCObjectTeapot pNew;

   pNew = new CObjectTeapot(NULL, &m_OSINFO);

   // clone template info
   CloneTemplate(pNew);

   return pNew;
}

PCMMLNode2 CObjectTeapot::MMLTo (void)
{
   // call into the template first
   PCMMLNode2 pNode = MMLToTemplate();
   if (!pNode)
      return NULL;

   return pNode;
}

BOOL CObjectTeapot::MMLFrom (PCMMLNode2 pNode)
{
   // call into the template first
   if (!MMLFromTemplate (pNode))
      return FALSE;

   return TRUE;
}
