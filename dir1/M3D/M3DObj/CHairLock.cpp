/************************************************************************
CHairLock.cpp - Draws a HairLock.

begun 26/11/03 by Mike Rozak
Copyright 2003 Mike Rozak. All rights reserved
*/

#include <windows.h>
#include "escarpment.h"
#include "..\M3D.h"
#include "resource.h"


#define MAXLOCKNODES    50       // cant have any more than this many nodes

/**********************************************************************************
CHairLock - Constructor and destructor
*/
CHairLock::CHairLock (void)
{
   m_lPathPoint.Init (sizeof(CPoint));
   m_lPathTwist.Init (sizeof(fp));

   m_atpProfile[0].h = m_atpProfile[0].v = 1;
   DWORD i;
   for (i = 1; i < 5; i++)
      m_atpProfile[i] = m_atpProfile[0];
   m_atpProfile[4].h = m_atpProfile[4].v = 0;   // to point // BUGBUG - may not want this
   m_fDiameter = 0.01;
   m_fLengthPerPoint = 0.02;
   m_fRemainder = 1;
   m_dwHairLayers = 1;
   for (i = 0; i < MAXHAIRLAYERS; i++) {
      m_adwHairLayerRepeat[i] = 3;
      m_afHairLayerScale[i] = (fp)(MAXHAIRLAYERS-i) / (fp)MAXHAIRLAYERS;
   }
   m_dwVariationSeed = 123;
   m_fVariation = 0.1;
   m_dwDetailProf = m_dwDetailLen = 0;
   m_fDirty = TRUE;
   m_dwRenderSub= 0;
   m_lpTwistVector.Init (sizeof(CPoint));
   m_lpDirVector.Init (sizeof(CPoint));
   m_mCalcPointToClipSpace.Identity();
   m_fCalcPointToClipSpace = FALSE;
   m_fCalcCompletelyClipped = FALSE;

}

CHairLock::~CHairLock (void)
{
   // do nothing
}

static PWSTR gpszHairLock = L"HairLock";
static PWSTR gpszSplinePath = L"SplinePath";
static PWSTR gpszDiameter = L"Diameter";
static PWSTR gpszLengthPerPoint = L"LengthPerPoint";
static PWSTR gpszRemainder = L"Remainder";
static PWSTR gpszVariation = L"Variation";
static PWSTR gpszVariationSeed = L"VariationSeed";
static PWSTR gpszHairLayers = L"HairLayers";


/**********************************************************************************
CHairLock::MMLToBinaryMinMax - Gets the minimum and maximum of the hair lock
so it can efficiently store a head of hair.

inputs
   PCPoint        pLocMin - Filled with the mimum. This must already have a valid value in it
   PCPoint        pLocMax - Filled with maximum. Likewise must be valid
   fp             *pfMin - General min, for other numbers
   fp             *pfMax - General max, for other numbers
returns
   none
*/
void CHairLock::MMLToBinaryMinMax (PCPoint pLocMin, PCPoint pLocMax, fp *pfMin, fp *pfMax)
{
   DWORD i;
   PCPoint pPath = (PCPoint) m_lPathPoint.Get(0);
   fp *pafPathTwist = (fp*) m_lPathTwist.Get(0);
   for (i = 0; i < m_lPathPoint.Num(); i++) {
      pLocMin->Min (&pPath[i]);
      pLocMax->Max (&pPath[i]);

      *pfMin = min(*pfMin, pafPathTwist[i]);
      *pfMax = max(*pfMax, pafPathTwist[i]);
   } // i

   for (i = 0; i < 5; i++) {
      *pfMin = min(*pfMin, m_atpProfile[i].h);
      *pfMax = max(*pfMax, m_atpProfile[i].h);
      *pfMin = min(*pfMin, m_atpProfile[i].v);
      *pfMax = max(*pfMax, m_atpProfile[i].v);
   } // i

   *pfMin = min(*pfMin, m_fDiameter);
   *pfMax = max(*pfMax, m_fDiameter);

   *pfMin = min(*pfMin, m_fLengthPerPoint);
   *pfMax = max(*pfMax, m_fLengthPerPoint);

   *pfMin = min(*pfMin, m_fRemainder);
   *pfMax = max(*pfMax, m_fRemainder);

   *pfMin = min(*pfMin, m_fVariation);
   *pfMax = max(*pfMax, m_fVariation);

   for (i = 0; i < m_dwHairLayers; i++) {
      *pfMin = min(*pfMin, m_afHairLayerScale[i]);
      *pfMax = max(*pfMax, m_afHairLayerScale[i]);
   } // i
}


// HAIRBIN - Binary representation of a hair lock
typedef struct {
   WORD           wSize;         // size in bytes
   WORD           wNumPath;      // number of paths
   WORD           wNumHairLayers;   // number of hair layers
   short          aiProfile[5][2];  // profile information
   short          iDiameter;     // diameter info
   short          iLengthPerPoint;  // length per point
   short          iRemainder;    // remainder
   short          iVariation;    // variation score
   DWORD          dwVariationSeed;  // variation seed
   DWORD          dwUserInfo;    // user info
   // array of wNumPath HAIRPATHTWISTBIN
   // array of wNumHairLayers HAIRLAYERBIN
} HAIRBIN, *PHAIRBIN;

// HAIRPATHTWISTBIN - For storing hair path info
typedef struct {
   short          aiPath[3];     // array of hair paths
   short          iTwist;        // twist angle
} HAIRPATHTWISTBIN, *PHAIRPATHTWISTBIN;

// HAIRLAYERBIN - For storing binary hair layer
typedef struct {
   WORD           wRepeat;       // number of times repeated
   short          iScale;        // how much to scale
} HAIRLAYERBIN, *PHAIRLAYERBIN;

/**********************************************************************************
CHairLock::MMLToBinary - Converts hair to a binary form that takes less space.

inputs
   PCMem       pMem - Append at m_dwCurPosn. Append to m_dwCurPosn.
   PCPoint     pScale - Scale for location
   fp          fScale - Scale for non-location values.
   DWORD       dwUserInfo - Extra value to store for each hair
returns
   BOOL - TRUE if success
*/
BOOL CHairLock::MMLToBinary (PCMem pMem, PCPoint pScale, fp fScale, DWORD dwUserInfo)
{
   // how much need
   DWORD dwNeed = sizeof(HAIRBIN) +
      sizeof(HAIRPATHTWISTBIN) * m_lPathPoint.Num() +
      sizeof(HAIRLAYERBIN) * m_dwHairLayers;
   if (!pMem->Required (pMem->m_dwCurPosn + dwNeed))
      return FALSE;
   if (dwNeed >= 0x10000)
      return FALSE;  // shouldnt happen

   // fill in
   PHAIRBIN phb = (PHAIRBIN) ((PBYTE)pMem->p + pMem->m_dwCurPosn);
   phb->wSize = (WORD)dwNeed;
   phb->wNumPath = (WORD)m_lPathPoint.Num();
   phb->wNumHairLayers = (WORD)m_dwHairLayers;
   DWORD i, j;
   fp f;
   short s;
   for (i = 0; i < 5; i++) for (j = 0; j < 2; j++) {
      f = j ? m_atpProfile[i].v : m_atpProfile[i].h;
      f = f * fScale + 0.5;
      s = (short)floor(f);
      phb->aiProfile[i][j] = s;
   } // i
   phb->iDiameter = (short)floor(m_fDiameter * fScale + 0.5);
   phb->iLengthPerPoint = (short)floor(m_fLengthPerPoint * fScale + 0.5);
   phb->iRemainder = (short)floor(m_fRemainder * fScale + 0.5);
   phb->iVariation = (short)floor(m_fVariation * fScale + 0.5);
   phb->dwVariationSeed = m_dwVariationSeed;
   phb->dwUserInfo = dwUserInfo;

   PHAIRPATHTWISTBIN php = (PHAIRPATHTWISTBIN) (phb+1);
   PCPoint pPath = (PCPoint) m_lPathPoint.Get(0);
   fp *pafPathTwist = (fp*) m_lPathTwist.Get(0);
   for (i = 0; i < m_lPathPoint.Num(); i++, php++) {
      for (j = 0; j < 3; j++)
         php->aiPath[j] = (short)floor(pPath[i].p[j] * pScale->p[j] + 0.5);
      php->iTwist = (short)floor(pafPathTwist[i] * fScale + 0.5);
   } // i

   PHAIRLAYERBIN phl = (PHAIRLAYERBIN) php;
   for (i = 0; i < m_dwHairLayers; i++, phl++) {
      phl->wRepeat = (WORD)m_adwHairLayerRepeat[i];
      phl->iScale = (short)floor(m_afHairLayerScale[i] * fScale + 0.5);
   } // i

   // done
   pMem->m_dwCurPosn += dwNeed;
   return TRUE;
}


/**********************************************************************************
CHairLock::MMLFromBinary - Standard API

inputs
   PBYTE          pab - Data
   DWORD          dwSize - Maximum size
   PCPoint        paScale - Scaling
   fp             fScale - Scaling for non-location
   DWORD          *pdwUserInfo - Filled with user info
returns
   DWORD - Number of bytes used
*/
DWORD CHairLock::MMLFromBinary (PBYTE pab, DWORD dwSize, PCPoint pScale, fp fScale, DWORD *pdwUserInfo)
{
   // clear out
   m_fDirty = TRUE;
   m_lPathPoint.Clear();
   m_lPathTwist.Clear();

   if (dwSize < sizeof(HAIRBIN))
      return 0;   // error
   PHAIRBIN phb = (PHAIRBIN) pab;
   DWORD dwNeed = sizeof(HAIRBIN) +
      sizeof(HAIRPATHTWISTBIN) * (DWORD) phb->wNumPath +
      sizeof(HAIRLAYERBIN) * (DWORD) phb->wNumHairLayers;
   if (dwNeed != (DWORD)phb->wSize)
      return 0; // error

   DWORD i, j;
   fp f;
   for (i = 0; i < 5; i++) for (j = 0; j < 2; j++) {
      f = (fp)phb->aiProfile[i][j] * fScale;
      if (j)
         m_atpProfile[i].v = f;
      else
         m_atpProfile[i].h = f;
   } // i

   m_fDiameter = (fp)phb->iDiameter * fScale;
   m_fLengthPerPoint = (fp)phb->iLengthPerPoint * fScale;
   m_fRemainder = (fp)phb->iRemainder * fScale;
   m_fVariation = (fp)phb->iVariation * fScale;
   m_dwVariationSeed = phb->dwVariationSeed;
   m_dwHairLayers = phb->wNumHairLayers;
   if (pdwUserInfo)
      *pdwUserInfo = phb->dwUserInfo;

   PHAIRPATHTWISTBIN php = (PHAIRPATHTWISTBIN) (phb+1);
   CPoint p;
   p.Zero();
   m_lPathPoint.Required (phb->wNumPath);
   m_lPathTwist.Required (phb->wNumPath);
   for (i = 0; i < phb->wNumPath; i++, php++) {
      for (j = 0; j < 3; j++)
         p.p[j] = (fp)php->aiPath[j] * pScale->p[j];
      m_lPathPoint.Add (&p);

      f = (fp)php->iTwist * fScale;
      m_lPathTwist.Add (&f);
   } // i

   PHAIRLAYERBIN phl = (PHAIRLAYERBIN) php;
   for (i = 0; i < m_dwHairLayers; i++, phl++) {
      m_adwHairLayerRepeat[i] = phl->wRepeat;
      m_afHairLayerScale[i] = (fp)phl->iScale * fScale;
   } // i

   // done
   return phb->wSize;
}

/**********************************************************************************
CHairLock::MMLTo - Standard API
*/
PCMMLNode2 CHairLock::MMLTo (void)
{
   PCMMLNode2 pNode = new CMMLNode2;
   if (!pNode)
      return NULL;
   pNode->NameSet (gpszHairLock);

   DWORD i;
   WCHAR szTemp[32];
   PCPoint pPath = (PCPoint) m_lPathPoint.Get(0);
   fp *pafPathTwist = (fp*) m_lPathTwist.Get(0);
   for (i = 0; i < m_lPathPoint.Num(); i++) {
      swprintf (szTemp, L"Path%d", (int)i);
      MMLValueSet (pNode, szTemp, &pPath[i]);

      swprintf (szTemp, L"Twist%d", (int)i);
      MMLValueSet (pNode, szTemp, pafPathTwist[i]);
   } // i

   for (i = 0; i < 5; i++) {
      swprintf (szTemp, L"Profile%d", (int)i);
      MMLValueSet (pNode, szTemp, &m_atpProfile[i]);
   } // i

   MMLValueSet (pNode, gpszDiameter, m_fDiameter);
   MMLValueSet (pNode, gpszLengthPerPoint, m_fLengthPerPoint);
   MMLValueSet (pNode, gpszRemainder, m_fRemainder);
   MMLValueSet (pNode, gpszVariation, m_fVariation);
   MMLValueSet (pNode, gpszVariationSeed, (int)m_dwVariationSeed);
   MMLValueSet (pNode, gpszHairLayers, (int)m_dwHairLayers);
   for (i = 0; i < m_dwHairLayers; i++) {
      swprintf (szTemp, L"HairLayerRepeat%d", (int)i);
      MMLValueSet (pNode, szTemp, (int)m_adwHairLayerRepeat[i]);

      swprintf (szTemp, L"HairLayerScale%d", (int)i);
      MMLValueSet (pNode, szTemp, m_afHairLayerScale[i]);
   } // i

   return pNode;
}

/**********************************************************************************
CHairLock::MMLFrom - Standard API
*/
BOOL CHairLock::MMLFrom (PCMMLNode2 pNode)
{
   m_fDirty = TRUE;
   m_lPathPoint.Clear();
   m_lPathTwist.Clear();

   DWORD i;
   WCHAR szTemp[32];
   for (i = 0; ; i++) {
      CPoint p;
      fp f;

      swprintf (szTemp, L"Path%d", (int)i);
      if (!MMLValueGetPoint (pNode, szTemp, &p))
         break;
      m_lPathPoint.Add (&p);

      swprintf (szTemp, L"Twist%d", (int)i);
      f = MMLValueGetDouble (pNode, szTemp, 0);
      m_lPathTwist.Add (&f);
   } // i

   for (i = 0; i < 5; i++) {
      swprintf (szTemp, L"Profile%d", (int)i);
      MMLValueGetTEXTUREPOINT (pNode, szTemp, &m_atpProfile[i]);
   } // i

   m_fDiameter = MMLValueGetDouble (pNode, gpszDiameter, 1);
   m_fLengthPerPoint = MMLValueGetDouble (pNode, gpszLengthPerPoint, 1);
   m_fRemainder = MMLValueGetDouble (pNode, gpszRemainder, 1);
   m_fVariation = MMLValueGetDouble (pNode, gpszVariation, 0);
   m_dwVariationSeed = (DWORD) MMLValueGetInt (pNode, gpszVariationSeed, (int)0);
   m_dwHairLayers = (DWORD) MMLValueGetInt (pNode, gpszHairLayers, (int)1);
   for (i = 0; i < m_dwHairLayers; i++) {
      swprintf (szTemp, L"HairLayerRepeat%d", (int)i);
      m_adwHairLayerRepeat[i] = (DWORD) MMLValueGetInt (pNode, szTemp, (int)1);

      swprintf (szTemp, L"HairLayerScale%d", (int)i);
      m_afHairLayerScale[i] = MMLValueGetDouble (pNode, szTemp, 1);
   } // i

   return TRUE;
}

/**********************************************************************************
CHairLock::CloneTo - Clones this

inputs
   PCHairLock        pTo - Clone to this object
returns
   BOOL - TRUE if success
*/
BOOL CHairLock::CloneTo (CHairLock *pTo)
{
   pTo->m_lPathPoint.Init (sizeof(CPoint), m_lPathPoint.Get(0), m_lPathPoint.Num());
   pTo->m_lPathTwist.Init (sizeof(fp), m_lPathTwist.Get(0), m_lPathTwist.Num());
   memcpy (pTo->m_atpProfile, m_atpProfile, sizeof(m_atpProfile));
   pTo->m_fDiameter = m_fDiameter;
   pTo->m_fLengthPerPoint = m_fLengthPerPoint;
   pTo->m_fRemainder = m_fRemainder;
   pTo->m_dwHairLayers = m_dwHairLayers;
   memcpy (pTo->m_adwHairLayerRepeat, m_adwHairLayerRepeat, sizeof(m_adwHairLayerRepeat));
   memcpy (pTo->m_afHairLayerScale, m_afHairLayerScale, sizeof(m_afHairLayerScale));
   pTo->m_dwVariationSeed = m_dwVariationSeed;
   pTo->m_fVariation = m_fVariation;
   pTo->m_mCalcPointToClipSpace.Copy (&m_mCalcPointToClipSpace);
   pTo->m_fCalcPointToClipSpace = m_fCalcPointToClipSpace;
   pTo->m_fCalcCompletelyClipped = m_fCalcCompletelyClipped;

   pTo->m_fDirty = TRUE;
   // NOTE: Not cloning calculated information

   return TRUE;
}

/**********************************************************************************
CHairLock::Clone - Standard API
*/
CHairLock *CHairLock::Clone (void)
{
   PCHairLock pNew = new CHairLock;
   if (!pNew)
      return NULL;

   if (!CloneTo (pNew)) {
      delete pNew;
      return NULL;
   }

   return pNew;
}


/**********************************************************************************
CHairLock::SplineSet - Sets the spline for the hair.

NOTE: This automatically adjusts the length of the spline nodes to comply with
the m_fLengthPerPoint setting.

inputs
   DWORD          dwNum - Number of points in the spline. THis must be at least 3 points. Point
                     0 is below the scalp, controlling the direction that the hair
                     comes out, 1 is right at the surface, and 2 is above the surface.
   PCPoint        paPoint - Pointer to an array of dwNum points. These will be rearranged
                     so that they are all m_fLengthPerPoint away from one another
   fp             *pafTwist - Pointer to anarray of dwNum fp. This is the angle
                     (in radians) to twist the hair.
returns
   BOOL - TRUE if success.
*/
BOOL CHairLock::SplineSet (DWORD dwNum, PCPoint paPoint, fp *pafTwist)
{
   if (dwNum < 3)
      return FALSE;

   m_lPathPoint.Init (sizeof(CPoint), paPoint, dwNum);
   m_lPathTwist.Init (sizeof(fp), pafTwist, dwNum);
   m_fDirty = TRUE;

   // adjust the lengths
   FixLengths ();
   return TRUE;
}


/**********************************************************************************
CHairLock::SplineGet - Get the information about the hair. NOTE: Do NOT change the
values.

inputs
   DWORD          *pdwNum - Filled with the number of elements
   fp             *ppafTwist - Filled with a pointer that will be filled with
                     a pointer to an array of fp for the twist
returns
   PCPoint - Returns a pointer to an array of spline locations
*/
PCPoint CHairLock::SplineGet (DWORD *pdwNum, fp **ppafTwist)
{
   if (pdwNum)
      *pdwNum = m_lPathPoint.Num();
   if (ppafTwist)
      *ppafTwist = (fp*)m_lPathTwist.Get(0);

   return (PCPoint)m_lPathPoint.Get(0);
}



/**********************************************************************************
CHairLock::FixLengths - An internal function that automatically adjusts all the
lengths between the spline points so that they're the correct distance away from
one another.

Call this after m_fLengthPerPoint has been changed

inputs
   PCPoint           pap - Pointer to an array of m_lPathPoint.Num() points.
                           If this is NULL then use m_lPathPoint.Get(0)
*/
void CHairLock::FixLengths (PCPoint pap)
{
   DWORD dwNum = m_lPathPoint.Num();
   if (dwNum < 2)
      return;
   if (!pap) {
      pap = (PCPoint)m_lPathPoint.Get(0);
      m_fDirty = TRUE;
   }

   // do the root
   pap[0].Subtract (&pap[1]);
   pap[0].Normalize();
   pap[0].Scale (m_fLengthPerPoint);
   pap[0].Add (&pap[1]);

   // pass one, convert everythign to deltas...
   DWORD i;
   for (i = dwNum-1; i >= 2; i--) {
      pap[i].Subtract (&pap[i-1]);
      pap[i].Normalize();
      pap[i].Scale (m_fLengthPerPoint);
   }

   // pass two, add together
   for (i = 2; i < dwNum; i++)
      pap[i].Add (&pap[i-1]);

   // done
}


/**********************************************************************************
CHairLock::ProfileSet - Sets the profile (x and y length of the ellipse) for the hair
along it's length.

inputs 
   PTEXTUREPOINT        patpProfile - Points to an array of 5 scales. Each scale
                        should have values between 0..1 for h and v, but usually 1 for
                        each to indicate a round lock of hair. [0] is the scale at
                        the root, [1] at the root+1, [2] in the cetner of the hair,
                        [3] for end-1, [4] for scale at end of lock
returns
   BOOL - TRUE if success
*/
BOOL CHairLock::ProfileSet (PTEXTUREPOINT patpProfile)
{
   memcpy (m_atpProfile, patpProfile, sizeof(m_atpProfile));
   m_fDirty = TRUE;
   return TRUE;
}

/**********************************************************************************
CHairLock::ProfileGet - Returns a pointer to the profile (see profile set) for the
hair strand.

returns
   PTEXTUREPOINT - Pointer to an array of 5 texturepoint. Do NOT change
*/
PTEXTUREPOINT CHairLock::ProfileGet (void)
{
   return m_atpProfile;
}

/**********************************************************************************
CHairLock::HairLayersSet - Sets the number of hair layers (so can do something
like fuzzy bunny), how often their texture repeats, and the scale of the hair
layer compared to the over-all scale of the strand.

inputs
   DWORD          dwHairLayers - Number of hair layers to set. from 1 to MAXHAIRLAYERS
   DWORD          *padwHairLayerRepeat - Array of dwHairLayer DWORD's to indicate the
                     number of times the layer's texture will be repeated around the
                     hair's profile
   fp             *pafHairLayerScale - Scale of the hair layer relative to the basic size
                     of the hair lock. Layer[0] should be 1 (normally), with lower values following

returns
   BOOL - TRUE if successs
*/
BOOL CHairLock::HairLayersSet (DWORD dwHairLayers, DWORD *padwHairLayerRepeat, fp *pafHairLayerScale)
{
   if ((dwHairLayers < 1) || (dwHairLayers > MAXHAIRLAYERS))
      return FALSE;

   m_fDirty = TRUE;
   m_dwHairLayers = dwHairLayers;
   DWORD i;
   for (i = 0; i < m_dwHairLayers; i++) {
      m_adwHairLayerRepeat[i] = padwHairLayerRepeat[i];
      m_afHairLayerScale[i] = pafHairLayerScale[i];
   }

   return TRUE;
}


/**********************************************************************************
CHairLock::HairLayersGet - Gets information about the hair layers. See HairLayersSet

inputs
   DWORD          *ppadwHairLayerRepeat - Filled with a pointer to an array of
                     DWORDs indicating the number of times it repeat. Do NOT change.
   fp             **ppafHairlayerScale - Likewise, for hair scaling
returns
   DWORD - Number of hair layers
*/
DWORD CHairLock::HairLayersGet (DWORD **ppadwHairLayerRepeat, fp **ppafHairlayerScale)
{
   if (ppadwHairLayerRepeat)
      *ppadwHairLayerRepeat = m_adwHairLayerRepeat;
   if (ppafHairlayerScale)
      *ppafHairlayerScale = m_afHairLayerScale;

   return m_dwHairLayers;
}


/**********************************************************************************
CHairLock::DiameterSet - Sets the diameter of the hair strands. (Note: The diameter
is modified by the hair's profile.

inputs
   fp          fDiameter - Length in meters
returns
   BOOL - TRUE if success
*/
BOOL CHairLock::DiameterSet (fp fDiameter)
{
   m_fDiameter = fDiameter;
   m_fDirty = TRUE;
   return TRUE;
}


/**********************************************************************************
CHairLock::DiameterGet - Returns the hairs diamter
*/
fp CHairLock::DiameterGet (void)
{
   return m_fDiameter;
}

/**********************************************************************************
CHairLock::VariationSet - Sets the amount of variation (in the profile) along the
length of the hair. More variation will cause more messy hair.

inputs
   fp          fVariation - Variation amount from 0 (none) to 1 (maximum)
   DWORD       dwSeed - Random seed for the variation
*/
BOOL CHairLock::VariationSet (fp fVariation, DWORD dwSeed)
{
   m_fDirty = TRUE;
   m_fVariation = fVariation;
   m_dwVariationSeed = dwSeed;
   return TRUE;
}


/**********************************************************************************
CHairLock::VariationGet - Returns the information passed in by VariationSet

inputs
   DWORD          *pdwSeed - Filled in with the variation seed
returns
   fp - Variation
*/
fp CHairLock::VariationGet (DWORD *pdwSeed)
{
   if (pdwSeed)
      *pdwSeed = m_dwVariationSeed;

   return m_fVariation;
}


/**********************************************************************************
CHairLock::LengthSet - Sets the length of each segment. This automatically resizes
the hair lock

inputs
   fp          fLength - Length in meters
returns
   BOOL - TRUE if success
*/
BOOL CHairLock::LengthSet (fp fLength)
{
   if (fLength < CLOSE)
      return FALSE;
   if (fLength == m_fLengthPerPoint)
      return TRUE;

   m_fLengthPerPoint = fLength;
   m_fDirty = TRUE;
   FixLengths();

   return TRUE;
}

/**********************************************************************************
CHairLock::LengthGet - returns the length per point
*/
fp CHairLock::LengthGet (void)
{
   return m_fLengthPerPoint;
}


/**********************************************************************************
CHairLock::RemainderSet - Sets the Remainder of each segment, which is the length
of the last segment, between 0 and 1. If 1 then runs all the way to the last segment.
If 0 finishes at 2nd to last segment.

inputs
   fp          fRemainder - Remainder in meters
returns
   BOOL - TRUE if success
*/
BOOL CHairLock::RemainderSet (fp fRemainder)
{
   fRemainder = max(fRemainder , 0);
   fRemainder = min(fRemainder, 1);
   if (fRemainder == m_fRemainder)
      return TRUE;

   m_fRemainder = fRemainder;
   m_fDirty = TRUE;

   return TRUE;
}

/**********************************************************************************
CHairLock::RemainderGet - returns the Remainder per point
*/
fp CHairLock::RemainderGet (void)
{
   return m_fRemainder;
}





/**********************************************************************************
CHairLock::TotalLengthSet - Sets the total length of the lock. This will end
up adding or removing hair segments as necessary.

inputs
   fp          fLength - Length in meters
   CHairLock   *pTemplate - Template hairlock. If the length of this hairlock ends
                  up being enlarged, the template hairlock will be used for the
                  new directions. This can be null
returns
   BOOL - TRUE if success
*/
BOOL CHairLock::TotalLengthSet (fp fLength, CHairLock *pTemplate)
{
   // figure out how many splint points need
   DWORD dwNeed = ceil(fLength / m_fLengthPerPoint + 2.0);
   DWORD dwNum = m_lPathPoint.Num();
   DWORD i;
   if (dwNum < 2)
      return FALSE;  // cant modify

   // set dirty
   m_fDirty = TRUE;

   if (dwNeed > dwNum) {
      // if there aren't enough points then add
      fp fTwist;;
      CPoint pDelta, pLoc;
      PCPoint pPath = (PCPoint) m_lPathPoint.Get(0);
      pLoc.Copy (&pPath[dwNum-1]);

      // BUGFIX - if template use that
      PCPoint pTempPoint = NULL;
      fp *pfTempTwist = NULL;
      DWORD dwTempNum = 0;
      if (pTemplate)
         pTempPoint = pTemplate->SplineGet (&dwTempNum, &pfTempTwist);

      m_lPathPoint.Required (dwNeed);
      m_lPathTwist.Required (dwNeed);
      for (i = dwNum; i < dwNeed; i++) {
         // if template use that
         if (pTempPoint && (i < dwTempNum)) {
            pDelta.Subtract (&pTempPoint[i], &pTempPoint[i-1]);
            fTwist = pfTempTwist[i];
         }
         else {
            pPath = (PCPoint) m_lPathPoint.Get(0);
            fp *pafTwist = (fp*) m_lPathTwist.Get(0);

            fTwist = pafTwist[dwNum-1];
            pDelta.Subtract (&pPath[dwNum-1], &pPath[dwNum-2]);
         }
         pDelta.Normalize();
         pDelta.Scale (m_fLengthPerPoint);

         pLoc.Add (&pDelta);
         m_lPathPoint.Add (&pLoc);
         m_lPathTwist.Add (&fTwist);
      }
   }
   else {
      // if there are too many points remove
      while (dwNeed < m_lPathPoint.Num()) {
         m_lPathPoint.Remove (m_lPathPoint.Num()-1);
         m_lPathTwist.Remove (m_lPathTwist.Num()-1);
      }
   }

   // figure out the total length now
   fp fTotal = m_fLengthPerPoint * (fp)(dwNeed-2);

   // and remainder
   m_fRemainder = 1.0 - (fTotal - fLength) / m_fLengthPerPoint;

   return TRUE;
}

/**********************************************************************************
CHairLock::TotalLengthGet - returns the total length of the hair
*/
fp CHairLock::TotalLengthGet (void)
{
   return ((fp)m_lPathPoint.Num() + m_fRemainder - 3) * m_fLengthPerPoint;
}




/**********************************************************************************
CHairLock::CalcRenderIfNecessary - Recalculates if the dirty flag is set (or if
the scale is different than what want)

inputs
   PCOBJECTRENDER       pr - Render info
   PCMatrix             pmPointToClipSpace - Converts from a point in the hair lock's coordinates
                        into a rotated and translated space where hair with z >= 0.0 is clipped due
                        to a hat. If this is NULL then no clipping
   PCMatrix             pmClipSpaceToPoint - Inverse of the matrix converting back. If pmPointToClipSpace
                        is null, this one should be NULL.
*/
BOOL CHairLock::CalcRenderIfNecessary (POBJECTRENDER pr, PCMatrix pmPointToClipSpace, PCMatrix pmClipSpaceToPoint)
{
   // automaticaly set to dirty if matricies dont match
   if (!m_fDirty) {
      if (pmPointToClipSpace && !m_fCalcPointToClipSpace)
         m_fDirty = TRUE;
      else if (!pmPointToClipSpace && m_fCalcPointToClipSpace)
         m_fDirty = TRUE;
      else if (pmPointToClipSpace && m_fCalcPointToClipSpace)
         m_fDirty = !pmPointToClipSpace->AreClose (&m_mCalcPointToClipSpace);
   }

   // figure out what detail want
   DWORD dwWantProf, dwWantLen;
   if (pr && (pr->dwReason != ORREASON_BOUNDINGBOX)) {
      fp fDetail = pr->pRS->QueryDetail ();
      // BUGFIX - dont allow for dwWantProf = 2 because too much detail and too slow to draw
      //if (fDetail < m_fDiameter / 2)
      //   dwWantProf = 2;
      //else
      if (fDetail < m_fDiameter)
         dwWantProf = 1;
      else
         dwWantProf = 0;

      if (fDetail < m_fLengthPerPoint / 2)
         dwWantLen = 2;
      else if (fDetail < m_fLengthPerPoint)
         dwWantLen = 1;
      else
         dwWantLen = 0;

      if ((dwWantProf != m_dwDetailProf) || (dwWantLen != m_dwDetailLen))
         m_fDirty = TRUE;
   }
   else {
      dwWantProf = m_dwDetailProf;
      dwWantLen = m_dwDetailLen;
   }


   // if not dirty done
   if (!m_fDirty)
      return TRUE;

   m_fCalcCompletelyClipped = FALSE;
   m_dwDetailProf = dwWantProf;
   m_dwDetailLen = dwWantLen;

   // smooth out the spline along the path
   DWORD dwSkipLen = (1 << m_dwDetailLen);
   DWORD i, j, k;
   CListFixed lPathPoint, lPathTwist;
   lPathPoint.Init (sizeof(CPoint));
   lPathTwist.Init (sizeof(fp));
   PCPoint pPoint = (PCPoint) m_lPathPoint.Get(0);
   fp *pafTwist = (fp*) m_lPathTwist.Get(0);
   DWORD dwNum = m_lPathPoint.Num();
   fp f;
   DWORD dwWillHave = (dwNum-1)*dwSkipLen + 1;  // number of points that will have when done
   DWORD dwNumClipped = 0;
   CPoint pClip;
   fp fRadius = m_fDiameter / 2.0;
   lPathPoint.Required (dwWillHave);
   lPathTwist.Required (dwWillHave);
   for (i = 0; i < dwWillHave; i++) {
      // figure out location along length of spline
      fp fLoc;
      if (i < dwSkipLen) {
         // occurs before root, so dont do any scaling
         fLoc = (fp)i / (fp)dwSkipLen;
      }
      else {
         // occurs after root
         fLoc = (fp)i - (fp)dwSkipLen; // so 0 at root
         fLoc /= (fp)(dwWillHave - dwSkipLen - 1); // so 0 is at root, 1 at end
         fLoc *= ((fp)m_lPathPoint.Num() + m_fRemainder - 3) * m_fLengthPerPoint;   // so from 0m to total m
         fLoc /= ((fp)m_lPathPoint.Num() - 2) * m_fLengthPerPoint;   // so from take into account remainder
         fLoc *= (fp)(dwNum - 2); // range from 0 to dwNum-2
         fLoc += 1;  // range from 1 to dwNum-1
      }

      // determine index into existing lit
      DWORD adw[4];
      adw[1] = (DWORD)fLoc;
      adw[1] = min(adw[1], dwNum-1);
      fLoc -= (fp)adw[1];

      // pre-post values
      adw[0] = (adw[1] ? (adw[1]-1) : 0);
      adw[2] = min(adw[1]+1, dwNum-1);
      adw[3] = min(adw[1]+2, dwNum-1);

      // add the point
      CPoint p;
      for (k = 0; k < 3; k++)
         p.p[k] = HermiteCubic (fLoc, pPoint[adw[0]].p[k], pPoint[adw[1]].p[k], pPoint[adw[2]].p[k], pPoint[adw[3]].p[k]);
      p.p[3] = 1;
      
      // clip this?
      if (/*m_fCalcPointToClipSpace && */ pmPointToClipSpace) {  // BUGFIX - Include pmPointToClipSpace
            // BUGFIX Had m_fCalcPointToClipSpace && pmPointToClipSpace, but busts hair so take out m_fCalcPointToClipSpace
         pClip.Copy (&p);
         pClip.MultiplyLeft (pmPointToClipSpace);
         if (pClip.p[2] >= fRadius) {
            // clipped
            dwNumClipped++;
            pClip.p[2] = fRadius;
            pClip.p[3] = 1; // just in case
            pClip.MultiplyLeft (pmClipSpaceToPoint);
            pClip.p[3] = 1; // just in case
            p.Copy (&pClip);
         }
      }



      // finally, add
      lPathPoint.Add (&p);

      // twist
      f = HermiteCubic (fLoc, pafTwist[adw[0]], pafTwist[adw[1]], pafTwist[adw[2]], pafTwist[adw[3]]);
      f /= (fp)dwSkipLen;
      lPathTwist.Add (&f);
   } // i
   if (dwNumClipped >= dwWillHave)
      m_fCalcCompletelyClipped = TRUE;


#if 0 // dead code
   for (i = 0; i < dwNum; i++) {
      // add this point to list
      lPathPoint.Add (&pPoint[i]);
      f = pafTwist[i] / (fp)dwSkipLen;
      lPathTwist.Add (&f);

      // add interps
      if (i+1 < dwNum) for (j = 1; j < dwSkipLen; j++) {
         CPoint p;
         DWORD adw[4];
         f = (fp)j / (fp)dwSkipLen;
         adw[0] = i ? (i-1) : 0;
         adw[1] = i;
         adw[2] = min(i+1, dwNum-1);
         adw[3] = min(i+2, dwNum-1);

         for (k = 0; k < 3; k++)
            p.p[k] = HermiteCubic (f, pPoint[adw[0]].p[k], pPoint[adw[1]].p[k], pPoint[adw[2]].p[k], pPoint[adw[3]].p[k]);
         p.p[3] = 1;
         lPathPoint.Add (&p);

         // twist
         f = HermiteCubic (f, pafTwist[adw[0]], pafTwist[adw[1]], pafTwist[adw[2]], pafTwist[adw[3]]);
         f /= (fp)dwSkipLen;
         lPathTwist.Add (&f);
      } // j
   } // i
#endif // 0 - dead code
   pPoint = (PCPoint) lPathPoint.Get(0);
   pafTwist = (fp*) lPathTwist.Get(0);
   dwNum = lPathPoint.Num();

   // go through and determine directional vectors...
   CListFixed lDir;
   PCPoint pDir;
   lDir.Init (sizeof(CPoint), pPoint, dwNum);
   pDir = (PCPoint)lDir.Get(0);
   for (i = 0; i < dwNum; i++) {
      pDir[i].Subtract (&pPoint[min(i+1,dwNum-1)], &pPoint[i ? (i-1) : 0]);
      f = pDir[i].Length();
      if (f < CLOSE) {
         pDir[i].Zero();
         pDir[i].p[0] = 1; // just pick a direction
      }
      else
         pDir[i].Scale (1.0 / f);   // normalize
   } // i

   // determine up and right vectors
   CListFixed lUp, lRight;
   PCPoint pUp, pRight;
   lUp.Init (sizeof(CPoint), pPoint, dwNum);
   lRight.Init (sizeof(CPoint), pPoint, dwNum);
   pUp = (PCPoint)lUp.Get(0);
   pRight = (PCPoint)lRight.Get(0);
   for (i = 0; i < dwNum; i++) {
      DWORD dwBackOff = 0;
      if (!i) {
         // pick an up vector
         pUp[i].Zero();
         pUp[i].p[dwBackOff] = 1;
         dwBackOff++;
      }
      else
         pUp[i].Copy (&pUp[i-1]); // take previous up

      // create a right vector from this
      while (TRUE) {
         pRight[i].CrossProd (&pDir[i], &pUp[i]);
         f = pRight[i].Length();
         if ((f > 0.01) || (dwBackOff >= 3))
            break;

         // that didn't work, so try another
         pUp[i].Zero();
         pUp[i].p[dwBackOff] = 1;
         dwBackOff++;
      }
      pRight[i].Normalize();

      // recalc up
      pUp[i].CrossProd (&pRight[i], &pDir[i]);

      // rotate?
      if (!pafTwist[i])
         continue;
      CPoint pOldUp, pOldRight;
      fp fCos = cos(pafTwist[i]);
      fp fSin = sin(pafTwist[i]);
      pOldUp.Copy (&pUp[i]);
      pOldRight.Copy (&pRight[i]);
      for (j = 0; j < 3; j++)
         pUp[i].p[j] = fCos * pOldUp.p[j] + fSin * pOldRight.p[j];
      pRight[i].CrossProd (&pDir[i], &pUp[i]);  // to reconstruct. dont need to normalize
   } // i

   // fill in the twist and direction vectors
   m_lpTwistVector.Init (sizeof(CPoint), pUp, m_lPathPoint.Num());
   m_lpDirVector.Init (sizeof(CPoint), pUp, m_lPathPoint.Num());
   PCPoint pTempTwist = (PCPoint)m_lpTwistVector.Get(0);
   PCPoint pTempDir = (PCPoint)m_lpDirVector.Get(0);
   for (i = 0; i < m_lPathPoint.Num(); i++) {
      pTempTwist[i].Copy (&pUp[i*dwSkipLen]);
      pTempDir[i].Copy (&pDir[i*dwSkipLen]);
   }

   // determine how many points around... and precalculate the sin and cos for these
   DWORD dwAround = (4 << m_dwDetailProf);
   CMem memAround;
   if (!memAround.Required (dwAround * sizeof(CPoint)))
      return FALSE;
   PCPoint pAround = (PCPoint)memAround.p;
   for (i = 0; i < dwAround; i++) {
      f = (fp)i / (fp)dwAround * 2.0 * PI;
      pAround[i].p[0] = cos(f);  // going clockwise around
      pAround[i].p[1] = sin(f);
   }

   // come up with scale points (based on original) for these
   CListFixed lScaleOrig;
   lScaleOrig.Init (sizeof(TEXTUREPOINT));
   DWORD dwOldNum = m_lPathPoint.Num();
   lScaleOrig.Required (dwOldNum);
   for (i = 0; i < dwOldNum; i++) {
      // if at beginning know
      if (i <= 1) {
         lScaleOrig.Add (&m_atpProfile[0]);
         continue;
      }

      // if at end know
      if (i+1 >= dwOldNum) {
         lScaleOrig.Add (&m_atpProfile[4]);
         continue;
      }

      // if 2nd from start
      if (i == 2) {
         lScaleOrig.Add (&m_atpProfile[1]);
         continue;
      }

      // if 2nd from end
      if (i+2 == dwOldNum) {
         lScaleOrig.Add (&m_atpProfile[3]);
         continue;
      }

      // else, calculate alpha from start to end
      fp fAlpha = (fp)(i-2) / (fp)(dwOldNum-4);
      DWORD dwLeft, dwRight;
      if (fAlpha < 0.5) {
         // first half of interp
         dwLeft = 1;
         dwRight = 2;
         fAlpha *= 2;
      }
      else {
         // second half of interp
         dwLeft = 2;
         dwRight = 3;
         fAlpha = (fAlpha - 0.5) * 2;
      }

      // interp scale
      TEXTUREPOINT tp;
      tp.h = (1.0 - fAlpha) * m_atpProfile[dwLeft].h + fAlpha * m_atpProfile[dwRight].h;
      tp.v = (1.0 - fAlpha) * m_atpProfile[dwLeft].v + fAlpha * m_atpProfile[dwRight].v;
      lScaleOrig.Add (&tp);
   }

   // go through and randomize the scale
   PTEXTUREPOINT ptpScaleOrig = (PTEXTUREPOINT)lScaleOrig.Get(0);
   if (m_fVariation) {
      srand (m_dwVariationSeed);
      for (i = 0; i < lScaleOrig.Num(); i++) {
         f = randf (-m_fVariation, m_fVariation);
         if (f >= 0)
            f = 1.0 + f;
         else
            f = 1.0 / (1.0 - f);

         ptpScaleOrig[i].h *= f;
         ptpScaleOrig[i].v /= f; // so area stays the same
      } // i
   }

   // now figure out the scale on a per-subdivide
   CListFixed lScale;
   lScale.Init (sizeof(TEXTUREPOINT));
   lScale.Required (lScaleOrig.Num() * dwSkipLen);
   for (i = 0; i < lScaleOrig.Num(); i++) {
      // add this point to list
      lScale.Add (&ptpScaleOrig[i]);

      // add interps
      if (i+1 < lScaleOrig.Num()) for (j = 1; j < dwSkipLen; j++) {
         TEXTUREPOINT tp;
         DWORD adw[4];
         f = (fp)j / (fp)dwSkipLen;
         adw[0] = i ? (i-1) : 0;
         adw[1] = i;
         adw[2] = min(i+1, lScaleOrig.Num()-1);
         adw[3] = min(i+2, lScaleOrig.Num()-1);

         tp.h = HermiteCubic (f, ptpScaleOrig[adw[0]].h, ptpScaleOrig[adw[1]].h, ptpScaleOrig[adw[2]].h, ptpScaleOrig[adw[3]].h);
         tp.v = HermiteCubic (f, ptpScaleOrig[adw[0]].v, ptpScaleOrig[adw[1]].v, ptpScaleOrig[adw[2]].v, ptpScaleOrig[adw[3]].v);
         lScale.Add (&tp);
      } // j
   } // i
   PTEXTUREPOINT patpScale = (PTEXTUREPOINT) lScale.Get(0);

   // contruct a mesh of points - x=dwAround, y=dwNum
   CMem memPoint, memNorm;
   if (!memPoint.Required (dwNum * dwAround * sizeof(CPoint)))
      return FALSE;
   if (!memNorm.Required (dwNum * dwAround * sizeof(CPoint)))
      return FALSE;
   PCPoint paNormSurf = (PCPoint)memNorm.p;
   PCPoint paPointSurf = (PCPoint)memPoint.p;
   DWORD x,y;
   CPoint pA, pB;
   for (y = 0; y < dwNum; y++) for (x = 0; x < dwAround; x++) {
      pA.Copy (&pUp[y]);
      pA.Scale (patpScale[y].v * pAround[x].p[0] * m_fDiameter/2);
      pB.Copy (&pRight[y]);
      pB.Scale (patpScale[y].h * pAround[x].p[1] * m_fDiameter/2);
      paPointSurf[x + y*dwAround].Add (&pPoint[y], &pA);
      paPointSurf[x + y*dwAround].Add (&pB);

      // clip this? BUGFIX - To make absolutely sure doesnt appear above hat
      if (/*m_fCalcPointToClipSpace &&*/ pmPointToClipSpace) {  // BUGFIX - Make sure have pointer
            // BUGFIX Had m_fCalcPointToClipSpace && pmPointToClipSpace, but busts hair so take out m_fCalcPointToClipSpace
         pClip.Copy (&paPointSurf[x + y*dwAround]);
         pClip.MultiplyLeft (pmPointToClipSpace);
         if (pClip.p[2] >= 0.0) {
            // clipped
            dwNumClipped++;
            pClip.p[2] = 0;
            pClip.p[3] = 1; // just in case
            pClip.MultiplyLeft (pmClipSpaceToPoint);
            pClip.p[3] = 1; // just in case
            paPointSurf[x + y*dwAround].Copy (&pClip);
         }
      }
   } // x,y

   // determine all the normals
   for (y = 0; y < dwNum; y++) for (x = 0; x < dwAround; x++) {
      // figure out vectors...
      fp fa, fb;
      pA.Subtract (&paPointSurf[x + min(y+1,dwNum-1)*dwAround],
         &paPointSurf[x + (y ? (y-1) : 0)*dwAround]);
      pB.Subtract (&paPointSurf[(x+1)%dwAround + y*dwAround],
         &paPointSurf[(x+dwAround-1)%dwAround + y * dwAround]);

      // figure out lengths
      fa = pA.Length();
      fb = pB.Length();


      // if either is too short then cant calculated valid normal so make one up
      if ((fa < CLOSE) || (fb < CLOSE)) {
         // hack in a normal... going directly out, and not actually normalized
         pA.Copy (&pUp[y]);
         pA.Scale (pAround[x].p[0]);
         pB.Copy (&pRight[y]);
         pB.Scale (pAround[x].p[1]);
         paNormSurf[x + y*dwAround].Add (&pA, &pB);
      }
      else {
         //pA.Scale (1.0 / fa);
         //pB.Scale (1.0 / fb);
         paNormSurf[x + y*dwAround].CrossProd (&pB, &pA);
      }

      // normalize
      paNormSurf[x + y*dwAround].Normalize();
   } // xy

   // make up the textures...
   CMem memText;
   if (!memText.Required (dwNum * (dwAround+1) * sizeof(TEXTPOINT5)))
      return FALSE;
   PTEXTPOINT5 paTextSurf = (PTEXTPOINT5)memText.p;
   for (y = 0; y < dwNum; y++) for (x = 0; x < dwAround+1; x++) {
      PCPoint pp = &paPointSurf[(x%dwAround) + y*dwAround];
      PTEXTPOINT5 ptp = &paTextSurf[x + y*(dwAround+1)];

      ptp->hv[0] = (fp) x / (fp)dwAround; // NOTE: No scaling by sub-strands because do that later
      ptp->hv[1] = -(fp) y * m_fLengthPerPoint / (fp)dwSkipLen;

      ptp->xyz[0] = pp->p[0];
      ptp->xyz[1] = pp->p[1];
      ptp->xyz[2] = pp->p[2];
   } // xy


   // figure out how many sub-blocks should split into so speed up ray tracing
   fp fSplit = (fp) m_lPathPoint.Num() * m_fLengthPerPoint / m_fDiameter / 4;
   m_dwRenderSub = (DWORD)fSplit;
   m_dwRenderSub = min(m_dwRenderSub, MAXRENDERSUB);
   m_dwRenderSub = min(m_dwRenderSub, m_lPathPoint.Num()/3);   // always at least some grouped
   m_dwRenderSub = max(m_dwRenderSub, 1);

   // figure out the starting index, since don't want to spend much time on
   // drawing the roots, because hidden
   DWORD dwRootStart = dwSkipLen - 1;

   // how many spline nodes per loop
   DWORD dwNodesPerLoop = (dwNum - dwRootStart) / m_dwRenderSub + 2;
      // NOTE: For last one may have fewer points because of roundoff error
      // Add extra one so that start node = end node of previous

   // loop over all sub-sections
   for (i = 0; i < m_dwRenderSub; i++, dwRootStart += dwNodesPerLoop - 1) {
      dwNodesPerLoop = min(dwNodesPerLoop, dwNum - dwRootStart);
      if (dwNodesPerLoop < 2) {
         // shouldnt happen, but just in case
         m_amemRenderPoint[i].m_dwCurPosn = 0;
         m_amemRenderNorm[i].m_dwCurPosn = 0;
         m_amemRenderText[i].m_dwCurPosn = 0;
         m_amemRenderVert[i].m_dwCurPosn = 0;
         m_amemRenderPoly[i].m_dwCurPosn = 0;
         continue;
      }

      // allocate enough for the points
      DWORD dwNeed = dwNodesPerLoop * dwAround * m_dwHairLayers * sizeof(CPoint);
      if (!m_amemRenderPoint[i].Required (dwNeed))
         return FALSE;
      m_amemRenderPoint[i].m_dwCurPosn = dwNeed;
      PCPoint paAlloc = (PCPoint)m_amemRenderPoint[i].p;

      // fill in the locations for the hair
      for (j = 0; j < m_dwHairLayers; j++)
         for (y = 0; y < dwNodesPerLoop; y++) for (x = 0; x < dwAround; x++)
            paAlloc[x + y*dwAround + j*dwAround*dwNodesPerLoop].Average (
               &paPointSurf[x + (y+dwRootStart)*dwAround],
               &pPoint[y+dwRootStart],
               m_afHairLayerScale[j]);

      // just copy over the normals since can use the same normals for all layers
      // of hair. This is a resonable assumption although not 100% accurate
      dwNeed = dwNodesPerLoop * dwAround * sizeof(CPoint);
      if (!m_amemRenderNorm[i].Required (dwNeed))
         return FALSE;
      m_amemRenderNorm[i].m_dwCurPosn = dwNeed;
      memcpy (m_amemRenderNorm[i].p, paNormSurf + (dwRootStart * dwAround), dwNeed);

      // just copy over for the textures, assuming that the textures remain the
      // same for all the layers. This is a good approx, although the volumetric
      // textures shoul really be different. I don't expect multiple layers of
      // volumetric textures though
      dwNeed = dwNodesPerLoop * (dwAround+1) * sizeof(TEXTPOINT5);
      if (!m_amemRenderText[i].Required (dwNeed))
         return FALSE;
      m_amemRenderText[i].m_dwCurPosn = dwNeed;
      memcpy (m_amemRenderText[i].p, paTextSurf + (dwRootStart * (dwAround+1)), dwNeed);

      // allocate for all the vertex definitions
      dwNeed = dwNodesPerLoop * (dwAround+1) * m_dwHairLayers * sizeof(VERTEX);
      if (!m_amemRenderVert[i].Required (dwNeed))
         return FALSE;
      m_amemRenderVert[i].m_dwCurPosn = dwNeed;
      PVERTEX pVert = (PVERTEX) m_amemRenderVert[i].p;
      for (j = 0; j < m_dwHairLayers; j++)
         for (y = 0; y < dwNodesPerLoop; y++) for (x = 0; x < dwAround+1; x++) {
            PVERTEX pv = &pVert[x + y*(dwAround+1) + j*(dwAround+1)*dwNodesPerLoop];
            pv->dwColor = j;
            pv->dwNormal = (x % dwAround) + y*dwAround;
            pv->dwPoint = (x % dwAround) + y*dwAround + j*dwAround*dwNodesPerLoop;
            pv->dwTexture = x + y*(dwAround+1) + j*(dwAround+1)*dwNodesPerLoop;
         }

      // allocate memroy for the polygons. Each polygon is a WORD for the hair layer,
      // followed by 4 WORDs for the polygon points
      dwNeed = (dwNodesPerLoop-1) * dwAround * m_dwHairLayers * 5 * sizeof(WORD);
      if (!m_amemRenderPoly[i].Required (dwNeed))
         return FALSE;
      m_amemRenderPoly[i].m_dwCurPosn = dwNeed;
      WORD *paw = (WORD*) m_amemRenderPoly[i].p;
      for (j = 0; j < m_dwHairLayers; j++)
         for (y = 0; y+1 < dwNodesPerLoop; y++) for (x = 0; x < dwAround; x++, paw+=5) {
            paw[0] = (WORD)j;

            paw[1] = (WORD)(x + y*(dwAround+1) + j*(dwAround+1)*dwNodesPerLoop);
            paw[2] = (WORD)(x + (y+1)*(dwAround+1) + j*(dwAround+1)*dwNodesPerLoop);
            paw[3] = (WORD)((x+1) + (y+1)*(dwAround+1) + j*(dwAround+1)*dwNodesPerLoop);
            paw[4] = (WORD)((x+1) + y*(dwAround+1) + j*(dwAround+1)*dwNodesPerLoop);

            // look for points that are the same (at end of hair) and combine them
            // so no duplicates
            DWORD k;
            DWORD dwPoints = 4;
            paAlloc = (PCPoint)m_amemRenderPoint[i].p;
            for (k = 4; k >= 1; k--) {
               PCPoint p1, p2;
               p1 = &paAlloc[pVert[paw[k]].dwPoint];
               p2 = &paAlloc[pVert[paw[(k%dwPoints)+1]].dwPoint];
               if (!p1->AreClose (p2))
                  continue;

               // else they're close
               memmove (paw + k, paw + (k+1), (dwPoints - k)*sizeof(WORD));
               paw[dwPoints] = -1;   // indicate no point
               dwPoints--;
            } // k
         } // xyj
   } // i - over all sub


   // done
   if (pmPointToClipSpace) {
      m_fCalcPointToClipSpace = TRUE;
      m_mCalcPointToClipSpace.Copy (pmPointToClipSpace);
   }
   else
      m_fCalcPointToClipSpace = FALSE;
   m_fDirty = FALSE;
   return TRUE;
}


/**********************************************************************************
CHairLock::Render - Causes the hair to render.

inputs
   POBJECTRENDER        pr - Render information
   PCRenderSurface      prs - Render information
   PCObjectTemplate     pTemp - Template used to set the current surface to use
   DWORD                dwSurfaceBase - Surface for the outer hair layer. Subsequent
                        hair layers use next surfaces.
   BOOL                 fBackfaceCull - If TRUE then allow backface culling
   PCMatrix             pmPointToClipSpace - Converts from a point in the hair lock's coordinates
                        into a rotated and translated space where hair with z >= 0.0 is clipped due
                        to a hat. If this is NULL then no clipping
   PCMatrix             pmClipSpaceToPoint - Inverse of the matrix converting back. If pmPointToClipSpace
                        is null, this one should be NULL.
returns
   BOOL - TRUE if usccess
*/
BOOL CHairLock::Render (POBJECTRENDER pr, PCRenderSurface prs, PCObjectTemplate pTemp, DWORD dwSurfaceBase,
                        BOOL fBackfaceCull, PCMatrix pmPointToClipSpace, PCMatrix pmClipSpaceToPoint)
{
   // calculate
   if (!CalcRenderIfNecessary (pr, pmPointToClipSpace, pmClipSpaceToPoint))
      return FALSE;

   // if completely clipped then ignore
   if (m_fCalcCompletelyClipped)
      return TRUE;

   // figure out all the hair layers
   DWORD i, j, k;
   DWORD adwColor[MAXHAIRLAYERS];
   DWORD adwSurface[MAXHAIRLAYERS];
   memset (adwColor, 0, sizeof(adwColor));
   memset (adwSurface, 0, sizeof(adwSurface));

   // loop over all the sub renders
   for (i = 0; i < m_dwRenderSub; i++) {
      // call commit so RT will be faster
      prs->Commit();

      // allocate for the points
      PCPoint pPoint, pNorm;
      PTEXTPOINT5 pText;
      PVERTEX pVert;
      DWORD dwPoint, dwNorm, dwText, dwVert;
      pPoint = prs->NewPoints (&dwPoint, (DWORD)m_amemRenderPoint[i].m_dwCurPosn / sizeof(CPoint));
      if (!pPoint)
         return FALSE;
      memcpy (pPoint, m_amemRenderPoint[i].p, m_amemRenderPoint[i].m_dwCurPosn);

      // allocate memory for the normals
      pNorm = prs->NewNormals (TRUE, &dwNorm, (DWORD)m_amemRenderNorm[i].m_dwCurPosn / sizeof(CPoint));
      if (pNorm)
         memcpy (pNorm, m_amemRenderNorm[i].p, m_amemRenderNorm[i].m_dwCurPosn);

      // allocate memory for the textures
      DWORD dwTextElem = (DWORD)m_amemRenderText[i].m_dwCurPosn / sizeof(TEXTPOINT5);
      pText = prs->NewTextures (&dwText, dwTextElem * m_dwHairLayers);
      for (k = 0; k < m_dwHairLayers; k++) {
         if (pText)
            memcpy (pText + k*dwTextElem, m_amemRenderText[i].p,
               m_amemRenderText[i].m_dwCurPosn);

         // call set def material to keep track of different materials
         prs->SetDefMaterial (pTemp->m_OSINFO.dwRenderShard, pTemp->ObjectSurfaceFind (dwSurfaceBase+k), pTemp->m_pWorld);

         adwColor[k] = prs->DefColor();
         adwSurface[k] = prs->DefSurface();

         // loop through all the textures and transform.
         DWORD dwAround = (4 << m_dwDetailProf);
         if (pText) {
            prs->ApplyTextureRotation (&pText[k*dwTextElem], dwTextElem);
            for (j = 0; j < dwTextElem; j++)
               pText[j + k*dwTextElem].hv[0] = (fp)(j % (dwAround+1)) / (fp)dwAround *
                  (fp)m_adwHairLayerRepeat[k];
         }
      } // k
      
      // allocate memory for the vertices
      DWORD dwNumVert = (DWORD)m_amemRenderVert[i].m_dwCurPosn / sizeof(VERTEX);
      pVert = prs->NewVertices (&dwVert, dwNumVert);
      PVERTEX pCopy = (PVERTEX)m_amemRenderVert[i].p;
      for (j = 0; j < dwNumVert; j++) {
         pVert[j].dwColor = adwColor[pCopy[j].dwColor];
         pVert[j].dwNormal = pNorm ? (dwNorm + pCopy[j].dwNormal) : 0;
         pVert[j].dwPoint = dwPoint + pCopy[j].dwPoint;
         pVert[j].dwTexture = pText ? (dwText + pCopy[j].dwTexture) : 0;
      }

      // add all the polygons
      DWORD dwNumPoly = (DWORD)m_amemRenderPoly[i].m_dwCurPosn / sizeof(WORD) / 5;
      WORD *pawPoly = (WORD*)m_amemRenderPoly[i].p;
      for (j = 0; j < dwNumPoly; j++, pawPoly += 5) {
         for (k = 1; k < 5; k++)
            if (pawPoly[k] == (WORD)-1)
               break;

         PPOLYDESCRIPT ppd;
         if (k == 4)
            ppd = prs->NewTriangle (dwVert + pawPoly[1], dwVert + pawPoly[2], dwVert + pawPoly[3],
               fBackfaceCull);
         else if (k == 5)
            ppd = prs->NewQuad (dwVert + pawPoly[1], dwVert + pawPoly[2], dwVert + pawPoly[3],
               dwVert + pawPoly[4], fBackfaceCull);
         else
            continue;

         // set the surface
         ppd->dwSurface = adwSurface[pawPoly[0]];
      } // j
   } // i - hair layers

   // call commit so RT will be faster
   prs->Commit();
   return TRUE;
}


/**********************************************************************************
CHairLock::Render - Causes the hair to render.

inputs
   POBJECTRENDER        pr - Render information
   PCRenderSurface      prs - Render information
   PCObjectTemplate     pTemp - Template used to set the current surface to use
   DWORD                dwSurfaceBase - Surface for the outer hair layer. Subsequent
                        hair layers use next surfaces.
   BOOL                 fBackfaceCull - If TRUE then allow backface culling
   PCMatrix             pmPointToClipSpace - Converts from a point in the hair lock's coordinates
                        into a rotated and translated space where hair with z >= 0.0 is clipped due
                        to a hat. If this is NULL then no clipping
   PCMatrix             pmClipSpaceToPoint - Inverse of the matrix converting back. If pmPointToClipSpace
                        is null, this one should be NULL.
returns
   BOOL - TRUE if usccess
*/
void CHairLock::QueryBoundingBox (PCPoint pCorner1, PCPoint pCorner2,
                        PCMatrix pmPointToClipSpace, PCMatrix pmClipSpaceToPoint)
{
   pCorner1->Zero();
   pCorner2->Zero();

   // calculate
   if (!CalcRenderIfNecessary (NULL, pmPointToClipSpace, pmClipSpaceToPoint))
      return;

   // if completely clipped then ignore
   if (m_fCalcCompletelyClipped)
      return;

   // figure out all the hair layers
   DWORD i, j;

   // loop over all the sub renders
   BOOL pSet = FALSE;
   for (i = 0; i < m_dwRenderSub; i++) {
      PCPoint pp = (PCPoint)m_amemRenderPoint[i].p;
      for (j = 0; j < m_amemRenderPoint[i].m_dwCurPosn / sizeof(CPoint); j++, pp++) {
         if (pSet) {
            pCorner1->Min (pp);
            pCorner2->Max (pp);
         }
         else {
            pCorner1->Copy (pp);
            pCorner2->Copy (pp);
            pSet = TRUE;
         }
      } // j
   } // i - hair layers
}
/**********************************************************************************
CHairLock::ControlPointQuery - Like the normal ControlPointQuery except...

inptus
   DWORD       dwControlBase - Where the control point index for this lock of hair
               starts
   BOOL        fRoot - If TRUE then the user can modify the root's location. Else,
               the root is hardcoded
   BOOL        fCanDelete - If TRUE then the user can delete the last point, and
               end up deleting the entire hair
*/
BOOL CHairLock::ControlPointQuery (DWORD dwControlBase, BOOL fRoot, BOOL fCanDelete, 
                                   DWORD dwID, POSCONTROL pInfo)
{
   DWORD dwNum = m_lPathPoint.Num();
   fp fCPSizeSmall = m_fDiameter / 2;
   fp fCPSizeLarge = m_fDiameter * 2;

   CalcRenderIfNecessary (NULL, NULL, NULL);

   if ((dwID == dwControlBase+0) && fRoot) {
      // enumerate the root
      memset (pInfo, 0, sizeof(*pInfo));
      pInfo->cColor = RGB(0x80, 0, 0x80);
      pInfo->dwID = dwID;
      pInfo->dwStyle = CPSTYLE_SPHERE;
      pInfo->fSize = fCPSizeLarge;
      pInfo->pLocation.Copy ((PCPoint)m_lPathPoint.Get(0));
      wcscpy (pInfo->szName, L"Orient hair roots");
      
      return TRUE;
   }
   else if (dwID == dwControlBase+1) {
      // drag to resize
      memset (pInfo, 0, sizeof(*pInfo));
      pInfo->cColor = RGB(0xff, 0xff, 0);
      pInfo->dwID = dwID;
      pInfo->dwStyle = CPSTYLE_SPHERE;
      pInfo->fSize = fCPSizeLarge;

      // can't take the end point exactly since must account for the remainder
      pInfo->pLocation.Subtract ((PCPoint)m_lPathPoint.Get(dwNum-1), (PCPoint)m_lPathPoint.Get(dwNum-2));
      pInfo->pLocation.Scale (m_fRemainder);
      pInfo->pLocation.Add ((PCPoint)m_lPathPoint.Get(dwNum-2));

      wcscpy (pInfo->szName, L"Drag to lengthen hair");
      
      return TRUE;
   }
   else if ((dwID == dwControlBase+2) && (dwNum > (DWORD)(2 + (fCanDelete ? 0 : 1)))) {
      // delete
      memset (pInfo, 0, sizeof(*pInfo));
      pInfo->fButton = TRUE;
      pInfo->cColor = RGB(0xff, 0, 0);
      pInfo->dwID = dwID;
      pInfo->dwStyle = CPSTYLE_POINTER;
      pInfo->fSize = fCPSizeLarge;
      pInfo->pLocation.Average ((PCPoint)m_lPathPoint.Get(dwNum-2),
         (PCPoint)m_lPathPoint.Get(dwNum-1));
      wcscpy (pInfo->szName, L"Click to shorten/delete"); // BUGFIX - Was delete
      
      return TRUE;
   }
   else if ((dwID >= dwControlBase+4+2) && (dwID < dwControlBase+4+dwNum-1)) {
      // move IK
      memset (pInfo, 0, sizeof(*pInfo));
      pInfo->cColor = RGB(0, 0xff, 0xff);
      pInfo->dwID = dwID;
      pInfo->dwStyle = CPSTYLE_SPHERE;
      pInfo->fSize = fCPSizeLarge;
      pInfo->pLocation.Copy ((PCPoint)m_lPathPoint.Get(dwID-dwControlBase-4));
      wcscpy (pInfo->szName, L"Position hair");
      
      return TRUE;
   }
   else if ((dwID >= dwControlBase+4+1+dwNum) && (dwID < dwControlBase+4+dwNum*2-1)) {
      // BUGFIX - Was +2, but changed to +1 so can apply twist to root
      // move orientation
      memset (pInfo, 0, sizeof(*pInfo));
      pInfo->cColor = RGB(0, 0, 0xff);
      pInfo->dwID = dwID;
      pInfo->dwStyle = CPSTYLE_CUBE;
      pInfo->fSize = fCPSizeSmall;
      pInfo->pLocation.Copy ((PCPoint)m_lpTwistVector.Get(dwID-dwControlBase-4-dwNum));
      pInfo->pLocation.Scale (m_fDiameter);
      pInfo->pLocation.Add ((PCPoint)m_lPathPoint.Get(dwID-dwControlBase-4-dwNum));
      wcscpy (pInfo->szName, L"Twist hair");
      
      return TRUE;
   }
   else
      return FALSE;
}


/**********************************************************************************
CHairLock::ControlPointSet - Like the normal ControlPointQuery except...

inptus
   PCObjectTemplate pTemp - For notifying object change
   DWORD       dwControlBase - Where the control point index for this lock of hair
               starts
   BOOL        fRoot - If TRUE then the user can modify the root's location. Else,
               the root is hardcoded
   BOOL        fCanDelete - If TRUE then the user can delete the last point, and
               end up deleting the entire hair
   PCPoint     pEllipseCent - Center of ellipse for collision detect
   PCPoint     pEllipseRad - Radius of ellipse for collision detect
   DWORD       *pdwChanged - If not NULL, filled with information about what has
                  been changed. 0 for nothing, 1 for move node, 2 for rotate node,
                  3 for delete entire hair lock, 4 for resize,
                  5 if moved the last CP and resized
   DWORD       *pdwNode - If pdwChanged is 1 or 2, this is the node
   PCPoint     pChangedVect - If pdwChanged is move node this is the amount moved,
                     if rotate node p[0] = amount rotated
   PCPoint     pChangedOrig - If pdwChanged is 1, then this is filled with the original
                     point that was moved
   fp          fMoveFalloffDist - If a CP is moved using IK, this is the maximum distance
                     away (in m) that other CP in the hair will also be moved.
   fp             fEndHairWeight - Weighting of the old location for hair weighting. 1.0 => end
                  hairs continue point in the same direction. 0.0 => end hairs drag along
                  in space
*/
BOOL CHairLock::ControlPointSet (PCObjectTemplate pTemp, DWORD dwControlBase, BOOL fRoot, BOOL fCanDelete, 
                                 DWORD dwID, PCPoint pVal, PCPoint pViewer,
                                 PCPoint pEllipseCent, PCPoint pEllipseRad,
                                 DWORD *pdwChanged, DWORD *pdwNode, PCPoint pChangedVect, PCPoint pChangedOrig,
                                 fp fMoveFalloffDist, fp fEndHairWeight)
{
   DWORD dwNum = m_lPathPoint.Num();
   if (pdwChanged)
      *pdwChanged = 0;
   if (pdwNode)
      *pdwNode = 0;
   if (pChangedVect)
      pChangedVect->Zero();
   if (pChangedOrig)
      pChangedOrig->Zero();


   if ((dwID == dwControlBase+0) && fRoot) {
      // enumerate the root
      PCPoint pPath = (PCPoint)m_lPathPoint.Get(0);
      CPoint pNew;
      pNew.Subtract (pVal, pPath + 1);
      pNew.Normalize();
      if (pNew.Length() < .5) {
         pNew.Zero();
         pNew.p[0] = 1;
      }
      pNew.Scale (m_fLengthPerPoint);
      pNew.Add (pPath + 1);

      if (pTemp && pTemp->m_pWorld)
         pTemp->m_pWorld->ObjectAboutToChange (pTemp);
      pPath[0].Copy (&pNew);
      m_fDirty = TRUE;
      if (pTemp && pTemp->m_pWorld)
         pTemp->m_pWorld->ObjectChanged (pTemp);

      return TRUE;
   }
   else if (dwID == dwControlBase+1) {
      // drag to resize
      PCPoint pPath;
      CPoint pNew;
      BOOL fSentChanged = FALSE;

      while (TRUE) {
         dwNum = m_lPathPoint.Num();
         pPath = (PCPoint)m_lPathPoint.Get(0);
         pNew.Subtract (pVal, pPath + (dwNum-2));
         fp fLen = pNew.Length();
         if (fLen <= m_fLengthPerPoint) {
            // only moving last one, no adding
            if (pTemp && pTemp->m_pWorld && !fSentChanged) {
               pTemp->m_pWorld->ObjectAboutToChange (pTemp);
               fSentChanged = TRUE;
            }

            // remember old loc
            CPoint pOld;
            pOld.Copy (&pPath[dwNum-1]);

            m_fRemainder = fLen / m_fLengthPerPoint;
            pNew.Normalize();
            pNew.Scale (m_fLengthPerPoint);
            pPath[dwNum-1].Add (pPath + (dwNum-2), &pNew);
            m_fDirty = TRUE;

            if (pdwChanged && !(*pdwChanged)) {
               // just moved last one
               *pdwChanged = 5; // resize
               if (pdwNode)
                  *pdwNode = dwNum-1;
               if (pChangedVect)
                  pChangedVect->Subtract (&pPath[dwNum-1], &pOld);
            }

            goto doneresize;
         }

         // else, length is longer than the point, so add a new one
         if (dwNum >= MAXLOCKNODES)
            goto doneresize; // too many nodes so do no more

         if (pdwChanged)
            *pdwChanged = 4; // resize

         if (pTemp && pTemp->m_pWorld && !fSentChanged) {
            pTemp->m_pWorld->ObjectAboutToChange (pTemp);
            fSentChanged = TRUE;
         }

         // modify the last length so points in the right direction
         pNew.Normalize();
         pNew.Scale (m_fLengthPerPoint);
         pPath[dwNum-1].Add (pPath + (dwNum-2), &pNew);
         m_fRemainder = 1;
         m_fDirty = TRUE;

         // create a new length
         pNew.Add (&pPath[dwNum-1]);
         fp fTwist = *((fp*)m_lPathTwist.Get(dwNum-1));
         m_lPathPoint.Add (&pNew);
         m_lPathTwist.Add (&fTwist);
      }  // while TRUE

doneresize:
      if (pTemp && pTemp->m_pWorld && fSentChanged)
         pTemp->m_pWorld->ObjectChanged (pTemp);

      return TRUE;
   }
   else if ((dwID == dwControlBase+2) && (dwNum > (DWORD)(2 + (fCanDelete ? 0 : 1)))) {
      // delete
      if (pTemp && pTemp->m_pWorld)
         pTemp->m_pWorld->ObjectAboutToChange (pTemp);

      m_lPathPoint.Remove (dwNum-1);
      m_lPathTwist.Remove (dwNum-1);
      m_fRemainder = 0.5; // set to half way
      m_fDirty = TRUE;

      if (pTemp && pTemp->m_pWorld)
         pTemp->m_pWorld->ObjectChanged (pTemp);

      if (pdwChanged)
         *pdwChanged = (m_lPathPoint.Num() <= 2) ? 3 : 4;

      return TRUE;
   }
   else if ((dwID >= dwControlBase+4+2) && (dwID < dwControlBase+4+dwNum-1)) {
      // move IK

      // which direction
      DWORD dwPoint = dwID - dwControlBase - 4;
      CPoint pPullDir, pPullCent;
      pPullCent.Copy ((PCPoint)m_lPathPoint.Get(dwPoint));
      pPullDir.Subtract (pVal, &pPullCent);

      // move it
      if (pTemp && pTemp->m_pWorld)
         pTemp->m_pWorld->ObjectAboutToChange (pTemp);
      srand (GetTickCount());
      IKPull (&pPullCent, &pPullDir, fMoveFalloffDist, pEllipseCent, pEllipseRad, fEndHairWeight);
      if (pTemp && pTemp->m_pWorld)
         pTemp->m_pWorld->ObjectChanged (pTemp);

      if (pdwChanged)
         *pdwChanged = 1;
      if (pdwNode)
         *pdwNode = dwPoint;
      if (pChangedVect)
         pChangedVect->Copy (&pPullDir);
      if (pChangedOrig)
         pChangedOrig->Copy (&pPullCent);

      return TRUE;
   }
   else if ((dwID >= dwControlBase+4+1+dwNum) && (dwID < dwControlBase+4+dwNum*2-1)) {
      // BUGFIX - Was +2, but changed to +1 so can apply twist to root
      // move orientation
      DWORD dwNode = dwID - dwControlBase - 4 - dwNum;

      // find dir and up vectors
      PCPoint pDir = (PCPoint)m_lpDirVector.Get (dwNode);
      PCPoint pUp = (PCPoint)m_lpTwistVector.Get (dwNode);

      // calculate right vector
      CPoint pRight;
      pRight.CrossProd (pDir, pUp);
      pRight.Normalize();

      // figure out new location
      CPoint pNew;
      pNew.Subtract (pVal, (PCPoint)m_lPathPoint.Get(dwNode));

      // and what angle is this
      fp fAngle, fx, fy;
      fy = pNew.DotProd (pUp);
      fx = pNew.DotProd (&pRight);
      fAngle = atan2(fx, fy);

      // apply the twist
      if (pTemp && pTemp->m_pWorld)
         pTemp->m_pWorld->ObjectAboutToChange (pTemp);

      fp *pf = (fp*)m_lPathTwist.Get(dwNode);
      *pf = *pf + fAngle;
      m_fDirty = TRUE;

      if (pTemp && pTemp->m_pWorld)
         pTemp->m_pWorld->ObjectChanged (pTemp);

      // notify what changed
      if (pdwChanged)
         *pdwChanged = 2;
      if (pdwNode)
         *pdwNode = dwNode;
      if (pChangedVect)
         pChangedVect->p[0] = fAngle;

      return TRUE;
   }
   else
      return FALSE;
}


/**********************************************************************************
CHairLock::ControlPointEnum - Like the normal ControlPointQuery except...

inptus
   DWORD       dwControlBase - Where the control point index for this lock of hair
               starts
   BOOL        fRoot - If TRUE then the user can modify the root's location. Else,
               the root is hardcoded
   BOOL        fCanDelete - If TRUE then the user can delete the last point, and
               end up deleting the entire hair
*/
void CHairLock::ControlPointEnum (DWORD dwControlBase, BOOL fRoot, BOOL fCanDelete, PCListFixed plDWORD)
{
   DWORD i, dwAdd;

   if (fRoot) {
      // allow to modify the root's direction
      dwAdd = dwControlBase + 0;
      plDWORD->Add (&dwAdd);
   }

   // allow to modify the tip
   dwAdd = dwControlBase + 1;
   plDWORD->Add (&dwAdd);

   // provide a delete button
   if (m_lPathPoint.Num() > (2 + (DWORD)(fCanDelete ? 0 : 1))) {
      dwAdd = dwControlBase + 2;
      plDWORD->Add (&dwAdd);
   }

   // IK drag points for hair
   plDWORD->Required (plDWORD->Num() + m_lPathPoint.Num());
   for (i = 2; i+1 < m_lPathPoint.Num(); i++) {
      // IK for hair
      dwAdd = dwControlBase + 4 + i;
      plDWORD->Add (&dwAdd);
   }
   plDWORD->Required (plDWORD->Num() + m_lPathPoint.Num());
   for (i = 1; i+1 < m_lPathPoint.Num(); i++) {
      // BUGFIX - Was 2, but changed to +1 so can apply twist to root
      // twist in the hair
      dwAdd = dwControlBase + 4 + m_lPathPoint.Num()+ i;
      plDWORD->Add (&dwAdd);
   }
}

/**********************************************************************************
CHairLock::IKScore - Given a path, this calculates the score. Higher numbers
are better.

inputs
   PCPoint        papPath - Pointer to an arrya of m_lPathPoint.Num() points
                  whose score is to be calculated
   DWORD          dwNumPoint - Number of points to pull
   DWORD          *padwSplintPoint - Pointer to an array of dwNumPoint DWORDs indicating
                     which point in the spline is pulled.
   PCPoint        paPullDir - Direction to pull each of the points.
   PCPoint        pEllipseCent - Center of the ellipse for collision. If NULL then no collision
   PCPoint        pEllipseRad - Radius of the ellipse for collision. If NULL then no collision
returns
   fp - Score. Higher is better
*/
fp CHairLock::IKScore (PCPoint papPath, DWORD dwNumPoint, DWORD *padwSplinePoint, PCPoint paPullDir, 
   PCPoint pEllipseCent, PCPoint pEllipseRad)
{
   DWORD dwNum = m_lPathPoint.Num();
   fp fScore = 0;

   // loop through all the points that can be moved
   DWORD i, j;
   PCPoint papOrig = (PCPoint)m_lPathPoint.Get(0);
   for (i = 2; i < dwNum; i++) {
      // see if this is one of the paths that's being pulled
      for (j = 0; j < dwNumPoint; j++)
         if (padwSplinePoint[j] == i)
            break;

      // find out where want it to be
      CPoint pWant;
      pWant.Copy (papOrig + i);
      if (j < dwNumPoint)
         pWant.Add (&paPullDir[j]);

      // find the distance
      fp fLen;
      pWant.Subtract (papPath + i);
      fLen = pWant.Length();
      fLen *= fLen;

      // subtract the distance from the score, subtracting more if it's one
      // of the points being pulled
      fScore -= fLen * ((j < dwNumPoint) ? 10 : 1);

      // see if it intersects the ellipse
      if (!pEllipseCent || !pEllipseRad)
         continue;   // no ellipse

      // figure out where point is relative to ellipse
      CPoint pRad;
      pWant.Subtract (papPath + i, pEllipseCent);
      pRad.Copy (&pWant);
      pRad.Normalize();
      pRad.Scale (m_fDiameter/2);
      pWant.Subtract (&pRad); // remove diameter of strand from check
      pWant.p[0] /= pEllipseRad->p[0];
      pWant.p[1] /= pEllipseRad->p[1];
      pWant.p[2] /= pEllipseRad->p[2];
      fp fDist;
      fDist = pWant.Length();
      if (fDist >= 1)
         continue;   // no intersection

      // else, intersects
      fDist = 1.0 - fDist;
      fDist *= pEllipseRad->Length();  // so in units of meters
      fDist *= 100;  // to make this important not to intersect
      fScore -= fDist * fDist;
   } // i

   return fScore;
}


/**********************************************************************************
CHairLock::IKJitter - This applies jitter to the existing path and PULLs it one
level. It ends up modifying the path in papPath

NOTE: Before calling this srand() must have been set, probably based on GetTickCount()

inputs
   PCPoint        papPath - Pointer to an arrya of m_lPathPoint.Num() points
                  which is the path to begin with and then modify.
   DWORD          dwNumPoint - Number of points to pull
   DWORD          *padwSplintPoint - Pointer to an array of dwNumPoint DWORDs indicating
                     which point in the spline is pulled.
   PCPoint        paPullDir - Direction to pull each of the points.
   PCPoint        pEllipseCent - Center of the ellipse for collision. If NULL then no collision
   PCPoint        pEllipseRad - Radius of the ellipse for collision. If NULL then no collision
returns
   BOOL - TRUE if found a better score than the default, FALSE if didn't
*/
BOOL CHairLock::IKJitter (PCPoint papPath, DWORD dwNumPoint, DWORD *padwSplinePoint, PCPoint paPullDir,
   PCPoint pEllipseCent, PCPoint pEllipseRad)
{
   BOOL fFoundBetter = FALSE;

   // allocate two lists, one to store the current jittered path, and one to store
   // the one with the best score
   CListFixed lTest, lBest;
   fp fTest, fBest;
   DWORD dwNum = m_lPathPoint.Num();
   lBest.Init (sizeof(CPoint), papPath, dwNum);
   fBest = IKScore ((PCPoint)lBest.Get(0), dwNumPoint, padwSplinePoint, paPullDir, pEllipseCent, pEllipseRad);

   // how much to jitter
   fp fJitter = m_fLengthPerPoint / 100.0;

   // try out 10 jitter attempts and take best score
   DWORD i, j;
   for (i = 0; i < 10; i++) {
      lTest.Init (sizeof(CPoint), papPath, dwNum);
      PCPoint pTest = (PCPoint)lTest.Get(0);

      // randomize coords
      for (j = 2; j < dwNum; j++) {
         pTest[j].p[0] += randf(-fJitter, fJitter);
         pTest[j].p[1] += randf(-fJitter, fJitter);
         pTest[j].p[2] += randf(-fJitter, fJitter);
      } // j

      // normalize the lengths
      FixLengths (pTest);

      // get a score
      fTest = IKScore (pTest, dwNumPoint, padwSplinePoint, paPullDir, pEllipseCent, pEllipseRad);
      if (fTest <= fBest)
         continue;   // not any better

      // else, found a better one
      fFoundBetter = TRUE;
      fBest = fTest;
      lBest.Init (sizeof(CPoint), pTest, dwNum);
   } // i

   // transfer over
   if (fFoundBetter)
      memcpy (papPath, (PCPoint)lBest.Get(0), lBest.Num() * sizeof(CPoint));

   return fFoundBetter;
}

/**********************************************************************************
CHairLock::IKPull - Pulls the given parts of the hair in the given direction.

NOTE: Before calling this srand() must have been set, probably based on GetTickCount()

inputs
   DWORD          dwNumPoint - Number of points to pull
   DWORD          *padwSplintPoint - Pointer to an array of dwNumPoint DWORDs indicating
                     which point in the spline is pulled.
   PCPoint        paPullDir - Direction to pull each of the points.
   PCPoint        pEllipseCent - Center of the ellipse for collision. If NULL then no collision
   PCPoint        pEllipseRad - Radius of the ellipse for collision. If NULL then no collision
   fp             fEndHairWeight - Weighting of the old location for hair weighting. 1.0 => end
                  hairs continue point in the same direction. 0.0 => end hairs drag along
                  in space
returns
   BOOL - TRUE if success
*/
BOOL CHairLock::IKPull (DWORD dwNumPoint, DWORD *padwSplinePoint, PCPoint paPullDir,
   PCPoint pEllipseCent, PCPoint pEllipseRad, fp fEndHairWeight)
{
   // copy over the current path so can jitter it
   CListFixed lJitter, lFinal;
   lJitter.Init (sizeof(CPoint), m_lPathPoint.Get(0), m_lPathPoint.Num());
   lFinal.Init (sizeof(CPoint));

   // loop while jitter is still getting closer
   DWORD dwSinceLastSuccess = 0;
   while (dwSinceLastSuccess < 5) {
      if (IKJitter ((PCPoint)lJitter.Get(0), dwNumPoint, padwSplinePoint, paPullDir, pEllipseCent, pEllipseRad))
         dwSinceLastSuccess = 0;
      else
         dwSinceLastSuccess++;   // didnt come up with better soln
   }

   // BUGFIX - Since can't really pull the last point properly, modify
   // the last point so continues in the same direction
   PCPoint paPoint = (PCPoint) lJitter.Get(0);
   DWORD dwNum = lJitter.Num();
   if (dwNum >= 4) { // BUGFIX - Was >=3 but want to make short hairs ignore
      paPoint[dwNum-1].Subtract (&paPoint[dwNum-2], &paPoint[dwNum-3]);
      paPoint[dwNum-1].Add (&paPoint[dwNum-2]);
   }

   // figure out the weighting of each of the points
   CMem memWeight;
   if (!memWeight.Required (dwNum*sizeof(fp)))
      return FALSE;
   fp *paf = (fp*)memWeight.p;
   memset (paf, 0, dwNum * sizeof(fp));
   fp fSum = 0;
   fp fDist;
   DWORD i;
   for (i = 0; i < dwNumPoint; i++) {
      fDist = paPullDir[i].Length();
      paf[padwSplinePoint[i]] = fDist;
      fSum += fDist;
   } // i
   if (fSum)
      fSum = fEndHairWeight / fSum;
   fp fCur = 0;
   for (i = 0; i < dwNum; i++) {
      fDist = paf[i] * fSum;  // scale so everthing eventually sums to fEndHairWeight
      paf[i] = fCur;
      fCur += fDist;
   } // i

   // go through and weight all the directions
   CPoint pDirNew, pDirOrig;
   fp fDistCur;
   PCPoint paOrig = (PCPoint)m_lPathPoint.Get(0);
   lFinal.Add (paOrig);
   for (i = 1; i < dwNum; i++) {
      pDirNew.Subtract (&paPoint[i], &paPoint[i-1]);
      fDist = pDirNew.Length();
      pDirOrig.Subtract (&paOrig[i], &paOrig[i-1]);
      pDirNew.Average (&pDirOrig, paf[i]);   // so end hairs are weighted more
      fDistCur = pDirNew.Length();
      if ((fDistCur < CLOSE) || (fDist < CLOSE))
         pDirNew.Subtract (&paPoint[i], &paPoint[i-1]);  // either divide by 0, or new one close to 0, so keep old
      else
         pDirNew.Scale (fDist / fDistCur);
      pDirNew.Add ((PCPoint)lFinal.Get(i-1));   // add to the previous
      lFinal.Add (&pDirNew);
   } // i

   // copy this over
   m_lPathPoint.Init (sizeof(CPoint), lFinal.Get(0), lFinal.Num());
   m_fDirty = TRUE;
   return TRUE;
}


/**********************************************************************************
CHairLock::IKPull - Pulls the given parts of the hair in the given direction.

NOTE: Before calling this srand() must have been set, probably based on GetTickCount()

inputs
   PCPoint        pPullFrom - What point is space is being pulled from
   PCPoint        pPullDir - Direction (and amount) to pull the given points. Base
                  this off m_lPathPoint.Get() locations
   fp             fFallOffDist - If point is further away than this then ignore it
   PCPoint        pEllipseCent - Center of the ellipse for collision. If NULL then no collision
   PCPoint        pEllipseRad - Radius of the ellipse for collision. If NULL then no collision
   fp             fEndHairWeight - Weighting of the old location for hair weighting. 1.0 => end
                  hairs continue point in the same direction. 0.0 => end hairs drag along
                  in space
returns
   BOOL - TRUE if success. FALSE if nothing to pull
*/
BOOL CHairLock::IKPull (PCPoint pPullFrom, PCPoint pPullDir, fp fFallOffDist,
   PCPoint pEllipseCent, PCPoint pEllipseRad, fp fEndHairWeight)
{
   // find all the points the get pulled and their weight
   CListFixed lIndex, lVect;
   lIndex.Init (sizeof(DWORD));
   lVect.Init (sizeof(CPoint));
   PCPoint pPath = (PCPoint) m_lPathPoint.Get(0);
   CPoint pDist;
   fp lDist;
   DWORD dwNum = m_lPathPoint.Num();
   DWORD i;
   lIndex.Required (dwNum);
   lVect.Required (dwNum);
   for (i = 2; i < dwNum; i++) {
      pDist.Subtract (pPullFrom, &pPath[i]);
      lDist = pDist.Length();
      if (lDist >= fFallOffDist)
         continue;

      // else found
      lDist = (1.0 - lDist / fFallOffDist);
      lDist = sqrt(lDist);

      pDist.Copy (pPullDir);
      pDist.Scale (lDist);

      // add
      lIndex.Add (&i);
      lVect.Add (&pDist);
   }

   // if no points then don't bother...
   if (!lIndex.Num())
      return FALSE;

   // else move
   return IKPull (lIndex.Num(), (DWORD*)lIndex.Get(0), (PCPoint)lVect.Get(0),
      pEllipseCent, pEllipseRad, fEndHairWeight);
}

/**********************************************************************************
CHairLock::Tip - Fills in a point where the tip of the hair lock is.

inputs
   PCPoint        pTip - Filled with the tip
*/
void CHairLock::Tip (PCPoint pTip)
{
   PCPoint pPath = (PCPoint) m_lPathPoint.Get(0);
   DWORD dwNum = m_lPathPoint.Num();

   pTip->Subtract (&pPath[dwNum-1], &pPath[dwNum-2]);
   pTip->Scale (m_fRemainder);
   pTip->Add (&pPath[dwNum-2]);
   
}

/**********************************************************************************
CHairLock::Blend - Blends together two or more other hair objects.

inputs
   PCPoint        pSubRoot - Point that's the location for node 0 (below the scalp),
                  for the new hair
   PCPoint        pRoot - Point that's the location of node 1 (root)
   DWORD          dwNum - Number of CHairLock objects to blend together
   CHairLock      **papBlend - Pointer to an array of dwNum PCHairLock to blend together
   fp             *pafBlend - Array of dwNum weights. The values can sum to anything.
returns
   BOOL - TRUE if sucess
*/
BOOL CHairLock::Blend (PCPoint pSubRoot, PCPoint pRoot, DWORD dwNum, CHairLock **papBlend, fp*pafBlend)
{
   // clone one of the hairs over to use for the length info, etc.
   papBlend[0]->CloneTo (this);

   // figure out the maximum length
   DWORD i, j, dwMax;
   dwMax = 0;
   for (i = 0; i < dwNum; i++)
      dwMax = max(dwMax, papBlend[i]->m_lPathPoint.Num());

   // populate the list...
   m_lPathPoint.Clear();
   m_lPathTwist.Clear();
   m_lPathPoint.Required (dwMax);
   m_lPathTwist.Required (dwMax);
   for (i = 0; i < dwMax; i++) {
      CPoint pUse;
      fp fUse;

      if (i == 0)
         pUse.Copy (pSubRoot);
      else if (i == 1)
         pUse.Copy (pRoot);
      else
         pUse.Zero();
      fUse = 0;

      fp fTotal = 0;
      for (j = 0; j < dwNum; j++) {
         PCPoint pAdd1 = (PCPoint) papBlend[j]->m_lPathPoint.Get(i);
         PCPoint pAdd2 = (PCPoint) papBlend[j]->m_lPathPoint.Get(i ? (i-1) : 0);
         fp *pfAdd = (fp*) papBlend[j]->m_lPathTwist.Get(i);

         if (!pAdd1 || !pfAdd)
            continue;   // nothing left of this

         // else, include weight
         fTotal += pafBlend[j];

         if (i >= 2) {
            CPoint pDelta;
            pDelta.Subtract (pAdd1, pAdd2);
            pDelta.Scale (pafBlend[j]);
            pUse.Add (&pDelta);
         }
         fUse += pafBlend[j] * pfAdd[0];
      } // j

      // normalize twist
      if (fTotal)
         fUse /= fTotal;

      // normalize length
      if (i >= 2) {
         pUse.Normalize();
         pUse.Scale (m_fLengthPerPoint);
         pUse.Add ((PCPoint)m_lPathPoint.Get(i-1));
      }

      // add
      m_lPathPoint.Add (&pUse);
      m_lPathTwist.Add (&fUse);
   } // i

   // make sure the lengths are fixed
   FixLengths();

   // adjust the length...
   fp fTotalLen = 0, fWeight = 0;
   for (i = 0; i < dwNum; i++) {
      fWeight += pafBlend[i];
      fTotalLen += papBlend[i]->TotalLengthGet() * pafBlend[i];
   }
   if (!fWeight)
      return FALSE;
   fTotalLen /= fWeight;
   TotalLengthSet (fTotalLen);   // shorten down to the average

   return TRUE;
}

/**********************************************************************************
CHairLock::GravitySimulate - Simulate gravity acting on the hair.

inputs
   fp             fAmount - Amount (relative to lengthperpoint). Uusallly 0.1
   PCPoint        pEllipseCent - Center of the ellipse
   PCPoint        pEllipseRad - Radius of the ellipse
*/
BOOL CHairLock::GravitySimulate (fp fAmount, PCPoint pEllipseCent, PCPoint pEllipseRad)
{
   // totla pull
   CPoint pPull;
   fp fGravity = m_fLengthPerPoint * fAmount;
   pPull.Zero();

   // figure out the pull on each hair
   CListFixed lPull, lIndex;
   lIndex.Init (sizeof(DWORD));
   lPull.Init (sizeof(CPoint));
   DWORD i;
   lIndex.Required (m_lPathPoint.Num());
   lPull.Required (m_lPathPoint.Num());
   for (i = 2; i < m_lPathPoint.Num(); i++) {
      lIndex.Add (&i);
      pPull.p[2] -= fGravity;
      lPull.Add (&pPull);
   }

   return IKPull (lPull.Num(), (DWORD*)lIndex.Get(0), (PCPoint)lPull.Get(0),
      pEllipseCent, pEllipseRad, 0.0);
}

/**********************************************************************************
CHairLock::Mirror - Mirrors the hair along the dimension.

inputs
   DWORD       dwDim - 0 for x, 1 for y, 2 for z
*/
BOOL CHairLock::Mirror (DWORD dwDim)
{
   m_fDirty = TRUE;

   PCPoint pPath = (PCPoint) m_lPathPoint.Get(0);
   fp *pafPathTwist = (fp*) m_lPathTwist.Get(0);
   DWORD i;
   for (i = 0; i < m_lPathPoint.Num(); i++) {
      pPath[i].p[dwDim] *= -1;
      pafPathTwist[i] *= -1;
   }

   return TRUE;
}

/**********************************************************************************
CHairLock::MessUp - Messes up the existing hair

NOTE: Before calling this srand() should be set

inputs
   fp             fAmount - Amount (relative to lengthperpoint). Uusallly 0.1
   PCPoint        pEllipseCent - Center of the ellipse
   PCPoint        pEllipseRad - Radius of the ellipse
returns
   BOOL - TRUE if success
*/
BOOL CHairLock::MessUp (fp fAmount, PCPoint pEllipseCent, PCPoint pEllipseRad)
{
   m_fDirty = TRUE;

   PCPoint pPath = (PCPoint) m_lPathPoint.Get(0);
   fp *pafPathTwist = (fp*) m_lPathTwist.Get(0);
   DWORD i, j;
   for (i = 2; i < m_lPathPoint.Num(); i++) for (j = 0; j < 3; j++)
      pPath[i].p[j] += randf(-fAmount * m_fLengthPerPoint/2, fAmount * m_fLengthPerPoint/2);
   FixLengths();

   // mess up curve too
   for (i = 1; i < m_lPathPoint.Num(); i++)
      pafPathTwist[i] += randf(-fAmount, fAmount) * PI/8;

   // make sure that ik ok
   return IKPull ((DWORD)0, (DWORD*)NULL, (PCPoint)NULL, pEllipseCent, pEllipseRad, 0.0);
}


/**********************************************************************************
CHairLock::Straighten - Mirrors the hair along the dimension.
*/
BOOL CHairLock::Straighten (void)
{
   m_fDirty = TRUE;

   PCPoint pPath = (PCPoint) m_lPathPoint.Get(0);
   fp *pafPathTwist = (fp*) m_lPathTwist.Get(0);

   CPoint pDelta;
   pDelta.Subtract (&pPath[1], &pPath[0]);

   DWORD i;
   for (i = 2; i < m_lPathPoint.Num(); i++)
      pPath[i].Add (&pPath[i-1], &pDelta);
   for (i = 0; i < m_lPathPoint.Num(); i++)
      pafPathTwist[i] = 0; // since straitening

   return TRUE;
}



/**********************************************************************************
CHairLock::Twist - Twists the hair

inputs
   BOOL        fLength - If TRUE then twists the hair along its length (2+). Otherwise
                        only twists the root (1)
   fp          fAngle - Angle to set the twist
*/
BOOL CHairLock::Twist (BOOL fLength, fp fAngle)
{
   m_fDirty = TRUE;

   PCPoint pPath = (PCPoint) m_lPathPoint.Get(0);
   fp *pafPathTwist = (fp*) m_lPathTwist.Get(0);

   DWORD i;
   if (!fLength)
      pafPathTwist[1] = fAngle;
   else for (i = 2; i < m_lPathPoint.Num(); i++)
      pafPathTwist[i] = fAngle;

   return TRUE;
}




