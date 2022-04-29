/************************************************************************
CObjectCabinet.cpp - Draws a box.

begun 14/9/01 by Mike Rozak
Copyright 2002 Mike Rozak. All rights reserved
*/

#include <windows.h>
#include "escarpment.h"
#include "..\M3D.h"
#include "resource.h"

#define SHELFSPACING          .30      // default shelf spacing

static PWSTR gszPoints = L"points%d-%d";
static PWSTR gpszCabinetClip = L"CabinetClip";
static PWSTR gpszTrimRoof = L"TrimRoof";

#define BBSURFMAJOR_WALL         0
#define BBSURFMAJOR_ROOF         1
#define BBSURFMAJOR_FLOOR        2
#define BBSURFMAJOR_TOPCEILING   3
#define BBSURFMAJOR_VERANDAH     4


#define BBSURFFLAG_CLIPAGAINSTROOF 0x0001 // this wall/roof need to be clipped against the roof, if roof changed
#define BBSURFFLAG_CLIPAGAINSTWALLS 0x0002   // this floor need to be clipped against the walls
#define BBSURFFLAG_CLIPAGAINSRROOFWALLS 0x0003  // floor needs clipping against the roof and walls

#define WALLSHAPE_RECTANGLE            0
#define WALLSHAPE_NONE                 1
#define WALLSHAPE_TRIANGLE             2
#define WALLSHAPE_PENTAGON             3
#define WALLSHAPE_HEXAGON              4
#define WALLSHAPE_CIRCLE               5
#define WALLSHAPE_SEMICIRCLE           6
#define WALLSHAPE_ANY                  7

#define ROOFSHAPE_HIPHALF        0
#define ROOFSHAPE_HIPPEAK        1
#define ROOFSHAPE_HIPRIDGE       2
#define ROOFSHAPE_OUTBACK1       3
#define ROOFSHAPE_SALTBOX        4
#define ROOFSHAPE_GABLETHREE     5
#define ROOFSHAPE_SHED           6
#define ROOFSHAPE_GABLEFOUR      7
#define ROOFSHAPE_SHEDFOLDED     8
#define ROOFSHAPE_HIPSHEDPEAK    9
#define ROOFSHAPE_HIPSHEDRIDGE   10
#define ROOFSHAPE_MONSARD        11
#define ROOFSHAPE_GAMBREL        12
#define ROOFSHAPE_GULLWING       13
#define ROOFSHAPE_OUTBACK2       14
#define ROOFSHAPE_BALINESE       15
#define ROOFSHAPE_FLAT           16
#define ROOFSHAPE_NONE           17 // special case
#define ROOFSHAPE_TRIANGLEPEAK   18
#define ROOFSHAPE_PENTAGONPEAK   19
#define ROOFSHAPE_HEXAGONPEAK    20
#define ROOFSHAPE_HALFHIPSKEW    21
#define ROOFSHAPE_SHEDSKEW       22

#define ROOFSHAPE_SHEDCURVED     100
#define ROOFSHAPE_BALINESECURVED 101
#define ROOFSHAPE_CONE           102
#define ROOFSHAPE_CONECURVED     103
#define ROOFSHAPE_HALFHIPLOOP    104
#define ROOFSHAPE_HEMISPHERE     105
#define ROOFSHAPE_HEMISPHEREHALF 106
#define ROOFSHAPE_GULLWINGCURVED 107
#define ROOFSHAPE_HALFHIPCURVED  109




// DialogShow information
typedef struct {
   PCObjectCabinet pThis;  // this object
   DWORD       dwSurface;  // surface ID that clicked on
   PBBSURF     pSurf;      // surface that clicked on (if did)
   int         iSide;      // side that clicked on: 1 for side A, -1 for B, 0 for unknown
   PCSplineSurface pssAny; // spline surface used for any shape
   BOOL        fWall;      // set to TRUE if mucking with walls, FALSE if vernadah
} DSI, *PDSI;

#define     SURFACEEDGE       10
#define     SIDEA             100
#define     SIDEB             200
#define     MAXCOLORPERSIDE   99          // maximum number of color settings per side




static BOOL GenerateThreeDFromSpline (PWSTR pszControl, PCEscPage pPage, PCSpline pSpline, PCSplineSurface pss,
                                      DWORD dwUse);
/**********************************************************************************
CObjectCabinet::Constructor and destructor

(DWORD)pParams is passed into CDoubleSurface::Init(). See definitions of parameters

 */
CObjectCabinet::CObjectCabinet (PVOID pParams, POSINFO pInfo)
{
   m_dwType = (DWORD)(size_t) pParams;
   m_OSINFO = *pInfo;

   m_listBBSURF.Init (sizeof(BBSURF));
   m_listWish.Init (sizeof(BBSURF));

   // set the path
   CPoint ap[3];
   memset (ap, 0, sizeof(ap));
   ap[0].p[0] = -1;
   ap[1].p[0] = 1;
   m_sPath.Init (FALSE, 2, ap, NULL, (DWORD*) SEGCURVE_LINEAR, 3, 3, .1);

   m_fDrawCounter = TRUE;
   m_fDrawOnlyBottom = FALSE;
   m_dwRoundEnd = 0;
   m_dwShowWall = 0x0f;
   m_fBaseHeight = .1;
   m_fCabinetDepth = CM_CABINETDEPTH;
   m_fCounterRounded = 0;
   DWORD i;
   for (i = 0; i < 4; i++) {
      m_pCounterOverhang.p[i] = CM_CABINETCOUNTEROVERHANG;
      m_pBaseIndent.p[i] = CM_CABINETBASEEXTEND;
   }
 

   m_fHeight = CM_CABINETHEIGHT;

   m_fCounterThick = CM_THICKCABINETCOUNTER;
   m_fWallThick = CM_THICKCABINETWALL;
   m_fShelfThick = CM_THICKCABINETSHELF;

   m_fLevelElevation[0] = m_fShelfThick;  // just above the bottom
   for (i = 1; i < NUMLEVELS; i++)
      m_fLevelElevation[i] = m_fLevelElevation[i-1] + SHELFSPACING;
   m_fLevelHigher = SHELFSPACING;
   m_fLevelMinDist = SHELFSPACING/2;



   m_dwRenderShow = RENDERSHOW_CABINETS;

   AdjustAllSurfaces();

   IntegrityCheck();
}


CObjectCabinet::~CObjectCabinet (void)
{
   // Need to delete doublesurface objects
   DWORD i;
   for (i = 0; i < m_listBBSURF.Num(); i++) {
      PBBSURF ps = (PBBSURF) m_listBBSURF.Get(i);
      if (ps->pSurface)
         delete ps->pSurface;
   }
   m_listBBSURF.Clear();

}




/**********************************************************************************
CObjectCabinet::AdjustAllSurfaces - This function:
   1) Figures out what surfaces are needed given the building block shape.
   2) If any exist that shouldn't, they're removed.
   3) If any don't exist, they're created with dummy sizes.
   4) Goes through all of them and makes sure they're in the right location
      and the right size.
   5) Clip the walls agasint the roof, and roof against roof, if necessary. (If
      any have changed.)
   6) Remembers to call the world and tell it things have changed (if they have)

inputs
   none
returns
   none
*/
static PWSTR gszWall = L"Wall";
static PWSTR gszFloor = L"Floor";
static PWSTR gszRoof = L"Roof";
static PWSTR gszCeiling = L"Ceiling";
static PWSTR gszBasement = L"Basement";
static PWSTR gszPad = L"Pad";
static PWSTR gszVerandah = L"Verandah";

void CObjectCabinet::AdjustAllSurfaces (void)
{
#ifdef _DEBUG
   OutputDebugString ("AdjustAllSurfaces\r\n");
#endif

   m_listWish.Clear();

   // remeber if sent world changed notifcation
   BOOL  fChanged = FALSE;

   DWORD i;

   CPoint pExtendCounter, pExtendCabinet;
   BOOL afRound[2];
   afRound[0] = (m_dwRoundEnd & 0x01) ? TRUE : FALSE;
   afRound[1] = (m_dwRoundEnd & 0x02) ? TRUE : FALSE;

   // countertop
   pExtendCounter.p[0] = m_fCabinetDepth / 2 + m_pCounterOverhang.p[0];
   pExtendCounter.p[1] = m_fCabinetDepth / 2 + m_pCounterOverhang.p[1];
   pExtendCounter.p[2] = m_pCounterOverhang.p[2];
   pExtendCounter.p[3] = m_pCounterOverhang.p[3];

   // shelves
   // If wall not visible then don't extend
   pExtendCabinet.p[0] = m_fCabinetDepth / 2 - ((m_dwShowWall & 0x01) ? m_fWallThick : 0);
   pExtendCabinet.p[1] = m_fCabinetDepth / 2 - ((m_dwShowWall & 0x02) ? m_fWallThick : 0);
   pExtendCabinet.p[2] = -((m_dwShowWall & 0x04) ? m_fWallThick : 0);
   pExtendCabinet.p[3] = -((m_dwShowWall & 0x08) ? m_fWallThick : 0);

   // draw the counter top
   if (m_fDrawCounter)
      MakeShelfOrCounter (2000, m_fHeight - m_fCounterThick/2, m_fCounterThick,
         TRUE, &pExtendCounter, m_fCounterRounded,
         afRound);
   else
      MakeShelfOrCounter (2001, m_fHeight - m_fShelfThick/2, m_fShelfThick,
         FALSE, &pExtendCabinet, 0);

   // draw the shelves
   for (i = 0; ; i++) {
      fp fHeight;
      if (i < NUMLEVELS)
         fHeight = m_fLevelElevation[i] + m_fBaseHeight;
      else
         fHeight = m_fLevelElevation[NUMLEVELS-1] + m_fBaseHeight + m_fLevelHigher * (i - NUMLEVELS+1);

      // if too high then no more shelves
      if (i && (fHeight + m_fLevelMinDist >= m_fHeight))
         break;

      MakeShelfOrCounter (4000 + i, fHeight - m_fShelfThick/2, m_fShelfThick,
         FALSE, &pExtendCabinet, 0);

      // if option only to draw bottom shelf then do just that
      if (m_fDrawOnlyBottom)
         break;
   }

   // cabinet walls
   fp fWallHeight;
   fWallHeight = m_fHeight - (m_fDrawCounter ? m_fCounterThick : 0);
   if (m_fBaseHeight < fWallHeight)
      MakeTheWalls (m_fBaseHeight, fWallHeight, FALSE);

   // base
   if (m_fBaseHeight > 0)
      MakeTheWalls (0, m_fBaseHeight, TRUE);



   // look for ones that exist that shouldn't
   DWORD j;
   PBBSURF pLookFor, pLookAt;
   for (i = m_listBBSURF.Num()-1; i < m_listBBSURF.Num(); i--) {
      pLookFor = (PBBSURF) m_listBBSURF.Get(i);

      // see if can find it
      for (j = 0; j < m_listWish.Num(); j++) {
         pLookAt = (PBBSURF) m_listWish.Get(j);
         if ((pLookFor->dwMajor == pLookAt->dwMajor) && (pLookFor->dwMinor == pLookAt->dwMinor))
            break;
      }
      if (j >= m_listWish.Num()) {
         // world about to change
         if (!fChanged && m_pWorld) {
            m_pWorld->ObjectAboutToChange (this);
            fChanged = TRUE;
         }

         // didn't find it on the wish list, so remove it
         pLookFor->pSurface->ClaimClear();
         delete pLookFor->pSurface;
         m_listBBSURF.Remove(i);
      }
   }



   // finally, if changed then send finish changed notification
   if (fChanged && m_pWorld)
      m_pWorld->ObjectChanged (this);
}

/**********************************************************************************
CObjectCabinet::Delete - Called to delete this object
*/
void CObjectCabinet::Delete (void)
{
   delete this;
}

/**********************************************************************************
CObjectCabinet::Render - Draws a test box.

inputs
   POBJECTRENDER     pr - Render information
*/
void CObjectCabinet::Render (POBJECTRENDER pr, DWORD dwSubObject)
{
   // create the surface render object and draw
   // CRenderSurface rs;
   m_Renderrs.ClearAll();
   CMatrix mObject;
   m_Renderrs.Init (pr->pRS);

   // real code
   ObjectMatrixGet (&mObject);
   m_Renderrs.Multiply (&mObject);

   DWORD i;
   PBBSURF pbbs;
   for (i = 0; i < m_listBBSURF.Num(); i++) {
      pbbs = (PBBSURF) m_listBBSURF.Get(i);
      if (pbbs->fHidden)
         continue;

      pbbs->pSurface->Render (m_OSINFO.dwRenderShard, pr, &m_Renderrs);
   }

   m_Renderrs.Commit();
}



/**********************************************************************************
CObjectCabinet::QueryBoundingBox - Standard API
*/
void CObjectCabinet::QueryBoundingBox (PCPoint pCorner1, PCPoint pCorner2, DWORD dwSubObject)
{
   BOOL fSet = FALSE;
   CPoint p1, p2;

   DWORD i;
   PBBSURF pbbs;
   for (i = 0; i < m_listBBSURF.Num(); i++) {
      pbbs = (PBBSURF) m_listBBSURF.Get(i);
      if (pbbs->fHidden)
         continue;

      pbbs->pSurface->QueryBoundingBox (&p1, &p2);

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

#ifdef _DEBUG
   // test, make sure bounding box not too small
   //CPoint p1,p2;
   //DWORD i;
   CObjectTemplate::QueryBoundingBox (&p1, &p2, dwSubObject);
   for (i = 0; i < 3; i++)
      if ((p1.p[i] < pCorner1->p[i] - CLOSE) || (p2.p[i] > pCorner2->p[i] + CLOSE))
         break;
   if (i < 3)
      OutputDebugString ("\r\nCObjectCabinet::QueryBoundingBox too small.");
#endif
}

/**********************************************************************************
CObjectCabinet::Clone - Clones this

inputs
   none
returns
   PCObjectSocket - Clone
*/
PCObjectSocket CObjectCabinet::Clone (void)
{
   PCObjectCabinet pNew;

   pNew = new CObjectCabinet((PVOID)(size_t) m_dwType, &m_OSINFO);

   // clone template info
   CloneTemplate(pNew);

   // variables
   m_sPath.CloneTo (&pNew->m_sPath);
   pNew->m_fHeight = m_fHeight;
   pNew->m_fCounterThick = m_fCounterThick;
   pNew->m_fWallThick = m_fWallThick;
   pNew->m_fShelfThick = m_fShelfThick;

   pNew->m_fDrawCounter = m_fDrawCounter;
   pNew->m_fDrawOnlyBottom = m_fDrawOnlyBottom;
   pNew->m_dwRoundEnd = m_dwRoundEnd;
   pNew->m_dwShowWall = m_dwShowWall;
   pNew->m_fBaseHeight = m_fBaseHeight;
   pNew->m_fCabinetDepth = m_fCabinetDepth;
   pNew->m_fCounterRounded = m_fCounterRounded;
   pNew->m_pCounterOverhang.Copy (&m_pCounterOverhang);
   pNew->m_pBaseIndent.Copy (&m_pBaseIndent);

   memcpy (pNew->m_fLevelElevation, m_fLevelElevation, sizeof(m_fLevelElevation));
   pNew->m_fLevelHigher = m_fLevelHigher;
   pNew->m_fLevelMinDist = m_fLevelMinDist;


   // clone all the surfaces
   DWORD i;
   PBBSURF pbbs;
   BBSURF bbs;
   for (i = 0; i < pNew->m_listBBSURF.Num(); i++) {
      PBBSURF ps = (PBBSURF) pNew->m_listBBSURF.Get(i);
      if (ps->pSurface)
         delete ps->pSurface;
   }
   pNew->m_listBBSURF.Clear();
   pNew->m_listBBSURF.Required (m_listBBSURF.Num());
   for (i = 0; i < m_listBBSURF.Num(); i++) {
      pbbs = (PBBSURF) m_listBBSURF.Get(i);
      bbs = *pbbs;
      bbs.pSurface = new CDoubleSurface;
      if (!bbs.pSurface)
         continue;

      // initialize but dont create
      bbs.pSurface->InitButDontCreate (bbs.dwType, pNew);
      pbbs->pSurface->CloneTo (bbs.pSurface, pNew);
      pNew->m_listBBSURF.Add (&bbs);
   }

   pNew->IntegrityCheck();

   return pNew;
}

static PWSTR gpszHeight = L"Height";
static PWSTR gpszRoofThickStruct = L"RoofThickStruct";
static PWSTR gpszWallThickStruct = L"WallThickStruct";
static PWSTR gpszFloorThickStruct = L"FloorThickStruct";

static PWSTR gpszBBSName = L"BBSName";
static PWSTR gpszBBSHidden = L"BBSHidden";
static PWSTR gpszBBSMajor = L"BBSMajor";
static PWSTR gpszBBSMinor = L"BBSMinor";
static PWSTR gpszBBSFlags = L"BBSFlags";
static PWSTR gpszBBSHeight = L"BBSHeight";
static PWSTR gpszBBSWidth = L"BBSWidth";
static PWSTR gpszBBSType = L"BBSType";
static PWSTR gpszBBSTrans = L"BBSTrans";
static PWSTR gpszBBSRotX = L"BBSRotX";
static PWSTR gpszBBSRotY = L"BBSRotY";
static PWSTR gpszBBSRotZ = L"BBSRotZ";
static PWSTR gpszLevelElevation = L"LevelElevation%d";
static PWSTR gpszLevelHigher = L"LevelHigher";
static PWSTR gpszLevelMinDist = L"LevelMinDist";
static PWSTR gpszDontIncludeInVolume = L"DontIncludeInVolume";
static PWSTR gpszPath = L"Path";
static PWSTR gpszDrawCounter = L"DrawCounter";
static PWSTR gpszDrawOnlyBottom = L"DrawOnlyBottom";
static PWSTR gpszRoundEnd = L"RoundEnd";
static PWSTR gpszShowWall = L"ShowWall";
static PWSTR gpszBaseHeight = L"BaseHeight";
static PWSTR gpszCabinetDepth = L"CabinetDepth";
static PWSTR gpszCounterRounded = L"CounterRounded";
static PWSTR gpszCounterOverhang = L"CounterOverhang";
static PWSTR gpszBaseIndent = L"BaseIndent";

PCMMLNode2 CObjectCabinet::MMLTo (void)
{
   // call into the template first
   PCMMLNode2 pNode = MMLToTemplate();
   if (!pNode)
      return NULL;

   // member variables go here
   PCMMLNode2 pSub;
   DWORD i;
   for (i = 0; i < m_listBBSURF.Num(); i++) {
      PBBSURF pbbs = (PBBSURF) m_listBBSURF.Get(i);

      pSub = pbbs->pSurface->MMLTo();
      if (!pSub)
         continue;

      pSub->NameSet (L"DoubleSurface");
      pNode->ContentAdd (pSub);

      MMLValueSet (pSub, gpszBBSName, pbbs->szName);
      MMLValueSet (pSub, gpszBBSHidden, (int) pbbs->fHidden);
      MMLValueSet (pSub, gpszBBSMajor, (int) pbbs->dwMajor);
      MMLValueSet (pSub, gpszBBSMinor, (int) pbbs->dwMinor);
      MMLValueSet (pSub, gpszBBSFlags, (int) pbbs->dwFlags);
      MMLValueSet (pSub, gpszBBSHeight, pbbs->fHeight);
      MMLValueSet (pSub, gpszBBSWidth, pbbs->fWidth);
      MMLValueSet (pSub, gpszBBSType, (int) pbbs->dwType);
      MMLValueSet (pSub, gpszBBSTrans, &pbbs->pTrans);
      MMLValueSet (pSub, gpszBBSRotX, pbbs->fRotX);
      MMLValueSet (pSub, gpszBBSRotY, pbbs->fRotY);
      MMLValueSet (pSub, gpszBBSRotZ, pbbs->fRotZ);
      MMLValueSet (pSub, gpszDontIncludeInVolume, (int)pbbs->fDontIncludeInVolume);
         //BUGFIX - So that intersect multistory building blocks works
      // dont need to save bevel
   }


   MMLValueSet (pNode, gpszDrawCounter, (int) m_fDrawCounter);
   MMLValueSet (pNode, gpszDrawOnlyBottom, (int) m_fDrawOnlyBottom);
   MMLValueSet (pNode, gpszRoundEnd, (int) m_dwRoundEnd);
   MMLValueSet (pNode, gpszShowWall, (int) m_dwShowWall);
   MMLValueSet (pNode, gpszBaseHeight, m_fBaseHeight);
   MMLValueSet (pNode, gpszCabinetDepth, m_fCabinetDepth);
   MMLValueSet (pNode, gpszCounterRounded, m_fCounterRounded);
   MMLValueSet (pNode, gpszCounterOverhang, &m_pCounterOverhang);
   MMLValueSet (pNode, gpszBaseIndent, &m_pBaseIndent);

   MMLValueSet (pNode, gpszHeight, m_fHeight);
   MMLValueSet (pNode, gpszRoofThickStruct, m_fCounterThick);
   MMLValueSet (pNode, gpszWallThickStruct, m_fWallThick);
   MMLValueSet (pNode, gpszFloorThickStruct, m_fShelfThick);

   MMLValueSet (pNode, gpszLevelHigher, m_fLevelHigher);
   MMLValueSet (pNode, gpszLevelMinDist, m_fLevelMinDist);
   WCHAR szTemp[64];
   for (i = 0; i < NUMLEVELS; i++) {
      swprintf (szTemp, gpszLevelElevation, (int) i);
      MMLValueSet (pNode, szTemp, m_fLevelElevation[i]);
   }


   // path
   pSub = m_sPath.MMLTo();
   if (pSub) {
      pSub->NameSet (gpszPath);
      pNode->ContentAdd (pSub);
   }

   return pNode;
}

BOOL CObjectCabinet::MMLFrom (PCMMLNode2 pNode)
{
   // call into the template first
   if (!MMLFromTemplate (pNode))
      return FALSE;


   // delete all old walls and stuff surfaces
   // dont need to remove their Claims (ObjectSurfaceSet, etc.) because by calling
   // MMLFromTemplate() that's already been done
   DWORD i;
   for (i = 0; i < m_listBBSURF.Num(); i++) {
      PBBSURF ps = (PBBSURF) m_listBBSURF.Get(i);
      if (ps->pSurface)
         delete ps->pSurface;
   }
   m_listBBSURF.Clear();

   // member variables go here
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
      if (!_wcsicmp(psz, L"DoubleSurface")) {
         PCDoubleSurface pds;
         pds = new CDoubleSurface;
         if (!pds)
            continue;

         // fill in the attached information
         BBSURF bbs;
         memset (&bbs, 0, sizeof(bbs));
         psz = MMLValueGet (pSub, gpszBBSName);
         if (psz)
            wcscpy (bbs.szName, psz);
         bbs.fHidden = (BOOL) MMLValueGetInt (pSub, gpszBBSHidden, 0);
         bbs.dwMajor = (DWORD) MMLValueGetInt (pSub, gpszBBSMajor, 0);
         bbs.dwMinor = (DWORD) MMLValueGetInt (pSub, gpszBBSMinor, 0);
         bbs.dwFlags = (DWORD) MMLValueGetInt (pSub, gpszBBSFlags, 0);
         bbs.fHeight = MMLValueGetDouble (pSub, gpszBBSHeight, 1);
         bbs.fWidth = MMLValueGetDouble (pSub, gpszBBSWidth, 1);
         bbs.dwType = (DWORD) MMLValueGetInt (pSub, gpszBBSType, 0);
         MMLValueGetPoint (pSub, gpszBBSTrans, &bbs.pTrans, &bbs.pTrans);
         bbs.fRotX = MMLValueGetDouble (pSub, gpszBBSRotX, 0);
         bbs.fRotZ = MMLValueGetDouble (pSub, gpszBBSRotZ, 0);
         bbs.fRotY = MMLValueGetDouble (pSub, gpszBBSRotY, 0);
         bbs.fDontIncludeInVolume = (int) MMLValueGetInt (pSub, gpszDontIncludeInVolume, FALSE);
         //BUGFIX - So that intersect multistory building blocks works
         // dont need to save bevel

         // Initializing the object, but then not creating any surfaces to register
         bbs.pSurface = pds;
         pds->InitButDontCreate (bbs.dwType, this);
         pds->MMLFrom (pSub);

         // add it
         m_listBBSURF.Add (&bbs);
      }
      else if (!_wcsicmp(psz, gpszPath)) {
         m_sPath.MMLFrom (pSub);
      }
   }

   m_fDrawCounter = (BOOL) MMLValueGetInt (pNode, gpszDrawCounter, (int) 0);
   m_fDrawOnlyBottom = (BOOL) MMLValueGetInt (pNode, gpszDrawOnlyBottom, (int) 0);
   m_dwRoundEnd = (DWORD) MMLValueGetInt (pNode, gpszRoundEnd, (int) 0);
   m_dwShowWall = (DWORD) MMLValueGetInt (pNode, gpszShowWall, (int) 0);
   m_fBaseHeight = MMLValueGetDouble (pNode, gpszBaseHeight, 0);
   m_fCabinetDepth = MMLValueGetDouble (pNode, gpszCabinetDepth, 0);
   m_fCounterRounded = MMLValueGetDouble (pNode, gpszCounterRounded, 0);
   MMLValueGetPoint (pNode, gpszCounterOverhang, &m_pCounterOverhang);
   MMLValueGetPoint (pNode, gpszBaseIndent, &m_pBaseIndent);

   m_fHeight = MMLValueGetDouble (pNode, gpszHeight, 2);
   m_fCounterThick = MMLValueGetDouble (pNode, gpszRoofThickStruct, 0.1);
   m_fWallThick = MMLValueGetDouble (pNode, gpszWallThickStruct, 0.1);
   m_fShelfThick = MMLValueGetDouble (pNode, gpszFloorThickStruct, 0.1);

   m_fLevelHigher = MMLValueGetDouble (pNode, gpszLevelHigher, SHELFSPACING);
   m_fLevelMinDist = MMLValueGetDouble (pNode, gpszLevelMinDist, SHELFSPACING/2);
   WCHAR szTemp[64];
   for (i = 0; i < NUMLEVELS; i++) {
      swprintf (szTemp, gpszLevelElevation, (int) i);
      m_fLevelElevation[i] = MMLValueGetDouble (pNode, szTemp,
         (i ? m_fLevelElevation[i-1] : 0) + m_fLevelMinDist);
   }

   IntegrityCheck();

   return TRUE;
}


/*************************************************************************************
CObjectCabinet::IntegrityCheck - Just a check to make sure there aren't extraneous
surfaces around.
*/
void CObjectCabinet::IntegrityCheck (void)
{
#ifdef _DEBUG
   // jsut to be paranoid, go through all the claims and make sure they have
   // an accompanying surface
   DWORD dw, j, i;
   PBBSURF pbbs;
   for (i = 0; ;i++) {
      dw = ObjectSurfaceGetIndex(i);
      if (dw == (DWORD)-1)
         break;

      // find in surfaces
      for (j = 0; j < m_listBBSURF.Num(); j++) {
         pbbs = (PBBSURF) m_listBBSURF.Get(j);
         if (pbbs->pSurface->ClaimFindByID (dw))
            break;
      }
      if (j < m_listBBSURF.Num())
         continue;

      OutputDebugString ("ERROR: Unclaimed surface found.\r\n");
   };
   for (i = 0; ;i++) {
      dw = ContainerSurfaceGetIndex(i);
      if (dw == (DWORD)-1)
         break;

      for (j = 0; j < m_listBBSURF.Num(); j++) {
         pbbs = (PBBSURF) m_listBBSURF.Get(j);
         if (pbbs->pSurface->ClaimFindByID (dw))
            break;
      }
      if (j <= m_listBBSURF.Num())
         continue;

      OutputDebugString ("ERROR: Unclaimed constainer surface found.\r\n");
   };
#endif

}
/*************************************************************************************
CObjectCabinet::ControlPointQuery - Called to query the information about a control
point identified by dwID.

inputs
   DWORD       dwID - ID
   POSCONTROL  pInfo - Filled in with the infomration
returns
   BOOL - TRUE if successful
*/
BOOL CObjectCabinet::ControlPointQuery (DWORD dwID, POSCONTROL pInfo)
{
   if (dwID >= m_sPath.OrigNumPointsGet())
      return FALSE;

   memset (pInfo,0, sizeof(*pInfo));

   pInfo->dwID = dwID;
//   pInfo->dwFreedom = 0;   // any direction
   pInfo->dwStyle = CPSTYLE_SPHERE;
   pInfo->fSize = max(m_fCounterThick, m_fHeight/20);
   if (pInfo->fSize < CLOSE)
      pInfo->fSize = .01;

   pInfo->cColor = RGB(0xff,0,0xff);
   wcscpy (pInfo->szName, L"Cabinet bend");

   m_sPath.OrigPointGet (dwID, &pInfo->pLocation);
   pInfo->pLocation.p[2] = m_fHeight;

   return TRUE;
}

/*************************************************************************************
CObjectCabinet::ControlPointSet - Called to change the valud of a control point.

inputs
   DWORD       dwID - ID
   PCPoint     pVal - Contains the new location, in object coordinates
returns
   BOOL - TRUE if successful
*/
BOOL CObjectCabinet::ControlPointSet (DWORD dwID, PCPoint pVal, PCPoint pViewer)
{
   if (dwID >= m_sPath.OrigNumPointsGet ())
      return FALSE;

   // tell the world we're about to change
   if (m_pWorld)
      m_pWorld->ObjectAboutToChange (this);

   CPoint p;
   p.Copy (pVal);
   p.p[2] = 0;
   m_sPath.OrigPointReplace (dwID, &p);

   // recalc
   AdjustAllSurfaces ();

   // tell the world we've changed
   if (m_pWorld)
      m_pWorld->ObjectChanged (this);

   return TRUE;
}

/*************************************************************************************
CObjectCabinet::ControlPointEnum - Called to enumerate a list of control point IDs
into the list.

inputs
   PCListFixed       plDWORD - Should be filled in with control points
returns
   none
*/
void CObjectCabinet::ControlPointEnum (PCListFixed plDWORD)
{
   DWORD i;

   DWORD dwNum = m_sPath.OrigNumPointsGet();
   plDWORD->Required (plDWORD->Num() + dwNum);
   for (i = 0; i < dwNum; i++)
      plDWORD->Add (&i);
}

/**********************************************************************************
CObjectCabinet::DialogQuery - Returns TRUE if the object supports a custom
dialog box, FALSE if it doesn't. The default object doesn't
*/
BOOL CObjectCabinet::DialogQuery (void)
{
   return TRUE;
}




/* CabinetDialogPage
*/
BOOL CabinetDialogPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCObjectCabinet pv = (PCObjectCabinet)pPage->m_pUserData;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         WCHAR szTemp[32];
         DWORD i;

         MeasureToString (pPage, L"height", pv->m_fHeight);
         MeasureToString (pPage, L"cabinetdepth", pv->m_fCabinetDepth);
         MeasureToString (pPage, L"counterthick", pv->m_fCounterThick);
         MeasureToString (pPage, L"counterrounded", pv->m_fCounterRounded);
         MeasureToString (pPage, L"wallthick", pv->m_fWallThick);
         MeasureToString (pPage, L"baseheight", pv->m_fBaseHeight);
         MeasureToString (pPage, L"shelfthick", pv->m_fShelfThick);
         MeasureToString (pPage, L"levelhigher", pv->m_fLevelHigher);
         MeasureToString (pPage, L"levelmindist", pv->m_fLevelMinDist);
         for (i = 0; i < 4; i++) {
            swprintf (szTemp, L"counteroverhang%d", (int) i);
            MeasureToString (pPage, szTemp, pv->m_pCounterOverhang.p[i]);
         }
         for (i = 0; i < 4; i++) {
            swprintf (szTemp, L"baseindent%d", (int) i);
            MeasureToString (pPage, szTemp, pv->m_pBaseIndent.p[i]);
         }
         for (i = 0; i < NUMLEVELS; i++) {
            swprintf (szTemp, L"levelelevation%d", (int) i);
            MeasureToString (pPage, szTemp, pv->m_fLevelElevation[i]);
         }

         PCEscControl pControl;
         pControl = pPage->ControlFind  (L"drawcounter");
         if (pControl)
            pControl->AttribSetBOOL (Checked(), pv->m_fDrawCounter);
         pControl = pPage->ControlFind  (L"drawonlybottom");
         if (pControl)
            pControl->AttribSetBOOL (Checked(), pv->m_fDrawOnlyBottom);
         for (i = 0; i < 2; i++) {
            swprintf (szTemp, L"roundend%d", (int) i);
            pControl = pPage->ControlFind  (szTemp);
            if (pControl)
               pControl->AttribSetBOOL (Checked(), (pv->m_dwRoundEnd & (1<<i)) ? TRUE : FALSE);
         }
         for (i = 0; i < 4; i++) {
            swprintf (szTemp, L"showwall%d", (int) i);
            pControl = pPage->ControlFind  (szTemp);
            if (pControl)
               pControl->AttribSetBOOL (Checked(), (pv->m_dwShowWall & (1<<i)) ? TRUE : FALSE);
         }

      }
      break;

   case ESCN_EDITCHANGE:
      {
         // since will be one of the edit controls, get all the values

         pv->m_pWorld->ObjectAboutToChange (pv);

         WCHAR szTemp[32];
         DWORD i;

         MeasureParseString (pPage, L"height", &pv->m_fHeight);
         pv->m_fHeight = max(CLOSE, pv->m_fHeight);
         MeasureParseString (pPage, L"cabinetdepth", &pv->m_fCabinetDepth);
         pv->m_fCabinetDepth = max(CLOSE, pv->m_fCabinetDepth);
         MeasureParseString (pPage, L"counterthick", &pv->m_fCounterThick);
         pv->m_fCounterThick = max (CLOSE, pv->m_fCounterThick);
         MeasureParseString (pPage, L"counterrounded", &pv->m_fCounterRounded);
         pv->m_fCounterRounded = max(0,pv->m_fCounterRounded);
         MeasureParseString (pPage, L"wallthick", &pv->m_fWallThick);
         pv->m_fWallThick = max(CLOSE, pv->m_fWallThick);
         MeasureParseString (pPage, L"baseheight", &pv->m_fBaseHeight);
         pv->m_fBaseHeight = max(0,pv->m_fBaseHeight);
         MeasureParseString (pPage, L"shelfthick", &pv->m_fShelfThick);
         pv->m_fShelfThick = max(CLOSE, pv->m_fShelfThick);
         MeasureParseString (pPage, L"levelhigher", &pv->m_fLevelHigher);
         pv->m_fLevelHigher = max (.01, pv->m_fLevelHigher);
         MeasureParseString (pPage, L"levelmindist", &pv->m_fLevelMinDist);
         pv->m_fLevelMinDist = max (CLOSE, pv->m_fLevelMinDist);
         for (i = 0; i < 4; i++) {
            swprintf (szTemp, L"counteroverhang%d", (int) i);
            MeasureParseString (pPage, szTemp, &pv->m_pCounterOverhang.p[i]);
         }
         for (i = 0; i < 4; i++) {
            swprintf (szTemp, L"baseindent%d", (int) i);
            MeasureParseString (pPage, szTemp, &pv->m_pBaseIndent.p[i]);
         }
         for (i = 0; i < NUMLEVELS; i++) {
            swprintf (szTemp, L"levelelevation%d", (int) i);
            MeasureParseString (pPage, szTemp, &pv->m_fLevelElevation[i]);
         }

         pv->AdjustAllSurfaces ();
         pv->m_pWorld->ObjectChanged (pv);
      }
      break;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;
         PWSTR psz;
         WCHAR szTemp[32];
         DWORD i;
         psz = p->pControl->m_pszName;

         if (!_wcsicmp(psz, L"drawcounter")) {
            pv->m_pWorld->ObjectAboutToChange (pv);
            pv->m_fDrawCounter = p->pControl->AttribGetBOOL (Checked());
            pv->AdjustAllSurfaces ();
            pv->m_pWorld->ObjectChanged (pv);
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"drawonlybottom")) {
            pv->m_pWorld->ObjectAboutToChange (pv);
            pv->m_fDrawOnlyBottom = p->pControl->AttribGetBOOL (Checked());
            pv->AdjustAllSurfaces ();
            pv->m_pWorld->ObjectChanged (pv);
            return TRUE;
         }

         for (i = 0; i < 2; i++) {
            swprintf (szTemp, L"roundend%d", (int) i);
            if (!_wcsicmp(psz, szTemp)) {
               pv->m_pWorld->ObjectAboutToChange (pv);
               pv->m_dwRoundEnd = (pv->m_dwRoundEnd & (~(1 << i))) |
                  (p->pControl->AttribGetBOOL (Checked()) ? (1 << i) : 0);
               pv->AdjustAllSurfaces ();
               pv->m_pWorld->ObjectChanged (pv);
               return TRUE;
            }
         }

         for (i = 0; i < 4; i++) {
            swprintf (szTemp, L"showwall%d", (int) i);
            if (!_wcsicmp(psz, szTemp)) {
               pv->m_pWorld->ObjectAboutToChange (pv);
               pv->m_dwShowWall = (pv->m_dwShowWall & (~(1 << i))) |
                  (p->pControl->AttribGetBOOL (Checked()) ? (1 << i) : 0);
               pv->AdjustAllSurfaces ();
               pv->m_pWorld->ObjectChanged (pv);
               return TRUE;
            }
         }
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Cabinet settings";
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}


/* CabinetCurvePage
*/
BOOL CabinetCurvePage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCObjectCabinet pv = (PCObjectCabinet)pPage->m_pUserData;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         PCEscControl pControl;

         // draw images
         PCSpline ps;
         ps = &pv->m_sPath;
         if (ps) {
            NoodGenerateThreeDFromSpline (L"edgeaddremove", pPage, ps, ps, 0, FALSE);
            NoodGenerateThreeDFromSpline (L"edgecurve", pPage, ps, ps, 1, FALSE);

            pControl = pPage->ControlFind (L"looped");
            if (pControl)
               pControl->AttribSetBOOL (Checked(), ps->LoopedGet());

            DWORD dwMin, dwMax;
            fp fDetail;
            ps->DivideGet (&dwMin, &dwMax, &fDetail);
            ComboBoxSet (pPage, L"divide", dwMax);
         }
      }
      break;

   case ESCN_COMBOBOXSELCHANGE:
      {
         PESCNCOMBOBOXSELCHANGE p = (PESCNCOMBOBOXSELCHANGE) pParam;

         if (!p->pControl || !p->pControl->m_pszName)
            break;
         DWORD dwVal;
         dwVal = p->pszName ? (DWORD) _wtoi(p->pszName) : 0;
         PWSTR psz;
         psz = p->pControl->m_pszName;

         if (!_wcsicmp(psz, L"divide")) {
            PCSpline ps;
            ps = &pv->m_sPath;
            if (!ps)
               return TRUE;
            DWORD dwMin, dwMax;
            fp fDetail;
            ps->DivideGet (&dwMin, &dwMax, &fDetail);
            if (dwVal == dwMax)
               return TRUE;   // nothing to change

            // get all the points
            CMem  memPoints, memSegCurve;
            DWORD dwOrig;
            BOOL fLooped;
            if (!ps->ToMem (&fLooped, &dwOrig, &memPoints, NULL, &memSegCurve, &dwMin, &dwMax, &fDetail))
               return FALSE;

            pv->m_pWorld->ObjectAboutToChange (pv);
            pv->m_sPath.Init (pv->m_sPath.LoopedGet(), dwOrig,
               (PCPoint) memPoints.p, NULL, (DWORD*) memSegCurve.p, dwVal, dwVal, .1);
            pv->AdjustAllSurfaces();
            pv->m_pWorld->ObjectChanged (pv);
            return TRUE;
         }
      }

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

         // get all the points
         CMem  memPoints, memSegCurve;
         DWORD dwMinDivide, dwMaxDivide, dwOrig;
         fp fDetail;
         BOOL fLooped;
         PCSpline ps;
         ps = &pv->m_sPath;
         if (!ps)
            return FALSE;
         if (!ps->ToMem (&fLooped, &dwOrig, &memPoints, NULL, &memSegCurve, &dwMinDivide, &dwMaxDivide, &fDetail))
            return FALSE;

         // make sure have enough memory for extra
         if (!memPoints.Required ((dwOrig+1) * sizeof(CPoint)))
            return TRUE;
         if (!memSegCurve.Required ((dwOrig+1) * sizeof(DWORD)))
            return TRUE;

         // load it in
         PCPoint paPoints;
         DWORD *padw;
         paPoints = (PCPoint) memPoints.p;
         padw = (DWORD*) memSegCurve.p;

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

         pv->m_pWorld->ObjectAboutToChange (pv);
         //pv->m_dwDisplayControl = 3;   // front
         pv->m_sPath.Init (fLooped, dwOrig, paPoints, NULL, padw, dwMaxDivide, dwMaxDivide, .1);
         pv->AdjustAllSurfaces();
         pv->m_pWorld->ObjectChanged (pv);

         // redraw the shapes
         ps = &pv->m_sPath;
         if (ps) {
            NoodGenerateThreeDFromSpline (L"edgeaddremove", pPage, ps, ps, 0, FALSE);
            NoodGenerateThreeDFromSpline (L"edgecurve", pPage, ps, ps, 1, FALSE);
         }
      }
      break;


   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Cabinet curve";
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"IFUSELOOP")) {
            p->pszSubString = L"<comment>";
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"ENDIFUSELOOP")) {
            p->pszSubString = L"</comment>";
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}



/**********************************************************************************
CObjectCabinet::DialogShow - Causes the object to show a dialog box that allows
different attributes to be changed.

inputs
   DWORD             dwSuface - Surface that was clicked on. 0 if unknown.
   PCEscWindow       pWindow - Escarpment window to draw it in
returns
   BOOL - TRUE if user pressed "Back", FALSE if user pressed "Close"
*/

BOOL CObjectCabinet::DialogShow (DWORD dwSurface, PCEscWindow pWindow, PCPoint pClick)
{
   PWSTR pszRet;

mainpage:
   pszRet = pWindow->PageDialog (ghInstance, IDR_MMLCABINETDIALOG, CabinetDialogPage, this);
//firstpage2:
   if (!pszRet)
      return FALSE;
   if (!_wcsicmp(pszRet, L"custom")) {
      pszRet = pWindow->PageDialog (ghInstance, IDR_MMLNOODLECURVE, CabinetCurvePage, this);
      if (!pszRet)
         return FALSE;
      if (!_wcsicmp(pszRet, Back()))
         goto mainpage;
      // else fall through
   }

   return !_wcsicmp(pszRet, Back());
}



/**********************************************************************************
CObjectCabinet::ContHVQuery
Called to see the HV location of what clicked on, or in other words, given a line
from pEye to pClick (object sapce), where does it intersect surface dwSurface (LOWORD(PIMAGEPIXEL.dwID))?
Fills pHV with the point. Returns FALSE if doesn't intersect, or invalid surface, or
can't tell with that surface. NOTE: if pOld is not NULL, then assume a drag from
pOld (converted to world object space) to pClick. Finds best intersection of the
surface then. Useful for dragging embedded objects around surface
*/
BOOL CObjectCabinet::ContHVQuery (PCPoint pEye, PCPoint pClick, DWORD dwSurface, PTEXTUREPOINT pOld, PTEXTUREPOINT pHV)
{
   // cycle through all the surfaces asking them this question
   DWORD i;
   PBBSURF pbbs;
   for (i = 0; i < m_listBBSURF.Num(); i++) {
      pbbs = (PBBSURF) m_listBBSURF.Get(i);
      if (pbbs->pSurface->ContHVQuery (pEye, pClick, dwSurface, pOld, pHV))
         return TRUE;
   }
   return FALSE;
}


/**********************************************************************************
CObjectTemplate::ContCutout
Called by an embeded object to specify an arch cutout within the surface so the
object can go through the surface (like a window). The container should check
that pgEmbed is a valid object. dwNum is the number of points in the arch,
paFront and paBack are the container-object coordinates. (See CSplineSurface::ArchToCutout)
If dwNum is 0 then arch is simply removed. If fBothSides is true then both sides
of the surface are cleared away, else only the side where the object is embedded
is affected.

*/
BOOL CObjectCabinet::ContCutout (const GUID *pgEmbed, DWORD dwNum, PCPoint paFront, PCPoint paBack, BOOL fBothSides)
{
   // cycle through all the surfaces asking them this question
   DWORD i;
   PBBSURF pbbs;
   for (i = 0; i < m_listBBSURF.Num(); i++) {
      pbbs = (PBBSURF) m_listBBSURF.Get(i);
      if (pbbs->pSurface->ContCutout (pgEmbed, dwNum, paFront, paBack, fBothSides))
         return TRUE;
   }
   return FALSE;
}

/**********************************************************************************
CObjectCabinet::ContCutoutToZipper -
Assumeing that ContCutout() has already been called for the pgEmbed, this askes the surfaces
for the zippering information (CRenderSufrace::ShapeZipper()) that would perfectly seal up
the cutout. plistXXXHVXYZ must be initialized to sizeof(HVXYZ). plisstFrontHVXYZ is filled with
the points on the side where the object was embedded, and plistBackHVXYZ are the opposite side.
NOTE: These are in the container's object space, NOT the embedded object's object space.
*/
BOOL CObjectCabinet::ContCutoutToZipper (const GUID *pgEmbed, PCListFixed plistFrontHVXYZ, PCListFixed plistBackHVXYZ)
{
   // cycle through all the surfaces asking them this question
   DWORD i;
   PBBSURF pbbs;
   for (i = 0; i < m_listBBSURF.Num(); i++) {
      pbbs = (PBBSURF) m_listBBSURF.Get(i);
      if (pbbs->pSurface->ContCutoutToZipper (pgEmbed, plistFrontHVXYZ, plistBackHVXYZ))
         return TRUE;
   }
   return FALSE;
}


/**********************************************************************************
CObjectCabinet::ContThickness - 
returns the thickness of the surface (dwSurface) at pHV. Used by embedded
objects like windows to know how deep they should be.

NOTE: usually overridden
*/
fp CObjectCabinet::ContThickness (DWORD dwSurface, PTEXTUREPOINT pHV)
{
   // cycle through all the surfaces asking them this question
   DWORD i;
   PBBSURF pbbs;
   for (i = 0; i < m_listBBSURF.Num(); i++) {
      pbbs = (PBBSURF) m_listBBSURF.Get(i);
      fp fThickness;
      fThickness = pbbs->pSurface->ContThickness (dwSurface, pHV);
      if (fThickness)
         return fThickness;
   }
   return 0;
}


/**********************************************************************************
CObjectCabinet::ContSideInfo -
returns a DWORD indicating what class of surface it is... right now
0 = internal, 1 = external. dwSurface is the surface. If fOtherSide
then want to know what's on the opposite side. Returns 0 if bad dwSurface.

NOTE: usually overridden
*/
DWORD CObjectCabinet::ContSideInfo (DWORD dwSurface, BOOL fOtherSide)
{
   // cycle through all the surfaces asking them this question
   DWORD i;
   PBBSURF pbbs;
   for (i = 0; i < m_listBBSURF.Num(); i++) {
      pbbs = (PBBSURF) m_listBBSURF.Get(i);
      DWORD dw;
      dw = pbbs->pSurface->ContSideInfo (dwSurface, fOtherSide);
      if (dw)
         return dw;
   }
   return 0;
}


/**********************************************************************************
CObjectCabinet::ContMatrixFromHV - THE SUPERCLASS SHOULD OVERRIDE THIS. Given
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
BOOL CObjectCabinet::ContMatrixFromHV (DWORD dwSurface, PTEXTUREPOINT pHV, fp fRotation, PCMatrix pm)
{
   // cycle through all the surfaces asking them this question
   DWORD i;
   PBBSURF pbbs;
   for (i = 0; i < m_listBBSURF.Num(); i++) {
      pbbs = (PBBSURF) m_listBBSURF.Get(i);
      if (pbbs->pSurface->ContMatrixFromHV (dwSurface, pHV, fRotation, pm)) {
         // then rotate by the object's rotation matrix to get to world space
         pm->MultiplyRight (&m_MatrixObject);
         return TRUE;
      }
   }
   return FALSE;
}



/*************************************************************************************
ClipClear - Clear out clipping information for the given PCDoubleSurface. Gets rid of
   gpszCabinetClear();

inputs
   PCDoubleSurface   pSurface - surface
*/
static void ClipClear (PCDoubleSurface pSurface)
{
   int iSide;
   PCSplineSurface pss;
   DWORD j;
   for (iSide = -1; iSide <= 1; iSide += 2) {  // -1 for side B, 1 for for side A, 0 for center
      pss = (iSide == 1) ? &pSurface->m_SplineA : &pSurface->m_SplineB;

      // clear out the side's cutouts
      for (j = pss->CutoutNum()-1; j < pss->CutoutNum(); j--) {
         PWSTR pszName;
         PTEXTUREPOINT ptp;
         DWORD dwNum;
         BOOL fClockwise;
         if (!pss->CutoutEnum(j, &pszName, &ptp, &dwNum, &fClockwise))
            continue;
         if (!_wcsnicmp (pszName, gpszCabinetClip, wcslen(gpszCabinetClip)))
            pss->CutoutRemove (pszName);
      }
   }
}


/**********************************************************************************
CObjectCabinet::
Called to tell the container that one of its contained objects has been renamed.
*/
BOOL CObjectCabinet::ContEmbeddedRenamed (const GUID *pgOld, const GUID *pgNew)
{
   // cycle through all the surfaces asking them this question
   DWORD i;
   PBBSURF pbbs;
   for (i = 0; i < m_listBBSURF.Num(); i++) {
      pbbs = (PBBSURF) m_listBBSURF.Get(i);

      // call this several times because need to make sure all bits are hit
      pbbs->pSurface->ContEmbeddedRenamed (pgOld, pgNew);
   }
   return FALSE;
}


/*************************************************************************************
CObjectCabinet::Deconstruct -
Tells the object to deconstruct itself into sub-objects.
Basically, new objects will be added that exactly mimic this object,
and any embedeeding objects will be moved to the new ones.
NOTE: This old one will not be deleted - the called of Deconstruct()
will need to call Delete()
If fAct is FALSE the function is just a query, that returns
TRUE if the object cares about adjustment and can try, FALSE if it can't.

NOTE: Often overridden.
*/
BOOL CObjectCabinet::Deconstruct (BOOL fAct)
{
   if (!m_pWorld)
      return FALSE;
   if (!fAct)
      return TRUE;

   // notify world of object changing
   m_pWorld->ObjectAboutToChange (this);

   // loop through all the surfaces... create them and clone them
   DWORD i, j;
   for (i = 0; i < m_listBBSURF.Num(); i++) {
      PBBSURF ps = (PBBSURF) m_listBBSURF.Get(i);

      // dont do hidden surfaces
      if (ps->fHidden)
         continue;

      // create a new one
      PCObjectStructSurface pss;
      OSINFO OI;
      memset (&OI, 0, sizeof(OI));
      OI.gCode = CLSID_StructSurface;
      OI.gSub = CLSID_StructSurfaceExternalStudWall;
      OI.dwRenderShard = m_OSINFO.dwRenderShard;
      pss = new CObjectStructSurface((PVOID) 0x1000d, &OI);
      if (!pss)
         continue;

      // BUGFIX - Set a name
      pss->StringSet (OSSTRING_NAME, L"Surface from cabinet");

      m_pWorld->ObjectAdd (pss);

      m_pWorld->ObjectAboutToChange (pss);

      // remove any colors or links in the new object
      while (TRUE) {
         j = pss->ContainerSurfaceGetIndex (0);
         if (j == (DWORD)-1)
            break;
         pss->ContainerSurfaceRemove (j);
      }
      while (TRUE) {
         j = pss->ObjectSurfaceGetIndex (0);
         if (j == (DWORD)-1)
            break;
         pss->ObjectSurfaceRemove (j);
      }

      // clone the surfaces
      ps->pSurface->ClaimCloneTo (&pss->m_ds, pss);

      // imbue it with new information
      ps->pSurface->CloneTo (&pss->m_ds, pss);

      // set the matrix
      CMatrix m;
      m.Identity();
      pss->m_ds.MatrixSet (&m);
      ps->pSurface->MatrixGet(&m);
      m.MultiplyRight (&m_MatrixObject);
      pss->ObjectMatrixSet (&m);

      // move embedded objects over that are embedded in this one
      while (TRUE) {
         for (j = 0; j < ContEmbeddedNum(); j++) {
            GUID gEmbed;
            TEXTUREPOINT pHV;
            fp fRotation;
            DWORD dwSurface;
            if (!ContEmbeddedEnum (j, &gEmbed))
               continue;
            if (!ContEmbeddedLocationGet (&gEmbed, &pHV, &fRotation, &dwSurface))
               continue;

            if (!ps->pSurface->ClaimFindByID (dwSurface))
               continue;

            // else, it's on this surface, so remove it from this object and
            // move it to the new one
            ContEmbeddedRemove (&gEmbed);
            pss->ContEmbeddedAdd (&gEmbed, &pHV, fRotation, dwSurface);

            // set j=0 just to make sure repeat
            j = 0;
            break;
         }

         // keep repeating until no more objects embedded in this surface
         if (j >= ContEmbeddedNum())
            break;
      }

      // eventually need to shorten walls if clones walls are clipped
      // and dont use extra
      // BUGFIX - Disable this because causes problems with all the rounded bits
      // on the end, and messes up the textures
      //pss->m_ds.ShrinkToCutouts (FALSE);

      // note changed
      m_pWorld->ObjectChanged (pss);
   }

   m_pWorld->ObjectChanged (this);
   return TRUE;
}

/**********************************************************************************
CObjectCabinet::WishListAdd - Adds an item (wall, roof, floor) to the wish
list of objects that are to be used in the building block. What this does is:
   1) If the object doesn't exist already it's created.
   2) All settings in the object are changed based on the wish list.

NOTE: Need to do the following before using:
   a) clear m_listWish

inputs
   PBBSURF        pSurf - What surface is desired
returns
   PBBSURF - Surface as is appears in the final list. This includes a pointer to
      the CDoubleSurface object
*/
PBBSURF CObjectCabinet::WishListAdd (PBBSURF pSurf)
{
   BOOL fChanged;
   fChanged = FALSE;

   // create any that don't exist but which should
   PBBSURF pLookFor, pLookAt;
   DWORD j;
   BBSURF bbs;
   pLookFor = pSurf;
   for (j = 0; j < m_listBBSURF.Num(); j++) {
      pLookAt = (PBBSURF) m_listBBSURF.Get(j);
      if ((pLookFor->dwMajor == pLookAt->dwMajor) && (pLookFor->dwMinor == pLookAt->dwMinor))
         break;
   }
   if (j >= m_listBBSURF.Num()) {
      // world about to change
      if (!fChanged && m_pWorld) {
         m_pWorld->ObjectAboutToChange (this);
         fChanged = TRUE;
      }

      // couldn't find, so add

      // copy over, but set some values so that the object will be properly
      // moved
      bbs = *pLookFor;
      bbs.fHeight = -1;
      bbs.fRotX = 0;
      bbs.fRotZ = 0;
      bbs.fRotY = 0;
      bbs.fWidth = -1;
      bbs.pTrans.Zero();
      bbs.fBevelBottom = bbs.fBevelTop = bbs.fBevelLeft = bbs.fBevelRight = 0;
      
      // create
      bbs.pSurface = new CDoubleSurface;
      if (!bbs.pSurface)
         return NULL;   // error
      bbs.pSurface->Init (m_OSINFO.dwRenderShard, bbs.dwType, this);
      
      // add it
      pLookAt = (PBBSURF) m_listBBSURF.Get(m_listBBSURF.Add (&bbs));
      if (!pLookAt)
         return NULL;   // error
      pLookAt->fChanged = TRUE;
   }
   else
      pLookAt->fChanged = FALSE;

   // remember the surface in the wish list since use this later
   pSurf->pSurface = pLookAt->pSurface;
   // add to wish list
   m_listWish.Add (pSurf);


   // if get here, pLookAt is filled with what's really there, and pLookFor
   // is what we want. If these differ then fix the problem

   // remember the setting for not including in volume
   // Do this, because if first create as perimiter (which will have as TRUE) and 
   // convert to basement (which will have as false) then doesn't use basement walls.
   pLookAt->fDontIncludeInVolume = pLookFor->fDontIncludeInVolume;
   pLookAt->fKeepClip = pLookFor->fKeepClip;
   pLookAt->pKeepA = pLookFor->pKeepA;
   pLookAt->pKeepB = pLookFor->pKeepB;
   pLookAt->fKeepLargest = pLookFor->fKeepLargest;
   pLookAt->dwFlags = pLookFor->dwFlags;

   // if the curvature is non-linear then need to resize anyway
   BOOL fNonLinear;
   fNonLinear = FALSE;
   DWORD i;
   for (i = 0; i < pLookAt->pSurface->ControlNumGet(TRUE)-2; i++)
      if (pLookAt->pSurface->SegCurveGet(TRUE, i) != SEGCURVE_LINEAR)
         fNonLinear = TRUE;
   for (i = 0; i < pLookAt->pSurface->ControlNumGet(FALSE)-2; i++)
      if (pLookAt->pSurface->SegCurveGet(FALSE, i) != SEGCURVE_LINEAR)
         fNonLinear = TRUE;

   // If different sizes deal with that.
   if (fNonLinear || (pLookAt->fWidth != pLookFor->fWidth) || (pLookAt->fHeight != pLookFor->fHeight)) {
      // world about to change
      if (!fChanged && m_pWorld) {
         m_pWorld->ObjectAboutToChange (this);
         fChanged = TRUE;
      }
      pLookAt->fChanged = TRUE;

      // remember old height and width
      fp fOldHeight, fOldWidth;
      fOldHeight = pLookAt->fHeight;
      fOldWidth = pLookAt->fWidth;
      pLookAt->dwStretchHInfo = pLookFor->dwStretchHInfo; // copy over from the widh
      pLookAt->dwStretchVInfo = pLookFor->dwStretchVInfo;
      if ((fOldHeight < 0) || (fOldHeight == pLookFor->fHeight))
         pLookAt->dwStretchVInfo = 0;  // no change
      if ((fOldWidth < 0) || (fOldWidth == pLookFor->fWidth))
         pLookAt->dwStretchHInfo = 0;  // no change

      // adjust the width and height
      pLookAt->fWidth = pLookFor->fWidth;
      pLookAt->fHeight = pLookFor->fHeight;


      // If resize, what do with overlays and embedded objects since HV
      // will change for them?
      fp fHOrig, fVOrig, fHNew, fVNew;
      switch (pLookAt->dwStretchHInfo) {
      case 1:  // stretch/shrink on the left
         if (pLookAt->fWidth > fOldWidth) {
            // expanded it
            fHOrig = EPSILON;
            fHNew = 1 - fOldWidth / pLookAt->fWidth;
         }
         else {
            // shrunk it
            fHOrig = 1 - pLookAt->fWidth / fOldWidth;
            fHNew = EPSILON;
         }
         break;

      case 2:  // stretch/shrink on the right
         if (pLookAt->fWidth > fOldWidth) {
            // expanded it
            fHOrig = 1 - EPSILON;
            fHNew = fOldWidth / pLookAt->fWidth;
         }
         else {
            // shrunk it
            fHOrig = pLookAt->fWidth / fOldWidth;
            fHNew = 1 - EPSILON;
         }
         break;
      }

      switch (pLookAt->dwStretchVInfo) {
      case 1:  // stretch/shrink on the top
         if (pLookAt->fHeight > fOldHeight) {
            // expanded it
            fVOrig = EPSILON;
            fVNew = 1 - fOldHeight / pLookAt->fHeight;
         }
         else {
            // shrunk it
            fVOrig = 1 - pLookAt->fHeight / fOldHeight;
            fVNew = EPSILON;
         }
         break;

      case 2:  // stretch/shrink on the bottom
         if (pLookAt->fHeight > fOldHeight) {
            // expanded it
            fVOrig = 1 - EPSILON;
            fVNew = fOldHeight / pLookAt->fHeight;
         }
         else {
            // shrunk it
            fVOrig = pLookAt->fHeight / fOldHeight;
            fVNew = 1 - EPSILON;
         }
         break;
      }


      pLookAt->pSurface->NewSize (pLookAt->fWidth, pLookAt->fHeight,
            pLookAt->dwStretchHInfo ? 1 : 0,
            pLookAt->dwStretchVInfo ? 1 : 0,
            &fHOrig, &fVOrig, &fHNew, &fVNew);
      // Because called NewSize() all the embedded objects have already been
      // told that they've moved

   }

   // If different rotation than deal with that
   if ((pLookAt->pTrans.p[0] != pLookFor->pTrans.p[0]) || (pLookAt->pTrans.p[1] != pLookFor->pTrans.p[1]) ||
      (pLookAt->pTrans.p[2] != pLookFor->pTrans.p[2]) || (pLookAt->fRotX != pLookFor->fRotX) ||
      (pLookAt->fRotZ != pLookFor->fRotZ) || (pLookAt->fRotY != pLookFor->fRotY)) {

      // world about to change
      if (!fChanged && m_pWorld) {
         m_pWorld->ObjectAboutToChange (this);
         fChanged = TRUE;
      }
      pLookAt->fChanged = TRUE;

      pLookAt->pTrans.Copy (&pLookFor->pTrans);
      pLookAt->fRotX = pLookFor->fRotX;
      pLookAt->fRotY = pLookFor->fRotY;
      pLookAt->fRotZ = pLookFor->fRotZ;

      CMatrix m;
      m.FromXYZLLT (&pLookAt->pTrans, pLookAt->fRotZ, pLookAt->fRotX, pLookAt->fRotY);
      pLookAt->pSurface->MatrixSet (&m);
      //CMatrix mRotX, mRotZ, mTrans;
      //mRotX.RotationX (pLookAt->fRotX);
      //mRotZ.RotationZ (pLookAt->fRotZ);
      //mTrans.Translation (pLookAt->pTrans.p[0], pLookAt->pTrans.p[1], pLookAt->pTrans.p[2]);
      //mTrans.MultiplyLeft (&mRotZ);
      //mTrans.MultiplyLeft (&mRotX);

      //pLookAt->pSurface->MatrixSet (&mTrans);

      // Because called MatrixSet, all the embedded objects are already
      // told that they've moved
   }

   // If different bevelling or thickness then deal with that
   if ((pLookFor->fBevelTop != pLookAt->pSurface->m_fBevelTop) ||
      (pLookFor->fBevelBottom != pLookAt->pSurface->m_fBevelBottom) ||
      (pLookFor->fBevelLeft != pLookAt->pSurface->m_fBevelLeft) ||
      (pLookFor->fBevelRight != pLookAt->pSurface->m_fBevelRight) ||
      (pLookFor->fThickStud != pLookAt->pSurface->m_fThickStud) ||
      (pLookFor->fThickSideA != pLookAt->pSurface->m_fThickA) ||
      (pLookFor->fThickSideB != pLookAt->pSurface->m_fThickB) ||
      (pLookFor->fHideEdges != pLookAt->pSurface->m_fHideEdges)
      ) {


      // world about to change
      if (!fChanged && m_pWorld) {
         m_pWorld->ObjectAboutToChange (this);
         fChanged = TRUE;
      }
      pLookAt->fChanged = TRUE;

      pLookAt->pSurface->m_fBevelTop = pLookFor->fBevelTop;
      pLookAt->pSurface->m_fBevelBottom = pLookFor->fBevelBottom;
      pLookAt->pSurface->m_fBevelLeft = pLookFor->fBevelLeft;
      pLookAt->pSurface->m_fBevelRight = pLookFor->fBevelRight;

      pLookAt->pSurface->m_fThickStud = pLookFor->fThickStud;
      pLookAt->pSurface->m_fThickA = pLookFor->fThickSideA;
      pLookAt->pSurface->m_fThickB = pLookFor->fThickSideB;

      pLookAt->pSurface->m_fHideEdges = pLookFor->fHideEdges;

      pLookAt->pSurface->RecalcSides();
      pLookAt->pSurface->RecalcEdges();

      // Don't need to tell objects they've moved because automagically
      // done by RecalcSides()
   }

   // Hide or not hide
   if (pLookAt->fHidden != pLookFor->fHidden) {
      if (!fChanged && m_pWorld) {
         m_pWorld->ObjectAboutToChange (this);
         fChanged = TRUE;
      }
      pLookAt->fChanged = TRUE;

      pLookAt->fHidden = pLookFor->fHidden;
   }
   // clear out the surface of bounding box clipping that had done
   ClipClear (pLookAt->pSurface);

   if (fChanged && m_pWorld) {
      m_pWorld->ObjectChanged (this);
   }

   return pLookAt;
}


/**********************************************************************************
ExtendLine - Extends a linear section from the line formed by p1 and p2.

inputs
   PCPoint     p1,p2 - Sequence that defines line
   fp      fExtend - Amount to extend by
   PCPoint     pExtend - Filled with the extension of p2 by fExtend meters.
retursn
   BOOL - TRUE if succcess
*/
static BOOL ExtendLine (PCPoint p1, PCPoint p2, fp fExtend, PCPoint pExtend)
{
   CPoint pDelta;
   pDelta.Subtract (p2, p1);
   pDelta.Normalize();
   pDelta.Scale (fExtend);
   pExtend->Add (p2, &pDelta);
   return TRUE;
}

/**********************************************************************************
ExtendCircle - Given three points that form a circle, p1..p3, this extends it
by fExtend (approximtely) meters. The p3 point is extended and written into
pExtend.

inputs
   PCPoint     p1,p2,p3 - Three sequenctial points that define a circle
   fp      fExtend - Number of meters (approximately) to extend by
   PCPoint     pExtend - Filled with the extended value for p3 (if returns true)
returns
   BOOL - TRUE if success, FALSE if failure
*/
static BOOL ExtendCircle (PCPoint p1, PCPoint p2, PCPoint p3, fp fExtend, PCPoint pExtend)
{
   // calcualte up
   CPoint pUp, pH, pV, pC;
   pC.Copy (p2);
   pH.Subtract (p3, &pC);
   pV.Subtract (p1, &pC);
   pUp.CrossProd (&pH, &pV);
   pUp.Normalize();
   pH.Normalize();
   pV.CrossProd (&pUp, &pH);

   // convert the three points into HV space
   CPoint t1, t2, t3;
   CPoint pTemp;
   t1.Zero();
   pTemp.Subtract (p1, &pC);
   t1.p[0] = pTemp.DotProd (&pH);
   t1.p[1] = pTemp.DotProd (&pV);
   t2.Zero();   // since p2 is used as the center point
   t3.Zero();
   pTemp.Subtract (p3, &pC);
   t3.p[0] = pTemp.DotProd (&pH);
   t3.p[1] = pTemp.DotProd (&pV);
   CPoint pHVUp;
   pHVUp.Zero();
   pHVUp.p[2] = 1;

   // create two lines, line 1 bisects p1 and p2, and line 2 biescts
   // p2 and p3. The slope of the lines is perpendicular to the
   // line p1->p2, and p2->p3. pLineP is the line anchor, pLineQ is the multiplied by alpha
   CPoint pLineP1, pLineP2, pLineQ1, pLineQ2;
   pLineP1.Subtract (&t1, &t2);
   pLineQ1.CrossProd (&pLineP1, &pHVUp);
   pLineP1.Add (&t1, &t2);
   pLineP1.Scale(.5);
   pLineQ1.Add (&pLineP1);

   // change of plans - treat the second one like a plane and intersect
   // the line with the plane, using algorithm already have
   pLineQ2.Subtract (&t3, &t2);  // normal to plane
   pLineP2.Add (&t3, &t2);
   pLineP2.Scale(.5);   // point in plane
   CPoint pIntersect;
   if (!IntersectLinePlane (&pLineP1, &pLineQ1, &pLineP2, &pLineQ2, &pIntersect)) {
      // if they don't intersect then dont have a circle, so pretend its a line
      return FALSE;
   }

   // calculate radius
   fp fRadius;
   pTemp.Subtract (&t2, &pIntersect);
   fRadius = pTemp.Length();
   if (fabs(fRadius) < EPSILON)
      return FALSE;

   // calculate the angle at the 2 points averaging
   // add two-pi so that know its a positive number
   fp fAngle1, fAngle2;
   fAngle1 = atan2((t2.p[0] - pIntersect.p[0]), (t2.p[1] - pIntersect.p[1]));
   fAngle2 = atan2((t3.p[0] - pIntersect.p[0]), (t3.p[1] - pIntersect.p[1]));
   // below was used in case of segcurvenext
   // fAngle1 = atan2((t1.p[0] - pIntersect.p[0]), (t1.p[1] - pIntersect.p[1]));
   // fAngle2 = atan2((t2.p[0] - pIntersect.p[0]), (t2.p[1] - pIntersect.p[1]));
   if (fAngle1 < fAngle2)
      fAngle1 += 2 * PI;

   // if the radius were 1, then the circmfrance would be 2 pi r - which means
   // that divide the extend amount by radius to get the amount of angle to change
   fExtend /= fRadius;

   fp fAngle;
   fAngle = fAngle2 - fExtend;

   // determine location in space
   fp h, v;
   h = sin (fAngle) * fRadius + pIntersect.p[0];
   v = cos (fAngle) * fRadius + pIntersect.p[1];
   CPoint pNew;
   pNew.Copy (&pH);
   pNew.Scale (h);
   pTemp.Copy (&pV);
   pTemp.Scale (v);
   pNew.Add (&pTemp);
   pNew.Add (&pC);

   pExtend->Copy (&pNew);
   return TRUE;
}

/**********************************************************************************
CObjectCabinet::WishListAddCurved - Adds a curved surface (sphere only) to the
wish list. It does this by pretending its a flat surface (using the edges) and calling
WishListAddFlat. Then, with what that returns it translates the other points and
creates a new surface with that. It's not too efficient, but will probably reduce the
number of bugs.

inputs
   PBBSURF        pSurf - What surface is desired
   DWORD          dwH, dwV - Number of control points horizontal and vertical
   PCPoint        papControl - An array of control points: Aranged as [0..dwV-1][0..dwH-1].
                     (Points in object space)
   DWORD          *padwSegCurveH, *padwSegCurveV - dwH-1 and dwV-1 elements. of SEGCURVE_XXX
   DWORD          dwFlags - Extra settings. See WishListAdd
returns
   PBBSURF - Surface as it appears in the final list. Like WishListAdd() return.
*/
#undef MESH
#define  MESH(x,y)   (papControl[(x)+(y)*(dwH)])
PBBSURF CObjectCabinet::WishListAddCurved (PBBSURF pSurf, DWORD dwH, DWORD dwV,
   PCPoint papControl, DWORD *padwSegCurveH, DWORD *padwSegCurveV, DWORD dwFlags)
{
   PBBSURF pAdd;
   DWORD i;
   pAdd = WishListAddFlat (pSurf, &MESH(0,dwV-1), &MESH(dwH-1,dwV-1), &MESH(0,0), &MESH(dwH-1,0),0);
   if (!pAdd)
      return NULL;

   PCDoubleSurface ps;
   ps = pAdd->pSurface;

   // keep flags indicating that flat
   ps->m_fConstAbove = FALSE;
   ps->m_fConstBottom = FALSE;
   ps->m_fConstRectangle = FALSE;

   // transform all the points into array
   CMem mem;
   if (!mem.Required(dwH*dwV*sizeof(CPoint)))
      return pAdd;
   memcpy (mem.p, papControl, dwH * dwV * sizeof(CPoint));
   papControl = (PCPoint) mem.p;
   CMatrix m, mInv;
   ps->MatrixGet(&m);
   m.Invert4 (&mInv);
   for (i = 0; i < dwH*dwV; i++) {
      papControl[i].p[3] = 1;
      papControl[i].MultiplyLeft (&mInv);
   }

   // Will need to extend
   fp fOverlap;
   fOverlap = (pSurf->fThickSideA + pSurf->fThickSideB + pSurf->fThickStud) * 2;
   if (dwFlags & 0x10)
      fOverlap *= 4;

   // left/right
   BOOL fRet;
   CPoint pTemp;
   for (i = 0; i < dwV; i++) {
      if (dwFlags & 0x01) {
         //extending left
         switch (padwSegCurveH[0]) {
         case SEGCURVE_LINEAR:
            fRet = ExtendLine (&MESH(1,i), &MESH(0,i), fOverlap, &pTemp);
            break;
         case SEGCURVE_CIRCLENEXT:
            if (dwH >= 3)
               fRet = ExtendCircle (&MESH(2,i), &MESH(1,i), &MESH(0,i), fOverlap, &pTemp);
            else
               fRet = ExtendLine (&MESH(1,i), &MESH(0,i), fOverlap, &pTemp);
            break;
         default:
            fRet = FALSE;
         }
         if (fRet)
            MESH(0,i).Copy (&pTemp);
      }
      if (dwFlags & 0x02) {
         //extending right
         switch (padwSegCurveH[dwH-2]) {
         case SEGCURVE_LINEAR:
            fRet = ExtendLine (&MESH(dwH-2,i), &MESH(dwH-1,i), fOverlap, &pTemp);
            break;
         case SEGCURVE_CIRCLEPREV:
            if (dwH >= 3)
               fRet = ExtendCircle (&MESH(dwH-3,i), &MESH(dwH-2,i), &MESH(dwH-1,i), fOverlap, &pTemp);
            else
               fRet = ExtendLine (&MESH(dwH-2,i), &MESH(dwH-1,i), fOverlap, &pTemp);
            break;
         default:
            fRet = FALSE;
         }
         if (fRet)
            MESH(dwH-1,i).Copy (&pTemp);
      }
   }

   for (i = 0; i < dwH; i++) {
      if (dwFlags & 0x04) {
         //extending top
         switch (padwSegCurveV[0]) {
         case SEGCURVE_LINEAR:
            fRet = ExtendLine (&MESH(i,1), &MESH(i,0), fOverlap, &pTemp);
            break;
         case SEGCURVE_CIRCLENEXT:
            if (dwV >= 3)
               fRet = ExtendCircle (&MESH(i,2), &MESH(i,1), &MESH(i,0), fOverlap, &pTemp);
            else
               fRet = ExtendLine (&MESH(i,1), &MESH(i,0), fOverlap, &pTemp);
            break;
         default:
            fRet = FALSE;
         }
         if (fRet)
            MESH(i,0).Copy (&pTemp);
      }
      if (dwFlags & 0x08) {
         //extending bottom
         switch (padwSegCurveV[dwV-2]) {
         case SEGCURVE_LINEAR:
            fRet = ExtendLine (&MESH(i,dwV-2), &MESH(i,dwV-1), fOverlap, &pTemp);
            break;
         case SEGCURVE_CIRCLEPREV:
            if (dwV >= 3)
               fRet = ExtendCircle (&MESH(i,dwV-3), &MESH(i,dwV-2), &MESH(i,dwV-1), fOverlap, &pTemp);
            else
               fRet = ExtendLine (&MESH(i,dwV-2), &MESH(i,dwV-1), fOverlap, &pTemp);
            break;
         default:
            fRet = FALSE;
         }
         if (fRet)
            MESH(i, dwV-1).Copy (&pTemp);
      }
   }

   // write them out
   ps->ControlPointsSet (dwH, dwV, papControl, padwSegCurveH, padwSegCurveV, 3);

   return pAdd;
}

/**********************************************************************************
CObjectCabinet::WishListAddCurved - Adds a curved surface based on a spline

inputs
   PBBSURF        pSurf - What surface is desired
   PCSpline       pSpline - Spline that runs over the horizontal
   fp             fBottom - Bottom Z
   fp             fTop - TOp Z
   DWORD          dwFlags - Extra settings. See WishListAdd
returns
   PBBSURF - Surface as it appears in the final list. Like WishListAdd() return.
*/
PBBSURF CObjectCabinet::WishListAddCurved (PBBSURF pSurf, PCSpline pSpline,
                                           fp fBottom, fp fTop, DWORD dwFlags)
{
   CMem memPoint, memCurve;
   PCPoint pPoint;
   DWORD *padwCurve;
   DWORD dwH, dwV, x, y, i;
   dwV = 2;
   dwH = pSpline->OrigNumPointsGet();
   if (!memPoint.Required (dwH * dwV * sizeof(CPoint)))
      return NULL;
   if (!memCurve.Required (dwH * sizeof(DWORD)))
      return NULL;
   pPoint = (PCPoint) memPoint.p;
   padwCurve = (DWORD*) memCurve.p;

   // fill in data
   for (y = 0; y < dwV; y++) for (x = 0; x < dwH; x++) {
      pSpline->OrigPointGet (x, &pPoint[y * dwH + x]);
      pPoint[y * dwH + x].p[2] = (y ? fBottom : fTop);
   }
   for (x = 0; x < dwH; x++)
      pSpline->OrigSegCurveGet (x, padwCurve + x);

   // pass it on
   i = SEGCURVE_LINEAR;
   return WishListAddCurved (pSurf, dwH, dwV, pPoint, padwCurve, &i, dwFlags);
}

/**********************************************************************************
CObjectCabinet::WishListAddFlat - Adds an item (wall, roof, floor) to wish
list like in WishListAdd(). The only difference is that this takes 2 base points (bottom
of roof) and 1 or 2 upper points (peak of roof). It then calculates the rotation and size4
based on that and stores them in pSurf. It calls WishListAdd. If there's any bend
in the surface then the object is modified to account for the bend.

inputs
   PBBSURF        pSurf - What surface is desired
   PCPoint        pBase1, pBase2 - Two base points
   PCPoint        pTop1, pTop2 - pTop2 can be null
   DWORD          dwFlags - Extra settings - one or more...
                        0x01 - Extend left sides by a surface thickness to ensure overlap
                        0x02 - Extenr right side
                        0x04 - Extend top side by a surface thickness to ensure overlap
                        0x08 - Extend bottom down down
                        0x10 - Make the extension 4x as large on either side
                        0x20 - Don't warp. No matter what, once the flat surface is created, return
returns
   PBBSURF - Surface as it appears in the final list. Like WishListAdd() return.
*/
PBBSURF CObjectCabinet::WishListAddFlat (PBBSURF pSurf, PCPoint pBase1, PCPoint pBase2,
                                            PCPoint pTop1, PCPoint pTop2, DWORD dwFlags)
{
   // calculate the size and angle based upon the base

   // three points for a plane. Two are the bottom, the third is either the only
   // top point or the average
   CPoint pPlane;
   if (pTop2) {
      pPlane.Add (pTop1, pTop2);
      pPlane.Scale (.5);
   }
   else
      pPlane.Copy (pTop1);

   // figure out the three vectors. pA gets translated to X, pB to Y, and pC to Z
   CPoint pA, pB, pC;
   pA.Subtract (pBase2, pBase1);
   pA.Normalize();
   pC.Subtract (&pPlane, pBase1);
   pB.CrossProd (&pC, &pA);
   pB.Normalize();
   pC.CrossProd (&pA, &pB);
   pC.Normalize();

   // make a matrix that converts from spline to object space
   CMatrix mSplineToObject;
   mSplineToObject.RotationFromVectors (&pA, &pB, &pC);

   // now, make a matrix that converts from object space to spline
   CMatrix mObjectToSpline;
   mSplineToObject.Invert (&mObjectToSpline);

   // convert the 4 points
   CPoint pB1, pB2, pT1, pT2;
   pB1.Copy (pBase1);
   pB1.p[3] = 1;
   pB1.MultiplyLeft (&mObjectToSpline);
   pB2.Copy (pBase2);
   pB2.p[3] = 1;
   pB2.MultiplyLeft (&mObjectToSpline);
   pT1.Copy (pTop1);
   pT1.p[3] = 1;
   pT1.MultiplyLeft (&mObjectToSpline);
   if (pTop2) {
      pT2.Copy (pTop2);
      pT2.p[3] = 1;
      pT2.MultiplyLeft (&mObjectToSpline);
   }

   // figure out the min/max in x and Z
   CPoint pMin, pMax;
   pMin.Copy (&pB1);
   pMax.Copy (&pB1);
   pMin.Min (&pB2);
   pMax.Max (&pB2);
   pMin.Min (&pT1);
   pMax.Max (&pT1);
   if (pTop2) {
      pMin.Min (&pT2);
      pMax.Max (&pT2);
   }

   // overlap
   fp fOverlap;
   fOverlap = (pSurf->fThickSideA + pSurf->fThickSideB + pSurf->fThickStud) * 2;
   if (dwFlags & 0x10)
      fOverlap *= 4;
   if (dwFlags & 0x01)
      pMin.p[0] -= fOverlap;
   if (dwFlags & 0x02)
      pMax.p[0] += fOverlap;
   if (dwFlags & 0x04)
      pMax.p[2] += fOverlap;
   if (dwFlags & 0x08)
      pMin.p[2] -= fOverlap;

   // fill in width and height
   pSurf->fWidth = pMax.p[0] - pMin.p[0];
   pSurf->fHeight = pMax.p[2] - pMin.p[2];

   // rotation
   CPoint pTrans;
   mSplineToObject.ToXYZLLT (&pTrans, &pSurf->fRotZ, &pSurf->fRotX, &pSurf->fRotY);

   // translate so that half way between (pMin.x, pB1.y,pB1.z) and (pMax,pB2.y,pB2.z) ends up at 0,0,0
   CPoint pHalf;
   pHalf.Add (&pB1, &pB2);
   pHalf.Scale (.5);
   pHalf.p[0] = (pMin.p[0] + pMax.p[0]) / 2;
   if (dwFlags & 0x08)
      pHalf.p[2] -= fOverlap;
   pHalf.MultiplyLeft (&mSplineToObject);
   pSurf->pTrans.Copy (&pHalf);

 
   // Call wishlistadd
   PBBSURF pAdd;
   pAdd = WishListAdd (pSurf);
   if (!pAdd)
      return NULL;

   // keep flags indicating that flat
   pAdd->pSurface->m_fConstAbove = TRUE;
   pAdd->pSurface->m_fConstBottom = TRUE;
   pAdd->pSurface->m_fConstRectangle = TRUE;

   if (!pTop2 || (dwFlags & 0x20))
      goto done;  // cant be warped

   BOOL fDontWarp;
   fDontWarp = (fabs(pT1.p[1] - pT2.p[1]) < CLOSE);

   // if not supposed to extend left BUT the top is not over the bottom then need to warp
   if (!(dwFlags & 0x01) && (fabs(pT1.p[0] - pB1.p[0]) > CLOSE) )
      fDontWarp = FALSE;
   // same for right
   if (!(dwFlags & 0x02) && (fabs(pT2.p[0] - pB2.p[0]) > CLOSE) )
      fDontWarp = FALSE;
   // same for above
   if (!(dwFlags & 0x04) && (fabs(pT1.p[2] - pT2.p[2]) > CLOSE) )
      fDontWarp = FALSE;
   // dont worry about below since they're always flat

   if (fDontWarp)
      goto done;  // not warped


   // need to convert T1 and T2, along with B1, B2, into coords
   // that reflect the new coord space
   // X
   CMatrix m, mInv;
   pAdd->pSurface->MatrixGet (&m);
   m.Invert4 (&mInv);
   pT1.Copy (pTop1);
   pT1.p[3] = 1;
   pT1.MultiplyLeft (&mInv);
   pT2.Copy (pTop2);
   pT2.p[3] = 1;
   pT2.MultiplyLeft (&mInv);
   pB1.Copy (pBase1);
   pB1.p[3] = 1;
   pB1.MultiplyLeft (&mInv);
   pB2.Copy (pBase2);
   pB2.p[3] = 1;
   pB2.MultiplyLeft (&mInv);

   // note if warped
   fp fDeltaXT, fDeltaXB;
   // else, it's warped, so see where the line from pT1 to pT2 intersects pMin and pMax
   fDeltaXT = pT2.p[0] - pT1.p[0];
   fDeltaXB = pB2.p[0] - pB1.p[0];

   if ((fabs(fDeltaXT) < CLOSE) || (fabs(fDeltaXB) < CLOSE))
      goto done;  // to close

   // if we're allowed to extend left then move topleft (T1) or bottom left(B1)
   // to keep the sheet as square as possible
   CPoint pDelta;
   CPoint pTemp;
   fp fAlpha;
   if (dwFlags & 0x01) {   // ented to the left
      if (pT1.p[0] > pB1.p[0]) {
         pDelta.Subtract (&pT2, &pT1);
         fDeltaXT = pT2.p[0] - pT1.p[0];
         if (fabs((fp)fDeltaXT) < CLOSE)	// BUGFIX - was fabs(fDeltaXT < CLOSE)
            goto done;
         fAlpha = (pB1.p[0] - pT1.p[0]) / fDeltaXT;
         pTemp.Copy (&pDelta);
         pTemp.Scale (fAlpha);
         pTemp.Add (&pT1);
         pT1.Copy (&pTemp);
      }
      else {
         // extend base. Since it's on a flat plane already just move it
         pB1.p[0] = pT1.p[0];
      }
   }
   if (dwFlags & 0x02) {   // extend to the right
      if (pT2.p[0] < pB2.p[0]) {
         pDelta.Subtract (&pT2, &pT1);
         fDeltaXT = pT2.p[0] - pT1.p[0];
         if (fabs((fp)fDeltaXT) < CLOSE) // BUGFIX - was fabs(fDeltaXT < CLOSE)
            goto done;
         fAlpha = (pB2.p[0] - pT1.p[0]) / fDeltaXT;
         pTemp.Copy (&pDelta);
         pTemp.Scale (fAlpha);
         pTemp.Add (&pT1);
         pT2.Copy (&pTemp);
      }
      else {
         // extend base. Since it's on a flat plane already just move it
         pB2.p[0] = pT2.p[0];
      }
   }

   // extend the points by overlap
   DWORD dwDir, i;   // 0 for horiz, 1 for vertical
   for (dwDir = 0; dwDir < 2; dwDir++) for (i = 0; i < 2; i++) {
      PCPoint p1, p2;
      DWORD dwBitLeft, dwBitRight;
      switch (dwDir) {
      case 0:  // horizontal
         p1 = i ? &pB1 : &pT1;
         p2 = i ? &pB2 : &pT2;
         dwBitLeft = 0x01;
         dwBitRight = 0x02;
         break;
      case 1:  // vertical
         p1 = i ? &pB2 : &pB1;
         p2 = i ? &pT2 : &pT1;
         dwBitLeft = 0x08;
         dwBitRight = 0x04;
         break;
      }

      // find the vector
      pDelta.Subtract (p2, p1);
      pDelta.Normalize();
      pDelta.Scale (fOverlap);
      if (dwFlags & dwBitRight)
         p2->Add (&pDelta);
      pDelta.Scale (-1);
      if (dwFlags & dwBitLeft)
         p1->Add (&pDelta);
   }

   // convert these points back into object space
   //pT1.MultiplyLeft (&m);
   //pT2.MultiplyLeft (&m);
   //pB1.MultiplyLeft (&m);
   //pB2.MultiplyLeft (&m);

   // write them out
   pAdd->pSurface->m_fConstAbove = FALSE;
   pAdd->pSurface->m_fConstBottom = FALSE;
   pAdd->pSurface->m_fConstRectangle = FALSE;
   CPoint   apPoints[2][2];
   apPoints[0][0].Copy (&pT1);
   apPoints[0][1].Copy (&pT2);
   apPoints[1][0].Copy (&pB1);
   apPoints[1][1].Copy (&pB2);

   pAdd->pSurface->ControlPointsSet (2, 2, &apPoints[0][0],
      (DWORD*) SEGCURVE_LINEAR, (DWORD*) SEGCURVE_LINEAR, 3);
   // Use a higher level of detail in curved sections because otherwise wierd
   // stuff happens

   // extend the 
done:
   return pAdd;
}
/**********************************************************************************
CObjectCabinet::MakeShelfOrCounter - Makes either a shelf or a counter.

inputs
   DWORD    dwID - ID to use
   fp       fHeight - Height of the vertical-center of the shelf/counter
   fp       fThick - Thickness of the shelf/counter
   BOOL     fCounter - Set to TRUE if it's the counter, FALSE if it's a shelf
   PCPoint  pExtend - Amount to extend.. p[0] = distance from spline to back,
            p[1] = distance from spline to front,
            p[2] = extend left by this much
            p[3] = extend right by this much
   fp       fRound - If > 0 then round the corners by this much
   BOOL     *pafCircular - If [0] then circular on the left end, [1] circular on right.
               Can be null, then not circular
returns
   BOOL - TRUE if success
*/
BOOL CObjectCabinet::MakeShelfOrCounter (DWORD dwID, fp fHeight, fp fThick, BOOL fCounter,
                                         PCPoint pExtend, fp fRound, BOOL *pafCircular)
{
   // first of all, extend/distend the current setup if it's a base
   CSpline sCenter;
   m_sPath.CloneTo (&sCenter);
   if (pExtend->p[2] != 0)
      sCenter.ExtendEndpoint (TRUE, pExtend->p[2]);
   if (pExtend->p[3] != 0)
      sCenter.ExtendEndpoint (FALSE, pExtend->p[3]);

   // construct the right path
   PCSpline pBack, pFront;
   CPoint pUp;
   pUp.Zero();
   pUp.p[2] = 1;
   pBack = sCenter.Expand (-pExtend->p[1], &pUp);
   if (!pBack)
      return FALSE;
   pFront = pBack->Flip ();
   delete pBack;
   if (!pFront)
      return FALSE;

   // left side
   pBack = sCenter.Expand (pExtend->p[0], &pUp);
   if (!pBack) {
      delete pBack;
      return FALSE;
   }

   // create one large spline that embodies all of these
   CListFixed lPoints, lCurve;
   lPoints.Init (sizeof(CPoint));
   lCurve.Init (sizeof(DWORD));

   DWORD i, j;
   for (j = 0; j < 2; j++) {
      PCSpline pUse = j ? pFront : pBack;
      PCSpline pOther = j ? pBack : pFront;
      DWORD dwNum = pUse->OrigNumPointsGet();
      CPoint p;
      DWORD dw;

      lPoints.Required (lPoints.Num()+dwNum);
      lCurve.Required (lCurve.Num()+dwNum);

      for (i = 0; i < dwNum; i++) {
         // copy over the point and textures
         pUse->OrigPointGet (i, &p);
         p.p[2] = 0; // so interpolation better
         lPoints.Add (&p);

         if (i+1 < dwNum)
            pUse->OrigSegCurveGet (i, &dw);
         else
            dw = (pafCircular && pafCircular[1-j]) ? SEGCURVE_CIRCLENEXT : SEGCURVE_LINEAR;
         lCurve.Add (&dw);

         // if circular then extra bits
         if ((i+1 == dwNum) && pafCircular && pafCircular[1-j]) {
            CPoint p2, pDir, pUp, pPerp;
            pOther->OrigPointGet (0, &p2);
            p2.p[2] = 0;

            // calculate the other point in the sphere and add it
            pDir.Subtract (&p2, &p);
            pUp.Zero();
            pUp.p[2] = 1;
            pPerp.CrossProd (&pUp, &pDir);
            pPerp.Normalize();
            pPerp.Scale (pDir.Length() / 2);
            pDir.Average (&p, &p2);
            pDir.Add (&pPerp);
            lPoints.Add (&pDir);

            dw = SEGCURVE_CIRCLEPREV;
            lCurve.Add (&dw);
         }  // if circular end
      } // over i, points in spline
   }  // over j

   delete pFront;
   delete pBack;

   //create spline from this
   DWORD dwMin, dwMax;
   fp fDetail;
   m_sPath.DivideGet (&dwMin, &dwMax, &fDetail);
   sCenter.Init (TRUE, lPoints.Num(), (PCPoint)lPoints.Get(0), NULL,
      (DWORD*) lCurve.Get(0), dwMin, dwMax, fDetail);

   if (fRound > CLOSE) {
      DWORD dwNum = sCenter.OrigNumPointsGet();
      lPoints.Clear();
      lCurve.Clear();

      // reconstruct with curves
      DWORD dwThis, dwPrev, dwNext, dwElNext, dwElPrev;
      CPoint pThis, pNext, pTemp;
      fp fLen;
      dwElNext = SEGCURVE_ELLIPSENEXT;
      dwElPrev = SEGCURVE_ELLIPSEPREV;
      lPoints.Required (dwNum);
      lCurve.Required (dwNum);
      for (i = 0; i < dwNum; i++) {
         // get surrounding points
         sCenter.OrigSegCurveGet (i, &dwThis);
         sCenter.OrigSegCurveGet ((i+1)%dwNum, &dwNext);
         sCenter.OrigSegCurveGet ((i+dwNum-1)%dwNum, &dwPrev);
         sCenter.OrigPointGet (i, &pThis);
         sCenter.OrigPointGet ((i+1)%dwNum, &pNext);

         if (dwThis != SEGCURVE_LINEAR) {
            // add it
            lPoints.Add (&pThis);
            lCurve.Add (&dwThis);
            continue;
         }

         if (dwPrev == SEGCURVE_LINEAR) {
            // add current pointer as start of ellipse
            lPoints.Add (&pThis);
            lCurve.Add (&dwElPrev);

            // add another point for start of linear
            pTemp.Subtract (&pNext, &pThis);
            fLen = pTemp.Length() / 2.01;
            fLen = min (fLen, fRound);
            pTemp.Normalize();
            pTemp.Scale (fLen);
            pTemp.Add (&pThis);
            lPoints.Add (&pTemp);
            lCurve.Add (&dwThis);
         }
         else {
            // else, add as a linear segment
            lPoints.Add (&pThis);
            lCurve.Add (&dwThis);
         }

         // if the next one is linear then cut this short and add
         // a curve
         if (dwNext == SEGCURVE_LINEAR) {
            pTemp.Subtract (&pThis, &pNext);
            fLen = pTemp.Length() / 2.01;
            fLen = min (fLen, fRound);
            pTemp.Normalize();
            pTemp.Scale (fLen);
            pTemp.Add (&pNext);
            lPoints.Add (&pTemp);
            lCurve.Add (&dwElNext);
         }
      }

      // Create the spline
      sCenter.Init (TRUE, lPoints.Num(), (PCPoint) lPoints.Get(0),
         NULL, (DWORD*) lCurve.Get(0), dwMin, dwMax, fDetail);
   }


   // find the boundaries for this
   CPoint pMin, pMax, pVal;
   for (i = 0; i < sCenter.QueryNodes(); i++) {
      pVal.Copy (sCenter.LocationGet (i));

      if (i) {
         pMin.Min (&pVal);
         pMax.Max (&pVal);
      }
      else {
         pMin.Copy (&pVal);
         pMax.Copy (&pVal);
      }
   }

   // make it slightly larger so have space to grow.
   // also, make it perfectly square so that circles stay as circles
   CPoint pCenter;
   pCenter.Average (&pMax, &pMin);
   pVal.Subtract (&pMax, &pMin);
   pVal.p[0] = max(pVal.p[0], CLOSE) * .5;
   pVal.p[1] = max(pVal.p[1], CLOSE) * .5;

   // BUGFIX - Only make perfectly swuare if using circle next
   BOOL fCircle, fCurve;
   DWORD dwCurve;
   fCircle = FALSE;
   fCurve = FALSE;
   for (i = 0; i < sCenter.OrigNumPointsGet(); i++) {
      dwCurve = sCenter.OrigSegCurveGet (i, &dwCurve);
      if (dwCurve != SEGCURVE_LINEAR)
         fCurve = TRUE;
      if ((dwCurve == SEGCURVE_CIRCLENEXT) || (dwCurve == SEGCURVE_CIRCLEPREV))
         fCircle = TRUE;
   }
   if (fCurve) {  // always make curved larger
      pVal.p[0] *= 1.5;
      pVal.p[1] *= 1.5;
   }
   if (fCircle)
      pVal.p[0] = pVal.p[1] = max(pVal.p[0], pVal.p[1]);

   pMin.Subtract (&pCenter, &pVal);
   pMax.Add (&pCenter, &pVal);
   pVal.Subtract (&pMax, &pMin);

   // border of the flat plane
   CPoint b1, b2, t1, t2;
   b1.Zero();
   b1.p[0] = pMin.p[0];
   b1.p[1] = pMin.p[1];
   b1.p[2] = fHeight;
   b2.Copy (&b1);
   b2.p[0] = pMax.p[0];
   t1.Copy (&b1);
   t2.Copy (&b2);
   t1.p[1] = t2.p[1] = pMax.p[1];

   // fill in the surface info that want
   BBSURF bbs;
   memset (&bbs, 0, sizeof(bbs));
   wcscpy (bbs.szName, fCounter ? L"Counter" : L"Shelf");
   bbs.dwMajor = fCounter ? BBSURFMAJOR_ROOF : BBSURFMAJOR_FLOOR;
   bbs.dwType = fCounter ? 0x30006 : 0x30007;
   bbs.fThickSideA = bbs.fThickSideB = 0;
   bbs.fThickStud = fThick;
   bbs.fHideEdges = FALSE; // need to show on all cabinets
   bbs.dwMinor = dwID;

   // create flat plane with wishlistadd
   PBBSURF pNew;
   pNew = WishListAddFlat (&bbs, &b1, &b2, &t1, &t2, 0);
   if (!pNew || !pNew->pSurface)
      return FALSE;

   // convert spline to plane coords
   lPoints.Clear();
   lCurve.Clear();
   lPoints.Required (sCenter.OrigNumPointsGet());
   lCurve.Required (sCenter.OrigNumPointsGet());
   for (i = 0; i < sCenter.OrigNumPointsGet(); i++) {
      CPoint p;
      DWORD dwCurve;

      // get the original
      sCenter.OrigPointGet (i, &p);
      sCenter.OrigSegCurveGet (i, &dwCurve);

      // convert to 0..1, 0..1 coords
      p.p[0] = (p.p[0] - pMin.p[0]) / pVal.p[0];
      p.p[1] = (pMax.p[1] - p.p[1]) / pVal.p[1];
      p.p[2] = 0;

      // add it
      lPoints.Add (&p);
      lCurve.Add (&dwCurve);
   }

   // set the edge
   pNew->pSurface->EdgeInit ( TRUE, lPoints.Num(), (PCPoint) lPoints.Get(0), NULL,
      (DWORD*) lCurve.Get(0), dwMin, dwMax, fDetail);

   return TRUE;

}



/**********************************************************************************
CObjectCabinet::MakeTheWalls - Build the walls of the building block object.

inputs
   fp      fStartAt - Height at which the walls start
   fp      fRoofHeight - Height that they go up to
   BOOL    fBase - If TRUE, these are for the base, so use the base ID, also, shrink
            in by the base indent
returns
   none
*/
void CObjectCabinet::MakeTheWalls (fp fStartAt, fp fRoofHeight, BOOL fBase)
{
   // what walls shown
   BOOL  afShowWall[4];    // [0] = back, [1]=front, [2]=left, [3]=right
   DWORD i;
   for (i = 0; i < 4; i++)
      afShowWall[i] = (m_dwShowWall & (1 << i)) ? TRUE : FALSE;

   // first of all, extend/distend the current setup if it's a base
   CSpline sCenter;
   m_sPath.CloneTo (&sCenter);
   if (fBase) {
      sCenter.ExtendEndpoint (TRUE, - m_pBaseIndent.p[2]);
      sCenter.ExtendEndpoint (FALSE, - m_pBaseIndent.p[3]);
   }

   PCSpline pBack, pFront;
   fp fExpand;
   CPoint pUp;
   pUp.Zero();
   pUp.p[2] = 1;

   // back side
   fExpand = m_fCabinetDepth / 2 - m_fWallThick/2; // BUGFIX - Was m_fWallThick / 1
   if (fBase)
      fExpand -= m_pBaseIndent.p[0];
   pFront = sCenter.Expand (fExpand, &pUp);
   if (!pFront)
      return;
   pBack = pFront->Flip();
   delete pFront;
   if (!pBack)
      return;

   // construct the right path
   fExpand = m_fCabinetDepth / 2 - m_fWallThick/2; // BUGFIX - Was m_fWallThick/1
   if (fBase)
      fExpand -= m_pBaseIndent.p[1];
   pFront = sCenter.Expand (-fExpand, &pUp);
   if (!pFront) {
      delete pBack;
      return;
   }

   // default setup
   BBSURF bbs;
   for (i = 0; i < 4; i++) {
      // maybe don't show the wall. Always draw the base though
      if (!fBase && !afShowWall[i])
         continue;

      memset (&bbs, 0, sizeof(bbs));
      wcscpy (bbs.szName, fBase ? L"Base" : gszWall);
      bbs.dwMajor = BBSURFMAJOR_WALL;
      bbs.dwType = 0x1000d;
      bbs.fThickSideA = bbs.fThickSideB = 0;
      bbs.fThickStud = m_fWallThick;
      bbs.fHideEdges = FALSE; // need to show on all cabinets
      bbs.dwMinor = i + (fBase ? 1000 : 0);

      switch (i) {
      case 0:  // back
         WishListAddCurved (&bbs, pBack, fStartAt, fRoofHeight, 0);
         break;
      case 1:  // front
         WishListAddCurved (&bbs, pFront, fStartAt, fRoofHeight, 0);
         break;
      case 2:  // left side
      case 3:  // right side
         CPoint b1, b2, t1, t2, pDelta, pUp, pIn;
         PCSpline pLeft, pRight;
         DWORD dwLeft, dwRight;
         pLeft = (i==2) ? pBack : pFront;
         pRight = (i==2) ? pFront : pBack;
         dwLeft = (i==2) ? 0 : 1;
         dwRight = (i==2) ? 1 : 0;
         pLeft->OrigPointGet (pLeft->OrigNumPointsGet()-1, &b1);
         pRight->OrigPointGet (0, &b2);

         // z height
         b1.p[2] = b2.p[2] = fStartAt;

         // shrink by a bit so don't go ingo wall
         pDelta.Subtract (&b2, &b1);
         pDelta.Normalize();

         // also, move in a bit
         pUp.Zero();
         pUp.p[2] = 1;
         pIn.CrossProd (&pUp, &pDelta);
         pIn.Normalize();
         pIn.Scale (m_fWallThick / 2.0);
         b2.Add (&pIn);
         b1.Add (&pIn);

         // shrink
         // If not showing front/back then need to expand
         pDelta.Scale (m_fWallThick / 2.0);
         if (fBase || afShowWall[dwRight])
            b2.Subtract (&pDelta);
         else
            b2.Add (&pDelta);

         if (fBase || afShowWall[dwLeft])
            b1.Add (&pDelta);
         else
            b1.Subtract (&pDelta);

         // points above
         t1.Copy (&b1);
         t1.p[2] = fRoofHeight;
         t2.Copy (&b2);
         t2.p[2] = fRoofHeight;

         WishListAddFlat (&bbs, &b1, &b2, &t1, &t2, 0);
         break;
      }
   }

   // finally
   delete pBack;
   delete pFront;
   return;

}



