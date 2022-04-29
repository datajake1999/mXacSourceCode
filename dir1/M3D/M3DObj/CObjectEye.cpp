/************************************************************************
CObjectEye.cpp - Draws a Eye.

begun 14/9/01 by Mike Rozak
Copyright 2001 Mike Rozak. All rights reserved
*/

#include <windows.h>
#include "escarpment.h"
#include "..\M3D.h"
#include "resource.h"



typedef struct {
   PWSTR       pszName;    // name of the move point
   int         iX, iY; // -1 if for left/top, 1 for right/bottom
} SPLINEMOVEP, *PSPLINEMOVEP;

static SPLINEMOVEP   gaEyeMove[] = {
   L"Center", 0, 0
};


/**********************************************************************************
CObjectEye::Constructor and destructor */
CObjectEye::CObjectEye (PVOID pParams, POSINFO pInfo)
{
   m_dwRenderShow = RENDERSHOW_CHARACTER;
   m_OSINFO = *pInfo;

   m_fDiameter = .035;
   m_fCorneaArc = 25.0 / 360.0 * 2.0 * PI;
   m_fCorneaHeight = .075;
   m_fApart = .06;
   m_dwNumEyes = 2;
   m_fIndepColor = m_fIndepMove = FALSE;
   m_dwRepeatSclera = 3;
   m_dwRepeatIris = 6;


   // looking forward
   CPoint pLook;
   pLook.Zero();
   pLook.p[1] = -2;
   m_apEyeInfo[0].p[2] = m_apEyeInfo[1].p[2] = .5;
   EyeInfoFromPoint (0, &pLook);
   EyeInfoFromPoint (1, &pLook);

   m_pCatEyes.Zero();
   m_fRevValid = FALSE;
   m_dwPrevRes = 0;

   // set the surfaces...
   // 0 and 10 are the left/right cornea
   // 1 and 11 are the left/right sclera
   // 2 and 12 are the left/right iris
   // 3 and 13 are the left/right pupil
   ObjectSurfaceAdd (0, RGB(0xff,0xff,0xff), MATERIAL_GLASSCLEARSOLID);
   ObjectSurfaceAdd (1, RGB(0xff,0xff,0xff), MATERIAL_PAINTGLOSS, NULL,
      &GTEXTURECODE_BloodVessels, &GTEXTURESUB_BloodVesselsEye);
   ObjectSurfaceAdd (2, RGB(0x0,0x80,0xff), MATERIAL_PAINTSEMIGLOSS, NULL,
      &GTEXTURECODE_Iris, &GTEXTURESUB_IrisEye);
   ObjectSurfaceAdd (3, RGB(0x0,0x0,0x0), MATERIAL_FLAT);
}


CObjectEye::~CObjectEye (void)
{
   // do nothing for now
}


/**********************************************************************************
CObjectEye::Delete - Called to delete this object
*/
void CObjectEye::Delete (void)
{
   delete this;
}

/**********************************************************************************
CObjectEye::Render - Draws a test box.

inputs
   POBJECTRENDER     pr - Render information
*/
void CObjectEye::Render (POBJECTRENDER pr, DWORD dwSubObject)
{
   DWORD i, j, k;

#define INNERSCALE         0.95        // size of inner parts of eye compared to outer

   // what resolutions
   DWORD dwRes;
   fp fDetail = pr->pRS->QueryDetail();
   if (fDetail <= m_fDiameter / 2.0)
      dwRes = 2;
   else if (fDetail <= m_fDiameter)
      dwRes = 1;
   else
      dwRes = 0;

   // if new resolution then force a recalc
   if ((pr->dwReason != ORREASON_BOUNDINGBOX) && (dwRes != m_dwPrevRes))
      m_fRevValid = FALSE;


   // calculate the cornea and sclera if necessary
   if (!m_fRevValid) {
      m_fRevValid = TRUE;
      m_dwPrevRes = dwRes;

      // at what point does bump occur?
      m_fCorneaArc = max(m_fCorneaArc, 0);
      m_fCorneaArc = min(m_fCorneaArc, PI / 4 * .9);
      fp fBump = 90.0 - m_fCorneaArc / (2.0 * PI) * 360.0;

      // figure out number of points on each part of cornea...
      DWORD dwNumRound, dwNumBump;
      fp fDetAngle = 40.0 / (fp)(1 << dwRes);
      dwNumRound = max((DWORD)(fBump / fDetAngle), 2) + 1;
      dwNumBump = max((DWORD)((90.0 - fBump) / (fDetAngle/2)), 2) + 1;

      // fill in list with points
      CListFixed lPoint;
      CSpline SplineCornea, SplineSclera;
      lPoint.Init (sizeof(CPoint));
      fp fAngle;
      CPoint p;
      lPoint.Required (dwNumRound);
      for (i = 0; i < dwNumRound; i++) {
         fAngle = (fp)i / (fp)(dwNumRound-1) * fBump;
         fAngle = fAngle / 360.0 * 2.0 * PI;
         p.Zero();
         p.p[0] = cos(fAngle);
         p.p[1] = sin(fAngle);
         lPoint.Add (&p);
      } // i
      SplineSclera.Init (FALSE, lPoint.Num(), (PCPoint)lPoint.Get(0), NULL,
         (DWORD*)SEGCURVE_CUBIC, 0, 0);

      // keep points the same before moving on...
      lPoint.Required (lPoint.Num() + dwNumBump);
      for (i = 0; i < dwNumBump; i++) {
         fAngle = ((fp)(i+1) / (fp)dwNumBump) * (90.0 - fBump) + fBump;
            // NOTE: Not starting at exactly point so get more gradual flow in
         fAngle = fAngle / 360.0 * 2.0 * PI;
         p.Zero();
         p.p[0] = cos(fAngle);
         p.p[1] = sin(fAngle);
         p.Scale (1.0 + sin((fp)i / (fp)(dwNumBump-1) * PI / 2.0) * m_fCorneaHeight);
         lPoint.Add (&p);
      }
      // add to spline for cornea
      SplineCornea.Init (FALSE, lPoint.Num(), (PCPoint)lPoint.Get(0), NULL,
         (DWORD*)SEGCURVE_CUBIC, 0, 0);


      // fill in the revolutions
      CPoint pBottom, pAround, pLeft, pScale;
      pBottom.Zero();
      pAround.Zero();
      pAround.p[1] = -1;
      pLeft.Zero();
      pLeft.p[0] = -1;
      pScale.Zero();
      pScale.p[0] = pScale.p[1] = pScale.p[2] = m_fDiameter;
      pScale.p[2] /= 2;
      m_revCornea.BackfaceCullSet (TRUE);
      m_revSclera.BackfaceCullSet (TRUE);
      m_revCornea.DirectionSet (&pBottom, &pAround, &pLeft);
      m_revSclera.DirectionSet (&pBottom, &pAround, &pLeft);
      m_revCornea.ProfileSet (&SplineCornea, 0, 1);
      m_revSclera.ProfileSet (&SplineSclera);
      m_revCornea.RevolutionSet (RREV_CIRCLE, dwRes);
      m_revSclera.RevolutionSet (RREV_CIRCLE, dwRes);
      m_revCornea.ScaleSet (&pScale);
      pScale.Scale (INNERSCALE);
      m_revSclera.ScaleSet (&pScale);
      m_revCornea.TextRepeatSet (m_dwRepeatSclera, 1);
      m_revSclera.TextRepeatSet (m_dwRepeatSclera, 1);
   } // fill in revolutions

   // create the surface render object and draw
   // CRenderSurface rs;
   m_Renderrs.ClearAll();

   CMatrix mObject;
   m_Renderrs.Init (pr->pRS);
   ObjectMatrixGet (&mObject);
   m_Renderrs.Multiply (&mObject);

   // draw the eyes
   m_dwNumEyes = max(m_dwNumEyes, 1);
   m_dwNumEyes = min(m_dwNumEyes, 2);
   PCObjectSurface pos;
   DWORD dwColorWant;
   for (i = 0; i < m_dwNumEyes; i++) {
      DWORD dwColorInc = (i && m_fIndepColor) ? 10 : 0;

      m_Renderrs.Push();
      CPoint pTrans;
      pTrans.Zero();
      if (m_dwNumEyes > 1)
         pTrans.p[0] = m_fApart / 2.0 * (i ? -1 : 1);

      CMatrix mRot;
      mRot.FromXYZLLT (&pTrans, m_apEyeInfo[i].p[0], m_apEyeInfo[i].p[1], 0);
      m_Renderrs.Multiply (&mRot);
      if (m_pCatEyes.p[1])
         m_Renderrs.Rotate (m_pCatEyes.p[1] * (i ? -1 : 1), 2);


      // color for the cornea
      dwColorWant = 0;
      pos = ObjectSurfaceFind (dwColorWant + dwColorInc);
      if (!pos && dwColorInc) {
         // copy over low-color
         pos = ObjectSurfaceFind (dwColorWant);
         if (pos)
            pos = pos->Clone();
         if (pos) {
            pos->m_dwID = dwColorWant + dwColorInc;
            ObjectSurfaceAdd (pos);
            delete pos;
         }
         pos = ObjectSurfaceFind (dwColorWant + dwColorInc);
      }
      m_Renderrs.SetDefMaterial (m_OSINFO.dwRenderShard, pos, m_pWorld);

      m_revCornea.Render (pr, &m_Renderrs);


      // color for the sclera
      dwColorWant = 1;
      pos = ObjectSurfaceFind (dwColorWant + dwColorInc);
      if (!pos && dwColorInc) {
         // copy over low-color
         pos = ObjectSurfaceFind (dwColorWant);
         if (pos)
            pos = pos->Clone();
         if (pos) {
            pos->m_dwID = dwColorWant + dwColorInc;
            ObjectSurfaceAdd (pos);
            delete pos;
         }
         pos = ObjectSurfaceFind (dwColorWant + dwColorInc);
      }
      m_Renderrs.SetDefMaterial (m_OSINFO.dwRenderShard, pos, m_pWorld);

      // draw sclera
      m_revSclera.Render (pr, &m_Renderrs);

      // figure out how many points around the iris
      DWORD dwAround = 8 * (1 << dwRes);

      // allocate enough points for 2 times around + 1 for center
      PCPoint pPoints, pNorm;
      PVERTEX pVert;
      PTEXTPOINT5 pText;
      DWORD dwPoints, dwNorm, dwVert, dwText;
      pPoints = m_Renderrs.NewPoints (&dwPoints, dwAround*2+1);
      if (!pPoints)
         continue;
      pNorm = m_Renderrs.NewNormals (TRUE, &dwNorm, dwAround*2+1);
      pText = m_Renderrs.NewTextures (&dwText, (dwAround+1)*3+1);   // (dwAround+1)*[outer iris, inner iris, outer cornea] + inner cornea
      pVert = m_Renderrs.NewVertices (&dwVert, (dwAround+1)*3+1);
      if (!pVert)
         continue;

      fp fBump = 90.0 - m_fCorneaArc / (2.0 * PI) * 360.0;

      // generate all the points, textures, and normals
      fp fCenterDepth = 0;
      for (j = 0; j < dwAround; j++) for (k = 0; k < 2; k++) {
         PCPoint p;
         p = pPoints + (k * dwAround + j);
         p->Zero();
         fp fAngle, fAngle2;
         fp fEyeOpen = m_apEyeInfo[i].p[2];

         if (k == 0) {
            fAngle = (fp)j / (fp)dwAround * 360.0;
            fAngle2 = fBump;

            fAngle = fAngle / 360.0 * 2.0 * PI;
            fAngle2 = fAngle2 / 360.0 * 2.0 * PI;

            // goes clocksize arund as fAngle (or j) increases
            p->p[2] = cos(fAngle) * cos(fAngle2);
            p->p[0] = sin(fAngle) * cos(fAngle2);
            p->p[1] = -sin(fAngle2);
            p->Scale (INNERSCALE * m_fDiameter / 2.0);
         }
         else { // && k==1
            p->p[0] = 0;
            p->p[1] = p[-(int)dwAround].p[1];
            p->p[2] = p[-(int)dwAround].p[2] * m_pCatEyes.p[0];
            p->Average (&p[-(int)dwAround], fEyeOpen);

            fCenterDepth = min(p->p[1], fCenterDepth);
         }


         // create a normal from this
         if (pNorm) {
            pNorm[k*dwAround+j].Copy (p);
            pNorm[k*dwAround+j].Normalize();
         }

         // create textures from this
         if (pText) {
            // main point
            PTEXTPOINT5 pt = &pText[k*2*(dwAround+1)+j];
            PTEXTPOINT5 ptDup = (j == 0) ? (pt + dwAround) : NULL;
            pt->xyz[0] = p->p[0];
            pt->xyz[1] = p->p[1];
            pt->xyz[2] = p->p[2];
            pt->hv[0] = 0;
            pt->hv[1] = 0;
            if (ptDup)
               *ptDup = *pt;

            // inner ring
            if (k == 1) {
               PTEXTPOINT5 pIn;
               pIn = &pText[(k*2-1)*(dwAround+1)+j];
               ptDup = (j == 0) ? (pIn + dwAround) : NULL;
               *pIn = *pt;
               pIn->hv[0] = 1;
               if (ptDup)
                  *ptDup = *pIn;
            } // if k==1: inner ring
         } // if texture
      } // j
      
      // do center point
      pPoints[dwAround*2].Zero();
      pPoints[dwAround*2].p[1] = fCenterDepth;
      if (pNorm) {
         pNorm[dwAround*2].Copy (&pPoints[dwAround*2]);
         pNorm[dwAround*2].Normalize();
      }
      if (pText) {
         // add in central texture
         pText[(dwAround+1)*3].xyz[0] = pPoints[dwAround*2].p[0];
         pText[(dwAround+1)*3].xyz[1] = pPoints[dwAround*2].p[1];
         pText[(dwAround+1)*3].xyz[2] = pPoints[dwAround*2].p[2];
         pText[(dwAround+1)*3].hv[0] = 1;
         pText[(dwAround+1)*3].hv[1] = 0;
            // not exactly a good texture map for hv, but it's a pupil

         // rotate all the textures
         m_Renderrs.ApplyTextureRotation (pText, (dwAround+1)*3+1);

         // override hv rotation
         for (j = 0; j < dwAround+1; j++) {
            pText[(dwAround+1)*0+j].hv[0] = pText[(dwAround+1)*1+j].hv[0] = -(fp)j / (fp)dwAround * (fp)m_dwRepeatIris;
            pText[(dwAround+1)*0+j].hv[1] = 1;
            pText[(dwAround+1)*1+j].hv[1] = 0;
         } // j,k

      }

      // color for the iris
      dwColorWant = 2;
      pos = ObjectSurfaceFind (dwColorWant + dwColorInc);
      if (!pos && dwColorInc) {
         // copy over low-color
         pos = ObjectSurfaceFind (dwColorWant);
         if (pos)
            pos = pos->Clone();
         if (pos) {
            pos->m_dwID = dwColorWant + dwColorInc;
            ObjectSurfaceAdd (pos);
            delete pos;
         }
         pos = ObjectSurfaceFind (dwColorWant + dwColorInc);
      }
      m_Renderrs.SetDefMaterial (m_OSINFO.dwRenderShard, pos, m_pWorld);

      // create the for the iris
      DWORD dwColor;
      dwColor = m_Renderrs.DefColor();
      for (j = 0; j < dwAround+1; j++) for (k = 0; k < 2; k++) {
         pVert[k*(dwAround+1)+j].dwColor = dwColor;
         pVert[k*(dwAround+1)+j].dwPoint = dwPoints + (j%dwAround) + k * dwAround;
         pVert[k*(dwAround+1)+j].dwNormal = pNorm ? (dwNorm + (j%dwAround) + k * dwAround) : 0;
         pVert[k*(dwAround+1)+j].dwTexture = pText ? (dwText + j + k*(dwAround+1)) : 0;
      } // j,k

      // draw iris
      for (j = 0; j < dwAround; j++)
         m_Renderrs.NewQuad (
            dwVert + 0*(dwAround+1) + j,
            dwVert + 0*(dwAround+1) + j+1,
            dwVert + 1*(dwAround+1) + j+1,
            dwVert + 1*(dwAround+1) + j);


      // color for the pupil
      dwColorWant = 3;
      pos = ObjectSurfaceFind (dwColorWant + dwColorInc);
      if (!pos && dwColorInc) {
         // copy over low-color
         pos = ObjectSurfaceFind (dwColorWant);
         if (pos)
            pos = pos->Clone();
         if (pos) {
            pos->m_dwID = dwColorWant + dwColorInc;
            ObjectSurfaceAdd (pos);
            delete pos;
         }
         pos = ObjectSurfaceFind (dwColorWant + dwColorInc);
      }
      m_Renderrs.SetDefMaterial (m_OSINFO.dwRenderShard, pos, m_pWorld);

      // create vertices for the pupil
      dwColor = m_Renderrs.DefColor();
      k = 2;
      for (j = 0; j < dwAround+1; j++) {
         pVert[k*(dwAround+1)+j].dwColor = dwColor;
         pVert[k*(dwAround+1)+j].dwPoint = dwPoints + (j%dwAround) + 1 * dwAround;
         pVert[k*(dwAround+1)+j].dwNormal = pNorm ? (dwNorm + (j%dwAround) + 1 * dwAround) : 0;
         pVert[k*(dwAround+1)+j].dwTexture = pText ? (dwText + j + k*(dwAround+1)) : 0;
      } // j,k
      
      // center point
      pVert[3*(dwAround+1)].dwColor = dwColor;
      pVert[3*(dwAround+1)].dwPoint = dwPoints + 2 * dwAround;
      pVert[3*(dwAround+1)].dwNormal = pNorm ? (dwNorm + 2*dwAround) : 0;
      pVert[3*(dwAround+1)].dwTexture = pText ? (dwText + 3 *(dwAround+1)) : 0;

      // draw pupil
      for (j = 0; j < dwAround; j++)
         m_Renderrs.NewTriangle (
            dwVert + 2*(dwAround+1) + j,
            dwVert + 2*(dwAround+1) + j+1,
            dwVert + 3*(dwAround+1));

      m_Renderrs.Pop();
   } // i

   m_Renderrs.Commit();
}



/**********************************************************************************
CObjectEye::QueryBoundingBox - Standard API
*/
void CObjectEye::QueryBoundingBox (PCPoint pCorner1, PCPoint pCorner2, DWORD dwSubObject)
{
   pCorner1->p[0] = -m_fDiameter/2;
   if (m_dwNumEyes >= 2)
      pCorner1->p[0] -= m_fApart/2;
   pCorner1->p[1] = -m_fDiameter/2 * (1.0 + m_fCorneaHeight);
   pCorner1->p[2] = -m_fDiameter/2;

   pCorner2->Copy (pCorner1);
   pCorner2->Scale (-1);

#ifdef _DEBUG
   // test, make sure bounding box not too small
   CPoint p1,p2;
   DWORD i;
   CObjectTemplate::QueryBoundingBox (&p1, &p2, dwSubObject);
   for (i = 0; i < 3; i++)
      if ((p1.p[i] < pCorner1->p[i] - CLOSE) || (p2.p[i] > pCorner2->p[i] + CLOSE))
         break;
   if (i < 3)
      OutputDebugString ("\r\nCObjectEye::QueryBoundingBox too small.");
#endif
}

/**********************************************************************************
CObjectEye::Clone - Clones this

inputs
   none
returns
   PCObjectSocket - Clone
*/
PCObjectSocket CObjectEye::Clone (void)
{
   PCObjectEye pNew;

   pNew = new CObjectEye(NULL, &m_OSINFO);

   // clone template info
   CloneTemplate(pNew);


   pNew->m_fDiameter = m_fDiameter;
   pNew->m_fCorneaArc = m_fCorneaArc;
   pNew->m_fCorneaHeight = m_fCorneaHeight;
   pNew->m_fApart = m_fApart;
   pNew->m_dwNumEyes = m_dwNumEyes;
   pNew->m_fIndepColor = m_fIndepColor;
   pNew->m_fIndepMove = m_fIndepMove;
   pNew->m_dwRepeatSclera = m_dwRepeatSclera;
   pNew->m_dwRepeatIris = m_dwRepeatIris;
   pNew->m_pCatEyes.Copy (&m_pCatEyes);
   pNew->m_apEyeInfo[0].Copy (&m_apEyeInfo[0]);
   pNew->m_apEyeInfo[1].Copy (&m_apEyeInfo[1]);
   pNew->m_fRevValid = FALSE;
   pNew->m_dwPrevRes = 0;

   return pNew;
}

static PWSTR gpszObjectEye = L"ObjectEye";
static PWSTR gpszDiameter = L"Diameter";
static PWSTR gpszCorneaArc = L"CorneaArc";
static PWSTR gpszCorneaHeight = L"CorneaHeight";
static PWSTR gpszApart = L"Apart";
static PWSTR gpszNumEyes = L"NumEyes";
static PWSTR gpszIndepColor = L"IndepColor";
static PWSTR gpszIndepMove = L"IndepMove";
static PWSTR gpszCatEyes = L"CatEyes";
static PWSTR gpszEyeInfo0 = L"EyeInfo0";
static PWSTR gpszEyeInfo1 = L"EyeInfo1";
static PWSTR gpszRepeatSclera = L"RepeatSclera";
static PWSTR gpszRepeatIris = L"RepeatIris";

PCMMLNode2 CObjectEye::MMLTo (void)
{
   // call into the template first
   PCMMLNode2 pNode = MMLToTemplate();
   if (!pNode)
      return NULL;
   pNode->NameSet (gpszObjectEye);

   MMLValueSet (pNode, gpszDiameter, m_fDiameter);
   MMLValueSet (pNode, gpszCorneaArc, m_fCorneaArc);
   MMLValueSet (pNode, gpszCorneaHeight, m_fCorneaHeight);
   MMLValueSet (pNode, gpszApart, m_fApart);
   MMLValueSet (pNode, gpszNumEyes, (int)m_dwNumEyes);
   MMLValueSet (pNode, gpszIndepColor, (int)m_fIndepColor);
   MMLValueSet (pNode, gpszIndepMove, (int)m_fIndepMove);
   MMLValueSet (pNode, gpszRepeatSclera, (int)m_dwRepeatSclera);
   MMLValueSet (pNode, gpszRepeatIris, (int)m_dwRepeatIris);
   MMLValueSet (pNode, gpszCatEyes, &m_pCatEyes);
   MMLValueSet (pNode, gpszEyeInfo0, &m_apEyeInfo[0]);
   MMLValueSet (pNode, gpszEyeInfo1, &m_apEyeInfo[1]);

   return pNode;
}

BOOL CObjectEye::MMLFrom (PCMMLNode2 pNode)
{
   // call into the template first
   if (!MMLFromTemplate (pNode))
      return FALSE;

   m_fDiameter = MMLValueGetDouble (pNode, gpszDiameter, .1);
   m_fCorneaArc = MMLValueGetDouble (pNode, gpszCorneaArc, .1);
   m_fCorneaHeight = MMLValueGetDouble (pNode, gpszCorneaHeight, .1);
   m_fApart = MMLValueGetDouble (pNode, gpszApart, .1);
   MMLValueGetPoint (pNode, gpszEyeInfo0, &m_apEyeInfo[0]);
   MMLValueGetPoint (pNode, gpszEyeInfo1, &m_apEyeInfo[1]);
   MMLValueGetPoint (pNode, gpszCatEyes, &m_pCatEyes);
   m_dwNumEyes = MMLValueGetInt (pNode, gpszNumEyes, (int)1);
   m_fIndepColor = (BOOL) MMLValueGetInt (pNode, gpszIndepColor, (int)0);
   m_fIndepMove = (BOOL) MMLValueGetInt (pNode, gpszIndepMove, (int)0);
   m_dwRepeatSclera = (DWORD) MMLValueGetInt (pNode, gpszRepeatSclera, (int)1);
   m_dwRepeatIris = (DWORD) MMLValueGetInt (pNode, gpszRepeatIris, (int)1);

   m_fRevValid = FALSE;
   return TRUE;
}




/**************************************************************************************
CObjectEye::MoveReferencePointQuery - 
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
BOOL CObjectEye::MoveReferencePointQuery (DWORD dwIndex, PCPoint pp)
{
   PSPLINEMOVEP ps;
   DWORD dwDataSize;
   ps = gaEyeMove;
   dwDataSize = sizeof(gaEyeMove);

   if (dwIndex >= dwDataSize / sizeof(SPLINEMOVEP))
      return FALSE;

   // always at 0,0 in Eyes
   pp->Zero();
   return TRUE;
}

/**************************************************************************************
CObjectEye::MoveReferenceStringQuery -
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
BOOL CObjectEye::MoveReferenceStringQuery (DWORD dwIndex, PWSTR psz, DWORD dwSize, DWORD *pdwNeeded)
{
   PSPLINEMOVEP ps;
   DWORD dwDataSize;
   ps = gaEyeMove;
   dwDataSize = sizeof(gaEyeMove);
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
CObjectEye::EyeInfoFromPoint - Given a point in space, figure out the eye info.
This fillis in m_apEyeInfo[dwEye].

inputs
   DWORD       dwEye - Eye 0 or 1
   PCPoint     pLoc - Location (eyes face in 0,-1,0 direction normally)
returns
   none
*/
void CObjectEye::EyeInfoFromPoint (DWORD dwEye, PCPoint pLoc)
{
   // convert the location, taking out the position of the eye
   CPoint p;
   p.Copy (pLoc);
   if (m_dwNumEyes > 1)
      p.p[0] -= m_fApart / 2.0 * (dwEye ? -1 : 1);
   m_apEyeInfo[dwEye].p[3] = p.Length();

   // convert to opposite direction and call it b
   CPoint A, B, C;
   B.Copy (&p);
   B.Scale (-1);
   B.Normalize();
   A.Zero();
   A.p[0] = 1;
   C.CrossProd (&A, &B);
   C.Normalize();
   A.CrossProd (&B, &C);
   A.Normalize();
   // NOTE: Not doing checks for AxB = 0 since eye shouldn't be able to look that far away anyway

   // make a matrix
   CMatrix m;
   m.RotationFromVectors (&A, &B, &C);
   fp f;
   m.ToXYZLLT (&p, &m_apEyeInfo[dwEye].p[0], &m_apEyeInfo[dwEye].p[1], &f);
}


/**********************************************************************************
CObjectEye::EyeInfoToPoint - Given an eye, fills in the point in space that it's
lookint at.

inputs
   DWORD       dwEye - Eye 0 or 1
   PCPoint     pLoc - Fills in point in space that it's looking at
returns
   none
*/
void CObjectEye::EyeInfoToPoint (DWORD dwEye, PCPoint pLoc)
{
   CPoint pTrans;
   pTrans.Zero();
   if (m_dwNumEyes > 1)
      pTrans.p[0] = m_fApart / 2.0 * (dwEye ? -1 : 1);

   CMatrix mRot;
   mRot.FromXYZLLT (&pTrans, m_apEyeInfo[dwEye].p[0], m_apEyeInfo[dwEye].p[1], 0);

   pLoc->Zero();
   pLoc->p[1] = -m_apEyeInfo[dwEye].p[3];
   pLoc->p[3] = 1;
   pLoc->MultiplyLeft (&mRot);
}



/*************************************************************************************
CObjectEye::ControlPointQuery - Called to query the information about a control
point identified by dwID.

inputs
   DWORD       dwID - ID
   POSCONTROL  pInfo - Filled in with the infomration
returns
   BOOL - TRUE if successful
*/
BOOL CObjectEye::ControlPointQuery (DWORD dwID, POSCONTROL pInfo)
{
   if (dwID >= m_dwNumEyes)
      return FALSE;
   if (dwID && !m_fIndepMove)
      return FALSE;  // must have independent flag set

   memset (pInfo,0, sizeof(*pInfo));

   pInfo->dwID = dwID;
//   pInfo->dwFreedom = 0;   // any direction
   pInfo->dwStyle = CPSTYLE_POINTER;
   pInfo->fSize = m_fDiameter;
   pInfo->cColor = RGB(0xff,0x80,0);

   if ((m_dwNumEyes <= 1) || !m_fIndepMove)
      wcscpy (pInfo->szName, L"Eyes look at");
   else if (dwID == 0)
      wcscpy (pInfo->szName, L"L eye look at");
   else //if (dwID == 1)
      wcscpy (pInfo->szName, L"R eye look at");

   MeasureToString (m_apEyeInfo[dwID].p[3], pInfo->szMeasurement);

   EyeInfoToPoint (dwID, &pInfo->pLocation);

   pInfo->fPassUp = TRUE;

   return TRUE;
}

/*************************************************************************************
CObjectEye::ControlPointSet - Called to change the valud of a control point.

inputs
   DWORD       dwID - ID
   PCPoint     pVal - Contains the new location, in object coordinates
returns
   BOOL - TRUE if successful
*/
BOOL CObjectEye::ControlPointSet (DWORD dwID, PCPoint pVal, PCPoint pViewer)
{
   if (dwID >= m_dwNumEyes)
      return FALSE;
   if (dwID && !m_fIndepMove)
      return FALSE;  // must have independent flag set

   // tell the world we're about to change
   if (m_pWorld)
      m_pWorld->ObjectAboutToChange (this);

   DWORD i;
   if (m_fIndepMove)
      EyeInfoFromPoint (dwID, pVal);
   else
      for (i = 0; i < m_dwNumEyes; i++)
         EyeInfoFromPoint (i, pVal);

   // tell the world we've changed
   if (m_pWorld)
      m_pWorld->ObjectChanged (this);

   return TRUE;
}

/*************************************************************************************
CObjectEye::ControlPointEnum - Called to enumerate a list of control point IDs
into the list.

inputs
   PCListFixed       plDWORD - Should be filled in with control points
returns
   none
*/
void CObjectEye::ControlPointEnum (PCListFixed plDWORD)
{
   DWORD i;

   DWORD dwNum = m_dwNumEyes;
   if (!m_fIndepMove)
      dwNum = 1;
   plDWORD->Required (plDWORD->Num() + dwNum);
   for (i = 0; i < dwNum; i++)
      plDWORD->Add (&i);
}




static PWSTR gpszAttribEye[4] = {L"Eyes LR angle", L"Eyes UD angle", L"Eyes dilation", L"Eyes focus distance"};
static PWSTR gpszAttribLEye[4] = {L"L eye LR angle", L"L eye UD angle", L"L eye dilation", L"L eye focus distance"};
static PWSTR gpszAttribREye[4] = {L"R eye LR angle", L"R eye UD angle", L"R eye dilation", L"R eye focus distance"};

/*****************************************************************************************
CObjectEye::AttribGetIntern - OVERRIDE THIS

Like AttribGet() except that only called if default attributes not handled.
*/
BOOL CObjectEye::AttribGetIntern (PWSTR pszName, fp *pfValue)
{
   DWORD i;
   if ((m_dwNumEyes <= 1) || !m_fIndepMove) {
      for (i = 0; i < 4; i++)
         if (!_wcsicmp(pszName, gpszAttribEye[i])) {
            *pfValue = m_apEyeInfo[0].p[i];
            return TRUE;
         }
      return FALSE;
   }
   
   // else two eyes
   PWSTR *ppsz;
   DWORD dwEye;
   if (towlower(pszName[0]) == L'l') {
      ppsz = gpszAttribLEye;
      dwEye = 0;
   }
   else if (towlower(pszName[0]) == L'r') {
      ppsz = gpszAttribREye;
      dwEye = 1;
   }
   else
      return FALSE;

   for (i = 0; i < 4; i++)
      if (!_wcsicmp(pszName, ppsz[i])) {
         *pfValue = m_apEyeInfo[dwEye].p[i];
         return TRUE;
      }
   return FALSE;
}


/*****************************************************************************************
CObjectEye::AttribGetAllIntern - OVERRIDE THIS

Like AttribGetAllIntern() EXCEPT plATTRIBVAL is already initialized and filled with
some parameters (default to the object template)
*/
void CObjectEye::AttribGetAllIntern (PCListFixed plATTRIBVAL)
{
   ATTRIBVAL av;
   DWORD i, j;
   memset (&av, 0, sizeof(av));

   if ((m_dwNumEyes <= 1) || !m_fIndepMove) {
      for (i = 0; i < 4; i++) {
         wcscpy (av.szName, gpszAttribEye[i]);
         av.fValue = m_apEyeInfo[0].p[i];
         plATTRIBVAL->Add (&av);
      }
      return;
   }
   
   // else two eyes
   for (j = 0; j < 2; j++) {
      PWSTR *ppsz = j ? gpszAttribREye : gpszAttribLEye;
      for (i = 0; i < 4; i++) {
         wcscpy (av.szName, ppsz[i]);
         av.fValue = m_apEyeInfo[j].p[i];
         plATTRIBVAL->Add (&av);
      }
   }
}


/*****************************************************************************************
CObjectEye::AttribSetGroupIntern - OVERRIDE THIS

Like AttribSetGroup() except passing on non-template attributes.
*/
void CObjectEye::AttribSetGroupIntern (DWORD dwNum, PATTRIBVAL paAttrib)
{
   BOOL fChanged = FALSE;

#define CHANGED if (!fChanged && m_pWorld) m_pWorld->ObjectAboutToChange(this); fChanged = TRUE

   BOOL fOnlyOne = ((m_dwNumEyes <= 1) || !m_fIndepMove);
   DWORD i, j, k;
   for (i = 0; i < dwNum; i++, paAttrib++) {
      if (fOnlyOne) {
         for (j = 0; j < 4; j++)
            if (!_wcsicmp(paAttrib->szName, gpszAttribEye[j]))
               break;
         if (j < 4) {
            // found
            if (paAttrib->fValue == m_apEyeInfo[0].p[j])
               continue;   // no change

            CHANGED;
            m_apEyeInfo[0].p[j] = paAttrib->fValue;
            continue;
         }
      } // if only one

      // else, two eyes
      if (towlower(paAttrib->szName[0]) == L'l')
         k = 0;
      else if (towlower(paAttrib->szName[0]) == L'r')
         k = 1;
      else
         continue;   // error

      PWSTR *ppsz = k ? gpszAttribREye : gpszAttribLEye;
      for (j = 0; j < 4; j++)
         if (!_wcsicmp(paAttrib->szName, ppsz[j]))
            break;
      if (j < 4) {
         // found
         if (paAttrib->fValue == m_apEyeInfo[k].p[j])
            break;   // no change

         CHANGED;
         m_apEyeInfo[k].p[j] = paAttrib->fValue;
         break;
      }

   }

   if (fChanged) {
      if ((m_dwNumEyes > 1) || !m_fIndepMove) {
         // moved one eye, must move the others
         CPoint p;
         EyeInfoToPoint (0, &p);

         for (i = 1; i < m_dwNumEyes; i++) {
            EyeInfoToPoint (i, &p); // BUGFIX - Was EyeInfoFromPoint
            m_apEyeInfo[i].p[2] = m_apEyeInfo[0].p[2];   // dilation
         }
      }
      if (m_pWorld)
         m_pWorld->ObjectChanged (this);
   }
}


/*****************************************************************************************
CObjectEye::AttribInfoIntern - OVERRIDE THIS

Like AttribInfo() except called if attribute is not for template.
*/
BOOL CObjectEye::AttribInfoIntern (PWSTR pszName, PATTRIBINFO pInfo)
{
   DWORD i;
   if ((m_dwNumEyes <= 1) || !m_fIndepMove) {
      for (i = 0; i < 4; i++)
         if (!_wcsicmp(pszName, gpszAttribEye[i]))
            break;
      if (i >= 4)
         return FALSE;
   }
   else {
      // else two eyes
      PWSTR *ppsz;
      DWORD dwEye;
      if (towlower(pszName[0]) == L'l') {
         ppsz = gpszAttribLEye;
         dwEye = 0;
      }
      else if (towlower(pszName[0]) == L'r') {
         ppsz = gpszAttribREye;
         dwEye = 1;
      }
      else
         return FALSE;

      for (i = 0; i < 4; i++)
         if (!_wcsicmp(pszName, ppsz[i]))
            break;
      if (i >= 4)
         return FALSE;
   }

   memset (pInfo, 0, sizeof(*pInfo));
   switch (i) {
      case 0:  // LR angle
      case 1:  // UD angle
         pInfo->dwType = AIT_ANGLE;
         pInfo->fMin = -PI/2;
         pInfo->fMax = PI/2;
         pInfo->pszDescription = (i == 0) ?
            L"Angle left/right that the eye looks towards." :
            L"Angle up/down that the eye looks towards.";
         break;
      case 2:  // dilation
         pInfo->dwType = AIT_NUMBER;
         pInfo->fMin = 0;
         pInfo->fMax = 1;
         pInfo->pszDescription = L"Amount the eye is dilated.";
         break;
      case 3:  // distance
         pInfo->dwType = AIT_DISTANCE;
         pInfo->fMin = 0;
         pInfo->fMax = 10;
         pInfo->pszDescription = L"Distance of the object which the eye is looking at.";
         break;
   }
   pInfo->fDefPassUp = TRUE;
   pInfo->fDefLowRank = FALSE;

   return TRUE;
}



/****************************************************************************
EyePage
*/
BOOL EyePage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCObjectEye pv = (PCObjectEye)pPage->m_pUserData;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         PCEscControl pControl;

         MeasureToString (pPage, L"diameter", pv->m_fDiameter);
         MeasureToString (pPage, L"apart", pv->m_fApart);
         AngleToControl (pPage, L"cateyes1", pv->m_pCatEyes.p[1], TRUE);
         DoubleToControl (pPage, L"repeatsclera", pv->m_dwRepeatSclera);
         DoubleToControl (pPage, L"repeatiris", pv->m_dwRepeatIris);


         if (pControl = pPage->ControlFind (L"corneaarc"))
            pControl->AttribSetInt (Pos(), (int)(pv->m_fCorneaArc * 100.0));
         if (pControl = pPage->ControlFind (L"corneaheight"))
            pControl->AttribSetInt (Pos(), (int)(pv->m_fCorneaHeight * 100.0));
         if (pControl = pPage->ControlFind (L"cateyes0"))
            pControl->AttribSetInt (Pos(), (int)(pv->m_pCatEyes.p[0] * 100.0));

         pControl = pPage->ControlFind (L"numeyes");
         if (pControl)
            pControl->AttribSetBOOL (Checked(), pv->m_dwNumEyes > 1);
         pControl = pPage->ControlFind (L"indepcolor");
         if (pControl)
            pControl->AttribSetBOOL (Checked(), pv->m_fIndepColor);
         pControl = pPage->ControlFind (L"indepmove");
         if (pControl)
            pControl->AttribSetBOOL (Checked(), pv->m_fIndepMove);
      }
      break;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;
         PWSTR psz;
         psz = p->pControl->m_pszName;

         if (!_wcsicmp(psz, L"numeyes")) {
            pv->m_pWorld->ObjectAboutToChange (pv);
            pv->m_dwNumEyes = (p->pControl->AttribGetBOOL (Checked()) ? 2 : 1);
            pv->m_pWorld->ObjectChanged (pv);
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"indepcolor")) {
            pv->m_pWorld->ObjectAboutToChange (pv);
            pv->m_fIndepColor = p->pControl->AttribGetBOOL (Checked());
            pv->m_pWorld->ObjectChanged (pv);
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"indepmove")) {
            pv->m_pWorld->ObjectAboutToChange (pv);
            pv->m_fIndepMove = p->pControl->AttribGetBOOL (Checked());

            if (!pv->m_fIndepMove) {
               // moved one eye, must move the others
               CPoint p;
               DWORD i;
               pv->EyeInfoToPoint (0, &p);

               for (i = 1; i < pv->m_dwNumEyes; i++) {
                  pv->EyeInfoFromPoint (i, &p);
                  pv->m_apEyeInfo[i].p[2] = pv->m_apEyeInfo[0].p[2];   // dilation
               }
            }
            pv->m_pWorld->ObjectChanged (pv);
            return TRUE;
         }
      }
      break;

   case ESCN_SCROLL:
   case ESCN_SCROLLING:
      {
         PESCNSCROLL p = (PESCNSCROLL) pParam;

         // only do one scroll bar
         if (!p->pControl->m_pszName)
            break;

         fp *pf = NULL;
         DWORD *pdw = NULL;
         if (!_wcsicmp(p->pControl->m_pszName, L"corneaarc"))
            pf = &pv->m_fCorneaArc;
         else if (!_wcsicmp(p->pControl->m_pszName, L"corneaheight"))
            pf = &pv->m_fCorneaHeight;
         else if (!_wcsicmp(p->pControl->m_pszName, L"cateyes0"))
            pf = &pv->m_pCatEyes.p[0];

         if (!pf && !pdw)
            break;   // default since not one of ours

         fp fVal;
         DWORD dwVal;
         fVal = p->pControl->AttribGetInt (Pos()) / 100.0;
         dwVal = (DWORD)p->pControl->AttribGetInt (Pos());

         if (pf && (*pf == fVal))
            return TRUE;   // no change
         if (pdw && (*pdw == dwVal))
            return TRUE;   // no change

         pv->m_pWorld->ObjectAboutToChange (pv);
         if (pf)
            *pf = fVal;
         if (pdw)
            *pdw = dwVal;
         pv->m_fRevValid = FALSE;   // so recalc eyes
         pv->m_pWorld->ObjectChanged (pv);
         return TRUE;
      }
      break;

   case ESCN_EDITCHANGE:
      {
         PESCNEDITCHANGE p = (PESCNEDITCHANGE) pParam;

         pv->m_pWorld->ObjectAboutToChange (pv);


         MeasureParseString (pPage, L"diameter", &pv->m_fDiameter);
         pv->m_fDiameter = max(pv->m_fDiameter, CLOSE);

         MeasureParseString (pPage, L"apart", &pv->m_fApart);
         pv->m_fApart = max(pv->m_fApart, CLOSE);

         pv->m_pCatEyes.p[1] = AngleFromControl (pPage, L"cateyes1");
         pv->m_dwRepeatSclera = (DWORD) DoubleFromControl (pPage, L"repeatsclera");
         pv->m_dwRepeatSclera = max(pv->m_dwRepeatSclera, 1);
         pv->m_dwRepeatIris = (DWORD) DoubleFromControl (pPage, L"repeatiris");
         pv->m_dwRepeatIris = max(pv->m_dwRepeatIris, 1);

         pv->m_fRevValid = FALSE;

         pv->m_pWorld->ObjectChanged (pv);
      }
      break;


   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Eye settings";
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}


/**********************************************************************************
CObjectEye::DialogShow - Causes the object to show a dialog box that allows
different attributes to be changed.

inputs
   DWORD             dwSuface - Surface that was clicked on. 0 if unknown.
   PCEscWindow       pWindow - Escarpment window to draw it in
returns
   BOOL - TRUE if user pressed "Back", FALSE if user pressed "Close"
*/
BOOL CObjectEye::DialogShow (DWORD dwSurface, PCEscWindow pWindow, PCPoint pClick)
{
   PWSTR pszRet;

   pszRet = pWindow->PageDialog (ghInstance, IDR_MMLEYE, EyePage, this);
   if (!pszRet)
      return FALSE;

   return !_wcsicmp(pszRet, Back());
}

/**********************************************************************************
CObjectEye::DialogQuery - Returns TRUE if the object supports a custom
dialog box, FALSE if it doesn't. The default object doesn't
*/
BOOL CObjectEye::DialogQuery (void)
{
   return TRUE;
}



