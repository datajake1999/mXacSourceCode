/****************************************************************************
ResLeak.cpp - Replacement code for resource/object leaking functions so
can trace leaks.

  BUGFIX - Trace downmemoryleak
begun Nov-23-2000 by Mike Rozak
Copytright2000 Mike Rozak. All rights reserved
*/

#ifdef _DEBUG
#include <windows.h>
#include "escarpment.h"

#define  NOREMAPRESLEAK

#include "resleak.h"


#define GDITYPE_BITMAP     0
#define GDITYPE_BRUSH      1
#define GDITYPE_FONT       2
#define GDITYPE_PEN        3
#define GDITYPE_REGION     4
#define GDITYPE_GDI        5  // border between DGI DeleteObject and non-DeleteObject
#define GDITYPE_HDCCREATE  6  // created HDC
#define GDITYPE_HDCCAP     7  // captured HDC
#define GDITYPE_TIMER      8  // timers
#define GDITYPE_CURSOR     9
#define GDITYPE_ICON       10
#define GDINUM             11 // Number of GDITYPE_XXX defnes

static PSTR gaszGDIType[GDINUM] = {
   "HBITMAP", "HBRUSH", "HFONT", "HPEN", "HRGN",
   "GDI", "HDC(Create)", "HDC(Capture)", "TIMER ID", "HCURSOR", "HICON"
};

typedef struct {
   DWORD    dwType;        // type. GDITYPE_XXX
   HANDLE   hObject;       // handle to the object
   const char *pszFile;    // file it  was created in
   int      iLine;         // line created from
} GDIOBJECT, *PGDIOBJECT;

/* globals */
static BOOL          gfInit = FALSE;   // set to TRUE if things initialized
static CListFixed    glistGDIObject;
static DWORD         gdwMaxCount[GDINUM];
static DWORD         gdwCurCount[GDINUM];


/***********************************************************************
Init - Inita;ozes the debug if not already
*/
static void ResLeakInit (void)
{
   if (gfInit)
      return;
   gfInit = TRUE;
   glistGDIObject.Init (sizeof(GDIOBJECT));
   memset (gdwMaxCount, 0 , sizeof(gdwMaxCount));
   memset (gdwCurCount, 0 , sizeof(gdwCurCount));
}

/***********************************************************************
AddObject - Add object to the list because it was just created by one
of the functions

inputs
   DWORD    dwType - GDITYPE_XXX
   HANDLE   hObject - handle
   const char *pszFile - File
   int      iLine - line
returns
   none
*/
static void AddObject (DWORD dwType, HANDLE hObject, const char *pszFile, int iLine)
{
   ResLeakInit();

   if (!hObject)
      return;

   GDIOBJECT o;
   o.dwType = dwType;
   o.hObject = hObject;
   o.iLine = iLine;
   o.pszFile = pszFile;
   glistGDIObject.Add(&o);

   gdwCurCount[dwType]++;
   gdwMaxCount[dwType] = max(gdwMaxCount[dwType], gdwCurCount[dwType]);
}

/***********************************************************************
FreeObject - Free object.

inputs
   DWORD    dwType - GDITYPE_XXX. If it's GDITYPE_GDI - then will accept
      anything with handle below this.
   HANDLE   hObject - object;
   const char *pszFile - File where it's being deleted
   int      iLine - line
returns
   none
*/
static void FreeObject (DWORD dwType, HANDLE hObject, const char *pszFile, int iLine)
{
   ResLeakInit();

   // find match
   DWORD i;
   PGDIOBJECT pi;
   for (i = 0; i < glistGDIObject.Num(); i++) {
      pi = (PGDIOBJECT) glistGDIObject.Get(i);
      if (pi->hObject != hObject)
         continue;
      if (dwType == GDITYPE_GDI) {
         if (pi->dwType >= GDITYPE_GDI)
            continue;
      }
      else {
         if (dwType != pi->dwType)
            continue;
      }

      // have it
      dwType = pi->dwType; // just in case was GDITYPE_GDI
      glistGDIObject.Remove (i);

      // decrease the count
      gdwCurCount[dwType]--;

      return;
   }

   // if go here couldn't find so error
   char szTemp[256];
   wsprintf (szTemp, "ResLeak: Tried to delete non-existent object %s %x in %s, line %d\r\n",
      gaszGDIType[dwType], (DWORD)hObject, pszFile, iLine);
   OutputDebugString (szTemp);

}

/***********************************************************************
ResLeakEnd - Closes resleak memory, and lists leaks and whatnot

inputs
   none
returns
   none
*/
void ResLeakEnd (void)
{
   // anything left is a leak
   PGDIOBJECT pi;
   DWORD i;
   char szTemp[256];
   BOOL  fDetected = FALSE;

   // show all the max counts
   for (i = 0; i < GDINUM; i++) {
      if (i == GDITYPE_GDI)
         continue;
      wsprintf (szTemp, "ResLeak: Maximum %s around = %d\r\n",
         gaszGDIType[i], gdwMaxCount[i]);
      OutputDebugString (szTemp);
   }

   // show leaks
   for (i = 0; i < glistGDIObject.Num(); i++) {
      if (!fDetected) {
         OutputDebugString ("\r\n\r\n!!! RESOURCE LEAKS DETECTED !!!\r\n\r\n");
         fDetected = TRUE;
      }
      pi = (PGDIOBJECT) glistGDIObject.Get(i);
      wsprintf (szTemp, "ResLeak: LEAK!!! %s %x in %s, line %d\r\n",
         gaszGDIType[pi->dwType], (DWORD)pi->hObject, pi->pszFile, pi->iLine);
      OutputDebugString (szTemp);
   }

}

/***********************************************************************
Various deletion functions from windows
*/
BOOL ResLeakDeleteObject (HGDIOBJ hObject, const char *pszFile, int iLine)
{
   BOOL fRet;
   fRet = DeleteObject (hObject);

   // track this
   FreeObject (GDITYPE_GDI, (HANDLE) hObject, pszFile, iLine);

   return fRet;
}

BOOL ResLeakDeleteDC(HDC hObject, const char *pszFile, int iLine)
{
   BOOL fRet;
   fRet = DeleteDC (hObject);

   // track this
   FreeObject (GDITYPE_HDCCREATE, (HANDLE) hObject, pszFile, iLine);

   return fRet;
}
 
int ResLeakReleaseDC(HWND hWnd, HDC hObject, const char *pszFile, int iLine)
{
   int fRet;
   fRet = ReleaseDC (hWnd, hObject);

   // track this
   FreeObject (GDITYPE_HDCCAP, (HANDLE) hObject, pszFile, iLine);

   return fRet;
}

BOOL ResLeakDestroyCursor(HCURSOR hObject, const char *pszFile, int iLine)
{
   BOOL fRet;
   fRet = DestroyCursor (hObject);

   // track this
   FreeObject (GDITYPE_CURSOR, (HANDLE) hObject, pszFile, iLine);

   return fRet;
}
 
BOOL ResLeakDestroyIcon(HICON hObject, const char *pszFile, int iLine)
{
   BOOL fRet;
   fRet = DestroyIcon (hObject);

   // track this
   FreeObject (GDITYPE_ICON, (HANDLE) hObject, pszFile, iLine);

   return fRet;
}
 
// BUGBUG - Do KillTimer and SetTimer
// BUGBUG - Paintstruct?


/******************************************************************************
Allocation */
HDC ResLeakCreateCompatibleDC(HDC hdc, const char *pszFile, int iLine)
{
   HDC hDC = CreateCompatibleDC (hdc);
   AddObject (GDITYPE_HDCCREATE, (HANDLE) hDC, pszFile, iLine);
   return hDC;
}

HDC ResLeakCreateDC(LPCTSTR lpszDriver,LPCTSTR lpszDevice, LPCTSTR lpszOutput, CONST DEVMODE *lpInitData,
   const char *pszFile, int iLine)
{
   HDC hDC = CreateDC (lpszDriver, lpszDevice, lpszOutput, lpInitData);
   AddObject (GDITYPE_HDCCREATE, (HANDLE) hDC, pszFile, iLine);
   return hDC;
}
 
HDC ResLeakGetDC(HWND hWnd,
   const char *pszFile, int iLine)
{
   HDC hDC = GetDC (hWnd);
   AddObject (GDITYPE_HDCCAP, (HANDLE) hDC, pszFile, iLine);
   return hDC;
}

HPEN ResLeakCreatePen(int fnPenStyle, int nWidth,COLORREF crColor,
   const char *pszFile, int iLine)
{
   HPEN hPen = CreatePen (fnPenStyle, nWidth, crColor);
   AddObject (GDITYPE_PEN, (HANDLE) hPen, pszFile, iLine);
   return hPen;
}

HPEN ResLeakCreatePenIndirect(CONST LOGPEN *lplgpn,
   const char *pszFile, int iLine)
{
   HPEN hPen = CreatePenIndirect (lplgpn);
   AddObject (GDITYPE_PEN, (HANDLE) hPen, pszFile, iLine);
   return hPen;
}

HBRUSH ResLeakCreateBrushIndirect(CONST LOGBRUSH *lplb,
   const char *pszFile, int iLine)
{
   HBRUSH hBrush = CreateBrushIndirect (lplb);
   AddObject (GDITYPE_BRUSH, (HANDLE) hBrush, pszFile, iLine);
   return hBrush;
}

HBRUSH ResLeakCreateSolidBrush(COLORREF crColor,
   const char *pszFile, int iLine)
{
   HBRUSH hBrush = CreateSolidBrush (crColor);
   AddObject (GDITYPE_BRUSH, (HANDLE) hBrush, pszFile, iLine);
   return hBrush;
}

HBITMAP ResLeakCreateBitmap(
  int nWidth,         // bitmap width, in pixels
  int nHeight,        // bitmap height, in pixels
  UINT cPlanes,       // number of color planes used by device
  UINT cBitsPerPel,   // number of bits required to identify a color
  CONST VOID *lpvBits, // pointer to array containing color data
   const char *pszFile, int iLine)
{
   HBITMAP hBit = CreateBitmap (nWidth, nHeight, cPlanes, cBitsPerPel, lpvBits);
   AddObject (GDITYPE_BITMAP, (HANDLE) hBit, pszFile, iLine);
   return hBit;
}

HBITMAP ResLeakCreateBitmapIndirect(
  CONST BITMAP *lpbm,    // pointer to the bitmap data
   const char *pszFile, int iLine)
{
   HBITMAP hBit = CreateBitmapIndirect (lpbm);
   AddObject (GDITYPE_BITMAP, (HANDLE) hBit, pszFile, iLine);
   return hBit;
}

HBITMAP ResLeakCreateCompatibleBitmap(
  HDC hdc,        // handle to device context
  int nWidth,     // width of bitmap, in pixels
  int nHeight,     // height of bitmap, in pixels
   const char *pszFile, int iLine)
{
   HBITMAP hBit = CreateCompatibleBitmap (hdc, nWidth, nHeight);
   AddObject (GDITYPE_BITMAP, (HANDLE) hBit, pszFile, iLine);
   return hBit;
}


HBITMAP ResLeakCreateDIBitmap(
  HDC hdc,                  // handle to device context
  CONST BITMAPINFOHEADER *lpbmih,  // pointer to bitmap size and
                                   // format data
  DWORD fdwInit,            // initialization flag
  CONST VOID *lpbInit,      // pointer to initialization data
  CONST BITMAPINFO *lpbmi,  // pointer to bitmap color-format data
  UINT fuUsage,              // color-data usage
   const char *pszFile, int iLine)
{
   HBITMAP hBit = CreateDIBitmap (hdc, lpbmih, fdwInit, lpbInit, lpbmi, fuUsage);
   AddObject (GDITYPE_BITMAP, (HANDLE) hBit, pszFile, iLine);
   return hBit;
}

HBITMAP ResLeakCreateDIBSection(
  HDC hdc,          // handle to device context
  CONST BITMAPINFO *pbmi,
                    // pointer to structure containing bitmap size, 
                    // format, and color data
  UINT iUsage,      // color data type indicator: RGB values or 
                    // palette indexes
  VOID **ppvBits,    // pointer to variable to receive a pointer to 
                    // the bitmap's bit values
  HANDLE hSection,  // optional handle to a file mapping object
  DWORD dwOffset,    // offset to the bitmap bit values within the 
                    // file mapping object
   const char *pszFile, int iLine)
{
   HBITMAP hBit = CreateDIBSection (hdc, pbmi, iUsage, ppvBits, hSection, dwOffset);
   AddObject (GDITYPE_BITMAP, (HANDLE) hBit, pszFile, iLine);
   return hBit;
}
 
HFONT ResLeakCreateFont(
  int nHeight,             // logical height of font
  int nWidth,              // logical average character width
  int nEscapement,         // angle of escapement
  int nOrientation,        // base-line orientation angle
  int fnWeight,            // font weight
  DWORD fdwItalic,         // italic attribute flag
  DWORD fdwUnderline,      // underline attribute flag
  DWORD fdwStrikeOut,      // strikeout attribute flag
  DWORD fdwCharSet,        // character set identifier
  DWORD fdwOutputPrecision,  // output precision
  DWORD fdwClipPrecision,  // clipping precision
  DWORD fdwQuality,        // output quality
  DWORD fdwPitchAndFamily,  // pitch and family
  LPCTSTR lpszFace,         // pointer to typeface name string
   const char *pszFile, int iLine)
{
   HFONT hFont = CreateFont (nHeight, nWidth, nEscapement, nOrientation, fnWeight,
      fdwItalic, fdwUnderline, fdwStrikeOut, fdwCharSet, fdwOutputPrecision,
      fdwClipPrecision, fdwQuality, fdwPitchAndFamily, lpszFace);
   AddObject (GDITYPE_FONT, (HANDLE) hFont, pszFile, iLine);
   return hFont;
}
     
HFONT  ResLeakCreateFontIndirect(
  CONST LOGFONT *lplf,   // pointer to logical font structure
   const char *pszFile, int iLine)
{
   HFONT hFont = CreateFontIndirect (lplf);
   AddObject (GDITYPE_FONT, (HANDLE) hFont, pszFile, iLine);
   return hFont;
}

HRGN ResLeakCreateRectRgn(
  int nLeftRect,   // x-coordinate of region's upper-left corner
  int nTopRect,    // y-coordinate of region's upper-left corner
  int nRightRect,  // x-coordinate of region's lower-right corner
  int nBottomRect,  // y-coordinate of region's lower-right corner
   const char *pszFile, int iLine)
{
   HRGN hRegion = CreateRectRgn (nLeftRect, nTopRect, nRightRect, nBottomRect);
   AddObject (GDITYPE_FONT, (HANDLE) hRegion, pszFile, iLine);
   return hRegion;
}

HANDLE ResLeakLoadImage(
  HINSTANCE hinst,   // handle of the instance containing the image
  LPCTSTR lpszName,  // name or identifier of image
  UINT uType,        // type of image
  int cxDesired,     // desired width
  int cyDesired,     // desired height
  UINT fuLoad,        // load flags
   const char *pszFile, int iLine)
{
   HANDLE h = LoadImage (hinst, lpszName, uType, cxDesired, cyDesired, fuLoad);
   AddObject ((uType == IMAGE_BITMAP) ? GDITYPE_BITMAP :
   ((uType == IMAGE_CURSOR) ? GDITYPE_CURSOR : GDITYPE_ICON), (HANDLE) h, pszFile, iLine);
   return h;
}
    
#endif _DEBUG


