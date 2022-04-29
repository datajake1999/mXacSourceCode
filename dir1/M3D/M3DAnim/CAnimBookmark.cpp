/************************************************************************
CAnimBookmark.cpp - Object code

Begun 29/1/03 by Mike Rozak
Copyrigh 2003 Mike Rozak. All rights reserved.
*/

#include <windows.h>
#include <math.h>
#include "escarpment.h"
#include "..\M3D.h"
#include "resource.h"

/************************************************************************
CAnimBookmark::Constructor and destructor

inputs
   DWORD       dwType - 0 for Bookmark excluding motion, 1 for location, 2 for rotation
*/
CAnimBookmark::CAnimBookmark (DWORD dwType)
{
   m_gClass = CLSID_AnimBookmark;
   m_dwType = dwType;
   m_iPriority = 0;
   m_iMinDisplayX = 16;
   m_iMinDisplayY = dwType ? 16 : 64;
   m_fTimeMax = (dwType ? 1000000 : 0);
   wcscpy (m_szName, L"Unnamed bookmark");
}

CAnimBookmark::~CAnimBookmark (void)
{
   // do nothing for now
}

/************************************************************************
CAnimBookmark::Delete - See CAnimSocket for info
*/
void CAnimBookmark::Delete (void)
{
   delete this;
}

/************************************************************************
CAnimBookmark::AnimObjSplineApply - See CAnimSocket for info
*/
void CAnimBookmark::AnimObjSplineApply (PCSceneObj pSpline)
{
   // do nothing
}

/************************************************************************
CAnimBookmark::Clone - See CAnimSocket for info
*/
CAnimSocket *CAnimBookmark::Clone (void)
{
   PCAnimBookmark pNew = new CAnimBookmark (m_dwType);
   if (!pNew)
      return NULL;
   CloneTemplate (pNew);

   pNew->m_dwType = m_dwType;
   wcscpy (pNew->m_szName, m_szName);

   return pNew;
}

/************************************************************************
CAnimBookmark::MMLTo - See CAnimSocket for info
*/
static PWSTR gpszName = L"Name";
static PWSTR gpszType = L"Type";

PCMMLNode2 CAnimBookmark::MMLTo (void)
{
   PCMMLNode2 pNode = MMLToTemplate ();
   if (!pNode)
      return NULL;

   MMLValueSet (pNode, gpszType, (int) m_dwType);
   if (m_szName[0])
      MMLValueSet (pNode, gpszName, m_szName);

   return pNode;
}

/************************************************************************
CAnimBookmark::MMLFrom - See CAnimSocket for info
*/
BOOL CAnimBookmark::MMLFrom (PCMMLNode2 pNode)
{
   if (!MMLFromTemplate (pNode))
      return FALSE;

   m_dwType = (DWORD) MMLValueGetInt (pNode, gpszType, 0);
   m_szName[0] = 0;
   PWSTR psz;
   psz = MMLValueGet (pNode, gpszName);
   if (psz)
      wcscpy (m_szName, psz);

   m_fTimeMax = (m_dwType ? 1000000 : 0);
   m_iMinDisplayY = m_dwType ? 16 : 64;
   return TRUE;
}


/************************************************************************
CAnimBookmark::Draw - See CAnimSocket for info
*/
void CAnimBookmark::Draw (HDC hDC, RECT *prHDC, fp fLeft, fp fRight)
{
   // paint green
   HBRUSH hbr = CreateSolidBrush (RGB(0x80,0xff,0x80));
   FillRect (hDC, prHDC, hbr);
   DeleteObject (hbr);

   // create the two fonts
   HFONT hFontNorm, hFont90, hFontOld;
   LOGFONT lf;
   DWORD i;
   for (i = 0; i < 2; i++) {
      memset (&lf, 0, sizeof(lf));
      lf.lfHeight = -10;   // 10 pixels high MulDiv(iPointSize, EscGetDeviceCaps(hDC, LOGPIXELSY), 72); 
      lf.lfCharSet = DEFAULT_CHARSET;
      lf.lfWeight = FW_NORMAL;   // BUGFIX - Adjust the weight of all fonts to normal
      lf.lfPitchAndFamily = FF_SWISS | VARIABLE_PITCH;
      strcpy (lf.lfFaceName, "Arial");
      if (i) {
         lf.lfEscapement = 2700; // 900;
         //lf.lfOrientation = 900;
         hFont90 = CreateFontIndirect (&lf);
      }
      else
      hFontNorm = CreateFontIndirect (&lf);
   }
   hFontOld = (HFONT) SelectObject (hDC, hFontNorm);
   int iOldMode;
   iOldMode = SetBkMode (hDC, TRANSPARENT);

   // name
   char szTemp[128];
   WideCharToMultiByte (CP_ACP, 0, m_szName, -1, szTemp, sizeof(szTemp),0,0);
   if (!szTemp[0])
      strcpy (szTemp, "Unknown");

   // calculate the text size
   int iLen;
   SIZE size;
   iLen = (DWORD)strlen(szTemp);
   SelectObject (hDC, hFontNorm);
   GetTextExtentPoint32 (hDC, szTemp, iLen, &size);

   // draw text
   COLORREF crOld;
   crOld = SetTextColor (hDC, RGB(0,0,0));
   if (!m_dwType) {
      SelectObject (hDC, hFont90);
      ExtTextOut (hDC,  prHDC->left + (prHDC->right - prHDC->left - size.cy) / 2 + size.cy, prHDC->top + 2,
         ETO_CLIPPED, prHDC, szTemp, iLen, NULL);
   }
   else {
      ExtTextOut (hDC,  prHDC->left + 2, prHDC->top + (prHDC->bottom - prHDC->top - size.cy)/2,
         ETO_CLIPPED, prHDC, szTemp, iLen, NULL);
   }
   SetTextColor (hDC, crOld);
   // free font
   SelectObject (hDC, hFontOld);
   DeleteObject (hFontNorm);
   DeleteObject (hFont90);
   SetBkMode (hDC, iOldMode);
}


/************************************************************************
CAnimBookmark::DialogQuery - See CAnimSocket for info
*/
BOOL CAnimBookmark::DialogQuery (void)
{
   return TRUE;
}

/****************************************************************************
AnimBookmarkPage
*/
BOOL AnimBookmarkPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCAnimBookmark pas = (PCAnimBookmark) pPage->m_pUserData;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         PCEscControl pControl;
         pControl = pPage->ControlFind (L"name");
         if (pControl)
            pControl->AttribSet (Text(), pas->m_szName);
      }
      break;


   case ESCN_EDITCHANGE:
      {
         PESCNEDITCHANGE p = (PESCNEDITCHANGE) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;
         PWSTR psz;
         psz = p->pControl->m_pszName;

         if (!_wcsicmp(psz, L"name")) {
            if (pas->m_pScene && pas->m_pScene->SceneSetGet())
               pas->m_pScene->SceneSetGet()->ObjectAboutToChange (pas);
            DWORD dwNeeded;
            p->pControl->AttribGet (Text(), pas->m_szName, sizeof(pas->m_szName), &dwNeeded);
            if (pas->m_pScene && pas->m_pScene->SceneSetGet())
               pas->m_pScene->SceneSetGet()->ObjectChanged (pas);
            return TRUE;
         }
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Bookmark settings";
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}

/************************************************************************
CAnimBookmark::DialogShow - See CAnimSocket for info
*/
BOOL CAnimBookmark::DialogShow (PCEscWindow pWindow)
{
   PWSTR pszRet;

   pszRet = pWindow->PageDialog (ghInstance, IDR_MMLANIMBOOKMARK, AnimBookmarkPage, this);

   if (!pszRet)
      return FALSE;

   return !_wcsicmp(pszRet, Back());
}

/************************************************************************
CAnimBookmark::Message - See CAnimSocket for info
*/
BOOL CAnimBookmark::Message (DWORD dwMessage, PVOID pParam)
{
   switch (dwMessage) {
   case OSM_BOOKMARK:
      {
         POSMBOOKMARK p = (POSMBOOKMARK) pParam;
         memset (p, 0, sizeof(*p));
         p->fEnd = m_pLoc.p[1];
         p->fStart = m_pLoc.p[0];
         p->gObjectAnim = m_gAnim;
         p->gObjectWorld = m_gWorldObject;
         wcscpy (p->szName, m_szName);
         return TRUE;
      }
      break;
   }

   return FALSE;
}

/************************************************************************
CAnimBookmark::Deconstruct - See CAnimSocket for info
*/
BOOL CAnimBookmark::Deconstruct (BOOL fAct)
{
   // while other objets can auomagically be deconstructed into Bookmarks, this
   // is already a Bookmark so it can't
   return FALSE;
}

