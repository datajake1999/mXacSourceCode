/*************************************************************************************
CTransition.cpp - Code for handling the transition from one image to another.

begun 22/8/04 by Mike Rozak.
Copyright 2004 by Mike Rozak. All rights reserved
*/

#include <windows.h>
#include <objbase.h>
#include "escarpment.h"
#include "..\..\m3d\M3D.h"
#include "..\..\m3d\mifl.h"
#include "..\mif.h"
#include "..\buildnum.h"
#include "resource.h"


/*************************************************************************************
CTransition::Constructor and destructor
*/
CTransition::CTransition (void)
{
   Clear();
}

CTransition::~CTransition (void)
{
   // do nothing for now
}

/*************************************************************************************
CTransition::Clear - Wipes out the settings
*/
void CTransition::Clear (void)
{
   m_fFadeFromDur = 0;
   m_cFadeFromColor = RGB(0,0,0);
   m_fFadeToStart = 0;
   m_fFadeToStop = 60;
   m_cFadeToColor = RGB(0,0,0);
   m_fFadeInDur = 0;
   m_cTransparent = RGB(0,0,0);
   m_fUseTransparent = FALSE;
   m_dwTransparentDist = 0;
   m_pPanStart.Zero();
   m_pPanStart.p[0] = m_pPanStart.p[1] = 0.5;
   m_pPanStart.p[2] = 1;
   m_pPanStop.Copy (&m_pPanStart);
   m_fPanStartTime = 0;
   m_fPanStopTime = 60;
   m_iResExtra = 0;

   m_fAnimateTime = 0;
   m_fAnimateQuery = FALSE;
}


/*************************************************************************************
CTransition::CloneTo - Standard API
*/
BOOL CTransition::CloneTo (CTransition *pTo)
{
   pTo->m_fFadeFromDur = m_fFadeFromDur;
   pTo->m_cFadeFromColor = m_cFadeFromColor;
   pTo->m_fFadeToStart = m_fFadeToStart;
   pTo->m_fFadeToStop = m_fFadeToStop;
   pTo->m_cFadeToColor = m_cFadeToColor;
   pTo->m_fFadeInDur = m_fFadeInDur;
   pTo->m_cTransparent = m_cTransparent;
   pTo->m_fUseTransparent = m_fUseTransparent;
   pTo->m_dwTransparentDist =m_dwTransparentDist;
   pTo->m_pPanStart.Copy (&m_pPanStart);
   pTo->m_pPanStop.Copy (&m_pPanStop);
   pTo->m_fPanStartTime = m_fPanStartTime;
   pTo->m_fPanStopTime = m_fPanStopTime;
   pTo->m_fAnimateTime = m_fAnimateTime;
   pTo->m_fAnimateQuery =m_fAnimateQuery;
   pTo->m_iResExtra = m_iResExtra;

   return TRUE;
}


/*************************************************************************************
CTransition::Clone - Standard API
*/
CTransition *CTransition::Clone (void)
{
   PCTransition pNew = new CTransition;
   if (!pNew)
      return NULL;
   if (!CloneTo (pNew)) {
      delete pNew;
      return NULL;
   }
   return pNew;
}

static PWSTR gpszFadeFromColor = L"FadeFromColor";
static PWSTR gpszFadeFromDur = L"FadeFromDur";
static PWSTR gpszFadeInDur = L"FadeInDur";
static PWSTR gpszFadeToColor = L"FadeToColor";
static PWSTR gpszFadeToStart = L"FadeToStart";
static PWSTR gpszFadeToStop = L"FadeToStop";
static PWSTR gpszPanStart = L"PanStart";
static PWSTR gpszPanStop = L"PanStop";
static PWSTR gpszPanStartTime = L"PanStartTime";
static PWSTR gpszPanStopTime = L"PanStopTime";
static PWSTR gpszTransparent = L"Transparent";
static PWSTR gpszTransparentDist = L"TransparentDist";
static PWSTR gpszResExtra = L"ResExtra";

/*************************************************************************************
CTransition::MMLTo - Stardard API EXCEPT if there isn't any info to write this
will return NULL. (Even though there's no error)
*/
PCMMLNode2 CTransition::MMLTo (void)
{
   PCMMLNode2 pNode = new CMMLNode2;
   if (!pNode)
      return NULL;
   pNode->NameSet (CircumrealityTransition());

   BOOL fWrote = FALSE;

   if (m_fFadeFromDur > 0) {
      fWrote = TRUE;

      MMLValueSet (pNode, gpszFadeFromDur, m_fFadeFromDur);
      if (m_cFadeFromColor)
         MMLValueSet (pNode, gpszFadeFromColor, (int)m_cFadeFromColor);
   }

   if (m_fFadeToStart > 0) {
      fWrote = TRUE;

      MMLValueSet (pNode, gpszFadeToStart, m_fFadeToStart);
      MMLValueSet (pNode, gpszFadeToStop, m_fFadeToStop);
      if (m_cFadeToColor)
         MMLValueSet (pNode, gpszFadeToColor, (int)m_cFadeToColor);
   }

   if (m_fFadeInDur > 0) {
      fWrote = TRUE;

      MMLValueSet (pNode, gpszFadeInDur, m_fFadeInDur);
   }

   if (m_fUseTransparent) {
      fWrote = TRUE;

      MMLValueSet (pNode, gpszTransparent, (int)m_cTransparent);
      if (m_dwTransparentDist)
         MMLValueSet (pNode, gpszTransparentDist, (int)m_dwTransparentDist);
   }

   if (m_fPanStartTime > 0) {
      fWrote = TRUE;

      MMLValueSet (pNode, gpszPanStart, &m_pPanStart);
      MMLValueSet (pNode, gpszPanStop, &m_pPanStop);
      MMLValueSet (pNode, gpszPanStartTime, m_fPanStartTime);
      MMLValueSet (pNode, gpszPanStopTime, m_fPanStopTime);
   }

   if (m_iResExtra) {
      fWrote = TRUE;

      MMLValueSet (pNode, gpszResExtra, (int)m_iResExtra);
   }

   if (fWrote)
      return pNode;
   else {
      delete pNode;
      return NULL;
   }
}


/*************************************************************************************
CTransition::MMLFrom - Standard API, except this one accepts NULL pNode
*/
BOOL CTransition::MMLFrom (PCMMLNode2 pNode)
{
   // clear
   Clear();
   if (!pNode)
      return TRUE;

   m_fFadeFromDur = MMLValueGetDouble (pNode, gpszFadeFromDur, 0);
   m_cFadeFromColor = (COLORREF) MMLValueGetInt (pNode, gpszFadeFromColor, 0);

   m_fFadeToStart = MMLValueGetDouble (pNode, gpszFadeToStart, 0);
   m_fFadeToStop = MMLValueGetDouble (pNode, gpszFadeToStop, 60);
   m_cFadeToColor = (COLORREF) MMLValueGetInt (pNode, gpszFadeToColor, 0);

   m_fFadeInDur = MMLValueGetDouble (pNode, gpszFadeInDur, 0);
   m_cTransparent = (COLORREF) MMLValueGetInt (pNode, gpszTransparent, -1);
   if (m_cTransparent == (COLORREF)-1) {
      m_fUseTransparent = FALSE;
      m_cTransparent = 0;  // so use black
   }
   else
      m_fUseTransparent = TRUE;
   m_dwTransparentDist = (DWORD) MMLValueGetInt (pNode, gpszTransparentDist, 0);

   MMLValueGetPoint (pNode, gpszPanStart, &m_pPanStart);
   MMLValueGetPoint (pNode, gpszPanStop, &m_pPanStop);
   m_fPanStartTime = MMLValueGetDouble (pNode, gpszPanStartTime, 0);
   m_fPanStopTime = MMLValueGetDouble (pNode, gpszPanStopTime, 60);

   m_iResExtra = MMLValueGetInt (pNode, gpszResExtra, 0);

   return TRUE;
}



/*************************************************************************************
CTransition::AnimateIsPassthrough - Returns TRUE if this animation is acting as
a passthrough (no effects whatsoever).
*/
BOOL CTransition::AnimateIsPassthrough (void)
{
   // if transparent then not passthrough
   if (m_fUseTransparent)
      return FALSE;

   // see if animating fade in
   if ((m_fFadeFromDur > 0) && (m_fAnimateTime < m_fFadeFromDur))
      return FALSE;

   // see if animating fade out
   if ((m_fFadeToStart > 0) && (m_fAnimateTime > m_fFadeToStart))
      return FALSE;

   // see if animating fade over image
   if ((m_fFadeInDur > 0) && (m_fAnimateTime < m_fFadeInDur))
      return FALSE;

   // see if animating pan/zoom
   if (m_fPanStartTime > 0)
      return FALSE;

   // else, not changing the image
   return TRUE;
}

/*************************************************************************************
CTransition::AnimateQuery - Returns TRUE if the transition is still going on,
or FALSE if there is no animation for the transition or its all finished.
*/
BOOL CTransition::AnimateQuery (void)
{
   return m_fAnimateQuery;
}


/*************************************************************************************
CTransition::AnimateQueryInternal - Internal function which returns TRUE if the animation
is happening at the current time
*/
BOOL CTransition::AnimateQueryInternal (void)
{
   // see if animating fade in
   if ((m_fFadeFromDur > 0) && (m_fAnimateTime <= m_fFadeFromDur))
      return TRUE;

   // see if animating fade out
   if ((m_fFadeToStart > 0) && (m_fAnimateTime <= m_fFadeToStop))
      return TRUE;

   // see if animating fade over image
   if ((m_fFadeInDur > 0) && (m_fAnimateTime <= m_fFadeInDur))
      return TRUE;

   // see if animating pan/zoom
   if ((m_fPanStartTime > 0) && (m_fAnimateTime <= m_fPanStopTime))
      return TRUE;

   // else, not animating
   return FALSE;
}



/*************************************************************************************
CTransition::AnimateInit - Call this to initialize the animation settings (after
get them
*/
void CTransition::AnimateInit (void)
{
   m_fAnimateTime = 0;
   m_fAnimateQuery = AnimateQueryInternal ();
}


/*************************************************************************************
CTransition::AnimateAdvance - Call this to advance the animation time. NOTE: Should
call AnimateQuery() before AnimateAdvance() to see if should do an animation (after
animate advance is called)

inputs
   fp          fTime - Number of seconds to advance the clock by
*/
void CTransition::AnimateAdvance (fp fTime)
{
   if (!m_fAnimateQuery)
      return;  // dont bother

   m_fAnimateTime += fTime;
   m_fAnimateQuery = AnimateQueryInternal ();
}


/*************************************************************************************
CTransition::AnimateNeedBelow - Returns TRUE if the animation needs the image
below it, given the current aniamtion time
*/
BOOL CTransition::AnimateNeedBelow (void)
{
   if (m_fUseTransparent)
      return TRUE;

   // see if animating fade over image
   if ((m_fFadeInDur > 0) && (m_fAnimateTime < m_fFadeInDur))
      return TRUE;

   return FALSE;
}



/*************************************************************************************
CTransition::AnimateFrame - This causes a frame to be animated for the transition.

inputs
   DWORD                dwc - Cache number, from 0..NUMISCACHE-1
   fp                   fLong - Longitude passed to 360 degree views

   PCImageStore         pisTop - Image that's assocated with this animation frame,
                        and which needs to be layered on top what's below.
   RECT                 *prFromTop - Where the image comes from for the top one.
   COLORREF             bkColor - Background color

   DWORD                dwNum - Number of transitions below this
   PCImageStore         *ppisImage - Images below this. dwNum of them. Some might be NULL.
   PCTransition         *ppTrans - Pointer to an array of transitions below this. dwNum of them
   RECT                 *prFrom - Array of RECTs describing where the image is taken from
                           in ppisImage
   
   PCImageStore         pisDest - Where to store the image. The store will
                        have been initialized to the proper width and height.
returns
   BOOL - TRUE if success
*/
BOOL CTransition::AnimateFrame (DWORD dwc, fp fLong,
                                PCImageStore pisTop, RECT *prFromTop, COLORREF bkColor, DWORD dwNum,
                                PCImageStore *ppisImage, PCTransition *ppTrans,
                                RECT *prFrom, PCImageStore pisDest)
{
   BOOL fRet = TRUE;
   PCImageStore pBelow = NULL;
   PCImageStore pCache = NULL;
   BOOL fCacheAlloc = FALSE;

   // if we need to look below then do so
   if (AnimateNeedBelow ()) {
      pBelow = new CImageStore;
      if (!pBelow) {
         fRet = FALSE;
         goto done;
      }
      if (!pBelow->Init (pisDest->Width(), pisDest->Height())) {
         fRet = FALSE;
         goto done;
      }
      
      if (dwNum) {
         fRet = ppTrans[0]->AnimateFrame (dwc, fLong, ppisImage[0], &prFrom[0], bkColor, dwNum-1,
            ppisImage + 1, ppTrans + 1, prFrom + 1, pBelow);
         if (!fRet)
            goto done;
      }
      else
         pBelow->ClearToColor(bkColor);   // nothing below, so use darkness
   }

   // determine the zoom
   RECT rZoom;
   rZoom = *prFromTop;
   if ((m_fPanStartTime > 0) && pisTop) {
      // animate
      fp fTime;
      if (m_fAnimateTime >= m_fPanStopTime)
         fTime = 1;
      else if (m_fAnimateTime > m_fPanStartTime)
         fTime = (m_fAnimateTime - m_fPanStartTime) / (m_fPanStopTime - m_fPanStartTime);
      else
         fTime = 0;

      fp fX, fY, fScale;
      fX = (1.0 - fTime) * m_pPanStart.p[0] + fTime * m_pPanStop.p[0];
      fY = (1.0 - fTime) * m_pPanStart.p[1] + fTime * m_pPanStop.p[1];
      fScale = (1.0 - fTime) * m_pPanStart.p[2] + fTime * m_pPanStop.p[2];
      fScale = max(fScale, 0.01);


      // convert from To to From
      CMatrix mToToFrom;
      CMatrix m; // temp
      
      // no need to translate to, since 0,0 is UL corner
      // need to do scaling
      mToToFrom.Scale (1.0 / (fp)pisDest->Width(), 1.0 / (fp)pisDest->Height(), 1);

      // scale out
      m.Scale (prFromTop->right - prFromTop->left, prFromTop->bottom - prFromTop->top, 1);
      mToToFrom.MultiplyRight (&m);

      // need to offset to get 0,0 to From.left, from.top
      m.Translation (prFromTop->left, prFromTop->top, 0);
      mToToFrom.MultiplyRight (&m);



      // convert from image coords (0..width,0..height) to image coords after zoom
      CMatrix mAllToZoom;

      // scale so 0..1 by 0..1 covers entire image
      mAllToZoom.Scale (1.0 / (fp)pisTop->Width(), 1.0 / (fp)pisTop->Height(), 1);

      // offset so zooms around the center of zoom loc
      m.Translation (-0.5, -0.5, 0);
      mAllToZoom.MultiplyRight (&m);

      // do the inverse of the zoom
      m.Scale (fScale, fScale, 1);
      mAllToZoom.MultiplyRight (&m);

      // translate so the zoom location is centered
      m.Translation (fX, fY, 0);
      mAllToZoom.MultiplyRight (&m);

      // scale up to original image width and height
      m.Scale (pisTop->Width(), pisTop->Height(), 1);
      mAllToZoom.MultiplyRight (&m);


      // combine the two matrices
      m.Multiply (&mAllToZoom, &mToToFrom);

      // determine the new point locations
      CPoint p;
      p.Zero();
      p.MultiplyLeft (&m);
      rZoom.left = (int)p.p[0];
      rZoom.top = (int)p.p[1];
      p.Zero();
      p.p[0] = pisDest->Width();
      p.p[1] = pisDest->Height();
      p.MultiplyLeft (&m);
      rZoom.right = (int)p.p[0];
      rZoom.bottom = (int)p.p[1];
   }

   // just make sure have some size
   rZoom.right = max(rZoom.right, rZoom.left+1);
   rZoom.bottom = max(rZoom.bottom, rZoom.top+1);

   // get the cache
   BOOL fTransparent = m_fUseTransparent;
   COLORREF cTransparent = m_cTransparent;
   if (pisTop) {
      pCache = pisTop->CacheGet (dwc, (int)pisDest->Width(), (int)pisDest->Height(),
         &rZoom, fLong, fTransparent ? m_cTransparent : bkColor);

      if (!pCache) {
         // shouldnt happen
         fRet = FALSE;
         goto done;
      }

      // NOTE: If the cache's size does NOT match the expected with and hight
      // then create blank one
      if ((pCache->Width() != pisDest->Width()) || (pCache->Height() != pisDest->Height()))
         goto newcache;
   }
   else {
newcache:
      pCache = new CImageStore;
      fCacheAlloc = TRUE;
      if (!pCache || !pCache->Init (pisDest->Width(), pisDest->Height()) ) {
         fRet = FALSE;
         goto done;
      }

      // make the entire thing transparent
      cTransparent = RGB(0,0,0);
      fTransparent = TRUE;
      pCache->ClearToColor(cTransparent);   // nothing below, so use darkness
   }

   // figure out if this is faded to a color
   BYTE abFadeColor[3];
   DWORD dwFadeColor = 0;
   if ((m_fFadeFromDur > 0) && (m_fAnimateTime < m_fFadeFromDur)) {
      abFadeColor[0] = GetRValue(m_cFadeFromColor);
      abFadeColor[1] = GetGValue(m_cFadeFromColor);
      abFadeColor[2] = GetBValue(m_cFadeFromColor);
      dwFadeColor = (DWORD) ((1.0 - m_fAnimateTime / m_fFadeFromDur) * 256);
   }
   else if ((m_fFadeToStart > 0) && (m_fAnimateTime > m_fFadeToStart)) {
      abFadeColor[0] = GetRValue(m_cFadeToColor);
      abFadeColor[1] = GetGValue(m_cFadeToColor);
      abFadeColor[2] = GetBValue(m_cFadeToColor);

      if (m_fAnimateTime >= m_fFadeToStop)
         dwFadeColor = 256;   // full amount
      else
         dwFadeColor = (DWORD) ((m_fAnimateTime - m_fFadeToStart) / (m_fFadeToStop - m_fFadeToStart) * 256);
               // part way there
   }
   DWORD dwFadeColorInv = 256 - dwFadeColor;

   // figure amount that background is faded in
   DWORD dwFadeBack = 0;
   if ((m_fFadeInDur > 0) && (m_fAnimateTime < m_fFadeInDur))
      dwFadeBack = (DWORD) ((1.0 - m_fAnimateTime / m_fFadeInDur) * 256);
   DWORD dwFadeBackInv = 256 - dwFadeBack;

   // loop through and merge the images together
   DWORD dwWidth = pisDest->Width();
   DWORD dwHeight = pisDest->Height();
   PBYTE pbBelow = pBelow ? pBelow->Pixel(0,0) : NULL;
   PBYTE pbCache = pCache->Pixel(0,0);
   PBYTE pbTo = pisDest->Pixel(0,0);
   DWORD x,y;
   BYTE abTrans[3], abBack[3];
   BOOL fTransPixel;
   abTrans[0] = GetRValue (cTransparent);
   abTrans[1] = GetGValue (cTransparent);
   abTrans[2] = GetBValue (cTransparent);
   abBack[0] = GetRValue (bkColor);
   abBack[1] = GetGValue (bkColor);
   abBack[2] = GetBValue (bkColor);

   for (y = 0; y < dwHeight; y++) {
      for (x = 0; x < dwWidth; x++) {
         // initial color
         pbTo[0] = *(pbCache++);
         pbTo[1] = *(pbCache++);
         pbTo[2] = *(pbCache++);

         // is this a transparent pixel
         if (fTransparent)
            fTransPixel = ((
               (DWORD) max(pbTo[0], abTrans[0]) - min(pbTo[0],abTrans[0]) +
               (DWORD) max(pbTo[1], abTrans[1]) - min(pbTo[1],abTrans[1]) +
               (DWORD) max(pbTo[2], abTrans[2]) - min(pbTo[2],abTrans[2]) ) <= m_dwTransparentDist);
         else
            fTransPixel = FALSE;

         // if it's a transparent pixel then take what's below
         if (fTransPixel) {
            if (pbBelow) {
               pbTo[0] = pbBelow[0];
               pbTo[1] = pbBelow[1];
               pbTo[2] = pbBelow[2];
            }
            else {
               pbTo[0] = abBack[0];
               pbTo[1] = abBack[1];
               pbTo[2] = abBack[2];
            }
         }
         else {
            // not a transparent pixel

            // merge to color?
            if (dwFadeColor) {
               // reach from cache and fade
               pbTo[0] = (BYTE)(((DWORD) pbTo[0] * dwFadeColorInv + abFadeColor[0] * dwFadeColor) >> 8);
               pbTo[1] = (BYTE)(((DWORD) pbTo[1] * dwFadeColorInv + abFadeColor[1] * dwFadeColor) >> 8);
               pbTo[2] = (BYTE)(((DWORD) pbTo[2] * dwFadeColorInv + abFadeColor[2] * dwFadeColor) >> 8);
            }

            // blend in with the background
            if (dwFadeBack) {
               if (pbBelow) {
                  pbTo[0] = (BYTE)(((DWORD) pbTo[0] * dwFadeBackInv + pbBelow[0] * dwFadeBack) >> 8);
                  pbTo[1] = (BYTE)(((DWORD) pbTo[1] * dwFadeBackInv + pbBelow[1] * dwFadeBack) >> 8);
                  pbTo[2] = (BYTE)(((DWORD) pbTo[2] * dwFadeBackInv + pbBelow[2] * dwFadeBack) >> 8);
               }
               else {
                  pbTo[0] = (BYTE)(((DWORD) pbTo[0] * dwFadeBackInv + abBack[0] * dwFadeBack) >> 8);
                  pbTo[1] = (BYTE)(((DWORD) pbTo[1] * dwFadeBackInv + abBack[1] * dwFadeBack) >> 8);
                  pbTo[2] = (BYTE)(((DWORD) pbTo[2] * dwFadeBackInv + abBack[2] * dwFadeBack) >> 8);
               }
            } // blend with background
         } // wasnt transparent pixel

         // move pointers
         pbTo += 3;
         if (pbBelow)
            pbBelow += 3;
      } // x
   } // y

done:
   // delte pBelow
   if (pBelow)
      delete pBelow;
   if (fCacheAlloc && pCache)
      delete pCache;
   return fRet;
}



/*************************************************************************
TransitionPage
*/
BOOL TransitionPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCTransition prs = (PCTransition)pPage->m_pUserData;   // node to modify

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         // scroll to right position
         if (prs->m_iVScroll > 0) {
            pPage->VScroll (prs->m_iVScroll);

            // when bring up pop-up dialog often they're scrolled wrong because
            // iVScoll was left as valeu, and they use defpage
            prs->m_iVScroll = 0;

            // BUGFIX - putting this invalidate in to hopefully fix a refresh
            // problem when add or move a task in the ProjectView page
            pPage->Invalidate();
         }

         PCEscControl pControl;
         DWORD i;
         WCHAR szTemp[64];
         switch (prs->m_dwTab) {
         case 0:  // fade in
            DoubleToControl (pPage, L"fadefromdur", prs->m_fFadeFromDur);
            FillStatusColor (pPage, L"fadefromcolor", prs->m_cFadeFromColor);
            break;

         case 1:  // fade out
            DoubleToControl (pPage, L"fadetostart", prs->m_fFadeToStart);
            DoubleToControl (pPage, L"fadetostop", prs->m_fFadeToStop);
            FillStatusColor (pPage, L"fadetocolor", prs->m_cFadeToColor);
            break;

         case 2:  // fade in from image
            DoubleToControl (pPage, L"fadeindur", prs->m_fFadeInDur);
            break;

         case 3:  // transparency
            if (pControl = pPage->ControlFind (L"usetransparent"))
               pControl->AttribSetBOOL (Checked(), prs->m_fUseTransparent);
            DoubleToControl (pPage, L"transparentdist", prs->m_dwTransparentDist);
            FillStatusColor (pPage, L"transparent", prs->m_cTransparent);
            break;

         case 4:  // pan/zoom
            DoubleToControl (pPage, L"panstarttime", prs->m_fPanStartTime);
            DoubleToControl (pPage, L"panstoptime", prs->m_fPanStopTime);
            for (i = 0; i < 3; i++) {
               swprintf (szTemp, L"panstart%d", (int)i);
               DoubleToControl (pPage, szTemp, prs->m_pPanStart.p[i]);
               swprintf (szTemp, L"panstop%d", (int)i);
               DoubleToControl (pPage, szTemp, prs->m_pPanStop.p[i]);
            } // i
            ComboBoxSet (pPage, L"resextra", prs->m_iResExtra);

            // update display of bitmap
            pPage->Message (ESCM_USER+186);
            break;

         } // switch tab
      }
      break;

   case ESCN_EDITCHANGE:
      {
         PESCNEDITCHANGE p = (PESCNEDITCHANGE)pParam;
         PWSTR psz = p->pControl->m_pszName;
         if (!psz)
            break;

         (*prs->m_pfChanged) = TRUE;

         DWORD i;
         WCHAR szTemp[64];
         switch (prs->m_dwTab) {
         case 0:  // fade in
            prs->m_fFadeFromDur = DoubleFromControl (pPage, L"fadefromdur");
            break;

         case 1:  // fade out
            prs->m_fFadeToStart = DoubleFromControl (pPage, L"fadetostart");
            prs->m_fFadeToStop = DoubleFromControl (pPage, L"fadetostop");
            break;

         case 2:  // fade in from image
            prs->m_fFadeInDur = DoubleFromControl (pPage, L"fadeindur");
            break;

         case 3:  // transparency
            prs->m_dwTransparentDist = (DWORD) DoubleFromControl (pPage, L"transparentdist");
            break;

         case 4:  // pan/zoom
            prs->m_fPanStartTime = DoubleFromControl (pPage, L"panstarttime");
            prs->m_fPanStopTime = DoubleFromControl (pPage, L"panstoptime");
            for (i = 0; i < 3; i++) {
               swprintf (szTemp, L"panstart%d", (int)i);
               prs->m_pPanStart.p[i] = DoubleFromControl (pPage, szTemp);
               swprintf (szTemp, L"panstop%d", (int)i);
               prs->m_pPanStop.p[i] = DoubleFromControl (pPage, szTemp);
            } // i

            // update display of bitmap
            pPage->Message (ESCM_USER+186);
            break;
         } // switch m_dwTab

      }
      break;

   case ESCM_USER + 186:   // so update the image
      {
         PCEscControl pControl = pPage->ControlFind (L"image");
         if (!pControl)
            return TRUE;

         // get the size of the bitmap
         BITMAP   bm;
         GetObject (prs->m_hBmp, sizeof(bm), &bm);

         CONTROLIMAGEDRAGRECT ac[2];
         ESCMIMAGERECTSET eis;
         memset (&eis, 0, sizeof(eis));
         eis.dwNum = 2;
         eis.pRect = &ac[0];
         int i;
         memset (ac, 0, sizeof(ac));
         ac[0].cColor = RGB(0, 0xff,0);
         ac[0].rPos.left = ac[0].rPos.right = (int)(prs->m_pPanStart.p[0] * (fp)bm.bmWidth);
         ac[0].rPos.top = ac[0].rPos.bottom = (int)(prs->m_pPanStart.p[1] * (fp)bm.bmHeight);
         i = (int)(prs->m_pPanStart.p[2] * (fp)bm.bmWidth / 2);
         ac[0].rPos.left -= i;
         ac[0].rPos.right += i;
         i = (int)((fp)i / prs->m_fAspect);
         ac[0].rPos.top -= i;
         ac[0].rPos.bottom += i;

         ac[1].cColor = RGB(0xff, 0, 0);
         ac[1].rPos.left = ac[1].rPos.right = (int)(prs->m_pPanStop.p[0] * (fp)bm.bmWidth);
         ac[1].rPos.top = ac[1].rPos.bottom = (int)(prs->m_pPanStop.p[1] * (fp)bm.bmHeight);
         i = (int)(prs->m_pPanStop.p[2] * (fp)bm.bmWidth / 2);
         ac[1].rPos.left -= i;
         ac[1].rPos.right += i;
         i = (int)((fp)i / prs->m_fAspect);
         ac[1].rPos.top -= i;
         ac[1].rPos.bottom += i;

         pControl->Message (ESCM_IMAGERECTSET, &eis);
      }
      return TRUE;

   case ESCN_COMBOBOXSELCHANGE:
      {
         PESCNCOMBOBOXSELCHANGE p = (PESCNCOMBOBOXSELCHANGE) pParam;
         PWSTR psz = p->pControl->m_pszName;
         if (!psz)
            break;

         if (!_wcsicmp(psz, L"resextra")) {
            int iVal = p->pszName ? _wtoi(p->pszName) : 0;
            if (iVal == prs->m_iResExtra)
               return TRUE;

            // else changed
            prs->m_iResExtra = iVal;
            (*prs->m_pfChanged) = TRUE;

            return TRUE;
         }
      }
      break;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;
         PWSTR psz;
         psz = p->pControl->m_pszName;

         if (!_wcsicmp(psz, L"changefadefromcolor")) {
            COLORREF cr;
            cr = AskColor (pPage->m_pWindow->m_hWnd, prs->m_cFadeFromColor, pPage, L"fadefromcolor");
            if (cr != prs->m_cFadeFromColor) {
               prs->m_cFadeFromColor = cr;
               (*prs->m_pfChanged) = TRUE;
            }
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"changefadetocolor")) {
            COLORREF cr;
            cr = AskColor (pPage->m_pWindow->m_hWnd, prs->m_cFadeToColor, pPage, L"fadetocolor");
            if (cr != prs->m_cFadeToColor) {
               prs->m_cFadeToColor = cr;
               (*prs->m_pfChanged) = TRUE;
            }
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"changetransparent")) {
            COLORREF cr;
            cr = AskColor (pPage->m_pWindow->m_hWnd, prs->m_cTransparent, pPage, L"transparent");
            if (cr != prs->m_cTransparent) {
               prs->m_cTransparent = cr;
               (*prs->m_pfChanged) = TRUE;
            }
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"usetransparent")) {
            (*prs->m_pfChanged) = TRUE;
            prs->m_fUseTransparent = p->pControl->AttribGetBOOL (Checked());
            return TRUE;
         }
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
            prs->m_dwTab = (DWORD)_wtoi(p->psz + dwLen);
            pPage->Exit (RedoSamePage());
            return TRUE;
         }

      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         PWSTR pszIfTab = L"IFTAB", pszEndIfTab = L"ENDIFTAB";
         DWORD dwIfTabLen = (DWORD)wcslen(pszIfTab), dwEndIfTabLen = (DWORD)wcslen(pszEndIfTab);

         if (!wcsncmp (p->pszSubName, pszIfTab, dwIfTabLen)) {
            DWORD dwNum = _wtoi(p->pszSubName + dwIfTabLen);
            if (dwNum == prs->m_dwTab)
               p->pszSubString = L"";
            else
               p->pszSubString = L"<comment>";
            return TRUE;
         }
         else if (!wcsncmp (p->pszSubName, pszEndIfTab, dwEndIfTabLen)) {
            DWORD dwNum = _wtoi(p->pszSubName + dwEndIfTabLen);
            if (dwNum == prs->m_dwTab)
               p->pszSubString = L"";
            else
               p->pszSubString = L"</comment>";
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"RSTABS")) {
            PWSTR apsz[] = {
               L"Fade in from color",
               L"Fade in from bkgnd",
               L"Fade out to color",
               L"Transparency",
               L"Zoom/pan",
            };
            PWSTR apszHelp[] = {
               L"Controls how the image fades in from a color.",
               L"Controls how the image fades in over the existing image.",
               L"Controls how the image fades out to a color.",
               L"Lets you make part of the image transparent.",
               L"Use this to zoom in/out of the image, or to pan."
            };
            DWORD adwID[] = {
               0,
               2,
               1,
               3,
               4
            };

            CListFixed lSkip;
            lSkip.Init (sizeof(DWORD));
            if (prs->m_f360) {
               DWORD dw = 4;
               lSkip.Add (&dw);
            }

            p->pszSubString = RenderSceneTabs (prs->m_dwTab, sizeof(apsz)/sizeof(PWSTR), apsz,
               apszHelp, adwID, lSkip.Num(), (DWORD*)lSkip.Get(0));
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Tranisiton";
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"IMAGEDRAG")) {
            MemZero (&gMemTemp);

            MemCat (&gMemTemp, L"<imagedrag name=image clickmode=");
            //switch (prs->m_fReadOnly ? 10000 : prs->m_dwTab) {
            //case 4:
            //   // click and drag
            //   MemCat (&gMemTemp, L"2");
            //   break;
            //default: // others (including RO)... cant click
               MemCat (&gMemTemp, L"0");
            //   break;
            // }
            MemCat (&gMemTemp, L" border=2 width=");
            int iWidth = 90;  // percent
            if (prs->m_fAspect < 1)
               iWidth = (int)((fp)iWidth * prs->m_fAspect);

            MemCat (&gMemTemp, iWidth);

            MemCat (&gMemTemp, L"% hbitmap=");
            WCHAR szTemp[32];
            swprintf (szTemp, L"%lx", (__int64)prs->m_hBmp);
            MemCat (&gMemTemp, szTemp);

            MemCat (&gMemTemp, L"/>");
            p->pszSubString = (PWSTR)gMemTemp.p;
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"LIBENABLE")) {
            if (prs && prs->m_fReadOnly)
               p->pszSubString = L"enabled=false";
            else
               p->pszSubString = L"";
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"LIBREADONLY")) {
            if (prs && prs->m_fReadOnly)
               p->pszSubString = L"readonly=true";
            else
               p->pszSubString = L"";
            return TRUE;
         }
      }
      break;

   }; // dwMessage

   return FALSE;
}


/*************************************************************************************
CTransition::Dialog - Bring up UI to edit this.

inputs
   PCEscWindow          pWindow - Window to display on
   HBITMAP              hBmp - Bitmap to display for the transition
   fp                   fAspect - Expected aspect ratio, width/height.
   BOOL                 f360 - If the image is 360 then no panning
   BOOL                 fReadOnly - If TRUE then readonly
   BOOL                 *pfChanged - Set to TRUE if changed the data
returns
   BOOL - TRUE if user pressed bakc, FALSE if cancelled.
*/
BOOL CTransition::Dialog (PCEscWindow pWindow, HBITMAP hBmp, fp fAspect,
                          BOOL f360, BOOL fReadOnly, BOOL *pfChanged)
{
   *pfChanged = FALSE;
   m_pfChanged = pfChanged;
   m_iVScroll = 0;
   m_hBmp = hBmp;
   m_fAspect = fAspect;
   m_fReadOnly = fReadOnly;
   m_dwTab = 0;
   m_f360 = f360;

   PWSTR psz;
redo:
   psz = pWindow->PageDialog (ghInstance, IDR_MMLTRANSITION, TransitionPage, this);
   m_iVScroll = pWindow->m_iExitVScroll;
   if (psz && !_wcsicmp(psz, RedoSamePage()))
      goto redo;

   return (psz && !_wcsicmp(psz, Back()));
}



/*************************************************************************************
CTransition::Scale - Looks at m_iResExtra and returns a scale that can be passed to
RenderSceneAspectToPixels().
*/
fp CTransition::Scale (void)
{
   return pow (2, (fp)m_iResExtra / 2.0);
}


/*************************************************************************************
CTransition::HotSpotRemap - Remaps from a pixel (thought to be on the original image)
into a pixel that is zoomed. (If zooming is taking place in the animation.)

inputs
   POINT          *pPixel - Originally should be filled in with the pixel as
                  it appears in the image (assuming that zoomed to show entire image).
                  This may change the value to simulate the pixel from the zoom.
   DWORD          dwWidth - Width of the image
   DWORD          dwHeight - Height of the image
returns
   none
*/
void CTransition::HotSpotRemap (POINT *pPixel, DWORD dwWidth, DWORD dwHeight)
{
   if (m_fPanStartTime <= 0)
      return;  // nothing

   // animate
   fp fTime;
   if (m_fAnimateTime >= m_fPanStopTime)
      fTime = 1;
   else if (m_fAnimateTime > m_fPanStartTime)
      fTime = (m_fAnimateTime - m_fPanStartTime) / (m_fPanStopTime - m_fPanStartTime);
   else
      fTime = 0;

   fp fX, fY, fScale;
   fX = (1.0 - fTime) * m_pPanStart.p[0] + fTime * m_pPanStop.p[0];
   fY = (1.0 - fTime) * m_pPanStart.p[1] + fTime * m_pPanStop.p[1];
   fScale = (1.0 - fTime) * m_pPanStart.p[2] + fTime * m_pPanStop.p[2];
   fScale = max(fScale, 0.01);


   CMatrix m; // temp
   
   // convert from image coords (0..width,0..height) to image coords after zoom
   CMatrix mAllToZoom;

   // scale so 0..1 by 0..1 covers entire image
   mAllToZoom.Scale (1.0 / (fp)dwWidth, 1.0 / (fp)dwHeight, 1);

   // offset so zooms around the center of zoom loc
   m.Translation (-0.5, -0.5, 0);
   mAllToZoom.MultiplyRight (&m);

   // do the inverse of the zoom
   m.Scale (fScale, fScale, 1);
   mAllToZoom.MultiplyRight (&m);

   // translate so the zoom location is centered
   m.Translation (fX, fY, 0);
   mAllToZoom.MultiplyRight (&m);

   // scale up to original image width and height
   m.Scale (dwWidth, dwHeight, 1);
   mAllToZoom.MultiplyRight (&m);


   // determine the new point locations
   CPoint p;
   p.Zero();
   p.p[0] = pPixel->x;
   p.p[1] = pPixel->y;
   p.MultiplyLeft (&mAllToZoom);
   pPixel->x = (int)p.p[0];
   pPixel->y = (int)p.p[1];
}
