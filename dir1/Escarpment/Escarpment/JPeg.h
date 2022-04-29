/* jpeg routines */

#ifndef _JPEG_H_
#define _JPEG_H_

typedef HANDLE HDIB;
HDIB BitmapToDIB(HBITMAP hBitmap) ;
WORD SaveDIB(HDIB hDib, LPSTR lpFileName);
WORD DestroyDIB(HDIB hDib);

//HBITMAP JPegOrBitmapLoad (char *szFile);
//DLLEXPORT HBITMAP JPegToBitmap (DWORD dwID, HINSTANCE hInstance);
//BOOL BMPSize (HBITMAP hBmp, int *piWidth, int *piHeight);
//HBITMAP TransparentBitmap (HBITMAP hBit, COLORREF cMatch, DWORD dwColorDist);
//void JPegTransparentBlt (HBITMAP hbmpImage, HBITMAP hbmpMask, HDC hDCInto,
//                     RECT *prInto, RECT *prFrom, RECT *prClip = NULL);
HBITMAP BMPToBitmap (DWORD dwID, HINSTANCE hInstance);


#endif _JPEG_H_
