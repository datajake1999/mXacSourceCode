/************************************************************************
CObjectHairHead.cpp - Draws a HairHead.

begun 14/9/01 by Mike Rozak
Copyright 2001 Mike Rozak. All rights reserved
*/

#include <windows.h>
#include "escarpment.h"
#include "..\M3D.h"
#include "resource.h"


#define HAIRLOCKBASE       100





typedef struct {
   PWSTR       pszName;    // name of the move point
   int         iX, iY; // -1 if for left/top, 1 for right/bottom
} SPLINEMOVEP, *PSPLINEMOVEP;

static SPLINEMOVEP   gaHairHeadMove[] = {
   L"Center", 0, 0
};

/**********************************************************************************
CObjectHairHead::Constructor and destructor */
CObjectHairHead::CObjectHairHead (PVOID pParams, POSINFO pInfo)
{
   m_dwRenderShow = RENDERSHOW_CHARACTER;
   m_OSINFO = *pInfo;

   m_pHat.Zero();
   m_pHat.p[0] = 1000;  // so dont use hat

   m_dwDensity = 10;
   m_dwCurEdit = 0;
   m_fShowEllipse = TRUE;
   m_fBackfaceCull = TRUE;
   m_fSymmetry = TRUE;
   m_pRadius.Zero();
   m_pRadius.p[0] = .25 / 2.0;
   m_pRadius.p[1] = .3 / 2.0;
   m_pRadius.p[2] = .25 / 2.0;
   m_lHAIRHEADROOT.Init (sizeof(HAIRHEADROOT));
   m_fBrushSize = 0.3;
   m_fEndHairWeight = 0.5;
   m_fDiameter = 0.7;
   m_fLengthPerPoint = m_pRadius.p[0] / 6;   // BUGFIX - Was /3 but too long
   m_dwHairLayers = 1;
   m_fVariation = 0.1;
   DWORD i;
   for (i = 0; i < MAXHAIRLAYERS; i++) {
      m_adwHairLayerRepeat[i] = 3;
      m_afHairLayerScale[i] = 1.0 - (fp)i / (fp)MAXHAIRLAYERS;
   }
   m_atpProfile[0].h = m_atpProfile[0].v = 1;
   for (i = 1; i < 5; i++)
      m_atpProfile[i] = m_atpProfile[0];
   m_atpProfile[4].h = m_atpProfile[4].v = 0;   // to point

   NewDensity ();

   // color for the HairHead
   ObjectSurfaceAdd (HAIRLOCKBASE, RGB(0x80, 0x80,0), MATERIAL_PAINTGLOSS, NULL,
      &GTEXTURECODE_Hair, &GTEXTURESUB_HairDefault);
   for (i = 1; i < MAXHAIRLAYERS; i++)
      ObjectSurfaceAdd (HAIRLOCKBASE+i, RGB(0xff,0,0xff), MATERIAL_PAINTGLOSS);
}


CObjectHairHead::~CObjectHairHead (void)
{
   DeleteAllLocks();
}


/**********************************************************************************
CObjectHairHead::Delete - Called to delete this object
*/
void CObjectHairHead::Delete (void)
{
   delete this;
}

/**********************************************************************************
CObjectHairHead::Render - Draws a test box.

inputs
   POBJECTRENDER     pr - Render information
*/
void CObjectHairHead::Render (POBJECTRENDER pr, DWORD dwSubObject)
{
   // create the surface render object and draw
   // CRenderSurface rs;
   m_Renderrs.ClearAll();

   CMatrix mObject;
   m_Renderrs.Init (pr->pRS);
   ObjectMatrixGet (&mObject);
   m_Renderrs.Multiply (&mObject);

   // hat hair
   CMatrix mPointToClipSpace, mClipSpaceToPoint;
   BOOL fUseHatHair = (m_pHat.p[0] < 999.0);
   if (fUseHatHair) {
      CPoint p;
      p.Zero();
      p.p[2] = m_pHat.p[0];
      mClipSpaceToPoint.FromXYZLLT (&p, 0, m_pHat.p[1], m_pHat.p[2]);
      mClipSpaceToPoint.Invert4 (&mPointToClipSpace);
   }

   DWORD i;
   BOOL fHair = FALSE;
   PHAIRHEADROOT phh = (PHAIRHEADROOT)m_lHAIRHEADROOT.Get(0);
   DWORD dwNum = m_lHAIRHEADROOT.Num();
   for (i = 0; i < dwNum; i++, phh++) {
      if (!phh->pLock)
         continue;

      fHair = TRUE;
      phh->pLock->Render (pr, &m_Renderrs, this, HAIRLOCKBASE, m_fBackfaceCull,
         fUseHatHair ? &mPointToClipSpace : NULL, fUseHatHair ? &mClipSpaceToPoint : NULL);
   }

   // draw sphere
   if (m_fShowEllipse || !fHair) {
      RENDERSURFACE mat;
      memset (&mat, 0, sizeof(mat));
      mat.Material.InitFromID (MATERIAL_PAINTSEMIGLOSS);
      mat.TextureMods.cTint = RGB(0xff,0xff,0xff);
      mat.TextureMods.wBrightness = 0x1000;
      mat.TextureMods.wContrast = 0x1000;
      mat.TextureMods.wHue = 0x0000;
      mat.TextureMods.wSaturation = 0x1000;

      m_Renderrs.SetDefColor (RGB(0x80,0x80,0x80));
      m_Renderrs.SetDefMaterial (&mat);

      m_Renderrs.ShapeEllipsoid (m_pRadius.p[0], m_pRadius.p[1], m_pRadius.p[2]);
   }

   m_Renderrs.Commit();

}


/**********************************************************************************
CObjectHairHead::QueryBoundingBox - Standard API
*/
void CObjectHairHead::QueryBoundingBox (PCPoint pCorner1, PCPoint pCorner2, DWORD dwSubObject)
{
   CPoint p1, p2;
   BOOL fSet = FALSE;
   pCorner1->Zero();
   pCorner2->Zero();

   // hat hair
   CMatrix mPointToClipSpace, mClipSpaceToPoint;
   BOOL fUseHatHair = (m_pHat.p[0] < 999.0);
   if (fUseHatHair) {
      CPoint p;
      p.Zero();
      p.p[2] = m_pHat.p[0];
      mClipSpaceToPoint.FromXYZLLT (&p, 0, m_pHat.p[1], m_pHat.p[2]);
      mClipSpaceToPoint.Invert4 (&mPointToClipSpace);
   }

   DWORD i;
   BOOL fHair = FALSE;
   PHAIRHEADROOT phh = (PHAIRHEADROOT)m_lHAIRHEADROOT.Get(0);
   DWORD dwNum = m_lHAIRHEADROOT.Num();
   for (i = 0; i < dwNum; i++, phh++) {
      if (!phh->pLock)
         continue;

      fHair = TRUE;
      phh->pLock->QueryBoundingBox (&p1, &p2,
         fUseHatHair ? &mPointToClipSpace : NULL, fUseHatHair ? &mClipSpaceToPoint : NULL);

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

   // draw sphere
   if (m_fShowEllipse || !fHair) {
      p1.Copy (&m_pRadius);
      p1.Scale (-1);
      p2.Copy (&m_pRadius);

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
      OutputDebugString ("\r\nCObjectHairHead::QueryBoundingBox too small.");
#endif
}


/**********************************************************************************
CObjectHairHead::Clone - Clones this

inputs
   none
returns
   PCObjectSocket - Clone
*/
PCObjectSocket CObjectHairHead::Clone (void)
{
   PCObjectHairHead pNew;

   pNew = new CObjectHairHead(NULL, &m_OSINFO);

   // clone template info
   CloneTemplate(pNew);

   // clone variables
   pNew->m_pHat.Copy (&m_pHat);
   pNew->m_dwDensity = m_dwDensity;
   pNew->m_dwCurEdit = m_dwCurEdit;
   pNew->m_fShowEllipse = m_fShowEllipse;
   pNew->m_fBackfaceCull = m_fBackfaceCull;
   pNew->m_fSymmetry = m_fSymmetry;
   pNew->m_pRadius.Copy (&m_pRadius);
   pNew->DeleteAllLocks();
   pNew->m_fBrushSize = m_fBrushSize;
   pNew->m_fEndHairWeight = m_fEndHairWeight;
   pNew->m_fDiameter = m_fDiameter;
   pNew->m_fLengthPerPoint = m_fLengthPerPoint;
   pNew->m_dwHairLayers = m_dwHairLayers;
   memcpy (pNew->m_adwHairLayerRepeat, m_adwHairLayerRepeat, sizeof(m_adwHairLayerRepeat));
   memcpy (pNew->m_afHairLayerScale, m_afHairLayerScale, sizeof(m_afHairLayerScale));
   memcpy (pNew->m_atpProfile, m_atpProfile, sizeof(m_atpProfile));
   pNew->m_fVariation = m_fVariation;
   pNew->m_lHAIRHEADROOT.Init (sizeof(HAIRHEADROOT), m_lHAIRHEADROOT.Get(0), m_lHAIRHEADROOT.Num());
   PHAIRHEADROOT phh = (PHAIRHEADROOT) pNew->m_lHAIRHEADROOT.Get(0);
   DWORD i;
   for (i = 0; i < pNew->m_lHAIRHEADROOT.Num(); i++, phh++)
      if (phh->pLock)
         phh->pLock = phh->pLock->Clone();


   return pNew;
}


static PWSTR gpszHairLock = L"HairLock";
static PWSTR gpszDiameter = L"Diameter";
static PWSTR gpszLengthPerPoint = L"LengthPerPoint";
static PWSTR gpszVariation = L"Variation";
static PWSTR gpszHairLayers = L"HairLayers";
static PWSTR gpszDensity = L"Density";
static PWSTR gpszCurEdit = L"CurEdit";
static PWSTR gpszShowEllipse = L"ShowEllipse";
static PWSTR gpszSymmetry = L"Symmetry";
static PWSTR gpszRadius = L"Radius";
static PWSTR gpszBrushSize = L"BrushSize";
static PWSTR gpszLoc = L"Loc";
static PWSTR gpszBackfaceCull = L"BackfaceCull";
static PWSTR gpszMaxPoint = L"MaxPoint";
static PWSTR gpszMaxValue = L"MaxValue";
static PWSTR gpszBinary = L"Binary";
static PWSTR gpszHat = L"Hat";
static PWSTR gpszEndHairWeight = L"EndHairWeight";

PCMMLNode2 CObjectHairHead::MMLTo (void)
{
   // call into the template first
   PCMMLNode2 pNode = MMLToTemplate();
   if (!pNode)
      return NULL;

   MMLValueSet (pNode, gpszHat, &m_pHat);
   MMLValueSet (pNode, gpszDensity, (int)m_dwDensity);
   MMLValueSet (pNode, gpszCurEdit, (int)m_dwCurEdit);
   MMLValueSet (pNode, gpszShowEllipse, (int)m_fShowEllipse);
   MMLValueSet (pNode, gpszBackfaceCull, (int)m_fBackfaceCull);
   MMLValueSet (pNode, gpszSymmetry, (int)m_fSymmetry);
   MMLValueSet (pNode, gpszRadius, &m_pRadius);
   MMLValueSet (pNode, gpszBrushSize, m_fBrushSize);
   MMLValueSet (pNode, gpszEndHairWeight, m_fEndHairWeight);

   DWORD i;
   WCHAR szTemp[32];
   for (i = 0; i < 5; i++) {
      swprintf (szTemp, L"Profile%d", (int)i);
      MMLValueSet (pNode, szTemp, &m_atpProfile[i]);
   } // i

   MMLValueSet (pNode, gpszDiameter, m_fDiameter);
   MMLValueSet (pNode, gpszLengthPerPoint, m_fLengthPerPoint);
   MMLValueSet (pNode, gpszVariation, m_fVariation);
   MMLValueSet (pNode, gpszHairLayers, (int)m_dwHairLayers);
   for (i = 0; i < m_dwHairLayers; i++) {
      swprintf (szTemp, L"HairLayerRepeat%d", (int)i);
      MMLValueSet (pNode, szTemp, (int)m_adwHairLayerRepeat[i]);

      swprintf (szTemp, L"HairLayerScale%d", (int)i);
      MMLValueSet (pNode, szTemp, m_afHairLayerScale[i]);
   } // i

   // write out all the roots
   PHAIRHEADROOT phh = (PHAIRHEADROOT)m_lHAIRHEADROOT.Get(0);
#define USEBINARY
#ifdef USEBINARY
   CMem mem;
   CPoint pMax, pMin;
   fp fMax = 0, fMin = 0;
   pMax.Zero();
   pMin.Zero();
   for (i = 0; i < m_lHAIRHEADROOT.Num(); i++)
      if (phh[i].pLock)
         phh[i].pLock->MMLToBinaryMinMax (&pMin, &pMax, &fMin, &fMax);
   for (i = 0; i < 3; i++) {
      pMax.p[i] = max(fabs(pMax.p[i]), fabs(pMin.p[i]));
      pMax.p[i] = max(pMax.p[i], CLOSE);
   }
   fMax = max(fabs(fMax), fabs(fMin));
   fMax = max(fMax, CLOSE);

   // figure out the scale
   CPoint pScale;
   fp fScale;
   for (i = 0; i < 3; i++)
      pScale.p[i] = 32767.0 / pMax.p[i];
   fScale = 32767.0 / fMax;

   // write out the max values
   MMLValueSet (pNode, gpszMaxPoint, &pMax);
   MMLValueSet (pNode, gpszMaxValue, fMax);

   // fill in the list
   for (i = 0; i < m_lHAIRHEADROOT.Num(); i++, phh++) {
      if (!phh->pLock)
         continue;

      phh->pLock->MMLToBinary (&mem, &pScale, fScale, i);
   } // i
   if (mem.m_dwCurPosn)
      MMLValueSet (pNode, gpszBinary, (PBYTE) mem.p, mem.m_dwCurPosn);
#else
   for (i = 0; i < m_lHAIRHEADROOT.Num(); i++, phh++) {
      if (!phh->pLock)
         continue;

      PCMMLNode2 pSub = phh->pLock->MMLTo();
      if (!pSub)
         continue;
      pSub->NameSet (gpszHairLock);
      MMLValueSet (pSub, gpszLoc, (int)i);
      pNode->ContentAdd (pSub);
   } // i
#endif // USEBINARY


   return pNode;
}

BOOL CObjectHairHead::MMLFrom (PCMMLNode2 pNode)
{
   // call into the template first
   if (!MMLFromTemplate (pNode))
      return FALSE;

   m_pHat.Zero();
   m_pHat.p[0] = 1000;  // so dont use
   MMLValueGetPoint (pNode, gpszHat, &m_pHat);
   m_dwDensity = (DWORD) MMLValueGetInt (pNode, gpszDensity, (int)1);
   m_dwCurEdit = (DWORD) MMLValueGetInt (pNode, gpszCurEdit, (int)0);
   m_fShowEllipse = (BOOL) MMLValueGetInt (pNode, gpszShowEllipse, (int)TRUE);
   m_fBackfaceCull = (BOOL) MMLValueGetInt (pNode, gpszBackfaceCull, (int)TRUE);
   m_fSymmetry = (BOOL) MMLValueGetInt (pNode, gpszSymmetry, (int)TRUE);
   MMLValueGetPoint (pNode, gpszRadius, &m_pRadius);
   m_fBrushSize = MMLValueGetDouble (pNode, gpszBrushSize, 1);
   m_fEndHairWeight = MMLValueGetDouble (pNode, gpszEndHairWeight, 0.5);

   DWORD i;
   WCHAR szTemp[32];
   for (i = 0; i < 5; i++) {
      swprintf (szTemp, L"Profile%d", (int)i);
      MMLValueGetTEXTUREPOINT (pNode, szTemp, &m_atpProfile[i]);
   } // i

   m_fDiameter = MMLValueGetDouble (pNode, gpszDiameter, 1);
   m_fLengthPerPoint = MMLValueGetDouble (pNode, gpszLengthPerPoint, 1);
   m_fVariation = MMLValueGetDouble (pNode, gpszVariation, 0);
   m_dwHairLayers = (DWORD) MMLValueGetInt (pNode, gpszHairLayers, (int)1);
   for (i = 0; i < m_dwHairLayers; i++) {
      swprintf (szTemp, L"HairLayerRepeat%d", (int)i);
      m_adwHairLayerRepeat[i] = (DWORD) MMLValueGetInt (pNode, szTemp, (int)1);

      swprintf (szTemp, L"HairLayerScale%d", (int)i);
      m_afHairLayerScale[i] = MMLValueGetDouble (pNode, szTemp, 1);
   } // i

   // clear exsiting roots
   NewDensity();  // clear existing and set to new density

   PHAIRHEADROOT phh = (PHAIRHEADROOT)m_lHAIRHEADROOT.Get(0);
   DWORD dwNum = m_lHAIRHEADROOT.Num();

   // read in binary hair
   CPoint pMax;
   CMem mem;
   fp fMax = 0;
   pMax.Zero();
   MMLValueGetPoint (pNode, gpszMaxPoint, &pMax);
   fMax = MMLValueGetDouble (pNode, gpszMaxValue, 0);
   pMax.Scale (1.0 / 32767.0); // so can scale
   fMax /= 32767.0;  // so can scale
   MMLValueGetBinary (pNode, gpszBinary, &mem);
   PBYTE pb = (PBYTE)mem.p;
   DWORD dwLeft = (DWORD) mem.m_dwCurPosn;
   while (dwLeft) {
      PCHairLock pNew = new CHairLock;
      if (!pNew)
         break;

      DWORD dwLoc;
      DWORD dwUsed = pNew->MMLFromBinary (pb, dwLeft, &pMax, fMax, &dwLoc);
      if (!dwUsed)
         break;   // error
      if (dwLoc >= dwNum) {
         delete pNew;
         continue;
      }
      if (phh[dwLoc].pLock) {
         delete pNew;
         continue;   // already there
      }

      // store away
      phh[dwLoc].pLock = pNew;

      // increment
      pb += dwUsed;
      dwLeft -= dwUsed;
   }

   // read in
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
      if (_wcsicmp(psz, gpszHairLock))
         continue;   // only care about hair locks

      // read in the location
      DWORD dwLoc;
      dwLoc = (DWORD)MMLValueGetInt (pSub, gpszLoc, -1);
      if (dwLoc >= dwNum)
         continue;
      if (phh[dwLoc].pLock)
         continue;   // already there

      // else, create new
      phh[dwLoc].pLock = new CHairLock;
      if (!phh[dwLoc].pLock)
         return FALSE;
      phh[dwLoc].pLock->MMLFrom (pSub);
   } // i

   return TRUE;
}


/**********************************************************************************
CObjectHairHead::DeleteAllLocks - Deletes all the locks (PCHairLock) rememberd
by the object and clears m_lHAIRHEADROOT to 0 length
*/
void CObjectHairHead::DeleteAllLocks (void)
{
   PHAIRHEADROOT phh = (PHAIRHEADROOT)m_lHAIRHEADROOT.Get(0);
   DWORD dwNum = m_lHAIRHEADROOT.Num();
   DWORD i;
   for (i = 0; i < dwNum; i++, phh++)
      if (phh->pLock)
         delete phh->pLock;
   m_lHAIRHEADROOT.Clear();
}

/**********************************************************************************
CObjectHairHead::ApplyRadius - Loops through all m_lHAIRHEADROOT and calculates
the pLocSphere and pNorm based on m_pRadius. If there are any hair-locks around then this
will adjust the lock's 0'th element.
*/
void CObjectHairHead::ApplyRadius (void)
{
   // make a matrix to make ellipical normal
   CMatrix mScale, mEllipse;
   mScale.Scale (m_pRadius.p[0], m_pRadius.p[1], m_pRadius.p[2]);
   mScale.Invert (&mEllipse);
   mEllipse.Transpose();

   PHAIRHEADROOT phh = (PHAIRHEADROOT)m_lHAIRHEADROOT.Get(0);
   DWORD dwNum = m_lHAIRHEADROOT.Num();

   // loop
   DWORD dwHemi, dwLat, dwLong;
   CListFixed lPoint, lTwist;
   for (dwHemi = 0; dwHemi < 2; dwHemi++)
      for (dwLat = 0; dwLat < m_dwDensity; dwLat++)
         for (dwLong = 0; dwLong < (m_dwDensity - dwLat)*4; dwLong++) {

            // figure out index
            DWORD dwX = dwLong;
            DWORD dwY = dwLat;
            if (dwHemi) {  // if in southern hemisphere flip
               dwX = (1 + m_dwDensity) * 4 - 1 - dwX;
               dwY = m_dwDensity - 1 - dwY;
            }
            DWORD dwIndex = dwX + dwY * (1 + m_dwDensity)*4;

            fp fLat = (fp)(dwLat + 0.5) / (fp)m_dwDensity * PI / 2 * (dwHemi ? -1 : 1);
            fp fLong = (fp)(dwLong + 0.5) / (fp)((m_dwDensity-dwLat)*4) * 2.0 * PI;

            phh[dwIndex].pLocSphere.p[0] = sin(fLong) * cos(fLat);
            phh[dwIndex].pLocSphere.p[1] = cos(fLong) * cos(fLat);
            phh[dwIndex].pLocSphere.p[2] = sin(fLat);
            phh[dwIndex].pLocSphere.p[3] = 1;

            phh[dwIndex].pNorm.Copy (&phh[dwIndex].pLocSphere);
            phh[dwIndex].pNorm.MultiplyLeft (&mEllipse);
            phh[dwIndex].pNorm.Normalize();

            // if there's a hair then adjust its root
            if (!phh[dwIndex].pLock)
               continue;
            PCPoint paPoint;
            fp *pafTwist;
            DWORD dwSplineNum;
            paPoint = phh[dwIndex].pLock->SplineGet (&dwSplineNum, &pafTwist);
            if (dwSplineNum < 2)
               continue;
            lPoint.Init (sizeof (CPoint), paPoint, dwSplineNum);
            lTwist.Init (sizeof(fp), pafTwist, dwSplineNum);
            paPoint = (PCPoint) lPoint.Get(0);

            // figure out where want the root to be
            CPoint pWant;
            pWant.Copy (&phh[dwIndex].pLocSphere);
            pWant.p[0] *= m_pRadius.p[0];
            pWant.p[1] *= m_pRadius.p[1];
            pWant.p[2] *= m_pRadius.p[2];


            // delta to existing points
            CPoint pDelta;
            pDelta.Subtract (&pWant, &paPoint[1]);

            // apply deltas
            DWORD j;
            for (j = 1; j < dwSplineNum; j++)
               paPoint[j].Add (&pDelta);

            // and the norm
            paPoint->Subtract (&paPoint[1], &phh[dwIndex].pNorm);

            // set it
            phh[dwIndex].pLock->SplineSet (dwSplineNum, paPoint, (fp*)lTwist.Get(0));
         } // dwHemi, dwLat, dwLong
}


/**********************************************************************************
CObjectHairHead::NewDensity - Deleletes all the locks and then intiializes the
array using the new density
*/
void CObjectHairHead::NewDensity (void)
{
   DeleteAllLocks ();

   // how many locks?
   DWORD dwNum = m_dwDensity * (1+m_dwDensity)*4;
   DWORD i;
   HAIRHEADROOT hhr;
   memset (&hhr, 0, sizeof(hhr));
   m_lHAIRHEADROOT.Clear();
   m_lHAIRHEADROOT.Required (dwNum);
   for (i = 0; i < dwNum; i++)
      m_lHAIRHEADROOT.Add (&hhr);

   ApplyRadius();
}

/**********************************************************************************
CObjectHairHead::SyncRoot - synchronize one root to the given diameter, 
   length per point, etc.

inputs
   DWORD          dwRoot - Root number to sychronize
returns
   none
*/
void CObjectHairHead::SyncRoot (DWORD dwRoot)
{
   PHAIRHEADROOT phh = (PHAIRHEADROOT)m_lHAIRHEADROOT.Get(dwRoot);

   if (!phh->pLock)
      return;

   phh->pLock->DiameterSet (PI/2.0 / (fp)m_dwDensity *
      (m_pRadius.p[0] + m_pRadius.p[1] + m_pRadius.p[2])/3.0 *
      m_fDiameter * 2.0);
   phh->pLock->ProfileSet (m_atpProfile);
   phh->pLock->HairLayersSet (m_dwHairLayers, m_adwHairLayerRepeat, m_afHairLayerScale);
   phh->pLock->VariationSet (m_fVariation, dwRoot);
   phh->pLock->LengthSet (m_fLengthPerPoint);
}

/**********************************************************************************
CObjectHairHead::SyncAllRoots - synchronize all roots to the given diameter, 
   length per point, etc.
*/
void CObjectHairHead::SyncAllRoots (void)
{
   DWORD i;
   for (i = 0; i < m_lHAIRHEADROOT.Num(); i++)
      SyncRoot (i);
}


/**********************************************************************************
CObjectHairHead::SproutNewHair - sprouts a new hair at the given root, if it
isn't already there. If hair is there, wont do anything

inputs
   DWORD          dwRoot - Root number to sychronize
   fp             fHeight - from 0 to 1
*/
void CObjectHairHead::SproutNewHair (DWORD dwRoot, fp fHeight)
{
   PHAIRHEADROOT phh = (PHAIRHEADROOT)m_lHAIRHEADROOT.Get(dwRoot);
   if (phh->pLock)
      return;

   phh->pLock = new CHairLock;
   if (!phh->pLock)
      return;
   SyncRoot (dwRoot);

   // apply short hair
   CPoint ap[3];
   fp af[3];
   memset (af, 0, sizeof(af));
   ap[1].Copy (&phh->pLocSphere);
   ap[1].p[0] *= m_pRadius.p[0];
   ap[1].p[1] *= m_pRadius.p[1];
   ap[1].p[2] *= m_pRadius.p[2];
   ap[2].Add (&ap[1], &phh->pNorm);
   ap[0].Subtract (&ap[1], &phh->pNorm);
   phh->pLock->SplineSet (3, ap, af);
   phh->pLock->RemainderSet (fHeight);

   // done
}




/**************************************************************************************
CObjectHairHead::MoveReferencePointQuery - 
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
BOOL CObjectHairHead::MoveReferencePointQuery (DWORD dwIndex, PCPoint pp)
{
   PSPLINEMOVEP ps;
   DWORD dwDataSize;
   ps = gaHairHeadMove;
   dwDataSize = sizeof(gaHairHeadMove);

   if (dwIndex >= dwDataSize / sizeof(SPLINEMOVEP))
      return FALSE;

   // always at 0,0 in Hairs
   pp->Zero();

   return TRUE;
}

/**************************************************************************************
CObjectHairHead::MoveReferenceStringQuery -
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
BOOL CObjectHairHead::MoveReferenceStringQuery (DWORD dwIndex, PWSTR psz, DWORD dwSize, DWORD *pdwNeeded)
{
   PSPLINEMOVEP ps;
   DWORD dwDataSize;
   ps = gaHairHeadMove;
   dwDataSize = sizeof(gaHairHeadMove);
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


/**************************************************************************************
CObjectHairHead::RootWeight - Used for moving control points around. Calculates the
weight of the affect for adjacent hairs.

inputs
   PCPoint        pOrig - The sphere location of the hair being moved
   DWORD          dwRoot - Root to test
returns
   fp - Weight from 0..1. 0 indicates no effect, 1 is max
*/
fp CObjectHairHead::RootWeight (PCPoint pOrig, DWORD dwRoot)
{
   if (!m_fBrushSize)
      return 0;

   // get the point
   PHAIRHEADROOT phh = (PHAIRHEADROOT)m_lHAIRHEADROOT.Get(dwRoot);

   // if mirror and is on the mirrored side then 0 since mirror handled differently
   if (m_fSymmetry && (phh->pLocSphere.p[0] * pOrig->p[0] <= 0))
      return 0;

   // else distance
   CPoint pDist;
   fp fDist;
   pDist.Subtract (&phh->pLocSphere, pOrig);
   fDist = pDist.Length();
   if (fDist >= m_fBrushSize)
      return 0;
   fDist = pow(1.0 - fDist / m_fBrushSize, 0.25);

   return fDist;
}


/**************************************************************************************
CObjectHairHead::RootMirror - Given a root number, this returns the mirror (LR) of
the root.

inputs
   DWORD          dwRoot - root number
returns
   DWORD - mirror
*/
DWORD CObjectHairHead::RootMirror (DWORD dwRoot)
{
   DWORD dwMod = (1 + m_dwDensity) * 4;
   DWORD dwX = dwRoot % dwMod;
   DWORD dwY = dwRoot / dwMod;
   DWORD dwWidth = (m_dwDensity - dwY) * 4;
   DWORD dwHemi = (dwX < dwWidth) ? 0 : 1;
   DWORD dwRealWidth = dwHemi ? (dwMod - dwWidth) : dwWidth;
   DWORD dwOffset = dwHemi ? (dwMod - dwRealWidth) : 0;

   // mirror
   dwX = dwX - dwOffset;
   dwX = dwRealWidth - dwX - 1;

   // put back together
   dwX += dwOffset;
   return dwX + dwY * dwMod;
}


/*************************************************************************************
CObjectHairHead::ControlPointQuery - Called to query the information about a control
point identified by dwID.

inputs
   DWORD       dwID - ID
   POSCONTROL  pInfo - Filled in with the infomration
returns
   BOOL - TRUE if successful
*/
BOOL CObjectHairHead::ControlPointQuery (DWORD dwID, POSCONTROL pInfo)
{
   // if the currently edited value doesn't have a hair, then dont bothter
   PHAIRHEADROOT phh = (PHAIRHEADROOT)m_lHAIRHEADROOT.Get(0);
   DWORD dwNum = m_lHAIRHEADROOT.Num();
   if ((m_dwCurEdit < dwNum) && !phh[m_dwCurEdit].pLock)
      m_dwCurEdit = -1;

   // if CP higher than number of points and have a current edit then pass down
   if ((dwID >= dwNum) && (m_dwCurEdit < dwNum))
      return phh[m_dwCurEdit].pLock->ControlPointQuery (dwNum, FALSE, TRUE, dwID, pInfo);

   if (dwID >= dwNum)
      return FALSE;

   // else, either have a button to add a CP or select a hair
   memset (pInfo, 0, sizeof(*pInfo));
   pInfo->cColor = phh[dwID].pLock ? RGB(0xff,0xff,0xff) : RGB(0x80,0x80,0x80);
   pInfo->dwID = dwID;
   pInfo->dwStyle = phh[dwID].pLock ? CPSTYLE_CUBE : CPSTYLE_POINTER;
   pInfo->fButton = TRUE;
   pInfo->fSize = PI / 2.0 / (fp)m_dwDensity * (m_pRadius.p[0] + m_pRadius.p[1] + m_pRadius.p[2]) / 3.0;
   wcscpy (pInfo->szName, phh[dwID].pLock ? L"Click to select hair" : L"Click to add hair");
   if (phh[dwID].pLock) {
      phh[dwID].pLock->Tip(&pInfo->pLocation);  // so end of hair
   }
   else {
      pInfo->pLocation.Copy (&phh[dwID].pLocSphere);
      pInfo->pLocation.p[0] *= m_pRadius.p[0];
      pInfo->pLocation.p[1] *= m_pRadius.p[1];
      pInfo->pLocation.p[2] *= m_pRadius.p[2];
   }

   return TRUE;
}

/*************************************************************************************
CObjectHairHead::ControlPointSet - Called to change the valud of a control point.

inputs
   DWORD       dwID - ID
   PCPoint     pVal - Contains the new location, in object coordinates
returns
   BOOL - TRUE if successful
*/
BOOL CObjectHairHead::ControlPointSet (DWORD dwID, PCPoint pVal, PCPoint pViewer)
{
   // if the currently edited value doesn't have a hair, then dont bothter
   PHAIRHEADROOT phh = (PHAIRHEADROOT)m_lHAIRHEADROOT.Get(0);
   DWORD dwNum = m_lHAIRHEADROOT.Num();
   if ((m_dwCurEdit < dwNum) && !phh[m_dwCurEdit].pLock)
      m_dwCurEdit = -1;

   // if CP higher than number of points and have a current edit then pass down
   if ((dwID >= dwNum) && (m_dwCurEdit < dwNum)) {
      BOOL fRet;
      DWORD dwChanged, dwNode;
      CPoint pChangedVect, pChangedOrig;
      CPoint pZero;
      pZero.Zero();

      // remember total length before operation
      fp fTotalPre = phh[m_dwCurEdit].pLock->TotalLengthGet();

      // falloff distance
      m_fBrushSize = max(m_fBrushSize, 0.005);
      fp fFalloff = m_fBrushSize * (m_pRadius.p[0] + m_pRadius.p[1] + m_pRadius.p[2]) / 3.0;

      // pass it on
      fRet = phh[m_dwCurEdit].pLock->ControlPointSet (this, dwNum, FALSE, TRUE, dwID, pVal, pViewer,
         &pZero, &m_pRadius, &dwChanged, &dwNode, &pChangedVect, &pChangedOrig, fFalloff, m_fEndHairWeight);
      if (!fRet)
         return fRet;

      // remember total length afterwards
      fp fTotalPost = phh[m_dwCurEdit].pLock->TotalLengthGet();

      m_pWorld->ObjectAboutToChange(this);

      // if deleted then delete this
      if (dwChanged == 3) {
         delete phh[m_dwCurEdit].pLock;
         phh[m_dwCurEdit].pLock = NULL;
      }

      // affect the mirror...
      DWORD dwMirror = -1;
      if (m_fSymmetry) {
         dwMirror = RootMirror (m_dwCurEdit);
         if (dwChanged == 3) {
            // deleted
            delete phh[dwMirror].pLock;
            phh[dwMirror].pLock = NULL;
         }
         if (phh[dwMirror].pLock) {
            phh[m_dwCurEdit].pLock->CloneTo (phh[dwMirror].pLock);
            phh[dwMirror].pLock->Mirror(0);
         }
      }

      // if deleted then delete all nearby
      DWORD i;
      if (dwChanged == 3) {
         for (i = 0; i < dwNum; i++) {
            if (!RootWeight (&phh[m_dwCurEdit].pLocSphere, i))
               continue;   // too far away
            if (phh[i].pLock) {
               delete phh[i].pLock;
               phh[i].pLock = NULL;
            }
            dwMirror = RootMirror (i);
            if (m_fSymmetry && phh[dwMirror].pLock) {
               delete phh[dwMirror].pLock;
               phh[dwMirror].pLock = NULL;
            }
         } // i
      }
      else if (dwChanged == 1) {
         // used IK
         for (i = 0; i < dwNum; i++) {
            // dont redo self
            if ((i == m_dwCurEdit) || !phh[i].pLock)
               continue;

            // if have mirroring on, and this is on the opposite side then skip
            if (m_fSymmetry && (phh[i].pLocSphere.p[0] * phh[m_dwCurEdit].pLocSphere.p[0] <= 0))
               continue;

            // else, see if any IK happens
            if (!phh[i].pLock->IKPull (&pChangedOrig, &pChangedVect, fFalloff, &pZero, &m_pRadius, m_fEndHairWeight))
               continue;   // no IK happened

            // apply symmetry
            dwMirror = RootMirror (i);
            if (m_fSymmetry && phh[dwMirror].pLock) {
               phh[i].pLock->CloneTo (phh[dwMirror].pLock);
               phh[dwMirror].pLock->Mirror(0);
            }
         } // i
      } // if pulled using ik
      else if (dwChanged) {
         // resized
         fp fAmt = fTotalPost - fTotalPre;
         CListFixed lPoint, lTwist;
         for (i = 0; i < dwNum; i++) {
            if ((i == m_dwCurEdit) || !phh[i].pLock)
               continue;
            fp fWeight = RootWeight (&phh[m_dwCurEdit].pLocSphere, i);
            if (!fWeight)
               continue;

            if ((dwChanged == 4) || (dwChanged == 5)) {   // resize
               // get the current size
               fp fSize = phh[i].pLock->TotalLengthGet ();
               fSize += fAmt * fWeight;
               fSize = max(fSize, 0.01);
               phh[i].pLock->TotalLengthSet (fSize, phh[m_dwCurEdit].pLock);

               // if also ended up moving the last control point then do this...
               if (dwChanged == 5) {
                  DWORD dwNumNode;
                  CPoint pAdd;
                  fp *pfTwist;
                  PCPoint paPoint = phh[i].pLock->SplineGet (&dwNumNode, &pfTwist);
                  lPoint.Init (sizeof(CPoint), paPoint, dwNumNode);
                  lTwist.Init (sizeof(fp), pfTwist, dwNumNode);
                  pAdd.Copy (&pChangedVect);
                  pAdd.Scale (fWeight);
                  paPoint = (PCPoint)lPoint.Get(0);
                  paPoint[dwNumNode-1].Add (&pAdd);
                  phh[i].pLock->SplineSet (dwNumNode, (PCPoint)lPoint.Get(0), pfTwist);
               }

            }
            else if (dwChanged == 2) { // twist
               DWORD dwNumNode;
               fp *pfTwist;
               PCPoint pfPoint = phh[i].pLock->SplineGet (&dwNumNode, &pfTwist);
               if (dwNode < dwNumNode) {
                  lPoint.Init (sizeof(CPoint), pfPoint, dwNumNode);
                  lTwist.Init (sizeof(fp), pfTwist, dwNumNode);
                  pfTwist = (fp*)lTwist.Get(0);
                  pfTwist[dwNode] += pChangedVect.p[0] * fWeight;
                  phh[i].pLock->SplineSet (dwNumNode, (PCPoint)lPoint.Get(0), pfTwist);
               }
            }

            // mirror?
            dwMirror = RootMirror (i);
            if (m_fSymmetry && phh[dwMirror].pLock) {
               phh[i].pLock->CloneTo (phh[dwMirror].pLock);
               phh[dwMirror].pLock->Mirror(0);
            }
         } // i
      } // if resize



      m_pWorld->ObjectChanged (this);

      return TRUE;
   }

   if (dwID >= dwNum)
      return FALSE;

   // else, clicked on a hair or an empty spot
   if (phh[dwID].pLock) {
      // clicked on hair, so sleect
      m_pWorld->ObjectAboutToChange (this);
      m_dwCurEdit = dwID;
      m_pWorld->ObjectChanged (this);
   }
   else {
      // create
      m_pWorld->ObjectAboutToChange (this);
      m_dwCurEdit = dwID;
      SproutNewHair (m_dwCurEdit, 0.5);
      if (m_fSymmetry)
         SproutNewHair (RootMirror(m_dwCurEdit), 0.5);

      // if have area of affect then sprout hairs in area aounnd affected,
      // but make length less
      DWORD i;
      for (i = 0; i < dwNum; i++) {
         if (i == m_dwCurEdit)
            continue;   // dont sprout on same one
         fp fScore = RootWeight (&phh[m_dwCurEdit].pLocSphere, i);
         if (fScore) {
            SproutNewHair (i, fScore * 0.5);
            if (m_fSymmetry)
               SproutNewHair (RootMirror(i), fScore * 0.5);
         }
      }

      m_pWorld->ObjectChanged (this);
   }
   return TRUE;
}

/*************************************************************************************
CObjectHairHead::ControlPointEnum - Called to enumerate a list of control point IDs
into the list.

inputs
   PCListFixed       plDWORD - Should be filled in with control points
returns
   none
*/
void CObjectHairHead::ControlPointEnum (PCListFixed plDWORD)
{
   // if the currently edited value doesn't have a hair, then dont bothter
   PHAIRHEADROOT phh = (PHAIRHEADROOT)m_lHAIRHEADROOT.Get(0);
   DWORD dwNum = m_lHAIRHEADROOT.Num();
   if ((m_dwCurEdit < dwNum) && !phh[m_dwCurEdit].pLock)
      m_dwCurEdit = -1;

   // if CP higher than number of points and have a current edit then pass down
   if (m_dwCurEdit < dwNum)
      phh[m_dwCurEdit].pLock->ControlPointEnum (dwNum, FALSE, TRUE, plDWORD);

   // also include points for general movement
   DWORD i;
   for (i = 0; i < dwNum; i++)
      if (i != m_dwCurEdit)
         plDWORD->Add (&i);
}







/****************************************************************************
HairHeadPage
*/
BOOL HairHeadPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCObjectHairHead pv = (PCObjectHairHead)pPage->m_pUserData;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         PCEscControl pControl;

         // checkboxes
         pControl = pPage->ControlFind (L"backfacecull");
         if (pControl)
            pControl->AttribSetBOOL (Checked(), pv->m_fBackfaceCull);
         pControl = pPage->ControlFind (L"showellipse");
         if (pControl)
            pControl->AttribSetBOOL (Checked(), pv->m_fShowEllipse);
         pControl = pPage->ControlFind (L"symmetry");
         if (pControl)
            pControl->AttribSetBOOL (Checked(), pv->m_fSymmetry);

         MeasureToString (pPage, L"radius0", pv->m_pRadius.p[0] * 2);
         MeasureToString (pPage, L"radius1", pv->m_pRadius.p[1] * 2);
         MeasureToString (pPage, L"radius2", pv->m_pRadius.p[2] * 2);

         MeasureToString (pPage, L"lengthperpoint", pv->m_fLengthPerPoint);

         // diameter will be scrollbar from 0 to 1
         pControl = pPage->ControlFind (L"diameter");
         if (pControl)
            pControl->AttribSetInt (Pos(), (int)(pv->m_fDiameter * 100.0));

         // brush size
         pControl = pPage->ControlFind (L"brushsize");
         if (pControl)
            pControl->AttribSetInt (Pos(), (int)(pv->m_fBrushSize * 100.0));

         // endhairweight
         pControl = pPage->ControlFind (L"endhairweight");
         if (pControl)
            pControl->AttribSetInt (Pos(), (int)(pv->m_fEndHairWeight * 100.0));

         // density
         pControl = pPage->ControlFind (L"density");
         if (pControl)
            pControl->AttribSetInt (Pos(), (int)pv->m_dwDensity);

         DWORD i, j;
         WCHAR szTemp[32];
         PTEXTUREPOINT ptpProf = pv->m_atpProfile;
         for (i = 0; i < 5; i++) for (j = 0; j < 2; j++) {
            swprintf (szTemp, L"profile%d%d", (int)i, (int)j);
            pControl = pPage->ControlFind (szTemp);
            if (pControl)
               pControl->AttribSetInt (Pos(), (int)((j ? ptpProf[i].v : ptpProf[i].h) * 100.0));
         }
         pControl = pPage->ControlFind (L"variation");
         if (pControl)
            pControl->AttribSetInt (Pos(), (int)(pv->m_fVariation * 100.0));

         DWORD *padwRepeat;
         fp *pafScale;
         DWORD dwHairLayers = pv->m_dwHairLayers;
         padwRepeat = pv->m_adwHairLayerRepeat;
         pafScale = pv->m_afHairLayerScale;
         ComboBoxSet (pPage, L"hairlayers", dwHairLayers-1);
         for (i = 0; i < 3; i++) {
            swprintf (szTemp, L"hairlayerscale%d", (int)i);
            pControl = pPage->ControlFind (szTemp);
            if (pControl) {
               pControl->AttribSetInt (Pos(), (int)(pafScale[i] * 100.0));
               pControl->Enable (i < dwHairLayers);
            }

            swprintf (szTemp, L"hairlayerrepeat%d", (int)i);
            DoubleToControl (pPage, szTemp, padwRepeat[i]);
            pControl = pPage->ControlFind (szTemp);
            if (pControl)
               pControl->Enable (i < dwHairLayers);
         }

      }
      break;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;
         PWSTR psz;
         if (!p->pControl || !(psz = p->pControl->m_pszName))
            break;

         BOOL fChecked = p->pControl->AttribGetBOOL (Checked());
         PHAIRHEADROOT phh = (PHAIRHEADROOT)pv->m_lHAIRHEADROOT.Get(0);
         DWORD dwNum = pv->m_lHAIRHEADROOT.Num();
         DWORD i;

         if (!_wcsicmp(psz, L"backfacecull")) {
            pv->m_pWorld->ObjectAboutToChange (pv);
            pv->m_fBackfaceCull = fChecked;
            pv->m_pWorld->ObjectChanged (pv);
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"symmetry")) {
            pv->m_pWorld->ObjectAboutToChange (pv);
            pv->m_fSymmetry = fChecked;
            pv->m_pWorld->ObjectChanged (pv);
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"showellipse")) {
            pv->m_pWorld->ObjectAboutToChange (pv);
            pv->m_fShowEllipse = fChecked;
            pv->m_pWorld->ObjectChanged (pv);
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"doublecheck")) {
            pv->m_pWorld->ObjectAboutToChange (pv);

            CPoint pZero;
            pZero.Zero();

            for (i = 0; i < dwNum; i++)
               if (phh[i].pLock)
                  phh[i].pLock->IKPull ((DWORD)0, (DWORD*)NULL, (PCPoint)NULL, &pZero, &pv->m_pRadius, 0 /*m_fEndHairWeight*/);

            pv->m_pWorld->ObjectChanged (pv);
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"gravity") || !_wcsicmp(psz, L"ungravity")) {
            fp fAmount = 0.2 * ((!_wcsicmp(psz, L"gravity")) ? 1 : -1);
            pv->m_pWorld->ObjectAboutToChange (pv);

            CPoint pZero;
            pZero.Zero();

            for (i = 0; i < dwNum; i++)
               if (phh[i].pLock)
                  phh[i].pLock->GravitySimulate (fAmount, &pZero, &pv->m_pRadius);

            pv->m_pWorld->ObjectChanged (pv);
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"messup")) {
            pv->m_pWorld->ObjectAboutToChange (pv);
            CPoint pZero;
            pZero.Zero();
            srand(GetTickCount());
            for (i = 0; i < dwNum; i++)
               if (phh[i].pLock)
                  phh[i].pLock->MessUp (0.5, &pZero, &pv->m_pRadius);
               // BUGFIX - Was mess up of 0.3, changed to 0.5 since too slow at messing
            pv->m_pWorld->ObjectChanged (pv);
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"straighten")) {
            pv->m_pWorld->ObjectAboutToChange (pv);
            for (i = 0; i < dwNum; i++)
               if (phh[i].pLock)
                  phh[i].pLock->Straighten();
            pv->m_pWorld->ObjectChanged (pv);
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"grow") || !_wcsicmp(psz, L"ungrow")) {
            BOOL fGrow = !_wcsicmp(psz, L"grow");

            pv->m_pWorld->ObjectAboutToChange (pv);
            CPoint pZero;
            pZero.Zero();
            for (i = 0; i < dwNum; i++)
               if (phh[i].pLock) {
                  fp fLen = phh[i].pLock->TotalLengthGet();
                  fLen += pv->m_fLengthPerPoint / 2.0 * (fGrow? 1 : -1);
                  fLen = max(pv->m_fLengthPerPoint/4.0, fLen);
                  phh[i].pLock->TotalLengthSet (fLen);
               }
            pv->m_pWorld->ObjectChanged (pv);
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"mirror")) {
            // create a scratch buffer so knows if have already swapped
            CMem mem;
            if (!mem.Required (dwNum * sizeof(BOOL)))
               return FALSE;
            BOOL *paf = (BOOL*)mem.p;
            memset (paf, 0, dwNum * sizeof(BOOL));

            pv->m_pWorld->ObjectAboutToChange (pv);
            for (i = 0; i < dwNum; i++) {
               if (paf[i])
                  continue;   // already checked

               // find mirror
               DWORD dwMirror = pv->RootMirror(i);
               paf[i] = paf[dwMirror] = TRUE;   // so know that mirrored

               // keep temp
               PCHairLock ph;
               ph = phh[i].pLock;
               phh[i].pLock = phh[dwMirror].pLock;
               if (phh[i].pLock)
                  phh[i].pLock->Mirror(0);
               phh[dwMirror].pLock = ph;
               if (phh[dwMirror].pLock)
                  phh[dwMirror].pLock->Mirror(0);
            }
            pv->m_pWorld->ObjectChanged (pv);
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"mirrorltor") || !_wcsicmp(psz, L"mirrorrtol")) {
            BOOL fLToR = !_wcsicmp(psz, L"mirrorltor");

            pv->m_pWorld->ObjectAboutToChange (pv);
            for (i = 0; i < dwNum; i++) {
               if (phh[i].pLocSphere.p[0] * (fLToR ? 1 : -1) < 0)
                  continue;   // dont care about this side

               // find mirror
               DWORD dwMirror = pv->RootMirror(i);

               // potentially delte/add
               if (phh[dwMirror].pLock)
                  delete phh[dwMirror].pLock;
               phh[dwMirror].pLock = NULL;
               if (!phh[i].pLock)
                  continue;

               // else want
               phh[dwMirror].pLock = phh[i].pLock->Clone();
               phh[dwMirror].pLock->Mirror(0);
            }
            pv->m_pWorld->ObjectChanged (pv);
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"twistlength") || !_wcsicmp(psz, L"twistroot")) {
            BOOL fLength = !_wcsicmp(psz, L"twistlength");

            // get scroll
            WCHAR szTemp[64];
            PCEscControl pControl;
            fp fVal = 0;
            wcscpy (szTemp, psz);
            wcscat (szTemp, L"s");
            pControl = pPage->ControlFind (szTemp);
            if (pControl)
               fVal = (fp)pControl->AttribGetInt (Pos()) / 50.0;
            fVal *= (fLength ? (PI/2) : PI);

            pv->m_pWorld->ObjectAboutToChange (pv);
            for (i = 0; i < dwNum; i++)
               if (phh[i].pLock)
                  phh[i].pLock->Twist(fLength, fVal);
            pv->m_pWorld->ObjectChanged (pv);
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"density4") || !_wcsicmp(psz, L"density2") || !_wcsicmp(psz, L"density12") || !_wcsicmp(psz, L"density14")) {
            fp fMult = 4;
            if (!_wcsicmp(psz, L"density2"))
               fMult = 2;
            else if (!_wcsicmp(psz, L"density12"))
               fMult = 0.5;
            else if (!_wcsicmp(psz, L"density14"))
               fMult = 0.25;
            fp fSqrt = sqrt(fMult);

            // figure out the distance that will need to go
            fp fDist =  2.0 * PI / ((fp)pv->m_dwDensity * 4) * 1.1;

            pv->m_pWorld->ObjectAboutToChange (pv);

            // copy over the existing hair information and wipe out the old one...
            CListFixed lCopy;
            lCopy.Init (sizeof (HAIRHEADROOT), phh, dwNum);
            for (i = 0; i < dwNum; i++)
               phh[i].pLock = NULL;   // so wont delete twice
            PHAIRHEADROOT phhCopy = (PHAIRHEADROOT)lCopy.Get(0);
            DWORD dwNumCopy = lCopy.Num();

            // wipe out existing data and fill with new
            pv->m_dwDensity = floor((fp)pv->m_dwDensity * fSqrt + 0.5);
            pv->m_dwDensity = max(pv->m_dwDensity, 1);
            pv->m_dwDensity = min(pv->m_dwDensity, 20);
            pv->NewDensity();


            // loop through all the points and see if want to create
            CListFixed lBlendHair, lBlendWeight;
            lBlendHair.Init (sizeof(PCHairLock));
            lBlendWeight.Init (sizeof(fp));
            phh = (PHAIRHEADROOT)pv->m_lHAIRHEADROOT.Get(0);
            dwNum = pv->m_lHAIRHEADROOT.Num();
            DWORD j, dwCloseEmpty;
            for (i = 0; i < dwNum; i++) {
               lBlendHair.Clear();
               lBlendWeight.Clear();
               dwCloseEmpty = 0;

               // see if any points in the old one are near...
               for (j = 0; j < dwNumCopy; j++) {
                  CPoint pDist;
                  fp fVal;
                  pDist.Subtract (&phh[i].pLocSphere, &phhCopy[j].pLocSphere);
                  fVal = pDist.Length();
                  if (fVal >= fDist)
                     continue;   // too far

                  // if it's close but it's empty, note this
                  if (!phhCopy[j].pLock) {
                     dwCloseEmpty++;
                     continue;
                  }

                  fVal = 1.0 - (fVal / fDist);
                  lBlendWeight.Add (&fVal);
                  lBlendHair.Add (&phhCopy[j].pLock);
               } // j

               // if there are more close and empty points then valid ones to average
               // then skip
               if (!lBlendHair.Num() || (dwCloseEmpty > lBlendHair.Num()))
                  continue;

               // else, create new hair
               phh[i].pLock = new CHairLock;
               if (!phh[i].pLock)
                  continue;

               // start location and normal
               CPoint pRoot, pSubRoot;
               pRoot.Copy (&phh[i].pLocSphere);
               pRoot.p[0] *= pv->m_pRadius.p[0];
               pRoot.p[1] *= pv->m_pRadius.p[1];
               pRoot.p[2] *= pv->m_pRadius.p[2];
               pSubRoot.Subtract (&pRoot, &phh[i].pNorm);

               // blend
               phh[i].pLock->Blend (&pSubRoot, &pRoot, lBlendHair.Num(),
                  (PCHairLock*)lBlendHair.Get(0), (fp*)lBlendWeight.Get(0));
            } // i

            // because have a new size, may have changed diameter, so set
            pv->SyncAllRoots ();

            pv->m_pWorld->ObjectChanged (pv);


            // before quit, free up copy
            for (i = 0; i < dwNumCopy; i++)
               if (phhCopy[i].pLock)
                  delete phhCopy[i].pLock;

            // set scrollbar
            PCEscControl pControl;
            pControl = pPage->ControlFind (L"density");
            if (pControl)
               pControl->AttribSetInt (Pos(), (int)pv->m_dwDensity);
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

         if (!_wcsicmp(psz, L"hairlayers")) {
            if (dwVal == pv->m_dwHairLayers-1)
               break;

            // else, change
            pv->m_pWorld->ObjectAboutToChange (pv);
            pv->m_dwHairLayers = dwVal+1;
            pv->SyncAllRoots();
            pv->m_pWorld->ObjectChanged (pv);

            // enable/disable
            DWORD i;
            PCEscControl pControl;
            WCHAR szTemp[32];
            for (i = 0; i < 3; i++) {
               swprintf (szTemp, L"hairlayerscale%d", (int)i);
               pControl = pPage->ControlFind (szTemp);
               if (pControl)
                  pControl->Enable (i < pv->m_dwHairLayers);

               swprintf (szTemp, L"hairlayerrepeat%d", (int)i);
               pControl = pPage->ControlFind (szTemp);
               if (pControl)
                  pControl->Enable (i < pv->m_dwHairLayers);
            }
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

         DWORD dwCat, dwArray, dwArray2;
         dwCat = -2;
         PWSTR pszProfile = L"profile", pszHairLayerScale = L"hairlayerscale";
         DWORD dwLenProfile = (DWORD)wcslen(pszProfile), dwLenHairLayerScale = (DWORD)wcslen(pszHairLayerScale);
         if (!_wcsicmp(p->pControl->m_pszName, L"variation"))
            dwCat = 0;
         else if (!wcsncmp(p->pControl->m_pszName, pszProfile, dwLenProfile)) {
            dwCat = 1;
            dwArray = p->pControl->m_pszName[dwLenProfile] - L'0';
            dwArray2 = p->pControl->m_pszName[dwLenProfile+1] - L'0';
         }
         else if (!wcsncmp(p->pControl->m_pszName, pszHairLayerScale, dwLenHairLayerScale)) {
            dwCat = 2;
            dwArray = p->pControl->m_pszName[dwLenHairLayerScale] - L'0';
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"diameter"))
            dwCat = 3;
         else if (!_wcsicmp(p->pControl->m_pszName, L"density"))
            dwCat = 4;
         else if (!_wcsicmp(p->pControl->m_pszName, L"brushsize"))
            dwCat = 5;
         else if (!_wcsicmp(p->pControl->m_pszName, L"endhairweight"))
            dwCat = 6;
         else
            break;   // not one of these

         fp fVal;
         int iVal;
         iVal = p->pControl->AttribGetInt (Pos());
         fVal = (fp)iVal / 100.0;

         pv->m_pWorld->ObjectAboutToChange (pv);
         if (dwCat == 0) {
            pv->m_fVariation = fVal;
            pv->SyncAllRoots();
         }
         else if (dwCat == 1) {
            if (dwArray2)
               pv->m_atpProfile[dwArray].v = fVal;
            else
               pv->m_atpProfile[dwArray].h = fVal;
            pv->SyncAllRoots();
         }
         else if (dwCat == 2) {
            pv->m_afHairLayerScale[dwArray] = fVal;
            pv->SyncAllRoots();
         }
         else if (dwCat == 3) {
            pv->m_fDiameter = max(fVal, 0.01);
            pv->SyncAllRoots();
         }
         else if (dwCat == 4) {
            pv->m_dwDensity = (DWORD)iVal;
            pv->NewDensity();
         }
         else if (dwCat == 5) {
            pv->m_fBrushSize = fVal;
         }
         else if (dwCat == 6) {
            pv->m_fEndHairWeight = fVal;
         }
         pv->m_pWorld->ObjectChanged (pv);
         return TRUE;
      }
      break;

   case ESCN_EDITCHANGE:
      {
         PESCNEDITCHANGE p = (PESCNEDITCHANGE) pParam;
         PWSTR psz = p->pControl->m_pszName;
         if (!psz)
            break;   // not this edit

         pv->m_pWorld->ObjectAboutToChange (pv);

         // hairs
         fp fVal;
         if (!_wcsicmp(psz, L"radius0") || !_wcsicmp(psz, L"radius1") || !_wcsicmp(psz, L"radius2")) {
            MeasureParseString (pPage, L"radius0", &fVal);
            if (fVal > CLOSE)
               pv->m_pRadius.p[0] = fVal / 2.0;
            MeasureParseString (pPage, L"radius1", &fVal);
            if (fVal > CLOSE)
               pv->m_pRadius.p[1] = fVal / 2.0;
            MeasureParseString (pPage, L"radius2", &fVal);
            if (fVal > CLOSE)
               pv->m_pRadius.p[2] = fVal / 2.0;

            pv->ApplyRadius();
            pv->SyncAllRoots();  // call this since changing the radius may affect the size of hairs
         }
         else {
            // code that causes hairs to redraw
            MeasureParseString (pPage, L"lengthperpoint", &fVal);
            if ((fVal > CLOSE) && (fVal != pv->m_fLengthPerPoint))
               pv->m_fLengthPerPoint = fVal;


            DWORD i;
            WCHAR szTemp[32];
            for (i = 0; i < MAXHAIRLAYERS; i++) {
               swprintf (szTemp, L"hairlayerrepeat%d", (int)i);
               fVal = DoubleFromControl (pPage, szTemp);
               pv->m_adwHairLayerRepeat[i] = max((DWORD)fVal,1);
            }

            pv->SyncAllRoots();
         }

         pv->m_pWorld->ObjectChanged (pv);
      }
      break;


   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Head-of-hair settings";
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}


/**********************************************************************************
CObjectHairHead::DialogShow - Causes the object to show a dialog box that allows
different attributes to be changed.

inputs
   DWORD             dwSuface - Surface that was clicked on. 0 if unknown.
   PCEscWindow       pWindow - Escarpment window to draw it in
returns
   BOOL - TRUE if user pressed "Back", FALSE if user pressed "Close"
*/
BOOL CObjectHairHead::DialogShow (DWORD dwSurface, PCEscWindow pWindow, PCPoint pClick)
{
   PWSTR pszRet;

   pszRet = pWindow->PageDialog (ghInstance, IDR_MMLHAIRHEAD, HairHeadPage, this);
   if (!pszRet)
      return FALSE;

   return !_wcsicmp(pszRet, Back());
}

/**********************************************************************************
CObjectHairHead::DialogQuery - Returns TRUE if the object supports a custom
dialog box, FALSE if it doesn't. The default object doesn't
*/
BOOL CObjectHairHead::DialogQuery (void)
{
   return TRUE;
}




/****************************************************************************
HairHeadCPPage
*/
BOOL HairHeadCPPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCObjectHairHead pv = (PCObjectHairHead)pPage->m_pUserData;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         PCEscControl pControl;

         // brush size
         pControl = pPage->ControlFind (L"brushsize");
         if (pControl)
            pControl->AttribSetInt (Pos(), (int)(pv->m_fBrushSize * 100.0));

         pControl = pPage->ControlFind (L"endhairweight");
         if (pControl)
            pControl->AttribSetInt (Pos(), (int)(pv->m_fEndHairWeight * 100.0));

         // density
         pControl = pPage->ControlFind (L"density");
         if (pControl)
            pControl->AttribSetInt (Pos(), (int)pv->m_dwDensity);
      }
      break;


   case ESCN_SCROLL:
   case ESCN_SCROLLING:
      {
         PESCNSCROLL p = (PESCNSCROLL) pParam;

         // only do one scroll bar
         if (!p->pControl->m_pszName)
            break;

         DWORD dwCat;
         dwCat = -2;
         if (!_wcsicmp(p->pControl->m_pszName, L"density"))
            dwCat = 4;
         else if (!_wcsicmp(p->pControl->m_pszName, L"brushsize"))
            dwCat = 5;
         else if (!_wcsicmp(p->pControl->m_pszName, L"endhairweight"))
            dwCat = 6;
         else
            break;   // not one of these

         fp fVal;
         int iVal;
         iVal = p->pControl->AttribGetInt (Pos());
         fVal = (fp)iVal / 100.0;

         pv->m_pWorld->ObjectAboutToChange (pv);
         if (dwCat == 4) {
            pv->m_dwDensity = (DWORD)iVal;
            pv->NewDensity();
         }
         else if (dwCat == 5) {
            pv->m_fBrushSize = fVal;
         }
         else if (dwCat == 6) {
            pv->m_fEndHairWeight = fVal;
         }
         pv->m_pWorld->ObjectChanged (pv);
         return TRUE;
      }
      break;


   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Head-of-hair control points";
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}


/**********************************************************************************
CObjectHairHead::DialogCPShow - Causes the object to show a dialog box that allows
different attributes to be changed.

inputs
   DWORD             dwSuface - Surface that was clicked on. 0 if unknown.
   PCEscWindow       pWindow - Escarpment window to draw it in
returns
   BOOL - TRUE if user pressed "Back", FALSE if user pressed "Close"
*/
BOOL CObjectHairHead::DialogCPShow (DWORD dwSurface, PCEscWindow pWindow, PCPoint pClick)
{
   PWSTR pszRet;

   pszRet = pWindow->PageDialog (ghInstance, IDR_MMLHAIRHEADCP, HairHeadCPPage, this);
   if (!pszRet)
      return FALSE;

   return !_wcsicmp(pszRet, Back());
}

/**********************************************************************************
CObjectHairHead::DialogCPQuery - Returns TRUE if the object supports a custom
dialog box, FALSE if it doesn't. The default object doesn't
*/
BOOL CObjectHairHead::DialogCPQuery (void)
{
   return TRUE;
}



/*****************************************************************************************
CObjectHairHead::AttribGetIntern - OVERRIDE THIS

Like AttribGet() except that only called if default attributes not handled.
*/
static PWSTR gpszAttribHatElev = L"HatElev";
static PWSTR gpszAttribHatRotX = L"HatRotX";
static PWSTR gpszAttribHatRotY = L"HatRotY";

BOOL CObjectHairHead::AttribGetIntern (PWSTR pszName, fp *pfValue)
{
   if (!_wcsicmp(pszName, gpszAttribHatElev)) {
      *pfValue = m_pHat.p[0];
      return TRUE;
   }
   else if (!_wcsicmp(pszName, gpszAttribHatRotX)) {
      *pfValue = m_pHat.p[1];
      return TRUE;
   }
   else if (!_wcsicmp(pszName, gpszAttribHatRotY)) {
      *pfValue = m_pHat.p[2];
      return TRUE;
   }
   else
      return FALSE;
}


/*****************************************************************************************
CObjectHairHead::AttribGetAllIntern - OVERRIDE THIS

Like AttribGetAllIntern() EXCEPT plATTRIBVAL is already initialized and filled with
some parameters (default to the object template)
*/
void CObjectHairHead::AttribGetAllIntern (PCListFixed plATTRIBVAL)
{
   ATTRIBVAL av;
   memset (&av, 0, sizeof(av));

   wcscpy (av.szName, gpszAttribHatElev);
   av.fValue = m_pHat.p[0];
   plATTRIBVAL->Add (&av);

   wcscpy (av.szName, gpszAttribHatRotX);
   av.fValue = m_pHat.p[1];
   plATTRIBVAL->Add (&av);

   wcscpy (av.szName, gpszAttribHatRotY);
   av.fValue = m_pHat.p[2];
   plATTRIBVAL->Add (&av);

}


/*****************************************************************************************
CObjectHairHead::AttribSetGroupIntern - OVERRIDE THIS

Like AttribSetGroup() except passing on non-template attributes.
*/
void CObjectHairHead::AttribSetGroupIntern (DWORD dwNum, PATTRIBVAL paAttrib)
{
   BOOL fChanged = FALSE;

#define CHANGED if (!fChanged && m_pWorld) m_pWorld->ObjectAboutToChange(this); fChanged = TRUE

   DWORD i;
   for (i = 0; i < dwNum; i++, paAttrib++) {
      if (!_wcsicmp (paAttrib->szName, gpszAttribHatElev)) {
         if (paAttrib->fValue == m_pHat.p[0])
            continue;   // no change

         CHANGED;
         m_pHat.p[0] = paAttrib->fValue;
      }
      else if (!_wcsicmp (paAttrib->szName, gpszAttribHatRotX)) {
         if (paAttrib->fValue == m_pHat.p[1])
            continue;   // no change

         CHANGED;
         m_pHat.p[1] = paAttrib->fValue;
      }
      else if (!_wcsicmp (paAttrib->szName, gpszAttribHatRotY)) {
         if (paAttrib->fValue == m_pHat.p[2])
            continue;   // no change

         CHANGED;
         m_pHat.p[2] = paAttrib->fValue;
      }
   } // i

   if (fChanged) {
      if (m_pWorld)
         m_pWorld->ObjectChanged (this);
   }
}


/*****************************************************************************************
CObjectHairHead::AttribInfoIntern - OVERRIDE THIS

Like AttribInfo() except called if attribute is not for template.
*/
BOOL CObjectHairHead::AttribInfoIntern (PWSTR pszName, PATTRIBINFO pInfo)
{
   DWORD dwNum;

   if (!_wcsicmp(pszName, gpszAttribHatElev))
      dwNum = 0;
   else if (!_wcsicmp(pszName, gpszAttribHatRotX))
      dwNum = 1;
   else if (!_wcsicmp(pszName, gpszAttribHatRotY))
      dwNum = 2;
   else
      return FALSE;

   memset (pInfo, 0, sizeof(*pInfo));
   pInfo->dwType = (dwNum == 0) ? AIT_NUMBER : AIT_ANGLE;
   pInfo->fDefPassUp = TRUE;
   pInfo->fDefLowRank = TRUE;
   pInfo->fMin = dwNum ? -PI : -1000;
   pInfo->fMax = dwNum ? PI : 1000;
   switch (dwNum) {
   case 0:
      pInfo->pszDescription = L"The height of the hat. Use 1000m for no hat.";
      break;
   case 1:
      pInfo->pszDescription = L"Angle of hat, rotated around X.";
      break;
   case 2:
      pInfo->pszDescription = L"Angle of hat, rotated around Y.";
      break;
   } // swtich

   return TRUE;
}

