/*********************************************************************************
CNPREffectHalftone.cpp - Code for effect

begun 15/4/2004
Copyright 2004 by Mike Rozak
All rights reserved
*/

#include <windows.h>
#include "escarpment.h"
#include "..\M3D.h"
#include "resource.h"






// HALFTONEPAGE - Page info
typedef struct {
   PCNPREffectHalftone pe;   // effects list
   int            iVScroll;      // inital scroll
   HBITMAP        hBit;    // bitmap for the image
   PCImage        pTest;   // test image
   PCRenderSuper  pRender; // renderer that generated test image
   PCWorldSocket  pWorld;  // world for image
   BOOL           fAllEffects;   // if TRUE then show all effects combined
   PCNPREffectsList  pAllEffects;   // shows all effects
} HALFTONEPAGE, *PHALFTONEPAGE;



PWSTR gpszEffectHalftone = L"Halftone";


// CHalftone - For creating halftone map
class CHalftone {
public:
   ESCNEWDELETE;

   CHalftone (void);
   ~CHalftone (void);

   BOOL Init (DWORD dwWidth, DWORD dwHeight, DWORD dwPattern);
   WORD ValueGet (DWORD dwX, DWORD dwY, DWORD dwAnti, WORD wOrig);

   // read but dont modify
   DWORD          m_dwWidth;  // width in pixels
   DWORD          m_dwHeight; // height in pixels
   WORD           *m_pawHalftone; // pointer to array of m_dwWidth x m_dwHeight WORDs
                              // with their halftoning values

private:
   BOOL InitAlloc (DWORD dwWidth, DWORD dwHeight);
   BOOL Normalize (void);

   CMem        m_memHalftone; // memory containing WORD values for threshhold
};
typedef CHalftone *PCHalftone;


// EMTHALFTONE- Multhtreaded outline info
typedef struct {
   DWORD          dwStart;
   DWORD          dwEnd;
   PCImage        pImage;
   PCFImage       pFImage;
   PCHalftone     pHalfTone;
} EMTHALFTONE, *PEMTHALFTONE;


/*********************************************************************************
CHalftone::Constructor and destructor
*/
CHalftone::CHalftone (void)
{
   m_dwWidth = m_dwHeight = 0;
   m_pawHalftone = NULL;
}

CHalftone::~CHalftone (void)
{
   // do nothing
}


/*********************************************************************************
CHalftone::Init - Initializes the halftone to a given size and pattern.

inputs
   DWORD          dwWidth - Width in pixels
   DWORD          dwHeight - Height in pixels
   DWORD          dwPattern - Pattern
returns
   BOOL - TRUE if success
*/
BOOL CHalftone::Init (DWORD dwWidth, DWORD dwHeight, DWORD dwPattern)
{
   if (!InitAlloc (dwWidth, dwHeight))
      return FALSE;

   DWORD x, y;
   WORD *paw = m_pawHalftone;
   fp f;
   for (y = 0; y < m_dwHeight; y++)
      for (x = 0; x < m_dwWidth; x++, paw++) {
         fp fx = ((fp)x + 0.1) / (fp)m_dwWidth;
         fp fy = ((fp)y + 0.2) / (fp)m_dwHeight;

         switch (dwPattern) {
         case 0: // standard rount
         default:
            f = sin(fx * 2.0 * PI) *
               sin(fy * 2.0 * PI);
               // BUGFIX - Add slight offsets to x and y, 0.1 and 0.2 so wont have numbers
               // being the same, and more predictable ordering
            f = (f+1) / 2.0;
            break;

         case 1: // square
            f = ((fx < 0.5) ? (fx * 4 - 1) : ((1.0 - fx) * 4 - 1)) *
               ((fy < 0.5) ? (fy * 4 - 1) : ((1.0 - fy) * 4 - 1));
            f = (f+1) / 2.0;
            f = min(f, 1);
            f = max(f, 0);
            break;

         case 2:  // horzontal line
            f = ((fy < 0.5) ? (fy * 2) : ((1.0 - fy) * 2)) + sin(fx * PI) / 100.0;
            f = min(f,1);
            break;

         case 3:  // vertical line
            f = ((fx < 0.5) ? (fx * 2) : ((1.0 - fx) * 2)) + sin(fy * PI) / 100.0;
            f = min(f,1);
            break;
         }; // switch

         paw[0] = (WORD)(f * (fp)0xffff);
      } // x,y

   // normnalize so get good distribution
   if (!Normalize ())
      return FALSE;

   return TRUE;
}

/*********************************************************************************
CHalftone::Init - Initializes the halftone to a given size.

inputs
   DWORD          dwWidth - Width in pixels
   DWORD          dwHeight - Height in pixels
returns
   BOOL - TRUE if success
*/
BOOL CHalftone::InitAlloc (DWORD dwWidth, DWORD dwHeight)
{
   m_dwWidth = dwWidth;
   m_dwHeight = dwHeight;
   if (!m_memHalftone.Required (dwWidth * dwHeight * sizeof(WORD)))
      return FALSE;
   m_pawHalftone = (WORD*)m_memHalftone.p;
   return TRUE;
}

/*********************************************************************************
CHalftone::Normalize - Normalizes the pattern in the halftone so that
the grey scale is evenly distributed.

returns
   BOOL - TRUE if success
*/
typedef struct {
   DWORD       dwX;     // x pixel
   DWORD       dwY;     // y pixel
   WORD        wValue;  // original value at X and Y
} HALFNORM, *PHALFNORM;

static int __cdecl NormalizeCompare (const void *p1, const void *p2)
{
   PHALFNORM ph1 = (PHALFNORM)p1;
   PHALFNORM ph2 = (PHALFNORM)p2;

   return (int)(DWORD)ph1->wValue - (int)(DWORD)ph2->wValue;
}

BOOL CHalftone::Normalize (void)
{
   CMem memNorm;
   if (!memNorm.Required (m_dwWidth * m_dwHeight * sizeof (HALFNORM)))
      return FALSE;
   PHALFNORM phn = (PHALFNORM)memNorm.p;

   // get all the values
   DWORD x,y;
   PHALFNORM phnTemp = phn;
   WORD *paw = m_pawHalftone;
   for (y = 0; y < m_dwHeight; y++)
      for (x = 0; x < m_dwWidth; x++, paw++, phnTemp++) {
         phnTemp->dwX = x;
         phnTemp->dwY = y;
         phnTemp->wValue = paw[0];
      } // x,y

   // sort them
   DWORD dwMax;
   qsort (phn, dwMax = m_dwWidth * m_dwHeight, sizeof(HALFNORM), NormalizeCompare);

   // loop back over and fill in
   phnTemp = phn;
   for (x = 0; x < dwMax; x++, phnTemp++) {
      WORD wValue = (WORD)((fp)x * (fp)0xffff / (fp)dwMax);
      paw = m_pawHalftone + (phnTemp->dwX + phnTemp->dwY * m_dwWidth);
      paw[0] = wValue;
   }

   // done
   return TRUE;
}


/*********************************************************************************
CHalftone::ValueGet - Returns the halftoning value.

inputs
   DWORD          dwX - X pixel from the image
   DWORD          dwY - Y pixel from the image
   DWORD          dwAnti - Amount of antiliasing, usually 4 => 4x4
   WORD           wOrig - Original value at dwX,dwY
returns
   WORD - Antialiases halftone value.
*/
WORD CHalftone::ValueGet (DWORD dwX, DWORD dwY, DWORD dwAnti, WORD wOrig)
{
   DWORD x,y;
   DWORD dwCount = 0;
   DWORD dwAntiExtra = dwAnti+2; // BUGFIX - Anti-alias one extra around so smoother
   dwX *= dwAnti;
   dwY *= dwAnti;
   dwX += m_dwWidth - 1;   // account for antiextra, subtracting 1
   dwY += m_dwHeight - 1;   // account for antiextra, subtracting 1

   for (y = 0; y < dwAntiExtra; y++) {
      DWORD yy = (dwY + y) % m_dwHeight;
      yy *= m_dwWidth;

      for (x = 0; x < dwAntiExtra; x++) {
         DWORD xx = (dwX + x) % m_dwWidth;
         WORD *paw = m_pawHalftone + (xx + yy); // since yy already includes with
         if (wOrig > paw[0])
            dwCount++;
      } // x
   } // y

   return (WORD)(dwCount * (DWORD)0xffff / (dwAntiExtra * dwAntiExtra));
}


/*********************************************************************************
CNPREffectHalftone::Constructor and destructor
*/
CNPREffectHalftone::CNPREffectHalftone (DWORD dwRenderShard)
{
   m_dwRenderShard = dwRenderShard;
   m_fIgnoreBackground = TRUE;

   m_fColor = FALSE;
   m_dwPattern = 0;
   m_fSize = 1;
}

CNPREffectHalftone::~CNPREffectHalftone (void)
{
   // do nothing
}


/*********************************************************************************
CNPREffectHalftone::Delete - From CNPREffect
*/
void CNPREffectHalftone::Delete (void)
{
   delete this;
}


/*********************************************************************************
CNPREffectHalftone::QueryInfo - From CNPREffect
*/
void CNPREffectHalftone::QueryInfo (PNPRQI pqi)
{
   memset (pqi, 0, sizeof(*pqi));
   pqi->fInPlace = TRUE;
   pqi->pszDesc = L"Makes you image look like it's printed in a newspaper.";
   pqi->pszName = L"Halftone";
   pqi->pszID = gpszEffectHalftone;
}



static PWSTR gpszIgnoreBackground = L"IgnoreBackground";
static PWSTR gpszColor = L"Color";
static PWSTR gpszSize = L"Size";
static PWSTR gpszPattern = L"Pattern";

/*********************************************************************************
CNPREffectHalftone::MMLTo - From CNPREffect
*/
PCMMLNode2 CNPREffectHalftone::MMLTo (void)
{
   PCMMLNode2 pNode = new CMMLNode2;
   if (!pNode)
      return NULL;
   pNode->NameSet (gpszEffectHalftone);

   MMLValueSet (pNode, gpszIgnoreBackground, (int)m_fIgnoreBackground);

   MMLValueSet (pNode, gpszColor, (int)m_fColor);
   MMLValueSet (pNode, gpszPattern, (int)m_dwPattern);
   MMLValueSet (pNode, gpszSize, m_fSize);

   return pNode;
}


/*********************************************************************************
CNPREffectHalftone::MMLFrom - From CNPREffect
*/
BOOL CNPREffectHalftone::MMLFrom (PCMMLNode2 pNode)
{
   m_fIgnoreBackground = (BOOL) MMLValueGetInt (pNode, gpszIgnoreBackground, (int)TRUE);

   m_fColor = (BOOL) MMLValueGetInt (pNode, gpszColor, (int)0);
   m_dwPattern = (DWORD) MMLValueGetInt (pNode, gpszPattern, (int)0);
   m_fSize = MMLValueGetDouble (pNode, gpszSize, 0.1);

   return TRUE;
}




/*********************************************************************************
CNPREffectHalftone::MMLFrom - From CNPREffect
*/
CNPREffect * CNPREffectHalftone::Clone (void)
{
   return CloneEffect ();
}

/*********************************************************************************
CNPREffectHalftone::CloneEffect - From CNPREffect
*/
CNPREffectHalftone * CNPREffectHalftone::CloneEffect (void)
{
   PCNPREffectHalftone pNew = new CNPREffectHalftone(m_dwRenderShard);
   if (!pNew)
      return NULL;

   pNew->m_fIgnoreBackground = m_fIgnoreBackground;
   pNew->m_fColor = m_fColor;
   pNew->m_dwPattern = m_dwPattern;
   pNew->m_fSize = m_fSize;

   return pNew;
}



#define HALFTONEANTI       4     // use 4x4 antialiasing

/*********************************************************************************
CNPREffectHalftone::EscMultiThreadedCallback - Handles multithreaded code

Standard API
*/
void CNPREffectHalftone::EscMultiThreadedCallback (PVOID pParams, DWORD dwParamSize,
      DWORD dwThread)
{
   PEMTHALFTONE pep = (PEMTHALFTONE)pParams;

   PCImage pImage = pep->pImage;
   PCFImage pFImage = pep->pFImage;

   PIMAGEPIXEL pip = pImage ? pImage->Pixel(0,0) : NULL;
   PFIMAGEPIXEL pfp = pImage ? NULL : pFImage->Pixel(0,0);
   DWORD dwWidth = pImage ? pImage->Width() : pFImage->Width();
   DWORD dwHeight = pImage ? pImage->Height() : pFImage->Height();
   PCHalftone pHalfTone = pep->pHalfTone;

   DWORD x,y, j;
   WORD aw[3];
   WORD *paw = &aw[0];

   if (pip)
      pip += pep->dwStart * dwWidth;
   if (pfp)
      pfp += pep->dwStart * dwWidth;

   for (y = pep->dwStart; y < pep->dwEnd; y++) {
      for (x = 0; x < dwWidth; x++, (pip ? pip++ : (PIMAGEPIXEL)(pfp++))) {
         if (m_fIgnoreBackground) {
            WORD w = pip ? HIWORD(pip->dwID) : HIWORD(pfp->dwID);
            if (!w)
               continue;
         }

         if (pip)
            paw = &pip->wRed;
         else for (j = 0; j < 3; j++) {
            float f = (&pfp->fRed)[j];
            if (f < 0)
               paw[j] = 0;
            else if (f >= 0xffff)
               paw[j] = 0xffff;
            else
               paw[j] = (WORD)f;
         } // j

         // depending upon color or not
         if (m_fColor) {
            for (j = 0; j < 3; j++)
               paw[j] = pHalfTone->ValueGet (x, y, HALFTONEANTI, paw[j]);
         }
         else {
            WORD wBW = paw[0] / 10 * 3 + paw[1] / 2 + paw[2] / 10 * 2;
            wBW = pHalfTone->ValueGet (x, y, HALFTONEANTI, wBW);
            paw[0] = paw[1] = paw[2] = wBW;
         }

         // potentially set fp values
         if (pfp) for (j = 0; j < 3; j++)
            (&pfp->fRed)[j] = paw[j];

      } // x
   } // y

}



/*********************************************************************************
CNPREffectHalftone::RenderAny - This renders both integer and FP image.

inputs
   PCImage        pImage - Image. If NULL then use pFImage
   PCFImage       pFImage - Floating point image. If NULL then use pImage
returns
   BOOL - TRUE if success
*/

BOOL CNPREffectHalftone::RenderAny (PCImage pImage, PCFImage pFImage)
{
   CHalftone HalfTone;
   PIMAGEPIXEL pip = pImage ? pImage->Pixel(0,0) : NULL;
   PFIMAGEPIXEL pfp = pImage ? NULL : pFImage->Pixel(0,0);
   DWORD dwWidth = pImage ? pImage->Width() : pFImage->Width();
   DWORD dwHeight = pImage ? pImage->Height() : pFImage->Height();

   DWORD dwHalf = (DWORD) ((fp)dwWidth * (fp)HALFTONEANTI * m_fSize / 100.0);
   dwHalf = max(dwHalf, 2);
   dwHalf = min(dwHalf, 100 * HALFTONEANTI);
   if (!HalfTone.Init (dwHalf, dwHalf, m_dwPattern))
      return FALSE;

   // BUGFIX - Multithreded
   EMTHALFTONE em;
   memset (&em, 0, sizeof(em));
   em.pFImage = pFImage;
   em.pImage = pImage;
   em.pHalfTone = &HalfTone;
   ThreadLoop (0, dwHeight, 1, &em, sizeof(em));

   return TRUE;
}


/*********************************************************************************
CNPREffectHalftone::Render - From CNPREffect
*/
BOOL CNPREffectHalftone::Render (PCImage pOrig, PCImage pDest, PCRenderSuper pRender,
                     PCWorldSocket pWorld, BOOL fFinal, PCProgressSocket pProgress)
{
   return RenderAny (pDest, NULL);
}



/*********************************************************************************
CNPREffectHalftone::Render - From CNPREffect
*/
BOOL CNPREffectHalftone::Render (PCFImage pOrig, PCFImage pDest, PCRenderSuper pRender,
                     PCWorldSocket pWorld, BOOL fFinal, PCProgressSocket pProgress)
{
   return RenderAny (NULL, pDest);
}




/****************************************************************************
EffectHalftonePage
*/
BOOL EffectHalftonePage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PHALFTONEPAGE pmp = (PHALFTONEPAGE)pPage->m_pUserData;
   PCNPREffectHalftone pv = pmp->pe;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         // set button for show all effects
         PCEscControl pControl;
         pControl = pPage->ControlFind (L"alleffects");
         if (pControl)
            pControl->AttribSetBOOL (Checked(), pmp->fAllEffects);

         // set the checkbox
         if (pControl = pPage->ControlFind (L"ignorebackground"))
            pControl->AttribSetBOOL (Checked(), pv->m_fIgnoreBackground);
         if (pControl = pPage->ControlFind (L"color"))
            pControl->AttribSetBOOL (Checked(), pv->m_fColor);

         DoubleToControl (pPage, L"size", pv->m_fSize);
         ComboBoxSet (pPage, L"pattern", pv->m_dwPattern);
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

         if (!_wcsicmp(p->pControl->m_pszName, L"ignorebackground")) {
            BOOL fNew;
            fNew = p->pControl->AttribGetBOOL (Checked());
            if (fNew != pv->m_fIgnoreBackground) {
               pv->m_fIgnoreBackground = fNew;
               pPage->Message (ESCM_USER+189);  // update bitmap
            }
            return TRUE;
         };

         if (!_wcsicmp(p->pControl->m_pszName, L"color")) {
            pv->m_fColor = p->pControl->AttribGetBOOL (Checked());
            pPage->Message (ESCM_USER+189);  // update bitmap
            return TRUE;
         }
      }
      break;   // default

   case ESCN_COMBOBOXSELCHANGE:
      {
         PESCNCOMBOBOXSELCHANGE p = (PESCNCOMBOBOXSELCHANGE)pParam;

         if (!p->pControl || !p->pControl->m_pszName)
            break;
         PWSTR psz = p->pControl->m_pszName;

         if (!_wcsicmp(psz, L"pattern")) {
            DWORD dwVal = p->pszName ? _wtoi(p->pszName) : 0;
            if (dwVal == pv->m_dwPattern)
               return TRUE;   // no change

            pv->m_dwPattern = dwVal;
            pPage->Message (ESCM_USER+189);  // update bitmap

            return TRUE;
         } // pattern
      }
      break;

   case ESCN_EDITCHANGE:
      {
         pv->m_fSize = DoubleFromControl (pPage, L"size");

         pPage->Message (ESCM_USER+189);  // update bitmap
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

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Halftone";
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
CNPREffectHalftone::IsPainterly - Returns TRUE if is a painterly effect
and doesn't need as high a resolution
*/
BOOL CNPREffectHalftone::IsPainterly (void)
{
   return TRUE;
}

/*********************************************************************************
CNPREffectHalftone::Dialog - From CNPREffect
*/
BOOL CNPREffectHalftone::Dialog (PCEscWindow pWindow, PCNPREffectsList pAllEffects, PCImage pTest, 
                            PCRenderSuper pRender, PCWorldSocket pWorld)
{
   PWSTR pszRet;
   HALFTONEPAGE mp;
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

   pszRet = pWindow->PageDialog (ghInstance, IDR_MMLEFFECTHALFTONE, EffectHalftonePage, &mp);

   // elete
   if (mp.hBit)
      DeleteObject (mp.hBit);
   mp.hBit = NULL;

   if (pszRet && !_wcsicmp(pszRet, Back()))
      return TRUE;
   return FALSE;
}

