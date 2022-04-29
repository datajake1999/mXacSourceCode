/************************************************************************
CObjectJaw.cpp - Draws a Jaw.

begun 9/12/03 by Mike Rozak
Copyright 2003 Mike Rozak. All rights reserved
*/

#include <windows.h>
#include "escarpment.h"
#include "..\M3D.h"
#include "resource.h"


// where CP's start
#define CPBASE_JAW            0
#define CPBASE_GUMS           (CPBASE_JAW + NUMJAWCP)
#define CPBASE_ROOF           (CPBASE_GUMS + 4*NUMJAWCP)
#define CPBASE_SIZE           (CPBASE_ROOF + 2)

typedef struct {
   PCObjectJaw       pJaw;       // this jaw
   int               iVScroll;   // scroll loc
} OJP, *POJP;

typedef struct {
   PWSTR       pszName;    // name of the move point
   int         iX, iY; // -1 if for left/top, 1 for right/bottom
} SPLINEMOVEP, *PSPLINEMOVEP;

static SPLINEMOVEP   gaJawMove[] = {
   L"Base", 0, 0
};

/**********************************************************************************
CObjectJaw::Constructor and destructor */
CObjectJaw::CObjectJaw (PVOID pParams, POSINFO pInfo)
{
   m_dwRenderShow = RENDERSHOW_CHARACTER;
   m_OSINFO = *pInfo;

   m_fLowerJaw = TRUE;
   m_fTeethSymmetry = TRUE;
   m_dwTeethMissing = 0;
   DWORD i;
   memset (m_apJawSpline, 0, sizeof(m_apJawSpline));
   memset (m_apGumSpline, 0, sizeof(m_apGumSpline));
   memset (m_apRoof, 0, sizeof(m_apRoof));
   for (i = 0; i < NUMJAWCP; i++) {
      fp fAngle = (fp)i / (fp)NUMJAWCP * PI / 2;

      m_apJawSpline[i].p[0] = cos(fAngle) * 0.03;
      m_apJawSpline[i].p[1] = -sin(fAngle) * 0.10;
      m_apJawSpline[i].p[2] = 0;

      // outer gums
      m_apGumSpline[i][1].p[0] = -0.005;
      m_apGumSpline[i][1].p[2] = -0.003;
      m_apGumSpline[i][0].Copy (&m_apGumSpline[i][1]);
      m_apGumSpline[i][0].p[2] -= 0.015;

      // inner gums
      m_apGumSpline[i][2].p[0] = 0.005;
      m_apGumSpline[i][2].p[2] = -0.003;
      m_apGumSpline[i][3].Copy (&m_apGumSpline[i][2]);
      m_apGumSpline[i][3].p[0] *= 2;
      m_apGumSpline[i][3].p[2] -= 0.015;
   }
   m_apRoof[0].p[2] = m_apGumSpline[0][3].p[2] * 2;
   m_apRoof[1].Copy (&m_apRoof[0]);
   m_apRoof[1].p[1] = m_apJawSpline[NUMJAWCP-1].p[1] + m_apGumSpline[NUMJAWCP-1][3].p[0] * 2;
   m_fTeethDeltaZ = 0.003;

   m_pTongueSize.Zero();
   m_pTongueLimit.Zero();
   m_pTongueSize.p[0] = m_apJawSpline[0].p[0] * 2.0 * 0.8;
   m_pTongueSize.p[1] = -m_apJawSpline[NUMJAWCP-1].p[1] * 0.98;
   m_pTongueSize.p[2] = m_pTongueSize.p[0] / 5;
   m_pTongueSize.p[3] = .5;
   m_pTongueLimit.p[0] = m_pTongueSize.p[0] / 2;
   m_pTongueLimit.p[1] = 0.7; // BUGFIX - Was .5
   m_fTongueShow = TRUE;
   m_tpTongueLoc.h = 1;
   m_tpTongueLoc.v = 0;

   m_lJAWTOOTH.Init (sizeof(JAWTOOTH));
   
   // fill in with a few teeth
#define TEMPTEETH       6
   JAWTOOTH jt;
   memset (&jt, 0, sizeof(jt));
   jt.mTransRot.Identity();
   m_lJAWTOOTH.Required (TEMPTEETH);
   for (i = 0; i < TEMPTEETH; i++) {
      jt.fLoc = ((fp)i / (fp)(TEMPTEETH-1) - 0.5) * 0.25 + 0.5;
      jt.pTooth = new CTooth;
      m_lJAWTOOTH.Add (&jt);
   } // i

   m_dwUserDetail = 0;
   m_fDirty = m_fTongueDirty = TRUE;
   m_dwDetail = 0;
   m_apBound[0].Zero();
   m_apBound[1].Zero();

   m_pTongue = new CNoodle;

   // color for the Jaw
   ObjectSurfaceAdd (1, RGB(0x80,0,0x40), MATERIAL_TILEGLAZED);   // gums
   ObjectSurfaceAdd (2, RGB(0x60,0,0x30), MATERIAL_FLAT);   // tongue
   ObjectSurfaceAdd (3, RGB(0xff,0xff,0xf0), MATERIAL_TILEGLAZED); // teeth
}


CObjectJaw::~CObjectJaw (void)
{
   // delete teeth
   DWORD i;
   PJAWTOOTH pjt = (PJAWTOOTH)m_lJAWTOOTH.Get(0);
   for (i = 0; i < m_lJAWTOOTH.Num(); i++)
      if (pjt[i].pTooth)
         delete pjt[i].pTooth;
   m_lJAWTOOTH.Clear();

   // delete the tongue
   if (m_pTongue)
      delete m_pTongue;

}


/**********************************************************************************
CObjectJaw::Delete - Called to delete this object
*/
void CObjectJaw::Delete (void)
{
   delete this;
}

/**********************************************************************************
CObjectJaw::Render - Draws a test box.

inputs
   POBJECTRENDER     pr - Render information
*/
void CObjectJaw::Render (POBJECTRENDER pr, DWORD dwSubObject)
{
   if (!CalcRenderIfNecessary(pr))
      return;

   // create the surface render object and draw
   // CRenderSurface rs;
   m_Renderrs.ClearAll();

   CMatrix mObject;
   m_Renderrs.Init (pr->pRS);
   ObjectMatrixGet (&mObject);
   m_Renderrs.Multiply (&mObject);

   // draw the gums
   m_Renderrs.SetDefMaterial (m_OSINFO.dwRenderShard, ObjectSurfaceFind (1), m_pWorld);
   PCPoint pPoint, pNorm;
   PVERTEX pVert;
   PTEXTPOINT5 pText;
   DWORD dwPoint, dwNorm, dwText, dwVert, i;
   pPoint = m_Renderrs.NewPoints (&dwPoint, (DWORD)m_memJawPoint.m_dwCurPosn / sizeof(CPoint));
   pNorm = m_Renderrs.NewNormals (TRUE, &dwNorm, (DWORD)m_memJawNorm.m_dwCurPosn / sizeof(CPoint));
   pVert = m_Renderrs.NewVertices (&dwVert, (DWORD)m_memJawVert.m_dwCurPosn / sizeof(VERTEX));
   pText = m_Renderrs.NewTextures (&dwText, (DWORD)m_memJawText.m_dwCurPosn / sizeof(TEXTPOINT5));
   if (!pPoint || !pVert) {
      m_Renderrs.Commit();
      return;
   }
   memcpy (pPoint, m_memJawPoint.p, m_memJawPoint.m_dwCurPosn);
   if (pNorm)
      memcpy (pNorm, m_memJawNorm.p, m_memJawNorm.m_dwCurPosn);
   if (pText) {
      memcpy (pText, m_memJawText.p, m_memJawText.m_dwCurPosn);
      m_Renderrs.ApplyTextureRotation (pText, (DWORD)m_memJawText.m_dwCurPosn / sizeof(TEXTPOINT5));
   }
   memcpy (pVert, m_memJawVert.p, m_memJawVert.m_dwCurPosn);
   DWORD dwDefColor = m_Renderrs.DefColor();
   for (i = 0; i < m_memJawVert.m_dwCurPosn / sizeof(VERTEX); i++, pVert++) {
      pVert->dwColor = dwDefColor;
      pVert->dwNormal = pNorm ? (pVert->dwNormal + dwNorm) : 0;
      pVert->dwPoint = pVert->dwPoint + dwPoint;
      pVert->dwTexture = pText ? (pVert->dwTexture + dwText) : 0;
   } // i
   DWORD *padwPoly = (DWORD*)m_memJawPoly.p;
   for (i = 0; i < m_memJawPoly.m_dwCurPosn / 4 / sizeof(DWORD); i++, padwPoly += 4) {
      if (padwPoly[3] != -1)
         m_Renderrs.NewQuad (padwPoly[0] + dwVert, padwPoly[1] + dwVert, padwPoly[2] + dwVert,
            padwPoly[3] + dwVert);
      else
         m_Renderrs.NewTriangle (padwPoly[0] + dwVert, padwPoly[1] + dwVert, padwPoly[2] + dwVert);
   } // i

   // draw teeth
   PJAWTOOTH pjt = (PJAWTOOTH)m_lJAWTOOTH.Get(0);
   CMatrix mAllTeeth, mTemp;
   mAllTeeth.RotationZ (-PI/2);
   mTemp.Translation (0, 0, -m_fTeethDeltaZ);
   mAllTeeth.MultiplyRight (&mTemp);
   if (!m_fLowerJaw) {
      mTemp.RotationX (PI);
      mAllTeeth.MultiplyRight (&mTemp);
   }
   DWORD dwMissing = m_dwTeethMissing;
   for (i = 0; i < m_lJAWTOOTH.Num(); i++, pjt++, dwMissing = (dwMissing >> 1) ) {
      if (dwMissing & 0x01)
         continue;   // no tooth

      m_Renderrs.Commit();   // so RT will be faster

      // rotate into place...
      m_Renderrs.Push();
      SplineLocToMatrix (pjt->fLoc, &mTemp);
      m_Renderrs.Multiply (&mTemp);
      m_Renderrs.Multiply (&mAllTeeth);
      m_Renderrs.Multiply (&pjt->mTransRot);

      m_Renderrs.SetDefMaterial (m_OSINFO.dwRenderShard, ObjectSurfaceFind (3), m_pWorld);
      pjt->pTooth->Render(pr, &m_Renderrs);

      // unrotate
      m_Renderrs.Pop();
   }
   m_Renderrs.Commit();   // so RT will be faster

   // draw tongue..
   if (m_fTongueShow && m_pTongue) {
      m_Renderrs.SetDefMaterial (m_OSINFO.dwRenderShard, ObjectSurfaceFind (2), m_pWorld);
      m_pTongue->Render (pr, &m_Renderrs);
   }

   m_Renderrs.Commit();
}





/**********************************************************************************
CObjectJaw::QueryBoundingBox - Standard API
*/
void CObjectJaw::QueryBoundingBox (PCPoint pCorner1, PCPoint pCorner2, DWORD dwSubObject)
{
   BOOL fSet = FALSE;
   CPoint p1, p2;
   pCorner1->Zero();
   pCorner2->Zero();


   if (!CalcRenderIfNecessary(NULL))
      return;

   PCPoint pp = (PCPoint)m_memJawPoint.p;
   DWORD i;
   for (i = 0; i < m_memJawPoint.m_dwCurPosn / sizeof(CPoint); i++, pp++) {
      if (fSet) {
         pCorner1->Min (pp);
         pCorner2->Max (pp);
      }
      else {
         pCorner1->Copy (pp);
         pCorner2->Copy (pp);
         fSet = TRUE;
      }
   } // i



   // draw teeth
   PJAWTOOTH pjt = (PJAWTOOTH)m_lJAWTOOTH.Get(0);
   CMatrix mAllTeeth, mTemp;
   mAllTeeth.RotationZ (-PI/2);
   mTemp.Translation (0, 0, -m_fTeethDeltaZ);
   mAllTeeth.MultiplyRight (&mTemp);
   if (!m_fLowerJaw) {
      mTemp.RotationX (PI);
      mAllTeeth.MultiplyRight (&mTemp);
   }
   DWORD dwMissing = m_dwTeethMissing;
   for (i = 0; i < m_lJAWTOOTH.Num(); i++, pjt++, dwMissing = (dwMissing >> 1) ) {
      if (dwMissing & 0x01)
         continue;   // no tooth

      // rotate into place...
      SplineLocToMatrix (pjt->fLoc, &mTemp);
      mTemp.MultiplyLeft (&mAllTeeth);
      mTemp.MultiplyLeft (&pjt->mTransRot);

      pjt->pTooth->QueryBoundingBox (&p1, &p2);

      BoundingBoxApplyMatrix (&p1, &p2, &mTemp);

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

   // draw tongue..
   if (m_fTongueShow && m_pTongue) {
      m_pTongue->QueryBoundingBox (&p1, &p2);

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
      OutputDebugString ("\r\nCObjectJaw::QueryBoundingBox too small.");
#endif
}


/**********************************************************************************
CObjectJaw::Clone - Clones this

inputs
   none
returns
   PCObjectSocket - Clone
*/
PCObjectSocket CObjectJaw::Clone (void)
{
   PCObjectJaw pNew;

   pNew = new CObjectJaw(NULL, &m_OSINFO);

   // clone template info
   CloneTemplate(pNew);

   // delete existing teeth
   DWORD i;
   PJAWTOOTH pjt = (PJAWTOOTH)pNew->m_lJAWTOOTH.Get(0);
   for (i = 0; i < pNew->m_lJAWTOOTH.Num(); i++)
      if (pjt[i].pTooth)
         delete pjt[i].pTooth;
   pNew->m_lJAWTOOTH.Clear();

   pNew->m_fLowerJaw = m_fLowerJaw;
   pNew->m_fTeethSymmetry = m_fTeethSymmetry;
   pNew->m_dwTeethMissing = m_dwTeethMissing;
   memcpy (pNew->m_apJawSpline, m_apJawSpline, sizeof(m_apJawSpline));
   memcpy (pNew->m_apGumSpline, m_apGumSpline, sizeof(m_apGumSpline));
   memcpy (pNew->m_apRoof, m_apRoof, sizeof(m_apRoof));
   pNew->m_fTeethDeltaZ = m_fTeethDeltaZ;
   pNew->m_dwUserDetail = m_dwUserDetail;
   pNew->m_dwDetail = m_dwDetail;
   pNew->m_fDirty = pNew->m_fTongueDirty = TRUE;  // so will redraw
   pNew->m_pTongueSize.Copy (&m_pTongueSize);
   pNew->m_pTongueLimit.Copy (&m_pTongueLimit);
   pNew->m_fTongueShow =m_fTongueShow;
   pNew->m_tpTongueLoc =m_tpTongueLoc;

   // copy over jawtooth
   pNew->m_lJAWTOOTH.Init (sizeof(JAWTOOTH), m_lJAWTOOTH.Get(0), m_lJAWTOOTH.Num());
   pjt = (PJAWTOOTH)pNew->m_lJAWTOOTH.Get(0);
   for (i = 0; i < pNew->m_lJAWTOOTH.Num(); i++)
      if (pjt[i].pTooth)
         pjt[i].pTooth = pjt[i].pTooth->Clone();

   return pNew;
}

static PWSTR gpszLowerJaw = L"LowerJaw";
static PWSTR gpszTeethSymmetry = L"TeethSymmetry";
static PWSTR gpszUserDetail = L"UserDetail";
static PWSTR gpszTeethDeltaZ = L"TeethDeltaZ";
static PWSTR gpszTooth = L"Tooth";
static PWSTR gpszLoc = L"Loc";
static PWSTR gpszTransRot = L"TransRot";
static PWSTR gpszTongueSize = L"TongueSize";
static PWSTR gpszTongueLimit = L"TongueLimit";
static PWSTR gpszTongueShow = L"TongueShow";
static PWSTR gpszTongueLoc = L"TongueLoc";
static PWSTR gpszTeethMissing = L"TeethMissing";

PCMMLNode2 CObjectJaw::MMLTo (void)
{
   // call into the template first
   PCMMLNode2 pNode = MMLToTemplate();
   if (!pNode)
      return NULL;

   MMLValueSet (pNode, gpszLowerJaw, (int)m_fLowerJaw);
   MMLValueSet (pNode, gpszTeethSymmetry, (int)m_fTeethSymmetry);
   MMLValueSet (pNode, gpszTeethMissing, (int)m_dwTeethMissing);
   MMLValueSet (pNode, gpszUserDetail, (int)m_dwUserDetail);
   MMLValueSet (pNode, gpszTeethDeltaZ, m_fTeethDeltaZ);
   MMLValueSet (pNode, gpszTongueSize, &m_pTongueSize);
   MMLValueSet (pNode, gpszTongueLimit, &m_pTongueLimit);
   MMLValueSet (pNode, gpszTongueShow, (int)m_fTongueShow);
   MMLValueSet (pNode, gpszTongueLoc, &m_tpTongueLoc);

   WCHAR szTemp[32];
   DWORD i,j;
   for (i = 0; i < NUMJAWCP; i++) {
      swprintf (szTemp,L"JawSpline%d", (int) i);
      MMLValueSet (pNode, szTemp, &m_apJawSpline[i]);

      for (j = 0; j < 4; j++) {
         swprintf (szTemp,L"GawSpline%d%d", (int) i, (int)j);
         MMLValueSet (pNode, szTemp, &m_apGumSpline[i][j]);
      } // j
   } // i
   for (i = 0; i < 2; i++) {
      swprintf (szTemp,L"Roof%d", (int) i);
      MMLValueSet (pNode, szTemp, &m_apRoof[i]);
   } // i

   // and all the teeth
   PJAWTOOTH pjt = (PJAWTOOTH)m_lJAWTOOTH.Get(0);
   for (i = 0; i < m_lJAWTOOTH.Num(); i++, pjt++) {
      if (!pjt->pTooth)
         continue;

      PCMMLNode2 pSub = pjt->pTooth->MMLTo();
      if (!pSub)
         continue;
      pSub->NameSet (gpszTooth);

      MMLValueSet (pSub, gpszLoc, pjt->fLoc);
      MMLValueSet (pSub, gpszTransRot, &pjt->mTransRot);

      pNode->ContentAdd (pSub);
   }
   return pNode;
}

BOOL CObjectJaw::MMLFrom (PCMMLNode2 pNode)
{
   // delete existing teeth
   DWORD i;
   PJAWTOOTH pjt = (PJAWTOOTH)m_lJAWTOOTH.Get(0);
   for (i = 0; i < m_lJAWTOOTH.Num(); i++)
      if (pjt[i].pTooth)
         delete pjt[i].pTooth;
   m_lJAWTOOTH.Clear();

   m_fDirty = TRUE;
   m_fTongueDirty = TRUE;
   m_dwDetail = 0;

   // call into the template first
   if (!MMLFromTemplate (pNode))
      return FALSE;

   m_fLowerJaw = (BOOL) MMLValueGetInt (pNode, gpszLowerJaw, (int)0);
   m_fTeethSymmetry = (BOOL) MMLValueGetInt (pNode, gpszTeethSymmetry, (int)1);
   m_dwTeethMissing = (DWORD) MMLValueGetInt (pNode, gpszTeethMissing, 0);
   m_dwUserDetail = (DWORD) MMLValueGetInt (pNode, gpszUserDetail, (int)0);
   m_fTeethDeltaZ = MMLValueGetDouble (pNode, gpszTeethDeltaZ, 0);
   MMLValueGetPoint (pNode, gpszTongueSize, &m_pTongueSize);
   MMLValueGetPoint (pNode, gpszTongueLimit, &m_pTongueLimit);
   MMLValueGetTEXTUREPOINT (pNode, gpszTongueLoc, &m_tpTongueLoc);
   m_fTongueShow = (BOOL) MMLValueGetInt (pNode, gpszTongueShow, (int)0);

   WCHAR szTemp[32];
   DWORD j;
   for (i = 0; i < NUMJAWCP; i++) {
      swprintf (szTemp,L"JawSpline%d", (int) i);
      MMLValueGetPoint (pNode, szTemp, &m_apJawSpline[i]);

      for (j = 0; j < 4; j++) {
         swprintf (szTemp,L"GawSpline%d%d", (int) i, (int)j);
         MMLValueGetPoint (pNode, szTemp, &m_apGumSpline[i][j]);
      } // j
   } // i
   for (i = 0; i < 2; i++) {
      swprintf (szTemp,L"Roof%d", (int) i);
      MMLValueGetPoint (pNode, szTemp, &m_apRoof[i]);
   } // i

   // and all the teeth
   PWSTR psz;
   PCMMLNode2 pSub;
   for (i = 0; i < pNode->ContentNum(); i++) {
      pSub = NULL;
      pNode->ContentEnum (i, &psz, &pSub);
      if (!pSub)
         continue;
      psz = pSub->NameGet();
      if (!psz)
         continue;
      if (!_wcsicmp(psz, gpszTooth)) {
         JAWTOOTH jt;
         memset (&jt, 0, sizeof(jt));
         jt.fLoc = MMLValueGetDouble (pSub, gpszLoc, 0);
         MMLValueGetMatrix (pSub, gpszTransRot, &jt.mTransRot);
         jt.pTooth = new CTooth;
         if (!jt.pTooth)
            continue;
         if (!jt.pTooth->MMLFrom (pSub)) {
            delete jt.pTooth;
            continue;
         }
         m_lJAWTOOTH.Add (&jt);
      }
   } // i


   return TRUE;
}



/**************************************************************************************
CObjectJaw::MoveReferencePointQuery - 
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
BOOL CObjectJaw::MoveReferencePointQuery (DWORD dwIndex, PCPoint pp)
{
   PSPLINEMOVEP ps;
   DWORD dwDataSize;
   ps = gaJawMove;
   dwDataSize = sizeof(gaJawMove);

   if (dwIndex >= dwDataSize / sizeof(SPLINEMOVEP))
      return FALSE;

   // always at 0,0 in Jaws
   pp->Zero();

   return TRUE;
}

/**************************************************************************************
CObjectJaw::MoveReferenceStringQuery -
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
BOOL CObjectJaw::MoveReferenceStringQuery (DWORD dwIndex, PWSTR psz, DWORD dwSize, DWORD *pdwNeeded)
{
   PSPLINEMOVEP ps;
   DWORD dwDataSize;
   ps = gaJawMove;
   dwDataSize = sizeof(gaJawMove);
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




/****************************************************************************
JawPage
*/
BOOL JawPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   POJP pt = (POJP)pPage->m_pUserData;
   PCObjectJaw pv = pt->pJaw;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         PCEscControl pControl;

         DWORD i;
         WCHAR szTemp[32];
         MeasureToString (pPage, L"teethdeltaz", pv->m_fTeethDeltaZ, TRUE);
         for (i = 0; i < 3; i++) {
            swprintf (szTemp, L"tonguesize%d", (int)i);
            MeasureToString (pPage, szTemp, pv->m_pTongueSize.p[i]);
         }
         MeasureToString (pPage, L"tonguelimit0", pv->m_pTongueLimit.p[0]);

         ComboBoxSet (pPage, L"userdetail", pv->m_dwUserDetail);

         pControl = pPage->ControlFind (L"lowerjaw");
         if (pControl)
            pControl->AttribSetBOOL (Checked(), pv->m_fLowerJaw);
         pControl = pPage->ControlFind (L"teethsymmetry");
         if (pControl)
            pControl->AttribSetBOOL (Checked(), pv->m_fTeethSymmetry);
         pControl = pPage->ControlFind (L"tongueshow");
         if (pControl)
            pControl->AttribSetBOOL (Checked(), pv->m_fTongueShow);

         pControl = pPage->ControlFind (L"tonguelimit1");
         if (pControl)
            pControl->AttribSetInt (Pos(), (int)(pv->m_pTongueLimit.p[1] * 100.0));
         pControl = pPage->ControlFind (L"tonguesize3");
         if (pControl)
            pControl->AttribSetInt (Pos(), (int)(pv->m_pTongueSize.p[3] * 100.0));

         for (i = 0; i < pv->m_lJAWTOOTH.Num(); i++)
            pPage->Message (ESCM_USER+82, (PVOID)(size_t)i);

         // Handle scroll on redosamepage
         if (pt->iVScroll >= 0) {
            pPage->VScroll (pt->iVScroll);

            // when bring up pop-up dialog often they're scrolled wrong because
            // iVScoll was left as valeu, and they use defpage
            pt->iVScroll = 0;

            // BUGFIX - putting this invalidate in to hopefully fix a refresh
            // problem when add or move a task in the ProjectView page
            pPage->Invalidate();
         }

      }
      break;

   case ESCM_USER+82:   // for updating controls with tooth
      {
         DWORD i = (DWORD)(size_t)pParam;
         PJAWTOOTH pjt = (PJAWTOOTH)pv->m_lJAWTOOTH.Get(i);
         DWORD j;
         WCHAR szTemp[32];
         PCEscControl pControl;

         // get the location..
         CPoint pLoc, pRot;
         pjt->mTransRot.ToXYZLLT (&pLoc, &pRot.p[2], &pRot.p[0], &pRot.p[1]);

         swprintf (szTemp, L"loc%d", (int) i);
         pControl = pPage->ControlFind (szTemp);
         if (pControl)
            pControl->AttribSetInt (Pos(), (int)(pjt->fLoc * 1000.0));

         for (j = 0; j < 3; j++) {
            swprintf (szTemp, L"offset%d%d", (int)j, (int) i);
            MeasureToString (pPage, szTemp, pLoc.p[j], TRUE);

            swprintf (szTemp, L"rot%d%d", (int)j, (int) i);
            AngleToControl (pPage, szTemp, pRot.p[j]);
         } // j
      }
      return TRUE;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;
         PWSTR psz;
         if (!p->pControl || !(psz = p->pControl->m_pszName))
            break;

         PWSTR pszDelete = L"delete";
         DWORD dwDeleteLen = (DWORD)wcslen(pszDelete);
         if (!wcsncmp(psz, pszDelete, dwDeleteLen)) {
            DWORD dwTooth = _wtoi(psz + dwDeleteLen);

            // find a mirror?
            DWORD dwMirror = -1;
            if (pv->m_fTeethSymmetry)
               dwMirror = pv->FindMirror (dwTooth);

            PJAWTOOTH pjt = (PJAWTOOTH)pv->m_lJAWTOOTH.Get(0);

            pv->m_pWorld->ObjectAboutToChange(pv);
            delete pjt[dwTooth].pTooth;
            if (dwMirror != -1)
               delete pjt[dwMirror].pTooth;

            if ((dwMirror != -1) && (dwMirror > dwTooth)) {
               pv->m_lJAWTOOTH.Remove (dwMirror);
               dwMirror = -1;
            }
            pv->m_lJAWTOOTH.Remove (dwTooth);
            if (dwMirror != -1)
               pv->m_lJAWTOOTH.Remove (dwMirror);

            pv->m_pWorld->ObjectChanged(pv);

            pPage->Exit (RedoSamePage());
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"lowerjaw")) {
            BOOL fNew = p->pControl->AttribGetBOOL (Checked());
            if (fNew == pv->m_fLowerJaw)
               return TRUE;   // no change

            DWORD i, j;
            pv->m_pWorld->ObjectAboutToChange(pv);
            pv->m_fLowerJaw = fNew;
            pv->m_fDirty = TRUE;
            pv->m_fTongueDirty = TRUE;
            for (i = 0; i < NUMJAWCP; i++) {
               pv->m_apJawSpline[i].p[2] *= -1; // since invert
               for (j = 0; j < 4; j++)
                  pv->m_apGumSpline[i][j].p[2] *= -1; // since invert
            } // i
            for (i = 0; i < 2; i++)
               pv->m_apRoof[i].p[2] *= -1;

            PJAWTOOTH pjt = (PJAWTOOTH)pv->m_lJAWTOOTH.Get(0);
            for (i = 0; i < pv->m_lJAWTOOTH.Num(); i++, pjt++) {
               CPoint pTrans, pRot;
               pjt->mTransRot.ToXYZLLT (&pTrans, &pRot.p[2], &pRot.p[0], &pRot.p[1]);
               //pTrans.p[2] *= -1;
               pRot.p[2] *= -1;
               pRot.p[1] *= -1;
               pjt->mTransRot.FromXYZLLT (&pTrans, pRot.p[2], pRot.p[0], pRot.p[1]);
            } // i

            pv->m_pWorld->ObjectChanged(pv);
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"teethsymmetry")) {
            pv->m_pWorld->ObjectAboutToChange(pv);
            pv->m_fTeethSymmetry = p->pControl->AttribGetBOOL (Checked());
            pv->m_pWorld->ObjectChanged(pv);
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"tongueshow")) {
            pv->m_pWorld->ObjectAboutToChange(pv);
            pv->m_fTongueShow = p->pControl->AttribGetBOOL (Checked());
            pv->m_fTongueDirty = TRUE;
            pv->m_pWorld->ObjectChanged(pv);
            return TRUE;
         }
      }
      break;


   case ESCN_COMBOBOXSELCHANGE:
      {
         PESCNCOMBOBOXSELCHANGE p = (PESCNCOMBOBOXSELCHANGE) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;
         PWSTR psz;
         psz = p->pControl->m_pszName;
         DWORD dwVal;
         dwVal = p->pszName ? (DWORD) _wtoi(p->pszName) : 0;

         if (!_wcsicmp(psz, L"userdetail")) {
            if (dwVal == pv->m_dwUserDetail)
               break;

            // else, change
            pv->m_pWorld->ObjectAboutToChange (pv);

            pv->m_dwUserDetail = dwVal;
            pv->m_fDirty = TRUE;
            pv->m_fTongueDirty = TRUE;
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
         PWSTR psz = p->pControl->m_pszName;
         if (!psz)
            break;

         PWSTR pszLoc = L"loc";
         DWORD dwLocLen = (DWORD)wcslen(pszLoc);

         if (!_wcsicmp(psz, L"tonguelimit1")) {
            pv->m_pWorld->ObjectAboutToChange (pv);
            pv->m_pTongueLimit.p[1] = (fp)p->iPos / 100.0;
            pv->m_fTongueDirty = TRUE;
            pv->m_pWorld->ObjectChanged (pv);
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"tonguesize3")) {
            pv->m_pWorld->ObjectAboutToChange (pv);
            pv->m_pTongueSize.p[3] = (fp)p->iPos / 100.0;
            pv->m_fTongueDirty = TRUE;
            pv->m_pWorld->ObjectChanged (pv);
            return TRUE;
         }
         else if (!wcsncmp(psz, pszLoc, dwLocLen)) {
            DWORD dwTooth = _wtoi(psz + dwLocLen);
            // find a mirror?
            DWORD dwMirror = -1;
            if (pv->m_fTeethSymmetry)
               dwMirror = pv->FindMirror (dwTooth);


            pv->m_pWorld->ObjectAboutToChange (pv);

            PJAWTOOTH pjt = (PJAWTOOTH)pv->m_lJAWTOOTH.Get(0);
            pjt[dwTooth].fLoc = (fp)p->iPos / 1000.0;

            if (dwMirror != -1) {
               pjt[dwMirror].fLoc = 1.0 - pjt[dwTooth].fLoc;
               pPage->Message (ESCM_USER+82, (PVOID)(size_t)dwMirror);
            }
   
            // NOTE: Don't set m_fDirty since havent dirtied the jaw
            pv->m_pWorld->ObjectChanged (pv);
            return TRUE;
         }

      }
      break;

   case ESCN_EDITCHANGE:
      {
         PESCNEDITCHANGE p = (PESCNEDITCHANGE) pParam;
         PWSTR psz = p->pControl->m_pszName;
         PWSTR pszOffset = L"offset";
         DWORD dwLenOffset = (DWORD)wcslen(pszOffset);
         PWSTR pszRot = L"rot";
         DWORD dwRotLen = (DWORD)wcslen(pszRot);

         if (psz && !wcsncmp(psz, pszRot, dwRotLen)) {
            // changed an offset param, figure out which one
            DWORD dwDim = psz[dwRotLen] - L'0';
            DWORD dwTooth = _wtoi(psz + (dwRotLen+1));

            // find a mirror?
            DWORD dwMirror = -1;
            if (pv->m_fTeethSymmetry)
               dwMirror = pv->FindMirror (dwTooth);

            // get all the rotation and location for the tooth
            PJAWTOOTH pjt = (PJAWTOOTH)pv->m_lJAWTOOTH.Get(0);
            CPoint pLoc, pRot;
            pjt[dwTooth].mTransRot.ToXYZLLT (&pLoc, &pRot.p[2], &pRot.p[0], &pRot.p[1]);
            pRot.p[dwDim] = AngleFromControl (pPage, psz);

            pv->m_pWorld->ObjectAboutToChange (pv);

            // convert to a matrix...
            pjt[dwTooth].mTransRot.FromXYZLLT (&pLoc, pRot.p[2], pRot.p[0], pRot.p[1]);

            // do mirror?
            pLoc.p[0] *= -1;
            pRot.p[1] *= -1;
            pRot.p[2] *= -1;
            if (dwMirror != -1) {
               pjt[dwMirror].mTransRot.FromXYZLLT (&pLoc, pRot.p[2], pRot.p[0], pRot.p[1]);
               pPage->Message (ESCM_USER+82, (PVOID)(size_t)dwMirror);
            }
   
            // NOTE: Don't set m_fDirty since havent dirtied the jaw
            pv->m_pWorld->ObjectChanged (pv);
            return TRUE;
         }
         else if (psz && !wcsncmp(psz, pszOffset, dwLenOffset)) {
            // changed an offset param, figure out which one
            DWORD dwDim = psz[dwLenOffset] - L'0';
            DWORD dwTooth = _wtoi(psz + (dwLenOffset+1));

            // find a mirror?
            DWORD dwMirror = -1;
            if (pv->m_fTeethSymmetry)
               dwMirror = pv->FindMirror (dwTooth);

            // get all the rotation and location for the tooth
            PJAWTOOTH pjt = (PJAWTOOTH)pv->m_lJAWTOOTH.Get(0);
            CPoint pLoc, pRot;
            pjt[dwTooth].mTransRot.ToXYZLLT (&pLoc, &pRot.p[2], &pRot.p[0], &pRot.p[1]);
            MeasureParseString (pPage, psz, &pLoc.p[dwDim]);

            pv->m_pWorld->ObjectAboutToChange (pv);

            // convert to a matrix...
            pjt[dwTooth].mTransRot.FromXYZLLT (&pLoc, pRot.p[2], pRot.p[0], pRot.p[1]);

            // do mirror?
            pLoc.p[0] *= -1;
            pRot.p[1] *= -1;
            pRot.p[2] *= -1;
            if (dwMirror != -1) {
               pjt[dwMirror].mTransRot.FromXYZLLT (&pLoc, pRot.p[2], pRot.p[0], pRot.p[1]);
               pPage->Message (ESCM_USER+82, (PVOID)(size_t)dwMirror);
            }
   
            // NOTE: Don't set m_fDirty since havent dirtied the jaw
            pv->m_pWorld->ObjectChanged (pv);
            return TRUE;
         }


         // else, get the rest
         pv->m_pWorld->ObjectAboutToChange (pv);
         MeasureParseString (pPage, L"teethdeltaz", &pv->m_fTeethDeltaZ);

         DWORD i;
         WCHAR szTemp[32];
         for (i = 0; i < 3; i++) {
            swprintf (szTemp, L"tonguesize%d", (int)i);
            MeasureParseString (pPage, szTemp, &pv->m_pTongueSize.p[i]);
            pv->m_pTongueSize.p[i] = max(pv->m_pTongueSize.p[i], .001);
         }
         MeasureParseString (pPage, L"tonguelimit0", &pv->m_pTongueLimit.p[0]);

         pv->m_fDirty = TRUE;
         pv->m_fTongueDirty = TRUE;
         pv->m_pWorld->ObjectChanged (pv);
      }
      break;


   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Jaw settings";
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"TOOTH")) {
            MemZero (&gMemTemp);

            PJAWTOOTH pjt = (PJAWTOOTH)pv->m_lJAWTOOTH.Get(0);
            DWORD i;
            for (i = 0; i < pv->m_lJAWTOOTH.Num(); i++, pjt++) {
               MemCat (&gMemTemp, L"<xtrheader>Tooth ");
               MemCat (&gMemTemp, (int)i + 1);
               MemCat (&gMemTemp, L"</xtrheader>"
                  L"<tr><td><bold>Position</bold> - Position of the tooth in the jaw.<p/>"
                  L"<scrollbar orient=horz min=0 max=1000 name=loc");
               MemCat (&gMemTemp, (int)i);
               MemCat (&gMemTemp, L"/></td></tr><tr>"
                  L"<td><bold>Offset</bold> - Offset the location by this many meters/feet.</td>"
                  L"<td align=right>L/R: <edit width=80% maxchars=32 name=offset0");
               MemCat (&gMemTemp, (int)i);
               MemCat (&gMemTemp, L"/><br/>F/B: <edit width=80% maxchars=32 name=offset1");
               MemCat (&gMemTemp, (int)i);
               MemCat (&gMemTemp, L"/><br/>U/D: <edit width=80% maxchars=32 name=offset2");
               MemCat (&gMemTemp, (int)i);
               MemCat (&gMemTemp, L"/></td></tr><tr>"
                  L"<td><bold>Rotation</bold> - Rotate the tooth. \"L/R\", \"F/B\", "
                  L"and \"U/D\" indicates the axis to rotate around.</td>"
                  L"<td align=right>L/R: <edit width=80% maxchars=32 name=rot0");
               MemCat (&gMemTemp, (int)i);
               MemCat (&gMemTemp, L"/><br/>F/B: <edit width=80% maxchars=32 name=rot1");
               MemCat (&gMemTemp, (int)i);
               MemCat (&gMemTemp, L"/><br/>U/D: <edit width=80% maxchars=32 name=rot2");
               MemCat (&gMemTemp, (int)i);
               MemCat (&gMemTemp, L"/></td></tr><tr>"
                  L"<td><button style=righttriangle href=edit");
               MemCat (&gMemTemp, (int)i);
               MemCat (&gMemTemp, L"><bold>Modify tooth shape</bold></button></td>"
                  L"<td><button style=righttriangle href=dup");
               MemCat (&gMemTemp, (int)i);
               MemCat (&gMemTemp, L"><bold>Duplicate</bold></button></td></tr>");
               MemCat (&gMemTemp, L"<tr><td/>"
                  L"<td><button style=righttriangle name=delete");
               MemCat (&gMemTemp, (int)i);
               MemCat (&gMemTemp, L"><bold>Delete</bold></button></td></tr>");
            } // i

            p->pszSubString = (PWSTR)gMemTemp.p;
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}


/**********************************************************************************
JAWTOOTHCompare - Used for sorting attributes in m_lPCOEAttrib
*/
static int _cdecl JAWTOOTHCompare (const void *elem1, const void *elem2)
{
   JAWTOOTH *pdw1, *pdw2;
   pdw1 = (JAWTOOTH*) elem1;
   pdw2 = (JAWTOOTH*) elem2;

   if (pdw1->fLoc > pdw2->fLoc)
      return 1;
   else if (pdw1->fLoc < pdw2->fLoc)
      return -1;
   else
      return 0;
}


/**********************************************************************************
CObjectJaw::DialogShow - Causes the object to show a dialog box that allows
different attributes to be changed.

inputs
   DWORD             dwSuface - Surface that was clicked on. 0 if unknown.
   PCEscWindow       pWindow - Escarpment window to draw it in
returns
   BOOL - TRUE if user pressed "Back", FALSE if user pressed "Close"
*/
BOOL CObjectJaw::DialogShow (DWORD dwSurface, PCEscWindow pWindow, PCPoint pClick)
{
   PWSTR pszRet;
   PWSTR pszEdit = L"edit", pszDup = L"dup";
   DWORD dwEditLen = (DWORD)wcslen(pszEdit), dwDupLen = (DWORD)wcslen(pszDup);
   OJP t;
   memset (&t, 0, sizeof(t));
   t.pJaw = this;

redo:
   // sort all the teeth by their location order...
   qsort (m_lJAWTOOTH.Get(0), m_lJAWTOOTH.Num(), sizeof(JAWTOOTH), JAWTOOTHCompare);

   pszRet = pWindow->PageDialog (ghInstance, IDR_MMLJAW, JawPage, &t);
   t.iVScroll = pWindow->m_iExitVScroll;

   if (!pszRet)
      return FALSE;
   else if (!_wcsicmp(pszRet, RedoSamePage()))
      goto redo;
   else if (!_wcsicmp(pszRet, L"swaplr")) {
      PJAWTOOTH pjt = (PJAWTOOTH)m_lJAWTOOTH.Get(0);
      DWORD i;

      m_pWorld->ObjectAboutToChange (this);

      for (i = 0; i < m_lJAWTOOTH.Num(); i++, pjt++) {
         pjt->fLoc = 1.0 - pjt->fLoc;
         pjt->pTooth->Mirror();

         CPoint pLoc, pRot;
         pjt->mTransRot.ToXYZLLT (&pLoc, &pRot.p[2], &pRot.p[0], &pRot.p[1]);
         pLoc.p[0] *= -1;
         pRot.p[1] *= -1;
         pRot.p[2] *= -1;
         pjt->mTransRot.FromXYZLLT (&pLoc, pRot.p[2], pRot.p[0], pRot.p[1]);
      }
      m_pWorld->ObjectChanged (this);

      goto redo;
   }
   else if (!_wcsicmp(pszRet, L"mirrorlr") || !_wcsicmp(pszRet, L"mirrorrl")) {
      BOOL fLR = !_wcsicmp(pszRet, L"mirrorlr");

      PJAWTOOTH pjt = (PJAWTOOTH)m_lJAWTOOTH.Get(0);
      DWORD i;

      m_pWorld->ObjectAboutToChange (this);

      // delete all on the wrong side
      for (i = m_lJAWTOOTH.Num()-1; i < m_lJAWTOOTH.Num(); i--) {
         pjt = (PJAWTOOTH)m_lJAWTOOTH.Get(i);
         if (fLR && (pjt->fLoc < 0.5))
            continue;   // keep this
         else if (!fLR && (pjt->fLoc > 0.5))
            continue;   // keep this

         // else, delete
         delete pjt->pTooth;
         m_lJAWTOOTH.Remove (i);
         pjt = (PJAWTOOTH)m_lJAWTOOTH.Get(0);
      } // i

      // duplicate
      DWORD dwNum = m_lJAWTOOTH.Num();
      m_lJAWTOOTH.Required (dwNum + m_lJAWTOOTH.Num());
      for (i = 0; i < dwNum; i++) {
         JAWTOOTH jt;
         pjt = (PJAWTOOTH)m_lJAWTOOTH.Get(i);
         jt = pjt[0];

         jt.pTooth = jt.pTooth->Clone();
         jt.fLoc = 1.0 - jt.fLoc;
         jt.pTooth->Mirror();

         CPoint pLoc, pRot;
         jt.mTransRot.ToXYZLLT (&pLoc, &pRot.p[2], &pRot.p[0], &pRot.p[1]);
         pLoc.p[0] *= -1;
         pRot.p[1] *= -1;
         pRot.p[2] *= -1;
         jt.mTransRot.FromXYZLLT (&pLoc, pRot.p[2], pRot.p[0], pRot.p[1]);

         m_lJAWTOOTH.Add (&jt);
      }
      m_pWorld->ObjectChanged (this);

      goto redo;
   }
   else if (!_wcsicmp(pszRet, L"newtooth")) {
      JAWTOOTH jt;
      memset (&jt, 0, sizeof(jt));
      jt.fLoc = 0;
      jt.mTransRot.Identity();
      jt.pTooth = new CTooth;
      m_pWorld->ObjectAboutToChange (this);

      // add duplicate
      m_lJAWTOOTH.Add (&jt);

      // add mirror
      if (m_fTeethSymmetry) {
         jt.pTooth = jt.pTooth->Clone();
         jt.pTooth->Mirror();

         jt.fLoc = 1.0 - jt.fLoc;
         m_lJAWTOOTH.Add (&jt);
      }

      m_pWorld->ObjectChanged (this);

      goto redo;
   }
   else if (!wcsncmp(pszRet, pszDup, dwDupLen)) {
      DWORD dwTooth = _wtoi(pszRet + dwDupLen); // BUGFIX - Was dwEditLen
      PJAWTOOTH pjt = (PJAWTOOTH)m_lJAWTOOTH.Get(0);
      JAWTOOTH jt = pjt[dwTooth];

      m_pWorld->ObjectAboutToChange (this);

      // add duplicate
      jt.pTooth = jt.pTooth->Clone();
      m_lJAWTOOTH.Add (&jt);

      // add mirror
      if (m_fTeethSymmetry) {
         jt.pTooth = jt.pTooth->Clone();
         jt.pTooth->Mirror();

         jt.fLoc = 1.0 - jt.fLoc;
         CPoint pLoc, pRot;
         jt.mTransRot.ToXYZLLT (&pLoc, &pRot.p[2], &pRot.p[0], &pRot.p[1]);
         pLoc.p[0] *= -1;
         pRot.p[1] *= -1;
         pRot.p[2] *= -1;
         jt.mTransRot.FromXYZLLT (&pLoc, pRot.p[2], pRot.p[0], pRot.p[1]);
         m_lJAWTOOTH.Add (&jt);
      }

      m_pWorld->ObjectChanged (this);

      goto redo;
   }
   else if (!wcsncmp(pszRet, pszEdit, dwEditLen)) {
      DWORD dwTooth = _wtoi(pszRet + dwEditLen);
      DWORD dwMirror = -1;
      if (m_fTeethSymmetry)
         dwMirror = FindMirror (dwTooth);
      PJAWTOOTH pjt = (PJAWTOOTH)m_lJAWTOOTH.Get(0);

      if (pjt[dwTooth].pTooth->Dialog (this, (dwMirror != -1) ? pjt[dwMirror].pTooth : NULL, pWindow))
         goto redo;
      return FALSE;     // pressed cancel
   }

   return !_wcsicmp(pszRet, Back());
}

/**********************************************************************************
CObjectJaw::DialogQuery - Returns TRUE if the object supports a custom
dialog box, FALSE if it doesn't. The default object doesn't
*/
BOOL CObjectJaw::DialogQuery (void)
{
   return TRUE;
}



/*************************************************************************************
CObjectJaw::ControlPointQuery - Called to query the information about a control
point identified by dwID.

inputs
   DWORD       dwID - ID
   POSCONTROL  pInfo - Filled in with the infomration
returns
   BOOL - TRUE if successful
*/
BOOL CObjectJaw::ControlPointQuery (DWORD dwID, POSCONTROL pInfo)
{
   DWORD dwType, dwIndex, dwIndex2;
   if ((dwID >= CPBASE_JAW) && (dwID < CPBASE_JAW + NUMJAWCP)) {
      dwType = 0;
      dwIndex = dwID - CPBASE_JAW;
   }
   else if ((dwID >= CPBASE_GUMS) && (dwID < CPBASE_GUMS + 4*NUMJAWCP)) {
      dwType = 1;
      dwIndex = (dwID - CPBASE_GUMS) / 4;
      dwIndex2 = (dwID - CPBASE_GUMS) % 4;
   }
   else if ((dwID >= CPBASE_ROOF) && (dwID < CPBASE_ROOF + 2)) {
      dwType = 2;
      dwIndex = dwID - CPBASE_ROOF;
   }
   else if ((dwID >= CPBASE_SIZE) && (dwID < CPBASE_SIZE + 4)) {
      dwType = 3;
      dwIndex = dwID - CPBASE_SIZE;
   }
   else
      return FALSE;

   // defaults
   memset (pInfo, 0, sizeof(*pInfo));
   pInfo->dwID = dwID;
   CPoint pDist;
   pDist.Subtract (&m_apJawSpline[NUMJAWCP-1], &m_apJawSpline[0]);
   pInfo->fSize = pDist.Length() / 50;

   CMatrix mTransRot;

   switch (dwType) {
   case 0:  // jaw
      pInfo->cColor = RGB(0xff, 0, 0);
      pInfo->dwStyle = CPSTYLE_SPHERE;
      wcscpy (pInfo->szName, L"Shape jaw");
      pInfo->pLocation.Copy (&m_apJawSpline[dwIndex]);
      break;

   case 1:  // gums
      pInfo->cColor = RGB(0xff, 0xff, 0);
      pInfo->dwStyle = CPSTYLE_SPHERE;
      wcscpy (pInfo->szName, L"Shape gums");
      pInfo->pLocation.Copy (&m_apGumSpline[dwIndex][dwIndex2]);
      pInfo->pLocation.p[3] = 1;
      SplineLocToMatrix ((fp)dwIndex / (fp)(NUMJAWCP*2-1), &mTransRot);
      pInfo->pLocation.MultiplyLeft (&mTransRot);
      break;

   case 2:  // roof
      pInfo->cColor = RGB(0xff, 0xff, 0);
      pInfo->dwStyle = CPSTYLE_SPHERE;
      wcscpy (pInfo->szName, L"Shape roof of mouth");
      pInfo->pLocation.Copy (&m_apRoof[dwIndex]);
      break;

   case 3:  // size
      pInfo->cColor = RGB(0xff, 0xff, 0xff);
      pInfo->dwStyle = CPSTYLE_CUBE;
      wcscpy (pInfo->szName, L"Resize the entire jaw");
      if (dwIndex < 3) {
         pInfo->pLocation.Average (&m_apBound[0], &m_apBound[1]);
         pInfo->pLocation.p[dwIndex] = m_apBound[1].p[dwIndex];
      }
      else
         pInfo->pLocation.Copy (&m_apBound[1]);
      break;
   }

   return TRUE;
}

/*************************************************************************************
CObjectJaw::ControlPointSet - Called to change the valud of a control point.

inputs
   DWORD       dwID - ID
   PCPoint     pVal - Contains the new location, in object coordinates
returns
   BOOL - TRUE if successful
*/
BOOL CObjectJaw::ControlPointSet (DWORD dwID, PCPoint pVal, PCPoint pViewer)
{
   DWORD dwType, dwIndex, dwIndex2;
   if ((dwID >= CPBASE_JAW) && (dwID < CPBASE_JAW + NUMJAWCP)) {
      dwType = 0;
      dwIndex = dwID - CPBASE_JAW;
   }
   else if ((dwID >= CPBASE_GUMS) && (dwID < CPBASE_GUMS + 4*NUMJAWCP)) {
      dwType = 1;
      dwIndex = (dwID - CPBASE_GUMS) / 4;
      dwIndex2 = (dwID - CPBASE_GUMS) % 4;
   }
   else if ((dwID >= CPBASE_ROOF) && (dwID < CPBASE_ROOF + 2)) {
      dwType = 2;
      dwIndex = dwID - CPBASE_ROOF;
   }
   else if ((dwID >= CPBASE_SIZE) && (dwID < CPBASE_SIZE + 4)) {
      dwType = 3;
      dwIndex = dwID - CPBASE_SIZE;
   }
   else
      return FALSE;

   CMatrix mTransRot, mInv;
   CPoint pTemp;

   switch (dwType) {
   case 0:  // jaw
      m_pWorld->ObjectAboutToChange(this);

      m_apJawSpline[dwIndex].Copy (pVal);
      m_apJawSpline[dwIndex].p[0] = max(m_apJawSpline[dwIndex].p[0], .001);
      m_fDirty = TRUE;

      m_pWorld->ObjectChanged (this);
      break;

   case 1:  // gums
      SplineLocToMatrix ((fp)dwIndex / (fp)(NUMJAWCP*2-1), &mTransRot);
      mTransRot.Invert4 (&mInv);
      pTemp.Copy (pVal);
      pTemp.p[3] = 1;
      pTemp.MultiplyLeft (&mInv);
      pTemp.p[1] = 0;   // since dont use this

      m_pWorld->ObjectAboutToChange(this);

      m_apGumSpline[dwIndex][dwIndex2].Copy (&pTemp);
      m_fDirty = TRUE;

      m_pWorld->ObjectChanged (this);
      break;

   case 2:  // roof
      m_pWorld->ObjectAboutToChange(this);

      m_apRoof[dwIndex].Copy (pVal);
      m_apRoof[dwIndex].p[0] = 0;
      m_fDirty = TRUE;

      m_pWorld->ObjectChanged (this);
      break;

   case 3:  // size
      {
         DWORD i, j;
         CPoint pSize, pDelta;
         pDelta.Subtract (&m_apBound[1], &m_apBound[0]);
         for (i = 0; i < 3; i++)
            pDelta.p[i] = max(pDelta.p[i], 0.01);
         pSize.p[0] = pSize.p[1] = pSize.p[2] = 1;
         if (dwIndex < 3) {
            pSize.p[dwIndex] = (pVal->p[dwIndex] - m_apBound[0].p[dwIndex]) / pDelta.p[dwIndex];
            pSize.p[dwIndex] = max(pSize.p[dwIndex], .01);
         }
         else {
            for (i = 0; i < 3; i++) {
               pSize.p[i] = (pVal->p[i] - m_apBound[0].p[i]) / pDelta.p[i];
               pSize.p[i] = max(pSize.p[i], .01);
            } // i
         }

         // if sizing in x/y, will need to determine average size for stuff like teeth
         fp fXYSize = sqrt(pSize.p[0] * pSize.p[1]);

         // matrix to convert world space...
         CMatrix mConvert, m;
         mConvert.Translation (0, -(m_apBound[0].p[1] + m_apBound[1].p[1])/2,
            -(m_apBound[0].p[2] + m_apBound[1].p[2])/2);
         m.Scale (pSize.p[0], pSize.p[1], pSize.p[2]);
         mConvert.MultiplyRight (&m);
         m.Translation (0, (m_apBound[0].p[1] + m_apBound[1].p[1])/2,
            (m_apBound[0].p[2] + m_apBound[1].p[2])/2);
         mConvert.MultiplyRight (&m);

         m_pWorld->ObjectAboutToChange(this);

         for (i = 0; i < NUMJAWCP; i++) {
            m_apJawSpline[i].p[3] = 1;
            m_apJawSpline[i].MultiplyLeft (&mConvert);

            for (j = 0; j < 4; j++) {
               m_apGumSpline[i][j].p[0] *= fXYSize;
               m_apGumSpline[i][j].p[2] *= pSize.p[2];
            }
         }
         for (i = 0; i < 2; i++) {
            m_apRoof[i].p[3] = 1;
            m_apRoof[i].MultiplyLeft (&mConvert);
         }
         m_fTeethDeltaZ *= pSize.p[2];

         PJAWTOOTH pjt = (PJAWTOOTH) m_lJAWTOOTH.Get(0);
         for (i = 0; i < m_lJAWTOOTH.Num(); i++, pjt++) {
            CPoint pTrans, pRot;
            pjt->mTransRot.ToXYZLLT (&pTrans, &pRot.p[2], &pRot.p[0], &pRot.p[1]);
            pTrans.p[0] *= fXYSize;
            pTrans.p[1] *= fXYSize;
            pTrans.p[2] *= pSize.p[2];
            pjt->mTransRot.FromXYZLLT (&pTrans, pRot.p[2], pRot.p[0], pRot.p[1]);

            pTrans.p[0] = pTrans.p[1] = fXYSize;
            pTrans.p[2] = pSize.p[2];
            pjt->pTooth->Scale (&pTrans);
         } // i

         // will need to resize tongue
         m_pTongueSize.p[0] *= pSize.p[0];
         m_pTongueSize.p[1] *= pSize.p[1];
         m_pTongueSize.p[2] *= pSize.p[2];
         m_pTongueLimit.p[0] *= pSize.p[2];

         m_fDirty = TRUE;
         m_fTongueDirty = TRUE;
         m_pWorld->ObjectChanged (this);
      }
      break;
   }

   return TRUE;
}

/*************************************************************************************
CObjectJaw::ControlPointEnum - Called to enumerate a list of control point IDs
into the list.

inputs
   PCListFixed       plDWORD - Should be filled in with control points
returns
   none
*/

void CObjectJaw::ControlPointEnum (PCListFixed plDWORD)
{
   DWORD i;

   // basic jaw
   for (i = CPBASE_JAW; i < CPBASE_JAW + NUMJAWCP; i++)
      plDWORD->Add (&i);

   // gums
   for (i = CPBASE_GUMS; i < CPBASE_GUMS + 4*NUMJAWCP; i++)
      plDWORD->Add (&i);

   // roof of mouth
   for (i = CPBASE_ROOF; i < CPBASE_ROOF + 2; i++)
      plDWORD->Add (&i);

   // size CP
   for (i = CPBASE_SIZE; i < CPBASE_SIZE + 4; i++)
      plDWORD->Add (&i);

}


/*************************************************************************************
CObjectJaw::SplineLocToPoint - Takes a location along the jaw spline (from 0 to 1)
and returns a location for it.

inputs
   fp          fLoc - From 0 to 1
   PCPoint     pLoc - Filled in with the location
*/
void CObjectJaw::SplineLocToPoint (fp fLoc, PCPoint pLoc)
{
   // figure out where it is along the spline...
   fLoc = max(0, fLoc);
   fLoc = min(1, fLoc);
   fLoc *= (2.0 * (fp)NUMJAWCP - 1.0);

   // index
   DWORD dwIndex = (DWORD)fLoc;
   dwIndex = min(dwIndex, 2*NUMJAWCP-1);
   fLoc -= (fp)dwIndex;

   // all the points
   DWORD adw[4];
   adw[0] = dwIndex ? (dwIndex - 1) : 0;
   adw[1] = dwIndex;
   adw[2] = min(dwIndex+1, 2*NUMJAWCP-1);
   adw[3] = min(dwIndex+2, 2*NUMJAWCP-1);

   // account for mirroring
   BOOL afMirror[4];
   DWORD i;
   for (i = 0; i < 4; i++) {
      afMirror[i] = (adw[i] >= NUMJAWCP);
      if (!afMirror[i])
         continue;
      adw[i] = NUMJAWCP * 2 - 1 - adw[i]; // mirror
   } // i

   // find points
   for (i = 0; i < 3; i++)
      pLoc->p[i] = HermiteCubic (fLoc,
         m_apJawSpline[adw[0]].p[i] * ((!i && afMirror[0]) ? -1 : 1),
         m_apJawSpline[adw[1]].p[i] * ((!i && afMirror[1]) ? -1 : 1),
         m_apJawSpline[adw[2]].p[i] * ((!i && afMirror[2]) ? -1 : 1),
         m_apJawSpline[adw[3]].p[i] * ((!i && afMirror[3]) ? -1 : 1)
         );

   // done
}



/*************************************************************************************
CObjectJaw::SplineLocToMatrix - Takes a location along the jaw spline (from 0 to 1)
and returns a matrix that converts from a rotateed XYZ space, with 0 offset being the
SplineLocToPoint(fLoc). X points towards the inside of the jaw. Y points in the positive
pLoc direction, Z points up.

inputs
   fp          fLoc - From 0 to 1
   PCMatrix    pMatrix - Filled in
*/
void CObjectJaw::SplineLocToMatrix (fp fLoc, PCMatrix pMatrix)
{
   CPoint pCenter, pNext, pPrev;
   SplineLocToPoint (fLoc, &pCenter);
   SplineLocToPoint (min(fLoc+0.01,1), &pNext);
   SplineLocToPoint (max(fLoc-0.01,0), &pPrev);

   CPoint pA, pB, pC;
   pC.Zero();
   pC.p[2] = 1;
   pB.Subtract (&pNext, &pPrev);
   pA.CrossProd (&pB, &pC);
   pA.Normalize();
   pB.CrossProd (&pC, &pA);
   pB.Normalize();

   pMatrix->RotationFromVectors (&pA, &pB, &pC);

   // include translation
   CMatrix mTrans;
   mTrans.Translation (pCenter.p[0], pCenter.p[1], pCenter.p[2]);
   pMatrix->MultiplyRight (&mTrans);
}

/*************************************************************************************
CObjectJaw::CalcRenderIfNecessary - Calculates the polygons for rendering the jaw
if they're dirty.

inputs
   POBJECTRENDER        pr - Render info
*/
BOOL CObjectJaw::CalcRenderIfNecessary (POBJECTRENDER pr)
{
   // Need to calculate detail level and maybe set dirty
   DWORD dwWant, i;
   if (pr && (pr->dwReason != ORREASON_BOUNDINGBOX)) {
      fp fDetail = pr->pRS->QueryDetail ();
      CPoint pDist;
      pDist.Subtract (&m_apJawSpline[0], &m_apJawSpline[NUMJAWCP-1]);
      fp fMax = pDist.Length() / (fp)NUMJAWCP;

      if (fDetail < fMax / 2)
         dwWant = 2;
      else if (fDetail < fMax)
         dwWant = 1;
      else
         dwWant = 0;

      if (dwWant != m_dwDetail)
         m_fDirty = m_fTongueDirty = TRUE;
   }
   else
      dwWant = m_dwDetail;

   if (m_fDirty) {
      // set detail
      m_dwDetail = dwWant;
      DWORD dwDetail = m_dwDetail + m_dwUserDetail;
      DWORD dwSkip = (1 << dwDetail);

      // how large is this
      DWORD dwWidth = 6;   // always 6 wide: 2 outer CP, 1 jaw CP, 2 inner CP, 1 roof CP
      dwWidth = (dwWidth - 1) * dwSkip + 1;  // account for extra CP
      DWORD dwLength = (NUMJAWCP*2-1)*dwSkip + 1;


      // allocate memory
      DWORD dwNeed = dwWidth * dwLength * sizeof(CPoint);
      if (!m_memJawPoint.Required (dwNeed))
         return FALSE;
      PCPoint pPoint = (PCPoint) m_memJawPoint.p;
      m_memJawPoint.m_dwCurPosn = dwNeed;

      if (!m_memJawNorm.Required (dwNeed))
         return FALSE;
      PCPoint pNorm = (PCPoint) m_memJawNorm.p;
      m_memJawNorm.m_dwCurPosn = dwNeed;

      dwNeed = dwWidth * dwLength * sizeof(TEXTPOINT5);
      if (!m_memJawText.Required (dwNeed))
         return FALSE;
      PTEXTPOINT5 pText = (PTEXTPOINT5) m_memJawText.p;
      m_memJawText.m_dwCurPosn = dwNeed;

      dwNeed = dwWidth * dwLength * sizeof(VERTEX);
      if (!m_memJawVert.Required (dwNeed))
         return FALSE;
      PVERTEX pVert = (PVERTEX) m_memJawVert.p;
      m_memJawVert.m_dwCurPosn = dwNeed;

      dwNeed = (dwWidth-1) * (dwLength-1) * 4 * sizeof(DWORD);
      if (!m_memJawPoly.Required (dwNeed))
         return FALSE;
      DWORD *padwPoly = (DWORD*) m_memJawPoly.p;
      m_memJawPoly.m_dwCurPosn = dwNeed;


      // fill in all the points
      DWORD j, k;
      for (i = 0; i < dwLength; i++) {
         // get the matrix that converts points from rotated space into world space
         CMatrix mTransRot;
         fp fLoc = (fp)i / (fp)(dwLength-1);
         fp fLocOrig = fLoc;
         SplineLocToMatrix (fLoc, &mTransRot);


         // figure out how far into index
         fLoc *= (2.0 * (fp)NUMJAWCP - 1.0);

         // index
         DWORD dwIndex = (DWORD)fLoc;
         dwIndex = min(dwIndex, 2*NUMJAWCP-1);
         fLoc -= (fp)dwIndex;

         // all the points
         DWORD adw[4];
         adw[0] = dwIndex ? (dwIndex - 1) : 0;
         adw[1] = dwIndex;
         adw[2] = min(dwIndex+1, 2*NUMJAWCP-1);
         adw[3] = min(dwIndex+2, 2*NUMJAWCP-1);

         // account for mirroring
         BOOL afMirror[4];
         for (j = 0; j < 4; j++) {
            afMirror[j] = (adw[j] >= NUMJAWCP);
            if (!afMirror[j])
               continue;
            adw[j] = NUMJAWCP * 2 - 1 - adw[j]; // mirror
         } // i

         // determine the values for the gums based on interpolation
         CPoint apGum[4];
         for (j = 0; j < 4; j++) {
            for (k = 0; k < 3; k++)
               apGum[j].p[k] = HermiteCubic (fLoc,
                  m_apGumSpline[adw[0]][j].p[k],
                  m_apGumSpline[adw[1]][j].p[k],
                  m_apGumSpline[adw[2]][j].p[k],
                  m_apGumSpline[adw[3]][j].p[k]
                  );
            apGum[j].p[3] = 1;

            // apply gum to matrix
            apGum[j].MultiplyLeft (&mTransRot);
         }

         // determine the value of the root of the mouth
         if (fLocOrig <= 0.5)
            fLocOrig *= 2;
         else
            fLocOrig = (1.0 - fLocOrig) * 2.0;

         // now, have array of 6 points
         CPoint apPath[6];
         apPath[0].Copy (&apGum[0]);
         apPath[1].Copy (&apGum[1]);
         apPath[2].Zero();
         apPath[2].MultiplyLeft (&mTransRot);
         apPath[3].Copy (&apGum[2]);
         apPath[4].Copy (&apGum[3]);
         apPath[5].Average (&m_apRoof[1], &m_apRoof[0], fLocOrig);

         // fill in the points... more hermite cubic...
         for (j = 0; j < dwWidth; j++) {
            fLoc = (fp)j / (fp)(dwWidth-1) * 6;
            dwIndex = (DWORD)fLoc;
            dwIndex = min(dwIndex, 6-1);
            fLoc -= dwIndex;

            adw[0] = dwIndex ? (dwIndex - 1) : 0;
            adw[1] = dwIndex;
            adw[2] = min(dwIndex+1, 6-1);
            adw[3] = min(dwIndex+2, 6-1);

            for (k = 0; k < 3; k++)
               pPoint[j + i*dwWidth].p[k] = HermiteCubic (fLoc,
                  apPath[adw[0]].p[k],
                  apPath[adw[1]].p[k],
                  apPath[adw[2]].p[k],
                  apPath[adw[3]].p[k]
                  );

            // apply it to the bounding box
            if (i || j) {
               m_apBound[0].Min (&pPoint[j + i*dwWidth]);
               m_apBound[1].Max (&pPoint[j + i*dwWidth]);
            }
            else {
               m_apBound[0].Copy (&pPoint[j + i*dwWidth]);
               m_apBound[1].Copy (&pPoint[j + i*dwWidth]);
            }
         } // j
      } // i, over dwLenght

      // calculate the normals...
      for (i = 0; i < dwLength; i++) for (j = 0; j < dwWidth; j++) {
         // points in different directions...
         PCPoint pForward = pPoint + (j + min(i+1,dwLength-1)*dwWidth);
         PCPoint pBack = pPoint + (j + (i ? (i-1) : 0)*dwWidth);
         PCPoint pRight = pPoint + (min(j+1,dwWidth-1) + i*dwWidth);
         PCPoint pLeft = pPoint + ((j ? (j-1) : 0) + i*dwWidth);

         // vectors
         CPoint pF, pR;
         pF.Subtract (pForward, pBack);
         if (pF.Length() < CLOSE)
            pF.p[0] = 1;   // only should happen when rotating around point in palette
         pR.Subtract (pRight, pLeft);

         // normal
         pNorm[j + i*dwWidth].CrossProd (&pR, &pF);

         // may need to flip normal if drawing roof of mouth
         if (!m_fLowerJaw)
            pNorm[j + i*dwWidth].Scale (-1);


         // if this is at the maximum width then it's appearing right at the roof
         // of the line, so the normal can't have any x component
         if (j+1 == dwWidth)
            pNorm[j + i*dwWidth].p[0] = 0;

         pNorm[j + i*dwWidth].Normalize();
      }

      // determine the texture point...
      fp fTextV = 0;
      fp fLen;
      CPoint pDist;
      for (i = 0; i < dwLength; i++) {
         fp fTextH = 0;
         for (j = 0; j < dwWidth; j++) {
            // store the texture..
            PTEXTPOINT5 ptp = pText + (j + i*dwWidth);
            ptp->hv[0] = fTextH;
            ptp->hv[1] = fTextV;
            ptp->xyz[0] = pPoint[j + i*dwWidth].p[0];
            ptp->xyz[1] = pPoint[j + i*dwWidth].p[1];
            ptp->xyz[2] = pPoint[j + i*dwWidth].p[2];

            // increase texture location
            if (j+1 < dwWidth) {
               pDist.Subtract (&pPoint[j+1 + i*dwWidth], &pPoint[j + i*dwWidth]);
               fLen = pDist.Length();
               fTextH += fLen;
            }
         } // j

         if (i+1 < dwLength) {
            pDist.Subtract (&pPoint[dwSkip*2 + (i+1)*dwWidth], &pPoint[dwSkip*2 + i*dwWidth]);
            fLen = pDist.Length();
            fTextV += (m_fLowerJaw ? -fLen : fLen);
         }
      } // i


      // fill in the vertices
      for (i = 0; i < dwLength; i++) for (j = 0; j < dwWidth; j++) {
         PVERTEX pv = pVert + (j + i*dwWidth);
         pv->dwNormal = j + i*dwWidth;
         pv->dwPoint = j + i*dwWidth;
         pv->dwTexture = j + i*dwWidth;
         pv->dwColor = 0;
      } // i,j

      // fill in the polygons...
      for (i = 0; i < dwLength-1; i++) for (j = 0; j < dwWidth-1; j++, padwPoly += 4) {
         padwPoly[0] = j + i*dwWidth;
         padwPoly[1] = j + (i+1)*dwWidth;
         padwPoly[2] = (j+1) + (i+1)*dwWidth;
         padwPoly[3] = (j+1) + i*dwWidth;

         if (!m_fLowerJaw) {
            // flip the order
            DWORD k;
            for (k = 0; k < 2; k++) {
               DWORD dwTemp = padwPoly[k];
               padwPoly[k] = padwPoly[3 - k];
               padwPoly[3-k] = dwTemp;
            }
         }
      } // i,j

      // no longer dirty
      m_fDirty = FALSE;
   }

   if (m_fTongueDirty && m_fTongueShow) {
      DWORD dwDetail = m_dwDetail + m_dwUserDetail;
      dwDetail = min(dwDetail, 2);
      DWORD dwNumJawCP = (NUMJAWCP << dwDetail);
      DWORD i;

      // determine spline for the tongue
      CPoint apLoc[NUMJAWCP*4+1], apSize[NUMJAWCP*4+1];
      memset (apLoc, 0, sizeof(apLoc));
      memset (apSize, 0, sizeof(apSize));
      fp fTotalLen = 0;
      for (i = 0; i < dwNumJawCP; i++) {
         fp fLoc = (fp)i / (fp)(dwNumJawCP-1);
         fp fForward = m_tpTongueLoc.h * (1.0 - m_pTongueLimit.p[1]) + m_pTongueLimit.p[1];
         apLoc[i].p[0] = 0;
         apLoc[i].p[1] = -sqrt(fLoc) * m_pTongueSize.p[1] * fForward;
         apLoc[i].p[2] = -m_pTongueSize.p[2] / 2 + fLoc * m_tpTongueLoc.v * m_pTongueLimit.p[0];

         // figure out the length...
         if (i) {
            CPoint pDist;
            pDist.Subtract (&apLoc[i], &apLoc[i-1]);
            fTotalLen += pDist.Length();
         }
      }

      // adjust thickness based on length
      fTotalLen = m_pTongueSize.p[1] / fTotalLen;

      // do size
      for (i = 0; i < dwNumJawCP; i++) {
         fp fSize = 1 - (fp)i / ((fp)dwNumJawCP - 0.5);
         // fSize = max(fSize, .01);
         fSize = pow (fSize, m_pTongueSize.p[3]);
         apSize[i].p[1] = fSize * m_pTongueSize.p[0];
         apSize[i].p[0] = sqrt(fSize) * m_pTongueSize.p[2] * fTotalLen;
      } // i
      apLoc[dwNumJawCP].Copy (&apLoc[dwNumJawCP]-1);
      apLoc[dwNumJawCP-1].Average (&apLoc[dwNumJawCP-2], 0.1);
      apSize[dwNumJawCP-1].p[1] /= 2;
      apSize[dwNumJawCP].Copy (&apSize[dwNumJawCP-1]);
      apSize[dwNumJawCP].Scale (0.01);

      CPoint pFront;
      pFront.Zero();
      pFront.p[0] = 1;
      m_pTongue->BackfaceCullSet (TRUE);
      m_pTongue->DrawEndsSet (FALSE);
      m_pTongue->FrontVector (&pFront);
      m_pTongue->PathSpline (FALSE, dwNumJawCP+1, apLoc, (DWORD*)SEGCURVE_CUBIC, 0);
      m_pTongue->ScaleSpline (dwNumJawCP+1, apSize, (DWORD*)SEGCURVE_CUBIC, 0);
      m_pTongue->ShapeDefault (dwDetail ? NS_CIRCLE : NS_CIRCLEFAST);

      m_fTongueDirty = FALSE;
   }
   return TRUE;
}


/*************************************************************************************
CObjectJaw::FindMirror - Finds the mirror of the given tooth.

inputs
   DWORD          dwTooth - Tooth index, into m_lJAWTOOTH.
returns
   DWORD - Mirror, or -1 if no mirror
*/
DWORD CObjectJaw::FindMirror (DWORD dwTooth)
{
   PJAWTOOTH pjt = (PJAWTOOTH)m_lJAWTOOTH.Get(0);
   PJAWTOOTH pTooth = pjt + dwTooth;
   CPoint pLocOrig, pRotOrig;
   pTooth->mTransRot.ToXYZLLT (&pLocOrig, &pRotOrig.p[2], &pRotOrig.p[0], &pRotOrig.p[1]);
   pLocOrig.p[0] *= -1;
   pRotOrig.p[1] *= -1;
   pRotOrig.p[2] *= -1;
   fp fLocWant = 1.0 - pTooth->fLoc;

   // compare
   DWORD i;
   DWORD dwBest = -1;
   fp fBestScore = 0;
   fp fScore;
   CPoint pLoc, pRot;
   for (i = 0; i < m_lJAWTOOTH.Num(); i++) {
      if (i == dwTooth)
         continue;   // not its own mirror

      if (fabs(pjt[i].fLoc - fLocWant) > CLOSE)
         continue;   // too far apart

      // compare scores
      fScore = pTooth->pTooth->IsMirror (pjt[i].pTooth);

      // also include matrix
      pjt[i].mTransRot.ToXYZLLT (&pLoc, &pRot.p[2], &pRot.p[0], &pRot.p[1]);
      pLoc.Subtract (&pLocOrig);
      pRot.Subtract (&pRotOrig);
      fScore -= pLoc.Length();
      fScore -= pRot.Length();

      if ((dwBest == -1) || (fScore > fBestScore)) {
         dwBest = i;
         fBestScore = fScore;
      }
   } // i

   return dwBest;
}





/*****************************************************************************************
CObjectJaw::AttribGetIntern - OVERRIDE THIS

Like AttribGet() except that only called if default attributes not handled.
*/
static PWSTR gpszAttribForward = L"LipSyncMusc:TongueForward";
static PWSTR gpszAttribUp = L"LipSyncMusc:TongueUp";
static PWSTR gpszAttribTeethMissing = L"TeethMissing";

BOOL CObjectJaw::AttribGetIntern (PWSTR pszName, fp *pfValue)
{
   if (m_fTongueShow && !_wcsicmp(pszName, gpszAttribForward)) {
      *pfValue = m_tpTongueLoc.h;
      return TRUE;
   }
   else if (m_fTongueShow && !_wcsicmp(pszName, gpszAttribUp)) {
      *pfValue = m_tpTongueLoc.v;
      return TRUE;
   }
   else if (!_wcsicmp(pszName, gpszAttribTeethMissing)) {
      *pfValue = m_dwTeethMissing;
      return TRUE;
   }
   else
      return FALSE;
}


/*****************************************************************************************
CObjectJaw::AttribGetAllIntern - OVERRIDE THIS

Like AttribGetAllIntern() EXCEPT plATTRIBVAL is already initialized and filled with
some parameters (default to the object template)
*/
void CObjectJaw::AttribGetAllIntern (PCListFixed plATTRIBVAL)
{
   ATTRIBVAL av;
   memset (&av, 0, sizeof(av));

   // teeth
   wcscpy (av.szName, gpszAttribTeethMissing);
   av.fValue = m_dwTeethMissing;
   plATTRIBVAL->Add (&av);

   if (m_fTongueShow) {
      wcscpy (av.szName, gpszAttribForward);
      av.fValue = m_tpTongueLoc.h;
      plATTRIBVAL->Add (&av);

      wcscpy (av.szName, gpszAttribUp);
      av.fValue = m_tpTongueLoc.v;
      plATTRIBVAL->Add (&av);
   }
}


/*****************************************************************************************
CObjectJaw::AttribSetGroupIntern - OVERRIDE THIS

Like AttribSetGroup() except passing on non-template attributes.
*/
void CObjectJaw::AttribSetGroupIntern (DWORD dwNum, PATTRIBVAL paAttrib)
{
   BOOL fChanged = FALSE;

#define CHANGED if (!fChanged && m_pWorld) m_pWorld->ObjectAboutToChange(this); fChanged = TRUE

   DWORD i;
   for (i = 0; i < dwNum; i++, paAttrib++) {
      if (m_fTongueShow && !_wcsicmp (paAttrib->szName, gpszAttribForward)) {
         if (paAttrib->fValue == m_tpTongueLoc.h)
            continue;   // no change

         CHANGED;
         m_tpTongueLoc.h = paAttrib->fValue;
      }
      else if (m_fTongueShow && !_wcsicmp (paAttrib->szName, gpszAttribUp)) {
         if (paAttrib->fValue == m_tpTongueLoc.v)
            continue;   // no change

         CHANGED;
         m_tpTongueLoc.v = paAttrib->fValue;
      }
      else if (!_wcsicmp (paAttrib->szName, gpszAttribTeethMissing)) {
         if ((DWORD)paAttrib->fValue == m_dwTeethMissing)
            continue;   // no change

         CHANGED;
         m_dwTeethMissing = (DWORD) paAttrib->fValue;
      }
   } // i

   if (fChanged) {
      m_fTongueDirty = TRUE;

      if (m_pWorld)
         m_pWorld->ObjectChanged (this);
   }
}


/*****************************************************************************************
CObjectJaw::AttribInfoIntern - OVERRIDE THIS

Like AttribInfo() except called if attribute is not for template.
*/
BOOL CObjectJaw::AttribInfoIntern (PWSTR pszName, PATTRIBINFO pInfo)
{
   DWORD dwNum;

   if (m_fTongueShow && !_wcsicmp(pszName, gpszAttribForward))
      dwNum = 0;
   else if (m_fTongueShow && !_wcsicmp(pszName, gpszAttribUp))
      dwNum = 1;
   else if (!_wcsicmp(pszName, gpszAttribTeethMissing))
      dwNum = 2;
   else
      return FALSE;

   memset (pInfo, 0, sizeof(*pInfo));
   pInfo->dwType = AIT_NUMBER;
   pInfo->fDefPassUp = (dwNum != 2);   // dont pass up for teeth missing
   pInfo->fDefLowRank = (dwNum == 2);  // default low rank
   pInfo->fMin = 0;
   pInfo->fMax = (dwNum == 2) ? (1 << m_lJAWTOOTH.Num()) : 1;
   switch (dwNum) {
   case 0:
      pInfo->pszDescription = L"Controls how far forward the tip of the tonue is.";
      break;
   case 1:
      pInfo->pszDescription = L"Controls the elevation of the tip of the tongue.";
      break;
   case 2:
      pInfo->pszDescription = L"Bit-field to specify which teeth are missing.";
      break;
   } // swtich

   return TRUE;
}


// BUGBUG - The tongue is too pointy - so some triangles don't seem to have right normals
