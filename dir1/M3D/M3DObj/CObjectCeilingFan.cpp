/************************************************************************
CObjectCeilingFan.cpp - Draws a CeilingFan.

begun 14/9/02 by Mike Rozak
Copyright 2002 Mike Rozak. All rights reserved
*/

#include <windows.h>
#include "escarpment.h"
#include "..\M3D.h"
#include "resource.h"


#define DISCDIAM        .18      // diameter of disc which blades attached to
#define DISCTHICK       .05      // thickness of disc to which blades are attached
#define CAPSIZE         .1       // diameter and length of the cap
#define BLADEWIDTH      .15      // avera width of the blade

typedef struct {
   PWSTR       pszName;    // name of the move point
   int         iX, iY; // -1 if for left/top, 1 for right/bottom
} SPLINEMOVEP, *PSPLINEMOVEP;

static SPLINEMOVEP   gaCeilingFanMove[] = {
   L"Fan base", 0, 0
};

/**********************************************************************************
CObjectCeilingFan::Constructor and destructor */
CObjectCeilingFan::CObjectCeilingFan (PVOID pParams, POSINFO pInfo)
{
   m_mHang.Identity();
   m_dwRenderShow = RENDERSHOW_ELECTRICAL;
   m_OSINFO = *pInfo;
   m_fCanBeEmbedded = TRUE;

   m_dwBlades = max ((DWORD)(size_t) pParams, 1);
   m_fStemLen = .5;
   m_fBladeLen = .5;

   CalcInfo ();

   // color for the CeilingFan
   ObjectSurfaceAdd (42, RGB(0xff,0xff,0xff), MATERIAL_PLASTIC, L"Ceiling fans");
}


CObjectCeilingFan::~CObjectCeilingFan (void)
{
   // do nothing for now
}


/**********************************************************************************
CObjectCeilingFan::Delete - Called to delete this object
*/
void CObjectCeilingFan::Delete (void)
{
   delete this;
}

/**********************************************************************************
CObjectCeilingFan::Render - Draws a test box.

inputs
   POBJECTRENDER     pr - Render information
*/
void CObjectCeilingFan::Render (POBJECTRENDER pr, DWORD dwSubObject)
{
   // create the surface render object and draw
   //CRenderSurface rs;
   m_Renderrs.ClearAll();

   CMatrix mObject;
   m_Renderrs.Init (pr->pRS);
   ObjectMatrixGet (&mObject);
   m_Renderrs.Multiply (&mObject);

   m_Renderrs.Multiply (&m_mHang); // apply hanging matrix

   // object specific
   m_Renderrs.SetDefMaterial (m_OSINFO.dwRenderShard, ObjectSurfaceFind (42), m_pWorld);

   // if hanging but not embeddd then rotate
   //GUID gContain;
   //if (!EmbedContainerGet(&gContain)) {
   //   // rotate so that Z=1 goes to Z=-1
   //   m_Renderrs.Rotate (PI/2, 1);
   //}

   m_Noodle.Render (pr, &m_Renderrs);
   DWORD i;
   for (i = 0 ; i < 3; i++)
      m_aRev[i].Render (pr, &m_Renderrs);

   // draw blades
   CPoint ap[4];
   memset (ap, 0, sizeof(ap));
   ap[0].p[0] = DISCDIAM/2; // UL
   ap[0].p[2] = BLADEWIDTH/3;
   ap[1].p[0] = DISCDIAM/2 + m_fBladeLen; // UR
   ap[1].p[2] = BLADEWIDTH/3;
   ap[2].p[0] = DISCDIAM/2 + m_fBladeLen * .9; // LR
   ap[2].p[1] = -BLADEWIDTH/10;
   ap[2].p[2] = -BLADEWIDTH/2.0;
   ap[3].p[0] = DISCDIAM/2;   // LL
   ap[3].p[1] = -BLADEWIDTH/10;
   ap[3].p[2] = -BLADEWIDTH*2.0/3.0;
   m_Renderrs.Translate (0, -(CAPSIZE + DISCTHICK/2 + m_fStemLen), 0); // so blades at the point
   for (i = 0; i < m_dwBlades; i++) {
      // rotate
      if (i)
         m_Renderrs.Rotate (2 * PI / (fp) m_dwBlades, 2);
      m_Renderrs.ShapeQuad (&ap[0], &ap[1], &ap[2], &ap[3], FALSE);
   }

   m_Renderrs.Commit();
}



/**********************************************************************************
CObjectCeilingFan::QueryBoundingBox - Standard API
*/
void CObjectCeilingFan::QueryBoundingBox (PCPoint pCorner1, PCPoint pCorner2, DWORD dwSubObject)
{
   CPoint p1, p2;
   m_Noodle.QueryBoundingBox (pCorner1, pCorner2);
   DWORD i;
   for (i = 0 ; i < 3; i++) {
      m_aRev[i].QueryBoundingBox (&p1, &p2);

      pCorner1->Min (&p1);
      pCorner2->Max (&p2);
   }

   // radius
   CPoint pBlade;
   pBlade.Zero();
   pBlade.p[0] = pBlade.p[2] = -(m_fBladeLen+DISCDIAM/2);
   pBlade.p[1] = -(CAPSIZE + DISCTHICK/2 + m_fStemLen) - BLADEWIDTH/2;
   pCorner1->Min (&pBlade);
   pBlade.p[0] = pBlade.p[2] = m_fBladeLen+DISCDIAM/2;
   pBlade.p[1] = -(CAPSIZE + DISCTHICK/2 + m_fStemLen) + BLADEWIDTH/2;
   pCorner2->Max (&pBlade);

   BoundingBoxApplyMatrix (pCorner1, pCorner2, &m_mHang);

#ifdef _DEBUG
   // test, make sure bounding box not too small
   //CPoint p1,p2;
   //DWORD i;
   CObjectTemplate::QueryBoundingBox (&p1, &p2, dwSubObject);
   for (i = 0; i < 3; i++)
      if ((p1.p[i] < pCorner1->p[i] - CLOSE) || (p2.p[i] > pCorner2->p[i] + CLOSE))
         break;
   if (i < 3)
      OutputDebugString ("\r\nCObjectCeilingFan::QueryBoundingBox too small.");
#endif
}


/**********************************************************************************
CObjectCeilingFan::Clone - Clones this

inputs
   none
returns
   PCObjectSocket - Clone
*/
PCObjectSocket CObjectCeilingFan::Clone (void)
{
   PCObjectCeilingFan pNew;

   pNew = new CObjectCeilingFan(NULL, &m_OSINFO);

   // clone template info
   CloneTemplate(pNew);

   pNew->m_dwBlades = m_dwBlades;
   pNew->m_fStemLen = m_fStemLen;
   pNew->m_fBladeLen = m_fBladeLen;
   pNew->m_mHang.Copy (&m_mHang);

   m_Noodle.CloneTo (&pNew->m_Noodle);
   DWORD i;
   for (i = 0; i < 3; i++)
      m_aRev[i].CloneTo (&pNew->m_aRev[i]);

   return pNew;
}

static PWSTR gpszBlades = L"Blades";
static PWSTR gpszStemLen = L"StemLen";
static PWSTR gpszBladeLen= L"BladeLen";

PCMMLNode2 CObjectCeilingFan::MMLTo (void)
{
   // call into the template first
   PCMMLNode2 pNode = MMLToTemplate();
   if (!pNode)
      return NULL;

   MMLValueSet (pNode, gpszBlades, (int) m_dwBlades);
   MMLValueSet (pNode, gpszStemLen, m_fStemLen);
   MMLValueSet (pNode, gpszBladeLen, m_fBladeLen);

   return pNode;
}

BOOL CObjectCeilingFan::MMLFrom (PCMMLNode2 pNode)
{
   // call into the template first
   if (!MMLFromTemplate (pNode))
      return FALSE;

   m_dwBlades = (DWORD) MMLValueGetInt (pNode, gpszBlades, (int) 0);
   m_fStemLen = MMLValueGetDouble (pNode, gpszStemLen, .1);
   m_fBladeLen = MMLValueGetDouble (pNode, gpszBladeLen, 1);

   // BUGFIX - When reloaded ceiling fan wasn't hanging
   CMatrix mEmbed;
   mEmbed.RotationX (PI/2);
   m_MatrixObject.Invert (&m_mHang);
   m_mHang.MultiplyLeft (&mEmbed);

   CalcInfo();

   return TRUE;
}


/********************************************************************************
CObjectCeilingFan::CalcInfo - Fill the noodles and revolutions
with the appropriate information
for drawing
*/
void CObjectCeilingFan::CalcInfo (void)
{
   // defaults
   m_dwBlades = max(1,m_dwBlades);
   m_fStemLen = max(.1, m_fStemLen);
   m_fBladeLen = max (.1, m_fBladeLen);

   // draw it as if attached to a wall and pointing out
   CPoint pFront, pStart, pEnd, pScale;
   pFront.Zero();
   pFront.p[2] = 1;
   pStart.Zero();
   pEnd.Zero();
   pEnd.p[1] = -m_fStemLen;
   pScale.Zero();
   pScale.p[0] = pScale.p[1] = .02;
   m_Noodle.BackfaceCullSet (TRUE);
   m_Noodle.DrawEndsSet (FALSE);
   m_Noodle.FrontVector (&pFront);
   m_Noodle.PathLinear (&pStart, &pEnd);
   m_Noodle.ScaleVector (&pScale);
   m_Noodle.ShapeDefault (NS_CIRCLEFAST);

   // base
   CPoint pDown, pLeft;
   pDown.Zero();
   pDown.p[1] = -1;
   pLeft.Zero();
   pLeft.p[0] = 1;
   pScale.Zero();
   pScale.p[0] = pScale.p[1] = pScale.p[2] = .15;
   pStart.p[1] = pScale.p[2] / 2;
   m_aRev[0].BackfaceCullSet (TRUE);
   m_aRev[0].DirectionSet (&pStart, &pDown, &pLeft);
   m_aRev[0].ProfileSet (RPROF_CIRCLE);
   m_aRev[0].RevolutionSet (RREV_CIRCLE);
   m_aRev[0].ScaleSet (&pScale);

   // cap and end of stem
   pStart.Copy (&pEnd);
   pScale.p[0] = pScale.p[1] = pScale.p[2] = CAPSIZE;
   m_aRev[1].BackfaceCullSet (TRUE);
   m_aRev[1].DirectionSet (&pStart, &pDown, &pLeft);
   m_aRev[1].ProfileSet (RPROF_LDIAG);
   m_aRev[1].RevolutionSet (RREV_CIRCLE);
   m_aRev[1].ScaleSet (&pScale);

   // circular bit that blades re attached to
   pStart.p[1] -= pScale.p[2];
   pScale.p[0] = pScale.p[1] = DISCDIAM;
   pScale.p[2] = DISCTHICK;
   m_aRev[2].BackfaceCullSet (TRUE);
   m_aRev[2].DirectionSet (&pStart, &pDown, &pLeft);
   m_aRev[2].ProfileSet (RPROF_C);
   m_aRev[2].RevolutionSet (RREV_CIRCLE);
   m_aRev[2].ScaleSet (&pScale);
}


/**************************************************************************************
CObjectCeilingFan::MoveReferencePointQuery - 
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
BOOL CObjectCeilingFan::MoveReferencePointQuery (DWORD dwIndex, PCPoint pp)
{
   PSPLINEMOVEP ps;
   DWORD dwDataSize;
   ps = gaCeilingFanMove;
   dwDataSize = sizeof(gaCeilingFanMove);

   if (dwIndex >= dwDataSize / sizeof(SPLINEMOVEP))
      return FALSE;

   // always at 0,0 in CeilingFans
   pp->Zero();
   return TRUE;
}

/**************************************************************************************
CObjectCeilingFan::MoveReferenceStringQuery -
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
BOOL CObjectCeilingFan::MoveReferenceStringQuery (DWORD dwIndex, PWSTR psz, DWORD dwSize, DWORD *pdwNeeded)
{
   PSPLINEMOVEP ps;
   DWORD dwDataSize;
   ps = gaCeilingFanMove;
   dwDataSize = sizeof(gaCeilingFanMove);
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
CObjectCeilingFan::ObjectMatrixSet - Capture so can notify object of what's up/down
*/
BOOL CObjectCeilingFan::ObjectMatrixSet (CMatrix *pObject)
{
   if (!CObjectTemplate::ObjectMatrixSet (pObject))
      return FALSE;

   if (m_pWorld)
      m_pWorld->ObjectAboutToChange (this);

   CMatrix mEmbed;
   //GUID gContain;
   //if (!EmbedContainerGet(&gContain))
   //   mEmbed.Identity();
   //else
   mEmbed.RotationX (PI/2);

   m_MatrixObject.Invert (&m_mHang);
   m_mHang.MultiplyLeft (&mEmbed);

   if (m_pWorld)
      m_pWorld->ObjectChanged (this);

   return TRUE;
}


/**********************************************************************************
CObjectCeilingFan::EmbedDoCutout - Member function specific to the template. Called
when the object has moved within the surface. This enables the super-class for
the embedded object to pass a cutout into the container. (Basically, specify the
hole for the window or door)
*/
BOOL CObjectCeilingFan::EmbedDoCutout (void)
{
   if (m_pWorld)
      m_pWorld->ObjectAboutToChange (this);

   CMatrix mEmbed;
   //GUID gContain;
   //if (!EmbedContainerGet(&gContain))
   //   mEmbed.Identity();
   //else
   mEmbed.RotationX (PI/2);

   m_MatrixObject.Invert (&m_mHang);
   m_mHang.MultiplyLeft (&mEmbed);


   if (m_pWorld)
      m_pWorld->ObjectChanged (this);


   // dont really wanta  cutout
   return FALSE;
}



/*************************************************************************************
CObjectCeilingFan::ControlPointQuery - Called to query the information about a control
point identified by dwID.

inputs
   DWORD       dwID - ID
   POSCONTROL  pInfo - Filled in with the infomration
returns
   BOOL - TRUE if successful
*/
BOOL CObjectCeilingFan::ControlPointQuery (DWORD dwID, POSCONTROL pInfo)
{
   if (dwID >= 2)
      return FALSE;

   memset (pInfo, 0, sizeof(*pInfo));
   pInfo->dwID = dwID;
   pInfo->cColor = dwID ? RGB(0xff,0xff,0) : RGB(0,0xff,0xff);
   pInfo->dwStyle = dwID ? CPSTYLE_CUBE : CPSTYLE_SPHERE;
   pInfo->fSize = DISCTHICK;
   MeasureToString (dwID ? m_fBladeLen : m_fStemLen, pInfo->szMeasurement);
   wcscpy (pInfo->szName, dwID ? L"Blade length" : L"Stem length");

   if (dwID) {
      pInfo->pLocation.p[0] = m_fBladeLen + DISCDIAM/2;
      pInfo->pLocation.p[1] = -(CAPSIZE + DISCTHICK/2 + m_fStemLen);
   }
   else {
      pInfo->pLocation.p[1] = -m_fStemLen;
   }
   pInfo->pLocation.p[3] = 1;

   // apply rotation for hanging
   pInfo->pLocation.MultiplyLeft (&m_mHang);

   return TRUE;
}

/*************************************************************************************
CObjectCeilingFan::ControlPointSet - Called to change the valud of a control point.

inputs
   DWORD       dwID - ID
   PCPoint     pVal - Contains the new location, in object coordinates
returns
   BOOL - TRUE if successful
*/
BOOL CObjectCeilingFan::ControlPointSet (DWORD dwID, PCPoint pVal, PCPoint pViewer)
{
   if (dwID >= 2)
      return FALSE;

   CPoint p;
   p.Copy (pVal);

   // apply inverse rotation
   CMatrix mInv;
   m_mHang.Invert (&mInv);
   p.MultiplyLeft (&mInv);

   m_pWorld->ObjectAboutToChange (this);

   if (dwID)
      m_fBladeLen = max(.01, p.p[0] - DISCDIAM/2);
   else
      m_fStemLen = max(.1, -p.p[1]);

   CalcInfo ();

   m_pWorld->ObjectChanged (this);

   return TRUE;
}

/*************************************************************************************
CObjectCeilingFan::ControlPointEnum - Called to enumerate a list of control point IDs
into the list.

inputs
   PCListFixed       plDWORD - Should be filled in with control points
returns
   none
*/
void CObjectCeilingFan::ControlPointEnum (PCListFixed plDWORD)
{
   DWORD i;
   for (i = 0; i < 2; i++)
      plDWORD->Add (&i);
}

