/************************************************************************
CObjectGround.cpp - Draws a box.

begun 12/9/01 by Mike Rozak
Copyright 2001 Mike Rozak. All rights reserved
*/

#include <windows.h>
#include <crtdbg.h>
#include "escarpment.h"
#include "..\M3D.h"
#include "resource.h"

// BUGFIX - More sub objects so that can render in multiple threads faster
// BUGFIX - Removed because actually slower
// #define USENUMSUBOJECTPERGOUNDSEC
#ifdef USENUMSUBOJECTPERGOUNDSEC
#define NUMSUBOJECTPERGOUNDSEC      (1 + NUMFORESTCANOPIES)
#else
#define NUMSUBOJECTPERGOUNDSEC      1
#endif

// OGCUTOUT - Stores information about each cutout
typedef struct {
   GUID           gID;        // ID for cutout
   BOOL           fEmbedded;  // TRUE if from embedded object
   PCListFixed    plTEXTUREPOINT;   // list of texturepoint

   // scratch variables
   TEXTUREPOINT   tpMin;      // minimum, from 0..m_dwWidth,m_dwHeight
   TEXTUREPOINT   tpMax;      // maximum, from 0..m_dwWidth,m_dwHeight
   BOOL           fAffect;    // set to true if affects the given grid
} OGCUTOUT, *POGCUTOUT;

// OGGRID - Stores information about each grid and if affected by cutout
typedef struct {
   DWORD          dwID;       // Location in ground mesh. = x + y * m_dwWidth
   PCMeshGrid     pGrid;      // if NULL then entire grid is cutout, else, use this to draw
} OGGRID, *POGGRID;

// GROUNDSEC - Information describing a section of ground
typedef struct {
   DWORD          dwXMin, dwXMax;   // X range
   DWORD          dwYMin, dwYMax;   // Y range
   CPoint         pBoundMin, pBoundMax;   // minimum and maximum bounding boxes
   fp             fDist;            // distance from the camera
} GROUNDSEC, *PGROUNDSEC;

//static PWSTR gszPoints = L"points%d-%d";

// GMS - SurfaceOverMain struct
//typedef struct {
//   PCListFixed             plSeq;      // sequences that intersect the roof
//   PCObjectGround          pThis;      // this
//   BOOL                    fSideA;     // true if it's side A
//   PCListFixed             plSelected; // list of selected segments
//   WCHAR                   szEditing[256];   // name of overlay that editing
//   DWORD                   *pdwCurColor;  // change this to change the current colored displayed
//                                          // set to &m_dwDisplayControl
//} GMS, *PGMS;

typedef struct {
   PWSTR       pszName;    // name of the move point
   int         iX, iY; // -1 if for left/top, 1 for right/bottom
} SPLINEMOVEP, *PSPLINEMOVEP;

static SPLINEMOVEP   gaGroundMove[] = {
   L"Center", 0, 0,
   L"North-west corner", -1, 1,
   L"North-east corner", 1, 1,
   L"South-west corner", -1, -1,
   L"South-east corner", 1, -1
};

#define     WATER             10          // iD for water
#define     SIDEA             100
#define     TREEID            110

#define     LESSDETAIL        4           // draw 1 out of 4. Note: This must be an integral multiple of BBSIZE
#define     BBSIZE            (LESSDETAIL*4)          // make a sub-bounding box for each 16x16 grid
               // BUGFIX - Increase bounding box to 32 x 32
               // BUGFIX - Back to 16x16 so can take advantage of pairing out (with lots of trees)

/**********************************************************************************
CGroundTexture - Draws the texture for the ground and water
*/

// BUGFIX - Use CGroundBlend so can access CGroundTexture multithreadeded
class CGroundBlend {
public:
   ESCNEWDELETE;

   CGroundBlend (void);

   // filled in by CalcBlend()
   DWORD             m_dwNumBlend;  // number of sockets in the blend
   PCTextureMapSocket *m_papBlendText;   // pointer to an array of texturemaps's to use for blending
   fp                *m_pafBlendAmt; // blend amount, from 0..1. These sum to 0.
   PCObjectSurface   *m_papBlendSurf;  // object surfaces
   DWORD             *m_padwBlendIndex; // index for blending
   CListFixed        m_lBlendText;   // list to store the blend texture pointers
   CListFixed        m_lBlendAmt;   // list fo store the blend amount
   CListFixed        m_lBlendSurf;  // list of PCObjectSurfaces
   CListFixed        m_lBlendIndex; // list of DWORD's for color index number
   CMem              m_memFillLine; // temporary memory used in FillLine
};
typedef CGroundBlend *PCGroundBlend;

#define THREADSAFECOPIES   // if TRUE use thread safe copies
                           // MUST be on if use critical section
#define USECRITSEC         // use critical section to ensure no crashes in texture

#ifdef USECRITSEC
#define THREADSAFECOPIES
#endif

class CGroundTexture : public CTextureMapSocket {
public:
   ESCNEWDELETE;

   CGroundTexture (GUID *pgCode, GUID *pgSub, PCObjectGround pGround, DWORD dwType);
   ~CGroundTexture (void);

   virtual void TextureModsSet (PTEXTUREMODS pt);
   virtual void TextureModsGet (PTEXTUREMODS pt);
   virtual void GUIDsGet (GUID *pCode, GUID *pSub);
   virtual void GUIDsSet (const GUID *pCode, const GUID *pSub);
   virtual void DefScaleGet (fp *pfDefScaleH, fp *pfDefScaleV);
   virtual void MaterialGet (DWORD dwThread, PCMaterial pMat);
   virtual DWORD MightBeTransparent (DWORD dwThread);
   virtual DWORD DimensionalityQuery (DWORD dwThread);
   virtual COLORREF AverageColorGet (DWORD dwThread, BOOL fGlow);
   virtual void Delete (void);
   virtual void ForceCache (DWORD dwForceCache);

   virtual void FillPixel (DWORD dwThread, DWORD dwFlags, WORD *pawColor, const PTEXTPOINT5 pText, const PTEXTPOINT5 pMax,
      PCMaterial pMat, float *pafGlow, BOOL fHighQuality);
   virtual BOOL QueryTextureBlurring (DWORD dwThread);
   virtual BOOL QueryBump (DWORD dwThread);
   virtual BOOL PixelBump (DWORD dwThread, const PTEXTPOINT5 pText, const PTEXTPOINT5 pRight,
                             const PTEXTPOINT5 pDown, const PTEXTUREPOINT pSlope, fp *pfHeight = NULL, BOOL fHighQuality = FALSE);
   virtual void FillLine (DWORD dwThread, PGCOLOR pac, DWORD dwNum, const PTEXTPOINT5 pLeft, const PTEXTPOINT5 pRight,
      float *pafGlow, BOOL *pfGlow, WORD *pawTrans, BOOL *pfTrans, fp *pafAlpha /*= NULL*/);

private:
   void CalcBlend (PCGroundBlend pBlend, PTEXTPOINT5 pt, BOOL fGetTexture /*= TRUE*/, DWORD dwThread);
   void CalcBlend (PCGroundBlend pBlend, DWORD dwThread);
   void TextureCacheGet (PCGroundBlend pBlend, DWORD dwThread);
   void TexturePointConvert (PTEXTPOINT5 pOrig, PRENDERSURFACE prs, PTEXTPOINT5 pNew);

   // passed through initialization
   GUID              m_gCode;       // GUID for ID
   GUID              m_gSub;        // GUID for ID
   PCObjectGround    m_pGround;     // ground object to get information from
   DWORD             m_dwType;      // 0 for ground, 1 for water
   DWORD             m_dwRenderShard;  // render shard to use

   // passed in through texturemodsset
   TEXTUREMODS       m_TextureMods; // not really sued for anything

   // blends
#ifdef THREADSAFECOPIES
   // BUGFIX - Multiple blends and crit sec to prevent bottleneck
   CGroundBlend      m_aBlend[GROUNDTHREADS];       // global blend for main thread - fast drawing
   // Moved to CObjectGround CRITICAL_SECTION  m_aCritSec[GROUNDTHREADS];     // critical section to block access
#else
   CGroundBlend      m_Blend;       // global blend for main thread - fast drawing
   DWORD             m_dwThreadID;     // current thread handle
#endif
};
typedef CGroundTexture *PCGroundTexture;



/**********************************************************************************
GroundThread - Returns the thread to use based on the thread ID
*/
DWORD GroundThread (void)
{
   return GetCurrentThreadId() % GROUNDTHREADS;
}

/**********************************************************************************
CGroundBlend::Constructor and destructor
*/
CGroundBlend::CGroundBlend (void)
{
   m_dwNumBlend = 0;
   m_papBlendText = NULL;
   m_pafBlendAmt = NULL;
   m_papBlendSurf = NULL;
   m_padwBlendIndex = NULL;
   m_lBlendText.Init (sizeof (PCTextureMapSocket));
   m_lBlendAmt.Init (sizeof(fp));
   m_lBlendSurf.Init (sizeof (PCObjectSurface));
   m_lBlendIndex.Init (sizeof(DWORD));
}

/**********************************************************************************
CGroundTexture::Constructor and destructor
*/
CGroundTexture::CGroundTexture (GUID *pgCode, GUID *pgSub, PCObjectGround pGround, DWORD dwType)
{
   m_gCode = *pgCode;
   m_gSub = *pgSub;
   m_pGround = pGround;
   m_dwType = dwType;
   m_dwRenderShard = pGround->m_OSINFO.dwRenderShard;
   memset (&m_TextureMods, 0, sizeof(m_TextureMods));

#ifdef THREADSAFECOPIES
   // moved to cGround
   //DWORD i;
   //for (i = 0; i < GROUNDTHREADS; i++)
   //   InitializeCriticalSection (&m_aCritSec[i]);
#else
   m_dwThreadID = GetCurrentThreadId ();
#endif
}

// NOTE: Dont call destructor directly.... call through delete
CGroundTexture::~CGroundTexture (void)
{

#ifdef THREADSAFECOPIES
   // moved to cObjectGround
   //DWORD i;
   //for (i = 0; i < GROUNDTHREADS; i++)
   //   DeleteCriticalSection (&m_aCritSec[i]);
#endif
}

/**********************************************************************************
CGroundTexture::CalcBlend - Fills in m_dwNumBlend, m_papBlenText, and m_pafBlendAmt
given the texture point location. This needs to be called whenever a new texture
point is requested, or whenever the ground texture code is edited because the texture
cache may end up deleting the objects it refers to.


inputs
   PCGroundBlend pBlend - Blend info to fill in. Note: Only hv[] is used
   BOOL        fGetTexture - If TUYE, also fills in m_lBlendText and m_papBlendText

NOTE: This does not call TextureCacheRelease(), which is OK since the function
does nothing right now.
*/
void CGroundTexture::CalcBlend (PCGroundBlend pBlend, PTEXTPOINT5 pt, BOOL fGetTexture, DWORD dwThread)
{
   // find the surrounding texture types
   TEXTUREPOINT tpIndex, tpDelta;
   int ih, iv;
   tpIndex.h = floor(pt->hv[0]);
   tpIndex.v = floor(pt->hv[1]);
   tpDelta.h = pt->hv[0] - tpIndex.h;
   tpDelta.v = pt->hv[1] - tpIndex.v;
   ih = (int) tpIndex.h - 1;
   iv = (int) tpIndex.v - 1;

   // clear out the list
   pBlend->m_lBlendAmt.Clear();
   pBlend->m_lBlendSurf.Clear();
   pBlend->m_lBlendIndex.Clear();

   fp f, f2;
   DWORD i, dw, j;
   PCObjectSurface pos;
   if (m_dwType == 1) {
      // what is the height of the land?
      CPoint pLoc;
      if ((pt->hv[0] >= 0) && (pt->hv[1] >= 0) && (pt->hv[0] <= (fp)(m_pGround->m_dwWidth-1)) &&
         (pt->hv[1] <= (fp)(m_pGround->m_dwHeight-1)) )
            m_pGround->HVToPoint (pt->hv[0] / (fp)(m_pGround->m_dwWidth-1),
               pt->hv[1] / (fp)(m_pGround->m_dwHeight-1), &pLoc);
      else
         pLoc.p[2] = m_pGround->m_tpElev.h;

      // how much higher is the water than the land
      fp fDepth;
      fDepth = m_pGround->m_fWaterElevation - pLoc.p[2];

      // which to blend
      for (i = 0; i < GWATERNUM; i++)
         if (fDepth < m_pGround->m_afWaterElev[i])
            break;
      if (!i || (i >= GWATERNUM)) {
         // either below lowest, or above highest
         i = min(i, GWATERNUM-1);
         f = 1;

         f /= (fp)GWATEROVERLAP;
         pBlend->m_lBlendAmt.Required (pBlend->m_lBlendAmt.Num() + GWATEROVERLAP);
         pBlend->m_lBlendSurf.Required (pBlend->m_lBlendSurf.Num() + GWATEROVERLAP);
         pBlend->m_lBlendIndex.Required (pBlend->m_lBlendIndex.Num() + GWATEROVERLAP);
         for (j = 0; j < GWATEROVERLAP; j++) {
            dw = i + j * GWATERNUM;
            pBlend->m_lBlendAmt.Add (&f);
            pos = m_pGround->m_apWaterSurf[dw];
            pBlend->m_lBlendSurf.Add (&pos);
            pBlend->m_lBlendIndex.Add (&dw);
         }
      }
      else { // blend
         f = m_pGround->m_afWaterElev[i] - m_pGround->m_afWaterElev[i-1];
         f = max(f, CLOSE);   // at least some difference
         f = (fDepth - m_pGround->m_afWaterElev[i-1]) / f;

         // add deeper bit
         f2 = f / (fp)GWATEROVERLAP;
         pBlend->m_lBlendAmt.Required (pBlend->m_lBlendAmt.Num() + GWATEROVERLAP);
         pBlend->m_lBlendSurf.Required (pBlend->m_lBlendSurf.Num() + GWATEROVERLAP);
         pBlend->m_lBlendIndex.Required (pBlend->m_lBlendIndex.Num() + GWATEROVERLAP);
         for (j = 0; j < GWATEROVERLAP; j++) {
            dw = i + j * GWATERNUM;
            pBlend->m_lBlendAmt.Add (&f2);
            pos = m_pGround->m_apWaterSurf[dw];
            pBlend->m_lBlendSurf.Add (&pos);
            pBlend->m_lBlendIndex.Add (&dw);
         }

         // add shallow bit
         f = 1 - f;
         f2 = f / (fp)GWATEROVERLAP;
         pBlend->m_lBlendAmt.Required (pBlend->m_lBlendAmt.Num() + GWATEROVERLAP);
         pBlend->m_lBlendSurf.Required (pBlend->m_lBlendSurf.Num() + GWATEROVERLAP);
         pBlend->m_lBlendIndex.Required (pBlend->m_lBlendIndex.Num() + GWATEROVERLAP);
         for (j = 0; j < GWATEROVERLAP; j++) {
            dw = (i-1) + j * GWATERNUM;
            pBlend->m_lBlendAmt.Add (&f2);
            pos = m_pGround->m_apWaterSurf[dw];
            pBlend->m_lBlendSurf.Add (&pos);
            pBlend->m_lBlendIndex.Add (&dw);
         }
      }
   }
   else {   // ground
      // index into the list of surfaces
      PCObjectSurface *ppos;
      ppos = (PCObjectSurface*) m_pGround->m_lTextPCObjectSurface.Get(0);

      DWORD dwText;
      BYTE *pab;
      int iHeight, iWidth;
      BOOL fSame;
      BYTE abText[4][4];   // [y][x]
      int x,y, xx, yy;
      fp fSum;
      fSum = 0;
      iHeight = (int)m_pGround->m_dwHeight;
      iWidth = (int)m_pGround->m_dwWidth;

      pBlend->m_lBlendAmt.Required (pBlend->m_lBlendAmt.Num() + m_pGround->m_lTextPCObjectSurface.Num());
      pBlend->m_lBlendSurf.Required (pBlend->m_lBlendSurf.Num() + m_pGround->m_lTextPCObjectSurface.Num());
      pBlend->m_lBlendIndex.Required (pBlend->m_lBlendIndex.Num() + m_pGround->m_lTextPCObjectSurface.Num());

      for (dwText = 0; dwText < m_pGround->m_lTextPCObjectSurface.Num(); dwText++) {
         pab = m_pGround->m_pabTextSet + ((int) dwText * iHeight * iWidth);

#define LINEARINTERPGROUND    // BUGFIX - so draws faster
#ifdef LINEARINTERPGROUND
         // get the textures, and see if they're the same
         fSame = TRUE;
         for (y = 0; y < 2; y++) {
            yy = iv + y + 1;
            yy = max(0, yy);
            yy = min(iHeight-1, yy);
            for (x = 0; x < 2; x++) {
               xx = ih + x + 1;
               xx = max(0, xx);
               xx = min(iWidth-1, xx);
               abText[y][x] = pab[xx + yy * iWidth];

               // keep track of if all the same
               fSame = fSame & (abText[y][x] == abText[0][0]);
            } // x
         } // y

         // if they're the same easy
         if (fSame) {
            if (!abText[0][0])
               continue;   // all zero
            f = abText[0][0];
            fSum += f;
            pBlend->m_lBlendSurf.Add (&ppos[dwText]);
            pBlend->m_lBlendAmt.Add (&f);
            pBlend->m_lBlendIndex.Add (&dwText);
            continue;
         }

         // else need to do linear
         fp afHerm[2];
         for (x = 0; x < 2; x++)
            afHerm[x] = (fp)abText[0][x] * (1.0 - tpDelta.v) + (fp)abText[1][x] * tpDelta.v;

         f = afHerm[0] * (1.0 - tpDelta.h) + afHerm[1] * tpDelta.h;
         f = max(0, f);
#else // use hermite, slower
         // get the textures, and see if they're the same
         fSame = TRUE;
         for (y = 0; y < 4; y++) {
            yy = iv + y;
            yy = max(0, yy);
            yy = min(iHeight-1, yy);
            for (x = 0; x < 4; x++) {
               xx = ih + x;
               xx = max(0, xx);
               xx = min(iWidth-1, xx);
               abText[y][x] = pab[xx + yy * iWidth];

               // keep track of if all the same
               fSame = fSame & (abText[y][x] == abText[0][0]);
            } // x
         } // y

         // if they're the same easy
         if (fSame) {
            if (!abText[0][0])
               continue;   // all zero
            f = abText[0][0];
            fSum += f;
            pBlend->m_lBlendSurf.Add (&ppos[dwText]);
            pBlend->m_lBlendAmt.Add (&f);
            pBlend->m_lBlendIndex.Add (&dwText);
            continue;
         }

         // else need to do hermite
         fp afHerm[4];
         for (x = 0; x < 4; x++)
            afHerm[x] = HermiteCubic (tpDelta.v, abText[0][x], abText[1][x],
               abText[2][x], abText[3][x]);

         f = HermiteCubic (tpDelta.h, afHerm[0], afHerm[1], afHerm[2], afHerm[3]);
         f = max(0, f);
#endif // !LINEARINTERPGROUND

         pBlend->m_lBlendAmt.Add (&f);
         fSum += f;

         // also add the surface
         pBlend->m_lBlendSurf.Add (&ppos[dwText]);
         pBlend->m_lBlendIndex.Add (&dwText);
      } // dwText


      // go back and normalize
      if (fSum) {
         fSum = max(fSum, CLOSE);   // so no divide by 0
         pBlend->m_pafBlendAmt = (fp*) pBlend->m_lBlendAmt.Get(0);
         for (i = 0; i < pBlend->m_lBlendAmt.Num(); i++)
            pBlend->m_pafBlendAmt[i] /= fSum;
      }
   }

   // fill in all the values
   pBlend->m_dwNumBlend = pBlend->m_lBlendSurf.Num();
   pBlend->m_pafBlendAmt = (fp*) pBlend->m_lBlendAmt.Get(0);
   pBlend->m_papBlendSurf = (PCObjectSurface*) pBlend->m_lBlendSurf.Get(0);
   pBlend->m_padwBlendIndex = (DWORD*)pBlend->m_lBlendIndex.Get(0);

   if (fGetTexture)
      TextureCacheGet(pBlend, dwThread);
}


/**********************************************************************************
CGroundTexture::CalcBlend - Calculates the blend if no texture point is specified.
Basically, this creates an equal-weight blend.

inputs
   PCGroundBlend pBlend - Blend info to fill in
*/
void CGroundTexture::CalcBlend (PCGroundBlend pBlend, DWORD dwThread)
{
   // clear out the list
   pBlend->m_lBlendAmt.Clear();
   pBlend->m_lBlendSurf.Clear();
   pBlend->m_lBlendIndex.Clear();

   // add them all
   PCObjectSurface *ppos;
   DWORD dwNum, i;
   if (m_dwType ==  1) {   // water
      ppos = m_pGround->m_apWaterSurf;
      dwNum = GWATERNUM * GWATEROVERLAP;
   }
   else {
      ppos = (PCObjectSurface*) m_pGround->m_lTextPCObjectSurface.Get(0);
      dwNum = m_pGround->m_lTextPCObjectSurface.Num();
   }

   fp f;
   f = 1.0 / (fp) dwNum;
   pBlend->m_lBlendAmt.Required (dwNum);
   pBlend->m_lBlendSurf.Required (dwNum);
   pBlend->m_lBlendIndex.Required (dwNum);
   for (i = 0; i < dwNum; i++) {
      pBlend->m_lBlendAmt.Add (&f);
      pBlend->m_lBlendIndex.Add (&i);
      pBlend->m_lBlendSurf.Add (&ppos[i]);
   }

   // fill in all the values
   pBlend->m_dwNumBlend = pBlend->m_lBlendSurf.Num();
   pBlend->m_pafBlendAmt = (fp*) pBlend->m_lBlendAmt.Get(0);
   pBlend->m_papBlendSurf = (PCObjectSurface*) pBlend->m_lBlendSurf.Get(0);
   pBlend->m_padwBlendIndex = (DWORD*)pBlend->m_lBlendIndex.Get(0);

   TextureCacheGet(pBlend, dwThread);
}

/**********************************************************************************
CGroundTexture::TextureCacheGet - Uses the m_papBlendSurf and m_dwNumBlend, and
fills in m_lBlendText and m_papBlendText. NOTE: May be filled in with NULL if a solid
color is selected.
*/
void CGroundTexture::TextureCacheGet (PCGroundBlend pBlend, DWORD dwThread)
{
   pBlend->m_lBlendText.Clear();

   DWORD i;
   PCTextureMapSocket pText;
   // get from the texture cache
   PCObjectSurface pos;
   PRENDERSURFACEPLUS prs;
   if (m_dwType == 1) // water
      prs = &m_pGround->m_aWaterRS[dwThread][0];
   else
      prs = (PRENDERSURFACEPLUS) m_pGround->m_alRENDERSURFACEPLUS[dwThread].Get(0);

   pBlend->m_lBlendText.Required (pBlend->m_dwNumBlend);
   for (i = 0; i < pBlend->m_dwNumBlend; i++) {
      pos = pBlend->m_papBlendSurf[i];

      if (!pos->m_fUseTextureMap) {
         pText = NULL;
         pBlend->m_lBlendText.Add (&pText);
         continue;
      }

      DWORD dwIndex = pBlend->m_padwBlendIndex[i];
      pText = prs[dwIndex].pTexture = ::TextureCacheGet (m_dwRenderShard, &prs[dwIndex].rs, &prs[dwIndex].qwTextureCacheTimeStamp, prs[dwIndex].pTexture);
      pBlend->m_lBlendText.Add (&pText);
   }

   // fill in pointer
   pBlend->m_papBlendText = (PCTextureMapSocket*) pBlend->m_lBlendText.Get(0);
}

/**********************************************************************************
CGroundTexture::TextureModsSet - As per CTextureMapSocket
*/
void CGroundTexture::TextureModsSet (PTEXTUREMODS pt)
{
   // no need to use critical section since not called multithreaded
   m_TextureMods = *pt;
}

/**********************************************************************************
CGroundTexture::TextureModsGet - As per CTextureMapSocket
*/
void CGroundTexture::TextureModsGet (PTEXTUREMODS pt)
{
   // No need to use critical section
   *pt = m_TextureMods;
}

/**********************************************************************************
CGroundTexture::ForceCache - As per CTextureMapSocket
*/
void CGroundTexture::ForceCache (DWORD dwForceCache)
{
	MALLOCOPT_INIT;
#ifdef THREADSAFECOPIES
   DWORD dwThread;
   for (dwThread = 0; dwThread < MAXRAYTHREAD; dwThread++) {
      DWORD dwGroundThread = dwThread;

#ifdef USECRITSEC
      EnterCriticalSection (&m_pGround->m_aCritSec[dwGroundThread]);
#endif
      PCGroundBlend pBlend = &m_aBlend[dwGroundThread];
	   MALLOCOPT_OKTOMALLOC;
      CalcBlend(pBlend, dwGroundThread);
   	MALLOCOPT_RESTORE;
      DWORD i, dw;
      dw = 0;
      for (i = 0; i < pBlend->m_dwNumBlend; i++)
         if (pBlend->m_papBlendText[i])
            pBlend->m_papBlendText[i]->ForceCache(dwForceCache);

#ifdef USECRITSEC
      LeaveCriticalSection (&m_pGround->m_aCritSec[dwGroundThread]);
#endif
   } // dwThread
#else


#ifdef USECRITSEC
   DWORD dwGroundThread = GroundThread ();
   EnterCriticalSection (&m_pGround->m_aCritSec[dwGroundThread]);
#endif
   // loop through all the sub-textures and force to cache
   //PCGroundBlend pBlend = &m_Blend;
#ifndef USECRITSEC
   CGroundBlend Blend;
   PCGroundBlend pBlend = (GetCurrentThreadId() == m_dwThreadID) ? &m_Blend : &Blend;
#else
   PCGroundBlend pBlend = &m_aBlend[dwGroundThread];
#endif

   CalcBlend(pBlend, dwGroundThread);
   DWORD i, dw;
   dw = 0;
   for (i = 0; i < pBlend->m_dwNumBlend; i++)
      if (pBlend->m_papBlendText[i])
         pBlend->m_papBlendText[i]->ForceCache(dwForceCache);

#ifdef USECRITSEC
   LeaveCriticalSection (&m_pGround->m_aCritSec[dwGroundThread]);
#endif
#endif // THREADSAFECOPIES
}

/**********************************************************************************
CGroundTexture::GUIDsGet - As per CTextureMapSocket
*/
void CGroundTexture::GUIDsGet (GUID *pCode, GUID *pSub)
{
   // No need to use critical section since not called multithreaded
   *pCode = m_gCode;
   *pSub = m_gSub;
}

/**********************************************************************************
CGroundTexture::GUIDsSet - As per CTextureMapSocket
*/
void CGroundTexture::GUIDsSet (const GUID *pCode, const GUID *pSub)
{
   // No need to use critical section since not called multithreaded
   m_gCode = *pCode;
   m_gSub = *pSub;
}

/**********************************************************************************
CGroundTexture::DefScaleGet - As per CTextureMapSocket
*/
void CGroundTexture::DefScaleGet (fp *pfDefScaleH, fp *pfDefScaleV)
{
   // No need to use criticcal section
   // return up junk
   *pfDefScaleH = *pfDefScaleV = 1;
}

/**********************************************************************************
CGroundTexture::MaterialGet - As per CTextureMapSocket
*/
void CGroundTexture::MaterialGet (DWORD dwThread, PCMaterial pMat)
{
#ifdef THREADSAFECOPIES
   DWORD dwGroundThread = dwThread;

#ifdef USECRITSEC
   EnterCriticalSection (&m_pGround->m_aCritSec[dwGroundThread]);
#endif

#else
#ifdef USECRITSEC
   DWORD dwGroundThread = GroundThread ();
   EnterCriticalSection (&m_pGround->m_aCritSec[dwGroundThread]);
#endif
#endif // THREADSAFECOPIES

   // take the first one that find
   //PCGroundBlend pBlend = &m_Blend;
#ifndef THREADSAFECOPIES
   CGroundBlend Blend;
   PCGroundBlend pBlend = (GetCurrentThreadId() == m_dwThreadID) ? &m_Blend : &Blend;
#else
   PCGroundBlend pBlend = &m_aBlend[dwGroundThread];
#endif
   CalcBlend(pBlend, dwGroundThread);
   DWORD i;
   pMat->m_fGlow = 0;
   //pMat->m_fThickness = 0;
   pMat->m_wFill = 0;
   pMat->m_wTranslucent = 0;
   pMat->m_wSpecExponent = 0;
   pMat->m_wSpecPlastic = 0;
   pMat->m_wSpecReflect = 0;
   pMat->m_wTransparency = 0;
   pMat->m_fNoShadows = FALSE;
   pMat->m_wIndexOfRefract = 0;
   pMat->m_wReflectAmount = 0;
   pMat->m_wReflectAngle = 0;
   pMat->m_wTransAngle = 0;

   CMaterial MatTemp;
   PRENDERSURFACEPLUS prs;
   if (m_dwType == 1) // water
      prs = m_pGround->m_aWaterRS[dwGroundThread];
   else
      prs = (PRENDERSURFACEPLUS) m_pGround->m_alRENDERSURFACEPLUS[dwGroundThread].Get(0);
   PRENDERSURFACEPLUS prsCur;
   fp fScale;
   for (i = 0; i < pBlend->m_dwNumBlend; i++) {
      fScale = pBlend->m_pafBlendAmt[i];
      prsCur = prs + pBlend->m_padwBlendIndex[i];
      if (pBlend->m_papBlendText[i])
         pBlend->m_papBlendText[i]->MaterialGet(dwThread, &MatTemp);
      else
         memcpy (&MatTemp, &prsCur->rs.Material, sizeof(MatTemp));

      pMat->m_fGlow |= MatTemp.m_fGlow;
      //pMat->m_fThickness += MatTemp.m_fThickness * fScale;
      pMat->m_wFill = 0;
      pMat->m_fNoShadows |= MatTemp.m_fNoShadows;
      pMat->m_wSpecExponent += (WORD)((fp)MatTemp.m_wSpecExponent * fScale);
      pMat->m_wSpecPlastic += (WORD)((fp)MatTemp.m_wSpecPlastic * fScale);
      pMat->m_wSpecReflect += (WORD)((fp)MatTemp.m_wSpecReflect * fScale);;
      pMat->m_wTransparency += (WORD)((fp)MatTemp.m_wTransparency * fScale);
      pMat->m_wIndexOfRefract += (WORD)((fp)MatTemp.m_wIndexOfRefract * fScale);
      pMat->m_wReflectAmount += (WORD)((fp)MatTemp.m_wReflectAmount * fScale);
      pMat->m_wReflectAngle += (WORD)((fp)MatTemp.m_wReflectAngle * fScale);
      pMat->m_wTransAngle += (WORD)((fp)MatTemp.m_wTransAngle * fScale);
      pMat->m_wTranslucent += (WORD)((fp)MatTemp.m_wTranslucent * fScale);
   }

#ifdef USECRITSEC
   LeaveCriticalSection (&m_pGround->m_aCritSec[dwGroundThread]);
#endif
}


/*******************************************************************************
CGroundTexture::DimensionalityQuery - Returns flags indicating what dimesnionality
is supported by the texture.

returns
   DWORD - 0x01 bit if supports HV, 0x02 bit if supports XYZ
*/
DWORD CGroundTexture::DimensionalityQuery (DWORD dwThread)
{
#ifdef THREADSAFECOPIES
   DWORD dwGroundThread = dwThread;
#ifdef USECRITSEC
   EnterCriticalSection (&m_pGround->m_aCritSec[dwGroundThread]);
#endif
#else
#ifdef USECRITSEC
   DWORD dwGroundThread = GroundThread ();
   EnterCriticalSection (&m_pGround->m_aCritSec[dwGroundThread]);
#endif
#endif

   DWORD dw = 0;
   //PCGroundBlend pBlend = &m_Blend;
#ifndef THREADSAFECOPIES
   CGroundBlend Blend;
   PCGroundBlend pBlend = (GetCurrentThreadId() == m_dwThreadID) ? &m_Blend : &Blend;
#else
   PCGroundBlend pBlend = &m_aBlend[dwGroundThread];
#endif
   CalcBlend(pBlend, dwGroundThread);
   DWORD i;
   for (i = 0; i < pBlend->m_dwNumBlend; i++) {
      if (!pBlend->m_papBlendText[i])
         continue;

      dw |= pBlend->m_papBlendText[i]->DimensionalityQuery(dwThread);
         // BUGFIX - Was calling MightBeTransparent
   }

#ifdef USECRITSEC
   LeaveCriticalSection (&m_pGround->m_aCritSec[dwGroundThread]);
#endif
   return dw;
}


/**********************************************************************************
CGroundTexture::MightBeTransparent - As per CTextureMapSocket
*/
DWORD CGroundTexture::MightBeTransparent (DWORD dwThread)
{
#ifdef THREADSAFECOPIES
   DWORD dwGroundThread = dwThread;
#ifdef USECRITSEC
   EnterCriticalSection (&m_pGround->m_aCritSec[dwGroundThread]);
#endif
#else
#ifdef USECRITSEC
   DWORD dwGroundThread = GroundThread ();
   EnterCriticalSection (&m_pGround->m_aCritSec[dwGroundThread]);
#endif
#endif //THREADSAFECOPIES

   //PCGroundBlend pBlend = &m_Blend;
#ifndef THREADSAFECOPIES
   CGroundBlend Blend;
   PCGroundBlend pBlend = (GetCurrentThreadId() == m_dwThreadID) ? &m_Blend : &Blend;
#else
   PCGroundBlend pBlend = &m_aBlend[dwGroundThread];
#endif
   CalcBlend(pBlend, dwGroundThread);
   DWORD i, dw;
   dw = 0;
   for (i = 0; i < pBlend->m_dwNumBlend; i++) {
      if (!pBlend->m_papBlendText[i]) {
         if (pBlend->m_papBlendSurf[i]->m_Material.m_wTransparency)
            dw |= 0x01;
         continue;
      }

      dw |= pBlend->m_papBlendText[i]->MightBeTransparent(dwThread);
   }

#ifdef USECRITSEC
   LeaveCriticalSection (&m_pGround->m_aCritSec[dwGroundThread]);
#endif

   // BUGFIX - If any possibility of transparency then also include
   // per-pixel
   if (dw & DHLTEXT_TRANSPARENT)
      dw |= DHLTEXT_PERPIXTRANS;

   return dw;
}

/**********************************************************************************
CGroundTexture::AverageColorGet - As per CTextureMapSocket
*/
COLORREF CGroundTexture::AverageColorGet (DWORD dwThread, BOOL fGlow)
{
#ifdef THREADSAFECOPIES
   DWORD dwGroundThread = dwThread;
#ifdef USECRITSEC
   EnterCriticalSection (&m_pGround->m_aCritSec[dwGroundThread]);
#endif
#else
#ifdef USECRITSEC
   DWORD dwGroundThread = GroundThread ();
   EnterCriticalSection (&m_pGround->m_aCritSec[dwGroundThread]);
#endif
#endif // THREADSAFECOPIES
   DWORD adwAverage[3];
   memset (adwAverage, 0, sizeof(adwAverage));

   //PCGroundBlend pBlend = &m_Blend;
#ifndef THREADSAFECOPIES
   CGroundBlend Blend;
   PCGroundBlend pBlend = (GetCurrentThreadId() == m_dwThreadID) ? &m_Blend : &Blend;
#else
   PCGroundBlend pBlend = &m_aBlend[dwGroundThread];
#endif
   CalcBlend(pBlend, dwGroundThread);
   DWORD i;
   COLORREF cr;
   WORD awColor[3];
   for (i = 0; i < pBlend->m_dwNumBlend; i++) {
      if (!pBlend->m_papBlendText[i]) {
         if (fGlow)
            continue;   // no color, so dont self illuminate
         cr = pBlend->m_papBlendSurf[i]->m_cColor;
      }
      else
         cr = pBlend->m_papBlendText[i]->AverageColorGet (dwThread, fGlow);

      Gamma (cr, awColor);
      adwAverage[0] += (DWORD) awColor[0];
      adwAverage[1] += (DWORD) awColor[1];
      adwAverage[2] += (DWORD) awColor[2];
   }
   if (pBlend->m_dwNumBlend) {
      awColor[0] = (WORD)(adwAverage[0] / pBlend->m_dwNumBlend);
      awColor[1] = (WORD)(adwAverage[1] / pBlend->m_dwNumBlend);
      awColor[2] = (WORD)(adwAverage[2] / pBlend->m_dwNumBlend);
   }

#ifdef USECRITSEC
   LeaveCriticalSection (&m_pGround->m_aCritSec[dwGroundThread]);
#endif
   return UnGamma (awColor);
}

/**********************************************************************************
CGroundTexture::Delete - As per CTextureMapSocket
*/
void CGroundTexture::Delete (void)
{
   // no need to protect with critical section
   delete this;
}

/**********************************************************************************
CGroundTexture::TexturePointConvert - Given a texturepoint in the ground (which
is in units of ground grids), this converts to the texture HV used for the specific
texturte.

inputs
   PTEXTPOINT5          pOrig - Original texture point
   PRENDERSURFACE       prs - Surface that converting for
   PTEXTPOINT5          pNew - Filled in with new value
returns
   none
*/
void CGroundTexture::TexturePointConvert (PTEXTPOINT5 pOrig, PRENDERSURFACE prs, PTEXTPOINT5 pNew)
{
   if (!prs->fUseTextureMap)
      return;  // dont bother

   pNew->hv[0] = (prs->afTextureMatrix[0][0] * pOrig->hv[0] + prs->afTextureMatrix[1][0] * pOrig->hv[0]) * m_pGround->m_fScale;
   pNew->hv[1] = (prs->afTextureMatrix[0][1] * pOrig->hv[1] + prs->afTextureMatrix[1][1] * pOrig->hv[1]) * m_pGround->m_fScale;

   // convert the 3-space while at it
   CPoint p;
   p.p[0] = pOrig->xyz[0];
   p.p[1] = pOrig->xyz[1];
   p.p[2] = pOrig->xyz[2];
   p.p[3] = 1;
   p.MultiplyLeft ((PCMatrix)(&prs->abTextureMatrix[0]));
   pNew->xyz[0] = p.p[0];
   pNew->xyz[1] = p.p[1];
   pNew->xyz[2] = p.p[2];
}


/*********************************************************************************
CGroundTexture::QueryTextureBlurring - SOme textures, like the skydome, ignore
the pMax parameter when FillPixel() is called. If this is the
case then QueryNoTextureBlurring() will return FALSE, meaning that
the calculations don't need to be made. However, the default
behavior uses texture blurring and returns TRUE.
*/
BOOL CGroundTexture::QueryTextureBlurring (DWORD dwThread)
{
   return TRUE;   // assume this is the case
}




/*********************************************************************************
CGroundTexture::QueryBump - Sees if a bump map is required
*/
BOOL CGroundTexture::QueryBump (DWORD dwThread)
{
#ifdef THREADSAFECOPIES
   DWORD dwGroundThread = dwThread;
#ifdef USECRITSEC
   EnterCriticalSection (&m_pGround->m_aCritSec[dwGroundThread]);
#endif
#else
#ifdef USECRITSEC
   DWORD dwGroundThread = GroundThread ();
   EnterCriticalSection (&m_pGround->m_aCritSec[dwGroundThread]);
#endif
#endif //THREADSAFECOPIES

   //PCGroundBlend pBlend = &m_Blend;
#ifndef THREADSAFECOPIES
   CGroundBlend Blend;
   PCGroundBlend pBlend = (GetCurrentThreadId() == m_dwThreadID) ? &m_Blend : &Blend;
#else
   PCGroundBlend pBlend = &m_aBlend[dwGroundThread];
#endif
   CalcBlend (pBlend, dwGroundThread);

   // loop through all the items
   DWORD i;
   BOOL fBump;
   fBump = FALSE;
   for (i = 0; i < pBlend->m_dwNumBlend; i++) {
      if (!pBlend->m_papBlendText[i])
         continue;   // only textures will affect

      if (pBlend->m_papBlendText[i]->QueryBump(dwThread)) {
         fBump = TRUE;
         break;
      }

   }

#ifdef USECRITSEC
   LeaveCriticalSection (&m_pGround->m_aCritSec[dwGroundThread]);
#endif
   return fBump;


}



/**********************************************************************************
CGroundTexture::FillPixel - As per CTextureMapSocket
*/
void CGroundTexture::FillPixel (DWORD dwThread, DWORD dwFlags, WORD *pawColor, PTEXTPOINT5 pText, PTEXTPOINT5 pMax,
   PCMaterial pMat, float *pafGlow, BOOL fHighQuality)
{
#ifdef THREADSAFECOPIES
   DWORD dwGroundThread = dwThread;
#ifdef USECRITSEC
   EnterCriticalSection (&m_pGround->m_aCritSec[dwGroundThread]);
#endif
#else
#ifdef USECRITSEC
   DWORD dwGroundThread = GroundThread ();
   EnterCriticalSection (&m_pGround->m_aCritSec[dwGroundThread]);
#endif
#endif //THREADSAFECOPIES

   //PCGroundBlend pBlend = &m_Blend;
#ifndef THREADSAFECOPIES
   CGroundBlend Blend;
   PCGroundBlend pBlend = (GetCurrentThreadId() == m_dwThreadID) ? &m_Blend : &Blend;
#else
   PCGroundBlend pBlend = &m_aBlend[dwGroundThread];
#endif
   CalcBlend (pBlend, pText, TRUE, dwGroundThread);
   if (pawColor)
      pawColor[0] = pawColor[1] = pawColor[2] = 0;
   if (pafGlow)
      pafGlow[0] = pafGlow[1] = pafGlow[2] = 0;

   if (dwFlags & TMFP_TRANSPARENCY)
      pMat->m_wTransparency = 0;
   if (dwFlags & TMFP_SPECULARITY) {
      pMat->m_wSpecExponent = 0;
      pMat->m_wSpecPlastic = 0;
      pMat->m_wSpecReflect = 0;
   }
   if (dwFlags & TMFP_OTHERMATINFO) {
      pMat->m_fGlow = 0;
      //pMat->m_fThickness = 0;
      pMat->m_wFill = 0;
      pMat->m_fNoShadows = FALSE;
      pMat->m_wIndexOfRefract = 0;
      pMat->m_wReflectAmount = 0;
      pMat->m_wReflectAngle = 0;
      pMat->m_wTransAngle = 0;
      pMat->m_wTranslucent = 0;
   }

   // loop through all the items
   DWORD i;
   CMaterial MatTemp;
   WORD awTemp[3];
   float afTemp[3];
   TEXTPOINT5 tpText, tpTextMax;
   PRENDERSURFACEPLUS prs;
   if (m_dwType == 1) // water
      prs = m_pGround->m_aWaterRS[dwGroundThread];
   else
      prs = (PRENDERSURFACEPLUS) m_pGround->m_alRENDERSURFACEPLUS[dwGroundThread].Get(0);
   PRENDERSURFACEPLUS prsCur;
   fp fScale;
   for (i = 0; i < pBlend->m_dwNumBlend; i++) {
      prsCur = prs + pBlend->m_padwBlendIndex[i];
      memcpy (&MatTemp, &prsCur->rs.Material, sizeof(MatTemp));
      fScale = pBlend->m_pafBlendAmt[i];

      if (pBlend->m_papBlendText[i]) {
         // texture, so get
         TexturePointConvert (pText, &prsCur->rs, &tpText);
         if (pMax)
            TexturePointConvert (pMax, &prsCur->rs, &tpTextMax);

         // call the function
         pBlend->m_papBlendText[i]->FillPixel (dwThread, dwFlags, pawColor ? awTemp : NULL, &tpText, pMax ? &tpTextMax : NULL,
            &MatTemp, afTemp, fHighQuality);
            // BUGFIX - was always passing in an awTemp, which caused some colors to be
            // cached that weren't cached.. so crash
      }
      else {
         // if it's not a texture then fill in
         Gamma (pBlend->m_papBlendSurf[i]->m_cColor, awTemp);
         afTemp[0] = afTemp[1] = afTemp[2] = 0;
      }

      // average in
      if (dwFlags & TMFP_TRANSPARENCY)
         pMat->m_wTransparency += (WORD)((fp)MatTemp.m_wTransparency * fScale);
      if (dwFlags & TMFP_SPECULARITY) {
         pMat->m_wSpecExponent += (WORD)((fp)MatTemp.m_wSpecExponent * fScale);
         pMat->m_wSpecPlastic += (WORD)((fp)MatTemp.m_wSpecPlastic * fScale);
         pMat->m_wSpecReflect += (WORD)((fp)MatTemp.m_wSpecReflect * fScale);;
      }
      if (dwFlags & TMFP_OTHERMATINFO) {
         pMat->m_fGlow |= MatTemp.m_fGlow;
         //pMat->m_fThickness += MatTemp.m_fThickness * fScale;
         pMat->m_wFill = 0;
         pMat->m_fNoShadows |= MatTemp.m_fNoShadows;
         pMat->m_wIndexOfRefract += (WORD)((fp)MatTemp.m_wIndexOfRefract * fScale);
         pMat->m_wReflectAmount += (WORD)((fp)MatTemp.m_wReflectAmount * fScale);
         pMat->m_wReflectAngle += (WORD)((fp)MatTemp.m_wReflectAngle * fScale);
         pMat->m_wTransAngle += (WORD)((fp)MatTemp.m_wTransAngle * fScale);
         pMat->m_wTranslucent += (WORD)((fp)MatTemp.m_wTranslucent * fScale);
      }

      if (pawColor) {
         pawColor[0] += (WORD)((fp)awTemp[0] * fScale);
         pawColor[1] += (WORD)((fp)awTemp[1] * fScale);
         pawColor[2] += (WORD)((fp)awTemp[2] * fScale);
      }
      if (pafGlow) {
         pafGlow[0] += afTemp[0] * fScale;
         pafGlow[1] += afTemp[1] * fScale;
         pafGlow[2] += afTemp[2] * fScale;
      }
   }
#ifdef USECRITSEC
   LeaveCriticalSection (&m_pGround->m_aCritSec[dwGroundThread]);
#endif
}

/**********************************************************************************
CGroundTexture::PixelBump - As per CTextureMapSocket
*/
BOOL CGroundTexture::PixelBump (DWORD dwThread, PTEXTPOINT5 pText, PTEXTPOINT5 pRight,
                           PTEXTPOINT5 pDown, PTEXTUREPOINT pSlope, fp *pfHeight, BOOL fHighQuality)
{
#ifdef THREADSAFECOPIES
   DWORD dwGroundThread = dwThread;
#ifdef USECRITSEC
   EnterCriticalSection (&m_pGround->m_aCritSec[dwGroundThread]);
#endif
#else
#ifdef USECRITSEC
   DWORD dwGroundThread = GroundThread ();
   EnterCriticalSection (&m_pGround->m_aCritSec[dwGroundThread]);
#endif
#endif //THREADSAFECOPIES
   if (pSlope)
      pSlope->h = pSlope->v = 0;
   if (pfHeight)
      *pfHeight = 0;
   //PCGroundBlend pBlend = &m_Blend;
#ifndef THREADSAFECOPIES
   CGroundBlend Blend;
   PCGroundBlend pBlend = (GetCurrentThreadId() == m_dwThreadID) ? &m_Blend : &Blend;
#else
   PCGroundBlend pBlend = &m_aBlend[dwGroundThread];
#endif
   CalcBlend (pBlend, pText, TRUE, dwGroundThread);

   // loop through all the items
   DWORD i;
   TEXTPOINT5 tpText, tpRight, tpDown;
   TEXTUREPOINT tpTemp;
   fp fHeight;
   BOOL fBump;
   fBump = FALSE;
   PRENDERSURFACEPLUS prs;
   if (m_dwType == 1) // water
      prs = m_pGround->m_aWaterRS[dwGroundThread];
   else
      prs = (PRENDERSURFACEPLUS) m_pGround->m_alRENDERSURFACEPLUS[dwGroundThread].Get(0);
   PRENDERSURFACEPLUS prsCur;
   for (i = 0; i < pBlend->m_dwNumBlend; i++) {
      if (!pBlend->m_papBlendText[i])
         continue;   // only textures will affect

      prsCur = prs + pBlend->m_padwBlendIndex[i];
      TexturePointConvert (pText, &prsCur->rs, &tpText);
      TexturePointConvert (pRight, &prsCur->rs, &tpRight);
      TexturePointConvert (pDown, &prsCur->rs, &tpDown);

      if (!pBlend->m_papBlendText[i]->PixelBump (dwThread, &tpText, &tpRight, &tpDown, pSlope ? &tpTemp : NULL, pfHeight ? &fHeight : NULL, fHighQuality))
         continue;
      fBump = TRUE;
      if (pSlope) {
         pSlope->h += tpTemp.h * pBlend->m_pafBlendAmt[i];
         pSlope->v += tpTemp.v * pBlend->m_pafBlendAmt[i];
      }
      if (pfHeight)
         *pfHeight += fHeight * pBlend->m_pafBlendAmt[i];
   }

#ifdef USECRITSEC
   LeaveCriticalSection (&m_pGround->m_aCritSec[dwGroundThread]);
#endif
   return fBump;
}

/**********************************************************************************
CGroundTexture::FillLine - As per CTextureMapSocket
*/
void CGroundTexture::FillLine (DWORD dwThread, PGCOLOR pac, DWORD dwNum, PTEXTPOINT5 pLeft, PTEXTPOINT5 pRight,
   float *pafGlow, BOOL *pfGlow, WORD *pawTrans, BOOL *pfTrans, fp *pafAlpha)
{
#ifdef THREADSAFECOPIES
   DWORD dwGroundThread = dwThread;
#ifdef USECRITSEC
   EnterCriticalSection (&m_pGround->m_aCritSec[dwGroundThread]);
#endif
#else
#ifdef USECRITSEC
   DWORD dwGroundThread = GroundThread ();
   EnterCriticalSection (&m_pGround->m_aCritSec[dwGroundThread]);
#endif
#endif // THREADSAFECOPIES
   DWORD dwNumSurf;
   //PCGroundBlend pBlend = &m_Blend;
#ifndef THREADSAFECOPIES
   CGroundBlend Blend;
   PCGroundBlend pBlend = (GetCurrentThreadId() == m_dwThreadID) ? &m_Blend : &Blend;
#else
   PCGroundBlend pBlend = &m_aBlend[dwGroundThread];
#endif
   if (m_dwType == 1)   // water
      dwNumSurf = GWATERNUM * GWATEROVERLAP;
   else
      dwNumSurf = m_pGround->m_lTextPCObjectSurface.Num();

   DWORD dwNeed;
   dwNeed =dwNumSurf * dwNum * sizeof(fp) + dwNumSurf * sizeof(BOOL) +
      dwNum * (sizeof(GCOLOR) + 3 * sizeof(float)) + dwNum * sizeof(WORD);
   dwNeed += dwNeed / 4;   // BUGFIX - Had a memory overrun when ray tracing with multiple threads
         // Cant quite see where problem is so just adding a bit of extra memory
	MALLOCOPT_INIT;
	MALLOCOPT_OKTOMALLOC;
   if (!pBlend->m_memFillLine.Required (dwNeed)) {
   	MALLOCOPT_RESTORE;
#ifdef USECRITSEC
      LeaveCriticalSection (&m_pGround->m_aCritSec[dwGroundThread]);
#endif
      return;
   }
	MALLOCOPT_RESTORE;
   fp *pafWeight; // [x + y * dwNum], x = 0..dwNum-1, y=dwNumSurf-1
   pafWeight = (fp*) pBlend->m_memFillLine.p;
   memset (pafWeight, 0, sizeof(fp) * dwNum * dwNumSurf);
   BOOL *pafUse;  // 0..dwNumSurf-1, saying if use that surface
   pafUse = (BOOL*) (pafWeight + dwNumSurf * dwNum);
   memset (pafUse, 0, sizeof(BOOL) * dwNumSurf);
   PGCOLOR pgcTemp;
   pgcTemp = (PGCOLOR) (pafUse + dwNumSurf);
   float *pfTemp;
   pfTemp = (float*) (pgcTemp + dwNum);
   WORD *pwTempTrans;
   pwTempTrans = (WORD*) (pfTemp + dwNum * 3);

   // initialize
   memset (pac, 0, sizeof(GCOLOR) * dwNum);
   memset (pafGlow, 0, sizeof(float) * 3 * dwNum);
   *pfGlow = FALSE;
   if (pawTrans)
      memset (pawTrans, 0, sizeof(WORD) * dwNum);
   if (pfTrans)
      *pfTrans = FALSE;

   // figure out weights for every surface combined with every pixel
   DWORD i, j;
   TEXTPOINT5 tpLeft, tpRight;
   fp fScale;
   for (i = 0; i < dwNum; i++) {
      // if only one surface then this is really fast
      if (dwNumSurf <= 1) {
         pafUse[0] = TRUE;
         pafWeight[i + dwNum * 0] = 1.0;
         continue;
      }

      if (pafAlpha) {
         for (j = 0; j < 2; j++)
            tpLeft.hv[j] = pLeft->hv[j] + pafAlpha[i] * (pRight->hv[j] - pLeft->hv[j]);
         for (j = 0; j < 2; j++)
            tpLeft.xyz[j] = pLeft->xyz[j] + pafAlpha[i] * (pRight->xyz[j] - pLeft->xyz[j]);
      }
      else {
         fScale = (fp) i / (fp)dwNum;
         for (j = 0; j < 2; j++)
            tpLeft.hv[0] = pLeft->hv[0] + fScale * (pRight->hv[0] - pLeft->hv[0]);
         for (j = 0; j < 3; j++)
            tpLeft.xyz[0] = pLeft->xyz[0] + fScale * (pRight->xyz[0] - pLeft->xyz[0]);
      }

      CalcBlend (pBlend, &tpLeft, FALSE, dwGroundThread);

      // see which ones are activated
      for (j = 0; j < pBlend->m_dwNumBlend; j++) {
         if (!pBlend->m_pafBlendAmt[j])
            continue;   // if 0 ignore
         pafUse[pBlend->m_padwBlendIndex[j]] = TRUE;
         pafWeight[i + dwNum * pBlend->m_padwBlendIndex[j]] = pBlend->m_pafBlendAmt[j];
      }
   }

   // loop
   BOOL fGlow, fTrans;
   PCTextureMapSocket pMap;
   PRENDERSURFACEPLUS prs;
   PCObjectSurface *ppos;
   if (m_dwType == 1) { // water
      prs = m_pGround->m_aWaterRS[dwGroundThread];
      ppos = m_pGround->m_apWaterSurf;
   }
   else {
      prs = (PRENDERSURFACEPLUS) m_pGround->m_alRENDERSURFACEPLUS[dwGroundThread].Get(0);
      ppos = (PCObjectSurface*) m_pGround->m_lTextPCObjectSurface.Get(0);
   }

   // BUGFIX - Call calcblend to make sure have all surfaces
   CalcBlend (pBlend, dwGroundThread);

   for (i = 0; i < dwNumSurf; i++) {
      if (!pafUse[i])
         continue;   // not used

      if (prs[i].rs.fUseTextureMap)
         pMap = prs[i].pTexture = ::TextureCacheGet (m_dwRenderShard, &prs[i].rs, &prs[i].qwTextureCacheTimeStamp, prs[i].pTexture);
      else
         pMap = NULL;

      if (pBlend->m_papBlendText[i]) { // texture
         TexturePointConvert (pLeft, &prs[i].rs, &tpLeft);
         TexturePointConvert (pRight, &prs[i].rs, &tpRight);

         pMap->FillLine (dwThread, pgcTemp, dwNum, &tpLeft, &tpRight, pfTemp, &fGlow,
            pwTempTrans, &fTrans, pafAlpha);
      }
      else { // solid color
         fGlow = FALSE;
         fTrans = FALSE;
         WORD awColor[3];
         Gamma (ppos[i]->m_cColor, awColor);

         for (j = 0; j < dwNum; j++) {
            pgcTemp[j].wRed = awColor[0];
            pgcTemp[j].wGreen = awColor[1];
            pgcTemp[j].wBlue = awColor[2];
         }
      }
      
      // BUGFIX - If material transparent, but not saying provided traparency then
      // fake it
      if (prs[i].rs.Material.m_wTransparency && !fTrans) {
         fTrans = TRUE;
         for (j = 0; j < dwNum; j++)
            pwTempTrans[j] = prs[i].rs.Material.m_wTransparency;
      }

      for (j = 0; j < dwNum; j++) {
         fScale = pafWeight[j + i * dwNum];
         if (!fScale)
            continue;

         pac[j].wRed += (WORD)((fp)pgcTemp[j].wRed * fScale);
         pac[j].wGreen += (WORD)((fp)pgcTemp[j].wGreen * fScale);
         pac[j].wBlue += (WORD)((fp)pgcTemp[j].wBlue * fScale);

         if (fGlow) {
            pafGlow[j*3+0] = pfTemp[j*3+0] * fScale;
            pafGlow[j*3+1] = pfTemp[j*3+1] * fScale;
            pafGlow[j*3+2] = pfTemp[j*3+2] * fScale;
         }

         if (fTrans)
            pawTrans[j] += (WORD)((fp)pwTempTrans[j] * fScale);
      }

      *pfGlow |= fGlow; // BUGFIX - was just straight equals
      *pfTrans |= fTrans;  // BUGFIX - was just straight equals
   }
#ifdef USECRITSEC
   LeaveCriticalSection (&m_pGround->m_aCritSec[dwGroundThread]);
#endif
}


/**********************************************************************************
CObjectGround::Constructor and destructor
*/
CObjectGround::CObjectGround (PVOID pParams, POSINFO pInfo)
{
   m_dwType = (DWORD)(size_t) pParams;
   m_OSINFO = *pInfo;
   m_dwRenderShow = RENDERSHOW_GROUND;

   GUIDGen (&m_gTextGround);
   GUIDGen (&m_gTextWater);

   DWORD i;
#ifdef USECRITSEC
   for (i = 0; i < GROUNDTHREADS; i++)
      InitializeCriticalSection (&m_aCritSec[i]);
#endif

   m_fWater = FALSE;
   m_fWaterSize = 10000;   // BUGFIX - Make smaller to reduce round-off error
   m_fWaterElevation = 0;
   m_fLessDetail = TRUE;
   m_fForestBoxes = TRUE;
   m_fSaveCompressed = TRUE;
   m_fDontDrawNearTrees = TRUE;
   m_lOGCUTOUT.Init (sizeof(OGCUTOUT));
   m_lOGGRID.Init (sizeof(OGGRID));
   m_lGROUNDSEC.Init (sizeof(GROUNDSEC));

   // BUGFIX - Use less detail in default ground so draws faster
   //m_dwWidth = m_dwHeight = 129;
   //m_fScale = .5;
   m_dwWidth = m_dwHeight = 65;
   m_fScale = 1;
   m_tpElev.h = -10;
   m_tpElev.v = 30;
   m_memElev.Required (m_dwWidth * m_dwHeight * sizeof(WORD));
   m_pawElev = (WORD*) m_memElev.p;

   // texture
   m_memTextSet.Required (m_dwWidth * m_dwHeight * sizeof(BYTE) * 1);   // since start out w/one texture
   m_pabTextSet = (BYTE*) m_memTextSet.p;
   m_lTextPCObjectSurface.Init (sizeof(PCObjectSurface));
   m_lTextCOLORREF.Init (sizeof(COLORREF));
   // fill in the texture
   memset (m_pabTextSet, 0xff, m_dwWidth * m_dwHeight * sizeof(BYTE));

   // forest
   // dont init anything m_memForest.Required (m_dwWidth * m_dwHeight * sizeof(BYTE));
   m_pabForestSet = NULL;  // no forst at first(BYTE*) m_memForestSet.p;
   m_lPCForest.Init (sizeof(PCForest));
   //memset (m_pabForest, 0, m_dwWidth * m_dwHeight * sizeof(BYTE));

   // water
   PCObjectSurface pos;
   memset (m_apWaterSurf, 0 ,sizeof(m_apWaterSurf));
   memset (m_afWaterElev, 0, sizeof(m_afWaterElev));
   m_apWaterSurf[0] = pos = new CObjectSurface;
   pos->m_cColor = RGB(0x40,0x40,0xff);
   pos->m_fUseTextureMap = TRUE;
   pos->m_Material.InitFromID (MATERIAL_WATERSHALLOW);
   pos->m_gTextureCode = GTEXTURECODE_ShallowWater;
   pos->m_gTextureSub = GTEXTURESUB_ShallowWater;
   m_apWaterSurf[1] = pos = pos->Clone();
   pos->m_Material.InitFromID (MATERIAL_WATERPOOL);
   pos->m_gTextureCode = GTEXTURECODE_PoolWater;
   pos->m_gTextureSub = GTEXTURESUB_PoolWater;
   m_apWaterSurf[2] = pos = pos->Clone();
   pos->m_Material.InitFromID (MATERIAL_WATERDEEP);
   pos->m_gTextureCode = GTEXTURECODE_LakeWater;
   pos->m_gTextureSub = GTEXTURESUB_LakeWater;

   // BUGFIX - Also add the same larger water
   DWORD x, y;
   CMatrix mOrig, mRot, mScale;
   mRot.RotationZ (PI/11);
   mScale.Scale (1.0 / 1.68, 1.0 / 1.68, 1.0 / 1.68);
   for (i = 3; i < 6; i++) {
      m_apWaterSurf[i] = m_apWaterSurf[i-3]->Clone();
      
      mOrig.Identity();
      for (x = 0; x < 2; x++) for (y = 0; y < 2; y++)
         mOrig.p[x][y] = m_apWaterSurf[i]->m_afTextureMatrix[x][y];

      mOrig.MultiplyRight (&mRot);
      mOrig.MultiplyRight (&mScale);

      for (x = 0; x < 2; x++) for (y = 0; y < 2; y++)
         m_apWaterSurf[i]->m_afTextureMatrix[x][y] = mOrig.p[x][y];

      // adjust the 3d texture too
      PCMatrix pm = &m_apWaterSurf[i]->m_mTextureMatrix;
      pm->MultiplyRight (&mRot);
      pm->MultiplyRight (&mScale);
   }

   m_afWaterElev[0] = 1;
   m_afWaterElev[1] = 3;
   m_afWaterElev[2] = 10;

   // start out with an initial surface
   //PCObjectSurface pos;
   pos = new CObjectSurface;
   pos->m_cColor = RGB(0x40,0xff,0x40);
   pos->m_Material.InitFromID (MATERIAL_FLAT);
   pos->m_fUseTextureMap = TRUE;
   pos->m_gTextureCode = GTEXTURECODE_Grass;
   pos->m_gTextureSub = GTEXTURESUB_Grass;
   if (pos->m_fUseTextureMap) {
      RENDERSURFACE rs;
      memset (&rs, 0, sizeof(rs));
      rs.fUseTextureMap = TRUE;
      rs.gTextureCode = pos->m_gTextureCode;
      rs.gTextureSub = pos->m_gTextureSub;
      rs.TextureMods = pos->m_TextureMods;
      memcpy (&rs.afTextureMatrix, &pos->m_afTextureMatrix, sizeof(pos->m_afTextureMatrix));
      memcpy (rs.abTextureMatrix, &pos->m_mTextureMatrix, sizeof(pos->m_mTextureMatrix));

      PCTextureMapSocket pm;
      pm = TextureCacheGet (m_OSINFO.dwRenderShard, &rs, NULL, NULL);
      if (pm) {
         fp fx, fy;
         pm->DefScaleGet (&fx, &fy);
         pos->m_afTextureMatrix[0][0] = 1.0 / fx;
         pos->m_afTextureMatrix[1][1] = 1.0 / fy;
         pm->MaterialGet (0, &pos->m_Material); // BUGFIX - Get material characteristics from texture
         TextureCacheRelease (m_OSINFO.dwRenderShard, pm);
      }
   }
   m_lTextCOLORREF.Add (&pos->m_cColor);
   m_lTextPCObjectSurface.Add (&pos);
   FillInRENDERSURFACE();

   // fill in elevation...
   for (y = 0; y < m_dwWidth; y++) for (x = 0; x < m_dwWidth; x++) {
      m_pawElev[x + y * m_dwWidth] = 0x4000;
   }
   m_fNormalsDirty = m_fBBDirty = TRUE;


   

   // Not longer need the grass and water because handled by custom textures
   //ObjectSurfaceAdd (SIDEA, RGB(0x40,0xff,0x40), MATERIAL_FLAT, L"Grass",
   //   &GTEXTURECODE_Grass, &GTEXTURESUB_Grass);
   //ObjectSurfaceAdd (WATER, RGB(0x80, 0x80, 0xff), MATERIAL_GLASSCLEAR, L"Water",
   //   &GTEXTURECODE_LakeWater, &GTEXTURESUB_LakeWater);

   // add for embedding
   ContainerSurfaceAdd (SIDEA);

}


CObjectGround::~CObjectGround (void)
{
   // free up the memory for textures
   DWORD i;
   PCObjectSurface *ppos = (PCObjectSurface*) m_lTextPCObjectSurface.Get(0);
   for (i = 0; i < m_lTextPCObjectSurface.Num(); i++)
      delete ppos[i];

   // free up the forests
   PCForest *ppf = (PCForest*) m_lPCForest.Get(0);
   for (i = 0; i < m_lPCForest.Num(); i++)
      delete ppf[i];

   // free up mmeory for cutouts
   POGCUTOUT pc = (POGCUTOUT) m_lOGCUTOUT.Get(0);
   for (i = 0; i < m_lOGCUTOUT.Num(); i++, pc++) {
      if (pc->plTEXTUREPOINT)
         delete pc->plTEXTUREPOINT;
   }
   m_lOGCUTOUT.Clear();

   // free up grid
   POGGRID pg = (POGGRID) m_lOGGRID.Get(0);
   for (i = 0; i < m_lOGGRID.Num(); i++, pg++) {
      if (pg->pGrid)
         delete pg->pGrid;
   }
   m_lOGGRID.Clear();

   // free up the water
   for (i = 0; i < GWATERNUM * GWATEROVERLAP; i++)
      if (m_apWaterSurf[i])
         delete m_apWaterSurf[i];

   // free texture from cache
   TextureCacheDelete (m_OSINFO.dwRenderShard, &m_gTextGround, &m_gTextGround);
   TextureCacheDelete (m_OSINFO.dwRenderShard, &m_gTextWater, &m_gTextWater);

#ifdef USECRITSEC
   for (i = 0; i < GROUNDTHREADS; i++)
      DeleteCriticalSection (&m_aCritSec[i]);
#endif
}


/**********************************************************************************
CObjectGround::Delete - Called to delete this object
*/
void CObjectGround::Delete (void)
{
   delete this;
}

/**********************************************************************************
CObjectGround::QuerySubObjects - From CObjectSocket
*/
DWORD CObjectGround::QuerySubObjects (void)
{
   CalcBoundingBoxes ();

   return m_lGROUNDSEC.Num() * NUMSUBOJECTPERGOUNDSEC + (m_fWater ? 1 : 0);
}


/**********************************************************************************
CObjectGround::QueryBoundingBox - From CObjectSocket
*/
void CObjectGround::QueryBoundingBox (PCPoint pCorner1, PCPoint pCorner2, DWORD dwSubObject)
{
   CalcBoundingBoxes ();

   PGROUNDSEC pgs;
   pgs = (PGROUNDSEC) m_lGROUNDSEC.Get(0);

   // determine the water's bounding box
   // draw water
   CPoint apWater[2];
   if (m_fWater) {
      apWater[0].Zero();
      if (m_fWaterSize > 0)
         apWater[0].p[0] = apWater[0].p[1] = -m_fWaterSize/2;
      else {
         XYToLoc (0, 0, &apWater[0]);
         apWater[0].p[0] = -fabs(apWater[0].p[0]);
         apWater[0].p[1] = -fabs(apWater[0].p[1]);
      }
      apWater[0].p[2] = m_fWaterElevation;
      apWater[1].Copy (&apWater[0]);
      apWater[1].Scale (-1);
      apWater[1].p[2] = m_fWaterElevation;
   }


   if (dwSubObject != -1) {

      if (dwSubObject >= m_lGROUNDSEC.Num() * NUMSUBOJECTPERGOUNDSEC) {
         // too high. assuming that asking for water
         pCorner1->Copy (&apWater[0]);
         pCorner2->Copy (&apWater[1]);
         return;
      }

      DWORD dwSubRendering = dwSubObject % NUMSUBOJECTPERGOUNDSEC;

      if (dwSubRendering) {
         // it's forest, so make sure there's a canopy setting for the specific one
         // loop through all the forests
         DWORD dwForest;
         PCForest *ppf;
         ppf = (PCForest*) m_lPCForest.Get(0);
         BOOL fFoundCanopy = FALSE;
         for (dwForest = 0; !fFoundCanopy && (dwForest < m_lPCForest.Num()); dwForest++) {
            // go through all the canopies
            DWORD dwCanopy = dwSubRendering-1;
            if (ppf[dwForest]->m_apCanopy[dwCanopy]->m_lCANOPYTREE.Num()) {
               fFoundCanopy = TRUE;
               break;
            }
         } // dwForest

         if (!fFoundCanopy) {
            // no actual bounding box
            pCorner1->Copy (&pgs[dwSubObject / NUMSUBOJECTPERGOUNDSEC].pBoundMin);
            pCorner2->Copy (pCorner1);
            return;
         }

      }

      pCorner1->Copy (&pgs[dwSubObject / NUMSUBOJECTPERGOUNDSEC].pBoundMin);
      pCorner2->Copy (&pgs[dwSubObject / NUMSUBOJECTPERGOUNDSEC].pBoundMax);
      return;
   }

   // else, combination of all ground secs
   DWORD i;
   for (i = 0; i < m_lGROUNDSEC.Num(); i++, pgs++) {
      if (i) {
         pCorner1->Min(&pgs->pBoundMin);
         pCorner2->Max(&pgs->pBoundMax);
      }
      else {
         pCorner1->Copy(&pgs->pBoundMin);
         pCorner2->Copy(&pgs->pBoundMax);
      }
   }

   // potentially add water
   if (m_fWater) {
      pCorner1->Min (&apWater[0]);
      pCorner2->Max (&apWater[1]);
   }
}

static int _cdecl GROUNDSECCompare (const void *elem1, const void *elem2)
{
   GROUNDSEC *pdw1, *pdw2;
   pdw1 = (GROUNDSEC*) elem1;
   pdw2 = (GROUNDSEC*) elem2;

   if (pdw1->fDist < pdw2->fDist)
      return -1;
   else if (pdw1->fDist > pdw2->fDist)
      return 1;
   else
      return 0;
}

/**********************************************************************************
CObjectGround::Render - Draws a test box.

inputs
   POBJECTRENDER     pr - Render information
*/
void CObjectGround::Render (POBJECTRENDER pr, DWORD dwSubObject)
{
	MALLOCOPT_INIT;
   MALLOCOPT_ASSERT;

   // make sure stuff calculated
   CalcNormals ();
   CalcBoundingBoxes ();
   FillInRENDERSURFACE();

   // fill in the materials for the ground and water
   RENDERSURFACE rsGround, rsWater;
   CMatrix mIdent;
   mIdent.Identity();
   memset (&rsGround, 0, sizeof(rsGround));
   rsGround.wMinorID = SIDEA;
   rsGround.afTextureMatrix[0][0] = rsGround.afTextureMatrix[1][1] = 1;
   rsGround.afTextureMatrix[0][1] = rsGround.afTextureMatrix[1][0] = 0;
   memcpy (rsGround.abTextureMatrix, &mIdent, sizeof(mIdent));
   rsGround.fUseTextureMap = TRUE;
   rsGround.gTextureCode = m_gTextGround;
   rsGround.gTextureSub = m_gTextGround;
   rsGround.Material.InitFromID (MATERIAL_FLAT);
   rsGround.TextureMods.cTint = RGB(0xff,0xff,0xff);
   rsGround.TextureMods.wBrightness = 0x1000;
   rsGround.TextureMods.wContrast = 0x1000;
   rsGround.TextureMods.wHue = 0x0000;
   rsGround.TextureMods.wSaturation = 0x1000;
   rsGround.Material.m_fGlow = FALSE;
   rsWater = rsGround;
   rsWater.gTextureCode = m_gTextWater;
   rsWater.gTextureSub = m_gTextWater;
   rsWater.Material.InitFromID (MATERIAL_WATERDEEP);
   rsWater.wMinorID = WATER;

   DWORD dwRenderShard = m_OSINFO.dwRenderShard;

   // get the texures to make sure they're created
   PCTextureMapSocket pMap;
   COLORREF cGround, cWater;
   pMap = TextureCacheGetDynamic (dwRenderShard, &m_gTextGround, &m_gTextGround, NULL, NULL);
   if (!pMap) {
      pMap = new CGroundTexture (&m_gTextGround, &m_gTextGround, this, 0);
      pMap->TextureModsSet (&rsGround.TextureMods);   // so find when look for this
      TextureCacheAddDynamic (dwRenderShard, pMap);
   }
   cGround = pMap ? pMap->AverageColorGet (0, FALSE) : 0;
   if (pMap)
      pMap->MaterialGet (0, &rsGround.Material);   // do this so if transparent wont cast shadow

   MALLOCOPT_ASSERT;

   if (m_fWater) {
      pMap = TextureCacheGetDynamic (dwRenderShard, &m_gTextWater, &m_gTextWater, NULL, NULL);
      if (!pMap) {
         pMap = new CGroundTexture (&m_gTextWater, &m_gTextWater, this, 1);
         pMap->TextureModsSet (&rsWater.TextureMods); // so find when look for this
         TextureCacheAddDynamic (dwRenderShard, pMap);
      }
      cWater = pMap ? pMap->AverageColorGet (0, FALSE) : 0;
      if (pMap)
         pMap->MaterialGet (0, &rsWater.Material);   // do this so if transparent wont cast shadow
   }
   MALLOCOPT_ASSERT;

   // make a list of all the points that plan to render
   // CListFixed lRender;
   DWORD dwSubRendering = (DWORD)-1;  // 0 for ground, 1+ for forest canopy, -1 for all
   m_lGroundRender.Init (1);  // just to clear, just in case
	MALLOCOPT_OKTOMALLOC;
   if (dwSubObject == -1)
      m_lGroundRender.Init (sizeof(GROUNDSEC), m_lGROUNDSEC.Get(0), m_lGROUNDSEC.Num());
   else if (dwSubObject < m_lGROUNDSEC.Num() * NUMSUBOJECTPERGOUNDSEC) {
      m_lGroundRender.Init (sizeof(GROUNDSEC), m_lGROUNDSEC.Get(dwSubObject / NUMSUBOJECTPERGOUNDSEC), 1);

#ifdef USENUMSUBOJECTPERGOUNDSEC
      dwSubRendering = dwSubObject % NUMSUBOJECTPERGOUNDSEC;
#else
      dwSubRendering = (DWORD)-1;
#endif
   }
	MALLOCOPT_RESTORE;

   MALLOCOPT_ASSERT;

   // Sort the sections by closest to furthest away
   PGROUNDSEC pgs;
   DWORD i, j;
   if (pr->fCameraValid && (dwSubObject == (DWORD)-1)) {
      CPoint p;
      pgs = (PGROUNDSEC) m_lGroundRender.Get(0);
      for (i = 0; i < m_lGroundRender.Num(); i++, pgs++) {
         // if camera if within the bounding box then distance is zero
         for (j = 0; j < 3; j++)
            if ((pr->pCamera.p[j] < pgs->pBoundMin.p[j]) || (pr->pCamera.p[j] > pgs->pBoundMax.p[j]))
               break;
         if (j >= 3) {
            pgs->fDist = 0;
            continue;
         }

         // else, average distance
         p.Average (&pgs->pBoundMax, &pgs->pBoundMin);
         p.Subtract (&pr->pCamera);
         pgs->fDist = p.Length();
      }

      // sort
      qsort (m_lGroundRender.Get(0), m_lGroundRender.Num(), sizeof(GROUNDSEC), GROUNDSECCompare);
   }

   MALLOCOPT_ASSERT;

   // loop through all the sections
   fp fDetail, fDetailTotal;
   pgs = (PGROUNDSEC) m_lGroundRender.Get(0);
   fDetailTotal = pr->pRS->QueryDetail();
   for (i = 0; i < m_lGroundRender.Num(); i++, pgs++) {
      fDetail = fDetailTotal;

      // See if should be clipped straiged out
      if (dwSubObject == (DWORD)-1) {
         if (!pr->pRS->QuerySubDetail (&m_MatrixObject, &pgs->pBoundMin, &pgs->pBoundMax, &fDetail))
            continue;
      }
      //else
      //   fDetail = fDetailTotal;

      // draw it
      if (!dwSubRendering || (dwSubRendering == (DWORD)-1)) {
         MALLOCOPT_ASSERT;
         RenderSection (pgs->dwXMin, pgs->dwXMax, pgs->dwYMin, pgs->dwYMax,
            pr, fDetail, cGround, &rsGround);
         MALLOCOPT_ASSERT;
      }

      // draw the trees
      BOOL fForestBoxes;
      fForestBoxes = m_fForestBoxes && (pr->dwReason != ORREASON_FINAL) && (pr->dwReason != ORREASON_SHADOWS);
         // BUGFIX - If calculating shadows then DONT want forest boxes

      BOOL fIndividualTrees;
      BOOL fTreeMightBeHidden = FALSE;
      if (dwSubObject == (DWORD) -1)
         fIndividualTrees = !pgs->fDist;  // BUGFIX - Was ((i < 4) && !fForestBoxes);
      else {
         fIndividualTrees = (fDetail ? FALSE : TRUE);

         // if know where camera is then use that to measure
         if (pr->fCameraValid) {
            // if camera if within the bounding box then distance is zero
            for (j = 0; j < 3; j++)
               if ((pr->pCamera.p[j] < pgs->pBoundMin.p[j]) || (pr->pCamera.p[j] > pgs->pBoundMax.p[j]))
                  break;
            if (j >= 3) {
               fIndividualTrees = TRUE;
               fTreeMightBeHidden = TRUE;
            }
         }
      }

      if (dwSubRendering || (dwSubRendering == (DWORD)-1) ) {
	      MALLOCOPT_ASSERT;

         RenderTrees (pgs->dwXMin, pgs->dwXMax, pgs->dwYMin, pgs->dwYMax,
            pr,
            fIndividualTrees ? -1 : fDetail,
            fForestBoxes,
            (fTreeMightBeHidden && pr->fCameraValid && m_fDontDrawNearTrees) ? &pr->pCamera : NULL,
            (dwSubRendering == (DWORD)-1) ? dwSubRendering : (dwSubRendering - 1));

         // BUGFIX - just to see if trees are messing up render malloc test
	      MALLOCOPT_ASSERT;
      }
      // BUGFIX - For 4 closest sections, go tree-by-tree and ask detail. Need to do
      // this since if one of the closest sections may be inhabited by the camera
      // and will always have a detail of 0, drawing every tree
   }

   // draw water
   if (m_fWater && ((dwSubObject == (DWORD)-1) || (dwSubObject >= m_lGROUNDSEC.Num() * NUMSUBOJECTPERGOUNDSEC)) ) {
      MALLOCOPT_ASSERT;

      // CRenderSurface rs;
      m_lRenderrs.ClearAll();
      CMatrix mObject;
      m_lRenderrs.Init (pr->pRS);
      ObjectMatrixGet (&mObject);
      m_lRenderrs.Multiply (&mObject);
      //m_lRenderrs.SetDefMaterial (ObjectSurfaceFind (WATER), m_pWorld);
      m_lRenderrs.SetDefColor (cWater);
      m_lRenderrs.SetDefMaterial (&rsWater);

      CPoint pUL;
      XYToLoc (0, 0, &pUL);

      DWORD dwPoints, dwTextures;
      PCPoint pap = m_lRenderrs.NewPoints (&dwPoints, 4);
      DWORD dwNormal = m_lRenderrs.NewNormal (0, 0, 1, TRUE);
      PTEXTPOINT5 ptp = m_lRenderrs.NewTextures (&dwTextures, 4);

      if (pap) {
         pap[0].Zero();
         if (m_fWaterSize > 0) {
            pap[0].p[0] = -m_fWaterSize/2;
            pap[0].p[1] = m_fWaterSize/2;
         }
         else {
            XYToLoc (0, 0, &pap[0]);
            //pap[0].p[0] = -(fp)(m_dwWidth-1) * m_fScale / 2.0;
            //pap[0].p[1] = (fp)(m_dwHeight-1) * m_fScale / 2.0;
         }
         pap[0].p[2] = m_fWaterElevation;
         pap[1].Copy (&pap[0]);
         pap[1].p[0] *= -1;
         pap[2].Copy (&pap[1]);
         pap[2].p[1] *= -1;
         pap[3].Copy (&pap[2]);
         pap[3].p[0] *= -1;

         DWORD i;
         if (ptp) {
            for (i = 0; i < 4; i++) {
               ptp[i].hv[0] = (pap[i].p[0] - pUL.p[0]) / m_fScale;
               ptp[i].hv[1] = (pUL.p[1] - pap[i].p[1]) / m_fScale;

               ptp[i].xyz[0] = pap[i].p[0];
               ptp[i].xyz[1] = pap[i].p[1];
               ptp[i].xyz[2] = pap[i].p[2];
            }
            // Wont want to apply rotation when get water all working
            //m_lRenderrs.ApplyTextureRotation (ptp, 4);
         }

         m_lRenderrs.NewQuad (m_lRenderrs.NewVertex (dwPoints+0, dwNormal, ptp ? (dwTextures+0) : 0),
            m_lRenderrs.NewVertex (dwPoints+1, dwNormal, ptp ? (dwTextures+1) : 0),
            m_lRenderrs.NewVertex (dwPoints+2, dwNormal, ptp ? (dwTextures+2) : 0),
            m_lRenderrs.NewVertex (dwPoints+3, dwNormal, ptp ? (dwTextures+3) : 0) );
      }

      m_lRenderrs.Commit();

      // BUGFIX - just to see if trees are messing up render malloc test
      MALLOCOPT_ASSERT;
   }

   MALLOCOPT_ASSERT;
}


/**********************************************************************************
CObjectGround::XYToLoc - Given an X and Y, fills in the .p[0] and .p[1] values
of the elevation.

inputs
   fp          fX, fY - x and Y. Assumed to be within m_dwWidth and m_dwHeight. Dont have to be
   PCPoint     pElev - Filled with elevation
returns
   none
*/
void CObjectGround::XYToLoc (fp fX, fp fY, PCPoint pElev)
{
   pElev->p[0] = (fX - (fp)(m_dwWidth-1)/2) * m_fScale;
   pElev->p[1] = -(fY - (fp)(m_dwHeight-1)/2) * m_fScale;
   pElev->p[3] = 1;
}

/**********************************************************************************
CObjectGround::Elevation - Fills in the elevation given and X and Y.

inputs
   DWORD       dwX, dwY - x and Y. Assumed to be within m_dwWidth and m_dwHeight
   PCPoint     pElev - Filled with elevation
returns
   none
*/
void CObjectGround::Elevation (DWORD dwX, DWORD dwY, PCPoint pElev)
{
   XYToLoc (dwX, dwY, pElev);
   pElev->p[2] = (fp)m_pawElev[dwX + dwY * m_dwWidth] / (fp)0xffff * (m_tpElev.v - m_tpElev.h) +
      m_tpElev.h;
}


/**********************************************************************************
CObjectGround::CalcBoundingBoxes - Calculates the bounding boxes for smaller portions
of the ground. These sub-sections are used for optimizations.

Fills in m_memBBMin and m_memBBMax.
*/
void CObjectGround::CalcBoundingBoxes (void)
{
   if (!m_fBBDirty)
      return;
   m_fBBDirty = FALSE;

   // how many sub-bb
   DWORD dwBBWidth, dwBBHeight;
   dwBBWidth = (m_dwWidth + BBSIZE - 1) / BBSIZE;
   dwBBHeight = (m_dwHeight + BBSIZE - 1) / BBSIZE;

   // allocate the memory
   m_lGROUNDSEC.Clear();

   // figure out the maximum size for any tree
   fp fMaxTree;
   fMaxTree = 0;
   DWORD i, j;
   PCForest *ppf;
   ppf = (PCForest*) m_lPCForest.Get(0);
   for (i = 0; i < m_lPCForest.Num(); i++) {
      for (j = 0; j < NUMFORESTCANOPIES; j++) {
         fMaxTree = max(fMaxTree, ppf[i]->m_apCanopy[j]->MaxTreeSize(m_OSINFO.dwRenderShard));
      }
   }

   // loop over all the bounding boxes
   DWORD x, y, xx, yy;
   GROUNDSEC gc;
   memset (&gc, 0, sizeof(gc));
   for (y = 0; y < dwBBHeight; y++) for (x = 0; x < dwBBWidth; x++) {
      // find the min/max height
      WORD wMin, wMax;
      wMin = 0xffff;
      wMax = 0;

      // bounding box
      gc.dwXMin = x * BBSIZE;
      gc.dwXMax = min((x+1)*BBSIZE, m_dwWidth-1);
      gc.dwYMin = y * BBSIZE;
      gc.dwYMax = min((y+1)*BBSIZE, m_dwHeight-1);

      if ((gc.dwXMax <= gc.dwXMin) || (gc.dwYMax <= gc.dwYMin))
         continue;

      for (yy = gc.dwYMin; yy < gc.dwYMax; yy++)
         for (xx = gc.dwXMin; xx < gc.dwXMax; xx++) {
            wMin = min(wMin, m_pawElev[xx + yy * m_dwWidth]);
            wMax = max(wMax, m_pawElev[xx + yy * m_dwWidth]);
         }
      
      // set the points
      XYToLoc (gc.dwXMin, gc.dwYMax, &gc.pBoundMin);
      XYToLoc (gc.dwXMax, gc.dwYMin, &gc.pBoundMax);
      gc.pBoundMin.p[2] = (fp)wMin / (fp)0xffff * (m_tpElev.v - m_tpElev.h) + m_tpElev.h;
      gc.pBoundMax.p[2] = (fp)wMax / (fp)0xffff * (m_tpElev.v - m_tpElev.h) + m_tpElev.h;
      gc.pBoundMax.p[2] = max(gc.pBoundMin.p[2] + CLOSE, gc.pBoundMax.p[2]);  // always at least some distance

      // BUGFIX - A bit extra so that won't be infinitely wafer thin
      fp fLen;
      CPoint p;
      p.Subtract (&gc.pBoundMin, &gc.pBoundMax);
      fLen = p.Length() / 100.0;
      gc.pBoundMin.p[2] -= fLen;
      gc.pBoundMax.p[2] += fLen;

      // include trees...
      for (i = 0; i < 3; i++) {
         gc.pBoundMin.p[i] -= fMaxTree;
         gc.pBoundMax.p[i] += fMaxTree;
      }

      // add this
      m_lGROUNDSEC.Add (&gc);
   }
}

/**********************************************************************************
CObjectGround::CalcNormals - If the m_fNormalsDirty flag is set, this recalculates
all the normals
*/
void CObjectGround::CalcNormals (void)
{
   if (!m_fNormalsDirty)
      return;
   m_fNormalsDirty = FALSE;

   // allocate the memory
   if (!m_memNormals.Required (m_dwWidth * m_dwHeight * sizeof(CPoint)))
      return;  // error
   PCPoint paNormals;
   paNormals = (PCPoint) m_memNormals.p;

   // loop
   DWORD x, y;
   CPoint pE, pW, pN, pS;
   for (y = 0; y < m_dwHeight; y++) for (x = 0; x < m_dwWidth; x++, paNormals++) {
      Elevation (min(x+1,m_dwWidth-1), y, &pE);
      Elevation (x ? (x-1) : 0, y, &pW);
      Elevation (x, min(y+1,m_dwHeight-1), &pS);
      Elevation (x, y ? (y-1) : 0, &pN);

      pE.Subtract (&pW);
      pS.Subtract (&pN);

      paNormals->CrossProd (&pS, &pE);
      paNormals->Normalize();
   }

   // calculate the cutouts
   CalcOGGrid ();
}

/**********************************************************************************
CObjectGround::Clone - Clones this

inputs
   none
returns
   PCObjectSocket - Clone
*/
PCObjectSocket CObjectGround::Clone (void)
{
   PCObjectGround pNew;

   pNew = new CObjectGround(NULL, &m_OSINFO);

   // free up all the surface info
   DWORD i;
   PCObjectSurface *ppos;
   ppos = (PCObjectSurface*) pNew->m_lTextPCObjectSurface.Get(0);
   for (i = 0; i < pNew->m_lTextPCObjectSurface.Num(); i++)
      delete ppos[i];
   pNew->m_lTextPCObjectSurface.Clear();
   pNew->m_lTextCOLORREF.Clear();

   PCForest *ppf;
   ppf = (PCForest*) pNew->m_lPCForest.Get(0);
   for (i = 0; i < pNew->m_lPCForest.Num(); i++)
      delete ppf[i];
   pNew->m_lPCForest.Clear();

   for (i = 0; i < GWATERNUM * GWATEROVERLAP; i++)
      if (pNew->m_apWaterSurf[i])
         delete pNew->m_apWaterSurf[i];

   // clone template info
   CloneTemplate(pNew);

   pNew->m_fWater = m_fWater;
   pNew->m_fWaterSize =m_fWaterSize;
   pNew->m_fWaterElevation = m_fWaterElevation;

   pNew->m_dwWidth = m_dwWidth;
   pNew->m_dwHeight = m_dwHeight;
   pNew->m_tpElev = m_tpElev;
   pNew->m_fLessDetail = m_fLessDetail;
   pNew->m_fForestBoxes = m_fForestBoxes;
   pNew->m_fSaveCompressed = m_fSaveCompressed;
   pNew->m_fDontDrawNearTrees = m_fDontDrawNearTrees;
   pNew->m_memElev.Required (m_dwWidth * m_dwHeight * sizeof(WORD));
   pNew->m_pawElev = (WORD*) pNew->m_memElev.p;
   memcpy (pNew->m_pawElev, m_pawElev, m_dwWidth * m_dwHeight * sizeof(WORD));

   // tranfer textures over
   pNew->m_memTextSet.Required (m_dwWidth * m_dwHeight * sizeof(BYTE) * m_lTextPCObjectSurface.Num() );
   pNew->m_pabTextSet = (BYTE*) pNew->m_memTextSet.p;
   memcpy (pNew->m_pabTextSet, m_pabTextSet, m_dwWidth * m_dwHeight * sizeof(BYTE) * m_lTextPCObjectSurface.Num());
   ppos = (PCObjectSurface*) m_lTextPCObjectSurface.Get(0);
   PCObjectSurface pos;
   pNew->m_lTextPCObjectSurface.Required (m_lTextPCObjectSurface.Num());
   for (i = 0; i < m_lTextPCObjectSurface.Num(); i++) {
      pos = ppos[i]->Clone();
      pNew->m_lTextPCObjectSurface.Add(&pos);
   }
   pNew->m_lTextCOLORREF.Init (sizeof(COLORREF), m_lTextCOLORREF.Get(0), m_lTextCOLORREF.Num());

   // transfer new forest over
   if (m_lPCForest.Num()) {
      pNew->m_memForestSet.Required (m_dwWidth * m_dwHeight * sizeof(BYTE) * m_lPCForest.Num());
      pNew->m_pabForestSet = (BYTE*) pNew->m_memForestSet.p;
      memcpy (pNew->m_pabForestSet, m_pabForestSet, m_dwWidth * m_dwHeight * sizeof(BYTE) * m_lPCForest.Num());
   }
   else
      pNew->m_pabForestSet = NULL;
   PCForest pf;
   ppf = (PCForest*) m_lPCForest.Get(0);
   pNew->m_lPCForest.Required (m_lPCForest.Num());
   for (i = 0; i < m_lPCForest.Num(); i++) {
      pf = ppf[i]->Clone();
      pNew->m_lPCForest.Add(&pf);
   }

   pNew->m_fScale = m_fScale;
   pNew->m_fNormalsDirty = TRUE;   // dont bother copying over notmals since can recalc
   pNew->m_fBBDirty = TRUE;   // will need to recalc bounding boxes
   // pNew->m_memNormals - not copied over
   // pNew->m_memBBMin and pNew->m_memBBMax - not copied over
   // not worrying about m_lOGGRID since normals dirty takes care of it

   // normally would clear pNew->m_lOGCUTOUT but know it wont have that
   pNew->m_lOGCUTOUT.Init (sizeof(OGCUTOUT), m_lOGCUTOUT.Get(0), m_lOGCUTOUT.Num());
   // clone memory for cutouts
   POGCUTOUT pc = (POGCUTOUT) pNew->m_lOGCUTOUT.Get(0);
   for (i = 0; i < pNew->m_lOGCUTOUT.Num(); i++, pc++) {
      if (!pc->plTEXTUREPOINT)
         continue;

      PCListFixed plOld;
      plOld = pc->plTEXTUREPOINT;
      pc->plTEXTUREPOINT = new CListFixed;
      if (!pc->plTEXTUREPOINT)
         continue;
      pc->plTEXTUREPOINT->Init (sizeof(TEXTUREPOINT), plOld->Get(0), plOld->Num());
   }

   // clone the water
   for (i = 0; i < GWATERNUM * GWATEROVERLAP; i++)
      pNew->m_apWaterSurf[i] = m_apWaterSurf[i]->Clone();
   for (i = 0; i < GWATERNUM; i++)
      pNew->m_afWaterElev[i] = m_afWaterElev[i];


   pNew->FillInRENDERSURFACE();

   return pNew;
}



static PWSTR gpszWater = L"Water";
static PWSTR gpszWaterElevation = L"WaterElevation";
static PWSTR gpszWidth = L"Width";
static PWSTR gpszHeight = L"Height";
static PWSTR gpszElevScale = L"ElevScale";
static PWSTR gpszScale = L"Scale";
static PWSTR gpszElev = L"Elev";
static PWSTR gpszCutout = L"Cutout";
static PWSTR gpszWaterSize = L"WaterSize";
static PWSTR gpszCutoutPoint = L"CutoutPoint%d";
static PWSTR gpszName = L"Name";
static PWSTR gpszEmbedded = L"Embedded";
static PWSTR gpszNumber = L"Number";
static PWSTR gpszText = L"Text";
static PWSTR gpszTextSurface = L"TextureSurface";
static PWSTR gpszShowColor = L"ShowColor";
static PWSTR gpszWaterSurf = L"WaterSurf";
static PWSTR gpszWaterElev = L"WaterElev";
static PWSTR gpszForest = L"Forest";
static PWSTR gpszForestMem = L"ForestMem";
static PWSTR gpszLessDetail = L"LessDetail";
static PWSTR gpszForestBoxes = L"ForestBoxes";
static PWSTR gpszSaveCompressed = L"SaveCompressed";
static PWSTR gpszDontDrawNearTrees = L"DontDrawNearTrees";

PCMMLNode2 CObjectGround::MMLTo (void)
{
   // call into the template first
   PCMMLNode2 pNode = MMLToTemplate();
   if (!pNode)
      return NULL;


   MMLValueSet (pNode, gpszWater, (int) m_fWater);
   MMLValueSet (pNode, gpszWaterElevation, m_fWaterElevation);
   MMLValueSet (pNode, gpszWaterSize, m_fWaterSize);

   MMLValueSet (pNode, gpszWidth, (int)m_dwWidth);
   MMLValueSet (pNode, gpszHeight, (int)m_dwHeight);
   MMLValueSet (pNode, gpszElevScale, &m_tpElev);
   MMLValueSet (pNode, gpszScale, m_fScale);
   MMLValueSet (pNode, gpszLessDetail, (int) m_fLessDetail);
   MMLValueSet (pNode, gpszForestBoxes, (int) m_fForestBoxes);
   MMLValueSet (pNode, gpszSaveCompressed, (int) m_fSaveCompressed);
   MMLValueSet (pNode, gpszDontDrawNearTrees, (int) m_fDontDrawNearTrees);

   CMem memRLE;

   // write elevation
   memRLE.m_dwCurPosn = 0;
   if (m_fSaveCompressed)
      CompressElevation (&memRLE);
   else
      RLEEncode ((PBYTE) m_pawElev, m_dwWidth * m_dwHeight, sizeof(WORD), &memRLE);
   MMLValueSet (pNode, gpszElev, (PBYTE) memRLE.p, memRLE.m_dwCurPosn);

   memRLE.m_dwCurPosn = 0;
   if (m_fSaveCompressed)
      CompressBytes (m_pabTextSet, m_dwWidth * m_dwHeight * m_lTextPCObjectSurface.Num(), &memRLE);
   else
      RLEEncode (m_pabTextSet, m_dwWidth * m_dwHeight * m_lTextPCObjectSurface.Num(), sizeof(BYTE), &memRLE);
   MMLValueSet (pNode, gpszText, (PBYTE) memRLE.p, memRLE.m_dwCurPosn);

   memRLE.m_dwCurPosn = 0;
   if (m_lPCForest.Num()) {
      if (m_fSaveCompressed)
         CompressBytes (m_pabForestSet, m_dwWidth * m_dwHeight * m_lPCForest.Num(), &memRLE);
      else
         RLEEncode (m_pabForestSet, m_dwWidth * m_dwHeight * m_lPCForest.Num(), sizeof(BYTE), &memRLE);
      MMLValueSet (pNode, gpszForestMem, (PBYTE) memRLE.p, memRLE.m_dwCurPosn);
   }


   // write out the texture info
   DWORD i;
   PCObjectSurface *ppos;
   COLORREF *pcr;
   ppos = (PCObjectSurface*) m_lTextPCObjectSurface.Get(0);
   pcr = (COLORREF*) m_lTextCOLORREF.Get(0);
   for (i = 0; i < m_lTextPCObjectSurface.Num(); i++) {
      PCMMLNode2 pSub = ppos[i]->MMLTo();
      if (!pSub)
         continue;
      pSub->NameSet (gpszTextSurface);
      MMLValueSet (pSub, gpszShowColor, (int)pcr[i]);
      pNode->ContentAdd (pSub);
   }

   // write out all the forests
   PCForest *ppf;
   ppf = (PCForest*) m_lPCForest.Get(0);
   for (i = 0; i < m_lPCForest.Num(); i++) {
      PCMMLNode2 pSub = ppf[i]->MMLTo();
      if (!pSub)
         continue;
      pSub->NameSet (gpszForest);
      pNode->ContentAdd (pSub);
   }

   for (i = 0; i < GWATERNUM * GWATEROVERLAP; i++) {
      PCMMLNode2 pSub = m_apWaterSurf[i]->MMLTo();
      if (!pSub)
         continue;
      pSub->NameSet (gpszWaterSurf);
      MMLValueSet (pSub, gpszWaterElev, m_afWaterElev[i % GWATERNUM]);
      pNode->ContentAdd (pSub);
   }

   POGCUTOUT pc = (POGCUTOUT) m_lOGCUTOUT.Get(0);
   for (i = 0; i < m_lOGCUTOUT.Num(); i++, pc++) {
      PCMMLNode2 pCut = new CMMLNode2;
      if (!pCut)
         continue;
      pCut->NameSet (gpszCutout);

      MMLValueSet (pCut, gpszName, (PBYTE) &pc->gID, sizeof(pc->gID));
      MMLValueSet (pCut, gpszEmbedded, (int) pc->fEmbedded);

      // number of points
      DWORD dwNum;
      DWORD x;
      PTEXTUREPOINT ptp;
      WCHAR szTemp[64];
      ptp = (PTEXTUREPOINT) pc->plTEXTUREPOINT->Get(0);
      MMLValueSet (pCut, gpszNumber, (int) (dwNum = pc->plTEXTUREPOINT->Num()));

      for (x = 0; x < dwNum; x++) {
         swprintf (szTemp, gpszCutoutPoint, (int) x);
         MMLValueSet (pCut, szTemp, ptp + x);
      }

      pNode->ContentAdd (pCut);
   }
   
  return pNode;
}

BOOL CObjectGround::MMLFrom (PCMMLNode2 pNode)
{
   // call into the template first
   if (!MMLFromTemplate (pNode))
      return FALSE;

   // free up mmeory for cutouts
   DWORD i;
   POGCUTOUT pc = (POGCUTOUT) m_lOGCUTOUT.Get(0);
   for (i = 0; i < m_lOGCUTOUT.Num(); i++, pc++) {
      if (pc->plTEXTUREPOINT)
         delete pc->plTEXTUREPOINT;
   }
   m_lOGCUTOUT.Clear();

   // free up all the surface info
   PCObjectSurface *ppos;
   ppos = (PCObjectSurface*) m_lTextPCObjectSurface.Get(0);
   for (i = 0; i < m_lTextPCObjectSurface.Num(); i++)
      delete ppos[i];
   m_lTextPCObjectSurface.Clear();
   m_lTextCOLORREF.Clear();

   PCForest *ppf;
   ppf = (PCForest*) m_lPCForest.Get(0);
   for (i = 0; i < m_lPCForest.Num(); i++)
      delete ppf[i];
   m_lPCForest.Clear();

   m_fWater = (BOOL) MMLValueGetInt (pNode, gpszWater, 0);
   m_fWaterElevation = MMLValueGetDouble (pNode, gpszWaterElevation, 0);
   m_fWaterSize =MMLValueGetDouble (pNode, gpszWaterSize, 0);

   m_dwWidth = (DWORD) MMLValueGetInt (pNode, gpszWidth, 16);
   m_dwHeight = (DWORD) MMLValueGetInt (pNode, gpszHeight, 16);
   m_dwWidth = max(m_dwWidth,1);
   m_dwHeight = max(m_dwHeight, 1);
   MMLValueGetTEXTUREPOINT (pNode, gpszElevScale, &m_tpElev);
   m_fScale = MMLValueGetDouble (pNode, gpszScale, 1);
   m_fLessDetail = (BOOL) MMLValueGetInt (pNode, gpszLessDetail, TRUE);
   m_fForestBoxes = (BOOL) MMLValueGetInt (pNode, gpszForestBoxes, TRUE);
   m_fSaveCompressed = (BOOL) MMLValueGetInt (pNode, gpszSaveCompressed, FALSE);
   m_fDontDrawNearTrees = (BOOL) MMLValueGetInt (pNode, gpszDontDrawNearTrees, TRUE);

   // read in the cutouts
   PCMMLNode2 pSub;
   PWSTR psz;
   CMem memCutout;
   DWORD dwCurSurf;
   dwCurSurf = 0;
   for (i = 0; i < pNode->ContentNum(); i++) {
      pSub = NULL;
      pNode->ContentEnum (i, &psz, &pSub);
      if (!pSub)
         continue;
      psz = pSub->NameGet();
      if (!psz)
         continue;
      if (!_wcsicmp(psz, gpszTextSurface)) {
         PCObjectSurface pos = new CObjectSurface;
         COLORREF c;
         if (!pos)
            continue;
         if (!pos->MMLFrom (pSub)) {
            delete pos;
            continue;
         }
         c = (COLORREF) MMLValueGetInt (pSub, gpszShowColor, 0);

         m_lTextPCObjectSurface.Add (&pos);
         m_lTextCOLORREF.Add (&c);
      }
      else if (!_wcsicmp(psz, gpszForest)) {
         PCForest pForest = new CForest;
         if (!pForest)
            continue;
         if (!pForest->MMLFrom (pSub)) {
            delete pForest;
            continue;
         }
         m_lPCForest.Add (&pForest);
      }
      else if (!_wcsicmp(psz, gpszWaterSurf)) {
         if (dwCurSurf >= GWATERNUM * GWATEROVERLAP)
            continue;   // too high
         PCObjectSurface pos = m_apWaterSurf[dwCurSurf];
         if (!pos->MMLFrom (pSub)) {
            continue;
         }
         if (dwCurSurf < GWATERNUM)
            m_afWaterElev[dwCurSurf] = MMLValueGetDouble (pSub, gpszWaterElev, m_afWaterElev[dwCurSurf]);
         dwCurSurf++;
      }
      else if (!_wcsicmp(psz, gpszCutout)) {
         // found a cutout

         OGCUTOUT cut;
         memset (&cut, 0, sizeof(cut));
         

         // get the name
         if (sizeof(cut.gID) != MMLValueGetBinary (pSub, gpszName, (PBYTE)&cut.gID, sizeof(cut.gID)))
            continue;

         cut.fEmbedded = (BOOL) MMLValueGetInt (pSub, gpszEmbedded, 0);

         // number of points
         DWORD dwNum;
         dwNum = (DWORD) MMLValueGetInt (pSub, gpszNumber, 0);
         if (!dwNum)
            continue;

         // allocate enough memory
         cut.plTEXTUREPOINT = new CListFixed;
         if (!cut.plTEXTUREPOINT)
            continue;
         cut.plTEXTUREPOINT->Init (sizeof(TEXTUREPOINT));

         TEXTUREPOINT tp;
         DWORD x;
         WCHAR szTemp[64];
         cut.plTEXTUREPOINT->Required (cut.plTEXTUREPOINT->Num() + dwNum);
         for (x = 0; x < dwNum; x++) {
            swprintf (szTemp, gpszCutoutPoint, (int) x);
            tp.h = tp.v = 0;
            MMLValueGetTEXTUREPOINT (pSub, szTemp, &tp);
            cut.plTEXTUREPOINT->Add (&tp);
         }

         // add this
         m_lOGCUTOUT.Add (&cut);
      }  // cutout
   }


   // not worrying about m_lOGGRID since normalsdirty takes care of it

   // If dont have at least one texture then make one
   if (!m_lTextPCObjectSurface.Num()) {
      PCObjectSurface pNew = new CObjectSurface;
      COLORREF c = RGB(0x80,0x80,0x80);
      m_lTextPCObjectSurface.Add (&pNew);
      m_lTextCOLORREF.Add (&c);
   }

   // make sure have plenty of scratch space to store RLE
   CMem memRLE;
   size_t dwUsed;
   //dwUsed =  0;
   //psz = MMLValueGet (pNode, gpszElev);
   //if (psz)
   //   dwUsed = max(dwUsed, wcslen(psz)/2);
   //psz = MMLValueGet (pNode, gpszText);
   //if (psz)
   //   dwUsed = max(dwUsed, wcslen(psz)/2);
   //psz = MMLValueGet (pNode, gpszForestMem);
   //if (psz)
   //   dwUsed = max(dwUsed, wcslen(psz)/2);
   //if (!memRLE.Required (dwUsed + 128))
   //   return FALSE;

   // get the elevation
   if (!m_memElev.Required (m_dwWidth * m_dwHeight * sizeof(WORD)))
      return FALSE;
   memset (m_memElev.p, 0, m_dwWidth * m_dwHeight * sizeof(WORD));
   MMLValueGetBinary (pNode, gpszElev, &memRLE);
   if (m_fSaveCompressed)
      DeCompressElevation ((PBYTE)memRLE.p, (DWORD)memRLE.m_dwCurPosn);
   else
      RLEDecode ((PBYTE) memRLE.p, memRLE.m_dwCurPosn, sizeof(WORD), &m_memElev, &dwUsed);
   m_pawElev = (WORD*) m_memElev.p;


   if (!m_memTextSet.Required (m_dwWidth * m_dwHeight * sizeof(BYTE) * m_lTextPCObjectSurface.Num()))
      return FALSE;
   memset (m_memTextSet.p, 0, m_dwWidth * m_dwHeight * sizeof(BYTE) * m_lTextPCObjectSurface.Num());
   MMLValueGetBinary (pNode, gpszText, &memRLE);
   if (m_fSaveCompressed)
      DeCompressBytes ((PBYTE) memRLE.p, memRLE.m_dwCurPosn, &m_memTextSet);
   else
      RLEDecode ((PBYTE) memRLE.p, memRLE.m_dwCurPosn, sizeof(BYTE), &m_memTextSet, &dwUsed);
   m_pabTextSet = (BYTE*) m_memTextSet.p;
   //MMLValueGetBinary (pNode, gpszText, (PBYTE) m_pabText, m_dwWidth * m_dwHeight * sizeof(BYTE));

   // read the forest
   if (m_lPCForest.Num()) {
      if (!m_memForestSet.Required (m_dwWidth * m_dwHeight * sizeof(BYTE) * m_lPCForest.Num()))
         return FALSE;
      memset (m_memForestSet.p, 0, m_dwWidth * m_dwHeight * sizeof(BYTE) * m_lPCForest.Num());
      MMLValueGetBinary (pNode, gpszForestMem, &memRLE);
      if (m_fSaveCompressed)
         DeCompressBytes ((PBYTE) memRLE.p, memRLE.m_dwCurPosn, &m_memForestSet);
      else
         RLEDecode ((PBYTE) memRLE.p, memRLE.m_dwCurPosn, sizeof(BYTE), &m_memForestSet, &dwUsed);
      m_pabForestSet = (BYTE*) m_memForestSet.p;
   }
   else
      m_pabForestSet = NULL;

   // set flag to indicate normals need recalc
   m_fNormalsDirty = TRUE;
   m_fBBDirty = TRUE;

   FillInRENDERSURFACE();

   return TRUE;
}


/**************************************************************************************
CObjectGround::CompressGrid - Compresses a grid of elevation

inputs
   rect        *pr - Rectangle of corners. UL is included, LR is not. Must be within m_dwWIdth, m_dwHeight
   PCMem       pMem - The memory is appended to at m_dwCurPosn. m_dwCurPosn
               is update with the new memory location after writing.
returns
   BOOL - TRUE if success
*/
BOOL CObjectGround::CompressGrid (RECT *pr, PCMem pMem)
{
   int iWidth = pr->right - pr->left;
   int iHeight = pr->bottom - pr->top;
   DWORD dwNeed = 2 * sizeof(WORD) + (DWORD)(iWidth*iHeight)*sizeof(BYTE);
   if (!pMem->Required (pMem->m_dwCurPosn + dwNeed))
      return FALSE; // error

   // find the min and max
   WORD wMin = 0xffff;
   WORD wMax = 0;
   WORD *paw;
   DWORD x, y;
   for (y = (DWORD)pr->top; y < (DWORD)pr->bottom; y++) {
      paw = m_pawElev + (y * m_dwWidth + (DWORD)pr->left);
      for (x = (DWORD)pr->left; x < (DWORD)pr->right; x++, paw++) {
         wMin = min(wMin, *paw);
         wMax = max(wMax, *paw);
      } // x
   } // y

   // determine a scale
   WORD wDelta = wMax - wMin;
   fp fScale = wDelta ? (255.5 / (fp)wDelta) : 0;

   // write the info
   paw = (WORD*)((PBYTE)pMem->p + pMem->m_dwCurPosn);
   paw[0] = wMin;
   paw[1] = wMax;
   PBYTE pb = (PBYTE) (paw + 2);
   for (y = (DWORD)pr->top; y < (DWORD)pr->bottom; y++) {
      paw = m_pawElev + (y * m_dwWidth + (DWORD)pr->left);
      for (x = (DWORD)pr->left; x < (DWORD)pr->right; x++, paw++, pb++)
         *pb = (BYTE)((fp)(*paw - wMin) * fScale);
   } // y

   pMem->m_dwCurPosn += dwNeed;

   return TRUE;
}

/**************************************************************************************
CObjectGround::DeCompressGrid - DeCompresses a grid of elevation

This automatically fills into m_memElev

inputs
   rect        *pr - Rectangle of corners. UL is included, LR is not. Must be within m_dwWIdth, m_dwHeight
   PVOID       pComp - Compressed data
   DWORD       dwSize - Number of bytes available in pComp to use
returns
   DWORD - Number of bytes used, or 0 if error
*/
size_t CObjectGround::DeCompressGrid (RECT *pr, PVOID pComp, size_t dwSize)
{
   int iWidth = pr->right - pr->left;
   int iHeight = pr->bottom - pr->top;
   size_t dwNeed = 2 * sizeof(WORD) + (DWORD)(iWidth*iHeight)*sizeof(BYTE);
   if (dwSize < dwNeed)
      return 0;   // error

   WORD *paw = (WORD*) pComp;
   WORD wMin = paw[0];
   WORD wMax = paw[1];
   PBYTE pb = (PBYTE)(paw + 2);
   fp fScale = (fp)((wMax - wMin) + 0.5) / 255.0;

   // decompress
   // write the info
   DWORD x,y;
   for (y = (DWORD)pr->top; y < (DWORD)pr->bottom; y++) {
      paw = (WORD*)m_memElev.p + (y * m_dwWidth + (DWORD)pr->left);
      for (x = (DWORD)pr->left; x < (DWORD)pr->right; x++, paw++, pb++)
         *paw = (WORD)((fp)*pb * fScale) + wMin;
   } // y


   return dwNeed;
}


/**************************************************************************************
CObjectGround::CompressElevation - Compresses the elevation (with loss)

inputs
   PCMem       pComp - Filled with the final compressed result, appended at m_dwCurPosn.
                  m_dwCurPosn is updated to the location
returns
   BOOL - TRUE if success
*/
#define ELEVBLOCKSIZE      16       // block into 16x16 pixels
BOOL CObjectGround::CompressElevation (PCMem pComp)
{
   // compress into blocks
   CMem memTemp;
   DWORD x, y;
   RECT r;
   for (y = 0; y < m_dwHeight; y += ELEVBLOCKSIZE) {
      r.top = (int)y;
      r.bottom = (int)min(y+ELEVBLOCKSIZE, m_dwHeight);
      for (x = 0; x < m_dwWidth; x += ELEVBLOCKSIZE) {
         r.left = (int)x;
         r.right = (int)min(x+ELEVBLOCKSIZE, m_dwWidth);
         if (!CompressGrid (&r, &memTemp))
            return FALSE;
      } // x
   } // y

   // finally RLE comprss
   RLEEncode ((PBYTE) memTemp.p, memTemp.m_dwCurPosn, sizeof(BYTE), pComp);
   return TRUE;
}



/**************************************************************************************
CObjectGround::DeCompressElevation - DeCompresses the elevation (with loss)

inputs
   PBYTE       pComp - Compressed memory
   DWORD       dwSize - Number of bytes in pComp
returns
   BOOL - TRUE if success
*/
BOOL CObjectGround::DeCompressElevation (PBYTE pComp, size_t dwSize)
{
   // decode from RLE
   CMem memTemp;
   size_t dwUsed;
   RLEDecode (pComp, dwSize, sizeof(BYTE), &memTemp, &dwUsed);
   if (dwUsed != dwSize)
      return FALSE;

   // new settings
   pComp = (PBYTE)memTemp.p;
   dwSize = memTemp.m_dwCurPosn;
   m_memElev.m_dwCurPosn = m_dwWidth * m_dwHeight * sizeof(WORD);
   if (!m_memElev.Required (m_memElev.m_dwCurPosn))
      return FALSE;

   // decompress into blocks
   DWORD x, y;
   RECT r;
   for (y = 0; y < m_dwHeight; y += ELEVBLOCKSIZE) {
      r.top = (int)y;
      r.bottom = (int)min(y+ELEVBLOCKSIZE, m_dwHeight);
      for (x = 0; x < m_dwWidth; x += ELEVBLOCKSIZE) {
         r.left = (int)x;
         r.right = (int)min(x+ELEVBLOCKSIZE, m_dwWidth);

         dwUsed = DeCompressGrid (&r, pComp, dwSize);
         if (!dwUsed)
            return FALSE;
         pComp += dwUsed;
         dwSize -= dwUsed;
      } // x
   } // y

   return !dwSize;   // since if anything left, error
}



/**************************************************************************************
CObjectGround::CompressBytes - This compresses the byte fileds for forest or
texture intensity into nibbles, and then RLE's them.

inputs
   PBYTE          pComp - Data to comrpress
   DWORD          dwNum - Number of points
   PCMem          pMem - Filled with compressed data, starting at m_dwCurPosn. m_dwCurPosn is udpated
returns
   BOOL - TRUE if success
*/
BOOL CObjectGround::CompressBytes (PBYTE pComp, size_t dwNum, PCMem pMem)
{
   // fill temporary memory with nibbles
   CMem memTemp;
   size_t dwNeed = (dwNum + 1) / 2;
   if (!memTemp.Required (dwNeed))
      return FALSE;
   DWORD i;
   PBYTE pbFill = (PBYTE)memTemp.p;
   for (i = 0; i < dwNeed; i++) {
      BYTE b1 = pComp[i*2];
      BYTE b2 = (i*2+1 < dwNum) ? pComp[i*2+1] : 0;
      pbFill[i] = (b1 & 0xf0) | (b2 >> 4);
   } // i

   // rle this
   RLEEncode (pbFill, dwNeed, sizeof(BYTE), pMem);
   return TRUE;
}


/**************************************************************************************
CObjectGround::DeCompressBytes - DeCompresses the bytes used for forest/texure density.

inputs
   PBYTE       pComp - Compressed memory
   DWORD       dwSize - Number of bytes in pComp
   PCMem       pMem - Fill this memory, starting at 0, and update m_dwCurPosn
returns
   BOOL - TRUE if success
*/
BOOL CObjectGround::DeCompressBytes (PBYTE pComp, size_t dwSize, PCMem pMem)
{
   // decode from RLE
   CMem memTemp;
   size_t dwUsed;
   RLEDecode (pComp, dwSize, sizeof(BYTE), &memTemp, &dwUsed);
   if (dwUsed != dwSize)
      return FALSE;

   // new settings
   pComp = (PBYTE)memTemp.p;
   dwSize = memTemp.m_dwCurPosn;

   // make sure enough memory
   pMem->m_dwCurPosn = 0;
   if (!pMem->Required(pMem->m_dwCurPosn + dwSize*2))
      return FALSE;
   PBYTE pDecomp = (PBYTE)pMem->p + pMem->m_dwCurPosn;
   pMem->m_dwCurPosn += dwSize*2;

   // decompress
   DWORD i;
   for (i = 0; i < dwSize; i++) {
      // duplicate nibbles so get range from 0 to 0xff, not 0 to 0xf0
      pDecomp[i*2+0] = (pComp[i] & 0xf0) | (pComp[i] >> 4);
      pDecomp[i*2+1] = (pComp[i] << 4) | (pComp[i] & 0x0f);
   } // i

   return TRUE;
}


   
/**************************************************************************************
CObjectGround::MoveReferencePointQuery - 
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
BOOL CObjectGround::MoveReferencePointQuery (DWORD dwIndex, PCPoint pp)
{
   PSPLINEMOVEP ps;
   DWORD dwDataSize;
   ps = gaGroundMove;
   dwDataSize = sizeof(gaGroundMove);

   if (dwIndex >= dwDataSize / sizeof(SPLINEMOVEP))
      return FALSE;

   pp->Zero();
   pp->p[0] = (fp)ps[dwIndex].iX * (fp) (m_dwWidth-1) * m_fScale / 2;
   pp->p[1] = (fp)ps[dwIndex].iY * (fp) (m_dwHeight-1) * m_fScale / 2;

   return TRUE;
}

/**************************************************************************************
CObjectGround::MoveReferenceStringQuery -
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
BOOL CObjectGround::MoveReferenceStringQuery (DWORD dwIndex, PWSTR psz, DWORD dwSize, DWORD *pdwNeeded)
{
   PSPLINEMOVEP ps;
   DWORD dwDataSize;
   ps = gaGroundMove;
   dwDataSize = sizeof(gaGroundMove);
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
CObjectGround::ContHVQuery
Called to see the HV location of what clicked on, or in other words, given a line
from pEye to pClick (object sapce), where does it intersect surface dwSurface (LOWORD(PIMAGEPIXEL.dwID))?
Fills pHV with the point. Returns FALSE if doesn't intersect, or invalid surface, or
can't tell with that surface. NOTE: if pOld is not NULL, then assume a drag from
pOld (converted to world object space) to pClick. Finds best intersection of the
surface then. Useful for dragging embedded objects around surface
*/
BOOL CObjectGround::ContHVQuery (PCPoint pEye, PCPoint pClick, DWORD dwSurface, PTEXTUREPOINT pOld, PTEXTUREPOINT pHV)
{
   if (dwSurface != SIDEA)
      return FALSE;

   // NOTE: Not supporting the drag feature because don't look at pOld. Hopefully
   // this wont cause and problems. Only put it in because had problems with cutout
   // right at the edge of a double surface

   CPoint pDir;
   pDir.Subtract (pClick, pEye);
   return IntersectLine (pEye, &pDir, FALSE, NULL, NULL, pHV);
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
BOOL CObjectGround::ContCutout (const GUID *pgEmbed, DWORD dwNum, PCPoint paFront, PCPoint paBack, BOOL fBothSides)
{
   // get the surface it's on
   TEXTUREPOINT tp;
   fp fRotation;
   DWORD dwSurface;
   dwSurface = 0;
   ContEmbeddedLocationGet (pgEmbed, &tp, &fRotation, &dwSurface);
   if (dwSurface != SIDEA)
      return FALSE;

   // figure out a list of all the points it goes through
   CListFixed lInter;
   lInter.Init (sizeof(TEXTUREPOINT));
   DWORD i;
   CPoint pDir;
   lInter.Required (dwNum);
   for (i = 0; i < dwNum; i++) {
      pDir.Subtract (&paBack[i], &paFront[i]);
      if (!IntersectLine (&paFront[i], &pDir, TRUE, NULL, NULL, &tp))
         continue;
      lInter.Add (&tp);
   }


   if (m_pWorld)
      m_pWorld->ObjectAboutToChange(this);

   m_fNormalsDirty = TRUE; // so recalc normals, and in the process cutout bits

   // see if can find existing element
   POGCUTOUT pc;
   pc = (POGCUTOUT) m_lOGCUTOUT.Get(0);
   for (i = 0; i < m_lOGCUTOUT.Num(); i++, pc++) {
      if (pc->fEmbedded && IsEqualGUID (pc->gID, *pgEmbed))
         break;
   }
   if (i >= m_lOGCUTOUT.Num())
      pc = NULL;

   if (lInter.Num() < 3) {
      if (pc) {
         delete pc->plTEXTUREPOINT;
         m_lOGCUTOUT.Remove (i);
      }
   }
   else {
      // see if need to add new cutout
      if (!pc) {
         OGCUTOUT cut;
         memset (&cut, 0, sizeof(cut));
         cut.gID = *pgEmbed;
         cut.fEmbedded = TRUE;
         cut.plTEXTUREPOINT = new CListFixed;
         if (!cut.plTEXTUREPOINT) {
            if (m_pWorld)
               m_pWorld->ObjectChanged(this);
            return FALSE;
         }
         m_lOGCUTOUT.Add (&cut);
         pc = (POGCUTOUT) m_lOGCUTOUT.Get(m_lOGCUTOUT.Num()-1);
      }

      // need to add
      pc->plTEXTUREPOINT->Init (sizeof(TEXTUREPOINT), lInter.Get(0), lInter.Num());
   }

   if (m_pWorld)
      m_pWorld->ObjectChanged(this);

   return TRUE;
}

/**********************************************************************************
CObjectGround::ContCutoutToZipper -
Assumeing that ContCutout() has already been called for the pgEmbed, this askes the surfaces
for the zippering information (CRenderSufrace::ShapeZipper()) that would perfectly seal up
the cutout. plistXXXHVXYZ must be initialized to sizeof(HVXYZ). plisstFrontHVXYZ is filled with
the points on the side where the object was embedded, and plistBackHVXYZ are the opposite side.
NOTE: These are in the container's object space, NOT the embedded object's object space.
*/
BOOL CObjectGround::ContCutoutToZipper (const GUID *pgEmbed, PCListFixed plistFrontHVXYZ, PCListFixed plistBackHVXYZ)
{
   // find out if this is valid and which side it's located on
   TEXTUREPOINT tp;
   fp fRotation;
   DWORD dwSurface;
   if (!ContEmbeddedLocationGet(pgEmbed, &tp, &fRotation, &dwSurface))
      return FALSE;

   if (dwSurface != SIDEA)
      return FALSE;

   // find it
   DWORD i;
   POGCUTOUT pc;
   pc = (POGCUTOUT) m_lOGCUTOUT.Get(0);
   for (i = 0; i < m_lOGCUTOUT.Num(); i++, pc++) {
      if (!pc->fEmbedded || !IsEqualGUID (pc->gID, *pgEmbed))
         continue;

      // else found match
      break;
   }
   if (i >= m_lOGCUTOUT.Num())
      return FALSE;  // nothing

   // convert these into two lists of texture points
   plistFrontHVXYZ->Init (sizeof(HVXYZ));
   plistBackHVXYZ->Init (sizeof(HVXYZ));
   HVXYZ hv;
   PTEXTUREPOINT ptp;
   ptp = (PTEXTUREPOINT) pc->plTEXTUREPOINT->Get(0);
   for (i = 0; i < pc->plTEXTUREPOINT->Num(); i++, ptp++) {
      hv.h = ptp->h;
      hv.v = ptp->v;
      HVToPoint (hv.h, hv.v, &hv.p);
      plistFrontHVXYZ->Add (&hv);
   }

   return TRUE;
}


/**********************************************************************************
CObjectGround::ContThickness - 
returns the thickness of the surface (dwSurface) at pHV. Used by embedded
objects like windows to know how deep they should be.

NOTE: usually overridden
*/
fp CObjectGround::ContThickness (DWORD dwSurface, PTEXTUREPOINT pHV)
{
   if (dwSurface != SIDEA)
      return FALSE;

   return 0;
}


/**********************************************************************************
CObjectGround::ContSideInfo -
returns a DWORD indicating what class of surface it is... right now
0 = internal, 1 = external. dwSurface is the surface. If fOtherSide
then want to know what's on the opposite side. Returns 0 if bad dwSurface.

NOTE: usually overridden
*/
DWORD CObjectGround::ContSideInfo (DWORD dwSurface, BOOL fOtherSide)
{
   if (dwSurface == SIDEA)
      return 1;
   else
      return 0;
}


/**********************************************************************************
CObjectGround::ContMatrixFromHV - THE SUPERCLASS SHOULD OVERRIDE THIS. Given
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
BOOL CObjectGround::ContMatrixFromHV (DWORD dwSurface, PTEXTUREPOINT pHV, fp fRotation, PCMatrix pm)
{
   if (dwSurface != SIDEA)
      return FALSE;

   // given HV, get normals and stuff
   CPoint pC, pH, pV, pN;
   HVToPoint (pHV->h, pHV->v, &pC);
   if (pHV->h + .01 < 1.0) {
      HVToPoint (pHV->h + .01, pHV->v, &pH);
      pH.Subtract (&pC);
   }
   else {
      HVToPoint (pHV->h - .01, pHV->v, &pH);
      pH.Subtract (&pC);
      pH.Scale (-1);
   }
   pH.Normalize();
   if (pHV->v > 0.01) {
      HVToPoint (pHV->h, pHV->v - .01, &pV);
      pV.Subtract (&pC);
   }
   else {
      HVToPoint (pHV->h, pHV->v + .01, &pV);
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
   pm->MultiplyRight (&m_MatrixObject);

   return TRUE;
}

/**********************************************************************************
CObjectGround::
Called to tell the container that one of its contained objects has been renamed.
*/
BOOL CObjectGround::ContEmbeddedRenamed (const GUID *pgOld, const GUID *pgNew)
{
   POGCUTOUT pc = (POGCUTOUT) m_lOGCUTOUT.Get(0);
   DWORD i;
   for (i = 0; i < m_lOGCUTOUT.Num(); i++, pc++) {
      if (IsEqualGUID (pc->gID, *pgOld)) {
         if (m_pWorld)
            m_pWorld->ObjectAboutToChange (this);
         pc->gID = *pgNew;
         if (m_pWorld)
            m_pWorld->ObjectChanged (this);
         return TRUE;
      }
   }
   return FALSE;
}

/**********************************************************************************
CObjectGround::Message -
sends a message to the object. The interpretation of the message depends upon
dwMessage, which is OSM_XXX. If the function understands and handles the
message it returns TRUE, otherwise FALE.
*/
BOOL CObjectGround::Message (DWORD dwMessage, PVOID pParam)
{
   switch (dwMessage) {
   case OSM_IGNOREWORLDBOUNDINGBOXGET:
      return TRUE;

   case OSM_QUERYGROUND:
      return TRUE;   // yes' we're ground

   case OSM_FLOORLEVEL:
      {
         POSMFLOORLEVEL p = (POSMFLOORLEVEL) pParam;

         // translate to object space
         CPoint pObject;
         CMatrix mWorldToObject;
         m_MatrixObject.Invert4 (&mWorldToObject);
         pObject.Copy (&p->pTest);
         pObject.p[3] = 1;
         pObject.MultiplyLeft (&mWorldToObject);

         // figure out it's location...
         fp fH = pObject.p[0] / m_fScale / (fp)(m_dwWidth-1) + 0.5;
         fp fV = -pObject.p[1] / m_fScale / (fp)(m_dwHeight-1) + 0.5;
         if ((fH < 0) || (fH > 1) || (fV < 0) || (fV > 1))
            return FALSE;

         // get the height
         CPoint pHeight;
         HVToPoint (fH, fV, &pHeight);
         pHeight.MultiplyLeft (&m_MatrixObject);
         p->fLevel = pHeight.p[2];

         // water level
         p->fIsWater = m_fWater;
         if (m_fWater) {
            pHeight.Copy (&pObject);
            pHeight.p[2] = m_fWaterElevation;
            pHeight.MultiplyLeft (&m_MatrixObject);
            p->fWater = pHeight.p[2];
         }

         p->fArea = (fp)((m_dwWidth-1) * (m_dwHeight-1)) * m_fScale;
      }
      return TRUE;

   case OSM_GROUNDINFOGET:
      {
         POSMGROUNDINFOGET p = (POSMGROUNDINFOGET) pParam;
         memset (p, 0, sizeof(*p));
         p->dwHeight = m_dwHeight;
         p->dwWidth = m_dwWidth;
         p->fScale = m_fScale;
         p->pawElev = m_pawElev;
         p->tpElev = m_tpElev;
         p->fWater = m_fWater;
         p->fWaterElevation = m_fWaterElevation;
         p->fWaterSize = m_fWaterSize;
         memcpy (p->apWaterSurf, m_apWaterSurf, sizeof(m_apWaterSurf));
         memcpy (p->afWaterElev, m_afWaterElev, sizeof(m_afWaterElev));

         p->pabTextSet = m_pabTextSet;
         p->dwNumText = m_lTextPCObjectSurface.Num();
         p->paTextSurf = (PCObjectSurface*)m_lTextPCObjectSurface.Get(0);
         p->paTextColor = (COLORREF*)m_lTextCOLORREF.Get(0);

         // forest
         p->pabForestSet = m_pabForestSet;
         p->dwNumForest = m_lPCForest.Num();
         p->paForest = (PCForest*) m_lPCForest.Get(0);
      }
      return TRUE;

   case OSM_GROUNDINFOSET:
      {
         POSMGROUNDINFOSET p = (POSMGROUNDINFOSET) pParam;

         if (m_pWorld)
            m_pWorld->ObjectAboutToChange (this);

         if (!m_memElev.Required (p->dwHeight * p->dwWidth * sizeof(WORD)))
            return FALSE;
         if (!m_memTextSet.Required (p->dwHeight * p->dwWidth * sizeof(BYTE) * p->dwNumText))
            return FALSE;
         if (!m_memForestSet.Required (p->dwHeight * p->dwWidth * sizeof(BYTE) * p->dwNumForest))
            return FALSE;

         // free up all the surface info
         PCObjectSurface *ppos;
         ppos = (PCObjectSurface*) m_lTextPCObjectSurface.Get(0);
         DWORD i;
         for (i = 0; i < m_lTextPCObjectSurface.Num(); i++)
            delete ppos[i];
         m_lTextPCObjectSurface.Clear();
         m_lTextCOLORREF.Clear();

         PCForest *ppf;
         ppf = (PCForest*) m_lPCForest.Get(0);
         for (i = 0; i < m_lPCForest.Num(); i++)
            delete ppf[i];
         m_lPCForest.Clear();

         m_dwHeight = p->dwHeight;
         m_dwWidth = p->dwWidth;
         m_fScale = p->fScale;
         m_tpElev = p->tpElev;
         m_pawElev = (WORD*) m_memElev.p;
         memcpy (m_pawElev, p->pawElev, m_dwHeight * m_dwWidth * sizeof(WORD));


         for (i = 0; i < GWATERNUM*GWATEROVERLAP; i++) {
            if (m_apWaterSurf[i])
               delete m_apWaterSurf[i];
            m_apWaterSurf[i] = p->apWaterSurf[i]->Clone();
         }
         memcpy (m_afWaterElev, p->afWaterElev, sizeof(m_afWaterElev));

         m_fWater = p->fWater;
         m_fWaterElevation = p->fWaterElevation;
         m_fWaterSize = p->fWaterSize;

         // texture
         m_pabTextSet = (BYTE*) m_memTextSet.p;
         memcpy (m_pabTextSet, p->pabTextSet, m_dwHeight * m_dwWidth * sizeof(BYTE) * p->dwNumText);
         m_lTextCOLORREF.Init (sizeof(COLORREF), p->paTextColor, p->dwNumText);
         PCObjectSurface pos;
         m_lTextPCObjectSurface.Required (p->dwNumText);
         for (i = 0; i < p->dwNumText; i++) {
            pos = p->paTextSurf[i]->Clone();
            m_lTextPCObjectSurface.Add (&pos);
         }

         // forest
         m_pabForestSet = (BYTE*) m_memForestSet.p;
         memcpy (m_pabForestSet, p->pabForestSet, m_dwHeight * m_dwWidth * sizeof(BYTE) * p->dwNumForest);
         PCForest pf;
         m_lPCForest.Required (p->dwNumForest);
         for (i = 0; i < p->dwNumForest; i++) {
            pf = p->paForest[i]->Clone();
            m_lPCForest.Add (&pf);
         }

         // refresh
         m_fBBDirty = TRUE;
         m_fNormalsDirty = TRUE;
         FillInRENDERSURFACE ();

         if (m_pWorld)
            m_pWorld->ObjectChanged(this);

         // call objectmatrixset so goes through and tells all contained objects that
         // things have moved
         CMatrix m;
         ObjectMatrixGet (&m);
         ObjectMatrixSet (&m);
      }
      return TRUE;

   case OSM_GROUNDCUTOUT:
      {
         POSMGROUNDCUTOUT pg = (POSMGROUNDCUTOUT) pParam;

         // figure out a list of all the points it goes through
         CListFixed lInter;
         lInter.Init (sizeof(TEXTUREPOINT));
         DWORD i;
         CPoint pDir, pBack, pFront;
         if (pg->paBottom && pg->paTop) {
            CMatrix m;
            TEXTUREPOINT tp;
            m_MatrixObject.Invert4 (&m);

            lInter.Required (pg->dwNum);
            for (i = 0; i < pg->dwNum; i++) {
               pBack.Copy (&pg->paBottom[i]);
               pFront.Copy (&pg->paTop[i]);
               pBack.p[3] = pFront.p[3] = 1;
               pBack.MultiplyLeft (&m);
               pFront.MultiplyLeft (&m);

               pDir.Subtract (&pBack, &pFront);
               if (!IntersectLine (&pFront, &pDir, TRUE, NULL, NULL, &tp))
                  continue;
               lInter.Add (&tp);
            }
         }

         // find a match
         POGCUTOUT pc;
         pc = (POGCUTOUT) m_lOGCUTOUT.Get(0);
         for (i = 0; i < m_lOGCUTOUT.Num(); i++, pc++) {
            if (pc->fEmbedded || !IsEqualGUID(pc->gID, pg->gBuildBlock))
               continue;
            break;
         }
         if (i >= m_lOGCUTOUT.Num())
            pc = NULL;

         // if dont have anything and dont want to add anything then no problem
         if (!pc && (lInter.Num() < 3))
            return TRUE;

         if (m_pWorld)
            m_pWorld->ObjectAboutToChange (this);

         m_fNormalsDirty = TRUE; // so recalc

         // if have and dont want then delete
         if (pc && (lInter.Num() < 3)) {
            delete pc->plTEXTUREPOINT;
            m_fNormalsDirty = TRUE;
            m_lOGCUTOUT.Remove (i);
            if (m_pWorld)
               m_pWorld->ObjectChanged (this);
            return TRUE;
         }

         // else, want to add
         if (!pc) {
            OGCUTOUT cut;
            memset (&cut, 0, sizeof(cut));
            cut.gID = pg->gBuildBlock;
            cut.fEmbedded = FALSE;
            cut.plTEXTUREPOINT = new CListFixed;
            if (!cut.plTEXTUREPOINT) {
               if (m_pWorld)
                  m_pWorld->ObjectChanged(this);
               return FALSE;
            }
            m_lOGCUTOUT.Add (&cut);
            pc = (POGCUTOUT) m_lOGCUTOUT.Get(m_lOGCUTOUT.Num()-1);
         }

         // copy over
         pc->plTEXTUREPOINT->Init (sizeof(TEXTUREPOINT), lInter.Get(0), lInter.Num());

         if (m_pWorld)
            m_pWorld->ObjectChanged (this);

      }
      return TRUE;

   case OSM_INTERSECTLINE:
      {
         POSMINTERSECTLINE p = (POSMINTERSECTLINE) pParam;
         p->fIntersect = FALSE;

         // convert from world to object space
         CMatrix mInv;
         m_MatrixObject.Invert4 (&mInv);
         CPoint ps, pe;
         ps.Copy (&p->pStart);
         ps.p[3] = 1;
         ps.MultiplyLeft (&mInv);
         pe.Copy (&p->pEnd);
         pe.p[3] = 1;
         pe.MultiplyLeft (&mInv);
         pe.Subtract (&ps);

         p->fIntersect = IntersectLine (&ps, &pe, TRUE, NULL, &p->pIntersect, NULL);

         if (p->fIntersect) {
            p->pIntersect.p[3] = 1;
            p->pIntersect.MultiplyLeft (&m_MatrixObject);
         }
         return TRUE;
      }
      return TRUE;

   }

   return FALSE;
}



/**********************************************************************************
CObjectGround::DialogQuery - Returns TRUE if the object supports a custom
dialog box, FALSE if it doesn't. The default object doesn't
*/
BOOL CObjectGround::DialogQuery (void)
{
   return TRUE;
}


/* GroundDialogPage
*/
BOOL GroundDialogPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCObjectGround pv = (PCObjectGround)pPage->m_pUserData;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         // see if can find excavations
         POGCUTOUT pc = (POGCUTOUT) pv->m_lOGCUTOUT.Get(0);
         BOOL fFound = FALSE;
         DWORD i;
         PCEscControl pControl;
         for (i = 0; i < pv->m_lOGCUTOUT.Num(); i++, pc++) {
            if (!pc->fEmbedded) {
               fFound = TRUE;
               break;
            }
         }

         if (!fFound) {
            pControl = pPage->ControlFind (L"excavate");
            if (pControl)
               pControl->Enable (FALSE);
         }

         pControl = pPage->ControlFind (L"lessdetail");
         if (pControl)
            pControl->AttribSetBOOL (Checked(), pv->m_fLessDetail);
         pControl = pPage->ControlFind (L"forestboxes");
         if (pControl)
            pControl->AttribSetBOOL (Checked(), pv->m_fForestBoxes);
         pControl = pPage->ControlFind (L"savecompressed");
         if (pControl)
            pControl->AttribSetBOOL (Checked(), pv->m_fSaveCompressed);
         pControl = pPage->ControlFind (L"dontdrawneartrees");
         if (pControl)
            pControl->AttribSetBOOL (Checked(), pv->m_fDontDrawNearTrees);
      }
      break;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;

         // if it's for the colors then do those
         if (!_wcsicmp(p->pControl->m_pszName, L"lessdetail")) {
            BOOL fCheck = p->pControl->AttribGetBOOL (Checked());
            if (fCheck == pv->m_fLessDetail)
               return TRUE;


            // world going to change
            pv->m_pWorld->ObjectAboutToChange (pv);
            pv->m_fLessDetail = fCheck;
            pv->m_pWorld->ObjectChanged (pv);
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"forestboxes")) {
            BOOL fCheck = p->pControl->AttribGetBOOL (Checked());
            if (fCheck == pv->m_fForestBoxes)
               return TRUE;


            // world going to change
            pv->m_pWorld->ObjectAboutToChange (pv);
            pv->m_fForestBoxes = fCheck;
            pv->m_pWorld->ObjectChanged (pv);
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"savecompressed")) {
            BOOL fCheck = p->pControl->AttribGetBOOL (Checked());
            if (fCheck == pv->m_fSaveCompressed)
               return TRUE;


            // world going to change
            pv->m_pWorld->ObjectAboutToChange (pv);
            pv->m_fSaveCompressed = fCheck;
            pv->m_pWorld->ObjectChanged (pv);
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"dontdrawneartrees")) {
            BOOL fCheck = p->pControl->AttribGetBOOL (Checked());
            if (fCheck == pv->m_fDontDrawNearTrees)
               return TRUE;


            // world going to change
            pv->m_pWorld->ObjectAboutToChange (pv);
            pv->m_fDontDrawNearTrees = fCheck;
            pv->m_pWorld->ObjectChanged (pv);
            return TRUE;
         }
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Ground settings";
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}


/* GroundCutoutMainPage
*/
BOOL GroundCutoutMainPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCObjectGround pv = (PCObjectGround)pPage->m_pUserData;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         // fill the listbox
         PCEscControl pControl;
         pControl = pPage->ControlFind (L"ex");

         DWORD i;
         POGCUTOUT pc = (POGCUTOUT) pv->m_lOGCUTOUT.Get(0);
         ESCMLISTBOXADD lba;
         for (i = 0; i < pv->m_lOGCUTOUT.Num(); i++, pc++) {
            if (pc->fEmbedded)
               continue;

            // get the name
            PCObjectSocket pos;
            PWSTR psz;
            psz = NULL;
            pos = pv->m_pWorld->ObjectGet(pv->m_pWorld->ObjectFind(&pc->gID));
            if (pos)
               psz = pos->StringGet (OSSTRING_NAME);
            if (!psz || !psz[0])
               psz = L"Unnamed";

            memset (&lba, 0, sizeof(lba));
            lba.dwInsertBefore = -1;
            lba.pszText = psz;
            pControl->Message (ESCM_LISTBOXADD, &lba);
         }

         // set selection?
         pControl->AttribSetInt (CurSel(), 0);
      }
      break;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;

         // if it's for the colors then do those
         if (!_wcsicmp(p->pControl->m_pszName, L"remove")) {
            PCEscControl pControl;
            DWORD dwSel, dwSelOrig;
            pControl = pPage->ControlFind (L"ex");
            dwSelOrig = dwSel = pControl->AttribGetInt (CurSel());

            POGCUTOUT pc = (POGCUTOUT) pv->m_lOGCUTOUT.Get(0);
            DWORD i;
            for (i = 0; i < pv->m_lOGCUTOUT.Num(); i++, pc++) {
               if (pc->fEmbedded)
                  continue;
               if (dwSel) {
                  // not this one
                  dwSel--;
                  continue;
               }

               // else this one
               break;
            }
            if (dwSel || (i >= pv->m_lOGCUTOUT.Num())) {
               pPage->MBWarning (L"You must select an excation to delete.");
               return TRUE;
            }

            // world going to change
            pv->m_pWorld->ObjectAboutToChange (pv);
            if (pc->plTEXTUREPOINT)
               delete pc->plTEXTUREPOINT;
            pv->m_lOGCUTOUT.Remove (i);
            pv->m_fNormalsDirty = TRUE;
            pv->m_pWorld->ObjectChanged (pv);

            // remove from list
            ESCMLISTBOXDELETE lbd;
            memset (&lbd, 0, sizeof(lbd));
            lbd.dwIndex = dwSelOrig;
            pControl->Message (ESCM_LISTBOXDELETE, &lbd);
            pControl->AttribSetInt (CurSel(), 0);

            return TRUE;
         }
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Basement/excavation cutouts";
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}


/**********************************************************************************
CObjectGround::DialogShow - Causes the object to show a dialog box that allows
different attributes to be changed.

inputs
   DWORD             dwSuface - Surface that was clicked on. 0 if unknown.
   PCEscWindow       pWindow - Escarpment window to draw it in
returns
   BOOL - TRUE if user pressed "Back", FALSE if user pressed "Close"
*/
BOOL CObjectGround::DialogShow (DWORD dwSurface, PCEscWindow pWindow, PCPoint pClick)
{
   PWSTR pszRet;

firstpage:
   pszRet = pWindow->PageDialog (ghInstance, IDR_MMLGROUNDDIALOG, GroundDialogPage, this);
firstpage2:
   if (!pszRet)
      return FALSE;
   if (!_wcsicmp(pszRet, L"excavate")) {
      pszRet = pWindow->PageDialog (ghInstance, IDR_MMLGROUNDCUTOUTMAIN, GroundCutoutMainPage, this);
      if (pszRet && !_wcsicmp(pszRet, Back()))
         goto firstpage;
      else
         goto firstpage2;
   }
   return !_wcsicmp(pszRet, Back());
}


/**********************************************************************************
CObjectGround::IntersectLine - Intersects a line with the ground and returns
the nearest point to pStart.

inputs
   PCPoint        pStart - Start of the line
   PCPoint        pDir - Direction of the line
   BOOL           fBehind - If TRUE will returns a point behend pStart, else will
                     only returns point in pStart + n * pDir, where n >= 0
   fp             *pfAlpha - Filled with the alpha, where intersection = pStart + fAlpha * pDir,
                     where intersects. Can be null.
   PCPoint        pInter - Filled with the closest intersection point. Can be NULL
   PTEXTUREPOINT  pText - Filled within the intersection texture point. Can be NULL
returns
   BOOL - TRUE if found intersection, FALSE if not
*/
BOOL CObjectGround::IntersectLine (PCPoint pStart, PCPoint pDir, BOOL fBehind,
                                   fp *pfAlpha, PCPoint pInter, PTEXTUREPOINT pText)
{
   // make sure have bounding boxes
   CalcBoundingBoxes ();

   // assume no intersect
   BOOL fFound = FALSE;
   fp fBestAlpha;
   TEXTUREPOINT tpHV, tpBest;

   // because the bounding box intersection is very particular about distance, lengthen
   // the intersection line
   fp fLen;
   fLen = pDir->Length();
   if (fabs(fLen) < EPSILON)
      return FALSE;  // too small of a delta
   fLen = 100000 / fLen;  // make a large #
   CPoint pS, pD;
   pS.Copy (pDir);
   pS.Scale (-fLen);
   pS.Add (pStart);
   pD.Copy (pDir);
   pD.Scale (1 + 2 * fLen);


   // loop through all bounding boxes
   CPoint pUL, pUR, pLL, pLR;
   DWORD dwBB;
   fp fCurAlpha;
   PGROUNDSEC pgs;
   pgs = (PGROUNDSEC) m_lGROUNDSEC.Get(0);
   for (dwBB = 0; dwBB < m_lGROUNDSEC.Num(); dwBB++, pgs++) {
      // see if it intersects
      if (!IntersectBoundingBox (&pS, &pD, &pgs->pBoundMin, &pgs->pBoundMax))
         continue;   // missed the boundig box

      // else, go through all sub bits
      DWORD xx, yy, dwMaxX, dwMaxY;
      dwMaxX = min(pgs->dwXMax, m_dwWidth-1);
      dwMaxY = min(pgs->dwYMax, m_dwHeight-1);
      for (yy = pgs->dwYMin; yy < dwMaxY; yy++)
         for (xx = pgs->dwXMin; xx < dwMaxX; xx++) {
            // get the corners
            Elevation (xx, yy, &pUL);
            Elevation (xx+1, yy, &pUR);   // dont worry about modulo because  in dwMaxX and dwMaxY
            Elevation (xx+1, yy+1, &pLR);
            Elevation (xx, yy+1, &pLL);

            if (!IntersectLineQuad (pStart, pDir, &pUL, &pUR, &pLR, &pLL,
               NULL, &fCurAlpha, pText ? &tpHV : NULL))
               continue;

            // else, intersected
            if (!fBehind && (fCurAlpha < 0))
               continue;   // cant take points behind

            if (!fFound || (fabs(fCurAlpha) < fabs(fBestAlpha))) {
               // found match
               fFound = TRUE;
               fBestAlpha = fCurAlpha;

               if (pText) {
                  tpBest.h = ((fp)xx + tpHV.h) / (fp) (m_dwWidth-1);
                  tpBest.v = ((fp)yy + tpHV.v) / (fp) (m_dwHeight-1);
               }  // if pText
            } // if found

         }  // xx, yy
   } // x and y

   if (!fFound)
      return FALSE;

   // set values
   if (pfAlpha)
      *pfAlpha = fBestAlpha;
   if (pInter) {
      pInter->Copy (pDir);
      pInter->Scale (fBestAlpha);
      pInter->Add (pStart);
   }
   if (pText)
      *pText = tpBest;
   return TRUE;
}


/**********************************************************************************
CObjectGround::HVToPoint - Takes an HV value (0..1, 0..1) and returns a point.

inputs
   fp       fH, fV - Horizontal and vertical values from 0..1, 0..1 (0,0 = NW) (1,1 = SE)
   PCPoint  pLoc - Fills in the location
returns
   none
*/
void CObjectGround::HVToPoint (fp fH, fp fV, PCPoint pLoc)
{
   // make sure it's within the limits
   fp fHOrig = fH, fVOrig = fV;
   fH *= (fp)(m_dwWidth-1);
   fV *= (fp)(m_dwHeight-1);
   fH = max(0,fH);
   fH = min((fp)m_dwWidth-1-CLOSE,fH);
   fV = max(1, fV);
   fV = min((fp)m_dwHeight-1-CLOSE,fV);

   // get the point
   WORD *pw;
   pw = &m_pawElev[(DWORD)fH + (DWORD)fV * m_dwWidth];
   fH -= floor(fH);
   fV -= floor(fV);

   // figure out top and bottom values
   fp fTop, fBottom, fValue;
   fTop = (1.0 - fH) * (fp)pw[0] + fH * (fp)pw[1];
   fBottom = (1.0 - fH) * (fp)pw[m_dwWidth] + fH * (fp)pw[m_dwWidth+1];
   fValue = (1.0 - fV) * fTop + fV * fBottom;

   // use this
   pLoc->p[0] = (fHOrig - .5) * (fp)(m_dwWidth-1) * m_fScale;
   pLoc->p[1] = -(fVOrig - .5) * (fp)(m_dwHeight-1) * m_fScale;
   pLoc->p[2] = fValue / (fp)0xffff * (m_tpElev.v - m_tpElev.h) +
      m_tpElev.h;
   pLoc->p[3] = 1;

   // NOTE: This glosses over the crease encountered in some quads. This
   // may cause problems in the future and may need to be fixes
}


/**********************************************************************************
CObjectGround::CalcOGGrid - Fills in m_lOGGRID based on all the cutouts.
*/
void CObjectGround::CalcOGGrid (void)
{
   // free up grid
   DWORD i, j;
   POGGRID pg = (POGGRID) m_lOGGRID.Get(0);
   for (i = 0; i < m_lOGGRID.Num(); i++, pg++) {
      if (pg->pGrid)
         delete pg->pGrid;
   }
   m_lOGGRID.Clear();

   // if not cutouts trivial
   if (!m_lOGCUTOUT.Num())
      return;

   // loop through all the cutouts and remember their min and max
   POGCUTOUT pc;
   PTEXTUREPOINT ptp;
   pc = (POGCUTOUT) m_lOGCUTOUT.Get(0);
   for (i = 0; i < m_lOGCUTOUT.Num(); i++, pc++) {
      ptp = (PTEXTUREPOINT) pc->plTEXTUREPOINT->Get(0);
      for (j = 0; j < pc->plTEXTUREPOINT->Num(); j++, ptp++) {
         if (j) {
            pc->tpMin.h = min(pc->tpMin.h, ptp->h);
            pc->tpMin.v = min(pc->tpMin.v, ptp->v);
            pc->tpMax.h = max(pc->tpMax.h, ptp->h);
            pc->tpMax.v = max(pc->tpMax.v, ptp->v);
         }
         else {
            pc->tpMin = *ptp;
            pc->tpMax = *ptp;
         }
      } // j

      // scale the min and max
      pc->tpMin.h *= (fp) (m_dwWidth - 1);
      pc->tpMin.v *= (fp) (m_dwWidth - 1);
      pc->tpMax.h *= (fp) (m_dwHeight - 1);
      pc->tpMax.v *= (fp) (m_dwHeight - 1);
   } // i

   // loop through all the meshes.. if any are affected by the grids then look at them
   DWORD x,y;
   BOOL fAffect;
   TEXTUREPOINT tpMin, tpMax;
   OGGRID og;
   pc = (POGCUTOUT) m_lOGCUTOUT.Get(0);
   i = 0;
   for (y = 0; y < m_dwHeight-1; y++) for (x = 0; x < m_dwWidth-1; x++) {
      i = x + y * m_dwHeight;

      // are affected
      fAffect = FALSE;
      for (j = 0; j < m_lOGCUTOUT.Num(); j++) {
         tpMax.h = min((fp)x+1.0, pc[j].tpMax.h);
         tpMin.h = max((fp)x, pc[j].tpMin.h);
         tpMax.v = min((fp)y+1.0, pc[j].tpMax.v);
         tpMin.v = max((fp)y, pc[j].tpMin.v);

         pc[j].fAffect = ((tpMax.h >= tpMin.h) && (tpMax.v >= tpMin.v));
         fAffect |= pc[j].fAffect;
      }
      if (!fAffect)
         continue;   // no changes

      // set up og
      memset (&og, 0, sizeof(og));
      og.dwID = i;
      og.pGrid = new CMeshGrid;
      if (!og.pGrid)
         continue;   // error

      // figure out edges
      CPoint pUL, pUR, pLL, pLR;
      PCPoint pnUL, pnUR, pnLL, pnLR;
      TEXTUREPOINT tUL, tUR, tLL, tLR;
      Elevation (x, y, &pUL);
      Elevation (x+1, y, &pUR);
      Elevation (x+1, y+1, &pLR);
      Elevation (x, y+1, &pLL);
      pnUL = ((PCPoint)m_memNormals.p) + i;
      pnUR = pnUL + 1;
      pnLL = pnUL + m_dwWidth;
      pnLR = pnUR + m_dwWidth;
      tUL.h = pUL.p[0];
      tUL.v = -pUL.p[1];
      tUR.h = pUR.p[0];
      tUR.v = -pUR.p[1];
      tLL.h = pLL.p[0];
      tLL.v = -pLL.p[1];
      tLR.h = pLR.p[0];
      tLR.v = -pLR.p[1];

      // should do mesh grid intersection
      og.pGrid->Init (
         0, 1, 0, 1,
         &pUL, &pUR, &pLL, &pLR,
         pnUL, pnUR, pnLL, pnLR,
         &tUL, &tUR, &tLL, &tLR);

      // intersect
      TEXTUREPOINT tp1, tp2;
      DWORD k, dwNum;
      for (j = 0; j < m_lOGCUTOUT.Num(); j++) {
         // if doesnt affect then ignore
         if (!pc[j].fAffect)
            continue;

         // scale the points
         ptp = (PTEXTUREPOINT) pc[j].plTEXTUREPOINT->Get(0);
         dwNum = pc[j].plTEXTUREPOINT->Num();
         for (k = 0; k < dwNum; k++) {
            // get
            tp1 = ptp[k];
            tp2 = ptp[(k+1)%dwNum];

            // scale
            tp1.h = tp1.h * (fp)(m_dwWidth-1) - (fp) x;
            tp2.h = tp2.h * (fp)(m_dwWidth-1) - (fp) x;
            tp1.v = tp1.v * (fp)(m_dwHeight-1) - (fp) y;
            tp2.v = tp2.v * (fp)(m_dwHeight-1) - (fp) y;

            // send down
            og.pGrid->IntersectLineWithGrid (&tp1, &tp2);
         } // k
      } // j

      // generate all the quads
      if (!og.pGrid->GenerateQuads ()) {
         delete og.pGrid;
         og.pGrid = NULL;
      }

      // if trivial accept then don't add anything
      if (og.pGrid && og.pGrid->m_fTrivialAccept) {
         delete og.pGrid;
         continue;
      }

      // if trivial reject then just store away null
      if (og.pGrid && og.pGrid->m_fTrivialClip) {
         delete og.pGrid;
         og.pGrid = NULL;
      }

      // add hit
      m_lOGGRID.Add (&og);
   } // xy
}

/**********************************************************************************
CObjectGround::FillInRENDERSURFACE - This must be called whenever m_lTextPCObjectSurface
changes, and just to be sure, call just before rendering. The list is used
by the the texture object.
*/
void CObjectGround::FillInRENDERSURFACE (void)
{
   DWORD dwGroundThread;
   for (dwGroundThread = 0; dwGroundThread < GROUNDTHREADS; dwGroundThread++) {
#ifdef USECRITSEC
      EnterCriticalSection (&m_aCritSec[dwGroundThread]);
#endif
      m_alRENDERSURFACEPLUS[dwGroundThread].Init (sizeof(RENDERSURFACEPLUS));
      m_alRENDERSURFACEPLUS[dwGroundThread].Clear();
      RENDERSURFACEPLUS rsp;

      // get from the texture cache
      PCObjectSurface *ppos;
      PCObjectSurface pos;
      DWORD i;
      memset (&rsp, 0, sizeof(rsp));
      ppos = (PCObjectSurface*)m_lTextPCObjectSurface.Get(0);
      for (i = 0; i < m_lTextPCObjectSurface.Num(); i++) {
         pos = ppos[i];

         if (pos->m_szScheme[0] && m_pWorld) {
            PCSurfaceSchemeSocket pss;
            pss = m_pWorld->SurfaceSchemeGet ();
            if (pss)
               pos = pss->SurfaceGet(pos->m_szScheme, pos, FALSE);
            if (!pos)
               pos = ppos[i];
         }

         memcpy (rsp.rs.afTextureMatrix, pos->m_afTextureMatrix, sizeof(rsp.rs.afTextureMatrix));
         memcpy (rsp.rs.abTextureMatrix, &pos->m_mTextureMatrix, sizeof(pos->m_mTextureMatrix));
         rsp.rs.fUseTextureMap = pos->m_fUseTextureMap;
         rsp.rs.gTextureCode = pos->m_gTextureCode;
         rsp.rs.gTextureSub = pos->m_gTextureSub;
         memcpy (&rsp.rs.Material, &pos->m_Material, sizeof(rsp.rs.Material));
         wcscpy (rsp.rs.szScheme, pos->m_szScheme);
         memcpy (&rsp.rs.TextureMods, &pos->m_TextureMods, sizeof(rsp.rs.TextureMods));

         if (pos != ppos[i])
            delete pos;

         // add it
         rsp.qwTextureCacheTimeStamp = 0;
         rsp.pTexture = NULL;
         m_alRENDERSURFACEPLUS[dwGroundThread].Add (&rsp);
      }

      // clear water rendersurface
      memset (m_aWaterRS[dwGroundThread], 0, sizeof (m_aWaterRS[dwGroundThread]));
      for (i = 0; i < GWATERNUM*GWATEROVERLAP; i++) {
         pos = m_apWaterSurf[i];

         if (pos->m_szScheme[0] && m_pWorld) {
            PCSurfaceSchemeSocket pss;
            pss = m_pWorld->SurfaceSchemeGet ();
            if (pss)
               pos = pss->SurfaceGet(pos->m_szScheme, pos, FALSE);
            if (!pos)
               pos = m_apWaterSurf[i];
         }

         RENDERSURFACE rs;
         memset (&rs, 0, sizeof(rs));

         memcpy (rs.afTextureMatrix, pos->m_afTextureMatrix, sizeof(rs.afTextureMatrix));
         memcpy (rs.abTextureMatrix, &pos->m_mTextureMatrix, sizeof(pos->m_mTextureMatrix));
         rs.fUseTextureMap = pos->m_fUseTextureMap;
         rs.gTextureCode = pos->m_gTextureCode;
         rs.gTextureSub = pos->m_gTextureSub;
         memcpy (&rs.Material, &pos->m_Material, sizeof(rs.Material));
         wcscpy (rs.szScheme, pos->m_szScheme);
         memcpy (&rs.TextureMods, &pos->m_TextureMods, sizeof(rs.TextureMods));

         if (pos != m_apWaterSurf[i])
            delete pos;

         // add it
         m_aWaterRS[dwGroundThread][i].rs = rs;
         m_aWaterRS[dwGroundThread][i].pTexture = NULL;
         m_aWaterRS[dwGroundThread][i].qwTextureCacheTimeStamp = 0;
      }
#ifdef USECRITSEC
   LeaveCriticalSection (&m_aCritSec[dwGroundThread]);
#endif
   } // dwGroundThread
}


/**********************************************************************************
CObjectGround::TextureQuery -
asks the object what textures it uses. This allows the save-function
to save custom textures into the file. The object just ADDS (doesn't
clear or remove) elements, which are two guids in a row: the
gCode followed by the gSub of the object. Of course, it may add more
than one texture
*/
BOOL CObjectGround::TextureQuery (PCListFixed plText)
{
   DWORD i;
   GUID ag[2];
   DWORD dwNumSurf = m_lTextPCObjectSurface.Num();
   PCObjectSurface *ppos = (PCObjectSurface*) m_lTextPCObjectSurface.Get(0);
   for (i = 0; i < dwNumSurf + GWATERNUM * GWATEROVERLAP; i++) {
      PCObjectSurface pos;
      if (i < dwNumSurf)
         pos = ppos[i];
      else
         pos = m_apWaterSurf[i-dwNumSurf];

      if (pos->m_szScheme[0] && m_pWorld) {
         PCObjectSurface p2;
         p2 = (m_pWorld->SurfaceSchemeGet())->SurfaceGet (pos->m_szScheme, pos, TRUE);
         if (p2) {
            if (p2->m_fUseTextureMap) {
               ag[0] = p2->m_gTextureCode;
               ag[1] = p2->m_gTextureSub;
               plText->Add (ag);
            }
            // since noclone in SurfaceGet() delete p2;
            continue;
         }
      }
      if (!pos->m_fUseTextureMap)
         continue;

      ag[0] = pos->m_gTextureCode;
      ag[1] = pos->m_gTextureSub;
      plText->Add (ag);
   }

   // Will need to call texture query for all the forests
   PCForest *ppf;
   ppf = (PCForest*) m_lPCForest.Get(0);
   for (i = 0; i < m_lPCForest.Num(); i++)
      ppf[i]->TextureQuery (m_OSINFO.dwRenderShard, plText);

   return TRUE;
}



/**********************************************************************************
CObjectGround::ColorQuery -
asks the object what colors it uses (exclusive of textures).
It adds elements to plColor, which is a list of COLORREF. It may
add more than one color
*/
BOOL CObjectGround::ColorQuery (PCListFixed plColor)
{
   DWORD i;
   DWORD dwNumSurf = m_lTextPCObjectSurface.Num();
   PCObjectSurface *ppos = (PCObjectSurface*) m_lTextPCObjectSurface.Get(0);
   for (i = 0; i < dwNumSurf + GWATERNUM*GWATEROVERLAP; i++) {
      PCObjectSurface pos;
      if (i < dwNumSurf)
         pos = ppos[i];
      else
         pos = m_apWaterSurf[i-dwNumSurf];

      if (pos->m_szScheme[0] && m_pWorld) {
         PCObjectSurface p2;
         p2 = (m_pWorld->SurfaceSchemeGet())->SurfaceGet (pos->m_szScheme, pos, TRUE);
         if (p2) {
            if (!p2->m_fUseTextureMap) {
               plColor->Add (&p2->m_cColor);
            }
            // since noclone in SurfaceGet() delete p2;
            continue;
         }
      }
      if (pos->m_fUseTextureMap)
         continue;

      plColor->Add (&pos->m_cColor);
   }

   // Will need to call colorquery for all the forests
   PCForest *ppf;
   ppf = (PCForest*) m_lPCForest.Get(0);
   for (i = 0; i < m_lPCForest.Num(); i++)
      ppf[i]->ColorQuery (m_OSINFO.dwRenderShard, plColor);

   return TRUE;
}


/**********************************************************************************
CObjectGround::ObjectClassQuery
asks the curent object what other objects (including itself) it requires
so that when a file is saved, all user objects will be saved along with
the file, so people on other machines can load them in.
The object just ADDS (doesn't clear or remove) elements, which are two
guids in a row: gCode followed by gSub of the object. All objects
must add at least on (their own). Some, like CObjectEditor, will add
all its sub-objects too
*/
BOOL CObjectGround::ObjectClassQuery (PCListFixed plObj)
{
   // call into template
   CObjectTemplate::ObjectClassQuery (plObj);


   // Will need to call colorquery for all the forests
   PCForest *ppf;
   DWORD i;
   ppf = (PCForest*) m_lPCForest.Get(0);
   for (i = 0; i < m_lPCForest.Num(); i++)
      ppf[i]->ObjectClassQuery (m_OSINFO.dwRenderShard, plObj);

   return TRUE;
}




/**********************************************************************************
CObjectGround::RenderSection - Draws one section of the ground.

inputs
   DWORD       dwXMin, dwXMax - X range, in pixels. (exclusive of dwXMax) dwXMax < m_dwWidth
   DWORD       dwYMin, dwYMax - Y range in pixels. (exclusive of dwYMax) dwYMax < m_dwHeight
   POBJECTRENDER     pr - Render information
   fp          fDetail - Detail to use
   COLORREF    cGround - Ground color
   RENDERSURFACE *prsGround - Ground surface
returns
   none
*/
void CObjectGround::RenderSection (DWORD dwXMin, DWORD dwXMax, DWORD dwYMin, DWORD dwYMax,
                                 POBJECTRENDER pr, fp fDetail, COLORREF cGround, RENDERSURFACE *prsGround)
{
   // create the surface render object and draw
   //CRenderSurface rs;
   m_lRenderSectionrs.ClearAll();
   CMatrix mObject;
   m_lRenderSectionrs.Init (pr->pRS);
   ObjectMatrixGet (&mObject);
   m_lRenderSectionrs.Multiply (&mObject);

   // draw with less detail?
   BOOL fLessDetail = m_fLessDetail && (pr->dwReason != ORREASON_FINAL);

   // if this section has any cutouts then dont do less detail
   DWORD x,y, i;
   POGGRID pg;
   DWORD dwNumGrid;
   if (fLessDetail && m_lOGGRID.Num()) {
      pg = (POGGRID) m_lOGGRID.Get(0);
      dwNumGrid = m_lOGGRID.Num();
      for (i = 0; i < dwNumGrid; i++) {
         // only draw if in area that care about
         x = pg->dwID % m_dwWidth;
         y = pg->dwID / m_dwWidth;
         if ((x < dwXMin) || (y < dwYMin) || (x >= dwXMax) || (y >= dwYMax))
            continue;

         // else, have something
         fLessDetail = FALSE;
         break;
      }
   }

   // set ground texture
   m_lRenderSectionrs.SetDefColor (cGround);
   m_lRenderSectionrs.SetDefMaterial (prsGround);
   //m_lRenderSectionrs.SetDefMaterial (ObjectSurfaceFind (SIDEA), m_pWorld);

   // what's the width of this section
   DWORD dwWidth, dwHeight, dwDetail;
   dwDetail = (fLessDetail ? LESSDETAIL : 1);
   dwWidth = (dwXMax - dwXMin + dwDetail-1) / dwDetail + 1;
   dwHeight = (dwYMax - dwYMin + dwDetail-1) / dwDetail + 1;

   // Draw surface
   DWORD dwPoints, dwNormals, dwVertices, dwTextures;
   PCPoint pPoints, pNormals;
   PVERTEX pVertices;
   PTEXTPOINT5 pText;
   pPoints = m_lRenderSectionrs.NewPoints (&dwPoints, dwWidth * dwHeight);
   if (!pPoints) {
      m_lRenderSectionrs.Commit();
      return;
   }
   pVertices = m_lRenderSectionrs.NewVertices (&dwVertices, dwWidth * dwHeight);
   if (!pVertices) {
      m_lRenderSectionrs.Commit();
      return;
   }
   pNormals = m_lRenderSectionrs.NewNormals (TRUE, &dwNormals, dwWidth * dwHeight);
   pText = m_lRenderSectionrs.NewTextures (&dwTextures, dwWidth * dwHeight);
   i = 0;
   DWORD xx, yy;
   for (y = 0; y < dwHeight; y++) for (x = 0; x < dwWidth; x++, i++) {
      xx = min(x * dwDetail + dwXMin, dwXMax);  // can do this min because dwXMax is <= dwWIdth-1
      yy = min(y * dwDetail + dwYMin, dwYMax); // can do this because dwYMax is <= dwHeight-1
      Elevation (xx, yy, &pPoints[i]);
      pVertices[i].dwPoint = i + dwPoints;

      if (pNormals) {
         //pNormals[i].Copy (((PCPoint)m_memNormals.p) + i);
         // BUGFIX - Was wrong values
         pNormals[i].Copy (((PCPoint)m_memNormals.p) + (xx + yy * m_dwWidth));
         pVertices[i].dwNormal = i + dwVertices;
      }
      else
         pVertices[i].dwNormal = 0;

      if (pText) {
         pText[i].hv[0] = (fp) xx;//pPoints[i].p[0];
         pText[i].hv[1] = (fp) yy;//-pPoints[i].p[1];

         pText[i].xyz[0] = pPoints[i].p[0];
         pText[i].xyz[1] = pPoints[i].p[1];
         pText[i].xyz[2] = pPoints[i].p[2];

         pVertices[i].dwTexture = i + dwTextures;
      }
      else
         pVertices[i].dwTexture = 0;
   }

   // transform texture
   // BUGFIX - No longer apply rotation
   //if (pText)
   //   m_lRenderSectionrs.ApplyTextureRotation (pText, dwWidth * dwHeight);

   // draw quads
   pg = (POGGRID) m_lOGGRID.Get(0);
   dwNumGrid = m_lOGGRID.Num();
   DWORD iMain;
   for (y = 0; y < dwHeight-1; y++) for (x = 0; x < dwWidth-1; x++) {
      i = x + y * dwWidth;
      xx = min(x * dwDetail + dwXMin, dwXMax);  // can do this min because dwXMax is <= dwWIdth-1
      yy = min(y * dwDetail + dwYMin, dwYMax); // can do this because dwYMax is <= dwHeight-1
      iMain = xx + yy * m_dwWidth;

      // see if this is one of the grids to skip
      while (dwNumGrid && (pg->dwID < iMain)) {
         dwNumGrid--;
         pg++;
      }
      if (dwNumGrid && (pg->dwID == iMain))
         continue;   // found a quad that should be skipped

      // draw
      i += dwVertices;
      m_lRenderSectionrs.NewQuad (i, i+1, i+1+dwWidth, i+dwWidth);
   }

   // now go back over and draw the quads that are partially cutout
   pg = (POGGRID) m_lOGGRID.Get(0);
   dwNumGrid = m_lOGGRID.Num();
   for (i = 0; i < dwNumGrid; i++, pg++) {
      if (!pg->pGrid)
         continue;

      // dont need to worry about dwDetail because if it hits here dwDetail==1

      // only draw if in area that care about
      x = pg->dwID % m_dwWidth;
      y = pg->dwID / m_dwWidth;
      if ((x < dwXMin) || (y < dwYMin) || (x >= dwXMax) || (y >= dwYMax))
         continue;

      // draw
      pg->pGrid->DrawQuads (&m_lRenderSectionrs);
   }

   m_lRenderSectionrs.Commit();
}

/**********************************************************************************
CObjectGround::RenderTrees - Draw all the trees in a given area.

inputs
   DWORD       dwXMin, dwXMax - X range, in pixels. (exclusive of dwXMax) dwXMax < m_dwWidth
   DWORD       dwYMin, dwYMax - Y range in pixels. (exclusive of dwYMax) dwYMax < m_dwHeight
   POBJECTRENDER     pr - Render information
   fp          fDetail - Detail to render in. If this is < 0 then calcs for every treee
   BOOL        fForestBoxes - If TRUE then draw trees as boxes for an optimization
   PCPoint     pCamera - Camera location (or NULL). Used to make sure that the camera isn't
               hidden by a tree
   DWORD       dwSubRendering - If -1 then all canopies, else canopy number
returns
   none
*/
void CObjectGround::RenderTrees (DWORD dwXMin, DWORD dwXMax, DWORD dwYMin, DWORD dwYMax,
                                 POBJECTRENDER pr, fp fDetail, BOOL fForestBoxes, PCPoint pCamera,
                                 DWORD dwSubRendering)
{
	MALLOCOPT_INIT;

   // figure out this location...
   CPoint pCorner1, pCorner2;
   pCorner1.Zero();
   pCorner2.Zero();
   XYToLoc (dwXMin, dwYMin, &pCorner1);
   XYToLoc (dwXMax, dwYMax, &pCorner2);
   //pCorner1.p[0] = ((fp)dwXMin - (fp)(m_dwWidth-1)/2) * m_fScale;
   //pCorner1.p[1] = -((fp)dwYMin - (fp)(m_dwHeight-1)/2) * m_fScale;
   //pCorner2.p[0] = ((fp)dwXMax - (fp)(m_dwWidth-1)/2) * m_fScale;
   //pCorner2.p[1] = -((fp)dwYMax - (fp)(m_dwHeight-1)/2) * m_fScale;

   // draw the forests as boxes
   // CRenderSurface rs;
   m_lRenderTreesrs.ClearAll();
   if (fForestBoxes) {
      CObjectSurface os;
      os.m_dwID = TREEID;
      os.m_Material.InitFromID (MATERIAL_FLAT);
      os.m_cColor = RGB(0,0xff,0);

      m_lRenderTreesrs.Init (pr->pRS);
      m_lRenderTreesrs.Multiply (&m_MatrixObject);
      m_lRenderTreesrs.SetDefMaterial (m_OSINFO.dwRenderShard, &os, m_pWorld);
   }

   // find out if the renderer supports clones
   BOOL fCloneRender;
   // CListFixed lCloneMatrix;
   fCloneRender = pr->pRS->QueryCloneRender();
   m_lRenderTreesCloneMatrix.Init (sizeof(CMatrix));

   // figure out where the camera is relative to the ground
   CPoint pCameraGround;
   CMatrix mWorldToGround;
   if (pCamera) {
      pCameraGround.Copy (pCamera);
      pCameraGround.p[3] = 1;
      if (fForestBoxes) {
         m_MatrixObject.Invert4 (&mWorldToGround);
         pCameraGround.MultiplyLeft (&mWorldToGround);
         // NOTE: If !fForestBoxes then including the ground later
      }
   }
   // loop through all the forests
   DWORD dwForest;
   PCForest *ppf;
   int x,y;
   ppf = (PCForest*) m_lPCForest.Get(0);
   DWORD j;
   for (dwForest = 0; dwForest < m_lPCForest.Num(); dwForest++) {
      // see if it's used
      BYTE *pabCurForest = m_pabForestSet + dwForest * m_dwWidth * m_dwHeight;
      BYTE *pabCur;
      BOOL fFound;
      fFound = FALSE;
      for (y = (int)dwYMin; y < (int)dwYMax; y++) {
         pabCur = pabCurForest + (dwXMin + y * m_dwWidth);
         for (x = (int)dwXMin; x < (int)dwXMax; x++, pabCur++)
            if (pabCur[0]) {
               fFound = TRUE;
               break;
            }
         if (fFound)
            break;
      }
      if (!fFound)
         continue;   // not used

      if (fForestBoxes)
         m_lRenderTreesrs.SetDefColor (ppf[dwForest]->m_cColor);

      // go through all the canopies
      DWORD dwCanopy;
      PCForestCanopy pCanopy;
      RECT rLoop;
      DWORD dwCanopyMax = (dwSubRendering == (DWORD)-1) ? NUMFORESTCANOPIES : (dwSubRendering + 1);
      for (dwCanopy = (dwSubRendering == (DWORD)-1) ? 0 : dwSubRendering; dwCanopy < dwCanopyMax; dwCanopy++) {
         pCanopy = ppf[dwForest]->m_apCanopy[dwCanopy];

         // if the largest tree in the canopy is too small then dont draw
         if (pCanopy->MaxTreeSize(m_OSINFO.dwRenderShard) < fDetail)
            continue;

         // BUGFIX - if this canopy is marked as not having shadows, and doing
         // shadows calculation, then skip
         if (pCanopy->m_fNoShadows && (pr->dwReason == ORREASON_SHADOWS))
            continue;

         // see the range to loop over here
         pCanopy->TreesInRange (m_OSINFO.dwRenderShard, &pCorner1, &pCorner2, &rLoop);
         if ((rLoop.left == rLoop.right) || (rLoop.top == rLoop.bottom))
            continue;

         // loop through all the tree types
         DWORD dwTree;
         CMatrix mTree, mTrans;
         CPoint pLoc;
         PCObjectClone pClone;
         for (dwTree = 0; dwTree < pCanopy->m_lCANOPYTREE.Num(); dwTree++) {
            m_lRenderTreesCloneMatrix.Clear();

            pClone = pCanopy->TreeClone (m_OSINFO.dwRenderShard, dwTree);
            if (!pClone)
               continue;

            // if tree too small then dont draw
            if (pClone->MaxSize() < fDetail)
               continue;

            // loop through these
            BYTE bScore;
            BOOL fIfDistantEliminate;
            for (y = rLoop.top; y <= rLoop.bottom; y++) for (x = rLoop.left; x <= rLoop.right; x++) {
               if (!pCanopy->EnumTree (m_OSINFO.dwRenderShard, x, y, dwTree, &mTree, &pLoc, &bScore, &fIfDistantEliminate))
                  continue;

               // BUGFIX - may skip trees
               if (fIfDistantEliminate && (pClone->MaxSize() < fDetail * 4.0))
                  continue;

               // make sure that this is in bounding box
               if ((pLoc.p[0] < pCorner1.p[0]) || (pLoc.p[0] >= pCorner2.p[0]) ||
                  (pLoc.p[1] >= pCorner1.p[1]) || (pLoc.p[1] < pCorner2.p[1]))
                  continue;

               // if convert back to a pixel, make sure it's in the right range...
               fp fPixX, fPixY;
               int iPixX, iPixY;
               fPixX = pLoc.p[0] / m_fScale + (fp)(m_dwWidth-1)/2.0;
               fPixY = -pLoc.p[1] / m_fScale + (fp)(m_dwHeight-1)/2.0;
               iPixX = (int) fPixX;
               iPixY = (int) fPixY;
               if ((iPixX < 0) || (iPixY < 0) || (iPixX >= (int)m_dwHeight) ||
                  (iPixY >= (int)m_dwHeight))
                  continue;   // out of range of the display

               // make sure it's within the score range
               if (bScore >= pabCurForest[iPixX + iPixY * (int)m_dwWidth])
                  continue;

               // get an elevation from this
               HVToPoint (fPixX / (fp)(m_dwWidth-1), fPixY / (fp)(m_dwHeight-1), &pLoc);

               // add that to the matrix
               mTrans.Translation (pLoc.p[0], pLoc.p[1], pLoc.p[2]);
               mTree.MultiplyRight (&mTrans);
               if (!fForestBoxes)
                  mTree.MultiplyRight (&m_MatrixObject); // BUGFIX - Need to include the object's location

               fp fDetailRender;
               fDetailRender = fDetail;
               if (fDetailRender < 0) {
                  // get the detail for the tree
                  CPoint pC[2];
                  CMatrix m;
                  pClone->BoundingBoxGet (&m, &pC[0], &pC[1]);
                  m.MultiplyRight (&mTree);

                  // if the camera intersects with the tree then skip
                  if (pCamera) {
                     CMatrix mInv;
                     CPoint p;
                     m.Invert4 (&mInv);
                     p.Copy (&pCameraGround);
                     p.p[3] = 1;
                     p.MultiplyLeft (&mInv);

                     for (j = 0; j < 3; j++)
                        if ((p.p[j] < pC[0].p[j]) || (p.p[j] > pC[1].p[j]))
                           break;
                     if (j >= 3)
                        continue;   // tree obstructs camera
                  }

                  if (!pr->pRS->QuerySubDetail (&m, &pC[0], &pC[1], &fDetailRender))
                     continue;   // not visible

                  // BUGFIX - Eliminate 3/4 if far away
                  if (fDetailRender * (fIfDistantEliminate ? 4.0 : 1.0) > pClone->MaxSize())
                     continue;   // too small
               }

               // draw it
               if (fCloneRender && !fForestBoxes) {   // BUGFIX - If forestboxes then just draw them
	               MALLOCOPT_OKTOMALLOC;
                  m_lRenderTreesCloneMatrix.Add (&mTree);
	               MALLOCOPT_RESTORE;
               }
               else if (fForestBoxes) {
                  m_lRenderTreesrs.Push();

                  // get the box for the tree
                  CPoint pC[2];
                  CMatrix m;
                  pClone->BoundingBoxGet (&m, &pC[0], &pC[1]);
                  m.MultiplyRight (&mTree);
                  m_lRenderTreesrs.Multiply (&m);

                  // draw box
                  //m_lRenderTreesrs.ShapeBox (pC[0].p[0], pC[0].p[1], pC[0].p[2],
                  //   pC[1].p[0], pC[1].p[1], pC[1].p[2]);

                  // BUGFIX - Draw a diamond so the trees take up less space
                  // draw as diamond
                  PCPoint pap;
                  DWORD dwPoints;
                  fp fHalf;
                  pap = m_lRenderTreesrs.NewPoints (&dwPoints, 6);
                  if (!pap)
                     goto pop;
                  fHalf = (pC[0].p[2] + pC[1].p[2]) / 2.0;
                  pap[0].Zero();
                  pap[0].p[0] = (pC[0].p[0] + pC[1].p[0]) / 2;
                  pap[0].p[1] = pC[0].p[1];
                  pap[0].p[2] = fHalf;
                  pap[2].Copy (&pap[0]);
                  pap[2].p[1] = pC[1].p[1];

                  pap[1].Zero();
                  pap[1].p[0] = pC[0].p[0];
                  pap[1].p[1] = (pC[0].p[1] + pC[1].p[1]) / 2;
                  pap[1].p[2] = fHalf;
                  pap[3].Copy (&pap[1]);
                  pap[3].p[0] = pC[1].p[0];

                  pap[4].Zero();
                  pap[4].p[0] = pap[0].p[0];
                  pap[4].p[1] = pap[1].p[1];
                  pap[4].p[2] = pC[0].p[2];
                  pap[5].Copy (&pap[4]);
                  pap[5].p[2] = pC[1].p[2];

                  // textures
                  DWORD dwText;
                  dwText = m_lRenderTreesrs.NewTexture (0, 0, 0, 0, 0);

                  DWORD dwNorm;
                  PCPoint paNorm;
                  dwNorm = 0;
                  paNorm = m_lRenderTreesrs.NewNormals (TRUE, &dwNorm, 3);
                  if (paNorm) {
                     paNorm[0].Zero();
                     paNorm[0].p[0] = 1;
                     paNorm[1].Zero();
                     paNorm[1].p[1] = 1;
                     paNorm[2].Zero();
                     paNorm[2].p[2] = 1;
                  }

                  DWORD dwVert, dw;
                  PVERTEX pv;
                  for (dw = 0; dw < 3; dw++) {
                     pv = m_lRenderTreesrs.NewVertices (&dwVert, 4);
                     if (!pv)
                        continue;

                     pv[0].dwColor = pv[1].dwColor = pv[2].dwColor = pv[3].dwColor = m_lRenderTreesrs.DefColor ();
                     pv[0].dwTexture = pv[1].dwTexture = pv[2].dwTexture = pv[3].dwTexture = dwText;
                     pv[0].dwNormal = pv[1].dwNormal = pv[2].dwNormal = pv[3].dwNormal =
                        paNorm ? (dwNorm + dw) : 0;

                     switch (dw) {
                     case 0:  // NS
                        pv[0].dwPoint = dwPoints + 0;
                        pv[1].dwPoint = dwPoints + 4;
                        pv[2].dwPoint = dwPoints + 2;
                        pv[3].dwPoint = dwPoints + 5;
                        break;
                     case 1: // ew
                        pv[0].dwPoint = dwPoints + 1;
                        pv[1].dwPoint = dwPoints + 4;
                        pv[2].dwPoint = dwPoints + 3;
                        pv[3].dwPoint = dwPoints + 5;
                        break;
                     case 2:  // level
                        pv[0].dwPoint = dwPoints + 0;
                        pv[1].dwPoint = dwPoints + 1;
                        pv[2].dwPoint = dwPoints + 2;
                        pv[3].dwPoint = dwPoints + 3;
                        break;
                     }

                     // add quad
                     m_lRenderTreesrs.NewQuad (dwVert + 0, dwVert + 1, dwVert + 2, dwVert + 3, FALSE);
                  }

#if 0 // slower to draw
                  // normals
                  DWORD adwNorm[8];
                  DWORD dw, dwCorn;
                  for (dw = 0; dw < 8; dw++) {
                     dwCorn = dw % 4;
                     if (dw < 4)
                        adwNorm[dw] = m_lRenderTreesrs.NewNormal (pap + dwCorn, pap + (dwCorn+1)%4, pap + 5);
                     else
                        adwNorm[dw] = m_lRenderTreesrs.NewNormal (pap + dwCorn, pap + 4, pap + (dwCorn+1)%4);
                  } // dw

                  // triangles
                  DWORD dwVert;
                  PVERTEX pv;
                  for (dw = 0; dw < 8; dw++) {
                     dwCorn = dw % 4;
                     
                     // new vertices
                     pv = m_lRenderTreesrs.NewVertices (&dwVert, 3);
                     if (!pv)
                        continue;

                     pv[0].dwColor = pv[1].dwColor = pv[2].dwColor = m_lRenderTreesrs.DefColor ();
                     pv[0].dwTexture = pv[1].dwTexture = pv[2].dwTexture = dwText;
                     pv[0].dwNormal = pv[1].dwNormal = pv[2].dwNormal = adwNorm[dw];

                     pv[0].dwPoint = dwCorn + dwPoints;
                     pv[1].dwPoint = pv[2].dwPoint = (dwCorn+1)%4 + dwPoints;
                     if (dw < 4)
                        pv[2].dwPoint = 5;
                     else
                        pv[1].dwPoint = 4;

                     // add triangle
                     m_lRenderTreesrs.NewTriangle (dwVert + 0, dwVert + 1, dwVert + 2);
                  } // dw
#endif // 0, slower to draw
pop:
                  m_lRenderTreesrs.Pop();
               }
               else
                  pClone->Render (pr, TREEID, &mTree, fDetailRender);
            }  // x and y

            // if there are any trees to render via clonerender then do so
            if (fCloneRender && m_lRenderTreesCloneMatrix.Num())
               pr->pRS->CloneRender (&pClone->m_gCode, &pClone->m_gSub,
                  m_lRenderTreesCloneMatrix.Num(), (PCMatrix)m_lRenderTreesCloneMatrix.Get(0));

         } // dwTree
      } // dwcanopy
   } // dwForest

   m_lRenderTreesrs.Commit();
}




/*****************************************************************************************
CObjectGround::EditorCreate - From CObjectSocket. SOMETIMES OVERRIDDEN
*/
BOOL CObjectGround::EditorCreate (BOOL fAct)
{
   if (!fAct)
      return TRUE;

   return GroundViewNew (m_pWorld, &m_gGUID);
}


/*****************************************************************************************
CObjectGround::EditorCreate - From CObjectSocket. SOMETIMES OVERRIDDEN
*/
BOOL CObjectGround::EditorDestroy (void)
{
   return GroundViewDestroy (m_pWorld, &m_gGUID);
}

/*****************************************************************************************
CObjectGround::EditorCreate - From CObjectSocket. SOMETIMES OVERRIDDEN
*/
BOOL CObjectGround::EditorShowWindow (BOOL fShow)
{
   return GroundViewShowHide (m_pWorld, &m_gGUID, fShow);
}


/*****************************************************************************************
CObjectGround::Deconstruct - Standard call
*/
BOOL CObjectGround::Deconstruct (BOOL fAct)
{
   return FALSE;
}


// FUTURERELEASE - Many objects send a message to the ground object to find out
// an intersection. Right now they stop at the first ground object they find.
// Should fix this so they go through all ground objects and find the closest one

