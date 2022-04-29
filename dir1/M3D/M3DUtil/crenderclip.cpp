/************************************************************************
CRenderClip.cpp - Does clipping for the renderer.
*/

#include <windows.h>
#include <math.h>
#include "escarpment.h"
#include "..\M3D.h"


typedef struct {
   fp      aMult[5];      // if aMult[0]*x + aMult[1]*y + aMult[2]*z + aMult[3]*w + aMult[4] >= 0 then clip
   CPoint      pPoint;        // original point
   CPoint      pNormal;       // orignal normal
} CLIPPLANE, *PCLIPPLANE;



/********************************************************************************
CRenderClip::Constructor and destructor.
*/
CRenderClip::CRenderClip (void)
{
   m_listCLIPPLANE.Init(sizeof(CLIPPLANE));
}

CRenderClip::~CRenderClip (void)
{
   // nothing for now
}


/********************************************************************************
CRenderClip::CloneTo - Clones this clip information to another copy

inputs
   CRenderClip    *pTo - Clone to
returns
   BOOL - TRUE if success
*/
BOOL CRenderClip::CloneTo (CRenderClip *pTo)
{
   pTo->m_listCLIPPLANE.Init (sizeof(CLIPPLANE), m_listCLIPPLANE.Get(0), m_listCLIPPLANE.Num());

   return TRUE;
}

/********************************************************************************
CRenderClip::Clear - Clears out the existing clipping planes
*/
void CRenderClip::Clear (void)
{
   m_listCLIPPLANE.Clear();
}

/********************************************************************************
CRenderClip::AddPlane - This adds a new cip plane onto the list.

inputs
   PCPoint     pNormal - Normal to the plane, pointing in the direction that is to
                        be clipped. This is a 4-dimensional normal, with W included,
                        so you might want w=0 if the perspective plane is not
                        important. NOTE: Make sure that W is 0 if it's not needed.
                        The points are post-perspective (before W is divided.) You
                        may need to TransformNormal() to get the right values.
   PCPoint     pPoint - A point on the plane. 4-dimensional also, so W may be 0.
                        You may need to Transform() to get this to the right values.
returns
   BOOL - TRUE if success. The system can only handle 32 planes.
*/
BOOL CRenderClip::AddPlane (PCPoint pNormal, PCPoint pPoint)
{
   if (m_listCLIPPLANE.Num() >= 31)
      return FALSE;

   CLIPPLANE cp;
   memset (&cp, 0, sizeof(cp));
   cp.pPoint.Copy(pPoint);
   cp.pNormal.Copy(pNormal);

   cp.aMult[0] = pNormal->p[0];
   cp.aMult[1] = pNormal->p[1];
   cp.aMult[2] = pNormal->p[2];
   cp.aMult[3] = pNormal->p[3];
   cp.aMult[4] = -(pPoint->p[0] * cp.aMult[0] + pPoint->p[1] * cp.aMult[1] +
      pPoint->p[2] * cp.aMult[2] + pPoint->p[3] * cp.aMult[3]);

   cp.aMult[4] += EPSILON;
   // BUGFIX: Adding EPSILON to take care of drawing a teapot with base at z=0
   // and then clipping at z=0 plane. Sometime had roundoff error

	MALLOCOPT_INIT;
	MALLOCOPT_OKTOMALLOC;
   m_listCLIPPLANE.Add (&cp);
	MALLOCOPT_RESTORE;

   return TRUE;
}


/********************************************************************************
CRenderClip::CalcClipBits - Loops through an array of points and sets a DWORD
for each point indicating if it should be clipped (TRUE) or not (FALSE). The bits
in the DWORD correspond to the clipping planes (bit 0 = first plane added, 1 = 2nd, etc.)
so that the system can know what lines to clip this against.

inputs
   DWORD       dwClipMask - Set to 0xffffffff to test against all known planes.
                        To test against only a few, have a bit-field set, bit 0 is
                        the 1st plane added, 1 is the 2nd plane, etc.
   DWORD       dwNum - Number of points to check
   PCPoint     apPoints - Pointer to an array of dwNum points. These should have had
                        all the transforms applied, including perspective. Except,
                        the final divide by W and rescaling to the screen should
                        NOT have been applied.
   DWORD       *padwBits - Caller should supply enough space for dwNum*sizeof(DWORD).
                        This is filled in with a bit field for which planes the point
                        is on the clipping side of.
returns
   none
*/

void CRenderClip::CalcClipBits (DWORD dwClipMask, DWORD dwNum, PCPoint apPoints, DWORD *padwBits)
{
   // make sure mask doens't include planes we don't have
   dwClipMask &= ((1 << m_listCLIPPLANE.Num()) - 1);

   // calculate max loop that need to get to
   DWORD dwMax, dwTemp;
   for (dwMax = 0, dwTemp = dwClipMask; dwTemp; dwMax++, dwTemp >>= 1);

   PCLIPPLANE pc, pc2;
   pc = (PCLIPPLANE) m_listCLIPPLANE.Get(0);

   // loop
   DWORD i, j;
   DWORD dwClip, *pdwClip;
   PCPoint p;
   for (i = 0, p = apPoints, pdwClip =padwBits; i < dwNum; i++, p++, pdwClip++) {
      dwClip = 0;

      // loop through all the clipping planes
      for (j = 0; j < dwMax; j++) {
         // if dont want to check this then continue
         if (!(dwClipMask & (1 << j)))
            continue;
         pc2 = pc + j;

         // compare
         if ((pc2->aMult[0] * p->p[0] + pc2->aMult[1] * p->p[1] + pc2->aMult[2] * p->p[2] +
            pc2->aMult[3] * p->p[3] + pc2->aMult[4]) >= 0)
            dwClip |= (1 << j);
      }

      // store the clip away
      *pdwClip = dwClip;
   }
}


/********************************************************************************
CRenderClip::ClipLine - This clips a line to a clipping plane. The line
is is known to have one point on the other side of the clipping plane.
To calculate that call CalcClipBits on the points of the line.

NOTE: We know only one point gets clipped by the plane because:
   a) If none did this wouldn't get called.
   b) If both did the line would be entirely clipped.

inputs
   DWORD       dwClipPlane - Clipping plane.
   PCPoint     pNC - Point that is not to be clipped.
   PCPoint     pNormalNC - Non-clipped normal. THis can be NULL
   WORD        *pwColorNC - RGB color for the vertex. If this is NULL then no color work is done
   PTEXTPOINT5 pTextureNC - Non-clipped texture. Can be NULL.
   PCPoint     pC - Point that is to be clipped. This is modified to contain the new
                  clipped value
   PCPoint     pNormalC - Clipped normal. This is normalized if interpolated.
   WORD        *pwColorC - RGB color for the vertex. Modified to the new color at the vertex.
                  Can be NULL.
   PTEXTPOINT5 pTextureC - Clipped texture. Can be NULL.
returns
   none
*/
void CRenderClip::ClipLine (DWORD dwClipPlane,
                            const PCPoint pNC, const PCPoint pNormalNC, const WORD *pwColorNC,
                            const PTEXTPOINT5 pTextureNC,
                            PCPoint pC, PCPoint pNormalC, WORD *pwColorC,
                            PTEXTPOINT5 pTextureC)
{
   PCLIPPLANE pc;
   pc = (PCLIPPLANE) m_listCLIPPLANE.Get(dwClipPlane);

   // find the longest dimension between pNC and pC
   CPoint   pSub;
   DWORD i;
   pSub.Subtract4 (pC, pNC);

   // find out where it crosses the plane on the dwLongest dimension
   // solve alpha = ((P-Q).v) / (w.v), P=point on plalne, Q=pNC, w=pC-pNC, v=Plane normal
   CPoint pPQ;
   pPQ.Subtract4 (&pc->pPoint, pNC);
   fp fAlpha;
   fAlpha = pPQ.DotProd4(&pc->pNormal) / pSub.DotProd4(&pc->pNormal);

   // new point is a combination of the three
   fp   *p1, *p2, *p3;
   p1 = &pC->p[0];
   p2 = &pNC->p[0];
   p3 = &pSub.p[0];
   for (i = 0; i < 4; i++)
      *(p1++) = *(p2++) + fAlpha * *(p3++);

   // do the normal interpolation if it's there
   if (pNormalC) {
      pSub.Subtract4 (pNormalC, pNormalNC);
      p1 = &pNormalC->p[0];
      p2 = &pNormalNC->p[0];
      p3 = &pSub.p[0];  // was commented out. put back in because I think needs to be
      for (i = 0; i < 4; i++)
         *(p1++) = *(p2++) + fAlpha * *(p3++);
   }

   // clip the texture map
   if (pTextureC) {
      fp fDelta;
      for (i = 0; i < 2; i++) {
         fDelta = pTextureC->hv[i] - pTextureNC->hv[i];
         pTextureC->hv[i] = pTextureNC->hv[i] + fAlpha * fDelta;
      }
      for (i = 0; i < 3; i++) {
         fDelta = pTextureC->xyz[i] - pTextureNC->xyz[i];
         pTextureC->xyz[i] = pTextureNC->xyz[i] + fAlpha * fDelta;
      }
#if 0 // old code for only 2d textures
      for (i = 0; i < 2; i++) {
         fStart = i ? pTextureNC->v : pTextureNC->h;
         fDelta = (i ? pTextureC->v : pTextureC->h) - fStart;

         fStart = fStart + fAlpha * fDelta;

         if (i)
            pTextureC->v = fStart;
         else
            pTextureC->h = fStart;
      }
#endif // 0

   }

   // do the colors if interpolate
   if (pwColorC) {
      DWORD  dwScale, dwScaleInv;
      dwScale = (DWORD)(fAlpha * 0x0ffff);
      dwScaleInv = 0xffff - dwScale;
      for (i = 0; i < 3; i++)
         pwColorC[i] = (WORD)((pwColorNC[i] * dwScale + pwColorC[i] * dwScaleInv) >> 16);
   }

   // done
}

/********************************************************************************
CRenderClip::ClipPolygonOneSide - Takes a polygon (list of PCLIPPOLYPOINT) and
clips it to ONE clip plane. The new polygon is written into pDest.

inputs
   DWORD       dwClipPlane - Plane to clip against. As a general rule, this should be
               one of the planes which definitely needs to get clipped for the polygon.
   BOOL        fColor - Set to TRUE if the colors in pSrc (and hence pDest) are valid.
               FALSE if the colors should not be interpolated
   BOOL        fNormal - Set to TRUE if the normal need to be interpolated.
   BOOL        fTexture - Set to TRUE if the texture needs to be interpolated
   DWORD       dwNumSrc - Number of points in the original polygon
   PCLIPPOLYPOINT pSrc - Pointer to an array of points of the polygon.
   DWORD       *pdwNumDest - Filled with the number of points in the clipped polygon.
   PCLIPPOLY   pDest - Filled with the clipped polygon. Have enough room for at least
               2 * dwNumSrc points.
returns
   BOOL - TRUE parts of it were clipped, FALSE if nothing clipped
*/
BOOL CRenderClip::ClipPolygonOneSide (DWORD dwClipPlane, BOOL fColor, BOOL fNormal,
                                      BOOL fTexture, DWORD dwNumSrc, PCLIPPOLYPOINT pSrc,
                                       DWORD *pdwNumDest, PCLIPPOLYPOINT pDest)
{
   // convert the plane to a bit field
   DWORD dwBit = (1 << dwClipPlane);

   // loop until find the first point that is not clipped
   DWORD dwFirst;
   for (dwFirst = 0; dwFirst < dwNumSrc; dwFirst++)
      if (!(pSrc[dwFirst].dwClipBits & dwBit))
         break;
   if (dwFirst >= dwNumSrc) {
      // we couldnt actually find a point that wasn't clipped, so the entire polygion is clipped
      *pdwNumDest = 0;
      return TRUE;
   }

   // add this point
   DWORD dwSrc, dwDest, dwNext, i;
   pDest[0] = pSrc[dwFirst];
   dwDest = 1;

   BOOL fClipped;
   fClipped = FALSE;

   // loop, from first point that isn't clipped
   for (i = 0; i < dwNumSrc; i++) {
      BOOL  fSrcClipped, fNextClipped;
      dwSrc = (i + dwFirst) % dwNumSrc;
      dwNext = (dwSrc+1) % dwNumSrc;
      fSrcClipped = (pSrc[dwSrc].dwClipBits & dwBit);
      fNextClipped = (pSrc[dwNext].dwClipBits & dwBit);

      // NOTE: If get to the last point and it isn't clipped then just exit because
      // it was already added the time before. If it is clipped, we may need to add
      // a point between this and the first point
      if ((i+1 == dwNumSrc) && !fSrcClipped)
         continue;

      // if we get to the last point, and get here, then it's clipped.
      // because the first point is always not-clipped, then we know we have
      // a border to stick in - see below

      // if the source is not clipped and the next one isn't then just add the source
      if (!fSrcClipped && !fNextClipped) {
         pDest[dwDest] = pSrc[dwNext];
         dwDest++;
         continue;
      }

      // if the source is clipped and the next one isn't then add the clip
      if (fSrcClipped && !fNextClipped) {
         // the next one is clipped, so figure out where
         CLIPPOLYPOINT cpp;
         cpp = pSrc[dwSrc];
         ClipLine (dwClipPlane,
            &pSrc[dwNext].Point,
            fNormal ? &pSrc[dwNext].Normal : NULL,
            fColor ? &pSrc[dwNext].wColor[0] : NULL,
            fTexture ? &pSrc[dwNext].Texture : NULL,
            &cpp.Point,
            fNormal ? &cpp.Normal : NULL,
            fColor ? &cpp.wColor[0] : NULL,
            fTexture ? &cpp.Texture : NULL);
         cpp.dwOrigIndex = (DWORD)-1;
      
         // since clipped this, need to recompared against all the other bit planes
         CalcClipBits ((pSrc[dwNext].dwClipBits | cpp.dwClipBits) & (~dwBit),
            1, &cpp.Point, &cpp.dwClipBits);

         // add this
         pDest[dwDest] = cpp;
         dwDest++;

         // don't add the extra point if we're at the last point because if
         // we're at the least point, and it's clipped, then the next point is
         // the first point (which was already added), so don't add again
         if (i+1 != dwNumSrc) {
            // and add the next point since always assume at the beginning of the
            // loop that the current point has been added
            pDest[dwDest] = pSrc[dwNext];
            dwDest++;
         }
         fClipped = TRUE;
         continue;
      }

      // if the point is not clipped but the next one is then clip
      if (!fSrcClipped && fNextClipped) {
         // the next one is clipped, so figure out where
         CLIPPOLYPOINT cpp;
         cpp = pSrc[dwNext];
         ClipLine (dwClipPlane,
            &pSrc[dwSrc].Point,
            fNormal ? &pSrc[dwSrc].Normal : NULL,
            fColor ? &pSrc[dwSrc].wColor[0] : NULL,
            fTexture ? &pSrc[dwSrc].Texture : NULL,
            &cpp.Point,
            fNormal ? &cpp.Normal : NULL,
            fColor ? &cpp.wColor[0] : NULL,
            fTexture ? &cpp.Texture : NULL);
         cpp.dwOrigIndex = (DWORD)-1;
      
         // since clipped this, need to recompared against all the other bit planes
         CalcClipBits ((pSrc[dwSrc].dwClipBits | cpp.dwClipBits) & (~dwBit),
            1, &cpp.Point, &cpp.dwClipBits);

         // add this
         pDest[dwDest] = cpp;
         dwDest++;
         fClipped = TRUE;
         continue;
      }

      // if it gets here then both the current point and next one are clipped so
      // dont add anything
      fClipped = TRUE;
      continue;
                  
   }

   *pdwNumDest = dwDest;
   return fClipped;
}


/********************************************************************************
CRenderClip::ClipPolygon - Takes a polygon (list of PCLIPPOLYPOINT) and
clips it to to all clipping planes that ir crosses. The new polygon is written into pDest.
NOTE: Don't call this if there's no need to clip since it will just slow things down.

inputs
   BOOL        fColor - Set to TRUE if the colors in pSrc (and hence pDest) are valid.
               FALSE if the colors should not be interpolated
   BOOL        fNormal - Set to TRUE if the normal need to be interpolated.
   BOOL        fTexture - Set to TRUE if the textures need to be interpolated.
   DWORD       dwNumSrc - Number of points in the original polygon
   PCLIPPOLYPOINT pSrc - Pointer to an array of points of the polygon.
   DWORD       *pdwNumDest - Filled with the number of points in the clipped polygon.
   PCLIPPOLY   pDest - Filled with the clipped polygon. Have enough room for at least
               4 * dwNumSrc points.
returns
   BOOL - TRUE if any part of the polygon has been clipped, FALSE if none has been clipped
*/
BOOL CRenderClip::ClipPolygon (BOOL fColor, BOOL fNormal, BOOL fTexture,
                               DWORD dwNumSrc, PCLIPPOLYPOINT pSrc,
                               DWORD *pdwNumDest, PCLIPPOLYPOINT pDest)
{
   // BUGFIX - Changed maxclip from 50 to 500
// #define     MAXCLIP     500
   // CLIPPOLYPOINT  aCPP[MAXCLIP*4];
   //if (dwNumSrc > MAXCLIP) {
   //   // too many points so exit
   //   *pdwNumDest = 0;
   //   return TRUE;
   //}

   // remember where the good working version is
   DWORD dwWorking; // 0 -> in aCPP, 1 -> in pSrc, 2->pDest
   DWORD dwWorkingNum;  // number of points in the working location
   PCLIPPOLYPOINT pWork;
   dwWorking = 1;
   dwWorkingNum = dwNumSrc;
   pWork = pSrc;

   BOOL fClipped;
   fClipped = FALSE;

   // find out which points need to clip against
   DWORD dwAnd, dwOr, i;
   while (TRUE) {
      dwAnd = 0xffffffff;
      dwOr = 0;
      for (i = 0; i < dwWorkingNum; i++) {
         dwAnd &= pWork[i].dwClipBits;
         dwOr |= pWork[i].dwClipBits;
      }

      // if there are any bits with everything, trivial. Just clip it all
      if (dwAnd) {
         *pdwNumDest = 0;
         return TRUE;
      }

      // if there's nothing left then just copy to the destination and return
      if (!dwOr) {
         if (dwWorking != 2)
            memcpy (pDest, pWork, dwWorkingNum * sizeof(CLIPPOLYPOINT));
         *pdwNumDest = dwWorkingNum;
         return fClipped;
      }

      // else, find the lowest bit that's set and clip against that
      DWORD dwLowest;
      for (dwLowest = 0; !((1 << dwLowest) & dwOr); dwLowest++);

      DWORD dwTo, dwToNum;
      PCLIPPOLYPOINT pTo;
      switch (dwWorking) {
      case 0:  // aCPP, intneral
      case 1:  // pSrc - source
         dwTo = 2;
         pTo = pDest;
         break;
      case 2:  // pDest - destination
         dwTo = 0;
         if (!m_memClipPolygonCPP.Required (2 * dwWorkingNum * sizeof(CLIPPOLYPOINT) )) {
            *pdwNumDest = 0;
            return TRUE;
         }
         pTo = (PCLIPPOLYPOINT)m_memClipPolygonCPP.p; // &aCPP[0];
         break;
      }

      // clip
      fClipped |= ClipPolygonOneSide (dwLowest, fColor, fNormal, fTexture,
         dwWorkingNum, pWork, &dwToNum, pTo);
      
      // start all over again
      dwWorkingNum = dwToNum;
      dwWorking = dwTo;
      pWork = pTo;
   }
}



/********************************************************************************
CRenderClip::ClipPolygons - Takes a PPOLYRENDERINFO structure with all transformed
points.

This:
   1) Goes through each point and sees if it needs to be clipped, calculating clipping bits.
   2) It then or's all the values together to find what planes need to clip against,
      and and's all the values together to find out if the object is trivially clipped.
   3) If it's trivially clipped then set pInfo->dwNumPolygons to 0, same with colors etc.
      and done with it. If trivially accept all then just return.
   4) Loops through all the polygons. For each polygon it gets the points associated
      and or's and and's. Same trivial clip, or tivial accept. If trivial accept then
      copy the polygon to the new memory block.
   5) Else, need to clip. Clip the polygon against the clipping planes it crosses
      and add it to the new list.

NOTE: ClipPolygons may remap pinfo's pointers to some internal memory. That means that
pInfo is valid until the next time ClipPolygons is called.

inputs
   DWORD       dwClipMask - Clipping Mask. Use -1 for all planes
   PPOLYRENDERINFO pInfo - Initially filled with the polygon rendering information
               that's clipped against. THIS MAY BE MODIFIED by the ClipPolygons
               function. NOTE: All the points and normals must be have all transfomrations
               applied, including perspective. (Except for final divide by W and inverse scaling.)
returns
   BOOL - TRUE if any of the polygons were clipped, FALSE if untouched
*/
BOOL CRenderClip::ClipPolygons (DWORD dwClipMask, PPOLYRENDERINFO pInfo)
{
   BOOL fClipped = FALSE;

   // go through all the points and see if they need to be clipped, simultaenously
   // working out a point re-map
   m_MemRemap.Required (pInfo->dwNumPoints * sizeof(DWORD));
   m_MemClipBits.Required (pInfo->dwNumPoints * sizeof(DWORD));
   DWORD *pdwRemap = (DWORD*) m_MemRemap.p;
   DWORD *pdwClipBits = (DWORD*) m_MemClipBits.p;
   CalcClipBits (dwClipMask, pInfo->dwNumPoints, pInfo->paPoints, pdwClipBits);
   DWORD i, dwAnd, dwOr, dwCurRemap;
   dwAnd = (DWORD)-1;
   dwOr = 0;
   dwCurRemap = 0;
   for (i = 0; i < pInfo->dwNumPoints; i++) {
      dwAnd &= pdwClipBits[i];
      dwOr |= pdwClipBits[i];
   }

   // do trivial reject and accept
   if (!dwOr)
      return FALSE;  // all in
   if (dwAnd) {
      // trivial reject
      pInfo->dwNumColors = 0;
      pInfo->dwNumNormals = 0;
      pInfo->dwNumTextures = 0;
      pInfo->dwNumPoints = 0;
      pInfo->dwNumPolygons = 0;
      pInfo->dwNumSurfaces = 0;
      pInfo->dwNumVertices = 0;
      return TRUE;
   }

   // work on remap so know what an original point gets remapped to
   // Don't bother with remaps for other since a) it's more difficult to tell,
   // and b) the wasted processing isn't that much
   m_MemPoints.Required (pInfo->dwNumPoints * sizeof(CPoint));
   for (i = 0; i < pInfo->dwNumPoints; i++) {
      // is it clipped
      if (pdwClipBits[i]) {
         pdwRemap[i] = (DWORD) -1;
      }
      else {
         // not clipped, so copy over
         ((PCPoint) m_MemPoints.p)[dwCurRemap].Copy (pInfo->paPoints + i);
         pdwRemap[i] = dwCurRemap;
         dwCurRemap++;
      }
   }
   m_MemPoints.m_dwCurPosn = dwCurRemap * sizeof(CPoint);

   // clear out all the temporary memories and copy stuff from pInfo in
   DWORD dwSize;

   m_MemNormals.Required (dwSize = pInfo->dwNumNormals * sizeof(CPoint));
   memcpy (m_MemNormals.p, pInfo->paNormals, dwSize);
   m_MemNormals.m_dwCurPosn = dwSize;

   m_MemTextures.Required (dwSize = pInfo->dwNumTextures * sizeof(TEXTPOINT5));
   memcpy (m_MemTextures.p, pInfo->paTextures, dwSize);
   m_MemTextures.m_dwCurPosn = dwSize;

   m_MemColors.Required (dwSize = pInfo->dwNumColors * sizeof(COLORREF));
   memcpy (m_MemColors.p, pInfo->paColors, dwSize);
   m_MemColors.m_dwCurPosn = dwSize;

   m_MemVertices.Required (dwSize = pInfo->dwNumVertices * sizeof(VERTEX));
   memcpy (m_MemVertices.p, pInfo->paVertices, dwSize);
   m_MemVertices.m_dwCurPosn = dwSize;

   m_MemPolygons.m_dwCurPosn = 0;

   // go through the new vertices and remap points
   PVERTEX pv;
   pv = (PVERTEX) m_MemVertices.p;
   for (i = 0; i < pInfo->dwNumVertices; i++) {
      pv[i].dwPoint = pdwRemap[pv[i].dwPoint];

      // if this vertex disappeared then just set its point to 0
      if (pv[i].dwPoint == (DWORD)-1)
         pv[i].dwPoint = 0;
   }

   DWORD dwNewPoly;
   dwNewPoly = 0;

   // loop through all the polygons
   DWORD dwPoly;
   PPOLYDESCRIPT pOrig;
   for (dwPoly = 0, pOrig = pInfo->paPolygons; dwPoly < pInfo->dwNumPolygons; dwPoly++, pOrig = (PPOLYDESCRIPT)((PBYTE)pOrig + (sizeof(POLYDESCRIPT)+pOrig->wNumVertices*sizeof(DWORD)))) {
      // see this clip status
      dwAnd = (DWORD)-1;
      dwOr = 0;
      DWORD *pdw;
      pdw = (DWORD*) (pOrig + 1);
      for (i = 0; i < (DWORD)pOrig->wNumVertices; i++,pdw++) {
         DWORD dwPoint;
         dwPoint = pInfo->paVertices[*pdw].dwPoint;
         dwAnd &= pdwClipBits[dwPoint];
         dwOr |= pdwClipBits[dwPoint];
      }

      // if trivial reject continue
      if (dwAnd) {
         fClipped = TRUE;
         continue;
      }

      // if trivial accept then allocate all the memory and copy it over
      if (!dwOr) {
         DWORD dwNeed;
         PVOID pNew;
         dwNeed = sizeof(POLYDESCRIPT) + pOrig->wNumVertices * sizeof(DWORD);
         pNew = MemMalloc (&m_MemPolygons, dwNeed);
         if (pNew) {
            memcpy (pNew, pOrig, dwNeed);
            dwNewPoly++;
            continue;
         }
      }

      // if get here, there's some clipping to be done, so convert to a form
      // that can be passed into the polygon clipper
      // CLIPPOLYPOINT pSrc[MAXCLIP], pDest[MAXCLIP*4];
      DWORD dwNewNum;
      //if (pOrig->wNumVertices > MAXCLIP) {
      //   fClipped = TRUE;
      //   continue;   // cant handle that many polygons
      //}
      BOOL fNormals, fColors, fTextures;
      fNormals = fColors = TRUE;
      fTextures = FALSE;

      // dont bother with textures unless texture map
      if (pOrig->dwSurface < pInfo->dwNumSurfaces) {
         fTextures = pInfo->paSurfaces[pOrig->dwSurface].fUseTextureMap;
      }

      if (!m_memClipPolygonsSrc.Required ((DWORD)pOrig->wNumVertices * sizeof(CLIPPOLYPOINT))) {
         fClipped = TRUE;
         continue;   // can't handle that many polygons
      }
      if (!m_memClipPolygonsDest.Required ((DWORD)pOrig->wNumVertices * 4 * sizeof(CLIPPOLYPOINT))) {
         fClipped = TRUE;
         continue;   // can't handle that many polygons
      }
      PCLIPPOLYPOINT pSrc = (PCLIPPOLYPOINT) m_memClipPolygonsSrc.p;
      PCLIPPOLYPOINT pDest = (PCLIPPOLYPOINT) m_memClipPolygonsDest.p;

      pdw = (DWORD*) (pOrig+1);
      for (i = 0; i < (DWORD) pOrig->wNumVertices; i++) {
         pv = pInfo->paVertices + pdw[i];
         pSrc[i].dwClipBits = pdwClipBits[pv->dwPoint];
         pSrc[i].dwOrigIndex = pdw[i];
         if (pv->dwNormal < pInfo->dwNumNormals)
            pSrc[i].Normal.Copy (pInfo->paNormals + pv->dwNormal);
         else
            fNormals = FALSE;

         if (fTextures && (pv->dwTexture < pInfo->dwNumTextures))
            pSrc[i].Texture = pInfo->paTextures[pv->dwTexture];
         else
            fTextures = FALSE;
         pSrc[i].Point.Copy (pInfo->paPoints + pv->dwPoint);

         if (pv->dwColor < pInfo->dwNumColors)
            m_TempImage.Gamma (pInfo->paColors[pv->dwColor], &pSrc[i].wColor[0]);
         else
            fColors = FALSE;
      }

      // clip
      fClipped |= ClipPolygon (fColors, fNormals, fTextures, (DWORD)pOrig->wNumVertices, pSrc, &dwNewNum, pDest);

      // if nothing left then continue
      if (!dwNewNum)
         continue;   // no polygon anyway

      // else, add the polygon
      PPOLYDESCRIPT pNewPoly;
      pNewPoly = (PPOLYDESCRIPT) MemMalloc (&m_MemPolygons, sizeof(POLYDESCRIPT) + dwNewNum * sizeof(DWORD));
      if (!pNewPoly)
         continue;
      dwNewPoly++;
      *pNewPoly = *pOrig;
      pNewPoly->wNumVertices = (WORD) dwNewNum;

      pdw = (DWORD*) (pNewPoly+1);
      for (i = 0; i < dwNewNum; i++, pdw++) {
         // if this point is the same as before then just copy the vertex number
         // over
         if (pDest[i].dwOrigIndex != (DWORD)-1) {
            *pdw = pDest[i].dwOrigIndex;
            continue;
         }

         // else, a new vertex, new point, new normal, new color are needed
         DWORD dwVertex;
         dwVertex = (DWORD)m_MemVertices.m_dwCurPosn / sizeof(VERTEX);
         pv = (PVERTEX) MemMalloc (&m_MemVertices, sizeof(VERTEX));
         *pdw = dwVertex;
         pv->dwColor = pv->dwNormal = pv->dwTexture = 0;

         // allocate a new point
         pv->dwPoint = (DWORD)m_MemPoints.m_dwCurPosn / sizeof(CPoint);
         PCPoint pNewPoint;
         pNewPoint = (PCPoint) MemMalloc (&m_MemPoints, sizeof(CPoint));
         pNewPoint->Copy (&pDest[i].Point);

         // allocate normal if using normals
         if (fNormals) {
            pv->dwNormal = (DWORD)m_MemNormals.m_dwCurPosn / sizeof(CPoint);
            PCPoint pNewPoint;
            pNewPoint = (PCPoint) MemMalloc (&m_MemNormals, sizeof(CPoint));
            pNewPoint->Copy (&pDest[i].Normal);
         }

         // allocate texture if using textures
         if (fTextures) {
            pv->dwTexture = (DWORD)m_MemTextures.m_dwCurPosn / sizeof(TEXTPOINT5);
            PTEXTPOINT5 pNewPoint;
            pNewPoint = (PTEXTPOINT5) MemMalloc (&m_MemTextures, sizeof(TEXTPOINT5));
            *pNewPoint = pDest[i].Texture;
         }

         // allocate colors if using colors
         if (fColors) {
            pv->dwColor = (DWORD)m_MemColors.m_dwCurPosn / sizeof(COLORREF);
            COLORREF *pc;
            pc = (COLORREF*) MemMalloc (&m_MemColors, sizeof(COLORREF));
            *pc = m_TempImage.UnGamma (&pDest[i].wColor[0]);
         }
      }  // loop over new polygon vertices
   }  // loop over polygons


   // set the pInfo to the new memory locations
   pInfo->dwNumPoints = (DWORD)m_MemPoints.m_dwCurPosn / sizeof(CPoint);
   pInfo->paPoints = (PCPoint) m_MemPoints.p;
   pInfo->dwNumNormals = (DWORD)m_MemNormals.m_dwCurPosn / sizeof(CPoint);
   pInfo->paNormals = (PCPoint) m_MemNormals.p;
   pInfo->dwNumTextures = (DWORD)m_MemTextures.m_dwCurPosn / sizeof(TEXTPOINT5);
   pInfo->paTextures = (PTEXTPOINT5) m_MemTextures.p;
   pInfo->dwNumColors = (DWORD)m_MemColors.m_dwCurPosn / sizeof(COLORREF);
   pInfo->paColors = (COLORREF*) m_MemColors.p;
   pInfo->dwNumVertices = (DWORD)m_MemVertices.m_dwCurPosn / sizeof(VERTEX);
   pInfo->paVertices = (PVERTEX) m_MemVertices.p;
   pInfo->dwNumPolygons = dwNewPoly;
   pInfo->paPolygons = (PPOLYDESCRIPT) m_MemPolygons.p;

   // done
   return fClipped;
}


/********************************************************************************
CRenderClip::TrivialClips - Tests a bounding box (or whatever) to see if an
object can quickly be clipped entirely, and/or to know what clipping planes
need to be tested for the polygons in the bounding box.

inputs
   DWORD       dwNum - Number of points in the bounding box
   PCPoint     paPoints - Pointer to the points. These have the perspective
               transform applied.
   DWORD       *pdwBitPlanes - Filled with a bit for each plane that the
               bounding points cross over, and hence which need to be clipped against.
returns
   BOOL - If TRUE then entire bounding box (and all its contents) are clipped.
      If FALSE, look at *pdwBitPlanes. If it's 0 then no clipping needs to be done
      because the object is entirely on the screen. If it's positive then some
      clipping needs to occur.
*/
BOOL CRenderClip::TrivialClip (DWORD dwNum, PCPoint paPoints, DWORD *pdwBitPlanes)
{
	MALLOCOPT_INIT;
	MALLOCOPT_OKTOMALLOC;
   m_MemClipBits.Required (dwNum * sizeof(DWORD));
	MALLOCOPT_RESTORE;
   DWORD *pdwClipBits = (DWORD*) m_MemClipBits.p;
   CalcClipBits ((DWORD)-1, dwNum, paPoints, pdwClipBits);
   DWORD i, dwAnd, dwOr;
   dwAnd = (DWORD)-1;
   dwOr = 0;
   for (i = 0; i < dwNum; i++) {
      dwAnd &= pdwClipBits[i];
      dwOr |= pdwClipBits[i];
   }

   *pdwBitPlanes = dwOr;
   return dwAnd ? TRUE : FALSE;
}


/********************************************************************************
CRenderClip::ClipMask - Returns a DWORD with one bit set for each clipping
plane that have.

returns
   DWORD - returns clipping mastk
*/

DWORD CRenderClip::ClipMask (void)
{
   return ((1 << m_listCLIPPLANE.Num()) - 1);
}