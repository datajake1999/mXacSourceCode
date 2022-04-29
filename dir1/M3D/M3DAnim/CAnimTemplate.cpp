/************************************************************************
CAnimTemplate.cpp - Code for template CAnim object, which handles
animations.
*/

#include <windows.h>
#include <math.h>
#include "escarpment.h"
#include "..\M3D.h"



/************************************************************************
CAnimTemplate::Constructor and destructor */
CAnimTemplate::CAnimTemplate (void)
{
   memset (&m_gClass, 0, sizeof(m_gClass));
   m_iPriority = 25;
   m_fAnimObjSplineSecondPass = FALSE;
   m_iMinDisplayX = m_iMinDisplayY = 32;
   m_fTimeMax = 1000000;
   m_pWorld = NULL;
   m_pScene = NULL;
   memset (&m_gWorldObject, 0, sizeof(m_gWorldObject));
   memset (&m_gAnim, 0, sizeof(m_gAnim));
   m_pLoc.Zero();
}

CAnimTemplate::~CAnimTemplate (void)
{
   // nothing for now
}

/************************************************************************
CAnimTemplate::Draw - See CAnimSocket for details
*/
void CAnimTemplate::Draw (HDC hDC, RECT *prHDC, fp fLeft, fp fRight)
{
   HBRUSH hbr = (HBRUSH) GetStockObject (LTGRAY_BRUSH);
   FillRect (hDC, prHDC, hbr);
}

/************************************************************************
CAnimTemplate::DialogQuery - See CAnimSocket for details
*/
BOOL CAnimTemplate::DialogQuery (void)
{
   return FALSE;
}

/************************************************************************
CAnimTemplate::DialogShow - See CAnimSocket for details
*/
BOOL CAnimTemplate::DialogShow (PCEscWindow pWindow)
{
   return FALSE;
}

/************************************************************************
CAnimTemplate::TimeSet - See CAnimSocket for details
*/
void CAnimTemplate::TimeSet (fp fStart, fp fEnd)
{
   // Send object about to change
   if (m_pScene && m_pScene->SceneSetGet())
      m_pScene->SceneSetGet()->ObjectAboutToChange (this);

   fEnd = max(fEnd, fStart);
   fEnd = min(fEnd, fStart + m_fTimeMax);
   m_pLoc.p[0] = fStart;
   m_pLoc.p[1] = fEnd;

   // Send object changed
   if (m_pScene && m_pScene->SceneSetGet())
      m_pScene->SceneSetGet()->ObjectChanged (this);
}

/************************************************************************
CAnimTemplate::Message - See CAnimSocket for details
*/
BOOL CAnimTemplate::Message (DWORD dwMessage, PVOID pParam)
{
   return FALSE;
}

/************************************************************************
CAnimTemplate::Deconstruct - See CAnimSocket for details
*/
BOOL CAnimTemplate::Deconstruct (BOOL fAct)
{
   // get the object this is in
   if (!m_pScene)
      return FALSE;
   PCSceneObj pSceneObj;
   pSceneObj = m_pScene->ObjectGet (&m_gWorldObject);
   if (!pSceneObj)
      return FALSE;

   if (!fAct)
      return TRUE;

   // build all the attributes without this object
   CListFixed lWithout, lWith;
   lWithout.Init (sizeof(PCAnimAttrib));
   lWith.Init (sizeof(PCAnimAttrib));
   pSceneObj->AnimAttribGenerateWithout (this);
   pSceneObj->AnimAttribGenerate();
   DWORD i;
   PCAnimAttrib paa;
   for (i = 0; i < pSceneObj->AnimAttribNum(); i++) {
      paa = pSceneObj->AnimAttribGet(i);
      if (!paa)
         continue;
      paa = paa->Clone();
      lWithout.Add (&paa);
   }
   
   // build with
   pSceneObj->AnimAttribGenerateWithout (NULL);
   pSceneObj->AnimAttribGenerate();
   for (i = 0; i < pSceneObj->AnimAttribNum(); i++) {
      paa = pSceneObj->AnimAttribGet(i);
      if (!paa)
         continue;
      paa = paa->Clone();
      lWith.Add (&paa);
   }

   // go through all the attributes in with
   DWORD j;
   PCAnimAttrib pWithout;
   for (i = 0; i < lWith.Num(); i++) {
      paa = *((PCAnimAttrib*) lWith.Get(i));

      // find this in without
      pWithout = NULL;
      for (j = 0; j < lWithout.Num(); j++) {
         pWithout = *((PCAnimAttrib*) lWithout.Get(j));
         if (!_wcsicmp(pWithout->m_szName, paa->m_szName))
            break;
      }
      if (j >= lWithout.Num())
         pWithout = NULL;  // couldn't find attribute in new one... which means we added it

      // now, loop through all the points in the original
      TEXTUREPOINT tp;
      DWORD dwLinear;
      fp fValue;
      for (j = 0; j < paa->PointNum(); j++) {
         if (!paa->PointGet(j, &tp, &dwLinear))
            continue;
         
         if (pWithout) {
            // what's the value in the new one?
            fValue = pWithout->ValueGet (tp.h);
            if (pWithout->m_dwType == AIT_ANGLE)
               fValue = AngleClosestModulo (fValue, tp.v);
            if (fabs(fValue - tp.h) < CLOSE)
               continue;   // same values
         }

         // if get here they're differnet values...

         // add it
         pSceneObj->GraphMovePoint (paa->m_szName, NULL, &tp, dwLinear);
      }  // j
   }// i


   // NOTE: Not handling audio recordings


   // finally, delete all
   PCAnimAttrib *ppaa;
   ppaa = (PCAnimAttrib*) lWith.Get(0);
   for (i = 0; i < lWith.Num(); i++)
      delete ppaa[i];
   ppaa = (PCAnimAttrib*) lWithout.Get(0);
   for (i = 0; i < lWithout.Num(); i++)
      delete ppaa[i];
   return TRUE;
}

/************************************************************************
CAnimTemplate::Merge - See CAnimSocket for details
*/
BOOL CAnimTemplate::Merge (GUID *pagWith, DWORD dwNum)
{
   return FALSE;
}


/************************************************************************
CAnimTemplate::WorldSet - See CAnimSocket for details
*/
void CAnimTemplate::WorldSet (PCWorldSocket pWorld, PCScene pScene, GUID *pgObject)
{
   // NOTE: Dont send object changed notification here
   m_pWorld = pWorld;
   m_pScene = pScene;
   m_gWorldObject = *pgObject;
}

/************************************************************************
CAnimTemplate::WorldGet - See CAnimSocket for details
*/
void CAnimTemplate::WorldGet (PCWorldSocket *ppWorld, PCScene *ppScene, GUID *pgObject)
{
   if (ppWorld)
      *ppWorld = m_pWorld;
   if (ppScene)
      *ppScene = m_pScene;
   if (pgObject)
      *pgObject = m_gWorldObject;
}

/************************************************************************
CAnimTemplate::GUIDSet - See CAnimSocket for details
*/
void CAnimTemplate::GUIDSet (GUID *pgObject)
{
   // NOTE: Dont tell world that objhect changed
   m_gAnim = *pgObject;
}

/************************************************************************
CAnimTemplate::GUIDGet - See CAnimSocket for details
*/
void CAnimTemplate::GUIDGet (GUID *pgObject)
{
   *pgObject = m_gAnim;
}

/************************************************************************
CAnimTemplate::ClassGet - See CAnimSocket for details
*/
void CAnimTemplate::ClassGet (GUID *pgClass)
{
   *pgClass = m_gClass;
}

/************************************************************************
CAnimTemplate::PriorityGet - See CAnimSocket for details
*/
int CAnimTemplate::PriorityGet (void)
{
   return m_iPriority;
}


/************************************************************************
CAnimTemplate::QueryMinDisplay - See CAnimSocket for details
*/
void CAnimTemplate::QueryMinDisplay (int *piX, int *piY)
{
   if (piX)
      *piX = m_iMinDisplayX;
   if (piY)
      *piY = m_iMinDisplayY;
}

/************************************************************************
CAnimTemplate::TimeGet - See CAnimSocket for details
*/
void CAnimTemplate::TimeGet (fp *pfStart, fp *pfEnd)
{
   if (pfStart)
      *pfStart = m_pLoc.p[0];
   if (pfEnd)
      *pfEnd = m_pLoc.p[1];
}


/************************************************************************
CAnimTemplate::QueryTimeMax - See CAnimSocket for details
*/
fp CAnimTemplate::QueryTimeMax (void)
{
   return m_fTimeMax;
}

/************************************************************************
CAnimTemplate::VertSet - See CAnimSocket for details
*/
void CAnimTemplate::VertSet (fp fVert)
{
   // inform scene that changing
   if (m_pScene && m_pScene->SceneSetGet())
      m_pScene->SceneSetGet()->ObjectAboutToChange (this);

   m_pLoc.p[2] = fVert;

   // inform scene that changed
   if (m_pScene && m_pScene->SceneSetGet())
      m_pScene->SceneSetGet()->ObjectChanged (this);
}

/************************************************************************
CAnimTemplate::VertGet - See CAnimSocket for details
*/
fp CAnimTemplate::VertGet (void)
{
   return m_pLoc.p[2];
}

/************************************************************************
CAnimTemplate::MMLToTemplate - A super-class should call this so the template
saves it's MML information away when the superclass gets ::MMLTo. The
superclass can use the node returned by MMLToTemplate() and just add its own
information there. (The name is set to gpszAnimTemplate)
*/
static PWSTR gpszAnimTemplate = L"AnimTemplate";
static PWSTR gpszAnim = L"ATAnim";
static PWSTR gpszLoc = L"ATLoc";

PCMMLNode2 CAnimTemplate::MMLToTemplate (void)
{
   PCMMLNode2 pNode = new CMMLNode2;
   if (!pNode)
      return NULL;
   pNode->NameSet (gpszAnimTemplate);

   MMLValueSet (pNode, gpszAnim, (PBYTE) &m_gAnim, sizeof(m_gAnim));
   MMLValueSet (pNode, gpszLoc, &m_pLoc);

   return pNode;
}

/************************************************************************
CAnimTemplate::MMLFromTemplate - A super-class should call this so the template
gets it's MML information when the superclass gets ::MMLFrom.
*/
BOOL CAnimTemplate::MMLFromTemplate (PCMMLNode2 pNode)
{
   MMLValueGetBinary (pNode, gpszAnim, (PBYTE) &m_gAnim, sizeof(m_gAnim));
   MMLValueGetPoint (pNode, gpszLoc, &m_pLoc);

   return TRUE;
}

/************************************************************************
CAnimTemplate::CloneTemplate - A super-class should call this so the template
cane clone its information into the new object.
*/
void CAnimTemplate::CloneTemplate (CAnimTemplate *pCloneTo)
{
   pCloneTo->m_gClass = m_gClass;
   pCloneTo->m_iPriority = m_iPriority;
   pCloneTo->m_fAnimObjSplineSecondPass = m_fAnimObjSplineSecondPass;
   pCloneTo->m_iMinDisplayX = m_iMinDisplayX;
   pCloneTo->m_iMinDisplayY = m_iMinDisplayY;
   pCloneTo->m_fTimeMax = m_fTimeMax;
   pCloneTo->m_pWorld = m_pWorld;
   pCloneTo->m_pScene = m_pScene;
   pCloneTo->m_gWorldObject = m_gWorldObject;
   pCloneTo->m_gAnim = m_gAnim;
   pCloneTo->m_pLoc.Copy (&m_pLoc);
}

/************************************************************************
CAnimTemplate::AnimObjSplineSecondPass - See standard CAnimSocket
*/
BOOL CAnimTemplate::AnimObjSplineSecondPass (void)
{
   return m_fAnimObjSplineSecondPass;
}


/************************************************************************
CAnimTemplate::WaveFormatGet - See standard CAnimSocket
*/
BOOL CAnimTemplate::WaveFormatGet (DWORD *pdwSamplesPerSec, DWORD *pdwChannels)
{
   return FALSE;
}

/************************************************************************
CAnimTemplate::WaveGet - See standard CAnimSocket
*/
BOOL CAnimTemplate::WaveGet (DWORD dwSamplesPerSec, DWORD dwChannels, int iTimeSec, int iTimeFrac,
                        DWORD dwSamples, short *pasSamp, fp *pfVolume)
{
   return FALSE;
}
