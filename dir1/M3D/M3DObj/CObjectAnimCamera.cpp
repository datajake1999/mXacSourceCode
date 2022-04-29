/************************************************************************
CObjectAnimCamera.cpp - Draws a camera used in animation.

begun 30/1/03 by Mike Rozak
Copyright 2003 Mike Rozak. All rights reserved
*/

#include <windows.h>
#include "escarpment.h"
#include "..\M3D.h"
#include "resource.h"

/**********************************************************************************
CObjectAnimCamera::Constructor and destructor */
CObjectAnimCamera::CObjectAnimCamera (PVOID pParams, POSINFO pInfo)
{
   m_OSINFO = *pInfo;

   m_dwRenderShow = RENDERSHOW_ANIMCAMERA;

   // Init other settings
   m_fFOV = PI / 4;
   m_fExposure = 1;

   // camera bits
   DWORD i;
   for (i = 0; i < 3; i++) {
      m_apRev[i] = new CRevolution;
      m_apRev[i]->BackfaceCullSet (FALSE);
   }
   // NOTE: Y==-1 is the FRONT. This is different from CObjectCamera where Y==1 is front
   CPoint pBottom, pAround, pLeft, pScale;

   // lense
   pBottom.Zero();
   pBottom.p[1] = .15 + .001; // so that if position camera at 0,0,0, wont see camera image
   pAround.Zero();
   pAround.p[1] = -1;
   pLeft.Zero();
   pLeft.p[0] = 1;
   pScale.Zero();
   pScale.p[0] = pScale.p[1] = .1;
   pScale.p[2] = .15;
   m_apRev[0]->DirectionSet (&pBottom, &pAround, &pLeft);
   m_apRev[0]->ProfileSet (RPROF_LIGHTSPOT);
   m_apRev[0]->RevolutionSet (RREV_SQUARE);
   m_apRev[0]->ScaleSet (&pScale);

   // reel
   pBottom.Zero();
   pBottom.p[0] = -.02;
   pBottom.p[1] = .20;
   pBottom.p[2] = .10;
   pAround.Zero();
   pAround.p[0] = 1;
   pLeft.Zero();
   pLeft.p[2] = 1;
   pScale.Zero();
   pScale.p[0] = pScale.p[1] = .20;
   pScale.p[2] = .04;
   m_apRev[1]->DirectionSet (&pBottom, &pAround, &pLeft);
   m_apRev[1]->ProfileSet (RPROF_C);
   m_apRev[1]->RevolutionSet (RREV_CIRCLE, 2);
   m_apRev[1]->ScaleSet (&pScale);

   // second reel
   pBottom.p[0] += .001;   // so dont have drawing problems with exact planes
   pBottom.p[1] = .35;
   m_apRev[2]->DirectionSet (&pBottom, &pAround, &pLeft);
   m_apRev[2]->ProfileSet (RPROF_C);
   m_apRev[2]->RevolutionSet (RREV_CIRCLE, 2);
   m_apRev[2]->ScaleSet (&pScale);

   // Need paint color
   ObjectSurfaceAdd (42, RGB(0x40,0x40,0x40), MATERIAL_PAINTGLOSS);
}


CObjectAnimCamera::~CObjectAnimCamera (void)
{
   DWORD i;
   for (i = 0; i < 3; i++)
      if (m_apRev[i])
         delete m_apRev[i];
}


/**********************************************************************************
CObjectAnimCamera::Delete - Called to delete this object
*/
void CObjectAnimCamera::Delete (void)
{
   delete this;
}

/**********************************************************************************
CObjectAnimCamera::Render - Draws a test box.

inputs
   POBJECTRENDER     pr - Render information
*/
void CObjectAnimCamera::Render (POBJECTRENDER pr, DWORD dwSubObject)
{
   // create the surface render object and draw
   // CRenderSurface rs;
   m_Renderrs.ClearAll();

   CMatrix mObject;
   m_Renderrs.Init (pr->pRS);
   ObjectMatrixGet (&mObject);
   m_Renderrs.Multiply (&mObject);

   // object specific
   m_Renderrs.SetDefMaterial (m_OSINFO.dwRenderShard, ObjectSurfaceFind (42), m_pWorld);

   // draw
   DWORD i;
   for (i = 0; i < 3; i++)
      m_apRev[i]->Render (pr, &m_Renderrs);

   // Draw main box
   m_Renderrs.ShapeBox (-.05, .15, -.05, .05, .4, .1);

   m_Renderrs.Commit();
}



/**********************************************************************************
CObjectAnimCamera::QueryBoundingBox - Standard API
*/
void CObjectAnimCamera::QueryBoundingBox (PCPoint pCorner1, PCPoint pCorner2, DWORD dwSubObject)
{
   // main box
   pCorner1->Zero();
   pCorner2->Zero();
   pCorner1->p[0] = -.05;
   pCorner1->p[1] = .15;
   pCorner1->p[2] = -.05;
   pCorner2->p[0] = .05;
   pCorner2->p[1] = .4;
   pCorner2->p[2] = .1;

   // draw
   DWORD i;
   CPoint p1, p2;
   for (i = 0; i < 3; i++) {
      m_apRev[i]->QueryBoundingBox (&p1, &p2);
      pCorner1->Min(&p1);
      pCorner2->Max(&p2);
   }

#ifdef _DEBUG
   // test, make sure bounding box not too small
   //CPoint p1,p2;
   //DWORD i;
   CObjectTemplate::QueryBoundingBox (&p1, &p2, dwSubObject);
   for (i = 0; i < 3; i++)
      if ((p1.p[i] < pCorner1->p[i] - CLOSE) || (p2.p[i] > pCorner2->p[i] + CLOSE))
         break;
   if (i < 3)
      OutputDebugString ("\r\nCObjectAnimCamera::QueryBoundingBox too small.");
#endif
}

/**********************************************************************************
CObjectAnimCamera::Clone - Clones this

inputs
   none
returns
   PCObjectSocket - Clone
*/
PCObjectSocket CObjectAnimCamera::Clone (void)
{
   PCObjectAnimCamera pNew;

   pNew = new CObjectAnimCamera(NULL, &m_OSINFO);

   // clone template info
   CloneTemplate(pNew);

   // Clone member variables
   pNew->m_fFOV = m_fFOV;
   pNew->m_fExposure = m_fExposure;

   // dont bother cloning revolutions since always created

   return pNew;
}




static PWSTR gpszFOV = L"FOV";
static PWSTR gpszExposure = L"Exp";

PCMMLNode2 CObjectAnimCamera::MMLTo (void)
{
   // call into the template first
   PCMMLNode2 pNode = MMLToTemplate();
   if (!pNode)
      return NULL;

   MMLValueSet (pNode, gpszFOV, m_fFOV);
   MMLValueSet (pNode, gpszExposure, m_fExposure);

   return pNode;
}

BOOL CObjectAnimCamera::MMLFrom (PCMMLNode2 pNode)
{
   // call into the template first
   if (!MMLFromTemplate (pNode))
      return FALSE;

   m_fFOV = MMLValueGetDouble (pNode, gpszFOV, PI/4);
   m_fExposure = MMLValueGetDouble (pNode, gpszExposure, 1);
   return TRUE;
}

/**************************************************************************************
CObjectTemplate::MoveReferencePointQuery - 
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
BOOL CObjectAnimCamera::MoveReferencePointQuery (DWORD dwIndex, PCPoint pp)
{
   if (dwIndex != 0)
      return FALSE;

   pp->Zero();

   return TRUE;
}

/**************************************************************************************
CObjectAnimCamera::MoveReferenceStringQuery -
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
BOOL CObjectAnimCamera::MoveReferenceStringQuery (DWORD dwIndex, PWSTR psz, DWORD dwSize, DWORD *pdwNeeded)
{
   if (dwIndex) {
      if (pdwNeeded)
         *pdwNeeded = 0;
      return FALSE;
   }

   PWSTR pszOrigin = L"Position";

   DWORD dwNeeded;
   dwNeeded = ((DWORD)wcslen (pszOrigin) + 1) * 2;
   if (pdwNeeded)
      *pdwNeeded = dwNeeded;
   if (dwNeeded <= dwSize) {
      wcscpy (psz, pszOrigin);
      return TRUE;
   }
   else
      return FALSE;
}


/**********************************************************************************
CObjectAnimCamera::Message -
sends a message to the object. The interpretation of the message depends upon
dwMessage, which is OSM_XXX. If the function understands and handles the
message it returns TRUE, otherwise FALE.
*/
BOOL CObjectAnimCamera::Message (DWORD dwMessage, PVOID pParam)
{
   switch (dwMessage) {
   case OSM_ANIMCAMERA:
      {
         POSMANIMCAMERA p = (POSMANIMCAMERA) pParam;
         p->poac = this;
      }
      return TRUE;
   }

   return FALSE;
}


/* AnimCameraDialogPage
*/
BOOL AnimCameraDialogPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCObjectAnimCamera pv = (PCObjectAnimCamera) pPage->m_pUserData;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         DoubleToControl (pPage, L"exposure", pv->m_fExposure);
         AngleToControl (pPage, L"fov", pv->m_fFOV);
      }
      break;


   case ESCN_EDITCHANGE:
      {
         PESCNEDITCHANGE p = (PESCNEDITCHANGE) pParam;

         if (!p->pControl || !p->pControl->m_pszName)
            break;
         PWSTR psz;
         psz = p->pControl->m_pszName;

         // since any edit change will result in redraw, get them all
         pv->m_pWorld->ObjectAboutToChange (pv);

         pv->m_fExposure = DoubleFromControl (pPage, L"exposure");
         pv->m_fFOV = AngleFromControl (pPage, L"fov");
         pv->m_fFOV = max(pv->m_fFOV, CLOSE);
         pv->m_fFOV = min(pv->m_fFOV, PI * .99);

         pv->m_pWorld->ObjectChanged (pv);


         break;
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Animation camera settings";
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}


/**********************************************************************************
CObjectAnimCamera::DialogShow - Causes the object to show a dialog box that allows
different attributes to be changed.

inputs
   DWORD             dwSuface - Surface that was clicked on. 0 if unknown.
   PCEscWindow       pWindow - Escarpment window to draw it in
returns
   BOOL - TRUE if user pressed "Back", FALSE if user pressed "Close"
*/
BOOL CObjectAnimCamera::DialogShow (DWORD dwSurface, PCEscWindow pWindow, PCPoint pClick)
{
   PWSTR pszRet;
   pszRet = pWindow->PageDialog (ghInstance, IDR_MMLANIMCAMERADIALOG, AnimCameraDialogPage, this);
   if (!pszRet)
      return FALSE;

   return !_wcsicmp(pszRet, Back());
}


/**********************************************************************************
CObjectAnimCamera::DialogQuery - Returns TRUE if the object supports a custom
dialog box, FALSE if it doesn't. The default object doesn't
*/
BOOL CObjectAnimCamera::DialogQuery (void)
{
   return TRUE;
}


static PWSTR gpszAttribExposure = L"Exposure";
static PWSTR gpszAttribFOV = L"FOV";
static DWORD gdwAttribExposure = (DWORD)wcslen(gpszAttribExposure);
static DWORD gdwAttribFOV = (DWORD)wcslen(gpszAttribFOV);

/*****************************************************************************************
CObjectAnimCamera::AttribGetIntern - OVERRIDE THIS

Like AttribGet() except that only called if default attributes not handled.
*/
BOOL CObjectAnimCamera::AttribGetIntern (PWSTR pszName, fp *pfValue)
{
   DWORD dwLen = (DWORD)wcslen(pszName);

   if ((dwLen == gdwAttribExposure) && !_wcsicmp(pszName, gpszAttribExposure)) {
      *pfValue = m_fExposure;
      return TRUE;
   }
   else if ((dwLen == gdwAttribFOV) && !_wcsicmp(pszName, gpszAttribFOV)) {
      *pfValue = m_fFOV;
      return TRUE;
   }

   return FALSE;
}


/*****************************************************************************************
CObjectAnimCamera::AttribGetAllIntern - OVERRIDE THIS

Like AttribGetAllIntern() EXCEPT plATTRIBVAL is already initialized and filled with
some parameters (default to the object template)
*/
void CObjectAnimCamera::AttribGetAllIntern (PCListFixed plATTRIBVAL)
{
   ATTRIBVAL av;
   memset (&av, 0, sizeof(av));

   wcscpy (av.szName, gpszAttribExposure);
   av.fValue = m_fExposure;
   plATTRIBVAL->Add (&av);

   wcscpy (av.szName, gpszAttribFOV);
   av.fValue = m_fFOV;
   plATTRIBVAL->Add (&av);

}


/*****************************************************************************************
CObjectAnimCamera::AttribSetGroupIntern - OVERRIDE THIS

Like AttribSetGroup() except passing on non-template attributes.
*/
void CObjectAnimCamera::AttribSetGroupIntern (DWORD dwNum, PATTRIBVAL paAttrib)
{
   DWORD i, dwLen;
   BOOL fChanged = FALSE;
   fp *pf;
   for (i = 0; i < dwNum; i++, paAttrib++) {
      dwLen = (DWORD)wcslen(paAttrib->szName);
      pf = NULL;

      if ((dwLen == gdwAttribExposure) && !_wcsicmp(paAttrib->szName, gpszAttribExposure))
         pf = &m_fExposure;
      else if ((dwLen == gdwAttribFOV) && !_wcsicmp(paAttrib->szName, gpszAttribFOV))
         pf = &m_fFOV;

      if (!pf)
         continue;

      if (!fChanged) {
         fChanged = TRUE;
         if (m_pWorld)
            m_pWorld->ObjectAboutToChange (this);
      }

      // set
      *pf = paAttrib->fValue;
      m_fFOV = max(m_fFOV, .001);
      m_fFOV = min(m_fFOV, PI * .99);
   }

   if (fChanged && m_pWorld)
      m_pWorld->ObjectChanged (this);
}


/*****************************************************************************************
CObjectAnimCamera::AttribInfoIntern - OVERRIDE THIS

Like AttribInfo() except called if attribute is not for template.
*/
BOOL CObjectAnimCamera::AttribInfoIntern (PWSTR pszName, PATTRIBINFO pInfo)
{
   fp *pf;
   DWORD dwLen = (DWORD)wcslen(pszName);
   pf = NULL;

   if ((dwLen == gdwAttribExposure) && !_wcsicmp(pszName, gpszAttribExposure))
      pf = &m_fExposure;
   else if ((dwLen == gdwAttribFOV) && !_wcsicmp(pszName, gpszAttribFOV))
      pf = &m_fFOV;

   if (!pf)
      return FALSE;

   memset (pInfo, 0, sizeof(*pInfo));
   if (pf == &m_fExposure) {
      pInfo->dwType = AIT_NUMBER;
      pInfo->fMin = 0;
      pInfo->fMax = 5;
      pInfo->pszDescription = L"Compensation for low light. Use 1.0 for bright sunlight. "
         L"3.0 for daytime interior. 5.0 for dark night-time interior.";
   }
   else if (pf == &m_fFOV) {
      pInfo->dwType = AIT_ANGLE;
      pInfo->fMin = 0;
      pInfo->fMax = PI;
      pInfo->pszDescription = L"Field of view. 45 is a typical lens. Larger "
         L"values result in wider angle lenses. Smaller values are longer lenses.";
   }

   return TRUE;
}




// BUGBUG - Eventually do after-effects
// BUGBUG - Will have more attributes when support aftereffects
