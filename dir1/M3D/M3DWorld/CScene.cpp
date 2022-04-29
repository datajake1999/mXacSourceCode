/************************************************************************
CScene.cpp - Code for CScene, CSceneObj, and CAnimAttrib
*/

#include <windows.h>
#include <math.h>
#include "escarpment.h"
#include "..\M3D.h"

typedef struct {
   fp       fTime;      // time
   fp       fValue;     // value
   DWORD    dwLinear;   // 0 for constant, 1 linear, 2 spline
} AAPOINT, *PAAPOINT;

/**********************************************************************************
AngleModulo - Given an angle, does a module to keep it between -PI and PI.

inputs
   fp       fAngle - in
returns
   fp - Modulo
*/
fp AngleModulo (fp fAngle)
{
   // fast check
   if ((fAngle >= -PI) && (fAngle <= PI))
      return fAngle;

   fAngle += PI;
   fAngle = myfmod(fAngle, 2 * PI);
   fAngle -= PI;
   return fAngle;
}

/**********************************************************************************
AngleClosestModulo - Given angle A, return the cloest modulo (+- 2PI) to angle B.
Use this so interpolation can loop around.

inputs
   fp       fA - Angle A. Will be returned with += N*2PI
   fp       fB - Angle B - to match
returns
   fp - A +/- N*2PI suchas that return - fB has minimum error
*/
fp AngleClosestModulo (fp fA, fp fB)
{
   fp fDist1, fDist2, fATry;
   BOOL fPositive = (fA < fB);

   fDist1 = fabs(fA - fB);

   while (TRUE) {
      // if within PI distance then adding 2 PI will only make further away
      if (fDist1 <= PI)
         return fA;

      fATry = fA + (fPositive ? 1 : -1) * 2 * PI;
      fDist2 = fabs(fATry - fB);
      if (fDist1 < fDist2)
         return fA;

      // else continue moving up
      fA = fATry;
      fDist1 = fDist2;
   }
}


/**********************************************************************************
CAnimAttrib::Constructor and destructor

inputs
   DWORD       dwType - One of AIT_XXX. If it's AIT_ANGLE then uses modulo when
               interpolating. If AIT_BOOL then uses constant values always.
*/
CAnimAttrib::CAnimAttrib (void)
{
   m_szName[0] = 0;
   m_dwType = 0;
   m_fMin = 0;
   m_fMax = 1;
   m_lAAPOINT.Init (sizeof(AAPOINT));
}

CAnimAttrib::~CAnimAttrib (void)
{
   // do nothing for now
}


/**********************************************************************************
CAnimAttrib::Clear - Clears out all the information in the attribute so can start
over again.
*/
void CAnimAttrib::Clear (void)
{
   m_lAAPOINT.Clear();
}


/**********************************************************************************
CAnimAttrib::PointNum - Returns the number of points in the spline.
*/
DWORD CAnimAttrib::PointNum (void)
{
   return m_lAAPOINT.Num();
}

/**********************************************************************************
CAnimAttrib::PointGet - Gets the value of a point.

inputs
   DWORD          dwIndex - from 0..PointNum()-1
   PTEXTUREPOINT  pt - Filled in with the point. .h = time, .v = value
   DWORD          *pdwLinear - Set to 0 if point is constant, 1 if lienar, 2 if spline
returns
   BOOL - TRUE if succede
*/
BOOL CAnimAttrib::PointGet (DWORD dwIndex, PTEXTUREPOINT pt, DWORD *pdwLinear)
{
   PAAPOINT p = (PAAPOINT) m_lAAPOINT.Get(dwIndex);
   if (!p)
      return FALSE;
   pt->h = p->fTime;
   pt->v = p->fValue;
   if (pdwLinear)
      *pdwLinear = p->dwLinear;
   return TRUE;
}

/**********************************************************************************
CAnimAttrib::PointSet - Sets the point at location dwIndex to pt. NOTE: If the
point is temporary moved left/right of the surrounding points then it is automatically
repositioned in time - which means that the point number for that point may change

inputs
   DWORD          dwIndex - from 0..PointNum()-1
   PTEXTUREPOINT  pt - Value of the point. .h = time, .v = value
   DWORD          dwLinear - How the values are interpolated after this, 0 for
                     constant, 1 for linear, 2 for spline
returns
   BOOL - TRUE if succede
*/
BOOL CAnimAttrib::PointSet (DWORD dwIndex, PTEXTUREPOINT pt, DWORD dwLinear)
{
   if (dwIndex >= m_lAAPOINT.Num())
      return FALSE;  // out of bounds

   // get the points
   DWORD dwNum;
   PAAPOINT ptp;
   dwNum = m_lAAPOINT.Num();
   ptp = (PAAPOINT) m_lAAPOINT.Get(0);

   // can use the same index
   BOOL fSameIndex;
   fSameIndex = TRUE;
   if (ptp[dwIndex].fTime != pt->h) {
      if (dwIndex && (ptp[dwIndex-1].fTime >= pt->h))
         fSameIndex = FALSE;
      if (fSameIndex && (dwIndex+1 < dwNum) && (ptp[dwIndex+1].fTime <= pt->h))
         fSameIndex = FALSE;
   }

   if (fSameIndex) {
      ptp[dwIndex].fTime = pt->h;
      ptp[dwIndex].fValue = pt->v;
      ptp[dwIndex].dwLinear = dwLinear;
      return TRUE;
   }

   // else, because might actually move onto an existing point, safest bug solution
   // is to remove and then re-add
   PointRemove (dwIndex);
   PointAdd (pt, dwLinear);

   return TRUE;
}


/**********************************************************************************
CAnimAttrib::PointRemove - Removes a point based on the index

inputs
   DWORD          dwIndex - from 0..PointNum()-1
returns
   BOOL - TRUE if succede
*/
BOOL CAnimAttrib::PointRemove (DWORD dwIndex)
{
   return m_lAAPOINT.Remove (dwIndex);
}

/**********************************************************************************
CAnimAttrib::PointClosest - Given a time, this returns the point closest to the time
requested.

inputs
   fp             fTime - Time looking for
   int            iDir - if 0 looking for closest entry either direction, -1 looking
                         for closest with time <= fTime. 1 looking for time >= fTime
returns
   DWORD - Index. -1 if cant find
*/
DWORD CAnimAttrib::PointClosest (fp fTime, int iDir)
{
   // find the general area
   DWORD dwNum = m_lAAPOINT.Num();
   PAAPOINT ptp = (PAAPOINT) m_lAAPOINT.Get(0);

   if (!dwNum)
      return -1;  // cant find beause no entries

   DWORD dwCur, dwIndex, dwTry;
   dwIndex = 0;
   for (dwCur = 1; dwCur < dwNum; dwCur *= 2);
   for (; dwCur; dwCur /= 2) {
      dwTry = dwIndex + dwCur;
      if (dwTry >= dwNum)
         continue;   // too high

      if (fTime >= ptp[dwTry].fTime)
         dwIndex = dwTry;
      // else, try is to the right of time so dont go there
   }
   // end up with ptp[dwIndex].h <= fTime and ptp[dwIndex+1].h > fTime, unless dwIndex == 0


   // if fTime == ptp[dwIndex].h then found no matter what
   if (fTime == ptp[dwIndex].fTime)
      return dwIndex;

   // if index at zero then fTime may never have >= ptp[dwTry].h, which
   // requires a special case
   if ((dwIndex == 0) && (fTime < ptp[dwIndex].fTime)) {
      if (iDir < 0)
         return -1;  // looking to the left, so no match
      return 0;   // else, looking for closest or to right, so choose 0
   }

   // if index is the last one then special case, because fTime > dwIndex
   if (dwIndex+1 >= dwNum) {
      return ((iDir <= 0) ? dwIndex : -1);
   }

   if (iDir == 0) { // closest
      fp fDist1, fDist2;
      fDist1 = ptp[dwIndex].fTime - fTime;
      fDist2 = ptp[dwIndex+1].fTime - fTime;  // can safely do this
      if (fabs (fDist1) <= fabs(fDist2))
         return dwIndex;
      else
         return dwIndex+1;
   }
   else if (iDir < 0) { // left
      return dwIndex;
   }
   else {   // right
      return dwIndex+1;
   }
}

/**********************************************************************************
CAnimAttrib::PointAdd - Adds a point into the list. It automatically inserts it in
the right place. If another point has the same time then the other point is overwritten.

inputs
   PTEXTUREPOINT  pt - point. pt.h = time, pt.v = value
   DWORD          dwLinear - 0 for constant, 1 for linear, 2 for spline
returns
   none
*/
void CAnimAttrib::PointAdd (PTEXTUREPOINT pt, DWORD dwLinear)
{
   DWORD dwClosest = PointClosest (pt->h, 1);

   AAPOINT aa;
   aa.fTime = pt->h;
   aa.fValue = pt->v;
   aa.dwLinear = dwLinear;

   if (dwClosest == -1) {
      // nothing closest to right, which means add to end
      m_lAAPOINT.Add (&aa);
      return;
   }

   // else, see if this is a match
   PAAPOINT p;
   p = (PAAPOINT) m_lAAPOINT.Get(dwClosest);
   if (p->fTime == pt->h) {
      // found exact match
      p->fValue = pt->v;
      p->dwLinear = dwLinear;
      return;
   }

   // else, insert
   m_lAAPOINT.Insert (dwClosest, &aa);
}


/**********************************************************************************
CAnimAttrib::ValueGet - Gets a value at any time. If the value ends up being between
points then a spline is used.

inputs
   fp          fTime - Time
returns
   fp - Value. May be spline interpolated
*/
fp CAnimAttrib::ValueGet (fp fTime)
{
   DWORD dwLeft = PointClosest (fTime, -1);
   DWORD dwNum = m_lAAPOINT.Num();
   PAAPOINT ptp = (PAAPOINT) m_lAAPOINT.Get(0);

   // if no points then nothing
   if (!dwNum)
      return 0;

   // if there's no point to the left then return the value furthest to the left
   if (dwLeft == -1)
      return ptp[0].fValue;

   // if the point to the left is the last point then return the last point
   if (dwLeft+1 >= dwNum)
      return ptp[dwNum-1].fValue;

   // if exact match then dont bother
   if (ptp[dwLeft].fTime == fTime)
      return ptp[dwLeft].fValue;

   // get the 4 values
   fp af[4];
   af[0] = ptp[dwLeft - (dwLeft ? 1 : 0)].fValue;
   af[1] = ptp[dwLeft].fValue;
   af[2] = ptp[min(dwLeft+1, dwNum-1)].fValue;
   af[3] = ptp[min(dwLeft+2, dwNum-1)].fValue;

   // if it's an angle then do module
   if (m_dwType == AIT_ANGLE) {
      af[0] = AngleClosestModulo (af[0], af[1]);
      af[2] = AngleClosestModulo (af[2], af[1]);
      af[3] = AngleClosestModulo (af[3], af[2]);
   }

   // amount
   fp fRet;
   if ((m_dwType == AIT_BOOL) || (ptp[dwLeft].dwLinear == 0)) {
      // if boolean always set param absolute
      fRet = af[1];
   }
   else {
      fp t;
      t = ptp[dwLeft+1].fTime - ptp[dwLeft].fTime;
      if (!t)
         return ptp[dwLeft].fValue;   // shouldnt ever happen
      t = (fTime - ptp[dwLeft].fTime) / t;

      if (ptp[dwLeft].dwLinear == 1)
         fRet = t * af[2] + (1.0 - t) * af[1];  // linear
      else
         fRet = HermiteCubic (t, af[0], af[1], af[2], af[3]);
   }

   if (m_dwType == AIT_ANGLE)
      fRet = AngleModulo (fRet);

   return fRet;
}

/**********************************************************************************
CAnimAttrib::Clone - Clones this animation set
*/
CAnimAttrib *CAnimAttrib::Clone (void)
{
   PCAnimAttrib pNew = new CAnimAttrib;
   if (!pNew)
      return NULL;
   
   pNew->m_dwType = m_dwType;
   pNew->m_fMin = m_fMin;
   pNew->m_fMax = m_fMax;

   wcscpy (pNew->m_szName, m_szName);
   pNew->m_lAAPOINT.Init (sizeof(AAPOINT), m_lAAPOINT.Get(0), m_lAAPOINT.Num());

   return pNew;
}






/**********************************************************************************
CSceneObj::Constructor and destructor
*/
CSceneObj::CSceneObj (void)
{
   m_pScene = NULL;
   m_lPCAnimSocket.Init (sizeof(PCAnimSocket));
   m_fAnimAttribValid = FALSE;
   m_pAnimAttribWithout = NULL;
   m_lPCAnimAttrib.Init (sizeof(PCAnimAttrib));
   memset (&m_gObjectWorld, 0, sizeof(m_gObjectWorld));
}

CSceneObj::~CSceneObj (void)
{
   PCAnimSocket *ppa = (PCAnimSocket*) m_lPCAnimSocket.Get(0);
   PCAnimAttrib *ppaa = (PCAnimAttrib*) m_lPCAnimAttrib.Get(0);
   DWORD i;
   for (i = 0; i < m_lPCAnimSocket.Num(); i++)
      ppa[i]->Delete ();
   for (i = 0; i < m_lPCAnimAttrib.Num(); i++)
      delete ppaa[i];
}

/**********************************************************************************
CSceneObj::WaveFormatGet - Returns the highest sampling rate and number of channels
found in the all the animation objects. This can be used to determine what sampling
rate to use when generating an AVI.

inputs
   DWORD          *pdwSamplesPerSec - If wave data exists filled in with the highest
   DWORD          *pdwChannels - If wave data exists filled in with the highest
returns
   BOOL - TRUE if wave data exists. FALSE if none of the sub-objects contain wave data
*/
BOOL CSceneObj::WaveFormatGet (DWORD *pdwSamplesPerSec, DWORD *pdwChannels)
{
   DWORD dwSamp, dwChan, dwTempSamp, dwTempChan;
   dwSamp = dwChan = 0;

   DWORD i;
   PCAnimSocket *ppa;
   ppa = (PCAnimSocket*) m_lPCAnimSocket.Get(0);
   for (i = 0; i < m_lPCAnimSocket.Num(); i++) {
      if (ppa[i]->WaveFormatGet (&dwTempSamp, &dwTempChan)) {
         dwSamp = max(dwSamp, dwTempSamp);
         dwChan = max(dwChan, dwTempChan);
      }
   }

   *pdwSamplesPerSec = dwSamp;
   *pdwChannels = dwChan;
   return (dwSamp && dwChan);
}


/**********************************************************************************
CSceneObj::GraphMovePoint - Moves a point that occurs on a the graph from one
time to another.

NOTE: This automagically creates/deletes keyframe objects.

inputs
   PWSTR             pszAttrib - Attribute name
   PTEXTUREPOINT     ptOld - Old/previous time/value. If NULL then there's no old value.
   PTEXTUREPOINT     ptNew - New time/value. NOTE: If the time's match between old and new
                     do less processing. If NULL then just removed the one clicked on
   DWORD             dwLinear - What linear setting to use

returns
   BOOL - TRUE if success
*/
BOOL CSceneObj::GraphMovePoint (PWSTR pszAttrib, PTEXTUREPOINT ptOld,
                                 PTEXTUREPOINT ptNew, DWORD dwLinear)
{
   // see if changing times
   BOOL fNewTime;
   if (ptNew && ptOld)
      fNewTime = (fabs(ptOld->h - ptNew->h) > CLOSE);
   else
      fNewTime = TRUE;  // so remove

   // find the object
   PCSceneObj pSceneObj;
   pSceneObj = this;

   // find the type - if it's bool then dont care about linear-nesss
   PCAnimAttrib paa;
   BOOL fIsBOOL;
   paa = pSceneObj->AnimAttribGet(pszAttrib, TRUE);
   fIsBOOL = (paa && (paa->m_dwType == AIT_BOOL));

   // if changing times, remove the old one first
   DWORD i;
   PCAnimSocket pas;
   fp fStart, fEnd;
   OSMKEYFRAME osk;
   osk.pak = NULL;
   if (fNewTime && ptOld) {
      // find any keyframes with old values
      for (i = pSceneObj->ObjectNum() - 1; i < pSceneObj->ObjectNum(); i--) {
         pas = pSceneObj->ObjectGet(i);
         if (!pas)
            continue;
         pas->TimeGet (&fStart, &fEnd);
         if (fStart != fEnd)
            continue;   // not an instantaneous event
         if (ptOld && (fabs(fStart- ptOld->h) > CLOSE))
            continue;   // not happening at the same time

         if (!pas->Message (OSM_KEYFRAME, &osk))
            continue;

         // found it, so remove
         osk.pak->AttribRemove (pszAttrib);

         // if no attributes left then delete entire object
         if (!osk.pak->m_lATTRIBVAL.Num())
            pSceneObj->ObjectRemove (i);

         // reset for next time
         osk.pak = NULL;   // for next time
      } // i
   } // if moving time

   // if no new location then done
   if (!ptNew)
      return TRUE;

   // find out what objects exist where moving to
   CListFixed lPCAnimKeyframe;
   lPCAnimKeyframe.Init (sizeof(PCAnimKeyframe));
   for (i = 0; i < pSceneObj->ObjectNum(); i++) {
      pas = pSceneObj->ObjectGet(i);
      if (!pas)
         continue;
      pas->TimeGet (&fStart, &fEnd);
      if ((fStart != fEnd) || (fabs(fStart- ptNew->h) > CLOSE))
         continue;   // not an instantaneous event

      if (!pas->Message (OSM_KEYFRAME, &osk))
         continue;

      // add htis
      lPCAnimKeyframe.Add (&osk.pak);
      osk.pak = NULL;   // for next time
   }

   // go through all of these
   DWORD j;
   PCAnimKeyframe *ppa;
   PCAnimKeyframe pOK;
   BOOL fSuccess;
   DWORD dwLen;
   DWORD dwType;
   // regest anim object list because may change each time
   ppa = (PCAnimKeyframe*) lPCAnimKeyframe.Get(0);
   pOK = NULL; // one that can accept the attribute
   fSuccess = FALSE; // found one to take attribute
   dwLen = (DWORD)wcslen(pszAttrib);

   // figure out the type of this attribute
   if (IsTemplateAttributeLoc (pszAttrib, dwLen))
      dwType = 1;
   else if (IsTemplateAttributeRot (pszAttrib, dwLen))
      dwType = 2;
   else
      dwType = 0;

   for (j = 0; j < lPCAnimKeyframe.Num(); j++) {
      if (ppa[j]->m_dwType != dwType)
         continue;   // wont support it

      // remember this in case need one to add to
      pOK = ppa[j];
      if (!fIsBOOL && (pOK->m_dwLinear != dwLinear))
         continue;   // not the same linearity so cant use

      // try adding it - but only if it already exists
      if (pOK->AttributeSet (pszAttrib, ptNew->v, FALSE))
         fSuccess = TRUE;
      // NOTE: Have to go through all the keyframes because it's possible have two
      // setting the same attribute at the same time
   }
   
   // if success then dont worry about this
   if (fSuccess)
      return TRUE;

   // else, didn't write. If found one OK then use that
   if (pOK) {
      pOK->AttributeSet (pszAttrib, ptNew->v, TRUE);
      return TRUE;
   }

   // else, need to create a new one
   pOK = new CAnimKeyframe (dwType);
   if (!pOK)
      return FALSE;   // give up
   pOK->m_dwLinear = dwLinear;
   pOK->TimeSet (ptNew->h, ptNew->h);
   pOK->VertSet ((fp) dwType * 30); // so placed in different locations
   pOK->AttributeSet (pszAttrib, ptNew->v, TRUE);
   pSceneObj->ObjectAdd (pOK);

   // done
   return TRUE;

}
/**********************************************************************************
CSceneObj::SceneSet - Tells the sceneobj to use the given scene. This must be called
just after the creation of the object/

inputs
   PCScene     pScene - Scene to use
*/
void CSceneObj::SceneSet (PCScene pScene)
{
   m_pScene = pScene;
}

/**********************************************************************************
CSceneObj::SceneGet - Returns the scene that this sceneobj is assigned to
*/
PCScene CSceneObj::SceneGet (void)
{
   return m_pScene;
}

/**********************************************************************************
CSceneObj::SceneSetGet - Returns the scene that this sceneobj's sceen is assigned to.
*/
PCSceneSet CSceneObj::SceneSetGet (void)
{
   if (!m_pScene)
      return NULL;
   return m_pScene->SceneSetGet ();
}

/**********************************************************************************
CSceneObj::WorldGet - Returns the world that this sceneobj's scene's sceneset is assigned to.
*/
PCWorld CSceneObj::WorldGet (void)
{
   if (!m_pScene)
      return NULL;
   return m_pScene->WorldGet();
}

/**********************************************************************************
CSceneObj::GUIDGet - Fills pgObjectWorld with the GUID of the object this is modifying.

inputs
   GUID        *pgObjectWorld - Filled with the GUID (for PCObjectSocket) that this is
                  modifying
returns
   none
*/
void CSceneObj::GUIDGet (GUID *pgObjectWorld)
{
   *pgObjectWorld = m_gObjectWorld;
}


/**********************************************************************************
CSceneObj::GUIDSet - Sets GUID of the object this is modifying. THis must be called
after initialization.

inputs
   GUID        *pgObjectWorld - Contains the GUID (for PCObjectSocket) that this is
                  modifying
returns
   none
*/
void CSceneObj::GUIDSet (GUID *pgObjectWorld)
{
   m_gObjectWorld = *pgObjectWorld;
}


/**********************************************************************************
CSceneObj::Clone - Clones this object and all its contents.

inputs
   PCScene        pScene - What scene the new object will be using
returns
   PCSceneObj - New scneeobj
*/
CSceneObj *CSceneObj::Clone (PCScene pScene)
{
   PCSceneObj pNew = new CSceneObj;
   if (!pNew)
      return NULL;

   pNew->m_pScene = pScene;
   pNew->m_fAnimAttribValid = m_fAnimAttribValid;
   pNew->m_pAnimAttribWithout = m_pAnimAttribWithout;
   pNew->m_gObjectWorld = m_gObjectWorld;

   DWORD i;
   PCAnimSocket *pps;
   PCWorld pWorld;
   GUID g;
   pWorld = pNew->m_pScene->WorldGet();
   pNew->m_lPCAnimSocket.Init (sizeof(PCAnimSocket), m_lPCAnimSocket.Get(0), m_lPCAnimSocket.Num());
   pps = (PCAnimSocket*) pNew->m_lPCAnimSocket.Get(0);
   for (i = 0; i < pNew->m_lPCAnimSocket.Num(); i++) {
      pps[i] = pps[i]->Clone();
      GUIDGen (&g);
      pps[i]->GUIDSet (&g);
      pps[i]->WorldSet (pWorld, pNew->m_pScene, &pNew->m_gObjectWorld);
   }

   PCAnimAttrib *ppa;
   pNew->m_lPCAnimAttrib.Init (sizeof(PCAnimAttrib), m_lPCAnimAttrib.Get(0), m_lPCAnimAttrib.Num());
   ppa = (PCAnimAttrib*) pNew->m_lPCAnimAttrib.Get(0);
   for (i = 0; i < pNew->m_lPCAnimAttrib.Num(); i++)
      ppa[i] = ppa[i]->Clone();

   return pNew;
}


static PWSTR gpszSceneObj = L"SceneObj";
static PWSTR gpszObjectWorld = L"ObjectWorld";
static PWSTR gpszAnimSocket = L"AnimSocket";
static PWSTR gpszAnimClass = L"SOAnimClass";

/**********************************************************************************
CSceneObj::MMLTo - Creates a MML node describing this. Standard API
*/
PCMMLNode2 CSceneObj::MMLTo (void)
{
   PCMMLNode2 pNode = new CMMLNode2;
   if (!pNode)
      return NULL;
   pNode->NameSet (gpszSceneObj);

   MMLValueSet (pNode, gpszObjectWorld, (PBYTE) &m_gObjectWorld, sizeof(m_gObjectWorld));

   DWORD i;
   PCAnimSocket *ppa;
   ppa = (PCAnimSocket*) m_lPCAnimSocket.Get(0);
   for (i = 0; i < m_lPCAnimSocket.Num(); i++) {
      PCMMLNode2 pSub = ppa[i]->MMLTo ();
      if (!pSub)
         continue;
      pSub->NameSet (gpszAnimSocket);

      // add the class
      GUID gClass;
      ppa[i]->ClassGet (&gClass);
      MMLValueSet (pSub, gpszAnimClass, (PBYTE) &gClass, sizeof(gClass));

      pNode->ContentAdd (pSub);
   }

   return pNode;
}


/**********************************************************************************
CSceneObj::MMLFrom - Read from MML. Standard
*/
BOOL CSceneObj::MMLFrom (PCMMLNode2 pNode)
{
   // clear existing
   PCAnimSocket *ppa = (PCAnimSocket*) m_lPCAnimSocket.Get(0);
   PCAnimAttrib *ppaa = (PCAnimAttrib*) m_lPCAnimAttrib.Get(0);
   DWORD i;
   for (i = 0; i < m_lPCAnimSocket.Num(); i++)
      ppa[i]->Delete ();
   for (i = 0; i < m_lPCAnimAttrib.Num(); i++)
      delete ppaa[i];
   m_lPCAnimSocket.Clear();
   m_lPCAnimAttrib.Clear();
   m_fAnimAttribValid = FALSE;

   // get the guid
   if (!MMLValueGetBinary (pNode, gpszObjectWorld, (PBYTE) &m_gObjectWorld, sizeof(m_gObjectWorld)))
      return FALSE;

   // all the animation sockets
   PWSTR psz;
   PCMMLNode2 pSub;
   for (i = 0; i < pNode->ContentNum(); i++) {
      pSub = NULL;
      pNode->ContentEnum (i, &psz, &pSub);
      if (!pSub)
         continue;
      psz = pSub->NameGet();

      if (psz && !_wcsicmp(psz, gpszAnimSocket)) {
         // get the GUID
         GUID gClass;
         if (!MMLValueGetBinary (pSub, gpszAnimClass, (PBYTE) &gClass, sizeof(gClass)))
            continue;

         // create the object
         PCAnimSocket pNew;
         if (IsEqualGUID (gClass, CLSID_AnimKeyframe))
            pNew = new CAnimKeyframe (0);
         else if (IsEqualGUID (gClass, CLSID_AnimPath))
            pNew = new CAnimPath;
         else if (IsEqualGUID (gClass, CLSID_AnimWave))
            pNew = new CAnimWave;
         else if (IsEqualGUID (gClass, CLSID_AnimBookmark))
            pNew = new CAnimBookmark (0);
         else
            pNew = NULL;
   
         if (!pNew)
            continue;
         if (!pNew->MMLFrom (pSub)) {
            delete pNew;
            continue;
         }

         // add it
         GUID gGUID;
         pNew->GUIDGet (&gGUID);
         ObjectAdd (pNew, TRUE, &gGUID);
       } // if animsocket
   } // over all node

   return TRUE;
}


/**********************************************************************************
CSceneObj::ObjectNum - Returns the number of animation objects in here
*/
DWORD CSceneObj::ObjectNum (void)
{
   return m_lPCAnimSocket.Num();
}

/**********************************************************************************
CSceneObj::ObjectGet - Gets the object based on an index.

inputs
   DWORD       dwIndex - From 0 .. ObjectNum()-1
returns
   PCAnimSocket - Animation object. NULL if cant find
*/
PCAnimSocket CSceneObj::ObjectGet (DWORD dwIndex)
{
   PCAnimSocket *pps = (PCAnimSocket*)m_lPCAnimSocket.Get(dwIndex);
   if (!pps)
      return NULL;
   return pps[0];
}

/**********************************************************************************
CSceneObj::ObjectFind - Finds the animation socket with the given guid.

inputs
   GUID        *pgObjectAnim - GUID looking for
returns
   DWORD - Index. -1 if cant find
*/
DWORD CSceneObj::ObjectFind (GUID *pgObjectAnim)
{
   PCAnimSocket *pps = (PCAnimSocket*)m_lPCAnimSocket.Get(0);
   DWORD i;
   GUID g;

   for (i = 0; i < m_lPCAnimSocket.Num(); i++) {
      pps[i]->GUIDGet (&g);
      if (IsEqualGUID (g, *pgObjectAnim))
         return i;
   }

   return -1;
}

/**********************************************************************************
CSceneObj::ObjectRemove - Removes the object from the list. In the process it deletes it.
   The object goes into undo. This also ends up sending out notifications.

inputs
   DWORD       dwIndex - From 0 .. ObjectNum()-1
   BOOL              fDontRememberForUndo - if FALSE (default) then updates the undo
                        buffer and sends out notifiations. if TRUE then wont do that
returns
   BOOL - TRUE if removed
*/
BOOL CSceneObj::ObjectRemove (DWORD dwIndex, BOOL fDontRememberForUndo)
{
   PCAnimSocket *pps = (PCAnimSocket*)m_lPCAnimSocket.Get(dwIndex);
   if (!pps)
      return FALSE;

   if (!fDontRememberForUndo)
      SceneSetGet()->ObjectRemoved (pps[0]);

   // NOTE: Not deleting because called to ObjectRemoved() should put into undo
   // buffer

   m_lPCAnimSocket.Remove (dwIndex);

   // note that spline calculations dirty
   m_fAnimAttribValid = FALSE;
   return TRUE;
}


/**********************************************************************************
CSceneObj::ObjectSwapForUndo - Called when the world does an undo/redo... this swaps
the current object at index with an older version of the same object.

inputs
   DWORD          dwIndex - Index into the object list
   PCAnimSocket   pSwap - Older version of the object
returns
   PCAnimSoceket - The one swapped out
*/
PCAnimSocket CSceneObj::ObjectSwapForUndo (DWORD dwIndex, PCAnimSocket pSwap)
{
   PCAnimSocket *ppa = (PCAnimSocket*) m_lPCAnimSocket.Get(dwIndex);
   if (!ppa)
      return NULL;

   // note that spline calculations dirty
   m_fAnimAttribValid = FALSE;

   PCAnimSocket pRet;
   pRet = ppa[0];
   ppa[0] = pSwap;
   return pRet;
}

/**********************************************************************************
CSceneObj::ObjectAdd - Adds the object to the list. In the process it
updates the undo buffer, sends out notification about the add, and makes note
that the animation attributes are no longer valid.

inputs
   PCAnimSocket      pObject - Object to add
   BOOL              fDontRememberForUndo - if FALSE (default) then updates the undo
                        buffer and sends out notifiations. if TRUE then wont do that
   GUID              *pgID - If not NULL then sets the object's guid to this.
                        Otherwise, makes a new GUID
returns
   DWORD - New object ID. -1 if error
*/
DWORD CSceneObj::ObjectAdd (PCAnimSocket pObject, BOOL fDontRememberForUndo, GUID *pgID)
{
   pObject->WorldSet (WorldGet(), SceneGet(), &m_gObjectWorld);
   GUID g;
   if (!pgID) {
      pgID = &g;
      GUIDGen (&g);
   }
   pObject->GUIDSet (pgID);

   // number
   DWORD dwNum;
   dwNum = m_lPCAnimSocket.Num();
   m_fAnimAttribValid = FALSE;   // note that spline calculations dirty
   m_lPCAnimSocket.Add (&pObject);

   // tell the world about this?
   if (!fDontRememberForUndo) {
      PCSceneSet pSceneSet = SceneSetGet();
      if (pSceneSet)
         pSceneSet->ObjectAdded (pObject);
#ifdef _DEBUG
      else
         pSceneSet = NULL;
#endif // 0
   }

   return dwNum;
}


/**********************************************************************************
CSceneObj::SyncFromWorld - This function does the following:
1) Finds the object in the world.
2) Gets all the attributes of the object.
3) Make sure all the attributes are generated
4) Any attributes which are not the same (or close) that what expecting are flagged.
5) If there's already one (or more) keyframe objects they're updated so that the attributes
   will be the same. If not, a keyframe object is created
*/
void CSceneObj::SyncFromWorld (void)
{
   PCWorld pWorld;
   if (!m_pScene)
      return;
   pWorld = m_pScene->WorldGet ();
   if (!pWorld)
      return;

   // current time
   PCSceneSet pSceneSet;
   pSceneSet = m_pScene->SceneSetGet ();
   if (!pSceneSet)
      return;
   fp fTime;
   pSceneSet->StateGet (NULL, &fTime);

   // get the object's current attributes
   PCObjectSocket pos;
   CListFixed lAV;
   pos = pWorld->ObjectGet(pWorld->ObjectFind(&m_gObjectWorld));
   if (!pos)
      return;
   lAV.Init (sizeof(ATTRIBVAL));
   pos->AttribGetAll (&lAV);
   
   // figure out what they should be
   AnimAttribGenerate();
   DWORD i;
   fp fVal;
   for (i = lAV.Num()-1; i < lAV.Num(); i--) {
      PATTRIBVAL pav = (PATTRIBVAL) lAV.Get(i);
      
      // see if have graph for it
      PCAnimAttrib paa;
      paa = AnimAttribGet (pav->szName);
      if (paa) {
         fVal = paa->ValueGet (fTime);

         // if the animattribute is an angle then get the closest module
         if (paa->m_dwType == AIT_ANGLE)
            fVal = AngleClosestModulo(fVal, pav->fValue);
      }
      // else, see what the default value is
      else if (!pSceneSet->DefaultAttribGet (&m_gObjectWorld, pav->szName, &fVal, TRUE))
         fVal = pav->fValue;   // no default attribute (or graph) so dont bother storing away change since not used

      // if changed a lot then keep
      if (fabs(fVal - pav->fValue) >= CLOSE)
         continue;

      // else, not changed much so remove
      lAV.Remove (i);
   }
   if (!lAV.Num())
      return;  // no changes

   // make a list of all the keyframes at the current time
   CListFixed lPCAnimKeyframe;
   PCAnimSocket pas;
   OSMKEYFRAME osk;
   memset (&osk, 0 ,sizeof(osk));
   fp fStart, fEnd;
   lPCAnimKeyframe.Init (sizeof(PCAnimKeyframe));
   for (i = 0; i < ObjectNum(); i++) {
      pas = ObjectGet(i);
      if (!pas)
         continue;
      pas->TimeGet (&fStart, &fEnd);
      if ((fStart != fEnd) || (fabs(fStart- fTime) > CLOSE))
         continue;   // not an instantaneous event

      if (!pas->Message (OSM_KEYFRAME, &osk))
         continue;

      // add htis
      lPCAnimKeyframe.Add (&osk.pak);
      osk.pak = NULL;   // for next time
   }

   // go through the attributes and put them someplace
   DWORD j;
   PCAnimKeyframe *ppa;
   PCAnimKeyframe pOK;
   PATTRIBVAL pav;
   BOOL fSuccess;
   DWORD dwLen;
   DWORD dwType;
   BOOL fCreateLoc, fCreateRot;
   fCreateLoc = fCreateRot = FALSE;
   pav = (PATTRIBVAL) lAV.Get(0);
   for (i = 0; i < lAV.Num(); i++, pav++) {
      // regest anim object list because may change each time
      ppa = (PCAnimKeyframe*) lPCAnimKeyframe.Get(0);
      pOK = NULL; // one that can accept the attribute
      fSuccess = FALSE; // found one to take attribute
      dwLen = (DWORD)wcslen(pav->szName);

      // figure out the type of this attribute
      if (IsTemplateAttributeLoc (pav->szName, dwLen))
         dwType = 1;
      else if (IsTemplateAttributeRot (pav->szName, dwLen))
         dwType = 2;
      else
         dwType = 0;

      for (j = 0; j < lPCAnimKeyframe.Num(); j++) {
         if (ppa[j]->m_dwType != dwType)
            continue;   // wont support it

         // remember this in case need one to add to
         pOK = ppa[j];

         // try adding it - but only if it already exists
         if (pOK->AttributeSet (pav->szName, pav->fValue, FALSE))
            fSuccess = TRUE;
         // NOTE: Have to go through all the keyframes because it's possible have two
         // setting the same attribute at the same time
      }
      
      // if success then dont worry about this
      if (fSuccess)
         continue;

      // else, didn't write. If found one OK then use that
      if (pOK) {
         pOK->AttributeSet (pav->szName, pav->fValue, TRUE);
         continue;
      }

      // else, need to create a new one
      pOK = new CAnimKeyframe (dwType);
      if (!pOK)
         continue;   // give up
      pOK->TimeSet (fTime, fTime);
      pOK->VertSet ((fp) dwType * 30); // so placed in different locations
      pOK->AttributeSet (pav->szName, pav->fValue, TRUE); // only really need to call this for type 0
      ObjectAdd (pOK);
      lPCAnimKeyframe.Add (&pOK);

      // BUGFIX - If type is 1 or 2 (location or rotation) and just added, then
      // sync it all
      if ((dwType == 1) || (dwType == 2))
         pOK->SyncFromWorld ();

      // remember what created
      if (dwType == 1)
         fCreateLoc = TRUE;
      else if (dwType == 2)
         fCreateRot = TRUE;
   }

   // BUGFIX - If create a location then create rotation at same time, and vice versa,
   // since user usually thinks as tied together
   if ((fCreateLoc && !fCreateRot) || (!fCreateLoc && fCreateRot)) {
      // else, need to create a new one
      pOK = new CAnimKeyframe (dwType = (fCreateLoc ? 2 : 1));
      if (!pOK)
         return;
      pOK->TimeSet (fTime, fTime);
      pOK->VertSet ((fp) dwType * 30); // so placed in different locations
      ObjectAdd (pOK);

      // BUGFIX - If type is 1 or 2 (location or rotation) and just added, then
      // sync it all
      pOK->SyncFromWorld ();
   }
}

/**********************************************************************************
CSceneObj::SyncToWorld - This function does the following:
1) Make sure all the attributes are generated.
2) Finds the object in the world
3) Gets the attributes of the object.
4) Based on the current scene and time, figures out what the attributes should be.
5) If they're not the same then the attributes are updated.
*/
void CSceneObj::SyncToWorld (void)
{
   // make sure all the attributes are generated
   AnimAttribGenerate ();

   // find the object in the world and get the objects
   PCWorld pWorld;
   PCSceneSet pSceneSet;
   PCObjectSocket pos;
   fp fTime;
   pWorld = m_pScene->WorldGet();
   pSceneSet = m_pScene->SceneSetGet();
   if (!pWorld || !pSceneSet)
      return;
   pos = pWorld->ObjectGet(pWorld->ObjectFind (&m_gObjectWorld));
   if (!pos)
      return;
   pSceneSet->StateGet (NULL, &fTime);

   // get all the attributes for the object
   CListFixed lAttribCur;
   lAttribCur.Init (sizeof(ATTRIBVAL));
   pos->AttribGetAll (&lAttribCur);

   // loop through these and update
   DWORD i;
   fp fVal;
   for (i = lAttribCur.Num()-1; i < lAttribCur.Num(); i--) {
      PATTRIBVAL pv = (PATTRIBVAL) lAttribCur.Get(i);

      // get the splien that affects
      PCAnimAttrib paa;
      paa = AnimAttribGet (pv->szName);
      if (paa) {
         fVal = paa->ValueGet (fTime);

         // if the animattribute is an angle then get the closest module
         if (paa->m_dwType == AIT_ANGLE)
            fVal = AngleClosestModulo(fVal, pv->fValue);

         if (fabs(fVal - pv->fValue) < CLOSE)
            goto remove;

         pv->fValue = fVal;
         continue;
      }

      // else, see if there's an attribute default in scene set
      if (pSceneSet->DefaultAttribGet (&m_gObjectWorld, pv->szName, &fVal, FALSE)) {
         if (fVal == pv->fValue)
            goto remove;
         pv->fValue = fVal;
         continue;
      }

remove:
      // else, dont actually change the attribute, so remove it
      lAttribCur.Remove (i);
   }

   // what's left is a list of attributes that have changed, so set them
   if (lAttribCur.Num())
      pos->AttribSetGroup (lAttribCur.Num(), (PATTRIBVAL) lAttribCur.Get(0));
}

/**********************************************************************************
CSceneObj::AnimAttribDirty - Sets a flag to indicate that the calculated attributes
are dirty. called by CSceneSet when an object in world sends change notification
*/
void CSceneObj::AnimAttribDirty (void)
{
   m_fAnimAttribValid = FALSE;
}


/**********************************************************************************
CSceneObj::AnimAttribGenerateWithout - This is a function used by the CAnimTemplate
object's deconstruct button. It causes the AnimAttribGenerate() to generate the spline
WITHOUT the given object. After the deconstruct is done this must be called with
NULL so all objects will be called.

inputs
   PCAnimSocket         pasIgnore - Ignore this. Set to NULL to clear the ignroe list
retursn  
   none
*/
void CSceneObj::AnimAttribGenerateWithout (PCAnimSocket pasIgnore)
{
   m_fAnimAttribValid = FALSE;
   m_pAnimAttribWithout = pasIgnore;
}

/**********************************************************************************
CSceneObj::AnimAttribGenerate - if the anim attributes are dirty this clears them
all and then fills them in.
*/
static int _cdecl AnimSocketSort (const void *elem1, const void *elem2)
{
   PCAnimSocket pdw1, pdw2;
   pdw1 = *((PCAnimSocket*) elem1);
   pdw2 = *((PCAnimSocket*) elem2);

   // sort by priotity
   int i1, i2;
   i1 = pdw1->PriorityGet ();
   i2 = pdw2->PriorityGet ();

   if (i1 < i2)
      return -1;
   else if (i1 > i2)
      return 1;

   // else, sort by start time, so ones that start at end first
   fp f1, f2;
   pdw1->TimeGet (&f1, NULL);
   pdw2->TimeGet (&f2, NULL);

   if (f1 < f2)
      return 1;
   else if (f1 > f2)
      return -1;
   else
      return 0;
}

BOOL CSceneObj::AnimAttribGenerate (void)
{
   if (m_fAnimAttribValid)
      return TRUE;

   // clear all the anim attributes out
   PCAnimAttrib *ppaa = (PCAnimAttrib*) m_lPCAnimAttrib.Get(0);
   DWORD i;
   for (i = 0; i < m_lPCAnimAttrib.Num(); i++)
      delete ppaa[i];
   m_lPCAnimAttrib.Clear();

   // copy the object list and sort by score
   CListFixed lObj;
   lObj.Init (sizeof(PCAnimSocket), m_lPCAnimSocket.Get(0), m_lPCAnimSocket.Num());
   qsort (lObj.Get(0), lObj.Num(), sizeof(PCAnimSocket), AnimSocketSort);

   // loop through all the objects and tell them to fill in their info
   PCAnimSocket *pps;
   pps = (PCAnimSocket*) lObj.Get(0);
   for (i = 0; i < lObj.Num(); i++) {
      if (pps[i] == m_pAnimAttribWithout)
         continue;   // ignoring this

      pps[i]->AnimObjSplineApply (this);
   }

   // Will need to make a second pass and have any keyframes re-set their
   // values. Otherwise SyncFromWorld() could get in infinite loops when setting keyframes
   // is not the final world on the value.
   for (i = 0; i < lObj.Num(); i++) {
      if (pps[i] == m_pAnimAttribWithout)
         continue;   // ignoring this

      if (pps[i]->AnimObjSplineSecondPass())
         pps[i]->AnimObjSplineApply (this);
   }

   // now have valid
   m_fAnimAttribValid = TRUE;

   return TRUE;
}


/**********************************************************************************
CSceneObj::AnimAttribNum - Returns the number of CAnimAttrib stored in this object.
*/
DWORD CSceneObj::AnimAttribNum (void)
{
   return m_lPCAnimAttrib.Num();
}

/**********************************************************************************
CSceneObj::AnimAttribGet - Gets a CAnimAttrib object by the index.

inputs
   DWORD       dwIndex - from 0 .. AnimAttribNUm()-1
returns 
   PCAnimAttrib - Attribute object. NULLif index too high
*/
PCAnimAttrib CSceneObj::AnimAttribGet (DWORD dwIndex)
{
   PCAnimAttrib *ppa = (PCAnimAttrib*) m_lPCAnimAttrib.Get(dwIndex);
   if (!ppa)
      return NULL;
   return ppa[0];
}

/**********************************************************************************
CSceneObj::AnimAttribGet - Gets an anim attribute by name.

inputs
   PWSTR          pszName - attribute name
   BOOL           fCreateIfNotExist - If TRUE and the attribute doesn't already exist
                     then a CAnimAttrib is created (and added to the list). It is filled
                     in with the starting animatable value. If this is called from a CAnimSocket
                     object that is going to adjust the attribute, then this should be set to TRUE.
returns
   PCAnimAttrib - Animation attribute
*/
PCAnimAttrib CSceneObj::AnimAttribGet (PWSTR pszName, BOOL fCreateIfNotExist)
{
   // search for it
   PCAnimAttrib *ppa = (PCAnimAttrib*) m_lPCAnimAttrib.Get(0);
   DWORD dwNum = m_lPCAnimAttrib.Num();
   DWORD dwCur, dwTry, dwIndex;
   for (dwCur = 1; dwCur < dwNum; dwCur *= 2);
   for (dwIndex = 0; dwCur; dwCur /= 2) {
      dwTry = dwIndex + dwCur;
      if (dwTry >= dwNum)
         continue;

      if (_wcsicmp(pszName, ppa[dwTry]->m_szName) >= 0)
         dwIndex = dwTry;
   }

   // see if found a match
   if ((dwIndex < dwNum) && !_wcsicmp(pszName, ppa[dwIndex]->m_szName))
      return ppa[dwIndex];

   // else, no match

   // if dont want to add then return NULL
   if (!fCreateIfNotExist)
      return NULL;

   // get the default value
   fp fDef;
   PCSceneSet pss;
   pss = SceneSetGet();
   if (!(pss->DefaultAttribGet (&m_gObjectWorld, pszName, &fDef, TRUE)))
      return NULL;   // error - object may not exist any more

   // get the attribute information
   ATTRIBINFO ai;
   PCWorld pWorld;
   PCObjectSocket pos;
   memset (&ai, 0, sizeof(ai));
   pWorld = pss->WorldGet ();
   if (pWorld)
      pos = pWorld->ObjectGet(pWorld->ObjectFind (&m_gObjectWorld));
   if (!pos || !pos->AttribInfo (pszName, &ai)) {
      ai.dwType = AIT_NUMBER;
      ai.fMin = -1;
      ai.fMax = 1;
   }

   // else create new one and fill with default
   PCAnimAttrib pNew;
   pNew = new CAnimAttrib;
   if (!pNew)
      return NULL;
   pNew->m_dwType = ai.dwType;
   pNew->m_fMin = ai.fMin;
   pNew->m_fMax = ai.fMax;
   wcscpy (pNew->m_szName, pszName);
   TEXTUREPOINT tp;
   tp.h = 0;   // start at 0 time
   tp.v = fDef;
   pNew->PointAdd (&tp, 2);

   // insert this in alphabetically
   if (dwIndex)
      dwIndex--;  // go back at least one
   for (dwIndex = 0; dwIndex < dwNum; dwIndex++)
      if (_wcsicmp(pszName, ppa[dwIndex]->m_szName) < 0)
         break;
   if (dwIndex < dwNum)
      m_lPCAnimAttrib.Insert (dwIndex, &pNew);
   else
      m_lPCAnimAttrib.Add (&pNew);

   // done
   return pNew;
}




/**********************************************************************************
CScene::Constructor and destructor
*/
CScene::CScene (void)
{
   m_pSceneSet = NULL;
   memset (&m_gGUID, 0, sizeof(m_gGUID));
   m_lPCSceneObj.Init (sizeof(PCSceneObj));
   m_fDuration = 60; // 60 seconds to start out with
   m_dwFPS = 30;

}

CScene::~CScene (void)
{
   DWORD i;
   PCSceneObj *ppo = (PCSceneObj*) m_lPCSceneObj.Get (0);
   for (i = 0; i < m_lPCSceneObj.Num(); i++)
      delete ppo[i];
   m_lPCSceneObj.Clear();
}


/**********************************************************************************
CScene::WaveFormatGet - Returns the highest sampling rate and number of channels
found in the all the animation objects. This can be used to determine what sampling
rate to use when generating an AVI.

inputs
   DWORD          *pdwSamplesPerSec - If wave data exists filled in with the highest
   DWORD          *pdwChannels - If wave data exists filled in with the highest
returns
   BOOL - TRUE if wave data exists. FALSE if none of the sub-objects contain wave data
*/
BOOL CScene::WaveFormatGet (DWORD *pdwSamplesPerSec, DWORD *pdwChannels)
{
   DWORD dwSamp, dwChan, dwTempSamp, dwTempChan;
   dwSamp = dwChan = 0;

   DWORD i;
   PCSceneObj pSceneObj;
   for (i = 0; i < ObjectNum(); i++) {
      pSceneObj = ObjectGet(i);
      if (!pSceneObj)
         continue;

      if (pSceneObj->WaveFormatGet (&dwTempSamp, &dwTempChan)) {
         dwSamp = max(dwSamp, dwTempSamp);
         dwChan = max(dwChan, dwTempChan);
      }
   }

   *pdwSamplesPerSec = dwSamp;
   *pdwChannels = dwChan;
   return (dwSamp && dwChan);
}


/**********************************************************************************
CameraEnum - Fills in a list with all the cameras in the world. Used
to display a list of cameras that can render from.

inputs
   PCWorldSocket  pWorld - World
   PCListFixed    pl - A list already initialized to sizeof (OSMANIMCAMERA). It may
                     already contain some cameras. New cameras will be appeneded.
returns
   BOOL - TRUE if success
*/
DLLEXPORT BOOL CameraEnum (PCWorldSocket pWorld, PCListFixed pl)
{
   DWORD i;
   OSMANIMCAMERA oac;
   PCObjectSocket pos;
   memset (&oac,0 , sizeof(oac));
   for (i = 0; i < pWorld->ObjectNum(); i++) {
      pos = pWorld->ObjectGet(i);
      if (!pos)
         continue;

      if (!pos->Message (OSM_ANIMCAMERA, &oac))
         continue;
      if (!oac.poac)
         continue;

      // add it
      pl->Add (&oac);
      oac.poac = NULL;  // reset
   }

   return TRUE;
}

/**********************************************************************************
CScene::CameraEnum - Fills in a list with all the cameras in the world. Used
to display a list of cameras that can render from.

inputs
   PCListFixed    pl - A list already initialized to sizeof (OSMANIMCAMERA). It may
                     already contain some cameras. New cameras will be appeneded.
returns
   BOOL - TRUE if success
*/
BOOL CScene::CameraEnum (PCListFixed pl)
{
   PCWorld pWorld;
   if (!m_pSceneSet)
      return FALSE;
   pWorld = m_pSceneSet->WorldGet();
   if (!pWorld)
      return FALSE;

   return ::CameraEnum (pWorld, pl);
}

/**********************************************************************************
CScene::BookmarkEnum - Fills a list in with all the bookmarks within the scene.

inputs
   PCListFixed    pl - A list already initialized to sizeof (OSMBOOKMARK). It may
                     already contain some bookmarks. New bookmarks will be appeneded.
   BOOL           fInstant - If TRUE include instant bookmarks
   BOOL           fDuration - If TRUE include durational bookmarks
returns
   BOOL - TRUE if success
*/
BOOL CScene::BookmarkEnum (PCListFixed pl, BOOL fInstant, BOOL fDuration)
{
   // loop
   DWORD i, j;
   PCSceneObj pSceneObj;
   PCAnimSocket pas;
   OSMBOOKMARK ob;
   for (i = 0; i < ObjectNum(); i++) {
      pSceneObj = ObjectGet(i);
      if (!pSceneObj)
         continue;

      for (j = 0; j < pSceneObj->ObjectNum(); j++) {
         pas = pSceneObj->ObjectGet (j);
         if (!pas)
            continue;

         if (!pas->Message (OSM_BOOKMARK, &ob))
            continue;

         // else found
         if (!fInstant && (ob.fStart == ob.fEnd))
            continue;
         if (!fDuration && (ob.fStart != ob.fEnd))
            continue;

         // else add
         pl->Add (&ob);
      }
   }

   return TRUE;
}


/**********************************************************************************
CScene::Clear - Clears out contents of scene
*/
void CScene::Clear (void)
{
   DWORD i;
   PCSceneObj *ppo = (PCSceneObj*) m_lPCSceneObj.Get (0);
   for (i = 0; i < m_lPCSceneObj.Num(); i++)
      delete ppo[i];
   m_lPCSceneObj.Clear();
}

/**********************************************************************************
CScene::SceneSetSet - Tells the scene object what SceneSet to use.

inputs
   PCSceneSet     pSet - sceneset to use
*/
void CScene::SceneSetSet (PCSceneSet pSet)
{
   m_pSceneSet = pSet;
}

/**********************************************************************************
CScene::SceneSetGet - Returns the SceneSet to use.

returns
   PCSceneSet     pSet - sceneset to use
*/
PCSceneSet CScene::SceneSetGet (void)
{
   return m_pSceneSet;
}


/**********************************************************************************
CScene::WorldGet - Returns the world to use.

returns
   PCWorld - world to use
*/
PCWorld CScene::WorldGet (void)
{
   if (!m_pSceneSet)
      return NULL;
   return m_pSceneSet->WorldGet();
}


/**********************************************************************************
CScene::GUIDSet - Sets the GUID that uniquely identifies the scene.

inputs
   GUID        *pgScene - GUID to use
*/
void CScene::GUIDSet (GUID *pgScene)
{
   m_gGUID = *pgScene;
}

/**********************************************************************************
CScene::GUIDGet - Gets the GUID that uniquely identifies the scene.

inputs
   GUID        *pgScene - GUID to fill in
*/
void CScene::GUIDGet (GUID *pgScene)
{
   *pgScene = m_gGUID;
}

/**********************************************************************************
CScene::NameSet - Sets the scene's name.

inputs
   PWSTR       pszName - name to use
returns
   none
*/
void CScene::NameSet (PWSTR pszName)
{
   DWORD dwLen = ((DWORD)wcslen(pszName)+1)*2;
   if (!m_memName.Required (dwLen))
      return;
   wcscpy ((PWSTR) m_memName.p, pszName);

}

/**********************************************************************************
CScene::NameGet - Returns a pointer to the scene's name. DONT change. This might
be NULL if no name has been set
*/
PWSTR CScene::NameGet (void)
{
   return (PWSTR) m_memName.p;
}

/**********************************************************************************
CScene::FPSGet - Returns the number of frames per second for the scene
*/
DWORD CScene::FPSGet (void)
{
   return m_dwFPS;
}

/**********************************************************************************
CScene::FPSSet - Sets the number of frames per second for the scene. If 0 then
no definite frames per second.
*/
void CScene::FPSSet (DWORD dwFPS)
{
   m_dwFPS = dwFPS;
}


/**********************************************************************************
CScene::ApplyGrid - Applies a grid to time, using the FPS for the application.

inputs
   fp       fValue - time, pre-grid
returns
   fp - Time with grid applied
*/
fp CScene::ApplyGrid (fp fValue)
{
   if (!m_dwFPS)
      return fValue; // no change

   fp fFPSInv, fFloor;
   fFPSInv = 1.0 / (fp) m_dwFPS;

   fValue += fFPSInv/2;
   fFloor = floor(fValue);
   fValue -= fFloor;
   fValue = floor(fValue * (fp) m_dwFPS);

   return fFloor + fValue * fFPSInv;
}


/**********************************************************************************
CScene::DurationGet - Returns the duration of the scene in seconds.
*/
fp CScene::DurationGet (void)
{
   return m_fDuration;
}

/**********************************************************************************
CScene::DurationSet - Sets the duration of the scene. In some ways this isn't
too relevent (except when the user clicks on "animate the entire scene") because the
duration doesn't affect anything in-between.
*/
void CScene::DurationSet (fp fDuration)
{
   m_fDuration = max(fDuration, 0.001);      // at least 1ms
}


/**********************************************************************************
CScene::Clone - Clones the scene and all objects within.

returns
   PCScene - New scene. Must be freed by caller
*/
CScene *CScene::Clone (void)
{
   PCScene pNew = new CScene;
   if (!pNew)
      return NULL;

   pNew->m_pSceneSet = m_pSceneSet;
   pNew->m_gGUID = m_gGUID;
   if (m_memName.p && ((PWSTR)m_memName.p)[0])
      pNew->NameSet ((PWSTR)m_memName.p);
   pNew->m_fDuration = m_fDuration;
   pNew->m_dwFPS = m_dwFPS;

   DWORD i;
   PCSceneObj *ppo;
   // can do this because defaults to no objects
   pNew->m_lPCSceneObj.Init (sizeof(PCSceneObj), m_lPCSceneObj.Get(0), m_lPCSceneObj.Num());
   ppo = (PCSceneObj*) pNew->m_lPCSceneObj.Get(0);
   for (i = 0; i < pNew->m_lPCSceneObj.Num(); i++)
      ppo[i] = ppo[i]->Clone(pNew);

   return pNew;
}


static PWSTR gpszScene = L"Scene";
static PWSTR gpszGUID = L"GUID";
static PWSTR gpszName = L"Name";
static PWSTR gpszDuration = L"Duration";
static PWSTR gpszFPS = L"FPS";

/**********************************************************************************
CScene::MMLTo - Standar MMLTo API
*/
PCMMLNode2 CScene::MMLTo (void)
{
   PCMMLNode2 pNode = new CMMLNode2;
   if (!pNode)
      return NULL;
   pNode->NameSet (gpszScene);

   MMLValueSet (pNode, gpszGUID, (PBYTE) &m_gGUID, sizeof(m_gGUID));
   if (m_memName.p && ((PWSTR)m_memName.p)[0])
      MMLValueSet (pNode, gpszName, (PWSTR)m_memName.p);
   MMLValueSet (pNode, gpszDuration, m_fDuration);
   MMLValueSet (pNode, gpszFPS, (int) m_dwFPS);

   // set all the objects
   DWORD i;
   PCSceneObj *ppo;
   ppo = (PCSceneObj*) m_lPCSceneObj.Get(0);
   for (i = 0; i < m_lPCSceneObj.Num(); i++) {
      // if the object is empty dont bother saving
      if (!ppo[i]->ObjectNum())
         continue;

      PCMMLNode2 pSub;
      pSub = ppo[i]->MMLTo ();
      // note: don't need to do name set here because MMLTo already did this
      if (pSub)
         pNode->ContentAdd (pSub);
   }

   return pNode;
}

/**********************************************************************************
CScene::MMLFrom - Standar MMLTo API
*/
BOOL CScene::MMLFrom (PCMMLNode2 pNode)
{
   DWORD i;
   PCSceneObj *ppo = (PCSceneObj*) m_lPCSceneObj.Get (0);
   for (i = 0; i < m_lPCSceneObj.Num(); i++)
      delete ppo[i];
   m_lPCSceneObj.Clear();

   if (m_memName.p)
      ((PWSTR)m_memName.p)[0] = 0;


   PWSTR psz;
   MMLValueGetBinary (pNode, gpszGUID, (PBYTE) &m_gGUID, sizeof(m_gGUID));
   psz = MMLValueGet (pNode, gpszName);
   if (psz)
      NameSet (psz);
   m_fDuration = MMLValueGetDouble (pNode, gpszDuration, 1);
   m_dwFPS = (DWORD) MMLValueGetInt (pNode, gpszFPS, 0);

   // get all the objects
   PCMMLNode2 pSub;
   for (i = 0; i < pNode->ContentNum(); i++) {
      pSub = NULL;
      pNode->ContentEnum (i, &psz, &pSub);
      if (!pSub)
         continue;
      psz = pSub->NameGet ();
      if (!psz)
         continue;

      if (!_wcsicmp(psz, gpszSceneObj)) {
         // know that it will be in sorted order
         PCSceneObj pNew = new CSceneObj;
         if (!pNew)
            continue;
         pNew->SceneSet (this);
         if (!pNew->MMLFrom (pSub)) {
            delete pNew;
            continue;
         }
         m_lPCSceneObj.Add (&pNew);
      }
   }

   return TRUE;
}


/**********************************************************************************
CScene::ObjectNum - Returns the number of PCSceneObj objects in the scene
*/
DWORD CScene::ObjectNum (void)
{
   return m_lPCSceneObj.Num();
}

/**********************************************************************************
CScene::ObjectGet - Gets a sceneobj object.

inputs
   DWORD       dwIndex - From 0 to ObjectNum()-1
returns
   PCSceneObj - Object. NULL if error
*/
PCSceneObj CScene::ObjectGet (DWORD dwIndex)
{
   PCSceneObj *ppo = (PCSceneObj*) m_lPCSceneObj.Get(dwIndex);
   if (!ppo)
      return NULL;
   return ppo[0];
}


/**********************************************************************************
CScene::ObjectRemove - Remove an object based on its index.

inputs
   DWORD       dwIndex - from 0 to ObjectNum()-1
returns
   BOOL - TRUE if success
*/
BOOL CScene::ObjectRemove (DWORD dwIndex)
{
   PCSceneObj *ppo = (PCSceneObj*) m_lPCSceneObj.Get(dwIndex);
   if (!ppo)
      return FALSE;
   delete ppo[0];
   m_lPCSceneObj.Remove (dwIndex);
   return TRUE;
}


/**********************************************************************************
CScene::ObjectFind - Given a GUID for the object (from PCObjectSocket->GUIDGet()), this
finds the index into the list of objects.

inputs
   GUID        *pgObjectWorld - Object guid
returns
   DWORD - Index into scene's collection of objects, or -1 if cant find
*/
DWORD CScene::ObjectFind (GUID *pgObjectWorld)
{
   // know the list is sorted so do binary search
   GUID g;
   DWORD dwNum = m_lPCSceneObj.Num();
   PCSceneObj *ppo = (PCSceneObj*) m_lPCSceneObj.Get(0);
   if (!dwNum)
      return -1;  // no objects

   DWORD dwCur, dwIndex, dwTry;
   for (dwCur = 1; dwCur < dwNum; dwCur *= 2);
   for (dwIndex = 0; dwCur; dwCur /= 2) {
      dwTry = dwIndex + dwCur;
      if (dwTry >= dwNum)
         continue;

      // compare the guids
      int iRet;
      ppo[dwTry]->GUIDGet (&g);
      iRet = memcmp (pgObjectWorld, &g, sizeof(g));
      if (!iRet)
         return dwTry;
      else if (iRet > 0)
         dwIndex = dwTry;
   }
   ppo[dwIndex]->GUIDGet (&g);
   if (!memcmp (pgObjectWorld, &g, sizeof(g)))
      return dwIndex;

   return -1;  // cant find
}

/**********************************************************************************
CScene::ObjectAdd - Adds a new object to the list of objects being edited
by the scene.

inputs
   GUID        *pgObjectWorld - Object's GUID from PCObjectSocket->GUIDGet()
returns
   DWORD - Index into the new object. -1 if error (such as object already exists)
*/
DWORD CScene::ObjectAdd (GUID *pgObjectWorld)
{
   // if already exist then fail
   if (ObjectFind (pgObjectWorld) != -1)
      return -1;

   // create
   PCSceneObj pNew;
   pNew = new CSceneObj;
   if (!pNew)
      return -1;
   pNew->SceneSet (this);
   pNew->GUIDSet (pgObjectWorld);

   // add it to the list. being lazy and just walking through to find insertion point
   DWORD i;
   PCSceneObj *ppo;
   GUID g;
   ppo = (PCSceneObj*) m_lPCSceneObj.Get(0);
   for (i = 0; i < m_lPCSceneObj.Num(); i++) {
      ppo[i]->GUIDGet (&g);
      if (memcmp(pgObjectWorld, &g, sizeof(g)) > 0)
         continue;   // go further on

      // else, match
      m_lPCSceneObj.Insert (i, &pNew);
      return i;
   }

   // else add
   i = m_lPCSceneObj.Num();
   m_lPCSceneObj.Add (&pNew);
   return i;
}


/**********************************************************************************
CScene::ObjectGet - Gets an object based on the GUID.

inputs
   GUID        *pgObjectWorld - GUID from CObjectSocket::GUIDGet()
   BOOL        fCreateIfNotExist - If the object isn't part of the scene and this
                  flag is TRUE, then automagically create and add it.
returns
   PCSceneObj - SceneObj. Or NULL if cant find
*/
PCSceneObj CScene::ObjectGet (GUID *pgObjectWorld, BOOL fCreateIfNotExist)
{
   DWORD dwIndex = ObjectFind (pgObjectWorld);
   PCSceneObj *ppo;
   if (dwIndex != -1) {
      ppo = (PCSceneObj*) m_lPCSceneObj.Get(dwIndex);
      return ppo ? ppo[0] : NULL;
   }

   // else, cant find
   if (!fCreateIfNotExist)
      return FALSE;

   // add it
   dwIndex = ObjectAdd (pgObjectWorld);
   if (dwIndex == -1)
      return NULL;

   ppo = (PCSceneObj*) m_lPCSceneObj.Get(dwIndex);
   return ppo ? ppo[0] : NULL;
}


/**********************************************************************************
CScene::SyncFromWorld - Called by the SceneSet() when it gets SyncFromWorld called.

What this does:
   1) For each object (PCObjectSocket) that has changed...
   2) If the object already exists in CScene then pass up SyncFromWorld()
   3) If it doesn't exist:
      a) Get the object's attributes
      b) Get all the default attributes from m_pSceneSet->DefaultXXX
      c) If any default attributes fail to match the object's attributes then
         automagically create a new CSceneObj for it, and call SyncFromWorld()

inputs
   DWORD       dwNum - Number of entries in pagObjects
   GUID        *pagObjects - Pointer to a list of GUIDS that are object IDs (from
               CObjectSocket::GUIDGet()). If this is NULL, then use all objects
               from m_pSceneSet.WorldGet().
returns
   none
*/
void CScene::SyncFromWorld (DWORD dwNum, GUID *pagObjects)
{
   PCWorld pWorld = WorldGet();
   PCSceneSet pSceneSet = SceneSetGet();
   if (!pWorld || !pSceneSet)
      return;

   if (!pagObjects)
      dwNum = pWorld->ObjectNum();
   DWORD dw;
   PCSceneObj pSceneObj;
   GUID g;
   CListFixed lAV;
   OSINFO pInfo;
   lAV.Init (sizeof(ATTRIBVAL));
   for (dw = 0; dw < dwNum; dw++) {
      PCObjectSocket pos;
      if (pagObjects)
         pos = pWorld->ObjectGet(pWorld->ObjectFind(&pagObjects[dw]));
      else
         pos = pWorld->ObjectGet(dw);
      if (!pos)
         continue;

      // Dont want to sync to the automatically generated cameras

      pos->InfoGet (&pInfo);
      if (IsEqualGUID (pInfo.gCode, CLSID_Camera))
         continue;

      // get the GUID
      pos->GUIDGet (&g);

      // see if it has a sceneobj - if so, call into that
      pSceneObj = ObjectGet (&g);
      if (pSceneObj) {
         pSceneObj->SyncFromWorld ();
         continue;
      }

      // else, see if anything has changed
      lAV.Clear();
      pos->AttribGetAll (&lAV);

      // note if dirty
      DWORD i;
      PATTRIBVAL pav;
      fp fVal;
      pav = (PATTRIBVAL) lAV.Get(0);
      for (i = 0; i < lAV.Num(); i++, pav++) {
         if (!m_pSceneSet->DefaultAttribGet (&g, pav->szName, &fVal, TRUE))
            continue;   // shouldnt happen

         if (fabs(fVal - pav->fValue) < CLOSE)
            continue;   // no change, so ignore

         // else, different
         break;
      }  // i

      // if any are different then just create a sceneobj and have that
      // deal with it
      if (i >= lAV.Num())
         continue;   // no different
      pSceneObj = ObjectGet (&g, TRUE);
      if (!pSceneObj)
         continue;
      pSceneObj->SyncFromWorld ();
   } // dw
}

/**********************************************************************************
CScene::SyncToWorld - Called by the SceneSet() when it gets SyncToWorld called.

What this does:
   1) For each object (PCObjectSocket) that needs to be changed...
   2) If the object already exists in CScene then pass up SyncToWorld()
   3) If it doesn't exist:
      a) Get the object's attributes
      b) Get all the default attributes from m_pSceneSet->DefaultXXX
      c) If any default attributes fail to match the object's attributes then
         set the object's attributes with the defaults.

inputs
   DWORD       dwNum - Number of entries in pagObjects
   GUID        *pagObjects - Pointer to a list of GUIDS that are object IDs (from
               CObjectSocket::GUIDGet()). If this is NULL, then use all objects
               from m_pSceneSet.WorldGet().
returns
   none
*/
void CScene::SyncToWorld (DWORD dwNum, GUID *pagObjects)
{
   // get the world
   PCWorld pWorld = WorldGet ();
   if (!pWorld || !m_pSceneSet)
      return;

   // go through all the objects in the world
   DWORD i;
   if (!pagObjects)
      dwNum = pWorld->ObjectNum();

   PCObjectSocket pos;
   GUID g;
   PCSceneObj pSceneObj;
   CListFixed lAttribCur;
   lAttribCur.Init (sizeof(ATTRIBVAL));
   for (i = 0; i < dwNum; i++) {
      if (pagObjects)
         pos = pWorld->ObjectGet(pWorld->ObjectFind(&pagObjects[i]));
      else
         pos = pWorld->ObjectGet(i);
      if (!pos)
         continue;

      // BUGFIX - If it's a view camera (not animation camera) then ignore
      OSINFO osi;
      pos->InfoGet (&osi);
      if (IsEqualGUID (osi.gCode, CLSID_Camera))
         continue;   // dont set this

      // get the guid
      pos->GUIDGet (&g);

      // see if it exists in database
      pSceneObj = ObjectGet (&g);
      if (pSceneObj) {
         // just use this object to fill in bits
         pSceneObj->SyncToWorld ();
         continue;
      }

      // else, see if there are any overrrides
      if (!m_pSceneSet->DefaultAttribExist (&g))
         continue;   // no defaults for this so dont change anything

      // else, defaults...
      // get all the attributes for the object
      lAttribCur.Clear();
      pos->AttribGetAll (&lAttribCur);

      // loop through these and update
      DWORD j;
      fp fVal;
      for (j = lAttribCur.Num()-1; j < lAttribCur.Num(); j--) {
         PATTRIBVAL pv = (PATTRIBVAL) lAttribCur.Get(j);

         // else, see if there's an attribute default in scene set
         if (!m_pSceneSet->DefaultAttribGet (&g, pv->szName, &fVal, FALSE))
            fVal = pv->fValue;

         // if no change remove
         if (fabs(fVal - pv->fValue) < CLOSE) {
            lAttribCur.Remove (j);
            continue;
         }
         
         // else, change
         pv->fValue = fVal;
      }

      // what's left is a list of attributes that have changed, so set them
      if (lAttribCur.Num())
         pos->AttribSetGroup (lAttribCur.Num(), (PATTRIBVAL) lAttribCur.Get(0));
   } // i

}


