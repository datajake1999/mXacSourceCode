/************************************************************************
CTooth.cpp - Draws a Tooth.

begun 14/9/01 by Mike Rozak
Copyright 2001 Mike Rozak. All rights reserved
*/

#include <windows.h>
#include "escarpment.h"
#include "..\M3D.h"
#include "resource.h"



/**********************************************************************************
CTooth::Constructor and destructor
*/
CTooth::CTooth (void)
{
   m_pNoodle = new CNoodle;

   m_fLength = 0.02;
   m_fCurvature = PI/4;
   m_fCurveLinear = 50;
   m_fProfileLinear = 50;
   m_tpRoot.h = 0.005;
   m_tpRoot.v = 0.005;
   m_tpTip.h = m_tpTip.v = 0;

   m_pProfileRoot.p[0] = m_pProfileRoot.p[1] = m_pProfileRoot.p[2] = m_pProfileRoot.p[3] = 50;
   m_pProfileTip.p[0] = m_pProfileTip.p[1] = m_pProfileTip.p[2] = m_pProfileTip.p[3] = 50;

   m_dwTextWrapProf = m_dwTextWrapLength = 0;
   m_dwUserDetail = 0;

   m_fDirty = TRUE;
   m_dwDetail = 0;
}

CTooth::~CTooth (void)
{
   if (m_pNoodle)
      delete m_pNoodle;
}

static PWSTR gpszTooth = L"Tooth";
static PWSTR gpszLength = L"Length";
static PWSTR gpszCurvature = L"Curvature";
static PWSTR gpszCurveLinear = L"CurveLinear";
static PWSTR gpszProfileLinear = L"ProfileLinear";
static PWSTR gpszRoot = L"Root";
static PWSTR gpszTip = L"Tip";
static PWSTR gpszProfileRoot = L"ProfileRoot";
static PWSTR gpszProfileTip = L"ProfileTip";
static PWSTR gpszTextWrapProf = L"TextWrapProf";
static PWSTR gpszTextWrapLength = L"TextWrapLength";
static PWSTR gpszUserDetail = L"UserDetail";

/**********************************************************************************
CTooth::MMLTo - Standard API
*/
PCMMLNode2 CTooth::MMLTo (void)
{
   PCMMLNode2 pNode = new CMMLNode2;
   if (!pNode)
      return NULL;
   pNode->NameSet (gpszTooth);

   MMLValueSet (pNode, gpszLength, m_fLength);
   MMLValueSet (pNode, gpszCurvature, m_fCurvature);
   MMLValueSet (pNode, gpszCurveLinear, m_fCurveLinear);
   MMLValueSet (pNode, gpszProfileLinear, m_fProfileLinear);
   MMLValueSet (pNode, gpszRoot, &m_tpRoot);
   MMLValueSet (pNode, gpszTip, &m_tpTip);
   MMLValueSet (pNode, gpszProfileRoot, &m_pProfileRoot);
   MMLValueSet (pNode, gpszProfileTip, &m_pProfileTip);
   MMLValueSet (pNode, gpszTextWrapProf, (int)m_dwTextWrapProf);
   MMLValueSet (pNode, gpszTextWrapLength, (int)m_dwTextWrapLength);
   MMLValueSet (pNode, gpszUserDetail, (int)m_dwUserDetail);

   return pNode;
}


/**********************************************************************************
CTooth::MMLFrom - Standard API
*/
BOOL CTooth::MMLFrom (PCMMLNode2 pNode)
{
   m_fLength = MMLValueGetDouble (pNode, gpszLength, 1);
   m_fCurvature = MMLValueGetDouble (pNode, gpszCurvature, 1);
   m_fCurveLinear = MMLValueGetDouble (pNode, gpszCurveLinear, 50);
   m_fProfileLinear = MMLValueGetDouble (pNode, gpszProfileLinear, 50);
   MMLValueGetTEXTUREPOINT (pNode, gpszRoot, &m_tpRoot);
   MMLValueGetTEXTUREPOINT (pNode, gpszTip, &m_tpTip);
   MMLValueGetPoint (pNode, gpszProfileRoot, &m_pProfileRoot);
   MMLValueGetPoint (pNode, gpszProfileTip, &m_pProfileTip);
   m_dwTextWrapProf = (DWORD) MMLValueGetInt (pNode, gpszTextWrapProf, (int)0);
   m_dwTextWrapLength = (DWORD) MMLValueGetInt (pNode, gpszTextWrapLength, (int)0);
   m_dwUserDetail = (DWORD) MMLValueGetInt (pNode, gpszUserDetail, (int)0);

   m_fDirty = TRUE;

   return TRUE;
}


/**********************************************************************************
CTooth::Clone - Standard API
*/
CTooth *CTooth::Clone (void)
{
   PCTooth pNew = new CTooth;
   if (!pNew)
      return NULL;
   if (!CloneTo (pNew)) {
      delete pNew;
      return NULL;
   }

   return pNew;
}



/**********************************************************************************
CTooth::CloneTo - Standard API
*/
BOOL CTooth::CloneTo (CTooth *pTo)
{
   pTo->m_fLength = m_fLength;
   pTo->m_fCurvature = m_fCurvature;
   pTo->m_fCurveLinear = m_fCurveLinear;
   pTo->m_fProfileLinear = m_fProfileLinear;
   pTo->m_tpRoot = m_tpRoot;
   pTo->m_tpTip = m_tpTip;
   pTo->m_pProfileRoot.Copy (&m_pProfileRoot);
   pTo->m_pProfileTip.Copy (&m_pProfileTip);
   pTo->m_dwTextWrapProf = m_dwTextWrapProf;
   pTo->m_dwTextWrapLength = m_dwTextWrapLength;
   pTo->m_dwUserDetail = m_dwUserDetail;
   pTo->m_fDirty = TRUE;

   return TRUE;
}


// BUGFIX - Disable optimizations because errors with INTERNAL COMPILER ERROR
// #pragma optimize ("", off)

/**********************************************************************************
CTooth::Render - This draws the tooth.

inputs
   POJECTRENDER         pr - To render to
   PCRenderSurface      prs - To Render to
returns
   BOOL - TRUE if success
*/
BOOL CTooth::Render (POBJECTRENDER pr, PCRenderSurface prs)
{
   // Need to calculate detail level and maybe set dirty
   DWORD dwWant;
   if (pr && (pr->dwReason != ORREASON_BOUNDINGBOX)) {
      fp fDetail = pr->pRS->QueryDetail ();
      fp fMax = max(m_fLength, m_tpRoot.h);
      fMax = max(fMax, m_tpRoot.v);
      fMax = max(fMax, m_tpTip.v);
      fMax = max(fMax, m_tpTip.v);
      // dont want this: fMax /= (fp) (1 << m_dwUserDetail);

      if (fDetail < fMax / 2)
         dwWant = 2;
      else if (fDetail < fMax)
         dwWant = 1;
      else
         dwWant = 0;

      if (dwWant != m_dwDetail)
         m_fDirty = TRUE;
   }
   else
      dwWant = m_dwDetail;


   if (!CalcIfNecessary (dwWant))
      return FALSE;

   // render
   return m_pNoodle->Render (pr, prs);
}


/**********************************************************************************
CTooth::CalcIfNecessary - If Dirty then calculate

inputs
   DWORD       dwWant - Detail that want
returns
   BOOL - TRUE if success. FALSE if no data
*/
BOOL CTooth::CalcIfNecessary (DWORD dwWant)
{
   if (!m_fDirty)
      return TRUE;

   m_fDirty = FALSE;
   m_dwDetail = dwWant;

   // total detail
#define MAXDETAIL    4
   DWORD dwDetail = m_dwDetail + m_dwUserDetail;
   dwDetail = min(dwDetail, MAXDETAIL);
   DWORD dwSkip = (1 << (MAXDETAIL - dwDetail));

   // calculate the path
   CMem memPath;
   DWORD dwNumPath = (2 << MAXDETAIL) + 1;
   DWORD dwNumScale = (2 << dwDetail) + 1;   // BUGFIX - was m_dwDetail
   DWORD dwNumProf = (4 << ((dwDetail+1)/2));   // BUGFIX - was m_dwDetail
   DWORD dwNeed = sizeof(CPoint) * (dwNumPath + dwNumScale + dwNumProf*2);
   if (!memPath.Required (dwNeed))
      return FALSE;
   memset (memPath.p, 0, dwNeed);
   PCPoint pPath = (PCPoint) memPath.p;
   PCPoint pScale = pPath + dwNumPath;
   PCPoint pProf = pScale + dwNumScale;

   // use a sphere...
   DWORD i;
   if (m_fCurvature < CLOSE) {
      for (i = 0; i < dwNumPath; i++)
         pPath[i].p[2] = i;  // dont worry about scale since will be normalized
   }
   else {
      // use sphere
      for (i = 0; i < dwNumPath; i++) {
         fp fAngle = (fp)i / (fp)(dwNumPath-1);
         fAngle *= (fp)m_fCurvature;
         pPath[i].p[2] = sin(fAngle);
         pPath[i].p[1] = 1.0 - cos(fAngle); // so arcs away
      } // i
   }

   // convert to deltas
   for (i = dwNumPath-1; i < dwNumPath; i--)
      if (i)
         pPath[i].Subtract (&pPath[i-1]);
      else
         pPath[i].Zero();

   // scale by pow
   fp fPow = (m_fCurveLinear - 50.0) / 25.0; // so from -2 to 2, 0 being none
   fp fCur = 1;
   fPow = exp(fPow / (fp)(dwNumPath-1));
   for (i = 0; i < dwNumPath; i++, fCur *= fPow)
      pPath[i].Scale (fCur);


   // apply non-linearity
   fp fLen = 0;
   for (i = 0; i < dwNumPath; i++)
      fLen += pPath[i].Length();
   fLen = m_fLength / fLen;

   // convert back from deltas
   for (i = 0; i < dwNumPath; i++) {
      pPath[i].Scale (fLen);
      if (i)
         pPath[i].Add (&pPath[i-1]);
   }


   // loop back over the path, and keep only a few points, as per the detail requested
   for (i = 0; i*dwSkip < dwNumPath; i++)
      if (i != i*dwSkip)
         pPath[i].Copy (&pPath[i*dwSkip]);
   dwNumPath = i;



   // calculate the scale...
   fPow = (m_fProfileLinear - 50.0) / 50.0 * 10.0; // so from -4 to 4, 0 being none
   fPow = exp(fPow / (fp)(dwNumPath-1));
   for (i = 0; i < dwNumScale; i++) {
      fp fAlpha = (fp)i / (fp)(dwNumScale-1);
      fAlpha = pow(fAlpha, fPow);

      // BUGFIX - Flip p[0] and p[1]
      pScale[i].p[1] = (1.0 - fAlpha)*m_tpRoot.h + fAlpha*m_tpTip.h;
      pScale[i].p[0]  = (1.0 - fAlpha)*m_tpRoot.v + fAlpha*m_tpTip.v;
   } // i, scale

   // profile
   DWORD dwTip;
   for (dwTip = 0; dwTip < 2; dwTip++) {
      PCPoint pUse = dwTip ? &m_pProfileTip : &m_pProfileRoot;
      PCPoint pOff = pProf + (dwTip * dwNumProf);

      for (i = 0; i < dwNumProf; i++) {
         fp fAngle = (fp)i / (fp)dwNumProf * 2.0 * PI;
         fp fx = sin(fAngle);
         fp fy = cos(fAngle);

         // amount of power to apply to y
         fPow = ((1.0 + fx) * pUse->p[3] + (1.0-fx) * pUse->p[2])/2.0;
         fPow = (fPow - 50.0) / 50.0 * 2.0;
         fPow = exp(fPow);
         pOff[i].p[0] = pow(fabs(fy), fPow) * ((fy < 0) ? -0.5 : 0.5);
            // BUGFIX - Swap p[0] and p[1]

         // amount of power to apply to x
         fPow = ((1.0 + fy) * pUse->p[1] + (1.0-fy) * pUse->p[0])/2.0;
         fPow = (fPow - 50.0) / 50.0 * 2.0;
         fPow = exp(fPow);
         pOff[i].p[1] = -pow(fabs(fx), fPow) * ((fx < 0) ? -0.5 : 0.5);
      } // i
   } // dwTip

   CPoint pFront;
   pFront.Zero();
   pFront.p[0] = 1;  // so guarantee front.

   // set noodle info
   m_pNoodle->BackfaceCullSet (TRUE);
   m_pNoodle->DrawEndsSet (m_tpTip.h && m_tpTip.v);
   m_pNoodle->FrontVector (&pFront);
   m_pNoodle->PathSpline (FALSE, dwNumPath, pPath, (DWORD*)SEGCURVE_CUBIC, 0);
   m_pNoodle->ScaleSpline (dwNumScale, pScale, (DWORD*)SEGCURVE_CUBIC, 0);
   m_pNoodle->ShapeSplineSurface (TRUE, dwNumProf, 2, pProf,
      (DWORD*)SEGCURVE_CUBIC, (DWORD*)SEGCURVE_LINEAR, 0, 0);
   // m_pNoodle->ShapeDefault (NS_CIRCLE);

   m_pNoodle->TextureWrapSet (m_dwTextWrapProf, m_dwTextWrapLength);

   return TRUE;
}
// #pragma optimize ("", on)


/**********************************************************************************
CTooth::QueryBoundingBox - Standard API
*/
void CTooth::QueryBoundingBox (PCPoint pCorner1, PCPoint pCorner2)
{
   CalcIfNecessary (m_dwDetail);

   m_pNoodle->QueryBoundingBox (pCorner1, pCorner2);
}


/**********************************************************************************
CTooth::Mirror - Does a L/R mirror so the tooth can be mirrored to the other side
of the jaw
*/
void CTooth::Mirror (void)
{
   fp f;

   // flip
   f = m_pProfileRoot.p[2];
   m_pProfileRoot.p[2] = m_pProfileRoot.p[3];
   m_pProfileRoot.p[3] = f;

   // flip
   f = m_pProfileTip.p[2];
   m_pProfileTip.p[2] = m_pProfileRoot.p[3];
   m_pProfileTip.p[3] = f;

   m_fDirty = TRUE;
}


// TIS - passed to dialog
typedef struct {
   PCTooth              pv;         // this
   PCObjectTemplate     pTemp;      // template
   PCTooth              pMirror;    // mirror
} TIS, *PTIS;




/****************************************************************************
ToothPage
*/
BOOL ToothPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PTIS ptis = (PTIS)pPage->m_pUserData;
   PCTooth pv = ptis->pv;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         PCEscControl pControl;

         MeasureToString (pPage, L"length", pv->m_fLength, TRUE);
         MeasureToString (pPage, L"root0", pv->m_tpRoot.h, TRUE);
         MeasureToString (pPage, L"root1", pv->m_tpRoot.v, TRUE);
         MeasureToString (pPage, L"tip0", pv->m_tpTip.h, TRUE);
         MeasureToString (pPage, L"tip1", pv->m_tpTip.v, TRUE);
         AngleToControl (pPage, L"curvature", pv->m_fCurvature);
         DoubleToControl (pPage, L"textwrapprof", pv->m_dwTextWrapProf);
         DoubleToControl (pPage, L"textwraplength", pv->m_dwTextWrapLength);

         ComboBoxSet (pPage, L"userdetail", pv->m_dwUserDetail);

         pControl = pPage->ControlFind (L"curvelinear");
         if (pControl)
            pControl->AttribSetInt (Pos(), (int)(pv->m_fCurveLinear));
         pControl = pPage->ControlFind (L"profilelinear");
         if (pControl)
            pControl->AttribSetInt (Pos(), (int)(pv->m_fProfileLinear));
         DWORD i, j;
         WCHAR szTemp[32];
         for (i = 0; i < 2; i++) for (j = 0; j < 4; j++) {
            swprintf (szTemp, L"profile%d%d", (int)i, (int)j);
            pControl = pPage->ControlFind (szTemp);
            if (pControl)
               pControl->AttribSetInt (Pos(), (int)((i ? pv->m_pProfileTip.p[j] : pv->m_pProfileRoot.p[j])));
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
         DWORD dwVal;
         dwVal = p->pszName ? (DWORD) _wtoi(p->pszName) : 0;

         if (!_wcsicmp(psz, L"userdetail")) {
            if (dwVal == pv->m_dwUserDetail)
               break;

            // else, change
            ptis->pTemp->m_pWorld->ObjectAboutToChange (ptis->pTemp);

            pv->m_dwUserDetail = dwVal;
            pv->m_fDirty = TRUE;
            if (ptis->pMirror) {
               ptis->pMirror->m_dwUserDetail = dwVal;
               ptis->pMirror->m_fDirty = TRUE;
            }
            ptis->pTemp->m_pWorld->ObjectChanged (ptis->pTemp);
            return TRUE;
         }
      }
      break;



   case ESCN_SCROLL:
   case ESCN_SCROLLING:
      {
         PESCNSCROLL p = (PESCNSCROLL) pParam;

         // only do one scroll bar
         if (!p->pControl->m_pszName)
            break;

         fp *pfMod = NULL;
         PWSTR pszProfile = L"profile";
         DWORD dwLenProfile = (DWORD)wcslen(pszProfile);
         if (!_wcsicmp(p->pControl->m_pszName, L"curvelinear"))
            pfMod = &pv->m_fCurveLinear;
         else if (!_wcsicmp(p->pControl->m_pszName, L"profilelinear"))
            pfMod = &pv->m_fProfileLinear;
         else if (!wcsncmp(p->pControl->m_pszName, pszProfile, dwLenProfile)) {
            pfMod = (p->pControl->m_pszName[dwLenProfile] - L'0') ?
               &pv->m_pProfileTip.p[0] : &pv->m_pProfileRoot.p[0];
            pfMod = pfMod + (p->pControl->m_pszName[dwLenProfile+1] - L'0');
         }
         else
            break;   // not one of these

         fp fVal;
         fVal = p->pControl->AttribGetInt (Pos());

         ptis->pTemp->m_pWorld->ObjectAboutToChange (ptis->pTemp);
         *pfMod = fVal;
         
         pv->m_fDirty = TRUE;

         // mirror
         if (ptis->pMirror) {
            pv->CloneTo (ptis->pMirror);
            ptis->pMirror->Mirror();
         }
         ptis->pTemp->m_pWorld->ObjectChanged (ptis->pTemp);
         return TRUE;
      }
      break;

   case ESCN_EDITCHANGE:
      {
         PESCNEDITCHANGE p = (PESCNEDITCHANGE) pParam;

         ptis->pTemp->m_pWorld->ObjectAboutToChange (ptis->pTemp);

         MeasureParseString (pPage, L"length", &pv->m_fLength);
         pv->m_fLength = max(pv->m_fLength, CLOSE);

         fp fTemp;
         MeasureParseString (pPage, L"root0", &fTemp);
         pv->m_tpRoot.h = fTemp;
         pv->m_tpRoot.h = max(pv->m_tpRoot.h, 0);

         MeasureParseString (pPage, L"root1", &fTemp);
         pv->m_tpRoot.v = fTemp;
         pv->m_tpRoot.v = max(pv->m_tpRoot.v, 0);

         MeasureParseString (pPage, L"tip0", &fTemp);
         pv->m_tpTip.h = fTemp;
         pv->m_tpTip.h = max(pv->m_tpTip.h, 0);

         MeasureParseString (pPage, L"tip1", &fTemp);
         pv->m_tpTip.v = fTemp;
         pv->m_tpTip.v = max(pv->m_tpTip.v, 0);

         pv->m_fCurvature = AngleFromControl (pPage, L"curvature");
         pv->m_fCurvature = max(pv->m_fCurvature, 0);

         pv->m_dwTextWrapProf = (DWORD) DoubleFromControl (pPage, L"textwrapprof");
         pv->m_dwTextWrapLength = (DWORD) DoubleFromControl (pPage, L"textwraplength");

         pv->m_fDirty = TRUE;

         // mirror
         if (ptis->pMirror) {
            pv->CloneTo (ptis->pMirror);
            ptis->pMirror->Mirror();
         }
         ptis->pTemp->m_pWorld->ObjectChanged (ptis->pTemp);
      }
      break;


   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Tooth/claw settings";
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}


/**********************************************************************************
CTooth::Dialog - Brings up a dialog so that can edit the tooth.

inputs
   PCObjectTemplate     pTemp - Used for objectchanged notifications
   PCTooth              pMirror - If not NULL, then any changes to this tooth will be
                           automatically mirrored to pMirror
   PCEscWindow          pWindow - Window to display on
returns
   BOOL - TRUE if user pressed back, FALSE if closed wdinow
*/
BOOL CTooth::Dialog (PCObjectTemplate pTemp, CTooth *pMirror, PCEscWindow pWindow)
{
   PWSTR pszRet;
   TIS tis;
   memset (&tis, 0, sizeof(tis));
   tis.pMirror = pMirror;
   tis.pTemp = pTemp;
   tis.pv = this;

   pszRet = pWindow->PageDialog (ghInstance, IDR_MMLTOOTH, ToothPage, &tis);
   if (!pszRet)
      return FALSE;

   return !_wcsicmp(pszRet, Back());
}


/**********************************************************************************
CTooth::Scale - Scales the tooth.

inputs
   PCPoint        pScale - p[0] = width, p[1] = depth, p[2] = height. These should
                  be positive numbers
*/
void CTooth::Scale (PCPoint pScale)
{
   m_fDirty = TRUE;
   m_fLength *= pScale->p[2];
   m_tpRoot.h *= pScale->p[0];
   m_tpRoot.v *= pScale->p[1];
   m_tpTip.h *= pScale->p[0];
   m_tpTip.v *= pScale->p[1];
}



/**********************************************************************************
CTooth::IsMirror - Compares this tooth against another one and returns a score.
The higher the score, the more mirror-like the teeth.

inputs
   PCTooth        pTooth - Tooth to compare to
returns
   fp - Value
*/
fp CTooth::IsMirror (CTooth *pTooth)
{
   fp fScore = 0;

   fScore -= fabs(m_fLength - pTooth->m_fLength);
   fScore -= fabs(m_fCurvature - pTooth->m_fCurvature);
   fScore -= fabs(m_fCurveLinear - pTooth->m_fCurveLinear)/100;
   fScore -= fabs(m_fProfileLinear - pTooth->m_fProfileLinear)/100;
   fScore -= fabs(m_tpRoot.h - pTooth->m_tpRoot.h);
   fScore -= fabs(m_tpRoot.v - pTooth->m_tpRoot.v);
   fScore -= fabs(m_tpTip.h - pTooth->m_tpTip.h);
   fScore -= fabs(m_tpTip.v - pTooth->m_tpTip.v);
   DWORD i;
   for (i = 0; i < 4; i++) {
      DWORD dwA = i, dwB = i;
      if (i == 2)
         dwB = 3; // mirror
      else if (i == 3)
         dwB = 2; // mirror
      fScore -= fabs(m_pProfileRoot.p[dwA] - pTooth->m_pProfileRoot.p[dwB]);
      fScore -= fabs(m_pProfileTip.p[dwA] - pTooth->m_pProfileTip.p[dwB]);
   } // i
   fScore -= fabs((fp)m_dwTextWrapProf - (fp)pTooth->m_dwTextWrapProf);
   fScore -= fabs((fp)m_dwTextWrapLength - (fp)pTooth->m_dwTextWrapLength);
   fScore -= fabs((fp)m_dwUserDetail - (fp)pTooth->m_dwUserDetail);

   return fScore;
}

