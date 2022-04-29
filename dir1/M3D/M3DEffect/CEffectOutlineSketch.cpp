/*********************************************************************************
CNPREffectOutlineSketch.cpp - Code for effect

begun 27/4/2004
Copyright 2004 by Mike Rozak
All rights reserved
*/

#include <windows.h>
#include "escarpment.h"
#include "..\M3D.h"
#include "resource.h"



#define STROKEANTI      2        // how much to antialias strokes.

// STROKEINFO - for sorting strokes
typedef struct {
   WORD        wX;         // x coord
   WORD        wY;         // y coord
   float       fZ;         // Z depth
} STROKEINFO, *PSTROKEINFO;

// EMTOUTLINESKETCH - Multhtreaded outline info
typedef struct {
   DWORD          dwStart;
   DWORD          dwEnd;
   PSTROKEINFO    psi;  // stroke info
   PCImage        pImageSrc;
   PCImage        pImageDest;
   PCFImage       pFImageSrc;
   PCFImage       pFImageDest;
   PCPaintingStroke paStroke;
   PBYTE          pbOutline;
   DWORD          dwLevel;
} EMTOUTLINESKETCH, *PEMTOUTLINESKETCH;



// OUTLINESKETCHPAGE - Page info
typedef struct {
   PCNPREffectOutlineSketch pe;   // effects list
   int            iVScroll;      // inital scroll
   HBITMAP        hBit;    // bitmap for the image
   PCImage        pTest;   // test image
   PCRenderSuper  pRender; // renderer that generated test image
   PCWorldSocket  pWorld;  // world for image
   BOOL           fAllEffects;   // if TRUE then show all effects combined
   PCNPREffectsList  pAllEffects;   // shows all effects
   DWORD          dwTab;   // tab to view
} OUTLINESKETCHPAGE, *POUTLINESKETCHPAGE;


PWSTR gpszEffectOutlineSketch = L"OutlineSketch";


/*********************************************************************************
CNPREffectOutlineSketch::Constructor and destructor
*/
CNPREffectOutlineSketch::CNPREffectOutlineSketch (DWORD dwRenderShard)
{
   m_dwRenderShard = dwRenderShard;
   m_fStrokeRandom = 0.2;

   m_fStrokeLen = 5;
   m_fStrokeStep = 2;
   m_fStrokeAnchor = 0.2;
   m_fStrokeLenVar = 0.5;
   m_fStrokePenColor = 0.5;
   m_fStrokePenObject = 0.5;

   m_pStrokeWidth.p[0] = .5;
   m_pStrokeWidth.p[1] = 1;
   m_pStrokeWidth.p[2] = .5;
   m_pStrokeWidth.p[3] = .2;

   m_pColorVar.Zero();
   m_pColorVar.p[0] = m_pColorVar.p[1] = m_pColorVar.p[2] = .1;
   m_pColorPalette.Zero();
   m_fColorHueShift = 0;
   m_fColorUseFixed = 0;
   m_acColorFixed[0] = RGB(0,0,0);
   m_acColorFixed[1] = RGB(0xff,0xff,0xff);
   m_acColorFixed[2] = RGB(0xff,0,0);
   m_acColorFixed[3] = RGB(0,0xff,0);
   m_acColorFixed[4] = RGB(0,0,0xff);


   m_dwBrushShape = 0;
   m_pBrushParam.Zero();
   m_pBrushParam.p[2] = m_pBrushParam.p[3] = 0.5;  // scaling and contrast

   m_fSpyglassScale = 1;
   m_fSpyglassBlur = .3;
   m_fSpyglassStickOut = 0;
   m_dwSpyglassShape = 0;

   m_dwLevel = 3;
   m_fStrokeLenMin = 2;
   m_fOvershoot = 0;
   m_fMaxAngle = .3;
   m_fStrokePrefAngle = PI/4;

   m_cOutlineColor = RGB(0,0,0);
   m_fOutlineBlend = 0.5;

   DWORD dwThread;
   for (dwThread = 0; dwThread < MAXRAYTHREAD; dwThread++) {
      //m_alPathPOINT[dwThread].Init (sizeof(POINT));
      m_alPathDWORD[dwThread].Init (sizeof(DWORD));
   }
}

CNPREffectOutlineSketch::~CNPREffectOutlineSketch (void)
{
   // do nothing for now
}


/*********************************************************************************
CNPREffectOutlineSketch::Delete - From CNPREffect
*/
void CNPREffectOutlineSketch::Delete (void)
{
   delete this;
}


/*********************************************************************************
CNPREffectOutlineSketch::QueryInfo - From CNPREffect
*/
void CNPREffectOutlineSketch::QueryInfo (PNPRQI pqi)
{
   memset (pqi, 0, sizeof(*pqi));
   pqi->fInPlace = FALSE;
   pqi->pszDesc = L"Draws a sketched outline around objects.";
   pqi->pszName = L"Outline (sketch)";
   pqi->pszID = gpszEffectOutlineSketch;
}



static PWSTR gpszIgnoreBackground = L"IgnoreBackground";
static PWSTR gpszTextBack = L"TextBack";
static PWSTR gpszTextColor0 = L"TextColor0";
static PWSTR gpszTextColor1 = L"TextColor1";
static PWSTR gpszTextWidth = L"TextWidth";
static PWSTR gpszTextSurf = L"TextSurf";
static PWSTR gpszStrokesNum = L"StrokesNum";
static PWSTR gpszStrokeMomentum = L"StrokeMomentum";
static PWSTR gpszStrokeRandom = L"StrokeRandom";
static PWSTR gpszStrokeZWeight = L"StrokeZWeight";
static PWSTR gpszStrokeColorWeight = L"StrokeColorWeight";
static PWSTR gpszStrokeObjectWeight = L"StrokeObjectWeight";
static PWSTR gpszStrokePrefWeight = L"StrokePrefWeight";
static PWSTR gpszStrokePrefAngle = L"StrokePrefAngle";
static PWSTR gpszStrokeZPerp = L"StrokeZPerp";
static PWSTR gpszStrokeColorPerp = L"StrokeColorPerp";
static PWSTR gpszStrokeObjectPerp = L"StrokeObjectPerp";
static PWSTR gpszStrokeCrosshatch = L"StrokeCrosshatch";
static PWSTR gpszStrokePrefEither = L"StrokePrefEither";
static PWSTR gpszStrokeZBlur = L"StrokeZBlur";
static PWSTR gpszStrokeColorBlur = L"StrokeColorBlur";
static PWSTR gpszStrokeObjectBlur = L"StrokeObjectBlur";
static PWSTR gpszStrokeLen = L"StrokeLen";
static PWSTR gpszStrokeStep = L"StrokeStep";
static PWSTR gpszStrokeAnchor = L"StrokeAnchor";
static PWSTR gpszStrokeLenVar = L"StrokeLenVar";
static PWSTR gpszStrokePenColor = L"StrokePenColor";
static PWSTR gpszStrokePenObject = L"StrokePenObject";
static PWSTR gpszStrokeWidth = L"StrokeWidth";
static PWSTR gpszColorVar = L"ColorVar";
static PWSTR gpszColorPalette = L"ColorPalette";
static PWSTR gpszColorHueShift = L"ColorHueShift";
static PWSTR gpszColorUseFixed = L"ColorUseFixed";
static PWSTR gpszBrushShape = L"BrushShape";
static PWSTR gpszBrushParam = L"BrushParam";
static PWSTR gpszDeltaFilterWidth = L"DeltaFilterWidth";
static PWSTR gpszBackMatch = L"BackMatch";
static PWSTR gpszBackColor = L"BackColor";
static PWSTR gpszSpyglassScale = L"SpyglassScale";
static PWSTR gpszSpyglassStickOut = L"SpyglassStickOut";
static PWSTR gpszSpyglassBlur = L"SpyglassBlur";
static PWSTR gpszSpyglassShape = L"SpyglassShape";
static PWSTR gpszLevel = L"Level";
static PWSTR gpszStrokeLenMin = L"StrokeLenMin";
static PWSTR gpszOvershoot = L"Overshoot";
static PWSTR gpszMaxAngle = L"MaxAngle";
static PWSTR gpszOutlineColor = L"OutlineColor";
static PWSTR gpszOutlineBlend = L"OutlineBlend";

/*********************************************************************************
CNPREffectOutlineSketch::MMLTo - From CNPREffect
*/
PCMMLNode2 CNPREffectOutlineSketch::MMLTo (void)
{
   PCMMLNode2 pNode = new CMMLNode2;
   if (!pNode)
      return NULL;
   pNode->NameSet (gpszEffectOutlineSketch);


   MMLValueSet (pNode, gpszSpyglassScale, m_fSpyglassScale);
   MMLValueSet (pNode, gpszSpyglassBlur, m_fSpyglassBlur);
   MMLValueSet (pNode, gpszSpyglassStickOut, m_fSpyglassStickOut);
   MMLValueSet (pNode, gpszSpyglassShape, (int)m_dwSpyglassShape);

   MMLValueSet (pNode, gpszStrokeRandom, m_fStrokeRandom);

   MMLValueSet (pNode, gpszLevel, (int) m_dwLevel);
   MMLValueSet (pNode, gpszStrokeLenMin, m_fStrokeLenMin);
   MMLValueSet (pNode, gpszOvershoot, m_fOvershoot);
   MMLValueSet (pNode, gpszMaxAngle, m_fMaxAngle);
   MMLValueSet (pNode, gpszStrokePrefAngle, m_fStrokePrefAngle);

   MMLValueSet (pNode, gpszOutlineColor, (int) m_cOutlineColor);
   MMLValueSet (pNode, gpszOutlineBlend, m_fOutlineBlend);

   MMLValueSet (pNode, gpszStrokeLen, m_fStrokeLen);
   MMLValueSet (pNode, gpszStrokeStep, m_fStrokeStep);
   MMLValueSet (pNode, gpszStrokeAnchor, m_fStrokeAnchor);
   MMLValueSet (pNode, gpszStrokeLenVar, m_fStrokeLenVar);
   MMLValueSet (pNode, gpszStrokePenColor, m_fStrokePenColor);
   MMLValueSet (pNode, gpszStrokePenObject, m_fStrokePenObject);

   MMLValueSet (pNode, gpszStrokeWidth, &m_pStrokeWidth);

   MMLValueSet (pNode, gpszColorVar, &m_pColorVar);
   MMLValueSet (pNode, gpszColorPalette, &m_pColorPalette);
   MMLValueSet (pNode, gpszColorHueShift, m_fColorHueShift);
   MMLValueSet (pNode, gpszColorUseFixed, (int)m_fColorUseFixed);
   DWORD i;
   WCHAR szTemp[64];
   for (i = 0; i < 5; i++) {
      swprintf (szTemp, L"ColorFixed%d", (int) i);
      MMLValueSet (pNode, szTemp, (int)m_acColorFixed[i]);
   }

   MMLValueSet (pNode, gpszBrushShape, (int)m_dwBrushShape);
   MMLValueSet (pNode, gpszBrushParam, &m_pBrushParam);

   return pNode;
}


/*********************************************************************************
CNPREffectOutlineSketch::MMLFrom - From CNPREffect
*/
BOOL CNPREffectOutlineSketch::MMLFrom (PCMMLNode2 pNode)
{

   m_fSpyglassScale = MMLValueGetDouble (pNode, gpszSpyglassScale, 1);
   m_fSpyglassBlur = MMLValueGetDouble (pNode, gpszSpyglassBlur, 0);
   m_fSpyglassStickOut = MMLValueGetDouble (pNode, gpszSpyglassStickOut, 0);
   m_dwSpyglassShape = (DWORD) MMLValueGetInt (pNode, gpszSpyglassShape, (int)0);

   m_fStrokeRandom = MMLValueGetDouble (pNode, gpszStrokeRandom, 0.5);

   m_dwLevel = MMLValueGetInt (pNode, gpszLevel, (int) 1);
   m_fStrokeLenMin = MMLValueGetDouble (pNode, gpszStrokeLenMin, 2);
   m_fOvershoot = MMLValueGetDouble (pNode, gpszOvershoot, 0);
   m_fMaxAngle = MMLValueGetDouble (pNode, gpszMaxAngle, .3);
   m_fStrokePrefAngle = MMLValueGetDouble (pNode, gpszStrokePrefAngle, PI/4);

   m_cOutlineColor = (COLORREF) MMLValueGetInt (pNode, gpszOutlineColor, (int) 0);
   m_fOutlineBlend = MMLValueGetDouble (pNode, gpszOutlineBlend, .5);

   m_fStrokeLen = MMLValueGetDouble (pNode, gpszStrokeLen, 5);
   m_fStrokeStep = MMLValueGetDouble (pNode, gpszStrokeStep, 2);
   m_fStrokeAnchor = MMLValueGetDouble (pNode, gpszStrokeAnchor, 0);
   m_fStrokeLenVar = MMLValueGetDouble (pNode, gpszStrokeLenVar, .5);
   m_fStrokePenColor = MMLValueGetDouble (pNode, gpszStrokePenColor, .5);
   m_fStrokePenObject = MMLValueGetDouble (pNode, gpszStrokePenObject, .5);

   MMLValueGetPoint (pNode, gpszStrokeWidth, &m_pStrokeWidth);

   MMLValueGetPoint (pNode, gpszColorVar, &m_pColorVar);
   MMLValueGetPoint (pNode, gpszColorPalette, &m_pColorPalette);
   m_fColorHueShift = MMLValueGetDouble (pNode, gpszColorHueShift, 0);
   m_fColorUseFixed = (BOOL) MMLValueGetInt (pNode, gpszColorUseFixed, (int)0);
   DWORD i;
   WCHAR szTemp[64];
   for (i = 0; i < 5; i++) {
      swprintf (szTemp, L"ColorFixed%d", (int) i);
      m_acColorFixed[i] = (COLORREF) MMLValueGetInt (pNode, szTemp, (int)m_acColorFixed[i]);
   }

   m_dwBrushShape = (DWORD)MMLValueGetInt (pNode, gpszBrushShape, 0);
   MMLValueGetPoint (pNode, gpszBrushParam, &m_pBrushParam);

   return TRUE;
}




/*********************************************************************************
CNPREffectOutlineSketch::MMLFrom - From CNPREffect
*/
CNPREffect * CNPREffectOutlineSketch::Clone (void)
{
   return CloneEffect ();
}

/*********************************************************************************
CNPREffectOutlineSketch::CloneEffect - From CNPREffect
*/
CNPREffectOutlineSketch * CNPREffectOutlineSketch::CloneEffect (void)
{
   PCNPREffectOutlineSketch pNew = new CNPREffectOutlineSketch(m_dwRenderShard);
   if (!pNew)
      return NULL;


   pNew->m_fStrokeRandom = m_fStrokeRandom;

   pNew->m_fStrokeLen = m_fStrokeLen;
   pNew->m_fStrokeStep = m_fStrokeStep;
   pNew->m_fStrokeAnchor = m_fStrokeAnchor;
   pNew->m_fStrokeLenVar = m_fStrokeLenVar;
   pNew->m_fStrokePenColor = m_fStrokePenColor;
   pNew->m_fStrokePenObject = m_fStrokePenObject;

   pNew->m_fSpyglassScale = m_fSpyglassScale;
   pNew->m_fSpyglassBlur = m_fSpyglassBlur;
   pNew->m_fSpyglassStickOut = m_fSpyglassStickOut;
   pNew->m_dwSpyglassShape = m_dwSpyglassShape;

   pNew->m_cOutlineColor =m_cOutlineColor;
   pNew->m_fOutlineBlend = m_fOutlineBlend;

   pNew->m_dwLevel = m_dwLevel;
   pNew->m_fStrokeLenMin = m_fStrokeLenMin;
   pNew->m_fOvershoot = m_fOvershoot;
   pNew->m_fMaxAngle = m_fMaxAngle;
   pNew->m_fStrokePrefAngle = m_fStrokePrefAngle;

   pNew->m_pStrokeWidth.Copy (&m_pStrokeWidth);

   pNew->m_pColorVar.Copy (&m_pColorVar);
   pNew->m_pColorPalette.Copy (&m_pColorPalette);
   pNew->m_fColorHueShift = m_fColorHueShift;
   pNew->m_fColorUseFixed = m_fColorUseFixed;
   memcpy (&pNew->m_acColorFixed, &m_acColorFixed, sizeof(m_acColorFixed));

   pNew->m_dwBrushShape = m_dwBrushShape;
   pNew->m_pBrushParam.Copy (&m_pBrushParam);

   return pNew;
}






static int __cdecl STROKEINFOCompare (const void *p1, const void *p2)
{
   PSTROKEINFO ph1 = (PSTROKEINFO)p1;
   PSTROKEINFO ph2 = (PSTROKEINFO)p2;

   if (ph1->fZ > ph2->fZ)
      return -1;
   else if (ph1->fZ < ph2->fZ)
      return 1;
   else
      return 0;
}


/*********************************************************************************
CNPREffectOutlineSketch::EscMultiThreadedCallback - Handles multithreaded code

Standard API
*/
void CNPREffectOutlineSketch::EscMultiThreadedCallback (PVOID pParams, DWORD dwParamSize,
      DWORD dwThread)
{
   PEMTOUTLINESKETCH pep = (PEMTOUTLINESKETCH)pParams;

   PSTROKEINFO psi = pep->psi;
   PCImage pImageSrc = pep->pImageSrc;
   PCImage pImageDest = pep->pImageDest;
   PCFImage pFImageSrc = pep->pFImageSrc;
   PCFImage pFImageDest = pep->pFImageDest;
   PCPaintingStroke pStroke = pep->paStroke + dwThread;
   PBYTE pbOutline = pep->pbOutline;
   DWORD dwLevel = pep->dwLevel;

   srand(GetTickCount() + dwThread * 1000);  // set random seed

   PIMAGEPIXEL pip = pImageSrc ? pImageSrc->Pixel(0,0) : NULL;
   PFIMAGEPIXEL pfp = pImageSrc ? NULL : pFImageSrc->Pixel(0,0);
   DWORD dwWidth = pImageDest ? pImageDest->Width() : pFImageDest->Width();
   DWORD dwHeight = pImageDest ? pImageDest->Height() : pFImageDest->Height();

   // spyglass
   // determine circle height/width
   DWORD dwMin = min(dwWidth, dwHeight);
   DWORD dwCircleWidth = dwMin;
   DWORD dwCircleHeight = dwMin;
   if (m_dwSpyglassShape == 2) {
      // elliptical
      dwCircleWidth = dwWidth;
      dwCircleHeight = dwHeight;
   }
   dwCircleWidth = (DWORD)((fp)dwCircleWidth * m_fSpyglassScale / 2.0);   // radius
   dwCircleHeight = (DWORD)((fp)dwCircleHeight * m_fSpyglassScale / 2.0);
   dwCircleWidth = max(dwCircleWidth, 1);
   dwCircleHeight = max(dwCircleHeight, 1);

   fp fScaleX = 1.0 / (fp)dwCircleWidth;
   fp fScaleY = 1.0 / (fp)dwCircleHeight;

   // center...
   TEXTUREPOINT afCenter[2];
   afCenter[0].h = (int)dwWidth/2;
   afCenter[0].v = (int)dwHeight/2;
   if (m_dwSpyglassShape == 3) {
      // binoculars
      afCenter[1] = afCenter[0];
      afCenter[0].h = (fp)dwMin/2;
      afCenter[1].h = (fp)dwWidth - (fp)dwMin/2;
   }
   // blur square
   m_fSpyglassBlur = max(m_fSpyglassBlur, 0.00001);
   fp fBlurSquare = (1.0 - m_fSpyglassBlur) * (1.0 - m_fSpyglassBlur);



   DWORD i;
   psi += pep->dwStart;
   // loop through all the points
   for (i = pep->dwStart; i < pep->dwEnd; i++, psi++) {
      // if the stroke is already drawn then don't bother
      if (pbOutline[(DWORD)psi->wY * dwWidth + (DWORD)psi->wX] != (BYTE)dwLevel)
         continue;   // has already been drawn

      // spyglass
      if (m_dwSpyglassShape) {
         // if out too close then dont affect
         if (pip) {
            if (pip->fZ < m_fSpyglassStickOut)
               goto donespyglass;
         }
         else {
            if (pfp->fZ < m_fSpyglassStickOut)
               goto donespyglass;
         }

         // distance from the center...
         fp fX = ((fp)psi->wX - afCenter[0].h) * fScaleX;
         fp fY = ((fp)psi->wY - afCenter[0].v) * fScaleY;
         fp fDist = fX * fX + fY * fY;
         if (m_dwSpyglassShape == 3) {
            // binoculars
            fX = ((fp)psi->wX - afCenter[1].h) * fScaleX;
            fY = ((fp)psi->wY - afCenter[1].v) * fScaleY;
            fp fDist2 = fX * fX + fY * fY;
            fDist = min(fDist, fDist2);
         }

         // if closer than blue then all visible
         if (fDist <= fBlurSquare)
            goto donespyglass;

         // if further away then all black
         if (fDist >= 1)
            continue;

         // else, blend
         // note: Dealing with square space so get rid of mach band
         fDist -= fBlurSquare;
         fDist /= (1.0 - fBlurSquare);
         //fDist = sqrt(fDist);
         //fDist -= (1.0 - m_fSpyglassBlur);
         //fDist /= m_fSpyglassBlur;
         // fDist = min(fDist, 1);
         fDist *= 256;
         if ((rand()%256) < fDist)
            continue;
      } // spyglassshape
donespyglass:
      // draw this
      Stroke (dwThread, (DWORD)psi->wX, (DWORD)psi->wY, pImageSrc, pFImageSrc, pImageDest,
         pFImageDest, pbOutline, pStroke);
   } // i
}

/*********************************************************************************
CNPREffectOutlineSketch::RenderAny - This renders both integer and FP image.

inputs
   PCImage        pImageSrc - Image. If NULL then use pFImage
   PCFImage       pFImageSrc - Floating point image. If NULL then use pImage
   PCImage        pImageDest - Dest image. If NULL then use pFImage
   PCFImage       pFImageDest - Dest floating point image. If NULL then use pImage
   PCProgressSocket pProgress - Progress
returns
   BOOL - TRUE if success
*/
BOOL CNPREffectOutlineSketch::RenderAny (PCImage pImageSrc, PCFImage pFImageSrc,
                                  PCImage pImageDest, PCFImage pFImageDest,
                                  PCProgressSocket pProgress)
{
   PIMAGEPIXEL pip = pImageSrc ? pImageSrc->Pixel(0,0) : NULL;
   PFIMAGEPIXEL pfp = pImageSrc ? NULL : pFImageSrc->Pixel(0,0);
   DWORD dwWidth = pImageDest ? pImageDest->Width() : pFImageDest->Width();
   DWORD dwHeight = pImageDest ? pImageDest->Height() : pFImageDest->Height();
   BOOL f360 = pImageDest ? pImageDest->m_f360 : pFImageDest->m_f360;

   CPaintingStroke avStroke[MAXRAYTHREAD];
   srand(GetTickCount());  // set random seed
   DWORD i;
   for (i = 0; i < MAXRAYTHREAD; i++)
      if (!avStroke[i].Init (m_dwBrushShape, 8, &m_pBrushParam))
         return FALSE;

   // figure out the outline areas
   CMem  memOutline;
   CListFixed alList[3];
   if (!TableOutlines (m_dwLevel, pImageSrc, pFImageSrc, &memOutline,
      &alList[0], &alList[1], &alList[2]))
      return FALSE;
   PBYTE pbOutline = (PBYTE)memOutline.p;



   // loop through all the points and draw lines
   DWORD dwLevel;
   for (dwLevel = m_dwLevel; dwLevel; dwLevel--) {
      DWORD dwNumStrokes = alList[dwLevel-1].Num();
      PSTROKEINFO psi = (PSTROKEINFO) alList[dwLevel-1].Get(0);

      // sort by Z
      qsort (psi, dwNumStrokes, sizeof(STROKEINFO), STROKEINFOCompare);


      // BUGFIX - multithreaded
      EMTOUTLINESKETCH ep;
      memset (&ep, 0, sizeof(ep));
      ep.pImageSrc = pImageSrc;
      ep.pImageDest = pImageDest;
      ep.pFImageSrc = pFImageSrc;
      ep.pFImageDest = pFImageDest;
      ep.psi = psi;
      ep.paStroke = avStroke;
      ep.dwLevel = dwLevel;
      ep.pbOutline = pbOutline;
      ThreadLoop (0, dwNumStrokes, 2, &ep, sizeof(ep));


   } // level

   return TRUE;
}


/*********************************************************************************
CNPREffectOutlineSketch::Render - From CNPREffect
*/
BOOL CNPREffectOutlineSketch::Render (PCImage pOrig, PCImage pDest, PCRenderSuper pRender,
                     PCWorldSocket pWorld, BOOL fFinal, PCProgressSocket pProgress)
{
   return RenderAny (pOrig, NULL, pDest, NULL, pProgress);
}



/*********************************************************************************
CNPREffectOutlineSketch::Render - From CNPREffect
*/
BOOL CNPREffectOutlineSketch::Render (PCFImage pOrig, PCFImage pDest, PCRenderSuper pRender,
                     PCWorldSocket pWorld, BOOL fFinal, PCProgressSocket pProgress)
{
   return RenderAny (NULL, pOrig, NULL, pDest, pProgress);
}


/*****************************************************************************
RenderSceneTabs - This code displays the tabs used for the RSTABS macro.

inputs
   DWORD          dwTab - Tab ID that's currently selected
   DWORD          dwNum - Number of tabs
   PWSTR          *ppsz - Pointer to an array of dwNum tabs
   PWSTR          *ppszHelp - Pointer to an arrya of dwNum help entries
   DWORD          *padwID - Array of dwNum IDs for each tab
   DWORD          dwSkipNum - Number of IDs stored in padwSkip
   DWORD          *padwSkip - If a tab is this number then it's skipped
returns
   PWSTR - gMemTemp.p with text
*/
static PWSTR RenderSceneTabs (DWORD dwTab, DWORD dwNum, PWSTR *ppsz, PWSTR *ppszHelp, DWORD *padwID,
                       DWORD dwSkipNum, DWORD *padwSkip)
{
   MemCat (&gMemTemp, L"<tr>");

   DWORD i, j;
   for (i = 0; i < dwNum; i++) {
      for (j = 0; j < dwSkipNum; j++)
         if (padwID[i] == padwSkip[j])
            break;   // skip this
      if (j < dwSkipNum)
         continue;

      if (!ppsz[i]) {
         MemCat (&gMemTemp, L"<td/>");
         continue;
      }

      MemCat (&gMemTemp, L"<td align=center");
      if (padwID[i] != dwTab)
         MemCat (&gMemTemp, L" bgcolor=#8080a0");
      MemCat (&gMemTemp,
         L">"
         L"<a href=tabpress:");
      MemCat (&gMemTemp, (int)padwID[i]);
      MemCat (&gMemTemp, L">"
         L"<bold>");
      MemCatSanitize (&gMemTemp, ppsz[i]);
      MemCat (&gMemTemp, L"</bold>"
         L"<xHoverHelp>");
      MemCatSanitize (&gMemTemp, ppszHelp[i]);
      MemCat (&gMemTemp,
         L"</xHoverHelp>"
         L"</a>"
         L"</td>");
   } // i

   MemCat (&gMemTemp, L"</tr>");
   return (PWSTR)gMemTemp.p;
}



/****************************************************************************
EffectOutlineSketchPage
*/
BOOL EffectOutlineSketchPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   POUTLINESKETCHPAGE pmp = (POUTLINESKETCHPAGE)pPage->m_pUserData;
   PCNPREffectOutlineSketch pv = pmp->pe;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         // set button for show all effects
         PCEscControl pControl;
         pControl = pPage->ControlFind (L"alleffects");
         if (pControl)
            pControl->AttribSetBOOL (Checked(), pmp->fAllEffects);

         // set the checkbox
         if (pControl = pPage->ControlFind (L"colorusefixed"))
            pControl->AttribSetBOOL (Checked(), pv->m_fColorUseFixed);

         // edit
         DoubleToControl (pPage, L"strokelen", pv->m_fStrokeLen);
         DoubleToControl (pPage, L"strokelenmin", pv->m_fStrokeLenMin);
         DoubleToControl (pPage, L"strokestep", pv->m_fStrokeStep);
         MeasureToString (pPage, L"spyglassstickout", pv->m_fSpyglassStickOut);
         AngleToControl (pPage, L"StrokePrefAngle", pv->m_fStrokePrefAngle);

         ComboBoxSet (pPage, L"brushshape", pv->m_dwBrushShape);

         DWORD i;
         WCHAR szTemp[64];
         for (i = 0; i < 3; i++) {
            swprintf (szTemp, L"strokewidth%d", (int)i);
            DoubleToControl (pPage, szTemp, pv->m_pStrokeWidth.p[i]);
         } // i
         for (i = 0; i < 3; i++) {
            swprintf (szTemp, L"colorpalette%d", (int)i);
            DoubleToControl (pPage, szTemp, pv->m_pColorPalette.p[i]);
         } // i

         // texture
         ComboBoxSet (pPage, L"spyglassshape", pv->m_dwSpyglassShape);
         ComboBoxSet (pPage, L"level", pv->m_dwLevel);

         for (i = 0; i < 5; i++) {
            swprintf (szTemp, L"colorfixed%d", (int) i);
            FillStatusColor (pPage, szTemp, pv->m_acColorFixed[i]);
         } // i
         FillStatusColor (pPage, L"outlinecolor", pv->m_cOutlineColor);

         // scrollbars
         if (pControl = pPage->ControlFind (L"overshoot"))
            pControl->AttribSetInt (Pos(), (int)(pv->m_fOvershoot * 100));
         if (pControl = pPage->ControlFind (L"outlineblend"))
            pControl->AttribSetInt (Pos(), (int)(pv->m_fOutlineBlend * 100));
         if (pControl = pPage->ControlFind (L"maxangle"))
            pControl->AttribSetInt (Pos(), (int)(pv->m_fMaxAngle * 100));
         if (pControl = pPage->ControlFind (L"strokerandom"))
            pControl->AttribSetInt (Pos(), (int)(pv->m_fStrokeRandom * 100));
         if (pControl = pPage->ControlFind (L"strokePenObject"))
            pControl->AttribSetInt (Pos(), (int)(pv->m_fStrokePenObject * 100));
         if (pControl = pPage->ControlFind (L"strokeanchor"))
            pControl->AttribSetInt (Pos(), (int)(pv->m_fStrokeAnchor * 100));
         if (pControl = pPage->ControlFind (L"strokelenvar"))
            pControl->AttribSetInt (Pos(), (int)(pv->m_fStrokeLenVar * 100));
         if (pControl = pPage->ControlFind (L"strokePenColor"))
            pControl->AttribSetInt (Pos(), (int)(pv->m_fStrokePenColor * 100));
         if (pControl = pPage->ControlFind (L"strokewidth3"))
            pControl->AttribSetInt (Pos(), (int)(pv->m_pStrokeWidth.p[3] * 100));
         if (pControl = pPage->ControlFind (L"colorhueshift"))
            pControl->AttribSetInt (Pos(), (int)(pv->m_fColorHueShift * 100));
         if (pControl = pPage->ControlFind (L"spyglassscale"))
            pControl->AttribSetInt (Pos(), (int)(pv->m_fSpyglassScale * 100));
         if (pControl = pPage->ControlFind (L"spyglassblur"))
            pControl->AttribSetInt (Pos(), (int)(pv->m_fSpyglassBlur * 100));
         for (i = 0; i < 3; i++) {
            swprintf (szTemp, L"colorvar%d", (int)i);
            if (pControl = pPage->ControlFind (szTemp))
               pControl->AttribSetInt (Pos(), (int)(pv->m_pColorVar.p[i] * 100));
         }
         for (i = 0; i < 4; i++) {
            swprintf (szTemp, L"brushparam%d", (int)i);
            if (pControl = pPage->ControlFind (szTemp))
               pControl->AttribSetInt (Pos(), (int)(pv->m_pBrushParam.p[i] * 100));
         }


      }
      break;

   case ESCN_COMBOBOXSELCHANGE:
      {
         PESCNCOMBOBOXSELCHANGE p = (PESCNCOMBOBOXSELCHANGE)pParam;

         if (!p->pControl || !p->pControl->m_pszName)
            break;
         PWSTR psz = p->pControl->m_pszName;

         if (!_wcsicmp(psz, L"brushshape")) {
            DWORD dwVal = p->pszName ? _wtoi(p->pszName) : 0;
            if (dwVal == pv->m_dwBrushShape)
               return TRUE;   // no change

            pv->m_dwBrushShape = dwVal;
            pPage->Message (ESCM_USER+189);  // update bitmap

            return TRUE;
         } // pattern
         else if (!_wcsicmp(psz, L"spyglassshape")) {
            DWORD dwVal = p->pszName ? _wtoi(p->pszName) : 0;
            if (dwVal == pv->m_dwSpyglassShape)
               return TRUE;   // no change

            pv->m_dwSpyglassShape = dwVal;
            pPage->Message (ESCM_USER+189);  // update bitmap

            return TRUE;
         } // pattern
         else if (!_wcsicmp(psz, L"level")) {
            DWORD dwVal = p->pszName ? _wtoi(p->pszName) : 0;
            if (dwVal == pv->m_dwLevel)
               return TRUE;   // no change

            pv->m_dwLevel = dwVal;
            pPage->Message (ESCM_USER+189);  // update bitmap

            return TRUE;
         } // pattern
      }
      break;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;

         PWSTR pszColorChangeFixed = L"changecolorfixed";
         DWORD dwColorChangeFixedLen = (DWORD)wcslen(pszColorChangeFixed);

         // see about all effects checked or unchecked
         if (!_wcsicmp(p->pControl->m_pszName, L"alleffects")) {
            pmp->fAllEffects = p->pControl->AttribGetBOOL (Checked());
            pPage->Message (ESCM_USER+189);  // update bitmap
            return TRUE;
         }

         // if it's for the colors then do those
         else if (!_wcsicmp(p->pControl->m_pszName, L"colorusefixed")) {
            pv->m_fColorUseFixed = p->pControl->AttribGetBOOL (Checked());
            pPage->Message (ESCM_USER+189);  // update bitmap
            return TRUE;
         }
         else if (!wcsncmp(p->pControl->m_pszName, pszColorChangeFixed, dwColorChangeFixedLen)) {
            DWORD dwNum = _wtoi(p->pControl->m_pszName + dwColorChangeFixedLen);
            WCHAR szTemp[64];
            swprintf (szTemp, L"colorfixed%d", (int) dwNum);
            pv->m_acColorFixed[dwNum] = AskColor (pPage->m_pWindow->m_hWnd,
               pv->m_acColorFixed[dwNum], pPage, szTemp);
            pPage->Message (ESCM_USER+189);  // update bitmap
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"changeoutlinecolor")) {
            pv->m_cOutlineColor = AskColor (pPage->m_pWindow->m_hWnd,
               pv->m_cOutlineColor, pPage, L"outlinecolor");
            pPage->Message (ESCM_USER+189);  // update bitmap
            return TRUE;
         }


      }
      break;   // default

   case ESCN_EDITCHANGE:
      {
         // just get all values
         if (pmp->dwTab == 1) {   // strokes
            pv->m_fStrokeLen = DoubleFromControl (pPage, L"strokelen");
            pv->m_fStrokeLenMin = DoubleFromControl (pPage, L"strokelenmin");
            pv->m_fStrokeStep = DoubleFromControl (pPage, L"strokestep");
         }
         else if (pmp->dwTab == 2) {   // misc
            MeasureParseString (pPage, L"spyglassstickout", &pv->m_fSpyglassStickOut);
         }
         else if (pmp->dwTab == 3) {   // stroke path
            pv->m_fStrokePrefAngle = AngleFromControl (pPage, L"StrokePrefAngle");
         }
         else if (pmp->dwTab == 4) {   // stroke shape
            DWORD i;
            WCHAR szTemp[64];
            for (i = 0; i < 3; i++) {
               swprintf (szTemp, L"strokewidth%d", (int)i);
               pv->m_pStrokeWidth.p[i] = DoubleFromControl (pPage, szTemp);
            } // i
         }
         else if (pmp->dwTab == 5) {   // stroke shape
            DWORD i;
            WCHAR szTemp[64];
            for (i = 0; i < 3; i++) {
               swprintf (szTemp, L"colorpalette%d", (int)i);
               pv->m_pColorPalette.p[i] = DoubleFromControl (pPage, szTemp);
            } // i
         }

         pPage->Message (ESCM_USER+189);  // update bitmap
      }
      break;

   case ESCM_LINK:
      {
         PESCMLINK p = (PESCMLINK) pParam;
         if (!p->psz)
            break;

         PWSTR pszTab = L"tabpress:";
         DWORD dwLen = (DWORD)wcslen(pszTab);

         if (!wcsncmp(p->psz, pszTab, dwLen)) {
            pmp->dwTab = (DWORD)_wtoi(p->psz + dwLen);
            pPage->Exit (RedoSamePage());
            return TRUE;
         }

      }
      break;

   case ESCN_SCROLL:
   //case ESCN_SCROLLING:
      {
         PESCNSCROLL p = (PESCNSCROLL)pParam;
         PWSTR psz = p->pControl->m_pszName;
         if (!psz)
            break;

         fp *pf = NULL;
         PWSTR pszColorVar = L"colorvar", pszBrushParam = L"brushparam";
         DWORD dwColorVarLen = (DWORD)wcslen(pszColorVar),
            dwBrushParamLen = (DWORD)wcslen(pszBrushParam);

         if (!_wcsicmp (psz, L"strokerandom"))
            pf = &pv->m_fStrokeRandom;
         else if (!_wcsicmp (psz, L"outlineblend"))
            pf = &pv->m_fOutlineBlend;
         else if (!_wcsicmp (psz, L"strokeanchor"))
            pf = &pv->m_fStrokeAnchor;
         else if (!_wcsicmp (psz, L"strokeLenVar"))
            pf = &pv->m_fStrokeLenVar;
         else if (!_wcsicmp (psz, L"strokePenColor"))
            pf = &pv->m_fStrokePenColor;
         else if (!_wcsicmp (psz, L"strokePenObject"))
            pf = &pv->m_fStrokePenObject;
         else if (!_wcsicmp (psz, L"strokewidth3"))
            pf = &pv->m_pStrokeWidth.p[3];
         else if (!_wcsicmp (psz, L"colorhueshift"))
            pf = &pv->m_fColorHueShift;
         else if (!_wcsicmp (psz, L"spyglassscale"))
            pf = &pv->m_fSpyglassScale;
         else if (!_wcsicmp (psz, L"spyglassblur"))
            pf = &pv->m_fSpyglassBlur;
         else if (!_wcsicmp (psz, L"overshoot"))
            pf = &pv->m_fOvershoot;
         else if (!_wcsicmp (psz, L"maxangle"))
            pf = &pv->m_fMaxAngle;
         else if (!wcsncmp(psz, pszColorVar, dwColorVarLen))
            pf = &pv->m_pColorVar.p[_wtoi(psz + dwColorVarLen)];
         else if (!wcsncmp(psz, pszBrushParam, dwBrushParamLen))
            pf = &pv->m_pBrushParam.p[_wtoi(psz + dwBrushParamLen)];

         if (!pf)
            break;   // not known

         *pf = (fp)p->iPos / 100.0;
         pPage->Message (ESCM_USER+189);  // update bitmap
         return TRUE;
      }
      break;

   case ESCM_USER+189:  // update image
      {
         if (pmp->hBit)
            DeleteObject (pmp->hBit);
         if (pmp->fAllEffects)
            pmp->hBit = EffectImageToBitmap (pmp->pTest, pmp->pAllEffects, NULL, pmp->pRender, pmp->pWorld);
         else
            pmp->hBit = EffectImageToBitmap (pmp->pTest, NULL, pmp->pe, pmp->pRender, pmp->pWorld);

         WCHAR szTemp[32];
         swprintf (szTemp, L"%lx", (__int64)pmp->hBit);
         PCEscControl pControl = pPage->ControlFind (L"image");
         if (pControl)
            pControl->AttribSet (L"hbitmap", szTemp);
      }
      return TRUE;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         PWSTR pszIfTab = L"IFTAB", pszEndIfTab = L"ENDIFTAB";
         DWORD dwIfTabLen = (DWORD)wcslen(pszIfTab), dwEndIfTabLen = (DWORD)wcslen(pszEndIfTab);

         if (!wcsncmp (p->pszSubName, pszIfTab, dwIfTabLen)) {
            DWORD dwNum = _wtoi(p->pszSubName + dwIfTabLen);
            if (dwNum == pmp->dwTab)
               p->pszSubString = L"";
            else
               p->pszSubString = L"<comment>";
            return TRUE;
         }
         else if (!wcsncmp (p->pszSubName, pszEndIfTab, dwEndIfTabLen)) {
            DWORD dwNum = _wtoi(p->pszSubName + dwEndIfTabLen);
            if (dwNum == pmp->dwTab)
               p->pszSubString = L"";
            else
               p->pszSubString = L"</comment>";
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Outline (sketch)";
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"HBITMAP")) {
            WCHAR szTemp[32];
            swprintf (szTemp, L"%lx", (__int64)pmp->hBit);
            MemZero (&gMemTemp);
            MemCat (&gMemTemp, szTemp);
            p->pszSubString = (PWSTR)gMemTemp.p;
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"RSTABS")) {
            PWSTR apsz[] = {
               L"Strokes",
               L"Path",
               L"Brush",
               L"Color",
               L"Misc.",
            };
            PWSTR apszHelp[] = {
               L"Controls the number and length of strokes.",
               L"Controls the path of strokes.",
               L"Lets you change the type and size of the brush.",
               L"Controls the color of the stroke.",
               L"Miscellaneous settings.",
            };
            DWORD adwID[] = {
               1, // Strokes
               3, // stroke path
               4, // brush
               5, // color
               2, // misc
            };

            CListFixed lSkip;
            lSkip.Init (sizeof(DWORD));


            p->pszSubString = RenderSceneTabs (pmp->dwTab, sizeof(apsz)/sizeof(PWSTR), apsz,
               apszHelp, adwID, lSkip.Num(), (DWORD*)lSkip.Get(0));
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}

/*********************************************************************************
CNPREffectOutlineSketch::IsPainterly - Returns TRUE if is a painterly effect
and doesn't need as high a resolution
*/
BOOL CNPREffectOutlineSketch::IsPainterly (void)
{
   return FALSE;
}

/*********************************************************************************
CNPREffectOutlineSketch::Dialog - From CNPREffect
*/
BOOL CNPREffectOutlineSketch::Dialog (PCEscWindow pWindow, PCNPREffectsList pAllEffects, PCImage pTest, 
                            PCRenderSuper pRender, PCWorldSocket pWorld)
{
   PWSTR pszRet;
   OUTLINESKETCHPAGE mp;
   memset (&mp, 0, sizeof(mp));
   mp.pe = this;
   mp.pTest = pTest;
   mp.pRender = pRender;
   mp.pWorld = pWorld;
   mp.pAllEffects = pAllEffects;
   mp.dwTab = 1;

   // delete existing
   if (mp.hBit)
      DeleteObject (mp.hBit);
   if (mp.fAllEffects)
      mp.hBit = EffectImageToBitmap (pTest, pAllEffects, NULL, pRender, pWorld);
   else
      mp.hBit = EffectImageToBitmap (pTest, NULL, this, pRender, pWorld);

redo:
   pszRet = pWindow->PageDialog (ghInstance, IDR_MMLEFFECTOUTLINESKETCH, EffectOutlineSketchPage, &mp);
   mp.iVScroll = pWindow->m_iExitVScroll;
   if (pszRet && !_wcsicmp(pszRet, RedoSamePage()))
      goto redo;

   // elete
   if (mp.hBit)
      DeleteObject (mp.hBit);
   mp.hBit = NULL;

   if (pszRet && !_wcsicmp(pszRet, Back()))
      return TRUE;
   return FALSE;
}


/*********************************************************************************
PixelColor - This adds the pixel color into the queue. It finds the pixel
with the lowest Z (closest) and includes that.

inputs
   PCImage        pImage- Image to use, or nULL if use PFImage
   PCFImage       pFImage - Image to use, or NULL if use pImage
   DWORD          dwWidth - Width of image
   DWORD          dwHeight - Height of image
   BOOL           f360 - TRUE if 360 degree image
   int            iX - X pixel
   int            iY - Y pixel
   double         *pafColor - Color added to this if found
returns
   BOOL - TRUE if found, FALSE if not
*/
static __inline BOOL PixelColor (PCImage pImage, PCFImage pFImage,
                                 DWORD dwWidth, DWORD dwHeight, BOOL f360,
                                 int iX, int iY, double *pafColor)
{
   int x, y, xx;
   PIMAGEPIXEL pip, pipBest = NULL;
   PFIMAGEPIXEL pfp, pfpBest = NULL;
   for (y = iY-1; y <= iY+1; y++) {
      if ((y < 0) || (y >= (int)dwHeight))
         continue;

      for (x = iX-1; x <= iX+1; x++) {
         xx = x;
         if ((xx < 0) || (xx >= (int)dwWidth)) {
            if (!f360)
               continue;

            while (xx < 0)
               xx += (int)dwWidth;
            while (xx >= (int)dwWidth)
               xx -= (int)dwWidth;
         } // if xx

         if (pImage) {
            pip = pImage->Pixel((DWORD)xx, (DWORD)iY);
            if (!pipBest || (pip->fZ < pipBest->fZ))
               pipBest = pip;
         }
         else {
            pfp = pFImage->Pixel((DWORD)xx, (DWORD)iY);
            if (!pfpBest || (pfp->fZ < pfpBest->fZ))
               pfpBest = pfp;
         }

      } // x
   } // y

   if (pipBest) {
      pafColor[0] += pipBest->wRed;
      pafColor[1] += pipBest->wGreen;
      pafColor[2] += pipBest->wBlue;
      return TRUE;
   }
   else if (pfpBest) {
      pafColor[0] += pfpBest->fRed;
      pafColor[1] += pfpBest->fGreen;
      pafColor[2] += pfpBest->fBlue;
      return TRUE;
   }
   else
      return FALSE;
}



/*********************************************************************************
CNPREffectOutlineSketch::Stroke - Draws a stroke. m_pFieldXXX must be valid.

inputs
   DWORD          dwThread - Thread number, from 0..MAXRAYTHREAD-1 doing the rendering
   DWORD          dwX - Starting X
   DWORD          dwY - Starting Y
   PCImage        pImageSrc - Image to use, or NULL if use pFImage
   PCFImage       pFImageSrc - Image to use, or NULL if use pImage
   PCImage        pImageDest - Image to use, or NULL if use pFImage
   PCFImage       pFImageDest - Image to use, or NULL if use pImage
   PBYTE          pbOutline - Filled with outline info from TableOutlines();
   PCPaintingStroke pStroke - What to paint with
returns
   BOOL - TRUE if success
*/
BOOL CNPREffectOutlineSketch::Stroke (DWORD dwThread, DWORD dwX, DWORD dwY, PCImage pImageSrc, PCFImage pFImageSrc,
                                 PCImage pImageDest, PCFImage pFImageDest,
                                 PBYTE pbOutline, PCPaintingStroke pStroke)
{
   DWORD dwWidth = pImageSrc ? pImageSrc->Width() : pFImageSrc->Width();
   DWORD dwHeight = pImageSrc ? pImageSrc->Height() : pFImageSrc->Height();
   BOOL f360 = pImageSrc ? pImageSrc->m_f360 : pFImageSrc->m_f360;
   BOOL fRet = TRUE;

   // figure out the level
   BYTE bLevel = pbOutline[dwY * dwWidth + dwX];
   if (!bLevel)
      return FALSE;  // shouldnt happen

   // figure out the length
   fp fLen = m_fStrokeLenVar ? randf(1.0 -m_fStrokeLenVar, 1.0 + m_fStrokeLenVar) : 1;
   fLen *= (fp)dwWidth / 100.0 * m_fStrokeLen;
   fLen = max(fLen, 1);
   DWORD dwLen = (DWORD)fLen;

   // figurte out best path
   PCListFixed plPath = GenerateBestPath (pbOutline, dwWidth, dwHeight, f360,
      bLevel, dwX, dwY, dwLen);
   if (!plPath || !plPath->Num()) {
      if (plPath)
         delete plPath;
      return FALSE;
   }

   // if the length of this is less than a minium line length then skip
   if ((fp)plPath->Num() < (fp)m_fStrokeLenMin / 100.0 * (fp)dwWidth)
      goto done;


   // figure out the pixel color by averaging all the colors along the length
   double afColor[3];
   DWORD dwCount = 0;
   memset (afColor, 0, sizeof(afColor));
   POINT *pp = (POINT*)plPath->Get(0);
   DWORD i;
   for (i = 0; i < plPath->Num(); i++, pp++) {
      if (PixelColor (pImageSrc, pFImageSrc, dwWidth, dwHeight, f360, pp->x, pp->y, &afColor[0]))
         dwCount++;
   } // i
   if (dwCount) for (i = 0; i < 3; i++)
      afColor[i] /= (double)dwCount;

   // average with base color
   WORD awColor[3];
   Gamma (m_cOutlineColor, awColor);
   for (i = 0; i < 3; i++)
      afColor[i] = m_fOutlineBlend * afColor[i] + (1.0 - m_fOutlineBlend) * (double)awColor[i];

   // write into awcolor and do randomization
   for (i = 0; i < 3; i++) {
      afColor[i] = max(afColor[i], 0);
      afColor[i] = min(afColor[i], (double)0xffff);
      awColor[i] = (WORD)afColor[i];
   }
   Color (awColor);

   // extend stroke's beyond start and end
   DWORD dwSafeMin = 0; // can eliminate > this point
   DWORD dwSafeMax = plPath->Num() - 1;   // can eliminate < this point
   if (m_fOvershoot && (plPath->Num() >= 2)) {
      // calculate amount to overshoot
      fp fOver = (fp)plPath->Num() * m_fOvershoot / (fp)(1 << bLevel);  // intenionally extra /2
      if (m_fStrokeLenVar)
         fOver *= randf(1.0 -m_fStrokeLenVar, 1.0 + m_fStrokeLenVar);
      fOver = min(fOver, 2 * m_fStrokeStep / 100.0 * (fp)dwWidth); // no more than 2 stroke steps

      // first point
      DWORD dwTest = min(plPath->Num()-1, 3);   // try to go 3 points in
      POINT *pp = (POINT*) plPath->Get(0);
      POINT pDelta, pOver;
      fp fLen;
      pDelta.x = pp[dwTest].x - pp[0].x;
      pDelta.y = pp[dwTest].y - pp[0].y;
      fLen = sqrt((fp)(pDelta.x * pDelta.x + pDelta.y * pDelta.y));
      if (fLen) {
         pOver.x = (int)((fp)pp[0].x - (fp)pDelta.x / fLen * fOver);
         pOver.y = (int)((fp)pp[0].y - (fp)pDelta.y / fLen * fOver);
         plPath->Insert (0, &pOver);
         dwSafeMin += 2;
         dwSafeMax++;
      }

      // last point
      DWORD dwNum = plPath->Num();
      dwTest = (dwNum > 4) ? (dwNum-4) : 0;
      pp = (POINT*) plPath->Get(0);
      pDelta.x = pp[dwNum-1].x - pp[dwTest].x;
      pDelta.y = pp[dwNum-1].y - pp[dwTest].y;
      fLen = sqrt((fp)(pDelta.x * pDelta.x + pDelta.y * pDelta.y));
      if (fLen) {
         pOver.x = (int)((fp)pp[dwNum-1].x + (fp)pDelta.x / fLen * fOver);
         pOver.y = (int)((fp)pp[dwNum-1].y + (fp)pDelta.y / fLen * fOver);
         plPath->Add (&pOver);
      }
   } // if overshoot

   // keep only one out of N points so smooth it all
   DWORD dwSkip = (DWORD)(m_fStrokeStep / 100.0 * (fp)dwWidth + 0.5);
   if (dwSkip > 1) for (i = dwSafeMax-1; (i > dwSafeMin) && (i < dwSafeMax); i--) {
      if (!(i%dwSkip))
         continue;   // dont delete this

      // delete this
      plPath->Remove (i);
   }

   // maybe flip back to front based on preferred angle
   pp = (POINT*) plPath->Get(0);
   POINT *pEnd = pp + (plPath->Num()-1);
   POINT pDelta;
   pDelta.x = pEnd->x - pp->x;
   pDelta.y = pEnd->y - pp->y;
   fp fDotProd = (fp)pDelta.x*sin(m_fStrokePrefAngle) - (fp)pDelta.y*cos(m_fStrokePrefAngle);
   if (fDotProd < 0) {
      // flip
      DWORD dwNum = plPath->Num();
      for (i = 0; i < dwNum/2; i++) {
         pDelta = pp[i];
         pp[i] = pp[dwNum-i-1];
         pp[dwNum-i-1] = pDelta;
      }
   }


   // add randomness to curve
   if (m_fStrokeRandom) {
      fp fVar = m_fStrokeRandom * m_fStrokeStep / 100.0 * (fp)dwWidth / 2;
      POINT *pp = (POINT*)plPath->Get(0);
      for (i = 0; i < plPath->Num(); i++, pp++) {
         pp->x = (int)((fp)pp->x + randf(-fVar, fVar));
         pp->y = (int)((fp)pp->y + randf(-fVar, fVar));
      } // i
   }

   // make up thickness
   DWORD dwThick;
   DWORD dwAnchor = (DWORD)((fp)(plPath->Num()-1) * m_fStrokeAnchor);
   if (!dwAnchor && (dwAnchor+1 < plPath->Num()))
      dwAnchor++; // if anchor starts at the very beginning then move up one
   m_alPathDWORD[dwThread].Clear();
   fp f, fThick;
   m_alPathDWORD[dwThread].Required (plPath->Num());
   for (i = 0; i < plPath->Num(); i++) {
      if (i < dwAnchor) {
         f = (fp)i / (fp)dwAnchor;
         fThick = (1.0 - f) * m_pStrokeWidth.p[0] + f * m_pStrokeWidth.p[1];
      }
      else if (i == dwAnchor)
         fThick = m_pStrokeWidth.p[1];
      else {
         f = (fp)(i - dwAnchor) / (fp)(plPath->Num() - dwAnchor);
         fThick = (1.0 - f) * m_pStrokeWidth.p[1] + f * m_pStrokeWidth.p[2];
      }

      fThick *= (fp)dwWidth / 100.0;
      fThick /= (fp)(1 << (bLevel-1));  // thickness depends on level
      if (m_pStrokeWidth.p[3])
         fThick *= randf (1.0 - m_pStrokeWidth.p[3], 1.0 + m_pStrokeWidth.p[3]);
      fThick = max(fThick, 1);
      fThick = min(fThick, 100); // dont make too large
      dwThick = (DWORD)(fThick + 0.5);
      m_alPathDWORD[dwThread].Add (&dwThick);
   }

   // draw it
   fRet = pStroke->Paint ((POINT*)plPath->Get(0), (DWORD*)m_alPathDWORD[dwThread].Get(0),
      plPath->Num(), 2,   // NOTE: Hardcode smoothing of 2
      pImageDest, pFImageDest, &awColor[0]);

done:
   delete plPath;
   return fRet;
}


/*********************************************************************************
CNPREffectOutlineSketch::Color - Modifies the stroke color based on settings.

inputs
   WORD        *pawColor - RGB color. Should be filled with initial value and
                     modified in place
*/
void CNPREffectOutlineSketch::Color (WORD *pawColor)
{
   // if using fixed colors then different code
   DWORD i;
   if (m_fColorUseFixed) {
      BYTE abColor[3];
      for (i = 0; i < 3; i++)
         abColor[i] = UnGamma(pawColor[i]);

      DWORD dwDist, dwBestDist = 0;
      DWORD dwBest = -1;
      for (i = 0; i < 5; i++) {
         BYTE r = GetRValue(m_acColorFixed[i]);
         BYTE g = GetGValue(m_acColorFixed[i]);
         BYTE b = GetBValue(m_acColorFixed[i]);

         dwDist = (r >= abColor[0]) ? (r - abColor[0]) : (abColor[0] - r);
         dwDist += (g >= abColor[1]) ? (g - abColor[1]) : (abColor[1] - g);
         dwDist += (b >= abColor[2]) ? (b - abColor[2]) : (abColor[2] - b);

         if ((dwBest == -1) || (dwDist < dwBestDist)) {
            dwBestDist = dwDist;
            dwBest = i;
         }
      } // i

      // save this out
      Gamma (m_acColorFixed[dwBest], pawColor);
      return;
   }

   DWORD adwSplit[3];
   float afScale[3];
   for (i = 0; i < 3; i++) {
      adwSplit[i] = (DWORD) floor(m_pColorPalette.p[i] + 0.5);
      adwSplit[i] = max(adwSplit[i], 1);

      if (i == 0)
         afScale[i] = (float)adwSplit[i] / 256.0;
      else if (adwSplit[i] >= 2)
         afScale[i] = (float)(adwSplit[i] - 1) / 256.0;
      else
         afScale[i] = 1;
   }

   // convert this to HLS
   float fH, fL, fS;
   ToHLS256 (UnGamma(pawColor[0]), UnGamma(pawColor[1]), UnGamma(pawColor[2]),
      &fH, &fL, &fS);

   // add variations
   if (!fS && !fH)
      fH = randf(0, 256);   // if no saturation, then random hue
   if (m_pColorVar.p[0])
      fH = myfmod(fH + randf(-128, 128)*m_pColorVar.p[0], 256);
   if (m_pColorVar.p[1]) {
      fL += randf(-128, 128) * m_pColorVar.p[1];
      fL = max(fL, 0);
      fL = min(fL, 255);
   }
   if (m_pColorVar.p[2]) {
      fS += randf(-128, 128) * m_pColorVar.p[2];
      fS = max(fS, 0);
      fS = min(fS, 255);
   }

   // apply quantization
   if (m_pColorPalette.p[0]) {
      fH = floor (fH * afScale[0] - m_fColorHueShift + 0.5);
      fH = (fH + m_fColorHueShift) / afScale[0];
      fH = myfmod (fH, 256);
   }

   // lightness
   if (m_pColorPalette.p[1]) {
      if (adwSplit[1] >= 2)
         fL = floor(fL * afScale[1] + 0.5) / afScale[1];
      else
         fL = 128;   // mid point
   }

   // saturations
   if (m_pColorPalette.p[2]) {
      if (adwSplit[2] >= 2)
         fS = floor(fS * afScale[2] + 0.5) / afScale[2];
      else
         fS = 256;   // full saturation
   }


   // convert back to HLS
   float afColor[3];
   FromHLS256 (fH, fL, fS, &afColor[0], &afColor[1], &afColor[2]);
   GammaFloat256 (afColor);
   for (i = 0; i < 3; i++) {
      afColor[i] = max(afColor[i], 0);
      afColor[i] = min(afColor[i], (float)0xffff);
      pawColor[i] = (WORD)afColor[i];
   }
}


/*********************************************************************************
CNPREffectOutlineSketch::TableOutlines - Scans through the image and creates
a table of BYTEs for each pixel, indicating 0 if there's no outline there,
1 if it's a major outline, 2 if it's minor, and 3 if tertiary.

inputs
   DWORD             dwLevel - 1 for primary, 2 for secondary, 3 for teriary
   PCImage           pImage - Image, NULL if use PFImage
   PCFImage          pFImage - Image, NULL if use pImage
   PCMem             pMem - Allocated with Width() x Height() BYTEs and filled with values
   PCListFixed       plPrimary - Initialized and fileld witth STROKEINFO for
                        primary outline pixels
   PCListFixed       plSecondary - Filled with secondary STROKEINFO
   PCListFixed       plTertiary - Filled with tertiary STROKEINFO
returns
   BOOL - TRUE if success
*/
BOOL CNPREffectOutlineSketch::TableOutlines (DWORD dwLevel, PCImage pImage, PCFImage pFImage,
                                             PCMem pMem, PCListFixed plPrimary,
                                             PCListFixed plSecondary, PCListFixed plTertiary)
{
   PIMAGEPIXEL pip = pImage ? pImage->Pixel(0,0) : NULL;
   PFIMAGEPIXEL pfp = pImage ? NULL : pFImage->Pixel(0,0);
   DWORD dwWidth = pImage ? pImage->Width() : pFImage->Width();
   DWORD dwHeight = pImage ? pImage->Height() : pFImage->Height();
   BOOL f360 = pImage ? pImage->m_f360 : pFImage->m_f360;

   if (!pMem->Required (dwWidth * dwHeight))
      return FALSE;
   PBYTE pbMem = (PBYTE)pMem->p;
   memset (pbMem, 0, dwWidth*dwHeight);

   plPrimary->Init (sizeof(STROKEINFO));
   plSecondary->Init (sizeof(STROKEINFO));
   plTertiary->Init (sizeof(STROKEINFO));

   PIMAGEPIXEL pLook = NULL;
   PFIMAGEPIXEL pfLook = NULL;
   WORD  wMajor, wMinor, wMajorLook, wMinorLook;
   DWORD dwTertiary, dwTertiaryLook;
   DWORD dwDir, x, y;
   for (y = 0; y < dwHeight; y++) {
      for (x = 0; x < dwWidth; pbMem++, x++, pip ? pip++ : (PIMAGEPIXEL)(pfp++)) {
         // find current ID for this pixel
         if (pip) {
            wMajor = HIWORD(pip->dwID);
            wMinor = LOWORD(pip->dwID);
            dwTertiary = pip->dwIDPart;
         }
         else {
            wMajor = HIWORD(pfp->dwID);
            wMinor = LOWORD(pfp->dwID);
            dwTertiary = pfp->dwIDPart;
         }

         for (dwDir = 0; dwDir < 2; dwDir++) {
            if (dwDir) {
               if (y+1 >= dwHeight)
                  continue;   // out of bounds
               if (pip)
                  pLook = pip + dwWidth;
               else
                  pfLook = pfp + dwWidth;
            }
            else {
               // look right
               if (x+1 < dwWidth) {
                  if (pip)
                     pLook = pip+1;
                  else
                     pfLook = pfp+1;
               }
               else if (f360) {
                  if (pip)
                     pLook = pip-(dwWidth-1);
                  else
                     pfLook = pfp-(dwWidth-1);
               }
               else
                  continue;   // none
            }

            if (pLook) {
               wMajorLook = HIWORD(pLook->dwID);
               wMinorLook = LOWORD(pLook->dwID);
               dwTertiaryLook = pLook->dwIDPart;
            }
            else {
               wMajorLook = HIWORD(pfLook->dwID);
               wMinorLook = LOWORD(pfLook->dwID);
               dwTertiaryLook = pfLook->dwIDPart;
            }

            // if transparency bit set then no outline
            if ((dwTertiaryLook | dwTertiary) & IDPARTBITS_TRANSPARENT)
               continue;

            BYTE bResult;
            if (wMajorLook != wMajor)
               bResult = 1;
            else if ((wMinorLook != wMinor) && (dwLevel >= 2))
               bResult  = 2;
            else if ((dwTertiaryLook != dwTertiary) && (dwLevel >= 3))
               bResult = 3;
            else
               continue;   // no worthwhile change

            // write out
            *pbMem = bResult;

            // add to appropriate list
            STROKEINFO si;
            si.fZ = pip ? min(pip->fZ, pLook->fZ) : min(pfp->fZ, pfLook->fZ);
            si.wX = (WORD) x;
            si.wY = (WORD) y;
            switch (bResult) {
            case 1:
               plPrimary->Add (&si);
               break;
            case 2:
               plSecondary->Add (&si);
               break;
            case 3:
               plTertiary->Add (&si);
               break;
            }
         } // dwDir
      } // x
   } // y

   return TRUE;
}



#define PATHRADIUS         4     // radius to look around in
#define PATHDIAM           (PATHRADIUS*2+1)  // total area

/*********************************************************************************
TryDirection - Used by GeneratePaths() to order all paths by their shortest
amount.

inputs
   PBYTE       pbPath - Pointer to an array of PATHDIAM x PATHDIAM bytes,
               with 0 being none, 0xff being un-marked, else, distance from center
   DWORD       dwX - X to look at
   DWORD       dwY - Y to look at
   BYTE        bValue - Value to write in X and Y (so long is value is < current value)
returns
   none
*/
static void TryDirection (PBYTE pbPath, DWORD dwX, DWORD dwY, BYTE bValue)
{
   PBYTE pbLook = pbPath + (dwY * PATHDIAM + dwX);
   if (*pbLook <= bValue)
      return;  // can't do better than this

   // else, can
   *pbLook = bValue;
   bValue++;   // so try higher values

   // look left
   if (dwX) {
      if (dwY)
         TryDirection (pbPath, dwX-1, dwY-1, bValue);
      TryDirection (pbPath, dwX-1, dwY, bValue);
      if (dwY + 1 < PATHDIAM)
         TryDirection (pbPath, dwX-1, dwY+1, bValue);
   }

   // look U/D
   if (dwY)
      TryDirection (pbPath, dwX, dwY-1, bValue);
   if (dwY + 1 < PATHDIAM)
      TryDirection (pbPath, dwX, dwY+1, bValue);

   // look right
   if (dwX + 1 < PATHDIAM) {
      if (dwY)
         TryDirection (pbPath, dwX+1, dwY-1, bValue);
      TryDirection (pbPath, dwX+1, dwY, bValue);
      if (dwY + 1 < PATHDIAM)
         TryDirection (pbPath, dwX+1, dwY+1, bValue);
   }
}


/*********************************************************************************
EnumPaths - This takes the path information produced by TryDirection and
enumerates all possible paths (assuming an ever-increasing direction) that
can be produced.

inputs
   PBYTE       pbPath - Pointer to an array of PATHDIAM x PATHDIAM bytes,
               with 0 being none, 0xff being un-marked, else, distance from center
   DWORD       dwX - X to look at
   DWORD       dwY - Y to look at
   POINT       *paHistory - History of points that looked at. This must
               be large enough to handle all possible paths.
   DWORD       dwNum - Number of elements in the history.
   PCListFixed plPaths - List initialized to sizeof of PCListFixed. If an end-path
               is found, a new CListFixed will be created and initialized to
               the points in the history, including the new point. It will be
               added to plPaths
returns
   BOOL - TRUE if found contiuations of path
*/
static BOOL EnumPaths (PBYTE pbPath, DWORD dwX, DWORD dwY, POINT *paHistory,
                       DWORD dwNum, PCListFixed plPaths)
{
   PBYTE pbLook = pbPath + (dwY * PATHDIAM + dwX);
   if (*pbLook <= (BYTE)dwNum)
      return FALSE;  // can't do better than this

   // add to path
   paHistory[dwNum].x = (int)dwX;
   paHistory[dwNum].y = (int)dwY;
   dwNum++;

   // because will have some error if go all the way to the edge, if x or y
   // touch the edge of the data then return FALSE indicating there are no more
   // paths
   if ((dwX == 0) || (dwX == PATHDIAM-1) || (dwY == 0) || (dwY == PATHDIAM-1))
      goto addthis;

   BOOL fFoundBetter = FALSE;

   // because know will stop as soon as touch edge of data, don't need to do
   // boundary checkes here

   // look left
   fFoundBetter |= EnumPaths (pbPath, dwX-1, dwY-1, paHistory, dwNum, plPaths);
   fFoundBetter |= EnumPaths (pbPath, dwX-1, dwY, paHistory, dwNum, plPaths);
   fFoundBetter |= EnumPaths (pbPath, dwX-1, dwY+1, paHistory, dwNum, plPaths);

   // look U/D
   fFoundBetter |= EnumPaths (pbPath, dwX, dwY-1, paHistory, dwNum, plPaths);
   fFoundBetter |= EnumPaths (pbPath, dwX, dwY+1, paHistory, dwNum, plPaths);

   // look right
   fFoundBetter |= EnumPaths (pbPath, dwX+1, dwY-1, paHistory, dwNum, plPaths);
   fFoundBetter |= EnumPaths (pbPath, dwX+1, dwY, paHistory, dwNum, plPaths);
   fFoundBetter |= EnumPaths (pbPath, dwX+1, dwY+1, paHistory, dwNum, plPaths);

   // if have found better then done
   if (fFoundBetter)
      return TRUE;

addthis:
   // else, if no better, then add this as a terminal path
   PCListFixed pNew = new CListFixed;
   if (!pNew)
      return FALSE;
   pNew->Init (sizeof(POINT), paHistory, dwNum);
   plPaths->Add (&pNew);

   return TRUE;
}


/*********************************************************************************
CNPREffectOutlineSketch::GeneratePaths - Generate all paths from the given
pixel that lead out from it.

inputs
   PBYTE       pbOutline - Outline info from TableOutlines
   DWORD       dwWidth - Width of image
   DWORD       dwHeight - Height of image
   BOOL        f360 - TRUE if 360 degree image
   BYTE        bLevel - Level that looking for, 1 for primary, 2=sec, 3=tert
   DWORD       dwX - Starting X (must be marked as type bLevel at dwX, dwY)
   DWORD       dwY - Starting Y 
returns
   PCListFixed - List of paths. Contains PCListFixed elements for each path.
         Each path is a list of POINT structures, starting at dwX, dwY. This must be freed.
*/
PCListFixed CNPREffectOutlineSketch::GeneratePaths (PBYTE pbOutline, DWORD dwWidth, DWORD dwHeight,
                               BOOL f360, BYTE bLevel, DWORD dwX, DWORD dwY)
{

   // go through pbOutline and figure out relevent points to copy over
   BYTE abPath[PATHDIAM][PATHDIAM]; // [y][x]
   memset (abPath, 0, sizeof(abPath));
   DWORD x, y;
   for (y = 0; y < PATHDIAM; y++) {
      int iY = (int)y - PATHRADIUS + (int)dwY;
      if ((iY < 0) || (iY >=(int)dwHeight))
         continue;   // off screen

      for (x = 0; x < PATHDIAM; x++) {
         int iX = (int)x - PATHRADIUS + (int)dwX;
         if ((iX < 0) || (iX >= (int)dwWidth)) {
            if (!f360)
               continue;   // nothing

            // else, modulo
            while (iX < 0)
               iX += (int)dwWidth;
            while (iX >= (int)dwWidth)
               iX -= (int)dwWidth;
         }

         if (pbOutline[iY * (int)dwWidth + iX] == bLevel)
            abPath[y][x] = 0xff;
      } // x
   } // y

   // verify that center point is marked
   if (!abPath[PATHRADIUS][PATHRADIUS])
      return NULL;

   // go through and figure out the distance from the center...
   TryDirection (&abPath[0][0], PATHRADIUS, PATHRADIUS, 1);

   // make a list of all the paths leaving this
   PCListFixed plPaths = new CListFixed;
   POINT aHistory[PATHDIAM * PATHDIAM];   // make sure large enough to handle anything
   if (!plPaths)
      return NULL;
   plPaths->Init (sizeof(PCListFixed));
   EnumPaths (&abPath[0][0], PATHRADIUS, PATHRADIUS, aHistory, 0, plPaths);

   return plPaths;
}




/*********************************************************************************
CNPREffectOutlineSketch::GenerateBestPathSeg - Generate all paths from the given
pixel that lead out from it. NOTE: This is only a small path segment.

inputs
   PBYTE       pbOutline - Outline info from TableOutlines
   DWORD       dwWidth - Width of image
   DWORD       dwHeight - Height of image
   BOOL        f360 - TRUE if 360 degree image
   BYTE        bLevel - Level that looking for, 1 for primary, 2=sec, 3=tert
   DWORD       dwX - Starting X (must be marked as type bLevel at dwX, dwY)
   DWORD       dwY - Starting Y
   fp          fAngleLast - Last angle, or -1000 is any angle is OK. THis
                  tries to keep the same angles
   fp          *pfAngle - Fills this in with the angle of the best path.
returns
   PCListFixed - List of POINT for the path coords, starting at dwX, dwY.
               NULL if no path segment leaving this. This must be freed.
*/
PCListFixed CNPREffectOutlineSketch::GenerateBestPathSeg (PBYTE pbOutline, DWORD dwWidth, DWORD dwHeight,
                               BOOL f360, BYTE bLevel, DWORD dwX, DWORD dwY,
                               fp fAngleLast, fp *pfAngle)
{
   // figure out all the paths leaving from here
   PCListFixed plPaths = GeneratePaths (pbOutline, dwWidth, dwHeight, f360, bLevel, dwX, dwY);
   if (!plPaths)
      return NULL;

   // loop through all the paths and see which one is best
   PCListFixed plBest = NULL;
   fp fBestAngle = 0, fBestDelta = 0;
   DWORD dwBestDist = 0;
   DWORD i;
   PCListFixed *ppl = (PCListFixed*) plPaths->Get(0);
   for (i = 0; i < plPaths->Num(); i++) {
      PCListFixed pl = ppl[i];
      DWORD dwNum = pl->Num();

      // should have at least two elements, otherwise isn't going anywhere
      if (dwNum < 3)
         continue;   // dont add

      // if this is MUCH shorter than the longest found so far then ignore
      if (dwNum < dwBestDist/2)
         continue;

      // figure out the angle
      POINT *pStart = (POINT*)pl->Get(0);
      POINT *pEnd = pStart + (dwNum-1);
      POINT pDelta;
      pDelta.x = pEnd->x - pStart->x;
      pDelta.y = pEnd->y - pStart->y;
      if (!pDelta.x && !pDelta.y)
         continue;   // shouldnt happen
      fp fAngle = atan2 ((fp)pDelta.x, (fp)pDelta.y);

      // what's the delta between this angle and the last angle
      fp fDelta = 0;
      if (fAngleLast != -1000) {
         fDelta = fabs(fAngle - fAngleLast);
         if (fDelta >= 2.0 * PI)
            fDelta -= 2.0 * PI;
         if (fDelta > PI)
            fDelta = 2.0 * PI - fDelta;
      }

      // if too large an angle then dont add
      if (fDelta > m_fMaxAngle * PI * 1.5)
         continue;

      // else, if angle isn't as good as best then skip
      if (plBest && (fDelta > fBestDelta))
         continue;

      // else, keep this
      plBest = pl;
      fBestDelta = fDelta;
      fBestAngle = fAngle;
      dwBestDist = dwNum;
   } // i

   // delete all lists except best
   for (i = 0; i < plPaths->Num(); i++) {
      PCListFixed pl = ppl[i];
      if (pl != plBest)
         delete pl;
   }
   delete plPaths;

   // modify the best so it's properly offseted by dwX and dwY
   if (plBest) {
      POINT *pap = (POINT*) plBest->Get(0);
      for (i = 0; i < plBest->Num(); i++, pap++) {
         pap->x += (int)dwX - PATHRADIUS;
         pap->y += (int)dwY - PATHRADIUS;
      } // i
   }

   // done
   *pfAngle = fBestAngle;
   return plBest;
}


/*********************************************************************************
ClearPixel - This clears a pixel and surrounding pixels of the border indicator.

inputs
   PBYTE       pbOutline - Outline info from TableOutlines
   DWORD       dwWidth - Width of image
   DWORD       dwHeight - Height of image
   BOOL        f360 - TRUE if 360 degree image
   BYTE        bLevel - Level that looking for, 1 for primary, 2=sec, 3=tert
   DWORD       dwX - X pixel
   DWORD       dwY - Y pixel
returns
   none
*/
static __inline void ClearPixel (PBYTE pbOutline, DWORD dwWidth, DWORD dwHeight,
                                 BOOL f360, BYTE bLevel, int dwX, int dwY)
{
   int iX, iY;
   for (iY = (int)dwY - 1; iY <= (int)dwY+1; iY++) {
      if ((iY < 0) || (iY >= (int)dwHeight))
         continue;

      for (iX = (int)dwX - 1; iX <= (int)dwX+1; iX++) {
         int xx = iX;

         if ((xx < 0) || (xx >= (int)dwWidth)) {
            if (!f360)
               continue;
            while (xx < 0)
               xx += (int)dwWidth;
            while (xx >= (int)dwWidth)
               xx -= (int)dwWidth;
         }

         PBYTE pb = pbOutline + (iY * (int)dwWidth + xx);
         if (*pb == bLevel)
            *pb = 0;
      } // iX
   } // iY
}


/*********************************************************************************
CNPREffectOutlineSketch::GenerateBestPath - Generate the best path from the given
pixel. This will clear the information in pbOutline indicating that a border
exists, so it won't be checked again.

inputs
   PBYTE       pbOutline - Outline info from TableOutlines
   DWORD       dwWidth - Width of image
   DWORD       dwHeight - Height of image
   BOOL        f360 - TRUE if 360 degree image
   BYTE        bLevel - Level that looking for, 1 for primary, 2=sec, 3=tert
   DWORD       dwX - Starting X (must be marked as type bLevel at dwX, dwY)
   DWORD       dwY - Starting Y
   DWORD       dwLen - Maximum length in pixels
returns
   PCListFixed - List of POINT for the path coords, starting at dwX, dwY.
               NULL if no path segment leaving this. This must be freed.
*/
PCListFixed CNPREffectOutlineSketch::GenerateBestPath (PBYTE pbOutline, DWORD dwWidth, DWORD dwHeight,
                               BOOL f360, BYTE bLevel, DWORD dwX, DWORD dwY,
                               DWORD dwLen)
{
   PCListFixed plPath = new CListFixed;
   if (!plPath)
      return NULL;
   plPath->Init (sizeof(POINT));

   // make sure to add at least the current point to the path
   POINT p;
   p.x = (int)dwX;
   p.y = (int)dwY;
   plPath->Add (&p);

   // set up some info
   BOOL fCanGoForward = TRUE, fCanGoBackward = TRUE;
   fp fAngleForward = -1000, fAngleBackward = -1000;
   BOOL fForward = TRUE;

   // repeat
   while (dwLen) {
      // if can't go forward or backward then must quit
      if (!fCanGoForward && !fCanGoBackward)
         break;

      // if trying to go forward but cant then go back, and vice versa
      if (fForward && !fCanGoForward)
         fForward = !fForward;
      if (!fForward && !fCanGoBackward)
         fForward = !fForward;

      // if we're going backwards and then backangle is unknown, and the
      // forward angle is known, then set the back angle to the reverse
      if (!fForward && (fAngleBackward == -1000) && (fAngleForward != -1000))
         fAngleBackward = myfmod(fAngleForward, 2.0 * PI) - PI;   // BUGFIX - Was just setting negative

      // point to go forward or backward from?
      POINT *pStart = (POINT*) plPath->Get(fForward ? (plPath->Num()-1) : 0);

      // make sure this is set, since may have accidentally cleared it before
      POINT pMod;
      pMod = *pStart;
      while (pMod.x < 0)
         pMod.x += (int)dwWidth;
      while (pMod.x >= (int)dwWidth)
         pMod.x -= (int)dwWidth;
      if ((pMod.y < 0) || (pMod.y >= (int)dwHeight)) {
         // shouldnt happen
         if (fForward)
            fCanGoForward = FALSE;
         else
            fCanGoBackward = FALSE;
         goto flip;
      }
      pbOutline[pMod.y * (int)dwWidth + pMod.x] = bLevel;

      // find the best path...
      PCListFixed pBest;
      fp fAngle;
      pBest = GenerateBestPathSeg (pbOutline, dwWidth, dwHeight, f360, bLevel,
         (DWORD)pMod.x, (DWORD)pMod.y, fForward ? fAngleForward : fAngleBackward, &fAngle);
      pbOutline[pMod.y * (int)dwWidth + pMod.x] = 0;  // undo any changes

      if (!pBest) {
         // cant go forward/backward anymore
         if (fForward)
            fCanGoForward = FALSE;
         else
            fCanGoBackward = FALSE;
         goto flip;
      }

      // add this to the path list
      POINT *pp = (POINT*) pBest->Get(0);
      DWORD i;
      for (i = 0; dwLen && (i < pBest->Num()); i++, pp++) {
         // BUGFIX - Take this out because clearing pixels around entire path
         // clear out adjacent points so long as this isn't the last one
         //if ((i+1 < pBest->Num()) && (i || (plPath->Num() >= 2)))
         //   ClearPixel (pbOutline, dwWidth, dwHeight, f360, bLevel, pp->x, pp->y);
            // BUGFIX - calling clear-pixel will also clear around starting point...
            // don't clear for first point

         // add to list so long as isn't the first one
         if (!i)
            continue;
         if (fForward)
            plPath->Add (pp);
         else
            plPath->Insert (0, pp);
         dwLen--;
      } // i

      // delete it
      delete pBest;
      if (fForward)
         fAngleForward = fAngle;
      else
         fAngleBackward = fAngle;


flip:
      // fially, flip direction that looking for
      fForward = !fForward;
   }

   // finally, clear pixel where started
   // BUGFIX - Taje this out because clearing pixels around entire path
   // ClearPixel (pbOutline, dwWidth, dwHeight, f360, bLevel, dwX, dwY);
   POINT *pp = (POINT*)plPath->Get(0);
   DWORD i;
   for (i = 0; i < plPath->Num(); i++, pp++)
      ClearPixel (pbOutline, dwWidth, dwHeight, f360, bLevel, pp->x, pp->y);

   return plPath;
}


// BUGBUG - I think outline sketch has a bug when tries to draw a line across
// a 360-degree border. Ends up drawing the line across the entire 360 degree image.
// Some sort of modulo error. This bug also occurs with paint brushes, so it's in the
// line drawing.