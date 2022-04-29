/*********************************************************************************
CNPREffectOutline.cpp - Code for effect

begun 7/4/2004
Copyright 2004 by Mike Rozak
All rights reserved
*/

#include <windows.h>
#include "escarpment.h"
#include "..\M3D.h"
#include "resource.h"



// OUTLINEPAGE - Page info
typedef struct {
   PCNPREffectOutline pe;   // effects list
   int            iVScroll;      // inital scroll
   HBITMAP        hBit;    // bitmap for the image
   PCImage        pTest;   // test image
   PCRenderSuper  pRender; // renderer that generated test image
   PCWorldSocket  pWorld;  // world for image
   BOOL           fAllEffects;   // if TRUE then show all effects combined
   PCNPREffectsList  pAllEffects;   // shows all effects
} OUTLINEPAGE, *POUTLINEPAGE;

#define PENANTI         4        // do 4x4 antialiasing, must be power of 2
#define PENVARNUM       16        // 8 variations in pen size



PWSTR gpszEffectOutline = L"Outline";

/*********************************************************************************
CPenBitfield - Object that is a bitfield for circular pen
*/
class CPenBitfield {
public:
   ESCNEWDELETE;

   CPenBitfield (void);
   ~CPenBitfield (void);

   BOOL Init (DWORD dwDiameter);
   __inline void MergeIn (int iCenterX, int iCenterY,
      DWORD *padwImage, DWORD dwImageWidthDWORD, DWORD dwImageWidth, DWORD dwImageHeight,
      BOOL f360);

   CMem           m_mem;      // memory of DWORDs
   DWORD          *m_padwMem;   // m_mem.p converted to DWORD
   DWORD          m_dwWidth;    // width (in DWORDs)
   DWORD          m_dwHeight;   // height (in pixels)
   DWORD          m_dwMidWidth;  // midpoint width (in pixels)
   DWORD          m_dwMidHeight; // midpoint height (in pixels)
};
typedef CPenBitfield *PCPenBitfield;


/*********************************************************************************
CPenBitfield::Constructor and destructor
*/
CPenBitfield::CPenBitfield (void)
{
   m_padwMem = NULL;
   m_dwWidth = m_dwHeight = 0;
   m_dwMidWidth = m_dwMidHeight = 0;
}

CPenBitfield::~CPenBitfield (void)
{
   // do nothing
}


/*********************************************************************************
CPenBitfield::Init - Initializes the bitfield to include a dot (of bitfields)
centered.

inputs
   DWORD       dwDiameter - Diameter of the dot, in pixels
returns
   BOOL - TRUE if success
*/
BOOL CPenBitfield::Init (DWORD dwDiameter)
{
   m_dwWidth = (dwDiameter + 31) / 32;
   m_dwHeight = dwDiameter;
   DWORD dwNeed = m_dwWidth * m_dwHeight * sizeof(DWORD);
   if (!m_mem.Required (dwNeed))
      return FALSE;
   m_padwMem = (DWORD*)m_mem.p;
   memset (m_padwMem, 0, dwNeed);
   m_dwMidWidth = m_dwWidth*32 / 2;
   m_dwMidHeight = m_dwHeight / 2;

   DWORD y;
   DWORD *padw;
   for (y = 0, padw = m_padwMem; y < m_dwHeight; y++, padw += m_dwWidth) {
      // figure out the size
      fp fx, fy;
      fy = y - (fp)m_dwMidHeight;
      fx = dwDiameter * dwDiameter / 4 - fy * fy;
      if (fx < 0)
         continue;   // out of range
      fx = sqrt(fx);
      
      int iStart, iEnd;
      iStart = (int)((fp)m_dwMidWidth - fx);
      iEnd = (int)((fp)m_dwMidWidth + fx);
      iStart = max(iStart, 0);
      iEnd = min(iEnd, (int)m_dwWidth*32);

      for (; iStart <= iEnd; iStart++)
         padw[iStart / 32] |= (1 << (iStart % 32));
   } // y

   return TRUE;
}



/*********************************************************************************
CPenBitfield::MergeIn - This draws the dot centered at the dwCenterX and
dwCenterY pixel.

inputs
   int         iCenterX - Where to center the dot
   int         iCenterY - Where to center the dot
   DWORD       *padwImage - Array of dwImageWidthDWORD x dwImageHeight DWORDs
   DWORD       dwImageWidthDWORD - WIdth of image (in DWORDs)
   DWORD       dwImageWidth - WIdth in pixels
   DWORD       dwImageHeight - Height in pixels
   BOOL        f360 - If TRUE then wraps on L/R sides
returns
   none
*/
__inline void CPenBitfield::MergeIn (int iCenterX, int iCenterY,
   DWORD *padwImage, DWORD dwImageWidthDWORD, DWORD dwImageWidth, DWORD dwImageHeight,
   BOOL f360)
{
   iCenterX -= (int)m_dwMidWidth;
   iCenterY -= (int)m_dwMidHeight;

   int iCenterX2;    // used for wrap
   DWORD dwWrap;
   if (!f360)
      dwWrap = 1;
   else if (iCenterX < 0) {
      iCenterX2 = iCenterX + (int)dwImageWidth; // on left, so wrap to right
      dwWrap = 2;
   }
   else if (iCenterX + (int)m_dwWidth*32 > (int)dwImageWidth) {
      iCenterX2 = iCenterX - (int)dwImageWidth; // on right, so wrap to left
      dwWrap = 2;
   }
   else
      dwWrap = 1;

   int x, y;
   for (y = 0; y < (int)m_dwHeight; y++, iCenterY++) {
      // if out of range then ignore
      if (iCenterY < 0)
         continue;
      if (iCenterY >= (int)dwImageHeight)
         continue;

      DWORD *padwLine = padwImage + iCenterY * (int)dwImageWidthDWORD;

      DWORD dwVers;
      for (dwVers = 0; dwVers < dwWrap; dwVers++) {
         DWORD *padwFrom = m_padwMem + y * (int)m_dwWidth;
         int iXUse = dwVers ? iCenterX2 : iCenterX;

         for (x = 0; x < (int)m_dwWidth; x++, padwFrom++, iXUse += 32) {
            if (!padwFrom[0])
               continue;

            int iDWORDTo = (iXUse >= 0) ? (iXUse / 32) : ((iXUse-31)/32);
            if (iDWORDTo >= (int)dwImageWidthDWORD)
               continue;   // out of range

            int iOffset = iXUse - iDWORDTo * 32;

            if ((iDWORDTo >= 0) && (iDWORDTo < (int)dwImageWidthDWORD))
               padwLine[iDWORDTo] |= (padwFrom[0] << iOffset);

            iDWORDTo++; // next entry
            if ((iDWORDTo >= 0) && (iDWORDTo < (int)dwImageWidthDWORD) && iOffset)
               padwLine[iDWORDTo] |= (padwFrom[0] >> (32 - iOffset));
         } // x
      } // dwVers - wrap l/r

   } // y
}


/*********************************************************************************
PenBitfieldAlloc - This fills a CListFixed with a set of pens, whose average
size is fWidth pixels, but ranging from fWidth / fVary to fWidth*fVary in size

inputs
   PCListFixed       plPen - Initialized to PCPenBitfield and filled with dwNum pens
                        NOTE: Should call PenBitfieldFree(plPen) when done
   DWORD             dwNum - Number of pens
   fp                fWidth - Width of centeral (dwNum/2) pen, diameter
   fp                fVary - Amount the pen width varies, 2.0 being from 1/2 to 2x of fWidth, 1.0 = none
returns
   BOOL - TRUE if success
*/
BOOL PenBitfieldAlloc (PCListFixed plPen, DWORD dwNum, fp fWidth, fp fVary)
{
   plPen->Init (sizeof(PCPenBitfield));

   DWORD i;
   fp fScale = (fVary > 0) ? pow ((fp)fVary, (fp)(1.0 / ((fp)dwNum/2))) : 1;
   fWidth = fWidth / fVary;
   plPen->Required (dwNum);
   for (i = 0; i < dwNum; i++, fWidth *= fScale) {
      PCPenBitfield pPen = new CPenBitfield;
      if (!pPen)
         return FALSE;

      if (!pPen->Init ((int)max(fWidth,1))) {
         delete pPen;
         return FALSE;
      }

      plPen->Add (&pPen);
   } // i

   return TRUE;
}

/*********************************************************************************
ImageBitfieldAlloc - This allocates the memory requied for an image bitfield,
and fills with 0's.

inputs
   PCMem          pMem - Memory to place image bitfield. pMem->p will point to buffer
   DWORD          dwWidth - Width in pixels of bitfield
   DWORD          dwHeight - Height in pixels of bitfield
returns
   DWORD - Width in DWORDs of bitfield, or 0 if error
*/
DWORD ImageBitfieldAlloc (PCMem pMem, DWORD dwWidth, DWORD dwHeight)
{
   DWORD dwWidthDWORD = (dwWidth+31)/32;
   DWORD dwNeed = dwWidthDWORD * dwHeight * sizeof(DWORD);
   if (!pMem->Required (dwNeed))
      return 0;
   memset (pMem->p, 0, dwNeed);
   return dwWidthDWORD;
}



/*********************************************************************************
PenBitfieldFree - Frees all the pens created by PenBitfieldAlloc()

inputs
   PCListFixed       plPen - Contains the pens
*/
void PenBitfieldFree (PCListFixed plPen)
{
   PCPenBitfield *ppb = (PCPenBitfield*) plPen->Get(0);
   DWORD i;
   for (i = 0; i < plPen->Num(); i++)
      delete ppb[i];
   plPen->Clear();
}


/*********************************************************************************
ImageBitfieldGetAnti - Gets an antialiased pixel from the bitfield, using
PENANTI as amount to antialias.

inputs
   DWORD          x - Pixel, must be within original image size
   DWORD          y - Pixel, must be within original image size
   DWORD          *padwImage - Image bitfield, from ImageBitfieldAlloc
   DWORD          dwWidthDWORD - With of padwImage, in DWORDs
   DWORD          dwWidth - Width of image (in pixels)
   DWORD          dwHeight - Height of image (in pixels)
returns
   WORD - 0xffff if fully filled, 0 if fully empty
*/
__inline WORD ImageBitfieldGetAnti (DWORD x, DWORD y, DWORD *padwImage,
                                    DWORD dwWidthDWORD, DWORD dwWidth, DWORD dwHeight)
{
   // figure out offset
   padwImage += y * PENANTI * dwWidthDWORD;  // account for y offset
   padwImage += (x * PENANTI / 32);  // account for X offset
   DWORD dwMask, i;
   dwMask = (1 << PENANTI)-1;
   dwMask <<= ((x * PENANTI) % 32);

   // loop
   DWORD dwCount = 0;
   for (i = 0; i < PENANTI; i++, padwImage += dwWidthDWORD) {
      DWORD dwValue = padwImage[0] & dwMask;
      while (dwValue) {
         dwCount++;
         dwValue = dwValue & (dwValue - 1);
      }
   } // over i

   return (WORD)(dwCount * 0xffff / PENANTI / PENANTI);
}




/*********************************************************************************
CNPREffectOutline::Constructor and destructor
*/
CNPREffectOutline::CNPREffectOutline (DWORD dwRenderShard)
{
   m_dwRenderShard = dwRenderShard;
   m_dwLevel = 2;
   m_cOutline = RGB(0,0,0);
   m_cOutlineSelected = RGB(0xff, 0,0);
   m_fShadeSelected = TRUE;
   m_fSetAllObjectsToColor = FALSE;
   m_cSetObjectsTo = RGB(0xff, 0xff, 0xff);

   m_pPenWidth.Zero();
   m_pPenWidth.p[0] = 0.5;
   m_pPenWidth.p[1] = 0.25;
   m_pPenWidth.p[2] = 0.15;
   m_fNoiseAmount = 0;
   m_dwNoiseDetail = 50;
}

CNPREffectOutline::~CNPREffectOutline (void)
{
   // do nothing
}


/*********************************************************************************
CNPREffectOutline::Delete - From CNPREffect
*/
void CNPREffectOutline::Delete (void)
{
   delete this;
}


/*********************************************************************************
CNPREffectOutline::QueryInfo - From CNPREffect
*/
void CNPREffectOutline::QueryInfo (PNPRQI pqi)
{
   memset (pqi, 0, sizeof(*pqi));
   pqi->fInPlace = TRUE;
   pqi->pszDesc = L"Outlines the objects in the scene using a toon-style outline.";
   pqi->pszName = L"Outline";
   pqi->pszID = gpszEffectOutline;
}



static PWSTR gpszLevel = L"Level";
static PWSTR gpszOutline = L"Outline";
static PWSTR gpszShadeSelected = L"ShadeSelected";
static PWSTR gpszSelectedColor = L"SelectedColor";
static PWSTR gpszSetAllObjectsToColor = L"SetAllObjectsToColor";
static PWSTR gpszSetObjectsTo = L"SetObjectsTo";
static PWSTR gpszPenWidth = L"PenWidth";
static PWSTR gpszNoiseAmount = L"NoiseAmount";
static PWSTR gpszNoiseDetail = L"NoiseDetail";

/*********************************************************************************
CNPREffectOutline::MMLTo - From CNPREffect
*/
PCMMLNode2 CNPREffectOutline::MMLTo (void)
{
   PCMMLNode2 pNode = new CMMLNode2;
   if (!pNode)
      return NULL;
   pNode->NameSet (gpszEffectOutline);

   MMLValueSet (pNode, gpszLevel, (int)m_dwLevel);
   MMLValueSet (pNode, gpszOutline, (int)m_cOutline);
   MMLValueSet (pNode, gpszShadeSelected, (int)m_fShadeSelected);
   MMLValueSet (pNode, gpszSelectedColor, (int)m_cOutlineSelected);
   MMLValueSet (pNode, gpszSetAllObjectsToColor, (int)m_fSetAllObjectsToColor);
   MMLValueSet (pNode, gpszSetObjectsTo, (int)m_cSetObjectsTo);

   MMLValueSet (pNode, gpszPenWidth, &m_pPenWidth);
   MMLValueSet (pNode, gpszNoiseAmount, m_fNoiseAmount);
   MMLValueSet (pNode, gpszNoiseDetail, (int) m_dwNoiseDetail);

   return pNode;
}


/*********************************************************************************
CNPREffectOutline::MMLFrom - From CNPREffect
*/
BOOL CNPREffectOutline::MMLFrom (PCMMLNode2 pNode)
{
   m_dwLevel = (DWORD) MMLValueGetInt (pNode, gpszLevel, (int)2);
   m_cOutline = (COLORREF) MMLValueGetInt (pNode, gpszOutline, (int)0);
   m_fShadeSelected = (BOOL) MMLValueGetInt (pNode, gpszShadeSelected, (int)TRUE);
   m_cOutlineSelected = (COLORREF) MMLValueGetInt (pNode, gpszSelectedColor, (int)0);
   m_fSetAllObjectsToColor = (BOOL) MMLValueGetInt (pNode, gpszSetAllObjectsToColor, (int)FALSE);
   m_cSetObjectsTo = (COLORREF) MMLValueGetInt (pNode, gpszSetObjectsTo, (int)0);

   MMLValueGetPoint (pNode, gpszPenWidth, &m_pPenWidth);
   m_fNoiseAmount = MMLValueGetDouble (pNode, gpszNoiseAmount, 0);
   m_dwNoiseDetail = (DWORD) MMLValueGetInt (pNode, gpszNoiseDetail, (int) 20);

   return TRUE;
}




/*********************************************************************************
CNPREffectOutline::MMLFrom - From CNPREffect
*/
CNPREffect * CNPREffectOutline::Clone (void)
{
   return CloneEffect ();
}

/*********************************************************************************
CNPREffectOutline::MMLFrom - From CNPREffect
*/
CNPREffectOutline * CNPREffectOutline::CloneEffect (void)
{
   PCNPREffectOutline pNew = new CNPREffectOutline(m_dwRenderShard);
   if (!pNew)
      return NULL;

   pNew->m_cOutline = m_cOutline;
   pNew->m_cOutlineSelected = m_cOutlineSelected;
   pNew->m_cSetObjectsTo = m_cSetObjectsTo;
   pNew->m_dwLevel = m_dwLevel;
   pNew->m_fSetAllObjectsToColor = m_fSetAllObjectsToColor;
   pNew->m_fShadeSelected = m_fShadeSelected;

   pNew->m_pPenWidth.Copy (&m_pPenWidth);
   pNew->m_fNoiseAmount = m_fNoiseAmount;
   pNew->m_dwNoiseDetail = m_dwNoiseDetail;
   return pNew;
}


static int _cdecl BCompare (const void *elem1, const void *elem2)
{
   DWORD *pdw1, *pdw2;
   pdw1 = (DWORD*) elem1;
   pdw2 = (DWORD*) elem2;

   return (int) (*pdw1) - (int)(*pdw2);
}

static __inline BOOL SelectionExists (DWORD dwID, DWORD *padwSelected, DWORD dwSelectedCount)
{
   DWORD *pdw;

   // only use the HIWORD, and -1 at that
   dwID = HIWORD(dwID);
   if (!dwID || !padwSelected)
      return FALSE;
   dwID--;

   pdw = (DWORD*) bsearch (&dwID, padwSelected, dwSelectedCount, sizeof(DWORD), BCompare);
   return pdw ? TRUE : FALSE;
}


/*********************************************************************************
CNPREffectOutline::RenderFinal - Renders a final version of the image.

inputs
   PCImage        pImage - If using integer coloration
   PCFImage       pFImage - If using FP coloration
returns
   BOOL - TRUE if success
*/
BOOL CNPREffectOutline::RenderFinal (PCImage pImage, PCFImage pFImage)
{
   CNoise2D cNoise, cNoiseWobbleX, cNoiseWobbleY;

   if (!m_dwLevel)
      return TRUE;

   // create memory for image
   DWORD dwWidth = pImage ? pImage->Width() : pFImage->Width();
   DWORD dwHeight = pImage ? pImage->Height() : pFImage->Height();
   DWORD dwWidthAnti = dwWidth * PENANTI;
   DWORD dwHeightAnti = dwHeight * PENANTI;
   DWORD dwWidthDWORD = ImageBitfieldAlloc (&m_memOutline, dwWidthAnti, dwHeightAnti);
   if (!dwWidthDWORD)
      return FALSE;
   DWORD *padwImage = (DWORD*)m_memOutline.p;
   BOOL f360 = pImage ? pImage->m_f360 : pFImage->m_f360;

   if (m_fNoiseAmount) {
      DWORD dwGridX = max(m_dwNoiseDetail, 1);
      DWORD dwGridY = max(dwWidth / dwGridX, 1);
      dwGridY = max(dwHeight / dwGridY, 1);  // so have approximately square

      srand (GetTickCount());  // just pick a seed
      cNoise.Init (dwGridX, dwGridY);
      cNoiseWobbleX.Init (dwGridX, dwGridY);
      cNoiseWobbleY.Init (dwGridX, dwGridY);
   }

   // allocate for 3 sizes of pen
   BOOL fRet = TRUE;
   CListFixed alPen[3];
   DWORD i;
   for (i = 0; i < 3; i++)
      fRet = fRet && PenBitfieldAlloc (&alPen[i], PENVARNUM, m_pPenWidth.p[i] / 100.0 * (fp)dwWidthAnti, 1+m_fNoiseAmount);
   if (!fRet)
      goto done;

   // loop
   DWORD x, y;
   PIMAGEPIXEL pip = pImage ? pImage->Pixel(0,0) : NULL;
   PFIMAGEPIXEL pfp = pImage ? NULL : pFImage->Pixel(0,0);
   PIMAGEPIXEL pLook = NULL;
   PFIMAGEPIXEL pfLook = NULL;
   WORD  wMajor, wMinor, wMajorLook, wMinorLook;
   DWORD dwTertiary, dwTertiaryLook;
   DWORD dwDir;
   fp fx, fy;
   for (y = 0; y < dwHeight; y++) {
      for (x = 0; x < dwWidth; x++, pip ? pip++ : (PIMAGEPIXEL)(pfp++)) {
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

            DWORD dwSize;
            if (wMajorLook != wMajor)
               dwSize = 0;
            else if ((wMinorLook != wMinor) && (m_dwLevel >= 2))
               dwSize  = 1;
            else if ((dwTertiaryLook != dwTertiary) && (m_dwLevel >= 3))
               dwSize = 2;
            else
               continue;   // no worthwhile change

            // randomize subsize
            DWORD dwSubSize = PENVARNUM / 2;
            if (m_fNoiseAmount) {
               fx = (fp)x / (fp)dwWidth * (fp)(dwSize+1);
               fy = (fp)y / (fp)dwHeight * (fp)(dwSize+1);
               fp f = cNoise.Value (fx, fy);
               f = (f + 1) / 2 * PENVARNUM;
               f = max(f, 0);
               f = min(f, PENVARNUM-1);
               dwSubSize = (DWORD)f;
            }

            PCPenBitfield pPen = *((PCPenBitfield*) alPen[dwSize].Get(dwSubSize));

            int iXWobble, iYWobble;
            iXWobble = (int) (x + (dwDir ? 0 : 1)) * PENANTI;
            iYWobble = (int)(y + (dwDir ? 1 : 0)) * PENANTI;

            if (m_fNoiseAmount) {
               fp f = cNoiseWobbleX.Value (fx, fy) * (fp)pPen->m_dwHeight / 2.0 * m_fNoiseAmount;
               iXWobble += (int)f;

               f = cNoiseWobbleY.Value (fx, fy) * (fp)pPen->m_dwHeight / 2.0 * m_fNoiseAmount;
               iYWobble += (int)f;
            }

            pPen->MergeIn (iXWobble, iYWobble,
               padwImage, dwWidthDWORD, dwWidthAnti, dwHeightAnti, f360);
         } // dwDir
      } // x
   } // y

   // loop through and re-incorporate
   WORD  awOutline[3];
   Gamma (m_cOutline, awOutline);
   pip = pImage ? pImage->Pixel(0,0) : NULL;
   pfp = pImage ? NULL : pFImage->Pixel(0,0);
   for (y = 0; y < dwHeight; y++) {
      for (x = 0; x < dwWidth; x++, pip ? pip++ : (PIMAGEPIXEL)(pfp++)) {
         DWORD dwPen = (DWORD) ImageBitfieldGetAnti (x, y,
            padwImage, dwWidthDWORD, dwWidthAnti, dwHeightAnti);
         if (!dwPen)
            continue;   // no change


         if (pip) {
            DWORD dwOrig = 0xffff - dwPen;
            pip->wRed = (WORD)(((DWORD)pip->wRed * dwOrig + (DWORD)awOutline[0] * dwPen) >> 16);
            pip->wGreen = (WORD)(((DWORD)pip->wGreen * dwOrig + (DWORD)awOutline[1] * dwPen) >> 16);
            pip->wBlue = (WORD)(((DWORD)pip->wBlue * dwOrig + (DWORD)awOutline[2] * dwPen) >> 16);
         }
         else {
            fp fPen = (fp)dwPen / (fp)0xffff;
            fp fOrig = 1.0 - fPen;

            pfp->fRed = pfp->fRed * fOrig + (fp)awOutline[0] * fPen;
            pfp->fGreen = pfp->fGreen * fOrig + (fp)awOutline[1] * fPen;
            pfp->fBlue = pfp->fBlue * fOrig + (fp)awOutline[2] * fPen;
         }

      } // x
   } // y

done:
   // free pens
   for (i = 0; i < 3; i++)
      PenBitfieldFree (&alPen[i]);
   return fRet;
}


/*********************************************************************************
CNPREffectOutline::Render - From CNPREffect
*/
BOOL CNPREffectOutline::Render (PCImage pOrig, PCImage pDest, PCRenderSuper pRender,
                     PCWorldSocket pWorld, BOOL fFinal, PCProgressSocket pProgress)
{
   if (fFinal)
      return RenderFinal (pDest, NULL);

   DWORD dwSelectedCount = 0;
   DWORD *padwSelected = pWorld ? pWorld->SelectionEnum (&dwSelectedCount) : 0;

   // if outlining is off then don't outline, unless we have a selection
   if (!m_dwLevel && !dwSelectedCount)
      return TRUE;

   WORD  awOutline[3], awFill[3], awSel[3];
   Gamma (m_cOutline, awOutline);
   Gamma (m_cSetObjectsTo, awFill);
   Gamma (m_cOutlineSelected, awSel);

   DWORD dwHeight = pDest->Height();
   DWORD dwWidth = pDest->Width();

   // temporary outline info
   PBYTE    pOut;
   pOut = NULL;
   if (m_memOutline.Required (dwHeight * dwWidth))
      pOut = (PBYTE) m_memOutline.p;

   // loop and find out what's selected
   DWORD x, y, dwMax;
   PIMAGEPIXEL p;
   WORD wLastObject, wCurObject;
   BYTE fWasSelected;
   PBYTE pCurOut;
   wLastObject = 0;
   fWasSelected = FALSE;
   dwMax = dwWidth * dwHeight;
   p = pDest->Pixel(0,0);
   for (pCurOut = pOut, x = dwMax; x; x--, pCurOut++, p++) {
      wCurObject = HIWORD(p->dwID);
      
      // quick check - if same as last object then it's selection info is the same
      if (wCurObject == wLastObject) {
         *pCurOut = fWasSelected;
         continue;
      }

      *pCurOut = fWasSelected = (BYTE) SelectionExists (p->dwID, padwSelected, dwSelectedCount);
      wLastObject = wCurObject;
   }

   // loop and outline
   BOOL fHaveOutline;
   pCurOut = pOut;
   for (y = 0; y < dwHeight; y++) {
      p = pDest->Pixel (0, y);
      for (x = 0; x < dwWidth; x++, p++, pCurOut++) {
         WORD  wMajor, wMinor;
         DWORD dwTertiary;
         wMajor = HIWORD(p->dwID);
         wMinor = LOWORD(p->dwID);
         dwTertiary = p->dwIDPart;

         // colorize based on selection
         if (pCurOut[0]) {
            p->wRed = p->wRed / 4 * 3 + awSel[0] / 4;
            p->wGreen = p->wGreen / 4 * 3 + awSel[1] / 4;
            p->wBlue = p->wBlue / 4 * 3 + awSel[2] / 4;
         }

         // major IDs
         if ( ((x > 0) && (HIWORD(p[-1].dwID) != wMajor)) ||
            ((x+1 < dwWidth) && (HIWORD(p[1].dwID) != wMajor)) ||
            ((y > 0) && (HIWORD(p[-(int)dwWidth].dwID) != wMajor)) ||
            ((y+1 < dwHeight) && (HIWORD(p[dwWidth].dwID) != wMajor)) ) {

            // found match

            // or all the wID part's together. If the transparency bit is set then no outline
            DWORD dwTrans;
            dwTrans = p[0].dwIDPart;
            if (x > 0)
               dwTrans |= p[-1].dwIDPart;
            if (x+1 < dwWidth)
               dwTrans |= p[1].dwIDPart;
            if (y > 0)
               dwTrans |= p[-(int)dwWidth].dwIDPart;
            if (y+1 < dwHeight)
               dwTrans |= p[dwWidth].dwIDPart;
            if (dwTrans & IDPARTBITS_TRANSPARENT)
               continue;

            // if any of the found objects are in the selection pool then
            // outline in a different color

            if ( ((x > 0) && pCurOut[-1]) ||
               ((x+1 < dwWidth) && pCurOut[1]) ||
               ((y > 0) && pCurOut[-(int)dwWidth]) ||
               ((y+1 < dwHeight) && pCurOut[dwWidth]) ) {
               p->wRed = awSel[0];
               p->wGreen = awSel[1];
               p->wBlue = awSel[2];
               fHaveOutline = TRUE;
            }
            else if (m_dwLevel) {  // only outline if have a level
               p->wRed = awOutline[0];
               p->wGreen = awOutline[1];
               p->wBlue = awOutline[2];
               fHaveOutline = TRUE;
            }
            else
               fHaveOutline = FALSE;

            if (fHaveOutline) {
               // set the z buffer here to make sure it's not drawn over
               if (x > 0)
                  p->fZ = min(p->fZ, p[-1].fZ);
               if (x+1 < dwWidth)
                  p->fZ = min(p->fZ, p[1].fZ);
               if (y > 0)
                  p->fZ = min(p->fZ, p[-(int)dwWidth].fZ);
               if (y+1 < dwHeight)
                  p->fZ = min(p->fZ, p[dwWidth].fZ);
            }
            continue;
         }

         // if set objects to color then do so
         if (m_fSetAllObjectsToColor && p->dwID) {
            p->wRed = awFill[0];
            p->wGreen = awFill[1];
            p->wBlue = awFill[2];
         }
         
         // minor
         if (m_dwLevel <= 1)
            continue;   // don't care about minor

         if ( ((x > 0) && (LOWORD(p[-1].dwID) != wMinor)) ||
            ((x+1 < dwWidth) && (LOWORD(p[1].dwID) != wMinor)) ||
            ((y > 0) && (LOWORD(p[-(int)dwWidth].dwID) != wMinor)) ||
            ((y+1 < dwHeight) && (LOWORD(p[dwWidth].dwID) != wMinor)) ) {

            // found match
            p->wRed = awOutline[0]/2 + p->wRed/2;
            p->wGreen = awOutline[1]/2 + p->wGreen/2;
            p->wBlue = awOutline[2]/2 + p->wBlue/2;
            continue;
         }

         // tertiary
         if (m_dwLevel <= 2)
            continue;   // don't care about tertiary
         if ( ((x > 0) && (p[-1].dwIDPart != dwTertiary)) ||
            ((y > 0) && (p[-(int)dwWidth].dwIDPart != dwTertiary)) ) {

            // found match
            p->wRed = awOutline[0]/2 + p->wRed/2;
            p->wGreen = awOutline[1]/2 + p->wGreen/2;
            p->wBlue = awOutline[2]/2 + p->wBlue/2;
//            p->wRed = awOutline[0]/4 + p->wRed/4*3;
//            p->wGreen = awOutline[1]/4 + p->wGreen/4*3;
//            p->wBlue = awOutline[2]/4 + p->wBlue/4*3;
            continue;
         }
      }
   }

   return TRUE;
}



/*********************************************************************************
CNPREffectOutline::Render - From CNPREffect
*/
BOOL CNPREffectOutline::Render (PCFImage pOrig, PCFImage pDest, PCRenderSuper pRender,
                     PCWorldSocket pWorld, BOOL fFinal, PCProgressSocket pProgress)
{
   if (fFinal)
      return RenderFinal (NULL, pDest);

   DWORD dwSelectedCount = 0;
   DWORD *padwSelected = pWorld ? pWorld->SelectionEnum (&dwSelectedCount) : 0;

   // if outlining is off then don't outline, unless we have a selection
   if (!m_dwLevel && !dwSelectedCount)
      return TRUE;

   WORD  awOutline[3], awFill[3], awSel[3];
   Gamma (m_cOutline, awOutline);
   Gamma (m_cSetObjectsTo, awFill);
   Gamma (m_cOutlineSelected, awSel);

   DWORD dwHeight = pDest->Height();
   DWORD dwWidth = pDest->Width();

   // temporary outline info
   PBYTE    pOut;
   pOut = NULL;
   if (m_memOutline.Required (dwHeight * dwWidth))
      pOut = (PBYTE) m_memOutline.p;

   // loop and find out what's selected
   DWORD x, y, dwMax;
   PFIMAGEPIXEL p;
   WORD wLastObject, wCurObject;
   BYTE fWasSelected;
   PBYTE pCurOut;
   wLastObject = 0;
   fWasSelected = FALSE;
   dwMax = dwWidth * dwHeight;
   p = pDest->Pixel(0,0);
   for (pCurOut = pOut, x = dwMax; x; x--, pCurOut++, p++) {
      wCurObject = HIWORD(p->dwID);
      
      // quick check - if same as last object then it's selection info is the same
      if (wCurObject == wLastObject) {
         *pCurOut = fWasSelected;
         continue;
      }

      *pCurOut = fWasSelected = (BYTE) SelectionExists (p->dwID, padwSelected, dwSelectedCount);
      wLastObject = wCurObject;
   }

   // loop and outline
   BOOL fHaveOutline;
   pCurOut = pOut;
   for (y = 0; y < dwHeight; y++) {
      p = pDest->Pixel (0, y);
      for (x = 0; x < dwWidth; x++, p++, pCurOut++) {
         WORD  wMajor, wMinor;
         DWORD dwTertiary;
         wMajor = HIWORD(p->dwID);
         wMinor = LOWORD(p->dwID);
         dwTertiary = p->dwIDPart;

         // colorize based on selection
         if (pCurOut[0]) {
            p->fRed = p->fRed / 4 * 3 + (float)awSel[0] / 4;
            p->fGreen = p->fGreen / 4 * 3 + (float)awSel[1] / 4;
            p->fBlue = p->fBlue / 4 * 3 + (float)awSel[2] / 4;
         }

         // major IDs
         if ( ((x > 0) && (HIWORD(p[-1].dwID) != wMajor)) ||
            ((x+1 < dwWidth) && (HIWORD(p[1].dwID) != wMajor)) ||
            ((y > 0) && (HIWORD(p[-(int)dwWidth].dwID) != wMajor)) ||
            ((y+1 < dwHeight) && (HIWORD(p[dwWidth].dwID) != wMajor)) ) {

            // found match

            // or all the wID part's together. If the transparency bit is set then no outline
            DWORD dwTrans;
            dwTrans = p[0].dwIDPart;
            if (x > 0)
               dwTrans |= p[-1].dwIDPart;
            if (x+1 < dwWidth)
               dwTrans |= p[1].dwIDPart;
            if (y > 0)
               dwTrans |= p[-(int)dwWidth].dwIDPart;
            if (y+1 < dwHeight)
               dwTrans |= p[dwWidth].dwIDPart;
            if (dwTrans & IDPARTBITS_TRANSPARENT)
               continue;

            // if any of the found objects are in the selection pool then
            // outline in a different color

            if ( ((x > 0) && pCurOut[-1]) ||
               ((x+1 < dwWidth) && pCurOut[1]) ||
               ((y > 0) && pCurOut[-(int)dwWidth]) ||
               ((y+1 < dwHeight) && pCurOut[dwWidth]) ) {
               p->fRed = awSel[0];
               p->fGreen = awSel[1];
               p->fBlue = awSel[2];
               fHaveOutline = TRUE;
            }
            else if (m_dwLevel) {  // only outline if have a level
               p->fRed = awOutline[0];
               p->fGreen = awOutline[1];
               p->fBlue = awOutline[2];
               fHaveOutline = TRUE;
            }
            else
               fHaveOutline = FALSE;

            if (fHaveOutline) {
               // set the z buffer here to make sure it's not drawn over
               if (x > 0)
                  p->fZ = min(p->fZ, p[-1].fZ);
               if (x+1 < dwWidth)
                  p->fZ = min(p->fZ, p[1].fZ);
               if (y > 0)
                  p->fZ = min(p->fZ, p[-(int)dwWidth].fZ);
               if (y+1 < dwHeight)
                  p->fZ = min(p->fZ, p[dwWidth].fZ);
            }
            continue;
         }

         // if set objects to color then do so
         if (m_fSetAllObjectsToColor && p->dwID) {
            p->fRed = awFill[0];
            p->fGreen = awFill[1];
            p->fBlue = awFill[2];
         }
         
         // minor
         if (m_dwLevel <= 1)
            continue;   // don't care about minor

         if ( ((x > 0) && (LOWORD(p[-1].dwID) != wMinor)) ||
            ((x+1 < dwWidth) && (LOWORD(p[1].dwID) != wMinor)) ||
            ((y > 0) && (LOWORD(p[-(int)dwWidth].dwID) != wMinor)) ||
            ((y+1 < dwHeight) && (LOWORD(p[dwWidth].dwID) != wMinor)) ) {

            // found match
            p->fRed = (float)awOutline[0]/2 + p->fRed/2;
            p->fGreen = (float)awOutline[1]/2 + p->fGreen/2;
            p->fBlue = (float)awOutline[2]/2 + p->fBlue/2;
            continue;
         }

         // tertiary
         if (m_dwLevel <= 2)
            continue;   // don't care about tertiary
         if ( ((x > 0) && (p[-1].dwIDPart != dwTertiary)) ||
            ((y > 0) && (p[-(int)dwWidth].dwIDPart != dwTertiary)) ) {

            // found match
            p->fRed = (float)awOutline[0]/2 + p->fRed/2;
            p->fGreen = (float)awOutline[1]/2 + p->fGreen/2;
            p->fBlue = (float)awOutline[2]/2 + p->fBlue/2;
//            p->wRed = awOutline[0]/4 + p->wRed/4*3;
//            p->wGreen = awOutline[1]/4 + p->wGreen/4*3;
//            p->wBlue = awOutline[2]/4 + p->wBlue/4*3;
            continue;
         }
      }
   }

   return TRUE;
}




/****************************************************************************
EffectOutlinePage
*/
BOOL EffectOutlinePage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   POUTLINEPAGE pmp = (POUTLINEPAGE)pPage->m_pUserData;
   PCNPREffectOutline pv = pmp->pe;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         // set button for show all effects
         PCEscControl pControl;
         pControl = pPage->ControlFind (L"alleffects");
         if (pControl)
            pControl->AttribSetBOOL (Checked(), pmp->fAllEffects);

         FillStatusColor (pPage, L"color", pv->m_cOutline);
         FillStatusColor (pPage, L"selcolor", pv->m_cOutlineSelected);

         // set the radio button
         PWSTR pszName;
         switch (pv->m_dwLevel) {
         case 0:
            pszName = L"off";
            break;
         case 1:
            pszName = L"out1";
            break;
         case 2:
            pszName = L"out2";
            break;
         default:
         case 3:
            pszName = L"out3";
         }
         pControl = pPage->ControlFind (pszName);
         if (pControl)
            pControl->AttribSetBOOL (Checked(), TRUE);

         pControl = pPage->ControlFind (L"noiseamount");
         if (pControl)
            pControl->AttribSetInt (Pos(), (int)(pv->m_fNoiseAmount * 100));
         pControl = pPage->ControlFind (L"noisedetail");
         if (pControl)
            pControl->AttribSetInt (Pos(), (int)pv->m_dwNoiseDetail);

         // values
         DWORD i;
         WCHAR szTemp[32];
         for (i = 0; i < 3; i++) {
            swprintf (szTemp, L"penwidth%d", (int)i);
            DoubleToControl (pPage, szTemp, pv->m_pPenWidth.p[i]);
         }
      }
      break;

   case ESCN_EDITCHANGE:
      {
         // just get all the values
         DWORD i;
         WCHAR szTemp[32];
         for (i = 0; i < 3; i++) {
            swprintf (szTemp, L"penwidth%d", (int)i);
            pv->m_pPenWidth.p[i] = DoubleFromControl (pPage, szTemp);
            pv->m_pPenWidth.p[i] = max(pv->m_pPenWidth.p[i], 0);
         }
         pPage->Message (ESCM_USER+189);  // update bitmap
      }
      break;

   case ESCN_SCROLL:
   case ESCN_SCROLLING:
      {
         PESCNSCROLL p = (PESCNSCROLL)pParam;
         PWSTR psz = p->pControl->m_pszName;
         if (!psz)
            break;

         if (!_wcsicmp(psz, L"noiseamount")) {
            fp f = (fp)p->iPos / 100.0;
            if (f != pv->m_fNoiseAmount) {
               pv->m_fNoiseAmount = f;
               pPage->Message (ESCM_USER+189);  // update bitmap
            }
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"noisedetail")) {
            if (p->iPos != (int)pv->m_dwNoiseDetail) {
               pv->m_dwNoiseDetail = (DWORD)p->iPos;
               pPage->Message (ESCM_USER+189);  // update bitmap
            }
            return TRUE;
         }

      }
      break;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;

         // see about all effects checked or unchecked
         if (!_wcsicmp(p->pControl->m_pszName, L"alleffects")) {
            pmp->fAllEffects = p->pControl->AttribGetBOOL (Checked());
            pPage->Message (ESCM_USER+189);  // update bitmap
            return TRUE;
         }


         DWORD dwOutline;
         COLORREF cOutline, cOutlineSelected;
         dwOutline = pv->m_dwLevel;
         cOutline = pv->m_cOutline;
         cOutlineSelected = pv->m_cOutlineSelected;

         // if it's for the colors then do those
         if (!_wcsicmp(p->pControl->m_pszName, L"changecolor")) {
            pv->m_cOutline = AskColor (pPage->m_pWindow->m_hWnd, pv->m_cOutline, pPage, L"color");
            pPage->Message (ESCM_USER+189);  // update bitmap
         }
         if (!_wcsicmp(p->pControl->m_pszName, L"changeselcolor")) {
            pv->m_cOutlineSelected = AskColor (pPage->m_pWindow->m_hWnd, pv->m_cOutlineSelected, pPage, L"selcolor");
            pPage->Message (ESCM_USER+189);  // update bitmap
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"off")) {
            pv->m_dwLevel = 0;
            pPage->Message (ESCM_USER+189);  // update bitmap
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"out1")) {
            pv->m_dwLevel = 1;
            pPage->Message (ESCM_USER+189);  // update bitmap
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"out2")) {
            pv->m_dwLevel = 2;
            pPage->Message (ESCM_USER+189);  // update bitmap
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"out3")) {
            pv->m_dwLevel = 3;
            pPage->Message (ESCM_USER+189);  // update bitmap
         }

      }
      break;   // default

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

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Outlines";
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
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}

/*********************************************************************************
CNPREffectOutline::IsPainterly - Returns TRUE if is a painterly effect
and doesn't need as high a resolution
*/
BOOL CNPREffectOutline::IsPainterly (void)
{
   return FALSE;
}

/*********************************************************************************
CNPREffectOutline::Dialog - From CNPREffect
*/
BOOL CNPREffectOutline::Dialog (PCEscWindow pWindow, PCNPREffectsList pAllEffects, PCImage pTest, 
                                PCRenderSuper pRender, PCWorldSocket pWorld)
{
   PWSTR pszRet;
   OUTLINEPAGE mp;
   memset (&mp, 0, sizeof(mp));
   mp.pe = this;
   mp.pTest = pTest;
   mp.pRender = pRender;
   mp.pWorld = pWorld;
   mp.pAllEffects = pAllEffects;

   // delete existing
   if (mp.hBit)
      DeleteObject (mp.hBit);
   if (mp.fAllEffects)
      mp.hBit = EffectImageToBitmap (pTest, pAllEffects, NULL, pRender, pWorld);
   else
      mp.hBit = EffectImageToBitmap (pTest, NULL, this, pRender, pWorld);


   pszRet = pWindow->PageDialog (ghInstance, IDR_MMLEFFECTOUTLINE, EffectOutlinePage, &mp);

   // elete
   if (mp.hBit)
      DeleteObject (mp.hBit);
   mp.hBit = NULL;

   if (pszRet && !_wcsicmp(pszRet, Back()))
      return TRUE;
   return FALSE;
}

