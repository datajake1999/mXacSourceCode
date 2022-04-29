/********************************************************************************
VolText.cpp - Code for handling volumetric texture maps.

begun 13/11/03 by Mike Rozak
Copyright 2001-3 Mike Rozak. All rights reserved
*/

#include <windows.h>
#include <math.h>
#include "resource.h"
#include "escarpment.h"
#include "..\M3D.h"
#include "texture.h"


BOOL TEHelperMessageHook (PCEscPage pPage, DWORD dwMessage, PVOID pParam);
BOOL TEHelperCreateBitmap (PTEXTEDITINFO pt, HWND hWnd);



// noise volumetric texture
class DLLEXPORT CTextureVolNoise : public CTextureVolSocket {
public:
   CTextureVolNoise (void);
   ~CTextureVolNoise (void);

   // ALWAYS OVERRIDDEN
   virtual BOOL Init (DWORD dwRenderShard, const GUID *pCode, const GUID *pSub, PCMMLNode2 pNode);
   virtual void Delete (void);
   virtual void FillPixel (DWORD dwThread, DWORD dwFlags, WORD *pawColor, const PTEXTPOINT5 pText, const PTEXTPOINT5 pMax,
      PCMaterial pMat, float *pafGlow, BOOL fHighQuality);
   virtual BOOL MMLFrom (PCMMLNode2 pNode);
   virtual PCMMLNode2 MMLTo (void);
   virtual BOOL Dialog (PCEscWindow pWindow);

   // sometimes overridden
   virtual BOOL BumpAtLoc (PTEXTPOINT5 pText, fp *pfHeight);
   virtual BOOL QueryBump (DWORD dwThread);

   void Clear();
   void SetDef();

   // variables
   fp          m_fNoiseSize;        // size of basic noise
   fp          m_fNoiseDetail;      // decay, around .5
   DWORD       m_dwRingNoiseIter;   // number of iterations into the noise
   COLORREF    m_acColorRange[2];   // colors (only 2 of them)
   fp          m_fBumpAmount;       // amount that affects bump map, 0 if dont affect
   DWORD       m_wTransMin;         // transparency at mimum noise
   DWORD       m_wTransMax;         // transparency at mimum noise
};
typedef CTextureVolNoise *PCTextureVolNoise;


// noise volumetric texture
class DLLEXPORT CTextureVolSandstone : public CTextureVolSocket {
public:
   CTextureVolSandstone (void);
   ~CTextureVolSandstone (void);

   // ALWAYS OVERRIDDEN
   virtual BOOL Init (DWORD dwRenderShard, const GUID *pCode, const GUID *pSub, PCMMLNode2 pNode);
   virtual void Delete (void);
   virtual void FillPixel (DWORD dwThread, DWORD dwFlags, WORD *pawColor, const PTEXTPOINT5 pText, const PTEXTPOINT5 pMax,
      PCMaterial pMat, float *pafGlow, BOOL fHighQuality);
   virtual BOOL MMLFrom (PCMMLNode2 pNode);
   virtual PCMMLNode2 MMLTo (void);
   virtual BOOL Dialog (PCEscWindow pWindow);

   // sometimes overridden
   virtual BOOL BumpAtLoc (PTEXTPOINT5 pText, fp *pfHeight);
   virtual BOOL QueryBump (DWORD dwThread);

   void Clear();
   void SetDef();

   // variables
   fp          m_fTotalThick;       // total thickness of the pattern
   fp          m_fNoiseSize;        // size of basic noise
   fp          m_fNoiseDetail;      // decay, around .5
   DWORD       m_dwRingNoiseIter;   // number of iterations into the noise
   fp          m_fNoiseAffect;      // push up/down in meters
   COLORREF    m_acColorRange[8];   // colors
   fp          m_afColorHeight[8];  // from 0..1, increasing values
   fp          m_fBumpAmount;       // amount that affects bump map, 0 if dont affect
};
typedef CTextureVolSandstone *PCTextureVolSandstone;





// tree volumetric wood
class DLLEXPORT CTextureVolWood : public CTextureVolSocket {
public:
   CTextureVolWood (void);
   ~CTextureVolWood (void);

   // ALWAYS OVERRIDDEN
   virtual BOOL Init (DWORD dwRenderShard, const GUID *pCode, const GUID *pSub, PCMMLNode2 pNode);
   virtual void Delete (void);
   virtual void FillPixel (DWORD dwThread, DWORD dwFlags, WORD *pawColor, const PTEXTPOINT5 pText, const PTEXTPOINT5 pMax,
      PCMaterial pMat, float *pafGlow, BOOL fHighQuality);
   virtual BOOL MMLFrom (PCMMLNode2 pNode);
   virtual PCMMLNode2 MMLTo (void);
   virtual BOOL Dialog (PCEscWindow pWindow);

   void Clear();
   void SetDef();

   // variables
   COLORREF       m_acColors[5];       // colors of ring at 5 points, progressing from lighter to darker
   fp             m_fRingThickness;    // thickness of a tree ring, in meters
   CPoint         m_pRingNoiseScale;   // amount to scale the noise controlling the tree rings, p[0..2] = in dimension, p[3] = 0..1.0
   DWORD          m_dwRingNoiseIter;   // number of iterations into the noise
   fp             m_fRingNoiseDecay;   // amount to decay the noise with each iteration
};
typedef CTextureVolWood *PCTextureVolWood;



// tree volumetric wood
class DLLEXPORT CTextureVolMarble : public CTextureVolSocket {
public:
   CTextureVolMarble (void);
   ~CTextureVolMarble (void);

   // ALWAYS OVERRIDDEN
   virtual BOOL Init (DWORD dwRenderShard, const GUID *pCode, const GUID *pSub, PCMMLNode2 pNode);
   virtual void Delete (void);
   virtual void FillPixel (DWORD dwThread, DWORD dwFlags, WORD *pawColor, const PTEXTPOINT5 pText, const PTEXTPOINT5 pMax,
      PCMaterial pMat, float *pafGlow, BOOL fHighQuality);
   virtual BOOL MMLFrom (PCMMLNode2 pNode);
   virtual PCMMLNode2 MMLTo (void);
   virtual BOOL Dialog (PCEscWindow pWindow);

   void Clear();
   void SetDef();

   // variables
   BOOL        m_fNoiseMax;         // TRUE if noise maxes out
   fp          m_fNoiseSize;        // size of basic noise
   fp          m_fNoiseDetail;      // decay, around .5
   fp          m_fNoiseContrib;     // noise contribution
   COLORREF    m_acColor[8];        // colors
   DWORD          m_dwRingNoiseIter;   // number of iterations into the noise
};
typedef CTextureVolMarble *PCTextureVolMarble;



// globals
static CNoise3D      gaNoise3D[MAXRENDERSHARDS];     // 3d noise used for various bits
static BOOL          gaNoise3DInit[MAXRENDERSHARDS] = {FALSE, FALSE, FALSE, FALSE};   // set to TRUE if noise 3D is intiialized


#define FRACT3DDIVS           32    // number of divisions in fractal 3d noise

/***********************************************************************************
Fractal3DNoise - 3D noise function.

inputs
   fp       x,y,z - xyz running from 0..1
   DWORD    dwIteration - Number of iterations to repeat to generate fractal noise.
            At least value of 1
   fp       fDecay - Each time double the frequency, multiply scale by this. Should
            be around .5.
   BOOL     fAbsolute - if TRUE then add absolute values to noise, else pos/neg
return
   fp - Value between -1 and 1, approx
*/
static fp Fractal3DNoise (DWORD dwRenderShard, fp x, fp y, fp z, DWORD dwIteration, fp fDecay, BOOL fAbsolute /*= FALSE*/)
{
   // make sure have noise
   if (!gaNoise3DInit[dwRenderShard]) {
      gaNoise3D[dwRenderShard].Init (FRACT3DDIVS, FRACT3DDIVS,FRACT3DDIVS);
      gaNoise3DInit[dwRenderShard] = TRUE;
   }

   fp fSum = 0;
   fp fScale = 1;
   DWORD i;
#define FRACTZOOMIN     2.1243
   for (i = 0; i < dwIteration; i++) {
      fp f = gaNoise3D[dwRenderShard].Value (x,y,z) * fScale;
      if (fAbsolute)
         f = fabs(f);
      fSum += f;

      fScale *= fDecay;
      x *= FRACTZOOMIN;
      y *= FRACTZOOMIN;
      z *= FRACTZOOMIN;
   } // i
   

   return fSum;
}


/***********************************************************************************
TextureCreateVol - Creates a new texture object based on the major GUIDs. This only
creates volumetric textures though. It DOES NOT intiialize the texture because
there isn't anything to initialize with.

inputs
   GUID     *pCode - Code guid
returns
   PCTextureMapSocket - New object. NULL if error
*/
PCTextureVolSocket TextureCreateVol (const GUID *pCode)
{
   if (IsEqualGUID(*pCode, GTEXTURECODE_VolNoise))
      return new CTextureVolNoise;
   else if (IsEqualGUID(*pCode, GTEXTURECODE_VolWood))
      return new CTextureVolWood;
   else if (IsEqualGUID(*pCode, GTEXTURECODE_VolMarble))
      return new CTextureVolMarble;
   else if (IsEqualGUID(*pCode, GTEXTURECODE_VolSandstone))
      return new CTextureVolSandstone;
   else
      return NULL;
}


/***********************************************************************************
TextureCreateVol - Creates a new texture object based on the GUIDs. This only
creates volumetric textures though

inputs
   GUID     *pCode - Code guid
   GUID     *pSub - sub guid
returns
   PCTextureMapSocket - New object. NULL if error
*/
PCTextureMapSocket TextureCreateVol (DWORD dwRenderShard, const GUID *pCode, const GUID *pSub)
{
   // find it
   PCMMLNode2 pNode;
   WCHAR szMajor[128], szMinor[128], szName[128];
   if (!LibraryTextures(dwRenderShard, FALSE)->ItemNameFromGUID (pCode, pSub, szMajor, szMinor, szName)) {
      if (!LibraryTextures(dwRenderShard, TRUE)->ItemNameFromGUID (pCode, pSub, szMajor, szMinor, szName))
         return NULL;  // cant find
   }
   pNode = LibraryTextures(dwRenderShard, FALSE)->ItemGet (szMajor, szMinor, szName);
   if (!pNode)
      pNode = LibraryTextures(dwRenderShard, TRUE)->ItemGet (szMajor, szMinor, szName);
   if (!pNode)
      return NULL;

   // try to create
   PCTextureVolSocket pNew;
   pNew = TextureCreateVol (pCode);
   if (!pNew)
      return NULL;

   // else init
   if (!pNew->Init (dwRenderShard, pCode, pSub, pNode)) {
      pNew->Delete();
      return NULL;
   }

   // else found
   return pNew;
}


/********************************************************************************
CTextureVolSocket::Constructor and destructor
*/
CTextureVolSocket::CTextureVolSocket (void)
{
   // clear to default state
   memset (&m_gCode, 0 ,sizeof(m_gCode));
   memset (&m_gSub, 0 ,sizeof(m_gSub));
   m_cColorAverage = -1;   // so know not filled in
   m_cColorAverageGlow = -1;
   m_dwRenderShard = 0;
   m_fMightBeTransparent=  FALSE;
   m_fDefH = m_fDefV = 1;
   m_TextureMods.cTint = RGB(0xff,0xff,0xff);
   m_TextureMods.wBrightness = 0x1000;
   m_TextureMods.wContrast = 0x1000;
   m_TextureMods.wHue = 0x0000;
   m_TextureMods.wSaturation = 0x1000;
   m_Material.InitFromID (MATERIAL_FLAT);
}

CTextureVolSocket::~CTextureVolSocket (void)
{
   // nothing for now
}


/*********************************************************************************
CTextureVolSocket::QueryTextureBlurring - SOme textures, like the skydome, ignore
the pMax parameter when FillPixel() is called. If this is the
case then QueryNoTextureBlurring() will return FALSE, meaning that
the calculations don't need to be made. However, the default
behavior uses texture blurring and returns TRUE.
*/
BOOL CTextureVolSocket::QueryTextureBlurring(DWORD dwThread)
{
   return TRUE;
}




/*********************************************************************************
CTextureVolSocket::QueryBump - Returns TRUE if the texture has a bump map. FALSE
if none.
*/
BOOL CTextureVolSocket::QueryBump(DWORD dwThread)
{
   return TRUE;   // default to TRUE
}


/********************************************************************************
CTextureVolSocket::TextureModsSet - Sets the texture mods for the object
*/
void CTextureVolSocket::TextureModsSet (PTEXTUREMODS pt)
{
   m_TextureMods = *pt;
   m_cColorAverage = -1;   // so will recalc default color
   m_cColorAverageGlow = -1;
}


/********************************************************************************
CTextureVolSocket::TextureModsGet - Get default texture mods. Stanrdad api
*/
void CTextureVolSocket::TextureModsGet (PTEXTUREMODS pt)
{
   *pt = m_TextureMods;
}

/********************************************************************************
CTextureVolSocket::GUIDsGet - Standard API
*/
void CTextureVolSocket::GUIDsGet (GUID *pCode, GUID *pSub)
{
   if (pCode)
      *pCode = m_gCode;
   if (pSub)
      *pSub = m_gSub;
}

/********************************************************************************
CTextureVolSocket::GUIDsSet - Standard API
*/
void CTextureVolSocket::GUIDsSet (const GUID *pCode, const GUID *pSub)
{
   m_gCode = *pCode;
   m_gSub = *pSub;
}


/********************************************************************************
CTextureVolSocket::DefScaleGet - Stanrdar API
*/
void CTextureVolSocket::DefScaleGet (fp *pfDefScaleH, fp *pfDefScaleV)
{
   *pfDefScaleH = m_fDefH;
   *pfDefScaleV = m_fDefV;
}

/********************************************************************************
CTextureVolSocket::MaterialGet - Standard API
*/
void CTextureVolSocket::MaterialGet (DWORD dwThread, PCMaterial pMat)
{
   memcpy (pMat, &m_Material, sizeof(m_Material));
   
   // since sometimes have predefined material when really custom, need
   // to check
   if (m_Material.m_dwID != MATERIAL_CUSTOM) {
      CMaterial cTest;
      cTest.InitFromID (m_Material.m_dwID);
      if (memcmp (&cTest, pMat, sizeof(cTest)))
         pMat->m_dwID = MATERIAL_CUSTOM;
   }
}

/********************************************************************************
CTextureVolSocket::AverageColorGet - Standard API
*/
COLORREF CTextureVolSocket::AverageColorGet (DWORD dwThread, BOOL fGlow)
{
   COLORREF *pc = (fGlow ? &m_cColorAverageGlow : &m_cColorAverage);
   if (*pc != -1)
      return *pc; // already calculated

   // else need to calculate... supersample a few points
   DWORD i, j;
   TEXTPOINT5 tp, tpMax;
   float afColorSum[3], afGlow[3];
   WORD awColor[3];
   memset (&tpMax, 0, sizeof(tpMax));
   memset (afColorSum, 0 ,sizeof(afColorSum));
   CMaterial Mat;
   m_fMightBeTransparent = FALSE;
   for (i = 0; i < 250; i++) {
      tp.hv[0] = randf (-100, 100);
      tp.hv[1] = randf (-100, 100);
      tp.xyz[0] = randf (-100, 100);
      tp.xyz[1] = randf (-100, 100);
      tp.xyz[2] = randf (-100, 100);

      FillPixel (dwThread, TMFP_ALL, awColor, &tp, &tpMax, &Mat, afGlow, FALSE); // no need for high quality

      for (j = 0; j < 3; j++)
         afColorSum[j] += (fGlow ? afGlow[j] : (float)awColor[j]);

      // if had transparent then set flag
      if (Mat.m_wTransparency)
         m_fMightBeTransparent = TRUE;
   } // i

   // average out
   for (j = 0; j < 3; j++) {
      afColorSum[j] /= (float)i;
      afColorSum[j] = min(afColorSum[j], (float)0xffff);
      afColorSum[j] = max(afColorSum[j], 0);

      awColor[j] = (WORD)afColorSum[j];
   }

   *pc = UnGamma (awColor);

   return *pc;
}

/********************************************************************************
CTextureVolSocket::FillLine - Standard API

inputs
   DWORD          dwThread - 0..MAXRAYTHREAD-1, for the current thread
   PGCOLOR        pac - Fill in dwNum samples. The buffer must be large enough
                  for dwNum * sizeof(PGCOLOR)
   DWORD          dwNum - Number of samples.
   PTEXTPOINT5  pLeft - Left starting point of the texture
   PTEXTPOINT5  pRight - Right ending point of texture
   float          *pafGlow - If the texture has a glow, these values will be filled
                  in with dwNum * 3 floats. If this is NULL no filling will happen
   BOOL           *pfGlow - Filled in with TRUE if there's a glow, FALSE if not
   WORD           *pawTrans - If the texture has a transparency map, this is filled
                  in with dwNum WORDs. If this is NULL no filling will happen
   BOOL           *pfTrans - Filled in with TRUE if there's transparency, FALSE if not
   fp             *pafAlpha - dwNum entries that indicate the alpha (from 0..1)
                  between pLeft and pRight... p[i] = pLeft + pafAlpha[i] * (pRight - pLeft)
                  Used so get perspective correct. If NULL assumes evenly spaced
returns
   none
*/
void CTextureVolSocket::FillLine (DWORD dwThread, PGCOLOR pac, DWORD dwNum, PTEXTPOINT5 pLeft, PTEXTPOINT5 pRight,
   float *pafGlow, BOOL *pfGlow, WORD *pawTrans, BOOL *pfTrans, fp *pafAlpha)
{

   // figure out parameters for use of alpha
   if (!m_amemPosn[dwThread].Required (dwNum * sizeof(TEXTPOINT5)))
      return;

   TEXTPOINT5 tDelta;
   PTEXTPOINT5 pt;
   pt = (PTEXTPOINT5) m_amemPosn[dwThread].p;
   DWORD i, j;
   for (i = 0; i < 2; i++)
      tDelta.hv[i] = pRight->hv[i] - pLeft->hv[i];
   for (i = 0; i < 3; i++)
      tDelta.xyz[i] = pRight->xyz[i] - pLeft->xyz[i];

   if (pafAlpha) {
      for (i = 0; i < dwNum; i++) {
         for (j = 0; j < 2; j++)
            pt[i].hv[j] = pLeft->hv[j] + pafAlpha[i] * tDelta.hv[j];
         for (j = 0; j < 3; j++)
            pt[i].xyz[j] = pLeft->xyz[j] + pafAlpha[i] * tDelta.xyz[j];
      }
   }
   else {   // linear
      for (i = 0; i < dwNum; i++) {
         for (j = 0; j < 2; j++)
            pt[i].hv[j] = pLeft->hv[j] + ( i / (fp) dwNum) * tDelta.hv[j];
         for (j = 0; j < 3; j++)
            pt[i].xyz[j] = pLeft->xyz[j] + ( i / (fp) dwNum) * tDelta.xyz[j];
      }
   }

   // keep track of if there's a glow or transparency
   BOOL fGlow = FALSE, fTrans = FALSE;

   // loop over all pixels
   CMaterial Mat, MatOrig;
   MaterialGet (dwThread, &MatOrig);
   TEXTPOINT5 tpMax;
   float afGlow[3];
   for (i = 0; i < dwNum; i++, pac++, pt++) {
      // figure out max
      if (i+1 < dwNum)
         tpMax = pt[1];
      else if (i)
         tpMax = pt[-1];
      else
         tpMax = pt[0]; // since no other possbility
      for (j = 0; j < 2; j++)
         tpMax.hv[j] = fabs(tpMax.hv[j] - pt->hv[j]);
      for (j = 0; j < 3; j++)
         tpMax.xyz[j] = fabs(tpMax.xyz[j] - pt->xyz[j]);

      pac->wIntensity = 0;
      Mat = MatOrig;
      FillPixel (dwThread, TMFP_ALL, &pac->wRed, pt, &tpMax, &Mat, afGlow, FALSE); // no need for high quality

      // fill in glow
      if (pafGlow)
         memcpy (pafGlow + (i*3), afGlow, sizeof(afGlow));
      if (!fGlow && (afGlow[0] || afGlow[1] || afGlow[2]))
         fGlow = TRUE;

      // fill in transparency
      if (pawTrans)
         pawTrans[i] = Mat.m_wTransparency;
      if (Mat.m_wTransparency)
         fTrans = TRUE;
   } // i

   // tell app if transparency/glow filled in
   if (pfGlow)
      *pfGlow = fGlow;
   if (pfTrans)
      *pfTrans = fTrans;
}

/*********************************************************************************
CTextureVolSocket::PixelBump - Given a pixel's texturepoint, and the texture delta
over the X and Y of the pixel, returns a bump amount.

inputs
   DWORD          dwThread
   PTEXTPOINT5     pText - Texture point center
   PTEXTPOINT5     pRight - Texture point of right side of pixel - texture point of left side
   PTEXTPOINT5     pDown - Texture point of bottom side of pixel - texture point top
   PTEXTUREPOINT     pSlope - Fills in height change (in meters) over the distance.
                              .h = right height - left height (positive values stick out of surface)
                               v = BOTTOM height - TOP height
                        Can be NULL;
   fp             *pfHeight - Absolute height in meters. Cann be NULL
   BOOL           fHighQuality - If TRUE then use high quality blurring, else bi-linear blurring
returns
   BOOL - TRUE if there's a bump map, FALSE if no bump maps for this one
*/
BOOL CTextureVolSocket::PixelBump (DWORD dwThread, PTEXTPOINT5 pText, PTEXTPOINT5 pRight,
                             PTEXTPOINT5 pDown, PTEXTUREPOINT pSlope, fp *pfHeight, BOOL fHighQuality)
{
   // NOTE: Volumetric textures ignore the high quality flag since they automatically
   // generate smooth surfaces

   TEXTPOINT5 t;
   DWORD i;
   fp fHeight;

   if (pSlope)
      pSlope->h = pSlope->v = 0;
   if (pfHeight)
      *pfHeight = 0;

   if (pSlope) {
      // look right
      t = *pText;
      for (i = 0; i < 2; i++)
         t.hv[i] += pRight->hv[i]/2;
      for (i = 0; i < 3; i++)
         t.xyz[i] += pRight->xyz[i]/2;
      if (!BumpAtLoc (&t, &fHeight))
         return FALSE;  // no bump map supported
      pSlope->h += fHeight;

      // left
      t = *pText;
      for (i = 0; i < 2; i++)
         t.hv[i] -= pRight->hv[i]/2;
      for (i = 0; i < 3; i++)
         t.xyz[i] -= pRight->xyz[i]/2;
      if (BumpAtLoc (&t, &fHeight))
         pSlope->h -= fHeight;

      // top
      t = *pText;
      for (i = 0; i < 2; i++)
         t.hv[i] -= pDown->hv[i]/2;
      for (i = 0; i < 3; i++)
         t.xyz[i] -= pDown->xyz[i]/2;
      if (BumpAtLoc (&t, &fHeight))
         pSlope->v -= fHeight;

      // bottom
      t = *pText;
      for (i = 0; i < 2; i++)
         t.hv[i] += pDown->hv[i]/2;
      for (i = 0; i < 3; i++)
         t.xyz[i] += pDown->xyz[i]/2;
      if (BumpAtLoc (&t, &fHeight))
         pSlope->v += fHeight;
   }
   if (pfHeight) {
      t = *pText;
      if (BumpAtLoc (&t, &fHeight))
         *pfHeight = fHeight;
   }

   return TRUE;
}




/*********************************************************************************
CTextureVolSocket::Standard API
*/
BOOL CTextureVolSocket::FillImageWithTexture (PCImage pImage, BOOL fEncourageSphere, BOOL fStretchToFit)
{
   fp fx, fy;
   DefScaleGet (&fx, &fy);
   fp afTextureMatrix[2][2];
   CMatrix mTextureMatrix;
   mTextureMatrix.Identity();
   memset (afTextureMatrix, 0, sizeof(afTextureMatrix));
   afTextureMatrix[0][0] = 1.0 / fx;
   afTextureMatrix[1][1] = 1.0 / fy;

   FillImage (pImage, TRUE, fStretchToFit, afTextureMatrix, &mTextureMatrix, NULL);

   return TRUE;
}

/*********************************************************************************
CTextureVolSocket::Standard API
*/
BOOL CTextureVolSocket::Render (PCImage pImage, PCImage pImage2, PCMaterial pMaterial, PTEXTINFO pTextInfo)
{
   return FALSE;
}



/********************************************************************************
CTextureVolSocket::MightbeTransparent - Starndard API.

SOMETIMES OVERRIDDEN
*/
DWORD CTextureVolSocket::MightBeTransparent (DWORD dwThread)
{
   // this flag is automatically calcualted when calculate average color
   AverageColorGet (dwThread, FALSE);
   return m_fMightBeTransparent;
}

/********************************************************************************
CTextureVolSocket::DimensionalityQuery - Starndard API.

SOMETIMES OVERRIDDEN
*/
DWORD CTextureVolSocket::DimensionalityQuery (DWORD dwThread)
{
   return 0x02;   // flag to indicate 3d only
}

/********************************************************************************
CTextureVolSocket::ForceCache - Forces the object to cache itself so that it can
then be called multithreaded later on. Standard API

SOMETIMES OVERRIDDEN
*/
void CTextureVolSocket::ForceCache (DWORD dwForceCache)
{
   // default - do nothing
}







/********************************************************************************
CTextureVolSocket::BumpAtLoc - This returns the height of the surface at the given
texture-point location. The height is the amount above (or if negative, blow) the
default surface height, in meters.

inputs
   fp          *pfHeight - Filled with the elevation at the location above the surface
retursn
   BOOL - TRUE if the surface supports bump maps. FALSE if not
*/
BOOL CTextureVolSocket::BumpAtLoc (PTEXTPOINT5 pText, fp *pfHeight)
{
   return FALSE;  // default response is false
}


/********************************************************************************
CTextureVolSocket::Init - Initializes the object, pulling out relevent information
from pNode. This must fill in a number of member variables, as listed under
"filled in by init". Also MUST clear m_cColorAverage and m_cColorAverageGlow to -1

USUALLY OVERRIDDEN

inputs
   GUID           *pCode - Major code to use
   GUID           *pSub - Minor code to use
   PCMMLNode2      pNode - Node to extract info from. If NULL then creates a default
                  setup for the bump map
returns
   BOOL - TRUE if success
*/
BOOL CTextureVolSocket::Init (DWORD dwRenderShard, const GUID *pCode, const GUID *pSub, PCMMLNode2 pNode)
{
   // must be done for every init call
   m_gCode = *pCode;
   m_gSub = *pSub;
   m_cColorAverage = -1;
   m_cColorAverageGlow = -1;
   m_dwRenderShard = dwRenderShard;

   // note: For real object do something with pNode
   return TRUE;
}





/********************************************************************************
CTextureVolSocket::Constructor and destructor
*/
CTextureVolNoise::CTextureVolNoise (void)
{
   Clear();
}

CTextureVolNoise::~CTextureVolNoise (void)
{
   // do nothing for now
}



/********************************************************************************
CTextureVolNoise::Clear - Clears the attributes to a default value
*/
void CTextureVolNoise::Clear(void)
{
   // smaller scale
   m_Material.InitFromID (MATERIAL_PAINTSEMIGLOSS);

   m_fNoiseSize = .05;
   m_fNoiseDetail = .5;
   m_dwRingNoiseIter = 2;
   m_acColorRange[0] = RGB(0xff,0xff,0xff);
   m_acColorRange[1] = RGB(0,0,0);
   m_fBumpAmount = 0;
   m_wTransMin = m_wTransMax = 0;

   SetDef();
}



/********************************************************************************
CTextureVolMarble::SetDef - Sets m_fDefH and m_fDefV based upon other settings.
*/
void CTextureVolNoise::SetDef()
{
   m_fDefH = m_fDefV = m_fNoiseSize * 10;
}




/********************************************************************************
CTextureVolNoise::Init - Initializes the object, pulling out relevent information
from pNode. This must fill in a number of member variables, as listed under
"filled in by init". Also MUST clear m_cColorAverage and m_cColorAverageGlow to -1

USUALLY OVERRIDDEN

inputs
   GUID           *pCode - Major code to use
   GUID           *pSub - Minor code to use
   PCMMLNode2      pNode - Node to extract info from. If NULL then creates a default
                  setup for the bump map
returns
   BOOL - TRUE if success
*/
BOOL CTextureVolNoise::Init (DWORD dwRenderShard, const GUID *pCode, const GUID *pSub, PCMMLNode2 pNode)
{
   // must be done for every init call
   m_gCode = *pCode;
   m_gSub = *pSub;
   m_cColorAverage = -1;
   m_cColorAverageGlow = -1;
   m_dwRenderShard = dwRenderShard;

   // just tweak fractal 3d noise so generated random numbers
   Fractal3DNoise (m_dwRenderShard, 0,0,0,0,0, FALSE);

   if (pNode && !MMLFrom (pNode))
      return FALSE;

   return TRUE;
}

/********************************************************************************
CTextureVolNoise::FillPixel - Given a point on the texture, fills a pixel describing
the texture. Standard API

ALWAYS OVERRIDDEN
*/
void CTextureVolNoise::FillPixel (DWORD dwThread, DWORD dwFlags, WORD *pawColor, PTEXTPOINT5 pText, PTEXTPOINT5 pMax,
   PCMaterial pMat, float *pafGlow, BOOL fHighQuality)
{
   // NOTE: Volumetric textures ignore fHighQuality since they automatically produce
   // smoth textures close up

   if (pafGlow)
      pafGlow[0] = pafGlow[1] = pafGlow[2] = 0;

   fp fVal = Fractal3DNoise(m_dwRenderShard, 
      pText->xyz[0] / (m_fNoiseSize * FRACT3DDIVS),
      pText->xyz[1] / (m_fNoiseSize * FRACT3DDIVS),
      pText->xyz[2] / (m_fNoiseSize * FRACT3DDIVS),
      m_dwRingNoiseIter,
      m_fNoiseDetail,
      FALSE);
   fVal = (fVal + 1) / 2.0;
   fVal = max(fVal, 0);
   fVal = min(fVal, 1);

   fp f;
   if (pawColor) {
      WORD aw[2][3];
      Gamma (m_acColorRange[0], aw[0]);
      Gamma (m_acColorRange[1], aw[1]);

      DWORD i;
      for (i = 0; i < 3; i++) {
         f = (1.0 - fVal) * (fp)aw[0][i] + fVal * (fp)aw[1][i];
         f = max(0,f);
         f = min((fp)0xffff,f);
         pawColor[i] = (WORD)f;
      } // i

      // finally, apply coloration
      ApplyTextureMods (&m_TextureMods, pawColor);
   }

   // affect transparency?
   if ((dwFlags & TMFP_TRANSPARENCY) && (m_wTransMin || m_wTransMax)) {
      f = (1.0 - fVal) * (fp)m_wTransMin + fVal * (fp)m_wTransMax;
      f = max(0,f);
      f = min((fp)0xffff,f);
      pMat->m_wTransparency = (WORD)f;
   }

}


/********************************************************************************
CTextureVolNoise::Delete - Standard API

ALWAYS OVERRIDDEN
*/
void CTextureVolNoise::Delete (void)
{
   delete this;
}



/*********************************************************************************
CTextureVolNoise::QueryBump - Sees if bump map used
*/
BOOL CTextureVolNoise::QueryBump (DWORD dwThread)
{
   return m_fBumpAmount ? TRUE : FALSE;
}



/********************************************************************************
CTextureVolNoise::BumpAtLoc - This returns the height of the surface at the given
texture-point location. The height is the amount above (or if negative, blow) the
default surface height, in meters.

inputs
   fp          *pfHeight - Filled with the elevation at the location above the surface
retursn
   BOOL - TRUE if the surface supports bump maps. FALSE if not
*/
BOOL CTextureVolNoise::BumpAtLoc (PTEXTPOINT5 pText, fp *pfHeight)
{
   if (m_fBumpAmount == 0)
      return FALSE;

   fp fVal = Fractal3DNoise(m_dwRenderShard, 
      pText->xyz[0] / (m_fNoiseSize * FRACT3DDIVS),
      pText->xyz[1] / (m_fNoiseSize * FRACT3DDIVS),
      pText->xyz[2] / (m_fNoiseSize * FRACT3DDIVS),
      m_dwRingNoiseIter,
      m_fNoiseDetail,
      FALSE);

   *pfHeight = fVal * m_fBumpAmount;

   return TRUE;
}


static PWSTR gpszDefH = L"DefH";
static PWSTR gpszDefV = L"DefV";
static PWSTR gpszTextVolNoise = L"TextVolNoise";
static PWSTR gpszNoiseSize = L"NoiseSize";
static PWSTR gpszNoiseDetail = L"NoiseDetail";
static PWSTR gpszNoiseContrib = L"NoiseContrib";
static PWSTR gpszNoiseMax = L"NoiseMax";
static PWSTR gpszRingNoiseIter = L"RingNoiseIter";
static PWSTR gpszBumpAmount = L"BumpAmount";
static PWSTR gpszTransMin = L"TransMin";
static PWSTR gpszTransMax = L"TransMax";

/********************************************************************************
CTextureVolNoise::MMLTo - Standard API
*/
PCMMLNode2 CTextureVolNoise::MMLTo (void)
{
   PCMMLNode2 pNode = new CMMLNode2;
   if (!pNode)
      return NULL;
   pNode->NameSet (gpszTextVolNoise);

   MMLValueSet (pNode, gpszDefH, m_fDefH);
   MMLValueSet (pNode, gpszDefV, m_fDefV);
   m_Material.MMLTo (pNode);

   MMLValueSet (pNode, gpszNoiseSize, m_fNoiseSize);
   MMLValueSet (pNode, gpszNoiseDetail, m_fNoiseDetail);
   MMLValueSet (pNode, gpszRingNoiseIter, (int)m_dwRingNoiseIter);
   MMLValueSet (pNode, gpszBumpAmount, m_fBumpAmount);
   MMLValueSet (pNode, gpszTransMin, (int)m_wTransMin);
   MMLValueSet (pNode, gpszTransMax, (int)m_wTransMax);

   DWORD i;
   WCHAR szTemp[64];
   for (i = 0; i < 2; i++) {
      swprintf (szTemp, L"color%d", (int)i);
      MMLValueSet (pNode, szTemp, (int) m_acColorRange[i]);
   }

   return pNode;
}


/********************************************************************************
CTextureVolNoise::MMLFrom - Standard API
*/
BOOL CTextureVolNoise::MMLFrom (PCMMLNode2 pNode)
{
   Clear();


   if (!pNode)
      return TRUE;

   m_fDefH = MMLValueGetDouble (pNode, gpszDefH, m_fDefH);
   m_fDefV = MMLValueGetDouble (pNode, gpszDefV, m_fDefV);
   m_Material.MMLFrom (pNode);

   m_fNoiseSize = MMLValueGetDouble (pNode, gpszNoiseSize, .1);
   m_fNoiseDetail = MMLValueGetDouble (pNode, gpszNoiseDetail, .1);
   m_dwRingNoiseIter = (DWORD) MMLValueGetInt (pNode, gpszRingNoiseIter, (int)m_dwRingNoiseIter);
   m_fBumpAmount = MMLValueGetDouble (pNode, gpszBumpAmount, m_fBumpAmount);
   m_wTransMin = (WORD) MMLValueGetInt (pNode, gpszTransMin, (int)m_wTransMin);
   m_wTransMax = (WORD) MMLValueGetInt (pNode, gpszTransMax, (int)m_wTransMax);

   DWORD i;
   WCHAR szTemp[64];
   for (i = 0; i < 2; i++) {
      swprintf (szTemp, L"color%d", (int)i);
      m_acColorRange[i] = (COLORREF) MMLValueGetInt (pNode, szTemp, m_acColorRange[i]);
   }

   SetDef();

   return TRUE;
}




BOOL VolNoisePage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PTEXTEDITINFO pt = (PTEXTEDITINFO) pPage->m_pUserData;
   PCTextureVolNoise pv = (PCTextureVolNoise) pt->pThis;

   if (TEHelperMessageHook (pPage, dwMessage, pParam))
      return TRUE;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         // set the material
         PCEscControl pControl;

         MeasureToString (pPage, L"noisesize", pv->m_fNoiseSize, TRUE);
         MeasureToString (pPage, L"bumpamount", pv->m_fBumpAmount, TRUE);

         pControl = pPage->ControlFind (L"noisedetail");
         if (pControl)
            pControl->AttribSetInt (Pos(), (int)(pv->m_fNoiseDetail * 100));

         pControl = pPage->ControlFind (L"transmin");
         if (pControl)
            pControl->AttribSetInt (Pos(), (int)pv->m_wTransMin);
         pControl = pPage->ControlFind (L"transmax");
         if (pControl)
            pControl->AttribSetInt (Pos(), (int)pv->m_wTransMax);

         DWORD i;
         WCHAR szTemp[64];
         for (i = 0; i < 2; i++) {
            swprintf (szTemp, L"cstatus%d", (int) i);
            FillStatusColor (pPage, szTemp, pv->m_acColorRange[i]);
         }

         // set the material
         ComboBoxSet (pPage, L"material", pv->m_Material.m_dwID);
         pControl = pPage->ControlFind (L"editmaterial");
         if (pControl)
            pControl->Enable (pv->m_Material.m_dwID ? FALSE : TRUE);

         // set the info
         DoubleToControl (pPage, L"ringnoiseiter", pv->m_dwRingNoiseIter);
      }
      break;

   case ESCM_USER+186:  // get all the control values
      {
         PCEscControl pControl;

         MeasureParseString (pPage, L"noisesize", &pv->m_fNoiseSize);
         pv->m_fNoiseSize = max(.001, pv->m_fNoiseSize);

         MeasureParseString (pPage, L"bumpamount", &pv->m_fBumpAmount);

         pControl = pPage->ControlFind (L"noisedetail");
         if (pControl)
            pv->m_fNoiseDetail = (fp)pControl->AttribGetInt (Pos()) / 100.0;

         pControl = pPage->ControlFind (L"transmin");
         if (pControl)
            pv->m_wTransMin = (WORD)pControl->AttribGetInt (Pos());
         pControl = pPage->ControlFind (L"transmax");
         if (pControl)
            pv->m_wTransMax = (WORD)pControl->AttribGetInt (Pos());

         pv->m_dwRingNoiseIter = (DWORD) DoubleFromControl (pPage, L"ringnoiseiter");
         pv->m_dwRingNoiseIter = max(pv->m_dwRingNoiseIter, 1);
         pv->m_dwRingNoiseIter = min(pv->m_dwRingNoiseIter, 10);

         pv->SetDef();
      }
      break;

   case ESCN_COMBOBOXSELCHANGE:
      {
         PESCNCOMBOBOXSELCHANGE p = (PESCNCOMBOBOXSELCHANGE) pParam;
         if (!p->pControl->m_pszName)
            break;

         // only care about transparency
         if (!_wcsicmp(p->pControl->m_pszName, L"material")) {
            DWORD dwVal;
            dwVal = p->pszName ? _wtoi(p->pszName) : 0;
            if (dwVal == pv->m_Material.m_dwID)
               break; // unchanged
            if (dwVal)
               pv->m_Material.InitFromID (dwVal);
            else
               pv->m_Material.m_dwID = MATERIAL_CUSTOM;

            // eanble/disable button to edit
            PCEscControl pControl;
            pControl = pPage->ControlFind (L"editmaterial");
            if (pControl)
               pControl->Enable (pv->m_Material.m_dwID ? FALSE : TRUE);

            return TRUE;
         }
      }
      break;


   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;

         if (!p->pControl || !p->pControl->m_pszName)
            break;

         PWSTR pszColor = L"cbutton";
         DWORD dwSizeColor = (DWORD)wcslen(pszColor);

         if (!_wcsicmp(p->pControl->m_pszName, L"editmaterial")) {
            if (!pv->m_Material.Dialog (pPage->m_pWindow->m_hWnd))
               pPage->Exit (L"[close]");
            return TRUE;
         }
         else if (!wcsncmp(p->pControl->m_pszName, pszColor, dwSizeColor)) {
            DWORD dwNum = (DWORD)_wtoi(p->pControl->m_pszName + dwSizeColor);
            dwNum = min(1,dwNum);
            WCHAR szTemp[64];
            swprintf (szTemp, L"cstatus%d", (int) dwNum);
            pv->m_acColorRange[dwNum] = AskColor (pPage->m_pWindow->m_hWnd, pv->m_acColorRange[dwNum], pPage, szTemp);
            return TRUE;
         }
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Volumetric noise";
            return TRUE;
         }
      }
      break;
   };

   return DefPage (pPage, dwMessage, pParam);
}



/********************************************************************************
CTextureVolNoise::MMLFrom - Standard API
*/
BOOL CTextureVolNoise::Dialog (PCEscWindow pWindow)
{
   TEXTEDITINFO ti;
   PWSTR pszRet;
   memset (&ti, 0, sizeof(ti));
   ti.hBit = NULL;
   ti.pCreator = this;
   ti.pThis = this;

   pszRet = pWindow->PageDialog (ghInstance, IDR_MMLVOLNOISE, VolNoisePage, &ti);
   if (ti.hBit) {
      DeleteObject (ti.hBit);
      ti.hBit = NULL;
   }

   return pszRet && !_wcsicmp(pszRet, Back());
}








/********************************************************************************
CTextureVolSocket::Constructor and destructor
*/
CTextureVolSandstone::CTextureVolSandstone (void)
{
   Clear();
}

CTextureVolSandstone::~CTextureVolSandstone (void)
{
   // do nothing for now
}



/********************************************************************************
CTextureVolSandstone::Clear - Clears the attributes to a default value
*/
void CTextureVolSandstone::Clear(void)
{
   // smaller scale
   m_Material.InitFromID (MATERIAL_FLAT);

   m_fNoiseSize = 10;
   m_fNoiseAffect = 1;
   m_fTotalThick = 50;
   m_fNoiseDetail = .5;
   m_dwRingNoiseIter = 2;
   m_acColorRange[0] = m_acColorRange[2] = m_acColorRange[4] = m_acColorRange[6] = RGB(0xd0, 0xb0, 0x40);
   m_acColorRange[1] = m_acColorRange[3] = m_acColorRange[5] = m_acColorRange[7] = RGB(0x50,0x30,0);
   m_fBumpAmount = 0;

   DWORD i;
   for (i = 0; i < 8; i++)
      m_afColorHeight[i] = (fp)i / 8.0;

   SetDef();
}



/********************************************************************************
CTextureVolMarble::SetDef - Sets m_fDefH and m_fDefV based upon other settings.
*/
void CTextureVolSandstone::SetDef()
{
   m_fDefH = m_fDefV = m_fTotalThick;
}




/********************************************************************************
CTextureVolSandstone::Init - Initializes the object, pulling out relevent information
from pNode. This must fill in a number of member variables, as listed under
"filled in by init". Also MUST clear m_cColorAverage and m_cColorAverageGlow to -1

USUALLY OVERRIDDEN

inputs
   GUID           *pCode - Major code to use
   GUID           *pSub - Minor code to use
   PCMMLNode2      pNode - Node to extract info from. If NULL then creates a default
                  setup for the bump map
returns
   BOOL - TRUE if success
*/
BOOL CTextureVolSandstone::Init (DWORD dwRenderShard, const GUID *pCode, const GUID *pSub, PCMMLNode2 pNode)
{
   // must be done for every init call
   m_gCode = *pCode;
   m_gSub = *pSub;
   m_cColorAverage = -1;
   m_cColorAverageGlow = -1;
   m_dwRenderShard = dwRenderShard;

   // just tweak fractal 3d noise so generated random numbers
   Fractal3DNoise (m_dwRenderShard, 0,0,0,0,0, FALSE);

   if (pNode && !MMLFrom (pNode))
      return FALSE;

   return TRUE;
}



/********************************************************************************
CTextureVolSandstone::FillPixel - Given a point on the texture, fills a pixel describing
the texture. Standard API

ALWAYS OVERRIDDEN
*/
void CTextureVolSandstone::FillPixel (DWORD dwThread, DWORD dwFlags, WORD *pawColor, PTEXTPOINT5 pText, PTEXTPOINT5 pMax,
   PCMaterial pMat, float *pafGlow, BOOL fHighQuality)
{
   if (pafGlow)
      pafGlow[0] = pafGlow[1] = pafGlow[2] = 0;

   if (!pawColor)
      return;   // nothing else to calculate

   fp fVal = Fractal3DNoise(m_dwRenderShard, 
      pText->xyz[0] / (m_fNoiseSize * FRACT3DDIVS),
      pText->xyz[1] / (m_fNoiseSize * FRACT3DDIVS),
      pText->xyz[2] / (m_fNoiseSize * FRACT3DDIVS),
      m_dwRingNoiseIter,
      m_fNoiseDetail,
      FALSE);
   fVal *= m_fNoiseAffect;
   fVal += pText->xyz[2];  // since sandstone affects z
   fVal = myfmod (fVal / m_fTotalThick, 1);

   // find which one this is more than...
   DWORD dwLayer = 7;   // default to last layer
   DWORD i;
   for (i = 0; i < 8; i++)
      if (fVal >= m_afColorHeight[i])
         dwLayer = i;

   Gamma (m_acColorRange[dwLayer], pawColor);

   // finally, apply coloration
   ApplyTextureMods (&m_TextureMods, pawColor);
}


/********************************************************************************
CTextureVolSandstone::Delete - Standard API

ALWAYS OVERRIDDEN
*/
void CTextureVolSandstone::Delete (void)
{
   delete this;
}



/*********************************************************************************
CTextureVolSandstone::QueryBump - Sees if bump map used
*/
BOOL CTextureVolSandstone::QueryBump (DWORD dwThread)
{
   return m_fBumpAmount ? TRUE : FALSE;
}


/********************************************************************************
CTextureVolSandstone::BumpAtLoc - This returns the height of the surface at the given
texture-point location. The height is the amount above (or if negative, blow) the
default surface height, in meters.

inputs
   fp          *pfHeight - Filled with the elevation at the location above the surface
retursn
   BOOL - TRUE if the surface supports bump maps. FALSE if not
*/
BOOL CTextureVolSandstone::BumpAtLoc (PTEXTPOINT5 pText, fp *pfHeight)
{
   if (m_fBumpAmount == 0)
      return FALSE;

   fp fVal = Fractal3DNoise(m_dwRenderShard, 
      pText->xyz[0] / (m_fNoiseSize * FRACT3DDIVS),
      pText->xyz[1] / (m_fNoiseSize * FRACT3DDIVS),
      pText->xyz[2] / (m_fNoiseSize * FRACT3DDIVS),
      m_dwRingNoiseIter,
      m_fNoiseDetail,
      FALSE);

   *pfHeight = fVal * m_fBumpAmount;

   return TRUE;
}


static PWSTR gpszTextVolSandstone = L"TextVolSandstone";
static PWSTR gpszNoiseAffect = L"NoiseAffect";
static PWSTR gpszTotalThick = L"TotalThick";

/********************************************************************************
CTextureVolSandstone::MMLTo - Standard API
*/
PCMMLNode2 CTextureVolSandstone::MMLTo (void)
{
   PCMMLNode2 pNode = new CMMLNode2;
   if (!pNode)
      return NULL;
   pNode->NameSet (gpszTextVolSandstone);

   MMLValueSet (pNode, gpszDefH, m_fDefH);
   MMLValueSet (pNode, gpszDefV, m_fDefV);
   m_Material.MMLTo (pNode);

   MMLValueSet (pNode, gpszNoiseSize, m_fNoiseSize);
   MMLValueSet (pNode, gpszNoiseDetail, m_fNoiseDetail);
   MMLValueSet (pNode, gpszTotalThick, m_fTotalThick);
   MMLValueSet (pNode, gpszNoiseAffect, m_fNoiseAffect);
   MMLValueSet (pNode, gpszRingNoiseIter, (int)m_dwRingNoiseIter);
   MMLValueSet (pNode, gpszBumpAmount, m_fBumpAmount);

   DWORD i;
   WCHAR szTemp[64];
   for (i = 0; i < 8; i++) {
      swprintf (szTemp, L"color%d", (int)i);
      MMLValueSet (pNode, szTemp, (int) m_acColorRange[i]);

      swprintf (szTemp, L"colorheight%d", (int)i);
      MMLValueSet (pNode, szTemp, m_afColorHeight[i]);
   }

   return pNode;
}


/********************************************************************************
CTextureVolSandstone::MMLFrom - Standard API
*/
BOOL CTextureVolSandstone::MMLFrom (PCMMLNode2 pNode)
{
   Clear();


   if (!pNode)
      return TRUE;

   m_fDefH = MMLValueGetDouble (pNode, gpszDefH, m_fDefH);
   m_fDefV = MMLValueGetDouble (pNode, gpszDefV, m_fDefV);
   m_Material.MMLFrom (pNode);

   m_fNoiseSize = MMLValueGetDouble (pNode, gpszNoiseSize, .1);
   m_fNoiseDetail = MMLValueGetDouble (pNode, gpszNoiseDetail, .1);
   m_fTotalThick = MMLValueGetDouble (pNode, gpszTotalThick, .1);
   m_fNoiseAffect = MMLValueGetDouble (pNode, gpszNoiseAffect, .1);
   m_dwRingNoiseIter = (DWORD) MMLValueGetInt (pNode, gpszRingNoiseIter, (int)m_dwRingNoiseIter);
   m_fBumpAmount = MMLValueGetDouble (pNode, gpszBumpAmount, m_fBumpAmount);

   DWORD i;
   WCHAR szTemp[64];
   for (i = 0; i < 8; i++) {
      swprintf (szTemp, L"color%d", (int)i);
      m_acColorRange[i] = (COLORREF) MMLValueGetInt (pNode, szTemp, m_acColorRange[i]);

      swprintf (szTemp, L"colorheight%d", (int)i);
      m_afColorHeight[i] = MMLValueGetDouble (pNode, szTemp, m_afColorHeight[i]);
   }

   SetDef();

   return TRUE;
}




BOOL VolSandstonePage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PTEXTEDITINFO pt = (PTEXTEDITINFO) pPage->m_pUserData;
   PCTextureVolSandstone pv = (PCTextureVolSandstone) pt->pThis;

   if (TEHelperMessageHook (pPage, dwMessage, pParam))
      return TRUE;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         // set the material
         PCEscControl pControl;

         MeasureToString (pPage, L"noisesize", pv->m_fNoiseSize, TRUE);
         MeasureToString (pPage, L"bumpamount", pv->m_fBumpAmount, TRUE);
         MeasureToString (pPage, L"noiseaffect", pv->m_fNoiseAffect, TRUE);
         MeasureToString (pPage, L"totalthick", pv->m_fTotalThick, TRUE);

         pControl = pPage->ControlFind (L"noisedetail");
         if (pControl)
            pControl->AttribSetInt (Pos(), (int)(pv->m_fNoiseDetail * 100));

         DWORD i;
         WCHAR szTemp[64];
         for (i = 0; i < 8; i++) {
            swprintf (szTemp, L"cstatus%d", (int) i);
            FillStatusColor (pPage, szTemp, pv->m_acColorRange[i]);

            swprintf (szTemp, L"cheight%d", (int) i);
            pControl = pPage->ControlFind (szTemp);
            if (pControl)
               pControl->AttribSetInt (Pos(), (int)(pv->m_afColorHeight[i] * 100));
         }

         // set the material
         ComboBoxSet (pPage, L"material", pv->m_Material.m_dwID);
         pControl = pPage->ControlFind (L"editmaterial");
         if (pControl)
            pControl->Enable (pv->m_Material.m_dwID ? FALSE : TRUE);

         // set the info
         DoubleToControl (pPage, L"ringnoiseiter", pv->m_dwRingNoiseIter);
      }
      break;

   case ESCM_USER+186:  // get all the control values
      {
         PCEscControl pControl;

         MeasureParseString (pPage, L"noisesize", &pv->m_fNoiseSize);
         pv->m_fNoiseSize = max(.001, pv->m_fNoiseSize);

         MeasureParseString (pPage, L"bumpamount", &pv->m_fBumpAmount);
         MeasureParseString (pPage, L"noiseaffect", &pv->m_fNoiseAffect);
         MeasureParseString (pPage, L"totalthick", &pv->m_fTotalThick);
         pv->m_fTotalThick = max(.001, pv->m_fTotalThick);

         pControl = pPage->ControlFind (L"noisedetail");
         if (pControl)
            pv->m_fNoiseDetail = (fp)pControl->AttribGetInt (Pos()) / 100.0;

         DWORD i;
         WCHAR szTemp[64];
         for (i = 0; i < 8; i++) {
            swprintf (szTemp, L"cheight%d", (int) i);
            pControl = pPage->ControlFind (szTemp);
            if (pControl)
               pv->m_afColorHeight[i] = (fp)pControl->AttribGetInt (Pos()) / 100.0;
         }

         pv->m_dwRingNoiseIter = (DWORD) DoubleFromControl (pPage, L"ringnoiseiter");
         pv->m_dwRingNoiseIter = max(pv->m_dwRingNoiseIter, 1);
         pv->m_dwRingNoiseIter = min(pv->m_dwRingNoiseIter, 10);

         pv->SetDef();
      }
      break;

   case ESCN_COMBOBOXSELCHANGE:
      {
         PESCNCOMBOBOXSELCHANGE p = (PESCNCOMBOBOXSELCHANGE) pParam;
         if (!p->pControl->m_pszName)
            break;

         // only care about transparency
         if (!_wcsicmp(p->pControl->m_pszName, L"material")) {
            DWORD dwVal;
            dwVal = p->pszName ? _wtoi(p->pszName) : 0;
            if (dwVal == pv->m_Material.m_dwID)
               break; // unchanged
            if (dwVal)
               pv->m_Material.InitFromID (dwVal);
            else
               pv->m_Material.m_dwID = MATERIAL_CUSTOM;

            // eanble/disable button to edit
            PCEscControl pControl;
            pControl = pPage->ControlFind (L"editmaterial");
            if (pControl)
               pControl->Enable (pv->m_Material.m_dwID ? FALSE : TRUE);

            return TRUE;
         }
      }
      break;


   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;

         if (!p->pControl || !p->pControl->m_pszName)
            break;

         PWSTR pszColor = L"cbutton";
         DWORD dwSizeColor = (DWORD)wcslen(pszColor);

         if (!_wcsicmp(p->pControl->m_pszName, L"editmaterial")) {
            if (!pv->m_Material.Dialog (pPage->m_pWindow->m_hWnd))
               pPage->Exit (L"[close]");
            return TRUE;
         }
         else if (!wcsncmp(p->pControl->m_pszName, pszColor, dwSizeColor)) {
            DWORD dwNum = (DWORD)_wtoi(p->pControl->m_pszName + dwSizeColor);
            dwNum = min(7,dwNum);
            WCHAR szTemp[64];
            swprintf (szTemp, L"cstatus%d", (int) dwNum);
            pv->m_acColorRange[dwNum] = AskColor (pPage->m_pWindow->m_hWnd, pv->m_acColorRange[dwNum], pPage, szTemp);
            return TRUE;
         }
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Volumetric noise";
            return TRUE;
         }
      }
      break;
   };

   return DefPage (pPage, dwMessage, pParam);
}



/********************************************************************************
CTextureVolSandstone::MMLFrom - Standard API
*/
BOOL CTextureVolSandstone::Dialog (PCEscWindow pWindow)
{
   TEXTEDITINFO ti;
   PWSTR pszRet;
   memset (&ti, 0, sizeof(ti));
   ti.hBit = NULL;
   ti.pCreator = this;
   ti.pThis = this;

   pszRet = pWindow->PageDialog (ghInstance, IDR_MMLVOLSANDSTONE, VolSandstonePage, &ti);
   if (ti.hBit) {
      DeleteObject (ti.hBit);
      ti.hBit = NULL;
   }

   return pszRet && !_wcsicmp(pszRet, Back());
}






/********************************************************************************
CTextureVolWood::Constructor and destructor
*/
CTextureVolWood::CTextureVolWood (void)
{
   Clear();
}

CTextureVolWood::~CTextureVolWood (void)
{
   // do nothing for now
}




/********************************************************************************
CTextureVolWood::Init - Initializes the object, pulling out relevent information
from pNode. This must fill in a number of member variables, as listed under
"filled in by init". Also MUST clear m_cColorAverage and m_cColorAverageGlow to -1

USUALLY OVERRIDDEN

inputs
   GUID           *pCode - Major code to use
   GUID           *pSub - Minor code to use
   PCMMLNode2      pNode - Node to extract info from. If NULL then creates a default
                  setup for the bump map
returns
   BOOL - TRUE if success
*/
BOOL CTextureVolWood::Init (DWORD dwRenderShard, const GUID *pCode, const GUID *pSub, PCMMLNode2 pNode)
{
   // must be done for every init call
   m_gCode = *pCode;
   m_gSub = *pSub;
   m_cColorAverage = -1;
   m_cColorAverageGlow = -1;
   m_dwRenderShard = dwRenderShard;

   // just tweak fractal 3d noise so generated random numbers
   Fractal3DNoise (m_dwRenderShard, 0,0,0,0,0, FALSE);

   if (pNode && !MMLFrom (pNode))
      return FALSE;

   return TRUE;
}


/********************************************************************************
CTextureVolWood::Clear - Clears the attributes to a default value
*/
void CTextureVolWood::Clear(void)
{
   // smaller scale
   m_Material.InitFromID (MATERIAL_PAINTSEMIGLOSS);

   m_acColors[0] = RGB(0xd0, 0xb0, 0x40);
   m_acColors[1] = RGB(0xd0, 0xb0, 0x30);
   m_acColors[2] = RGB(0xb0, 0x90, 0x30);
   m_acColors[3] = RGB(0x90, 0x70, 0x00);
   m_acColors[4] = RGB(0x50,0x30,0);

   m_fRingThickness = 0.005;
   m_pRingNoiseScale.Zero();
   m_pRingNoiseScale.p[0] = m_pRingNoiseScale.p[1] = m_pRingNoiseScale.p[2] = 1.0 / FRACT3DDIVS; // 1 m to repeat pattern
   m_pRingNoiseScale.p[3] = 0.2;
   m_dwRingNoiseIter = 3;
   m_fRingNoiseDecay = 0.8;

   SetDef();
}


/********************************************************************************
CTextureVolWood::SetDef - Sets m_fDefH and m_fDefV based upon other settings.
*/
void CTextureVolWood::SetDef()
{
   m_fDefH = m_fDefV = m_fRingThickness * 10;
}


/********************************************************************************
CTextureVolWood::FillPixel - Given a point on the texture, fills a pixel describing
the texture. Standard API

ALWAYS OVERRIDDEN
*/
void CTextureVolWood::FillPixel (DWORD dwThread, DWORD dwFlags, WORD *pawColor, PTEXTPOINT5 pText, PTEXTPOINT5 pMax,
   PCMaterial pMat, float *pafGlow, BOOL fHighQuality)
{
   if (pafGlow)
      pafGlow[0] = pafGlow[1] = pafGlow[2] = 0;

   if (!pawColor)
      return;  // nothing else to calculate

   // figure out relationship compared to center of tree
   fp fTreeVar = Fractal3DNoise(m_dwRenderShard, 
      pText->xyz[0] / (m_pRingNoiseScale.p[0] * FRACT3DDIVS),
      pText->xyz[1] / (m_pRingNoiseScale.p[1] * FRACT3DDIVS),
      pText->xyz[2] / (m_pRingNoiseScale.p[2] * FRACT3DDIVS),
      m_dwRingNoiseIter,
      m_fRingNoiseDecay,
      FALSE);
   fTreeVar = fTreeVar * m_pRingNoiseScale.p[3];

   // how far in
   fp fRing = sqrt(pText->xyz[0]*pText->xyz[0] + pText->xyz[2] * pText->xyz[2]) /
      m_fRingThickness + fTreeVar;
   fRing = myfmod(fRing, 1) * 5.0;

   // position within ring
   DWORD dwRingLoc = (DWORD)fRing;
   fRing -= (fp)dwRingLoc;
   dwRingLoc = min(dwRingLoc, 5-1);

   // convert colors
   WORD aw[2][3];
   Gamma (m_acColors[dwRingLoc], aw[0]);
   Gamma (m_acColors[(dwRingLoc+1)%5], aw[1]);

   DWORD i;
   fp f;
   for (i = 0; i < 3; i++) {
      f = (fp)aw[0][i] * (1.0 - fRing) + (fp)aw[1][i] * fRing;
      f = min(f, 0xffff);
      pawColor[i] = (WORD)f;
   }

   // finally, apply coloration
   ApplyTextureMods (&m_TextureMods, pawColor);
}


/********************************************************************************
CTextureVolWood::Delete - Standard API

ALWAYS OVERRIDDEN
*/
void CTextureVolWood::Delete (void)
{
   delete this;
}



static PWSTR gpszTextVolWood = L"TextVolWood";
static PWSTR gpszRingThickness = L"RingThickness";
static PWSTR gpszRingNoiseScale = L"RingNoiseScale";
static PWSTR gpszRingNoiseDecay = L"RingNoiseDecay";

/********************************************************************************
CTextureVolWood::MMLTo - Standard API
*/
PCMMLNode2 CTextureVolWood::MMLTo (void)
{
   PCMMLNode2 pNode = new CMMLNode2;
   if (!pNode)
      return NULL;
   pNode->NameSet (gpszTextVolWood);

   MMLValueSet (pNode, gpszDefH, m_fDefH);
   MMLValueSet (pNode, gpszDefV, m_fDefV);
   m_Material.MMLTo (pNode);

   DWORD i;
   for (i = 0; i < 5; i++) {
      WCHAR szTemp[32];
      swprintf (szTemp, L"color%d", (int)i);
      MMLValueSet (pNode, szTemp, (int)m_acColors[i]);
   }

   MMLValueSet (pNode, gpszRingThickness, m_fRingThickness);
   MMLValueSet (pNode, gpszRingNoiseScale, &m_pRingNoiseScale);
   MMLValueSet (pNode, gpszRingNoiseIter, (int)m_dwRingNoiseIter);
   MMLValueSet (pNode, gpszRingNoiseDecay, m_fRingNoiseDecay);

   return pNode;
}



/********************************************************************************
CTextureVolWood::MMLFrom - Standard API
*/
BOOL CTextureVolWood::MMLFrom (PCMMLNode2 pNode)
{
   Clear();

   if (!pNode)
      return TRUE;

   m_fDefH = MMLValueGetDouble (pNode, gpszDefH, m_fDefH);
   m_fDefV = MMLValueGetDouble (pNode, gpszDefV, m_fDefV);
   m_Material.MMLFrom (pNode);

   DWORD i;
   for (i = 0; i < 5; i++) {
      WCHAR szTemp[32];
      swprintf (szTemp, L"color%d", (int)i);
      m_acColors[i] = (COLORREF) MMLValueGetInt (pNode, szTemp, (int)m_acColors[i]);
   }

   m_fRingThickness = MMLValueGetDouble (pNode, gpszRingThickness, m_fRingThickness);
   MMLValueGetPoint (pNode, gpszRingNoiseScale, &m_pRingNoiseScale);
   m_dwRingNoiseIter = (DWORD) MMLValueGetInt (pNode, gpszRingNoiseIter, (int)m_dwRingNoiseIter);
   m_fRingNoiseDecay = MMLValueGetDouble (pNode, gpszRingNoiseDecay, m_fRingNoiseDecay);

   SetDef();

   return TRUE;
}



BOOL VolWoodPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PTEXTEDITINFO pt = (PTEXTEDITINFO) pPage->m_pUserData;
   PCTextureVolWood pv = (PCTextureVolWood) pt->pThis;

   if (TEHelperMessageHook (pPage, dwMessage, pParam))
      return TRUE;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         DWORD i;
         WCHAR szTemp[64];
         for (i = 0; i < 5; i++) {
            swprintf (szTemp, L"colorcolor%d", (int) i);
            FillStatusColor (pPage, szTemp, pv->m_acColors[i]);
         }

         // set the material
         ComboBoxSet (pPage, L"material", pv->m_Material.m_dwID);
         PCEscControl pControl;
         pControl = pPage->ControlFind (L"editmaterial");
         if (pControl)
            pControl->Enable (pv->m_Material.m_dwID ? FALSE : TRUE);

         // set the info
         MeasureToString (pPage, L"ringthickness", pv->m_fRingThickness, TRUE);
         DoubleToControl (pPage, L"ringnoisescale3", pv->m_pRingNoiseScale.p[3]);
         MeasureToString (pPage, L"ringnoisescale0", pv->m_pRingNoiseScale.p[0]);
         DoubleToControl (pPage, L"ringnoiseiter", pv->m_dwRingNoiseIter);
         DoubleToControl (pPage, L"ringnoisedecay", pv->m_fRingNoiseDecay);
      }
      break;

   case ESCM_USER+186:  // get all the control values
      {
         MeasureParseString (pPage, L"ringthickness", &pv->m_fRingThickness);
         pv->m_fRingThickness = max(pv->m_fRingThickness, 0.001);

         MeasureParseString (pPage, L"ringnoisescale0", &pv->m_pRingNoiseScale.p[0]);
         pv->m_pRingNoiseScale.p[0] = max(pv->m_pRingNoiseScale.p[0], .01);
         pv->m_pRingNoiseScale.p[1] = pv->m_pRingNoiseScale.p[2] = pv->m_pRingNoiseScale.p[0];

         pv->m_pRingNoiseScale.p[3] = DoubleFromControl (pPage, L"ringnoisescale3");
         pv->m_pRingNoiseScale.p[3] = max (pv->m_pRingNoiseScale.p[3], 0);
         pv->m_pRingNoiseScale.p[3] = min(pv->m_pRingNoiseScale.p[3], 1);

         pv->m_dwRingNoiseIter = (DWORD) DoubleFromControl (pPage, L"ringnoiseiter");
         pv->m_dwRingNoiseIter = max(pv->m_dwRingNoiseIter, 1);
         pv->m_dwRingNoiseIter = min(pv->m_dwRingNoiseIter, 10);

         pv->m_fRingNoiseDecay = DoubleFromControl (pPage, L"ringnoisedecay");
         pv->m_fRingNoiseDecay = max(pv->m_fRingNoiseDecay, 0);
         pv->m_fRingNoiseDecay = min(pv->m_fRingNoiseDecay, 1);

         pv->SetDef();
      }
      break;

   case ESCN_COMBOBOXSELCHANGE:
      {
         PESCNCOMBOBOXSELCHANGE p = (PESCNCOMBOBOXSELCHANGE) pParam;
         if (!p->pControl->m_pszName)
            break;

         // only care about transparency
         if (!_wcsicmp(p->pControl->m_pszName, L"material")) {
            DWORD dwVal;
            dwVal = p->pszName ? _wtoi(p->pszName) : 0;
            if (dwVal == pv->m_Material.m_dwID)
               break; // unchanged
            if (dwVal)
               pv->m_Material.InitFromID (dwVal);
            else
               pv->m_Material.m_dwID = MATERIAL_CUSTOM;

            // eanble/disable button to edit
            PCEscControl pControl;
            pControl = pPage->ControlFind (L"editmaterial");
            if (pControl)
               pControl->Enable (pv->m_Material.m_dwID ? FALSE : TRUE);

            return TRUE;
         }
      }
      break;


   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;

         if (!p->pControl || !p->pControl->m_pszName)
            break;

         PWSTR szColor = L"colorbutton";
         DWORD dwLen = (DWORD)wcslen(szColor);
         if (!_wcsicmp(p->pControl->m_pszName, L"editmaterial")) {
            if (!pv->m_Material.Dialog (pPage->m_pWindow->m_hWnd))
               pPage->Exit (L"[close]");
            return TRUE;
         }
         else if (!wcsncmp(p->pControl->m_pszName, szColor, dwLen)) {
            DWORD i = _wtoi(p->pControl->m_pszName + dwLen);
            i = min(4,i);
            WCHAR szTemp[64];
            swprintf (szTemp, L"colorcolor%d", i);
            pv->m_acColors[i] = AskColor (pPage->m_pWindow->m_hWnd, pv->m_acColors[i], pPage, szTemp);
            return TRUE;
         }
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Volumetric wood";
            return TRUE;
         }
      }
      break;
   };

   return DefPage (pPage, dwMessage, pParam);
}




/********************************************************************************
CTextureVolWood::Dialog - Standard API
*/
BOOL CTextureVolWood::Dialog (PCEscWindow pWindow)
{
   TEXTEDITINFO ti;
   PWSTR pszRet;
   memset (&ti, 0, sizeof(ti));
   ti.hBit = NULL;
   ti.pCreator = this;
   ti.pThis = this;

   pszRet = pWindow->PageDialog (ghInstance, IDR_MMLVOLWOOD, VolWoodPage, &ti);
   if (ti.hBit) {
      DeleteObject (ti.hBit);
      ti.hBit = NULL;
   }

   return pszRet && !_wcsicmp(pszRet, Back());
}












/********************************************************************************
CTextureVolMarble::Constructor and destructor
*/
CTextureVolMarble::CTextureVolMarble (void)
{
   Clear();
}

CTextureVolMarble::~CTextureVolMarble (void)
{
   // do nothing for now
}




/********************************************************************************
CTextureVolMarble::Init - Initializes the object, pulling out relevent information
from pNode. This must fill in a number of member variables, as listed under
"filled in by init". Also MUST clear m_cColorAverage and m_cColorAverageGlow to -1

USUALLY OVERRIDDEN

inputs
   GUID           *pCode - Major code to use
   GUID           *pSub - Minor code to use
   PCMMLNode2      pNode - Node to extract info from. If NULL then creates a default
                  setup for the bump map
returns
   BOOL - TRUE if success
*/
BOOL CTextureVolMarble::Init (DWORD dwRenderShard, const GUID *pCode, const GUID *pSub, PCMMLNode2 pNode)
{
   // must be done for every init call
   m_gCode = *pCode;
   m_gSub = *pSub;
   m_cColorAverage = -1;
   m_cColorAverageGlow = -1;
   m_dwRenderShard = dwRenderShard;

   // just tweak fractal 3d noise so generated random numbers
   Fractal3DNoise (m_dwRenderShard, 0,0,0,0,0, FALSE);

   if (pNode && !MMLFrom (pNode))
      return FALSE;

   return TRUE;
}


/********************************************************************************
CTextureVolMarble::Clear - Clears the attributes to a default value
*/
void CTextureVolMarble::Clear(void)
{
   // smaller scale
   m_Material.InitFromID (MATERIAL_TILEGLAZED);

   m_fNoiseMax = FALSE;
   m_fNoiseSize = .05;
   m_fNoiseDetail = .5;
   m_fNoiseContrib = .2;
   m_dwRingNoiseIter = 3;

   DWORD i;
   for (i = 0; i < 8; i ++)
      m_acColor[i] = RGB(i * 32, i * 32, i * 32);

   SetDef();
}



/********************************************************************************
CTextureVolMarble::SetDef - Sets m_fDefH and m_fDefV based upon other settings.
*/
void CTextureVolMarble::SetDef()
{
   m_fDefH = m_fDefV = m_fNoiseSize * 10;
}


/********************************************************************************
CTextureVolMarble::FillPixel - Given a point on the texture, fills a pixel describing
the texture. Standard API

ALWAYS OVERRIDDEN
*/
void CTextureVolMarble::FillPixel (DWORD dwThread, DWORD dwFlags, WORD *pawColor, PTEXTPOINT5 pText, PTEXTPOINT5 pMax,
   PCMaterial pMat, float *pafGlow, BOOL fHighQuality)
{
   if (pafGlow)
      pafGlow[0] = pafGlow[1] = pafGlow[2] = 0;

   if (!pawColor)
      return;  // nothing else to calcuate

   // figure out relationship compared to center of tree
   fp f = Fractal3DNoise(m_dwRenderShard, 
      pText->xyz[0] / (m_fNoiseSize * FRACT3DDIVS),
      pText->xyz[1] / (m_fNoiseSize * FRACT3DDIVS),
      pText->xyz[2] / (m_fNoiseSize * FRACT3DDIVS),
      m_dwRingNoiseIter,
      m_fNoiseDetail,
      TRUE);


   f *= m_fNoiseContrib * 2 * PI;
   // BUGFIX - Have to take out sin or pattern doenst repeat f += fSin * (fp)x + fCos * (fp)y;

   if (m_fNoiseMax) {
      f = max(0,f);
      f = min(PI/2,f);
   }
   f = sin (f);

   // convert to color number
   DWORD dwNumLevels = 8;
   f = (f + 1) / 2.0 * (fp)(dwNumLevels - 1);
   f = max(0,f);
   DWORD dwC;
   dwC = (DWORD) f;
   dwC = min(dwNumLevels-2, f);
   f = (f - (fp) dwC); // so from 0 to 1
   f = min(1,f);

   // average
   WORD aw[2][3];
   Gamma (m_acColor[dwC], aw[0]);
   Gamma (m_acColor[dwC+1], aw[1]);

   DWORD i;
   fp f2;
   for (i = 0; i < 3; i++) {
      f2 = (fp)aw[0][i] * (1.0 - f) + (fp)aw[1][i] * f;
      f2 = min(f2, 0xffff);
      pawColor[i] = (WORD)f2;
   }

   // finally, apply coloration
   ApplyTextureMods (&m_TextureMods, pawColor);
}


/********************************************************************************
CTextureVolMarble::Delete - Standard API

ALWAYS OVERRIDDEN
*/
void CTextureVolMarble::Delete (void)
{
   delete this;
}



static PWSTR gpszTextVolMarble = L"TextVolMarble";

/********************************************************************************
CTextureVolMarble::MMLTo - Standard API
*/
PCMMLNode2 CTextureVolMarble::MMLTo (void)
{
   PCMMLNode2 pNode = new CMMLNode2;
   if (!pNode)
      return NULL;
   pNode->NameSet (gpszTextVolMarble);

   MMLValueSet (pNode, gpszDefH, m_fDefH);
   MMLValueSet (pNode, gpszDefV, m_fDefV);
   m_Material.MMLTo (pNode);

   MMLValueSet (pNode, gpszNoiseMax, (int) m_fNoiseMax);
   MMLValueSet (pNode, gpszNoiseSize, m_fNoiseSize);
   MMLValueSet (pNode, gpszNoiseDetail, m_fNoiseDetail);
   MMLValueSet (pNode, gpszNoiseContrib, m_fNoiseContrib);
   MMLValueSet (pNode, gpszRingNoiseIter, (int)m_dwRingNoiseIter);

   DWORD i;
   WCHAR szTemp[64];
   for (i = 0; i < 8; i++) {
      swprintf (szTemp, L"color%d", (int)i);
      MMLValueSet (pNode, szTemp, (int) m_acColor[i]);
   }

   return pNode;
}



/********************************************************************************
CTextureVolMarble::MMLFrom - Standard API
*/
BOOL CTextureVolMarble::MMLFrom (PCMMLNode2 pNode)
{
   Clear();

   if (!pNode)
      return TRUE;

   m_fDefH = MMLValueGetDouble (pNode, gpszDefH, m_fDefH);
   m_fDefV = MMLValueGetDouble (pNode, gpszDefV, m_fDefV);
   m_Material.MMLFrom (pNode);

   m_fNoiseMax = (BOOL)MMLValueGetInt (pNode, gpszNoiseMax, (int) FALSE);
   m_fNoiseSize = MMLValueGetDouble (pNode, gpszNoiseSize, .1);
   m_fNoiseDetail = MMLValueGetDouble (pNode, gpszNoiseDetail, .1);
   m_fNoiseContrib = MMLValueGetDouble (pNode, gpszNoiseContrib, .1);
   m_dwRingNoiseIter = (DWORD) MMLValueGetInt (pNode, gpszRingNoiseIter, (int)m_dwRingNoiseIter);

   DWORD i;
   WCHAR szTemp[64];
   for (i = 0; i < 8; i++) {
      swprintf (szTemp, L"color%d", (int)i);
      m_acColor[i] = (COLORREF) MMLValueGetInt (pNode, szTemp, 0);
   }

   SetDef();

   return TRUE;
}



BOOL VolMarblePage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PTEXTEDITINFO pt = (PTEXTEDITINFO) pPage->m_pUserData;
   PCTextureVolMarble pv = (PCTextureVolMarble) pt->pThis;

   if (TEHelperMessageHook (pPage, dwMessage, pParam))
      return TRUE;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         // set the material
         PCEscControl pControl;
         pControl = pPage->ControlFind (L"noisemax");
         if (pControl)
            pControl->AttribSetBOOL (Checked(), pv->m_fNoiseMax);

         MeasureToString (pPage, L"noisesize", pv->m_fNoiseSize, TRUE);

         pControl = pPage->ControlFind (L"noisedetail");
         if (pControl)
            pControl->AttribSetInt (Pos(), (int)(pv->m_fNoiseDetail * 100));
         pControl = pPage->ControlFind (L"noisecontrib");
         if (pControl)
            pControl->AttribSetInt (Pos(), (int)(pv->m_fNoiseContrib * 100));

         DWORD i;
         WCHAR szTemp[64];
         for (i = 0; i < 8; i++) {
            swprintf (szTemp, L"cstatus%d", (int) i);
            FillStatusColor (pPage, szTemp, pv->m_acColor[i]);
         }

         // set the material
         ComboBoxSet (pPage, L"material", pv->m_Material.m_dwID);
         pControl = pPage->ControlFind (L"editmaterial");
         if (pControl)
            pControl->Enable (pv->m_Material.m_dwID ? FALSE : TRUE);

         // set the info
         DoubleToControl (pPage, L"ringnoiseiter", pv->m_dwRingNoiseIter);
      }
      break;

   case ESCM_USER+186:  // get all the control values
      {
         PCEscControl pControl;
         pControl = pPage->ControlFind (L"noisemax");
         if (pControl)
            pv->m_fNoiseMax = pControl->AttribGetBOOL (Checked());

         MeasureParseString (pPage, L"noisesize", &pv->m_fNoiseSize);
         pv->m_fNoiseSize = max(.001, pv->m_fNoiseSize);

         pControl = pPage->ControlFind (L"noisedetail");
         if (pControl)
            pv->m_fNoiseDetail = (fp)pControl->AttribGetInt (Pos()) / 100.0;
         pControl = pPage->ControlFind (L"noisecontrib");
         if (pControl)
            pv->m_fNoiseContrib = (fp)pControl->AttribGetInt (Pos()) / 100.0;

         pv->m_dwRingNoiseIter = (DWORD) DoubleFromControl (pPage, L"ringnoiseiter");
         pv->m_dwRingNoiseIter = max(pv->m_dwRingNoiseIter, 1);
         pv->m_dwRingNoiseIter = min(pv->m_dwRingNoiseIter, 10);

         pv->SetDef();
      }
      break;

   case ESCN_COMBOBOXSELCHANGE:
      {
         PESCNCOMBOBOXSELCHANGE p = (PESCNCOMBOBOXSELCHANGE) pParam;
         if (!p->pControl->m_pszName)
            break;

         // only care about transparency
         if (!_wcsicmp(p->pControl->m_pszName, L"material")) {
            DWORD dwVal;
            dwVal = p->pszName ? _wtoi(p->pszName) : 0;
            if (dwVal == pv->m_Material.m_dwID)
               break; // unchanged
            if (dwVal)
               pv->m_Material.InitFromID (dwVal);
            else
               pv->m_Material.m_dwID = MATERIAL_CUSTOM;

            // eanble/disable button to edit
            PCEscControl pControl;
            pControl = pPage->ControlFind (L"editmaterial");
            if (pControl)
               pControl->Enable (pv->m_Material.m_dwID ? FALSE : TRUE);

            return TRUE;
         }
      }
      break;


   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;

         if (!p->pControl || !p->pControl->m_pszName)
            break;

         PWSTR pszColor = L"cbutton";
         DWORD dwSizeColor = (DWORD)wcslen(pszColor);

         if (!_wcsicmp(p->pControl->m_pszName, L"editmaterial")) {
            if (!pv->m_Material.Dialog (pPage->m_pWindow->m_hWnd))
               pPage->Exit (L"[close]");
            return TRUE;
         }
         else if (!wcsncmp(p->pControl->m_pszName, pszColor, dwSizeColor)) {
            DWORD dwNum = (DWORD)_wtoi(p->pControl->m_pszName + dwSizeColor);
            dwNum = min(7,dwNum);
            WCHAR szTemp[64];
            swprintf (szTemp, L"cstatus%d", (int) dwNum);
            pv->m_acColor[dwNum] = AskColor (pPage->m_pWindow->m_hWnd, pv->m_acColor[dwNum], pPage, szTemp);
            return TRUE;
         }
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Volumetric marble";
            return TRUE;
         }
      }
      break;
   };

   return DefPage (pPage, dwMessage, pParam);
}




/********************************************************************************
CTextureVolMarble::Dialog - Standard API
*/
BOOL CTextureVolMarble::Dialog (PCEscWindow pWindow)
{
   TEXTEDITINFO ti;
   PWSTR pszRet;
   memset (&ti, 0, sizeof(ti));
   ti.hBit = NULL;
   ti.pCreator = this;
   ti.pThis = this;

   pszRet = pWindow->PageDialog (ghInstance, IDR_MMLVOLMARBLE, VolMarblePage, &ti);
   if (ti.hBit) {
      DeleteObject (ti.hBit);
      ti.hBit = NULL;
   }

   return pszRet && !_wcsicmp(pszRet, Back());
}



// BUGBUG - May want to add noise to sandstone
