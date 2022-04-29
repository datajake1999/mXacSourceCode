/**********************************************************************************
Texture.h - internal header file
*/

#ifndef _TEXTURE_H_
#define _TEXTURE_H_


/***************************************************************************************
AlgoText.cpp */
#define BACKCOLORRGB    RGB(0,1,2)

typedef struct {
   PVOID                pThis;      // whichever object is being edited
   DWORD                dwRenderShard; // render shard to use
   PCTextCreatorSocket  pCreator; // texture creator
   HBITMAP              hBit;       // current bitmap being displayed
   BOOL                 fDrawFlat;  // if TRUE then draw as flat, else draw as sphere
   BOOL                 fStretchToFit;  // if TRUE then stretch to fit, else draw to scale
} TEXTEDITINFO, *PTEXTEDITINFO;

BOOL TEHelperMessageHook (PCEscPage pPage, DWORD dwMessage, PVOID pParam);
void MassageForCreator (PCImage pImage, PCMaterial pMat, PTEXTINFO pInfo, BOOL fFlatten = FALSE);
void DrawHair (PCImage pImage, WORD *pawColor, DWORD dwNum,
                              PCPoint papPoint, fp fWidth, BOOL fAutoDepth,
                              BOOL fHair, PCMaterial pMaterial);

fp TextureDetailApply (DWORD dwRenderShard, fp fPixelLen);
void TextureDetailApply (DWORD dwRenderShard, DWORD dwWidth, DWORD dwHeight, DWORD *pdwWidth, DWORD *pdwHeight);
void ColorDarkenAtBottom (PCImage pImage, COLORREF cTrans, fp fScaleAmt);
void ColorToTransparency (PCImage pImage, COLORREF cTrans, DWORD dwDist, PCMaterial pMat);
void DrawLineSegment (PCImage pImage, PCPoint pStart, PCPoint pEnd, fp fWidthStart, fp fWidthEnd,
                      COLORREF cColor, fp fZDelta);
void DrawCurvedLineSegment (PCImage pImage, DWORD dwNum, PCPoint pPoint,
                            fp fWidthStart, fp fWidthMiddle, fp fWidthEnd,
                            COLORREF cColorStart, COLORREF cColorEnd,
                            fp fZDelta);


/***************************************************************************************
CTextCreatorFaceomatic.cpp */

/****************************************************************************
CSubTexture - Used for storing information about a subtexture within a texture
*/
class CSubTexture {
public:
   ESCNEWDELETE;

   CSubTexture (void);
   ~CSubTexture (void);

   CSubTexture *Clone (void);
   BOOL CloneTo (CSubTexture *pTo);
   PCMMLNode2 MMLTo (void);
   BOOL MMLFrom (PCMMLNode2 pNode);
   void Clear (BOOL fClearAll = FALSE, BOOL fShutDown = FALSE);

   BOOL DialogSelect (DWORD dwRenderShard, HWND hWnd);
   void BoundaryGet (PTEXTUREPOINT ptpUL, PTEXTUREPOINT ptpLR);
   void BoundarySet (PTEXTUREPOINT ptpUL, PTEXTUREPOINT ptpLR);
   DWORD MirrorGet (void);
   BOOL MirrorSet (DWORD dwMirror);
   void ScaleSet (fp fMasterWidth, fp fMasterHeight);
   void StretchToFitSet (BOOL fStretchToFit);
   PCMaterial MaterialGet (DWORD dwThread)
      {
         return &m_pObjectSurface->m_Material;
      };
   
   void FillTexture (DWORD dwRenderShard, fp fWidth, fp fHeight, fp fPixelLen, BOOL fHighQuality,
      PCImage pImage, PCImage pImage2, PCMaterial pMaterial, fp *pfGlowScale);

   BOOL TextureCache (DWORD dwRenderShard);
   BOOL TextureRelease (void);
   BOOL FillPixel (DWORD dwThread, DWORD dwFlags, WORD *pawColor, PTEXTPOINT5 pText, PTEXTPOINT5 pMax,
      PCMaterial pMat, float *pafGlow, BOOL fHighQuality);
   BOOL PixelBump (DWORD dwThread, PTEXTPOINT5 pText, PTEXTPOINT5 pRight,
                             PTEXTPOINT5 pDown, PTEXTUREPOINT pSlope, fp *pfHeight = NULL, BOOL fHighQuality = FALSE);
   COLORREF AverageColorGet (DWORD dwThread, BOOL fGlow);

   HBITMAP BitmapGet (DWORD dwRenderShard, DWORD dwWidth, DWORD dwHeight, BOOL fSphere, BOOL fStretchToFit, BOOL fForceRecalc /*= FALSE*/);
   BOOL PageTrapMessages (DWORD dwRenderShard, PWSTR pszPrefix, PCEscPage pPage, DWORD dwMessage, PVOID pParam);
   BOOL TextureQuery (DWORD dwRenderShard, PCListFixed plText, PCBTree pTree);
   BOOL SubTextureNoRecurse (DWORD dwRenderShard, PCListFixed plText);

   // stored in MML, and can access freely
   BOOL                 m_fUse;           // set to TRUE if can use. Not used internally, but can store values

private:
   BOOL PixelInRangeAndMirror (PTEXTPOINT5 pOrig, PTEXTPOINT5 pMirror);

   // stored in MML
   PCObjectSurface      m_pObjectSurface;  // material (sub-texture)
   TEXTUREPOINT         m_atpBoundary[2]; // [0] = UL boundary, [1] = LR boundary
   DWORD                m_dwMirror;       // 0 if none, 1 is left is mirrored, 2 if right is mirrored, 3 if left-to-right

   // probably want to set
   BOOL                 m_fStretchToFit;  // if TRUE stretch texture over range in master, else
                                          // use appropriate scaling
   TEXTUREPOINT         m_tpMaster;       // .h = master width in meters, .v = master height in meters

   // temporary
   PCTextureMapSocket   m_pTexture;    // valid only while texture is cached and uncached for getting info
   BOOL                 m_fBmpStretched;  // set to TRUE if the current bitmap is stretched
   BOOL                 m_fBmpSphere;  // set to TRUE if the current bitmap is a sphere
   BOOL                 m_fBmpDirty;   // set to TRUE if bitmap is dirty
   DWORD                m_dwBmpWidth;  // bitmap width, in cache
   DWORD                m_dwBmpHeight; // bitmap height, in cache
   HBITMAP              m_hBmp;        // bitmap sample of material
};
typedef CSubTexture *PCSubTexture;

class CTextCreatorFaceomatic;
typedef CTextCreatorFaceomatic *PCTextCreatorFaceomatic;

/* CFaceMakeup */
class CFaceMakeup : public CEscMultiThreaded {
public:
   ESCNEWDELETE;

   CFaceMakeup (void);
   ~CFaceMakeup (void);

   BOOL MMLFrom (PCMMLNode2 pNode);
   PCMMLNode2 MMLTo (void);
   CFaceMakeup *Clone (void);
   BOOL CloneTo (CFaceMakeup *pTo);
   BOOL Render (DWORD dwRenderShard, PCImage pImage, PCImage pImage2, PCTextCreatorFaceomatic pFace);
   BOOL Dialog (DWORD dwRenderShard, PCEscWindow pWindow, PCTextCreatorFaceomatic pFace);
   BOOL TextureQuery (DWORD dwRenderShard, PCListFixed plText, PCBTree pTree);
   BOOL SubTextureNoRecurse (DWORD dwRenderShard, PCListFixed plText);
   void Clear (void);
   void ScaleSet (fp fMasterWidth, fp fMasterHeight);

   // CEscMultiThreaded
   virtual void EscMultiThreadedCallback (PVOID pParams, DWORD dwParamSize,
      DWORD dwThread);

   // save to MML
   CSubTexture       m_Coverage;       // overall makeup coverage, on/off from here is used
   CSubTexture       m_Makeup;         // repeating makeup texture
   CPoint            m_pCoverageAmt;   // p[0] = coverage for black, p[1] = coverage for white, p[2] = coverage off texture
   BOOL              m_fAffectSpecularity; // if TRUE makeup affects specularity. else just discoloration
   BOOL              m_fDiscoloration; // if TRUE then discoloration dependent upon change of makeup from it's average color
   fp                m_fDiscolorationScale;  // how much to scale the change
   BOOL              m_fBeforeWarts;   // if TRUE, apply this before the warts

private:
};
typedef CFaceMakeup *PCFaceMakeup;


/* CFaceWarts */
class CFaceWarts {
public:
   ESCNEWDELETE;

   CFaceWarts (void);
   ~CFaceWarts (void);

   BOOL MMLFrom (PCMMLNode2 pNode);
   PCMMLNode2 MMLTo (void);
   CFaceWarts *Clone (void);
   BOOL CloneTo (CFaceWarts *pTo);
   BOOL Render (DWORD dwRenderShard, PCImage pImage, PCImage pImage2, PCTextCreatorFaceomatic pFace);
   BOOL Dialog (DWORD dwRenderShard, PCEscWindow pWindow, PCTextCreatorFaceomatic pFace);
   BOOL TextureQuery (DWORD dwRenderShard, PCListFixed plText, PCBTree pTree);
   BOOL SubTextureNoRecurse (DWORD dwRenderShard, PCListFixed plText);
   void Clear (void);
   void ScaleSet (fp fMasterWidth, fp fMasterHeight);

   // save to MML
   CSubTexture       m_Coverage;       // overall Warts coverage, on/off from here is used
   CSubTexture       m_Discolor;       // repeating Warts texture
   CPoint            m_pCoverageAmt;   // p[0] = coverage for black, p[1] = coverage for white, p[2] = coverage off texture
   BOOL              m_fAffectSpecularity; // if TRUE Warts affects specularity. else just discoloration
   CPoint            m_pSize;          // p[0] = avg distance, p[1] = avg diameter, p[2] = variation
   CPoint            m_pHeight;        // p[0] = height in meters, p[1] = pointyness
   CPoint            m_pColoration;    // p[0] = blend amount, p[1] = pointyness
   int               m_iSeed;          // seed to use for random

private:
   BOOL RenderWart (DWORD dwRenderShard, PCImage pImage, PCImage pImage2, PCTextCreatorFaceomatic pFace,
                             fp fX, fp fY, fp fDiameter, fp fHeight, fp fColoration);
};
typedef CFaceWarts *PCFaceWarts;


/* CFaceScars */
class CFaceScars : public CEscMultiThreaded {
public:
   ESCNEWDELETE;

   CFaceScars (void);
   ~CFaceScars (void);

   BOOL MMLFrom (PCMMLNode2 pNode);
   PCMMLNode2 MMLTo (void);
   CFaceScars *Clone (void);
   BOOL CloneTo (CFaceScars *pTo);
   BOOL Render (DWORD dwRenderShard, PCImage pImage, PCImage pImage2, PCTextCreatorFaceomatic pFace);
   BOOL Dialog (DWORD dwRenderShard, PCEscWindow pWindow, PCTextCreatorFaceomatic pFace);
   BOOL TextureQuery (DWORD dwRenderShard, PCListFixed plText, PCBTree pTree);
   BOOL SubTextureNoRecurse (DWORD dwRenderShard, PCListFixed plText);
   void Clear (void);
   void ScaleSet (fp fMasterWidth, fp fMasterHeight);
   fp IsOverScar (PTEXTPOINT5 pLoc);


   // CEscMultiThreaded
   virtual void EscMultiThreadedCallback (PVOID pParams, DWORD dwParamSize,
      DWORD dwThread);

   // save to MML
   CSubTexture       m_Coverage;       // overall Scars coverage, on/off from here is used
   CSubTexture       m_Discolor;       // repeating Scars texture
   CPoint            m_pCoverageAmt;   // p[0] = coverage for black, p[1] = coverage for white, p[2] = coverage off texture
   BOOL              m_fAffectSpecularity; // if TRUE Scars affects specularity. else just discoloration
   BOOL              m_fAffectFur;     // set to TRUE if cuts out fur
   fp                m_fHeight;        // height/depth of scar in meters
   fp                m_fColoration;    // how much scar discolors at full value
};
typedef CFaceScars *PCFaceScars;


// HAIRCENTER - Center for hair direction
typedef struct {
   BOOL           fUse;       // set to TRUE if want to use
   TEXTUREPOINT   tpCenter;   // center of the swirl, 0..1
   TEXTUREPOINT   tpSize;     // width and height of the swirl
   fp             fStrength;  // strength of the swirl
} HAIRCENTER, *PHAIRCENTER;


/* CFaceFur */
#define MAXHAIR      4     // number of hair centers
class CFaceFur : public CEscMultiThreaded {
public:
   ESCNEWDELETE;

   CFaceFur (void);
   ~CFaceFur (void);

   BOOL MMLFrom (PCMMLNode2 pNode);
   PCMMLNode2 MMLTo (void);
   CFaceFur *Clone (void);
   BOOL CloneTo (CFaceFur *pTo);
   BOOL Render (DWORD dwRenderShard, PCImage pImage, PCImage pImage2, PCTextCreatorFaceomatic pFace, PCImage pImageZ);
   BOOL Dialog (DWORD dwRenderShard, PCEscWindow pWindow, PCTextCreatorFaceomatic pFace);
   BOOL TextureQuery (DWORD dwRenderShard, PCListFixed plText, PCBTree pTree);
   BOOL SubTextureNoRecurse (DWORD dwRenderShard, PCListFixed plText);
   void Clear (void);
   void ScaleSet (fp fMasterWidth, fp fMasterHeight);

   // CEscMultiThreaded
   virtual void EscMultiThreadedCallback (PVOID pParams, DWORD dwParamSize,
      DWORD dwThread);

   // save to MML
   CSubTexture       m_Coverage;       // overall Fur coverage, on/off from here is used
   CPoint            m_pCoverageAmt;   // p[0] = coverage for black, p[1] = coverage for white, p[2] = coverage off texture
   int               m_iSeed;          // seed to use for random

   CSubTexture       m_Length;         // fur length
   CSubTexture       m_Color;          // fur color
   CPoint            m_pLenVal;        // p[0] = length for black, p[1] = length for white, p[2] = length off texture
   CPoint            m_pHair;          // p[0] = avg distance, p[1] = avg diameter, p[2] = angle (radians)
   CPoint            m_pVariation;     // p[0] = color variation, p[1] = length variation, p[2] = direction variation, p[3] = angle variation
   HAIRCENTER        m_aHAIRCENTER[MAXHAIR]; // number of hair centers

private:
   void HairDirection (fp fH, fp fV, PCPoint pDir);
   BOOL RenderHair (DWORD dwRenderShard, fp fH, fp fV, PCImage pImage, PCImage pImage2, PCTextCreatorFaceomatic pFace, PCImage pImageZ);

   // stored away for internal used
   TEXTUREPOINT      m_tpScale;        // width and height of actual image
};
typedef CFaceFur *PCFaceFur;


/* CTextCreatorFaceomatic*/
#define MAXMAKEUP    4        // 4 makeup layers
#define MAXWARTS     4        // 4 warts layers
#define MAXSCARS     4        // 4 scar layers
#define MAXFUR       2        // 1 fur layer

class CTextCreatorFaceomatic : public CEscMultiThreaded, public CTextCreatorSocket {
public:
   ESCNEWDELETE;

   CTextCreatorFaceomatic (DWORD dwRenderShard, DWORD dwType);
   ~CTextCreatorFaceomatic (void);

   virtual void Delete (void);
   virtual BOOL MMLFrom (PCMMLNode2 pNode);
   virtual PCMMLNode2 MMLTo (void);
   virtual BOOL Render (PCImage pImage, PCImage pImage2, PCMaterial pMaterial, PTEXTINFO pTextInfo);
   virtual BOOL Dialog (PCEscWindow pWindow);
   virtual BOOL TextureQuery (PCListFixed plText, PCBTree pTree, GUID *pagThis);
   virtual BOOL SubTextureNoRecurse (PCListFixed plText, GUID *pagThis);

   // CEscMultiThreaded
   virtual void EscMultiThreadedCallback (PVOID pParams, DWORD dwParamSize,
      DWORD dwThread);

   DWORD             m_dwRenderShard;
   CMaterial         m_Material;  // material to use

   fp                m_fWidth;      // width, in meters
   fp                m_fHeight;     // height, in meters
   fp                m_fPixelLen;   // pixel size, in meters

   CSubTexture       m_Base;     // base sub-texture
   CSubTexture       m_Oily;     // oily skin texture
   CPoint            m_pOilyShiny;  // p[0] = shininess for black, p[1] = shininess for white, p[2] = shininess off texture
   PCFaceMakeup      m_apMakeup[MAXMAKEUP];   // makeup layers
   PCFaceWarts       m_apWarts[MAXWARTS];     // warts layers
   PCFaceScars       m_apScars[MAXSCARS];     // scars layers
   PCFaceFur         m_apFur[MAXFUR];         // max fur layers
};
typedef CTextCreatorFaceomatic *PCTextCreatorFaceomatic;

/******************************************************************************
CTextEffectSocket - Texture effects should be subclassed from this so their
calling convention is more systematic. NOTE: Not using virtual because object
calling into texture effect will always be aware of what it is.
*/

class CTextEffectSocket {
public:
   BOOL MMLFrom (PCMMLNode2 pNode);
      // object reads in information about itself from MML.

   PCMMLNode2 MMLTo (void);
      // object writes information about itself to MML

   void Apply (PCImage pImage, fp fPixelLen);
      // applies the texture effect onto the image passed into pImage.
      // fPixelLen is the number of meters per pixel width/height

   BOOL Dialog (PCEscWindow pWindow, PCTextCreatorSocket pCreator);
      // tells the object to display a page allowing the user to edit the information.
      // Should return TRUE if the user pressed Back, FALSE if they closed the window
      // PCreator is the creator socket
};


/***************************************************************************************
CTextEffectNoise - Noise texture effect
*/
class CTextEffectNoise : public CTextEffectSocket  {
public:
   ESCNEWDELETE;

   CTextEffectNoise (void);
   BOOL MMLFrom (PCMMLNode2 pNode);
   PCMMLNode2 MMLTo (void);
   void Apply (PCImage pImage, fp fPixelLen);
   BOOL Dialog (PCEscWindow pWindow, PCTextCreatorSocket pCreator);

   BOOL        m_fTurnOn;          // if true the turn this on, ele sdisabled
   fp          m_fNoiseX;          // noise size (x) in meters
   fp          m_fNoiseY;          // noise size (y) in meters
   fp          m_fZDeltaMax;       // Z-level change in meters
   fp          m_fZDeltaMin;       // Z-level change in meters
   COLORREF    m_cMax;             // color to use at maximum
   COLORREF    m_cMin;             // color to use at minimum
   fp          m_fTransMax;        // Transparency at maximum. 0=opaque,1=transparent
   fp          m_fTransMin;        // transparency at minimum
};
typedef CTextEffectNoise *PCTextEffectNoise;

/***********************************************************************************
CTextCreatorTreeBark - Noise functions
*/

class CTextCreatorTreeBark : public CTextCreatorSocket {
public:
   ESCNEWDELETE;

   CTextCreatorTreeBark (DWORD dwRenderShard, DWORD dwType);
   virtual void Delete (void);
   virtual BOOL MMLFrom (PCMMLNode2 pNode);
   virtual PCMMLNode2 MMLTo (void);
   virtual BOOL Render (PCImage pImage, PCImage pImage2, PCMaterial pMaterial, PTEXTINFO pTextInfo);
   virtual BOOL Dialog (PCEscWindow pWindow);

   DWORD             m_dwRenderShard;
   CMaterial         m_Material;  // material to use
   DWORD             m_dwType; // initial type of tile - when constructed
   int               m_iSeed;   // seed for the random
   fp                m_fPixelLen;  // meters per pixel
   fp                m_fPatternWidth; // pattern width in meters
   fp                m_fPatternHeight;   // pattern height in meters

   COLORREF          m_cColor;  // basic color
   COLORREF          m_cColorChannel;  // basic color
   COLORREF          m_cColorEdge;  // basic color

   TEXTUREPOINT      m_tpNoise;     // noise size, x and y
   fp                m_fDepth;      // depth of the channel
   fp                m_fDepthSquare;   // 0..1, how square the depth is
   fp                m_fBarkChannel;   // 0..1, how much bark is in the channel
   fp                m_fNoiseDetail;   // 0..1, recursion detail of noise
   fp                m_fBlendEdge;  // blend in edge color
   fp                m_fBlendChannel;  // blend in channel color

   TEXTUREPOINT      m_tpNoiseRib;  // noise size, x and y
   fp                m_fRibWidth;   // width in meters
   fp                m_fRibNoise;   // how much the noise affects L/R in meters
   fp                m_fRibStrength;   // 0..1, how strong compared to the bark noise
   CTextEffectNoise  m_aNoise[2];  // introduce noise to surface
};
typedef CTextCreatorTreeBark *PCTextCreatorTreeBark;


/***********************************************************************************
CTextCreatorMix
*/

class CTextCreatorMix : public CTextCreatorSocket {
public:
   ESCNEWDELETE;

   CTextCreatorMix (DWORD dwRenderShard, DWORD dwType);
   virtual void Delete (void);
   virtual BOOL MMLFrom (PCMMLNode2 pNode);
   virtual PCMMLNode2 MMLTo (void);
   virtual BOOL Render (PCImage pImage, PCImage pImage2, PCMaterial pMaterial, PTEXTINFO pTextInfo);
   virtual BOOL Dialog (PCEscWindow pWindow);
   virtual BOOL TextureQuery (PCListFixed plText, PCBTree pTree, GUID *pagThis);
   virtual BOOL SubTextureNoRecurse (PCListFixed plText, GUID *pagThis);

   DWORD             m_dwRenderShard;
   // CMaterial         m_Material;  // material to use
   DWORD             m_dwType; // initial type of tile - when constructed
   fp                m_fPixelLen;  // meters per pixel
   fp                m_fPatternWidth; // pattern width in meters
   fp                m_fPatternHeight;   // pattern height in meters

   CSubTexture       m_TextA;       // subtexture A
   CSubTexture       m_TextB;       // subtexture B

   // Mix info: 0 = from A, 1 = from B, 2 = (A+B)/2, 3 = 0
   // For non-color: 10 = A's color, 11 = B's color
   // For transparency only: 12 = A's color inverse, 13 = B's color inverse
   DWORD             m_dwMixColor;  // what colors from where
   DWORD             m_dwMixSpec;   // where specularity comes from
   DWORD             m_dwMixTrans;  // where transparency comes from
   DWORD             m_dwMixBump;   // where bump maps come from
   DWORD             m_dwMixGlow;   // where glow comes from
   fp                m_fMixBumpScale;  // how much to scale bump maps
   fp                m_fMixGlowScale;  // much much to scale glow
};
typedef CTextCreatorMix *PCTextCreatorMix;



/***********************************************************************************
CTextCreatorText
*/


class CTextEffectText : public CTextEffectSocket  {
public:
   ESCNEWDELETE;

   CTextEffectText (void);
   BOOL MMLFrom (PCMMLNode2 pNode);
   PCMMLNode2 MMLTo (void);
   void Apply (PCImage pImage, PCImage pImage2, fp *pfGlowScale, fp fPixelLen);
   BOOL Dialog (PCEscWindow pWindow, PCTextCreatorSocket pCreator);

   BOOL DrawTextToImage (PWSTR pszText, PCImage pImage,
                                       PCImage pImage2, fp *pfGlowScale,
                                       RECT *prBound, int iLRAlign, int iTBAlign,
                                       PLOGFONT plf, COLORREF cColor, HWND hWnd, fp fPixelLen);

   BOOL        m_fTurnOn;        // if true the turn this on, ele sdisabled
   CMaterial   m_Material;       // material for the text
   CPoint               m_pPosn; // position from 0..1. [0] = Left, [1] = top, [2] = right, [3] = bottom
   CMem                 m_memText;      // string to be used. Must be freed
   LOGFONT              m_lf;      // font to use
   COLORREF             m_cColor;  // color
   int                  m_iLRAlign;// left/right align
   int                  m_iTBAlign;// top/bottom align

   COLORREF             m_cGlow;          // glow color
   BOOL                 m_fTextRaiseAdd; // set to TRUE if add the raising, FALSE if set absolute
   fp                   m_fColorOpacity;  // from 0..1, how opaque the color is
   fp                   m_fGlowOpacity;   // from 0..1, how opaque the glow is
   fp                   m_fGlowScale;     // how much to scale the glow
   fp                   m_fTextRaise;    // how much the text is raised, in meters
};
typedef CTextEffectText *PCTextEffectText;


#define NUMTEXTLINES       4     // 4 lines of text

class CTextCreatorText : public CTextCreatorSocket {
public:
   ESCNEWDELETE;

   CTextCreatorText (DWORD dwRenderShard, DWORD dwType);
   virtual void Delete (void);
   virtual BOOL MMLFrom (PCMMLNode2 pNode);
   virtual PCMMLNode2 MMLTo (void);
   virtual BOOL Render (PCImage pImage, PCImage pImage2, PCMaterial pMaterial, PTEXTINFO pTextInfo);
   virtual BOOL Dialog (PCEscWindow pWindow);
   virtual BOOL TextureQuery (PCListFixed plText, PCBTree pTree, GUID *pagThis);
   virtual BOOL SubTextureNoRecurse (PCListFixed plText, GUID *pagThis);

   DWORD             m_dwRenderShard;  // rendering shard
   // CMaterial         m_Material;  // material to use
   DWORD             m_dwType; // initial type of tile - when constructed
   fp                m_fPixelLen;  // meters per pixel
   fp                m_fPatternWidth; // pattern width in meters
   fp                m_fPatternHeight;   // pattern height in meters

   CSubTexture       m_Texture;       // subtexture
   CTextEffectText   m_aText[NUMTEXTLINES];  // text lines
};
typedef CTextCreatorText *PCTextCreatorText;



/***********************************************************************************
CTextCreatorGrass - Noise functions
*/

// CLeafForTexture - Stores information about a leaf for a texture
class CLeafForTexture {
public:
   ESCNEWDELETE;

   CLeafForTexture (void);
   ~CLeafForTexture (void);

   BOOL CloneTo (CLeafForTexture *pTo);
   CLeafForTexture *Clone (void);
   PCMMLNode2 MMLTo (void);
   BOOL MMLFrom (PCMMLNode2 pNode);
   void Clear (void);

   fp FindMaxZ (PCImage pImage,fp fScale, PCPoint pStart, PCPoint pDirection);
   fp FindMinZInTexture (void);
   void Render (PCImage pImage, fp fBrightness, fp fScale, PCPoint pStart, PCPoint pDirection,
      fp fZDelta = -ZINFINITE);
   BOOL TextureCache (DWORD dwRenderShard);
   BOOL TextureRelease (void);
   //COLORREF AverageColor (void);
   BOOL TextureQuery (DWORD dwRenderShard, PCListFixed plText, PCBTree pTree);
   BOOL SubTextureNoRecurse (DWORD dwRenderShard, PCListFixed plText);
   BOOL Dialog (DWORD dwRenderShard, PCEscWindow pWindow);

// private:
   BOOL              m_fUseText;    // if TRUE use the sub-texture, else the image
   fp                m_fWidth;      // width, in pixels
   fp                m_fHeight;     // length, in pixels
   fp                m_fSizeVar;    // size variation, 0..1
   fp                m_fWidthVar;   // width variation, 0..1... Won't ever stretch, but may shrink
   fp                m_fHeightVar;  // height variation, 0..1... Won't ever stretch, but may shrink
   fp                m_fHueVar;     // hue variation, 0..1
   fp                m_fSatVar;     // saturation variation, 0..1
   fp                m_fLightVar;   // lightness variation, 0..1
   fp                m_fDarkEdge;   // dark edges around leaf, 0..2. 1.0 = none. 0.5 = default. 1.5 = lighter.
   BOOL              m_fAllowMirror;   // if TRUE, 50% of using a mirror image

   // shape
   DWORD             m_dwShape;     // shape number. 0 = none
   COLORREF          m_cShapeColor; // color of the shape

   DWORD             m_dwRenderShardTemp; // for temp UI display

   // texture
   CSubTexture       m_Text;        // sub-texture
};
typedef CLeafForTexture *PCLeafForTexture;


class CTextCreatorGrass : public CTextCreatorSocket {
public:
   ESCNEWDELETE;

   CTextCreatorGrass (DWORD dwRenderShard, DWORD dwType);
   virtual void Delete (void);
   virtual BOOL MMLFrom (PCMMLNode2 pNode);
   virtual PCMMLNode2 MMLTo (void);
   virtual BOOL Render (PCImage pImage, PCImage pImage2, PCMaterial pMaterial, PTEXTINFO pTextInfo);
   virtual BOOL Dialog (PCEscWindow pWindow);
   virtual BOOL TextureQuery (PCListFixed plText, PCBTree pTree, GUID *pagThis);
   virtual BOOL SubTextureNoRecurse (PCListFixed plText, GUID *pagThis);

   DWORD             m_dwRenderShard;
   CMaterial         m_Material;  // material to use
   DWORD             m_dwType; // initial type of tile - when constructed
   int               m_iSeed;   // seed for the random
   int               m_iWidth;      // width, in pixels
   int               m_iHeight;     // height, in pixels
   DWORD             m_dwNumBlades; // number of blades
   COLORREF          m_acStem[3];   // color of the grass stems
   fp                m_fCluster;    // how much the values cluster, from 0 to 1
   fp                m_fColorVariation;   // color variation amount, 0 to 1
   fp                m_fAngleOut;   // how much grass angles out, 0 to 1
   fp                m_fAngleVariation;  // how much grass angles varies, 0 to 1
   fp                m_fAngleSkew;  // grass tends to point in one direction, -1 to 1
   fp                m_fShorterEdge;   // shorter towards the edge, 0..1
   fp                m_fWidthBase;  // width of the blade at the base, 0..1
   fp                m_fWidthMid;   // mid width
   fp                m_fWidthTop;   // top width
   fp                m_fGravity;    // gravity strength on grass, 0..1
   fp                m_fDarkEdge;   // how much darker the edges are than the interio, 0..2. 0.5 is default
   fp                m_fTransBase;  // how transparent the base is, 0..1
   fp                m_fDarkenAtBottom;   // how much to darken at the bottom, 0..1

   // tip
   CLeafForTexture   m_Leaf;        // leaf
   //DWORD             m_dwTipShape;  // tip shape, 0 = none
   //COLORREF          m_cTipColor;   // color of tip
   //fp                m_fTipWidth;   // tip width, 0..1
   //fp                m_fTipLength;  // tip height, 0..1
   fp                m_fTipProb;    // tip probability for long, 0..1
   fp                m_fTipProbShort;  // tip probability for short, 0..1. 0=>low prob for short
};
typedef CTextCreatorGrass *PCTextCreatorGrass;

void DrawGrassTip (PCImage pImage, COLORREF cColor, PCPoint pStart, PCPoint pDirection,
                     fp fWidth, fp fLength, DWORD dwNum,fp fZDelta = -ZINFINITE);


/***********************************************************************************
CTextCreatorLeafLitter
*/

#define LEAFLITTERVARIETIES         5     // number of varieties of leaves

class CTextCreatorLeafLitter : public CTextCreatorSocket {
public:
   ESCNEWDELETE;

   CTextCreatorLeafLitter (DWORD dwRenderShard, DWORD dwType);
   virtual void Delete (void);
   virtual BOOL MMLFrom (PCMMLNode2 pNode);
   virtual PCMMLNode2 MMLTo (void);
   virtual BOOL Render (PCImage pImage, PCImage pImage2, PCMaterial pMaterial, PTEXTINFO pTextInfo);
   virtual BOOL Dialog (PCEscWindow pWindow);
   virtual BOOL TextureQuery (PCListFixed plText, PCBTree pTree, GUID *pagThis);
   virtual BOOL SubTextureNoRecurse (PCListFixed plText, GUID *pagThis);

   DWORD             m_dwRenderShard;
   // CMaterial         m_Material;  // material to use
   DWORD             m_dwType; // initial type of tile - when constructed
   fp                m_fPixelLen;  // meters per pixel
   fp                m_fPatternWidth; // pattern width in meters
   fp                m_fPatternHeight;   // pattern height in meters

   CSubTexture       m_Background;       // subtexture for background

   DWORD             m_dwNumX;            // number of leaves across
   DWORD             m_dwNumY;            // number of leaves up/down
   int               m_iSeed;             // random seed

   CLeafForTexture   m_aLeaf[LEAFLITTERVARIETIES];    // leaf
   BOOL              m_afLeafUse[LEAFLITTERVARIETIES];   // check to use the leaf

};
typedef CTextCreatorLeafLitter *PCTextCreatorLeafLitter;



/***********************************************************************************
CTextCreatorBranch - Noise functions
*/

/* CBranchNode - For storing branch information */
class CBranchNode;
typedef CBranchNode *PCBranchNode;
typedef struct {
   CPoint            pLoc;       // offset location from the start of the current branch
   PCBranchNode      pNode;      // new node
} BRANCHINFO, *PBRANCHINFO;

typedef struct {
   CPoint            pX;         // vector pointing in x=1 direction for leaf (normalized)
   CPoint            pY;         // vector pointing in y=1 direction for leaf (from the node). Normalized
   CPoint            pZ;         // normalized Up (z=1) vector
   CMatrix           mLeafToObject; // converts from leaf space to object space, includes translation, rotation, and scale
   fp                fScale;     // amount to scale. 1.0 = default size
   DWORD             dwID;       // leaf ID - index into list of leaves supported
} LEAFINFO, *PLEAFINFO;

class CBranchNode {
public:
   CBranchNode (void);
   ~CBranchNode (void);

   CBranchNode *Clone (void);

   fp CalcThickness (void);
   void ScaleThick (fp fScale);
   void CalcLoc (PCPoint pLoc, PCPoint pUp);
   //void SortByThickness (void);
   void RemoveLeaf (DWORD dwID);
   void LeafMatricies (PCListFixed plCMatrix);
   void FillNoodle (fp fThickScale, PCListFixed plLoc, PCListFixed plUp, PCListFixed plThick,
      PCListFixed plBRANCHNOODLE, DWORD dwDivide, BOOL fRound, DWORD dwTextureWrap, BOOL fCap);

//private:
   CListFixed           m_lBRANCHINFO;    // list of branches
   CListFixed           m_lLEAFINFO;      // list of leaves

   // calculate
   fp                   m_fThickness;     // how thick it is
   fp                   m_fThickDist;     // thickness in meters
   CPoint               m_pLoc;           // location
   CPoint               m_pUp;            // up vector, so noodle has proper up vector
   BOOL                 m_fIsRoot;        // set to TRUE if it's shte start of a root
};


class CTextCreatorBranch : public CTextCreatorSocket {
public:
   CTextCreatorBranch (DWORD dwRenderShard, DWORD dwType);
   ~CTextCreatorBranch (void);

   virtual void Delete (void);
   virtual BOOL MMLFrom (PCMMLNode2 pNode);
   virtual PCMMLNode2 MMLTo (void);
   virtual BOOL Render (PCImage pImage, PCImage pImage2, PCMaterial pMaterial, PTEXTINFO pTextInfo);
   virtual BOOL Dialog (PCEscWindow pWindow);
   virtual BOOL TextureQuery (PCListFixed plText, PCBTree pTree, GUID *pagThis);
   virtual BOOL SubTextureNoRecurse (PCListFixed plText, GUID *pagThis);

   void AutoBranchFill (void);
   PCBranchNode AutoBranchGen (fp fFullHeight);
   PCBranchNode AutoBranchGenIndiv (PCPoint pDir, DWORD dwMaxGen, PAUTOBRANCH pAB);
   void CalcBranches (PCBranchNode pRoot);

   DWORD             m_dwRenderShard;
   CMaterial         m_Material;  // material to use
   DWORD             m_dwType; // initial type of tile - when constructed
   int               m_iSeed;   // seed for the random
   int               m_iWidth;      // width, in pixels
   int               m_iHeight;     // height, in pixels
   // COLORREF          m_acLeaf[3];   // color of the leaves
   fp                m_fAngleVariation;  // how much Branch angles varies, 0 to 1
   fp                m_fThickScale; // how much to scale thickness
   fp                m_fTransBase;  // how transparent the base is, 0..1
   fp                m_fDarkenAtBottom;   // how much to darken at the bottom
   fp                m_fDarkEdge;   // how dark edge of branch is, 0..2. 0.5 is default. 1.0 = no change

   // tip
   CLeafForTexture   m_Leaf;        // leaf
   //DWORD             m_dwTipShape;  // tip shape, 0 = none
   COLORREF          m_cStemColor;   // color of tip
   COLORREF          m_cBranchColor;
   //fp                m_fTipWidth;   // tip width, 0..1
   //fp                m_fTipLength;  // tip height, 0..1
   fp                m_fStemLength; // length of the stem to leaf, 0..1
   fp                m_fLowerDarker;   // if 0.5 no change. 1.0 then lower leaves darker, 0 then upper leaves darker

   AUTOBRANCH        m_AutoBranch;  // autobranch info

   // internal scratch
   CListFixed        m_lBRANCHNOODLE;  // list of branch noodles
};
typedef CTextCreatorBranch *PCTextCreatorBranch;


#endif //  _TEXTURE_H_