/************************************************************************
CAnimPath.cpp - Object code
*/

#include <windows.h>
#include <math.h>
#include "escarpment.h"
#include "..\M3D.h"
#include "resource.h"

/************************************************************************
CAnimPath::Constructor and destructor */
CAnimPath::CAnimPath (void)
{
   m_gClass = CLSID_AnimPath;
   m_iPriority = 10;
   m_iMinDisplayX = 16;
   m_iMinDisplayY = 32;
   
   // BUGBUG - acclocate for attributes
}

CAnimPath::~CAnimPath (void)
{
   // BUGBUG - free up attributes stored?
}

/************************************************************************
CAnimPath::Delete - See CAnimSocket for info
*/
void CAnimPath::Delete (void)
{
   delete this;
}

/************************************************************************
CAnimPath::AnimObjSplineApply - See CAnimSocket for info
*/
void CAnimPath::AnimObjSplineApply (PCSceneObj pSpline)
{
   // BUGBUG - This is a hack to make sure it's working

   // find the object
   PCSceneObj pSceneObj;
   if (!m_pScene)
      return;
   pSceneObj = m_pScene->ObjectGet (&m_gWorldObject);
   if (!pSceneObj)
      return;

   // get the left and right starting values
   fp fLeft, fRight;
   PCAnimAttrib paa;
   PWSTR pszAttrib = L"Rotation (Z)";
   paa = pSceneObj->AnimAttribGet (pszAttrib, TRUE);
   if (!paa)
      return;
   fLeft = paa->ValueGet (m_pLoc.p[0]);
   fRight = paa->ValueGet (m_pLoc.p[1]);

   // put in sine wave over 10 slices, interp
   DWORD i;
   TEXTUREPOINT tp;
   for (i = 0; i < 10; i++) {
      tp.h = (fp) i / 9.0 * (m_pLoc.p[1] - m_pLoc.p[0]) + m_pLoc.p[0];
      tp.v = sin ((fp) i / 9.0 * 2 * PI) * PI +
         (fp) i / 9.0 * (fRight - fLeft) + fLeft;
      paa->PointAdd (&tp, 2); // add
   }
}

/************************************************************************
CAnimPath::Clone - See CAnimSocket for info
*/
CAnimSocket *CAnimPath::Clone (void)
{
   PCAnimPath pNew = new CAnimPath;
   if (!pNew)
      return NULL;
   CloneTemplate (pNew);

   // BUGBUG - clone attributes

   return pNew;
}

/************************************************************************
CAnimPath::MMLTo - See CAnimSocket for info
*/
PCMMLNode2 CAnimPath::MMLTo (void)
{
   PCMMLNode2 pNode = MMLToTemplate ();
   if (!pNode)
      return NULL;

   // BUGBUG - write in attributes

   return pNode;
}

/************************************************************************
CAnimPath::MMLFrom - See CAnimSocket for info
*/
BOOL CAnimPath::MMLFrom (PCMMLNode2 pNode)
{
   if (!MMLFromTemplate (pNode))
      return FALSE;

   // BUGBUG - read in attributes

   return TRUE;
}


/************************************************************************
CAnimPath::Draw - See CAnimSocket for info
*/
void CAnimPath::Draw (HDC hDC, RECT *prHDC, fp fLeft, fp fRight)
{
   // BUGBUG - Different image

   // overwrite with bitmap
   HDC hDCNew;
   HBITMAP hBmp;
   hDCNew = CreateCompatibleDC (hDC);
   hBmp = LoadBitmap (ghInstance, MAKEINTRESOURCE(IDB_ANIMKEYFRAME));
   SelectObject (hDCNew, hBmp);

   // get the size of the bitmap
   BITMAP   bm;
   GetObject (hBmp, sizeof(bm), &bm);

   StretchBlt (hDC, prHDC->left, prHDC->top,
      prHDC->right - prHDC->left, prHDC->bottom - prHDC->top,
      hDCNew, 0, 0, bm.bmWidth, bm.bmHeight, SRCCOPY);

   DeleteDC (hDCNew);
   DeleteObject (hBmp);
}

/************************************************************************
CAnimPath::DialogQuery - See CAnimSocket for info
*/
BOOL CAnimPath::DialogQuery (void)
{
   return TRUE;
}

/************************************************************************
CAnimPath::DialogShow - See CAnimSocket for info
*/
BOOL CAnimPath::DialogShow (PCEscWindow pWindow)
{
   // BUGBUG - Do a dialog for this
   return FALSE;
}

