/*****************************************************************************
CLight.cpp - For one shadow buffer

begun 27/5/02 by Mike Rozak.
Copyright 2002 Mike Rozak. All rights reserved
*/

#include <windows.h>
#include <math.h>
#include "escarpment.h"
#include "..\M3D.h"
#include "resource.h"

#define  SIZEINFINITE      1024 // works at 512, but much nicer at 1024
#define  SIZEINFINITEONCAMERA    (SIZEINFINITE + SIZEINFINITE/2)     // infinte light centered on camera
   // BUGFIX - Was 2048
#define  SIZESTRONGLIGHT   (SIZEINFINITE/2)   // BUGFIX - Increased this size from 512
   // BUGFIX - Was 768
#define  SIZEWEAKLIGHT     (SIZESTRONGLIGHT - SIZESTRONGLIGHT/4)   // BUGFIX - Increase this size from 256
   // BUGFIX - Was 512
#define LIGHTMAXIMUMSCALESTRONGLIGHT      0.5      // strng light uses 1/2 light maximum
#define LIGHTMAXIMUMSCALEWEAKLIGHT        0.25     // weak light uses 1/4 light maximum

#define INTERIORINFINITELIGHTHACK         0.15      // let a small percentage of infinite lights into
                                                   // shadowed areas so that textures aren't all flat
                                                   // in shadow

#define FASTSHADOWS           // so faster, but wont get blending

/************************************************************************************
CLight::Constructor and destructor
*/
CLight::CLight (DWORD dwRenderShard, int iPriorityIncrease)
{
   m_dwRenderShard = dwRenderShard;
   m_iPriorityIncrease = iPriorityIncrease;
   memset (&m_li, 0, sizeof(m_li));
   m_fDirty = TRUE;
   m_apBoundary[0].Zero();
   m_apBoundary[1].Zero();
   m_pWorld = NULL;
   m_dwShowFlags = 0;
   m_fFinalRender = FALSE;
   m_dwShowOnly = -1;
   memset (m_apShadowBuf, 0, sizeof(m_apShadowBuf));
   memset (m_afDirectCos, 0, sizeof(m_afDirectCos));
   m_fLightMaximum = 0;
   m_pSpotCenter.Zero();
   m_fSpotDiameter = 0;
}

CLight::~CLight (void)
{
   DWORD i;
   for (i = 0; i < 6; i++)
      if (m_apShadowBuf[i])
         delete m_apShadowBuf[i];
}


/************************************************************************************
CLight::Clone - Clones the light and all the info

returns
   PCLight - New object, or NULL if error
*/
PCLight CLight::Clone (void)
{
   PCLight pClone = new CLight(m_dwRenderShard, m_iPriorityIncrease);
   if (!pClone)
      return NULL;

   pClone->m_li = m_li;
   pClone->m_fDirty = m_fDirty;
   memcpy (pClone->m_apBoundary, m_apBoundary, sizeof(m_apBoundary));
   pClone->m_pWorld = m_pWorld;
   pClone->m_dwShowFlags = m_dwShowFlags;
   pClone->m_fFinalRender = m_fFinalRender;
   pClone->m_dwShowOnly = m_dwShowOnly;
   pClone->m_pDirNorm.Copy (&m_pDirNorm);
   memcpy (pClone->m_afDirectCos, m_afDirectCos, sizeof(m_afDirectCos));
   pClone->m_pSpotCenter.Copy (&m_pSpotCenter);
   pClone->m_fSpotDiameter = m_fSpotDiameter;
   pClone->m_fLightMaximum = m_fLightMaximum;

   DWORD i;
   for (i = 0; i < 6; i++) {
      if (!m_apShadowBuf[i]) {
         pClone->m_apShadowBuf[i] = NULL;
         continue;
      }
      
      pClone->m_apShadowBuf[i] = m_apShadowBuf[i]->Clone();
   } // i

   return pClone;
}

/************************************************************************************
CLight::WorldSet - Should be called by the light's creator to tell it what world
to use for the light buffer AND what show flags to pass down. Can call this several
times no problem.

inputs
   PCWorldSocket        pWorld - World to use
   DWORD          dwShowFlags - To pass into Render::Render()
   BOOL           fFinalRender - If TRUE then shadows should be for the final render
   DWORD          dwShowOnly - -1 to have all objects cast shadows, or an object number for pecific
returns
   none
*/
void CLight::WorldSet (PCWorldSocket pWorld, DWORD dwShowFlags, BOOL fFinalRender, DWORD dwShowOnly)
{
   if (pWorld != m_pWorld) {
      m_pWorld = pWorld;
      m_fDirty = TRUE;
   }
   if (dwShowFlags != m_dwShowFlags) {
      m_dwShowFlags = dwShowFlags;
      m_fDirty = TRUE;
   }
   if (fFinalRender != m_fFinalRender) {
      m_fFinalRender = fFinalRender;
      m_fDirty = TRUE;
   }

   if (dwShowOnly != m_dwShowOnly) {
      m_dwShowOnly = dwShowOnly;
      m_fDirty = TRUE;
   }
}

/************************************************************************************
CLight::LightInfoSet - Sets the light's location and power according to the structure
passed in. All coords are in world space.

inputs
   PLIGHTINFO     pInfo - Information
   fp             fLightMaximum - Sets the maximum distance the light will travel (as set
                  in traditional render). This may be reduced down for weaker lights.
                  Only used for non-infinite lights
returns
   BOOL - TRUE if succes
*/
BOOL CLight::LightInfoSet (PLIGHTINFO pInfo, fp fLightMaximum)
{
   if (!memcmp (pInfo, &m_li, sizeof(m_li)) && (m_fLightMaximum == fLightMaximum) )
      return TRUE;   // no change

   // it's only dirty if actually move the light. Can change the intensity and color
   // no problem
   if (!m_li.pDir.AreClose (&pInfo->pDir) || !m_li.pLoc.AreClose(&pInfo->pLoc) ||
      (m_li.dwForm != pInfo->dwForm) || (m_li.fNoShadows != pInfo->fNoShadows) || (m_fLightMaximum != fLightMaximum))
         m_fDirty = TRUE;

   // copy over
   memcpy (&m_li, pInfo, sizeof(m_li));
   m_fLightMaximum = fLightMaximum;
   m_pDirNorm.Copy (&m_li.pDir);
   m_pDirNorm.Normalize();

   // if there's any directionl light, take the cos of the angle for faster reference
   if (m_li.afLumens[0] || m_li.afLumens[1]) {
      DWORD i,j;
      for (i = 0; i < 2; i++) for (j = 0; j < 2; j++)
         m_afDirectCos[i][j] = cos(m_li.afDirectionality[i][j] / 2.0);
         // divide by 2.0 so that over full arc will be correct length
   }
   return TRUE;
}

/************************************************************************************
CLight::LightInfoGet - Fills pInfo in with the current light information.

inputs
   PLIGHTINFO     pInfo - information
   fp             *pfLightMaximum - Filled with fLightMaximum from LightInfoSet().
                     Can be NULL.
returns
   BOOL - TRUE if succes
*/
BOOL CLight::LightInfoGet (PLIGHTINFO pInfo, fp *pfLightMaximum)
{
   *pInfo = m_li;
   if (pfLightMaximum)
      *pfLightMaximum = m_fLightMaximum;
   return TRUE;
}

/************************************************************************************
CLight::Dirty Set - Sets the dirty flag to TRUE.
*/
void CLight::DirtySet (void)
{
   m_fDirty = TRUE;
}

/************************************************************************************
CLight::DirtyGet - Gets the state of the dirty flag. Calling render can use this
to determine if the lights need updating. If they do can adjust the drawing progress
meter.
*/
BOOL CLight::DirtyGet (void)
{
   return m_fDirty;
}


/************************************************************************************
CLight::HasShadowBuf - Tests to see if any of the shadow buffers actually have
shadows.

returns
   DWORD - Number that have shadows
*/
DWORD CLight::HasShadowBuf (void)
{
   DWORD i;
   DWORD dwRet = 0;
   for (i = 0; i < 6; i++)
      if (m_apShadowBuf[i] && m_apShadowBuf[i]->HasShadowBuf())
         dwRet++;

   return dwRet;
}

/************************************************************************************
CLight::RecalcIfDirty - If the dirty flag is set, this will recalculate the light buffers
and ilk. Render can use this to recalculate in advance.

NOTE: If this is an infinite light then it WON'T actually calculate here, but instead
will create two spotlight later on.

inputs
   PCRenderTraditional     *ppRender - Will initially be passed a pointer to NULL.
                           If this needs to recalculate the light, this will allocate
                           a CRenderTraditional and then fill it into *ppRender.
                           The caller will need to delete this.
                           This minimizes the creation of render objects
                           Can be NULL, in which case won't create
   PCProgressSocket        pProgress - Progress bar to send info to. Can be NULL
returns
   BOOL - TRUE if success
*/
BOOL CLight::RecalcIfDirty (PCRenderTraditional *ppRender, PCProgressSocket pProgress)
{
   MALLOCOPT_INIT;
   if (!m_fDirty)
      return TRUE;
   if (!m_pWorld)
      return FALSE;

   // clear the old bits
   DWORD i;
   for (i = 0; i < 6; i++)
      if (m_apShadowBuf[i]) {
         delete m_apShadowBuf[i];
         m_apShadowBuf[i] = NULL;
      }

   // BUGFIX - If no shadows then ignore
   if (m_li.fNoShadows) {
      m_fDirty = FALSE;
      return TRUE;
   }

   // BUGFIX - Instead of trying to send a spotlight over the entire world, whcih
   // was ok for just houses, instead, create two spots instead of one
   // when calculating that
   if (m_li.dwForm == LIFORM_INFINITE) {
      m_fDirty = FALSE;
      return TRUE;
   }


   // figure out front, right, and up
   CPoint pX, pY, pZ;
   pY.Copy (&m_pDirNorm);
   pZ.Zero();
   pZ.p[2] = 1;
   pX.CrossProd (&pY, &pZ);
   if (pX.Length() < CLOSE) {
      pZ.Zero();
      pZ.p[0] = 1;
      pX.CrossProd (&pY, &pZ);
      if (pX.Length() < CLOSE)
         return FALSE;
   }
   pX.Normalize();
   pZ.CrossProd (&pX, &pY);
   pZ.Normalize();

   // how many panels do we need?
   DWORD dwPanels;

   // if doing the sun then zoom in as much as possible
   CPoint pEye;
   fp fScale, fScale2;

   if (m_li.dwForm == LIFORM_INFINITE) {
      // flat
      dwPanels = 1;
      pEye.Zero();
      CPoint b[2], c, c2;
      m_pWorld->WorldBoundingBoxGet (&b[0], &b[1], FALSE);
      c.Average (&b[0], &b[1]);
      pEye.Copy (&c);
      c2.Subtract (&c, &b[0]);
      fScale = 2 * c2.Length(); // a bit extra
   }
   else {
      dwPanels = 6;
      pEye.Copy (&m_li.pLoc);
      fScale = 0;

      // if no omni-light then fewer panels
      if (!m_li.afLumens[2]) {
         // no omni-directional light, so can make only one or two panels in the
         // direction of the light
         dwPanels = (m_li.afLumens[1] ? 2 : 1);
         fScale = -fabs(m_li.afDirectionality[0][0]);

         // BUGFIX - Also remember the other directionality
         fScale2 = -fabs(m_li.afDirectionality[1][0]);
      }
   }

   // how many pixels to divide by?
   DWORD dwPixels;
   fp fLightMaximum = m_fLightMaximum;
   if (m_li.dwForm == LIFORM_INFINITE)
      dwPixels = SIZEINFINITE;
   else {
      // if it's a strong light (such as outdoor), it will reach further, so
      // give it more detail
      if ((m_li.afLumens[0] + m_li.afLumens[1] + m_li.afLumens[2]) > 120 * CM_LUMENSPERINCANWATT) {
         dwPixels = SIZESTRONGLIGHT;
         fLightMaximum *= LIGHTMAXIMUMSCALESTRONGLIGHT;
      }
      else {
         dwPixels = SIZEWEAKLIGHT;
         fLightMaximum *= LIGHTMAXIMUMSCALEWEAKLIGHT;
      }
   }

   // BUGFIX - adjust the pixels by the texture detail
   int iDetail = TextureDetailGet(m_dwRenderShard);
   dwPixels = (DWORD)((fp)dwPixels * pow(2.0, (fp)iDetail/2.0) + 0.5);
   //if (iDetail > 0)
   //   dwPixels <<= (iDetail/2);  // BUGFIX - Detail doesn't affect shadows as much
   //else if (iDetail < 0)
   //   dwPixels >>= ((-iDetail)/2);  // BUGFIX - Detail doesn't affect shadows as much

   // if no render traditional then can't create
   if (!ppRender)
      return FALSE;

   // create all the panels
   BOOL fBoundary;
   fBoundary = FALSE;
   m_apBoundary[0].Zero();
   m_apBoundary[1].Zero();
   for (i = 0; i < dwPanels; i++) {
      MALLOCOPT_OKTOMALLOC;
      m_apShadowBuf[i] = new CShadowBuf;
      MALLOCOPT_RESTORE;
      if (!m_apShadowBuf[i])
         return FALSE;

      // which direction is front, etc.
      CPoint pFront, pUp;
      pUp.Copy (&pZ);
      switch (i) {
      case 0:  // facing front
         pFront.Copy (&pY);
         break;
      case 1:  // fcing back
         pFront.Copy (&pY);
         pFront.Scale (-1);
         break;
      case 2:  // facing right
         pFront.Copy (&pX);
         break;
      case 3:  // facing left
         pFront.Copy (&pX);
         pFront.Scale (-1);
         break;
      case 4:  // facing up
         pFront.Copy (&pZ);
         pUp.Copy (&pX);
         break;
      case 5:  // facing down
         pFront.Copy (&pZ);
         pFront.Scale (-1);
         pUp.Copy (&pX);
         break;
      }

      // BUGFIX - Can have different angle for top than bottom
      fp fTempScale;
      fTempScale = fScale;
      if ((i == 1) && (fScale < 0) && (dwPanels == 2))
         fTempScale = fScale2;

      if (!*ppRender) {
         *ppRender = new CRenderTraditional(m_dwRenderShard);
         if (!*ppRender)
            return FALSE;
         (*ppRender)->m_iPriorityIncrease = m_iPriorityIncrease;
      }
      if (pProgress)
         pProgress->Push ((fp)i / (fp)dwPanels, (fp)(i+1) / (fp) dwPanels);
      BOOL fRet = m_apShadowBuf[i]->Init (*ppRender, &pEye, &pFront, &pUp, m_pWorld, fTempScale, m_dwShowFlags,
         dwPixels, m_fFinalRender, m_dwShowOnly, NULL, fLightMaximum, pProgress);
      if (pProgress)
         pProgress->Pop();
      if (!fRet) {
         for (i = 0; i < dwPanels; i++)
            if (m_apShadowBuf[i]) {
               delete m_apShadowBuf[i];
               m_apShadowBuf[i] = NULL;
            }
         return FALSE;
      }

      // get the boundary
      CPoint   ab[2];
      m_apShadowBuf[i]->BoundaryGet (&ab[0], &ab[1]);
      if (!ab[0].AreClose(&ab[1])) {
         if (fBoundary) {
            m_apBoundary[0].Min (&ab[0]);
            m_apBoundary[1].Max (&ab[1]);
         }
         else {
            m_apBoundary[0].Copy (&ab[0]);
            m_apBoundary[1].Copy (&ab[1]);
            fBoundary = TRUE;
         }
      }
   }

   // done 
   m_fDirty = FALSE;
   return TRUE;
}

/*******************************************************************************
FPSort */
int __cdecl FPSort (const void *elem1, const void *elem2 )
{
   fp *p1, *p2;
   p1 = (fp*) elem1;
   p2 = (fp*) elem2;

   if (*p1 > *p2)
      return 1;
   else if (*p1 < *p2)
      return -1;
   else
      return 0;
}


/************************************************************************************
CLight::CalcSCANLINEINTERP - As an optimization, calculates all
the SCANLINEINTERP for the light ahead of time.

inputs
   PCRenderTraditional     *ppRender - Will initially be passed a pointer to NULL.
                           If this needs to recalculate the light, this will allocate
                           a CRenderTraditional and then fill it into *ppRender.
                           The caller will need to delete this.
                           This minimizes the creation of render objects
                           Can be NULL, in which case won't create
   DWORD             dwPixels - Number of pixels in a scanline
   PCPoint           pBaseLeft - Left at the base location. Should have p[3] = 1.
   PCPoint           pBaseRight - Right value at the base location. Should have p[3] = 1.
   PCPoint           pBasePlusMeterLeft - Base plus a meter depth, at the left. Should have p[3] = 1.
   PCPoint           pBasePlusMeterRight - Right base plus a meter depth. Should have p[3] = 1.
   PCListFixed       plSCANLINEINTERP - List of SCANLINEINTERP that's appended
                     to by this light
returns
   none
*/
void CLight::CalcSCANLINEINTERP (PCRenderTraditional *ppRender, DWORD dwPixels, PCPoint pBaseLeft, PCPoint pBaseRight,
                         PCPoint pBasePlusMeterLeft, PCPoint pBasePlusMeterRight,
                         PCListFixed plSCANLINEINTERP)
{
   if (!RecalcIfDirty(ppRender, NULL))
      return;

   // BUGFIX - If doesn't cast shadow then skip
   if (m_li.fNoShadows)
      return;

   DWORD j;
   SCANLINEINTERP sli;
   CPoint pBaseLeft2, pBaseRight2, pBasePlusMeterLeft2, pBasePlusMeterRight2;
	MALLOCOPT_INIT;
	MALLOCOPT_OKTOMALLOC;
   plSCANLINEINTERP->Required (plSCANLINEINTERP->Num() + 6);
	MALLOCOPT_RESTORE;
   for (j = 0; j < 6; j++) {
      if (!m_apShadowBuf[j])
         continue;

      m_apShadowBuf[j]->ShadowTransform (pBaseLeft, &pBaseLeft2);
      m_apShadowBuf[j]->ShadowTransform (pBaseRight, &pBaseRight2);
      m_apShadowBuf[j]->ShadowTransform (pBasePlusMeterLeft, &pBasePlusMeterLeft2);
      m_apShadowBuf[j]->ShadowTransform (pBasePlusMeterRight, &pBasePlusMeterRight2);

      SCANLINEINTERPCalc (TRUE, dwPixels, &pBaseLeft2, &pBaseRight2,
         &pBasePlusMeterLeft2, &pBasePlusMeterRight2, &sli);
      plSCANLINEINTERP->Add (&sli);
   } // j
}


/************************************************************************************
CLight::CalcIntensity - Given a point in world space (pLoc), this returns the intensity
of light (by RGB) hitting the surface from this light source.

inputs
   PCRenderTraditional     *ppRender - Will initially be passed a pointer to NULL.
                           If this needs to recalculate the light, this will allocate
                           a CRenderTraditional and then fill it into *ppRender.
                           The caller will need to delete this.
                           This minimizes the creation of render objects
                           Can be NULL, in which case won't create
   PCPoint     pLoc - location in world space
   PSCANLINEINTERP paSLI - Array of up to 6 scan-line interps used to track the
               location in the shadow's transformed space. These come from
               CLight::CalcSCANLINEINTERP().
   fp          fZScanLineInterp - Z-depth, if using paSLI
   fp          *pafColor - Areray of 3 fp that are filled with red, green, and blue
               intensities. Of course, if light occluded then fill with 0.
   fp          fMinIntensity - Optimization so that if the returned color won't
               be any brighter than fMinIntensity then ignore. (Max of pafColor[0]..[2])
returns
   BOOL - TRUE if success
*/
BOOL CLight::CalcIntensity (PCRenderTraditional *ppRender, const PCPoint pLoc, PSCANLINEINTERP paSLI, fp fZScanLineInterp,
                            fp *pafColor, fp fMinIntensity)
{
   // fp fComp;
   fp fShadow;
   CPoint pLocShadow;
   fShadow = 1;

   // just to be safe
   DWORD i, j;
   for (i = 0; i < 3; i++)
      pafColor[i] = 0;

   // trey recalc
   if (!RecalcIfDirty(ppRender, NULL))
      return FALSE;

   // BUGFIX - Move calculation of basic intensity in front since will help
   // if multiple lights in a scene
   // take distance into account
   CPoint   pDir;
   fp fDist;
   if (m_li.dwForm != LIFORM_INFINITE) {
      pDir.Subtract (pLoc, &m_li.pLoc);
      fDist = pDir.Length();
      if (fDist > EPSILON)
         fDist = 1.0 / fDist;
      pDir.Scale (fDist);
      fDist *= fDist;
      fDist = min(100,fDist);
   }
   else {
      pDir.Copy (&m_pDirNorm);
      fDist = 1;
   }


   // check against minintensity
   if (fMinIntensity) {
      fp fStrength = (m_li.afLumens[0] + m_li.afLumens[1] + m_li.afLumens[2]) * fDist;
      if (fStrength < fMinIntensity)
         return FALSE;
   }

   // BUGFIX - If doesn't cast shadow then skip
   if (m_li.fNoShadows)
      goto skipshadow;

   // if it's not in the bounding box then fail
   // BUGFIX - If it's infinite, then don't do this text. Cant do for intinite because
   // wont calculate bounding volume properly
   if (m_li.dwForm != LIFORM_INFINITE) for (i = 0; i < 3; i++) {
      if ((pLoc->p[i] < m_apBoundary[0].p[i]) || (pLoc->p[i] > m_apBoundary[1].p[i])) {
         // BUGFIX - So clouds arent' in shadow
         //if (m_li.dwForm == LIFORM_INFINITE)
         //   goto skipshadow;
         return TRUE;   // cut out
      }
   }

   // find out which dwX and dwY of which shadow volume
   fp fBestX, fBestY;
   PCShadowBuf pBestShadow;
   fp fBestDist;
   CPoint ap[2];
   pBestShadow = NULL;
   DWORD dwOptIndex;
   for (j = dwOptIndex = 0; j < 6; j++, dwOptIndex++) {
      if (!m_apShadowBuf[j]) {
         dwOptIndex--;  // since counteract dwOptIndex++ in loop
         continue;
      }

      // bounding volume for this
      m_apShadowBuf[j]->BoundaryGet (&ap[0], &ap[1]);

#if 0 // BUGFIX - Remove this because it's flawed... have to test each and every one
      // outside bounding volume?
      for (i = 0; i < 3; i++) {
         if ((pLoc->p[i] < ap[0].p[i]) || (pLoc->p[i] > ap[1].p[i]))
            break;
      }
      if (i < 3)
         continue;
#endif

      // see if intersects
      fp fX, fY;
      fp fDist;
      if (paSLI) {
         // precalculated
         SCANLINEINTERPFromZ (&paSLI[dwOptIndex], fZScanLineInterp, &pLocShadow);
         if (!m_apShadowBuf[j]->ShadowTest (&pLocShadow, TRUE, &fDist, &fX, &fY))
            continue;
      }
      else {
         // no precalculated info
         if (!m_apShadowBuf[j]->ShadowTest (pLoc, FALSE, &fDist, &fX, &fY))
            continue;
      }

      // else found one
      if (!pBestShadow) {
         fBestX = fX;
         fBestY = fY;
         fBestDist = fDist;
         pBestShadow = m_apShadowBuf[j];
         continue;
      }

      // if there's more than one - which could happen because FOV is slightly
      // more than 90 degrees, find the one with points closest to the center
      fp iDist1, iDist2;
      fp iCenter;
      DWORD dwMax;
      dwMax = m_apShadowBuf[j]->Size();
      
      // special case - if it's an isometric view, AND this is the second one, and
      // we're at least 4 pixels from the edge, then take the second one since it's
      // the spotlight
      if (m_li.dwForm == LIFORM_INFINITE) {
         if ((j == 1) && (fX > 4) && (fY > 4) && (fX+4 < dwMax) && (fY + 4 < dwMax)) {
            iDist1 = 0; // BUGFIX - Sawp
            iDist2 = 1;
         }
         else {
            iDist1 = 1; // prefer large shadow
            iDist2 = 0;
         }
      }
      else {
         // not infinite light source

         iCenter = (fp) m_apShadowBuf[j]->Size() / 2;
         // BUGFIX - Distance calculates relative to distance from edge of screen
         iDist1 = min(iCenter - fabs(fBestX - iCenter), iCenter - fabs(fBestY - iCenter));
         iDist2 = min(iCenter - fabs(fX - iCenter), iCenter - fabs(fY - iCenter));
      }

      if (iDist2 > iDist1) {  // BUGFIX - Was iDist2 < iDist1, but changed meaning of distance
         fBestX = fX;
         fBestY = fY;
         fBestDist = fDist;
         pBestShadow = m_apShadowBuf[j];
      }
   } // j, all light planes

   // if didn't intersect antyhing then done
   if (!pBestShadow) {
      // BUGFIX - So clouds arent' in shadow
      if (m_li.dwForm == LIFORM_INFINITE)
         goto skipshadow;
      return TRUE;
   }

#ifdef FASTSHADOWS
   fp fVal = pBestShadow->PixelInterp (fBestX, fBestY, TRUE);

   // if this is an infinite it will already have had some error added in.
   // however, if it's a local light then add some more error
   if (m_li.dwForm != LIFORM_INFINITE)
      fVal += max(0, fabs(fVal)*.01);// * 5;   // BUGFIX - Added *5 to fix some z-aliasing showing up
   
   if (fVal < fBestDist) {
      // BUGFIX - Allow light from infinite light source to stream through
      // so that textures in shadow aren't quite so bland
      if (m_li.dwForm == LIFORM_INFINITE) {
         fShadow = INTERIORINFINITELIGHTHACK;
         goto skipshadow;
      }

      return TRUE;
   }
   // else, full intensity

#else // not FASTSHADOWS


   // get the max of the chosen point and all surrounding
   fp fMax; // , fMin;
   BOOL fFound;
   int   iSize;
   iSize = (int)pBestShadow->Size();
   fFound = FALSE;
   int ix, iy;
#if 0 // old code
   fp fVal;
   for (ix = (int)dwBestX - 1; ix <= (int)dwBestX + 1; ix++) {
      if ((ix < 0) || (ix >= iSize))
         continue;

      for (iy = (int)dwBestY - 1; iy <= (int)dwBestY + 1; iy++) {
         if ((iy < 0) || (iy >= iSize))
            continue;

         fVal = pBestShadow->Pixel((DWORD)ix, (DWORD)iy);
         if (fFound)
            fMax = max(fVal, fMax);
         else {
            fFound = TRUE;
            fMax = fVal;
         }
      }
   }
#endif // 0
//#define NUMSAMPLES   9  // BUGFIX - Was 7, changed to 9 so would have smoother shadow
//#define NOSHADOWZONE 5  // BUGFIX - Was 3, changed to 5 so round-off error better taken care of
// BUGFIX - Changed back so would be faster, since will eventually have ray trace
#define NOSHADOWZONE 3
#define NUMSAMPLES   (NOSHADOWZONE+2)
#define NOSHADOWZONEOFFSET ((NUMSAMPLES-NOSHADOWZONE)/2)
#define SAMPLEDELTA  .5
#define VARIANCESCALE      0.9   // so account for some error in calcs due to large variance
   fp afSample[NUMSAMPLES][NUMSAMPLES];
   fp fPreCalcCenter = (fp)(NUMSAMPLES-1)/2;
   fp fYCalc, fXCalc;
   fp *pafLine;

#if 0
   fp fVariation;
   fp afVariationStore[NUMSAMPLES*NUMSAMPLES*2];
   DWORD dwVariationCount = 0;
#endif // 0

   fXCalc = fBestX - (fPreCalcCenter + NOSHADOWZONEOFFSET) * SAMPLEDELTA;
   for (ix = 0; ix < NOSHADOWZONE; ix++, fXCalc += SAMPLEDELTA) {
      pafLine = &afSample[ix+NOSHADOWZONEOFFSET][NOSHADOWZONEOFFSET];
      fYCalc = fBestY - (fPreCalcCenter + NOSHADOWZONEOFFSET) * SAMPLEDELTA;
      for (iy = 0; iy < NOSHADOWZONE; iy++, fYCalc += SAMPLEDELTA, pafLine++) {
         *pafLine = pBestShadow->PixelInterp (fXCalc, fYCalc, FALSE);

         if (fFound) {
            fMax = max(*pafLine, fMax);
            // fMin = min(*pafLine, fMin);

            // since max only goes up, and do a later check for (fComp <= fMax)
            // fComp = fBestDist + adjust
            if (fBestDist <= fMax)
               goto skipshadow;
         }
         else {
            fFound = TRUE;
            fMax = /*fMin =*/ *pafLine;
         }

#if 0 // BUGFIX - no more variation calc since cause tree shadows to show through roof
         // calculate variation
         if (ix) {
            // variation from left
            afVariationStore[dwVariationCount] = fabs(pafLine[0] - pafLine[-NUMSAMPLES]);
            dwVariationCount++;
         }
         if (iy) {
            // variation from top
            afVariationStore[dwVariationCount] = fabs(pafLine[0] - pafLine[-1]);
            dwVariationCount++;
         }
#endif // 0
      } // iy
   } // ix

   // NOTE - Had to make 0.01 fuge factor - might be able to make it smaller
   // once I start interpolating points
   fp fAdjust;
   fAdjust = 0.01;

#if 0
   // find median variation over the points
   qsort(afVariationStore, dwVariationCount, sizeof (fp), FPSort);
   fVariation = afVariationStore[dwVariationCount/2];
   fAdjust += fVariation / 2; // so if lots of change in depth then paranoid about variation
#endif // 0

   // if this is an infinite it will already have had some error added in.
   // however, if it's a local light then add some more error
   if (m_li.dwForm != LIFORM_INFINITE)
      fAdjust += max(0, fabs(fBestDist)*.001);// * 5;   // BUGFIX - Added *5 to fix some z-aliasing showing up
   
   // fAdjust += max(.01, fabs(fBestDist)*.001);// * 5;   // BUGFIX - Added *5 to fix some z-aliasing showing up
   // fAdjust *= 5;   // BUGFIX - Compensate for roundoff err so dont see spots in landscape
   // BUGFIX - fAdjust is based on max - min in the noshadows area

   // calculate how much variance there is
   //fp fVariance = max(fMax - fMin, CLOSE);
   //fp fVarianceInv = 1.0 / fVariance;

   //if (fBestDist > fMax + fabs(fMax)*0.01)   // add a small amount for round off error
   fp fComp = fBestDist - fAdjust;
   // fMax -= fAdjust;  // remove error from fMax
   fShadow = 1.0;
   fp f;
   if (fComp <= fMax)
      goto skipshadow;

   // else, some of this is in shadow...

   // find out how many of the surrounding pixels are closer than the point (fBEstDist - fMax)
   // and how many would be closer if fBestDist == fMax + fAdjust
   DWORD dwCloser, dwHyp, dwTotal;
   dwCloser = dwHyp = 0;
   dwTotal = NUMSAMPLES * NUMSAMPLES;

   // calculate the points outside the shadow zone...
   // top and bottom lines
   fXCalc = fBestX - fPreCalcCenter * SAMPLEDELTA;
   pafLine = &afSample[0][0];
   for (ix = 0; ix < NUMSAMPLES; ix++, fXCalc += SAMPLEDELTA) {
      fYCalc = fBestY - fPreCalcCenter * SAMPLEDELTA;
      for (iy = 0; iy < NUMSAMPLES; iy++, fYCalc += SAMPLEDELTA, pafLine++) {
         // need to fill in point if not already
         if ( (ix < NOSHADOWZONEOFFSET) || (ix >= NUMSAMPLES - NOSHADOWZONEOFFSET) ||
            (iy < NOSHADOWZONEOFFSET) || (iy >= NUMSAMPLES - NOSHADOWZONEOFFSET) )
            f = pBestShadow->PixelInterp (fXCalc, fYCalc, FALSE);
         else
            f = *pafLine;

#if 0
         // BUGFIX - squish all the points towards min in order to minimize error...
         // the amount of squish depends upon how much variance there is
         if (f < fMin)
            f += (1.0 * VARIANCESCALE) * fVariance;   // part of the error reduced
         else if (f < fMax) {
            f = (fMax - f) * fVarianceInv;
            f *= VARIANCESCALE;
            f = fMax - f * fVariance;
         }
#endif // 0

         if (f < fComp)
            dwCloser++;
         if (f < fMax)
            dwHyp++;
      } // iy
   } // ix

   if ((dwCloser == dwHyp) || (dwTotal == dwHyp))
      return TRUE;   // no different so all in shadow

   fShadow = 1 - (fp) (dwCloser - dwHyp) / (fp)(dwTotal - dwHyp);
   if (fShadow < 0)
      fShadow = 0;
   else if (fShadow > 1)
      fShadow = 1;
   if (fShadow == 0)
      return TRUE;   // shaded
   // if (fShadow < 1) return TRUE;   // shaded - emphasize shadow
#endif // not FASTSHADOWS

   // amount of sha
skipshadow:
   // else, it's light
   fp fScaleDiffuse, fScaleFront, fScaleBack;
   fScaleDiffuse = 1;
   fScaleFront = fScaleBack = 0;

   if (m_li.afLumens[0] || m_li.afLumens[1]) {
      // what's the intensity?
      fScaleFront = pDir.DotProd (&m_pDirNorm);
      fScaleBack = -fScaleFront;

      if (fScaleFront <= m_afDirectCos[0][0])
         fScaleFront = 0;
      else if (fScaleFront >= m_afDirectCos[0][1])
         fScaleFront = 1;
      else
         fScaleFront = (fScaleFront - m_afDirectCos[0][0]) / (m_afDirectCos[0][1] - m_afDirectCos[0][0]);

      if (fScaleBack <= m_afDirectCos[1][0])
         fScaleBack = 0;
      else if (fScaleBack >= m_afDirectCos[1][1])
         fScaleBack = 1;
      else
         fScaleBack = (fScaleBack - m_afDirectCos[1][0]) / (m_afDirectCos[1][1] - m_afDirectCos[1][0]);

   }

   fp fScaleColor = fDist * fShadow / (fp)0xffff;
   for (j = 0; j < 3; j++) {
      if (!m_li.afLumens[j])
         continue;   // no light

      fp fScaleThis;
      if (j == 2)
         fScaleThis = fScaleDiffuse;
      else if (j == 0)
         fScaleThis = fScaleFront;
      else if (j == 1)
         fScaleThis = fScaleBack;

      for (i = 0; i < 3; i++)
         pafColor[i] += (fp)m_li.awColor[j][i] * m_li.afLumens[j] * fScaleThis;
   } // j
   for (i = 0; i < 3; i++)
      pafColor[i] *= fScaleColor;

   return TRUE;
}


/************************************************************************************
CLight::TestImage - Draws a test shadow image

inputs
   PCZImage     pImage - draw on this image
   DWORD       dwImage - Shadow image, from 0 to 5
*/
void CLight::TestImage (PCImage pImage, DWORD dwImage)
{
   if (!m_apShadowBuf[dwImage])
      return;
   m_apShadowBuf[dwImage]->TestToImage (pImage);
}


/************************************************************************************
CLight::Spotlight - Called by the renderer after it knows what scene is being looked
at by the user. This allows the sun (or other infinitelight) to be spotlighted with
a second more-narrow shadow beam.

NOTE: The SpotLight() call should be called TWICE by the renderer, once with
a large spotlight that covers everything that's seen, and the second with a smaller
spotlight that covers what's being focused on by the viewer.

NOTE: This may ignore the call, relying on what's tere if fLargeSpot==TRUE,
papClipPlane==NULL, and pCenter and fDiameter match the previous call.

inputs
   PCRenderTraditional     *ppRender - Will initially be passed a pointer to NULL.
                           If this needs to recalculate the light, this will allocate
                           a CRenderTraditional and then fill it into *ppRender.
                           The caller will need to delete this.
                           This minimizes the creation of render objects.
                           This can be NULL, but will fail if doesn't exist
   BOOL        fLargeSpot - Set to TRUE if this is the large spotlight, FALSE if its
                  the smaller one.
   PCPoint     pCenter - Center point for the spotlight. If NULL then just clear
   fp          fDiameter - Diameter of the spotlight
   PCPoint     papClipPlane - Array of 15 points, [5][3], for four clipping planes,
               and three points (in world space) defining each plane. The points go clockwise to create
               the normal, which points OUTSIDE the plane. The clipping planes
               should encompass what's visible by the camera. This can be NULL.
   PCProgressSocket  pProgress - Progress
returns
   none
*/
void CLight::Spotlight (PCRenderTraditional *ppRender, BOOL fLargeSpot, PCPoint pCenter, fp fDiameter,
                        PCPoint papClipPlane, PCProgressSocket pProgress)
{
	MALLOCOPT_INIT;
   if ((m_li.dwForm != LIFORM_INFINITE) || m_li.fNoShadows)
      return;

   DWORD dwLight = fLargeSpot ? 0 : 1;

   // 2nd light,[dwLight], is the spotlight
   if (m_apShadowBuf[dwLight]) {
      // if the parameters are the same as used last time and are allowed to keep, then
      // do so
      if (fLargeSpot && !papClipPlane && pCenter && (fDiameter == m_fSpotDiameter) && m_fSpotDiameter) {
         CPoint pDist;
         pDist.Subtract (pCenter, &m_pSpotCenter);
         fp fDist = pDist.Length();
         if (fDist < m_fSpotDiameter / 4.0)
            return;
      }

      delete m_apShadowBuf[dwLight];
   }
   m_apShadowBuf[dwLight] = NULL;

   if (fLargeSpot && !papClipPlane) {
      if (pCenter)
         m_pSpotCenter.Copy (pCenter);
      else
         m_pSpotCenter.Zero();
      m_fSpotDiameter = fDiameter;
   }
   else if (fLargeSpot)
      m_fSpotDiameter = 0.0;  // to indicate bad

   // if no center then done since just want to clear
   if (!pCenter)
      return;

   // figure out front, right, and up
   CPoint pX, pY, pZ;
   pY.Copy (&m_pDirNorm);
   pZ.Zero();
   pZ.p[2] = 1;
   pX.CrossProd (&pY, &pZ);
   if (pX.Length() < CLOSE) {
      pZ.Zero();
      pZ.p[0] = 1;
      pX.CrossProd (&pY, &pZ);
      if (pX.Length() < CLOSE)
         return;
   }
   pX.Normalize();
   pZ.CrossProd (&pX, &pY);
   pZ.Normalize();

   // if doing the sun then zoom in as much as possible
   CPoint pEye;
   fp fScale;

   // flat
   pEye.Copy (pCenter);
   fScale = fDiameter;

   // if get here will need ppRender
   if (!ppRender)
      return;

   // create all the panels
   BOOL fBoundary;
   fBoundary = FALSE;
   m_apBoundary[0].Zero();
   m_apBoundary[1].Zero();
   DWORD i;
   for (i = 0; i < 6; i++) {
      // only create panel 1, which we know is freed
      if (i == dwLight) {
         MALLOCOPT_OKTOMALLOC;
         m_apShadowBuf[i] = new CShadowBuf;
         MALLOCOPT_RESTORE;
         if (!m_apShadowBuf[i])
            return;

         // which direction is front, etc.
         CPoint pFront, pUp;
         pUp.Copy (&pZ);
         pFront.Copy (&pY);

         // BUGFIX - adjust the pixels by the texture detail
         DWORD dwPixels = ((fLargeSpot && !papClipPlane && pCenter && fDiameter) ? SIZEINFINITEONCAMERA : SIZEINFINITE); // BUGFIX - extra-detailed if using spotlight
         int iDetail = TextureDetailGet(m_dwRenderShard);
         dwPixels = (DWORD)((fp)dwPixels * pow(2.0, (fp)iDetail/2.0) + 0.5);
         //if (iDetail > 0)
         //   dwPixels <<= (iDetail/2);  // BUGFIX - Detail doesn't affect shadows as much
         //else if (iDetail < 0)
         //   dwPixels >>= ((-iDetail)/2);  // BUGFIX - Detail doesn't affect shadows as much

         if (!*ppRender) {
            *ppRender = new CRenderTraditional(m_dwRenderShard);
            if (!*ppRender)
               return;
            (*ppRender)->m_iPriorityIncrease = m_iPriorityIncrease;
         }

            // just pass in m_fLightMaximum since infinite lights are full strength
         if (!m_apShadowBuf[i]->Init (*ppRender, &pEye, &pFront, &pUp, m_pWorld, fScale, m_dwShowFlags,
            dwPixels, m_fFinalRender, m_dwShowOnly, papClipPlane, m_fLightMaximum, pProgress)) {
               // error
               for (i = 0; i < 6; i++)
                  if (m_apShadowBuf[i]) {
                     delete m_apShadowBuf[i];
                     m_apShadowBuf[i] = NULL;
                  }
               return;
            }
      } // if match light

      if (!m_apShadowBuf[i])
         continue;

      // get the boundary
      CPoint   ab[2];
      m_apShadowBuf[i]->BoundaryGet (&ab[0], &ab[1]);
      if (!ab[0].AreClose(&ab[1])) {
         if (fBoundary) {
            m_apBoundary[0].Min (&ab[0]);
            m_apBoundary[1].Max (&ab[1]);
         }
         else {
            m_apBoundary[0].Copy (&ab[0]);
            m_apBoundary[1].Copy (&ab[1]);
            fBoundary = TRUE;
         }
      }
   } // i

   // done
}

// FUTURERELEASE - Lights that dont cast shadows - for faster drawing, especially if hve
// small lights

// NOT REPRO - View Eagle Eye from above - with quick view. Set time to noon.
// Get problem with some stuff being in shadow that shouldn't be.