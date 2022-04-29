/********************************************************************************
Textures.cpp - Code for handling texture maps and color schemes.

begun 3/11/01 by Mike Rozak
Copyright 2001 Mike Rozak. All rights reserved
*/

#include <windows.h>
#include <math.h>
#include <crtdbg.h>
#include "resource.h"
#include "escarpment.h"
#include "..\M3D.h"
#include "texture.h"

#ifdef _DEBUG
#define WORKONTEXTURES // BUGBUG
#endif


#define MAXTEXTCACHE    250      // BUGFIX - Was 100, moved back to 250


// EMTINFO - Information for multithreaded
typedef struct {
   DWORD          dwStart;    // start number
   DWORD          dwStop;     // stop number
   DWORD          dwPass;     // pass type to do
   DWORD          dwVer;      // version info

   // specificall for pass 1
   PTMIMAGE       pImage;
   DWORD          dwNewX;
   DWORD          dwNewY;
   DWORD          dwType;
   DWORD          dwTakeFrom;
   DWORD          dwFilter;
} EMTINFO, *PEMTINFO;

typedef struct {
   GUID        gCode;   // code guid
   GUID        gSub;    // sub guid
   DWORD       dwX;     // x pixels across
   DWORD       dwY;     // y pixels across
   //DFDATE      dwLastUsed; // last used date - so toss out unused ones

   // from material
   DWORD       m_dwID;              // material ID, see MATERIAL_XXX
   DWORD       m_dwMaterialType;    // steel, wood, etc. To be defined
   float       m_fThickness;        // thikcnes of material. Not used at moment
   WORD        m_wTransparency;     // 0 = opaque, 0xffff is fully transparent
   WORD        m_wTransAngle;       // 0xffff causes transparency to decrease when opaque
   WORD        m_wSpecExponent;     // specularity exponent, 100 = 1.0, higher number larger multiples
   WORD        m_wSpecReflect;      // amount reflected by specularity, 0xffff is max, 0x0000 is none
   WORD        m_wSpecPlastic;      // how much specularilty looks like plastic. 0xffff - very plasticy, 0x0000 - not plasticy
   WORD        m_wIndexOfRefract;   // index of refraction x 100
   WORD        m_wReflectAmount;    // amount of relfection
   WORD        m_wReflectAngle;     // how reflection reacts to angle
   WORD        m_wTranslucent;      // 0xffff = max translucency
   BOOL        m_fNoShadows;        // if TRUE, no shadows
   
   TEXTINFO    ti;      // texture info

   // dwY x dwX x 3bytes - color map, specularity and bump applied, DWORD aligned

   // if ti.dwMap & 0x03 (specularity or bump map) - Color map without spec and bump
   // dwY x dwX x 3bytes - RGB. DWORD aligned

   // if ti.dwMap & 0x01 (specularity map)
   // dwY x dwX x 2 bytes (HIBYTE(m_wSpecExponent), HIBYTE(m_wSpecReflect)) - DWORD aligned

   // if ti.dwMap & 0x02 (bump map)
   // dwY x dwX x short (bump map height in pixels, 0x100 == 1.0 pixel)

   // if ti.dwMap & 0x04 (transparency map)
   // dwY x dwX x BYTE (0..255 transparency)... DWORD aligned

   // if ti.dwMap & 0x08 (glow map)
   // dwY x dwX x 3 bytes - RGB. DWORD aligned

} ALGOTEXTCACHE, *PALGOTEXTCACHE;


void SphereDraw (PCImage pImage,
                  fp fRadius, PCTextureMapSocket pTexture,
                  fp afTextureMatrix[2][2], PCMatrix pmTextureMatrix,
                  PCMaterial pMaterial, BOOL fStretchToFit);

BOOL TextureIsInTextureList (DWORD dwRenderShard, GUID *pgCode, GUID *pgSub);
void TextureAddTextureColorList (DWORD dwRenderShard, PCObjectSurface pos);
void TextureAlgoMaxCache (DWORD dwRenderShard, DWORD dwMax /* = MAXTEXTCACHE*/);

static CListVariable galistALGOTEXTCACHE[MAXRENDERSHARDS];   // list of algorithmic textures, each elemennt contains ALGOTEXTCACHE structure followed by texture
static PCMegaFile gapmfAlgoTexture[MAXRENDERSHARDS] = {NULL,NULL,NULL,NULL};     // algorithmic texture megafile


static CBTree gaTreeTextureList[MAXRENDERSHARDS];     // list of textures at use right now. List of 2xGUID
static CBTree gaTreeColorList[MAXRENDERSHARDS];       // list of colors at use right now. List of COLORREF

static PWSTR gpszInfoDate = L"InfoDate";
static PWSTR gpszInfoTime = L"InfoTime";

static int gaiTextureDetail[MAXRENDERSHARDS] = {0,0,0,0};   // amount of detail to augment procedural textures by. 0=none, 1=2x, 2=4x, -1=1/2, etc.
static PWSTR gpszTextureDetail = L"TextureDetail";


/*********************************************************************************
TextureDetailGet - Returns the current texture detail.

returns
   int - Amount of detail to augment procedural textures by. 0=none, 1=2x, 2=4x, -1=1/2, etc.
*/
int TextureDetailGet (DWORD dwRenderShard)
{
   return gaiTextureDetail[dwRenderShard];
}



/*********************************************************************************
TextureDetailSet - Sets the current texture detail.

inputs
   int      iDetail - Amount of detail to augment procedural textures by. 0=none, 1=2x, 2=4x, -1=1/2, etc.
returns
   BOOL - TRUE if changed, FALSE if the same
*/
BOOL TextureDetailSet (DWORD dwRenderShard, int iDetail)
{
   if (gaiTextureDetail[dwRenderShard] == iDetail)
      return FALSE;

   // changing the detail... therefore

   // remember
   gaiTextureDetail[dwRenderShard] = iDetail;

   // wipe out the current cache
   TextureCacheClear (dwRenderShard, 0);

   // BUGFIX - wipe out cached data
   TextureAlgoMaxCache (dwRenderShard, 0);

   // wipe out the megafile contents
   gapmfAlgoTexture[dwRenderShard]->Clear();

   // write a new size into the megafile contens
   gapmfAlgoTexture[dwRenderShard]->Save (gpszTextureDetail, &gaiTextureDetail[dwRenderShard], sizeof(gaiTextureDetail[dwRenderShard]));

   return gaiTextureDetail[dwRenderShard];
}


/*********************************************************************************
CubicTextureInterp - Interpolates a cubic textures.

inputs
   fp       af[4][4] - Array of 4x4 values, [y][x]
   fp       fX - X, from 0..1
   fp       fy - Y, from 0..1
returns
   fp - Value... may excede min and max values
*/
fp CubicTextureInterp (fp af[4][4], fp fX, fp fY)
{
   // BUGFIX if all the points are the same then easy... optimization for the face rendering
   // where most of the time the textures dont need interpolation
   DWORD i;
   for (i = 1; i < 16; i++)
      if (af[i%4][i&0x03] != af[0][0])
         break;
   if (i >= 16)
      return af[0][0];  // all values the same so fast

   fp afVert[4];
   for (i = 0; i < 4; i++)
      afVert[i] = HermiteCubic (fX, af[i][0], af[i][1], af[i][2], af[i][3]);

   return HermiteCubic (fY, afVert[0], afVert[1], afVert[2], afVert[3]);
}

/**********************************************************************************
TextureThread - Returns the thread to use based on the thread ID
*/
DWORD TextureThread (void)
{
   return GetCurrentThreadId() % GROUNDTHREADS;
}


/******************************************************************************
IsImage - Returns true if a texture is thought to be an image - by checking
to see that the major names has image in it.

inputs
   PWSTR       pszMajor - Major
   PWSTR       pszMinor - Minor
returns
   BOOL - TRUE
*/
BOOL IsImage (PWSTR pszMajor, PWSTR pszMinor)
{
   if (!_wcsicmp(pszMajor, L"images"))
      return TRUE;

   //if (!_wcsicmp(pszMajor, L"outside") && !_wcsicmp(pszMinor, L"grass"))
   //   return TRUE;

   return FALSE;
}

/******************************************************************************
FloatColorToTMGLOW - Converts an array of 3 floats to a TMGLOW value.

inputs
   float       *pafColor - Array of 3 colors
   PTMGLOW     pGlow - Filled in
*/
void FloatColorToTMGLOW (float *pafColor, PTMGLOW pGlow)
{
   // find the max
   float fMax;
   DWORD i;
   fMax = 0;
   for (i = 0; i < 3; i++) {
      fMax = max(fMax, pafColor[i]);
   }

   // fill in
   float f;
   fMax /= (fp)0xffff;
   pGlow->fIntensity = fMax;
   WORD aw[3];
   if (fMax) {
      for (i = 0; i < 3; i++) {
         f = pafColor[i] / fMax;
         f = min(f, 0xffff);
         aw[i] = (WORD) f;
      }
      pGlow->cRGB = UnGamma (aw);
   }
   else
      pGlow->cRGB = 0;
}



/******************************************************************************
TMGLOWToFloatColor - Converts a TMGLOW to an array of 3 floats.

inputs
   PTMGLOW     pGlow - Contains glow
   float       *pafColor - Array of 3 colors that filled in
*/
void TMGLOWToFloatColor (PTMGLOW pGlow, float *pafColor)
{
   // gamma
   WORD aw[3];
   Gamma (pGlow->cRGB, aw);

   DWORD i;
   for (i = 0; i < 3; i++)
      pafColor[i] = (fp)aw[i] * pGlow->fIntensity;
}

/******************************************************************************
AttachDateTimeToMML - Writes the date and time into MML.

input
   PCMMLNode2      pNode - node to attach to
*/
void AttachDateTimeToMML (PCMMLNode2 pNode)
{
   MMLValueSet (pNode, gpszInfoDate, (int) Today());
   MMLValueSet (pNode, gpszInfoTime, (int) Now());
}


/*****************************************************************************
GetDateAndTimeFromMML - Get the date and time from MML

inputs
   PCMMLNode2      pNode - to get from
   DFDATE         *pDate - Filled with date
   DFTIME         *pTime - Filled with time
*/
void GetDateAndTimeFromMML (PCMMLNode2 pNode, DFDATE *pDate, DFTIME *pTime)
{
   *pDate = (DFDATE)MMLValueGetInt (pNode, gpszInfoDate, 0);
   *pTime = (DFTIME)MMLValueGetInt (pNode, gpszInfoTime, 0);
}

/****************************************************************************
TextureAlgoCacheName - Makes the name for the algorithmic cache, as it
appears in the megafile.

inputs
   GUID        *pgCode - Code
   GUID        *pgSub - Sub-code
   PWSTR       psz - Filled with the name. Must be at least 65 chars long
returns
   PWSTR - Returns psz
*/
PWSTR TextureAlgoCacheName (const GUID *pgCode, const GUID *pgSub, PWSTR psz)
{
   GUID ag[2];
   ag[0] = *pgSub;
   ag[1] = *pgCode;
   MMLBinaryToString ((PBYTE) &ag[0], sizeof(ag), psz);
   return psz;
}

/****************************************************************************
TextureAlgoCacheRemove - Removed a cached texture from the list.

inputs
   GUID        *pgCode - Texture code
   GUID        *pgSub - Texture sub
returns
   BOOL - TRUE if found and removed
*/
BOOL TextureAlgoCacheRemove (DWORD dwRenderShard, const GUID *pgCode, const GUID *pgSub)
{
   PALGOTEXTCACHE p;
   DWORD i;
   BOOL fRet = FALSE;

   for (i = galistALGOTEXTCACHE[dwRenderShard].Num()-1; i < galistALGOTEXTCACHE[dwRenderShard].Num(); i--) {
      p = (PALGOTEXTCACHE) galistALGOTEXTCACHE[dwRenderShard].Get (i);

      if (IsEqualGUID (*pgCode, p->gCode) && IsEqualGUID (*pgSub, p->gSub)) {
         // found
         galistALGOTEXTCACHE[dwRenderShard].Remove (i);
         fRet = TRUE;
         // BUGFIX - Continue on removing all instancesreturn TRUE;
      }
   }

   WCHAR szTemp[96];
   fRet |= gapmfAlgoTexture[dwRenderShard]->Delete (TextureAlgoCacheName(pgCode, pgSub, szTemp));
   return fRet;
}


/****************************************************************************
TextureAlgoMaxCache - If the cache is larger than a 500(?) elements, then
remove the oldest.
*/

void TextureAlgoMaxCache (DWORD dwRenderShard, DWORD dwMax)
{
   if (galistALGOTEXTCACHE[dwRenderShard].Num() <= dwMax)
      return;  // no change

   while (galistALGOTEXTCACHE[dwRenderShard].Num() > dwMax) {
      // just pick one at random and delete it
      DWORD dwNum = (DWORD)rand() % galistALGOTEXTCACHE[dwRenderShard].Num();
      galistALGOTEXTCACHE[dwRenderShard].Remove (dwNum);
   }
}

/****************************************************************************
TextureAlgoCache - Cache an algorithmic texture given the guids and
   other information from the render call.

inputs
   GUID        *pgCode - Texture code
   GUID        *pgSub - Texture sub
   PCImage     pImage - Image from CObjectCreator::Render
   PCImage     pImage2 - Image from CObjectCreator::Render. Only used if have spec map
   PCMaterial  pMaterial - Material from render
   PTEXTINFo   pti - Texture infor from render call
returns
   BOOL - TRUE if success
*/
BOOL TextureAlgoCache (DWORD dwRenderShard, const GUID *pgCode, const GUID *pgSub, PCImage pImage, PCImage pImage2,
                       PCMaterial pMaterial, PTEXTINFO pti)
{
   // remove it in case it's there
   // NOTE: Removing all instances of the texture
   TextureAlgoCacheRemove (dwRenderShard, pgCode, pgSub);

   // allocate the memory needed tow rite it out - basically a lot
   CMem mem;
   DWORD dwNeeded;
   DWORD dwMult;
   dwMult = 3; // RGB of color
   if (pti->dwMap & 0x03)
      dwMult += 3;   // if have spec/bump also need to account for combined map
   if (pti->dwMap & 0x01)
      dwMult += 2;   // specularity...
   if (pti->dwMap & 0x02)
      dwMult += 2;   // bump...
   if (pti->dwMap & 0x04)
      dwMult += 1;   // transparency... 1 byte
   if (pti->dwMap & 0x08)
      dwMult += 3;  // glow - wrap as bytes

   dwNeeded = sizeof(ALGOTEXTCACHE) + pImage->Width() * pImage->Height() * dwMult + 256;
      // BUGFIX - Was *17, changed to *dwMult
   if (!mem.Required (dwNeeded))
      return FALSE;  // error

   PALGOTEXTCACHE p;
   p = (PALGOTEXTCACHE) mem.p;
   // p->dwLastUsed = TodayFast();
   p->dwX = pImage->Width();
   p->dwY = pImage->Height();
   p->gCode = *pgCode;
   p->gSub = *pgSub;
   p->m_dwID = pMaterial->m_dwID;
   p->m_dwMaterialType = pMaterial->m_dwMaterialType;
   p->m_fThickness = 0;//pMaterial->m_fThickness;
   p->m_wTranslucent = pMaterial->m_wTranslucent;
   p->m_fNoShadows = pMaterial->m_fNoShadows;
   p->m_wSpecExponent = pMaterial->m_wSpecExponent;
   p->m_wSpecPlastic = pMaterial->m_wSpecPlastic;
   p->m_wSpecReflect = pMaterial->m_wSpecReflect;
   p->m_wIndexOfRefract = pMaterial->m_wIndexOfRefract;
   p->m_wReflectAmount = pMaterial->m_wReflectAmount;
   p->m_wReflectAngle = pMaterial->m_wReflectAngle;
   p->m_wTransAngle = pMaterial->m_wTransAngle;
   p->m_wTransparency = pMaterial->m_wTransparency;
   p->ti = *pti;

   // fill in the maps
   PBYTE pbColorMap, pCur;
   DWORD dwColorMapSize;
   dwColorMapSize = (p->dwX * p->dwY * 3 + 3) & ~0x03;   // DWORD align
   pbColorMap = (PBYTE) (p+1);
   pCur = pbColorMap + dwColorMapSize;

   // color-only map
   PBYTE pbColorOnly;
   pbColorOnly = NULL;
   if (p->ti.dwMap & 0x03) {  // both specularity and bump
      pbColorOnly = pCur;
      pCur += dwColorMapSize; // since same size as above
   }

   // specularity map?
   PBYTE pbSpecMap;
   pbSpecMap = NULL;
   if (p->ti.dwMap & 0x01) {
      pbSpecMap = pCur;
      pCur += ((p->dwX * p->dwY * 2 + 3) & ~0x03); // DWORD align this;
   }

   // bump map?
   PSHORT pBumpMap;
   pBumpMap = NULL;
   if (p->ti.dwMap & 0x02) {
      pBumpMap = (PSHORT) pCur;
      pCur += (p->dwX * p->dwY * sizeof(short));
   }

   // transparency map?
   PBYTE pTrans;
   pTrans = NULL;
   if (p->ti.dwMap & 0x04) {
      pTrans = pCur;
      pCur += ((p->dwX * p->dwY + 3) & ~0x03); // DWORD align this
   }

   // glow map
   PBYTE pGlow;
   pGlow = NULL;
   if (p->ti.dwMap & 0x08) {
      pGlow = pCur;
      pCur += dwColorMapSize; // since same size as above
   }

   // transfer over
   PIMAGEPIXEL pip;
   DWORD i;
   if (pbColorOnly) {
      PBYTE pb;
      pb = pbColorOnly;
      pip = pImage->Pixel(0,0);
      for (i = 0; i < pImage->Width() * pImage->Height(); i++, pip++) {
         *(pb++) = UnGamma (pip->wRed);
         *(pb++) = UnGamma (pip->wGreen);
         *(pb++) = UnGamma (pip->wBlue);
      }
   }
   if (pbSpecMap) {
      PBYTE pb;
      pb = pbSpecMap;
      pip = pImage->Pixel(0,0);
      for (i = 0; i < pImage->Width() * pImage->Height(); i++, pip++) {
         *(pb++) = HIBYTE(LOWORD(pip->dwID));
         *(pb++) = UnGamma(HIWORD(pip->dwID));
      }
   }
   if (pBumpMap) {
      PSHORT ps;
      //int   iZ;
      ps = pBumpMap;
      pip = pImage->Pixel(0,0);
      for (i = 0; i < pImage->Width() * pImage->Height(); i++, pip++) {
         // BUGFIX - Changed pixel Z to float
         fp f = pip->fZ * 256;

         f = min(f, 0x7fff);
         f = max(f, -0x7fff);
         *(ps++) = (short)f;//iZ;
         //iZ = pip->iZ / 256;  // since less precsion
         //iZ = min(iZ, 0x7fff);
         //iZ = max(iZ, -0x7fff);
         //*(ps++) = (short)iZ;
      }
   }
   if (pTrans) {
      PBYTE pb;
      pb = pTrans;
      pip = pImage->Pixel(0,0);
      for (i = 0; i < pImage->Width() * pImage->Height(); i++, pip++)
         *(pb++) = LOBYTE(pip->dwIDPart);
   }
   if (pGlow) {   // if glow then Image2 must be valid
      PBYTE pb;
      pb = pGlow;
      pip = pImage2->Pixel(0,0);
      for (i = 0; i < pImage2->Width() * pImage2->Height(); i++, pip++) {
         *(pb++) = UnGamma (pip->wRed);
         *(pb++) = UnGamma (pip->wGreen);
         *(pb++) = UnGamma (pip->wBlue);
      }
   }

   // now apply bump map
   pImage->TGBumpMapApply ();
   if (pbColorMap) {
      PBYTE pb;
      pb = pbColorMap;
      pip = pImage->Pixel(0,0);
      for (i = 0; i < pImage->Width() * pImage->Height(); i++, pip++) {
         *(pb++) = UnGamma (pip->wRed);
         *(pb++) = UnGamma (pip->wGreen);
         *(pb++) = UnGamma (pip->wBlue);
      }
   }

   // add it to temporary list
   DWORD dwSize = (DWORD)((PBYTE) pCur - (PBYTE) p);
   galistALGOTEXTCACHE[dwRenderShard].Add (mem.p, dwSize);

   // save it to disk
   WCHAR szTemp[96];
   gapmfAlgoTexture[dwRenderShard]->Save (TextureAlgoCacheName(pgCode, pgSub, szTemp), mem.p, dwSize);

   return TRUE;
}


/****************************************************************************
TextureAlgoCache - Cache an algorithmic texture to the memory list
that will eventually be written out to disk.

inputs
   GUID        *pgCode - Texture code
   GUID        *pgSub - Texture sub
returns
   none
*/
void TextureAlgoCache (DWORD dwRenderShard, const GUID *pgCode, const GUID *pgSub)
{
   // remove it in case it's there
   TextureAlgoCacheRemove (dwRenderShard, pgCode, pgSub);

   // max cache size check
   TextureAlgoMaxCache(dwRenderShard, MAXTEXTCACHE);

   // find it
   PCMMLNode2 pNode;
   WCHAR szMajor[128], szMinor[128], szName[128];
   if (!LibraryTextures(dwRenderShard, FALSE)->ItemNameFromGUID (pgCode, pgSub, szMajor, szMinor, szName)) {
      if (!LibraryTextures(dwRenderShard, TRUE)->ItemNameFromGUID (pgCode, pgSub, szMajor, szMinor, szName))
         return;  // cant find
   }
   pNode = LibraryTextures(dwRenderShard, FALSE)->ItemGet (szMajor, szMinor, szName);
   if (!pNode)
      pNode = LibraryTextures(dwRenderShard, TRUE)->ItemGet (szMajor, szMinor, szName);
   if (!pNode)
      return;

   // else, create it
   PCTextCreatorSocket pCreate;
   pCreate = CreateTextureCreator (dwRenderShard, pgCode, 0, pNode, FALSE);
   if (!pCreate)
      return;  // cant create

   // draw this
   CImage Image, Image2;
   CMaterial Material;
   TEXTINFO ti;
   if (!pCreate->Render (&Image, &Image2, &Material, &ti)) {
      pCreate->Delete();
      return;  // cant create
   }


   TextureAlgoCache (dwRenderShard, pgCode, pgSub, &Image, &Image2, &Material, &ti);

   pCreate->Delete();
}


   
/****************************************************************************
TextureAlgoUnCache - Looks through the memory list of cached 2D algorithm textures.
If its not found then it tries to create it. If that fails then it returns false

inputs
   GUID        *pgCode - Texture code
   GUID        *pgSub - Texture sub

   DWORD       *pdwX - Filled with pixels across (can be null)
   DWORD       *pdwY - Filled with pixels down (can be null)
   PCMaterial  pMat - Filled with material info (can be null)
   PTEXTINFO   pInfo - Filled with texture inform (can be null)
   PBYTE       *ppbCombined - Filled with a pointer to the combined map. (includes specularity and bump), dwY x dwX x 3 bytes (RGB)
   PBYTE       *ppbColorOnly - Filled with pointer to color only map, dwY x dwX x 3 bytes (RGB)
                  Could be null if no color-only map
   PBYTE       *ppbSpec - Filled with pointer to specularyity map. dwY x dwX x (HIBYTE(wExponent), HIBYTE(wReflect))
                  Could be null of no specularity map
   PSHORT      *ppsBump  - Filled with pointer to bump map. dwY x dwX x short.
                  Could be NULL if no bump map
   PBYTE       *ppbTrans - Filled with pointer to transparency map. dwY x dwX x byte
                  Could be NULL if no transparency map
returns
   none
*/

BOOL TextureAlgoUnCache (DWORD dwRenderShard, const GUID *pgCode, const GUID *pgSub,
                         DWORD *pdwX, DWORD *pdwY, PCMaterial pMat, PTEXTINFO pInfo,
                         PBYTE *ppbCombined, PBYTE *ppbColorOnly, PBYTE *ppbSpec,
                         PSHORT *ppsBump, PBYTE *ppbTrans, PBYTE *ppbGlow)
{
   PALGOTEXTCACHE p;
   DWORD i, dwAttempts;

   for (dwAttempts = 0; dwAttempts < 2; dwAttempts++) {
      for (i = 0; i < galistALGOTEXTCACHE[dwRenderShard].Num(); i++) {
         p = (PALGOTEXTCACHE) galistALGOTEXTCACHE[dwRenderShard].Get (i);

         if (IsEqualGUID (*pgCode, p->gCode) && IsEqualGUID (*pgSub, p->gSub))
            break;
      }
      if (i < galistALGOTEXTCACHE[dwRenderShard].Num())
         break; //found it

      // if it's the second attempt then fail here
      if (dwAttempts)
         return FALSE;

      // BUGFIX - Once in awhile set the "dont update access time" to FALSE
      // so that keep track of which thumbnails are most recently used
      BOOL fOldInfo = gapmfAlgoTexture[dwRenderShard]->m_fDontUpdateLastAccess;
      if ((GetTickCount()%0xf00) == 0)
         gapmfAlgoTexture[dwRenderShard]->m_fDontUpdateLastAccess = FALSE;

      // if couldnt' find, then try to load?
      __int64 iSize;
      WCHAR szTemp[96];
      PBYTE pData;
      pData = (PBYTE) gapmfAlgoTexture[dwRenderShard]->Load (TextureAlgoCacheName(pgCode, pgSub, szTemp), &iSize, FALSE);
      gapmfAlgoTexture[dwRenderShard]->m_fDontUpdateLastAccess = fOldInfo;

      if (pData) {
         // eliminate if too many
         TextureAlgoMaxCache (dwRenderShard, MAXTEXTCACHE);

         // add this
         galistALGOTEXTCACHE[dwRenderShard].Add (pData, (DWORD)iSize);

         // free memory
         MegaFileFree (pData);

         continue;   // so will find exact memory where located
      }

      // else, try to create
      TextureAlgoCache (dwRenderShard, pgCode, pgSub);
      // and loop to try again
   }

   // keep track of when last used so that if too many
   // textures can delete unusued ones
   //p->dwLastUsed = TodayFast();

   if (pdwX)
      *pdwX = p->dwX;
   if (pdwY)
      *pdwY = p->dwY;
   if (pMat) {
      pMat->m_dwID = p->m_dwID;
      pMat->m_dwMaterialType = p->m_dwMaterialType;
      //pMat->m_fThickness = 0;//p->m_fThickness;
      pMat->m_wTranslucent = p->m_wTranslucent;
      pMat->m_fNoShadows = p->m_fNoShadows;
      pMat->m_wSpecExponent = p->m_wSpecExponent;
      pMat->m_wSpecPlastic = p->m_wSpecPlastic;
      pMat->m_wSpecReflect = p->m_wSpecReflect;
      pMat->m_wTransparency = p->m_wTransparency;
      pMat->m_wIndexOfRefract = p->m_wIndexOfRefract;
      pMat->m_wReflectAmount = p->m_wReflectAmount;
      pMat->m_wReflectAngle = p->m_wReflectAngle;
      pMat->m_wTransAngle = p->m_wTransAngle;

      // BUGFIX - Transfer over glow
      pMat->m_fGlow = FALSE;
      pMat->m_wFill = 0;
   }
   if (pInfo)
      *pInfo = p->ti;

   // fill in the maps
   PBYTE pbColorMap, pCur;
   DWORD dwColorMapSize;
   dwColorMapSize = (p->dwX * p->dwY * 3 + 3) & ~0x03;   // DWORD align
   pbColorMap = (PBYTE) (p+1);
   pCur = pbColorMap + dwColorMapSize;

   // color-only map
   PBYTE pbColorOnly;
   pbColorOnly = NULL;
   if (p->ti.dwMap & 0x03) {  // both specularity and bump
      pbColorOnly = pCur;
      pCur += dwColorMapSize; // since same size as above
   }

   // specularity map?
   PBYTE pbSpecMap;
   pbSpecMap = NULL;
   if (p->ti.dwMap & 0x01) {
      pbSpecMap = pCur;
      pCur += ((p->dwX * p->dwY * 2 + 3) & ~0x03); // DWORD align this;
   }

   // bump map?
   PSHORT pBumpMap;
   pBumpMap = NULL;
   if (p->ti.dwMap & 0x02) {
      pBumpMap = (PSHORT) pCur;
      pCur += (p->dwX * p->dwY * sizeof(short));
   }

   // transparency map?
   PBYTE pTrans;
   pTrans = NULL;
   if (p->ti.dwMap & 0x04) {
      pTrans = pCur;
      pCur += ((p->dwX * p->dwY + 3) & ~0x03); // DWORD align this
   }

   // glow
   PBYTE pGlow;
   pGlow = NULL;
   if (p->ti.dwMap & 0x08) {
      pGlow = pCur;
      pCur += dwColorMapSize; // since same size as above
   }

   if (ppbCombined)
      *ppbCombined = pbColorMap;
   if (ppbColorOnly)
      *ppbColorOnly = pbColorOnly;
   if (ppbSpec)
      *ppbSpec = pbSpecMap;
   if (ppsBump)
      *ppsBump = pBumpMap;
   if (ppbTrans)
      *ppbTrans = pTrans;
   if (ppbGlow)
      *ppbGlow = pGlow;

   return TRUE;
}


typedef struct {
   DWORD             dwID;    // for getting the scheme
   PCObjectSocket    pos;     // object that modifying. Might be null
   PCWorldSocket           pWorld;  // world
   HBITMAP           hBit;    // bitmap for the texture
} OPPAGE, *POPPAGE;


// TEXTURECACHE - information about a cached texture
typedef struct {
   __int64                 iLastUsed;    // last used time, so know which ones to delete first
   PCTextureMapSocket      pTexture;      // texture, mapped
   DWORD                   dwCount;       // reference count. When 0, not used
} TEXTURECACHE, *PTEXTURECACHE;

static CListFixed galTEXTURECACHE[MAXRENDERSHARDS];   // texture cache
static __int64 gaiTextureTime[MAXRENDERSHARDS] = {0, 0, 0, 0};     // incremented every time
#define NUMLASTCACHE    (MAXRAYTHREAD*2)           // keep track of last 8 textres
                           // BUGFIX - So that keep track of extra textures if more threads
static PTEXTURECACHE gaaTEXTURECACHELast[MAXRENDERSHARDS][NUMLASTCACHE] = {
   NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
   NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
   NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
   NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL
};  // last texture map accessed
static QWORD         gaqwTextureCacheTimeStamp[MAXRENDERSHARDS] = {1,1,1,1};     // time stamp for last time modified and textures might be invalid
   // IMPORTANT - Change above if change MAXRENDERSHARDS define
static PCObjectSurface gaOSLastPaint[MAXRENDERSHARDS] = {NULL, NULL, NULL, NULL};  // so keep track of the last color painted
static CRITICAL_SECTION gacsTextureMapLast[MAXRENDERSHARDS];      // critical section to access texture map last, for multithread





/****************************************************************************
TextureAlgoTextureQuery - Given a texture GUID, this fills in a list
with the texture and sub-textures. Use this to figure out what textures need
to be cached along with a file.

inputs
   PCListFixed       plText - List of 2-GUIDs (major & minor) for the textures
                     that are already known. If the texture is already on here,
                     this returns. If not, the texture is added, and sub-textures
                     are also added.
   PCBTree           pTree - Also added to
   GUID              *pagThis - Pointer to an array of 2 guids. pagThis[0] = code, pagThis[1] = sub
returns
   BOOL - TRUE if success
*/
BOOL TextureAlgoTextureQuery (DWORD dwRenderShard, PCListFixed plText, PCBTree pTree, GUID *pagThis)
{
   WCHAR szTemp[sizeof(GUID)*4+2];
   MMLBinaryToString ((PBYTE)pagThis, sizeof(GUID)*2, szTemp);
   if (pTree->Find (szTemp))
      return TRUE;


   // as an optimization, if the guid isn't of a texture that references other
   // guids then just (essentially) call default API directly without creating
   if (
      !IsEqualGUID (pagThis[0], GTEXTURECODE_Faceomatic) &&
      !IsEqualGUID (pagThis[0], GTEXTURECODE_Mix) &&
      !IsEqualGUID (pagThis[0], GTEXTURECODE_LeafLitter) &&
      !IsEqualGUID (pagThis[0], GTEXTURECODE_Text)
      ) {
      // add itself
      plText->Add (pagThis);
      pTree->Add (szTemp, NULL, 0);

      return TRUE;
   }


   // find it
   PCMMLNode2 pNode;
   WCHAR szMajor[128], szMinor[128], szName[128];
   if (!LibraryTextures(dwRenderShard, FALSE)->ItemNameFromGUID (&pagThis[0], &pagThis[1], szMajor, szMinor, szName)) {
      if (!LibraryTextures(dwRenderShard, TRUE)->ItemNameFromGUID (&pagThis[0], &pagThis[1], szMajor, szMinor, szName))
         return FALSE;  // cant find
   }
   pNode = LibraryTextures(dwRenderShard, FALSE)->ItemGet (szMajor, szMinor, szName);
   if (!pNode)
      pNode = LibraryTextures(dwRenderShard, TRUE)->ItemGet (szMajor, szMinor, szName);
   if (!pNode)
      return FALSE;

   // else, create it
   PCTextCreatorSocket pCreate;
   pCreate = CreateTextureCreator (dwRenderShard, &pagThis[0], 0, pNode, FALSE);
   if (!pCreate)
      return FALSE;  // cant create

   // see about sub-textures, which will cause it to be added
   pCreate->TextureQuery (plText, pTree, pagThis);

   pCreate->Delete();

   return TRUE;
}



/****************************************************************************
TextureAlgoSubTextureNoRecurse - Given a texture GUID, this determines if the
texture recurses on itself... which is bad.

inputs
   PCListFixed       plText - List of 2-GUIDs (major & minor) for the textures
                     that are already known. If the texture is already on here,
                     this returns FALSE. If not, the texture is TEMPORARILY added, and sub-textures
                     are also TEMPORARILY added.
   GUID              *pagThis - Pointer to an array of 2 guids. pagThis[0] = code, pagThis[1] = sub
returns
   BOOL - TRUE if success. FALSE if recurses on itself
*/
BOOL TextureAlgoSubTextureNoRecurse (DWORD dwRenderShard, PCListFixed plText, GUID *pagThis)
{
   // look for itself on the list
   GUID *pag = (GUID*)plText->Get(0);
   DWORD i;
   for (i = 0; i < plText->Num(); i++, pag += 2)
      if (!memcmp (pag, pagThis, sizeof(GUID)*2))
         return FALSE;  // found itself


   // as an optimization, if the guid isn't of a texture that references other
   // guids then just (essentially) call default API directly without creating
   if (
      !IsEqualGUID (pagThis[0], GTEXTURECODE_Faceomatic) &&
      !IsEqualGUID (pagThis[0], GTEXTURECODE_Mix) &&
      !IsEqualGUID (pagThis[0], GTEXTURECODE_LeafLitter) &&
      !IsEqualGUID (pagThis[0], GTEXTURECODE_Text)
      )
      return TRUE;

   // find it
   PCMMLNode2 pNode;
   WCHAR szMajor[128], szMinor[128], szName[128];
   if (!LibraryTextures(dwRenderShard, FALSE)->ItemNameFromGUID (&pagThis[0], &pagThis[1], szMajor, szMinor, szName)) {
      if (!LibraryTextures(dwRenderShard, TRUE)->ItemNameFromGUID (&pagThis[0], &pagThis[1], szMajor, szMinor, szName))
         return TRUE;  // cant find, so no recurse
   }
   pNode = LibraryTextures(dwRenderShard, FALSE)->ItemGet (szMajor, szMinor, szName);
   if (!pNode)
      pNode = LibraryTextures(dwRenderShard, TRUE)->ItemGet (szMajor, szMinor, szName);
   if (!pNode)
      return TRUE; // cant create, so no recurse

   // else, create it
   PCTextCreatorSocket pCreate;
   pCreate = CreateTextureCreator (dwRenderShard, &pagThis[0], 0, pNode, FALSE);
   if (!pCreate)
      return TRUE;  // cant create, so no recruse

   // see about sub-textures, which will cause it to be added
   BOOL fRet = pCreate->SubTextureNoRecurse (plText, pagThis);

   pCreate->Delete();

   return fRet;
}




/**************************************************************************************
CSurfaceScheme::Constructor and destructor
*/
CSurfaceScheme::CSurfaceScheme (void)
{
   m_listPCObjectSurface.Init (sizeof(PCObjectSurface));
   m_fObjectSurfacesSorted = TRUE;
   m_pWorld = NULL;
}


CSurfaceScheme::~CSurfaceScheme (void)
{
   DWORD i;
   // free up the object surfaces
   for (i = 0; i < m_listPCObjectSurface.Num(); i++) {
      PCObjectSurface pos = *((PCObjectSurface*) m_listPCObjectSurface.Get(i));
      delete pos;
   }
}


/**************************************************************************************
CSurfaceScheme::Clear - Clears out all the surfaces
*/
void CSurfaceScheme::Clear (void)
{
   DWORD i;
   // free up the object surfaces
   for (i = 0; i < m_listPCObjectSurface.Num(); i++) {
      PCObjectSurface pos = *((PCObjectSurface*) m_listPCObjectSurface.Get(i));
      delete pos;
   }
   m_listPCObjectSurface.Clear();
}

/**************************************************************************************
CSurfaceScheme::MMLTo - Writes out the scheme to a MML node.

inputs
   none
returns
   PCMMLNode2 - This must be freed by the calling application
*/
PCMMLNode2 CSurfaceScheme::MMLTo (void)
{
   PCMMLNode2 pNode = new CMMLNode2;
   if (!pNode)
      return NULL;

   pNode->NameSet (L"SurfaceScheme");

   DWORD i;
   // write out the object surfaces
   for (i = 0; i < m_listPCObjectSurface.Num(); i++) {
      PCObjectSurface pos = *((PCObjectSurface*) m_listPCObjectSurface.Get(i));
      pNode->ContentAdd (pos->MMLTo());
   }
   return pNode;
}

/**************************************************************************************
CSurfaceScheme::MMLFrom - Initializes the surface scheme from the MML.

inputs
   PCMMLNode2   pNode - node to use. Came from CSurfaceScheme::MMLTo().
returns
   BOOL - TRUE if succeded.
*/
BOOL CSurfaceScheme::MMLFrom (PCMMLNode2 pNode)
{
   // read in the surfaces
   DWORD i;
   // free up the object surfaces
   for (i = 0; i < m_listPCObjectSurface.Num(); i++) {
      PCObjectSurface pos = *((PCObjectSurface*) m_listPCObjectSurface.Get(i));
      delete pos;
   }
   m_listPCObjectSurface.Clear();
   for (i = 0; ; i++) {
      PCMMLNode2 pSub;
      PWSTR psz;
      if (!pNode->ContentEnum(i, &psz, &pSub))
         break;
      if (!pSub)
         continue;
      psz = pSub->NameGet();
      if (!psz)
         continue;
      if (_wcsicmp (psz, ObjectSurface()))
         continue;

      // else, add it
      PCObjectSurface pos;
      pos = new CObjectSurface;
      if (!pos)
         break;
      pos->MMLFrom (pSub);
      m_listPCObjectSurface.Add (&pos);
   }
   m_fObjectSurfacesSorted = FALSE;
   return TRUE;
}


/**************************************************************************************
CSurfaceScheme::Clone - Clones the current surface scheme object

inputs
   none
returns
   CSurfaceScheme * - New object
*/
CSurfaceScheme *CSurfaceScheme::Clone (void)
{
   PCSurfaceScheme pCloneTo = new CSurfaceScheme;
   if (!pCloneTo)
      return NULL;

   pCloneTo->m_pWorld = m_pWorld;

   // clone the objects
   DWORD i;
   pCloneTo->m_listPCObjectSurface.Required (m_listPCObjectSurface.Num());
   for (i = 0; i < m_listPCObjectSurface.Num(); i++) {
      PCObjectSurface pos = *((PCObjectSurface*) m_listPCObjectSurface.Get(i));
      PCObjectSurface pNew = pos->Clone();
      if (pNew)
         pCloneTo->m_listPCObjectSurface.Add (&pNew);
   }
   pCloneTo->m_fObjectSurfacesSorted = m_fObjectSurfacesSorted;

   return pCloneTo;
}

/**************************************************************************************
CSurfaceScheme::SurfaceGet - Returns a copy of a CObjectSurface C++ object so that
the application can query the surface information.

inputs
   PWSTR          pszScheme - scheme name. Case insensative.
   PCObjectSurface pDefault - If pszScheme is not found and pDefault is non null,
                  then pDefault will be copied and used as the particular
                  scheme from now on. Default of NULL.
   BOOL           fNoClone - If TRUE, the DON'T clone the surface, which means
                  it won't be around for long. Normally pass in FALSE unless
                  doing a very quick query
returns
   PCObjectSurface - Surface information. This must be freed by the caller
*/
PCObjectSurface CSurfaceScheme::SurfaceGet (PWSTR pszScheme, PCObjectSurface pDefault, BOOL fNoClone)
{
   // must have a name
   if (!pszScheme[0])
      return NULL;

   // find it
   PCObjectSurface pFind;
   pFind = FindSurface(pszScheme);
   if (pFind)
      return fNoClone ? pFind : pFind->Clone();

   // if can't find then add the default
   if (!pDefault)
      return NULL;
   if (_wcsicmp(pDefault->m_szScheme, pszScheme))
      return NULL;   // names should be the same

   // add it
   pFind = pDefault->Clone(); // NOTE: Always cloning this because added to list of surfaces
   if (!pFind)
      return NULL;
   m_listPCObjectSurface.Add (&pFind);
   m_fObjectSurfacesSorted = FALSE;
   // dont bother notifying world object since nothing else asked for this
   // so adding this wont change the world as drwan up until now


   // clone and return that
   return fNoClone ? pFind : pDefault->Clone();
}


/**************************************************************************************
CSurfaceScheme::SurfaceExists - Returns TRUE if the surface exists.

inputs
   PWSTR          pszScheme - scheme name. Case insensative.
returns
   BOOL - TRUE if exists and is valid
*/
BOOL CSurfaceScheme::SurfaceExists (PWSTR pszScheme)
{
   // must have a name
   if (!pszScheme[0])
      return FALSE;

   // find it
   PCObjectSurface pFind;
   pFind = FindSurface(pszScheme);
   return pFind ? TRUE : FALSE;
}

/**************************************************************************************
CSurfaceScheme::SurfaceSet - Sets the surface, either adding it over overwriting
the existing one.

inputs
   PCObjectSurface pSurface - Surface to replace/add
returns
   BOOL - TRUE if OK
*/
BOOL CSurfaceScheme::SurfaceSet (PCObjectSurface pSurface)
{
   if (!pSurface->m_szScheme[0])
      return FALSE;

   // find it
   PCObjectSurface pFind;
   pFind = FindSurface(pSurface->m_szScheme);
   if (pFind) {
      memcpy (pFind, pSurface, sizeof(*pFind));

      // alert the world of a change
      if (m_pWorld)
         m_pWorld->NotifySockets (WORLDC_SURFACECHANGED, NULL);

      return TRUE;
   }

   // else, add it
   pFind = pSurface->Clone();
   if (!pFind)
      return FALSE;
   m_listPCObjectSurface.Add (&pFind);
   m_fObjectSurfacesSorted = FALSE;


   // alert the world of a change
   if (m_pWorld)
      m_pWorld->NotifySockets (WORLDC_SURFACECHANGED, NULL);

   return TRUE;
}


/**************************************************************************************
CSurfaceScheme::SurfaceRemove - Removes the surface from the list.

inputs
   PWSTR       pszScheme - surface scheme name
returns
   BOOL - TRUE if removed, FALSE if can't find
*/
BOOL CSurfaceScheme::SurfaceRemove (PWSTR pszScheme)
{
   DWORD dwIndex;
   PCObjectSurface pFind;
   dwIndex = FindIndex (pszScheme);
   if (dwIndex == (DWORD)-1)
      return FALSE;
   pFind = FindSurface (pszScheme);
   if (!pFind)
      return FALSE;

   delete pFind;
   m_listPCObjectSurface.Remove (dwIndex);

   return TRUE;
}


/**************************************************************************************
CSurfaceScheme::SurfaceEnum - Given an index into a the list of surfaces, this
returns the surface object.

inputs
   DWORD       dwIndex - Index, 0..# surfaces-1. If more than the number of surfaces
                  will return nULL
returns
   PCObjectSurface - Surface. THis must be freed by the calling application.
*/
PCObjectSurface CSurfaceScheme::SurfaceEnum (DWORD dwIndex)
{
   // call find just to sort
   FindIndex (L"");

   PCObjectSurface *pps;
   pps = (PCObjectSurface*) m_listPCObjectSurface.Get(dwIndex);
   if (!pps)
      return NULL;

   return (*pps)->Clone();
}

/**************************************************************************************
CSurfaceScheme::WorldSet - Should be called right away when it's added to the world
(by the CWorldSocket object) to tell the object what world it's in. That way the object can
notify the world of any changes to itself.

inputs
   CWorldSocket      *pWorld - world
*/
void CSurfaceScheme::WorldSet (CWorldSocket *pWorld)
{
   m_pWorld = pWorld;
}


/*************************************************************************************
CSurfaceScheme::FindIndex - Internal function that sorts the object surfaces
if they're not alreay sorted, and then does a bsearch.

inputs
   DWORD       dwID - ID
returns
   DWORD - Index in m_listpCObjectSurface, or -1 if cant find
*/
static int _cdecl BCompare (const void *elem1, const void *elem2)
{
   PCObjectSurface *pdw1, *pdw2;
   pdw1 = (PCObjectSurface*) elem1;
   pdw2 = (PCObjectSurface*) elem2;

   return _wcsicmp((*pdw1)->m_szScheme, (*pdw2)->m_szScheme);
}

DWORD CSurfaceScheme::FindIndex (PWSTR pszScheme)
{
   if (!m_fObjectSurfacesSorted) {
      m_fObjectSurfacesSorted = TRUE;
      qsort (m_listPCObjectSurface.Get(0), m_listPCObjectSurface.Num(),
         sizeof(PCObjectSurface), BCompare);
   }

   PCObjectSurface *ppos;
   CObjectSurface os;
   wcscpy (os.m_szScheme, pszScheme);
   PCObjectSurface pos;
   pos = &os;
   ppos = (PCObjectSurface*) bsearch (&pos, m_listPCObjectSurface.Get(0), m_listPCObjectSurface.Num(),
         sizeof(PCObjectSurface), BCompare);
   if (!ppos)
      return (DWORD)-1;

   return (DWORD)(size_t)((PBYTE) ppos - (PBYTE) (m_listPCObjectSurface.Get(0))) / sizeof(PCObjectSurface);
}

/*************************************************************************************
CSurfaceScheme::FindSurface - Function called find a surface

inputs
   DWORD       dwID - ID
returns
   PCObjectSurface - Object surface object, or NULL if error.
*/
PCObjectSurface CSurfaceScheme::FindSurface (PWSTR pszScheme)
{
   DWORD dwIndex = FindIndex (pszScheme);
   if (dwIndex == (DWORD)-1)
      return NULL;
   return *((PCObjectSurface*) m_listPCObjectSurface.Get(dwIndex));
}



#if 0 // DEAD code
/****************************************************************************
ObjPaintPage
*/
BOOL ObjPaintPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   POPPAGE pop = (POPPAGE) pPage->m_pUserData;
   static WCHAR sTextureTemp[16];

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         // what's the current surface
         PCObjectSurface psCur;
         psCur = pop->pos->SurfaceGet (pop->dwID);

         // fill in the list box
         DWORD i;
         PCSurfaceSchemeSocket pss = pop->pWorld->m_pSurfaceScheme;
         CMem mem;
         MemZero (&mem);
         DWORD dwSel = 0;

         for (i = 0; ; i++) {
            PCObjectSurface ps;
            ps = pss->SurfaceEnum (i);
            if (!ps)
               break;

            // do we set this to the selection
            if (psCur && !_wcsicmp(ps->m_szScheme, psCur->m_szScheme))
               dwSel = i+1;

            // make a string from this
            MemCat (&mem, L"<elem name=");
            MemCat (&mem, (int) i);
            MemCat (&mem, L"><font color=#004000><bold>");
            MemCatSanitize (&mem, ps->m_szScheme);
            MemCat (&mem, L"</bold></font><small><italic> (");
            if (ps->m_fUseTextureMap)
               MemCat (&mem, L"Texture");
            else
               MemCat (&mem, L"Solid color");
            MemCat (&mem, L")</italic></small></elem>");

            delete ps;
         }
         if (psCur)
            delete psCur;

         ESCMLISTBOXADD lba;
         memset (&lba, 0, sizeof(lba));
         lba.dwInsertBefore = -1;
         lba.pszMML = (PWSTR) mem.p;
         
         PCEscControl pControl;
         pControl = pPage->ControlFind (L"schemes");
         if (pControl) {
            pControl->Message (ESCM_LISTBOXADD, &lba);

            // set the selection
            pControl->AttribSetInt (CurSel(), (int) dwSel);
         }

         // Based on the selection set values below like radio buttons and color
         pPage->Message (ESCM_USER+82);
      }
      break;

   case ESCM_USER+82:   // set values from down below based on current settings
      {
         // get the info
         PCObjectSurface ps;
         ps = pop->pos->SurfaceGet(pop->dwID);
         if (!ps)
            return FALSE;
         if (ps->m_szScheme[0]) {
            PCObjectSurface p2;
            p2 = pop->pWorld->m_pSurfaceScheme->SurfaceGet (ps->m_szScheme, ps);
            if (p2) {
               delete ps;
               ps = p2;
            }
         }

         // set the color in the status control
         FillStatusColor (pPage, L"csolid", ps->m_cColor);

         // check the radio button
         PCEscControl pControl;
         pControl = pPage->ControlFind (L"texture");
         if (pControl)
            pControl->AttribSetBOOL (Checked(), ps->m_fUseTextureMap);
         pControl = pPage->ControlFind (L"solidcolor");
         if (pControl)
            pControl->AttribSetBOOL (Checked(), !ps->m_fUseTextureMap);

         // set the transparency
         ComboBoxSet (pPage, L"material", ps->m_Material.m_dwID);
         pControl = pPage->ControlFind (L"editmaterial");
         if (pControl)
            pControl->Enable (ps->m_Material.m_dwID ? FALSE : TRUE);

         // Show the texture in the little display on the right
         PCTextureMapSocket pMap;   // BUGFIX - Was PCTextureMap
         pMap = TextureCreate (&ps->m_gTextureCode, &ps->m_gTextureSub);
         if (pMap) {
            CImage image;
            image.Init (100, 100, 0);
            pMap->TextureModsSet (&ps->m_TextureMods);

            // redo the bitmap
            TEXTUREPOINT atp[4];
            memset (&atp, 0, sizeof(atp));
            atp[1].h = 1;
            atp[3].h = 1;
            atp[3].v = 1;
            atp[2].v = 1;
            // multiply by all the points
            DWORD i;
            for (i = 0; i < 4; i++) {
               TextureMatrixMultiply (ps->m_afTextureMatrix, &atp[i]);
            }

            pMap->FillImage (&image, &atp[0], &atp[1], &atp[2], &atp[3]);
            HDC hDC;
            hDC = GetDC (pPage->m_pWindow->m_hWnd);
            if (pop->hBit)
               DeleteObject (pop->hBit);
            pop->hBit = image.ToBitmap (hDC);
            ReleaseDC (pPage->m_pWindow->m_hWnd, hDC);

            PCEscControl pControl = pPage->ControlFind (L"image");

            WCHAR szTemp[32];
            swprintf (szTemp, L"%x", (int) pop->hBit);
            pControl->AttribSet (L"hbitmap", szTemp);
            pMap->Delete();   // BUGFIX - Was delete pMap
         }

         delete ps;
      }
      break;


   case ESCN_COMBOBOXSELCHANGE:
      {
         PESCNCOMBOBOXSELCHANGE p = (PESCNCOMBOBOXSELCHANGE) pParam;
         if (!p->pControl->m_pszName)
            break;

         // only care about transparency
         if (_wcsicmp(p->pControl->m_pszName, L"material"))
            break;

         PCObjectSurface ps;
         BOOL fCustom;
         fCustom = TRUE;
         ps = pop->pos->SurfaceGet(pop->dwID);
         if (!ps)
            break;
         if (ps->m_szScheme[0]) {
            PCObjectSurface p2;
            p2 = pop->pWorld->m_pSurfaceScheme->SurfaceGet (ps->m_szScheme, ps);
            if (p2) {
               fCustom = FALSE;
               delete ps;
               ps = p2;
            }
         }


         DWORD dwVal;
         dwVal = p->pszName ? _wtoi(p->pszName) : 0;
         if (dwVal != ps->m_Material.m_dwID) {
            if (dwVal)
               ps->m_Material.InitFromID (dwVal);
            else
               ps->m_Material.m_dwID = MATERIAL_CUSTOM;

            // write it out
            if (fCustom)
               pop->pos->SurfaceSet (ps);
            else {
               pop->pWorld->SurfaceAboutToChange (ps->m_szScheme);
               pop->pWorld->m_pSurfaceScheme->SurfaceSet (ps);
               pop->pWorld->SurfaceChanged (ps->m_szScheme);
            }

            // eanble/disable button to edit
            PCEscControl pControl;
            pControl = pPage->ControlFind (L"editmaterial");
            if (pControl)
               pControl->Enable (ps->m_Material.m_dwID ? FALSE : TRUE);
         }

         delete ps;
      }
      break;

   case ESCM_LINK:
      {
         PESCMLINK p = (PESCMLINK)pParam;

         if (!p->psz)
            break;
#if 0
         if (!_wcsicmp(p->psz, L"changecolor")) {
            // user requested to change the color
            PCObjectSurface ps;
            BOOL fCustom;
            fCustom = TRUE;
            ps = pop->pos->SurfaceGet(pop->dwID);
            if (!ps)
               return TRUE;
            if (ps->m_szScheme[0]) {
               PCObjectSurface p2;
               p2 = pop->pWorld->m_pSurfaceScheme->SurfaceGet (ps->m_szScheme, ps);
               if (p2) {
                  fCustom = FALSE;
                  delete ps;
                  ps = p2;
               }
            }

            // color dialog
            COLORREF c;
            c = AskColor (pPage->m_pWindow->m_hWnd, ps->m_cColor, NULL, NULL);
            if (c != ps->m_cColor) {
               // changed the color
               ps->m_cColor = c;
               ps->m_fUseTextureMap = FALSE; // switch to this mode

               // write it out
               if (fCustom)
                  pop->pos->SurfaceSet (ps);
               else {
                  pop->pWorld->SurfaceAboutToChange (ps->m_szScheme);
                  pop->pWorld->m_pSurfaceScheme->SurfaceSet (ps);
                  pop->pWorld->SurfaceChanged (ps->m_szScheme);
               }
            }

            delete ps;

            // refresh the display
            pPage->Message (ESCM_USER+82);
            return TRUE;
         }
#endif         
         if (!_wcsicmp(p->psz, L"changetexture") || !_wcsicmp(p->psz, L"changecolor")) {
            // update the texture
            // user requested to change the color
            PCObjectSurface ps;
            BOOL fCustom;
            fCustom = TRUE;
            ps = pop->pos->SurfaceGet(pop->dwID);
            if (!ps)
               break;
            if (ps->m_szScheme[0]) {
               PCObjectSurface p2;
               p2 = pop->pWorld->m_pSurfaceScheme->SurfaceGet (ps->m_szScheme, ps);
               if (p2) {
                  fCustom = FALSE;
                  delete ps;
                  ps = p2;
               }
            }

            // use the texture map
            //ps->m_fUseTextureMap = TRUE;

            // see how it goes
            if (TextureSelDialog(pPage->m_pWindow->m_hWnd, ps, pop->pWorld)) {
               // write it out
               if (fCustom)
                  pop->pos->SurfaceSet (ps);
               else {
                  pop->pWorld->SurfaceAboutToChange (ps->m_szScheme);
                  pop->pWorld->m_pSurfaceScheme->SurfaceSet (ps);
                  pop->pWorld->SurfaceChanged (ps->m_szScheme);
               }
            }

            delete ps;

            // refresh the display
            pPage->Message (ESCM_USER+82);
            return TRUE;
         }

      }
      break;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;

         if (!p->pControl || !p->pControl->m_pszName)
            break;

         if (!_wcsicmp(p->pControl->m_pszName, L"texture") ||
            !_wcsicmp(p->pControl->m_pszName, L"solidcolor") ||
            !_wcsicmp(p->pControl->m_pszName, L"editmaterial") ) {
            // update the texture
            // user requested to change the color
            PCObjectSurface ps;
            BOOL fCustom;
            fCustom = TRUE;
            ps = pop->pos->SurfaceGet(pop->dwID);
            if (!ps)
               break;
            if (ps->m_szScheme[0]) {
               PCObjectSurface p2;
               p2 = pop->pWorld->m_pSurfaceScheme->SurfaceGet (ps->m_szScheme, ps);
               if (p2) {
                  fCustom = FALSE;
                  delete ps;
                  ps = p2;
               }
            }

            BOOL  fWriteOut;
            fWriteOut = FALSE;
            if (!_wcsicmp(p->pControl->m_pszName, L"editmaterial")) {
               // pressed the material custom button
               if (ps->m_Material.Dialog(pPage->m_pWindow->m_hWnd))
                  fWriteOut = TRUE;
            }
            else {   // pressed the texture or color change button
               PCEscControl pControl;
               pControl = pPage->ControlFind (L"texture");
               BOOL f;
               f = FALSE;
               if (pControl)
                  f = pControl->AttribGetBOOL (Checked());

               if (f != ps->m_fUseTextureMap) {
                  // changed the color
                  ps->m_fUseTextureMap = f;
                  fWriteOut = TRUE;

               }
            }

            if (fWriteOut) {
               // write it out
               if (fCustom)
                  pop->pos->SurfaceSet (ps);
               else {
                  pop->pWorld->SurfaceAboutToChange (ps->m_szScheme);
                  pop->pWorld->m_pSurfaceScheme->SurfaceSet (ps);
                  pop->pWorld->SurfaceChanged (ps->m_szScheme);
               }
            }

            delete ps;

            break;
         }
      }
      break;

   case ESCN_LISTBOXSELCHANGE:
      {
         PESCNLISTBOXSELCHANGE p = (PESCNLISTBOXSELCHANGE) pParam;
         if (!p->pControl->m_pszName || _wcsicmp(p->pControl->m_pszName, L"schemes"))
            break;

         // get the value and convert that to a string
         WCHAR    szScheme[64];
         szScheme[0] = 0;
         PCObjectSurface ps;
         if (p->dwCurSel) {
            ps = pop->pWorld->m_pSurfaceScheme->SurfaceEnum (p->dwCurSel-1);
            if (ps) {
               wcscpy (szScheme, ps->m_szScheme);
               delete ps;
            }
         }

         // get the current scheme of the selection
         ps = pop->pos->SurfaceGet(pop->dwID);
         if (!ps)
            return FALSE;
         if (!_wcsicmp(ps->m_szScheme, szScheme)) {
            // they're the same so no change
            delete ps;
            return TRUE;
         }

         // else, it's changed, so updata
         wcscpy (ps->m_szScheme, szScheme);
         pop->pos->SurfaceSet (ps);
         delete ps;

         // update the display
         pPage->Message (ESCM_USER+82);
         return TRUE;
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Change color or texture";
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"HBITMAP")) {
            swprintf (sTextureTemp, L"%x", (int) pop->hBit);
            p->pszSubString = sTextureTemp;
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}


/****************************************************************************
ObjPaintAddSchemePage
*/
BOOL ObjPaintAddSchemePage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   POPPAGE pop = (POPPAGE) pPage->m_pUserData;

   switch (dwMessage) {
   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;

         if (!p->pControl || !p->pControl->m_pszName)
            break;

         if (!_wcsicmp(p->pControl->m_pszName, L"add")) {
            // make sure it doesn't already exit
            WCHAR szTemp[64];
            PCEscControl pControl;
            pControl = pPage->ControlFind (L"scheme");
            szTemp[0] = 0;
            DWORD dwNeeded;
            pControl->AttribGet (Text(), szTemp, sizeof(szTemp), &dwNeeded);
            if (!szTemp[0]) {
               pPage->MBWarning (L"Please type in a name first.");
               return TRUE;
            }

            // see if it's a duplicate
            PCObjectSurface ps;
            ps = pop->pWorld->m_pSurfaceScheme->SurfaceGet (szTemp);
            if (ps) {
               delete ps;
               pPage->MBWarning (L"The scheme name already exists. Please type in a new one.");
               return TRUE;
            }

            // base it on the object's current coloration
            ps = pop->pos->SurfaceGet(pop->dwID);
            if (!ps)
               return TRUE;
            if (ps->m_szScheme[0]) {
               PCObjectSurface p2;
               p2 = pop->pWorld->m_pSurfaceScheme->SurfaceGet (ps->m_szScheme, ps);
               if (p2) {
                  delete ps;
                  ps = p2;
               }
            }

            wcscpy (ps->m_szScheme, szTemp);

            // add it
            pop->pWorld->m_pSurfaceScheme->SurfaceSet (ps);

            // set the object's surface too
            pop->pos->SurfaceSet (ps);

            delete ps;

            // exit and go back
            pPage->Exit (Back());
            return TRUE;
         }
      }
      break;


   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Add a color/texture scheme";
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}



/****************************************************************************
ObjPaintHelpPage
*/
BOOL ObjPaintHelpPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   POPPAGE pop = (POPPAGE) pPage->m_pUserData;

   switch (dwMessage) {
   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Help about color/texture schemes";
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}
#endif // 0

/****************************************************************************
ObjPaintDialog - This brings up the UI for changing an object's color.

inputs
   DWORD          dwID - surface ID
   PCObjectSocket pos - object socket
   PCHouseView    pView - view to use.
   BOOL           fNoUI - If TRUE, then don't bring up any UI. Instead, use the
                     same texture as the last one.
returns
   none
*/
void ObjPaintDialog (DWORD dwRenderShard, DWORD dwID, PCObjectSocket pos, PCHouseView pView, BOOL fNoUI)
{
   if (fNoUI && gaOSLastPaint[dwRenderShard]) {
      gaOSLastPaint[dwRenderShard]->m_dwID = dwID;
      pos->SurfaceSet (gaOSLastPaint[dwRenderShard]);
   }
   else {
      PCObjectSurface psCur;
      psCur = pos->SurfaceGet (dwID);
      if (!psCur)
         return;

      if (TextureSelDialog (dwRenderShard, pView->m_hWnd, psCur, pView->m_pWorld))
         pos->SurfaceSet (psCur);

      // keep this around
      if (gaOSLastPaint[dwRenderShard])
         delete gaOSLastPaint[dwRenderShard];
      gaOSLastPaint[dwRenderShard] = psCur;

#if 0 // DEAD code
      CEscWindow cWindow;
      RECT r;
      DialogBoxLocation (pView->m_hWnd, &r);

      cWindow.Init (ghInstance, pView->m_hWnd, EWS_FIXEDSIZE, &r);
      PWSTR pszRet;

      // set up the info
      OPPAGE op;
      op.dwID = dwID;
      op.pos = pos;
      op.pWorld = pView->m_pWorld;
      op.hBit = NULL;

      CImage image;
      image.Init (100, 100, RGB(0xff,0xff,0xff));
      HDC hDC;
      hDC = GetDC (pView->m_hWnd);
      op.hBit = image.ToBitmap(hDC);
      ReleaseDC (pView->m_hWnd, hDC);

   firstpage:
      pszRet = cWindow.PageDialog (ghInstance, IDR_MMLOBJPAINT, ObjPaintPage, &op);
      if (!pszRet)
         goto freeup;

      if (!_wcsicmp(pszRet, L"addscheme")) {
         pszRet = cWindow.PageDialog (ghInstance, IDR_MMLOBJPAINTADDSCHEME, ObjPaintAddSchemePage, &op);
         if (pszRet && !_wcsicmp(pszRet, Back()))
            goto firstpage;
         goto freeup;
      }
      if (!_wcsicmp(pszRet, L"help")) {
         pszRet = cWindow.PageDialog (ghInstance, IDR_MMLOBJPAINTHELP, ObjPaintHelpPage, &op);
         if (pszRet && !_wcsicmp(pszRet, Back()))
            goto firstpage;
         goto freeup;
      }
      else  // probably pressed back
         goto freeup;  // exit

   freeup:
      if (op.hBit)
         DeleteObject (op.hBit);

      // keep the last one stored away
      PCObjectSurface ps;
      ps = pos->SurfaceGet (dwID);
      if (ps) {
         if (gaOSLastPaint[dwRenderShard])
            delete gaOSLastPaint[dwRenderShard];
         gaOSLastPaint[dwRenderShard] = ps;
      }
#endif // 0
   }
}


/*****************************************************************
TextureMatrixMultiply2D - Multiply a 2x2 matrix with a point

inputs
   fp   pm[2][2] - matrix
   PTEXTUREPOINT pt - point
returns
   none
*/
__inline void TextureMatrixMultiply2D (fp pm[2][2], PTEXTUREPOINT pt)
{
   TEXTUREPOINT tp;
   tp = *pt;
   pt->h = pm[0][0] * tp.h + pm[1][0] * tp.v;
   pt->v = pm[0][1] * tp.h + pm[1][1] * tp.v;
}

/*****************************************************************
TextureMatrixMultiply - Multiply a 2x2 matrix with a point and a 4x4 XYZ matrix

inputs
   fp          pm[2][2] - matrix for HV transform
   PCMatrix    pMatrixXYZ - Matric to do XYZ transform
   PTEXTPOINT5 pt - point
returns
   none
*/
void TextureMatrixMultiply (fp pm[2][2], PCMatrix pMatrixXYZ, PTEXTPOINT5 pt)
{
   // 2d multiply
   TEXTUREPOINT tp;
   tp.h = pt->hv[0];
   tp.v = pt->hv[1];
   TextureMatrixMultiply2D (pm, &tp);
   pt->hv[0] = tp.h;
   pt->hv[1] = tp.v;

   // 3d multiply
   if (pMatrixXYZ) {
      CPoint p;
      p.p[0] = pt->xyz[0];
      p.p[1] = pt->xyz[1];
      p.p[2] = pt->xyz[2];
      p.p[3] = 1;
      p.MultiplyLeft (pMatrixXYZ);
      pt->xyz[0] = p.p[0];
      pt->xyz[1] = p.p[1];
      pt->xyz[2] = p.p[2];
   }
}

/*******************************************************************
TextureMatrixRotate - Create a rotation and scale matrix.

inputs
   fp   *pm[2][2] - Pointer to the matrix
   fp   rot - rotation, in radians
   fp   fScaleH,fScaleV - Horizontal and vertical scaling, BEFORE the rotation
returns
   none
*/

void TextureMatrixRotate (fp pm[2][2], fp rot, fp fScaleH, fp fScaleV)
{
   fp   CosZ, SinZ;

   CosZ = cos(-rot);
   SinZ = sin(-rot);

   pm[0][0] = CosZ / fScaleH;
   pm[1][0] = -SinZ / fScaleH;

   pm[0][1] = SinZ / fScaleV;
   pm[1][1] = CosZ / fScaleV;


}


/**************************************************************************************
CTextureMap::Constructor and destrucgtor
*/
CTextureMap::CTextureMap (void)
{
   memset (m_apImageOrig, 0, sizeof(m_apImageOrig));
   memset (m_apImageColorized, 0, sizeof(m_apImageColorized));
   DWORD i;
   for (i = 0; i < NUMTMIMAGE; i++)
      m_alistImageDown[i].Init (sizeof(PTMIMAGE));
   memset (&m_gCode, 0 ,sizeof(m_gCode));
   memset (&m_gSub, 0, sizeof(m_gSub));
   m_fDefH = m_fDefV = 1;
   m_fDontDownsample = FALSE;
   m_dwX = m_dwY = 0;

   memset (&m_TextureMods, 0, sizeof(m_TextureMods));
   m_TextureMods.cTint = RGB(0xff,0xff,0xff);
   m_TextureMods.wBrightness = 0x1000;
   m_TextureMods.wContrast = 0x1000;
   // m_TextureMods.wFiller = 0;
   m_TextureMods.wSaturation = 0x1000;
   memset (m_awColorAverage, 0, sizeof(m_awColorAverage));
   for (i = 0; i < NUMTMIMAGE; i++)
      m_acColorAverage[i] = -1;

   m_dwForceCache = 0;
   //m_TextureMods.wHue = 0;
}

CTextureMap::~CTextureMap (void)
{
   FreeAllImages (FALSE);
}

/**************************************************************************************
CTextureMap::EscMultiThreadedCallback - Callback that does mutlithreaded downsampling
of textures
*/
void CTextureMap::EscMultiThreadedCallback (PVOID pParams, DWORD dwParamSize,
      DWORD dwThread)
{
   PEMTINFO pei = (PEMTINFO) pParams;
   DWORD dwVer = pei->dwVer;

   if (pei->dwPass == 0) { // colorize image
      DWORD x,y, i;
      for (y = pei->dwStart; y < pei->dwStop; y++) for (x = 0; x < m_apImageColorized[dwVer]->dwX; x++) {
         // NOTE: Specifically not allowing the colorization of the glow
         PTMCOLOR pOrig = TMImagePixelTMCOLOR (m_apImageOrig[dwVer], (int) x, (int) y);
         PTMCOLOR pNew = TMImagePixelTMCOLOR (m_apImageColorized[dwVer], (int) x, (int) y);

         // convert to doubles
         float   afColor[3];
         afColor[0] = Gamma(GetRValue(pOrig->cRGB));
         afColor[1] = Gamma(GetGValue(pOrig->cRGB));
         afColor[2] = Gamma(GetBValue(pOrig->cRGB));

         ApplyTextureMods (&m_TextureMods, afColor);

         // convert from doubles
         for (i = 0; i < 3; i++) {
            afColor[i] = min(0xffff, afColor[i]);
            afColor[i] = max(0, afColor[i]);
         }

         pNew->cRGB = RGB(UnGamma((WORD) afColor[0]), UnGamma((WORD) afColor[1]), UnGamma((WORD) afColor[2]));
      }
   }
   else if (pei->dwPass == 1) { // if !m_fDontDownsample
      PTMIMAGE pImage = pei->pImage;
      DWORD dwNewX = pei->dwNewX;
      DWORD dwNewY = pei->dwNewY;
      DWORD dwType = pei->dwType;
      DWORD dwTakeFrom = pei->dwTakeFrom;
      DWORD dwFilter = pei->dwFilter;

      // loop
      DWORD x, y;
      PTMCOLOR pt;
      for (y = pei->dwStart; y < pei->dwStop; y++) for (x = 0; x < dwNewX; x++) {
         // where does it go in the new one
         if (dwType == TMIMAGETYPE_TMCOLOR)
            pt = TMImagePixelTMCOLOR (pImage, (int) x, (int) y);
         else if (dwType == TMIMAGETYPE_TMTRANS)
            pt = (PTMCOLOR) TMImagePixelTMTRANS (pImage, (int) x, (int) y);
         else  // tmglow
            pt = (PTMCOLOR) TMImagePixelTMGLOW (pImage, (int) x, (int) y);

         // get the pixel from the origianl
         FilteredPixelFromOrig (dwVer, dwTakeFrom, (int) x * m_dwX / dwNewX,
            (int) y * m_dwY / dwNewY, dwFilter, pt);
      }
   }
}


/**************************************************************************************
CTextureMap::AverageColorGet - Returns the RGB values of the
average color in the texture map.

inpiuts
   DWORD    dwThread - 0..MAXRAYTHREAD-1
   BOOL     fGlow - if TRUE then get the self-illumination color (under full sun).
                     if FALSE then get the reflective color.
returns
   COLORREF - Average color
*/
COLORREF CTextureMap::AverageColorGet (DWORD dwThread, BOOL fGlow)
{
   // BUGFIX - Use ForceCache() to ensure multithreaded is used to the max
   ForceCache(fGlow ? FORCECACHE_GLOW : FORCECACHE_COMBINED);
   //ProduceDownsamples(fGlow ? TI_GLOW : TI_COMBINED);
   return m_acColorAverage[fGlow ? TI_GLOW : TI_COMBINED];
}

/**************************************************************************************
CTextureMap::Delete - Call this to delete the texture, and not the "delete" call because
this handles deleting virtual function
*/
void CTextureMap::Delete (void)
{
   delete this;
}


/**************************************************************************************
CTextureMap::Init (in general)

inputs
   GUID        *pCode - GUID that identifies the texture map when querried
   GUID        *pSub - GUID that identifies the texture map when querries
   DWORD       dwX, dwY - Width and height of data in pimage
   PCMaterial  pMat - Material for the texture
   PTEXTINFO   pTextInfo - Misc information about the texture
   PBUTE       pabCombined - Array of [dwY][dwX][0..2] for colors used
               Can pass in NULL, in which case base color will be black
   PBYTE       pabColorsOnly - Array of [dwY][dwX][0..2] for colors before spec/bump.
                  Might be NULL.
   BYTE        *pabSpec - Array of [dwY][dwX][2] - Speculary map
   short       *pasBump - Array of [dwY][dwX] - Bump map
   PBYTE       pabTrans - Array of [dwY][dwX] - Transparency amount
   float       *pafGlow - Array of [dwY][dwX][3] - Amount of glow intensity. If expsoure
                  of camera is set to full sunlight then 65536.0 goes to white.
retursn
   BOOL - TRUE if initialized. FALSE if not
*/
BOOL CTextureMap::Init (const GUID *pCode, const GUID *pSub,
   DWORD dwX, DWORD dwY, PCMaterial pMat, PTEXTINFO pTextInfo,
   PBYTE pabCombined, PBYTE pabColorOnly,
   BYTE *pabSpec, short *pasBump, PBYTE pabTrans,
   float *pafGlow)
{
   // BUGFIX - make sure gamma is initialized
   GammaInit ();

   FreeAllImages (FALSE);
   m_gCode = *pCode;
   m_gSub = *pSub;
   m_fDefH = pTextInfo->fPixelLen * dwX;
   m_fDefV = pTextInfo->fPixelLen * dwY;
   memcpy (&m_Material, pMat, sizeof(m_Material));
   m_TI = *pTextInfo;
   m_dwX = dwX;
   m_dwY = dwY;

   // allocate and store the image with specularity and bump, and color-only
   DWORD i, j, dwNum;
   PTMCOLOR pt;
   dwNum = dwX * dwY;
   for (j = 0; j < 2; j++) {
      PBYTE pb = (j ? pabColorOnly : pabCombined);
      if (!pb)
         continue;

      m_apImageOrig[j] = TMImageAlloc (dwX, dwY, TMIMAGETYPE_TMCOLOR);
      if (!m_apImageOrig[j])
         return FALSE;

      CImage Image;

      // copy over
      pt = TMImagePixelTMCOLOR (m_apImageOrig[j], 0, 0);
      for (i = 0; i < dwNum; i++, pt++) {
         pt->cRGB = RGB  (pb[0], pb[1], pb[2]);
         pb += 3;
      }
   }

   // BUGFIX - Will need to allocate for glow
   if (pafGlow) {
      m_apImageOrig[TI_GLOW] = TMImageAlloc (dwX, dwY, TMIMAGETYPE_TMGLOW);
      if (!m_apImageOrig[TI_GLOW])
         return FALSE;

      PTMGLOW pg;
      pg = TMImagePixelTMGLOW (m_apImageOrig[TI_GLOW], 0, 0);
      for (i = 0; i < dwNum; i++, pg++, pafGlow += 3)
         FloatColorToTMGLOW (pafGlow, pg);
   }

   // store the Z-buf and transparency
   if (pasBump || pabTrans || pabSpec) {
      m_apImageOrig[TI_BUMP] = TMImageAlloc (dwX, dwY, TMIMAGETYPE_TMTRANS);
      if (!m_apImageOrig[TI_BUMP])
         return FALSE;

      PTMTRANS pt;
      pt = TMImagePixelTMTRANS (m_apImageOrig[TI_BUMP], 0, 0);
      for (i = 0; i < dwNum; i++, pt++) {
         pt->wBump = (pasBump ? ((WORD)pasBump[i] + 0x8000) : 0);
         pt->bTrans = pabTrans ? pabTrans[i] : 0;
         pt->bSpecExp = pabSpec ? pabSpec[i*2+0] : 0;
         pt->bSpecScale = pabSpec ? pabSpec[i*2+1] : 0;
      }
   }


   // BUGFIX - Had called forecache before

   return TRUE;
}

/**********************************************************************************
CTextureMap::ForceCache - This is called before ray tracing (which is multithreaded)
and forces the texture to cache everything about itself. NOrmally a texture will only
generate downsample info as needed, but this will cause it to all be pre-generated
so there's no multi-threaded textures walking over each other.

inputs
   DWORD          dwForceCache - One or more FORCECACHE_XXX bits indicating what
                     needs to be precalculated
*/
void CTextureMap::ForceCache (DWORD dwForceCache)
{
   // BUGFIX - optimize so that check num first
   if ((m_dwForceCache & dwForceCache) == dwForceCache)
      return;  // already cached

	MALLOCOPT_INIT;
	MALLOCOPT_OKTOMALLOC;
   // BUGFIX - Produce all the downsamples at the beginning so works with multi-threaded code
   if ((dwForceCache & FORCECACHE_COLORONLY) && !(m_dwForceCache & FORCECACHE_COLORONLY))
      ProduceDownsamples(TI_COLORONLY);
   if ((dwForceCache & FORCECACHE_COMBINED) && !(m_dwForceCache & FORCECACHE_COMBINED))
      ProduceDownsamples(TI_COMBINED);
   if  (
      ((dwForceCache & FORCECACHE_BUMP) && !(m_dwForceCache & FORCECACHE_BUMP) && (m_TI.dwMap & 0x02)) ||
      ((dwForceCache & FORCECACHE_SPEC) && !(m_dwForceCache & FORCECACHE_SPEC) && (m_TI.dwMap & 0x01)) ||
      ((dwForceCache & FORCECACHE_TRANS) && !(m_dwForceCache & FORCECACHE_TRANS) && (m_TI.dwMap & 0x04))
      )
         ProduceDownsamples(TI_BUMP);
   if ((dwForceCache & FORCECACHE_GLOW) && !(m_dwForceCache & FORCECACHE_GLOW))
      ProduceDownsamples (TI_GLOW);
	MALLOCOPT_RESTORE;

   m_dwForceCache |= dwForceCache;
}

/**********************************************************************************
CTextureMap::TextureModsSet - Change the texture modifications

inputs
   PTEXTUREMODS      pt - new modifications based on original pattern
returns
   none
*/
void CTextureMap::TextureModsSet (PTEXTUREMODS pt)
{
   if (!memcmp(&m_TextureMods, pt, sizeof(m_TextureMods)))
      return;  // no change so dont go on

   m_TextureMods = *pt;

   // wipe everything out so will recerate
   FreeAllImages (TRUE);

   // BUGFIX - Had called ForceCache before
}


/**********************************************************************************
CTextureMap::TextureModsGet - Fills in a buffer with the texture mods

inputs
   PTEXTUREMODS      pt - Filled in with modifications based on original pattern
returns
   none
*/
void CTextureMap::TextureModsGet (PTEXTUREMODS pt)
{
   *pt = m_TextureMods;
}

/**********************************************************************************
CTextureMap::GUIDsGet - Filled in the object GUID information.

inputs
   GUID     *pCode, *pSub - If not null then filled in with the object's guids.
returns
   none
*/
void CTextureMap::GUIDsGet (GUID *pCode, GUID *pSub)
{
   if (pCode)
      *pCode = m_gCode;
   if (pSub)
      *pSub = m_gSub;
}

/**********************************************************************************
CTextureMap::GUIDsSet - Sets the object GUID information.

inputs
   GUID     *pCode, *pSub
returns
   none
*/
void CTextureMap::GUIDsSet (const GUID *pCode, const GUID *pSub)
{
   m_gCode = *pCode;
   m_gSub = *pSub;
}

/**********************************************************************************
CTextureMap::DefScaleGet - Returns the default scaling that the texture model
usues. For example: If the texture is of 30cm x 30cm tiles then the default scale
would be .3, .3.

inputs
   fp      *pfDefScaleH, *pfDefScaleV - Filled in with the default scale
                  if not NULL
returns 
   none
*/
void CTextureMap::DefScaleGet (fp *pfDefScaleH, fp *pfDefScaleV)
{
   if (pfDefScaleH)
      *pfDefScaleH = m_fDefH;
   if (pfDefScaleV)
      *pfDefScaleV = m_fDefV;
}


/**********************************************************************************
CTextureMap::TMImageAlloc - Allocate the memory required for the TM image given
a width and height, and sets the variables

inputs
   DWORD       dwX,dwY - Width and height
   DWORD       dwType - Type of data. One of TMIMAGETYPE_XXX
returns
   PTMIMAGE - Allocated image. Must be freed with TMImageFree()
*/
PTMIMAGE CTextureMap::TMImageAlloc (DWORD dwX, DWORD dwY, DWORD dwType)
{
   DWORD dwSize;
   PTMIMAGE pNew;
   switch (dwType) {
      default:
      case TMIMAGETYPE_TMCOLOR:
         dwSize = sizeof(TMCOLOR);
         break;
      case TMIMAGETYPE_TMTRANS:
         dwSize = sizeof(TMTRANS);
         break;
      case TMIMAGETYPE_TMGLOW:
         dwSize = sizeof(TMGLOW);
         break;
   }
   pNew = (PTMIMAGE) ESCMALLOC (sizeof(TMIMAGE) + (dwX * dwY * dwSize));
   if (!pNew)
      return NULL;

   pNew->dwX = dwX;
   pNew->dwY = dwY;
   pNew->dwType = dwType;

   return pNew;
}

/**********************************************************************************
CTextureMap::TMImageFree - Frees the memory allocated to the image.

inputs
   PTMIMAGE    pImage - image
retunrs
   none
*/
void CTextureMap::TMImageFree (PTMIMAGE pImage)
{
   ESCFREE (pImage);
}

/**********************************************************************************
CTextureMap::FreeAllImages - Free all the images in the texture map object.

inputs
   BOOL        fLeaveOrig - If TRUE then free everything except the original.
                  If FALSE,free that too
returns
   none
*/
void CTextureMap::FreeAllImages (BOOL fLeaveOrig)
{
   DWORD i, j;
   if (!fLeaveOrig) {
      for (i = 0; i < NUMTMIMAGE; i++)
         if (m_apImageOrig[i]) {
            TMImageFree (m_apImageOrig[i]);
            m_apImageOrig[i] = NULL;
         }
   }

   for (i = 0; i < NUMTMIMAGE; i++)
      if (m_apImageColorized[i]) {
         TMImageFree (m_apImageColorized[i]);
         m_apImageColorized[i] = NULL;
      }

   for (j = 0; j < NUMTMIMAGE; j++) {
      for (i = 0; i < m_alistImageDown[j].Num(); i++) {
         PTMIMAGE pt = *((PTMIMAGE*) m_alistImageDown[j].Get(i));
         TMImageFree (pt);
      }
      m_alistImageDown[j].Clear();
   }

   m_dwForceCache = 0; // so cache
}


/**********************************************************************************
CTextureMap::FilteredPixelFromOrig - Reads the pixels from the COLOROIZED image
and does a cone filter on them.

inputs
   DWORD       dwVer - Version of image, [0] for combined, [1] for color only, [2] for spec and transparency
   DWORD       dwTakeFrom - If this is 0 then is uses the master image, otherwise it takes
                  from one of the previously downsamples images. (index = dwTakeFrom-1)
                  This can be a good way
                  to speed up downsampling. The iX, iY, and dwWidth are still in original units
   int         iX, iY - X and Y position within the original image (does modulo)
   DWORD       dwWidth - With of the image in pixels. Will be rounded down to the nearest
                         ODD value.
   PTMCOLOR     pc - Filled with the averaged color. Can also be a PTMGLOW, or PTMTRANS
returns
   none
*/
void CTextureMap::FilteredPixelFromOrig (DWORD dwVer, DWORD dwTakeFrom, int iX, int iY, DWORD dwWidth, PTMCOLOR pc)
{

   int x, y;
   fp acSum[4], dwScale, dwScaleSum;
   acSum[0] = acSum[1] = acSum[2] = acSum[3] = 0;
   dwScaleSum = 0;
   DWORD dwType;

   // BUGFIX - Get the original image, but potentially go into downsampled one
   PTMIMAGE ptiMain, pti;
   ptiMain = (dwVer < 2) ? m_apImageColorized[dwVer] : m_apImageOrig[dwVer];
   pti = ptiMain;
   if (dwTakeFrom) {
      PTMIMAGE *ppi = (PTMIMAGE*)m_alistImageDown[dwVer].Get(dwTakeFrom-1);
      if (!ppi)
         goto skipspeedup;
      pti = *ppi;

      // adjust the width, etc.
      DWORD dwScale;
      dwScale = max(ptiMain->dwX, ptiMain->dwY) / max(pti->dwX, pti->dwY);
      dwScale = max(dwScale, 1);
      dwWidth = max(dwWidth / dwScale, 3);
      iX = iX * (int) pti->dwX / (int) ptiMain->dwX;
      iY = iY * (int) pti->dwY / (int) ptiMain->dwY;
      //iX /= (int)dwScale;
      //iY /= (int)dwScale;
   }
skipspeedup:

   dwType = pti->dwType;
   dwWidth = (dwWidth - 1) / 2;
   for (y = -(int)dwWidth; y <= (int) dwWidth; y++)
      for (x = -(int)dwWidth; x <= (int) dwWidth; x++) {
         // cone shape
         dwScale = dwWidth + 1 - (DWORD) (max(y,-y));
         dwScale += dwWidth + 1 - (DWORD) (max(x,-x));

         // get the pixel
         PTMCOLOR p;
         PTMGLOW pg;
         PTMTRANS pt;
         float af[3];
         if (dwType == TMIMAGETYPE_TMGLOW) {
            pg = TMImagePixelTMGLOW (pti, iX + x, iY + y);

            TMGLOWToFloatColor (pg, af);
            acSum[0] += dwScale * af[0];
            acSum[1] += dwScale * af[1];
            acSum[2] += dwScale * af[2];
         }
         else if (dwType == TMIMAGETYPE_TMTRANS) {
            pt = TMImagePixelTMTRANS (pti, iX + x, iY + y);

            acSum[0] += dwScale * (fp) pt->bSpecExp;
            acSum[1] += dwScale * (fp) Gamma(pt->bSpecScale);
            acSum[2] += dwScale * (fp) Gamma(pt->bTrans);
            acSum[3] += dwScale * (fp) pt->wBump;
         }
         else {
            p = TMImagePixelTMCOLOR (pti, iX + x, iY + y);

            acSum[0] += dwScale * (fp) Gamma(GetRValue(p->cRGB));
            acSum[1] += dwScale * (fp) Gamma(GetGValue(p->cRGB));
            acSum[2] += dwScale * (fp) Gamma(GetBValue(p->cRGB));
         }

         // sum
         dwScaleSum += dwScale;
      }

   // divide out
   DWORD i;
   for (i = 0; i < 4; i++)
      acSum[i] /= (fp)dwScaleSum;


   // write
   if (dwType == TMIMAGETYPE_TMGLOW) {
      PTMGLOW pg = (PTMGLOW) pc;
      float af[3];
      for (i = 0; i < 3; i++)
         af[i] = acSum[i];
      FloatColorToTMGLOW (af, pg);
   }
   else if (dwType == TMIMAGETYPE_TMTRANS) {
      PTMTRANS pt = (PTMTRANS) pc;
      pt->bSpecExp = (BYTE) acSum[0];
      pt->bSpecScale = UnGamma((WORD) acSum[1]);
      pt->bTrans = UnGamma((WORD) acSum[2]);
      pt->wBump = (WORD) acSum[3];
   }
   else {
      pc->cRGB = RGB( UnGamma((WORD) acSum[0]),  UnGamma((WORD) acSum[1]), UnGamma((WORD) acSum[2]));
   }
}


/*****************************************************************************************
CTextureMap::WantToProduceDownsamples - Returns TRUE if there's any need to produce
downsamples.

inputs
   DWORD    dwVer - Version, 0 for combined, 1 for color only, 2 for bump/transparency
returns
   BOOL - TRUE if wants to produce them
*/
BOOL CTextureMap::WantToProduceDownsamples (DWORD dwVer)
{
   if (m_alistImageDown[dwVer].Num())
      return FALSE;  // already have them

   // if not supposed to downsample and have generated color then exit
   if (m_fDontDownsample && (m_acColorAverage[dwVer] != -1))
      return FALSE;

   // if no image of the type in the first place then skip
   if (!m_apImageOrig[dwVer])
      return FALSE;

   return TRUE;
}

/*****************************************************************************************
CTextureMap::ProduceDownsamples - If there aren't already downsamples then this
generates downsamples until the image is only 1 pixel wide. Each downsample is
half the width and height of the previous ones.

inputs
   DWORD    dwVer - Version, 0 for combined, 1 for color only, 2 for bump/transparency
*/ 
void CTextureMap::ProduceDownsamples (DWORD dwVer)
{
   if (!WantToProduceDownsamples(dwVer))
      return;

   // make the colorized one first
   if (dwVer < 2)
      ProduceColorized (dwVer);

   // the first down sample is the same image except with a width of 3
   // the next ones have half the pixels in either direction, and a filter width of 5, 10, 20, etc.
   DWORD dwNewX, dwNewY, dwFilter, dwTakeFrom;
   PTMIMAGE pImage;
   dwNewX = m_dwX;  // all are the same size, and this one is guaranteed to be around
   dwNewY = m_dwY;
   dwFilter = 3;
   dwTakeFrom = 0;
   while (!m_fDontDownsample) {
      // if width and height are 0 then exit
      if (!dwNewX && !dwNewY)
         break;

      // if width or height are 0 set to 1
      if (!dwNewX)
         dwNewX = 1;
      if (!dwNewY)
         dwNewY = 1;

      // generate this one
      DWORD dwType;
      dwType = m_apImageOrig[dwVer]->dwType;
      pImage = TMImageAlloc (dwNewX, dwNewY, dwType);
      if (!pImage)
         break;

      // BUGFIX - multithread thie
      EMTINFO ei;
      memset (&ei, 0, sizeof(ei));
      ei.dwVer = dwVer;
      ei.dwPass = 1;
      ei.dwFilter = dwFilter;
      ei.dwNewX = dwNewX;
      ei.dwNewY = dwNewY;
      ei.dwTakeFrom = dwTakeFrom;
      ei.dwType = dwType;
      ei.pImage = pImage;
      ThreadLoop (0, dwNewY,
         (dwNewY > 200) ? 2 : 1,
         &ei, sizeof(ei));

      // add this
      m_alistImageDown[dwVer].Add (&pImage);
      
      // BUGFIX - When downsampling take from a level up so downsampling is faster,
      // since don't resum all the same numbers all the time. To minimize errors however,
      // take from 2 levels higher
      dwTakeFrom = m_alistImageDown[dwVer].Num();
      if (dwTakeFrom)
         dwTakeFrom--;

      // reduce down
      dwNewX /= 2;
      dwNewY /= 2;
      if (dwFilter == 3)
         dwFilter = 5;
      else
         dwFilter *= 2;
   }

   // BUGFIX - if have don't downsample on, need to average all pixels
   // toghether to come up with color average
   if (m_alistImageDown[dwVer].Num())
      pImage = *((PTMIMAGE*) m_alistImageDown[dwVer].Get(m_alistImageDown[dwVer].Num()-1));
   else
      pImage = (dwVer < 2) ? m_apImageColorized[dwVer] : m_apImageOrig[dwVer];
   if (pImage && (pImage->dwType == TMIMAGETYPE_TMTRANS))
      pImage = NULL; // no average color calculation for transparency
   m_acColorAverage[dwVer] = 0;  // so dont recalc all the time, since use this to detect if calculated
   if (pImage) {
      __int64 iRed, iGreen, iBlue;
      iRed = iGreen = iBlue = 0;
      DWORD x,y;
      PTMCOLOR pt;
      PTMGLOW pg;
      DWORD dwType;
      dwType = pImage->dwType;

      for (y = 0; y < pImage->dwY; y++) for (x = 0; x < pImage->dwX; x++) {
         if (dwType == TMIMAGETYPE_TMGLOW) {
            float af[3];
            pg = TMImagePixelTMGLOW (pImage, x, y);
            TMGLOWToFloatColor (pg, af);
            af[0] = min(0xffff,af[0]);
            af[1] = min(0xffff,af[1]);
            af[2] = min(0xffff,af[2]);

            iRed += (int)af[0];
            iGreen += (int)af[1];
            iBlue += (int)af[2];
         }
         else {
            pt = TMImagePixelTMCOLOR (pImage, x, y);
            iRed += (int)(DWORD)Gamma (GetRValue(pt->cRGB));
            iGreen += (int)(DWORD)Gamma (GetGValue(pt->cRGB));
            iBlue += (int)(DWORD)Gamma (GetBValue(pt->cRGB));
         }
      }

      iRed /= (pImage->dwX * pImage->dwY);
      iGreen /= (pImage->dwX * pImage->dwY);
      iBlue /= (pImage->dwX * pImage->dwY);

      m_awColorAverage[dwVer][0] = (WORD) iRed;
      m_awColorAverage[dwVer][1] = (WORD) iGreen;
      m_awColorAverage[dwVer][2] = (WORD) iBlue;
      m_acColorAverage[dwVer] = UnGamma (m_awColorAverage[dwVer]);
   }


   // done
}

// #pragma optimize ("", off)
// BUGFIX - Turn optimization off so dont get internal compiler error

/*********************************************************************************
ApplyTextureMods - This function applies texture modifications to the colors,
such as colorization and tint.

inputs
   PTEXTUREMODS pMods - Modifications
   fp or WORD        *pafColor - Array of 3 fp's that contain the initial color and are
               modified in place
returns
   none
*/

void ApplyTextureMods (PTEXTUREMODS pMods, WORD *pawColor)
{
   DWORD i;
   float afColor[3];
   for (i = 0; i < 3;i++)
      afColor[i] = (fp)pawColor[i];

   ApplyTextureMods (pMods, afColor);

   for (i = 0; i < 3; i++) {
      afColor[i] = max(afColor[i], 0);
      afColor[i] = min(afColor[i], (fp)0xffff);
      pawColor[i] = (WORD)afColor[i];
   }
}

void ApplyTextureMods (PTEXTUREMODS pMods, float *pafColor)
{
   // apply hue/saturation
   DWORD i;
   if ((pMods->wHue != 0) || (pMods->wSaturation != 0x1000)) {
      float fHue, fSat, fLight;
      // convert to 256 base since these functions use that
      for (i = 0; i < 3; i++)
         pafColor[i] /= 256;

      ToHLS256 (pafColor[0], pafColor[1], pafColor[2], &fHue, &fLight, &fSat);
      
      fHue = fmod(fHue + pMods->wHue / 256.0, 256.0);
      fSat *= ((fp) pMods->wSaturation / 0x1000);
      if (fSat > 255)
         fSat = 255;

      FromHLS256 (fHue, fLight,fSat, &pafColor[0], &pafColor[1], &pafColor[2]);

      // convert to 256 base since these functions use that
      for (i = 0; i < 3; i++)
         pafColor[i] *= 256;
   }

   // apply tint
   // BUGFIX - Put in if statement to make faster
   if (pMods->cTint != RGB(0xff,0xff,0xff)) {
      WORD awTint[3];
      Gamma (pMods->cTint, awTint);
      for (i = 0; i < 3; i++)
         pafColor[i] *= awTint[i] / (fp) 0xffff;
   }

   // apply contrast and brightness
   if (pMods->wContrast != 0x1000) {
      fp fContrast = (fp) pMods->wContrast / 0x1000;
      for (i = 0; i < 3; i++)
         pafColor[i] = pow((fp)pafColor[i] / (fp)0x10000, (fp)fContrast) * 0x10000;
   }
   if (pMods->wBrightness != 0x1000) {
      fp fBrightness = (fp) pMods->wBrightness / 0x1000;
      for (i = 0; i < 3; i++)
         pafColor[i] *= fBrightness;
   }

}


/*********************************************************************************
CTextureMap::ProduceColorized - Produce the colorized image if its not already
there

inputs
   DWORD    dwVer - Version of the image, eitehr 0 or 1
*/
void CTextureMap::ProduceColorized(DWORD dwVer)
{
   if (m_apImageColorized[dwVer])
      return;

   if (m_apImageOrig[dwVer]->dwType != TMIMAGETYPE_TMCOLOR)
      return;

   m_apImageColorized[dwVer] = TMImageAlloc (m_apImageOrig[dwVer]->dwX,
      m_apImageOrig[dwVer]->dwY, m_apImageOrig[dwVer]->dwType);
   if (!m_apImageColorized[dwVer])
      return;

   // BUGFIX - multithread thie
   EMTINFO ei;
   memset (&ei, 0, sizeof(ei));
   ei.dwVer = dwVer;
   ei.dwPass = 0;
   ThreadLoop (0, m_apImageColorized[dwVer]->dwY,
      (m_apImageColorized[dwVer]->dwY > 200) ? 2 : 1,
      &ei, sizeof(ei));

}

/*********************************************************************************
CTextureMap::TMImagePixelInterpTMCOLOR - Gets the value of a pixel by interpolating between
two values.

inputs
   PTMIMAGE    pImage - image
   fp          fX - X value, will use own fMod
   fp          fY - Y value, will fMod it
   PGCOLOR     pc - will be filled in with interpolated value
   BOOL        fHighQuality - If TRUE then cubic interpolation
*/
void CTextureMap::TMImagePixelInterpTMCOLOR (PTMIMAGE pImage, fp fX, fp fY, PGCOLOR pc, BOOL fHighQuality)
{
   fX = myfmod(fX, pImage->dwX);
   fY = myfmod(fY, pImage->dwY);

   int   iX, iY;
   iX = (int) fX;
   iY = (int) fY;
   fX -= iX;
   fY -= iY;

   if (!fHighQuality) {
      PTMCOLOR ptUL, ptUR, ptLL, ptLR;
      ptUL = TMImagePixelTMCOLOR (pImage, iX, iY);
      ptUR = TMImagePixelTMCOLOR (pImage, iX+1, iY);
      ptLL = TMImagePixelTMCOLOR (pImage, iX, iY+1);
      ptLR = TMImagePixelTMCOLOR (pImage, iX+1, iY+1);

      WORD aw[4][3];
      Gamma (ptUL->cRGB, aw[0]);
      Gamma (ptUR->cRGB, aw[1]);
      Gamma (ptLL->cRGB, aw[2]);
      Gamma (ptLR->cRGB, aw[3]);

      WORD *pUL, *pUR, *pLL, *pLR;
      pUL = &aw[0][0];
      pUR = &aw[1][0];
      pLL = &aw[2][0];
      pLR = &aw[3][0];

      DWORD i;
      WORD *pw;
      pw = (WORD*)pc;
      for (i = 0; i < 3; i++) {
         fp fL, fR;
         fL = (1.0 - fY) * (fp)pUL[i] + fY * (fp) pLL[i];
         fR = (1.0 - fY) * (fp)pUR[i] + fY * (fp) pLR[i];
         pw[i] = (WORD)((1.0 - fX) * fL + fX * fR);
      }
   }
   else {
      // high quality
      fp af[3][4][4];
      WORD aw[3];
      DWORD x, y, i;
      PTMCOLOR ptc;
      for (y = 0; y < 4; y++)
         for (x = 0; x < 4; x++) {
            ptc = TMImagePixelTMCOLOR (pImage,
               iX + (int)pImage->dwX + (int)x - 1,
               iY + (int)pImage->dwY + (int)y - 1);   // this functional already does modulo

            Gamma (ptc->cRGB, aw);

            for (i = 0; i < 3; i++)
               af[i][y][x] = aw[i];
         }

      WORD *pw;
      pw = (WORD*)pc;
      for (i = 0; i < 3; i++) {
         fp f = CubicTextureInterp (af[i], fX, fY);
         f = max(f, 0);
         f = min(f, 0xffff);
         pw[i] = (WORD)(int)f;
      }
   }
}

// BUGFIX - Bug in optimization causes color problems
// #pragma optimize ("", on)


/*********************************************************************************
CTextureMap::TMImagePixelInterpTMTRANS - Gets the value of a pixel by interpolating between
two values.

inputs
   PTMIMAGE    pImage - image
   fp          fX - X value, will use own fMod
   fp          fY - Y value, will fMod it
   PGCOLOR     pc - will be filled in with interpolated value
   DWORD       dwWant - What bits want from pc. 0x01 => pc.wRed, 0x02 =>pc.wGreen, 0x04 => pc.wBlue, 0x08=>pc.wIntensity
   BOOL        fHighQuality - If TRUE want high quality results
*/
void CTextureMap::TMImagePixelInterpTMTRANS (PTMIMAGE pImage, fp fX, fp fY, PGCOLOR pc, DWORD dwWant, BOOL fHighQuality)
{
   fX = myfmod(fX, pImage->dwX);
   fY = myfmod(fY, pImage->dwY);

   int   iX, iY;
   iX = (int) fX;
   iY = (int) fY;
   fX -= iX;
   fY -= iY;

   if (!fHighQuality) {
      PTMTRANS apt[4];//ptUL, ptUR, ptLL, ptLR;
      apt[0] = TMImagePixelTMTRANS (pImage, iX, iY); // ul
      apt[1] = TMImagePixelTMTRANS (pImage, iX+1, iY); // ur
      apt[2] = TMImagePixelTMTRANS (pImage, iX, iY+1); // ll
      apt[3] = TMImagePixelTMTRANS (pImage, iX+1, iY+1); // lr

      DWORD i;
      WORD aw[4][4];
      if (dwWant & 0x01)
         for (i = 0; i < 4; i++)
            aw[i][0] = apt[i]->wBump;
      if (dwWant & 0x02)
         for (i = 0; i < 4; i++)
            aw[i][1] = Gamma(apt[i]->bTrans);
      if (dwWant & 0x04)
         for (i = 0; i < 4; i++)
            aw[i][2] = MAKEWORD(apt[i]->bSpecExp,apt[i]->bSpecExp);
      if (dwWant & 0x08)
         for (i = 0; i < 4; i++)
            aw[i][3] = Gamma(apt[i]->bSpecScale);

      WORD *pUL, *pUR, *pLL, *pLR;
      pUL = &aw[0][0];
      pUR = &aw[1][0];
      pLL = &aw[2][0];
      pLR = &aw[3][0];

      WORD *pw;
      pw = (WORD*)pc;
      fp fOneMinusX = 1.0 - fX;
      fp fOneMinusY = 1.0 - fY;
      for (i = 0; i < 4; i++) {
         if (!((dwWant >> i) & 0x01))
            continue;

         fp fL, fR;
         fL = fOneMinusY * (fp)pUL[i] + fY * (fp) pLL[i];
         fR = fOneMinusY * (fp)pUR[i] + fY * (fp) pLR[i];
         pw[i] = (WORD)(fOneMinusX * fL + fX * fR);
      }
   }
   else {
      // high quality
      PTMTRANS apt[4][4];//ptUL, ptUR, ptLL, ptLR;
      DWORD x, y;
      for (y = 0; y < 4; y++)
         for (x = 0; x < 4; x++)
            apt[y][x] = TMImagePixelTMTRANS (pImage,
               iX + (int)pImage->dwX + (int)x - 1,
               iY + (int)pImage->dwY + (int)y - 1);   // this functional already does modulo

      fp af[4][4][4];
      if (dwWant & 0x01)
         for (y = 0; y < 4; y++) for (x = 0; x < 4; x++)
            af[0][y][x] = apt[y][x]->wBump;
      if (dwWant & 0x02)
         for (y = 0; y < 4; y++) for (x = 0; x < 4; x++)
            af[1][y][x] = Gamma(apt[y][x]->bTrans);
      if (dwWant & 0x04)
         for (y = 0; y < 4; y++) for (x = 0; x < 4; x++)
            af[2][y][x] = MAKEWORD(apt[y][x]->bSpecExp, apt[y][x]->bSpecExp);
      if (dwWant & 0x08)
         for (y = 0; y < 4; y++) for (x = 0; x < 4; x++)
            af[3][y][x] = Gamma(apt[y][x]->bSpecScale);

      DWORD i;
      WORD *pw;
      pw = (WORD*)pc;
      for (i = 0; i < 4; i++) {
         if (!((dwWant >> i) & 0x01))
            continue;

         fp f = CubicTextureInterp (af[i], fX, fY);
         f = max(f, 0);
         f = min(f, 0xffff);
         pw[i] = (WORD)(int)f;
      }
   }
}

/*********************************************************************************
CTextureMap::TMImagePixelInterpTMGLOW - Gets the value of a pixel by interpolating between
two values.

inputs
   PTMIMAGE    pImage - image
   fp          fX - X value, will use own fMod
   fp          fY - Y value, will fMod it
   PTMGLOW     pc - will be filled in with interpolated value
   BOOL        fHighQuality - If TRUE then use cubic interpolation
*/
void CTextureMap::TMImagePixelInterpTMGLOW (PTMIMAGE pImage, fp fX, fp fY, PTMGLOW pc, BOOL fHighQuality)
{
   fX = myfmod(fX, pImage->dwX);
   fY = myfmod(fY, pImage->dwY);

   int   iX, iY;
   iX = (int) fX;
   iY = (int) fY;
   fX -= iX;
   fY -= iY;

   float afI[3];
   if (!fHighQuality) {
      PTMGLOW apg[4]; //pUL, pUR, pLL, pLR;
      apg[0] = TMImagePixelTMGLOW (pImage, iX, iY); // UL
      apg[1] = TMImagePixelTMGLOW (pImage, iX+1, iY); // UR
      apg[2] = TMImagePixelTMGLOW (pImage, iX, iY+1); // LL
      apg[3] = TMImagePixelTMGLOW (pImage, iX+1, iY+1); // LR

      // ungamma these and calculate float values
      float af[4][3];   // [UL,etc][rgb]
      DWORD i;
      for (i = 0; i < 4; i++)
         TMGLOWToFloatColor (apg[i], &af[i][0]);


      // interpolate
      for (i = 0; i < 3; i++) {
         fp fL, fR;
         fL = (1.0 - fY) * af[0][i] + fY * af[2][i];
         fR = (1.0 - fY) * af[1][i] + fY * af[3][i];
         afI[i] = ((1.0 - fX) * fL + fX * fR);
      }
   }
   else {
      // high quality
      fp af[3][4][4];
      DWORD x, y, i;
      PTMGLOW ptc;
      for (y = 0; y < 4; y++)
         for (x = 0; x < 4; x++) {
            ptc = TMImagePixelTMGLOW (pImage,
               iX + (int)pImage->dwX + (int)x - 1,
               iY + (int)pImage->dwY + (int)y - 1);   // this functional already does modulo

            TMGLOWToFloatColor (ptc, afI);

            for (i = 0; i < 3; i++)
               af[i][y][x] = afI[i];
         }

      for (i = 0; i < 3; i++) {
         fp f = CubicTextureInterp (af[i], fX, fY);
         f = max(f, 0);
         afI[i] = f;
      }
   }


   FloatColorToTMGLOW (afI, pc);
}

/*********************************************************************************
CTextureMap::QueryTextureBlurring - SOme textures, like the skydome, ignore
the pMax parameter when FillPixel() is called. If this is the
case then QueryNoTextureBlurring() will return FALSE, meaning that
the calculations don't need to be made. However, the default
behavior uses texture blurring and returns TRUE.

inputs
   DWORD       dwThread - 0..MAXRAYTHREAD-1
*/
BOOL CTextureMap::QueryTextureBlurring(DWORD dwThread)
{
   return !m_fDontDownsample;
}


/*********************************************************************************
CTextureMap::QueryBump - Returns TRUE if the texture has a bump map. FALSE
if none.

inputs
   DWORD          dwThread - 0..MAXRAYTRHEAD - 1
*/
BOOL CTextureMap::QueryBump(DWORD dwThread)
{
   return (m_TI.dwMap & 0x02) ? TRUE : FALSE;
}

/*********************************************************************************
CTextureMap::PixelBump - Given a pixel's texturepoint, and the texture delta
over the X and Y of the pixel, returns a bump amount.

inputs
   DWORD           dwThread - 0..MAXRAYTHREAD-1
   PTEXTPOINT5     pText - Texture point center
   PTEXTPOINT5     pRight - Texture point of right side of pixel - texture point of left side
   PTEXTPOINT5     pDown - Texture point of bottom side of pixel - texture point top
   PTEXTUREPOINT     pSlope - Fills in height change (in meters) over the distance.
                              .h = right height - left height (positive values stick out of surface)
                               v = BOTTOM height - TOP height
                         NOTE: This can be NULL.
   fp             *pfHeight - Fills in absolute height, in meters. Can be NULL.
   BOOL           fHighQuality - If TRUE then use high quality blurring, else bi-linear blurring
returns
   BOOL - TRUE if there's a bump map, FALSE if no bump maps for this one
*/
BOOL CTextureMap::PixelBump (DWORD dwThread, PTEXTPOINT5 pText, PTEXTPOINT5 pRight,
                             PTEXTPOINT5 pDown, PTEXTUREPOINT pSlope, fp *pfHeight, BOOL fHighQuality)
{
   if (!(m_TI.dwMap & 0x02)) {
      if (pSlope)
         pSlope->h = pSlope->v = 0;
      if (pfHeight)
         *pfHeight = 0;
      return FALSE;
   }

   // make sure have downsamples
   // BUGFIX - Use ForceCache() to ensure multithreaded is used to the max
   ForceCache(FORCECACHE_BUMP);
   //ProduceDownsamples(TI_BUMP);

   // transfer the colors over...
   // figure out what image to use based on the width of the pixel.
   PTMIMAGE pUseMap;
   PTMIMAGE *pUseBaseMap;
   pUseBaseMap = (PTMIMAGE*) m_alistImageDown[TI_BUMP].Get(0);
   if (!pUseBaseMap) {
      if (pSlope)
         pSlope->h = pSlope->v = 0;
      if (pfHeight)
         *pfHeight = 0;
      return FALSE;
   }
   fp fdwX, fdwY;
   fdwX = m_dwX; // all the same size, so use guaranteed one
   fdwY = m_dwY;
   int iNum = (int)m_alistImageDown[TI_BUMP].Num();
   int iNumMinusOne = iNum - 1;

   TEXTPOINT5   tMax;
#define BUMPDETAIL      2.0  // / 8.0 = so have some detail
      // BUGFIX - Changed BUMPDETAIL to /2 instead of /4 so wouldnt have so much aliasing
   tMax.hv[0] = max(fabs(pRight->hv[0]), fabs(pDown->hv[0])) / BUMPDETAIL;
   tMax.hv[1] = max(fabs(pRight->hv[1]), fabs(pDown->hv[1])) / BUMPDETAIL;
   // BUGFIX - dont bother with XYZ for bump map since will never use
   //tMax.xyz[0] = max(fabs(pRight->xyz[0]), fabs(pDown->xyz[0])) / BUMPDETAIL;
   //tMax.xyz[1] = max(fabs(pRight->xyz[1]), fabs(pDown->xyz[1])) / BUMPDETAIL;
   //tMax.xyz[2] = max(fabs(pRight->xyz[2]), fabs(pDown->xyz[2])) / BUMPDETAIL;

   // multiply by the image width to get the number of pixels transvered
   // by each step. Then, look at worst case scenario
   // BUGFIX - Some optimizations
   int iTXOrig = (int) max(tMax.hv[0] * fdwX, tMax.hv[1] * fdwY);
   int iTYOrig;
   for (iTYOrig = 0; iTXOrig && (iTYOrig < iNum); iTYOrig++, iTXOrig /= 2);
   int iTYColor = iTYOrig - 1;

   // if 0 then use the full colorized one
   if ((iTYColor < 0) || (iNumMinusOne == -1)) {
      pUseMap = m_apImageOrig[TI_BUMP];
   }
   else {
      // count
      // for (dwTY = 0, dwTX /= 2; dwTX && (dwTY < dwNumMinusOne); dwTY++, dwTX /= 2);

      pUseMap = pUseBaseMap[iTYColor /*dwTY*/];
   }

   // get left, right , top, bottom
   GCOLOR cL, cR, cT, cB;
   TEXTUREPOINT tText;
   tText.h = pText->hv[0] - floor(pText->hv[0]);
   tText.v = pText->hv[1] - floor(pText->hv[1]);
   // BUGFIX - Using floor instead of myfmod since should be faster
   //pText.h = myfmod(pText->hv[0], 1);
   //tText.v = myfmod(pText->hv[1], 1);
   
   // precalc coms stuff
   fp afRight[2], afText[2];
   fp fX = pUseMap->dwX;
   fp fY = pUseMap->dwY;
   afRight[0] = pRight->hv[0] / 2.0 * fX;
   afRight[1] = pRight->hv[1] / 2.0 * fY;
   afText[0] = tText.h * fX;
   afText[1] = tText.v * fY;

   if (pSlope) {
      // BUGFIX - So don't have precision error with floats - didnt seem to help
      // much with bump maps close-up though, but I think is because would
      // really need to do non-linear interp
      TMImagePixelInterpTMTRANS (pUseMap,
         afText[0] - afRight[0],
         afText[1] - afRight[1],
         &cL, 0x01, fHighQuality
         );
      TMImagePixelInterpTMTRANS (pUseMap,
         afText[0] + afRight[0],
         afText[1] + afRight[1],
         &cR, 0x01, fHighQuality
         );
      TMImagePixelInterpTMTRANS (pUseMap,
         afText[0] - afRight[0],
         afText[1] - afRight[1],
         &cT, 0x01, fHighQuality
         );
      TMImagePixelInterpTMTRANS (pUseMap,
         afText[0] + afRight[0],
         afText[1] + afRight[1],
         &cB, 0x01, fHighQuality
         );

      pSlope->h = ((fp)cR.wRed - (fp)cL.wRed) / 256.0 * m_TI.fPixelLen;
      pSlope->v = ((fp)cB.wRed - (fp)cT.wRed) / 256.0 * m_TI.fPixelLen;
   }
   if (pfHeight) {
      TMImagePixelInterpTMTRANS (pUseMap,
         afText[0],
         afText[1],
         &cL, 0x01, fHighQuality
         );
      *pfHeight = (fp)cL.wRed / 256.0 * m_TI.fPixelLen;
   }

   return pSlope ? (pSlope->h || pSlope->v) : FALSE;
}



/*********************************************************************************
CTextureMap::FillPixel - This fills one pixel
colors from the texture map. It automagically deals with antialiasing, choosing
the appropraite downsampled image.

NOTE: Since this is only called by the shader it will attempt to use the true-color
pixel and provide bump map information.

inputs
   DWORD          dwThread - 0 .. MAXRAYTHREAD-1
   DWORD          dwFlags - Flags indicating what kind of information want.
                     Use TMFP_XXX
   WORD           *pawColor - Filled with the color. Can be NULL.
   PTEXTPOINT5  pText - Texture point center.
   PTEXTPOINT5  pMax - Maximum distance covered over the pixel. Can be NULL.
   PCMaterial     pMat - Should initially be filled with the default material for
                  the surface. If there is any kind of mapping (like transparency
                  or specularity) this will be modified.
   float          *pafGlow - Filled with the glow, as seen in full sunlight. Can be NULL.
   BOOL           fHighQuality - If TRUE then use high quality blurring, else bi-linear blurring
returns
   none
*/
void CTextureMap::FillPixel (DWORD dwThread, DWORD dwFlags, WORD *pawColor, PTEXTPOINT5 pText, PTEXTPOINT5 pMax,
                             PCMaterial pMat, float *pafGlow, BOOL fHighQuality)
{
   DWORD dwVer = m_apImageOrig[TI_COLORONLY] ? TI_COLORONLY : TI_COMBINED;

   // make sure have downsamples
   // BUGFIX - Use ForceCache() to ensure multithreaded is used to the max
   ForceCache(
      (pawColor ? ((dwVer == TI_COLORONLY) ? FORCECACHE_COLORONLY : FORCECACHE_COMBINED) : 0) |
      (pafGlow ? FORCECACHE_GLOW : 0) |
      ((dwFlags & TMFP_SPECULARITY) ? FORCECACHE_SPEC : 0) |
      ((dwFlags & TMFP_TRANSPARENCY) ? FORCECACHE_TRANS : 0)
      );
   //ProduceDownsamples(dwVer);
   //ProduceDownsamples(TI_BUMP);

   BOOL fWantTransSpec = (dwFlags & (TMFP_SPECULARITY | TMFP_TRANSPARENCY));

   // transfer the colors over...
   // figure out what image to use based on the width of the pixel.
   PTMIMAGE pUse, pUseMap;
   PTMIMAGE *pUseBase, *pUseBaseMap, *pUseGlow;
   fp fdwX, fdwY;
   fdwX = m_dwX; // all the same size, so use guaranteed one
   fdwY = m_dwY;
   int iNum = (int)m_alistImageDown[dwVer].Num();
   int iNumMinusOne = iNum - 1;

   // multiply by the image width to get the number of pixels transvered
   // by each step. Then, look at worst case scenario
   // BUGFIX - Some optimizations
   int iTYOrig;
   int iTXOrig = pMax ? (int) max(pMax->hv[0] * fdwX, pMax->hv[1] * fdwY) : 0;
   for (iTYOrig = 0; iTXOrig && (iTYOrig < iNum); iTYOrig++, iTXOrig /= 2);
   int iTYColor = iTYOrig - 1;
   // iTYColor = min(iTYColor, iNumMinusOne);   // wont happen becayse iTYOrig won't be > iNum

   // if 0 then use the full colorized one
   if ((iTYColor < 0) || (iNumMinusOne == -1)) {
      pUse = pawColor ? m_apImageColorized[dwVer] : NULL;
      pUseMap = fWantTransSpec ? m_apImageOrig[TI_BUMP] : NULL;
   }
   else {
      if (pawColor) {
         // count
         // for (dwTY = 0, dwTX /= 2; dwTX && (dwTY < dwNumMinusOne); dwTY++, dwTX /= 2);

         //pUse = *((PTMIMAGE*) m_alistImageDown[dwVer].Get(dwTY));
         pUseBase = (PTMIMAGE*) m_alistImageDown[dwVer].Get(0);
         pUse = pUseBase[iTYColor /* dwTY */];
      }
      else {
         pUseBase = NULL;
         pUse = NULL;
      }

      if (fWantTransSpec) {
         pUseBaseMap = (PTMIMAGE*) m_alistImageDown[TI_BUMP].Get(0);

         // BUGFIX - Use one resolution higher for transparency and specularity so
         // that leaves arent so blurred
         // BUGFIX - Use even higher resolution for transparency, so leaves less blurred
         // BUGFIX - really need to recalc dwTY
         int iTYTransSpec = min (iTYColor - 2, iNumMinusOne);
         // for (dwTY = 0, dwTX = dwTXOrig / 8; dwTX && (dwTY < dwNumMinusOne); dwTY++, dwTX /= 2);
         if (iTYTransSpec >= 0)
            pUseMap = (pUseBaseMap ? pUseBaseMap[iTYTransSpec /*dwTY*/] : m_apImageOrig[TI_BUMP]);
         else
            pUseMap = m_apImageOrig[TI_BUMP];
      }
      else
         pUseMap = NULL;
   }

   // know which one to use, so pull out the color
   // BUGFIX - Interpolate the colors
   //pc = TMImagePixel (pUse, (int) (pText->h * (fp) pUse->dwX),
   //   (int) (pText->v * (fp) pUse->dwY));
   //memcpy (pawColor, &pc->wRed, sizeof(WORD)*3);
   GCOLOR cTemp;
   // BUGFIX - If no source color then fill with black
   // BUGFIX - Only if have color
   if (pawColor) {
      if (pUse) {
         TMImagePixelInterpTMCOLOR (pUse, pText->hv[0] * (fp)pUse->dwX,
            pText->hv[1] * (fp)pUse->dwY, &cTemp, fHighQuality);
         memcpy (pawColor, &cTemp.wRed, sizeof(WORD)*3);
      }
      else
         pawColor[0] = pawColor[1] = pawColor[2] = 0;
   }

   // Need to include glow
   if (pafGlow) {
      pafGlow[0] = pafGlow[1] = pafGlow[2] = 0;
      pUseGlow = (PTMIMAGE*) m_alistImageDown[TI_GLOW].Get(0);

      if (m_apImageOrig[TI_GLOW]) {
         DWORD dwNumMinusOne = m_alistImageDown[TI_GLOW].Num() - 1;
         // dwTX = dwTXOrig;
         if ((iTYColor < 0) || (iNumMinusOne == -1)) {
            pUse = m_apImageOrig[TI_GLOW];
         }
         else {
            // count
            // for (dwTY = 0, dwTX /= 2; dwTX && (dwTY < dwNumMinusOne); dwTY++, dwTX /= 2);

            //pUse = *((PTMIMAGE*) m_alistImageDown[dwVer].Get(dwTY));
            pUse = pUseGlow[iTYColor /*dwTY*/];
         }

         // interp
         TMGLOW g;
         TMImagePixelInterpTMGLOW (pUse, pText->hv[0] * (fp)pUse->dwX,
            pText->hv[1] * (fp)pUse->dwY, &g, fHighQuality);
         TMGLOWToFloatColor (&g, pafGlow);
      } // if glow
   }

   if (pUseMap && (dwFlags & (TMFP_SPECULARITY | TMFP_TRANSPARENCY)) ) {
      BOOL fWantSpec = ((m_TI.dwMap & 0x01) && (dwFlags & TMFP_SPECULARITY) );
      BOOL fWantTrans = ((m_TI.dwMap & 0x04) && (dwFlags & TMFP_TRANSPARENCY));

      // BUGFIX - Interpolate colors
      //pc = TMImagePixel (pUseMap, (int) (pText->h * (fp) pUseMap->dwX),
      //   (int) (pText->v * (fp) pUseMap->dwY));
      // BUGFIX - Only call if want one or the other
      if (fWantSpec || fWantTrans)
         TMImagePixelInterpTMTRANS (pUseMap, pText->hv[0] * (fp)pUseMap->dwX,
            pText->hv[1] * (fp)pUseMap->dwY, &cTemp,
            (fWantSpec ? 0x0c : 0) | (fWantTrans ? 0x02 : 0), fHighQuality);
            //    BUGFIX - Was pUse->dwX and pUse->dwY, which is wrong

      if (fWantSpec) {
         pMat->m_wSpecExponent = cTemp.wBlue;
         pMat->m_wSpecReflect = cTemp.wIntensity;
      }
      if (fWantTrans)
         pMat->m_wTransparency = cTemp.wGreen;
   }
}


/*******************************************************************************
CTextureMap::DimensionalityQuery - Returns flags indicating what dimesnionality
is supported by the texture.

inputs
   DWORD dwThread       - 0..MAXRAYTHREAD-1
returns
   DWORD - 0x01 bit if supports HV, 0x02 bit if supports XYZ
*/
DWORD CTextureMap::DimensionalityQuery (DWORD dwThread)
{
   return 0x01;   // to indicate only HV
}


/*******************************************************************************
CTextureMap::MightBeTransparent - Returns bit flags if the texture map might contain
transparencies - basically if it has a transparency map or it's inherenly
transparent

inputs
   DWORD    dwThread - 0..MAXRAYTHREAD-1
returns
   DWORD - 0x01 if the material has m_wTransparency. 0x02 if there's a texture map
         with transparency. Or combination;
*/
DWORD CTextureMap::MightBeTransparent (DWORD dwThread)
{
   DWORD dw;
   dw = (m_TI.dwMap & 0x04) ? 0x02 : 0;
   if (m_Material.m_wTransparency)
      dw |= 0x01;
   return dw;
}


/********************************************************************************
CTextureMap::MaterialGet - Gets the default material type that the texture recommends.

inputs
   DWORD          dwThread - 0..MAXRAYTHREAD-1
   PCMaterial     pMat - Filled with type
*/
void CTextureMap::MaterialGet (DWORD dwThread, PCMaterial pMat)
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


/*********************************************************************************
CTextureMap::FillLine - This fills memory containing an array of colors with the
colors from the texture map. It automagically deals with antialiasing, choosing
the appropraite downsampled image.

inputs
   DWORD          dwThread - 0..MAXRAYTHREAD-1
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
void CTextureMap::FillLine (DWORD dwThread, PGCOLOR pac, DWORD dwNum, const PTEXTPOINT5 pLeft, const PTEXTPOINT5 pRight,
                            float *pafGlow, BOOL *pfGlow, WORD *pawTrans, BOOL *pfTrans, fp *pafAlpha)
{
   _ASSERTE (dwNum < 1000000);   // to make sure not negative
   // _ASSERTE (!m_amemPosn[dwThread].m_dwCurPosn);   // to make sure not already in this fun

#ifdef _DEBUG
//   m_amemPosn[dwThread].m_dwCurPosn = 1;
#endif

   // converting fill-line to use textpoint5, but may be able to
   // use texturepoint since mostly will be done with only hv. Ultimately may
   // have different c++ object for volumetric textures
   BOOL fNeedXYZ = (DimensionalityQuery(dwThread) & 0x02) ? TRUE : FALSE;

   // figure out parameters for use of alpha
	MALLOCOPT_INIT;
	MALLOCOPT_OKTOMALLOC;
   if (!m_amemPosn[dwThread].Required (dwNum * sizeof(TEXTPOINT5))) {
   	MALLOCOPT_RESTORE;
#ifdef _DEBUG // hack - these are to look for memory leakj
//      m_amemPosn[dwThread].m_dwCurPosn = 0;
#endif
      return;
   }
#ifdef _DEBUG
//   if (sizeof(PVOID) > sizeof(DWORD))
//      EscMemoryIntegrity (m_amemPosn[dwThread].p);
#endif

	MALLOCOPT_RESTORE;
   TEXTPOINT5 tDelta;
   PTEXTPOINT5 pt;
   pt = (PTEXTPOINT5) m_amemPosn[dwThread].p;
   DWORD i, j;
   for (i = 0; i < 2; i++)
      tDelta.hv[i] = pRight->hv[i] - pLeft->hv[i];
   if (fNeedXYZ)
      for (i = 0; i < 3; i++)
         tDelta.xyz[i] = pRight->xyz[i] - pLeft->xyz[i];

   if (pafAlpha) {
      for (i = 0; i < dwNum; i++) {
         for (j = 0; j < 2; j++)
            pt[i].hv[j] = pLeft->hv[j] + pafAlpha[i] * tDelta.hv[j];
         if (fNeedXYZ) for (j = 0; j < 3; j++)
            pt[i].xyz[j] = pLeft->xyz[j] + pafAlpha[i] * tDelta.xyz[j];
      }
   }
   else {   // linear
      for (i = 0; i < dwNum; i++) {
         for (j = 0; j < 2; j++)
            pt[i].hv[j] = pLeft->hv[j] + ( i / (fp) dwNum) * tDelta.hv[j];
         if (fNeedXYZ) for (j = 0; j < 3; j++)
            pt[i].xyz[j] = pLeft->xyz[j] + ( i / (fp) dwNum) * tDelta.xyz[j];
      }
   }

   // since called by fast renderer, awlays use version 0
   DWORD dwVer = TI_COMBINED;

   // make sure have downsamples
   // BUGFIX - Use ForceCache() to ensure multithreaded is used to the max
   ForceCache(
      FORCECACHE_COMBINED |
      (pafGlow ? FORCECACHE_GLOW : 0) |
      (pawTrans ? FORCECACHE_TRANS : 0)
      );
   //ProduceDownsamples(dwVer);

   // glow
   BOOL fGlow;
   fGlow = m_apImageOrig[TI_GLOW] ? TRUE : FALSE;
   if (pfGlow)
      *pfGlow = fGlow;
   if (!pafGlow)
      fGlow = FALSE;
   // This call to ProduceDownsamples no longer needed
   //if (fGlow)
   //   ProduceDownsamples (TI_GLOW);

   // transparency
   BOOL fTrans;
   fTrans = (m_apImageOrig[TI_BUMP] ? TRUE : FALSE) && (m_TI.dwMap & 0x04);
   if (pfTrans)
      *pfTrans = fTrans;
   if (!pawTrans)
      fTrans = FALSE;
   // This call to ProduceDownsamples no longer needed
   //if (fTrans)
   //   ProduceDownsamples (TI_BUMP);

   // transfer the colors over...
   // figure out what image to use based on the width of the pixel.
   // DWORD dwTX, dwTY, dwNumMinusOne;
   PTMIMAGE pUse;
   PTMIMAGE *pUseBase, *pUseGlow; // , *pUseTrans;
   pUseBase = (PTMIMAGE*) m_alistImageDown[dwVer].Get(0);
   pUseGlow = (PTMIMAGE*) m_alistImageDown[TI_GLOW].Get(0);
   // pUseTrans = (PTMIMAGE*) m_alistImageDown[TI_BUMP].Get(0);
   PTMCOLOR ptc;
   fp fdwX, fdwY;
   fdwX = m_dwX; // since all the same size, use one that's guarantted
   fdwY = m_dwY;
   int iNum = (int) m_alistImageDown[dwVer].Num();
   int iNumMinusOne = iNum- 1;
   for (i = 0; i < dwNum; i++) {
      // width
      if (i < (dwNum-1)) {
         for (j = 0; j < 2; j++)
            tDelta.hv[j] = fabs(pt[i].hv[j] - pt[i+1].hv[j]);
         if (fNeedXYZ) for (j = 0; j < 3; j++)
            tDelta.xyz[j] = fabs(pt[i].xyz[j] - pt[i+1].xyz[j]);
      }
      else {
         // look left if possible
         if (i) {
            for (j = 0; j < 2; j++)
               tDelta.hv[j] = fabs(pt[i].hv[j] - pt[i-1].hv[j]);
            if (fNeedXYZ) for (j = 0; j < 3; j++)
               tDelta.xyz[j] = fabs(pt[i].xyz[j] - pt[i-1].xyz[j]);
         }
         else {
            for (j = 0; j < 2; j++)
               tDelta.hv[j] = fabs(pRight->hv[j] - pLeft->hv[j]);
            if (fNeedXYZ) for (j = 0; j < 3; j++)
               tDelta.xyz[j] = fabs(pRight->xyz[j] - pLeft->xyz[j]);
         }
      }

      // multiply by the image width to get the number of pixels transvered
      // by each step. Then, look at worst case scenario
      // BUGFIX - Some optimizations
      int iTYOrig;
      int iTXOrig = (int) max(tDelta.hv[0] * fdwX, tDelta.hv[1] * fdwY);
      for (iTYOrig = 0; iTXOrig && (iTYOrig < iNum); iTYOrig++, iTXOrig /= 2);
      int iTYColor = iTYOrig - 1;

      // if 0 then use the full colorized one
      if ((iTYColor < 0) || (iNumMinusOne == -1))
         pUse = m_apImageColorized[dwVer];
      else {
         // count
         // for (dwTY = 0, dwTX /= 2; dwTX && (dwTY < dwNumMinusOne); dwTY++, dwTX /= 2);

         //pUse = *((PTMIMAGE*) m_alistImageDown[dwVer].Get(dwTY));
         pUse = pUseBase[iTYColor /*dwTY*/];

      }

      // BUGFIX - If no pixel data then fill with 0
      if (!pUse)
         memset (pac +i, 0, sizeof(pac[i]));
      else {
         // know which one to use, so pull out the color
         ptc = TMImagePixelTMCOLOR (pUse, (int) (pt[i].hv[0] * (fp) pUse->dwX),
            (int) (pt[i].hv[1] * (fp) pUse->dwY));
         Gamma (ptc->cRGB, &pac[i].wRed);
      }

      // if glow do that
      if (fGlow) {
         int iNumMinusOne = (int)m_alistImageDown[TI_GLOW].Num() - 1;
         PTMGLOW pg;

         // if 0 then use the full colorized one
         // dwTX = dwTXOrig;
         if ((iTYColor < 0) || (iNumMinusOne == -1))
            pUse = m_apImageOrig[TI_GLOW];
         else {
            // count
            // for (dwTY = 0, dwTX /= 2; dwTX && (dwTY < dwNumMinusOne); dwTY++, dwTX /= 2);

            //pUse = *((PTMIMAGE*) m_alistImageDown[dwVer].Get(dwTY));
            pUse = pUseGlow[min(iTYColor, iNumMinusOne) /*dwTY*/];
         }

         pg = TMImagePixelTMGLOW (pUse, (int) (pt[i].hv[0] * (fp) pUse->dwX),
            (int) (pt[i].hv[1] * (fp) pUse->dwY));
         TMGLOWToFloatColor (pg, pafGlow + i * 3);
      }  // fGlow

      // if trans do that
      if (fTrans) {
         // iNumMinusOne = (int) m_alistImageDown[TI_BUMP].Num() - 1;
         PTMTRANS ptt;

         // if 0 then use the full colorized one
         // dwTX = dwTXOrig;
         // BUGFIX - For transparency of non-quality rendering, don't try to do
         // blurring. Just use original
         pUse = m_apImageOrig[TI_BUMP];
         //if (!dwTX || (dwNumMinusOne == (DWORD)-1))
         //   pUse = m_apImageOrig[TI_BUMP];
         //else {
            // count
         //   for (dwTY = 0, dwTX /= 2; dwTX && (dwTY < dwNumMinusOne); dwTY++, dwTX /= 2);

            //pUse = *((PTMIMAGE*) m_alistImageDown[dwVer].Get(dwTY));
         //   pUse = pUseTrans[dwTY];
         //}

         ptt = TMImagePixelTMTRANS (pUse, (int) (pt[i].hv[0] * (fp) pUse->dwX),
            (int) (pt[i].hv[1] * (fp) pUse->dwY));
         pawTrans[i] = MAKEWORD(ptt->bTrans, ptt->bTrans);
      }  // fGlow

   } // i

#ifdef _DEBUG
//   if (sizeof(PVOID) > sizeof(DWORD))
//      EscMemoryIntegrity (m_amemPosn[dwThread].p);
//   m_amemPosn[dwThread].m_dwCurPosn = 0;
//   _ASSERTE (pt == m_amemPosn[dwThread].p);
#endif
}

/***********************************************************************************
CTextureMapSocket::FillImage - A convencience function to fill a CImage with the texutre
map. Used to show a sample of the texture map.

inputs
   PCImage           pImage - image to fill in
   BOOL              fEncourageSphere - If TRUE then encourage it to draw a sphere
   BOOL              fStretchToFit - If TRUE stretch texture over entire area
   fp                afTextureMatrix[2][2] - For how to rotate the HV texture
   PCMatrix          pmTextureMatrix - For how to rotate the volumetric texture
   PCMaterial        pMaterial - Material to use - overrides default material for texture
                     Can be NULL
returns
   none
*/
void CTextureMapSocket::FillImage (PCImage pImage, BOOL fEncourageSphere, BOOL fStretchToFit,
                  fp afTextureMatrix[2][2], PCMatrix pmTextureMatrix,
                  PCMaterial pMaterial)
{
   // want to draw as sphere
   BOOL fDrawAsSphere = fEncourageSphere;
   if (DimensionalityQuery(0) & 0x02)
      fDrawAsSphere = TRUE;

   // determine the ideal size
   fp fH, fV;
   DefScaleGet (&fH, &fV);
   fp fRadius = max(fH, fV);
   if (fDrawAsSphere)
      fRadius *= sqrt((fp)2);  // need slightly larger so can see more of texture
   fRadius = MeasureFindScale(fRadius);

   // if this is only a flat texture then fill it with a flat image,
   // else if it's a volumetric image then use sphere
   if (fDrawAsSphere) {
      SphereDraw (pImage, fRadius/2, this, afTextureMatrix, pmTextureMatrix, pMaterial, fStretchToFit);
   }
   else {
      TEXTPOINT5  atp[4];
      memset (atp, 0, sizeof(atp));
      atp[1].hv[0] = atp[1].xyz[0] = fRadius;
      atp[3].hv[0] = atp[3].xyz[0] = fRadius;
      atp[3].hv[1] = atp[3].xyz[1] = fRadius;
      atp[2].hv[1] = atp[2].xyz[1] = fRadius;

      if (fStretchToFit) {
         atp[1].hv[0] = atp[3].hv[0] = fH;
         atp[2].hv[1] = atp[3].hv[1] = fV;
      }
      
      // rotate
      DWORD i, j;
      CPoint p;
      for (i = 0; i < 4; i++) {
         // rotate 2d
         TextureMatrixMultiply2D (afTextureMatrix, (PTEXTUREPOINT)(&atp[i].hv[0]));

         // rotate 3d
         for (j = 0; j < 3; j++)
            p.p[j] = atp[i].xyz[j];
         p.p[3] = 1;
         p.MultiplyLeft (pmTextureMatrix);
         for (j = 0; j < 3; j++)
            atp[i].xyz[j] = p.p[j];
      }
      FillImageFlat (pImage, &atp[0], &atp[1], &atp[2], &atp[3]);
   }


   // want to write the scale into the image
   WCHAR szTemp[64];
   MeasureToString (fRadius, szTemp);
   DrawTextOnImage (pImage, 1, -1, NULL, szTemp);

   return;
}


/***********************************************************************************
CTextureMapSocket::FillImageFlat - A convencience function to fill a CImage with the texutre
map. Used to show a sample of the texture map.

inputs
   PCImage     pImage - image to fill in
   PTEXTPOINT5  pUL, pUR, pLL, pLR - Texture h and v at the corners
returns
   none
*/
void CTextureMapSocket::FillImageFlat (PCImage pImage, PTEXTPOINT5 pUL, PTEXTPOINT5 pUR,
      PTEXTPOINT5 pLL, PTEXTPOINT5 pLR)
{
   // memory for the colors
   CMem mem, memGlow;
   if (!mem.Required (pImage->Width() * sizeof(GCOLOR)))
      return;
   if (!memGlow.Required (pImage->Width() * sizeof(float) * 3))
      return;
   PGCOLOR pc;
   float *pfGlow;
   BOOL fGlow;
   pc = (PGCOLOR) mem.p;
   pfGlow = (float*) memGlow.p;

   // loop
   DWORD x, y;
   DWORD j;
   for (y = 0; y < pImage->Height(); y++) {
      TEXTPOINT5 l, r;
      fp fAlpha = y / (fp)pImage->Height();

      // figure out left and right
      for (j = 0; j < 2; j++) {
         l.hv[j] = pUL->hv[j] + (pLL->hv[j] - pUL->hv[j]) * fAlpha;
         r.hv[j] = pUR->hv[j] + (pLR->hv[j] - pUR->hv[j]) * fAlpha;
      }
      for (j = 0; j < 3; j++) {
         l.xyz[j] = pUL->xyz[j] + (pLL->xyz[j] - pUL->xyz[j]) * fAlpha;
         r.xyz[j] = pUR->xyz[j] + (pLR->xyz[j] - pUR->xyz[j]) * fAlpha;
      }

      // fill in the colors
      pc = (PGCOLOR) mem.p;
      pfGlow = (float*) memGlow.p;  // BUGFIX - Put this in or wouild overwrite mem
      FillLine (0/* assume thread 0 */, pc, pImage->Width(), &l, &r, pfGlow, &fGlow, NULL, NULL, 0);

      // transfer over
      PIMAGEPIXEL pip;
      pip = pImage->Pixel (0, y);
      for (x = 0; x < pImage->Width(); x++, pip++, pc++) {
         pip->wRed = pc->wRed;
         pip->wGreen = pc->wGreen;
         pip->wBlue = pc->wBlue;

         if (fGlow) {
            pip->wRed = (WORD) min((fp)0xffff, (fp)pip->wRed + pfGlow[0]);
            pip->wGreen = (WORD) min((fp)0xffff, (fp)pip->wGreen + pfGlow[1]);
            pip->wBlue = (WORD) min((fp)0xffff, (fp)pip->wBlue + pfGlow[2]);
            pfGlow += 3;
         }
      }
   }

   // done
}


#if 0 // - DEAD CODE
/************************************************************************************
TextureQuerySize - Gets the default width and height of a texture.

inputs
   GUID        *pCode, *pSub - texture ID
   fp      *pfx, *pfy - Filled in with default width and height in meters
returns
   BOOL - TRUE if succes
*/
BOOL TextureQuerySize (const GUID *pCode, const GUID *pSub, fp *pfx, fp *pfy)
{
   TEXTINFO ti;
   DWORD dwX, dwY;
   *pfx = *pfx = 0;
   if (!TextureAlgoUnCache(pCode, pSub, &dwX, &dwY, NULL, &ti)) 
      return FALSE;
   *pfx = ti.fPixelLen * (fp)dwX;
   *pfy = ti.fPixelLen * (fp)dwY;

   return TRUE;
}
#endif // 0

/***********************************************************************************
TextureCreate - Creates a new texture object based on the GUIDs.

inputs
   GUID     *pCode - Code guid
   GUID     *pSub - sub guid
returns
   PCTextureMapSocket - New object. NULL if error
*/
PCTextureMapSocket TextureCreate (DWORD dwRenderShard, const GUID *pCode, const GUID *pSub)
{
   TEXTINFO ti;
   CMaterial Mat;
   DWORD dwX, dwY;
   PBYTE pComplete, pColorOnly, pTrans, pGlow;
   PSHORT pBumpMap;
   PBYTE pbSpecMap;
   
   if (!TextureAlgoUnCache (dwRenderShard, pCode, pSub, &dwX, &dwY, &Mat, &ti, &pComplete,
      &pColorOnly, &pbSpecMap, &pBumpMap, &pTrans, &pGlow)) {

         // try creating a volumetric texture then
         return TextureCreateVol (dwRenderShard, pCode, pSub);
         //return NULL;
      }

   // BUGFIX - Got a crash in release mode of MIFClient when dwX and dwY came
   // out 0. therefore extra caution
   if (!dwX || !dwY)
      return NULL;

   // found the right one
   PCTextureMap pNew;
   pNew = new CTextureMap;
   if (!pNew)
      return NULL;

   CMem memGlow;
   if (pGlow && memGlow.Required (dwX * dwY * sizeof(float) * 3)) {
      float *paf = (float*) memGlow.p;
      BYTE *pb = pGlow;
      DWORD i;
      for (i = 0; i < dwX * dwY * 3; i++, paf++, pb++)
         paf[0] = (fp) Gamma (pb[0]) * ti.fGlowScale;
   }

   if (pNew->Init (pCode, pSub, dwX, dwY, &Mat, &ti, pComplete, pColorOnly, pbSpecMap,
      pBumpMap, pTrans, pGlow ? (float*) memGlow.p : NULL))
      return pNew;

   // else dont know
   delete pNew;
   return NULL;
}


/***********************************************************************************
TextureEnumMajor - Enumerates the major category names for textures.

inputs
   PCListFixed    pl- List to be filled with, List of PWSTR that point to the category
      names. DO NOT change the strings. This IS sorted.
returns
   none
*/
void TextureEnumMajor (DWORD dwRenderShard, PCListFixed pl)
{
   CListFixed l2;
   LibraryTextures(dwRenderShard, FALSE)->EnumMajor (pl);
   LibraryTextures(dwRenderShard, TRUE)->EnumMajor(&l2);
   LibraryCombineLists (pl, &l2);
}


/***********************************************************************************
TextureEnumMinor - Enumerates the minor category names for textures.

inputs
   PCListFixed    pl- List to be filled with, List of PWSTR that point to the category
      names. DO NOT change the strings. This IS sorted.
   PWSTR          pszMajor - Major category name
returns
   none
*/
void TextureEnumMinor (DWORD dwRenderShard, PCListFixed pl, PWSTR pszMajor)
{
   CListFixed l2;
   LibraryTextures(dwRenderShard, FALSE)->EnumMinor (pl, pszMajor);
   LibraryTextures(dwRenderShard, TRUE)->EnumMinor (&l2, pszMajor);
   LibraryCombineLists (pl, &l2);
}

/***********************************************************************************
TextureEnumItems - Enumerates the items of a minor category in textures.

inputs
   PCListFixed    pl- List to be filled with, List of PWSTR that point to the category
      names. DO NOT change the strings. This IS sorted.
   PWSTR          pszMajor - Major category name
   PWSTR          pszMinor - Minor category name
returns
   none
*/
void TextureEnumItems (DWORD dwRenderShard, PCListFixed pl, PWSTR pszMajor, PWSTR pszMinor)
{
   CListFixed l2;
   LibraryTextures(dwRenderShard, FALSE)->EnumItems (pl, pszMajor, pszMinor);
   LibraryTextures(dwRenderShard, TRUE)->EnumItems (&l2, pszMajor, pszMinor);
   LibraryCombineLists (pl, &l2);
}

/***********************************************************************************
TextureGUIDsFromName - Given a texture name, retuns the GUIDs

inputs
   PWSTR          pszMajor - Major category name
   PWSTR          pszMinor - Minor category name
   PWSTR          pszName - Name of the texture
   GUID           *pgCode - Filled with the code GUID if successful
   GUID           *pgSub - Filled with the sub GUID if successful
returns
   BOOL - TRUE if find
*/
BOOL TextureGUIDsFromName (DWORD dwRenderShard, PWSTR pszMajor, PWSTR pszMinor, PWSTR pszName, GUID *pgCode, GUID *pgSub)
{
   if (LibraryTextures(dwRenderShard, FALSE)->ItemGUIDFromName (pszMajor, pszMinor, pszName, pgCode, pgSub))
      return TRUE;
   if (LibraryTextures(dwRenderShard, TRUE)->ItemGUIDFromName (pszMajor, pszMinor, pszName, pgCode, pgSub))
      return TRUE;
   return FALSE;
}

/***********************************************************************************
TextureNameFromGUIDs - Given a textures GUIDs, this fills in a buffer with
its name strings.

inputs
   GUID           *pgCode - Code guid
   GUID           *pgSub - Sub guid
   PWSTR          pszMajor - Filled with Major category name
   PWSTR          pszMinor - Filled with Minor category name
   PWSTR          pszName - Filled with Name of the texture
returns
   BOOL - TRUE if find
*/
BOOL TextureNameFromGUIDs (DWORD dwRenderShard, GUID *pgCode, GUID *pgSub,PWSTR pszMajor, PWSTR pszMinor, PWSTR pszName)
{
   if (LibraryTextures(dwRenderShard, FALSE)->ItemNameFromGUID (pgCode, pgSub, pszMajor, pszMinor, pszName))
      return TRUE;
   if (LibraryTextures(dwRenderShard, TRUE)->ItemNameFromGUID (pgCode, pgSub, pszMajor, pszMinor, pszName))
      return TRUE;
   return FALSE;
}

/***********************************************************************************
TextureCacheInit - Call in winmain to initialize
*/
void TextureCacheInit (DWORD dwRenderShard, PCProgressSocket pProgress)
{
   if (gapmfAlgoTexture[dwRenderShard])
      return;  // already initialized

   InitializeCriticalSection (&gacsTextureMapLast[dwRenderShard]);

   galTEXTURECACHE[dwRenderShard].Init (sizeof(TEXTURECACHE));
   memset (gaaTEXTURECACHELast[dwRenderShard], 0, sizeof(gaaTEXTURECACHELast[dwRenderShard]));
   gaqwTextureCacheTimeStamp[dwRenderShard]++;
   gaOSLastPaint[dwRenderShard] = NULL;

   // read in the algorithmic texture cache
   WCHAR szTemp[512];
   MultiByteToWideChar (CP_ACP, 0, gszAppDir, -1, szTemp, sizeof(szTemp)/sizeof(WCHAR));
   wcscat (szTemp, L"AlgoTextureCache." CACHEFILEEXT);

   gapmfAlgoTexture[dwRenderShard] = new CMegaFile;
   if (!gapmfAlgoTexture[dwRenderShard])
      return;
   if (!gapmfAlgoTexture[dwRenderShard]->Init (szTemp, &GUID_AlgoFileHeader))
      return;

   gapmfAlgoTexture[dwRenderShard]->LimitSize (500 * 1000000, 0); // don't allow this to get too large
      // BUGFIX - Was 100 MB. Upped to 500 MB of textures

   // get the texture cache size
   __int64 iSize;
   int *pi = (int*) gapmfAlgoTexture[dwRenderShard]->Load (gpszTextureDetail, &iSize);
   if (pi) {
      gaiTextureDetail[dwRenderShard] = *pi;
      MegaFileFree (pi);
   }


#if 0 // to test
   TextureDetailSet (0);
#endif

   // done
}

/***********************************************************************************
TextureCacheEnd - Call before leaving to free up any cached textures
*/
void TextureCacheEnd (DWORD dwRenderShard, PCProgressSocket pProgress)
{
   if (gaOSLastPaint[dwRenderShard])
      delete gaOSLastPaint[dwRenderShard];
   gaOSLastPaint[dwRenderShard] = NULL;

#ifdef _DEBUG
   //_CrtCheckMemory ();
#endif
   if (gapmfAlgoTexture[dwRenderShard]) {
      delete gapmfAlgoTexture[dwRenderShard];
      gapmfAlgoTexture[dwRenderShard] = NULL;
   }
   galistALGOTEXTCACHE[dwRenderShard].Clear();

   // free up all the textures
   DWORD i;
   PTEXTURECACHE ptc = (PTEXTURECACHE)galTEXTURECACHE[dwRenderShard].Get(0);
   for (i = 0; i < galTEXTURECACHE[dwRenderShard].Num(); i++, ptc++) {
#ifdef _DEBUG
      // NOTE: Disabling because at the moment reference counts aren't kept up to date properly
      //if (ptc->dwCount)
      //   OutputDebugString ("\r\nTexture not released!");
#endif
      ptc->pTexture->Delete();
   }
   galTEXTURECACHE[dwRenderShard].Clear();
   memset (gaaTEXTURECACHELast[dwRenderShard], 0, sizeof(gaaTEXTURECACHELast[dwRenderShard]));
   gaqwTextureCacheTimeStamp[dwRenderShard]++;

   DeleteCriticalSection (&gacsTextureMapLast[dwRenderShard]);
}


/***********************************************************************************
TextureCacheClear - Call this to clear the cache from time to time,
ensuring that not too many textures are cached, and that they're not consuming
all memory.

NOTE: Only call this when you're sure a texture ISN'T cached... ie: when not
rendering.

inputs
   DWORD          dwMax - Max number of texture caches allowed, such as 100
*/
static int _cdecl TEXTURECACHECompare (const void *elem1, const void *elem2)
{
   TEXTURECACHE *pdw1, *pdw2;
   pdw1 = (TEXTURECACHE*) elem1;
   pdw2 = (TEXTURECACHE*) elem2;

   if (pdw2->iLastUsed > pdw1->iLastUsed)
      return 1;
   else if (pdw2->iLastUsed == pdw1->iLastUsed)
      return 0;
   else
      return -1;
   // return (int)(pdw2->dwLastUsed) - (int)(pdw1->dwLastUsed);
}

void TextureCacheClear (DWORD dwRenderShard, DWORD dwMax)
{
   EnterCriticalSection (&gacsTextureMapLast[dwRenderShard]);
   if (galTEXTURECACHE[dwRenderShard].Num() <= dwMax) {
      LeaveCriticalSection (&gacsTextureMapLast[dwRenderShard]);
      return;
   }

   // clear last cache since will be invalid
   memset (gaaTEXTURECACHELast[dwRenderShard], 0, sizeof(gaaTEXTURECACHELast[dwRenderShard]));
   gaqwTextureCacheTimeStamp[dwRenderShard]++;

   // resort the cache
   qsort (galTEXTURECACHE[dwRenderShard].Get(0), galTEXTURECACHE[dwRenderShard].Num(), sizeof(TEXTURECACHE), TEXTURECACHECompare);

   // clear from end
   DWORD dwNum = galTEXTURECACHE[dwRenderShard].Num();
   PTEXTURECACHE ptc = (PTEXTURECACHE) galTEXTURECACHE[dwRenderShard].Get(dwMax);
   DWORD i;
   for (i = dwMax; i < dwNum; i++, ptc++)
      ptc->pTexture->Delete();
   galTEXTURECACHE[dwRenderShard].Truncate (dwMax);

   LeaveCriticalSection (&gacsTextureMapLast[dwRenderShard]);
}



/***********************************************************************************
TextureCacheNum - Returns the number of cached textures
*/
DWORD TextureCacheNum (DWORD dwRenderShard)
{
   EnterCriticalSection (&gacsTextureMapLast[dwRenderShard]);
   DWORD dwRet = galTEXTURECACHE[dwRenderShard].Num();
   LeaveCriticalSection (&gacsTextureMapLast[dwRenderShard]);
   return dwRet;
}

/***********************************************************************************
TextureCacheRemeber - Assume that's already in the critical section.

inputs
   PTEXTURECACHE         pAdd - To add
returns
   none
*/
void TextureCacheRemember (DWORD dwRenderShard, PTEXTURECACHE pAdd)
{
   // if already top then don't care
   if (gaaTEXTURECACHELast[dwRenderShard][0] == pAdd)
      return;

   // else, insert at the top of the list
   DWORD i;
   PTEXTURECACHE pTextureTemp, pTextureTemp2;
   pTextureTemp = pAdd;
   for (i = 0; (i < NUMLASTCACHE); i++) {
      if (gaaTEXTURECACHELast[dwRenderShard][i] == pAdd) {
         // found where it was, so came to end of line
         gaaTEXTURECACHELast[dwRenderShard][i] = pTextureTemp;
         return;
      }

      // else, remember this, and insert old temp in place
      pTextureTemp2 = gaaTEXTURECACHELast[dwRenderShard][i];
      gaaTEXTURECACHELast[dwRenderShard][i] = pTextureTemp;
      if (!pTextureTemp2)
         break;   // all done
      pTextureTemp = pTextureTemp2;
   }

   // when get to the end, anything in pTextureTemp is left off the edge

}

/***********************************************************************************
TextureCacheAddDynamic - Adds a texture to the cache. This is used when dynamic
textures are created, such as for the skydome. NOTE: Assumes that the texture does
not already exist in the cache - so should call TextureCacheGetDynamic() before this
if it might.

inputs
   PCTextureMapSocket      pAdd - To add
returns
   none
*/
void TextureCacheAddDynamic (DWORD dwRenderShard, PCTextureMapSocket pAdd)
{

   // done
   EnterCriticalSection (&gacsTextureMapLast[dwRenderShard]);

   TEXTURECACHE tc;
   memset (&tc, 0, sizeof(tc));
   tc.dwCount = 1;
   tc.iLastUsed = gaiTextureTime[dwRenderShard]++;
   tc.pTexture = pAdd;
   galTEXTURECACHE[dwRenderShard].Add (&tc);  // BUGFIX - Moved lin into critical seciton

   memset (gaaTEXTURECACHELast[dwRenderShard], 0, sizeof(gaaTEXTURECACHELast[dwRenderShard]));
   gaqwTextureCacheTimeStamp[dwRenderShard]++;
   TextureCacheRemember (dwRenderShard, (PTEXTURECACHE) galTEXTURECACHE[dwRenderShard].Get(galTEXTURECACHE[dwRenderShard].Num()-1) );
   LeaveCriticalSection (&gacsTextureMapLast[dwRenderShard]);
}

   
/***********************************************************************************
TextureCacheGetDynamic - Given a GUID for a texture, this sees if it exists.
If so, it returns the texture. Used by the skydome and others for automatically
created textures.

inputs
   GUID              *pgCode - Code
   GUID              *pgSub - Sub-code
   QWORD             *pqwTextureCacheTimeStamp - If this isn't NULL, this should be
                        filled in with the last time stamp used for TextureCacheGet()
                        and TextureCacheGetDynamic(). This function will fill it
                        in with a new time stamp.
   PCTextureMapSocket      pTextureMapLast - Last time this was called, the texture map was
                        this. If the time stamps match then pTextureMapLast will
                        be returned (assuming it's not NULL).
returns
   PCTextureMap - Don't free this. Instead call TextureCacheRelease()
*/
PCTextureMapSocket TextureCacheGetDynamic (DWORD dwRenderShard, GUID *pgCode, GUID *pgSub,
                                           QWORD *pqwTextureCacheTimeStamp, PCTextureMapSocket pTextureMapLast)
{
   GUID gCode, gSub;

   // NOTE: This isn't necessarily thread safe, but because of the way that textures are cached,
   // I don't expect a problem
   if (pqwTextureCacheTimeStamp && pTextureMapLast && (*pqwTextureCacheTimeStamp == gaqwTextureCacheTimeStamp[dwRenderShard]))
      return pTextureMapLast;


   EnterCriticalSection (&gacsTextureMapLast[dwRenderShard]);

   if (pqwTextureCacheTimeStamp)
      *pqwTextureCacheTimeStamp = gaqwTextureCacheTimeStamp[dwRenderShard];

   // look in the last one accessed
   DWORD i;
   for (i = 0; i < NUMLASTCACHE; i++)
      if (gaaTEXTURECACHELast[dwRenderShard][i]) {
         gaaTEXTURECACHELast[dwRenderShard][i]->pTexture->GUIDsGet (&gCode, &gSub);
         if (IsEqualGUID(gSub, *pgSub) && IsEqualGUID(gCode, *pgCode)) {
            // found it again
            PCTextureMapSocket pRet = gaaTEXTURECACHELast[dwRenderShard][i]->pTexture;
            gaaTEXTURECACHELast[dwRenderShard][i]->dwCount++;
            gaaTEXTURECACHELast[dwRenderShard][i]->iLastUsed = gaiTextureTime[dwRenderShard]++;

            // potentially rearrange list
            if (i)
               TextureCacheRemember (dwRenderShard, gaaTEXTURECACHELast[dwRenderShard][i]);
            LeaveCriticalSection (&gacsTextureMapLast[dwRenderShard]);

            return pRet;
         }
      }

   // look in the texture map cache
   PTEXTURECACHE ptc = (PTEXTURECACHE)galTEXTURECACHE[dwRenderShard].Get(0);
   for (i = 0; i < galTEXTURECACHE[dwRenderShard].Num(); i++, ptc++) {
      ptc->pTexture->GUIDsGet (&gCode, &gSub);
      if (IsEqualGUID(gSub, *pgSub) && IsEqualGUID(gCode, *pgCode)) {
         ptc->iLastUsed = gaiTextureTime[dwRenderShard]++;
         ptc->dwCount++;
         TextureCacheRemember (dwRenderShard, ptc);
         LeaveCriticalSection (&gacsTextureMapLast[dwRenderShard]);
         return ptc->pTexture;
      }
   }

   LeaveCriticalSection (&gacsTextureMapLast[dwRenderShard]);
   return NULL;
}

/***********************************************************************************
TextureCacheGet - Look through the texture map cache for a texture of the
requested type. If it's found, return it. If not, create one.

IMPORTANT: Not doing reference counting since just keeping
the cached textures around until shut down

inputs
   PRENDERSURFACE       pRS - Use this information for the texture. the code guid,
                        sub guid, and modifications to the texture map
   QWORD             *pqwTextureCacheTimeStamp - If this isn't NULL, this should be
                        filled in with the last time stamp used for TextureCacheGet()
                        and TextureCacheGetDynamic(). This function will fill it
                        in with a new time stamp.
   PCTextureMapSocket      pTextureMapLast - Last time this was called, the texture map was
                        this. If the time stamps match then pTextureMapLast will
                        be returned (assuming it's not NULL).
returns
   PCTextureMap - Don't free this. Instead call TextureCacheRelease()
*/
PCTextureMapSocket TextureCacheGet (DWORD dwRenderShard, PRENDERSURFACE pRS,
                                    QWORD *pqwTextureCacheTimeStamp, PCTextureMapSocket pTextureMapLast)
{
   GUID gCode, gSub;
   TEXTUREMODS mods;


   // NOTE: This isn't necessarily thread safe, but because of the way that textures are cached,
   // I don't expect a problem
   if (pqwTextureCacheTimeStamp && pTextureMapLast && (*pqwTextureCacheTimeStamp == gaqwTextureCacheTimeStamp[dwRenderShard]))
      return pTextureMapLast;

   EnterCriticalSection (&gacsTextureMapLast[dwRenderShard]);
   // look in the last one accessed

   _ASSERTE (MAXRENDERSHARDS == 4); // since hardcoding all values to 1,1,1,1 at initialization
   if (pqwTextureCacheTimeStamp)
      *pqwTextureCacheTimeStamp = gaqwTextureCacheTimeStamp[dwRenderShard];

   // look in the last one accessed
   DWORD i;
   for (i = 0; i < NUMLASTCACHE; i++)
      if (gaaTEXTURECACHELast[dwRenderShard][i]) {
         gaaTEXTURECACHELast[dwRenderShard][i]->pTexture->GUIDsGet (&gCode, &gSub);
         if (IsEqualGUID(gSub, pRS->gTextureSub) && IsEqualGUID(gCode, pRS->gTextureCode)) {
            // compare the texture mods
            gaaTEXTURECACHELast[dwRenderShard][i]->pTexture->TextureModsGet (&mods);

            if (!memcmp(&mods, &pRS->TextureMods, sizeof(mods))) {
               // found it again
               PCTextureMapSocket pRet = gaaTEXTURECACHELast[dwRenderShard][i]->pTexture;
               gaaTEXTURECACHELast[dwRenderShard][i]->dwCount++;
               gaaTEXTURECACHELast[dwRenderShard][i]->iLastUsed = gaiTextureTime[dwRenderShard]++;

               // potentially rearrange list
               if (i)
                  TextureCacheRemember (dwRenderShard, gaaTEXTURECACHELast[dwRenderShard][i]);

               LeaveCriticalSection (&gacsTextureMapLast[dwRenderShard]);
               return pRet;
            }
         }
      }

   // look in the texture map cache
   PTEXTURECACHE ptc = (PTEXTURECACHE)galTEXTURECACHE[dwRenderShard].Get(0);
   for (i = 0; i < galTEXTURECACHE[dwRenderShard].Num(); i++, ptc++) {
      ptc->pTexture->GUIDsGet (&gCode, &gSub);
      if (IsEqualGUID(gSub, pRS->gTextureSub) && IsEqualGUID(gCode, pRS->gTextureCode)) {
         // compare the texture mods
         ptc->pTexture->TextureModsGet (&mods);

         if (!memcmp(&mods, &pRS->TextureMods, sizeof(mods))) {
            // found it
            ptc->dwCount++;
            ptc->iLastUsed = gaiTextureTime[dwRenderShard]++;
            TextureCacheRemember (dwRenderShard, ptc);
            LeaveCriticalSection (&gacsTextureMapLast[dwRenderShard]);
            return ptc->pTexture;

         }
      }
   }

   // create it
   PCTextureMapSocket pNew;
   pNew = TextureCreate (dwRenderShard, &pRS->gTextureCode, &pRS->gTextureSub);
   if (!pNew) {
      LeaveCriticalSection (&gacsTextureMapLast[dwRenderShard]);
      return NULL;
   }

   // set the modifications
   pNew->TextureModsSet (&pRS->TextureMods);
   TEXTURECACHE tc;
   memset (&tc, 0, sizeof(tc));
   tc.dwCount = 1;
   tc.iLastUsed = gaiTextureTime[dwRenderShard]++;
   tc.pTexture = pNew;
   galTEXTURECACHE[dwRenderShard].Add (&tc);

   // done
   memset (gaaTEXTURECACHELast[dwRenderShard], 0, sizeof(gaaTEXTURECACHELast[dwRenderShard]));
   gaqwTextureCacheTimeStamp[dwRenderShard]++;
   TextureCacheRemember (dwRenderShard, (PTEXTURECACHE)galTEXTURECACHE[dwRenderShard].Get(galTEXTURECACHE[dwRenderShard].Num()-1) );
   LeaveCriticalSection (&gacsTextureMapLast[dwRenderShard]);
   return pNew;
}

/***********************************************************************************
TextureCacheRelease - Release a texture map. Call this once for every call to
TextureCacheGet().

inputs
   PCTextureMapSocket         pTexture - To be released.
returns
   BOOL - TRUE if found, FALSE if didnt
*/
BOOL TextureCacheRelease (DWORD dwRenderShard, PCTextureMapSocket pTexture)
{
   // find it and release
   EnterCriticalSection (&gacsTextureMapLast[dwRenderShard]);
   PTEXTURECACHE ptc = (PTEXTURECACHE)galTEXTURECACHE[dwRenderShard].Get(0);
   DWORD i;
   for (i = 0; i < galTEXTURECACHE[dwRenderShard].Num(); i++, ptc++)
      if (ptc->pTexture == pTexture) {

#ifdef _DEBUG
         // NOTE: Disabling because at the moment reference counts aren't kept up to date properly
         //if (!ptc->dwCount)
         //   OutputDebugString ("\r\nReleasing texture too many times!");
#endif

         ptc->dwCount--;
         break;
      }
   LeaveCriticalSection (&gacsTextureMapLast[dwRenderShard]);

   // NOTE: if ever change this, need to modify the shadow renderer so it
   // locks and unlocks the texturecache objects as they are remembered
   return TRUE;
}


/***********************************************************************************
TextureCacheDelete - Deletes a cached texture from the list. Call this if actually
change the texture

inputs
   GUID     *pgCode, *pgSub - texture cache
   BOOL     fReallyDelete - If TRUE then really delete the texture, otherwise
            just remove it from the list
*/
void TextureCacheDelete (DWORD dwRenderShard, const GUID *pgCode, const GUID *pgSub, BOOL fReallyDelete)
{
   EnterCriticalSection (&gacsTextureMapLast[dwRenderShard]);
   // BUGFIX - clear out last texture cache kept
   memset (gaaTEXTURECACHELast[dwRenderShard], 0, sizeof(gaaTEXTURECACHELast[dwRenderShard]));
   gaqwTextureCacheTimeStamp[dwRenderShard]++;

   // look in the texture map cache
   DWORD i;
   GUID gCode, gSub;
   for (i = galTEXTURECACHE[dwRenderShard].Num()-1; i < galTEXTURECACHE[dwRenderShard].Num(); i--) {
      PTEXTURECACHE ptc = (PTEXTURECACHE)galTEXTURECACHE[dwRenderShard].Get(i);

      ptc->pTexture->GUIDsGet (&gCode, &gSub);
      if (IsEqualGUID(gCode, *pgCode) && IsEqualGUID(gSub, *pgSub)) {
#ifdef _DEBUG
         // NOTE: Disabling because at the moment reference counts aren't kept up to date properly
         //if (ptc->dwCount)
         //   OutputDebugString ("\r\nDeleting cached texture with ref count!");
#endif
         if (fReallyDelete) {
            ptc->pTexture->Delete();
         }
         galTEXTURECACHE[dwRenderShard].Remove (i);
         // keep on going because there may be several
      }
   }

   // BUGFIX - move leave critical section to end
   LeaveCriticalSection (&gacsTextureMapLast[dwRenderShard]);
}
typedef struct {
   PCObjectSurface pSurf;  // object surface
   WCHAR       szMajor[128];   // major category name
   WCHAR       szMinor[128];   // minor category name
   WCHAR       szName[128];    // name
   PCTextureMapSocket pMap;      // texture map currently using
   PCImage     pImage;     // image usaing
   HBITMAP     hBit;       // bitmap of the image - 200 x 200
   WCHAR       szCurMajorCategory[128]; // current major category shown
   WCHAR       szCurMinorCategory[128]; // current minor cateogyr shown
   BOOL        fPressedOK; // set to TRUE if press OK
   DWORD       dwRenderShard;
} TSPAGE, *PTSPAGE;


/***************************************************************************
HackTextureCreate - Just for now, fill in library with some default textures
*/
#if 0
/************************************************************************************
Default textures - for now */

// major category strings
static PWSTR gszMCFlooring = L"Flooring";
static PWSTR gszMCPictures = L"Pictures";
static PWSTR gszMCWalls = L"Walls";
static PWSTR gszMCCeiling = L"Ceilings";
static PWSTR gszMCOutside = L"Outside";

// sub-category strings
static PWSTR gszSCFlooringTiles = L"Tiles";
static PWSTR gszSCFlooringWood = L"Wood";
static PWSTR gszSCFlooringCarpet = L"Carpet";
static PWSTR gszSCPicturesBitmaps = L"Bitmaps";
static PWSTR gszSCWallsPlaster = L"Plaster";
static PWSTR gszSCWallsMetal = L"Metal";
static PWSTR gszSCWallsMasonry = L"Bricks and blocks";
static PWSTR gszSCWallsWood = L"Wood";
static PWSTR gszSCCeilingPlaster = L"Plaster";
static PWSTR gszSCOutsideGrass = L"Grass";
static PWSTR gszSCPicturesPaintings = L"Paintings";
static PWSTR gszSCPicturesPhotos = L"Photos";

// TEXTURERECORD - Global array of these to indicate what textures
// are supported by ASP. In the future, might read from a file.
typedef struct {
   PWSTR       pszMajor;   // name of the main category
   PWSTR       pszMinor;   // name of the minor category
   PWSTR       pszName;    // name of the texture
   const GUID  *pCode;     // identifying guid
   const GUID  *pSub;      // identifying guid
   DWORD       dwType;     // passed in when creating
} TEXTURERECORD, *PTEXTURERECORD;

static TEXTURERECORD gaTextureRecord[] = {
   gszMCFlooring, gszSCFlooringTiles, L"White tiles",
      &GTEXTURECODE_Tiles, &GTEXTURESUB_TilesWhite,
      0,

   gszMCFlooring, gszSCFlooringTiles, L"White (smooth) tiles",
      &GTEXTURECODE_Tiles, &GTEXTURESUB_TilesWhiteSmooth,
      1,

   gszMCFlooring, gszSCFlooringTiles, L"Red tiles",
      &GTEXTURECODE_Tiles, &GTEXTURESUB_TilesRed,
      2,

   gszMCFlooring, gszSCFlooringTiles, L"Green tiles",
      &GTEXTURECODE_Tiles, &GTEXTURESUB_TilesGreen,
      4,

   gszMCFlooring, gszSCFlooringTiles, L"Blue tiles",
      &GTEXTURECODE_Tiles, &GTEXTURESUB_TilesBlue,
      3,

   gszMCFlooring, gszSCFlooringTiles, L"Checkerboard, rough",
      &GTEXTURECODE_Tiles, &GTEXTURESUB_TilesCheckerboard,
      5,

   gszMCFlooring, gszSCFlooringTiles, L"Checkerboard, cyan",
      &GTEXTURECODE_Tiles, &GTEXTURESUB_TilesCheckerboardCyan,
      6,

   gszMCFlooring, gszSCFlooringTiles, L"Slate",
      &GTEXTURECODE_Tiles, &GTEXTURESUB_TilesSlate,
      7,

   gszMCFlooring, gszSCFlooringTiles, L"Teracotta",
      &GTEXTURECODE_Tiles, &GTEXTURESUB_TilesTeracotta,
      8,


   gszMCWalls, gszSCWallsMasonry, L"Bricks",
      &GTEXTURECODE_Tiles, &GTEXTURESUB_Brick,
      0x10000,

   gszMCWalls, gszSCWallsMasonry, L"Bricks, grey",
      &GTEXTURECODE_Tiles, &GTEXTURESUB_BrickGrey,
      0x10001,

   gszMCWalls, gszSCWallsMasonry, L"Cement block",
      &GTEXTURECODE_Tiles, &GTEXTURESUB_CementBlock,
      0x10002,

   gszMCWalls, gszSCWallsMasonry, L"Bricks, painted",
      &GTEXTURECODE_Tiles, &GTEXTURESUB_BrickPainted,
      0x10003,

   gszMCWalls, gszSCWallsMasonry, L"Cut stone",
      &GTEXTURECODE_Tiles, &GTEXTURESUB_CutStone,
      0x10004,



   gszMCWalls, gszSCWallsPlaster, L"Stucco",
      &GTEXTURECODE_TextureAlgoNoise, &GTEXTURESUB_Stucco,
      0,

   gszMCWalls, gszSCWallsPlaster, L"Rendered cement",
      &GTEXTURECODE_TextureAlgoNoise, &GTEXTURESUB_RenderedCement,
      1,

   gszMCCeiling, gszSCCeilingPlaster, L"Speckled ceiling",
      &GTEXTURECODE_TextureAlgoNoise, &GTEXTURESUB_SpeckledCeiling,
      2,

   gszMCOutside, gszSCOutsideGrass, L"Grass",
      &GTEXTURECODE_TextureAlgoNoise, &GTEXTURESUB_Grass,
      3,

   gszMCFlooring, gszSCFlooringCarpet, L"Beige carpet",
      &GTEXTURECODE_TextureAlgoNoise, &GTEXTURESUB_CarpetBeige,
      4,






   gszMCWalls, gszSCWallsMetal, L"Custom orb, Zincalum",
      &GTEXTURECODE_Corrogated, &GTEXTURESUB_CustomOrbZinc,
      0,

   gszMCWalls, gszSCWallsMetal, L"Custom orb, Red",
      &GTEXTURECODE_Corrogated, &GTEXTURESUB_CustomOrbRed,
      1,

   gszMCWalls, gszSCWallsMetal, L"Custom orb, Green",
      &GTEXTURECODE_Corrogated, &GTEXTURESUB_CustomOrbGreen,
      2,

   gszMCWalls, gszSCWallsMetal, L"Custom orb, Blue",
      &GTEXTURECODE_Corrogated, &GTEXTURESUB_CustomOrbBlue,
      3,

   gszMCWalls, gszSCWallsMetal, L"Custom orb, Creme",
      &GTEXTURECODE_Corrogated, &GTEXTURESUB_CustomOrbCreme,
      4,

   gszMCWalls, gszSCWallsMetal, L"Custom orb, Grey",
      &GTEXTURECODE_Corrogated, &GTEXTURESUB_CustomOrbGrey,
      5,

   gszMCWalls, gszSCWallsMetal, L"Custom orb, Black",
      &GTEXTURECODE_Corrogated, &GTEXTURESUB_CustomOrbBlack,
      6,

   gszMCWalls, gszSCWallsMetal, L"Custom orb, Cyan",
      &GTEXTURECODE_Corrogated, &GTEXTURESUB_CustomOrbCyan,
      7,

   gszMCWalls, gszSCWallsMetal, L"Custom orb, White",
      &GTEXTURECODE_Corrogated, &GTEXTURESUB_CustomOrbWhite,
      8,

   gszMCFlooring, gszSCFlooringWood, L"Hardwood floor, dark",
      &GTEXTURECODE_WoodPlanks, &GTEXTURESUB_HardwoodDark,
      0,

   gszMCFlooring, gszSCFlooringWood, L"Hardwood floor, light",
      &GTEXTURECODE_WoodPlanks, &GTEXTURESUB_HardwoodLight,
      1,

   gszMCFlooring, gszSCFlooringWood, L"Decking",
      &GTEXTURECODE_WoodPlanks, &GTEXTURESUB_Decking,
      2,

   gszMCWalls, gszSCWallsWood, L"Plywood",
      &GTEXTURECODE_WoodPlanks, &GTEXTURESUB_Plywood,
      3,

   gszMCWalls, gszSCWallsWood, L"Painted planks",
      &GTEXTURECODE_WoodPlanks, &GTEXTURESUB_PlanksPainted,
      4,

   gszMCWalls, gszSCWallsMetal, L"Square orb, Zincalum",
      &GTEXTURECODE_Corrogated, &GTEXTURESUB_SquareOrbZinc,
      0x10000,

   gszMCWalls, gszSCWallsMetal, L"Square orb, Red",
      &GTEXTURECODE_Corrogated, &GTEXTURESUB_SquareOrbRed,
      0x10001,

   gszMCWalls, gszSCWallsMetal, L"Square orb, Green",
      &GTEXTURECODE_Corrogated, &GTEXTURESUB_SquareOrbGreen,
      0x10002,

   gszMCWalls, gszSCWallsMetal, L"Square orb, Blue",
      &GTEXTURECODE_Corrogated, &GTEXTURESUB_SquareOrbBlue,
      0x10003,

   gszMCWalls, gszSCWallsMetal, L"Square orb, Creme",
      &GTEXTURECODE_Corrogated, &GTEXTURESUB_SquareOrbCreme,
      0x10004,

   gszMCWalls, gszSCWallsMetal, L"Square orb, Grey",
      &GTEXTURECODE_Corrogated, &GTEXTURESUB_SquareOrbGrey,
      0x10005,

   gszMCWalls, gszSCWallsMetal, L"Square orb, Black",
      &GTEXTURECODE_Corrogated, &GTEXTURESUB_SquareOrbBlack,
      0x10006,

   gszMCWalls, gszSCWallsMetal, L"Square orb, Cyan",
      &GTEXTURECODE_Corrogated, &GTEXTURESUB_SquareOrbCyan,
      0x10007,

   gszMCWalls, gszSCWallsMetal, L"Square orb, White",
      &GTEXTURECODE_Corrogated, &GTEXTURESUB_SquareOrbWhite,
      0x10008,

   gszMCPictures, gszSCPicturesBitmaps, L"Printer",
      &GTEXTURECODE_Bitmap, &GTEXTURESUB_Printer,
      IDB_PRINTER,

   gszMCPictures, gszSCPicturesBitmaps, L"Help",
      &GTEXTURECODE_Bitmap, &GTEXTURESUB_Help,
      IDB_HELP,

   gszMCPictures, gszSCPicturesBitmaps, L"Dialog box",
      &GTEXTURECODE_Bitmap, &GTEXTURESUB_DialogBox,
      IDB_DIALOGBOX,

   gszMCPictures, gszSCPicturesBitmaps, L"New",
      &GTEXTURECODE_Bitmap, &GTEXTURESUB_New,
      IDB_NEW,

   gszMCPictures, gszSCPicturesPhotos, L"Letchworth State Park",
      &GTEXTURECODE_JPEG, &GTEXTURESUB_PhotoLetchworth,
      IDR_PHOTOLETCHWORTH,

   gszMCPictures, gszSCPicturesPaintings, L"Wallaroo - Mitch",
      &GTEXTURECODE_JPEG, &GTEXTURESUB_PaintingMitch,
      IDR_PAINTINGMITCH,

};

void HackTextureCreate (void)
{
   DWORD i, dwNum;
   dwNum = sizeof(gaTextureRecord) / sizeof(TEXTURERECORD);
   for (i = 0; i < dwNum; i++) {
      PTEXTURERECORD pt = &gaTextureRecord[i];

      PCTextCreatorSocket pts;
      pts = CreateTextureCreator (pt->pCode, pt->dwType, NULL);
      if (!pts)
         continue;

      PCMMLNode2 pNode;
      pNode = pts->MMLTo ();
      pts->Delete ();
      if (!pNode)
         continue;

      AttachDateTimeToMML(pNode);

      LibraryTextures(FALSE)->ItemAdd (pt->pszMajor, pt->pszMinor, pt->pszName,
         pt->pCode, pt->pSub, pNode);
   }
}
#endif


/****************************************************************************
UniqueTextureName - Given a name, comes up with a unique one that wont
conflict with other textures.

inputs
   PWSTR       pszMajor, pszMinor - Categories
   PWSTR       pszName - Initially filled with name, and then modified
   PCLibrary   pLibrary - If not NULL, the user library is replaced with pLibrary
returns
   none
*/
void UniqueTextureName (DWORD dwRenderShard, PWSTR pszMajor, PWSTR pszMinor, PWSTR pszName, PCLibrary pLibrary = NULL)
{
   PCLibraryCategory plc = pLibrary ?
      pLibrary->CategoryGet (L"Textures" /*gpszTextures*/) :
      LibraryTextures(dwRenderShard, TRUE);
   DWORD i;
   for (i = 0; ; i++) {
      if (i) {
         PWSTR pszCur;
         // remove numbers from end
         for (pszCur = pszName  + (wcslen(pszName)-1); (pszCur >= pszName) && (*pszCur >= L'0') && (*pszCur <= L'9'); pszCur--)
            pszCur[0] = 0;

         // remove spaces
         for (pszCur = pszName  + (wcslen(pszName)-1); (pszCur >= pszName) && (*pszCur == L' '); pszCur--)
            pszCur[0] = 0;

         // append space and number
         swprintf (pszName + wcslen(pszName), L" %d", (int) i+1);
      }

      // if it matches try again
      if (LibraryTextures(dwRenderShard, FALSE)->ItemGet(pszMajor, pszMinor, pszName))
         continue;
      if (plc->ItemGet(pszMajor, pszMinor, pszName))
         continue;

      // no match
      return;
   }
}


/**************************************************************************
TextureRenamePage
*/
BOOL TextureRenamePage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PTSPAGE pt = (PTSPAGE) pPage->m_pUserData;
   DWORD dwRenderShard = pt->dwRenderShard;
   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         PCEscControl pControl;

         pControl = pPage->ControlFind (L"name");
         if (pControl)
            pControl->AttribSet (Text(), pt->szName);
         pControl = pPage->ControlFind (L"category");
         if (pControl)
            pControl->AttribSet (Text(), pt->szMajor);
         pControl = pPage->ControlFind (L"subcategory");
         if (pControl)
            pControl->AttribSet (Text(), pt->szMinor);
      }
      break;

   case ESCM_LINK:
      {
         PESCMLINK p = (PESCMLINK) pParam;
         if (!p->psz)
            break;
         if (!_wcsicmp(p->psz, L"ok")) {
            WCHAR szMajor[128], szMinor[128], szName[128];

            PCEscControl pControl;

            szMajor[0] = szMinor[0] = szName[0] = 0;
            DWORD dwNeeded;
            pControl = pPage->ControlFind (L"name");
            if (pControl)
               pControl->AttribGet (Text(), szName, sizeof(szName), &dwNeeded);
            pControl = pPage->ControlFind (L"category");
            if (pControl)
               pControl->AttribGet (Text(), szMajor, sizeof(szName), &dwNeeded);
            pControl = pPage->ControlFind (L"subcategory");
            if (pControl)
               pControl->AttribGet (Text(), szMinor, sizeof(szName), &dwNeeded);

            if (!szMajor[0] || !szMinor[0] || !szName[0]) {
               pPage->MBWarning (L"You cannot leave the category, sub-category, or name blank.");
               return TRUE;
            }

            GUID gCode, gSub;
            if (TextureGUIDsFromName(dwRenderShard, szMajor, szMinor, szName, &gCode, &gSub)) {
               pPage->MBWarning (L"That name already exists.",
                  L"Your texture must have a unique name.");
               return TRUE;
            }

            // ok, rename it
            LibraryTextures(dwRenderShard, FALSE)->ItemRename (pt->szMajor, pt->szMinor, pt->szName,
               szMajor, szMinor, szName);
            LibraryTextures(dwRenderShard, TRUE)->ItemRename (pt->szMajor, pt->szMinor, pt->szName,
               szMajor, szMinor, szName);

            wcscpy (pt->szMajor, szMajor);
            wcscpy (pt->szMinor, szMinor);
            wcscpy (pt->szName, szName);

            break;
         }
      }
      break;
   }

   return DefPage (pPage, dwMessage, pParam);
}

/****************************************************************************
TextureNewPage
*/
BOOL TextureNewPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   switch (dwMessage) {
   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Create a new texture from scratch";
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"IFDEBUG")) {
#ifdef WORKONTEXTURES
            p->pszSubString = L"";
#else
            p->pszSubString = L"<comment>";
#endif
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"ENDIFDEBUG")) {
#ifdef WORKONTEXTURES
            p->pszSubString = L"";
#else
            p->pszSubString = L"</comment>";
#endif
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}


/****************************************************************************
TextureLibraryPage
*/
BOOL TextureLibraryPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   static WCHAR sTextureTemp[16];

   PTSPAGE pt = (PTSPAGE) pPage->m_pUserData;
   DWORD dwRenderShard = pt->dwRenderShard;
   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         // fill in the list of categories
         pPage->Message (ESCM_USER+91);

         // new texture so set all the parameters
         pPage->Message (ESCM_USER+84);
      }
      break;

   case ESCM_USER+91: // update the major categoriy list
      {
         CListFixed list;
         DWORD i;
         CMem mem;
         PWSTR psz;
         PCEscControl pControl;
         TextureEnumMajor (dwRenderShard, &list);
         MemZero (&mem);
         for (i = 0; i < list.Num(); i++) {
            psz = *((PWSTR*) list.Get(i));
            MemCat (&mem, L"<elem name=\"");
            MemCatSanitize (&mem, psz);
            MemCat (&mem, L"\">");
            MemCatSanitize (&mem, psz);
            MemCat (&mem, L"</elem>");
         }
         pControl = pPage->ControlFind (L"major");
         if (pControl) {
            pControl->Message (ESCM_COMBOBOXRESETCONTENT);

            ESCMCOMBOBOXADD add;
            memset (&add, 0, sizeof(add));
            add.dwInsertBefore = -1;
            add.pszMML = (PWSTR) mem.p;
            pControl->Message (ESCM_COMBOBOXADD, &add);

            ESCMCOMBOBOXSELECTSTRING sel;
            memset (&sel, 0, sizeof(sel));
            sel.fExact = TRUE;
            sel.psz = pt->szMajor;
            pControl->Message (ESCM_COMBOBOXSELECTSTRING, &sel);
         }
      }
      return TRUE;

   case ESCM_USER+82: // set the minor category box
      {
         // if we're showing the right one then don't care
         if (!_wcsicmp(pt->szCurMajorCategory, pt->szMajor))
            return TRUE;

         // if changed, then names will be changed to
         pt->szCurMinorCategory[0] = 0;
         wcscpy (pt->szCurMajorCategory, pt->szMajor);

         // fill in the list of categories
         CListFixed list;
         DWORD i;
         CMem mem;
         PWSTR psz;
         PCEscControl pControl;
         TextureEnumMinor (dwRenderShard, &list, pt->szMajor);
         MemZero (&mem);
         for (i = 0; i < list.Num(); i++) {
            psz = *((PWSTR*) list.Get(i));
            MemCat (&mem, L"<elem name=\"");
            MemCatSanitize (&mem, psz);
            MemCat (&mem, L"\">");
            MemCatSanitize (&mem, psz);
            MemCat (&mem, L"</elem>");
         }
         pControl = pPage->ControlFind (L"minor");
         if (pControl) {
            pControl->Message (ESCM_COMBOBOXRESETCONTENT);

            ESCMCOMBOBOXADD add;
            memset (&add, 0, sizeof(add));
            add.dwInsertBefore = -1;
            add.pszMML = (PWSTR) mem.p;
            pControl->Message (ESCM_COMBOBOXADD, &add);

            ESCMCOMBOBOXSELECTSTRING sel;
            memset (&sel, 0, sizeof(sel));
            sel.fExact = TRUE;
            sel.psz = pt->szMinor;
            pControl->Message (ESCM_COMBOBOXSELECTSTRING, &sel);
         }
      }
      break;


   case ESCM_USER+83: // set the names list box
      {
         // if we're showing the right one then don't care
         if (!_wcsicmp(pt->szCurMinorCategory, pt->szMinor))
            return TRUE;

         // if changed, then names will be changed to
         wcscpy (pt->szCurMinorCategory, pt->szMinor);

         // fill in the list of names
         CListFixed list;
         DWORD i;
         CMem mem;
         PWSTR psz;
         PCEscControl pControl;
         TextureEnumItems (dwRenderShard, &list, pt->szMajor, pt->szMinor);
         MemZero (&mem);
         BOOL fBold;
         for (i = 0; i < list.Num(); i++) {
            psz = *((PWSTR*) list.Get(i));
            MemCat (&mem, L"<elem name=\"");
            MemCatSanitize (&mem, psz);
            MemCat (&mem, L"\"><small>");

            // if it's a custom texture then bold it
            if (LibraryTextures(dwRenderShard, TRUE)->ItemGet (pt->szMajor, pt->szMinor, psz))
               fBold = TRUE;
            else
               fBold = FALSE;
            if (fBold)
               MemCat (&mem, L"<font color=#008000>");
            MemCatSanitize (&mem, psz);
            if (fBold)
               MemCat (&mem, L"</font>");
            MemCat (&mem, L"</small></elem>");
         }
         pControl = pPage->ControlFind (L"name");
         if (pControl) {
            pControl->Message (ESCM_LISTBOXRESETCONTENT);

            ESCMLISTBOXADD add;
            memset (&add, 0, sizeof(add));
            add.dwInsertBefore = -1;
            add.pszMML = (PWSTR) mem.p;
            pControl->Message (ESCM_LISTBOXADD, &add);

            ESCMLISTBOXSELECTSTRING sel;
            memset (&sel, 0, sizeof(sel));
            sel.fExact = TRUE;
            sel.psz = pt->szName;
            pControl->Message (ESCM_LISTBOXSELECTSTRING, &sel);
         }
      }
      break;

   case ESCM_USER+84:  // we have a new texture so set the parameters
      {
         // set the minor category box if necessary
         pPage->Message (ESCM_USER+82);

         // set the name box if necessary
         pPage->Message (ESCM_USER+83);

         // set it in the comboboxes and list boxes
         PCEscControl pControl;
         pControl = pPage->ControlFind (L"major");
         if (pControl) {
            ESCMCOMBOBOXSELECTSTRING sel;
            memset (&sel, 0, sizeof(sel));
            sel.fExact = TRUE;
            sel.psz = pt->szMinor;
            pControl->Message (ESCM_COMBOBOXSELECTSTRING, &sel);
         }
         pControl = pPage->ControlFind (L"minor");
         if (pControl) {
            ESCMCOMBOBOXSELECTSTRING sel;
            memset (&sel, 0, sizeof(sel));
            sel.fExact = TRUE;
            sel.psz = pt->szMinor;
            pControl->Message (ESCM_COMBOBOXSELECTSTRING, &sel);
         }
         pControl = pPage->ControlFind (L"name");
         if (pControl) {
            ESCMLISTBOXSELECTSTRING sel;
            memset (&sel, 0, sizeof(sel));
            sel.fExact = TRUE;
            sel.psz = pt->szName;
            pControl->Message (ESCM_LISTBOXSELECTSTRING, &sel);
         }

         // redo the bitmap
         pPage->Message (ESCM_USER+85);
      }
      return TRUE;

   case ESCM_USER+85:   // redo the bitmap
      {
         // update the buttons for editing if custom
         BOOL  fBuiltIn;
         PCEscControl pControl;
         if (LibraryTextures(dwRenderShard, FALSE)->ItemGet (pt->szMajor, pt->szMinor, pt->szName))
            fBuiltIn = TRUE;
         else
            fBuiltIn = FALSE;
#ifdef WORKONTEXTURES
         fBuiltIn = FALSE; // allow to modify all
#endif
         pControl = pPage->ControlFind (L"edit");
         if (pControl)
            pControl->Enable (!fBuiltIn);
         pControl = pPage->ControlFind (L"rename");
         if (pControl)
            pControl->Enable (!fBuiltIn);
         pControl = pPage->ControlFind (L"remove");
         if (pControl)
            pControl->Enable (!fBuiltIn);


         // redo the bitmap
         pt->pMap->FillImage (pt->pImage, !IsImage(pt->szMajor, pt->szMinor), FALSE, pt->pSurf->m_afTextureMatrix,
            &pt->pSurf->m_mTextureMatrix, &pt->pSurf->m_Material);
         HDC hDC;
         hDC = GetDC (pPage->m_pWindow->m_hWnd);
         if (pt->hBit)
            DeleteObject (pt->hBit);
         pt->hBit = pt->pImage->ToBitmap (hDC);
         ReleaseDC (pPage->m_pWindow->m_hWnd, hDC);

         pControl = pPage->ControlFind (L"image");

         WCHAR szTemp[32];
         swprintf (szTemp, L"%lx", (__int64) pt->hBit);
         pControl->AttribSet (L"hbitmap", szTemp);

#ifdef _DEBUG
         // write out guids
         FILE *f;
         f = fopen("c:\\texture.txt", "wt");
         if (f) {
            fprintf (f, "DEFINE_GUID(GTEXTURECODE,\n"
               "0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x);\n",
               (DWORD) pt->pSurf->m_gTextureCode.Data1, 
               (DWORD) pt->pSurf->m_gTextureCode.Data2, 
               (DWORD) pt->pSurf->m_gTextureCode.Data3, 
               (DWORD) pt->pSurf->m_gTextureCode.Data4[0],
               (DWORD) pt->pSurf->m_gTextureCode.Data4[1],
               (DWORD) pt->pSurf->m_gTextureCode.Data4[2],
               (DWORD) pt->pSurf->m_gTextureCode.Data4[3],
               (DWORD) pt->pSurf->m_gTextureCode.Data4[4],
               (DWORD) pt->pSurf->m_gTextureCode.Data4[5],
               (DWORD) pt->pSurf->m_gTextureCode.Data4[6],
               (DWORD) pt->pSurf->m_gTextureCode.Data4[7]
               );
            fprintf (f, "DEFINE_GUID(GTEXTURESUB,\n"
               "0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x);\n",
               (DWORD) pt->pSurf->m_gTextureSub.Data1, 
               (DWORD) pt->pSurf->m_gTextureSub.Data2, 
               (DWORD) pt->pSurf->m_gTextureSub.Data3, 
               (DWORD) pt->pSurf->m_gTextureSub.Data4[0],
               (DWORD) pt->pSurf->m_gTextureSub.Data4[1],
               (DWORD) pt->pSurf->m_gTextureSub.Data4[2],
               (DWORD) pt->pSurf->m_gTextureSub.Data4[3],
               (DWORD) pt->pSurf->m_gTextureSub.Data4[4],
               (DWORD) pt->pSurf->m_gTextureSub.Data4[5],
               (DWORD) pt->pSurf->m_gTextureSub.Data4[6],
               (DWORD) pt->pSurf->m_gTextureSub.Data4[7]
               );

            fclose (f);
         }
#endif
      }
      return TRUE;

   case ESCM_USER + 86: // create a new texture using the new major, minor, and name
      {
         // get the guids
         if (!TextureGUIDsFromName (dwRenderShard, pt->szMajor, pt->szMinor, pt->szName, &pt->pSurf->m_gTextureCode, &pt->pSurf->m_gTextureSub))
            return FALSE;  // error

         // since just created a new texture, set up some values
         pt->pSurf->m_TextureMods.cTint = RGB(0xff,0xff,0xff);
         pt->pSurf->m_TextureMods.wBrightness = 0x1000;
         pt->pSurf->m_TextureMods.wContrast = 0x1000;
         pt->pSurf->m_TextureMods.wSaturation = 0x1000;
         pt->pSurf->m_TextureMods.wHue = 0;
         pt->pSurf->m_afTextureMatrix[0][0] = pt->pSurf->m_afTextureMatrix[1][1] = 1.0;
         pt->pSurf->m_afTextureMatrix[0][1] = pt->pSurf->m_afTextureMatrix[1][0] = 0.0;
         pt->pSurf->m_mTextureMatrix.Identity();

         // get the teture
         if (pt->pMap)
            pt->pMap->Delete();
         pt->pMap = TextureCreate (dwRenderShard, &pt->pSurf->m_gTextureCode, &pt->pSurf->m_gTextureSub);
         if (!pt->pMap)
            return FALSE;
         pt->pMap->TextureModsSet (&pt->pSurf->m_TextureMods);

         // get the texture's default width and height
         fp fx, fy;
         pt->pMap->DefScaleGet (&fx, &fy);
         pt->pSurf->m_afTextureMatrix[0][0] = 1.0 / fx;
         pt->pSurf->m_afTextureMatrix[1][1] = 1.0 / fy;
         pt->pMap->MaterialGet (0, &pt->pSurf->m_Material);

         // and make sure to refresh
         pPage->Message (ESCM_USER+84);
      }
      return TRUE;

   case ESCM_LINK:
      {
         PESCMLINK p = (PESCMLINK) pParam;
         // BUGFIX - If modifying a texture through the library, recommend that edit
         // through the painting view
         if (p->psz && !_wcsicmp(p->psz, L"edit")) {
            GUID gCode, gSub;
            if (!TextureGUIDsFromName (dwRenderShard, pt->szMajor, pt->szMinor, pt->szName, &gCode, &gSub))
               break;

            if (IsEqualGUID (gCode, GTEXTURECODE_ImageFile)) {
               pPage->MBInformation (L"While you can still edit an image texture in the "
                  L"texture library, you may find it easier to modify from the "
                  L"\"Painting view\".");
            }

         }
      }
      break;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;

         if (!p->pControl || !p->pControl->m_pszName)
            break;

         if (!_wcsicmp(p->pControl->m_pszName, L"copyedit")) {
            // get it
            GUID gCode, gSub;
            PCMMLNode2 pNode;
            if (!TextureGUIDsFromName (dwRenderShard, pt->szMajor, pt->szMinor, pt->szName, &gCode, &gSub))
               return TRUE;

            if (IsEqualGUID (gCode, GTEXTURECODE_ImageFile)) {
               pPage->MBInformation (L"Copying the texture will not duplicate the .bmp or "
                  L".jpg file used by it.",
                  L"Any changes you make to the image file will affect all textures that use the image file.");

               pPage->MBInformation (L"While you can still edit an image texture in the "
                  L"texture library, you may find it easier to modify from the "
                  L"\"Painting view\".");
            }

            pNode = LibraryTextures(dwRenderShard, FALSE)->ItemGet (pt->szMajor, pt->szMinor, pt->szName);
            if (!pNode)
               pNode = LibraryTextures(dwRenderShard, TRUE)->ItemGet  (pt->szMajor, pt->szMinor, pt->szName);
            if (!pNode)
               return TRUE;

            // clone
            pNode = pNode->Clone();
            if (!pNode)
               return TRUE;

            // find a new name
            UniqueTextureName (dwRenderShard, pt->szMajor, pt->szMinor, pt->szName);

            GUIDGen(&gSub);

            //add it
#ifdef WORKONTEXTURES
            LibraryTextures(dwRenderShard, FALSE)->ItemAdd (pt->szMajor, pt->szMinor, pt->szName, &gCode, &gSub, pNode);
#else
            LibraryTextures(dwRenderShard, TRUE)->ItemAdd (pt->szMajor, pt->szMinor, pt->szName, &gCode, &gSub, pNode);
#endif

            // clean out the thumbnail just in case
            ThumbnailGet()->ThumbnailRemove (&gCode, &gSub);

            TextureGUIDsFromName (dwRenderShard, pt->szMajor, pt->szMinor, pt->szName,
               &pt->pSurf->m_gTextureCode, &pt->pSurf->m_gTextureSub);

            // update lists and selection
            pt->szCurMinorCategory[0] = 0;   // so will refresh
            pt->szCurMajorCategory[0] = 0;   // so refreshes

            BOOL fControl = (GetKeyState (VK_CONTROL) < 0);
            pPage->MBSpeakInformation (L"Texture copied.");

            if (fControl)   // BUGFIX - hold down control to redo same page
               pPage->Exit (RedoSamePage());
            else
               pPage->Exit (L"edit");
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"rename")) {
            // get the angle
            CEscWindow cWindow;
            RECT r;
            DialogBoxLocation2 (pPage->m_pWindow->m_hWnd, &r);
            cWindow.Init (ghInstance, pPage->m_pWindow->m_hWnd, EWS_FIXEDSIZE | EWS_AUTOHEIGHT | EWS_NOTITLE, &r);
            PWSTR pszRet;
            pszRet = cWindow.PageDialog (ghInstance, IDR_MMLTEXTURERENAME, TextureRenamePage, pt);

            //if (!_wcsicmp(pszRet, L"ok")) { // BUGFIX - Because if hit enter returns [close]
               pt->szCurMajorCategory[0] = 0;
               pt->szCurMinorCategory[0] = 0;
               pPage->Message (ESCM_USER+91);   // new major categories
               pPage->Message (ESCM_USER+86);   // other stuff may have changed
               // BUGFIX - Was +84, but try 86 to see if fixes occasional bug
            //}

            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"remove")) {
            if (IDYES != pPage->MBYesNo (L"Are you sure you wish to permenantly delete this texture?"))
               return TRUE;

            // clear the cache
            GUID gCode, gSub;
            TextureGUIDsFromName (dwRenderShard, pt->szMajor, pt->szMinor, pt->szName, &gCode, &gSub);
            TextureAlgoCacheRemove (dwRenderShard, &gCode, &gSub);
            TextureCacheDelete (dwRenderShard, &gCode, &gSub);


            // remove it from either one
            LibraryTextures(dwRenderShard, FALSE)->ItemRemove (pt->szMajor, pt->szMinor, pt->szName);
            LibraryTextures(dwRenderShard, TRUE)->ItemRemove (pt->szMajor, pt->szMinor, pt->szName);

            // clean out the thumbnail just in case
            ThumbnailGet()->ThumbnailRemove (&gCode, &gSub);


            // new texture
            CListFixed list;
            PWSTR psz;
            TextureEnumItems (dwRenderShard, &list, pt->szMajor, pt->szMinor);
            if (!list.Num()) {
               // minor no longer exists
               TextureEnumMinor (dwRenderShard, &list, pt->szMajor);
               if (!list.Num()) {
                  // minor no longer exists
                  TextureEnumMajor (dwRenderShard, &list);
                  psz = *((PWSTR*) list.Get(0));
                  wcscpy (pt->szMajor, psz);

                  TextureEnumMinor (dwRenderShard, &list, pt->szMajor);
                  // will have something there
               }

               psz = *((PWSTR*) list.Get(0));
               wcscpy (pt->szMinor, psz);
               TextureEnumItems (dwRenderShard, &list, pt->szMajor, pt->szMinor);
               // will have something there
            }
            psz = *((PWSTR*) list.Get(0));
            wcscpy(pt->szName, psz);
            pt->szCurMajorCategory[0] = 0;
            pt->szCurMinorCategory[0] = 0;
            TextureGUIDsFromName (dwRenderShard, pt->szMajor, pt->szMinor, pt->szName,
               &pt->pSurf->m_gTextureCode, &pt->pSurf->m_gTextureSub);
            pPage->Message (ESCM_USER+91);   // new major categories
            pPage->Message (ESCM_USER+86);   // other stuff may have changed
               // BUGFIX - Was +84 but changed to +86

            pPage->MBSpeakInformation (L"Texture removed.");
            return TRUE;
         }
      }
      break;

   case ESCN_COMBOBOXSELCHANGE:
      {
         PESCNCOMBOBOXSELCHANGE p = (PESCNCOMBOBOXSELCHANGE) pParam;
         if (!p->pControl->m_pszName)
            break;

         // if the major ID changed then pick the first minor and texture that
         // comes to mind
         if (!_wcsicmp(p->pControl->m_pszName, L"major")) {
            // if it hasn't reall change then ignore
            if (!_wcsicmp(pt->szMajor, p->pszName))
               return TRUE;

            wcscpy (pt->szMajor, p->pszName);

            CListFixed list;
            PWSTR psz;

            // first minor we find
            TextureEnumMinor (dwRenderShard, &list, pt->szMajor);
            psz = *((PWSTR*) list.Get(0));
            wcscpy (pt->szMinor, psz);

            // first texture we find
            TextureEnumItems (dwRenderShard, &list, pt->szMajor, pt->szMinor);
            psz = *((PWSTR*) list.Get(0));
            wcscpy(pt->szName, psz);

            pPage->Message (ESCM_USER + 86);
            return TRUE;
         }
         if (!_wcsicmp(p->pControl->m_pszName, L"minor")) {
            // if it hasn't reall change then ignore
            if (!_wcsicmp(pt->szMinor, p->pszName))
               return TRUE;

            wcscpy (pt->szMinor, p->pszName);

            CListFixed list;
            PWSTR psz;

            // first texture we find
            TextureEnumItems (dwRenderShard, &list, pt->szMajor, pt->szMinor);
            if (list.Num()) {
               psz = *((PWSTR*) list.Get(0));
               wcscpy(pt->szName, psz);
            }
            else
               pt->szName[0] = 0;

            pPage->Message (ESCM_USER + 86);
            return TRUE;
         }
      }
      break;

   case ESCN_LISTBOXSELCHANGE:
      {
         PESCNLISTBOXSELCHANGE p = (PESCNLISTBOXSELCHANGE) pParam;
         if (!p->pControl->m_pszName)
            break;

         // if the major ID changed then pick the first minor and texture that
         // comes to mind
         if (!_wcsicmp(p->pControl->m_pszName, L"name")) {
            // if it hasn't reall change then ignore
            if (!_wcsicmp(pt->szName, p->pszName))
               return TRUE;

            wcscpy (pt->szName, p->pszName);

            pPage->Message (ESCM_USER + 86);
            return TRUE;
         }
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"HBITMAP")) {
            swprintf (sTextureTemp, L"%lx", (__int64) pt->hBit);
            p->pszSubString = sTextureTemp;
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Texture library";
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}



/****************************************************************************
TextureLibraryDialog - This brings up the UI for changing the library

inputs
   HWND           hWnd - Window to create this over
returns
   BOOL - TRUE if the user presses OK, FALSE if not
*/
BOOL TextureLibraryDialog (DWORD dwRenderShard, HWND hWnd)
{
   CEscWindow cWindow;
   RECT r;
   DialogBoxLocation (hWnd, &r);
   CImage   Image;
   Image.Init (200, 200, RGB(0xff,0xff,0xff));

   cWindow.Init (ghInstance, hWnd, EWS_FIXEDSIZE, &r);
   PWSTR pszRet;

   // set up the info
   TSPAGE t;
   memset (&t, 0, sizeof(t));
   CObjectSurface cs;
   t.pSurf = &cs;
   t.pMap = NULL;
   t.pImage = &Image;
   t.hBit = NULL;
   t.dwRenderShard = dwRenderShard;
   
   // couldn't find, so start out with something
   CListFixed list;
   PWSTR psz;

   // first major we find
   TextureEnumMajor (dwRenderShard, &list);
   psz = *((PWSTR*) list.Get(0));
   wcscpy (t.szMajor, psz);

   // first minor we find
   TextureEnumMinor (dwRenderShard, &list, t.szMajor);
   psz = *((PWSTR*) list.Get(0));
   wcscpy (t.szMinor, psz);

   // first texture we find
   TextureEnumItems (dwRenderShard, &list, t.szMajor, t.szMinor);
   psz = *((PWSTR*) list.Get(0));
   wcscpy(t.szName, psz);

newtext:
   memset (&cs, 0, sizeof(cs));
   cs.m_TextureMods.cTint = RGB(0xff,0xff,0xff);
   cs.m_TextureMods.wBrightness = 0x1000;
   cs.m_TextureMods.wContrast = 0x1000;
   cs.m_TextureMods.wSaturation = 0x1000;
   cs.m_TextureMods.wHue = 0;
   cs.m_afTextureMatrix[0][0] = cs.m_afTextureMatrix[1][1] = 1.0;
   cs.m_afTextureMatrix[0][1] = cs.m_afTextureMatrix[1][0] = 0.0;
   cs.m_mTextureMatrix.Identity();

   // get the guids
   if (!TextureGUIDsFromName (dwRenderShard, t.szMajor, t.szMinor, t.szName, &cs.m_gTextureCode, &cs.m_gTextureSub))
      return FALSE;  // error

   // get the teture
   if (t.pMap)
      t.pMap->Delete();
   if (t.hBit)
      DeleteObject (t.hBit);
   t.hBit = NULL;
   t.pMap = TextureCreate (dwRenderShard, &cs.m_gTextureCode, &cs.m_gTextureSub);
   if (!t.pMap)
      return FALSE;
   t.pMap->TextureModsSet (&cs.m_TextureMods);

   // get the texture's default width and height
   fp fx, fy;
   t.pMap->DefScaleGet (&fx, &fy);
   cs.m_afTextureMatrix[0][0] = 1.0 / fx;
   cs.m_afTextureMatrix[1][1] = 1.0 / fy;

   HDC hDC;
   hDC = GetDC (hWnd);
   if (t.hBit)
      DeleteObject (t.hBit);
   t.hBit = t.pImage->ToBitmap (hDC);
   ReleaseDC (hWnd, hDC);


   // start with the first page
mainpage:
   // BUGFIX - Set curmajor and minor to 0 so refreshes
   t.szCurMajorCategory[0] = 0;
   t.szCurMinorCategory[0] = 0;
   pszRet = cWindow.PageDialog (ghInstance, IDR_MMLTEXTURELIBRARY, TextureLibraryPage, &t);
mainpage2:
   if (pszRet && !_wcsicmp(pszRet, RedoSamePage()))
      goto mainpage;
   if (pszRet && !_wcsicmp(pszRet, L"newtext")) {
      pszRet = cWindow.PageDialog (ghInstance, IDR_MMLTEXTURENEW, TextureNewPage, &t);
      if (!pszRet)
         goto mainpage2;

      if (!_wcsicmp(pszRet, Back()))
         goto mainpage;

      if ((pszRet[0] != L'X') || (pszRet[1] != L'X') || (pszRet[2] != L'X'))
         goto mainpage2;

      // else chouse options
      GUID  gChoose;
      if (!_wcsicmp(pszRet, L"XXXbmpresource"))
         gChoose = GTEXTURECODE_Bitmap;
      else if (!_wcsicmp(pszRet, L"XXXimagefile"))
         gChoose = GTEXTURECODE_ImageFile;
      else if (!_wcsicmp(pszRet, L"XXXjpgresource"))
         gChoose = GTEXTURECODE_JPEG;
      else if (!_wcsicmp(pszRet, L"XXXcorrogated"))
         gChoose = GTEXTURECODE_Corrogated;
      else if (!_wcsicmp(pszRet, L"XXXnoise"))
         gChoose = GTEXTURECODE_TextureAlgoNoise;
      else if (!_wcsicmp(pszRet, L"XXXmarble"))
         gChoose = GTEXTURECODE_Marble;
      else if (!_wcsicmp(pszRet, L"XXXfabric"))
         gChoose = GTEXTURECODE_Fabric;
      else if (!_wcsicmp(pszRet, L"XXXgrass"))
         gChoose = GTEXTURECODE_GrassTussock;
      else if (!_wcsicmp(pszRet, L"XXXfaceomatic"))
         gChoose = GTEXTURECODE_Faceomatic;
      else if (!_wcsicmp(pszRet, L"XXXmix"))
         gChoose = GTEXTURECODE_Mix;
      else if (!_wcsicmp(pszRet, L"XXXleaflitter"))
         gChoose = GTEXTURECODE_LeafLitter;
      else if (!_wcsicmp(pszRet, L"XXXtext"))
         gChoose = GTEXTURECODE_Text;
      else if (!_wcsicmp(pszRet, L"XXXtreebark"))
         gChoose = GTEXTURECODE_TreeBark;
      else if (!_wcsicmp(pszRet, L"XXXbranch"))
         gChoose = GTEXTURECODE_Branch;
      else if (!_wcsicmp(pszRet, L"XXXiris"))
         gChoose = GTEXTURECODE_Iris;
      else if (!_wcsicmp(pszRet, L"XXXbloodvessels"))
         gChoose = GTEXTURECODE_BloodVessels;
      else if (!_wcsicmp(pszRet, L"XXXhair"))
         gChoose = GTEXTURECODE_Hair;
      else if (!_wcsicmp(pszRet, L"XXXtiles"))
         gChoose = GTEXTURECODE_Tiles;
      else if (!_wcsicmp(pszRet, L"XXXparquet"))
         gChoose = GTEXTURECODE_Parquet;
      else if (!_wcsicmp(pszRet, L"XXXstones"))
         gChoose = GTEXTURECODE_StonesStacked;
      else if (!_wcsicmp(pszRet, L"XXXstonesrandom"))
         gChoose = GTEXTURECODE_StonesRandom;
      else if (!_wcsicmp(pszRet, L"XXXpavers"))
         gChoose = GTEXTURECODE_Pavers;
      else if (!_wcsicmp(pszRet, L"XXXclapboards"))
         gChoose = GTEXTURECODE_Clapboards;
      else if (!_wcsicmp(pszRet, L"XXXlattice"))
         gChoose = GTEXTURECODE_Lattice;
      else if (!_wcsicmp(pszRet, L"XXXwicker"))
         gChoose = GTEXTURECODE_Wicker;
      else if (!_wcsicmp(pszRet, L"XXXchainmail"))
         gChoose = GTEXTURECODE_Chainmail;
      else if (!_wcsicmp(pszRet, L"XXXtestpattern"))
         gChoose = GTEXTURECODE_TestPattern;
      else if (!_wcsicmp(pszRet, L"XXXboardbatten"))
         gChoose = GTEXTURECODE_BoardBatten;
      else if (!_wcsicmp(pszRet, L"XXXshingles"))
         gChoose = GTEXTURECODE_Shingles;
      else if (!_wcsicmp(pszRet, L"XXXvolnoise"))
         gChoose = GTEXTURECODE_VolNoise;
      else if (!_wcsicmp(pszRet, L"XXXvolsandstone"))
         gChoose = GTEXTURECODE_VolSandstone;
      else if (!_wcsicmp(pszRet, L"XXXvolwood"))
         gChoose = GTEXTURECODE_VolWood;
      else if (!_wcsicmp(pszRet, L"XXXvolmarble"))
         gChoose = GTEXTURECODE_VolMarble;
      else // must be wood
         gChoose = GTEXTURECODE_WoodPlanks;

      // create it
      PCTextCreatorSocket pts;
      pts = CreateTextureCreator (dwRenderShard, &gChoose, 0, NULL, TRUE);
      if (!pts)
         goto mainpage;

      PCMMLNode2 pNode;
      pNode = pts->MMLTo ();
      pts->Delete ();
      if (!pNode)
         goto mainpage;

      AttachDateTimeToMML (pNode);

      GUID gSub;
      GUIDGen (&gSub);

      // get a unique name
      wcscpy (t.szName, L"New texture");
      UniqueTextureName (dwRenderShard, t.szMajor, t.szMinor, t.szName);
      t.szCurMajorCategory[0] = t.szCurMinorCategory[0] = 0;

#ifdef WORKONTEXTURES
      LibraryTextures(dwRenderShard, FALSE)->ItemAdd (t.szMajor, t.szMinor, t.szName,
         &gChoose, &gSub, pNode);
#else
      LibraryTextures(dwRenderShard, TRUE)->ItemAdd (t.szMajor, t.szMinor, t.szName,
         &gChoose, &gSub, pNode);
#endif

      // clean out the thumbnail just in case
      ThumbnailGet()->ThumbnailRemove (&gChoose, &gSub);

      goto edit;
   }
   else if (pszRet && !_wcsicmp(pszRet, L"edit")) {
edit:
      GUID gCode, gSub;
      if (!TextureGUIDsFromName(dwRenderShard, t.szMajor, t.szMinor, t.szName, &gCode, &gSub))
         goto mainpage; // error

      // get the MML
      PCMMLNode2 pNode;
      BOOL fUser;
      fUser = TRUE;
      pNode = LibraryTextures(dwRenderShard, TRUE)->ItemGet (t.szMajor, t.szMinor, t.szName);
      if (!pNode) {
         pNode = LibraryTextures(dwRenderShard, FALSE)->ItemGet (t.szMajor, t.szMinor, t.szName);
         fUser = FALSE;
      }
      if (!pNode)
         goto mainpage; // error

      // create
      PCTextCreatorSocket pUse;
      pUse = CreateTextureCreator (dwRenderShard, &gCode, 0, pNode, TRUE);
      // NOTE: Dont delete pNode because it's not ours to delete
      if (!pUse)
         goto mainpage; // error

      // UI for this
      BOOL fRet;
      pszRet = NULL; // BUGFIX so doesnt crash
      fRet = pUse->Dialog (&cWindow);
      int iRet;
      iRet = EscMessageBox (cWindow.m_hWnd, ASPString(),
         L"Do you want to save the changes to your texture?",
         NULL,
         MB_ICONQUESTION | MB_YESNO);
      if (iRet == IDYES) {
         PCMMLNode2 pNode;
         pNode = pUse->MMLTo();
         if (pNode)
            AttachDateTimeToMML(pNode);

         if (fUser) {
            LibraryTextures(dwRenderShard, TRUE)->ItemRemove (t.szMajor, t.szMinor, t.szName);
            LibraryTextures(dwRenderShard, TRUE)->ItemAdd (t.szMajor, t.szMinor, t.szName,
               &gCode, &gSub, pNode);
         }
         else {
            LibraryTextures(dwRenderShard, FALSE)->ItemRemove (t.szMajor, t.szMinor, t.szName);
            LibraryTextures(dwRenderShard, FALSE)->ItemAdd (t.szMajor, t.szMinor, t.szName,
               &gCode, &gSub, pNode);
         }
         // clean out the thumbnail just in case
         ThumbnailGet()->ThumbnailRemove (&gCode, &gSub);

         TextureAlgoCacheRemove (dwRenderShard, &gCode, &gSub);
         TextureCacheDelete (dwRenderShard, &gCode, &gSub);

         // refresh the world in case suing
         PCWorldSocket pWorld;
         pWorld = WorldGet(dwRenderShard, NULL);
         if (pWorld)
            pWorld->NotifySockets (WORLDC_SURFACECHANGED, NULL);
      }
      pUse->Delete();

      t.szCurMinorCategory[0] = 0;   // so will refresh
      if (fRet)
         goto newtext; // done
      // else fall through
   }

   // free the texture map and bitmap
   if (t.pMap)
      t.pMap->Delete();
   if (t.hBit)
      DeleteObject (t.hBit);

   if (pszRet && !_wcsicmp(pszRet, L"ok"))
      return TRUE;
   else
      return FALSE;
}


static PWSTR gpszTexture = L"Texture";

/******************************************************************************
TextureCacheUserTextures - This is called when an ASP file is being saved.
This function calls all of the objects in the world and asks them what
textures they use. It then determines which ones are "installed" and ignores them.
The custom textures are then indexed into the custom list and saved into the
MML for retrieval by TextureUncacheUserTextures. This way a user texture
will be tranferred to a new machine.

inputs
   PCWorldSocket        pWorld - World to look through
returns
   PCMMLNode2 - Cache node
*/
PCMMLNode2 TextureCacheUserTextures (DWORD dwRenderShard, PCWorldSocket pWorld)
{
   // make the list
   CListFixed lText, lTextSub;
   GUID *pg;
   lText.Init (sizeof(GUID)*2);
   lTextSub.Init (sizeof(GUID)*2);

   CBTree tree;
   //WCHAR szTemp[sizeof(GUID)*4+2];

   // all the objects
   DWORD i, j;
   for (i = 0; i < pWorld->ObjectNum(); i++) {
      PCObjectSocket pos = pWorld->ObjectGet(i);
      if (!pos)
         continue;

      lText.Clear();
      pos->TextureQuery (&lText);

      // get new sub-textures
      pg = (GUID*) lText.Get(0);
      lTextSub.Clear();
      for (j = 0; j < lText.Num(); j++, pg+=2)
         TextureAlgoTextureQuery (dwRenderShard, &lTextSub, &tree, pg);


      // loop through all the elements returned
      // NO longer needed since do this directly with texturealgotexturequery()
      //for (j = 0; j < lText.Num(); j++) {
      //   pg = (GUID*) lText.Get(j);

      //   // add to the tree
      //   MMLBinaryToString ((PBYTE)pg, sizeof(GUID)*2, szTemp);

      //   if (!tree.Find(szTemp))
      //      tree.Add (szTemp, &i, sizeof(i));
      //}
   }

   PCMMLNode2 pList;
   pList = new CMMLNode2;
   if (!pList)
      return NULL;
   pList->NameSet (gpszTexture);

   // loop through the tree
   GUID ag[2];
   size_t dwSize;
   for (i = 0; i < tree.Num(); i++) {
      PWSTR psz = tree.Enum(i);
      if (!psz)
         continue;
      dwSize = MMLBinaryFromString (psz, (PBYTE) ag, sizeof(ag));
      if (dwSize != sizeof(ag))
         continue;

      // see if can load it from the user ID
      WCHAR szMajor[128], szMinor[128], szName[128];
      if (!LibraryTextures(dwRenderShard, TRUE)->ItemNameFromGUID (&ag[0], &ag[1], szMajor, szMinor, szName))
         continue;   // not a user texture

      // else, found it
      PCMMLNode2 pFound;
      pFound = LibraryTextures(dwRenderShard, TRUE)->ItemGet (szMajor, szMinor, szName);
      if (!pFound)
         continue;

      // clone and write in into
      PCMMLNode2 pNew;
      pNew = pFound->Clone();
      if (!pNew)
         continue;
      MMLValueSet (pNew, CatMajor(), szMajor);
      MMLValueSet (pNew, CatMinor(), szMinor);
      MMLValueSet (pNew, CatName(), szName);
      MMLValueSet (pNew, CatGUIDCode(), (PBYTE) &ag[0], sizeof(ag[0]));
      MMLValueSet (pNew, CatGUIDSub(), (PBYTE) &ag[1], sizeof(ag[1]));
      pNew->NameSet (gpszTexture);

      // add it
      pList->ContentAdd (pNew);
   }

   return pList;
}


/******************************************************************************
TextureUnCacheUserTextures - Called when an ASP file is being loaded. It
looks through all textures embedded in the file. If they don't exist in
the current library then they're added (make sure name is unique). If they
do exist and they're older, they're ignored. If they're newer, ask the user
if they want to import the new changes.

inputs
   PCMMLNode2      pNode - From TextureCacheUserTextures()
   HWND           hWnd - To bring question message boxes up from. If pLibrary != NULL then
                  hWnd won't be used to display,a nd a reasonable default will be used.
   PCLibrary      pLibrary - If not NULL, then all new textures are written to this library.
                     If NULL, then the default user library is used
returns
   BOOL - TRUE if success
*/
BOOL TextureUnCacheUserTextures (DWORD dwRenderShard, PCMMLNode2 pNode, HWND hWnd, PCLibrary pLibrary)
{
   DWORD i;
   PWSTR psz;
   PCMMLNode2 pSub;
   for (i = 0; i < pNode->ContentNum(); i++) {
      pSub = NULL;
      if (!pNode->ContentEnum(i, &psz, &pSub))
         continue;
      if (!pSub)
         continue;
      psz = pSub->NameGet();
      if (!psz || _wcsicmp(psz, gpszTexture))
         continue;

      // have a textre.... get its guids and name
      PWSTR pszMajor, pszMinor, pszName;
      GUID gCode, gSub;
      DFDATE dwDate;
      DFTIME dwTime;
      pszMajor = MMLValueGet (pSub, CatMajor());
      pszMinor = MMLValueGet (pSub, CatMinor());
      pszName = MMLValueGet (pSub, CatName());
      if (!pszMajor || !pszMinor || !pszName)
         continue;
      MMLValueGetBinary (pSub, CatGUIDCode(), (PBYTE) &gCode, sizeof(gCode));
      MMLValueGetBinary (pSub, CatGUIDSub(), (PBYTE) &gSub, sizeof(gSub));
      GetDateAndTimeFromMML (pSub, &dwDate, &dwTime);

      // if it exists as an installed then ignore
      if (LibraryTextures(dwRenderShard, FALSE)->ItemGet(&gCode, &gSub))
         continue;

      // which category object
      // BUGFIX - Allow to come from different library
      PCLibraryCategory plc = pLibrary ?
         pLibrary->CategoryGet (L"Textures" /*gpszTextures*/) :
         LibraryTextures(dwRenderShard, TRUE);

      // see if it exsits as a user
      PCMMLNode2 pUser;
      pUser = plc->ItemGet(&gCode, &gSub);
      WCHAR szOldMajor[128], szOldMinor[128], szOldName[128];
      DFDATE dwExistDate;
      DFTIME dwExistTime;
      if (pUser) {
         GetDateAndTimeFromMML (pUser, &dwExistDate, &dwExistTime);
         plc->ItemNameFromGUID (&gCode, &gSub, szOldMajor, szOldMinor, szOldName);
      }

      BOOL  fAdd;
      fAdd = FALSE;
      if (!pUser)
         fAdd = TRUE;
      else {
         // date compare
         if ((dwDate > dwExistDate) || ((dwDate == dwExistDate) && (dwTime > dwExistTime))) {
            if (!pLibrary) {
               // the one in the file is more recent. Ask user if want to replace
               WCHAR szTemp[512];
               swprintf (szTemp, L"The file contains a more recent version of the texture, \"%s\", "
                  L"located in \"%s, %s\". Do you want to use the new version?",
                  szOldName, szOldMajor, szOldMinor);

               int iRet;
               iRet = EscMessageBox (hWnd, ASPString(),
                  szTemp,
                  L"If you press \"Yes\" your old texture will be overwritten. If you "
                  L"press \"No\" the building you are viewing may not look correct because "
                  L"it won't be displayed with the updated texture.",
                  MB_ICONQUESTION | MB_YESNOCANCEL);
               if (iRet == IDYES)
                  fAdd = TRUE;
            }
            else
               fAdd = TRUE;
         } // date compare
      }
      if (!fAdd)
         continue;

      // else, adding...

      // if there is an existing one then remove it
      if (pUser) {
         if (!pLibrary) {
            TextureAlgoCacheRemove (dwRenderShard, &gCode, &gSub);
            TextureCacheDelete (dwRenderShard, &gCode, &gSub);
         }
         plc->ItemRemove (szOldMajor, szOldMinor, szOldName);
      }

      // make sure the new name is safe
      wcscpy (szOldMajor, pszMajor);
      wcscpy (szOldMinor, pszMinor);
      wcscpy (szOldName, pszName);
      UniqueTextureName (dwRenderShard, szOldMajor, szOldMinor, szOldName, pLibrary);

      // add it
      PCMMLNode2 pNew;
      pNew = pSub->Clone();
      if (!pNew)
         continue;

      // add it
      plc->ItemAdd (szOldMajor, szOldMinor, szOldName, &gCode, &gSub, pNew);
   } // over all nodes

   return TRUE;
}


/****************************************************************************
TextureTintPage
*/
BOOL TextureTintPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   static WCHAR sTextureTemp[16];

   PTSPAGE pt = (PTSPAGE) pPage->m_pUserData;
   DWORD dwRenderShard = pt->dwRenderShard;
   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         // new texture so set all the parameters
         pPage->Message (ESCM_USER + 84);
      }
      break;
   case ESCM_USER+84:  // we have a new texture so set the parameters
      {
         PCEscControl pControl;

         // Set the sliders and stuff
         pControl = pPage->ControlFind (L"hue");
         if (pControl)
            pControl->AttribSetInt (Pos(), (int) (short) pt->pSurf->m_TextureMods.wHue);
         pControl = pPage->ControlFind (L"saturation");
         if (pControl)
            pControl->AttribSetInt (Pos(), min((int) sqrt((fp)pt->pSurf->m_TextureMods.wSaturation), 127));
         pControl = pPage->ControlFind (L"contrast");
         if (pControl)
            pControl->AttribSetInt (Pos(), min((int) sqrt((fp)pt->pSurf->m_TextureMods.wContrast), 127));
         pControl = pPage->ControlFind (L"brightness");
         if (pControl)
            pControl->AttribSetInt (Pos(), min((int) sqrt((fp)pt->pSurf->m_TextureMods.wBrightness), 127));

         pControl = pPage->ControlFind (L"redtint");
         if (pControl)
            pControl->AttribSetInt (Pos(), (int) GetRValue(pt->pSurf->m_TextureMods.cTint));
         pControl = pPage->ControlFind (L"greentint");
         if (pControl)
            pControl->AttribSetInt (Pos(), (int) GetGValue(pt->pSurf->m_TextureMods.cTint));
         pControl = pPage->ControlFind (L"bluetint");
         if (pControl)
            pControl->AttribSetInt (Pos(), (int) GetBValue(pt->pSurf->m_TextureMods.cTint));

         // BUGFIX - Do this so caclualte the scaling and angle properly. Wasnt right before
         // find the angle
         // NOTE: This isolation of scale may not be correct, but since it seems to work
         // leaving it as it is
         double fLenR, fLenU;
         fLenR = sqrt(pt->pSurf->m_afTextureMatrix[0][0] * pt->pSurf->m_afTextureMatrix[0][0] +
            pt->pSurf->m_afTextureMatrix[1][0] * pt->pSurf->m_afTextureMatrix[1][0]);
         if (fLenR > EPSILON)
            fLenR = 1.0 / fLenR;
         fLenU = sqrt(pt->pSurf->m_afTextureMatrix[0][1] * pt->pSurf->m_afTextureMatrix[0][1] +
            pt->pSurf->m_afTextureMatrix[1][1] * pt->pSurf->m_afTextureMatrix[1][1]);
         if (fLenU > EPSILON)
            fLenU = 1.0 / fLenU;

         fp tNew[2][2];
         memcpy (tNew, pt->pSurf->m_afTextureMatrix, sizeof(tNew));
         tNew[0][0] *= fLenR;
         tNew[1][0] *= fLenR;
         tNew[0][1] *= fLenU;
         tNew[1][1] *= fLenU;


         fp fAngle;
         fAngle = -atan2 (tNew[0][1], tNew[1][1]);
         AngleToControl (pPage, L"angle", fAngle, TRUE);

         // find the scale
         MeasureToString (pPage, L"width", fLenR);
         MeasureToString (pPage, L"height", fLenU);


         // calculate 3d cscaling
         DWORD i, j;
         fp afScale[3];
         CMatrix mInv, mScale;
         pt->pSurf->m_mTextureMatrix.Invert (&mInv);
         for (i = 0; i < 3; i++) {
            afScale[i] = 0;
            for (j = 0; j < 3; j++)
               afScale[i] += mInv.p[j][i] * mInv.p[j][i];
            afScale[i] = sqrt(afScale[i]);
            afScale[i] = max(afScale[i], CLOSE);
            //if (afScale[i] > EPSILON)
            //   afScale[i] = 1.0 / afScale[i];
         } // i
         mScale.Scale (1.0 / afScale[0], 1.0 / afScale[1], 1.0 / afScale[2]);

         // mod matrix and get the rest
         CMatrix mNew;
         mNew.Multiply (&pt->pSurf->m_mTextureMatrix, &mScale);
         //for (i = 0; i < 3; i++) for (j = 0; j < 3; j++)
         //   mNew.p[j][i] *= afScale[i];

         // now find the other angles
         CPoint pTrans, pRot;
         mNew.ToXYZLLT (&pTrans, &pRot.p[2], &pRot.p[0], &pRot.p[1]);

         // set all the values for the 3d matrix
         for (i = 0; i < 3; i++) {
            WCHAR szTemp[32];

            swprintf (szTemp, L"rot%d", i);
            AngleToControl (pPage, szTemp, pRot.p[i]);

            swprintf (szTemp, L"offset%d", i);
            MeasureToString (pPage, szTemp, -pTrans.p[i]);

            swprintf (szTemp, L"scale%d", i);
            DoubleToControl (pPage, szTemp, afScale[i]);
         } // i

         // redo the bitmap
         pPage->Message (ESCM_USER+85);
      }
      return TRUE;

   case ESCM_USER+85:   // redo the bitmap
      {
         // redo the bitmap
         pt->pMap->FillImage (pt->pImage, FALSE, FALSE, pt->pSurf->m_afTextureMatrix,
            &pt->pSurf->m_mTextureMatrix, &pt->pSurf->m_Material);
         HDC hDC;
         hDC = GetDC (pPage->m_pWindow->m_hWnd);
         if (pt->hBit)
            DeleteObject (pt->hBit);
         pt->hBit = pt->pImage->ToBitmap (hDC);
         ReleaseDC (pPage->m_pWindow->m_hWnd, hDC);

         PCEscControl pControl = pPage->ControlFind (L"image");

         WCHAR szTemp[32];
         swprintf (szTemp, L"%lx", (__int64) pt->hBit);
         pControl->AttribSet (L"hbitmap", szTemp);
      }
      return TRUE;

   case ESCN_SCROLL:
   // BUGFIX - Because these are slow calculations, dont do when scrolling case ESCN_SCROLLING:
      {
         PESCNSCROLL p = (PESCNSCROLL) pParam;

         if (!p->pControl->m_pszName)
            break;

         if (!_wcsicmp(p->pControl->m_pszName, L"hue")) {
            pt->pSurf->m_TextureMods.wHue = (WORD) (short) p->iPos;
            pt->pMap->TextureModsSet (&pt->pSurf->m_TextureMods);
            pPage->Message (ESCM_USER+85);
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"saturation")) {
            pt->pSurf->m_TextureMods.wSaturation = (WORD) (p->iPos * p->iPos);
            pt->pMap->TextureModsSet (&pt->pSurf->m_TextureMods);
            pPage->Message (ESCM_USER+85);
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"contrast")) {
            pt->pSurf->m_TextureMods.wContrast = (WORD) (p->iPos * p->iPos);
            pt->pMap->TextureModsSet (&pt->pSurf->m_TextureMods);
            pPage->Message (ESCM_USER+85);
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"brightness")) {
            pt->pSurf->m_TextureMods.wBrightness = (WORD) (p->iPos * p->iPos);
            pt->pMap->TextureModsSet (&pt->pSurf->m_TextureMods);
            pPage->Message (ESCM_USER+85);
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"redtint")) {
            pt->pSurf->m_TextureMods.cTint = 
               RGB((BYTE) p->iPos, GetGValue(pt->pSurf->m_TextureMods.cTint), GetBValue(pt->pSurf->m_TextureMods.cTint));
            pt->pMap->TextureModsSet (&pt->pSurf->m_TextureMods);
            pPage->Message (ESCM_USER+85);
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"greentint")) {
            pt->pSurf->m_TextureMods.cTint = 
               RGB(GetRValue(pt->pSurf->m_TextureMods.cTint), (BYTE) p->iPos, GetBValue(pt->pSurf->m_TextureMods.cTint));
            pt->pMap->TextureModsSet (&pt->pSurf->m_TextureMods);
            pPage->Message (ESCM_USER+85);
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"bluetint")) {
            pt->pSurf->m_TextureMods.cTint = 
               RGB(GetRValue(pt->pSurf->m_TextureMods.cTint), GetGValue(pt->pSurf->m_TextureMods.cTint),(BYTE) p->iPos);
            pt->pMap->TextureModsSet (&pt->pSurf->m_TextureMods);
            pPage->Message (ESCM_USER+85);
            return TRUE;
         }
      }
      break;

   case ESCN_EDITCHANGE:
      {
         // if any edit changed then get the scaling and do the rotation
         if (pPage->ControlFind (L"width")) {
            fp fWidth, fHeight, fAngle;
            if (!MeasureParseString (pPage, L"width", &fWidth))
               fWidth = 0;
            fWidth = max(0.01, fWidth);
            if (!MeasureParseString (pPage, L"height", &fHeight))
               fHeight = 0;
            fHeight = max(0.01, fHeight);
            fAngle = AngleFromControl (pPage, L"angle");

            TextureMatrixRotate (pt->pSurf->m_afTextureMatrix, fAngle, fWidth,fHeight);
         }


         if (pPage->ControlFind (L"rot0")) {
            CPoint pTrans, pRot, pScale;
            DWORD i;

            // set all the values for the 3d matrix
            WCHAR szTemp[32];
            for (i = 0; i < 3; i++) {
               swprintf (szTemp, L"rot%d", i);
               pRot.p[i] = AngleFromControl (pPage, szTemp);

               swprintf (szTemp, L"offset%d", i);
               if (!MeasureParseString (pPage, szTemp, &pTrans.p[i]))
                  pTrans.p[i] = 0;
               pTrans.p[i] *= -1;   // since moving in wrong direction

               swprintf (szTemp, L"scale%d", i);
               pScale.p[i] = DoubleFromControl (pPage, szTemp);
               pScale.p[i] = max(pScale.p[i], CLOSE);
            } // i

            CMatrix mNew;
            mNew.FromXYZLLT (&pTrans, pRot.p[2], pRot.p[0], pRot.p[1]);
            pt->pSurf->m_mTextureMatrix.Scale (1.0 / pScale.p[0], 1.0 / pScale.p[1], 1.0 / pScale.p[2]);
            pt->pSurf->m_mTextureMatrix.MultiplyRight (&mNew);
         }

         pPage->Message (ESCM_USER+85);
         return TRUE;
      }
      break;

   case ESCM_LINK:
      {
         PESCMLINK p = (PESCMLINK) pParam;
         if (p->psz && !_wcsicmp(p->psz, L"ok"))
            pt->fPressedOK = TRUE;  // BUGFIX - since pressing enter was causing problems
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"HBITMAP")) {
            swprintf (sTextureTemp, L"%lx", (__int64) pt->hBit);
            p->pszSubString = sTextureTemp;
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"IFHV")) {
            BOOL fHV = (pt->pMap->DimensionalityQuery(0) & 0x01) ? TRUE : FALSE;
            p->pszSubString = fHV ? L"" : L"<comment>";
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"ENDIFHV")) {
            BOOL fHV = (pt->pMap->DimensionalityQuery(0) & 0x01) ? TRUE : FALSE;
            p->pszSubString = fHV ? L"" : L"</comment>";
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"IFHVXYZ")) {
            BOOL fHV = (pt->pMap->DimensionalityQuery(0) & 0x01) ? TRUE : FALSE;
            BOOL fXYZ = (pt->pMap->DimensionalityQuery(0) & 0x02) ? TRUE : FALSE;
            p->pszSubString = (fHV && fXYZ) ? L"" : L"<comment>";
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"ENDIFHVXYZ")) {
            BOOL fHV = (pt->pMap->DimensionalityQuery(0) & 0x01) ? TRUE : FALSE;
            BOOL fXYZ = (pt->pMap->DimensionalityQuery(0) & 0x02) ? TRUE : FALSE;
            p->pszSubString = (fHV && fXYZ) ? L"" : L"</comment>";
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"IFXYZ")) {
            BOOL fXYZ = (pt->pMap->DimensionalityQuery(0) & 0x02) ? TRUE : FALSE;
            p->pszSubString = fXYZ ? L"" : L"<comment>";
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"ENDIFXYZ")) {
            BOOL fXYZ = (pt->pMap->DimensionalityQuery(0) & 0x02) ? TRUE : FALSE;
            p->pszSubString = fXYZ ? L"" : L"</comment>";
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}




/****************************************************************************
TextureTintDialog - This brings up the UI for changing CObjectSurface. It's
passed in an object surface that's manupulated (and changed). Retrusn TRUE
if the user presses OK, FALSE if they press cancel. If the user presses OK
the object surface should probably be set.

inputs
   HWND           hWnd - Window to create this over
   PCObjectSurface   pSurf - Surface
returns
   BOOL - TRUE if the user presses OK, FALSE if not
*/
BOOL TextureTintDialog (DWORD dwRenderShard, HWND hWnd, PCObjectSurface pSurf)
{
   CEscWindow cWindow;
   RECT r;
   DialogBoxLocation2 (hWnd, &r);
   CImage   Image;
   Image.Init (200, 200, RGB(0xff,0xff,0xff));

   cWindow.Init (ghInstance, hWnd, EWS_FIXEDSIZE | EWS_AUTOHEIGHT | EWS_NOTITLE, &r);
   PWSTR pszRet;
   CObjectSurface os;
   memcpy (&os, pSurf, sizeof(os));

   // set up the info
   TSPAGE t;
   memset (&t, 0, sizeof(t));
   t.pSurf = &os;
   t.pMap = NULL;
   t.pImage = &Image;
   t.hBit = NULL;
   t.dwRenderShard = dwRenderShard;
   
   // get the name of the object
   WCHAR szMajor[128], szMinor[128], szName[128];
   if (!TextureNameFromGUIDs(dwRenderShard, &os.m_gTextureCode, &os.m_gTextureSub, szMajor, szMinor, szName))
      return FALSE;

   // get the name
   wcscpy (t.szMajor, szMajor);
   wcscpy (t.szMinor, szMinor);
   wcscpy (t.szName, szName);

   // already have a texture, so create it
   // get the teture
   if (t.pMap)
      t.pMap->Delete();
   t.pMap = TextureCreate (dwRenderShard, &os.m_gTextureCode, &os.m_gTextureSub);
   if (!t.pMap)
      return FALSE;
   t.pMap->TextureModsSet (&os.m_TextureMods);

   HDC hDC;
   hDC = GetDC (hWnd);
   if (t.hBit)
      DeleteObject (t.hBit);
   t.hBit = t.pImage->ToBitmap (hDC);
   ReleaseDC (hWnd, hDC);


   // start with the first page
   pszRet = cWindow.PageDialog (ghInstance, IDR_MMLTEXTURESEL, TextureTintPage, &t);

   // free the texture map and bitmap
   if (t.pMap)
      t.pMap->Delete();
   if (t.hBit)
      DeleteObject (t.hBit);

   if (t.fPressedOK || !_wcsicmp(pszRet, L"ok")) {
      // BUGFIX - So OK and cancel work properly
      memcpy (pSurf, &os, sizeof(os));
      return TRUE;
   }
   else
      return FALSE;
}

/****************************************************************************
TextureCreateThumbnail - Create a thumbnail from an object. This is added to
the thumbnail list.

inputs
   GUID        *pgMajor - Major GUID
   GUID        *pgMinor - Minor GUID
   BOOL        fStretchToFit - If TRUE, stretch texture to fit
returns
   BOOL - TRUE if success
*/
BOOL TextureCreateThumbnail (DWORD dwRenderShard, GUID *pgCode, GUID *pgSub, BOOL fStretchToFit)
{
   PCTextCreatorSocket pCreator;
   CImage Image;
   Image.Init (TEXTURETHUMBNAIL, TEXTURETHUMBNAIL);

   PCMMLNode2 pNode;
   WCHAR szMajor[128], szMinor[128], szName[128];
   if (!LibraryTextures(dwRenderShard, FALSE)->ItemNameFromGUID (pgCode, pgSub, szMajor, szMinor, szName)) {
      if (!LibraryTextures(dwRenderShard, TRUE)->ItemNameFromGUID (pgCode, pgSub, szMajor, szMinor, szName))
         return FALSE;  // cant find
   }
   pNode = LibraryTextures(dwRenderShard, FALSE)->ItemGet (szMajor, szMinor, szName);
   if (!pNode)
      pNode = LibraryTextures(dwRenderShard, TRUE)->ItemGet (szMajor, szMinor, szName);
   if (!pNode)
      return FALSE;

   // else, create it
   pCreator = CreateTextureCreator (dwRenderShard, pgCode, 0, pNode, TRUE);
   if (!pCreator)
      return FALSE;  // cant create

   pCreator->FillImageWithTexture(&Image, !IsImage(szMajor, szMinor), fStretchToFit);

   pCreator->Delete();

   // cache this
   ThumbnailGet()->ThumbnailAdd (pgCode, pgSub, &Image, -1);
   
   return TRUE;
}

/****************************************************************************
TextureGetThumbnail - Returns the thumbnail for a texture. If the thumbnail
doesn't exist then it's created.

inputs
   GUID        *pgMajor - Major GUID
   GUID        *pgMinor - Minor GUID
   HWND        hWnd - To get HDC from
   COLORREF    *pcTransparent - Filled with the transparent color

returns
   HBITMAP - Bitmap.Must have DestroyObject() called with it
*/
HBITMAP TextureGetThumbnail (DWORD dwRenderShard, GUID *pgCode, GUID *pgSub, HWND hWnd, COLORREF *pcTransparent)
{
   HBITMAP hBit;
   hBit = ThumbnailGet()->ThumbnailToBitmap (pgCode, pgSub, hWnd, pcTransparent);
   if (hBit)
      return hBit;


   // create it
   TextureCreateThumbnail (dwRenderShard, pgCode, pgSub, FALSE);

   // try again
   return ThumbnailGet()->ThumbnailToBitmap (pgCode, pgSub, hWnd, pcTransparent);
}


typedef struct {
   GUID        gCode;      // id GUID
   GUID        gSub;       // id GUID
   WCHAR       szMajor[128];   // major category name
   WCHAR       szMinor[128];   // minor category name
   WCHAR       szName[128];    // name
   int         iVScroll;      // where to scroll to
   PCListFixed pThumbInfo;    // for whichimages there
   DWORD       dwTimerID;     // for page
   PCObjectSurface pSurf;  // object surface
   BOOL        fChanged;      // set to TRUE if anything was changed that requires refresh
   PCWorldSocket     pWorld;        // world
   CMaterial   Material;      // used for TextBlankPage and TextFromFilePage
   COLORREF    cTransColor;   // used for TextBlankPage and TextFromFilePage
   DWORD       dwRenderShard; // render shard
} TNPAGE, *PTNPAGE;

typedef struct {
   HBITMAP     hBitmap;    // bitmap used
   BOOL        fEmpty;     // if fEmpty is set then the bitmap used is a placeholder
   PWSTR       pszName;    // name of the texture - do not modify
   COLORREF    cTransparent;  // if not -1 then this is the transparent color
} THUMBINFO, *PTHUMBINFO;


/****************************************************************************
ObjPaintAddSchemePage
*/
BOOL ObjPaintAddSchemePage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PTNPAGE pop = (PTNPAGE) pPage->m_pUserData;
   DWORD dwRenderShard = pop->dwRenderShard;

   switch (dwMessage) {
   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;

         if (!p->pControl || !p->pControl->m_pszName)
            break;

         if (!_wcsicmp(p->pControl->m_pszName, L"ok")) {
            // make sure it doesn't already exit
            WCHAR szTemp[64];
            PCEscControl pControl;
            pControl = pPage->ControlFind (L"scheme");
            szTemp[0] = 0;
            DWORD dwNeeded;
            pControl->AttribGet (Text(), szTemp, sizeof(szTemp), &dwNeeded);
            if (!szTemp[0]) {
               pPage->MBWarning (L"Please type in a name first.");
               return TRUE;
            }

            // see if it's a duplicate
            PCObjectSurface ps;
            ps = (pop->pWorld->SurfaceSchemeGet())->SurfaceGet (szTemp, NULL, TRUE);
            if (ps) {
               // since no clone delete ps;
               pPage->MBWarning (L"The scheme name already exists. Please type in a new one.");
               return TRUE;
            }

            // base it on the object's current coloration
            wcscpy (pop->pSurf->m_szScheme, szTemp);
            pop->fChanged = TRUE;

            // add it
            (pop->pWorld->SurfaceSchemeGet())->SurfaceSet (pop->pSurf);

            // exit and go back
            pPage->Exit (L"ok");
            return TRUE;
         }
      }
      break;


   };


   return DefPage (pPage, dwMessage, pParam);
}


BOOL TextBlankPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PTNPAGE pt = (PTNPAGE) pPage->m_pUserData;
   DWORD dwRenderShard = pt->dwRenderShard;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         PCEscControl pControl;

         pControl = pPage->ControlFind (L"mapcolor");
         if (pControl)
            pControl->AttribSetBOOL (Checked(), TRUE);

#if 0 // dead code
         // directory
         PCWorldSocket pWorld;
         PWSTR psz;
         WCHAR szDir[256];
         szDir[0] = 0;
         pWorld = WorldGet();
         psz = pWorld ? pWorld->NameGet() : NULL;
         if (psz && psz[0]) {
            wcscpy (szDir, psz);
            DWORD dwLen, i;
            dwLen = wcslen(szDir);
            for (i = dwLen-1; i < dwLen; i--)
               if (szDir[i] == L'\\') {
                  szDir[i+1] = 0;
                  break;
               }
         };

         if (!szDir[0])
            MultiByteToWideChar (CP_ACP, 0, gszAppDir, -1, szDir, sizeof(szDir)/2);
         pControl = pPage->ControlFind (L"dir");
         if (pControl)
            pControl->AttribSet (Text(), szDir);
#endif // 0

         pControl = pPage->ControlFind (L"name");
         if (pControl)
            pControl->AttribSet (Text(), L"New texture");

         //ComboBoxSet (pPage, L"fileformat", 0);

         DoubleToControl (pPage, L"widthpix", 500);
         DoubleToControl (pPage, L"heightpix", 500);
         MeasureToString (pPage, L"widthdist", 1);

         FillStatusColor (pPage, L"defcolor", pt->cTransColor = RGB(0xff,0xff,0xff)); 

         // set the material
         ComboBoxSet (pPage, L"material", pt->Material.m_dwID);
         pControl = pPage->ControlFind (L"editmaterial");
         if (pControl)
            pControl->Enable (pt->Material.m_dwID ? FALSE : TRUE);

      }
      break;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;

         if (!p->pControl || !p->pControl->m_pszName)
            break;

         if (!_wcsicmp(p->pControl->m_pszName, L"ok")) {
            // get the values
            PCEscControl pControl;
            BOOL afUse[5];
            BOOL fUse;
            DWORD i;
            memset (afUse, 0, sizeof(afUse));
            if (pControl = pPage->ControlFind (L"mapcolor"))
               afUse[0] = pControl->AttribGetBOOL (Checked());
            if (pControl = pPage->ControlFind (L"mapglow"))
               afUse[1] = pControl->AttribGetBOOL (Checked());
            if (pControl = pPage->ControlFind (L"mapbump"))
               afUse[2] = pControl->AttribGetBOOL (Checked());
            if (pControl = pPage->ControlFind (L"maptrans"))
               afUse[3] = pControl->AttribGetBOOL (Checked());
            if (pControl = pPage->ControlFind (L"mapspec"))
               afUse[4] = pControl->AttribGetBOOL (Checked());
            fUse = FALSE;
            for (i = 0; i < 5; i++)
               fUse |= afUse[i];
            if (!fUse) {
               pPage->MBWarning (L"You must select at least one of the maps.");
               return TRUE;
            }

            // file name
            WCHAR szFile[240];
            DWORD dwNeeded;
            szFile[0] = 0;
            pControl = pPage->ControlFind (L"file");
            if (pControl)
               pControl->AttribGet (Text(), szFile, sizeof(szFile), &dwNeeded);
            DWORD dwExt;
            dwExt = (DWORD)wcslen(szFile);
            if (dwExt >= 4)
               dwExt -= 4;
            else
               dwExt = 0;
            BOOL fJPEG;
            if (dwExt && !_wcsicmp(szFile + dwExt, L".jpg"))
               fJPEG = TRUE;
            else if (dwExt && !_wcsicmp(szFile + dwExt, L".bmp"))
               fJPEG = FALSE;
            else {
               // no  file
               pPage->MBWarning (L"Please select a file to save the texture to.");
               return TRUE;
            }

            // make up file names
            WCHAR aszFile[5][256];
            BOOL fFirst;
            char szTemp[256];
            fFirst = TRUE;
            for (i = 0; i < 5; i++) {
               if (!afUse[i])
                  continue;

               wcscpy (aszFile[i], szFile);

               if (fFirst)
                  fFirst = FALSE;
               else {
                  // else, append extra character
                  aszFile[i][dwExt] = L'a' + i;
                  wcscpy (aszFile[i] + (dwExt+1), fJPEG ? L".jpg" : L".bmp");
               }

               // make sure doesnt exist
               WideCharToMultiByte (CP_ACP, 0, aszFile[i], -1, szTemp, sizeof(szTemp), 0 ,0);
               FILE *f;
               OUTPUTDEBUGFILE (szTemp);
               f = fopen (szTemp, "rb");
               if (f) {
                  fclose (f);
                  pPage->MBWarning (L"The file already exists.", L"Please choose a different name.");
                  return TRUE;
               }
            } // i

            // make sure that have a unique name
            WCHAR szName[128];
            szName[0] = 0;
            pControl = pPage->ControlFind (L"name");
            if (pControl)
               pControl->AttribGet (Text(), szName, sizeof(szName), &dwNeeded);
            if (!szName[0])
               wcscpy (szName, L"New texture");
            UniqueTextureName (dwRenderShard, pt->szMajor, pt->szMinor, szName);

            // get the width and height
            DWORD dwWidth, dwHeight;
            dwWidth = (DWORD)DoubleFromControl (pPage, L"widthpix");
            dwHeight = (DWORD)DoubleFromControl (pPage, L"heightpix");
            dwWidth = max (dwWidth,2);
            dwHeight = max(dwHeight, 2);
            

            // make all the files
            CImage Image;
            Image.Init (dwWidth, dwHeight);
            DWORD dw;
            for (i = 0; i < 5; i++) {
               if (!afUse[i])
                  continue;
               
               // fill it in
               PIMAGEPIXEL pip;
               pip = Image.Pixel(0,0);
               for (dw = 0; dw < dwWidth * dwHeight; dw++, pip++) {
                  switch (i) {
                  case 0:  // RGB
                     pip->wRed = Gamma (GetRValue(pt->cTransColor));
                     pip->wGreen = Gamma (GetGValue(pt->cTransColor));
                     pip->wBlue = Gamma (GetBValue(pt->cTransColor));
                     break;
                  case 1:  // glow
                     if (afUse[0])
                        pip->wRed = pip->wGreen = pip->wBlue = 0;
                     else {
                        pip->wRed = Gamma (GetRValue(pt->cTransColor));
                        pip->wGreen = Gamma (GetGValue(pt->cTransColor));
                        pip->wBlue = Gamma (GetBValue(pt->cTransColor));
                     }
                     break;
                  case 2:  // bump
                     pip->wRed = pip->wGreen = pip->wBlue = Gamma(0x80);
                     break;
                  case 3:  // trans
                     pip->wRed = pip->wGreen = pip->wBlue = pt->Material.m_wTransparency;
                     break;
                  case 4:  // spec
                     pip->wRed = pip->wGreen = pip->wBlue = pt->Material.m_wSpecReflect;
                     break;
                  }
               } // dw

               // save
               HDC hDC;
               HBITMAP hBit;
               BOOL fRet;
               fRet = FALSE;
               hDC = GetDC (GetDesktopWindow ());
               hBit = Image.ToBitmap (hDC);
               ReleaseDC (GetDesktopWindow(), hDC);
               
               WideCharToMultiByte (CP_ACP, 0, aszFile[i], -1, szTemp, sizeof(szTemp), 0 ,0);
               if (hBit && !fJPEG)
                  fRet = BitmapSave (hBit, szTemp);
               else if (hBit && fJPEG)
                  fRet = BitmapToJPegNoMegaFile (hBit, szTemp);
               if (hBit)
                  DeleteObject (hBit);
               if (!fRet) {
                  pPage->MBWarning (L"The file couldn't be saved.", L"There may not be enough space on the disk.");
                  return TRUE;
               }
            } // i

            // make the creator
            PCTextCreatorImageFile pNew;
            pNew = new CTextCreatorImageFile(dwRenderShard, 0);
            if (!pNew)
               return TRUE;
            pNew->m_cDefColor = pt->cTransColor;
            pNew->m_dwX = dwWidth;
            pNew->m_dwY = dwHeight;
            pNew->m_fBumpHeight = .01;
            pControl = pPage->ControlFind (L"cached");
            if (pControl)
               pNew->m_fCached = pControl->AttribGetBOOL (Checked());
            pNew->m_fTransUse = FALSE;
            MeasureParseString (pPage, L"widthdist", &pNew->m_fWidth);
            pNew->m_fWidth = max(CLOSE, pNew->m_fWidth);
            memcpy (&pNew->m_Material, &pt->Material, sizeof(pt->Material));
            if (afUse[0])
               wcscpy (pNew->m_szFile, aszFile[0]);
            if (afUse[1])
               wcscpy (pNew->m_szGlowFile, aszFile[1]);
            if (afUse[2])
               wcscpy (pNew->m_szBumpFile, aszFile[2]);
            if (afUse[3])
               wcscpy (pNew->m_szTransFile, aszFile[3]);
            if (afUse[4])
               wcscpy (pNew->m_szGlossFile, aszFile[4]);

            pNew->LoadImageFromDisk();

            // add it
            PCMMLNode2 pNode;
            pNode = pNew->MMLTo();
            pNew->Delete();
            if (pNode) {
               GUID gSub;
               AttachDateTimeToMML(pNode);
               GUIDGen (&gSub);

               // note: only doing user
               LibraryTextures(dwRenderShard, TRUE)->ItemRemove (pt->szMajor, pt->szMinor, szName);
               LibraryTextures(dwRenderShard, TRUE)->ItemAdd (pt->szMajor, pt->szMinor, szName,
                  &GTEXTURECODE_ImageFile, &gSub, pNode);
               // NOTE: Not deleting pNode because is kept by LibraryTextures

               // copy new name and guid over
               pt->gCode = GTEXTURECODE_ImageFile;
               pt->gSub = gSub;
               wcscpy (pt->szName, szName);
            }

            // exit
            pPage->Exit(RedoSamePage());
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"editmaterial")) {
            pt->Material.Dialog (pPage->m_pWindow->m_hWnd);
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"changedef")) {
            COLORREF cr;
            cr = AskColor (pPage->m_pWindow->m_hWnd, pt->cTransColor, pPage, L"defcolor");
            if (cr != pt->cTransColor) {
               pt->cTransColor = cr;
            }
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"browse")) {
            OPENFILENAME   ofn;
            char  szTemp[256];
            szTemp[0] = 0;
            memset (&ofn, 0, sizeof(ofn));
   
            // get what's typed in
            PCEscControl pControl;
            DWORD dwNeeded;
            WCHAR szw[256];
            szw[0] = 0;
            pControl = pPage->ControlFind (L"file");
            if (pControl)
               pControl->AttribGet (Text(), szw, sizeof(szw), &dwNeeded);


            // BUGFIX - Set directory
            char szInitial[256];
            GetLastDirectory(szInitial, sizeof(szInitial));
            if (szw[0]) {
               WideCharToMultiByte (CP_ACP, 0, szw, -1, szInitial, sizeof(szInitial), 0, 0);
            }
            // get name to save as
            ofn.lpstrInitialDir = szInitial;
            ofn.lStructSize = sizeof(ofn);
            ofn.hwndOwner = pPage->m_pWindow->m_hWnd;
            ofn.hInstance = ghInstance;
            ofn.lpstrFilter = "Bitmap file (*.bmp)\0*.bmp\0JPEG file (*.jpg)\0*.jpg\0\0\0";
            ofn.lpstrFile = szTemp;
            ofn.nMaxFile = sizeof(szTemp);
            ofn.lpstrTitle = "Save image file";
            ofn.Flags = OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
            ofn.lpstrDefExt = ".bmp";
            // nFileExtension 
            if (!GetSaveFileName(&ofn))
               return TRUE;

            // convert to unicode
            MultiByteToWideChar (CP_ACP, 0, szTemp, -1, szw, sizeof(szw)/2);
            if (pControl)
               pControl->AttribSet (Text(), szw);
            return TRUE;
         }
      }
      break;

   case ESCN_COMBOBOXSELCHANGE:
      {
         PESCNCOMBOBOXSELCHANGE p = (PESCNCOMBOBOXSELCHANGE) pParam;
         if (!p->pControl->m_pszName)
            break;

         DWORD dwVal;
         dwVal = p->pszName ? _wtoi(p->pszName) : 0;

         if (!_wcsicmp(p->pControl->m_pszName, L"material")) {
            if (dwVal == pt->Material.m_dwID)
               break; // unchanged
            if (dwVal)
               pt->Material.InitFromID (dwVal);
            else
               pt->Material.m_dwID = MATERIAL_CUSTOM;

            // eanble/disable button to edit
            PCEscControl pControl;
            pControl = pPage->ControlFind (L"editmaterial");
            if (pControl)
               pControl->Enable (pt->Material.m_dwID ? FALSE : TRUE);

            return TRUE;
         }
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Blank texture for painting";
            return TRUE;
         }
      }
      break;
   }


   return DefPage (pPage, dwMessage, pParam);
}
   





BOOL TextFromFilePage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PTNPAGE pt = (PTNPAGE) pPage->m_pUserData;
   DWORD dwRenderShard = pt->dwRenderShard;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         PCEscControl pControl;

         pControl = pPage->ControlFind (L"name");
         if (pControl)
            pControl->AttribSet (Text(), L"New texture");

         MeasureToString (pPage, L"width", 1);

         // set the material
         ComboBoxSet (pPage, L"material", pt->Material.m_dwID);
         pControl = pPage->ControlFind (L"editmaterial");
         if (pControl)
            pControl->Enable (pt->Material.m_dwID ? FALSE : TRUE);

         pControl = pPage->ControlFind (L"transuse");
         if (pControl)
            pControl->AttribSetBOOL (Checked(), FALSE);
         pControl = pPage->ControlFind (L"transdist");
         if (pControl)
            pControl->AttribSetInt (Pos(), (int)50);

         FillStatusColor (pPage, L"transcolor", pt->cTransColor = RGB(0xff,0xff,0xff));

      }
      break;


   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;

         if (!p->pControl || !p->pControl->m_pszName)
            break;

         if (!_wcsicmp(p->pControl->m_pszName, L"ok")) {
            // get the values
            PCEscControl pControl;
            // file name
            WCHAR szFile[256];
            DWORD dwNeeded;
            szFile[0] = 0;
            pControl = pPage->ControlFind (L"file");
            if (pControl)
               pControl->AttribGet (Text(), szFile, sizeof(szFile), &dwNeeded);
            DWORD dwExt;
            dwExt = (DWORD)wcslen(szFile);
            if (dwExt >= 4)
               dwExt -= 4;
            else
               dwExt = 0;
            BOOL fJPEG;
            if (dwExt && !_wcsicmp(szFile + dwExt, L".jpg"))
               fJPEG = TRUE;
            else if (dwExt && !_wcsicmp(szFile + dwExt, L".bmp"))
               fJPEG = FALSE;
            else {
               // no  file
               pPage->MBWarning (L"Please select an image file to use for the texture.");
               return TRUE;
            }

            // make sure file exists
            CImage Image;
            if (!Image.Init (szFile)) {
               pPage->MBWarning (L"Please select an image file to use for the texture.");
               return TRUE;
            }

            // make sure that have a unique name
            WCHAR szName[128];
            szName[0] = 0;
            pControl = pPage->ControlFind (L"name");
            if (pControl)
               pControl->AttribGet (Text(), szName, sizeof(szName), &dwNeeded);
            if (!szName[0])
               wcscpy (szName, L"New texture");
            UniqueTextureName (dwRenderShard, pt->szMajor, pt->szMinor, szName);


            // make the creator
            PCTextCreatorImageFile pNew;
            pNew = new CTextCreatorImageFile(dwRenderShard, 0);
            if (!pNew)
               return TRUE;
            pNew->m_cDefColor = RGB(0xff,0xff,0xff);
            pNew->m_cTransColor = pt->cTransColor;
            pControl = pPage->ControlFind (L"transdist");
            if (pControl)
               pNew->m_dwTransDist = (DWORD) pControl->AttribGetInt (Pos());
            pControl = pPage->ControlFind (L"transuse");
            if (pControl)
               pNew->m_fTransUse = pControl->AttribGetBOOL (Checked());
            pNew->m_dwX = Image.Width();
            pNew->m_dwY = Image.Height();
            pNew->m_fBumpHeight = .01;
            pControl = pPage->ControlFind (L"cached");
            if (pControl)
               pNew->m_fCached = pControl->AttribGetBOOL (Checked());
            MeasureParseString (pPage, L"width", &pNew->m_fWidth);
            pNew->m_fWidth = max(CLOSE, pNew->m_fWidth);
            memcpy (&pNew->m_Material, &pt->Material, sizeof(pt->Material));
            wcscpy (pNew->m_szFile, szFile);
            pNew->LoadImageFromDisk();

            // add it
            PCMMLNode2 pNode;
            pNode = pNew->MMLTo();
            pNew->Delete();
            if (pNode) {
               GUID gSub;
               AttachDateTimeToMML(pNode);
               GUIDGen (&gSub);

               // note: only doing user
               LibraryTextures(dwRenderShard, TRUE)->ItemRemove (pt->szMajor, pt->szMinor, szName);
               LibraryTextures(dwRenderShard, TRUE)->ItemAdd (pt->szMajor, pt->szMinor, szName,
                  &GTEXTURECODE_ImageFile, &gSub, pNode);
               // NOTE: Not deleting pNode because is kept by LibraryTextures

               // copy new name and guid over
               pt->gCode = GTEXTURECODE_ImageFile;
               pt->gSub = gSub;
               wcscpy (pt->szName, szName);
            }

            // exit
            pPage->Exit(RedoSamePage());
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"editmaterial")) {
            pt->Material.Dialog (pPage->m_pWindow->m_hWnd);
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"browse")) {
            OPENFILENAME   ofn;
            char  szTemp[256];
            szTemp[0] = 0;
            memset (&ofn, 0, sizeof(ofn));
   
            // get what's typed in
            PCEscControl pControl;
            DWORD dwNeeded;
            WCHAR szw[256];
            szw[0] = 0;
            pControl = pPage->ControlFind (L"file");
            if (pControl)
               pControl->AttribGet (Text(), szw, sizeof(szw), &dwNeeded);


            // BUGFIX - Set directory
            char szInitial[256];
            GetLastDirectory(szInitial, sizeof(szInitial));
            if (szw[0]) {
               WideCharToMultiByte (CP_ACP, 0, szw, -1, szInitial, sizeof(szInitial), 0, 0);
            }
            ofn.lpstrInitialDir = szInitial;

            ofn.lStructSize = sizeof(ofn);
            ofn.hwndOwner = pPage->m_pWindow->m_hWnd;
            ofn.hInstance = ghInstance;
            ofn.lpstrFilter = "All (*.bmp,*.jpg)\0*.bmp;*.jpg\0JPEG (*.jpg)\0*.jpg\0Bitmap (*.bmp)\0*.bmp\0\0\0";
            ofn.lpstrFile = szTemp;
            ofn.nMaxFile = sizeof(szTemp);
            ofn.lpstrTitle = "Open image file";
            ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
            ofn.lpstrDefExt = "jpg";
            // nFileExtension 

            if (!GetOpenFileName(&ofn))
               return TRUE;   // failed to specify file so go back

            // convert to unicode
            MultiByteToWideChar (CP_ACP, 0, szTemp, -1, szw, sizeof(szw)/2);
            if (pControl)
               pControl->AttribSet (Text(), szw);
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"changetrans")) {
            COLORREF cr;
            cr = AskColor (pPage->m_pWindow->m_hWnd, pt->cTransColor, pPage, L"transcolor");
            if (cr != pt->cTransColor) {
               pt->cTransColor = cr;
            }
            return TRUE;
         }
      }
      break;

   case ESCN_COMBOBOXSELCHANGE:
      {
         PESCNCOMBOBOXSELCHANGE p = (PESCNCOMBOBOXSELCHANGE) pParam;
         if (!p->pControl->m_pszName)
            break;

         DWORD dwVal;
         dwVal = p->pszName ? _wtoi(p->pszName) : 0;

         if (!_wcsicmp(p->pControl->m_pszName, L"material")) {
            if (dwVal == pt->Material.m_dwID)
               break; // unchanged
            if (dwVal)
               pt->Material.InitFromID (dwVal);
            else
               pt->Material.m_dwID = MATERIAL_CUSTOM;

            // eanble/disable button to edit
            PCEscControl pControl;
            pControl = pPage->ControlFind (L"editmaterial");
            if (pControl)
               pControl->Enable (pt->Material.m_dwID ? FALSE : TRUE);

            return TRUE;
         }
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"New texture from file";
            return TRUE;
         }
      }
      break;
   }


   return DefPage (pPage, dwMessage, pParam);
}
   


BOOL TextureSelPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PTNPAGE pt = (PTNPAGE) pPage->m_pUserData;
   DWORD dwRenderShard = pt->dwRenderShard;
   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         // add enter - close
         ESCACCELERATOR a;
         memset (&a, 0, sizeof(a));
         a.c = VK_ESCAPE;
         a.fAlt = FALSE;
         a.dwMessage = ESCM_CLOSE;
         pPage->m_listESCACCELERATOR.Add (&a);

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

         // set off time if any images are unfinished
         DWORD i;
         PTHUMBINFO pti;
         pt->dwTimerID = 0;
         for (i = 0; i < pt->pThumbInfo->Num(); i++) {
            pti = (PTHUMBINFO) pt->pThumbInfo->Get(i);
            if (pti->fEmpty && !pti->hBitmap)
               break;
         }
         if (i < pt->pThumbInfo->Num())
            pt->dwTimerID = pPage->m_pWindow->TimerSet (100, pPage); // first one right away

         // set the transparency
         PCEscControl pControl;
         ComboBoxSet (pPage, L"material", pt->pSurf->m_Material.m_dwID);
         pControl = pPage->ControlFind (L"editmaterial");
         if (pControl)
            pControl->Enable (pt->pSurf->m_Material.m_dwID ? FALSE : TRUE);

         // fill in the list box
         PCSurfaceSchemeSocket pss = pt->pWorld->SurfaceSchemeGet();
         CMem mem;
         MemZero (&mem);
         DWORD dwSel = 0;

         for (i = 0; ; i++) {
            PCObjectSurface ps;
            ps = pss->SurfaceEnum (i);
            if (!ps)
               break;

            // do we set this to the selection
            if (pt->pSurf->m_szScheme[0] && !_wcsicmp(ps->m_szScheme, pt->pSurf->m_szScheme))
               dwSel = i+1;

            // make a string from this
            MemCat (&mem, L"<elem name=\"");
            MemCatSanitize (&mem, ps->m_szScheme);
            MemCat (&mem, L"\"><bold>");
            MemCatSanitize (&mem, ps->m_szScheme);
            MemCat (&mem, L"</bold><small><italic> (");
            if (ps->m_fUseTextureMap)
               MemCat (&mem, L"Texture");
            else
               MemCat (&mem, L"Solid color");
            MemCat (&mem, L")</italic></small></elem>");

            delete ps;
         }

         ESCMCOMBOBOXADD lba;
         ESCMCOMBOBOXSELECTSTRING ess;
         memset (&lba, 0, sizeof(lba));
         lba.dwInsertBefore = -1;
         lba.pszMML = (PWSTR) mem.p;
         memset (&ess, 0, sizeof(ess));
         ess.iStart = -1;
         ess.fExact = TRUE;
         ess.psz = pt->pSurf->m_szScheme[0] ? pt->pSurf->m_szScheme : L"AAAAAcustom";
         
         pControl = pPage->ControlFind (L"schemes");
         if (pControl) {
            pControl->Message (ESCM_COMBOBOXADD, &lba);

            // set the selection
            pControl->Message (ESCM_COMBOBOXSELECTSTRING, &ess);
         }

      }
      break;

   case ESCM_DESTRUCTOR:
      // kill the timer
      if (pt->dwTimerID)
         pPage->m_pWindow->TimerKill (pt->dwTimerID);
      pt->dwTimerID = 0;
      break;


   case ESCM_LINK:
      {
         PESCMLINK p = (PESCMLINK) pParam;
         if (!p->psz)
            break;

         if (!_wcsicmp(p->psz, L"textblank")) {
            // Settings dialog
            CEscWindow cWindow;
            RECT r;
            DialogBoxLocation (pPage->m_pWindow->m_hWnd, &r);

            cWindow.Init (ghInstance, pPage->m_pWindow->m_hWnd, EWS_FIXEDSIZE, &r);
            PWSTR pszRet;

            pt->Material.InitFromID (MATERIAL_PAINTSEMIGLOSS);
            pszRet = cWindow.PageDialog (ghInstance, IDR_MMLTEXTBLANK, TextBlankPage, pt);
            if (!pszRet || _wcsicmp(pszRet, RedoSamePage()))
               return TRUE;   // absorb this
            break; // allow link to fall through
         }
         else if (!_wcsicmp(p->psz, L"textfromfile")) {
            // Settings dialog
            CEscWindow cWindow;
            RECT r;
            DialogBoxLocation (pPage->m_pWindow->m_hWnd, &r);

            cWindow.Init (ghInstance, pPage->m_pWindow->m_hWnd, EWS_FIXEDSIZE, &r);
            PWSTR pszRet;

            pt->Material.InitFromID (MATERIAL_PAINTSEMIGLOSS);
            pszRet = cWindow.PageDialog (ghInstance, IDR_MMLTEXTFROMFILE, TextFromFilePage, pt);
            if (!pszRet || _wcsicmp(pszRet, RedoSamePage()))
               return TRUE;   // absorb this
            break;   // fall through
         }
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
            if (dwVal != pt->pSurf->m_Material.m_dwID) {
               if (dwVal)
                  pt->pSurf->m_Material.InitFromID (dwVal);
               else
                  pt->pSurf->m_Material.m_dwID = MATERIAL_CUSTOM;
               pt->fChanged = TRUE;

               // eanble/disable button to edit
               PCEscControl pControl;
               pControl = pPage->ControlFind (L"editmaterial");
               if (pControl)
                  pControl->Enable (pt->pSurf->m_Material.m_dwID ? FALSE : TRUE);
            }

            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"schemes")) {
            // it is any different
            PWSTR pszNone = L"AAAAAcustom";
            if (!p->pszName)
               break;   // cant do this

            if (!_wcsicmp(p->pszName, pt->pSurf->m_szScheme[0] ? pt->pSurf->m_szScheme : pszNone))
               break;   // no change

            if (!_wcsicmp(p->pszName, pszNone)) {
               // set to not being in one. Dont do anything else
               pt->pSurf->m_szScheme[0] = 0;
            }
            else {
               // get the value and convert that to a string
               PCObjectSurface ps;
               ps = (pt->pWorld->SurfaceSchemeGet())->SurfaceGet (p->pszName, NULL, TRUE);
               if (!ps)
                  break;   // cant do this eitehr
               memcpy (pt->pSurf, ps, sizeof(CObjectSurface));
               // since noclone - delete ps;
            }
            pt->fChanged = TRUE;

            // update the display
            pPage->Exit (RedoSamePage());
            return TRUE;
         }

      }
      break;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;

         if (!p->pControl || !p->pControl->m_pszName)
            break;

         if (!_wcsicmp(p->pControl->m_pszName, L"texturetint")) {
            if (TextureTintDialog (dwRenderShard, pPage->m_pWindow->m_hWnd, pt->pSurf))
               pt->fChanged = TRUE;
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"editmaterial")) {
            if (pt->pSurf->m_Material.Dialog(pPage->m_pWindow->m_hWnd))
               pt->fChanged = TRUE;
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"changecolor")) {
            // color dialog
            COLORREF c;
            c = AskColor (pPage->m_pWindow->m_hWnd, pt->pSurf->m_cColor, NULL, NULL);
            if (c != pt->pSurf->m_cColor) {
               // changed the color
               pt->pSurf->m_cColor = c;
               pt->fChanged = TRUE;
               pPage->Exit (RedoSamePage());
            }
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"addscheme")) {
            CEscWindow cWindow;
            RECT r;
            DialogBoxLocation2 (pPage->m_pWindow->m_hWnd, &r);
            cWindow.Init (ghInstance, pPage->m_pWindow->m_hWnd, EWS_FIXEDSIZE | EWS_AUTOHEIGHT | EWS_NOTITLE, &r);
            PWSTR pszRet;

            pszRet = cWindow.PageDialog (ghInstance, IDR_MMLOBJPAINTADDSCHEME, ObjPaintAddSchemePage, pt);
            pPage->Exit (RedoSamePage());
            return TRUE;
         }
      }
      break;

   case ESCM_TIMER:
      {
         pPage->m_pWindow->TimerKill (pt->dwTimerID);
         pt->dwTimerID = 0;

         DWORD i;
         PTHUMBINFO pti;
         for (i = 0; i < pt->pThumbInfo->Num(); i++) {
            pti = (PTHUMBINFO) pt->pThumbInfo->Get(i);
            if (pti->fEmpty && !pti->hBitmap)
               break;
         }
         if (i >= pt->pThumbInfo->Num())
            return TRUE;

         // create
         pPage->m_pWindow->SetCursor (IDC_NOCURSOR);
         GUID gCode, gSub;
         TextureGUIDsFromName (dwRenderShard, pt->szMajor, pt->szMinor, pti->pszName, &gCode, &gSub);
         TextureCreateThumbnail (dwRenderShard, &gCode, &gSub, FALSE);
         COLORREF cTransparent;
         pti->hBitmap = ThumbnailGet()->ThumbnailToBitmap (&gCode, &gSub, pPage->m_pWindow->m_hWnd,
            &cTransparent);
         if (pti->hBitmap) {
            pti->fEmpty = FALSE;

            WCHAR szTemp[32];
            swprintf (szTemp, L"bitmap%d", (int) i);
            PCEscControl pControl;
            pControl = pPage->ControlFind (szTemp);
            swprintf (szTemp, L"%lx", (__int64) pti->hBitmap);
            if (pControl)
               pControl->AttribSet (L"hbitmap", szTemp);
         }
         pPage->m_pWindow->SetCursor (IDC_HANDCURSOR);

         // set another timer
         for (i = 0; i < pt->pThumbInfo->Num(); i++) {
            pti = (PTHUMBINFO) pt->pThumbInfo->Get(i);
            if (pti->fEmpty && !pti->hBitmap)
               break;
         }
         if (i < pt->pThumbInfo->Num())
            pt->dwTimerID = pPage->m_pWindow->TimerSet (250, pPage);
      }
      return TRUE;

   }

   return DefPage (pPage, dwMessage, pParam);
}



/****************************************************************************
DefaultSurfFromNewTexture - Given that a new texture was selected, fills
in pSurf with some intiial values.

inputs
   GUID     *pgCode - Code
   GUID     *pgSub - Subcode
   PCObjectSurface pSurf - Filled in with defaults
returns
   BOOL - TRUE if success
*/
DLLEXPORT BOOL DefaultSurfFromNewTexture (DWORD dwRenderShard, GUID *pgCode, GUID *pgSub, PCObjectSurface pSurf)
{
   pSurf->m_gTextureCode = *pgCode;
   pSurf->m_gTextureSub = *pgSub;

   pSurf->m_fUseTextureMap = TRUE;

   // since just created a new texture, set up some values
   pSurf->m_TextureMods.cTint = RGB(0xff,0xff,0xff);
   pSurf->m_TextureMods.wBrightness = 0x1000;
   pSurf->m_TextureMods.wContrast = 0x1000;
   pSurf->m_TextureMods.wSaturation = 0x1000;
   pSurf->m_TextureMods.wHue = 0;
   pSurf->m_afTextureMatrix[0][0] = pSurf->m_afTextureMatrix[1][1] = 1.0;
   pSurf->m_afTextureMatrix[0][1] = pSurf->m_afTextureMatrix[1][0] = 0.0;
   pSurf->m_mTextureMatrix.Identity();

   // get the teture
   PCTextureMapSocket pm;  // BUGFIX - Was PCTextureMap
   pm = TextureCreate (dwRenderShard, &pSurf->m_gTextureCode, &pSurf->m_gTextureSub);
   if (!pm)
      return FALSE;
   pm->MaterialGet (0, &pSurf->m_Material);
   pm->TextureModsSet (&pSurf->m_TextureMods);

   // get the texture's default width and height
   fp fx, fy;
   pm->DefScaleGet (&fx, &fy);
   pSurf->m_afTextureMatrix[0][0] = 1.0 / fx;
   pSurf->m_afTextureMatrix[1][1] = 1.0 / fy;
   pm->Delete();  // BUGFIX - Was delete pm;

   return TRUE;
}


/****************************************************************************
DefaultSurfFromNewTexture - Given that a new texture was selected, fills
in pSurf with some intiial values.

inputs
   PWSTR       pszMajor, pszMinor, pszName - Name of texture
   PCObjectSurface pSurf - Filled in with defaults
returns
   BOOL - TRUE if success
*/
BOOL DefaultSurfFromNewTexture (DWORD dwRenderShard, PWSTR pszMajor, PWSTR pszMinor, PWSTR pszName, PCObjectSurface pSurf)
{
   GUID gCode, gSub;
   if (!TextureGUIDsFromName (dwRenderShard, pszMajor, pszMinor, pszName, &gCode, &gSub))
      return FALSE;

   return DefaultSurfFromNewTexture (dwRenderShard, &gCode, &gSub, pSurf);
}

typedef struct {
   COLORREF    rgb;
   COLORREF    hls;
} MYHLS, *PMYHLS;

static int _cdecl HLSCompare (const void *elem1, const void *elem2)
{
   MYHLS *pdw1, *pdw2;
   pdw1 = (MYHLS*) elem1;
   pdw2 = (MYHLS*) elem2;

   return (int)(pdw1->hls) - (int)(pdw2->hls);
}

/****************************************************************************
TextureSelDialog - This brings up the UI for changing CObjectSurface. It's
passed in an object surface that's manupulated (and changed). Retrusn TRUE
if the user presses OK, FALSE if they press cancel. If the user presses OK
the object surface should probably be set.

inputs
   HWND           hWnd - Window to create this over
   PCObjectSurface   pSurf - Surface - modified if returns TRUE
   PCWorldSocket        pWorld - World where to get schemes from
returns
   BOOL - TRUE if the user presses OK, FALSE if not
*/
BOOL TextureSelDialog (DWORD dwRenderShard, HWND hWnd, PCObjectSurface pOrigSurf, PCWorldSocket pWorld)
{
   PWSTR pszMajorCat = L"mcat:", pszMinorCat = L"icat:", pszNameCat = L"obj:", pszColorCat = L"col:";
   DWORD dwMajorCat = (DWORD)wcslen(pszMajorCat), dwMinorCat = (DWORD)wcslen(pszMinorCat), dwNameCat = (DWORD)wcslen(pszNameCat),
      dwColorCat = (DWORD)wcslen(pszColorCat);
   CObjectSurface Surf;
   memcpy (&Surf, pOrigSurf, sizeof(Surf));

   // if it's a scheme then switch to that
   if (Surf.m_szScheme[0]) {
      PCObjectSurface pScheme = (pWorld->SurfaceSchemeGet())->SurfaceGet (Surf.m_szScheme, NULL, TRUE);
      if (pScheme) {
         memcpy (&Surf, pScheme, sizeof(Surf));
         // since no clone delete pScheme;
      }
      else
         Surf.m_szScheme[0] = 0; // doesnt exist
   }


   CEscWindow cWindow;
   RECT r;
   DialogBoxLocation2 (hWnd, &r);

   // how many columns
   DWORD dwColumns;
   dwColumns = (DWORD)max(r.right - r.left,0) / (TEXTURETHUMBNAIL * 5 / 4);
   dwColumns = max(dwColumns, 3);

   cWindow.Init (ghInstance, hWnd, EWS_FIXEDSIZE, &r);
   PWSTR pszRet;

   // set up the info
   TNPAGE t;
   memset (&t, 0, sizeof(t));
   t.pSurf = &Surf;
   t.pWorld = pWorld;
   t.dwRenderShard = dwRenderShard;

   //HDC hDC;
   //hDC = GetDC (hWnd);
   //if (t.hBit)
   //   DeleteObject (t.hBit);
   //t.hBit = t.pImage->ToBitmap (hDC);
   //ReleaseDC (hWnd, hDC);

   CListFixed lThumbInfo;
   lThumbInfo.Init (sizeof(THUMBINFO));
   t.pThumbInfo = &lThumbInfo;

redoscheme:
   // get the name of the object
   WCHAR szMajor[128], szMinor[128], szName[128];
   if (Surf.m_fUseTextureMap && !TextureNameFromGUIDs(dwRenderShard, &Surf.m_gTextureCode, &Surf.m_gTextureSub, szMajor, szMinor, szName))
      Surf.m_fUseTextureMap = FALSE;
   if (!Surf.m_fUseTextureMap) {
      szMajor[0] = szMinor[0] = szName[0] = 0;
   }
   wcscpy (t.szMajor, szMajor);
   wcscpy (t.szMinor, szMinor);
   wcscpy (t.szName, szName);

redopage:
   // create a bitmap for error
   HBITMAP hBlank;
   HDC hDCBlank, hDCWnd;
   hDCWnd = GetDC (hWnd);
   hDCBlank = CreateCompatibleDC (hDCWnd);
   hBlank = CreateCompatibleBitmap (hDCWnd, TEXTURETHUMBNAIL, TEXTURETHUMBNAIL);
   SelectObject (hDCBlank, hBlank);
   ReleaseDC (hWnd, hDCWnd);
   RECT rBlank;
   rBlank.left = rBlank.top = 0;
   rBlank.right = rBlank.bottom = TEXTURETHUMBNAIL;
   FillRect (hDCBlank, &rBlank, (HBRUSH) GetStockObject (BLACK_BRUSH));
   // draw the text
   HFONT hFont;
   LOGFONT  lf;
   memset (&lf, 0, sizeof(lf));
   lf.lfHeight = -MulDiv(12, GetDeviceCaps(hDCBlank, LOGPIXELSY), 72); 
   lf.lfCharSet = DEFAULT_CHARSET;
   lf.lfPitchAndFamily = FF_SWISS | VARIABLE_PITCH;
   strcpy (lf.lfFaceName, "Arial");
   hFont = CreateFontIndirect (&lf);
   SelectObject (hDCBlank, hFont);
   SetTextColor (hDCBlank, RGB(0xff,0xff,0xff));
   SetBkMode (hDCBlank, TRANSPARENT);
   DrawText(hDCBlank, "Drawing...", -1, &rBlank, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_SINGLELINE);
   DeleteObject (hFont);
   DeleteDC (hDCBlank);

   // create the page
   CListFixed  lNames;
   DWORD i;
   PWSTR psz;
   BOOL fBold;
   MemZero (&gMemTemp);
   MemCat (&gMemTemp, L"<?Include resource=500?>"
            L"<PageInfo index=false title=\"Change an object's color or texture\"/>"
            L"<colorblend tcolor=#000040 bcolor=#000080 posn=background/>"
            L"<font color=#ffffff><table width=100%% innerlines=0 bordercolor=#c0c0c0 valign=top><tr>");

   // major category
   MemCat (&gMemTemp, L"<td bgcolor=#000000 lrmargin=0 tbmargin=0 width=");
   MemCat (&gMemTemp, (int) 100 / (int) dwColumns);
   MemCat (&gMemTemp, L"%%>");
   MemCat (&gMemTemp, L"<table width=100%% border=0 innerlines=0>");
   TextureEnumMajor (dwRenderShard, &lNames);
   if (t.pSurf->m_fUseTextureMap) {
      // make sure major is valid
      for (i = 0; i < lNames.Num(); i++) {
         psz = *((PWSTR*) lNames.Get(i));
         if (!_wcsicmp(psz, t.szMajor))
            break;
      }
      if (i >= lNames.Num())
         wcscpy (t.szMajor, *((PWSTR*) lNames.Get(0)));
   }

   // add the solid color
   MemCat (&gMemTemp, L"<tr><td");
   if (!t.pSurf->m_fUseTextureMap) {
      MemCat (&gMemTemp, L" bgcolor=#101020");
      fBold = TRUE;
   }
   else
      fBold = FALSE;

   MemCat (&gMemTemp, L">");
   MemCat (&gMemTemp, L"<a href=\"mcat:\">");
   if (fBold)
      MemCat (&gMemTemp, L"<font color=#ffffff>");
   MemCat (&gMemTemp, L"<italic>Solid color</italic>");
   if (fBold)
      MemCat (&gMemTemp, L"</font>");
   MemCat (&gMemTemp, L"</a>");
   MemCat (&gMemTemp, L"</td></tr>");

   // add textures
   for (i = 0; i < lNames.Num(); i++) {
      psz = *((PWSTR*) lNames.Get(i));

      MemCat (&gMemTemp, L"<tr><td");
      if (t.pSurf->m_fUseTextureMap && !_wcsicmp(psz, t.szMajor)) {
         MemCat (&gMemTemp, L" bgcolor=#101020");
         fBold = TRUE;
      }
      else
         fBold = FALSE;

      MemCat (&gMemTemp, L">");
      MemCat (&gMemTemp, L"<a href=\"mcat:");
      MemCatSanitize (&gMemTemp, psz);
      MemCat (&gMemTemp, L"\">");
      if (fBold)
         MemCat (&gMemTemp, L"<font color=#ffffff>");
      MemCatSanitize (&gMemTemp, psz);
      if (fBold)
         MemCat (&gMemTemp, L"</font>");
      MemCat (&gMemTemp, L"</a>");
      MemCat (&gMemTemp, L"</td></tr>");
   }
   MemCat (&gMemTemp, L"</table>");
   MemCat (&gMemTemp, L"</td>");

   if (!t.pSurf->m_fUseTextureMap) {
      MemCat (&gMemTemp, L"<td bgcolor=#101020 width=");
      MemCat (&gMemTemp, (int) 100 * (int)(dwColumns - 1) / (int) dwColumns);
      MemCat (&gMemTemp, L"%%>");

      // make sure this color is in the list
      TextureAddTextureColorList (dwRenderShard, t.pSurf);

      // get all the colors from the list and then sort them
      CListFixed  lColor;
      lColor.Init (sizeof(MYHLS));
      MYHLS my;
      float f1,f2,f3;
      lColor.Required (gaTreeColorList[dwRenderShard].Num());
      for (i = 0; i < gaTreeColorList[dwRenderShard].Num(); i++) {
         PWSTR psz = gaTreeColorList[dwRenderShard].Enum(i);
         my.rgb = _wtoi (psz);

         // convert to HLS first
         ToHLS256 (GetRValue(my.rgb), GetGValue (my.rgb), GetBValue (my.rgb), &f1, &f2, &f3);
         my.hls = RGB ((BYTE)f3, (BYTE)f2, (BYTE)f1);
         lColor.Add (&my);
      }
      // Sort by HLS
      qsort (lColor.Get(0), lColor.Num(), sizeof(MYHLS), HLSCompare);

      // draw colors
      MemCat (&gMemTemp, L"<table width=100%% border=0 innerlines=0>");
      DWORD j;
      for (i = 0; i < lColor.Num(); ) {
         MemCat (&gMemTemp, L"<tr>");
         for (j = 0; j < (dwColumns-1)*2; j++) {
            MemCat (&gMemTemp, L"<td width=");
            MemCat (&gMemTemp, (int) 100 / (int)(dwColumns - 1) / 2);
            MemCat (&gMemTemp, L"%%>");

            if (i + j < lColor.Num()) {
               PMYHLS pmy;
               MemCat (&gMemTemp, L"<p align=center>");

               pmy = (PMYHLS) lColor.Get(i+j);

               // bitmap
               MemCat (&gMemTemp, L"<table width=50 height=50 lrmargin=0 tbmargin=0 ");
               if (pmy->rgb == t.pSurf->m_cColor)
                  MemCat (&gMemTemp, L"border=4 bordercolor=#ff0000");
               else
                  MemCat (&gMemTemp, L"border=2 bordercolor=#ffffff");
               MemCat (&gMemTemp, L"><tr><td>");
               MemCat (&gMemTemp, L"<colorblend width=100%% height=100%% color=");
               WCHAR szTemp[32];
               ColorToAttrib (szTemp, pmy->rgb);
               MemCat (&gMemTemp, szTemp);
               MemCat (&gMemTemp, L" href=col:");
               MemCat (&gMemTemp, (int) pmy->rgb);
               MemCat (&gMemTemp, L"/>");
               MemCat (&gMemTemp, L"</td></tr></table>");
               MemCat (&gMemTemp, L"</p>");
            }
         
            MemCat (&gMemTemp, L"</td>");
         }
         MemCat (&gMemTemp, L"</tr>");
         i += (dwColumns-1)*2;
      }
      MemCat (&gMemTemp, L"</table>");

      MemCat (&gMemTemp,
         L"<p/><xchoicebutton name=changecolor>"
         L"<bold>Add a new color</bold><br/>"
         L"Press this if the color you want isn't on the list of currently "
         L"available colors."
         L"</xchoicebutton>");


      MemCat (&gMemTemp, L"</td>");
   }
   else {  // suing texture map
      // minor categories
      MemCat (&gMemTemp, L"<td lrmargin=0 bgcolor=#101020 tbmargin=0 width=");
      MemCat (&gMemTemp, (int) 100 / (int) dwColumns);
      MemCat (&gMemTemp, L"%%>");
      MemCat (&gMemTemp, L"<table width=100%% border=0 innerlines=0>");
      TextureEnumMinor (dwRenderShard, &lNames, t.szMajor);
      // make sure minor is valid
      for (i = 0; i < lNames.Num(); i++) {
         psz = *((PWSTR*) lNames.Get(i));
         if (!_wcsicmp(psz, t.szMinor))
            break;
      }
      if ((i >= lNames.Num()) && lNames.Num())
         wcscpy (t.szMinor, *((PWSTR*) lNames.Get(0)));
      for (i = 0; i < lNames.Num(); i++) {
         psz = *((PWSTR*) lNames.Get(i));

         MemCat (&gMemTemp, L"<tr><td");
         if (!_wcsicmp(psz, t.szMinor)) {
            MemCat (&gMemTemp, L" bgcolor=#202040");
            fBold = TRUE;
         }
         else
            fBold = FALSE;

         MemCat (&gMemTemp, L">");
         MemCat (&gMemTemp, L"<a href=\"icat:");
         MemCatSanitize (&gMemTemp, psz);
         MemCat (&gMemTemp, L"\">");
         if (fBold)
            MemCat (&gMemTemp, L"<font color=#ffffff>");
         MemCatSanitize (&gMemTemp, psz);
         if (fBold)
            MemCat (&gMemTemp, L"</font>");
         MemCat (&gMemTemp, L"</a>");
         MemCat (&gMemTemp, L"</td></tr>");
      }
      MemCat (&gMemTemp, L"</table>");
      MemCat (&gMemTemp, L"</td>");

      // textures within major and minor
      MemCat (&gMemTemp, L"<td lrmargin=0 tbmargin=0 bgcolor=#202040 width=");
      MemCat (&gMemTemp, (int) 100 * (int)(dwColumns - 2) / (int) dwColumns);
      MemCat (&gMemTemp, L"%%>");
      MemCat (&gMemTemp, L"<table width=100%% border=0 innerlines=0>");
      TextureEnumItems (dwRenderShard, &lNames, t.szMajor, t.szMinor);
      // make sure minor is valid
      for (i = 0; i < lNames.Num(); i++) {
         psz = *((PWSTR*) lNames.Get(i));
         if (!_wcsicmp(psz, t.szName))
            break;
      }
      if ((i >= lNames.Num()) && lNames.Num())
         wcscpy (t.szName, *((PWSTR*) lNames.Get(0)));
      // fill in the list of bitmaps
      lThumbInfo.Clear();
      DWORD j;
      for (i = 0; i < lNames.Num() + 2; ) {
         MemCat (&gMemTemp, L"<tr>");
         for (j = 0; j < dwColumns-2; j++) {
            MemCat (&gMemTemp, L"<td width=");
            MemCat (&gMemTemp, (int) 100 / (int)(dwColumns - 2));
            MemCat (&gMemTemp, L"%%>");

            if (i + j == lNames.Num()) {
               // button to read in from bitmap
               //MemCat (&gMemTemp, L"<p align=center>");
               MemCat (&gMemTemp, L"<a color=#ffffff href=textfromfile>New texture from file</a>");
               MemCat (&gMemTemp, L"<small><font color=#c0c0c0> - Create a new texture using a .bmp or .jpg from "
                  L"another application.</font></small>");
               //MemCat (&gMemTemp, L"</p>");
            }
            else if (i + j == lNames.Num()+1) {
               // button to create blank
               //MemCat (&gMemTemp, L"<p align=center>");
               MemCat (&gMemTemp, L"<a color=#ffffff href=textblank>Blank texture for painting</a>");
               MemCat (&gMemTemp, L"<small><font color=#c0c0c0> - Create a new texture that you can paint on using "
                  L"the painting view.</font></small>");
               //MemCat (&gMemTemp, L"</p>");
            }
            if (i + j < lNames.Num()) {
               psz = *((PWSTR*) lNames.Get(i+j));
               MemCat (&gMemTemp, L"<p align=center>");

               THUMBINFO ti;
               ti.fEmpty = TRUE;
               TextureGUIDsFromName (dwRenderShard, t.szMajor, t.szMinor, psz, &t.gCode, &t.gSub);
               ti.hBitmap = ThumbnailGet()->ThumbnailToBitmap (&t.gCode, &t.gSub, cWindow.m_hWnd, &ti.cTransparent);
               ti.pszName = *((PWSTR*) lNames.Get(i+j));
               if (!ti.hBitmap)
                  ti.cTransparent = 0;
               lThumbInfo.Add (&ti);

               // bitmap
               MemCat (&gMemTemp, L"<image hbitmap=");
               WCHAR szTemp[32];
               swprintf (szTemp, L"%lx", ti.hBitmap ? (__int64) ti.hBitmap : (__int64)hBlank);
               MemCat (&gMemTemp, szTemp);
               if (ti.cTransparent != -1) {
                  MemCat (&gMemTemp, L" transparent=true transparentdistance=0 transparentcolor=");
                  ColorToAttrib (szTemp, ti.cTransparent);
                  MemCat (&gMemTemp, szTemp);
               }

               MemCat (&gMemTemp, L" name=bitmap");
               MemCat (&gMemTemp, (int) i+j);
               // Show if it the current texture
               if (!_wcsicmp(szName, psz) && !_wcsicmp(szMajor, t.szMajor) && !_wcsicmp(szMinor, t.szMinor))
                  MemCat (&gMemTemp, L" border=4 bordercolor=#ff0000");
               else if (TextureIsInTextureList(dwRenderShard, &t.gCode, &t.gSub))
                  MemCat (&gMemTemp, L" border=2 bordercolor=#ffffff");
               else
                  MemCat (&gMemTemp, L" border=2 bordercolor=#000000");
               MemCat (&gMemTemp, L" href=\"obj:");
               MemCatSanitize (&gMemTemp, psz);
               MemCat (&gMemTemp, L"\"/>");

               MemCat (&gMemTemp, L"<br/>");
               MemCat (&gMemTemp, L"<small><a color=#ffffff href=\"obj:");
               MemCatSanitize (&gMemTemp, psz);
               MemCat (&gMemTemp, L"\">");
               MemCatSanitize (&gMemTemp, psz);
               MemCat (&gMemTemp, L"</a></small>");
               MemCat (&gMemTemp, L"</p>");
            }
         
            MemCat (&gMemTemp, L"</td>");
         }
         MemCat (&gMemTemp, L"</tr>");
         i += dwColumns-2;
      }
      MemCat (&gMemTemp, L"</table>");
      MemCat (&gMemTemp, L"</td>");
      }  // if pSurf->m_fTextureMap

   // finish off the row
   MemCat (&gMemTemp, L"</tr>");
   MemCat (&gMemTemp, L"</table>");

   // scheme
   MemCat (&gMemTemp,
      L"<p/>"
      L"<table width=100%% bordercolor=#c0c0c0>"
	   L"<xtrheader>Color/texture scheme</xtrheader>"
      L"<tr>"
      );
   MemCat (&gMemTemp, L"<td>"
      L"<bold>Scheme</bold> - If the surface is part of a scheme then "
      L"changing the color of this surface will change the color of all other surfaces "
      L"using the same scheme."
      L"</td>");
   MemCat (&gMemTemp, L"<td>"
      L"<bold><combobox name=schemes width=100%% cbheight=200 sort=true>"
      L"<elem name=AAAAAcustom><font color=#00ffff><bold>Not in a scheme</bold></font></elem>"
      L"</combobox></bold>"
      L"<br/>"
      L"<button name=addscheme>"
      L"<bold>Add scheme...</bold><br/>"
		L"Click on this to add a new scheme."
      L"</button>"
      L"</td>");
   MemCat (&gMemTemp, L"</tr>");
   MemCat (&gMemTemp, L"</table>");

   MemCat (&gMemTemp, L"<p/>");

   // material buttons
   MemCat (&gMemTemp,
      L"<table width=100%% bordercolor=#c0c0c0>"
	   L"<xtrheader>Advanced</xtrheader>"
      L"<tr>"
      L"<td>"
      L"<bold>Material</bold> - Affects the surface's glossiness and"
      L"transparency. <italic>(This does not affect some textures. You need to use"
      L"\"Shadows\" image quality to see most effects.)</italic>"
      L"</td>"
      L"<td><xcombomaterial width=100%% name=material/></td>"
      L"</tr>"
      L"<tr><td>"
      L"<xchoicebutton name=editmaterial>"
      L"<bold>Modify custom material</bold><br/>"
      L"If you select the \"Custom\" material from above, then press this button to"
      L"hand-modify the glossiness, transparency, translucency, and self-illumination."
      L"</xchoicebutton>"
      L"</td></tr>"
      );

   // add a button to modify the texture tint,e tc.
   if (t.pSurf->m_fUseTextureMap)
      MemCat (&gMemTemp,
         L"<tr><td>"
         L"<xchoicebutton name=texturetint>"
         L"<bold>Change the texture's size and coloration</bold><br/>"
         L"If you find a texture that's close to what you want, but its color is slightly "
         L"off or it's the wrong size, first click on the texture with the control-button "
         L"held down, and then press this button."
         L"</xchoicebutton>"
         L"</td></tr>");

   MemCat (&gMemTemp,
      L"</table>");

   // add button for clearing
   MemCat (&gMemTemp, L"<p/><xbr/>"
      L"<xchoicebutton style=righttriangle href=clearcache>"
      L"<bold>Refresh thumbnails</bold><br/>"
      L"The thumbnails you see are generated the first time they're needed and then "
      L"saved to disk. From then on the saved images are used. If you install a new "
      L"version of " APPLONGNAMEW L", some textures may have changed; you can "
      L"press this button to refresh all your thumbnails. Otherise, ignore it."
      L"</xchoicebutton>");
   MemCat (&gMemTemp,
      L"<xchoicebutton style=righttriangle href=clearlist>"
      L"<bold>Refresh the list of textures and colors</bold><br/>"
      L"Textures that are already used in the house are outlined in white. "
      L"Colors already used are displayed in the colors list. "
      L"The list of textures and colors used in the house is created when the file is loaded. "
      L"Sometimes this list gets out-of-date. "
      L"Press this button to update the list if you need the most up-to-date information."
      L"</xchoicebutton>");
   MemCat (&gMemTemp, L"</font>");

   // start with the first page
   pszRet = cWindow.PageDialog (ghInstance, (PWSTR) gMemTemp.p, TextureSelPage, &t);
   t.iVScroll = cWindow.m_iExitVScroll;

   // free bitmaps
   if (hBlank)
      DeleteObject (hBlank);
   for (i = 0; i < lThumbInfo.Num(); i++) {
      PTHUMBINFO pti = (PTHUMBINFO) lThumbInfo.Get(i);
      if (pti->hBitmap)
         DeleteObject (pti->hBitmap);
   }


   if (!pszRet)
      goto alldone;
   if (!_wcsicmp(pszRet, RedoSamePage()))
      goto redoscheme;
   else if (!_wcsicmp(pszRet, L"clearcache")) {
      ThumbnailGet()->ThumbnailClearAll (FALSE);
      goto redopage;
   }
   else if (!_wcsicmp(pszRet, L"clearlist")) {
      TextureRebuildTextureColorList (dwRenderShard);
      EscChime (ESCCHIME_INFORMATION);
      goto redopage;
   }
   else if (!wcsncmp(pszRet, pszMajorCat, dwMajorCat)) {
      wcscpy (t.szMajor, pszRet + dwMajorCat);

      // handle switching between texture map and non-texture map
      if (!t.szMajor[0]) {
         if (t.pSurf->m_fUseTextureMap)
            t.fChanged = TRUE;
         t.pSurf->m_fUseTextureMap = FALSE;
      }
      else {
         if (!t.pSurf->m_fUseTextureMap)
            t.fChanged = TRUE;
         t.pSurf->m_fUseTextureMap = TRUE;
      }

      goto redopage;
   }
   else if (!wcsncmp(pszRet, pszMinorCat, dwMinorCat)) {
      wcscpy (t.szMinor, pszRet + dwMinorCat);
      goto redopage;
   }
   else if (!wcsncmp(pszRet, pszNameCat, dwNameCat)) {
      BOOL fControl = (GetKeyState (VK_CONTROL) < 0);
      wcscpy (t.szName, pszRet + dwNameCat);
      // rmember the names in case changes
      wcscpy (szMajor, t.szMajor);
      wcscpy (szMinor, t.szMinor);
      wcscpy (szName, t.szName);
      t.fChanged = TRUE;

      // get the guids
      DefaultSurfFromNewTexture (dwRenderShard, t.szMajor, t.szMinor, t.szName, t.pSurf);

      if (fControl)  // select, but stay in same page
         goto redopage;
      else
         goto alldone;
   }
   else if (!_wcsicmp(pszRet, L"textblank") || !_wcsicmp(pszRet, L"textfromfile")) {
      // rmember the names in case changes
      wcscpy (szMajor, t.szMajor);
      wcscpy (szMinor, t.szMinor);
      wcscpy (szName, t.szName);
      t.fChanged = TRUE;

      // get the guids
      DefaultSurfFromNewTexture (dwRenderShard, t.szMajor, t.szMinor, t.szName, t.pSurf);

      goto alldone;
   }
   else if (!wcsncmp(pszRet, pszColorCat, dwColorCat)) {
      BOOL fControl = (GetKeyState (VK_CONTROL) < 0);
      t.pSurf->m_cColor = _wtoi(pszRet + dwColorCat);
      t.pSurf->m_fUseTextureMap = FALSE;
      t.fChanged = TRUE;

      if (fControl)  // select, but stay in same page
         goto redopage;
      else
         goto alldone;
   }
   
alldone:
   DWORD dwID;

   if (t.fChanged && Surf.m_szScheme[0]) {
      pWorld->SurfaceAboutToChange (Surf.m_szScheme);
      (pWorld->SurfaceSchemeGet())->SurfaceSet (&Surf);
      pWorld->SurfaceChanged (Surf.m_szScheme);
   }

   dwID = pOrigSurf->m_dwID;  // to restore back to original
   if (t.fChanged) {
      memcpy (pOrigSurf, &Surf, sizeof(Surf));
      pOrigSurf->m_dwID = dwID;

      // remember new textures and colors
      TextureAddTextureColorList (dwRenderShard, &Surf);
   }
   return t.fChanged;
}


/******************************************************************************
TextureIsInTextureList - Returns TRUE if the texture appears in the list of
textures that we think are in the world, FALSE if not
*/
BOOL TextureIsInTextureList (DWORD dwRenderShard, GUID *pgCode, GUID *pgSub)
{
   WCHAR szTemp[sizeof(GUID)*4+2];
   GUID ag[2];
   ag[0] = *pgCode;
   ag[1] = *pgSub;
   MMLBinaryToString ((PBYTE)&ag[0], sizeof(GUID)*2, szTemp);
   return (gaTreeTextureList[dwRenderShard].Find (szTemp) ? TRUE : FALSE);
}

/******************************************************************************
TextureAddTextureColorList - Add a new texture/color to the list because just
modified it.

inputs
   PCObjectSurface      pos - surface just modified
*/
void TextureAddTextureColorList (DWORD dwRenderShard, PCObjectSurface pos)
{
   WCHAR szTemp[sizeof(GUID)*4+2];
   DWORD i = 1;
   if (pos->m_fUseTextureMap) {
      GUID ag[2];
      ag[0] = pos->m_gTextureCode;
      ag[1] = pos->m_gTextureSub;
      MMLBinaryToString ((PBYTE)&ag[0], sizeof(GUID)*2, szTemp);

      if (!gaTreeTextureList[dwRenderShard].Find(szTemp))
         gaTreeTextureList[dwRenderShard].Add (szTemp, &i, sizeof(i));
   }
   else {
      swprintf (szTemp, L"%d", (int) pos->m_cColor);
      if (!gaTreeColorList[dwRenderShard].Find (szTemp))
         gaTreeColorList[dwRenderShard].Add (szTemp, &i, sizeof(i));
   }
}

/******************************************************************************
TextureRebuildTextureColorList - ASP maintains a list of what textures and colors
are used for the object. This rebuilds it by asking every object for entries.

returns
   none
*/
void TextureRebuildTextureColorList (DWORD dwRenderShard)
{
   // make the list
   CListFixed lText, lColor;
   lText.Init (sizeof(GUID)*2);
   lColor.Init (sizeof(COLORREF));

   gaTreeTextureList[dwRenderShard].Clear();
   gaTreeColorList[dwRenderShard].Clear();
   WCHAR szTemp[sizeof(GUID)*4+2];

   // all the objects
   DWORD i, j;
   PCWorldSocket pWorld;
   pWorld = WorldGet(dwRenderShard, NULL);
   if (pWorld) for (i = 0; i < pWorld->ObjectNum(); i++) {
      PCObjectSocket pos = pWorld->ObjectGet(i);
      if (!pos)
         continue;

      // ask the objects what textures they use
      lText.Clear();
      pos->TextureQuery (&lText);

      // NOTE: Not worrying about sub-textures here since its only for display

      // loop through all the elements returned
      for (j = 0; j < lText.Num(); j++) {
         GUID *pg = (GUID*) lText.Get(j);

         // add to the tree
         MMLBinaryToString ((PBYTE)pg, sizeof(GUID)*2, szTemp);

         if (!gaTreeTextureList[dwRenderShard].Find(szTemp))
            gaTreeTextureList[dwRenderShard].Add (szTemp, &i, sizeof(i));
      }

      // and colors
      lColor.Clear();
      pos->ColorQuery (&lColor);
      for (j = 0; j < lColor.Num(); j++) {
         swprintf (szTemp, L"%d", (int) (*((COLORREF*) lColor.Get(j))));
         if (!gaTreeColorList[dwRenderShard].Find (szTemp))
            gaTreeColorList[dwRenderShard].Add (szTemp, &i, sizeof(i));
      }
   }

}



/***********************************************************************************
CTextCreatorSocket::TextureQuery - Adds this object's textures (and it's sub-textures)
if it's not already on the list.

inputs
   PCListFixed       plText - List of 2-GUIDs (major & minor) for the textures
                     that are already known. If not, the texture is added, and sub-textures
                     are also added.
   PCBTree           pTree - Tree of strings (Using MMLBinaryToString of 2 guids). If
                     the texture is already on here, then don't add. Elese, texture
                     and sub-textures are also added.
   GUID              *pagThis - Pointer to an array of 2 guids. pagThis[0] = code, pagThis[1] = sub
returns
   BOOL - TRUE if success
*/
BOOL CTextCreatorSocket::TextureQuery (PCListFixed plText, PCBTree pTree, GUID *pagThis)
{
   WCHAR szTemp[sizeof(GUID)*4+2];
   MMLBinaryToString ((PBYTE)pagThis, sizeof(GUID)*2, szTemp);
   if (pTree->Find (szTemp))
      return TRUE;

   
   // add itself
   plText->Add (pagThis);
   pTree->Add (szTemp, NULL, 0);

   return TRUE;
}



/***********************************************************************************
CTextCreatorSocket::SubTextureNoRecurse - Adds this object's textures (and it's sub-textures)
if it's not already on the list.

inputs
   PCListFixed       plText - List of 2-GUIDs (major & minor) for the textures
                     that are already known. If not, the texture is TEMPORARILY added, and sub-textures
                     are also TEMPORARILY added.
   GUID              *pagThis - Pointer to an array of 2 guids. pagThis[0] = code, pagThis[1] = sub
returns
   BOOL - TRUE if success. FALSE if they recurse
*/
BOOL CTextCreatorSocket::SubTextureNoRecurse (PCListFixed plText, GUID *pagThis)
{
   GUID *pag = (GUID*)plText->Get(0);
   DWORD i;
   for (i = 0; i < plText->Num(); i++, pag += 2)
      if (!memcmp (pag, pagThis, sizeof(GUID)*2))
         return FALSE;  // found itself

   // note: by default, textures dont recurse, so always return TRUE
   return TRUE;
}




/**************************************************************************************
CTextCreatorSocket::FillImageWithTexture - Given a texture creator and PCImage, fills the image with teh
texture

inputs 
   PCImage                 pImage - Filled in
   BOOL              fEncourageSphere - If TRUE then encourage it to draw a sphere
   BOOL              fStretchToFit - If TRUE then stretch to fit
returns
   BOOL - TRUE if success
*/
BOOL CTextCreatorSocket::FillImageWithTexture (PCImage pImage, BOOL fEncourageSphere, BOOL fStretchToFit)
{
   TEXTINFO ti;
   CMaterial Mat;
   CImage Image, Image2;
   if (!Render(&Image, &Image2, &Mat, &ti))
      return FALSE;

   // allocate the memory needed tow rite it out - basically a lot
   CMem mem;
   DWORD dwNeeded;
   DWORD dwMult;
   dwMult = 3; // RGB of color
   if (ti.dwMap & 0x03)
      dwMult += 3;   // if have spec/bump also need to account for combined map
   if (ti.dwMap & 0x01)
      dwMult += 2;   // specularity...
   if (ti.dwMap & 0x02)
      dwMult += 2;   // bump...
   if (ti.dwMap & 0x04)
      dwMult += 1;   // transparency... 1 byte
   if (ti.dwMap & 0x08)
      dwMult += 3;  // glow - wrap as bytes

   dwNeeded = sizeof(ALGOTEXTCACHE) + Image.Width() * Image.Height() * dwMult + 256;
      // BUGFIX - Was *17, changed to *dwMult
   if (!mem.Required (dwNeeded))
      return FALSE;  // error

   // fill in the maps
   PBYTE pbColorMap, pCur;
   DWORD dwColorMapSize;
   dwColorMapSize = (Image.Width() * Image.Height() * 3 + 3) & ~0x03;   // DWORD align
   pbColorMap = (PBYTE) mem.p;
   pCur = pbColorMap + dwColorMapSize;

   // color-only map
   PBYTE pbColorOnly;
   pbColorOnly = NULL;
   if (ti.dwMap & 0x03) {  // both specularity and bump
      pbColorOnly = pCur;
      pCur += dwColorMapSize; // since same size as above
   }

   // specularity map?
   PBYTE pbSpecMap;
   pbSpecMap = NULL;
   if (ti.dwMap & 0x01) {
      pbSpecMap = pCur;
      pCur += ((Image.Width() * Image.Height() * 2 + 3) & ~0x03); // DWORD align this;
   }

   // bump map?
   PSHORT pBumpMap;
   pBumpMap = NULL;
   if (ti.dwMap & 0x02) {
      pBumpMap = (PSHORT) pCur;
      pCur += (Image.Width() * Image.Height() * sizeof(short));
   }

   // transparency map?
   PBYTE pTrans;
   pTrans = NULL;
   if (ti.dwMap & 0x04) {
      pTrans = pCur;
      pCur += ((Image.Width() * Image.Height() + 3) & ~0x03); // DWORD align this
   }

   // glow map
   PBYTE pGlow;
   pGlow = NULL;
   if (ti.dwMap & 0x08) {
      pGlow = pCur;
      pCur += dwColorMapSize; // since same size as above
   }

   // transfer over
   PIMAGEPIXEL pip;
   DWORD i;
   if (pbColorOnly) {
      PBYTE pb;
      pb = pbColorOnly;
      pip = Image.Pixel(0,0);
      for (i = 0; i < Image.Width() * Image.Height(); i++, pip++) {
         *(pb++) = Image.UnGamma (pip->wRed);
         *(pb++) = Image.UnGamma (pip->wGreen);
         *(pb++) = Image.UnGamma (pip->wBlue);
      }
   }
   if (pbSpecMap) {
      PBYTE pb;
      pb = pbSpecMap;
      pip = Image.Pixel(0,0);
      for (i = 0; i < Image.Width() * Image.Height(); i++, pip++) {
         *(pb++) = HIBYTE(LOWORD(pip->dwID));
         *(pb++) = UnGamma(HIWORD(pip->dwID));
      }
   }
   if (pBumpMap) {
      PSHORT ps;
      //int   iZ;
      ps = pBumpMap;
      pip = Image.Pixel(0,0);
      for (i = 0; i < Image.Width() * Image.Height(); i++, pip++) {
         // BUGFIX - Changed pixel Z to float
         fp f = pip->fZ * 256;

         f = min(f, 0x7fff);
         f = max(f, -0x7fff);
         //iZ = pip->iZ / 256;  // since less precsion
         //iZ = min(iZ, 0x7fff);
         //iZ = max(iZ, -0x7fff);
         *(ps++) = (short)f;//iZ;
      }
   }
   if (pTrans) {
      PBYTE pb;
      pb = pTrans;
      pip = Image.Pixel(0,0);
      for (i = 0; i < Image.Width() * Image.Height(); i++, pip++)
         *(pb++) = LOBYTE(pip->dwIDPart);
   }
   if (pGlow) {   // if glow then Image2 must be valid
      PBYTE pb;
      pb = pGlow;
      pip = Image2.Pixel(0,0);
      for (i = 0; i < Image2.Width() * Image2.Height(); i++, pip++) {
         *(pb++) = Image2.UnGamma (pip->wRed);
         *(pb++) = Image2.UnGamma (pip->wGreen);
         *(pb++) = Image2.UnGamma (pip->wBlue);
      }
   }

   // now apply bump map
   Image.TGBumpMapApply ();
   if (pbColorMap) {
      PBYTE pb;
      pb = pbColorMap;
      pip = Image.Pixel(0,0);
      for (i = 0; i < Image.Width() * Image.Height(); i++, pip++) {
         *(pb++) = Image.UnGamma (pip->wRed);
         *(pb++) = Image.UnGamma (pip->wGreen);
         *(pb++) = Image.UnGamma (pip->wBlue);
      }
   }


   CMem memGlow;
   DWORD dwX, dwY;
   dwX = Image.Width();
   dwY = Image.Height();
   if (pGlow && memGlow.Required (dwX * dwY * sizeof(float) * 3)) {
      float *paf = (float*) memGlow.p;
      BYTE *pb = pGlow;
      DWORD i;
      for (i = 0; i < dwX * dwY * 3; i++, paf++, pb++)
         paf[0] = (fp) Gamma (pb[0]) * ti.fGlowScale;
   }

   // create the textures
   CTextureMap Map;
   GUID gCode, gSub;
   memset (&gCode, 0, sizeof(gCode));
   memset (&gSub, 0, sizeof(gSub));
   Map.Init (&gCode, &gSub, Image.Width(), Image.Height(), &Mat, &ti,
      pbColorMap, pbColorOnly, pbSpecMap, pBumpMap, pTrans, pGlow ? (float*) memGlow.p : NULL);

   // get the texture's default width and height
   fp fx, fy;
   Map.DefScaleGet (&fx, &fy);
   fp afTextureMatrix[2][2];
   CMatrix mTextureMatrix;
   mTextureMatrix.Identity();
   memset (afTextureMatrix, 0, sizeof(afTextureMatrix));
   afTextureMatrix[0][0] = 1.0 / fx;
   afTextureMatrix[1][1] = 1.0 / fy;

   Map.FillImage (pImage, fEncourageSphere, fStretchToFit, afTextureMatrix, &mTextureMatrix, NULL);

   return TRUE;
}



#if 0 // dead code
/**************************************************************************************
RenderSphereWithPlane - Renders a sphere with a plane onto the image. Uses ray tracing

inputs
   PCImage           pImage - Image to use. The width and height are already set
   fp                fRadius - Radius of the sphere
   PCTextureMapSocket pTexture - Texture to use for the sphere. NOTE: For this
                     to work the texture's guids
   fp                afTextureMatrix[2][2] - For how to rotate the HV texture
   PCMatrix          pmTextureMatrix - For how to rotate the volumetric texture
   PTEXTUREMODS      pTextureMods - Texture mods to use
returns
   BOOL - TRUE if success
*/
BOOL RenderSphereWithPlane (PCImage pImage, fp fRadius, PCTextureMapSocket pTexture,
                            fp afTextureMatrix[2][2], PCMatrix pmTextureMatrix,
                            PTEXTUREMODS pTextureMods)
{
   // create the texture
   GUID gCode, gSubOrig, gSubNew;
   GUIDGen (&gSubNew);
   pTexture->GUIDsGet (&gCode, &gSubOrig);
   pTexture->GUIDsSet (&gCode, &gSubNew);
   TextureCacheAddDynamic (pTexture);


   // create the sphere and set its surface
   OSINFO oi;
   memset (&oi, 0 ,sizeof(oi));
   oi.gClassCode = CLSID_Revolution;
   PCObjectRevolution pSphere = new CObjectRevolution(0, &oi);
   if (!pSphere) {
      TextureCacheDelete (&gCode, &gSubOrig, FALSE);
      pTexture->GUIDsSet (&gCode, &gSubOrig);
      return FALSE;
   }
   pSphere->m_Rev.BackfaceCullSet (FALSE);
   CPoint pBottom, pAround, pLeft, pScale;
   pBottom.Zero();
   pBottom.p[2] = -fRadius;
   pAround.Zero();
   pAround.p[2] = 1;
   pLeft.Zero();
   pLeft.p[0] = -1;
   pScale.Zero();
   pScale.p[0] = pScale.p[1] = pScale.p[2] = fRadius * 2;
   pSphere->m_Rev.ProfileSet (PRROF_CIRCLEHEMI, 2);
   pSphere->m_Rev.RevolutionSet (RREV_CIRCLE, 2);
   pSphere->m_Rev.DirectionSet (&pBottom, &pAround, &pLeft);
   pSphere->m_Rev.BackfaceCullSet (FALSE);
   pSphere->m_Rev.ScaleSet (&pScale);
   DWORD dwSurf = pSphere->ObjectSurfaceFindIndex (0);
   pSphere->ObjectSurfaceRemove (dwSurf);
   CObjectSurface os;
   os.m_dwID = dwSurf;
   memcpy (os.m_afTextureMatrix, afTextureMatrix, sizeof(os.m_afTextureMatrix));
   os.m_mTextureMatrix.Copy (pmTextureMatrix);
   os.m_cColor = 0;  // since will use texture
   os.m_fUseTextureMap = TRUE;
   os.m_gTextureCode = gCode;
   os.m_gTextureSub = gSubOrig;
   pTexture->MaterialGet (&os.m_Material);
   os.m_szScheme[0] = 0;
   if (pTextureMods)
      memcpy (&os.m_TextureMods, pTextureMods, sizeof(os.m_TextureMods));
   pSphere->ObjectSurfaceAdd (&os);

   // create the world
   CWorld World;
   World.RenderShardSet (dwRenderShard);
   World.ObjectAdd (pSphere);

   // render
   CRenderRay rt;
   CFImage Image;
   CPoint pFrom;
   Image.Init (pImage->Width(), pImage->Height());
   pFrom.Zero();
   pFrom.p[1] = -fRadius * 4;
   rt.QuickSetRay (0, 1, 1);
   rt.BackgroundSet (RGB(0x80, 0x80, 0xff));
   rt.CImageSet (&Image);
   rt.CameraPerspWalkthrough (&pFrom);
   rt.CWorldSet (&World);
   rt.
   note - not doing this because need to add light and other stuff.
   will be too slow


   // restore values
   World.Clear();
   TextureCacheDelete (&gCode, &gSubOrig, FALSE);
   pTexture->GUIDsSet (&gCode, &gSubOrig);

   return TRUE;
}
#endif // 0



/**************************************************************************************
SphereDrawInterRay - Intersect ray with sphere and plane. Used when drawing the
sphere and plane, to intersect a ray with a sphere (at 0,0,0) and a plane (at 0, 2r, 0).

inputs
   PCPoint        pStart - Start of the ray
   PCPoint        pDir - Direction (normalized) of ray
   fp             fRadius - Radius of sphere
   PTEXTUREPOINT  pTextSize - Width and height of texture, for stretching
   PTEXTPOINT5    pTextInter - Filled in with texture point where intersected.
                     Sphere's hv coords are mapped to meters
   PCPoint        pInter - Filled in with location where intersected.
   PCPoint        pNorm - Filled with the normal where intersected
   BOOL           fStretchToFit - If TRUE stretching texture to fit
returns
   DWORD - 0 if intersects nothing, 1 if intersects sphere, 2 if intersects plane
*/
DWORD SphereDrawInterRay (PCPoint pStart, PCPoint pDir, fp fRadius, PTEXTUREPOINT pTextSize,
                          PTEXTPOINT5 pTextInter, PCPoint pInter, PCPoint pNorm,
                          BOOL fStretchToFit)
{
   // intersect with the sphere
   CPoint pCenter;
   pCenter.Zero();
   fp fAlpha1, fAlpha2;
   DWORD dwNum = IntersectRaySpehere (pStart, pDir, &pCenter, fRadius, &fAlpha1, &fAlpha2);
   if (dwNum == 2) {
      // intersect in two points
      if (fAlpha1 < CLOSE)
         fAlpha1 = fAlpha2;   // take 2nd
      else if (fAlpha2 < CLOSE)
         fAlpha1 = fAlpha1;   // 2nd before start of ray so take that
      else
         fAlpha1 = min(fAlpha1, fAlpha2);
   }
   if (dwNum && (fAlpha1 < CLOSE))
      dwNum = 0;  // none intersected

   // try with plane
   CPoint pEnd;
   pEnd.Add (pStart, pDir);
   pCenter.p[2] = -2.01 * fRadius;  // dont use 2.0 because have round-off errors
   pNorm->Zero();
   pNorm->p[2] = 1;
   BOOL fWithPlane = IntersectLinePlane (pStart, &pEnd, &pCenter, pNorm, pInter);
   if (fWithPlane) {
      // calculate alpha
      pEnd.Subtract (pInter, pStart);
      fAlpha2 = pEnd.DotProd (pDir);
      if (fAlpha2 < CLOSE)
         fWithPlane = FALSE;

      if ((fabs(pInter->p[0]) > 20 * fRadius) || (fabs(pInter->p[1]) > 20 * fRadius))
         fWithPlane = FALSE;
   }

   // which one does it intersect with?
   DWORD dwRet;
   if (dwNum && fWithPlane)
      dwRet = (fAlpha1 < fAlpha2) ? 1 : 2;
   else if (dwNum)
      dwRet = 1;
   else if (fWithPlane)
      dwRet = 2;
   else
      dwRet = 0;

   // fill in the intersection surface info
   if (dwRet == 1) { // ray
      // note where intersected
      pInter->Copy (pDir);
      pInter->Scale (fAlpha1);
      pInter->Add (pStart);

      // normal
      pNorm->Copy (pInter);
      pNorm->Normalize ();
      
      // texture
      fp fLen;
      pTextInter->hv[0] = atan2(pNorm->p[0], -pNorm->p[1]);
      fLen = sqrt(pNorm->p[0] * pNorm->p[0] + pNorm->p[1] * pNorm->p[1]);
      pTextInter->hv[1] = -acos(min(fLen,1));
      if (pNorm->p[2] < 0)
         pTextInter->hv[1] *= -1;
      if (fStretchToFit) {
         pTextInter->hv[0] = (pTextInter->hv[0] + PI) / (PI * 2.0) * pTextSize->h;
         pTextInter->hv[1] = (pTextInter->hv[1] - PI/2.0) / PI * pTextSize->v;
      }
      else {
         pTextInter->hv[0] *= fRadius;
         pTextInter->hv[1] *= fRadius;
      }

      pTextInter->xyz[0] = pInter->p[0];
      pTextInter->xyz[1] = pInter->p[1];
      pTextInter->xyz[2] = pInter->p[2];
   }
   else if (dwRet == 2) { // plane
      // note where intersected
      // pInter is still valid from above
      //pInter->Copy (pDir);
      //pInter->Scale (fAlpha2);
      //pInter->Add (pStart);

      // normal
      pNorm->Zero();
      pNorm->p[2] = 1;
      
      // texture
      pTextInter->hv[0] = pInter->p[0];
      pTextInter->hv[1] = -pInter->p[1];
      pTextInter->xyz[0] = pInter->p[0];
      pTextInter->xyz[1] = pInter->p[1];
      pTextInter->xyz[2] = pInter->p[2];
   }

   return dwRet;
}


// BUGFIX - Turn optimizations off so that doesn't draw blackness in sphere
// #pragma optimize ("", off)

/**************************************************************************************
SphereDrawRayColor - This sends a ray starting at pStart and in the given direction.
It fills in the color.

inputs
   PCPoint        pStart - Start of the ray
   PCPoint        pDir - Direction (normalized) of ray
   DWORD          dwBounceLeft - Number of bounces left
   fp             fRadius - Radius of sphere
   PTEXTUREPOINT  pTextSize - Width and height of texture, for stretch purposes
   fp             fBeamWidth - Beam width per meter of ray length
   fp             fDistSoFar - Number of meters ray has gone so far
   PCTextureMapSocket pTexture - Texture of the sphere
   fp                afTextureMatrix[2][2] - For how to rotate the HV texture
   PCMatrix          pmTextureMatrix - For how to rotate the volumetric texture
   PTEXTUREMODS      pTextureMods - Texture mods to use
   float          *pafColor - Array of 3 colors that are filled in
   BOOL           fStretchToFit - If TRUE then stretch texture to fit
returns
   none
*/
void SphereDrawRayColor (PCPoint pStart, PCPoint pDir, DWORD dwBounceLeft,
                         fp fRadius, PTEXTUREPOINT pTextSize, fp fBeamWidth, fp fDistSoFar, PCTextureMapSocket pTexture,
                         fp afTextureMatrix[2][2], PCMatrix pmTextureMatrix,PCMaterial pMaterial,
                         float *pafColor, BOOL fStretchToFit)
{
   // if no bounces left then none
   pafColor[0] = pafColor[1] = pafColor[2] = 0;
   if (!dwBounceLeft)
      return;

   // see what intersects
   DWORD dwInter;
   TEXTPOINT5 tpInter;
   CPoint pInter, pNorm;
   dwInter = SphereDrawInterRay (pStart, pDir, fRadius, pTextSize, &tpInter, &pInter, &pNorm, fStretchToFit);

   // if intersects nothing then sky
   if (!dwInter) {
      pafColor[0] = pafColor[1] = pafColor[2] = 0xffff;
      return;
   }

   // if intersects surface then easy
   if (dwInter == 2) {
      fp fBand1 = tpInter.hv[0] / fRadius;
      fp fBand2 = tpInter.hv[1] / fRadius;
      fBand1 = myfmod(fBand1, 2) - 1;
      fBand2 = myfmod(fBand2, 2) - 1;
      fBand1 *= fBand2;
      if (fBand1 >= 0) {
         pafColor[0] = pafColor[1] = 0x8000;
         pafColor[2] = 0xffff;
      }
      else
         pafColor[0] = pafColor[1] = pafColor[2] = 0x2000;
      return;
   }


   // adjust the texture point to the sphere
   TEXTPOINT5 tpOrig = tpInter;
   CPoint p;
   DWORD i,j;
   TextureMatrixMultiply2D (afTextureMatrix, (PTEXTUREPOINT) (&tpInter.hv[0]));
   for (i = 0; i < 3; i++)
      p.p[i] = tpInter.xyz[i];
   p.p[3] = 1;
   p.MultiplyLeft (pmTextureMatrix);
   for (i = 0; i < 3; i++)
      tpInter.xyz[i] = p.p[i];

   // increase the distance travelled so far and calculate current beam width
   CPoint pDist;
   fp fCurWidth;
   pDist.Subtract (&pInter, pStart);
   fDistSoFar += pDist.Length();
   fCurWidth = fDistSoFar * fBeamWidth;

   // figure out ray perpendicular to this
   CPoint pUp, pRight;
   pRight.Zero();
   pRight.p[0] = 1;
   pUp.CrossProd (&pRight, pDir);
   if (pUp.Length() < .01) {
      // choose other direction
      pRight.Zero();
      pRight.p[1] = 1;
      pUp.CrossProd (&pRight, pDir);
   }
   pUp.Normalize();
   pRight.CrossProd (pDir, &pUp);
   pRight.Normalize();

   // send 4 rays, slighty to right,left, etc.
   DWORD adwInter[4];   //[0] = right, [1]=left, [2]=up, [3]=down
   TEXTPOINT5 atpInter[4]; // texturepoint intersections
   CPoint apInter[4];   // where intersected
   TEXTPOINT5 tpZero;
   memset (&tpZero, 0, sizeof(tpZero));
   BOOL fBump = FALSE;
   for (i = 0; i < 4; i++) {
      CPoint pTestDir, pN2;
      pTestDir.Copy ((i < 2) ? &pRight : &pUp);
      if (i%2)
         pTestDir.Scale (-1);
      pTestDir.Scale (fBeamWidth);  // only worry about antialias for first bounce
      pTestDir.Add (pDir);
      pTestDir.Normalize();

      // intersect it
      adwInter[i] = SphereDrawInterRay (pStart, &pTestDir, fRadius, pTextSize, &atpInter[i], &apInter[i], &pN2, fStretchToFit);
      if (adwInter[i] != 1)
         continue;

      // adjust the texture point to the sphere
      CPoint p;
      TextureMatrixMultiply2D (afTextureMatrix, (PTEXTUREPOINT) (&atpInter[i].hv[0]));
      for (j = 0; j < 3; j++)
         p.p[j] = atpInter[i].xyz[j];
      p.p[3] = 1;
      p.MultiplyLeft (pmTextureMatrix);
      for (j = 0; j < 3; j++)
         atpInter[i].xyz[j] = p.p[j];

      // find difference in where intersect
      for (j = 0; j < 2; j++)
         atpInter[i].hv[j] -= tpInter.hv[j];
      for (j = 0; j < 3; j++)
         atpInter[i].xyz[j] -= tpInter.xyz[j];
         
      // find out how much stick out of surface
      TEXTUREPOINT pSlope;
      if (!pTexture->PixelBump (0, &tpInter, &atpInter[i], &tpZero, &pSlope))
         continue;

      // if get here, sticks out of surface, so increase intersection location
      CPoint pSum;
      pSum.Copy (&pNorm);
      pSum.Scale (pSlope.h);
      apInter[i].Add (&pSum);
      fBump = TRUE;
   }

   // figure out the new normal...
   CPoint pNormText;
   pNormText.Copy (&pNorm);
   if (fBump) {
      CPoint pRight, pLeft, pUp, pDown;
      pRight.Copy (&pInter);
      pLeft.Copy (&pInter);
      pUp.Copy (&pInter);
      pDown.Copy (&pInter);
      if (adwInter[0] == 1)
         pRight.Copy (&apInter[0]);
      if (adwInter[1] == 1)
         pLeft.Copy (&apInter[1]);
      if (adwInter[2] == 1)
         pUp.Copy (&apInter[2]);
      if (adwInter[3] == 1)
         pDown.Copy (&apInter[3]);

      // find differnce
      pUp.Subtract (&pDown);
      pRight.Subtract (&pLeft);
      if ((pUp.Length() > CLOSE) && (pRight.Length() > CLOSE)) {
         pUp.Normalize();
         pRight.Normalize();
         pNormText.CrossProd (&pRight, &pUp);
         if (pNormText.Length() < .1)
            pNormText.Copy (&pNorm);
         else
            pNormText.Normalize();
      }
   }

   // vector to the light
   CPoint pLightVector;
   pLightVector.Zero();
   pLightVector.p[0] = pLightVector.p[1] = -1;
   pLightVector.p[2] = 1;
   pLightVector.Normalize();

   // eye normal
   CPoint pEyeNorm;
   pEyeNorm.Copy (pDir);
   pEyeNorm.Scale (-1);

   // figure out tp max by looking at beam width in 2 directions
   TEXTPOINT5 tpMax;
   memset (&tpMax, 0 ,sizeof(tpMax));
   for (j = 0; j < 4; j++) {
      // only care about ones that reintersect sphere
      if (adwInter[j] != 1)
         continue;

      for (i = 0; i < 2; i++)
         tpMax.hv[i] = max(tpMax.hv[i],fabs(atpInter[j].hv[i]));
      for (i = 0; i < 3; i++)
         tpMax.xyz[i] = max(tpMax.xyz[i],fabs(atpInter[j].xyz[i]));
   }

   // get the texture at the intersection, and automatically include the glow
   WORD awColor[3];
   CMaterial Mat;
   if (pMaterial)
      memcpy (&Mat, pMaterial, sizeof(Mat));
   else
      pTexture->MaterialGet (0, &Mat);
   pTexture->FillPixel (0, TMFP_ALL, awColor, &tpInter, &tpMax, &Mat, pafColor, FALSE);
      // Dont bother with high quality
      // BUGFIX - Was just using TMFP_SPECULARITY

   // if looking not looking at the side the normal is on, then swap over the normals
   fp fFlip;
   fFlip = pEyeNorm.DotProd (&pNorm);
   CPoint pNFlipText;
   pNFlipText.Copy (&pNormText);
   if (fFlip < 0)
      pNFlipText.Scale (-1);  // yes, looking from back side

   // if light is behind user then get rid of
   fp fNDotL, fLight;
   BOOL fUseTranslucent;
   fNDotL = pLightVector.DotProd (&pNFlipText);
   if (Mat.m_wTranslucent && (fNDotL < 0)) {
      fNDotL *= -1;
      fUseTranslucent = TRUE;
   }
   else
      fUseTranslucent = FALSE;

   if ((fNDotL > 0) /*&& !Mat.m_fSelfIllum*/) {
      // BUGFIX - amount of color depends on transparency too
      fp fTrans = 1;
      if (Mat.m_wTransparency) {
         fTrans = (fp)Mat.m_wTransparency / (fp)0xffff;

         // BUGFIX - Transparency depends on angle of viewer
         fp fNDotEye, fTransAngle;
         fNDotEye = fabs(pEyeNorm.DotProd (&pNFlipText));
         fTransAngle = (fp) Mat.m_wTransAngle / (fp)0xffff;
         fTrans *= (fNDotEye + (1.0 - fNDotEye) * (1.0 - fTransAngle));

         fTrans = 1.0 - fTrans;
      }

      // BUGFIX - If reflective then decrease the color by that much too
      if (Mat.m_wReflectAmount)
         fTrans *= (fp)(0xffff - Mat.m_wReflectAmount) / (fp)0xffff;

      // if translucent side then might be darker
      if (fUseTranslucent)
         fTrans *= (fp) Mat.m_wTranslucent / (fp)0xffff;

      // diffuse component
      fLight = fNDotL * fTrans;
      for (i = 0; i < 3; i++)
         pafColor[i] += fLight * (fp)awColor[i];
   }

   // specular component
   CPoint pH;
   pH.Add (&pEyeNorm, &pLightVector);
   pH.Normalize();
   fp fNDotH, fVDotH;
   fNDotH = pNFlipText.DotProd (&pH);
   //if (Mat.m_fTranslucent)  BUGFIX - Take out because dont want specularity on non-translucent side
   //   fNDotH = fabs(fNDotH);
   if (!fUseTranslucent && (fNDotH > 0) && Mat.m_wSpecReflect) {
      fVDotH = pEyeNorm.DotProd (&pH);
      fVDotH = max(0,fVDotH);
      fNDotH = pow (fNDotH, (fp) ((fp)Mat.m_wSpecExponent / 100.0) );
      fLight = fNDotH * (fp) Mat.m_wSpecReflect / (fp)0xffff;


      fp fPureLight, fMixed, fMax;
      if (Mat.m_wSpecPlastic > 0x8000)
         fVDotH = pow(fVDotH, (fp)(1.0 + (fp)(Mat.m_wSpecPlastic - 0x8000) / (fp)0x1000));
      else if (Mat.m_wSpecPlastic < 0x8000)
         fVDotH = pow(fVDotH, (fp)(1.0 / (1.0 + (fp)(0x8000 - Mat.m_wSpecPlastic) / (fp)0x1000)));
      fPureLight = fLight * (1.0 - fVDotH);
      fMixed = fLight * fVDotH;

      // BUGFIX - For the mixing component, offset by the maximum brightness component of
      // the object so the specularily is relatively as bright.
      //fMax = (pawColor[0] + pawColor[1] + pawColor[2]) / (fp) 0xffff;   // NOTE: Secifically not using /3.0
      fMax = (fp)max(max(awColor[0], awColor[1]),awColor[2]) / (fp) 0xffff;
      if (fMax > EPSILON)
         fMixed /= fMax;

      for (i = 0; i < 3; i++)
         pafColor[i] += fPureLight * (fp)0xffff + fMixed * (fp)awColor[i];
   }

   // determine if reflection
   if (Mat.m_wReflectAmount) {
      CPoint pN;
      pN.Copy (&pNormText);
      if (pN.DotProd (pDir) > 0)
         pN.Scale (-1); // mirror on the opposite side;

      // R = I - 2(N.I)N
      CPoint pReflectDir;
      pReflectDir.Copy (&pN);
      pReflectDir.Scale (-pN.DotProd (pDir) * 2.0);
      pReflectDir.Add (pDir);
      pReflectDir.Normalize (); // just in case

      // amount based on angle of incidence
      fp fAngle, f;
      fAngle = 1.0 - fabs(pN.DotProd (pDir));
      f = ((fp)Mat.m_wReflectAngle / (fp)0xffff);
      fAngle = f * fAngle + (1.0-f) * 1.0;   // if glass reflects most when dotprod is 0

      // Need to normalize spec-plastic surface color
      WORD wMaxPlastic;
      fp fPlastic;
      wMaxPlastic = max(max(awColor[0], awColor[1]), awColor[2]);
      if (wMaxPlastic)
         fPlastic = (fp)0xffff / (fp)wMaxPlastic;   // so specularity always max brightness
      else
         fPlastic = 1;

      // color based on surface color and plasticness and angle
      f = (fp)Mat.m_wSpecPlastic / (fp)0xffff;  // = 1 if plastic
      float afReflectColor[3], afReflect[3];
      for (i = 0; i < 3; i++) {
         afReflectColor[i] = (1.0 - f) * (fp) awColor[i] * fPlastic / (fp)0xffff + f * 1.0;
         afReflectColor[i] *= fAngle;

         // BUGFIX - Hadn't included reflect amount in calculations
         afReflectColor[i] *= (fp)Mat.m_wReflectAmount / (fp)0xffff;
      }

      // loop
      SphereDrawRayColor (&pInter, &pReflectDir, dwBounceLeft-1, fRadius, pTextSize, fBeamWidth, fDistSoFar,
         pTexture, afTextureMatrix, pmTextureMatrix, pMaterial, afReflect, fStretchToFit);

      for (i = 0; i < 3; i++)
         pafColor[i] += afReflectColor[i] * afReflect[i];
   }

   if (Mat.m_wTransparency) {
      // calculate info
      BOOL fFlip;
      
      CPoint pN;
      pN.Copy (&pNormText);
      fp fCi = -pDir->DotProd (&pN);
      if (fCi < 0) {
         // coming from the other side so flip
         fFlip = TRUE;
         fCi *= -1;
         pN.Scale (-1);
      }
      else
         fFlip = FALSE;
      
      // angle of incidence
      fp fNit;
      fNit = (fp) Mat.m_wIndexOfRefract / 100.0;
      if (!fNit)
         fNit = .01;  // need something
      if (!fFlip)
         fNit = 1.0 / fNit; // so if glass (1.44), can never get total internal reflection

      // sqrt value
      fp fRoot;
      fRoot = 1 + fNit * fNit * (fCi * fCi - 1);
      float afRefractColor[3];
      memset (afRefractColor, 0, sizeof(afRefractColor));
      CPoint pRefractDir;
      if (fRoot < 0) {
         // total internal reflection
         afRefractColor[0] = afRefractColor[1] = afRefractColor[2] = 1;

         // R = I - 2(N.I)N
         pRefractDir.Copy (&pN);
         pRefractDir.Scale (-pN.DotProd (pDir) * 2.0);
         pRefractDir.Add (pDir);
         pRefractDir.Normalize (); // just in case
      }
      else {
         // refraction
         fRoot = fNit * fCi - sqrt(fRoot);
         pRefractDir.Copy (&pN);
         pRefractDir.Scale (fRoot);
         pN.Copy (pDir);
         pN.Scale (fNit);
         pRefractDir.Add (&pN);
         pRefractDir.Normalize();  // just in case

         // color is affected by transparency
         fp fTrans;
         fTrans = (fp)Mat.m_wTransparency / (fp)0xffff;

         if (fTrans) {
            // BUGFIX - Transparency depends on angle of viewer
            fp fNDotEye, fTransAngle;
            fNDotEye = fabs(pDir->DotProd (&pNormText));
            fTransAngle = (fp) Mat.m_wTransAngle / (fp)0xffff;
            fTrans *= (fNDotEye + (1.0 - fNDotEye) * (1.0 - fTransAngle));
         }

         for (i = 0; i < 3; i++)
            afRefractColor[i] = fTrans * (fTrans + (1.0 - fTrans) * (1.0 - fRoot) * (fp)awColor[i] / (fp)0xffff);
            // afRefractColor[i] = fTrans * (1.0 + (1.0 - fTrans) * (1.0 - fRoot) * (fp)awColor[i] / (fp)0xffff);
            //afRefractColor[i] = fTrans + (1.0 - fTrans) * (1.0 - fRoot) * (fp)awColor[i] / (fp)0xffff;
            // BUGFIX - Changed fTrans to 1.0 - fTrans since completely transparent was
            // actually making brighter
            // BUGFIX - Changed from "ftrans + (1.0 - ftrans*x)" to what it is now
         // BUGFIX - Do this equation so if it's 100% transparent all of the color goes through,
         // while if 0 transparency then none. Need to do this for leaves

         // loop
         float afReflect[3];
         SphereDrawRayColor (&pInter, &pRefractDir, dwBounceLeft-1, fRadius, pTextSize, fBeamWidth, fDistSoFar,
            pTexture, afTextureMatrix, pmTextureMatrix, pMaterial, afReflect, fStretchToFit);

         for (i = 0; i < 3; i++)
            pafColor[i] += afRefractColor[i] * afReflect[i];
      }
   }

   return;
}
// #pragma optimize ("", on)

/**************************************************************************************
SphereDraw - Draws a sphere onto the image.

inputs
   PCImage           pImage - Image to use. Width and height are already set
   fp                fRadius - Radius of sphere
   PCTextureMapSocket pTexture - Texture of the sphere
   fp                afTextureMatrix[2][2] - For how to rotate the HV texture
   PCMatrix          pmTextureMatrix - For how to rotate the volumetric texture
   PCMaterial        pMaterial - Material to use - overrides default material for texture
                     Can be NULL
   BOOL           fStetchToFit - If TRUE the stretch texture to fit
returns
   none
*/

void SphereDraw (PCImage pImage,
                  fp fRadius, PCTextureMapSocket pTexture,
                  fp afTextureMatrix[2][2], PCMatrix pmTextureMatrix,
                  PCMaterial pMaterial, BOOL fStretchToFit)
{
   // origin
   CPoint pStart;
   pStart.Zero();
   pStart.p[1] = -fRadius * 6;

   // max amount..
   DWORD dwMax = max(pImage->Width(), pImage->Height());

   // determine the width of a beam...
   fp fBeamWidth = (2.0 / (fp)dwMax) / (-pStart.p[1] / fRadius);

   TEXTUREPOINT tpTextSize;
   fp fDSGH, fDSGV;
   pTexture->DefScaleGet (&fDSGH, &fDSGV);
   tpTextSize.h = fDSGH;
   tpTextSize.v = fDSGV;
   tpTextSize.h = max(tpTextSize.h, 0.0001);
   tpTextSize.v = max(tpTextSize.v, 0.0001);

   // loop
   DWORD x, y, i;
   float afColor[3];
   for (x = 0; x < pImage->Width(); x++) for (y = 0; y < pImage->Height(); y++) {   // BUGFIX - CHanged pImage->Width() to pImage->Height()
      CPoint pTo;
      pTo.Zero();
      pTo.p[0] = ((fp)x * 2.0 - (fp)pImage->Width()) / (fp)dwMax * fRadius;
      pTo.p[1] = -fRadius;
      pTo.p[2] = -((fp)y * 2.0 - (fp)pImage->Height()) / (fp)dwMax * fRadius;
      pTo.Subtract (&pStart);
      pTo.Normalize();

      SphereDrawRayColor (&pStart, &pTo, 4, fRadius, &tpTextSize, fBeamWidth, 0,
         pTexture, afTextureMatrix,
         pmTextureMatrix, pMaterial, afColor, fStretchToFit);

      // write out
      PIMAGEPIXEL pip = pImage->Pixel (x, y);
      for (i = 0; i < 3; i++) {
         afColor[i] = max(afColor[i], 0);
         afColor[i] = min(afColor[i], 0xffff);
         (&pip->wRed)[i] = (WORD)afColor[i];
      } // i
   } // x,y
}
   

// FUTURERELEASE - Two more patterns for crooked brick pavers - like at zoo.

// FUTURERELEASE - Leaves as textures

// FUTURERELEASE - Need a UI setting for "ultra-detail" that causes the all automatically
// generated textures to have fp the pattern (in meters) and twice the resolution
// in meters/pixel. Of course, need to clear cache before calling.

// FUTURERELEASE - If create a wall, and then pull up the UI to change the color, should
// default to the color-section for walls, as opposed to roofing, etc.

// FUTURERELEASE - Imported .jpg image of moon that dan sent me and the jpeg routines
// refused to load it. Saved it using microsoft paint package and ok


// BUGBUG - Need a way to get rid of unused schemes in house files and objects


// BUGBUG - The cycad leaves are too transparent when rendered in ray tracing. THis
// could be because they're so thin

// BUGBUG - Water reflectiveness seems wrong when look at it in sphere mode

// BUGBUG - In general, too much light generated by something like glass

// BUGBUG - when preview texture in the "texture library" window (for selecting
// which texture to edit, it doesn't set the right default material, or so it seems
// since this looks different than the preview in the page where actually
// editing texture
