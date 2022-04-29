/**********************************************************************************
CDoubleSurface - This C++ object has two CSplineSurface object, and draws a fp-sided
surface (with edges). It manages everything too. Essentially, it's a segment of
wall, roof, floor, or whatever.

begun 21/12/2001 by Mike Rozak
Copyright 2001 Mike Rozak. All rights reserved
*/

#include <windows.h>
#include "escarpment.h"
#include "..\M3D.h"
#include "resource.h"

// SOMS - SurfaceOverMain struct
typedef struct {
   PCListFixed             plSeq;      // sequences that intersect the roof
   PCDoubleSurface         pThis;      // this
   BOOL                    fSideA;     // true if it's side A
   PCListFixed             plSelected; // list of selected segments
   WCHAR                   szEditing[256];   // name of overlay that editing
   DWORD                   *pdwCurColor;  // change this to change the current colored displayed
                                          // set to &m_dwDisplayControl
} SOMS, *PSOMS;

// FNOODLE - Framing noodle
typedef struct {
   PCNoodle                pNoodle;    // draws the frame
   DWORD                   dwColor;    // color, 0..2
} FNOODLE, *PFNOODLE;

/************************************************************************************
OverlayShapeNameToColor - Looks to the number after the colon, and returns that.
*/
DWORD OverlayShapeNameToColor (PWSTR pszName)
{
   PWSTR pszColon;
   pszColon = wcschr (pszName, L':');
   if (!pszColon)
      return -1;
   return _wtoi(pszColon+1);
}

#if 0

/************************************************************************************
OverlayShapeShapeFindByColor - Given a color (also used for the current control
points displayed), find the overlay information.

inputs
   PCObjectStructSurface pObj - Object
   DWORD       dwColor - color
   PWSTR       *ppszName - Filled with the name
   PTEXTUREPOINT *pptp - Filled with the pointer to teh texture points
   DWORD       *pdwNum - Filled with the number
   BOOL        *pfClockwise - Filled with TRUE if clockwise
returns
   PCSplineSurface - Spline. NULL if can't find
*/
PCSplineSurface OverlayShapeFindByColor (CObjectStructSurface *pObj, DWORD dwColor, PWSTR *ppszName, PTEXTUREPOINT *pptp,
                                         DWORD *pdwNum, BOOL *pfClockwise)
{
   return pObj->m_ds.OverlayShapeFindByColor (dwColor, ppszName, pptp, pdwNum, pfClockwise);

}
#endif // 0


/***********************************************************************************
OverlayShapeFromPoints - Given an array of TEXTUREPOINTs, this will fill in
a pCenter, fWidth, and fHeight. These same numbers can be passed into OverlayShapeToPoints()
to reproduce the same shape - except if it's type 8, which is arbitrary shape.

inputs
   DWORD          dwID - ID for shape - from OverlayShapeNameToID
   PTEXTUREPOINT  ptp - Texturepoints
   DWORD          dwNum - Number of points
   PCPoint        pCenter - Filled in with the center
   fp         *pfWidth, *pfHeight - Filled in with width and height
returns
   BOOL - TRUE if success
*/
BOOL OverlayShapeFromPoints (DWORD dwID, PTEXTUREPOINT ptp, DWORD dwNum,
                             PCPoint pCenter, fp *pfWidth, fp *pfHeight)
{
   DWORD i, j;
   CPoint pMin, pMax, pTemp;

   // just default to pcenter in the center of the object
   pCenter->Zero();
   pCenter->p[0] = .5;
   pCenter->p[1] = .5;
   *pfWidth = *pfHeight = .1;
   switch (dwID) {
   case 0:  // rectangle
      if (dwNum != 4)
         return FALSE;
      pCenter->p[0] = (ptp[0].h + ptp[1].h) / 2;
      pCenter->p[1] = (ptp[0].v + ptp[2].v) / 2;
      *pfWidth = (pCenter->p[0] - ptp[0].h)*2;
      *pfHeight = (pCenter->p[1] - ptp[0].v)*2;
      break;
   case 1:  // ellipse
      pMin.Zero();
      pMax.Zero();
      pTemp.Zero();
      for (i = 0; i < dwNum; i++) {
         pTemp.p[0] = ptp[i].h;
         pTemp.p[1] = ptp[i].v;

         if (!i) {
            pMin.Copy(&pTemp);
            pMax.Copy (&pTemp);
            continue;
         }
         for (j = 0; j < 2; j++) {
            pMin.p[j] = min(pMin.p[j], pTemp.p[j]);
            pMax.p[j] = max(pMax.p[j], pTemp.p[j]);
         }
      }
      pCenter->Add (&pMin, &pMax);
      pCenter->Scale (.5);
      *pfWidth = pMax.p[0] - pMin.p[0];
      *pfHeight = pMax.p[1] - pMin.p[1];
      break;
   case 2:  // below
   case 3:  // above
      if (dwNum != 4)
         return FALSE;
      pCenter->p[1] = ptp[0].v;
      break;
   case 4:  // left
   case 5:  // right
      if (dwNum != 4)
         return FALSE;
      pCenter->p[0] = ptp[0].h;
      break;
   case 6:  // horizontal band
      if (dwNum != 4)
         return FALSE;
      pCenter->p[1] = (ptp[0].v + ptp[2].v) / 2;
      *pfHeight = (pCenter->p[1] - ptp[0].v)*2;
      break;
   case 7:  // vertical band
      if (dwNum != 4)
         return FALSE;
      pCenter->p[0] = (ptp[0].h + ptp[1].h) / 2;
      *pfWidth = (pCenter->p[0] - ptp[0].h)*2;
      break;
   case 8:  // any shape
   default:
      return FALSE;
   }

   return TRUE;
}

/************************************************************************************
OverlayShapeToPoints - Given a name for an overlay shape, along with center
and "width" and "height", this generates the points. It returns a PCListFixed
array of points.

inputs
   DWORD       dwID - ID of shaep -  from OverlayShapeNameToID
   PCPoint     pCent - Center point (usually)
   fp      fWidth, fHeight - Width and height (usually)
returns
   PCListFixed - list of TEXTUREPOINTs. Must be freed by caller.
*/
PCListFixed OverlayShapeToPoints (DWORD dwID, PCPoint pCent, fp fWidth,
                                  fp fHeight)
{
   // see which one is clicked
   CPoint pCenter;
   pCenter.Copy (pCent);
   CSpline spline;
   CPoint ap[8];
   memset (&ap, 0, sizeof(ap));
   DWORD dwNum;
   DWORD dwCurve = SEGCURVE_LINEAR;
   DWORD dwDivide = 0;
   switch (dwID) {
   case 0:  // rectangle
      ap[0].p[0] =ap[3].p[0] = -fWidth/2;
      ap[1].p[0] =ap[2].p[0] = fWidth/2;
      ap[0].p[1] =ap[1].p[1] = -fHeight/2;
      ap[2].p[1] =ap[3].p[1] = fHeight/2;
      dwNum = 4;
      break;
   case 1:  // ellipse
      ap[0].p[0] = -fWidth/2;
      ap[0].p[1] = 0;
      ap[1].p[0] = -fWidth/2;
      ap[1].p[1] = -fHeight/2;
      ap[2].p[0] = 0;
      ap[2].p[1] = -fHeight/2;
      ap[3].p[0] = fWidth/2;
      ap[3].p[1] = -fHeight/2;
      ap[4].p[0] = fWidth/2;
      ap[4].p[1] = 0;
      ap[5].p[0] = fWidth/2;
      ap[5].p[1] = fHeight/2;
      ap[6].p[0] = 0;
      ap[6].p[1] = fHeight/2;
      ap[7].p[0] = -fWidth/2;
      ap[7].p[1] = fHeight/2;
      dwNum = 8;
      dwCurve = SEGCURVE_ELLIPSENEXT;
      dwDivide = 2;
      break;
   case 2:  // below
      ap[0].p[0] = 0;
      ap[0].p[1] = pCenter.p[1];
      ap[1].p[0] = 1;
      ap[1].p[1] = pCenter.p[1];
      ap[2].p[0] = 1;
      ap[2].p[1] = 1;
      ap[3].p[0] = 0;
      ap[3].p[1] = 1;
      dwNum = 4;
      pCenter.Zero();
      break;
   case 3:  // above
      ap[0].p[0] = 1;
      ap[0].p[1] = pCenter.p[1];
      ap[1].p[0] = 0;
      ap[1].p[1] = pCenter.p[1];
      ap[2].p[0] = 0;
      ap[2].p[1] = 0;
      ap[3].p[0] = 1;
      ap[3].p[1] = 0;
      dwNum = 4;
      pCenter.Zero();
      break;
   case 4:  // left
      ap[0].p[0] = pCenter.p[0];
      ap[0].p[1] = 0;
      ap[1].p[0] = pCenter.p[0];
      ap[1].p[1] = 1;
      ap[2].p[0] = 0;
      ap[2].p[1] = 1;
      ap[3].p[0] = 0;
      ap[3].p[1] = 0;
      dwNum = 4;
      pCenter.Zero();
      break;
   case 5:  // right
      ap[0].p[0] = pCenter.p[0];
      ap[0].p[1] = 0;
      ap[1].p[0] = 1;
      ap[1].p[1] = 0;
      ap[2].p[0] = 1;
      ap[2].p[1] = 1;
      ap[3].p[0] = pCenter.p[0];
      ap[3].p[1] = 1;
      dwNum = 4;
      pCenter.Zero();
      break;
   case 6:  // horizontal band
      ap[0].p[0] = 0;
      ap[0].p[1] = pCenter.p[1] - fHeight/2;
      ap[1].p[0] = 1;
      ap[1].p[1] = pCenter.p[1] - fHeight/2;
      ap[2].p[0] = 1;
      ap[2].p[1] = pCenter.p[1] + fHeight/2;
      ap[3].p[0] = 0;
      ap[3].p[1] = pCenter.p[1] + fHeight/2;
      dwNum = 4;
      pCenter.Zero();
      break;
   case 7:  // vertical band
      ap[0].p[0] = pCenter.p[0] - fWidth/2;
      ap[0].p[1] = 0;
      ap[1].p[0] = pCenter.p[0] + fWidth/2;
      ap[1].p[1] = 0;
      ap[2].p[0] = pCenter.p[0] + fWidth/2;
      ap[2].p[1] = 1;
      ap[3].p[0] = pCenter.p[0] - fWidth/2;
      ap[3].p[1] = 1;
      dwNum = 4;
      pCenter.Zero();
      break;
   case 8:  // any shape
      ap[0].p[0] = -fWidth/2;
      ap[0].p[1] = fHeight/2;
      ap[1].p[0] = 0;
      ap[1].p[1] = -fHeight/2;
      ap[2].p[0] = fWidth/2;
      ap[2].p[1] = fHeight/2;
      dwNum = 3;
      break;
   }

   DWORD i;
   for (i = 0; i < dwNum; i++)
      ap[i].Add (&pCenter);

   spline.Init (TRUE, dwNum, &ap[0], NULL, (DWORD*) (size_t) dwCurve, dwDivide, dwDivide);

   // fill in the points
   PCListFixed pl;
   pl = new CListFixed;
   if (!pl)
      return NULL;
   pl->Init (sizeof(TEXTUREPOINT));
   TEXTUREPOINT tp;
   pl->Required (spline.QueryNodes());
   for (i = 0; i < spline.QueryNodes(); i++) {
      PCPoint p;
      p = spline.LocationGet (i);
      tp.h = p->p[0];
      tp.v = p->p[1];
      pl->Add (&tp);
   }

   return pl;
}




/************************************************************************************
OverlayShapeNameToID - Given an overlay shape name, returns an ID.

inputs
   PWSTR       pszName - name
returns
   DWORD - ID for the name
*/
PWSTR    gpapszOverlayNames[9] = {L"Rectangle", L"Ellipse", L"Below", L"Above", L"Left",
   L"Right", L"Horizontal band", L"Vertical band", L"Any shape"};

DWORD OverlayShapeNameToID (PWSTR pszName)
{
   DWORD dwID = 0xffffffff;

   // find a colon?
   PWSTR pszColon;
   pszColon = wcschr (pszName, L':');
   if (pszColon)
      *pszColon = 0;

   DWORD i;
   for (i = 0; i < sizeof(gpapszOverlayNames) / sizeof(PWSTR); i++)
      if (!_wcsicmp(pszName, gpapszOverlayNames[i])) {
         dwID = i;
         break;
      }

   // restore the colon
   if (pszColon)
      *pszColon = L':';

   return dwID;
}
/***********************************************************************************
CDoubleSurface::Constructor and destructor
*/
CDoubleSurface::CDoubleSurface (void)
{
   m_dwType= 0;
   m_fBevelLeft = m_fBevelRight = m_fBevelTop = m_fBevelBottom = 0;
   m_fThickStud = CM_THICKSTUDWALL;
   m_fThickA = m_fThickB = CM_THICKSHEETROCK;
   m_fHideSideA = m_fHideSideB = m_fHideEdges = FALSE;
   m_dwExtSideA = m_dwExtSideB = 0;
   m_fConstBottom = TRUE;
   m_fConstAbove = TRUE;
   m_fConstRectangle = TRUE;
   m_pTemplate = NULL;
   m_MatrixToObject.Identity();
   m_MatrixFromObject.Identity();
   m_listCLAIMSURFACE.Init (sizeof(CLAIMSURFACE));
   m_listFloorsA.Init (sizeof(fp));
   m_listFloorsB.Init (sizeof(fp));

   m_fFramingDirty = TRUE;
   m_fFramingRotate = FALSE;
   m_fFramingShowOnlyWhenVisible = TRUE;
   memset (m_afFramingShow, 0, sizeof(m_afFramingShow));
   memset (m_afFramingInset, 0 ,sizeof(m_afFramingInset));
   memset (m_afFramingExtThick, 0 ,sizeof(m_afFramingExtThick));
   memset (m_afFramingInsetTL, 0, sizeof(m_afFramingInsetTL));
   m_afFramingMinDist[0] = .9;
   m_afFramingMinDist[1] = m_afFramingMinDist[2] = m_afFramingMinDist[0] * 4;
   memset (m_afFramingBevel, 0, sizeof(m_afFramingBevel));
   memset (m_afFramingExtend, 0, sizeof(m_afFramingExtend));
   m_adwFramingShape[0] = m_adwFramingShape[1] = m_adwFramingShape[2] = NS_RECTANGLE;
   m_apFramingScale[0].Zero();
   m_apFramingScale[0].p[0] = .05;
   m_apFramingScale[0].p[1] = .1;
   m_apFramingScale[1].Copy (&m_apFramingScale[0]);
   m_apFramingScale[2].p[0] = .1;
   m_apFramingScale[2].p[1] = .2;
   m_lFNOODLE.Init (sizeof(FNOODLE));

   m_lDrawEdgeABack.Init (sizeof(CListFixed));
   m_lDrawEdgeBBack.Init (sizeof(CListFixed));

}

CDoubleSurface::~CDoubleSurface (void)
{
   // clear the framing list
   FramingClear();

   //ClaimClear(); - BUGFIX - Don't do this here because confuses things when
   // deleting master object. Instead, have master object call if specially
   // destroying only this surface.

   // free them up
   DWORD i;
   PCListFixed plf;
   for (i = 0; i < m_lDrawEdgeABack.Num(); i++) {
      plf = *((PCListFixed*) m_lDrawEdgeABack.Get(i));
      delete plf;
   }
   for (i = 0; i < m_lDrawEdgeBBack.Num(); i++) {
      plf = *((PCListFixed*) m_lDrawEdgeBBack.Get(i));
      delete plf;
   }
}


/*************************************************************************************
CDoubleSurface::ClaimClear - Clear all the claimed surfaces. This ends up
gettind rid of colors information and embedding information.

  
NOTE: An object that has a CDoubleSurface should call this if it's deleting
the furace but NOT itself. THis eliminates the claims for the surfaces

*/
void CDoubleSurface::ClaimClear (void)
{
   DWORD i;

   // free up all the current claim surfaces
   if (m_pTemplate) {
      for (i = 0; i < m_listCLAIMSURFACE.Num(); i++) {
         PCLAIMSURFACE p = (PCLAIMSURFACE) m_listCLAIMSURFACE.Get(i);
         m_pTemplate->ObjectSurfaceRemove (p->dwID);
         if (p->fAlsoEmbed)
            m_pTemplate->ContainerSurfaceRemove (p->dwID);
      }
   }
   m_listCLAIMSURFACE.Clear();
}

/*************************************************************************************
CDoubleSurface::ClaimCloneTo - Looks at all the claims used in this object. These
are copied into the new fp surface - keeping the same IDs. Which means that can
onlyu call ClaimCloneTo when cloning to a blank destination object.


inputs
   PCDoubleSurface      pCloneTo - cloning to this
   PCObjectTemplate     pTemplate - Template. THis is what really is changed.
                        New ObjectSurfaceAdd() and ContainerSurfaceAdd()
returns
   none
*/
void CDoubleSurface::ClaimCloneTo (CDoubleSurface *pCloneTo, PCObjectTemplate pTemplate)
{
   if (!m_pTemplate)
      return;

   DWORD i;
   for (i = 0; i < m_listCLAIMSURFACE.Num(); i++) {
      PCLAIMSURFACE p = (PCLAIMSURFACE) m_listCLAIMSURFACE.Get(i);
      pTemplate->ObjectSurfaceAdd (m_pTemplate->ObjectSurfaceFind(p->dwID));
      if (p->fAlsoEmbed)
         pTemplate->ContainerSurfaceAdd (p->dwID);
   }
}

/*************************************************************************************
CDoubleSurface::ClaimRemove - Remove a claim on a particular ID.

inputs
   DWORD       dwID - to remove the claim on
returns
   BOOL - TRUE if success
*/
BOOL CDoubleSurface::ClaimRemove (DWORD dwID)
{
   PCLAIMSURFACE p;
   DWORD i;

   for (i = 0; i < m_listCLAIMSURFACE.Num(); i++) {
      p = (PCLAIMSURFACE) m_listCLAIMSURFACE.Get(i);
      if (p->dwID == dwID)
         break;
   }
   if (i >= m_listCLAIMSURFACE.Num())
      return FALSE;

   // found it
   m_pTemplate->ObjectSurfaceRemove (dwID);
   if (p->fAlsoEmbed)
      m_pTemplate->ContainerSurfaceRemove (dwID);

   // delete it
   m_listCLAIMSURFACE.Remove (i);
   return TRUE;
}

/*************************************************************************************
CDoubleSurface::ClaimFindByReason - Given a dwReason, finds the first claim with that reason.

inputs
   DWORD       dwReason
returns
   PCLAIMSURFACE - Claim using the reason, or NULL if can't find
*/
PCLAIMSURFACE CDoubleSurface::ClaimFindByReason (DWORD dwReason)
{
   DWORD i;
   for (i = 0; i < m_listCLAIMSURFACE.Num(); i++) {
      PCLAIMSURFACE p = (PCLAIMSURFACE) m_listCLAIMSURFACE.Get(i);
      if (p->dwReason == dwReason)
         return p;
   }
   return NULL;
}

/*************************************************************************************
CDoubleSurface::ClaimFindByID - Given a claim ID, finds the first claim with that ID.

inputs
   DWORD       dwID
returns
   PCLAIMSURFACE - Claim using the ID, or NULL if can't find
*/
PCLAIMSURFACE CDoubleSurface::ClaimFindByID (DWORD dwID)
{
   DWORD i;
   for (i = 0; i < m_listCLAIMSURFACE.Num(); i++) {
      PCLAIMSURFACE p = (PCLAIMSURFACE) m_listCLAIMSURFACE.Get(i);
      if (p->dwID == dwID)
         return p;
   }
   return NULL;
}

/*************************************************************************************
CDoubleSurface::ClaimSurface - Loops through all the texture surfaces and finds one
that can claim as own. If so, claims it.

inputs
   DWORD       dwReason - Reason. See CLAIMSURFACE for definition of alues
   BOOL        fAlsoEmbed - If TRUE, also claim the embedding surface of the same
               ID while at it.
   COLORREF    cColor - Color to use if no texture is available.
   PWSTR       pszScheme - Scheme to use. If NULL then no scheme will be used.
   GUID        pgTextureCode - Texture code to use. If this is not NULL then its
                        assumed the surface will use a texture in preference to a color
   GUID        pgTextureSub - Sub-type of the texture in gpTextureCode.
   PTEXTUREMODS pTextureMods - Modifications to the basic texture. Can be NULL
   fp      *padTextureMatrix - 2x2 array of doubles for texture rotation matrix. Can be null
returns
   DWORD ID - 0 if error
*/
DWORD CDoubleSurface::ClaimSurface (DWORD dwReason, BOOL fAlsoEmbed, COLORREF cColor, DWORD dwMaterialID, PWSTR pszScheme,
      const GUID *pgTextureCode, const GUID *pgTextureSub,
      PTEXTUREMODS pTextureMods, fp *pafTextureMatrix)
{
   // loop through surfaces until find one
   DWORD i;
   for (i = 10; i < 512; i++)
      if (-1 == m_pTemplate->ObjectSurfaceFindIndex (i))
         break;
   if (i >= 512)
      return 0;

   // that's the one we want
   m_pTemplate->ObjectSurfaceAdd (i, cColor, dwMaterialID, pszScheme, pgTextureCode, pgTextureSub,
      pTextureMods, pafTextureMatrix);
   if (fAlsoEmbed)
      m_pTemplate->ContainerSurfaceAdd (i);

   // remember this
   CLAIMSURFACE cs;
   memset (&cs, 0, sizeof(cs));
   cs.dwID = i;
   cs.dwReason = dwReason;
   cs.fAlsoEmbed = fAlsoEmbed;
   m_listCLAIMSURFACE.Add (&cs);

   return i;
}


/*************************************************************************************
CDoubleSurface::InitButDontCreate - Initializes the object half way, but doesn't
actually create surfaces - which is ultimately causing problems in CObjectBuildBlock.
Basically, this just remembers the template and dwType and that's it.
*/
BOOL CDoubleSurface::InitButDontCreate (DWORD dwType, PCObjectTemplate pTemplate)
{
   m_dwType = dwType;
   m_pTemplate = pTemplate;
   return TRUE;
}

/*************************************************************************************
CDoubleSurface::Init - Initialize

inputs
   DWORD       dwType - Type of surface.

HIWORD(pParams) is the major object type, and LOWORD(pParams) is the minor type...
0x1000 - Wall
   0x0001 - Internal
   0x0002 - Extneral
   0x0003 - Internal curved
   0x0004 - External curved
   0x0005 - Internal cement block
   0x0006 - External cement block
   0x0007 - Internal cement block, curved
   0x0008 - External cement block, curved
   0x000A - External, Based on global wall settings
   0x000C - Skirting around piers of house
   0x000D - Cabinet wall

0x2000 - Roof/ceiling
   0x0001 - Flat
   0x0002 - Curved, full arc
   0x0003 - Curved, half arc

0x3000 - Floor/ceiling
   0x0001 - Slab - with tiles
   0x0002 - Floor
   0x0003 - Drop ceiling
   0x0004 - Slab - concrete
   0x0005 - Decking
   0x0006 - Cabinet countertop
   0x0007 - Cabinet shelf


   PCObjectTemplate     pTemplate - Used for information like getting surfaces available.

returns
   BOOL - TRUE if success. FALSE if failure
*/
BOOL CDoubleSurface::Init (DWORD dwRenderShard, DWORD dwType, PCObjectTemplate pTemplate)
{
   InitButDontCreate (dwType, pTemplate);

   // get global flag for type of wall
   PWSTR psz;
   DWORD dwExternalWall;
   PCWorldSocket pWorld = WorldGet (dwRenderShard, NULL);
   psz = pWorld ? pWorld->VariableGet (WSExternalWalls()) : NULL;
   dwExternalWall = 0;
   if (psz)
      dwExternalWall = (DWORD) _wtoi(psz);
   else
      DefaultBuildingSettings (NULL, NULL, NULL, NULL, NULL, &dwExternalWall);  // BUGFIX - extenral

   // get roofing, wall info, etc.
   DWORD dwRoofing;
   psz = pWorld->VariableGet (WSRoofing());
   dwRoofing = 0;
   if (psz)
      dwRoofing = (DWORD) _wtoi(psz);
   else
      DefaultBuildingSettings (NULL, NULL, NULL, NULL, &dwRoofing);  // BUGFIX - Default climate

   m_fConstBottom = TRUE;
   m_fConstAbove = TRUE;
   m_fConstRectangle = TRUE;

   // derive whether internal or external, curved or not, cement block or not
   BOOL fInternal, fCurved, fCementBlock, fDropCeiling;
   BOOL fTiled, fDecking;
   fInternal = fCurved = fCementBlock = fDropCeiling = fTiled = fDecking = FALSE;
   switch (HIWORD(m_dwType)) {
   case 1:  // wall
      switch (LOWORD(m_dwType)) {
      case 1:
      case 3:
      case 5:
      case 7:
         fInternal = TRUE;
         break;
      default:
         fInternal = FALSE;
         break;
      }
      switch (LOWORD(m_dwType)) {
      case 3:
      case 4:
      case 7:
      case 8:
         fCurved = TRUE;
         break;
      default:
         fCurved = FALSE;
         break;
      }
      switch (LOWORD(m_dwType)) {
      case 5:
      case 6:
      case 7:
      case 8:
         fCementBlock = TRUE;
         break;

      default:
         fCementBlock = FALSE;
      }
      break;
   case 2:  // roof/ceiling
      switch (LOWORD(m_dwType)) {
      case 2:
      case 3:
         fCurved = TRUE;
         break;
      default:
         fCurved = FALSE;
      }
      break;
   case 3:  // floor/ceiling
      fInternal = TRUE;
      // BUGFIX - Take out the constant requirements because screwing up mobility
      // of cabinet surfaces that create.
      m_fConstBottom = FALSE;
      switch (LOWORD(m_dwType)) {
      case 1:  // slab
         fCementBlock = TRUE;
         fTiled = TRUE;
         break;
      case 4:  // slab
         fCementBlock = TRUE;
         break;
      case 2:  // floor
         break;
      case 3:  // drop ceiling
         fDropCeiling = TRUE;
         break;
      case 5:  // deck
         fDecking = TRUE;
         break;
      case 6:  // cabinet counter
         fInternal = FALSE;   // BUGFIX
         break;
      case 7:  // cabinet shelf
         // do nothing
         break;
      }
      break;
   }

   // initialize the bevel
   m_fBevelLeft = m_fBevelRight = m_fBevelTop = m_fBevelBottom = 0;
   m_fThickStud = CM_THICKSTUDWALL;
   m_fThickA = CM_THICKSHEETROCK;
   m_fThickB = CM_THICKSHEETROCK;
   m_fHideSideA = m_fHideSideB = m_fHideEdges = FALSE;
   m_dwExtSideA = fInternal ? 0 : 1;
   m_dwExtSideB = 0;

   BOOL fHasStuds;
   fHasStuds = FALSE;

   switch (HIWORD(m_dwType)) {
   case 1:  // walls
      if (fCementBlock) {
         m_fThickStud = CM_THICKCEMENTBLOCK;
         if (fInternal)
            m_fThickStud = CM_THICKTHINCEMENTBLOCK;
      }
      else {
         // stud wall
         m_fThickStud = CM_THICKSTUDWALL;
      }
      if (!fInternal)
         m_fThickA = CM_THICKEXTNERALCLADDING;

      if (m_dwType == 0x1000c) {  // skirting
         m_fThickStud = 0;
         m_fThickA = .01;
         m_fThickB = .01;
         m_fHideSideB = m_fHideEdges = TRUE;
      }
      else if (m_dwType == 0x1000D) {  // cabinet walls
         m_fThickStud = CM_THICKCABINETWALL;
         m_fThickA = .0;
         m_fThickB = 0;
      }

      // if it's the special wall for bulding blocks then...
      if (LOWORD(m_dwType) == 0x0a) {  // BUGFIX - Was accidentally LOWORD(m_dwType == 0x0a)
         switch (dwExternalWall) {
         case 0:  // corrogated
         default:
            fHasStuds = TRUE;
            break;
         case 1:  // brick veneer
            m_fThickA = CM_THICKBRICKVENEER;
            fHasStuds = TRUE;
            break;
         case 2:  // clapboards
            fHasStuds = TRUE;
            break;
         case 3:  // stucco
            fHasStuds = TRUE;
            break;
         case 4:  // brick
            m_fThickStud = CM_THICKBRICKWALL;
            break;
         case 5:  // stone
            m_fThickStud = CM_THICKSTONEWALL;
            break;
         case 6:  // cement block
            m_fThickStud = CM_THICKCEMENTBLOCK;
            break;
         case 7:  // hay bale
            m_fThickStud = CM_THICKHAYBALE;
            break;
         case 8:  // logs
            m_fThickStud = CM_THICKLOGWALL;
            break;
         }
      }
      break;

   case 2:   // roof
      m_fConstBottom = FALSE; // dont hold to a flat bottom

      // change thickness and stuff
      m_fThickStud = CM_THICKROOFBEAM;
      m_fThickA = CM_THICKROOFING;
      break;

   case 3:
      m_fThickA = .01;  // lining for tiles usually
      m_fThickB = CM_THICKSHEETROCK; // lining for sheetrock usually
      if (fCementBlock) {
         m_fThickStud = CM_THICKTILEONLY;
      }
      else if (fDropCeiling)
         m_fThickStud = CM_THICKDROPCEILING;
      else
         m_fThickStud = CM_THICKFLOORJOIST;

      if (m_dwType == 0x30006) {  // counter top
         m_fThickStud = CM_THICKCABINETCOUNTER;
         m_fThickA = .0;
         m_fThickB = 0;
      }
      else if (m_dwType == 0x30007) {  // cabinet shelf
         m_fThickStud = CM_THICKCABINETSHELF;
         m_fThickA = .0;
         m_fThickB = 0;
      }
      break;
   }
   if (fCurved) {
      if (HIWORD(m_dwType) == 1) {
         CPoint   apPoints[2][3];
         DWORD x, y;
         fp fScaleX, fScaleZ;
         fScaleX = 4;   // start 4m wide
         fScaleZ = 4;   // 4m high

         for (y = 0; y < 2; y++) for (x = 0; x < 3; x++) {
            apPoints[y][x].Zero();
            apPoints[y][x].p[0] = ((fp)x - 1) * fScaleX/2;
            apPoints[y][x].p[2] = (1.0 - (fp)y) * fScaleZ;
            if (x == 1)
               apPoints[y][x].p[1] = -fScaleX/2;
         }

         m_SplineCenter.ControlPointsSet(
            FALSE, FALSE,
            3, 2, &apPoints[0][0],
      //         (DWORD*) SEGCURVE_CUBIC, (DWORD*) SEGCURVE_CUBIC);
            (DWORD*) SEGCURVE_CIRCLENEXT, (DWORD*) SEGCURVE_LINEAR);
         m_SplineCenter.DetailSet (1,3,1,3, .5);
      }
      else {   // roof
         CPoint   apPoints[3][2];
         DWORD x, y;
         for (y = 0; y < 3; y++) for (x = 0; x < 2; x++) {
            apPoints[y][x].Zero();
            apPoints[y][x].p[0] = ((fp)x - .5) * 12;
            apPoints[y][x].p[1] = (y == 1) ? -3 : 0;
            apPoints[y][x].p[2] = (1.0 - (fp)y) * 6;
            if (LOWORD(m_dwType) == 3) {
               apPoints[y][x].p[2] /= 2;  // shortened roof
               apPoints[y][x].p[1] /= 3;
            }
         }

         m_SplineCenter.ControlPointsSet(
            FALSE, FALSE,
            2, 3, &apPoints[0][0],
      //         (DWORD*) SEGCURVE_CUBIC, (DWORD*) SEGCURVE_CUBIC);
            (DWORD*) SEGCURVE_LINEAR, (DWORD*) SEGCURVE_CIRCLENEXT);
         m_SplineCenter.DetailSet (2,3,2,3, .5);
      }
   }
   else {
      CPoint   apPoints[2][2];
      DWORD x, y;
      fp fScaleX, fScaleZ;

      fScaleX = 4;   // start walls at 4m wide
      fScaleZ = 4;   // 4m high

      // if it's a roof or ceilingdouble the size
      if (HIWORD(m_dwType)==2) {
         fScaleX = 12;
         fScaleZ = 6;
      }
      else if (m_dwType == 0x1000D) {  // cabinet walls
         fScaleX = 2;
         fScaleZ = 1;
      }

      for (y = 0; y < 2; y++) for (x = 0; x < 2; x++) {
         apPoints[y][x].Zero();
         apPoints[y][x].p[0] = ((fp)x - .5) * fScaleX;
         apPoints[y][x].p[2] = (1.0 - (fp)y) * fScaleZ;


         // if it's a floor then make it square
         if (HIWORD(m_dwType)==3) {
            if ((m_dwType ==0x30006) || (m_dwType == 0x30007)) {
               // cabinet shelf or counter
               apPoints[y][x].p[0] =  ((fp)x - .5) * 2;
               apPoints[y][x].p[2] =  (.5 - (fp)y) * 1;
            }
            else {   // normal floor
               apPoints[y][x].p[0] =  ((fp)x - .5) * 12;
               apPoints[y][x].p[2] =  (.5 - (fp)y) * 12;
            }
         }

      }

      m_SplineCenter.ControlPointsSet(
         FALSE, FALSE,
         2, 2, &apPoints[0][0],
   //         (DWORD*) SEGCURVE_CUBIC, (DWORD*) SEGCURVE_CUBIC);
         (DWORD*) SEGCURVE_LINEAR, (DWORD*) SEGCURVE_LINEAR);
      m_SplineCenter.DetailSet (1,3,1,3, .5);
   }

   // Need to set spline A and B
   RecalcSides ();

   // edge spline
   CPoint apEdge[4];
   apEdge[0].Zero();
   apEdge[1].Zero();
   apEdge[1].p[0] = 1;
   apEdge[2].Zero();
   apEdge[2].p[0] = apEdge[2].p[1] = 1;
   apEdge[3].Zero();
   apEdge[3].p[1] = 1;
   m_SplineEdge.Init (TRUE, 4, apEdge, NULL, (DWORD*) SEGCURVE_LINEAR, 0, 2, .2);
   RecalcEdges ();

   // framing depends upon type
   m_fFramingDirty = TRUE;
   m_apFramingScale[0].p[1] = m_apFramingScale[1].p[1] = m_fThickStud;
   switch (HIWORD(m_dwType)) {
   case 1:  // walls
      switch (LOWORD(m_dwType)) {
      case 0x000A: // - External, Based on global wall settings
         if (!fHasStuds)
            break;   // no studs, so default of none
         // else, fall through
      case 0x0001: // - Internal
      case 0x0002: // - Extneral
      case 0x0003: // - Internal curved
      case 0x0004: // - External curved
         m_afFramingShow[0] = TRUE;
         m_afFramingMinDist[0] = .6;
         break;

      default:
      case 0x0005: // - Internal cement block
      case 0x0006: // - External cement block
      case 0x0007: // - Internal cement block, curved
      case 0x0008: // - External cement block, curved
      case 0x000C: // - Skirting around piers of house
      case 0x000D: // - Cabinet wall   
         // no studs
         break;
      }
      break;

   case 2:  // roof
      m_fFramingRotate = TRUE;
      m_afFramingShow[0] = TRUE;
      m_afFramingMinDist[0] = .9;
      m_afFramingShow[2] = TRUE;
      m_afFramingMinDist[2] = .9 * 4;
      m_apFramingScale[2].p[0] = .1;
      m_apFramingScale[2].p[1] = .2;
      break;

   case 3:  // floor/ceiling
      switch (LOWORD(m_dwType)) {
      case 0x0002: // - Floor
         m_afFramingShow[0] = TRUE;
         m_afFramingMinDist[0] = .45;
         break;
      case 0x0003: // - Drop ceiling
         m_afFramingShow[0] = TRUE;
         m_afFramingMinDist[0] = .6;
         m_fHideSideA = TRUE; // nothing above drop ceiling
         m_fHideEdges = TRUE;
         break;
      case 0x0005: // - Decking
         m_afFramingShow[0] = TRUE;
         m_afFramingMinDist[0] = .45;
         m_fHideSideB = TRUE; // no underside
         m_fHideEdges = TRUE;
         break;
      default:
      case 0x0001: // - Slab - with tiles
      case 0x0004: // - Slab - concrete
      case 0x0006: // - Cabinet countertop
      case 0x0007: // - Cabinet shelf
         // no framing
         break;
      }
      break;
   }

   // different colors for different sides
   switch (HIWORD(m_dwType)) {
   case 1:  // walls
      if (LOWORD(m_dwType) == 0x0A) {
         // special type
         switch (dwExternalWall) {
         case 0:  // corrogated
         case 1:  // brick veneer
         case 2:  // clapboards
         case 3:  // stucco
         default:
            if (dwExternalWall == 3)   // stucco
               ClaimSurface (0, TRUE, RGB(0xc0,0xc0,0xc0), MATERIAL_PAINTSEMIGLOSS, L"External stucco cladding",
                  &GTEXTURECODE_SidingStucco, &GTEXTURESUB_SidingStucco);
            else if (dwExternalWall == 2) // clapboards
               ClaimSurface (0, TRUE, RGB(0xe0,0xe0,0xe0), MATERIAL_PAINTSEMIGLOSS, L"External clapboards",
                  &GTEXTURECODE_SidingClapboards, &GTEXTURESUB_SidingClapboards);
            else if (dwExternalWall == 1) // brick veneer
               ClaimSurface (0, TRUE, RGB(0xc0,0xc0,0xc0), MATERIAL_TILEMATTE, L"External brick veneer",
                  &GTEXTURECODE_SidingBricks, &GTEXTURESUB_SidingBricks);
            else  // corrogated
               ClaimSurface (0, TRUE, RGB(0xc0,0xc0,0xc0), MATERIAL_PAINTGLOSS, L"External corrugated cladding",
                  &GTEXTURECODE_SidingCorrugated, &GTEXTURESUB_SidingCorrugated);

            ClaimSurface (1, TRUE, RGB(0xff, 0xff, 0xff), MATERIAL_PAINTSEMIGLOSS, L"Internal cladding");
            ClaimSurface (2, FALSE, RGB(0xc0,0xc0,0xc0), MATERIAL_PAINTSEMIGLOSS, L"Internal cladding");
            break;
         case 4:  // brick
            ClaimSurface (0, TRUE, RGB(0xc0,0xc0,0xc0), MATERIAL_TILEMATTE, L"Brick wall",
               &GTEXTURECODE_SidingBricks, &GTEXTURESUB_SidingBricks);
            ClaimSurface (1, TRUE, RGB(0xc0,0xc0,0xc0), MATERIAL_TILEMATTE, L"Brick wall",
               &GTEXTURECODE_SidingBricks, &GTEXTURESUB_SidingBricks);
            ClaimSurface (2, TRUE, RGB(0xc0,0xc0,0xc0), MATERIAL_TILEMATTE, L"Brick wall",
               &GTEXTURECODE_SidingBricks, &GTEXTURESUB_SidingBricks);
            break;
         case 5:  // stone
            ClaimSurface (0, TRUE, RGB(0xc0,0xc0,0xc0), MATERIAL_TILEMATTE, L"Stone wall",
               &GTEXTURECODE_SidingStone, &GTEXTURESUB_SidingStone);
            ClaimSurface (1, TRUE, RGB(0xc0,0xc0,0xc0), MATERIAL_TILEMATTE, L"Stone wall",
               &GTEXTURECODE_SidingStone, &GTEXTURESUB_SidingStone);
            ClaimSurface (2, TRUE, RGB(0xc0,0xc0,0xc0), MATERIAL_TILEMATTE, L"Stone wall",
               &GTEXTURECODE_SidingStone, &GTEXTURESUB_SidingStone);
            break;
         case 6:  // cement block
            ClaimSurface (0, TRUE, RGB(0xc0,0xc0,0xc0), MATERIAL_TILEMATTE, L"Cement block wall",
               &GTEXTURECODE_SidingBlock, &GTEXTURESUB_SidingBlock);
            ClaimSurface (1, TRUE, RGB(0xc0,0xc0,0xc0), MATERIAL_TILEMATTE, L"Cement block wall",
               &GTEXTURECODE_SidingBlock, &GTEXTURESUB_SidingBlock);
            ClaimSurface (2, TRUE, RGB(0xc0,0xc0,0xc0), MATERIAL_TILEMATTE,  L"Cement block wall",
               &GTEXTURECODE_SidingBlock, &GTEXTURESUB_SidingBlock);
            break;
         case 7:  // hay bale
            ClaimSurface (0, TRUE, RGB(0xc0,0xc0,0xc0), MATERIAL_PAINTSEMIGLOSS, L"Hay bale wall",
               &GTEXTURECODE_SidingStucco, &GTEXTURESUB_SidingStucco);
            ClaimSurface (1, TRUE, RGB(0xc0,0xc0,0xc0), MATERIAL_PAINTSEMIGLOSS, L"Hay bale wall",
               &GTEXTURECODE_SidingStucco, &GTEXTURESUB_SidingStucco);
            ClaimSurface (2, TRUE, RGB(0xc0,0xc0,0xc0), MATERIAL_PAINTSEMIGLOSS, L"Hay bale wall",
               &GTEXTURECODE_SidingStucco, &GTEXTURESUB_SidingStucco);
            break;
         case 8:  // logs
            ClaimSurface (0, TRUE, RGB(0xc0,0xc0,0xc0),MATERIAL_FLAT,  L"Log wall",
               &GTEXTURECODE_SidingLogs, &GTEXTURESUB_SidingLogs);
            ClaimSurface (1, TRUE, RGB(0xc0,0xc0,0xc0), MATERIAL_FLAT, L"Log wall",
               &GTEXTURECODE_SidingLogs, &GTEXTURESUB_SidingLogs);
            ClaimSurface (2, TRUE, RGB(0xc0,0xc0,0xc0), MATERIAL_FLAT, L"Log wall",
               &GTEXTURECODE_SidingLogs, &GTEXTURESUB_SidingLogs);
            break;
         }
      }
      else if (m_dwType == 0x1000C) {  // skirting
         ClaimSurface (0, TRUE, RGB(0xc0,0xc0,0x80), MATERIAL_PAINTSEMIGLOSS, L"Pier skirting",
            &GTEXTURECODE_Skirting, &GTEXTURESUB_Skirting);
         ClaimSurface (1, TRUE, RGB(0xc0,0xc0,0x80), MATERIAL_PAINTSEMIGLOSS, L"Pier skirting",
            &GTEXTURECODE_Skirting, &GTEXTURESUB_Skirting);
         ClaimSurface (2, TRUE, RGB(0xc0,0xc0,0x80), MATERIAL_PAINTSEMIGLOSS, L"Pier skirting",
            &GTEXTURECODE_Skirting, &GTEXTURESUB_Skirting);
      }
      else if (m_dwType == 0x1000D) {  // Cabinet walls
         ClaimSurface (0, TRUE, RGB(0xff, 0xff, 0xff), MATERIAL_PAINTSEMIGLOSS, L"Cabinet");
         ClaimSurface (1, TRUE, RGB(0xff, 0xff, 0xff), MATERIAL_PAINTSEMIGLOSS, L"Cabinet");
         ClaimSurface (2, TRUE, RGB(0xff, 0xff, 0xff), MATERIAL_PAINTSEMIGLOSS, L"Cabinet");
      }
      else if (fCementBlock) {
         if (!fInternal)
            ClaimSurface (0, TRUE, RGB(0xc0,0xc0,0xc0), MATERIAL_TILEMATTE, L"External block",
               &GTEXTURECODE_SidingBlock, &GTEXTURESUB_SidingBlock);
         else
            ClaimSurface (0, TRUE, RGB(0xff, 0xff, 0xff), MATERIAL_TILEMATTE, L"Internal block");
         ClaimSurface (1, TRUE, RGB(0xff, 0xff, 0xff), MATERIAL_TILEMATTE, L"Internal block");
         ClaimSurface (2, FALSE, RGB(0xff, 0xff, 0xff), MATERIAL_TILEMATTE, L"Internal block");
      }
      else {   // stud wall
         if (!fInternal)
            ClaimSurface (0, TRUE, RGB(0xc0,0xc0,0xc0), MATERIAL_PAINTGLOSS, L"External cladding",
               &GTEXTURECODE_SidingCorrugated, &GTEXTURESUB_SidingCorrugated);
         else
            ClaimSurface (0, TRUE, RGB(0xff, 0xff, 0xff), MATERIAL_PAINTSEMIGLOSS, L"Internal cladding");
         ClaimSurface (1, TRUE, RGB(0xff, 0xff, 0xff), MATERIAL_PAINTSEMIGLOSS, L"Internal cladding");
         ClaimSurface (2, FALSE, RGB(0xc0,0xc0,0xc0), MATERIAL_PAINTSEMIGLOSS, L"Internal cladding");
      }
      break;
   case 2:  // roof
      switch (dwRoofing) {
      case 0:  // corrugated iron
         ClaimSurface (0, TRUE, RGB(0xc0,0xc0,0xc0), MATERIAL_METALROUGH, L"Corrugated iron roofing",
            &GTEXTURECODE_RoofingCorrugated, &GTEXTURESUB_RoofingCorrugated);
         break;
      case 1:  // wood shings
         ClaimSurface (0, TRUE, RGB(0x60, 0x60, 0x40), MATERIAL_FLAT, L"Wood shingle roofing",
            &GTEXTURECODE_RoofingWoodShingles, &GTEXTURESUB_RoofingWoodShingles);
         break;
      case 2:  // tiles (spanish)
         ClaimSurface (0, TRUE, RGB(0xa0, 0xa0, 0x40), MATERIAL_TILEMATTE, L"Tile roofing",
            &GTEXTURECODE_RoofingTiles, &GTEXTURESUB_RoofingTiles);
         break;
      case 3:  // aspalt shigles
         ClaimSurface (0, TRUE, RGB(0x40, 0x40, 0x40), MATERIAL_FLAT, L"Asphalt shingle roofing",
            &GTEXTURECODE_RoofingAsphalt, &GTEXTURESUB_RoofingAsphalt);
         break;
      case 4:  // thatching
         // FUTURERELEASE - should have texture
         ClaimSurface (0, TRUE, RGB(0xc0, 0xc0, 0x40), MATERIAL_FLAT, L"Thatched roofing");
         break;
      }
      ClaimSurface (1, TRUE, RGB(0xff,0xff,0xff), MATERIAL_PAINTSEMIGLOSS, L"Ceiling");
      ClaimSurface (2, FALSE, RGB(0x40,0x40,0x40), MATERIAL_PAINTSEMIGLOSS, L"Roof trim");
      break;
   case 3:
      if (m_dwType == 0x30006) {  // countertop
         ClaimSurface (0, TRUE, RGB(0x40, 0x40, 0xff), MATERIAL_PAINTGLOSS, L"Countertop");
         ClaimSurface (1, TRUE, RGB(0x40, 0x40, 0xff), MATERIAL_PAINTGLOSS, L"Countertop");
         ClaimSurface (2, TRUE, RGB(0x40, 0x40, 0xff), MATERIAL_PAINTGLOSS, L"Countertop");
      }
      else if (m_dwType == 0x30007) {  // cabinet shelf
         ClaimSurface (0, TRUE, RGB(0xff, 0xff, 0xff), MATERIAL_PAINTSEMIGLOSS, L"Cabinet shelf");
         ClaimSurface (1, TRUE, RGB(0xff, 0xff, 0xff), MATERIAL_PAINTSEMIGLOSS, L"Cabinet shelf");
         ClaimSurface (2, TRUE, RGB(0xff, 0xff, 0xff), MATERIAL_PAINTSEMIGLOSS, L"Cabinet shelf");
      }
      else if (fCementBlock) {
         if (fTiled) {
            ClaimSurface (0, TRUE, RGB(0xc0, 0xc0, 0xc0), MATERIAL_TILEGLAZED, L"Floor tiles",
               &GTEXTURECODE_FlooringTiles, &GTEXTURESUB_FlooringTiles);
         }
         else
            ClaimSurface (0, TRUE, RGB(0xc0,0xc0,0xc0), MATERIAL_TILEMATTE);
         ClaimSurface (1, TRUE, RGB(0xc0,0xc0,0xc0), MATERIAL_TILEMATTE);
         ClaimSurface (2, FALSE, RGB(0xc0,0xc0,0xc0), MATERIAL_TILEMATTE);
      }
      else {
         if (fDropCeiling) {
            ClaimSurface (0, TRUE, RGB(0xc0,0xc0,0xc0), MATERIAL_PAINTSEMIGLOSS);

            // hide top and edges
            m_fHideSideA = m_fHideEdges = TRUE;
         }
         else {
            if (fDecking)
               ClaimSurface (0, TRUE, RGB(0xc0, 0xc0, 0x40), MATERIAL_FLAT, L"Decking",
                  &GTEXTURECODE_FlooringDecking, &GTEXTURESUB_FlooringDecking);
            else
               ClaimSurface (0, TRUE, RGB(0xc0, 0xc0, 0x40), MATERIAL_PAINTGLOSS, L"Flooring",
                  &GTEXTURECODE_FlooringHardwood, &GTEXTURESUB_FlooringHardwood);
         }

         ClaimSurface (1, TRUE, RGB(0xff,0xff,0xff),MATERIAL_PAINTSEMIGLOSS, L"Ceiling");
         ClaimSurface (2, FALSE, RGB(0xff, 0xff, 0xff), MATERIAL_PAINTSEMIGLOSS);
      }
      break;
   default:
      ClaimSurface (0, TRUE, RGB(0x80,0x80,0xff), MATERIAL_PAINTSEMIGLOSS, L"Side A");
      ClaimSurface (1, TRUE, RGB(0x80,0xff,0x80), MATERIAL_PAINTSEMIGLOSS, L"Side B");
      ClaimSurface (2, FALSE, RGB(0xc0, 0xc0, 0xc0), MATERIAL_PAINTSEMIGLOSS, L"Surface edge");
      break;
   }

   // for walls default to floor at 0
   if (HIWORD(m_dwType) == 1) {
      fp f;
      f = 1;
      m_listFloorsA.Add (&f);
      m_listFloorsB.Add (&f);
   }



   return TRUE;
}

/**********************************************************************************
CDoubleSurface::Render - Draws the surface

inputs
   POBJECTRENDER     pr - Render information
   PCRenderSurface   prs - Render surface. This is used for drawing, so should
         have any previous rotations already done. Push() and Pop() are used
         to insure the main matrix isn't affected
*/
void CDoubleSurface::Render (DWORD dwRenderShard, POBJECTRENDER pr, PCRenderSurface prs)
{

   // will need to check for framing before know if really can exclude
   BOOL fShowFramingInside, fShowFramingOutside;
   fShowFramingInside = m_afFramingShow[0] || m_afFramingShow[1];
   fShowFramingOutside = m_afFramingShow[2];
   if (!(pr->dwShow & RENDERSHOW_FRAMING))
      fShowFramingInside = fShowFramingOutside = FALSE;

   // might not want to draw this based on the type
   DWORD dwShowBits;
   dwShowBits = RENDERSHOW_MISC;
   switch (HIWORD(m_dwType)) {
   case 1:  // wall
      if (m_dwType == 0x1000d) {
         dwShowBits = RENDERSHOW_CABINETS;
      }
      else {
         dwShowBits = RENDERSHOW_WALLS;
      }
      break;
   case 2:  // roof/ceiling
      dwShowBits = RENDERSHOW_ROOFS;
      break;
   case 3:  // floor/ceiling
      if ((m_dwType == 0x30006) || (m_dwType == 0x30007)) {
         dwShowBits = RENDERSHOW_CABINETS;
      }
      else {
         dwShowBits = RENDERSHOW_FLOORS;
      }
      break;
   }

   // if have m_fFramingShowOnlyWhenVisible flag, then may not show framing
   if (m_fFramingShowOnlyWhenVisible && fShowFramingInside) {
      if ((pr->dwShow & dwShowBits) && !m_fHideSideA && !m_fHideSideB)
         fShowFramingInside = FALSE;

      // loop over all the sides and see if any of the framing is thicker than the cladding
      DWORD i;
      for (i = 0; i < 3; i++)
         if (m_afFramingShow[i] && (m_afFramingExtThick[i][0] || m_afFramingExtThick[i][1]))
            fShowFramingInside = TRUE;
   }

   if (!(pr->dwShow & dwShowBits) && !fShowFramingInside && !fShowFramingOutside)
      return;   // nothing to draw

   prs->Push();
   prs->Multiply (&m_MatrixToObject);

   // if not showing this then go to framing
   if (!(pr->dwShow & dwShowBits))
      goto showframing;

   DWORD dwSideA, dwSideB, dwEdge;
   PCLAIMSURFACE pcs;
   pcs = ClaimFindByReason(0);
   dwSideA = pcs ? pcs->dwID : 0;
   pcs = ClaimFindByReason(1);
   dwSideB = pcs ? pcs->dwID : 0;
   pcs = ClaimFindByReason (2);
   dwEdge = pcs ? pcs->dwID : 0;

   // ok to to backface culling on the sides if not hiding edges
   BOOL fBack;
   fBack = !(m_fHideSideA || m_fHideSideB);

   // BUGFIX - If anything if transparent then dont backface cull. If dont do this
   // when make exposed eaves transparent, cant see surface above.
   DWORD i, j;
   for (j = 0; j < 2; j++) {
      // first pass is fast and ignroes shemes, second looks into schems
      for (i = 0; fBack && (i < m_listCLAIMSURFACE.Num()); i++) {
         PCLAIMSURFACE p = (PCLAIMSURFACE) m_listCLAIMSURFACE.Get(i);
         if (p->dwReason > 5)
            continue;   // framing or something, so dont care for transparency
         PCObjectSurface pos;
         pos = m_pTemplate->ObjectSurfaceFind (p->dwID);
         if (!pos)
            continue;

         PCSurfaceSchemeSocket pss;
         if (pos->m_szScheme[0] && (j != 1))
            continue;   // ignore for this pass since the mallocing is slow
         if (pos->m_szScheme[0] && m_pTemplate->m_pWorld && (pss = m_pTemplate->m_pWorld->SurfaceSchemeGet ())) {
            PCObjectSurface pNew;
            pNew = pss->SurfaceGet (pos->m_szScheme, NULL, TRUE);
            if (!pNew)
               continue;   // error
            if (pNew->m_Material.m_wTransparency)
               fBack = FALSE;
            // since noclone delete pNew;
            continue;
         }
         
         // else just the surface
         if (pos->m_Material.m_wTransparency)
            fBack = FALSE;
      }  // i
   }  // j
   
   // BUGFIX - Because the object editor doesn't know what surfaces are needed
   // until they're requested, need to request them even with a bounding
   // box. Otherwise, surface modification won't work in the MIFLServer CRenderScene
   // draw side A

   // draw side A
   if (!m_fHideSideA) {
      prs->SetDefMaterial (dwRenderShard, m_pTemplate->ObjectSurfaceFind (dwSideA), m_pTemplate->m_pWorld);
      if (pr->dwReason == ORREASON_BOUNDINGBOX)
         m_SplineA.Render (prs, fBack, FALSE);
      else
         m_SplineA.Render (prs, fBack, TRUE, 1);

      // loop through the overlays
      DWORD i;
      PWSTR pszName;
      BOOL fClockwise;
      PTEXTUREPOINT ptp;
      DWORD dwNum;
      for (i = 0; i < m_SplineA.OverlayNum(); i++) {
         if (!m_SplineA.OverlayEnum (i, &pszName, &ptp, &dwNum, &fClockwise))
            continue;

         // find the ID
         PWSTR psz;
         DWORD dwID;
         for (psz = pszName; psz[0] && (psz[0] != L':'); psz++);
         if (psz[0]) {
            dwID = (DWORD)_wtoi(psz+1);
            prs->SetDefMaterial (dwRenderShard, m_pTemplate->ObjectSurfaceFind (dwID), m_pTemplate->m_pWorld);

            // BUGFIX - Only if not bounding box
            if (pr->dwReason != ORREASON_BOUNDINGBOX)
               m_SplineA.Render (prs, fBack, TRUE, i+2);
         }
      }
   }

   // draw side B
   if (!m_fHideSideB) {
      prs->SetDefMaterial (dwRenderShard, m_pTemplate->ObjectSurfaceFind (dwSideB), m_pTemplate->m_pWorld);
      if (pr->dwReason == ORREASON_BOUNDINGBOX)
         m_SplineB.Render (prs, fBack, FALSE);
      else
         m_SplineB.Render (prs, fBack, TRUE, 1);

      // loop through the overlays
      DWORD i;
      PWSTR pszName;
      BOOL fClockwise;
      PTEXTUREPOINT ptp;
      DWORD dwNum;
      for (i = 0; i < m_SplineB.OverlayNum(); i++) {
         if (!m_SplineB.OverlayEnum (i, &pszName, &ptp, &dwNum, &fClockwise))
            continue;

         // find the ID
         PWSTR psz;
         DWORD dwID;
         for (psz = pszName; psz[0] && (psz[0] != L':'); psz++);
         if (psz[0]) {
            dwID = (DWORD)_wtoi(psz+1);
            prs->SetDefMaterial (dwRenderShard, m_pTemplate->ObjectSurfaceFind (dwID), m_pTemplate->m_pWorld);

            // BUGFIX - Only if not bounding box
            if (pr->dwReason != ORREASON_BOUNDINGBOX)
               m_SplineB.Render (prs, fBack, TRUE, i+2);
         }
      }
   }


   // draw the edge
   if (!m_fHideEdges) {
      prs->SetDefMaterial (dwRenderShard, m_pTemplate->ObjectSurfaceFind (dwEdge), m_pTemplate->m_pWorld);

      // FUTURERELEASE - At some point multiple textures on edges?

      // find the two cutouts in A and B for the edges.
      // know its there.
      PTEXTUREPOINT pList1, pList2;
      DWORD dwNum1; //, dwNum2;
      BOOL fClockwise;
      PWSTR pszName;
      DWORD i;
      pList1 = pList2 = NULL;
      for (i = 0; i < m_SplineA.CutoutNum(); i++) {
         m_SplineA.CutoutEnum (i, &pszName, &pList1, &dwNum1, &fClockwise);
         if (!_wcsicmp(pszName, L"edge"))
            break;
      }
      //for (i = 0; i < m_SplineB.CutoutNum(); i++) {
      //   m_SplineB.CutoutEnum (i, &pszName, &pList2, &dwNum2, &fClockwise);
      //   if (!_wcsicmp(pszName, L"edge"))
      //      break;
      //}
      if (!pList1)// || !pList2)
         goto skiphideedges;

      DrawEdge (pList1, dwNum1, prs);


#if 0 // old code
      // convert these into two lists of texture points
      CListFixed  lTP1, lTP2;
      lTP1.Init (sizeof(HVXYZ));
      lTP2.Init (sizeof(HVXYZ));
      m_SplineA.IntersectLoopWithGrid (pList1, dwNum1, TRUE, &lTP1, TRUE);
         // want to inverse this so it goes around clickwise when looking at A
      m_SplineB.IntersectLoopWithGrid (pList2, dwNum2, TRUE, &lTP2, FALSE);
         // dont inverse this since B is already an inverted image

      // zipper them up
      prs->ShapeZipper ((PHVXYZ)lTP1.Get(0), lTP1.Num(),
         (PHVXYZ)lTP2.Get(0), lTP2.Num(), TRUE); //, .3, .4);
#endif
   }

skiphideedges:
showframing:
   if (fShowFramingInside || fShowFramingOutside) {
      // make sure framing is there
      FramingCalcIfDirty();

      PFNOODLE pn;
      DWORD dwNum;
      dwNum = m_lFNOODLE.Num();
      pn = (PFNOODLE) m_lFNOODLE.Get(0);

      // 3 colors
      DWORD i,j;
      for (i = 0; i < 3; i++) {
         if (!m_afFramingShow[i])
            continue;
         if (!fShowFramingInside && (i < 2))
            continue;   // if not showing inside framing then dont bother
         if (!fShowFramingOutside && (i >= 2))
            continue;   // not showing outside framing

         // Set the color for framing
         pcs = ClaimFindByReason(10+i);
         if (pcs)
            prs->SetDefMaterial (dwRenderShard, m_pTemplate->ObjectSurfaceFind (pcs->dwID), m_pTemplate->m_pWorld);

         // loop through and draw the noodles
         for (j = 0; j < dwNum; j++) {
            if (pn[j].dwColor != i)
               continue;   // wrong color
            if (pn[j].pNoodle)
               pn[j].pNoodle->Render (pr, prs);
         }
      }
   }  // if show framing


   prs->Pop();
}


/**********************************************************************************
BoundingBoxApplyMatrix - Applies a matrix to a bounding box. The points
   are modified in place.

inputs
   PCPoint           pCorner1 - Corner, minimum
   PCPoint           pCorner2 - Corner, maximum
   PCMatrix          pMatrix - Converts in current corner coords into new space
returns
   none
*/
void BoundingBoxApplyMatrix (PCPoint pCorner1, PCPoint pCorner2, PCMatrix pMatrix)
{
   CPoint p;
   CPoint p1, p2;
   p1.Copy (pCorner1);
   p2.Copy (pCorner2);
   DWORD i;
   for (i = 0; i < 8; i++) {
      p.p[0] = (i & 0x01) ? p1.p[0] : p2.p[0];
      p.p[1] = (i & 0x02) ? p1.p[1] : p2.p[1];
      p.p[2] = (i & 0x04) ? p1.p[2] : p2.p[2];
      p.p[3] = 1;

      p.MultiplyLeft (pMatrix);

      if (i) {
         pCorner1->Min (&p);
         pCorner2->Max (&p);
      }
      else {
         pCorner1->Copy (&p);
         pCorner2->Copy (&p);
      }
   } // i
}

/**********************************************************************************
CDoubleSurface::QueryBoundingBox - Standard API
*/
void CDoubleSurface::QueryBoundingBox (PCPoint pCorner1, PCPoint pCorner2)
{
   BOOL fSet = FALSE;
   CPoint p1, p2;

   pCorner1->Zero();
   pCorner2->Zero();

   if (!m_fHideSideA || !m_fHideEdges) {
      m_SplineA.QueryBoundingBox (&p1, &p2);

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

   if (!m_fHideSideB || !m_fHideEdges) {
      m_SplineB.QueryBoundingBox (&p1, &p2);

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

   // will need to check for framing before know if really can exclude
   BOOL fShowFramingInside, fShowFramingOutside;
   fShowFramingInside = m_afFramingShow[0] || m_afFramingShow[1];
   fShowFramingOutside = m_afFramingShow[2];


   if (fShowFramingInside || fShowFramingOutside) {
      // make sure framing is there
      FramingCalcIfDirty();

      PFNOODLE pn;
      DWORD dwNum;
      dwNum = m_lFNOODLE.Num();
      pn = (PFNOODLE) m_lFNOODLE.Get(0);

      // 3 colors
      DWORD i,j;
      for (i = 0; i < 3; i++) {
         if (!m_afFramingShow[i])
            continue;
         if (!fShowFramingInside && (i < 2))
            continue;   // if not showing inside framing then dont bother
         if (!fShowFramingOutside && (i >= 2))
            continue;   // not showing outside framing

         // loop through and draw the noodles
         for (j = 0; j < dwNum; j++)
            if (pn[j].pNoodle) {
               pn[j].pNoodle->QueryBoundingBox (&p1, &p2);
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
      } // i
   }  // if show framing

   // apply matrix
   BoundingBoxApplyMatrix (pCorner1, pCorner2, &m_MatrixToObject);


}


/************************************************************************************
DrawEdgeCache - Cached lists instead of always allocing them.

inputs
   PCListFixed       plDrawEdge - DrawEdgeA or B
   PCListFixed       plDrawEdgeBack - DrawEdgeABack or DrawEdgeBBack
returns
   PCListFixed - new list that was just gotten from the backup, or added to the backup
*/
PCListFixed DrawEdgeCache (PCListFixed plDrawEdge, PCListFixed plDrawEdgeBack)
{
   DWORD dwNum = plDrawEdge->Num();
   if (dwNum < plDrawEdgeBack->Num())
      return *((PCListFixed*)plDrawEdgeBack->Get(dwNum));
      // already cached

   // else, need to create
   PCListFixed plNew = new CListFixed;
   if (!plNew)
      return NULL;   // error. sholdnt happen
   plDrawEdgeBack->Add (&plNew);
   return plNew;
}

/************************************************************************************
CDoubleSurface::DrawEdge - Given two cutouts (from side A and B resepctively), this
draws the edge using the cutouts. An extra trick is that the edge is only drawn where
the two cutouts really end up being the edge... if more bits are cut out intenerally
then no line is drawn there.

NOTE: This only works for the actual edge - since it assumes that given  cutout A,
cutout B is a mirror image.

inputs
   PTEXTUREPOINT     pCutoutA - Cutout on side A
   DWORD             dwNumA - Number of points on side A.
   PCRenderSurface   prs - render surface
returns
   none
*/
void CDoubleSurface::DrawEdge (PTEXTUREPOINT pCutoutA, DWORD dwNumA, PCRenderSurface prs)
{
   // create a list of lists for A and B
   DWORD i;//,j;
   // CListFixed  lA, lB;
   // PCListFixed plf;
   m_lDrawEdgeA.Init (sizeof(CListFixed));
   m_lDrawEdgeB.Init (sizeof(CListFixed));

   // loop over edges
   // CListFixed lHVTempA, lHVTempB;
   m_lDrawEdgeHVTempA.Init (sizeof(TEXTUREPOINT));
   m_lDrawEdgeHVTempB.Init (sizeof(TEXTUREPOINT));
   PCListFixed plCurHVXYZ;
   // loop over all the points
   for (i = 0; i < dwNumA; i++) {
      // these points
      TEXTUREPOINT tpCurSA, tpCurEA, tpCurSB, tpCurEB;
      tpCurSA = pCutoutA[i];
      tpCurEA = pCutoutA[(i+1)%dwNumA];
      tpCurSB = pCutoutA[i];
      tpCurSB.h = 1.0 - tpCurSB.h;  // take into account side B is flipped
      tpCurEB = pCutoutA[(i+1)%dwNumA];
      tpCurEB.h = 1.0 - tpCurEB.h;  // take into account side B is flipped

      while (TRUE) {
         // find out where this intersects
         TEXTUREPOINT tpsA, tpeA, tpsB, tpeB;
         BOOL fRetA, fRetB;
         fRetA = m_SplineA.FindLongestSegment (&tpCurSA, &tpCurEA, &tpsA, &tpeA);
         fRetB = m_SplineB.FindLongestSegment (&tpCurSB, &tpCurEB, &tpsB, &tpeB);
         if (!fRetA || !fRetB) {
            // couldnt find anything in this segment, therefore, if there is
            // a lHVTemp, convert it to HVXYZ and add to the list
            plCurHVXYZ = m_lDrawEdgeHVTempA.Num() ? DrawEdgeCache(&m_lDrawEdgeA, &m_lDrawEdgeABack) : NULL;
            if (plCurHVXYZ) {
               plCurHVXYZ->Init (sizeof(HVXYZ));
               m_SplineA.IntersectLoopWithGrid ((PTEXTUREPOINT) m_lDrawEdgeHVTempA.Get(0),
                  m_lDrawEdgeHVTempA.Num(), FALSE, plCurHVXYZ, TRUE);
               m_lDrawEdgeA.Add (&plCurHVXYZ);
            }
            plCurHVXYZ = m_lDrawEdgeHVTempB.Num() ? DrawEdgeCache(&m_lDrawEdgeB, &m_lDrawEdgeBBack) : NULL;
            if (plCurHVXYZ) {
               plCurHVXYZ->Init (sizeof(HVXYZ));
               m_SplineB.IntersectLoopWithGrid ((PTEXTUREPOINT) m_lDrawEdgeHVTempB.Get(0),
                  m_lDrawEdgeHVTempB.Num(), FALSE, plCurHVXYZ, TRUE);
               m_lDrawEdgeB.Add (&plCurHVXYZ);
            }

            // clear old list
            m_lDrawEdgeHVTempA.Clear();
            m_lDrawEdgeHVTempB.Clear();

            break;   // nothing found here
         }

         // else, found a line segment that matches.

         // if the found start is not the intendend start then
         // there was a break in the line. Therefore, start a new sequences
         if (!AreClose (&tpCurSA, &tpsA) || !AreClose(&tpCurSB, &tpsB)) {
            // save this sequences
            plCurHVXYZ = m_lDrawEdgeHVTempA.Num() ? DrawEdgeCache(&m_lDrawEdgeA, &m_lDrawEdgeABack) : NULL;
            if (plCurHVXYZ) {
               plCurHVXYZ->Init (sizeof(HVXYZ));
               m_SplineA.IntersectLoopWithGrid ((PTEXTUREPOINT) m_lDrawEdgeHVTempA.Get(0),
                  m_lDrawEdgeHVTempA.Num(), FALSE, plCurHVXYZ, TRUE);
               m_lDrawEdgeA.Add (&plCurHVXYZ);
            }
            plCurHVXYZ = m_lDrawEdgeHVTempB.Num() ? DrawEdgeCache(&m_lDrawEdgeB, &m_lDrawEdgeBBack) : NULL;
            if (plCurHVXYZ) {
               plCurHVXYZ->Init (sizeof(HVXYZ));
               m_SplineB.IntersectLoopWithGrid ((PTEXTUREPOINT) m_lDrawEdgeHVTempB.Get(0),
                  m_lDrawEdgeHVTempB.Num(), FALSE, plCurHVXYZ, TRUE);
               m_lDrawEdgeB.Add (&plCurHVXYZ);
            }

            // clear old list
            m_lDrawEdgeHVTempA.Clear();
            m_lDrawEdgeHVTempB.Clear();

            // follow through and add this
         }

         // if nothing in the list then add the start
         if (!m_lDrawEdgeHVTempA.Num())
            m_lDrawEdgeHVTempA.Add (&tpsA);
         if (!m_lDrawEdgeHVTempB.Num())
            m_lDrawEdgeHVTempB.Add (&tpsB);

         // add the end
         m_lDrawEdgeHVTempA.Add (&tpeA);
         m_lDrawEdgeHVTempB.Add (&tpeB);

         // if the ends are close then we're done with this segment
         if (AreClose (&tpCurEA, &tpeA) || AreClose(&tpCurEB, &tpeB))
            break;

         // move on
         tpCurSA = tpeA;
         tpCurSB = tpeB;

      } // while(TRUE)

   }  // over all points
   

   // if anything in m_lDrawEdgeHVTemp, add it
   plCurHVXYZ = m_lDrawEdgeHVTempA.Num() ? DrawEdgeCache(&m_lDrawEdgeA, &m_lDrawEdgeABack) : NULL;
   if (plCurHVXYZ) {
      plCurHVXYZ->Init (sizeof(HVXYZ));
      m_SplineA.IntersectLoopWithGrid ((PTEXTUREPOINT) m_lDrawEdgeHVTempA.Get(0),
         m_lDrawEdgeHVTempA.Num(), FALSE, plCurHVXYZ, TRUE);
      m_lDrawEdgeA.Add (&plCurHVXYZ);
   }
   plCurHVXYZ = m_lDrawEdgeHVTempB.Num() ? DrawEdgeCache(&m_lDrawEdgeB, &m_lDrawEdgeBBack) : NULL;
   if (plCurHVXYZ) {
      plCurHVXYZ->Init (sizeof(HVXYZ));
      m_SplineB.IntersectLoopWithGrid ((PTEXTUREPOINT) m_lDrawEdgeHVTempB.Get(0),
         m_lDrawEdgeHVTempB.Num(), FALSE, plCurHVXYZ, TRUE);
      m_lDrawEdgeB.Add (&plCurHVXYZ);
   }

   // clear old list
   m_lDrawEdgeHVTempA.Clear();
   m_lDrawEdgeHVTempB.Clear();

   // loop through all the lists in A
   PCListFixed plfA, plfB;
   //CPoint p;
   PHVXYZ ptpA, ptpB;
   DWORD dwClosest;
   for (i = 0; i < m_lDrawEdgeA.Num(); i++) {
      plfA = *((PCListFixed*) m_lDrawEdgeA.Get(i));
      ptpA = (PHVXYZ) plfA->Get(0);

      // since everything done in tandem, know the closest for B also
      dwClosest = i;

      // know which one is closest, so use that for zipper
      plfB = *((PCListFixed*) m_lDrawEdgeB.Get(dwClosest));
      ptpB = (PHVXYZ) plfB->Get(0);

      // draw it
      prs->ShapeZipper (ptpA, plfA->Num(), ptpB, plfB->Num(), FALSE); //, .3, .4);

   }

   // note: Don't free them up since doing this in destructor
   // as assigned to m_lDrawEdgeABack and m_lDrawEdgeBBack
#if 0
   // free them up
   for (i = 0; i < m_lDrawEdgeA.Num(); i++) {
      plf = *((PCListFixed*) m_lDrawEdgeA.Get(i));
      delete plf;
   }
   for (i = 0; i < m_lDrawEdgeB.Num(); i++) {
      plf = *((PCListFixed*) m_lDrawEdgeB.Get(i));
      delete plf;
   }
#endif
}

#if 0 // doesnt work well
/************************************************************************************
CDoubleSurface::DrawEdge - Given two cutouts (from side A and B resepctively), this
draws the edge using the cutouts. An extra trick is that the edge is only drawn where
the two cutouts really end up being the edge... if more bits are cut out intenerally
then no line is drawn there.

IMPORTANT: This assumes that A and B are both counter-clocwise cutouts that are
essentially mirrors of one another.

inputs
   PTEXTUREPOINT     pCutoutA - Cutout on side A
   DWORD             dwNumA - Number of points on side A.
   PTEXTUREPOINT     pCutoutB - Cutout on side B
   DWORD             dwNumB - Number of points ons ide B
   PCRenderSurface   prs - render surface
returns
   none
*/
void CDoubleSurface::DrawEdge (PTEXTUREPOINT pCutoutA, DWORD dwNumA,
                               PTEXTUREPOINT pCutoutB, DWORD dwNumB, PCRenderSurface prs)
{
   // find the point in B which is closest to pCutoutA[0]
   DWORD dwClosest;
   fp fClosest, fDist;
   DWORD i, j;
   for (i = 0; i < dwNumB; i++) {
      // take into account fact that B is reversed along h
      fDist = (1.0 - pCutoutB[i].h) - pCutoutA[0].h;
      fDist = fDist * fDist + (pCutoutB[i].v - pCutoutA[0].v) * (pCutoutB[i].v - pCutoutA[0].v);
      if (!i || (fDist < fClosest)) {
         dwClosest = i;
         fClosest = fDist;
      }
   }

   // reverse B now
   CMem mem;
   if (!mem.Required (sizeof(TEXTUREPOINT) * dwNumB))
      return;
   PTEXTUREPOINT p2;
   p2 = (PTEXTUREPOINT) mem.p;
   for (i = 0; i < dwNumB; i++)
      p2[i] = pCutoutB[(dwNumB+dwClosest-i) % dwNumB];
   pCutoutB = p2;

   // create a list of lists for A and B
   CListFixed  lA, lB;
   PCListFixed plf;
   lA.Init (sizeof(CListFixed));
   lB.Init (sizeof(CListFixed));

   // loop over edges
   DWORD dwEdge;
   CListFixed lHVTemp;
   lHVTemp.Init (sizeof(TEXTUREPOINT));
   PCListFixed plCurHVXYZ;
   for (dwEdge = 0; dwEdge < 2; dwEdge++) {
      PTEXTUREPOINT pCutout = dwEdge ? pCutoutB : pCutoutA;
      DWORD dwNum = dwEdge ? dwNumB : dwNumA;
      PCListFixed pl = dwEdge ? &lB : &lA;
      PCSplineSurface pss = dwEdge ? &m_SplineB : &m_SplineA;

      // loop over all the points
      for (i = 0; i < dwNum; i++) {
         // these points
         TEXTUREPOINT tpCurS, tpCurE;
         tpCurS = pCutout[i];
         tpCurE = pCutout[(i+1)%dwNum];

         while (TRUE) {
            // find out where this intersects
            TEXTUREPOINT tps, tpe;
            BOOL fRet;
            fRet = pss->FindLongestSegment (&tpCurS, &tpCurE, &tps, &tpe);
            if (!fRet) {
               // couldnt find anything in this segment, therefore, if there is
               // a lHVTemp, convert it to HVXYZ and add to the list
               plCurHVXYZ = lHVTemp.Num() ? (new CListFixed) : NULL;
               if (plCurHVXYZ) {
                  plCurHVXYZ->Init (sizeof(HVXYZ));
                  pss->IntersectLoopWithGrid ((PTEXTUREPOINT) lHVTemp.Get(0),
                     lHVTemp.Num(), FALSE, plCurHVXYZ, TRUE);
                  pl->Add (&plCurHVXYZ);
               }

               // clear old list
               lHVTemp.Clear();

               break;   // nothing found here
            }

            // else, found a line segment that matches.

            // if the found start is not the intendend start then
            // there was a break in the line. Therefore, start a new sequences
            if (!AreClose (&tpCurS, &tps)) {
               // save this sequences
               plCurHVXYZ = lHVTemp.Num() ? (new CListFixed) : NULL;
               if (plCurHVXYZ) {
                  plCurHVXYZ->Init (sizeof(HVXYZ));
                  pss->IntersectLoopWithGrid ((PTEXTUREPOINT) lHVTemp.Get(0),
                     lHVTemp.Num(), FALSE, plCurHVXYZ, TRUE);
                  pl->Add (&plCurHVXYZ);
               }

               // clear old list
               lHVTemp.Clear();

               // follow through and add this
            }

            // if nothing in the list then add the start
            if (!lHVTemp.Num())
               lHVTemp.Add (&tps);

            // add the end
            lHVTemp.Add (&tpe);

            // if the ends are close then we're done with this segment
            if (AreClose (&tpCurE, &tpe))
               break;

            // move on
            tpCurS = tpe;

         } // while(TRUE)

      }  // over all points
      

      // if anything in lHVTemp, add it
      plCurHVXYZ = lHVTemp.Num() ? (new CListFixed) : NULL;
      if (plCurHVXYZ) {
         plCurHVXYZ->Init (sizeof(HVXYZ));
         pss->IntersectLoopWithGrid ((PTEXTUREPOINT) lHVTemp.Get(0),
            lHVTemp.Num(), FALSE, plCurHVXYZ, TRUE);
         pl->Add (&plCurHVXYZ);
      }

      // clear old list
      lHVTemp.Clear();

   }  // over both edges

   // loop through all the lists in A
   PCListFixed plfA, plfB;
   CPoint p;
   PHVXYZ ptpA, ptpB;
   for (i = 0; i < lA.Num(); i++) {
      plfA = *((PCListFixed*) lA.Get(i));
      ptpA = (PHVXYZ) plfA->Get(0);

      // find the closest starting point in B
      DWORD dwClosest;
      fp fClosest, fDist;
      dwClosest = (DWORD)-1;
      for (j = 0; j < lB.Num(); j++) {
         plfB = *((PCListFixed*) lB.Get(j));
         ptpB = (PHVXYZ) plfB->Get(0);

         p.Subtract (&ptpB->p, &ptpA->p);
         fDist = p.Length();
         if (!j || (fDist < fClosest)) {
            dwClosest = j;
            fClosest = fDist;
         }
      }
      if (dwClosest == (DWORD)-1)
         continue;

      // know which one is closest, so use that for zipper
      plfB = *((PCListFixed*) lB.Get(dwClosest));
      ptpB = (PHVXYZ) plfB->Get(0);

      // draw it
      prs->ShapeZipper (ptpA, plfA->Num(), ptpB, plfB->Num(), FALSE); //, .3, .4);

   }

   // free them up
   for (i = 0; i < lA.Num(); i++) {
      plf = *((PCListFixed*) lA.Get(i));
      delete plf;
   }
   for (i = 0; i < lB.Num(); i++) {
      plf = *((PCListFixed*) lB.Get(i));
      delete plf;
   }
}
#endif


/************************************************************************************
OverlayShapeShapeFindByColor - Given a color (also used for the current control
points displayed), find the overlay information.

inputs
   DWORD       dwColor - color
   PWSTR       *ppszName - Filled with the name
   PTEXTUREPOINT *pptp - Filled with the pointer to teh texture points
   DWORD       *pdwNum - Filled with the number
   BOOL        *pfClockwise - Filled with TRUE if clockwise
returns
   PCSplineSurface - Spline. NULL if can't find
*/
PCSplineSurface CDoubleSurface::OverlayShapeFindByColor (DWORD dwColor, PWSTR *ppszName, PTEXTUREPOINT *pptp,
                                         DWORD *pdwNum, BOOL *pfClockwise)
{
   DWORD i;
   for (i = 0; i < m_listCLAIMSURFACE.Num(); i++) {
      PCLAIMSURFACE p = (PCLAIMSURFACE) m_listCLAIMSURFACE.Get(i);
      if (p->dwID == dwColor) {
         PCSplineSurface pss;

         if ((p->dwReason == 0) || (p->dwReason == 3))
            pss = &m_SplineA;
         else if ((p->dwReason == 1) || (p->dwReason == 4))
            pss = &m_SplineB;
         else
            return NULL;

         DWORD i;
         for (i = 0; i < pss->OverlayNum(); i++) {
            if (!pss->OverlayEnum (i, ppszName, pptp, pdwNum, pfClockwise))
               continue;
            if (dwColor == OverlayShapeNameToColor(*ppszName)) {
               return pss;
            }
         }

         return NULL;
      }
   }

   return NULL;
}

/**********************************************************************************
CDoubleSurface::Clone - Clones this

NOTE: This does not actually clone the claimed colors, etc.

inputs
   PCDoubleSurface      pNew - Overwrite this one's information.
   PCObjecTemplate      pTemplate - New template.
returns
   PCDoubleSurface - Clone
*/
void CDoubleSurface::CloneTo (CDoubleSurface *pNew, PCObjectTemplate pTemplate)
{
   // Clone member variables
   m_SplineCenter.CloneTo (&pNew->m_SplineCenter);
   m_SplineA.CloneTo (&pNew->m_SplineA);
   m_SplineB.CloneTo (&pNew->m_SplineB);
   m_SplineEdge.CloneTo (&pNew->m_SplineEdge);

   // clone bevel information
   pNew->m_fBevelLeft = m_fBevelLeft;
   pNew->m_fBevelRight = m_fBevelRight;
   pNew->m_fBevelBottom = m_fBevelBottom;
   pNew->m_fBevelTop = m_fBevelTop;

   pNew->m_fConstBottom = m_fConstBottom;
   pNew->m_fConstAbove = m_fConstAbove;
   pNew->m_fConstRectangle = m_fConstRectangle;

   // clone thickness
   pNew->m_fThickStud = m_fThickStud;
   pNew->m_fThickA = m_fThickA;
   pNew->m_fThickB = m_fThickB;

   pNew->m_fHideSideA = m_fHideSideA;
   pNew->m_fHideSideB = m_fHideSideB;
   pNew->m_fHideEdges = m_fHideEdges;
   pNew->m_dwExtSideA = m_dwExtSideA;
   pNew->m_dwExtSideB = m_dwExtSideB;

   pNew->m_dwType = m_dwType;
   pNew->m_MatrixFromObject.Copy (&m_MatrixFromObject);
   pNew->m_MatrixToObject.Copy (&m_MatrixToObject);
   pNew->m_pTemplate = pTemplate;

   pNew->m_listFloorsA.Init (sizeof(fp), m_listFloorsA.Get(0), m_listFloorsA.Num());
   pNew->m_listFloorsB.Init (sizeof(fp), m_listFloorsB.Get(0), m_listFloorsB.Num());

   // clone framing info
   pNew->m_fFramingDirty = m_fFramingDirty;
   pNew->m_fFramingRotate = m_fFramingRotate;
   pNew->m_fFramingShowOnlyWhenVisible = m_fFramingShowOnlyWhenVisible;
   memcpy (pNew->m_afFramingShow, m_afFramingShow, sizeof(m_afFramingShow));
   memcpy (pNew->m_afFramingInset, m_afFramingInset, sizeof(m_afFramingInset));
   memcpy (pNew->m_afFramingExtThick, m_afFramingExtThick, sizeof(m_afFramingExtThick));
   memcpy (pNew->m_afFramingInsetTL, m_afFramingInsetTL, sizeof(m_afFramingInsetTL));
   memcpy (pNew->m_afFramingMinDist, m_afFramingMinDist, sizeof(m_afFramingMinDist));
   memcpy (pNew->m_afFramingBevel, m_afFramingBevel, sizeof(m_afFramingBevel));
   memcpy (pNew->m_afFramingExtend, m_afFramingExtend, sizeof(m_afFramingExtend));
   memcpy (pNew->m_adwFramingShape, m_adwFramingShape, sizeof(m_adwFramingShape));
   memcpy (pNew->m_apFramingScale, m_apFramingScale, sizeof(m_apFramingScale));
   pNew->FramingClear();
   pNew->m_lFNOODLE.Init (sizeof(FNOODLE), m_lFNOODLE.Get(0), m_lFNOODLE.Num());
   DWORD i;
   for (i = 0; i < pNew->m_lFNOODLE.Num(); i++) {
      PFNOODLE pn = (PFNOODLE) pNew->m_lFNOODLE.Get(i);
      pn->pNoodle = pn->pNoodle->Clone();
   }

   // clone the claim surface - because assuming that the cobjectemplate, when
   // cloned, erased the old ones, dont need to do this
   pNew->m_listCLAIMSURFACE.Init (sizeof(CLAIMSURFACE), m_listCLAIMSURFACE.Get(0),
      m_listCLAIMSURFACE.Num());

}


static PWSTR gpszConstAbove = L"ConstAbove";
static PWSTR gpszConstBottom = L"ConstBottom";
static PWSTR gpszConstPlane = L"ConstPlane";
static PWSTR gpszConstRectangle = L"Rectangle";

static PWSTR gpszBevelLeft = L"BevelLeft";
static PWSTR gpszBevelRight = L"BevelRight";
static PWSTR gpszBevelTop = L"BevelTop";
static PWSTR gpszBevelBottom = L"BevelBottom";
static PWSTR gpszThickStud = L"ThickStud";
static PWSTR gpszThickA = L"ThickA";
static PWSTR gpszThickB = L"ThickB";
static PWSTR gpszHideSideA = L"HideSideA";
static PWSTR gpszHideSideB = L"HideSideB";
static PWSTR gpszHideEdges = L"HideEdges";
static PWSTR gpszExtSideA = L"ExtSideA";
static PWSTR gpszExtSideB = L"ExtSideB";
static PWSTR gpszType = L"Type";
static PWSTR gpszMatrixToObject = L"MatrixToObject";
static PWSTR gpszMatrixFromObject = L"MatrixFromObject";
static PWSTR gpszReason = L"Reason";
static PWSTR gpszID = L"ID";
static PWSTR gpszAlsoEmbed = L"AlsoEmbed";
static PWSTR gpszClaimSurface = L"ClaimSurface";
static PWSTR gpszFloorsA = L"FloorsA%d";
static PWSTR gpszFloorsB = L"FloorsB%d";
static PWSTR gpszFramingRotate = L"FramingRotate";
static PWSTR gpszFramingShow = L"FramingShow%d";
static PWSTR gpszFramingInsetTL = L"FramingInsetTL%d";
static PWSTR gpszFramingShape = L"FramingShape%d";
static PWSTR gpszFramingInset = L"FramingInset%d%d";
static PWSTR gpszFramingExtThick = L"FramingExtThick%d%d";
static PWSTR gpszFramingMinDist = L"FramingMinDist%d";
static PWSTR gpszFramingScale = L"FramingScale%d";
static PWSTR gpszFramingBevel = L"FramingBevel%d%d";
static PWSTR gpszFramingExtend = L"FramingExtend%d%d";
static PWSTR gpszFramingShowOnlyWhenVisible = L"FramingShowOnlyWhenVisible";

PCMMLNode2 CDoubleSurface::MMLTo (void)
{
   PCMMLNode2 pNode;
   pNode = new CMMLNode2;
   if (!pNode)
      return NULL;
   pNode->NameSet (L"DoubleSurface");

   // member variables go here
   PCMMLNode2 pSub;
   pSub = m_SplineCenter.MMLTo ();
   if (pSub) {
      pSub->NameSet (L"SplineCenter");
      pNode->ContentAdd (pSub);
   }
   pSub = m_SplineA.MMLTo ();
   if (pSub) {
      pSub->NameSet (L"SplineA");
      pNode->ContentAdd (pSub);
   }
   pSub = m_SplineB.MMLTo ();
   if (pSub) {
      pSub->NameSet (L"SplineB");
      pNode->ContentAdd (pSub);
   }
   pSub = m_SplineEdge.MMLTo ();
   if (pSub) {
      pSub->NameSet (L"Edge");
      pNode->ContentAdd (pSub);
   }


   // save bevel information
   MMLValueSet (pNode, gpszBevelLeft, m_fBevelLeft);
   MMLValueSet (pNode, gpszBevelRight, m_fBevelRight);
   MMLValueSet (pNode, gpszBevelTop, m_fBevelTop);
   MMLValueSet (pNode, gpszBevelBottom, m_fBevelBottom);

   // save thickness information
   MMLValueSet (pNode, gpszThickStud, m_fThickStud);
   MMLValueSet (pNode, gpszThickA, m_fThickA);
   MMLValueSet (pNode, gpszThickB, m_fThickB);

   // save contrains information
   MMLValueSet (pNode, gpszConstBottom, (int) m_fConstBottom);
   MMLValueSet (pNode, gpszConstAbove, (int) m_fConstAbove);
   MMLValueSet (pNode, gpszConstRectangle, (int) m_fConstRectangle);
   
   // save hiding info
   MMLValueSet (pNode, gpszHideSideA, (int) m_fHideSideA);
   MMLValueSet (pNode, gpszHideSideB, (int) m_fHideSideB);
   MMLValueSet (pNode, gpszHideEdges, (int) m_fHideEdges);
   MMLValueSet (pNode, gpszExtSideA, (int) m_dwExtSideA);
   MMLValueSet (pNode, gpszExtSideB, (int) m_dwExtSideB);

   MMLValueSet (pNode, gpszType, (int)m_dwType);
   MMLValueSet (pNode, gpszMatrixToObject, &m_MatrixToObject);
   MMLValueSet (pNode, gpszMatrixFromObject, &m_MatrixFromObject);

   // framing info
   MMLValueSet (pNode, gpszFramingRotate, (int) m_fFramingRotate);
   MMLValueSet (pNode, gpszFramingShowOnlyWhenVisible, (int) m_fFramingShowOnlyWhenVisible);
   DWORD i,j;
   WCHAR szTemp[64];
   for (i = 0; i < 3; i++) {
      swprintf (szTemp, gpszFramingShow, (int) i);
      MMLValueSet (pNode, szTemp, (int) m_afFramingShow[i]);

      swprintf (szTemp, gpszFramingInsetTL, (int) i);
      MMLValueSet (pNode, szTemp, (int) m_afFramingInsetTL[i]);

      swprintf (szTemp, gpszFramingShape, (int) i);
      MMLValueSet (pNode, szTemp, (int) m_adwFramingShape[i]);

      swprintf (szTemp, gpszFramingMinDist, (int) i);
      MMLValueSet (pNode, szTemp, m_afFramingMinDist[i]);

      swprintf (szTemp, gpszFramingScale, (int) i);
      MMLValueSet (pNode, szTemp, &m_apFramingScale[i]);

      for (j = 0; j < 2; j++) {
         swprintf (szTemp, gpszFramingInset, (int) i, (int) j);
         MMLValueSet (pNode, szTemp, m_afFramingInset[i][j]);

         if (m_afFramingExtThick[i][j]) {
            swprintf (szTemp, gpszFramingExtThick, (int) i, (int) j);
            MMLValueSet (pNode, szTemp, m_afFramingExtThick[i][j]);
         }

         swprintf (szTemp, gpszFramingBevel, (int) i, (int)j);
         MMLValueSet (pNode, szTemp, m_afFramingBevel[i][j]);

         swprintf (szTemp, gpszFramingExtend, (int) i, (int)j);
         MMLValueSet (pNode, szTemp, m_afFramingExtend[i][j]);
      }
   }

   //DWORD i;
   for (i = 0; i < m_listCLAIMSURFACE.Num(); i++) {
      PCLAIMSURFACE pcs = (PCLAIMSURFACE) m_listCLAIMSURFACE.Get(i);
      PCMMLNode2 pSub;
      pSub = pNode->ContentAddNewNode ();
      if (!pSub)
         continue;
      pSub->NameSet (gpszClaimSurface);

      MMLValueSet (pSub, gpszReason, (int) pcs->dwReason);
      MMLValueSet (pSub, gpszID, (int) pcs->dwID);
      MMLValueSet (pSub, gpszAlsoEmbed, (int) pcs->fAlsoEmbed);
   }

   //WCHAR szTemp[64];
   fp f;
   for (i = 0; i < m_listFloorsA.Num(); i++) {
      swprintf (szTemp, gpszFloorsA, (int) i);
      f = *((fp*) m_listFloorsA.Get(i));
      MMLValueSet (pNode, szTemp, f);
   }
   for (i = 0; i < m_listFloorsB.Num(); i++) {
      swprintf (szTemp, gpszFloorsB, (int) i);
      f = *((fp*) m_listFloorsB.Get(i));
      MMLValueSet (pNode, szTemp, f);
   }

   return pNode;
}

BOOL CDoubleSurface::MMLFrom (PCMMLNode2 pNode)
{
   // member variables go here
   PCMMLNode2 pSub;
   PWSTR psz;
   DWORD i;
   m_listCLAIMSURFACE.Clear();
   for (i = 0; i < pNode->ContentNum(); i++) {
      pSub = NULL;
      pNode->ContentEnum (i, &psz, &pSub);
      if (!pSub)
         continue;
      psz = pSub->NameGet();
      if (!psz)
         continue;
      if (!_wcsicmp(psz, L"SplineCenter"))
         m_SplineCenter.MMLFrom (pSub);
      else if (!_wcsicmp(psz, L"SplineA"))
         m_SplineA.MMLFrom (pSub);
      else if (!_wcsicmp(psz, L"SplineB"))
         m_SplineB.MMLFrom (pSub);
      else if (!_wcsicmp(psz, L"Edge"))
         m_SplineEdge.MMLFrom (pSub);
      else if (!_wcsicmp(psz, gpszClaimSurface)) {
         // found a claim surface
         CLAIMSURFACE cs;
         memset (&cs, 0, sizeof(cs));

         cs.dwReason = (DWORD) MMLValueGetInt (pSub, gpszReason, 0);
         cs.dwID = (DWORD) MMLValueGetInt (pSub, gpszID, 0);
         cs.fAlsoEmbed = (BOOL) MMLValueGetInt (pSub, gpszAlsoEmbed, FALSE);
         m_listCLAIMSURFACE.Add (&cs);
         continue;
      }
   }

   // get bevel
   m_fBevelLeft = MMLValueGetDouble (pNode, gpszBevelLeft, 0);
   m_fBevelRight = MMLValueGetDouble (pNode, gpszBevelRight, 0);
   m_fBevelTop = MMLValueGetDouble (pNode, gpszBevelTop, 0);
   m_fBevelBottom = MMLValueGetDouble (pNode, gpszBevelBottom, 0);

   m_fConstBottom = (BOOL) MMLValueGetInt (pNode, gpszConstBottom, TRUE);
   m_fConstAbove = (BOOL) MMLValueGetInt (pNode, gpszConstAbove, TRUE);
   m_fConstRectangle = (BOOL) MMLValueGetInt (pNode, gpszConstRectangle, TRUE);

   // get thickness information
   m_fThickStud = MMLValueGetDouble (pNode, gpszThickStud, CM_THICKSTUDWALL);
   m_fThickA = MMLValueGetDouble (pNode, gpszThickA, CM_THICKSHEETROCK);
   m_fThickB = MMLValueGetDouble (pNode, gpszThickB, CM_THICKSHEETROCK);

   m_fHideSideA = (BOOL) MMLValueGetInt (pNode, gpszHideSideA, FALSE);
   m_fHideSideB = (BOOL) MMLValueGetInt (pNode, gpszHideSideB, FALSE);
   m_fHideEdges = (BOOL) MMLValueGetInt (pNode, gpszHideEdges, FALSE);
   m_dwExtSideA = (DWORD) MMLValueGetInt (pNode, gpszExtSideA, 1);
   m_dwExtSideB = (DWORD) MMLValueGetInt (pNode, gpszExtSideB, 0);

   m_dwType = (DWORD) MMLValueGetInt (pNode, gpszType, 0);
   MMLValueGetMatrix (pNode, gpszMatrixToObject, &m_MatrixToObject, NULL);
   MMLValueGetMatrix (pNode, gpszMatrixFromObject, &m_MatrixFromObject, NULL);

   // framing info
   m_fFramingRotate = (BOOL)MMLValueGetInt (pNode, gpszFramingRotate, FALSE);
   m_fFramingShowOnlyWhenVisible = (BOOL)MMLValueGetInt (pNode, gpszFramingShowOnlyWhenVisible, TRUE);
   DWORD j;
   WCHAR szTemp[64];
   for (i = 0; i < 3; i++) {
      swprintf (szTemp, gpszFramingShow, (int) i);
      m_afFramingShow[i] = (BOOL) MMLValueGetInt (pNode, szTemp, FALSE);

      swprintf (szTemp, gpszFramingInsetTL, (int) i);
      m_afFramingInsetTL[i] = (BOOL) MMLValueGetInt (pNode, szTemp, FALSE);

      swprintf (szTemp, gpszFramingShape, (int) i);
      m_adwFramingShape[i] = (DWORD)MMLValueGetInt (pNode, szTemp, NS_RECTANGLE);

      swprintf (szTemp, gpszFramingMinDist, (int) i);
      m_afFramingMinDist[i] = MMLValueGetDouble (pNode, szTemp, .9);

      swprintf (szTemp, gpszFramingScale, (int) i);
      MMLValueGetPoint (pNode, szTemp, &m_apFramingScale[i]);

      for (j = 0; j < 2; j++) {
         swprintf (szTemp, gpszFramingInset, (int) i, (int) j);
         m_afFramingInset[i][j] = MMLValueGetDouble (pNode, szTemp, 0);

         swprintf (szTemp, gpszFramingExtThick, (int) i, (int) j);
         m_afFramingExtThick[i][j] = MMLValueGetDouble (pNode, szTemp, 0);

         swprintf (szTemp, gpszFramingBevel, (int) i, (int)j);
         m_afFramingBevel[i][j] = MMLValueGetDouble (pNode, szTemp, 0);

         swprintf (szTemp, gpszFramingExtend, (int) i, (int)j);
         m_afFramingExtend[i][j] = MMLValueGetDouble (pNode, szTemp, 0);
      }
   }
   m_fFramingDirty = TRUE;


   //WCHAR szTemp[64];
   fp f;
   m_listFloorsA.Clear();
   m_listFloorsB.Clear();
   for (i = 0; ; i++) {
      swprintf (szTemp, gpszFloorsA, (int) i);
      f = MMLValueGetDouble (pNode, szTemp, -100);
      if (f == -100)
         break;
      m_listFloorsA.Add (&f);
   }
   for (i = 0; ; i++) {
      swprintf (szTemp, gpszFloorsB, (int) i);
      f = MMLValueGetDouble (pNode, szTemp, -100);
      if (f == -100)
         break;
      m_listFloorsB.Add (&f);
   }
   return TRUE;
}


/*******************************************************************************
CDoubleSurface::ControlNumGet - Returns the number of control points across or down.

inputs
   BOOL        fH - If TRUE then in H(across) direction, else V(down) direction
returns
   DWORD number
*/
DWORD CDoubleSurface::ControlNumGet (BOOL fH)
{
   return m_SplineCenter.ControlNumGet (fH);
}


/*******************************************************************************
CDoubleSurface::Side - Given an iSide (0 for center, 1 for A, -1 for B)
returns a PCSplineSurface, or NULL.

inputs
   int         iSide - 0 for center, 1 for A, -1 for B
returns
   PCSplineSurface
*/
PCSplineSurface CDoubleSurface::Side (int iSide)
{
  switch (iSide) {
   case 0:  
      return &m_SplineCenter;
      break;
   case 1:
      return &m_SplineA;
      break;
   case -1:
      return &m_SplineB;
      break;
   default:
      return NULL;
   }
}
/*******************************************************************************
CDoubleSurface::ControlPointGet - Given an H and V index into the control
pointers (see ContorlNumGet()), fills in a point with the value of the control
point.

inputs
   int         iSide - 0 for center, 1 for A, -1 for B
   DWORD       dwH, dwV - Index from 0..# pooints across - 1
   PCPoint     pVal - Filled in with the point. NOTE: This has been rotated to object space
returns
   BOOL - TRUE if succeded
*/
BOOL CDoubleSurface::ControlPointGet (int iSide, DWORD dwH, DWORD dwV, PCPoint pVal)
{
   PCSplineSurface pss = Side (iSide);
   if (!pss) {
      pVal->Zero();
      return FALSE;
   }

   if (!pss->ControlPointGet (dwH, dwV, pVal))
      return FALSE;
   pVal->MultiplyLeft (&m_MatrixToObject);

   return TRUE;
}

/*********************************************************************************
CDoubleSurface::EdgeOrigNumPointsGet - Returns the number of points originally passed into the
spline.
*/
DWORD CDoubleSurface::EdgeOrigNumPointsGet (void)
{
   return m_SplineEdge.OrigNumPointsGet();
}



/*********************************************************************************
CDoubleSurface::EdgeOrigPointGet - Fills in pP with the original point.

NOTE: Coordinates are not transformed because they are in HV.

inputs
   DWORD       dwH - from 0 to OrigNumPointsGet()-1
   PCPoint     pP - Filled with the original point
returns
   BOOL - TRUE if successful
*/
BOOL CDoubleSurface::EdgeOrigPointGet (DWORD dwH, PCPoint pP)
{
   return m_SplineEdge.OrigPointGet (dwH, pP);
}

/***********************************************************************************
CDoubleSurface::HVToInfo - Given an H and V, this will fill in the position,
normal, and texture.

inputs
   int         iSide - 0 for center, 1 for A, -1 for B
   fp      h, v - Location
   PCPoint     pPoint - Filled in with the point. Can be NULL.
   PCPoint     pNorm - Filled in with the normal. Can be NULL.
   PTEXTUREPOINT pText - Filled in with the texture. Can be NULL.
rturns
   BOOL - TRUE if point h,v inside. FALSE if not
*/
BOOL CDoubleSurface::HVToInfo (int iSide, fp h, fp v, PCPoint pPoint, PCPoint pNorm, PTEXTUREPOINT pText)
{
   PCSplineSurface pss = Side(iSide);
   if (!pss)
      return FALSE;

   if (pNorm)
      return FALSE;  // NOTE: Not doing inverse on normals

   if (!pss->HVToInfo (h, v, pPoint, pNorm, pText))
      return FALSE;

   if (pPoint)
      pPoint->MultiplyLeft (&m_MatrixToObject);

   return TRUE;
}


/*************************************************************************************
CDoubleSurface::ControlPointQuery - Called to query the information about a control
point identified by dwID.

inputs
   DWORD       dwColor - Control point color/surface. group
   DWORD       dwID - ID of control point
   POSCONTROL  pInfo - Filled in with the infomration
returns
   BOOL - TRUE if successful
*/
BOOL CDoubleSurface::ControlPointQuery (DWORD dwColor, DWORD dwID, POSCONTROL pInfo)
{
   PCLAIMSURFACE pcs = ClaimFindByID (dwColor);
   if (!pcs)
      return FALSE;
   fp fKnobSize = .2;

   PCSplineSurface pss;
   PWSTR pszName;
   PTEXTUREPOINT ptp;
   DWORD dwNum;
   BOOL fClockwise;
   pss = OverlayShapeFindByColor (dwColor, &pszName, &ptp, &dwNum, &fClockwise);
   if (!pss)
      return FALSE;

   memset (pInfo,0, sizeof(*pInfo));

   pInfo->dwID = dwID;
//   pInfo->dwFreedom = 0;   // any direction
   pInfo->dwStyle = CPSTYLE_CUBE;
   pInfo->fSize = fKnobSize / 2;
   pInfo->cColor = RGB(0x00,0xff,0xff);

   PWSTR pszPointName;
   TEXTUREPOINT HV;
   DWORD dwShapeID;
   dwShapeID = OverlayShapeNameToID(pszName);
   
   // convert this to center, width, and height
   CPoint pCenter;
   fp fWidth, fHeight;
   OverlayShapeFromPoints (dwShapeID, ptp, dwNum, &pCenter, &fWidth, &fHeight);

   HV.h = pCenter.p[0];
   HV.v = pCenter.p[1];
   pszPointName = 0;
   switch (dwShapeID) {
   case 0:  // rectangle
   case 1:  // ellipse
      switch (dwID) {
      case 0:
         // what have already is good enough
         pszPointName = L"Center";
         break;
      case 1:
         HV.h -= fWidth/2;
         pszPointName = L"Width";
         break;
      case 2:
         HV.v -= fHeight/2;
         pszPointName = L"Height";
         break;
      default:
         return FALSE;
      }
      break;

   case 2: // below
   case 3:  // above
   case 4:  // left
   case 5:  // right
      switch (dwID) {
      case 0:
         // what have already is good enoguh
         pszPointName = L"Edge";
         break;
      default:
         return FALSE;
      }
      break;

   case 6:  //hband
   case 7:  //vband
      switch (dwID) {
      case 0:
         // what have already is good enough
         pszPointName = L"Center";
         break;
      case 1:
         if (dwShapeID == 6)
            HV.v -= fHeight/2;
         else
            HV.h -= fWidth/2;
         pszPointName = L"Edge";
         break;
      default:
         return FALSE;
      }
      break;

   default:
      if (dwID >= dwNum)
         return FALSE;
      HV.h = ptp[dwID].h;
      HV.v = ptp[dwID].v;
      pszPointName = L"Point";
      break;
   }
   if (pszPointName)
      wcscpy (pInfo->szName, pszPointName);

   pss->HVToInfo (HV.h, HV.v, &pInfo->pLocation);
   pInfo->pLocation.MultiplyLeft (&m_MatrixToObject);

   return TRUE;

}

/*******************************************************************************
CDoubleSurface::ControlPointSet - Given an H and V index into the control
pointers (see ContorlNumGet()), changes the control point to something new.

inputs
   int         iSide - 0 for center, 1 for A, -1 for B
   DWORD       dwH, dwV - Index from 0..# pooints across - 1
   PCPoint     pVal - Use this point now
   DWORD       dwDivide - Number of times to divide
returns
   BOOL - TRUE if succeded
*/
BOOL CDoubleSurface::ControlPointSet (int iSide, DWORD dwH, DWORD dwV, PCPoint pVal)
{
   PCSplineSurface pss = Side(iSide);
   if (!pss)
      return FALSE;

   CPoint p;
   p.Copy (pVal);
   p.MultiplyLeft (&m_MatrixFromObject);

   if (!pss->ControlPointSet (dwH, dwV, &p))
      return FALSE;

   // modify the other sides
   ControlPointChanged (dwH, dwV, iSide);

   // set framing dirty
   m_fFramingDirty = TRUE;

   return TRUE;
}




/***************************************************************************************
CDoubleSurface::HVDrag - Utility function used to help dragging of control points
in a surface. It helps with control points stored as HV coordinates (0..1,0..1) relative
to the surface.

1) It runs a ray from the viewer to the new point (in object space) and
   sees where it intersects the surface, and that HV is returned.
2) If if doesn't intersect the surface then a triangle (unbounded on one end) is
   created with three points, the old HV converted to object space, the new control
   point location, and the viewer. This is intersected with the edge of the surface
   to find a hit point. (The closest one to the viewer is taken).
3) If nothing can be found then returns FALSE, meaning the drag should be ignored.
4) (Not done yet) Special case. If the surface is 2x2 (and flat) then the
   surface plane is allowed to extend beyond the edge of the normal bounds, and
   a value < 0 or > 1 may be returned for h and v.

inputs
   int         iSide - 0 for center, 1 for A, -1 for B
   PTEXTUREPOINT        pTextOld - Old H and 
   PCPoint              pNew - New Point, in object space. (Spline space actually)
   PCPoint              pViewer - Viewer, in object space. (Spline space actually)
   PTEXTUREPOINT        pTextNew - Filled with the new texture point if successful
returns
   BOOL - TRUE if success, FALSE if cant find intersect
*/
BOOL CDoubleSurface::HVDrag (int iSide, PTEXTUREPOINT pTextOld, PCPoint pNew, PCPoint pViewer, PTEXTUREPOINT pTextNew)
{
   PCSplineSurface pss = Side(iSide);
   if (!pss)
      return FALSE;

   CPoint pN, pV;
   pN.Copy (pNew);
   pN.MultiplyLeft (&m_MatrixFromObject);
   pV.Copy (pViewer);
   pV.MultiplyLeft (&m_MatrixFromObject);

   return pss->HVDrag (pTextOld, &pN, &pV, pTextNew);
}


/*********************************************************************************
CDoubleSurface::EdgeOrigSegCurveGet - Returns the segcurve descriptor... SEGCURVE_XXX, for
the index into the original points.

inputs
   DWORD       dwH - From 0 to OrigNumPointsGet()-1. an extra -1 if not looped
   DWORD       *padwSegCurve - Filled in with the segment curve
returns
   BOOL - TRUE if successful
*/
BOOL CDoubleSurface::EdgeOrigSegCurveGet (DWORD dwH, DWORD *pdwSegCurve)
{
   return m_SplineEdge.OrigSegCurveGet (dwH, pdwSegCurve);
}


/*********************************************************************************
CDoubleSurface::EdgeDivideGet - Fills in the divide information

inputs
   DWORD*      pdwMinDivide, pdwMaxDivide - If not null, filled in with min/max divide.
   fp*     pfDetail - If not null, filled in with detail
*/
void CDoubleSurface::EdgeDivideGet (DWORD *pdwMinDivide, DWORD *pdwMaxDivide, fp *pfDetail)
{
   m_SplineEdge.DivideGet (pdwMinDivide, pdwMaxDivide, pfDetail);
}


/***********************************************************************************
CDoubleSurface::EdgeInit - Initializes the spline.

inputs
   BOOL        fLooped - TRUE if the spline loops around on itself, FALSE if it starts
                  and ends at the first and last point.
   DWORD       dwPoints - Number of points in the spline.
   PCPoint     paPoints - Pointer to an array of dwPoints points for the spline control
                  points.
   PTEXTUREPOINT paTextures - Pointer to an array of dwPoints texture values. If this
                  is NULL then don't calculate textures. NOTE: If fLooped this must be
                  filled with dwPoints+1 points, so that know the texture of the first
                  point at the beginning of the loop and the end.
   DWORD       *padwSegCurve - Pointer to anarray of DWORDs describing the curve/spline
                  to use between this point and the next. Must have dwPoints elements.
                  This can also be set directly to one of the SEGCURVE_XXXX values
                  to use that SEGCURVE for all the segments. Has dwPoints unless is NOT
                  looped, then dwPoints-1.
   DWORD       dwMinDivide - Minimum number of times to divide each spline point.
                  0 => no times, 1 => halve, 2=>quarter, 3=>eiths, etc.
   DWORD       dwMaxDivide - Maximum number of times to divide each splint point
   fp      fDetail - Divide spline points until the longest length < fDetail,
                  or have divided dwMaxDivide times
returns
   BOOL - TRUE if succeded
*/
BOOL CDoubleSurface::EdgeInit (BOOL fLooped, DWORD dwPoints, PCPoint paPoints, PTEXTUREPOINT paTextures,
      DWORD *padwSegCurve, DWORD dwMinDivide, DWORD dwMaxDivide,
      fp fDetail)
{
   if (!m_SplineEdge.Init (fLooped, dwPoints, paPoints, paTextures, padwSegCurve,
      dwMinDivide, dwMaxDivide, fDetail))
      return FALSE;

   RecalcEdges ();


   // set framing dirty
   m_fFramingDirty = TRUE;

   return TRUE;
}
/*************************************************************************************
CDoubleSurface::ControlPointSet - Called to change the valud of a control point.

inputs
   DWORD       dwColor - Current surface being viewed
   DWORD       dwID - ID
   PCPoint     pVal - Contains the new location, in object coordinates
returns
   BOOL - TRUE if successful
*/
BOOL CDoubleSurface::ControlPointSet (DWORD dwColor, DWORD dwID, PCPoint pVal, PCPoint pViewer)
{
   PCLAIMSURFACE pcs = ClaimFindByID (dwColor);
   if (!pcs)
      return FALSE;

   // convert the points
   CPoint pValNew, pViewerNew;
   if (pVal) {
      pValNew.Copy (pVal);
      pValNew.MultiplyLeft (&m_MatrixFromObject);
      pVal = &pValNew;
   }
   if (pViewer) {
      pViewerNew.Copy (pViewer);
      pViewerNew.MultiplyLeft (&m_MatrixFromObject);
      pViewer = &pViewerNew;
   }

   PCSplineSurface pss;
   PWSTR pszName;
   PTEXTUREPOINT ptp;
   DWORD dwNum;
   BOOL fClockwise;
   pss = OverlayShapeFindByColor (dwColor, &pszName, &ptp, &dwNum, &fClockwise);
   if (!pss)
      return FALSE;

   TEXTUREPOINT HV;
   DWORD dwShapeID;
   dwShapeID = OverlayShapeNameToID(pszName);
   
   // convert this to center, width, and height
   CPoint pCenter;
   fp fWidth, fHeight;
   OverlayShapeFromPoints (dwShapeID, ptp, dwNum, &pCenter, &fWidth, &fHeight);

   // get the old spline point
   HV.h = pCenter.p[0];
   HV.v = pCenter.p[1];
   switch (dwShapeID) {
   case 0:  // rectangle
   case 1:  // ellipse
      switch (dwID) {
      case 0:
         // what have already is good enough
         break;
      case 1:
         HV.h -= fWidth/2;
         break;
      case 2:
         HV.v -= fHeight/2;
         break;
      default:
         return FALSE;
      }
      break;

   case 2: // below
   case 3:  // above
   case 4:  // left
   case 5:  // right
      switch (dwID) {
      case 0:
         // what have already is good enoguh
         break;
      default:
         return FALSE;
      }
      break;

   case 6:  //hband
   case 7:  //vband
      switch (dwID) {
      case 0:
         // what have already is good enough
         break;
      case 1:
         if (dwShapeID == 6)
            HV.v -= fHeight/2;
         else
            HV.h -= fWidth/2;
         break;
      default:
         return FALSE;
      }
      break;

   default:
      if (dwID >= dwNum)
         return FALSE;
      HV.h = ptp[dwID].h;
      HV.v = ptp[dwID].v;
      break;
   }

   // get the old spline point
   //CPoint pOldHV;
   //pss->HVToInfo (HV.h, HV.v, &pOldHV);
   TEXTUREPOINT tpOld, tpNew;
   //tpOld.h = pOldHV.p[0];
   //tpOld.v = pOldHV.p[1];
   // BUGFIX - Was doing something weird to get tpOld, which wasnt correct
   tpOld = HV;

   // find out where it intersects
   if (!pss->HVDrag (&tpOld, pVal, pViewer, &tpNew))
      return FALSE;  // doesnt intersect

   // tell the world we're about to change
   if (m_pTemplate->m_pWorld)
      m_pTemplate->m_pWorld->ObjectAboutToChange (m_pTemplate);

   switch (dwShapeID) {
   case 0:  // rectangle
   case 1:  // ellipse
      switch (dwID) {
      case 0:
         pCenter.p[0] = tpNew.h;
         pCenter.p[1] = tpNew.v;
         break;
      case 1:
         fWidth = fabs(pCenter.p[0] - tpNew.h) *2;
         break;
      case 2:
         fHeight = fabs(pCenter.p[1] - tpNew.v) *2;
         break;
      }
      break;

   case 2: // below
   case 3:  // above
   case 4:  // left
   case 5:  // right
      pCenter.p[0] = tpNew.h;
      pCenter.p[1] = tpNew.v;
      break;

   case 6:  //hband
   case 7:  //vband
      switch (dwID) {
      case 0:
         pCenter.p[0] = tpNew.h;
         pCenter.p[1] = tpNew.v;
         break;
      case 1:
         if (dwShapeID == 6)
            fHeight = fabs(pCenter.p[1] - tpNew.v) *2;
         else
            fWidth = fabs(pCenter.p[0] - tpNew.h) *2;
         break;
      default:
         return FALSE;
      }
      break;
   }
   PCListFixed pl;
   if (dwShapeID >= 8) { // handle 8 (any shape), or -1 for some other type
      pl = new CListFixed;
      if (pl) {
         pl->Init (sizeof(TEXTUREPOINT), ptp, dwNum);
         PTEXTUREPOINT pt;
         pt = (PTEXTUREPOINT) pl->Get(0);
         if (dwID < pl->Num())
            pt[dwID] = tpNew;
      }
   }
   else {
      pl = OverlayShapeToPoints (dwShapeID, &pCenter, fWidth, fHeight);
   }
   WCHAR szTemp[128];
   wcscpy (szTemp, pszName);
   if (pl) {
      pss->OverlaySet (szTemp, (PTEXTUREPOINT)pl->Get(0), pl->Num(), fClockwise);
      delete pl;
   }


   // set framing dirty
   m_fFramingDirty = TRUE;

   // tell the world we've changed
   if (m_pTemplate->m_pWorld)
      m_pTemplate->m_pWorld->ObjectChanged (m_pTemplate);

   return TRUE;
}


/*************************************************************************************
CDoubleSurface::ControlPointEnum - Called to enumerate a list of control point IDs
into the list.

inputs
   DWORD             dwColor - Color surface ID
   PCListFixed       plDWORD - Should be filled in with control points
returns
   none
*/
void CDoubleSurface::ControlPointEnum (DWORD dwColor, PCListFixed plDWORD)
{
   // 6 control points starting at 10
   DWORD i;
   PCSplineSurface pss;
   PWSTR pszName;
   PTEXTUREPOINT ptp;
   DWORD dwNum;
   BOOL fClockwise;
   pss = OverlayShapeFindByColor (dwColor, &pszName, &ptp, &dwNum, &fClockwise);
   if (!pss)
      return;

   DWORD dwPoints;
   dwPoints = 0;
   switch (OverlayShapeNameToID(pszName)) {
   case 0:  // rectangle
   case 1:  // ellipse
      dwPoints = 3;
      break;
   case 2: // belo
   case 3:  // above
   case 4:  // left
   case 5:  // right
      dwPoints = 1;
      break;
   case 6:  //hband
   case 7:  //vband
      dwPoints = 2;
      break;
   default:
      dwPoints = dwNum;
      break;
   }

   plDWORD->Required (plDWORD->Num() + dwPoints);
   for (i = 0; i < dwPoints; i++)
      plDWORD->Add (&i);
}



/*************************************************************************************
CDoubleSurface::BevelPoint - Given a control point on one of three surfaces, A, B, or Center,
this uses the bevel angles to calculate what the point should be on the other surface.

inputs
   DWORD       dwX, dwY - Control point index. Gets the point from the appropriate spline.
   int         iSideOrig - Side that measurement is from. 1 for A, 0 for center, -1 for B
   PCPoint     pNew - Filled with the point on the new side (in CDoubleSurface coordinates)
   int         iSideNew - Side to conver to, 1 for A, 0 ,for ceneter, -1 for B.
                  Either iSideOrig or iSideNew must be 0. Cannot convert more than two sides
                  They cant be the same value, since there would be no conversion
   BOOL        fOvershoot - If set to TRUE, will overshoot the mark (when generating an A
                  or B side) by .01m. Use this to intersect walls against floors, since
                  theoretically, walls won't go through floors.
returns
   BOOL - TRUE is success
*/


BOOL CDoubleSurface::BevelPoint (DWORD dwX, DWORD dwY, int iSideOrig,
                                    PCPoint pNew, int iSideNew, BOOL fOvershoot)
{
   // first off, if this is beyond the edge of control points then error
   DWORD dwWidth, dwHeight;
   dwWidth = ControlNumGet(TRUE);
   dwHeight = ControlNumGet(FALSE);
   if ((dwX >= dwWidth) || (dwY >= dwHeight))
      return FALSE;

   // make sure sides are OK
   if ((iSideOrig < -1) || (iSideOrig > 1) || (iSideNew < -1) || (iSideNew > 1))
      return FALSE;
   if (iSideOrig == iSideNew)
      return FALSE;
   if (abs(iSideOrig - iSideNew) >= 2)
      return FALSE;

   // get the point and the normals at that point
   CPoint   pOrig, pN, pH, pV;
   PCSplineSurface pss;
   fp fDepthOrig, fDepthNew;
   switch (iSideOrig) {
   case -1:
      pss = &m_SplineB;
      fDepthOrig = m_fThickStud / 2 + m_fThickB;
      break;
   case 0:
      pss = &m_SplineCenter;
      fDepthOrig = 0;
      break;
   case 1:
      pss = &m_SplineA;
      fDepthOrig = -(m_fThickStud / 2 + m_fThickA);
      break;
   }
   if (!pss->ControlPointGet (dwX, dwY, &pOrig))
      return FALSE;
   if (!pss->ControlPointsToNormals (dwX, dwY, &pN, &pH, &pV))
      return FALSE;

   // if dealing with side B then slip pH and pN since the mesh is
   // flipped horizontally to ensure backface culling ok
   if (iSideOrig == -1) {
      pN.Scale (-1);
      pH.Scale (-1);
   }

   // depth of new
   switch (iSideNew) {
   case -1:
      fDepthNew = m_fThickStud / 2 + m_fThickB;
      if (fOvershoot)
         fDepthNew += .02;
      break;
   case 0:
      fDepthNew = 0;
      break;
   case 1:
      fDepthNew = -(m_fThickStud / 2 + m_fThickA);
      if (fOvershoot)
         fDepthNew -= .02;
      break;
   }
   fp fDepth;
   fDepth = fDepthOrig - fDepthNew;

   // interpolate angles
   fp fAngleH, fAngleV;
   fAngleH = (fp)dwX / (fp)(dwWidth-1);

   // if dealing with side B then flip the angle since the mesh is flipped horinztaonlly
   if (iSideOrig == -1)
      fAngleH = 1.0 - fAngleH;

   fAngleH = fAngleH * m_fBevelRight + (1.0 - fAngleH) * (-m_fBevelLeft);
      // note: flipping sign of left and top so same angle's cause to bevel inwards
      // or outwards at same time
   fAngleH = max(fAngleH, -PI/2*.95);   // dont left it be too steep
   fAngleH = min(fAngleH, PI/2*.95);   // dont let it be too steep
   fAngleV = (fp)dwY / (fp)(dwHeight-1);
   fAngleV = fAngleV * m_fBevelBottom + (1.0 - fAngleV) * (-m_fBevelTop);
      // note: flipping sign of left and top so same angle's cause to bevel inwards
      // or outwards at same time
   fAngleV = max(fAngleV, -PI/2*.95);   // dont left it be too steep
   fAngleV = min(fAngleV, PI/2*.95);   // dont let it be too steep

   // find out delta based on this
   pH.Scale (tan(fAngleH) * fDepth);
   pV.Scale (tan(fAngleV) * fDepth);
   pN.Scale (fDepth);

   // the rest is easy.  Just add the offsets
   pNew->Copy (&pOrig);
   pNew->Add (&pH);
   pNew->Add (&pV);
   pNew->Add (&pN);

   return TRUE;
}




/*************************************************************************************
CDoubleSurface::ControlPointChanged - Call this when a control point has changed
in either side A, B, or the center. It's assumed that the change has already been
written into the side that was changed. This change is then translated into the
center spline. From there, both sides (A and B) are recalculated.

inputs
   DWORD          dwX, dwY - Control point location. This should already have been changed
                     in the given spline
   int            iSide - 1 for side A, 0 for center, -1 for side B.
returns
   BOOL - TRUE if success
*/
BOOL CDoubleSurface::ControlPointChanged (DWORD dwX, DWORD dwY, int iSide)
{

   // set framing dirty
   m_fFramingDirty = TRUE;

   // translate this to the center
   if (iSide) {
      // take bevelling and thickness into account
      CPoint p;
      if (!BevelPoint (dwX, dwY, iSide, &p, 0))
         return FALSE;

      // if it's side B then flip dwX because side B has coordintes reversed
      if (iSide < 0)
         dwX = ControlNumGet(TRUE) - dwX - 1;

      // write it in
      if (!m_SplineCenter.ControlPointSet (dwX, dwY, &p))
         return FALSE;
   }

   // if there are any contraints then apply those
   if (m_fConstAbove || m_fConstBottom || m_fConstRectangle) {
      DWORD dwWidth, dwHeight;
      dwWidth = ControlNumGet (TRUE) - 1;
      dwHeight = ControlNumGet (FALSE) - 1;

      CPoint p;
      m_SplineCenter.ControlPointGet (dwX, dwY, &p);

      // if bottom contraint then keep z=0 on the bottom
      if (m_fConstBottom && (dwY == dwHeight))
         p.p[2] = 0; // always keep z == 0

#if 0
      // if constraint that must be in plane then force that
      if (m_fConstPlane && ((dwY == dwHeight) || (dwY == 0)) && ((dwX == dwWidth) || (dwX==0) ))
         p.p[1] = 0;
#endif //0 


      // if constraint that top must be above bottom then move both in tandem
      if (m_fConstAbove) {
         if (dwY == 0) {
            // set the below value too
            CPoint p2;
            m_SplineCenter.ControlPointGet (dwX, dwHeight, &p2);
            p2.p[0] = p.p[0];
            p2.p[1] = p.p[1];
            m_SplineCenter.ControlPointSet (dwX, dwHeight, &p2);
         }
         else if (dwY == dwHeight) {
            // set the above value too
            CPoint p2;
            m_SplineCenter.ControlPointGet (dwX, 0, &p2);
            p2.p[0] = p.p[0];
            p2.p[1] = p.p[1];
            m_SplineCenter.ControlPointSet (dwX, 0, &p2);
         }
      }

      // if other points must be in rectangle then move two others in tandem
      if (m_fConstRectangle && ((dwY == 0) || (dwY == dwHeight)) && ((dwX == 0) || (dwX == dwWidth))) {
         DWORD dwOtherY, dwOtherX;
         dwOtherY = (dwY == 0) ? dwHeight : 0;
         dwOtherX = (dwX == 0) ? dwWidth : 0;
         CPoint p2;

         // set the other Y's value
         m_SplineCenter.ControlPointGet (dwX, dwOtherY, &p2);
         p2.p[0] = p.p[0];
         p2.p[1] = p.p[1];
         m_SplineCenter.ControlPointSet (dwX, dwOtherY, &p2);

         // set the other x's calue
         m_SplineCenter.ControlPointGet (dwOtherX, dwY, &p2);
         p2.p[1] = p.p[1];
         p2.p[2] = p.p[2];
         m_SplineCenter.ControlPointSet (dwOtherX, dwY, &p2);

         // and totally opposite corner
         m_SplineCenter.ControlPointGet (dwOtherX, dwOtherY, &p2);
         p2.p[1] = p.p[1];
         m_SplineCenter.ControlPointSet (dwOtherX, dwOtherY, &p2);
      }

      m_SplineCenter.ControlPointSet (dwX, dwY, &p);
   }

   // recalc both sides
   return RecalcSides ();
}

/*************************************************************************************
CDoubleSurface::SurfaceOvershootForIntersect - Returns a CSplineSurface object that's
to be used for intersection detection. The surface is either side A or B, but
it is specifically "overshot" so that it will intersect with surfaces adjacent to it.

  NOTE: The coordinates are in the same transformation as the CDoubleSurface object.

inputs
   BOOL        fSideA - If TRUE get side A, FALSE side B
returns
   PCSplineSurface - This must be deleted by the caller.
*/
PCSplineSurface CDoubleSurface::SurfaceOvershootForIntersect (BOOL fSideA)
{
   PCSplineSurface pNewSS = new CSplineSurface;
   if (!pNewSS)
      return NULL;

   // how much space do we need
   DWORD i, x, y;
   DWORD dwWidth, dwHeight;
   DWORD *padwSegCurveHA, *padwSegCurveHB, *padwSegCurveV;
   CMem  mem, memHA, memHB, memV;
   PCPoint  pNew;
   dwWidth = ControlNumGet(TRUE);
   dwHeight = ControlNumGet(FALSE);
   if (!mem.Required(dwWidth * dwHeight * sizeof(CPoint))) {
      delete pNewSS;
      return NULL;
   }
   pNew = (PCPoint) mem.p;
   if (!memHA.Required(dwWidth * sizeof(DWORD))) {
      delete pNewSS;
      return NULL;
   }
   padwSegCurveHA = (DWORD*) memHA.p;
   if (!memHB.Required(dwWidth * sizeof(DWORD))) {
      delete pNewSS;
      return NULL;
   }
   padwSegCurveHB = (DWORD*) memHB.p;
   if (!memV.Required(dwHeight * sizeof(DWORD))) {
      delete pNewSS;
      return NULL;
   }
   padwSegCurveV = (DWORD*) memV.p;

   // get some values used to initialize
   //fp m[2][2];
   DWORD dwMinDivideH, dwMaxDivideH, dwMinDivideV, dwMaxDivideV;
   fp fDetail;
   //m_SplineCenter.TextureInfoGet (m);
   m_SplineCenter.DetailGet (&dwMinDivideH, &dwMaxDivideH, &dwMinDivideV, &dwMaxDivideV, &fDetail);
   DWORD dwSegments;
   dwSegments = dwWidth - (m_SplineCenter.LoopGet (TRUE) ? 0 : 1);
   for (i = 0; i < dwSegments; i++) {
      DWORD dwVal;
      dwVal = padwSegCurveHA[i] = m_SplineCenter.SegCurveGet (TRUE, i);

      // for side B flip left to right, and also take care to flip circle and ellipse
      switch (dwVal) {
      case SEGCURVE_CIRCLEPREV:
         dwVal = SEGCURVE_CIRCLENEXT;
         break;
      case SEGCURVE_CIRCLENEXT:
         dwVal = SEGCURVE_CIRCLEPREV;
         break;
      case SEGCURVE_ELLIPSEPREV:
         dwVal = SEGCURVE_ELLIPSENEXT;
         break;
      case SEGCURVE_ELLIPSENEXT:
         dwVal = SEGCURVE_ELLIPSEPREV;
         break;
      }
      padwSegCurveHB[dwSegments - i - 1] = dwVal;
   }
   for (i = 0; i < dwHeight; i++)
      padwSegCurveV[i] = m_SplineCenter.SegCurveGet (FALSE, i);

   // do side A and b
   // if doing side B then flip horizontally
   for (y = 0; y < dwHeight; y++) for (x = 0; x < dwWidth; x++) {
      BevelPoint (x, y, 0, pNew + (((!fSideA) ? (dwWidth - x - 1): x) + y * dwWidth), fSideA ? 1 : -1, TRUE);
   }

   // use this to initialize
   if (!pNewSS->ControlPointsSet (m_SplineCenter.LoopGet(TRUE), m_SplineCenter.LoopGet(FALSE),
      dwWidth, dwHeight, pNew, (!fSideA) ? padwSegCurveHB : padwSegCurveHA, padwSegCurveV)) {
      delete pNewSS;
      return NULL;
   }

   //pss->TextureInfoSet (m);

   pNewSS->DetailSet (dwMinDivideH, dwMaxDivideH, dwMinDivideV, dwMaxDivideV, fDetail);

   return pNewSS;
}



/*************************************************************************************
CDoubleSurface::RecalcSides - Recalculate side A and B from the center spline.

IMPORTANT: This does NOT call the world to tell it the object is or has changed.

inputs
   none
returns
   BOOL - TRUE if success
*/
BOOL CDoubleSurface::RecalcSides (void)
{
   // how much space do we need
   DWORD i, x, y;
   DWORD dwWidth, dwHeight;
   DWORD *padwSegCurveHA, *padwSegCurveHB, *padwSegCurveV;
   CMem  mem, memHA, memHB, memV;
   PCPoint  pNew;
   dwWidth = ControlNumGet(TRUE);
   dwHeight = ControlNumGet(FALSE);
   if (!mem.Required(dwWidth * dwHeight * sizeof(CPoint)))
      return FALSE;
   pNew = (PCPoint) mem.p;
   if (!memHA.Required(dwWidth * sizeof(DWORD)))
      return FALSE;
   padwSegCurveHA = (DWORD*) memHA.p;
   if (!memHB.Required(dwWidth * sizeof(DWORD)))
      return FALSE;
   padwSegCurveHB = (DWORD*) memHB.p;
   if (!memV.Required(dwHeight * sizeof(DWORD)))
      return FALSE;
   padwSegCurveV = (DWORD*) memV.p;


   // set framing dirty
   m_fFramingDirty = TRUE;

   // get some values used to initialize
   //fp m[2][2];
   DWORD dwMinDivideH, dwMaxDivideH, dwMinDivideV, dwMaxDivideV;
   fp fDetail;
   //m_SplineCenter.TextureInfoGet (m);
   m_SplineCenter.DetailGet (&dwMinDivideH, &dwMaxDivideH, &dwMinDivideV, &dwMaxDivideV, &fDetail);
   DWORD dwSegments;
   dwSegments = dwWidth - (m_SplineCenter.LoopGet (TRUE) ? 0 : 1);
   for (i = 0; i < dwSegments; i++) {
      DWORD dwVal;
      dwVal = padwSegCurveHA[i] = m_SplineCenter.SegCurveGet (TRUE, i);

      // for side B flip left to right, and also take care to flip circle and ellipse
      switch (dwVal) {
      case SEGCURVE_CIRCLEPREV:
         dwVal = SEGCURVE_CIRCLENEXT;
         break;
      case SEGCURVE_CIRCLENEXT:
         dwVal = SEGCURVE_CIRCLEPREV;
         break;
      case SEGCURVE_ELLIPSEPREV:
         dwVal = SEGCURVE_ELLIPSENEXT;
         break;
      case SEGCURVE_ELLIPSENEXT:
         dwVal = SEGCURVE_ELLIPSEPREV;
         break;
      }
      padwSegCurveHB[dwSegments - i - 1] = dwVal;
   }
   for (i = 0; i < dwHeight; i++)
      padwSegCurveV[i] = m_SplineCenter.SegCurveGet (FALSE, i);

   // do side A and b
   // if doing side B then flip horizontally
   for (i = 0; i < 2; i++) {  // i == 0 then side A, i==1 then side B
      for (y = 0; y < dwHeight; y++) for (x = 0; x < dwWidth; x++) {
         if (!BevelPoint (x, y, 0, pNew + (((i==1) ? (dwWidth - x - 1): x) + y * dwWidth), i ? -1 : 1))
            return FALSE;
      }

      // use this to initialize
      PCSplineSurface pss;
      pss = i ? &m_SplineB : &m_SplineA;
      if (!pss->ControlPointsSet (m_SplineCenter.LoopGet(TRUE), m_SplineCenter.LoopGet(FALSE),
         dwWidth, dwHeight, pNew, (i==1) ? padwSegCurveHB : padwSegCurveHA, padwSegCurveV))
         return FALSE;

      //pss->TextureInfoSet (m);

      pss->DetailSet (dwMinDivideH, dwMaxDivideH, dwMinDivideV, dwMaxDivideV, fDetail);
   }

   // tell the embedded objects that they've moved
   TellEmbeddedThatMoved();

   return TRUE;
}

/*************************************************************************************
CDoubleSurface::TellEmbeddedThatMoved - Calls all the embedded objects within this
and tells the embedded objects that they've been moved somehow.

inputs
   none
returns
   none
*/
void CDoubleSurface::TellEmbeddedThatMoved (void)
{

   // tell all the objects they've moved so they end up redoing the cutouts
   DWORD dwNum, i;
   TEXTUREPOINT tp;
   fp fRotation;
   DWORD dwSurface;
   GUID gEmbed;

   dwNum = m_pTemplate->ContEmbeddedNum();
   if (m_pTemplate->m_pWorld) for (i = 0; i < dwNum; i++) {
      if (!m_pTemplate->ContEmbeddedEnum (i, &gEmbed))
         continue;
      if (!m_pTemplate->ContEmbeddedLocationGet (&gEmbed, &tp, &fRotation, &dwSurface))
         continue;

      // if this is not one of this objects surfaces then ignore
      if (!ClaimFindByID (dwSurface))
         continue;

      // calculate the new matrix and call into object
      CMatrix m;
      m.Identity();
      m_pTemplate->ContMatrixFromHV (dwSurface, &tp, fRotation, &m);
      PCObjectSocket pos;
      pos = m_pTemplate->m_pWorld->ObjectGet(m_pTemplate->m_pWorld->ObjectFind(&gEmbed));
      if (pos)
         pos->EmbedMovedWithinContainer (dwSurface, &tp, fRotation, &m);
   }

}

/*************************************************************************************
CDoubleSurface::RecalcEdges - Recalculate edge cutouts side A and B from the edge spline.

IMPORTANT: This does NOT call the world to tell it the object is or has changed.

inputs
   none
returns
   BOOL - TRUE if success
*/
BOOL CDoubleSurface::RecalcEdges (void)
{

   // set framing dirty
   m_fFramingDirty = TRUE;

   // allocate enough memory for all the points
   DWORD i, dwNum;
   dwNum = m_SplineEdge.QueryNodes ();
   CMem mem;
   if (!mem.Required (dwNum * sizeof(TEXTUREPOINT)))
      return FALSE;
   PTEXTUREPOINT pt;
   pt = (PTEXTUREPOINT)mem.p;
   PCPoint pp;
   for (i = 0; i < dwNum; i++) {
      pp = m_SplineEdge.LocationGet (i);
      if (!pp)
         return FALSE;

      // x and y values go to h and v
      // flipping rotation order so will be inclusive area
      pt[dwNum - i - 1].h = pp->p[0];
      pt[dwNum - i - 1].v = pp->p[1];
   }

   // set this on side A
   m_SplineA.CutoutSet (L"Edge", pt, dwNum, FALSE);

   // BUGFIX - Set the center spline cutout here so that intellisensed wall will not
   // go through the floor and affect flooring above.
   m_SplineCenter.CutoutSet (L"Edge", pt, dwNum, FALSE);

   // loop and flip h value for side B
   for (i = 0; i < dwNum; i++) {
      pp = m_SplineEdge.LocationGet (i);
      if (!pp)
         return FALSE;

      // x and y values go to h and v
      // flipping h because on side b
      pt[i].h = 1.0 - pp->p[0];
      pt[i].v = pp->p[1];
   }
   m_SplineB.CutoutSet (L"Edge", pt, dwNum, FALSE);

   return TRUE;
}


/*******************************************************************************
CDoubleSurface::DetailGet - Gets the current detail vlaues.

inputs
   DWORD*      pdwMin/MaxDivideH/V - Filled in the value. Can be null
   fp*     pfDetail - Filled in with detail. Can be null
returns
   non
*/
void CDoubleSurface::DetailGet (DWORD *pdwMinDivideH, DWORD *pdwMaxDivideH, DWORD *pdwMinDivideV, DWORD *pdwMaxDivideV,
   fp *pfDetail)
{
   m_SplineCenter.DetailGet (pdwMinDivideH, pdwMaxDivideH, pdwMinDivideV, pdwMaxDivideV, pfDetail);
}



/*************************************************************************************
CDoubleSurface::DetailSet - Sets the detail information for drawing the surface.

inputs
   DWORD          dwMinDivideH - Minimum number of divides that will do horizontal
   DWORD          dwMaxDivideH - Maximum number of divides that will do horizontal
   DWORD          dwMinDivideV - Minimum number of divides that will do vertical
   DWORD          dwMaxDivideV - Maximum number of divides that will do vertical
   fp         fDetail - Divide until every spline distance is less than this
returns
   BOOL - TRUE if succeded
*/
BOOL CDoubleSurface::DetailSet (DWORD dwMinDivideH, DWORD dwMaxDivideH, DWORD dwMinDivideV, DWORD dwMaxDivideV,
   fp fDetail)
{
   if (!m_SplineCenter.DetailSet (dwMinDivideH, dwMaxDivideH, dwMinDivideV, dwMaxDivideV, fDetail))
      return FALSE;
   RecalcSides();

   // set framing dirty
   m_fFramingDirty = TRUE;

   return TRUE;
}


/*******************************************************************************
CDoubleSurface::SegCurveGet - Gets the curvature of the segment at the given index.

inputs
   BOOL        fH - If TRUE use the horizontal segment, FALSE use vertical
   DWORD       dwIndex - Index into segment. From 0 .. ControlNumGet()-1.
returns
   DWORD - Curve. 0 if failed.
*/
DWORD CDoubleSurface::SegCurveGet (BOOL fH, DWORD dwIndex)
{
   return m_SplineCenter.SegCurveGet (fH, dwIndex);
}


/*******************************************************************************
CDoubleSurface::EnforceConstraints - Call this if one of the constraints flags
was set to true. It goes through the points and enforces some flattness.

inputs
   none
returns
   none
*/
void CDoubleSurface::EnforceConstraints (void)
{
   if (m_fConstBottom) {
      // set all the bottom z values to 0
      DWORD dwHeight, dwWidth, x;
      CPoint pPoint;
      dwHeight = ControlNumGet (FALSE);
      dwWidth = ControlNumGet (TRUE);

      for (x = 0; x < dwWidth; x++) {
         m_SplineCenter.ControlPointGet (x, dwHeight-1, &pPoint);
         pPoint.p[2] = 0;  // set z value to 0
         m_SplineCenter.ControlPointSet (x, dwHeight-1, &pPoint);
      }

   }

   if (m_fConstAbove) {
      // set all the bottom z values to 0
      DWORD dwHeight, dwWidth, x;
      CPoint pPoint, pPointBottom;
      dwHeight = ControlNumGet (FALSE);
      dwWidth = ControlNumGet (TRUE);

      for (x = 0; x < dwWidth; x++) {
         m_SplineCenter.ControlPointGet (x, 0, &pPoint);
         m_SplineCenter.ControlPointGet (x, dwHeight-1, &pPointBottom);
         pPoint.p[0] = pPointBottom.p[0]; // x and y are the same as bottom
         pPoint.p[1] = pPointBottom.p[1];
         m_SplineCenter.ControlPointSet (x, 0, &pPoint);
      }

   }
   if (m_fConstRectangle) {
      // set all the bottom z values to 0
      DWORD dwHeight, dwWidth;
      CPoint p, p2;
      dwHeight = ControlNumGet (FALSE) - 1;
      dwWidth = ControlNumGet (TRUE) - 1;

      // use lower left as reference
      m_SplineCenter.ControlPointGet (0, dwHeight, &p);

      // modify lower right corner
      m_SplineCenter.ControlPointGet (dwWidth, dwHeight, &p2);
      p2.p[1] = p.p[1];
      p2.p[2] = p.p[2];
      m_SplineCenter.ControlPointSet (dwWidth, dwHeight, &p2);

      // modify uppler left corner
      m_SplineCenter.ControlPointGet (0, 0, &p2);
      p2.p[0] = p.p[0];
      p2.p[1] = p.p[1];
      m_SplineCenter.ControlPointSet (0, 0, &p2);

      // mdify upper right corner
      p.Copy (&p2);
      m_SplineCenter.ControlPointGet (dwWidth, 0, &p2);
      p2.p[1] = p.p[1];
      p2.p[2] = p.p[2];
      m_SplineCenter.ControlPointSet (dwWidth, 0, &p2);

   }

   RecalcSides();
}



/*******************************************************************************
CDoubleSurface::SegCurveSet - Sets the curvature of the segment at the given index.

inputs
   BOOL        fH - If TRUE use the horizontal segment, FALSE use vertical
   DWORD       dwIndex - Index into segment. From 0 .. ControlNumGet()-1.
   DWORD       dwValue - new value
returns
   BOOL - TRUE if success
*/
BOOL CDoubleSurface::SegCurveSet (BOOL fH, DWORD dwIndex, DWORD dwValue)
{
   if (!m_SplineCenter.SegCurveSet (fH, dwIndex, dwValue))
      return FALSE;

   RecalcSides();

   // set framing dirty
   m_fFramingDirty = TRUE;

   return TRUE;
}



/* SurfaceDetailPage
*/
BOOL SurfaceDetailPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCDoubleSurface pv = (PCDoubleSurface)pPage->m_pUserData;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         DWORD dwMinDivideH, dwMaxDivideH, dwMinDivideV, dwMaxDivideV;
         fp fDetail;
         pv->DetailGet (&dwMinDivideH, &dwMaxDivideH, &dwMinDivideV, &dwMaxDivideV, &fDetail);

         MeasureToString (pPage, L"detail", fDetail);

         ESCMCOMBOBOXSELECTSTRING s;
         PCEscControl pControl;
         WCHAR szTemp[2];
         szTemp[1] = 0;
         memset (&s, 0, sizeof(s));
         s.fExact = TRUE;
         s.iStart = -1;
         s.psz = szTemp;

         // set them
         szTemp[0] = L'0' + (WCHAR) dwMinDivideH;
         pControl = pPage->ControlFind (L"minh");
         if (pControl)
            pControl->Message (ESCM_COMBOBOXSELECTSTRING, &s);
         szTemp[0] = L'0' + (WCHAR) dwMinDivideV;
         pControl = pPage->ControlFind (L"minv");
         if (pControl)
            pControl->Message (ESCM_COMBOBOXSELECTSTRING, &s);
         szTemp[0] = L'0' + (WCHAR) dwMaxDivideH;
         pControl = pPage->ControlFind (L"maxh");
         if (pControl)
            pControl->Message (ESCM_COMBOBOXSELECTSTRING, &s);
         szTemp[0] = L'0' + (WCHAR) dwMaxDivideV;
         pControl = pPage->ControlFind (L"maxv");
         if (pControl)
            pControl->Message (ESCM_COMBOBOXSELECTSTRING, &s);

      }
      break;

   case ESCN_EDITCHANGE:
      {
         PESCNEDITCHANGE p = (PESCNEDITCHANGE) pParam;

         if (!p->pControl || !p->pControl->m_pszName)
            break;

         if (!_wcsicmp(p->pControl->m_pszName, L"detail")) {
            DWORD dwMinDivideH, dwMaxDivideH, dwMinDivideV, dwMaxDivideV;
            fp fDetail;
            pv->DetailGet (&dwMinDivideH, &dwMaxDivideH, &dwMinDivideV, &dwMaxDivideV, &fDetail);
            pv->m_pTemplate->m_pWorld->ObjectAboutToChange (pv->m_pTemplate);
            MeasureParseString (pPage, p->pControl->m_pszName, &fDetail);
            fDetail = max(0.01, fDetail);
            pv->DetailSet (dwMinDivideH, dwMaxDivideH, dwMinDivideV, dwMaxDivideV, fDetail);
            pv->m_pTemplate->m_pWorld->ObjectChanged (pv->m_pTemplate);
         }
      }
      break;

   case ESCN_COMBOBOXSELCHANGE:
      {
         PESCNCOMBOBOXSELCHANGE p = (PESCNCOMBOBOXSELCHANGE) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;

         DWORD dwMinDivideH, dwMaxDivideH, dwMinDivideV, dwMaxDivideV;
         fp fDetail;
         pv->DetailGet (&dwMinDivideH, &dwMaxDivideH, &dwMinDivideV, &dwMaxDivideV, &fDetail);

         DWORD dwTemp;
         if (!_wcsicmp(p->pControl->m_pszName, L"minh")) {
            dwTemp = p->pszName[0] - L'0';
            if (dwTemp == dwMinDivideH)
               break;   // no change
            dwMinDivideH = dwTemp;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"maxh")) {
            dwTemp = p->pszName[0] - L'0';
            if (dwTemp == dwMaxDivideH)
               break;   // no change
            dwMaxDivideH = dwTemp;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"minv")) {
            dwTemp = p->pszName[0] - L'0';
            if (dwTemp == dwMinDivideV)
               break;   // no change
            dwMinDivideV = dwTemp;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"maxv")) {
            dwTemp = p->pszName[0] - L'0';
            if (dwTemp == dwMaxDivideV)
               break;   // no change
            dwMaxDivideV = dwTemp;
         }
         else
            break;

         pv->m_pTemplate->m_pWorld->ObjectAboutToChange (pv->m_pTemplate);
         pv->DetailSet (dwMinDivideH, dwMaxDivideH, dwMinDivideV, dwMaxDivideV, fDetail);
         pv->m_pTemplate->m_pWorld->ObjectChanged (pv->m_pTemplate);
      }
      return 0;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Curvature detail";
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}


/* SurfaceThicknessPage
*/
BOOL SurfaceThicknessPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCDoubleSurface pv = (PCDoubleSurface)pPage->m_pUserData;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         MeasureToString (pPage, L"stud", pv->m_fThickStud);
         MeasureToString (pPage, L"sidea", pv->m_fThickA);
         MeasureToString (pPage, L"sideb", pv->m_fThickB);
      }
      break;

   case ESCN_EDITCHANGE:
      {
         PESCNEDITCHANGE p = (PESCNEDITCHANGE) pParam;

         if (!p->pControl || !p->pControl->m_pszName)
            break;

         if (!_wcsicmp(p->pControl->m_pszName, L"stud")) {
            pv->m_pTemplate->m_pWorld->ObjectAboutToChange (pv->m_pTemplate);
            MeasureParseString (pPage, p->pControl->m_pszName, &pv->m_fThickStud);
            pv->m_fThickStud = max(0, pv->m_fThickStud);
            pv->RecalcSides();
            pv->m_pTemplate->m_pWorld->ObjectChanged (pv->m_pTemplate);
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"sidea")) {
            pv->m_pTemplate->m_pWorld->ObjectAboutToChange (pv->m_pTemplate);
            MeasureParseString (pPage, p->pControl->m_pszName, &pv->m_fThickA);
            pv->m_fThickA = max(0, pv->m_fThickA);
            pv->RecalcSides();
            pv->m_pTemplate->m_pWorld->ObjectChanged (pv->m_pTemplate);
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"sideb")) {
            pv->m_pTemplate->m_pWorld->ObjectAboutToChange (pv->m_pTemplate);
            MeasureParseString (pPage, p->pControl->m_pszName, &pv->m_fThickB);
            pv->m_fThickB = max(0, pv->m_fThickB);
            pv->RecalcSides();
            pv->m_pTemplate->m_pWorld->ObjectChanged (pv->m_pTemplate);
         }
      }
      break;
   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Thickness";
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}


/* SurfaceBevellingPage
*/
BOOL SurfaceBevellingPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCDoubleSurface pv = (PCDoubleSurface)pPage->m_pUserData;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         AngleToControl (pPage, L"left", pv->m_fBevelLeft, TRUE);
         AngleToControl (pPage, L"right", pv->m_fBevelRight, TRUE);
         AngleToControl (pPage, L"top", pv->m_fBevelTop, TRUE);
         AngleToControl (pPage, L"bottom", pv->m_fBevelBottom, TRUE);
      }
      break;

   case ESCN_EDITCHANGE:
      {
         PESCNEDITCHANGE p = (PESCNEDITCHANGE) pParam;

         if (!p->pControl || !p->pControl->m_pszName)
            break;

         if (!_wcsicmp(p->pControl->m_pszName, L"left")) {
            pv->m_pTemplate->m_pWorld->ObjectAboutToChange (pv->m_pTemplate);
            pv->m_fBevelLeft = AngleFromControl (pPage, p->pControl->m_pszName);
            pv->m_fBevelLeft = max(pv->m_fBevelLeft, -PI/2*.95);
            pv->m_fBevelLeft = min(pv->m_fBevelLeft, PI/2*.95);
            pv->RecalcSides();
            pv->m_pTemplate->m_pWorld->ObjectChanged (pv->m_pTemplate);
         }
         if (!_wcsicmp(p->pControl->m_pszName, L"right")) {
            pv->m_pTemplate->m_pWorld->ObjectAboutToChange (pv->m_pTemplate);
            pv->m_fBevelRight = AngleFromControl (pPage, p->pControl->m_pszName);
            pv->m_fBevelRight = max(pv->m_fBevelRight, -PI/2*.95);
            pv->m_fBevelRight = min(pv->m_fBevelRight, PI/2*.95);
            pv->RecalcSides();
            pv->m_pTemplate->m_pWorld->ObjectChanged (pv->m_pTemplate);
         }
         if (!_wcsicmp(p->pControl->m_pszName, L"top")) {
            pv->m_pTemplate->m_pWorld->ObjectAboutToChange (pv->m_pTemplate);
            pv->m_fBevelTop = AngleFromControl (pPage, p->pControl->m_pszName);
            pv->m_fBevelTop = max(pv->m_fBevelTop, -PI/2*.95);
            pv->m_fBevelTop = min(pv->m_fBevelTop, PI/2*.95);
            pv->RecalcSides();
            pv->m_pTemplate->m_pWorld->ObjectChanged (pv->m_pTemplate);
         }
         if (!_wcsicmp(p->pControl->m_pszName, L"bottom")) {
            pv->m_pTemplate->m_pWorld->ObjectAboutToChange (pv->m_pTemplate);
            pv->m_fBevelBottom = AngleFromControl (pPage, p->pControl->m_pszName);
            pv->m_fBevelBottom = max(pv->m_fBevelBottom, -PI/2*.95);
            pv->m_fBevelBottom = min(pv->m_fBevelBottom, PI/2*.95);
            pv->RecalcSides();
            pv->m_pTemplate->m_pWorld->ObjectChanged (pv->m_pTemplate);
         }
      }
      break;
   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Bevelling";
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}


/* SurfaceCurvaturePage
*/
BOOL SurfaceCurvaturePage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCDoubleSurface pv = (PCDoubleSurface)pPage->m_pUserData;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         PCEscControl pControl;
         DWORD i;
         WCHAR szTemp[16];
         DWORD dwNum;

         // set the comboboxes to their current values
         dwNum = pv->ControlNumGet (FALSE) - 1;  // row
         for (i = 0; i < dwNum; i++) {
            swprintf (szTemp, L"xr%d", (int)i);
            pControl = pPage->ControlFind (szTemp);
            if (pControl)
               pControl->AttribSetInt (gszCurSel, (int) pv->SegCurveGet (FALSE, i));
         }
         dwNum = pv->ControlNumGet (TRUE) - 1;  // row
         for (i = 0; i < dwNum; i++) {
            swprintf (szTemp, L"xc%d", (int)i);
            pControl = pPage->ControlFind (szTemp);
            if (pControl)
               pControl->AttribSetInt (gszCurSel, (int) pv->SegCurveGet (TRUE, i));
         }

         // set thecheckboxes
         pControl = pPage->ControlFind (L"bottom");
         if (pControl)
            pControl->AttribSetBOOL (gszChecked, pv->m_fConstBottom);
         pControl = pPage->ControlFind (L"above");
         if (pControl)
            pControl->AttribSetBOOL (gszChecked, pv->m_fConstAbove);
         pControl = pPage->ControlFind (L"rectangle");
         if (pControl)
            pControl->AttribSetBOOL (gszChecked, pv->m_fConstRectangle);
      }
      break;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;

         // if it's for the colors then do those
         if (!_wcsicmp(p->pControl->m_pszName, L"bottom")) {
            pv->m_pTemplate->m_pWorld->ObjectAboutToChange (pv->m_pTemplate);
            pv->m_fConstBottom = p->pControl->AttribGetBOOL (gszChecked);

            if (pv->m_fConstBottom) {
               pv->EnforceConstraints ();
            }
            pv->m_pTemplate->m_pWorld->ObjectChanged (pv->m_pTemplate);
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"above")) {
            pv->m_pTemplate->m_pWorld->ObjectAboutToChange (pv->m_pTemplate);
            pv->m_fConstAbove = p->pControl->AttribGetBOOL (gszChecked);

            if (pv->m_fConstAbove) {
               pv->EnforceConstraints();
            }
            pv->m_pTemplate->m_pWorld->ObjectChanged (pv->m_pTemplate);
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"rectangle")) {
            pv->m_pTemplate->m_pWorld->ObjectAboutToChange (pv->m_pTemplate);
            pv->m_fConstRectangle = p->pControl->AttribGetBOOL (gszChecked);

            if (pv->m_fConstRectangle) {
               pv->EnforceConstraints ();
            }
            pv->m_pTemplate->m_pWorld->ObjectChanged (pv->m_pTemplate);
            return TRUE;
         }
#if 0
         else if (!_wcsicmp(p->pControl->m_pszName, L"plane")) {
            pv->m_pTemplate->m_pWorld->ObjectAboutToChange (pv->m_pTemplate);
            pv->m_fConstPlane = p->pControl->AttribGetBOOL (gszChecked);

            if (pv->m_fConstPlane) {
               // set all the Y values to 0 around the edges
               DWORD dwHeight, dwWidth, x;
               CPoint pPoint;
               dwHeight = pv->ControlNumGet (FALSE);
               dwWidth = pv->ControlNumGet (TRUE);

               // corners
               for (x = 0; x < dwWidth; x += dwWidth-1) {
                  // left
                  pv->m_SplineCenter.ControlPointGet (x, 0, &pPoint);
                  pPoint.p[1] = 0;
                  pv->m_SplineCenter.ControlPointSet (x, 0, &pPoint);

                  // right
                  pv->m_SplineCenter.ControlPointGet (x, dwHeight-1, &pPoint);
                  pPoint.p[1] = 0;
                  pv->m_SplineCenter.ControlPointSet (x, dwHeight-1, &pPoint);
               }

               pv->RecalcSides();
            }
            pv->m_pTemplate->m_pWorld->ObjectChanged (pv->m_pTemplate);
            return TRUE;
         }
#endif // 0
         // dont need to refresh

      }
      break;   // default

   case ESCN_COMBOBOXSELCHANGE:
      {
         PESCNCOMBOBOXSELCHANGE p = (PESCNCOMBOBOXSELCHANGE) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;

         // only care about "xr" and "xc"
         PWSTR psz;
         psz = p->pControl->m_pszName;
         if (psz[0] != L'x')
            break;
         BOOL fRow;
         if (psz[1] == L'r')
            fRow = TRUE;
         else if (psz[1] == L'c')
            fRow = FALSE;
         else
            break;
         DWORD dwIndex;
         dwIndex = (DWORD) _wtoi (psz + 2);
         
         // see if it hasn't change
         DWORD dwCur;
         dwCur = pv->SegCurveGet (!fRow, dwIndex);
         if (dwCur == (DWORD)p->dwCurSel)
            return TRUE;   // no change

         // else changed
         pv->m_pTemplate->m_pWorld->ObjectAboutToChange (pv->m_pTemplate);
         pv->SegCurveSet (!fRow, dwIndex, p->dwCurSel);
         //pv->RecalcSides();
         pv->m_pTemplate->m_pWorld->ObjectChanged (pv->m_pTemplate);
         return TRUE;
      }
      return 0;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Specify curvature";
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"COLUMN") || !_wcsicmp(p->pszSubName, L"ROW")) {
            BOOL fRow = !_wcsicmp(p->pszSubName, L"ROW");
            DWORD dwNum = pv->ControlNumGet(!fRow) - 1;
            DWORD i;

            MemZero (&gMemTemp);
            for (i = 0; i < dwNum; i++) {
               MemCat (&gMemTemp, L"<tr><td width=50%>");
               MemCat (&gMemTemp, fRow ? L"Row " : L"Column ");
               MemCat (&gMemTemp, (int)i + 1);
               if (i == 0)
                  MemCat (&gMemTemp, fRow ? L" (top)" : L" (left)");
               if (i == dwNum-1)
                  MemCat (&gMemTemp, fRow ? L" (bottom)" : L" (right)");
               MemCat (&gMemTemp, L"</td><td width=50%><xcombodetail name=x");
               MemCat (&gMemTemp, fRow ? L"r" : L"c");
               MemCat (&gMemTemp, (int) i);
               MemCat (&gMemTemp, L"/></td></tr>");
            }
            p->pszSubString = (PWSTR) gMemTemp.p;
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}


/* SurfaceShowCladPage
*/
BOOL SurfaceShowCladPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCDoubleSurface pv = (PCDoubleSurface)pPage->m_pUserData;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         PCEscControl pControl;

         // set thecheckboxes
         pControl = pPage->ControlFind (L"sidea");
         if (pControl)
            pControl->AttribSetBOOL (gszChecked, pv->m_fHideSideA);
         pControl = pPage->ControlFind (L"sideb");
         if (pControl)
            pControl->AttribSetBOOL (gszChecked, pv->m_fHideSideB);
         pControl = pPage->ControlFind (L"edges");
         if (pControl)
            pControl->AttribSetBOOL (gszChecked, pv->m_fHideEdges);
         pControl = pPage->ControlFind (L"extsidea");
         if (pControl)
            pControl->AttribSetBOOL (gszChecked, pv->m_dwExtSideA);
         pControl = pPage->ControlFind (L"extsideb");
         if (pControl)
            pControl->AttribSetBOOL (gszChecked, pv->m_dwExtSideB);
      }
      break;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;

         // if it's for the colors then do those
         if (!_wcsicmp(p->pControl->m_pszName, L"sidea")) {
            pv->m_pTemplate->m_pWorld->ObjectAboutToChange (pv->m_pTemplate);
            pv->m_fHideSideA = p->pControl->AttribGetBOOL (gszChecked);
            pv->m_pTemplate->m_pWorld->ObjectChanged (pv->m_pTemplate);
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"sideb")) {
            pv->m_pTemplate->m_pWorld->ObjectAboutToChange (pv->m_pTemplate);
            pv->m_fHideSideB = p->pControl->AttribGetBOOL (gszChecked);
            pv->m_pTemplate->m_pWorld->ObjectChanged (pv->m_pTemplate);
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"edges")) {
            pv->m_pTemplate->m_pWorld->ObjectAboutToChange (pv->m_pTemplate);
            pv->m_fHideEdges = p->pControl->AttribGetBOOL (gszChecked);
            pv->m_pTemplate->m_pWorld->ObjectChanged (pv->m_pTemplate);
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"extsidea")) {
            pv->m_pTemplate->m_pWorld->ObjectAboutToChange (pv->m_pTemplate);
            pv->m_dwExtSideA = p->pControl->AttribGetBOOL (gszChecked) ? 1 : 0;
            pv->RecalcSides();   // so message sent to embedded objects
            pv->m_pTemplate->m_pWorld->ObjectChanged (pv->m_pTemplate);
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"extsideb")) {
            pv->m_pTemplate->m_pWorld->ObjectAboutToChange (pv->m_pTemplate);
            pv->m_dwExtSideB = p->pControl->AttribGetBOOL (gszChecked) ? 1 : 0;
            pv->RecalcSides();   // so message sent to embedded objects
            pv->m_pTemplate->m_pWorld->ObjectChanged (pv->m_pTemplate);
            return TRUE;
         }
         // dont need to refresh

      }
      break;   // default


   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Cladding";
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}



/* SurfaceFloorsPage
*/
BOOL SurfaceFloorsPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCDoubleSurface pv = (PCDoubleSurface)pPage->m_pUserData;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         CMatrix m1, m2, m;
         pv->MatrixGet (&m1);
         pv->m_pTemplate->ObjectMatrixGet (&m2);
         m.Multiply (&m2, &m1);

         // initialize from side A
         DWORD i;
         WCHAR szTemp[32];
         fp *paf;
         paf = (fp*) pv->m_listFloorsA.Get(0);
         for (i = 0; i < pv->m_listFloorsA.Num(); i++) {
            swprintf (szTemp, L"floor%d", i);

            // get this location
            CPoint p;
            pv->m_SplineA.HVToInfo (.5, paf[i], &p);
            p.MultiplyLeft (&m);

            MeasureToString (pPage, szTemp, p.p[2]);
         }
      }
      break;

   case ESCN_EDITCHANGE:
      {
         PESCNEDITCHANGE p = (PESCNEDITCHANGE) pParam;
         PWSTR szFloor = L"floor";
         int iLen = (DWORD)wcslen(szFloor);

         if (!p->pControl || !p->pControl->m_pszName)
            break;

         // if it's for the colors then do those
         if (!wcsncmp(p->pControl->m_pszName, szFloor, iLen)) {
            pv->m_pTemplate->m_pWorld->ObjectAboutToChange (pv->m_pTemplate);

            // inverse to go from world space to object space
            CMatrix m1, m2, m, mInv;
            pv->MatrixGet (&m1);
            pv->m_pTemplate->ObjectMatrixGet (&m2);
            m.Multiply (&m2, &m1);
            m.Invert4 (&mInv);

            // find out where half way between is
            CPoint pHalfA, pHalfB;
            pv->m_SplineB.HVToInfo (.5, .5, &pHalfB);
            pv->m_SplineA.HVToInfo (.5, .5, &pHalfA);

            // clear the old ones
            pv->m_listFloorsA.Clear();
            pv->m_listFloorsB.Clear();

            // loop
            DWORD i;
            WCHAR szTemp[32];
            fp f;
            for (i = 0; i < 10; i++) {
               swprintf (szTemp, L"floor%d", i);

               if (!MeasureParseString (pPage, szTemp, &f))
                  continue;

               // convert this point to object space
               CPoint p;
               p.Zero();
               p.p[2] = f;
               p.MultiplyLeft (&mInv);

               // see where this intersects with side A
               CPoint pStart, pDir;
               TEXTUREPOINT tp;
               pStart.Copy (&pHalfA);
               pStart.p[2] = p.p[2];
               pDir.Zero();
               pDir.p[1] = -1;   // since surfaces generally flat in y
               if (!pv->m_SplineA.IntersectLine (&pStart, &pDir, &tp, FALSE, FALSE))
                  tp.v = 1;   // assume was at the base
               pv->m_listFloorsA.Add (&tp.v);

               // b side
               pStart.Copy (&pHalfB);
               pStart.p[2] = p.p[2];
               pDir.Zero();
               pDir.p[1] = -1;   // since surfaces generally flat in y
               if (!pv->m_SplineB.IntersectLine (&pStart, &pDir, &tp, FALSE, FALSE))
                  tp.v = 1;
               pv->m_listFloorsB.Add (&tp.v);
            }


            
            pv->m_pTemplate->m_pWorld->ObjectChanged (pv->m_pTemplate);
            return TRUE;
         }

      }
      break;   // default


   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Floors";
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}

/********************************************************************************
DSGenerateThreeDFromSplineSurface - Given a spline surface, this sets a threeD
control with the surface.

inputs
   PWSTR       pszControl - Control name.
   PCEscPage   pPage - Page
   PCSplineSurface pss - Surface
   DWORD       dwUse - 0 for control add/remove by column, 1 for control add/remove by row
returns
   BOOl - TRUE if success

NOTE: The ID's are such:
   LOBYTE = x
   2nd lowest byte = y
   3rd lowest byte = 1 for polygon, 2 for line
*/
BOOL DSGenerateThreeDFromSplineSurface (PWSTR pszControl, PCEscPage pPage, PCSplineSurface pss, DWORD dwUse)
{
   PCEscControl pControl = pPage->ControlFind (pszControl);
   if (!pControl)
      return FALSE;

   // figure out the center
   CPoint pCenter, pTemp, pMin, pMax;
   DWORD i;
   pCenter.Zero();
   pMin.Zero();
   pMax.Zero();
   DWORD x, y, dwWidth, dwHeight;
   dwWidth = pss->ControlNumGet (TRUE);
   dwHeight = pss->ControlNumGet (FALSE);
   for (y = 0; y < dwHeight; y++) for (x = 0; x < dwWidth; x++) {
      if (!pss->ControlPointGet (x, y, &pTemp))
         return FALSE;
      if ((y == 0) && (x == 0)) {
         pMin.Copy (&pTemp);
         pMax.Copy (&pTemp);
         continue;
      }

      // else, do min/max
      for (i = 0; i < 3; i++) {
         pMin.p[i] = min(pMin.p[i], pTemp.p[i]);
         pMax.p[i] = max(pMax.p[i], pTemp.p[i]);
      }
   }
   pCenter.Copy (&pMin);
   pCenter.Add (&pMax);
   pCenter.Scale (.5);

   // figure out the maximum distance
   fp fTemp, fMax;
   fMax = 0.001;
   for (y = 0; y < dwHeight; y++) for (x = 0; x < dwWidth; x++) {
      if (!pss->ControlPointGet (x, y, &pTemp))
         return FALSE;
      pTemp.Subtract (&pCenter);
      fTemp = pTemp.Length();
      if (fTemp > fMax)
         fMax = fTemp;
   }
   fMax /= 5;  // so is larger

   // when draw points, get the point, subtract the center, and divide by fMax

   // use gmemtemp
   MemZero (&gMemTemp);
   MemCat (&gMemTemp, L"<rotatex val=-90/><backculloff/>");   //  so that Z is up (on the screen)

   // polygons for segments
   WCHAR szTemp[128];
   DWORD dwYMax, dwXMax;
   // BUGFIX - Deal with loop check
   dwXMax = dwWidth - (pss->LoopGet(TRUE) ? 0 : 1);
   dwYMax = dwHeight - (pss->LoopGet(FALSE) ? 0 : 1);
   for (y = 0; y < dwYMax; y++) for (x = 0; x < dwXMax; x++) {

      // set the ID
      MemCat (&gMemTemp, L"<id val=");
      MemCat (&gMemTemp, (int)((1 << 16) | (y << 8) | x));
      MemCat (&gMemTemp, L"/>");

      // Change color from light to dark?
      BOOL fLight;
      fLight = TRUE;
      if ((dwUse == 0) && (x % 2))
         fLight = FALSE;
      else if ((dwUse == 1) && (y%2))
         fLight = FALSE;
      MemCat (&gMemTemp, fLight ? L"<colordefault color=#c0c0c0/>" : L"<colordefault color=#808080/>");

      MemCat (&gMemTemp, L"<shapepolygon");

      for (i = 0; i < 4; i++) {
         switch (i) {
         case 0: // UL
            pss->ControlPointGet (x,y, &pTemp);
            break;
         case 1: // UR
            pss->ControlPointGet ((x+1) % dwWidth,y, &pTemp);
            break;
         case 2: // LR
            pss->ControlPointGet ((x+1) % dwWidth, (y+1) % dwHeight, &pTemp);
            break;
         case 3: // LL
            pss->ControlPointGet (x, (y+1) % dwHeight, &pTemp);
            break;
         }
         pTemp.Subtract (&pCenter);
         pTemp.Scale (1.0 / fMax);

         swprintf (szTemp, L" p%d=%g,%g,%g",
            (int) i+1, (double)pTemp.p[0], (double)pTemp.p[1], (double)pTemp.p[2]);
         MemCat (&gMemTemp, szTemp);
      }

      MemCat (&gMemTemp, L"/>");
   }

   // Do row/column line
   MemCat (&gMemTemp, L"<colordefault color=#ff0000/>");
   if (dwUse == 0) {
      // draw columns lines
      for (x = (pss->LoopGet(TRUE) ? 0 : 1); x < dwXMax; x++) {
         // set the ID
         MemCat (&gMemTemp, L"<id val=");
         MemCat (&gMemTemp, (int)((2 << 16) | x));
         MemCat (&gMemTemp, L"/>");

         MemCat (&gMemTemp, L"<shapearrow tip=false width=.2");

         for (y = 0; y < dwHeight; y++) {
            pss->ControlPointGet (x, y, &pTemp);
            pTemp.Subtract (&pCenter);
            pTemp.Scale (1.0 / fMax);

            swprintf (szTemp, L" p%d=%g,%g,%g",
               (int) y+1, (double)pTemp.p[0], (double)pTemp.p[1], (double)pTemp.p[2]);
            MemCat (&gMemTemp, szTemp);
         }

         MemCat (&gMemTemp, L"/>");
      }
   }
   else if (dwUse ==1 ) {
      // draw row lines
      for (y = (pss->LoopGet(FALSE) ? 0 : 1); y < dwYMax; y++) {
         // set the ID
         MemCat (&gMemTemp, L"<id val=");
         MemCat (&gMemTemp, (int)((2 << 16) | (y << 8)));
         MemCat (&gMemTemp, L"/>");

         MemCat (&gMemTemp, L"<shapearrow tip=false width=.2");

         for (x = 0; x < dwWidth; x++) {
            pss->ControlPointGet (x, y, &pTemp);
            pTemp.Subtract (&pCenter);
            pTemp.Scale (1.0 / fMax);

            swprintf (szTemp, L" p%d=%g,%g,%g",
               (int) x+1, (double)pTemp.p[0], (double)pTemp.p[1], (double)pTemp.p[2]);
            MemCat (&gMemTemp, szTemp);
         }

         MemCat (&gMemTemp, L"/>");
      }
   }

   // set the threeD control
   ESCMTHREEDCHANGE tc;
   memset (&tc, 0, sizeof(tc));
   tc.pszMML = (PWSTR) gMemTemp.p;
   pControl->Message (ESCM_THREEDCHANGE, &tc);

   return TRUE;
}


/* SurfaceControlPointsPage
*/
BOOL SurfaceControlPointsPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCDoubleSurface pv = (PCDoubleSurface)pPage->m_pUserData;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         // draw images
         DSGenerateThreeDFromSplineSurface (L"row", pPage, &pv->m_SplineCenter, 1);
         DSGenerateThreeDFromSplineSurface (L"column", pPage, &pv->m_SplineCenter, 0);
      }
      break;

   case ESCN_THREEDCLICK:
      {
         PESCNTHREEDCLICK p = (PESCNTHREEDCLICK) pParam;
         if (!p->pControl->m_pszName)
            break;

         BOOL  fCol;
         if (!_wcsicmp(p->pControl->m_pszName, L"column"))
            fCol = TRUE;
         else if (!_wcsicmp(p->pControl->m_pszName, L"row"))
            fCol = FALSE;
         else
            break;

         // figure out x, y, and what clicked on
         DWORD x, y, dwMode;
         dwMode = (BYTE)(p->dwMajor >> 16);
         x = (BYTE) p->dwMajor;
         y = (BYTE)(p->dwMajor >> 8);
         if ((dwMode < 1) || (dwMode > 2))
            break;

         // allocate enough memory so can do the calculations
         DWORD dwWidth, dwHeight;
         dwWidth = pv->ControlNumGet(TRUE);
         dwHeight = pv->ControlNumGet(FALSE);
         switch (dwMode) {
         case 1:  // insert row column
            if (fCol)
               dwWidth++;
            else
               dwHeight++;
            break;
         case 2:  // delete row/column
            if (fCol)
               dwWidth--;
            else
               dwHeight--;
            break;
         }

         DWORD dwTotal;
         dwTotal = dwWidth * dwHeight;
         if (!dwTotal)
            return FALSE;
         CMem memPoints, memSegCurveH, memSegCurveV;
         if (!memPoints.Required (dwTotal * sizeof(CPoint)))
            return FALSE;
         if (!memSegCurveH.Required ((dwWidth-1) * sizeof(DWORD)))
            return FALSE;
         if (!memSegCurveV.Required ((dwHeight-1) * sizeof(DWORD)))
            return FALSE;
         PCPoint pPoints;
         DWORD *padwSegCurveH, *padwSegCurveV;
         pPoints = (PCPoint) memPoints.p;
         padwSegCurveH = (DWORD*) memSegCurveH.p;
         padwSegCurveV = (DWORD*) memSegCurveV.p;

         // tell world about to change
         pv->m_pTemplate->m_pWorld->ObjectAboutToChange (pv->m_pTemplate);

         // fill in the new values
         DWORD i, j;
         CPoint pTemp, pTemp2;
         switch (dwMode) {
         case 1:  // insert row/column
            {
               for (i = 0; i < dwWidth; i++) for (j = 0; j < dwHeight; j++) {
                  DWORD dwOrigX,dwOrigY;
                  dwOrigX = i;
                  dwOrigY = j;
                  if (fCol) {
                     if (i == x+1) {
                        // do an average of the points
                        pv->m_SplineCenter.ControlPointGet (dwOrigX, dwOrigY, &pTemp);
                        pv->m_SplineCenter.ControlPointGet (dwOrigX-1, dwOrigY, &pTemp2);
                        pTemp.Add (&pTemp2);
                        pTemp.Scale (.5);

                        // VERSION2 - probably want to do actual spline calc
                     }
                     else {
                        // just get this value
                        if (dwOrigX > x)
                           dwOrigX--;

                        pv->m_SplineCenter.ControlPointGet (dwOrigX, dwOrigY, &pTemp);
                     }
                  }
                  else {
                     if (j == y+1) {
                        // do an average of the points
                        pv->m_SplineCenter.ControlPointGet (dwOrigX, dwOrigY, &pTemp);
                        pv->m_SplineCenter.ControlPointGet (dwOrigX, dwOrigY-1, &pTemp2);
                        pTemp.Add (&pTemp2);
                        pTemp.Scale (.5);

                        // VERSION2 - probably want to do actual spline calc
                     }
                     else {
                        // just get this value
                        if (dwOrigY > y)
                           dwOrigY--;

                        pv->m_SplineCenter.ControlPointGet (dwOrigX, dwOrigY, &pTemp);
                     }
                  }

                  // write out pTemp
                  pPoints[j * dwWidth + i].Copy (&pTemp);
               }

               // do the curvature
               for (i = 0; i < dwWidth-1; i++) {
                  if (fCol && (i > x))
                     padwSegCurveH[i] = pv->m_SplineCenter.SegCurveGet (TRUE, i-1);
                  else
                     padwSegCurveH[i] = pv->m_SplineCenter.SegCurveGet (TRUE, i);
               }
               for (i = 0; i < dwHeight-1; i++) {
                  if (!fCol && (i > y))
                     padwSegCurveV[i] = pv->m_SplineCenter.SegCurveGet (FALSE, i-1);
                  else
                     padwSegCurveV[i] = pv->m_SplineCenter.SegCurveGet (FALSE, i);
               }

               // Call API to readjust all stored H & V values
               fp afOrig[2], afNew[2];
               if (fCol) {
                  afOrig[0] = (fp)x / (fp)(dwWidth-2);
                  afOrig[1] = (fp)(x+1) / (fp)(dwWidth-2);
                  afNew[0] = (fp)x / (fp)(dwWidth-1);
                  afNew[1] = (fp)(x+2) / (fp)(dwWidth-1);
                  pv->ChangedHV (2, 0, afOrig, NULL, afNew, NULL);
               }
               else {
                  afOrig[0] = (fp)y / (fp)(dwHeight-2);
                  afOrig[1] = (fp)(y+1) / (fp)(dwHeight-2);
                  afNew[0] = (fp)y / (fp)(dwHeight-1);
                  afNew[1] = (fp)(y+2) / (fp)(dwHeight-1);
                  pv->ChangedHV (0, 2, NULL, afOrig, NULL, afNew);
               }
            }
            break;

         case 2:  // delete row/column
            {
               for (i = 0; i < dwWidth; i++) for (j = 0; j < dwHeight; j++) {
                  DWORD dwOrigX,dwOrigY;
                  dwOrigX = i;
                  dwOrigY = j;
                  if (fCol) {
                     // just get this value
                     if (dwOrigX >= x)
                        dwOrigX++;

                     pv->m_SplineCenter.ControlPointGet (dwOrigX, dwOrigY, &pTemp);
                  }
                  else {
                     // just get this value
                     if (dwOrigY >= y)
                        dwOrigY++;

                     pv->m_SplineCenter.ControlPointGet (dwOrigX, dwOrigY, &pTemp);
                  }

                  // write out pTemp
                  pPoints[j * dwWidth + i].Copy (&pTemp);
               }

               // do the curvature
               for (i = 0; i < dwWidth-1; i++) {
                  if (fCol && (i >= x))
                     padwSegCurveH[i] = pv->m_SplineCenter.SegCurveGet (TRUE, i+1);
                  else
                     padwSegCurveH[i] = pv->m_SplineCenter.SegCurveGet (TRUE, i);
               }
               for (i = 0; i < dwHeight-1; i++) {
                  if (!fCol && (i >= y))
                     padwSegCurveV[i] = pv->m_SplineCenter.SegCurveGet (FALSE, i+1);
                  else
                     padwSegCurveV[i] = pv->m_SplineCenter.SegCurveGet (FALSE, i);
               }

               // Call API to readjust all stored H & V values
               // IMPORTANT: There's a fundamental flaw with this... if remove
               // a column/row that's evenly divided it works great. However, if there's
               // an uneven divide it has no way of properly handling the change.
               // The only real way is the remember all the previous points in object space,
               // divide, and find their closest point in the new surfaces HV space
               // once the divide has occurred
               fp afOrig[2], afNew[2];
               if (fCol) {
                  afOrig[0] = (fp)(x-1) / (fp)(dwWidth+0);
                  afOrig[1] = (fp)(x+1) / (fp)(dwWidth+0);
                  afNew[0] = (fp)(x-1) / (fp)(dwWidth-1);
                  afNew[1] = (fp)x / (fp)(dwWidth-1);
                  pv->ChangedHV (2, 0, afOrig, NULL, afNew, 0);
               }
               else {
                  afOrig[0] = (fp)(y-1) / (fp)(dwHeight+0);
                  afOrig[1] = (fp)(y+1) / (fp)(dwHeight+0);
                  afNew[0] = (fp)(y-1) / (fp)(dwHeight-1);
                  afNew[1] = (fp)y / (fp)(dwHeight-1);
                  pv->ChangedHV (0, 2, NULL, afOrig, NULL, afNew);
               }
            }
            break;
         }

         // set new value
         pv->m_SplineCenter.ControlPointsSet (FALSE, FALSE, dwWidth, dwHeight,
            pPoints, padwSegCurveH, padwSegCurveV);

         // changed
         pv->RecalcSides();
         pv->m_pTemplate->m_pWorld->ObjectChanged (pv->m_pTemplate);

         // redraw the shapes
         DSGenerateThreeDFromSplineSurface (L"row", pPage, &pv->m_SplineCenter, 1);
         DSGenerateThreeDFromSplineSurface (L"column", pPage, &pv->m_SplineCenter, 0);
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Add or remove control point";
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}


/********************************************************************************
DSGenerateThreeDFromSpline - Given a spline on a surface, this sets a threeD
control with the spline.

inputs
   PWSTR       pszControl - Control name.
   PCEscPage   pPage - Page
   PCSpline    pSpline - Spline to draw
   PCSplineSurface pss - Surface
   DWORD       dwUse - If 0 it's for adding/remove splines, else if 1 it's for cycling curves
returns
   BOOl - TRUE if success

NOTE: The ID's are such:
   LOBYTE = x
   3rd lowest byte = 1 for edge, 2 for point
*/
BOOL DSGenerateThreeDFromSpline (PWSTR pszControl, PCEscPage pPage, PCSpline pSpline, PCSplineSurface pss,
                                      DWORD dwUse)
{
   PCEscControl pControl = pPage->ControlFind (pszControl);
   if (!pControl)
      return FALSE;

   // figure out the center
   CPoint pCenter, pTemp, pMin, pMax;
   DWORD i;
   pCenter.Zero();
   pMin.Zero();
   pMax.Zero();
   DWORD x, y, dwWidth, dwHeight;
   dwWidth = pss->ControlNumGet (TRUE);
   dwHeight = pss->ControlNumGet (FALSE);
   for (y = 0; y < dwHeight; y++) for (x = 0; x < dwWidth; x++) {
      if (!pss->ControlPointGet (x, y, &pTemp))
         return FALSE;
      if ((y == 0) && (x == 0)) {
         pMin.Copy (&pTemp);
         pMax.Copy (&pTemp);
         continue;
      }

      // else, do min/max
      for (i = 0; i < 3; i++) {
         pMin.p[i] = min(pMin.p[i], pTemp.p[i]);
         pMax.p[i] = max(pMax.p[i], pTemp.p[i]);
      }
   }
   pCenter.Copy (&pMin);
   pCenter.Add (&pMax);
   pCenter.Scale (.5);

   // figure out the maximum distance
   fp fTemp, fMax;
   fMax = 0.001;
   for (y = 0; y < dwHeight; y++) for (x = 0; x < dwWidth; x++) {
      if (!pss->ControlPointGet (x, y, &pTemp))
         return FALSE;
      pTemp.Subtract (&pCenter);
      fTemp = pTemp.Length();
      if (fTemp > fMax)
         fMax = fTemp;
   }
   fMax /= 5;  // so is larger

   // when draw points, get the point, subtract the center, and divide by fMax

   // use gmemtemp
   MemZero (&gMemTemp);
   MemCat (&gMemTemp, L"<rotatex val=-90/><backculloff/>");   //  so that Z is up (on the screen)

   // draw the surface as wireframe
   MemCat (&gMemTemp, L"<id val=1/><wireframeon/>");
   WCHAR szTemp[128];
   DWORD dwYMax, dwXMax;
   // BUGFIX - Deal with loop check
   dwXMax = dwWidth - (pss->LoopGet(TRUE) ? 0 : 1);
   dwYMax = dwHeight - (pss->LoopGet(FALSE) ? 0 : 1);
   for (y = 0; y < dwYMax; y++) for (x = 0; x < dwXMax; x++) {
      MemCat (&gMemTemp, L"<colordefault color=#404080/>");

      MemCat (&gMemTemp, L"<shapepolygon");

      for (i = 0; i < 4; i++) {
         switch (i) {
         case 0: // UL
            pss->ControlPointGet (x,y, &pTemp);
            break;
         case 1: // UR
            pss->ControlPointGet ((x+1)%dwWidth,y, &pTemp);
            break;
         case 2: // LR
            pss->ControlPointGet ((x+1)%dwWidth,(y+1)%dwHeight, &pTemp);
            break;
         case 3: // LL
            pss->ControlPointGet (x,(y+1)%dwHeight, &pTemp);
            break;
         }
         pTemp.Subtract (&pCenter);
         pTemp.Scale (1.0 / fMax);

         swprintf (szTemp, L" p%d=%g,%g,%g",
            (int) i+1, (double)pTemp.p[0], (double)pTemp.p[1], (double)pTemp.p[2]);
         MemCat (&gMemTemp, szTemp);
      }

      MemCat (&gMemTemp, L"/>");
   }

   MemCat (&gMemTemp, L"<wireframeoff/>");

   // draw the outline
   DWORD dwNum;
   dwNum = pSpline->OrigNumPointsGet();
   for (x = 0; x < dwNum; x++) {
      CPoint p1, p2;
      pSpline->OrigPointGet (x, &p1);
      pSpline->OrigPointGet ((x+1) % dwNum, &p2);

      // convert from HV to object space
      pss->HVToInfo (p1.p[0], p1.p[1], &p1);
      pss->HVToInfo (p2.p[0], p2.p[1], &p2);
      p1.Subtract (&pCenter);
      p2.Subtract (&pCenter);
      p1.Scale (1.0 / fMax);
      p2.Scale (1.0 / fMax);


      // draw a line
      if (dwUse == 0)
         MemCat (&gMemTemp, L"<colordefault color=#c0c0c0/>");
      else {
         DWORD dwSeg;
         pSpline->OrigSegCurveGet (x, &dwSeg);
         switch (dwSeg) {
         case SEGCURVE_CUBIC:
            MemCat (&gMemTemp, L"<colordefault color=#8080ff/>");
            break;
         case SEGCURVE_CIRCLENEXT:
            MemCat (&gMemTemp, L"<colordefault color=#ffc0c0/>");
            break;
         case SEGCURVE_CIRCLEPREV:
            MemCat (&gMemTemp, L"<colordefault color=#c04040/>");
            break;
         case SEGCURVE_ELLIPSENEXT:
            MemCat (&gMemTemp, L"<colordefault color=#40c040/>");
            break;
         case SEGCURVE_ELLIPSEPREV:
            MemCat (&gMemTemp, L"<colordefault color=#004000/>");
            break;
         default:
         case SEGCURVE_LINEAR:
            MemCat (&gMemTemp, L"<colordefault color=#c0c0c0/>");
            break;
         }
      }

      // set the ID
      MemCat (&gMemTemp, L"<id val=");
      MemCat (&gMemTemp, (int)((1 << 16) | x));
      MemCat (&gMemTemp, L"/>");

      MemCat (&gMemTemp, L"<shapearrow tip=false width=.1");

      swprintf (szTemp, L" p1=%g,%g,%g p2=%g,%g,%g/>",
         (double)p1.p[0], (double)p1.p[1], (double)p1.p[2], (double)p2.p[0], (double)p2.p[1], (double)p2.p[2]);
      MemCat (&gMemTemp, szTemp);

      // do push point if more than 3 points
      if ((dwUse == 0) && (dwNum > 3)) {
         MemCat (&gMemTemp, L"<matrixpush>");
         swprintf (szTemp, L"<translate point=%g,%g,%g/>",
            (double)p1.p[0], (double)p1.p[1], (double)p1.p[2]);
         MemCat (&gMemTemp, szTemp);
         MemCat (&gMemTemp, L"<colordefault color=#ff0000/>");
         // set the ID
         MemCat (&gMemTemp, L"<id val=");
         MemCat (&gMemTemp, (int)((2 << 16) | x));
         MemCat (&gMemTemp, L"/>");

         MemCat (&gMemTemp, L"<MeshSphere radius=.3/><shapemeshsurface/>");

         MemCat (&gMemTemp, L"</matrixpush>");
      }
   }

   // set the threeD control
   ESCMTHREEDCHANGE tc;
   memset (&tc, 0, sizeof(tc));
   tc.pszMML = (PWSTR) gMemTemp.p;
   pControl->Message (ESCM_THREEDCHANGE, &tc);

   return TRUE;
}


/********************************************************************************
GenerateThreeDFromSements - Draws the splinesurface with all of the edges.
   It draws:
         1) The outline of the surface
         2) The current "Edge" cutout
         3) Any segments in plSeq. From LineSegmentsToLineSequences(). Each
               of these is given an ID, starting at 0x1000.

inputs
   PWSTR       pszControl - Control name.
   PCEscPage   pPage - Page
   PCSplineSurface pss - Surface
   PCListFixed plSeq - From LineSegmentsToLineSequences()
   PCListFixed plSelected - List of selected sequences from plSeq.
   DWORD       dwSideA - 1 is side A, If it's side B (0) then need to flip coordinates.
                           Use 2 for ground, which means rotation different.
returns
   BOOl - TRUE if success

*/
BOOL GenerateThreeDFromSegments (PWSTR pszControl, PCEscPage pPage, PCSplineSurface pss,
                                      PCListFixed plSeq, PCListFixed plSelected, DWORD dwSideA)
{
   PCEscControl pControl = pPage->ControlFind (pszControl);
   if (!pControl)
      return FALSE;

   // figure out the center
   CPoint pCenter, pTemp, pMin, pMax;
   DWORD i;
   pCenter.Zero();
   pMin.Zero();
   pMax.Zero();
   DWORD x, y, dwWidth, dwHeight;
   dwWidth = pss->ControlNumGet (TRUE);
   dwHeight = pss->ControlNumGet (FALSE);
   for (y = 0; y < dwHeight; y++) for (x = 0; x < dwWidth; x++) {
      if (!pss->ControlPointGet (x, y, &pTemp))
         return FALSE;
      if ((y == 0) && (x == 0)) {
         pMin.Copy (&pTemp);
         pMax.Copy (&pTemp);
         continue;
      }

      // else, do min/max
      for (i = 0; i < 3; i++) {
         pMin.p[i] = min(pMin.p[i], pTemp.p[i]);
         pMax.p[i] = max(pMax.p[i], pTemp.p[i]);
      }
   }
   pCenter.Copy (&pMin);
   pCenter.Add (&pMax);
   pCenter.Scale (.5);

   // figure out the maximum distance
   fp fTemp, fMax;
   fMax = 0.001;
   for (y = 0; y < dwHeight; y++) for (x = 0; x < dwWidth; x++) {
      if (!pss->ControlPointGet (x, y, &pTemp))
         return FALSE;
      pTemp.Subtract (&pCenter);
      fTemp = pTemp.Length();
      if (fTemp > fMax)
         fMax = fTemp;
   }
   if (dwSideA != 2)
      fMax /= 7;  // so is larger
   else
      fMax /= 8;

   // when draw points, get the point, subtract the center, and divide by fMax

   // use gmemtemp
   MemZero (&gMemTemp);
   if (!dwSideA)
      MemCat (&gMemTemp, L"<rotatey val=180/>");
   if (dwSideA != 2)
      MemCat (&gMemTemp, L"<rotatex val=-90/>");
   MemCat (&gMemTemp, L"<backculloff/>");   //  so that Z is up (on the screen)


   // draw the surface as wireframe
   MemCat (&gMemTemp, L"<id val=1/><wireframeon/>");
   WCHAR szTemp[128];
   for (y = 0; y < dwHeight-1; y++) for (x = 0; x < dwWidth-1; x++) {
      MemCat (&gMemTemp, L"<colordefault color=#404080/>");

      MemCat (&gMemTemp, L"<shapepolygon");

      for (i = 0; i < 4; i++) {
         switch (i) {
         case 0: // UL
            pss->ControlPointGet (x,y, &pTemp);
            break;
         case 1: // UR
            pss->ControlPointGet (x+1,y, &pTemp);
            break;
         case 2: // LR
            pss->ControlPointGet (x+1,y+1, &pTemp);
            break;
         case 3: // LL
            pss->ControlPointGet (x,y+1, &pTemp);
            break;
         }
         pTemp.Subtract (&pCenter);
         pTemp.Scale (1.0 / fMax);

         swprintf (szTemp, L" p%d=%g,%g,%g",
            (int) i+1, (double)pTemp.p[0], (double)pTemp.p[1], (double)pTemp.p[2]);
         MemCat (&gMemTemp, szTemp);
      }

      MemCat (&gMemTemp, L"/>");
   }

   MemCat (&gMemTemp, L"<wireframeoff/>");

   // draw the currend edge
   DWORD dwNum;
   PWSTR pszName;
   PTEXTUREPOINT ptp;
   BOOL fClockwise;
   for (i = 0; i < pss->CutoutNum(); i++) {
      if (!pss->CutoutEnum (i, &pszName, &ptp, &dwNum, &fClockwise))
         continue;
      if (!_wcsicmp(pszName, L"edge"))
         break;
   }
   if (i < pss->CutoutNum()) {
      // found the edge

      for (x = 0; x < dwNum; x++) {
         CPoint p1, p2;

         // convert from HV to object space
         pss->HVToInfo (ptp[x].h, ptp[x].v, &p1);
         pss->HVToInfo (ptp[(x+1)%dwNum].h, ptp[(x+1)%dwNum].v, &p2);
         p1.Subtract (&pCenter);
         p2.Subtract (&pCenter);
         p1.Scale (1.0 / fMax);
         p2.Scale (1.0 / fMax);


         // draw a line
         MemCat (&gMemTemp, L"<colordefault color=#c0c0c0/>");

         MemCat (&gMemTemp, L"<shapearrow tip=false width=.05");

         swprintf (szTemp, L" p1=%g,%g,%g p2=%g,%g,%g/>",
            (double)p1.p[0], (double)p1.p[1], (double)p1.p[2], (double)p2.p[0], (double)p2.p[1], (double)p2.p[2]);
         MemCat (&gMemTemp, szTemp);
      }
   }

   // loop through all the sequences
   DWORD *padwSelected;
   DWORD dwNumSel;
   dwNumSel = plSelected->Num();
   padwSelected = (DWORD*) plSelected->Get(0);

   if (plSeq) for (i = 0; i < plSeq->Num(); i++) {
      PCListFixed pl = *((PCListFixed*) plSeq->Get(i));
      PTEXTUREPOINT pt = (PTEXTUREPOINT) pl->Get(0);
      dwNum = pl->Num();

      MemCat (&gMemTemp, L"<id val=");
      MemCat (&gMemTemp, (int)(0x1000 | i));
      MemCat (&gMemTemp, L"/>");

      // see if it's selected
      for (x = 0; x < dwNumSel; x++)
         if (padwSelected[x] == i)
            break;
      if (x >= dwNumSel)
         MemCat (&gMemTemp, L"<colordefault color=#c0c0ff/>");
      else
         MemCat (&gMemTemp, L"<colordefault color=#ff0000/>");

         // loop through all the points
      for (x = 0; x+1 < dwNum; x++) {
         CPoint p1, p2;

         // convert from HV to object space
         pss->HVToInfo (pt[x].h, pt[x].v, &p1);
         pss->HVToInfo (pt[x+1].h, pt[x+1].v, &p2);
         p1.Subtract (&pCenter);
         p2.Subtract (&pCenter);
         p1.Scale (1.0 / fMax);
         p2.Scale (1.0 / fMax);


         MemCat (&gMemTemp, L"<shapearrow tip=false width=.10");

         swprintf (szTemp, L" p1=%g,%g,%g p2=%g,%g,%g/>",
            (double)p1.p[0], (double)p1.p[1], (double)p1.p[2], (double)p2.p[0], (double)p2.p[1], (double)p2.p[2]);
         MemCat (&gMemTemp, szTemp);
      }
   }


   // set the threeD control
   ESCMTHREEDCHANGE tc;
   memset (&tc, 0, sizeof(tc));
   tc.pszMML = (PWSTR) gMemTemp.p;
   pControl->Message (ESCM_THREEDCHANGE, &tc);

   return TRUE;
}



/* SurfaceEdgeInterPage
*/
BOOL SurfaceEdgeInterPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PSOMS ps = (PSOMS)pPage->m_pUserData;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         // draw images
         ps->plSelected->Clear();
         GenerateThreeDFromSegments (L"overlayedit", pPage,
            ps->fSideA ? &ps->pThis->m_SplineA : &ps->pThis->m_SplineB,
            ps->plSeq, ps->plSelected, ps->fSideA);
      }
      break;

   case ESCN_THREEDCLICK:
      {
         PESCNTHREEDCLICK p = (PESCNTHREEDCLICK) pParam;
         if (!p->pControl->m_pszName)
            break;

         if (_wcsicmp(p->pControl->m_pszName, L"overlayedit"))
            break;

         // click on the right object
         DWORD x, i;
         if (p->dwMajor < 0x1000)
            return TRUE;
         x = p->dwMajor - 0x1000;
         if (x >= ps->plSeq->Num())
            return TRUE;

         // reverse selection
         DWORD *padw, dwNum;
         padw = (DWORD*) ps->plSelected->Get(0);
         dwNum = ps->plSelected->Num();
         for (i = 0; i < dwNum; i++)
            if (padw[i] == x)
               break;
         if (i < dwNum)
            ps->plSelected->Remove(i);
         else
            ps->plSelected->Add (&x);

         // redraw
         GenerateThreeDFromSegments (L"overlayedit", pPage,
            ps->fSideA ? &ps->pThis->m_SplineA : &ps->pThis->m_SplineB,
            ps->plSeq, ps->plSelected,ps->fSideA);
      }
      break;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;

         // if it's for the colors then do those
         if (!_wcsicmp(p->pControl->m_pszName, L"add")) {
            // try to put the pieces together
            PCListFixed pltp;
            pltp = PathToSplineListFillInGaps(ps->plSeq, (DWORD*) ps->plSelected->Get(0), ps->plSelected->Num());
            if (!pltp) {
               pPage->MBWarning (L"Please click on the intersections to enclose an area.");
               return TRUE;
            }

            // allocate enough memory so can do the calculations
            CMem  memPoints;
            DWORD dwOrig;
            PTEXTUREPOINT pt;
            dwOrig = pltp->Num();
            pt = (PTEXTUREPOINT) pltp->Get(0);
            if (!memPoints.Required (dwOrig * sizeof(CPoint)))
               return TRUE;

            // load it in
            PCPoint paPoints;
            paPoints = (PCPoint) memPoints.p;
            DWORD i;
            for (i = 0; i < dwOrig; i++) {
               paPoints[i].Zero();
               paPoints[i].p[0] = pt[i].h;
               paPoints[i].p[1] = pt[i].v;
            }
            DWORD dwMinDivide, dwMaxDivide;
            fp fDetail;
            ps->pThis->m_SplineEdge.DivideGet (&dwMinDivide, &dwMaxDivide, &fDetail);

            ps->pThis->m_pTemplate->m_pWorld->ObjectAboutToChange (ps->pThis->m_pTemplate);
            ps->pThis->m_SplineEdge.Init (TRUE, dwOrig, paPoints, NULL, (DWORD*)SEGCURVE_LINEAR, dwMinDivide, dwMaxDivide, fDetail);
            ps->pThis->RecalcEdges ();
            ps->pThis->m_pTemplate->m_pWorld->ObjectChanged (ps->pThis->m_pTemplate);

            delete pltp;

            pPage->Exit (gszBack);
            return TRUE;
         }
      }
      break;   // default

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Specify the edge through intersections";
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}



/* SurfaceEdgePage
*/
BOOL SurfaceEdgePage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCDoubleSurface pv = (PCDoubleSurface)pPage->m_pUserData;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         // draw images
         DSGenerateThreeDFromSpline (L"edgeaddremove", pPage, &pv->m_SplineEdge, &pv->m_SplineCenter, 0);
         DSGenerateThreeDFromSpline (L"edgecurve", pPage, &pv->m_SplineEdge, &pv->m_SplineCenter, 1);
      }
      break;

   case ESCN_THREEDCLICK:
      {
         PESCNTHREEDCLICK p = (PESCNTHREEDCLICK) pParam;
         if (!p->pControl->m_pszName)
            break;

         BOOL  fCol;
         if (!_wcsicmp(p->pControl->m_pszName, L"edgeaddremove"))
            fCol = TRUE;
         else if (!_wcsicmp(p->pControl->m_pszName, L"edgecurve"))
            fCol = FALSE;
         else
            break;

         // figure out x, y, and what clicked on
         DWORD x, dwMode;
         dwMode = (BYTE)(p->dwMajor >> 16);
         x = (BYTE) p->dwMajor;
         if ((dwMode < 1) || (dwMode > 2))
            break;

         // allocate enough memory so can do the calculations
         CMem  memPoints;
         CMem  memSegCurve;
         DWORD dwOrig;
         dwOrig = pv->m_SplineEdge.OrigNumPointsGet();
         if (!memPoints.Required ((dwOrig+1) * sizeof(CPoint)))
            return TRUE;
         if (!memSegCurve.Required ((dwOrig+1) * sizeof(DWORD)))
            return TRUE;

         // load it in
         PCPoint paPoints;
         DWORD *padw;
         paPoints = (PCPoint) memPoints.p;
         padw = (DWORD*) memSegCurve.p;
         DWORD i;
         for (i = 0; i < dwOrig; i++) {
            pv->m_SplineEdge.OrigPointGet (i, paPoints+i);
         }
         for (i = 0; i < dwOrig; i++)
            pv->m_SplineEdge.OrigSegCurveGet (i, padw + i);
         DWORD dwMinDivide, dwMaxDivide;
         fp fDetail;
         pv->m_SplineEdge.DivideGet (&dwMinDivide, &dwMaxDivide, &fDetail);

         if (fCol) {
            if (dwMode == 1) {
               // inserting
               memmove (paPoints + (x+1), paPoints + x, sizeof(CPoint) * (dwOrig-x));
               paPoints[x+1].Add (paPoints + ((x+2) % (dwOrig+1)));
               paPoints[x+1].Scale (.5);
               memmove (padw + (x+1), padw + x, sizeof(DWORD) * (dwOrig - x));
               dwOrig++;
            }
            else if (dwMode == 2) {
               // deleting
               memmove (paPoints + x, paPoints + (x+1), sizeof(CPoint) * (dwOrig-x-1));
               memmove (padw + x, padw + (x+1), sizeof(DWORD) * (dwOrig - x - 1));
               dwOrig--;
            }
         }
         else {
            // setting curvature
            if (dwMode == 1) {
               padw[x] = (padw[x] + 1) % (SEGCURVE_MAX+1);
            }
         }

         pv->m_pTemplate->m_pWorld->ObjectAboutToChange (pv->m_pTemplate);
         pv->m_SplineEdge.Init (TRUE, dwOrig, paPoints, NULL, padw, dwMinDivide, dwMaxDivide, fDetail);
         pv->RecalcEdges ();
         pv->m_pTemplate->m_pWorld->ObjectChanged (pv->m_pTemplate);

         // redraw the shapes
         DSGenerateThreeDFromSpline (L"edgeaddremove", pPage, &pv->m_SplineEdge, &pv->m_SplineCenter, 0);
         DSGenerateThreeDFromSpline (L"edgecurve", pPage, &pv->m_SplineEdge, &pv->m_SplineCenter, 1);
      }
      break;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;

         // if it's for the colors then do those
         if (!_wcsicmp(p->pControl->m_pszName, L"reset")) {
            pv->m_pTemplate->m_pWorld->ObjectAboutToChange (pv->m_pTemplate);
            CPoint apEdge[4];
            apEdge[0].Zero();
            apEdge[1].Zero();
            apEdge[1].p[0] = 1;
            apEdge[2].Zero();
            apEdge[2].p[0] = apEdge[2].p[1] = 1;
            apEdge[3].Zero();
            apEdge[3].p[1] = 1;
            pv->m_SplineEdge.Init (TRUE, 4, apEdge, NULL, (DWORD*) SEGCURVE_LINEAR, 0, 2, .2);
            pv->RecalcEdges ();
            pv->m_pTemplate->m_pWorld->ObjectChanged (pv->m_pTemplate);

            // redraw the shapes
            DSGenerateThreeDFromSpline (L"edgeaddremove", pPage, &pv->m_SplineEdge, &pv->m_SplineCenter, 0);
            DSGenerateThreeDFromSpline (L"edgecurve", pPage, &pv->m_SplineEdge, &pv->m_SplineCenter, 1);
            return TRUE;
         }
      }
      break;   // default

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Modify edge control points";
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}


/********************************************************************************
GenerateThreeDFromOverlays - Draws the splinesurface with all of the edges.
   It draws:
         1) The outline of the surface
         2) The current "Edge" cutout
         3) The overlays in different colors.

inputs
   PWSTR       pszControl - Control name.
   PCEscPage   pPage - Page
   PCSplineSurface pss - Surface
   BOOL        fSideA - If its side B (FALSE) need to flip coordinatese
returns
   BOOl - TRUE if success

*/
BOOL DSGenerateThreeDFromOverlays (PWSTR pszControl, PCEscPage pPage, PCSplineSurface pss,
                                        BOOL fSideA)
{
   PCEscControl pControl = pPage->ControlFind (pszControl);
   if (!pControl)
      return FALSE;

   // figure out the center
   CPoint pCenter, pTemp, pMin, pMax;
   DWORD i;
   pCenter.Zero();
   pMin.Zero();
   pMax.Zero();
   DWORD x, y, dwWidth, dwHeight;
   dwWidth = pss->ControlNumGet (TRUE);
   dwHeight = pss->ControlNumGet (FALSE);
   for (y = 0; y < dwHeight; y++) for (x = 0; x < dwWidth; x++) {
      if (!pss->ControlPointGet (x, y, &pTemp))
         return FALSE;
      if ((y == 0) && (x == 0)) {
         pMin.Copy (&pTemp);
         pMax.Copy (&pTemp);
         continue;
      }

      // else, do min/max
      for (i = 0; i < 3; i++) {
         pMin.p[i] = min(pMin.p[i], pTemp.p[i]);
         pMax.p[i] = max(pMax.p[i], pTemp.p[i]);
      }
   }
   pCenter.Copy (&pMin);
   pCenter.Add (&pMax);
   pCenter.Scale (.5);

   // figure out the maximum distance
   fp fTemp, fMax;
   fMax = 0.001;
   for (y = 0; y < dwHeight; y++) for (x = 0; x < dwWidth; x++) {
      if (!pss->ControlPointGet (x, y, &pTemp))
         return FALSE;
      pTemp.Subtract (&pCenter);
      fTemp = pTemp.Length();
      if (fTemp > fMax)
         fMax = fTemp;
   }
   fMax /= 5;  // so is larger

   // when draw points, get the point, subtract the center, and divide by fMax

   // use gmemtemp
   MemZero (&gMemTemp);
   if (!fSideA)
      MemCat (&gMemTemp, L"<rotatey val=180/>");
   MemCat (&gMemTemp, L"<rotatex val=-90/><backculloff/>");   //  so that Z is up (on the screen)

   // draw the surface as wireframe
   MemCat (&gMemTemp, L"<id val=1/><wireframeon/>");
   WCHAR szTemp[128];
   DWORD dwYMax, dwXMax;
   // BUGFIX - Deal with loop check
   dwXMax = dwWidth - (pss->LoopGet(TRUE) ? 0 : 1);
   dwYMax = dwHeight - (pss->LoopGet(FALSE) ? 0 : 1);
   for (y = 0; y < dwYMax; y++) for (x = 0; x < dwXMax; x++) {
      MemCat (&gMemTemp, L"<colordefault color=#404080/>");

      MemCat (&gMemTemp, L"<shapepolygon");

      for (i = 0; i < 4; i++) {
         switch (i) {
         case 0: // UL
            pss->ControlPointGet (x,y, &pTemp);
            break;
         case 1: // UR
            pss->ControlPointGet ((x+1)%dwWidth,y, &pTemp);
            break;
         case 2: // LR
            pss->ControlPointGet ((x+1)%dwWidth,(y+1)%dwHeight, &pTemp);
            break;
         case 3: // LL
            pss->ControlPointGet (x,(y+1)%dwHeight, &pTemp);
            break;
         }
         pTemp.Subtract (&pCenter);
         pTemp.Scale (1.0 / fMax);

         swprintf (szTemp, L" p%d=%g,%g,%g",
            (int) i+1, (double)pTemp.p[0], (double)pTemp.p[1], (double)pTemp.p[2]);
         MemCat (&gMemTemp, szTemp);
      }

      MemCat (&gMemTemp, L"/>");
   }

   MemCat (&gMemTemp, L"<wireframeoff/>");

   // draw the currend edge
   DWORD dwNum;
   PWSTR pszName;
   PTEXTUREPOINT ptp;
   BOOL fClockwise;
   for (i = 0; i < pss->CutoutNum(); i++) {
      if (!pss->CutoutEnum (i, &pszName, &ptp, &dwNum, &fClockwise))
         continue;
      if (!_wcsicmp(pszName, L"edge"))
         break;
   }
   if (i < pss->CutoutNum()) {
      // found the edge

      for (x = 0; x < dwNum; x++) {
         CPoint p1, p2;

         // convert from HV to object space
         pss->HVToInfo (ptp[x].h, ptp[x].v, &p1);
         pss->HVToInfo (ptp[(x+1)%dwNum].h, ptp[(x+1)%dwNum].v, &p2);
         p1.Subtract (&pCenter);
         p2.Subtract (&pCenter);
         p1.Scale (1.0 / fMax);
         p2.Scale (1.0 / fMax);


         // draw a line
         MemCat (&gMemTemp, L"<colordefault color=#c0c0c0/>");

         MemCat (&gMemTemp, L"<shapearrow tip=false width=.05");

         swprintf (szTemp, L" p1=%g,%g,%g p2=%g,%g,%g/>",
            (double)p1.p[0], (double)p1.p[1], (double)p1.p[2], (double)p2.p[0], (double)p2.p[1], (double)p2.p[2]);
         MemCat (&gMemTemp, szTemp);
      }
   }

   // loop through the overlays
   for (i = 0; i < pss->OverlayNum(); i++) {
      if (!pss->OverlayEnum (i, &pszName, &ptp, &dwNum, &fClockwise))
         continue;

      MemCat (&gMemTemp, L"<id val=");
      MemCat (&gMemTemp, (int)(0x1000 | i));
      MemCat (&gMemTemp, L"/>");

      // color
      switch (i % 6) {
      case 0:  // red
         MemCat (&gMemTemp, L"<colordefault color=#ff0000/>");
         break;
      case 1:  // green
         MemCat (&gMemTemp, L"<colordefault color=#00ff00/>");
         break;
      case 2:  // blue
         MemCat (&gMemTemp, L"<colordefault color=#0000ff/>");
         break;
      case 3:  // ellow
         MemCat (&gMemTemp, L"<colordefault color=#ffff00/>");
         break;
      case 4:  // blue-green
         MemCat (&gMemTemp, L"<colordefault color=#00ffff00/>");
         break;
      case 5:  // purple
         MemCat (&gMemTemp, L"<colordefault color=#ff00ff/>");
         break;
      }

      // loop through all the points
      for (x = 0; x < dwNum; x++) {
         CPoint p1, p2;

         // convert from HV to object space
         pss->HVToInfo (ptp[x].h, ptp[x].v, &p1);
         pss->HVToInfo (ptp[(x+1)%dwNum].h, ptp[(x+1)%dwNum].v, &p2);
         p1.Subtract (&pCenter);
         p2.Subtract (&pCenter);
         p1.Scale (1.0 / fMax);
         p2.Scale (1.0 / fMax);


         MemCat (&gMemTemp, L"<shapearrow tip=false width=.20");

         swprintf (szTemp, L" p1=%g,%g,%g p2=%g,%g,%g/>",
            (double)p1.p[0], (double)p1.p[1], (double)p1.p[2], (double)p2.p[0], (double)p2.p[1], (double)p2.p[2]);
         MemCat (&gMemTemp, szTemp);
      }
   }


   // set the threeD control
   ESCMTHREEDCHANGE tc;
   memset (&tc, 0, sizeof(tc));
   tc.pszMML = (PWSTR) gMemTemp.p;
   pControl->Message (ESCM_THREEDCHANGE, &tc);

   return TRUE;
}



/********************************************************************************
GenerateThreeDForPosition - Draws the splinesurface with all of the edges.
   It's divided into a grid so that the user can click on the approximate place
   to put the overlay.

  The ID used is 0x1000 + y * dwGridSize + x.
inputs
   PWSTR       pszControl - Control name.
   PCEscPage   pPage - Page
   PCSplineSurface pss - Surface
   DWORD       dwGridSize - Size of the grid
   BOOL        fSideA - If it's side B (FALSE) need to flip coordinates
returns
   BOOl - TRUE if success

*/
BOOL DSGenerateThreeDForPosition (PWSTR pszControl, PCEscPage pPage, PCSplineSurface pss,
                                       DWORD dwGridSize, BOOL fSideA)
{
   PCEscControl pControl = pPage->ControlFind (pszControl);
   if (!pControl)
      return FALSE;

   // figure out the center
   CPoint pCenter, pTemp, pMin, pMax;
   DWORD i;
   pCenter.Zero();
   pMin.Zero();
   pMax.Zero();
   DWORD x, y, dwWidth, dwHeight;
   dwWidth = pss->ControlNumGet (TRUE);
   dwHeight = pss->ControlNumGet (FALSE);
   for (y = 0; y < dwHeight; y++) for (x = 0; x < dwWidth; x++) {
      if (!pss->ControlPointGet (x, y, &pTemp))
         return FALSE;
      if ((y == 0) && (x == 0)) {
         pMin.Copy (&pTemp);
         pMax.Copy (&pTemp);
         continue;
      }

      // else, do min/max
      for (i = 0; i < 3; i++) {
         pMin.p[i] = min(pMin.p[i], pTemp.p[i]);
         pMax.p[i] = max(pMax.p[i], pTemp.p[i]);
      }
   }
   pCenter.Copy (&pMin);
   pCenter.Add (&pMax);
   pCenter.Scale (.5);

   // figure out the maximum distance
   fp fTemp, fMax;
   fMax = 0.001;
   for (y = 0; y < dwHeight; y++) for (x = 0; x < dwWidth; x++) {
      if (!pss->ControlPointGet (x, y, &pTemp))
         return FALSE;
      pTemp.Subtract (&pCenter);
      fTemp = pTemp.Length();
      if (fTemp > fMax)
         fMax = fTemp;
   }
   fMax /= 5;  // so is larger

   // when draw points, get the point, subtract the center, and divide by fMax

   // use gmemtemp
   MemZero (&gMemTemp);
   if (!fSideA)
      MemCat (&gMemTemp, L"<rotatey val=180/>");
   MemCat (&gMemTemp, L"<rotatex val=-90/><backculloff/>");   //  so that Z is up (on the screen)

   // draw the surface as wireframe
   WCHAR szTemp[128];
   // IMPORTANT - I don't think I need to change this, but make sure it works
   // for looped surfaces
   for (y = 0; y < dwGridSize; y++) for (x = 0; x < dwGridSize; x++) {
      // checkerboard
      if ((y%2) == (x%2))
         MemCat (&gMemTemp, L"<colordefault color=#8080ff/>");
      else
         MemCat (&gMemTemp, L"<colordefault color=#9090ff/>");

      MemCat (&gMemTemp, L"<id val=");
      MemCat (&gMemTemp, (int)(0x1000 + y * dwGridSize + x));
      MemCat (&gMemTemp, L"/>");

      MemCat (&gMemTemp, L"<shapepolygon");

      fp fDelta;
      fDelta = 1.0 / ((fp)dwGridSize);

      for (i = 0; i < 4; i++) {
         switch (i) {
         case 0: // UL
            pss->HVToInfo (x*fDelta, y*fDelta, &pTemp);
            break;
         case 1: // UR
            pss->HVToInfo ((x+1)*fDelta, y*fDelta, &pTemp);
            break;
         case 2: // LR
            pss->HVToInfo ((x+1)*fDelta, (y+1)*fDelta, &pTemp);
            break;
         case 3: // LL
            pss->HVToInfo (x*fDelta, (y+1)*fDelta, &pTemp);
            break;
         }
         pTemp.Subtract (&pCenter);
         pTemp.Scale (1.0 / fMax);

         swprintf (szTemp, L" p%d=%g,%g,%g",
            (int) i+1, (double)pTemp.p[0], (double)pTemp.p[1], (double)pTemp.p[2]);
         MemCat (&gMemTemp, szTemp);
      }

      MemCat (&gMemTemp, L"/>");
   }


   // draw the currend edge
   DWORD dwNum;
   PWSTR pszName;
   PTEXTUREPOINT ptp;
   BOOL fClockwise;
   for (i = 0; i < pss->CutoutNum(); i++) {
      if (!pss->CutoutEnum (i, &pszName, &ptp, &dwNum, &fClockwise))
         continue;
      if (!_wcsicmp(pszName, L"edge"))
         break;
   }
   if (i < pss->CutoutNum()) {
      // found the edge

      for (x = 0; x < dwNum; x++) {
         CPoint p1, p2;

         // convert from HV to object space
         pss->HVToInfo (ptp[x].h, ptp[x].v, &p1);
         pss->HVToInfo (ptp[(x+1)%dwNum].h, ptp[(x+1)%dwNum].v, &p2);
         p1.Subtract (&pCenter);
         p2.Subtract (&pCenter);
         p1.Scale (1.0 / fMax);
         p2.Scale (1.0 / fMax);


         // draw a line
         MemCat (&gMemTemp, L"<colordefault color=#c0c0c0/>");

         MemCat (&gMemTemp, L"<shapearrow tip=false width=.2");

         swprintf (szTemp, L" p1=%g,%g,%g p2=%g,%g,%g/>",
            (double)p1.p[0], (double)p1.p[1], (double)p1.p[2], (double)p2.p[0], (double)p2.p[1], (double)p2.p[2]);
         MemCat (&gMemTemp, szTemp);
      }
   }


   // set the threeD control
   ESCMTHREEDCHANGE tc;
   memset (&tc, 0, sizeof(tc));
   tc.pszMML = (PWSTR) gMemTemp.p;
   pControl->Message (ESCM_THREEDCHANGE, &tc);

   return TRUE;
}



/********************************************************************************
DSGenerateThreeDForSplitOverlay - Given a spline on a surface, this sets a threeD
control with the spline.

inputs
   PWSTR       pszControl - Control name.
   PCEscPage   pPage - Page
   PCSplineSurface pss - Surface
   PWSTR       pszOverlay - Overlay name
   BOOL        fSideA - If TRUE, displaying side A
returns
   BOOl - TRUE if success

NOTE: The ID's are such:
   0x10000 + X - Clicked on a line. ID's are 0-based
   0x20000 + X - Clicked on a point. ID's are 0-based
*/
BOOL DSGenerateThreeDForSplitOverlay (PWSTR pszControl, PCEscPage pPage, PCSplineSurface pss,
                                      PWSTR pszOverlay, BOOL fSideA)
{
   PCEscControl pControl = pPage->ControlFind (pszControl);
   if (!pControl)
      return FALSE;

   // get the overlay
   DWORD i;
   PWSTR psz;
   PTEXTUREPOINT ptp;
   DWORD dwNum;
   BOOL fClockwise;
   for (i = 0; i < pss->OverlayNum(); i++) {
      if (!pss->OverlayEnum (i, &psz, &ptp, &dwNum, &fClockwise))
         continue;
      if (!_wcsicmp(psz, pszOverlay))
         break;
   }
   if ((i >= pss->OverlayNum()) || !dwNum)
      return FALSE;   // error

   // figure out the center
   CPoint pCenter, pTemp, pMin, pMax;
   pCenter.Zero();
   pMin.Zero();
   pMax.Zero();
   DWORD x;
   for (x = 0; x < dwNum; x++) {
      pss->HVToInfo (ptp[x].h, ptp[x].v, &pTemp);
      if (x == 0) {
         pMin.Copy (&pTemp);
         pMax.Copy (&pTemp);
         continue;
      }

      // else, do min/max
      for (i = 0; i < 3; i++) {
         pMin.p[i] = min(pMin.p[i], pTemp.p[i]);
         pMax.p[i] = max(pMax.p[i], pTemp.p[i]);
      }
   }
   pCenter.Copy (&pMin);
   pCenter.Add (&pMax);
   pCenter.Scale (.5);

   // figure out the maximum distance
   fp fTemp, fMax;
   fMax = 0.001;
   for (x = 0; x < dwNum; x++) {
      pss->HVToInfo (ptp[x].h, ptp[x].v, &pTemp);
      pTemp.Subtract (&pCenter);
      fTemp = pTemp.Length();
      if (fTemp > fMax)
         fMax = fTemp;
   }
   fMax /= 5;  // so is larger

   // when draw points, get the point, subtract the center, and divide by fMax

   // use gmemtemp
   MemZero (&gMemTemp);
   if (!fSideA)
      MemCat (&gMemTemp, L"<rotatey val=180/>");
   MemCat (&gMemTemp, L"<rotatex val=-90/><backculloff/>");   //  so that Z is up (on the screen)


   // IMPORTANT - I don't think I need to change this but test just to make sure
   // it workes with looped surfaces

   // draw the overlay
   WCHAR szTemp[128];
   for (x = 0; x < dwNum; x++) {
      CPoint p1, p2;
      pss->HVToInfo (ptp[x].h, ptp[x].v, &p1);
      pss->HVToInfo (ptp[(x+1) % dwNum].h, ptp[(x+1) % dwNum].v, &p2);

      // convert from HV to object space
      p1.Subtract (&pCenter);
      p2.Subtract (&pCenter);
      p1.Scale (1.0 / fMax);
      p2.Scale (1.0 / fMax);


      // draw a line
      MemCat (&gMemTemp, L"<colordefault color=#c0c0c0/>");

      // set the ID
      MemCat (&gMemTemp, L"<id val=");
      MemCat (&gMemTemp, (int)(0x10000 | x));
      MemCat (&gMemTemp, L"/>");

      MemCat (&gMemTemp, L"<shapearrow tip=false width=.2");

      swprintf (szTemp, L" p1=%g,%g,%g p2=%g,%g,%g/>",
         (double)p1.p[0], (double)p1.p[1], (double)p1.p[2], (double)p2.p[0], (double)p2.p[1], (double)p2.p[2]);
      MemCat (&gMemTemp, szTemp);

      // do push point if more than 3 points
      if (dwNum > 3) {
         MemCat (&gMemTemp, L"<matrixpush>");
         swprintf (szTemp, L"<translate point=%g,%g,%g/>",
            (double)p1.p[0], (double)p1.p[1], (double)p1.p[2]);
         MemCat (&gMemTemp, szTemp);
         MemCat (&gMemTemp, L"<colordefault color=#ff0000/>");
         // set the ID
         MemCat (&gMemTemp, L"<id val=");
         MemCat (&gMemTemp, (int)(0x20000 | x));
         MemCat (&gMemTemp, L"/>");

         MemCat (&gMemTemp, L"<MeshSphere radius=.3/><shapemeshsurface/>");

         MemCat (&gMemTemp, L"</matrixpush>");
      }
   }

   // set the threeD control
   ESCMTHREEDCHANGE tc;
   memset (&tc, 0, sizeof(tc));
   tc.pszMML = (PWSTR) gMemTemp.p;
   pControl->Message (ESCM_THREEDCHANGE, &tc);

   return TRUE;
}


/* SurfaceOverMainPage
*/
BOOL SurfaceOverMainPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PSOMS ps = (PSOMS)pPage->m_pUserData;
   static BOOL fMode = 0;  // click and go to edit. If 1 then click and delete

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         fMode = 0;
         DSGenerateThreeDFromOverlays (L"overlayedit", pPage,
            ps->fSideA ? &ps->pThis->m_SplineA : &ps->pThis->m_SplineB,
            ps->fSideA);
      }
      break;

   case ESCN_THREEDCLICK:
      {
         PESCNTHREEDCLICK p = (PESCNTHREEDCLICK) pParam;
         if (!p->pControl->m_pszName)
            break;

         if (_wcsicmp(p->pControl->m_pszName, L"overlayedit"))
            break;
         DWORD x;
         if (p->dwMajor < 0x1000)
            return TRUE;
         x = p->dwMajor - 0x1000;
         
         PCSplineSurface pss;
         pss = ps->fSideA ?&ps->pThis->m_SplineA : &ps->pThis->m_SplineB;
         if (x >= pss->OverlayNum())
            return FALSE;


         PWSTR pszName;
         DWORD dwNumPoints;
         PTEXTUREPOINT ptp;
         BOOL fClockwise;
         pss->OverlayEnum (x, &pszName, &ptp, &dwNumPoints, &fClockwise);

         if (fMode == 1) {
            ps->pThis->m_pTemplate->m_pWorld->ObjectAboutToChange (ps->pThis->m_pTemplate);

            PWSTR psz;
            DWORD dwID;
            for (psz = pszName; psz[0] && (psz[0] != L':'); psz++);
            if (psz[0]) {
               dwID = (DWORD)_wtoi(psz+1);
               ps->pThis->ClaimRemove (dwID);
            }
            pss->OverlayRemove (pszName);
            ps->pThis->m_pTemplate->m_pWorld->ObjectChanged (ps->pThis->m_pTemplate);

            // redraw the shapes
            DSGenerateThreeDFromOverlays (L"overlayedit", pPage,
               ps->fSideA ? &ps->pThis->m_SplineA : &ps->pThis->m_SplineB,
               ps->fSideA);
          
            fMode = 0;
            return TRUE;
         }
         else {
            if (!pszName)
               return TRUE;

            WCHAR szTemp[128];
            wcscpy (ps->szEditing, pszName);
            wcscpy (szTemp, pszName);
            PWSTR psz;
            for (psz = szTemp; psz[0] && (psz[0] != L':'); psz++);
            psz[0] = 0;

            // next window depends upon the case
            if (!_wcsicmp(szTemp, L"Intersection"))
               pPage->Exit (L"EditInter");
            else if (!_wcsicmp(szTemp, L"Any shape"))
               pPage->Exit (L"EditEdge");
            else
               pPage->Exit (L"EditCustom");

            return TRUE;
         }
      }
      break;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;

         // if it's for the colors then do those
         if (!_wcsicmp(p->pControl->m_pszName, L"remove")) {
            pPage->MBSpeakInformation (L"Click on an overlay to delete it.");
            fMode = 1;  // delete when click
            return TRUE;
         }
      }
      break;   // default

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Overlays";
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}


/* SurfaceOverAddInterPage
*/
BOOL SurfaceOverAddInterPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PSOMS ps = (PSOMS)pPage->m_pUserData;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         // draw images
         ps->plSelected->Clear();
         GenerateThreeDFromSegments (L"overlayedit", pPage,
            ps->fSideA ? &ps->pThis->m_SplineA : &ps->pThis->m_SplineB,
            ps->plSeq, ps->plSelected, ps->fSideA);
      }
      break;

   case ESCN_THREEDCLICK:
      {
         PESCNTHREEDCLICK p = (PESCNTHREEDCLICK) pParam;
         if (!p->pControl->m_pszName)
            break;

         if (_wcsicmp(p->pControl->m_pszName, L"overlayedit"))
            break;

         // click on the right object
         DWORD x, i;
         if (p->dwMajor < 0x1000)
            return TRUE;
         x = p->dwMajor - 0x1000;
         if (x >= ps->plSeq->Num())
            return TRUE;

         // reverse selection
         DWORD *padw, dwNum;
         padw = (DWORD*) ps->plSelected->Get(0);
         dwNum = ps->plSelected->Num();
         for (i = 0; i < dwNum; i++)
            if (padw[i] == x)
               break;
         if (i < dwNum)
            ps->plSelected->Remove(i);
         else
            ps->plSelected->Add (&x);

         // redraw
         GenerateThreeDFromSegments (L"overlayedit", pPage,
            ps->fSideA ? &ps->pThis->m_SplineA : &ps->pThis->m_SplineB,
            ps->plSeq, ps->plSelected,ps->fSideA);
      }
      break;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;

         // if it's for the colors then do those
         if (!_wcsicmp(p->pControl->m_pszName, L"add")) {
            // try to put the pieces together
            PCListFixed pltp;
            pltp = PathToSplineListFillInGaps(ps->plSeq, (DWORD*) ps->plSelected->Get(0), ps->plSelected->Num());
            if (!pltp) {
               pPage->MBWarning (L"Please click on the intersections to enclose an area.");
               return TRUE;
            }

            ps->pThis->AddOverlay (ps->fSideA, L"Intersection", (PTEXTUREPOINT)pltp->Get(0),
               pltp->Num(), TRUE, ps->pdwCurColor);

            delete pltp;

            pPage->Exit (gszBack);
            return TRUE;
         }
      }
      break;   // default

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Add an overlay based on intersections";
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}


/* SurfaceOverEditInterPage
*/
BOOL SurfaceOverEditInterPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PSOMS ps = (PSOMS)pPage->m_pUserData;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         // draw images
         ps->plSelected->Clear();
         GenerateThreeDFromSegments (L"overlayedit", pPage,
            ps->fSideA ? &ps->pThis->m_SplineA : &ps->pThis->m_SplineB,
            ps->plSeq, ps->plSelected,ps->fSideA);
      }
      break;

   case ESCN_THREEDCLICK:
      {
         PESCNTHREEDCLICK p = (PESCNTHREEDCLICK) pParam;
         if (!p->pControl->m_pszName)
            break;

         if (_wcsicmp(p->pControl->m_pszName, L"overlayedit"))
            break;

         // click on the right object
         DWORD x, i;
         if (p->dwMajor < 0x1000)
            return TRUE;
         x = p->dwMajor - 0x1000;
         if (x >= ps->plSeq->Num())
            return TRUE;

         // reverse selection
         DWORD *padw, dwNum;
         padw = (DWORD*) ps->plSelected->Get(0);
         dwNum = ps->plSelected->Num();
         for (i = 0; i < dwNum; i++)
            if (padw[i] == x)
               break;
         if (i < dwNum)
            ps->plSelected->Remove(i);
         else
            ps->plSelected->Add (&x);

         // redraw
         GenerateThreeDFromSegments (L"overlayedit", pPage,
            ps->fSideA ? &ps->pThis->m_SplineA : &ps->pThis->m_SplineB,
            ps->plSeq, ps->plSelected,ps->fSideA);
      }
      break;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;

         // if it's for the colors then do those
         if (!_wcsicmp(p->pControl->m_pszName, L"add")) {
            // try to put the pieces together
            PCListFixed pltp;
            pltp = PathToSplineListFillInGaps(ps->plSeq, (DWORD*) ps->plSelected->Get(0), ps->plSelected->Num());
            if (!pltp) {
               pPage->MBWarning (L"Please click on the intersections to enclose an area.");
               return TRUE;
            }

            // find this one and set the new values
            ps->pThis->m_pTemplate->m_pWorld->ObjectAboutToChange (ps->pThis->m_pTemplate);

            DWORD i;
            PWSTR pszName;
            BOOL fClockwise;
            PTEXTUREPOINT ptp;
            DWORD dwNum;
            PCSplineSurface pss;
            pss = ps->fSideA ?&ps->pThis->m_SplineA : &ps->pThis->m_SplineB;
            for (i = 0; i < pss->OverlayNum(); i++) {
               if (!pss->OverlayEnum (i, &pszName, &ptp, &dwNum, &fClockwise))
                  continue;

               if (_wcsicmp(pszName, ps->szEditing))
                  continue;

               // if got here found it, so set the new one
               pss->OverlaySet (ps->szEditing, (PTEXTUREPOINT)pltp->Get(0),
                  pltp->Num(), TRUE);
               break;

            }
            ps->pThis->m_pTemplate->m_pWorld->ObjectChanged (ps->pThis->m_pTemplate);


            delete pltp;

            pPage->Exit (gszBack);
            return TRUE;
         }
      }
      break;   // default

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Modify an overlay based on intersections";
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}


/* SurfaceOverEditEdgePage
*/
BOOL SurfaceOverEditEdgePage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PSOMS ps = (PSOMS)pPage->m_pUserData;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         // draw images
         DSGenerateThreeDForSplitOverlay (L"edgeaddremove", pPage,
            ps->fSideA ? &ps->pThis->m_SplineA : &ps->pThis->m_SplineB,
            ps->szEditing, ps->fSideA);
      }
      break;

   case ESCN_THREEDCLICK:
      {
         PESCNTHREEDCLICK p = (PESCNTHREEDCLICK) pParam;
         if (!p->pControl->m_pszName)
            break;

         if (_wcsicmp(p->pControl->m_pszName, L"edgeaddremove"))
            break;

         // find the number of points
         DWORD i;
         PWSTR psz;
         PTEXTUREPOINT ptp;
         DWORD dwNum;
         BOOL fClockwise;
         PCSplineSurface pss;
         pss = ps->fSideA ? &ps->pThis->m_SplineA : &ps->pThis->m_SplineB;
         for (i = 0; i < pss->OverlayNum(); i++) {
            if (!pss->OverlayEnum (i, &psz, &ptp, &dwNum, &fClockwise))
               continue;
            if (!_wcsicmp(psz, ps->szEditing))
               break;
         }
         if ((i >= pss->OverlayNum()) || !dwNum)
            return TRUE;   // error

         // click on the right object
         DWORD x, dwUse;
         if ((p->dwMajor >= 0x10000) && (p->dwMajor < 0x10000 + dwNum)) {
            x = p->dwMajor - 0x10000;
            dwUse = 0;  // splitting
         }
         else if ((p->dwMajor >= 0x20000) && (p->dwMajor < 0x20000 + dwNum)) {
            x = p->dwMajor - 0x20000;
            dwUse = 1;  // removing
         }
         else
            return TRUE;   // unknown

         // allocate enough memory for the overlay
         CMem mem;
         if (!mem.Required(sizeof(TEXTUREPOINT) * (dwNum+1)))
            return TRUE;
         PTEXTUREPOINT pNew;
         pNew = (PTEXTUREPOINT) mem.p;

         // fill it in
         if (dwUse == 0) {
            memcpy (pNew, ptp+0, (x+1) * sizeof(TEXTUREPOINT));
            memcpy (pNew + (x+1), ptp + x, (dwNum-x) * sizeof(TEXTUREPOINT));
            pNew[x+1].h = (ptp[x].h + ptp[(x+1)%dwNum].h) / 2;
            pNew[x+1].v = (ptp[x].v + ptp[(x+1)%dwNum].v) / 2;
            dwNum++;
         }
         else {
            memcpy (pNew, ptp+0, x * sizeof(TEXTUREPOINT));
            memcpy (pNew + x, ptp + (x+1), (dwNum - x - 1) * sizeof(TEXTUREPOINT));
            dwNum--;
         }

         // set this overlay
         ps->pThis->m_pTemplate->m_pWorld->ObjectAboutToChange (ps->pThis->m_pTemplate);
         pss->OverlaySet (ps->szEditing, pNew, dwNum, fClockwise);
         // Set flag so show the control points for this
         (*ps->pdwCurColor) = OverlayShapeNameToColor(ps->szEditing);
         ps->pThis->m_pTemplate->m_pWorld->ObjectChanged (ps->pThis->m_pTemplate);

         // make sure they're visible
         ps->pThis->m_pTemplate->m_pWorld->SelectionClear();
         GUID gObject;
         ps->pThis->m_pTemplate->GUIDGet (&gObject);
         ps->pThis->m_pTemplate->m_pWorld->SelectionAdd (ps->pThis->m_pTemplate->m_pWorld->ObjectFind(&gObject));

         // redraw
         DSGenerateThreeDForSplitOverlay (L"edgeaddremove", pPage,
            ps->fSideA ? &ps->pThis->m_SplineA : &ps->pThis->m_SplineB,
            ps->szEditing, ps->fSideA);
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Modify edge control points";
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}

/* SurfaceOverAddCustomPage
*/
BOOL SurfaceOverAddCustomPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PSOMS ps = (PSOMS)pPage->m_pUserData;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         DSGenerateThreeDForPosition (L"overlayedit", pPage,
            ps->fSideA ? &ps->pThis->m_SplineA : &ps->pThis->m_SplineB, 8, ps->fSideA);

         PCEscControl pControl;
         pControl = pPage->ControlFind (L"rect");
         if (pControl)
            pControl->AttribSetBOOL (gszChecked, TRUE);
      }
      break;

   case ESCN_THREEDCLICK:
      {
         PESCNTHREEDCLICK p = (PESCNTHREEDCLICK) pParam;
         if (!p->pControl->m_pszName)
            break;

         if (_wcsicmp(p->pControl->m_pszName, L"overlayedit"))
            break;

         // click on the right object
         DWORD x, y;
         if (p->dwMajor < 0x1000)
            return TRUE;
         x = (p->dwMajor - 0x1000) % 8;;
         y = (p->dwMajor - 0x1000) / 8;
         if (y >= 8)
            return TRUE;

         // convert this to a center point
         CPoint pCenter;
         pCenter.Zero();
         pCenter.p[0] = (fp) (x +.5) / (fp) 8;
         pCenter.p[1] = (fp) (y +.5) / (fp) 8;
         fp fWidth, fHeight;
         fWidth = .1;
         fHeight = .1;

         // see which one is clicked
         PWSTR pszName;
         PCEscControl pControl;
         DWORD dwID;
         if ((pControl = pPage->ControlFind(L"rect")) && pControl->AttribGetBOOL(gszChecked)) {
            dwID = 0;
         }
         else if ((pControl = pPage->ControlFind(L"ellipse")) && pControl->AttribGetBOOL(gszChecked)) {
            dwID = 1;
         }
         else if ((pControl = pPage->ControlFind(L"below")) && pControl->AttribGetBOOL(gszChecked)) {
            dwID = 2;
         }
         else if ((pControl = pPage->ControlFind(L"above")) && pControl->AttribGetBOOL(gszChecked)) {
            dwID = 3;
         }
         else if ((pControl = pPage->ControlFind(L"left")) && pControl->AttribGetBOOL(gszChecked)) {
            dwID = 4;
         }
         else if ((pControl = pPage->ControlFind(L"right")) && pControl->AttribGetBOOL(gszChecked)) {
            dwID = 5;
         }
         else if ((pControl = pPage->ControlFind(L"hband")) && pControl->AttribGetBOOL(gszChecked)) {
            dwID = 6;
         }
         else if ((pControl = pPage->ControlFind(L"vband")) && pControl->AttribGetBOOL(gszChecked)) {
            dwID = 7;
         }
         else if ((pControl = pPage->ControlFind(L"any")) && pControl->AttribGetBOOL(gszChecked)) {
            dwID = 8;
         }
         else
            return TRUE;

         PCListFixed pl;
         pl = OverlayShapeToPoints (dwID, &pCenter, fWidth, fHeight);
         pszName = gpapszOverlayNames[dwID];
         if (pl) {
            ps->pThis->AddOverlay (ps->fSideA, pszName, (PTEXTUREPOINT)pl->Get(0), pl->Num(), TRUE,
               ps->pdwCurColor);
            delete pl;
         }

         pPage->Exit (gszBack);
         return TRUE;
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Add an overlay from your own points";
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}


/* SurfaceOverEditCustomPage
*/
BOOL SurfaceOverEditCustomPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PSOMS ps = (PSOMS)pPage->m_pUserData;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         PCEscControl pControl;
         PWSTR pszName;
         pszName = NULL;
         switch (OverlayShapeNameToID(ps->szEditing)) {
         case 0:
            pszName = L"rect";
            break;
         case 1:
            pszName = L"ellipse";
            break;
         case 2:
            pszName = L"below";
            break;
         case 3:
            pszName = L"above";
            break;
         case 4:
            pszName = L"left";
            break;
         case 5:
            pszName = L"right";
            break;
         case 6:
            pszName = L"hband";
            break;
         case 7:
            pszName = L"vband";
            break;
         case 8:
            pszName = L"any";
            break;
         }

         pControl = NULL;
         if (pszName)
            pControl = pPage->ControlFind (pszName);
         if (pControl)
            pControl->AttribSetBOOL (gszChecked, TRUE);
      }
      break;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;

         // only care ab out change shape
         if (_wcsicmp(p->pControl->m_pszName, L"changeshape"))
            break;

         // see which one is clicked
         PWSTR pszName;
         DWORD dwID;
         PCEscControl pControl;
         if ((pControl = pPage->ControlFind(L"rect")) && pControl->AttribGetBOOL(gszChecked)) {
            dwID = 0;
         }
         else if ((pControl = pPage->ControlFind(L"ellipse")) && pControl->AttribGetBOOL(gszChecked)) {
            dwID = 1;
         }
         else if ((pControl = pPage->ControlFind(L"below")) && pControl->AttribGetBOOL(gszChecked)) {
            dwID = 2;
         }
         else if ((pControl = pPage->ControlFind(L"above")) && pControl->AttribGetBOOL(gszChecked)) {
            dwID = 3;
         }
         else if ((pControl = pPage->ControlFind(L"left")) && pControl->AttribGetBOOL(gszChecked)) {
            dwID = 4;
         }
         else if ((pControl = pPage->ControlFind(L"right")) && pControl->AttribGetBOOL(gszChecked)) {
            dwID = 5;
         }
         else if ((pControl = pPage->ControlFind(L"hband")) && pControl->AttribGetBOOL(gszChecked)) {
            dwID = 6;
         }
         else if ((pControl = pPage->ControlFind(L"vband")) && pControl->AttribGetBOOL(gszChecked)) {
            dwID = 7;
         }
         else if ((pControl = pPage->ControlFind(L"any")) && pControl->AttribGetBOOL(gszChecked)) {
            dwID = 8;
         }
         else
            return TRUE;

         // find the number of points
         DWORD i;
         PWSTR psz;
         PTEXTUREPOINT ptp;
         DWORD dwNum;
         BOOL fClockwise;
         PCSplineSurface pss;
         pss = ps->fSideA ? &ps->pThis->m_SplineA : &ps->pThis->m_SplineB;
         for (i = 0; i < pss->OverlayNum(); i++) {
            if (!pss->OverlayEnum (i, &psz, &ptp, &dwNum, &fClockwise))
               continue;
            if (!_wcsicmp(psz, ps->szEditing))
               break;
         }
         if ((i >= pss->OverlayNum()) || !dwNum)
            return TRUE;   // error

         // see which one currently have
         DWORD dwCur, dwColor;
         dwCur = OverlayShapeNameToID (ps->szEditing);
         if (dwCur == dwID) {
            pPage->Exit (gszBack);
            return TRUE;   // no change
         }
         dwColor = OverlayShapeNameToColor (ps->szEditing);

         // get the current parameters
         CPoint pCenter;
         fp fWidth, fHeight;
         if (!OverlayShapeFromPoints (dwCur, ptp, dwNum, &pCenter, &fWidth, &fHeight))
            return FALSE;

         // convert it to a new shape
         PCListFixed pl;
         if (dwID != 8)
            pl = OverlayShapeToPoints (dwID, &pCenter, fWidth, fHeight);
         else {
            // if convert to any shape then keep the old points
            pl = new CListFixed;
            if (pl)
               pl->Init (sizeof(TEXTUREPOINT), ptp, dwNum);
         }
         pszName = gpapszOverlayNames[dwID];

         WCHAR szTemp[128];
         swprintf (szTemp, L"%s:%d", pszName, (int) dwColor);

         if (pl) {
            ps->pThis->m_pTemplate->m_pWorld->ObjectAboutToChange (ps->pThis->m_pTemplate);
            // delete the old one
            pss->OverlayRemove (ps->szEditing);

            // add the new one
            pss->OverlaySet (szTemp, (PTEXTUREPOINT)pl->Get(0), pl->Num(), fClockwise);

            // Set flag so show the control points for this
            (*ps->pdwCurColor) = dwColor;
            ps->pThis->m_pTemplate->m_pWorld->ObjectChanged (ps->pThis->m_pTemplate);

            // make sure they're visible
            ps->pThis->m_pTemplate->m_pWorld->SelectionClear();
            GUID gObject;
            ps->pThis->m_pTemplate->GUIDGet (&gObject);
            ps->pThis->m_pTemplate->m_pWorld->SelectionAdd (ps->pThis->m_pTemplate->m_pWorld->ObjectFind(&gObject));



            delete pl;
         }


         pPage->Exit (gszBack);
         return TRUE;
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Modify an overlay's shape";
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}


/* SurfaceFramingPage
*/
BOOL SurfaceFramingPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCDoubleSurface pv = (PCDoubleSurface)pPage->m_pUserData;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         PCEscControl pControl;
         DWORD i, j;
         WCHAR szTemp[64];

         // buttons
         pControl = pPage->ControlFind (L"showonlywhenvisible");
         if (pControl)
            pControl->AttribSetBOOL (gszChecked, pv->m_fFramingShowOnlyWhenVisible);
         pControl = pPage->ControlFind (L"rotate");
         if (pControl)
            pControl->AttribSetBOOL (gszChecked, pv->m_fFramingRotate);
         for (i = 0; i < 3; i++) {
            swprintf (szTemp, L"show%d", (int) i);
            pControl = pPage->ControlFind (szTemp);
            if (pControl)
               pControl->AttribSetBOOL (gszChecked, pv->m_afFramingShow[i]);
         }

         // combobox
         for (i = 0; i < 3; i++) {
            swprintf (szTemp, L"shape%d", (int)i);
            ComboBoxSet (pPage, szTemp, pv->m_adwFramingShape[i]);

            swprintf (szTemp, L"insettl%d", (int)i);
            ComboBoxSet (pPage, szTemp, pv->m_afFramingInsetTL[i]);
         }

         // measurements
         for (i = 0; i < 3; i++) {
            swprintf (szTemp, L"mindist%d", (int)i);
            MeasureToString (pPage, szTemp, pv->m_afFramingMinDist[i]);

            for (j = 0; j < 2; j++) {
               swprintf (szTemp, L"inset%d%d", (int)i, (int) j);
               MeasureToString (pPage, szTemp, pv->m_afFramingInset[i][j]);

               swprintf (szTemp, L"extthick%d%d", (int)i, (int) j);
               MeasureToString (pPage, szTemp, pv->m_afFramingExtThick[i][j]);

               swprintf (szTemp, L"scale%d%d", (int)i, (int)j);
               MeasureToString (pPage, szTemp, pv->m_apFramingScale[i].p[j]);

               swprintf (szTemp, L"extend%d%d", (int)i, (int)j);
               MeasureToString (pPage, szTemp, pv->m_afFramingExtend[i][j]);

               swprintf (szTemp, L"bevel%d%d", (int)i, (int)j);
               AngleToControl (pPage, szTemp, pv->m_afFramingBevel[i][j], TRUE);
            }
         }

      }
      break;

   case ESCN_COMBOBOXSELCHANGE:
      {
         PESCNCOMBOBOXSELCHANGE p = (PESCNCOMBOBOXSELCHANGE) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;

         PWSTR pszShape = L"shape";
         DWORD dwShapeLen = (DWORD)wcslen(pszShape);
         PWSTR pszInsetTL = L"insettl";
         DWORD dwInsetTLLen = (DWORD)wcslen(pszInsetTL);

         if (!wcsncmp(p->pControl->m_pszName, pszShape, dwShapeLen)) {
            DWORD dwShow = _wtoi(p->pControl->m_pszName + dwShapeLen);
            DWORD dwVal = p->pszName ? _wtoi(p->pszName) : 0;
            dwShow = min(2,dwShow);
            if (dwVal == pv->m_adwFramingShape[dwShow])
               return TRUE;   // already done

            pv->m_pTemplate->m_pWorld->ObjectAboutToChange (pv->m_pTemplate);
            pv->m_adwFramingShape[dwShow] = dwVal;
            pv->m_fFramingDirty = TRUE;
            pv->m_pTemplate->m_pWorld->ObjectChanged (pv->m_pTemplate);
            return TRUE;
         }
         else if (!wcsncmp(p->pControl->m_pszName, pszInsetTL, dwInsetTLLen)) {
            DWORD dwShow = _wtoi(p->pControl->m_pszName + dwInsetTLLen);
            DWORD dwVal = p->pszName ? _wtoi(p->pszName) : 0;
            dwShow = min(2,dwShow);
            if (dwVal == pv->m_afFramingInsetTL[dwShow])
               return TRUE;   // already done

            pv->m_pTemplate->m_pWorld->ObjectAboutToChange (pv->m_pTemplate);
            pv->m_afFramingInsetTL[dwShow] = dwVal;
            pv->m_fFramingDirty = TRUE;
            pv->m_pTemplate->m_pWorld->ObjectChanged (pv->m_pTemplate);
            return TRUE;
         }
      }
      return 0;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;

         PWSTR pszShow = L"show";
         DWORD dwShowLen = (DWORD)wcslen(pszShow);


         // if it's for the colors then do those
         if (!_wcsicmp(p->pControl->m_pszName, L"showonlywhenvisible")) {
            pv->m_pTemplate->m_pWorld->ObjectAboutToChange (pv->m_pTemplate);
            pv->m_fFramingShowOnlyWhenVisible =p->pControl->AttribGetBOOL(gszChecked);
            pv->m_fFramingDirty = TRUE;
            pv->m_pTemplate->m_pWorld->ObjectChanged (pv->m_pTemplate);
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"rotate")) {
            pv->m_pTemplate->m_pWorld->ObjectAboutToChange (pv->m_pTemplate);
            pv->m_fFramingRotate =p->pControl->AttribGetBOOL(gszChecked);
            pv->m_fFramingDirty = TRUE;
            pv->m_pTemplate->m_pWorld->ObjectChanged (pv->m_pTemplate);
            return TRUE;
         }
         else if (!wcsncmp(p->pControl->m_pszName, pszShow, dwShowLen) && (wcslen(p->pControl->m_pszName) == dwShowLen+1)) {
            DWORD dwShow = _wtoi(p->pControl->m_pszName + dwShowLen);
            dwShow = min(2,dwShow);
            pv->m_pTemplate->m_pWorld->ObjectAboutToChange (pv->m_pTemplate);
            pv->m_afFramingShow[dwShow] = p->pControl->AttribGetBOOL(gszChecked);
            pv->m_fFramingDirty = TRUE;
            pv->m_pTemplate->m_pWorld->ObjectChanged (pv->m_pTemplate);
         }
      }
      break;   // default

   case ESCN_EDITCHANGE:
      {
         PESCNEDITCHANGE p = (PESCNEDITCHANGE) pParam;

         if (!p->pControl || !p->pControl->m_pszName)
            break;

         // just get all the values
         DWORD i, j;
         WCHAR szTemp[64];
         pv->m_pTemplate->m_pWorld->ObjectAboutToChange (pv->m_pTemplate);

         for (i = 0; i < 3; i++) {
            swprintf (szTemp, L"mindist%d", (int)i);
            MeasureParseString (pPage, szTemp, &pv->m_afFramingMinDist[i]);
            pv->m_afFramingMinDist[i] = max (.01, pv->m_afFramingMinDist[i]);

            for (j = 0; j < 2; j++) {
               swprintf (szTemp, L"inset%d%d", (int)i, (int)j);
               MeasureParseString (pPage, szTemp, &pv->m_afFramingInset[i][j]);
               pv->m_afFramingInset[i][j] = max(0, pv->m_afFramingInset[i][j]);

               swprintf (szTemp, L"extthick%d%d", (int)i, (int)j);
               MeasureParseString (pPage, szTemp, &pv->m_afFramingExtThick[i][j]);
               pv->m_afFramingExtThick[i][j] = max(0, pv->m_afFramingExtThick[i][j]);

               swprintf (szTemp, L"scale%d%d", (int)i, (int)j);
               MeasureParseString (pPage, szTemp, &pv->m_apFramingScale[i].p[j]);
               pv->m_apFramingScale[i].p[j] = max(CLOSE, pv->m_apFramingScale[i].p[j]);

               swprintf (szTemp, L"extend%d%d", (int)i, (int)j);
               MeasureParseString (pPage, szTemp, &pv->m_afFramingExtend[i][j]);

               swprintf (szTemp, L"bevel%d%d", (int)i, (int)j);
               pv->m_afFramingBevel[i][j] = AngleFromControl (pPage, szTemp);
               pv->m_afFramingBevel[i][j] = max(-PI/2*.99, pv->m_afFramingBevel[i][j]);
               pv->m_afFramingBevel[i][j] = min(PI/2*.99, pv->m_afFramingBevel[i][j]);
            }
         }

         pv->m_fFramingDirty = TRUE;
         pv->m_pTemplate->m_pWorld->ObjectChanged (pv->m_pTemplate);
      }
      break;
   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Framing";
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}




/*******************************************************************************
CDoubleSurface::SurfaceDetailPage - Shows the page.

inputs
   PCEscWindow    pWindow - window
returns
   PWSTR - Return string
*/
PWSTR CDoubleSurface::SurfaceDetailPage (PCEscWindow pWindow)
{
   return pWindow->PageDialog (ghInstance, IDR_MMLSURFACEDETAIL, ::SurfaceDetailPage, this);
}

/*******************************************************************************
CDoubleSurface::SurfaceThicknessPage - Shows the page.

inputs
   PCEscWindow    pWindow - window
returns
   PWSTR - Return string
*/
PWSTR CDoubleSurface::SurfaceThicknessPage (PCEscWindow pWindow)
{
   return pWindow->PageDialog (ghInstance, IDR_MMLSURFACETHICKNESS, ::SurfaceThicknessPage, this);
}

/*******************************************************************************
CDoubleSurface::SurfaceFramingPage - Shows the page.

inputs
   PCEscWindow    pWindow - window
returns
   PWSTR - Return string
*/
PWSTR CDoubleSurface::SurfaceFramingPage (PCEscWindow pWindow)
{
   return pWindow->PageDialog (ghInstance, IDR_MMLSURFACEFRAMING, ::SurfaceFramingPage, this);
}


/*******************************************************************************
CDoubleSurface::SurfaceBevellingPage - Shows the page.

inputs
   PCEscWindow    pWindow - window
returns
   PWSTR - Return string
*/
PWSTR CDoubleSurface::SurfaceBevellingPage (PCEscWindow pWindow)
{
   return pWindow->PageDialog (ghInstance, IDR_MMLSURFACEBEVELLING, ::SurfaceBevellingPage, this);
}



/*******************************************************************************
CDoubleSurface::SurfaceCurvaturePage - Shows the page.

inputs
   PCEscWindow    pWindow - window
returns
   PWSTR - Return string
*/
PWSTR CDoubleSurface::SurfaceCurvaturePage (PCEscWindow pWindow)
{
   return pWindow->PageDialog (ghInstance, IDR_MMLSURFACECURVATURE, ::SurfaceCurvaturePage, this);
}



/*******************************************************************************
CDoubleSurface::SurfaceShowCladPage - Shows the page.

inputs
   PCEscWindow    pWindow - window
returns
   PWSTR - Return string
*/
PWSTR CDoubleSurface::SurfaceShowCladPage (PCEscWindow pWindow)
{
   return pWindow->PageDialog (ghInstance, IDR_MMLSURFACESHOWCLAD, ::SurfaceShowCladPage, this);
}




/*******************************************************************************
CDoubleSurface::SurfaceFloorsPage - Shows the page.

inputs
   PCEscWindow    pWindow - window
returns
   PWSTR - Return string
*/
PWSTR CDoubleSurface::SurfaceFloorsPage (PCEscWindow pWindow)
{
   return pWindow->PageDialog (ghInstance, IDR_MMLSURFACEFLOORS, ::SurfaceFloorsPage, this);
}



/*******************************************************************************
CDoubleSurface::SurfaceControlPointsPage - Shows the page.

inputs
   PCEscWindow    pWindow - window
returns
   PWSTR - Return string
*/
PWSTR CDoubleSurface::SurfaceControlPointsPage (PCEscWindow pWindow)
{
   return pWindow->PageDialog (ghInstance, IDR_MMLSURFACECONTROLPOINTS, ::SurfaceControlPointsPage, this);
}


/*******************************************************************************
CDoubleSurface::SurfaceEdgeInterPage - Shows the page.

inputs
   PCEscWindow    pWindow - window
returns
   PWSTR - Return string
*/
PWSTR CDoubleSurface::SurfaceEdgeInterPage (PCEscWindow pWindow)
{
   BOOL fSideA = TRUE;

   // BUGFIX - Call same overlay intersection function as when do automatic
   // overlay for paint
   PCListFixed plSeq;
   plSeq = OverlayIntersections (fSideA);

#if 0 // OLDCODE
   // find the intersections
   PCListFixed plLines, plSeq;
   plSeq = NULL;
   // if it's a floor will need to overshoot
   int iFloor;
   iFloor = (HIWORD(m_dwType) == 3) ? 2 : 1;
   plLines = FindIntersections (0.0, fSideA ? iFloor : -iFloor, 5, 0x0001, 0x0001, TRUE);
   if (!plLines) {
      // start with blank because will put outline in
      plLines = new CListFixed;
      if (plLines)
         plLines->Init (sizeof(TEXTUREPOINT)*2);
   }
   if (plLines) {
      plSeq = LineSequencesForRoof (plLines, TRUE, TRUE);
      delete plLines;
   }
#endif //0

   CListFixed lSelected;
   lSelected.Init (sizeof(DWORD));

   SOMS soms;
   soms.fSideA = fSideA;
   soms.plSelected = &lSelected;
   soms.plSeq = plSeq;
   soms.pThis = this;

   PWSTR pszRet;
   pszRet = pWindow->PageDialog (ghInstance, IDR_MMLSURFACEEDGEINTER, ::SurfaceEdgeInterPage, &soms);
   
   if (plSeq) {
      DWORD i;
      for (i = 0; i < plSeq->Num(); i++) {
         PCListFixed pl = *((PCListFixed*) plSeq->Get(i));
         delete pl;
      }
      delete plSeq;
   }

   return pszRet;
}
/*******************************************************************************
CDoubleSurface::SurfaceEdgePage - Shows the page.

inputs
   PCEscWindow    pWindow - window
returns
   PWSTR - Return string
*/
PWSTR CDoubleSurface::SurfaceEdgePage (PCEscWindow pWindow)
{
   return pWindow->PageDialog (ghInstance, IDR_MMLSURFACEEDGE, ::SurfaceEdgePage, this);
}



/*******************************************************************************
CDoubleSurface::SurfaceOverMainPage - Shows the page.

inputs
   PCEscWindow    pWindow - window
   BOOL           fSideA - set to side A if mucking with overlays on side a
   DWORD          *pdwDisplayControl - Pointer to the m_dwDisplayControl value
                     so it gets changed to display the right points
returns
   PWSTR - Return string
*/
PWSTR CDoubleSurface::SurfaceOverMainPage (PCEscWindow pWindow, BOOL fSideA,
                                           DWORD *pdwDisplayControl)
{

   // BUGFIX - Call same overlay intersection function as when do automatic
   // overlay for paint
   PCListFixed plSeq;
   plSeq = OverlayIntersections (fSideA);


   CListFixed lSelected;
   PWSTR pszRet = NULL;
   lSelected.Init (sizeof(DWORD));

   SOMS soms;
   soms.fSideA = fSideA;
   soms.plSelected = &lSelected;
   soms.plSeq = plSeq;
   soms.pThis = this;
   soms.pdwCurColor = pdwDisplayControl;

overmainpage:
   pszRet = pWindow->PageDialog (ghInstance, IDR_MMLSURFACEOVERMAIN, ::SurfaceOverMainPage, &soms);
   if (!pszRet)
      goto cleanup;
   if (!_wcsicmp(pszRet, L"addinter")) {
      pszRet = pWindow->PageDialog (ghInstance, IDR_MMLSURFACEOVERADDINTER, SurfaceOverAddInterPage, &soms);
      if (!pszRet)
         goto cleanup;
      if (!_wcsicmp(pszRet, gszBack))
         goto overmainpage;
      goto cleanup;
   }
   else if (!_wcsicmp(pszRet, L"addcustom")) {
      pszRet = pWindow->PageDialog (ghInstance, IDR_MMLSURFACEOVERADDCUSTOM, SurfaceOverAddCustomPage, &soms);
      if (!pszRet)
         goto cleanup;
      if (!_wcsicmp(pszRet, gszBack))
         goto overmainpage;
      goto cleanup;
   }
   else if (!_wcsicmp(pszRet, L"editinter")) {
      pszRet = pWindow->PageDialog (ghInstance, IDR_MMLSURFACEOVEREDITINTER, SurfaceOverEditInterPage, &soms);
      if (!pszRet)
         goto cleanup;
      if (!_wcsicmp(pszRet, gszBack))
         goto overmainpage;
      goto cleanup;
   }
   else if (!_wcsicmp(pszRet, L"editedge")) {
      pszRet = pWindow->PageDialog (ghInstance, IDR_MMLSURFACEOVEREDITEDGE, SurfaceOverEditEdgePage, &soms);
      if (!pszRet)
         goto cleanup;
      if (!_wcsicmp(pszRet, gszBack))
         goto overmainpage;
      goto cleanup;
   }
   else if (!_wcsicmp(pszRet, L"editcustom")) {
      pszRet = pWindow->PageDialog (ghInstance, IDR_MMLSURFACEOVEREDITCUSTOM, SurfaceOverEditCustomPage, &soms);
      if (!pszRet)
         goto cleanup;
      if (!_wcsicmp(pszRet, gszBack))
         goto overmainpage;
      goto cleanup;
   }
   goto cleanup;


cleanup:
   if (plSeq) {
      DWORD i;
      for (i = 0; i < plSeq->Num(); i++) {
         PCListFixed pl = *((PCListFixed*) plSeq->Get(i));
         delete pl;
      }
      delete plSeq;
   }
   return pszRet;
}


/********************************************************************************
DSGenerateThreeDFromCutouts - Draws the splinesurface with all of the edges.
   It draws:
         1) The outline of the surface
         3) The cutouts in different colors.

inputs
   PWSTR       pszControl - Control name.
   PCEscPage   pPage - Page
   PCSplineSurface pss - Surface
   BOOL        fSideA - If its side B (FALSE) need to flip coordinatese
returns
   BOOl - TRUE if success

*/
BOOL DSGenerateThreeDFromCutouts (PWSTR pszControl, PCEscPage pPage, PCSplineSurface pss,
                                        BOOL fSideA)
{
   PCEscControl pControl = pPage->ControlFind (pszControl);
   if (!pControl)
      return FALSE;

   // figure out the center
   CPoint pCenter, pTemp, pMin, pMax;
   DWORD i;
   pCenter.Zero();
   pMin.Zero();
   pMax.Zero();
   DWORD x, y, dwWidth, dwHeight;
   dwWidth = pss->ControlNumGet (TRUE);
   dwHeight = pss->ControlNumGet (FALSE);
   for (y = 0; y < dwHeight; y++) for (x = 0; x < dwWidth; x++) {
      if (!pss->ControlPointGet (x, y, &pTemp))
         return FALSE;
      if ((y == 0) && (x == 0)) {
         pMin.Copy (&pTemp);
         pMax.Copy (&pTemp);
         continue;
      }

      // else, do min/max
      for (i = 0; i < 3; i++) {
         pMin.p[i] = min(pMin.p[i], pTemp.p[i]);
         pMax.p[i] = max(pMax.p[i], pTemp.p[i]);
      }
   }
   pCenter.Copy (&pMin);
   pCenter.Add (&pMax);
   pCenter.Scale (.5);

   // figure out the maximum distance
   fp fTemp, fMax;
   fMax = 0.001;
   for (y = 0; y < dwHeight; y++) for (x = 0; x < dwWidth; x++) {
      if (!pss->ControlPointGet (x, y, &pTemp))
         return FALSE;
      pTemp.Subtract (&pCenter);
      fTemp = pTemp.Length();
      if (fTemp > fMax)
         fMax = fTemp;
   }
   fMax /= 5;  // so is larger

   // when draw points, get the point, subtract the center, and divide by fMax

   // use gmemtemp
   MemZero (&gMemTemp);
   if (!fSideA)
      MemCat (&gMemTemp, L"<rotatey val=180/>");
   MemCat (&gMemTemp, L"<rotatex val=-90/><backculloff/>");   //  so that Z is up (on the screen)

   // draw the surface as wireframe
   MemCat (&gMemTemp, L"<id val=1/><wireframeon/>");
   WCHAR szTemp[128];
   DWORD dwYMax, dwXMax;
   // BUGFIX - Deal with loop check
   dwXMax = dwWidth - (pss->LoopGet(TRUE) ? 0 : 1);
   dwYMax = dwHeight - (pss->LoopGet(FALSE) ? 0 : 1);
   for (y = 0; y < dwYMax; y++) for (x = 0; x < dwXMax; x++) {
      MemCat (&gMemTemp, L"<colordefault color=#404080/>");

      MemCat (&gMemTemp, L"<shapepolygon");

      for (i = 0; i < 4; i++) {
         switch (i) {
         case 0: // UL
            pss->ControlPointGet (x,y, &pTemp);
            break;
         case 1: // UR
            pss->ControlPointGet ((x+1)%dwWidth,y, &pTemp);
            break;
         case 2: // LR
            pss->ControlPointGet ((x+1)%dwWidth,(y+1)%dwHeight, &pTemp);
            break;
         case 3: // LL
            pss->ControlPointGet (x,(y+1)%dwHeight, &pTemp);
            break;
         }
         pTemp.Subtract (&pCenter);
         pTemp.Scale (1.0 / fMax);

         swprintf (szTemp, L" p%d=%g,%g,%g",
            (int) i+1, (double)pTemp.p[0], (double)pTemp.p[1], (double)pTemp.p[2]);
         MemCat (&gMemTemp, szTemp);
      }

      MemCat (&gMemTemp, L"/>");
   }

   MemCat (&gMemTemp, L"<wireframeoff/>");

   // draw the currend edge
   DWORD dwNum;
   PWSTR pszName;
   PTEXTUREPOINT ptp;
   BOOL fClockwise;
   // loop through the overlays
   for (i = 0; i < pss->CutoutNum(); i++) {
      if (!pss->CutoutEnum (i, &pszName, &ptp, &dwNum, &fClockwise))
         continue;

      // dont show edges caused by object cutouts or the edge setting
      if ((pszName[0] == L'{') || (!_wcsicmp(pszName, L"edge")))
         continue;

      MemCat (&gMemTemp, L"<id val=");
      MemCat (&gMemTemp, (int)(0x1000 | i));
      MemCat (&gMemTemp, L"/>");

      // color
      switch (i % 6) {
      case 0:  // red
         MemCat (&gMemTemp, L"<colordefault color=#ff0000/>");
         break;
      case 1:  // green
         MemCat (&gMemTemp, L"<colordefault color=#00ff00/>");
         break;
      case 2:  // blue
         MemCat (&gMemTemp, L"<colordefault color=#0000ff/>");
         break;
      case 3:  // ellow
         MemCat (&gMemTemp, L"<colordefault color=#ffff00/>");
         break;
      case 4:  // blue-green
         MemCat (&gMemTemp, L"<colordefault color=#00ffff00/>");
         break;
      case 5:  // purple
         MemCat (&gMemTemp, L"<colordefault color=#ff00ff/>");
         break;
      }

      // loop through all the points
      for (x = 0; x < dwNum; x++) {
         CPoint p1, p2;

         // convert from HV to object space
         pss->HVToInfo (ptp[x].h, ptp[x].v, &p1);
         pss->HVToInfo (ptp[(x+1)%dwNum].h, ptp[(x+1)%dwNum].v, &p2);
         p1.Subtract (&pCenter);
         p2.Subtract (&pCenter);
         p1.Scale (1.0 / fMax);
         p2.Scale (1.0 / fMax);


         MemCat (&gMemTemp, L"<shapearrow tip=true width=.15");

         swprintf (szTemp, L" p1=%g,%g,%g p2=%g,%g,%g/>",
            (double)p1.p[0], (double)p1.p[1], (double)p1.p[2], (double)p2.p[0], (double)p2.p[1], (double)p2.p[2]);
         MemCat (&gMemTemp, szTemp);
      }
   }


   // set the threeD control
   ESCMTHREEDCHANGE tc;
   memset (&tc, 0, sizeof(tc));
   tc.pszMML = (PWSTR) gMemTemp.p;
   pControl->Message (ESCM_THREEDCHANGE, &tc);

   return TRUE;
}


/* SurfaceCutoutAddInterPage
*/
BOOL SurfaceCutoutAddInterPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PSOMS ps = (PSOMS)pPage->m_pUserData;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         // draw images
         ps->plSelected->Clear();
         GenerateThreeDFromSegments (L"overlayedit", pPage,
            ps->fSideA ? &ps->pThis->m_SplineA : &ps->pThis->m_SplineB,
            ps->plSeq, ps->plSelected, ps->fSideA);
      }
      break;

   case ESCN_THREEDCLICK:
      {
         PESCNTHREEDCLICK p = (PESCNTHREEDCLICK) pParam;
         if (!p->pControl->m_pszName)
            break;

         if (_wcsicmp(p->pControl->m_pszName, L"overlayedit"))
            break;

         // click on the right object
         DWORD x, i;
         if (p->dwMajor < 0x1000)
            return TRUE;
         x = p->dwMajor - 0x1000;
         if (x >= ps->plSeq->Num())
            return TRUE;

         // reverse selection
         DWORD *padw, dwNum;
         padw = (DWORD*) ps->plSelected->Get(0);
         dwNum = ps->plSelected->Num();
         for (i = 0; i < dwNum; i++)
            if (padw[i] == x)
               break;
         if (i < dwNum)
            ps->plSelected->Remove(i);
         else
            ps->plSelected->Add (&x);

         // redraw
         GenerateThreeDFromSegments (L"overlayedit", pPage,
            ps->fSideA ? &ps->pThis->m_SplineA : &ps->pThis->m_SplineB,
            ps->plSeq, ps->plSelected,ps->fSideA);
      }
      break;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;

         // if it's for the colors then do those
         if (!_wcsicmp(p->pControl->m_pszName, L"addclock") || !_wcsicmp(p->pControl->m_pszName, L"addcounter")) {
            BOOL fClock = !_wcsicmp(p->pControl->m_pszName, L"addclock");

            // try to put the pieces together
            PCListFixed pltp;
            pltp = PathToSplineListFillInGaps(ps->plSeq, (DWORD*) ps->plSelected->Get(0), ps->plSelected->Num(),
               fClock);
            if (!pltp) {
               pPage->MBWarning (L"Please click on the intersections to enclose an area.");
               return TRUE;
            }

            ps->pThis->m_pTemplate->m_pWorld->ObjectAboutToChange (ps->pThis->m_pTemplate);

            WCHAR szTemp[64];
            PCSplineSurface pss;
            pss = ps->fSideA ? &ps->pThis->m_SplineA : &ps->pThis->m_SplineB;
            swprintf (szTemp, L"AddInter%d-%d-%d", (int) Today(), (int) Now(), (int)rand());
            pss->CutoutSet (szTemp, (PTEXTUREPOINT)pltp->Get(0),pltp->Num(), fClock);
            
            ps->pThis->m_pTemplate->m_pWorld->ObjectChanged (ps->pThis->m_pTemplate);

            delete pltp;

            pPage->Exit (gszBack);
            return TRUE;
         }
      }
      break;   // default

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Add a cutout based on intersections";
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}


/* SurfaceCutoutMainPage
*/
BOOL SurfaceCutoutMainPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PSOMS ps = (PSOMS)pPage->m_pUserData;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         DSGenerateThreeDFromCutouts (L"overlayedit", pPage,
            ps->fSideA ? &ps->pThis->m_SplineA : &ps->pThis->m_SplineB,
            ps->fSideA);
      }
      break;

   case ESCN_THREEDCLICK:
      {
         PESCNTHREEDCLICK p = (PESCNTHREEDCLICK) pParam;
         if (!p->pControl->m_pszName)
            break;

         if (_wcsicmp(p->pControl->m_pszName, L"overlayedit"))
            break;
         DWORD x;
         if (p->dwMajor < 0x1000)
            return TRUE;
         x = p->dwMajor - 0x1000;
         
         PCSplineSurface pss;
         pss = ps->fSideA ?&ps->pThis->m_SplineA : &ps->pThis->m_SplineB;
         if (x >= pss->CutoutNum())
            return FALSE;


         PWSTR pszName;
         DWORD dwNumPoints;
         PTEXTUREPOINT ptp;
         BOOL fClockwise;
         pss->CutoutEnum (x, &pszName, &ptp, &dwNumPoints, &fClockwise);

         ps->pThis->m_pTemplate->m_pWorld->ObjectAboutToChange (ps->pThis->m_pTemplate);

         pss->CutoutRemove (pszName);
         ps->pThis->m_pTemplate->m_pWorld->ObjectChanged (ps->pThis->m_pTemplate);

         // redraw the shapes
         DSGenerateThreeDFromCutouts (L"overlayedit", pPage,
            ps->fSideA ? &ps->pThis->m_SplineA : &ps->pThis->m_SplineB,
            ps->fSideA);
       
         return TRUE;
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Cutouts";
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}


/*******************************************************************************
CDoubleSurface::SurfaceCutoutMainPage - Shows the page.

inputs
   PCEscWindow    pWindow - window
   BOOL           fSideA - set to side A if mucking with overlays on side a
returns
   PWSTR - Return string
*/
PWSTR CDoubleSurface::SurfaceCutoutMainPage (PCEscWindow pWindow, BOOL fSideA)
{

   // BUGFIX - Call same overlay intersection function as when do automatic
   // overlay for paint
   PCListFixed plSeq;
   plSeq = OverlayIntersections (fSideA);

#if 0 // OLDCODE
   // find the intersections
   PCListFixed plLines, plSeq;
   plSeq = NULL;
   // if it's a floor will need to overshoot
   int iFloor;
   iFloor = (HIWORD(m_dwType) == 3) ? 2 : 1;
   plLines = FindIntersections (INTERSECTEXTEND, fSideA ? iFloor : -iFloor, 5, 0x0001, 0x0001, TRUE);
      // BUGFIX - Was passing 0 to FindInersections. Changed to 5 so only give center
      // BUGFIX - Extend by .05 when find interstions. Was 0
   if (!plLines) {
      // start with blank because will put outline in
      plLines = new CListFixed;
      if (plLines)
         plLines->Init (sizeof(TEXTUREPOINT)*2);
   }
   if (plLines) {
      plSeq = LineSequencesForRoof (plLines, TRUE, TRUE);
      delete plLines;
   }
#endif // 0

   CListFixed lSelected;
   lSelected.Init (sizeof(DWORD));

   SOMS soms;
   soms.fSideA = fSideA;
   soms.plSelected = &lSelected;
   soms.plSeq = plSeq;
   soms.pThis = this;
   soms.pdwCurColor = 0;
   PWSTR pszRet;

overmainpage:
   pszRet = pWindow->PageDialog (ghInstance, IDR_MMLSURFACECUTOUTMAIN, ::SurfaceCutoutMainPage, &soms);
   if (!pszRet)
      goto cleanup;
   if (!_wcsicmp(pszRet, L"addinter")) {
      pszRet = pWindow->PageDialog (ghInstance, IDR_MMLSURFACECUTOUTADDINTER, SurfaceCutoutAddInterPage, &soms);
      if (!pszRet)
         goto cleanup;
      if (!_wcsicmp(pszRet, gszBack))
         goto overmainpage;
      goto cleanup;
   }
   goto cleanup;


cleanup:
   if (plSeq) {
      DWORD i;
      for (i = 0; i < plSeq->Num(); i++) {
         PCListFixed pl = *((PCListFixed*) plSeq->Get(i));
         delete pl;
      }
      delete plSeq;
   }
   return pszRet;
}

/**************************************************************************************
CDoubleSurface::ShrinkToCutouts - Shrinks the fp surface so that there's no
empty space around from the cutouts.

inputs
   BOOL     fQuery - If TRUE, this is only asking if it can be done. FALSE it wants it done
returns
   BOOL - TRUE if successful
*/
BOOL CDoubleSurface::ShrinkToCutouts (BOOL fQuery)
{
   // if it's not a strictly linear piece can't do
   if ((m_SplineCenter.ControlNumGet(TRUE) != 2) || (m_SplineCenter.ControlNumGet(FALSE) != 2))
      return FALSE;
   if (m_SplineCenter.LoopGet(TRUE) || m_SplineCenter.LoopGet(FALSE))
      return FALSE;
   if ((m_SplineCenter.SegCurveGet (TRUE, 0) != SEGCURVE_LINEAR) || (m_SplineCenter.SegCurveGet (FALSE, 0) != SEGCURVE_LINEAR))
      return FALSE;

   // if any of the control points have a Y of non-zero then cant use
   DWORD x,y;
   fp fLastY;
   for (x = 0; x < 2; x++) for (y = 0; y < 2; y++) {
      CPoint p;
      m_SplineCenter.ControlPointGet (x, y, &p);

      if ((x == 0) && (y == 0))
         fLastY = p.p[1];
      else
         if (fabs(p.p[1] - fLastY) > CLOSE)
            return FALSE;
   }

   // find the minimum and maximum h,v
   TEXTUREPOINT tpMin, tpMax, tpMinB, tpMaxB;
   BOOL fFoundA, fFoundB;
   fFoundA = m_SplineA.FindMinMaxHV (&tpMin, &tpMax);
   fFoundB = m_SplineB.FindMinMaxHV (&tpMinB, &tpMaxB);
   if (!fFoundA && !fFoundB)
      return FALSE;
   if (fFoundB) {
      // take into account that B side has inverted (essentially) h
      fp f1, f2;
      f1 = 1.0 - tpMinB.h;
      f2 = 1.0 - tpMaxB.h;
      tpMinB.h = min(f1,f2);
      tpMaxB.h = max(f1,f2);

      if (fFoundA) {
         tpMin.h = min(tpMin.h, tpMinB.h);
         tpMin.v = min(tpMin.v, tpMinB.v);
         tpMax.h = max(tpMax.h, tpMaxB.h);
         tpMax.v = max(tpMax.v, tpMaxB.v);
      }
      else {
         // nothing in A, so just copy over
         tpMin = tpMinB;
         tpMax = tpMaxB;
      }
   }

   // BUGFIX - If any of the cutouts are beyond the clipping area AND they arent
   // square (90 degrees) then can't cut down because not doing intelligent clipping
   // of them
   CPoint p;
   DWORD i, dwSide, dwCutout, dwElem, j;
   TEXTUREPOINT tp;
   for (dwSide = 0; dwSide < 2; dwSide++) {
      PCSplineSurface pss = (dwSide ? &m_SplineB : &m_SplineA);
      for (dwCutout = 0; dwCutout < 2; dwCutout++) {
         dwElem = dwCutout ? pss->CutoutNum() : pss->OverlayNum();
         PWSTR pszName;
         PTEXTUREPOINT pt;
         DWORD dwNum;
         BOOL fClockwise;
         for (i = 0; i < dwElem; i++) {
            dwNum = 0;
            pt = NULL;
            if (dwCutout)
               pss->CutoutEnum (i, &pszName, &pt, &dwNum, &fClockwise);
            else
               pss->OverlayEnum (i, &pszName, &pt, &dwNum, &fClockwise);

            for (j = 0; j < dwNum; j++) {
               // if it's a straight line EW or NS then dont care what its bounds are
               tp.h = fabs(pt[j].h - pt[(j+1)%dwNum].h);
               tp.v = fabs(pt[j].v - pt[(j+1)%dwNum].v);
               if ((tp.h < CLOSE) || (tp.v < CLOSE))
                  continue;

               // BUGFIX - If on side B then flip h
               TEXTUREPOINT tpA, tpB;
               tpA = pt[j];
               tpB = pt[(j+1)%dwNum];
               if (dwSide) {
                  tpA.h = 1 - tpA.h;
                  tpB.h = 1 - tpB.h;
               }

               // else, it's diagonal.. do a min and max so that it wont
               // ever get into a position where it's clipped
               tpMin.h = min(tpMin.h, tpA.h);
               tpMin.h = min(tpMin.h, tpB.h);
               tpMin.v = min(tpMin.v, tpA.v);
               tpMin.v = min(tpMin.v, tpB.v);
               tpMax.h = max(tpMax.h, tpA.h);
               tpMax.h = max(tpMax.h, tpB.h);
               tpMax.v = max(tpMax.v, tpA.v);
               tpMax.v = max(tpMax.v, tpB.v);
            }
         }
      }  // dwcutout
   } // dwSide


   // if the edge is not line sequences then cant shrink below edge
   BOOL fEdgeOK;
   fEdgeOK = TRUE;
   for (i = 0; i < m_SplineEdge.OrigNumPointsGet(); i++) {
      DWORD dwCurve;
      m_SplineEdge.OrigSegCurveGet (i, &dwCurve);
      if (dwCurve != SEGCURVE_LINEAR)
         fEdgeOK = FALSE;
   }

#if 0 // dont need to do this because of check for cutouts and overlays
   // BUGFIX - If the edge is not rectangular then cant do this either
   // because will cause problems because I'm not intelligently clipping the edge
   // against the new bounding box, so only a rectangle can be accuratly produced
   for (i = 0; fEdgeOK && (i < m_SplineEdge.OrigNumPointsGet()); i++) {
      m_SplineEdge.OrigPointGet (i, &p);
      m_SplineEdge.OrigPointGet ((i+1) % m_SplineEdge.OrigNumPointsGet(), &p2);
      p2.Subtract (&p);
      // if edge line isn't perpendicular then won't work
      if ((fabs(p2.p[0]) > CLOSE) && (fabs(p2.p[1] > CLOSE)))
         fEdgeOK = FALSE;
   }
#endif

   if (!fEdgeOK) {
      TEXTUREPOINT tpEdgeMin, tpEdgeMax;
      for (i = 0; i < m_SplineEdge.OrigNumPointsGet(); i++) {
         m_SplineEdge.OrigPointGet (i, &p);
         if (i) {
            tpEdgeMin.h = min(tpEdgeMin.h, p.p[0]);
            tpEdgeMin.v = min(tpEdgeMin.v, p.p[1]);
            tpEdgeMax.h = max(tpEdgeMax.h, p.p[0]);
            tpEdgeMax.v = max(tpEdgeMax.v, p.p[1]);
         }
         else {
            // nothing in A, so just copy over
            tpEdgeMin.h = p.p[0];
            tpEdgeMin.v = p.p[1];
            tpEdgeMax = tpEdgeMin;
         }
      }

      tpMin.h = min(tpMin.h, tpEdgeMin.h);
      tpMax.h = max(tpMax.h, tpEdgeMax.h);
      tpMin.v = min(tpMin.v, tpEdgeMin.v);
      tpMax.v = max(tpMax.v, tpEdgeMax.v);
   }

#ifdef _DEBUG
      if ((tpMin.h < -EPSILON) || (tpMin.h > 1+EPSILON) ||
         (tpMin.v < -EPSILON) || (tpMin.v > 1+EPSILON))
         OutputDebugString ("WARNING: ShrinkToCutout too large\r\n");
      if ((tpMax.h < -EPSILON) || (tpMax.h > 1+EPSILON) ||
         (tpMax.v < -EPSILON) || (tpMax.v > 1+EPSILON))
         OutputDebugString ("WARNING: ShrinkToCutout too large\r\n");
#endif

   tpMin.h = max(0,tpMin.h);
   tpMin.v = max(0,tpMin.v);
   tpMax.h = min(1,tpMax.h);
   tpMax.v = min(1,tpMax.v);

   // if the minimum and maximum are both close to the edge then nothing to do
   if ((tpMin.h < EPSILON) && (tpMin.v < EPSILON) && (tpMax.h > 1-EPSILON) && (tpMax.v > 1-EPSILON))
      return FALSE;

   if ((fabs(tpMin.h - tpMax.h) < EPSILON) || (fabs(tpMin.v - tpMax.v) < EPSILON))
      return FALSE;  // nothing left


   // if just querying then can change
   if (fQuery)
      return TRUE;

   // will need to change the bevel
   if (m_pTemplate->m_pWorld)
      m_pTemplate->m_pWorld->ObjectAboutToChange(m_pTemplate);

   // reverse so it's effectively pointing inwards
   m_fBevelLeft *= -1;
   m_fBevelTop *= -1;
   fp fLeft, fTop, fRight, fBottom;
   fLeft = tpMin.h * m_fBevelRight + (1.0 - tpMin.h) * m_fBevelLeft;
   fRight = tpMax.h * m_fBevelRight + (1.0 - tpMax.h) * m_fBevelLeft;
   fTop = tpMin.v * m_fBevelBottom + (1.0 - tpMin.v) * m_fBevelTop;
   fBottom = tpMax.v * m_fBevelBottom + (1.0 - tpMax.v) * m_fBevelTop;
   m_fBevelLeft = fLeft;
   m_fBevelRight = fRight;
   m_fBevelTop = fTop;
   m_fBevelBottom = fBottom;
   // undo reversal
   m_fBevelLeft *= -1;
   m_fBevelTop *= -1;

   // get the dimensions of the corners and change those
   CPoint ap[2][2];
   for (x = 0; x < 2; x++) for (y = 0; y < 2; y++)
      m_SplineCenter.ControlPointGet (x, y, &ap[x][y]);
   for (x = 0; x < 2; x++) for (y = 0; y < 2; y++) {
      CPoint p1, p2, p;

      // interpolate up and down
      p1.Subtract (&ap[0][1], &ap[0][0]);
      p1.Scale (y ? tpMax.v : tpMin.v);
      p1.Add (&ap[0][0]);
      p2.Subtract (&ap[1][1], &ap[1][0]);
      p2.Scale (y ? tpMax.v : tpMin.v);
      p2.Add (&ap[1][0]);

      // interpolate left and right
      p.Subtract (&p2, &p1);
      p.Scale (x ? tpMax.h : tpMin.h);
      p.Add (&p1);

      // use this
      m_SplineCenter.ControlPointSet (x, y, &p);
   }
   RecalcSides ();

   // now call changedHV
   // BUGFIX - Need negative numbers so that will do some clipping
   fp afHOld[2], afHNew[2];
   fp afVOld[2], afVNew[2];
   fp fDeltaH, fDeltaV;
   fDeltaH = tpMax.h - tpMin.h;
   fDeltaV = tpMax.v - tpMin.v;
   afHOld[0] = 0;
   afHNew[0] = -tpMin.h / fDeltaH;
   //afHOld[1] = tpMin.h;
   //afHNew[1] = 0;
   //afHOld[2] = tpMax.h;
   //afHNew[2] = 1;
   afHOld[1] = 1;
   afHNew[1] = 1 + (1-tpMax.h) / fDeltaH;

   afVOld[0] = 0;
   afVNew[0] = -tpMin.v / fDeltaV;
   //afVOld[1] = tpMin.v;
   //afVNew[1] = 0;
   //afVOld[2] = tpMax.v;
   //afVNew[2] = 1;
   afVOld[1] = 1;
   afVNew[1] = 1 + (1-tpMax.v) / fDeltaV;

   ChangedHV (2, 2, afHOld, afVOld, afHNew, afVNew);

   if (m_pTemplate->m_pWorld)
      m_pTemplate->m_pWorld->ObjectChanged(m_pTemplate);
   return TRUE;
}


/*********************************************************************************
ClipCutout - Used to clip a cutout or overlay to boundary of surface.

inputs
   PTEXTUREPOINT     ptp - Original texture points
   DWORD             dwNum - Number of points
   BOOL              fClockwise - TRUE if it's clockwise.
reutrns
   PVOID - Returns ptp if no change is necessary.
            PCListFixed (which must be freed by caller) if new points. List of TEXTUREPOINTS
            NULL if clipped completely out
*/
PVOID ClipCutout (PTEXTUREPOINT ptp, DWORD dwNum, BOOL fClockwise)
{
   BOOL fBeyondBounds = FALSE;
   DWORD dwp;
   for (dwp = 0; dwp < dwNum; dwp++)
      fBeyondBounds |= (ptp[dwp].h < 0) || (ptp[dwp].v < 0) || (ptp[dwp].h > 1) || (ptp[dwp].v > 1);
   if (!fBeyondBounds)
      return ptp;

   // create the polygon
   POLYRENDERINFO info;
   CMem memPoints, memVertices, memPoly;
   memPoints.Required (sizeof(CPoint) * dwNum);
   memVertices.Required (sizeof(VERTEX) * dwNum);
   memPoly.Required(sizeof(POLYDESCRIPT) + dwNum * sizeof(DWORD));
   PCPoint paPoints;
   paPoints = (PCPoint) memPoints.p;
   PVERTEX paVertices;
   PPOLYDESCRIPT pPoly;
   DWORD *padwPoly;
   paVertices = (PVERTEX) memVertices.p;
   pPoly = (PPOLYDESCRIPT) memPoly.p;
   padwPoly = (DWORD*) (pPoly+1);
   memset (&info, 0, sizeof(info));
   info.dwNumPoints = dwNum;
   info.paPoints = paPoints;
   info.dwNumVertices = dwNum;
   info.paVertices = paVertices;
   info.dwNumPolygons = 1;
   info.paPolygons = pPoly;

   // fill in
   for (dwp = 0; dwp < dwNum; dwp++) {
      DWORD dwFrom;
      if (fClockwise)
         dwFrom = dwp;
      else
         dwFrom = dwNum - dwp - 1;
      paPoints[dwp].Zero();
      paPoints[dwp].p[0] = ptp[dwFrom].h;
      paPoints[dwp].p[1] = ptp[dwFrom].v;

      memset (&paVertices[dwp], 0, sizeof(VERTEX));
      paVertices[dwp].dwPoint = dwp;
      padwPoly[dwp] = dwp;
   }
   memset (pPoly, 0, sizeof(*pPoly));
   pPoly->wNumVertices = (WORD) dwNum;

   // clip it
   CRenderClip clip;
   CPoint pNormal, pPlane;

   // left plane
   pNormal.Zero4();
   pNormal.p[0] = -1;
   pPlane.Zero4();
   clip.AddPlane (&pNormal, &pPlane);
   // right plane
   pNormal.Zero4();
   pNormal.p[0] = 1;
   pPlane.Zero4();
   pPlane.p[0] = 1;
   clip.AddPlane (&pNormal, &pPlane);
   // top plane
   pNormal.Zero4();
   pNormal.p[1] = -1;
   pPlane.Zero4();
   clip.AddPlane (&pNormal, &pPlane);
   // bottom plane
   pNormal.Zero4();
   pNormal.p[1] = 1;
   pPlane.Zero4();
   pPlane.p[1] = 1;
   clip.AddPlane (&pNormal, &pPlane);

   clip.ClipPolygons (-1, &info);
   if (!info.dwNumPolygons)
      return NULL;

   // convert this back
   PCListFixed plt;
   plt = new CListFixed;
   if (!plt)
      return ptp;
   plt->Init (sizeof(TEXTUREPOINT));

   pPoly = info.paPolygons;
   padwPoly = (DWORD*) (pPoly+1);
   for (dwp = 0; dwp < (DWORD) pPoly->wNumVertices; dwp++) {
      DWORD dwLookAt;
      if (fClockwise)
         dwLookAt = dwp;
      else
         dwLookAt = (DWORD)pPoly->wNumVertices - dwp - 1;

      PVERTEX pv = info.paVertices + padwPoly[dwLookAt];
      PCPoint pp = info.paPoints + pv->dwPoint;

      TEXTUREPOINT tp;
      tp.h = pp->p[0];
      tp.v = pp->p[1];
      plt->Add (&tp);
   }

   return plt;
}

/**************************************************************************************
CDoubleSurface::ChangedHV - Internal function that is called whenever the HV
relationship of the surface spline is somehow changed - generally by inserting or
removing rows/columns of control points. What it does is loop through all the
variables that remember location as HV and adjust them.

Basically, all points left of fHLeftOrig (pafHOrig[0] are expanded/squashed to be between 0 and
fHleftNew (pafHNew[0]). Between fHleftOrig (pafHOrig[0]) and fHRightOrig (pafHOrig[1]) are expaneded/squashed to bwtween
fHLeftNew (pafHNew[0]) and fHRightNew (pafHNew[1]). And right of fHRightOrig, expanded/squashed to be between
fHRightNew and 1. Same for V params.

NOTE: This does NOT call m_pWorld->AboutToChange() and m_pWorld->Changed(0.

inputs
   DWORD       dwH - Number of points in pafHOrig and pafHNew
   DWOD        dwV - Number of points in pafVOrig and pafVNew
   fp      fHLeftOrig, fHRightOrig, fVTopOrig, fVBottomOrig - Location from 0..1
               of original values of H and V.
   fp      fHLeftNew, fHRightNew, fVTopNew, fVBottomNew - Location from 0..1 for new
   BOOL        fMinMax - Set to TRUE if want to min/max it
returns
   none
*/
void FitTo (PTEXTUREPOINT pt, DWORD dwFitH, DWORD dwFitV, fp *afHOrig, fp *afVOrig, fp *afHNew, fp *afVNew, fp *afHScale, fp *afVScale,
            BOOL fMinMax)
{
   DWORD dwFit;

   // hozontal
   for(dwFit=0;dwFit<dwFitH;dwFit++) {
      if (pt->h < afHOrig[dwFit]) break;
      if (pt->h >= afHOrig[dwFit+1]) continue;
      pt->h = (pt->h - afHOrig[dwFit]) * afHScale[dwFit] + afHNew[dwFit];
      break;
   };
   // BUGFIX - Disable this because it's causing problems when there's
   // more than one cutout and decompose since sometimes then nubmers end
   // up being > 1
   if (fMinMax) {
      pt->h = max(0,pt->h);
      pt->h = min(1,pt->h);
   }

   // vertical
   for(dwFit=0;dwFit<dwFitV;dwFit++) {
      if (pt->v < afVOrig[dwFit]) break;
      if (pt->v >= afVOrig[dwFit+1]) continue;
      pt->v = (pt->v - afVOrig[dwFit]) * afVScale[dwFit] + afVNew[dwFit];
      break;
   };
   if (fMinMax) {
      pt->v = max(0,pt->v);
      pt->v = min(1,pt->v);
   }
}

#define FITTO(pt) FitTo(pt, dwH+1, dwV+1, afHOrig, afVOrig, afHNew, afVNew, afHScale, afVScale)
#define FITTO2(pt) FitTo(pt, dwH+1, dwV+1, afHOrig, afVOrig, afHNew, afVNew, afHScale, afVScale, FALSE)
#define MAXSPLIT  6

void CDoubleSurface::ChangedHV (DWORD dwH, DWORD dwV,
                                   fp *pafHOrig, fp *pafVOrig,
                                   fp *pafHNew, fp *pafVNew)
{
   // find the inverse scale
   fp afHScale[MAXSPLIT+1], afVScale[MAXSPLIT+1];
   fp afHOrig[MAXSPLIT+2], afVOrig[MAXSPLIT+2], afHNew[MAXSPLIT+2], afVNew[MAXSPLIT+2];

   if ((dwH > MAXSPLIT) || (dwV > MAXSPLIT))
      return;

   DWORD i;
   for (i = 0; i < dwH; i++) {
      afHOrig[i+1] = pafHOrig[i];
      afHNew[i+1] = pafHNew[i];
   }
   for (i = 0; i < dwV; i++) {
      afVOrig[i+1] = pafVOrig[i];
      afVNew[i+1] = pafVNew[i];
   }
   afHOrig[0] = afVOrig[0] = 0;
   afHNew[0] = afVNew[0] = 0;
   afHOrig[dwH+1] = afVOrig[dwV+1] = 1;
   afHNew[dwH+1] = afVNew[dwV+1] = 1;

   for (i = 0; i < dwH+1; i++) {
      if (afHOrig[i+1] > afHOrig[i])
         afHScale[i] = (afHNew[i+1] - afHNew[i]) / (afHOrig[i+1] - afHOrig[i]);
      else
         afHScale[i] = 0;
   }
   for (i = 0; i < dwV+1; i++) {
      if (afVOrig[i+1] > afVOrig[i])
         afVScale[i] = (afVNew[i+1] - afVNew[i]) / (afVOrig[i+1] - afVOrig[i]);
      else
         afVScale[i] = 0;
   }

   // loop through all the cutouts and overalys on side A and B and change them
   // NOTE: Theoretically I should be doing CutoutSet() and OverlaySet() instead
   // of just changing the memory in-place, but because I know will change
   // the edge, which will set dirty and cause a rebuild, don't bother
   DWORD dwSide;
   for (dwSide = 0; dwSide < 2; dwSide++) {
      PCSplineSurface pss = dwSide ? &m_SplineA : &m_SplineB;

      DWORD j;
      for (j = 0; j < 2; j++) {
         DWORD dwNum;
         dwNum = j ? pss->CutoutNum() : pss->OverlayNum();

         for (i = dwNum-1; i < dwNum; i--) {
            PWSTR pszName;
            DWORD dwNumPoints;
            PTEXTUREPOINT ptp;
            BOOL fClockwise;
            if (j)
               pss->CutoutEnum (i, &pszName, &ptp, &dwNumPoints, &fClockwise); 
            else
               pss->OverlayEnum (i, &pszName, &ptp, &dwNumPoints, &fClockwise);

            // loop through all the points
            DWORD dwp;
            for (dwp = 0; dwp < dwNumPoints; dwp++) {
               // if it's side B then reverse
               if (dwSide == 0)
                  ptp[dwp].h = 1.0 - ptp[dwp].h;

               // update the point
               FITTO2(&ptp[dwp]);

               // if it's side B then reverse back
               if (dwSide == 0)
                  ptp[dwp].h = 1.0 - ptp[dwp].h;
            } // loop over dwp - points in each cutout/overlay

            PVOID pRet;
            pRet = ClipCutout (ptp, dwNumPoints, fClockwise);
            if (pRet == NULL) {
               if (j)
                  pss->CutoutRemove (pszName); 
               else {
                  // NOTE: Don't remove overlay because otherwise things its
                  // attached to cant be removed
                  pss->OverlayRemove (pszName);
               }
            }
            else if (pRet != ptp) {
               PCListFixed plr = (PCListFixed)pRet;
               // reset it
               WCHAR szName[128];
               wcscpy (szName, pszName);
               if (j)
                  pss->CutoutSet (szName, (PTEXTUREPOINT) plr->Get(0), plr->Num(), fClockwise); 
               else
                  pss->OverlaySet (szName, (PTEXTUREPOINT) plr->Get(0), plr->Num(), fClockwise); 
               delete plr;
            }

         } // loop over dwNum
      }  // loop over cutout vs. overlay
   }  // loop over dwSide

   // Remap points in edge spline
   CMem  memPoints;
   CMem  memSegCurve;
   DWORD dwOrig;
   TEXTUREPOINT tp;
   dwOrig = m_SplineEdge.OrigNumPointsGet();
   if (memPoints.Required (dwOrig * sizeof(CPoint)) && memSegCurve.Required (dwOrig * sizeof(DWORD))) {
      PCPoint paPoints;
      DWORD *padw;
      paPoints = (PCPoint) memPoints.p;
      padw = (DWORD*) memSegCurve.p;
      DWORD i;
      for (i = 0; i < dwOrig; i++) {
         m_SplineEdge.OrigPointGet (i, paPoints+i);
         tp.h = paPoints[i].p[0];
         tp.v = paPoints[i].p[1];
         FITTO(&tp);
         paPoints[i].p[0] = tp.h;
         paPoints[i].p[1] = tp.v;
      }
      for (i = 0; i < dwOrig; i++)
         m_SplineEdge.OrigSegCurveGet (i, padw + i);
      DWORD dwMinDivide, dwMaxDivide;
      fp fDetail;
      m_SplineEdge.DivideGet (&dwMinDivide, &dwMaxDivide, &fDetail);
      m_SplineEdge.Init (TRUE, dwOrig, paPoints, NULL, padw, dwMinDivide, dwMaxDivide, fDetail);
      RecalcEdges ();
   }

   // remap all the floors
   DWORD dwNum;
   fp *paFloor;
   for (dwSide = 0; dwSide < 2; dwSide++) {
      paFloor = (fp*) (dwSide ? m_listFloorsA.Get(0) : m_listFloorsB.Get(0));
      dwNum = (dwSide ? m_listFloorsA.Num() : m_listFloorsB.Num());

      for (i = 0; i < dwNum; i++) {
         TEXTUREPOINT t;
         t.h = .5;
         t.v = paFloor[i];
         FITTO (&t);
         paFloor[i] = t.v;
      }
   }
   // tell all the objects they've moved so they end up redoing the cutouts
   //DWORD dwNum;
   fp fRotation;
   DWORD dwSurface;
   GUID gEmbed;
   dwNum = m_pTemplate->ContEmbeddedNum();
   if (m_pTemplate->m_pWorld) for (i = 0; i < dwNum; i++) {
      if (!m_pTemplate->ContEmbeddedEnum (i, &gEmbed))
         continue;
      if (!m_pTemplate->ContEmbeddedLocationGet (&gEmbed, &tp, &fRotation, &dwSurface))
         continue;

      // if this is not one of this objects surfaces then ignore
      PCLAIMSURFACE pcs;
      pcs = ClaimFindByID (dwSurface);
      if (!pcs)
         continue;

      // BUGFIX: If on Side B then flip H
      if ((pcs->dwReason == 1) || (pcs->dwReason == 4))
         tp.h = 1.0 - tp.h;

      // squash this
      FITTO (&tp);

      // BUGFIX: If on Side B then flip H
      if ((pcs->dwReason == 1) || (pcs->dwReason == 4))
         tp.h = 1.0 - tp.h;

      // pass it back to the object so it recalculates all its cutouts and stuff
      m_pTemplate->ContEmbeddedLocationSet (&gEmbed, &tp, fRotation, dwSurface);

   }
}


/*********************************************************************************
CDoubleSurface::IntersectLine - Intersects a line with the spline. If there is
more than one intersection point then it chooses the point closest to the start
of the line. NOTE: The line is not bounded by pStart or pDirection, but go beyond.

inputs
   int            iSide - side
   PCPoint        pStart - Start of the line.
   PCPoint        pDirection - Direction vector.
   PTEXTUREPOINT  ptp - Filled with texture
   BOOL              fIncludeCutouts - If TRUE, then intersect with the surface, but only
                     if the area is not already cutout
*/
BOOL CDoubleSurface::IntersectLine (int iSide, PCPoint pStart, PCPoint pDirection, PTEXTUREPOINT ptp,
                                    BOOL fIncludeCutouts, BOOL fPositiveAlpha)
{
   PCSplineSurface pss = Side(iSide);
   if (!pss)
      return FALSE;

   // convert to spline space
   CPoint pS, pD;
   pS.Copy (pStart);
   pD.Add (pDirection, pStart);
   pS.MultiplyLeft (&m_MatrixFromObject);
   pD.MultiplyLeft (&m_MatrixFromObject);
   pD.Subtract (&pS);

   return pss->IntersectLine (&pS, &pD, ptp, fIncludeCutouts, fPositiveAlpha);
}



/**********************************************************************************
CDoubleSurface::ContHVQuery
Called to see the HV location of what clicked on, or in other words, given a line
from pEye to pClick (object sapce), where does it intersect surface dwSurface (LOWORD(PIMAGEPIXEL.dwID))?
Fills pHV with the point. Returns FALSE if doesn't intersect, or invalid surface, or
can't tell with that surface. NOTE: if pOld is not NULL, then assume a drag from
pOld (converted to world object space) to pClick. Finds best intersection of the
surface then. Useful for dragging embedded objects around surface
*/
BOOL CDoubleSurface::ContHVQuery (PCPoint pEye, PCPoint pClick, DWORD dwSurface, PTEXTUREPOINT pOld, PTEXTUREPOINT pHV)
{
   PCLAIMSURFACE pcs = ClaimFindByID (dwSurface);
   if (!pcs)
      return FALSE;
   int iSide;
   iSide = ((pcs->dwReason == 0) || (pcs->dwReason == 3)) ? 1 : -1;

   if (pOld) {
      // drag, so use one API
      return HVDrag (iSide, pOld, pClick, pEye, pHV);
   }
   else {
      CPoint pDirection;
      pDirection.Subtract (pClick, pEye);

      // click, so use slightly different
      return IntersectLine (iSide, pEye, &pDirection, pHV, FALSE, TRUE);
   }

   return FALSE;
}




/**********************************************************************************
CDoubleSurface::ContCutout
Called by an embeded object to specify an arch cutout within the surface so the
object can go through the surface (like a window). The container should check
that pgEmbed is a valid object. dwNum is the number of points in the arch,
paFront and paBack are the container-object coordinates. (See CSplineSurface::ArchToCutout)
If dwNum is 0 then arch is simply removed. If fBothSides is true then both sides
of the surface are cleared away, else only the side where the object is embedded
is affected.

*/
BOOL CDoubleSurface::ContCutout (const GUID *pgEmbed, DWORD dwNum, PCPoint paFront, PCPoint paBack, BOOL fBothSides)
{
   // get the surface it's on
   TEXTUREPOINT tp;
   fp fRotation;
   DWORD dwSurface;
   dwSurface = 0;
   m_pTemplate->ContEmbeddedLocationGet (pgEmbed, &tp, &fRotation, &dwSurface);

   PCLAIMSURFACE pcs = ClaimFindByID (dwSurface);
   if (!pcs)
      return FALSE;
   int iSide;
   iSide = ((pcs->dwReason == 0) || (pcs->dwReason == 3)) ? 1 : -1;

   // make up a string for the object GUID
   WCHAR szTemp[64];
   StringFromGUID2 (*pgEmbed, szTemp, sizeof(szTemp)/2);

   // transform the points into this space
   CMem memFront, memBack;
   if (!memFront.Required(dwNum * sizeof(CPoint)) || !memBack.Required(dwNum * sizeof(CPoint)))
      return FALSE;
   memcpy (memFront.p, paFront, dwNum * sizeof(CPoint));
   memcpy (memBack.p, paBack, dwNum * sizeof(CPoint));
   paFront = (PCPoint) memFront.p;
   paBack = (PCPoint) memBack.p;
   DWORD i;
   for (i = 0; i < dwNum; i++) {
      paFront[i].MultiplyLeft (&m_MatrixFromObject);
      paBack[i].MultiplyLeft (&m_MatrixFromObject);
   }

   if (m_pTemplate->m_pWorld)
      m_pTemplate->m_pWorld->ObjectAboutToChange(m_pTemplate);


   // set framing dirty
   m_fFramingDirty = TRUE;

   // if dwNum < 3 then just clear out the cutouts
   if (dwNum < 3) {
      m_SplineA.CutoutRemove (szTemp);
      m_SplineB.CutoutRemove (szTemp);

      if (m_pTemplate->m_pWorld)
         m_pTemplate->m_pWorld->ObjectChanged(m_pTemplate);
      return TRUE;
   }

   // else create
   if (fBothSides || (iSide == 1))
      m_SplineA.ArchToCutout (dwNum, paFront, paBack, szTemp, FALSE);
   if (fBothSides || (iSide == -1))
      m_SplineB.ArchToCutout (dwNum, paFront, paBack, szTemp, FALSE);

   if (m_pTemplate->m_pWorld)
      m_pTemplate->m_pWorld->ObjectChanged(m_pTemplate);

   return TRUE;
}


/**********************************************************************************
CDoubleSurface::ContCutoutToZipper -
Assumeing that ContCutout() has already been called for the pgEmbed, this askes the surfaces
for the zippering information (CRenderSufrace::ShapeZipper()) that would perfectly seal up
the cutout. plistXXXHVXYZ must be initialized to sizeof(HVXYZ). plisstFrontHVXYZ is filled with
the points on the side where the object was embedded, and plistBackHVXYZ are the opposite side.
NOTE: These are in the container's object space, NOT the embedded object's object space.
*/
BOOL CDoubleSurface::ContCutoutToZipper (const GUID *pgEmbed, PCListFixed plistFrontHVXYZ, PCListFixed plistBackHVXYZ)
{
   // find out if this is valid and which side it's located on
   TEXTUREPOINT tp;
   fp fRotation;
   DWORD dwSurface;
   if (!m_pTemplate->ContEmbeddedLocationGet(pgEmbed, &tp, &fRotation, &dwSurface))
      return FALSE;

   PCLAIMSURFACE pcs = ClaimFindByID (dwSurface);
   if (!pcs)
      return FALSE;
   int iSide;
   iSide = ((pcs->dwReason == 0) || (pcs->dwReason == 3)) ? 1 : -1;

   PCSplineSurface pa, pb;
   pa = (iSide == 1) ? &m_SplineA : &m_SplineB;
   pb = (iSide == 1) ? &m_SplineB : &m_SplineA;

   WCHAR szTemp[64];
   StringFromGUID2 (*pgEmbed, szTemp, sizeof(szTemp)/2);

   // find the two cutouts in A and B for the edges.
   // know its there.
   PTEXTUREPOINT pList1, pList2;
   DWORD dwNum1, dwNum2;
   BOOL fClockwise;
   PWSTR pszName;
   DWORD i;
   pList1 = pList2 = NULL;
   for (i = 0; i < pa->CutoutNum(); i++) {
      pa->CutoutEnum (i, &pszName, &pList1, &dwNum1, &fClockwise);
      if (!_wcsicmp(pszName, szTemp))
         break;
   }
   for (i = 0; i < pb->CutoutNum(); i++) {
      pb->CutoutEnum (i, &pszName, &pList2, &dwNum2, &fClockwise);
      if (!_wcsicmp(pszName, szTemp))
         break;
   }
   if (!pList1 || !pList2)
      return FALSE;

   // convert these into two lists of texture points
   plistFrontHVXYZ->Init (sizeof(HVXYZ));
   plistBackHVXYZ->Init (sizeof(HVXYZ));
   pa->IntersectLoopWithGrid (pList1, dwNum1, TRUE, plistFrontHVXYZ, TRUE);
      // want to inverse this so it goes around clickwise when looking at A
   pb->IntersectLoopWithGrid (pList2, dwNum2, TRUE, plistBackHVXYZ, FALSE);
      // dont inverse this since B is already an inverted image

   // convert these to object space
   PHVXYZ phv;
   for (i = 0; i < plistFrontHVXYZ->Num(); i++) {
      phv = (PHVXYZ) plistFrontHVXYZ->Get(i);
      phv->p.MultiplyLeft (&m_MatrixToObject);
   }
   for (i = 0; i < plistBackHVXYZ->Num(); i++) {
      phv = (PHVXYZ) plistBackHVXYZ->Get(i);
      phv->p.MultiplyLeft (&m_MatrixToObject);
   }
   return TRUE;
}



/**********************************************************************************
CDoubleSurface::ContThickness - 
returns the thickness of the surface (dwSurface) at pHV. Used by embedded
objects like windows to know how deep they should be.

NOTE: usually overridden
*/
fp CDoubleSurface::ContThickness (DWORD dwSurface, PTEXTUREPOINT pHV)
{
   PCLAIMSURFACE pcs = ClaimFindByID (dwSurface);
   if (!pcs)
      return 0;

   return m_fThickStud + m_fThickA + m_fThickB;
}



/**********************************************************************************
CDoubleSurface::ContSideInfo -
returns a DWORD indicating what class of surface it is... right now
0 = internal, 1 = external. dwSurface is the surface. If fOtherSide
then want to know what's on the opposite side. Returns 0 if bad dwSurface.

NOTE: usually overridden
*/
DWORD CDoubleSurface::ContSideInfo (DWORD dwSurface, BOOL fOtherSide)
{
   PCLAIMSURFACE pcs = ClaimFindByID (dwSurface);
   if (!pcs)
      return 0;

   int iSide;
   iSide = ((pcs->dwReason == 0) || (pcs->dwReason == 3)) ? 1 : -1;

   if (iSide == 1)
      return fOtherSide ? m_dwExtSideB : m_dwExtSideA;
   else if (iSide == -1)
      return fOtherSide ? m_dwExtSideA : m_dwExtSideB;
   else
      return 0;
}


/**********************************************************************************
CDoubleSurface::ContMatrixFromHV - THE SUPERCLASS SHOULD OVERRIDE THIS. Given
a point on a surface (that supports embedding), this returns a matrix that translates
0,0,0 to the same in world space as where pHV is, and also applies fRotation around Y (clockwise)
so that X and Z are (for the most part) still on the surface.

inputs
   DWORD             dwSurface - Surface ID that can be embedded. Should check to make sure is valid
   PTEXTUREPOINT     pHV - HV, 0..1 x 0..1 locaiton within the surface
   fp            fRotation - Rotation in radians, clockwise.
   PCMatrix          pm - Filled with the new rotation matrix
returns
   BOOL - TRUE if success
*/
BOOL CDoubleSurface::ContMatrixFromHV (DWORD dwSurface, PTEXTUREPOINT pHV, fp fRotation, PCMatrix pm)
{
   PCLAIMSURFACE pcs = ClaimFindByID (dwSurface);
   if (!pcs)
      return 0;

   int iSide;
   iSide = ((pcs->dwReason == 0) || (pcs->dwReason == 3)) ? 1 : -1;

   PCSplineSurface pss;
   pss = Side(iSide);

   // given HV, get normals and stuff
   CPoint pC, pH, pV, pN;
   pss->HVToInfo (pHV->h, pHV->v, &pC);
   if (pHV->h + .01 < 1.0) {
      pss->HVToInfo (pHV->h + .01, pHV->v, &pH);
      pH.Subtract (&pC);
   }
   else {
      pss->HVToInfo (pHV->h - .01, pHV->v, &pH);
      pH.Subtract (&pC);
      pH.Scale (-1);
   }
   pH.Normalize();
   if (pHV->v > 0.01) {
      pss->HVToInfo (pHV->h, pHV->v - .01, &pV);
      pV.Subtract (&pC);
   }
   else {
      pss->HVToInfo (pHV->h, pHV->v + .01, &pV);
      pV.Subtract (&pC);
      pV.Scale (-1);
   }
   
   // calc normal, and then recalt pV
   pN.CrossProd (&pV, &pH);
   pN.Normalize();
   pV.CrossProd (&pH, &pN);
   pV.Normalize();

   // create a rotation matrix such that H rotates to X, V rotates to Z, N rotates to Y
   CMatrix mr;
   mr.RotationFromVectors (&pH, &pN, &pV);

   // create a rotation matrix that translates 0,0,0 to pC
   CMatrix mt;
   mt.Translation (pC.p[0], pC.p[1], pC.p[2]);

   // create a rotation matrix that rotates around Y for fRotation
   CMatrix mr2;
   mr2.RotationY (fRotation);

   // combine these all together
   pm->Multiply (&mr, &mr2);
   pm->MultiplyRight (&mt);

   // then rotate by the object's rotation matrix to get to world space
   pm->MultiplyRight (&m_MatrixToObject);

   return TRUE;
}


/**********************************************************************************
CDoubleSurface::Message -
sends a message to the object. The interpretation of the message depends upon
dwMessage, which is OSM_XXX. If the function understands and handles the
message it returns TRUE, otherwise FALE.
*/
BOOL CDoubleSurface::Message (DWORD dwMessage, PVOID pParam)
{
   switch (dwMessage) {
   case OSM_WALLQUERYEDGES:
      {
         // fill in information about corners
         POSMWALLQUERYEDGES p = (POSMWALLQUERYEDGES) pParam;
         
         // make matrix to convert from spline space to world space
         if (!m_pTemplate)
            return FALSE;
         CMatrix m;
         m.Multiply (&m_pTemplate->m_MatrixObject, &m_MatrixToObject);

         // loop
         DWORD i;
         DWORD dwWidth, dwHeight, dwCur;
         dwWidth = ControlNumGet(TRUE);
         dwHeight = ControlNumGet(FALSE);
         for (i = 0; i < 2; i++) {
            dwCur = i ? (dwWidth - 1) : 0;
            
            m_SplineCenter.ControlPointGet (dwCur, dwHeight-1, &p->apCorner[i]);
            m_SplineA.ControlPointGet (dwCur, dwHeight-1, &p->apACorner[i]);
            m_SplineB.ControlPointGet (dwWidth - dwCur - 1, dwHeight-1, &p->apBCorner[i]); // side B reversed
            m_SplineA.HVToInfo (i ? .99 : .1, 1, &p->apAVector[i]);
            m_SplineB.HVToInfo (1.0 - (i ? .99 : .1), 1, &p->apBVector[i]);   // side b reversed

            // convert to world space
            p->apCorner[i].MultiplyLeft (&m);
            p->apACorner[i].MultiplyLeft (&m);
            p->apBCorner[i].MultiplyLeft (&m);
            p->apAVector[i].MultiplyLeft (&m);
            p->apBVector[i].MultiplyLeft (&m);

            p->apAVector[i].Subtract (&p->apACorner[i]);
            p->apAVector[i].Scale(-1);
            p->apBVector[i].Subtract (&p->apBCorner[i]);
            p->apBVector[i].Scale(-1);
         }
      }
      return TRUE;


   case OSM_SETFLOORS:
      {
         POSMSETFLOORS p = (POSMSETFLOORS) pParam;
         PCLAIMSURFACE pcs = ClaimFindByID (p->dwSurface);
         if (!pcs)
            return FALSE;

         PCListFixed pl;
         if ((pcs->dwReason == 0) || (pcs->dwReason == 3))
            pl = &m_listFloorsA;
         else if ((pcs->dwReason == 1) || (pcs->dwReason == 4))
            pl = &m_listFloorsB;
         else
            return FALSE;

         // if these are the same then ignore
         if ((pl->Num() == p->dwNum) && !memcmp (pl->Get(0), p->pafV, p->dwNum * sizeof(fp)))
            return TRUE;


         if (m_pTemplate->m_pWorld)
            m_pTemplate->m_pWorld->ObjectAboutToChange (m_pTemplate);
         pl->Init (sizeof(fp), p->pafV, p->dwNum);
         if (m_pTemplate->m_pWorld)
            m_pTemplate->m_pWorld->ObjectChanged (m_pTemplate);
         return TRUE;
      }
      return FALSE;

   case OSM_FINDCLOSESTFLOOR:
      {
         POSMFINDCLOSESTFLOOR p = (POSMFINDCLOSESTFLOOR) pParam;
         PCLAIMSURFACE pcs = ClaimFindByID (p->dwSurface);
         if (!pcs)
            return FALSE;

         PCListFixed pl;
         if ((pcs->dwReason == 0) || (pcs->dwReason == 3))
            pl = &m_listFloorsA;
         else if ((pcs->dwReason == 1) || (pcs->dwReason == 4))
            pl = &m_listFloorsB;
         else
            return FALSE;
         if (!pl->Num())
            return FALSE;  // no closest


         DWORD i, dwClosest;
         fp fVal, fClosest, fDist;
         for (i = 0; i < pl->Num(); i++) {
            fVal = *((fp*) pl->Get(i));
            fDist = fabs(fVal - p->pOrig.v);
            if (!i) {
               fClosest = fDist;
               dwClosest = 0;
               continue;
            }

            // dont take any that are higher than the desired setting
            // NOTE: Because of above check, will always have a closest,
            // even if door requested below.
            if (fVal < p->pOrig.v)
               continue;
            if (fDist < fClosest) {
               fClosest = fDist;
               dwClosest = i;
            }
         }

         p->pClosest.h = p->pOrig.h;
         p->pClosest.v = *((fp*) pl->Get(dwClosest));
         return TRUE;
      }
      return FALSE;

   case OSM_SPLINESURFACEGET:
      {
         POSMSPLINESURFACEGET p = (POSMSPLINESURFACEGET) pParam;
         PCSplineSurface pss;
         CMatrix m;

         switch (p->dwType) {
         case 0:  // any
            pss = &m_SplineA;
            p->pListPSS->Add (&pss);
            m_pTemplate->ObjectMatrixGet (&m);
            m.MultiplyLeft (&m_MatrixToObject);
            p->pListMatrix->Add (&m);

            pss = &m_SplineB;
            p->pListPSS->Add (&pss);
            p->pListMatrix->Add (&m);
            break;
         case 1:  // ceiling
         case 11: // ceiling, but return center
            if ((HIWORD(m_dwType) == 2) || HIWORD(m_dwType) == 3) {
               // BUGFIX - Was HIWORD(m_dwType==3) - which was typo
               pss = (p->dwType == 11) ? &m_SplineCenter : &m_SplineB;
               p->pListPSS->Add (&pss);

               m_pTemplate->ObjectMatrixGet (&m);
               m.MultiplyLeft (&m_MatrixToObject);
               p->pListMatrix->Add (&m);
            }
            break;
         case 2:  // wall
            if (HIWORD(m_dwType) == 1) {
               pss = &m_SplineCenter;
               p->pListPSS->Add (&pss);

               m_pTemplate->ObjectMatrixGet (&m);
               m.MultiplyLeft (&m_MatrixToObject);
               p->pListMatrix->Add (&m);
            }
            break;
         case 3:  // floor
         case 13: // floor, but use the center
            if (HIWORD(m_dwType) == 3) {
               pss = (p->dwType == 13) ? &m_SplineCenter : &m_SplineA;
               p->pListPSS->Add (&pss);

               m_pTemplate->ObjectMatrixGet (&m);
               m.MultiplyLeft (&m_MatrixToObject);
               p->pListMatrix->Add (&m);
            }
            break;

         case 4:  // roof spine
         case 14: // bottom roof spline
            if (HIWORD(m_dwType) == 2) {
               pss = (p->dwType == 14) ? &m_SplineB : &m_SplineCenter;
               p->pListPSS->Add (&pss);

               m_pTemplate->ObjectMatrixGet (&m);
               m.MultiplyLeft (&m_MatrixToObject);
               p->pListMatrix->Add (&m);
            }
            break;
         case 5:  // any spline cetner
            pss = &m_SplineCenter;
            p->pListPSS->Add (&pss);

            m_pTemplate->ObjectMatrixGet (&m);
            m.MultiplyLeft (&m_MatrixToObject);
            p->pListMatrix->Add (&m);
            break;
         case 6:  // any spline center so long as roof or wall
            if ((HIWORD(m_dwType) == 1) || (HIWORD(m_dwType) == 2)) {
               pss = &m_SplineCenter;
               p->pListPSS->Add (&pss);

               m_pTemplate->ObjectMatrixGet (&m);
               m.MultiplyLeft (&m_MatrixToObject);
               p->pListMatrix->Add (&m);
               break;
            }
         }
      }
      break;


   case OSM_CLICKONOVERLAY:
      {
         POSMCLICKONOVERLAY p = (POSMCLICKONOVERLAY) pParam;
         PCLAIMSURFACE pcs = ClaimFindByID (p->dwID);
         if (!pcs)
            return FALSE;

         // if the reason is 3 or 4 then it's an overlay
         return (pcs->dwReason == 3) || (pcs->dwReason == 4);
      }
      break;

   case OSM_NEWOVERLAYFROMINTER:
      {
         POSMNEWOVERLAYFROMINTER p = (POSMNEWOVERLAYFROMINTER) pParam;
         PCLAIMSURFACE pcs = ClaimFindByID (p->dwIDClick);
         if (!pcs)
            return FALSE;

         // if the reason is 3 or 4 then it's an overlay
         if ((pcs->dwReason != 0) && (pcs->dwReason != 1))
            return FALSE;  // clicked on an existing overlay, dont do this

         CListFixed lOver;
         lOver.Init (sizeof(TEXTUREPOINT));
         if (!OverlayForPaint (pcs->dwReason == 0, &p->tpClick, &lOver))
            return FALSE;

         // add it
         DWORD dwNew;
         // BUGFIX - Use L"Any shape" so can hand-edit later
         if (!AddOverlay (pcs->dwReason == 0, L"Any shape", (PTEXTUREPOINT)lOver.Get(0),
            lOver.Num(), TRUE, &dwNew, FALSE))
            return FALSE;

         p->dwIDNew = dwNew;
         return TRUE;
      }
      break;
   }

   return FALSE;
}



/*************************************************************************************
CDoubleSurface::CutoutFromSpline - Given spline infomration that's going clockwise and which
is in 0..1, 0..1 space, this cuts it out.

inputs
   PWSTR       pszName - name
   DWORD       dwNum - Number of points
   PCPoint     paPoints - List of points. p[0] = 0..1, p[1] = 0..1, p[2] = 0
   DWORD       *padwSegCurve - Type of curve.
   DWORD       dwMinDivide, dwMaxDivide - Divide
   fp      fDetail - Detail
   BOOL        fClockwise - Set to TRUE if want cutout to be clockwise (meaning it cuts
                  out the inside), or FALSE for counterclockwise (cuts out outside)
returns
   BOOL - true if succeded
*/
BOOL CDoubleSurface::CutoutFromSpline (PWSTR pszName, DWORD dwNum, PCPoint paPoints, DWORD *padwSegCurve,
                                       DWORD dwMinDivide, DWORD dwMaxDivide, fp fDetail,
                                       BOOL fClockwise)
{
   // make a new spline
   CSpline spline;
   if (!spline.Init (TRUE, dwNum, paPoints, NULL, padwSegCurve, dwMinDivide, dwMaxDivide, fDetail))
      return FALSE;

   // get the points
   CListFixed lTP;
   TEXTUREPOINT tp, *ptp;
   lTP.Init (sizeof(TEXTUREPOINT));
   DWORD i, dwNew;
   dwNew = spline.QueryNodes ();
   for (i = 0; i < dwNew; i++) {
      PCPoint pp = spline.LocationGet (i);
      tp.h = pp->p[0];
      tp.v = pp->p[1];
      lTP.Add (&tp);
   }
   ptp = (PTEXTUREPOINT) lTP.Get(0);


   // set framing dirty
   m_fFramingDirty = TRUE;

   // if it's the entire surface then just remove this cutout and be done with it
   if (!fClockwise && IsCutoutEntireSurface (lTP.Num(), ptp)) {
      m_SplineA.CutoutRemove (pszName);
      m_SplineB.CutoutRemove (pszName);
      return TRUE;
   }

   // reverse the direction?
   if (!fClockwise) {
      for (i = 0; i < lTP.Num()/2; i++) {
         tp = ptp[i];
         ptp[i] = ptp[lTP.Num() - i - 1];
         ptp[lTP.Num() - i - 1] = tp;
      }
   }

   // write it out
   // set it to side A
   m_SplineA.CutoutSet (pszName, (PTEXTUREPOINT) lTP.Get(0),
      lTP.Num(), fClockwise);

   // for side B do 1-h and go reverse direction
   ptp = (PTEXTUREPOINT) lTP.Get(0);
   for (i = 0; i < lTP.Num(); i++) {
      ptp[i].h = 1.0 - ptp[i].h;
   }
   for (i = 0; i < lTP.Num()/2; i++) {
      tp = ptp[i];
      ptp[i] = ptp[lTP.Num() - i - 1];
      ptp[lTP.Num() - i - 1] = tp;
   }

   // set it to side B
   m_SplineB.CutoutSet (pszName, (PTEXTUREPOINT) lTP.Get(0),
      lTP.Num(), fClockwise);

   return TRUE;
}


/*************************************************************************************
CDoubleSurface::IntersectWithOtherSurfaces - Searches through the world (or just
this object) and intersects it with either bits from its own object, or the rest of the
world.

inputs
   DWORD    dwFlags - 0x0001 means intersect with the world, 0x0002 intersect with
                     this object. Can be both.
   DWORD    dwMode - If:
               0 - Assume this is a wall, and intersect against roofs to shorted
               1 - Assume this is a roof, and intersect against other roofs
               2 - Assume this is a floor, instersect against walls and roofs.
               3 - Assume this is a floor, intersect against roofs only
               4 - Assume this is a wall, intersect to shorten but use dwFI=1
   PWSTR    pszCutout - Name of the cutout. If this is NULL uses an edge.
   BOOL     fIgnoreCutouts - Set to TRUE if should ignore cutouts,  FALSE if use
returns
   BOOL - TRUE if succes
*/
BOOL CDoubleSurface::IntersectWithOtherSurfaces (DWORD dwFlags, DWORD dwMode, PWSTR pszCutout, BOOL fIgnoreCutouts)
{

   // set framing dirty
   m_fFramingDirty = TRUE;

   // delete the old one
   if (pszCutout) {
      m_SplineA.CutoutRemove (pszCutout);
      m_SplineB.CutoutRemove (pszCutout);
   }

   // find out what line segments intersect
   DWORD i;
   CMem mem;
   PCListFixed plLines, plLines2;
   DWORD dwFI, dwSurf;
   switch (dwMode) {
   default:
   case 0:  // its a wall, so intersect against roofs
      // BUGFIX - Was 1, but had problem where would paint wall in a room
      // on ground floor, and this wouldnt work because didnt form air-tight seal with
      // wall and floor above.
      //dwFI = 11;
      // BUGFIX - Change back to 1 (was 11) because if don't do side A or side B then won't
      // have the cutouts. This causes problems for walls on the top floor of a 4-gabled
      // roof becuse some of the roofs have large cutouts going through them. It seems
      // to still work because I extend the intersection lines on either end by  .05
      dwFI = 1;
      dwSurf = 0;
      break;
   case 4:  // its a wall, so intersect against roofs
      dwFI = 14;   // BUGFIX - wall, intersect against roof only
      dwSurf = 0;
      break;
   case 1:
      dwFI = 4;
      dwSurf = 0;
      break;
   case 2:
      dwSurf = 2;
      dwFI = 6;
      break;
   case 3:
      //dwSurf = 0; // BUGFIX - Was 2 for some unknown reason;
      dwSurf = -1;
         // BUGFIX - Using -1 so that intersects using the bottom of the floor
         // against the bottom of the roof. This ensures the floor/ceiling seems to
         // go all the way into the roof. It may coause problems though of the floor/ceiling
         // exceting beyond the roof if it's a very thick floor or shallow roof...
      dwFI = 14;
         // BUGFIX - Was 4, but had problems when intersecting top floor/ceiling
         // with 3 or 4 gabled roof. Since was using center of roof before it was
         // not including cutouts on side A and B, non-visible walls ended up clipping
      break;
   }
   plLines = FindIntersections (0.0, dwSurf, dwFI, dwFI, dwFlags, fIgnoreCutouts);
   // NOTE: Keeping extend amount 0.0 since already seems to be working with no extension,
   // and many called for building block creation anyhow. Dont fix the bug if it's
   // not broken

   if (!plLines)
      goto resetwall;

   if ((dwMode == 3) || (dwMode == 4)) {
      // BUGFIX - Because changed the wall to intersect with the center of teh ceiling
      // structure insread of the bottom, need to reverse the lines since they are going
      // in the countclickwise direction
      for (i = 0; i < plLines->Num(); i++) {
         PTEXTUREPOINT pt = (PTEXTUREPOINT) plLines->Get(i);
         TEXTUREPOINT t;
         t = pt[0];
         pt[0] = pt[1];
         pt[1] = t;
      }
   }

   if ((dwMode == 1) || (dwMode == 2) || (dwMode == 3)){
      // roof
      BOOL fClockwise;
      fClockwise = TRUE;   // so that cuts out on the inside
      plLines2 = LineSequencesForRoof (plLines, TRUE);
      delete plLines;
      if (!plLines2)
         goto resetwall;

      CListVariable lv;
      DWORD       dwTemp[100];
      PCListFixed plScores;
      plScores = SequencesSortByScores (plLines2, !fClockwise);
      if (!plScores)
         goto resetwall;
      PathFromSequences (plLines2, &lv, dwTemp, 0, sizeof(dwTemp)/sizeof(DWORD));

      DWORD dwBest;
      dwBest = PathWithLowestScore (plLines2, &lv, plScores);
      delete plScores;
#if 0// def _DEBUG
      char szTemp[64];
      sprintf (szTemp, "Lowest = %d\r\n", dwBest);
      OutputDebugString (szTemp);
#endif
      if (dwBest != (DWORD) -1) {
         PCListFixed pEdge;
         pEdge = PathToSplineList (plLines2, &lv, dwBest, !fClockwise);

         if (pEdge) {
            // adjust the clipping spline
            DWORD dwMinDivide, dwMaxDivide;
            fp fDetail;
            EdgeDivideGet (&dwMinDivide, &dwMaxDivide, &fDetail);
            if (pszCutout)
               CutoutFromSpline (pszCutout, pEdge->Num(), (PCPoint) pEdge->Get(0), (DWORD*) SEGCURVE_LINEAR,
                  dwMinDivide, dwMaxDivide, fDetail, FALSE);
            else
               EdgeInit (TRUE, pEdge->Num(), (PCPoint) pEdge->Get(0), NULL, (DWORD*) SEGCURVE_LINEAR, dwMinDivide, dwMaxDivide, fDetail);
            //RecalcEdges ();

            delete pEdge;
         }
      }



      // Delete plLines2
      for (i = 0; i < plLines2->Num(); i++) {
         PCListFixed pl = *((PCListFixed*) plLines2->Get(i));
         delete pl;
      }
      delete plLines2;

      return TRUE;
   }
   else if ((dwMode == 0) || (dwMode == 4)) {
      // wall
      plLines2 = LineSegmentsForWall (plLines);
      delete plLines;
      if (!plLines2)
         goto resetwall;
      plLines = LineSegmentsFromWallToPoints (plLines2);
      delete plLines2;
      if (!plLines)
         goto resetwall;
   }

   // convert these to CPoints
   DWORD dwNum;
   dwNum = plLines->Num();
   if (!mem.Required (dwNum * sizeof(CPoint)))
      goto resetwall;
   PCPoint pap;
   pap = (PCPoint) mem.p;
   PTEXTUREPOINT ptp;
   ptp = (PTEXTUREPOINT) plLines->Get(0);
   for (i = 0; i < dwNum; i++) {
      pap[i].Zero();
      pap[i].p[0] = ptp[i].h;
      pap[i].p[1] = ptp[i].v;
   }
   delete plLines;

   // adjust the clipping spline
   DWORD dwMinDivide, dwMaxDivide;
   fp fDetail;
   EdgeDivideGet (&dwMinDivide, &dwMaxDivide, &fDetail);
   if (pszCutout)
      CutoutFromSpline (pszCutout, dwNum, pap, (DWORD*) SEGCURVE_LINEAR,
         dwMinDivide, dwMaxDivide, fDetail, FALSE);
   else
      EdgeInit (TRUE, dwNum, pap, NULL, (DWORD*) SEGCURVE_LINEAR, dwMinDivide, dwMaxDivide, fDetail);
   //RecalcEdges ();


   return TRUE;

resetwall:
   CPoint apEdge[4];
   apEdge[0].Zero();
   apEdge[1].Zero();
   apEdge[1].p[0] = 1;
   apEdge[2].Zero();
   apEdge[2].p[0] = apEdge[2].p[1] = 1;
   apEdge[3].Zero();
   apEdge[3].p[1] = 1;
   EdgeDivideGet (&dwMinDivide, &dwMaxDivide, &fDetail);

   // don't need to do anything if pszCutout since already deleted
   if (!pszCutout)
      EdgeInit (TRUE, 4, apEdge, NULL, (DWORD*) SEGCURVE_LINEAR, dwMinDivide, dwMaxDivide, fDetail);
   //RecalcEdges ();
   return TRUE;
}


/*************************************************************************************
CDoubleSurface::FindIntersections - Find the intersections of this object
with other objects. Returns a PCListFixed from IntersectWithSurfaces.

inputs
   fp       fExtend - Amount to extend intersections by in HV space. Just to make sure they
               overlap
   int      iThis - Which surface of this one to use.
                     0 for m_SplineCenter
                     1 for m_SplineA
                     -1 for m_SplineB
                     2 for m_SplineA extended a bit
                     -2 for m_SplineB extended a bit
   DWORD    dwWithWorld - Passed to OSMSPLINESURFACEGET, for world objects
   DWORD    dwWithSelf - Passed onto OSM_SPLINESURFACEGET, for self
   DWORD    dwFlags - 0x0001 means intersect with the world, 0x0002 intersect with
                     this object. Can be both.
   BOOL     fIgnoreCutouts - If TRUE, then ignore the cutouts in the stuff that
            intersecting with.
returns
   PCListFixed - From IntersectWithSurfaces(), must be freed. NULL if can't find.
*/
PCListFixed CDoubleSurface::FindIntersections (fp fExtend, int iThis, DWORD dwWithWorld, DWORD dwWithSelf, DWORD dwFlags,
                                               BOOL fIgnoreCutouts)
{
   PCSplineSurface pss;
   PCListFixed pRet = NULL;
   BOOL fDeletePSS = FALSE;

   // see what can find
   CListFixed lPSS, lTemp, lMatrix;
   lPSS.Init (sizeof(PCSplineSurface));
   lTemp.Init (sizeof(PCSplineSurface));
   lMatrix.Init (sizeof(CMatrix));
   if (dwFlags & 0x0001)
      FindIntersectionsWithWorld (dwWithWorld, &lPSS, &lMatrix);
   if (dwFlags & 0x0002)
      FindIntersectionsWithSelf (dwWithSelf, &lPSS, &lMatrix);
   if (!lPSS.Num())
      return NULL;   // nothing

   // get the right surface
   switch (iThis) {
   case 0:
      pss = &m_SplineCenter;
      break;
   case 1:
      pss = &m_SplineA;
      break;
   case -1:
      pss = &m_SplineB;
      break;
   case 2:
      pss = SurfaceOvershootForIntersect (TRUE);
      fDeletePSS = TRUE;
      break;
   case -2:
      pss = SurfaceOvershootForIntersect (FALSE);
      fDeletePSS = TRUE;
      break;
   default:
      return NULL;
   }
   if (!pss)
      return NULL;

   // look through the list and remove any that point back on itself
   DWORD i;
   for (i = lPSS.Num()-1; i < lPSS.Num(); i--) {
      PCSplineSurface pTemp = *((PCSplineSurface*) lPSS.Get(i));
      if (pTemp == pss) {
         lPSS.Remove(i);
         lMatrix.Remove(i);
      }
   }

   // intersect these with the central spline
   pRet = pss->IntersectWithSurfaces (fExtend, lPSS.Num(), (PCSplineSurface*) lPSS.Get(0), (PCMatrix)lMatrix.Get(0),
      fIgnoreCutouts);


   // done
   if (fDeletePSS && pss)
      delete pss;
   return pRet;
}

/*************************************************************************************
CDoubleSurface::FindIntersectionsWithWorld - Returns a PCListFixed of PCSplineSurfaces which
are the spline surfaces of other objects that intersect with this surface.

inputs
   DWORD          dwWith - Passed to OSMSPLINESURFACEGET
   PCListFixed    plPSS - Must be be initialized as PCSplineSurface. Contains list of
                  splines that intersected with. Appended to, so anything already
                  there will be left.
   PCListFixed    plMatrix - Must be initialized as CMatrix. Contains list of matrixes
                  that convert from the spline's space into this CDoubleSurface space.
                  Appended to, so anything already there will be left.
returns
   BOOL - TRUE if found some, FALSE if not
*/
BOOL CDoubleSurface::FindIntersectionsWithWorld (DWORD dwWith, PCListFixed plPSS, PCListFixed plMatrix)
{
   if (!m_pTemplate->m_pWorld)
      return FALSE;


   CMem mem;

   // get the bounding box for this. I know that because of tests in render
   // it will return even already clipped stuff
   DWORD dwThis;
   dwThis = m_pTemplate->m_pWorld->ObjectFind (&m_pTemplate->m_gGUID);
   CPoint pCorner1, pCorner2;
   CMatrix m, mInv;
   m_pTemplate->m_pWorld->BoundingBoxGet (dwThis, &m, &pCorner1, &pCorner2);
   // moved later because mucking up... m.MultiplyLeft (&m_MatrixToObject);
   // moved later because mucking up... m.Invert4 (&mInv);

   // extend the bounding box a bit
   CPoint pMin, pMax, pDelta;
   pMin.Copy (&pCorner1);
   pMin.Min (&pCorner2);
   pMax.Copy (&pCorner1);
   pMax.Max (&pCorner2);
   pDelta.Zero();
   pDelta.p[0] = pDelta.p[1] = pDelta.p[2] = .1;
   pCorner1.Subtract (&pMin, &pDelta);
   pCorner2.Add (&pMax, &pDelta);

   // find out what objects this intersects with
   CListFixed     lObjects;
   lObjects.Init(sizeof(DWORD));
   m_pTemplate->m_pWorld->IntersectBoundingBox (&m, &pCorner1, &pCorner2, &lObjects);

   // BUGFIX - Moved from earlier because cases where not finding all the walls touching
   // the floor because they're outside the bounding box
   m.MultiplyLeft (&m_MatrixToObject);
   m.Invert4 (&mInv);

   // go through those objects and find out what surfaces are ceilings
   CListFixed lTemp, lTemp2;
   lTemp.Init (sizeof(PCSplineSurface));
   lTemp2.Init (sizeof(CMatrix));
   PCMatrix pMat;
   DWORD i, j;
   PCObjectSocket pos;
   OSMSPLINESURFACEGET ssg;
   memset (&ssg, 0, sizeof(ssg));
   ssg.dwType = dwWith;
   ssg.pListPSS = &lTemp;
   ssg.pListMatrix = &lTemp2;
   BOOL fFound;
   fFound = FALSE;
   for (i = 0; i < lObjects.Num(); i++) {
      DWORD dwOn = *((DWORD*)lObjects.Get(i));
      if (dwOn == dwThis)
         continue;
      pos = m_pTemplate->m_pWorld->ObjectGet (dwOn);
      if (!pos)
         continue;
      lTemp.Clear();
      lTemp2.Clear();
      pos->Message (OSM_SPLINESURFACEGET, &ssg);


      // add all these to the list
      plPSS->Required (plPSS->Num() + lTemp.Num());
      plMatrix->Required (plMatrix->Num() + lTemp.Num());
      for (j = 0; j < lTemp.Num(); j++) {
         PCSplineSurface *pp = (PCSplineSurface*) lTemp.Get(j);
         plPSS->Add (pp);

         pMat = (PCMatrix) lTemp2.Get(j);
         pMat->MultiplyRight (&mInv);
         plMatrix->Add (pMat);
         fFound = TRUE;
      }
   }

   return fFound;
}

/*************************************************************************************
CDoubleSurface::FindIntersectionsWithSelf - Returns a PCListFixed of PCSplineSurfaces which
are the spline surfaces of other surfaces within this one that intersect with this surface.

inputs
   DWORD          dwWith - Passed to OSMSPLINESURFACEGET
   PCListFixed    plPSS - Must be be initialized as PCSplineSurface. Contains list of
                  splines that intersected with. Appended to, so anything already
                  there will be left.
   PCListFixed    plMatrix - Must be initialized as CMatrix. Contains list of matrixes
                  that convert from the spline's space into this CDoubleSurface space.
                  Appended to, so anything already there will be left.
returns
   BOOL - TRUE if found some, FALSE if not
*/
BOOL CDoubleSurface::FindIntersectionsWithSelf (DWORD dwWith, PCListFixed plPSS, PCListFixed plMatrix)
{
   CMem mem;

   CMatrix m, mInv;
   m_pTemplate->ObjectMatrixGet (&m);
   m.MultiplyLeft (&m_MatrixToObject);
   m.Invert4 (&mInv);


   OSMSPLINESURFACEGET ssg;
   DWORD dwOldNum;
   dwOldNum = plMatrix->Num();
   memset (&ssg, 0, sizeof(ssg));
   ssg.dwType = dwWith;
   ssg.pListPSS = plPSS;
   ssg.pListMatrix = plMatrix;
   m_pTemplate->Message (OSM_SPLINESURFACEGET, &ssg);

   // add all these to the list
   DWORD j;
   BOOL fFound;
   fFound = FALSE;
   for (j = dwOldNum; j < plMatrix->Num(); j++) {
      PCMatrix pMat = (PCMatrix) plMatrix->Get(j);
      pMat->MultiplyRight (&mInv);
      fFound = TRUE;
   }

   return fFound;
}

/*************************************************************************************
CDoubleSurface::AddOverlay - Adds a new overlay to either side A or side B.
It does the following:
   1) Muck with the name to indicate the surface ID in the name
   2) Add the overlay
   3) Add the texture (make it red so it's noticable)
   4) Add the click-area
   5) Of course, notify world that object has changed

inputs
   BOOL              fSideA - TRUE if adding to side a, FALSE if side B
   PWSTR             pszName - Name of the overlay
   PTEXTUREPOINT     ptp - Overlay info
   DWORD             dwNum - Number of points in ptp
   BOOL              fClockwise - TRUE if it's clockwise
   DWORD             *pdwDisplayControl - Filled with a pointer to the main object's
                     m_dwDisplayControl. This will be set so that the control
                     points are displayed.
   BOOL              fSelect - If TRUE, select the object after add the overlay
returns
   BOOL - TRUE if success
*/
BOOL CDoubleSurface::AddOverlay (BOOL fSideA, PWSTR pszName, PTEXTUREPOINT ptp,
                                       DWORD dwNum, BOOL fClockwise, DWORD *pdwDisplayControl,
                                       BOOL fSelect)
{
   // tell the world that changing
   if (m_pTemplate->m_pWorld)
      m_pTemplate->m_pWorld->ObjectAboutToChange (m_pTemplate);

   DWORD i;
   i = ClaimSurface (fSideA ? 3 : 4, TRUE, RGB(0xff,0,0), MATERIAL_PAINTSEMIGLOSS);
   if (!i) {
      if (m_pTemplate->m_pWorld)
         m_pTemplate->m_pWorld->ObjectChanged (m_pTemplate);
      return FALSE;
   }

   PCSplineSurface pss;
   pss = fSideA ? &m_SplineA : &m_SplineB;


   // muck with the name
   WCHAR szTemp[128];
   swprintf (szTemp, L"%s:%d", pszName, (int)i);

   // add the overlay
   pss->OverlaySet (szTemp, ptp, dwNum, fClockwise);

   // set flag so this is the current one displayed
   if (pdwDisplayControl)
      (*pdwDisplayControl) = i;

   // tell the world we've changed
   if (m_pTemplate->m_pWorld) {
      m_pTemplate->m_pWorld->ObjectChanged (m_pTemplate);

      // BUGFIX - SO doesnt select when automatically add an overlay when painting
      if (fSelect) {
         // make sure they're visible
         m_pTemplate->m_pWorld->SelectionClear();
         GUID gObject;
         m_pTemplate->GUIDGet (&gObject);
         m_pTemplate->m_pWorld->SelectionAdd (m_pTemplate->m_pWorld->ObjectFind(&gObject));
      }
   }


   return TRUE;
}


/**********************************************************************************
CDoubleSurface::
Called to tell the container that one of its contained objects has been renamed.
*/
BOOL CDoubleSurface::ContEmbeddedRenamed (const GUID *pgOld, const GUID *pgNew)
{
   // go through cutouts and find match
   // make up a string for the object GUID
   WCHAR szTemp[64], szTemp2[64];
   StringFromGUID2 (*pgOld, szTemp, sizeof(szTemp)/2);
   StringFromGUID2 (*pgNew, szTemp2, sizeof(szTemp)/2);
   DWORD i, j;
   DWORD dwNum;
   PWSTR pszName;
   PTEXTUREPOINT ptp;
   BOOL fClockwise;
   PCSplineSurface pss;
   for (j = 0; j < 2; j++) {
      pss = j ? &m_SplineB : &m_SplineA;

      for (i = 0; i < pss->CutoutNum(); i++) {
         if (!pss->CutoutEnum (i, &pszName, &ptp, &dwNum, &fClockwise))
            continue;
         if (_wcsicmp (pszName, szTemp))
            continue;

         // else, found match
         CListFixed list;
         list.Init (sizeof(TEXTUREPOINT), ptp, dwNum);

         pss->CutoutRemove (szTemp);
         pss->CutoutSet (szTemp2, (PTEXTUREPOINT) list.Get(0), list.Num(), fClockwise);

      }
   }


   // call into the template
   return m_pTemplate->CObjectTemplate::ContEmbeddedRenamed (pgOld, pgNew);
}


/**********************************************************************************
CDoubleSurface::MatrixSet - Sets the translation and rotation matrix. This is
the matrix for converting from the surface space into object space.

inputs
   PCMatrix       pm - Matrix that want
returns
   none
*/
void CDoubleSurface::MatrixSet (PCMatrix pm)
{
   m_MatrixToObject.Copy (pm);
   m_MatrixToObject.Invert4 (&m_MatrixFromObject);

   // tell the embedded objects that they've moved
   TellEmbeddedThatMoved();
}

/**********************************************************************************
CDoubleSurface::MatrixGet - Gets the translation and rotation matrix. This is
the matrix for converting from the surface space into object space.

inputs
   PCMatrix       pm - Matrix that want
returns
   none
*/
void CDoubleSurface::MatrixGet (PCMatrix pm)
{
   pm->Copy (&m_MatrixToObject);
}


/**********************************************************************************
CDoubleSurface::NewSize - Adjusts the width and height of surface.

inputs
   fp      fWidth - New width. Translates from -fWidth/2 to fWidth/2
   fp      fHeight - New height. Translates from 0..fHeight

   THe following are from ChangeHV, and passed into it
   DWORD       dwH - Number of points in pafHOrig and pafHNew
   DWOD        dwV - Number of points in pafVOrig and pafVNew
   fp      fHLeftOrig, fHRightOrig, fVTopOrig, fVBottomOrig - Location from 0..1
               of original values of H and V.
   fp      fHLeftNew, fHRightNew, fVTopNew, fVBottomNew - Location from 0..1 for new
returns
   BOOL - TRUE if success
*/

BOOL CDoubleSurface::NewSize (fp fWidth, fp fHeight, DWORD dwH, DWORD dwV,
                                   fp *pafHOrig, fp *pafVOrig,
                                   fp *pafHNew, fp *pafVNew)
{

   // set framing dirty
   m_fFramingDirty = TRUE;

   CPoint   apPoints[2][2];
   DWORD x, y;
   for (y = 0; y < 2; y++) for (x = 0; x < 2; x++) {
      apPoints[y][x].Zero();
      apPoints[y][x].p[0] = ((fp)x - .5) * fWidth;
      apPoints[y][x].p[2] = (1.0 - (fp)y) * fHeight;

   }

   ControlPointsSet (2, 2, &apPoints[0][0],
      (DWORD*) SEGCURVE_LINEAR, (DWORD*) SEGCURVE_LINEAR, 1);

   // If have strech information then change the embedded objects
   if (dwH || dwV)
      ChangedHV (dwH, dwV, pafHOrig, pafVOrig, pafHNew, pafVNew);

   return TRUE;
}



/**********************************************************************************
CDoubleSurface::ControlPointsSet - Sets a bunch of control points at once

inputs
   DWORD       dwControlH - Number of control points in H
   DWORD       dwControlV - Number of control points in V
   PCPoint     paPoints - Pointer to control pointed. Arrayed as [dwControlV][dwControlH]
   DWORD       *padwSegCurveH - Pointer to an array of dwControlH DWORDs continging
                  SEGCURVE_XXX descrbing the curve. This can also be (DWORD*) SEGCURVE_XXX
                  directly. If not looped in h then dwControlH-1 points.

  NOTE: This points are in spline space, NOT object space.

   DWORD       *padwSegCurveV - Like padwSegCurveH. If not looped in v then dwControlV-1 points.
   DWORD       dwDetail - Detail level ot use. Passed to CSplineSurface::DetailSet()
returns
   BOOL - TRUE if success
*/

BOOL CDoubleSurface::ControlPointsSet (DWORD dwControlH, DWORD dwControlV,
   PCPoint paPoints, DWORD *padwSegCurveH, DWORD *padwSegCurveV, DWORD dwDetail)
{
   m_SplineCenter.DetailSet (dwDetail, dwDetail, dwDetail, dwDetail, .1);
   m_SplineCenter.ControlPointsSet(
      FALSE, FALSE,
      dwControlH, dwControlV, paPoints, padwSegCurveH, padwSegCurveV);

   // Readjust
   RecalcSides ();
   RecalcEdges ();


   // set framing dirty
   m_fFramingDirty = TRUE;

   return TRUE;
}



/************************************************************************************
CDoubleSurface::IntersectWithSurfacesAndCutout - Intersects this surface
with one or more surfaces.

inputs
   // for side A
   PCSplineSurface      *papssA - Pointer to an array of surfaces to test against
   PCMatrix             paMatrixA - Pointer to an array of matricies that convert
                        from papss space into this spline's space
   DWORD                dwNumA - Number of papss and paMatrix
   PTEXTUREPOINT        pKeepA - Use dwMode==2, keep this

   // for side B
   PCSplineSurface      *papssB - Pointer to an array of surfaces to test against
   PCMatrix             paMatrixB - Pointer to an array of matricies that convert
                        from papss space into this spline's space
   DWORD                dwNumB - Number of papss and paMatrix
   PTEXTUREPOINT        pKeepB - Use dwMode==2, keep this

   PWSTR                pszName - Name to use for the cutout
   BOOL                 fIgnoreCutouts - If TRUE then looks at the entire sheet of papss.
                        If FALSE, use only the post-cutout version.
   BOOL                 fClockwise - If TRUE use clockwise border, and clockwise orientation
   DWORD                dwMode -
                           0 - Don't change the orientation of intersections
                           1 - reorient the intersection so the smallest
                              cutout area (estimated) for that one intersection, is on the left.
                              The reorient function
                              only really works if there's just one sequence.
                           2 - Use pKeep
   BOOL                 fUseMin - If TRUE, use the minimum size result, FALSE use maximum.
                           Note: If !fUseMin then won't draw a loop around the edge of
                           the surface
   BOOL                 fFinalClockwise - If TRUE, want the final loop to be clockwise
                           (cut out the inside), or counter clockwise (cut outside out).
returns
   BOOL - TRUE if success
*/
BOOL CDoubleSurface::IntersectWithSurfacesAndCutout (
      CSplineSurface **papssA, PCMatrix paMatrixA, DWORD dwNumA, PTEXTUREPOINT pKeepA,
      CSplineSurface **papssB, PCMatrix paMatrixB, DWORD dwNumB, PTEXTUREPOINT pKeepB,
      PWSTR pszName, BOOL fIgnoreCutouts,
      BOOL fClockwise, DWORD dwMode, BOOL fUseMin, BOOL fFinalClockwise)
{
   m_SplineA.IntersectWithSurfacesAndCutout (papssA, paMatrixA, dwNumA, pszName,
      fIgnoreCutouts, fClockwise, dwMode, fUseMin, fFinalClockwise, pKeepA);
   m_SplineB.IntersectWithSurfacesAndCutout (papssB, paMatrixB, dwNumB, pszName,
      fIgnoreCutouts, fClockwise, dwMode, fUseMin, fFinalClockwise, pKeepB);
   return TRUE;
}


/*************************************************************************************
CDoubleSurface::RotateSplines - Rotate and translate the internal splines
so that the Y values at the left,bottom and right,bottom are 0. This also does
a translation in Y as necessary, and it centers in X while it's at it.

NOTE: Does NOT call RecalcSides()

inputs
   PCPoint     pLB - Left,bottom - although not actually stored in spline center, used as
                     a guide
   PCPoint     pR - Right,bottom - not actually stored either. just as a guide for rotation
returns
   none
*/
void CDoubleSurface::RotateSplines (PCPoint pLB, PCPoint pRB)
{
   CPoint p1, p2;
   DWORD x,y;
   DWORD dwHeight, dwWidth;
   p1.Copy (pLB);
   p2.Copy (pRB);

   if ((fabs(p1.p[1]) < CLOSE) && (fabs(p2.p[1]) < CLOSE))
      return;

   dwWidth = ControlNumGet(TRUE);
   dwHeight = ControlNumGet(FALSE);

   // generate a matrix that rotates the points
   CPoint A, B, C;
   CMatrix m, mInv;
   A.Subtract (&p2, &p1);
   A.p[2] = 0; // only do rotation on x and y, don't rotate through z
   A.Normalize();
   if (A.Length() < CLOSE) {
      m.Identity();
      mInv.Identity();
      goto translate;   // no point rotating
   }
   C.Zero();
   C.p[2] = 1;
   B.CrossProd (&C, &A);
   B.Normalize();

   m.RotationFromVectors (&A, &B, &C);
   m.Invert (&mInv); // dont need to use Invert4

   // rotate the two points
   p1.p[3] = p2.p[3] = 1;
   p1.MultiplyLeft (&mInv);
   p2.MultiplyLeft (&mInv);

translate:
   // translate - both p1 & p2 should have same Y
   CMatrix pTrans;
   pTrans.Translation (-(p1.p[0] + p2.p[0])/2, -(p1.p[1] + p2.p[1])/2, 0);
#ifdef _DEBUG
   // just to test that it goes in the right direction
   p1.p[3] = p2.p[3] = 1;
   p1.MultiplyLeft (&pTrans);
   p2.MultiplyLeft (&pTrans);
#endif
   mInv.MultiplyRight (&pTrans);

   // now invert again to get the matrix
   mInv.Invert4 (&m);

   if (m_pTemplate->m_pWorld)
      m_pTemplate->m_pWorld->ObjectAboutToChange (m_pTemplate);

   // apply this to all the points in the spline center
   for (x = 0; x < dwWidth; x++) for (y = 0; y < dwHeight; y++) {
      m_SplineCenter.ControlPointGet (x, y, &p1);
      p1.p[3] = 1;
      p1.MultiplyLeft (&mInv);
      m_SplineCenter.ControlPointSet (x, y, &p1);
   }

   // set rotation matrix.
   CMatrix mMain;
   MatrixGet (&mMain);
   mMain.MultiplyLeft (&m);
   MatrixSet (&mMain);

   if (m_pTemplate->m_pWorld)
      m_pTemplate->m_pWorld->ObjectChanged (m_pTemplate);
}

/*************************************************************************************
CDoubleSurface::ControlPointSetRotation - Called to change the valud of a control point
when in rotation mode

inputs
   DWORD       dwX, dwY - X and Y location (0 for left/top, 1 for right/bottom)
   PCPoint     pVal - Contains the new location, in object coordinates
   BOOL        fSendNotify - If TRUE, send the notification OSM_WALLMOVED to any
               other walls in the area. If FALSE, dont
   BOOL        fKeepSplineMatrixIdent - If TRUE, this keeps the spline matrix
               as identity (assuming that it's a wall object).
returns
   BOOL - TRUE if successful
*/
BOOL CDoubleSurface::ControlPointSetRotation (DWORD dwX, DWORD dwY, PCPoint pVal,
                                              BOOL fSendNotify, BOOL fKeepSplineMatrixIdent)
{
   DWORD dwWidth, dwHeight, x, y;
   dwWidth = ControlNumGet(TRUE);
   dwHeight = ControlNumGet(FALSE);

   // need to see if rotation matrix changed - if did then keep
   // m_ds matrix as identity, but change this object matrix
   CMatrix mOld, mNew;
   MatrixGet (&mOld);

   // copy of the point to internal variable since will modify it
   CPoint   pv;
   pv.Copy (pVal);
   pv.p[3] = 1;
   pv.MultiplyLeft (&m_MatrixToObject);

   // the rule of rotation is that the two end points on the bottom
   // must be on y=0 plane. If they're not right now then make it so
   CPoint p1, p2;
   m_SplineCenter.ControlPointGet (0, dwHeight-1, &p1);
   m_SplineCenter.ControlPointGet (dwWidth-1, dwHeight-1, &p2);
   RotateSplines (&p1, &p2);

   // because may have rotated object, reapply
   pv.MultiplyLeft (&m_MatrixFromObject);

   // get the current 4 corner pints
   CPoint apCorner[2][2];  // [x][y]
   for (x = 0; x < 2; x++) for (y = 0; y < 2; y++)
      m_SplineCenter.ControlPointGet (x ? (dwWidth-1) : 0, y ? (dwHeight-1) : 0, &apCorner[x][y]);

   // remember the old point, in world space
   CPoint pMoveOld;
   pMoveOld.Copy (&apCorner[dwX][1]);
   pMoveOld.p[3] = 1;
   pMoveOld.MultiplyLeft (&m_MatrixToObject);
   if (m_pTemplate)
      pMoveOld.MultiplyLeft (&m_pTemplate->m_MatrixObject);

   // remeber old stretch vector
   CPoint pOld, pNew;
   pOld.Subtract (&apCorner[1][1], &apCorner[0][1]);

   // since keep top and bottom points in sync, remember delta that changed
   CPoint pDelta;
   pDelta.Subtract (&pv, &apCorner[dwX][dwY]);
   if (pDelta.Length() < CLOSE)
      return TRUE;  // didn't move

   // change both the corner and the one below
   // BUGFIX - Different dimensions affected differently
   fp fZ;
   fZ = pDelta.p[2];
   pDelta.p[2] = 0;  // add this separately
   apCorner[dwX][0].Add (&pDelta);
   apCorner[dwX][1].Add (&pDelta);
   // BUGFIX - Allow to stretch height of wall up or down
   // BUGFIX - Only did if dwY==0 originally, but because using this when creating
   // wall, also did for dwY == 1
   apCorner[0][dwY].p[2] += fZ;
   apCorner[1][dwY].p[2] += fZ;

   // apply the stretch
   pNew.Subtract (&apCorner[1][1], &apCorner[0][1]);
   pOld.p[2] = 0; // dont apply stretch in z
   pNew.p[2] = 0; // dont apply stretch in z
   fp fStretch;
   fStretch = pOld.Length();
   if (fStretch > CLOSE) {
      fStretch = pNew.Length() / fStretch;
      if (m_pTemplate->m_pWorld)
         m_pTemplate->m_pWorld->ObjectAboutToChange (m_pTemplate);

      // the anchor point (for stretching) is on the opposite horizontal side
      CPoint pAnchor;
      m_SplineCenter.ControlPointGet (dwX ? 0 : (dwWidth-1), dwHeight-1, &pAnchor);

      // calculate rotation matrix for these
      CPoint p1, p2;
      p1.Copy (&apCorner[0][1]);
      p2.Copy (&apCorner[1][1]);
      p1.p[3] = p2.p[3] = 1;

      // generate a matrix that rotates the points
      CPoint A, B, C;
      CMatrix m, mInv;
      A.Subtract (&p2, &p1);
      A.p[2] = 0; // only do rotation on x and y, don't rotate through z
      A.Normalize();
      if (A.Length() < CLOSE) {
         m.Identity();
         mInv.Identity();
      }
      else {
         C.Zero();
         C.p[2] = 1;
         B.CrossProd (&C, &A);

         m.RotationFromVectors (&A, &B, &C);
         m.Invert (&mInv); // dont need to use Invert4

         // rotate the two points
         p1.MultiplyLeft (&mInv);
         p2.MultiplyLeft (&mInv);
      }

#if 0 // I dont think I need this
      // translate - both p1 & p2 should have same Y
      CMatrix pTrans;
      pTrans.Translation (-(p1.p[0] + p2.p[0]), -(p1.p[1] + p2.p[1])/2, 0);
   #ifdef _DEBUG
      // just to test that it goes in the right direction
      p1.MultiplyLeft (&pTrans);
      p2.MultiplyLeft (&pTrans);
   #endif
      mInv.MultiplyRight (&pTrans);
#endif //0

      // now invert again to get the matrix
      mInv.Invert4 (&m);

      for (x = 0; x < dwWidth; x++) for (y = 0; y < dwHeight; y++) {
         // dont do corner points since will do them later
         if ((x == 0) && ((y == 0) || (y == (dwHeight-1))))
            continue;
         if ((x == (dwWidth-1)) && ((y == 0) || (y == (dwHeight-1))))
            continue;

         // get the point
         m_SplineCenter.ControlPointGet (x, y, &p1);

         p2.Subtract (&p1, &pAnchor);  // stretch against corner that didn't change
         p2.p[2] = 0;   // dont change z
         p2.Scale (fStretch);
         p2.MultiplyLeft (&m);
         p2.Add (&pAnchor);
         p1.p[0] = p2.p[0];
         p1.p[1] = p2.p[1];
         p1.p[3] = 1;
         m_SplineCenter.ControlPointSet (x, y, &p1);

      }

      if (m_pTemplate->m_pWorld)
         m_pTemplate->m_pWorld->ObjectChanged (m_pTemplate);
   }


   // use this the bottom for rotation
   if (m_pTemplate->m_pWorld)
      m_pTemplate->m_pWorld->ObjectAboutToChange (m_pTemplate);
   for (x = 0; x < 2; x++) for (y = 0; y < 2; y++)
      m_SplineCenter.ControlPointSet (x ? (dwWidth-1) : 0, y ? (dwHeight-1) : 0, &apCorner[x][y]);
   if (m_pTemplate->m_pWorld)
      m_pTemplate->m_pWorld->ObjectChanged (m_pTemplate);

   RotateSplines (&apCorner[0][1], &apCorner[1][1]);

   // recalc other sides
   RecalcSides ();

   if (fKeepSplineMatrixIdent) {
      // get the new matrix
      MatrixGet (&mNew);

      // clear it to identity
      CMatrix mIdent, mInv;
      mIdent.Identity();
      if (!mIdent.AreClose (&mNew)) {
         MatrixSet (&mIdent);

         // add this into existing
         m_pTemplate->ObjectMatrixGet (&mIdent);
         mOld.Invert4 (&mInv);
         mNew.MultiplyRight (&mInv);
         mIdent.MultiplyLeft (&mNew);  // = oldObjMatrix x oldSurfMatrix-1 x newSurfMatrix
         m_pTemplate->ObjectMatrixSet (&mIdent);
      }
   }


   // send notification to others
   if (fSendNotify) {
      CPoint   pMoveNew;
      m_SplineCenter.ControlPointGet (dwX ? (dwWidth-1) : 0, dwHeight-1, &pMoveNew);
      pMoveNew.p[3] = 1;
      pMoveNew.MultiplyLeft (&m_MatrixToObject);
      if (m_pTemplate)
         pMoveNew.MultiplyLeft (&m_pTemplate->m_MatrixObject);

      TellOtherWallsThatMoving (&pMoveOld, &pMoveNew);
   }

   return TRUE;
}


/*************************************************************************************
CDoubleSurface::TellOtherWallsThatMoving - Tells all the other walls that we have
a point that's moving. Other walls with corners at this point will move also.

inputs
   PCPoint     pOld - Old point. This is the bottom of the corner moving
   PCPoint     pNew - New point. This is the new location.
            (Both of these are in WORLD coords)
returns
   none
*/
void CDoubleSurface::TellOtherWallsThatMoving (PCPoint pOld, PCPoint pNew)
{
   // must be a wall, and must have a world
   if (HIWORD(m_dwType != 1) || !m_pTemplate || !m_pTemplate->m_pWorld)
      return;

   // convert to world space
   CPoint pCorner1, pCorner2, pDelta;
   pDelta.Zero();
   pDelta.p[0] = pDelta.p[1] = pDelta.p[2] = .1;
   pCorner1.Subtract (pOld, &pDelta);
   pCorner2.Add (pOld, &pDelta);

   CListFixed list;
   CMatrix mIdent;
   mIdent.Identity();
   list.Init (sizeof(DWORD));
   m_pTemplate->m_pWorld->IntersectBoundingBox (&mIdent, &pCorner1, &pCorner2, &list);

   // wrap up messge
   OSMWALLMOVED wm;
   memset (&wm, 0, sizeof(wm));
   wm.pNew.Copy (pNew);
   wm.pOld.Copy (pOld);

   // sent messages out
   GUID gGUID;
   DWORD i;
   for (i = 0; i < list.Num(); i++) {
      DWORD dwObject = *((DWORD*) list.Get(i));
      PCObjectSocket pos = m_pTemplate->m_pWorld->ObjectGet (dwObject);
      if (!pos)
         continue;
      pos->GUIDGet (&gGUID);
      if (IsEqualGUID(gGUID, m_pTemplate->m_gGUID))
         continue;   // dont send to self

      pos->Message (OSM_WALLMOVED, &wm);
   }
}

/*************************************************************************************
CDoubleSurface::WallIntelligentBevel - Look for other walls at the intersections
and calculate bevel points. This then changes the object to use the new bevels,
or sets to 0 if no change.

NOTE: Does NOT call world changed
*/
void CDoubleSurface::WallIntelligentBevel (void)
{
   // must be a wall, and must have a world
   if (HIWORD(m_dwType != 1) || !m_pTemplate || !m_pTemplate->m_pWorld)
      return;

   DWORD dwEdge;
   DWORD dwWidth, dwHeight, j;
   dwWidth = ControlNumGet(TRUE);
   dwHeight = ControlNumGet(FALSE);
   fp   afBevel[2];
   afBevel[0] = afBevel[1] = 0;

   // get the left and right edges for this one by just calling self with message
   OSMWALLQUERYEDGES wqThis, wqTest;
   if (!Message (OSM_WALLQUERYEDGES, &wqThis))
      return;
   for (j = 0; j < 2; j++) {
      wqThis.apAVector[j].Normalize();
      wqThis.apBVector[j].Normalize();
   }

   // matrix to convert from spline space to world space, and back
   CMatrix m, mInv;
   m.Multiply (&m_pTemplate->m_MatrixObject, &m_MatrixToObject);
   m.Invert4 (&mInv);

   // loop over both edges
   for (dwEdge = 0; dwEdge < 2; dwEdge++) {
      // convert to world space
      CPoint pCorner1, pCorner2, pDelta;
      pDelta.Zero();
      pDelta.p[0] = pDelta.p[1] = pDelta.p[2] = .1;
      pCorner1.Subtract (&wqThis.apCorner[dwEdge], &pDelta);
      pCorner2.Add (&wqThis.apCorner[dwEdge], &pDelta);

      CListFixed list;
      CMatrix mIdent;
      mIdent.Identity();
      list.Init (sizeof(DWORD));
      list.Clear();
      m_pTemplate->m_pWorld->IntersectBoundingBox (&mIdent, &pCorner1, &pCorner2, &list);

      // sent messages out
      GUID gGUID;
      DWORD i, dwInter;
      for (i = 0; i < list.Num(); i++) {
         DWORD dwObject = *((DWORD*) list.Get(i));
         PCObjectSocket pos = m_pTemplate->m_pWorld->ObjectGet (dwObject);
         if (!pos)
            continue;
         pos->GUIDGet (&gGUID);
         if (IsEqualGUID(gGUID, m_pTemplate->m_gGUID))
            continue;   // dont send to self

         if (!pos->Message (OSM_WALLQUERYEDGES, &wqTest))
            continue;

         for (dwInter = 0; dwInter < 2; dwInter++) {
            // if it's not intersecting then don't care
            if ( (fabs(wqTest.apCorner[dwInter].p[0] - wqThis.apCorner[dwEdge].p[0]) > CLOSE)  ||
               (fabs(wqTest.apCorner[dwInter].p[1] - wqThis.apCorner[dwEdge].p[1]) > CLOSE)  ||
               (fabs(wqTest.apCorner[dwInter].p[2] - wqThis.apCorner[dwEdge].p[2]) > 1) )   // allow z to be off by 1m
               continue;

            // if basically parallel then don't care
            wqTest.apAVector[dwInter].Normalize();
            wqTest.apBVector[dwInter].Normalize();
            CPoint pt;
            pt.CrossProd (&wqTest.apAVector[dwInter], &wqThis.apAVector[dwInter]);
            if (pt.Length() < CLOSE)
               continue;
            pt.CrossProd (&wqTest.apBVector[dwInter], &wqThis.apBVector[dwInter]);
            if (pt.Length() < CLOSE)
               continue;
            
            // if gets here meets critera
            break;
         }
         if (dwInter >= 2)
            continue;   // meets criteria

         // figure out where lines in side A intersect, and side B interesct
         CPoint apInter[2];   // 0 = A, 1 = B
         DWORD dwSide;
         for (dwSide = 0; dwSide < 2; dwSide++) {
            // mught need to flip the sides. Calculate normals for each line and see how
            // they compare
            DWORD dwTestSide;
            CPoint pV, pN, pN2, pN3;
            pV.Subtract (&wqTest.apBCorner[dwInter], &wqTest.apACorner[dwInter]);
            pV.p[2] = 0;
            pV.Normalize();
            pN.CrossProd (&pV, &wqTest.apAVector[dwInter]);
            pN2.CrossProd (&pN, &wqTest.apAVector[dwInter]);
            pN2.p[2] = 0;
            pN2.Normalize();
            pV.Subtract (&wqThis.apBCorner[dwEdge], &wqThis.apACorner[dwEdge]);
            pV.p[2] = 0;
            pV.Normalize();
            pN.CrossProd (&pV, &wqThis.apAVector[dwEdge]);
            pN3.CrossProd (&pN, &wqThis.apAVector[dwEdge]);
            pN3.p[2] = 0;
            pN3.Normalize();

            // if they're not pointing in generally the same direction then will need to flip
            // before averaging
            dwTestSide = dwSide;
//            if (pN2.DotProd(&pN3) < 0) {
//               dwTestSide = !dwTestSide;
//               pN3.Scale (-1);
//            }

            pN3.Average (&pN2);  // average together to get half way point

            // cross produc with line and see edge
            CPoint pCross;
            pCross.CrossProd (&pN3, &wqThis.apAVector[dwEdge]);
            if (pCross.p[2] < 0)
               dwTestSide = !dwTestSide;

            if (!dwEdge && !dwInter)
               dwTestSide = dwTestSide;
            else if (!dwEdge && dwInter)
               dwTestSide = !dwTestSide;
            else if (dwEdge && !dwInter)
               dwTestSide = dwTestSide;
            else if (dwEdge && dwInter)
               dwTestSide = !dwTestSide;

            // convert one of the lines into a plane
            pV.Subtract (&wqTest.apBCorner[dwInter], &wqTest.apACorner[dwInter]);
            pV.Normalize();
            if (pV.Length() < CLOSE)
               break;
            pN.CrossProd (&pV, dwTestSide ? &wqTest.apBVector[dwInter] : &wqTest.apAVector[dwInter]);
            if (pN.Length() < CLOSE)
               break;
            pN2.CrossProd (&pN, dwTestSide ? &wqTest.apBVector[dwInter] : &wqTest.apAVector[dwInter]);

            // interesct
            CPoint pStart, pEnd;
            pStart.Copy (dwSide ? &wqThis.apBCorner[dwEdge] : &wqThis.apACorner[dwEdge]);
            pEnd.Add (&pStart, dwSide ? &wqThis.apBVector[dwEdge] : &wqThis.apAVector[dwEdge]);
            if (!IntersectLinePlane (&pStart, &pEnd,
               dwTestSide ? &wqTest.apBCorner[dwInter] : &wqTest.apACorner[dwInter],
               &pN2, &apInter[dwSide]))
               break;   // didnt intersect
         }
         if (dwSide < 2)
            continue;   // no intersection here
   
         // convert the two in object space
         apInter[0].MultiplyLeft (&mInv);
         apInter[1].MultiplyLeft (&mInv);

         // convert this to an angle line
         CPoint pInter, pTemp;
         fp fInter, f;
         pInter.Subtract (&apInter[1], &apInter[0]);
         fInter = atan2(pInter.p[0], pInter.p[1]);

         // get the point
         m_SplineCenter.HVToInfo (dwEdge ? 1 : 0, 1, NULL, &pTemp);
         pTemp.Scale(-1);

         // left side
         f = atan2(pTemp.p[0], pTemp.p[1]);
         if (!dwEdge) {
            f = fInter - f;
         }
         else {
            f = f - fInter;
         }
            f = fmod(f + 3 * PI, 2 * PI) - PI; 
         afBevel[dwEdge] = f;

         // found match to exit
         break;
      }
      if (i >= list.Num())
         continue;   // didnt find anything to match with
   }

   // set the angles
   m_fBevelLeft = afBevel[0];
   m_fBevelRight = afBevel[1];
   RecalcSides();
}


/*************************************************************************************
CDoubleSurface::EmbedIntegrityCheck - Looks through all the cutouts for
embeddd objects (which are guids). Then, checks in the m_pTemplate->m_pWorld to
see if they still exist. If they don't the holes are removed.
*/
void CDoubleSurface::EmbedIntegrityCheck (void)
{
   if (!m_pTemplate || !m_pTemplate->m_pWorld)
      return;

   DWORD dwSide, i;
   BOOL fChanged;
   fChanged = FALSE;
   for (dwSide = 0; dwSide < 2; dwSide++) {
      PCSplineSurface pss = (dwSide ? &m_SplineA : &m_SplineB);

      for (i = pss->CutoutNum()-1; i < pss->CutoutNum(); i--) {
         PWSTR psz;
         PTEXTUREPOINT ptp;
         DWORD dwNum;
         BOOL fClock;
         psz = NULL;
         pss->CutoutEnum (i, &psz, &ptp, &dwNum, &fClock);
         if (!psz)
            continue;

         if (psz[0] != L'{')
            continue;   // needs to start with that

         GUID gVal;
         if (S_OK != IIDFromString (psz, &gVal))
            continue;

         // find it
         if (m_pTemplate->m_pWorld->ObjectFind (&gVal) != -1)
            continue;

         if (!fChanged)
            m_pTemplate->m_pWorld->ObjectAboutToChange (m_pTemplate);
         fChanged = TRUE;

         // remove
         WCHAR szTemp[64];
         wcscpy (szTemp, psz);
         pss->CutoutRemove (szTemp);
      }
   }

   if (fChanged)
      m_pTemplate->m_pWorld->ObjectChanged (m_pTemplate);
}


/***************************************************************************************
CDoubleSurface::OverlayIntersections - Find the intersections for this surface so they can be
used for overlay paint, or for create overlay from intersection.

inputs
   BOOL              fSideA - TRUE if doing side A, FALSE if side B
returns
   PCListFixed - List of sequences. The list contains a list of PCListFixed, which are
   the specific coordinates. This (including sub-lists) must be freed by caller
*/
PCListFixed CDoubleSurface::OverlayIntersections (BOOL fSideA)
{
   // find out what line segments intersect
   DWORD i;
   CMem mem;
   PCListFixed plLines, plLines2;
   DWORD dwCheckAgainst, dwIntersectWith;
   dwCheckAgainst = 0x0001;   // intersect with the world
   dwIntersectWith = 5; // any spline center (wh
   switch (HIWORD(m_dwType)) {
      default:
      case 1:  // wall
         dwCheckAgainst |= 0x0002;  // check against itself
         dwIntersectWith = 13;  // intersect with own floors, but use the spline center
         break;
      case 2:  // roof/ceiling
         dwCheckAgainst |= 0x0002;  // check against itself
         dwIntersectWith = 2;   // intersect with own outside walls, thats it
         break;
      case 3:  // floor
         // if it's a floor, dont bother checking with other parts of self since already clipped to walls/roof
         break;
   }
   plLines = FindIntersections (INTERSECTEXTEND, fSideA ? 2 : -2,
      5 /* with any spline center for other objects*/, dwIntersectWith,
      dwCheckAgainst, FALSE /*use cutouts */);

   if (!plLines)
      return FALSE;
   if (!plLines->Num()) {
      delete plLines;
      return FALSE;
   }

#ifdef _DEBUG
   // display all the sequences
   char szTemp[64];
   PCListFixed pt;
   DWORD j;
   for (j = 0; j < plLines->Num(); j++) {
      PTEXTUREPOINT p = (PTEXTUREPOINT) plLines->Get(j);

      sprintf (szTemp, "(%g,%g) to (%g,%g)\r\n", (double)p[0].h, (double)p[0].v, (double)p[1].h, (double)p[1].v);
      OutputDebugString (szTemp);
   }

   OutputDebugString ("\r\n");
#endif

   // BUGFIX - Originally didn't think could use cutouts because of the doors in walls
   // would not make a perfect intersection, but since intersecting against the center
   // I can. Want to use the cutouts so that walls on ground floor don't intrude to floor
   // above. NOTE: This doesn't really seem to fix the walls from ground floor intruding above
   // problem

   // Also intersect with edge cutout
   TEXTUREPOINT atp[2];
   PCPoint pp;
   plLines->Required (plLines->Num() + m_SplineEdge.QueryNodes());
   for (i = 0; i < m_SplineEdge.QueryNodes(); i++) {
      pp = m_SplineEdge.LocationGet (i);
      atp[0].h = pp->p[0];
      atp[0].v = pp->p[1];
      pp = m_SplineEdge.LocationGet ((i+1) % m_SplineEdge.QueryNodes());
      atp[1].h = pp->p[0];
      atp[1].v = pp->p[1];

      plLines->Add (atp);
   }

   // convert to sequences
   plLines2 = LineSegmentsToSequences (plLines);
   delete plLines;   // BUGFIX
   if (!plLines2)
      return NULL;

   // get rid of dangling sequences
   LineSequencesRemoveHanging (plLines2, TRUE);

#ifdef _DEBUG
   // display all the sequences
   //char szTemp[64];
   //DWORD j;
   for (i = 0; i < plLines2->Num(); i++) {
      sprintf (szTemp, "Sequence %d:", i);
      OutputDebugString (szTemp);

      pt = *((PCListFixed*)plLines2->Get(i));

      for (j = 0; j < pt->Num(); j++) {
         PTEXTUREPOINT p = (PTEXTUREPOINT) pt->Get(j);

         sprintf (szTemp, " (%g,%g)", (double)p->h, (double)p->v);
         OutputDebugString (szTemp);
      }

      OutputDebugString ("\r\n");
   }
#endif

   return plLines2;
}

/***************************************************************************************
CDoubleSurface::OverlayForPaint - This figures out where the overlay should go
for paint so that it occupies the space taken up by the wall (intersected with other
walls and floors).

inputs
   BOOL              fSideA - TRUE if doing this on side A, FALSE side B
   PTEXTUREPOINT     pClick - Location that clicked on
   PCListFixed       pl - To be filled in with a list of TEXTUREPOINT going clockwise
                     around the point of click
returns
   BOOL - TRUE if found an intersection. FALSE if didnt find one
*/
BOOL CDoubleSurface::OverlayForPaint (BOOL fSideA, PTEXTUREPOINT pClick, PCListFixed pl)
{
   BOOL fRet;
   fRet = FALSE;
   pl->Init (sizeof(TEXTUREPOINT));


   PCListFixed plLines2;
   DWORD i;
   plLines2 = OverlayIntersections (fSideA);

   // BUGFIX - Mirror interected lines so they go in both directions. Before doing that,
   // convert into sequences so that will minimize the number of line segments
   SequencesAddFlips (plLines2);


   CListVariable lv;
   DWORD       dwTemp[10];   // BUGFIX - Make dwTemp[10] instead of dwTemp[100], limit complexity. Speed up
   PCListFixed plScores;
   BOOL fClockwise;
   fClockwise = TRUE;
   // BUGFIX - Passed point clicked on so they'll be in the center
   plScores = SequencesSortByScores (plLines2, !fClockwise, pClick);
   if (!plScores) {
      fRet = FALSE;
      goto finished;
   }
#ifdef _DEBUG
   {
   // display all the sequences
   char szTemp[64];
   PCListFixed pt;
   for (i = 0; i < plLines2->Num(); i++) {
      sprintf (szTemp, "Sequence %d (score=%d):", i, *((int*)plScores->Get(i)));
      OutputDebugString (szTemp);

      pt = *((PCListFixed*)plLines2->Get(i));

      DWORD j;
      for (j = 0; j < pt->Num(); j++) {
         PTEXTUREPOINT p = (PTEXTUREPOINT) pt->Get(j);

         sprintf (szTemp, " (%g,%g)", (double)p->h, (double)p->v);
         OutputDebugString (szTemp);
      }

      OutputDebugString ("\r\n");
   }
   }
#endif

   PathFromSequences (plLines2, &lv, dwTemp, 0, sizeof(dwTemp)/sizeof(DWORD), pClick);

#ifdef _DEBUG
   {
      // display all the sequences
      char szTemp[64];
      //DWORD i, j;
      DWORD *padw, dw;
      for (i = 0; i < lv.Num(); i++) {
         sprintf (szTemp, "Path %d:", i);
         OutputDebugString (szTemp);

         dw =  (DWORD)lv.Size(i) / sizeof(DWORD);
         padw = (DWORD*)  lv.Get(i);

         DWORD j;
         for (j = 0; j < dw; j++) {
            sprintf (szTemp, " %d", padw[j]);
            OutputDebugString (szTemp);
         }

         OutputDebugString ("\r\n");
      }
   }
#endif

   DWORD dwBest;
   dwBest = PathWithLowestScore (plLines2, &lv, plScores);
   delete plScores;

#if 0// def _DEBUG
   char szTemp[64];
   sprintf (szTemp, "Lowest = %d\r\n", dwBest);
   OutputDebugString (szTemp);
#endif
   if (dwBest != (DWORD) -1) {
      fRet = TRUE;
      PCListFixed pEdge;
      pEdge = PathToSplineList (plLines2, &lv, dwBest, !fClockwise);

      if (pEdge) {
         TEXTUREPOINT tp;
         pl->Required (pl->Num() + pEdge->Num());
         for (i = 0; i < pEdge->Num(); i++) {
            PCPoint pp = (PCPoint) pEdge->Get(i);
            tp.h = pp->p[0];
            tp.v = pp->p[1];
            pl->Add (&tp);
         }
         delete pEdge;
      }
      else
         fRet = FALSE;
   }



finished:
   // Delete plLines2
   for (i = 0; i < plLines2->Num(); i++) {
      PCListFixed pl = *((PCListFixed*) plLines2->Get(i));
      delete pl;
   }
   delete plLines2;

   if (!pl->Num())
      fRet = FALSE;

   return fRet;
}


/***************************************************************************************
CDoubleSurface::FramingAddSolid - Adds a length of framing (noodle) to the list
of noodles to be rendered. The framing is a complete length with no breaks.

inputs
   PCSplineSurface      pss - Spline surface to use
   PTEXTUREPOINT        patp - Pointer to 2 texture points for start and end. Solid
                           column drawn between start and end
   fp                   *pafBevel - Pointer to 2 bevel angles (0 = perp to surfaces) for
                           start and end. in radians.
   fp                   *pafExtend - Extend this many meters beyond the edge of what
                           start/stop point given. Can also be negative.
   DWORD                dwShape - Shape number to pass onto the noodle
   PCPoint              pScale - Size of the shape, p[0] = width, p[1] = depth
   DWORD                dwColor - Which color class to put it in, just written in the
                           database along with noodle. 0..2
   fp                   fCenter - Number of meters the center of the framing member is
                           inside the surface. (Basically surface thick + frame thick/2).
                           Make negative to have framing on outside.)
returns
   BOOL - TRUE if success
*/
typedef struct {
   CPoint      pLoc;       // location
   CPoint      pNorm;      // normal
} FAS, *PFAS;

BOOL CDoubleSurface::FramingAddSolid (PCSplineSurface pss, PTEXTUREPOINT patp, fp *pafBevel,
                                      fp *pafExtend, DWORD dwShape, PCPoint pScale,
                                      DWORD dwColor, fp fCenter)
{
   // figure out if this framing is mostly horizontal or vertical
   // dwDim is the dimension that changes the most, 0 = h, 1 = v
   fp fDeltaH = patp[1].h - patp[0].h;
   fp fDeltaV = patp[1].v - patp[0].v;
   DWORD dwDim = (fabs(fDeltaH) > fabs(fDeltaV)) ? 0 : 1;
   if ((fabs(fDeltaH) < CLOSE) && (fabs(fDeltaV) < CLOSE))
      return TRUE;   // nothing to add

   // figure out the detail in the direction that going
   DWORD dwDetail = pss->QueryNodes (!dwDim) - 1;
   if (!dwDetail)
      return TRUE;
   fp fDetail = 1.0 / (fp) dwDetail;

   // get the start point
   CListFixed lFAS;
   lFAS.Init (sizeof(FAS));
   FAS fas;
   memset (&fas, 0, sizeof(fas));
   pss->HVToInfo (patp[0].h, patp[0].v, &fas.pLoc, &fas.pNorm);
   lFAS.Add (&fas);

   // add points in-between since the surface might be curved
   fp fCur;
   fCur = ceil((&patp[0].h)[dwDim] * (fp)dwDetail + CLOSE) * fDetail;
   for (; fCur + CLOSE < (&patp[1].h)[dwDim]; fCur += fDetail) {
      fp fOther = (fCur - (&patp[0].h)[dwDim]) / (dwDim ? fDeltaV : fDeltaH);
      fOther = fOther * (dwDim ? fDeltaH : fDeltaV) + (&patp[0].h)[!dwDim];
      fOther = max(0, fOther);
      fOther = min(1, fOther);

      pss->HVToInfo (dwDim ? fOther : fCur, dwDim ? fCur : fOther, &fas.pLoc, &fas.pNorm);
      lFAS.Add (&fas);
   }

   // add the end point
   pss->HVToInfo (patp[1].h, patp[1].v, &fas.pLoc, &fas.pNorm);
   lFAS.Add (&fas);

   // look for linear segments and eliminate them
   PFAS pfas;
   DWORD i;
   for (i = 0; i+2 < lFAS.Num(); i++) {
      pfas = (PFAS) lFAS.Get(i);

      // subtract two distances
      CPoint p1, p2;
      p1.Subtract (&pfas[1].pLoc, &pfas[0].pLoc);
      p2.Subtract (&pfas[2].pLoc, &pfas[0].pLoc);
      p1.Normalize();
      p2.Normalize();
      if (p1.DotProd (&p2) >= .99)
         lFAS.Remove (i+1);   // next one is linear with following
   }

   // extend the sides
   pfas = (PFAS) lFAS.Get(0);
   DWORD dwNum;
   dwNum = lFAS.Num();
   for (i = 0; i < 2; i++) {
      if (!pafExtend[i])
         continue;

      PCPoint p1, p2;
      CPoint pSub;
      p1 = i ? &pfas[dwNum-1].pLoc : &pfas[0].pLoc;
      p2 = i ? &pfas[dwNum-2].pLoc : &pfas[1].pLoc;
      pSub.Subtract (p1, p2);
      pSub.Normalize ();
      pSub.Scale (pafExtend[i]);
      p1->Add (&pSub);
   }

   // create the noodle
   PCNoodle pNood;
   pNood = new CNoodle;
   if (!pNood)
      return FALSE;
   
   CPoint pBevel;
   for (i = 0; i < 2; i++) {
      if (!pafBevel[i])
         continue;

      pBevel.Zero();
      pBevel.p[1] = sin(pafBevel[i]) * (i ? 1 : -1);
      pBevel.p[2] = cos(pafBevel[i]);
      pNood->BevelSet (!i, 1, &pBevel);
   }

   pNood->DrawEndsSet (TRUE);

   // path spline
   CMem mem;
   if (!mem.Required (dwNum * sizeof(CPoint)))
      return FALSE;
   PCPoint pMem;
   pMem = (PCPoint) mem.p;
   for (i = 0; i < dwNum; i++)
      pMem[i].Copy (&pfas[i].pLoc);
   pNood->PathSpline (FALSE, dwNum, pMem, (DWORD*)SEGCURVE_LINEAR, 0);

   // creae the front spline...
   BOOL fAllSame;
   fAllSame = TRUE;
   for (i = 0; i < dwNum; i++) {
      pfas[i].pNorm.Normalize();
      if (!i)
         continue;
      if (fAllSame && (pfas[i].pNorm.DotProd (&pfas[0].pNorm) < .99))
         fAllSame = FALSE;
   }
   if (fAllSame)
      pNood->FrontVector (&pfas[0].pNorm);
   else {
      for (i = 0; i < dwNum; i++)
         pMem[i].Copy (&pfas[i].pNorm);
      pNood->FrontSpline (dwNum, pMem, (DWORD*) SEGCURVE_LINEAR, 0);
   }

   CPoint pOffset;
   pOffset.Zero();
   pOffset.p[1] = fCenter;
   pNood->OffsetSet(&pOffset);

   pNood->ScaleVector (pScale);
   pNood->ShapeDefault (dwShape);

   // add the noodle to the list
   FNOODLE fn;
   memset (&fn, 0, sizeof(fn));
   fn.dwColor = dwColor;
   fn.pNoodle = pNood;
   m_lFNOODLE.Add (&fn);

   return TRUE;
}



/***************************************************************************************
CDoubleSurface::FramingAddWithBreaks - Adds a length of framing (noodle) to the list
of noodles to be rendered. The framing WILL BE BROKEN UP by cutouts in pss.

inputs
   PCSplineSurface      pss - Spline surface to use
   PTEXTUREPOINT        patp - Pointer to 2 texture points for start and end. Solid
                           column drawn between start and end
   fp                   *pafBevel - Pointer to 2 bevel angles (0 = perp to surfaces) for
                           start and end. in radians.
   fp                   *pafExtend - Extend this many meters beyond the edge of what
                           start/stop point given. Can also be negative.
   DWORD                dwShape - Shape number to pass onto the noodle
   PCPoint              pScale - Size of the shape, p[0] = width, p[1] = depth
   DWORD                dwColor - Which color class to put it in, just written in the
                           database along with noodle. 0..2
   fp                   fCenter - Number of meters the center of the framing member is
                           inside the surface. (Basically surface thick + frame thick/2).
                           Make negative to have framing on outside.)
returns
   BOOL - TRUE if success
*/
BOOL CDoubleSurface::FramingAddWithBreaks (PCSplineSurface pss, PTEXTUREPOINT patp, fp *pafBevel,
                                      fp *pafExtend, DWORD dwShape, PCPoint pScale,
                                      DWORD dwColor, fp fCenter)
{
   // see what kind of line segments get out of the path
   CListFixed lKeep;
   lKeep.Init (sizeof(TEXTUREPOINT)*2);
   TEXTUREPOINT tpCur, atp[2];
   PTEXTUREPOINT ptp;
   tpCur = patp[0];
   while (TRUE) {
      if (AreClose(&tpCur, &patp[1]))
         break;   // near end
      if (!pss->FindLongestSegment (&tpCur, &patp[1], &atp[0], &atp[1], FALSE))
         break;

      lKeep.Add (&atp[0]);

      // move on
      tpCur.h = atp[1].h * .99 + patp[1].h * .01;  // just so skip a bit
      tpCur.v = atp[1].v * .99 + patp[1].v * .01;  // just so skip a bit
   }

   // draw the segments
   DWORD i;
   fp afBevel[2];
   fp afExtend[2];
   for (i = 0; i < lKeep.Num(); i++) {
      ptp = (PTEXTUREPOINT) lKeep.Get(i);

      // account for edge bevel and extend
      memcpy (afBevel, pafBevel, sizeof(afBevel));
      memcpy (afExtend, pafExtend, sizeof(afExtend));
      if (i) {
         // do a hard left edge since isn't first time through. past cutout
         afBevel[0] = afExtend[0] = 0;
      }
      if (i < lKeep.Num()-1) {
         // not at the end, to a hard right
         afBevel[1] = afExtend[1] = 0;
      }

      if (!FramingAddSolid (pss, ptp, &afBevel[0], &afExtend[0], dwShape, pScale, dwColor, fCenter))
         return FALSE;
   }



   return TRUE;
}


/***************************************************************************************
DistIntoLengths - Given a length to look for, and an array of lengths, this returns
a floating point number where the integer is the index into the array, and the fraction
is the percent through th elength.

inputs
   fp       fDist - Distnace to look for
   fp       *pafArray - Array of lengths
   DWORD    dwNum - Number of elements in the array
returns
   fp - value
*/
fp DistIntoLengths (fp fDist, fp *pafArray, DWORD dwNum)
{
   fp fVal = 0;

   while (dwNum && (fDist >= pafArray[0])) {
      fDist -= pafArray[0];
      dwNum--;
      pafArray++;
      fVal++;
   }

   if (!dwNum)
      return fVal;   // since went all the way to the end

   // else, fraction
   fVal += fDist / pafArray[0];
   return fVal;
}

/***************************************************************************************
CDoubleSurface::FramingAdd - Adds framing for a whole surface. This adds multiple frames.

inputs
   PCSplineSurface      pss - Spline surface to use
   BOOL                 fHorzBeams - If TRUE the beams are horizontal and run from h=0 to h=1.
                        If FALSE, they run from v=0 to v=1.
   fp                   *pafInset - Number of meters inside the TB edge (in case of fHorzBeams)
                        or LR edge (in case of !fHorzBeams), where the beams start.
   BOOL                 fMeasureInsetTL - If TRUE, measure the inset distance from the top (or left)
                        line. If false, measure inset from the right (or bottom) line.
                        Must be positive.
   fp                   fMinDist - Minimum distance between beams.
   fp                   *pafBevel - Pointer to 2 bevel angles (0 = perp to surfaces) for
                           start and end. in radians.
   fp                   *pafExtend - Extend this many meters beyond the edge of what
                           start/stop point given. Can also be negative.
   DWORD                dwShape - Shape number to pass onto the noodle
   PCPoint              pScale - Size of the shape, p[0] = width, p[1] = depth
   DWORD                dwColor - Which color class to put it in, just written in the
                           database along with noodle. 0..2
   fp                   fCenter - Number of meters the center of the framing member is
                           inside the surface. (Basically surface thick + frame thick/2).
                           Make negative to have framing on outside.)
returns
   BOOL - TRUE if success
*/
BOOL CDoubleSurface::FramingAdd (PCSplineSurface pss, 
                                 BOOL fHorzBeams, fp *pafInset, BOOL fMeasureInsetTL, fp fMinDist,
                                 fp *pafBevel, fp *pafExtend, DWORD dwShape, PCPoint pScale,
                                 DWORD dwColor, fp fCenter)
{
   // find the minimum and maximum extent of this
   TEXTUREPOINT tpMin, tpMax;
   if (!pss->FindMinMaxHV (&tpMin, &tpMax))
      return FALSE;
   TEXTUREPOINT tpDelta;
   tpDelta.h = tpMax.h - tpMin.h;
   tpDelta.v = tpMax.v - tpMin.v;
   if ((tpDelta.h < CLOSE) || (tpDelta.v < CLOSE))
      return TRUE;   // nothing here

   fp afInset[2];
   afInset[0] = pafInset[0];
   afInset[1] = pafInset[1];

   // loop over N points and get the locations. Do this to find length
#define FADETAIL     9
   DWORD i;
   CPoint apLoc[FADETAIL];
   fp afLen[FADETAIL-1];
   CPoint p;
   fp fLen, fOther, f;
   fLen = 0;
   fOther = (fMeasureInsetTL ? &tpMin.h : &tpMax.h)[!fHorzBeams];
   for (i = 0; i < FADETAIL; i++) {
      f = (fp) i / (fp) (FADETAIL-1) * (&tpDelta.h)[fHorzBeams] + (&tpMin.h)[fHorzBeams];
      pss->HVToInfo (fHorzBeams ? fOther : f, fHorzBeams ? f : fOther, &apLoc[i]);

      if (i) {
         p.Subtract (&apLoc[i], &apLoc[i-1]);
         afLen[i-1] = p.Length();
         fLen += afLen[i-1];
      }
   }

   // figure out how much to indent
   afInset[0] += pScale->p[0] / 2.0; // always indent by half a framing width
   afInset[1] += pScale->p[0] / 2.0; // always indent by half a framing width
   if (fLen <= afInset[0] + afInset[1] +CLOSE)
      return TRUE;   // not wide enough, so no inset

   // where does inset occur
   fp fStart, fEnd;
   fStart = DistIntoLengths (afInset[0], afLen, FADETAIL-1) / (fp)(FADETAIL-1);
   fEnd = DistIntoLengths (fLen - afInset[1], afLen, FADETAIL-1) / (fp)(FADETAIL-1);

   // how many do I need
   fMinDist = max(CLOSE,fMinDist);
   fp fNeed;
   fNeed = ceil((fLen - afInset[0] - afInset[1]) / fMinDist);
   fNeed = min(1000, fNeed);  // just put a ceiling on it


   // what's the real spacing
   fp fReal;
   fReal = (fLen - afInset[0] - afInset[1]) / fNeed;

   // add them
   fp fScale;
   TEXTUREPOINT atp[2];
   for (f = afInset[0]; f <= fLen - afInset[1] + CLOSE; f += fReal) {
      // scale into HV for spline
      fScale = DistIntoLengths (f, afLen, FADETAIL-1) / (fp)(FADETAIL-1);
      fScale = fScale * (&tpDelta.h)[fHorzBeams] + (&tpMin.h)[fHorzBeams];

      atp[0] = tpMin;
      atp[1] = tpMax;
      if (fHorzBeams)
         atp[0].v = atp[1].v = fScale;
      else
         atp[0].h = atp[1].h = fScale;

      if (!FramingAddWithBreaks(pss, &atp[0], pafBevel, pafExtend, dwShape, pScale, dwColor, fCenter))
         return FALSE;
   }

   return TRUE;
}


/***************************************************************************************
CDoubleSurface::FramingClear - Frees up all the noodles in the framing database.
*/
void CDoubleSurface::FramingClear (void)
{
   PFNOODLE pn = (PFNOODLE) m_lFNOODLE.Get(0);
   DWORD dwNum = m_lFNOODLE.Num();
   DWORD i;
   for (i = 0; i < dwNum; i++)
      if (pn[i].pNoodle)
         delete pn[i].pNoodle;
   m_lFNOODLE.Clear();
}

/***************************************************************************************
CDoubleSurface::FramingCalcIfDirty - Recalculates all the framing if the dirty framing
flag is set. THis will:
   1) Clear exsiting framing.
   2) Make new framing.
   3) Make sure needed colors are claimed
   4) Unclaim colors not used
*/
void CDoubleSurface::FramingCalcIfDirty (void)
{
   if (!m_fFramingDirty)
      return;
   m_fFramingDirty = FALSE;
   FramingClear();

   // because size of framing inside the wall is affected by the structure thickness,
   // make sure they dont exceed
   CPoint   apScale[2];
   apScale[0].Copy (&m_apFramingScale[0]);
   apScale[1].Copy (&m_apFramingScale[1]);
   if (!m_afFramingShow[0])
      apScale[0].p[1] = 0;
   if (!m_afFramingShow[1])
      apScale[1].p[1] = 0;
   fp fSum;
   fSum = apScale[0].p[1] + apScale[1].p[1];
   if (fSum && m_fThickStud && (fSum != m_fThickStud)) {
      apScale[0].p[1] *= m_fThickStud / fSum;
      apScale[1].p[1] *= m_fThickStud / fSum;
   }

   // create the directions and counter directions
   BOOL fHorzBeam;
   fHorzBeam = m_fFramingRotate;
   DWORD i;
   for (i = 0; i < 3; i++) {
      if (!m_afFramingShow[i])
         continue;

      PCSplineSurface pss;
      fp fCenter;
      fp fExtThick = m_afFramingExtThick[i][0] + m_afFramingExtThick[i][1];
      CPoint pScale;
      pScale.Copy ((i < 2) ? &apScale[i] : &m_apFramingScale[i]);
      pScale.p[1] += fExtThick;
      switch (i) {
         default:
         case 0:  // inside, side A
            pss = &m_SplineA;
            fCenter = pScale.p[1] / 2 + m_fThickA - m_afFramingExtThick[i][0];
            break;
         case 1:  // inside, side B
            pss = &m_SplineB;
            fCenter = pScale.p[1] / 2 + m_fThickB - m_afFramingExtThick[i][1];
            break;
         case 2:  // outside, side B
            pss = &m_SplineB;
            fCenter = -pScale.p[1] / 2 + m_fThickB + m_afFramingExtThick[i][0];
            break;
      }

      if (!FramingAdd (pss, fHorzBeam, m_afFramingInset[i], m_afFramingInsetTL[i],
         m_afFramingMinDist[i], &m_afFramingBevel[i][0], &m_afFramingExtend[i][0],
         m_adwFramingShape[i], &pScale, i, fCenter))
         return;

      // rotate framing to 90 degrees for the next one
      fHorzBeam = !fHorzBeam;
   }

   // Figure out what colors need
   BOOL fColorUsed[3];
   memset (fColorUsed, 0, sizeof(fColorUsed));
   for (i = 0; i < m_lFNOODLE.Num(); i++) {
      PFNOODLE pn = (PFNOODLE) m_lFNOODLE.Get(i);

      fColorUsed[pn->dwColor] = TRUE;
   }
   for (i = 0; i < 3; i++) {
      PCLAIMSURFACE pcs;
      pcs = ClaimFindByReason (10 + i);

      if (fColorUsed[i]) {
         if (pcs)
            continue;   // already have color

         // else, add
         ClaimSurface (10+i, FALSE, RGB(0x80,0x80,0x80), MATERIAL_PAINTSEMIGLOSS, L"Framing");
      }
      else {
         if (!pcs)
            continue;   // doint want color and dont have

         // else, remove
         ClaimRemove (pcs->dwID);
      }

   }

}



// NOT GOING TO FIX - Created a overlay of anyshape on the floor. While in walkthrough
// view tried to move the points, but found it difficult. It didn't move where
// I expected it to. - Movement works find if use NSEWUD control point move, but
// not NSEW move. This happens because doing its own intersect call for moving control
// points.

// FUTURERELEASE - eventually an option for insulation

// FUTURERELEASE - The OverlayIntersections() call unintentionally includes invisible walls
// from other building blocks as they intersect... this happens because the invisible walls
// are done by cutouts on the A and B side, but not by the edge in the center, which is
// used. NOt a terribly nice thing to happen, but it's not the end of the world because
// can do overlays by hand.


// FUTURERELEASE - Could speed up PathFromSequences() later (and hence paint create overlay)
// by eliminating any paths getting longer then the shortest path so far added.
// Didn't do now since limiting to 10 elements seems to have done a fairly good job anyway

// FUTURERELEASE - Way to show/hide side A, B, edges from building block?


