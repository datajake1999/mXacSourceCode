/************************************************************************
CObjectHairLock.cpp - Draws a HairLock.

begun 26/11/03 by Mike Rozak
Copyright 2003 Mike Rozak. All rights reserved
*/

#include <windows.h>
#include "escarpment.h"
#include "..\M3D.h"
#include "resource.h"


#define HAIRLOCKBASE       100




typedef struct {
   PWSTR       pszName;    // name of the move point
   int         iX, iY; // -1 if for left/top, 1 for right/bottom
} SPLINEMOVEP, *PSPLINEMOVEP;

static SPLINEMOVEP   gaHairMove[] = {
   L"Root", 0, 0
};






/**********************************************************************************
CObjectHairLock::Constructor and destructor */
CObjectHairLock::CObjectHairLock (PVOID pParams, POSINFO pInfo)
{
   m_dwRenderShow = RENDERSHOW_CHARACTER;
   m_OSINFO = *pInfo;

   m_fBackfaceCull = TRUE;

   CPoint apPoint[5];
   fp afTwist[5];
   memset (apPoint, 0, sizeof(apPoint));
   memset (afTwist, 0, sizeof(afTwist));
   apPoint[0].p[2] = -1;
   apPoint[2].p[0] = apPoint[2].p[2] = 1;
   apPoint[3].p[0] = 4;
   apPoint[3].p[2] = 1;
   apPoint[4].p[0] = 5;
   apPoint[4].p[1] = apPoint[4].p[2] = 1;
   m_HairLock.LengthSet (0.05);
   m_HairLock.DiameterSet (0.02);
   DWORD i;
   m_HairLock.SplineSet (5, apPoint, afTwist);

   // color for the HairHead
   ObjectSurfaceAdd (HAIRLOCKBASE, RGB(0x80, 0x80,0), MATERIAL_PAINTGLOSS, NULL,
      &GTEXTURECODE_Hair, &GTEXTURESUB_HairDefault);
   for (i = 1; i < MAXHAIRLAYERS; i++)
      ObjectSurfaceAdd (HAIRLOCKBASE+i, RGB(0xff,0,0xff), MATERIAL_PAINTGLOSS);
}


CObjectHairLock::~CObjectHairLock (void)
{
   // do nothing for now
}


/**********************************************************************************
CObjectHairLock::Delete - Called to delete this object
*/
void CObjectHairLock::Delete (void)
{
   delete this;
}

/**********************************************************************************
CObjectHairLock::Render - Draws a test box.

inputs
   POBJECTRENDER     pr - Render information
*/
void CObjectHairLock::Render (POBJECTRENDER pr, DWORD dwSubObject)
{
   // CRenderSurface rs;
   m_Renderrs.ClearAll();

   CMatrix mObject;
   m_Renderrs.Init (pr->pRS);
   ObjectMatrixGet (&mObject);
   m_Renderrs.Multiply (&mObject);

   m_HairLock.Render (pr, &m_Renderrs, this, HAIRLOCKBASE, m_fBackfaceCull, NULL, NULL);

   m_Renderrs.Commit();
}



/**********************************************************************************
CObjectHairLock::QueryBoundingBox - Standard API
*/
void CObjectHairLock::QueryBoundingBox (PCPoint pCorner1, PCPoint pCorner2, DWORD dwSubObject)
{
   m_HairLock.QueryBoundingBox (pCorner1, pCorner2, NULL, NULL);

#ifdef _DEBUG
   // test, make sure bounding box not too small
   CPoint p1,p2;
   DWORD i;
   CObjectTemplate::QueryBoundingBox (&p1, &p2, dwSubObject);
   for (i = 0; i < 3; i++)
      if ((p1.p[i] < pCorner1->p[i] - CLOSE) || (p2.p[i] > pCorner2->p[i] + CLOSE))
         break;
   if (i < 3)
      OutputDebugString ("\r\nCObjectHairLock::QueryBoundingBox too small.");
#endif
}

/**********************************************************************************
CObjectHairLock::Clone - Clones this

inputs
   none
returns
   PCObjectSocket - Clone
*/
PCObjectSocket CObjectHairLock::Clone (void)
{
   PCObjectHairLock pNew;

   pNew = new CObjectHairLock(NULL, &m_OSINFO);

   m_HairLock.CloneTo (&pNew->m_HairLock);
   pNew->m_fBackfaceCull = m_fBackfaceCull;

   // clone template info
   CloneTemplate(pNew);

   return pNew;
}


static PWSTR gpszHairLock = L"HairLock";
static PWSTR gpszBackfaceCull = L"BackfaceCull";

PCMMLNode2 CObjectHairLock::MMLTo (void)
{
   // call into the template first
   PCMMLNode2 pNode = MMLToTemplate();
   if (!pNode)
      return NULL;

   MMLValueSet (pNode, gpszBackfaceCull, (int)m_fBackfaceCull);

   PCMMLNode2 pSub = m_HairLock.MMLTo();
   if (pSub) {
      pSub->NameSet (gpszHairLock);
      pNode->ContentAdd (pSub);
   }

   return pNode;
}

BOOL CObjectHairLock::MMLFrom (PCMMLNode2 pNode)
{
   // call into the template first
   if (!MMLFromTemplate (pNode))
      return FALSE;

   m_fBackfaceCull = (BOOL) MMLValueGetInt (pNode, gpszBackfaceCull, (int)TRUE);

   PCMMLNode2 pSub;
   PWSTR psz;
   pSub = NULL;
   pNode->ContentEnum (pNode->ContentFind (gpszHairLock), &psz, &pSub);
   if (pSub)
      m_HairLock.MMLFrom (pSub);

   return TRUE;
}




/**************************************************************************************
CObjectHairLock::MoveReferencePointQuery - 
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
BOOL CObjectHairLock::MoveReferencePointQuery (DWORD dwIndex, PCPoint pp)
{
   PSPLINEMOVEP ps;
   DWORD dwDataSize;
   ps = gaHairMove;
   dwDataSize = sizeof(gaHairMove);

   if (dwIndex >= dwDataSize / sizeof(SPLINEMOVEP))
      return FALSE;

   // always at 0,0 in Hairs
   PCPoint pPath = m_HairLock.SplineGet (NULL, NULL);
   pp->Copy (pPath + 1);

   return TRUE;
}

/**************************************************************************************
CObjectHairLock::MoveReferenceStringQuery -
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
BOOL CObjectHairLock::MoveReferenceStringQuery (DWORD dwIndex, PWSTR psz, DWORD dwSize, DWORD *pdwNeeded)
{
   PSPLINEMOVEP ps;
   DWORD dwDataSize;
   ps = gaHairMove;
   dwDataSize = sizeof(gaHairMove);
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


/*************************************************************************************
CObjectHairLock::ControlPointQuery - Called to query the information about a control
point identified by dwID.

inputs
   DWORD       dwID - ID
   POSCONTROL  pInfo - Filled in with the infomration
returns
   BOOL - TRUE if successful
*/
BOOL CObjectHairLock::ControlPointQuery (DWORD dwID, POSCONTROL pInfo)
{
   return m_HairLock.ControlPointQuery (HAIRLOCKBASE, TRUE, FALSE, dwID, pInfo);
}

/*************************************************************************************
CObjectHairLock::ControlPointSet - Called to change the valud of a control point.

inputs
   DWORD       dwID - ID
   PCPoint     pVal - Contains the new location, in object coordinates
returns
   BOOL - TRUE if successful
*/
BOOL CObjectHairLock::ControlPointSet (DWORD dwID, PCPoint pVal, PCPoint pViewer)
{
   return m_HairLock.ControlPointSet (this, HAIRLOCKBASE, TRUE, FALSE, dwID, pVal, pViewer);
}

/*************************************************************************************
CObjectHairLock::ControlPointEnum - Called to enumerate a list of control point IDs
into the list.

inputs
   PCListFixed       plDWORD - Should be filled in with control points
returns
   none
*/
void CObjectHairLock::ControlPointEnum (PCListFixed plDWORD)
{
   m_HairLock.ControlPointEnum (HAIRLOCKBASE, TRUE, FALSE, plDWORD);
}






/****************************************************************************
HairLockPage
*/
BOOL HairLockPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCObjectHairLock pv = (PCObjectHairLock)pPage->m_pUserData;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         PCEscControl pControl;

         // checkboxes
         pControl = pPage->ControlFind (L"backfacecull");
         if (pControl)
            pControl->AttribSetBOOL (Checked(), pv->m_fBackfaceCull);

         MeasureToString (pPage, L"diameter", pv->m_HairLock.DiameterGet());
         MeasureToString (pPage, L"lengthperpoint", pv->m_HairLock.LengthGet());

         DWORD i, j;
         WCHAR szTemp[32];
         PTEXTUREPOINT ptpProf = pv->m_HairLock.ProfileGet ();
         for (i = 0; i < 5; i++) for (j = 0; j < 2; j++) {
            swprintf (szTemp, L"profile%d%d", (int)i, (int)j);
            pControl = pPage->ControlFind (szTemp);
            if (pControl)
               pControl->AttribSetInt (Pos(), (int)((j ? ptpProf[i].v : ptpProf[i].h) * 100.0));
         }
         pControl = pPage->ControlFind (L"variation");
         if (pControl)
            pControl->AttribSetInt (Pos(), (int)(pv->m_HairLock.VariationGet(NULL) * 100.0));

         DWORD *padwRepeat;
         fp *pafScale;
         DWORD dwHairLayers = pv->m_HairLock.HairLayersGet (&padwRepeat, &pafScale);
         ComboBoxSet (pPage, L"hairlayers", dwHairLayers-1);
         for (i = 0; i < 3; i++) {
            swprintf (szTemp, L"hairlayerscale%d", (int)i);
            pControl = pPage->ControlFind (szTemp);
            if (pControl) {
               pControl->AttribSetInt (Pos(), (int)(pafScale[i] * 100.0));
               pControl->Enable (i < dwHairLayers);
            }

            swprintf (szTemp, L"hairlayerrepeat%d", (int)i);
            DoubleToControl (pPage, szTemp, padwRepeat[i]);
            pControl = pPage->ControlFind (szTemp);
            if (pControl)
               pControl->Enable (i < dwHairLayers);
         }

      }
      break;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;
         PWSTR psz;
         if (!p->pControl || !(psz = p->pControl->m_pszName))
            break;

         if (!_wcsicmp(psz, L"backfacecull")) {
            BOOL fChecked = p->pControl->AttribGetBOOL (Checked());
            pv->m_pWorld->ObjectAboutToChange (pv);
            pv->m_fBackfaceCull = fChecked;
            pv->m_pWorld->ObjectChanged (pv);
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
         DWORD dwVal;
         dwVal = p->pszName ? (DWORD) _wtoi(p->pszName) : 0;

         if (!_wcsicmp(psz, L"hairlayers")) {
            DWORD *padwRepeat;
            fp *pafScale;
            DWORD dwHairLayers = pv->m_HairLock.HairLayersGet (&padwRepeat, &pafScale);

            if (dwVal == dwHairLayers-1)
               break;

            // else, change
            pv->m_pWorld->ObjectAboutToChange (pv);
            DWORD adwRepeat[MAXHAIRLAYERS];
            fp afScale[MAXHAIRLAYERS];
            memcpy (adwRepeat, padwRepeat, sizeof(adwRepeat));
            memcpy (afScale, pafScale, sizeof(afScale));

            pv->m_HairLock.HairLayersSet (dwHairLayers = dwVal+1, adwRepeat, afScale);
            pv->m_pWorld->ObjectChanged (pv);

            // enable/disable
            DWORD i;
            PCEscControl pControl;
            WCHAR szTemp[32];
            for (i = 0; i < 3; i++) {
               swprintf (szTemp, L"hairlayerscale%d", (int)i);
               pControl = pPage->ControlFind (szTemp);
               if (pControl)
                  pControl->Enable (i < dwHairLayers);

               swprintf (szTemp, L"hairlayerrepeat%d", (int)i);
               pControl = pPage->ControlFind (szTemp);
               if (pControl)
                  pControl->Enable (i < dwHairLayers);
            }
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

         DWORD dwCat, dwArray, dwArray2;
         dwCat = -2;
         PWSTR pszProfile = L"profile", pszHairLayerScale = L"hairlayerscale";
         DWORD dwLenProfile = (DWORD)wcslen(pszProfile), dwLenHairLayerScale = (DWORD)wcslen(pszHairLayerScale);
         if (!_wcsicmp(p->pControl->m_pszName, L"variation"))
            dwCat = 0;
         else if (!wcsncmp(p->pControl->m_pszName, pszProfile, dwLenProfile)) {
            dwCat = 1;
            dwArray = p->pControl->m_pszName[dwLenProfile] - L'0';
            dwArray2 = p->pControl->m_pszName[dwLenProfile+1] - L'0';
         }
         else if (!wcsncmp(p->pControl->m_pszName, pszHairLayerScale, dwLenHairLayerScale)) {
            dwCat = 2;
            dwArray = p->pControl->m_pszName[dwLenHairLayerScale] - L'0';
         }
         else
            break;   // not one of these

         fp fVal;
         fVal = p->pControl->AttribGetInt (Pos()) / 100.0;

         pv->m_pWorld->ObjectAboutToChange (pv);
         if (dwCat == 0) {
            DWORD dwSeed;
            pv->m_HairLock.VariationGet (&dwSeed);
            pv->m_HairLock.VariationSet (fVal, dwSeed);
         }
         else if (dwCat == 1) {
            PTEXTUREPOINT ptpProf = pv->m_HairLock.ProfileGet ();
            TEXTUREPOINT atp[5];
            memcpy (atp, ptpProf, sizeof(atp));
            if (dwArray2)
               atp[dwArray].v = fVal;
            else
               atp[dwArray].h = fVal;
            pv->m_HairLock.ProfileSet (atp);
         }
         else if (dwCat == 2) {
            DWORD *padwRepeat;
            fp *pafScale;
            DWORD dwHairLayers = pv->m_HairLock.HairLayersGet (&padwRepeat, &pafScale);
            DWORD adwRepeat[MAXHAIRLAYERS];
            fp afScale[MAXHAIRLAYERS];
            memcpy (adwRepeat, padwRepeat, sizeof(adwRepeat));
            memcpy (afScale, pafScale, sizeof(afScale));

            afScale[dwArray] = fVal;
            pv->m_HairLock.HairLayersSet (dwHairLayers, adwRepeat, afScale);
         }
         pv->m_pWorld->ObjectChanged (pv);
         return TRUE;
      }
      break;

   case ESCN_EDITCHANGE:
      {
         PESCNEDITCHANGE p = (PESCNEDITCHANGE) pParam;

         pv->m_pWorld->ObjectAboutToChange (pv);

         fp fVal;
         MeasureParseString (pPage, L"diameter", &fVal);
         if (fVal > CLOSE)
            pv->m_HairLock.DiameterSet (fVal);
         MeasureParseString (pPage, L"lengthperpoint", &fVal);
         if ((fVal > CLOSE) && (fVal != pv->m_HairLock.LengthGet()))
            pv->m_HairLock.LengthSet (fVal);


         DWORD *padwRepeat;
         fp *pafScale;
         DWORD dwHairLayers = pv->m_HairLock.HairLayersGet (&padwRepeat, &pafScale);
         DWORD adwRepeat[MAXHAIRLAYERS];
         fp afScale[MAXHAIRLAYERS];
         memcpy (adwRepeat, padwRepeat, sizeof(adwRepeat));
         memcpy (afScale, pafScale, sizeof(afScale));
         DWORD i;
         WCHAR szTemp[32];
         for (i = 0; i < MAXHAIRLAYERS; i++) {
            swprintf (szTemp, L"hairlayerrepeat%d", (int)i);
            fVal = DoubleFromControl (pPage, szTemp);
            adwRepeat[i] = max((DWORD)fVal,1);
         }
         pv->m_HairLock.HairLayersSet (dwHairLayers, adwRepeat, afScale);

         pv->m_pWorld->ObjectChanged (pv);
      }
      break;


   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Length-of-hair settings";
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}


/**********************************************************************************
CObjectHairLock::DialogShow - Causes the object to show a dialog box that allows
different attributes to be changed.

inputs
   DWORD             dwSuface - Surface that was clicked on. 0 if unknown.
   PCEscWindow       pWindow - Escarpment window to draw it in
returns
   BOOL - TRUE if user pressed "Back", FALSE if user pressed "Close"
*/
BOOL CObjectHairLock::DialogShow (DWORD dwSurface, PCEscWindow pWindow, PCPoint pClick)
{
   PWSTR pszRet;

   pszRet = pWindow->PageDialog (ghInstance, IDR_MMLHAIRLOCK, HairLockPage, this);
   if (!pszRet)
      return FALSE;

   return !_wcsicmp(pszRet, Back());
}

/**********************************************************************************
CObjectHairLock::DialogQuery - Returns TRUE if the object supports a custom
dialog box, FALSE if it doesn't. The default object doesn't
*/
BOOL CObjectHairLock::DialogQuery (void)
{
   return TRUE;
}


