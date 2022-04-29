/************************************************************************
CObjectSkydome.cpp - Draws a Skydome.

begun 7/2/03 by Mike Rozak
Copyright 2003 Mike Rozak. All rights reserved
*/

#include <windows.h>
#include "escarpment.h"
#include "..\M3D.h"
#include "resource.h"

// MTSKYDOME - Multithreaded skydome draw
typedef struct {
   DWORD       dwStart;    // start
   DWORD       dwStop;     // stop location
   DWORD       dwPass;     // different passes

   // pass 0, fill skydome
   PSKYPIXEL   ps;
   fp          fRadHorizon;
   CPoint      pGround;
   CPoint      pMid;
   CPoint      pZenith;

   // pass 1, cloud render
   // PSKYPIXEL ps;     // also used
   DWORD       dwXMin;
   DWORD       dwXMax;
   float       afAmbient[3];
   DWORD       dwNumSun;
   PSKYDOMELIGHT pSunList;
   COLORREF    cColor;
   fp          fDensityScale;

   // pass 2, calccloudcirrus1
   // PSKYPIXEL ps;     // also used
   DWORD       dwLayer;
   fp          fSkyCircle;

   // pass 3, calccloudcirrus 2
   // PSKYPIXEL ps;     // also used

   // pass 4, calccloudcumulus
   // PSKYPIXEL ps;     // also used
} MTSKYDOME, *PMTSKYDOME;

typedef struct {
   PWSTR       pszName;    // name of the move point
   int         iX, iY; // -1 if for left/top, 1 for right/bottom
} SPLINEMOVEP, *PSPLINEMOVEP;

static SPLINEMOVEP   gaSkydomeMove[] = {
   L"Center", 0, 0
};

// #define     SKYRES               ((DWORD)(500 << m_dwResolution))                    // resolution across and down in pixels
   // BUGFIX - Changes sky resolution to be 250 x 250 at lowest
   // BUGFIX - Upped lowest to 500 x 500 since 250 x 250 is too low
//#define     DENSITYATMID         .15                     // atmopheric density at midpoint
#define     DENSITYATZENITH      m_fDensityZenith                     // atmopheric desnity at zenith
#define     DENSITYATGROUND      .99                     // atmopheric density at ground level

// BUGFIX - Dont base SKYDOMEGRID on m_dwResolution anymore
//#define     SKYDOMEGRID          (15.0 / (fp)(m_dwResolution+1))                      // number of degrees in each grid
#define     SKYDOMEGRID          15.0                      // number of degrees in each grid
#define     SKYDOMETRI           (90 / SKYDOMEGRID)      // number of columns unique to 1/4 of skydome
#define     SKYDOMECOLUMNS       (4 * SKYDOMETRI)        // number of columns unique to skydome
#define     SKYDOMEROWS          (SKYDOMETRI + 1)        // number of rows (excluding extra one)
#define     SKYDOMEROWSEXTRA     (SKYDOMEROWS + 1)       // number of rows (including extra one)
#define     SKYDOMELATRANGE      ((90 + SKYDOMEGRID) / 360.0 * 2.0 * PI)   // number of angles of lattitude covered by skydome

#define     SUNMOONSIZE          (0.53 / 180.0 * PI)      // sun/moon are 5 degrees of arc


// skydome textures
#define MAXSKYDOMECACHE          10              // dont cache any more than this number of skydomes
   // BUGFIX - Upped from 5 to 10 so wouldn't run out
static DWORD      gadwNumSkydome[MAXRENDERSHARDS] = {0, 0, 0, 0};      // number of skydomes
static __int64    gaiCachSkydomeTime[MAXRENDERSHARDS] = {0, 0, 0, 0};        // current tick count for last used
static double     gafCacheHash[MAXRENDERSHARDS][MAXSKYDOMECACHE]; // hash for the cache
static GUID       gagCacheGUID[MAXRENDERSHARDS][MAXSKYDOMECACHE]; // GUID
static __int64    gaiCacheUsed[MAXRENDERSHARDS][MAXSKYDOMECACHE]; // when last used


/**********************************************************************************
LumensToRange - Converts lumens to a range from 0..1

inputs
   fp       fLumens - Lumens, from very small to CM_LUMENSSUN*2
returns
   fp - Rande from 0 to 1
*/
#define LUMMAX    12
fp LumensToRange (fp fLumens)
{
   fLumens = log(fLumens / CM_LUMENSSUN);
   fLumens += (LUMMAX-1);   // so 2.7 * CM_LUMENSSUN = 12
   fLumens /= LUMMAX;
   fLumens = max(0,fLumens);
   fLumens = min(1,fLumens);
   return fLumens;
}


/**********************************************************************************
RangeToLumens - The opposite of Lumens to range, converts from 0..1 into lumens

inputs
   fp       fRange - from 0 to 1
returns
   fp - Lumens, up to 2.7 * CM_LUMENSSUN
*/
fp RangeToLumens (fp fRange)
{
   fRange *= LUMMAX;
   fRange -= (LUMMAX-1);
   fRange = exp(fRange) * CM_LUMENSSUN;
   return fRange;
}



/**********************************************************************************
CObjectSkydome::Constructor and destructor */
CObjectSkydome::CObjectSkydome (PVOID pParams, POSINFO pInfo)
{
   m_dwRenderShow = RENDERSHOW_WEATHER;
   m_OSINFO = *pInfo;

   if (!gadwNumSkydome[m_OSINFO.dwRenderShard])
      memset (gaiCacheUsed[m_OSINFO.dwRenderShard], 0, sizeof(gaiCacheUsed[m_OSINFO.dwRenderShard]));
   gadwNumSkydome[m_OSINFO.dwRenderShard]++;

   GammaInit();
   m_lAttrib.Init (sizeof(ATTRIBVAL));

   // GUIDGen (&m_gTexture);
   m_dwResolution = 0;  // default to lowest resoltuon
   m_lSuns.Init (sizeof(SKYDOMELIGHT));

   // Call default building settgings
   fp fLat;
   DWORD dwDate, dwTime;
   DefaultBuildingSettings (&dwDate, &dwTime, &fLat);
   m_fYear = YEARFROMDFDATE(dwDate);
   m_fTimeInYear = DFDATEToTimeInYear (dwDate);
   m_fTimeInDay = DFTIMEToTimeInDay (dwTime);

   // calc sun posn
   memset (&m_lMoon, 0, sizeof(m_lMoon));
   memset (&m_lSun, 0, sizeof(m_lSun));
   if (!m_lMoon.pSurface)
      m_lMoon.pSurface = new CObjectSurface;
   m_fSunMoonDirty = TRUE;
   m_fMoonSize = 1;
   m_fShowSun = m_fShowMoon = TRUE;
   m_fDimClouds = TRUE;
   m_fHaze = 1;
   m_fDensityZenith = .05;
   m_dwStarsNum = 1000;

   DWORD i;
   for (i = 0; i < CLOUDLAYERS; i++) {
      m_afCirrusDraw[i] = TRUE;
      m_acCirrusColor[i] = RGB(0xff,0xff,0xff);
      m_afCirrusScale[i] = 20000;
      m_afCirrusDetail[i] = .5;
      m_afCirrusCover[i] = i ? .5 : .3;   // BUGFIX - If i==1 then stratonimbus, so make more dense
      m_afCirrusThickness[i] = i ? .5 : .2;
      m_apCirrusLoc[i].Zero();
      m_apCirrusLoc[i].p[2] = i ? 3000 : 8000;
      m_adwCirrusSeed[i] = 10+i;
   }

   m_fCumulusDraw = FALSE; // BUGFIX - Default to not drawing cumulus since not so good
   m_cCumulusColor = RGB(0xff,0xff,0xff);
   m_fCumulusScale = 20000;
   m_fCumulusSheer = .25;
   m_fCumulusSheerAngle = 0;
   m_fCumulusDetail = .5;
   m_fCumulusCover = .3;
   m_fCumulusThickness = .2;
   m_pCumulusLoc.Zero();
   m_pCumulusLoc.p[2] = 1500;
   m_dwCumulusSeed = 1;


   // default colors
   m_cAtmosphereGround = RGB(0xe0,0xe0,0xff);
   m_cAtmosphereMid = RGB(0x90,0x90,0xff);
   m_cAtmosphereZenith = RGB(0x40, 0x40, 0xe0);
   m_fAutoColors = TRUE;


   // init memory for dome
   m_dwNumPoints = m_dwNumNormals = m_dwNumText = m_dwNumVertices = m_dwNumPoly = 0;
   m_fSkydomeSize = 10 * 1000;  // 10 km radius

   CalcAttribList();
}


CObjectSkydome::~CObjectSkydome (void)
{
   // delete this dome's texture if there is one
   // TextureCacheDelete (&m_gTexture, &m_gTexture);

   if (m_lSun.pSurface)
      delete m_lSun.pSurface;
   if (m_lMoon.pSurface)
      delete m_lMoon.pSurface;

   // Free up other object sufraces
   DWORD i;
   PSKYDOMELIGHT pSun;
   pSun = (PSKYDOMELIGHT) m_lSuns.Get(0);
   for (i = 0; i < m_lSuns.Num(); i++, pSun++) {
      if (pSun->pSurface)
         delete pSun->pSurface;
   }
   m_lSuns.Clear();

   // if just freed up the last skydome then make sure all the caches are freed
   gadwNumSkydome[m_OSINFO.dwRenderShard]--;
   if (!gadwNumSkydome[m_OSINFO.dwRenderShard])
      for (i = 0; i < MAXSKYDOMECACHE; i++)
         if (gaiCacheUsed[m_OSINFO.dwRenderShard][i])
            TextureCacheDelete (m_OSINFO.dwRenderShard, &gagCacheGUID[m_OSINFO.dwRenderShard][i], &gagCacheGUID[m_OSINFO.dwRenderShard][i]);
}


/**********************************************************************************
CObjectSkydome::Delete - Called to delete this object
*/
void CObjectSkydome::Delete (void)
{
   delete this;
}

/**********************************************************************************
CObjectSkydome::Render - Draws a test box.

inputs
   POBJECTRENDER     pr - Render information
*/
void CObjectSkydome::Render (POBJECTRENDER pr, DWORD dwSubObject)
{
   // fill in the sky dome shape if necessary
   if (!m_dwNumPoints)
      CreateDome ();
   
   // create the surface render object and draw
   // CRenderSurface rs;
	MALLOCOPT_INIT;
	MALLOCOPT_OKTOMALLOC;
   m_Renderrs.ClearAll();
	MALLOCOPT_RESTORE;

   CMatrix mObject;
   m_Renderrs.Init (pr->pRS);
   ObjectMatrixGet (&mObject);
   m_Renderrs.Multiply (&mObject);

   // BUGFIX - Only make sure texture exists when actually want to draw.
   // Do this because there was a bug when added skydome to EA house, the first time
   // called didn't have any lattitude because was merely querying bounding box
   if (m_Renderrs.m_fNeedTextures)
      MakeSureTextureExists();

   GUID gTexture;
   CalcTextureGUID (&gTexture);

   // set the surface
   RENDERSURFACE Mat;
   CMatrix mIdent;
   mIdent.Identity();
   memset (&Mat, 0, sizeof(Mat));
   Mat.afTextureMatrix[0][0] = Mat.afTextureMatrix[1][1] = 1;
   memcpy (Mat.abTextureMatrix, &mIdent, sizeof(mIdent));
   Mat.fUseTextureMap = TRUE;
   Mat.gTextureCode = Mat.gTextureSub = gTexture;
   Mat.wMinorID = 1;
   Mat.Material.InitFromID (MATERIAL_FLAT);
   Mat.TextureMods.cTint = RGB(0xff,0xff,0xff);
   Mat.TextureMods.wBrightness = 0x1000;
   Mat.TextureMods.wContrast = 0x1000;
   Mat.TextureMods.wHue = 0x0000;
   Mat.TextureMods.wSaturation = 0x1000;
   Mat.Material.m_fGlow = TRUE;  // set this so will not apply lighting to it
   Mat.Material.m_fNoShadows = TRUE;
   PCTextureMapSocket pText;
   pText = TextureCacheGetDynamic (m_OSINFO.dwRenderShard, &gTexture, &gTexture, NULL, NULL);
   m_Renderrs.SetDefColor (pText ? pText->AverageColorGet(0, TRUE) : RGB(0xe0,0xe0,0xff));
      // NOTE: Include glow color
   m_Renderrs.SetDefMaterial (&Mat);

   // draw it
   DWORD dwColor, dwPoint, dwNorm, dwText, dwVert;
   PCPoint paPoint, paNorm;
   PTEXTPOINT5 paText;
   PVERTEX paVert;
   dwColor = m_Renderrs.DefColor ();
   paPoint = m_Renderrs.NewPoints (&dwPoint, m_dwNumPoints);
   paNorm = m_Renderrs.NewNormals (TRUE, &dwNorm, m_dwNumNormals);
   paText = m_Renderrs.NewTextures (&dwText, m_dwNumText);
   paVert = m_Renderrs.NewVertices (&dwVert, m_dwNumVertices);
   if (!paPoint || !paVert) {
      m_Renderrs.Commit();
      return;  // errro
   }
   if (!paNorm)
      dwNorm = 0;
   if (!paText)
      dwText = 0;
   memcpy (paPoint, m_memPoints.p, m_dwNumPoints * sizeof(CPoint));
   if (paNorm)
      memcpy (paNorm, m_memNormals.p, m_dwNumNormals * sizeof(CPoint));
   if (paText)
      memcpy (paText, m_memText.p, m_dwNumText * sizeof(TEXTPOINT5));
   memcpy (paVert, m_memVertices.p, m_dwNumVertices * sizeof(VERTEX));
   DWORD i;
   for (i = 0; i < m_dwNumVertices; i++) {
      paVert[i].dwColor = dwColor;
      paVert[i].dwNormal += dwNorm;
      paVert[i].dwPoint += dwPoint;
      paVert[i].dwTexture += dwText;
   }

   // all the polygons
   DWORD *padw;
   padw = (DWORD*) m_memPoly.p;
   for (i = 0; i < m_dwNumPoly; i++, padw += 3) {
      m_Renderrs.NewTriangle (padw[0] + dwVert, padw[1] + dwVert, padw[2] + dwVert, TRUE);
   }

   m_Renderrs.Commit();
}


/**********************************************************************************
CObjectSkydome::QueryBoundingBox - Standard API
*/
void CObjectSkydome::QueryBoundingBox (PCPoint pCorner1, PCPoint pCorner2, DWORD dwSubObject)
{
   pCorner1->p[0] = -m_fSkydomeSize;
   pCorner1->p[1] = -m_fSkydomeSize;
   pCorner1->p[2] = -m_fSkydomeSize/3; // can go down a bit
   pCorner2->p[0] = m_fSkydomeSize;
   pCorner2->p[1] = m_fSkydomeSize;
   pCorner2->p[2] = m_fSkydomeSize;


#ifdef _DEBUG
   // test, make sure bounding box not too small
   CPoint p1,p2;
   DWORD i;
   CObjectTemplate::QueryBoundingBox (&p1, &p2, dwSubObject);
   for (i = 0; i < 3; i++)
      if ((p1.p[i] < pCorner1->p[i]) || (p2.p[i] > pCorner2->p[i]))
         break;
   if (i < 3)
      OutputDebugString ("\r\nCObjectSkydome::QueryBoundingBox too small.");
#endif
}

/**********************************************************************************
CObjectSkydome::Clone - Clones this

inputs
   none
returns
   PCObjectSocket - Clone
*/
PCObjectSocket CObjectSkydome::Clone (void)
{
   PCObjectSkydome pNew;

   pNew = new CObjectSkydome(NULL, &m_OSINFO);

   // Free up other object sufraces
   DWORD i;
   PSKYDOMELIGHT pSun;
   pSun = (PSKYDOMELIGHT) pNew->m_lSuns.Get(0);
   for (i = 0; i < pNew->m_lSuns.Num(); i++, pSun++) {
      if (pSun->pSurface)
         delete pSun->pSurface;
   }
   pNew->m_lSuns.Clear();
   if (pNew->m_lSun.pSurface)
      delete pNew->m_lSun.pSurface;
   if (pNew->m_lMoon.pSurface)
      delete pNew->m_lMoon.pSurface;

   // clone template info
   CloneTemplate(pNew);

   // clone info
   pNew->m_fYear = m_fYear;
   pNew->m_fTimeInDay = m_fTimeInDay;
   pNew->m_fTimeInYear = m_fTimeInYear;
   pNew->m_dwStarsNum = m_dwStarsNum;
   pNew->m_fMoonSize = m_fMoonSize;
   pNew->m_fShowSun =m_fShowSun;
   pNew->m_fDimClouds = m_fDimClouds;
   pNew->m_fShowMoon = m_fShowMoon;
   pNew->m_dwResolution = m_dwResolution;
   pNew->m_fHaze = m_fHaze;
   pNew->m_fDensityZenith = m_fDensityZenith;
   pNew->m_lSun = m_lSun;
   pNew->m_lMoon = m_lMoon;
   if (pNew->m_lSun.pSurface)
      pNew->m_lSun.pSurface = pNew->m_lSun.pSurface->Clone();
   if (pNew->m_lMoon.pSurface)
      pNew->m_lMoon.pSurface = pNew->m_lMoon.pSurface->Clone();
   pNew->m_fSunMoonDirty =m_fSunMoonDirty;
   pNew->m_cAtmosphereGround =m_cAtmosphereGround;
   pNew->m_cAtmosphereMid = m_cAtmosphereMid;
   pNew->m_cAtmosphereZenith = m_cAtmosphereZenith;
   pNew->m_fAutoColors = m_fAutoColors;
   // NOTE: Specically NOT clongin m_gTexture

   for (i = 0; i < CLOUDLAYERS; i++) {
      pNew->m_afCirrusDraw[i] = m_afCirrusDraw[i];
      pNew->m_acCirrusColor[i] = m_acCirrusColor[i];
      pNew->m_afCirrusScale[i] = m_afCirrusScale[i];
      pNew->m_afCirrusDetail[i] = m_afCirrusDetail[i];
      pNew->m_afCirrusCover[i] = m_afCirrusCover[i];
      pNew->m_afCirrusThickness[i] = m_afCirrusThickness[i];
      pNew->m_apCirrusLoc[i].Copy (&m_apCirrusLoc[i]);
      pNew->m_adwCirrusSeed[i] = m_adwCirrusSeed[i];
   }

   pNew->m_fCumulusDraw = m_fCumulusDraw;
   pNew->m_cCumulusColor = m_cCumulusColor;
   pNew->m_fCumulusScale = m_fCumulusScale;
   pNew->m_fCumulusSheer = m_fCumulusSheer;
   pNew->m_fCumulusSheerAngle = m_fCumulusSheerAngle;
   pNew->m_fCumulusDetail = m_fCumulusDetail;
   pNew->m_fCumulusCover = m_fCumulusCover;
   pNew->m_fCumulusThickness = m_fCumulusThickness;
   pNew->m_pCumulusLoc.Copy (&m_pCumulusLoc);
   pNew->m_dwCumulusSeed = m_dwCumulusSeed;

   pNew->m_dwNumPoints = m_dwNumPoints;
   pNew->m_dwNumNormals = m_dwNumNormals;
   pNew->m_dwNumText = m_dwNumText;
   pNew->m_dwNumVertices = m_dwNumVertices;
   pNew->m_dwNumPoly = m_dwNumPoly;
   pNew->m_fSkydomeSize = m_fSkydomeSize;
   DWORD dwSize;
   dwSize = m_dwNumPoints * sizeof(CPoint);
   if (pNew->m_memPoints.Required (dwSize))
      memcpy (pNew->m_memPoints.p, m_memPoints.p, dwSize);
   dwSize = m_dwNumNormals * sizeof(CPoint);
   if (pNew->m_memNormals.Required (dwSize))
      memcpy (pNew->m_memNormals.p, m_memNormals.p, dwSize);
   dwSize = m_dwNumText * sizeof(TEXTPOINT5);
   if (pNew->m_memText.Required (dwSize))
      memcpy (pNew->m_memText.p, m_memText.p, dwSize);
   dwSize = m_dwNumVertices * sizeof(VERTEX);
   if (pNew->m_memVertices.Required (dwSize))
      memcpy (pNew->m_memVertices.p, m_memVertices.p, dwSize);
   dwSize = m_dwNumPoly * (3 * sizeof(DWORD));
   if (pNew->m_memPoly.Required (dwSize))
      memcpy (pNew->m_memPoly.p, m_memPoly.p, dwSize);

   pNew->m_lAttrib.Init (sizeof(ATTRIBVAL), m_lAttrib.Get(0), m_lAttrib.Num());

   // clone the clouds
   pNew->m_lSuns.Init (sizeof(SKYDOMELIGHT), m_lSuns.Get(0), m_lSuns.Num());
   pSun = (PSKYDOMELIGHT) pNew->m_lSuns.Get(0);
   for (i = 0; i < pNew->m_lSuns.Num(); i++, pSun++) {
      if (pSun->pSurface)
         pSun->pSurface = pSun->pSurface->Clone();
   }

   return pNew;
}

static PWSTR gpszSkydomeSize = L"SkydomeSize";
static PWSTR gpszAtmosphereGround = L"AtmosphereGround";
static PWSTR gpszAtmosphereMid = L"AtmosphereMid";
static PWSTR gpszAtmosphereZenith = L"AtmosphereZenth";
static PWSTR gpszAtmosphereMidAngle = L"AtmosphereMidAngle";
static PWSTR gpszMoonSize = L"MoonSize";
static PWSTR gpszHaze = L"Haze";
static PWSTR gpszStarsNum = L"StarsNum";
static PWSTR gpszCirrusDraw = L"CirrusDraw%d";
static PWSTR gpszCirrusColor = L"CirrusColor%d";
static PWSTR gpszCirrusScale = L"CirrusScale%d";
static PWSTR gpszCirrusDetail = L"CirrusDetail%d";
static PWSTR gpszCirrusCover = L"CirrusCover%d";
static PWSTR gpszCirrusThickness = L"CirrusThickness%d";
static PWSTR gpszCirrusLoc = L"CirrusLoc%d";
static PWSTR gpszResolution = L"Resolution";
static PWSTR gpszCumulusDraw = L"CumulusDraw";
static PWSTR gpszCumulusColor = L"CumulusColor";
static PWSTR gpszCumulusScale = L"CumulusScale";
static PWSTR gpszCumulusDetail = L"CumulusDetail";
static PWSTR gpszCumulusCover = L"CumulusCover";
static PWSTR gpszCumulusThickness = L"CumulusThickness";
static PWSTR gpszCumulusLoc = L"CumulusLoc";
static PWSTR gpszCumulusSheer = L"CumulusSheer";
static PWSTR gpszCumulusSheerAngle = L"CumulusSheerAngle";
static PWSTR gpszDensityAtZenith = L"DensityAtZenth";
static PWSTR gpszAutoColors = L"AutoColors";
static PWSTR gpszShowSun = L"ShowSun";
static PWSTR gpszShowMoon = L"ShowMoon";
static PWSTR gpszDimClouds = L"DimClouds";
static PWSTR gpszCirrusSeed = L"CirrusSeed%d";
static PWSTR gpszCumulusSeed = L"CumulusSeed";
static PWSTR gpszSun = L"Sun";
static PWSTR gpszVisible = L"Visible";
static PWSTR gpszDir = L"Dir";
static PWSTR gpszLumens = L"Lumens";
static PWSTR gpszColor = L"Color";
static PWSTR gpszAmbientLumens = L"AmbientLumens";
static PWSTR gpszAmbientColor = L"AmbientColor";
static PWSTR gpszSolarLumens = L"SolarLumens";
static PWSTR gpszSolarColor = L"SolarColor";
static PWSTR gpszSizeRad = L"SizeRad";
static PWSTR gpszEmitLight = L"EmitLight";
static PWSTR gpszBrightness = L"Brightness";
static PWSTR gpszSurface = L"Surface";
static PWSTR gpszDist = L"Dist";
static PWSTR gpszYear = L"Year";
static PWSTR gpszTimeInDay = L"TimeInDay";
static PWSTR gpszTimeInYear = L"TimeInYear";
static PWSTR gpszName = L"Name";

PCMMLNode2 CObjectSkydome::MMLTo (void)
{
   // call into the template first
   PCMMLNode2 pNode = MMLToTemplate();
   if (!pNode)
      return NULL;

   MMLValueSet (pNode, gpszYear, m_fYear);
   MMLValueSet (pNode, gpszTimeInDay, m_fTimeInDay);
   MMLValueSet (pNode, gpszTimeInYear, m_fTimeInYear);
   MMLValueSet (pNode, gpszSkydomeSize, m_fSkydomeSize);
   MMLValueSet (pNode, gpszAtmosphereGround, (int)m_cAtmosphereGround);
   MMLValueSet (pNode, gpszAtmosphereMid, (int)m_cAtmosphereMid);
   MMLValueSet (pNode, gpszAtmosphereZenith, (int)m_cAtmosphereZenith);
   MMLValueSet (pNode, gpszAutoColors, (int)m_fAutoColors);
   MMLValueSet (pNode, gpszMoonSize, m_fMoonSize);
   MMLValueSet (pNode, gpszShowSun, (int)m_fShowSun);
   MMLValueSet (pNode, gpszDimClouds, (int)m_fDimClouds);
   MMLValueSet (pNode, gpszShowMoon, (int)m_fShowMoon);
   MMLValueSet (pNode, gpszResolution, (int) m_dwResolution);
   MMLValueSet (pNode, gpszHaze, m_fHaze);
   MMLValueSet (pNode, gpszDensityAtZenith, m_fDensityZenith);
   MMLValueSet (pNode, gpszStarsNum, (int)m_dwStarsNum);

   DWORD i;
   for (i = 0; i < CLOUDLAYERS; i++) {
      WCHAR szTemp[64];

      swprintf (szTemp, gpszCirrusDraw, (int) i);
      MMLValueSet (pNode, szTemp, (int) m_afCirrusDraw[i]);

      swprintf (szTemp, gpszCirrusColor, (int) i);
      MMLValueSet (pNode, szTemp, (int) m_acCirrusColor[i]);

      swprintf (szTemp, gpszCirrusScale, (int) i);
      MMLValueSet (pNode, szTemp, m_afCirrusScale[i]);

      swprintf (szTemp, gpszCirrusDetail, (int) i);
      MMLValueSet (pNode, szTemp, m_afCirrusDetail[i]);

      swprintf (szTemp, gpszCirrusCover, (int) i);
      MMLValueSet (pNode, szTemp, m_afCirrusCover[i]);

      swprintf (szTemp, gpszCirrusThickness, (int) i);
      MMLValueSet (pNode, szTemp, m_afCirrusThickness[i]);

      swprintf (szTemp, gpszCirrusLoc, (int) i);
      MMLValueSet (pNode, szTemp, &m_apCirrusLoc[i]);

      swprintf (szTemp, gpszCirrusSeed, (int) i);
      MMLValueSet (pNode, szTemp, (int)m_adwCirrusSeed[i]);
   }

   MMLValueSet (pNode, gpszCumulusDraw, (int) m_fCumulusDraw);
   MMLValueSet (pNode, gpszCumulusColor, (int) m_cCumulusColor);
   MMLValueSet (pNode, gpszCumulusScale, m_fCumulusScale);
   MMLValueSet (pNode, gpszCumulusDetail, m_fCumulusDetail);
   MMLValueSet (pNode, gpszCumulusCover, m_fCumulusCover);
   MMLValueSet (pNode, gpszCumulusThickness, m_fCumulusThickness);
   MMLValueSet (pNode, gpszCumulusLoc, &m_pCumulusLoc);
   MMLValueSet (pNode, gpszCumulusSeed, (int)m_dwCumulusSeed);

   MMLValueSet (pNode, gpszCumulusSheer, m_fCumulusSheer);
   MMLValueSet (pNode, gpszCumulusSheerAngle, m_fCumulusSheerAngle);


   // write out the suns
   PSKYDOMELIGHT pSun;
   pSun = (PSKYDOMELIGHT) m_lSuns.Get(0);
   for (i = 0; i < m_lSuns.Num(); i++, pSun++) {
      PCMMLNode2 pSub = pNode->ContentAddNewNode ();
      if (!pSub)
         continue;
      pSub->NameSet (gpszSun);

      if (pSun->szName[0])
         MMLValueSet (pSub, gpszName, pSun->szName);
      MMLValueSet (pSub, gpszVisible, pSun->fVisible);
      MMLValueSet (pSub, gpszDir, &pSun->pDir);
      MMLValueSet (pSub, gpszLumens, pSun->fLumens);
      MMLValueSet (pSub, gpszColor, (int) UnGamma (pSun->awColor));
      MMLValueSet (pSub, gpszAmbientLumens, pSun->fAmbientLumens);
      MMLValueSet (pSub, gpszAmbientColor, (int) UnGamma(pSun->awAmbientColor));
      MMLValueSet (pSub, gpszSolarLumens, pSun->fSolarLumens);
      MMLValueSet (pSub, gpszSolarColor, (int) UnGamma(pSun->awSolarColor));
      MMLValueSet (pSub, gpszSizeRad, pSun->fSizeRad);
      MMLValueSet (pSub, gpszDist, (fp)(pSun->fDist / 1000000.0));
      MMLValueSet (pSub, gpszEmitLight, (int) pSun->fEmitLight);
      MMLValueSet (pSub, gpszBrightness, pSun->fBrightness);
      if (pSun->pSurface) {
         PCMMLNode2 pSurf = pSun->pSurface->MMLTo();
         if (pSurf) {
            pSurf->NameSet (gpszSurface);
            pSub->ContentAdd (pSurf);
         }
      }
   }  // i

   return pNode;
}

BOOL CObjectSkydome::MMLFrom (PCMMLNode2 pNode)
{
   // Free up other object sufraces
   DWORD i;
   PSKYDOMELIGHT pSun;
   pSun = (PSKYDOMELIGHT) m_lSuns.Get(0);
   for (i = 0; i < m_lSuns.Num(); i++, pSun++) {
      if (pSun->pSurface)
         delete pSun->pSurface;
   }
   m_lSuns.Clear();

   // call into the template first
   if (!MMLFromTemplate (pNode))
      return FALSE;

   m_fYear = MMLValueGetDouble (pNode, gpszYear, 2003);
   m_fTimeInDay = MMLValueGetDouble (pNode, gpszTimeInDay, 0);
   m_fTimeInYear = MMLValueGetDouble (pNode, gpszTimeInYear, 0);
   m_fSkydomeSize = MMLValueGetDouble (pNode, gpszSkydomeSize, 10 * 1000);
   m_dwStarsNum = (DWORD) MMLValueGetInt (pNode, gpszStarsNum, 0);

   m_cAtmosphereGround = (COLORREF) MMLValueGetInt (pNode, gpszAtmosphereGround, 0);
   m_cAtmosphereMid = (COLORREF) MMLValueGetInt (pNode, gpszAtmosphereMid, 0);
   m_cAtmosphereZenith = (COLORREF) MMLValueGetInt (pNode, gpszAtmosphereZenith, 0);
   m_fAutoColors = (BOOL) MMLValueGetInt (pNode, gpszAutoColors, TRUE);
   m_fShowSun = (BOOL) MMLValueGetInt (pNode, gpszShowSun, TRUE);
   m_fDimClouds = (BOOL) MMLValueGetInt (pNode, gpszDimClouds, TRUE);
   m_fShowMoon = (BOOL) MMLValueGetInt (pNode, gpszShowMoon, TRUE);
   m_fMoonSize = MMLValueGetDouble (pNode, gpszMoonSize, 1);
   m_dwResolution = (DWORD) MMLValueGetInt (pNode, gpszResolution, 0);
   m_fHaze = MMLValueGetDouble (pNode, gpszHaze, 1);
   m_fDensityZenith = MMLValueGetDouble (pNode, gpszDensityAtZenith, .05);
   m_fDensityZenith = max(.001, m_fDensityZenith);

   for (i = 0; i < CLOUDLAYERS; i++) {
      WCHAR szTemp[64];

      swprintf (szTemp, gpszCirrusDraw, (int) i);
      m_afCirrusDraw[i] = (BOOL) MMLValueGetInt (pNode, szTemp, FALSE);

      swprintf (szTemp, gpszCirrusColor, (int) i);
      m_acCirrusColor[i] = (COLORREF) MMLValueGetInt (pNode, szTemp, RGB(0xff,0xff,0xff));

      swprintf (szTemp, gpszCirrusScale, (int) i);
      m_afCirrusScale[i] = MMLValueGetDouble (pNode, szTemp, 8000);

      swprintf (szTemp, gpszCirrusDetail, (int) i);
      m_afCirrusDetail[i] = MMLValueGetDouble (pNode, szTemp, .5);

      swprintf (szTemp, gpszCirrusCover, (int) i);
      m_afCirrusCover[i] = MMLValueGetDouble (pNode, szTemp, .5);

      swprintf (szTemp, gpszCirrusThickness, (int) i);
      m_afCirrusThickness[i] = MMLValueGetDouble (pNode, szTemp, .5);

      swprintf (szTemp, gpszCirrusLoc, (int) i);
      MMLValueGetPoint (pNode, szTemp, &m_apCirrusLoc[i]);

      swprintf (szTemp, gpszCirrusSeed, (int) i);
      m_adwCirrusSeed[i] = (DWORD) MMLValueGetInt (pNode, szTemp, 0);
   }

   m_fCumulusDraw = (BOOL) MMLValueGetInt (pNode, gpszCumulusDraw, FALSE);
   m_cCumulusColor = (COLORREF) MMLValueGetInt (pNode, gpszCumulusColor, RGB(0xff,0xff,0xff));
   m_fCumulusScale = MMLValueGetDouble (pNode, gpszCumulusScale, 8000);
   m_fCumulusDetail = MMLValueGetDouble (pNode, gpszCumulusDetail, .5);
   m_fCumulusCover = MMLValueGetDouble (pNode, gpszCumulusCover, .5);
   m_fCumulusThickness = MMLValueGetDouble (pNode, gpszCumulusThickness, .5);
   MMLValueGetPoint (pNode, gpszCumulusLoc, &m_pCumulusLoc);
   m_dwCumulusSeed = (DWORD) MMLValueGetInt (pNode, gpszCumulusSeed, 0);

   m_fCumulusSheer = MMLValueGetDouble (pNode, gpszCumulusSheer, 0);
   m_fCumulusSheerAngle = MMLValueGetDouble (pNode, gpszCumulusSheerAngle, 0);


   // read out the suns
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

      // only looking for suns
      if (_wcsicmp (psz, gpszSun))
         continue;

      SKYDOMELIGHT Sun;
      memset (&Sun, 0, sizeof(Sun));

      psz = MMLValueGet (pSub, gpszName);
      if (psz)
         wcscpy (Sun.szName, psz);
      else
         Sun.szName[0] = 0;
      Sun.fVisible = (BOOL) MMLValueGetInt (pSub, gpszVisible, TRUE);
      MMLValueGetPoint (pSub, gpszDir, &Sun.pDir);

      COLORREF c;
      Sun.fLumens = MMLValueGetDouble (pSub, gpszLumens, 0);
      c = (COLORREF) MMLValueGetInt (pSub, gpszColor, 0);
      Gamma (c, Sun.awColor);

      Sun.fAmbientLumens = MMLValueGetDouble (pSub, gpszAmbientLumens, 0);
      c = (COLORREF) MMLValueGetInt (pSub, gpszAmbientColor, 0);
      Gamma (c, Sun.awAmbientColor);

      Sun.fSolarLumens = MMLValueGetDouble (pSub, gpszSolarLumens, 0);
      c = (COLORREF) MMLValueGetInt (pSub, gpszSolarColor, 0);
      Gamma (c, Sun.awSolarColor);

      Sun.fSizeRad = MMLValueGetDouble (pSub, gpszSizeRad, 1);
      Sun.fDist = 1000000.0 * MMLValueGetDouble (pSub, gpszDist, 1);
      Sun.fEmitLight = (BOOL) MMLValueGetInt (pSub, gpszEmitLight, TRUE);
      Sun.fBrightness = MMLValueGetDouble (pSub, gpszBrightness, 1);

      PCMMLNode2 pSurf;
      pSurf = NULL;
      pSub->ContentEnum (pSub->ContentFind (gpszSurface), &psz, &pSurf);
      if (pSurf) {
         Sun.pSurface = new CObjectSurface;
         if (Sun.pSurface)
            Sun.pSurface->MMLFrom (pSurf);
      }

      m_lSuns.Add (&Sun);
   }  // i

   // calc sun posn
   CalcAll ();
   m_dwNumPoints = 0;   // so re-calc skydome
   CalcAttribList();

   // delete this dome's texture if there is one
   // TextureCacheDelete (&m_gTexture, &m_gTexture);

   return TRUE;
}

/**************************************************************************************
CObjectSkydome::MoveReferencePointQuery - 
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
BOOL CObjectSkydome::MoveReferencePointQuery (DWORD dwIndex, PCPoint pp)
{
   PSPLINEMOVEP ps;
   DWORD dwDataSize;
   ps = gaSkydomeMove;
   dwDataSize = sizeof(gaSkydomeMove);

   if (dwIndex >= dwDataSize / sizeof(SPLINEMOVEP))
      return FALSE;

   // always at 0,0 in Skydomes
   pp->Zero();
   return TRUE;
}

/**************************************************************************************
CObjectSkydome::MoveReferenceStringQuery -
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
BOOL CObjectSkydome::MoveReferenceStringQuery (DWORD dwIndex, PWSTR psz, DWORD dwSize, DWORD *pdwNeeded)
{
   PSPLINEMOVEP ps;
   DWORD dwDataSize;
   ps = gaSkydomeMove;
   dwDataSize = sizeof(gaSkydomeMove);
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
CObjectSkydome::CalcSunAndMoon - Calculates information based on the sun and the moon.
Takes m_fYear, m_fTimeInYear, m_fTimeInDay. Fills in information such as m_lSun and m_lMoon
*/
void CObjectSkydome::CalcSunAndMoon (void)
{
   if (!m_fSunMoonDirty)
      return;
   m_fSunMoonDirty = FALSE;


   double fLatitude;
   double fTrueNorth;
   // Call default building settgings
   DWORD dwDate, dwTime;
   fp fLatDef;
   DefaultBuildingSettings (&dwDate, &dwTime, &fLatDef);
   fTrueNorth = 0;
   fLatitude = fLatDef;
   PWSTR psz;
   if (m_pWorld) {
      psz = m_pWorld->VariableGet (WSLatitude());
      if (!psz || !AttribToDouble(psz, &fLatitude))
         fLatitude = fLatDef;
      psz = m_pWorld->VariableGet (WSTrueNorth());
      if (!psz || !AttribToDouble(psz, &fTrueNorth))
         fTrueNorth = 0;
   }

   // calculate cloud cover
   fp fClouds, fCloudCover, fCloudsNoCeiling;
   if (m_fCumulusDraw) {
      fCloudCover = m_fCumulusCover + m_afCirrusCover[1]/2;
      fClouds = fCloudsNoCeiling = m_fCumulusCover * sqrt(m_fCumulusThickness) +
         m_afCirrusCover[1] * sqrt(m_afCirrusThickness[1]);
   }
   else
      fClouds = fCloudsNoCeiling = fCloudCover = 0;
   fClouds = min(1,fClouds);
   fCloudCover = min(1, fCloudCover);
   if (!m_fDimClouds)
      fClouds = fCloudCover = 0;

   // calculate where the sun is
   CPoint pSun;
   fp fLat;
   fLat = fLatitude / 2.0 / PI * 360.0;
   SunVector ((DWORD)m_fYear, m_fTimeInYear, m_fTimeInDay, fLat, &pSun);
   // rotate this
   CMatrix m;
   m.RotationZ (-fTrueNorth);
   pSun.MultiplyLeft (&m); // normally would invert and transpose, but just put in the opposite angle
   pSun.Normalize();
   m_lSun.pDir.Copy (&pSun);

   // and moon location
   fp fMoonPhase;
   CPoint pMoonVector;
   fMoonPhase = MoonVector ((DWORD)m_fYear, m_fTimeInYear, m_fTimeInDay, fLat, &pMoonVector);
   fMoonPhase = sin(fMoonPhase * PI);   // so know intensity
   pMoonVector.MultiplyLeft (&m);
   m_lMoon.pDir.Copy (&pMoonVector);

   // Set the light intensity based on the sun's height and cloud cover when sunlight and
   // moonlight brightness
   fp fScale;
   fScale = 1.01 - sqrt(fClouds);   // use 1.01 so somelight gets through. If no sun creates automatic light
   m_lSun.fLumens = sqrt(max(m_lSun.pDir.p[2]*.9+.1, 0)) * fScale * CM_LUMENSSUN;
   m_lSun.fVisible = TRUE;


   m_lMoon.fLumens = sqrt(max(m_lMoon.pDir.p[2]*.9+.1, 0)) * fScale * CM_LUMENSMOON * fMoonPhase;
   m_lMoon.fVisible = TRUE;

   // solar strength
   m_lSun.fSolarLumens = CM_LUMENSSUN;
   m_lMoon.fSolarLumens = CM_LUMENSMOON;
   Gamma (RGB(0xff,0xff,0xff),m_lSun.awSolarColor);
   Gamma (RGB(0xf0,0xf0,0xff),m_lMoon.awSolarColor);

   // Set the light's color based on the sun's height and cloud cover
   CPoint pSunColor;
   pSunColor.p[0] = pSunColor.p[1] = 1.0;
   pSunColor.p[2] = fCloudCover * 1.0 + (1-fCloudCover) * 0.75;
         // BUGFIX - Make sun yellower, from 0.9 to 0.75

   // average between blue and slightly red
   fp f;

   // make the sun go more yellow as it gets lower
   if (m_lSun.pDir.p[2] < .4) {
      f = (m_lSun.pDir.p[2] - .1) / .3;   // so at .1 = 1.0
      f = max(0, f);
      // note: If lods of clouds then no color change
      f = (1 - fCloudCover) * f + fCloudCover;
      pSunColor.p[2] *= f;
   }

   // make sun go redder as it gets lower
   if (m_lSun.pDir.p[2] < .2) {
      f = (m_lSun.pDir.p[2] + 0) / .2;   // so at .1 = 1.0
      f = max(0, f);

      // note: if lots of clouds then no color change
      f = (1 - fCloudCover) * f + fCloudCover;
      pSunColor.p[1] *= f;
   }

   m_lSun.awColor[0] = (WORD) (0xffff * pSunColor.p[0]);
   m_lSun.awColor[1] = (WORD) (0xffff * pSunColor.p[1]);
   m_lSun.awColor[2] = (WORD) (0xffff * pSunColor.p[2]);

   // moon is bluish
   m_lMoon.awColor[0] = 0xe000;
   m_lMoon.awColor[1] = 0xe000;
   m_lMoon.awColor[2] = 0xffff;


   // Set the ambient intensity based on the sun's height (or moon's)
   //fScale = ((1 - fClouds) * .66 + .33; // ambient light decreases
   fScale = max(1 - fCloudsNoCeiling*.66, 0.05);

   // BUGFIX - Make ambient lumens pow(x,.333) instead of pow(x,.5) - so ambient light comes up faster
   // BUGFIX - Make ambient lumens brighter, 1/6 full sun instead of 1/8
   m_lSun.fAmbientLumens = fScale * pow(max(m_lSun.pDir.p[2]*.8 + .20, 0), 1.0 / 3.0) / 6 * CM_LUMENSSUN;
   m_lMoon.fAmbientLumens = fScale * pow(max(m_lMoon.pDir.p[2]*.8 + .20, 0), 1.0 / 3.0) / 6 * CM_LUMENSMOON *
      fMoonPhase;
      // BUGFIX - Ambient light was /8. Changed to /6
      // BUGFIX - Changed back to /8 because was too bright for clouds

   m_lSun.awAmbientColor[0] = m_lSun.awAmbientColor[1] =
      (WORD) ((1-fCloudCover) * (fp)0x8000 + fCloudCover * 0xffff);
            // BUGFIX - Make ambient bluer, from 0xc000 to 0x8000
   m_lSun.awAmbientColor[2] = 0xffff;
   if (m_lSun.pDir.p[2] < .2) {
      // make the sky go reddish
      fp fAmt;
      fAmt = (m_lSun.pDir.p[2] + .4) / .6;   // so at .2 = 1.0
      fAmt = max(0, fAmt);
      fAmt = (1.0 - fCloudCover) * fAmt + fCloudCover;
      m_lSun.awAmbientColor[1] = (WORD) (m_lSun.awAmbientColor[1] * fAmt);
      m_lSun.awAmbientColor[2] = (WORD) (m_lSun.awAmbientColor[2] * fAmt);
   }

   m_lMoon.awAmbientColor[0] = 0xc000 / 2;
   m_lMoon.awAmbientColor[1] = 0xc000 / 2;
   m_lMoon.awAmbientColor[2] = 0xffff;
   m_fMoonSize = max(1, m_fMoonSize);
   m_lSun.fSizeRad = m_lMoon.fSizeRad = SUNMOONSIZE * m_fMoonSize;
   m_lSun.fDist = 150000000.0;
   m_lMoon.fDist = 384000.0;
   m_lSun.fEmitLight = TRUE;
   m_lMoon.fEmitLight = FALSE;


   // calculate the sky color based on the intensity
   WORD awColor[3][3];     // [0=ground,1=mid,2=zenith], [0=red,1=green,2=blue]
   Gamma (RGB(0xc0,0xc0,0xff), awColor[0]);  // sky near ground
   Gamma (RGB(0x70,0x75,0xff), awColor[1]);  // sky near mid
   Gamma (RGB(0x40, 0x40, 0xe0), awColor[2]);  // sky near zenith
   // calc diffuse light color
   fp afDiffuse[3];
   DWORD i,j;

   // make the sky grey as cloud cover increases
   fp fAverage;
   for (i = 0; i < 3; i++) {
      fAverage = 0;
      for (j = 0; j < 3; j++)
         fAverage += awColor[i][j] / 3;

      // as have more clouds, sky near ground darkens
      if (i == 0) for (j = 0; j < 3; j++)
         fAverage *= (1-fClouds)/2 + .5;
      if (i == 1) for (j = 0; j < 3; j++)
         fAverage *= (1-fClouds)/4 + .75;

      for (j = 0; j < 3; j++)
         awColor[i][j] = (WORD)((1-fCloudCover) * (fp)awColor[i][j] + fCloudCover * fAverage);
   }
   for (i = 0; i < 3; i++) {
      afDiffuse[i] = 0;
      afDiffuse[i] += (fp) m_lSun.awAmbientColor[i] * m_lSun.fAmbientLumens / CM_LUMENSSUN;
      afDiffuse[i] += (fp) m_lMoon.awAmbientColor[i] * m_lMoon.fAmbientLumens / CM_LUMENSSUN;
      afDiffuse[i] /= 6000;   // typical diffuse light expected
   }
   for (i = 0; i < 3; i++) for (j = 0; j < 3; j++) {
      f = afDiffuse[j] * (fp)awColor[i][j];
      f = min((fp)0xffff, f);
      awColor[i][j] = (WORD)f;
   }
   if (m_fAutoColors) {
      m_cAtmosphereGround = UnGamma (awColor[0]);
      m_cAtmosphereMid = UnGamma (awColor[1]);
      m_cAtmosphereZenith = UnGamma (awColor[2]);
   }

   // create the moon surface
   m_lMoon.fBrightness = CM_LUMENSMOON * 300;
      // 300 = fudge factor so that if moon were drawn as a sun (as opposed to an aimage)
      // the brightness would come out the same
   m_lMoon.fBrightness *= 4; // BUGFIX - Another fudge factor to make moon brighter
   if (m_lMoon.pSurface) {
      // init
      // NOTE - Not bothering to rotate moon along it's path, but I dont think anyone will notice
      CMatrix mRot;
      mRot.RotationY (0);
      for (i = 0; i < 2; i++) for (j = 0; j < 2; j++)
         m_lMoon.pSurface->m_afTextureMatrix[i][j] = mRot.p[i ? 2 : 0][j ? 2 : 0];

      m_lMoon.pSurface->m_mTextureMatrix.Identity();
      m_lMoon.pSurface->m_cColor = RGB(0xff,0xff,0xff);
      m_lMoon.pSurface->m_dwID = 0;
      m_lMoon.pSurface->m_fUseTextureMap = TRUE;
      m_lMoon.pSurface->m_gTextureCode = CLSID_ImageMoonCode;
      m_lMoon.pSurface->m_gTextureSub = CLSID_ImageMoonSub;
      m_lMoon.pSurface->m_Material.InitFromID (MATERIAL_FLAT);
      m_lMoon.pSurface->m_szScheme[0] = 0;
      m_lMoon.pSurface->m_TextureMods.cTint = RGB(0xff,0xff,0xff);
      m_lMoon.pSurface->m_TextureMods.cTint = RGB(0xff,0xff,0xff);
      m_lMoon.pSurface->m_TextureMods.wBrightness = 0x1000;
      m_lMoon.pSurface->m_TextureMods.wContrast = 0x1000;
      m_lMoon.pSurface->m_TextureMods.wSaturation = 0x1000;
      m_lMoon.pSurface->m_TextureMods.wHue = 0;
   }

}


/**************************************************************************************
CObjectSkydome::LightQuery - Called to see what lights this exhibits.

Standard function for CObjectTemplate
*/
BOOL CObjectSkydome::LightQuery (PCListFixed pl, DWORD dwShow)
{
   BOOL fRet = FALSE;

   if (!dwShow & m_dwRenderShow)
      return fRet;

   DWORD dwStartNum = pl->Num();

   // calculate the sun and moon if not already
   CalcSunAndMoon();

   // fill in
   LIGHTINFO li;
   DWORD i;
   if (m_fShowSun && m_lSun.fLumens) {
      memset (&li, 0, sizeof(li));
      for (i = 0; i < 3; i++)
         li.awColor[2][i] = m_lSun.awColor[i];
      li.afLumens[2] = m_lSun.fLumens;
      li.dwForm = LIFORM_INFINITE;
      li.pDir.Copy (&m_lSun.pDir);
      li.pDir.Scale(-1);
      pl->Add (&li);

      fRet |= TRUE;
   }
   if (m_fShowMoon && m_lMoon.fLumens) {
      memset (&li, 0, sizeof(li));
      for (i = 0; i < 3; i++)
         li.awColor[2][i] = m_lMoon.awColor[i];
      li.afLumens[2] = m_lMoon.fLumens;
      li.dwForm = LIFORM_INFINITE;
      li.pDir.Copy (&m_lMoon.pDir);
      li.pDir.Scale(-1);
      pl->Add (&li);

      fRet |= TRUE;
   }

   // Pass in ambient light for sun and moon
   if (m_fShowSun && m_lSun.fAmbientLumens) {
      memset (&li, 0, sizeof(li));
      for (i = 0; i < 3; i++)
         li.awColor[2][i] = m_lSun.awAmbientColor[i];
      li.afLumens[2] = m_lSun.fAmbientLumens;
      li.dwForm = LIFORM_AMBIENT;
      pl->Add (&li);

      fRet |= TRUE;
   }
   if (m_fShowMoon && m_lMoon.fAmbientLumens) {
      memset (&li, 0, sizeof(li));
      for (i = 0; i < 3; i++)
         li.awColor[2][i] = m_lMoon.awAmbientColor[i];
      li.afLumens[2] = m_lMoon.fAmbientLumens;
      li.dwForm = LIFORM_AMBIENT;
      pl->Add (&li);

      fRet |= TRUE;
   }

   // Light from other planets and sunds
   PSKYDOMELIGHT pSun;
   pSun = (PSKYDOMELIGHT) m_lSuns.Get(0);
   for (i = 0; i < m_lSuns.Num(); i++, pSun++) {
      if (pSun->fLumens && pSun->fVisible) {
         memset (&li, 0, sizeof(li));
         for (i = 0; i < 3; i++)
            li.awColor[2][i] = pSun->awColor[i];
         li.afLumens[2] =pSun->fLumens;
         li.dwForm = LIFORM_INFINITE;
         li.pDir.Copy (&pSun->pDir);
         li.pDir.Scale(-1);
         pl->Add (&li);

         fRet |= TRUE;
      }
      if (pSun->fVisible && pSun->fAmbientLumens) {
         memset (&li, 0, sizeof(li));
         for (i = 0; i < 3; i++)
            li.awColor[2][i] = pSun->awAmbientColor[i];
         li.afLumens[2] = pSun->fAmbientLumens;
         li.dwForm = LIFORM_AMBIENT;
         pl->Add (&li);

         fRet |= TRUE;
      }
   }

   // BUFIX - go through all the lights that added and find the maximum strength
   // light, deleting a light if it's too dim
   PLIGHTINFO pli = (PLIGHTINFO) pl->Get(0);
   fp fMaxLight = 0;
   for (i = dwStartNum; i < pl->Num(); i++) {
      if (pli[i].dwForm != LIFORM_INFINITE)
         continue;

      fMaxLight = max(fMaxLight, pli[i].afLumens[2]);
   }
   fMaxLight /= 1000;   // if a light is less than 1000th the max, then ignore
   for (i = dwStartNum; i < pl->Num(); i++) {
      if (pli[i].dwForm != LIFORM_INFINITE)
         continue;

      if (pli[i].afLumens[2] >= fMaxLight)
         continue;   // dont delete

      // delete this because too dim
      pl->Remove (i);
      pli = (PLIGHTINFO) pl->Get(0);
      i--;  // to offset the i++
   }

   return fRet;
}



/**********************************************************************************
CObjectSkydome::Message -
sends a message to the object. The interpretation of the message depends upon
dwMessage, which is OSM_XXX. If the function understands and handles the
message it returns TRUE, otherwise FALE.
*/
BOOL CObjectSkydome::Message (DWORD dwMessage, PVOID pParam)
{
   switch (dwMessage) {
   case OSM_IGNOREWORLDBOUNDINGBOXGET:
      {
      POSMIGNOREWORLDBOUNDINGBOXGET p= (POSMIGNOREWORLDBOUNDINGBOXGET) pParam;
      p->fIgnoreCompletely = TRUE;
      }
      return TRUE;

   case OSM_NEWLATITUDE:
      // tell object changing so can be redrawn for new season - just in case need snow
      m_pWorld->ObjectAboutToChange (this);
      CalcAll ();
      m_pWorld->ObjectChanged (this);
      return TRUE;

   case OSM_SKYDOME:
      return TRUE;

   }

   return FALSE;
}


/**********************************************************************************
CObjectSkydome::PointExistInDome - Looks through the skydome's m_memPoints for
a point that is close to what looking for.

inputs
   PCPoint     pWant - Point that looking for
returns
   DWORD - Index, or -1 if cant find
*/
DWORD CObjectSkydome::PointExistInDome (PCPoint pWant)
{
   DWORD i;
   PCPoint pa = (PCPoint) m_memPoints.p;
   for (i = 0; i < m_dwNumPoints; i++)
      if (pa->AreClose (pWant))
         return i;

   return -1;
}

/**********************************************************************************
CObjectSkydome::CreateDome - Creates all the polygons necessary for the dome.
*/
void CObjectSkydome::CreateDome (void)
{

   // make sure there's enough space
   DWORD dwSize;
   DWORD dwNum;
   dwNum = SKYDOMECOLUMNS * SKYDOMEROWSEXTRA;   // estimate
   dwSize = dwNum * sizeof(CPoint);
   if (!m_memPoints.Required (dwSize))
      return;
   dwSize = dwNum * sizeof(CPoint);
   if (!m_memNormals.Required (dwSize))
      return;
   dwSize = dwNum * sizeof(TEXTPOINT5);
   if (!m_memText.Required (dwSize))
      return;
   dwSize = dwNum * sizeof(VERTEX);
   if (!m_memVertices.Required (dwSize))
      return;
   dwNum *= 2; // since 2 triangles per
   dwSize = dwNum * (3 * sizeof(DWORD));
   if (!m_memPoly.Required (dwSize))
      return;

   PCPoint paPoints, paNormals;
   PTEXTPOINT5 paText;
   PVERTEX paVertices;
   paPoints = (PCPoint) m_memPoints.p;
   paNormals = (PCPoint) m_memNormals.p;
   paText = (PTEXTPOINT5) m_memText.p;
   paVertices = (PVERTEX) m_memVertices.p;

   // reset vars
   m_dwNumPoints = m_dwNumNormals = m_dwNumText = m_dwNumVertices = m_dwNumPoly = 0;

   // allocate memory for scratch
   DWORD *padwScratch;
   CMem memScratch;
   dwNum = SKYDOMETRI;
   if (!memScratch.Required ((dwNum+1) * (dwNum+1) * sizeof(DWORD)))
      return;
   padwScratch = (DWORD*) memScratch.p;

   // create 8 triangles
   DWORD dwBottom, dwSide;
   for (dwBottom = 0; dwBottom < 2; dwBottom++) {
      for (dwSide = 0; dwSide < 4; dwSide++) {

         // in each triangle, calculate the points...
         DWORD x, y;
         for (x = 0; x <= dwNum; x++) for (y = 0; x+y <= dwNum; y++) {
            fp fLat, fLong;
            if (y < dwNum)
               fLong = ((fp)x / (fp)(dwNum-y) + (fp)dwSide) * PI/2;
            else
               fLong = 0;  // at north/south pole
            fLat = (fp) y / (fp)dwNum * PI/2;

            // if in the southern hemispher then negative latitiude and reverse longitude
            if (dwBottom) {
               if (y >= 2)
                  continue;   // only one bottom row
               fLat *= -1;
               fLong *= -1;
            }

            // calculate the point
            CPoint pLoc, pNorm;
            TEXTPOINT5 tp;
            pLoc.Zero();
            pLoc.p[0] = (tp.hv[0] = sin(fLong)) * cos(fLat);
            pLoc.p[1] = (tp.hv[1] = cos(fLong)) * cos(fLat);
            pLoc.p[2] = sin(fLat);
            pNorm.Copy (&pLoc);
            pLoc.Scale (m_fSkydomeSize);

            // set volumetric texture point
            tp.xyz[0] = pLoc.p[0];
            tp.xyz[1] = pLoc.p[1];
            tp.xyz[2] = pLoc.p[2];

            // scale texture so that is a flattened dome with equal number of pixels
            // for each amount of latitude, sprialing out from .5,.5
            fp fAmt;
            fAmt = (PI/2 - fLat) / SKYDOMELATRANGE * .5;
            tp.hv[0] = tp.hv[0] * fAmt + .5;
            tp.hv[1] = 1.0 - (tp.hv[1] * fAmt + .5); // flip

            // find out which point it's nearest
            DWORD dwNear;
            dwNear = PointExistInDome (&pLoc);
            if (dwNear == -1) {
               // add it
               dwNear = m_dwNumPoints;

               // write into
               paPoints[dwNear].Copy (&pLoc);
               paNormals[dwNear].Copy (&pNorm);
               paText[dwNear] = tp;
               paVertices[dwNear].dwColor = 0;
               paVertices[dwNear].dwNormal = paVertices[dwNear].dwPoint = paVertices[dwNear].dwTexture = dwNear;
               m_dwNumPoints++;
               m_dwNumNormals++;
               m_dwNumText++;
               m_dwNumVertices++;
            }

            // remember this
            padwScratch[x + (y * (dwNum+1))] = dwNear;

         }  // x and y


         // now, create all the triangles
         DWORD *padwVert;
         dwSize = 3 * sizeof(DWORD);
         for (x = 0; x < dwNum; x++) for (y = 0; x+y < dwNum; y++) {
            // if on the bottom then only allow one row
            if (dwBottom && y)
               continue;

            // first one
            padwVert = (DWORD*)((PBYTE) m_memPoly.p + m_dwNumPoly * dwSize);
            padwVert[0] = padwScratch[(x+1) + (y * (dwNum+1))];
            padwVert[1] = padwScratch[x + (y * (dwNum+1))];
            padwVert[2] = padwScratch[x + ((y+1) * (dwNum+1))];
            m_dwNumPoly++;

            // might be another side
            if (x+y+2 > dwNum)
               continue;
            padwVert = (DWORD*)((PBYTE) m_memPoly.p + m_dwNumPoly * dwSize);
            padwVert[0] = padwScratch[(x+1) + (y * (dwNum+1))];
            padwVert[1] = padwScratch[x + ((y+1) * (dwNum+1))];
            padwVert[2] = padwScratch[(x+1) + ((y+1) * (dwNum+1))];
            m_dwNumPoly++;

         }  // over x and y
      }  // over dwSide
   }  // over dwBottom

   // done
}


/**************************************************************************************
CObjectSkydome::SkyRes - Returns the resolution of the skydome.

returns
   DWORD - Resolution
*/
DWORD CObjectSkydome::SkyRes (void)
{
   // BUGFIX - Skydome resolution is affected by the texture detail
   int iShift = (int) m_dwResolution + TextureDetailGet(m_OSINFO.dwRenderShard);
   DWORD dwRes = 500;
   if (iShift > 0)
      dwRes <<= iShift;
   else if (iShift < 0)
      dwRes >>= (-iShift);

   return dwRes;
}

/**************************************************************************************
CObjectSkydome::EscMultiThreadedCallback - Callback that does mutlithreaded downsampling
of textures
*/

#define DENSITYATMID       .3
#define TRANSPDENSITY    300     // how dense can be and still get transparency
#define TRANSMISSIONDEN  (TRANSPDENSITY*2)     // how dense can be and still get light going through

#define CLOUDMAXDENSE   0.05     // if density greater than this dont bother
#define SKIPPIX      1     // BUGFIX - Was 2, reduced so looks better

void CObjectSkydome::EscMultiThreadedCallback (PVOID pParams, DWORD dwParamSize,
      DWORD dwThread)
{
   PMTSKYDOME pmt = (PMTSKYDOME) pParams;

   if (pmt->dwPass == 4) { // cumulus
      PSKYPIXEL ps = pmt->ps;

      DWORD x,y;
      PSKYPIXEL psCur;
      TEXTUREPOINT tpPixel;
      CPoint pDir, pDirOrig;
      CNoise2D Noise;
      srand(m_dwCumulusSeed);
      Noise.Init (100, 100);

   //#define SKIPCUMPIX      2 //(2 << m_dwResolution)
   //#define CUMSPHERE       (SKIPCUMPIX * 4)
   //#define SKIPCUMPIX      2
   //#define CUMSPHERE       ((SKIPCUMPIX * 2) << m_dwResolution)
   #define SKIPCUMPIX      1     // BUGFIX - Was 2
      int iShift = (int) m_dwResolution + TextureDetailGet(m_OSINFO.dwRenderShard) + 2;
   #define CUMSPHERE       (SKIPCUMPIX << iShift)  // BUGFIX *4 since increase m_dwResolution
   //#define CUMSPHERE       ((SKIPCUMPIX*4) << m_dwResolution)  // BUGFIX *4 since increase m_dwResolution

      m_fCumulusScale = max(m_fCumulusScale, 1);
      fp fSkyCircle;
      DWORD dwSkyRes = SkyRes ();
      fSkyCircle = (PI / 2) / SKYDOMELATRANGE * (dwSkyRes/2);
      fSkyCircle *= fSkyCircle;
      fp afRand[4];

      // sheet offset
      CPoint pSheer, pSheerTemp;
      pSheer.Zero();
      pSheer.p[0] = sin(m_fCumulusSheerAngle);
      pSheer.p[1] = cos(m_fCumulusSheerAngle);
      pSheer.Scale (m_fCumulusSheer);
      pSheer.p[2] = 1;

      // BUGFIX - dont draw cumulus if less then 10 degrees above skyline
      fp fMinZ = sin(5.0 / 180.0 * PI);


      for (y = 0; y < dwSkyRes; y += SKIPCUMPIX) {
         psCur = ps + (y * dwSkyRes);

         for (x = 0; x < dwSkyRes; x += SKIPCUMPIX, psCur += SKIPCUMPIX) {
            // if beyond the edge of the sky sphere then ignore
            tpPixel.h = (fp)x - (fp)dwSkyRes/2;
            tpPixel.v = (fp)y - (fp)dwSkyRes/2;
            if ((tpPixel.h * tpPixel.h + tpPixel.v * tpPixel.v) > fSkyCircle)
               continue;

            // precalc some randoms so animates properly
            afRand[0] = MyRand (.8, 1.4);
            afRand[1] = MyRand (.8, 1.4);
            afRand[2] = MyRand (.8, 1.4);
            afRand[3] = MyRand (.8, 1.4);

            // find out the location
            tpPixel.h = x + MyRand (-SKIPCUMPIX/2,SKIPCUMPIX); // put some wobble in
            tpPixel.v = y + MyRand (-SKIPCUMPIX/2,SKIPCUMPIX);
            PixelToDirection (&tpPixel, &pDir);
            if (pDir.p[2] < fMinZ)
               continue;   // below the level we want to calc

            // scale up so it's at the specified elevation
            pDir.Scale (m_pCumulusLoc.p[2] / pDir.p[2]);

            // translate
            pDirOrig.Copy (&pDir);
            pDir.p[0] -= m_pCumulusLoc.p[0];  // do minus so looks like going in that direction
            pDir.p[1] -= m_pCumulusLoc.p[1];

            // sum up all the points
            fp fCurScale, fCurDetail, fSum, fSumDetails;
            DWORD dwUse;
            fCurScale = m_fCumulusScale * 10; // 10 is a fudge factor
            fCurDetail = 1;
            fSum = fSumDetails = 0;
            // use .71 because not going to repeat sequence too quickly
            for (dwUse = 0; (dwUse < 5) && (fCurDetail > .05); dwUse++, fCurDetail *= m_fCumulusDetail, fCurScale *= .5) {
               fSum += fCurDetail * Noise.Value (pDir.p[0] / fCurScale, pDir.p[1] / fCurScale);
               fSumDetails += fCurDetail;
            }
            if (!fSumDetails)
               continue;   // nothing actually written so no data

            // because adding noise increases scope somewhat, but not ccompletely, do a sqrt
            fSumDetails = sqrt(fSumDetails);

            fSum = fSum / (2 * fSumDetails) + .5;
            fSum -= (1 - m_fCumulusCover);

   #if 0 // test checkerboard
            fSum = 0;
            fp f1, f2;
            f1 = myfmod(pDir.p[0] / m_fCumulusScale, 1);
            f2 = myfmod(pDir.p[1] / m_fCumulusScale, 1);
            if (f2 > .5)
               f1 = 1 - f1;
            if (f1 > .5)
               fSum = 1;
   #endif
            if (fSum <= 0)
               continue;   // not in the cover

            // scale by thickness
            fp fSumLow, fSumOrig;
            fSumOrig = fSum;
            fSumLow = fSumOrig * 100;   // lower level varies by 0 to 100m
            fSum = fSumOrig * m_fCumulusThickness * 10000;   // BUGFIX - Make less dense
            fSumOrig = sqrt(min(1, fSumOrig * 3)); // so smaller speheres on edge of cloud

            // keep track of pixel that was in
            tpPixel.h = (fp)x;
            tpPixel.v = (fp)y;

            // draw lower spehere
            pDir.Copy (&pDirOrig);
            pDir.p[2] -= fSumLow;
            DrawSphere (&pDir, fSumOrig * afRand[0] * CUMSPHERE, &tpPixel, ps);

            // draw mid
            pDir.Copy (&pDirOrig);
            pSheerTemp.Copy (&pSheer);
            pSheerTemp.Scale (fSum * 0.33);
            pDir.Add (&pSheerTemp);
            DrawSphere (&pDir, fSumOrig * afRand[1] * CUMSPHERE, &tpPixel, ps);

            // draw close to top
            pDir.Copy (&pDirOrig);
            pSheerTemp.Copy (&pSheer);
            pSheerTemp.Scale (fSum * 0.66);
            pDir.Add (&pSheerTemp);
            DrawSphere (&pDir, fSumOrig * afRand[2] * CUMSPHERE, &tpPixel, ps);

            // Draw upper level
            pDir.Copy (&pDirOrig);
            pSheerTemp.Copy (&pSheer);
            pSheerTemp.Scale (fSum);
            pDir.Add (&pSheerTemp);
            DrawSphere (&pDir, fSumOrig * afRand[3] * CUMSPHERE, &tpPixel, ps);
         } // x
      } // y
   }
   else if (pmt->dwPass == 3) { // calccloudcirrus 2
      PSKYPIXEL ps = pmt->ps;

      // go back over and interpolate
      DWORD x,y;
      PSKYPIXEL psCur;
      psCur = ps;
      DWORD xx, yy, j;
      PSKYPIXEL pUL, pUR, pLL, pLR;
      SKYPIXEL sTop, sBottom;
      fp f;

      DWORD dwSkyRes = SkyRes();
      for (y = 0; y < dwSkyRes; y ++) {
         yy = y - (y % SKIPPIX);
         for (x = 0; x < dwSkyRes; x ++, psCur++) {
            xx = x - (x % SKIPPIX);

            // if already filled in value don't bother
            if (!(x % SKIPPIX) && !(y % SKIPPIX))
               continue;

            // if to right or bottom exceeds edge then exit
            if ((xx + SKIPPIX >= dwSkyRes) || (yy + SKIPPIX >= dwSkyRes))
               continue;

            // interpolate if have values all around
            pUL = ps + (xx + yy * dwSkyRes);
            pUR = ps + (xx + SKIPPIX + yy * dwSkyRes);
            pLR = ps + (xx + SKIPPIX + (yy + SKIPPIX) * dwSkyRes);
            pLL = ps + (xx + (yy + SKIPPIX) * dwSkyRes);

            if ((pUL->fCloudZ == ZINFINITE) || (pUR->fCloudZ == ZINFINITE) ||
               (pLR->fCloudZ == ZINFINITE) || (pLL->fCloudZ == ZINFINITE))
               continue;

            // if all the densities are 0 then dont bother
            if ((pUL->fCloudDensity == 0) && (pUR->fCloudDensity == 0) &&
               (pLR->fCloudDensity == 0) && (pLL->fCloudDensity == 0))
               continue;


            // else, average
            f = (fp)(x - xx) / (fp) SKIPPIX;

            // top
            sTop.fCloudDensity = (1 - f) * pUL->fCloudDensity + f * pUR->fCloudDensity;
            sTop.fCloudZ = (1 - f) * pUL->fCloudZ + f * pUR->fCloudZ;
            for (j = 0; j < 3; j++)
               sTop.afCloudPixel[j] = (1 - f) * pUL->afCloudPixel[j] + f * pUR->afCloudPixel[j];

            // bottom
            sBottom.fCloudDensity = (1 - f) * pLL->fCloudDensity + f * pLR->fCloudDensity;
            sBottom.fCloudZ = (1 - f) * pLL->fCloudZ + f * pLR->fCloudZ;
            for (j = 0; j < 3; j++)
               sBottom.afCloudPixel[j] = (1 - f) * pLL->afCloudPixel[j] + f * pLR->afCloudPixel[j];

            // combined
            f = (fp)(y - yy) / (fp) SKIPPIX;
            psCur->fCloudDensity = (1 - f) * sTop.fCloudDensity + f * sBottom.fCloudDensity;
            psCur->fCloudZ = (1 - f) * sTop.fCloudZ + f * sBottom.fCloudZ;
            for (j = 0; j < 3; j++)
               psCur->afCloudPixel[j] = (1 - f) * sTop.afCloudPixel[j] + f * sBottom.afCloudPixel[j];
         } // x
      } // y
   }
   else if (pmt->dwPass == 2) { // calccloudcirrius 1
      PSKYPIXEL ps = pmt->ps;
      DWORD dwLayer = pmt->dwLayer;
      fp fSkyCircle = pmt->fSkyCircle;

      DWORD x,y;
      PSKYPIXEL psCur;
      TEXTUREPOINT tpPixel;
      CPoint pDir;
      CNoise2D Noise;
      srand(m_adwCirrusSeed[dwLayer]);
      Noise.Init (100, 100);


      DWORD dwSkyRes = SkyRes();
      for (y = pmt->dwStart; y < pmt->dwStop; y += SKIPPIX) {
         psCur = ps + (y * dwSkyRes);

         for (x = 0; x < dwSkyRes; x += SKIPPIX, psCur += SKIPPIX) {
            // if beyond the edge of the sky sphere then ignore
            tpPixel.h = (fp)x - (fp)dwSkyRes/2;
            tpPixel.v = (fp)y - (fp)dwSkyRes/2;
            if ((tpPixel.h * tpPixel.h + tpPixel.v * tpPixel.v) > fSkyCircle)
               continue;

            // find out the location
            tpPixel.h = x;
            tpPixel.v = y;
            PixelToDirection (&tpPixel, &pDir);
            if (pDir.p[2] < CLOSE)
               continue;   // below the ground

            // scale up so it's at the specified elevation
            pDir.Scale (m_apCirrusLoc[dwLayer].p[2] / pDir.p[2]);

            // store away points anyway so that can do interpolation for pixels
            // that have skipped
            psCur->afCloudPixel[0] = pDir.p[0];
            psCur->afCloudPixel[1] = pDir.p[1];
            psCur->afCloudPixel[2] = pDir.p[2];
            psCur->fCloudZ = psCur->afCloudPixel[2];  // NOTE: Not technically correct but good enough

            // translate
            pDir.p[0] -= m_apCirrusLoc[dwLayer].p[0];  // do minus so looks like going in that direction
            pDir.p[1] -= m_apCirrusLoc[dwLayer].p[1];

            // sum up all the points
            fp fCurScale, fCurDetail, fSum, fSumDetails;
            DWORD dwUse;
            fCurScale = m_afCirrusScale[dwLayer] * (dwLayer ? 30 : 100); // 10 is a fudge factor
            fCurDetail = 1;
            fSum = fSumDetails = 0;
            // use .71 because not going to repeat sequence too quickly
            for (dwUse = 0; (dwUse < 5) && (fCurDetail > .05); dwUse++, fCurDetail *= m_afCirrusDetail[dwLayer], fCurScale *= .5) {
               fSum += fCurDetail * Noise.Value (pDir.p[0] / fCurScale, pDir.p[1] / fCurScale);
               fSumDetails += fCurDetail;
            }
            if (!fSumDetails)
               continue;   // nothing actually written so no data

            // because adding noise increases scope somewhat, but not ccompletely, do a sqrt
            fSumDetails = sqrt(fSumDetails);

            fSum = fSum / (2 * fSumDetails) + .5;
            fSum -= (1 - m_afCirrusCover[dwLayer]);

   #if 0 // test checkerboard
            fSum = 0;
            fp f1, f2;
            f1 = myfmod(pDir.p[0] / m_afCirrusScale[dwLayer], 1);
            f2 = myfmod(pDir.p[1] / m_afCirrusScale[dwLayer], 1);
            if (f2 > .5)
               f1 = 1 - f1;
            if (f1 > .5)
               fSum = 1;
   #endif
            if (fSum <= 0)
               continue;   // not in the cover

            // scale by thickness
            fSum = fSum * m_afCirrusThickness[dwLayer] * (dwLayer ? 2000 : 1000);   // BUGFIX - Make less dense

            // store away
            psCur->fCloudDensity = fSum;
            psCur->afCloudPixel[2] = pDir.p[2] - fSum / 2;
            psCur->fCloudZ = psCur->afCloudPixel[2];  // NOTE: Not technically correct but good enough
         } // x
      } // y
   }
   else if (pmt->dwPass == 0) {   // fillAtmosphere
      // loop
      DWORD x,y;
      fp fDist, fx, fy, f;
      DWORD dwSkyRes = SkyRes();
      PSKYPIXEL ps = pmt->ps + (pmt->dwStart * dwSkyRes);
      fp fRadHorizon = pmt->fRadHorizon;
      CPoint pGround, pMid, pZenith;
      pGround.Copy (&pmt->pGround);
      pMid.Copy (&pmt->pMid);
      pZenith.Copy (&pmt->pZenith);

      for (y = pmt->dwStart; y < pmt->dwStop; y++) for (x = 0; x < dwSkyRes; x++, ps++) {
         // what's the distance
         fx = (fp) x - (fp) dwSkyRes/2;
         fy = (fp) y - (fp) dwSkyRes/2;
         fDist = sqrt (fx * fx + fy * fy);

         // calulcate the density
         ps->fDensity = ZINFINITE;
         if (fDist < fRadHorizon) {
            f = cos (fDist / fRadHorizon * PI / 2);
            if (f > CLOSE) {
               f = 1.0 / f;
               ps->fDensity = DENSITYATZENITH * f;
               // BUGFIX - Take out min, so have better idea of density
               //if (ps->fDensity > DENSITYATGROUND)
               //   ps->fDensity = DENSITYATGROUND;
            }
         }

         if (ps->fDensity >= DENSITYATGROUND) {
            // all ground color
            ps->fRed = pGround.p[0];
            ps->fGreen = pGround.p[1];
            ps->fBlue = pGround.p[2];
         }
         else if (ps->fDensity <= DENSITYATZENITH) {
            ps->fRed = pZenith.p[0];
            ps->fGreen = pZenith.p[1];
            ps->fBlue = pZenith.p[2];
         }
         else if (ps->fDensity <= DENSITYATMID) {
            // interpolate between mid and zenith
            f = (ps->fDensity - DENSITYATZENITH) / (DENSITYATMID - DENSITYATZENITH);
            ps->fRed = (1.0 - f) * pZenith.p[0] + f * pMid.p[0];
            ps->fGreen = (1.0 - f) * pZenith.p[1] + f * pMid.p[1];
            ps->fBlue = (1.0 - f) * pZenith.p[2] + f * pMid.p[2];
         }
         else {   // between mid and ground
            f = min(ps->fDensity, DENSITYATGROUND);
            f = (f - DENSITYATMID) / (DENSITYATGROUND - DENSITYATMID);

            // else, interpolate between mid and ground
            ps->fRed = (1.0 - f) * pMid.p[0] + f * pGround.p[0];
            ps->fGreen = (1.0 - f) * pMid.p[1] + f * pGround.p[1];
            ps->fBlue = (1.0 - f) * pMid.p[2] + f * pGround.p[2];
         }

         ps->afOrig[0] = ps->fRed;
         ps->afOrig[1] = ps->fGreen;
         ps->afOrig[2] = ps->fBlue;
      } // y
   } // pass 0
   
   else if (pmt->dwPass == 1) { // clound render
      PSKYPIXEL ps = pmt->ps;
      DWORD dwXMin = pmt->dwXMin;
      DWORD dwXMax = pmt->dwXMax;
      float afAmbient[3];
      memcpy (afAmbient, pmt->afAmbient, sizeof(afAmbient));
      DWORD dwNumSun = pmt->dwNumSun;
      PSKYDOMELIGHT pSunList = pmt->pSunList;
      COLORREF cColor = pmt->cColor;
      fp fDensityScale = pmt->fDensityScale;

      // falloff is exp(k * distance) = .99 and TRANSPDENSITY or TRANSMISSIONDENS
      fp fKTransp, fKTransm;
      fKTransp = log(.01) / TRANSPDENSITY;
      fKTransm = log(.01) / TRANSMISSIONDEN;

      // color
      WORD  awColor[3];
      Gamma (cColor, awColor);

      DWORD i, j;

      DWORD x,y;
      PSKYPIXEL psCur;
      float afColor[3];
      fp f;
      DWORD dwSkyRes = SkyRes();
      for (y = pmt->dwStart; y < pmt->dwStop; y++) {
         psCur = ps + (y * dwSkyRes + dwXMin);
         for (x = dwXMin; x < dwXMax; x++, psCur++) {
            if ((psCur->fCloudZ == ZINFINITE) || (psCur->fCloudDensity <= 0))
               continue;

            // start with ambient
            for (j = 0; j < 3; j++)
               afColor[j] = afAmbient[j];

            // to transmission
            for (i = 0; i < dwNumSun; i++) {
               if (!pSunList[i].fLumens)
                  continue;
               if (pSunList[i].pDir.p[2] < CLOSE)
                  continue;   // light on wrong side

               f = psCur->fCloudDensity / pSunList[i].pDir.p[2];
               if (f >= TRANSMISSIONDEN*2)
                  continue;   // cloud too thick

               // else, some light gets through
               f = exp(fKTransm * f);
               //f = (1 - f / (fp) TRANSMISSIONDEN);
               f *= pSunList[i].fLumens / CM_LUMENSSUN;
               f /= (fp)0x10000;
               for (j = 0; j < 3; j++)
                  afColor[j] += f * (fp) awColor[j] * (fp) pSunList[i].awColor[j];
            }

            // do reflection

            // figure out the normal
            PSKYPIXEL pL, pR, pU, pD;
            CPoint pLR, pUD, pN;
            pL = pR = pU = pD = psCur;
            if (x && (psCur[-1].fCloudZ != ZINFINITE))
               pL = &psCur[-1];
            if ((x+1 < dwSkyRes) && (psCur[1].fCloudZ != ZINFINITE))
               pR = &psCur[1];
            if (y && (psCur[-(int)dwSkyRes].fCloudZ != ZINFINITE))
               pU = &psCur[-(int)dwSkyRes];
            if ((y+1 < dwSkyRes) && (psCur[dwSkyRes].fCloudZ != ZINFINITE))
               pD = &psCur[dwSkyRes];
            if ((pL != pR) && (pU != pD)) {
               for (j = 0; j < 3; j++) {
                  pLR.p[j] = pR->afCloudPixel[j] - pL->afCloudPixel[j];
                  pUD.p[j] = pU->afCloudPixel[j] - pD->afCloudPixel[j];
               }
               pN.CrossProd (&pUD, &pLR);
               pN.Normalize();
            }
            else {
               pN.Zero();
               pN.p[2] = -1;
            }
            for (i = 0; i < dwNumSun; i++) {
               if (!pSunList[i].fLumens)
                  continue;
               f = pN.DotProd (&pSunList[i].pDir);

               // because clouds are translucent, allow light to bend around
               f = f * .7 + .3;
               if (f <= 0)
                  continue;   // no light

               // how bright?
               f *= pSunList[i].fLumens / CM_LUMENSSUN;
               f /= (fp)0x10000;
               for (j = 0; j < 3; j++)
                  afColor[j] += f * (fp) awColor[j] * (fp) pSunList[i].awColor[j];
            }



            // apply the color - dont assume infinitely far, so it's an averaging process
            f = min(1,psCur->fDensity *fDensityScale);  // assume half density since not all the way up
            f = sqrt(f);
            for (j = 0; j < 3; j++) {
               afColor[j] =
                  afColor[j] * (1.0 - f) + // amount of light that gets throygh
                  psCur->afOrig[j] * f;   // from background
            }

            // set it

            // do transparency
            f = 1 - exp(fKTransp * psCur->fCloudDensity);
            //f = psCur->fCloudDensity / TRANSPDENSITY;
            f = min(f,1);
            f = 1 - f;

            if (f >= 1) {
               // do nothing
               continue;
            }
            else if (f) {
               psCur->fRed = f * psCur->fRed + (1 - f) * afColor[0];
               psCur->fGreen = f * psCur->fGreen + (1 - f) * afColor[1];
               psCur->fBlue = f * psCur->fBlue + (1 - f) * afColor[2];
            }
            else {
               psCur->fRed = afColor[0];
               psCur->fGreen = afColor[1];
               psCur->fBlue = afColor[2];
            }
         } // x
      } // y
   }
}

/**********************************************************************************
CObjectSkydome::FillAtmosphere - Fills the sky with the atmosphere.

inputs
   PSKYPIXEL         ps - To fill in. Assume is SKYPIXEL x SKYPIXEL in size
*/
void CObjectSkydome::FillAtmosphere (PSKYPIXEL ps)
{
   // middle angle
   fp fAtmosphereMidAngle = asin(min(1, DENSITYATZENITH / DENSITYATMID));
   // figure out distances in pixels
   fp fRadius, fMid, fRadMinusMid, fRadHorizon;
   DWORD dwSkyRes = SkyRes();
   fRadius = (fp)dwSkyRes / 2.0;
   fMid = (PI/2 - fAtmosphereMidAngle) / SKYDOMELATRANGE * fRadius;
   fRadMinusMid = fRadius - fMid;
   fRadHorizon = PI/2 / SKYDOMELATRANGE * fRadius;

   // calculate the color
   CPoint pGround, pMid, pZenith;
   pGround.p[0] = Gamma(GetRValue(m_cAtmosphereGround));
   pGround.p[1] = Gamma(GetGValue(m_cAtmosphereGround));
   pGround.p[2] = Gamma(GetBValue(m_cAtmosphereGround));
   pMid.p[0] = Gamma(GetRValue(m_cAtmosphereMid));
   pMid.p[1] = Gamma(GetGValue(m_cAtmosphereMid));
   pMid.p[2] = Gamma(GetBValue(m_cAtmosphereMid));
   pZenith.p[0] = Gamma(GetRValue(m_cAtmosphereZenith));
   pZenith.p[1] = Gamma(GetGValue(m_cAtmosphereZenith));
   pZenith.p[2] = Gamma(GetBValue(m_cAtmosphereZenith));

   // BUGFIX - multithreaded
   MTSKYDOME mt;
   memset (&mt, 0, sizeof(mt));
   mt.dwPass = 0;
   mt.ps = ps;
   mt.fRadHorizon = fRadHorizon;
   mt.pGround.Copy (&pGround);
   mt.pMid.Copy (&pMid);
   mt.pZenith.Copy (&pZenith);
   ThreadLoop (0, dwSkyRes, 1, &mt, sizeof(mt));
}


/**********************************************************************************
CObjectSkydome::CalcTexutureHash - Calculates the hash for this texture.

returns
   double - Hash value
*/
double CObjectSkydome::CalcTextureHash (void)
{
   double fHash = (double) SkyRes();

   fHash += 10000 * (double) m_fYear;
   fHash += 100 * (double) m_fTimeInYear;
   fHash += 10 * (double) m_fTimeInDay;

   fHash += 1043 * (double) m_fMoonSize;
   fHash += 4321 * (double) m_fShowSun;
   fHash += 1433 * (double) m_fShowSun;
   fHash += 8943 * (double) m_fDimClouds;

   fHash += 4895 * (double) m_dwResolution;

   fHash += 2345 * (double) m_fHaze;
   fHash += 4993 * (double) m_fDensityZenith;
   fHash += 4352 * (double) m_fSkydomeSize;
   fHash += 5431 * (double) m_cAtmosphereGround;
   fHash += 2345 * (double) m_cAtmosphereMid;
   fHash += 4539 * (double) m_cAtmosphereZenith;
   fHash += 0234 * (double) m_fAutoColors;
   fHash += 2345 * (double) m_dwStarsNum;

   DWORD i, j;
   for (i = 0; i < CLOUDLAYERS; i++) {
      fHash += 2342 * (double)(i+1) * (double)m_afCirrusDraw[i];
      fHash += 9351 * (double)(i+1) * (double)m_acCirrusColor[i];
      fHash += 2348 * (double)(i+1) * (double)m_afCirrusScale[i];
      fHash += 4589 * (double)(i+1) * (double)m_afCirrusDetail[i];
      fHash += 1234 * (double)(i+1) * (double)m_afCirrusCover[i];
      fHash += 5319 * (double)(i+1) * (double)m_afCirrusThickness[i];
      for (j = 0; j < 3; j++)
         fHash += 6403 * (double)(i+1) * (double)(j+1) * (double)m_apCirrusLoc[i].p[j];
      fHash += 1254 * (double)(i+1) * (double)m_adwCirrusSeed[i];
   } // i


   fHash += 2345 * (double) m_fCumulusDraw;
   fHash += 2345 * (double) m_cCumulusColor;
   fHash += 2345 * (double) m_fCumulusScale;
   fHash += 2345 * (double) m_fCumulusDetail;
   fHash += 2345 * (double) m_fCumulusCover;
   fHash += 2345 * (double) m_fCumulusThickness;
   fHash += 2345 * (double) m_fCumulusSheer;
   fHash += 2345 * (double) m_fCumulusSheerAngle;
   for (j = 0; j < 3; j++)
      fHash += 2345 * (double)(j+1) * (double) m_pCumulusLoc.p[j];
   fHash += 2345 * (double) m_dwCumulusSeed;


   // add other suns and planets
   PSKYDOMELIGHT pSun;
   pSun = (PSKYDOMELIGHT) m_lSuns.Get(0);
   for (i = 0; i < m_lSuns.Num(); i++, pSun++) {
      fHash += 1254 * (double)(i+1) * (double)pSun->fVisible;
      if (!pSun->fVisible)
         continue;

      for (j = 0; j < 3; j++) {
         fHash += 2341 * (double)(i+1)* (double)(j+1) * (double)pSun->pDir.p[j];
         fHash += 1235 * (double)(i+1)* (double)(j+1) * (double)pSun->awColor[j];
         fHash += 4528 * (double)(i+1)* (double)(j+1) * (double)pSun->awAmbientColor[j];
         fHash += 8549 * (double)(i+1)* (double)(j+1) * (double)pSun->awSolarColor[j];
      }

      fHash += 9483 * (double)(i+1) * (double)pSun->fLumens;
      fHash += 6783 * (double)(i+1) * (double)pSun->fAmbientLumens;
      fHash += 5938 * (double)(i+1) * (double)pSun->fSizeRad;
      fHash += 3485 * (double)(i+1) * (double)pSun->fDist;
      fHash += 6984 * (double)(i+1) * (double)pSun->fEmitLight;
      fHash += 5849 * (double)(i+1) * (double)pSun->fBrightness;
      // NOTE: ignoring pSurface
   }

   return fHash;
}


/**********************************************************************************
CObjectSkydome::CalcTextureGUID - From the hash, determine the GUID to use
for the texture.

inputs
   GUID        *pgID - Filled with the texture GUID
*/
void CObjectSkydome::CalcTextureGUID (GUID *pgID)
{
   // get the hash
   double fHash = CalcTextureHash();

   gaiCachSkydomeTime[m_OSINFO.dwRenderShard]++;

   DWORD i;
   for (i = 0; i < MAXSKYDOMECACHE; i++)
      if (gafCacheHash[m_OSINFO.dwRenderShard][i] == fHash) {
         gaiCacheUsed[m_OSINFO.dwRenderShard][i] = gaiCachSkydomeTime[m_OSINFO.dwRenderShard];
         *pgID = gagCacheGUID[m_OSINFO.dwRenderShard][i];
         return;
      }

   // else, find oldest one
   DWORD dwOldest = 0;
   for (i = 1; i < MAXSKYDOMECACHE; i++)  // intentionally start from 1
      if (gaiCacheUsed[m_OSINFO.dwRenderShard][i] < gaiCacheUsed[m_OSINFO.dwRenderShard][dwOldest])
         dwOldest = i;

   // delete this
   if (gaiCacheUsed[m_OSINFO.dwRenderShard][dwOldest])
      TextureCacheDelete (m_OSINFO.dwRenderShard, &gagCacheGUID[m_OSINFO.dwRenderShard][dwOldest], &gagCacheGUID[m_OSINFO.dwRenderShard][dwOldest]);
   else
      GUIDGen (&gagCacheGUID[m_OSINFO.dwRenderShard][dwOldest]);
   gafCacheHash[m_OSINFO.dwRenderShard][dwOldest] = fHash;
   gaiCacheUsed[m_OSINFO.dwRenderShard][dwOldest] = gaiCachSkydomeTime[m_OSINFO.dwRenderShard];
   *pgID = gagCacheGUID[m_OSINFO.dwRenderShard][dwOldest];
}



/**********************************************************************************
CObjectSkydome::MakeSureTextureExists - Makes sure can get the texture for the
sky. If not, it creates it
*/
static int _cdecl SKYDOMELIGHTCompare (const void *elem1, const void *elem2)
{
   SKYDOMELIGHT *pdw1, *pdw2;
   pdw1 = (SKYDOMELIGHT*) elem1;
   pdw2 = (SKYDOMELIGHT*) elem2;

   if (pdw1->fDist > pdw2->fDist)
      return -1;
   else if (pdw1->fDist < pdw2->fDist)
      return 1;
   else
      return 0;
}

void CObjectSkydome::MakeSureTextureExists (void)
{
   GUID gTexture;
   CalcTextureGUID (&gTexture);

   if (TextureCacheGetDynamic (m_OSINFO.dwRenderShard, &gTexture, &gTexture, NULL, NULL))
      return;

   // allocate enough space for the skydome
   CMem  memSky, memPassIn;
   DWORD dwSkyRes = SkyRes();
   if (!memSky.Required (dwSkyRes * dwSkyRes * sizeof(SKYPIXEL)))
      return;
   if (!memPassIn.Required (dwSkyRes * dwSkyRes * sizeof(float) * 3))
      return;
   PSKYPIXEL ps;
   float *pafPassIn;
   ps = (PSKYPIXEL) memSky.p;
   pafPassIn = (float*) memPassIn.p;

   // zero it out
   memset (ps, 0, dwSkyRes * dwSkyRes * sizeof(SKYPIXEL));

   // temporary memory with all suns and planets
   PSKYDOMELIGHT psd;
   CListFixed lSun;
   lSun.Init (sizeof(SKYDOMELIGHT));
   CalcSunAndMoon ();
   if (m_fShowSun)
      lSun.Add (&m_lSun);
   if (m_fShowMoon)
      lSun.Add (&m_lMoon);

   // add other suns and planets
   PSKYDOMELIGHT pSun;
   DWORD dwNum, i;
   pSun = (PSKYDOMELIGHT) m_lSuns.Get(0);
   for (i = 0; i < m_lSuns.Num(); i++, pSun++) {
      if (!pSun->fVisible)
         continue;
      lSun.Add (pSun);
   }

   psd = (PSKYDOMELIGHT) lSun.Get(0);
   dwNum = lSun.Num();

   // sort so furthest away are first
   qsort (psd, dwNum, sizeof(SKYDOMELIGHT), SKYDOMELIGHTCompare);

   // calculate location of all bodies
   for (i = 0; i < dwNum; i++)
      CalcSunOrPlanet (&psd[i]);

   // Atmosphere
   FillAtmosphere (ps);

   // draw all the suns and moons
   fp fBrightest;
   fBrightest = 0;
   for (i = 0; i < dwNum; i++) {
      fBrightest = max(fBrightest, psd[i].fLumens);
   }

   // Draw haze around bodies
   for (i = 0; i < dwNum; i++)
      HazeAroundSunOrPlanet (&psd[i], ps, fBrightest);

   // transfer the current color to the original color, since the haze affects
   // the color of the atmophere
   PSKYPIXEL psCur;
   for (i = 0, psCur = ps; i < dwSkyRes * dwSkyRes; i++, psCur++) {
      psCur->afOrig[0] = psCur->fRed;
      psCur->afOrig[1] = psCur->fGreen;
      psCur->afOrig[2] = psCur->fBlue;
   }

   // Draw stars
   DrawStars (ps);

   // draw the planets
   for (i = 0; i < dwNum; i++) {
      DrawSunOrPlanet (&psd[i], ps, psd, dwNum);
   }

   // Upper clouds
   for (i = 0; i < CLOUDLAYERS; i++)
      DrawCirrus (i, ps, psd, dwNum);

   // Lower clouds
   DrawCumulus (ps, psd, dwNum);

   // create new texture


   // fill in pass in
   float *paf;
   ps = (PSKYPIXEL) memSky.p;
   for (paf = pafPassIn, i = 0; i < dwSkyRes * dwSkyRes; i++, ps++) {
      *(paf++) = ps->fRed;
      *(paf++) = ps->fGreen;
      *(paf++) = ps->fBlue;
   }

   PCTextureMap pMap;
   pMap = new CTextureMap;
   if (!pMap)
      return;
   CMaterial Mat;
   TEXTINFO TI;
   Mat.InitFromID (MATERIAL_FLAT);
   memset (&TI, 0, sizeof(TI));
   TI.dwMap = 0;
   TI.fFloor = TRUE;
   TI.fPixelLen = .1;   // arbitrary since wont ever use

   // initialize
   pMap->m_fDontDownsample = TRUE;
   pMap->Init (&gTexture, &gTexture, dwSkyRes, dwSkyRes, &Mat, &TI, NULL,
      NULL, NULL, NULL, NULL, (float*) pafPassIn);

   // add it to the cache
   TextureCacheAddDynamic (m_OSINFO.dwRenderShard, pMap);

   // done
}

/**********************************************************************************
CObjectSkydome::DrawStars - Draw the stars.

inputs
   PSKYPIXEL         ps - dwSkyRes x dwSkyRes pixels to fill in
*/
void CObjectSkydome::DrawStars (PSKYPIXEL ps)
{
   // if no stars dont bother
   if (!m_dwStarsNum)
      return;

   SRand(0);

   // loop
   DWORD dwX, dwY;
   float afColor[3];
   fp fIntensity;
   PSKYPIXEL psCur;
   DWORD i;
   DWORD dwSkyRes = SkyRes();
   for (i = 0; i < m_dwStarsNum; i++) {
      dwX = rand() % dwSkyRes;
      dwY = rand() % dwSkyRes;
      psCur = ps + (dwX + dwY * dwSkyRes);


      afColor[0] = MyRand (.5, 1); // BUGFIX - Make starts less colorful, was 0 to 1. BUGFIX - Take out sqrt() to make more colorful
      afColor[1] = MyRand (.5, 1);
      afColor[2] = MyRand (.5, 1);

      // intensity
      fIntensity = MyRand (.1, 10) * CM_LUMENSMOON / CM_LUMENSSUN * (fp)0x10000;
      fIntensity /= 4.0; // BUGFIX - Make stars less bright

      // BUGFIX - Increase intensity as increase resolution so is accurate according
      // to light diffusion
      fIntensity *= pow(dwSkyRes / 500.0,2);
      
      afColor[0] *= fIntensity;
      afColor[1] *= fIntensity;
      afColor[2] *= fIntensity;

      ApplyAtmophereToPixel ((int)dwX, (int)dwY, afColor, ps);

      psCur->fRed = afColor[0];
      psCur->fGreen = afColor[1];
      psCur->fBlue = afColor[2];
   }
}

/**********************************************************************************
CObjectSkydome::DirectionToAltAz - Converts from a direction vector pointing at the
light source to an altituude (latitude) and azimth (longitude) value.

inputs
   PCPoint        pDir - Direction
   PTEXTUREPOINT  ptAltAz - .h = azimth, .v = altitude in radians
returns
   none
*/
void CObjectSkydome::DirectionToAltAz (PCPoint pDir, PTEXTUREPOINT ptAltAz)
{
   fp fLen = sqrt(pDir->p[0] * pDir->p[0] + pDir->p[1] * pDir->p[1]);
   ptAltAz->h = atan2(pDir->p[0], pDir->p[1]);
   ptAltAz->v = atan2(pDir->p[2], fLen);
}


/**********************************************************************************
CObjectSkydome::AltAzToDirection - Converts from an altituude (latitude) and azimth (longitude) value,
to a direction bectori

inputs
   PTEXTUREPOINT  ptAltAz - .h = azimth, .v = altitude in radians
   PCPoint        pDir - Direction
returns
   none
*/
void CObjectSkydome::AltAzToDirection (PTEXTUREPOINT ptAltAz, PCPoint pDir)
{
   pDir->p[0] = sin(ptAltAz->h) * cos (ptAltAz->v);
   pDir->p[1] = cos(ptAltAz->h) * cos(ptAltAz->v);
   pDir->p[2] = sin(ptAltAz->v);
}

/**********************************************************************************
CObjectSkydome::AltAzToPixel - Converts from an altituude (latitude) and azimth (longitude) value,
to a pixel in the skydome.

inputs
   PTEXTUREPOINT  ptAltAz - .h = azimth, .v = altitude in radians
   PTEXTUREPOINT  ptPixel - X and Y pixel
   BOOL  fLimit - if TRUE then limit to the bitmap size
returns
   none
*/
void CObjectSkydome::AltAzToPixel (PTEXTUREPOINT ptAltAz, PTEXTUREPOINT ptPixel, BOOL fLimit)
{
   DWORD dwSkyRes = SkyRes();
   ptPixel->h = ptPixel->v = (PI / 2 - ptAltAz->v) / SKYDOMELATRANGE / 2;
   ptPixel->h *= sin(ptAltAz->h);
   ptPixel->v *= -cos(ptAltAz->h);  // BUGFIX - Since image flipped
   ptPixel->h += .5;
   ptPixel->v += .5;
   ptPixel->h *= dwSkyRes;
   ptPixel->v *= dwSkyRes;

   if (fLimit) {
      ptPixel->h = max(0,ptPixel->h);
      ptPixel->h = min(dwSkyRes-1, ptPixel->h);
      ptPixel->v = max(0,ptPixel->v);
      ptPixel->v = min(dwSkyRes-1, ptPixel->v);
   }
}

/**********************************************************************************
CObjectSkydome::PixelToAltAz - Converts from an altituude (latitude) and azimth (longitude) value,
to a pixel in the skydome.

inputs
   PTEXTUREPOINT  ptPixel - X and Y pixel
   PTEXTUREPOINT  ptAltAz - .h = azimth, .v = altitude in radians
returns
   none
*/
void CObjectSkydome::PixelToAltAz (PTEXTUREPOINT ptPixel, PTEXTUREPOINT ptAltAz)
{
   DWORD dwSkyRes = SkyRes();
   TEXTUREPOINT tp = *ptPixel;
   tp.h /= (fp)dwSkyRes;
   tp.v /= (fp)dwSkyRes;
   tp.h -= .5;
   tp.v = .5 - tp.v;  // BUGFIX - Since y flipped
   tp.h = tp.h * 2 * SKYDOMELATRANGE;
   tp.v = tp.v * 2 * SKYDOMELATRANGE;
   ptAltAz->v = PI / 2 - sqrt(tp.h * tp.h + tp.v * tp.v);
   ptAltAz->h = atan2(tp.h, tp.v);
}

/**********************************************************************************
CObjectSkydome::PixelToDirection - Converts from a pixel to a direction.

inputs
   PTEXTUREPOINT  ptPixel - X and Y pixel
   *CPoint         pDir - Filled with direction
returns
   none
*/
void CObjectSkydome::PixelToDirection (PTEXTUREPOINT ptPixel, PCPoint pDir)
{
   DWORD dwSkyRes = SkyRes();
   TEXTUREPOINT tp = *ptPixel;
   tp.h /= (fp)dwSkyRes;
   tp.v /= (fp)dwSkyRes;
   tp.h -= .5;
   tp.v = .5 - tp.v;  // BUGFIX - Since y flipped
   tp.h = tp.h * 2 * SKYDOMELATRANGE;
   tp.v = tp.v * 2 * SKYDOMELATRANGE;

   fp fLen;
   fLen = sqrt(tp.h * tp.h + tp.v * tp.v);

   fp fSin;
   if (fLen >= CLOSE) {
      fSin = sin(fLen);
      pDir->p[0] = tp.h / fLen * fSin;
      pDir->p[1] = tp.v / fLen * fSin;
   }
   else
      pDir->p[0] = pDir->p[1] = 0;
   pDir->p[2] = cos(fLen);
}

/**********************************************************************************
CObjectSkydome::DirectionToPixel - Converts from a direction to a pixel.

inputs
   PCPoint         pDir - Direction
   PTEXTUREPOINT  ptPixel - Filled with X and Y pixel
returns
   none
*/
void CObjectSkydome::DirectionToPixel (PCPoint pDir, PTEXTUREPOINT ptPixel)
{
   // normalize
   CPoint pN;
   pN.Copy (pDir);
   pN.Normalize();

   // find the length in x and y
   fp fLen;
   fLen = sqrt(pN.p[0] * pN.p[0] + pN.p[1] * pN.p[1]);
   DWORD dwSkyRes = SkyRes();
   if (fLen < CLOSE) {
      // right above
      ptPixel->h = ptPixel->v = (fp)dwSkyRes/2;
      return;
   }

   // else, fill in
   fp fAngle;
   ptPixel->h = pN.p[0] / fLen;
   ptPixel->v = -pN.p[1] / fLen; // BUGFIX - since y flipped
   fAngle = atan2(pN.p[2], fLen);
   fAngle = PI/2 - fAngle;
   fAngle = fAngle / SKYDOMELATRANGE * (fp)dwSkyRes/2.0;
   ptPixel->h = ptPixel->h * fAngle + (fp)dwSkyRes/2.0;
   ptPixel->v = ptPixel->v * fAngle + (fp)dwSkyRes/2.0;
}

/**********************************************************************************
CObjectSkydome::ApplyAtmphereToPixel - Given a pixel (array of 3 floats) with
a given brightness as it would appear in space (with no atmophere), this
applies the atmophere to it to take into account addition of light and/or removal.

inputs
   int         iX, iY - Pixel as it appears on the skydome bitmap
   float       *pafPixel - Initially filled with value. Will be modified with new value.
   PSKYPIXEL   ps - Sky pixel data - Should have already gone through FillAtmophere()
returns
   none
*/
void CObjectSkydome::ApplyAtmophereToPixel (int iX, int iY, float *pafPixel, PSKYPIXEL ps)
{
   DWORD dwSkyRes = SkyRes();
   PSKYPIXEL pUse = ps + (iX + iY * dwSkyRes);
   DWORD j;
   for (j = 0; j < 3; j++) {
      pafPixel[j] =
         pafPixel[j] * (1.0 - min(1,pUse->fDensity)) + // amount of light that gets throygh
         pUse->afOrig[j];   // from background
   }
}


/**********************************************************************************
CObjectSkydome::CalcSunOrPlanet - Calculate the sun or planet's location.
Fills in some calcualted variabels

inputs
   PSKYDOMELIGHT  pSun - Pointer to the sun of planet object
returns
   none
*/
void CObjectSkydome::CalcSunOrPlanet (PSKYDOMELIGHT pSun)
{
   CPoint pToSun;
   pToSun.Copy (&pSun->pDir);
   pToSun.Normalize();

   // figure out the intensity of the light
   pSun->fIntensity = pSun->fLumens / CM_LUMENSMOON / 4;
      // moon, as seen in full sunlight, is light gray (1/16 intensity)
      // BUGFIX - Moon was too dark during daylight. Changed from /16 to /4
   pSun->fIntensity *= SUNMOONSIZE * SUNMOONSIZE / (pSun->fSizeRad * pSun->fSizeRad);
      // smaller images will have to emit more intensity per pixel

   // figure a place for the sun/planet in the world
   pSun->fSizeRad = min(PI * .99, pSun->fSizeRad);
   pSun->fRadius =  tan (pSun->fSizeRad / 2) * pSun->fDist;
   pSun->pLoc.Copy (&pToSun);
   pSun->pLoc.Scale (pSun->fDist);

   // altitude and azimth
   DirectionToAltAz (&pToSun, &pSun->tpAltAz);
   AltAzToPixel (&pSun->tpAltAz, &pSun->tpPixel);
}

/**********************************************************************************
CObjectSkydome::DrawSunOrPlanet - Draws a sun or planet. Call this after the
starts and atmophere have been drawn. Should sort so start drawn first.

inputs
   PSKYDOMELIGHT     pSun - Pointer to the sun or planet object
   PSKYPIXEL         ps - To fill in. Assume is SKYPIXEL x SKYPIXEL in size
   PSKYDOMELIGHT     pSunList - List of all the suns in the area - so can see which light emitted from what
   DWORD             dwNumSun - Number of elements in pSunList
returns
   none
*/
void CObjectSkydome::DrawSunOrPlanet (PSKYDOMELIGHT pSun, PSKYPIXEL ps,
                                      PSKYDOMELIGHT pSunList, DWORD dwNumSun)
{
   // then the color of the light if
   float afSun[3];
   DWORD i;
   for (i = 0; i < 3; i++)
      afSun[i] = (fp)pSun->awColor[i] * pSun->fIntensity;

   // figure out a bounding box
   DWORD dwSkyRes = SkyRes();
   TEXTUREPOINT tpAltAz;
   TEXTUREPOINT tpPixel;
   int iXMin, iXMax, iYMin, iYMax;
   fp fSizePixels;
   tpAltAz = pSun->tpAltAz;
   tpPixel = pSun->tpPixel;
   fSizePixels = pSun->fSizeRad / (SKYDOMELATRANGE*2) * dwSkyRes;
   fSizePixels *= 1.5;  // just a bit larger just in case
   iXMin = (int) max(0, tpPixel.h - fSizePixels/2 * (PI/2));
   iXMax = (int) min(dwSkyRes-1, tpPixel.h + fSizePixels/2 * (PI/2));
   iYMin = (int) max(0, tpPixel.v - fSizePixels/2);
   iYMax = (int) min(dwSkyRes-1, tpPixel.v + fSizePixels/2);

   // if there's a surface then use it to get the texture
   PCTextureMapSocket pText;
   COLORREF cSurfaceColor; // if have pSun->pSurface
   RENDERSURFACE rs;
   pText = NULL;
   if (pSun->pSurface) {
      // if there's a scheme get that
      PCObjectSurface pUse = pSun->pSurface;
      PCSurfaceSchemeSocket pss;
      if (pUse->m_szScheme[0] && m_pWorld && m_pWorld->SurfaceSchemeGet()) {
         pss = m_pWorld->SurfaceSchemeGet();
         pUse = pss->SurfaceGet (pUse->m_szScheme, pUse, FALSE);
         if (!pUse)
            pUse = pSun->pSurface;
      }

      cSurfaceColor = pUse->m_cColor;

      if (pUse->m_fUseTextureMap) {
         memset (&rs, 0, sizeof(rs));
         memcpy (rs.afTextureMatrix, pUse->m_afTextureMatrix, sizeof(rs.afTextureMatrix));
         memcpy (rs.abTextureMatrix, &pUse->m_mTextureMatrix, sizeof(pUse->m_mTextureMatrix));
         rs.fUseTextureMap = pUse->m_fUseTextureMap;
         rs.gTextureCode = pUse->m_gTextureCode;
         rs.gTextureSub = pUse->m_gTextureSub;
         memcpy (&rs.Material, &pUse->m_Material, sizeof(rs.Material));
         wcscpy (rs.szScheme, pUse->m_szScheme);
         rs.TextureMods = pUse->m_TextureMods;

         pText = TextureCacheGet (m_OSINFO.dwRenderShard, &rs, NULL, NULL);
      }

      // if got this from surfaceget then it's a clone, so deltete
      if (pUse != pSun->pSurface)
         delete pUse;
   }

   // calculate a matrix that converts from sun coords into a texture
   TEXTUREPOINT tpUp;
   CMatrix mTextToWorld, mWorldToText;//, mScale;
   CPoint pBack, pUp, pRight;
   PixelToDirection (&pSun->tpPixel, &pBack);
   tpUp = pSun->tpPixel;
   tpUp.v -= 1;
   PixelToDirection (&tpUp, &pUp);
   pUp.Subtract (&pBack);
   pUp.Normalize();
   pBack.Normalize();
   pRight.CrossProd (&pBack, &pUp);
   pRight.Normalize();
   pBack.CrossProd (&pUp, &pRight); // just to make sure
   mTextToWorld.RotationFromVectors (&pRight, &pBack, &pUp);
   // mScale.Scale (pSun->fRadius*2, pSun->fRadius*2, pSun->fRadius*2);
   //mTextToWorld.MultiplyRight (&mScale);
   //mScale.Translation (pSun->pLoc.p[0], pSun->pLoc.p[1], pSun->pLoc.p[2]);
   //mTextToWorld.MultiplyRight (&mScale);

   // Need to take into account rotation set in the texture rotation
   if (pText) {
      double fLenR, fLenU;
      fLenR = sqrt(rs.afTextureMatrix[0][0] * rs.afTextureMatrix[0][0] +
         rs.afTextureMatrix[1][0] * rs.afTextureMatrix[1][0]);
      if (fLenR > EPSILON)
         fLenR = 1.0 / fLenR;
      fLenU = sqrt(rs.afTextureMatrix[0][1] * rs.afTextureMatrix[0][1] +
         rs.afTextureMatrix[1][1] * rs.afTextureMatrix[1][1]);
      if (fLenU > EPSILON)
         fLenU = 1.0 / fLenU;

      rs.afTextureMatrix[0][0] *= fLenR;
      rs.afTextureMatrix[1][0] *= fLenR;
      rs.afTextureMatrix[0][1] *= fLenU;
      rs.afTextureMatrix[1][1] *= fLenU;

      CMatrix mRot;
      mRot.Identity();
      DWORD j;
      for (i = 0; i < 2; i++) for (j = 0; j < 2; j++)
         mRot.p[i ? 2 : 0][j ? 2 : 0] = rs.afTextureMatrix[i][j];

      mTextToWorld.MultiplyLeft (&mRot);
   }

   // invert it
   mTextToWorld.Invert (&mWorldToText);


   // loop over the bounding box and see if point intersects with the sun/planet
   int x,y, xx, yy;
   fp ft1, ft2;
   CPoint pZero, pDir;
   pZero.Zero();
   DWORD dwRet;
   DWORD j;
   PSKYPIXEL psTemp;
   float afAnti[3], afTemp[3];
   BOOL fIntersect;
   for (y = iYMin; y <= iYMax; y++) for (x = iXMin; x <= iXMax; x++) {
      // antialias
      afAnti[0] = afAnti[1] = afAnti[2] = 0;
      fIntersect = FALSE;

      for (xx = -1; xx <= 1; xx++) for (yy = -1; yy <= 1; yy++) {
         // convert the pixel to altitude and azimuth
         tpPixel.h = (fp)x + (fp) xx * .333;
         tpPixel.v = (fp)y + (fp) yy * .333;
         PixelToAltAz (&tpPixel, &tpAltAz);

         // convert this to a direction
         AltAzToDirection (&tpAltAz, &pDir);
         pDir.Normalize();

         // see if this intersects with sphere
         dwRet = IntersectRaySpehere (&pZero, &pDir, &pSun->pLoc, pSun->fRadius, &ft1, &ft2);
         if (!dwRet)
            continue;
         fIntersect = TRUE;

         // where it intersects
         CPoint pInter;
         pInter.Copy (&pDir);
         pInter.Scale (ft1);
         // could add pZero, but no point

         if (pSun->pSurface) {
            // have specified some sort of surface color
            WORD awColor[3];
            float afGlow[3];
            afGlow[0] = afGlow[1] = afGlow[2] = 0;
            if (pText) {
               TEXTPOINT5 tpLoc, tpSize;
               CPoint pInterText;

               // NOTE - Assuming that original image of planet is square

               // figure out the size
               tpSize.hv[0] = tpSize.hv[1] = 1.0 / fSizePixels / 3.0;
                  // BUGFIX - 3.0 to include anti-aliasing
               tpSize.xyz[0] = tpSize.xyz[1] = tpSize.xyz[2] = 0; // since doing 2d image only


               // calculate where moon should go
               pInterText.Copy (&pInter);
               pInterText.Subtract (&pSun->pLoc);
               pInterText.p[3] = 1;
               pInterText.MultiplyLeft (&mWorldToText);
               pInterText.Scale (1.0 / (pSun->fRadius * 2));
               tpLoc.hv[0] = pInterText.p[0] + .5;
               tpLoc.hv[1] = .5 - pInterText.p[2];
               tpLoc.hv[0] = max(0, tpLoc.hv[0]);
               tpLoc.hv[0] = min(1, tpLoc.hv[0]);
               tpLoc.hv[1] = max(0, tpLoc.hv[1]);
               tpLoc.hv[1] = min(1, tpLoc.hv[1]);
               tpLoc.xyz[0] = tpLoc.xyz[1] = tpLoc.xyz[2] = 0; // since will be image of planet

               CMaterial Material;
               pText->FillPixel (0, TMFP_ALL, awColor, &tpLoc, &tpSize, &Material, afGlow, FALSE);
                  // NOTE: Passing in fHighQuality = FALSE
            }
            else
               // no texture, so use color
               Gamma (cSurfaceColor, awColor);

            // scale by number of lumens
            for (j = 0; j < 3; j++) {
               afTemp[j] = awColor[j];
               afTemp[j] *= pSun->fBrightness / CM_LUMENSSUN;
               afTemp[j] += afGlow[j];
            }
         }
         else {
            // if sun just solid color
            for (j = 0; j < 3; j++)
               afTemp[j] = afSun[j];
         }

         // Calculate if in shadow
         if (!pSun->fEmitLight) {
            // normal at point
            CPoint pNorm, pMoonToSun;
            fp f;
            pNorm.Subtract (&pInter, &pSun->pLoc);
            pNorm.Normalize();

            // loop through all the other suns
            fp afIllum[3], fIllumMax;
            afIllum[0] = afIllum[1] = afIllum[2] = fIllumMax = 0;
            for (i = 0; i < dwNumSun; i++) {
               // eliminate matches, or other lights
               if ((&pSunList[i] == pSun) || !pSunList[i].fEmitLight)
                  continue;

               // NOTE: Not actually calculating the amount of light from each
               // sun, just seeing if is in shadow

               // how much light?
               fIllumMax += pSunList[i].fSolarLumens;
               pMoonToSun.Subtract (&pSunList[i].pLoc, &pInter);
               pMoonToSun.Normalize();
               f = pMoonToSun.DotProd (&pNorm);
               if (f <= 0) // in shadow
                  continue;

               // else lit
               f = sqrt(f);   // just to simulate moon better
               afIllum[0] += f * pSunList[i].fSolarLumens * (fp)pSunList[i].awSolarColor[0] / (fp)0x10000;
               afIllum[1] += f * pSunList[i].fSolarLumens * (fp)pSunList[i].awSolarColor[1] / (fp)0x10000;
               afIllum[2] += f * pSunList[i].fSolarLumens * (fp)pSunList[i].awSolarColor[2] / (fp)0x10000;
            } // i

            // is it in darkness?
            if (!fIllumMax)
               continue;

            // else, scale
            for (j = 0; j < 3; j++)
               afTemp[j] *= afIllum[j] / fIllumMax;
         }

         // add it in
         for (j = 0; j < 3; j++)
            afAnti[j] += afTemp[j];
      }

      // if not intersect ignore
      if (!fIntersect)
         continue;

      // BUGFIX - Need to check if nothing because then need to draw black so
      // dont see stars behind
      // if nothing then dont bother
      //if (!afAnti[0] && !afAnti[1] && !afAnti[2])
      //   continue;

      // scale sown
      for (j = 0; j < 3; j++)
         afAnti[j] /= 9;

      // figure out atmopheric attenuation
      ApplyAtmophereToPixel (x, y, afAnti, ps);

      // draw pixel
      psTemp = ps + (x + y * dwSkyRes);
      psTemp->fRed = afAnti[0];
      psTemp->fGreen = afAnti[1];
      psTemp->fBlue = afAnti[2];
   }

   if (pText)
      TextureCacheRelease (m_OSINFO.dwRenderShard, pText);
}


/**********************************************************************************
CObjectSkydome::HazeAroundSunOrPlanet - Draws the haze around a sun or planet.
Call this after the planets have been drawn.

inputs
   PSKYDOMELIGHT     pSun - Pointer to the sun or planet object
   PSKYPIXEL         ps - To fill in. Assume is SKYPIXEL x SKYPIXEL in size
   fp                fBrightest - Number of lumens in the brightest. Dim objects can have smaller
returns
   none
*/
#define DENSITYAFFECTHAZE(x)     (x)  // BUGFIX - set was pow(x,2)

void CObjectSkydome::HazeAroundSunOrPlanet (PSKYDOMELIGHT pSun, PSKYPIXEL ps, fp fBrightest)
{
   // BUGFIX - Test for brightest of lumens to make sure no crash
   if (!m_fHaze || (fBrightest < EPSILON) || (pSun->fLumens < CLOSE))
      return;

   // then the color of the light if
   float afSun[3];
   DWORD i;
   for (i = 0; i < 3; i++) {
      afSun[i] = (fp)pSun->awColor[i] * pSun->fLumens / CM_LUMENSSUN;
      //afSun[i] /= m_fHaze;
      afSun[i] *= 3 / DENSITYAFFECTHAZE(DENSITYATZENITH);   // fudge factor so is about 1/10th the strength of the light
   }

   // equation is exp( K * (dist*dist) / (haze*haze))

   // how large to test for the haze
   DWORD dwSkyRes = SkyRes();
   int iHazeSize;
   fp fK;
   iHazeSize = (int) (dwSkyRes / 5 * m_fHaze);
   if (pSun->pDir.p[2] < .4) {
      // enlarge the haze area as get towards sunset
      fp fEnlarge = (.4 - pSun->pDir.p[2]) * 2;
      fEnlarge = min(1, fEnlarge);
      iHazeSize += iHazeSize * fEnlarge;
   }
   iHazeSize = (int) ((fp)iHazeSize * pow((fp)(pSun->fLumens / fBrightest), (fp).5));
   iHazeSize = min((int)dwSkyRes, iHazeSize);
   if (!iHazeSize)
      return;
   fK = -1.0 / (pow((fp)dwSkyRes * (fp)m_fHaze, (fp).25));
   fK *= 8; //15   // fudge factor

   // figure out intensity at end and always subtract this
   fp fAtEdge, f;
   fAtEdge = exp(fK * pow(iHazeSize * 2.0 / PI, .25)) * DENSITYAFFECTHAZE(DENSITYATGROUND);

   // figure out a bounding box
   TEXTUREPOINT tpAltAz;
   TEXTUREPOINT tpPixel;
   int iXMin, iXMax, iYMin, iYMax;
   tpAltAz = pSun->tpAltAz;
   tpPixel = pSun->tpPixel;
   iXMin = (int) max(0, tpPixel.h - iHazeSize);
   iXMax = (int) min(dwSkyRes-1, tpPixel.h + iHazeSize);
   iYMin = (int) max(0, tpPixel.v - iHazeSize);
   iYMax = (int) min(dwSkyRes-1, tpPixel.v + iHazeSize);

   // location of point of light
   CPoint pCenter, pCur;
   TEXTUREPOINT ptAltAz;
   ptAltAz = pSun->tpAltAz;
   AltAzToDirection (&ptAltAz, &pCenter);

   // loop over the bounding box and calculate hace
   int x,y;
   PSKYPIXEL psCur;
   for (y = iYMin; y <= iYMax; y++) for (x = iXMin; x <= iXMax; x++) {
      // current pixel...
      psCur = ps + (x + y * dwSkyRes);

      // convert this to location in space
      ptAltAz.h = x;
      ptAltAz.v = y;
      PixelToDirection (&ptAltAz, &pCur);
      
      // distance
      pCur.Subtract (&pCenter);
      f = pCur.Length();   // in units
      f = (f / sqrt((fp)2)) * PI/2;  // in radians distance
      f = (f / SKYDOMELATRANGE) * (dwSkyRes / 2); // convert to pixels
      f = exp(fK * pow(f, (fp).25));
      f -= fAtEdge;
      if (f <= 0)
         continue;
      f *= DENSITYAFFECTHAZE(min(DENSITYATGROUND,psCur->fDensity));

      //if (psCur->fRed > 1000000)
      //   psCur->fRed++;

      // else, add haze
      psCur->fRed += afSun[0] * f;
      psCur->fGreen += afSun[1] * f;
      psCur->fBlue += afSun[2] * f;
   }
}




/****************************************************************************
SkydomePage
*/
BOOL SkydomePage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCObjectSkydome pv = (PCObjectSkydome)pPage->m_pUserData;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         DFDATE dwDate;
         DFTIME dwTime;
         TimeInYearDayToDFDATETIME (pv->m_fTimeInDay, pv->m_fTimeInYear, (DWORD)pv->m_fYear,
            &dwTime, &dwDate);

         DateControlSet (pPage, L"sundate", dwDate);
         TimeControlSet (pPage, L"suntime", dwTime);
      }
      break;

   case ESCN_DATECHANGE:
      {
         PESCNDATECHANGE p = (PESCNDATECHANGE) pParam;
         if (!p->pControl || !p->pControl->m_pszName || _wcsicmp(p->pControl->m_pszName, L"sundate"))
            break;
         
         DFDATE dwDate;
         DFTIME dwTime;
         TimeInYearDayToDFDATETIME (pv->m_fTimeInDay, pv->m_fTimeInYear, (DWORD)pv->m_fYear,
            &dwTime, &dwDate);

         DFDATE dw;
         dw = DateControlGet (pPage, p->pControl->m_pszName);
         if (dw != dwDate) {
            pv->m_pWorld->ObjectAboutToChange (pv);
            pv->m_fYear = YEARFROMDFDATE (dw);
            pv->m_fTimeInYear = DFDATEToTimeInYear (dw);
            pv->CalcAll ();
            pv->m_pWorld->ObjectChanged (pv);

            // set the world date
            WCHAR szTemp[32];
            swprintf (szTemp, L"%d", (int) dw);
            pv->m_pWorld->VariableSet (WSDate(), szTemp);
         }
      }
      break;   // default

   case ESCN_TIMECHANGE:
      {
         PESCNTIMECHANGE p = (PESCNTIMECHANGE) pParam;
         if (!p->pControl || !p->pControl->m_pszName || _wcsicmp(p->pControl->m_pszName, L"suntime"))
            break;
         
         DFDATE dwDate;
         DFTIME dwTime;
         TimeInYearDayToDFDATETIME (pv->m_fTimeInDay, pv->m_fTimeInYear, (DWORD)pv->m_fYear,
            &dwTime, &dwDate);

         DFTIME dw;
         dw = TimeControlGet (pPage, p->pControl->m_pszName);
         if (dw != dwTime) {
            pv->m_pWorld->ObjectAboutToChange (pv);
            pv->m_fTimeInDay = DFTIMEToTimeInDay (dw);
            pv->CalcAll ();
            pv->m_pWorld->ObjectChanged (pv);

            // set the world time
            WCHAR szTemp[32];
            swprintf (szTemp, L"%d", (int) dw);
            pv->m_pWorld->VariableSet (WSTime(), szTemp);
         }

      }
      break;   // default

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Skydome settings";
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}




/****************************************************************************
SkydomeSunPage
*/
BOOL SkydomeSunPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCObjectSkydome pv = (PCObjectSkydome)pPage->m_pUserData;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         // sun/moon size
         PCEscControl pControl;
         pControl = pPage->ControlFind (L"moonsize");
         if (pControl)
            pControl->AttribSetInt (Pos(), (int)(pv->m_fMoonSize * 10));

         // buttons
         pControl = pPage->ControlFind (L"showsun");
         if (pControl)
            pControl->AttribSetBOOL (Checked(), pv->m_fShowSun);
         pControl = pPage->ControlFind (L"showmoon");
         if (pControl)
            pControl->AttribSetBOOL (Checked(), pv->m_fShowMoon);
         pControl = pPage->ControlFind (L"dimclouds");
         if (pControl)
            pControl->AttribSetBOOL (Checked(), pv->m_fDimClouds);

      }
      break;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;
         PWSTR psz;
         psz = p->pControl->m_pszName;

         if (!_wcsicmp(psz, L"showsun")) {
            pv->m_pWorld->ObjectAboutToChange (pv);
            pv->m_fShowSun = p->pControl->AttribGetBOOL (Checked());
            pv->CalcAll ();
            pv->m_pWorld->ObjectChanged (pv);
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"showmoon")) {
            pv->m_pWorld->ObjectAboutToChange (pv);
            pv->m_fShowMoon = p->pControl->AttribGetBOOL (Checked());
            pv->CalcAll ();
            pv->m_pWorld->ObjectChanged (pv);
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"dimclouds")) {
            pv->m_pWorld->ObjectAboutToChange (pv);
            pv->m_fDimClouds = p->pControl->AttribGetBOOL (Checked());
            pv->CalcAll ();
            pv->m_pWorld->ObjectChanged (pv);
            return TRUE;
         }
      }
      break;


   case ESCN_SCROLL:
   // take out because too slow - case ESCN_SCROLLING:
      {
         PESCNSCROLL p = (PESCNSCROLL) pParam;

         // only do one scroll bar
         if (!p->pControl->m_pszName)
            break;

         // set value
         fp fVal;
         if (!_wcsicmp(p->pControl->m_pszName, L"moonsize")) {
            fVal = p->pControl->AttribGetInt (Pos()) / 10.0;
            if (fVal != pv->m_fMoonSize) {
               pv->m_pWorld->ObjectAboutToChange (pv);
               pv->m_fMoonSize = fVal;
               pv->CalcAll ();
               pv->m_pWorld->ObjectChanged (pv);
            }
            return TRUE;
         }
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Sun and moon";
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}

typedef struct {
   PCObjectSkydome      pSky;    // object
   DWORD                dwLayer; // layer to use
} CIRRUSPAGE, *PCIRRUSPAGE;

/****************************************************************************
SkydomeCirrusPage
*/
BOOL SkydomeCirrusPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCIRRUSPAGE pcp = (PCIRRUSPAGE)pPage->m_pUserData;
   PCObjectSkydome pv = pcp->pSky;
   DWORD dwLayer = pcp->dwLayer;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         PCEscControl pControl;

         pControl = pPage->ControlFind (L"draw");
         if (pControl)
            pControl->AttribSetBOOL (Checked(), pv->m_afCirrusDraw[dwLayer]);


         // sliders
         pControl = pPage->ControlFind (L"detail");
         if (pControl)
            pControl->AttribSetInt (Pos(), (int)(pv->m_afCirrusDetail[dwLayer] * 100));
         pControl = pPage->ControlFind (L"cover");
         if (pControl)
            pControl->AttribSetInt (Pos(), (int)(pv->m_afCirrusCover[dwLayer] * 100));
         pControl = pPage->ControlFind (L"scale");
         if (pControl)
            pControl->AttribSetInt (Pos(), (int)(pv->m_afCirrusScale[dwLayer] / 100));
         pControl = pPage->ControlFind (L"thick");
         if (pControl)
            pControl->AttribSetInt (Pos(), (int)(pv->m_afCirrusThickness[dwLayer] * 100));

         // numbers
         MeasureToString (pPage, L"loc0", pv->m_apCirrusLoc[dwLayer].p[0]);
         MeasureToString (pPage, L"loc1", pv->m_apCirrusLoc[dwLayer].p[1]);
         MeasureToString (pPage, L"loc2", pv->m_apCirrusLoc[dwLayer].p[2]);
         DoubleToControl (pPage, L"seed", pv->m_adwCirrusSeed[dwLayer]);

         // color
         FillStatusColor (pPage, L"cirruscolor", pv->m_acCirrusColor[dwLayer]);
      }
      break;

   case ESCN_EDITCHANGE:
      {
         PESCNEDITCHANGE p = (PESCNEDITCHANGE) pParam;

         pv->m_pWorld->ObjectAboutToChange (pv);
         MeasureParseString (pPage, L"loc0", &pv->m_apCirrusLoc[dwLayer].p[0]);
         MeasureParseString (pPage, L"loc1", &pv->m_apCirrusLoc[dwLayer].p[1]);
         MeasureParseString (pPage, L"loc2", &pv->m_apCirrusLoc[dwLayer].p[2]);
         pv->m_adwCirrusSeed[dwLayer] = (DWORD) DoubleFromControl (pPage, L"seed");
         pv->m_apCirrusLoc[dwLayer].p[2] = max(pv->m_apCirrusLoc[dwLayer].p[2], 1);
         pv->CalcAll ();
         pv->m_pWorld->ObjectChanged (pv);
      }
      break;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;
         PWSTR psz;
         psz = p->pControl->m_pszName;

         if (!_wcsicmp(psz, L"draw")) {
            pv->m_pWorld->ObjectAboutToChange (pv);
            pv->m_afCirrusDraw[dwLayer] = p->pControl->AttribGetBOOL (Checked());
            pv->CalcAll ();
            pv->m_pWorld->ObjectChanged (pv);
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"changecirrus")) {
            COLORREF cr;
            cr = AskColor (pPage->m_pWindow->m_hWnd, pv->m_acCirrusColor[dwLayer], pPage, L"cirruscolor");
            if (cr != pv->m_acCirrusColor[dwLayer]) {
               pv->m_pWorld->ObjectAboutToChange (pv);
               pv->m_acCirrusColor[dwLayer] = cr;
               pv->CalcAll ();
               pv->m_pWorld->ObjectChanged (pv);
            }
            return TRUE;
         }
      }
      break;



   case ESCN_SCROLL:
   // take out because too slow - case ESCN_SCROLLING:
      {
         PESCNSCROLL p = (PESCNSCROLL) pParam;

         // only do one scroll bar
         if (!p->pControl->m_pszName)
            break;

         // set value
         fp fVal;
         fVal = p->pControl->AttribGetInt (Pos());
         if (!_wcsicmp(p->pControl->m_pszName, L"detail")) {
            fVal /= 100.0;
            if (fVal != pv->m_afCirrusDetail[dwLayer]) {
               pv->m_pWorld->ObjectAboutToChange (pv);
               pv->m_afCirrusDetail[dwLayer] = fVal;
               pv->CalcAll ();
               pv->m_pWorld->ObjectChanged (pv);
            }
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"cover")) {
            fVal /= 100.0;
            if (fVal != pv->m_afCirrusCover[dwLayer]) {
               pv->m_pWorld->ObjectAboutToChange (pv);
               pv->m_afCirrusCover[dwLayer] = fVal;
               pv->CalcAll ();
               pv->m_pWorld->ObjectChanged (pv);
            }
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"scale")) {
            fVal *= 100.0;
            if (fVal != pv->m_afCirrusScale[dwLayer]) {
               pv->m_pWorld->ObjectAboutToChange (pv);
               pv->m_afCirrusScale[dwLayer] = fVal;
               pv->CalcAll ();
               pv->m_pWorld->ObjectChanged (pv);
            }
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"thick")) {
            fVal /= 100.0;
            if (fVal != pv->m_afCirrusThickness[dwLayer]) {
               pv->m_pWorld->ObjectAboutToChange (pv);
               pv->m_afCirrusThickness[dwLayer] = fVal;
               pv->CalcAll ();
               pv->m_pWorld->ObjectChanged (pv);
            }
            return TRUE;
         }
      }
      break;


   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"High-level clouds";
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}



/****************************************************************************
SkydomeAtmospherePage
*/
BOOL SkydomeAtmospherePage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCObjectSkydome pv = (PCObjectSkydome)pPage->m_pUserData;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         PCEscControl pControl;

         ComboBoxSet (pPage, L"resolution", pv->m_dwResolution);

         // haze
         pControl = pPage->ControlFind (L"haze");
         if (pControl)
            pControl->AttribSetInt (Pos(), (int)(pv->m_fHaze * 100));
         pControl = pPage->ControlFind (L"density");
         if (pControl)
            pControl->AttribSetInt (Pos(), (int)(pv->m_fDensityZenith * 100));
         pControl = pPage->ControlFind (L"numstars");
         if (pControl)
            pControl->AttribSetInt (Pos(), (int)pv->m_dwStarsNum);


         // numbers
         MeasureToString (pPage, L"skydomesize", pv->m_fSkydomeSize);

         pControl = pPage->ControlFind (L"autocolors");
         if (pControl)
            pControl->AttribSetBOOL (Checked(), pv->m_fAutoColors);

         // color
         FillStatusColor (pPage, L"zenithcolor", pv->m_cAtmosphereZenith);
         FillStatusColor (pPage, L"midcolor", pv->m_cAtmosphereMid);
         FillStatusColor (pPage, L"groundcolor", pv->m_cAtmosphereGround);
      }
      break;

   case ESCN_COMBOBOXSELCHANGE:
      {
         PESCNCOMBOBOXSELCHANGE p = (PESCNCOMBOBOXSELCHANGE) pParam;

         if (!p->pControl || !p->pControl->m_pszName)
            break;
         if (!_wcsicmp(p->pControl->m_pszName, L"resolution")) {
            DWORD dwVal = (p->pszName ? (DWORD) _wtoi(p->pszName) : 0);
            if (dwVal == pv->m_dwResolution)
               return TRUE;

            pv->m_pWorld->ObjectAboutToChange (pv);
            pv->m_dwResolution = dwVal;
            pv->CalcAll ();
            pv->m_dwNumPoints = 0;   // so re-calc skydome
            pv->m_pWorld->ObjectChanged (pv);
            return TRUE;
         }
      }

   case ESCN_EDITCHANGE:
      {
         PESCNEDITCHANGE p = (PESCNEDITCHANGE) pParam;

         pv->m_pWorld->ObjectAboutToChange (pv);
         MeasureParseString (pPage, L"skydomesize", &pv->m_fSkydomeSize);
         pv->m_fSkydomeSize = max(1, pv->m_fSkydomeSize);
         pv->m_dwNumPoints = 0;  // so recalc skydome
         // dont need to recalc rest - pv->CalcAll ();
         pv->m_pWorld->ObjectChanged (pv);
      }
      break;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;
         PWSTR psz;
         psz = p->pControl->m_pszName;

         if (!_wcsicmp(psz, L"autocolors")) {
            pv->m_pWorld->ObjectAboutToChange (pv);
            pv->m_fAutoColors = p->pControl->AttribGetBOOL (Checked());

            // only recalc if turned autocolors on
            if (pv->m_fAutoColors) {
               pv->CalcAll ();

               // relcalc sun and moon
               pv->CalcSunAndMoon ();

               // reset the colors
               FillStatusColor (pPage, L"zenithcolor", pv->m_cAtmosphereZenith);
               FillStatusColor (pPage, L"midcolor", pv->m_cAtmosphereMid);
               FillStatusColor (pPage, L"groundcolor", pv->m_cAtmosphereGround);
            }

            pv->m_pWorld->ObjectChanged (pv);
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"changezenith")) {
            COLORREF cr;
            cr = AskColor (pPage->m_pWindow->m_hWnd, pv->m_cAtmosphereZenith, pPage, L"zenithcolor");
            if (cr != pv->m_cAtmosphereZenith) {
               pv->m_pWorld->ObjectAboutToChange (pv);
               pv->m_cAtmosphereZenith = cr;
               pv->CalcAll ();
               pv->m_pWorld->ObjectChanged (pv);
            }
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"changemid")) {
            COLORREF cr;
            cr = AskColor (pPage->m_pWindow->m_hWnd, pv->m_cAtmosphereMid, pPage, L"midcolor");
            if (cr != pv->m_cAtmosphereMid) {
               pv->m_pWorld->ObjectAboutToChange (pv);
               pv->m_cAtmosphereMid = cr;
               pv->CalcAll ();
               pv->m_pWorld->ObjectChanged (pv);
            }
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"changeground")) {
            COLORREF cr;
            cr = AskColor (pPage->m_pWindow->m_hWnd, pv->m_cAtmosphereGround, pPage, L"groundcolor");
            if (cr != pv->m_cAtmosphereGround) {
               pv->m_pWorld->ObjectAboutToChange (pv);
               pv->m_cAtmosphereGround = cr;
               pv->CalcAll ();
               pv->m_pWorld->ObjectChanged (pv);
            }
            return TRUE;
         }
      }
      break;



   case ESCN_SCROLL:
   // take out because too slow - case ESCN_SCROLLING:
      {
         PESCNSCROLL p = (PESCNSCROLL) pParam;

         // only do one scroll bar
         if (!p->pControl->m_pszName)
            break;

         // set value
         fp fVal;
         fVal = p->pControl->AttribGetInt (Pos());

         if (!_wcsicmp(p->pControl->m_pszName, L"haze")) {
            fVal /= 100.0;
            if (fVal != pv->m_fHaze) {
               pv->m_pWorld->ObjectAboutToChange (pv);
               pv->m_fHaze = fVal;
               pv->CalcAll ();
               pv->m_pWorld->ObjectChanged (pv);
            }
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"numstars")) {
            if ((DWORD)fVal != pv->m_dwStarsNum) {
               pv->m_pWorld->ObjectAboutToChange (pv);
               pv->m_dwStarsNum = (DWORD)fVal;
               pv->CalcAll ();
               pv->m_pWorld->ObjectChanged (pv);
            }
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"density")) {
            fVal /= 100.0;
            if (fVal != pv->m_fDensityZenith) {
               pv->m_pWorld->ObjectAboutToChange (pv);
               pv->m_fDensityZenith = max(.01, fVal);
               pv->CalcAll ();
               pv->m_pWorld->ObjectChanged (pv);
            }
            return TRUE;
         }
      }
      break;


   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Atmosphere";
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}


/****************************************************************************
SkydomeCumulusPage
*/
BOOL SkydomeCumulusPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCObjectSkydome pv = (PCObjectSkydome)pPage->m_pUserData;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         PCEscControl pControl;

         pControl = pPage->ControlFind (L"draw");
         if (pControl)
            pControl->AttribSetBOOL (Checked(), pv->m_fCumulusDraw);


         // sliders
         pControl = pPage->ControlFind (L"detail");
         if (pControl)
            pControl->AttribSetInt (Pos(), (int)(pv->m_fCumulusDetail * 100));
         pControl = pPage->ControlFind (L"cover");
         if (pControl)
            pControl->AttribSetInt (Pos(), (int)(pv->m_fCumulusCover * 100));
         pControl = pPage->ControlFind (L"scale");
         if (pControl)
            pControl->AttribSetInt (Pos(), (int)(pv->m_fCumulusScale / 100));
         pControl = pPage->ControlFind (L"thick");
         if (pControl)
            pControl->AttribSetInt (Pos(), (int)(pv->m_fCumulusThickness * 100));
         pControl = pPage->ControlFind (L"sheer");
         if (pControl)
            pControl->AttribSetInt (Pos(), (int)(pv->m_fCumulusSheer * 100));

         // numbers
         MeasureToString (pPage, L"loc0", pv->m_pCumulusLoc.p[0]);
         MeasureToString (pPage, L"loc1", pv->m_pCumulusLoc.p[1]);
         MeasureToString (pPage, L"loc2", pv->m_pCumulusLoc.p[2]);
         AngleToControl (pPage, L"sheerangle", pv->m_fCumulusSheerAngle);
         DoubleToControl (pPage, L"seed", pv->m_dwCumulusSeed);

         // color
         FillStatusColor (pPage, L"Cumuluscolor", pv->m_cCumulusColor);
      }
      break;

   case ESCN_EDITCHANGE:
      {
         PESCNEDITCHANGE p = (PESCNEDITCHANGE) pParam;

         pv->m_pWorld->ObjectAboutToChange (pv);
         MeasureParseString (pPage, L"loc0", &pv->m_pCumulusLoc.p[0]);
         MeasureParseString (pPage, L"loc1", &pv->m_pCumulusLoc.p[1]);
         MeasureParseString (pPage, L"loc2", &pv->m_pCumulusLoc.p[2]);
         pv->m_dwCumulusSeed = (DWORD) DoubleFromControl (pPage, L"seed");
         pv->m_fCumulusSheerAngle = AngleFromControl (pPage, L"sheerangle");
         pv->m_pCumulusLoc.p[2] = max(pv->m_pCumulusLoc.p[2], 1);
         pv->CalcAll ();
         pv->m_pWorld->ObjectChanged (pv);
      }
      break;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;
         PWSTR psz;
         psz = p->pControl->m_pszName;

         if (!_wcsicmp(psz, L"draw")) {
            pv->m_pWorld->ObjectAboutToChange (pv);
            pv->m_fCumulusDraw = p->pControl->AttribGetBOOL (Checked());
            pv->CalcAll ();
            pv->m_pWorld->ObjectChanged (pv);
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"changeCumulus")) {
            COLORREF cr;
            cr = AskColor (pPage->m_pWindow->m_hWnd, pv->m_cCumulusColor, pPage, L"Cumuluscolor");
            if (cr != pv->m_cCumulusColor) {
               pv->m_pWorld->ObjectAboutToChange (pv);
               pv->m_cCumulusColor = cr;
               pv->CalcAll ();
               pv->m_pWorld->ObjectChanged (pv);
            }
            return TRUE;
         }
      }
      break;



   case ESCN_SCROLL:
   // take out because too slow - case ESCN_SCROLLING:
      {
         PESCNSCROLL p = (PESCNSCROLL) pParam;

         // only do one scroll bar
         if (!p->pControl->m_pszName)
            break;

         // set value
         fp fVal;
         fVal = p->pControl->AttribGetInt (Pos());
         if (!_wcsicmp(p->pControl->m_pszName, L"detail")) {
            fVal /= 100.0;
            if (fVal != pv->m_fCumulusDetail) {
               pv->m_pWorld->ObjectAboutToChange (pv);
               pv->m_fCumulusDetail = fVal;
               pv->CalcAll ();
               pv->m_pWorld->ObjectChanged (pv);
            }
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"cover")) {
            fVal /= 100.0;
            if (fVal != pv->m_fCumulusCover) {
               pv->m_pWorld->ObjectAboutToChange (pv);
               pv->m_fCumulusCover = fVal;
               pv->CalcAll ();
               pv->m_pWorld->ObjectChanged (pv);
            }
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"scale")) {
            fVal *= 100.0;
            if (fVal != pv->m_fCumulusScale) {
               pv->m_pWorld->ObjectAboutToChange (pv);
               pv->m_fCumulusScale = fVal;
               pv->CalcAll ();
               pv->m_pWorld->ObjectChanged (pv);
            }
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"thick")) {
            fVal /= 100.0;
            if (fVal != pv->m_fCumulusThickness) {
               pv->m_pWorld->ObjectAboutToChange (pv);
               pv->m_fCumulusThickness = fVal;
               pv->CalcAll ();
               pv->m_pWorld->ObjectChanged (pv);
            }
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"sheer")) {
            fVal /= 100.0;
            if (fVal != pv->m_fCumulusSheer) {
               pv->m_pWorld->ObjectAboutToChange (pv);
               pv->m_fCumulusSheer = fVal;
               pv->CalcAll ();
               pv->m_pWorld->ObjectChanged (pv);
            }
            return TRUE;
         }
      }
      break;


   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Cumulus clouds";
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}

typedef struct {
   PCObjectSkydome   pv;         // skydome
   DWORD             dwPlanet;   // index into m_lSuns that editing
} PLANETSPAGE, *PPLANETSPAGE;

/****************************************************************************
SkydomePlanetsPage
*/
BOOL SkydomePlanetsPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PPLANETSPAGE ppp = (PPLANETSPAGE)pPage->m_pUserData;
   PCObjectSkydome pv = ppp->pv;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         // fill in the list boxes
         PCEscControl pControl;
         DWORD i;
         ESCMLISTBOXADD lba;
         PSKYDOMELIGHT pSun;
         pSun = (PSKYDOMELIGHT) pv->m_lSuns.Get(0);
         pControl = pPage->ControlFind (L"planets");
         for (i = 0; i < pv->m_lSuns.Num(); i++, pSun++) {
            memset (&lba, 0, sizeof(lba));
            lba.dwInsertBefore = -1;
            lba.pszText = pSun->szName[0] ? pSun->szName : L"Un-named";
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
         PWSTR psz;
         psz = p->pControl->m_pszName;

         PCEscControl pControl;
         DWORD dwSel;
         pControl = pPage->ControlFind (L"planets");
         dwSel = pControl->AttribGetInt (CurSel());

         if (!_wcsicmp(psz, L"edit")) {
            if (dwSel >= pv->m_lSuns.Num()) {
               pPage->MBWarning (L"You must first select a sun, planet, or moon from the above list.");
               return TRUE;
            }

            ppp->dwPlanet = dwSel;
            pPage->Exit (L"edit");
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"remove")) {
            if (dwSel >= pv->m_lSuns.Num()) {
               pPage->MBWarning (L"You must first select a sun, planet, or moon from the above list.");
               return TRUE;
            }

            if (IDYES != pPage->MBYesNo (L"Are you sure you wish to delete the selected sun, moon, or planet?"))
               return TRUE;

            // delete it
            PSKYDOMELIGHT ps;
            if (pv->m_pWorld)
               pv->m_pWorld->ObjectAboutToChange (pv);
            ps = (PSKYDOMELIGHT) pv->m_lSuns.Get(dwSel);
            if (ps->pSurface)
               delete ps->pSurface;
            pv->m_lSuns.Remove (dwSel);
            pv->CalcAll();
            pv->CalcAttribList();
            if (pv->m_pWorld)
               pv->m_pWorld->ObjectAboutToChange (pv);

            // remove from list
            ESCMLISTBOXDELETE lbd;
            memset (&lbd, 0, sizeof(lbd));
            lbd.dwIndex = dwSel;
            pControl->Message (ESCM_LISTBOXDELETE, &lbd);

            // set the sel
            pControl->AttribSetInt (CurSel(), 0);

            return TRUE;
         }
         else if (!_wcsicmp(psz, L"copy")) {
            if (dwSel >= pv->m_lSuns.Num()) {
               pPage->MBWarning (L"You must first select a sun, planet, or moon from the above list.");
               return TRUE;
            }

            PSKYDOMELIGHT ps;
            SKYDOMELIGHT sd;
            ps = (PSKYDOMELIGHT) pv->m_lSuns.Get(dwSel);
            sd = *ps;
            if (sd.pSurface)
               sd.pSurface = sd.pSurface->Clone();
            wcscpy (sd.szName, L"New copy");

            // add it
            if (pv->m_pWorld)
               pv->m_pWorld->ObjectAboutToChange (pv);
            pv->m_lSuns.Add (&sd);
            pv->CalcAll();
            pv->CalcAttribList();
            if (pv->m_pWorld)
               pv->m_pWorld->ObjectAboutToChange (pv);

            // jump right to editing it
            ppp->dwPlanet = pv->m_lSuns.Num()-1;
            pPage->Exit (L"edit");
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"addsun") || !_wcsicmp(psz, L"addmoon")) {
            PSKYDOMELIGHT ps;
            SKYDOMELIGHT sd;
            ps = (!_wcsicmp(psz, L"addsun")) ? &pv->m_lSun : &pv->m_lMoon;
            sd = *ps;
            if (sd.pSurface)
               sd.pSurface = sd.pSurface->Clone();
            sd.fVisible = TRUE;
            wcscpy (sd.szName, (ps == &pv->m_lSun)? L"Copy of sun" : L"Copy of moon");

            // add it
            if (pv->m_pWorld)
               pv->m_pWorld->ObjectAboutToChange (pv);
            pv->m_lSuns.Add (&sd);
            pv->CalcAll();
            pv->CalcAttribList();
            if (pv->m_pWorld)
               pv->m_pWorld->ObjectChanged (pv);

            // jump right to editing it
            ppp->dwPlanet = pv->m_lSuns.Num()-1;
            pPage->Exit (L"edit");
            return TRUE;
         }
      }
      break;



   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Suns, planets, and moons";
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}


/****************************************************************************
SkydomePlanetsEditPage
*/
BOOL SkydomePlanetsEditPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PPLANETSPAGE ppp = (PPLANETSPAGE)pPage->m_pUserData;
   PCObjectSkydome pv = ppp->pv;
   PSKYDOMELIGHT pSun = (PSKYDOMELIGHT) ppp->pv->m_lSuns.Get(ppp->dwPlanet);

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         PCEscControl pControl;

         pControl = pPage->ControlFind (L"name");
         if (pControl)
            pControl->AttribSet (Text(), pSun->szName);

         TEXTUREPOINT tp;
         pv->DirectionToAltAz (&pSun->pDir, &tp);
         AngleToControl (pPage, L"azimuth", tp.h);
         AngleToControl (pPage, L"altitude", tp.v);

         // bools
         pControl = pPage->ControlFind (L"visible");
         if (pControl)
            pControl->AttribSetBOOL (Checked(), pSun->fVisible);
         pControl = pPage->ControlFind (L"emitlight");
         if (pControl)
            pControl->AttribSetBOOL (Checked(), pSun->fEmitLight);
         pControl = pPage->ControlFind (L"usetexture");
         if (pControl)
            pControl->AttribSetBOOL (Checked(), pSun->pSurface ? TRUE : FALSE);

         // sliders
         pControl = pPage->ControlFind (L"lumens");
         if (pControl)
            pControl->AttribSetInt (Pos(), (int)(LumensToRange(pSun->fLumens) * 100));
         pControl = pPage->ControlFind (L"ambientlumens");
         if (pControl)
            pControl->AttribSetInt (Pos(), (int)(LumensToRange(pSun->fAmbientLumens) * 100));
         pControl = pPage->ControlFind (L"solarlumens");
         if (pControl)
            pControl->AttribSetInt (Pos(), (int)(LumensToRange(pSun->fSolarLumens) * 100));
         pControl = pPage->ControlFind (L"sizerad");
         if (pControl)
            pControl->AttribSetInt (Pos(), (int)(pSun->fSizeRad * 100));
         pControl = pPage->ControlFind (L"dist");
         if (pControl)
            pControl->AttribSetInt (Pos(), (int)(sqrt(pSun->fDist)));
         pControl = pPage->ControlFind (L"brightness");
         if (pControl)
            pControl->AttribSetInt (Pos(), (int)(LumensToRange(pSun->fBrightness) * 100));


         // color
         FillStatusColor (pPage, L"directcolor", UnGamma (pSun->awColor));
         FillStatusColor (pPage, L"ambientcolor", UnGamma (pSun->awAmbientColor));
         FillStatusColor (pPage, L"solarcolor", UnGamma (pSun->awSolarColor));

         // enable/disable button based on texture
         pControl = pPage->ControlFind (L"changetexture");
         if (pControl)
            pControl->Enable (pSun->pSurface ? TRUE : FALSE);
      }
      break;

   case ESCN_EDITCHANGE:
      {
         PESCNEDITCHANGE p = (PESCNEDITCHANGE) pParam;
         PCEscControl pControl;
         DWORD dwNeeded;

         pv->m_pWorld->ObjectAboutToChange (pv);

         pControl = pPage->ControlFind (L"name");
         if (pControl)
            pControl->AttribGet (Text(), pSun->szName, sizeof(pSun->szName), &dwNeeded);

         TEXTUREPOINT tp;
         tp.h = AngleFromControl (pPage, L"azimuth");
         tp.v = AngleFromControl (pPage, L"altitude");
         pv->AltAzToDirection (&tp, &pSun->pDir);

         pv->CalcAll ();
         pv->CalcAttribList();   // in case the name has changed
         pv->m_pWorld->ObjectChanged (pv);
      }
      break;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;
         PWSTR psz;
         psz = p->pControl->m_pszName;

         if (!_wcsicmp(psz, L"visible")) {
            pv->m_pWorld->ObjectAboutToChange (pv);
            pSun->fVisible = p->pControl->AttribGetBOOL (Checked());
            pv->CalcAll ();
            pv->m_pWorld->ObjectChanged (pv);
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"emitlight")) {
            pv->m_pWorld->ObjectAboutToChange (pv);
            pSun->fEmitLight = p->pControl->AttribGetBOOL (Checked());
            pv->CalcAll ();
            pv->m_pWorld->ObjectChanged (pv);
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"changetexture")) {
            if (!pSun->pSurface)
               return TRUE;

            PCObjectSurface pNew;
            pNew = pSun->pSurface->Clone();

            if (!TextureSelDialog(pv->m_OSINFO.dwRenderShard, pPage->m_pWindow->m_hWnd, pNew, pv->m_pWorld)) {
               delete pNew;
               return TRUE;
            }

            // else, new texture
            pv->m_pWorld->ObjectAboutToChange (pv);
            delete pSun->pSurface;
            pSun->pSurface = pNew;
            
            //delete the current teture since changing surface not affected by hash
            GUID gTexture;
            pv->CalcTextureGUID (&gTexture);
            TextureCacheDelete (pv->m_OSINFO.dwRenderShard, &gTexture, &gTexture);

            pv->CalcAll ();
            pv->m_pWorld->ObjectChanged (pv);
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"usetexture")) {
            pv->m_pWorld->ObjectAboutToChange (pv);
            BOOL fUse = p->pControl->AttribGetBOOL (Checked());
            if (fUse && !pSun->pSurface) {
               pSun->pSurface = pv->m_lMoon.pSurface->Clone();
            }
            else if (!fUse && pSun->pSurface) {
               delete pSun->pSurface;
               pSun->pSurface = NULL;
            }

            //delete the current teture since changing surface not affected by hash
            GUID gTexture;
            pv->CalcTextureGUID (&gTexture);
            TextureCacheDelete (pv->m_OSINFO.dwRenderShard, &gTexture, &gTexture);

            pv->CalcAll ();
            pv->m_pWorld->ObjectChanged (pv);


            // enable/disable button based on texture
            PCEscControl pControl;
            pControl = pPage->ControlFind (L"changetexture");
            if (pControl)
               pControl->Enable (pSun->pSurface ? TRUE : FALSE);
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"changedirect")) {
            COLORREF cr;
            cr = AskColor (pPage->m_pWindow->m_hWnd, UnGamma(pSun->awColor), pPage, L"directcolor");
            if (cr != UnGamma(pSun->awColor)) {
               pv->m_pWorld->ObjectAboutToChange (pv);
               Gamma (cr, pSun->awColor);
               pv->CalcAll ();
               pv->m_pWorld->ObjectChanged (pv);
            }
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"changeambient")) {
            COLORREF cr;
            cr = AskColor (pPage->m_pWindow->m_hWnd, UnGamma(pSun->awAmbientColor), pPage, L"ambientcolor");
            if (cr != UnGamma(pSun->awAmbientColor)) {
               pv->m_pWorld->ObjectAboutToChange (pv);
               Gamma (cr, pSun->awAmbientColor);
               pv->CalcAll ();
               pv->m_pWorld->ObjectChanged (pv);
            }
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"changesolar")) {
            COLORREF cr;
            cr = AskColor (pPage->m_pWindow->m_hWnd, UnGamma(pSun->awSolarColor), pPage, L"solarcolor");
            if (cr != UnGamma(pSun->awSolarColor)) {
               pv->m_pWorld->ObjectAboutToChange (pv);
               Gamma (cr, pSun->awSolarColor);
               pv->CalcAll ();
               pv->m_pWorld->ObjectChanged (pv);
            }
            return TRUE;
         }
      }
      break;



   case ESCN_SCROLL:
   // take out because too slow - case ESCN_SCROLLING:
      {
         PESCNSCROLL p = (PESCNSCROLL) pParam;

         // only do one scroll bar
         if (!p->pControl->m_pszName)
            break;

         // set value
         fp fVal;
         fVal = p->pControl->AttribGetInt (Pos());

         if (!_wcsicmp(p->pControl->m_pszName, L"lumens")) {
            fVal /= 100.0;
            fVal = RangeToLumens (fVal);
            if (fVal != pSun->fLumens) {
               pv->m_pWorld->ObjectAboutToChange (pv);
               pSun->fLumens = fVal;
               pv->CalcAll ();
               pv->m_pWorld->ObjectChanged (pv);
            }
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"ambientlumens")) {
            fVal /= 100.0;
            fVal = RangeToLumens (fVal);
            if (fVal != pSun->fAmbientLumens) {
               pv->m_pWorld->ObjectAboutToChange (pv);
               pSun->fAmbientLumens = fVal;
               pv->CalcAll ();
               pv->m_pWorld->ObjectChanged (pv);
            }
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"solarlumens")) {
            fVal /= 100.0;
            fVal = RangeToLumens (fVal);
            if (fVal != pSun->fSolarLumens) {
               pv->m_pWorld->ObjectAboutToChange (pv);
               pSun->fSolarLumens = fVal;
               pv->CalcAll ();
               pv->m_pWorld->ObjectChanged (pv);
            }
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"brightness")) {
            fVal /= 100.0;
            fVal = RangeToLumens (fVal);
            if (fVal != pSun->fBrightness) {
               pv->m_pWorld->ObjectAboutToChange (pv);
               pSun->fBrightness = fVal;
               pv->CalcAll ();
               pv->m_pWorld->ObjectChanged (pv);
            }
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"sizerad")) {
            fVal /= 100.0;
            if (fVal != pSun->fSizeRad) {
               pv->m_pWorld->ObjectAboutToChange (pv);
               pSun->fSizeRad = fVal;
               pv->CalcAll ();
               pv->m_pWorld->ObjectChanged (pv);
            }
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"dist")) {
            fVal *= fVal;
            if (fVal != pSun->fDist) {
               pv->m_pWorld->ObjectAboutToChange (pv);
               pSun->fDist = max(fVal,1);
               pv->CalcAll ();
               pv->m_pWorld->ObjectChanged (pv);
            }
            return TRUE;
         }
      }
      break;


   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Modify a sun, planet, or moon";
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}
/**********************************************************************************
CObjectSkydome::DialogShow - Causes the object to show a dialog box that allows
different attributes to be changed.

inputs
   DWORD             dwSuface - Surface that was clicked on. 0 if unknown.
   PCEscWindow       pWindow - Escarpment window to draw it in
returns
   BOOL - TRUE if user pressed "Back", FALSE if user pressed "Close"
*/
BOOL CObjectSkydome::DialogShow (DWORD dwSurface, PCEscWindow pWindow, PCPoint pClick)
{
   PWSTR pszRet;

ask:
   pszRet = pWindow->PageDialog (ghInstance, IDR_MMLSKYDOME, SkydomePage, this);
parse:
   if (!pszRet)
      return FALSE;
   if (!_wcsicmp(pszRet, L"cirrus") ||!_wcsicmp(pszRet, L"cirrus2")) {
      CIRRUSPAGE cp;
      cp.dwLayer = (!_wcsicmp(pszRet, L"cirrus2")) ? 1 : 0;
      cp.pSky = this;
      pszRet = pWindow->PageDialog (ghInstance, IDR_MMLSKYDOMECIRRUS, SkydomeCirrusPage, &cp);
      if (pszRet && !_wcsicmp(pszRet, Back()))
         goto ask;
      goto parse;
   }
   else if (!_wcsicmp(pszRet, L"Cumulus")) {
      pszRet = pWindow->PageDialog (ghInstance, IDR_MMLSKYDOMECUMULUS, SkydomeCumulusPage, this);
      if (pszRet && !_wcsicmp(pszRet, Back()))
         goto ask;
      goto parse;
   }
   else if (!_wcsicmp(pszRet, L"atmosphere")) {
      pszRet = pWindow->PageDialog (ghInstance, IDR_MMLSKYDOMEATMOSPHERE, SkydomeAtmospherePage, this);
      if (pszRet && !_wcsicmp(pszRet, Back()))
         goto ask;
      goto parse;
   }
   else if (!_wcsicmp(pszRet, L"planets")) {
      PLANETSPAGE pp;
      pp.dwPlanet = 0;
      pp.pv = this;

planets:
      pszRet = pWindow->PageDialog (ghInstance, IDR_MMLSKYDOMEPLANETS, SkydomePlanetsPage, &pp);
      if (pszRet && !_wcsicmp(pszRet, Back()))
         goto ask;
      if (pszRet && !_wcsicmp(pszRet, L"edit")) {
         pszRet = pWindow->PageDialog (ghInstance, IDR_MMLSKYDOMEPLANETSEDIT, SkydomePlanetsEditPage, &pp);
         if (pszRet && !_wcsicmp(pszRet, Back()))
            goto planets;
      }
      goto parse;
   }
   else if (!_wcsicmp(pszRet, L"sun")) {
      pszRet = pWindow->PageDialog (ghInstance, IDR_MMLSKYDOMESUN, SkydomeSunPage, this);
      if (pszRet && !_wcsicmp(pszRet, Back()))
         goto ask;
      goto parse;
   }

   return !_wcsicmp(pszRet, Back());
}

/**********************************************************************************
CObjectSkydome::DialogQuery - Returns TRUE if the object supports a custom
dialog box, FALSE if it doesn't. The default object doesn't
*/
BOOL CObjectSkydome::DialogQuery (void)
{
   return TRUE;
}


/**********************************************************************************
CObjectSkydome::CalcAll - Call this when an attribute has changed that will require
the skydome image to change

NOTE: Does NOT include the recalulation of the dome.
*/
void CObjectSkydome::CalcAll (void)
{
   m_fSunMoonDirty = TRUE;

   // delete this dome's texture if there is one
   // TextureCacheDelete (&m_gTexture, &m_gTexture);
}



/**********************************************************************************
CObjectSkydome::CloudClearArea - Given an area of the skydome, clear it so can
put clouds there.

inputs
   PSKYPIXEL      ps - Current sky. dwSkyRes x dwSkyRes
   DWORD          dwXMin, dwYMin, dwXMax, dwYMax - min and max areas to clear
returns
   none
*/
void CObjectSkydome::CloudClearArea (PSKYPIXEL ps, DWORD dwXMin, DWORD dwYMin,
                                     DWORD dwXMax, DWORD dwYMax)
{
   DWORD dwSkyRes = SkyRes();
   dwXMax = min(dwSkyRes, dwXMax);
   dwYMax = min(dwSkyRes, dwYMax);

   DWORD x,y;
   PSKYPIXEL psCur;
   for (y = dwYMin; y < dwYMax; y++) {
      psCur = ps + (y * dwSkyRes + dwXMin);
      for (x = dwXMin; x < dwXMax; x++, psCur++) {
         psCur->fCloudDensity = 0;
         psCur->fCloudZ = ZINFINITE;
      }
   }
}


/**********************************************************************************
CObjectSkydome::CloudCalcLoc - Given an area of the skydome, this calculates
the cloud location (stored into afCloudPixel) so that the normal of the cloud
can more quickly be calculated.

inputs
   PSKYPIXEL      ps - Current sky. dwSkyRes x dwSkyRes
   DWORD          dwXMin, dwYMin, dwXMax, dwYMax - min and max areas to clear
returns
   none
*/
void CObjectSkydome::CloudCalcLoc (PSKYPIXEL ps, DWORD dwXMin, DWORD dwYMin,
                                     DWORD dwXMax, DWORD dwYMax)
{
   DWORD dwSkyRes = SkyRes();
   dwXMax = min(dwSkyRes, dwXMax);
   dwYMax = min(dwSkyRes, dwYMax);

   DWORD x,y;
   PSKYPIXEL psCur;
   TEXTUREPOINT tpPixel;
   CPoint pDir;
   for (y = dwYMin; y < dwYMax; y++) {
      psCur = ps + (y * dwSkyRes + dwXMin);
      for (x = dwXMin; x < dwXMax; x++, psCur++) {
         if (psCur->fCloudZ == ZINFINITE)
            continue;

         tpPixel.h = x;
         tpPixel.v = y;
         PixelToDirection (&tpPixel, &pDir);

         // accound for Z
         pDir.Normalize();
         pDir.Scale (psCur->fCloudZ);

         psCur->afCloudPixel[0] = pDir.p[0];
         psCur->afCloudPixel[1] = pDir.p[1];
         psCur->afCloudPixel[2] = pDir.p[2];
      } // x
   } // y
}



/**********************************************************************************
CObjectSkydome::CloudRender - Looks at each pixel in the range and renders
it. It uses the fCloudDensity and afCloudPixel information for drawing.

inputs
   COLORREF       cColor - Color of the cloud
   PSKYPIXEL      ps - Current sky. dwSkyRes x dwSkyRes
   PSKYDOMELIGHT  pSunList - List of light emitters
   DWORD          dwNumSun - Number of suns
   fp             fDensityScale - Scale the atmopheric density by this much... basically
                  based on the height of the clouds. Cirrus is 1, cumulus about .2
   DWORD          dwXMin, dwYMin, dwXMax, dwYMax - min and max areas to clear
returns
   none
*/

void CObjectSkydome::CloudRender (COLORREF cColor, PSKYPIXEL ps,
                                  PSKYDOMELIGHT pSunList, DWORD dwNumSun,
                                  fp fDensityScale,
                                  DWORD dwXMin, DWORD dwYMin,
                                     DWORD dwXMax, DWORD dwYMax)
{
   DWORD dwSkyRes = SkyRes();
   dwXMax = min(dwSkyRes, dwXMax);
   dwYMax = min(dwSkyRes, dwYMax);

   // color
   WORD  awColor[3];
   Gamma (cColor, awColor);

   // determine the ambient light
   float afAmbient[3];
   DWORD i, j;
   afAmbient[0] = afAmbient[1] = afAmbient[2] = 0;
   for (i = 0; i < dwNumSun; i++) {
      if (!pSunList[i].fAmbientLumens)
         continue;

      for (j = 0; j < 3; j++)
         afAmbient[j] += pSunList[i].fAmbientLumens / CM_LUMENSSUN *
            (fp)(pSunList[i].awAmbientColor[j] / 4 + 0xc000);
      // BUGFIX - Sun ambient color was correct, but clouds were looking too blue
      // so average in with some grey
   }
   for (j = 0; j < 3; j++)
      afAmbient[j] *= (fp) awColor[j] / 0x10000;

   // BUGFIX - multithreaded
   MTSKYDOME mt;
   memset (&mt, 0, sizeof(mt));
   mt.dwPass = 1;
   mt.ps = ps;
   mt.dwXMin = dwXMin;
   mt.dwXMax = dwXMax;
   mt.dwNumSun = dwNumSun;
   mt.pSunList = pSunList;
   mt.cColor = cColor;
   mt.fDensityScale = fDensityScale;
   memcpy (mt.afAmbient, afAmbient, sizeof(afAmbient));
   ThreadLoop (0, dwSkyRes, 1, &mt, sizeof(mt));

}



/**********************************************************************************
CObjectSkydome::CloudCalcCirrus - Fills in the cloud information for a cirrus
cloud. It fills in the whole sky.

For each pixel it affects, this fills in:
   fCloudDensity
   fCloudZ
   afCloudPixel - No need to call CloudCalcLoc()

inputs
   DWORD          dwLayer - 0 is highest, 1 is lower
   PSKYPIXEL      ps - Current sky. dwSkyRes x dwSkyRes
returns
   none
*/
void CObjectSkydome::CloudCalcCirrus (DWORD dwLayer, PSKYPIXEL ps)
{

   DWORD dwSkyRes = SkyRes();
   m_afCirrusScale[dwLayer] = max(m_afCirrusScale[dwLayer], 1);
   fp fSkyCircle;
   fSkyCircle = (PI / 2) / SKYDOMELATRANGE * (dwSkyRes/2);
   fSkyCircle *= fSkyCircle;

   // BUGFIX - multithreaded
   MTSKYDOME mt;
   memset (&mt, 0, sizeof(mt));
   mt.dwPass = 2;
   mt.ps = ps;
   mt.dwLayer = dwLayer;
   mt.fSkyCircle = fSkyCircle;
   ThreadLoop (0, dwSkyRes, 1, &mt, sizeof(mt));



   // BUGFIX - multithreaded
   memset (&mt, 0, sizeof(mt));
   mt.dwPass = 3;
   mt.ps = ps;
   ThreadLoop (0, dwSkyRes, 1, &mt, sizeof(mt));
}


/**********************************************************************************
CObjectSkydome::DrawCirrus - Draws the cirrus clouds in the sky.

inputs
   DWORD          dwLayer - 0 is higherst, 1 less
   PSKYPIXEL      ps - Current sky. dwSkyRes x dwSkyRes
   PSKYDOMELIGHT  pSunList - List of light emitters
   DWORD          dwNumSun - Number of suns
returns
   none
*/
void CObjectSkydome::DrawCirrus (DWORD dwLayer, PSKYPIXEL ps,PSKYDOMELIGHT pSunList, DWORD dwNumSun)
{
   if (!m_afCirrusDraw[dwLayer])
      return;

   CloudClearArea(ps);
   CloudCalcCirrus(dwLayer, ps);
   CloudRender(m_acCirrusColor[dwLayer], ps, pSunList, dwNumSun, dwLayer ? .4 : 1);
}



/**********************************************************************************
CObjectSkydome::CloudCalcCumulus - Fills in the cloud information for a Cumulus
cloud. It fills in the whole sky.

For each pixel it affects, this fills in:
   fCloudDensity
   fCloudZ
   NOTE - NEED to call CloudCalcLoc() after this

inputs
   PSKYPIXEL      ps - Current sky. dwSkyRes x dwSkyRes
returns
   none
*/
void CObjectSkydome::CloudCalcCumulus (PSKYPIXEL ps)
{
   DWORD dwSkyRes = SkyRes();
   // BUGFIX - multithreaded
   MTSKYDOME mt;
   memset (&mt, 0, sizeof(mt));
   mt.dwPass = 4;
   mt.ps = ps;
   ThreadLoop (0, dwSkyRes, 1, &mt, sizeof(mt));
      // NOTE: There's a potential the spheres in  the cumulus could collide,
      // but it's unlikely, and not worth worrying about
}


/**********************************************************************************
CObjectSkydome::CloudSmooth - Do smoothing on the clouds. Smooths out the z values.

inputs
   PSKYPIXEL      ps - Sky
   DWORD          dwSize - Filter size, dwSize x dwSize pixels
returns
   none
*/
void CObjectSkydome::CloudSmooth (PSKYPIXEL ps, DWORD dwSize)
{
   int x, y, xx, yy;
   PSKYPIXEL psCur = ps;
   PSKYPIXEL psCur2;
   int iSize = (int)dwSize/2;
   fp fSum;
   DWORD dwCount;

   // how close need to be for an acceptable blend
   DWORD dwSkyRes = SkyRes();
   int iDist = dwSkyRes / 50;
   int iCurDist;

   // NOTE: Cant multithread this because depends on previous points
   for (y = 0; y < (int)dwSkyRes; y++) for (x = 0; x < (int)dwSkyRes; x++, psCur++) {
      if (psCur->fCloudZ == ZINFINITE)
         continue;   // no cloud

      fSum = 0;
      dwCount = 0;
      for (yy = max(y-iSize,0); yy < min(y+iSize,(int)dwSkyRes); yy++)
         for (xx = max(x-iSize,0); xx < min(x+iSize,(int)dwSkyRes); xx++) {
            psCur2 = ps + (yy * dwSkyRes + xx);
            if (psCur2->fCloudZ == ZINFINITE)
               continue;

            // don't include if from a futher pixel
            iCurDist = abs((int)xx - (int)psCur2->awCloudPixel[0]) + abs((int)yy - (int)psCur2->awCloudPixel[1]);
            if (iCurDist > iDist)
               continue;

            fSum += psCur2->fCloudZ;
            dwCount++;
         } // xx and yy

      if (dwCount)
         psCur->fCloudZ = fSum / (fp) dwCount;
   } // x and y
}


/**********************************************************************************
CObjectSkydome::DrawCumulus - Draws the Cumulus clouds in the sky.

inputs
   PSKYPIXEL      ps - Current sky. dwSkyRes x dwSkyRes
   PSKYDOMELIGHT  pSunList - List of light emitters
   DWORD          dwNumSun - Number of suns
returns
   none
*/
void CObjectSkydome::DrawCumulus (PSKYPIXEL ps,PSKYDOMELIGHT pSunList, DWORD dwNumSun)
{
   if (!m_fCumulusDraw)
      return;

   CloudClearArea(ps);
   CloudCalcCumulus(ps);

   int iShift = (int) m_dwResolution + TextureDetailGet(m_OSINFO.dwRenderShard) + 1;
   CloudSmooth (ps, 1 << iShift);
   //CloudSmooth (ps, 2 << m_dwResolution);
      // BUGFIX - Less smoothing. Was 2 << m_dwResolution

   CloudCalcLoc(ps);
   CloudRender(m_cCumulusColor, ps, pSunList, dwNumSun, .2);
}

/**********************************************************************************
CObjectSkydome::SRand - Seeds the random value for this sky

inputs
   DWORD       dwSeed - Extra seed to add
*/
void CObjectSkydome::SRand (DWORD dwSeed)
{
   // seed random based on GUID of tree, additional value
   DWORD dwCalc;
   DWORD *padw;
   DWORD i;
   dwCalc = 0;
   padw = (DWORD*) &m_gGUID;
   for (i = 0; i < sizeof(m_gGUID)/sizeof(DWORD); i++)
      dwCalc += padw[i];
   srand (dwCalc+dwSeed);
}


/**********************************************************************************
CObjectSkydome::DrawSphere - Draws a sphere onto the the skydome.

Will will in Z and density

inputs
   PCPoint     pLoc - Location of the spehere in space
   fp          fRadPix - Radius of the spehere in pixels
   PTEXTUREPOINT ptpPixelOrig - Original pixel (for lower end of cloud) - used to make
               sure the blurring of clouds looks better
   PSKYPIXEL   ps - Pixels to write to
returns
   none
*/
void CObjectSkydome::DrawSphere (PCPoint pLoc, fp fRadPix, PTEXTUREPOINT ptpPixelOrig, PSKYPIXEL ps)
{
   // find out what pixel it's around
   TEXTUREPOINT tp;
   DirectionToPixel (pLoc, &tp);

   // how large is it in terms of space?
   fp fRad, fLen;
   DWORD dwSkyRes = SkyRes();
   fRad = fRadPix / ((fp)dwSkyRes/2) * SKYDOMELATRANGE; // amount of arc
   fLen = pLoc->Length();
   fRad = fLen * tan(fRad);

   // bounding box
   int iXMin, iXMax, iYMin, iYMax;
   iXMin = (int) max(0, tp.h - fRadPix - 1);
   iXMax = (int) min((fp)dwSkyRes, tp.h + fRadPix + 1);
   iYMin = (int) max(0, tp.v - fRadPix - 1);
   iYMax = (int) min((fp)dwSkyRes, tp.v + fRadPix + 1);

   // loop
   int x, y;
   fp fX, fY, fZ, fR, fDensity;
   PSKYPIXEL psCur;
   fR = fRadPix * fRadPix;
   TEXTUREPOINT tpNew, tpExist;
   for (y = iYMin; y < iYMax; y++) {
      psCur = ps + (y * dwSkyRes + iXMin);
      fY = (fp) y - tp.v;
      fY *= fY;
      for (x = iXMin; x < iXMax; x++, psCur++) {
         fX = (fp) x - tp.h;
         fX *= fX;
         fZ = fR - fX - fY;
         if (fZ <= 0)
            continue; // no pixel
         fZ = sqrt(fZ);
         fZ = fZ / fRadPix * fRad;  // so have it in meters
         fDensity = fZ * 2;
         fZ = fLen - fZ;
       
         // BUGFIX - MOre intelligent cloud density
         psCur->fCloudDensity += fDensity;   // assume no overlap
         if (psCur->fCloudZ != ZINFINITE) {
            // figure out overlap
            tpNew.h = fZ;
            tpNew.v = fZ + fDensity;
            tpExist.h = psCur->fCloudZ;
            tpExist.v = psCur->fCloudZ + psCur->fCloudDensity;

            // see if there's overlap
            tpNew.h = max(tpNew.h, tpExist.h);
            tpNew.v = min(tpNew.v, tpExist.v);
            if (tpNew.v > tpNew.h)
               psCur->fCloudDensity -= (tpNew.v - tpNew.h);
         }

         // increase the density and location
         if (fZ < psCur->fCloudZ) {
            psCur->fCloudZ = fZ;
            psCur->awCloudPixel[0] = (WORD) ptpPixelOrig->h;
            psCur->awCloudPixel[1] = (WORD) ptpPixelOrig->v;
         }
      } // x
   } // y
}

/**********************************************************************************
CObjectSkydome::TextureQuery -
asks the object what textures it uses. This allows the save-function
to save custom textures into the file. The object just ADDS (doesn't
clear or remove) elements, which are two guids in a row: the
gCode followed by the gSub of the object. Of course, it may add more
than one texture
*/
BOOL CObjectSkydome::TextureQuery (PCListFixed plText)
{
   DWORD i;
   GUID ag[2];
   PSKYDOMELIGHT pSun;
   pSun = (PSKYDOMELIGHT) m_lSuns.Get(0);
   for (i = 0; i < m_lSuns.Num() + 2; i++, pSun++) {
      if (i == m_lSuns.Num())
         pSun = &m_lSun;
      else if (i == m_lSuns.Num()+1)
         pSun = &m_lMoon;
      if (!pSun->pSurface)
         continue;

      PCObjectSurface pos;
      pos = pSun->pSurface;

      if (pos->m_szScheme[0] && m_pWorld) {
         PCObjectSurface p2;
         p2 = (m_pWorld->SurfaceSchemeGet())->SurfaceGet (pos->m_szScheme, pos, TRUE);
         if (p2) {
            if (p2->m_fUseTextureMap) {
               ag[0] = p2->m_gTextureCode;
               ag[1] = p2->m_gTextureSub;
               plText->Add (ag);
            }
            // since noclone delete p2;
            continue;
         }
      }
      if (!pos->m_fUseTextureMap)
         continue;

      ag[0] = pos->m_gTextureCode;
      ag[1] = pos->m_gTextureSub;
      plText->Add (ag);
   }

   return TRUE;
}


/**********************************************************************************
CObjectSkydome::CalcAttribList - Fills in m_lAttrib so know what attributes the
object supports.
*/
#define ADDATTRIB(x,y) wcscpy(av.szName,x); av.fValue=(y); m_lAttrib.Add(&av)
void CObjectSkydome::CalcAttribList (void)
{
   m_lAttrib.Clear();
   ATTRIBVAL av;
   memset (&av, 0, sizeof(av));

   ADDATTRIB(L"Time: Year", 1);
   ADDATTRIB(L"Time: Month", 2);
   ADDATTRIB(L"Time: Hour", 3);

   ADDATTRIB(L"Atmosphere: Haze", 10);
   ADDATTRIB(L"Atmosphere: Density zenith", 11);
   ADDATTRIB(L"Atmosphere: Automatic colors", 12);

   ADDATTRIB(L"Atmosphere: Ground, red", 20);
   ADDATTRIB(L"Atmosphere: Ground, green", 21);
   ADDATTRIB(L"Atmosphere: Ground, blue", 22);

   ADDATTRIB(L"Atmosphere: Mid, red", 30);
   ADDATTRIB(L"Atmosphere: Mid, green", 31);
   ADDATTRIB(L"Atmosphere: Mid, blue", 32);

   ADDATTRIB(L"Atmosphere: Zenith, red", 40);
   ADDATTRIB(L"Atmosphere: Zenith, green", 41);
   ADDATTRIB(L"Atmosphere: Zenith, blue", 42);

   DWORD i;
   PWSTR psz;
   WCHAR szTemp[64];
   for (i = 0; i < CLOUDLAYERS; i++) {
      psz = i ? L"Clouds, altocumulus: " : L"Clouds, cirrus: ";

      wcscpy (szTemp, psz);
      wcscat (szTemp, L"Draw");
      ADDATTRIB (szTemp, 100 + i * 10 + 0);

      wcscpy (szTemp, psz);
      wcscat (szTemp, L"Coverage");
      ADDATTRIB (szTemp, 100 + i * 10 + 1);

      wcscpy (szTemp, psz);
      wcscat (szTemp, L"Thickness");
      ADDATTRIB (szTemp, 100 + i * 10 + 2);

      wcscpy (szTemp, psz);
      wcscat (szTemp, L"Location (X)");
      ADDATTRIB (szTemp, 100 + i * 10 + 3);

      wcscpy (szTemp, psz);
      wcscat (szTemp, L"Location (Y)");
      ADDATTRIB (szTemp, 100 + i * 10 + 4);

      wcscpy (szTemp, psz);
      wcscat (szTemp, L"Location (Z)");
      ADDATTRIB (szTemp, 100 + i * 10 + 5);

      wcscpy (szTemp, psz);
      wcscat (szTemp, L"Version");
      ADDATTRIB (szTemp, 100 + i * 10 + 6);
   }

   // cumulus clouds
   psz = L"Clouds, cumulus: ";
   wcscpy (szTemp, psz);
   wcscat (szTemp, L"Draw");
   ADDATTRIB (szTemp, 150 + 0);

   wcscpy (szTemp, psz);
   wcscat (szTemp, L"Coverage");
   ADDATTRIB (szTemp, 150 + 1);

   wcscpy (szTemp, psz);
   wcscat (szTemp, L"Thickness");
   ADDATTRIB (szTemp, 150 + 2);

   wcscpy (szTemp, psz);
   wcscat (szTemp, L"Sheer");
   ADDATTRIB (szTemp, 150 + 3);

   wcscpy (szTemp, psz);
   wcscat (szTemp, L"Sheer angle");
   ADDATTRIB (szTemp, 150 + 4);

   wcscpy (szTemp, psz);
   wcscat (szTemp, L"Location (X)");
   ADDATTRIB (szTemp, 150 + 5);

   wcscpy (szTemp, psz);
   wcscat (szTemp, L"Location (Y)");
   ADDATTRIB (szTemp, 150 + 6);

   wcscpy (szTemp, psz);
   wcscat (szTemp, L"Location (Z)");
   ADDATTRIB (szTemp, 150 + 7);

   wcscpy (szTemp, psz);
   wcscat (szTemp, L"Version");
   ADDATTRIB (szTemp, 150 + 8);


   PSKYDOMELIGHT ps;
   ps = (PSKYDOMELIGHT) m_lSuns.Get(0);
   for (i = 0; i < m_lSuns.Num(); i++, ps++) {
      WCHAR szName[64];
      wcscpy (szName, ps->szName);
      szName[32] = 0;   // just to make sure not too long
      if (!szName[0])
         swprintf (szName, L"Sun %d", (int) i);
      wcscat (szName, L": ");
      psz = szName;

      wcscpy (szTemp, psz);
      wcscat (szTemp, L"Visible");
      ADDATTRIB (szTemp, 1000 + 100 * i + 0);

      wcscpy (szTemp, psz);
      wcscat (szTemp, L"Location (X)");
      ADDATTRIB (szTemp, 1000 + 100 * i + 1);

      wcscpy (szTemp, psz);
      wcscat (szTemp, L"Location (Y)");
      ADDATTRIB (szTemp, 1000 + 100 * i + 2);

      wcscpy (szTemp, psz);
      wcscat (szTemp, L"Location (Z)");
      ADDATTRIB (szTemp, 1000 + 100 * i + 3);

      wcscpy (szTemp, psz);
      wcscat (szTemp, L"Emitted brightness");
      ADDATTRIB (szTemp, 1000 + 100 * i + 4);

      wcscpy (szTemp, psz);
      wcscat (szTemp, L"Emitted color, red");
      ADDATTRIB (szTemp, 1000 + 100 * i + 5);

      wcscpy (szTemp, psz);
      wcscat (szTemp, L"Emitted color, green");
      ADDATTRIB (szTemp, 1000 + 100 * i + 6);

      wcscpy (szTemp, psz);
      wcscat (szTemp, L"Emitted color, blue");
      ADDATTRIB (szTemp, 1000 + 100 * i + 7);

      wcscpy (szTemp, psz);
      wcscat (szTemp, L"Ambient brightness");
      ADDATTRIB (szTemp, 1000 + 100 * i + 8);

      wcscpy (szTemp, psz);
      wcscat (szTemp, L"Ambient color, red");
      ADDATTRIB (szTemp, 1000 + 100 * i + 9);

      wcscpy (szTemp, psz);
      wcscat (szTemp, L"Ambient color, green");
      ADDATTRIB (szTemp, 1000 + 100 * i + 10);

      wcscpy (szTemp, psz);
      wcscat (szTemp, L"Ambient color, blue");
      ADDATTRIB (szTemp, 1000 + 100 * i + 11);
   }


   // sort
   qsort (m_lAttrib.Get(0), m_lAttrib.Num(), sizeof(ATTRIBVAL), ATTRIBVALCompare);

}




/*****************************************************************************************
CObjectSkydome::AttribGetIntern - OVERRIDE THIS

Like AttribGet() except that only called if default attributes not handled.
*/
BOOL CObjectSkydome::AttribGetIntern (PWSTR pszName, fp *pfValue)
{
   ATTRIBVAL av;
   wcscpy (av.szName, pszName);

   // do search
   PATTRIBVAL pav;
   pav = (PATTRIBVAL) bsearch (&av, m_lAttrib.Get(0), m_lAttrib.Num(), sizeof(ATTRIBVAL), ATTRIBVALCompare);
   if (!pav)
      return NULL;

   DWORD dwIndex, dwLayer;
   dwIndex = (DWORD) pav->fValue;

   if ((dwIndex >= 1000) && (dwIndex < 2000)) { // suns
      dwLayer = (dwIndex - 1000) / 100;
      dwIndex = (dwIndex - 1000) % 100;
      PSKYDOMELIGHT pSun;
      pSun = (PSKYDOMELIGHT) m_lSuns.Get(dwLayer);
      if (!pSun)
         return FALSE;

      switch (dwIndex) {
      case 0:  // visible
         *pfValue = pSun->fVisible ? 1 : 0;
         break;
      case 1:  // location
      case 2:  // location
      case 3:  // location
         *pfValue = pSun->pDir.p[dwIndex-1];
         break;
      case 4:  // emit, brightness
         *pfValue = LumensToRange (pSun->fLumens);
         break;
      case 5: // lumens color
      case 6:
      case 7:
         *pfValue = pSun->awColor[dwIndex-5] / (fp)0xffff;
         break;
      case 8:  // ambient, brightness
         *pfValue = LumensToRange (pSun->fAmbientLumens);
         break;
      case 9: // ambient color
      case 10:
      case 11:
         *pfValue = pSun->awAmbientColor[dwIndex-9] / (fp)0xffff;
         break;
      default:
         return FALSE;
      }
   }
   else if ((dwIndex >= 150) && (dwIndex < 200)) { // cumuls
      dwIndex -= 150;
      switch (dwIndex) {
      case 0:  // draw
         *pfValue = m_fCumulusDraw ? 1 : 0;
         break;
      case 1:  // coverage
         *pfValue = m_fCumulusCover;
         break;
      case 2:  // thickness
         *pfValue = m_fCumulusThickness;
         break;
      case 3:  // sheer
         *pfValue = m_fCumulusSheer;
         break;
      case 4:  // sheer angle
         *pfValue = m_fCumulusSheerAngle;
         break;
      case 5:  // location
      case 6:
      case 7:
         *pfValue = m_pCumulusLoc.p[dwIndex-5];
         break;
      case 8:  // version
         *pfValue = m_dwCumulusSeed;
         break;
      default:
         return FALSE;
      }
   }
   else if ((dwIndex >= 100) && (dwIndex < 150)) { // cloud layers
      dwLayer = (dwIndex - 100) / 10;
      dwIndex = (dwIndex - 100) % 10;

      switch (dwIndex) {
      case 0:  // draw
         *pfValue = m_afCirrusDraw[dwLayer] ? 1 : 0;
         break;
      case 1:  // coverage
         *pfValue = m_afCirrusCover[dwLayer];
         break;
      case 2:  // thickness
         *pfValue = m_afCirrusThickness[dwLayer];
         break;
      case 3:  // location
      case 4:
      case 5:
         *pfValue = m_apCirrusLoc[dwLayer].p[dwIndex-3];
         break;
      case 6:  // version
         *pfValue = m_adwCirrusSeed[dwLayer];
         break;
      default:
         return FALSE;
      }
   }
   else { // object
      switch (dwIndex) {
      case 1:  // year
         *pfValue = m_fYear;
         break;
      case 2:  // month
         *pfValue = m_fTimeInYear;
         break;
      case 3:  // hour
         *pfValue = m_fTimeInDay;
         break;
      case 10: // haze
         *pfValue = m_fHaze;
         break;
      case 11: // density zenith
         *pfValue = m_fDensityZenith;
         break;
      case 12: // auto colors
         *pfValue = m_fAutoColors ? 1 : 0;
         break;
      case 20: // ground color, red
         *pfValue = GetRValue(m_cAtmosphereGround) / 255.0;
         break;
      case 21: // ground color, green
         *pfValue = GetGValue(m_cAtmosphereGround) / 255.0;
         break;
      case 22: // ground color, blue
         *pfValue = GetBValue(m_cAtmosphereGround) / 255.0;
         break;
      case 30: // Mid color, red
         *pfValue = GetRValue(m_cAtmosphereMid) / 255.0;
         break;
      case 31: // Mid color, green
         *pfValue = GetGValue(m_cAtmosphereMid) / 255.0;
         break;
      case 32: // Mid color, blue
         *pfValue = GetBValue(m_cAtmosphereMid) / 255.0;
         break;
      case 40: // Zenith color, red
         *pfValue = GetRValue(m_cAtmosphereZenith) / 255.0;
         break;
      case 41: // Zenith color, green
         *pfValue = GetGValue(m_cAtmosphereZenith) / 255.0;
         break;
      case 42: // Zenith color, blue
         *pfValue = GetBValue(m_cAtmosphereZenith) / 255.0;
         break;

      default:
         return FALSE;
      }
   }

   return TRUE;
}


/*****************************************************************************************
CObjectSkydome::AttribGetAllIntern - OVERRIDE THIS

Like AttribGetAllIntern() EXCEPT plATTRIBVAL is already initialized and filled with
some parameters (default to the object template)
*/
void CObjectSkydome::AttribGetAllIntern (PCListFixed plATTRIBVAL)
{
   DWORD i;
   PATTRIBVAL pav, pav2;
   pav = (PATTRIBVAL) m_lAttrib.Get(0);
   fp fValue;
   for (i = 0; i < m_lAttrib.Num(); i++, pav++) {
      if (!AttribGetIntern (pav->szName, &fValue))
         continue;

      // add and set the value
      plATTRIBVAL->Add (pav);
      pav2 = (PATTRIBVAL) plATTRIBVAL->Get(plATTRIBVAL->Num()-1);
      pav2->fValue = fValue;
   }
}


/*****************************************************************************************
CObjectSkydome::AttribSetGroupIntern - OVERRIDE THIS

Like AttribSetGroup() except passing on non-template attributes.
*/
void CObjectSkydome::AttribSetGroupIntern (DWORD dwNum, PATTRIBVAL paAttrib)
{
   BOOL fChanged = FALSE;

#define CHANGED if (!fChanged && m_pWorld) m_pWorld->ObjectAboutToChange(this); fChanged = TRUE

   DWORD i;
   PATTRIBVAL pf;
   PSKYDOMELIGHT pSun;
   fp fValue;
   for (i = 0; i < dwNum; i++, paAttrib++) {
      // see if can find attribute
      pf = (PATTRIBVAL) bsearch (paAttrib, m_lAttrib.Get(0), m_lAttrib.Num(), sizeof(ATTRIBVAL), ATTRIBVALCompare);
      if (!pf)
         continue;

      // ID?
      DWORD dwIndex, dwLayer;
      dwIndex = (DWORD) pf->fValue;
      fValue = paAttrib->fValue;

      if ((dwIndex >= 1000) && (dwIndex < 2000)) { // suns
         dwLayer = (dwIndex - 1000) / 100;
         dwIndex = (dwIndex - 1000) % 100;
         pSun = (PSKYDOMELIGHT) m_lSuns.Get(dwLayer);
         if (!pSun)
            continue;

         switch (dwIndex) {
         case 0:  // visible
            if (fValue != (pSun->fVisible ? 1 : 0)) {
               CHANGED;
               pSun->fVisible = (fValue >= .5);
            }
            break;
         case 1:  // location
         case 2:  // location
         case 3:  // location
            if (fValue != pSun->pDir.p[dwIndex-1]) {
               CHANGED;
               pSun->pDir.p[dwIndex-1] = fValue;
            }
            break;
         case 4:  // emit, brightness
            if (fValue != LumensToRange (pSun->fLumens)) {
               CHANGED;
               pSun->fLumens = RangeToLumens (fValue);
            }
            break;
         case 5: // lumens color
         case 6:
         case 7:
            fValue *= (fp)0xffff;
            fValue = max(0,fValue);
            fValue = min(0xffff,fValue);
            if (fValue != pSun->awColor[dwIndex-5]) {
               CHANGED;
               pSun->awColor[dwIndex-5] = fValue;
            }
            break;
         case 8:  // ambient, brightness
            if (fValue != LumensToRange (pSun->fAmbientLumens)) {
               CHANGED;
               pSun->fAmbientLumens = RangeToLumens (fValue);
            }
            break;
         case 9: // ambient color
         case 10:
         case 11:
            fValue *= (fp)0xffff;
            fValue = max(0,fValue);
            fValue = min(0xffff,fValue);
            if (fValue != pSun->awAmbientColor[dwIndex-9]) {
               CHANGED;
               pSun->awAmbientColor[dwIndex-9] = fValue;
            }
            break;
         default:
            continue;
         }
      }
      else if ((dwIndex >= 150) && (dwIndex < 200)) { // cumuls
         dwIndex -= 150;
         switch (dwIndex) {
         case 0:  // draw
            if (fValue != (m_fCumulusDraw ? 1 : 0)) {
               CHANGED;
               m_fCumulusDraw = (fValue >= .5);
            }
            break;
         case 1:  // coverage
            fValue = min(1,fValue);
            fValue = max(0, fValue);
            if (fValue != m_fCumulusCover) {
               CHANGED;
               m_fCumulusCover = fValue;
            }
            break;
         case 2:  // thickness
            fValue = min(1,fValue);
            fValue = max(0, fValue);
            if (fValue != m_fCumulusThickness) {
               CHANGED;
               m_fCumulusThickness = fValue;
            }
            break;
         case 3:  // sheer
            fValue = min(1,fValue);
            fValue = max(0, fValue);
            if (fValue != m_fCumulusSheer) {
               CHANGED;
               m_fCumulusSheer = fValue;
            }
            break;
         case 4:  // sheer angle
            if (fValue != m_fCumulusSheerAngle) {
               CHANGED;
               m_fCumulusSheerAngle = fValue;
            }
            break;
         case 5:  // location
         case 6:
         case 7:
            if (fValue != m_pCumulusLoc.p[dwIndex-5]) {
               CHANGED;
               m_pCumulusLoc.p[dwIndex-5] = fValue;
               if (dwIndex==5)
                  m_pCumulusLoc.p[dwIndex-5] = max(1, m_pCumulusLoc.p[dwIndex-5]);
            };
            break;
         case 8:  // version
            if ((DWORD) fValue != m_dwCumulusSeed) {
               CHANGED;
               m_dwCumulusSeed = (DWORD) fValue;
            };
            break;
         default:
            continue;
         }
      }
      else if ((dwIndex >= 100) && (dwIndex < 150)) { // cloud layers
         dwLayer = (dwIndex - 100) / 10;
         dwIndex = (dwIndex - 100) % 10;

         switch (dwIndex) {
         case 0:  // draw
            if (fValue != (m_afCirrusDraw[dwLayer] ? 1 : 0)) {
               CHANGED;
               m_afCirrusDraw[dwLayer] = (fValue >= .5);
            }
            break;
         case 1:  // coverage
            fValue = min(1,fValue);
            fValue = max(0, fValue);
            if (fValue != m_afCirrusCover[dwLayer]) {
               CHANGED;
               m_afCirrusCover[dwLayer] = fValue;
            }
            break;
         case 2:  // thickness
            fValue = min(1,fValue);
            fValue = max(0, fValue);
            if (fValue != m_afCirrusThickness[dwLayer]) {
               CHANGED;
               m_afCirrusThickness[dwLayer] = fValue;
            }
            break;
         case 3:  // location
         case 4:
         case 5:
            if (fValue != m_apCirrusLoc[dwLayer].p[dwIndex-3]) {
               CHANGED;
               m_apCirrusLoc[dwLayer].p[dwIndex-3] = fValue;
               if (dwIndex == 5)
                  m_apCirrusLoc[dwLayer].p[dwIndex-3] = max(m_apCirrusLoc[dwLayer].p[dwIndex-3], 1);
            }
            break;
         case 6:  // version
            if ((DWORD)fValue != m_adwCirrusSeed[dwLayer]) {
               CHANGED;
               m_adwCirrusSeed[dwLayer] = (DWORD) fValue;
            }
            break;
         default:
            continue;
         }
      }
      else { // object
         switch (dwIndex) {
         case 1:  // year
            if (fValue != m_fYear) {
               CHANGED;
               m_fYear = fValue;
            }
            break;
         case 2:  // month
            if (fValue != m_fTimeInYear) {
               CHANGED;
               m_fTimeInYear = fValue;
            }
            break;
         case 3:  // hour
            if (fValue != m_fTimeInDay) {
               CHANGED;
               m_fTimeInDay = fValue;
            }
            break;
         case 10: // haze
            fValue = min(10,fValue);
            fValue = max(0, fValue);
            if (fValue != m_fHaze) {
               CHANGED;
               m_fHaze = fValue;
            }
            break;
         case 11: // density zenith
            fValue = min(.30,fValue);
            fValue = max(.01, fValue);
            if (fValue != m_fDensityZenith) {
               CHANGED;
               m_fDensityZenith = fValue;
            }
            break;
         case 12: // auto colors
            if (fValue != (m_fAutoColors ? 1 : 0)) {
               CHANGED;
               m_fAutoColors = (fValue >= .5);
            }
            break;
         case 20: // ground color, red
            fValue *= 255;
            fValue = min(255,fValue);
            fValue = max(0, fValue);
            if ((BYTE)fValue != GetRValue(m_cAtmosphereGround)) {
               CHANGED;
               m_cAtmosphereGround = RGB((BYTE)fValue,
                  GetGValue(m_cAtmosphereGround),
                  GetBValue(m_cAtmosphereGround));
            }
            break;
         case 21: // ground color, green
            fValue *= 255;
            fValue = min(255,fValue);
            fValue = max(0, fValue);
            if ((BYTE)fValue != GetGValue(m_cAtmosphereGround)) {
               CHANGED;
               m_cAtmosphereGround = RGB(
                  GetRValue(m_cAtmosphereGround),
                  (BYTE)fValue,
                  GetBValue(m_cAtmosphereGround));
            }
            break;
         case 22: // ground color, blue
            fValue *= 255;
            fValue = min(255,fValue);
            fValue = max(0, fValue);
            if ((BYTE)fValue != GetBValue(m_cAtmosphereGround)) {
               CHANGED;
               m_cAtmosphereGround = RGB(
                  GetRValue(m_cAtmosphereGround),
                  GetGValue(m_cAtmosphereGround),
                  (BYTE)fValue);
            }
            break;
         case 30: // Mid color, red
            fValue *= 255;
            fValue = min(255,fValue);
            fValue = max(0, fValue);
            if ((BYTE)fValue != GetRValue(m_cAtmosphereMid)) {
               CHANGED;
               m_cAtmosphereMid = RGB((BYTE)fValue,
                  GetGValue(m_cAtmosphereMid),
                  GetBValue(m_cAtmosphereMid));
            }
            break;
         case 31: // Mid color, green
            fValue *= 255;
            fValue = min(255,fValue);
            fValue = max(0, fValue);
            if ((BYTE)fValue != GetGValue(m_cAtmosphereMid)) {
               CHANGED;
               m_cAtmosphereMid = RGB(
                  GetRValue(m_cAtmosphereMid),
                  (BYTE)fValue,
                  GetBValue(m_cAtmosphereMid));
            }
            break;
         case 32: // Mid color, blue
            fValue *= 255;
            fValue = min(255,fValue);
            fValue = max(0, fValue);
            if ((BYTE)fValue != GetBValue(m_cAtmosphereMid)) {
               CHANGED;
               m_cAtmosphereMid = RGB(
                  GetRValue(m_cAtmosphereMid),
                  GetGValue(m_cAtmosphereMid),
                  (BYTE)fValue);
            }
            break;
         case 40: // Zenith color, red
            fValue *= 255;
            fValue = min(255,fValue);
            fValue = max(0, fValue);
            if ((BYTE)fValue != GetRValue(m_cAtmosphereZenith)) {
               CHANGED;
               m_cAtmosphereZenith = RGB((BYTE)fValue,
                  GetGValue(m_cAtmosphereZenith),
                  GetBValue(m_cAtmosphereZenith));
            }
            break;
         case 41: // Zenith color, green
            fValue *= 255;
            fValue = min(255,fValue);
            fValue = max(0, fValue);
            if ((BYTE)fValue != GetGValue(m_cAtmosphereZenith)) {
               CHANGED;
               m_cAtmosphereZenith = RGB(
                  GetRValue(m_cAtmosphereZenith),
                  (BYTE)fValue,
                  GetBValue(m_cAtmosphereZenith));
            }
            break;
         case 42: // Zenith color, blue
            fValue *= 255;
            fValue = min(255,fValue);
            fValue = max(0, fValue);
            if ((BYTE)fValue != GetBValue(m_cAtmosphereZenith)) {
               CHANGED;
               m_cAtmosphereZenith = RGB(
                  GetRValue(m_cAtmosphereZenith),
                  GetGValue(m_cAtmosphereZenith),
                  (BYTE)fValue);
            }
            break;

         default:
            continue;
         }
      }

   }

   if (fChanged) {
      // just make sure that direction is normalized
      pSun = (PSKYDOMELIGHT) m_lSuns.Get(0);
      for (i = 0; i < m_lSuns.Num(); i++, pSun++)
         pSun->pDir.Normalize(); // just to make sure

      // recalc
      CalcAll();

      if (m_pWorld)
         m_pWorld->ObjectChanged (this);
   }
}


/*****************************************************************************************
CObjectSkydome::AttribInfoIntern - OVERRIDE THIS

Like AttribInfo() except called if attribute is not for template.
*/
BOOL CObjectSkydome::AttribInfoIntern (PWSTR pszName, PATTRIBINFO pInfo)
{
   ATTRIBVAL av;
   wcscpy (av.szName, pszName);

   // do search
   PATTRIBVAL pav;
   pav = (PATTRIBVAL) bsearch (&av, m_lAttrib.Get(0), m_lAttrib.Num(), sizeof(ATTRIBVAL), ATTRIBVALCompare);
   if (!pav)
      return FALSE;

   memset (pInfo, 0, sizeof(*pInfo));
   pInfo->dwType = AIT_NUMBER;
   pInfo->fDefPassUp = FALSE;
   pInfo->fDefLowRank = FALSE;

   DWORD dwIndex, dwLayer;
   dwIndex = (DWORD) pav->fValue;

   if ((dwIndex >= 1000) && (dwIndex < 2000)) { // suns
      dwLayer = (dwIndex - 1000) / 100;
      dwIndex = (dwIndex - 1000) % 100;
      PSKYDOMELIGHT pSun;
      pSun = (PSKYDOMELIGHT) m_lSuns.Get(dwLayer);
      if (!pSun)
         return FALSE;

      switch (dwIndex) {
      case 0:  // visible
         pInfo->dwType = AIT_BOOL;
         pInfo->fMin = 0;
         pInfo->fMax = 1;
         break;
      case 1:  // location
      case 2:  // location
      case 3:  // location
         pInfo->fMin = -1;
         pInfo->fMax = 1;
         break;
      case 4:  // emit, brightness
      case 5: // lumens color
      case 6:
      case 7:
      case 8:  // ambient, brightness
      case 9: // ambient color
      case 10:
      case 11:
         pInfo->fMin = 0;
         pInfo->fMax = 1;
         break;
      default:
         return FALSE;
      }
   }
   else if ((dwIndex >= 150) && (dwIndex < 200)) { // cumuls
      dwIndex -= 150;
      switch (dwIndex) {
      case 0:  // draw
         pInfo->dwType = AIT_BOOL;
         pInfo->fMin = 0;
         pInfo->fMax = 1;
         break;
      case 1:  // coverage
      case 2:  // thickness
      case 3:  // sheer
         pInfo->fMin = 0;
         pInfo->fMax = 1;
         break;
      case 4:  // sheer angle
         pInfo->dwType = AIT_ANGLE;
         pInfo->fMin = -PI;
         pInfo->fMax = PI;
         break;
      case 5:  // location
      case 6:
         pInfo->dwType = AIT_DISTANCE;
         pInfo->fMin = -10000;
         pInfo->fMax = 10000;
         break;
      case 7:
         pInfo->dwType = AIT_DISTANCE;
         pInfo->fMin = 1;
         pInfo->fMax = 30000;
         break;
      case 8:  // version
         pInfo->fMin = 0;
         pInfo->fMax = 100;
         break;
      default:
         return FALSE;
      }
   }
   else if ((dwIndex >= 100) && (dwIndex < 150)) { // cloud layers
      dwLayer = (dwIndex - 100) / 10;
      dwIndex = (dwIndex - 100) % 10;

      switch (dwIndex) {
      case 0:  // draw
         pInfo->dwType = AIT_BOOL;
         pInfo->fMin = 0;
         pInfo->fMax = 1;
         break;
      case 1:  // coverage
      case 2:  // thickness
         pInfo->fMin = 0;
         pInfo->fMax = 1;
         break;
      case 3:  // location
      case 4:
         pInfo->dwType = AIT_DISTANCE;
         pInfo->fMin = -10000;
         pInfo->fMax = 10000;
         break;
      case 5:
         pInfo->dwType = AIT_DISTANCE;
         pInfo->fMin = 1;
         pInfo->fMax = 30000;
         break;
      case 6:  // version
         pInfo->fMin = 0;
         pInfo->fMax = 100;
         break;
      default:
         return FALSE;
      }
   }
   else { // object
      switch (dwIndex) {
      case 1:  // year
         pInfo->fMin = 1900;
         pInfo->fMax = 2100;
         break;
      case 2:  // month
         pInfo->fMin = 0;
         pInfo->fMax = 12;
         break;
      case 3:  // hour
         pInfo->fMin = 0;
         pInfo->fMax = 24;
         break;
      case 10: // haze
         pInfo->fMin = 0;
         pInfo->fMax = 10;
         break;
      case 11: // density zenith
         pInfo->fMin = .01;
         pInfo->fMax = .30;
         break;
      case 12: // auto colors
         pInfo->dwType = AIT_BOOL;
         pInfo->fMin = 0;
         pInfo->fMax = 1;
         break;
      case 20: // ground color, red
      case 21: // ground color, green
      case 22: // ground color, blue
      case 30: // Mid color, red
      case 31: // Mid color, green
      case 32: // Mid color, blue
      case 40: // Zenith color, red
      case 41: // Zenith color, green
      case 42: // Zenith color, blue
         pInfo->fMin = 0;
         pInfo->fMax = 1;
         break;

      default:
         return FALSE;
      }
   }

   return TRUE;
}

/*****************************************************************************************
CObjectSkydome::Deconstruct - Standard call
*/
BOOL CObjectSkydome::Deconstruct (BOOL fAct)
{
   return FALSE;
}



// BUGBUG - Option for skydome resolution should be off the main skydome dialog,
// not under atmosphere

// BUGBUG - might want to save skydome settings in the texture so that when
// rendering a virtual world and coontinually get skydome date/time set and then
// undo, wont recalculate the texture
