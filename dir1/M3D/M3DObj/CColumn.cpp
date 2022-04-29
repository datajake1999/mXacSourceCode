/**********************************************************************************
CColumn.cpp - Draw columns

begun 14/3/2002 by Mike Rozak
Copyright 2002 Mike Rozak. All rights reserved
*/

#include <windows.h>
#include "escarpment.h"
#include "..\M3D.h"
#include "resource.h"


/******************************************************************************
CColumn::Constructor and destructor */
CColumn::CColumn (void)
{
   m_pStart.Zero();
   m_pEnd.Zero();
   m_pEnd.p[2] = 1;
   m_pFront.Zero();
   m_pFront.p[1] = -1;
   m_pSize.Zero();
   m_pSize.p[0] = m_pSize.p[1] = .075;
   m_dwShape = NS_RECTANGLE;
   memset (&m_biBottom, 0, sizeof(m_biBottom));
   memset (&m_biTop, 0, sizeof(m_biTop));
   memset (m_biBrace, 0, sizeof(m_biBrace));
   m_fBraceStart = 1;
   m_Matrix.Identity();

   m_fDirty = TRUE;
   m_pColumn = NULL;
   m_pBottom = NULL;
   m_pTop = NULL;
   memset (m_apBraces, 0, sizeof(m_apBraces));
}

CColumn::~CColumn (void)
{
   if (m_pColumn)
      delete m_pColumn;
   if (m_pBottom)
      delete m_pBottom;
   if (m_pTop)
      delete m_pTop;
   DWORD i;
   for (i = 0; i < COLUMNBRACES; i++)
      if (m_apBraces[i])
         delete m_apBraces[i];
}

static PWSTR gpszStart = L"Start";
static PWSTR gpszEnd = L"End";
static PWSTR gpszFront = L"Front";
static PWSTR gpszBraceStart = L"BraceStart";
static PWSTR gpszMatrix = L"Matrix";
static PWSTR gpszUse = L"Use";
static PWSTR gpszShape = L"Shape";
static PWSTR gpszTaper = L"Taper";
static PWSTR gpszBevelMode = L"BevelMode";
static PWSTR gpszBevelNorm = L"BevelNorm";
static PWSTR gpszSize = L"Size";
static PWSTR gpszCapped = L"Capped";

/******************************************************************************
CColumn::MMLTo - Writes the Column to a MML Node.

inputs
   none
returns
   PCMMLNode2 - New node. Must be feed by the caller. Returns NULL if error
*/
PCMMLNode2 CColumn::MMLTo (void)
{
   PCMMLNode2 pNode = new CMMLNode2;
   if (!pNode)
      return NULL;

   MMLValueSet (pNode, gpszStart, &m_pStart);
   MMLValueSet (pNode, gpszEnd, &m_pEnd);
   MMLValueSet (pNode, gpszFront, &m_pFront);
   MMLValueSet (pNode, gpszSize, &m_pSize);
   MMLValueSet (pNode, gpszShape, (int) m_dwShape);
   MMLValueSet (pNode, gpszBraceStart, m_fBraceStart);
   MMLValueSet (pNode, gpszMatrix, &m_Matrix);

   DWORD i;
   PCMMLNode2 pSub;
   WCHAR szTemp[64];
   for (i = 0; i < 2; i++) {
      PCBASEINFO pi = i ? &m_biBottom : &m_biTop;
      pSub = new CMMLNode2;
      if (!pSub) {
         delete pNode;
         return NULL;
      }

      swprintf (szTemp, L"EndCap%d", i);
      pSub->NameSet (szTemp);
      pNode->ContentAdd (pSub);


      // write values
      MMLValueSet (pSub, gpszUse, (int) pi->fUse);
      if (pi->fUse) {
         MMLValueSet (pSub, gpszShape, (int) pi->dwShape);
         MMLValueSet (pSub, gpszTaper, (int) pi->dwTaper);
         MMLValueSet (pSub, gpszSize, &pi->pSize);
      }
      MMLValueSet (pSub, gpszBevelMode, (int) pi->dwBevelMode);
      if (pi->dwBevelMode)
         MMLValueSet (pSub, gpszBevelNorm, &pi->pBevelNorm);
      MMLValueSet (pSub, gpszCapped, (int) pi->fCapped);
   }

   for (i = 0; i < COLUMNBRACES; i++) {
      PCBRACEINFO pi = &m_biBrace[i];
      if (!pi->fUse)
         continue;   // dont write out if no brace

      pSub = new CMMLNode2;
      if (!pSub) {
         delete pNode;
         return NULL;
      }

      swprintf (szTemp, L"BraceInfo%d", i);
      pSub->NameSet (szTemp);
      pNode->ContentAdd (pSub);

      MMLValueSet (pSub, gpszUse, (int) pi->fUse);
      MMLValueSet (pSub, gpszShape, (int) pi->dwShape);
      MMLValueSet (pSub, gpszSize, &pi->pSize);
      MMLValueSet (pSub, gpszEnd, &pi->pEnd);
      MMLValueSet (pSub, gpszBevelMode, (int) pi->dwBevelMode);
      if (pi->dwBevelMode)
         MMLValueSet (pSub, gpszBevelNorm, &pi->pBevelNorm);
   }

   return pNode;
}

/******************************************************************************
CColumn::MMLFrom - Reads the information from MML.

inputs
   PCMMLNode2      pNode - Node to read from
returns
   BOOL - TRUE if success
*/
BOOL CColumn::MMLFrom (PCMMLNode2 pNode)
{
   m_fDirty = TRUE;
   memset (m_biBrace, 0, sizeof(m_biBrace));

   CPoint pZero;
   pZero.Zero();
   CMatrix mZero;
   mZero.Identity();
   MMLValueGetPoint (pNode, gpszStart, &m_pStart, &pZero);
   MMLValueGetPoint (pNode, gpszEnd, &m_pEnd, &pZero);
   MMLValueGetPoint (pNode, gpszFront, &m_pFront, &pZero);
   MMLValueGetPoint (pNode, gpszSize, &m_pSize, &pZero);
   m_dwShape = (DWORD)MMLValueGetInt (pNode, gpszShape, NS_RECTANGLE);
   m_fBraceStart = MMLValueGetDouble (pNode, gpszBraceStart, 1);
   MMLValueGetMatrix (pNode, gpszMatrix, &m_Matrix, &mZero);

   DWORD i;
   PCMMLNode2 pSub;
   PWSTR psz;
   for (i = 0; i < pNode->ContentNum(); i++) {
      pSub = NULL;
      pNode->ContentEnum (i, &psz, &pSub);
      if (!pSub)
         continue;
      psz = pSub->NameGet();
      if (!psz)
         continue;

      PWSTR pszEndCap = L"EndCap";
      PWSTR pszBrace = L"BraceInfo";

      if (!wcsncmp (psz, pszEndCap, wcslen(pszEndCap))) {
         DWORD dwID = _wtoi (psz + wcslen(pszEndCap));
         if (dwID > 1)
            continue;

         PCBASEINFO pi;
         pi = dwID ? &m_biBottom : &m_biTop;
         pi->fUse = (BOOL) MMLValueGetInt (pSub, gpszUse, (int) FALSE);
         if (pi->fUse) {
            pi->dwShape = (DWORD) MMLValueGetInt (pSub, gpszShape, 0);
            pi->dwTaper = (DWORD) MMLValueGetInt (pSub, gpszTaper, 0);
            MMLValueGetPoint (pSub, gpszSize, &pi->pSize, &pZero);
         }
         pi->dwBevelMode = (DWORD) MMLValueGetInt (pSub, gpszBevelMode, 0);
         if (pi->dwBevelMode)
            MMLValueGetPoint (pSub, gpszBevelNorm, &pi->pBevelNorm, &pZero);
         pi->fCapped = (BOOL) MMLValueGetInt (pSub, gpszCapped, 0);

      }
      else if (!wcsncmp (psz, pszBrace, wcslen(pszBrace))) {
         DWORD dwID = _wtoi (psz + wcslen(pszBrace));
         if (dwID >= COLUMNBRACES)
            continue;
         PCBRACEINFO pi;
         pi = &m_biBrace[dwID];

         pi->fUse = (BOOL) MMLValueGetInt (pSub, gpszUse, 0);
         pi->dwShape = (DWORD) MMLValueGetInt (pSub, gpszShape, 0);
         MMLValueGetPoint (pSub, gpszSize, &pi->pSize, &pZero);
         MMLValueGetPoint (pSub, gpszEnd, &pi->pEnd, &pZero);
         pi->dwBevelMode = (DWORD) MMLValueGetInt (pSub, gpszBevelMode, 0);
         if (pi->dwBevelMode)
            MMLValueGetPoint (pSub, gpszBevelNorm, &pi->pBevelNorm, &pZero);
      }
   }


   return TRUE;
}

/******************************************************************************
CColumn::Clone - Clones the object into a new one.

returns
   PCColumn - New column object to be freed by the caller. NULL if error
*/
CColumn *CColumn::Clone (void)
{
   PCColumn pNew = new CColumn;
   if (!pNew)
      return NULL;

   if (!CloneTo (pNew)) {
      delete pNew;
      return NULL;
   }

   return pNew;
}

/******************************************************************************
CColumn::CloneTo - Copies over all the relevent member variables of this column to the new
one,

inputs
   PCColumn    pNew - New column
returns
   BOOL - TRUE if succes
*/
BOOL CColumn::CloneTo (CColumn *pNew)
{
   pNew->m_fDirty = TRUE;

   pNew->m_pStart.Copy (&m_pStart);
   pNew->m_pEnd.Copy (&m_pEnd);
   pNew->m_pSize.Copy (&m_pSize);
   pNew->m_dwShape = m_dwShape;
   pNew->m_pFront.Copy (&m_pFront);
   pNew->m_biBottom = m_biBottom;
   pNew->m_biTop = m_biTop;
   memcpy (pNew->m_biBrace, m_biBrace, sizeof(m_biBrace));
   pNew->m_fBraceStart = m_fBraceStart;
   pNew->m_Matrix.Copy (&m_Matrix);

   return TRUE;
}

/******************************************************************************
CColumn::StartEndFrontSet - Sets the columns start and end points, and a vector
pointing towards the front.

inputs
   PCPoint     pStart - Start location. Can be NULL
   PCPoint     pEnd - End location. Can be NULL
   PCPoint     pFront - Front vector. Can be NULL. Doesn't need to be normalized
returns
   BOOL - TRUE if success
*/
BOOL CColumn::StartEndFrontSet (PCPoint pStart, PCPoint pEnd, PCPoint pFront)
{
   if (pStart)
      m_pStart.Copy (pStart);
   if (pEnd)
      m_pEnd.Copy (pEnd);
   if (pFront)
      m_pFront.Copy (pFront);
   m_fDirty = TRUE;
   return TRUE;
}

/******************************************************************************
CColumn::StartEndFrontGet - Gets the columns start and end points, and a vector
pointing towards the front.

inputs
   PCPoint     pStart - Start location. Can be NULL
   PCPoint     pEnd - End location. Can be NULL
   PCPoint     pFront - Front vector. Can be NULL. Not necessarily normalized
returns
   BOOL - TRUE if success
*/
BOOL CColumn::StartEndFrontGet (PCPoint pStart, PCPoint pEnd, PCPoint pFront)
{
   if (pStart)
      pStart->Copy (&m_pStart);
   if (pEnd)
      pEnd->Copy (&m_pEnd);
   if (pFront)
      pFront->Copy (&m_pFront);
   return TRUE;
}

/******************************************************************************
CColumn::Sets the size of the column.

inputs
   PCPoint     pSize - pSize.p[0] is the width, and .p[1] is the depth
returns
   BOOL - TRUE if success
*/
BOOL CColumn::SizeSet (PCPoint pSize)
{
   m_pSize.Copy (pSize);
   m_fDirty = TRUE;
   return TRUE;
}

/******************************************************************************
CColumn::Gets the size of the column.

inputs
   PCPoint     pSize - pSize.p[0] is the width, and .p[1] is the depth
returns
   BOOL - TRUE if success
*/
BOOL CColumn::SizeGet (PCPoint pSize)
{
   pSize->Copy (&m_pSize);
   return TRUE;
}

/******************************************************************************
CColumn::ShapeSet - Sets the shape of the column.

inputs
   DWORD       dwShape - One of NS_XXX, except NS_CUSTOM
returns
   BOOL - TRUE if success
*/
BOOL CColumn::ShapeSet (DWORD dwShape)
{
   m_dwShape = dwShape;
   m_fDirty = TRUE;
   return TRUE;
}

/******************************************************************************
CColumn::ShapeGet - Gets the shape of the column.

inputs
   PCPoint     pSize - pSize.p[0] is the width, and .p[1] is the depth
returns
   BOOL - TRUE if success
*/
DWORD CColumn::ShapeGet (void)
{
   return m_dwShape;
}
/******************************************************************************
CColumn:: BaseInfoSet - Sets the information about the base or top of the column.

inputs
   BOOL        fBottom - If TRUE then ask about the bottom, FALSE about the top
   PCBASEINFO  pInfo - New information to use about the base
returns
   BOOL - TRUE if success
*/
BOOL CColumn::BaseInfoSet (BOOL fBottom, PCBASEINFO pInfo)
{
   if (fBottom)
      m_biBottom = *pInfo;
   else
      m_biTop = *pInfo;
   m_fDirty = TRUE;
   return TRUE;
}

/******************************************************************************
CColumn:: BaseInfoGet - Gets the information about the base or top of the column.

inputs
   BOOL        fBottom - If TRUE then ask about the bottom, FALSE about the top
   PCBASEINFO  pInfo - Filled in with info about the top/bttom
returns
   BOOL - TRUE if success
*/
BOOL CColumn::BaseInfoGet (BOOL fBottom, PCBASEINFO pInfo)
{
   if (fBottom)
      *pInfo = m_biBottom;
   else
      *pInfo = m_biTop;
   return TRUE;
}

/******************************************************************************
CColumn::BraceInfoSet - Sets information about one of the braces.

inputs
   DWORD       dwBrace - Brace, from 0 .. COLUMNBRACES-1
   PCBRACEINFO pInfo - New information for the brace
returns
   BOOL - TRUE if success
*/
BOOL CColumn::BraceInfoSet (DWORD dwBrace, PCBRACEINFO pInfo)
{
   if (dwBrace >= COLUMNBRACES)
      return FALSE;

   m_biBrace[dwBrace] = *pInfo;
   m_fDirty = TRUE;
   return TRUE;
}

/******************************************************************************
CColumn::BraceInfoGet - Gets information about one of the braces.

inputs
   DWORD       dwBrace - Brace, from 0 .. COLUMNBRACES-1
   PCBRACEINFO pInfo - Filled with the brace info.
returns
   BOOL - TRUE if success
*/
BOOL CColumn::BraceInfoGet (DWORD dwBrace, PCBRACEINFO pInfo)
{
   if (dwBrace >= COLUMNBRACES)
      return FALSE;

   *pInfo = m_biBrace[dwBrace];
   return TRUE;
}


/******************************************************************************
CColumn::BraceStartSet - Sets the starting point of the brace, as a distance
(in meters) from the top of the column.

inputs
   fp      fDistance - New distance
returns
   BOOL - TRUE if success
*/
BOOL CColumn::BraceStartSet (fp fDistance)
{
   m_fBraceStart = fDistance;
   m_fDirty = TRUE;
   return TRUE;
}

/******************************************************************************
CColumn::BraceStartGet - Returns the value passed into BraceStartSet ()

inputs
   noen
returens
   fp - Distance from top of column
*/
fp CColumn::BraceStartGet (void)
{
   return m_fBraceStart;
}

/******************************************************************************
CColumn::MatrixSet - Sets the rotation matrix for the whole object.

inputs
   PCMatrix          pm
returns 
   BOOL - TRUE if success
*/
BOOL CColumn::MatrixSet (PCMatrix pm)
{
   m_Matrix.Copy (pm);
   m_fDirty = TRUE;
   return TRUE;
}

/******************************************************************************
CColumn::MatrixSet - Gets the matrix for the whole object.

inputs
   PCMatrix       pm - Filled with the matri
returns
   BOOL - TRUE if success
*/
BOOL CColumn::MatrixGet (PCMatrix pm)
{
   pm->Copy (&m_Matrix);
   return TRUE;
}



/******************************************************************************
CColumn::Render - Draws the column.

inputs
   POBJECTRENDER     pr - Callbacks for drawing
   PCRenderSurface   prs - Draw to this. The default surface is the one used for darwing
   DWORD             dwElements - Set of bit flags for which elements are drawn.
                           Use this to draw the base with a different color/texture than the column, etc.
                           A combination of the following:
                              0x01 - Column
                              0x02 - Base
                              0x04 - Top
                              0x08 - Braces
returns
   BOOL - TRUE if succes
*/
BOOL CColumn::Render (POBJECTRENDER pr, PCRenderSurface prs, DWORD dwElements)
{
   // Make sure to call function to rebuild if dirty
   if (!BuildNoodles ())
      return FALSE;

   if ((dwElements & 0x01) && m_pColumn)
      if (!m_pColumn->Render (pr, prs))
         return FALSE;
   if ((dwElements & 0x02) && m_pBottom)
      if (!m_pBottom->Render (pr, prs))
         return FALSE;
   if ((dwElements & 0x04) && m_pTop)
      if (!m_pTop->Render (pr, prs))
         return FALSE;
   if (dwElements & 0x08) {
      DWORD i;
      for (i = 0; i < COLUMNBRACES; i++)
         if (m_apBraces[i])
            if (!m_apBraces[i]->Render (pr, prs))
               return FALSE;
   }

   return TRUE;
}



/**********************************************************************************
CColumn::QueryBoundingBox - Standard API
*/
void CColumn::QueryBoundingBox (PCPoint pCorner1, PCPoint pCorner2, DWORD dwElements)
{
   CPoint p1, p2;
   BOOL fSet = FALSE;

   // Make sure to call function to rebuild if dirty
   if (!BuildNoodles ()) {
      pCorner1->Zero();
      pCorner2->Zero();
      return;
   }

   if ((dwElements & 0x01) && m_pColumn) {
      m_pColumn->QueryBoundingBox (&p1, &p2);
      if (fSet) {
         pCorner1->Min (&p1);
         pCorner2->Max (&p2);
      }
      else {
         pCorner1->Copy (&p1);
         pCorner2->Copy (&p2);
         fSet = TRUE;
      }
   }

   if ((dwElements & 0x02) && m_pBottom) {
      m_pBottom->QueryBoundingBox (&p1, &p2);
      if (fSet) {
         pCorner1->Min (&p1);
         pCorner2->Max (&p2);
      }
      else {
         pCorner1->Copy (&p1);
         pCorner2->Copy (&p2);
         fSet = TRUE;
      }
   }

   if ((dwElements & 0x04) && m_pTop) {
      m_pTop->QueryBoundingBox (&p1, &p2);
      if (fSet) {
         pCorner1->Min (&p1);
         pCorner2->Max (&p2);
      }
      else {
         pCorner1->Copy (&p1);
         pCorner2->Copy (&p2);
         fSet = TRUE;
      }
   }

   if (dwElements & 0x08) {
      DWORD i;
      for (i = 0; i < COLUMNBRACES; i++)
         if (m_apBraces[i]) {
            m_apBraces[i]->QueryBoundingBox (&p1, &p2);
            if (fSet) {
               pCorner1->Min (&p1);
               pCorner2->Max (&p2);
            }
            else {
               pCorner1->Copy (&p1);
               pCorner2->Copy (&p2);
               fSet = TRUE;
            }
         }
   }
}

/***************************************************************************************
Taper - Fills in a list of points with the tapering information.

inputs
   PCPoint     pBottom - Bottom width p[0] and depth, p[1].
   PCPoint     pTop - Top width, p[0], and depth, p[1]
   DWORD       dwTaper - One of CT_XXX
   BOOL        fFlip - If TRUE, flip the direction
   PCPoint     paFill - Fill taper into here
   DWORD       dwNum - Maximum number of paFill
   DWORD       *pdwCurve - Filled with the type of curve
returns
   DWORD - Number of point for the taper. Or 0 is should skip taper
*/
static DWORD Taper (PCPoint pBottom, PCPoint pTop, DWORD dwTaper, BOOL fFlip,
                    PCPoint paFill, DWORD dwNum, DWORD *pdwCurve)
{
   DWORD iRet = 0;
   *pdwCurve = SEGCURVE_LINEAR;

   switch (dwTaper) {
   case CT_FULLTAPER:
      if (dwNum < 2)
         return 0;
      paFill[0].Copy (pBottom);
      paFill[1].Copy (pTop);
      iRet = 2;
      break;

   case CT_SPIKE:
      if (dwNum < 2)
         return 0;
      paFill[0].Copy (pBottom);
      paFill[0].Scale (.1);
      paFill[1].Copy (pBottom);
      iRet = 2;
      break;

   case CT_SPIKE2:
      if (dwNum < 2)
         return 0;
      paFill[0].Copy (pBottom);
      paFill[1].Copy (pTop);
      iRet = 2;
      break;

   case CT_HALFTAPER:
      if (dwNum < 2)
         return 0;
      paFill[0].Copy (pBottom);
      paFill[1].Average (pBottom, pTop);
      iRet = 2;
      break;

   case CT_UPANDTAPER:
      if (dwNum < 3)
         return 0;
      paFill[0].Copy (pBottom);
      paFill[1].Copy (pBottom);
      paFill[2].Copy (pTop);
      iRet = 3;
      break;

   case CT_SPHERE:
      if (dwNum < 3)
         return 0;
      paFill[0].Copy (pBottom);
      paFill[0].Scale (.3);
      paFill[1].Copy (pBottom);
      paFill[2].Copy (pTop);
      paFill[2].Scale (.3);
      *pdwCurve = SEGCURVE_CIRCLENEXT;
      iRet = 3;
      break;

   case CT_DIAMOND:
      if (dwNum < 3)
         return 0;
      paFill[0].Copy (pBottom);
      paFill[0].Scale (.1);
      paFill[1].Copy (pBottom);
      paFill[2].Copy (pTop);
      paFill[2].Scale (.1);
      iRet = 3;
      break;

   case CT_CAPPED:
      if (dwNum < 3)
         return 0;
      paFill[0].Copy (pBottom);
      paFill[0].Scale (.1);
      paFill[1].Copy (pBottom);
      paFill[2].Copy (pBottom);
      iRet = 3;
      break;

   case CT_NONE:
   default:
      return 0;
   }

   if (fFlip) {
      DWORD i;
      CPoint pt;
      for (i = 0; i < iRet/2; i++) {
         pt.Copy (&paFill[i]);
         paFill[i].Copy (&paFill[iRet - i - 1]);
         paFill[iRet-i-1].Copy (&pt);
      }
   }

   return iRet;
}

/***************************************************************************************
CColumn::BuildNoodles - If the object has the dirty flag set then builds all the noodle
objects.
*/
BOOL CColumn::BuildNoodles (void)
{
   if (!m_fDirty)
      return TRUE;

   // create main column
   if (!m_pColumn)
      m_pColumn = new CNoodle;
   if (!m_pColumn)
      return NULL;
   CPoint pDir;
   fp fLen;
   pDir.Subtract (&m_pEnd, &m_pStart);
   fLen = pDir.Length();
   pDir.Normalize();

   // where it really starts and ends
   CPoint pRealStart, pRealEnd;
   if (m_biBottom.fUse) {
      pRealStart.Copy (&pDir);
      pRealStart.Scale (m_biBottom.pSize.p[2]);
      pRealStart.Add (&m_pStart);
   }
   else
      pRealStart.Copy (&m_pStart);
   if (m_biTop.fUse) {
      pRealEnd.Copy (&pDir);
      pRealEnd.Scale (-m_biTop.pSize.p[2]);
      pRealEnd.Add (&m_pEnd);
   }
   else
      pRealEnd.Copy (&m_pEnd);

   // set params
   m_pColumn->BackfaceCullSet (TRUE);
   m_pColumn->FrontVector (&m_pFront);
   m_pColumn->MatrixSet (&m_Matrix);
   m_pColumn->PathLinear (&pRealStart, &pRealEnd);
   m_pColumn->ScaleVector (&m_pSize);
   m_pColumn->ShapeDefault ((m_dwShape != NS_CUSTOM) ? m_dwShape : NS_RECTANGLE);
   // BUGFIX - Was only capping if no bottom/top - instead, cap if request on bottom or top
   // since if dont do, causes problems when put a ball (or something) on top of a square
   // post and can see the main column
   m_pColumn->DrawEndsSet (m_biBottom.fCapped || m_biTop.fCapped);
   m_pColumn->BevelSet (TRUE, m_biBottom.fUse ? 0 : m_biBottom.dwBevelMode, &m_biBottom.pBevelNorm);
   m_pColumn->BevelSet (FALSE, m_biTop.fUse ? 0 : m_biTop.dwBevelMode, &m_biTop.pBevelNorm);

   // bottom
#define FILLNUM      10
   CPoint   apFill[FILLNUM];
   DWORD dwNum;
   DWORD dwCurve;
   if (m_biBottom.fUse) {
      if (!m_pBottom)
         m_pBottom = new CNoodle;
      if (!m_pBottom)
         return FALSE;
      m_pBottom->BackfaceCullSet (TRUE);
      m_pBottom->FrontVector (&m_pFront);
      m_pBottom->MatrixSet (&m_Matrix);
      m_pBottom->PathLinear (&m_pStart, &pRealStart);
      m_pBottom->ShapeDefault ((m_biBottom.dwShape != NS_CUSTOM) ? m_biBottom.dwShape : NS_RECTANGLE);
      m_pBottom->DrawEndsSet (TRUE);   // always draw ends because at least on end exposed
      m_pBottom->BevelSet (TRUE, m_biBottom.dwBevelMode, &m_biBottom.pBevelNorm);
      m_pBottom->BevelSet (FALSE, 0, &m_biBottom.pBevelNorm);

      // do the tapering
      dwNum = Taper (&m_biBottom.pSize, &m_pSize, m_biBottom.dwTaper, FALSE, apFill, FILLNUM, &dwCurve);
      if (dwNum)
         m_pBottom->ScaleSpline (dwNum, apFill, (DWORD*)(size_t)dwCurve, (dwCurve == SEGCURVE_LINEAR) ? 0 : 2);
      else
         m_pBottom->ScaleVector (&m_biBottom.pSize);
   }
   else {
      if (m_pBottom)
         delete m_pBottom;
      m_pBottom = NULL;
   }

   // Top
   if (m_biTop.fUse) {
      if (!m_pTop)
         m_pTop = new CNoodle;
      if (!m_pTop)
         return FALSE;
      m_pTop->BackfaceCullSet (TRUE);
      m_pTop->FrontVector (&m_pFront);
      m_pTop->MatrixSet (&m_Matrix);
      m_pTop->PathLinear (&pRealEnd, &m_pEnd);
      m_pTop->ScaleVector (&m_biTop.pSize);
      m_pTop->ShapeDefault ((m_biTop.dwShape != NS_CUSTOM) ? m_biTop.dwShape : NS_RECTANGLE);
      m_pTop->DrawEndsSet (TRUE);   // always draw ends because at least on end exposed
      m_pTop->BevelSet (FALSE, m_biTop.dwBevelMode, &m_biTop.pBevelNorm);
      m_pTop->BevelSet (TRUE, 0, &m_biTop.pBevelNorm);

      dwNum = Taper (&m_biTop.pSize, &m_pSize, m_biTop.dwTaper, TRUE, apFill, FILLNUM, &dwCurve);
      if (dwNum)
         m_pTop->ScaleSpline (dwNum, apFill, (DWORD*)(size_t)dwCurve, (dwCurve == SEGCURVE_LINEAR) ? 0 : 2);
      else
         m_pTop->ScaleVector (&m_biTop.pSize);
   }
   else {
      if (m_pTop)
         delete m_pTop;
      m_pTop = NULL;
   }

   // where brace begins and ends
   CPoint pBrace;
   pBrace.Copy (&pDir);
   pBrace.Scale (-m_fBraceStart);
   pBrace.Add (&m_pEnd);

   // Draw braces
   DWORD i;
   for (i = 0; i < COLUMNBRACES; i++) {
      PCBRACEINFO pb = &m_biBrace[i];
      if (!pb->fUse) {
         if (m_apBraces[i])
            delete m_apBraces[i];
         m_apBraces[i] = NULL;
         continue;
      }

      if (!m_apBraces[i])
         m_apBraces[i] = new CNoodle;
      if (!m_apBraces[i])
         return FALSE;

      // draw
      m_apBraces[i]->BackfaceCullSet (TRUE);
      m_apBraces[i]->MatrixSet (&m_Matrix);
      m_apBraces[i]->PathLinear (&pBrace, &pb->pEnd);
      m_apBraces[i]->ScaleVector (&pb->pSize);
      m_apBraces[i]->ShapeDefault ((pb->dwShape != NS_CUSTOM) ? pb->dwShape : NS_RECTANGLE);
      m_apBraces[i]->DrawEndsSet (FALSE);
      m_apBraces[i]->BevelSet (FALSE, pb->dwBevelMode, &pb->pBevelNorm);

      // calculate the intersection of the brace with the column
      CPoint pBraceDir, n1, n2;
      pBraceDir.Subtract (&pb->pEnd, &pBrace);
      n1.CrossProd (&pBraceDir, &pDir);
      n2.CrossProd (&n1, &pDir);
      m_apBraces[i]->BevelSet (TRUE, 2, &n2);

      // use this for the front
      n1.Normalize();
      if (n1.Length() < .5)
         n1.Copy (&m_pFront);
      m_apBraces[i]->FrontVector (&n1);

   }

   // set dirty flag
   m_fDirty = FALSE;
   return TRUE;
}


// FUTURERELEASE - May want more varieties of CT_XXX
