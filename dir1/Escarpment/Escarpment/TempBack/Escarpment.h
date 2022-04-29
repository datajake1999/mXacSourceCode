/***************************************************************************
Escarpment.h - Header for using the Escarpment API.

Copyright 2000 by Mike Rozak. All rights reserved
*/

#ifndef _ESCARPMENT_H_
#define _ESCARPMENT_H_

#include <stdio.h>

#ifndef DLLEXPORT
#define  DLLEXPORT      __declspec (dllexport)
#endif

/*********************************************************************************************
tools.h */
typedef unsigned __int64 QWORD;

/* CMem - Memory object */
class DLLEXPORT CMem {
   public:
      PVOID    p;       // memory. Allocated to size m_dwAllocated
      DWORD    m_dwAllocated;   // amount of memory allocated
      DWORD    m_dwCurPosn;     // current position

      CMem (void);
      ~CMem (void);
      void* operator new (size_t);
      void  operator delete (void*);

      BOOL  Required (DWORD dwSize);
      DWORD Malloc (DWORD dwSize);
      BOOL  StrCat (PCWSTR psz, DWORD dwCount = (DWORD)-1);   // if count==-1 then uses a strlen
      BOOL  CharCat (WCHAR c);
   };

typedef CMem * PCMem;


/* CListVariable - List of variable sized elements*/
class DLLEXPORT CListVariable {
private:
   CMem     m_apv;      // array of pointers to LISTINFO. All pointers must be freed
   DWORD    m_dwElem;   // number of elements in list

public:
   CListVariable (void);
   ~CListVariable (void);
   void* operator new (size_t);
   void  operator delete (void*);

   DWORD    Add (PVOID pMem, DWORD dwSize);
   BOOL     Insert (DWORD dwElem, PVOID pMem, DWORD dwSize);
   BOOL     Set (DWORD dwElem, PVOID pMem, DWORD dwSize);
   BOOL     Remove (DWORD dwElem);
   DWORD    Num (void);
   PVOID    Get (DWORD dwElem);
   DWORD    Size (DWORD dwElem);
   void     Clear (void);
   BOOL     Merge (CListVariable *pMerge);
   BOOL     FileWrite (FILE *pf);
   BOOL     FileRead (FILE *pf);
};
typedef CListVariable * PCListVariable;


/* CListFixed - List of fixed sized elements*/
class DLLEXPORT CListFixed {
private:
   DWORD    m_dwElemSize;
   CMem     m_apv;      // array of elements. Size = m_dwElemSize
   DWORD    m_dwElem;   // number of elements in list

public:
   CListFixed (void);
   ~CListFixed (void);
   void* operator new (size_t);
   void  operator delete (void*);

   BOOL     Init (DWORD dwElemSize);
   BOOL     Init (DWORD dwElemSize, PVOID paElems, DWORD dwElems);

   DWORD    Add (PVOID pMem);
   BOOL     Insert (DWORD dwElem, PVOID pMem);
   BOOL     Set (DWORD dwElem, PVOID pMem);
   BOOL     Remove (DWORD dwElem);
   DWORD    Num (void);
   PVOID    Get (DWORD dwElem);
   void     Clear (void);
   BOOL     Merge (CListFixed *pMerge);
   BOOL     Truncate (DWORD dwElems);
};
typedef CListFixed * PCListFixed;


/* CBTree - binary tree */
class DLLEXPORT CBTree {
private:
   CListVariable  m_list;
   DWORD       FindNum (WCHAR *pszKey);
   BOOL        AddToNodes (DWORD dwElemNum, WCHAR *pszKey);

public:
   CBTree (void);
   ~CBTree (void);
   void* operator new (size_t);
   void  operator delete (void*);

   BOOL        m_fIgnoreCase; // if set to TRUE (default) then case is isgored

   BOOL        Add (WCHAR *pszKey, PVOID pData, DWORD dwSize);
   PVOID       Find (WCHAR *pszKey, DWORD *pdwSize = NULL);
   BOOL        Remove (WCHAR *pszKey);
   DWORD       Num (void);
   WCHAR *     Enum (DWORD dwNum);
   void        Clear (void);
   BOOL        FileWrite (FILE *pf);
   BOOL        FileRead (FILE *pf);

   };

typedef CBTree * PCBTree;



/**********************************************************************************
TextWrap */
/* TWFONTINFO - Describes the text formatting of the character */
typedef struct {
   HFONT    hFont;      // hfont to use.
   int      iAbove;     // height of font (in pixels) above the baseline
   int      iBelow;     // height of font (in pixels) below the baseline
   DWORD    dwJust;     // 0 for left, 1 = center, 2 = right
   COLORREF cText;      // color of the text
   COLORREF cBack;      // color of the background; -1 == transparent
   QWORD    qwLinkID;   // if non-0, it's a clickable link. This is the ID.
   int      iIndentLeftFirst; // left indent (pixels) on the first line
   int      iIndentLeftWrap;  // left indent (pixels) on wrapped lines
   int      iIndentRight;  // indent on the right
   int      iTab;       // tabs occur every iTab pixels
   int      iLineSpacePar; // number of pixels between lines separating paragraphs
                           // when passed into CTextCache::Need, this will convert negative
                           // numbers to 1/100th the height of the font. A good seeing is -40 or -180.
   int      iLineSpaceWrap;   // number of pixels between lines separating word wraps
                           // when passed into CTextCache::Need, this will convert negative
                           // numbers to 1/100th the height of the font. A good seeing is -40.
   int      iSuper;     // elevate the baseline iSuper pixels above/below the line baseline
   DWORD    dwAutoNumber;  // automatic numbering bulleting. 0 for none. 0x00001 for basic numbers. 0x10001 for basic bullets.
   DWORD    dwFlags;    // left empty right now
                        // BUGBUG - 2.0 - eventually shadow, outline, emboss, engrave, strike through, double strike through
} TWFONTINFO, *PTWFONTINFO;



/**********************************************************************************
FontCache */
class DLLEXPORT CFontCache {
private:
   CListFixed     m_listHFONT;   // list containing hfonts
   CListFixed     m_listLOGFONT;  // list containing LOGFONT structures. One per HFONT
   CListVariable  m_listTWFONTINFO; // list of font info

public:
   CFontCache (void);
   ~CFontCache (void);
   void* operator new (size_t);
   void  operator delete (void*);

   TWFONTINFO *Need (HDC hDC, TWFONTINFO *pfi, int iPointSize, DWORD dwFlags, PCWSTR pszFace);
   void     Clear (void);
};

#define  FCFLAG_BOLD       0x0001
#define  FCFLAG_ITALIC     0x0002
#define  FCFLAG_UNDERLINE  0x0004
#define  FCFLAG_STRIKEOUT  0x0008

typedef CFontCache * PCFontCache;



/**********************************************************************************
MMLParse */
/* CMMLNode - node resulting from parse of MML text */
#define MMLCLASS_ELEMENT               0
#define MMLCLASS_MACRO                 1
#define MMLCLASS_PARSEINSTRUCTION      2

class DLLEXPORT CMMLNode {
private:
   CBTree      m_treeAttrib;  // tree of the attributes. Data is the attribute string
   CMem        m_memName;     // name string
   CListVariable m_listContent;  // where the contet is stored with MMCONTENT structures

public:
   CMMLNode (void);
   ~CMMLNode (void);
   void* operator new (size_t);
   void  operator delete (void*);

   DWORD       m_dwType;   // one of MMLCLASS_XXX
   BOOL        m_fDirty;   // if it's dirty. Only set to dirty if it doesn't have a parent
   CMMLNode    *m_pParent; // parent node

   BOOL        AttribSet (WCHAR *pszAttrib, WCHAR *pszValue);
   WCHAR*      AttribGet (WCHAR *pszAttrib);
   BOOL        AttribDelete (WCHAR *pszAttrib);
   BOOL        AttribEnum (DWORD dwNum, PWSTR *ppszAttrib, PWSTR *ppszValue);
   DWORD       AttribNum (void);

   WCHAR *     NameGet (void);
   BOOL        NameSet (WCHAR *pszName);

   BOOL        ContentAdd (WCHAR *psz);
   BOOL        ContentAdd (CMMLNode *pNode);
   BOOL        ContentAddCloneNode (CMMLNode *pNode);
   CMMLNode*   ContentAddNewNode (void);
   BOOL        ContentInsert (DWORD dwIndex, WCHAR *psz);
   BOOL        ContentInsert (DWORD dwIndex, CMMLNode *pNode);
   BOOL        ContentRemove (DWORD dwIndex, BOOL fDelete = TRUE);
   BOOL        ContentEnum (DWORD dwIndex, PWSTR *ppsz, CMMLNode **ppNode);
   DWORD       ContentNum (void);
   DWORD       ContentFind (PWSTR psz, PWSTR pszAttrib = NULL, PWSTR pszValue = NULL);

   CMMLNode*   Clone (void);
   void        DirtySet (void);
};
typedef CMMLNode * PCMMLNode;

/* CEscError - for returning parse errors */
class DLLEXPORT CEscError {
private:
   CMem     m_memDesc;  // description
   CMem     m_memSurround; // surrounding text

public:
   CEscError (void);
   ~CEscError (void);
   void* operator new (size_t);
   void  operator delete (void*);

   DWORD    m_dwNum;   // error num
   PCWSTR   m_pszDesc;     // description
   PCWSTR   m_pszSurround;    // surrounding text
   DWORD    m_dwSurroundChar; // surroundig character
   PCWSTR   m_pszSource;   // original source text

   void     Set (DWORD dwNum = 0, WCHAR *pszDesc = NULL, WCHAR *pszSurround = NULL,
                     DWORD dwSurroundChar = 0, WCHAR *pszSource = NULL);

};
typedef CEscError * PCEscError;



/* functions */
DLLEXPORT PCMMLNode ParseMML (PWSTR psz, HINSTANCE hInstance, PVOID pCallback,
                    PVOID pCallbackParam, PCEscError pError, BOOL fDoMacros = TRUE);



/***************************************************************************
TextBlock */
class CEscPage;
class CEscControl;

typedef struct {
   TWFONTINFO     fi;      // all info from here used except hFont, iAbove, iBelow
   WCHAR          szFont[LF_FACESIZE];
   int            iPointSize; // point size to use
   DWORD          dwFlags; // One or more ofFCFLAG_XXX
} IFONTINFO, *PIFONTINFO;

/* TABLEINFO - default info for table. Can override for each cell */
typedef struct {
   int         iWidth;  // width (in pixels). Could have been entered in percent of display
   int         iHeight; // min height (in pixels). Could have been entered as percent of display width
   COLORREF    cBack;   // default background color, -1 for clear
   COLORREF    cEdge;   // color of the edge
   DWORD       dwAlignHorz;   // alignment of cell-data horizontal, 0=left, 1=center, 2=right
   DWORD       dwAlignVert;   // alignment of cell-data vertical, 0=top, 1=center, 2=right
   int         iEdgeOuter; // size of outer edge
   int         iEdgeInner; // size of cells no inner edge
   int         iMarginTopBottom; // margin on top & bottom
   int         iMarginLeftRight; // margin on the left & right
} TABLEINFO, *PTABLEINFO;



class DLLEXPORT CEscTextBlock {
private:
   HDC            m_hDC;         // HDC to use
   HINSTANCE      m_hInstance;   // instance for resources
   CListFixed     m_listText;    // list of WCHARs, which is the text generated from CMMLNode
   CListFixed     m_listPTWFONTINFO;   // list of PTWFONTINFO, for font info for each character
   CListFixed     m_listTWOBJECTINFO;  // list of objects
   BOOL           m_fUseOtherError;    // if TRUE, using a differnet objects error so dont free on destry
   BOOL           m_fUseOtherFont;     // if TRUE using a different objects font cache, so dont free on destroy
   BOOL           m_fUseOtherControlCache;   // if TRUE, using a different control cache, so don't clear it
   int            m_iWidth;      // looking for this
   CEscPage       *m_pPage;      // page object. Needed for the controls
   CEscControl    *m_pControl;    // control object - parent of controls added. Might be null.
   CListFixed     *m_plistPCESCCONTROL;  // list of controls that should reuse
   PCMMLNode      m_pNode;       // start node being used
   BOOL           m_fRootNodeNULL;  // interpret root node as NULL, interpret text & other nodes

   void           OtherError (PCEscError pError);
   void           OtherControlCache (PCListFixed pList);
   BOOL           AddChar (WCHAR c, IFONTINFO *pIFontInfo = NULL);
   BOOL           AddString (PWSTR psz, IFONTINFO *pIFontInfo = NULL);
   BOOL           AddObject (QWORD qwID, int iWidth, int iHeight, DWORD dwText, PIFONTINFO pfi = NULL);
   BOOL           Tag (PCMMLNode pNode);
   BOOL           TagStretch (BOOL fStart);
   BOOL           TagSection (PCMMLNode pNode);
   BOOL           TagSimpleText (PCMMLNode pNode, BOOL *pfTagKnown, BOOL fPretend = FALSE);
   BOOL           TagUnknown (PCMMLNode pNode);
   BOOL           TagMain (PCMMLNode pNode);
   BOOL           TagTable (PCMMLNode pNode);
   BOOL           TagList (PCMMLNode pNode, BOOL fNumber);
   void           FillInTABLEINFO (PCMMLNode pNode, TABLEINFO *pti, int iPercent);
   BOOL           TagRow (PCMMLNode pNode, TABLEINFO *ptai, int *piHeight,
                            CListVariable *plte, CListFixed *plop,
                            BOOL fTopEdge, BOOL fBottomEdge);
   BOOL           TagControl (PCMMLNode pNode, PVOID pCallback);
   void           PullOutTables (void);
   BOOL           ConvertLinksToControls (void);
   void           FreeObjects (void);
   void           FreeControls (void);

public:
   CEscTextBlock (void);
   ~CEscTextBlock (void);
   void* operator new (size_t);
   void  operator delete (void*);

   IFONTINFO      m_fi;             // current font information. Internal functions can temporarily change this, but must restore it
   CEscError      *m_pError;       // where error return information is stored
   CListVariable  *m_plistTWTEXTELEM; // filled with locations of text element
   CListFixed     *m_plistTWOBJECTPOSN;  // filled with object position lists
   CFontCache     *m_pFontCache;   // font cache
   int            m_iCalcWidth;  // width of all generated objects
   int            m_iCalcHeight; // height of all genreted objects
   BOOL           m_fDeleteCMMLNode;   // if TRUE, deletes stored node (from Interpret). Defaults to FALSE
   PCMMLNode      m_pNodePageInfo;  // filled with a pointer to the <pageinfo> node when interpret or reinterpret
   BOOL           m_fFoundStretch;  // set to true if found stretch indicators

   BOOL           Init (void);
   BOOL           Interpret (CEscPage *pPage, CEscControl *pControl,
      CMMLNode *pNode, HDC hDC,
      HINSTANCE hInstance, int iWidth, BOOL fDontClearFont = FALSE,
      BOOL fRootNodeNULL = FALSE);
   BOOL           PostInterpret (int iDeltaX, int iDeltaY, RECT *prFull);
   BOOL           ReInterpret (HDC hDC, int iWidth, BOOL fDontClearFont = FALSE);
   BOOL           Stretch (int iStretchTo);
   BOOL           Paint (HDC hDC, POINT *pOffset = NULL, RECT *prClip = NULL, RECT *prScreen = NULL,
                  RECT *prTotalScreen = NULL);
   void           OtherFontCache (PCFontCache pFontCache);
   int            SectionFind (PWSTR psz);
   PWSTR          SectionFromY (int iY, BOOL fReturnFirst = TRUE);
   int            FindBreak (int iYMin, int iYMax);
};

typedef CEscTextBlock * PCEscTextBlock;









/****************************************************************8
3d Rendering */


#define  PI    (3.14159265358979323846)
#define  TWOPI (2.0 * PI)
#define  INFINITY (1.0E30)

// ShapeAxis flags
#define  AXIS_FOG       0x00001     // draw graph, and then set fog. If you draw objects
                                    // after this, they will be fogged. However, if this
                                    // is on, you may want to kill fog later on
#define  AXIS_DISABLEGIRD 0x00002   // disabled the grid-lines on the axis
#define  AXIS_DISABLEINTERNALGIRD 0x00004 // disables the internal grid, but leaves
                                    // external grid lines

typedef double Matrix[4][4];
typedef double pnt[4];

typedef struct _MatrixStruct {
   Matrix   m;
   struct _MatrixStruct *next;
} MatrixStruct;


DLLEXPORT void RotationMatrix (Matrix m, double xrot, double yrot, double zrot);
DLLEXPORT void MakeMatrixSquare (Matrix m);

inline void CopyPnt (const pnt source, pnt dest)
{
   memcpy (dest, source, sizeof(pnt));
}

/*******************************************************************
MultplyMatices - Multiply two matricies together 

inputs
   Matrix   left, right - two to multiply together
   Matrix   newM - where the result is to be stored.
            new != left and new!=right
*/
inline void MultiplyMatrices (const Matrix left, const Matrix right, Matrix newM)
{
   int   i;
   int   j;

   for (i = 0; i < 4; i++)
      for (j = 0; j < 4; j++)
         newM[j][i] = left[0][i] * right[j][0] +
                     left[1][i] * right[j][1] +
                     left[2][i] * right[j][2] +
                     left[3][i] * right[j][3];
}

typedef PSTR (__cdecl *PRENDERAXISCALLBACK )(PVOID pInstance, DWORD dwAxis, double fValue);


class DLLEXPORT CRender {
public:

   CRender (void);
   ~CRender (void);
   void* operator new (size_t);
   void  operator delete (void*);

   void Init (HDC hDC, RECT *pRect); // initializes the graphics system over again to default values
   void InitForControl (HDC hDC, RECT *prHDC, RECT *prScreen, RECT *prTotal);
   void Commit (HDC hDCNew, RECT *prHDC, BOOL fObjectsOnly = TRUE);  // cause the image to draw the the HDC if it isn't already
   void Clear (float fZValue = - (float)INFINITY);   // erases the display
   void MatrixSet (const Matrix m);
   void MatrixGet (Matrix m);
   void MatrixMultiply (const Matrix m);
   void MatrixPush (void)
         { mpush(); };
   DWORD MatrixPop (void)
         { return mpop(); };
   void Translate (double x = 0.0, double y = 0.0, double z = 0.0);   // translates in x,y,z
   void Rotate (double fRadians, DWORD dwDim);  // Rotate. dwdim = 1 for x, 2 for y, 3 for z
   void Scale (double fScale);   // universally scale
   void Scale (double x, double y, double z);   // scale in x, y, z
   void TransRot (const pnt p1, const pnt p2);
   void LightVector (const pnt p);
   void FogRange (double zStart, double zEnd);   // set the fog intensity based upon this point
   void ShapePolygon (DWORD dwCount, pnt paVert[], pnt paColor[]);
   void ShapeBox (double x = 1.0, double y = 1.0, double z = 1.0, double *pColor = NULL);  // draws a box
   void ShapeBox (pnt apCorner[8], double *pColor = NULL);
   void ShapeFlatPyramid (double fBaseX,double fBaseY, double fTopX, double fTopY,
                                double fZ, double *pColor = NULL);
   void ShapeDeepTriangle (double fBaseX,double fBaseY, double fTopX, double fTopY,
                                double fZ, double *pColor = NULL);
   void ShapeDeepArrow (double fX, double fY, double fZ, double *pColor = NULL);
   void ShapeDeepFrame (double fX, double fY, double fZ,
                              double fFrameBase, double fFrameTop, double *pColor = NULL);
   void ShapeDot (pnt p);
   void ShapeLine (DWORD dwCount, double *pafPoint, double *pafColor = NULL, BOOL fArrow = FALSE);
   void ShapeArrow (DWORD dwCount, double *pafPoint, double fWidth = .1, BOOL fTip = TRUE);
   void ShapeMeshSurface (void);
   void ShapeMeshVectors (DWORD dwX, DWORD dwY, double *pafVectors);
   void ShapeMeshVectorsFromBitmap (HBITMAP hBit);
   void ShapeTeapot (void);
   void ShapeAxis (pnt p1, pnt p2, pnt pu1, pnt pu2,
      PSTR *papszAxis = NULL, DWORD dwFlags = 0,
      PRENDERAXISCALLBACK pCallback = NULL, PVOID pCallbackUser = NULL);
   BOOL BoundingBox (double x1, double y1, double z1,
                           double x2, double y2, double z2, DWORD *pdwFacets = NULL, BOOL fAllTrans = TRUE);
   BOOL BoundingBox (DWORD dwCount, double *pafPoints, DWORD *pdwFacets = NULL, BOOL fAllTrans = TRUE);
   void ColorMapFree (void);
   void ColorMapFromBitmap (HBITMAP hBit);
   void ColorMap (DWORD dwWidth, DWORD dwHeight, const DWORD *padwData);
   void BumpMapFree (void);
   void BumpMapFromBitmap (HBITMAP hBit, DWORD dwColor);
   void BumpMap (DWORD dwWidth, DWORD dwHeight, const double *pafData);
   void BumpMapApply (void);
   BOOL ObjectGet (int iX, int iY, DWORD *pdwMajor, DWORD *pdwMinor);
   void GlassesLeft (double fEyeDistance);
   void GlassesRight (double fEyeDistance);
   void GlassesMerge (CRender *pRight);
   void MeshFromPoints (DWORD dwType, DWORD dwX, DWORD dwY,
           double *paPoints,
           double *paNormals = NULL, double *paEast = NULL, double *paNorth = NULL,
           BOOL fCheckBoundary = TRUE);
   void MeshBezier (DWORD dwType, DWORD dwX, DWORD dwY, double *paPoints);
   void MeshEllipsoid (double x, double y, double z);
   void MeshSphere (double x) {MeshEllipsoid (x,x,x); };
   void MeshPlane (double x = 1.0, double y = 1.0);
   void MeshRotation (DWORD dwNum, double *pafPoints, BOOL fBezier = TRUE);
   void MeshFunnel (double fHeight = 1.0, double fWidthBase = 1.0, double fWidthTop = 0.0);
   void MeshTube (double fHeight = 1.0, double fWidth = 1.0)
                  { MeshFunnel (fHeight, fWidth, fWidth); };
   void TextFontSet (LOGFONT *plf, double fHeight, BOOL fPixels);
   void Text (const char *psz, pnt pLeft, pnt pRight,
                        int iHorz = 0, int iVert = 0);

   // public variables that can be modified
   double      m_fLightIntensity;   // intensity of light. If intensity + background > 1.0 get saturation
   double      m_fLightBackground;  // intensity of light. If intensity + background > 1.0 get saturation
   BOOL        m_fWireFrame;        // if true, draw everything wireframe
   BOOL        m_fDrawNormals;      // if true, draw normals off mesh. mostly for debug purposes
   BOOL        m_fBackCull;         // if true, do back-culling.
   BOOL        m_fDrawFacets;       // if true, draw shapes to look faceted. else, will be smoothed with garaud.
                                    // faceted is slightly faster
   BOOL        m_fFogOn;            // if TRUE, fog is on. see m_fFogDistance
   double      m_fFogMaxDistance;   // distance (in positive z) at which things are completely fogged out
                                    // should not be 0
   double      m_fFogMinDistance;   // distance (in positive z) at which things start fogging out
                                    // should not be 0. Should be < m_fFogMaxDistance
   pnt         m_DefColor;          // default color for some of the objects
   pnt         m_BackColor;         // background (and fog) color
   DWORD       m_dwColorMode;       // 0 for full color, 1 for gray scale, 2 for B&W only
                                    // callsing GlassesLeft/Right will set to 3 for left, 4 for right
   DWORD       m_dwMaxFacets;       // maximum facets to allow per side of automatically
                                    // smoothing objects. Starts at 32
   double      m_fPixelsPerFacet;   // number of pixels per facet that aiming for. Start 10.
                                    // smaller number makes smoother shapes
   DWORD       m_dwMajorObjectID;   // current # to be used when plotting, for major object #
   DWORD       m_dwMinorObjectID;   // current # to be used when plotting, for minor object #.
                                    // this value is changed by meshes
   double      m_fBumpScale;        // amount to scale bump sizes by
   double      m_fVectorScale;      // amount to scale ShapeMeshVectors sizes by
   BOOL        m_fVectorArrows;     // if TRUE, vectors have arrows drawn on the end
   BOOL        m_fDontInterpVectors;   // if true, don't interpolate vectors when doing ShapeMeshVector
   DWORD       m_dwMaxAxisLines;    // maximum lines along an axis in the ShapeAxis.

private:
   Matrix      CTM;     // objects matrix
   Matrix      mainMatrix; // = perMatrix * camera * CTM
   MatrixStruct *TOS;   // matrix at the top of stack
   Matrix      persMatrix; // perspective matrix
   Matrix      scaleMatrix; // scaling matrix for windows
   Matrix      perspNScale;   // combination of perspective and scaling
   Matrix      camera;  // camera matrix
   Matrix      normalMatrix;  // multiply your normals by this to calc them
   pnt         plfrom, plat, plup;  // vectors for use in camera model commands
   pnt         pwfore, pwback;
   pnt         psfore, psback;
   double      dell; // distance to foreground object
   double      dfov; // field of view
   double      aspect;  // aspect ratio
   pnt         last; // last moved to

   int         ixl, ixr;   // x coord of left-most and right-most pixel
   int         iyb, iyt;   // y coord of top-most and bottom-most pixel
   int         izn, izf;   // greatest and least display depth. There isn't so = 0
   double      centerx, centery; // center of the viewport, screen coords
   double      scalex, scaley;   // pixels per length 1.0
   HDC         m_hDC;      // device context to draw into, eventually blt into
   RECT        m_rHDC;     // rectangle to draw into, eventuall blt into
   RECT        m_rDraw;    // rectangle to draw into, corner at 0,0
   RECT        m_rClip;    // rectangle to clip pixels to. See SetHDC for description
   POINT       m_pClipSize;   // .x is width, .y is height
   HDC         m_hDCDraw;  // HDC to draw into
   HBITMAP     m_hBitDrawOld; // old bitmap selected
   HBITMAP     m_hBitDraw; // bitmap to draw into
   BITMAP      m_bm;       // bitmap info if drawing directly into it
   BOOL        m_fWriteToMemory; // if true, write to memory
   float       *m_pafZBuf;  // z buffer. [m_rDraw.top][m_rDraw.right]
   DWORD       *m_padwObject; // object buffer. [m_rDraw.top][m_rDraw.right][2] - major first, then minor

   pnt         old;  // old move point
   pnt         oldColor;   // old color
   pnt         m_LightVector; // light vector. Normalized to 1
   DWORD*      m_pdwColorMap;  // color map to use. NULL if ther isn't one
   DWORD       m_dwColorMapX; // color map width in pixels
   DWORD       m_dwColorMapY; // color map height in pixels
   double*     m_pfBumpMap;   // bump map to use. NULL if ther isn't one. [m_dwBumpMapY][m_dwBumpMapX][1]
   DWORD       m_dwBumpMapX;  // bump map width in pixels
   DWORD       m_dwBumpMapY;  // bump map height in pixels

   DWORD       m_dwMeshX;     // size of mesh array
   DWORD       m_dwMeshY;
   DWORD       m_dwMeshStyle;  // how the edges connect.
                              // 0 - ends are unconnected to one another
                              // 1 - ends on left and right (across axis) are connected
                              // 2 - ends on top and bottom (down axis) are connected
                              // 3 - all ends connected
   BOOL        m_fMeshCheckedBoundary;  // if TRUE, already checked the mesh boundary when
                                 // genreated a spehere, or whatever
   BOOL        m_fMeshShouldHide;   // if m_fCheckedBoundary, and this, then shouldn't draw the mesh surface
   double*     m_pafMeshPoints;  // array of points m_dwMeshY x m_dwMeshX x 4
   double*     m_pafMeshNormals; // normals
   double*     m_pafMeshEast;    // vector pointing east
   double*     m_pafMeshNorth;   // vector pointing north

   double      m_fBumpVectorLen; // the length of a 1.0 vector/bump after trans1() is applied

   HFONT       m_hFont;          // font to draw with
   LOGFONT     m_lf;             // logical font to use
   BOOL        m_fFontPixels;    // TRUE then height is in pixels, else subject to z
   double      m_fFontHeight;    // height
   DWORD       m_dwFontLastHeightPixels;  // current pixel height of m_hFont


   void MakeMainMatrix (void);
   void mtransform (pnt old, pnt newP);
   void mtrans1 (pnt old, pnt newP);
   void mtrans2 (const pnt old, pnt newP);
   void mtransNormal (pnt old, pnt newP, BOOL fNormalize = TRUE);
   void mtranslate (double dx, double dy, double dz);
   void mscale (double sx, double sy, double sz);
   void mrotate (double angle, int axis);
   void mperspect (double znear, double zfar_inv);
   void lfrom (const pnt p);
   void lat (const pnt p);
   void lup (const pnt p);
   void wfore (const pnt p);
   void wback (const pnt p);
   void sfore (const pnt p);
   void sback (const pnt p);
   void fov (double p);
   void ell (double p);
   void FOHBOT (const pnt lup, const pnt wfore, const pnt wback,
      const pnt sfore, const pnt sback, double fov, double ell);
   void build_camera (void);
   void build_FOHBOT (void);
   void SetScaleMatrix (void);
   void mident(void);
   void mpush (void);
   DWORD mpop (void);
   void stkinit (void);
   void initl1 (void);
   void gsinit (void);
   void endl1 (void);
   void viewpt (double xl, double xr, double yb, double yt);
   void winviewpt (void);
   void DevIndepToDevDep (double x, double y, double *xx, double *yy);
   void mdl1 (int ix1, int iy1, int ix2, int iy2, pnt colorStart, pnt colorEnd);
   BOOL Clip (pnt start, pnt end, pnt colorStart, pnt colorEnd, DWORD *pdwClipSides = NULL,
      BOOL fOnlyZ = FALSE);
   void mdl2 (const pnt start, const pnt end, pnt colorStart, pnt colorEnd, BOOL fArrow = FALSE);
   void mdl3 (pnt start, pnt end, pnt colorStart, pnt colorEnd, BOOL fArrow = FALSE);
   void SetHDC (HDC hDC, RECT *pRect, RECT *pClip = NULL, float fZValue = - (float)INFINITY); // should be called at initialization
   void DrawPixel (int iX, int iY, float fZ, DWORD rgb);
   void DrawLine (int x1, int y1, int x2, int y2,
                  double z1, double z2,
                  pnt RGB1, pnt RGB2);
   void DrawHorzLine (double x1, double x2, int y, double z1, double z2,
                   pnt c1, pnt c2);
   void FillBetweenLines (double y1, double y2,
                                double x1Left, double x2Left, double x1Right, double x2Right,
                                double z1Left, double z2Left, double z1Right, double z2Right,
                                pnt c1Left, pnt c2Left,
                                pnt c1Right, pnt c2Right);
   void DrawClippedFlatTriangle (pnt apVert[3], pnt apColor[3]);
   void DrawFlatTriangle (pnt apVert[3], pnt apColor[3]);
   void DrawFlatPoly (DWORD dwCount, pnt apVert[], pnt apColor[]);
   void CalcNormal (const pnt a, const pnt b, const pnt c, pnt n);
   void CalcNormal (const pnt a1, const pnt a2, const pnt b1, const pnt b2, pnt n);
   void CalcColor (const pnt pColorOrig, const pnt n, double z, pnt pColorNew);
   BOOL ShouldHideIt (const pnt a, const pnt b, const pnt c);
   void Move (double x, double y, double z, DWORD dwRGB); // move to a point
   void Draw (double x, double y, double z, DWORD dwRGB); // draw to a point
   void ApplyColorMode (pnt c, BOOL fWhiteInBNW = FALSE);
   void ApplyFog (pnt c, double z);
   void ApplyTransform (DWORD dwCount, pnt p[]);  // apply translation and rotation transforms to point. No perspective
   void MeshFree (void);
   void MeshGenerateNormals (void);
   void MeshGenerateNorthEast (void);
   void MapCheck (DWORD dwFacets, DWORD *pdwX, DWORD *pdwY);
   void BulkNormalize (DWORD dwNum, double *pafNorm);
   double *BezierArray (DWORD dwNumIn, double *pafPointsIn, BOOL fLoop, DWORD dwNumOut);
   DWORD Bezier (double *pafPointsIn, double *pafPointsOut, DWORD dwDepth);
   void TextDraw (const char *psz, HFONT hFont, int iVert, int iHorz, int iRot,
                        int iX, int iY, double z, RECT *prSize = NULL);
   void TextFontGenerate (double z);

};

typedef CRender * PCRender;



/* CBitmapCache - bitmap cache */
class DLLEXPORT CBitmapCache {
private:
   CListFixed     m_listBMCACHE; // list of BMCACHE strucutres

   DWORD Cache (PWSTR psz, DWORD dwID, BOOL fJPEG,HINSTANCE hInstance,
      HBITMAP *phbmpBitmap, HBITMAP *phbmpMask = NULL,
      COLORREF cMatch = (DWORD)-1, DWORD dwColorDist = 10);

public:
   CBitmapCache (void);
   ~CBitmapCache (void);
   void* operator new (size_t);
   void  operator delete (void*);

   void CacheCleanUp (BOOL fForce = FALSE);
   DWORD CacheFile (PWSTR psz, HBITMAP *phbmpBitmap, HBITMAP *phbmpMask = NULL,
      COLORREF cMatch = (DWORD)-1, DWORD dwColorDist = 10);
   DWORD CacheResourceBMP (DWORD dwID, HINSTANCE hInstance, HBITMAP *phbmpBitmap, HBITMAP *phbmpMask = NULL,
      COLORREF cMatch = (DWORD)-1, DWORD dwColorDist = 10);
   DWORD CacheResourceJPG (DWORD dwID, HINSTANCE hInstance, HBITMAP *phbmpBitmap, HBITMAP *phbmpMask = NULL,
      COLORREF cMatch = (DWORD)-1, DWORD dwColorDist = 10);
   BOOL CacheRelease (DWORD dwID);
};

typedef CBitmapCache * PCBitmapCache;
DLLEXPORT PCBitmapCache EscBitmapCache (void);



class CEscControl;
class CEscPage;
class CEscWindow;

#define  IDC_HANDCURSOR             0x3453   // cutom cursor for click-on hand
#define  IDC_NOCURSOR               0x3454   // custom cursor

/***************************************************************************
Messages common to both Controls & Pages
*/
#define  ESCM_CONTROLBASE     0x1000
#define  ESCM_PAGEBASE        0x2000
#define  ESCM_EITHER          0x3000
#define  ESCM_NOTIFICATION    0x4000
#define  ESCM_MESSAGE         0x500
#define  ESCM_USER            0x80000000     // where user messages go after.


/**************************************************************************
Notifications from controls
*/
#define  ESCN_BUTTONPRESS     (ESCM_NOTIFICATION+1)
// Sent by button when its pressed.
typedef struct {
   CEscControl    *pControl;
} ESCNBUTTONPRESS, *PESCNBUTTONPRESS;


#define  ESCN_SCROLL             (ESCM_NOTIFICATION+2)
#define  ESCN_SCROLLING          (ESCM_NOTIFICATION+3)
// These two messagesa are sent by the scroll bar to its parent to tell
// it that the scroll bar has changed. ESCM_SCROLLING means that the
// value has changed and will continue to change as the user drags the
// scroll bar. ESCM_SCROLL means that it's finished changing.
typedef struct {
   CEscControl    *pControl;
   int            iPos;    // position
   int            iMin;    // minimum
   int            iMax;    // maximum
   int            iPage;   // page size
} ESCNSCROLL, *PESCNSCROLL;


#define  ESCN_THREEDCLICK        (ESCM_NOTIFICATION+4)
// Send by the 3d control when the user clicks on an object that
// have specified an object ID for.
typedef struct {
   CEscControl    *pControl;
   DWORD          dwMajor; // ID specified
   DWORD          dwMinor; // what part clicked on
} ESCNTHREEDCLICK, *PESCNTHREEDCLICK;


#define  ESCN_EDITCHANGE         (ESCM_NOTIFICATION+5)
// Sent by the edit control when the text has changed.
typedef struct {
   CEscControl    *pControl;
} ESCNEDITCHANGE, *PESCNEDITCHANGE;


#define  ESCN_LISTBOXSELCHANGE   (ESCM_NOTIFICATION+6)
// Sent by a list box when the selection has changed.
typedef struct {
   CEscControl    *pControl;  // control
   DWORD          dwCurSel;   // current selection
   PWSTR          pszName;    // name of the selection. Only valid during this callback. Do not change. May be NULL
   PWSTR          pszData;    // data of the selection. Only valid during this callback. Do not change. May be NULL
   DWORD          dwReason;   // 0 = scroll with keyboard, 1 = click with mouse
} ESCNLISTBOXSELCHANGE, *PESCNLISTBOXSELCHANGE;

#define  ESCN_COMBOBOXSELCHANGE   (ESCM_NOTIFICATION+7)
// Sent by a list box when the selection has changed.
typedef struct {
   CEscControl    *pControl;  // control
   DWORD          dwCurSel;   // current selection
   PWSTR          pszName;    // name of the selection. Only valid during this callback. Do not change. May be NULL
   PWSTR          pszData;    // data of the selection. Only valid during this callback. Do not change. May be NULL
} ESCNCOMBOBOXSELCHANGE, *PESCNCOMBOBOXSELCHANGE;


#define  ESCN_TIMECHANGE         (ESCM_NOTIFICATION+8)
// Sent by the time control when it's values have changed due to user input.
typedef struct {
   CEscControl    *pControl;  // control
   int            iHour;      // new hour
   int            iMinute;    // new minute
} ESCNTIMECHANGE, *PESCNTIMECHANGE;

#define  ESCN_DATECHANGE         (ESCM_NOTIFICATION+9)
// Sent by the date control when it's values have changed due to user input.
typedef struct {
   CEscControl    *pControl;  // control
   int            iDay;       // new day
   int            iMonth;     // new month
   int            iYear;      // new year
} ESCNDATECHANGE, *PESCNDATECHANGE;


#define  ESCN_FILTEREDLISTCHANGE (ESCM_NOTIFICATION+10)
// Sent by the filtered list control when it's values have changed due to user input.
typedef struct {
   CEscControl    *pControl;  // control
   int            iCurSel;    // curent selection. >= 0 for a value in the list.
                              // -1 for no selection. -2 for "Add new XXX"
} ESCNFILTEREDLISTCHANGE, *PESCNFILTEREDLISTCHANGE;



#define  ESCN_FILTEREDLISTQUERY (ESCM_NOTIFICATION+11)
// Sent by the filtered list when it's painting a new element or when
// it's displaying the dropdown. You need to fill in pList.
typedef struct {
   CEscControl    *pControl;  // control
   PWSTR          pszListName;// name of the list, as specified in "listname=" in the control's attribute
   PCListVariable pList;      // fill in with a pointer to the list (as specified by pszListName).
                              // The element of the list contains two packed strings, the first
                              // being the visible text, and the second being a sequence of invisible
                              // keywords.
                              // Ex: If this were names, an entry for my name might be "Michael Rozak\0Mike\0"
                              // so my name would appear as "Michael Rozak" but a user could find me if they
                              // typed "Mike" or "Michael".
} ESCNFILTEREDLISTQUERY, *PESCNFILTEREDLISTQUERY;






/**************************************************************************
Messages to controls
*/
#define  ESCM_RADIOWANTFOCUS     (ESCM_MESSAGE+1)
// A special message sent by the page to a radio button when switching
// to the next tab. It sees if the raido control wants focus or not. The
// radio control will see if any other controls in the group are checked;
// if there are, it denies focus.
typedef struct {
   BOOL        fAcceptFocus;
} ESCMRADIOWANTFOCUS, *PESCMRADIOWANTFOCUS;


#define  ESCM_STATUSTEXT         (ESCM_MESSAGE+2)
// Tells a Status control to change its text. Only one of pNode, pszText,
// or pszMML must be set. If pNode is set, the node (and children) will
// ultimately be deleted by the Status object. The status object will copy
// pszText or pszMML.
typedef struct {
   PCMMLNode      pNode;   // node to use. Status object will NOT delete this.
   PWSTR          pszText; // raw text to be stuck in
   PWSTR          pszMML;  // mml text to be parsed
} ESCMSTATUSTEXT, *PESCMSTATUSTEXT;

#define  ESCM_EDITCUT            (ESCM_MESSAGE+3)
#define  ESCM_EDITCOPY           (ESCM_MESSAGE+4)
#define  ESCM_EDITPASTE          (ESCM_MESSAGE+5)
// sent to an edit control to cut, copy, or paste to/from the clipboard


#define  ESCM_EDITUNDO           (ESCM_MESSAGE+6)
#define  ESCM_EDITREDO           (ESCM_MESSAGE+7)
// sent to an edit control to undo/redo one level


#define  ESCM_EDITCANUNDOREDO    (ESCM_MESSAGE+8)
// fills in the structure fUndo with TRUE if can undo
typedef struct {
   BOOL     fUndo;   // cna undo
   BOOL     fRedo;   // can redo
} ESCMEDITCANUNDOREDO, *PESCMEDITCANUNDOREDO;


#define  ESCM_EDITCHARFROMPOS    (ESCM_MESSAGE+9)
// given a position, returns the character its over
typedef struct {
   POINT    p;       // should have point in page coordinates
   DWORD    dwChar;  // filled with the character index. -1 if no character
} ESCMEDITCHARFROMPOS, *PESCMEDITCHARFROMPOS;


#define  ESCM_EDITEMPTYUNDO      (ESCM_MESSAGE+10)
// empties both the undo & redo buffers


#define  ESCM_EDITGETLINE        (ESCM_MESSAGE+11)
// copies a line string into the buffer
typedef struct {
   DWORD       dwLine;        // should contain the line number
   PWSTR       psz;           // filled with the line string if it's long enough
   DWORD       dwSize;        // Number of  bytes available in psz
   DWORD       dwNeeded;      // filled with the number needed
   BOOL        fFilledIn;     // fill in with TRUE if filled in psz, FALSE if didn't (buffer too small or didnt exist?)
} ESCMEDITGETLINE, *PESCMEDITGETLINE;


#define  ESCM_EDITLINEFROMCHAR   (ESCM_MESSAGE+12)
// given a character number, returns the line number
typedef struct {
   DWORD       dwChar;        // character #
   DWORD       dwLine;        // filled with the line #. -1 if no such character
} ESCMEDITLINEFROMCHAR, *PESCMEDITLINEFROMCHAR;



#define  ESCM_EDITLINEINDEX      (ESCM_MESSAGE+13)
// given a line number, return the character that starts the line
typedef struct {
   DWORD       dwLine;        // line #
   DWORD       dwChar;        // filled with character #. -1 if no such line
   DWORD       dwLen;         // fileld with the line length. -1 is no such line
} ESCMEDITLINEINDEX, *PESCMEDITLINEINDEX;


#define  ESCM_EDITPOSFROMCHAR    (ESCM_MESSAGE+14)
// given a character number, returns a rect bounding the character
typedef struct {
   DWORD       dwChar;        // character #
   RECT        r;             // bounding rectangle, in page coords. All 0s if cant find
} ESCMEDITPOSFROMCHAR, *PESCMEDITPOSFROMCHAR;



#define  ESCM_EDITREPLACESEL     (ESCM_MESSAGE+15)
// replaces the current selection
typedef struct {
   PWSTR       psz;           // string to replace with
   DWORD       dwLen;         // # of characters
} ESCMEDITREPLACESEL, *PESCMEDITREPLACESEL;



#define  ESCM_EDITSCROLLCARET    (ESCM_MESSAGE+16)
// scroll so the caret is visible

#define  ESCM_EDITFINDTEXT     (ESCM_MESSAGE+33)
// searches for text
typedef struct {
   DWORD       dwFlags;       // FR_MATCHCASE and/or FR_WHOLEWORD
   DWORD       dwStart;       // start of the search
   DWORD       dwEnd;         // end limit of search (string must be within dwStart & dwEnd)
   PWSTR       pszFind;       // string to find
   DWORD       dwFoundStart;  // set to where the string is found start. -1 if not found
   DWORD       dwFoundEnd;    // set to where the string end is. -1 if not found
} ESCMEDITFINDTEXT, *PESCMEDITFINDTEXT;





#define  ESCM_LISTBOXADD         (ESCM_MESSAGE+17)
// adds an item to the list box. (Or inserts.). Either pNode or pszText or pszMML
// must be filled in.
// if it's of pNode it must be of type <elem>. Name sure name= and potentially data= are set
// if use pszMML, have one or more <elem>xxx</elem> in the text. This way can add multiple
// if if's pszText, the text displayed will be pszText. The name will be pszText. The data will be pszText
typedef struct {
   PCMMLNode      pNode;   // node to use. Status object will NOT delete this.
   PWSTR          pszText; // raw text to be stuck in
   PWSTR          pszMML;  // mml text to be parsed
   DWORD          dwInsertBefore;   // where to insert before. Use -1 to add
                                    // note: inserting in a sorted list does no good
} ESCMLISTBOXADD, *PESCMLISTBOXADD;


#define  ESCM_LISTBOXDELETE      (ESCM_MESSAGE+18)
// deletes an element in the list box
typedef struct {
   DWORD          dwIndex; // index number
} ESCMLISTBOXDELETE, *PESCMLISTBOXDELETE;



#define  ESCM_LISTBOXFINDSTRING  (ESCM_MESSAGE+19)
// searches through the list box looking for an element whose name= attrib matches
// the string
typedef struct {
   int            iStart;  // zero-based element that occurs before the element to
                           // search at. Set to -1 to search the entire list box.
                           // if it goes beyond then end it will wrap around until it
                           // hits iStart
   PWSTR          psz;     // string to search for
   BOOL           fExact;  // if TRUE, need an exact match. If not, accepts any string
                           // that contains the given string
   DWORD          dwIndex; // filled in with the element number found. -1 if cant find
} ESCMLISTBOXFINDSTRING, *PESCMLISTBOXFINDSTRING;


#define  ESCM_LISTBOXGETCOUNT    (ESCM_MESSAGE+20)
// returns the number of elements in the list box
typedef struct {
   DWORD          dwNum;   // filled in with the number of elements
} ESCMLISTBOXGETCOUNT, *PESCMLISTBOXGETCOUNT;



#define  ESCM_LISTBOXGETITEM     (ESCM_MESSAGE+21)
// returns details about an item
typedef struct {
   DWORD          dwIndex; // item to look for
   PWSTR          pszName; // filled with the name string. Valid until the list box contents are changed. Do not modify. May be NULL
   PWSTR          pszData; // filled with the data string. Valid until the list box contents are changed. Do not modify. May be NULL
   RECT           rPage;   // rectangle as it appears on the page. Note, may be out of range of the control's display area
} ESCMLISTBOXGETITEM, *PESCMLISTBOXGETITEM;


#define  ESCM_LISTBOXITEMFROMPOINT  (ESCM_MESSAGE+22)
// given a point (only really caring about the Y value), fills it in with the list box
// index that its over.
typedef struct {
   POINT          pPoint;  // point. Only Y is looked at. Page coordinates.
   DWORD          dwIndex; // filled with item number. -1 if cant find
} ESCMLISTBOXITEMFROMPOINT, *PESCMLISTBOXITEMFROMPOINT;


#define  ESCM_LISTBOXRESETCONTENT   (ESCM_MESSAGE+23)
// clears the contents of the list box


#define  ESCM_LISTBOXSELECTSTRING   (ESCM_MESSAGE+24)
// searches for the given string and selects it. Also fills in dwIndex
typedef ESCMLISTBOXFINDSTRING ESCMLISTBOXSELECTSTRING;
typedef ESCMLISTBOXSELECTSTRING *PESCMLISTBOXSELECTSTRING;



/* combo box */

#define  ESCM_COMBOBOXADD         (ESCM_MESSAGE+25)
// adds an item to the list box. (Or inserts.). Either pNode or pszText or pszMML
// must be filled in.
// if it's of pNode it must be of type <elem>. Name sure name= and potentially data= are set
// if use pszMML, have one or more <elem>xxx</elem> in the text. This way can add multiple
// if if's pszText, the text displayed will be pszText. The name will be pszText. The data will be pszText
typedef struct {
   PCMMLNode      pNode;   // node to use. Status object will NOT delete this.
   PWSTR          pszText; // raw text to be stuck in
   PWSTR          pszMML;  // mml text to be parsed
   DWORD          dwInsertBefore;   // where to insert before. Use -1 to add
                                    // note: inserting in a sorted list does no good
} ESCMCOMBOBOXADD, *PESCMCOMBOBOXADD;


#define  ESCM_COMBOBOXDELETE      (ESCM_MESSAGE+26)
// deletes an element in the list box
typedef struct {
   DWORD          dwIndex; // index number
} ESCMCOMBOBOXDELETE, *PESCMCOMBOBOXDELETE;



#define  ESCM_COMBOBOXFINDSTRING  (ESCM_MESSAGE+27)
// searches through the list box looking for an element whose name= attrib matches
// the string
typedef struct {
   int            iStart;  // zero-based element that occurs before the element to
                           // search at. Set to -1 to search the entire list box.
                           // if it goes beyond then end it will wrap around until it
                           // hits iStart
   PWSTR          psz;     // string to search for
   BOOL           fExact;  // if TRUE, need an exact match. If not, accepts any string
                           // that contains the given string
   DWORD          dwIndex; // filled in with the element number found. -1 if cant find
} ESCMCOMBOBOXFINDSTRING, *PESCMCOMBOBOXFINDSTRING;


#define  ESCM_COMBOBOXGETCOUNT    (ESCM_MESSAGE+28)
// returns the number of elements in the list box
typedef struct {
   DWORD          dwNum;   // filled in with the number of elements
} ESCMCOMBOBOXGETCOUNT, *PESCMCOMBOBOXGETCOUNT;



#define  ESCM_COMBOBOXGETITEM     (ESCM_MESSAGE+29)
// returns details about an item
typedef struct {
   DWORD          dwIndex; // item to look for
   PWSTR          pszName; // filled with the name string. Valid until the list box contents are changed. Do not modify. May be NULL
   PWSTR          pszData; // filled with the data string. Valid until the list box contents are changed. Do not modify. May be NULL
} ESCMCOMBOBOXGETITEM, *PESCMCOMBOBOXGETITEM;


#define  ESCM_COMBOBOXRESETCONTENT   (ESCM_MESSAGE+31)
// clears the contents of the list box


#define  ESCM_COMBOBOXSELECTSTRING   (ESCM_MESSAGE+32)
// searches for the given string and selects it. Also fills in dwIndex
typedef ESCMCOMBOBOXFINDSTRING ESCMCOMBOBOXSELECTSTRING;
typedef ESCMCOMBOBOXSELECTSTRING *PESCMCOMBOBOXSELECTSTRING;


/* chart control */
#define  ESCM_CHARTDATA              (ESCM_MESSAGE+33)
// Tells the chart control to update it's data displayed with new data.
// pNode is a PCMMLNODE with a root object and then "axis" or "dataset" sub-nodes.
// or, it uses pszMML, which is parsed into a node.
typedef struct {
   PCMMLNode      pNode;   // node to use. Status object will NOT delete this.
   PWSTR          pszMML;  // mml text to be parsed
} ESCMCHARTDATA, *PESCMCHARTDATA;

#define  ESCM_THREEDCHANGE         (ESCM_MESSAGE+34)
// Tells a 3D control to change its display. Only one of pNode, 
// or pszMML must be set. If pNode is set, the node (and children) will
// NOT be deleted by the ThreeD object. The status object will copy
// pszMML.
typedef struct {
   PCMMLNode      pNode;   // node to use. Status object will NOT delete this.
   PWSTR          pszMML;  // mml text to be parsed
} ESCMTHREEDCHANGE, *PESCMTHREEDCHANGE;




/**************************************************************************
common to both controls & pages
*/
#define  ESCM_CONSTRUCTOR     (ESCM_EITHER+1)
// called in the constructor of the control/page. The callback function can
// set up any parameters in here. Called after the other member variables in
// the control/page have been set up

#define  ESCM_DESTRUCTOR      (ESCM_EITHER+2)
// called in the destructor of the control/page before the other member
// variables have been freed. The callback can do any releases here

#define  ESCM_CHAR               (ESCM_EITHER+3)
#define  ESCM_SYSCHAR            (ESCM_EITHER+103)
// called to indicate a key was pressed.
//
// Control: The control has the opportunity to capture
// it. If not the main page gets it. The default behaviour is to look through
// all the m_listAccelFocus and see if it's there. If it is that's run.
//
// Page: Called if the control with focus doesn't take it. Allows the page to capture
// it. The default behavior is to look through m_listAccel. If not,
// go through all the controls and see if it matches
// an acclerator from m_listAccelNoFocus or m_AccelSwitch.
typedef struct {
   WCHAR    wCharCode;  // see WM_CHAR
   LPARAM   lKeyData;   // see WM_CHAR
   BOOL     fEaten;  // should set this to TRUE if eat it. Leave as FALSE if not.
} ESCMCHAR, *PESCMCHAR;


#define  ESCM_KEYDOWN            (ESCM_EITHER+4)
#define  ESCM_KEYUP              (ESCM_EITHER+5)
#define  ESCM_SYSKEYDOWN         (ESCM_EITHER+105)
#define  ESCM_SYSKEYUP           (ESCM_EITHER+106)
// called to indicate a key was pressed down.
//
// Control: The control has the opporunity to
// eat it. If not, the main page gets it. The default behaviour is to do nothing.
//
// Page: Called if the control doesn't eat. The default behaviour is to do nothing.
typedef struct {
   int      nVirtKey;   // see WM_KEYDOWN
   LPARAM   lKeyData;   // see WM_KEYDOWN
   BOOL     fEaten;  // should set this to TRUE if eat it. Leave as FALSE if not.
} ESCMKEYDOWN, *PESCMKEYDOWN;
typedef ESCMKEYDOWN ESCMKEYUP;
typedef ESCMKEYDOWN *PESCMKEYUP;


#define  ESCM_TIMER              (ESCM_EITHER+8)
// called if a timer created in the CEscWindow is created. The default behaviour
// is to do nothing
typedef struct {
   DWORD    dwID; // ID
} ESCMTIMER, *PESCMTIMER;


#define  ESCM_SIZE               (ESCM_EITHER+9)
// Control: notified the control that it's size has been changed. The new size is in m_rPosn
// already. (The control has no choice.) The default message handler just invlidates the
// control rectangle
//
// Page: notified that it has been resized. The new size is already in m_Visible.
// The default is to call ReInterpret().


#define  ESCM_MOVE               (ESCM_EITHER+10)
// Control: ntoifies the control that it has been moved. The new size is in m_rPosn already.
// (The control has no choice.) The default message handler just invalidates the
// control rectangle
//
// Page: Notifies the page that it has been movied. The default behavior is to
// invalidate the entire page so controls with 3d perspective can be redrawn.



#define  ESCM_MOUSEENTER         (ESCM_EITHER+11)
// Control: called to indicate that the mouse has entered the control's display region.
// the default hanlder invalidates the rectangle if m_fWantMouse is TRUE.
// It also sets the caret if m_fWantMouse.
//
// Page: Default behaviour is to see if a control has been entered and deal with that
// by passing onto the control.
// Also special case for links in the text since they're not official controls.

#define  ESCM_MOUSELEAVE         (ESCM_EITHER+12)
// Control: called to indicate that the mouse has left the control's display region (when
// it's not captured). The default handler invalidates the rectangle if m_fWantMouse.
// it doesn't bother with the caret assuming something else will.
//
// Page: Default behaviour is to see if a control has been left and deal with that
// by passing onto the control. Also special case for links in the text since they're
// not offical controls.

#define  ESCM_MOUSEMOVE          (ESCM_EITHER+13)
// Control: called to indicate that the mouse has moved within the control's display region,
// or if the mouse is captured, that it's moved at all. The default handler does nothing.
//
// Page: Default behavior is to see what control (or link) it's over. Based on that may
// send ESCM_MOUSELEAVE and ESCM_MOUSENETER messages, along with ESCM_MOUSEMOVE. For links
// may need to redraw. May also change the cursor.

typedef struct {
   POINT    pPosn;      // mouse position - in page coordinates
   WPARAM   wKeys;      // flags from WM_MOUSEMOVE
} ESCMMOUSEMOVE, *PESCMMOUSEMOVE;

#define  ESCM_LBUTTONDOWN        (ESCM_EITHER+14)
#define  ESCM_MBUTTONDOWN        (ESCM_EITHER+15)
#define  ESCM_RBUTTONDOWN        (ESCM_EITHER+16)
// Control: called to indicate that the XXX button is down. Unlike normal Windows behaviour,
// if the button is down the mouse is automatically captured, and not released until
// the button is up. The default message handler does nothing.
//
// Page: Passed down to whatever control it's for, and remember what control. FOr a link,
// special case.

typedef ESCMMOUSEMOVE ESCMLBUTTONDOWN;
typedef ESCMMOUSEMOVE ESCMMBUTTONDOWN;
typedef ESCMMOUSEMOVE ESCMRBUTTONDOWN;
typedef ESCMMOUSEMOVE *PESCMLBUTTONDOWN;
typedef ESCMMOUSEMOVE *PESCMMBUTTONDOWN;
typedef ESCMMOUSEMOVE *PESCMRBUTTONDOWN;

#define  ESCM_LBUTTONUP          (ESCM_EITHER+17)
#define  ESCM_MBUTTONUP          (ESCM_EITHER+18)
#define  ESCM_RBUTTONUP          (ESCM_EITHER+19)
// Control: called to indicate that the XXX button is up. Capture is released.
// The default message handler does nothing.
//
// Page: Default behavior is to pass down to control. Or deal with link.

typedef ESCMMOUSEMOVE ESCMLBUTTONUP;
typedef ESCMMOUSEMOVE ESCMMBUTTONUP;
typedef ESCMMOUSEMOVE ESCMRBUTTONUP;
typedef ESCMMOUSEMOVE *PESCMLBUTTONUP;
typedef ESCMMOUSEMOVE *PESCMMBUTTONUP;
typedef ESCMMOUSEMOVE *PESCMRBUTTONUP;

#define  ESCM_MOUSEHOVER              (ESCM_EITHER+20)
// Control: called to indicate that the mouse has been hovering over the control for
// awhile. the default message handler looks in m_pNode for a <Hoverhelp> tag. If it
// finds it, it passes it to the main window to display.
//
// Page: Default behaviour is to pass down to control. Or deal with link.

typedef ESCMMOUSEMOVE ESCMMOUSEHOVER;
typedef ESCMMOUSEMOVE *PESCMMOUSEHOVER;




/***************************************************************************
CEscControl - control class
*/
typedef BOOL (__cdecl *PESCCONTROLCALLBACK )(CEscControl *pPage, DWORD dwMessage, PVOID pParam) ;
typedef struct {
   WCHAR       c;    // character looking for
   BOOL        fAlt; // if TRUE need an alt
   BOOL        fShift;  // if TRUE need a shift
   BOOL        fControl;   // if TRUE need a control;
   DWORD       dwMessage;  // message
} ESCACCELERATOR, *PESCACCELERATOR;

class DLLEXPORT CEscControl {
public:
   //variables
   CMem           m_mem;         // the callback function can use this as it likes
   CFontCache     m_FontCache;   // just in case control wants to use
   PCMMLNode      m_pNode;       // node that created. Don't change. This is NOT deleted by control
   IFONTINFO      m_fi;          // font used immediately before control created.
   HINSTANCE      m_hInstance;   // hinstance for resources
   CEscPage       *m_pParentPage;// parent page. Must have one
   CEscControl    *m_pParentControl;   // if have parent control, pointer to it. Else NULL.
   PESCCONTROLCALLBACK m_pCallback; // callback. DO not touch.
   RECT           m_rPosn;       // position within the page coordinates
   BOOL           m_fEnabled;    // TRUE if control enabled. Do not change
   BOOL           m_fDefControl; // TRUE if the control is to be the default one with focus
   BOOL           m_fFocus;      // TRUE if control has focus. Do not change
   BOOL           m_fMouseOver;  // TRUE if the mouse is over the actual control. Do not change
   BOOL           m_fLButtonDown;   // TRUE if the left button is down. Do not change.
   BOOL           m_fMButtonDown;   // TRUE if the middle button is down. Do not change.
   BOOL           m_fRButtonDown;   // TRUE if the right button is down. Do not change.
   BOOL           m_fCapture;    // TRUE if the mouse is captured for control. Do not change.
   CBTree         m_treeAttrib;  // attribute tree
   PWSTR          m_pszName;     // control name. Uses the pointer from m_pNode.AttribGet("name"). Do not free
   DWORD          m_dwWantFocus;  // 0 cant tab to, 1=yes, 2=once has focus only take away with a tab
   BOOL           m_fWantMouse;  // if TRUE red box is drawn around when mouse moved over, else not invalid
   BOOL           m_fRedrawOnMove;  // if TRUE, invalidate rect when control is moved.
                                    // use this for 3d controls whose persepctive changes
                                    // depending upon the screen location
   CListFixed     m_listAccelFocus; // accelerators that are enabled ONLY when control has focus
   CListFixed     m_listAccelNoFocus;  // accelerators that are enable even when control doesn't have focus
   ESCACCELERATOR m_AccelSwitch; // fast accelerator that switches focus to this control



   // functions
   CEscControl (void);
   ~CEscControl (void);
   void* operator new (size_t);
   void  operator delete (void*);

   BOOL Init (PCMMLNode pNode, IFONTINFO *pfi, HINSTANCE hInstance,
      CEscPage *pParentPage, CEscControl *pParentControl, PESCCONTROLCALLBACK pCallback);


   void CoordPageToWindow (POINT *pPage, POINT *pWindow);
   void CoordPageToWindow (RECT *pPage, RECT *pWindow);
   void CoordWindowToPage (POINT *pWindow, POINT *pPage);
   void CoordWindowToPage (RECT *pWindow, RECT *pPage);
   void CoordPageToScreen (POINT *pPage, POINT *pScreen);
   void CoordPageToScreen (RECT *pPage, RECT *pScreen);
   void CoordScreenToPage (POINT *pScreen, POINT *pPage);
   void CoordScreenToPage (RECT *pScreen, RECT *pPage);
   BOOL IsVisible (void);
   BOOL Enable (BOOL fEnable);
   BOOL TimerSet (DWORD dwTime);
   BOOL TimerKill (void);
   BOOL Invalidate (RECT *pPage = NULL);
   BOOL Message (DWORD dwMessage, PVOID pParam = NULL);
   BOOL MessageToParent (DWORD dwMessage, PVOID pParam = NULL);
   BOOL AttribGet (PWSTR pszAttrib, PWSTR pszValue, DWORD dwSize, DWORD *pdwNeeded);
   BOOL AttribGetBOOL (PWSTR pszAttrib);
   int  AttribGetInt (PWSTR pszAttrib);
   BOOL AttribSet (PWSTR pszAttrib, PWSTR pszValue);
   BOOL AttribSetBOOL (PWSTR pszAttrib, BOOL fValue);
   BOOL AttribSetInt (PWSTR pszAttrib, int iValue);
   BOOL AttribEnum (DWORD dwNum, PWSTR pszAttrib, DWORD dwSize, DWORD *pdwNeeded);
   BOOL Paint (RECT *prPage, RECT *prDC, RECT *prScreen, RECT *pTotalScreen, HDC hDC);
   BOOL AttribListAddHex (WCHAR *psz, DWORD *pdwAttrib, BOOL *pfDirty = NULL, BOOL fRepaint = NULL, DWORD dwMessage = NULL);
   BOOL AttribListAddDecimal (WCHAR *psz, int *piAttrib, BOOL *pfDirty = NULL, BOOL fRepaint = NULL, DWORD dwMessage = NULL);
   BOOL AttribListAddColor (WCHAR *psz, COLORREF *pcr, BOOL *pfDirty = NULL, BOOL fRepaint = NULL, DWORD dwMessage = NULL);
   BOOL AttribListAddDecimalOrPercent (WCHAR *psz, int *piAttrib, BOOL *pfPercent, BOOL *pfDirty = NULL, BOOL fRepaint = NULL, DWORD dwMessage = NULL);
   BOOL AttribListAddBOOL (WCHAR *psz, BOOL *pfAttrib, BOOL *pfDirty = NULL, BOOL fRepaint = NULL, DWORD dwMessage = NULL);
   BOOL AttribListAddPercent (WCHAR *psz, int *piAttrib, BOOL *pfDirty = NULL, BOOL fRepaint = NULL, DWORD dwMessage = NULL);
   BOOL AttribListAddAccelerator (WCHAR *psz, PESCACCELERATOR pAttrib, BOOL *pfDirty = NULL, BOOL fRepaint = NULL, DWORD dwMessage = NULL);
   BOOL AttribListAddString (WCHAR *psz, PWSTR *ppszAttrib, BOOL *pfDirty = NULL, BOOL fRepaint = NULL, DWORD dwMessage = NULL);
   BOOL AttribListAddString (WCHAR *psz, PWSTR pszAttrib, DWORD dwSize, BOOL *pfDirty = NULL, BOOL fRepaint = NULL, DWORD dwMessage = NULL);
   BOOL AttribListAddDouble (WCHAR *psz, double *piAttrib, BOOL *pfDirty = NULL, BOOL fRepaint = NULL, DWORD dwMessage = NULL);
   BOOL AttribListAdd3DPoint (WCHAR *psz, double *piAttrib, BOOL *pfDirty = NULL, BOOL fRepaint = NULL, DWORD dwMessage = NULL);
   BOOL AttribListAddCMem (WCHAR *psz, PCMem pMem, BOOL *pfDirty = NULL, BOOL fRepaint = NULL, DWORD dwMessage = NULL);
   CEscTextBlock *TextBlock (HDC hDC, CMMLNode *pNode, int iWidth, BOOL fDeleteNode, BOOL fRootNodeNULL = TRUE);

private:
   DWORD          m_dwTimerID;   // timer ID
   CBTree         m_treeAttribList; // conversion info for attribute lists. type ATTRIBCONVERT
   BOOL AttribListSet (WCHAR *psz, WCHAR *pszValue);
   BOOL AttribListGet (WCHAR *psz, WCHAR *pszValue, DWORD dwSize, DWORD *pdwNeeded);
};
typedef CEscControl * PCEscControl;


#define  ESCM_INITCONTROL        (ESCM_CONTROLBASE+1)
// The next message to be called after the constructor. m_pNode, m_pfi,  m_hInstance
// m_pParentPage, m_pParentControl, m_pCallback will have been filled in.
// Also, m_treeAttrib and m_pszName filled in from m_pNode. m_AccelSwitch filled in
// based on the control's parameters
// The control should probably fill in accelerator information in m_listAccelFocus,
// m_listAccelNoFocus, m_dwWantFocus, m_fWantMouse, m_fRedrawOnMove


#define  ESCM_QUERYSIZE          (ESCM_CONTROLBASE+2)
// called after INITCONTROL, and maybe at later points if the screen changes. The control
// should figure out what size it wants to be. pParam points to ESCMQUERYSIZE.
// The default message handler looks in the m_pNode for a width/height and interprets
// it as pixels or %. The default handler leaves iWidth and/or iHeight blank if
// no "width" or "height" attributes are set in m_pNode. Control callbacks may want
// to set the values if any other than width/height indicate the size, and then
// let the default handler override.
typedef struct {
   int      iDisplayWidth; // number of pixels across that can be displayed- start
   HDC      hDC;           // HDC to test with
   int      iWidth;        // fill this in with desired height if control has opinion
   int      iHeight;       // fill this in with desired height if control has opinion
} ESCMQUERYSIZE, *PESCMQUERYSIZE;


#define  ESCM_PAINT              (ESCM_CONTROLBASE+5)
// Tells the control to paint itself. Note: The control does not have to paint the
// focus or mouse-over rectangle - this is done by default (although it can be
// overridden). THe control should paint differently if its disabled. The default
// message proc just paints a dummy.
typedef struct {
   HDC      hDC;           // HDC to draw to
   RECT     rControlPage;  // the location of the page. will be m_rPosn
   RECT     rControlHDC;   // location of control as displayed in HDC
   RECT     rControlScreen;// location of control as displayed on the screen
   RECT     rInvalidPage;  // invalid rectangle in page coordinates, same coords as m_rPosn
   RECT     rInvalidHDC;   // invalid where should paint to in the HDC. width & height of rHDC = width & height rPage
   RECT     rInvalidScreen;// invalidwhere this will appear on the screen (for perspective reasons.)
                           // width & height of rHDC = width & height of rScreen
   RECT     rTotalScreen;  // rectangle giving entire screen, since some controls use for perspective
} ESCMPAINT, *PESCMPAINT;

#define  ESCM_PAINTFOCUS         (ESCM_CONTROLBASE+6)
// Only called if m_fEnabled & m_fFocus. Tells the control it should draw a dotted-line
// box (or something) to indicate focus. (Called after ESCM_PAINT). The default
// message handler draws a dotted-line box
typedef ESCMPAINT ESCMPAINTFOCUS;
typedef ESCMPAINTFOCUS * PESCMPAINTFOCUS;

#define  ESCM_PAINTMOUSEOVER     (ESCM_CONTROLBASE+7)
// Only called if m_fEnabled & m_fMouseOver & m_fWantMouse. Tells the control it should draw a red
// box (or something) to indicate this is clickable.
// (Called after ESCM_PAINT). The default message handler draws a red box.
typedef ESCMPAINT ESCMPAINTMOUSEOVER;
typedef ESCMPAINTMOUSEOVER * PESCMPAINTMOUSEOVER;

#define  ESCM_ENABLE             (ESCM_CONTROLBASE+8)
// called to indicate that the control's enable state has changed. The default
// handler just invalidates the rectangle.

#define  ESCM_FOCUS              (ESCM_CONTROLBASE+9)
// called to indicate that the control's focus state has changed. The default
// handler just invalidates the rectangle.


#define  ESCM_CONTROLTIMER       (ESCM_CONTROLBASE+23)
// called if the control's timer goes off. Default message procedure does nothing.
// Control's timer is CEscControl::TimerSet, TimerKill

#define  ESCM_ATTRIBENUM         (ESCM_CONTROLBASE+26)
// tells the control to enumerate a specific attribute name. The default behavior
// is to call m_treeAttrib::Enum.
typedef struct {
   DWORD       dwIndex;       // index, 0 based. If more than # of attributes the fill fExist with FALSE
   PWSTR       pszAttrib;     // memory to be filled in with attribute name
   DWORD       dwSize;        // number of bytes available
   DWORD       dwNeeded;      // filled with the number needed
   BOOL        fFilledIn;     // fill in with TRUE if filled in pszAttrib, FALSE if didn't (buffer too small or didnt exsit?)
} ESCMATTRIBENUM, *PESCMATTRIBENUM;

#define  ESCM_ATTRIBGET          (ESCM_CONTROLBASE+27)
// asks the control for the value of an attribute. The default behaviour is to look
// in m_treeAttrib::Get. A control's message handler may trap this, see what attribute
// is being requested, update it in m_treeAttrib, and then return FALSE to let the
// default handler do it.
typedef struct {
   PWSTR       pszAttrib;     // attrbiute
   PWSTR       pszValue;      // fill this in with the value
   DWORD       dwSize;        // number of bytes available
   DWORD       dwNeeded;      // filled with the number needed
   BOOL        fFilledIn;     // fill in with TRUE if filled in pszAttrib, FALSE if didn't (buffer too small or didnt exist?)
} ESCMATTRIBGET, *PESCMATTRIBGET;

#define  ESCM_ATTRIBSET          (ESCM_CONTROLBASE+28)
// sets the controls value of an attribute. The default behaviour is to look
// in m_treeAttrib::Set. A control's message handler may trap this, see what attribute
// is being changed, modify some internal variables, and then return FALSE to let the
// default handler do it.
typedef struct {
   PWSTR       pszAttrib;     // attrbiute
   PWSTR       pszValue;      // new value. If this is NULL then attribute should be deleted
} ESCMATTRIBSET, *PESCMATTRIBSET;



#define  ESCM_SWITCHACCEL        (ESCM_CONTROLBASE+29)
// called if the control's m_AccelSwitch is pressed. the default behaviour is to
// call pParentPage->FocusSet() for the control. Some controls may just want to
// activate a link or something, such as buttons.





/***************************************************************************
CEscPage - page class.
*/
typedef BOOL (__cdecl *PESCPAGECALLBACK )(CEscPage *pPage, DWORD dwMessage, PVOID pParam) ;

class DLLEXPORT CEscPage {
   friend class CEscWindow;

public:
   // variables
   CMem           m_mem;         // the callback function can use this as it likes
   PCEscControl   m_pControlFocus;// the control with focus. NULL if no focus. Do not change,
   PESCPAGECALLBACK m_pCallback; // callback for the page's messages. Do not change.
   CEscWindow     *m_pWindow;    // windo in which the page lies
   RECT           m_rTotal;      // total area used by page's contents. Do not change
   RECT           m_rVisible;    // amount that's visible
   CListFixed     m_listESCACCELERATOR;   // accelerators captures for the page, and sending messages to it's callback
   PCMMLNode      m_pNode;       // main node that created the page. This is deleted when page object destroyed. Do not modify.
   CEscTextBlock  m_TextBlock;   // interpretation object. Do not modify
   CBTree         m_treeControls;   // tree with each node containing a pointer to control, key = control name
   PVOID          m_pUserData;      // passed in during page creation. Meaning depends on caller.
   IFONTINFO      m_fi;          // font used for default text
   RECT           m_rMargin;     // size of left,right,top, bottom margins (in pixels). Set by tag in MML


   // functions
   CEscPage (void);
   ~CEscPage (void);
   void* operator new (size_t);
   void  operator delete (void*);

   BOOL Init (PWSTR pszPageText, PESCPAGECALLBACK pCallback, CEscWindow *pWindow, PVOID pUserData = NULL, IFONTINFO *pfi = NULL);
   BOOL Init (PSTR pszPageText, PESCPAGECALLBACK pCallback, CEscWindow *pWindow, PVOID pUserData = NULL, IFONTINFO *pfi = NULL);
   BOOL Init (DWORD dwResource, PESCPAGECALLBACK pCallback, CEscWindow *pWindow, PVOID pUserData = NULL, IFONTINFO *pfi = NULL);
   BOOL Init (PCMMLNode pNode, PESCPAGECALLBACK pCallback, CEscWindow *pWindow, PVOID pUserData = NULL, IFONTINFO *pfi = NULL);
   BOOL InitFile (PWSTR pszFile, PESCPAGECALLBACK pCallback, CEscWindow *pWindow, PVOID pUserData = NULL, IFONTINFO *pfi = NULL);

   BOOL Exit (PWSTR pszExitCode);
   BOOL Link (PWSTR pszLink);
   BOOL ReInterpret (void);
   BOOL FocusSet (PCEscControl pControl);
   PCEscControl FocusGet (void);
   BOOL VScroll (int iY);
   BOOL VScrollToSection (PWSTR psz);
   BOOL SetCursor (HCURSOR hCursor);
   BOOL SetCursor (DWORD dwID);
   BOOL Invalidate (RECT *pRect = NULL);
   BOOL Update (void);
   void CoordPageToWindow (POINT *pPage, POINT *pWindow);
   void CoordPageToWindow (RECT *pPage, RECT *pWindow);
   void CoordWindowToPage (POINT *pWindow, POINT *pPage);
   void CoordWindowToPage (RECT *pWindow, RECT *pPage);
   void CoordPageToScreen (POINT *pPage, POINT *pScreen);
   void CoordPageToScreen (RECT *pPage, RECT *pScreen);
   void CoordScreenToPage (POINT *pScreen, POINT *pPage);
   void CoordScreenToPage (RECT *pScreen, RECT *pPage);
   BOOL Message (DWORD dwMessage, PVOID pParam = NULL);
   PCEscControl ControlFromPoint (POINT *pPage);
   PCEscControl ControlFind (WCHAR *pszName);
   BOOL MouseCaptureRelease (PCEscControl pControl);
   BOOL FocusToNextControl (BOOL fForward = TRUE);
   BOOL IsControlValid (PCEscControl pControl);

   DWORD MessageBox (PWSTR pszTitle, PWSTR pszSummary, PWSTR pszFinePrint, DWORD dwType);
   DWORD MBSpeakInformation (PWSTR pszSummary, PWSTR pszFinePrint = NULL);
   DWORD MBSpeakWarning (PWSTR pszSummary, PWSTR pszFinePrint = NULL);
   DWORD MBInformation (PWSTR pszSummary, PWSTR pszFinePrint = NULL, BOOL fCancel = FALSE);
   DWORD MBYesNo (PWSTR pszSummary, PWSTR pszFinePrint = NULL, BOOL fCancel = FALSE);
   DWORD MBWarning (PWSTR pszSummary, PWSTR pszFinePrint = NULL, BOOL fCancel = FALSE);
   DWORD MBError (PWSTR pszSummary, PWSTR pszFinePrint = NULL, BOOL fCancel = FALSE);


private:
   PCEscControl   m_pControlMouse;  // control that the mouse is over, NULL if none
   PCEscControl   m_pControlCapture;   // control that has mouse capture
   int            m_iLastWidth;     // last width that had interpreted at
   int            m_iLastHeight;    // last height that interpreted at

   BOOL Paint (RECT *pPageCoord, RECT *pHDCCoord, RECT *pScreenCoord, HDC hDC);
   BOOL MassageControls (void);
   BOOL Handle3DControls (void);
   BOOL Init2 (BOOL fCall, PCMMLNode pNode, PESCPAGECALLBACK pCallback, CEscWindow *pWindow, PVOID pUserData = NULL, IFONTINFO *pfi = NULL);
};
typedef CEscPage * PCEscPage;



/* defines for Beep */
//#define  ESCBEEP_BUTTONDOWN         76
//#define  ESCBEEP_BUTTONUP           77
#define  ESCBEEP_BUTTONDOWN         37
#define  ESCBEEP_BUTTONUP           37
#define  ESCBEEP_RADIOCLICK         37
//#define  ESCBEEP_DONTCLICK          56
//#define  ESCBEEP_DONTCLICK          58
#define  ESCBEEP_DONTCLICK          65
//#define  ESCBEEP_SCROLLLINEUP       60
//#define  ESCBEEP_SCROLLLINEDOWN     62
//#define  ESCBEEP_SCROLLPAGEUP       61
//#define  ESCBEEP_SCROLLPAGEDOWN     63
#define  ESCBEEP_SCROLLLINEUP       69
#define  ESCBEEP_SCROLLLINEDOWN     69
#define  ESCBEEP_SCROLLPAGEUP       70
#define  ESCBEEP_SCROLLPAGEDOWN     70
#define  ESCBEEP_SCROLLDRAGSTART    73
#define  ESCBEEP_SCROLLDRAGSTOP     74
//#define  ESCBEEP_LINKCLICK          81
#define  ESCBEEP_LINKCLICK          42
#define  ESCBEEP_MENUOPEN           80
#define  ESCBEEP_MENUCLOSE          74


#define  ESCM_INITPAGE           (ESCM_PAGEBASE+1)
// called after everything in the page has been compiled. Called so the page
// can initialize itself. The default message proc does nothing.



#define  ESCM_INTERPRETERROR     (ESCM_PAGEBASE+2)
// called when there's an error on interpretation. the appliction can save
// the error away to show the user. The default message proc does nothing
typedef struct {
   CEscError   *pError; // error object to query. It may be deleted after the app returns.
} ESCMINTERPRETERROR, *PESCMINTERPRETERROR;



#define  ESCM_ACTIVATE           (ESCM_PAGEBASE+3)
// called when the page's window is activated or deactivated. the default
// message proc does nothing
typedef struct {
   WORD        fActive;    // see WM_ACTIVATE
   WORD        fMinimized; // see WM_ACTIVATE
   HWND        hWndPrevious;  // see WM_ACTIVATE
} ESCMACTIVATE, *PESCMACTIVATE;


#define  ESCM_ACTIVATEAPP        (ESCM_PAGEBASE+4)
// sent when the app's activated. the default message proc does nothing
typedef struct {
   BOOL        fActive;    // see WM_ACTIVATEAPP
   DWORD       dwThreadID; // see WM_ACTIVATEAPP
} ESCMACTIVATEAPP, *PESCMATIVATEAPP;



#define  ESCM_CLOSE              (ESCM_PAGEBASE+5)
// a close request as come in frm alt-f4 or pressing the close box.
// the default behavious is to call Exit(). If an app doesn't want to
// close it can trap this.


#define  ESCM_LINK               (ESCM_PAGEBASE+6)
// CEscPage::Link was called. The default behaviour is to see if the string
// starts with "http:", "https:", or "mailto:". If so, the link is ShellExecute()-ed.
// else, Exit(sz) is called. The page may trap this for whatever reason.
typedef struct {
   WCHAR       *psz;       // link string
} ESCMLINK, *PESCMLINK;


#define  ESCM_ENDSESSION         (ESCM_PAGEBASE+7)
// WM_Endsession is called. The default behavior is to call Exit()
typedef struct {
   BOOL        fEndSession;   // see WM_ENDSESSION
   LPARAM      fLogOff;       // see WM_ENDSESSION
} ESCMENDSESSION, *PESCMENDSESSION;


#define  ESCM_QUERYENDSESSION    (ESCM_PAGEBASE+8)
// WM_QUERYENDSESSION is called. The default behavior is to call ESCM_CLOSE
typedef struct {
   UINT        nSource;       // see WM_QUERYENDSESSION
   LPARAM      fLogOff;       // see WM_QUERYENDSESSION
   BOOL        fTerminate;    // fill in with TRUE if can terminate conveniently, FALSE if cant
} ESCMQUERYENDSESSION, *PESCMQUERYENDSESSION;


#define  ESCM_SCROLL             (ESCM_PAGEBASE+9)
// notify the page that it has been scrolled. the default behaviour is to invalide
// the controls that want m_fRedrawOnMove. It's assumed that CEscWindow will bitblt
// scrolled areas of the image properly. m_rVisible has already been updated.



#define  ESCM_MOUSEWHEEL         (ESCM_PAGEBASE+10)
// the mouse wheel has moved. Default behavior is to scroll the window.
typedef struct {
   WORD     fKeys;      // see WM_MOUSEWHEEL
   short    zDelta;
   POINT    pPosn;      // in page coordinates
} ESCMMOUSEWHEEL, *PESCMMOUSEWHEEL;


#define  ESCM_SUBSTITUTION       (ESCM_PAGEBASE+11)
// called when the page is compiling and a substitution appears in the MML
// text.
typedef struct {
   PWSTR    pszSubName;       // substitution name
   PWSTR    pszSubString;     // fill this in with the string to substitute
   BOOL     fMustFree;        // fill this in with TRUE if caller must HeapFree(hMemHandle, pszSubString),
                              // or FALSE if it's some sort of global that doens't need
                              // freeing.
   HANDLE   hMemHandle;       // used for HeapFree if fMustFree is set
} ESCMSUBSTITUTION, *PESCMSUBSTITUTION;

#define  ESCM_POWERBROADCAST    (ESCM_PAGEBASE+12)
// WM_POWERBROADCAST is called. The default behavior is to do nothing
typedef struct {
   DWORD       dwPowerEvent;  // see WM_POWERBROADCAST
   DWORD       dwData;        // see WM_POWERBROADCAST
   int         iRet;          // return. TRUE to grant request, or BROADCAST_QUERY_DENY
} ESCMPOWERBROADCAST, *PESCMPOWERBROADCAST;



/***************************************************************************
CEscWindow - window class.
*/
// EWS_XXX - Used in the Init for the window style
#define  EWS_TITLE         0x0000   // window has a title bar
#define  EWS_NOTITLE       0x0001

#define  EWS_SIZABLE       0x0000   // window can be resized
#define  EWS_FIXEDSIZE     0x0002

#define  EWS_VSCROLL       0x0000   // window shows a vertical scroll bar
#define  EWS_NOVSCROLL     0x0004

#define  EWS_FIXEDHEIGHT   0x0000   // windows height doesnt change with each page
#define  EWS_AUTOHEIGHT    0x0008   // height changes with each page to just fit text

#define  EWS_FIXEDWIDTH    0x0000   // window width is fixed or changes from user size
#define  EWS_AUTOWIDTH     0x0020   // width width is as wide as it can go

#define  EWS_NOSPECIALCLOSE 0x0000  // special closing considerations
#define  EWS_CLOSEMOUSEMOVE 0x0100    // close if the mouse moves
#define  EWS_CLOSENOMOUSE  0x0200   // close if the mouse moves off window

#define  EWS_HIDE          0x0400   // when start up hide the window

// Print() flags
#define  ESCPRINT_EVENONLY 0x0001   // only print even pages
#define  ESCPRINT_ODDONLY  0x0002   // only print odd pages
typedef struct {
   DWORD    dwID;    // ID for Set/Kill timer
   DWORD    dwMessage;  // to send
   PCEscControl   pControl;   // control to send to
   PCEscPage      pPage;      // page to send to
} ESCTIMERID, *PESCTIMERID;

typedef struct {
   WORD     wFontScale;    // scaling for fonts. 0x100 = normal. 0x200=2x, 0x80=1/2x etc.
   WORD     wOtherScale;   // scaling for printing of buttons/bitmaps. 0x100 = normal. 0x200=2x, 0x80=1/2x etc.
   WORD     wColumns;      // # of columns
   WORD     wRows;         // # of rows (up & down)
   PWSTR    pszHeader;     // header MML. May contain substitutions for <<<PAGE>>>,
                           // <<<TIME>>>, <<<DATE>>>, <<<SECTION>>>, <<<PAGETITLE>>>
   PWSTR    pszFooter;     // footer MML. Same substitutions as header
   RECT     rPageMargin;   // left, right, top, bottom margin in TWIPS, 1/1440"
   int      iColumnSepX;   // distance between columns, in TWIPS
   int      iRowSepY;      // distance between rows, in TWIPS
   int      iHeaderSepY;   // distance between the header and text
   int      iFooterSepY;   // distance between the footer and text
   BOOL     fColumnLine;   // if TRUE, draw a line between columns
   BOOL     fRowLine;      // if TRUE, draw a line between rows
   BOOL     fHeaderLine;   // if TRUE, draw a line between the header and text
   BOOL     fFooterLine;   // if TRUE, draw a line bettween the footer and text
} ESCPRINTINFO, *PESCPRINTINFO;

// internal derived
typedef struct {
   RECT     rPage;         // size of the page
   int      iColumnWidth;  // column width
   int      iRowHeight;    // row height. estimated only
   int      iHeaderHeight; // header height. estimated only
   int      iFooterHeight; // footer height. estiamted only
} ESCPRINTINFO2, *PESCPRINTINFO2;

class DLLEXPORT CEscWindow {
   friend class CEscPage;
   friend class CEscControl;
public:
   // variable
   HWND        m_hWnd;     // window used. Do not change
   HWND        m_hWndParent;  // parent window. NULL if none. Do not change
   HINSTANCE   m_hInstance;   // where to get resources from. Do not change.
   CEscPage    *m_pPage;   // current page. NULL if none. Do not change
   DWORD       m_dwStyle;     // style from init
   PWSTR       m_pszExitCode; // exit code/string. Freed when window freed.
   int         m_iExitVScroll;// vertical scroll of the page when it exits
   BOOL        m_fMouseOver;  // TRUE if the mouse is over the actual window client area. Do not change
   BOOL        m_fLButtonDown;   // TRUE if the left button is down. Do not change.
   BOOL        m_fMButtonDown;   // TRUE if the middle button is down. Do not change.
   BOOL        m_fRButtonDown;   // TRUE if the right button is down. Do not change.
   BOOL        m_fCapture;    // TRUE if the mouse is captured for page. Do not change.
   POINT       m_pLastMouse;  // last mouse location (in screen coords) measured
   DWORD       m_dwLastMouse; // gettickcount() of last mouse
   IFONTINFO   m_fi;          // font used for default text. Initialized by Init, but may be changed later
   RECT        m_rMouseMove;  // if EWS_CLOSENOSE mouse active, close the mouse if it moves off the window
                              // AND it's not over m_rMouseMove. Set this after calling init.
   ESCPRINTINFO m_PrintInfo;  // info about how to print. Used by printing routines


   // funtions
   CEscWindow (void);
   ~CEscWindow (void);
   void* operator new (size_t);
   void  operator delete (void*);

   BOOL Init (HINSTANCE hInstance, HWND hWndParent = NULL,
      DWORD dwStyle = EWS_TITLE | EWS_SIZABLE | EWS_VSCROLL | EWS_FIXEDHEIGHT | EWS_NOSPECIALCLOSE,
      RECT *pRectSize = NULL);
   BOOL PageDisplay (PWSTR pszPageText, PESCPAGECALLBACK pCallback, PVOID pUserData = NULL);
   BOOL PageDisplay (PSTR pszPageText, PESCPAGECALLBACK pCallback, PVOID pUserData = NULL);
   BOOL PageDisplay (DWORD dwResource, PESCPAGECALLBACK pCallback, PVOID pUserData = NULL);
   BOOL PageDisplay (PCMMLNode pNode, PESCPAGECALLBACK pCallback, PVOID pUserData = NULL);
   BOOL PageDisplayFile (PWSTR pszFile, PESCPAGECALLBACK pCallback, PVOID pUserData = NULL);
   BOOL PageClose (void);
   PWSTR PageDialog (PWSTR pszPageText, PESCPAGECALLBACK pCallback, PVOID pUserData = NULL);
   PWSTR PageDialog (PSTR pszPageText, PESCPAGECALLBACK pCallback, PVOID pUserData = NULL);
   PWSTR PageDialog (DWORD dwResource, PESCPAGECALLBACK pCallback, PVOID pUserData = NULL);
   PWSTR PageDialog (PCMMLNode pNode, PESCPAGECALLBACK pCallback, PVOID pUserData = NULL);
   PWSTR PageDialogFile (PWSTR pszFile, PESCPAGECALLBACK pCallback, PVOID pUserData = NULL);

   BOOL InitForPrint (HDC hDCPrint, HINSTANCE hInstance, HWND hWndParent = NULL);
   BOOL PrintPageLoad (DWORD dwResource,
                                PESCPAGECALLBACK pCallback = 0, PVOID pUserData = 0,
                                int iWidth = 0, int iHeight = 0);
   BOOL PrintPageLoad (PWSTR psz,
                                PESCPAGECALLBACK pCallback = 0, PVOID pUserData = 0,
                                int iWidth = 0, int iHeight = 0);
   BOOL PrintPageLoad (char* psz,
                                PESCPAGECALLBACK pCallback = 0, PVOID pUserData = 0,
                                int iWidth = 0, int iHeight = 0);
   BOOL PrintPageLoadFile (PWSTR pszFile,
                                PESCPAGECALLBACK pCallback = 0, PVOID pUserData = 0,
                                int iWidth = 0, int iHeight = 0);
   BOOL PrintPaint (RECT *pPageCoord, RECT *pHDCCoord, RECT *pScreenCoord);
   BOOL Print (PWSTR pszDocName = NULL, DWORD dwStart = 1,
      DWORD dwNum = 1000000, DWORD dwFlags = 0, DWORD *pdwNumPages = NULL);
   DWORD  PrintCalcPages (void);

   BOOL HoverHelp (PCMMLNode pNode, BOOL fKeyActivated, POINT *pUL);

   BOOL Move (int iX, int iY);
   BOOL Size (int iX, int iY);
   BOOL Center (void);
   BOOL ShowWindow (int nCmdShow);
   BOOL PosnGet (RECT *pr);
   BOOL PosnSet (RECT *pr);
   BOOL TitleSet (WCHAR *psz);
   BOOL TitleGet (WCHAR *psz, DWORD dwSize);
   DWORD TimerSet (DWORD dwTime, PCEscControl pControl, DWORD dwMessage = ESCM_TIMER);
   DWORD TimerSet (DWORD dwTime, PCEscPage pPage, DWORD dwMessage = ESCM_TIMER);
   BOOL TimerKill (DWORD dwID);
   BOOL SetCursor (HCURSOR hCursor);
   BOOL SetCursor (DWORD dwID);
   BOOL IconSet (HICON hIcon);
   BOOL Beep (DWORD dwSound, DWORD dwDuration=250);
   HDC DCGet (void);
   void DCRelease (void);

   int EscWndProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);


private:
   CListFixed  m_listESCTIMERID;  // list of timers and their IDS.
   HBITMAP     m_hbitLastPage;   // bitmap cache of the visible page for simple refresh reasons
   RECT        m_rLastPage;      // m_pPage->m_rVisible used to generate m_hbitLastPage
   CEscWindow  *m_pTooltip;    // used for tooltip
   BOOL        m_fIsTimerOn;  // have a 250(?) ms timer that manages various things
   DWORD       m_dwTimerNum;  // next timer number to use
   DWORD       m_dwTitleStyle;   // current title style. DO not change
   RECT        m_rScreen;        // location ofthe window in screen coordinates
   RECT        m_rClient;        // location of client area in screen coordinates
   HCURSOR     m_hCursor;        // cursor to draw over location
   CListFixed  m_listInvalid;    // list of invalid rectangle regions that need refreshing
   HCURSOR     m_hHand;          // hand cursor
   HCURSOR     m_hNo;            // no cursor
   HMIDIOUT    m_hMIDI;          // for beeps
   CListFixed  m_listNOTEOFF;    // so know when to shut off the MIDI notes
   DWORD       m_dwMIDIOff;      // number of timer clcks before shut off midi, 0 if none
   HDC         m_hDC;            // used for DCGet and DCRelease
   DWORD       m_dwDCCount;      // used for DCGet and DCRelease
   DWORD       m_dwInstance;     // for class registration
   HDC         m_hDCPrint;       // for printing
   BOOL        m_fDeleteDCPrint; // if true, delete DC on destructor
   BOOL        m_fHasPagePainted;   // set to TRUE if the page has been painted once. Affects ScrollMe()
   DWORD       m_dwScrollBarRecurse;   // fix infinite resizing problem

   void ScrollBar (int iPos, int iHeight, int iMax);
   BOOL MouseCaptureRelease (void);
   void ScrollMe (void);
   PWSTR DialogLoop (void);
   void PrintRealInfo (PESCPRINTINFO pi, PESCPRINTINFO2 pi2);
   BOOL PrintSetMapMode (void);
   BOOL PrintPageLoad2 (PESCPAGECALLBACK pCallback, PVOID pUserData,
                        int iWidth, int iHeight);
};
typedef CEscWindow *PCEscWindow;



/*************************************************************************88
CEscSearch - Search object
*/
class DLLEXPORT CEscSearch;
typedef BOOL (__cdecl *PESCINDEXCALLBACK )(CEscSearch *pSearch, DWORD dwDocument, PVOID pUserData);

typedef struct {
   HWND                 hWndUI;              // see ::Index() with params
   PESCPAGECALLBACK     pCallback;           // see ::Index() with params
   BOOL                 fNotEnumMML;         // see ::Index() with params, except reversed
   DWORD                *pdwMMLExclude;      // see ::Index() with params
   DWORD                dwMMLExcludeCount;   // see ::Index() with params
   PWSTR                *papszIncludeFile;   // see ::Index() with params
   DWORD                dwIncludeFileCount;  // see ::Index() with params
   PESCINDEXCALLBACK    pIndexCallback;      // pointer to app-callback function for indexing
   DWORD                dwIndexDocuments;    // number of documents app has to index
   PVOID                pIndexUserData;      // application user data passed to callback
} ESCINDEX, *PESCINDEX;

typedef struct {
   DWORD                dwOldest;   // oldest acceptable date. (year << 16) | (month << 8) | (day).
                                    // year=1999,etc. month = 1..12. day=1..31
                                    // or 0 for no oldest date specified
   DWORD                dwMostRecent; // most recent acceptable date. 0 if newest date not specified
   BOOL                 fExclude;   // Use only if dwOldest | dwMostRecent. If the article's date stamp == 0
                                    // and fExclude==TRUE then it's excluded from search. Else, it's included
   BOOL                 fRamp;      // Use only if dwOldest && dwMostRecent. If date within dwOldest && dwMostRecent
                                    // then score is ramped so if occurs on dwOldest, score=0, and if occurs
                                    // at most recent date is maximum
   BOOL                 *pafUseCategory; // pointer to an array of bools (# elem = dwUseCategoryCount)
                                    // specifying if each category is to be used (TRUE) in search or not
                                    // if category outside dwUseCategoryCount then assume use
   DWORD                dwUseCategoryCount;  // number of cateogories pointed to by pafUseCategory
} ESCADVANCEDSEARCH, *PESCADVANCEDSEARCH;

class DLLEXPORT CEscSearch {

public:
   CListVariable     m_listFound;      // List of search results, sorted by highest score
                                       // Each element contains:
                                       // DWORD - score. range from 0 to 100,000
                                       // WCHAR[] - NULL-terminated document name from PageInfo
                                       // WCHAR[] - NULL-terminated section name, from PageInfo
                                       // WCHAR[] - Either filename or "r:XXX" (resource number)
                                       //       may be appended with "#YYY" where YYY is the section name
   PWSTR             m_pszLastSearch;  // last search. may be null

   CEscSearch(void);
   ~CEscSearch(void);
   void* operator new (size_t);
   void  operator delete (void*);

   BOOL Init (HINSTANCE hInstance, DWORD dwAppVersion, PWSTR pszFile = NULL);
   BOOL NeedIndexing (void);
   BOOL Index (HWND hWndUI, PESCPAGECALLBACK pCallback,
      BOOL fEnumMML = TRUE, DWORD *pdwMMLExclude = NULL, DWORD dwMMLExcludeSize = 0,
      PWSTR *papszIncludeFile = NULL, DWORD dwIncludeFile = NULL);
   BOOL Index (PESCINDEX pIndex);
   BOOL Search (PWSTR pszInput, PESCADVANCEDSEARCH pInfo = NULL);
   BOOL IndexText (PWSTR psz);   // index the text into m_treeWordsInSection
   BOOL IndexNode (PCMMLNode pNode);   // index the current node and its children
   BOOL SectionFlush (PWSTR pszDocName, PWSTR pszSectionName, PWSTR pszLinkData,
      DWORD dwDate = 0, DWORD dwCategory = 0);     // flush the current section to m_treeWords

   BYTE        m_bCurRelevence;  // current word relevence to search. Default 32. Used by index callback

   friend BOOL GenerateIndexPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam);

private:
   HINSTANCE   m_hInstance;      // from INIT
   BOOL        m_fLoaded;        // set to TRUE if have already loaded or tried to load file
   WCHAR       m_pszFile[256];   // file name
   BOOL        m_fNeedIndexing;  // set to TRUE if cant find the file
   CBTree      m_treeWords;      // tree indexed by words. Contains a DWORD to indicate the
                                 // number of words, followed by that many DWORDS. The low 3-bytes
                                 // are the document number, and the hi-byte is a linear score
                                 // for the document if it has the word.
   CListVariable m_listDocuments;// list of documents that were indexed.
                                 // Each element contains:
                                 // WCHAR[] - NULL-terminated document name from PageInfo
                                 // WCHAR[] - NULL-terminated section name, from PageInfo
                                 // WCHAR[] - Either filename or "r:XXX" (resource number)
                                 //       may be appended with "#YYY" where YYY is the section name
   CMem        m_memLastSearch;  // where the last search string is stored
   DWORD       m_dwFileVersion;  // version info written in the file
   DWORD       m_dwAppVersion;   // version of the application
   CListFixed  *m_plistEnum;     // list of resources to enumerate through
   PWSTR       *m_papszFileEnum;  // list of files to enum through
   DWORD       m_dwNumFileEnum;  // number of files to enum through
   DWORD       m_dwCurEnum;      // current element being enumerated
   PESCPAGECALLBACK m_pCallback; // from Index() call
   PWSTR       m_pszTitle;       // node title, found during search
   PWSTR       m_pszCurDocument; // current document that scanning
   WCHAR       m_szCurSection[512];  // current seciton. May be 0's
   CBTree      m_treeWordsInSection;   // words found in the current section.
                                 // value contains a DWORD - storing maximum word score found
   PESCINDEXCALLBACK m_pIndexCallback;      // pointer to app-callback function for indexing
   DWORD       m_dwIndexDocuments;    // number of documents app has to index
   PVOID       m_pIndexUserData;      // application user data passed to callback

   BOOL LoadIfNotLoaded (void);  // load the file if it's not already loaded
   BOOL IndexNodeInternal (PCMMLNode pNode);   // index the current node and its children. Internal
   BOOL SectionFlush (void);     // flush the current section to m_treeWords
};
typedef CEscSearch *PCEscSearch;


/***************************************************************************
Useful functions
*/
DLLEXPORT BOOL EscControlAdd (PWSTR psz, PESCCONTROLCALLBACK pCallback);
DLLEXPORT PESCCONTROLCALLBACK EscControlGet (PWSTR psz);

// to convert attributes to values
DLLEXPORT BOOL AttribToHex (WCHAR *psz, DWORD *pRet);
DLLEXPORT BOOL AttribToDecimal (WCHAR *psz, int *pRet);
DLLEXPORT BOOL AttribToColor (WCHAR *psz, COLORREF *pRet);
DLLEXPORT BOOL AttribToDecimalOrPercent (WCHAR *psz, BOOL *pfPercent, int *pRet);
DLLEXPORT BOOL AttribToPositioning (WCHAR *psz, DWORD *pRet);
DLLEXPORT BOOL AttribToYesNo (WCHAR *psz, BOOL *pRet);
DLLEXPORT BOOL AttribToPercent (WCHAR *psz, int *pRet);
DLLEXPORT BOOL AttribToAccelerator (WCHAR *psz, PESCACCELERATOR pRet);
DLLEXPORT BOOL AttribToDouble (WCHAR *psz, double *pRet);
DLLEXPORT BOOL AttribTo3DPoint (WCHAR *psz, double *pRet);
DLLEXPORT void ColorToAttrib (PWSTR psz, COLORREF cr);

// conversion to & from MML
DLLEXPORT BOOL StringToMMLString (WCHAR *pszString, WCHAR *pszMML, DWORD dwSize, DWORD *pdwNeeded);

// message box
DLLEXPORT DWORD EscMessageBox (HWND hWnd, PWSTR pszTitle, PWSTR pszSummary, PWSTR pszFinePrint, DWORD dwType);

// remapping for debug purposes
DLLEXPORT void EscRemapJPEG (DWORD dwID, PWSTR pszFile);
DLLEXPORT void EscRemapBMP (DWORD dwID, PWSTR pszFile);
DLLEXPORT void EscRemapMML (DWORD dwID, PWSTR pszFile);

// controlling sound features
#define  ESCS_CLICKS       0x0001      // clicks when buttons & stuff pressed
#define  ESCS_SPEAK        0x0002      // speech is turned on
#define  ESCS_CHIME        0x0004      // play chimes
#define  ESCS_ALL          (ESCS_CLICKS | ESCS_SPEAK | ESCS_CHIME)
DLLEXPORT void EscSoundsSet (DWORD dwFlags);
DLLEXPORT DWORD EscSoundsGet (void);

// bitmap functions
DLLEXPORT BOOL BMPSize (HBITMAP hBmp, int *piWidth, int *piHeight);
DLLEXPORT void BMPTransparentBlt (HBITMAP hbmpImage, HBITMAP hbmpMask, HDC hDCInto,
                     RECT *prInto, RECT *prFrom, RECT *prClip);
DLLEXPORT int EscGetDeviceCaps (HDC hDC, int nIndex);
DLLEXPORT BOOL EscStretchBlt(  HDC hdcDest,      // handle to destination device context
  int nXOriginDest, // x-coordinate of upper-left corner of dest. rectangle
  int nYOriginDest, // y-coordinate of upper-left corner of dest. rectangle
  int nWidthDest,   // width of destination rectangle
  int nHeightDest,  // height of destination rectangle
  HDC hdcSrc,       // handle to source device context
  int nXOriginSrc,  // x-coordinate of upper-left corner of source rectangle
  int nYOriginSrc,  // y-coordinate of upper-left corner of source rectangle
  int nWidthSrc,    // width of source rectangle
  int nHeightSrc,   // height of source rectangle
  DWORD dwRop,       // raster operation code
  HBITMAP hbmpSource);
DLLEXPORT BOOL EscBitBlt( HDC hdcDest, // handle to destination device context
  int nXDest,  // x-coordinate of destination rectangle's upper-left 
               // corner
  int nYDest,  // y-coordinate of destination rectangle's upper-left 
               // corner
  int nWidth,  // width of destination rectangle
  int nHeight, // height of destination rectangle
  HDC hdcSrc,  // handle to source device context
  int nXSrc,   // x-coordinate of source rectangle's upper-left 
               // corner
  int nYSrc,   // y-coordinate of source rectangle's upper-left 
               // corner
  DWORD dwRop,  // raster operation code
  HBITMAP hbmpSource);

// font scaling
DLLEXPORT void EscFontScaleSet (WORD wScale);
DLLEXPORT WORD EscFontScaleGet (void);

// register
DLLEXPORT BOOL EscInitialize (PWSTR pszEmail, DWORD dwRegKey, DWORD dwFlags);
DLLEXPORT void EscUninitialize (void);

// speaking
#define  ESCSPEAK_STOPPREVIOUS      0x0001   // if already speaking stop whatever was speaking
#define  ESCSPEAK_WAITFORCHIME      0x0002   // wait until after the currently playing chime finished
DLLEXPORT BOOL EscSpeak (PWSTR psz, DWORD dwFlags = ESCSPEAK_STOPPREVIOUS | ESCSPEAK_WAITFORCHIME);
DLLEXPORT PVOID EscSpeakTTS (void);

// chimes
typedef struct {
   DWORD       dwDelay;    // delay (in ms) between this event and the previous one
   DWORD       dwMIDI;     // MIDI message. See Windows API midiOutShortMsg().
} ESCMIDIEVENT, *PESCMIDIEVENT;

#define ESCCHIME_INFORMATION     0x0001 // - Tell user there's information
#define ESCCHIME_WARNING         0x0002 // - Tell user there's a problem
#define ESCCHIME_ERROR           0x0003 // - Tell user there's an error
#define ESCCHIME_QUESTION        0x0004 // - Question
#define ESCCHIME_HOVERFHELP      0x0005 // - For help

#define  MIDINOTEON(channel,note,volume)     ((0x90 | channel) | (((DWORD) note) << 8) | (((DWORD) volume) << 16))
#define  MIDINOTEOFF(channel,note)           ((0x80 | channel) | (((DWORD) note) << 8))
#define  MIDIINSTRUMENT(channel,instrument)  ((0xc0 | channel) | (((DWORD) instrument) << 8))

DLLEXPORT BOOL EscChime (PESCMIDIEVENT paMidi, DWORD dwNum);
DLLEXPORT BOOL EscChime (DWORD dwNum);

// date utility function
DLLEXPORT int EscDaysInMonth (int iMonth, int iYear, int *piDay);
DLLEXPORT BOOL JPegToBitmap (char *szJPeg, char *szBmp);
DLLEXPORT HBITMAP JPegToBitmap (char *szJPeg);
DLLEXPORT HBITMAP JPegToBitmap (DWORD dwID, HINSTANCE hInstance);


DLLEXPORT void EscTraditionalCursorSet (BOOL fUse);
DLLEXPORT BOOL EscTraditionalCursorGet (void);

#endif //  _ESCARPMENT_H_

