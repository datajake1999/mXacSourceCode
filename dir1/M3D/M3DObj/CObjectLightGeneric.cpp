/************************************************************************
CObjectLightGeneric.cpp - Draws a generic light

begun 17/4/03 by Mike Rozak
Copyright 2004 Mike Rozak. All rights reserved
*/

#include <windows.h>
#include "escarpment.h"
#include "..\M3D.h"
#include "resource.h"

typedef struct {
   PWSTR       pszName;    // name of the move point
   int         iX, iY; // -1 if for left/top, 1 for right/bottom
} SPLINEMOVEP, *PSPLINEMOVEP;

static SPLINEMOVEP   gaLightMove[] = {
   L"Light", 0, 0
};

/**********************************************************************************
CObjectLightGeneric::Constructor and destructor */
CObjectLightGeneric::CObjectLightGeneric (PVOID pParams, POSINFO pInfo)
{
   m_dwType = (DWORD)(size_t) pParams;
   m_OSINFO = *pInfo;

   m_dwRenderShow = RENDERSHOW_ELECTRICAL;

   m_fLightOn = 1;

   memset (&m_li, 0, sizeof(m_li));
   m_li.dwForm = LIFORM_POINT;
   m_li.fNoShadows = FALSE;
   m_li.afLumens[0] = m_li.afLumens[1] = 0;
   m_li.afLumens[2] = 60 * CM_LUMENSPERINCANWATT;
   m_li.pLoc.Zero();
   m_li.pDir.Zero();
   m_li.pDir.p[2] = -1; // point down
   
   DWORD i;
   for (i = 0; i < 3; i++)
      m_li.awColor[i][0] = m_li.awColor[i][1] = m_li.awColor[i][2] = 0xffff;
   m_li.afDirectionality[0][0] = m_li.afDirectionality[1][0] = PI/2;
   m_li.afDirectionality[0][1] = m_li.afDirectionality[1][1] = PI/4;

   // create the bulb
   CPoint pBottom, pAroundVec, pLeftVec, pScale;
   DWORD dwType;
   pBottom.Zero();
   pAroundVec.Zero();
   pAroundVec.p[2] = -1;   // make it pointing down
   pLeftVec.Zero();
   pLeftVec.p[0] = 1;
   pScale.Zero();
   m_Revolution.BackfaceCullSet (TRUE); // since entirely enclosed
   m_Revolution.DirectionSet (&pBottom, &pAroundVec, &pLeftVec);
   m_Revolution.RevolutionSet (RREV_CIRCLE);
   pScale.p[0] = pScale.p[1] = CM_INCANSPOTDIAMETER;
   pScale.p[2] = CM_INCANSPOTLENGTH;
   dwType = RPROF_LIGHTSPOT;
   m_Revolution.ScaleSet (&pScale);
   m_Revolution.ProfileSet (dwType);
}


CObjectLightGeneric::~CObjectLightGeneric (void)
{
   // nothing for now
}


/**********************************************************************************
CObjectLightGeneric::Delete - Called to delete this object
*/
void CObjectLightGeneric::Delete (void)
{
   delete this;
}

/**********************************************************************************
CObjectLightGeneric::Render - Draws a test box.

inputs
   POBJECTRENDER     pr - Render information
*/
void CObjectLightGeneric::Render (POBJECTRENDER pr, DWORD dwSubObject)
{
   // if final render don't draw
   if (pr->dwReason == ORREASON_FINAL)
      return;

   // create the surface render object and draw
   // CRenderSurface rs;
   m_Renderrs.ClearAll();

   CMatrix mObject;
   m_Renderrs.Init (pr->pRS);
   ObjectMatrixGet (&mObject);
   m_Renderrs.Multiply (&mObject);


   GammaInit ();

   // make the color
   RENDERSURFACE Mat;
   memset (&Mat, 0, sizeof(Mat));
   Mat.Material.InitFromID (MATERIAL_GLASSFROSTED);
   Mat.Material.m_wTranslucent = 0xffff; // BUGFIX - Translucency was 0, make 0xffff
   //Mat.Material.m_fSelfIllum = TRUE;
   Mat.Material.m_wTransparency = 1;   // so can see it illuminated, but so light goes through
   Mat.Material.m_fNoShadows = TRUE;   // so get transparency

   m_Renderrs.SetDefMaterial (&Mat);
   m_Renderrs.SetDefColor (UnGamma (m_li.awColor[0])); // color of bulb based on light

   // draw it
   m_Revolution.Render (pr, &m_Renderrs);

   m_Renderrs.Commit();
}





/**********************************************************************************
CObjectLightGeneric::QueryBoundingBox - Standard API
*/
void CObjectLightGeneric::QueryBoundingBox (PCPoint pCorner1, PCPoint pCorner2, DWORD dwSubObject)
{
   m_Revolution.QueryBoundingBox (pCorner1, pCorner2);

#ifdef _DEBUG
   // test, make sure bounding box not too small
   CPoint p1,p2;
   DWORD i;
   CObjectTemplate::QueryBoundingBox (&p1, &p2, dwSubObject);
   for (i = 0; i < 3; i++)
      if ((p1.p[i] < pCorner1->p[i] - CLOSE) || (p2.p[i] > pCorner2->p[i] + CLOSE))
         break;
   if (i < 3)
      OutputDebugString ("\r\nCObjectLightGeneric::QueryBoundingBox too small.");
#endif
}


/**********************************************************************************
CObjectLightGeneric::Clone - Clones this

inputs
   none
returns
   PCObjectSocket - Clone
*/
PCObjectSocket CObjectLightGeneric::Clone (void)
{
   PCObjectLightGeneric pNew;

   pNew = new CObjectLightGeneric(NULL, &m_OSINFO);

   // clone template info
   CloneTemplate(pNew);

   pNew->m_li = m_li;
   pNew->m_fLightOn = m_fLightOn;
   return pNew;
}

static PWSTR gpszLightOn = L"LightOn";
static PWSTR gpszForm = L"Form";
static PWSTR gpszNoShadows = L"NoShadows";
static PWSTR gpszDirect = L"Direct";

PCMMLNode2 CObjectLightGeneric::MMLTo (void)
{
   // call into the template first
   PCMMLNode2 pNode = MMLToTemplate();
   if (!pNode)
      return NULL;

   // write info
   MMLValueSet (pNode, gpszLightOn, m_fLightOn);
   MMLValueSet (pNode, gpszForm, (int)m_li.dwForm);
   MMLValueSet (pNode, gpszNoShadows, (int)m_li.fNoShadows);

   CPoint pDir;
   DWORD i;
   for (i = 0; i < 4; i++)
      pDir.p[i] = m_li.afDirectionality[i / 2][i % 2];
   MMLValueSet (pNode, gpszDirect, &pDir);

   WCHAR szTemp[64];
   COLORREF cr;
   GammaInit();
   for (i = 0; i < 3; i++) {
      swprintf (szTemp, L"lumens%d", (int)i);
      MMLValueSet (pNode, szTemp, m_li.afLumens[i]);

      swprintf (szTemp, L"color%d", (int)i);
      cr = UnGamma (m_li.awColor[i]);
      MMLValueSet (pNode, szTemp, (int)cr);
   };


   return pNode;
}

BOOL CObjectLightGeneric::MMLFrom (PCMMLNode2 pNode)
{
   // call into the template first
   if (!MMLFromTemplate (pNode))
      return FALSE;


   GammaInit();
   m_fLightOn = MMLValueGetDouble (pNode, gpszLightOn, 0);

   m_li.dwForm = (DWORD) MMLValueGetInt (pNode, gpszForm, LIFORM_POINT);
   m_li.fNoShadows = (BOOL) MMLValueGetInt (pNode, gpszNoShadows, 0);

   CPoint pDir;
   pDir.Zero4();
   MMLValueGetPoint (pNode, gpszDirect, &pDir);
   DWORD i;
   for (i = 0; i < 4; i++)
      m_li.afDirectionality[i / 2][i % 2] = pDir.p[i];

   WCHAR szTemp[64];
   COLORREF cr;
   GammaInit();
   for (i = 0; i < 3; i++) {
      swprintf (szTemp, L"lumens%d", (int)i);
      m_li.afLumens[i] = MMLValueGetDouble (pNode, szTemp, 0);

      swprintf (szTemp, L"color%d", (int)i);
      cr = (COLORREF) MMLValueGetInt (pNode, szTemp, RGB(0xff,0xff,0xff));
      Gamma (cr, m_li.awColor[i]);
   };


   return TRUE;
}




/********************************************************************************
CObjectLightGeneric::LightQuery -
ask the object if it has any lights. If it does, pl is added to (it
is already initialized to sizeof LIGHTINFO) with one or more LIGHTINFO
structures. Coordinates are in OBJECT SPACE.
*/
BOOL CObjectLightGeneric::LightQuery (PCListFixed pl, DWORD dwShow)
{
   if (!m_fLightOn)
      return FALSE;

   // make sure want to see lights
   if (!(dwShow & m_dwRenderShow))
      return FALSE;

   LIGHTINFO li;
   DWORD i;
   li = m_li;
   for (i = 0; i < 3; i++)
      li.afLumens[i] *= m_fLightOn;

   // convert from local coord to object space
   // BUGFIX - Dont do because lightinfo seems to be in object space
   //li.pDir.Add (&li.pLoc);
   //li.pLoc.MultiplyLeft (&m_MatrixObject);
   //li.pDir.MultiplyLeft (&m_MatrixObject);
   //li.pDir.Subtract (&li.pLoc);
   //li.pDir.Normalize();  // in case scaling
   pl->Add (&li);

   return TRUE;
}


/**********************************************************************************
CObjectLightGeneric::TurnOnGet - 
returns how TurnOn the object is, from 0 (closed) to 1.0 (TurnOn), or
< 0 for an object that can't be TurnOned
*/
fp CObjectLightGeneric::TurnOnGet (void)
{
   return m_fLightOn;
}

/**********************************************************************************
CObjectLightGeneric::TurnOnSet - 
TurnOns/closes the object. if fTurnOn==0 it's close, 1.0 = TurnOn, and
values in between are partially TurnOned closed. Returne TRUE if success
*/
BOOL CObjectLightGeneric::TurnOnSet (fp fTurnOn)
{
   fTurnOn = max(0,fTurnOn);
   fTurnOn = min(1,fTurnOn);

   // BUBUG - Some ligths have no dimmer setting

   m_pWorld->ObjectAboutToChange (this);
   m_fLightOn = fTurnOn;
   m_pWorld->ObjectChanged (this);

   return TRUE;
}



/**********************************************************************************
CObjectLightGeneric::DialogQuery - Returns TRUE if the object supports a custom
dialog box, FALSE if it doesn't. The default object doesn't
*/
BOOL CObjectLightGeneric::DialogQuery (void)
{
   return TRUE;
}

/****************************************************************************
LightGenericPage
*/
static BOOL LightGenericPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCObjectLightGeneric pv = (PCObjectLightGeneric) pPage->m_pUserData;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         ComboBoxSet (pPage, L"type", pv->m_li.dwForm);

         COLORREF cr;
         cr = UnGamma (pv->m_li.awColor[0]);
         FillStatusColor (pPage, L"color", cr);

         // set the intensities
         pPage->Message (ESCM_USER+182);

         PCEscControl pControl;
         pControl = pPage->ControlFind (L"noshadows");
         if (pControl)
            pControl->AttribSetBOOL (Checked(), pv->m_li.fNoShadows);

         // do the angles
         DWORD i,j;
         WCHAR szTemp[64];
         for (i = 0; i < 2; i++) for (j = 0; j < 2; j++) {
            swprintf (szTemp, L"direct%d%d", (int)i, (int)j);
            AngleToControl (pPage, szTemp, pv->m_li.afDirectionality[i][j], FALSE);
         }
      }
      break;

   case ESCM_USER+182:  // update the edit fields for intensities
      {
         DWORD i;
         WCHAR szTemp[64];
         for (i = 0; i < 3; i++) {
            swprintf (szTemp, L"watts%d", (int)i);
            DoubleToControl (pPage, szTemp, pv->m_li.afLumens[i] /
               ((pv->m_li.dwForm == LIFORM_POINT) ? CM_LUMENSPERINCANWATT : CM_LUMENSSUN));
         }
      }
      return TRUE;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;

         if (!_wcsicmp(p->pControl->m_pszName, L"noshadows")) {
            if (pv->m_pWorld)
               pv->m_pWorld->ObjectAboutToChange (pv);
            pv->m_li.fNoShadows = p->pControl->AttribGetBOOL (Checked());
            if (pv->m_pWorld)
               pv->m_pWorld->ObjectChanged (pv);
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"changecolor")) {
            COLORREF cr, crOrig;
            crOrig = UnGamma (pv->m_li.awColor[0]);
            cr = AskColor (pPage->m_pWindow->m_hWnd, crOrig, pPage, L"color");
            if (cr != crOrig) {
               pv->m_pWorld->ObjectAboutToChange (pv);
               DWORD i;
               for (i = 0; i < 3; i++)
                  Gamma (cr, pv->m_li.awColor[i]);
               pv->m_pWorld->ObjectChanged (pv);
            }
            return TRUE;
         }
      }
      break;

   case ESCN_COMBOBOXSELCHANGE:
      {
         PESCNCOMBOBOXSELCHANGE p = (PESCNCOMBOBOXSELCHANGE) pParam;
         if (!p->pControl->m_pszName)
            break;

         PWSTR psz;
         DWORD dwVal;
         psz = p->pControl->m_pszName;
         dwVal = p->pszName ? _wtoi(p->pszName) : 0;

         if (!_wcsicmp(psz, L"type")) {
            if (dwVal == pv->m_li.dwForm)
               return TRUE;   // no change

            if (pv->m_pWorld)
               pv->m_pWorld->ObjectAboutToChange (pv);
            pv->m_li.dwForm = dwVal;
            pv->m_li.afLumens[2] = (dwVal == LIFORM_POINT) ?
               (60.0 * CM_LUMENSPERINCANWATT) : CM_LUMENSSUN;
            pv->m_li.afLumens[0] = pv->m_li.afLumens[1] = 0;
            if (pv->m_pWorld)
               pv->m_pWorld->ObjectChanged (pv);

            // set the intensities
            pPage->Message (ESCM_USER+182);
            return TRUE;
         }
      }
      break;

   case ESCN_EDITCHANGE:
      {
         PESCNEDITCHANGE p = (PESCNEDITCHANGE) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;   // default

         // if any edit changed get all values
         if (pv->m_pWorld)
            pv->m_pWorld->ObjectAboutToChange (pv);

         WCHAR szTemp[64];
         DWORD i, j;
         for (i = 0; i < 3; i++) {
            swprintf (szTemp, L"watts%d", (int)i);
            pv->m_li.afLumens[i] = DoubleFromControl (pPage, szTemp);
            if (pv->m_li.dwForm == LIFORM_POINT)
               pv->m_li.afLumens[i] *= CM_LUMENSPERINCANWATT;
            else
               pv->m_li.afLumens[i] *= CM_LUMENSSUN;
         }
         for (i = 0; i < 2; i++) for (j = 0; j < 2; j++) {
            swprintf (szTemp, L"direct%d%d", (int)i, (int)j);
            pv->m_li.afDirectionality[i][j] = AngleFromControl (pPage, szTemp);
         }
         if (pv->m_pWorld)
            pv->m_pWorld->ObjectChanged (pv);
      }
      break;
 
   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Light (generic) settings";
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}


/**********************************************************************************
CObjectLightGeneric::DialogShow - Causes the object to show a dialog box that allows
different attributes to be changed.

inputs
   DWORD             dwSuface - Surface that was clicked on. 0 if unknown.
   PCEscWindow       pWindow - Escarpment window to draw it in
returns
   BOOL - TRUE if user pressed "Back", FALSE if user pressed "Close"
*/

BOOL CObjectLightGeneric::DialogShow (DWORD dwSurface, PCEscWindow pWindow, PCPoint pClick)
{
   PWSTR pszRet;

   pszRet = pWindow->PageDialog (ghInstance, IDR_MMLLIGHTGENERIC, LightGenericPage, this);

   if (!pszRet)
      return FALSE;
   return !_wcsicmp(pszRet, Back());
}





/**************************************************************************************
CObjectLightGeneric::MoveReferencePointQuery - 
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
BOOL CObjectLightGeneric::MoveReferencePointQuery (DWORD dwIndex, PCPoint pp)
{
   PSPLINEMOVEP ps;
   DWORD dwDataSize;
   ps = gaLightMove;
   dwDataSize = sizeof(gaLightMove);

   if (dwIndex >= dwDataSize / sizeof(SPLINEMOVEP))
      return FALSE;

   // always at 0,0 in Lights
   pp->Zero();
   return TRUE;
}

/**************************************************************************************
CObjectLightGeneric::MoveReferenceStringQuery -
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
BOOL CObjectLightGeneric::MoveReferenceStringQuery (DWORD dwIndex, PWSTR psz, DWORD dwSize, DWORD *pdwNeeded)
{
   PSPLINEMOVEP ps;
   DWORD dwDataSize;
   ps = gaLightMove;
   dwDataSize = sizeof(gaLightMove);
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


