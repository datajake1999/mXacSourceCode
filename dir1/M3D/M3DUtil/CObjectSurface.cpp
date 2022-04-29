/************************************************************************
CObjectSurface.cpp - Surface description object

begun 12/9/01 by Mike Rozak
Copyright 2001 Mike Rozak. All rights reserved
*/

#include <windows.h>
#include "escarpment.h"
#include "..\M3D.h"
#include "resource.h"



/**************************************************************************************
CObjectSurface::Contsructor */
CObjectSurface::CObjectSurface (void)
{
   m_dwID = 0;
   memset (m_szScheme, 0, sizeof(m_szScheme));
   m_fUseTextureMap = FALSE;
   m_cColor = RGB(0xff,0xff,0xff);
   memset (&m_gTextureCode, 0, sizeof(m_gTextureCode));
   memset (&m_gTextureSub, 0, sizeof(m_gTextureSub));
   memset (&m_TextureMods, 0, sizeof(m_TextureMods));
   m_TextureMods.cTint = RGB(0xff,0xff,0xff);
   m_TextureMods.wBrightness = 0x1000;
   m_TextureMods.wContrast = 0x1000;
   // m_TextureMods.wFiller = 0;
   m_TextureMods.wSaturation = 0x1000;
   //m_TextureMods.wHue = 0;
   m_afTextureMatrix[0][0] = m_afTextureMatrix[1][1] = 1.0;
   m_afTextureMatrix[0][1] = m_afTextureMatrix[1][0] = 0.0;
   m_mTextureMatrix.Identity();
   m_Material.InitFromID (MATERIAL_FLAT);
}


/**************************************************************************************
CObjectSurface::MMLTo - Creates a MML node with all of the information written in.

inputs
   none
returns
   PCMMLNode2 - MMLNode describing the object. This must be freed by the caller
*/
PCMMLNode2 CObjectSurface::MMLTo (void)
{
   PCMMLNode2 pNode = new CMMLNode2;
   if (!pNode)
      return NULL;

   pNode->NameSet (ObjectSurface());
   MMLValueSet (pNode, L"ID", (int) m_dwID);
   if (m_szScheme[0])
      MMLValueSet (pNode, L"Scheme", m_szScheme);
   if (m_fUseTextureMap) {
      MMLValueSet (pNode, L"UseTextureMap", (int) m_fUseTextureMap);
      MMLValueSet (pNode, L"TextureCode", (LPBYTE)&m_gTextureCode, sizeof(m_gTextureCode));
      MMLValueSet (pNode, L"TextureSub", (LPBYTE)&m_gTextureSub, sizeof(m_gTextureSub));

      // BUGFIX - if XYZ matrix is identity then dont save
      CMatrix mIdent;
      mIdent.Identity();
      if (!m_mTextureMatrix.AreClose (&mIdent))
         MMLValueSet (pNode, L"MatrixXYZ", &m_mTextureMatrix);
      if (m_afTextureMatrix[0][0] != 1.0)
         MMLValueSet (pNode, L"m00", m_afTextureMatrix[0][0]);
      if (m_afTextureMatrix[0][1] != 0.0)
         MMLValueSet (pNode, L"m01", m_afTextureMatrix[0][1]);
      if (m_afTextureMatrix[1][0] != 0.0)
         MMLValueSet (pNode, L"m10", m_afTextureMatrix[1][0]);
      if (m_afTextureMatrix[1][1] != 1.0)
         MMLValueSet (pNode, L"m11", m_afTextureMatrix[1][1]);
      if (m_TextureMods.cTint != RGB(0xff,0xff,0xff))
         MMLValueSet (pNode, L"Tint", (int) m_TextureMods.cTint);
      if (m_TextureMods.wBrightness != 0x1000)
         MMLValueSet (pNode, L"Brightness", (int) m_TextureMods.wBrightness);
      if (m_TextureMods.wContrast != 0x1000)
         MMLValueSet (pNode, L"Contrast", (int) m_TextureMods.wContrast);
      if (m_TextureMods.wSaturation != 0x1000)
         MMLValueSet (pNode, L"Saturation", (int) m_TextureMods.wSaturation);
      if (m_TextureMods.wHue != 0)
         MMLValueSet (pNode, L"Hue", (int) m_TextureMods.wHue);
   }
   MMLValueSet (pNode, L"Color", (int) m_cColor);
   m_Material.MMLTo (pNode);
   //if (m_wTransparency)
   //   MMLValueSet (pNode, L"Transparency", (int) m_wTransparency);

   return pNode;
}

/**************************************************************************************
CObjectSurface::MMLFrom - Reads in an MML node and gets all the information needed
for the object surface.

inputs
   PCMMLNode2      pNode - node
returns
   BOOL - TRUE if successful
*/
BOOL CObjectSurface::MMLFrom (PCMMLNode2 pNode)
{
   m_dwID = (DWORD)MMLValueGetInt (pNode, L"ID", 0);
   PWSTR psz;
   psz = MMLValueGet (pNode, L"Scheme");
   if (psz)
      wcscpy (m_szScheme, psz);
   else
      m_szScheme[0] = 0;
   m_fUseTextureMap = (BOOL) MMLValueGetInt (pNode, L"UseTextureMap", 0);
   if (m_fUseTextureMap) {
      MMLValueGetBinary (pNode, L"TextureCode", (LPBYTE)&m_gTextureCode, sizeof(m_gTextureCode));
      MMLValueGetBinary (pNode, L"TextureSub", (LPBYTE)&m_gTextureSub, sizeof(m_gTextureSub));
      CMatrix mIdent;
      mIdent.Identity();
      MMLValueGetMatrix (pNode, L"MatrixXYZ", &m_mTextureMatrix, &mIdent);
      m_afTextureMatrix[0][0] = MMLValueGetDouble (pNode, L"m00", 1);
      m_afTextureMatrix[0][1] = MMLValueGetDouble (pNode, L"m01", 0);
      m_afTextureMatrix[1][0] = MMLValueGetDouble (pNode, L"m10", 0);
      m_afTextureMatrix[1][1] = MMLValueGetDouble (pNode, L"m11", 1);
      m_TextureMods.cTint = (COLORREF) MMLValueGetInt (pNode, L"Tint", (int) RGB(0xff,0xff,0xff));
      m_TextureMods.wBrightness = (WORD) MMLValueGetInt (pNode, L"Brightness", 0x1000);
      m_TextureMods.wContrast = (WORD) MMLValueGetInt (pNode, L"Contrast", 0x1000);
      m_TextureMods.wSaturation = (WORD) MMLValueGetInt (pNode, L"Saturation", 0x1000);
      m_TextureMods.wHue = (WORD) MMLValueGetInt (pNode, L"Hue", 0);
   }
   else {
      memset (&m_gTextureCode, 0, sizeof(m_gTextureCode));
      memset (&m_gTextureSub, 0, sizeof(m_gTextureSub));
      m_afTextureMatrix[0][0] =m_afTextureMatrix[1][1] = 1;
      m_afTextureMatrix[0][1] = m_afTextureMatrix[1][0] = 0;
      m_TextureMods.cTint = RGB(0xff,0xff,0xff);
      m_TextureMods.wBrightness = 0x1000;
      m_TextureMods.wContrast = 0x1000;
      m_TextureMods.wSaturation = 0x1000;
      m_TextureMods.wHue = 0;
      m_mTextureMatrix.Identity();
   }
   m_cColor = (COLORREF) MMLValueGetInt (pNode, L"Color", (int) RGB(0xff,0xff,0xff));
   m_Material.MMLFrom (pNode);
   //m_wTransparency = MMLValueGetInt (pNode, L"Transparency", (int) 0);

   return TRUE;
}

/**************************************************************************************
CObjectSurface::Clone - Clones the object.

inputs
   none
returns
   CObjectSurface* - New version
*/
CObjectSurface *CObjectSurface::Clone (void)
{
   PCObjectSurface pNew = new CObjectSurface;
   if (!pNew)
      return NULL;
   memcpy (pNew, this, sizeof(CObjectSurface));
   return pNew;
}


/**************************************************************************************
CObjectSurface::AreTheSame - Returns TRUE if this surface and the other one
are the same, FALSE if they're different

inputs
   PCObjectSurface      pComp To comparte with
*/
BOOL CObjectSurface::AreTheSame (CObjectSurface *pComp)
{
   DWORD i,j;
   if (_wcsicmp(m_szScheme, pComp->m_szScheme))
      return FALSE;

   if (pComp->m_fUseTextureMap != m_fUseTextureMap)
      return FALSE;
   if (m_fUseTextureMap) {
      if (!IsEqualGUID (pComp->m_gTextureCode, m_gTextureCode))
         return FALSE;
      if (!IsEqualGUID (pComp->m_gTextureSub, m_gTextureSub))
         return FALSE;

      if (memcmp(&pComp->m_TextureMods, &m_TextureMods, sizeof(m_TextureMods)))
         return FALSE;

      for (i = 0; i < 2; i++) for (j = 0; j < 2; j++)
         if (fabs(pComp->m_afTextureMatrix[i][j] - m_afTextureMatrix[i][j]) > CLOSE)
            return FALSE;

      if (!pComp->m_mTextureMatrix.AreClose (&m_mTextureMatrix))
         return FALSE;
   }
   else { // no texture map
      if (pComp->m_cColor != m_cColor)
         return FALSE;
   }

   if (memcmp(&pComp->m_Material, &m_Material, sizeof(m_Material)))
      return FALSE;

   return TRUE;
}



/**************************************************************************************
CMaterial::InitFromID - Fills in all the parameters of CMaterial based on the ID.

inputs
   DWORD       dwID - ID. One of MATERIAL_XXX
returns
   none
*/
BOOL CMaterial::InitFromID (DWORD dwID)
{
   m_dwID = dwID;
   m_dwMaterialType = 0;
   //m_fThickness = 0;
   m_wTransparency = 0;
   m_wSpecExponent = 100;
   m_wSpecReflect = 0;
   m_wSpecPlastic = 0x8000;
   m_fGlow = FALSE;
   m_wIndexOfRefract = 100;
   m_wReflectAmount = 0;
   m_wReflectAngle = 0x8000;
   m_wTransAngle = 0;
   m_wTranslucent = 0;
   m_wFill = 0;
   m_fNoShadows = FALSE;

   // BUGBUG - Should fine-tune specularity amounts now that can easily preview on sphere

#define SCALE     2
   switch (dwID) {
   case MATERIAL_PAINTMATTE:
      m_wSpecReflect = 0xffff / 12 * SCALE;  // BUGFIX - Was 10 * SCALE
      m_wSpecExponent = 300;
      break;
   case MATERIAL_PAINTSEMIGLOSS:
      m_wSpecReflect = 0xffff / 8;  // BUGFIX - Was / 4
      m_wSpecExponent = 1000;
      m_wSpecPlastic = 0x8000;   // BUGFIX - Was 0xe000;
      break;
   case MATERIAL_PAINTGLOSS:
      m_wSpecReflect = 0xffff / 4;  // BUGFIX - Was / 2
      m_wSpecExponent = 2000;
      m_wSpecPlastic = 0x8000;   // BUGFIX - Was 0xffff;
      break;
   case MATERIAL_CLOTHROUGH:
      m_wSpecReflect = 0;
      m_wSpecExponent = 200;
      m_wSpecPlastic = 0;
      break;
   case MATERIAL_CLOTHSMOOTH:
      m_wSpecReflect = 0xffff / 8;
      m_wSpecExponent = 200;
      m_wSpecPlastic = 0;
      break;
   case MATERIAL_CLOTHSILKY:
      m_wSpecReflect = 0xffff / 4;
      m_wSpecExponent = 800;
      m_wSpecPlastic = 0;
      break;
   case MATERIAL_TILEMATTE:
      m_wSpecReflect = 0xffff / 4;
      m_wSpecExponent = 500;
      m_wSpecPlastic = 0x1000;
      break;
   case MATERIAL_METALROUGH:
      m_wSpecReflect = 0xffff / 2 * SCALE;
      m_wSpecExponent = 800;
      m_wSpecPlastic = 0x4000;
      break;
   case MATERIAL_MIRROR:
      // BUGBUG - Modify mirror objects in built-in library to use the mirror surface
      m_wSpecReflect = 0;
      m_wSpecExponent = 0xffff;
      m_wSpecPlastic = 0xffff;
      m_wReflectAmount = 0xffff;
      m_wReflectAngle = 0;
      break;
   case MATERIAL_METALSMOOTH:
      m_wSpecReflect = 0xffff / 2 * SCALE;
      m_wSpecExponent = 5000;
      m_wSpecPlastic = 0x4000;
      m_wReflectAmount = 0x2000;
      m_wReflectAngle = 0;
      break;
   case MATERIAL_PLASTIC:
   case MATERIAL_TILEGLAZED:
      m_wSpecReflect = 0xffff / 2 * SCALE;
      m_wSpecPlastic = 0xffff;
      m_wSpecExponent = 2700;
      //m_wReflectAmount = 0x4000;
      m_wReflectAngle = 0;
      break;
   case MATERIAL_GLASSCLEARSOLID:
      // BUGFIX - Glass was 99% transparent. Make it 90%
      // BUGFIX - Up transparency again to 95%
      m_wTransparency = 0xffff - 0xffff/20;
      m_wSpecReflect = 0xffff / 2 * SCALE;
      m_wSpecPlastic = 0xffff;
      m_wSpecExponent = 5000;
      // BUGFIX - Take out: m_fNoShadows = TRUE;
      m_wReflectAmount = 0x4000;
      m_wReflectAngle = 0xffff;
      m_wIndexOfRefract = 144;
      break;
   case MATERIAL_GLASSCLEAR:
      // BUGFIX - Glass was 99% transparent. Make it 90%
      // BUGFIX - Up transparency again to 95%
      m_wTransparency = 0xffff - 0xffff/20;
      m_wSpecReflect = 0xffff / 2 * SCALE;
      m_wSpecPlastic = 0xffff;
      m_wSpecExponent = 5000;
      m_fNoShadows = TRUE; // BUGFIX - So lamps dont shadow
      m_wReflectAmount = 0x4000;
      m_wReflectAngle = 0xffff;
      break;
   case MATERIAL_GLASSFROSTED:
      m_wTranslucent = 0x8000;
      m_wSpecReflect = 0xffff / 2 * SCALE;
      m_wSpecPlastic = 0xffff;
      m_wSpecExponent = 2500;
      m_wTransparency = 0x400;
      m_wReflectAmount = 0x4000;
      m_wReflectAngle = 0xffff;
      m_fNoShadows = TRUE; // BUGFIX - So lamps dont shadow
      break;
   case MATERIAL_INVISIBLE:
      m_wTransparency = 0xffff;
      m_wSpecReflect = 0;
      m_fNoShadows = TRUE;
      break;
   case MATERIAL_WATERSHALLOW:
      // BUGFIX - Remove m_fTranslucent = TRUE;
      m_wSpecReflect = 0xffff / 2 * SCALE;
      m_wSpecPlastic = 0x4000;
      m_wSpecExponent = 2500;
      m_wTransparency = 0xc000;
      m_wReflectAmount = 0x1000;
      m_wReflectAngle = 0xffff;
      m_wIndexOfRefract = 133;
      break;
   case MATERIAL_WATERPOOL:
      // BUGFIX - Remove m_fTranslucent = TRUE;
      m_wSpecReflect = 0xffff / 2 * SCALE;
      m_wSpecPlastic = 0x4000;
      m_wSpecExponent = 2500;
      m_wTransparency = 0x4000;
      m_wReflectAmount = 0x800; // BUGFIX - Reduce since bumbier
      m_wReflectAngle = 0xffff;
      m_wIndexOfRefract = 133;
      break;
   case MATERIAL_WATERDEEP:
      // BUGFIX - Remove m_fTranslucent = TRUE;
      m_wSpecReflect = 0xffff / 2 * SCALE;
      m_wSpecPlastic = 0x4000;
      m_wSpecExponent = 2500;
      m_wTransparency = 0;
      m_wReflectAmount = 0x400;   // BUGFIX - nothing since too bumpy 0x4000;
      m_wReflectAngle = 0xffff;
      m_wIndexOfRefract = 133;
      break;
   case MATERIAL_FLAT:
      // do nothing
      break;
   case MATERIAL_FLYSCREEN:
      m_wTransparency = 0xffff - 0xffff/4;
      m_wTransAngle = 0xffff;
      // BUGFIX - Have specularity of flyscreen and doubled directionality
      m_wSpecReflect = 0xffff / 8 * SCALE;
      m_wSpecExponent = 1000;
      m_wSpecPlastic = 0xe000;
      break;
   case MATERIAL_TARP:
      m_wTransparency = 0x800;
      m_wTransAngle = 0xffff;
      m_wTranslucent = 0x4000;
      m_wSpecReflect = 0xffff / 4;
      m_wSpecExponent = 1000;
      m_wSpecPlastic = 0xe000;
      m_fNoShadows = TRUE; // BUGFIX: dont cast shadows so that lamp shades dont
      break;
   case MATERIAL_LEAF:
      // keep this matte since leaves all disorganized
      //m_wSpecReflect = 0xffff / 8;  // BUGFIX - Was / 4
      //m_wSpecExponent = 1000;
      //m_wSpecPlastic = 0x8000;   // BUGFIX - Was 0xe000;
      m_wTranslucent = 0x4000;   // about 25% translucent
      break;
   default:
      return FALSE;  // not valid
   }

   return TRUE;
}

static PWSTR gpszMaterialID = L"MaterialID";
static PWSTR gpszMaterialType = L"MaterialType";
static PWSTR gpszMatThickness = L"MaterialThickness";
static PWSTR gpszTransparency = L"Transparency";
static PWSTR gpszSpecExponent = L"SpecExponent";
static PWSTR gpszSpecReflect = L"SpecReflect";
static PWSTR gpszSpecPlastic = L"SpecPlastic";
static PWSTR gpszSelfIllum = L"SelfIllum";
static PWSTR gpszTranslucent = L"Translucent";
static PWSTR gpszNoShadows = L"NoShadows";
static PWSTR gpszIndexOfRefract = L"IndexOfRefract";
static PWSTR gpszReflectAmount = L"ReflectAmount";
static PWSTR gpszReflectAngle = L"ReflectAngle";
static PWSTR gpszTransAngle = L"TransAngle";

/**************************************************************************************
CMaterial::MMLTo - Writes MML out for the surface into the existing node. Names
probably shouldn't clash so it isn't a big deal.

inputs
   PCMMLNode2      pNode - Node to write to
returns
   BOOL - TRUE if success
*/
BOOL CMaterial::MMLTo (PCMMLNode2 pNode)
{
   MMLValueSet (pNode, gpszMaterialID, (int) m_dwID);
   //if (m_fThickness)
   //   MMLValueSet (pNode, gpszMatThickness, m_fThickness);
   if (m_dwID)
      return TRUE;   // nothing else to write

   // else custom
   if (m_dwMaterialType)
      MMLValueSet (pNode, gpszMaterialType, (int) m_dwMaterialType);
   if (m_wTransparency)
      MMLValueSet (pNode, gpszTransparency, (int) m_wTransparency);
   if (m_wSpecExponent != 100)
      MMLValueSet (pNode, gpszSpecExponent, (int)m_wSpecExponent);
   if (m_wSpecReflect)
      MMLValueSet (pNode, gpszSpecReflect, (int)m_wSpecReflect);
   if (m_wSpecPlastic != 0x8000)
      MMLValueSet (pNode, gpszSpecPlastic, (int)m_wSpecPlastic);
   if (m_fGlow)
      MMLValueSet (pNode, gpszSelfIllum, (int)m_fGlow);
   if (m_wTranslucent)
      MMLValueSet (pNode, gpszTranslucent, (int)m_wTranslucent);
   if (m_fNoShadows)
      MMLValueSet (pNode, gpszNoShadows, (int)m_fNoShadows);
   if (m_wIndexOfRefract != 100)
      MMLValueSet (pNode, gpszIndexOfRefract, (int)m_wIndexOfRefract);
   if (m_wReflectAmount != 0)
      MMLValueSet (pNode, gpszReflectAmount, (int)m_wReflectAmount);
   if (m_wReflectAngle != 0x8000)
      MMLValueSet (pNode, gpszReflectAngle, (int)m_wReflectAngle);
   if (m_wTransAngle)
      MMLValueSet (pNode, gpszTransAngle, (int)m_wTransAngle);

   return TRUE;
}

/**************************************************************************************
CMaterial::MMLFrom - Reads MML out for the node into the surface.

inputs
   PCMMLNode2      pNode - Node to write to
returns
   BOOL - TRUE if success
*/
BOOL CMaterial::MMLFrom (PCMMLNode2 pNode)
{
   m_dwID = (DWORD) MMLValueGetInt (pNode, gpszMaterialID, 0);
   InitFromID (m_dwID);
   //m_fThickness = MMLValueGetDouble (pNode, gpszMatThickness, 0);

   if (m_dwID)
      return TRUE;   // nothing else to read


   // else read in values
   m_dwMaterialType = (DWORD) MMLValueGetInt (pNode, gpszMaterialType, 0);
   m_wTransparency = (WORD) MMLValueGetInt (pNode, gpszTransparency, 0);
   m_wSpecExponent = (WORD) MMLValueGetInt (pNode, gpszSpecExponent, 100);
   m_wSpecReflect = (WORD) MMLValueGetInt (pNode, gpszSpecReflect, 0);
   m_wSpecPlastic = (WORD) MMLValueGetInt (pNode, gpszSpecPlastic, 0x8000);
   m_fGlow = (BOOL) MMLValueGetInt (pNode, gpszSelfIllum, FALSE);

   // BUGFIX - Because some m_fGlow's may have been written with random value,
   // if it's not TRUE then set to FALSE
   if (m_fGlow != TRUE)
      m_fGlow = FALSE;

   m_wTranslucent = (WORD) MMLValueGetInt (pNode, gpszTranslucent, 0);
   m_fNoShadows = (BOOL) MMLValueGetInt (pNode, gpszNoShadows, FALSE);
   m_wIndexOfRefract = (WORD) MMLValueGetInt (pNode, gpszIndexOfRefract, 100);
   m_wReflectAmount = (WORD) MMLValueGetInt (pNode, gpszReflectAmount, 0);
   m_wReflectAngle = (WORD) MMLValueGetInt (pNode, gpszReflectAngle, 0x8000);
   m_wTransAngle = (WORD) MMLValueGetInt (pNode, gpszTransAngle, 0);
   return TRUE;
}


/****************************************************************************
MaterialCustomPage
*/
BOOL MaterialCustomPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCMaterial pm = (PCMaterial) pPage->m_pUserData;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         PCEscControl pControl;

         // Set the sliders and stuff
         pControl = pPage->ControlFind (L"specexponent");
         fp fExp;
         fExp = log10((fp)pm->m_wSpecExponent / 100.0) * (fp)0x6000;
         fExp = max(0,fExp);
         fExp = min(0xffff, fExp);
         fExp = 0xffff - fExp;
         if (pControl)
            pControl->AttribSetInt (gszPos, (int) fExp);
         pControl = pPage->ControlFind (L"specreflect");
         if (pControl)
            pControl->AttribSetInt (gszPos, (int) pm->m_wSpecReflect);
         pControl = pPage->ControlFind (L"specplastic");
         if (pControl)
            pControl->AttribSetInt (gszPos, (int) pm->m_wSpecPlastic);
         pControl = pPage->ControlFind (L"transparency");
         if (pControl)
            pControl->AttribSetInt (gszPos, (int) pm->m_wTransparency);
         pControl = pPage->ControlFind (L"reflectamount");
         if (pControl)
            pControl->AttribSetInt (gszPos, (int) pm->m_wReflectAmount);
         pControl = pPage->ControlFind (L"reflectAngle");
         if (pControl)
            pControl->AttribSetInt (gszPos, (int) pm->m_wReflectAngle);
         pControl = pPage->ControlFind (L"transAngle");
         if (pControl)
            pControl->AttribSetInt (gszPos, (int) pm->m_wTransAngle);
         pControl = pPage->ControlFind (L"translucent");
         if (pControl)
            pControl->AttribSetInt (gszPos, (int) pm->m_wTranslucent);

         pControl = pPage->ControlFind (L"noshadows");
         if (pControl)
            pControl->AttribSetBOOL (gszChecked, pm->m_fNoShadows);

         ComboBoxSet (pPage, L"indexofrefract", pm->m_wIndexOfRefract);
      }
      break;

   case ESCM_LINK:
      {
         PESCMLINK p = (PESCMLINK) pParam;
         if (p->psz && !_wcsicmp(p->psz, L"ok")) {
            // get the values
            PCEscControl pControl;

            // Set the sliders and stuff
            pControl = pPage->ControlFind (L"specexponent");
            if (pControl) {
               fp fExp;
               fExp = pControl->AttribGetInt (gszPos);
               fExp = 0xffff - fExp;
               fExp = pow(10, fExp / (fp)0x6000) * 100.0;
               fExp = max(0,fExp);
               fExp = min(0xffff, fExp);
               pm->m_wSpecExponent = (WORD) fExp;
            }
            pControl = pPage->ControlFind (L"specreflect");
            if (pControl)
               pm->m_wSpecReflect = (WORD) pControl->AttribGetInt (gszPos);
            pControl = pPage->ControlFind (L"specplastic");
            if (pControl)
               pm->m_wSpecPlastic = (WORD) pControl->AttribGetInt (gszPos);
            pControl = pPage->ControlFind (L"transparency");
            if (pControl)
               pm->m_wTransparency = (WORD) pControl->AttribGetInt (gszPos);
            pControl = pPage->ControlFind (L"reflectamount");
            if (pControl)
               pm->m_wReflectAmount = (WORD) pControl->AttribGetInt (gszPos);
            pControl = pPage->ControlFind (L"reflectangle");
            if (pControl)
               pm->m_wReflectAngle = (WORD) pControl->AttribGetInt (gszPos);
            pControl = pPage->ControlFind (L"transangle");
            if (pControl)
               pm->m_wTransAngle = (WORD) pControl->AttribGetInt (gszPos);
            pControl = pPage->ControlFind (L"translucent");
            if (pControl)
               pm->m_wTranslucent = (WORD) pControl->AttribGetInt (gszPos);

            pControl = pPage->ControlFind (L"indexofrefract");
            if (pControl) {
               ESCMCOMBOBOXGETITEM gi;
               memset (&gi, 0, sizeof(gi));
               gi.dwIndex = pControl->AttribGetInt (CurSel());
               pControl->Message (ESCM_COMBOBOXGETITEM, &gi);
               if (gi.pszName)
                  pm->m_wIndexOfRefract = (WORD) _wtoi(gi.pszName);
            }

            pControl = pPage->ControlFind (L"noshadows");
            if (pControl)
               pm->m_fNoShadows = pControl->AttribGetBOOL (gszChecked);
            break;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}


/**************************************************************************************
CMaterial::Dialog - Brings up a pop-up dialog (like the texture selection one)
so the user can change the transparency, etc. This can only be called if m_dwID == 0.

inputs
   HWND        hWnd - Window to bring up from
returns
   BOOL - TRUE if the user pressed OK, FALSE if cancel
*/
BOOL CMaterial::Dialog (HWND hWnd)
{
   if (m_dwID)
      return FALSE;

   CEscWindow cWindow;
   RECT r;
   DialogBoxLocation2 (hWnd, &r);
   cWindow.Init (ghInstance, hWnd, EWS_FIXEDSIZE | EWS_AUTOHEIGHT | EWS_NOTITLE, &r);
   PWSTR pszRet;
   pszRet = cWindow.PageDialog (ghInstance, IDR_MMLMATERIALCUSTOM, MaterialCustomPage, this);
   if (!_wcsicmp(pszRet, L"ok"))
      return TRUE;
   else
      return FALSE;
}


