/* oilpaint.h */

#include <escarpment.h>

class CImage {
public:
   CImage (void);
   ~CImage (void);
   CImage *Clone (void);

   HRESULT Clear (DWORD dwX, DWORD dwY);
   HRESULT PixelSet (DWORD dwX, DWORD dwY, COLORREF rgb);
   HRESULT PixelSet (DWORD dwX, DWORD dwY, WORD r, WORD g, WORD b);
   HRESULT PixelGet (DWORD dwX, DWORD dwY, WORD *r, WORD *g, WORD *b);

   HRESULT FromBitmapFile (char *psz, BOOL fShrink = TRUE);
   HRESULT FromBitmap (HBITMAP hBit);
   HBITMAP ToBitmap (HDC hDCOrig = NULL, BOOL fInvert = FALSE);
   HRESULT ToJPEGFile (HWND hWnd, char* psz);
   
   WORD    Gamma (BYTE b) {return m_awGamma[b];};
   BYTE    UnGamma (WORD w);
   COLORREF UnGamma (WORD r, WORD g, WORD b);

   DWORD   DimX (void) {return m_dwX;};
   DWORD   DimY (void) {return m_dwY;};

private:
   DWORD    m_dwX, m_dwY;     // XY dimesnions
   WORD     *m_pawImage;        // image. pawImage[y][x][3]. r,g,b for 3
   WORD     m_awGamma[256];     // gamma conversion table
};

typedef CImage * PCImage;


#define IDH_CANVASLOCATION 111
#define IDH_CANVASSIZE 103
#define IDH_COLORIZE 112
#define IDH_GENERATING 110
#define IDH_GRID 113
#define IDH_HELLO 101
#define IDH_LOADBITMAP 102
#define IDH_LOADSESSION 106
#define IDH_PAINTBASE 115
#define IDH_PAINTCOLOR 108
#define IDH_PAINTDETAILS 116
#define IDH_PAINTNAME 107
#define IDH_PAINTS 109
#define IDH_PRINT 104
#define IDH_SIGN 105
#define IDH_SKETCH 114
#define IDH_CATALOGPAINT 117

BOOL DefPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam);
DWORD GetAndIncreaseUsage (void);
BOOL RegisterIsRegistered (void);
BOOL RegisterPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam);

extern WCHAR gszText[];

/********************************************************************************
Util */
void MemCat (PCMem pMem, double fNum, BOOL fDollars);
void MemZero (PCMem pMem);
void MemCatSanitize (PCMem pMem, PWSTR psz);
void MemCat (PCMem pMem, PWSTR psz);
void MemCat (PCMem pMem, int iNum);
