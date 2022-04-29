/************************************************************************
CObjectLight.cpp - Draws a Light.

begun 28/5/02 by Mike Rozak
Copyright 2002 Mike Rozak. All rights reserved
*/

#include <windows.h>
#include "escarpment.h"
#include "..\M3D.h"
#include "resource.h"


typedef struct {
   PWSTR       pszName;    // name of the move point
   int         iX, iY; // -1 if for left/top, 1 for right/bottom
} SPLINEMOVEP, *PSPLINEMOVEP;

static SPLINEMOVEP   gaLightMove[] = {
   L"Light base", 0, 0
};

#define LOSURF_STALK             5  // stalk color
#define LOSURF_POST              6  // post color
#define LOSURF_STAND             7  // stand color
#define LOSURF_FORSHADE          10 // material for the shade start at this number, 10..13

/**********************************************************************************
CLightBulb - Code to manage and draw a light bulb.
*/
#define LBS_FLAMECANDLE          10
#define LBS_FLAMETORCH           11
#define LBS_FLAMEFIRE            12
#define LBS_INCANGLOBE           20
#define LBS_INCANSPOT            21
#define LBS_HALOGENSPOT          30
#define LBS_HALOGENBULB          31
#define LBS_HALOGENTUBE          32
#define LBS_FLOURO8              40
#define LBS_FLOURO18             41
#define LBS_FLOURO36             42
#define LBS_FLOUROROUND          43
#define LBS_FLOUROGLOBE          44
#define LBS_SODIUMGLOBE          50
#define LBS_SUNLIGHT             60       // used for diffuser skylights

// dwType passed into InitFromType() is combination of LBT_WATTS(x) | LBT_COLOR_XYZ | LBS_XYZ
#define LBT_WATTS(x)             ((DWORD)(x) << 16)
#define LBT_COLOR_WHITE          0x0000
#define LBT_COLOR_PINK           0x0100
#define LBT_COLOR_FLOURO         0x0200
#define LBT_COLOR_FLOUROINSECT   0x0300
#define LBT_COLOR_BLACKLIGHT     0x0400
#define LBT_COLOR_RED            0x0500
#define LBT_COLOR_GREEN          0x0600
#define LBT_COLOR_BLUE           0x0700
#define LBT_COLOR_YELLOW         0x0800
#define LBT_COLOR_CYAN           0x0900
#define LBT_COLOR_MAGENTA        0x0a00
#define LBT_COLOR_SODIUM         0x0b00    // color from sodium light


// all combined
#define LBT_INCANGLOBE60         (LBT_WATTS(60) | LBS_INCANGLOBE | LBT_COLOR_WHITE)
#define LBT_INCANGLOBE240        (LBT_WATTS(240) | LBS_INCANGLOBE | LBT_COLOR_WHITE)
#define LBT_INCANSPOT100         (LBT_WATTS(100) | LBS_INCANSPOT | LBT_COLOR_WHITE)
#define LBT_FLAMECANDLE          (LBT_WATTS(60) | LBS_FLAMECANDLE | LBT_COLOR_FLOUROINSECT)
#define LBT_FLAMETORCH           (LBT_WATTS(200) | LBS_FLAMETORCH | LBT_COLOR_FLOUROINSECT)
#define LBT_FLAMEFIRE            (LBT_WATTS(600) | LBS_FLAMEFIRE | LBT_COLOR_FLOUROINSECT)
#define LBT_HALOSPOT25           (LBT_WATTS(25) | LBS_HALOGENSPOT | LBT_COLOR_WHITE)
#define LBT_HALOBULB8            (LBT_WATTS(8) | LBS_HALOGENBULB | LBT_COLOR_WHITE)
#define LBT_HALOTUBE150          (LBT_WATTS(150) | LBS_HALOGENTUBE | LBT_COLOR_WHITE)
#define LBT_HALOTUBE250          (LBT_WATTS(250) | LBS_HALOGENTUBE | LBT_COLOR_WHITE)
#define LBT_FLOURO8              (LBT_WATTS(8) | LBS_FLOURO8 | LBT_COLOR_FLOURO)
#define LBT_FLOURO18             (LBT_WATTS(18) | LBS_FLOURO18 | LBT_COLOR_FLOURO)
#define LBT_FLOURO36             (LBT_WATTS(36) | LBS_FLOURO36 | LBT_COLOR_FLOURO)
#define LBT_FLOURO18x2           (LBT_WATTS(36) | LBS_FLOURO18 | LBT_COLOR_FLOURO)
#define LBT_FLOURO36x2           (LBT_WATTS(72) | LBS_FLOURO36 | LBT_COLOR_FLOURO)
#define LBT_FLOURO36x4           (LBT_WATTS(144) | LBS_FLOURO36 | LBT_COLOR_FLOURO)
#define LBT_FLOUROGLOBE          (LBT_WATTS(22) | LBS_FLOUROGLOBE | LBT_COLOR_FLOURO)
#define LBT_FLOUROROUND          (LBT_WATTS(22) | LBS_FLOUROROUND | LBT_COLOR_FLOURO)
#define LBT_SODIUMGLOBE          (LBT_WATTS(500) | LBS_SODIUMGLOBE | LBT_COLOR_SODIUM) // BUGBUG - Not sure of this wattage
#define LBT_SKYLIGHT             (LBT_WATTS((DWORD)CM_SUNWATTS) | LBS_SUNLIGHT | LBT_COLOR_WHITE)

class CLightBulb {
public:
   ESCNEWDELETE;

   CLightBulb (void);
   ~CLightBulb (void);

   PCMMLNode2 MMLTo (void);
   BOOL MMLFrom (PCMMLNode2 pNode);
   CLightBulb *Clone (void);
   BOOL CloneTo (CLightBulb *pTo);

   BOOL InitFromType (DWORD dwType);   // from LBT_XXX

   BOOL ShapeSet (DWORD dwShape);      // sets the lightbulb shape, LBS_XXX
   DWORD ShapeGet (void);
   BOOL WattsSet (fp fWatts);
   fp WattsGet (void);
   BOOL ColorSet (DWORD dwColor);
   DWORD ColorGet (void);
   BOOL NoShadowsSet (BOOL fNoShadows);
   BOOL NoShadowsGet (void);

   BOOL QueryLightInfo (PLIGHTINFO pLight, PCWorldSocket pWorld);
   BOOL Render (POBJECTRENDER pr, PCRenderSurface prs, BOOL fDrawOn);


private:
   BOOL CalcIfDirty (void);

   DWORD       m_dwShape;     // LBS_XXX
   fp          m_fWatts;      // number of watts - resulting lumens based on LBS_XXX
   DWORD       m_dwColor;     // color of the light, LBT_COLOR_XXX
   BOOL        m_fNoShadows;  // if TRUE dont cast shadows

   // derived
   BOOL        m_fDirtyLI;    // set to TRUE if dirty
   BOOL        m_fDirtyRender;   // set to TRUE if dirty
   LIGHTINFO   m_LI;          // light info
   PCNoodle    m_pNoodle;     // for drawing
   PCRevolution m_pRevolution;   // for drawing
   COLORREF    m_cColor;      // color when on
   COLORREF    m_cColorOff;   // color when off
};
typedef CLightBulb * PCLightBulb;


/**********************************************************************************
CLightShade */

class CLightShade {
public:
   ESCNEWDELETE;

   CLightShade (void);
   ~CLightShade (void);

   BOOL Init (DWORD dwType);
   BOOL SizeSet (PCPoint pSize);
   void SizeGet (PCPoint pSize);
   DWORD TypeGet (void);
   BOOL BulbInfoSet (fp fWatts, DWORD dwColor, BOOL fNoShadows);
   BOOL BulbInfoGet (fp *pfWatts, DWORD *pdwColor, DWORD *pdwShape, BOOL *pfNoShadows);   // so can change the bulb wattage and color
   DWORD QueryMaterials (void);     // returns bit-field indicating what materials want

   PCMMLNode2 MMLTo (void);
   BOOL MMLFrom (PCMMLNode2 pNode);
   CLightShade *Clone (void);
   BOOL CloneTo (CLightShade *pTo);
   BOOL QueryLightInfo (PLIGHTINFO pLight, PCWorldSocket pWorld, PCObjectSurface *papShadeMaterial);
   BOOL Render (DWORD dwRenderShard, POBJECTRENDER pr, PCRenderSurface prs, BOOL fDrawOn, PCWorldSocket pWorld, PCObjectSurface *papShadeMaterial);

private:
   BOOL CalcRenderIfNecessary(void);
   BOOL CalcLIGHTINFOIfNecessary(PCWorldSocket pWorld, PCObjectSurface *papShadeMaterial);

   // set by user
   DWORD       m_dwType;      // type of shades, LST_XXX
   fp          m_fBulbWatts;  // lightbulb watts - affected by Init, but can be changed later
   DWORD       m_dwBulbColor; // lightbulb color - affected by Init, but can be changed later
   BOOL        m_fBulbNoShadows;  // if TRUE light casts no shadows
   CPoint      m_pSize;       // size of light shade

   // defined by m_dwType
   DWORD       m_dwBulbShape; // lightbulb shape
   CPoint      m_pBulbLoc;    // location relative to light shade coords
   CPoint      m_pBulbDir;    // direction relative to light shade coors
   CPoint      m_pBulbLeft;   // left side of bulb (relative to shade coords)
   BOOL        m_fBulbDraw;   // set to TRUE if want to draw the bulb

   CPoint      m_pLightVec;   // vector for the main direction of the light (not necessarily
                              // the same as m_pBulbDir. If bulb is OMNIdirectional then will change bulb's direction
                              // if bulb is DIRECTIONAL then will genrally ignore m_pLightVec, m_afLightAmt, m_afLightColor, etc.
   fp          m_afLightAmt[3];  // amount of light (totals to 1.0) going in [0]=lightvec, [1]=-lightvec, [2]=omni
                              // if neither shade is translucent then will cut out omni-light
   WORD        m_awLightColor[3][3];   // discoloration for light going in [0][y]=lightvec, [1][y]=-lightvec, [2][y]=omni
   TEXTUREPOINT m_apLightDirect[2]; // affects light directionality... for [0]=lightvec, [1]=-lightvec
                              // .h = diameter of opening, .v = distance from light

   BOOL        m_afShadeShow[2]; // set to TRUE if want to draw the shade
   DWORD       m_adwShadeProfile[2];  // profile of shade
   DWORD       m_adwShadeRev[2]; // revolution of shade
   CPoint      m_apShadeBottom[2];  // bottom of shade
   CPoint      m_apShadeAroundVec[2];  // rotate around
   CPoint      m_apShadeLeftVec[2]; // left vector
   CPoint      m_apShadeSize[2];    // size of the shade
   DWORD       m_adwShadeMaterial[2];  // special material settings for what want in shade...
                              // 0 = default opaque
                              // 1 = default cloth (like lamp shade)
                              // 2 = default translucent glass
                              // 3 = default transparent glass
   BOOL        m_afShadeAffectsOmni[2];   // if flag TRUE and the shade's material is opaque
                              // then turn off the omnidirectional light
            
   // dirty
   BOOL        m_fDirtyLI;    // set to TRUE if LIGHTINFO is dirty
   BOOL        m_fDirtyRender;   // set to TRUE if render info is dirty
   BOOL        m_fWasShadeTransparent; // set to TRUE if shade was transparent last time got lightinfo or drew,
                              // FALSE if wasn't a transparent one. If this changes between this and last call then
                              // will set m_fDirtyLI

   // generated
   CLightBulb  m_LightBulb;
   LIGHTINFO   m_LI;          // light info - assuming that light info from bulb hasnt changed
                              // coords are in shade coords
   PCRevolution m_apRevolution[2];  // to draw the shades
   CMatrix     m_mLightBulb;   // translates from light bulb space into shade space
};
typedef CLightShade *PCLightShade;



/**********************************************************************************
CLightStalk */
typedef struct {
   CPoint   pLoc;          // location of joint
   DWORD    dwFreedomBits; // freedom of movement, bit1 = any x dir, bit2 = any y dir, bit3 = any z dir,
                           // bit 4 allows for length to be changed
} JOINTINFO, *PJOINTINFO;

class CLightStalk {
public:
   ESCNEWDELETE;

   CLightStalk (void);
   ~CLightStalk (void);

   BOOL Init (DWORD dwType);     // initialize to type LSTALKT_XXX
   BOOL MatrixGet (PCMatrix mShadeToStalk); // fills in matrix from shade to stalk space

   BOOL ControlPointQuery (DWORD dwID, POSCONTROL pInfo);
   BOOL ControlPointSet (DWORD dwID, PCPoint pVal, PCPoint pViewer);
   void ControlPointEnum (PCListFixed plDWORD);
   BOOL ControlPointIsValid (DWORD dwID);

   PCMMLNode2 MMLTo (void);
   BOOL MMLFrom (PCMMLNode2 pNode);
   CLightStalk *Clone (void);
   BOOL CloneTo (CLightStalk *pTo);
   BOOL Render (POBJECTRENDER pr, PCRenderSurface prs);

private:
   DWORD JointNum (void);
   BOOL JointSet (DWORD dwJoint, PCPoint pLoc, DWORD dwFreedomBits);
   BOOL JointGet (DWORD dwJoint, PCPoint pLoc, DWORD *pdwFreedomBits);
   BOOL CalcRenderIfNecessary(void);

   // set by user
   DWORD       m_dwType;      // predefined type
   CListFixed  m_lJOINTINFO;  // list of JOINTINGO, initially set by m_dwType, but may be modified by user
   CPoint      m_pShadeDir;   // shade direction, initally set bt m_dwType
   CPoint      m_pShadeLeft;  // shade left, initally set by m_dwType

   // derived from m_dwType
   BOOL        m_fDrawStalk;  // set to TRUE if should draw the stalk
   DWORD       m_dwSegCurve;  // for noodle
   CPoint      m_pSize;       // for noodle
   CPoint      m_pFront;      // front vector, for noodle
   DWORD       m_dwShape;     // for noodle
   DWORD       m_dwShadeDirFreedom; // bits for freedom of movement for the shade
   DWORD       m_dwShadeLeftFreedom;   // bits for freedom of movement of left vector

   // calculted
   BOOL        m_fDirty;      // set to TRUE if dirty
   CMatrix     m_mShadeToStalk;  // convert from shade space to stalk space
   CNoodle     m_Noodle;      // to draw
};
typedef CLightStalk *PCLightStalk;


/**********************************************************************************
CLightPost - Sticks out of the stand and has one or more stalks associated.
*/

#define CONTROLPERSTALK       10 // number of control points allotted to each stalk

typedef struct {
   PCLightStalk      pStalk;     // light stalk object
   CMatrix           mStalkToPost;  // convert from stalk to post coords
   fp                fOffset;    // offset in Z, where 1.0 == m_fPostLength
} LIGHTSTALK, *PLIGHTSTALK;

class CLightPost {
   friend class CLightStand;
public:
   ESCNEWDELETE;

   CLightPost (void);
   ~CLightPost (void);

   BOOL Init (DWORD dwType);     // initialize to type LPOSTT_XXX

   DWORD StalkNum (void);
   PCLightStalk StalkGet (DWORD dwIndex);
   BOOL StalkMatrix (DWORD dwIndex, PCMatrix pm);

   BOOL ControlPointQuery (DWORD dwID, POSCONTROL pInfo);
   BOOL ControlPointSet (DWORD dwID, PCPoint pVal, PCPoint pViewer);
   void ControlPointEnum (PCListFixed plDWORD);
   BOOL ControlPointIsValid (DWORD dwID);
   DWORD ControlPointMax (void);

   PCMMLNode2 MMLTo (void);
   BOOL MMLFrom (PCMMLNode2 pNode);
   CLightPost *Clone (void);
   BOOL CloneTo (CLightPost *pTo);
   BOOL Render (POBJECTRENDER pr, PCRenderSurface prs);  // draws the stalk only, not posts

private:
   DWORD          m_dwType;      // type, passed in by Init
   CListFixed     m_lLIGHTSTALK; // list of light stalk information, initally set by m_dwType, but saved into registry
   fp             m_fPostLength; // post length - used for placing stalks, may be adjusted by user

   // set by m_dwType
   BOOL           m_fPostShow;  // set to TRUE if should draw Post
   BOOL           m_fCanLength;  // if TRUE then can lengthen the post, when lengthened redraws post with m_fPostScale
                                 // scaled set to m_fPostLength
   CPoint         m_pPostBottom;   // where revolution starts
   CPoint         m_pPostAroundVec;   // revolve around this vector
   CPoint         m_pPostLeftVec;  // draw revolution - left info
   CPoint         m_pPostScale; // scale vector for revolution
   DWORD          m_dwPostRevolution; // revolution type
   DWORD          m_dwPostProfile; // profile type

   // derived
   CRevolution    m_Revolution;     // used for drawing
};
typedef CLightPost *PCLightPost;


/**********************************************************************************
CLightStand - Holds one or more posts
*/


typedef struct {
   PCLightPost       pPost;      // light post object
   CPoint            pBottom;    // botom location of post, but coords are in original
                                 // coords (before X location changed by resizing)
   CPoint            pUpVec;     // direction of the post
   CPoint            pLeftVec;   // original left vector
   CMatrix           mPostToStand;  // convert from post to stand coords
} LIGHTPOST, *PLIGHTPOST;

class CLightStand {
public:
   ESCNEWDELETE;

   CLightStand (void);
   ~CLightStand (void);

   BOOL Init (DWORD dwType);     // initialize to type LSTANDT_XXX
   DWORD TypeGet (void);

   DWORD PostNum (void);
   PCLightPost PostGet (DWORD dwIndex);
   BOOL PostMatrix (DWORD dwIndex, PCMatrix pm);

   BOOL CanEmbed (DWORD *padwCutoutShape = NULL, PCPoint pCutoutSize = NULL);
   BOOL IsHanging (void);
   void HangingMatrixSet (PCMatrix pm);

   BOOL ControlPointQuery (DWORD dwID, POSCONTROL pInfo);
   BOOL ControlPointSet (DWORD dwID, PCPoint pVal, PCPoint pViewer);
   void ControlPointEnum (PCListFixed plDWORD);
   BOOL ControlPointIsValid (DWORD dwID);

   PCMMLNode2 MMLTo (void);
   BOOL MMLFrom (PCMMLNode2 pNode);
   CLightStand *Clone (void);
   BOOL CloneTo (CLightStand *pTo);
   BOOL Render (POBJECTRENDER pr, PCRenderSurface prs);  // draws the Post only, not posts

private:
   void RecalcLIGHTPOST(void);      // call this if definition of up changes, or object elongated, or one post moved

   DWORD          m_dwType;      // type, passed in by Init
   CListFixed     m_lLIGHTPOST; // list of light Post information, initally set by m_dwType, but saved into registry
   fp             m_fStandLength; // Stand length - used for placing Stands, may be adjusted by user
   CPoint         m_pHangDown;  // normalized vector for absolute down

   // set by m_dwType
   BOOL           m_fStandShow;  // set to TRUE if should draw Stand
   BOOL           m_fCanLength;  // if TRUE then can lengthen the Stand, when lengthened redraws Stand with m_fStandScale
                                 // scaled set to m_fStandLength
   BOOL           m_fCanMove;    // if TRUE, then can move the posts along the length (x) of the stand
   BOOL           m_fPostsHang;  // if TRUE, the posts are always oriented down (absolute, regardless of which way object rotated)
   BOOL           m_fCanEmbed;   // set to TRUE if can embed the object
   DWORD          m_dwPostType;  // post type
   DWORD          m_dwCutoutShape;   // if can embed, this is 0 for none, 1 for rectangular, 2 for ellipse
   CPoint         m_pCutoutSize;  // if embedeed, x and y are the length from end to end of the cutout
   CPoint         m_pStandBottom;   // where revolution starts
   CPoint         m_pStandAroundVec;   // revolve around this vector
   CPoint         m_pStandLeftVec;  // draw revolution - left info
   CPoint         m_pStandScale; // scale vector for revolution
   DWORD          m_dwStandRevolution; // revolution type
   DWORD          m_dwStandProfile; // profile type

   // derived
   CRevolution    m_Revolution;     // used for drawing
};
typedef CLightStand *PCLightStand;


/**********************************************************************************
CLightStand::Constructor and destructor
*/
CLightStand::CLightStand (void)
{
   m_dwType = 0;
   m_lLIGHTPOST.Init (sizeof(LIGHTPOST));
   m_fStandLength = 0;
   m_pHangDown.Zero();
   m_pHangDown.p[2] = 1;
   m_fStandShow = TRUE;
   m_fCanLength = FALSE;
   m_fCanMove = FALSE;
   m_fPostsHang = FALSE;
   m_fCanEmbed = FALSE;
   m_dwCutoutShape = 0;
   m_pCutoutSize.Zero();
   m_pStandBottom.Zero();
   m_pStandAroundVec.Zero();
   m_pStandLeftVec.Zero();
   m_pStandScale.Zero();
   m_dwStandRevolution = 0;
   m_dwStandProfile = 0;
   m_dwPostType = 0;
}

CLightStand::~CLightStand (void)
{
   DWORD i;
   for (i = 0; i < m_lLIGHTPOST.Num(); i++) {
      PLIGHTPOST p = (PLIGHTPOST) m_lLIGHTPOST.Get(i);
      if (p->pPost)
         delete p->pPost;
   }
   m_lLIGHTPOST.Clear();
}


/**********************************************************************************
CLightStand::Init - Initializes the light standad based upon a specific type.

inputs
   DWORD       dwType - One of LSTANDT_XXXX
returns
   BOOL - TRUE if success
*/
BOOL CLightStand::Init (DWORD dwType)
{
   m_dwType = dwType;

   // clear out existing posts
   DWORD i;
   for (i = 0; i < m_lLIGHTPOST.Num(); i++) {
      PLIGHTPOST p = (PLIGHTPOST) m_lLIGHTPOST.Get(i);
      if (p->pPost)
         delete p->pPost;
   }
   m_lLIGHTPOST.Clear();

   // some defaults
   m_fStandShow = TRUE;
   m_fCanLength = FALSE;
   m_fCanMove = FALSE;
   m_fPostsHang = FALSE;
   m_fCanEmbed = FALSE;
   m_dwCutoutShape = 0;
   m_pCutoutSize.Zero();
   m_pStandAroundVec.Zero();
   m_pStandAroundVec.p[2] = -1;  // since usually bowl pointing down
   m_pStandLeftVec.Zero();
   m_pStandLeftVec.p[0] = 1;
   m_pStandScale.Zero();
   m_pStandScale.p[0] = m_pStandScale.p[1] = .2;
   m_pStandScale.p[2] = .05;
   m_pStandBottom.Zero();
   m_pStandBottom.p[2] = m_pStandScale.p[2];
   m_dwStandRevolution = RREV_CIRCLE;
   m_dwStandProfile = RPROF_LINECONE;
   m_dwPostType = 0;

#define EZPOST 6  // max # of EZ posts
   DWORD dwNumEZ;
   CPoint apEZLoc[EZPOST], apEZUp[EZPOST], apEZLeft[EZPOST];
   dwNumEZ = 0;
   for (i = 0; i < EZPOST; i++) {
      apEZLoc[i].Zero();
      apEZUp[i].Zero();
      apEZUp[i].p[2] = 1;
      apEZLeft[i].Zero();
      apEZLeft[i].p[0] = 1;
   }

   // use the same values for post type
   m_dwPostType = m_dwType;
   dwNumEZ = 1;

   switch (m_dwType) {
   case LSTANDT_FIRETORCHHOLDER:      // holds torch at an angle
      m_fCanEmbed = TRUE;
      apEZUp[0].Zero();
      apEZUp[0].p[1] = 1;
      apEZUp[0].p[2] = .5;
      apEZUp[0].Normalize();
      break;

   case LSTANDT_FIRELOG:      // log for the fireplace
      m_fCanLength = TRUE;

      dwNumEZ = 1;

      m_fCanMove = TRUE;

      m_pStandScale.p[0] = m_pStandScale.p[1] = .15;
      m_pStandScale.p[2] = (fp)dwNumEZ * 0.5;
      m_pStandAroundVec.Zero();
      m_pStandAroundVec.p[0] = 1;  // since usually bowl pointing down
      m_pStandLeftVec.Zero();
      m_pStandLeftVec.p[1] = 1;
      m_pStandBottom.Zero();
      m_pStandBottom.p[2] = m_pStandScale.p[1] / 2;
      m_pStandBottom.p[0] = -m_pStandScale.p[2]/2;
      m_dwStandRevolution = RREV_CIRCLE;
      m_dwStandProfile = RPROF_C;

      // place the objects
      for (i = 0; i < dwNumEZ; i++) {
         apEZLoc[i].Zero();
         apEZLoc[i].p[0] = (((fp)i + .5) / (fp)dwNumEZ - .5) * m_pStandScale.p[2];
         apEZLoc[i].p[2] = m_pStandScale.p[1];
         apEZUp[i].Zero();
         apEZUp[i].p[2] = 1;
         apEZLeft[i].Zero();
         apEZLeft[i].p[0] = 1;
      }
      break;

   case LSTANDT_FIRECANDELABRA:      // candelabra
      // just use defaults
      break;

   case LSTANDT_FLOURO8W1:      // 1 8w flouro
   case LSTANDT_FLOURO18W1:      // 1 18w flouro
   case LSTANDT_FLOURO18W2:      // 2 18w flouro
   case LSTANDT_FLOURO36W1:      // 1 36w flouro
   case LSTANDT_FLOURO36W2:      // 2 36w flouro
   case LSTANDT_FLOURO36W4:      // 4 36w flouro in drop-down commerical ceiling
   case LSTANDT_FLOUROROUND:      // round flouro light in hemi-sphere
      m_fCanEmbed = TRUE;
      m_dwStandRevolution = RREV_SQUARE;
      m_dwStandProfile = RPROF_LINEVERT;

      switch (m_dwType) {
      case LSTANDT_FLOURO8W1:      // 1 8w flouro
         m_pStandScale.p[0] = CM_FLOURO8LENGTH * 1.2;
         m_pStandScale.p[1] = CM_FLOURO8DIAMETER * 2;
         break;
      case LSTANDT_FLOURO18W1:      // 1 18w flouro
         m_pStandScale.p[0] = CM_FLOURO18LENGTH * 1.2;
         m_pStandScale.p[1] = CM_FLOURO18DIAMETER * 2;
         break;
      case LSTANDT_FLOURO18W2:      // 2 18w flouro
         m_pStandScale.p[0] = CM_FLOURO18LENGTH * 1.2;
         m_pStandScale.p[1] = CM_FLOURO18DIAMETER * 4;
         break;
      case LSTANDT_FLOURO36W1:      // 1 36w flouro
         m_pStandScale.p[0] = CM_FLOURO36LENGTH * 1.2;
         m_pStandScale.p[1] = CM_FLOURO36DIAMETER * 2;
         break;
      case LSTANDT_FLOURO36W2:      // 2 36w flouro
         m_pStandScale.p[0] = CM_FLOURO36LENGTH * 1.2;
         m_pStandScale.p[1] = CM_FLOURO36DIAMETER * 4;
         break;
      case LSTANDT_FLOURO36W4:      // 4 36w flouro in drop-down commerical ceiling
         m_pStandScale.p[0] = CM_OFFICE4FLOUROLENGTH;
         m_pStandScale.p[1] = CM_OFFICE4FLOUROWIDTH;
         break;
      case LSTANDT_FLOUROROUND:      // round flouro light in hemi-sphere
         m_pStandScale.p[0] = m_pStandScale.p[1] = CM_FLOUROROUNDRINGDIAMETER * 1.2;
         m_dwStandRevolution = RREV_CIRCLE;
         break;
      }

      m_pStandScale.p[2] = .04;
      m_pStandBottom.p[2] = m_pStandScale.p[2];
      apEZLoc[0].p[2] = m_pStandScale.p[2];
      break;

   case LSTANDT_TRACK2:
   case LSTANDT_TRACK3:
   case LSTANDT_TRACK4:
   case LSTANDT_TRACK5:
   case LSTANDT_TRACKGLOBES2:
   case LSTANDT_TRACKGLOBES3:
   case LSTANDT_TRACKGLOBES4:
   case LSTANDT_TRACKGLOBES5:
      m_fCanLength = TRUE;
      m_fCanEmbed = TRUE;

      switch (m_dwType) {
      default:
      case LSTANDT_TRACK2:
      case LSTANDT_TRACKGLOBES2:
         dwNumEZ = 2;
         break;
      case LSTANDT_TRACK3:
      case LSTANDT_TRACKGLOBES3:
         dwNumEZ = 3;
         break;
      case LSTANDT_TRACK4:
      case LSTANDT_TRACKGLOBES4:
         dwNumEZ = 4;
         break;
      case LSTANDT_TRACK5:
      case LSTANDT_TRACKGLOBES5:
         dwNumEZ = 5;
         break;
      }

      switch (m_dwType) {
      case LSTANDT_TRACK2:
      case LSTANDT_TRACK3:
      case LSTANDT_TRACK4:
      case LSTANDT_TRACK5:
         m_fCanMove = TRUE;
         m_pStandScale.p[0] = .05;
         m_pStandScale.p[2] = (fp)dwNumEZ * 0.5;
         break;
      default:
         m_pStandScale.p[0] = .10;
         m_pStandScale.p[2] = (fp)(dwNumEZ+1) * CM_INCANGLOBEDIAMETER*2;
         m_dwPostType = LSTANDT_NONEMOUNTED;
         break;
      }
      m_pStandAroundVec.Zero();
      m_pStandAroundVec.p[0] = 1;  // since usually bowl pointing down
      m_pStandLeftVec.Zero();
      m_pStandLeftVec.p[1] = 1;
      m_pStandScale.p[1] = .02;
      m_pStandBottom.Zero();
      m_pStandBottom.p[2] = m_pStandScale.p[1] / 2;
      m_pStandBottom.p[0] = -m_pStandScale.p[2]/2;
      m_dwStandRevolution = RREV_SQUARE;
      m_dwStandProfile = RPROF_C;

      // place the objects
      for (i = 0; i < dwNumEZ; i++) {
         apEZLoc[i].Zero();
         apEZLoc[i].p[0] = (((fp)i + .5) / (fp)dwNumEZ - .5) * m_pStandScale.p[2];
         apEZLoc[i].p[2] = m_pStandScale.p[1];
         apEZUp[i].Zero();
         apEZUp[i].p[2] = 1;
         apEZLeft[i].Zero();
         apEZLeft[i].p[0] = 1;
      }
      break;

   case LSTANDT_NONEMOUNTED:
      m_fCanEmbed = TRUE;
      m_fStandShow = 0;
      m_pStandScale.Zero();   // clear out
      break;
   case LSTANDT_NONESTANDING:
      m_fStandShow = 0;
      m_pStandScale.Zero();   // clear out
      break;

   case LSTANDT_CEILINGCHANDELIER3:
   case LSTANDT_CEILINGCHANDELIER4:
   case LSTANDT_CEILINGCHANDELIER5:
   case LSTANDT_CEILINGCHANDELIER6:
   case LSTANDT_CEILINGCHANDELIER3UP:
   case LSTANDT_CEILINGCHANDELIER4UP:
   case LSTANDT_CEILINGCHANDELIER5UP:
   case LSTANDT_CEILINGCHANDELIER6UP:
      m_fCanEmbed = TRUE;
      m_fPostsHang = TRUE;
      m_pStandScale.p[0] = m_pStandScale.p[1] = .3;
      m_pStandScale.p[2] = .015;
      m_pStandBottom.Zero();
      m_pStandBottom.p[2] = m_pStandScale.p[2];
      apEZLoc[0].p[2] = m_pStandScale.p[2];
      m_dwStandProfile = RPROF_L;
      break;

   case LSTANDT_WALLSTALKOUT:      // wall mounted, straight out
   case LSTANDT_WALLSTALKCURVEDDOWN:      // wall mounted, curve down
   case LSTANDT_WALLSTALKCURVEUP:      // wall mounted curve up
   case LSTANDT_WALLSTALK2:      // two stalks
   case LSTANDT_WALLSTALK3:      // three stalks
   case LSTANDT_CEILINGSTALK1:
   case LSTANDT_CEILINGSTALK2:
   case LSTANDT_CEILINGSTALK3:
   case LSTANDT_CEILINGFIXED1:
   case LSTANDT_CEILINGFIXED2:
   case LSTANDT_CEILINGFIXED3:
      m_fCanEmbed = TRUE;
      m_pStandScale.p[0] = m_pStandScale.p[1] = .15;
      switch (m_dwType) {
      case LSTANDT_CEILINGSTALK1:
         m_fPostsHang = TRUE;
         break;
      case LSTANDT_CEILINGSTALK2:
      case LSTANDT_CEILINGSTALK3:
         m_pStandScale.Scale (3);   // because of extra bits hanging, make larger
         m_fPostsHang = TRUE;
         break;
      case LSTANDT_CEILINGFIXED2:
      case LSTANDT_CEILINGFIXED3:
         m_pStandScale.Scale (2.5);   // because of extra bits hanging, make larger
         break;
      }
      m_pStandScale.p[2] = .015;
      m_pStandBottom.Zero();
      m_pStandBottom.p[2] = m_pStandScale.p[2];
      apEZLoc[0].p[2] = m_pStandScale.p[2];
      m_dwStandProfile = RPROF_L;

      switch (m_dwType) {
      case LSTANDT_WALLSTALK2:
      case LSTANDT_CEILINGSTALK2:
      case LSTANDT_CEILINGFIXED2:
         dwNumEZ = 2;
         apEZLoc[0].p[0] = -m_pStandScale.p[0]/ 3;
         apEZLoc[1].Copy (&apEZLoc[0]);
         apEZLoc[1].p[0] *= -1;
         break;

      case LSTANDT_WALLSTALK3:
      case LSTANDT_CEILINGSTALK3:
      case LSTANDT_CEILINGFIXED3:
         dwNumEZ = 3;
         DWORD dw;
         for (dw = 0; dw< 3; dw++) {
            apEZLoc[dw].p[2] = apEZLoc[0].p[2];
            apEZLoc[dw].p[0] = sin((fp)dw / 3.0 * 2 * PI) * m_pStandScale.p[0] /3;
            apEZLoc[dw].p[1] = cos((fp)dw / 3.0 * 2 * PI) * m_pStandScale.p[0] /3;
         }
         break;
      }

      break;

   default:
   case LSTANDT_OUTDOORPATHWAY:       // outdoor, pathway light
   case LSTANDT_OUTDOORLAMP2M:       // 2M lamp post
   case LSTANDT_OUTDOORLAMP3M:       // 3M lamp post
   case LSTANDT_OUTDOORTULIP:       // tulip stand
   case LSTANDT_OUTDOORLAMPTRIPPLE:       // hold up three lamps
   case LSTANDT_OUTDOOROVERHANG:       // large overhang outdoor
   case LSTANDT_OUTDOOROVERHANG2:       // overhang, with two lights
   case LSTANDT_OUTDOOROVERHANG3:       // overhand with three lights
      // no stand, just one post
      m_fStandShow = 0;
      break;

   case LSTANDT_FLOORLAMPSIMPLE:       // straight up
   case LSTANDT_FLOORLAMPSIMPLETALL:   // straight up, but tall
   case LSTANDT_FLOORLAMPMULTISTALK:       // multiple stalks for spot lights
   case LSTANDT_FLOORLAMPTULIP:       // bend on top
   case LSTANDT_FLOORLAMPARMUP:       // arm, light facing up
   case LSTANDT_FLOORLAMPARMANY:       // arm, light any direction
      m_pStandScale.p[0] = m_pStandScale.p[1] = .35;
      break;

   case LSTANDT_DESKLAMPARM:       // desk lamp with simple arm
   case LSTANDT_DESKLAMPARMSMALL:       // small arm
      m_pStandScale.p[0] = m_pStandScale.p[1] = .10;
      m_dwStandRevolution = RREV_SQUARE;
      m_dwStandProfile = RPROF_C;
      break;
   case LSTANDT_DESKLAMPFLEXI:       // flexible
   case LSTANDT_DESKLAMPSIMPLE:       // simple post up
   case LSTANDT_DESKLAMPCURVEDOWN:       // tulip bend, ends up curcing down
   case LSTANDT_DESKLAMPCURVEFRONT:       // half bend, ends up going only half of curve
      m_pStandScale.p[0] = m_pStandScale.p[1] = .20;
      break;
   case LSTANDT_DESKLAMPCERAMIC1:
   case LSTANDT_DESKLAMPCERAMIC2:
   case LSTANDT_DESKLAMPCERAMIC3:
   case LSTANDT_DESKLAMPLAVA:       // lava lamp base
      m_pStandScale.p[0] = m_pStandScale.p[1] = .12;
      break;
   case LSTANDT_TABLELAMPSIMPLE1:       // just vertical out of round stand
      m_pStandScale.p[0] = m_pStandScale.p[1] = .30;
      break;
   case LSTANDT_TABLELAMPSIMPLE2:       // square vertical out of sqaure stand
      m_pStandScale.p[0] = m_pStandScale.p[1] = .30;
      m_dwStandRevolution = RREV_SQUARE;
      break;
   case LSTANDT_TABLELAMPCERAMIC1:       // shape 1
   case LSTANDT_TABLELAMPCERAMIC2:
   case LSTANDT_TABLELAMPCERAMIC3:
   case LSTANDT_TABLELAMPCERAMIC4:
   case LSTANDT_TABLELAMPCERAMIC5:
   case LSTANDT_TABLELAMPCERAMIC6:
      m_pStandScale.p[0] = m_pStandScale.p[1] = .20;
      break;

   case LSTANDT_TABLELAMPSQUAREBASE:
      m_pStandScale.p[0] = m_pStandScale.p[1] = .20;
      m_dwStandRevolution = RREV_SQUARE;
      m_dwStandProfile = RPROF_C;
      m_pStandScale.p[2] = .30;
      m_pStandBottom.p[2] = m_pStandScale.p[2];
      apEZLoc[0].p[2] = m_pStandScale.p[2];
      break;

   case LSTANDT_TABLELAMPCYLINDERBASE:
      m_pStandScale.p[0] = m_pStandScale.p[1] = .20;
      m_dwStandRevolution = RREV_CIRCLE;
      m_dwStandProfile = RPROF_C;
      m_pStandScale.p[2] = .30;
      m_pStandBottom.p[2] = m_pStandScale.p[2];
      apEZLoc[0].p[2] = m_pStandScale.p[2];
      break;

   case LSTANDT_TABLELAMPCURVEDBASE:
      m_pStandScale.p[0] = m_pStandScale.p[1] = .20;
      m_dwStandRevolution = RREV_CIRCLE;
      m_dwStandProfile = PRROF_CIRCLEHEMI;
      m_pStandScale.p[2] = .30;
      m_pStandBottom.p[2] = m_pStandScale.p[2];
      apEZLoc[0].p[2] = m_pStandScale.p[2];
      break;

   case LSTANDT_TABLELAMPHEXBASE:
      m_pStandScale.p[0] = m_pStandScale.p[1] = .20;
      m_dwStandRevolution = RREV_HEXAGON;
      m_dwStandProfile = RPROF_C;
      m_pStandScale.p[2] = .30;
      m_pStandBottom.p[2] = m_pStandScale.p[2];
      apEZLoc[0].p[2] = m_pStandScale.p[2];
      break;

   case LSTANDT_TABLELAMPHORNBASE:
      m_pStandScale.p[0] = m_pStandScale.p[1] = .20;
      m_dwStandRevolution = RREV_CIRCLE;
      m_dwStandProfile = RPROF_HORN;
      m_pStandScale.p[2] = .30;
      m_pStandBottom.p[2] = m_pStandScale.p[2];
      apEZLoc[0].p[2] = m_pStandScale.p[2];
      break;

   }


   // create
   for (i = 0; i < dwNumEZ; i++) {
      LIGHTPOST lp;
      memset (&lp, 0, sizeof(lp));

      lp.pBottom.Copy (&apEZLoc[i]);
      lp.pUpVec.Copy (&apEZUp[i]);
      lp.pLeftVec.Copy (&apEZLeft[i]);
      
      lp.pPost = new CLightPost;
      if (!lp.pPost)
         continue;
      lp.pPost->Init (m_dwPostType);

      m_lLIGHTPOST.Add (&lp);
   }

   // finally
   m_fStandLength = m_pStandScale.p[2];   // BUGFFIX - Was m_fCanLength ? 0 : 2];  // BUGFIX - 0 and 2 were flipped
   m_pStandAroundVec.Normalize();
   m_Revolution.BackfaceCullSet (TRUE);
   m_Revolution.DirectionSet (&m_pStandBottom, &m_pStandAroundVec, &m_pStandLeftVec);
   m_Revolution.ProfileSet (m_dwStandProfile);
   m_Revolution.RevolutionSet (m_dwStandRevolution);

   CPoint pScale, pBottom;
   pScale.Copy (&m_pStandScale);
   pBottom.Copy (&m_pStandBottom);
   if (m_fCanLength) {
      pScale.p[2] = m_fStandLength;
      pBottom.p[0] = -m_fStandLength/2;
   }
   m_Revolution.ScaleSet(&pScale);
   m_Revolution.DirectionSet (&pBottom, &m_pStandAroundVec, &m_pStandLeftVec);

   // recalculate where all the posts go
   RecalcLIGHTPOST();

   return TRUE;
}

/**********************************************************************************
CLightStand::TypeGet - Returns the value passed into init
*/
DWORD CLightStand::TypeGet (void)
{
   return m_dwType;
}

/**********************************************************************************
CLightStand::HangingMatrixSet - This should be called by the owner whenever the
object is moved (assuming that IsHanging() returns TRUE). The owner provides
a matrix that translates from post space into world space. This allows
the light to know what is absolute down.

inputs
   PCMatrix       pm - post space to world space
returns
   none
*/
void CLightStand::HangingMatrixSet (PCMatrix pm)
{
   CMatrix  m;
   m.Copy (pm);
   m.p[0][3] = m.p[1][3] = m.p[2][3] = m.p[3][0] = m.p[3][1] = m.p[3][2] = 0; // no translate
   m.Transpose();

   // convert the point
   m_pHangDown.Zero();
   m_pHangDown.p[2] = -1;
   m_pHangDown.p[3] = 1;
   m_pHangDown.MultiplyLeft (&m);

   if (m_fPostsHang)
      RecalcLIGHTPOST();
}

/**********************************************************************************
CLightStand::IsHanging - Returns TRUE if it's a hanging light - which means
the stands are based off abosolute Z.
*/
BOOL CLightStand::IsHanging (void)
{
   return m_fPostsHang;
}

/**********************************************************************************
CLightStand::CanEmbed - Returns TRUE if the light is designed for being embedded
and FALSE if it's designed for standing on the floor.

inputs
   DWORD    *padwCutoutShape - Filled in with the cutout shape. Right now, 0 for
               no ctuout, 1 for rectangular, 2 for elliptical. Can be NULL
   PCPoint  pCutoutSize - Filled with the cutout size - diameter in x and y.
               Can be null
returns
   BOOL - TRUE if can be embedded
*/
BOOL CLightStand::CanEmbed (DWORD *padwCutoutShape, PCPoint pCutoutSize)
{
   if (padwCutoutShape)
      *padwCutoutShape = m_dwCutoutShape;
   if (pCutoutSize)
      pCutoutSize->Copy (&m_pCutoutSize);

   return m_fCanEmbed;
}

/**********************************************************************************
CLightStand::PostNum - Returns the number of posts.
*/
DWORD CLightStand::PostNum (void)
{
   return m_lLIGHTPOST.Num();
}

/**********************************************************************************
CLightStand::PostGet - Returns a pointer to the post. DO NOT delete.

inputs
   DWORD    dwIndex - 0 to PostNum()-1
returns
   PCLightPost - post object
*/
PCLightPost CLightStand::PostGet (DWORD dwIndex)
{
   PLIGHTPOST p = (PLIGHTPOST) m_lLIGHTPOST.Get(dwIndex);
   if (!p)
      return NULL;
   return p->pPost;
}

/**********************************************************************************
CLightStand::PostMatrix - Fills in matrix for going from post coords to stand coords.

inputs
   DWORD    dwIndex - 0 to PostNum()-1
   PCMatrix pm - Filled with matrix
returns
   PCLightPost - post object
*/
BOOL CLightStand::PostMatrix (DWORD dwIndex, PCMatrix pm)
{
   PLIGHTPOST p = (PLIGHTPOST) m_lLIGHTPOST.Get(dwIndex);
   if (!p)
      return FALSE;
   pm->Copy (&p->mPostToStand);
   return TRUE;

}

/**********************************************************************************
CLightStand::ControlPointQuery - Standard call
*/
BOOL CLightStand::ControlPointQuery (DWORD dwID, POSCONTROL pInfo)
{
   DWORD dwOrigID;
   dwOrigID = dwID;

   // stand size
   if (m_fCanLength) {
      if (dwID == 0) {
         memset (pInfo, 0 ,sizeof(*pInfo));
         pInfo->cColor = RGB(0xff,0xff,0x80);
         pInfo->dwID = dwOrigID;
         pInfo->dwStyle = CPSTYLE_CUBE;
         pInfo->fSize = min(max(m_pStandScale.p[0], m_pStandScale.p[1]),.1) * 2;
         pInfo->pLocation.Copy (&m_pStandBottom);
         pInfo->pLocation.p[0] = m_fStandLength / 2;
         wcscpy (pInfo->szName, L"Stand size");
         return TRUE;
      }
      else
         dwID--;
   }

   // location control points
   if (m_fCanMove) {
      if (dwID < m_lLIGHTPOST.Num()) {
         PLIGHTPOST p = (PLIGHTPOST) m_lLIGHTPOST.Get(dwID);

         memset (pInfo, 0 ,sizeof(*pInfo));
         pInfo->cColor = RGB(0xff,0xff,0);
         pInfo->dwID = dwOrigID;
         pInfo->dwStyle = CPSTYLE_CUBE;
         pInfo->fSize = min(.1,max(p->pPost->m_pPostScale.p[0], p->pPost->m_pPostScale.p[1])) * 2;
         pInfo->pLocation.Copy (&p->pBottom);
         if (m_fCanLength && m_fStandLength && m_pStandScale.p[2])
            pInfo->pLocation.p[0] = p->pBottom.p[0] / m_pStandScale.p[2] * m_fStandLength;
         wcscpy (pInfo->szName, L"Move post");
         return TRUE;
      }
      else
         dwID -= m_lLIGHTPOST.Num();
   }

   DWORD i;
   for (i = 0; i < m_lLIGHTPOST.Num(); i++) {
      PLIGHTPOST p = (PLIGHTPOST) m_lLIGHTPOST.Get(i);
      if (dwID >= p->pPost->ControlPointMax()) {
         dwID -= p->pPost->ControlPointMax();
         continue;
      }

      // else, have post it's from
      if (!p->pPost->ControlPointQuery(dwID, pInfo))
         return FALSE;
      pInfo->dwID = dwOrigID;

      // rotate into post space
//      pInfo->pDirection.p[3] = 1;
      pInfo->pLocation.p[3] = 1;
      // ignoreing v1 and v2
      CMatrix mInvTrans;
      p->mPostToStand.Invert (&mInvTrans);
      mInvTrans.Transpose();
//      pInfo->pDirection.MultiplyLeft (&mInvTrans);
      pInfo->pLocation.MultiplyLeft (&p->mPostToStand);
   
      return TRUE;
   }

   return FALSE;
}

/**********************************************************************************
CLightStand::ControlPointSet - Standard call
*/
BOOL CLightStand::ControlPointSet (DWORD dwID, PCPoint pVal, PCPoint pViewer)
{
   // length
   if (m_fCanLength) {
      if (dwID == 0) {
         fp f;
         f = pVal->p[0] * 2;
         f = max(CLOSE, f);

         m_fStandLength = f;

         // new size
         CPoint pScale, pBottom;
         pScale.Copy (&m_pStandScale);
         pScale.p[2] = m_fStandLength;
         pBottom.Copy (&m_pStandBottom);
         pBottom.p[0] = -m_fStandLength/2;
         m_Revolution.ScaleSet(&pScale);
         m_Revolution.DirectionSet (&pBottom, &m_pStandAroundVec, &m_pStandLeftVec);

         RecalcLIGHTPOST();
         return TRUE;
      }
      else
         dwID--;
   }

   // location control points
   if (m_fCanMove) {
      if (dwID < m_lLIGHTPOST.Num()) {
         PLIGHTPOST p = (PLIGHTPOST) m_lLIGHTPOST.Get(dwID);

         fp f;
         f = pVal->p[0];
         if (m_fCanLength && m_fStandLength && m_pStandScale.p[2])
            f = f / m_fStandLength * m_pStandScale.p[2];

         // max move
         fp fMax;
         fMax = max(max(m_pStandScale.p[0], m_pStandScale.p[1]), m_pStandScale.p[2]);
         f = max(f, -fMax/2);
         f = min(f, fMax/2);

         p->pBottom.p[0] = f;

         RecalcLIGHTPOST();
         return TRUE;
      }
      else
         dwID -= m_lLIGHTPOST.Num();
   }

   DWORD i;
   for (i = 0; i < m_lLIGHTPOST.Num(); i++) {
      PLIGHTPOST p = (PLIGHTPOST) m_lLIGHTPOST.Get(i);
      if (dwID >= p->pPost->ControlPointMax()) {
         dwID -= p->pPost->ControlPointMax();
         continue;
      }

      if (!p->pPost->ControlPointIsValid (dwID))
         return FALSE;

      // convert the params to stalk space
      CPoint pNewVal, pNewViewer;
      CMatrix mInv;
      p->mPostToStand.Invert4 (&mInv);

      pNewVal.Copy (pVal);
      pNewViewer.Copy (pViewer ? pViewer : pVal);
      pNewVal.p[3] = pNewViewer.p[3] = 1;
      pNewVal.MultiplyLeft (&mInv);
      pNewViewer.MultiplyLeft (&mInv);

      if (!p->pPost->ControlPointSet(dwID, &pNewVal, pViewer ? &pNewViewer : NULL))
         return FALSE;
      return TRUE;
   }

   return FALSE;
}

/**********************************************************************************
CLightStand::ControlPointEnum - Standard call
*/
void CLightStand::ControlPointEnum (PCListFixed plDWORD)
{
   DWORD dwCur;
   DWORD i, j, dwLast;
   dwCur = 0;

   // Stand size
   if (m_fCanLength) {
      i = dwCur;
      plDWORD->Add (&i);
      dwCur ++;;
   }

   // location control points
   if (m_fCanMove) {
      for (i = 0; i < m_lLIGHTPOST.Num(); i++) {
          j = i + dwCur;
          plDWORD->Add (&j);
      }
      dwCur += m_lLIGHTPOST.Num();
   }

   for (i = 0; i < m_lLIGHTPOST.Num(); i++) {
      PLIGHTPOST p = (PLIGHTPOST) m_lLIGHTPOST.Get(i);
      dwLast = plDWORD->Num();
      p->pPost->ControlPointEnum(plDWORD);

      for (j = dwLast; j < plDWORD->Num(); j++) {
         DWORD *pdw = (DWORD*) plDWORD->Get(j);
         (*pdw) += dwCur;
      }

      dwCur += p->pPost->ControlPointMax();
   }

}

/**********************************************************************************
CLightStand::ControlIsValid - Returns true if the control point is a valid one
*/
BOOL CLightStand::ControlPointIsValid (DWORD dwID)
{
   // Stand size
   if (m_fCanLength) {
      if (dwID < 1)
         return TRUE;
      else
         dwID--;
   }

   // location control points
   if (m_fCanMove) {
      if (dwID < m_lLIGHTPOST.Num())
         return TRUE;
      else
         dwID -= m_lLIGHTPOST.Num();
   }

   DWORD i;
   for (i = 0; i < m_lLIGHTPOST.Num(); i++) {
      PLIGHTPOST p = (PLIGHTPOST) m_lLIGHTPOST.Get(i);
      if (dwID >= p->pPost->ControlPointMax()) {
         dwID -= p->pPost->ControlPointMax();
         continue;
      }

      return p->pPost->ControlPointIsValid (dwID);
   }

   return FALSE;
}

static PWSTR gpszLightPost = L"LightPost";
static PWSTR gpszType = L"Type";
static PWSTR gpszLightStand = L"LightStand";
static PWSTR gpszStandLength = L"StandLength";
static PWSTR gpszPostToStandMatrix = L"PostToStandMatrix";
static PWSTR gpszPostBottom = L"PostBottom";
static PWSTR gpszPostLeftVec = L"PostLeftVec";
static PWSTR gpszPostUpVec = L"PostUpVec";
static PWSTR gpszHangDown = L"HangDown";

/**********************************************************************************
CLightStand::MMLTo - Standard call
*/
PCMMLNode2 CLightStand::MMLTo (void)
{
   PCMMLNode2 pNode = new CMMLNode2;
   if (!pNode)
      return NULL;
   pNode->NameSet (gpszLightStand);

   // write some info
   MMLValueSet (pNode, gpszType, (int)m_dwType);
   MMLValueSet (pNode, gpszStandLength, m_fStandLength);
   MMLValueSet (pNode, gpszHangDown, &m_pHangDown);

   DWORD i;
   for (i = 0; i < m_lLIGHTPOST.Num(); i++) {
      PLIGHTPOST p = (PLIGHTPOST) m_lLIGHTPOST.Get(i);
      PCMMLNode2 pSub;
      pSub = p->pPost->MMLTo ();
      if (!pSub)
         continue;
      pSub->NameSet (gpszLightPost);

      // stick some additional params in there
      MMLValueSet (pSub, gpszPostToStandMatrix, &p->mPostToStand);
      MMLValueSet (pSub, gpszPostBottom, &p->pBottom);
      MMLValueSet (pSub, gpszPostLeftVec, &p->pLeftVec);
      MMLValueSet (pSub, gpszPostUpVec, &p->pUpVec);

      pNode->ContentAdd (pSub);
   }

   // done
   return pNode;
}

/**********************************************************************************
CLightStand::MMLFrom - Standard call
*/
BOOL CLightStand::MMLFrom (PCMMLNode2 pNode)
{
   m_dwType = (DWORD) MMLValueGetInt (pNode, gpszType, 0);
   if (!Init(m_dwType))
      return FALSE;

   m_fStandLength = MMLValueGetDouble (pNode, gpszStandLength, m_fStandLength);
   MMLValueGetPoint (pNode, gpszHangDown, &m_pHangDown);

   // new size
   CPoint pScale, pBottom;
   pScale.Copy (&m_pStandScale);
   pBottom.Copy (&m_pStandBottom);
   if (m_fCanLength) {
      pScale.p[2] = m_fStandLength;
      pBottom.p[0] = -m_fStandLength/2;
   }
   m_Revolution.ScaleSet(&pScale);
   m_Revolution.DirectionSet (&pBottom, &m_pStandAroundVec, &m_pStandLeftVec);

   // clear out posts and replace
   DWORD i;
   for (i = 0; i < m_lLIGHTPOST.Num(); i++) {
      PLIGHTPOST p = (PLIGHTPOST) m_lLIGHTPOST.Get(i);
      if (p->pPost)
         delete p->pPost;
   }
   m_lLIGHTPOST.Clear();

   PWSTR psz;
   PCMMLNode2 pSub;
   for (i = 0; i < pNode->ContentNum(); i++) {
      pSub = NULL;
      pNode->ContentEnum (i, &psz, &pSub);
      if (!pSub)
         continue;
      psz = pSub->NameGet ();
      if (!psz)
         continue;

      if (!_wcsicmp(psz, gpszLightPost)) {
         LIGHTPOST ls;
         memset (&ls, 0, sizeof(ls));

         // get some additional params
         // stick some additional params in there
         MMLValueGetMatrix (pSub, gpszPostToStandMatrix, &ls.mPostToStand);
         MMLValueGetPoint (pSub, gpszPostBottom, &ls.pBottom);
         MMLValueGetPoint (pSub, gpszPostLeftVec, &ls.pLeftVec);
         MMLValueGetPoint (pSub, gpszPostUpVec, &ls.pUpVec);

         // Stand
         ls.pPost = new CLightPost;
         if (!ls.pPost)
            continue;
         ls.pPost->MMLFrom (pSub);

         // add it
         m_lLIGHTPOST.Add (&ls);
      }
   }

   // recalc light posts in case repositioned
   RecalcLIGHTPOST();

   // done
   return TRUE;
}

/**********************************************************************************
CLightStand::Clone - Standard call
*/
CLightStand *CLightStand::Clone (void)
{
   PCLightStand pNew = new CLightStand;
   if (!pNew)
      return NULL;
   if (!CloneTo (pNew)) {
      delete pNew;
      return NULL;
   }
   return pNew;
}

/**********************************************************************************
CLightStand::CloneTo - Standard call
*/
BOOL CLightStand::CloneTo (CLightStand *pTo)
{
   // clear out posts and replace
   DWORD i;
   for (i = 0; i < pTo->m_lLIGHTPOST.Num(); i++) {
      PLIGHTPOST p = (PLIGHTPOST) pTo->m_lLIGHTPOST.Get(i);
      if (p->pPost)
         delete p->pPost;
   }
   pTo->m_lLIGHTPOST.Clear();

   pTo->m_dwType = m_dwType;
   pTo->m_lLIGHTPOST.Init (sizeof(LIGHTPOST), m_lLIGHTPOST.Get(0), m_lLIGHTPOST.Num());
   for (i = 0; i < pTo->m_lLIGHTPOST.Num(); i++) {
      PLIGHTPOST p = (PLIGHTPOST) pTo->m_lLIGHTPOST.Get(i);
      if (p->pPost)
         p->pPost = p->pPost->Clone();
   }
   pTo->m_fStandLength = m_fStandLength;
   pTo->m_pHangDown.Copy (&m_pHangDown);
   pTo->m_fStandShow = m_fStandShow;
   pTo->m_fCanLength = m_fCanLength;
   pTo->m_pStandBottom.Copy (&m_pStandBottom);
   pTo->m_pStandAroundVec.Copy (&m_pStandAroundVec);
   pTo->m_pStandLeftVec.Copy (&m_pStandLeftVec);
   pTo->m_pStandScale.Copy (&m_pStandScale);
   pTo->m_dwStandRevolution = m_dwStandRevolution;
   pTo->m_dwStandProfile = m_dwStandProfile;
   m_Revolution.CloneTo (&pTo->m_Revolution);
   pTo->m_fCanMove = m_fCanMove;
   pTo->m_fPostsHang = m_fPostsHang;
   pTo->m_fCanEmbed = m_fCanEmbed;
   pTo->m_dwPostType = m_dwPostType;
   pTo->m_dwCutoutShape = m_dwCutoutShape;
   pTo->m_pCutoutSize.Copy (&m_pCutoutSize);

   return TRUE;
}

/**********************************************************************************
CLightStand::Render - Standard call
*/
BOOL CLightStand::Render (POBJECTRENDER pr, PCRenderSurface prs)
{
   if (m_fStandShow)
      m_Revolution.Render(pr, prs);
   return TRUE;
}

/**********************************************************************************
CLightStand::Go through all the light poste recalculating the postiion matrix.
call this if definition of up changes, or object elongated, or one post moved
*/
void CLightStand::RecalcLIGHTPOST(void)
{
   DWORD i, j;
   for (i = 0; i < m_lLIGHTPOST.Num(); i++) {
      PLIGHTPOST p = (PLIGHTPOST) m_lLIGHTPOST.Get(i);
      
      CPoint pA, pB, pC;
      pC.Copy (&p->pUpVec);
      if (m_fPostsHang)
         pC.Copy (&m_pHangDown);
      pC.Normalize();

      pA.Copy (&p->pLeftVec);
      pA.Normalize();
      if (pA.DotProd(&pC) >= 1 - CLOSE) {
         // too close
         for (j = 0; j < 3; j++) {
            pA.Zero();
            pA.p[j] = 1;
            if (pA.DotProd (&pC) < 1 - CLOSE)
               break;
         }
         // when get here pA has perp vector
      }

      pB.CrossProd (&pC, &pA);
      pB.Normalize();
      pA.CrossProd (&pB, &pC);
      pA.Normalize();

      // make rotation from this
      p->mPostToStand.RotationFromVectors (&pA, &pB, &pC);

      // offset
      CPoint pOffset;
      pOffset.Copy (&p->pBottom);
      // consider adjusting offset if lengthen/shorten the stand
      if (m_fCanLength && m_fStandLength && m_pStandScale.p[2])
         pOffset.p[0] = pOffset.p[0] / m_pStandScale.p[2] * m_fStandLength;
      CMatrix mTrans;
      mTrans.Translation (pOffset.p[0], pOffset.p[1], pOffset.p[2]);
      p->mPostToStand.MultiplyRight (&mTrans);
   }

   // done
}


/**********************************************************************************
CLightPost::Contructor and destructor
*/
CLightPost::CLightPost (void)
{
   m_dwType = 0;
   m_fPostLength = 0;
   m_lLIGHTSTALK.Init (sizeof(LIGHTSTALK));

   m_fPostShow = TRUE;
   m_fCanLength = FALSE;
   m_pPostBottom.Zero();
   m_pPostAroundVec.Zero();
   m_pPostLeftVec.Zero();
   m_pPostScale.Zero();
   m_dwPostRevolution = 0;
   m_dwPostProfile = 0;
}

CLightPost::~CLightPost (void)
{
   DWORD i;
   for (i = 0; i < m_lLIGHTSTALK.Num(); i++) {
      PLIGHTSTALK p = (PLIGHTSTALK) m_lLIGHTSTALK.Get(i);
      if (p->pStalk)
         delete p->pStalk;
   }
   m_lLIGHTSTALK.Clear();
}


/**********************************************************************************
CLightPost::Init - Based on dwType, which is one of LPOSTT_XXXX, this initializes
the stalk. Along the way it also creates one or more posts.

inputs
   DWORD       dwType - Type
returns
   BOOL - TRUE if success
*/
BOOL CLightPost::Init (DWORD dwType)
{
   m_dwType = dwType;
   
   // clear out old stalk info
   DWORD i;
   for (i = 0; i < m_lLIGHTSTALK.Num(); i++) {
      PLIGHTSTALK p = (PLIGHTSTALK) m_lLIGHTSTALK.Get(i);
      if (p->pStalk)
         delete p->pStalk;
   }
   m_lLIGHTSTALK.Clear();

   // set some defaults
   m_fPostShow = TRUE;
   m_fCanLength = TRUE;
   m_pPostBottom.Zero();
   m_pPostAroundVec.Zero();
   m_pPostAroundVec.p[2] = 1;
   m_pPostLeftVec.Zero();
   m_pPostLeftVec.p[0] = 1;
   m_pPostScale.Zero();
   m_pPostScale.p[0] = m_pPostScale.p[1] = .1;
   m_pPostScale.p[2] = 1;
   m_dwPostRevolution = RREV_CIRCLE;
   m_dwPostProfile = RPROF_LINEVERT;
   m_fPostLength = 1;

   // easy stalk....
#define EZSTALK      6  // max stalks from EZ stalk
   DWORD    dwNumEZ; // number of auto stalks
   DWORD    dwEZStalkType; // passed into Init
   DWORD    dwEZStalkType2;   // for 2nd, 3rd, etc. stalk
   fp       afEZLoc[EZSTALK]; // location from 0..1, which is position from 0,0,0 to 0,0,fPostLen
   fp       afEZUp[EZSTALK]; // number of radians for up: 0 = perp to Z, PI/2 = Z, -PI/2 =-Z
   fp       afEZLeft[EZSTALK];   // left direction - 0 = X, PI/2 = y, etc.
   dwNumEZ = 1;
   dwEZStalkType = m_dwType;  // pass the same paramters on
   dwEZStalkType2 = m_dwType; // for 2nd, 3rd, etc. posts
   for (i = 0; i < EZSTALK; i++) {
      afEZLoc[i] = 1;
      afEZUp[i] = PI/2;
      afEZLeft[i] = 0;
   }

   switch (m_dwType) {
   case LSTANDT_TRACK2:
   case LSTANDT_TRACK3:
   case LSTANDT_TRACK4:
   case LSTANDT_TRACK5:
      m_fPostShow = TRUE;
      m_fCanLength = FALSE;
      m_pPostScale.p[0] = m_pPostScale.p[1] = .015;
      m_fPostLength = m_pPostScale.p[2] = .05;
      break;

   case LSTANDT_WALLSTALKOUT:      // wall mounted, straight out
   case LSTANDT_WALLSTALKCURVEDDOWN:      // wall mounted, curve down
   case LSTANDT_WALLSTALKCURVEUP:      // wall mounted curve up
   case LSTANDT_WALLSTALK2:      // two stalks
   case LSTANDT_WALLSTALK3:      // three stalks
      m_fPostShow = TRUE;
      m_fCanLength = TRUE;
      m_pPostScale.p[0] = m_pPostScale.p[1] = .015;
      m_fPostLength = m_pPostScale.p[2] = .1;
      break;

   case LSTANDT_CEILINGCHANDELIER3:
   case LSTANDT_CEILINGCHANDELIER4:
   case LSTANDT_CEILINGCHANDELIER5:
   case LSTANDT_CEILINGCHANDELIER6:
   case LSTANDT_CEILINGCHANDELIER3UP:
   case LSTANDT_CEILINGCHANDELIER4UP:
   case LSTANDT_CEILINGCHANDELIER5UP:
   case LSTANDT_CEILINGCHANDELIER6UP:
      m_fPostShow = TRUE;
      m_fCanLength = TRUE;
      m_pPostScale.p[0] = m_pPostScale.p[1] = .015;
      m_fPostLength = m_pPostScale.p[2] = 1;

      DWORD dwNum;
      switch (m_dwType) {
      default:
      case LSTANDT_CEILINGCHANDELIER3:
      case LSTANDT_CEILINGCHANDELIER3UP:
         dwNum = 3;
         break;
      case LSTANDT_CEILINGCHANDELIER4:
      case LSTANDT_CEILINGCHANDELIER4UP:
         dwNum = 4;
         break;
      case LSTANDT_CEILINGCHANDELIER5:
      case LSTANDT_CEILINGCHANDELIER5UP:
         dwNum = 5;
         break;
      case LSTANDT_CEILINGCHANDELIER6:
      case LSTANDT_CEILINGCHANDELIER6UP:
         dwNum = 6;
         break;
      }
      dwNumEZ = dwNum;
      for (i = 0; i < dwNum; i++) {
         afEZLoc[i] = 1;
         afEZUp[i] = 0;
         afEZLeft[i] = (fp)i / (fp)dwNum * 2.0 * PI;;
      }
      break;

   case LSTANDT_CEILINGSTALK1:
   case LSTANDT_CEILINGSTALK2:
   case LSTANDT_CEILINGSTALK3:
      m_fPostShow = TRUE;
      m_fCanLength = TRUE;
      m_pPostScale.p[0] = m_pPostScale.p[1] = .015;
      m_fPostLength = m_pPostScale.p[2] = .5;
      break;

   case LSTANDT_TABLELAMPSIMPLE1:       // just vertical out of round stand
      m_pPostScale.p[0] = m_pPostScale.p[1] = .035;
      m_pPostScale.p[2] = m_fPostLength = .40;
      break;

   case LSTANDT_TABLELAMPSIMPLE2:       // square vertical out of sqaure stand
      m_pPostScale.p[0] = m_pPostScale.p[1] = .035;
      m_pPostScale.p[2] = m_fPostLength = .40;
      m_dwPostRevolution = RREV_CIRCLE;
      break;

   case LSTANDT_TABLELAMPCERAMIC1:       // shape 1
      m_pPostScale.p[0] = m_pPostScale.p[1] = .25;
      m_pPostScale.p[2] = m_fPostLength = .40;
      m_dwPostRevolution = RREV_CIRCLE;
      m_dwPostProfile = PRROF_CERAMICLAMPSTAND1;
      break;

   case LSTANDT_TABLELAMPCERAMIC2:
      m_pPostScale.p[0] = m_pPostScale.p[1] = .25;
      m_pPostScale.p[2] = m_fPostLength = .40;
      m_dwPostRevolution = RREV_CIRCLE;
      m_dwPostProfile = PRROF_CERAMICLAMPSTAND2;
      break;

   case LSTANDT_TABLELAMPCERAMIC3:
      m_pPostScale.p[0] = m_pPostScale.p[1] = .25;
      m_pPostScale.p[2] = m_fPostLength = .40;
      m_dwPostRevolution = RREV_CIRCLE;
      m_dwPostProfile = PRROF_CERAMICLAMPSTAND3;
      break;

   case LSTANDT_TABLELAMPCERAMIC4:
      m_pPostScale.p[0] = .4;
      m_pPostScale.p[1] = .25;
      m_pPostScale.p[2] = m_fPostLength = .40;
      m_dwPostRevolution = RREV_CIRCLE;
      m_dwPostProfile = PRROF_CERAMICLAMPSTAND1;
      break;

   case LSTANDT_TABLELAMPCERAMIC5:
      m_pPostScale.p[0] = m_pPostScale.p[1] = .25;
      m_pPostScale.p[2] = m_fPostLength = .40;
      m_dwPostRevolution = RREV_HEXAGON;
      m_dwPostProfile = PRROF_CERAMICLAMPSTAND2;
      break;

   case LSTANDT_TABLELAMPCERAMIC6:
      m_pPostScale.p[0] = m_pPostScale.p[1] = .25;
      m_pPostScale.p[2] = m_fPostLength = .40;
      m_dwPostRevolution = RREV_SQUARE;
      m_dwPostProfile = PRROF_CERAMICLAMPSTAND3;
      break;

   case LSTANDT_TABLELAMPSQUAREBASE:
   case LSTANDT_TABLELAMPCYLINDERBASE:
   case LSTANDT_TABLELAMPCURVEDBASE:
   case LSTANDT_TABLELAMPHEXBASE:
   case LSTANDT_TABLELAMPHORNBASE:
      m_pPostScale.p[0] = m_pPostScale.p[1] = .035;
      m_pPostScale.p[2] = m_fPostLength = .10;
      m_dwPostRevolution = RREV_CIRCLE;
      break;

   default:
   case LSTANDT_DESKLAMPSIMPLE:       // simple post up
   case LSTANDT_DESKLAMPCURVEFRONT:       // half bend, ends up going only half of curve
      m_pPostScale.p[0] = m_pPostScale.p[1] = .025;
      m_pPostScale.p[2] = m_fPostLength = .30;
      break;

   case LSTANDT_DESKLAMPCERAMIC1:
      m_pPostScale.p[0] = m_pPostScale.p[1] = .15;
      m_pPostScale.p[2] = m_fPostLength = .30;
      m_dwPostProfile = PRROF_CERAMICLAMPSTAND1;
      break;

   case LSTANDT_DESKLAMPCERAMIC2:
      m_pPostScale.p[0] = m_pPostScale.p[1] = .15;
      m_pPostScale.p[2] = m_fPostLength = .30;
      m_dwPostProfile = PRROF_CERAMICLAMPSTAND2;
      break;

   case LSTANDT_DESKLAMPCERAMIC3:
      m_pPostScale.p[0] = m_pPostScale.p[1] = .15;
      m_pPostScale.p[2] = m_fPostLength = .30;
      m_dwPostProfile = PRROF_CERAMICLAMPSTAND3;
      break;

   case LSTANDT_DESKLAMPCURVEDOWN:       // tulip bend, ends up curcing down
      m_pPostScale.p[0] = m_pPostScale.p[1] = .025;
      m_pPostScale.p[2] = m_fPostLength = .40;
      break;
   case LSTANDT_DESKLAMPLAVA:       // lava lamp base
      m_pPostScale.p[0] = m_pPostScale.p[1] = .15;
      m_pPostScale.p[2] = m_fPostLength = .15;
      m_dwPostProfile = RPROF_LDIAG;
      break;

   case LSTANDT_DESKLAMPARM:       // desk lamp with simple arm
   case LSTANDT_DESKLAMPARMSMALL:       // small arm
   case LSTANDT_DESKLAMPFLEXI:       // flexible
   case LSTANDT_NONEMOUNTED:
   case LSTANDT_NONESTANDING:
   case LSTANDT_CEILINGFIXED1:
   case LSTANDT_CEILINGFIXED2:
   case LSTANDT_CEILINGFIXED3:
   case LSTANDT_FLOURO8W1:      // 1 8w flouro
   case LSTANDT_FLOURO18W1:      // 1 18w flouro
   case LSTANDT_FLOURO18W2:      // 2 18w flouro
   case LSTANDT_FLOURO36W1:      // 1 36w flouro
   case LSTANDT_FLOURO36W2:      // 2 36w flouro
   case LSTANDT_FLOURO36W4:      // 4 36w flouro in drop-down commerical ceiling
   case LSTANDT_FLOUROROUND:      // round flouro light in hemi-sphere
   case LSTANDT_FIRETORCHHOLDER:      // holds torch at an angle
   case LSTANDT_FIRELOG:      // log for the fireplace
      // none
      m_fPostShow = FALSE;
      m_fCanLength = FALSE;
      m_fPostLength = m_pPostScale.p[2] = 0;
      break;

   case LSTANDT_FLOORLAMPSIMPLETALL:   // straight up but tall
      m_pPostScale.p[0] = m_pPostScale.p[1] = .05;
      m_pPostScale.p[2] = m_fPostLength = 2;
      break;
   case LSTANDT_FLOORLAMPSIMPLE:       // straight up
   case LSTANDT_FLOORLAMPTULIP:       // bend on top
   case LSTANDT_FLOORLAMPARMUP:       // arm, light facing up
   case LSTANDT_FLOORLAMPARMANY:       // arm, light any direction
      m_pPostScale.p[0] = m_pPostScale.p[1] = .035;
      m_pPostScale.p[2] = m_fPostLength = 1.5;
      break;

   case LSTANDT_FLOORLAMPMULTISTALK:       // multiple stalks for spot lights
      m_pPostScale.p[0] = m_pPostScale.p[1] = .035;
      m_pPostScale.p[2] = m_fPostLength = 1.5;
      dwNumEZ = 3;
      afEZUp[0] = PI; // so goes straight up
      afEZLoc[1] = .5;
      afEZLoc[2] = .75;
      afEZLeft[2] = PI; // rotate around 180 degrees
      break;

   case LSTANDT_FIRECANDELABRA:      // candelabra
      m_pPostScale.p[0] = m_pPostScale.p[1] = .025;
      m_pPostScale.p[2] = m_fPostLength = .4;
      dwEZStalkType = LSTANDT_FIRETORCHHOLDER; // so one stands up straight
      dwNumEZ = 3;
      afEZLoc[1] = afEZLoc[2] = .5;
      afEZLeft[2] = PI; // rotate around 180 degrees
      break;

   case LSTANDT_OUTDOORPATHWAY:       // outdoor, pathway light
      m_pPostScale.p[0] = m_pPostScale.p[1] = .025;
      m_pPostScale.p[2] = m_fPostLength = .2;
      break;
   case LSTANDT_OUTDOORLAMP2M:       // 2M lamp post
      m_pPostScale.p[0] = m_pPostScale.p[1] = .035;
      m_pPostScale.p[2] = m_fPostLength = 2;
      break;
   case LSTANDT_OUTDOORLAMP3M:       // 3M lamp post
      m_pPostScale.p[0] = m_pPostScale.p[1] = .05;
      m_pPostScale.p[2] = m_fPostLength = 3;
      break;
   case LSTANDT_OUTDOORTULIP:       // tulip stand
      m_pPostScale.p[0] = m_pPostScale.p[1] = .025;
      m_pPostScale.p[2] = m_fPostLength = 1;
      break;
   case LSTANDT_OUTDOORLAMPTRIPPLE:       // hold up three lamps
      m_pPostScale.p[0] = m_pPostScale.p[1] = .075;
      m_pPostScale.p[2] = m_fPostLength = 2;
      dwEZStalkType = LSTANDT_OUTDOORLAMP2M; // so one stands up straight
      dwNumEZ = 3;
      afEZLoc[1] = afEZLoc[2] = .8;
      afEZLeft[2] = PI; // rotate around 180 degrees
      break;
   case LSTANDT_OUTDOOROVERHANG:       // large overhang outdoor
      m_pPostScale.p[0] = m_pPostScale.p[1] = .15;
      m_pPostScale.p[2] = m_fPostLength = 4;
      break;
   case LSTANDT_OUTDOOROVERHANG2:       // overhang, with two lights
      m_pPostScale.p[0] = m_pPostScale.p[1] = .15;
      m_pPostScale.p[2] = m_fPostLength = 4;
      dwNumEZ = 2;
      afEZLeft[1] = PI;
      break;
   case LSTANDT_OUTDOOROVERHANG3:       // overhand with three lights
      m_pPostScale.p[0] = m_pPostScale.p[1] = .15;
      m_pPostScale.p[2] = m_fPostLength = 4;
      dwNumEZ = 3;
      afEZLeft[1] = 2 * PI / 3.0;
      afEZLeft[2] = 2 * PI * 2.0 / 3.0;
      break;
   }


   // pass drawing information to revolution
   m_pPostAroundVec.Normalize();
   m_Revolution.BackfaceCullSet (TRUE);
   m_Revolution.DirectionSet (&m_pPostBottom, &m_pPostAroundVec, &m_pPostLeftVec);
   m_Revolution.ProfileSet (m_dwPostProfile);
   m_Revolution.RevolutionSet (m_dwPostRevolution);
   CPoint pScale;
   pScale.Copy (&m_pPostScale);
   if (m_fCanLength)
      pScale.p[2] = m_fPostLength;
   m_Revolution.ScaleSet (&pScale);

   // create all the stalks
   for (i = 0; i < dwNumEZ; i++) {
      LIGHTSTALK si;
      memset (&si, 0,sizeof(si));
      si.pStalk = new CLightStalk;
      if (!si.pStalk)
         continue;
      si.pStalk->Init (i ? dwEZStalkType2 : dwEZStalkType);

      // create the rotation matrix
      CMatrix mRotUpDown, mRotAround, mTrans;
      mRotUpDown.RotationY (PI/2-afEZUp[i]);
      mRotAround.RotationZ (afEZLeft[i]);
      mRotAround.MultiplyLeft (&mRotUpDown);
      mTrans.Translation (0, 0, afEZLoc[i] * m_fPostLength);
      mTrans.MultiplyLeft (&mRotAround);
      si.mStalkToPost.Copy (&mTrans);

      si.fOffset = afEZLoc[i];

      // keep this
      m_lLIGHTSTALK.Add (&si);
   }

   // done
   return TRUE;
}


/**********************************************************************************
CLightPost::StalkNum - Retursn the number of stalks attached to the post
*/
DWORD CLightPost::StalkNum (void)
{
   return m_lLIGHTSTALK.Num();
}

/**********************************************************************************
CLightPost::StalkGet - Returns a pointer to a stalk object. DONT delelete it.

inputs
   DWORD       dwIndex - From 0 .. StalkNum()-1
returns
   PCLightStalk - stalk object
*/
PCLightStalk CLightPost::StalkGet (DWORD dwIndex)
{
   PLIGHTSTALK p = (PLIGHTSTALK) m_lLIGHTSTALK.Get(dwIndex);
   if (!p)
      return NULL;
   return p->pStalk;
}

/**********************************************************************************
CLightPost::StalkMatrix - Filles pm in with a matrix that convers from stalk coords
to post coords.

inputs
   DWORD       dwIndex - From 0 .. StalkNum()-1
   PCMatrix    pm- filled in
returns
   BOOL - TRUE if success
*/
BOOL CLightPost::StalkMatrix (DWORD dwIndex, PCMatrix pm)
{
   PLIGHTSTALK p = (PLIGHTSTALK) m_lLIGHTSTALK.Get(dwIndex);
   if (!p)
      return FALSE;
   pm->Copy (&p->mStalkToPost);
   return TRUE;
}

/**********************************************************************************
CLightPost::ControlPointQuery - Standard call
*/
BOOL CLightPost::ControlPointQuery (DWORD dwID, POSCONTROL pInfo)
{
   if ((dwID == m_lLIGHTSTALK.Num() * CONTROLPERSTALK) && m_fCanLength) {
      // control point specific to posts where can lengthen

      memset (pInfo, 0 ,sizeof(*pInfo));
      pInfo->cColor = RGB(0,0xff,0xff);
      pInfo->dwID = dwID;
      pInfo->dwStyle = CPSTYLE_CUBE;
      pInfo->fSize = min(.1,max(m_pPostScale.p[0], m_pPostScale.p[1])) * 2;
      pInfo->pLocation.p[2] = m_fPostLength;
      wcscpy (pInfo->szName, L"Shorten/lengthen post");
      return TRUE;
   }

   PLIGHTSTALK p;
   p = (PLIGHTSTALK) m_lLIGHTSTALK.Get(dwID / CONTROLPERSTALK);
   if (!p)
      return FALSE;
   if (!p->pStalk->ControlPointQuery(dwID % CONTROLPERSTALK, pInfo))
      return FALSE;
   pInfo->dwID = dwID;

   // rotate into post space
//   pInfo->pDirection.p[3] = 1;
   pInfo->pLocation.p[3] = 1;
   // ignoreing v1 and v2
   CMatrix mInvTrans;
   p->mStalkToPost.Invert (&mInvTrans);
   mInvTrans.Transpose();
//   pInfo->pDirection.MultiplyLeft (&mInvTrans);
   pInfo->pLocation.MultiplyLeft (&p->mStalkToPost);
   
   return TRUE;
}

/**********************************************************************************
CLightPost::ControlPointSet - Standard call
*/
BOOL CLightPost::ControlPointSet (DWORD dwID, PCPoint pVal, PCPoint pViewer)
{
   if ((dwID == m_lLIGHTSTALK.Num() * CONTROLPERSTALK) && m_fCanLength) {
      m_fPostLength = max(CLOSE, pVal->p[2]);

      CPoint pScale;
      pScale.Copy (&m_pPostScale);
      if (m_fCanLength)
         pScale.p[2] = m_fPostLength;
      m_Revolution.ScaleSet (&pScale);

      DWORD i;
      for (i = 0; i < m_lLIGHTSTALK.Num(); i++) {
         PLIGHTSTALK p = (PLIGHTSTALK) m_lLIGHTSTALK.Get(i);

         CPoint pZero;
         pZero.Zero();
         pZero.p[3] = 1;
         pZero.MultiplyLeft (&p->mStalkToPost);
         
         CMatrix mTrans;
         mTrans.Translation (0, 0, m_fPostLength * p->fOffset - pZero.p[2]);
         p->mStalkToPost.MultiplyRight (&mTrans);
      }
      return TRUE;
   }

   PLIGHTSTALK p;
   p = (PLIGHTSTALK) m_lLIGHTSTALK.Get(dwID / CONTROLPERSTALK);
   if (!p)
      return FALSE;
   if (!p->pStalk->ControlPointIsValid (dwID % CONTROLPERSTALK))
      return FALSE;

   // convert the params to stalk space
   CPoint pNewVal, pNewViewer;
   CMatrix mInv;
   p->mStalkToPost.Invert4 (&mInv);

   pNewVal.Copy (pVal);
   pNewViewer.Copy (pViewer ? pViewer : pVal);
   pNewVal.p[3] = pNewViewer.p[3] = 1;
   pNewVal.MultiplyLeft (&mInv);
   pNewViewer.MultiplyLeft (&mInv);

   if (!p->pStalk->ControlPointSet(dwID % CONTROLPERSTALK, &pNewVal, pViewer ? &pNewViewer : NULL))
      return FALSE;
   return TRUE;
}

/**********************************************************************************
CLightPost::ControlPointEnum - Standard call
*/
void CLightPost::ControlPointEnum (PCListFixed plDWORD)
{
   DWORD i, j, dwLast;
   for (i = 0; i < m_lLIGHTSTALK.Num(); i++) {
      PLIGHTSTALK p = (PLIGHTSTALK) m_lLIGHTSTALK.Get(i);
      dwLast = plDWORD->Num();
      p->pStalk->ControlPointEnum(plDWORD);

      for (j = dwLast; j < plDWORD->Num(); j++) {
         DWORD *pdw = (DWORD*) plDWORD->Get(j);
         (*pdw) += (CONTROLPERSTALK * i);
      }
   }

   if (m_fCanLength) {
      i = m_lLIGHTSTALK.Num() * CONTROLPERSTALK;
      plDWORD->Add (&i);
   }
}

/**********************************************************************************
CLightPost::ControlPointIsValid - Returns TRUE if the control point is valid

inputs
   DWORD       dwID - Contorl point
retursn
   BOOl - TRUE if valid
*/
BOOL CLightPost::ControlPointIsValid (DWORD dwID)
{
   if ((dwID == m_lLIGHTSTALK.Num() * CONTROLPERSTALK) && m_fCanLength) {
      return TRUE;
   }

   PLIGHTSTALK p;
   p = (PLIGHTSTALK) m_lLIGHTSTALK.Get(dwID / CONTROLPERSTALK);
   if (!p)
      return FALSE;
   return p->pStalk->ControlPointIsValid (dwID % CONTROLPERSTALK);
}

/**********************************************************************************
CLightPost::ControlPointMax - Returns the maximum number of control points that
this post (and included stalks) uses
*/
DWORD CLightPost::ControlPointMax (void)
{
   return (m_lLIGHTSTALK.Num()+1) * CONTROLPERSTALK;  // include +1 for own control points
}


static PWSTR gpszPostLength = L"PostLength";
static PWSTR gpszLightStalk = L"LightStalk";
static PWSTR gpszStalkToPostMatrix = L"StalkToPostMatrix";
static PWSTR gpszStalkOffset = L"StalkOffset";

/**********************************************************************************
CLightPost::MMLTo - Standard call
*/
PCMMLNode2 CLightPost::MMLTo (void)
{
   PCMMLNode2 pNode = new CMMLNode2;
   if (!pNode)
      return NULL;
   pNode->NameSet (gpszLightPost);

   // write some info
   MMLValueSet (pNode, gpszType, (int)m_dwType);
   MMLValueSet (pNode, gpszPostLength, m_fPostLength);

   DWORD i;
   for (i = 0; i < m_lLIGHTSTALK.Num(); i++) {
      PLIGHTSTALK p = (PLIGHTSTALK) m_lLIGHTSTALK.Get(i);
      PCMMLNode2 pSub;
      pSub = p->pStalk->MMLTo ();
      if (!pSub)
         continue;
      pSub->NameSet (gpszLightStalk);

      // stick some additional params in there
      MMLValueSet (pSub, gpszStalkToPostMatrix, &p->mStalkToPost);
      MMLValueSet (pSub, gpszStalkOffset, p->fOffset);

      pNode->ContentAdd (pSub);
   }

   // done
   return pNode;
}

/**********************************************************************************
CLightPost::MMLFrom - Standard call
*/
BOOL CLightPost::MMLFrom (PCMMLNode2 pNode)
{
   m_dwType = (DWORD) MMLValueGetInt (pNode, gpszType, 0);
   if (!Init(m_dwType))
      return FALSE;

   m_fPostLength = MMLValueGetDouble (pNode, gpszPostLength, m_fPostLength);
   CPoint pScale;
   pScale.Copy (&m_pPostScale);
   if (m_fCanLength)
      pScale.p[2] = m_fPostLength;
   m_Revolution.ScaleSet (&pScale);

   // clear out stalks and replace
   DWORD i;
   for (i = 0; i < m_lLIGHTSTALK.Num(); i++) {
      PLIGHTSTALK p = (PLIGHTSTALK) m_lLIGHTSTALK.Get(i);
      if (p->pStalk)
         delete p->pStalk;
   }
   m_lLIGHTSTALK.Clear();

   PWSTR psz;
   PCMMLNode2 pSub;
   for (i = 0; i < pNode->ContentNum(); i++) {
      pSub = NULL;
      pNode->ContentEnum (i, &psz, &pSub);
      if (!pSub)
         continue;
      psz = pSub->NameGet ();
      if (!psz)
         continue;

      if (!_wcsicmp(psz, gpszLightStalk)) {
         LIGHTSTALK ls;
         memset (&ls, 0, sizeof(ls));

         // get some additional params
         // stick some additional params in there
         MMLValueGetMatrix (pSub, gpszStalkToPostMatrix, &ls.mStalkToPost);
         ls.fOffset = MMLValueGetDouble (pSub, gpszStalkOffset, 1);

         // post
         ls.pStalk = new CLightStalk;
         if (!ls.pStalk)
            continue;
         ls.pStalk->MMLFrom (pSub);

         // add it
         m_lLIGHTSTALK.Add (&ls);
      }
   }

   // done
   return TRUE;
}

/**********************************************************************************
CLightPost::Clone - Standard call
*/
CLightPost *CLightPost::Clone (void)
{
   PCLightPost pNew = new CLightPost;
   if (!pNew)
      return NULL;
   if (!CloneTo (pNew)) {
      delete pNew;
      return NULL;
   }
   return pNew;
}

/**********************************************************************************
CLightPost::CloneTo - Standard call
*/
BOOL CLightPost::CloneTo (CLightPost *pTo)
{
   // clear out stalks and replace
   DWORD i;
   for (i = 0; i < pTo->m_lLIGHTSTALK.Num(); i++) {
      PLIGHTSTALK p = (PLIGHTSTALK) pTo->m_lLIGHTSTALK.Get(i);
      if (p->pStalk)
         delete p->pStalk;
   }
   pTo->m_lLIGHTSTALK.Clear();

   pTo->m_dwType = m_dwType;
   pTo->m_lLIGHTSTALK.Init (sizeof(LIGHTSTALK), m_lLIGHTSTALK.Get(0), m_lLIGHTSTALK.Num());
   for (i = 0; i < pTo->m_lLIGHTSTALK.Num(); i++) {
      PLIGHTSTALK p = (PLIGHTSTALK) pTo->m_lLIGHTSTALK.Get(i);
      if (p->pStalk)
         p->pStalk = p->pStalk->Clone();
   }
   pTo->m_fPostLength = m_fPostLength;
   pTo->m_fPostShow = m_fPostShow;
   pTo->m_fCanLength = m_fCanLength;
   pTo->m_pPostBottom.Copy (&m_pPostBottom);
   pTo->m_pPostAroundVec.Copy (&m_pPostAroundVec);
   pTo->m_pPostLeftVec.Copy (&m_pPostLeftVec);
   pTo->m_pPostScale.Copy (&m_pPostScale);
   pTo->m_dwPostRevolution = m_dwPostRevolution;
   pTo->m_dwPostProfile = m_dwPostProfile;
   m_Revolution.CloneTo (&pTo->m_Revolution);
   
   return TRUE;
}

/**********************************************************************************
CLightPost::Render - Standard call. NOTE: Only draws the post, NOT the stalks
*/
BOOL CLightPost::Render (POBJECTRENDER pr, PCRenderSurface prs)
{
   if (m_fPostShow)
      m_Revolution.Render (pr, prs);
   return TRUE;
}


/**********************************************************************************
CLightStalk::Constructor and destructor
*/
CLightStalk::CLightStalk (void)
{
   m_dwType = 0;
   m_lJOINTINFO.Init (sizeof(JOINTINFO));
   m_pShadeDir.Zero();
   m_pShadeLeft.Zero();
   m_fDrawStalk = FALSE;
   m_dwSegCurve = 0;
   m_pSize.Zero();
   m_pFront.Zero();
   m_dwShape = 0;
   m_dwShadeDirFreedom = m_dwShadeLeftFreedom = 0;
   m_fDirty = TRUE;
   m_mShadeToStalk.Identity();
}

CLightStalk::~CLightStalk (void)
{
   // intentionally left blank
}


/**********************************************************************************
CLightStalk::Init - Initializes the stalk based on dwType passed in

inputs
   DWORD       dwType - One of LSTALKT_XXX
returns
   BOOL - TRUE if success
*/
BOOL CLightStalk::Init (DWORD dwType)
{
   JOINTINFO ji;
   DWORD i;

   memset (&ji, 0, sizeof(ji));

   m_dwType = dwType;
   m_lJOINTINFO.Clear();
   m_lJOINTINFO.Add (&ji); // so that have starting joint at 0,0,0 that cant move
   m_pShadeDir.Zero();
   m_pShadeDir.p[2] = 1;
   m_pShadeLeft.Zero();
   m_pShadeLeft.p[0] = 1;
   m_fDrawStalk = TRUE;
   m_dwSegCurve = SEGCURVE_LINEAR;
   m_pSize.Zero();
   m_pSize.p[0] = m_pSize.p[1] = .025;
   m_pFront.Zero();
   m_pFront.p[0] = m_pFront.p[2] = 1;
   m_dwShape = NS_CIRCLEFAST;
   m_dwShadeDirFreedom = 0;
   m_dwShadeLeftFreedom = 0;

   switch (m_dwType) {
   case LSTANDT_CEILINGCHANDELIER3:
   case LSTANDT_CEILINGCHANDELIER4:
   case LSTANDT_CEILINGCHANDELIER5:
   case LSTANDT_CEILINGCHANDELIER6:
   case LSTANDT_CEILINGCHANDELIER3UP:
   case LSTANDT_CEILINGCHANDELIER4UP:
   case LSTANDT_CEILINGCHANDELIER5UP:
   case LSTANDT_CEILINGCHANDELIER6UP:
      BOOL fUp;
      fUp = FALSE;
      switch (m_dwType) {
      case LSTANDT_CEILINGCHANDELIER3UP:
      case LSTANDT_CEILINGCHANDELIER4UP:
      case LSTANDT_CEILINGCHANDELIER5UP:
      case LSTANDT_CEILINGCHANDELIER6UP:
         fUp = TRUE;
         break;
      }
      m_dwSegCurve = SEGCURVE_CUBIC;
      m_pSize.p[0] = m_pSize.p[1] = .015;
      m_pShadeDir.Zero();
      m_pShadeDir.p[0] = 1;
      m_pShadeLeft.Zero();
      m_pShadeLeft.p[1] = 1;
      if (!fUp)
         m_pShadeDir.Scale(-1);
      m_dwShadeDirFreedom = 0x07;
      m_dwShadeLeftFreedom = 0x07;
      ji.dwFreedomBits = 0x0f;
      ji.pLoc.Zero();
#define CHANDELTAIL     4
      for (i = 1; i < CHANDELTAIL; i++) {
         ji.pLoc.p[0] = sin((fp)i / (fp)(CHANDELTAIL-1) * PI) * .3 * (fUp ? -1 : 1);
         ji.pLoc.p[2] = .1 * (fp)i;
         m_lJOINTINFO.Add (&ji);
      }
      break;

   case LSTANDT_WALLSTALKOUT:      // wall mounted, straight out
      m_fDrawStalk = FALSE;
      m_pShadeDir.Zero();
      m_pShadeDir.p[1] = 1;
      m_pShadeDir.Scale(-1);
      m_pShadeLeft.Zero();
      m_pShadeLeft.p[2] = 1;
      m_dwShadeDirFreedom = 0x07;
      m_dwShadeLeftFreedom = 0x07;
      break;

   case LSTANDT_WALLSTALKCURVEDDOWN:      // wall mounted, curve down
   case LSTANDT_WALLSTALKCURVEUP:      // wall mounted curve up
      m_dwSegCurve = SEGCURVE_ELLIPSENEXT;
      m_pSize.p[0] = m_pSize.p[1] = .015;
      m_pShadeDir.Zero();
      m_pShadeDir.p[1] = 1;
      m_pShadeLeft.Zero();
      m_pShadeLeft.p[2] = 1;
      if (m_dwType == LSTANDT_WALLSTALKCURVEDDOWN)
         m_pShadeDir.Scale(-1);
      m_dwShadeDirFreedom = 0x07;
      m_dwShadeLeftFreedom = 0x07;
      ji.pLoc.Zero();
      ji.pLoc.p[2] = .05;
      ji.dwFreedomBits = 0;
      m_lJOINTINFO.Add (&ji);
      ji.pLoc.p[1] = .05 * ((m_dwType == LSTANDT_WALLSTALKCURVEDDOWN) ? -1 : 1);
      m_lJOINTINFO.Add (&ji);
      break;

   case LSTANDT_DESKLAMPCURVEDOWN:       // tulip bend, ends up curcing down
      m_dwSegCurve = SEGCURVE_ELLIPSENEXT;
      m_pSize.p[0] = m_pSize.p[1] = .025;
      m_pShadeDir.Scale (-1); // point down
      m_dwShadeDirFreedom = 0x07;
      m_dwShadeLeftFreedom = 0x07;
      ji.pLoc.Zero();
      ji.pLoc.p[2] = .1;
      ji.dwFreedomBits = 0;
      m_lJOINTINFO.Add (&ji);
      ji.pLoc.p[0] = .1;
      m_lJOINTINFO.Add (&ji);
      ji.pLoc.p[0] = .2;
      m_lJOINTINFO.Add (&ji);
      ji.pLoc.p[2] = 0;
      m_lJOINTINFO.Add (&ji);
      break;

   case LSTANDT_DESKLAMPCURVEFRONT:       // half bend, ends up going only half of curve
      m_dwSegCurve = SEGCURVE_ELLIPSENEXT;
      m_pSize.p[0] = m_pSize.p[1] = .025;
      m_pShadeDir.Scale (-1); // point down
      m_dwShadeDirFreedom = 0x07;
      m_dwShadeLeftFreedom = 0x07;
      ji.pLoc.Zero();
      ji.pLoc.p[2] = .1;
      ji.dwFreedomBits = 0;
      m_lJOINTINFO.Add (&ji);
      ji.pLoc.p[0] = .1;
      m_lJOINTINFO.Add (&ji);
      break;

   case LSTANDT_DESKLAMPARM:       // desk lamp with simple arm
   case LSTANDT_DESKLAMPARMSMALL:       // small arm
      m_dwShape = NS_RECTANGLE;
      m_dwSegCurve = SEGCURVE_LINEAR;
      m_pSize.p[0] = m_pSize.p[1] = .05;
      m_pShadeDir.Scale (-1); // point down
      m_dwShadeDirFreedom = 0x07;
      m_dwShadeLeftFreedom = 0x07;
      ji.pLoc.Zero();
      ji.pLoc.p[2] = (m_dwType == LSTANDT_DESKLAMPARMSMALL) ? .4 : .6;
      ji.dwFreedomBits = 0x07;
      m_lJOINTINFO.Add (&ji);
      ji.pLoc.p[0] = (m_dwType == LSTANDT_DESKLAMPARMSMALL) ? .4 : .6;
      m_lJOINTINFO.Add (&ji);
      break;

   case LSTANDT_DESKLAMPFLEXI:       // flexible
      m_dwSegCurve = SEGCURVE_CUBIC;
      m_pSize.p[0] = m_pSize.p[1] = .025;
      m_pShadeDir.Scale (-1); // point down
      m_dwShadeDirFreedom = 0x07;
      m_dwShadeLeftFreedom = 0x07;
      ji.pLoc.Zero();
      ji.pLoc.p[2] = .2;
      ji.dwFreedomBits = 0x07;
      m_lJOINTINFO.Add (&ji);
      ji.pLoc.p[2] += .2 * sqrt((fp)2);
      ji.pLoc.p[0] = .2 * sqrt((fp)2);
      m_lJOINTINFO.Add (&ji);
      ji.pLoc.p[0] += .2 * sqrt((fp)2);
      m_lJOINTINFO.Add (&ji);
      break;

   case LSTANDT_FLOORLAMPMULTISTALK:       // multiple stalks for spot lights
      m_dwSegCurve = SEGCURVE_LINEAR;
      m_pSize.p[0] = m_pSize.p[1] = .025;
      m_pShadeDir.p[1] = 1;
      m_pShadeDir.p[2] = -1;
      m_pShadeDir.Normalize();
      m_dwShadeDirFreedom = 0x07;
      m_dwShadeLeftFreedom = 0x07;
      ji.pLoc.Zero();
      ji.pLoc.p[0] = .20;
      ji.dwFreedomBits = 0;
      m_lJOINTINFO.Add (&ji);
      break;

   case LSTANDT_FLOORLAMPTULIP:       // bend on top
      m_dwSegCurve = SEGCURVE_ELLIPSENEXT;
      m_pSize.p[0] = m_pSize.p[1] = .035;
      m_pShadeDir.Scale (-1); // point down
      m_dwShadeDirFreedom = 0x07;
      m_dwShadeLeftFreedom = 0x07;
      ji.pLoc.Zero();
      ji.pLoc.p[2] = .25;
      ji.dwFreedomBits = 0;
      m_lJOINTINFO.Add (&ji);
      ji.pLoc.p[0] = .25;
      m_lJOINTINFO.Add (&ji);
      ji.pLoc.p[0] = .5;
      m_lJOINTINFO.Add (&ji);
      ji.pLoc.p[2] = 0;
      m_lJOINTINFO.Add (&ji);
      break;

   case LSTANDT_FLOORLAMPARMUP:       // arm, light facing up
      m_dwSegCurve = SEGCURVE_LINEAR;
      m_pSize.p[0] = m_pSize.p[1] = .025;
      ji.pLoc.Zero();
      ji.pLoc.p[0] = .40;
      ji.dwFreedomBits = 0x01 | 0x02;
      m_lJOINTINFO.Add (&ji);
      break;

   case LSTANDT_FLOORLAMPARMANY:       // arm, light any direction
      m_dwSegCurve = SEGCURVE_LINEAR;
      m_pSize.p[0] = m_pSize.p[1] = .025;
      m_pShadeDir.Scale (-1); // point down
      ji.pLoc.Zero();
      ji.pLoc.p[0] = .40;
      ji.dwFreedomBits = 0x01 | 0x02;
      m_lJOINTINFO.Add (&ji);
      m_dwShadeDirFreedom = 0x01 | 0x02 | 0x04;
      m_dwShadeLeftFreedom = m_dwShadeDirFreedom;
      break;

   default:
   case LSTANDT_DESKLAMPSIMPLE:       // simple post up
   case LSTANDT_DESKLAMPLAVA:       // lava lamp base
   case LSTANDT_OUTDOORPATHWAY:       // outdoor, pathway light
   case LSTANDT_OUTDOORLAMP2M:       // 2M lamp post
   case LSTANDT_OUTDOORLAMP3M:       // 3M lamp post
   case LSTANDT_FLOORLAMPSIMPLE:       // straight up
   case LSTANDT_FLOORLAMPSIMPLETALL:   // straight up, but tall
   case LSTANDT_DESKLAMPCERAMIC1:
   case LSTANDT_DESKLAMPCERAMIC2:
   case LSTANDT_DESKLAMPCERAMIC3:
   case LSTANDT_TABLELAMPSIMPLE1:       // just vertical out of round stand
   case LSTANDT_TABLELAMPSIMPLE2:       // square vertical out of sqaure stand
   case LSTANDT_TABLELAMPCERAMIC1:       // shape 1
   case LSTANDT_TABLELAMPCERAMIC2:
   case LSTANDT_TABLELAMPCERAMIC3:
   case LSTANDT_TABLELAMPCERAMIC4:
   case LSTANDT_TABLELAMPCERAMIC5:
   case LSTANDT_TABLELAMPCERAMIC6:
   case LSTANDT_TABLELAMPSQUAREBASE:
   case LSTANDT_TABLELAMPCYLINDERBASE:
   case LSTANDT_TABLELAMPCURVEDBASE:
   case LSTANDT_TABLELAMPHEXBASE:
   case LSTANDT_TABLELAMPHORNBASE:
   case LSTANDT_NONEMOUNTED:
   case LSTANDT_NONESTANDING:
   case LSTANDT_CEILINGSTALK1:
   case LSTANDT_CEILINGSTALK2:
   case LSTANDT_CEILINGSTALK3:
   case LSTANDT_CEILINGFIXED1:
   case LSTANDT_CEILINGFIXED2:
   case LSTANDT_CEILINGFIXED3:
   case LSTANDT_TRACKGLOBES2:
   case LSTANDT_TRACKGLOBES3:
   case LSTANDT_TRACKGLOBES4:
   case LSTANDT_TRACKGLOBES5:
   case LSTANDT_FLOURO8W1:      // 1 8w flouro
   case LSTANDT_FLOURO18W1:      // 1 18w flouro
   case LSTANDT_FLOURO18W2:      // 2 18w flouro
   case LSTANDT_FLOURO36W1:      // 1 36w flouro
   case LSTANDT_FLOURO36W2:      // 2 36w flouro
   case LSTANDT_FLOURO36W4:      // 4 36w flouro in drop-down commerical ceiling
   case LSTANDT_FLOUROROUND:      // round flouro light in hemi-sphere
   case LSTANDT_FIRETORCHHOLDER:      // holds torch at an angle
   case LSTANDT_FIRELOG:      // log for the fireplace
      // no stalk
      m_fDrawStalk = FALSE;
      break;
   case LSTANDT_TRACK2:
   case LSTANDT_TRACK3:
   case LSTANDT_TRACK4:
   case LSTANDT_TRACK5:
   case LSTANDT_WALLSTALK2:      // two stalks
   case LSTANDT_WALLSTALK3:      // three stalks
      // BUGFIX - Moved two wallstalks here so could reorient lights on them
      // no stalk
      m_dwShadeDirFreedom = 0x01 | 0x02 | 0x04;
      m_dwShadeLeftFreedom = m_dwShadeDirFreedom;
      m_fDrawStalk = FALSE;
      break;
   case LSTANDT_OUTDOORTULIP:       // tulip stand
      m_dwSegCurve = SEGCURVE_ELLIPSENEXT;
      m_pSize.p[0] = m_pSize.p[1] = .025;
      m_pShadeDir.Scale (-1); // point down
      ji.pLoc.Zero();
      ji.pLoc.p[2] = .25;
      ji.dwFreedomBits = 0;
      m_lJOINTINFO.Add (&ji);
      ji.pLoc.p[0] = .25;
      m_lJOINTINFO.Add (&ji);
      ji.pLoc.p[0] = .5;
      m_lJOINTINFO.Add (&ji);
      ji.pLoc.p[2] = 0;
      m_lJOINTINFO.Add (&ji);
      break;
   case LSTANDT_OUTDOORLAMPTRIPPLE:       // hold up three lamps
      m_dwSegCurve = SEGCURVE_ELLIPSENEXT;
      m_pSize.p[0] = m_pSize.p[1] = .025;
      ji.pLoc.Zero();
      ji.pLoc.p[0] = .5;
      ji.dwFreedomBits = 0;
      m_lJOINTINFO.Add (&ji);
      ji.pLoc.Zero();
      ji.pLoc.p[0] = .5;
      ji.pLoc.p[2] = .2;
      ji.dwFreedomBits = 0;
      m_lJOINTINFO.Add (&ji);
      break;
   case LSTANDT_FIRECANDELABRA:       // hold up three lamps
      m_dwSegCurve = SEGCURVE_ELLIPSENEXT;
      m_pSize.p[0] = m_pSize.p[1] = .015;
      ji.pLoc.Zero();
      ji.pLoc.p[0] = .15;
      ji.dwFreedomBits = 0;
      m_lJOINTINFO.Add (&ji);
      ji.pLoc.p[2] = .15;
      ji.dwFreedomBits = 0;
      m_lJOINTINFO.Add (&ji);
      break;
   case LSTANDT_OUTDOOROVERHANG:       // large overhang outdoor
   case LSTANDT_OUTDOOROVERHANG2:       // overhang, with two lights
   case LSTANDT_OUTDOOROVERHANG3:       // overhand with three lights
      m_dwSegCurve = SEGCURVE_ELLIPSENEXT;
      m_pSize.p[0] = m_pSize.p[1] = .075;
      m_pShadeDir.Scale (-1); // point down
      ji.pLoc.Zero();
      ji.pLoc.p[2] = 1;
      ji.dwFreedomBits = 0;
      m_lJOINTINFO.Add (&ji);
      ji.pLoc.Zero();
      ji.pLoc.p[0] = 1.5;
      ji.pLoc.p[2] = .5;
      ji.dwFreedomBits = 0;
      m_lJOINTINFO.Add (&ji);
      break;
   }

   // done
   m_fDirty = TRUE;
   return TRUE;
}

/**********************************************************************************
CLightStalk::MatrixGet - Fills mShadeToStalk in.

inputs
   PCMatrix    mShadeToStalk - Filled in with the matrix that will rotate and tanslate
               the shade to the proper location
returns
   BOOL - TRUE if success
*/
BOOL CLightStalk::MatrixGet (PCMatrix mShadeToStalk)
{
   if (!CalcRenderIfNecessary())
      return FALSE;

   mShadeToStalk->Copy (&m_mShadeToStalk);
   return TRUE;
}


/**********************************************************************************
CLightStalk::ControlPointQuery - Standard function
*/
BOOL CLightStalk::ControlPointQuery (DWORD dwID, POSCONTROL pInfo)
{
   if (!ControlPointIsValid(dwID))
      return FALSE;
   if (!CalcRenderIfNecessary())
      return FALSE;

   memset (pInfo, 0, sizeof(*pInfo));
   pInfo->dwID = dwID;

   if (dwID == 0) {
      pInfo->cColor = RGB(0xff,0xff,0);
      pInfo->dwStyle = CPSTYLE_POINTER;
      pInfo->fSize = .1;
//      pInfo->pDirection.Copy (&m_pShadeDir);
      pInfo->pLocation.Copy (&m_pShadeDir);
      pInfo->pLocation.Normalize();
      pInfo->pLocation.Scale (.5);

      CPoint pAdd;
      pAdd.Zero();
      if (JointNum())
         JointGet (JointNum()-1, &pAdd, NULL);

      pInfo->pLocation.Add (&pAdd);

      wcscpy (pInfo->szName, L"Light direction");
      return TRUE;
   }
   else if (dwID == 1) {
      pInfo->cColor = RGB(0xff,0x80,0);
      pInfo->dwStyle = CPSTYLE_POINTER;
      pInfo->fSize = .05;
//      pInfo->pDirection.Copy (&m_pShadeLeft);
      pInfo->pLocation.Copy (&m_pShadeLeft);
      pInfo->pLocation.Normalize();
      pInfo->pLocation.Scale (.3);

      CPoint pAdd;
      pAdd.Zero();
      if (JointNum())
         JointGet (JointNum()-1, &pAdd, NULL);

      pInfo->pLocation.Add (&pAdd);

      wcscpy (pInfo->szName, L"Light tilt");
      return TRUE;
   }

   // else it's a joint
   CPoint pLoc;
   if (!JointGet (dwID - 2, &pLoc, NULL))
      return FALSE;
   pInfo->cColor = RGB(0xff,0, 0xff);
   pInfo->dwStyle = CPSTYLE_CUBE;
   pInfo->fSize = min(.1,max(m_pSize.p[0], m_pSize.p[1])) * 2;
   pInfo->pLocation.Copy (&pLoc);
   wcscpy (pInfo->szName, L"Arm movement");

   return TRUE;
}

/**********************************************************************************
CLightStalk::ControlPointSet - Standard function
*/
BOOL CLightStalk::ControlPointSet (DWORD dwID, PCPoint pVal, PCPoint pViewer)
{
   if (!ControlPointIsValid(dwID))
      return FALSE;
   if (!CalcRenderIfNecessary())
      return FALSE;

   DWORD i;
   CPoint pCenter;
   CPoint pNew;

   if ((dwID == 0) || (dwID == 1)) {
      pCenter.Zero();
      if (JointNum())
         JointGet (JointNum()-1, &pCenter, NULL);

      pNew.Subtract (pVal, &pCenter);
      DWORD dwFreedom;
      dwFreedom = (dwID ? m_dwShadeLeftFreedom : m_dwShadeDirFreedom);
      for (i = 0; i < 3; i++)
         if (!(dwFreedom & (1 << i)))
            pNew.p[i] = 0;
      pNew.Normalize();
      if (pNew.Length() < .5)
         pNew.p[0] = 1; // pick a direction

      if (dwID)
         m_pShadeLeft.Copy (&pNew);
      else
         m_pShadeDir.Copy (&pNew);

      // make sure left is perp to dir
      CPoint pA;
      if (m_pShadeDir.DotProd (&m_pShadeLeft) >= 1.0 - CLOSE) {
         for (i = 0; i < 3; i++) {
            m_pShadeLeft.Zero();
            m_pShadeLeft.p[i] = 1;
            if (m_pShadeDir.DotProd (&m_pShadeLeft) < 1.0 - CLOSE)
               break;
         }
      }
      pA.CrossProd (&m_pShadeDir, &m_pShadeLeft);
      pA.Normalize();
      m_pShadeLeft.CrossProd (&pA, &m_pShadeDir);
      m_pShadeLeft.Normalize();

      m_fDirty = TRUE;
      return TRUE;
   }

   // else it's a joint

   // get old locaiton
   DWORD dwJoint;
   DWORD dwFreedom;
   dwJoint = dwID - 2;
   if (!JointGet (dwJoint, &pCenter, &dwFreedom))
      return FALSE;

   // and old length
   fp fOldLength;
   CPoint pOld;
   pOld.Zero();
   if (dwJoint)
      JointGet (dwJoint-1, &pOld, NULL);
   CPoint pLen;
   pLen.Subtract (&pOld, &pCenter);
   fOldLength = pLen.Length();

   // adjust the new value to planes
   pNew.Copy (pVal);
   for (i = 0; i < 3; i++)
      if (!(dwFreedom & (1 << i)))
         pNew.p[i] = pCenter.p[i];

   if (!(dwFreedom & 0x08)) {
      // make sure the length is the same
      pNew.Subtract (&pOld);
      pNew.Normalize();
      if (pNew.Length() < .5)
         pNew.p[0] = 1;
      pNew.Scale (fOldLength);
      pNew.Add (&pOld);
   }

   // set it
   JointSet (dwJoint, &pNew, dwFreedom);

   // offset all the others
   pNew.Subtract (&pCenter);
   for (i = dwJoint+1; i < JointNum(); i++) {
      JointGet (i, &pCenter, &dwFreedom);
      pCenter.Add (&pNew);
      JointSet (i, &pCenter, dwFreedom);
   }
   m_fDirty = TRUE;

   return TRUE;
}

/**********************************************************************************
CLightStalk::ControlPointEnum - Standard function
*/
void CLightStalk::ControlPointEnum (PCListFixed plDWORD)
{
   DWORD i;
   for (i = 0; i < 2 + JointNum(); i++)
      if (ControlPointIsValid(i))
         plDWORD->Add (&i);
}

/**********************************************************************************
CLightStalk::ControlPointIsValid - Returns TRUE if the control point requested
is valid

input
   DWORD       dwID - Control point ID
returns
   BOOL - TRUE if valid
*/
BOOL CLightStalk::ControlPointIsValid (DWORD dwID)
{
   switch (dwID) {
   case 0:  // shade direction
      if (m_dwShadeDirFreedom)
         return TRUE;
      break;
   case 1:  // shade left
      if (m_dwShadeLeftFreedom)
         return TRUE;
      break;
   default:
      DWORD dwFreedomBits;
      if (JointGet (dwID - 2, NULL, &dwFreedomBits))
         return dwFreedomBits ? TRUE : FALSE;
      break;
   }
   return FALSE;
}

static PWSTR gpszShadeDir = L"ShadeDir";
static PWSTR gpszShadeLeft = L"ShadeLeft";

/**********************************************************************************
CLightStalk::MMLTo - Writes the light stalk information to MML

returns
   PCMMLNode2 - MML node with info
*/
PCMMLNode2 CLightStalk::MMLTo (void)
{
   PCMMLNode2 pNode = new CMMLNode2;
   if (!pNode)
      return NULL;
   pNode->NameSet (gpszLightStalk);

   MMLValueSet (pNode, gpszType, (int) m_dwType);
   MMLValueSet (pNode, gpszShadeDir, &m_pShadeDir);
   MMLValueSet (pNode, gpszShadeLeft, &m_pShadeLeft);

   DWORD i;
   WCHAR szTemp[64];
   for (i = 0; i < m_lJOINTINFO.Num(); i++) {
      PJOINTINFO pji = (PJOINTINFO) m_lJOINTINFO.Get(i);

      swprintf (szTemp, L"JointLoc%d", (int) i);
      MMLValueSet (pNode, szTemp, &pji->pLoc);
      swprintf (szTemp, L"JointFreedomBits%d", (int) i);
      MMLValueSet (pNode, szTemp, (int) pji->dwFreedomBits);
   }

   return pNode;
}

/**********************************************************************************
CLightStalk::MMLFrom - Standard frunction
*/
BOOL CLightStalk::MMLFrom (PCMMLNode2 pNode)
{
   m_dwType = (DWORD) MMLValueGetInt (pNode, gpszType, (int) 0);
   if (!Init(m_dwType))
      return FALSE;

   MMLValueGetPoint (pNode, gpszShadeDir, &m_pShadeDir);
   MMLValueGetPoint (pNode, gpszShadeLeft, &m_pShadeLeft);

   DWORD i;
   WCHAR szTemp[64];
   for (i = 0; i < m_lJOINTINFO.Num(); i++) {
      PJOINTINFO pji = (PJOINTINFO) m_lJOINTINFO.Get(i);

      swprintf (szTemp, L"JointLoc%d", (int) i);
      MMLValueGetPoint (pNode, szTemp, &pji->pLoc);
      swprintf (szTemp, L"JointFreedomBits%d", (int) i);
      pji->dwFreedomBits = MMLValueGetInt (pNode, szTemp, (int) pji->dwFreedomBits);
   }

   return TRUE;
}

/**********************************************************************************
CLightStalk::Clone - Standard function
*/
CLightStalk *CLightStalk::Clone (void)
{
   PCLightStalk pNew = new CLightStalk;
   if (!pNew)
      return NULL;
   if (!CloneTo (pNew)) {
      delete pNew;
      return NULL;
   }
   return pNew;
}

/**********************************************************************************
CLightStalk::CloneTo - Standard functuon
*/
BOOL CLightStalk::CloneTo (CLightStalk *pTo)
{
   pTo->Init (m_dwType);
   pTo->m_pShadeDir.Copy (&m_pShadeDir);
   pTo->m_pShadeLeft.Copy (&m_pShadeLeft);
   pTo->m_lJOINTINFO.Init (sizeof(JOINTINFO), m_lJOINTINFO.Get(0), m_lJOINTINFO.Num());

   pTo->m_fDirty = TRUE;
   return TRUE;
}

/**********************************************************************************
CLightStalk::Render - Draw the stalk. NOTE: Must have set surface before calling this.
*/
BOOL CLightStalk::Render (POBJECTRENDER pr, PCRenderSurface prs)
{
   if (!m_fDrawStalk)
      return TRUE;

   if (!CalcRenderIfNecessary())
      return FALSE;

   m_Noodle.Render (pr, prs);
   return TRUE;
}

/**********************************************************************************
CLightStalk::JointNum - Returns the number of joints
*/
DWORD CLightStalk::JointNum (void)
{
   DWORD dwNum = m_lJOINTINFO.Num();
   if (dwNum)
      return dwNum-1;
   else
      return 0;
}

/**********************************************************************************
CLightStalk::JointSet - Sets the value of a joint. No length adjustment or
freedom adjustments are done.
*/
BOOL CLightStalk::JointSet (DWORD dwJoint, PCPoint pLoc, DWORD dwFreedomBits)
{
   JOINTINFO ji;
   ji.dwFreedomBits = dwFreedomBits;
   ji.pLoc.Copy (pLoc);
   m_fDirty = TRUE;
   return m_lJOINTINFO.Set (dwJoint+1, &ji);
}

/**********************************************************************************
CLightStalk::JointGet - Gets the values for a joint.

inputs
   DWORD       dwJoint - Joint number
   PCPoint     pLoc - Filled with location
   DWORD       *pdwFreedomBits - Filled with fredom bits
returns
   BOOL - TRUE if success
*/
BOOL CLightStalk::JointGet (DWORD dwJoint, PCPoint pLoc, DWORD *pdwFreedomBits)
{
   PJOINTINFO pji = (PJOINTINFO) m_lJOINTINFO.Get(dwJoint+1);
   if (!pji)
      return FALSE;

   if (pLoc)
      pLoc->Copy (&pji->pLoc);
   if (pdwFreedomBits)
      *pdwFreedomBits = pji->dwFreedomBits;
   return TRUE;
}

/**********************************************************************************
CLightStalk::CalcRenderIfNecessary - If needs to recalculate the noodle or some
or the matricies it does
*/
BOOL CLightStalk::CalcRenderIfNecessary(void)
{
   DWORD i;
   if (!m_fDirty)
      return TRUE;

   // calculate soem information
   PJOINTINFO pji;
   m_pFront.Zero();
   if (m_lJOINTINFO.Num() >= 3) {
      // calculate front vector
      pji = (PJOINTINFO) m_lJOINTINFO.Get(0);
      CPoint p1, p2;
      p1.Subtract (&pji[0].pLoc, &pji[1].pLoc);
      p2.Subtract (&pji[1].pLoc, &pji[2].pLoc);
      m_pFront.CrossProd (&p1, &p2);
      m_pFront.Normalize();
   }
   if (m_pFront.Length() < .5)
      m_pFront.p[0] = m_pFront.p[2] = 1;  // pick something


   // calculate shade to stalk
   CPoint pA, pB, pC;
   m_pShadeDir.Normalize();
   pC.Copy (&m_pShadeDir);
   m_pShadeLeft.Normalize();
   if (m_pShadeLeft.DotProd (&m_pShadeDir) >= 1 - CLOSE) {
      // left it too close
      for (i = 0; i < 3; i++) {
         m_pShadeLeft.Zero();
         m_pShadeLeft.p[i] = 1;
         if (m_pShadeLeft.DotProd (&m_pShadeDir) < 1 - CLOSE)
            break;
      }
      // new shade left
   }
   pA.Copy (&m_pShadeLeft);
   pB.CrossProd (&pC, &pA);
   pB.Normalize();
   pA.CrossProd (&pB, &pC);
   pA.Normalize();
   m_mShadeToStalk.RotationFromVectors (&pA, &pB, &pC);
   if (m_lJOINTINFO.Num()) {
      pji = (PJOINTINFO) m_lJOINTINFO.Get(m_lJOINTINFO.Num()-1);
      CMatrix mTrans;
      mTrans.Translation (pji->pLoc.p[0], pji->pLoc.p[1], pji->pLoc.p[2]);
      m_mShadeToStalk.MultiplyRight (&mTrans);
   }

   // create the noodle
   m_Noodle.BackfaceCullSet (TRUE);
   m_Noodle.DrawEndsSet (TRUE);
   m_Noodle.FrontVector (&m_pFront);
   m_Noodle.ScaleVector (&m_pSize);
   m_Noodle.ShapeDefault (m_dwShape);
   CListFixed lPoints;
   lPoints.Init (sizeof(CPoint));
   lPoints.Required (m_lJOINTINFO.Num());
   for (i = 0; i < m_lJOINTINFO.Num(); i++) {
      PJOINTINFO pji = (PJOINTINFO) m_lJOINTINFO.Get(i);
      lPoints.Add (&pji->pLoc);
   }
   m_Noodle.PathSpline (FALSE, lPoints.Num(), (PCPoint)lPoints.Get(0),
      (DWORD*)(size_t) m_dwSegCurve, 2);

   m_fDirty = FALSE;
   return TRUE;
}


/**********************************************************************************
CLightShade::Constructor and destructor
*/
CLightShade::CLightShade (void)
{
   m_dwType = 0;
   m_fBulbWatts = 0;
   m_fBulbNoShadows = FALSE;
   m_dwBulbColor = LBT_COLOR_WHITE;
   m_pSize.Zero();
   m_dwBulbShape = 0;
   m_pBulbLoc.Zero();
   m_pBulbDir.Zero();
   m_pBulbLeft.Zero();
   m_fBulbDraw = FALSE;
   m_mLightBulb.Identity();

   m_pLightVec.Zero();
   memset (m_afLightAmt, 0, sizeof(m_afLightAmt));
   memset (m_awLightColor, 0, sizeof(m_awLightColor));
   memset (m_apLightDirect, 0, sizeof(m_apLightDirect));
   memset (m_afShadeShow, 0, sizeof(m_afShadeShow));
   memset (m_adwShadeProfile, 0, sizeof(m_adwShadeProfile));
   memset (m_adwShadeRev, 0,sizeof( m_adwShadeRev));
   memset (m_apShadeBottom, 0 , sizeof(m_apShadeBottom));
   memset (m_apShadeSize, 0 , sizeof(m_apShadeSize));
   memset (m_apShadeAroundVec, 0, sizeof(m_apShadeAroundVec));
   memset (m_apShadeLeftVec, 0,sizeof(m_apShadeLeftVec));
   memset (m_adwShadeMaterial, 0, sizeof(m_adwShadeMaterial));
   memset (m_afShadeAffectsOmni, 0, sizeof(m_afShadeAffectsOmni));
   m_fDirtyLI = TRUE;
   m_fDirtyRender = TRUE;
   m_fWasShadeTransparent = TRUE;
   memset (&m_LI, 0, sizeof(m_LI));
   memset (m_apRevolution, 0, sizeof(m_apRevolution));
}

CLightShade::~CLightShade (void)
{
   DWORD i;
   for (i = 0; i < 2; i++) {
      if (m_apRevolution[i])
         delete m_apRevolution[i];
      m_apRevolution[i] = 0;
   }
}


/**********************************************************************************
CLightShade::Init - Initialize all the parameters of LightShade based on dwType.

inputs
   DWORD       dwType - One of LST_XXX
returns
   BOOL - TRUE if success
*/
BOOL CLightShade::Init (DWORD dwType)
{
   CImage Image;
   CPoint pSize;

   m_dwType = dwType;

   // some reasonable defaults
   DWORD dwBulbQuick;
   dwBulbQuick = LBT_INCANGLOBE60;
   m_fBulbDraw = TRUE;
   pSize.Zero();

   switch (m_dwType) {
   case LST_FIRECANDLE1:      // candle with holder
      dwBulbQuick = LBT_FLAMECANDLE;
      pSize.p[0] = pSize.p[1] = .02;
      pSize.p[2] = .3;
      break;

   case LST_FIRECANDLE2:      // candle without holder, but thicker
      dwBulbQuick = LBT_FLAMECANDLE;
      pSize.p[0] = pSize.p[1] = .10;
      pSize.p[2] = .1;
      break;

   case LST_FIRETORCH:      // torch
      dwBulbQuick = LBT_FLAMETORCH;
      pSize.p[0] = pSize.p[1] = .1;
      pSize.p[2] = .6;
      break;

   case LST_FIREBOWL:      // bowl with fire in it
      dwBulbQuick = LBT_FLAMEFIRE;
      pSize.p[0] = pSize.p[1] = .5;
      pSize.p[2] = .1;
      break;

   case LST_FIRELANTERNHUNG:      // hanging lantern
      dwBulbQuick = LBT_FLAMECANDLE;
      pSize.p[0] = pSize.p[1] = .15;
      pSize.p[2] = .4;
      break;

   case LST_FLAMECANDLE:      // candle flame by itself
      dwBulbQuick = LBT_FLAMECANDLE;
      pSize.Zero();
      break;
   case LST_FLAMETORCH:      // torch flame by itself
      dwBulbQuick = LBT_FLAMETORCH;
      pSize.Zero();
      break;
   case LST_FLAMEFIRE:      // fire flame by itself
      dwBulbQuick = LBT_FLAMEFIRE;
      pSize.Zero();
      break;

   case LST_BULBINCANGLOBE:
      dwBulbQuick = LBT_INCANGLOBE60;
      pSize.Zero();
      break;
   case LST_BULBINCANSPOT:
      dwBulbQuick = LBT_INCANSPOT100;
      pSize.Zero();
      break;
   case LST_BULBHALOGENSPOT:
      dwBulbQuick = LBT_HALOSPOT25;
      pSize.Zero();
      break;
   case LST_BULBHALOGENBULB:
      dwBulbQuick = LBT_HALOBULB8;
      pSize.Zero();
      break;
   case LST_BULBHALOGENTUBE:
      dwBulbQuick = LBT_HALOTUBE150;
      pSize.Zero();
      break;
   case LST_BULBFLOURO8:
      dwBulbQuick = LBT_FLOURO8;
      pSize.Zero();
      break;
   case LST_BULBFLOURO18:
      dwBulbQuick = LBT_FLOURO18;
      pSize.Zero();
      break;
   case LST_BULBFLOURO36:
      dwBulbQuick = LBT_FLOURO36;
      pSize.Zero();
      break;
   case LST_BULBFLOUROROUND:
      dwBulbQuick = LBT_FLOUROROUND;
      pSize.Zero();
      break;
   case LST_BULBFLOUROGLOBE:
      dwBulbQuick = LBT_FLOUROGLOBE;
      pSize.Zero();
      break;
   case LST_BULBSODIUMGLOBE:
      dwBulbQuick = LBT_SODIUMGLOBE;
      pSize.Zero();
      break;

   case LST_INCANSCONCECONE:      // cone with light going up
   case LST_INCANSCONCEHEMISPHERE:      // hemisphere with light goingup
   case LST_INCANSCONELOOP:      // loop with light going up and down
      dwBulbQuick = LBT_INCANGLOBE60;
      pSize.p[0] = pSize.p[1] = .3;
      pSize.p[2] = pSize.p[0] / 2.0;;
      break;

   case LST_FLOURO8W1:      // 1 8w flouro
   case LST_FLOURO8W1CURVED:      // 1 8w flouro
      dwBulbQuick = LBT_FLOURO8;
      pSize.p[0] = CM_FLOURO8LENGTH * 1.2;
      pSize.p[1] = CM_FLOURO8DIAMETER * 2;
      pSize.p[2] = CM_FLOURO8DIAMETER * 2;
      break;
      
   case LST_FLOURO18W1:      // 1 18w flouro
   case LST_FLOURO18W1CURVED:      // 1 18w flouro
      dwBulbQuick = LBT_FLOURO18;
      pSize.p[0] = CM_FLOURO18LENGTH * 1.2;
      pSize.p[1] = CM_FLOURO18DIAMETER * 2;
      pSize.p[2] = CM_FLOURO18DIAMETER * 2;
      break;
      
   case LST_FLOURO18W2:      // 2 18w flouro
   case LST_FLOURO18W2CURVED:      // 2 18w flouro
      dwBulbQuick = LBT_FLOURO18x2;
      pSize.p[0] = CM_FLOURO18LENGTH * 1.2;
      pSize.p[1] = CM_FLOURO18DIAMETER * 4;
      pSize.p[2] = CM_FLOURO18DIAMETER * 2;
      m_fBulbDraw = FALSE;
      break;

   case LST_FLOURO36W1:      // 1 36w flouro
   case LST_FLOURO36W1CURVED:      // 1 36w flouro
      dwBulbQuick = LBT_FLOURO36;
      pSize.p[0] = CM_FLOURO36LENGTH * 1.2;
      pSize.p[1] = CM_FLOURO36DIAMETER * 2;
      pSize.p[2] = CM_FLOURO36DIAMETER * 2;
      break;

   case LST_FLOURO36W2:      // 2 36w flouro
   case LST_FLOURO36W2CURVED:      // 2 36w flouro
      dwBulbQuick = LBT_FLOURO36x2;
      pSize.p[0] = CM_FLOURO36LENGTH * 1.2;
      pSize.p[1] = CM_FLOURO36DIAMETER * 4;
      pSize.p[2] = CM_FLOURO36DIAMETER * 2;
      m_fBulbDraw = FALSE;
      break;

   case LST_FLOURO36W4:      // 4 36w flouro in drop-down commerical ceiling
      dwBulbQuick = LBT_FLOURO36x4;
      pSize.p[0] = CM_OFFICE4FLOUROLENGTH;
      pSize.p[1] = CM_OFFICE4FLOUROWIDTH;
      pSize.p[2] = CM_FLOURO36DIAMETER / 2;   // no actuall height on this one
      m_fBulbDraw = FALSE;
      break;

   case LST_FLOUROROUND:      // round flouro light in hemi-sphere
      dwBulbQuick = LBT_FLOUROROUND;
      pSize.p[0] = pSize.p[1] = CM_FLOUROROUNDRINGDIAMETER * 1.2;
      pSize.p[2] = CM_FLOUROROUNDDIAMETER * 2;
      break;

   case LST_INCANBULBSOCKET:
      dwBulbQuick = LBT_INCANGLOBE60;
      pSize.p[0] = pSize.p[1] = CM_INCANGLOBEDIAMETER * 2.0 / 3.0;
      pSize.p[2] = CM_INCANGLOBELENGTH / 3;
      break;

   case LST_INCANSPOTSOCKET:
      dwBulbQuick = LBT_INCANSPOT100;
      pSize.p[0] = pSize.p[1] = CM_INCANSPOTDIAMETER * 2.0 / 3.0;
      pSize.p[2] = CM_INCANSPOTLENGTH * 2 / 3;
      break;

   case LST_SKYLIGHTDIFFUSERROUND:
   case LST_SKYLIGHTDIFFUSERSQUARE:
      dwBulbQuick = LBT_SKYLIGHT;
      pSize.p[0] = pSize.p[1] = .5;
      pSize.p[2] = .02;
      break;

   case LST_INCANCEILSQUARE:      // square profile, on the ceiling
   case LST_INCANCEILROUND:      // roun, on ceiling
      dwBulbQuick = LBT_INCANGLOBE60;
      pSize.p[0] = pSize.p[1] = CM_INCANGLOBELENGTH * 3;
      pSize.p[2] = CM_INCANGLOBEDIAMETER;
      break;
   case LST_INCANCEILHEMISPHERE:      // hemisphere profile, on the ceiling
   case LST_INCANCEILCONE:      // cone, on the ceiling
      dwBulbQuick = LBT_INCANGLOBE60;
      pSize.p[0] = pSize.p[1] = CM_INCANGLOBELENGTH * 3;
      pSize.p[2] = CM_INCANGLOBEDIAMETER * 1.5;
      break;
   case LST_INCANCEILSPOT:      // embedded in ceiling as spotlight
      dwBulbQuick = LBT_INCANGLOBE60;
      pSize.p[0] = pSize.p[1] = CM_INCANGLOBEDIAMETER * 2;
      pSize.p[2] = CM_INCANGLOBEDIAMETER / 4;
      break;

   case LST_HALOGENCEILSPOT:      // embedded in ceiling as spotlight
      dwBulbQuick = LBT_HALOSPOT25;
      pSize.p[0] = pSize.p[1] = CM_HALOGENSPOTDIAMETER * 2;
      pSize.p[2] = CM_HALOGENSPOTDIAMETER / 4;
      break;

   default:
   case LST_GLASSINCANROUNDCONEDN:
   case LST_GLASSINCANSQUARECONEDN:
   case LST_GLASSINCANHEXCONEDN:
   case LST_GLASSINCANROUNDHEMIDN:       // hemisphere
   case LST_GLASSINCANHEXHEMIDN:       // hexagon around, hemisphere shape
   case LST_GLASSINCANOCTHEMIDN:
   case LST_GLASSINCANROUNDCONE:
   case LST_GLASSINCANSQUARECONE:
   case LST_GLASSINCANHEXCONE:
   case LST_GLASSINCANROUNDHEMI:       // hemisphere
   case LST_GLASSINCANHEXHEMI:       // hexagon around, hemisphere shape
   case LST_GLASSINCANOCTHEMI:
      dwBulbQuick = LBT_INCANGLOBE60;
      pSize.p[0] = pSize.p[1] = .7;
      pSize.p[2] = .4;
      break;

   case LST_INCANPOOLTABLE:
      dwBulbQuick = LBT_INCANGLOBE240;
      pSize.p[0] = 1;
      pSize.p[1] = .5;
      pSize.p[2] = .25;
      break;

   case LST_GLASSINCANSQUAREOPENDN:
   case LST_GLASSINCANSQUAREOPEN:
   case LST_GLASSINCANROUNDEDDN:
   case LST_GLASSINCANROUNDED:
      dwBulbQuick = LBT_INCANGLOBE60;
      pSize.p[0] = pSize.p[1] = .3;
      pSize.p[2] = .6;
      break;
      
   case LST_GLASSINCANOLDFASHIONED:       // old fashioned incandescent cover
      dwBulbQuick = LBT_INCANGLOBE60;
      pSize.p[0] = pSize.p[1] = .3;
      pSize.p[2] = .25;
      break;

   case LST_GLASSINCANBOX:       // box around the light
   case LST_GLASSINCANBOXLID:       // box around the light
   case LST_GLASSINCANCYLINDER:       // cylinder around the light
   case LST_GLASSINCANCYLINDERLID:       // cylinder around the light
      dwBulbQuick = LBT_INCANGLOBE60;
      pSize.p[0] = pSize.p[1] = .15;
      pSize.p[2] = .2;
      break;
   case LST_GLASSINCANBULB:       // bulb around the light
   case LST_GLASSINCANBULBLID:       // bulb around the light
   case LST_GLASSINCANBULBBBASE:
      dwBulbQuick = LBT_INCANGLOBE60;
      pSize.p[0] = pSize.p[1] = .2;
      pSize.p[2] = .2;
      break;

   case LST_GLASSINCANBOXROOF:       // rectangular with roof
   case LST_GLASSINCANCYLINDERROOF:       // cylinder with roof
      dwBulbQuick = LBT_INCANGLOBE60;
      pSize.p[0] = pSize.p[1] = .20;
      pSize.p[2] = .25;
      break;
   case LST_HALOGEN8BOXROOF:       // rectangular with roof
   case LST_HALOGEN8CYLINDERROOF:       // cylinder with roof
      dwBulbQuick = LBT_HALOBULB8;
      pSize.p[0] = pSize.p[1] = .1;
      pSize.p[2] = .125;
      break;
   case LST_HALOGENSPOT1:       // style 1 - flared
      dwBulbQuick = LBT_HALOSPOT25;
      pSize.p[0] = pSize.p[1] = CM_HALOGENSPOTDIAMETER * 3;
      pSize.p[2] = CM_HALOGENSPOTLENGTH * 2;
      break;
   case LST_HALOGENSPOT2:       // style 2- angled
      dwBulbQuick = LBT_HALOSPOT25;
      pSize.p[0] = pSize.p[1] = CM_HALOGENSPOTDIAMETER * 2;
      pSize.p[2] = CM_HALOGENSPOTLENGTH * 2;
      break;
   case LST_HALOGENSPOT3:       // style 3 - larger at one end
      dwBulbQuick = LBT_HALOSPOT25;
      pSize.p[0] = pSize.p[1] = CM_HALOGENSPOTDIAMETER * 1.5;
      pSize.p[2] = CM_HALOGENSPOTLENGTH * 1.8;
      break;
   case LST_HALOGENSPOT4:       // cylinder
      dwBulbQuick = LBT_HALOSPOT25;
      pSize.p[0] = pSize.p[1] = CM_HALOGENSPOTDIAMETER * 1.2;
      pSize.p[2] = CM_HALOGENSPOTLENGTH * 1.5;
      break;

   case LST_HALOGENTUBESPOT:
      dwBulbQuick = LBT_HALOTUBE250;
      pSize.p[0] = CM_HALOGENTUBELENGTH * 2;
      pSize.p[1] = CM_HALOGENTUBELENGTH * 1.5;
      pSize.p[2] = pSize.p[1];
      break;

   case LST_INCANSPOT1:       // style 1 - flared
      dwBulbQuick = LBT_INCANGLOBE60;
      pSize.p[0] = pSize.p[1] = CM_INCANGLOBEDIAMETER * 3;
      pSize.p[2] = CM_INCANGLOBELENGTH * 2;
      break;
   case LST_INCANSPOT2:       // style 2- angled
      dwBulbQuick = LBT_INCANGLOBE60;
      pSize.p[0] = pSize.p[1] = CM_INCANGLOBEDIAMETER * 2;
      pSize.p[2] = CM_INCANGLOBELENGTH * 2;
      break;
   case LST_INCANSPOT3:       // style 3 - larger at one end
      dwBulbQuick = LBT_INCANGLOBE60;
      pSize.p[0] = pSize.p[1] = CM_INCANGLOBEDIAMETER * 1.5;
      pSize.p[2] = CM_INCANGLOBELENGTH * 1.8;
      break;
   case LST_INCANSPOT4:       // cylinder
      dwBulbQuick = LBT_INCANGLOBE60;
      pSize.p[0] = pSize.p[1] = CM_INCANGLOBEDIAMETER * 1.2;
      pSize.p[2] = CM_INCANGLOBELENGTH * 1.5;
      break;
   case LST_INCANSPOT5:       // hemi-sphere
      dwBulbQuick = LBT_INCANGLOBE60;
      pSize.p[0] = pSize.p[1] = CM_INCANGLOBELENGTH * 2;
      pSize.p[2] = CM_INCANGLOBELENGTH;
      break;
   case LST_INCANSPOT6:       // half cylinder
      dwBulbQuick = LBT_INCANGLOBE60;
      pSize.p[0] = CM_INCANGLOBEDIAMETER * 2;
      pSize.p[2] = pSize.p[0] / 2;
      pSize.p[1] = CM_INCANGLOBELENGTH * 2;
      break;
   case LST_FLOUROSPOT1:
   case LST_FLOUROSPOT2LID:
   case LST_FLOUROSPOT2:
      dwBulbQuick = LBT_FLOURO8;
      pSize.p[0] = CM_FLOURO8DIAMETER * 4;
      pSize.p[2] = pSize.p[0] / 2;
      pSize.p[1] = CM_FLOURO8LENGTH * 1.2;
      break;

   case LST_LAVALAMP:
      dwBulbQuick = LBT_INCANGLOBE60;
      pSize.p[0] = pSize.p[1] =.15;
      pSize.p[2] = .3;
      break;

   case LST_SODIUMSPOT1:       // hemi-sphere
      dwBulbQuick = LBT_SODIUMGLOBE;
      pSize.p[0] = pSize.p[1] = CM_SODIUMGLOBELENGTH * 2;
      pSize.p[2] = CM_SODIUMGLOBELENGTH;
      break;
   case LST_SODIUMSPOT2:       // half cylinder
      dwBulbQuick = LBT_SODIUMGLOBE;
      pSize.p[0] = CM_SODIUMGLOBEDIAMETER * 2;
      pSize.p[2] = pSize.p[0] / 2;
      pSize.p[1] = CM_SODIUMGLOBELENGTH * 2;
      break;

   case LST_HALOGEN8BOX:       // box around the light
   case LST_HALOGEN8BOXLID:       // box around the light
   case LST_HALOGEN8CYLINDER:       // cylinder around the light
   case LST_HALOGEN8CYLINDERLID:       // cylinder around the light
      dwBulbQuick = LBT_HALOBULB8;
      pSize.p[0] = pSize.p[1] = .075;
      pSize.p[2] = .1;
      break;
   case LST_HALOGEN8BULB:       // bulb around the light
   case LST_HALOGEN8BULBLID:       // bulb around the light
      dwBulbQuick = LBT_HALOBULB8;
      pSize.p[0] = pSize.p[1] = .1;
      pSize.p[2] = .1;
      break;

   case LST_HALOGENFAKECANDLE:
      dwBulbQuick = LBT_HALOBULB8;
      pSize.p[0] = pSize.p[1] = .025;
      pSize.p[2] = .25;
      break;

   case LST_HALOGENBOWL:
      dwBulbQuick = LBT_HALOTUBE150;
      pSize.p[0] = pSize.p[1] = .4;
      pSize.p[2] = .1;
      break;

   case LST_ROUNDDIAGCLOTH2:       // round, linediag2, cloth, incandescent
   case LST_ROUNDDIAGCLOTH23:       // round, linediag23, cloth, incandecsent
   case LST_ROUNDDIAGSTRAIGHT:       // round, line, cloth, incandecsent
   case LST_SQUAREDIAGCLOTH2:       // square, linediag2, cloth, incandescent
   case LST_SQUAREDIAGCLOTH23:       // square, linediag23, cloth, incandecsent
   case LST_SQUAREDIAGSTRAIGHT:       // square, line, cloth, incandecsent
   case LST_FANDIAGCLOTH2:       // round fan, linediag2, cloth, incandescent
   case LST_FANDIAGCLOTH23:       // round fan, linediag23, cloth, incandecsent
   case LST_FANDIAGSTRAIGHT:       // round fan, line, cloth, incandecsent
   case LST_ROUNDDIAGCLOTH2DN:       // round, linediag2, cloth, incandescent
   case LST_ROUNDDIAGCLOTH23DN:       // round, linediag23, cloth, incandecsent
   case LST_ROUNDDIAGSTRAIGHTDN:       // round, line, cloth, incandecsent
   case LST_SQUAREDIAGCLOTH2DN:       // square, linediag2, cloth, incandescent
   case LST_SQUAREDIAGCLOTH23DN:       // square, linediag23, cloth, incandecsent
   case LST_SQUAREDIAGSTRAIGHTDN:       // square, line, cloth, incandecsent
   case LST_FANDIAGCLOTH2DN:       // round fan, linediag2, cloth, incandescent
   case LST_FANDIAGCLOTH23DN:       // round fan, linediag23, cloth, incandecsent
   case LST_FANDIAGSTRAIGHTDN:       // round fan, line, cloth, incandecsent
   case LST_ROUNDOLDFASHCLOTH:
   case LST_ROUNDOLDFASHCLOTHDN:
      dwBulbQuick = LBT_INCANGLOBE60;
      pSize.p[0] = pSize.p[1] = .4;
      pSize.p[2] = .4;
      break;

   }




   // set the light bulb
   m_fBulbWatts = HIWORD(dwBulbQuick);
   m_fBulbNoShadows = FALSE;
   m_dwBulbColor = dwBulbQuick & 0xff00;
   m_dwBulbShape = dwBulbQuick & 0xff;
   m_LightBulb.ColorSet (m_dwBulbColor);
   m_LightBulb.ShapeSet (m_dwBulbShape);
   m_LightBulb.WattsSet (m_fBulbWatts);
   m_LightBulb.NoShadowsSet (m_fBulbNoShadows);

   if (!SizeSet (&pSize))
      return FALSE;

   return TRUE;
}


/**********************************************************************************
CLightShade::SizeSet - Sets the lamp shade size.

inputs
   PCPoint        pSize - X, Y, and Z (depth) size
returns
   BOOL - TRUE if success
*/
BOOL CLightShade::SizeSet (PCPoint pSize)
{
   m_pSize.Copy (pSize);

   // set some defaults
   m_pBulbLoc.Zero();
   m_pBulbLoc.p[2] = m_pSize.p[2] / 2; // half way
   m_pBulbDir.Zero();
   m_pBulbDir.p[2] = 1;
   m_pBulbLeft.Zero();
   m_pBulbLeft.p[0] = 1;
   m_pLightVec.Zero();
   m_pLightVec.p[2] = 1;

   BOOL  fCenterBulb;
   fCenterBulb = TRUE;

   m_afLightAmt[0] = m_afLightAmt[1] = .0;
   m_afLightAmt[2] = 1;

   DWORD i, j;
   for (i = 0; i < 3; i++) for (j = 0; j < 3; j++)
      m_awLightColor[i][j] = 0xffff;

   memset (m_apLightDirect, 0 ,sizeof(m_apLightDirect));
   memset (m_afShadeShow, 0, sizeof(m_afShadeShow));
   m_adwShadeProfile[0] = m_adwShadeProfile[1] = RPROF_LINEVERT;
   m_adwShadeRev[0] = m_adwShadeRev[1] = RREV_CIRCLE;
   memset (m_apShadeBottom, 0 , sizeof(m_apShadeBottom));
   m_apShadeSize[0].Copy (&m_pSize);
   m_apShadeSize[1].Copy (&m_pSize);
   m_apShadeAroundVec[0].Copy (&m_pLightVec);
   m_apShadeAroundVec[1].Copy (&m_pLightVec);
   m_apShadeLeftVec[0].Zero();
   m_apShadeLeftVec[0].p[0] = 1;
   m_apShadeLeftVec[1].Copy (&m_apShadeLeftVec[0]);
   m_adwShadeMaterial[0] = m_adwShadeMaterial[1] = 0;
   m_afShadeAffectsOmni[0] = TRUE;
   m_afShadeAffectsOmni[1] = FALSE;

   fp fAmount;
   DWORD dwDir;

   switch (m_dwType) {
   case LST_BULBINCANGLOBE:
   case LST_BULBINCANSPOT:
   case LST_BULBHALOGENSPOT:
   case LST_BULBHALOGENBULB:
   case LST_BULBHALOGENTUBE:
   case LST_BULBFLOURO8:
   case LST_BULBFLOURO18:
   case LST_BULBFLOURO36:
   case LST_BULBFLOUROROUND:
   case LST_BULBFLOUROGLOBE:
   case LST_BULBSODIUMGLOBE:
      fCenterBulb = FALSE;
      m_pBulbLoc.Zero();
      break;

   case LST_FIRECANDLE1:      // candle with holder
   case LST_FIRECANDLE2:      // candle without holder, but thicker
      m_pBulbLoc.Zero();
      m_pBulbLoc.p[2] = m_pSize.p[2] + .02;

      m_afShadeAffectsOmni[0] = FALSE;
      m_afShadeShow[0] = TRUE;
      m_adwShadeProfile[0] = RPROF_C;

      if (m_dwType != LST_FIRECANDLE2) {
         m_afShadeShow[1] = TRUE;
         m_adwShadeProfile[1] = PRROF_CIRCLEHEMI;
         m_apShadeSize[1].Copy (&m_pSize);
         m_apShadeSize[1].Scale (10);
         m_apShadeSize[1].p[2] = max(m_apShadeSize[0].p[0], m_apShadeSize[0].p[1]) / 10;
      }
      break;

   case LST_FIRETORCH:      // torch
      m_pBulbLoc.Zero();
      m_pBulbLoc.p[2] = m_pSize.p[2] + .02 * 4.0;

      m_afShadeAffectsOmni[0] = FALSE;
      m_afShadeShow[0] = TRUE;
      m_adwShadeProfile[0] = RPROF_LDIAG;
      break;

   case LST_FIREBOWL:      // bowl with fire in it
      m_pBulbLoc.Zero();
      m_pBulbLoc.p[2] = .02 * 8.0;

      m_afShadeAffectsOmni[0] = FALSE;
      m_afShadeShow[0] = TRUE;
      m_adwShadeProfile[0] = PRROF_CIRCLEHEMI;
      break;

   case LST_FIRELANTERNHUNG:      // hanging lantern
      m_pBulbLoc.Zero();
      m_pBulbLoc.p[2] = m_pSize.p[2] * .66;

      m_afShadeShow[0] = TRUE;
      m_adwShadeProfile[0] = RPROF_LINECONE;
      m_adwShadeRev[0] = RREV_SQUARE;
      m_apShadeBottom[0].Zero();
      m_apShadeAroundVec[0].Zero();
      m_apShadeAroundVec[0].p[2] = 1;
      m_afShadeAffectsOmni[0] = FALSE;
      m_apShadeSize[0].p[2] = m_pSize.p[2] / 2;

      m_afShadeShow[1] = TRUE;
      m_adwShadeProfile[1] = RPROF_L;
      m_adwShadeRev[1] = RREV_SQUARE;
      m_apShadeBottom[1].Zero();
      m_apShadeBottom[1].p[2] = m_pSize.p[2];
      m_apShadeSize[1].p[2] = m_pSize.p[2] / 6;
      m_apShadeAroundVec[1].Zero();
      m_apShadeAroundVec[1].p[2] = -1;
      m_afShadeAffectsOmni[1] = FALSE;
      break;

   case LST_FLAMECANDLE:      // candle flame by itself
      m_pBulbLoc.Zero();
      m_pBulbLoc.p[2] = m_pSize.p[2] + .02;
      break;
   case LST_FLAMETORCH:      // torch flame by itself
      m_pBulbLoc.Zero();
      m_pBulbLoc.p[2] = m_pSize.p[2] + .02 * 4.0;
      break;
   case LST_FLAMEFIRE:      // fire flame by itself
      m_pBulbLoc.Zero();
      m_pBulbLoc.p[2] = m_pSize.p[2] + .02 * 8.0;
      break;

   case LST_HALOGENFAKECANDLE:
      m_pBulbLoc.Zero();
      m_pBulbLoc.p[2] = m_pSize.p[2] + .02;

      m_afShadeShow[0] = TRUE;
      m_adwShadeProfile[0] = PRROF_CIRCLEHEMI;
      m_apShadeSize[0].Copy (&m_pSize);
      m_apShadeSize[0].Scale (5);
      m_apShadeSize[0].p[2] = max(m_apShadeSize[0].p[0], m_apShadeSize[0].p[1]) / 10;
      m_afShadeAffectsOmni[0] = FALSE;

      m_afShadeShow[1] = TRUE;
      m_adwShadeProfile[1] = RPROF_L;
      break;

   case LST_SKYLIGHTDIFFUSERROUND:
   case LST_SKYLIGHTDIFFUSERSQUARE:
      // set some defaults
      m_pBulbLoc.Zero();
      m_pBulbLoc.p[2] = m_pSize.p[2] + .05;  // BUGFIX - Was .01

      m_afLightAmt[0] = .9;
      m_afLightAmt[1] = 0;
      m_afLightAmt[2] = .1;

      // BUGFIX - light was only 90 degrees. Up to 170
      m_apLightDirect[0].h = 1;
      m_apLightDirect[0].v = .1; // diffuse over 90 degress
      
      m_afShadeShow[0] = TRUE;
      m_adwShadeProfile[0] = RPROF_L;
      m_adwShadeRev[0] = (m_dwType == LST_SKYLIGHTDIFFUSERROUND) ? RREV_CIRCLE : RREV_SQUARE;
      m_apShadeBottom[0].Zero();
      m_apShadeBottom[0].p[2] = m_pSize.p[2];
      m_apShadeAroundVec[0].Scale (-1);
      m_adwShadeMaterial[0] = 2;
      m_afShadeAffectsOmni[0] = FALSE;

      m_afShadeShow[1] = TRUE;
      m_adwShadeProfile[1] = RPROF_RINGWIDE;
      m_adwShadeRev[1] = m_adwShadeRev[0];
      m_apShadeBottom[1].Zero();
      m_apShadeBottom[1].p[2] = .001;  // just above
      m_adwShadeMaterial[1] = 0;
      m_afShadeAffectsOmni[1] = FALSE;
      break;

   case LST_INCANSCONCECONE:      // cone with light going up
   case LST_INCANSCONCEHEMISPHERE:      // hemisphere with light goingup
   case LST_INCANSCONELOOP:      // loop with light going up and down
      m_pBulbLoc.Zero();
      m_pBulbLoc.p[2] = m_pSize.p[2] / 6; // close to the wall
      m_pLightVec.Zero();
      m_pLightVec.p[1] = 1;

      switch (m_dwType) {
      case LST_INCANSCONCECONE:      // cone with light going up
      case LST_INCANSCONCEHEMISPHERE:      // hemisphere with light goingup
         m_afLightAmt[0] = .5;
         m_afLightAmt[1] = .0;
         m_afLightAmt[2] = .5;
         m_apLightDirect[0].v = m_pSize.p[1] / 3;
         break;
      case LST_INCANSCONELOOP:      // loop with light going up and down
         m_afLightAmt[0] = m_afLightAmt[1] = .33;
         m_afLightAmt[2] = .33;
         m_apLightDirect[0].v = m_pSize.p[1] / 2;
         break;
      }

      m_apLightDirect[0].h = max(m_pSize.p[0],m_pSize.p[2]*2);
      m_apLightDirect[1] = m_apLightDirect[0];

      m_afShadeShow[0] = 1;
      switch (m_dwType) {
      case LST_INCANSCONCECONE:      // cone with light going up
         m_adwShadeProfile[0] = RPROF_LINECONE;
         break;
      case LST_INCANSCONELOOP:      // loop with light going up and down
         m_adwShadeProfile[0] = RPROF_LINEVERT;
         break;
      case LST_INCANSCONCEHEMISPHERE:      // hemisphere with light goingup
         m_adwShadeProfile[0] = PRROF_CIRCLEHEMI;
         break;
      }
      
      m_adwShadeRev[0] = RREV_CIRCLEHEMI;
      m_apShadeBottom[0].Zero();
      m_apShadeBottom[0].p[1] = -m_pSize.p[1] * ((m_dwType == LST_INCANSCONELOOP) ? .5 : (2.0 / 3.0));
      m_apShadeSize[0].Zero();
      m_apShadeSize[0].p[0] = m_pSize.p[0];
      m_apShadeSize[0].p[1] = m_pSize.p[2] * 2;
      m_apShadeSize[0].p[2] = m_pSize.p[1];
      m_apShadeAroundVec[0].Zero();
      m_apShadeAroundVec[0].p[1] = 1;
      m_apShadeLeftVec[0].Zero();
      m_apShadeLeftVec[0].p[0] = -1;
      m_adwShadeMaterial[0] = 2;
      m_afShadeAffectsOmni[0] = TRUE;
      break;

   case LST_INCANBULBSOCKET:
      m_pBulbLoc.Zero();
      m_pBulbLoc.p[2] = m_pSize.p[2] + CM_INCANGLOBELENGTH / 3; // half way

      m_afShadeShow[0] = TRUE;
      m_adwShadeProfile[0] = RPROF_L;
      m_adwShadeMaterial[0] = 0;
      m_afShadeAffectsOmni[0] = FALSE;
      break;

   case LST_INCANSPOTSOCKET:
      m_pBulbLoc.Zero();
      m_pBulbLoc.p[2] = m_pSize.p[2] + CM_INCANSPOTLENGTH / 4; // half way

      m_afShadeShow[0] = TRUE;
      m_adwShadeProfile[0] = RPROF_LDIAG;
      m_adwShadeMaterial[0] = 0;
      m_afShadeAffectsOmni[0] = FALSE;
      break;

   case LST_FLOURO8W1:      // 1 8w flouro
   case LST_FLOURO18W1:      // 1 18w flouro
   case LST_FLOURO18W2:      // 2 18w flouro
   case LST_FLOURO36W1:      // 1 36w flouro
   case LST_FLOURO36W2:      // 2 36w flouro
   case LST_FLOURO8W1CURVED:      // 1 8w flouro
   case LST_FLOURO18W1CURVED:      // 1 18w flouro
   case LST_FLOURO18W2CURVED:      // 2 18w flouro
   case LST_FLOURO36W1CURVED:      // 1 36w flouro
   case LST_FLOURO36W2CURVED:      // 2 36w flouro
   case LST_FLOURO36W4:      // 4 36w flouro in drop-down commerical ceiling
      m_pBulbLoc.p[2] = .01; // part way
      m_afShadeShow[0] = TRUE;
      m_adwShadeProfile[0] = RPROF_C;

      switch (m_dwType) {
      case LST_FLOURO8W1CURVED:      // 1 8w flouro
      case LST_FLOURO18W1CURVED:      // 1 18w flouro
      case LST_FLOURO18W2CURVED:      // 2 18w flouro
      case LST_FLOURO36W1CURVED:      // 1 36w flouro
      case LST_FLOURO36W2CURVED:      // 2 36w flouro
         m_adwShadeRev[0] = RREV_CIRCLEHEMI;
         break;
      default:
         m_adwShadeRev[0] = RREV_SQUAREHALF;
         break;
      }

      m_apShadeBottom[0].Zero();
      m_apShadeAroundVec[0].Zero();
      m_apShadeLeftVec[0].Zero();
      m_apShadeBottom[0].p[0] = -m_pSize.p[0] / 2.0;
      m_apShadeAroundVec[0].p[0] = 1;
      m_apShadeLeftVec[0].p[1] = 1;

      m_apShadeSize[0].p[0] = m_pSize.p[1];
      m_apShadeSize[0].p[1] = m_pSize.p[2] * 2;
      m_apShadeSize[0].p[2] = m_pSize.p[0];
      m_adwShadeMaterial[0] = 2;
      m_afShadeAffectsOmni[0] = FALSE;

      // outline
      if (m_dwType == LST_FLOURO36W4) {
         // NOTE - Bulb location doesnt matter with w4 since not acutally drawing them
         m_afShadeShow[1] = TRUE;
         m_adwShadeProfile[1] = RPROF_RING;
         m_adwShadeRev[1] = RREV_SQUARE;
         m_apShadeBottom[1].p[2] = .001;  // just above
         m_apShadeSize[1].Copy (&m_pSize);
         m_adwShadeMaterial[1] = 0;
         m_afShadeAffectsOmni[1] = FALSE;
      }
      break;

   case LST_INCANCEILSQUARE:      // square profile, on the ceiling
   case LST_INCANCEILROUND:      // roun, on ceiling
   case LST_INCANCEILHEMISPHERE:      // hemisphere profile, on the ceiling
   case LST_INCANCEILCONE:      // cone, on the ceiling
   case LST_INCANCEILSPOT:      // embedded in ceiling as spotlight
   case LST_HALOGENCEILSPOT:
      switch (m_dwType) {
      case LST_HALOGENCEILSPOT:
      case LST_INCANCEILSPOT:
         m_fBulbDraw = FALSE;
         m_pBulbLoc.p[2] = .02;   // just in front so wont get clipped against ceiling
         // BUGFIX - Was thism_pBulbLoc.p[2] = -CM_HALOGENSPOTLENGTH/2;
         // BUGFIX - Was m_pBulbLoc.p[2] = -CM_INCANGLOBELENGTH/2;
         break;
      default:
         m_pBulbLoc.p[2] = m_pSize.p[2] / 4;
         break;
      }
      m_afLightAmt[0] = m_afLightAmt[1] = .0;
      m_afLightAmt[2] = 1;

      m_afShadeShow[0] = TRUE;
      switch (m_dwType) {
      case LST_INCANCEILSQUARE:      // square profile, on the ceiling
         m_adwShadeProfile[0] = RPROF_L;
         m_adwShadeRev[0] = RREV_SQUARE;
         break;
      case LST_INCANCEILROUND:      // roun, on ceiling
         m_adwShadeProfile[0] = RPROF_L;
         m_adwShadeRev[0] = RREV_CIRCLE;
         break;
      case LST_HALOGENCEILSPOT:
      case LST_INCANCEILSPOT:      // embedded in ceiling as spotlight
         m_adwShadeProfile[0] = RPROF_L;
         m_adwShadeRev[0] = RREV_CIRCLE;
         m_afLightAmt[0] = 1;
         m_afLightAmt[2] = 0;
         m_apLightDirect[0].h = 1;
         m_apLightDirect[0].v = 1;  // so get about 45 degree
         // BUGFIX - Was m_apLightDirect[0].h = max(m_pSize.p[0],m_pSize.p[1]);
         // BUGFIX - Was m_apLightDirect[0].v = m_pSize.p[2] - m_pBulbLoc.p[2]; // distnace
         break;
      case LST_INCANCEILHEMISPHERE:      // hemisphere profile, on the ceiling
         m_adwShadeProfile[0] = PRROF_CIRCLEHEMI;
         m_adwShadeRev[0] = RREV_CIRCLE;
         break;
      case LST_INCANCEILCONE:      // cone, on the ceiling
         m_adwShadeProfile[0] = RPROF_LINECONE;
         m_adwShadeRev[0] = RREV_CIRCLE;
         break;
      }

      m_apShadeBottom[0].Zero();
      m_apShadeBottom[0].p[2] = m_pSize.p[2];
      m_apShadeAroundVec[0].Scale (-1);
      m_adwShadeMaterial[0] = 2;
      m_afShadeAffectsOmni[0] = FALSE;

      m_afShadeShow[1] = TRUE;
      m_adwShadeProfile[1] = RPROF_RINGWIDE;
      switch (m_dwType) {
      case LST_INCANCEILSQUARE:      // square profile, on the ceiling
         m_adwShadeRev[1] = RREV_SQUARE;
         break;
      default:
      case LST_INCANCEILROUND:      // roun, on ceiling
      case LST_HALOGENCEILSPOT:
      case LST_INCANCEILSPOT:      // embedded in ceiling as spotlight
      case LST_INCANCEILHEMISPHERE:      // hemisphere profile, on the ceiling
      case LST_INCANCEILCONE:      // cone, on the ceiling
         m_adwShadeRev[1] = RREV_CIRCLE;
         break;
      }
      m_apShadeBottom[1].Zero();
      m_apShadeBottom[1].p[2] = .001;  // just above
      m_adwShadeMaterial[1] = 0;
      m_afShadeAffectsOmni[1] = FALSE;
      break;

   case LST_FLOUROROUND:      // round flouro light in hemi-sphere
      m_pBulbLoc.p[2] = m_pSize.p[2] / 2;
      m_afShadeShow[0] = TRUE;
      m_adwShadeProfile[0] = RPROF_L;
      m_adwShadeRev[0] = RREV_CIRCLE;
      m_apShadeBottom[0].Zero();
      m_apShadeBottom[0].p[2] = m_pSize.p[2];
      m_apShadeAroundVec[0].Scale (-1);
      m_adwShadeMaterial[0] = 2;
      m_afShadeAffectsOmni[0] = FALSE;

      m_afShadeShow[1] = TRUE;
      m_adwShadeProfile[1] = RPROF_RINGWIDE;
      m_adwShadeRev[1] = RREV_CIRCLE;
      m_apShadeBottom[1].Zero();
      m_apShadeBottom[1].p[2] = .001;  // just above
      m_adwShadeMaterial[1] = 0;
      m_afShadeAffectsOmni[1] = FALSE;
      break;

   case LST_LAVALAMP:
      m_pBulbLoc.Zero();

      DWORD i, j;
      for (i = 0; i < 3; i++) for (j = 0; j < 3; j++)
         m_awLightColor[i][j] = (j == 0) ? 0xffff : 0;

      m_afShadeShow[0] = m_afShadeShow[1] = TRUE;
      m_adwShadeProfile[1] = RPROF_L;
      m_adwShadeProfile[0] = RPROF_LDIAG;
      m_adwShadeRev[0] = m_adwShadeRev[1] = RREV_CIRCLE;
      m_apShadeBottom[0].p[2] = m_pSize.p[2];
      m_apShadeBottom[1].p[2] = m_pSize.p[2] * 1.01;  // slightly higer
      m_apShadeSize[1].Scale (.67);
      m_apShadeSize[1].p[2] /= 16.0; // cap
      m_apShadeAroundVec[0].Scale(-1);
      m_apShadeAroundVec[1].Scale(-1);
      m_adwShadeMaterial[0] = 2;
      m_adwShadeMaterial[1] = 0;
      m_afShadeAffectsOmni[0] = m_afShadeAffectsOmni[1] = FALSE;
      break;

   case LST_INCANSPOT5:  // hemisphere
   case LST_SODIUMSPOT1:
      m_pBulbLoc.Zero();
      m_pBulbLoc.p[2] = 0;
      m_pBulbLoc.p[0] = m_pSize.p[0] / 2.0;
      m_pBulbDir.Zero();
      m_pBulbDir.p[0] = 1;
      m_pBulbLeft.Zero();
      m_pBulbLeft.p[1] = 1;

      m_afLightAmt[0] = .7;
      m_afLightAmt[1] = 0;
      m_afLightAmt[2] = .3;

      m_apLightDirect[0].h = max(m_apShadeSize[0].p[0],m_apShadeSize[0].p[1]);
      m_apLightDirect[0].v = m_apShadeSize[0].p[2] / 3.0; // distnace

      m_afShadeShow[0] = TRUE;

      m_adwShadeProfile[0] = PRROF_CIRCLEHEMI;
      
      m_apShadeBottom[0].Zero();
      m_apShadeBottom[0].p[2] = -m_pSize.p[2] *2.0 / 3.0;
      m_apShadeBottom[0].p[0] = m_pSize.p[0] / 2.0;
      m_apShadeAroundVec[0].Copy (&m_pLightVec);
      m_adwShadeMaterial[0] = (m_dwType == LST_SODIUMSPOT1) ? 0 : 2;
      break;

   case LST_INCANSPOT6:  // half cylinder
   case LST_SODIUMSPOT2:
   case LST_FLOUROSPOT1:   // attached on long end
   case LST_FLOUROSPOT2LID:
   case LST_FLOUROSPOT2:   // attached on short end
      m_pBulbLoc.Zero();
      if ((m_dwType == LST_FLOUROSPOT2) || (m_dwType == LST_FLOUROSPOT2LID)) {
         m_pBulbLoc.p[2] = m_pSize.p[2] / 3.0;
         m_pBulbLoc.p[0] = m_pSize.p[0] / 2;
      }
      else {
         m_pBulbLoc.p[2] = 0;
         m_pBulbLoc.p[0] = m_pSize.p[1] / 2.0;
      }
      m_pBulbDir.Zero();
      m_pBulbLeft.Zero();
      switch (m_dwType) {
      default:
      case LST_INCANSPOT6:  // half cylinder
      case LST_SODIUMSPOT2:
         m_pBulbDir.p[0] = 1;
         m_pBulbLeft.p[1] = 1;
         break;
      case LST_FLOUROSPOT1:   // attached on long end
         m_pBulbDir.p[2] = 1;
         m_pBulbLeft.p[0] = 1;
         break;
      case LST_FLOUROSPOT2:   // attached on short end
      case LST_FLOUROSPOT2LID:
         m_pBulbDir.p[2] = 1;
         m_pBulbLeft.p[1] = 1;
         break;
      }

      m_afLightAmt[0] = .7;
      m_afLightAmt[1] = 0;
      m_afLightAmt[2] = .3;

      m_apLightDirect[0].h = max(m_pSize.p[0],m_pSize.p[1]);
      m_apLightDirect[0].v = m_pSize.p[2] / 3.0; // distnace

      m_afShadeShow[0] = TRUE;

      m_adwShadeProfile[0] = RPROF_C;
      m_adwShadeRev[0] = RREV_CIRCLEHEMI;
      
      m_apShadeBottom[0].Zero();
      m_apShadeAroundVec[0].Zero();
      m_apShadeLeftVec[0].Zero();
      if ((m_dwType == LST_FLOUROSPOT2) || (m_dwType == LST_FLOUROSPOT2LID)) {
         m_apShadeBottom[0].p[0] = m_pSize.p[0] / 2;
         m_apShadeBottom[0].p[1] = -m_pSize.p[1] / 2;
         m_apShadeBottom[0].p[2] = m_pSize.p[2] * 2.0 / 3.0;
         m_apShadeAroundVec[0].p[1] = 1;
         m_apShadeLeftVec[0].p[0] = 1;
      }
      else {
         m_apShadeBottom[0].p[2] = m_pSize.p[2] / 3.0;
         m_apShadeAroundVec[0].p[0] = 1;
         m_apShadeLeftVec[0].p[1] = -1;
      }
      m_apShadeSize[0].p[0] = m_pSize.p[0];
      m_apShadeSize[0].p[1] = m_pSize.p[2] * 2;
      m_apShadeSize[0].p[2] = m_pSize.p[1];
      m_adwShadeMaterial[0] = 0;

      if (m_dwType == LST_FLOUROSPOT2LID) {
         // draw the diffuser
         m_afShadeShow[1] = TRUE;
         m_adwShadeProfile[1] = m_adwShadeProfile[0];
         m_adwShadeRev[1] = m_adwShadeRev[0];
         m_apShadeBottom[1].Copy (&m_apShadeBottom[0]);
         m_apShadeAroundVec[1].Copy (&m_apShadeAroundVec[0]);
         m_apShadeLeftVec[1].Copy (&m_apShadeLeftVec[0]);
         m_apShadeLeftVec[1].Scale (-1);
         m_apShadeSize[1].Copy (&m_apShadeSize[0]);
         m_adwShadeMaterial[1] = 2;
      }
      break;

   case LST_HALOGENTUBESPOT:
      m_pBulbLoc.Zero();
      m_pBulbLoc.p[2] = m_pSize.p[2] / 3.0;

      m_afLightAmt[0] = 1;
      m_afLightAmt[1] = 0;
      m_afLightAmt[2] = 0;

      m_apLightDirect[0].h = max(m_apShadeSize[0].p[0],m_apShadeSize[0].p[1]);
      m_apLightDirect[0].v = m_apShadeSize[0].p[2] * 2.0 / 3.0; // distnace

      m_afShadeShow[0] = TRUE;

      m_adwShadeProfile[0] = RPROF_LDIAG;
      m_adwShadeRev[0] = RREV_SQUARE;
      m_adwShadeMaterial[0] = 0;
      m_afShadeAffectsOmni[0] = FALSE;
      break;

   case LST_INCANSPOT1:       // style 1 - flared
   case LST_INCANSPOT2:       // style 2- angled
   case LST_INCANSPOT3:       // style 3 - larger at one end
   case LST_INCANSPOT4:       // cylinder
   case LST_HALOGENSPOT1:       // style 1 - flared
   case LST_HALOGENSPOT2:       // style 2- angled
   case LST_HALOGENSPOT3:       // style 3 - larger at one end
   case LST_HALOGENSPOT4:       // cylinder
      m_pBulbLoc.Zero();
      m_pBulbLoc.p[2] = m_pSize.p[2] / 3.0;
      m_pBulbLoc.p[0] = m_pSize.p[0] / 3.0;
      m_pBulbDir.Zero();
      m_pBulbDir.p[2] = 1;
      m_pBulbLeft.Zero();
      m_pBulbLeft.p[0] = 1;
      m_pLightVec.Zero();
      m_pLightVec.p[2] = 1;

      m_afLightAmt[0] = 1;
      m_afLightAmt[1] = 0;
      m_afLightAmt[2] = 0;

      m_apLightDirect[0].h = max(m_apShadeSize[0].p[0],m_apShadeSize[0].p[1]);
      m_apLightDirect[0].v = m_apShadeSize[0].p[2] / 3.0; // distnace

      m_afShadeShow[0] = TRUE;

      switch (m_dwType) {
      default:
      case LST_INCANSPOT1:       // style 1 - flared
      case LST_HALOGENSPOT1:       // style 1 - flared
         m_adwShadeProfile[0] = RPROF_SHADEDIRECTIONAL;
         break;
      case LST_INCANSPOT2:       // style 2- angled
      case LST_HALOGENSPOT2:       // style 2- angled
         m_adwShadeProfile[0] = RPROF_LDIAG;
         break;
      case LST_INCANSPOT3:       // style 3 - larger at one end
      case LST_HALOGENSPOT3:       // style 3 - larger at one end
         m_adwShadeProfile[0] = RPROF_SHADEDIRECTIONAL2;
         break;
      case LST_INCANSPOT4:       // cylinder
      case LST_HALOGENSPOT4:       // cylinder
         m_adwShadeProfile[0] = RPROF_L;
         break;
      }
      
      m_apShadeBottom[0].Zero();
      m_apShadeBottom[0].p[2] = -m_pSize.p[2] / 3.0;
      m_apShadeBottom[0].p[0] = m_pSize.p[0] / 3.0;
      m_apShadeSize[0].Copy (&m_pSize);
      m_apShadeAroundVec[0].Copy (&m_pLightVec);
      m_apShadeLeftVec[0].Zero();
      m_apShadeLeftVec[0].p[0] = 1;
      m_adwShadeMaterial[0] = 0;
      m_afShadeAffectsOmni[0] = FALSE;
      break;

   case LST_GLASSINCANBOXROOF:       // rectangular with roof
   case LST_GLASSINCANCYLINDERROOF:       // cylinder with roof
   case LST_HALOGEN8BOXROOF:       // rectangular with roof
   case LST_HALOGEN8CYLINDERROOF:       // cylinder with roof
      m_afShadeAffectsOmni[0] = m_afShadeAffectsOmni[1] = FALSE;
      m_adwShadeProfile[0] = RPROF_LDIAG;
      m_adwShadeProfile[1] = RPROF_LINECONE;

      // rotation
      switch (m_dwType) {
      default:
      case LST_GLASSINCANCYLINDERROOF:       // cylinder with roof
      case LST_HALOGEN8CYLINDERROOF:       // cylinder with roof
         m_adwShadeRev[0] = RREV_CIRCLE;
         m_adwShadeRev[1] = RREV_CIRCLE;
         break;
      case LST_GLASSINCANBOXROOF:       // rectangular with roof
      case LST_HALOGEN8BOXROOF:       // rectangular with roof
         m_adwShadeRev[0] = RREV_SQUARE;
         m_adwShadeRev[1] = RREV_SQUARE;
         break;
      }

      // directionality
      m_afLightAmt[0] = m_afLightAmt[1] = 0;
      m_afLightAmt[2] = 1;

      m_afShadeShow[0] = TRUE;
      m_adwShadeMaterial[0] = 2;
      m_apShadeSize[0].Scale (.75);
      m_apShadeBottom[0].Zero();

      // lid?
      m_afShadeShow[1] = TRUE;
      m_adwShadeMaterial[1] = 0;
      m_apShadeSize[1].p[2] /= 4.0;
      m_apShadeAroundVec[1].Scale (-1); // flip direction
      m_apShadeBottom[1].Zero();
      m_apShadeBottom[1].p[2] = m_pSize.p[2];

      // bulb location in the middle
      m_pBulbLoc.p[2] = m_apShadeSize[0].p[2] / 2;
      break;



   case LST_GLASSINCANBULB:       // bulb around the light
   case LST_GLASSINCANBULBBBASE:
   case LST_GLASSINCANBOX:       // box around the light
   case LST_GLASSINCANCYLINDER:       // cylinder around the light
   case LST_GLASSINCANBULBLID:       // bulb around the light
   case LST_GLASSINCANBOXLID:       // box around the light
   case LST_GLASSINCANCYLINDERLID:       // cylinder around the light
   case LST_HALOGEN8BULB:       // bulb around the light
   case LST_HALOGEN8BOX:       // box around the light
   case LST_HALOGEN8CYLINDER:       // cylinder around the light
   case LST_HALOGEN8BULBLID:       // bulb around the light
   case LST_HALOGEN8BOXLID:       // box around the light
   case LST_HALOGEN8CYLINDERLID:       // cylinder around the light
      m_afShadeAffectsOmni[0] = m_afShadeAffectsOmni[1] = FALSE;
      switch (m_dwType) {
      default:
      case LST_GLASSINCANBULB:       // bulb around the light
      case LST_GLASSINCANBULBLID:       // bulb around the light
      case LST_HALOGEN8BULB:       // bulb around the light
      case LST_HALOGEN8BULBLID:       // bulb around the light
         m_adwShadeProfile[0] = RPROF_CIRCLE;
         m_adwShadeProfile[1] = PRROF_CIRCLEHEMI;
         break;
      case LST_GLASSINCANBULBBBASE:
         m_adwShadeProfile[0] = RPROF_CIRCLEOPEN;
         m_adwShadeProfile[1] = RPROF_L;
         break;
      case LST_GLASSINCANBOX:       // box around the light
      case LST_GLASSINCANCYLINDER:       // cylinder around the light
      case LST_HALOGEN8BOX:       // box around the light
      case LST_HALOGEN8CYLINDER:       // cylinder around the light
         m_adwShadeProfile[0] = RPROF_C;
         break;
      case LST_GLASSINCANBOXLID:       // box around the light
      case LST_GLASSINCANCYLINDERLID:       // cylinder around the light
      case LST_HALOGEN8BOXLID:       // box around the light
      case LST_HALOGEN8CYLINDERLID:       // cylinder around the light
         m_adwShadeProfile[0] = RPROF_L;
         m_adwShadeProfile[1] = RPROF_L;
         break;
      }

      // rotation
      switch (m_dwType) {
      default:
      case LST_GLASSINCANBULB:       // bulb around the light
      case LST_GLASSINCANBULBBBASE:
      case LST_GLASSINCANCYLINDER:       // cylinder around the light
      case LST_GLASSINCANBULBLID:       // bulb around the light
      case LST_GLASSINCANCYLINDERLID:       // cylinder around the light
      case LST_HALOGEN8BULB:       // bulb around the light
      case LST_HALOGEN8CYLINDER:       // cylinder around the light
      case LST_HALOGEN8BULBLID:       // bulb around the light
      case LST_HALOGEN8CYLINDERLID:       // cylinder around the light
         m_adwShadeRev[0] = RREV_CIRCLE;
         m_adwShadeRev[1] = RREV_CIRCLE;
         break;
      case LST_GLASSINCANBOX:       // box around the light
      case LST_GLASSINCANBOXLID:       // box around the light
      case LST_HALOGEN8BOX:       // box around the light
      case LST_HALOGEN8BOXLID:       // box around the light
         m_adwShadeRev[0] = RREV_SQUARE;
         m_adwShadeRev[1] = RREV_SQUARE;
         break;
      }

      // directionality
      m_afLightAmt[0] = m_afLightAmt[1] = 0;
      m_afLightAmt[2] = 1;

      m_afShadeShow[0] = TRUE;
      m_adwShadeMaterial[0] = 2;

      // lid?
      switch (m_dwType) {
      case LST_GLASSINCANBULBLID:       // bulb around the light
      case LST_GLASSINCANBOXLID:       // box around the light
      case LST_GLASSINCANCYLINDERLID:       // cylinder around the light
      case LST_HALOGEN8BULBLID:       // bulb around the light
      case LST_HALOGEN8BOXLID:       // box around the light
      case LST_HALOGEN8CYLINDERLID:       // cylinder around the light
         m_afShadeShow[1] = TRUE;
         m_adwShadeMaterial[1] = 0;
         m_apShadeSize[1].Scale (1.01);   // slightly larger
         switch (m_dwType) {
         default: // hemisphere
            m_apShadeSize[1].p[2] /= 2.0;
            break;
         case LST_GLASSINCANBOXLID:       // box around the light
         case LST_GLASSINCANCYLINDERLID:       // cylinder around the light
         case LST_HALOGEN8BOXLID:       // box around the light
         case LST_HALOGEN8CYLINDERLID:       // cylinder around the light
            m_apShadeSize[1].p[2] /= 3.0; // so doesn cover everything
            break;
         }
         m_apShadeAroundVec[1].Scale (-1); // flip direction
         m_apShadeBottom[1].Zero();
         m_apShadeBottom[1].p[2] = m_pSize.p[2] * 1.01;
         break;
      case LST_GLASSINCANBULBBBASE:
         m_afShadeShow[1] = TRUE;
         m_adwShadeMaterial[1] = 0;
         m_apShadeSize[1].Scale (.51);   // slightly larger
         m_apShadeSize[1].p[2] /= 10;
         m_apShadeBottom[1].Zero();
         break;
      default:
         // do nothing
         break;
      }

      // bottom and direction
      m_apShadeBottom[0].Zero();

      // bulb location in the middle
      m_pBulbLoc.p[2] = m_apShadeSize[0].p[2] / 2;
      break;

   case LST_HALOGENBOWL:
      m_adwShadeProfile[0] = RPROF_CIRCLEQUARTER;
      m_adwShadeRev[0] = RREV_CIRCLE;

      // directionality
      m_apLightDirect[0].h = max(m_apShadeSize[0].p[0],m_apShadeSize[0].p[1]);
      m_apLightDirect[0].v = m_apShadeSize[0].p[2] / 2.0; // distnace
      m_afLightAmt[0] = .7;
      m_afLightAmt[1] = 0;
      m_afLightAmt[2] = .3;
      dwDir = 1;
      m_afShadeShow[0] = TRUE;
      m_adwShadeMaterial[0] = 0;

      // bottom and direction
      m_apShadeBottom[0].Zero();

      // bulb location in the middle
      m_pBulbLoc.p[2] = m_apShadeSize[0].p[2] / 2;
      m_pBulbDir.Zero();
      m_pBulbDir.p[0] = 1;
      m_pBulbLeft.Zero();
      m_pBulbLeft.p[1] = 1;
      break;

   case LST_GLASSINCANROUNDCONEDN:
   case LST_GLASSINCANSQUARECONEDN:
   case LST_GLASSINCANHEXCONEDN:
   case LST_GLASSINCANROUNDHEMIDN:       // hemisphere
   case LST_GLASSINCANHEXHEMIDN:       // hexagon around, hemisphere shape
   case LST_GLASSINCANOCTHEMIDN:
   case LST_GLASSINCANROUNDCONE:
   case LST_GLASSINCANSQUARECONE:
   case LST_GLASSINCANHEXCONE:
   case LST_GLASSINCANROUNDHEMI:       // hemisphere
   case LST_GLASSINCANHEXHEMI:       // hexagon around, hemisphere shape
   case LST_GLASSINCANOCTHEMI:
   case LST_GLASSINCANOLDFASHIONED:
   case LST_GLASSINCANSQUAREOPENDN:
   case LST_GLASSINCANSQUAREOPEN:
   case LST_GLASSINCANROUNDEDDN:
   case LST_GLASSINCANROUNDED:
   case LST_INCANPOOLTABLE:
      switch (m_dwType) {
      default:
      case LST_GLASSINCANROUNDCONEDN:
      case LST_GLASSINCANSQUARECONEDN:
      case LST_GLASSINCANHEXCONEDN:
      case LST_GLASSINCANROUNDCONE:
      case LST_GLASSINCANSQUARECONE:
      case LST_GLASSINCANHEXCONE:
         m_adwShadeProfile[0] = RPROF_LINECONE;
         break;
      case LST_GLASSINCANROUNDHEMIDN:       // hemisphere
      case LST_GLASSINCANHEXHEMIDN:       // hexagon around, hemisphere shape
      case LST_GLASSINCANOCTHEMIDN:
      case LST_GLASSINCANROUNDHEMI:       // hemisphere
      case LST_GLASSINCANHEXHEMI:       // hexagon around, hemisphere shape
      case LST_GLASSINCANOCTHEMI:
         m_adwShadeProfile[0] = PRROF_CIRCLEHEMI;
         break;
      case LST_GLASSINCANOLDFASHIONED:
         m_adwShadeProfile[0] = RPROF_FANCYGLASSSHADE;
         break;
      case LST_GLASSINCANSQUAREOPENDN:
      case LST_GLASSINCANSQUAREOPEN:
         m_adwShadeProfile[0] = RPROF_LINEVERT;
         break;
      case LST_INCANPOOLTABLE:
         m_adwShadeProfile[0] = RPROF_LDIAG;
         break;
      case LST_GLASSINCANROUNDEDDN:
      case LST_GLASSINCANROUNDED:
         m_adwShadeProfile[0] = PPROF_CIRCLEQS;
         break;
      }

      // rotation
      switch (m_dwType) {
      default:
      case LST_GLASSINCANROUNDCONEDN:
      case LST_GLASSINCANROUNDHEMIDN:       // hemisphere
      case LST_GLASSINCANROUNDCONE:
      case LST_GLASSINCANROUNDHEMI:       // hemisphere
      case LST_GLASSINCANOLDFASHIONED:
      case LST_GLASSINCANROUNDEDDN:
      case LST_GLASSINCANROUNDED:
         m_adwShadeRev[0] = RREV_CIRCLE;
         break;
      case LST_GLASSINCANSQUARECONEDN:
      case LST_GLASSINCANSQUARECONE:
      case LST_GLASSINCANSQUAREOPENDN:
      case LST_GLASSINCANSQUAREOPEN:
      case LST_INCANPOOLTABLE:
         m_adwShadeRev[0] = RREV_SQUARE;
         break;
      case LST_GLASSINCANHEXCONEDN:
      case LST_GLASSINCANHEXHEMIDN:       // hexagon around, hemisphere shape
      case LST_GLASSINCANHEXCONE:
      case LST_GLASSINCANHEXHEMI:       // hexagon around, hemisphere shape
         m_adwShadeRev[0] = RREV_HEXAGON;
         break;
      case LST_GLASSINCANOCTHEMIDN:
      case LST_GLASSINCANOCTHEMI:
         m_adwShadeRev[0] = RREV_OCTAGON;
         break;

      }

      // directionality
      m_apLightDirect[0].h = max(m_apShadeSize[0].p[0],m_apShadeSize[0].p[1]);
      m_apLightDirect[0].v = m_apShadeSize[0].p[2] / 2.0; // distnace
      m_afLightAmt[0] = .7;
      m_afLightAmt[1] = 0;
      m_afLightAmt[2] = .3;
      switch (m_dwType) {
      case LST_GLASSINCANOLDFASHIONED:
         m_apLightDirect[0].h *= .4;
         m_apLightDirect[1] = m_apLightDirect[0];
         m_afLightAmt[0] = m_afLightAmt[1] = .35;
         break;
      case LST_GLASSINCANSQUAREOPEN:
      case LST_GLASSINCANSQUAREOPENDN:
         m_apLightDirect[1] = m_apLightDirect[0];
         m_afLightAmt[0] = m_afLightAmt[1] = .35;
         break;
      default:
         break;
         // do nothing
      }

      switch (m_dwType) {
      default:
      case LST_GLASSINCANROUNDCONE:
      case LST_GLASSINCANSQUARECONE:
      case LST_GLASSINCANHEXCONE:
      case LST_GLASSINCANROUNDHEMI:       // hemisphere
      case LST_GLASSINCANHEXHEMI:       // hexagon around, hemisphere shape
      case LST_GLASSINCANOCTHEMI:
      case LST_GLASSINCANOLDFASHIONED:
      case LST_GLASSINCANSQUAREOPEN:
      case LST_GLASSINCANROUNDED:
      case LST_INCANPOOLTABLE:
         dwDir = 1;  // z=1 direction opened up more
         break;
      case LST_GLASSINCANROUNDCONEDN:
      case LST_GLASSINCANSQUARECONEDN:
      case LST_GLASSINCANHEXCONEDN:
      case LST_GLASSINCANROUNDHEMIDN:       // hemisphere
      case LST_GLASSINCANHEXHEMIDN:       // hexagon around, hemisphere shape
      case LST_GLASSINCANOCTHEMIDN:
      case LST_GLASSINCANSQUAREOPENDN:
      case LST_GLASSINCANROUNDEDDN:
         dwDir = 0;  // z=0 direction opened up more
         break;
      }
      m_afShadeShow[0] = TRUE;
      m_adwShadeMaterial[0] = (m_dwType == LST_INCANPOOLTABLE) ? 0 : 2;

      // bottom and direction
      m_apShadeBottom[0].Zero();
      if (!dwDir) {
         m_apShadeBottom[0].p[2] = m_apShadeSize[0].p[2];
         m_pLightVec.Scale (-1);
         m_apShadeAroundVec[0].Scale (-1);
      }

      // bulb location in the middle
      m_pBulbLoc.p[2] = m_apShadeSize[0].p[2] / 2;
      break;

   default:
   case LST_ROUNDDIAGCLOTH2:       // round, linediag2, cloth, incandescent
   case LST_ROUNDDIAGCLOTH23:       // round, linediag23, cloth, incandecsent
   case LST_ROUNDDIAGSTRAIGHT:       // round, line, cloth, incandecsent
   case LST_SQUAREDIAGCLOTH2:       // square, linediag2, cloth, incandescent
   case LST_SQUAREDIAGCLOTH23:       // square, linediag23, cloth, incandecsent
   case LST_SQUAREDIAGSTRAIGHT:       // square, line, cloth, incandecsent
   case LST_FANDIAGCLOTH2:       // round fan, linediag2, cloth, incandescent
   case LST_FANDIAGCLOTH23:       // round fan, linediag23, cloth, incandecsent
   case LST_FANDIAGSTRAIGHT:       // round fan, line, cloth, incandecsent
   case LST_ROUNDDIAGCLOTH2DN:       // round, linediag2, cloth, incandescent
   case LST_ROUNDDIAGCLOTH23DN:       // round, linediag23, cloth, incandecsent
   case LST_ROUNDDIAGSTRAIGHTDN:       // round, line, cloth, incandecsent
   case LST_SQUAREDIAGCLOTH2DN:       // square, linediag2, cloth, incandescent
   case LST_SQUAREDIAGCLOTH23DN:       // square, linediag23, cloth, incandecsent
   case LST_SQUAREDIAGSTRAIGHTDN:       // square, line, cloth, incandecsent
   case LST_FANDIAGCLOTH2DN:       // round fan, linediag2, cloth, incandescent
   case LST_FANDIAGCLOTH23DN:       // round fan, linediag23, cloth, incandecsent
   case LST_FANDIAGSTRAIGHTDN:       // round fan, line, cloth, incandecsent
   case LST_ROUNDOLDFASHCLOTH:
   case LST_ROUNDOLDFASHCLOTHDN:
      // profile
      m_apLightDirect[0].h = m_apLightDirect[1].h = max(m_apShadeSize[0].p[0],m_apShadeSize[0].p[1]);
      switch (m_dwType) {
      default:
      case LST_ROUNDDIAGCLOTH2:       // round, linediag2, cloth, incandescent
      case LST_SQUAREDIAGCLOTH2:       // square, linediag2, cloth, incandescent
      case LST_FANDIAGCLOTH2:       // round fan, linediag2, cloth, incandescent
      case LST_ROUNDDIAGCLOTH2DN:       // round, linediag2, cloth, incandescent
      case LST_SQUAREDIAGCLOTH2DN:       // square, linediag2, cloth, incandescent
      case LST_FANDIAGCLOTH2DN:       // round fan, linediag2, cloth, incandescent
         m_adwShadeProfile[0] = RPROF_LINEDIAG2;
         fAmount = .5;
         break;
      case LST_ROUNDOLDFASHCLOTH:
      case LST_ROUNDOLDFASHCLOTHDN:
         m_adwShadeProfile[0] = RPROF_SHADECURVED;
         fAmount = .5;
         break;
      case LST_ROUNDDIAGCLOTH23:       // round, linediag23, cloth, incandecsent
      case LST_SQUAREDIAGCLOTH23:       // square, linediag23, cloth, incandecsent
      case LST_FANDIAGCLOTH23:       // round fan, linediag23, cloth, incandecsent
      case LST_ROUNDDIAGCLOTH23DN:       // round, linediag23, cloth, incandecsent
      case LST_SQUAREDIAGCLOTH23DN:       // square, linediag23, cloth, incandecsent
      case LST_FANDIAGCLOTH23DN:       // round fan, linediag23, cloth, incandecsent
         m_adwShadeProfile[0] = RPROF_LINEDIAG23;
         fAmount = 2.0 / 3.0;
         break;
      case LST_ROUNDDIAGSTRAIGHT:       // round, line, cloth, incandecsent
      case LST_SQUAREDIAGSTRAIGHT:       // square, line, cloth, incandecsent
      case LST_FANDIAGSTRAIGHT:       // round fan, line, cloth, incandecsent
      case LST_ROUNDDIAGSTRAIGHTDN:       // round, line, cloth, incandecsent
      case LST_SQUAREDIAGSTRAIGHTDN:       // square, line, cloth, incandecsent
      case LST_FANDIAGSTRAIGHTDN:       // round fan, line, cloth, incandecsent
         m_adwShadeProfile[0] = RPROF_LINEVERT;
         fAmount = 1;
         break;
      }

      // rotation
      switch (m_dwType) {
      default:
      case LST_ROUNDDIAGCLOTH2:       // round, linediag2, cloth, incandescent
      case LST_ROUNDDIAGCLOTH23:       // round, linediag23, cloth, incandecsent
      case LST_ROUNDDIAGSTRAIGHT:       // round, line, cloth, incandecsent
      case LST_ROUNDDIAGCLOTH2DN:       // round, linediag2, cloth, incandescent
      case LST_ROUNDDIAGCLOTH23DN:       // round, linediag23, cloth, incandecsent
      case LST_ROUNDDIAGSTRAIGHTDN:       // round, line, cloth, incandecsent
      case LST_ROUNDOLDFASHCLOTH:
      case LST_ROUNDOLDFASHCLOTHDN:
         m_adwShadeRev[0] = RREV_CIRCLE;
         break;
      case LST_SQUAREDIAGCLOTH2:       // square, linediag2, cloth, incandescent
      case LST_SQUAREDIAGCLOTH23:       // square, linediag23, cloth, incandecsent
      case LST_SQUAREDIAGSTRAIGHT:       // square, line, cloth, incandecsent
      case LST_SQUAREDIAGCLOTH2DN:       // square, linediag2, cloth, incandescent
      case LST_SQUAREDIAGCLOTH23DN:       // square, linediag23, cloth, incandecsent
      case LST_SQUAREDIAGSTRAIGHTDN:       // square, line, cloth, incandecsent
         m_adwShadeRev[0] = RREV_SQUARE;
         // Take out this scaling since causes problems with translucent surfaces
         //m_apLightDirect[0].h *= sqrt(2);
         //m_apLightDirect[1].h *= sqrt(2);
         break;
      case LST_FANDIAGCLOTH2:       // round fan, linediag2, cloth, incandescent
      case LST_FANDIAGCLOTH23:       // round fan, linediag23, cloth, incandecsent
      case LST_FANDIAGSTRAIGHT:       // round fan, line, cloth, incandecsent
      case LST_FANDIAGCLOTH2DN:       // round fan, linediag2, cloth, incandescent
      case LST_FANDIAGCLOTH23DN:       // round fan, linediag23, cloth, incandecsent
      case LST_FANDIAGSTRAIGHTDN:       // round fan, line, cloth, incandecsent
         m_adwShadeRev[0] = RREV_CIRCLEFAN;
         break;
      }

      // directionality
      m_apLightDirect[0].v = m_apLightDirect[1].v = m_apShadeSize[0].p[2] / 2.0; // distnace
      m_afLightAmt[0] = m_afLightAmt[1] = .4;  // note - ignoring the fact that smaller hole on one side
      m_afLightAmt[2] = .2;
      switch (m_dwType) {
      default:
      case LST_ROUNDDIAGCLOTH2:       // round, linediag2, cloth, incandescent
      case LST_ROUNDDIAGCLOTH23:       // round, linediag23, cloth, incandecsent
      case LST_ROUNDDIAGSTRAIGHT:       // round, line, cloth, incandecsent
      case LST_SQUAREDIAGCLOTH2:       // square, linediag2, cloth, incandescent
      case LST_SQUAREDIAGCLOTH23:       // square, linediag23, cloth, incandecsent
      case LST_SQUAREDIAGSTRAIGHT:       // square, line, cloth, incandecsent
      case LST_FANDIAGCLOTH2:       // round fan, linediag2, cloth, incandescent
      case LST_FANDIAGCLOTH23:       // round fan, linediag23, cloth, incandecsent
      case LST_FANDIAGSTRAIGHT:       // round fan, line, cloth, incandecsent
      case LST_ROUNDOLDFASHCLOTH:
         dwDir = 1;  // z=1 direction opened up more
         break;
      case LST_ROUNDDIAGCLOTH2DN:       // round, linediag2, cloth, incandescent
      case LST_ROUNDDIAGCLOTH23DN:       // round, linediag23, cloth, incandecsent
      case LST_ROUNDDIAGSTRAIGHTDN:       // round, line, cloth, incandecsent
      case LST_SQUAREDIAGCLOTH2DN:       // square, linediag2, cloth, incandescent
      case LST_SQUAREDIAGCLOTH23DN:       // square, linediag23, cloth, incandecsent
      case LST_SQUAREDIAGSTRAIGHTDN:       // square, line, cloth, incandecsent
      case LST_FANDIAGCLOTH2DN:       // round fan, linediag2, cloth, incandescent
      case LST_FANDIAGCLOTH23DN:       // round fan, linediag23, cloth, incandecsent
      case LST_FANDIAGSTRAIGHTDN:       // round fan, line, cloth, incandecsent
      case LST_ROUNDOLDFASHCLOTHDN:
         dwDir = 0;  // z=0 direction opened up more
         break;
      }
      m_apLightDirect[1].h *= fAmount;
      m_afLightAmt[1] *= sqrt(fAmount);   // so keep same strength
      m_afLightAmt[0] /= sqrt(fAmount);

      m_afShadeShow[0] = TRUE;
      m_adwShadeMaterial[0] = 1;

      // bottom and direction
      m_apShadeBottom[0].Zero();
      if (!dwDir) {
         m_apShadeBottom[0].p[2] = m_apShadeSize[0].p[2];
         m_pLightVec.Scale (-1);
         m_apShadeAroundVec[0].Scale (-1);
      }

      // bulb location in the middle
      m_pBulbLoc.p[2] = m_apShadeSize[0].p[2] / 2;
      break;

   }

   // move the ligth bulb
   // create the rotation matrix
   CPoint pA, pB, pC;
   pC.Copy (&m_pBulbDir);
   pC.Normalize();
   pA.Copy (&m_pBulbLeft);
   pA.Normalize();
   pB.CrossProd (&pC, &pA);
   pB.Normalize();
   pA.CrossProd (&pB, &pC);
   pA.Normalize();
   m_mLightBulb.RotationFromVectors (&pA, &pB, &pC);

   // Want to reposition light bulb so that it's center is where says
   // bulb should go
   m_LightBulb.QueryLightInfo (&m_LI, NULL);
   m_LI.pLoc.p[3] = 1;
   m_LI.pLoc.MultiplyLeft (&m_mLightBulb);
   if (fCenterBulb)
      m_pBulbLoc.Subtract (&m_LI.pLoc);

   // add translation to matrix
   CMatrix mTrans;
   mTrans.Translation (m_pBulbLoc.p[0], m_pBulbLoc.p[1], m_pBulbLoc.p[2]);
   m_mLightBulb.MultiplyRight (&mTrans);

   m_fDirtyLI = TRUE;
   m_fDirtyRender = TRUE;
   m_fWasShadeTransparent = TRUE;

   return TRUE;
}


/**********************************************************************************
CLightShade::SizeGet - Returns the lamp shade size

inputs
   PCPoint        pSize - Filled with size
returns
   void
*/
void CLightShade::SizeGet (PCPoint pSize)
{
   pSize->Copy (&m_pSize);
}

/**********************************************************************************
CLightShade::TypeGet - Returns the lightshade type, as passed into Init()
*/
DWORD CLightShade::TypeGet (void)
{
   return m_dwType;
}

/**********************************************************************************
CLightShade::BulbInfoSet - Sets the bulb brightness and color.

inputs
   fp       fWatts - Number of watts
   DWORD    dwColor - color
   BOOL     fNoShadows - Set to TRUE if casts no shadows
*/
BOOL CLightShade::BulbInfoSet (fp fWatts, DWORD dwColor, BOOL fNoShadows)
{
   m_fBulbWatts = fWatts;
   m_dwBulbColor = dwColor;
   m_fBulbNoShadows = fNoShadows;
   m_fDirtyLI = TRUE;
   m_LightBulb.ColorSet (m_dwBulbColor);
   m_LightBulb.WattsSet (m_fBulbWatts);
   m_LightBulb.NoShadowsSet (m_fBulbNoShadows);
   return TRUE;
}

/**********************************************************************************
CLightShade::BulbInfoGet - Fills in information about the light bulb.

inputs
   fp       *pfWatts - Filled with the wattage
   DWORD    *pdwColor - Filled with the color. from lightbulb color
   DWORD    *pdwShape - Filled with the light shape
   BOOL     *pfNoShadows - Filled with flag for no-shadows
returns
   BOOL - TRUE if success
*/
BOOL CLightShade::BulbInfoGet (fp *pfWatts, DWORD *pdwColor, DWORD *pdwShape, BOOL *pfNoShadows)
{
   if (pfWatts)
      *pfWatts = m_fBulbWatts;
   if (pfNoShadows)
      *pfNoShadows = m_fBulbNoShadows;
   if (pdwColor)
      *pdwColor = m_dwBulbColor;
   if (pdwShape)
      *pdwShape = m_dwBulbShape;
   return TRUE;
}

/**********************************************************************************
CLightShade::QueryMaterials - Returns a bit-field flag indicating what materials are
wanted. THe object should create these materials.

returns
   DWORD - bit 0 == material 0, bit 1 == material 1, etc.
*/
DWORD CLightShade::QueryMaterials (void)
{
   DWORD dwRet = 0, i;
   for (i = 0; i < 2; i++)
      if (m_afShadeShow[i])
         dwRet |= (1 << m_adwShadeMaterial[i]);
   return dwRet;
}


static PWSTR gpszLightShade = L"LightShade";
static PWSTR gpszBulbWatts = L"BulbWatts";
static PWSTR gpszBulbColor = L"BulbColor";
static PWSTR gpszBulbNoShadows = L"BulbNoShadows";
static PWSTR gpszSize = L"Size";

/**********************************************************************************
CLightShade::MMLTo - Standard writes out to MML
*/
PCMMLNode2 CLightShade::MMLTo (void)
{
   PCMMLNode2 pNode = new CMMLNode2;
   if (!pNode)
      return NULL;
   pNode->NameSet (gpszLightShade);

   MMLValueSet (pNode, gpszType, (int)m_dwType);
   MMLValueSet (pNode, gpszBulbWatts, m_fBulbWatts);
   MMLValueSet (pNode, gpszBulbColor, (int) m_dwBulbColor);
   MMLValueSet (pNode, gpszBulbNoShadows, (int) m_fBulbNoShadows);
   MMLValueSet (pNode, gpszSize, &m_pSize);
   return pNode;
}


/**********************************************************************************
CLightShade::MMLFrom -Standard, reads in from MML
*/
BOOL CLightShade::MMLFrom (PCMMLNode2 pNode)
{
   m_dwType = (DWORD) MMLValueGetInt (pNode, gpszType, 0);
   if (!Init (m_dwType))
      return FALSE;
   m_fBulbWatts = MMLValueGetDouble (pNode, gpszBulbWatts, m_fBulbWatts);
   m_dwBulbColor = (DWORD)MMLValueGetInt (pNode, gpszBulbColor, m_dwBulbColor);
   m_fBulbNoShadows = (BOOL)MMLValueGetInt (pNode, gpszBulbNoShadows, FALSE);
   CPoint pSize;
   MMLValueGetPoint (pNode, gpszSize, &pSize);

   if (!SizeSet (&pSize))
      return FALSE;

   if (!BulbInfoSet (m_fBulbWatts, m_dwBulbColor, m_fBulbNoShadows))
      return FALSE;

   // dirty flags will have been set byt the two calls
   return TRUE;
}

/**********************************************************************************
CLightShade::Clone - Standard, clones.
*/
CLightShade *CLightShade::Clone (void)
{
   PCLightShade pNew = new CLightShade;
   if (!pNew)
      return NULL;
   if (!CloneTo (pNew)) {
      delete pNew;
      return NULL;
   }
   return pNew;
}


/**********************************************************************************
CLightShade::CloneTo - Standard clones to
*/
BOOL CLightShade::CloneTo (CLightShade *pTo)
{
   // just call the functions to set
   if (!pTo->Init (m_dwType))
      return FALSE;

   if (!pTo->BulbInfoSet (m_fBulbWatts, m_dwBulbColor, m_fBulbNoShadows))
      return FALSE;

   return TRUE;
}

/**********************************************************************************
CLightShade::QueryLightInfo - Fills in pLight with information about the light.

inputs
   PINFOLIGHT     pLight - Filled with information about the light. All coordinates
                  are in shade-space.
   PCWorldSocket           pWorld - Used incase the shade material points to a color scheme
   PCObjectSurface *papShadeMaterial - Array of 4 PCObjectSurface
                  These are checked so that if there are or aren't
                  transparent shades around it then the nature of the omnidirectional
                  light is changed.

returns
   BOOL - TRUE if success
*/
BOOL CLightShade::QueryLightInfo (PLIGHTINFO pLight, PCWorldSocket pWorld, PCObjectSurface *papShadeMaterial)
{
   if (!CalcLIGHTINFOIfNecessary(pWorld, papShadeMaterial))
      return FALSE;

   *pLight = m_LI;
   return TRUE;
}

/**********************************************************************************
CLightShade::Render - Tells the object to draw itsels.

Some notes:
   - Will draw such that 0,0,1 is the main light direction (genreally).
   - Light is attached to 0,0,0 and tends to point in 0,0,1 direction
   - Uses papShadeMaterialID to access color/texture in pTemplate

inputs
   POBJECTRENDER     pr - Render info
   PCRenderSurface   prs - Draw to
   BOOL              fDrawOn - Set to TRUE if the light is on, FALSE if it's off
   PCWorldSocket           pWorld - Used incase the shade material points to a color scheme
   PCObjectSurface   *papShadeMaterial - Pointer to an array of 4 PCObjectSurface
                     used for drawing
*/
BOOL CLightShade::Render (DWORD dwRenderShard, POBJECTRENDER pr, PCRenderSurface prs, BOOL fDrawOn,
                          PCWorldSocket pWorld, PCObjectSurface *papShadeMaterial)
{
   if (!CalcRenderIfNecessary())
      return FALSE;

   // loop over both shades
   DWORD dwShade;
   for (dwShade = 0; dwShade < 2; dwShade++) {
      if (!m_afShadeShow[dwShade] || !m_apRevolution[dwShade])
         continue;   // didn't want to show it

      // get the surface
      PCObjectSurface pos = papShadeMaterial[m_adwShadeMaterial[dwShade]];
      if (pos) {
         prs->SetDefMaterial (dwRenderShard, pos, pWorld);


#if 0 // take this out because it doesn't seem to work well
         // if it's transparent and we have lit up, then light it up
         if (fDrawOn) {
            PRENDERSURFACE pmat;
            pmat = prs->GetDefSurface ();
            if (pmat->Material.m_wTransparency)
               pmat->Material.m_fSelfIllum = TRUE;
         }
#endif // 0
      }

      // draw it
      m_apRevolution[dwShade]->Render (pr, prs);
   }

   // draw the light bulb
   if (m_fBulbDraw) {
      prs->Push();
      prs->Multiply (&m_mLightBulb);
      m_LightBulb.Render (pr, prs, fDrawOn);
      prs->Pop();
   }

   return TRUE;
}

/**********************************************************************************
CLightShade::CalcRenderIfNecessary - Calculates bits necessary for rendering if they're
not done already
*/
BOOL CLightShade::CalcRenderIfNecessary(void)
{
   if (!m_fDirtyRender)
      return TRUE;

   // free up existing
   DWORD dwShade;
   for (dwShade = 0; dwShade < 2; dwShade++) {
      // not supposed to have
      if (!m_afShadeShow[dwShade]) {
         if (m_apRevolution[dwShade])
            delete m_apRevolution[dwShade];
         m_apRevolution[dwShade] = NULL;
         continue;
      }

      // else make sure there'sa revolution
      if (!m_apRevolution[dwShade])
         m_apRevolution[dwShade] = new CRevolution;
      if (!m_apRevolution[dwShade])
         return FALSE;  // error

      m_apRevolution[dwShade]->BackfaceCullSet (FALSE);
      m_apRevolution[dwShade]->DirectionSet (&m_apShadeBottom[dwShade],
         &m_apShadeAroundVec[dwShade], &m_apShadeLeftVec[dwShade]);
      m_apRevolution[dwShade]->ProfileSet (m_adwShadeProfile[dwShade]);
      m_apRevolution[dwShade]->RevolutionSet (m_adwShadeRev[dwShade]);
      m_apRevolution[dwShade]->ScaleSet (&m_apShadeSize[dwShade]);
   }

   m_fDirtyRender = FALSE;
   return TRUE;
}

/**********************************************************************************
CLightShade::CalcLIGHTINFOIfNecessary - Calc bits for the LIGHTINFO if its
not done already.

inputs
   PCWorldSocket           pWorld - Used incase the shade material points to a color scheme
   PCObjectSurface   *papShadeMaterial - Pointer to an array of 4 PCObjectSurface for surface
                     IDs in the pTemplate. If a surface if transparent/opaque effects
                     whether or not light is omnidirectional.

*/
BOOL CLightShade::CalcLIGHTINFOIfNecessary(PCWorldSocket pWorld, PCObjectSurface *papShadeMaterial)
{
   // look at which shades we're using and see if any have switched from transparent
   // to not transparent
   BOOL fFound, fTrans;
   DWORD dwShade, i, j;
   fFound = FALSE;
   fTrans = TRUE;
   for (dwShade = 0; dwShade < 2; dwShade++) {
      if (!m_afShadeShow[dwShade] || !m_afShadeAffectsOmni[dwShade])
         continue;

      // else found one that affects
      fFound = TRUE;
      PCObjectSurface pos;
      BOOL fDelete;
      fDelete = FALSE;
      pos = papShadeMaterial[m_adwShadeMaterial[dwShade]];
      if (pos && pos->m_szScheme[0] && pWorld) {
         PCObjectSurface pNew;
         pNew = (pWorld->SurfaceSchemeGet())->SurfaceGet (pos->m_szScheme, NULL, FALSE);
         if (pNew) {
            pos = pNew;
            fDelete = TRUE;
         }
      }
      if (!pos)
         continue;   // cant tell from this surface

      // if found a non-transparent one then note because will muck up global direction
      if (!pos->m_Material.m_wTransparency)
         fTrans = FALSE;

      if (fDelete)
         delete pos;
   }

   // if the shade was transparent and is no longer, or vice-versa, then the light info
   // is diryt
   if (m_fWasShadeTransparent != fTrans) {
      m_fDirtyLI = TRUE;
      m_fWasShadeTransparent = fTrans;
   }

   // if it's sunlight then always dirty
   if (m_LightBulb.ShapeGet() == LBS_SUNLIGHT)
      m_fDirtyLI = TRUE;

   // if not dirty then done
   if (!m_fDirtyLI)
      return TRUE;

   // else recalc

   // get the light bulb info and then rotate
   m_LightBulb.QueryLightInfo (&m_LI, pWorld);
   m_LI.pLoc.p[3] = 1;
   m_LI.pLoc.MultiplyLeft (&m_mLightBulb);

   // NOTE: Toss out the old directionality and use the direction of the shade
   m_LI.pDir.Copy (&m_pLightVec);
   m_LI.pDir.Normalize();

   // what angle of incidence
   fp afAngle[2];
   for (i = 0; i < 2; i++) {
      if (m_afLightAmt[i]) {
         if (m_apLightDirect[i].v > CLOSE)
            afAngle[i] = atan (m_apLightDirect[i].h / 2.0 / m_apLightDirect[i].v) * 2;
         else
            afAngle[i] = 2 * PI;
      }
      else
         afAngle[i] = 2 * PI; // wont direct light there but just in case
      afAngle[i] = min(afAngle[i], PI*.9);

      if (m_LI.afLumens[i]) {
         if (afAngle[i] < m_LI.afDirectionality[i][0]) {
            m_LI.afDirectionality[i][0] = afAngle[i];
            // BUGFIX - Was multiplying by .9 instead of .85, but make less harsh
            m_LI.afDirectionality[i][1] = min(m_LI.afDirectionality[i][1], afAngle[i] * .85);
         }
      }
      else {
         // fill in some defaults just in case
         m_LI.afDirectionality[i][0] = afAngle[i];
         m_LI.afDirectionality[i][1] = afAngle[i] * .9;

         memcpy (m_LI.awColor[i], m_LI.awColor[2], sizeof(WORD)*3);
      }

      // so can scale and take into account more light in an area
      afAngle[i] = m_LI.afDirectionality[i][0];
      afAngle[i] = max(PI/20, afAngle[i]);
   }

   // reallocate the light energy
   m_LI.afLumens[0] += m_LI.afLumens[2] * m_afLightAmt[0] * (2 * PI / afAngle[0]);
   m_LI.afLumens[1] += m_LI.afLumens[2] * m_afLightAmt[1] * (2 * PI / afAngle[1]);
   m_LI.afLumens[2] *= m_afLightAmt[2];

   // if there aren't any transparencies then set main light to 0
   if (!m_fWasShadeTransparent)
      m_LI.afLumens[2] = 0;

   // adjust the color
   for (i = 0; i < 3; i++) for (j = 0; j < 3; j++)
      m_LI.awColor[i][j] = (WORD)(((DWORD)m_LI.awColor[i][j] * (DWORD)m_awLightColor[i][j]) >> 16);

   m_fDirtyLI = FALSE;
   return TRUE;
}



/**********************************************************************************
CLightBulb::Constructor and destructor
*/
CLightBulb::CLightBulb (void)
{
   m_dwShape = 0;
   m_fWatts = 0;
   m_fNoShadows = FALSE;
   m_dwColor = 0;
   m_fDirtyLI = m_fDirtyRender = TRUE;
   memset (&m_LI, 0, sizeof(m_LI));
   m_pNoodle = NULL;
   m_pRevolution = NULL;
   m_cColor = m_cColorOff = 0;
}

CLightBulb::~CLightBulb (void)
{
   if (m_pNoodle)
      delete m_pNoodle;
   if (m_pRevolution)
      delete m_pRevolution;
}

static PWSTR gpszLightBulb = L"LightBulb";
static PWSTR gpszShape = L"Shape";
static PWSTR gpszWatts = L"Watts";
static PWSTR gpszColor = L"Color";
static PWSTR gpszNoShadows = L"NoShadows";

/**********************************************************************************
CLightBulb::MMLTo - Writes the information to MML.

returns
   PCMMLNode2      pNode - Light bulb
*/
PCMMLNode2 CLightBulb::MMLTo (void)
{
   PCMMLNode2 pNode = new CMMLNode2;
   if (!pNode)
      return NULL;
   pNode->NameSet (gpszLightBulb);

   MMLValueSet (pNode, gpszShape, (int)m_dwShape);
   MMLValueSet (pNode, gpszWatts, m_fWatts);
   MMLValueSet (pNode, gpszColor, (int)m_dwColor);
   MMLValueSet (pNode, gpszNoShadows, (int) m_fNoShadows);

   return pNode;
}

/**********************************************************************************
CLightBulb::MMLFrom - Standard MML function to read in info
*/
BOOL CLightBulb::MMLFrom (PCMMLNode2 pNode)
{
   DWORD dwShape, dwColor;
   fp fWatts;
   BOOL fNoShadows;
   dwShape = (DWORD) MMLValueGetInt (pNode, gpszShape, LBS_INCANGLOBE);
   fWatts  = MMLValueGetDouble (pNode, gpszWatts, 60);
   dwColor = (DWORD) MMLValueGetInt (pNode, gpszColor, LBT_COLOR_WHITE);
   fNoShadows = (BOOL) MMLValueGetInt (pNode, gpszNoShadows, FALSE);

   ShapeSet (dwShape);
   WattsSet (fWatts);
   ColorSet (dwColor);
   NoShadowsSet (fNoShadows);

   return TRUE;
}

/**********************************************************************************
CLightBulb::Clone - Standard clone function
*/
CLightBulb *CLightBulb::Clone (void)
{
   PCLightBulb pNew = new CLightBulb;
   if (!pNew)
      return NULL;
   if (!CloneTo (pNew)) {
      delete pNew;
      return NULL;
   }
   return pNew;
}

/**********************************************************************************
CLightBulb::CloneTo - Standard cloneto function
*/
BOOL CLightBulb::CloneTo (CLightBulb *pTo)
{
   pTo->m_dwShape = m_dwShape;
   pTo->m_fWatts = m_fWatts;
   pTo->m_fNoShadows = m_fNoShadows;
   pTo->m_dwColor = m_dwColor;

   // just set the dirty flags so the rest is calculated
   pTo->m_fDirtyLI = TRUE;
   pTo->m_fDirtyRender = TRUE;

   return TRUE;
}

/**********************************************************************************
CLightBulb::InitFromType - Pass in dwType, which is a combination of LBS_XXX |
   LBT_COLOR_XXX | LBT_WATTS(x), and this intializes all the settings

inputs
   DWORD    dwType - Type
*/
BOOL CLightBulb::InitFromType (DWORD dwType)
{
   ShapeSet (dwType & 0xff);
   WattsSet (dwType >> 16);
   ColorSet (dwType & 0xff00);
   NoShadowsSet (FALSE);

   return TRUE;
}


/**********************************************************************************
CLightBulb::ShapeSet - Sets the light bulbs shape - which is one of LBS_XXX

inputs
   DWORD    dwShape - LBS_XXX
returns
   BOOL - TRUE if succes
*/
BOOL CLightBulb::ShapeSet (DWORD dwShape)
{
   m_dwShape = dwShape;
   m_fDirtyLI = TRUE;
   m_fDirtyRender = TRUE;

   return TRUE;
}

/**********************************************************************************
CLightBulb::ShapeGet - Returns the shape
*/
DWORD CLightBulb::ShapeGet (void)
{
   return m_dwShape;
}

/**********************************************************************************
CLightBulb::WattsSet - Sets the number of watts.
*/
BOOL CLightBulb::WattsSet (fp fWatts)
{
   m_fWatts = fWatts;
   m_fDirtyLI = TRUE;
   return TRUE;
}

/**********************************************************************************
CLightBulb::WattsGet - Retrusnt he number of watts
*/
fp CLightBulb::WattsGet (void)
{
   return m_fWatts;
}


/**********************************************************************************
CLightBulb::NoShadowsSet - Sets fkag for no shadows.
*/
BOOL CLightBulb::NoShadowsSet (BOOL fNoShadows)
{
   m_fNoShadows = fNoShadows;
   m_fDirtyLI = TRUE;
   return TRUE;
}

/**********************************************************************************
CLightBulb::NoShadowsGet - Retruns no shadows flag
*/
BOOL CLightBulb::NoShadowsGet (void)
{
   return m_fNoShadows;
}

/**********************************************************************************
CLightBulb::ColorSet - Sets the light bulb color.

inputs
   DWORD       dwColor - One of LBT_COLOR_XXX
*/
BOOL CLightBulb::ColorSet (DWORD dwColor)
{
   m_dwColor = dwColor;
   m_fDirtyLI = TRUE;
   return TRUE;
}

/**********************************************************************************
CLightBulb::ColorGet - Returns the current color, one of LBT_COLOR_XXX
*/
DWORD CLightBulb::ColorGet (void)
{
   return m_dwColor;
}

/**********************************************************************************
CLightBulb::QueryLightInfo - Fills in pLight with information abou the light.

Some importnat info:
   - Assums that th light bulb is attached at 0,0,0
   - The light bulb always points in 0,0,1
   - The center of the light is probably at 0,0,Z, where Z is a few cm from
         the attachment point
   - Fills in directionality if the light is a spotlight

inputs
   PLIGHTINFO     pLight - Filled in
   PCWorldSocket        pWorld - Pass it in so if using sunlight can find the time
returns
   BOOL - TRUE if success
*/
BOOL CLightBulb::QueryLightInfo (PLIGHTINFO pLight, PCWorldSocket pWorld)
{
   if (m_dwShape == LBS_SUNLIGHT)
      m_fDirtyLI = TRUE;   // because always changing

   if (m_fDirtyLI) {
      DWORD i, j;

      m_fDirtyLI = FALSE;

      memset (&m_LI, 0, sizeof(m_LI));

      m_LI.fNoShadows = m_fNoShadows;

      // some defails
      m_LI.pDir.p[2] = 1;
      m_LI.pDir.p[3] = 1;  // set W so dont have to later
      m_LI.pLoc.p[3] = 1;  // set W so dont have to later

      WORD awColor[3];
      m_cColor = RGB(0xff,0xff,0xff);

      switch (m_dwColor) {
      default:
      case LBT_COLOR_WHITE:
         // do nothing
         break;
      case LBT_COLOR_PINK:
         m_cColor = RGB(0xff,0xe0,0xe0);
         break;
      case LBT_COLOR_FLOURO:
         m_cColor = RGB(0xf0,0xff,0xf0);
         break;
      case LBT_COLOR_FLOUROINSECT:
         m_cColor = RGB(0xff,0xe0,0x20);
         break;
      case LBT_COLOR_BLACKLIGHT:
         m_cColor = RGB(0x80,0x00,0xff);
         break;
      case LBT_COLOR_RED:
         m_cColor = RGB(0xff,0x00,0x00);
         break;
      case LBT_COLOR_GREEN:
         m_cColor = RGB(0x00,0xff,0x00);
         break;
      case LBT_COLOR_BLUE:
         m_cColor = RGB(0x00,0x00,0xff);
         break;
      case LBT_COLOR_YELLOW:
         m_cColor = RGB(0xff,0xff,0x00);
         break;
      case LBT_COLOR_CYAN:
         m_cColor = RGB(0x00,0xff,0xff);
         break;
      case LBT_COLOR_MAGENTA:
         m_cColor = RGB(0xff,0x00,0xff);
         break;
      case LBT_COLOR_SODIUM:
         m_cColor = RGB(0xff,0xf0,0x80);
         break;
      }

      CImage Image;
      Image.Gamma (m_cColor, awColor);

      for (i = 0; i < 3; i++) for (j = 0; j < 3; j++)
         m_LI.awColor[i][j] = awColor[j];

      // make dimmer when off
      awColor[0] /= 2;
      awColor[1] /= 2;
      awColor[2] /= 2;
      m_cColorOff = Image.UnGamma (awColor);

      // based on lightbulb shape
      BOOL fDirectional;
      fp fLumensPerWatt;
      fDirectional = FALSE;
      switch (m_dwShape) {
      case LBS_FLAMECANDLE:
         fLumensPerWatt = CM_LUMENSPERFIREWATT;
         m_LI.pLoc.p[2] = .02;
         break;

      case LBS_FLAMETORCH:
         fLumensPerWatt = CM_LUMENSPERFIREWATT;
         m_LI.pLoc.p[2] = .08;
         break;

      case LBS_FLAMEFIRE:
         fLumensPerWatt = CM_LUMENSPERFIREWATT;
         m_LI.pLoc.p[2] = .16;
         break;

      case LBS_SUNLIGHT:
         fLumensPerWatt = CM_LUMENSPERSODIUMWATT;
         break;

      default:
      case LBS_INCANGLOBE:
         fLumensPerWatt = CM_LUMENSPERINCANWATT;
         m_LI.pLoc.p[2] = .08;
         break;

      case LBS_INCANSPOT:
         fLumensPerWatt = CM_LUMENSPERINCANWATT;
         m_LI.pLoc.p[2] = .08;
         fDirectional = TRUE;
         m_LI.afDirectionality[0][0] = PI/2;
         m_LI.afDirectionality[0][1] = PI/4;
         break;

      case LBS_HALOGENSPOT:
         fLumensPerWatt = CM_LUMENSPERHALOWATT;
         m_LI.pLoc.p[2] = .04;
         fDirectional = TRUE;
         m_LI.afDirectionality[0][0] = PI/2;
         m_LI.afDirectionality[0][1] = PI/4;
         break;

      case LBS_HALOGENBULB:
         fLumensPerWatt = CM_LUMENSPERHALOWATT;
         m_LI.pLoc.p[2] = .01;
         break;

      case LBS_HALOGENTUBE:
         fLumensPerWatt = CM_LUMENSPERHALOWATT;
         break;

      case LBS_FLOURO8:
      case LBS_FLOURO18:
      case LBS_FLOURO36:
      case LBS_FLOUROROUND:
         fLumensPerWatt = CM_LUMENSPERFLOURWATT;
         break;

      case LBS_FLOUROGLOBE:
         fLumensPerWatt = CM_LUMENSPERFLOURWATT;
         m_LI.pLoc.p[2] = .08;
         break;

      case LBS_SODIUMGLOBE:
         fLumensPerWatt = CM_LUMENSPERSODIUMWATT;
         m_LI.pLoc.p[2] = .15;
         break;

      }

      // set the wattage
      if (m_dwShape == LBS_SUNLIGHT) {
         // special case - amount of light depends on time
         DFTIME dwTime;
         fp fTime;
         PWSTR psz;
         if (pWorld) {
            psz = pWorld->VariableGet (WSTime());
            if (psz)
               dwTime = _wtoi(psz);
            else
               DefaultBuildingSettings (NULL, &dwTime);  // BUGFIX - Default building settings

            // while not totally correct, gives a good approximation of the amount
            // of light coming through
            fTime = (fp)HOURFROMDFTIME(dwTime) + (fp)MINUTEFROMDFTIME(dwTime)/60.0;
            fTime -= 12;   // so noon is highest
            fTime = fTime / 6.0 * PI / 2.0;
            fTime = cos(fTime);
            fTime = max(0,fTime);
            fTime = sqrt(fTime);
         }
         else
            fTime = 0;  // doesnt matter

         fLumensPerWatt *= fTime; // assume some light loss
      };

      if (fDirectional) {
         m_LI.afLumens[0] = m_fWatts * fLumensPerWatt;

         // because lumemns measure of total output, and spotlighting,
         // increate the relative strength
         m_LI.afLumens[0] *= (2 * PI / m_LI.afDirectionality[0][0]);
            // IMPORTANT: I think this isn't exactly right, because as the beam gets narrower
            // should be to the 2 power, and to 180 degrees 1.0 power, so chosing 1.0
            // since most lights will be wider
      }
      else {
         m_LI.afLumens[2] = m_fWatts * fLumensPerWatt;
      }
   }

   // copy the info
   *pLight = m_LI;
   return TRUE;
}

/**********************************************************************************
CLightBulb::Render - Draws the light bulb.

Some notes:
   - It will chose its own color and material.
   - Direction will be in 0,0,1
   - Attachement for the bulb is at 0,0,0 (except for tubes, where this is the center)
   - Left (in the case of flouros, the pointy end) is at 1,0,0

inputs
   POBJECTRENDER     pr - Drawing information
   PCRenderSurface   prs - Draw to this.
   BOOL              fDrawOn - If TRUE, draw the lightbulb on
returns
   none
*/
BOOL CLightBulb::Render (POBJECTRENDER pr, PCRenderSurface prs, BOOL fDrawOn)
{
   if (!CalcIfDirty())
      return FALSE;

   if (m_dwShape == LBS_SUNLIGHT)
      return TRUE;   // nothing to draw

   // if this is a final render, and light is not on, then don't draw
   switch (m_dwShape) {
   case LBS_FLAMECANDLE:
   case LBS_FLAMETORCH:
   case LBS_FLAMEFIRE:
      if (!fDrawOn && (pr->dwReason == ORREASON_FINAL))
         return TRUE;   // so dont see
      break;
   } // switch

   // also need light info calculated
   LIGHTINFO li;
   if (m_fDirtyLI)
      QueryLightInfo (&li, NULL);

   // make the color
   COLORREF   cColor;
   RENDERSURFACE Mat;
   memset (&Mat, 0, sizeof(Mat));
   Mat.Material.InitFromID (MATERIAL_GLASSFROSTED);
   Mat.Material.m_wTranslucent = 0xffff; // BUGFIX - Translucency was 0, make 0xffff
   if (fDrawOn) {
      //Mat.Material.m_fSelfIllum = TRUE;
      Mat.Material.m_wTransparency = 1;   // so can see it illuminated, but so light goes through
      Mat.Material.m_fNoShadows = TRUE;   // so get transparency
   }
   else
      Mat.Material.m_wTransparency = 0;

   cColor = fDrawOn ? m_cColor : m_cColorOff;

   switch (m_dwShape) {
   case LBS_FLAMECANDLE:
   case LBS_FLAMETORCH:
   case LBS_FLAMEFIRE:
      if (!fDrawOn)
         Mat.Material.m_wTransparency = 0xffff;   // so can barely see it
      break;

   case LBS_INCANGLOBE:
   case LBS_INCANSPOT:
   case LBS_HALOGENSPOT:
   case LBS_HALOGENBULB:
   case LBS_HALOGENTUBE:
   case LBS_FLOURO8:
   case LBS_FLOURO18:
   case LBS_FLOURO36:
   case LBS_FLOUROROUND:
   case LBS_FLOUROGLOBE:
   case LBS_SODIUMGLOBE:
      // dont need to do anything
      break;
   }


   prs->SetDefMaterial (&Mat);
   prs->SetDefColor (cColor);

   // draw it
   if (m_pNoodle)
      m_pNoodle->Render(pr, prs);
   if (m_pRevolution)
      m_pRevolution->Render (pr, prs);

   if ((m_dwShape == LBS_FLAMECANDLE) || (m_dwShape == LBS_FLAMETORCH) || (m_dwShape == LBS_FLAMEFIRE)) {

      CPoint pSize;
      pSize.Zero();
      pSize.p[0] = .02;
      pSize.p[1] = .02;
      pSize.p[2] = .04;
      switch (m_dwShape) {
      case LBS_FLAMECANDLE:
         break;
      case LBS_FLAMETORCH:
         pSize.Scale (4);
         break;
      case LBS_FLAMEFIRE:
         pSize.Scale (8);
         break;
      }
      // draw a bumpy surface
#define  FIREX      8
#define  FIREY      5
      DWORD stx, sty;
      CPoint ap[FIREX * FIREY];
      PCPoint pp;
      stx = FIREX;
      sty = FIREY;
      DWORD x,y;
      srand (1234);
      for (x = 0; x < stx; x++) for (y = 0; y < sty; y++) {
         pp = &ap[y*stx+x];
         fp fRadK, fRadI;
         fRadK = PI/2 * (1.0-CLOSE) - y / (fp) (sty-1) * PI * (1.0-CLOSE);
         fRadI = (fp)x / (fp) stx * 2.0 * PI;
         pp->p[1] = cos(fRadI) * cos(fRadK) * pSize.p[1]/2;
         pp->p[2] = sin(fRadK) * pSize.p[2]/2;
         pp->p[0] = -sin(fRadI) * cos(fRadK) * pSize.p[0]/2;

         if ((y != 0) && (y != sty-1)) {
            pp->p[0] *= MyRand (.8, 1.2);
            pp->p[1] *= MyRand (.8, 1.2);
            pp->p[2] *= MyRand (.8, 1.2);
         }

         // offset
         pp->p[2] += pSize.p[2] / 2;
      }
      DWORD dwIndex;
      pp = prs->NewPoints (&dwIndex, stx * sty);
      if (pp) {
         memcpy (pp, ap, stx * sty * sizeof(CPoint));
         prs->ShapeSurface (1, stx, sty, pp, dwIndex);
      }

   }  // draw fire

   return TRUE;
}

BOOL CLightBulb::CalcIfDirty (void)
{
   if (!m_fDirtyRender)
      return TRUE;

   // clear existing
   if (m_pNoodle)
      delete m_pNoodle;
   m_pNoodle = NULL;
   if (m_pRevolution)
      delete m_pRevolution;
   m_pRevolution = NULL;

   CPoint pBottom, pAroundVec, pLeftVec, pScale;
   fp fLength;
   DWORD dwType;
   pBottom.Zero();
   pAroundVec.Zero();
   pAroundVec.p[2] = 1;
   pLeftVec.Zero();
   pLeftVec.p[0] = 1;
   pScale.Zero();
   fLength = 0;

   // based on the type
   switch (m_dwShape) {
   case LBS_FLAMECANDLE:
   case LBS_FLAMETORCH:
   case LBS_FLAMEFIRE:
      // no noodles/revolutions to create
      break;

   case LBS_INCANGLOBE:
   case LBS_FLOUROGLOBE:
   case LBS_SODIUMGLOBE:
   case LBS_INCANSPOT:
   case LBS_HALOGENSPOT:
   case LBS_HALOGENBULB:
      m_pRevolution = new CRevolution;
      if (!m_pRevolution)
         return FALSE;
      m_pRevolution->BackfaceCullSet (TRUE); // since entirely enclosed
      m_pRevolution->DirectionSet (&pBottom, &pAroundVec, &pLeftVec);
      m_pRevolution->RevolutionSet (RREV_CIRCLE);
      switch (m_dwShape) {
      default:
      case LBS_INCANGLOBE:
         pScale.p[0] = pScale.p[1] = CM_INCANGLOBEDIAMETER;
         pScale.p[2] = CM_INCANGLOBELENGTH;
         dwType = RPROF_LIGHTGLOBE;
         break;
      case LBS_FLOUROGLOBE:
         pScale.p[0] = pScale.p[1] = CM_FLOUROGLOBEDIAMETER;
         pScale.p[2] = CM_FLOUROGLOBELENGTH;
         dwType = RPROF_LIGHTGLOBE;
         break;
      case LBS_SODIUMGLOBE:
         pScale.p[0] = pScale.p[1] = CM_SODIUMGLOBEDIAMETER;
         pScale.p[2] = CM_SODIUMGLOBELENGTH;
         dwType = RPROF_LIGHTGLOBE;
         break;
      case LBS_INCANSPOT:
         pScale.p[0] = pScale.p[1] = CM_INCANSPOTDIAMETER;
         pScale.p[2] = CM_INCANSPOTLENGTH;
         dwType = RPROF_LIGHTSPOT;
         break;
      case LBS_HALOGENSPOT:
         pScale.p[0] = pScale.p[1] = CM_HALOGENSPOTDIAMETER;
         pScale.p[2] = CM_HALOGENSPOTLENGTH;
         dwType = RPROF_LIGHTSPOT;
         break;
      case LBS_HALOGENBULB:
         pScale.p[0] = pScale.p[1] = CM_HALOGENBULBDIAMETER;
         pScale.p[2] = CM_HALOGENBULBLENGTH;
         dwType = RPROF_CIRCLE;
         break;
      }
      m_pRevolution->ScaleSet (&pScale);
      m_pRevolution->ProfileSet (dwType);
      break;



   case LBS_HALOGENTUBE:
   case LBS_FLOURO8:
   case LBS_FLOURO18:
   case LBS_FLOURO36:
   case LBS_FLOUROROUND:
      m_pNoodle = new CNoodle;
      if (!m_pNoodle)
         return FALSE;

      switch (m_dwShape) {
      case LBS_HALOGENTUBE:
         pScale.p[0] = pScale.p[1] = CM_HALOGENTUBEDIAMETER;
         fLength = CM_HALOGENTUBELENGTH;
         break;
      case LBS_FLOURO8:
         pScale.p[0] = pScale.p[1] = CM_FLOURO8DIAMETER;
         fLength = CM_FLOURO8LENGTH;
         break;
      case LBS_FLOURO18:
         pScale.p[0] = pScale.p[1] = CM_FLOURO18DIAMETER;
         fLength = CM_FLOURO18LENGTH;
         break;
      case LBS_FLOURO36:
         pScale.p[0] = pScale.p[1] = CM_FLOURO36DIAMETER;
         fLength = CM_FLOURO36LENGTH;
         break;
      case LBS_FLOUROROUND:
         pScale.p[0] = pScale.p[1] = CM_FLOUROROUNDDIAMETER;
         fLength = CM_FLOUROROUNDRINGDIAMETER;
         break;
      }

      m_pNoodle->BackfaceCullSet (TRUE);
      m_pNoodle->DrawEndsSet (TRUE);
      m_pNoodle->FrontVector (&pAroundVec);
      m_pNoodle->ScaleVector (&pScale);
      CPoint ap[4];
      memset (ap, 0, sizeof(ap));
      if (m_dwShape == LBS_FLOUROROUND) {
         ap[0].p[0] = fLength/2;
         ap[1].p[1] = fLength/2;
         ap[2].p[0] = -fLength/2;
         ap[3].p[1] = -fLength/2;
         CSpline s;
         m_pNoodle->PathSpline (TRUE, 4, ap, (DWORD*)SEGCURVE_CIRCLENEXT);
      }
      else {
         // linear
         ap[0].p[0] = -fLength/2;
         ap[1].p[0] = fLength/2;
         m_pNoodle->PathLinear (&ap[0], &ap[1]);
      }
      m_pNoodle->ShapeDefault (NS_CIRCLEFAST);
      break;
   }

   m_fDirtyRender = FALSE;
   return TRUE;
}


/**********************************************************************************
CObjectLight::Constructor and destructor */
CObjectLight::CObjectLight (PVOID pParams, POSINFO pInfo)
{
   m_dwType = (DWORD)(size_t) pParams;
   m_OSINFO = *pInfo;

   m_dwRenderShow = RENDERSHOW_ELECTRICAL;

   m_fLightOn = 0.0;
   m_pLightShade = new CLightShade;
   m_pLightShade->Init ((m_dwType >> 8) & 0xff);
   DWORD dwScale;
   dwScale = (m_dwType >> 16) & 0xff;
   if ((dwScale != 0x10) && dwScale) {
      CPoint pScale;
      m_pLightShade->SizeGet (&pScale);
      pScale.Scale ((fp)dwScale / 16.0);
      m_pLightShade->SizeSet (&pScale);
   }

   m_pLightStand = new CLightStand;
   m_pLightStand->Init (m_dwType & 0xff);


   m_fCanBeEmbedded = m_pLightStand->CanEmbed();

   // create surfaces for the light
   CreateSurfaces ();
}


CObjectLight::~CObjectLight (void)
{
   if (m_pLightShade)
      delete m_pLightShade;
   if (m_pLightStand)
      delete m_pLightStand;
}


/**********************************************************************************
CObjectLight::Delete - Called to delete this object
*/
void CObjectLight::Delete (void)
{
   delete this;
}

/**********************************************************************************
CObjectLight::Render - Draws a test box.

inputs
   POBJECTRENDER     pr - Render information
*/
void CObjectLight::Render (POBJECTRENDER pr, DWORD dwSubObject)
{
   // create the surface render object and draw
   // CRenderSurface rs;
   m_Renderrs.ClearAll();

   CMatrix mObject;
   m_Renderrs.Init (pr->pRS);
   ObjectMatrixGet (&mObject);
   m_Renderrs.Multiply (&mObject);


   CreateSurfaces();

   GUID gContain;
   if (m_pLightStand->IsHanging() && !EmbedContainerGet(&gContain)) {
      // rotate so that Z=1 goes to Z=-1
      m_Renderrs.Rotate (PI, 1);
   }
   else if (m_fCanBeEmbedded) {
      // rotate so that Z=1 goes to Y = -1
      m_Renderrs.Rotate (PI/2, 1);
   }

   // render the stand
   m_Renderrs.SetDefMaterial (m_OSINFO.dwRenderShard, ObjectSurfaceFind (LOSURF_STAND), m_pWorld);
   m_pLightStand->Render (pr, &m_Renderrs);
  
   DWORD dwPost;
   for (dwPost = 0; dwPost < m_pLightStand->PostNum(); dwPost++) {
      CMatrix mPostToStand;
      PCLightPost pPost;
      pPost = m_pLightStand->PostGet (dwPost);
      if (!pPost)
         continue;
      m_pLightStand->PostMatrix (dwPost, &mPostToStand);
      m_Renderrs.Push();
      m_Renderrs.Multiply (&mPostToStand);

      // to the post
      m_Renderrs.SetDefMaterial (m_OSINFO.dwRenderShard, ObjectSurfaceFind (LOSURF_POST), m_pWorld);
      pPost->Render (pr, &m_Renderrs);

      // do the stalks
      DWORD i;
      for (i = 0; i < pPost->StalkNum(); i++) {
         CMatrix mStalk;
         PCLightStalk pStalk;
         pStalk = pPost->StalkGet(i);
         if (!pStalk)
            continue;
         pPost->StalkMatrix (i, &mStalk);

         m_Renderrs.Push();
         m_Renderrs.Multiply (&mStalk);
         m_Renderrs.SetDefMaterial (m_OSINFO.dwRenderShard, ObjectSurfaceFind (LOSURF_STALK), m_pWorld);
         pStalk->Render (pr, &m_Renderrs);

         // and shades within
         // shade
         {
            CMatrix  m;
            pStalk->MatrixGet (&m);
            m_Renderrs.Push();
            m_Renderrs.Multiply (&m);

            // render
            PCObjectSurface aPOS[4];
            DWORD k;
            for (k = 0; k < 4; k++)
               aPOS[k] = ObjectSurfaceFind (LOSURF_FORSHADE + k);
            m_pLightShade->Render (m_OSINFO.dwRenderShard, pr, &m_Renderrs, m_fLightOn ? TRUE : FALSE, m_pWorld, aPOS);

            m_Renderrs.Pop();
         }  // shade

         m_Renderrs.Pop();
      }  // over stalks

      m_Renderrs.Pop();
   }  // over posts


   m_Renderrs.Commit();
}

// NOTE: Not doing QueryBoundingBox() because code is a bit complicated and likely
// to result in error

/**********************************************************************************
CObjectLight::Clone - Clones this

inputs
   none
returns
   PCObjectSocket - Clone
*/
PCObjectSocket CObjectLight::Clone (void)
{
   PCObjectLight pNew;

   pNew = new CObjectLight(NULL, &m_OSINFO);

   // clone template info
   CloneTemplate(pNew);

   m_pLightShade->CloneTo (pNew->m_pLightShade);
   m_pLightStand->CloneTo (pNew->m_pLightStand);
   pNew->m_fLightOn = m_fLightOn;
   return pNew;
}

static PWSTR gpszLightOn = L"LightOn";
static PWSTR gpszInfinite = L"Infinite";
static PWSTR gpszLoc = L"Loc";
static PWSTR gpszDir = L"Dir";

PCMMLNode2 CObjectLight::MMLTo (void)
{
   // call into the template first
   PCMMLNode2 pNode = MMLToTemplate();
   if (!pNode)
      return NULL;

   // write info
   MMLValueSet (pNode, gpszLightOn, m_fLightOn);

   PCMMLNode2 pSub;
   pSub = m_pLightShade->MMLTo();
   if (pSub) {
      pSub->NameSet (gpszLightShade);
      pNode->ContentAdd (pSub);
   }
   pSub = m_pLightStand->MMLTo();
   if (pSub) {
      pSub->NameSet (gpszLightStand);
      pNode->ContentAdd (pSub);
   }

   return pNode;
}

BOOL CObjectLight::MMLFrom (PCMMLNode2 pNode)
{
   // call into the template first
   if (!MMLFromTemplate (pNode))
      return FALSE;


   m_fLightOn = MMLValueGetDouble (pNode, gpszLightOn, 0);

   PCMMLNode2 pSub;
   PWSTR psz;
   pSub = NULL;
   pNode->ContentEnum (pNode->ContentFind (gpszLightShade), &psz, &pSub);
   if (pSub)
      m_pLightShade->MMLFrom (pSub);
   pSub = NULL;
   pNode->ContentEnum (pNode->ContentFind (gpszLightStand), &psz, &pSub);
   if (pSub)
      m_pLightStand->MMLFrom (pSub);

   // remember can be embedded based on type
   m_fCanBeEmbedded = m_pLightStand->CanEmbed();

   return TRUE;
}



/**************************************************************************************
CObjectLight::MoveReferencePointQuery - 
given a move reference index, this fill in pp with the position of
the move reference RELATIVE to ObjectMatrixGet. References are numbers
from 0+. If the index is more than the number of points then the
function returns FALSE

inputs
   DWORD       dwIndex - index.0 .. # ref
   PCPoint     pp - Filled with point relative to ObjectMatrixGet() IF its valid
returns
   BOOL - TRUE if valid index.
*/
BOOL CObjectLight::MoveReferencePointQuery (DWORD dwIndex, PCPoint pp)
{
   PSPLINEMOVEP ps;
   DWORD dwDataSize;
   ps = gaLightMove;
   dwDataSize = sizeof(gaLightMove);

   if (dwIndex >= dwDataSize / sizeof(SPLINEMOVEP))
      return FALSE;

   // always at 0,0 in Lights
   pp->Zero();
   return TRUE;
}

/**************************************************************************************
CObjectLight::MoveReferenceStringQuery -
given a move reference index (numbered from 0 to the number of references)
this fills in a string at psz and dwSize BYTES that names the move reference
to the end user. *pdwNeeded is filled with the number of bytes needed for
the string. Returns FALSE if dwIndex is too high, or dwSize is too small (although
pdwNeeded will be filled in)

inputs
   DWORD       dwIndex - index. 0.. # ref
   PWSTR       psz - To be filled in witht he string
   DWORD       dwSize - # of bytes available in psz
   DWORD       *pdwNeeded - If not NULL, filled with the size needed
returns
   BOOL - TRUE if psz copied.
*/
BOOL CObjectLight::MoveReferenceStringQuery (DWORD dwIndex, PWSTR psz, DWORD dwSize, DWORD *pdwNeeded)
{
   PSPLINEMOVEP ps;
   DWORD dwDataSize;
   ps = gaLightMove;
   dwDataSize = sizeof(gaLightMove);
   if (dwIndex >= dwDataSize / sizeof(SPLINEMOVEP)) {
      if (pdwNeeded)
         *pdwNeeded = 0;
      return FALSE;
   }

   DWORD dwNeeded;
   dwNeeded = ((DWORD)wcslen (ps[dwIndex].pszName) + 1) * 2;
   if (pdwNeeded)
      *pdwNeeded = dwNeeded;
   if (dwNeeded <= dwSize) {
      wcscpy (psz, ps[dwIndex].pszName);
      return TRUE;
   }
   else
      return FALSE;
}

/********************************************************************************
CObjectLight::LightQuery -
ask the object if it has any lights. If it does, pl is added to (it
is already initialized to sizeof LIGHTINFO) with one or more LIGHTINFO
structures. Coordinates are in OBJECT SPACE.
*/
BOOL CObjectLight::LightQuery (PCListFixed pl, DWORD dwShow)
{
   if (!m_fLightOn)
      return FALSE;

   // make sure want to see lights
   if (!(dwShow & m_dwRenderShow))
      return FALSE;

   // copy over
   LIGHTINFO liOrig, li;
   PCObjectSurface aPOS[4];
   DWORD k;
   for (k = 0; k < 4; k++)
      aPOS[k] = ObjectSurfaceFind (LOSURF_FORSHADE + k);
   if (!m_pLightShade->QueryLightInfo (&liOrig, m_pWorld, aPOS))
      return FALSE;

   DWORD dwPost;
   CMatrix mEmbed;
   GUID gContain;
   if (m_pLightStand->IsHanging() && !EmbedContainerGet(&gContain)) {
      // rotate so that Z=1 goes to Z=-1
      mEmbed.RotationX (PI);
   }
   else if (m_fCanBeEmbedded)
      mEmbed.RotationX (PI/2);
   else
      mEmbed.Identity();

   for (dwPost = 0; dwPost < m_pLightStand->PostNum(); dwPost++) {
      DWORD i;
      PCLightPost pPost;
      CMatrix mPostToStand;
      pPost = m_pLightStand->PostGet(dwPost);
      if (!pPost)
         continue;
      m_pLightStand->PostMatrix (dwPost, &mPostToStand);

      mPostToStand.MultiplyRight (&mEmbed);

      for (i = 0; i < pPost->StalkNum(); i++) {
         li = liOrig;

         PCLightStalk pStalk;
         pStalk = pPost->StalkGet(i);
         if (!pStalk)
            continue;
         CMatrix mStalkToPost;
         pPost->StalkMatrix (i, &mStalkToPost);
         mStalkToPost.MultiplyRight (&mPostToStand);

         // rotate so Z=1 goes to Y=-1
         CMatrix mStalk, mInvT;
         pStalk->MatrixGet (&mStalk);
         mStalk.MultiplyRight (&mStalkToPost);
         mStalk.Invert (&mInvT);
         mInvT.Transpose ();
         li.pDir.p[3] = 1;
         li.pLoc.p[3] = 1;
         li.pDir.MultiplyLeft (&mInvT);
         li.pLoc.MultiplyLeft (&mStalk);

         // BUGFIX - Dimmer wasnt working
         li.afLumens[0] *= m_fLightOn;
         li.afLumens[1] *= m_fLightOn;
         li.afLumens[2] *= m_fLightOn;

         pl->Add (&li);
      }  // over stalks
   }  // over posts
   return TRUE;
}


/**********************************************************************************
CObjectLight::TurnOnGet - 
returns how TurnOn the object is, from 0 (closed) to 1.0 (TurnOn), or
< 0 for an object that can't be TurnOned
*/
fp CObjectLight::TurnOnGet (void)
{
   return m_fLightOn;
}

/**********************************************************************************
CObjectLight::TurnOnSet - 
TurnOns/closes the object. if fTurnOn==0 it's close, 1.0 = TurnOn, and
values in between are partially TurnOned closed. Returne TRUE if success
*/
BOOL CObjectLight::TurnOnSet (fp fTurnOn)
{
   fTurnOn = max(0,fTurnOn);
   fTurnOn = min(1,fTurnOn);

   // BUBUG - Some ligths have no dimmer setting

   m_pWorld->ObjectAboutToChange (this);
   m_fLightOn = fTurnOn;
   m_pWorld->ObjectChanged (this);

   return TRUE;
}


/*************************************************************************************
CObjectLight::ControlPointQuery - Called to query the information about a control
point identified by dwID.

inputs
   DWORD       dwID - ID
   POSCONTROL  pInfo - Filled in with the infomration
returns
   BOOL - TRUE if successful
*/
BOOL CObjectLight::ControlPointQuery (DWORD dwID, POSCONTROL pInfo)
{
   if (m_pLightStand->ControlPointQuery (dwID, pInfo)) {
      // rotate into light stalk space if embedded
      if (m_fCanBeEmbedded || m_pLightStand->IsHanging()) {
         CMatrix m, mInvTrans;
         GUID gContain;
         m.RotationX ((m_pLightStand->IsHanging() && !EmbedContainerGet(&gContain)) ?
            (PI) : (PI/2));
         m.Invert (&mInvTrans);
         mInvTrans.Transpose();

//         pInfo->pDirection.MultiplyLeft (&mInvTrans);
         pInfo->pLocation.MultiplyLeft (&m);
         // dont bother with v1 and v2 since wont use
      }
      return TRUE;
   }

   return FALSE;
}

/*************************************************************************************
CObjectLight::ControlPointSet - Called to change the valud of a control point.

inputs
   DWORD       dwID - ID
   PCPoint     pVal - Contains the new location, in object coordinates
returns
   BOOL - TRUE if successful
*/
BOOL CObjectLight::ControlPointSet (DWORD dwID, PCPoint pVal, PCPoint pViewer)
{
   if (m_pLightStand->ControlPointIsValid(dwID)) {
      CPoint pNewVal, pNewView;
      pNewVal.Copy (pVal);
      pNewView.Copy (pViewer ? pViewer : pVal);

      if (m_fCanBeEmbedded || m_pLightStand->IsHanging()) {
         // rotate embedded a bit
         CMatrix m, mInv;
         GUID gContain;
         m.RotationX ((m_pLightStand->IsHanging() && !EmbedContainerGet(&gContain)) ?
            (PI) : (PI/2));
         m.Invert4 (&mInv);

         pNewVal.p[3] = 1;
         pNewView.p[3] = 1;
         pNewVal.MultiplyLeft (&mInv);
         pNewView.MultiplyLeft (&mInv);
      }

      if (m_pWorld)
         m_pWorld->ObjectAboutToChange (this);
      m_pLightStand->ControlPointSet (dwID, &pNewVal, pViewer ? &pNewView : NULL);
      if (m_pWorld)
         m_pWorld->ObjectChanged (this);
      return TRUE;
   }
   return FALSE;
}

/*************************************************************************************
CObjectLight::ControlPointEnum - Called to enumerate a list of control point IDs
into the list.

inputs
   PCListFixed       plDWORD - Should be filled in with control points
returns
   none
*/
void CObjectLight::ControlPointEnum (PCListFixed plDWORD)
{
   m_pLightStand->ControlPointEnum (plDWORD);
}

/*************************************************************************************
CObjectLight::CreateSurfaces - If ther are any surfaces that need to be created
the are. If they dont need to exist they're deleted
*/
void CObjectLight::CreateSurfaces (void)
{
   // find out what surfaces he shade needs
   DWORD dwShade, i;
   dwShade = m_pLightShade->QueryMaterials();

   for (i = 0; i < 4; i++) {
      if (!(dwShade & (1 << i))) {
         // dont want it
         if (ObjectSurfaceFind (LOSURF_FORSHADE + i))
            ObjectSurfaceRemove (LOSURF_FORSHADE + i);
         continue;
      }

      // if already have it good enough
      if (ObjectSurfaceFind (LOSURF_FORSHADE + i))
         continue;

      // else, add it
      switch (i) {
      case 0: // 0 = default opaque
         ObjectSurfaceAdd (LOSURF_FORSHADE + i, RGB(0x40,0x40,0x40), MATERIAL_METALROUGH,
            L"Light shade, opaque");
         break;
      case 1: // 1 = default cloth (like lamp shade)
         ObjectSurfaceAdd (LOSURF_FORSHADE + i, RGB(0x80,0x80,0x00), MATERIAL_TARP,
            L"Light shade, translucent cover",
            &GTEXTURECODE_LightShade, &GTEXTURESUB_LightShade);
         break;
      case 2: // 2 = default translucent glass
         ObjectSurfaceAdd (LOSURF_FORSHADE + i, RGB(0xc0,0xc0,0xc0), MATERIAL_GLASSFROSTED,
            L"Light cover, frosted");
         break;
      default:
      case 3: // 3 = default transparent glass
         ObjectSurfaceAdd (LOSURF_FORSHADE + i, RGB(0xff,0xff,0xff), MATERIAL_GLASSCLEAR,
            L"Light cover, glass");
         break;
      }
   }


   if (!ObjectSurfaceFind (LOSURF_STALK))
      ObjectSurfaceAdd (LOSURF_STALK, RGB(0x40,0x40,0x40), MATERIAL_METALROUGH,
         L"Light stand, adjustable");
   if (!ObjectSurfaceFind (LOSURF_POST))
      ObjectSurfaceAdd (LOSURF_POST, RGB(0x40,0x40,0x60), MATERIAL_METALROUGH,
         L"Light post");
   if (!ObjectSurfaceFind (LOSURF_STAND))
      ObjectSurfaceAdd (LOSURF_STAND, RGB(0x20,0x20,0x40), MATERIAL_METALROUGH,
         L"Light stand");
}


/**********************************************************************************
CObjectLight::EmbedDoCutout - Member function specific to the template. Called
when the object has moved within the surface. This enables the super-class for
the embedded object to pass a cutout into the container. (Basically, specify the
hole for the window or door)
*/
BOOL CObjectLight::EmbedDoCutout (void)
{
   // take this opportunity to adjust the hanging bits since will be called
   // after the object has been moved
   if (m_pLightStand->IsHanging()) {
      if (m_pWorld)
         m_pWorld->ObjectAboutToChange (this);

      CMatrix mEmbed, mRot;
      GUID gContain;
      if (m_pLightStand->IsHanging() && !EmbedContainerGet(&gContain)) {
         // rotate so that Z=1 goes to Z=-1
         mEmbed.RotationX (PI);
      }
      else if (m_fCanBeEmbedded)
         mEmbed.RotationX (PI/2);
      else
         mEmbed.Identity();

      mRot.Multiply (&m_MatrixObject, &mEmbed);

      m_pLightStand->HangingMatrixSet (&mRot);

      if (m_pWorld)
         m_pWorld->ObjectChanged (this);
   }

   // make sure we can do cutouts
   DWORD dwShape;
   CPoint pSize;
   if (!m_pLightStand->CanEmbed(&dwShape, &pSize))
      return FALSE;
   if (!dwShape)
      return FALSE;
   pSize.Scale (.5);

   // find the surface
   GUID gCont;
   PCObjectSocket pos;
   if (!m_pWorld || !EmbedContainerGet (&gCont))
      return FALSE;
   pos = m_pWorld->ObjectGet (m_pWorld->ObjectFind (&gCont));
   if (!pos)
      return FALSE;

   // will need to transform from this object space into the container space
   CMatrix mCont, mTrans;
   pos->ObjectMatrixGet (&mCont);
   mCont.Invert4 (&mTrans);
   mTrans.MultiplyLeft (&m_MatrixObject);

   // make fill in based on the shape
   DWORD dwNum;
   switch (dwShape) {
   case 1: // rectangular
   default:
      dwNum = 4;
      break;
   case 2:  // elliptical
      dwNum = 16;
      break;
   }

   CMem memFront, memBack;
   if (!memFront.Required(dwNum * sizeof(CPoint)) || !memBack.Required(dwNum * sizeof(CPoint)))
      return FALSE;
   PCPoint pFront, pBack;
   pFront = (PCPoint) memFront.p;
   pBack = (PCPoint) memBack.p;

   DWORD i;
   for (i = 0; i < dwNum; i++) {
      fp fAngle, fLen;

      switch (dwShape) {
      case 1: // rectangular
      default:
         fAngle = ((fp) i + .5) /  4.0 * PI * 2.0;
         fLen = sqrt((fp)2);
         pFront[i].p[0] = cos(fAngle) * fLen * pSize.p[0];
         pFront[i].p[2] = -sin(fAngle) * fLen * pSize.p[1];
         break;
      case 2:  // elliptical
         fAngle = (fp) i / (fp)dwNum * PI  * 2.0;
         pFront[i].p[0] = cos(fAngle) * pSize.p[0];
         pFront[i].p[2] = -sin(fAngle) * pSize.p[1];
         break;
      }
      pFront[i].p[1] = 0;
      pFront[i].p[3] = 1;

      pBack[i].Copy (&pFront[i]);
      pBack[i].p[1] = m_fContainerDepth + CLOSE;
      pBack[i].p[3] = 1;
   }

   // bevel

   // convert to object's space
   for (i = 0; i < dwNum; i++) {
      pFront[i].MultiplyLeft (&mTrans);
      pBack[i].MultiplyLeft (&mTrans);
   }
   pos->ContCutout (&m_gGUID, dwNum, pFront, pBack, FALSE);

   return TRUE;
}



/**********************************************************************************
CObjectLight::ObjectMatrixSet - Capture so can notify object of what's up/down
*/
BOOL CObjectLight::ObjectMatrixSet (CMatrix *pObject)
{
   if (!CObjectTemplate::ObjectMatrixSet (pObject))
      return FALSE;

   if (m_pLightStand->IsHanging()) {
      if (m_pWorld)
         m_pWorld->ObjectAboutToChange (this);

      CMatrix mEmbed, mRot;
      GUID gContain;
      if (m_pLightStand->IsHanging() && !EmbedContainerGet(&gContain)) {
         // rotate so that Z=1 goes to Z=-1
         mEmbed.RotationX (PI);
      }
      else if (m_fCanBeEmbedded)
         mEmbed.RotationX (PI/2);
      else
         mEmbed.Identity();

      mRot.Multiply (&m_MatrixObject, &mEmbed);

      m_pLightStand->HangingMatrixSet (&mRot);

      if (m_pWorld)
         m_pWorld->ObjectChanged (this);
   }

   return TRUE;
}

/**********************************************************************************
CObjectLight::DialogQuery - Returns TRUE if the object supports a custom
dialog box, FALSE if it doesn't. The default object doesn't
*/
BOOL CObjectLight::DialogQuery (void)
{
   return TRUE;
}

/****************************************************************************
LightPage
*/
BOOL LightPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCObjectLight pv = (PCObjectLight) pPage->m_pUserData;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         DWORD dwShape, dwColor;
         BOOL fNoShadows;
         fp fWatts;
         pv->m_pLightShade->BulbInfoGet(&fWatts, &dwColor, &dwShape, &fNoShadows);
         ComboBoxSet (pPage, L"lightbulb", dwShape);
         ComboBoxSet (pPage, L"lightcolor", dwColor);
         DoubleToControl (pPage, L"watts", fWatts);

         PCEscControl pControl;
         pControl = pPage->ControlFind (L"noshadows");
         if (pControl)
            pControl->AttribSetBOOL (Checked(), fNoShadows);

         DWORD dwType;
         CPoint pSize;
         dwType = pv->m_pLightShade->TypeGet ();
         pv->m_pLightShade->SizeGet (&pSize);
         ComboBoxSet (pPage, L"lightshade", dwType);
         MeasureToString (pPage, L"size0", pSize.p[0]);
         MeasureToString (pPage, L"size1", pSize.p[1]);
         MeasureToString (pPage, L"size2", pSize.p[2]);

         dwType = pv->m_pLightStand->TypeGet ();
         ComboBoxSet (pPage, L"lightstand", dwType);
      }
      break;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;

         if (!_wcsicmp(p->pControl->m_pszName, L"noshadows")) {
            DWORD dwShape, dwColor;
            BOOL fNoShadows;
            fp fWatts;
            pv->m_pLightShade->BulbInfoGet(&fWatts, &dwColor, &dwShape, &fNoShadows);
            fNoShadows = p->pControl->AttribGetBOOL (Checked());
            if (pv->m_pWorld)
               pv->m_pWorld->ObjectAboutToChange (pv);
            pv->m_pLightShade->BulbInfoSet (fWatts, dwColor, fNoShadows);
            if (pv->m_pWorld)
               pv->m_pWorld->ObjectChanged (pv);
            return TRUE;
         }
      }
      break;

   case ESCN_COMBOBOXSELCHANGE:
      {
         PESCNCOMBOBOXSELCHANGE p = (PESCNCOMBOBOXSELCHANGE) pParam;
         if (!p->pControl->m_pszName)
            break;

         PWSTR psz;
         DWORD dwVal;
         psz = p->pControl->m_pszName;
         dwVal = p->pszName ? _wtoi(p->pszName) : 0;

         if (!_wcsicmp(psz, L"lightcolor")) {
            DWORD dwShape, dwColor;
            BOOL fNoShadows;
            fp fWatts;
            pv->m_pLightShade->BulbInfoGet(&fWatts, &dwColor, &dwShape, &fNoShadows);
            if (dwColor == dwVal)
               return TRUE;   // nothing really changed

            if (pv->m_pWorld)
               pv->m_pWorld->ObjectAboutToChange (pv);
            pv->m_pLightShade->BulbInfoSet (fWatts, dwVal, fNoShadows);
            if (pv->m_pWorld)
               pv->m_pWorld->ObjectChanged (pv);
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"lightshade")) {
            DWORD dwType = pv->m_pLightShade->TypeGet();
            if (dwType == dwVal)
               return TRUE;   // nothing really changed

            if (pv->m_pWorld)
               pv->m_pWorld->ObjectAboutToChange (pv);
            pv->m_pLightShade->Init (dwVal);
            if (pv->m_pWorld)
               pv->m_pWorld->ObjectChanged (pv);

            // refresh the light bulb since that will change
            DWORD dwShape, dwColor;
            BOOL fNoShadows;
            fp fWatts;
            pv->m_pLightShade->BulbInfoGet(&fWatts, &dwColor, &dwShape, &fNoShadows);
            ComboBoxSet (pPage, L"lightbulb", dwShape);
            ComboBoxSet (pPage, L"lightcolor", dwColor);
            DoubleToControl (pPage, L"watts", fWatts);

            // and refresh the size
            CPoint pSize;
            pv->m_pLightShade->SizeGet (&pSize);
            MeasureToString (pPage, L"size0", pSize.p[0]);
            MeasureToString (pPage, L"size1", pSize.p[1]);
            MeasureToString (pPage, L"size2", pSize.p[2]);
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"lightstand")) {
            DWORD dwType = pv->m_pLightStand->TypeGet();
            if (dwType == dwVal)
               return TRUE;   // nothing really changed

            if (pv->m_pWorld)
               pv->m_pWorld->ObjectAboutToChange (pv);
            pv->m_pLightStand->Init (dwVal);
            if (pv->m_pWorld)
               pv->m_pWorld->ObjectChanged (pv);
            return TRUE;
         }

      }
      break;

   case ESCN_EDITCHANGE:
      {
         PESCNEDITCHANGE p = (PESCNEDITCHANGE) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;   // default
         PWSTR psz;
         psz = p->pControl->m_pszName;

         PWSTR pszSize = L"size";
         DWORD dwLen = (DWORD)wcslen(pszSize);

         if (!wcsncmp(psz, pszSize, dwLen)) {
            DWORD dwDim = _wtoi(psz + dwLen);
            dwDim = min(2, dwDim);

            CPoint pSize;
            pv->m_pLightShade->SizeGet (&pSize);

            if (pv->m_pWorld)
               pv->m_pWorld->ObjectAboutToChange (pv);
            MeasureParseString (pPage, psz, &pSize.p[dwDim]);
            pv->m_pLightShade->SizeSet (&pSize);
            if (pv->m_pWorld)
               pv->m_pWorld->ObjectChanged (pv);
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"watts")) {
            DWORD dwColor;
            BOOL fNoShadows;
            fp fWatts;
            pv->m_pLightShade->BulbInfoGet(&fWatts, &dwColor, NULL, &fNoShadows);

            if (pv->m_pWorld)
               pv->m_pWorld->ObjectAboutToChange (pv);
            fWatts = DoubleFromControl (pPage, psz);
            pv->m_pLightShade->BulbInfoSet (fWatts, dwColor, fNoShadows);
            if (pv->m_pWorld)
               pv->m_pWorld->ObjectChanged (pv);
            return TRUE;
         }
      }
      break;
 
   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Light settings";
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}



/**********************************************************************************
CObjectLight::DialogShow - Causes the object to show a dialog box that allows
different attributes to be changed.

inputs
   DWORD             dwSuface - Surface that was clicked on. 0 if unknown.
   PCEscWindow       pWindow - Escarpment window to draw it in
returns
   BOOL - TRUE if user pressed "Back", FALSE if user pressed "Close"
*/

BOOL CObjectLight::DialogShow (DWORD dwSurface, PCEscWindow pWindow, PCPoint pClick)
{
   PWSTR pszRet;

   pszRet = pWindow->PageDialog (ghInstance, IDR_MMLLIGHT, LightPage, this);

   // remember can be embedded based on type
   m_fCanBeEmbedded = m_pLightStand->CanEmbed();

   if (!pszRet)
      return FALSE;
   return !_wcsicmp(pszRet, Back());
}





