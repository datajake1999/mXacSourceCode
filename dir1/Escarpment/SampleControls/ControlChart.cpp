/***********************************************************************
ControlChart.cpp - Code for a control

begun OCt-12-2000 by Mike Rozak
Copyright 2000 mike Rozak. All rights reserved
*/

//#include "mymalloc.h"
#define MYMALLOC(x)     malloc(x)
#define MYREALLOC(x,y)  realloc(x,y)
#define MYFREE(x)      free(x)

#include <windows.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include "escarpment.h"
#include "resleak.h"


extern HINSTANCE ghInstance;
#define     AUTOAXIS       -23.4525634     // if axis is this # then automatically calculate min/max

typedef struct {
   PWSTR          pszName;    // name of the dataset
   PWSTR          pszLink;    // link if click
   COLORREF       color;      // color
   double         fLine;      // line width. 0 = one pixel. 1.0 = normal
   double         fSphere;    // size of sphere. 0 = none, 1.0 = normal
   PCListFixed    pData;      // datapoints. Array of DATAPOINTINFO
} DATASETINFO, *PDATASETINFO;

typedef struct {
   double         fX;
   double         fY;
   PWSTR          pszLink;    // so can jump right to source
} DATAPOINTINFO, *PDATAPOINTINFO;

typedef struct {
   PWSTR       pszScrollRot[3];    // control to rotate about XYZ
   PWSTR       pszScrollDistance;
   PWSTR       pszHRef;          // href for entire image
   double      fMinDistance;     // positive
   double      fMaxDistance;     // positive
   COLORREF       cBorder;
   int            iBorderSize;
   PWSTR       pszStyle;         // "line", "bar", or "pie"
   PCListFixed pListData;        // list extracted
   BOOL        fExtracted; // if TRUE then list already extracted
   DWORD       dwStyle; // 0 for line, 1 for bar, 2 for pie
   DWORD       dwMaxPoints;   // max # points found in datasets
   PWSTR       apszAxisNames[2]; // axis names
   DWORD       adwAxisUnits[2];  // units
   double      afAxisMin[2];     // minimum setting
   double      afAxisMax[2];     // maximum setting

   PCEscControl   pScroll[3], pScrollDistance;
   BOOL        fRecalc;
   int         iDistance;
   int         iRot[3];
   DWORD       dwRotating;       // current dimension rotating
   Matrix      mRot;             // curent rotation matrix
   POINT       pOffset;          // add to page coords to get HDC coords
   PCMMLNode   pNode;            // use this as an alternative to the control's contents

   CRender     *pRender;
   HDC            hDC;           // used during rendering
   char        szTemp[64];      // for displaying labels
} CHART, *PCHART;

#define  NULLOBJECT        0x53481243  // a random number
#define  MAXPOLYVERT       40       // maximum vertices

#define  CUNIT_NONE           0     // default, floating point
#define  CUNIT_INTEGER        1     // only display integer values
#define  CUNIT_DOLLARS        2     // dollars
#define  CUNIT_YEARMONTH      3     // each 1.0 = year number. 1/12 = month. Won't display any more accurately than that
#define  CUNIT_YEARMONTHDAY   4     // like years, except that each month divided into 28-31 days (depending on days/month)
#define  CUNIT_TIME           5     // 1.0 = hour number. Divided into 60th's for minutes


/********************************************************************************
UnitToString - Given a number, this converts it to a string.

inputs
   double   fVal - value
   DWORD    dwUnit - Unit. CUNIT_XXX
   PWSTR    psz - Filled in.
returns
   PWSTR - psz. Unless it's not a valid value (such as 1.5 passed into integer.), then
      it returns NULL
*/
#define  EPSILON  0.00001
PSTR UnitToString (double fVal, DWORD dwUnit, PSTR psz)
{
   switch (dwUnit) {
   case CUNIT_INTEGER:
      sprintf (psz, "%d", (int) fVal);
      if ((floor(fVal+EPSILON) - fVal) > 2*EPSILON)
         return NULL;
      break;

   case CUNIT_DOLLARS:
      sprintf (psz, "$%.2f", fVal);
      break;

   case CUNIT_YEARMONTH:
   case CUNIT_YEARMONTHDAY:
      {
         // figure this out
         int   iYear, iMonth, iDay, iDaysPerMonth;
         iYear = (int) fVal;
         fVal = (fVal - iYear) * 12;
         iMonth = (int) fVal;
         if (dwUnit == CUNIT_YEARMONTHDAY)
            iDaysPerMonth = EscDaysInMonth (iMonth+1, iYear, NULL);
         else
            iDaysPerMonth = 31;
         fVal = (fVal - iMonth) * iDaysPerMonth;
         iDay = (int) fVal + 1;

         PSTR aszMonth[12] = {"Jan","Feb","Mar","Apr","May","Jun",
            "Jul","Aug","Sep","Oct","Nov","Dec"};
         if (dwUnit == CUNIT_YEARMONTHDAY)
            sprintf (psz, "%d-%s-%d", iDay, aszMonth[iMonth%12], iYear);
         else
            sprintf (psz, "%s-%d", aszMonth[iMonth%12], iYear);
      }

      break;

   case CUNIT_TIME:
      {
         int   iHour, iMinute;
         iHour = (int) fVal;
         iMinute = (int) ((fVal - iHour) * 60.0);
         sprintf (psz, "%d:%d%d", iHour, iMinute/10, iMinute%10);
      }
      break;

   default: // CUNIT_NONE
      sprintf (psz, "%g", fVal);
      break;
   }

   return psz;
}


/********************************************************************************
MyAxisCallback - Callback for the axis
*/
char *MyAxisCallback (PVOID pUser, DWORD dwAxis, double fValue)
{
   PCHART pc = (PCHART) pUser;

   // if it's an XY axis the get the conversion from the chart
   if (dwAxis < 2) {
      // if it's UNITS_NONE then use default handler
      if (pc->adwAxisUnits[dwAxis] == CUNIT_NONE)
         return NULL;

      PSTR psz;
      psz = UnitToString (fValue, pc->adwAxisUnits[dwAxis], pc->szTemp);
      if (!psz)
         pc->szTemp[0] = 0;   // blank it out because not correct multiple

      return pc->szTemp;
   }

   // if it's the Z axis, then look up the name
   if (dwAxis == 2) {
      // clear out szTemp just in case
      pc->szTemp[0]  = 0;

      // if it's not an integer multiple then blank
      if ((floor(fValue) != fValue) || (fValue < 0))
         return pc->szTemp;

      PDATASETINFO pi = NULL;
      if (pc->pListData)
         pi = (PDATASETINFO) pc->pListData->Get((DWORD) fValue);
      if (!pi)
         return pc->szTemp;   // cant get

      // if there's no string then blank
      if (!pi->pszName)
         return pc->szTemp;

      // else convert
      WideCharToMultiByte (CP_ACP, 0, pi->pszName, -1, pc->szTemp, 64, 0, 0);
      return pc->szTemp;

   }

   return NULL;   // shouldn't get here
}

/********************************************************************************
Rotated - This should be called if any of the x,y,z rotation scroll bars
   are changed

inputs
   int    x,y,z - new rotation values. 0 = no change
   BOOL     fForce - if TRUE, force to recaululate
*/
void Rotated (PCEscControl pControl, PCHART pc, int x, int y, int z, BOOL fForce = FALSE)
{
   DWORD dwLast;
   dwLast = pc->dwRotating;

   // figure which has changed
   DWORD dwChanged;
   int dwVal;
   if (x != pc->iRot[0]) {
      dwChanged = 0;
      dwVal = x;
   }
   else if (y != pc->iRot[1]) {
      dwChanged = 1;
      dwVal = y;
   }
   else if (z != pc->iRot[2]) {
      dwChanged = 2;
      dwVal = z;
   }
   else {
      dwChanged = (pc->dwRotating + 1) % 3;
      if (!fForce)
         return;     // nothing has changed
   }

   // has the dimension changed?
   if (dwChanged != pc->dwRotating) {
      // pre-rotate by old m_adwCurRot[m_dwRotating] value
      Matrix   r, r2;
      DWORD x, y;
      for (x = 0; x < 4; x++) for (y = 0; y < 4; y++)
         r[x][y] = (x == y) ? 1.0 : 0.0;
      double   fAngle;
      fAngle = (double) pc->iRot[pc->dwRotating] / 180.0 * PI;
      RotationMatrix (r, (pc->dwRotating == 0) ? fAngle : 0,
         (pc->dwRotating == 1) ? fAngle : 0, (pc->dwRotating == 2) ? fAngle : 0);
      MultiplyMatrices (r, pc->mRot, r2);

      // should make sure matrix is still 1.0 and square
      MakeMatrixSquare (r2);

      // store away
      memcpy (pc->mRot, r2, sizeof(pc->mRot));

      // update the sliders. Old one to 0. New one to new value
      if (pc->pScroll[dwLast])
         pc->pScroll[dwLast]->AttribSet (L"pos", L"0");
      // don't need to update the one just changed since it's obviously been moved
   }

   // set everything to 180 except the new changed
   pc->iRot[0] = pc->iRot[1] = pc->iRot[2] = 0;

   if (fForce)
      return;  // don't actually want to change anything

   pc->dwRotating = dwChanged;
   pc->iRot[dwChanged] = dwVal;

}


/***********************************************************************
FreeDataSet - Free memory occupied by data set

inputs
   CHART           *pc - pointer to chart
*/
void FreeDataSet (CHART *pc)
{
   if (!pc->pListData)
      return;

   // free up memory
   DWORD i;
   for (i = 0; i < pc->pListData->Num(); i++) {
      PDATASETINFO pi = (PDATASETINFO) pc->pListData->Get(i);
      if (pi->pData)
         delete pi->pData;
   }
   delete pc->pListData;
   pc->pListData = NULL;
}

/***********************************************************************
ExtractDataSet - Extract the dataset information from the control
so lang as pc->fExtracted = FALSE.

inputs
   PCEscControl   pControl - Control
   CHART           *pc - pointer to chart
*/
void ExtractDataSet (PCEscControl pControl, CHART *pc)
{
   if (pc->fExtracted && pc->pListData)
      return;  // already done

   // free the data
   DWORD i, j;
   FreeDataSet (pc);
   pc->fExtracted = TRUE;
   for (i = 0; i < 2; i++) {
      pc->apszAxisNames[i] = NULL;
      pc->afAxisMax[i] = AUTOAXIS;
      pc->afAxisMin[i] = AUTOAXIS;
      pc->adwAxisUnits[i] = CUNIT_NONE;
   }

   // extract the contents
   PCMMLNode   pNode;
   pc->dwMaxPoints = 0;
   pc->pListData = new CListFixed;
   if (!pc->pListData)
      return;
   pc->pListData->Init(sizeof(DATASETINFO));
   pNode = pc->pNode ? pc->pNode : pControl->m_pNode;
   if (pNode) for (i = 0; i < pNode->ContentNum(); i++) {
      // get the node name
      PCMMLNode   pSub;
      PWSTR       psz;
      pSub = NULL;
      pNode->ContentEnum (i, &psz, &pSub);
      if (!pSub)
         continue;
      PWSTR pszName;
      pszName = pSub->NameGet();
      if (!pszName)
         continue;

      if (!wcsicmp(pszName, L"Dataset")) {
         DATASETINFO info;
         memset (&info, 0, sizeof(info));
         info.pData = new CListFixed;
         if (!info.pData)
            continue;   // error
         info.pData->Init(sizeof(DATAPOINTINFO));
         info.pszName = pSub->AttribGet (L"Name");
         info.pszLink = pSub->AttribGet (L"href");
         if (!AttribToDouble (pSub->AttribGet(L"LineSize"), &info.fLine))
            info.fLine = 1.0;
         if (!AttribToDouble (pSub->AttribGet(L"SphereSize"), &info.fSphere))
            info.fSphere = 1.0;
         
         // if no color, automatically generate one
         COLORREF acrDefault[] = {
            RGB(0xff,0x80,0x80),
            RGB(0x80,0xff,0x80),
            RGB(0x80,0x80,0xff),
            RGB(0xff,0xff,0x00),
            RGB(0x00,0xff,0xff),
            RGB(0xff,0x00,0xff),
            RGB(0xff,0x00,0x00),
            RGB(0x00,0xff,0x00),
            RGB(0x00,0x00,0xff)
         };
         DWORD dwNumDef;
         dwNumDef = sizeof(acrDefault) / sizeof(COLORREF);

         // get the color
         if (!AttribToColor (pSub->AttribGet(L"Color"), &info.color))
            info.color = acrDefault[pc->pListData->Num() % dwNumDef];
            

         // look through this and get all the data points
         for (j = 0; j < pSub->ContentNum(); j++) {
            PCMMLNode   pData;
            pData = NULL;
            pSub->ContentEnum (j, &psz, &pData);
            if (!pData)
               continue;
            PWSTR pszName;
            pszName = pData->NameGet();
            if (!pszName || wcsicmp(pszName, L"Point"))
               continue;

            // get the X and Y attributes
            DATAPOINTINFO point;
            memset (&point, 0, sizeof(point));
            AttribToDouble (pData->AttribGet(L"X"), &point.fX);
            AttribToDouble (pData->AttribGet(L"Y"), &point.fY);
            point.pszLink = pData->AttribGet (L"href");

            info.pData->Add (&point);
         }

         // store the max # points away
         pc->dwMaxPoints = max(pc->dwMaxPoints, info.pData->Num());

         pc->pListData->Add (&info);
      }
      else if (!wcsicmp(pszName, L"Axis")) {
         DWORD dwAxis;
         PWSTR pszAxis = pSub->AttribGet(L"Axis");
         if (!pszAxis)
            continue;   // need X or Y
         if (!wcsicmp(pszAxis, L"X"))
            dwAxis = 0;
         else if (!wcsicmp(pszAxis, L"Y"))
            dwAxis = 1;
         else
            continue;   // need X or Y

         // extract axis names while at it
         pc->apszAxisNames[dwAxis] = pSub->AttribGet (L"Label");

         // extract axis min/max
         if (!AttribToDouble (pSub->AttribGet (L"Min"), &pc->afAxisMin[dwAxis]))
            pc->afAxisMin[dwAxis] = AUTOAXIS;
         if (!AttribToDouble (pSub->AttribGet (L"Max"), &pc->afAxisMax[dwAxis]))
            pc->afAxisMax[dwAxis] = AUTOAXIS;

         // extract axis units
         PWSTR pszUnit;
         pszUnit = pSub->AttribGet (L"Units");
         if (pszUnit && !wcsicmp(pszUnit, L"integer"))
            pc->adwAxisUnits[dwAxis] = CUNIT_INTEGER;
         else if (pszUnit && !wcsicmp(pszUnit, L"dollars"))
            pc->adwAxisUnits[dwAxis] = CUNIT_DOLLARS;
         else if (pszUnit && !wcsicmp(pszUnit, L"yearmonth"))
            pc->adwAxisUnits[dwAxis] = CUNIT_YEARMONTH;
         else if (pszUnit && !wcsicmp(pszUnit, L"yearmonthday"))
            pc->adwAxisUnits[dwAxis] = CUNIT_YEARMONTHDAY;
         else if (pszUnit && !wcsicmp(pszUnit, L"time"))
            pc->adwAxisUnits[dwAxis] = CUNIT_TIME;
      }
   }

   // what kind of chart is it
   // DWORD dwStyle; // 0 for line, 1 for bar, 2 for pie
   if (pc->pszStyle && !wcsicmp(pc->pszStyle, L"bar"))
      pc->dwStyle = 1;
   else if (pc->pszStyle && !wcsicmp(pc->pszStyle, L"pie"))
      pc->dwStyle = 2;
   else
      pc->dwStyle = 0;
}


/***********************************************************************
ChartPaint - Paint the chart into the render object.

inputs
   PCEscControl   pControl - Control
   CHART          *pc - pointer to chart
*/
void ChartPaint (PCEscControl pControl, CHART *pc)
{
   // determine the size of the axis
   pnt pAxis;
   DWORD i, j;
   pAxis[0] = 8;
   if (pControl->m_rPosn.right == pControl->m_rPosn.left)
      return;  // zero height
   pAxis[1] = pAxis[0] * (pControl->m_rPosn.bottom - pControl->m_rPosn.top) /
      (double) (pControl->m_rPosn.right - pControl->m_rPosn.left);
   pAxis[2] = 0;

   ExtractDataSet (pControl, pc);

   // set the font
   LOGFONT  lf;
   memset (&lf, 0, sizeof(lf));
   lf.lfHeight = -MulDiv(pControl->m_fi.iPointSize, EscGetDeviceCaps(pc->hDC, LOGPIXELSY), 72); 
   lf.lfCharSet = DEFAULT_CHARSET;
   lf.lfPitchAndFamily = FF_SWISS | VARIABLE_PITCH;
   lf.lfWeight = FW_NORMAL;   // BUGFIX - Adjust the weight of all fonts to normal
   WideCharToMultiByte (CP_ACP, 0, pControl->m_fi.szFont, -1, lf.lfFaceName, sizeof(lf.lfFaceName), 0, 0);
   if (pControl->m_fi.dwFlags & FCFLAG_BOLD)
      lf.lfWeight = FW_BOLD;
   if (pControl->m_fi.dwFlags & FCFLAG_ITALIC)
      lf.lfItalic = TRUE;
   if (pControl->m_fi.dwFlags & FCFLAG_UNDERLINE)
      lf.lfUnderline = TRUE;
   if (pControl->m_fi.dwFlags & FCFLAG_STRIKEOUT)
      lf.lfStrikeOut = TRUE;
   pc->pRender->TextFontSet (&lf, fabs(lf.lfHeight) / ((pc->dwStyle == 2) ? 1.0 : 1.5), TRUE);

   // if it's a pie chart then special case the rest
   if (pc->dwStyle == 2) {
      // BUGBUG - Note in documentation that pie charts need 1+ data sets, and only
      // use the first value, Y values for size. Must be > 0
      // find the sum of the values

      // rotate a pie chart just a bit
      pc->pRender->Rotate (-PI/8, 2);
      pc->pRender->Rotate (-PI/8, 1);

      // find sum of values
      double   fSum = 0;
      for (i = 0; i < pc->pListData->Num(); i++) {
         PDATASETINFO  pi = (PDATASETINFO) pc->pListData->Get(i);
         PDATAPOINTINFO pd = (PDATAPOINTINFO) pi->pData->Get(0);
         if (pd)
            fSum += (pd->fY > 0) ? pd->fY : 0;
      }

      double   fRadius;
      fRadius = min(pAxis[0], pAxis[1]) / 2.0;

      // loop through all the slices
      double   fCurRot;
      fCurRot = 0;
      for (i = 0; i < pc->pListData->Num(); i++) {
         PDATASETINFO  pi = (PDATASETINFO) pc->pListData->Get(i);
         PDATAPOINTINFO pd = (PDATAPOINTINFO) pi->pData->Get(0);
         if (!pd || (pd->fY <=0))
            continue;

         // set the color
         pc->pRender->m_DefColor[0] = GetRValue(pi->color);
         pc->pRender->m_DefColor[1] = GetGValue(pi->color);
         pc->pRender->m_DefColor[2] = GetBValue(pi->color);

         // set object ID
         pc->pRender->m_dwMajorObjectID = ((i+1) << 16);

         double   fSize;
         fSize = pd->fY / fSum * 2 * PI;
         pnt   apSlice[100][4];
         DWORD dwNeeded;
         dwNeeded = 2 + (DWORD) (fSize / (2 * PI) * 100);
         dwNeeded = min(100,dwNeeded);

         // fill in slice
         for (j = 0; j < dwNeeded; j++) {
            apSlice[j][0][2] = apSlice[j][1][2] = fRadius/4;
            apSlice[j][2][2] = apSlice[j][3][2] = -fRadius/4;

            double   fAngle;
            fAngle = fCurRot + (j / (double)(dwNeeded-1)) * fSize;
            apSlice[j][1][0] = apSlice[j][2][0] = sin(fAngle)*fRadius;
            apSlice[j][1][1] = apSlice[j][2][1] = cos(fAngle)*fRadius;
            apSlice[j][0][0] = apSlice[j][3][0] = apSlice[j][1][0] / 1000.0;  // center
            apSlice[j][0][1] = apSlice[j][3][1] = apSlice[j][1][1] / 1000.0;  // center
         }

         // draw
         pc->pRender->MeshFromPoints(0, 4, dwNeeded, (double*)apSlice);
         pc->pRender->ShapeMeshSurface ();

         // draw text in front of it
         pc->pRender->m_dwMajorObjectID = 1;
         COLORREF crAxis;
         crAxis = pControl->m_fi.fi.cText;
         pc->pRender->m_DefColor[0] = GetRValue(crAxis);
         pc->pRender->m_DefColor[1] = GetGValue(crAxis);
         pc->pRender->m_DefColor[2] = GetBValue(crAxis);
         pnt   pText, pRight;
         pText[0] = sin(fCurRot+fSize/2) * fRadius * .9;
         pText[1] = cos(fCurRot+fSize/2) * fRadius * .9;
         pText[2] = fRadius/3;
         pRight[0] = pText[0] * 1.1;
         pRight[1] = pText[1] * 1.1;
         pRight[2] = fRadius/3;
         char  szTemp[256];
         szTemp[0] = 0;
         if (pi->pszName)
            WideCharToMultiByte (CP_ACP, 0, pi->pszName, -1, szTemp, sizeof(szTemp), 0, 0);
         pc->pRender->Text (szTemp, pText, pRight, -1);


         fCurRot += fSize;
      }

      // done
      return;
   }

   // if it's a bar chart then it's depth is max(width,height), unless
   // fewer than 5 elemns
   if (pc->dwStyle == 1) {
      pAxis[2] = (pAxis[0] + pAxis[1]) / 2;  // average of depth
      if (pc->pListData->Num() < 5)
         pAxis[2] = pAxis[2] * pc->pListData->Num() / 5.0;
   }


   // find min/max for XY axis values
   pnt   pu1,pu2;
   pu1[0] = pu1[1] = pu1[2] = 0;
   pu2[0] = pu2[1] = pu2[2] = 0;
   double fBarWidth, fLastX;
   BOOL  fFoundPoint, fFoundWidth;
   fFoundPoint = FALSE;
   fFoundWidth = FALSE;
   for (i = 0; i < pc->pListData->Num(); i++) {
      PDATASETINFO  pi = (PDATASETINFO) pc->pListData->Get(i);
      for (j = 0; j < pi->pData->Num(); j++) {
         PDATAPOINTINFO pd = (PDATAPOINTINFO) pi->pData->Get(j);
         if (fFoundPoint) {
            pu1[0] = min(pu1[0], pd->fX);
            pu2[0] = max(pu2[0], pd->fX);
            pu1[1] = min(pu1[1], pd->fY);
            pu2[1] = max(pu2[1], pd->fY);

         }
         else {
            pu1[0] = pu2[0] = pd->fX;
            pu1[1] = pu2[1] = pd->fY;
            fFoundPoint = TRUE;
         }

         // store away the smallest bar width that find
         if (j >= 1) {
            if (fFoundWidth)
               fBarWidth = min(fBarWidth, pd->fX - fLastX);
            else
               fBarWidth = pd->fX - fLastX;
            fFoundWidth = TRUE;
         };

         // remember last x
         fLastX = pd->fX;
      }
   }

   // adjust the min/max based upon preferences
   for (i = 0; i < 2; i++) {
      if (pc->afAxisMin[i] != AUTOAXIS)
         pu1[i] = pc->afAxisMin[i];
      if (pc->afAxisMax[i] != AUTOAXIS)
         pu2[i] = pc->afAxisMax[i];
   }

   // if bar chart then adjust the width by the bar width
   if (!fFoundWidth)
      fBarWidth = 1.0;
   if (pc->dwStyle == 1) {
      pu1[0] -= fBarWidth/2;
      pu2[0] += fBarWidth/2;
      pu1[2] = -.5;  // record the number of data sets
      pu2[2] = -.5 + pc->pListData->Num();

      // Rotate the entire graph depending upon how many points
      // are found and how manay data sets are found
      DWORD dwSum;
      dwSum = pc->pListData->Num() + pc->dwMaxPoints;
      if (!dwSum)
         dwSum = 1;
      pc->pRender->Rotate (sqrt(pc->pListData->Num() / (double)dwSum) * PI / 2.0, 2);
   }

   // if it's a line chart translate so more screen real-estate is used
   if (pc->dwStyle == 0) {
      pc->pRender->Translate (pAxis[0]/6, pAxis[1]/6, 0);
   }

   // from min/max determine scaling
   pnt pMidUnits, pScaleUnits;
   for (i = 0; i < 3; i++) {
      pMidUnits[i] = (pu1[i] + pu2[i])/2.0;
      pScaleUnits[i] = pu2[i] - pu1[i];
      if (pScaleUnits[i])
         pScaleUnits[i] = pAxis[i] / pScaleUnits[i];
   }

   // draw the lines
   PDATAPOINTINFO pLast;
   pnt   pDraw[2];
   for (i = 0; i < pc->pListData->Num(); i++) {
      PDATASETINFO  pi = (PDATASETINFO) pc->pListData->Get(i);
      double   fLine, fSphere;
      fLine = pi->fLine * min(pAxis[0],pAxis[1])/50.0;
      fSphere = pi->fSphere * min(pAxis[0],pAxis[1])/25.0;
      pLast = NULL;

      // set the color
      pc->pRender->m_DefColor[0] = GetRValue(pi->color);
      pc->pRender->m_DefColor[1] = GetGValue(pi->color);
      pc->pRender->m_DefColor[2] = GetBValue(pi->color);

      for (j = 0; j < pi->pData->Num(); j++) {
         PDATAPOINTINFO pd = (PDATAPOINTINFO) pi->pData->Get(j);

         // figure out where to draw
         pDraw[1][0] = (pd->fX - pMidUnits[0]) * pScaleUnits[0];
         pDraw[1][1] = (pd->fY - pMidUnits[1]) * pScaleUnits[1];

         // set object ID
         pc->pRender->m_dwMajorObjectID = ((i+1) << 16) | j;

         switch (pc->dwStyle) {
         case 0:  // line
            pDraw[1][2] = (0.0 - pMidUnits[2]) * pScaleUnits[2]; // line chart
            if (pLast) {
               // draw line
               if (fLine)
                  pc->pRender->ShapeArrow (2, (double*) pDraw, fLine, FALSE);
               else
                  pc->pRender->ShapeLine (2, (double*) pDraw);
            }

            // draw the sphere
            if (fSphere) {
               pc->pRender->MatrixPush();
               pc->pRender->Translate (pDraw[1][0], pDraw[1][1], pDraw[1][2]);
               pc->pRender->MeshSphere (fSphere);
               pc->pRender->ShapeMeshSurface ();
               pc->pRender->MatrixPop();
            }
            break;

         case 1:  // bar
            pDraw[1][1] = (pDraw[1][1] + -pAxis[1]/2) / 2;  // mid way
            pDraw[1][2] = (i - pMidUnits[2]) * pScaleUnits[2];
            pc->pRender->MatrixPush ();
            pc->pRender->Translate (pDraw[1][0], pDraw[1][1], -pDraw[1][2]);
            pc->pRender->ShapeBox (fBarWidth * pScaleUnits[0], (pd->fY - pu1[1])*pScaleUnits[1], 1*pScaleUnits[2]);
            pc->pRender->MatrixPop();
            break;

         case 2:  // pie
            // NOTE: Done elsewhere
            break;
         }

         // next
         pLast = pd;
         CopyPnt (pDraw[1], pDraw[0]);
      }
   }


   // Set chart axis color
   pc->pRender->m_dwMajorObjectID = 1;
   COLORREF crAxis;
   crAxis = pControl->m_fi.fi.cText;
   pc->pRender->m_DefColor[0] = GetRValue(crAxis);
   pc->pRender->m_DefColor[1] = GetGValue(crAxis);
   pc->pRender->m_DefColor[2] = GetBValue(crAxis);

   pnt   p1,p2;
   p1[0] = -pAxis[0]/2;
   p1[1] = -pAxis[1]/2;
   p1[2] = pAxis[2]/2;
   p2[0] = pAxis[0]/2;
   p2[1] = pAxis[1]/2;
   p2[2] = -pAxis[2]/2;
   PSTR paszAxis[3];
   char  aszAxis[2][512];
   for (i = 0; i < 2; i++) {
      aszAxis[i][0] = 'X' + (char) i;
      aszAxis[i][1] = 0;
      if (pc->apszAxisNames[i])
         WideCharToMultiByte (CP_ACP, 0, pc->apszAxisNames[i], -1,
         aszAxis[i], sizeof(aszAxis[i]), 0, 0);
   }
   paszAxis[0] = aszAxis[0];
   paszAxis[1] = aszAxis[1];
   paszAxis[2] = NULL;
   pc->pRender->m_fBackCull = FALSE;
   pc->pRender->ShapeAxis (p1, p2, pu1, pu2, paszAxis,
      ((pc->dwStyle == 0) ? 0 : AXIS_DISABLEGIRD) | AXIS_DISABLEINTERNALGIRD,
      MyAxisCallback, pc);

}


/***********************************************************************
Control callback
*/
BOOL ControlChart (PCEscControl pControl, DWORD dwMessage, PVOID pParam)
{
   CHART  *pc = (CHART*) pControl->m_mem.p;
   switch (dwMessage) {
   case ESCM_CONSTRUCTOR:
      {
         pControl->m_mem.Required (sizeof(CHART));
         pc = (CHART*) pControl->m_mem.p;
         memset (pc, 0, sizeof(CHART));
         pc->pRender = new CRender;
         pc->fMinDistance = 5;
         pc->fMaxDistance = 50;
         pc->iDistance = 50;
         pc->cBorder = 0;
         pc->iBorderSize = 2;
         pc->pszStyle = NULL;
         DWORD x, y;
         for (x = 0; x < 4; x++) for (y = 0; y < 4; y++)
            pc->mRot[x][y] = (x == y) ? 1.0 : 0.0;

         pControl->AttribListAddColor (L"bordercolor", &pc->cBorder, FALSE, TRUE);
         pControl->AttribListAddDecimal (L"border", &pc->iBorderSize, FALSE, TRUE);
         pControl->AttribListAddString (L"style", &pc->pszStyle, NULL, TRUE);
         pControl->AttribListAddString (L"ScrollRotX", &pc->pszScrollRot[0], &pc->fRecalc, TRUE);
         pControl->AttribListAddString (L"ScrollRotY", &pc->pszScrollRot[1], &pc->fRecalc, TRUE);
         pControl->AttribListAddString (L"ScrollRotZ", &pc->pszScrollRot[2], &pc->fRecalc, TRUE);
         pControl->AttribListAddString (L"ScrollDistance", &pc->pszScrollDistance, &pc->fRecalc, TRUE);
         pControl->AttribListAddDouble (L"mindistance", &pc->fMinDistance, NULL, TRUE);
         pControl->AttribListAddDouble (L"mindistance", &pc->fMaxDistance, NULL, TRUE);
         pControl->AttribListAddString (L"hRef", &pc->pszHRef, FALSE, FALSE);
      }
      return TRUE;

   case ESCM_DESTRUCTOR:
      if (pc->pRender)
         delete pc->pRender;
      if (pc->pNode)
         delete pc->pNode;
      FreeDataSet (pc);
      return TRUE;

   case ESCM_INITCONTROL:
      {
         // if this has a href then want mouse
         pControl->m_fWantMouse = TRUE;
         if (pControl->m_fWantMouse)
            pControl->m_dwWantFocus = 1;

         // secify that accept space or enter
         if (pControl->m_dwWantFocus) {
            ESCACCELERATOR a;
            memset (&a, 0, sizeof(a));
            a.c = L' ';
            a.dwMessage = ESCM_SWITCHACCEL;
            pControl->m_listAccelFocus.Add (&a);
            a.c = L'\n';
            pControl->m_listAccelFocus.Add (&a);
         }

      }
      return TRUE;

   case ESCM_MOUSEMOVE:
      {
         ESCMMOUSEMOVE *p = (ESCMMOUSEMOVE*) pParam;

         // if there's a link for the hold picture can click anywhere
         if (pc->pszHRef) {
            pControl->m_pParentPage->SetCursor (IDC_HANDCURSOR);
            return TRUE;
         }

         // set the cursor based upon whether it's over something
         // if click on render object
         if (pc->pRender) {
            DWORD dwMajor, dwMinor;
            dwMajor = 0;
            pc->pRender->ObjectGet (p->pPosn.x - pControl->m_rPosn.left,
               p->pPosn.y - pControl->m_rPosn.top, &dwMajor, &dwMinor);

            // if hit
            pControl->m_pParentPage->SetCursor (HIWORD(dwMajor) ? IDC_HANDCURSOR : IDC_NOCURSOR);
         }
      }
      return TRUE;

   case ESCM_LBUTTONDOWN:
      {
         ESCMLBUTTONDOWN *p = (ESCMLBUTTONDOWN*) pParam;
         PWSTR    pszLink = pc->pszHRef;

         // must release capture or bad things happen
         pControl->m_pParentPage->MouseCaptureRelease(pControl);

         // look up click location
         if (!pc->pRender) {
            if (pszLink)
               goto dolink;

            pControl->m_pParentPage->m_pWindow->Beep (ESCBEEP_DONTCLICK);
            return FALSE;
         }

         DWORD dwMajor, dwMinor;
         dwMajor = 0;
         pc->pRender->ObjectGet (p->pPosn.x - pControl->m_rPosn.left,
            p->pPosn.y - pControl->m_rPosn.top, &dwMajor, &dwMinor);

         if (!HIWORD(dwMajor)) {
            if (pszLink)
               goto dolink;

            pControl->m_pParentPage->m_pWindow->Beep (ESCBEEP_DONTCLICK);
            return FALSE;
         }

         // find the point
         PDATASETINFO pi;
         pi = NULL;
         PDATAPOINTINFO pd;
         pd = NULL;
         if (pc->pListData)
            pi = (PDATASETINFO) pc->pListData->Get(HIWORD(dwMajor)-1);
         if (pi && pi->pData)
            pd = (PDATAPOINTINFO) pi->pData->Get(LOWORD(dwMajor));
         if (pd && pd->pszLink)
            pszLink = pd->pszLink;
         else if (pi && pi->pszLink)
            pszLink = pi->pszLink;

         if (!pszLink) {
            pControl->m_pParentPage->m_pWindow->Beep (ESCBEEP_DONTCLICK);
            return FALSE;
         }

         pControl->m_pParentPage->m_pWindow->Beep (ESCBEEP_LINKCLICK);

dolink:
         pControl->m_pParentPage->Link (pszLink);
         return TRUE;
      }
      return TRUE;

   case ESCM_PAINT:
      {
         PESCMPAINT p = (PESCMPAINT) pParam;
         if (!pc->pRender)
            return FALSE;

         pc->pOffset.x = p->rControlHDC.left - p->rControlPage.left;
         pc->pOffset.y = p->rControlHDC.top - p->rControlPage.top;

         // if we have attributes that point to the scroll bar but dont
         // actually have the control then find
         DWORD i;
         for (i = 0; i < 3; i++)
            if (pc->pszScrollRot[i] && !pc->pScroll[i]) {
               pc->pScroll[i] = pControl->m_pParentPage->ControlFind (pc->pszScrollRot[i]);
               if (pc->pScroll[i]) {
                  WCHAR szTemp[64];
                  pc->pScroll[i]->AttribSet (L"min", L"-180");
                  pc->pScroll[i]->AttribSet (L"max", L"180");
                  pc->pScroll[i]->AttribSet (L"page", L"10");
                  swprintf (szTemp, L"%d", pc->iRot[i]);
                  pc->pScroll[i]->AttribSet (L"pos", szTemp);
                  pc->pScroll[i]->m_pParentControl = pControl;
               }
            }
         if (pc->pszScrollDistance && !pc->pScrollDistance) {
            pc->pScrollDistance = pControl->m_pParentPage->ControlFind (pc->pszScrollDistance);
            if (pc->pScrollDistance) {
               WCHAR szTemp[64];
               pc->pScrollDistance->AttribSet (L"min", L"0");
               pc->pScrollDistance->AttribSet (L"max", L"100");
               pc->pScrollDistance->AttribSet (L"page", L"5");
               swprintf (szTemp, L"%d", pc->iDistance);
               pc->pScrollDistance->AttribSet (L"pos", szTemp);
               pc->pScrollDistance->m_pParentControl = pControl;
            }
         }

         // init
         pc->hDC = p->hDC;
         pc->pRender->Init (p->hDC, &p->rControlHDC);
         pc->pRender->m_fPixelsPerFacet /= 2.0;
         pc->pRender->m_dwMaxFacets /= 2;
         pc->pRender->m_dwMajorObjectID = NULLOBJECT;

         // convert distance number from 0 to 100, to a real distance
         double   fVal;
         double   fPerUnit;
         if (pc->fMaxDistance <= pc->fMinDistance)
            pc->fMaxDistance = fabs(pc->fMinDistance) + 50;
         if (pc->fMinDistance <= 0.0)
            pc->fMinDistance = pc->fMaxDistance / 10;
         fPerUnit = pow (pc->fMaxDistance / pc->fMinDistance, 1.0/100.0);
         fVal = pc->fMinDistance * pow(fPerUnit, pc->iDistance);
         pc->pRender->Translate (0, 0, -fVal);

         // rotate by the current rotation component
         pc->pRender->Rotate (pc->iRot[pc->dwRotating] / 180.0 * PI, pc->dwRotating+1);

         // multiply by our rotation matrix
         pc->pRender->MatrixMultiply (pc->mRot);

         // paint
         ChartPaint (pControl, pc);
         // draw
         pc->pRender->Commit (p->hDC, &p->rControlHDC);


         if (pc->iBorderSize) {
            HBRUSH hbr;
            int   iBorder = (int) pc->iBorderSize;
            hbr = CreateSolidBrush (pc->cBorder);

            // left
            RECT r;
            r = p->rControlHDC;
            r.right = r.left + iBorder;
            FillRect (p->hDC, &r, hbr);

            // right
            r = p->rControlHDC;
            r.left = r.right - iBorder;
            FillRect (p->hDC, &r, hbr);

            // top
            r = p->rControlHDC;
            r.bottom = r.top + iBorder;
            FillRect (p->hDC, &r, hbr);

            // bottom
            r = p->rControlHDC;
            r.top = r.bottom - iBorder;
            FillRect (p->hDC, &r, hbr);

            DeleteObject (hbr);
         }
      }
      return TRUE;

   case ESCM_MOUSEHOVER:
      {
         PESCMMOUSEHOVER p = (PESCMMOUSEHOVER) pParam;

         // if a button is down dont do
         if (pControl->m_fLButtonDown || pControl->m_fMButtonDown || pControl->m_fRButtonDown)
            return TRUE;

         // set the cursor based upon whether it's over something
         // if click on render object
         if (!pc->pRender)
            return FALSE;

         DWORD dwMajor, dwMinor;
         dwMajor = 0;
         pc->pRender->ObjectGet (p->pPosn.x - pControl->m_rPosn.left,
            p->pPosn.y - pControl->m_rPosn.top, &dwMajor, &dwMinor);

         if (!HIWORD(dwMajor))
            return FALSE;

         // find the point
         PDATASETINFO pi = NULL;
         PDATAPOINTINFO pd = NULL;
         if (pc->pListData)
            pi = (PDATASETINFO) pc->pListData->Get(HIWORD(dwMajor)-1);
         if (pi && pi->pData)
            pd = (PDATAPOINTINFO) pi->pData->Get(LOWORD(dwMajor));


         // convert the string
         WCHAR szHuge[5000];
         DWORD dwNeeded;
         BOOL  fLineAlready;
         fLineAlready = FALSE;
         wcscpy (szHuge, L"<small><colorblend posn=background lcolor=#f0ffe0 rcolor=#e0f0c0/>");
         // show the x value
         if ((pc->dwStyle != 2) && pd) {
            if (fLineAlready)
               wcscat (szHuge, L"<br/>");
            fLineAlready = TRUE;

            if (pc->apszAxisNames[0]) {
               StringToMMLString (pc->apszAxisNames[0], szHuge + wcslen(szHuge), 1000, &dwNeeded);
               wcscat (szHuge, L": ");
            }
            wcscat (szHuge, L"<bold>");
            char  szTemp[128];
            UnitToString (pd->fX, pc->adwAxisUnits[0], szTemp);
            MultiByteToWideChar (CP_ACP, 0, szTemp, -1, szHuge + wcslen(szHuge), 1000);
            wcscat (szHuge, L"</bold>");
         }

         // show the y value
         if (pd) {
            if (fLineAlready)
               wcscat (szHuge, L"<br/>");
            fLineAlready = TRUE;

            if (pc->apszAxisNames[1]) {
               StringToMMLString (pc->apszAxisNames[1], szHuge + wcslen(szHuge), 1000, &dwNeeded);
               wcscat (szHuge, L": ");
            }
            wcscat (szHuge, L"<bold>");
            char  szTemp[128];
            UnitToString (pd->fY, pc->adwAxisUnits[1], szTemp);
            MultiByteToWideChar (CP_ACP, 0, szTemp, -1, szHuge + wcslen(szHuge), 1000);
            wcscat (szHuge, L"</bold>");
         }

         // show the data set
         if (pi && pi->pszName) {
            if (fLineAlready)
               wcscat (szHuge, L"<br/>");
            fLineAlready = TRUE;

            StringToMMLString (pi->pszName, szHuge + wcslen(szHuge), 1000, &dwNeeded);
         }

         wcscat (szHuge, L"</small>");

         PCMMLNode pSub;
         CEscError err;
         pSub = ParseMML (szHuge, ghInstance, NULL, NULL, &err);
         if (!pSub)
            return FALSE;
         pSub->AttribSet(L"HResize", L"yes");

         POINT pt, pt2;
         if (p) {
            // mouse
            pt2 = p->pPosn;
            pt2.y += 16;   // so it's below the icon
         }
         else {
            // use bottom of control
            pt2.x = pControl->m_rPosn.left;
            pt2.y = pControl->m_rPosn.bottom;
         }
         pControl->CoordPageToScreen (&pt2, &pt);

         // else
         pControl->m_pParentPage->m_pWindow->HoverHelp (pSub, 0, &pt);
         delete pSub;
      }
      return TRUE;


   case ESCN_SCROLL:
   case ESCN_SCROLLING:
      {
         ESCNSCROLL  *p = (ESCNSCROLL*) pParam;
         DWORD i;
         for (i = 0; i < 3; i++)
            if (p->pControl == pc->pScroll[i]) {
               int   r;
               r = p->iPos;
               if (i == 2)
                  r = -r;  // flip z

               // found it. it's 3D
               Rotated (pControl, pc,
                  (i == 0) ? r : pc->iRot[0],
                  (i == 1) ? r : pc->iRot[1],
                  (i == 2) ? r : pc->iRot[2]);

               // pc->iRot[i] = p->iPos;
               pControl->Invalidate ();
               return TRUE;
            }
         if (p->pControl == pc->pScrollDistance) {
            // the distance has changed
            pc->iDistance = p->iPos;
            pControl->Invalidate();
            return TRUE;
         }
      }
      return TRUE;

   case ESCM_CHARTDATA:
      {
         ESCMCHARTDATA *p = (ESCMCHARTDATA*) pParam;

         PCMMLNode   pNode;
         if (p->pNode) {
            pNode = p->pNode->Clone();
         }
         else
            pNode = NULL;
         if (!pNode && p->pszMML) {
            CEscError   err;
            pNode = ParseMML (p->pszMML, pControl->m_hInstance, NULL, NULL, &err);
         }

         if (!pNode)
            return FALSE;

         // delete the old one
         if (pc->pNode)
            delete pc->pNode;
         pc->pNode = pNode;
         pControl->Invalidate();
         pc->fExtracted = FALSE;
      }
      return TRUE;

   }

   return FALSE;
}


// BUGBUG - 2.0 - Allow to link to a status-text control that will be filled with
// the chart's color-key for data sets
