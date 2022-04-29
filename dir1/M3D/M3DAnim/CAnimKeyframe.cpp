/************************************************************************
CAnimKeyframe.cpp - Object code
*/

#include <windows.h>
#include <math.h>
#include "escarpment.h"
#include "..\M3D.h"
#include "resource.h"

/************************************************************************
CAnimKeyframe::Constructor and destructor

inputs
   DWORD       dwType - 0 for keyframe excluding motion, 1 for location, 2 for rotation
*/
CAnimKeyframe::CAnimKeyframe (DWORD dwType)
{
   m_gClass = CLSID_AnimKeyframe;
   m_dwType = dwType;
   m_iPriority = 0;
   m_iMinDisplayX = 16;
   m_iMinDisplayY = dwType ? 16 : 32;
   m_fTimeMax = 0;
   m_fAnimObjSplineSecondPass = TRUE;
   m_dwLinear = 2;   // default to spline

   m_lATTRIBVAL.Init (sizeof(ATTRIBVAL));
}

CAnimKeyframe::~CAnimKeyframe (void)
{
   // do nothing for now
}

/************************************************************************
CAnimKeyframe::Delete - See CAnimSocket for info
*/
void CAnimKeyframe::Delete (void)
{
   delete this;
}

/************************************************************************
CAnimKeyframe::AnimObjSplineApply - See CAnimSocket for info
*/
void CAnimKeyframe::AnimObjSplineApply (PCSceneObj pSpline)
{
   // loop through all the attributes and set them
   DWORD i;
   DWORD dwNum = m_lATTRIBVAL.Num();
   PATTRIBVAL pav = (PATTRIBVAL) m_lATTRIBVAL.Get(0);
   for (i = 0; i < dwNum; i++, pav++) {
      PCAnimAttrib paa = pSpline->AnimAttribGet (pav->szName, TRUE);
      if (!paa)
         continue;

      TEXTUREPOINT tp;
      tp.h = m_pLoc.p[0];
      tp.v = pav->fValue;
      paa->PointAdd (&tp, m_dwLinear);
   }
}

/************************************************************************
CAnimKeyframe::Clone - See CAnimSocket for info
*/
CAnimSocket *CAnimKeyframe::Clone (void)
{
   PCAnimKeyframe pNew = new CAnimKeyframe (m_dwType);
   if (!pNew)
      return NULL;
   CloneTemplate (pNew);

   pNew->m_dwType = m_dwType;
   pNew->m_dwLinear = m_dwLinear;
   pNew->m_lATTRIBVAL.Init (sizeof(ATTRIBVAL), m_lATTRIBVAL.Get(0), m_lATTRIBVAL.Num());

   return pNew;
}

/************************************************************************
CAnimKeyframe::MMLTo - See CAnimSocket for info
*/
static PWSTR gpszName = L"Name";
static PWSTR gpszVal = L"Val";
static PWSTR gpszType = L"Type";
static PWSTR gpszLinear = L"Linear";

PCMMLNode2 CAnimKeyframe::MMLTo (void)
{
   PCMMLNode2 pNode = MMLToTemplate ();
   if (!pNode)
      return NULL;

   MMLValueSet (pNode, gpszType, (int) m_dwType);
   MMLValueSet (pNode, gpszLinear, (int) m_dwLinear);

   // write out attributes
   DWORD i;
   PATTRIBVAL pav;
   pav = (PATTRIBVAL) m_lATTRIBVAL.Get(0);
   for (i = 0; i < m_lATTRIBVAL.Num(); i++, pav++) {
      PCMMLNode2 pSub;
      if (!pav->szName[0])
         continue;   // must have name
      pSub = pNode->ContentAddNewNode ();
      if (!pSub)
         continue;
      pSub->NameSet (Attrib());

      MMLValueSet (pSub, gpszName, pav->szName);
      MMLValueSet (pSub, gpszVal, pav->fValue);
   }

   return pNode;
}

/************************************************************************
CAnimKeyframe::MMLFrom - See CAnimSocket for info
*/
BOOL CAnimKeyframe::MMLFrom (PCMMLNode2 pNode)
{
   if (!MMLFromTemplate (pNode))
      return FALSE;

   m_dwLinear = (DWORD) MMLValueGetInt (pNode, gpszLinear, 2); // default to spline
   m_dwType = (DWORD) MMLValueGetInt (pNode, gpszType, 0);
   m_iMinDisplayY = m_dwType ? 16 : 32;

   // read in attributes
   DWORD i;
   m_lATTRIBVAL.Clear();
   PWSTR psz;
   PCMMLNode2 pSub;
   ATTRIBVAL av;
   memset (&av, 0, sizeof(av));
   m_lATTRIBVAL.Required (pNode->ContentNum());
   for (i = 0; i < pNode->ContentNum(); i++) {
      pSub = NULL;
      pNode->ContentEnum (i, &psz, &pSub);
      if (!pSub)
         continue;
      psz = pSub->NameGet();
      if (!psz)
         continue;
      if (!_wcsicmp(psz, Attrib())) {
         psz = MMLValueGet (pSub, gpszName);
         if (!psz)
            continue;
         wcscpy (av.szName, psz);
         av.fValue = MMLValueGetDouble (pSub, gpszVal, 0);
         m_lATTRIBVAL.Add (&av);
         continue;
      }
   }

   return TRUE;
}


/************************************************************************
CAnimKeyframe::Draw - See CAnimSocket for info
*/
void CAnimKeyframe::Draw (HDC hDC, RECT *prHDC, fp fLeft, fp fRight)
{
   // overwrite with bitmap
   HDC hDCNew;
   HBITMAP hBmp;
   hDCNew = CreateCompatibleDC (hDC);
   DWORD dwBmp;
   if (m_dwType == 1)
      dwBmp = IDB_ANIMKEYFRAMELOC;
   else if (m_dwType == 2)
      dwBmp = IDB_ANIMKEYFRAMEROT;
   else
      dwBmp = IDB_ANIMKEYFRAME;
   hBmp = LoadBitmap (ghInstance, MAKEINTRESOURCE(dwBmp));
   SelectObject (hDCNew, hBmp);

   // get the size of the bitmap
   BITMAP   bm;
   GetObject (hBmp, sizeof(bm), &bm);

   BitBlt (hDC, prHDC->left, prHDC->top, bm.bmWidth, bm.bmHeight,
      hDCNew, 0, 0, SRCCOPY);

   DeleteDC (hDCNew);
   DeleteObject (hBmp);
}

/***********************************************************************
ATTRIBVALSort */
static int _cdecl ATTRIBVALSort (const void *elem1, const void *elem2)
{
   ATTRIBVAL *pdw1, *pdw2;
   pdw1 = (ATTRIBVAL*) elem1;
   pdw2 = (ATTRIBVAL*) elem2;

   return _wcsicmp (pdw1->szName, pdw2->szName);
}


/************************************************************************
CAnimKeyframe::DialogQuery - See CAnimSocket for info
*/
BOOL CAnimKeyframe::DialogQuery (void)
{
   return TRUE;
}

/****************************************************************************
AnimKeyframePage
*/
BOOL AnimKeyframePage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCAnimKeyframe pas = (PCAnimKeyframe) pPage->m_pUserData;
   PCListFixed plATTRIBVAL = &pas->m_lATTRIBVAL;
   PATTRIBVAL pav = (PATTRIBVAL) plATTRIBVAL->Get(0);
   DWORD dwNumAV = plATTRIBVAL->Num();

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         DWORD i;
         WCHAR szTemp[64];
         PCEscControl pControl;

         // set the radio buttons
         if (pas->m_dwLinear == 0)
            pControl = pPage->ControlFind (L"constant");
         else if (pas->m_dwLinear == 1)
            pControl = pPage->ControlFind (L"linear");
         else
            pControl = pPage->ControlFind (L"spline");
         if (pControl)
            pControl->AttribSetBOOL (Checked(), TRUE);

         // get the object
         PCObjectSocket pos;
         ATTRIBINFO ai;
         pos = pas->m_pWorld->ObjectGet(pas->m_pWorld->ObjectFind (&pas->m_gWorldObject));

         for (i = 0; i < dwNumAV; i++) {
            fp fVal = pav[i].fValue;

            // get the info
            memset (&ai, 0, sizeof(ai));
            if (pos)
               pos->AttribInfo (pav[i].szName, &ai);

            if (ai.dwType == AIT_BOOL) {
               swprintf (szTemp, L"b:%d", (int)i);
               pControl = pPage->ControlFind(szTemp);
               if (pControl)
                  pControl->AttribSetBOOL (Checked(), fVal > (ai.fMax + ai.fMin)/2);
            }
            else {
               // set edit
               swprintf (szTemp, L"e:%d", (int) i);
               if (ai.dwType == AIT_DISTANCE)
                  MeasureToString (pPage, szTemp, fVal);
               else if (ai.dwType == AIT_ANGLE)
                  AngleToControl (pPage, szTemp, fVal, TRUE);
               else
                  DoubleToControl (pPage, szTemp, fVal);

               // set the slider
               fVal = (fVal - ai.fMin) /
                  max(CLOSE, ai.fMax - ai.fMin) * 100.0;
               fVal = max(0, fVal);
               fVal = min(100, fVal);
               swprintf (szTemp, L"s:%d", (int) i);
               pControl = pPage->ControlFind (szTemp);
               if (pControl)
                  pControl->AttribSetInt (Pos(), (int) fVal);
            }
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

         if (!_wcsicmp(psz, L"constant")) {
            if (pas->m_pScene && pas->m_pScene->SceneSetGet())
               pas->m_pScene->SceneSetGet()->ObjectAboutToChange (pas);
            pas->m_dwLinear = 0;
            if (pas->m_pScene && pas->m_pScene->SceneSetGet())
               pas->m_pScene->SceneSetGet()->ObjectChanged (pas);
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"linear")) {
            if (pas->m_pScene && pas->m_pScene->SceneSetGet())
               pas->m_pScene->SceneSetGet()->ObjectAboutToChange (pas);
            pas->m_dwLinear = 1;
            if (pas->m_pScene && pas->m_pScene->SceneSetGet())
               pas->m_pScene->SceneSetGet()->ObjectChanged (pas);
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"spline")) {
            if (pas->m_pScene && pas->m_pScene->SceneSetGet())
               pas->m_pScene->SceneSetGet()->ObjectAboutToChange (pas);
            pas->m_dwLinear = 2;
            if (pas->m_pScene && pas->m_pScene->SceneSetGet())
               pas->m_pScene->SceneSetGet()->ObjectChanged (pas);
            return TRUE;
         }
         else if ((psz[0] == L'b') && (psz[1] == L':')) {
            // index
            DWORD dwIndex = _wtoi(psz + 2);
            if (dwIndex >= dwNumAV)
               return TRUE;   // shouldnt happen

            // get the object info
            PCObjectSocket pos;
            ATTRIBINFO ai;
            pos = pas->m_pWorld->ObjectGet(pas->m_pWorld->ObjectFind (&pas->m_gWorldObject));
            memset (&ai, 0, sizeof(ai));
            if (pos)
               pos->AttribInfo (pav[dwIndex].szName, &ai);

            if (pas->m_pScene && pas->m_pScene->SceneSetGet())
               pas->m_pScene->SceneSetGet()->ObjectAboutToChange (pas);

            // set the attribute
            pav[dwIndex].fValue = p->pControl->AttribGetBOOL(Checked()) ?
               ai.fMax : ai.fMin;

            if (pas->m_pScene && pas->m_pScene->SceneSetGet())
               pas->m_pScene->SceneSetGet()->ObjectChanged (pas);

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
         PWSTR psz;
         psz = p->pControl->m_pszName;


         if ((psz[0] == L's') && (psz[1] == L':')) {
            // index
            DWORD dwIndex = _wtoi(psz + 2);
            if (dwIndex >= dwNumAV)
               return TRUE;   // shouldnt happen

            // get the object info
            PCObjectSocket pos;
            ATTRIBINFO ai;
            pos = pas->m_pWorld->ObjectGet(pas->m_pWorld->ObjectFind (&pas->m_gWorldObject));
            memset (&ai, 0, sizeof(ai));
            if (pos)
               pos->AttribInfo (pav[dwIndex].szName, &ai);

            // get the value
            fp fVal;
            fVal = p->pControl->AttribGetInt (Pos()) / 100.0 *
               (ai.fMax - ai.fMin) + ai.fMin;


            // set the edit
            WCHAR szTemp[64];
            swprintf (szTemp, L"e:%d", (int) dwIndex);
            if (ai.dwType == AIT_DISTANCE)
               MeasureToString (pPage, szTemp, fVal);
            else if (ai.dwType == AIT_ANGLE)
               AngleToControl (pPage, szTemp, fVal, TRUE);
            else
               DoubleToControl (pPage, szTemp, fVal);


            if (pas->m_pScene && pas->m_pScene->SceneSetGet())
               pas->m_pScene->SceneSetGet()->ObjectAboutToChange (pas);

            // set the attribute
            pav[dwIndex].fValue = fVal;

            if (pas->m_pScene && pas->m_pScene->SceneSetGet())
               pas->m_pScene->SceneSetGet()->ObjectChanged (pas);

            return TRUE;
         }
      }
      break;

   case ESCN_EDITCHANGE:
      {
         PESCNEDITCHANGE p = (PESCNEDITCHANGE) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;
         PWSTR psz;
         psz = p->pControl->m_pszName;

         if ((psz[0] == L'e') && (psz[1] == L':')) {
            // index
            DWORD dwIndex = _wtoi(psz + 2);
            if (dwIndex >= dwNumAV)
               return TRUE;   // shouldnt happen

            // get the object info
            PCObjectSocket pos;
            ATTRIBINFO ai;
            pos = pas->m_pWorld->ObjectGet(pas->m_pWorld->ObjectFind (&pas->m_gWorldObject));
            memset (&ai, 0, sizeof(ai));
            if (pos)
               pos->AttribInfo (pav[dwIndex].szName, &ai);

            // get the value
            fp fVal;
            fVal = 0;
            if (ai.dwType == AIT_DISTANCE)
               MeasureParseString (pPage, psz, &fVal);
            else if (ai.dwType == AIT_ANGLE)
               fVal = AngleFromControl (pPage, psz);
            else
               fVal = DoubleFromControl (pPage, psz);


            if (pas->m_pScene && pas->m_pScene->SceneSetGet())
               pas->m_pScene->SceneSetGet()->ObjectAboutToChange (pas);
            // set the attribute
            pav[dwIndex].fValue = fVal;
            if (pas->m_pScene && pas->m_pScene->SceneSetGet())
               pas->m_pScene->SceneSetGet()->ObjectChanged (pas);

            // set the slider
            PCEscControl pControl;
            WCHAR szTemp[64];
            fVal = (fVal - ai.fMin) /
               max(CLOSE, ai.fMax - ai.fMin) * 100.0;
            fVal = max(0, fVal);
            fVal = min(100, fVal);
            swprintf (szTemp, L"s:%d", (int) dwIndex);
            pControl = pPage->ControlFind (szTemp);
            if (pControl)
               pControl->AttribSetInt (Pos(), (int) fVal);

            return TRUE;
         }
      }
      break;

   case ESCM_LINK:
      {
         PESCMLINK p = (PESCMLINK) pParam;
         if (!p->psz)
            break;
         PWSTR psz;
         psz = p->psz;

         if ((psz[0] == L'a') && (psz[1] == L':')) {
            // index
            DWORD dwIndex = _wtoi(psz + 2);
            if (dwIndex >= dwNumAV)
               return TRUE;   // shouldnt happen

            if (IDYES != pPage->MBYesNo (L"Are you sure you wish to remove this attribute?"))
               return TRUE;

            // delete it
            if (pas->m_pScene && pas->m_pScene->SceneSetGet())
               pas->m_pScene->SceneSetGet()->ObjectAboutToChange (pas);
            pas->m_lATTRIBVAL.Remove (dwIndex);
            if (pas->m_pScene && pas->m_pScene->SceneSetGet())
               pas->m_pScene->SceneSetGet()->ObjectChanged (pas);

            // exit and redraw
            pPage->Exit (RedoSamePage());
            return TRUE;
         }
         else if ((psz[0] == L'a') && (psz[1] == L'a') && (psz[2] == L':')) {
            // attribute
            ATTRIBVAL av;
            memset (&av, 0, sizeof(av));
            wcscpy (av.szName, psz + 3);

            // see if can get the attribute at the given time
            PCScene pScene;
            PCSceneObj pSceneObj;
            PCAnimAttrib paa;
            GUID gObjectWorld;
            pScene = NULL;
            pSceneObj = NULL;
            paa = NULL;
            pas->WorldGet (NULL, &pScene, &gObjectWorld);
            if (pScene)
               pSceneObj = pScene->ObjectGet (&gObjectWorld);
            if (pSceneObj) {
               pSceneObj->AnimAttribGenerate();
               paa = pSceneObj->AnimAttribGet (av.szName);
            }
            if (paa)
               av.fValue = paa->ValueGet (pas->m_pLoc.p[0]);
            else {
               // get the attribute as it stands now
               PCObjectSocket pos;
               pos = pas->m_pWorld->ObjectGet(pas->m_pWorld->ObjectFind (&pas->m_gWorldObject));
               if (pos)
                  pos->AttribGet (av.szName, &av.fValue);
            }

            // add it
            if (pas->m_pScene && pas->m_pScene->SceneSetGet())
               pas->m_pScene->SceneSetGet()->ObjectAboutToChange (pas);
            pas->m_lATTRIBVAL.Add (&av);
            if (pas->m_pScene && pas->m_pScene->SceneSetGet())
               pas->m_pScene->SceneSetGet()->ObjectChanged (pas);

            // exit and redraw
            pPage->Exit (RedoSamePage());
            return TRUE;
         }
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            switch (pas->m_dwType) {
            case 0:
            default:
               p->pszSubString = L"Keyframe (general) settings";
               break;
            case 1:  // location
               p->pszSubString = L"Keyframe (location) settings";
               break;
            case 2:  // rotation
               p->pszSubString = L"Keyframe (rotation) settings";
               break;
            }
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"TABLE")) {
            MemZero (&gMemTemp);
            // get the object info
            PCObjectSocket pos;
            ATTRIBINFO ai;
            pos = pas->m_pWorld->ObjectGet(pas->m_pWorld->ObjectFind (&pas->m_gWorldObject));

            if (!dwNumAV || !pos) {
               MemCat (&gMemTemp, L"<tr><td>"
                  L"At the moment this keyframe doesn't change any attributes. "
                  L"Press the \"Add\" button below to add some."
                  L"</td></tr>");
               p->pszSubString = (PWSTR) gMemTemp.p;
               return TRUE;
            }

            DWORD i;
            for (i = 0; i < dwNumAV; i++) {
               memset (&ai, 0, sizeof(ai));
               pos->AttribInfo (pav[i].szName, &ai);

               // row
               MemCat (&gMemTemp, L"<tr>");

               // show the name
               MemCat (&gMemTemp, L"<td width=50% valign=center>");

               // name
               MemCat (&gMemTemp, L"<a href=\"a:");
               MemCat (&gMemTemp, (int) i);
               MemCat (&gMemTemp, L"\">");
               MemCatSanitize (&gMemTemp, pav[i].szName);
               if (ai.pszDescription) {
                  MemCat (&gMemTemp, L"<xHoverHelp>");
                  MemCatSanitize (&gMemTemp, (PWSTR) ai.pszDescription);
                  MemCat (&gMemTemp, L"</xHoverHelp>");
               }
               MemCat (&gMemTemp, L"</a>");
               MemCat (&gMemTemp, L"</td>");

               // scrollbar or on/off
               if (ai.dwType == AIT_BOOL) {
                  MemCat (&gMemTemp, L"<td width=75% align=right><button checkbox=true style=x name=b:");
                  MemCat (&gMemTemp, (int) i);
                  MemCat (&gMemTemp, L"/></td>");
               }
               else {   // edit and scrollbar
                  // edit
                  MemCat (&gMemTemp, L"<td width=25%><edit maxchars=32 width=100% selall=true name=e:");
                  MemCat (&gMemTemp, (int) i);
                  MemCat (&gMemTemp, L"/></td>");

                  // scroll
                  MemCat (&gMemTemp, L"<td width=50%><scrollbar orient=horz min=0 max=100 width=100% name=s:");
                  MemCat (&gMemTemp, (int) i);
                  MemCat (&gMemTemp, L"/></td>");
               }

               // end row
               MemCat (&gMemTemp, L"</tr>");
            }

            p->pszSubString = (PWSTR) gMemTemp.p;
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"TABLEADD")) {
            MemZero (&gMemTemp);
            // get the object info
            PCObjectSocket pos;
            CListFixed lAV;
            lAV.Init (sizeof(ATTRIBVAL));
            pos = pas->m_pWorld->ObjectGet(pas->m_pWorld->ObjectFind (&pas->m_gWorldObject));
            if (pos)
               pos->AttribGetAll (&lAV);

            // remove items in the list that already exist or which shouldnt be in this
            // type of keyframe
            DWORD i, j;
            PATTRIBVAL pvCur;
            for (i = lAV.Num()-1; i < lAV.Num(); i--) {
               pvCur = (PATTRIBVAL)lAV.Get(i);

               // see if it's in the existing list
               for (j = 0; j < dwNumAV; j++)
                  if (!_wcsicmp(pav[j].szName, pvCur->szName))
                     break;
               if (j < dwNumAV) {
                  lAV.Remove(i);
                  continue;
               }

               // Remove attributes that shouldnt be in this template type
               BOOL fRemove;
               DWORD dwLen;
               dwLen = (DWORD)wcslen(pvCur->szName);
               fRemove = !pas->SupportAttribute (pvCur->szName, dwLen);
               if (fRemove) {
                  lAV.Remove(i);
                  continue;
               }
            }

            // overwrite variables for convenience
            dwNumAV = lAV.Num();
            pav = (PATTRIBVAL) lAV.Get(0);
            qsort (pav, dwNumAV, sizeof(ATTRIBVAL), ATTRIBVALSort);
            if (!dwNumAV || !pos) {
               MemCat (&gMemTemp, L"<tr><td>"
                  L"There are no attributes left to add to the keyframe."
                  L"</td></tr>");
               p->pszSubString = (PWSTR) gMemTemp.p;
               return TRUE;
            }

            ATTRIBINFO ai;
            for (i = 0; i < dwNumAV; i++) {
               memset (&ai, 0, sizeof(ai));
               pos->AttribInfo (pav[i].szName, &ai);

               // row
               MemCat (&gMemTemp, L"<li>");

               // name
               MemCat (&gMemTemp, L"<a href=\"aa:");
               MemCatSanitize (&gMemTemp, pav[i].szName);
               MemCat (&gMemTemp, L"\">");
               MemCatSanitize (&gMemTemp, pav[i].szName);
               MemCat (&gMemTemp, L"</a>");
               if (ai.pszDescription) {
                  MemCat (&gMemTemp, L" - ");
                  MemCatSanitize (&gMemTemp, (PWSTR) ai.pszDescription);
               }
               MemCat (&gMemTemp, L"</li>");
            }

            p->pszSubString = (PWSTR) gMemTemp.p;
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}

/************************************************************************
CAnimKeyframe::DialogShow - See CAnimSocket for info
*/
BOOL CAnimKeyframe::DialogShow (PCEscWindow pWindow)
{
   PWSTR pszRet;

mainpage:
   // sort the attributes before calling this
   qsort (m_lATTRIBVAL.Get(0), m_lATTRIBVAL.Num(), sizeof(ATTRIBVAL), ATTRIBVALSort);

   pszRet = pWindow->PageDialog (ghInstance, IDR_MMLANIMKEYFRAME, AnimKeyframePage, this);

   if (!pszRet)
      return FALSE;
   if (!_wcsicmp (pszRet, RedoSamePage()))
      goto mainpage;

   return !_wcsicmp(pszRet, Back());
}

/************************************************************************
CAnimKeyframe::Message - See CAnimSocket for info
*/
BOOL CAnimKeyframe::Message (DWORD dwMessage, PVOID pParam)
{
   switch (dwMessage) {
   case OSM_KEYFRAME:
      {
         POSMKEYFRAME p = (POSMKEYFRAME) pParam;
         p->pak = this;
         return TRUE;
      }
      break;
   }
   return FALSE;
}

/************************************************************************
CAnimKeyframe::Deconstruct - See CAnimSocket for info
*/
BOOL CAnimKeyframe::Deconstruct (BOOL fAct)
{
   // while other objets can auomagically be deconstructed into keyframes, this
   // is already a keyframe so it can't
   return FALSE;
}

/************************************************************************
CAnimKeyframe::Merge - See CAnimSocket for info
*/
BOOL CAnimKeyframe::Merge (GUID *pagWith, DWORD dwNum)
{
   // get the object
   PCSceneObj pSceneObj;
   if (!m_pScene)
      return FALSE;
   pSceneObj = m_pScene->ObjectGet (&m_gWorldObject);
   if (!pSceneObj)
      return FALSE;

   // loop through all the objects
   DWORD i;
   PCAnimSocket pas;
   DWORD dwIndex;
   OSMKEYFRAME osk;
   BOOL fRet;
   fRet = FALSE;
   for (i = 0; i < dwNum; i++, pagWith++) {
      // get it
      dwIndex = pSceneObj->ObjectFind (pagWith);
      pas = pSceneObj->ObjectGet (dwIndex);
      if (!pas)
         continue;

      // only merge if it's a keyframe
      memset (&osk, 0, sizeof(osk));
      if (!pas->Message (OSM_KEYFRAME, &osk) || !osk.pak || (osk.pak == this))
         continue;   // not a keyframe

      // must be the same type
      if ((osk.pak->m_dwLinear != m_dwLinear) || (osk.pak->m_dwType != m_dwType))
         continue;

      // must be the same time
      fp fStart, fEnd;
      pas->TimeGet (&fStart, &fEnd);
      if (fStart != m_pLoc.p[0])
         continue;

      // else, can merge
      PATTRIBVAL pav;
      DWORD dwNum, j;
      pav = (PATTRIBVAL) osk.pak->m_lATTRIBVAL.Get(0);
      dwNum = osk.pak->m_lATTRIBVAL.Num();
      for (j = 0; j < dwNum; j++, pav++)
         AttributeSet (pav->szName, pav->fValue, TRUE);

      // remove the old one
      pSceneObj->ObjectRemove (dwIndex);
      fRet = TRUE;
   }

   return fRet;
}

/************************************************************************
CAnimKeyframe::SyncFromWorld - Tells the keyframe to syncronize all its attributes
to the ones currently set in the world.
*/
void CAnimKeyframe::SyncFromWorld (void)
{
   // get the attributes from the object
   if (!m_pWorld)
      return;
   PCObjectSocket pos;
   pos = m_pWorld->ObjectGet (m_pWorld->ObjectFind (&m_gWorldObject));
   if (!pos)
      return;

   CListFixed lAttrib;
   lAttrib.Init (sizeof(ATTRIBVAL));
   pos->AttribGetAll (&lAttrib);

   // remove those attributes not supported
   DWORD i;
   PATTRIBVAL pav;
   for (i = lAttrib.Num()-1; i < lAttrib.Num(); i--) {
      pav = (PATTRIBVAL) lAttrib.Get(i);

      if (!SupportAttribute (pav->szName, (DWORD)wcslen(pav->szName)))
         lAttrib.Remove (i);
   }

   // what's left is the list of attributes
   if (m_pScene && m_pScene->SceneSetGet())
      m_pScene->SceneSetGet()->ObjectAboutToChange (this);
   m_lATTRIBVAL.Init (sizeof(ATTRIBVAL), lAttrib.Get(0), lAttrib.Num());
   if (m_pScene && m_pScene->SceneSetGet())
      m_pScene->SceneSetGet()->ObjectChanged (this);

}

/************************************************************************
CAnimKeyframe::SupportAttribute - Returns TRUE if the keyframe supports
this type of attribute, FALSE if it doesnt. Some keyframes only support location, etc.

inputs
   PWSTR       pszName - Attribute
   DWORD       dwLen - wcslen(pszName)
returns
   BOOL - TRUE if it supports it
*/
BOOL CAnimKeyframe::SupportAttribute (PWSTR pszName, DWORD dwLen)
{
   switch (m_dwType) {
   case 1:  // location
      return IsTemplateAttributeLoc(pszName, dwLen);

   case 2:  // rotation
      return IsTemplateAttributeRot(pszName, dwLen);

   case 0:  // other
   default:
      return !IsTemplateAttributeLoc(pszName, dwLen) &&
         !IsTemplateAttributeRot(pszName, dwLen);
   }
}

/************************************************************************
CAnimKeyframe::AttributeSet - Sets an attribute of the keyframe.
This assumes that SupportAttribute() has already been called to weed
out unacceptable attributes.

If the attribute already exsits its new value is set to fValue.
If it doesn't already exist, returns FALSE. UNLESS, fCreateIfNotExist
is TRUE then the attribute will be set.

inputs
   PWSTR          pszName - Attribute name
   fp             fValue - Value
   BOOL           fCreateIfNotExist - If the attribute doesn't already
                     exist then this will create it.
returns
   BOOL - TRUE if attribute was rememberd, FALSE if not
*/
BOOL CAnimKeyframe::AttributeSet (PWSTR pszName, fp fValue, BOOL fCreateIfNotExist)
{
   // see if can find it
   PATTRIBVAL pav = (PATTRIBVAL) m_lATTRIBVAL.Get(0);
   DWORD dwNum = m_lATTRIBVAL.Num();
   DWORD i;
   for (i = 0; i < dwNum; i++) {
      if (!_wcsicmp(pszName, pav[i].szName)) {
         // found a match
         if (m_pScene && m_pScene->SceneSetGet())
            m_pScene->SceneSetGet()->ObjectAboutToChange (this);

         // set the attribute
         pav[i].fValue = fValue;

         if (m_pScene && m_pScene->SceneSetGet())
            m_pScene->SceneSetGet()->ObjectChanged (this);


         return TRUE;
      }
   }

   // if gets here cant find
   if (!fCreateIfNotExist)
      return FALSE;  // not going to add

   // else, create
   ATTRIBVAL av;
   memset (&av, 0, sizeof(av));
   wcscpy (av.szName, pszName);
   av.fValue = fValue;

   if (m_pScene && m_pScene->SceneSetGet())
      m_pScene->SceneSetGet()->ObjectAboutToChange (this);
   m_lATTRIBVAL.Add (&av);
   if (m_pScene && m_pScene->SceneSetGet())
      m_pScene->SceneSetGet()->ObjectChanged (this);


   return TRUE;
}

/************************************************************************
CAnimKeyframe::AttribRemove - Removes the attribute from the object.

inputs
   PWSTR    pszName
returns
   BOOL - TRUE if success
*/
BOOL CAnimKeyframe::AttribRemove (PWSTR pszName)
{
   // see if can find it
   PATTRIBVAL pav = (PATTRIBVAL) m_lATTRIBVAL.Get(0);
   DWORD dwNum = m_lATTRIBVAL.Num();
   DWORD i;
   for (i = 0; i < dwNum; i++) {
      if (!_wcsicmp(pszName, pav[i].szName)) {
         // found a match
         if (m_pScene && m_pScene->SceneSetGet())
            m_pScene->SceneSetGet()->ObjectAboutToChange (this);

         m_lATTRIBVAL.Remove (i);

         if (m_pScene && m_pScene->SceneSetGet())
            m_pScene->SceneSetGet()->ObjectChanged (this);

         return TRUE;
      }
   }

   // else none
   return FALSE;
}


// FUTURE RELEASE - UI option to split in two


// BUGBUG - If merge loc/rot together over longer timeframe create a motion path?

// BUGBUG - If merge several keyframes together form a pose? Or over time a pose transition
// (aka: motion)?

// BUGBUG - If merge multiple motions create a walk path?
