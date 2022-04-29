/**********************************************************************************
CSingleSurface - This C++ object has one CSplineSurface object, and draws a single-sided
surface. It manages everything too.

begun 29/8/2002 by Mike Rozak
Copyright 2002 Mike Rozak. All rights reserved
*/

#include <windows.h>
#include "escarpment.h"
#include "..\M3D.h"
#include "resource.h"

// SOMS - SurfaceOverMain struct
typedef struct {
   PCListFixed             plSeq;      // sequences that intersect the roof
   PCSingleSurface         pThis;      // this
   PCListFixed             plSelected; // list of selected segments
   WCHAR                   szEditing[256];   // name of overlay that editing
   DWORD                   *pdwCurColor;  // change this to change the current colored displayed
                                          // set to &m_dwDisplayControl
} SOMS, *PSOMS;


/***********************************************************************************
CSingleSurface::Constructor and destructor
*/
CSingleSurface::CSingleSurface (void)
{
   m_dwType= 0;
   m_pTemplate = NULL;
   m_MatrixToObject.Identity();
   m_MatrixFromObject.Identity();
   m_listCLAIMSURFACE.Init (sizeof(CLAIMSURFACE));

}

CSingleSurface::~CSingleSurface (void)
{
   //ClaimClear(); - BUGFIX - Don't do this here because confuses things when
   // deleting master object. Instead, have master object call if specially
   // destroying only this surface.
}


/*************************************************************************************
CSingleSurface::ClaimClear - Clear all the claimed surfaces. This ends up
gettind rid of colors information and embedding information.

  
NOTE: An object that has a CSingleSurface should call this if it's deleting
the furace but NOT itself. THis eliminates the claims for the surfaces

*/
void CSingleSurface::ClaimClear (void)
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
CSingleSurface::ClaimCloneTo - Looks at all the claims used in this object. These
are copied into the new fp surface - keeping the same IDs. Which means that can
onlyu call ClaimCloneTo when cloning to a blank destination object.


inputs
   PCSingleSurface      pCloneTo - cloning to this
   PCObjectTemplate     pTemplate - Template. THis is what really is changed.
                        New ObjectSurfaceAdd() and ContainerSurfaceAdd()
returns
   none
*/
void CSingleSurface::ClaimCloneTo (CSingleSurface *pCloneTo, PCObjectTemplate pTemplate)
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
CSingleSurface::ClaimRemove - Remove a claim on a particular ID.

inputs
   DWORD       dwID - to remove the claim on
returns
   BOOL - TRUE if success
*/
BOOL CSingleSurface::ClaimRemove (DWORD dwID)
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
CSingleSurface::ClaimFindByReason - Given a dwReason, finds the first claim with that reason.

inputs
   DWORD       dwReason
returns
   PCLAIMSURFACE - Claim using the reason, or NULL if can't find
*/
PCLAIMSURFACE CSingleSurface::ClaimFindByReason (DWORD dwReason)
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
CSingleSurface::ClaimFindByID - Given a claim ID, finds the first claim with that ID.

inputs
   DWORD       dwID
returns
   PCLAIMSURFACE - Claim using the ID, or NULL if can't find
*/
PCLAIMSURFACE CSingleSurface::ClaimFindByID (DWORD dwID)
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
CSingleSurface::ClaimSurface - Loops through all the texture surfaces and finds one
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
DWORD CSingleSurface::ClaimSurface (DWORD dwReason, BOOL fAlsoEmbed, COLORREF cColor, DWORD dwMaterialID, PWSTR pszScheme,
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
CSingleSurface::InitButDontCreate - Initializes the object half way, but doesn't
actually create surfaces - which is ultimately causing problems in CObjectBuildBlock.
Basically, this just remembers the template and dwType and that's it.
*/
BOOL CSingleSurface::InitButDontCreate (DWORD dwType, PCObjectTemplate pTemplate)
{
   m_dwType = dwType;
   m_pTemplate = pTemplate;
   return TRUE;
}

/*************************************************************************************
CSingleSurface::Init - Initialize

inputs
   DWORD       dwType - Type of surface.
      0 = rectangle/plane
      1 = sphere
      2 = tube
      3 = taurus


   PCObjectTemplate     pTemplate - Used for information like getting surfaces available.

returns
   BOOL - TRUE if success. FALSE if failure
*/
BOOL CSingleSurface::Init (DWORD dwType, PCObjectTemplate pTemplate)
{
   InitButDontCreate (dwType, pTemplate);

   DWORD x, y;
   fp fScaleX, fScaleZ;

#define  CURVEDETAIL       8
#define  SPHERELAT         (6+1)

   switch (dwType) {
   case 0:  // rectangular plane
   default:
      {
         CPoint   apPoints[2][2];

         fScaleX = 1;
         fScaleZ = 1;

         for (y = 0; y < 2; y++) for (x = 0; x < 2; x++) {
            apPoints[y][x].Zero();
            apPoints[y][x].p[0] = ((fp)x - .5) * fScaleX;
            apPoints[y][x].p[2] = (1.0 - (fp)y) * fScaleZ;
         }

         m_Spline.ControlPointsSet(
            FALSE, FALSE,
            2, 2, &apPoints[0][0],
            (DWORD*) SEGCURVE_LINEAR, (DWORD*) SEGCURVE_LINEAR);
         m_Spline.DetailSet (1,2,1,2, .2);
      }
      break;

   case 1:  // sphere
      {
         CPoint   apPoints[SPHERELAT][CURVEDETAIL];

         for (y = 0; y < SPHERELAT; y++) for (x = 0; x < CURVEDETAIL; x++) {
            fp fRadI, fRadK;
            fRadI = -(fp)x / (fp) CURVEDETAIL * 2.0 * PI;
            fRadK = -((fp) y / (fp) (SPHERELAT-1) - .5) * PI * (1.0 - CLOSE);

            apPoints[y][x].Zero();
            apPoints[y][x].p[0] = cos(fRadI) * cos(fRadK) * 0.5;
            apPoints[y][x].p[2] = sin(fRadK) * 0.5;
            apPoints[y][x].p[1] = -sin(fRadI) * cos(fRadK) * 0.5;
         }

         m_Spline.ControlPointsSet(
            TRUE, FALSE,
            CURVEDETAIL, SPHERELAT, &apPoints[0][0],
            (DWORD*) SEGCURVE_CUBIC, (DWORD*) SEGCURVE_CUBIC);
         m_Spline.DetailSet (1,3,1,3, .2);
      }
      break;

   case 2:  // tube
      {
         CPoint   apPoints[2][CURVEDETAIL];

         for (y = 0; y < 2; y++) for (x = 0; x < CURVEDETAIL; x++) {
            fp fRadI;
            fRadI = -(fp)x / (fp) CURVEDETAIL * 2.0 * PI;

            apPoints[y][x].Zero();
            apPoints[y][x].p[0] = cos(fRadI) * 0.5;
            apPoints[y][x].p[1] = -sin(fRadI) * 0.5;
            apPoints[y][x].p[2] = (1.0 - (fp)y) * 0.5;
         }

         m_Spline.ControlPointsSet(
            TRUE, FALSE,
            CURVEDETAIL, 2, &apPoints[0][0],
            (DWORD*) SEGCURVE_CUBIC, (DWORD*) SEGCURVE_CUBIC);
         m_Spline.DetailSet (1,3,1,3, .2);
      }
      break;

   case 3:  // taurus
      {
         CPoint   apPoints[CURVEDETAIL][CURVEDETAIL];

         for (y = 0; y < CURVEDETAIL; y++) for (x = 0; x < CURVEDETAIL; x++) {
            fp fRadI, fRadK;
            fRadI = -(fp)x / (fp) CURVEDETAIL * 2.0 * PI;
            fRadK = -((fp) y / (fp) CURVEDETAIL - .5) * PI*2;  // over the ring

            apPoints[y][x].Zero();
            apPoints[y][x].p[0] = cos(fRadI) * (1.0 + cos(fRadK)/2.0) * 0.5;
            apPoints[y][x].p[2] = (sin(fRadK)/2.0) * 0.5;
            apPoints[y][x].p[1] = -sin(fRadI) * (1.0 + cos(fRadK)/2.0) * 0.5;
         }

         m_Spline.ControlPointsSet(
            TRUE, TRUE,
            CURVEDETAIL, CURVEDETAIL, &apPoints[0][0],
            (DWORD*) SEGCURVE_CUBIC, (DWORD*) SEGCURVE_CUBIC);
         m_Spline.DetailSet (1,3,1,3, .2);
      }
      break;

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

   ClaimSurface (0, TRUE, RGB(0x80,0x80,0xff), MATERIAL_PAINTSEMIGLOSS);

   return TRUE;
}

/**********************************************************************************
CSingleSurface::Render - Draws the surface

inputs
   POBJECTRENDER     pr - Render information
   PCRenderSurface   prs - Render surface. This is used for drawing, so should
         have any previous rotations already done. Push() and Pop() are used
         to insure the main matrix isn't affected
*/
void CSingleSurface::Render (DWORD dwRenderShard, POBJECTRENDER pr, PCRenderSurface prs)
{
   // might not want to draw this based on the type
   switch (HIWORD(m_dwType)) {
   case 1:  // wall
      if (!(pr->dwShow & RENDERSHOW_WALLS))
         return;
      break;
   case 2:  // roof/ceiling
      if (!(pr->dwShow & RENDERSHOW_ROOFS))
         return;
      break;
   case 3:  // floor/ceiling
      if (!(pr->dwShow & RENDERSHOW_FLOORS))
         return;
      break;
   }

   prs->Push();
   prs->Multiply (&m_MatrixToObject);

   DWORD dwSideA;
   PCLAIMSURFACE pcs;
   pcs = ClaimFindByReason(0);
   dwSideA = pcs ? pcs->dwID : 0;

   // ok to to backface culling on the sides if not hiding edges
   BOOL fBack;
   fBack = FALSE;

   // BUGFIX - Because the object editor doesn't know what surfaces are needed
   // until they're requested, need to request them even with a bounding
   // box. Otherwise, surface modification won't work in the MIFLServer CRenderScene
   // draw side A
   prs->SetDefMaterial (dwRenderShard, m_pTemplate->ObjectSurfaceFind (dwSideA), m_pTemplate->m_pWorld);
   if (pr->dwReason == ORREASON_BOUNDINGBOX)
      m_Spline.Render (prs, fBack, FALSE);
   else
      m_Spline.Render (prs, fBack, TRUE, 1);

   // loop through the overlays
   DWORD i;
   PWSTR pszName;
   BOOL fClockwise;
   PTEXTUREPOINT ptp;
   DWORD dwNum;
   for (i = 0; i < m_Spline.OverlayNum(); i++) {
      if (!m_Spline.OverlayEnum (i, &pszName, &ptp, &dwNum, &fClockwise))
         continue;

      // find the ID
      PWSTR psz;
      DWORD dwID;
      for (psz = pszName; psz[0] && (psz[0] != L':'); psz++);
      if (psz[0]) {
         dwID = (DWORD)_wtoi(psz+1);
         prs->SetDefMaterial (dwRenderShard, m_pTemplate->ObjectSurfaceFind (dwID), m_pTemplate->m_pWorld);

         // BUGFIX - Only do this if not looking for bounding box
         if (pr->dwReason != ORREASON_BOUNDINGBOX)
            m_Spline.Render (prs, fBack, TRUE, i+2);
      }
   } // i


   prs->Pop();
}


/**********************************************************************************
CSingleSurface::QueryBoundingBox - Standard API
*/
void CSingleSurface::QueryBoundingBox (PCPoint pCorner1, PCPoint pCorner2)
{
   m_Spline.QueryBoundingBox (pCorner1, pCorner2);
}


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
PCSplineSurface CSingleSurface::OverlayShapeFindByColor (DWORD dwColor, PWSTR *ppszName, PTEXTUREPOINT *pptp,
                                         DWORD *pdwNum, BOOL *pfClockwise)
{
   DWORD i;
   for (i = 0; i < m_listCLAIMSURFACE.Num(); i++) {
      PCLAIMSURFACE p = (PCLAIMSURFACE) m_listCLAIMSURFACE.Get(i);
      if (p->dwID == dwColor) {
         PCSplineSurface pss;

         if ((p->dwReason == 0) || (p->dwReason == 3))
            pss = &m_Spline;
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
CSingleSurface::Clone - Clones this

NOTE: This does not actually clone the claimed colors, etc.

inputs
   PCSingleSurface      pNew - Overwrite this one's information.
   PCObjecTemplate      pTemplate - New template.
returns
   PCSingleSurface - Clone
*/
void CSingleSurface::CloneTo (CSingleSurface *pNew, PCObjectTemplate pTemplate)
{
   // Clone member variables
   m_Spline.CloneTo (&pNew->m_Spline);
   m_SplineEdge.CloneTo (&pNew->m_SplineEdge);

   pNew->m_dwType = m_dwType;
   pNew->m_MatrixFromObject.Copy (&m_MatrixFromObject);
   pNew->m_MatrixToObject.Copy (&m_MatrixToObject);
   pNew->m_pTemplate = pTemplate;

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

PCMMLNode2 CSingleSurface::MMLTo (void)
{
   PCMMLNode2 pNode;
   pNode = new CMMLNode2;
   if (!pNode)
      return NULL;
   pNode->NameSet (L"SingleSurface");

   // member variables go here
   PCMMLNode2 pSub;
   pSub = m_Spline.MMLTo ();
   if (pSub) {
      pSub->NameSet (L"SplineCenter");
      pNode->ContentAdd (pSub);
   }
   pSub = m_SplineEdge.MMLTo ();
   if (pSub) {
      pSub->NameSet (L"Edge");
      pNode->ContentAdd (pSub);
   }


   MMLValueSet (pNode, gpszType, (int)m_dwType);
   MMLValueSet (pNode, gpszMatrixToObject, &m_MatrixToObject);
   MMLValueSet (pNode, gpszMatrixFromObject, &m_MatrixFromObject);

   DWORD i;
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


   return pNode;
}

BOOL CSingleSurface::MMLFrom (PCMMLNode2 pNode)
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
         m_Spline.MMLFrom (pSub);
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

   m_dwType = (DWORD) MMLValueGetInt (pNode, gpszType, 0);
   MMLValueGetMatrix (pNode, gpszMatrixToObject, &m_MatrixToObject, NULL);
   MMLValueGetMatrix (pNode, gpszMatrixFromObject, &m_MatrixFromObject, NULL);


   return TRUE;
}


/*******************************************************************************
CSingleSurface::ControlNumGet - Returns the number of control points across or down.

inputs
   BOOL        fH - If TRUE then in H(across) direction, else V(down) direction
returns
   DWORD number
*/
DWORD CSingleSurface::ControlNumGet (BOOL fH)
{
   return m_Spline.ControlNumGet (fH);
}

/*******************************************************************************
CSingleSurface::ControlPointGet - Given an H and V index into the control
pointers (see ContorlNumGet()), fills in a point with the value of the control
point.

inputs
   DWORD       dwH, dwV - Index from 0..# pooints across - 1
   PCPoint     pVal - Filled in with the point. NOTE: This has been rotated to object space
returns
   BOOL - TRUE if succeded
*/
BOOL CSingleSurface::ControlPointGet (DWORD dwH, DWORD dwV, PCPoint pVal)
{
   PCSplineSurface pss = &m_Spline;
   if (!pss) {
      pVal->Zero();
      return FALSE;
   }

   if (!pss->ControlPointGet (dwH, dwV, pVal))
      return FALSE;
   pVal->MultiplyLeft (&m_MatrixToObject);

   return TRUE;
}

/*******************************************************************************
CSingleSurface::LoopGet - Returns whether the direction is looped.

inputs
   BOOL        fH - TRUE if it's the horizontal direction, FALSE if vertical
returns
   BOOL - TRUE if looped
*/
BOOL CSingleSurface::LoopGet (BOOL fH)
{
   return m_Spline.LoopGet(fH);
}

/*******************************************************************************
CSingleSurface::LoopSet - Changes the looping of either the horizontal or
vertical directions

inputs
   BOOL        fH - TRUE if it's the horizontal direction, FALSE if vertical
   BOOL        fLoop - New Loop status. TRUE if looped.
returns
   BOOL - TRUE if looped
*/
BOOL CSingleSurface::LoopSet (BOOL fH, BOOL fLoop)
{
   if (fLoop == m_Spline.LoopGet(fH))
      return TRUE;

   // else, change loop
   m_Spline.LoopSet (fH, fLoop);

   // need to recalculate some stuff
   RecalcSides();

   return TRUE;
}



/*********************************************************************************
CSingleSurface::EdgeOrigNumPointsGet - Returns the number of points originally passed into the
spline.
*/
DWORD CSingleSurface::EdgeOrigNumPointsGet (void)
{
   return m_SplineEdge.OrigNumPointsGet();
}



/*********************************************************************************
CSingleSurface::EdgeOrigPointGet - Fills in pP with the original point.

NOTE: Coordinates are not transformed because they are in HV.

inputs
   DWORD       dwH - from 0 to OrigNumPointsGet()-1
   PCPoint     pP - Filled with the original point
returns
   BOOL - TRUE if successful
*/
BOOL CSingleSurface::EdgeOrigPointGet (DWORD dwH, PCPoint pP)
{
   return m_SplineEdge.OrigPointGet (dwH, pP);
}

/***********************************************************************************
CSingleSurface::HVToInfo - Given an H and V, this will fill in the position,
normal, and texture.

inputs
   fp      h, v - Location
   PCPoint     pPoint - Filled in with the point. Can be NULL.
   PCPoint     pNorm - Filled in with the normal. Can be NULL.
   PTEXTUREPOINT pText - Filled in with the texture. Can be NULL.
rturns
   BOOL - TRUE if point h,v inside. FALSE if not
*/
BOOL CSingleSurface::HVToInfo (fp h, fp v, PCPoint pPoint, PCPoint pNorm, PTEXTUREPOINT pText)
{
   PCSplineSurface pss = &m_Spline;
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
CSingleSurface::ControlPointQuery - Called to query the information about a control
point identified by dwID.

inputs
   DWORD       dwColor - Control point color/surface. group
   DWORD       dwID - ID of control point
   POSCONTROL  pInfo - Filled in with the infomration
returns
   BOOL - TRUE if successful
*/
BOOL CSingleSurface::ControlPointQuery (DWORD dwColor, DWORD dwID, POSCONTROL pInfo)
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
   //pInfo->dwFreedom = 0;   // any direction
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
CSingleSurface::ControlPointSet - Given an H and V index into the control
pointers (see ContorlNumGet()), changes the control point to something new.

inputs
   DWORD       dwH, dwV - Index from 0..# pooints across - 1
   PCPoint     pVal - Use this point now
   DWORD       dwDivide - Number of times to divide
returns
   BOOL - TRUE if succeded
*/
BOOL CSingleSurface::ControlPointSet (DWORD dwH, DWORD dwV, PCPoint pVal)
{
   PCSplineSurface pss = &m_Spline;
   if (!pss)
      return FALSE;

   CPoint p;
   p.Copy (pVal);
   p.MultiplyLeft (&m_MatrixFromObject);

   if (!pss->ControlPointSet (dwH, dwV, &p))
      return FALSE;

   // modify the other sides
   ControlPointChanged (dwH, dwV);
   return TRUE;
}




/***************************************************************************************
CSingleSurface::HVDrag - Utility function used to help dragging of control points
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
   PTEXTUREPOINT        pTextOld - Old H and 
   PCPoint              pNew - New Point, in object space. (Spline space actually)
   PCPoint              pViewer - Viewer, in object space. (Spline space actually)
   PTEXTUREPOINT        pTextNew - Filled with the new texture point if successful
returns
   BOOL - TRUE if success, FALSE if cant find intersect
*/
BOOL CSingleSurface::HVDrag (PTEXTUREPOINT pTextOld, PCPoint pNew, PCPoint pViewer, PTEXTUREPOINT pTextNew)
{
   PCSplineSurface pss = &m_Spline;
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
CSingleSurface::EdgeOrigSegCurveGet - Returns the segcurve descriptor... SEGCURVE_XXX, for
the index into the original points.

inputs
   DWORD       dwH - From 0 to OrigNumPointsGet()-1. an extra -1 if not looped
   DWORD       *padwSegCurve - Filled in with the segment curve
returns
   BOOL - TRUE if successful
*/
BOOL CSingleSurface::EdgeOrigSegCurveGet (DWORD dwH, DWORD *pdwSegCurve)
{
   return m_SplineEdge.OrigSegCurveGet (dwH, pdwSegCurve);
}


/*********************************************************************************
CSingleSurface::EdgeDivideGet - Fills in the divide information

inputs
   DWORD*      pdwMinDivide, pdwMaxDivide - If not null, filled in with min/max divide.
   fp*     pfDetail - If not null, filled in with detail
*/
void CSingleSurface::EdgeDivideGet (DWORD *pdwMinDivide, DWORD *pdwMaxDivide, fp *pfDetail)
{
   m_SplineEdge.DivideGet (pdwMinDivide, pdwMaxDivide, pfDetail);
}


/***********************************************************************************
CSingleSurface::EdgeInit - Initializes the spline.

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
BOOL CSingleSurface::EdgeInit (BOOL fLooped, DWORD dwPoints, PCPoint paPoints, PTEXTUREPOINT paTextures,
      DWORD *padwSegCurve, DWORD dwMinDivide, DWORD dwMaxDivide,
      fp fDetail)
{
   if (!m_SplineEdge.Init (fLooped, dwPoints, paPoints, paTextures, padwSegCurve,
      dwMinDivide, dwMaxDivide, fDetail))
      return FALSE;

   RecalcEdges ();

   return TRUE;
}
/*************************************************************************************
CSingleSurface::ControlPointSet - Called to change the valud of a control point.

inputs
   DWORD       dwColor - Current surface being viewed
   DWORD       dwID - ID
   PCPoint     pVal - Contains the new location, in object coordinates
returns
   BOOL - TRUE if successful
*/
BOOL CSingleSurface::ControlPointSet (DWORD dwColor, DWORD dwID, PCPoint pVal, PCPoint pViewer)
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
   // BUGFIX - Was doing something wierd to get tpOld, and wasnt right
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

   // tell the world we've changed
   if (m_pTemplate->m_pWorld)
      m_pTemplate->m_pWorld->ObjectChanged (m_pTemplate);

   return TRUE;
}


/*************************************************************************************
CSingleSurface::ControlPointEnum - Called to enumerate a list of control point IDs
into the list.

inputs
   DWORD             dwColor - Color surface ID
   PCListFixed       plDWORD - Should be filled in with control points
returns
   none
*/
void CSingleSurface::ControlPointEnum (DWORD dwColor, PCListFixed plDWORD)
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
CSingleSurface::ControlPointChanged - Call this when a control point has changed
in either side A, B, or the center. It's assumed that the change has already been
written into the side that was changed. This change is then translated into the
center spline. From there, both sides (A and B) are recalculated.

inputs
   DWORD          dwX, dwY - Control point location. This should already have been changed
                     in the given spline
returns
   BOOL - TRUE if success
*/
BOOL CSingleSurface::ControlPointChanged (DWORD dwX, DWORD dwY)
{
   // NOTE: Now that got rid of both sides not doing much here.

   // recalc both sides
   return RecalcSides ();
}


/*************************************************************************************
CSingleSurface::RecalcSides - Recalculate side A and B from the center spline.

IMPORTANT: This does NOT call the world to tell it the object is or has changed.

inputs
   none
returns
   BOOL - TRUE if success
*/
BOOL CSingleSurface::RecalcSides (void)
{

   // NOTE: Not doing much here that have removed side a and b

   // tell the embedded objects that they've moved
   TellEmbeddedThatMoved();

   return TRUE;
}

/*************************************************************************************
CSingleSurface::TellEmbeddedThatMoved - Calls all the embedded objects within this
and tells the embedded objects that they've been moved somehow.

inputs
   none
returns
   none
*/
void CSingleSurface::TellEmbeddedThatMoved (void)
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
CSingleSurface::RecalcEdges - Recalculate edge cutouts side A and B from the edge spline.

IMPORTANT: This does NOT call the world to tell it the object is or has changed.

inputs
   none
returns
   BOOL - TRUE if success
*/
BOOL CSingleSurface::RecalcEdges (void)
{
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
   m_Spline.CutoutSet (L"Edge", pt, dwNum, FALSE);

   return TRUE;
}


/*******************************************************************************
CSingleSurface::DetailGet - Gets the current detail vlaues.

inputs
   DWORD*      pdwMin/MaxDivideH/V - Filled in the value. Can be null
   fp*     pfDetail - Filled in with detail. Can be null
returns
   non
*/
void CSingleSurface::DetailGet (DWORD *pdwMinDivideH, DWORD *pdwMaxDivideH, DWORD *pdwMinDivideV, DWORD *pdwMaxDivideV,
   fp *pfDetail)
{
   m_Spline.DetailGet (pdwMinDivideH, pdwMaxDivideH, pdwMinDivideV, pdwMaxDivideV, pfDetail);
}



/*************************************************************************************
CSingleSurface::DetailSet - Sets the detail information for drawing the surface.

inputs
   DWORD          dwMinDivideH - Minimum number of divides that will do horizontal
   DWORD          dwMaxDivideH - Maximum number of divides that will do horizontal
   DWORD          dwMinDivideV - Minimum number of divides that will do vertical
   DWORD          dwMaxDivideV - Maximum number of divides that will do vertical
   fp         fDetail - Divide until every spline distance is less than this
returns
   BOOL - TRUE if succeded
*/
BOOL CSingleSurface::DetailSet (DWORD dwMinDivideH, DWORD dwMaxDivideH, DWORD dwMinDivideV, DWORD dwMaxDivideV,
   fp fDetail)
{
   if (!m_Spline.DetailSet (dwMinDivideH, dwMaxDivideH, dwMinDivideV, dwMaxDivideV, fDetail))
      return FALSE;
   RecalcSides();
   return TRUE;
}


/*******************************************************************************
CSingleSurface::SegCurveGet - Gets the curvature of the segment at the given index.

inputs
   BOOL        fH - If TRUE use the horizontal segment, FALSE use vertical
   DWORD       dwIndex - Index into segment. From 0 .. ControlNumGet()-1.
returns
   DWORD - Curve. 0 if failed.
*/
DWORD CSingleSurface::SegCurveGet (BOOL fH, DWORD dwIndex)
{
   return m_Spline.SegCurveGet (fH, dwIndex);
}



/*******************************************************************************
CSingleSurface::SegCurveSet - Sets the curvature of the segment at the given index.

inputs
   BOOL        fH - If TRUE use the horizontal segment, FALSE use vertical
   DWORD       dwIndex - Index into segment. From 0 .. ControlNumGet()-1.
   DWORD       dwValue - new value
returns
   BOOL - TRUE if success
*/
BOOL CSingleSurface::SegCurveSet (BOOL fH, DWORD dwIndex, DWORD dwValue)
{
   if (!m_Spline.SegCurveSet (fH, dwIndex, dwValue))
      return FALSE;

   RecalcSides();
   return TRUE;
}



/* SingleSurfaceDetailPage
*/
BOOL SingleSurfaceDetailPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCSingleSurface pv = (PCSingleSurface)pPage->m_pUserData;

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


/* SingleSurfaceCurvaturePage
*/
BOOL SingleSurfaceCurvaturePage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCSingleSurface pv = (PCSingleSurface)pPage->m_pUserData;


   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         PCEscControl pControl;
         DWORD i;
         WCHAR szTemp[16];
         DWORD dwNum;

         // set the comboboxes to their current values
         dwNum = pv->ControlNumGet (FALSE) - (pv->LoopGet(FALSE) ? 0 : 1);  // row
         for (i = 0; i < dwNum; i++) {
            swprintf (szTemp, L"xr%d", (int)i);
            pControl = pPage->ControlFind (szTemp);
            if (pControl)
               pControl->AttribSetInt (gszCurSel, (int) pv->SegCurveGet (FALSE, i));
         }
         dwNum = pv->ControlNumGet (TRUE) - (pv->LoopGet(TRUE) ? 0 : 1);  // row
         for (i = 0; i < dwNum; i++) {
            swprintf (szTemp, L"xc%d", (int)i);
            pControl = pPage->ControlFind (szTemp);
            if (pControl)
               pControl->AttribSetInt (gszCurSel, (int) pv->SegCurveGet (TRUE, i));
         }

         // check looped boxes
         pControl = pPage->ControlFind (L"looph");
         if (pControl)
            pControl->AttribSetBOOL (gszChecked, pv->LoopGet(TRUE));
         pControl = pPage->ControlFind (L"loopv");
         if (pControl)
            pControl->AttribSetBOOL (gszChecked, pv->LoopGet(FALSE));

      }
      break;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;
         PWSTR psz;
         psz = p->pControl->m_pszName;

         if (!_wcsicmp(psz, L"looph")) {
            pv->m_pTemplate->m_pWorld->ObjectAboutToChange (pv->m_pTemplate);

            BOOL fOrig, fNew;
            fOrig = pv->LoopGet (TRUE);
            fNew = p->pControl->AttribGetBOOL(gszChecked);

            pv->LoopSet (TRUE, fNew);

            // Will need to adjust embedded object locations
            fp afOrig[2], afNew[2];
            DWORD dwNum;
            dwNum = pv->ControlNumGet(TRUE);
            afOrig[0] = afOrig[1] = afNew[0] = afNew[1] = 1.0;
            if (fOrig && !fNew) {   // original looped, but new one not
               afOrig[0] = (fp)(dwNum-1) / (fp)dwNum;
               pv->ChangedHV (2, 0, afOrig, NULL, afNew, NULL);
            }
            else if (!fOrig && fNew) { // new one looped but original not
               afNew[0] = (fp)(dwNum-1) / (fp)dwNum;
               pv->ChangedHV (2, 0, afOrig, NULL, afNew, NULL);
            }

            // get the embedded to recalc
            pv->TellEmbeddedThatMoved();

            pv->m_pTemplate->m_pWorld->ObjectChanged (pv->m_pTemplate);

            pPage->Exit(RedoSamePage());
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"loopv")) {
            pv->m_pTemplate->m_pWorld->ObjectAboutToChange (pv->m_pTemplate);

            BOOL fOrig, fNew;
            fOrig = pv->LoopGet (FALSE);
            fNew = p->pControl->AttribGetBOOL(gszChecked);

            pv->LoopSet (FALSE, fNew);

            // Will need to adjust embedded object locations
            fp afOrig[2], afNew[2];
            DWORD dwNum;
            dwNum = pv->ControlNumGet(FALSE);
            afOrig[0] = afOrig[1] = afNew[0] = afNew[1] = 1.0;
            if (fOrig && !fNew) {   // original looped, but new one not
               afOrig[0] = (fp)(dwNum-1) / (fp)dwNum;
               pv->ChangedHV (0, 2, NULL, afOrig, NULL, afNew);
            }
            else if (!fOrig && fNew) { // new one looped but original not
               afNew[0] = (fp)(dwNum-1) / (fp)dwNum;
               pv->ChangedHV (0, 2, NULL, afOrig, NULL, afNew);
            }

            // get the embedded to recalc
            pv->TellEmbeddedThatMoved();

            pv->m_pTemplate->m_pWorld->ObjectChanged (pv->m_pTemplate);

            pPage->Exit(RedoSamePage());
            return TRUE;
         }
      }
      break;


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
            DWORD dwNum = pv->ControlNumGet(!fRow) - (pv->LoopGet(!fRow) ? 0 : 1);
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




/* SingleSurfaceControlPointsPage
*/
BOOL SingleSurfaceControlPointsPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCSingleSurface pv = (PCSingleSurface)pPage->m_pUserData;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         // draw images
         DSGenerateThreeDFromSplineSurface (L"row", pPage, &pv->m_Spline, 1);
         DSGenerateThreeDFromSplineSurface (L"column", pPage, &pv->m_Spline, 0);
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
         if (!memSegCurveH.Required (dwWidth * sizeof(DWORD)))
            return FALSE;
         if (!memSegCurveV.Required (dwHeight * sizeof(DWORD)))
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
                        pv->m_Spline.ControlPointGet ((dwOrigX % pv->ControlNumGet(TRUE)), dwOrigY, &pTemp);
                        pv->m_Spline.ControlPointGet (dwOrigX-1, dwOrigY, &pTemp2);
                        pTemp.Add (&pTemp2);
                        pTemp.Scale (.5);

                        // VERSION2 - probably want to do actual spline calc
                     }
                     else {
                        // just get this value
                        if (dwOrigX > x)
                           dwOrigX--;

                        pv->m_Spline.ControlPointGet (dwOrigX, dwOrigY, &pTemp);
                     }
                  }
                  else {
                     if (j == y+1) {
                        // do an average of the points
                        pv->m_Spline.ControlPointGet (dwOrigX, (dwOrigY % pv->ControlNumGet(FALSE)), &pTemp);
                        pv->m_Spline.ControlPointGet (dwOrigX, dwOrigY-1, &pTemp2);
                        pTemp.Add (&pTemp2);
                        pTemp.Scale (.5);

                        // VERSION2 - probably want to do actual spline calc
                     }
                     else {
                        // just get this value
                        if (dwOrigY > y)
                           dwOrigY--;

                        pv->m_Spline.ControlPointGet (dwOrigX, dwOrigY, &pTemp);
                     }
                  }

                  // write out pTemp
                  pPoints[j * dwWidth + i].Copy (&pTemp);
               }

               // do the curvature
               for (i = 0; i < dwWidth; i++) {   // BUGFIX - Removed -1 so work if loop
                  if (fCol && (i > x))
                     padwSegCurveH[i] = pv->m_Spline.SegCurveGet (TRUE, i-1);
                  else
                     padwSegCurveH[i] = pv->m_Spline.SegCurveGet (TRUE, i);
               }
               for (i = 0; i < dwHeight; i++) {  // BUGFIX - Removed -1 so work if loop
                  if (!fCol && (i > y))
                     padwSegCurveV[i] = pv->m_Spline.SegCurveGet (FALSE, i-1);
                  else
                     padwSegCurveV[i] = pv->m_Spline.SegCurveGet (FALSE, i);
               }

               // Call API to readjust all stored H & V values
               fp afOrig[2], afNew[2];
               DWORD dwWidthOrig, dwWidthNew, dwHeightOrig, dwHeightNew;
               dwWidthOrig = dwWidth-1-(pv->LoopGet(TRUE) ? 0 : 1);
               dwWidthNew = dwWidthOrig+1;
               dwHeightOrig = dwHeight-1-(pv->LoopGet(FALSE) ? 0 : 1);
               dwHeightNew = dwHeightOrig+1;
               if (fCol) {
                  afOrig[0] = (fp)x / (fp)dwWidthOrig;
                  afOrig[1] = (fp)(x+1) / (fp)dwWidthOrig;
                  afNew[0] = (fp)x / (fp)dwWidthNew;
                  afNew[1] = (fp)(x+2) / (fp)dwWidthNew;
                  pv->ChangedHV (2, 0, afOrig, NULL, afNew, NULL);
               }
               else {
                  afOrig[0] = (fp)y / (fp)dwHeightOrig;
                  afOrig[1] = (fp)(y+1) / (fp)dwHeightOrig;
                  afNew[0] = (fp)y / (fp)dwHeightNew;
                  afNew[1] = (fp)(y+2) / (fp)dwHeightNew;
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

                     pv->m_Spline.ControlPointGet (dwOrigX, dwOrigY, &pTemp);
                  }
                  else {
                     // just get this value
                     if (dwOrigY >= y)
                        dwOrigY++;

                     pv->m_Spline.ControlPointGet (dwOrigX, dwOrigY, &pTemp);
                  }

                  // write out pTemp
                  pPoints[j * dwWidth + i].Copy (&pTemp);
               }

               // do the curvature
               for (i = 0; i < dwWidth; i++) {   // BUGFIX - Get rid of -1 so deals with loops
                  if (fCol && (i >= x))
                     padwSegCurveH[i] = pv->m_Spline.SegCurveGet (TRUE, i+1);
                  else
                     padwSegCurveH[i] = pv->m_Spline.SegCurveGet (TRUE, i);
               }
               for (i = 0; i < dwHeight; i++) {  // BUGFIX - Get rid of -1 so deals with loops
                  if (!fCol && (i >= y))
                     padwSegCurveV[i] = pv->m_Spline.SegCurveGet (FALSE, i+1);
                  else
                     padwSegCurveV[i] = pv->m_Spline.SegCurveGet (FALSE, i);
               }

               // Call API to readjust all stored H & V values
               // IMPORTANT: There's a fundamental flaw with this... if remove
               // a column/row that's evenly divided it works great. However, if there's
               // an uneven divide it has no way of properly handling the change.
               // The only real way is the remember all the previous points in object space,
               // divide, and find their closest point in the new surfaces HV space
               // once the divide has occurred
               fp afOrig[2], afNew[2];
               DWORD dwWidthOrig, dwWidthNew, dwHeightOrig, dwHeightNew;
               // BUGFIX - Added 1 to dwWIdth orig because of error introduced
               // when aded loopget
               dwWidthOrig = dwWidth+1-(pv->LoopGet(TRUE) ? 0 : 1);
               dwWidthNew = dwWidthOrig-1;
               dwHeightOrig = dwHeight+1-(pv->LoopGet(FALSE) ? 0 : 1);
               dwHeightNew = dwHeightOrig-1;
               if (fCol) {
                  afOrig[0] = (fp)(x-1) / (fp)dwWidthOrig;
                  afOrig[1] = (fp)(x+1) / (fp)dwWidthOrig;
                  afNew[0] = (fp)(x-1) / (fp)dwWidthNew;
                  afNew[1] = (fp)x / (fp)dwWidthNew;
                  pv->ChangedHV (2, 0, afOrig, NULL, afNew, NULL);
               }
               else {
                  afOrig[0] = (fp)(y-1) / (fp)dwHeightOrig;
                  afOrig[1] = (fp)(y+1) / (fp)dwHeightOrig;
                  afNew[0] = (fp)(y-1) / (fp)dwHeightNew;
                  afNew[1] = (fp)y / (fp)dwHeightNew;
                  pv->ChangedHV (0, 2, NULL, afOrig, NULL, afNew);
               }

//               if (fCol) {
//                  afOrig[0] = (fp)(x-1) / (fp)(dwWidth+0);
//                  afOrig[1] = (fp)(x+1) / (fp)(dwWidth+0);
//                  afNew[0] = (fp)(x-1) / (fp)(dwWidth-1);
//                  afNew[1] = (fp)x / (fp)(dwWidth-1);
//                  pv->ChangedHV (2, 0, afOrig, NULL, afNew, 0);
//               }
//               else {
//                  afOrig[0] = (fp)(y-1) / (fp)(dwHeight+0);
//                  afOrig[1] = (fp)(y+1) / (fp)(dwHeight+0);
//                  afNew[0] = (fp)(y-1) / (fp)(dwHeight-1);
//                  afNew[1] = (fp)y / (fp)(dwHeight-1);
//                  pv->ChangedHV (0, 2, NULL, afOrig, NULL, afNew);
//               }
            }
            break;
         }

         // set new value
         BOOL fLoopH, fLoopV;
         fLoopH = pv->m_Spline.LoopGet(TRUE);
         fLoopV = pv->m_Spline.LoopGet(FALSE);
         pv->m_Spline.ControlPointsSet (fLoopH, fLoopV, dwWidth, dwHeight,
            pPoints, padwSegCurveH, padwSegCurveV);

         // changed
         pv->RecalcSides();
         pv->m_pTemplate->m_pWorld->ObjectChanged (pv->m_pTemplate);

         // redraw the shapes
         DSGenerateThreeDFromSplineSurface (L"row", pPage, &pv->m_Spline, 1);
         DSGenerateThreeDFromSplineSurface (L"column", pPage, &pv->m_Spline, 0);
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



/* SingleSurfaceEdgePage
*/
BOOL SingleSurfaceEdgePage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCSingleSurface pv = (PCSingleSurface)pPage->m_pUserData;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         // draw images
         DSGenerateThreeDFromSpline (L"edgeaddremove", pPage, &pv->m_SplineEdge, &pv->m_Spline, 0);
         DSGenerateThreeDFromSpline (L"edgecurve", pPage, &pv->m_SplineEdge, &pv->m_Spline, 1);
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
         // BUGFIX - Always use divide of 2, since detail less relevent for these
         pv->m_SplineEdge.Init (TRUE, dwOrig, paPoints, NULL, padw, 2, 2, fDetail);
         pv->RecalcEdges ();
         pv->m_pTemplate->m_pWorld->ObjectChanged (pv->m_pTemplate);

         // redraw the shapes
         DSGenerateThreeDFromSpline (L"edgeaddremove", pPage, &pv->m_SplineEdge, &pv->m_Spline, 0);
         DSGenerateThreeDFromSpline (L"edgecurve", pPage, &pv->m_SplineEdge, &pv->m_Spline, 1);
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
            DSGenerateThreeDFromSpline (L"edgeaddremove", pPage, &pv->m_SplineEdge, &pv->m_Spline, 0);
            DSGenerateThreeDFromSpline (L"edgecurve", pPage, &pv->m_SplineEdge, &pv->m_Spline, 1);
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




/* SingleSurfaceOverMainPage
*/
BOOL SingleSurfaceOverMainPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PSOMS ps = (PSOMS)pPage->m_pUserData;
   static BOOL fMode = 0;  // click and go to edit. If 1 then click and delete

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         fMode = 0;
         DSGenerateThreeDFromOverlays (L"overlayedit", pPage,
            &ps->pThis->m_Spline,
            TRUE);
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
         pss = &ps->pThis->m_Spline;
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
               &ps->pThis->m_Spline,
               TRUE);
          
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


/* SingleSurfaceOverEditEdgePage
*/
BOOL SingleSurfaceOverEditEdgePage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PSOMS ps = (PSOMS)pPage->m_pUserData;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         // draw images
         DSGenerateThreeDForSplitOverlay (L"edgeaddremove", pPage,
            &ps->pThis->m_Spline,
            ps->szEditing, TRUE);
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
         pss = &ps->pThis->m_Spline;
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
            &ps->pThis->m_Spline,
            ps->szEditing, TRUE);
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

/* SingleSurfaceOverAddCustomPage
*/
BOOL SingleSurfaceOverAddCustomPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PSOMS ps = (PSOMS)pPage->m_pUserData;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         DSGenerateThreeDForPosition (L"overlayedit", pPage,
            &ps->pThis->m_Spline, 8, TRUE);

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
            ps->pThis->AddOverlay (pszName, (PTEXTUREPOINT)pl->Get(0), pl->Num(), TRUE,
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


/* SingleSurfaceOverEditCustomPage
*/
BOOL SingleSurfaceOverEditCustomPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
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
         pss = &ps->pThis->m_Spline;
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



/*******************************************************************************
CSingleSurface::SurfaceDetailPage - Shows the page.

inputs
   PCEscWindow    pWindow - window
returns
   PWSTR - Return string
*/
PWSTR CSingleSurface::SurfaceDetailPage (PCEscWindow pWindow)
{
   return pWindow->PageDialog (ghInstance, IDR_MMLSURFACEDETAIL, ::SingleSurfaceDetailPage, this);
      // NOTE: Using the same page as DoubleSurface
}

/*******************************************************************************
CSingleSurface::SurfaceCurvaturePage - Shows the page.

inputs
   PCEscWindow    pWindow - window
returns
   PWSTR - Return string
*/
PWSTR CSingleSurface::SurfaceCurvaturePage (PCEscWindow pWindow)
{
   PWSTR psz;
redo:
   psz = pWindow->PageDialog (ghInstance, IDR_MMLSINGLECURVATURE, ::SingleSurfaceCurvaturePage, this);
   if (psz && !_wcsicmp(psz, RedoSamePage()))
      goto redo;

   return psz;
}







/*******************************************************************************
CSingleSurface::SurfaceControlPointsPage - Shows the page.

inputs
   PCEscWindow    pWindow - window
returns
   PWSTR - Return string
*/
PWSTR CSingleSurface::SurfaceControlPointsPage (PCEscWindow pWindow)
{
   return pWindow->PageDialog (ghInstance, IDR_MMLSINGLECONTROLPOINTS, ::SingleSurfaceControlPointsPage, this);
}

/*******************************************************************************
CSingleSurface::SurfaceEdgePage - Shows the page.

inputs
   PCEscWindow    pWindow - window
returns
   PWSTR - Return string
*/
PWSTR CSingleSurface::SurfaceEdgePage (PCEscWindow pWindow)
{
   return pWindow->PageDialog (ghInstance, IDR_MMLSURFACEEDGE, ::SingleSurfaceEdgePage, this);
      // NOTE: Using the same MML resource as the double surface object
}



/*******************************************************************************
CSingleSurface::SurfaceOverMainPage - Shows the page.

inputs
   PCEscWindow    pWindow - window
   DWORD          *pdwDisplayControl - Pointer to the m_dwDisplayControl value
                     so it gets changed to display the right points
returns
   PWSTR - Return string
*/
PWSTR CSingleSurface::SurfaceOverMainPage (PCEscWindow pWindow,
                                           DWORD *pdwDisplayControl)
{


   CListFixed lSelected;
   lSelected.Init (sizeof(DWORD));

   SOMS soms;
   memset (&soms, 0, sizeof(soms));
   soms.plSelected = &lSelected;
   soms.pThis = this;
   soms.pdwCurColor = pdwDisplayControl;
   PWSTR pszRet;

overmainpage:
   pszRet = pWindow->PageDialog (ghInstance, IDR_MMLSINGLEOVERMAIN, ::SingleSurfaceOverMainPage, &soms);
   if (!pszRet)
      goto cleanup;
   if (!_wcsicmp(pszRet, L"addcustom")) {
      pszRet = pWindow->PageDialog (ghInstance, IDR_MMLSURFACEOVERADDCUSTOM, SingleSurfaceOverAddCustomPage, &soms);
         // NOTE: using same MML resource as DoubleSurface
      if (!pszRet)
         goto cleanup;
      if (!_wcsicmp(pszRet, gszBack))
         goto overmainpage;
      goto cleanup;
   }
   else if (!_wcsicmp(pszRet, L"editedge")) {
      pszRet = pWindow->PageDialog (ghInstance, IDR_MMLSURFACEOVEREDITEDGE, SingleSurfaceOverEditEdgePage, &soms);
         // NOTE: using the same MML resource as doublesurface
      if (!pszRet)
         goto cleanup;
      if (!_wcsicmp(pszRet, gszBack))
         goto overmainpage;
      goto cleanup;
   }
   else if (!_wcsicmp(pszRet, L"editcustom")) {
      pszRet = pWindow->PageDialog (ghInstance, IDR_MMLSURFACEOVEREDITCUSTOM, SingleSurfaceOverEditCustomPage, &soms);
         // NOTE: Using the same MML resoruce as doublesurface
      if (!pszRet)
         goto cleanup;
      if (!_wcsicmp(pszRet, gszBack))
         goto overmainpage;
      goto cleanup;
   }
   goto cleanup;


cleanup:
   return pszRet;
}


/**************************************************************************************
CSingleSurface::ChangedHV - Internal function that is called whenever the HV
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

#define FITTO(pt) FitTo(pt, dwH+1, dwV+1, afHOrig, afVOrig, afHNew, afVNew, afHScale, afVScale)
#define FITTO2(pt) FitTo(pt, dwH+1, dwV+1, afHOrig, afVOrig, afHNew, afVNew, afHScale, afVScale, FALSE)
#define MAXSPLIT  6

void CSingleSurface::ChangedHV (DWORD dwH, DWORD dwV,
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
   dwSide = 1;
   PCSplineSurface pss = &m_Spline;

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

   // tell all the objects they've moved so they end up redoing the cutouts
   DWORD dwNum;
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

      // squash this
      FITTO (&tp);

      // pass it back to the object so it recalculates all its cutouts and stuff
      m_pTemplate->ContEmbeddedLocationSet (&gEmbed, &tp, fRotation, dwSurface);

   }
}


/*********************************************************************************
CSingleSurface::IntersectLine - Intersects a line with the spline. If there is
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
BOOL CSingleSurface::IntersectLine (PCPoint pStart, PCPoint pDirection, PTEXTUREPOINT ptp,
                                    BOOL fIncludeCutouts, BOOL fPositiveAlpha)
{
   PCSplineSurface pss = &m_Spline;
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
CSingleSurface::ContHVQuery
Called to see the HV location of what clicked on, or in other words, given a line
from pEye to pClick (object sapce), where does it intersect surface dwSurface (LOWORD(PIMAGEPIXEL.dwID))?
Fills pHV with the point. Returns FALSE if doesn't intersect, or invalid surface, or
can't tell with that surface. NOTE: if pOld is not NULL, then assume a drag from
pOld (converted to world object space) to pClick. Finds best intersection of the
surface then. Useful for dragging embedded objects around surface
*/
BOOL CSingleSurface::ContHVQuery (PCPoint pEye, PCPoint pClick, DWORD dwSurface, PTEXTUREPOINT pOld, PTEXTUREPOINT pHV)
{
   PCLAIMSURFACE pcs = ClaimFindByID (dwSurface);
   if (!pcs)
      return FALSE;

   if (pOld) {
      // drag, so use one API
      return HVDrag (pOld, pClick, pEye, pHV);
   }
   else {
      CPoint pDirection;
      pDirection.Subtract (pClick, pEye);

      // click, so use slightly different
      return IntersectLine (pEye, &pDirection, pHV, FALSE, TRUE);
   }

   return FALSE;
}




/**********************************************************************************
CSingleSurface::ContCutout
Called by an embeded object to specify an arch cutout within the surface so the
object can go through the surface (like a window). The container should check
that pgEmbed is a valid object. dwNum is the number of points in the arch,
paFront and paBack are the container-object coordinates. (See CSplineSurface::ArchToCutout)
If dwNum is 0 then arch is simply removed. If fBothSides is true then both sides
of the surface are cleared away, else only the side where the object is embedded
is affected.

*/
BOOL CSingleSurface::ContCutout (const GUID *pgEmbed, DWORD dwNum, PCPoint paFront, PCPoint paBack, BOOL fBothSides)
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

   // if dwNum < 3 then just clear out the cutouts
   if (dwNum < 3) {
      m_Spline.CutoutRemove (szTemp);

      if (m_pTemplate->m_pWorld)
         m_pTemplate->m_pWorld->ObjectChanged(m_pTemplate);
      return TRUE;
   }

   // else create
   m_Spline.ArchToCutout (dwNum, paFront, paBack, szTemp, FALSE);

   if (m_pTemplate->m_pWorld)
      m_pTemplate->m_pWorld->ObjectChanged(m_pTemplate);

   return TRUE;
}




/**********************************************************************************
CSingleSurface::ContSideInfo -
returns a DWORD indicating what class of surface it is... right now
0 = internal, 1 = external. dwSurface is the surface. If fOtherSide
then want to know what's on the opposite side. Returns 0 if bad dwSurface.

NOTE: usually overridden
*/
DWORD CSingleSurface::ContSideInfo (DWORD dwSurface, BOOL fOtherSide)
{
   PCLAIMSURFACE pcs = ClaimFindByID (dwSurface);
   if (!pcs)
      return 0;

   return 1;
}


/**********************************************************************************
CSingleSurface::ContMatrixFromHV - THE SUPERCLASS SHOULD OVERRIDE THIS. Given
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
BOOL CSingleSurface::ContMatrixFromHV (DWORD dwSurface, PTEXTUREPOINT pHV, fp fRotation, PCMatrix pm)
{
   PCLAIMSURFACE pcs = ClaimFindByID (dwSurface);
   if (!pcs)
      return 0;

   PCSplineSurface pss;
   pss = &m_Spline;

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



/*************************************************************************************
CSingleSurface::CutoutFromSpline - Given spline infomration that's going clockwise and which
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
BOOL CSingleSurface::CutoutFromSpline (PWSTR pszName, DWORD dwNum, PCPoint paPoints, DWORD *padwSegCurve,
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

   // if it's the entire surface then just remove this cutout and be done with it
   if (!fClockwise && IsCutoutEntireSurface (lTP.Num(), ptp)) {
      m_Spline.CutoutRemove (pszName);
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
   m_Spline.CutoutSet (pszName, (PTEXTUREPOINT) lTP.Get(0),
      lTP.Num(), fClockwise);

   return TRUE;
}


/*************************************************************************************
CSingleSurface::AddOverlay - Adds a new overlay to either side A or side B.
It does the following:
   1) Muck with the name to indicate the surface ID in the name
   2) Add the overlay
   3) Add the texture (make it red so it's noticable)
   4) Add the click-area
   5) Of course, notify world that object has changed

inputs
   PWSTR             pszName - Name of the overlay
   PTEXTUREPOINT     ptp - Overlay info
   DWORD             dwNum - Number of points in ptp
   BOOL              fClockwise - TRUE if it's clockwise
   DWORD             *pdwDisplayControl - Filled with a pointer to the main object's
                     m_dwDisplayControl. This will be set so that the control
                     points are displayed.
returns
   BOOL - TRUE if success
*/
BOOL CSingleSurface::AddOverlay (PWSTR pszName, PTEXTUREPOINT ptp,
                                       DWORD dwNum, BOOL fClockwise, DWORD *pdwDisplayControl)
{
   // tell the world that changing
   if (m_pTemplate->m_pWorld)
      m_pTemplate->m_pWorld->ObjectAboutToChange (m_pTemplate);

   DWORD i;

   i = ClaimSurface (3, TRUE, RGB(0xff,0,0), MATERIAL_PAINTSEMIGLOSS);
   if (!i) {
      if (m_pTemplate->m_pWorld)
         m_pTemplate->m_pWorld->ObjectChanged (m_pTemplate);
      return FALSE;
   }

   PCSplineSurface pss;
   pss = &m_Spline;


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

      // make sure they're visible
      m_pTemplate->m_pWorld->SelectionClear();
      GUID gObject;
      m_pTemplate->GUIDGet (&gObject);
      m_pTemplate->m_pWorld->SelectionAdd (m_pTemplate->m_pWorld->ObjectFind(&gObject));
   }


   return TRUE;
}


/**********************************************************************************
CSingleSurface::
Called to tell the container that one of its contained objects has been renamed.
*/
BOOL CSingleSurface::ContEmbeddedRenamed (const GUID *pgOld, const GUID *pgNew)
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
   j = 1;
   pss = &m_Spline;

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


   // call into the template
   return m_pTemplate->CObjectTemplate::ContEmbeddedRenamed (pgOld, pgNew);
}


/**********************************************************************************
CSingleSurface::MatrixSet - Sets the translation and rotation matrix. This is
the matrix for converting from the surface space into object space.

inputs
   PCMatrix       pm - Matrix that want
returns
   none
*/
void CSingleSurface::MatrixSet (PCMatrix pm)
{
   m_MatrixToObject.Copy (pm);
   m_MatrixToObject.Invert4 (&m_MatrixFromObject);

   // tell the embedded objects that they've moved
   TellEmbeddedThatMoved();
}

/**********************************************************************************
CSingleSurface::MatrixGet - Gets the translation and rotation matrix. This is
the matrix for converting from the surface space into object space.

inputs
   PCMatrix       pm - Matrix that want
returns
   none
*/
void CSingleSurface::MatrixGet (PCMatrix pm)
{
   pm->Copy (&m_MatrixToObject);
}



/**********************************************************************************
CSingleSurface::ControlPointsSet - Sets a bunch of control points at once

inputs
   BOOL        fLoopH, fLoopV - TRUE if loops in either direction
   DWORD       dwControlH - Number of control points in H
   DWORD       dwControlV - Number of control points in V
   PCPoint     paPoints - Pointer to control pointed. Arrayed as [dwControlV][dwControlH]
   DWORD       *padwSegCurveH - Pointer to an array of dwControlH DWORDs continging
                  SEGCURVE_XXX descrbing the curve. This can also be (DWORD*) SEGCURVE_XXX
                  directly. If not looped in h then dwControlH-1 points.

  NOTE: This points are in spline space, NOT object space.

   DWORD       *padwSegCurveV - Like padwSegCurveH. If not looped in v then dwControlV-1 points.
returns
   BOOL - TRUE if success
*/

BOOL CSingleSurface::ControlPointsSet (BOOL fLoopH, BOOL fLoopV, DWORD dwControlH, DWORD dwControlV,
   PCPoint paPoints, DWORD *padwSegCurveH, DWORD *padwSegCurveV)
{
   m_Spline.ControlPointsSet(
      fLoopH, fLoopV,
      dwControlH, dwControlV, paPoints, padwSegCurveH, padwSegCurveV);

   // Readjust
   RecalcSides ();
   RecalcEdges ();

   return TRUE;
}


/*************************************************************************************
CSingleSurface::EmbedIntegrityCheck - Looks through all the cutouts for
embeddd objects (which are guids). Then, checks in the m_pTemplate->m_pWorld to
see if they still exist. If they don't the holes are removed.
*/
void CSingleSurface::EmbedIntegrityCheck (void)
{
   if (!m_pTemplate || !m_pTemplate->m_pWorld)
      return;

   DWORD dwSide, i;
   BOOL fChanged;
   fChanged = FALSE;
   dwSide = 1;
   PCSplineSurface pss = &m_Spline;

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

   if (fChanged)
      m_pTemplate->m_pWorld->ObjectChanged (m_pTemplate);
}



