/**********************************************************************************
CDoor.cpp - File to draw doors, windows, and cabinet doors.

begun 27/4/02 by Mike Rozak
Copyright 2002 Mike Rozak. All rights reserved
*/

#include <windows.h>
#include "escarpment.h"
#include "..\M3D.h"
#include "resource.h"

/* locals */
#define REF_FLIPTEXT          0x0001      // if TRUE, flip the texture because polygon's front is y=1. If FALSE, front is y=-1
#define REF_ROTTEXT           0x0002      // if TRUE, rotate the texture 90 degrees
#define REF_CUTOUT            0x0004      // if TRUE then pPoly is the cutout, and pPanel the edge
#define REF_NOSIDES           0x0008      // if TRUE, then pPoly doesnt draw zipper around the sides

#define BITMORE               .001

typedef struct {
   DWORD          dwSurface;     // one of the DSURF_XXX
   PCNoodle       pNoodle;       // if not NULL, a noodle to draw
   PCRevolution   pRevolution;   // if not NULL, revolution to draw
   PCListFixed    pPoly;         // if not NULL, a list of CPoint for the polygon
   PCListFixed    pPanel;        // if not NULL, draw a panel. THis is the edge part. Use pPoly as raised part
   DWORD          dwFlags;       // one of the REF_XXX
   BOOL           fApplyMatrix;  // if TRUE then apply the rotation matrix
   CMatrix        mRotate;       // rotation matrix
} RENDELEM, *PRENDELEM;

typedef struct {
   PCDoor         pv;       // this door
   PCWorldSocket        pWorld;  // to notify if changed
   PCObjectSocket pThis;   // to notify is changed
   PCDoorSet      pDoorSet;   // to notify if changed
   DWORD          dwDoorNum;  // to pass to DoorSet
   PCObjectDoor   pObjectDoor;  // for opening
   int            iVScroll;   // for redo same page
} DCP, *PDCP;


#define DDPARAM   8              // maximum number of parameters
#define DDDIV     2              // maximum number of divisions

#define DDCUT_CIRCLE       0     // circle cutout shape
#define DDCUT_SQUARE       1     // square cutout shape
#define DDCUT_HEMICIRCLED  2     // hemicircle with down cut off
#define DDCUT_HEMICIRCLEU  3     // hemicircle with up cut of
#define DDCUT_HEMICIRCLEL  4     // hemiscircle with left cut off
#define DDCUT_HEMICIRCLER  5     // hemicircle with right cut off

#define DDMS_NONE          0     // no mullions
#define DDMS_HV            1     // horiontal and vertical
#define DDMS_DIAMOND       2     // diamond pattern
#define DDMS_RADIATE       3     // radiated pattern

#define DDCB_Z             0     // Z cross brace
#define DDCB_SLASH         1     // slash cross brace
#define DDCB_MORETHAN      2     // more than cross brace

#define DDT_FRAME          0     // put a frame around the door. 1 param=frame width; 1 div=contents
                                 // p1=color
#define DDT_SOLID          1     // solid panel, 1 param, 0 div
                                 // p1=fraction of depth, p2=DSURF_XXX
#define DDT_FRAMETB        2     // put a frame on top and bottom. 1 param=frame width, 1 div=between
                                 // p1=color
#define DDT_FRAMELR        3     // same as DDT_FRAMETB except on left and right
#define DDT_VOID           4     // nothing drawn inside
#define DDT_DIVIDEMULTIH   5     // divide into multiple bits using framing - horizontal dividers
                                 // 2 params=frame_width,#divides, 1 div=all divisions
                                 // p2=color
#define DDT_DIVIDEMULTIV   6     // like DDT_DIVIDEMULTIH but vertical
#define DDT_DIVIDEFIXEDH   7     // divide into two halves - a certain distance from the left/bottom
                                 // 2 params=frame_width,0..1 offset, 2 div=left/bottom and right/top
                                 // p2=color
#define DDT_DIVIDEFIXEDV   8     // like DDT_DIVIDEFIXEDH but vertical dividers
#define DDT_PANEL          9     // raised panel. 1 param=frame width, 0 div
#define DDT_CUTOUT         10    // solid panel with a cutout in it
                                 // p0=thicknes, p1=appear ID (DDCUT_XXX), p2=width as %, p3=height as %,
                                 // p4=keep square, p5=x 0..1, p6=z 0..1
                                 // 1 div=in cutout
#define DDT_GLASS          11    // glass panel
                                 // p0=coloring (DSURF_XXX or 0 for empty), p1=mullion style (DDMS_XXX),
                                 // p2=mulltion width, p3=X dist between mullion, p4=Z dist between mullion
                                 // If DDMS_RADIATE: p5=X 0..1 for radiate location, p6=Z 0..1 for radiate location,
                                 // if DDMS_RADIATE: p3=radius increment of circle, p4=length of divisions at largest radius
                                 // p7= coloring mullion
                                 // 0 div
#define DDT_CROSSBRACE     12    // cross bracing
                                 // p0=width, p1=thickness (as a fraction of door thickness),
                                 // p2=side (0 for front, 1 for back), p3=pattern (DDCB_XXX)
                                 // p4=rotation (0=0deg, 1=90deg, 2=180deg, 3=270deg)

class CDoorDiv {
public:
   ESCNEWDELETE;

   CDoorDiv(void);
   ~CDoorDiv(void);
   PCMMLNode2 MMLTo (void);
   BOOL MMLFrom (PCMMLNode2 pNode);
   PCDoorDiv Clone (void);
   DWORD ParamNum(void);
   DWORD DivNum(void);
   BOOL Commit (DWORD dwNum, PCPoint papShape, PCDoor pDoor);
   void AutoFill (DWORD dwType, BOOL fDoor);

   DWORD          m_dwType;      // type of division, see DDT_XXX
   fp         m_afParam[DDPARAM];  // parameters for the door
   PCDoorDiv      m_apDiv[DDDIV];   // divisions

private:
   BOOL CommitPolygon (DWORD dwNum, PCPoint papShape, PCDoor pDoor, fp fThickness,
                              DWORD dwColorFront, DWORD dwColorBack, DWORD dwFlags = 0);

};


/**********************************************************************************
DSurfMassage - Given the surface color from DSURF_XXX, will massage it
into the right EXT or INT setting.

inputs
   DWORD    dwSurf - DSURF_XXX
   BOOL     fExt - TRUE if external
returns
   DWORD - Surface. Usually the same, except sets right EXT and INT setting
*/
DWORD DSurfMassage (DWORD dwSurf, BOOL fExt = TRUE)
{
   switch (dwSurf) {
   case DSURF_EXTPANEL:
   case DSURF_INTPANEL:
      return (fExt ? DSURF_EXTPANEL : DSURF_INTPANEL);
   case DSURF_EXTFRAME:
   case DSURF_INTFRAME:
      return (fExt ? DSURF_EXTFRAME: DSURF_INTFRAME);
   default:
      return dwSurf;
   }
}

/**********************************************************************************
CDoorDiv::Constructor and destructor */
CDoorDiv::CDoorDiv (void)
{
   m_dwType = DDT_VOID;
   memset (m_afParam, 0, sizeof(m_afParam));
   memset (m_apDiv, 0, sizeof(m_apDiv));
}
CDoorDiv::~CDoorDiv (void)
{
   DWORD i;
   for (i = 0; i < DDDIV; i++)
      if (m_apDiv[i])
         delete m_apDiv[i];
}

/**********************************************************************************
CDoorDiv::AutoFill - Automatically fills in m_dwType with dwType, and
based on that fills in other parameters.

inputs
   DWORD       dwType - type
   BOOL        fDoor - TRUE if it's for a door, FALSE for a window
*/
void CDoorDiv::AutoFill (DWORD dwType, BOOL fDoor)
{
   m_dwType = dwType;

   // set up the parameters
   switch (m_dwType) {
   case DDT_FRAME:
   case DDT_FRAMELR:
   case DDT_FRAMETB:
      m_afParam[0] = fDoor ? CM_DOORDIVFRAMEDOOR : CM_DOORDIVFRAMEWINDOW;
      m_afParam[1] = DSURF_EXTFRAME;
      break;
   case DDT_SOLID :
      m_afParam[0] = 1;
      m_afParam[1] = DSURF_EXTPANEL;
      break;
   case DDT_PANEL:
      m_afParam[0] = (fDoor ? CM_DOORDIVFRAMEDOOR : CM_DOORDIVFRAMEWINDOW) *2 / 3;
      break;
   case DDT_VOID:
      break;
   case DDT_DIVIDEMULTIH:
   case DDT_DIVIDEMULTIV:
      m_afParam[0] = fDoor ? CM_DOORDIVFRAMEDOOR : CM_DOORDIVFRAMEWINDOW;
      m_afParam[1] = 1;
      m_afParam[2] = DSURF_EXTFRAME;
      break;
   case DDT_DIVIDEFIXEDH:
   case DDT_DIVIDEFIXEDV:
      m_afParam[0] = fDoor ? CM_DOORDIVFRAMEDOOR : CM_DOORDIVFRAMEWINDOW;
      m_afParam[1] = .5;
      m_afParam[2] = DSURF_EXTFRAME;
      break;
   case DDT_CUTOUT:
      m_afParam[0] = 1;
      m_afParam[1] = DDCUT_CIRCLE;
      m_afParam[2] = .5;
      m_afParam[3] = .5;
      m_afParam[4] = 1;
      m_afParam[5] = .5;
      m_afParam[6] = .5;
      break;
   case DDT_GLASS:
      m_afParam[0] = DSURF_GLASS;
      m_afParam[1] = DDMS_NONE;
      m_afParam[2] = CM_MULLIONWIDTH;
      m_afParam[3] = .2;
      m_afParam[4] = .3;
      m_afParam[5] = .5;
      m_afParam[6] = .5;
      m_afParam[7] = DSURF_EXTFRAME;
      break;
   case DDT_CROSSBRACE:
      m_afParam[0] = fDoor ? CM_DOORDIVFRAMEDOOR : CM_DOORDIVFRAMEWINDOW;
      m_afParam[1] = 1;
      m_afParam[2] = 0;
      m_afParam[3] = DDCB_Z;
      m_afParam[4] = 0;
      break;
   }

}


static PWSTR gpszType = L"type";
static PWSTR gpszDoorDiv = L"DoorDiv";

/**********************************************************************************
CDoorDiv::MMLTo - Writes the division to a MML node.

returns
   PCMMLNode2 - Node. NULL if error
*/
PCMMLNode2 CDoorDiv::MMLTo (void)
{
   PCMMLNode2 pNode = new CMMLNode2;
   if (!pNode)
      return NULL;
   pNode->NameSet (gpszDoorDiv);

   MMLValueSet (pNode, gpszType, (int) m_dwType);

   DWORD i, dwParam, dwDiv;
   dwParam = ParamNum();
   dwDiv = DivNum();
   WCHAR szTemp[32];
   for (i = 0; i < dwParam; i++) {
      swprintf (szTemp, L"param%d", i);
      MMLValueSet (pNode, szTemp, m_afParam[i]);
   }
   for (i = 0; i < dwDiv; i++) {
      if (!m_apDiv[i])
         continue;
      PCMMLNode2 pSub;
      pSub = m_apDiv[i]->MMLTo();
      if (!pSub)
         continue;
      swprintf (szTemp, L"div%d", i);
      pSub->NameSet (szTemp);
      pNode->ContentAdd (pSub);
   }

   return pNode;
}

/**********************************************************************************
CDoorDiv::MMLFrom - Reads the door division info from mml

inputsw
   PCMMLNode2         pNode - to read from
returns
   BOOL - TRUE if success
*/
BOOL CDoorDiv::MMLFrom (PCMMLNode2 pNode)
{
   // wipe out old stuff
   DWORD i;
   for (i = 0; i < DDDIV; i++)
      if (m_apDiv[i])
         delete m_apDiv[i];
   m_dwType = 0;
   memset (m_afParam, 0, sizeof(m_afParam));
   memset (m_apDiv, 0, sizeof(m_apDiv));

   // read in
   m_dwType = (DWORD) MMLValueGetInt (pNode, gpszType, 0);

   DWORD dwParam, dwDiv;
   dwParam = ParamNum();
   dwDiv = DivNum();
   WCHAR szTemp[32];
   for (i = 0; i < dwParam; i++) {
      swprintf (szTemp, L"param%d", i);
      m_afParam[i] = MMLValueGetDouble (pNode, szTemp, 0);
   }
   for (i = 0; i < dwDiv; i++) {
      swprintf (szTemp, L"div%d", i);
      DWORD dwIndex;
      dwIndex = pNode->ContentFind (szTemp);
      if (dwIndex == -1)
         continue;
      PWSTR psz;
      PCMMLNode2 pSub;
      pSub = NULL;
      pNode->ContentEnum (dwIndex, &psz, &pSub);
      if (!pSub)
         continue;

      // know have right name
      m_apDiv[i] = new CDoorDiv;
      if (!m_apDiv[i])
         continue;
      if (!m_apDiv[i]->MMLFrom (pSub))
         return FALSE;
   }

   return TRUE;
}

/**********************************************************************************
CDoorDiv::Clone - Clones the division.

returns
   PCDoorDiv - New division. NULL if error
*/
PCDoorDiv CDoorDiv::Clone (void)
{
   PCDoorDiv pNew = new CDoorDiv;
   if (!pNew)
      return NULL;

   // know that it starts out blank so dont need to delete
   pNew->m_dwType = m_dwType;
   memcpy (pNew->m_afParam, m_afParam, sizeof(m_afParam));

   DWORD i;
   for (i = 0; i < DDDIV; i++)
      if (m_apDiv[i])
         pNew->m_apDiv[i] = m_apDiv[i]->Clone();

   return pNew;
}

/**********************************************************************************
CDoorDiv::ParamNum - Returns the number of parameters based on the division's
m_dwType.
*/
DWORD CDoorDiv::ParamNum(void)
{
   switch (m_dwType) {
   case DDT_CUTOUT:
      return 7;
   case DDT_GLASS:
      return 8;
   case DDT_CROSSBRACE:
      return 5;
   case DDT_FRAME:
   case DDT_FRAMETB:
   case DDT_FRAMELR:
      return 2;
   case DDT_PANEL:
      return 1;
   case DDT_DIVIDEMULTIH:
   case DDT_DIVIDEMULTIV:
   case DDT_DIVIDEFIXEDH:
   case DDT_DIVIDEFIXEDV:
      return 3;
   case DDT_SOLID:
      return 2;
   case DDT_VOID:
      return 0;
   }
   return 0;   // default
}

/**********************************************************************************
CDoorDiv::DivNum - Retuns the number of division objects based on the 
division's m_dwType.
*/
DWORD CDoorDiv::DivNum(void)
{
   switch (m_dwType) {
   case DDT_FRAME:
   case DDT_FRAMETB:
   case DDT_FRAMELR:
   case DDT_DIVIDEMULTIH:
   case DDT_DIVIDEMULTIV:
   case DDT_CUTOUT:
   case DDT_CROSSBRACE:
      return 1;
   case DDT_DIVIDEFIXEDH:
   case DDT_DIVIDEFIXEDV:
      return 2;
   case DDT_SOLID:
   case DDT_VOID:
   case DDT_PANEL:
   case DDT_GLASS:
      return 0;
   }
   return 0;   // default
}

/**********************************************************************************
CDoorDiv::CommitPolygon - Given a list of points, commits it as a polygon.

inputs
   DWORD       dwNum - Number of points
   PCPoint     papShape - Shape
   PCDoor      pDoor - Door
   fp      fThickness - How thick. If 0 then only use dwColorFront
   DWORD       dwColorFront - Color ID to use
   DWORD       dwColorBack - Color on the back
   DWORD       dwFlags - Or this flags in
returns
   BOOL - TRUE if success
*/
BOOL CDoorDiv::CommitPolygon (DWORD dwNum, PCPoint papShape, PCDoor pDoor, fp fThickness,
                              DWORD dwColorFront, DWORD dwColorBack, DWORD dwFlags)
{
   // create two panels
   RENDELEM re;
   DWORD i, j;
   memset (&re, 0, sizeof(re));

   for (i = 0; i < 2; i++) {
      if (!fThickness && i)
         break;

      //re.dwSurface = i ? DSURF_INTPANEL : DSURF_EXTPANEL;
      re.dwSurface = i ? dwColorBack : dwColorFront;
      re.pPoly = new CListFixed;
      if (!re.pPoly)
         continue;
      re.pPoly->Init (sizeof(CPoint), papShape, dwNum);
      RemoveLinear (re.pPoly, TRUE);
      re.dwFlags = (i ? REF_FLIPTEXT : 0) | dwFlags;
      pDoor->m_lRENDELEM.Add (&re);
      
      DWORD dw;
      PCPoint p;
      dw = re.pPoly->Num();
      p = (PCPoint) re.pPoly->Get(0);
      for (j = 0; j < dw; j++) {
         p[j].p[1] = fThickness / 2 * (i ? 1 : -1);
      }

      // reverse
      if (i) {
         CPoint pt;
         for (j = 0; j < dw/2; j++) {
            pt.Copy (&p[j]);
            p[j].Copy (&p[dw-j-1]);
            p[dw-j-1].Copy (&pt);
         }
      }

   }
   return TRUE;
}

// BUGFIX - Put this in because in release mode the oval cutouts from doors
// were getting partially cut off. Was also causing problems with bevelling on doors
// #pragma optimize ("", off)

/**********************************************************************************
DoorZipper - Does zipper for some of the door functions (like the cuoutouts)

inputs
   PCPoint     pInner - Inner points
   DWORD       dwNumInner - Number of inner points
   PCPoint     pOuter - Outer points
   DWORD       dwNumOuter - Number of outer points
   BOOL        fFrontNeg - If TRUE, the normal vector is positive - away from user. FALSE, positive
   PCRenderSurface      *prs - Render to.
returns
   none
*/
void DoorZipper (PCPoint pInner, DWORD dwNumInner, PCPoint pOuter, DWORD dwNumOuter,
                 BOOL fFrontNeg, PCRenderSurface prs)
{

   // find the center of the cutout
   CPoint pMin, pMax, pCenterCut;
   DWORD i;
   for (i = 0; i < dwNumInner; i++) {
      if (i) {
         pMin.Min (&pInner[i]);
         pMax.Max (&pInner[i]);
      }
      else {
         pMin.Copy (&pInner[i]);
         pMax.Copy (&pInner[i]);
      }
   }
   pCenterCut.Average (&pMin, &pMax);

   // find min and max for texture
   for (i = 0; i < dwNumOuter; i++) {
      if (i) {
         pMin.Min (&pOuter[i]);
         pMax.Max (&pOuter[i]);
      }
      else {
         pMin.Copy (&pOuter[i]);
         pMax.Copy (&pOuter[i]);
      }
   }

   // new minor ID
   prs->NewIDPart ();

   DWORD dwNum1, dwNum2;
   PCPoint pLine1, pLine2;
   dwNum1 = dwNumInner;
   dwNum2 = dwNumOuter;
   pLine1 = pInner;
   pLine2 = pOuter;
   BOOL fLooped;
   fLooped = TRUE;

   // how many points really?
   DWORD dw1, dw2;
   dw1 = dwNum1;
   dw2 = dwNum2;
   if (fLooped) {
      dw1++;
      dw2++;
   }

   // find out the first point with lowest atan2() (+2PI opt)
   DWORD adwBest[2];
   fp afBest[2];
   fp fTemp;
   DWORD j;
   for (i = 0; i < 2; i++) {
      PCPoint pl;
      adwBest[i] = -1;
      afBest[i] = 1000;
      pl = (i ? pLine2 : pLine1);
      for (j = 0; j < (i ? dwNum2 : dwNum1); j++) {
         fTemp = atan2 (pl[j].p[0] - pCenterCut.p[0], pl[j].p[2] - pCenterCut.p[2]) * (fFrontNeg ? 1 : -1);
         if (fTemp < 0)
            fTemp += PI*2;
         if (fTemp < afBest[i]) {
            afBest[i] = fTemp;
            adwBest[i] = j;
         }
      }
   }
   fp fStart1, fStart2;
   fStart1 = afBest[0];
   fStart2 = afBest[1];
   if (fStart1 < 0)
      fStart1 += 2*PI;
   if (fStart2 < 0)
      fStart2 += 2*PI;

   // start at first point on each
   DWORD dwCur1, dwCur2;
   dwCur1 = 0;
   dwCur2 = 0;
   while (TRUE) {
      if ((dwCur1 >= dw1) && (dwCur2 >= dw2))
         break;   // done

      PCPoint p1, p2, p1a, p2b;
      p1 = pLine1 + ((dwCur1+adwBest[0]) % dwNum1);
      p2 = pLine2 + ((dwCur2+adwBest[1]) % dwNum2);
      p1a = pLine1 + ((dwCur1+1+adwBest[0]) % dwNum1);
      p2b = pLine2 + ((dwCur2+1+adwBest[1]) % dwNum2);

      // figure out which triangle to use, one of two choices.
      // both triangles use two points from dwCur1 to dwCur2
      // however, third point can eitehr be (dwCur1+1) (tri A) or (dwCur2+1) (triB).
      // use whichever one has the shortest length
      BOOL  fAOK, fBOK;
      fAOK = fBOK = FALSE;
      //pt.Subtract (&p1->p, &p2->p);
#if 0
      fp fADist, fBDist;
      fADist = fBDist = 0; // should include pt.Length(), but makes no different to final comparison
      CPoint pt, pt2;
      fp fLen;
      if (dwCur1+1 < dw1) {
         fAOK = TRUE;
         pt.Subtract (p1a, p2);
         fADist += pt.Length();
         pt.Subtract (p1a, p1);
         fLen = pt.Length();
      }
      if (dwCur2+1 < dw2) {
         fBOK = TRUE;
         pt.Subtract (p2b, p1);
         fBDist += pt.Length();
         pt.Subtract (p2b, p2);
         fLen = pt.Length();
      }
#endif

      // use the atan2 to figure out the distance
      // just make sure to keep going around in a circle

      fp f1a, f2b;
      if (dwCur1+1 < dw1) {
         fAOK = TRUE;

         f1a = atan2 (p1a->p[0] - pCenterCut.p[0], p1a->p[2] - pCenterCut.p[2]) * (fFrontNeg ? 1 : -1);
         while (f1a <= fStart1)
            f1a += PI * 2;
      }
      if (dwCur2+1 < dw2) {
         fBOK = TRUE;

         f2b = atan2 (p2b->p[0] - pCenterCut.p[0], p2b->p[2] - pCenterCut.p[2]) * (fFrontNeg ? 1 : -1);
         while (f2b <= fStart2)
            f2b += PI * 2;
      }

      // take whichever triangle has the smallest perimiter
      BOOL fTakeA;
retryother:
      if (fAOK && fBOK)
         fTakeA = (f2b > f1a);
      else if (!fAOK && !fBOK)
         return;  // done
      else
         fTakeA = fAOK;

      // add the triangle
      PCPoint     pv1, pv2, pv3;
      pv1 = fTakeA ? p1a : p2b;
      pv2 = p1;
      pv3 = p2;


      // find the normal for this
      CPoint pNormal;
      pNormal.MakeANormal (pv1, pv2, pv3, TRUE);
      if ((pNormal.p[1] < 0) != fFrontNeg) {
         // have a problem since pointing the wrong way, so cant use this
         if (fTakeA)
            fAOK = FALSE;
         else
            fBOK = FALSE;
         goto retryother;
      }

      DWORD dwNormal;
      dwNormal = prs->NewNormal (pv1, pv2, pv3);

      // rotate
      DWORD dwTextures;
      PTEXTPOINT5 ptp;
      ptp = prs->NewTextures (&dwTextures, 3);
      if (ptp) {
         ptp[0].hv[0] = pv1->p[0];
         ptp[0].hv[1] = pv1->p[2];
         ptp[1].hv[0] = pv2->p[0];
         ptp[1].hv[1] = pv2->p[2];
         ptp[2].hv[0] = pv3->p[0];
         ptp[2].hv[1] = pv3->p[2];
         for (j = 0; j < 3; j++) {
            ptp[j].hv[1] -= pMin.p[2];
            ptp[j].hv[1] *= -1;   // since reverse

            if (!fFrontNeg)
               ptp[j].hv[0] = pMax.p[0] - ptp[j].hv[0];
            else
               ptp[j].hv[0] -= pMin.p[0];
         }

         // and the xyz
         for (j = 0; j < 3; j++) {
            ptp[0].xyz[j] = pv1->p[j];
            ptp[1].xyz[j] = pv2->p[j];
            ptp[2].xyz[j] = pv3->p[j];
         }

         prs->ApplyTextureRotation (ptp, 3);
      }

      prs->NewTriangle (
         prs->NewVertex (prs->NewPoint(pv1->p[0], pv1->p[1], pv1->p[2]), dwNormal, ptp ? (dwTextures+0) : 0),
         prs->NewVertex (prs->NewPoint(pv2->p[0], pv2->p[1], pv2->p[2]), dwNormal, ptp ? (dwTextures+1) : 0),
         prs->NewVertex (prs->NewPoint(pv3->p[0], pv3->p[1], pv3->p[2]), dwNormal, ptp ? (dwTextures+2) : 0),
         TRUE
         );
      // know that cant ever have both being false because tested that earlier

      if (fTakeA) {
         dwCur1++;   // advance
      }
      else {
         dwCur2++;   // advance
      }
   }

}
// #pragma optimize ("", on)

/**********************************************************************************
IntersectLineWithLines - Intersect a 2D line (using X and Z) with a set of lines
in a loop (dwNum and papShape), X & Z.

inputs
   PCPoint     paLine - Two points of a line. These may be modified
   DWORD       dwNum - Number of points in looped boundary
   PCPoint     papShape - Points in boundary
returns
   DWORD - 0 if line is completely out of shape, 1 if line was partially in and
            paLine is modified (only keeping one segment) and 2 if the line was entirely in.
*/
DWORD IntersectLineWithLines (PCPoint paLine, DWORD dwNum, PCPoint papShape)
{
   // treat paLine is aplane, and the other bits as lines.
   CPoint pPlane, pLineDir, pUp, pPlaneNormal;
   pPlane.Copy (&paLine[0]);
   pLineDir.Subtract (paLine+1, paLine+0);
   pLineDir.p[1] = 0;
   if (pLineDir.Length() < EPSILON)
      pLineDir.p[0] = CLOSE;  // just to have something
   pUp.Zero();
   pUp.p[1] = 1;
   pPlaneNormal.CrossProd (&pLineDir, &pUp);

   // remember min and max
   CPoint pMin, pMax;
   BOOL fMinMaxValid;
   fMinMaxValid = FALSE;

   // find all the points in papShape that intersect with the plane
   DWORD i;
   CPoint pPQ;
   CPoint   pSub, pIntersect;
   fp fAlpha;
   for (i = 0; i < dwNum; i++) {
      PCPoint pLineStart = papShape + i;
      PCPoint pLineEnd = papShape + ((i+1)%dwNum);

      // find the longest dimension between pNC and pC
      pSub.Subtract (pLineEnd, pLineStart);

      // find out where it crosses the plane on the dwLongest dimension
      // solve alpha = ((P-Q).v) / (w.v), P=point on plalne, Q=pLineStart, w=pLineEnd-pLineStart, v=Plane normal
      pPQ.Subtract (&pPlane, pLineStart);
      fAlpha = pSub.DotProd (&pPlaneNormal);
      if (fabs(fAlpha) < EPSILON)
         continue;  // doens't intersect
      fAlpha = pPQ.DotProd(&pPlaneNormal) / fAlpha;

      // if beyond the start/end (allow for error) then miss
      if ((fAlpha < -EPSILON) || (fAlpha > 1+EPSILON))
         continue;

      pIntersect.Average (pLineStart, pLineEnd, 1.0 - fAlpha);
      if (fMinMaxValid) {
         pMin.Min (&pIntersect);
         pMax.Max (&pIntersect);
      }
      else {
         fMinMaxValid = TRUE;
         pMin.Copy (&pIntersect);
         pMax.Copy (&pIntersect);
      }
   }


   // if nothing is valid then never intersected
   if (!fMinMaxValid)
      return 0;

   // because this is a line, and just do some min and max
   DWORD dwDimMax;
   dwDimMax = (fabs(pLineDir.p[0]) > fabs(pLineDir.p[2])) ? 0 : 2;
   fp fDir;
   fDir = pLineDir.p[dwDimMax];
   if (fabs(fDir) < EPSILON)
      fDir = EPSILON;
   fp fAlpha1, fAlpha2;
   fAlpha1 = 0;
   fAlpha2 = 1;
   
   // intersection
   PCPoint pp;
   fp afAlpha[2];
   afAlpha[0] = 0;
   afAlpha[1] = 1;
   // oriented the same way
   BOOL fSameOrient;
   fSameOrient = ((pMax.p[dwDimMax] - pMin.p[dwDimMax]) * fDir) >= 0;
   for (i = 0; i < 2; i++) {
      if (fSameOrient)
         pp = i ? &pMax : &pMin;
      else
         pp = i ? &pMin : &pMax;
      fAlpha = (pp->p[dwDimMax] - paLine[0].p[dwDimMax]) / fDir;

      if (i)
         afAlpha[1] = min(afAlpha[1], fAlpha);
      else
         afAlpha[0] = max(afAlpha[0], fAlpha);
   }
   if (afAlpha[0] >= afAlpha[1])
      return 0;   // shouldn't happen, but intersected out
   if ((afAlpha[0] == 0) && (afAlpha[1] == 1))
      return 2;   // no change
   
   CPoint apt[2];
   memcpy (apt, paLine, sizeof(apt));
   paLine[0].Average (&apt[0], &apt[1], 1.0 - afAlpha[0]);
   paLine[1].Average (&apt[0], &apt[1], 1.0 - afAlpha[1]);
   return 1;
}

/**********************************************************************************
IntersectPathWithBorder - Interesect a path (CListFixed of points) with a border
(dwNum and papPoints) and modified the path to reflect the new intersection.

For longer paths, if there's a break in the path (clipped into two or more paths)
then will insert an x=0,y=0,z=0,W=0 point.

inputs
   PCListFixed       plPath - Path. Initialized to list of CPoint. Only X and Z used.
                     This is modified with a new path, or cleared if no intersection
   BOOL              *pfLooped - Should initially be filled with flag indicating if the
                     path is looped. Gets filled with a flag indicating if intersection is looped
   DWORD             dwNum - Number of points in th elooped border
   PCPoint           papPoints - Points in the border.
*/
void IntersectPathWithBorder (PCListFixed plPath, BOOL *pfLooped, DWORD dwNum, PCPoint papPoints)
{
   PCPoint pPath = (PCPoint) plPath->Get(0);
   DWORD dwNumPath = plPath->Num();

   DWORD dwRet;
   if (dwNumPath == 2) {
      dwRet = IntersectLineWithLines (pPath, dwNum, papPoints);
      if (!dwRet)
         plPath->Clear();
      // else if modify OK since it's in situ
      return;
   }

   // create a new list
   CListFixed lNew;
   lNew.Init (sizeof(CPoint));
   CPoint apt[2], aptSafe[2];
   DWORD i;
   BOOL fPutInSplits;
   fPutInSplits = FALSE;
   for (i = 0; i < dwNumPath; i++) {
      if (!(*pfLooped) && (i+1 >= dwNumPath))
         continue;

      // get these two points
      apt[0].Copy (&pPath[i]);
      apt[1].Copy (&pPath[(i+1)%dwNumPath]);
      memcpy (aptSafe, apt, sizeof(apt));

      dwRet = IntersectLineWithLines (apt, dwNum, papPoints);
      if (!dwRet)
         continue;   // dont add
      apt[0].p[3] = apt[1].p[3] = 1;   // so dont mistake break points

      // if the starting point changed then may need to note an insertion point
      if (!aptSafe[0].AreClose (&apt[0]) || !(lNew.Num()) ) {
         if (lNew.Num()) {
            CPoint pZero;
            pZero.Zero4();
            lNew.Add (&pZero);
            fPutInSplits = TRUE;
         }

         // add the point
         lNew.Add (&apt[0]);
      }

      // add the second point
      lNew.Add (&apt[1]);
   }

   // if it's marked as looped then see if it really is
   if (*pfLooped && lNew.Num()) {
      PCPoint ps = (PCPoint) lNew.Get(0);
      PCPoint pe = (PCPoint) lNew.Get(lNew.Num()-1);
      if (fPutInSplits)
         *pfLooped = FALSE;
      else if (ps->AreClose (pe)) { // !fPutInSplits
         // the start matches the end, so leave looped flag true but remove last pint
         lNew.Remove (lNew.Num()-1);
      }
      else {
         // the start doesnt match the end, so split at the end
         *pfLooped = FALSE;
      }
   }

   // just as a niceity - if there are splits, look for the last one and see
   // if it should be merged in with the first part
   if (!(*pfLooped) && lNew.Num() && fPutInSplits) {
      PCPoint ps, pe;
      for (i = lNew.Num()-1; i < lNew.Num(); i--) {
         ps = (PCPoint) lNew.Get(i);
         if (!ps->p[0] && !ps->p[1] && !ps->p[2] && !ps->p[3])
            break;
      }
      ps = (PCPoint) lNew.Get(0);
      pe = (PCPoint) lNew.Get(lNew.Num()-1);
      if ((i < lNew.Num()) && ps->AreClose(pe)) {
         // found a last break and it's close, so combine that, therefore repeat,
         // removing from beginngin and placing at end until get an empty.
         while (TRUE) {
            CPoint p;
            p.Copy ((PCPoint)lNew.Get(0));
            lNew.Remove(0);
            if (!p.p[0] && !p.p[1] && !p.p[2] && !p.p[3])
               break;
            lNew.Add (&p);
         }  // moveing breaks to end
      } // if move break
   }

   // copy over
   plPath->Init (sizeof(CPoint), lNew.Get(0), lNew.Num());
   return;
}


/*********************************************************************************
ExpandRemoveOutOfBounds - When shrink a loop around, sometimes have problems
around edges of curve. Can fix this by knowing in what range all the new points
should be and eliminating any not in that range.

inputs
   DWORD       dwOrig - original number of points (before expanding)
   PCPoint     paOrig - Original points - before expanding
   fp      fShrink - Amount that shinrk this. Postive number
   PCListFixed pl - List of points after expanion. Any out of min/max of paOrig-fShrink
               are eliminated
*/
void ExpandRemoveOutOfBounds (DWORD dwOrig, PCPoint paOrig, fp fShrink, PCListFixed pl)
{
   CPoint pMin, pMax;
   DWORD i;
   for (i = 0; i < dwOrig; i++) {
      if (i) {
         pMin.Min (&paOrig[i]);
         pMax.Max (&paOrig[i]);
      }
      else {
         pMin.Copy (&paOrig[i]);
         pMax.Copy (&paOrig[i]);
      }
   }

   pMin.p[0] += fShrink;
   pMin.p[2] += fShrink;
   pMax.p[0] -= fShrink;
   pMax.p[2] -= fShrink;

   for (i = pl->Num()-1; i < pl->Num(); i--) {
      PCPoint p = (PCPoint) pl->Get(i);
      if ((p->p[0] < pMin.p[0] - EPSILON) || (p->p[0] > pMax.p[0] + EPSILON) ||
         (p->p[2] < pMin.p[2] - EPSILON) || (p->p[2] > pMax.p[2] + EPSILON)) {
         pl->Remove (i);
      }
   }

   // if nothing left then do something special
   if (!pl->Num()) {
      CPoint pTop, pBottom;
      pTop.Copy (&pMax);
      pBottom.Copy (&pMin);
      if (pMax.p[0] < pMin.p[0])
         pTop.p[0] = pBottom.p[0] = (pMax.p[0] + pMin.p[0]) / 2;
      if (pMax.p[2] < pMin.p[2])
         pTop.p[2] = pBottom.p[2] = (pMax.p[2] + pMin.p[2]) / 2;

      pl->Add (&pTop);
      pl->Add (&pBottom);
   }
}


/**********************************************************************************
CDoorDiv::Commit - Given a clockwise outline of the door based on dwNum and papShape,
this adds elements to pDoor's m_lRENDERLEM list to draw the division. It also
recurses and gets sub-divisions drawn.

inputs
   DWORD       dwNum - Number of points in papShape
   PCPoint     papShape - Shape
   PCDoor      pDoor - Door to add m_lRENDERELEM elements to
returns
   BOOL - TRUE if success
*/
BOOL CDoorDiv::Commit (DWORD dwNum, PCPoint papShape, PCDoor pDoor)
{
   RENDELEM re;
   DWORD i;
   memset (&re, 0, sizeof(re));

   // find the min and max
   CPoint pMin, pMax;
   for (i = 0; i < dwNum; i++) {
      if (i) {
         pMin.Min (&papShape[i]);
         pMax.Max (&papShape[i]);
      }
      else {
         pMin.Copy (&papShape[i]);
         pMax.Copy (&papShape[i]);
      }
   }

   switch (m_dwType) {
   case DDT_CROSSBRACE:
      {
         DWORD dwDirUp, dwDirRight;
         fp fScaleUp, fScaleRight;
         fScaleUp = fScaleRight = 1;
         switch ((DWORD) m_afParam[4]) {
         case 0:  // 0 deg
         default:
            dwDirUp = 2;
            break;
         case 1: // 90 deg
            dwDirUp = 0;
            fScaleRight = -1;
            break;
         case 2:  // 180 deg
            dwDirUp = 2;
            fScaleUp = -1;
            if (m_afParam[3] == DDCB_MORETHAN)
               fScaleRight = -1; // not quite rotaton but includes flip
            break;
         case 3:  // 270 deg
            dwDirUp = 0;
            if (m_afParam[3] == DDCB_MORETHAN)
               fScaleUp = -1;  // not quite rotation, but includes flip
            break;
         }
         dwDirRight = (dwDirUp == 0) ? 2 : 0;

         re.dwSurface = DSURF_BRACING;
         CPoint ap[2], pScale, pOffset;
         memset (ap, 0, sizeof(ap));
         pScale.Zero();
         pScale.p[0] = m_afParam[0];
         pScale.p[1] = pDoor->m_fThickness * m_afParam[1];
         pOffset.Zero();

         switch ((DWORD)m_afParam[3]) {
         default:
         case DDCB_MORETHAN:
            // more than brace
            fp fExtra, fMin, fMax;
            fMax = max((pMax.p[dwDirUp] - pMin.p[dwDirUp])/2, pMax.p[dwDirRight] - pMin.p[dwDirRight]);
            fMin = min((pMax.p[dwDirUp] - pMin.p[dwDirUp])/2, pMax.p[dwDirRight] - pMin.p[dwDirRight]);
            //fMax -= m_afParam[0]/2;
            fMax = max(.01, fMax);
            fMin = max(.01, fMin);
            fExtra = m_afParam[0] / 2 / cos(atan2(fMax,fMin));

            // top line
            // do the brace
            // brace all the way opposite to the the longest dir
            DWORD dwLongestDir, dwShortestDir;
            if ((pMax.p[dwDirUp] - pMin.p[dwDirUp])/2 > (pMax.p[dwDirRight] - pMin.p[dwDirRight])) {
               dwLongestDir = dwDirUp;
               dwShortestDir = dwDirRight;
            }
            else {
               dwLongestDir = dwDirRight;
               dwShortestDir = dwDirUp;
            }
            re.pNoodle = new CNoodle;
            if (!re.pNoodle)
               break;
            pDoor->m_lRENDELEM.Add (&re);
            ap[0].p[dwDirUp] = (fScaleUp > 0) ? pMax.p[dwDirUp] : pMin.p[dwDirUp];
            ap[0].p[dwDirRight] = (fScaleRight <= 0) ? pMax.p[dwDirRight] : pMin.p[dwDirRight];
            ap[1].p[dwDirUp] = (pMax.p[dwDirUp] + pMin.p[dwDirUp]) / 2;
            ap[1].p[dwDirRight] = (fScaleRight > 0) ? pMax.p[dwDirRight] : pMin.p[dwDirRight];

            CPoint pBevel, pBevelC;
            pBevel.Zero();
            pBevelC.Zero();
            pBevelC.p[2] = 1;
            if (dwLongestDir == dwDirUp) {
               // brace all the way to the right
               ap[0].p[dwDirUp] -= fExtra * fScaleUp;
               ap[1].p[dwDirRight] -= fExtra * fScaleRight;
               pBevel.p[dwDirRight] = 1;
            }
            else {
               // brace all the way to the top
               ap[0].p[dwDirRight] += fExtra * fScaleRight;
               ap[1].p[dwDirRight] -= fExtra * fScaleRight;
               pBevel.p[dwDirUp] = 1;
            }
            re.pNoodle->BevelSet (TRUE, 2, &pBevel);
            re.pNoodle->BevelSet (FALSE, 2, &pBevelC);
            re.pNoodle->PathLinear (&ap[0], &ap[1]);
            re.pNoodle->ShapeDefault (NS_RECTANGLE);
            re.pNoodle->ScaleVector (&pScale);
            pOffset.p[1] = (pDoor->m_fThickness + pScale.p[1]) * (m_afParam[2] ? 1 : -1) / 2.0;
            re.pNoodle->OffsetSet (&pOffset);
            re.pNoodle->DrawEndsSet (TRUE);

            // lower one
            re.pNoodle = new CNoodle;
            if (!re.pNoodle)
               break;
            pDoor->m_lRENDELEM.Add (&re);
            ap[0].p[dwDirUp] = (fScaleUp <= 0) ? pMax.p[dwDirUp] : pMin.p[dwDirUp];
            ap[0].p[dwDirRight] = (fScaleRight <= 0) ? pMax.p[dwDirRight] : pMin.p[dwDirRight];
            ap[1].p[dwDirUp] = (pMax.p[dwDirUp] + pMin.p[dwDirUp]) / 2;
            ap[1].p[dwDirRight] = (fScaleRight > 0) ? pMax.p[dwDirRight] : pMin.p[dwDirRight];

            pBevel.Zero();
            pBevelC.Zero();
            pBevelC.p[2] = 1;
            if (dwLongestDir == dwDirUp) {
               // brace all the way to the right
               ap[0].p[dwDirUp] += fExtra * fScaleUp;
               ap[1].p[dwDirRight] -= fExtra * fScaleRight;
               pBevel.p[dwDirRight] = 1;
            }
            else {
               // brace all the way to the top
               ap[0].p[dwDirRight] += fExtra * fScaleRight;
               ap[1].p[dwDirRight] -= fExtra * fScaleRight;
               pBevel.p[dwDirUp] = 1;
            }
            re.pNoodle->BevelSet (TRUE, 2, &pBevel);
            re.pNoodle->BevelSet (FALSE, 2, &pBevelC);
            re.pNoodle->PathLinear (&ap[0], &ap[1]);
            re.pNoodle->ShapeDefault (NS_RECTANGLE);
            re.pNoodle->ScaleVector (&pScale);
            re.pNoodle->OffsetSet (&pOffset);
            re.pNoodle->DrawEndsSet (TRUE);
            break;

         case DDCB_Z:
            // doing the Z brace
            // top line
            re.pNoodle = new CNoodle;
            if (!re.pNoodle)
               break;
            pDoor->m_lRENDELEM.Add (&re);
            ap[0].p[dwDirRight] = pMin.p[dwDirRight];
            ap[0].p[dwDirUp] = pMax.p[dwDirUp] - m_afParam[0]/2;
            ap[1].p[dwDirRight] = pMax.p[dwDirRight];
            ap[1].p[dwDirUp] = ap[0].p[dwDirUp];
            re.pNoodle->PathLinear (&ap[0], &ap[1]);
            re.pNoodle->ShapeDefault (NS_RECTANGLE);
            re.pNoodle->ScaleVector (&pScale);
            pOffset.p[1] = (pDoor->m_fThickness + pScale.p[1]) * (m_afParam[2] ? 1 : -1) / 2.0;
            re.pNoodle->OffsetSet (&pOffset);
            re.pNoodle->DrawEndsSet (TRUE);
            pMax.p[dwDirUp] -= m_afParam[0]; // so use for brace

            // bottom line
            re.pNoodle = new CNoodle;
            if (!re.pNoodle)
               break;
            pDoor->m_lRENDELEM.Add (&re);
            ap[0].p[dwDirRight] = pMin.p[dwDirRight];
            ap[0].p[dwDirUp] = pMin.p[dwDirUp] + m_afParam[0]/2;
            ap[1].p[dwDirRight] = pMax.p[dwDirRight];
            ap[1].p[dwDirUp] = ap[0].p[dwDirUp];
            re.pNoodle->PathLinear (&ap[0], &ap[1]);
            re.pNoodle->ShapeDefault (NS_RECTANGLE);
            re.pNoodle->ScaleVector (&pScale);
            pOffset.p[1] = (pDoor->m_fThickness + pScale.p[1]) * (m_afParam[2] ? 1 : -1) / 2.0;
            re.pNoodle->OffsetSet (&pOffset);
            re.pNoodle->DrawEndsSet (TRUE);
            pMin.p[dwDirUp] += m_afParam[0]; // so use for brace
            
            // fall throught to diagonal
         case DDCB_SLASH:

            // do the brace
            // brace all the way opposite to the the longest dir
            //DWORD dwLongestDir, dwShortestDir;
            if ((pMax.p[dwDirUp] - pMin.p[dwDirUp]) > (pMax.p[dwDirRight] - pMin.p[dwDirRight])) {
               dwLongestDir = dwDirUp;
               dwShortestDir = dwDirRight;
            }
            else {
               dwLongestDir = dwDirRight;
               dwShortestDir = dwDirUp;
            }
            re.pNoodle = new CNoodle;
            if (!re.pNoodle)
               break;
            pDoor->m_lRENDELEM.Add (&re);
            //fp fExtra, fMin, fMax;
            fMax = max(pMax.p[0] - pMin.p[0], pMax.p[2] - pMin.p[2]);
            fMin = min(pMax.p[0] - pMin.p[0], pMax.p[2] - pMin.p[2]);
            fMax -= m_afParam[0];
            fMax = max(.01, fMax);
            fMin = max(.01, fMin);
            fExtra = m_afParam[0] / 2 / cos(atan2(fMax,fMin));

            ap[0].p[dwDirUp] = (fScaleUp > 0) ? pMax.p[dwDirUp] : pMin.p[dwDirUp];
            ap[0].p[dwDirRight] = (fScaleRight > 0) ? pMax.p[dwDirRight] : pMin.p[dwDirRight];
            ap[1].p[dwDirUp] = (fScaleUp <= 0) ? pMax.p[dwDirUp] : pMin.p[dwDirUp];
            ap[1].p[dwDirRight] = (fScaleRight <= 0) ? pMax.p[dwDirRight] : pMin.p[dwDirRight];

            //CPoint pBevel;
            pBevel.Zero();
            if (dwLongestDir == dwDirUp) {
               // brace all the way to the right
               ap[0].p[dwDirUp] -= fExtra * fScaleUp;
               ap[1].p[dwDirUp] += fExtra * fScaleUp;
               pBevel.p[dwDirRight] = 1;
            }
            else {
               // brace all the way to the top
               ap[0].p[dwDirRight] -= fExtra * fScaleRight;
               ap[1].p[dwDirRight] += fExtra * fScaleRight;
               pBevel.p[dwDirUp] = 1;
            }
            re.pNoodle->BevelSet (TRUE, 2, &pBevel);
            re.pNoodle->BevelSet (FALSE, 2, &pBevel);
            re.pNoodle->PathLinear (&ap[0], &ap[1]);
            re.pNoodle->ShapeDefault (NS_RECTANGLE);
            re.pNoodle->ScaleVector (&pScale);
            pOffset.p[1] = (pDoor->m_fThickness + pScale.p[1]) * (m_afParam[2] ? 1 : -1) / 2.0;
            re.pNoodle->OffsetSet (&pOffset);
            re.pNoodle->DrawEndsSet (TRUE);
            break;
         }


         // draw the contents
         if (m_apDiv[0])
            m_apDiv[0]->Commit (dwNum, papShape, pDoor);
      }
      break;

   case DDT_GLASS:
      {
         // glass element
         if (m_afParam[0])
            CommitPolygon (dwNum, papShape, pDoor, 0, (DWORD)m_afParam[0], (DWORD)m_afParam[0]);

         // figure out mullion distX and distZ
         fp fWidth, fHeight;
         DWORD dwNumX, dwNumZ;
         fWidth = max(m_afParam[3], .01);
         fHeight = max(m_afParam[4], .01);
         dwNumX = (DWORD)ceil((pMax.p[0] - pMin.p[0]) / fWidth);
         dwNumZ = (DWORD)ceil((pMax.p[2] - pMin.p[2]) / fHeight);
         dwNumX = max(1,dwNumX);
         dwNumZ = max(1,dwNumZ);
         fWidth = (pMax.p[0] - pMin.p[0]) / (fp) dwNumX;
         fHeight = (pMax.p[2] - pMin.p[2]) / (fp) dwNumZ;

         fp fThick;
         CListFixed lLine;
         lLine.Init (sizeof(CPoint));
         CPoint p;
         DWORD dwDir;
         fThick = pDoor->m_fThickness / 2 * .99;   // just slightly smaller than frame
         CPoint pScale, pOffset;
         pScale.Zero();
         pScale.p[0] = m_afParam[2];
         pScale.p[1] = fThick;
         pOffset.Zero();

         switch ((DWORD)m_afParam[1]) {
         case DDMS_NONE:
            // don nothing
            break;

         case DDMS_RADIATE:   // radiated pattern
            {
               // find the center
               CPoint pCenter;
               pCenter.Zero();
               pCenter.p[0] = (pMax.p[0] - pMin.p[0]) * m_afParam[5] + pMin.p[0];
               pCenter.p[2] = (pMax.p[2] - pMin.p[2]) * m_afParam[6] + pMin.p[2];

               // radius and angles
               fp fRadius, fAngle;
               fRadius = max(m_afParam[3], .01);
               dwNumX = (DWORD)ceil(max(pMax.p[0] - pMin.p[0], pMax.p[2] - pMin.p[2]) / fRadius);
               dwNumX = max(1,dwNumX);

               // loops
               fp fMaxRadius;
               fMaxRadius = fRadius / 2;
               for (i = 0; i < dwNumX; i++) {
                  // radius to use
                  fp fUse = fRadius / 2 + fRadius * i;

                  // circle
                  DWORD dwNumPoint;
                  lLine.Clear();
                  dwNumPoint = 32;  // seems like a reasonable #
                  DWORD j;
                  for (j = 0; j < dwNumPoint; j++) {
                     p.Copy (&pCenter);
                     p.p[0] += sin((fp)j / (fp)dwNumPoint * 2 * PI) * fUse;
                     p.p[2] += cos((fp)j / (fp)dwNumPoint * 2 * PI) * fUse;
                     lLine.Add (&p);
                  }

                  BOOL fLooped;
                  fLooped = TRUE;
                  IntersectPathWithBorder (&lLine, &fLooped, dwNum, papShape);
                  if (lLine.Num() < 2)
                     continue;   // clipped

                  fMaxRadius = max(fMaxRadius, fUse);

                  // draw portion
                  DWORD dwStart, dwEnd;
                  for (dwStart = 0; dwStart+1 < lLine.Num(); dwStart = dwEnd+1) {
                     for (dwEnd = dwStart+1; dwEnd < lLine.Num(); dwEnd++) {
                        PCPoint pt = (PCPoint) lLine.Get(dwEnd);
                        if (!pt->p[0] && !pt->p[1] && !pt->p[2] && !pt->p[3])
                           break;
                     }

                     if (dwStart || (dwEnd != lLine.Num()))
                        fLooped = FALSE;

                     // mullion in front
                     re.pNoodle = new CNoodle;
                     re.dwSurface = DSurfMassage((DWORD)m_afParam[7], TRUE);
                     if (!re.pNoodle)
                        continue;
                     pDoor->m_lRENDELEM.Add (&re);
                     re.pNoodle->PathSpline (fLooped, dwEnd - dwStart, ((PCPoint)lLine.Get(0)) + dwStart, (DWORD*)SEGCURVE_LINEAR, 0);
                     re.pNoodle->ShapeDefault (NS_RECTANGLE);
                     re.pNoodle->ScaleVector (&pScale);
                     pOffset.p[1] = -fThick/2;
                     re.pNoodle->OffsetSet (&pOffset);

                     // mullion in back
                     re.pNoodle = new CNoodle;
                     re.dwSurface = DSurfMassage((DWORD)m_afParam[7], FALSE);
                     if (!re.pNoodle)
                        continue;
                     pDoor->m_lRENDELEM.Add (&re);
                     re.pNoodle->PathSpline (fLooped, dwEnd - dwStart, ((PCPoint)lLine.Get(0)) + dwStart, (DWORD*)SEGCURVE_LINEAR, 0);
                     re.pNoodle->ShapeDefault (NS_RECTANGLE);
                     re.pNoodle->ScaleVector (&pScale);
                     pOffset.p[1] = fThick/2;
                     re.pNoodle->OffsetSet (&pOffset);

                  }  // while dwStart
               }

               // radiate out
               fAngle = max(m_afParam[4], .01);
               dwNumZ = (DWORD)ceil(2 * PI * fMaxRadius / fAngle);
               dwNumZ = ((dwNumZ + 3) / 4) * 4; // multiple of 4
               dwNumZ = max(1,dwNumZ);
               fAngle = 2 * PI / (fp) dwNumZ;

               for (i = 0; i < dwNumZ; i++) {
                  lLine.Clear();
                  p.Copy (&pCenter);
                  p.p[0] += sin(fAngle * (fp)i) * fRadius/2;
                  p.p[2] += cos(fAngle * (fp)i) * fRadius/2;
                  lLine.Add (&p);
                  p.p[0] += sin(fAngle * (fp)i) * fRadius * 1000; // just large #
                  p.p[2] += cos(fAngle * (fp)i) * fRadius * 1000; // just large #
                  lLine.Add (&p);

                  BOOL fLooped;
                  fLooped = FALSE;
                  IntersectPathWithBorder (&lLine, &fLooped, dwNum, papShape);
                  if (lLine.Num() < 2)
                     continue;   // clipped

                  // mullion in front
                  re.pNoodle = new CNoodle;
                  re.dwSurface = DSurfMassage((DWORD)m_afParam[7], TRUE);
                  if (!re.pNoodle)
                     continue;
                  pDoor->m_lRENDELEM.Add (&re);
                  re.pNoodle->PathSpline (fLooped, lLine.Num(), (PCPoint)lLine.Get(0), (DWORD*)SEGCURVE_LINEAR, 0);
                  re.pNoodle->ShapeDefault (NS_RECTANGLE);
                  re.pNoodle->ScaleVector (&pScale);
                  pOffset.p[1] = -fThick/2;
                  re.pNoodle->OffsetSet (&pOffset);

                  // mullion in back
                  re.pNoodle = new CNoodle;
                  re.dwSurface = DSurfMassage((DWORD)m_afParam[7], FALSE);
                  if (!re.pNoodle)
                     continue;
                  pDoor->m_lRENDELEM.Add (&re);
                  re.pNoodle->PathSpline (fLooped, lLine.Num(), (PCPoint)lLine.Get(0), (DWORD*)SEGCURVE_LINEAR, 0);
                  re.pNoodle->ShapeDefault (NS_RECTANGLE);
                  re.pNoodle->ScaleVector (&pScale);
                  pOffset.p[1] = fThick/2;
                  re.pNoodle->OffsetSet (&pOffset);
               }
            }
            break;

         case DDMS_DIAMOND:   // diamond pattern
            {
               // max sure to go well beyond so entire diamond is covered
               int   iMin, iMax, i;
               iMin = -(int)dwNumZ;
               iMax = (int)(dwNumX + dwNumZ);

               for (dwDir = 0; dwDir < 2; dwDir++) {
                  for (i = iMin; i < iMax; i++) {   // intentionally start at 1
                     lLine.Clear();
                     p.Zero();
                     fp fCur, fCenter;
                     fCur = (fp)i * fWidth + pMin.p[0];
                     fCenter = (pMax.p[2] + pMax.p[2]) / 2;
                     fp fDeltaX, fDeltaZ;
                     fDeltaX = fWidth * 10* (fp)(dwNumX + dwNumZ);   // just to make sure long enough
                     fDeltaZ = fHeight * 10 * (fp)(dwNumX + dwNumZ);   // just to make sure long enough

                     // angling up/down
                     p.p[0] = fCur + fDeltaX;
                     p.p[2] = fCenter + fDeltaZ * (dwDir ? 1 : -1);
                     lLine.Add (&p);
                     p.p[0] = fCur - fDeltaX;
                     p.p[2] = fCenter - fDeltaZ * (dwDir ? 1 : -1);
                     lLine.Add (&p);

                     BOOL fLooped;
                     fLooped = FALSE;
                     IntersectPathWithBorder (&lLine, &fLooped, dwNum, papShape);
                     if (lLine.Num() < 2)
                        continue;   // clipped

                     // mullion in front
                     re.pNoodle = new CNoodle;
                     re.dwSurface = DSurfMassage((DWORD)m_afParam[7], TRUE);
                     if (!re.pNoodle)
                        continue;
                     pDoor->m_lRENDELEM.Add (&re);
                     re.pNoodle->PathSpline (fLooped, lLine.Num(), (PCPoint)lLine.Get(0), (DWORD*)SEGCURVE_LINEAR, 0);
                     re.pNoodle->ShapeDefault (NS_RECTANGLE);
                     re.pNoodle->ScaleVector (&pScale);
                     pOffset.p[1] = -fThick/2;
                     re.pNoodle->OffsetSet (&pOffset);

                     // mullion in back
                     re.pNoodle = new CNoodle;
                     re.dwSurface = DSurfMassage((DWORD)m_afParam[7], FALSE);
                     if (!re.pNoodle)
                        continue;
                     pDoor->m_lRENDELEM.Add (&re);
                     re.pNoodle->PathSpline (fLooped, lLine.Num(), (PCPoint)lLine.Get(0), (DWORD*)SEGCURVE_LINEAR, 0);
                     re.pNoodle->ShapeDefault (NS_RECTANGLE);
                     re.pNoodle->ScaleVector (&pScale);
                     pOffset.p[1] = fThick/2;
                     re.pNoodle->OffsetSet (&pOffset);
                  }
               }
            }
            break;

         case DDMS_HV:  // horizontal and vertical
            // across and down
            for (dwDir = 0; dwDir < 2; dwDir++) {
               for (i = 1; i < (dwDir ? dwNumX : dwNumZ); i++) {   // intentionally start at 1
                  lLine.Clear();
                  p.Zero();
                  if (dwDir) {   // over dwNumX
                     p.p[2] = pMin.p[2];
                     p.p[0] = pMin.p[0] + (fp)i*fWidth;
                     lLine.Add (&p);
                     p.p[2] = pMax.p[2];
                     lLine.Add (&p);
                  }
                  else {   // over dwNumZ
                     p.p[0] = pMin.p[0];
                     p.p[2] = pMin.p[2] + (fp)i*fHeight;
                     lLine.Add (&p);
                     p.p[0] = pMax.p[0];
                     lLine.Add (&p);
                  }

                  BOOL fLooped;
                  fLooped = FALSE;
                  IntersectPathWithBorder (&lLine, &fLooped, dwNum, papShape);
                  if (lLine.Num() < 2)
                     continue;   // clipped

                  // mullion in front
                  re.pNoodle = new CNoodle;
                  re.dwSurface = DSurfMassage((DWORD)m_afParam[7], TRUE);
                  if (!re.pNoodle)
                     continue;
                  pDoor->m_lRENDELEM.Add (&re);
                  re.pNoodle->PathSpline (fLooped, lLine.Num(), (PCPoint)lLine.Get(0), (DWORD*)SEGCURVE_LINEAR, 0);
                  re.pNoodle->ShapeDefault (NS_RECTANGLE);
                  re.pNoodle->ScaleVector (&pScale);
                  pOffset.p[1] = -fThick/2;
                  re.pNoodle->OffsetSet (&pOffset);

                  // mullion in back
                  re.pNoodle = new CNoodle;
                  re.dwSurface = DSurfMassage((DWORD)m_afParam[7], FALSE);
                  if (!re.pNoodle)
                     continue;
                  pDoor->m_lRENDELEM.Add (&re);
                  re.pNoodle->PathSpline (fLooped, lLine.Num(), (PCPoint)lLine.Get(0), (DWORD*)SEGCURVE_LINEAR, 0);
                  re.pNoodle->ShapeDefault (NS_RECTANGLE);
                  re.pNoodle->ScaleVector (&pScale);
                  pOffset.p[1] = fThick/2;
                  re.pNoodle->OffsetSet (&pOffset);
               }
            }
            break;
         }  // switch
      }
      return TRUE;

   case DDT_DIVIDEFIXEDH:
   case DDT_DIVIDEFIXEDV:
      {
         DWORD dwDir  = (m_dwType == DDT_DIVIDEFIXEDH) ? 2 : 0;
         DWORD dwFlags = (m_dwType == DDT_DIVIDEFIXEDH) ? REF_ROTTEXT : 0;

         // how many divisions?
         DWORD dwDivisions = (DWORD) m_afParam[1];
         dwDivisions = max(1,dwDivisions);   // at least one division

         // width
         fp fWidth;
         fWidth = pMax.p[dwDir] - pMin.p[dwDir];

         // if the height < 2x frame size then make it solid
         if ((!m_afParam[0]) || (fWidth < m_afParam[0])) {
            return CommitPolygon (dwNum, papShape, pDoor, m_afParam[0] * pDoor->m_fThickness,
               DSurfMassage((DWORD)m_afParam[2], TRUE), DSurfMassage((DWORD)m_afParam[2], FALSE), dwFlags);
         }

         // space for each opening (on the left/bottom)
         fp fOpenSpace;
         fOpenSpace = fWidth * m_afParam[1] - m_afParam[0]/2;
         fOpenSpace = max(.01, fOpenSpace);
         fOpenSpace = min(fWidth - m_afParam[0], fOpenSpace);

         // do the framing piece
         CListFixed l;
         fp fCur;
         fCur = pMin.p[dwDir];
         l.Init (sizeof(CPoint), papShape, dwNum);

         // skip over division
         fCur += fOpenSpace;

         // get rid of to left
         ::ClipPolygons (&l, dwDir, fCur, FALSE);

         // get right of right
         fCur += m_afParam[0];
         ::ClipPolygons (&l, dwDir, fCur, TRUE);

         RemoveLinear (&l, TRUE);
         if (l.Num())
            CommitPolygon (l.Num(), (PCPoint) l.Get(0), pDoor, pDoor->m_fThickness,
               DSurfMassage((DWORD)m_afParam[2], TRUE), DSurfMassage((DWORD)m_afParam[2], FALSE), dwFlags);

         // and the bits in-between
         fCur = pMin.p[dwDir];
         for (i = 0; i < 2; i++) {
            l.Init (sizeof(CPoint), papShape, dwNum);

            // get rid of to left
            if (i)
               ::ClipPolygons (&l, dwDir, fCur, FALSE);

            // get right of right
            if (!i) {
               fCur += fOpenSpace;
               ::ClipPolygons (&l, dwDir, fCur, TRUE);
            }

            fCur += m_afParam[0];

            RemoveLinear (&l, TRUE);
            if (l.Num() && m_apDiv[i])
               m_apDiv[i]->Commit (l.Num(), (PCPoint)l.Get(0), pDoor);
         }

      }
      return TRUE;


   case DDT_DIVIDEMULTIH:
   case DDT_DIVIDEMULTIV:
      {
         DWORD dwDir  = (m_dwType == DDT_DIVIDEMULTIH) ? 2 : 0;
         DWORD dwFlags = (m_dwType == DDT_DIVIDEMULTIH) ? REF_ROTTEXT : 0;

         // how many divisions?
         DWORD dwDivisions = (DWORD) m_afParam[1];
         dwDivisions = max(1,dwDivisions);   // at least one division

         // width
         fp fWidth;
         fWidth = pMax.p[dwDir] - pMin.p[dwDir];

         // if the height < 2x frame size then make it solid
         if ((!m_afParam[0]) || (fWidth < (fp)dwDivisions * m_afParam[0])) {
            return CommitPolygon (dwNum, papShape, pDoor, m_afParam[0] * pDoor->m_fThickness,
               DSurfMassage((DWORD)m_afParam[2], TRUE), DSurfMassage((DWORD)m_afParam[2], FALSE), dwFlags);
         }

         // space for each opening
         fp fOpenSpace;
         fOpenSpace = (fWidth - (fp)dwDivisions * m_afParam[0]) / (fp)(dwDivisions+1);

         // do all the framing pieces
         CListFixed l;
         fp fCur;
         fCur = pMin.p[dwDir];
         for (i = 0; i < dwDivisions; i++) {
            l.Init (sizeof(CPoint), papShape, dwNum);

            // skip over division
            fCur += fOpenSpace;

            // get rid of to left
            ::ClipPolygons (&l, dwDir, fCur, FALSE);

            // get right of right
            fCur += m_afParam[0];
            ::ClipPolygons (&l, dwDir, fCur, TRUE);

            RemoveLinear (&l, TRUE);
            if (l.Num())
               CommitPolygon (l.Num(), (PCPoint) l.Get(0), pDoor, pDoor->m_fThickness,
                  DSurfMassage((DWORD)m_afParam[2], TRUE), DSurfMassage((DWORD)m_afParam[2], FALSE), dwFlags);
         }

         // and the bits in-between
         fCur = pMin.p[dwDir];
         if (m_apDiv[0]) for (i = 0; i <= dwDivisions; i++) {
            l.Init (sizeof(CPoint), papShape, dwNum);

            // get rid of to left
            ::ClipPolygons (&l, dwDir, fCur, FALSE);

            // get right of right
            fCur += fOpenSpace;
            ::ClipPolygons (&l, dwDir, fCur, TRUE);

            fCur += m_afParam[0];

            RemoveLinear (&l, TRUE);
            if (l.Num())
               m_apDiv[0]->Commit (l.Num(), (PCPoint)l.Get(0), pDoor);
         }

      }
      return TRUE;

   case DDT_FRAMETB:
   case DDT_FRAMELR:
      {
         DWORD dwDir  = (m_dwType == DDT_FRAMETB) ? 2 : 0;
         DWORD dwFlags = (m_dwType == DDT_FRAMETB) ? REF_ROTTEXT : 0;

         // if the height < 2x frame size then make it solid
         if (pMax.p[dwDir] - pMin.p[dwDir] < 2 * m_afParam[0]) {
            return CommitPolygon (dwNum, papShape, pDoor, m_afParam[0] * pDoor->m_fThickness,
               DSurfMassage((DWORD)m_afParam[1], TRUE), DSurfMassage((DWORD)m_afParam[1], FALSE), dwFlags);
         }

         // else, do some clipping
         CListFixed l;
         if (m_afParam[0] > 0) {
            // top bit
            l.Init (sizeof(CPoint), papShape, dwNum);
            ::ClipPolygons (&l, dwDir, pMax.p[dwDir] - m_afParam[0], FALSE);
            RemoveLinear (&l, TRUE);
            if (l.Num())
               CommitPolygon (l.Num(), (PCPoint) l.Get(0), pDoor, pDoor->m_fThickness,
                  DSurfMassage((DWORD)m_afParam[1], TRUE), DSurfMassage((DWORD)m_afParam[1], FALSE), dwFlags);

            // bottom bit
            l.Init (sizeof(CPoint), papShape, dwNum);
            ::ClipPolygons (&l, dwDir, pMin.p[dwDir] + m_afParam[0], TRUE);
            RemoveLinear (&l, TRUE);
            if (l.Num())
               CommitPolygon (l.Num(), (PCPoint) l.Get(0), pDoor, pDoor->m_fThickness,
                  DSurfMassage((DWORD)m_afParam[1], TRUE), DSurfMassage((DWORD)m_afParam[1], FALSE), dwFlags);
         }

         // and what's left
         l.Init (sizeof(CPoint), papShape, dwNum);
         if (m_afParam[0] > 0) {
            ::ClipPolygons (&l, dwDir, pMax.p[dwDir] - m_afParam[0], TRUE);
            ::ClipPolygons (&l, dwDir, pMin.p[dwDir] + m_afParam[0], FALSE);
         }
         if (l.Num() && m_apDiv[0])
            m_apDiv[0]->Commit (l.Num(), (PCPoint)l.Get(0), pDoor);
      }
      return TRUE;

   case DDT_FRAME:
      {
         // create the noodles. One for each side
         for (i = 0; i < 2; i++) {
            re.dwSurface = DSurfMassage((DWORD)m_afParam[1], i ? FALSE : TRUE);
            re.pNoodle = new CNoodle;
            if (!re.pNoodle)
               continue;
            pDoor->m_lRENDELEM.Add (&re);
            re.pNoodle->PathSpline (TRUE, dwNum, papShape, (DWORD*)SEGCURVE_LINEAR, 0);
            re.pNoodle->ShapeDefault (NS_RECTANGLE);
            CPoint pScale, pOffset;
            pScale.Zero();
            pScale.p[0] = m_afParam[0];
            pScale.p[1] = pDoor->m_fThickness / 2;
            pOffset.Zero();
            pOffset.p[0] = pScale.p[0] / 2;
            pOffset.p[1] = pDoor->m_fThickness / 4 * (i ? 1: -1);
            re.pNoodle->ScaleVector (&pScale);
            re.pNoodle->OffsetSet (&pOffset);
         }

         // sub frame which is shrunk in
         CSpline s;
         s.Init (TRUE, dwNum, papShape, NULL, (DWORD*)SEGCURVE_LINEAR, 0, 0, 0);
         PCSpline psExtend;
         CPoint pUp;
         pUp.Zero();
         pUp.p[1] = -1; // front is up
         psExtend = s.Expand (-m_afParam[0], &pUp);
         if (psExtend) {
            // convert back to points
            CListFixed l;
            CPoint p;
            l.Init (sizeof(p));
            for (i = 0; i < psExtend->OrigNumPointsGet(); i++) {
               psExtend->OrigPointGet (i, &p);
               l.Add (&p);
            }
            delete psExtend;

            ExpandRemoveOutOfBounds (dwNum, papShape, m_afParam[0], &l);

            RemoveLinear (&l, TRUE);
            if (m_apDiv[0])
               m_apDiv[0]->Commit (l.Num(), (PCPoint)l.Get(0), pDoor);
         }
      }
      return TRUE;

   case DDT_SOLID:
      {
         DWORD dwExt, dwInt;
         dwExt = DSurfMassage ((DWORD) m_afParam[1], TRUE);
         dwInt = DSurfMassage ((DWORD) m_afParam[1], FALSE);
         return CommitPolygon (dwNum, papShape, pDoor, m_afParam[0] * pDoor->m_fThickness,
            dwExt, dwInt);
      }


   case DDT_CUTOUT:
      {
         // figure out the shape and location
         fp fWidth, fHeight;
         fWidth = (pMax.p[0] - pMin.p[0]) * m_afParam[2];
         fHeight = (pMax.p[2] - pMin.p[2]) * m_afParam[3];
         if (m_afParam[4]) {
            // keep square
            switch ((DWORD)m_afParam[1]) {
            default:
            case DDCUT_CIRCLE:
            case DDCUT_SQUARE:
               fWidth = fHeight = min(fWidth, fHeight);
               break;
            case DDCUT_HEMICIRCLEL:
            case DDCUT_HEMICIRCLER:
               fWidth = min(fWidth,fHeight/2);
               fHeight = fWidth * 2;
               break;
            case DDCUT_HEMICIRCLED:
            case DDCUT_HEMICIRCLEU:
               fHeight = min(fWidth/2, fHeight);
               fWidth = fHeight * 2;
               break;
            }
         }
         fp fCenterX, fCenterZ;
         fCenterX = pMin.p[0] + (pMax.p[0] - pMin.p[0]) * m_afParam[5];
         fCenterZ = pMin.p[2] + (pMax.p[2] - pMin.p[2]) * m_afParam[6];

         // create it
         CListFixed lCutout;
         CPoint p;
         lCutout.Init (sizeof(CPoint));
         switch ((DWORD)m_afParam[1]) {
         case DDCUT_HEMICIRCLED:
         case DDCUT_HEMICIRCLEU:
         case DDCUT_HEMICIRCLEL:
         case DDCUT_HEMICIRCLER:
            {
               DWORD dwDir, dwPerp;
               fp fScaleDir, fScalePerp;
               switch ((DWORD)m_afParam[1]) {
               case DDCUT_HEMICIRCLED:
                  dwDir = 2;
                  fScaleDir = fHeight;
                  fScalePerp = -fWidth/2;
                  break;
               case DDCUT_HEMICIRCLEU:
                  dwDir = 2;
                  fScaleDir = -fHeight;
                  fScalePerp = fWidth/2;
                  break;
               case DDCUT_HEMICIRCLEL:
                  dwDir = 0;
                  fScaleDir = fWidth;
                  fScalePerp = fHeight/2;
                  break;
               case DDCUT_HEMICIRCLER:
                  dwDir = 0;
                  fScaleDir = -fWidth;
                  fScalePerp = -fHeight/2;
                  break;
               }
               dwPerp = ((dwDir == 0) ? 2 : 0);
               p.Zero();
               for (i = 0; i <= 8; i++) {
                  p.p[dwDir] = ((dwDir == 0) ? fCenterX : fCenterZ) - fScaleDir/2 + fScaleDir * sin((fp)i / 16.0 * PI * 2.0);
                  p.p[dwPerp] = ((dwDir == 2) ? fCenterX : fCenterZ) + fScalePerp * cos((fp)i / 16.0 * PI * 2.0);
                  lCutout.Add (&p);
               }
            }
            break;
         default:
         case DDCUT_SQUARE:
            // rectangle
            p.Zero();
            p.p[0] = fCenterX - fWidth/2;
            p.p[2] = fCenterZ + fHeight/2;
            lCutout.Add (&p);
            p.p[0] = fCenterX + fWidth/2;
            lCutout.Add (&p);
            p.p[2] = fCenterZ - fHeight/2;
            lCutout.Add (&p);
            p.p[0] = fCenterX - fWidth/2;
            lCutout.Add (&p);
            break;
         case DDCUT_CIRCLE:
            p.Zero();
            for (i = 0; i < 16; i++) {
               p.p[0] = fCenterX + fWidth/2 * sin((fp)i / 16.0 * PI * 2.0);
               p.p[2] = fCenterZ + fHeight/2 * cos((fp)i / 16.0 * PI * 2.0);
               lCutout.Add (&p);
            }
            break;
         }

         // find out which cutout point is cloest to the 1st panel point
         DWORD j, dwCutClosest;
         PCPoint paCutout;
         DWORD dwCutoutNum;
         fp fClosest;
         dwCutClosest = (DWORD)-1;
         paCutout = (PCPoint) lCutout.Get(0);
         dwCutoutNum = lCutout.Num();
         for (i = 0; i < dwNum; i++) {
            CPoint p;
            fp fLen;
            p.Subtract (&paCutout[i], papShape);
            fLen = p.Length();
            if ((dwCutClosest == -1) || (fLen < fClosest)) {
               dwCutClosest = i;
               fClosest = fLen;
            }
         }


         // create two panels
         for (i = 0; i < 2; i++) {
            re.dwSurface = i ? DSURF_INTPANEL : DSURF_EXTPANEL;
            re.pPoly = new CListFixed;
            re.pPanel = new CListFixed;
            if (!re.pPoly || !re.pPanel)
               continue;
            re.pPanel->Init (sizeof(CPoint), papShape, dwNum);
            re.pPoly->Init (sizeof(CPoint));
            RemoveLinear (re.pPanel, TRUE);
            re.dwFlags = (i ? REF_FLIPTEXT : 0) | REF_CUTOUT;
            pDoor->m_lRENDELEM.Add (&re);
      
            // cutout
            re.pPoly->Required (re.pPoly->Num() + dwCutoutNum);
            for (j = 0; j < dwCutoutNum; j++)
               re.pPoly->Add (paCutout + ((j+dwCutClosest)%dwCutoutNum));
            RemoveLinear (re.pPoly, TRUE);   // shouldnt be any but...

            DWORD dw;
            PCPoint p;
            dw = re.pPoly->Num();
            p = (PCPoint) re.pPoly->Get(0);
            for (j = 0; j < dw; j++) {
               p[j].p[1] = pDoor->m_fThickness / 2 * m_afParam[0] * (i ? 1 : -1);
            }
            dw = re.pPanel->Num();
            p = (PCPoint) re.pPanel->Get(0);
            for (j = 0; j < dw; j++) {
               p[j].p[1] = pDoor->m_fThickness / 2 * m_afParam[0] * (i ? 1 : -1);
            }

            // reverse
            if (i) {
               // polygon - raised part
               dw = re.pPoly->Num();
               p = (PCPoint) re.pPoly->Get(0);
               CPoint pt;
               for (j = 0; j < dw/2; j++) {
                  pt.Copy (&p[j]);
                  p[j].Copy (&p[dw-j-1]);
                  p[dw-j-1].Copy (&pt);
               }

               // and do the panel - edge
               dw = re.pPanel->Num();
               p = (PCPoint) re.pPanel->Get(0);
               for (j = 0; j < dw/2; j++) {
                  pt.Copy (&p[j]);
                  p[j].Copy (&p[dw-j-1]);
                  p[dw-j-1].Copy (&pt);
               }
            }

         }  // i - over panels

         // pass this cutout down
         if (m_apDiv[0])
            m_apDiv[0]->Commit (dwCutoutNum, paCutout, pDoor);
      }
      return TRUE;

   case DDT_PANEL:
      {
         // create two panels
         DWORD j;
         for (i = 0; i < 2; i++) {
            re.dwSurface = i ? DSURF_INTPANEL : DSURF_EXTPANEL;
            re.pPoly = new CListFixed;
            re.pPanel = new CListFixed;
            if (!re.pPoly || !re.pPanel)
               continue;
            re.pPanel->Init (sizeof(CPoint), papShape, dwNum);
            re.pPoly->Init (sizeof(CPoint));
            RemoveLinear (re.pPanel, TRUE);
            re.dwFlags = (i ? REF_FLIPTEXT : 0);
            pDoor->m_lRENDELEM.Add (&re);
      
            // sub frame which is shrunk in
            CSpline s;
            s.Init (TRUE, re.pPanel->Num(), (PCPoint)re.pPanel->Get(0), NULL, (DWORD*)SEGCURVE_LINEAR, 0, 0, 0);
            PCSpline psExtend;
            CPoint pUp;
            pUp.Zero();
            pUp.p[1] = -1; // front is up
            psExtend = s.Expand (-m_afParam[0], &pUp);
            if (psExtend) {
               // convert back to points
               CPoint p;
               re.pPoly->Required (re.pPoly->Num() + psExtend->OrigNumPointsGet());
               for (j = 0; j < psExtend->OrigNumPointsGet(); j++) {
                  psExtend->OrigPointGet (j, &p);
                  re.pPoly->Add (&p);
               }
               delete psExtend;

               ExpandRemoveOutOfBounds (re.pPanel->Num(), (PCPoint)re.pPanel->Get(0), m_afParam[0], re.pPoly);
            }
            RemoveLinear (re.pPoly, TRUE);

            DWORD dw;
            PCPoint p;
            dw = re.pPoly->Num();
            p = (PCPoint) re.pPoly->Get(0);
#if 0
            // for test
            char szTemp[128];
            if (dw > 15) {
               OutputDebugString ("x,y,z\r\n");
               for (j = 0; j < dw; j++) {
                  sprintf (szTemp, "%g,%g,%g\r\n", (double)p[j].p[0], (double)p[j].p[1], (double)p[j].p[2]);
                  OutputDebugString (szTemp);
               }
               OutputDebugString ("\r\n");
            }
#endif
            for (j = 0; j < dw; j++) {
               p[j].p[1] = pDoor->m_fThickness / 2 * (i ? 1 : -1);
            }

            // reverse
            if (i) {
               // polygon - raised part
               dw = re.pPoly->Num();
               p = (PCPoint) re.pPoly->Get(0);
               CPoint pt;
               for (j = 0; j < dw/2; j++) {
                  pt.Copy (&p[j]);
                  p[j].Copy (&p[dw-j-1]);
                  p[dw-j-1].Copy (&pt);
               }

               // and do the panel - edge
               dw = re.pPanel->Num();
               p = (PCPoint) re.pPanel->Get(0);
               for (j = 0; j < dw/2; j++) {
                  pt.Copy (&p[j]);
                  p[j].Copy (&p[dw-j-1]);
                  p[dw-j-1].Copy (&pt);
               }
            }

         }  // i - over panels
      }
      return TRUE;

   case DDT_VOID:
      // do nothing
      return TRUE;
   }




   return TRUE;   // default
}

/**********************************************************************************
CDoor::Constructor and destructor
*/
CDoor::CDoor (void)
{
   m_lShape.Init (sizeof(CPoint));
   m_fThickness = .01;
   m_dwStyle = DS_DOORSOLID;
   m_dwHandleStyle = DHS_NONE;
   m_pHandleLoc.Zero();
   m_dwHandlePos = 0;
   m_fDirty = TRUE;
   m_dwSurface = 0;
   m_lRENDELEM.Init (sizeof(RENDELEM));
   m_pDoorDiv = NULL;
   memset (&m_DoorKnob, 0, sizeof(m_DoorKnob));
}

CDoor::~CDoor (void)
{
   // free render elements
   DWORD i;
   for (i = 0; i < m_lRENDELEM.Num(); i++) {
      PRENDELEM pr = (PRENDELEM) m_lRENDELEM.Get(i);
      if (pr->pNoodle)
         delete pr->pNoodle;
      if (pr->pRevolution)
         delete pr->pRevolution;
      if (pr->pPoly)
         delete pr->pPoly;
      if (pr->pPanel)
         delete pr->pPanel;
   }
   m_lRENDELEM.Clear();

   // free door
   if (m_pDoorDiv)
      delete m_pDoorDiv;
}



/**********************************************************************************
CDoor::ShapeSet - Sets the shape of the door.

inputs
   DWORD       dwNum - Number of points
   PCPoint     paPoints - Pointer to an array of dwNum points. These loop
               around teh door clockwise. Only the X and Z values are used, Y is
               ignored. It's assumed that Y=-1 is the FRONT of the door, and Z=1
               is up.
   fp      fTHickness - When the door is drawn, it will be this thick in the
               Y dimension. (More like maximum thickness.) Y will be from
               -fTHickness/2 to fThickness/2
*/
BOOL CDoor::ShapeSet (DWORD dwNum, PCPoint pPoints, fp fThickness)
{
   m_fDirty = TRUE;
   m_fThickness = fThickness;
   m_lShape.Init (sizeof(CPoint), pPoints, dwNum);

   DWORD i;
   PCPoint pap;
   pap = (PCPoint) m_lShape.Get(0);
   for (i = 0; i < dwNum; i++)
      pap[i].p[1] = 0;  // just enforece y=0 to save work later


   return TRUE;
}

/**********************************************************************************
CDoor::ShapeGet - Returns the shape

inputs
   fp      *pfThickness - Filled with the thickness
returns
   PCListFixed - Pointer to list initialized with CPoint() containing the shape.
                     This is only valid until the object is changed.
*/
PCListFixed CDoor::ShapeGet (fp *pfThickness)
{
   if (pfThickness)
      *pfThickness = m_fThickness;

   return &m_lShape;
}


/**********************************************************************************
CDoor::StyleSet - Sets the style, one of DS_XXXX

inputs
   DWORD       dwStyle - New style. Can also be DS_CUSTOM.
returns
   BOOL - TRUE if success
*/
BOOL CDoor::StyleSet (DWORD dwStyle)
{
   m_fDirty = TRUE;
   m_dwStyle = dwStyle;
   if (!m_dwStyle)
      return TRUE;

   // else, non-custom style so replace the existing door
   if (m_pDoorDiv)
      delete m_pDoorDiv;
   m_pDoorDiv = NULL;

   // depending on the style
   m_pDoorDiv = new CDoorDiv;
   if (!m_pDoorDiv)
      return FALSE;

   PCDoorDiv pNew, pNew2;
   pNew = m_pDoorDiv;
   switch (m_dwStyle) {
   default:
   case DS_DOORSOLID:
   case DS_CABINETSOLID:
      pNew->AutoFill (DDT_SOLID, TRUE);
      break;
   case DS_DOORSCREEN:
      pNew->AutoFill (DDT_FRAME, TRUE);
      pNew->m_afParam[0] = CM_DOORFRAMEALUMDOOR;
      pNew->m_afParam[1] = DSURF_ALFRAME;
      pNew = pNew->m_apDiv[0] = new CDoorDiv;
      if (!pNew)
         break;
      pNew->AutoFill (DDT_GLASS, TRUE);
      pNew->m_afParam[0] = DSURF_FLYSCREEN;
      pNew->m_afParam[1] = DDMS_HV;
      pNew->m_afParam[3] = pNew->m_afParam[4] = 1.2;
      pNew->m_afParam[7] = DSURF_ALFRAME;
      break;
   case DS_DOORGLASS1:  // door frame with open glass inside
      pNew->AutoFill (DDT_FRAME, TRUE);
      pNew = m_pDoorDiv->m_apDiv[0] = new CDoorDiv;
      if (!pNew)
         break;
      pNew->AutoFill (DDT_GLASS, TRUE);
      pNew->m_afParam[1] = DDMS_NONE;
      break;
   case DS_DOORGLASS2:  // door frame with open glass containing lites
      pNew->AutoFill (DDT_FRAME, TRUE);
      pNew = m_pDoorDiv->m_apDiv[0] = new CDoorDiv;
      if (!pNew)
         break;
      pNew->AutoFill (DDT_GLASS, TRUE);
      pNew->m_afParam[1] = DDMS_HV;
      break;
   case DS_DOORGLASS3:  // frame with divider and lites
      pNew->AutoFill (DDT_FRAME, TRUE);
      pNew = m_pDoorDiv->m_apDiv[0] = new CDoorDiv;
      if (!pNew)
         break;
      pNew->m_afParam[0] = DSURF_FROSTED;
      pNew->AutoFill (DDT_GLASS, TRUE);
      pNew->m_afParam[1] = DDMS_HV;
      pNew->m_afParam[2] = CM_DOORDIVFRAMEDOOR;
      pNew->m_afParam[3] = pNew->m_afParam[4] = 1.2;
      break;
   case DS_DOORGLASS4:  // frame with glass and horizontal lites
      pNew->AutoFill (DDT_FRAME, TRUE);
      pNew = m_pDoorDiv->m_apDiv[0] = new CDoorDiv;
      if (!pNew)
         break;
      pNew->AutoFill (DDT_GLASS, TRUE);
      pNew->m_afParam[1] = DDMS_HV;
      pNew->m_afParam[3] = 1.2;
      pNew->m_afParam[4] = .5;
      break;
   case DS_DOORGLASS5:  // frame with glass and horizontal lites and 1 divider
      pNew->AutoFill (DDT_FRAME, TRUE);
      pNew = m_pDoorDiv->m_apDiv[0] = new CDoorDiv;
      if (!pNew)
         break;
      pNew->AutoFill (DDT_GLASS, TRUE);
      pNew->m_afParam[1] = DDMS_HV;
      pNew->m_afParam[3] = .5;
      pNew->m_afParam[4] = .5;
      break;
   case DS_DOORGLASS6:  // frame (thin),  frosted glass, small square divisions
      pNew->AutoFill (DDT_FRAME, FALSE);
      pNew = m_pDoorDiv->m_apDiv[0] = new CDoorDiv;
      if (!pNew)
         break;
      pNew->AutoFill (DDT_GLASS, TRUE);
      pNew->m_afParam[0] = DSURF_FROSTED;
      pNew->m_afParam[1] = DDMS_HV;
      pNew->m_afParam[3] = pNew->m_afParam[4] = .2;
      break;
   case DS_DOORGLASS7:  // frame on top and bottom with glass inside
      pNew->AutoFill (DDT_FRAMETB, TRUE);
      pNew = m_pDoorDiv->m_apDiv[0] = new CDoorDiv;
      if (!pNew)
         break;
      pNew->AutoFill (DDT_GLASS, TRUE);
      pNew->m_afParam[1] = DDMS_NONE;
      break;
   case DS_DOORGLASS8:  // oval in middle
      pNew->AutoFill (DDT_CUTOUT, TRUE);
      pNew->m_afParam[1] = DDCUT_CIRCLE;
      pNew->m_afParam[2] = .66;
      pNew->m_afParam[3] = .66;
      pNew->m_afParam[4] = FALSE;
      pNew = m_pDoorDiv->m_apDiv[0] = new CDoorDiv;
      if (!pNew)
         break;
      pNew->AutoFill (DDT_GLASS, TRUE);
      pNew->m_afParam[1] = DDMS_NONE;
      break;
   case DS_DOORGLASS9:  // circle on top
      pNew->AutoFill (DDT_CUTOUT, TRUE);
      pNew->m_afParam[1] = DDCUT_CIRCLE;
      pNew->m_afParam[2] = .5;
      pNew->m_afParam[3] = .5;
      pNew->m_afParam[4] = TRUE;
      pNew->m_afParam[6] = .75;
      pNew = m_pDoorDiv->m_apDiv[0] = new CDoorDiv;
      if (!pNew)
         break;
      pNew->AutoFill (DDT_GLASS, TRUE);
      pNew->m_afParam[1] = DDMS_NONE;
      break;
   case DS_DOORGLASS10: // semi-circle cutout
      pNew->AutoFill (DDT_CUTOUT, TRUE);
      pNew->m_afParam[1] = DDCUT_HEMICIRCLEL;
      pNew->m_afParam[2] = .66;
      pNew->m_afParam[3] = .66;
      pNew->m_afParam[4] = TRUE;
      pNew = m_pDoorDiv->m_apDiv[0] = new CDoorDiv;
      if (!pNew)
         break;
      pNew->AutoFill (DDT_GLASS, TRUE);
      pNew->m_afParam[1] = DDMS_NONE;
      break;
   case DS_DOORGLASS11: // small rectangular opening
      pNew->AutoFill (DDT_CUTOUT, TRUE);
      pNew->m_afParam[1] = DDCUT_SQUARE;
      pNew->m_afParam[2] = .2;
      pNew->m_afParam[3] = .8;
      pNew->m_afParam[4] = FALSE;
      pNew->m_afParam[5] = .3;
      pNew = m_pDoorDiv->m_apDiv[0] = new CDoorDiv;
      if (!pNew)
         break;
      pNew->AutoFill (DDT_GLASS, TRUE);
      pNew->m_afParam[1] = DDMS_NONE;
      break;
   case DS_DOORGLASS12: // square on top
      pNew->AutoFill (DDT_CUTOUT, TRUE);
      pNew->m_afParam[1] = DDCUT_SQUARE;
      pNew->m_afParam[2] = .4;
      pNew->m_afParam[3] = .4;
      pNew->m_afParam[4] = TRUE;
      pNew->m_afParam[6] = .7;
      pNew = m_pDoorDiv->m_apDiv[0] = new CDoorDiv;
      if (!pNew)
         break;
      pNew->AutoFill (DDT_GLASS, TRUE);
      pNew->m_afParam[1] = DDMS_NONE;
      break;
   case DS_DOORGLASS13: // dividers into 3 frames, with hemicircle on top nad bottom, open in middle
      pNew->AutoFill (DDT_DIVIDEFIXEDH, TRUE);
      pNew->m_afParam[0] = CM_MULLIONWIDTH;
      pNew->m_afParam[1] = .33;
      pNew->m_afParam[2] = DSURF_EXTPANEL;
      pNew2 = pNew;

      // bottom loop
      pNew = pNew->m_apDiv[0] = new CDoorDiv;
      if (!pNew)
         break;
      pNew->AutoFill (DDT_CUTOUT, TRUE);
      pNew->m_afParam[1] = DDCUT_HEMICIRCLEU;
      pNew->m_afParam[2] = .5;
      pNew->m_afParam[3] = .5;
      pNew->m_afParam[4] = TRUE;
      pNew->m_afParam[6] = .75;
      pNew = pNew->m_apDiv[0] = new CDoorDiv;
      if (!pNew)
         break;
      pNew->AutoFill (DDT_GLASS, TRUE);
      pNew->m_afParam[1] = DDMS_RADIATE;
      pNew->m_afParam[6] = 1.01;

      // divide again
      pNew = pNew2->m_apDiv[1] = new CDoorDiv;
      if (!pNew)
         break;
      pNew->AutoFill (DDT_DIVIDEFIXEDH, TRUE);
      pNew->m_afParam[0] = CM_MULLIONWIDTH;
      pNew->m_afParam[1] = .5;   // half this time
      pNew->m_afParam[2] = DSURF_EXTPANEL;
      pNew2 = pNew;

      // cetner bit
      // bottom loop
      pNew = pNew->m_apDiv[0] = new CDoorDiv;
      if (!pNew)
         break;
      pNew->AutoFill (DDT_CUTOUT, TRUE);
      pNew->m_afParam[1] = DDCUT_SQUARE;
      pNew->m_afParam[2] = .5;
      pNew->m_afParam[3] = .99;
      pNew->m_afParam[4] = FALSE;
      pNew = pNew->m_apDiv[0] = new CDoorDiv;
      if (!pNew)
         break;
      pNew->AutoFill (DDT_GLASS, TRUE);
      pNew->m_afParam[1] = DDMS_HV;

      // top bit
      pNew = pNew2->m_apDiv[1] = new CDoorDiv;
      if (!pNew)
         break;
      pNew->AutoFill (DDT_CUTOUT, TRUE);
      pNew->m_afParam[1] = DDCUT_HEMICIRCLED;
      pNew->m_afParam[2] = .5;
      pNew->m_afParam[3] = .5;
      pNew->m_afParam[4] = TRUE;
      pNew->m_afParam[6] = .25;
      pNew = pNew->m_apDiv[0] = new CDoorDiv;
      if (!pNew)
         break;
      pNew->AutoFill (DDT_GLASS, TRUE);
      pNew->m_afParam[1] = DDMS_RADIATE;
      pNew->m_afParam[6] = -.01;
      break;
   case DS_DOORPANEL1:  // one main panel - flat
      pNew->AutoFill (DDT_FRAME, TRUE);
      pNew = m_pDoorDiv->m_apDiv[0] = new CDoorDiv;
      if (!pNew)
         break;
      pNew->AutoFill (DDT_SOLID, TRUE);
      pNew->m_afParam[0] = .5;
      break;
   case DS_CABINETPANEL:
      pNew->AutoFill (DDT_FRAME, FALSE);
      pNew = m_pDoorDiv->m_apDiv[0] = new CDoorDiv;
      if (!pNew)
         break;
      pNew->AutoFill (DDT_PANEL, FALSE);
      break;
   case DS_DOORPANEL2:  // top and bottom raised panel
      pNew->AutoFill (DDT_FRAME, TRUE);
      pNew = pNew->m_apDiv[0] = new CDoorDiv;
      if (!pNew)
         break;
      pNew->AutoFill (DDT_DIVIDEFIXEDH, TRUE);
      pNew->m_afParam[1] = CM_DOORHEIGHTDFRAME / CM_DOORHEIGHT;
      pNew2 = pNew;

      // bottom one
      pNew = pNew2->m_apDiv[0] = new CDoorDiv;
      if (!pNew)
         break;
      pNew->AutoFill (DDT_PANEL, TRUE);

      // top one
      pNew = pNew2->m_apDiv[1] = new CDoorDiv;
      if (!pNew)
         break;
      pNew->AutoFill (DDT_PANEL, TRUE);
      break;
   case DS_DOORPANEL3:  // top section divided into two vertical raised panels, bottom is one
      pNew->AutoFill (DDT_FRAME, TRUE);
      pNew = pNew->m_apDiv[0] = new CDoorDiv;
      if (!pNew)
         break;
      pNew->AutoFill (DDT_DIVIDEFIXEDH, TRUE);
      pNew->m_afParam[1] = CM_DOORHEIGHTDFRAME / CM_DOORHEIGHT;
      pNew2 = pNew;

      // bottom one
      pNew = pNew2->m_apDiv[0] = new CDoorDiv;
      if (!pNew)
         break;
      pNew->AutoFill (DDT_PANEL, TRUE);

      // top one
      pNew = pNew2->m_apDiv[1] = new CDoorDiv;
      if (!pNew)
         break;
      pNew->AutoFill (DDT_DIVIDEMULTIV, TRUE);
      pNew = pNew->m_apDiv[0] = new CDoorDiv;
      if (!pNew)
         break;
      pNew->AutoFill (DDT_PANEL, TRUE);
      break;
   case DS_DOORPANEL4:  // split in two vertically, bottom is split again
      pNew->AutoFill (DDT_FRAME, TRUE);
      pNew = pNew->m_apDiv[0] = new CDoorDiv;
      if (!pNew)
         break;
      pNew->AutoFill (DDT_DIVIDEFIXEDH, TRUE);
      pNew->m_afParam[1] = CM_DOORHEIGHTDFRAME / CM_DOORHEIGHT;
      pNew2 = pNew;

      // top one
      pNew = pNew2->m_apDiv[1] = new CDoorDiv;
      if (!pNew)
         break;
      pNew->AutoFill (DDT_PANEL, TRUE);

      // bottom one
      pNew = pNew2->m_apDiv[0] = new CDoorDiv;
      if (!pNew)
         break;
      pNew->AutoFill (DDT_DIVIDEMULTIV, TRUE);
      pNew = pNew->m_apDiv[0] = new CDoorDiv;
      if (!pNew)
         break;
      pNew->AutoFill (DDT_PANEL, TRUE);
      break;
   case DS_DOORPANEL5:  // split into quarters
      pNew->AutoFill (DDT_FRAME, TRUE);
      pNew = pNew->m_apDiv[0] = new CDoorDiv;
      if (!pNew)
         break;
      pNew->AutoFill (DDT_DIVIDEFIXEDH, TRUE);
      pNew->m_afParam[1] = CM_DOORHEIGHTDFRAME / CM_DOORHEIGHT;
      pNew2 = pNew;

      // bottom one
      pNew = pNew2->m_apDiv[0] = new CDoorDiv;
      if (!pNew)
         break;
      pNew->AutoFill (DDT_DIVIDEMULTIV, TRUE);
      pNew = pNew->m_apDiv[0] = new CDoorDiv;
      if (!pNew)
         break;
      pNew->AutoFill (DDT_PANEL, TRUE);

      // top one
      pNew = pNew2->m_apDiv[1] = new CDoorDiv;
      if (!pNew)
         break;
      pNew->AutoFill (DDT_DIVIDEMULTIV, TRUE);
      pNew = pNew->m_apDiv[0] = new CDoorDiv;
      if (!pNew)
         break;
      pNew->AutoFill (DDT_PANEL, TRUE);
      break;
   case DS_DOORPANEL6:  // split into 8
      pNew->AutoFill (DDT_FRAME, TRUE);
      pNew = pNew->m_apDiv[0] = new CDoorDiv;
      if (!pNew)
         break;
      pNew->AutoFill (DDT_DIVIDEMULTIH, TRUE);
      pNew->m_afParam[1] = 3;

      pNew = pNew->m_apDiv[0] = new CDoorDiv;
      if (!pNew)
         break;
      pNew->AutoFill (DDT_DIVIDEMULTIV, TRUE);

      pNew = pNew->m_apDiv[0] = new CDoorDiv;
      if (!pNew)
         break;
      pNew->AutoFill (DDT_PANEL, TRUE);
      break;
   case DS_DOORPANEL7:  // several horizontal
      pNew->AutoFill (DDT_FRAME, TRUE);
      pNew = pNew->m_apDiv[0] = new CDoorDiv;
      if (!pNew)
         break;
      pNew->AutoFill (DDT_DIVIDEMULTIH, TRUE);
      pNew->m_afParam[1] = 3;

      pNew = pNew->m_apDiv[0] = new CDoorDiv;
      if (!pNew)
         break;
      pNew->AutoFill (DDT_PANEL, TRUE);
      break;

   case DS_DOORLOUVER1:  // one main louver panel - flat
      pNew->AutoFill (DDT_FRAME, TRUE);
      pNew = pNew->m_apDiv[0] = new CDoorDiv;
      if (!pNew)
         break;
      pNew->AutoFill (DDT_SOLID, TRUE);
      pNew->m_afParam[1] = DSURF_LOUVERS;
      break;
   case DS_DOORLOUVER2:  // top and bottom louver panel
      pNew->AutoFill (DDT_FRAME, TRUE);
      pNew = pNew->m_apDiv[0] = new CDoorDiv;
      if (!pNew)
         break;
      pNew->AutoFill (DDT_DIVIDEFIXEDH, TRUE);
      pNew->m_afParam[1] = CM_DOORHEIGHTDFRAME / CM_DOORHEIGHT;
      pNew2 = pNew;

      // bottom one
      pNew = pNew2->m_apDiv[0] = new CDoorDiv;
      if (!pNew)
         break;
      pNew->AutoFill (DDT_SOLID, TRUE);
      pNew->m_afParam[1] = DSURF_LOUVERS;

      // top one
      pNew = pNew2->m_apDiv[1] = new CDoorDiv;
      if (!pNew)
         break;
      pNew->AutoFill (DDT_SOLID, TRUE);
      pNew->m_afParam[1] = DSURF_LOUVERS;
      break;
   case DS_DOORLOUVER3:  // top and bottom louver panel
      pNew->AutoFill (DDT_FRAME, TRUE);
      pNew = pNew->m_apDiv[0] = new CDoorDiv;
      if (!pNew)
         break;
      pNew->AutoFill (DDT_DIVIDEFIXEDH, TRUE);
      pNew->m_afParam[1] = CM_DOORHEIGHTDFRAME / CM_DOORHEIGHT;
      pNew2 = pNew;

      // bottom one
      pNew = pNew2->m_apDiv[0] = new CDoorDiv;
      if (!pNew)
         break;
      pNew->AutoFill (DDT_PANEL, TRUE);

      // top one
      pNew = pNew2->m_apDiv[1] = new CDoorDiv;
      if (!pNew)
         break;
      pNew->AutoFill (DDT_SOLID, TRUE);
      pNew->m_afParam[1] = DSURF_LOUVERS;
      break;

   case DS_DOORPG1:  // split in two vertically, top glass, bottom is split again
      pNew->AutoFill (DDT_FRAME, TRUE);
      pNew = pNew->m_apDiv[0] = new CDoorDiv;
      if (!pNew)
         break;
      pNew->AutoFill (DDT_DIVIDEFIXEDH, TRUE);
      pNew->m_afParam[1] = CM_DOORHEIGHTDFRAME / CM_DOORHEIGHT;
      pNew2 = pNew;

      // top one
      pNew = pNew2->m_apDiv[1] = new CDoorDiv;
      if (!pNew)
         break;
      pNew->AutoFill (DDT_GLASS, TRUE);
      pNew->m_afParam[1] = DDMS_HV;

      // bottom one
      pNew = pNew2->m_apDiv[0] = new CDoorDiv;
      if (!pNew)
         break;
      pNew->AutoFill (DDT_DIVIDEMULTIV, TRUE);
      pNew = pNew->m_apDiv[0] = new CDoorDiv;
      if (!pNew)
         break;
      pNew->AutoFill (DDT_PANEL, TRUE);
      break;
   case DS_DOORPG2:  // split into quarters, top is glass
      pNew->AutoFill (DDT_FRAME, TRUE);
      pNew = pNew->m_apDiv[0] = new CDoorDiv;
      if (!pNew)
         break;
      pNew->AutoFill (DDT_DIVIDEFIXEDH, TRUE);
      pNew->m_afParam[1] = CM_DOORHEIGHTDFRAME / CM_DOORHEIGHT;
      pNew2 = pNew;

      // top one
      pNew = pNew2->m_apDiv[1] = new CDoorDiv;
      if (!pNew)
         break;
      pNew->AutoFill (DDT_DIVIDEMULTIV, TRUE);
      pNew = pNew->m_apDiv[0] = new CDoorDiv;
      if (!pNew)
         break;
      pNew->AutoFill (DDT_GLASS, TRUE);
      pNew->m_afParam[1] = DDMS_NONE;

      // bottom one
      pNew = pNew2->m_apDiv[0] = new CDoorDiv;
      if (!pNew)
         break;
      pNew->AutoFill (DDT_DIVIDEMULTIV, TRUE);
      pNew = pNew->m_apDiv[0] = new CDoorDiv;
      if (!pNew)
         break;
      pNew->AutoFill (DDT_PANEL, TRUE);
      break;
   case DS_DOORPG3:  // split in two vertically, top oval glass
      pNew->AutoFill (DDT_FRAME, TRUE);
      pNew = pNew->m_apDiv[0] = new CDoorDiv;
      if (!pNew)
         break;
      pNew->AutoFill (DDT_DIVIDEFIXEDH, TRUE);
      pNew->m_afParam[1] = CM_DOORHEIGHTDFRAME / CM_DOORHEIGHT / 2;
      pNew2 = pNew;

      // top one
      pNew = pNew2->m_apDiv[1] = new CDoorDiv;
      if (!pNew)
         break;
      pNew->AutoFill (DDT_CUTOUT, TRUE);
      pNew->m_afParam[0] = .5;
      pNew->m_afParam[1] = DDCUT_CIRCLE;
      pNew->m_afParam[2] = .9;
      pNew->m_afParam[3]  = .9;
      pNew->m_afParam[4] = FALSE;
      pNew = pNew->m_apDiv[0] = new CDoorDiv;
      if (!pNew)
         break;
      pNew->AutoFill (DDT_GLASS, TRUE);
      pNew->m_afParam[1] = DDMS_NONE;

      // bottom one
      pNew = pNew2->m_apDiv[0] = new CDoorDiv;
      if (!pNew)
         break;
      pNew->AutoFill (DDT_PANEL, TRUE);
      break;
   case DS_DOORPG4:  // split in two vertically, top oval glass, bottom is split again
      pNew->AutoFill (DDT_FRAME, TRUE);
      pNew = pNew->m_apDiv[0] = new CDoorDiv;
      if (!pNew)
         break;
      pNew->AutoFill (DDT_DIVIDEFIXEDH, TRUE);
      pNew->m_afParam[1] = CM_DOORHEIGHTDFRAME / CM_DOORHEIGHT;
      pNew2 = pNew;

      // top one
      pNew = pNew2->m_apDiv[1] = new CDoorDiv;
      if (!pNew)
         break;
      pNew->AutoFill (DDT_CUTOUT, TRUE);
      pNew->m_afParam[0] = .5;
      pNew->m_afParam[1] = DDCUT_CIRCLE;
      pNew->m_afParam[2] = .66;
      pNew->m_afParam[3]  = .66;
      pNew->m_afParam[4] = FALSE;
      pNew = pNew->m_apDiv[0] = new CDoorDiv;
      if (!pNew)
         break;
      pNew->AutoFill (DDT_GLASS, TRUE);
      pNew->m_afParam[1] = DDMS_NONE;

      // bottom one
      pNew = pNew2->m_apDiv[0] = new CDoorDiv;
      if (!pNew)
         break;
      pNew->AutoFill (DDT_DIVIDEMULTIV, TRUE);
      pNew = pNew->m_apDiv[0] = new CDoorDiv;
      if (!pNew)
         break;
      pNew->AutoFill (DDT_PANEL, TRUE);
      break;
   case DS_DOORPG5:  // 3 vertical sections, top one is oval, bottom are split into quarters
      pNew->AutoFill (DDT_FRAME, TRUE);
      pNew = pNew->m_apDiv[0] = new CDoorDiv;
      if (!pNew)
         break;
      pNew->AutoFill (DDT_DIVIDEFIXEDH, TRUE);
      pNew->m_afParam[1] = 2.0 / 3.0;
      pNew2 = pNew;

      // top one
      pNew = pNew2->m_apDiv[1] = new CDoorDiv;
      if (!pNew)
         break;
      pNew->AutoFill (DDT_CUTOUT, TRUE);
      pNew->m_afParam[0] = 1;
      pNew->m_afParam[1] = DDCUT_HEMICIRCLED;
      pNew->m_afParam[2] = .99;
      pNew->m_afParam[3]  = .99;
      pNew->m_afParam[4] = TRUE;
      pNew = pNew->m_apDiv[0] = new CDoorDiv;
      if (!pNew)
         break;
      pNew->AutoFill (DDT_GLASS, TRUE);
      pNew->m_afParam[1] = DDMS_RADIATE;
      pNew->m_afParam[6] = -.01;

      // bottom one
      pNew = pNew2->m_apDiv[0] = new CDoorDiv;
      if (!pNew)
         break;
      pNew->AutoFill (DDT_DIVIDEMULTIH, TRUE);
      pNew = pNew->m_apDiv[0] = new CDoorDiv;
      if (!pNew)
         break;
      pNew->AutoFill (DDT_DIVIDEMULTIV, TRUE);
      pNew = pNew->m_apDiv[0] = new CDoorDiv;
      if (!pNew)
         break;
      pNew->AutoFill (DDT_PANEL, TRUE);
      break;

   case DS_DOORBRACE1:
      pNew->AutoFill (DDT_CROSSBRACE, TRUE);
      pNew->m_afParam[2] = 1;
      pNew->m_afParam[3] = DDCB_Z;
      pNew = pNew->m_apDiv[0] = new CDoorDiv;
      if (!pNew)
         break;
      pNew->AutoFill (DDT_SOLID, TRUE);
      break;
   case DS_DOORBRACE2:
      pNew->AutoFill (DDT_CROSSBRACE, TRUE);
      pNew->m_afParam[2] = 1;
      pNew->m_afParam[3] = DDCB_MORETHAN;
      pNew = pNew->m_apDiv[0] = new CDoorDiv;
      if (!pNew)
         break;
      pNew->AutoFill (DDT_SOLID, TRUE);
      break;

   case DS_DOORGARAGE1:
      pNew->AutoFill (DDT_SOLID, TRUE);
      break;

   case DS_DOORGARAGE2:  // several vertical
      pNew->AutoFill (DDT_FRAME, TRUE);
      pNew = pNew->m_apDiv[0] = new CDoorDiv;
      if (!pNew)
         break;
      pNew->AutoFill (DDT_DIVIDEMULTIV, TRUE);
      pNew->m_afParam[1] = 3;

      pNew = pNew->m_apDiv[0] = new CDoorDiv;
      if (!pNew)
         break;
      pNew->AutoFill (DDT_PANEL, TRUE);
      break;

   case DS_DOORGARAGE3:  // several vertical with windows in them
      pNew->AutoFill (DDT_FRAME, TRUE);
      pNew = pNew->m_apDiv[0] = new CDoorDiv;
      if (!pNew)
         break;
      pNew->AutoFill (DDT_DIVIDEMULTIV, TRUE);
      pNew->m_afParam[1] = 3;

      pNew = pNew->m_apDiv[0] = new CDoorDiv;
      if (!pNew)
         break;
      pNew->AutoFill (DDT_CUTOUT, TRUE);
      pNew->m_afParam[0] = .5;
      pNew->m_afParam[1] = DDCUT_HEMICIRCLED;
      pNew->m_afParam[2] = .8;
      pNew->m_afParam[3] = .8;
      pNew->m_afParam[4] = TRUE;
      pNew = pNew->m_apDiv[0] = new CDoorDiv;
      if (!pNew)
         break;
      pNew->AutoFill (DDT_GLASS, TRUE);
      pNew->m_afParam[1] = DDMS_NONE;
      break;

   case DS_DOORGATE1:    // wrought iron gate
      pNew->AutoFill (DDT_DIVIDEFIXEDH, TRUE);
      pNew->m_afParam[0] = CM_MULLIONWIDTH;
      pNew->m_afParam[1] = .1;
      pNew->m_afParam[2] = DSURF_ALFRAME;
      pNew2 = pNew;

      // bottom
      pNew = pNew2->m_apDiv[0] = new CDoorDiv;
      if (!pNew)
         break;
      pNew->AutoFill (DDT_DIVIDEMULTIV, TRUE);
      pNew->m_afParam[0] = CM_MULLIONWIDTH;
      pNew->m_afParam[1] = 8;
      pNew->m_afParam[2] = DSURF_ALFRAME;
      pNew = pNew->m_apDiv[0] = new CDoorDiv;
      if (!pNew)
         break;
      pNew->AutoFill (DDT_VOID, TRUE);

      // top is split yet again
      pNew = pNew2->m_apDiv[1] = new CDoorDiv;
      if (!pNew)
         break;
      pNew->AutoFill (DDT_DIVIDEFIXEDH, TRUE);
      pNew->m_afParam[0] = CM_MULLIONWIDTH;
      pNew->m_afParam[1] = (1 - .1) * .8;
      pNew->m_afParam[2] = DSURF_ALFRAME;
      pNew2 = pNew;

      // middle
      pNew = pNew2->m_apDiv[0] = new CDoorDiv;
      if (!pNew)
         break;
      pNew->AutoFill (DDT_CROSSBRACE, TRUE);
      pNew->m_afParam[0] = CM_MULLIONWIDTH;
      pNew->m_afParam[2] = TRUE;
      pNew->m_afParam[3] = DDCB_SLASH;
      pNew = pNew->m_apDiv[0] = new CDoorDiv;
      if (!pNew)
         break;
      pNew->AutoFill (DDT_DIVIDEMULTIV, TRUE);
      pNew->m_afParam[0] = CM_MULLIONWIDTH;
      pNew->m_afParam[1] = 8;
      pNew->m_afParam[2] = DSURF_ALFRAME;
      pNew = pNew->m_apDiv[0] = new CDoorDiv;
      if (!pNew)
         break;
      pNew->AutoFill (DDT_VOID, TRUE);

      // top
      pNew = pNew2->m_apDiv[1] = new CDoorDiv;
      if (!pNew)
         break;
      pNew->AutoFill (DDT_DIVIDEMULTIV, TRUE);
      pNew->m_afParam[0] = CM_MULLIONWIDTH;
      pNew->m_afParam[1] = 8;
      pNew->m_afParam[2] = DSURF_ALFRAME;
      pNew = pNew->m_apDiv[0] = new CDoorDiv;
      if (!pNew)
         break;
      pNew->AutoFill (DDT_VOID, TRUE);
      break;


   case DS_WINDOWPLAIN1:
      pNew->AutoFill (DDT_GLASS, FALSE);
      pNew->m_afParam[1] = DDMS_NONE;
      break;
   case DS_WINDOWPLAIN2:
      pNew->AutoFill (DDT_FRAME, FALSE);
      pNew->m_afParam[0] = CM_MULLIONWIDTH;
      pNew->m_afParam[1] = DSURF_ALFRAME;
      pNew = pNew->m_apDiv[0] = new CDoorDiv;
      if (!pNew)
         break;
      pNew->AutoFill (DDT_GLASS, FALSE);
      pNew->m_afParam[1] = DDMS_NONE;
      break;
   case DS_WINDOWPLAIN3:
   case DS_CABINETGLASS:
      pNew->AutoFill (DDT_FRAME, FALSE);
      pNew = pNew->m_apDiv[0] = new CDoorDiv;
      if (!pNew)
         break;
      pNew->AutoFill (DDT_GLASS, FALSE);
      pNew->m_afParam[1] = DDMS_NONE;
      break;
   case DS_WINDOWPLAIN4:
      pNew->AutoFill (DDT_GLASS, FALSE);
      pNew->m_afParam[0] = DSURF_FROSTED;
      pNew->m_afParam[1] = DDMS_NONE;
      break;

   case DS_CABINETLITES:
      pNew->AutoFill (DDT_FRAME, FALSE);
      pNew = pNew->m_apDiv[0] = new CDoorDiv;
      if (!pNew)
         break;
      pNew->AutoFill (DDT_GLASS, FALSE);
      pNew->m_afParam[1] = DDMS_HV;
      pNew->m_afParam[2] = .01;
      pNew->m_afParam[3] = .1;
      pNew->m_afParam[4] = .15;
      break;
   case DS_WINDOWLITE1:
      pNew->AutoFill (DDT_FRAME, FALSE);
      pNew = pNew->m_apDiv[0] = new CDoorDiv;
      if (!pNew)
         break;
      pNew->AutoFill (DDT_GLASS, FALSE);
      pNew->m_afParam[1] = DDMS_HV;
      break;
   case DS_WINDOWLITE2:
      pNew->AutoFill (DDT_FRAME, FALSE);
      pNew = pNew->m_apDiv[0] = new CDoorDiv;
      if (!pNew)
         break;
      pNew->AutoFill (DDT_GLASS, FALSE);
      pNew->m_afParam[1] = DDMS_RADIATE;
      break;
   case DS_WINDOWLITE3:
      pNew->AutoFill (DDT_FRAME, FALSE);
      pNew = pNew->m_apDiv[0] = new CDoorDiv;
      if (!pNew)
         break;
      pNew->AutoFill (DDT_GLASS, FALSE);
      pNew->m_afParam[1] = DDMS_DIAMOND;
      break;
   case DS_WINDOWLITE4:
      pNew->AutoFill (DDT_FRAME, FALSE);
      pNew = pNew->m_apDiv[0] = new CDoorDiv;
      if (!pNew)
         break;
      pNew->AutoFill (DDT_GLASS, FALSE);
      pNew->m_afParam[1] = DDMS_RADIATE;
      pNew->m_afParam[6] = -.01;
      break;
   case DS_WINDOWLITE5:
      pNew->AutoFill (DDT_FRAME, FALSE);
      pNew = pNew->m_apDiv[0] = new CDoorDiv;
      if (!pNew)
         break;
      pNew->AutoFill (DDT_GLASS, FALSE);
      pNew->m_afParam[1] = DDMS_RADIATE;
      pNew->m_afParam[5] = -.01;
      pNew->m_afParam[6] = -.01;
      break;
   case DS_WINDOWLITE6:
      pNew->AutoFill (DDT_FRAME, FALSE);
      pNew = pNew->m_apDiv[0] = new CDoorDiv;
      if (!pNew)
         break;
      pNew->AutoFill (DDT_GLASS, FALSE);
      pNew->m_afParam[1] = DDMS_RADIATE;
      pNew->m_afParam[5] = 1.01;
      pNew->m_afParam[6] = -.01;
      break;
   case DS_WINDOWLITE7:
      pNew->AutoFill (DDT_FRAME, FALSE);
      pNew = pNew->m_apDiv[0] = new CDoorDiv;
      if (!pNew)
         break;
      pNew->AutoFill (DDT_DIVIDEFIXEDH, FALSE);
      pNew->m_afParam[1] = 2.0 / 3.0;
      pNew2 = pNew;

      // bottom
      pNew = pNew2->m_apDiv[0] = new CDoorDiv;
      if (!pNew)
         break;
      pNew->AutoFill (DDT_GLASS, FALSE);
      pNew->m_afParam[1] = DDMS_HV;

      // top
      pNew = pNew2->m_apDiv[1] = new CDoorDiv;
      if (!pNew)
         break;
      pNew->AutoFill (DDT_GLASS, FALSE);
      pNew->m_afParam[1] = DDMS_RADIATE;
      pNew->m_afParam[6] = -.01;
      break;
   case DS_WINDOWLITE8:
      pNew->AutoFill (DDT_FRAME, FALSE);
      pNew = pNew->m_apDiv[0] = new CDoorDiv;
      if (!pNew)
         break;
      pNew->AutoFill (DDT_GLASS, FALSE);
      pNew->m_afParam[0] = DSURF_FROSTED;
      pNew->m_afParam[1] = DDMS_HV;
      pNew->m_afParam[3] = pNew->m_afParam[4] = .2;
      break;

   case DS_CABINETLOUVER:  // one main louver panel - flat
      pNew->AutoFill (DDT_FRAME, FALSE);
      pNew = pNew->m_apDiv[0] = new CDoorDiv;
      if (!pNew)
         break;
      pNew->AutoFill (DDT_SOLID, FALSE);
      pNew->m_afParam[1] = DSURF_LOUVERS;
      break;

   case DS_WINDOWSHUTTER1:  // one main louver panel - flat
      pNew->AutoFill (DDT_FRAME, FALSE);
      pNew->m_afParam[1] = DSURF_ALFRAME;
      pNew = pNew->m_apDiv[0] = new CDoorDiv;
      if (!pNew)
         break;
      pNew->AutoFill (DDT_SOLID, FALSE);
      pNew->m_afParam[1] = DSURF_LOUVERS;
      break;
   case DS_WINDOWSHUTTER2:
      pNew->AutoFill (DDT_CROSSBRACE, FALSE);
      pNew->m_afParam[2] = 1;
      pNew->m_afParam[3] = DDCB_MORETHAN;
      pNew->m_afParam[4] = 1;
      pNew = pNew->m_apDiv[0] = new CDoorDiv;
      if (!pNew)
         break;
      pNew->AutoFill (DDT_FRAME, FALSE);
      pNew = pNew->m_apDiv[0] = new CDoorDiv;
      if (!pNew)
         break;
      pNew->AutoFill (DDT_SOLID, FALSE);
      break;
   case DS_WINDOWSCREEN:
      pNew->AutoFill (DDT_FRAME, FALSE);
      pNew->m_afParam[0] = CM_MULLIONWIDTH;
      pNew->m_afParam[1] = DSURF_ALFRAME;
      pNew = pNew->m_apDiv[0] = new CDoorDiv;
      if (!pNew)
         break;
      pNew->AutoFill (DDT_GLASS, FALSE);
      pNew->m_afParam[0] = DSURF_FLYSCREEN;
      pNew->m_afParam[1] = DDMS_HV;
      pNew->m_afParam[3] = pNew->m_afParam[4] = 1.2;
      pNew->m_afParam[7] = DSURF_ALFRAME;
      break;
   case DS_WINDOWLOUVER1:  // frame on top and bottom with glass inside
      pNew->AutoFill (DDT_FRAMELR, TRUE);
      pNew->m_afParam[0] = .01;
      pNew->m_afParam[1] = DSURF_ALFRAME;
      pNew = m_pDoorDiv->m_apDiv[0] = new CDoorDiv;
      if (!pNew)
         break;
      pNew->AutoFill (DDT_GLASS, TRUE);
      pNew->m_afParam[1] = DDMS_NONE;
      break;
   case DS_WINDOWLOUVER2:  // frame on top and bottom with glass inside
      pNew->AutoFill (DDT_FRAMETB, TRUE);
      pNew->m_afParam[0] = .01;
      pNew->m_afParam[1] = DSURF_ALFRAME;
      pNew = m_pDoorDiv->m_apDiv[0] = new CDoorDiv;
      if (!pNew)
         break;
      pNew->AutoFill (DDT_GLASS, TRUE);
      pNew->m_afParam[1] = DDMS_NONE;
      break;

   case DS_WINDOWBARS1:
      pNew->AutoFill (DDT_GLASS, FALSE);
      pNew->m_afParam[0] = 0;
      pNew->m_afParam[1] = DDMS_HV;
      pNew->m_afParam[3] = 1.0;
      pNew->m_afParam[4] = 0.12;
      pNew->m_afParam[7] = DSURF_ALFRAME;
      break;
   case DS_WINDOWBARS2:
      pNew->AutoFill (DDT_GLASS, FALSE);
      pNew->m_afParam[0] = 0;
      pNew->m_afParam[1] = DDMS_HV;
      pNew->m_afParam[4] = 1.0;
      pNew->m_afParam[3] = 0.12;
      pNew->m_afParam[7] = DSURF_ALFRAME;
      break;
   case DS_WINDOWBARS3:
      pNew->AutoFill (DDT_GLASS, FALSE);
      pNew->m_afParam[0] = 0;
      pNew->m_afParam[1] = DDMS_DIAMOND;
      pNew->m_afParam[3] = 0.12;
      pNew->m_afParam[4] = 0.12;
      pNew->m_afParam[7] = DSURF_ALFRAME;
      break;
   case DS_WINDOWBARS4:
      pNew->AutoFill (DDT_FRAME, FALSE);
      pNew = m_pDoorDiv->m_apDiv[0] = new CDoorDiv;
      if (!pNew)
         break;
      pNew->AutoFill (DDT_GLASS, FALSE);
      pNew->m_afParam[0] = 0;
      pNew->m_afParam[1] = DDMS_HV;
      pNew->m_afParam[2] = .05;
      pNew->m_afParam[3] = 1.0;
      pNew->m_afParam[4] = 0.10;
      break;
   case DS_WINDOWBARS5:
      pNew->AutoFill (DDT_FRAME, FALSE);
      pNew = m_pDoorDiv->m_apDiv[0] = new CDoorDiv;
      if (!pNew)
         break;
      pNew->AutoFill (DDT_GLASS, FALSE);
      pNew->m_afParam[0] = 0;
      pNew->m_afParam[1] = DDMS_HV;
      pNew->m_afParam[2] = .05;
      pNew->m_afParam[4] = 1.0;
      pNew->m_afParam[3] = 0.10;
      break;
   }

   return TRUE;
}

/**********************************************************************************
CDoor::StyleHandleSet - Sets the doorknob style.

inputs
   DOORKNOB       *pKnob - New doorknob style
returns
   BOOL - TRUE if success
*/
BOOL CDoor::StyleHandleSet (DOORKNOB *pKnob)
{
   m_fDirty = TRUE;
   m_DoorKnob = *pKnob;
   return TRUE;
}


/**********************************************************************************
CDoor::Clone - Clones the door.

returns
   CDoor *- CLone
*/
CDoor * CDoor::Clone (void)
{
   PCDoor pNew = new CDoor;
   if (!pNew)
      return NULL;

   pNew->m_lShape.Init (sizeof(CPoint), m_lShape.Get(0), m_lShape.Num());
   pNew->m_fThickness = m_fThickness;
   pNew->m_dwStyle = m_dwStyle;
   pNew->m_dwHandleStyle = m_dwHandleStyle;
   pNew->m_pHandleLoc.Copy (&pNew->m_pHandleLoc);
   pNew->m_dwHandlePos = m_dwHandlePos;
   pNew->m_fDirty = m_fDirty;
   pNew->m_dwSurface = m_dwSurface;
   pNew->m_DoorKnob = m_DoorKnob;
   pNew->m_lRENDELEM.Init (sizeof(RENDELEM), m_lRENDELEM.Get(0), m_lRENDELEM.Num());

   DWORD i;
   for (i = 0; i < pNew->m_lRENDELEM.Num(); i++) {
      PRENDELEM pr = (PRENDELEM) pNew->m_lRENDELEM.Get(i);
      if (pr->pNoodle)
         pr->pNoodle = pr->pNoodle->Clone();
      if (pr->pRevolution)
         pr->pRevolution = pr->pRevolution->Clone();
      if (pr->pPoly) {
         PCListFixed pOld = pr->pPoly;
         pr->pPoly = new CListFixed;
         if (pr->pPoly)
            pr->pPoly->Init (sizeof(CPoint), pOld->Get(0), pOld->Num());
      }
      if (pr->pPanel) {
         PCListFixed pOld = pr->pPanel;
         pr->pPanel = new CListFixed;
         if (pr->pPanel)
            pr->pPanel->Init (sizeof(CPoint), pOld->Get(0), pOld->Num());
      }
   }

   pNew->m_pDoorDiv = m_pDoorDiv->Clone();

   return pNew;
}

static PWSTR gpszDoor = L"Door";
static PWSTR gpszStyle = L"Style";
static PWSTR gpszHandleStyle = L"HandleStyle";
static PWSTR gpszHandlePos = L"HandlePos";
static PWSTR gpszHandleLoc = L"HandleLoc";

/**********************************************************************************
CDoor::MMLTo - Writes the door to MML. NOTE: This writes as little as possible.
It does NOT write anything about the shape or thickness. Basically it writes
the style ID and if it's custom, any style info.

returns
   PCMMLNode2 - Node that contains the door information
*/
PCMMLNode2 CDoor::MMLTo (void)
{
   PCMMLNode2 pNode = new CMMLNode2;
   if (!pNode)
      return NULL;
   pNode->NameSet (gpszDoor);

   // dont write out m_lShape
   // dont write out m_fThickness
   // dont write out m_DoorKnob

   MMLValueSet (pNode, gpszStyle, (int) m_dwStyle);
   if (m_dwHandleStyle != DHS_NONE) {
      MMLValueSet (pNode, gpszHandleStyle, (int) m_dwHandleStyle);
      MMLValueSet (pNode, gpszHandlePos, (int) m_dwHandlePos);
      MMLValueSet (pNode, gpszHandleLoc, &m_pHandleLoc);
   }

   // If custom, need to save door division
   if (!m_dwStyle && m_pDoorDiv) {
      PCMMLNode2 pSub;
      pSub = m_pDoorDiv->MMLTo();
      if (pSub) {
         pSub->NameSet (gpszDoorDiv);
         pNode->ContentAdd (pSub);
      }
   }

   return pNode;
}

/**********************************************************************************
CDoor::MMLFrom - Reads in information about the door. Because not much was written,
this only reads in the style information.

inputs
   PCMMLNode2      pNode - TO read from
returns
   BOOL - TRUE if success
*/
BOOL CDoor::MMLFrom (PCMMLNode2 pNode)
{
   // free up
   if (m_pDoorDiv)
      delete m_pDoorDiv;
   m_pDoorDiv = NULL;

   m_fDirty = TRUE;

   // dont read in m_lShape
   // dont read in m_fThickness
   // dont write out m_DoorKnob

   m_dwStyle = (DWORD) MMLValueGetInt (pNode, gpszStyle, DS_DOORSOLID);
   m_dwHandleStyle = (DWORD) MMLValueGetInt (pNode, gpszHandleStyle, DHS_NONE);
   if (m_dwHandleStyle != DHS_NONE) {
      m_dwHandlePos = (DWORD) MMLValueGetInt (pNode, gpszHandlePos, 0);

      CPoint pZero;
      pZero.Zero();
      MMLValueGetPoint (pNode, gpszHandleLoc, &m_pHandleLoc, &pZero);
   }

   if (m_dwStyle) {
      // May need to call ::StyleSet() from m_dwStyle to intialize all the parameters
      StyleSet (m_dwStyle);
   }
   else {
      // If custom, retrieve the door
      DWORD dwIndex;
      PCMMLNode2 pSub;
      PWSTR psz;
      dwIndex = pNode->ContentFind (gpszDoorDiv);
      pSub = NULL;
      if (dwIndex != -1)
         pNode->ContentEnum (dwIndex, &psz, &pSub);
      if (pSub) {
         m_pDoorDiv = new CDoorDiv;
         if (m_pDoorDiv)
            m_pDoorDiv->MMLFrom (pSub);
      }
   }
   if (!m_pDoorDiv)
      StyleSet (1);  // pick a style


   return TRUE;
}

/**********************************************************************************
CDoor::SurfaceQuery - Returns a DWORD with a bitfield indicating what types
of surfaces are on the door (glass, framing, etc.). This is used to determine what
colors are needed for the object calling.

returns
   DWORD - bitfield of DSURF_XXX types
*/
DWORD CDoor::SurfaceQuery (void)
{
   CalcIfNecessary();

   return m_dwSurface;
}


/**********************************************************************************
CDoor::Render - Tells the door to render itself. It will be within the original X and Z
coordinates given, but will go from -m_fThickness/2 to m_fThickness/2.

inputs
   POBJECTRENDER        pr - Rendering information
   PCRenderSurface      *prs - Render to.
   DWORD                dwSurface - Which surfaces to render. From DSURF_XXX
returns
   none
*/
void CDoor::Render (POBJECTRENDER pr, CRenderSurface *prs, DWORD dwSurface)
{
   CalcIfNecessary();

   // if dont have this color dont bother
   if (!(dwSurface & m_dwSurface))
      return;

   DWORD i, j;
   for (i = 0; i < m_lRENDELEM.Num(); i++) {
      PRENDELEM pre = (PRENDELEM) m_lRENDELEM.Get(i);

      // dont draw this one if surfaces aren't matched
      if (!(pre->dwSurface & dwSurface))
         continue;

      if (pre->fApplyMatrix) {
         prs->Push();
         prs->Multiply (&pre->mRotate);
      }

      if (pre->pNoodle)
         pre->pNoodle->Render (pr, prs);
      if (pre->pRevolution)
         pre->pRevolution->Render(pr, prs);

      if (pre->pPoly && pre->pPanel && (pre->dwFlags & REF_CUTOUT)) {
         // draw the zipper
         DoorZipper ((PCPoint)pre->pPoly->Get(0), pre->pPoly->Num(),
            (PCPoint)pre->pPanel->Get(0), pre->pPanel->Num(),
            (pre->dwFlags & REF_FLIPTEXT) ? FALSE : TRUE, prs);

         // draw zipper around inside and outside
         DWORD k;
         for (k = 0; k < 2; k++) {
            // BUGFIX - If low detail then dont bother with this zipper
            if (prs->m_fDetail > m_fThickness * 5.0)
               continue;

            // draw the zipper
            // CListFixed lFront, lBack;
            m_lRenderFront.Init (sizeof(HVXYZ));
            m_lRenderBack.Init (sizeof(HVXYZ));
            HVXYZ hv;
            memset (&hv, 0, sizeof(hv));
            PCPoint pPoints;
            DWORD dwNum;
            pPoints = (PCPoint) (k ? pre->pPanel->Get(0) : pre->pPoly->Get(0));
            dwNum = (k ? pre->pPanel->Num() : pre->pPoly->Num());
            for (j = 0; j < dwNum; j++) {
               if (k) {
                  hv.p.Copy (&pPoints[j]);
                  m_lRenderFront.Add (&hv);
                  hv.p.p[1] = 0; // keep same value and go down
                  m_lRenderBack.Add (&hv);
               }
               else {
                  hv.p.Copy (&pPoints[j]);
                  m_lRenderBack.Add (&hv);
                  hv.p.p[1] = 0; // keep same value and go down
                  m_lRenderFront.Add (&hv);
               }
            }
            prs->ShapeZipper ((PHVXYZ) m_lRenderFront.Get(0), m_lRenderFront.Num(),
               (PHVXYZ)m_lRenderBack.Get(0), m_lRenderBack.Num(), TRUE, 0, 1, FALSE);
         }
      }
      else if (pre->pPoly) {  // just polygon
         // create all the bits for this
         // NOTE: Assuming that normal is y=-1, or if reverse, y=1
         DWORD dwNormal, dwPoints, dwTextures, dwVertices;
         PCPoint pPoints;
         PVERTEX pVertices;
         PTEXTPOINT5 pTextures;
         PPOLYDESCRIPT pPoly;
         DWORD dwNum = pre->pPoly->Num();
         dwNormal = prs->NewNormal(0, (pre->dwFlags & REF_FLIPTEXT) ? 1 : -1, 0, TRUE);
         pPoints = prs->NewPoints (&dwPoints, dwNum);
         if (pPoints)
            memcpy (pPoints, pre->pPoly->Get(0), dwNum * sizeof(CPoint));
         pTextures = prs->NewTextures (&dwTextures, dwNum);
         if (pTextures && pPoints) {
            // copy over
            TEXTPOINT5 pll;
            memset (&pll, 0, sizeof(pll));
            for (j = 0; j < dwNum; j++) {
               pTextures[j].hv[0] = pPoints[j].p[0] * ((pre->dwFlags & REF_FLIPTEXT) ? -1 : 1);
               pTextures[j].hv[1] = -pPoints[j].p[2];  // since textures go in reverse direction

               // volumetrix
               pTextures[j].xyz[0] = pPoints[j].p[0];
               pTextures[j].xyz[1] = pPoints[j].p[1];
               pTextures[j].xyz[2] = pPoints[j].p[2];

               // keep track of lower left corner
               if (j) {
                  pll.hv[0] = min(pll.hv[0], pTextures[j].hv[0]);
                  pll.hv[1] = max(pll.hv[1], pTextures[j].hv[1]);
               }
               else
                  pll = pTextures[j];
            }

            // loop and adjust by offset
            for (j = 0; j < dwNum; j++) {
               pTextures[j].hv[0] -= pll.hv[0];
               pTextures[j].hv[1] -= pll.hv[1];
            }

            // if have flag for rotation then do so
            if (pre->dwFlags & REF_ROTTEXT) for (j = 0; j < dwNum; j++) {
               fp f;
               f = pTextures[j].hv[0];
               pTextures[j].hv[0] = pTextures[j].hv[1];
               pTextures[j].hv[1] = -f;
            }
            // scale the textures
            prs->ApplyTextureRotation (pTextures, dwNum);
         }  // pTextures

         pVertices = prs->NewVertices (&dwVertices, dwNum);
         DWORD dwColor;
         dwColor = prs->DefColor();
         if (pVertices) {
            for (j = 0; j < dwNum; j++) {
               pVertices[j].dwNormal = dwNormal;
               pVertices[j].dwColor = dwColor;
               pVertices[j].dwPoint = pPoints ? (dwPoints + j) : 0;
               pVertices[j].dwTexture = pTextures ? (dwTextures + j) : 0;
            }
         }

         // polugon
         pPoly = prs->NewPolygon (dwNum);
         if (pPoly) {
            // can backface cull if thick (which means Y != 0), but
            // if thin, dont
            pPoly->fCanBackfaceCull = (pPoints[0].p[1] ? TRUE : FALSE);
            pPoly->dwSurface = prs->DefSurface();
            pPoly->dwIDPart = prs->NewIDPart();

            DWORD *padw;
            padw = (DWORD*) (pPoly + 1);
            for (j = 0; j < dwNum; j++)
               padw[j] = dwVertices + j;
         }

         // dont draw the outline if REF_NOSIDES is set
         if (!(pre->dwFlags & REF_NOSIDES) && pPoints[0].p[1]) {
            // get location of panel
            PCPoint pPan = (PCPoint) (pre->pPanel ? pre->pPanel->Get(0) : NULL);

            if (pPan) {
               DoorZipper ((PCPoint)pre->pPoly->Get(0), pre->pPoly->Num(),
                  (PCPoint)pre->pPanel->Get(0), pre->pPanel->Num(),
                  (pre->dwFlags & REF_FLIPTEXT) ? FALSE : TRUE, prs);
            }
            else {
               // BUGFIX - If low detail then dont bother with this zipper
               if (prs->m_fDetail > m_fThickness * 5.0)
                  goto donerotate;  // BUGFIX - was continue;

               // draw the zipper
               // CListFixed lFront, lBack;
               m_lRenderFront.Init (sizeof(HVXYZ));
               m_lRenderBack.Init (sizeof(HVXYZ));
               HVXYZ hv;
               memset (&hv, 0, sizeof(hv));
               for (j = 0; j < dwNum; j++) {
                  hv.p.Copy (&pPoints[j]);
                  m_lRenderFront.Add (&hv);
                  if (pPan)   // go out to panel
                     hv.p.Copy (&pPan[j]);
                  else
                     hv.p.p[1] = 0; // keep same value and go down
                  m_lRenderBack.Add (&hv);
               }

               prs->ShapeZipper ((PHVXYZ) m_lRenderFront.Get(0), m_lRenderFront.Num(),
                  (PHVXYZ)m_lRenderBack.Get(0), m_lRenderBack.Num(), TRUE, 0, 1, FALSE);
            }
         }  // draw zipper

      }  // if pPoly

donerotate:
      if (pre->fApplyMatrix)
         prs->Pop();

   } // over render objects

}


/**********************************************************************************
CDoor::CommitRect - Given a rectangle with LL and UR corner, commits it as a polygon
(just the one side though)

NOTE: pMin and pMax can be any corner since internally takes max and min.

inputs
   PCPoint     pMin - lower left corner (assuming that looking from y=-1 direction)
   PCPoint     pMax - upper right corner (assuming that looking from y=-1 direction)
   fp      fY - Y location. If it's negative then pMin and pMax use as LL and UR corner.
               if positive, pMin is LR and pMax is UR corner
   DWORD       dwColor - Color to use
   DWORD       dwFlags - Flags to or in
   PCMatrix    pMatrix - If NOT null, then rotate by this amount
returns
   BOOL - TRUE if success
*/
BOOL CDoor::CommitRect (PCPoint ppMin, PCPoint ppMax, fp fY, DWORD dwColor, DWORD dwFlags, PCMatrix pMatrix)
{
   // clear it out
   RENDELEM re;
   memset (&re, 0, sizeof(re));
   re.dwFlags = dwFlags;
   re.dwSurface = dwColor;
   if (pMatrix) {
      re.fApplyMatrix = TRUE;
      re.mRotate.Copy (pMatrix);
   }

   CPoint pMax, pMin;
   pMax.Copy (ppMin);
   pMax.Max (ppMax);
   pMin.Copy (ppMin);
   pMin.Min (ppMax);

   // new list
   PCListFixed pl;
   pl = new CListFixed;
   if (!pl)
      return FALSE;
   pl->Init (sizeof(CPoint));

   DWORD dws;
   dws = (fY < 0) ? 0 : 1;

   // fill in list
   CPoint p;
   pl->Required (4);
   p.Zero();
   p.p[0] = dws ? pMax.p[0] : pMin.p[0];
   p.p[1] = fY;
   p.p[2] = pMax.p[2];
   pl->Add (&p);
   p.p[0] = dws ? pMin.p[0] : pMax.p[0];
   pl->Add (&p);
   p.p[2] = pMin.p[2];
   pl->Add (&p);
   p.p[0] = dws ? pMax.p[0] : pMin.p[0];
   pl->Add (&p);
   if (dws)
      re.dwFlags |= REF_FLIPTEXT;

   re.pPoly = pl;
   m_lRENDELEM.Add (&re);

   return TRUE;
}

/**********************************************************************************
CDoor::CalcDoorKnob - Adds the doorknob to the m_lRENDEELEM list.
*/
void CDoor::CalcDoorKnob (void)
{
   // doorknob - loop over sides, 0 is front
   DWORD dws;
   RENDELEM re;

   for (dws = 0; dws < 2; dws++) {

      if (!m_DoorKnob.adwStyle[dws])
         continue;

      // clear it out
      memset (&re, 0, sizeof(re));
      re.dwFlags = REF_NOSIDES;
      re.dwSurface = DSURF_DOORKNOB;

      // make the matrix to rotate around the knob
      CMatrix  m, m2;
      BOOL fRotate;
      m.Identity();
      fRotate = FALSE;
      if (m_DoorKnob.fKnobRotate) {
         fRotate = TRUE;
         m.Translation (-m_DoorKnob.pKnob.p[0], -m_DoorKnob.pKnob.p[1], -m_DoorKnob.pKnob.p[2]);
         m2.RotationY (m_DoorKnob.fKnobRotate);
         m.MultiplyRight (&m2);
         m2.Translation (m_DoorKnob.pKnob.p[0], m_DoorKnob.pKnob.p[1], m_DoorKnob.pKnob.p[2]);
         m.MultiplyRight (&m2);
      }

      // direction of the other end
      // pRight = vector to other side. (It's only the right if doorknob on left)
      CPoint pRight, pFront, pOpposite, pUp;
      pFront.Zero();
      pFront.p[1] = (dws ? 1  : -1);
      pOpposite.Copy (&m_DoorKnob.pOpposite);
      pRight.Subtract (&pOpposite, &m_DoorKnob.pKnob);
      pRight.Normalize();
      pUp.CrossProd (&pFront, &pRight);
      pUp.Normalize();
      pUp.p[2] = fabs(pUp.p[2]); // set certain direciton for this
      pUp.p[0] = fabs(pUp.p[0]); // set certain direction for this

      PCNoodle pn;
      PCRevolution pr;
      BOOL fLever, fLock;
      CPoint pStart, pEnd, pScale, pt, pt2;
      CPoint paScale[2], paLoc[7];
      fp fScale;
      DWORD adwCurve[7];
      DWORD i;
      pStart.Zero();
      pEnd.Zero();
      pScale.Zero();
      switch (m_DoorKnob.adwStyle[dws]) {
      case DKS_CABINET1:       // small D handle 1
      case DKS_CABINET2:       // larger D handle
      case DKS_CABINET7:       // classic handle
         pt.Copy (&pUp);
         fScale = (m_DoorKnob.adwStyle[dws] != DKS_CABINET2) ? .66 : 1;
         pt.Scale (fScale *.08);

         for (i = 0; i < 5; i++) {
            paLoc[i].Copy (&m_DoorKnob.pKnob);
            paLoc[i].p[1] = pFront.p[1] * (m_fThickness/2 + (((i > 0) && (i < 4)) ? (fScale * .04) : 0) );
            if (i < 2)
               paLoc[i].Subtract (&pt);
            if (i > 2)
               paLoc[i].Add (&pt);
         }
         pn = new CNoodle;
         if (!pn)
            continue;
         pn->DrawEndsSet (FALSE);
         pn->FrontVector (&pRight);
         pn->PathSpline (FALSE, 5, paLoc, (DWORD*)SEGCURVE_ELLIPSENEXT, 1);
         pScale.p[0] = fScale *.01;
         pScale.p[1] = fScale *.02;
         pn->ScaleVector (&pScale);
         pn->ShapeDefault (NS_CIRCLEFAST);
         if (fRotate)
            pn->MatrixSet (&m);
         re.pNoodle = pn;
         m_lRENDELEM.Add (&re);

         if (m_DoorKnob.adwStyle[dws] == DKS_CABINET7) {
            pStart.Copy (&m_DoorKnob.pKnob);
            pStart.p[1] = pFront.p[1] * m_fThickness/2;
            pScale.Zero();
            pScale.p[0] = fScale * .25;
            pScale.p[1] = fScale * .10;
            pScale.p[2] = .003;

            pr = new CRevolution;
            if (!pr)
               continue;
            pr->BackfaceCullSet (TRUE);
            pr->DirectionSet (&pStart, &pFront, &pUp);
            pr->ProfileSet (RPROF_C);
            pr->RevolutionSet (RREV_CIRCLE);
            pr->ScaleSet (&pScale);
            re.pNoodle = NULL;
            re.pRevolution = pr;
            if (fRotate) {
               re.fApplyMatrix = TRUE;
               re.mRotate.Copy (&m);
            }
            m_lRENDELEM.Add (&re);
         }

         break;

      case DKS_CABINET3:       // small F hangle
      case DKS_CABINET4:       // larger F handle
         fScale = (m_DoorKnob.adwStyle[dws] == DKS_CABINET3) ? .66 : 1;
         pt.Copy (&pUp);
         pt.Scale (fScale *.1);

         CPoint pStart, pEnd;
         pStart.Copy (&m_DoorKnob.pKnob);
         pStart.p[1] = pFront.p[1] * (m_fThickness/2 + fScale * .04);
         pEnd.Copy (&pStart);
         pStart.Add (&pt);
         pEnd.Subtract (&pt);

         pn = new CNoodle;
         if (!pn)
            continue;
         pn->DrawEndsSet (TRUE);
         pn->FrontVector (&pRight);
         pn->PathLinear (&pStart, &pEnd);
         pScale.p[0] = pScale.p[1] = fScale *.02;
         pn->ScaleVector (&pScale);
         pn->ShapeDefault (NS_CIRCLEFAST);
         if (fRotate)
            pn->MatrixSet (&m);
         re.pNoodle = pn;
         m_lRENDELEM.Add (&re);

         // two posts
         for (i = 0; i < 2; i++) {
            pt.Copy (&pUp);
            pt.Scale (fScale *.075 * (i ? -1 : 1));
            pStart.Copy (&m_DoorKnob.pKnob);
            pStart.p[1] = pFront.p[1] * m_fThickness/2;
            pStart.Add (&pt);
            pEnd.Copy (&pStart);
            pEnd.p[1] += pFront.p[1] * fScale * .04;

            pn = new CNoodle;
            if (!pn)
               continue;
            pn->DrawEndsSet (FALSE);
            pn->FrontVector (&pRight);
            pn->PathLinear (&pStart, &pEnd);
            pScale.p[0] = pScale.p[1] = fScale *.01;
            pn->ScaleVector (&pScale);
            pn->ShapeDefault (NS_CIRCLEFAST);
            if (fRotate)
               pn->MatrixSet (&m);
            re.pNoodle = pn;
            m_lRENDELEM.Add (&re);
         }
         
         break;

      case DKS_CABINET5:       // knob
         pStart.Copy (&m_DoorKnob.pKnob);
         pStart.p[1] = pFront.p[1] * m_fThickness/2;
         pScale.Zero();
         pScale.p[0] = pScale.p[1] = .04;
         pScale.p[2] = .05;

         pr = new CRevolution;
         if (!pr)
            continue;
         pr->BackfaceCullSet (TRUE);
         pr->DirectionSet (&pStart, &pFront, &pUp);
         pr->ProfileSet (RPROF_LIGHTGLOBE);
         pr->RevolutionSet (RREV_CIRCLE);
         pr->ScaleSet (&pScale);
         re.pRevolution = pr;
         if (fRotate) {
            re.fApplyMatrix = TRUE;
            re.mRotate.Copy (&m);
         }
         m_lRENDELEM.Add (&re);

         break;

      case DKS_CABINET6:       // loop that put fingers in
         pStart.Copy (&m_DoorKnob.pKnob);
         pStart.p[1] = pFront.p[1] * m_fThickness/2;
         pt.Copy (&pRight);
         pt.Scale (-.025);
         pStart.Add (&pt);
         pScale.Zero();
         pScale.p[0] = .1;
         pScale.p[1] = pScale.p[2] = .05;

         pr = new CRevolution;
         if (!pr)
            continue;
         pr->BackfaceCullSet (FALSE);

         // BUGFIX - So handles on opposite sides of double doors appear
         pEnd.CrossProd (&pRight, &pUp);
         if (pEnd.p[1] > 0) {
            pEnd.Copy (&pUp);
            pEnd.Scale (-1);
         }
         else
            pEnd.Copy (&pUp);
         pr->DirectionSet (&pStart, &pRight, &pEnd);
         pr->ProfileSet (PRROF_CIRCLEHEMI);
         pr->RevolutionSet (RREV_CIRCLEHEMI);
         pr->ScaleSet (&pScale);
         re.pRevolution = pr;
         if (fRotate) {
            re.fApplyMatrix = TRUE;
            re.mRotate.Copy (&m);
         }
         m_lRENDELEM.Add (&re);

         break;



      case DKS_DOORKNOB:        // basic doorknob that twist
      case DKS_DOORKNOBLOCK:        // doorknob with deadbolt lock above it
      case DKS_LEVER:        // doorknob that has a lever
      case DKS_LEVERLOCK:        // lever with a deadbolt lock above it
         fLever = (m_DoorKnob.adwStyle[dws] == DKS_LEVER) || (m_DoorKnob.adwStyle[dws] == DKS_LEVERLOCK);
         fLock = (m_DoorKnob.adwStyle[dws] == DKS_DOORKNOBLOCK) || (m_DoorKnob.adwStyle[dws] == DKS_LEVERLOCK);

         // draw the base of the knob
         pn = new CNoodle;
         if (!pn)
            continue;
         pScale.p[0] = pScale.p[1] = .065;
         pStart.Copy (&m_DoorKnob.pKnob);
         pStart.p[1] = pFront.p[1] * m_fThickness/2;
         pEnd.Copy (&pStart);
         pEnd.p[1] += .01 * pFront.p[1];
         pn->DrawEndsSet (TRUE);
         pn->FrontVector (&pUp);
         pn->PathLinear (&pStart, &pEnd);
         pn->ScaleVector (&pScale);
         pn->ShapeDefault (NS_CIRCLE);
         if (fRotate)
            pn->MatrixSet (&m);
         re.pNoodle = pn;
         m_lRENDELEM.Add (&re);

         // draw the pole
         pn = new CNoodle;
         if (!pn)
            continue;
         pScale.p[0] = pScale.p[1] = .025;
         pStart.Copy (&pEnd); // use old end
         pEnd.Copy (&pStart);
         pEnd.p[1] += (fLever ? .05 : .02) * pFront.p[1];
         pn->DrawEndsSet (FALSE);
         pn->FrontVector (&pUp);
         pn->PathLinear (&pStart, &pEnd);
         pn->ScaleVector (&pScale);
         pn->ShapeDefault (NS_CIRCLE);
         if (fRotate)
            pn->MatrixSet (&m);
         re.pNoodle = pn;
         m_lRENDELEM.Add (&re);

         // knob or lever
         pn = new CNoodle;
         if (!pn)
            continue;
         pStart.Copy (&pEnd);
         if (fLever) {
            pt.Copy (&pRight);
            pt.Scale (-.02);
            pStart.Add (&pt);
            pt.Copy (&pRight);
            pt.Scale (.15);
            pEnd.Add (&pStart, &pt);
            paScale[0].Zero();
            paScale[1].Zero();
            paScale[0].p[0] = paScale[0].p[1] = .02;
            paScale[1].p[0] = .02;
            paScale[1].p[1] = .01;
         }
         else {
            pEnd.Copy (&pStart);
            pEnd.p[1] += .04 * pFront.p[1];
            paScale[0].Zero();
            paScale[1].Zero();
            paScale[0].p[0] = paScale[0].p[1] = .03;
            paScale[1].p[0] = paScale[1].p[1] = .05;
         }
         pn->DrawEndsSet (TRUE);
         pn->FrontVector (&pUp);
         pn->PathLinear (&pStart, &pEnd);
         pn->ShapeDefault (fLever ? NS_RECTANGLE : NS_CIRCLE);
         pn->ScaleSpline (2, paScale, (DWORD*)SEGCURVE_LINEAR, 0);
         if (fRotate)
            pn->MatrixSet (&m);
         re.pNoodle = pn;
         m_lRENDELEM.Add (&re);


         // deadbolt
         if (fLock) {
            pn = new CNoodle;
            if (!pn)
               continue;
            pScale.p[0] = pScale.p[1] = .065;
            pStart.Copy (&pUp);
            pStart.Scale (.15);
            pStart.Add (&m_DoorKnob.pKnob);
            pStart.p[1] = pFront.p[1] * m_fThickness/2;
            pEnd.Copy (&pStart);
            pEnd.p[1] += .03 * pFront.p[1];
            pn->DrawEndsSet (TRUE);
            pn->FrontVector (&pUp);
            pn->PathLinear (&pStart, &pEnd);
            paScale[0].Zero();
            paScale[1].Zero();
            paScale[0].p[0] = paScale[0].p[1] = .065;
            paScale[1].p[0] = paScale[1].p[1] = .05;
            pn->ScaleSpline (2, paScale, (DWORD*)SEGCURVE_LINEAR, 0);
            pn->ShapeDefault (NS_CIRCLE);
            if (fRotate)
               pn->MatrixSet (&m);
            re.pNoodle = pn;
            m_lRENDELEM.Add (&re);
         }
         break;

      case DKS_POCKET:        // small plate with handle used in internal pocket doors
         // point to one corner of plate
         pt.Copy (&pRight);
         pt.Scale (.02);
         pScale.Copy (&pUp);
         pScale.Scale (.05);
         pt.Add (&pScale);
         pStart.Add (&m_DoorKnob.pKnob, &pt);
         pEnd.Subtract (&m_DoorKnob.pKnob, &pt);
         CommitRect (&pStart, &pEnd, (dws ? 1  : -1) * (m_fThickness / 2 + BITMORE),
            DSURF_DOORKNOB, REF_NOSIDES,
            fRotate ? &m : NULL);
         break;

      case DKS_SLIDER:          // small handle used with sliding doors
         pScale.p[0] = .02;
         pScale.p[1] = .04;
         pt.Copy (&pUp);
         pt.Scale (.075);
         pStart.Add (&m_DoorKnob.pKnob, &pt);
         pEnd.Subtract (&m_DoorKnob.pKnob, &pt);
         pEnd.p[1] = pStart.p[1] = pFront.p[1] * (m_fThickness/2 + pScale.p[1]/2);
         pn = new CNoodle;
         if (!pn)
            continue;
         pn->DrawEndsSet (TRUE);
         pn->FrontVector (&pFront);
         pn->PathLinear (&pStart, &pEnd);
         pn->ScaleVector (&pScale);
         pn->ShapeDefault (NS_RECTANGLE);
         if (fRotate)
            pn->MatrixSet (&m);
         re.pNoodle = pn;
         m_lRENDELEM.Add (&re);
         break;

      case DKS_DOORPULL1:       // old-style door handle
         pt.Copy (&pUp);
         pt.Scale (.075);
         for (i = 0; i < 5; i++) {
            paLoc[i].Copy (&m_DoorKnob.pKnob);
            paLoc[i].p[1] = pFront.p[1] * (m_fThickness/2 + (((i > 0) && (i < 4)) ? .04 : 0) );
            if (i < 2)
               paLoc[i].Subtract (&pt);
            if (i > 2)
               paLoc[i].Add (&pt);
         }
         pn = new CNoodle;
         if (!pn)
            continue;
         pn->DrawEndsSet (FALSE);
         pn->FrontVector (&pRight);
         pn->PathSpline (FALSE, 5, paLoc, (DWORD*)SEGCURVE_ELLIPSENEXT, 1);
         pScale.p[0] = .005;
         pScale.p[1] = .03;
         pn->ScaleVector (&pScale);
         pn->ShapeDefault (NS_RECTANGLE);
         if (fRotate)
            pn->MatrixSet (&m);
         re.pNoodle = pn;
         m_lRENDELEM.Add (&re);
         break;

      case DKS_DOORPULL2:       // door handle attached to large plate - industrial
         // plate
         pt.Copy (&pRight);
         pt.Scale (.0625);
         pScale.Copy (&pUp);
         pScale.Scale (.175);
         pt.Add (&pScale);
         pStart.Add (&m_DoorKnob.pKnob, &pt);
         pEnd.Subtract (&m_DoorKnob.pKnob, &pt);
         CommitRect (&pStart, &pEnd, (dws ? 1  : -1) * (m_fThickness / 2 + BITMORE),
            DSURF_DOORKNOB, REF_NOSIDES, fRotate ? &m : NULL);

         // handle
         pt.Copy (&pUp);
         pt.Scale (.125);
         for (i = 0; i < 4; i++) {
            paLoc[i].Copy (&m_DoorKnob.pKnob);
            paLoc[i].p[1] = pFront.p[1] * (m_fThickness/2 + (((i > 0) && (i < 3)) ? .08 : 0) );
            if (i < 2)
               paLoc[i].Subtract (&pt);
            else
               paLoc[i].Add (&pt);
         }
         pn = new CNoodle;
         if (!pn)
            continue;
         pn->DrawEndsSet (FALSE);
         pn->FrontVector (&pRight);
         pn->PathSpline (FALSE, 4, paLoc, (DWORD*)SEGCURVE_LINEAR, 1);
         pScale.p[0] = pScale.p[1] = .04;
         pn->ScaleVector (&pScale);
         pn->ShapeDefault (NS_CIRCLEFAST);
         if (fRotate)
            pn->MatrixSet (&m);
         re.pNoodle = pn;
         m_lRENDELEM.Add (&re);
         break;

      case DKS_DOORPULL3:       // piece of bent flat metal that's door handle
         // handle
         pt.Copy (&pRight);
         pt.Scale (.1);
         for (i = 0; i < 3; i++) {
            paLoc[i].Copy (&m_DoorKnob.pKnob);
            paLoc[i].p[1] = pFront.p[1] * (m_fThickness/2 + ((i > 0) ? .06 : 0) );
            if (i >= 2)
               paLoc[i].Add (&pt);
         }
         pn = new CNoodle;
         if (!pn)
            continue;
         pn->DrawEndsSet (FALSE);
         pt.Copy (&pRight);
         pt.Scale (-1);
         pt.Add (&pFront);
         pn->FrontVector (&pt);
         pn->PathSpline (FALSE, 3, paLoc, (DWORD*)SEGCURVE_LINEAR, 1);
         pScale.p[0] = .25;
         pScale.p[1] = .01;
         pn->ScaleVector (&pScale);
         pn->ShapeDefault (NS_RECTANGLE);
         if (fRotate)
            pn->MatrixSet (&m);
         re.pNoodle = pn;
         m_lRENDELEM.Add (&re);
         break;

      case DKS_DOORPULL4:       // bar bent in square config to pull
      case DKS_DOORPULL5:       // bar bent in loop to pull
         BOOL fCurve;
         fCurve = (m_DoorKnob.adwStyle[dws] == DKS_DOORPULL5);
         // handle
         pt.Copy (&pUp);
         pt.Scale (.125);
         pt2.Copy (&pRight);
         pt2.Scale (fCurve ? .15 : .1);
         for (i = 0; i < 7; i++) {
            paLoc[i].Copy (&m_DoorKnob.pKnob);
            paLoc[i].p[1] = pFront.p[1] * (m_fThickness/2 + (((i > 0) && (i < 6)) ? .08 : 0) );
            if (i < 3)
               paLoc[i].Subtract (&pt);
            else if (i > 3)
               paLoc[i].Add (&pt);
            if ((i > 1) && (i < 5))
               paLoc[i].Add (&pt2);

            adwCurve[i] = SEGCURVE_LINEAR;
            if (fCurve && (i > 0) && (i < 6))
               adwCurve[i] = (i%2) ?  SEGCURVE_ELLIPSENEXT : SEGCURVE_ELLIPSEPREV;
         }
         pn = new CNoodle;
         if (!pn)
            continue;
         pn->DrawEndsSet (FALSE);
         pt.Copy (&pRight);
         pt.Scale (-1);
         pt.Add (&pFront);
         pn->FrontVector (&pt);
         pn->PathSpline (FALSE, 7, paLoc, adwCurve, 1);
         pScale.p[0] = pScale.p[1] = .04;
         pn->ScaleVector (&pScale);
         pn->ShapeDefault (NS_CIRCLEFAST);
         if (fRotate)
            pn->MatrixSet (&m);
         re.pNoodle = pn;
         m_lRENDELEM.Add (&re);
         break;

      case DKS_DOORPULL6:       // long horizontal across door
         // handle
         for (i = 0; i < 4; i++) {
            paLoc[i].Copy ((i < 2) ? &m_DoorKnob.pKnob : &m_DoorKnob.pOpposite);
            paLoc[i].p[1] = pFront.p[1] * (m_fThickness/2 + (((i > 0) && (i < 3)) ? .08 : 0) );
         }
         pn = new CNoodle;
         if (!pn)
            continue;
         pn->DrawEndsSet (FALSE);
         pn->FrontVector (&pUp);
         pn->PathSpline (FALSE, 4, paLoc, (DWORD*)SEGCURVE_LINEAR, 1);
         pScale.p[0] = pScale.p[1] = .04;
         pn->ScaleVector (&pScale);
         pn->ShapeDefault (NS_CIRCLEFAST);
         if (fRotate)
            pn->MatrixSet (&m);
         re.pNoodle = pn;
         m_lRENDELEM.Add (&re);
         break;

      case DKS_DOORPULL7:       // long handle up and down door
         // handle
         pt.Copy (&pUp);
         pt.Scale (.5);
         for (i = 0; i < 4; i++) {
            paLoc[i].Copy (&m_DoorKnob.pKnob);
            paLoc[i].p[1] = pFront.p[1] * (m_fThickness/2 + (((i > 0) && (i < 3)) ? .08 : 0) );
            if (i < 2)
               paLoc[i].Subtract (&pt);
            else
               paLoc[i].Add (&pt);
         }
         pn = new CNoodle;
         if (!pn)
            continue;
         pn->DrawEndsSet (FALSE);
         pn->FrontVector (&pRight);
         pn->PathSpline (FALSE, 4, paLoc, (DWORD*)SEGCURVE_LINEAR, 1);
         pScale.p[0] = pScale.p[1] = .04;
         pn->ScaleVector (&pScale);
         pn->ShapeDefault (NS_CIRCLEFAST);
         if (fRotate)
            pn->MatrixSet (&m);
         re.pNoodle = pn;
         m_lRENDELEM.Add (&re);
         break;

      case DKS_DOORPULL8:       // diagonal across door
         // handle
         for (i = 0; i < 4; i++) {
            paLoc[i].Copy ((i < 2) ? &m_DoorKnob.pKnob : &m_DoorKnob.pOpposite);
            paLoc[i].p[1] = pFront.p[1] * (m_fThickness/2 + (((i > 0) && (i < 3)) ? .08 : 0) );

            // make it diagonal
            if (i >= 2) {
               fp fLen;
               pt.Subtract (&m_DoorKnob.pKnob, &m_DoorKnob.pOpposite);
               fLen = pt.Length();
               pt.Copy (&pUp);
               pt.Scale (-fLen/2);
               paLoc[i].Add (&pt);
            }
         }
         pn = new CNoodle;
         if (!pn)
            continue;
         pn->DrawEndsSet (FALSE);
         pn->FrontVector (&pUp);
         pn->PathSpline (FALSE, 4, paLoc, (DWORD*)SEGCURVE_LINEAR, 1);
         pScale.p[0] = pScale.p[1] = .04;
         pn->ScaleVector (&pScale);
         pn->ShapeDefault (NS_CIRCLEFAST);
         if (fRotate)
            pn->MatrixSet (&m);
         re.pNoodle = pn;
         m_lRENDELEM.Add (&re);
         break;

      case DKS_DOORPUSH1:       // plate opposite hinged side of door that piush on
         // plate
         pt.Copy (&pRight);
         pt.Scale (.0625);
         pScale.Copy (&pUp);
         pScale.Scale (.175);
         pt.Add (&pScale);
         pStart.Add (&m_DoorKnob.pKnob, &pt);
         pEnd.Subtract (&m_DoorKnob.pKnob, &pt);
         CommitRect (&pStart, &pEnd, (dws ? 1  : -1) * (m_fThickness / 2 + BITMORE),
            DSURF_DOORKNOB, REF_NOSIDES, fRotate ? &m : NULL);
         break;

      case DKS_DOORPUSH2:       // horizontal push-bar across door
         pScale.p[0] = .08;
         pScale.p[1] = .08;
         pn = new CNoodle;
         if (!pn)
            continue;
         pn->DrawEndsSet (TRUE);
         pn->FrontVector (&pFront);
         pStart.Copy (&m_DoorKnob.pKnob);
         pEnd.Copy (&m_DoorKnob.pOpposite);
         pEnd.p[1] = pStart.p[1] = pFront.p[1] * (m_fThickness/2 + pScale.p[1]/2);
         pn->PathLinear (&pStart, &pEnd);
         pn->ScaleVector (&pScale);
         pn->ShapeDefault (NS_RECTANGLE);
         if (fRotate)
            pn->MatrixSet (&m);
         re.pNoodle = pn;
         m_lRENDELEM.Add (&re);
         break;
      }
   }

   // kickboard
   for (dws = 0; dws < 2; dws++) {
      if (!m_DoorKnob.afKick[dws])
         continue;

      // find the min and max of the shape
      CPoint pMin, pMax;
      PCPoint pp;
      DWORD i;
      for (i = 0; i < m_lShape.Num(); i++) {
         pp = (PCPoint) m_lShape.Get(i);
         if (i) {
            pMin.Min (pp);
            pMax.Max (pp);
         }
         else {
            pMin.Copy (pp);
            pMax.Copy (pp);
         }
      }
      pMin.p[0] += m_DoorKnob.fKickIndent;
      pMax.p[0] -= m_DoorKnob.fKickIndent;
      pMin.p[2] += m_DoorKnob.fKickIndent;
      pMax.p[2] = pMin.p[2] + m_DoorKnob.fKickHeight;

      CommitRect (&pMin, &pMax, (dws ? 1  : -1) * (m_fThickness / 2 + BITMORE),
         DSURF_DOORKNOB, REF_NOSIDES);
   }
}

/**********************************************************************************
CDoor::CalcIfNecessary - If the dirty flag is set then this recalculates all the
necessary noodles and polygons.
*/
void CDoor::CalcIfNecessary (void)
{
   // if not dirty dont do anything
   if (!m_fDirty)
      return;

   // free render elements
   DWORD i;
   for (i = 0; i < m_lRENDELEM.Num(); i++) {
      PRENDELEM pr = (PRENDELEM) m_lRENDELEM.Get(i);
      if (pr->pNoodle)
         delete pr->pNoodle;
      if (pr->pRevolution)
         delete pr->pRevolution;
      if (pr->pPoly)
         delete pr->pPoly;
      if (pr->pPanel)
         delete pr->pPanel;
   }
   m_lRENDELEM.Clear();

   if (m_pDoorDiv)
      m_pDoorDiv->Commit (m_lShape.Num(), (PCPoint) m_lShape.Get(0), this);

   CalcDoorKnob ();

   // find out what colors necessary
   m_fDirty = FALSE;
   m_dwSurface = 0;
   for (i = 0; i < m_lRENDELEM.Num(); i++) {
      PRENDELEM pr = (PRENDELEM) m_lRENDELEM.Get(i);
      m_dwSurface |= pr->dwSurface;
   }
}


/********************************************************************************
DivParamToType - Given a door parameter, returns a type.

inputs
   DWORD       dwDivType - Division type
   DWORD       dwParam - parameter number
retursn
   DWORD - type...0 = measure, 1=number, 2=DDCUT_ list, 3=DDMS_ list, 4=DDCB_ list,
      5=DSURF_ list, 6=percent, 7 = bool, 8=brace angle
*/
DWORD DivParamToType (DWORD dwDivType, DWORD dwParam)
{
   DWORD dwType = 0; // 0 = measure, 1=number, 2=DDCUT_ list, 3=DDMS_ list, 4=DDCB_ list, 5=DSURF_ list, 6=percent, 7 = bool, 8=brace angle
   switch (dwDivType) {
   case DDT_FRAME:
   case DDT_FRAMETB:
   case DDT_FRAMELR:
      switch (dwParam) {
      case 0:
         // leave dwtype
         break;
      case 1:
         dwType = 5;
         break;
      }
      break;
      break;
   case DDT_PANEL:
      break;
   case DDT_SOLID:
      switch (dwParam) {
      case 0:
         dwType = 6;
         break;
      case 1:
         dwType = 5;
         break;
      }
      break;
   case DDT_DIVIDEMULTIH:
   case DDT_DIVIDEMULTIV:
      switch (dwParam) {
      case 0:
         break;
      case 1:
         dwType = 1;
         break;
      case 2:
         dwType = 5;
         break;
      }
      break;
   case DDT_DIVIDEFIXEDH:
   case DDT_DIVIDEFIXEDV:
      switch (dwParam) {
      case 0:
         break;
      case 1:
         dwType = 6;
         break;
      case 2:
         dwType = 5;
         break;
      }
      break;
   case DDT_CUTOUT:
      switch (dwParam) {
      case 0:
         dwType = 6;
         break;
      case 1:
         dwType = 2;
         break;
      case 2:
         dwType = 6;
         break;
      case 3:
         dwType = 6;
         break;
      case 4:
         dwType = 7;
         break;
      case 5:
         dwType = 6;
         break;
      case 6:
         dwType = 6;
         break;
      }
      break;
   case DDT_GLASS:
      switch (dwParam) {
      case 0:
         dwType = 5;
         break;
      case 1:
         dwType = 3;
         break;
      case 2:
         break;
      case 3:
         break;
      case 4:
         break;
      case 5:
         dwType = 6;
         break;
      case 6:
         dwType = 6;
         break;
      case 7:
         dwType = 5;
         break;
      }
      break;
   case DDT_CROSSBRACE:
      switch (dwParam) {
      case 0:
         break;
      case 1:
         dwType = 6;
         break;
      case 2:
         dwType = 7;
         break;
      case 3:
         dwType = 4;
         break;
      case 4:
         dwType = 8;
         break;
      }
      break;
   }

   return dwType;
}

/********************************************************************************
CDoor::DoorDivToControls - Set the controsl from DoorDivMML

inputs
   PCDoorDiv   pDiv - Divider
   PWSTR       pszID - String that identifies the door up to this point. Basically,
               an array of '0'..'9' for the particular m_apDiv to take down the tree.
   PCEscPage   pPage - page
*/
void CDoor::DoorDivToControls (PCDoorDiv pDiv, PWSTR pszID, PCEscPage pPage)
{
   WCHAR szTemp[128];

   // type
   wcscpy (szTemp, L"divtype");
   wcscat (szTemp, pszID);
   ComboBoxSet (pPage, szTemp, pDiv->m_dwType);

   DWORD dwParam;
   for (dwParam = 0; dwParam < pDiv->ParamNum(); dwParam++) {
      DWORD dwType = DivParamToType (pDiv->m_dwType, dwParam);

      swprintf (szTemp, L"param%s:%d", pszID, (int) dwParam);

      PCEscControl pControl;
      pControl = pPage->ControlFind (szTemp);
      if (!pControl)
         continue;

      switch (dwType) {
      case 0:  // distance
         MeasureToString (pPage, szTemp, pDiv->m_afParam[dwParam]);
         break;
      case 1:  // number
         DoubleToControl (pPage, szTemp, pDiv->m_afParam[dwParam]);
         break;
      case 2:  // DDCUT_
      case 3:  // DDMS_
      case 4:  // DDCB_
      case 5:  // DDSURF_
      case 8:  // brace angle
         ComboBoxSet (pPage, szTemp, (DWORD)pDiv->m_afParam[dwParam]);
         break;
      case 6:  // percent
         DoubleToControl (pPage, szTemp, pDiv->m_afParam[dwParam] * 100.0);
         break;
      case 7:  // boolean
         pControl->AttribSetBOOL (Checked(), (BOOL)(int) pDiv->m_afParam[dwParam]);
         break;
      }
   }


   // Display subframes
   DWORD i;
   for (i = 0; i < pDiv->DivNum(); i++) {
      // shouldnt happen
      if (!pDiv->m_apDiv[i])
         continue;

      DWORD dwLen;
      wcscpy (szTemp, pszID);
      dwLen = (DWORD)wcslen(szTemp);
      szTemp[dwLen] = '0' + (WCHAR)i;
      szTemp[dwLen+1] = 0;
      DoorDivToControls (pDiv->m_apDiv[i], szTemp, pPage);
   }

}

/********************************************************************************
CDoor::DoorDivMML - Appends MML to the door given the divider.

inputs
   PCMem       pm - memory to add to
   PCDoorDiv   pDiv - Divider
   PWSTR       pszID - String that identifies the door up to this point. Basically,
               an array of '0'..'9' for the particular m_apDiv to take down the tree.
*/
void CDoor::DoorDivMML (PCMem pm, PCDoorDiv pDiv, PWSTR pszID)
{
   MemCat (pm, L"<xComboDivType width=100%% name=divtype");
   MemCat (pm, pszID);
   MemCat (pm, L"/><br/>");

   switch (pDiv->m_dwType) {
   case DDT_FRAME:
      MemCat (pm, L"<bold>Frame, around entire section</bold> - "
         L"Draws a frame around the entire section.");
      break;
   case DDT_SOLID:
      MemCat (pm, L"<bold>Panel, solid</bold> - "
         L"The section is filled with a solid panel without any elevations.");
      break;
   case DDT_FRAMETB:
      MemCat (pm, L"<bold>Frame, on top and bottom</bold> - "
         L"Framing that only occurs on the top and bottom of the section.");
      break;
   case DDT_FRAMELR:
      MemCat (pm, L"<bold>Frame, on left and right</bold> - "
         L"Framing that only occurs on the left and right of the section.");
      break;
   case DDT_VOID:
      MemCat (pm, L"<bold>Open</bold> - "
         L"The section is completely open.");
      break;
   case DDT_DIVIDEMULTIH:
      MemCat (pm, L"<bold>Dividers, horizontal</bold> - "
         L"The section is divided by one or more horizontal framing members.");
      break;
   case DDT_DIVIDEMULTIV:
      MemCat (pm, L"<bold>Dividers, vertical</bold> - "
         L"The section is divided by one or more vertical framing members.");
      break;
   case DDT_DIVIDEFIXEDH:
      MemCat (pm, L"<bold>Divider, horizontal</bold> - "
         L"The section is divided by one horizontal framing member places at a specific elevation.");
      break;
   case DDT_DIVIDEFIXEDV:
      MemCat (pm, L"<bold>Divider, vertical</bold> - "
         L"The section is divided by one vertical framing member places at a specific distance from the left edge.");
      break;
   case DDT_PANEL:
      MemCat (pm, L"<bold>Panel, raised</bold> - "
         L"The section is filled with a raised panel.");
      break;
   case DDT_CUTOUT:
      MemCat (pm, L"<bold>Panel, with cutout</bold> - "
         L"The section is filled with a solid panel. A shape (such as a circle) is cut out of the shape.");
      break;
   case DDT_GLASS:
      MemCat (pm, L"<bold>Glass</bold> - "
         L"The section is filled with glass (or other material) and (optionally) divided into lites.");
      break;
   case DDT_CROSSBRACE:
      MemCat (pm, L"<bold>Bracing</bold> - "
         L"Crossbracing is applied over the section.");
      break;
   }
   MemCat (pm, L"<p/>");

   if (pDiv->ParamNum()) {
      // Display controls
      MemCat (pm, L"<table width=100%%>");

      DWORD dwParam;
      for (dwParam = 0; dwParam < pDiv->ParamNum(); dwParam++) {
         PWSTR psz = NULL;
         DWORD dwType = DivParamToType (pDiv->m_dwType, dwParam);

         PWSTR pszFrameWidth = L"<bold>Frame width</bold>";
         PWSTR pszFrameDepth = L"<bold>Panel thickness</bold> - Thickness as a percent of the door thickness.";
         PWSTR pszColor = L"<bold>Material/color</bold>";

         switch (pDiv->m_dwType) {
         case DDT_FRAME:
         case DDT_FRAMETB:
         case DDT_FRAMELR:
         case DDT_PANEL:
            switch (dwParam) {
            case 0:
               psz = pszFrameWidth;
               break;
            case 1:
               psz = pszColor;
               break;
            }
            break;
         case DDT_SOLID:
            switch (dwParam) {
            case 0:
               psz = pszFrameDepth;
               break;
            case 1:
               psz = pszColor;
               break;
            }
            break;
         case DDT_DIVIDEMULTIH:
         case DDT_DIVIDEMULTIV:
            switch (dwParam) {
            case 0:
               psz = pszFrameWidth;
               break;
            case 1:
               psz = L"<bold>Divisons</bold> - Number of divisions to make.";
               break;
            case 2:
               psz = pszColor;
               break;
            }
            break;
         case DDT_DIVIDEFIXEDH:
         case DDT_DIVIDEFIXEDV:
            switch (dwParam) {
            case 0:
               psz = pszFrameWidth;
               break;
            case 1:
               psz = L"<bold>Offset percent</bold> - Divider is places this far (in percent) up the section.";
               break;
            case 2:
               psz = pszColor;
               break;
            }
            break;
         case DDT_CUTOUT:
            switch (dwParam) {
            case 0:
               psz = pszFrameDepth;
               break;
            case 1:
               psz = L"<bold>Hole shape</bold>";
               break;
            case 2:
               psz = L"<bold>Width as percent</bold> - Width of the hole as a percent if the section width.";
               break;
            case 3:
               psz = L"<bold>Height as percent</bold> - Height of the hole as a percent if the section height.";
               break;
            case 4:
               psz = L"<bold>Keep the opening square</bold><br/>If the width and height are different then this uses the smallest of "
                  L"the two for both the width and height.";
               break;
            case 5:
               psz = L"<bold>LR offset as percent</bold> - Offset the opening this far into the section horizontally.";
               break;
            case 6:
               psz = L"<bold>UD offset as percent</bold> - Offset the opening this far into the section vertically.";
               break;
            }
            break;
         case DDT_GLASS:
            switch (dwParam) {
            case 0:
               psz = pszColor;
               break;
            case 1:
               psz = L"<bold>Mullion orientation</bold>";
               break;
            case 2:
               psz = L"<bold>Mullion width</bold>";
               break;
            case 3:
               psz = L"<bold>Lite width</bold> - Horizontal distance between mullions.";
               break;
            case 4:
               psz = L"<bold>Lite height</bold> - Vertical distance between mullions.";
               break;
            case 5:
               psz = L"<bold>LR offset as percent</bold> - Horizontal offset of the radiating mullion style.";
               break;
            case 6:
               psz = L"<bold>UD offset as percent</bold> - Vertical offset of the radiating mullion style.";
               break;
            case 7:
               psz = L"<bold>Mullion color</bold>";
               break;
            }
            break;
         case DDT_CROSSBRACE:
            switch (dwParam) {
            case 0:
               psz = pszFrameWidth;
               break;
            case 1:
               psz = L"<bold>Bracing thickness</bold> - As a percentage of the door width.";
               break;
            case 2:
               psz = L"<bold>Bracing on back side</bold><br/>"
                  L"Check this to place the bracing on the back side of the door instead of the front.";
               break;
            case 3:
               psz = L"<bold>Bracing shape</bold>";
               break;
            case 4:
               psz = L"<bold>Bracing rotation</bold>";
               break;
            }
            break;
         }
         if (!psz)
            continue;

         if (dwType == 7) {
            MemCat (pm, L"<tr><td><xchoicebutton style=x checkbox=true name=param");
            MemCat (pm, pszID);
            MemCat (pm, L":");
            MemCat (pm, (int) dwParam);
            MemCat (pm, L">");
            MemCat (pm, psz);
            MemCat (pm, L"</xchoicebutton></td></tr>");
         }
         else {
            MemCat (pm, L"<tr><td>");
            MemCat (pm, psz);
            MemCat (pm, L"</td><td>");
            WCHAR szName[128];
            swprintf (szName, L"param%s:%d", pszID, (int) dwParam);
            switch (dwType) {
            case 0:  // distance
            case 1:  // number
            case 6:  // percent
               MemCat (pm, L"<edit maxchars=32 width=100%% name=");
               break;
            case 2:  // DDCUT_
               MemCat (pm, L"<xComboDDCUT name=");
               break;
            case 3:  // DDMS_
               MemCat (pm, L"<xComboDDMS name=");
               break;
            case 4:  // DDCB_
               MemCat (pm, L"<xComboDDCB name=");
               break;
            case 5:  // DDSURF_
               MemCat (pm, L"<xComboDDSURF name=");
               break;
            case 8:  // brace angle
               MemCat (pm, L"<xComboBAngle name=");
               break;
            }
            MemCat (pm, szName);
            MemCat (pm, L"/>");
            MemCat (pm, L"</td></tr>");
         }
      }

      MemCat (pm, L"</table><p/>");
   }

   // Display subframes
   DWORD i;
   for (i = 0; i < pDiv->DivNum(); i++) {
      // shouldnt happen
      if (!pDiv->m_apDiv[i])
         continue;

      MemCat (pm, L"<xtablecenter width=100%%>");
      MemCat (pm, L"<xtrheader>");
      switch (pDiv->m_dwType) {
      case DDT_DIVIDEFIXEDH:
         MemCat (pm, i ? L"Upper sub-section" : L"Lower sub-section");
         break;
      case DDT_DIVIDEFIXEDV:
         MemCat (pm, i ? L"Left sub-section" : L"Right sub-section");
         break;
      default:
         MemCat (pm, L"Sub-section");
         break;
      }
      MemCat (pm, L"</xtrheader>");
      MemCat (pm, L"<tr><td>");

      WCHAR szTemp[64];
      DWORD dwLen;
      wcscpy (szTemp, pszID);
      dwLen = (DWORD)wcslen(szTemp);
      szTemp[dwLen] = '0' + (WCHAR)i;
      szTemp[dwLen+1] = 0;
      DoorDivMML (pm, pDiv->m_apDiv[i], szTemp);

      MemCat (pm, L"</td></tr>");
      MemCat (pm, L"</xtablecenter>");

   }

}


/**************************************************************************************
ParseDivTreeString - Given the string of 0..9's that point down the tree, this parses
it and follows the tree down to the right division. Returns the PCDoorDiv that is named.

inputs
   PWSTR       pszString - sting starting at 0..9. Will stop when get to non-number
   PCDoorDiv   pRoot - Root division
returns
   PCDoorDiv - Division pointeed to
*/
PCDoorDiv ParseDivTreeString (PWSTR pszString, PCDoorDiv pRoot)
{
   while ((pszString[0] >= L'0') && (pszString[0] <= L'9')) {
      DWORD dwIndex = (DWORD) (pszString[0] - L'0');
      if (dwIndex >= pRoot->DivNum())
         return NULL;
      pRoot = pRoot->m_apDiv[dwIndex];
      if (!pRoot)
         return NULL;
      pszString++;
   }

   return pRoot;
}

/**************************************************************************************
ParseParamName - Given a name of a control, this parses it to figure out if it's a parameter
control from DoorDivMML(). If not, returns NULL. If TRUE, returns a pointer to the attribute
fp that can be modified.

inputs
   PWSTR       pszControl - control name
   PCDoorDiv   pRoot - Root division
   DWORD       *pdwType - Filled with the type from DivParamToType()
returns
   fp * - Attribute to be modified.
*/
fp *ParseParamName (PWSTR pszControl, PCDoorDiv pRoot, DWORD *pdwType)
{
   PWSTR pszParam = L"param";
   DWORD dwLen = (DWORD)wcslen(pszParam);
   if (!pszControl)
      return NULL;   // no string

   if (wcsncmp(pszControl, pszParam, dwLen))
      return NULL;   // not parameterr

   // get the root
   pRoot = ParseDivTreeString (pszControl + dwLen, pRoot);
   if (!pRoot)
      return NULL;

   // attrib number
   DWORD dwNum;
   PWSTR psz;
   psz = wcschr (pszControl + dwLen, L':');
   if (!psz)
      return NULL;
   dwNum = _wtoi(psz+1);
   if (dwNum >= pRoot->ParamNum())
      return NULL;
   *pdwType = DivParamToType (pRoot->m_dwType, dwNum);
   return &pRoot->m_afParam[dwNum];
}

/***************************************************************************
UpdateDoorSet - Tells the doorset of the change.

inputs
   PDDoor         pDoor - Door division that just changed
   PCDoorSet      pSet - Door set to notify
   DWORD          dwIndex - Index, or -1 if all
*/
void UpdateDoorSet (PCDoor pDoor, PCDoorSet pSet, DWORD dwIndex)
{
   if (!pSet)
      return;

   DWORD dwNum, i;
   dwNum = pSet->DoorNum();
   if (dwIndex < pSet->DoorNum())
      pSet->DoorSet (dwIndex, pDoor);
   else {
      // else loop over all
      for (i = 0; i < dwNum; i++)
         pSet->DoorSet (i, pDoor);
   }
}

/* DoorCustomPage
*/
BOOL DoorCustomPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PDCP pdcp = (PDCP) pPage->m_pUserData;
   PCDoor pv = pdcp->pv;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         // Handle scroll on redosamepage
         if (pdcp->iVScroll >= 0) {
            pPage->VScroll (pdcp->iVScroll);

            // when bring up pop-up dialog often they're scrolled wrong because
            // iVScoll was left as valeu, and they use defpage
            pdcp->iVScroll = 0;

            // BUGFIX - putting this invalidate in to hopefully fix a refresh
            // problem when add or move a task in the ProjectView page
            pPage->Invalidate();
         }

         // Set up values
         pv->DoorDivToControls (pv->m_pDoorDiv, L"", pPage);

         // set the list box
         ListBoxSet (pPage, L"doortype", pv->m_dwStyle);
      }
      break;

   case ESCN_LISTBOXSELCHANGE:
      {
         PESCNLISTBOXSELCHANGE p = (PESCNLISTBOXSELCHANGE) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;
         PWSTR psz;
         psz = p->pControl->m_pszName;

         if (!_wcsicmp(psz, L"doortype")) {
            DWORD dwOld = pv->m_dwStyle;
            DWORD dwNew = p->pszName ? _wtoi(p->pszName) : 0;
            if (dwOld == dwNew)
               break;   // no change

            // else change
            // else modify this
            if (pdcp->pWorld && pdcp->pThis)
               pdcp->pWorld->ObjectAboutToChange (pdcp->pThis);

            pv->StyleSet (dwNew);

            if (pdcp->pObjectDoor)
               pdcp->pObjectDoor->m_fDirty = TRUE;   // set to dirty so will recacl colors

            // Tell the doorset
            UpdateDoorSet (pv, pdcp->pDoorSet, pdcp->dwDoorNum);

            if (pdcp->pWorld && pdcp->pThis)
               pdcp->pWorld->ObjectChanged (pdcp->pThis);

            // may want to refresh
            if (!dwOld || !dwNew)
               pPage->Exit (RedoSamePage());

            return TRUE;
         }
      }
      break;

   case ESCN_COMBOBOXSELCHANGE:
      {
         PESCNCOMBOBOXSELCHANGE p = (PESCNCOMBOBOXSELCHANGE) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;
         PWSTR psz;
         psz = p->pControl->m_pszName;

         // see if it's a parameter
         fp *pfModify;
         DWORD dwType;
         pfModify = ParseParamName (psz, pv->m_pDoorDiv, &dwType);
         if (pfModify) {
            DWORD dwVal = p->pszName ? _wtoi(p->pszName) : 0;
            if (dwVal == (DWORD) (*pfModify))
               break;   // no change

            // else modify this
            if (pdcp->pWorld && pdcp->pThis)
               pdcp->pWorld->ObjectAboutToChange (pdcp->pThis);

            *pfModify = dwVal;

            // make sure it's custom
            if (pv->m_dwStyle != DS_CUSTOM) {
               pv->m_dwStyle = DS_CUSTOM;
            }

            if (pdcp->pObjectDoor)
               pdcp->pObjectDoor->m_fDirty = TRUE;   // set to dirty so will recacl colors

            // Tell the doorset
            UpdateDoorSet (pv, pdcp->pDoorSet, pdcp->dwDoorNum);

            if (pdcp->pWorld && pdcp->pThis)
               pdcp->pWorld->ObjectChanged (pdcp->pThis);
            return TRUE;
         }

         PWSTR pszDivType = L"divtype";
         DWORD dwDivType = (DWORD)wcslen(pszDivType);
         if (!wcsncmp (psz, pszDivType, dwDivType)) {
            // changed the division type
            PCDoorDiv pDiv = ParseDivTreeString (psz + dwDivType, pv->m_pDoorDiv);
            DWORD dwVal = p->pszName ? _wtoi(p->pszName) : 0;
            if (!pDiv || (dwVal == pDiv->m_dwType))
               break;   // no change

            // else modify this
            if (pdcp->pWorld && pdcp->pThis)
               pdcp->pWorld->ObjectAboutToChange (pdcp->pThis);

            // erase any current divisions before change type
            DWORD i;
            DWORD dwOrigNum;
            dwOrigNum = pDiv->DivNum();


            // change type
            pDiv->AutoFill (dwVal, FALSE);

            for (i = pDiv->DivNum(); i < dwOrigNum; i++) {
               if (pDiv->m_apDiv[i])
                  delete pDiv->m_apDiv[i];
               pDiv->m_apDiv[i] = NULL;
            }
            // fill in any openings with blanks
            for (i = 0; i < pDiv->DivNum(); i++) {
               if (!pDiv->m_apDiv[i]) {
                  pDiv->m_apDiv[i] = new CDoorDiv;
                  if (!pDiv->m_apDiv[i])
                     continue;
                  pDiv->m_apDiv[i]->AutoFill (DDT_VOID, FALSE);
               }
            }

            // make sure it's custom
            if (pv->m_dwStyle != DS_CUSTOM) {
               pv->m_dwStyle = DS_CUSTOM;
            }

            if (pdcp->pObjectDoor)
               pdcp->pObjectDoor->m_fDirty = TRUE;   // set to dirty so will recacl colors

            // Tell the doorset
            UpdateDoorSet (pv, pdcp->pDoorSet, pdcp->dwDoorNum);

            if (pdcp->pWorld && pdcp->pThis)
               pdcp->pWorld->ObjectChanged (pdcp->pThis);

            pPage->Exit (RedoSamePage());
            return TRUE;
         }
      }

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;

         if (!p->pControl->m_pszName)
            break;
         PWSTR psz;
         psz = p->pControl->m_pszName;
         fp *pfModify;
         DWORD dwType;
         pfModify = ParseParamName (psz, pv->m_pDoorDiv, &dwType);
         if (!pfModify)
            break;

         // else modify this
         if (pdcp->pWorld && pdcp->pThis)
            pdcp->pWorld->ObjectAboutToChange (pdcp->pThis);

         *pfModify = p->pControl->AttribGetBOOL (Checked()) ? 1.0 : 0;

         // make sure it's custom
         if (pv->m_dwStyle != DS_CUSTOM) {
            pv->m_dwStyle = DS_CUSTOM;
         }

         // Tell the doorset
         UpdateDoorSet (pv, pdcp->pDoorSet, pdcp->dwDoorNum);

         if (pdcp->pWorld && pdcp->pThis)
            pdcp->pWorld->ObjectChanged (pdcp->pThis);
         return TRUE;
      }

   case ESCN_EDITCHANGE:
      {
         PESCNEDITCHANGE p = (PESCNEDITCHANGE) pParam;

         // anything that happens here will be one of my edit boxes
         if (!p->pControl || !p->pControl->m_pszName)
            break;
         PWSTR psz;
         psz = p->pControl->m_pszName;
         fp *pfModify;
         DWORD dwType;
         pfModify = ParseParamName (psz, pv->m_pDoorDiv, &dwType);
         if (!pfModify)
            break;

         // else modify this
         if (pdcp->pWorld && pdcp->pThis)
            pdcp->pWorld->ObjectAboutToChange (pdcp->pThis);
         
         // based on the type
         switch (dwType) {
            case 0:  // distance
               MeasureParseString (pPage, psz, pfModify);
               break;
            case 1:  // number
               *pfModify = DoubleFromControl (pPage, psz);
               break;
            case 6:  // percent
               *pfModify = DoubleFromControl (pPage, psz) / 100.0;
               break;
         }

         // make sure it's custom
         if (pv->m_dwStyle != DS_CUSTOM) {
            pv->m_dwStyle = DS_CUSTOM;
         }

         // Tell the doorset
         UpdateDoorSet (pv, pdcp->pDoorSet, pdcp->dwDoorNum);

         if (pdcp->pWorld && pdcp->pThis)
            pdcp->pWorld->ObjectChanged (pdcp->pThis);
         return TRUE;
      }
      break;


   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Door/window selection";
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"IFHELP")) {
            p->pszSubString = pv->m_dwStyle ? L"<comment>" : L"";
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"ENDIFHELP")) {
            p->pszSubString = pv->m_dwStyle ? L"</comment>" : L"";
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"THEPAGE")) {
            MemZero (&gMemTemp);

            if (!pv->m_dwStyle) {
               // dipsplay table around top frame
               MemCat (&gMemTemp, L"<xtablecenter width=100%>"
                  L"<xtrheader>Main section</xtrheader><tr><td>");

               // display the top door frame
               pv->DoorDivMML (&gMemTemp, pv->m_pDoorDiv, L"");
               MemCat (&gMemTemp, L"</td></tr></xtablecenter>");
            }

            p->pszSubString = (PWSTR) gMemTemp.p;
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}



/**********************************************************************************
CDoor::CustomDialog - Brings up UI for modifying a custom door-set. (If you call
with a non-custom door, any changes will cause it to be custom.

inputs
   PCEscWindow    pWindow - Widnow to use
   PCWorldSocket        pWorld - World to notify of changes
   PCObjectSocket pThis - Pass this into the world change notifications
   PCDoorSet      pDoorSet - When the door updates it will call pDoorSet->DoorSet()
   DWORD          dwDoorNum - Door number to use for door set. If -1 then set all doors
   PCObjectDoor   pObjectDoor - If not NULL, m_fDirty is set at times when the color potentially changes
returns
   BOOL - TRUE if the user pressed back/OK, FALSE if pressed close
*/
BOOL CDoor::CustomDialog (PCEscWindow pWindow, PCWorldSocket pWorld, PCObjectSocket pThis,
                          CDoorSet *pDoorSet, DWORD dwDoorNum, PCObjectDoor pObjectDoor)
{
   DCP dcp;
   memset (&dcp, 0, sizeof(dcp));
   dcp.pDoorSet = pDoorSet;
   dcp.pThis = pThis;
   dcp.pv = this;
   dcp.pWorld = pWorld;
   dcp.dwDoorNum = dwDoorNum;
   dcp.pObjectDoor = pObjectDoor;

   PWSTR psz;
firstpage:
   psz = pWindow->PageDialog (ghInstance, IDR_MMLDOORCUSTOM, DoorCustomPage, &dcp);
   if (!psz)
      return FALSE;
   if (!_wcsicmp(psz, RedoSamePage())) {
      dcp.iVScroll = pWindow->m_iExitVScroll;
      goto firstpage;
   }
   if (!_wcsicmp(psz, Back()))
      return TRUE;

   // else close
   return FALSE;
}



