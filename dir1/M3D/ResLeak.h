/*******************************************************************
ResLeak.h - Resource leak detection
*/

#ifndef _RESLEAK_H_
#define _RESLEAK_H_


#ifdef _DEBUG

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
