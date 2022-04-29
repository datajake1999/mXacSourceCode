/************************************************************************
Util.cpp - Utility functions

begun 16/9/01 by Mike Rozak
Copyright 2001 Mike Rozak. All rights reserved
*/

#include "mymalloc.h"
#include <windows.h>
#include <crtdbg.h>
#define  COMPILE_MULTIMON_STUBS     // so that works win win95
#include <multimon.h>
#include "escarpment.h"
#include "..\M3D.h"
#include "resource.h"

DWORD gdwToday = Today();
PWSTR gpszAttrib = L"Attrib";
PWSTR gszBack = L"back";
PWSTR gszChecked = L"checked";
PWSTR gszPos = L"pos";
PWSTR gszColor = L"color";
PWSTR gszText = L"text";
PWSTR gszCurSel = L"cursel";
PWSTR gpszASP = APPLONGNAMEW;
PSTR        gpszRegBase = "Software\\mXac\\" APPLONGNAME;
PWSTR       gpszwRegBase = L"Software\\mXac\\" APPLONGNAMEW;
PWSTR gpszWSRoofing = L"Roofing";   // 3 asphalt, 1 wood shingles, 2 tiles, 0 corrogated iron, 4 thatching
PWSTR gpszWSExternalWalls = L"ExternalWalls";   // 0 corrogates, 1 brick veneer, 2 clapboards, 3 stucco, 4 brick, 5 stone, 6 cement block, 7 hay bale, 8 logs
PCEscWindow gpBeepWindow = NULL;        // used to play beeps
static BOOL gfDynamic = FALSE;         // if use dynamic settings
static BOOL gfVistaThreadPriorityHack = FALSE;

static PWSTR gszPercentD = L"%d";
static PWSTR gszDay = L"day";
static PWSTR gszMonth = L"month";
static PWSTR gszYear = L"year";
static PWSTR gszHour = L"hour";
static PWSTR gszMinute = L"minute";
PWSTR gszRedoSamePage = L"RedoSamePage";
PWSTR gpszWSDate = L"Date";   // date that showing
PWSTR gpszWSTime = L"Time";   // time that showing
PWSTR gpszWSLatitude = L"Latitude";   // of the building site
PWSTR gpszWSTrueNorth = L"TrueNorth";   // # of radins (clockwise) that true north is.
PWSTR gpszWSClimate = L"Climate";   // 0 for tropical, 1 sub-tropical, 2 temperate, 3 alpine, 5 arid, 6 mediterranean, 7 tundra
PWSTR gpszWSFoundation = L"Foundation";   // 1 for pier, 2 for pad, 3 for perimeter, 4 for basement


PWSTR gpszWSObjLocation = L"ObjLocation"; // 0 for ground (default), 1 for wall, 2 for ceiling
PWSTR gpszWSObjShowPlain = L"ObjShowPlane";   // 0 for no, 1 for yes (default)
PWSTR gpszWSObjType = L"ObjType";   // object type - RENDERSHOW_XYZ. Default RENDERSHOW_FURNITURE
PWSTR gpszWSObjEmbed = L"ObjEmbed"; // object embedding - 0 for none, 1 for just stick, 2 for one layers, 3 for two layers

CListFixed    gListXMONITORINFO;       // list of monitors on the system
PCWorldSocket gapWorld[MAXRENDERSHARDS] = {NULL, NULL, NULL, NULL}; // can be set to some objects know which world to use
PCSceneSet    gapScene[MAXRENDERSHARDS] = {NULL, NULL, NULL, NULL}; // can be set by worldset
PWSTR gszObjectSurface = L"ObjectSurface";

// from library
PWSTR gpszCatMajor = L"CatMajor";
PWSTR gpszCatMinor = L"CatMinor";
PWSTR gpszCatName = L"CatName";
PWSTR gpszCatGUIDCode = L"CatGUIDCode";
PWSTR gpszCatGUIDSub = L"CatGUIDSub";

/*************************************************************************
Functions to get strings
*/
PWSTR CatMinor (void)
{
   return gpszCatMinor;
}
PWSTR CatMajor (void)
{
   return gpszCatMajor;
}
PWSTR CatName (void)
{
   return gpszCatName;
}
PWSTR CatGUIDCode (void)
{
   return gpszCatGUIDCode;
}
PWSTR CatGUIDSub (void)
{
   return gpszCatGUIDSub;
}


PWSTR WSObjEmbed (void)
{
   return gpszWSObjEmbed;
}
PWSTR WSObjLocation (void)
{
   return gpszWSObjLocation;
}
PWSTR WSObjShowPlain (void)
{
   return gpszWSObjShowPlain;
}
PWSTR WSObjType (void)
{
   return gpszWSObjType;
}

PWSTR Attrib (void)
{
   return gpszAttrib;
}

PWSTR Pos (void)
{
   return gszPos;
}

PWSTR Text (void)
{
   return gszText;
}

PWSTR Back (void)
{
   return gszBack;
}

PWSTR Checked (void)
{
   return gszChecked;
}

PWSTR CurSel (void)
{
   return gszCurSel;
}

PWSTR ASPString (void)
{
   return gpszASP;
}

PWSTR WSFoundation (void)
{
   return gpszWSFoundation;
}

PWSTR WSClimate (void)
{
   return gpszWSClimate;
}

PWSTR WSTime (void)
{
   return gpszWSTime;
}

PWSTR WSLatitude (void)
{
   return gpszWSLatitude;
}

PWSTR WSTrueNorth (void)
{
   return gpszWSTrueNorth;
}

PWSTR WSDate (void)
{
   return gpszWSDate;
}

DWORD TodayFast (void)
{
   return gdwToday;
}

PSTR RegBase (void)
{
   return gpszRegBase;
}

PCWSTR RegBaseW (void)
{
   return gpszwRegBase;
}

PWSTR WSRoofing (void)
{
   return gpszWSRoofing;
}
PWSTR WSExternalWalls (void)
{
   return gpszWSExternalWalls;
}
PWSTR RedoSamePage (void)
{
   return gszRedoSamePage;
}
PWSTR ObjectSurface (void)
{
   return gszObjectSurface;
}

/*************************************************************************
DialogBoxLocation - Calculates the dialog box locations.

inputs
   HWND     hWnd - parent
   RECT     *pr - Filled with the rectangle
returns
   none
*/
void DialogBoxLocation (HWND hWnd, RECT *pr)
{
   // get the monitor info
   HMONITOR hMon = MonitorFromWindow (hWnd, MONITOR_DEFAULTTONEAREST);

   // find it
   FillXMONITORINFO ();
   DWORD i;
   PXMONITORINFO p;
   for (i = 0; i < gListXMONITORINFO.Num(); i++) {
      p = (PXMONITORINFO) gListXMONITORINFO.Get(i);
      if (p->hMonitor == hMon)
         break;
   }

   // move it to the next one
   p = (PXMONITORINFO) gListXMONITORINFO.Get(i % gListXMONITORINFO.Num());

   *pr = p->rWork;
//   GetWindowRect (hWnd, pr);

   int iWidth, iHeight;
   iWidth = pr->right - pr->left;
   iHeight = pr->bottom - pr->top;

   // just occupy the right hand side of the screen since in
   // many dialogs can change what see right away
   pr->left = pr->right - max(iWidth/3,320); // BUGFIX - Smaller
}


/*************************************************************************
DialogBoxLocation2 - Calculates the dialog box locations that are centered over the window.

inputs
   HWND     hWnd - parent
   RECT     *pr - Filled with the rectangle
returns
   none
*/
void DialogBoxLocation2 (HWND hWnd, RECT *pr)
{
   // get the monitor info
   HMONITOR hMon = MonitorFromWindow (hWnd, MONITOR_DEFAULTTONEAREST);

   // find it
   FillXMONITORINFO ();
   DWORD i;
   PXMONITORINFO p;
   for (i = 0; i < gListXMONITORINFO.Num(); i++) {
      p = (PXMONITORINFO) gListXMONITORINFO.Get(i);
      if (p->hMonitor == hMon)
         break;
   }

   // move it to the next one
   p = (PXMONITORINFO) gListXMONITORINFO.Get(i % gListXMONITORINFO.Num());

   *pr = p->rWork;
//   while (GetParent(hWnd))
//      hWnd = GetParent(hWnd);

//   GetWindowRect (hWnd, pr);

   int iWidth, iHeight, icx, icy;
   iWidth = pr->right - pr->left;
   iHeight = pr->bottom - pr->top;
   icx = (pr->right + pr->left) / 2;
   icy = (pr->bottom + pr->top) / 2;

   // reduce the width and height somewhat
   iWidth -= iWidth / 10;
   iHeight -= iHeight / 10;

   pr->left = icx - iWidth/2;
   pr->right = icx + iWidth / 2;
   pr->top = icy - iHeight/2;
   pr->bottom = icy + iHeight/2;

}



/*************************************************************************
DialogBoxLocation3 - Occupies the left side of the screen

inputs
   HWND     hWnd - parent
   RECT     *pr - Filled with the rectangle
   BOOL     fLeft - If TRUE occupy the left side, FALSE the right
returns
   none
*/
void DialogBoxLocation3 (HWND hWnd, RECT *pr, BOOL fLeft)
{
   // get the monitor info
   HMONITOR hMon = MonitorFromWindow (hWnd, MONITOR_DEFAULTTONEAREST);

   // find it
   FillXMONITORINFO ();
   DWORD i;
   PXMONITORINFO p;
   for (i = 0; i < gListXMONITORINFO.Num(); i++) {
      p = (PXMONITORINFO) gListXMONITORINFO.Get(i);
      if (p->hMonitor == hMon)
         break;
   }

   // move it to the next one
   p = (PXMONITORINFO) gListXMONITORINFO.Get(i % gListXMONITORINFO.Num());

   *pr = p->rWork;
//   while (GetParent(hWnd))
//      hWnd = GetParent(hWnd);

//   GetWindowRect (hWnd, pr);

   int iWidth, iHeight;
   iWidth = pr->right - pr->left;
   iHeight = pr->bottom - pr->top;

   // just occupy the right hand side of the screen since in
   // many dialogs can change what see right away
   if (fLeft)
      pr->right = pr->left + iWidth*2/3;
   else
      pr->left = pr->right - iWidth/3;
}

/*******************************************************************************
DefPage - Handle default page settings
*/
BOOL DefPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         // NOTE: This is causing some problems where controls also claim Enter
         // accelerator. leaving it in for now and working around problem in rename,
         // but may be other consequences

         // add enter - close
         ESCACCELERATOR a;
         memset (&a, 0, sizeof(a));
         a.c = VK_RETURN;
         a.fAlt = FALSE;
         a.dwMessage = ESCM_CLOSE;
         pPage->m_listESCACCELERATOR.Add (&a);
      }
      break;

   }
   // do nothing for now

   return FALSE;
}


/***********************************************************************
MemCat - Concatenated a unicode string onto a CMem object. It also
adds null-termination, although the m_dwCurPosn is decreased by 2 so
then next MemCat() will overwrite the NULL.

inputs
   PCMem    pMem - memory object
   PWSTR    psz - string
*/
void MemCat (PCMem pMem, PWSTR psz)
{
   // Too slow _ASSERTE (pMem->m_dwCurPosn == (DWORD)wcslen((PWSTR)pMem->p)*sizeof(WCHAR));

   pMem->StrCat (psz);
   size_t dwTemp;
   dwTemp = pMem->m_dwCurPosn;
   pMem->CharCat(0);
   pMem->m_dwCurPosn = dwTemp;
}

/***********************************************************************
MemCat - Concatenated a number onto a CMem object. It also
adds null-termination, although the m_dwCurPosn is decreased by 2 so
then next MemCat() will overwrite the NULL.

inputs
   PCMem    pMem - memory object
   int      iNum - number
*/
void MemCat (PCMem pMem, int iNum)
{
   WCHAR szTemp[32];
   swprintf (szTemp, gszPercentD, iNum);
   MemCat (pMem, szTemp);
}

/***********************************************************************
MemZero - Zeros out memory
inputs
   PCMem    pMem - memory object
*/
void MemZero (PCMem pMem)
{
   pMem->m_dwCurPosn = 0;
   pMem->CharCat(0);
   pMem->m_dwCurPosn = 0;
}

/***********************************************************************
MemCatSanitize - Concatenates a memory string, but sanitizes it for MML
first.

inputs
   PCMem    pMem - memory object
   PWSTR    psz - Needs to be sanitized
*/
void MemCatSanitize (PCMem pMem, PWSTR psz)
{
   // assume santizied will be about the same size
   pMem->Required(pMem->m_dwCurPosn + (wcslen(psz)+1)*2);
   size_t dwNeeded;
   if (StringToMMLString(psz, (PWSTR)((PBYTE)pMem->p + pMem->m_dwCurPosn),
      pMem->m_dwAllocated - pMem->m_dwCurPosn, &dwNeeded)) {

      pMem->m_dwCurPosn += wcslen((PWSTR)((PBYTE)pMem->p + pMem->m_dwCurPosn))*2;
      return;  // worked
   }

   // else need larger
   pMem->Required(pMem->m_dwCurPosn + dwNeeded + 2);
   StringToMMLString(psz, (PWSTR)((PBYTE)pMem->p + pMem->m_dwCurPosn),
      pMem->m_dwAllocated - pMem->m_dwCurPosn, &dwNeeded);
   pMem->m_dwCurPosn += wcslen((PWSTR)((PBYTE)pMem->p + pMem->m_dwCurPosn))*2;
}


/***********************************************************************
DateControlSet - Given a DFDATE, this sets the date control.

inputs
   PCEscPage      pPage - page
   PWSTR          pszControl - control name
   DFDATE         date - date
returns
   BOOL - TRUE if succeded
*/
BOOL DateControlSet (PCEscPage pPage, PWSTR pszControl, DFDATE date)
{
   PCEscControl pControl;
   pControl = pPage->ControlFind(pszControl);
   if (!pControl)
      return FALSE;

   pControl->AttribSetInt (gszDay, DAYFROMDFDATE(date));
   pControl->AttribSetInt (gszMonth, MONTHFROMDFDATE(date));
   pControl->AttribSetInt (gszYear, YEARFROMDFDATE(date));

   return TRUE;
}


/***********************************************************************
DateControlGet - Gets the date (DFDATE format) from the date control

inputs
   PCEscPage      pPage - page
   PWSTR          pszControl - control name
returns
   DFDATE - date. 0 if none specified
*/
DFDATE DateControlGet (PCEscPage pPage, PWSTR pszControl)
{
   PCEscControl pControl;
   pControl = pPage->ControlFind(pszControl);
   if (!pControl)
      return FALSE;

   int   iDay, iMonth, iYear;
   iDay = pControl->AttribGetInt (gszDay);
   iMonth = pControl->AttribGetInt (gszMonth);
   iYear = pControl->AttribGetInt (gszYear);

   // if any are 0 then return 0
   if (/*(iDay <= 0) ||*/ (iMonth <= 0) || (iYear <= 0))
      return 0;

   // else combin
   return TODFDATE(iDay, iMonth, iYear);
}


/***********************************************************************
TimeControlSet - Given a DFTIME, this sets the time control.

inputs
   PCEscPage      pPage - page
   PWSTR          pszControl - control name
   DFTIME         time - time
returns
   BOOL - TRUE if succeded
*/
BOOL TimeControlSet (PCEscPage pPage, PWSTR pszControl, DFTIME time)
{
   PCEscControl pControl;
   pControl = pPage->ControlFind(pszControl);
   if (!pControl)
      return FALSE;

   pControl->AttribSetInt (gszHour, HOURFROMDFTIME(time));
   pControl->AttribSetInt (gszMinute, MINUTEFROMDFTIME(time));

   return TRUE;
}


/***********************************************************************
TimeControlGet - Gets the time (DFTIME format) from the time control

inputs
   PCEscPage      pPage - page
   PWSTR          pszControl - control name
returns
   DFTIME - time. -1 if none specified
*/
DFTIME TimeControlGet (PCEscPage pPage, PWSTR pszControl)
{
   PCEscControl pControl;
   pControl = pPage->ControlFind(pszControl);
   if (!pControl)
      return (DWORD)-1;

   int   iHour, iMinute;
   iHour = pControl->AttribGetInt (gszHour);
   iMinute = pControl->AttribGetInt (gszMinute);

   // if any are 0 then return 0
   if ((iHour < 0) || (iMinute < 0))
      return (DWORD)-1;

   // else combin
   return TODFTIME(iHour, iMinute);
}




/*******************************************************************************
FillStatusColor - Call this from ESCM_INIT to change one of the xStatusColor
controls in the page to the given color.

inputs
   PCEscPage      pPage - page
   PWSTR          pszControl - control name
   COLORREF       cr - color
returns
   BOOL - TRUE if found the control and set it
*/
BOOL FillStatusColor (PCEscPage pPage, PWSTR pszControl, COLORREF cr)
{
   PCEscControl pControl = pPage->ControlFind (pszControl);
   if (!pControl)
      return FALSE;

   ESCMSTATUSTEXT st;
   WCHAR szTemp[32];
   CMem mem;
   memset (&st, 0, sizeof(st));
   MemZero (&mem);
   MemCat (&mem, L"<colorblend posn=background color=");
   ColorToAttrib (szTemp, cr);
   MemCat (&mem, szTemp);
   MemCat (&mem, L"/>");
   st.pszMML = (PWSTR) mem.p;

   return pControl->Message (ESCM_STATUSTEXT, &st);
}


#if 0 // DEAD code
/*******************************************************************************
AskColor - Brings up a dialog that asks the user to change a color.

inputs
   HWND        hWnd - Window to bring it up over
   COLORREF    cr - Starting color
   PCEscPage   pPage - If not NULL, and pszControl is not NULL, then if color is changed
                  this will call FillStatusColor() to the new color.
   PWSTR       pszControl - See pPage
returns
   COLORREF - New color
*/
COLORREF AskColor (HWND hWnd, COLORREF cr, PCEscPage pPage, PWSTR pszControl)
{
   CHOOSECOLOR cc;
   memset (&cc, 0, sizeof(cc));
   cc.lStructSize = sizeof(cc);
   cc.hwndOwner = hWnd;
   cc.rgbResult = cr;
   cc.Flags = CC_FULLOPEN | CC_RGBINIT | CC_SOLIDCOLOR;
   DWORD adwCust[16];
   DWORD j;
   for (j = 0; j < 16; j++)
      adwCust[j] = RGB(j * 16, j*16, j*16);
   cc.lpCustColors = adwCust;
   ChooseColor (&cc);

   // if changed then set status
   if ((cr != cc.rgbResult) && pPage && pszControl)
      FillStatusColor (pPage, pszControl, cc.rgbResult);

   return cc.rgbResult;
}
#endif

/********************************************************************************
AngleToControl - Takes an angle in RADIANS and sets an edit control with the value,
converting to DEGREES, and rounding to 0.1 degrees, and doing fMod.

inputs
   PCEscPage      pPage - page
   PWSTR          pszControl - control
   fp         fAngle - In radians
   BOOL           fNegative - If TRUE, it converts 180-360 to -180 to 0
returns
   BOOL - TRUE if succede
*/
BOOL AngleToControl (PCEscPage pPage, PWSTR pszControl, fp fAngle, BOOL fNegative)
{
   PCEscControl pControl = pPage->ControlFind (pszControl);
   if (!pControl)
      return FALSE;

   WCHAR szw[64];
   fAngle = ApplyGrid(fmod((fp)fAngle, (fp)(2 * PI)) / 2 / PI * 360, .1);
   if (fNegative && (fAngle > 180))
      fAngle -= 360;
   swprintf (szw, L"%g", (double)fAngle);
   return pControl->AttribSet (gszText, szw);
}


/********************************************************************************
AngleToString - Takes an angle in RADIANS and sets an edit control with the value,
converting to DEGREES, and rounding to 0.1 degrees, and doing fMod.

inputs
   char           *psz - Filled in
   fp         fAngle - In radians
   BOOL           fNegative - If TRUE, it converts 180-360 to -180 to 0
returns
   BOOL - TRUE if succede
*/
BOOL AngleToString (char *psz, fp fAngle, BOOL fNegative)
{
   fAngle = ApplyGrid(fmod((fp)fAngle, (fp)(2 * PI)) / 2 / PI * 360, .1);
   if (fNegative && (fAngle > 180))
      fAngle -= 360;
   if (fabs(fAngle) < CLOSE)
      fAngle = 0; // BUGFIX: fix roundoff error
   sprintf (psz, "%g", (double)fAngle);
   return TRUE;
}

/********************************************************************************
AngleFromControl - Returns an angle in RADIANS from an edit control. Converting
from DEGREES.

inputs
   PCEscPage      pPage - page
   PWSTR          pszControl - control
returns
   fp - fangle
*/
fp AngleFromControl (PCEscPage pPage, PWSTR pszControl)
{
   PCEscControl pControl = pPage->ControlFind (pszControl);
   if (!pControl)
      return 0;
   WCHAR szTemp[64];
   char sza[64];
   fp fVal;
   DWORD dwNeeded;
   szTemp[0] = 0;
   pControl->AttribGet (gszText, szTemp, sizeof(szTemp), &dwNeeded);
   WideCharToMultiByte (CP_ACP,0,szTemp,-1,sza,sizeof(sza),0,0);
   fVal = atof (sza);

   return fVal / 360.0 * 2 * PI;
}



/********************************************************************************
DoubleToControl - Takes an Double and sets an edit control with the value.

inputs
   PCEscPage      pPage - page
   PWSTR          pszControl - control
   fp         fDouble
returns
   BOOL - TRUE if succede
*/
BOOL DoubleToControl (PCEscPage pPage, PWSTR pszControl, fp fDouble)
{
   PCEscControl pControl = pPage->ControlFind (pszControl);
   if (!pControl)
      return FALSE;

   WCHAR szw[64];
   swprintf (szw, L"%g", (double)fDouble);
   return pControl->AttribSet (gszText, szw);
}


/********************************************************************************
DoubleFromControl - Returns an fp from a control

inputs
   PCEscPage      pPage - page
   PWSTR          pszControl - control
returns
   fp - fDouble
*/
fp DoubleFromControl (PCEscPage pPage, PWSTR pszControl)
{
   PCEscControl pControl = pPage->ControlFind (pszControl);
   if (!pControl)
      return 0;
   WCHAR szTemp[64];
   char sza[64];
   fp fVal;
   DWORD dwNeeded;
   szTemp[0] = 0;
   pControl->AttribGet (gszText, szTemp, sizeof(szTemp), &dwNeeded);
   WideCharToMultiByte (CP_ACP,0,szTemp,-1,sza,sizeof(sza),0,0);
   fVal = atof (sza);

   return fVal;
}



/**************************************************************************************
GenerateAlphaInPerspective - This fills in a buffer with alpha values for a series
of points in a line. Given that alpha ranges from 0 to 1 for a line.

inputs
   fp      x1,x2 - X values (in pixel) that the line is moving across.
                        x1 is the starting pixel, x2 is the ending.
   fp      w1,w2 - W values at the start and end
   int         iStart - Since calculate points on integer multiples, this is the
                     pixels to acutally start on. (x1 <= iStart <= x2
   int         iCount - Number of pixels to calculate.
                     (x1 <= (iStart + iCount-1) <= x2
   PCMem       pMem - Starting at pMem.m_dwCurPosn, and going for iCount * sizeof(fp),
                  the memory is filled in with the alpha values at each of the pixels.
   BOOL        fOneExtra - If TRUE, generates one extra point.
returns
   DWORD - Number of bytes used in pMem. You can get the pointer to pMem (After calling
   this function) by looking at m_dwCurPosn.
*/
DWORD GenerateAlphaInPerspective (fp x1, fp x2, fp w1, fp w2,
                                  int iStart, int iCount, PCMem pMem, BOOL fOneExtra)
{
   // multiply x's by w's
   x1 *= w1;
   x2 *= w2;

   // figure out some constants to speed up operations
   fp fDeltaX, fDeltaW;
   fp   fNumerator, fDenominator;
   fp   fDeltaNum, fDeltaDenom;
   fDeltaX = x2 - x1;
   fDeltaW = w2 - w1;
   fNumerator = w1 * iStart - x1;
   fDenominator = fDeltaX - iStart * fDeltaW;
   fDeltaNum = w1;
   fDeltaDenom = -fDeltaW;

   // allocate enough memory
   DWORD dwNum;
   dwNum = (DWORD) iCount * sizeof(fp);
   if (!pMem->Required(pMem->m_dwCurPosn + (dwNum + (fOneExtra ? sizeof(fp) : 0)) ))
      return 0;

   // fill it in
   fp   *pf;
   pf = (fp*) (((PBYTE) pMem->p) + pMem->m_dwCurPosn);
   pMem->m_dwCurPosn += dwNum;
   if (fOneExtra)
      pMem->m_dwCurPosn += sizeof(fp);

   for (; iCount; iCount--, pf++, fNumerator += fDeltaNum, fDenominator += fDeltaDenom) {
      *pf = fNumerator / fDenominator;
   }
   if (fOneExtra)
      *pf = fNumerator / fDenominator;

   return dwNum;
}



/*************************************************************************
Today - Returns today's date as DFDATE

*/
DFDATE Today (void)
{
   SYSTEMTIME  st;
   GetLocalTime (&st);
   return TODFDATE (st.wDay, st.wMonth, st.wYear);
}


/*****************************************************************************
DFDATEToMinutes - Converts a DFDATE to the number of minutes since Jan1, 1601.

inputs
   DFDATE      date - date
returns
   __int64 - number of minutes
*/
__int64 DFDATEToMinutes (DFDATE date)
{
   SYSTEMTIME  st;
   FILETIME ft;
   __int64 *pi = (__int64*) &ft;

   memset (&st, 0, sizeof(st));
   st.wDay = (WORD) DAYFROMDFDATE(date);
   st.wMonth = (WORD) MONTHFROMDFDATE(date);
   st.wYear = (WORD) YEARFROMDFDATE(date);

   SystemTimeToFileTime (&st, &ft);

   *pi = *pi / ((__int64)10 * 1000000 * 60);

   return *pi;
}


/****************************************************************************
MinutesToDFDATE - Converts a minutes value (see DFDATEToMinutes) to a DFDATE value.

inputs
   __int64     iMinutes - minutes
returns
   DFDATE - date
*/
DFDATE MinutesToDFDATE (__int64 iMinutes)
{
   SYSTEMTIME  st;
   FILETIME ft;
   __int64 *pi = (__int64*) &ft;

   *pi = iMinutes * 10 * 1000000 * 60;

   FileTimeToSystemTime (&ft, &st);

   return TODFDATE ((DWORD)st.wDay, (DWORD)st.wMonth, (DWORD) st.wYear);
}



/*************************************************************************
Now - Returns the curent time as DFTIME

*/
DFTIME Now (void)
{
   SYSTEMTIME  st;
   GetLocalTime (&st);
   return TODFTIME (st.wHour, st.wMinute);
}




/****************************************************************************
GetLastDirectory - Gets the last directory that used (or makes one up)
from the registry.

inputs
   char     *psz - Filled in
   DWORD    dwSize - # bytes
returns
   none
*/
void GetLastDirectory (char *psz, DWORD dwSize)
{
   // copy last directory in
   strcpy (psz, gszAppDir);

   HKEY  hKey = NULL;
   RegOpenKeyEx (HKEY_CURRENT_USER, RegBase(), 0, KEY_READ, &hKey);
   DWORD dw,dwType;
   if (hKey) {
      dw = dwSize;
      RegQueryValueEx (hKey, "Directory", 0, &dwType, (BYTE*) psz, &dw);
      RegCloseKey (hKey);
   }
}

void GetLastDirectory (WCHAR *psz, DWORD dwSize)
{
   char szTemp[MAX_PATH];
   GetLastDirectory (szTemp, sizeof(szTemp));

   MultiByteToWideChar (CP_ACP, 0, szTemp, -1, psz, dwSize / sizeof(WCHAR));
}

/****************************************************************************
SetLastDirectory - Gets the last directory that used in the registry

inputs
   char     *psz - Directory
returns
   none
*/
void SetLastDirectory (char *psz)
{
   HKEY  hKey = NULL;
   DWORD dwDisp;
   RegCreateKeyEx (HKEY_CURRENT_USER, RegBase(), 0, 0, REG_OPTION_NON_VOLATILE,
      KEY_READ | KEY_WRITE, NULL, &hKey, &dwDisp);
   if (hKey) {
      RegSetValueEx (hKey, "Directory", 0, REG_SZ, (BYTE*) psz, (DWORD)strlen(psz)+1);

      RegCloseKey (hKey);
   }
}

void SetLastDirectory (WCHAR *psz)
{
   char szTemp[MAX_PATH];
   WideCharToMultiByte (CP_ACP, 0, psz, -1, szTemp, sizeof(szTemp), 0, 0);

   SetLastDirectory (szTemp);
}

/***************************************************************************
MyRand - Random value from fmin to fmax;

inputs
   fp   fMin, fMax - Range of floating point values that can have
returns
   fp - value
*/
fp MyRand (fp fMin, fp fMax)
{
   int i = rand();
   i = i % 10000;
   return (fp)i / 10000 * (fMax - fMin) + fMin;
}


/****************************************************************8
KeyGet - Get a DWORD value from registry.

inputs
   char  *pszKey - Key to use
   DWORD    dwDefault - default value
returns
   DWORD - value
*/
DWORD KeyGet (char *pszKey, DWORD dwDefault)
{
   DWORD dwKey;
   dwKey = 0;

   // save to registry
   HKEY  hKey = NULL;
   DWORD dwDisp;
   RegCreateKeyEx (HKEY_CURRENT_USER, RegBase(), 0, 0, REG_OPTION_NON_VOLATILE,
      KEY_READ | KEY_WRITE, NULL, &hKey, &dwDisp);


   // write the version
   if (!hKey)
      return 0;

   DWORD dwSize, dwType;
   dwSize = sizeof(DWORD);
   dwKey = dwDefault;
   RegQueryValueEx (hKey, pszKey, NULL, &dwType, (LPBYTE) &dwKey, &dwSize);

   RegCloseKey (hKey);

   return dwKey;
}

/****************************************************************8
KeySet - Write a key to the registry

inputs
   char  *pszKey - Key to use
   DWORD dwValue - value
returns
   none
*/
void KeySet (char *pszKey, DWORD dwValue)
{
   // save to registry
   HKEY  hKey = NULL;
   DWORD dwDisp;
   RegCreateKeyEx (HKEY_CURRENT_USER, RegBase(), 0, 0, REG_OPTION_NON_VOLATILE,
      KEY_READ | KEY_WRITE, NULL, &hKey, &dwDisp);


   // write the version
   if (!hKey)
      return;

   RegSetValueEx (hKey, pszKey, 0, REG_DWORD, (BYTE*) &dwValue, sizeof(dwValue));

   RegCloseKey (hKey);

   return;
}



/****************************************************************8
KeyGet - Get a GUID value from registry.

inputs
   char  *pszKey - Key to use
   GUID    *pgValue - Should be filled in with a default value that's
            used if can't get one. If success, then fills in key
returns
   DWORD - value
*/
void KeyGet (char *pszKey, GUID *pgValue)
{
   // save to registry
   HKEY  hKey = NULL;
   DWORD dwDisp;
   RegCreateKeyEx (HKEY_CURRENT_USER, RegBase(), 0, 0, REG_OPTION_NON_VOLATILE,
      KEY_READ | KEY_WRITE, NULL, &hKey, &dwDisp);


   // write the version
   if (!hKey)
      return;

   DWORD dwSize, dwType;
   GUID gValue;
   dwSize = sizeof(GUID);
   if (ERROR_SUCCESS == RegQueryValueEx (hKey, pszKey, NULL, &dwType, (LPBYTE) &gValue, &dwSize))
      *pgValue = gValue;

   RegCloseKey (hKey);
}

/****************************************************************8
KeySet - Write a key to the registry

inputs
   char  *pszKey - Key to use
   GUID  *pgValue - value
returns
   none
*/
void KeySet (char *pszKey, GUID *pgValue)
{
   // save to registry
   HKEY  hKey = NULL;
   DWORD dwDisp;
   RegCreateKeyEx (HKEY_CURRENT_USER, RegBase(), 0, 0, REG_OPTION_NON_VOLATILE,
      KEY_READ | KEY_WRITE, NULL, &hKey, &dwDisp);


   // write the version
   if (!hKey)
      return;

   RegSetValueEx (hKey, pszKey, 0, REG_BINARY, (BYTE*) pgValue, sizeof(*pgValue));

   RegCloseKey (hKey);

   return;
}

/****************************************************************************
ColorSelPage
*/
#define  COLORSINBLOCK        7
#define  COLORSINROW          (COLORSINBLOCK * 3 + 2)

typedef struct {
   COLORREF       cCur;
   COLORREF       acRows[2][COLORSINROW];
   COLORREF       acBlocks[3][COLORSINBLOCK][COLORSINBLOCK];      // [0..2][x][y]
   DWORD          dwHRange;      // how much H varies, 1..256
   DWORD          dwSRange;      // how much S varies, 1..256
   DWORD          dwLRange;      // how much L varies, 1..256
   BOOL           fUse;          // set to TRUE if should use cCur
} CSPAGE, *PCSPAGE;

BOOL ColorSelPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCSPAGE pt = (PCSPAGE) pPage->m_pUserData;
   switch (dwMessage) {
   case ESCM_USER + 100:      // call this to fill the main color into the main color box
      {
         PCEscControl pControl;
         WCHAR szTemp[32];
         pControl = pPage->ControlFind (L"cbmain");
         ColorToAttrib (szTemp, pt->cCur);
         if (pControl)
            pControl->AttribSet (gszColor, szTemp);
      }
      return TRUE;

   case ESCM_USER + 101:       // call this to update the edit controls
      {
         PCEscControl pControl;
         pControl = pPage->ControlFind (L"red");
         if (pControl)
            pControl->AttribSetInt (gszText, (int)(DWORD)GetRValue(pt->cCur));
         pControl = pPage->ControlFind (L"green");
         if (pControl)
            pControl->AttribSetInt (gszText, (int)(DWORD)GetRValue(pt->cCur));
         pControl = pPage->ControlFind (L"blue");
         if (pControl)
            pControl->AttribSetInt (gszText, (int)(DWORD)GetRValue(pt->cCur));
      }
      return TRUE;

   case ESCM_USER + 102:      // call this to update all the colors in all the rows and blocks, calc them first
      {
         DWORD x,y, i;

         // min/max values
         pt->dwHRange = min(256, pt->dwHRange);
         pt->dwSRange = min(256, pt->dwSRange);
         pt->dwLRange = min(256, pt->dwLRange);
         pt->dwHRange = max(8, pt->dwHRange);
         pt->dwSRange = max(8, pt->dwSRange);
         pt->dwLRange = max(8, pt->dwLRange);

         float fH, fS, fL;
         ToHLS256 (GetRValue(pt->cCur), GetGValue(pt->cCur), GetBValue(pt->cCur),
            &fH, &fL, &fS);

         // min and max LS
         fp fMinS, fMaxS, fMinL, fMaxL;
         fMinS = max(fS - (fp) pt->dwSRange / 2.0, 0);
         fMaxS = fMinS + pt->dwSRange;
         if (fMaxS > 255) {
            fMinS -= (fMaxS - 255);
            fMaxS = 255;
         }
         fMinS = max(0,fMinS);

         fMinL = max(fL - (fp) pt->dwLRange / 2.0, 0);
         fMaxL = fMinL + pt->dwLRange;
         if (fMaxL > 255) {
            fMinL -= (fMaxL - 255);
            fMaxL = 255;
         }
         fMinL = max(0,fMinL);

         if ((pt->dwHRange >= 256) && (pt->dwSRange >= 256) && (pt->dwLRange >= 256)) {
            for (x = 0; x < COLORSINROW; x++) {
               pt->acRows[0][x] = FromHLS256 ((fp)x / (fp)COLORSINROW * 256.0, 128, 256);
               pt->acRows[1][x] = FromHLS256 (0, 256.0 * (1.0 - (fp)x / (fp)(COLORSINROW-1)), 0);
            }

            for (x = 0; x < COLORSINBLOCK; x++) for (y = 0; y < COLORSINBLOCK; y++) for (i = 0; i < 3; i++) {
               pt->acBlocks[i][x][y] = FromHLS256 (
                  (fp)x / (fp) COLORSINBLOCK * 256,
                  (1.0 - (fp)(y+1) / (fp)(COLORSINBLOCK+1)) * 256,
                  256 * (1.0 - (fp)i / 3.0));
            }
         }
         else {   // there's a range to deal with based of cCur
            for (x = 0; x < COLORSINROW; x++) {
               pt->acRows[0][x] = FromHLS256 (
                  myfmod(((fp)(x+1) / (fp)(COLORSINROW+1) - .5) * (fp)pt->dwHRange + fH, 256), fL, fS);
               pt->acRows[1][x] = FromHLS256 (
                  fH, fMinL + (fMaxL - fMinL) * (1.0 - (fp)x / (fp)(COLORSINROW-1)), fS);
            }
            
            for (x = 0; x < COLORSINBLOCK; x++) for (y = 0; y < COLORSINBLOCK; y++) {
               pt->acBlocks[0][x][y] = FromHLS256 (
                  myfmod(((fp)(x+1) / (fp)(COLORSINBLOCK+1) - .5) * (fp)pt->dwHRange + fH, 256),
                  fMinL + (fMaxL - fMinL) * (1.0 - (fp)y / (fp)(COLORSINBLOCK-1)),
                  fS);

               pt->acBlocks[1][x][y] = FromHLS256 (
                  myfmod(((fp)(x+1) / (fp)(COLORSINBLOCK+1) - .5) * (fp)pt->dwHRange + fH, 256),
                  fL,
                  fMinS + (fMaxS - fMinS) * (1.0 - (fp)y / (fp)(COLORSINBLOCK-1)) );

               pt->acBlocks[2][x][y] = FromHLS256 (
                  fH,
                  fMinL + (fMaxL - fMinL) * (1.0 - (fp)x / (fp)(COLORSINBLOCK-1)),
                  fMinS + (fMaxS - fMinS) * (1.0 - (fp)y / (fp)(COLORSINBLOCK-1)) );
            }
         }

         // fill in the boxes
         PCEscControl pControl;
         WCHAR szTemp[32], szTemp2[32];
         for (x = 0; x < COLORSINROW; x++) for (i = 0; i < 2; i++) {
            swprintf (szTemp, L"row%d%d", i, x);
            pControl = pPage->ControlFind (szTemp);
            ColorToAttrib (szTemp2, pt->acRows[i][x]);
            if (pControl)
               pControl->AttribSet (gszColor, szTemp2);
         }
         for (x = 0; x < COLORSINBLOCK; x++) for (y = 0; y < COLORSINBLOCK; y++) for (i = 0; i < 3; i++) {
            swprintf (szTemp, L"block%d%d%d", i, x, y);
            pControl = pPage->ControlFind (szTemp);
            ColorToAttrib (szTemp2, pt->acBlocks[i][x][y]);
            if (pControl)
               pControl->AttribSet (gszColor, szTemp2);
         }

         pPage->Invalidate ();
      }
      return TRUE;

   case ESCM_INITPAGE:
      {
         // start out with maximum range
         pt->dwHRange = pt->dwSRange = pt->dwLRange = 256;
         pPage->Message (ESCM_USER + 100);
         pPage->Message (ESCM_USER + 101);
         pPage->Message (ESCM_USER + 102);
      }
      break;

   case ESCM_LINK:
      {
         PESCMLINK p = (PESCMLINK) pParam;
         PWSTR pszRow = L"row", pszBlock = L"block";
         DWORD dwRow = (DWORD)wcslen(pszRow), dwBlock = (DWORD)wcslen(pszBlock);
         if (!p->psz)
            break;

         DWORD i, x, y;
         if (!wcsncmp(p->psz, pszRow, dwRow)) {
            // clicked on a row
            i = (DWORD) (p->psz[dwRow] - L'0');
            x = _wtoi (p->psz + (dwRow+1));
            i = min (1, i);
            x = min (COLORSINROW-1, x);

            pt->cCur = pt->acRows[i][x];

            // zoom in
            pt->dwHRange /= 2;
            pt->dwSRange /= 2;
            pt->dwLRange /= 2;
            if (i)   // further zoom in depending upon which one pressed
               pt->dwLRange /= 2;
            else
               pt->dwHRange /= 2;

            // refresh
            pPage->Message (ESCM_USER + 100);
            pPage->Message (ESCM_USER + 101);
            pPage->Message (ESCM_USER + 102);

            return TRUE;
         }
         else if (!wcsncmp(p->psz, pszBlock, dwBlock)) {
            // clicked on a row
            i = (DWORD) (p->psz[dwBlock] - L'0');
            x = (DWORD) (p->psz[dwBlock+1] - L'0');
            y = (DWORD) (p->psz[dwBlock+2] - L'0');
            i = min (2, i);
            x = min (COLORSINBLOCK-1, x);
            y = min (COLORSINBLOCK-1, y);

            pt->cCur = pt->acBlocks[i][x][y];

            // zoom in
            if ((pt->dwHRange >= 256) && (pt->dwSRange >= 256) && (pt->dwLRange >= 256)) {
               // special first mode, doesn't distinguish one any better than rest
            }
            else {
               // clicked on one of three differnt ones
               if (i != 2)
                  pt->dwHRange /= 2;
               if (i != 0)
                  pt->dwSRange /= 2;
               if (i != 1)
                  pt->dwLRange /= 2;
            }

            // futher zoom in by a factor of 2
            pt->dwHRange /= 2;
            pt->dwSRange /= 2;
            pt->dwLRange /= 2;

            // refresh
            pPage->Message (ESCM_USER + 100);
            pPage->Message (ESCM_USER + 101);
            pPage->Message (ESCM_USER + 102);

            return TRUE;
         }
         else if (!_wcsicmp(p->psz, L"cbmain")) {
            // zoom in, but make it a lot since original was cloest
            pt->dwHRange /= 2;
            pt->dwSRange /= 2;
            pt->dwLRange /= 2;

            // refresh
            pPage->Message (ESCM_USER + 102);

            return TRUE;
         }
      }
      break;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;

         if (!p->pControl || !p->pControl->m_pszName)
            break;

         if (!_wcsicmp(p->pControl->m_pszName, L"ok")) {
            pt->fUse = TRUE;
            pPage->Exit (L"ok");
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"morecolors")) {
            pt->dwHRange *= 2;
            pt->dwSRange *= 2;
            pt->dwLRange *= 2;

            // refresh
            pPage->Message (ESCM_USER + 102);

            return TRUE;
         }
      }
      break;

   case ESCN_EDITCHANGE:
      {
         // if any edit changed then has to do with this
         pt->cCur = RGB(
            (BYTE) DoubleFromControl (pPage, L"red"),
            (BYTE) DoubleFromControl (pPage, L"green"),
            (BYTE) DoubleFromControl (pPage, L"blue"));
         // refresh
         pPage->Message (ESCM_USER + 100);
         pPage->Message (ESCM_USER + 102);
      }
      break;


   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"COLORLIST")) {
            DWORD i, j, k;
            MemZero (&gMemTemp);
            PWSTR pszTRStart = L"<tr><td tbmargin=0/>";
            PWSTR pszTREnd = L"<td tbmargin=0/></tr>";
            PWSTR pszBlankLine = L"<tr><td tbmargin=0><br/></td></tr>";
            PWSTR pszBlank = L"<td tbmargin=0/>";
            PWSTR pszColor = L"<xcbtable name=%s%d href=%s%d/>";
            WCHAR szTemp[64];

            // top row
            MemCat (&gMemTemp, pszTRStart);
            for (i = 0; i < COLORSINROW; i++) {
               swprintf (szTemp, pszColor, L"row0", i, L"row0", i);
               MemCat (&gMemTemp, szTemp);
            }
            MemCat (&gMemTemp, pszTREnd);

            // divider
            MemCat (&gMemTemp, pszBlankLine);

            // center blocks
            for (i = 0; i < COLORSINBLOCK; i++) {
               MemCat (&gMemTemp, pszTRStart);
               for (k = 0; k < 3; k++) {
                  if (k)
                     MemCat (&gMemTemp, pszBlank);

                  for (j = 0; j < COLORSINBLOCK; j++) {
                     swprintf (szTemp, L"<xcbtable name=block%d%d%d href=block%d%d%d/>",
                        k, j, i, k, j, i);
                     MemCat (&gMemTemp, szTemp);
                  }
               }
               MemCat (&gMemTemp, pszTREnd);
            }

            // divider
            MemCat (&gMemTemp, pszBlankLine);

            // bottom row
            MemCat (&gMemTemp, pszTRStart);
            for (i = 0; i < COLORSINROW; i++) {
               swprintf (szTemp, pszColor, L"row1", i, L"row1", i);
               MemCat (&gMemTemp, szTemp);
            }
            MemCat (&gMemTemp, pszTREnd);

            p->pszSubString = (PWSTR) gMemTemp.p;
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}




/*******************************************************************************
AskColor - Brings up a dialog that asks the user to change a color.

inputs
   HWND        hWnd - Window to bring it up over
   COLORREF    cr - Starting color
   PCEscPage   pPage - If not NULL, and pszControl is not NULL, then if color is changed
                  this will call FillStatusColor() to the new color.
   PWSTR       pszControl - See pPage
   BOOL        *pfCancel - If TRUE pressed cancel. Can be NULL
returns
   COLORREF - New color
*/
COLORREF AskColor (HWND hWnd, COLORREF cr, PCEscPage pPage, PWSTR pszControl, BOOL *pfCancel)
{
   CEscWindow cWindow;
   RECT r;
   DialogBoxLocation2 (hWnd, &r);

   cWindow.Init (ghInstance, hWnd, EWS_FIXEDSIZE, &r);
   PWSTR pszRet;

   // set up the info
   CSPAGE t;
   memset (&t, 0, sizeof(t));
   t.cCur = cr;

   // start with the first page
   pszRet = cWindow.PageDialog (ghInstance, IDR_MMLCOLORSEL, ColorSelPage, &t);

   if (t.fUse) {
      if ((cr != t.cCur) && pPage && pszControl)
         FillStatusColor (pPage, pszControl, t.cCur);
      if (pfCancel)
         *pfCancel = FALSE;
      return t.cCur;
   }
   else {
      if (pfCancel)
         *pfCancel = TRUE;
      return cr;
   }
}



/****************************************************************************
ToHLS - Converts from RGB to HLS

inputs
   float   fRed, fGreen, fBlue - rgb
   float   *pfH, *pfL, *pfS
returns
   none
*/
void ToHLS256 (float fRed, float fGreen, float fBlue,
            float* pfH, float* pfL, float* pfS)
{
   float   fMax, fMin;
   fMax = max(fRed, fGreen);
   fMax = max(fMax, fBlue);
   fMin = min(fRed, fGreen);
   fMin = min(fMin, fBlue);

   *pfL = (fMax + fMin)/2;

   if (fMax == fMin) {
      // achromatic case
      *pfS = 0;
      *pfH = 0;
      return;
   }

   // else, not the same
   if (*pfL <= 128)
      *pfS = 256 * (fMax - fMin) / (fMax + fMin);
   else
      *pfS = 256 * (fMax - fMin) / (2 * 256 - fMax - fMin);
   *pfS = max(0, *pfS);
   *pfS = min(255, *pfS);

   // calculate the hue
   float fDelta;
   fDelta = fMax - fMin;
   if (fRed == fMax)
      *pfH = (fGreen - fBlue) / fDelta;
   else if (fGreen == fMax)
      *pfH = 2 + (fBlue - fRed)  /fDelta;
   else // if (fBlue == fMax)
      *pfH = 4 + (fRed - fGreen) / fDelta;
   *pfH = *pfH * 60 / 360 * 256;
   if (*pfH < 0)
      *pfH += 256;
   *pfH = max(0, *pfH);
   *pfH = min(255, *pfH);
}


/****************************************************************************
FromHLS256 - Convers from HLS to RGB.

inputs
   float       fH, fL, fS - HLS from 0 to 255
returns
   COLORREF - RGB
*/
COLORREF FromHLS256 (float fH, float fL, float fS)
{
   float fR, fG, fB;
   FromHLS256 (fH, fL, fS, &fR, &fG, &fB);

   return RGB ((BYTE)fR, (BYTE)fG, (BYTE)fB);
}

/****************************************************************************
FromHLS - Converts from HLS to RGB HLS

inputs
   float   fRed, fGreen, fBlue - rgb
   float   *pfH, *pfL, *pfS
returns
   none
*/
void FromHLS256 (float fH, float fL, float fS,
            float* pfRed, float* pfGreen, float* pfBlue)
{
   // combination of lightness and saturation tells us min and max
   float fMin, fMax, k;
   k = (256 - fS) / (256 + fS);
   if (!fS && !fH) {
      fMin = fMax = fL;
   }
   else {
      if (fL <= 128) {
         fMin = 2 * k * fL / (1 + k);
      }
      else {
         fMin = fL - fS + fS * fL / 256;
      }
      fMax = 2 * fL - fMin;
   }

   // delta
   float fDelta;
   fDelta = fMax - fMin;

   float r,g,b;
#define  COLOREND    (256.0 / 3.0)
   if (fH <= COLOREND/2) {   // red is max, green increase
      g = fMin + fH / (COLOREND/2) * fDelta;
      r = fMax;
      b = fMin;
   }
   else if (fH <= COLOREND*1.0) { // green is max, red decrease
      r = fMax - (fH - COLOREND*.5) / (COLOREND/2) * fDelta;
      g = fMax;
      b = fMin;
   }
   else if (fH <= COLOREND*1.5) { // green is max, blue increase
      r = fMin;
      g = fMax;
      b = fMin + (fH - COLOREND*1.0) / (COLOREND/2) * fDelta;
   }
   else if (fH <= COLOREND*2.0) { // blue is max, green decrease
      r = fMin;
      g = fMax - (fH - COLOREND*1.5) / (COLOREND/2) * fDelta;
      b = fMax;
   }
   else if (fH <= COLOREND*2.5) { // blue is max, red increase
      r = fMin + (fH - COLOREND*2.0) / (COLOREND/2) * fDelta;
      g = fMin;
      b = fMax;
   }
   else { // red is max, blue is decreasing
      r = fMax;
      g = fMin;
      b = fMax - (fH - COLOREND*2.5) / (COLOREND/2) * fDelta;
   }



   // can't go beyond
   r = min(r,255);
   g = min(g, 255);
   b = min(b, 255);

   // done
   *pfRed = r;
   *pfGreen = g;
   *pfBlue = b;
}


void ToHLS (WORD r, WORD g, WORD b, WORD *ph, WORD *pl, WORD *ps)
{
   float   fr, fg, fb, fh, fl, fs;

   fr = r / 256.0;
   fg = g / 256.0;
   fb = b / 256.0;

   ToHLS256 (fr, fg, fb, &fh, &fl, &fs);
   *ph = (WORD) (fh * 255);
   *pl = (WORD) (fl * 255);
   *ps = (WORD) (fs * 255);
}

void FromHLS (WORD h, WORD l, WORD s, WORD *pr, WORD *pg, WORD *pb)
{
   float   fr, fg, fb, fh, fl, fs;

   fh = h / 256.0;
   fl = l / 256.0;
   fs = s / 256.0;

   FromHLS256 (fh, fl, fs, &fr, &fg, &fb);
   *pb = (WORD) (fr * 255);
   *pg = (WORD) (fg * 255);
   *pb = (WORD) (fb * 255);
}


/****************************************************************************
ReturnXMONITORINFO - Returns a pointer to gListXMONITORINFO
*/
PCListFixed ReturnXMONITORINFO (void)
{
   return &gListXMONITORINFO;
}

/****************************************************************************
FillXMONITORINFO - Enum all the monitors on the system and fill in XMONITORINFO.

inputs
   none
retrusn
   DWORD - Index into XMONITORINFO with the least number of views on that monitor
*/
BOOL CALLBACK MyMonitorEnumProc(
  HMONITOR hMonitor,  // handle to display monitor
  HDC hdcMonitor,     // handle to monitor-appropriate device context
  LPRECT lprcMonitor, // pointer to monitor intersection rectangle
  LPARAM dwData       // data passed from EnumDisplayMonitors
)
{
   XMONITORINFO mi;
   memset (&mi, 0, sizeof(mi));
   mi.hDCMonitor = hdcMonitor;
   mi.hMonitor = hMonitor;
   mi.rMonitor = *lprcMonitor;

   // get some more
   MONITORINFO m;
   memset (&m, 0, sizeof(m));
   m.cbSize = sizeof(m);
   GetMonitorInfo (mi.hMonitor, &m);
   mi.rWork = m.rcWork;
   mi.fPrimary = (m.dwFlags & MONITORINFOF_PRIMARY) ? TRUE : FALSE;

   gListXMONITORINFO.Add (&mi);

   return TRUE;
}
 
DWORD FillXMONITORINFO (void)
{
   DWORD dwLeastIndex = 0, dwLeastCount = 1000;

   gListXMONITORINFO.Init (sizeof(XMONITORINFO));
   gListXMONITORINFO.Clear();

   // get the monitors
   EnumDisplayMonitors (NULL, NULL, MyMonitorEnumProc, 0);

   // find out which views are on which monitors
   DWORD j;
   for (j = 0; j < gListXMONITORINFO.Num(); j++) {
      PXMONITORINFO px = (PXMONITORINFO) gListXMONITORINFO.Get(j);

      // if it's not the primary monitor then add one.
      // this way defaults to the primary monitor first
      if (!px->fPrimary)
         px->dwUsed++;

      // BUGFIX - Take this out since now have multiple windows and cant really
      // look at gListPCHouseView only
#if 0
      for (i = 0; i < gListPCHouseView.Num(); i++) {
         PCHouseView ph = *((PCHouseView*)gListPCHouseView.Get(i));

         if (!ph->m_hWnd)
            continue;


         RECT r, rWnd;
         GetWindowRect (ph->m_hWnd, &rWnd);
         if (IntersectRect(&r, &rWnd, &px->rMonitor))
            px->dwUsed += 2;  // since need 1/2 point for primary monitor
      }
#endif // 0

      // if this has the les used then remember
      if (px->dwUsed < dwLeastCount) {
         dwLeastCount = px->dwUsed;
         dwLeastIndex = j;
      }
   }

   return dwLeastIndex;
}



/********************************************************************************
ComboBoxSet - Set the combobox based on a value.

inputs
   PCEscPage      pPage -page
   PWSTR          psz - Control
   DWORD          dwVal - value
returns
   BOOL - TRUE if usccess
*/
BOOL ComboBoxSet (PCEscPage pPage, PWSTR psz, DWORD dwVal)
{
   PCEscControl pControl;
   pControl = pPage->ControlFind (psz);
   if (!pControl)
      return FALSE;

   ESCMCOMBOBOXSELECTSTRING sel;
   WCHAR szTemp[32];
   swprintf (szTemp, L"%d", dwVal);
   memset (&sel, 0, sizeof(sel));
   sel.fExact = TRUE;
   sel.psz = szTemp;
   sel.iStart = -1;
   pControl->Message (ESCM_COMBOBOXSELECTSTRING, &sel);

   // BUGFIX - If can't find then error
   if (sel.dwIndex == -1)
      return FALSE;

   return TRUE;
}


/********************************************************************************
ListBoxSet - Set the ListBox based on a value.

inputs
   PCEscPage      pPage -page
   PWSTR          psz - Control
   DWORD          dwVal - value
returns
   BOOL - TRUE if usccess
*/
BOOL ListBoxSet (PCEscPage pPage, PWSTR psz, DWORD dwVal)
{
   PCEscControl pControl;
   pControl = pPage->ControlFind (psz);
   if (!pControl)
      return FALSE;

   ESCMLISTBOXSELECTSTRING sel;
   WCHAR szTemp[32];
   swprintf (szTemp, L"%d", dwVal);
   memset (&sel, 0, sizeof(sel));
   sel.fExact = TRUE;
   sel.psz = szTemp;
   sel.iStart = -1;
   pControl->Message (ESCM_LISTBOXSELECTSTRING, &sel);

   return TRUE;
}



/**************************************************************************************
DefaultBuildingSettings - If a building setting (such as the date, time, latitude, etc.)
is unknown, then this fills it in based on the user's time zone and country code.

inputs
   DFDATE      *pdwDate - If not null, filled with default date
   DFTIME      *pwfTime - If not null, filled with default time
   fp          *pfLatitude - If not null, filled with default latitude
   DWORD       *pdwClimate - If not null, filled with default climate
   DWORD       *pdwRoofing - If not null, filled with default roofing
   DWORD       *pdwExternalWalls - If not null, filled with default external walls
   DOWRD       *pdwFoundation - If not null, filled with default foundation
*/
void DefaultBuildingSettings (DFDATE *pdwDate, DFTIME *pdwTime, fp *pfLatitude,
                              DWORD *pdwClimate, DWORD *pdwRoofing,
                              DWORD *pdwExternalWalls, DWORD *pdwFoundation)
{
   static DWORD dwClimate, dwRoofing, dwExternalWalls, dwFoundation;
   static DFDATE dwDate;
   static DFTIME dwTime;
   static fp fLatitude;
   static BOOL fIsValid = FALSE;

   if (!fIsValid) {
      //TIME_ZONE_INFORMATION tzi;
      //memset (&tzi, 0, sizeof(tzi));
      //GetTimeZoneInformation (&tzi);
      LANGID lid;
      lid = GetUserDefaultLangID ();
      if (!M3DDynamicGet())
         lid = 0x0409;


      // some defualts
      dwClimate = 2; // temperate
      dwRoofing = 3; // asphalt
      dwExternalWalls = 2; // clapboards
      dwFoundation = 3; // perimeter
      fLatitude = 35;

      dwDate = Today();
      dwTime = TODFTIME (15, 0);

      switch (lid) {
      case 0x0401: // Arabic (Saudi Arabia) 
      case 0x0801: // Arabic (Iraq) 
      case 0x0c01: // Arabic (Egypt) 
      case 0x1001: // Arabic (Libya) 
      case 0x1401: //  Arabic (Algeria) 
      case 0x1801: //  Arabic (Morocco) 
      case 0x1c01: //  Arabic (Tunisia) 
      case 0x2001: //  Arabic (Oman) 
      case 0x2401: //  Arabic (Yemen) 
      case 0x2801: //  Arabic (Syria) 
      case 0x2c01: //  Arabic (Jordan) 
      case 0x3001: //  Arabic (Lebanon) 
      case 0x3401: //  Arabic (Kuwait) 
      case 0x3801: //  Arabic (U.A.E.) 
      case 0x3c01: //  Arabic (Bahrain) 
      case 0x4001: //  Arabic (Qatar) 
      case 0x040d: //  Hebrew 
      case 0x041f: //  Turkish 
      case 0x0429: //  Farsi 
         fLatitude = 30;
         dwClimate = 5; // arid
         dwRoofing = 2; // tiles
         dwFoundation = 2; // pad
         dwExternalWalls = 6; // cement block
         break;

      case 0x0804: //  Chinese (PRC) 
         fLatitude = 40;
         dwClimate = 2; // temperate
         dwRoofing = 2; // tiles
         dwFoundation = 2; // pad
         dwExternalWalls = 6; // cement block
         break;

      case 0x0404: //  Chinese (Taiwan Region) 
      case 0x0c04: //  Chinese (Hong Kong SAR, PRC) 
      case 0x1004: //  Chinese (Singapore) 
         fLatitude = 20;
         dwClimate = 1; // subtropical
         dwRoofing = 2; // tiles
         dwFoundation = 2; // pad
         dwExternalWalls = 6; // cement block
         break;

      case 0x0405: //  Czech 
      case 0x0406: //  Danish 
      case 0x0407: //  German (Standard) 
      case 0x0807: //  German (Swiss) 
      case 0x0c07: //  German (Austrian) 
      case 0x1007: //  German (Luxembourg) 
      case 0x1407: //  German (Liechtenstein) 
      case 0x0809: //  English (United Kingdom) 
      case 0x1809: //  English (Ireland) 
      case 0x040b: //  Finnish 
      case 0x040c: //  French (Standard) 
      case 0x100c: //  French (Swiss) 
      case 0x140c: //  French (Luxembourg) 
      case 0x080c: //  French (Belgian) 
      case 0x040e: //  Hungarian 
      case 0x040f: //  Icelandic 
      case 0x0413: //  Dutch (Standard) 
      case 0x0813: //  Dutch (Belgian) 
      case 0x0414: //  Norwegian (Bokmal) 
      case 0x0814: //  Norwegian (Nynorsk) 
      case 0x0415: //  Polish 
      case 0x0419: //  Russian 
      case 0x041a: //  Croatian 
      case 0x081a: //  Serbian (Latin) 
      case 0x0c1a: //  Serbian (Cyrillic) 
      case 0x041b: //  Slovak 
      case 0x041c: //  Albanian 
      case 0x041d: //  Swedish 
      case 0x081d: //  Swedish (Finland) 
      case 0x0422: //  Ukrainian 
      case 0x0423: //  Belarusian 
      case 0x0424: //  Slovenian 
      case 0x0425: //  Estonian 
      case 0x0426: //  Latvian 
      case 0x0427: //  Lithuanian 
         // northern eurpe
         dwClimate = 2; // temperate
         dwRoofing = 2; // tiles
         dwExternalWalls = 1; // brick veneer
         dwFoundation = 4; // basement
         fLatitude = 50;
         break;

      case 0x0402: //  Bulgarian 
      case 0x0403: //  Catalan 
      case 0x0408: //  Greek 
      case 0x040a: //  Spanish (Traditional Sort) 
      case 0x0c0a: //  Spanish (Modern Sort) 
      case 0x0410: //  Italian (Standard) 
      case 0x0810: //  Italian (Swiss) 
      case 0x0816: //  Portuguese (Standard) 
      case 0x0418: //  Romanian 
      case 0x042d: //  Basque 
         // southern europe
         fLatitude = 30;
         dwClimate = 6; // mediterranean
         dwRoofing = 2; // tiles
         dwFoundation = 2; // pad
         dwExternalWalls = 6; // cement block
         break;

      case 0x0409: //  English (United States) 
      case 0x1009: //  English (Canadian) 
      case 0x0c0c: //  French (Canadian) 
         // already defaults
         break;

      case 0x0c09: //  English (Australian) 
         fLatitude = -25;
         dwClimate = 6; // mediterranean
         dwRoofing = 2; // tiles
         dwFoundation = 2; // pad
         dwExternalWalls = 6; // cement block
         break;

      case 0x1409: //  English (New Zealand) 
         fLatitude = -40;
         dwClimate = 2; // temparate
         dwRoofing = 2; // tiles
         dwFoundation = 2; // pad
         dwExternalWalls = 1; // brick veneer
         break;

      case 0x1c09: //  English (South Africa) 
      case 0x2009: //  English (Jamaica) 
      case 0x2409: //  English (Caribbean) 
      case 0x2809: //  English (Belize) 
      case 0x2c09: //  English (Trinidad) 
      case 0x100a: //  Spanish (Guatemala) 
      case 0x140a: //  Spanish (Costa Rica) 
      case 0x180a: //  Spanish (Panama) 
      case 0x1c0a: //  Spanish (Dominican Republic) 
      case 0x200a: //  Spanish (Venezuela) 
      case 0x240a: //  Spanish (Colombia) 
      case 0x440a: //  Spanish (El Salvador) 
      case 0x480a: //  Spanish (Honduras) 
      case 0x4c0a: //  Spanish (Nicaragua) 
      case 0x500a: //  Spanish (Puerto Rico) 
         // tropical
         fLatitude = 10;
         dwClimate = 0; // tropical
         dwRoofing = 2; // tiles
         dwFoundation = 2; // pad
         dwExternalWalls = 6; // cement block
         break;

      case 0x280a: //  Spanish (Peru) 
      case 0x300a: //  Spanish (Ecuador) 
         // tropical, south
         fLatitude = -10;
         dwClimate = 0; // tropical
         dwRoofing = 2; // tiles
         dwFoundation = 2; // pad
         dwExternalWalls = 6; // cement block
         break;

      case 0x0421: //  Indonesian 
         // tropical, south
         fLatitude = -10;
         dwClimate = 0; // tropical
         dwRoofing = 0; // corrogated iron
         dwFoundation = 2; // pad
         dwExternalWalls = 6; // cement block
         break;

      case 0x080a: //  Spanish (Mexican) 
         // tropical arid
         fLatitude = 15;
         dwClimate = 5; // arid
         dwRoofing = 2; // tiles
         dwFoundation = 2; // pad
         dwExternalWalls = 6; // cement block
         break;

      case 0x400a: //  Spanish (Bolivia) 
      case 0x0416: //  Portuguese (Brazilian) 
         // sub-tropical south
         fLatitude = -15;
         dwClimate = 1; // sub-tropical
         dwRoofing = 2; // tiles
         dwFoundation = 2; // pad
         dwExternalWalls = 6; // cement block
         break;

      case 0x2c0a: //  Spanish (Argentina) 
      case 0x340a: //  Spanish (Chile) 
      case 0x380a: //  Spanish (Uruguay) 
      case 0x3c0a: //  Spanish (Paraguay) 
      case 0x0436: //  Afrikaans 
         // temperate
         fLatitude = -35;
         // rest is same as defaults
         break;


      case 0x0411: //  Japanese 
      case 0x0412: //  Korean 
      case 0x0812: //  Korean (Johab) 
         // temprate
         dwClimate = 2; // temperate
         dwRoofing = 2; // tiles
         dwExternalWalls = 6; // cement block
         dwFoundation = 2; // pad
         fLatitude = 50;
         break;

      case 0x041e: //  Thai 
      case 0x042a: //  Vietnamese 
      case 0x0438: //  Faeroese ???
         // tropical, north
         fLatitude = 10;
         dwClimate = 0; // tropical
         dwRoofing = 0; // corrogated iron
         dwFoundation = 2; // pad
         dwExternalWalls = 6; // cement block
         break;
      }

      // fix latitude
      fLatitude = (fLatitude / 180.0) * PI;

      fIsValid = TRUE;
   }

   if (pdwDate)
      *pdwDate = dwDate;
   if (pdwTime)
      *pdwTime = dwTime;
   if (pfLatitude)
      *pfLatitude = fLatitude;
   if (pdwClimate)
      *pdwClimate = dwClimate;
   if (pdwRoofing)
      *pdwRoofing = dwRoofing;
   if (pdwExternalWalls)
      *pdwExternalWalls = dwExternalWalls;
   if (pdwFoundation)
      *pdwFoundation = dwFoundation;
}


/************************************************************************************
BeepWindowBeep - Plays a beep
*/
void BeepWindowBeep (DWORD dwBeep)
{
   if (gpBeepWindow)
      gpBeepWindow->Beep (dwBeep);
}

/************************************************************************************
BeepWindowInit - Initialize the beep window if it isn't already
*/
void BeepWindowInit (void)
{
   if (gpBeepWindow)
      return;

   gpBeepWindow = new CEscWindow;
   if (gpBeepWindow)
      gpBeepWindow->Init (ghInstance, NULL, EWS_HIDE);
}


/************************************************************************************
BeepWindowEnd - Frees the beep window
*/
void BeepWindowEnd (void)
{
   // delete the beep window
   if (gpBeepWindow)
      delete gpBeepWindow;
   gpBeepWindow = NULL;
}

/************************************************************************************
WorldSet - Sets the current world, so some objects can get info from it, such
as default climate.

inputs
   PCWorldSocket     pWorld - world
   PCSceneSet        pScene - Scene currently in use
*/
void WorldSet (DWORD dwRenderShard, PCWorldSocket pWorld, PCSceneSet pScene)
{
   _ASSERTE (!pWorld || (pWorld->RenderShardGet() == dwRenderShard));

   gapWorld[dwRenderShard] = pWorld;
   gapScene[dwRenderShard] = pScene;
}



/************************************************************************************
WorldGet - Gets the current world, so some objects can get info from it, such
as default climate.

inputs
   PCSceneSet *ppScene - If not NULL then filled with the scene
returns
   PCWorldSocket - world. May be null
*/
PCWorldSocket WorldGet (DWORD dwRenderShard, PCSceneSet *ppScene)
{
   if (ppScene)
      *ppScene = gapScene[dwRenderShard];
   return gapWorld[dwRenderShard];
}


/**************************************************************************************
MapColorPicker - Given an index, returns a color. Use to generate map colors.

inputs
   DWORD       dwIndex - index
returns
   COLORREF - COlor
*/
COLORREF MapColorPicker (DWORD dwIndex)
{
   static COLORREF adwColor[] = {
      RGB (0xff,0xff,0),
      RGB (0,0xff,0xff),
      RGB (0xff,0,0xff),
      RGB (0xff, 0, 0),
      RGB (0, 0xff, 0),
      RGB (0, 0, 0xff),
      RGB (0xff,0x80,0),
      RGB (0,0xff,0x80),
      RGB (0x80,0,0xff),
      RGB (0x80,0xff,0),
      RGB (0,0x80,0xff),
      RGB (0x80,0,0xff)
   };

   return adwColor[dwIndex % (sizeof(adwColor)/sizeof(COLORREF))];
}

/**************************************************************************************
M3DLibraryDir - Fills in the string with M3D library's directory, so knows where
to look.

inputs
   char        *psz - To be filled in
   DWORD       dwLen - Number of characters in buffer
returns
   none
*/
DLLEXPORT void M3DLibraryDir (char *psz, DWORD dwLen)
{
   HKEY  hKey = NULL;
   psz[0] = 0;
   RegOpenKeyEx (HKEY_CURRENT_USER, RegBase(), 0, KEY_READ, &hKey);
   DWORD dw,dwType;
   if (hKey) {
      dw = dwLen;
      RegQueryValueEx (hKey, "LibraryLoc", 0, &dwType, (BYTE*) psz, &dw);
      RegCloseKey (hKey);
   }
   if (!psz[0])
      strcpy (psz, gszAppDir);
}


/**************************************************************************************
M3DDynamicGet - The returns true if the "dynamic" settings are on. If the dynamic
settings are on the DefaultBuildingSettings() will base the building off the user's
current langID, and the view quality settings will be read from the registry. If FALSE
they act as though hard coded.

The dynamic settings defaults to false.
*/
DLLEXPORT BOOL M3DDynamicGet (void)
{
   return gfDynamic;
}


/**************************************************************************************
M3DDynamicSet - Sets whether the dynamic settings will be used. See M3dDynamicGet
*/
DLLEXPORT void M3DDynamicSet (BOOL fDynamic)
{
   gfDynamic = TRUE;
}


/**************************************************************************************
VistaThreadPriorityHack - This function bumbs the thread priority up by one
for circumreality so that when vista spends the first half hour sucking up CPU
for no reason, circumreality works ok on a single-core machine.

This only acts if gfVistaThreadPriorityHack is TRUE (set by calling VistaThreadPriorityHackSet())

inputs
   int      iPriority - Current priority
   int      iIncrease - Amount to increase/decrease, +1 increases thread priority, -1 decreases
returns
   int - New priority.
*/
DLLEXPORT int VistaThreadPriorityHack (int iPriority, int iIncrease)
{
   int iPriInt = 0;
   switch (iPriority) {
      case THREAD_PRIORITY_LOWEST:
         iPriInt = -2;
         break;
      case THREAD_PRIORITY_BELOW_NORMAL:
         iPriInt = -1;
         break;
      case THREAD_PRIORITY_NORMAL:
         iPriInt = 0;
         break;
      case THREAD_PRIORITY_ABOVE_NORMAL:
         iPriInt = 1;
         break;
      case THREAD_PRIORITY_HIGHEST:
         iPriInt = 2;
         break;
      default: // not sure what to do
         break;   // do nothing
   } // switch iPriority

   iPriInt += iIncrease;

   if (gfVistaThreadPriorityHack)
      iPriInt += 1;

   iPriInt = max(iPriInt, -2);
   iPriInt = min(iPriInt, 2);

   switch (iPriInt) {
      case -2:
         return THREAD_PRIORITY_LOWEST;
      case -1:
         return THREAD_PRIORITY_BELOW_NORMAL;
      case 0:
      default: // not sure what to do
         return THREAD_PRIORITY_NORMAL;
      case 1:
         return THREAD_PRIORITY_ABOVE_NORMAL;
      case 2:
         return THREAD_PRIORITY_HIGHEST;
   } // swtich

}


/**************************************************************************************
VistaThreadPriorityHackSet - Turns on/off vista thread priority hack

inputs
   BOOL        fHack - What to set gfVistaThreadPriorityHack to
*/
DLLEXPORT void VistaThreadPriorityHackSet (BOOL fHack)
{
   gfVistaThreadPriorityHack = fHack;
}

// BUGBUG - the rgb numbers in the color selection dialog dont
// update properly when click on color

// BUGBUG - At some point make CListFixed .Get() and .Num() be inlined
