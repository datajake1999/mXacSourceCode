/*******************************************************************
ResLeak.h - Resource leak detection
*/

#ifndef _RESLEAK_H_
#define _RESLEAK_H_


#ifdef _DEBUG
void ResLeakEnd (void);
BOOL ResLeakDeleteObject (HGDIOBJ hObject, const char *pszFile, int iLine);
BOOL ResLeakDeleteDC(HDC hObject, const char *pszFile, int iLine);
int ResLeakReleaseDC(HWND hWnd, HDC hObject, const char *pszFile, int iLine);
BOOL ResLeakDestroyCursor(HCURSOR hObject, const char *pszFile, int iLine);
BOOL ResLeakDestroyIcon(HICON hObject, const char *pszFile, int iLine);
HDC ResLeakCreateCompatibleDC(HDC hdc, const char *pszFile, int iLine);
HDC ResLeakCreateDC(LPCTSTR lpszDriver,LPCTSTR lpszDevice, LPCTSTR lpszOutput, CONST DEVMODE *lpInitData,
   const char *pszFile, int iLine);
HDC ResLeakGetDC(HWND hWnd,
   const char *pszFile, int iLine);
HPEN ResLeakCreatePen(int fnPenStyle, int nWidth,COLORREF crColor,
   const char *pszFile, int iLine);
HPEN ResLeakCreatePenIndirect(CONST LOGPEN *lplgpn,
   const char *pszFile, int iLine);
HBRUSH ResLeakCreateBrushIndirect(CONST LOGBRUSH *lplb,
   const char *pszFile, int iLine);
HBRUSH ResLeakCreateSolidBrush(COLORREF crColor,
   const char *pszFile, int iLine);
HBITMAP ResLeakCreateBitmap(
  int nWidth,         // bitmap width, in pixels
  int nHeight,        // bitmap height, in pixels
  UINT cPlanes,       // number of color planes used by device
  UINT cBitsPerPel,   // number of bits required to identify a color
  CONST VOID *lpvBits, // pointer to array containing color data
   const char *pszFile, int iLine);
HBITMAP ResLeakCreateBitmapIndirect(
  CONST BITMAP *lpbm,    // pointer to the bitmap data
   const char *pszFile, int iLine);
HBITMAP ResLeakCreateCompatibleBitmap(
  HDC hdc,        // handle to device context
  int nWidth,     // width of bitmap, in pixels
  int nHeight,     // height of bitmap, in pixels
   const char *pszFile, int iLine);
HBITMAP ResLeakCreateDIBitmap(
  HDC hdc,                  // handle to device context
  CONST BITMAPINFOHEADER *lpbmih,  // pointer to bitmap size and
                                   // format data
  DWORD fdwInit,            // initialization flag
  CONST VOID *lpbInit,      // pointer to initialization data
  CONST BITMAPINFO *lpbmi,  // pointer to bitmap color-format data
  UINT fuUsage,              // color-data usage
   const char *pszFile, int iLine);
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
   const char *pszFile, int iLine);
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
   const char *pszFile, int iLine);
HFONT  ResLeakCreateFontIndirect(
  CONST LOGFONT *lplf,   // pointer to logical font structure
   const char *pszFile, int iLine);
HRGN ResLeakCreateRectRgn(
  int nLeftRect,   // x-coordinate of region's upper-left corner
  int nTopRect,    // y-coordinate of region's upper-left corner
  int nRightRect,  // x-coordinate of region's lower-right corner
  int nBottomRect,  // y-coordinate of region's lower-right corner
   const char *pszFile, int iLine);
HANDLE ResLeakLoadImage(
  HINSTANCE hinst,   // handle of the instance containing the image
  LPCTSTR lpszName,  // name or identifier of image
  UINT uType,        // type of image
  int cxDesired,     // desired width
  int cyDesired,     // desired height
  UINT fuLoad,        // load flags
   const char *pszFile, int iLine);

#define  RLEND()    ResLeakEnd()

#ifndef NOREMAPRESLEAK
#undef CreateDC
#undef CreateFont
#undef CreateFontIndirect
#undef LoadImage
#define DeleteObject(x) ResLeakDeleteObject(x,__FILE__,__LINE__)
#define DeleteDC(x) ResLeakDeleteDC(x,__FILE__,__LINE__)
#define ReleaseDC(x,y) ResLeakReleaseDC(x,y,__FILE__,__LINE__)
#define DestroyCursor(x) ResLeakDestroyCursor(x,__FILE__,__LINE__)
#define DestroyIcon(x) ResLeakDestroyIcon(x,__FILE__,__LINE__)
#define CreateCompatibleDC(x) ResLeakCreateCompatibleDC(x,__FILE__,__LINE__)
#define CreateDC(x,y,z,a) ResLeakCreateDC(x,y,z,a,__FILE__,__LINE__)
#define GetDC(x) ResLeakGetDC(x,__FILE__,__LINE__)
#define CreatePen(x,y,z) ResLeakCreatePen(x,y,z,__FILE__,__LINE__)
#define CreatePenIndirect(x) ResLeakCreatePenIndirect(x,__FILE__,__LINE__)
#define CreateBrushIndirect(x) ResLeakCreateBrushIndirect(x,__FILE__,__LINE__)
#define CreateSolidBrush(x) ResLeakCreateSolidBrush(x,__FILE__,__LINE__)
#define CreateBitmap(x,y,z,a,b) ResLeakCreateBitmap(x,y,z,a,b,__FILE__,__LINE__)
#define CreateBitmapIndirect(x) ResLeakCreateBitmapIndirect(x,__FILE__,__LINE__)
#define CreateCompatibleBitmap(x,y,z) ResLeakCreateCompatibleBitmap(x,y,z,__FILE__,__LINE__)
#define CreateDIBitmap(x,y,z,a,b,c) ResLeakCreateDIBitmap(x,y,z,a,b,c,__FILE__,__LINE__)
#define CreateDIBSection(x,y,z,a,b,c) ResLeakCreateDIBSection(x,y,z,a,b,c,__FILE__,__LINE__)
#define CreateFont(x,y,z,a,b,c,d,e,f,g,h,i,j,k) ResLeakCreateFont(x,y,z,a,b,c,d,e,f,g,h,i,j,k,__FILE__,__LINE__)
#define CreateFontIndirect(x) ResLeakCreateFontIndirect(x,__FILE__,__LINE__)
#define CreateRectRgn(x,y,z,a) ResLeakCreateRectRgn(x,y,z,a,__FILE__,__LINE__)
#define LoadImage(x,y,z,a,b,c) ResLeakLoadImage(x,y,z,a,b,c,__FILE__,__LINE__)
#endif

#else // _DEBUG
#define  RLEND()

#endif // _DEBUG


#endif // _RESLEAK_H_
