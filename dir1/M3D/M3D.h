/***********************************************************************************
M3D.h - header file for ASP
*/

#ifndef _M3D_H_
#define _M3D_H_

#ifndef DLLEXPORT
#define  DLLEXPORT      __declspec (dllexport)
#endif

#define M3DFILEEXT      "m3d"
#define CACHEFILEEXT    L"me2"
#define CACHEFILEEXTA    "me2"
#define LIBRARYFILEEXT  L"me3"
#define LIBRARYFILEEXTA  "me3"
#define APPSHORTNAME    "3DOB"
#define APPSHORTNAMEW   L"3DOB"
#define APPLONGNAME     "3D Outside the Box"
#define APPLONGNAMEW    L"3D Outside the Box"

#include <math.h>
#include "resleak.h"
//#define  COMPILE_MULTIMON_STUBS     // so that works win win95
#include <multimon.h>

// {3E29DDF3-CD44-4617-AE85-2FB2FC91FBB8}
DEFINE_GUID(CLSID_FileHeaderOld, 
0x3e29ddf3, 0xcd44, 0x4617, 0xae, 0x85, 0x2f, 0xb2, 0xfc, 0x91, 0xfb, 0xb8);

// {EB2351B1-EAA6-4200-825D-AAD21B5416A2}
DEFINE_GUID(CLSID_FileHeaderNew, 
0xeb2351b1, 0xeaa6, 0x4200, 0x82, 0x5d, 0xaa, 0xd2, 0x1b, 0x54, 0x16, 0xa2);

DEFINE_GUID(CLSID_FileHeaderNewer, 
0xeb2351b2, 0xeaa6, 0x4200, 0x82, 0x5d, 0xaa, 0xd2, 0x1b, 0x54, 0x16, 0xa2);

// define internal setting, fp, as either double or float depending upon accuracy desired
// using floats because in test was 10% faster and uses a lot less memory
//typedef double fp;

// #ifdef _WIN64
typedef double fp;
#define  EPSILON           0.0000001
#define CLOSE              .00001
#define LONGDIST           1000000.0    // 100 km
//#else
#if 0 // BUGFIX - Disable because doubles are now faster
typedef float fp;
#define  EPSILON           0.000001
#define CLOSE              .0001
#define LONGDIST           100000.0    // 100 km
#endif

#pragma warning(disable:4305 4244)     // disable float to double conversion warnings

#define MAXRENDERSHARDS             4  // maximum number of shards for rendering
#define DEFAULTRENDERSHARD          (MAXRENDERSHARDS-1)  // just to test for 3DOB

/*********************************************************************************
Common measurements */
// BUGBUG - Need to verify all these measurements
#define  CM_THICKCEMENTBLOCK        .2    // thickness of cement block
#define  CM_THICKTHINCEMENTBLOCK    .1    // thickness of the half cement blocks for internal
#define  CM_THICKSTUDWALL           .075  // thickness of a stud wall
#define  CM_THICKSHEETROCK          .008  // thickness of sheet rock
#define  CM_THICKEXTNERALCLADDING   .015  // thickness of typical external cladding material
#define  CM_THICKRENDER             .01   // thicknes of rendering
#define  CM_THICKBRICKVENEER        .1    // thickness of brick veneer
#define  CM_THICKBRICKWALL          .2    // thickness of a brick wall
#define  CM_THICKSTONEWALL          .3    // thickness of a stone wall
#define  CM_THICKHAYBALE            .5    // thickness of a hay bale wall
#define  CM_THICKLOGWALL            .2    // thickness of a log wall
#define  CM_THICKROOFBEAM           .15   // thickness of roof (beams)
#define  CM_THICKROOFING            .025  // thickness of roofing materials
#define  CM_THICKWOODFLOOR          .015  // thickness of wood floor
#define  CM_THICKTILEONLY           .01   // thickness of tile only
#define  CM_THICKTILEONSUB          (CM_THICKWOODFLOOR + CM_THICKTILEONLY) // thickness of tile  on a wood subfloor
#define  CM_THICKFLOORJOIST         .15   // thickness of a floor joist
#define  CM_THICKDROPCEILING        .075  // thickness of a drop ceiling
#define  CM_THICKPAD                .15   // thickness of pad
#define  CM_THICKCABINETCOUNTER     .040  // usual countertop thickness
#define  CM_THICKCABINETWALL        .015  // usal  cabinet wall thickness
#define  CM_THICKCABINETSHELF       .020  // usual cabinet shelf thickness
#define  CM_RISERHEIGHT             .18   // height between steps
#define  CM_TREADDEPTH              .30   // ideal tread depth given CM_RISERHEIGHT
#define  CM_DOORWIDTH               .80   // standard door width, 2'8"
#define  CM_EXTERIORDOORWIDTH       .90   // exterior door width, 3'0"
#define  CM_DOORHEIGHT              2.05  // starndard door height
#define  CM_DOORHEIGHTDFRAME        0.9   // standard location of fram-divider when dividing
#define  CM_WINDOWWIDTH             1.0   // standard window width
#define  CM_WINDOWHEIGHT            1.0   // standard window height
#define  CM_CABINETDOORWIDTH        0.45   // standard cabinet door with
#define  CM_CABINETDOORHEIGHT       0.70  // standard cabinet door height
#define  CM_FRAMEWIDTH              0.05  // standard frame width
#define  CM_FRAMEDEPTH              0.125  // standard frame depth
#define  CM_SUBFRAMEWIDTH           0.025  // standard sub-frame width
#define  CM_SUBFRAMEDEPTH           0.050  // standard sub-frame depth
#define  CM_DOORDIVFRAMEWINDOW      0.06    // standard frame in door division for a window
#define  CM_DOORDIVFRAMEDOOR        0.10   // standard frame in door division for a door
#define  CM_MULLIONWIDTH            0.02  // standard mullion width
#define  CM_DOORFRAMEALUMDOOR       0.04  // door frame thickness for alumninmum frame
#define  CM_DOORTHICKNESS           0.04  // thickness of a door
#define  CM_WINDOWTHICKNESS         0.03  // window thickness
#define  CM_CABINETDOORTHICKNESS    0.015 // typical cabinet door thickness
#define  CM_LOUVERHEIGHT            0.15  // typical louver height
#define  CM_DOOKNOBHEIGHT           1.03  // height of doorknob above ground
#define  CM_DOORKNOBINSIDE          0.06  // amount that doorknob is inside edge of door
#define  CM_CABINETKNOBHEIGHT       0.6  // height of doorknob above ground
#define  CM_CABINETKNOBINSIDE       0.04  // amount that doorknob is inside edge of door
#define  CM_EYEHEIGHT               1.8   // eye height of someone walking

#define  CM_LUMENSPERFLOURWATT      75    // lumens per watt of flourscent
#define  CM_LUMENSPERINCANWATT      13    // number of lumens per watt of incandescent
#define  CM_LUMENSPERHALOWATT       25    // number of luments per watt of halogen light
#define  CM_LUMENSPERSODIUMWATT     120   // number of lumens per watt of sodium vapour
#define  CM_LUMENSPERFIREWATT      (CM_LUMENSPERINCANWATT / 10.0)    // number of lumens per watt of fire
#define  CM_SODIUMEFFICIENCY        0.9   // amount of energy into flourescent bulb that becomes light
#define  CM_LUMENSSUN               (CM_LUMENSPERSODIUMWATT * CM_SUNWATTS / CM_SODIUMEFFICIENCY) // number of lumens from sun
                                    // BUGFIX - Had / 4 / PI in CM_LUMNENSSUN- dont want this, since since wasn't strong enough
#define  CM_LUMENSMOON              (CM_LUMENSPERFLOURWATT * .25)       // guess about illumination from moonlight
#define  CM_BESTEXPOSURE            (CM_LUMENSSUN / 2.71)   // default exposure for full sunlight

#define  CM_SUNWATTS                1370.0   // number of watts in 1 m2 of sun
#define  CM_PANELWATTSPERM2         (2.0 * 75)  // number of watts electricity from one square meter of solar cells
#define  CM_PANELEFFICIENCY         0.15  // solar panel efficiency

#define  CM_INCANGLOBEDIAMETER      .07      // incandecsnet globe
#define  CM_INCANGLOBELENGTH        .12
#define  CM_FLOUROGLOBEDIAMETER     .07      // flourescent globe
#define  CM_FLOUROGLOBELENGTH       .15
#define  CM_SODIUMGLOBEDIAMETER     (CM_INCANGLOBEDIAMETER*2)      // soduium globe
#define  CM_SODIUMGLOBELENGTH       (CM_INCANGLOBELENGTH*2)
#define  CM_INCANSPOTDIAMETER       .13      // large incandesncent spotlight
#define  CM_INCANSPOTLENGTH         .12
#define  CM_HALOGENSPOTDIAMETER     .05      // small halogen spotlight
#define  CM_HALOGENSPOTLENGTH       .07
#define  CM_HALOGENBULBDIAMETER     .02      // small halogen bulb
#define  CM_HALOGENBULBLENGTH       .03
#define  CM_HALOGENTUBEDIAMETER     .015     // halogen tube
#define  CM_HALOGENTUBELENGTH       .10
#define  CM_FLOURO8DIAMETER         .02      // flour tube
#define  CM_FLOURO8LENGTH           .27
#define  CM_FLOURO18DIAMETER        .03      // flour tube
#define  CM_FLOURO18LENGTH          .55
#define  CM_FLOURO36DIAMETER        .03      // flour tube
#define  CM_FLOURO36LENGTH          1.10
#define  CM_FLOUROROUNDDIAMETER        .03      // flour ring
#define  CM_FLOUROROUNDRINGDIAMETER    .20

#define  CM_OFFICE4FLOUROLENGTH     (CM_FLOURO36LENGTH*1.2) // flouros embedded in office ceiling
#define  CM_OFFICE4FLOUROWIDTH      (CM_OFFICE4FLOUROLENGTH / 2)

#define  CM_CABINETHEIGHT           .90         // default cabinet height
#define  CM_CABINETDEPTH            .55         // default cabinet depth
#define  CM_CABINETCOUNTEROVERHANG  .05         // default countertop overhang
#define  CM_CABINETBASEEXTEND       .05         // default amount base of cabinet underneath cabinet

/*********************************************************************************
Time and date */
typedef DWORD DFDATE;
typedef DWORD DFTIME;

#define TODFDATE(day,month,year) ((DFDATE) ((DWORD)(day) + ((DWORD)(month)<<8) + ((DWORD)(year)<<16) ))
#define YEARFROMDFDATE(df) ((DWORD)(df) >> 16)
#define MONTHFROMDFDATE(df) (((DWORD)(df) >> 8) & 0xff)
#define DAYFROMDFDATE(df) ((DWORD)(df) & 0xff)

#define TODFTIME(hour,minute) ( ((DWORD)((hour) & 0xff)<<8) + (DWORD)((minute)&0xff) )
#define HOURFROMDFTIME(df) ( ((df) >= 0x1800) ? (DWORD)-1 : ((DWORD)(df) >> 8) )
#define MINUTEFROMDFTIME(df) ( ((df) >= 0x1800) ? (DWORD)-1 : ((DWORD)(df) & 0xff) )

/*****************************************************************************
Util */

#define  M3DBUTTONSIZE     28    // buttons are 32 pixels
   // BUGFIX - Made buttons a bit larger
   // BUGFIX - Shrunk from 36 to 30 since getting too crowded
#define  M3DSMALLBUTTONSIZE   18

/*****************************************************************************
CProgress */

class DLLEXPORT CProgress : public CProgressSocket {
public:
   ESCNEWDELETE;

   CProgress (void);
   ~CProgress (void);

   BOOL Start (HWND hWnd, char *pszTitle, BOOL fShowRightAway = FALSE);
   BOOL Stop (void);
   virtual BOOL Push (float fMin, float fMax);
   virtual BOOL Pop (void);
   virtual int  Update (float fProgress);
   virtual BOOL WantToCancel (void);
   virtual void CanRedraw (void);

   // private
   LRESULT WndProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

private:
   DWORD       m_dwStartTime;          // GetTickCount() when start last called, 0 if not started
   DWORD       m_dwLastUpdateTime;     // last time update was called
   HWND        m_hWndParent;           // passed into start
   char        m_szTitle[64];          // title to display
   HWND        m_hWnd;                 // window displaying to, NULL if nothing showing
   fp      m_fProgress;            // progress, from 0..1
   CListFixed  m_lStack;               // stack containing the current min and max - to scale Update() call
};
typedef CProgress *PCProgress;


/***********************************************************************************
CRenderMatrix */



class DLLEXPORT CMatrix;

// NOTE: Specifically not making CPoint a public CEscObject, which means
// shouldn't call "new CPoint"
class DLLEXPORT CPoint {
public:
   fp      p[4];

   __inline void Copy (const CPoint *pSrc)  // copy source point into this
      {
         memcpy (p, pSrc->p, sizeof(p));
      };

   __inline void Scale (fp fScale)    // scale xyz by fScale
      {
         p[0] *= fScale;
         p[1] *= fScale;
         p[2] *= fScale;
      };

   __inline void Zero (void) // zeros out the contents
      {
         p[0] = p[1] = p[2] = 0;
         p[3] = 1;
      };

   __inline void Max (const CPoint *pCompare)
      {
         DWORD i;
         for (i = 0; i < 3; i++)
            p[i] = max(p[i], pCompare->p[i]);
      };

   __inline void Min (const CPoint *pCompare)
      {
         DWORD i;
         for (i = 0; i < 3; i++)
            p[i] = min(p[i], pCompare->p[i]);
      };

   __inline void Zero4 (void) // zeros out the contents
      {
         p[0] = p[1] = p[2] = p[3] = 0;
      };

   __inline void CrossProd (const CPoint *pLeft, const CPoint *pRight)  // cross product
      {
         p[0] = pLeft->p[1] * pRight->p[2] - pLeft->p[2] * pRight->p[1];
         p[1] = pLeft->p[2] * pRight->p[0] - pLeft->p[0] * pRight->p[2];
         p[2] = pLeft->p[0] * pRight->p[1] - pLeft->p[1] * pRight->p[0];
      };

   __inline fp DotProd (const CPoint *pRight)  // cross product, using current one as left, and paramter as right
      {
         return (p[0] * pRight->p[0] + p[1] * pRight->p[1] + p[2] * pRight->p[2]);
      }

   __inline fp DotProd4 (const CPoint *pRight)  // cross product, using current one as left, and paramter as right
      {
         return (p[0] * pRight->p[0] + p[1] * pRight->p[1] + p[2] * pRight->p[2] + p[3] * pRight->p[3]);
      }

   __inline void Add (const CPoint *pAdd)  // add the first 3 elem of the vecotor
      {
         p[0] += pAdd->p[0];
         p[1] += pAdd->p[1];
         p[2] += pAdd->p[2];
      }

   __inline void Add (const CPoint *pLeft, const CPoint *pRight)  // add the first 3 elem of the vecotor
      {
         p[0] = pLeft->p[0] + pRight->p[0];
         p[1] = pLeft->p[1] + pRight->p[1];
         p[2] = pLeft->p[2] + pRight->p[2];
         p[3] = 1;
      }

   __inline void Subtract (const CPoint *pSub)  // subract the first 3 elem of the vecotor, pSub from this
      {
         p[0] -= pSub->p[0];
         p[1] -= pSub->p[1];
         p[2] -= pSub->p[2];
      }

   __inline void Subtract (const CPoint *pLeft, const CPoint *pRight)  // add the first 3 elem of the vecotor
      {
         p[0] = pLeft->p[0] - pRight->p[0];
         p[1] = pLeft->p[1] - pRight->p[1];
         p[2] = pLeft->p[2] - pRight->p[2];
         p[3] = 1;
      }

   __inline void Subtract4 (const CPoint *pLeft, const CPoint *pRight)  // add the first 3 elem of the vecotor
      {
         p[0] = pLeft->p[0] - pRight->p[0];
         p[1] = pLeft->p[1] - pRight->p[1];
         p[2] = pLeft->p[2] - pRight->p[2];
         p[3] = pLeft->p[3] - pRight->p[3];
      }

   __inline fp Length (void) 
      {
         return (fp)sqrt (p[0] * p[0] + p[1] * p[1] + p[2] * p[2]);
      }

   __inline fp Length4 (void) 
      {
         return (fp)sqrt (p[0] * p[0] + p[1] * p[1] + p[2] * p[2] + p[3] * p[3]);
      }

   __inline BOOL Normalize (void)   // normalize. FALSE if can't
      {
         fp fInv, f = Length();
         if (f) {
            fInv = (fp)(1.0 / f);
            p[0] *= fInv;
            p[1] *= fInv;
            p[2] *= fInv;
            return TRUE;
         }
         else
            return FALSE;
      }

   __inline BOOL Normalize4 (void)   // normalize. FALSE if can't
      {
         fp fInv, f = Length4();
         if (f) {
            fInv = (fp)(1.0 / f);
            p[0] *= fInv;
            p[1] *= fInv;
            p[2] *= fInv;
            p[3] *= fInv;
            return TRUE;
         }
         else
            return FALSE;
      }
   void MultiplyLeft (CMatrix *pLeft);    // multiply by a matrix, to the left
   void MakeANormal (CPoint *pCounterClock, CPoint *pCenter, CPoint *pClock, BOOL fNormalize = TRUE);
   void CrossProd4 (CPoint *p1, CPoint *p2, CPoint *p3);
   void Average (CPoint *pOther, fp fContribOther = .5);
   void Average (CPoint *pA, CPoint *pB, fp fContribA = .5);
   BOOL AreClose (const CPoint *pp);
   void IfCloseToZeroMakeZero (void);
};

typedef CPoint *PCPoint;


class DLLEXPORT CMatrix {
public:
   ESCNEWDELETE;

   fp      p[4][4];

   __inline void Copy (const CMatrix *pSrc)  // copy src matrix to this
      {
      memcpy (p, pSrc->p, sizeof(p));
      };

   __inline void Multiply (const CMatrix *pLeft, const CMatrix *pRight) // multiply two matrices into this one
      {
         int   i;
         int   j;

         for (i = 0; i < 4; i++)
            for (j = 0; j < 4; j++)
               p[j][i] = pLeft->p[0][i] * pRight->p[j][0] +
                           pLeft->p[1][i] * pRight->p[j][1] +
                           pLeft->p[2][i] * pRight->p[j][2] +
                           pLeft->p[3][i] * pRight->p[j][3];
      };

   __inline void MultiplyLeft (const CMatrix *pRight) // multiply two matrices into this one
      {
         CMatrix t;
         t.Copy (this);
         Multiply (&t, pRight);
      };

   __inline void MultiplyRight (const CMatrix *pLeft) // multiply two matrices into this one
      {
         CMatrix t;
         t.Copy (this);
         Multiply (pLeft, &t);
      };

   __inline void Zero (void) // zeros out the contents
      {
         memset (p, 0, sizeof(p));
      };

   __inline void Transpose (void) // transpose this matrix into the desitnation matrix
   {
      int   i,j;
      fp t;

      for (i = 0; i < 4; i++)
         for (j = i+1; j < 4; j++) {
            t = p[i][j];
            p[i][j] = p[j][i];
            p[j][i] = t;
         }
   };

   __inline fp Determinant (void) // take the determinant of the matrix
      {
         return   (p[0][1] * p[1][2] - p[0][2] * p[1][1]) * p[2][0] +
                  (p[0][2] * p[1][0] - p[0][0] * p[1][2]) * p[2][1] +
                  (p[0][0] * p[1][1] - p[0][1] * p[1][0]) * p[2][2];
      };

   __inline void Multiply (const CPoint *pRight, CPoint *pDest)   // multiply this matrix by point pRight store in pDest
      {
      #define  MMP(i)   pDest->p[i] = p[0][i] * pRight->p[0] + p[1][i] * pRight->p[1] + p[2][i] * pRight->p[2] + p[3][i] * pRight->p[3]

         MMP(0);
         MMP(1);
         MMP(2);
         MMP(3);
      #undef MMP
      }

   __inline void Multiply2D (const CPoint *pRight, CPoint *pDest)   // multiply this matrix by point pRight store in pDest
      // assumes only X,Y and W are valid. Ignores W
      {
      #define  MMP(i)   pDest->p[i] = p[0][i] * pRight->p[0] + p[1][i] * pRight->p[1] + p[3][i] * pRight->p[3]

         MMP(0);
         MMP(1);
         MMP(3);
      #undef MMP
      }

   __inline void Add (const CMatrix *pAdd) // add a matrix onto this one
      {
         int   i,j;
         for (i = 0; i < 4; i++)
            for (j = 0; j < 4; j++)
               p[i][j] += pAdd->p[i][j];
      }

   __inline void Identity (void)
      {
         Zero ();
         p[0][0] = p[1][1] = p[2][2] = p[3][3] = 1.0;
      }

   fp CoFactor (DWORD dwRow, DWORD dwColumn);
   void Invert (CMatrix *pDest);
   void Invert4 (CMatrix *pDest);
   void MakeSquareOld (void);
   void Rotation (fp xrot, fp yrot, fp zrot);
   void RotationX (fp xrot); // rotation about x
   void RotationY (fp yrot); // rotation about y
   void RotationZ (fp zrot); // rotation about z
   void Translation (fp x, fp y, fp z);
   void Scale (fp x, fp y, fp z);
   void Perspect (fp fov = PI / 4, fp znear = 0, fp zfar_inv = 0);
   void ToXYZLLT (PCPoint pXYZ, fp *pfLongitude, fp *pfLatitude, fp *pfTilt);
   void FromXYZLLT (PCPoint pXYZ, fp fLongitude, fp fLatitude, fp fTilt);
   void RotationBasedOnVector (PCPoint pOrig, PCPoint pAfter);
   void RotationFromVectors (PCPoint pA, PCPoint pB, PCPoint pC);
   BOOL AreClose (const CMatrix *pm);
   void MakeSquare (void);

};
typedef CMatrix *PCMatrix;


/******************************************************************************
CObjectSurface */

#define  MATERIAL_CUSTOM         0     // custom material - reads transparency and rest in
#define  MATERIAL_PAINTMATTE     11    // matte paint
#define  MATERIAL_PAINTSEMIGLOSS 12    // semi-closs paint
#define  MATERIAL_PAINTGLOSS     13    // gloss paint
#define  MATERIAL_METALROUGH     21    // metal, rough finish
#define  MATERIAL_METALSMOOTH    22    // metal, smooth finish
#define  MATERIAL_GLASSCLEAR     31    // clear glass - pane
#define  MATERIAL_GLASSFROSTED   32    // frosted glass - pane
#define  MATERIAL_WATERSHALLOW   33    // shallow water
#define  MATERIAL_WATERPOOL      34    // pool depth water
#define  MATERIAL_WATERDEEP      35    // deep water
#define  MATERIAL_INVISIBLE      36    // make something invisible
#define  MATERIAL_GLASSCLEARSOLID 37    // clear glass - solid
#define  MATERIAL_MIRROR         38    // total mirror
#define  MATERIAL_PLASTIC        41    // plastic look
#define  MATERIAL_FLAT           51    // completely flat material, like tree
#define  MATERIAL_TILEGLAZED     61    // glazed tile
#define  MATERIAL_TILEMATTE      62    // unglassed
#define  MATERIAL_FLYSCREEN      71    // flyscreen appearnace
#define  MATERIAL_TARP           72    // tarp
#define  MATERIAL_CLOTHROUGH     73
#define  MATERIAL_CLOTHSMOOTH    74
#define  MATERIAL_CLOTHSILKY     75
#define  MATERIAL_LEAF           76

#define MATERIAL_MAX             77    // highst value for materials

// NOTE: Specificall not making CMaterial : public CEscObject because causes problems with RENDERSURFACE
// This is OK so long as dont do "new CMaterial"
class DLLEXPORT CMaterial {
public:
   DWORD       m_dwID;              // material ID, see MATERIAL_XXX
   DWORD       m_dwMaterialType;    // steel, wood, etc. To be defined
   WORD        m_wTransparency;     // 0 = opaque, 0xffff is fully transparent
   WORD        m_wTransAngle;       // if 0xffff acts like flyscreen and becomes opaque at an angle. 0x0000 like glass
   WORD        m_wSpecExponent;     // specularity exponent, 100 = 1.0, higher number larger multiples
   WORD        m_wSpecReflect;      // amount reflected by specularity, 0xffff is max, 0x0000 is none
   WORD        m_wSpecPlastic;      // how much specularilty looks like plastic. 0xffff - very plasticy, 0x0000 - not plasticy
   WORD        m_wIndexOfRefract;   // index of refraction x 100
   WORD        m_wReflectAmount;    // brightness of reflection
   WORD        m_wReflectAngle;     // if 0xffff acts like glass, only reflecting at angle. 0x0000 mirror
   WORD        m_wTranslucent;      // 0 = none, 0xffff = max translucency
   WORD        m_wFill;             // fill - must be set to 0
   BOOL        m_fGlow;             // Only used for non-texture mode. if TRUE, self illuminating surface (light emitted = color)
   BOOL        m_fNoShadows;        // if TRUE, dont cast shadows

   BOOL InitFromID (DWORD dwID);
   BOOL MMLTo (PCMMLNode2 pNode);
   BOOL MMLFrom (PCMMLNode2 pNode);
   BOOL Dialog (HWND hWnd);
};
typedef CMaterial * PCMaterial;

// TEXTUREMODS is a set of texture modifications based on the original
typedef struct {
   WORD           wHue;          // Hue rotation. 0 = no change. + are clockwise
   WORD           wSaturation;   // 0x1000 is normal saturation
   COLORREF       cTint;         // multiply this color by any in the texture map
   WORD           wContrast;     // 0x1000 is normal contrast, >1000 is higher contract
   WORD           wBrightness;   // 0x1000 is normal brightness
} TEXTUREMODS, *PTEXTUREMODS;

// CObjectSurface is a structure used to describe the coloring/texture of
// a surface on an object
class DLLEXPORT CObjectSurface {
public:
   ESCNEWDELETE;

   CObjectSurface (void);
   PCMMLNode2 MMLTo (void);
   BOOL MMLFrom (PCMMLNode2 pNode);
   CObjectSurface *Clone (void);
   BOOL AreTheSame (CObjectSurface *pComp);


   // member variables. Can be modified no problem
   DWORD          m_dwID;          // used by CRenderSurface for the minor ID. 0 to 1023
   WCHAR          m_szScheme[64];  // if not NULL, then try to use the color from the scheme
                                 // if cant find the scheme then use info below

   BOOL           m_fUseTextureMap; // if TRUE use the texture map information (most of below)
                                 // else, use cColor
   COLORREF       m_cColor;        // color to use if no texture
   GUID           m_gTextureCode;  // GUID that specifies the texture code to use
   GUID           m_gTextureSub;   // GUID that specifies sub-type of gTextureCode
   TEXTUREMODS    m_TextureMods;   // modifications applied to the texture
   fp             m_afTextureMatrix[2][2];// rotation/scaling matrix for texture HV
   CMatrix        m_mTextureMatrix; // matrix for rotating/scaling xyz texture

   CMaterial      m_Material;       // information about material for surface
};


/***********************************************************************************
CImage */
#define  IMAGETRANSSIZE    4
#define  IMAGEDITHERBITS   3
#define  IMAGEDITHERBINS   (1 << IMAGEDITHERBITS)
extern int gaImageDither[IMAGEDITHERBINS][IMAGETRANSSIZE][IMAGETRANSSIZE];
extern BYTE gaImageUnGamma[0x10000];
extern WORD gaImageGamma[0x100];

// useful tools
DLLEXPORT void GammaInit (void);  /// initialize gamma values if not already initialized
DLLEXPORT WORD Gamma (BYTE bColor);   // gamma correct
DLLEXPORT void Gamma (COLORREF cr, WORD *pRGB);  // gamma correct
DLLEXPORT BYTE UnGamma (WORD wColor); // un-gamma correct
DLLEXPORT COLORREF UnGamma (const WORD *pRGB);     // un-gamma correect

// IDPARTBITS_XXX some information hidden in the upper bits of dwIDPart
#define IDPARTBITS_TRANSPARENT        0x80000000
#define IDPARTBITS_MASK               (~(IDPARTBITS_TRANSPARENT))

#define ZINFINITE          1000000        // distance that's considered infinite

typedef struct {
   WORD     wRed, wGreen, wBlue; // gamma corrected, 0..0xffff
   WORD     wFill;               // not used
   DWORD    dwIDPart;            // least-signficiant info of ID. No bits on high end
   DWORD    dwID;                // ID of what was drawn. HIWORD = major, LOWORD=minor
   float    fZ;                  // z depth
   // int      iZ;                  // z depth. In 1/65536 of a meter. Positive values.
} IMAGEPIXEL, *PIMAGEPIXEL;

typedef struct {
   float            h, v;                // horizontal and vertical location of texture
      // BUGFIX - Changed from fp to float so that no more than 8-bytes to store texture
} TEXTUREPOINT, *PTEXTUREPOINT;

// TEXTPOINT5 - For storing texturepoint
typedef struct {
   float            hv[2];               // horizontal and vertical location of texture, [0]=h, [1]=v
         // NOTE: hv[2] must be same as TEXTUREPOINT or code will break
   float            xyz[3];              // xyz location within texture
} TEXTPOINT5, *PTEXTPOINT5;


typedef struct {
   fp   x; // in pixels. Might be off the display
   fp   y; // y value. Not used in all the calls.
   fp   z; // positive number. depth in meters
   fp   w; // w-value. Used to make sure that perspective is applied to z, color, and texture
   WORD     aColor[3];  // RGB color. 0..65535. Not used in all the calls
   TEXTPOINT5 tp;  // texture point
} IHLEND, *PIHLEND;

typedef struct {
   WORD     wRed, wGreen, wBlue;      // red, green, blue. Used for some calls but not others.
   WORD     wFill;               // not used
   DWORD    dwIDPart;             // least-signficiant info of ID
   DWORD    dwID;                // ID of what was drawn. HIWORD = major, LOWORD=minor
   fp   fTextureError;       // Acceptable error (in pixels) for texture map
   CMaterial Material;           // material info
} IDMISC, *PIDMISC;

typedef struct {
   COLORREF cRGB;             // basic color
   fp       fIntensity;       // multuply the un-gammad RGB by this
} TMGLOW, *PTMGLOW;

typedef struct {
   COLORREF cRGB;             // basic color
} TMCOLOR, *PTMCOLOR;

typedef struct {
   WORD     wBump;            // bump value
   BYTE     bSpecExp;         // specular exponent
   BYTE     bSpecScale;       // specular scale
   BYTE     bTrans;           // transparency
   BYTE     bFill;            // filler - does nothing
} TMTRANS, *PTMTRANS;

#define TMIMAGETYPE_TMCOLOR      0     // color map
#define TMIMAGETYPE_TMGLOW       1     // glow map
#define TMIMAGETYPE_TMTRANS      2     // specularity map

typedef struct {
   DWORD    dwX, dwY;   // width and height
   DWORD    dwType;     // See TMIMAGETYPE_XXX
   // followed by array of TMCOLOR or whaever is specified by TMIMAGETYPE_XXX
} TMIMAGE, *PTMIMAGE;

class CTextureMapSocket;
typedef CTextureMapSocket *PCTextureMapSocket;


#define IMAGESCANLINESPERCRITSEC       4     // number of scanlines per critical section
#define IMAGECRITSEC                   (MAXRAYTHREAD*16)  // number of critical sections for scanlines

class CPoint;
class DLLEXPORT CImage {
private:
   CMem     m_memBuffer;   // memory buffer, array of IMAGEPIXEL
   CMem     m_amemZTemp[MAXRAYTHREAD];    // temporary int storage for Z buffer tests. size = m_dwWidth
   CMem     m_amemPolyDivide[MAXRAYTHREAD];  // temporary memory used for polygon divide
   CMem     m_amemLineDivide[MAXRAYTHREAD];  // temporary memory used for line divide
   CMem     m_amemTexture[MAXRAYTHREAD];  // to keep the texture map in temporarily
   CMem     m_amemGlow[MAXRAYTHREAD];     // to keep glow temporaryly
   CMem     m_amemTrans[MAXRAYTHREAD];    // to keep trans temporarily
   DWORD    m_dwWidth;
   DWORD    m_dwHeight;
   HBITMAP  m_hBitCache;   // cache the bitmap from the last paint
   CListFixed m_alistPoly[MAXRAYTHREAD];  // list of lines in the polygon, for DrawPolygon

   // critical sections for scanliens
   BOOL                 m_fCritSecInitialized;        // set to true if CritSecInitialize() has been called
   CRITICAL_SECTION     m_aCritSec[IMAGECRITSEC];     // if m_fCritSecInitialized, then critical sections for scanline access

public:
   ESCNEWDELETE;

   CImage (void);
   ~CImage (void);

   // initialization
   BOOL Init (DWORD dwX, DWORD dwY); // initialzes, or re-initializes. Doesn't clear to a color
   BOOL Init (DWORD dwX, DWORD dwY, COLORREF cColor, fp fZ = ZINFINITE); // initialzes, or re-initializes
   BOOL Init (HBITMAP hBit, fp fZ = ZINFINITE); // initialize from a bitmap
   BOOL InitBitmap (HINSTANCE hInstance, DWORD dwID, fp fZ = ZINFINITE); // initialize from bitmap resource
   BOOL InitJPEG (HINSTANCE hInstance, DWORD dwID, fp fZ = ZINFINITE);   // initialize from JPEG resource
   BOOL Init (PWSTR pszFile, fp fZ = ZINFINITE, BOOL fIgnoreMegaFile = FALSE); // initialize from a file

   void CritSectionInitialize (void);

   BOOL Clear (COLORREF cColor, fp fZ = ZINFINITE); // clears to a color and removes Z buffer
   CImage *Clone (void);         // make a copy of this

   // getting values
   __inline DWORD Width (void) {return m_dwWidth;};
   __inline DWORD Height (void) {return m_dwHeight;};
   __inline PIMAGEPIXEL Pixel (DWORD dwX, DWORD dwY)
      {
         return ((PIMAGEPIXEL)m_memBuffer.p) + (dwX + dwY * m_dwWidth);
      };

   // useful tools
   __inline WORD Gamma (BYTE bColor)   // gamma correct
      {
         return ::Gamma(bColor);
      };
   __inline void Gamma (COLORREF cr, WORD *pRGB)  // gamma correct
      {
         ::Gamma (cr, pRGB);
      };
   __inline BYTE UnGamma (WORD wColor) // un-gamma correct
      {
         return ::UnGamma (wColor);
      };
   __inline COLORREF UnGamma (const WORD *pRGB)     // un-gamma correect
      {
         return ::UnGamma (pRGB);
      };

   // taking image data to bitmaps or the screen
   HBITMAP ToBitmap (HDC hDC);   // creates a bitmap for purposes of drawin on the HDC
   HBITMAP ToBitmapAntiAlias (HDC hDC, DWORD dwDown);
   HBITMAP ToBitmapEnlarge (HDC hDC, DWORD dwScale);
   BOOL Paint (HDC hDC, RECT *prSrc, POINT pDest); // draw onto screen. Creates a temporary bitmap and blits that
   BOOL Paint (HDC hDC, RECT *prSrc, RECT *prDest); // draw onto screen. Creates a temporary bitmap and blits that
   void PaintCacheClear (void);  // clears cache of HBITMAP for paint

   // effects
   void DrawBlock (RECT *pr, COLORREF crUL, COLORREF crUR, COLORREF crLL, COLORREF crLR);
   void DrawPixel (DWORD dwX, DWORD dwY, fp fZ, const WORD *pwColor, const PIDMISC pm);  // pixel
   void DrawLine (PIHLEND pStart, PIHLEND pEnd, PIDMISC pm);  // diagonal line
   void DrawHorzLine (int y, const PIHLEND pLeft, const PIHLEND pRight,
      const PIDMISC pm, BOOL fSolid, const PCTextureMapSocket pTexture,
      fp fGlowScale, DWORD dwTextInfo, DWORD dwThread, BOOL fFinalRender); // horizontal line. pLeft.x <= pRight.x. pXXX.y ignored
   //void DrawFillBetween2Lines (fp yTop, PIHLEND pLeftTop, PIHLEND pRightTop,
   //   fp yBottom, PIHLEND pLeftBottom, PIHLEND pRightBottom,
   //   PIDMISC pm, BOOL fSolid);  // fill between two lines. pXXX.y ignored
   void DrawPolygon (DWORD dwNum, const PIHLEND ppEnd[],
      const PIDMISC pm, BOOL fSolid, const PCTextureMapSocket pTexture, fp fGlowScale,
      BOOL fTriangle, DWORD dwThread, BOOL fFinalRender);  // draw a polygon with color blend
   void DrawPolygonInt (DWORD dwNum, const PIHLEND ppEnd[],
      const PIDMISC pm, BOOL fSolid, const PCTextureMapSocket pTexture, fp fGlowScale,
      DWORD dwThread, DWORD dwTextInfo, BOOL fFinalRender);  // draw a polygon with color blend

   int *TransparencyDither (WORD wTransparency, int iY, DWORD *pdwElem);  // returns a pointer to an array of BOOL used for transparency dithering
   BOOL IsCompletelyCovered (const RECT *pRect, fp fZ);      // returns true if all points in z buffer are higher


   // pasting onto another image
   BOOL Overlay (RECT *prSrc, CImage *pDestImage, POINT pDest, BOOL fOverlayZObject);  // over lay the portion of the src onto the dest
   BOOL Blend (RECT *prSrc, CImage *pDestImage, POINT pDest, WORD wTransparency);  // blend pixels onto destination
   BOOL Merge (RECT *prSrc, CImage *pDestImage, POINT pDest);  // merge, taking Z into account
   void MergeSelected (CImage *pDestImage, BOOL fBlendSelAbove);
   BOOL RBGlassesMerge (CImage *pNonDom, BOOL fNonDomRed);
   void Downsample (CImage *pDown, DWORD dwAnti);

   // texture map generation functions
   void TGDrawHorzLine (int y, PIHLEND pLeft, PIHLEND pRight, DWORD dwZFunc = 1,
                             BOOL fApplyColor = TRUE, BOOL fApplySpecularity = TRUE,
                             WORD wSpecReflection = 0x1000, WORD wSpecDirection = 100);
   void TGDrawPolygon (DWORD dwNum, PIHLEND ppEnd[], DWORD dwZFunc = 1,
                            BOOL fApplyColor = TRUE, BOOL fApplySpecularity = TRUE,
                            WORD wSpecReflection = 0x1000, WORD wSpecDirection = 1000,
                            BOOL fModulo = TRUE);
   void TGDrawPolygonCurved (DWORD dwNum, PIHLEND ppEnd[],
                            WORD wSpecReflection, WORD wSpecDirection,
                            DWORD dwCurvePix, fp fZEdge, fp fZCenter);
   void TGDrawPolygonCurvedExtra (DWORD dwNum, PIHLEND ppEnd[],
                            WORD wSpecReflection, WORD wSpecDirection,
                            DWORD dwCurvePix, fp fZEdge, fp fZCenter,
                            fp fCutCorner);
   PIHLEND ToIHLEND (PIHLEND p, fp x, fp y, fp z, COLORREF c);
   void TGDrawQuad (PIHLEND p1,PIHLEND p2,PIHLEND p3,PIHLEND p4, DWORD dwZFunc = 1,
                            BOOL fApplyColor = TRUE, BOOL fApplySpecularity = TRUE,
                            WORD wSpecReflection = 0x1000, WORD wSpecDirection = 1000);
   BOOL TGMerge (RECT *prSrc, CImage *pDestImage, POINT pDest);
   BOOL TGMergeRotate (RECT *prSrc, CImage *pDestImage, POINT pDest);
   void TGNoise (int iNoiseXSize, int iNoiseYSize, fp fZDeltaMax, fp fZDeltaMin,
                      COLORREF cMax, COLORREF cMin, fp fTransMax, fp fTransMin);
   void TGMarbling (int iNoiseSize, fp fDecay, fp fNoiseContribution,
                         DWORD dwNumLevels, COLORREF *pacLevel, BOOL fNoiseMax);
   void TGBumpMapApply (fp fAmbient = .25, fp fLight = .75, CPoint *pLight = NULL, CPoint *pViewer = NULL);
   void TGColorByZ (fp fZMin, fp fZMax, COLORREF cZMin, COLORREF cZMax,
                         fp fTransMin, fp fTransMax);
   void TGDirtAndPaint (DWORD dwFilterSize, COLORREF cDirt, fp fTransNone,
                             fp fTransA, fp fAHeight,
                             WORD wSpecReflection, WORD wSpecDirection);
   void TGFilterZ (DWORD dwFilterSize, fp fZInc = 0);
   void TGMergeForChipping (CImage *pDest, COLORREF cChip, fp fColorTrans,
                                 WORD wSpecReflection, WORD wSpecDirection);

   BOOL           m_f360;     // set to TRUE if this a 360 degree image
};

typedef CImage * PCImage;

DLLEXPORT BOOL DrawTextOnImage (PCImage pImage, int iHorz, int iVert, HFONT hFont, PWSTR psz);

/************************************************************************
CZImage */

typedef struct {
   float    fZ;                  // z depth
} ZIMAGEPIXEL, *PZIMAGEPIXEL;

// DHLTEXT_XXX - For dwTextInfo in DrawHorzLine
#define DHLTEXT_TRANSPARENT         0x01     // - Transparent texture
#define DHLTEXT_PERPIXTRANS         0x02     // - Transparency might be per pixel
#define DHLTEXT_HV                  0x04     // - Uses HV
#define DHLTEXT_XYZ                 0x08     // - Uses XYZ

class CPoint;
class DLLEXPORT CZImage {
private:
   CMem     m_memBuffer;   // memory buffer, array of ZIMAGEPIXEL
   CMem     m_amemPolyDivide[MAXRAYTHREAD];  // temporary memory used for polygon divide
   CMem     m_amemLineDivide[MAXRAYTHREAD];  // temporary memory used for line divide
   DWORD    m_dwWidth;
   DWORD    m_dwHeight;
   CListFixed m_alistPoly[MAXRAYTHREAD];  // list of lines in the polygon, for DrawPolygon


   // critical sections for scanliens
   BOOL                 m_fCritSecInitialized;        // set to true if CritSecInitialize() has been called
   CRITICAL_SECTION     m_aCritSec[IMAGECRITSEC];     // if m_fCritSecInitialized, then critical sections for scanline access

public:
   ESCNEWDELETE;

   CZImage (void);
   ~CZImage (void);

   // initialization
   BOOL Init (DWORD dwX, DWORD dwY); // initialzes, or re-initializes. Doesn't clear to a color
   BOOL Init (DWORD dwX, DWORD dwY, DWORD dwColor, fp fZ = ZINFINITE); // initialzes, or re-initializes

   void CritSectionInitialize (void);

   BOOL Clear (fp fZ = ZINFINITE); // clears to a color and removes Z buffer
   CZImage *Clone (void);         // make a copy of this

   // getting values
   __inline DWORD Width (void) {return m_dwWidth;};
   __inline DWORD Height (void) {return m_dwHeight;};
   __inline PZIMAGEPIXEL Pixel (DWORD dwX, DWORD dwY)
      {
         return ((PZIMAGEPIXEL)m_memBuffer.p) + (dwX + dwY * m_dwWidth);
      };

   // effects
   void DrawPixel (DWORD dwX, DWORD dwY, fp fZ);  // pixel
   void DrawLine (PIHLEND pStart, PIHLEND pEnd, PIDMISC pm);  // diagonal line
   void DrawHorzLine (int y, const PIHLEND pLeft,const PIHLEND pRight,
      const PIDMISC pm, BOOL fSolid, const PCTextureMapSocket pTexture,
      DWORD dwTextInfo, DWORD dwThread); // horizontal line. pLeft.x <= pRight.x. pXXX.y ignored
   //void DrawFillBetween2Lines (fp yTop, PIHLEND pLeftTop, PIHLEND pRightTop,
   //   fp yBottom, PIHLEND pLeftBottom, PIHLEND pRightBottom,
   //   PIDMISC pm, BOOL fSolid);  // fill between two lines. pXXX.y ignored
   void DrawPolygon (DWORD dwNum, const PIHLEND ppEnd[],
      const PIDMISC pm, BOOL fSolid, const PCTextureMapSocket pTexture,
      BOOL fTriangle, DWORD dwThread);  // draw a polygon with color blend
   void DrawPolygonInt (DWORD dwNum, const PIHLEND ppEnd[],
      const PIDMISC pm, BOOL fSolid, const PCTextureMapSocket pTexture,
      DWORD dwThread, DWORD dwTextInfo);  // draw a polygon with color blend

   BOOL IsCompletelyCovered (const RECT *pRect, fp fZ);      // returns true if all points in z buffer are higher


   // pasting onto another image
   BOOL Overlay (RECT *prSrc, CZImage *pDestImage, POINT pDest, BOOL fOverlayZObject);  // over lay the portion of the src onto the dest
   BOOL Merge (RECT *prSrc, CZImage *pDestImage, POINT pDest);  // merge, taking Z into account
   void MergeSelected (CZImage *pDestImage, BOOL fBlendSelAbove);

   BOOL           m_f360;     // set to TRUE if this a 360 degree image
};

typedef CZImage * PCZImage;



/***********************************************************************************
CFImage.cpp */

// FIMAGEPIXEL - floating point image
typedef struct {
   float    fRed, fGreen, fBlue; // gamma corrected, 0..0xffff
   DWORD    dwIDPart;             // least-signficiant info of ID. No bits on high end
   DWORD    dwID;                // ID of what was drawn. HIWORD = major, LOWORD=minor
   float    fZ;                  // z depth, in meters
} FIMAGEPIXEL, *PFIMAGEPIXEL;

class DLLEXPORT CFImage {
private:
   CMem     m_memBuffer;   // memory buffer, array of FIMAGEPIXEL
   DWORD    m_dwWidth;
   DWORD    m_dwHeight;


public:
   ESCNEWDELETE;

   CFImage (void);
   ~CFImage (void);

   // initialization
   BOOL Init (DWORD dwX, DWORD dwY); // initialzes, or re-initializes. Doesn't clear to a color
   BOOL Init (DWORD dwX, DWORD dwY, COLORREF cColor, fp fZ = ZINFINITE); // initialzes, or re-initializes
   BOOL Init (HBITMAP hBit, fp fZ = ZINFINITE); // initialize from a bitmap
   BOOL InitBitmap (HINSTANCE hInstance, DWORD dwID, fp fZ = ZINFINITE); // initialize from bitmap resource
   BOOL InitJPEG (HINSTANCE hInstance, DWORD dwID, fp fZ = ZINFINITE);   // initialize from JPEG resource
   BOOL Init (PWSTR pszFile, fp fZ = ZINFINITE); // initialize from a file

   BOOL Clear (COLORREF cColor, fp fZ = ZINFINITE); // clears to a color and removes Z buffer
   CFImage *Clone (void);         // make a copy of this

   // getting values
   __inline DWORD Width (void) {return m_dwWidth;};
   __inline DWORD Height (void) {return m_dwHeight;};
   __inline PFIMAGEPIXEL Pixel (DWORD dwX, DWORD dwY)
      {
         return ((PFIMAGEPIXEL)m_memBuffer.p) + (dwX + dwY * m_dwWidth);
      };

   // taking image data to bitmaps or the screen
   HBITMAP ToBitmap (HDC hDC);   // creates a bitmap for purposes of drawin on the HDC
   void Downsample (CFImage *pDown, DWORD dwAnti);

   void MergeSelected (CFImage *pDestImage, BOOL fBlendSelAbove);

   BOOL           m_f360;     // set to TRUE if this a 360 degree image
};

typedef CFImage * PCFImage;


/***********************************************************************************
CRenderMatrix */

#define  PI    (3.14159265358979323846)
#define  TWOPI (2.0 * PI)

/*********************************************************************************
AreClose - Returns TRUE if two texturepoints are close to one another. EPSILON disntance

inputs
   PTEXTUREPOINT     p1, p2 - two texture points.
retrurns
   BOOL - TRUE if they're close, FALSE if further away than epsilon
*/
__inline BOOL AreClose (PTEXTUREPOINT p1, PTEXTUREPOINT p2)
{
   if ((p1->h - CLOSE < p2->h) && (p1->h + CLOSE > p2->h) &&
      (p1->v - CLOSE < p2->v) && (p1->v + CLOSE > p2->v))
      return TRUE;
   return FALSE;
}


/*********************************************************************************
AreClose - Returns TRUE if two texturepoints are close to one another. EPSILON disntance

inputs
   PTEXTPOINT5     p1, p2 - two texture points.
retrurns
   BOOL - TRUE if they're close, FALSE if further away than epsilon
*/
__inline BOOL AreClose (PTEXTPOINT5 p1, PTEXTPOINT5 p2)
{
   DWORD i;
   for (i = 0; i < 2; i++)
      if ((p1->hv[i] - CLOSE >= p2->hv[i]) || (p1->hv[i] + CLOSE <= p2->hv[i]))
         return FALSE;
   for (i = 0; i < 3; i++)
      if ((p1->xyz[i] - CLOSE >= p2->xyz[i]) || (p1->xyz[i] + CLOSE <= p2->xyz[i]))
         return FALSE;

   return TRUE;
}

/*******************************************************************
randf - Floating point random

inputs
   fp   fMin, fMax - Minimum and maximum
returns
   fp -value
*/
inline fp randf (fp fMin, fp fMax)
{
   int i = rand() % 10000;

   return (fp)(fMin + (fMax - fMin) * (i / 10000.0));
}

/*******************************************************************
myfmod - Does an fmod the right way.

inputs
   fp   x,y - x mod y
returns
   fp - value
*/
inline fp myfmod (fp x, fp y)
{
   // BUGFIX - Quick escape if already within range
   if ((x >= 0) && (x < y))
      return x;

   return  (fp)(x - floor (x/y) * y);
}

/*******************************************************************
myimod - Does an fmod the right way.

inputs
   int   x,y - x mod y
returns
   int - value
*/
inline int myimod (int x, int y)
{
   if (x < 0) {
      // BUGFIX - Wasn't taking into acount that i==y case before
      int i;
      i = y - ((-x)%y);
      if (i == y)
         i = 0;
      return i;
   }
   else
      return x % y;
}


/*************************************************************************************
Intersect.cpp*/

DLLEXPORT DWORD IntersectRayCylinder (PCPoint pRA, PCPoint pRB, PCPoint pCA, PCPoint pCB,
                            fp fRadius, fp *pfAlpha1, fp *pfAlpha2);
DLLEXPORT DWORD IntersectRaySpehere (PCPoint pStart, PCPoint pDir, PCPoint pCenter,
                           fp fRadius, fp *pft1, fp *pft2);
DLLEXPORT fp HermiteCubic (fp t, fp fp1, fp fp2, fp fp3, fp fp4);
DLLEXPORT fp DistancePointToPoint (PCPoint pP1, PCPoint pP2);
DLLEXPORT fp DistancePointToLine (PCPoint pPoint, PCPoint pLineStart, PCPoint pLineVector, PCPoint pNearest);
DLLEXPORT fp DistancePointToPlane (PCPoint pPoint, PCPoint pPlaneAnchor, PCPoint pHVector, PCPoint pVVector, PCPoint pNearest);
DLLEXPORT BOOL IntersectLinePlane (PCPoint pLineStart, PCPoint pLineEnd, PCPoint pPlane,
                         PCPoint pPlaneNormal, PCPoint pIntersect);
DLLEXPORT BOOL IntersectLineTriangle (PCPoint pLineStart, PCPoint pSub,
                         PCPoint p1, PCPoint p2, PCPoint p3,
                         PCPoint pIntersect = NULL, fp *pfAlpha = NULL, PTEXTUREPOINT pHV = NULL,
                         DWORD dwTestBits = 0x7);
DLLEXPORT BOOL IntersectLineQuad (PCPoint pLineStart, PCPoint pSub,
                        PCPoint pUL, PCPoint pUR, PCPoint pLR, PCPoint pLL,
                        PCPoint pIntersect = NULL, fp *pfAlpha = NULL,
                        PTEXTUREPOINT pHV = NULL, BOOL *pfIntersectA = NULL,
                        DWORD dwFlags = 0);
DLLEXPORT BOOL IntersectTwoPolygons (PCPoint pA, PTEXTUREPOINT patA, DWORD dwA,
                           PCPoint pB, DWORD dwB, PCListFixed pl);
DLLEXPORT PCListFixed LineSegmentsToSequences (PCListFixed plLines);
DLLEXPORT void LinesMinimize (PCListFixed plLines, fp fExtend);
DLLEXPORT void LineSequencesRemoveHanging (PCListFixed pl, BOOL fAllowBothSides = FALSE);
DLLEXPORT void LineSequencesSplitHoriz (PCListFixed pl);
DLLEXPORT void LineSequencesSortHorz (PCListFixed pl);
DLLEXPORT PCListFixed LineSegmentsForWall (PCListFixed plLine);
DLLEXPORT PCListFixed LineSegmentsFromWallToPoints (PCListFixed plLine);
DLLEXPORT PCListFixed LineSequencesForRoof (PCListFixed plLines, BOOL fClockwise,
                                  BOOL fAllowBothSides = FALSE, BOOL fPerimeter = TRUE);
DLLEXPORT void PathFromSequences (PCListFixed pl, PCListVariable pListPath, DWORD *padwHistory,
                        DWORD dwNum, DWORD dwMax, PTEXTUREPOINT pInPath = NULL);
DLLEXPORT DWORD PathWithLowestScore (PCListFixed pListSequences, PCListVariable pListPaths, PCListFixed plScores);
DLLEXPORT DWORD PathWithHighestScore (PCListFixed pListSequences, PCListVariable pListPaths, PCListFixed plScores);
DLLEXPORT PCListFixed PathToSplineList (PCListFixed pListSequences, PCListVariable pListPaths,
                                    DWORD dwPathNum, BOOL fReverse);
DLLEXPORT BOOL IsCurveClockwise (PTEXTUREPOINT pt, DWORD dwNum);
DLLEXPORT PCListFixed PathToSplineList (PCListFixed pListSequences, DWORD *padwPath, DWORD dwNum,
                              BOOL fClockwise = TRUE);
DLLEXPORT PCListFixed PathToSplineListFillInGaps (PCListFixed pListSequences, DWORD *padwPath, DWORD dwNum,
                              BOOL fClockwise = TRUE);
DLLEXPORT PCListFixed SequencesScores (PCListFixed pl, BOOL fKeepOnLeft);
DLLEXPORT PCListFixed SequencesSortByScores (PCListFixed pl, BOOL fKeepOnLeft, PTEXTUREPOINT pCenter = NULL);
DLLEXPORT void SequencesLookForFlips (PCListFixed plSeq);
DLLEXPORT void SequencesAddFlips (PCListFixed plSeq);
DLLEXPORT int SequenceRightCount (PTEXTUREPOINT pCheck, PTEXTUREPOINT paSeq, DWORD dwNum);
DLLEXPORT int SequenceRightCount (PTEXTUREPOINT pCheck, DWORD *padwPath, DWORD dwNum, PCListFixed plSeq);
DLLEXPORT BOOL Intersect2DLineWith2DPoly (PTEXTUREPOINT pl1, PTEXTUREPOINT pl2,
                                PTEXTUREPOINT pp1, PTEXTUREPOINT pp2, PTEXTUREPOINT pp3, PTEXTUREPOINT pp4,
                                PTEXTUREPOINT pnl1, PTEXTUREPOINT pnl2,
                                PTEXTUREPOINT ps1, PTEXTUREPOINT ps2);
DLLEXPORT fp PointOn2DLine (PTEXTUREPOINT p, PTEXTUREPOINT p1, PTEXTUREPOINT p2);
DLLEXPORT DWORD IntersectLineSphere (PCPoint pStart, PCPoint pDir, PCPoint pCenter, fp fRadius,
                           PCPoint pt0, PCPoint pt1);
DLLEXPORT DWORD IntersectNormLineSphere (PCPoint pStart, PCPoint pDir, PCPoint pCenter,
                           fp *pafAlpha);
DLLEXPORT BOOL MakePointCoplanar (PCPoint pPoint, PCPoint pPlane,
                         PCPoint pPlaneNormal, PCPoint pIntersect);
DLLEXPORT BOOL Intersect2DLineWithBox (PTEXTUREPOINT pl1, PTEXTUREPOINT pl2,
                             PTEXTUREPOINT pnl1, PTEXTUREPOINT pnl2);
DLLEXPORT BOOL ThreePointsToCircle (PCPoint p1, PCPoint p2, PCPoint p3,
                          PCMatrix pm, fp *pfAngle1, fp *pfAngle2, fp *pfRadius);
DLLEXPORT BOOL IntersectBoundingBox (PCPoint pStart, PCPoint pDir, PCPoint pBBMin, PCPoint pBBMax);


class DLLEXPORT CRenderMatrix {
public:
   ESCNEWDELETE;

   CRenderMatrix (void);
   ~CRenderMatrix (void);
   void ClearAll (void);
   void CloneTo (CRenderMatrix *pClone);
   PCMMLNode2 MMLTo (void);
   BOOL MMLFrom (PCMMLNode2 pNode);

   void Push (void);
   BOOL Pop (void);
   void Set (const PCMatrix pSet);
   void Get (PCMatrix pGet);
   void Multiply (const PCMatrix pm);
   void Translate (fp x = 0.0, fp y = 0.0, fp z = 0.0);   // translates in x,y,z
   void Rotate (fp fRadians, DWORD dwDim);  // Rotate. dwdim = 1 for x, 2 for y, 3 for z
   void Scale (fp fScale);   // universally scale
   void Scale (fp x, fp y, fp z);   // scale in x, y, z
   void TransRot (const PCPoint p1, const PCPoint p2);
   void Transform (const PCPoint pSrc, PCPoint pDest);
   void Transform (DWORD dwNum, PCPoint apPoints);  // bulk transformation of points
   void TransformNormal (const PCPoint pSrc, PCPoint pDest, BOOL fNormalize = TRUE);
   void TransformNormal (DWORD dwNum, PCPoint apPoints, BOOL fNormalize = TRUE); // bulk transformation of points
   void TransformViewSpaceToWorldSpace (const PCPoint pSrc, PCPoint pDest);
   void Perspective (fp fFOV = PI/4, fp fZNear = 0, fp fZFarInv = 0);
   void PerspectiveClear (void);
   void PerspectiveScale (fp fX, fp fY, fp fZ);
   void ScreenInfo (DWORD dwWidth, DWORD dwHeight, fp fXOffset = 0);
   void Clear (void);
   void CTMGet (PCMatrix pm) {pm->Copy (&m_CTM);};
   void GetPerspScale (PCMatrix pPerspScale);

private:
   CListFixed     m_listStack;
   CMatrix        m_CTM;     // objects matrix
   CMatrix        m_mainMatrix; // = perMatrix * camera * CTM
   CMatrix        m_persMatrix; // perspective matrix
   CMatrix        m_scaleMatrix; // scaling matrix for windows
   CMatrix        m_transMatrixGlasses;   // translation matrix for 3d glasses
   CMatrix        m_perspNScale;   // combination of perspective and scaling
   CMatrix        m_normalMatrix;  // multiply your normals by this to calc them
   CMatrix        m_CTMInverse4;    // 4x4 inverted matrix of CTM
   BOOL           m_fCTMInverse4Valid;  // set to true if 4x4 inverted CTM is valid
   BOOL           m_fCTMDirty;            // set to TRUE if CTM or anything generating main matrix has changed, but main not recalculated
   BOOL           m_fMatrixPerspective;   // set to true if the perspective matrix is non-identity
   BOOL           m_fMatrixCamera;        // set to true if the camera matrix is non-identity
   BOOL           m_fMatrixScale;         // set to tur if matrix is non-identity
   BOOL           m_fMatrixPerspNScale;   // set to true if m_perspnScale is non-identity
   BOOL           m_fMatrixNormalValid;   // if normal matrix is valid or not

   void           MakeMainMatrix (void);  // makes the main matrix if it isn't made already
   void           MakeNormalMatrix (void);   // makes the normal matrix if it's not valid
   void           MakeInverse4Matrix (void);
};
typedef CRenderMatrix *PCRenderMatrix;


/***********************************************************************************
CImageShader */

typedef struct {
   WORD     wRed, wGreen, wBlue;       // red, green, blue.
   WORD     wFill;                     // not used
   DWORD    dwIDPart;                  // least-signficiant info of ID
   DWORD    dwID;                      // ID of what was drawn. HIWORD = major, LOWORD=minor
   PCTextureMapSocket pTexture;              // texture. NULL if no texture used.
   CMaterial Material;                 // material info
} ISDMISC, *PISDMISC;

typedef struct {
   float       fZ;                  // z depth
   // int      iZ;                  // z depth. In 1/65536 of a meter. Positive values.
   short       aiNorm[4];           // normal, xyz values. [3] ifnored. / 32767.0 for value
   TEXTPOINT5  tpText;          // texture at this point
   PISDMISC    pMisc;            // misc info
   // DWORD    dwMiscIndex;         // index, 0 based, into the object information. (Note that 0 is always the background.)
   PVOID       pTrans;          // transparency info, NULL if none, of type PISPIXEL
   // DWORD    dwTransIndex;        // index, 0 based, intto transparency pixel list. (Note that 0 means no transparencies)
} ISPIXEL, *PISPIXEL;

typedef struct {
   fp       x; // in pixels. Might be off the display
   fp       y; // y value. Not used in all the calls.
   fp       z; // positive number. depth in meters
   fp       w; // w-value. Used to make sure that perspective is applied to z, color, and texture
   CPoint   pNorm;   // normal for the endpoint
   TEXTPOINT5 tp;  // texture point
} ISHLEND, *PISHLEND;


class DLLEXPORT CImageShader {
private:
   CMem     m_memBuffer;   // memory buffer, array of ISPIXEL
   CMem     m_amemZTemp[MAXRAYTHREAD];    // temporary int storage for Z buffer tests. size = m_dwWidth
   CMem     m_amemPolyDivide[MAXRAYTHREAD];  // temporary memory used for polygon divide
   CMem     m_amemLineDivide[MAXRAYTHREAD];  // temporary memory used for line divide
   CMem     m_amemTexture[MAXRAYTHREAD];  // to keep the texture map in temporarily
   CListVariable m_alMisc[MAXRAYTHREAD];     // translates ISPIXEL.dwMiscIndex into memory for ISDMISC
   CListVariable m_alTrans[MAXRAYTHREAD];    // list of ISPIXEL structures for transparency
   DWORD    m_dwWidth;
   DWORD    m_dwHeight;
   DWORD    m_adwMisc[MAXRAYTHREAD];      // number of misc allocated
   DWORD    m_adwTrans[MAXRAYTHREAD];     // number of transparency pixels allocated
   CListFixed m_alistPoly[MAXRAYTHREAD];  // list of lines in the polygon, for DrawPolygon

   PISDMISC MiscAdd (const PISDMISC pMisc, DWORD dwThread);
   void DrawTransparentPixel (PISPIXEL pip, const PISPIXEL pTrans,
      const PCTextureMapSocket pTexture, const PCMaterial pMaterial,
      const PTEXTPOINT5 pPixelSize, DWORD dwThread);
   void DrawOpaquePixel (PISPIXEL pip, const PISPIXEL pTrans, DWORD dwThread);

   // critical sections for scanliens
   BOOL                 m_fCritSecInitialized;        // set to true if CritSecInitialize() has been called
   CRITICAL_SECTION     m_aCritSec[IMAGECRITSEC];     // if m_fCritSecInitialized, then critical sections for scanline access

public:
   ESCNEWDELETE;

   CImageShader (void);
   ~CImageShader (void);

   // initialization
   BOOL InitNoClear (DWORD dwX, DWORD dwY); // initialzes, or re-initializes. Doesn't clear to a color
   BOOL Init (DWORD dwX, DWORD dwY, fp fZ = ZINFINITE); // initialzes, or re-initializes

   void CritSectionInitialize (void);

   BOOL Clear (fp fZ = ZINFINITE); // clears to a color and removes Z buffer
   CImageShader *Clone (void);         // make a copy of this

   // getting values
   __inline DWORD Width (void) {return m_dwWidth;};
   __inline DWORD Height (void) {return m_dwHeight;};
   __inline PISPIXEL Pixel (DWORD dwX, DWORD dwY)
      {
         return ((PISPIXEL)m_memBuffer.p) + (dwX + dwY * m_dwWidth);
      };
   // __inline PISDMISC MiscGet (DWORD dwIndex, DWORD dwThread)
   //    {
   //       return ((PISDMISC)m_amemMisc[dwThread].p) + dwIndex;
   //    };
   // __inline PISDMISC MiscGetWithThread (DWORD dwIndex)
   //    {
   //       return ((PISDMISC)m_amemMisc[dwIndex >> 24].p) + (dwIndex & 0xffffff);
   //    };
   // __inline PISPIXEL TransPixel (DWORD dwIndex, DWORD dwThread)
   //    {
   //       return ((PISPIXEL)m_amemTrans[dwThread].p) + dwIndex;
   //    };
   // __inline PISPIXEL TransPixelWithThread (DWORD dwIndex)
   //    {
   //       return ((PISPIXEL)m_amemTrans[dwIndex >> 24].p) + (dwIndex & 0xffffff);
   //    };

   // effects
   void DrawPixel (DWORD dwX, DWORD dwY, fp fZ, const PISDMISC pm, DWORD dwThread);  // pixel
   void DrawLine (PISHLEND pStart, PISHLEND pEnd, PISDMISC pm, DWORD dwThread);  // diagonal line
   void DrawHorzLine (int y, const PISHLEND pLeft, const PISHLEND pRight,
      // no longer using PTEXTUREPOINT ptDeltaLeft, PTEXTUREPOINT ptDeltaRight,
      const PISDMISC pm, const PTEXTPOINT5 ptPixelSize,
      DWORD dwTextInfo, DWORD dwThread); // horizontal line. pLeft.x <= pRight.x. pXXX.y ignored
   void DrawPolygon (DWORD dwNum, const PISHLEND ppEnd[],
      const PISDMISC pm, BOOL fTriangle, DWORD dwThread);  // draw a polygon with color blend
   void DrawPolygonInt (DWORD dwNum, const PISHLEND ppEnd[],
      const PISDMISC pm, DWORD dwThread, DWORD dwTextInfo);  // draw a polygon with color blend

   BOOL IsCompletelyCovered (const RECT *pRect, fp fZ);      // returns true if all points in z buffer are higher


   DWORD    m_dwMaxTransparent;        // maximum layers of transparency, 1+. Defaults to 4
};

typedef CImageShader * PCImageShader;

/*********************************************************************************
CSpline */

#define SEGCURVE_LINEAR          0
#define SEGCURVE_CUBIC           1
#define SEGCURVE_CIRCLENEXT      2
#define SEGCURVE_CIRCLEPREV      3
#define SEGCURVE_ELLIPSENEXT     4
#define SEGCURVE_ELLIPSEPREV     5
#define SEGCURVE_MAX             5     // maximum value for segment curve

DLLEXPORT DWORD SegCurve (DWORD dwIndex, DWORD *padwSegCurve);

class DLLEXPORT CSpline {
public:
   ESCNEWDELETE;

   CSpline (void);
   ~CSpline (void);

   BOOL Init (BOOL fLooped, DWORD dwPoints, PCPoint paPoints, PTEXTUREPOINT paTextures,
      DWORD *padwSegCurve, DWORD dwMinDivide = 0, DWORD dwMaxDivide = 4,
      fp fDetail = .1);

   CSpline *Expand (fp *pafExpand, PCPoint pUpDir = NULL);
   CSpline *Expand (fp fExpand, PCPoint pUpDir = NULL);
   CSpline *Flip (void);
   BOOL ExtendEndpoint (BOOL fStart, fp fDist);
   fp FindCrossing (DWORD dwPlane, fp fValue);

   DWORD QueryNodes(void);
   BOOL  QueryLooped (void);
   PCPoint LocationGet (DWORD dwH);
   PTEXTUREPOINT TextureGet (DWORD dwH);
   PCPoint TangentGet (DWORD dwH, BOOL fRight);
   BOOL TangentGet (fp fH, PCPoint pVal);
   BOOL LocationGet (fp fH, PCPoint pP);
   BOOL TextureGet (fp fH, PTEXTUREPOINT pT);

   // To/FromMML, and cloning
   PCMMLNode2 MMLTo (void);
   BOOL MMLFrom (PCMMLNode2 pNode);
   BOOL CloneTo (CSpline *pCloneTo);

   // get original data
   DWORD OrigNumPointsGet (void);
   BOOL LoopedGet (void);
   BOOL LoopedSet (BOOL fLoop);
   BOOL OrigPointGet (DWORD dwH, PCPoint pP);
   BOOL OrigTextureGet (DWORD dwH, PTEXTUREPOINT pT);
   BOOL OrigSegCurveGet (DWORD dwH, DWORD *pdwSegCurve);
   BOOL OrigPointReplace (DWORD dwIndex, PCPoint pNew);
   void DivideGet (DWORD *pdwMinDivide, DWORD *pdwMaxDivide, fp *pfDetail);
   void DivideSet (DWORD dwMinDivide, DWORD dwMaxDivide, fp fDetail);
   BOOL ToMem (BOOL *pfLooped, DWORD *pdwPoints, PCMem pMemPoints, PCMem pMemTextures,
                     PCMem pMemSegCurve, DWORD *pdwMinDivide, DWORD *pdwMaxDivide, fp *pfDetail);

private:
   BOOL Recalc (void);

   BOOL        m_fLoop;             // TRUE if it's a loop.

   // original information
   DWORD       m_dwOrigPoints;      // original points
   CMem        m_memOrigPoints;     // original number of points, array of CPoint, m_dwOrigPointsWorth
   CMem        m_memOrigTextures;   // original texture values, array of TEXTUREPOINT, m_dwOrigPoints worth, +1 if m_fLoop
   CMem        m_memOrigSegCurve;   // original segment curve, array of DWORD's. m_dwOrigPoints worth, -1 if !m_fLoop
   DWORD       m_dwMinDivide;       // minimum divide setting
   DWORD       m_dwMaxDivide;       // maximum divide setting
   fp      m_fDetail;           // detail setting

   // generated after splining
   DWORD       m_dwPoints;          // number of points after splining
   CMem        m_memPoints;         // array of m_dwPoints elem of CPoints
   CMem        m_memTexture;        // if given texture, and array of m_dwPoints(+1 if m_fLoop) of TEXTUREPOINT
   CMem        m_memTangentStart;    // array of points for tangent at the start of the segment
   CMem        m_memTangentEnd;     // array of points for tangent at end of segment
   PCPoint     m_pPoints;           // pointer to m_memPoints.p
   PTEXTUREPOINT m_pTexture;        // pointer to m_memTexture.p, or NULL if no textures
   PCPoint     m_pTangentStart;      // m_memTansgetStart.p
   PCPoint     m_pTangentEnd;     // m_memTangentEnd.p
};
typedef CSpline * PCSpline;



/********************************************************************************
CSplineSurface - Spline surface
*/

class CRenderSurface;
typedef CRenderSurface *PCRenderSurface;

/* CMeshGrid - Represents one quadrialteral in mesh and used for clipping*/
// MGLINE - Keeps track of lines running through grid
typedef struct {
   TEXTUREPOINT      atHV[2];     // tHV[0] is the top, tHV[1] is the bottom
   CPoint            apPosn[2];  // position of each point in world space
   CPoint            apNorm[2];  // normal of each point, normalized
   TEXTUREPOINT      atText[2];  // texture at each point
   BOOL              fRight;     // if true, cutout to the right, else to the left.
} MGLINE, *PMGLINE;


class DLLEXPORT CMeshGrid {
public:
   ESCNEWDELETE;

   CMeshGrid (void);
   ~CMeshGrid (void);

   BOOL Init (fp fHleft, fp fHRight, fp fVTop, fp fVBottom,
      PCPoint pUL, PCPoint pUR, PCPoint pLL, PCPoint pLR,
      PCPoint nUL, PCPoint nUR, PCPoint nLL, PCPoint nLR,
      PTEXTUREPOINT ptUL, PTEXTUREPOINT ptUR, PTEXTUREPOINT ptLL, PTEXTUREPOINT ptLR);
   
   BOOL IntersectLineWithGrid (PTEXTUREPOINT p1, PTEXTUREPOINT p2);
   BOOL GenerateQuads (void); // after caller intersectlinewithgrid(), generates polygons
   BOOL DrawQuads (PCRenderSurface pSurf, BOOL fBackfaceCull = TRUE, BOOL fIgnoreCutouts = FALSE); // draw the quads
   void Point (fp h, fp v, PCPoint pP = NULL, PCPoint pN = NULL,
      PTEXTUREPOINT pT = NULL);   // generate point given h and v inside grid
   BOOL TickIncrement (int iInc);

   // intersections
   BOOL IntersectLineWithGrid (PCPoint pStart, PCPoint pDirection, PTEXTUREPOINT ptp,
      fp *pfAlpha, BOOL fIncludeCutouts);
   BOOL FindLongestSegment (PTEXTUREPOINT pStart, PTEXTUREPOINT pEnd,
      PTEXTUREPOINT pFoundStart, PTEXTUREPOINT pFoundEnd);
   BOOL FindLongestIntersectSegment (PTEXTUREPOINT pStart, PTEXTUREPOINT pEnd,
      PTEXTUREPOINT pFoundStart, PTEXTUREPOINT pFoundEnd);
   BOOL FindMinMaxHV (PTEXTUREPOINT pMin, PTEXTUREPOINT pMax);

   // do not change the following yourself. You can read the values though
   CPoint            m_apPosn[2][2];      // Corner position in object space, [y=0..1][x=0..1]
   TEXTUREPOINT      m_aText[2][2];       // texture map at corners
   fp            m_afH[2];            // h (from 0..1) at left and right edges, h[0] < h[1]
   fp            m_afV[2];            // v (from 0..1) at top and bottom edges, v[0] < v[1]
                        // dont cahnge m_apPosn yourself

   // created by GenerateQuadss(). Can be examined, but shouldnt be modified by outside callers
   CListFixed        m_listMGQUAD;        // quadrilaterals generated by IntersectLineWithGrid
   BOOL              m_fTrivialClip;      // everything was cut out.m_listMGQuad empty
   BOOL              m_fTrivialAccept;    // the whole section is still in. m_listMGQuad empty

private:
   DWORD TickBisect (PCListFixed pl, fp fValue);   // inserts a new tick for fValue in the MGTICK list, keeps old icount
   BOOL TickIncrement (PCListFixed pl, fp fMin, fp fMax, int iInc); // increments tick count from fMin to fMax
   BOOL TickMinimize (PCListFixed pl);  // looks for ticks following one afte the other with the same value and eliminates the second
   fp IntersectTwoLines (PMGLINE pl1, PMGLINE pl2); // returns V where intersect, or -1 if don't
   BOOL IntersectLinesAndTick (void);     // intersects all lines with each other and creates ticks for them
   BOOL GenerateQuads (int iCutOut, fp fVTop, fp fVBottom);   // generates all the quads between two points - assumed
                                                         // that intersection lines are wholly within


   CPoint            m_apNorm[2][2];      // noramls at corners

   CListFixed        m_listMGLINE;        // list of lines
   CListFixed        m_listMGTICK;        // cutout values on left edge of grid

   CListFixed        m_lFindLongestSegmentFound;   // reduce number of rendered mallocs
};
typedef CMeshGrid *PCMeshGrid;


class DLLEXPORT CSplineSurface {
public:
   ESCNEWDELETE;

   CSplineSurface (void);
   ~CSplineSurface (void);

   // to set up the system
   BOOL ControlPointsSet (BOOL fLoopH, BOOL fLoopV, DWORD dwControlH, DWORD dwControlV,
      PCPoint paPoints, DWORD *padwSegCurveH, DWORD *padwSegCurveV);
   BOOL TextureInfoSet (fp m[2][2]);
   BOOL DetailSet (DWORD dwMinDivideH, DWORD dwMaxDivideH, DWORD dwMinDivideV, DWORD dwMaxDivideV,
      fp fDetail = .1);
   BOOL CutoutSet (PWSTR pszName, PTEXTUREPOINT paPoints, DWORD dwNum, BOOL fClockwise);
   BOOL CutoutRemove (PWSTR pszName);
   BOOL CutoutEnum (DWORD dwIndex, PWSTR *ppszName, PTEXTUREPOINT *ppaPoints, DWORD *pdwNum, BOOL *pfClockwise);
   DWORD CutoutNum (void);
   BOOL OverlaySet (PWSTR pszName, PTEXTUREPOINT paPoints, DWORD dwNum, BOOL fClockwise);
   BOOL OverlayRemove (PWSTR pszName);
   BOOL OverlayEnum (DWORD dwIndex, PWSTR *ppszName, PTEXTUREPOINT *ppaPoints, DWORD *pdwNum, BOOL *pfClockwise);
   DWORD OverlayNum (void);

   // read the values gotten
   DWORD ControlNumGet (BOOL fH);
   DWORD QueryNodes (BOOL fH);
   BOOL LoopGet (BOOL fH);
   BOOL LoopSet (BOOL fH, BOOL fLoop);
   BOOL ControlPointGet (DWORD dwH, DWORD dwV, PCPoint pVal);
   BOOL ControlPointSet (DWORD dwH, DWORD dwV, PCPoint pVal);
   DWORD SegCurveGet (BOOL fH, DWORD dwIndex);
   BOOL SegCurveSet (BOOL fH, DWORD dwIndex, DWORD dwValue);
   BOOL TextureInfoGet (fp m[2][2]);
   void DetailGet (DWORD *pdwMinDivideH, DWORD *pdwMaxDivideH, DWORD *pdwMinDivideV, DWORD *pdwMaxDivideV,
      fp *pfDetail);

   // drawing
   BOOL Render (CRenderSurface *pSurf, BOOL fBackfaceCull = TRUE, BOOL fWithCutout = TRUE,
      DWORD dwOverlay = 0);   // draws it to the surface object
   void QueryBoundingBox (PCPoint pCorner1, PCPoint pCorner2);

   // To/FromMML, and cloning
   PCMMLNode2 MMLTo (void);
   BOOL MMLFrom (PCMMLNode2 pNode);
   BOOL CloneTo (CSplineSurface *pCloneTo);
   CSplineSurface *CloneAndExtend (fp fExtendL, fp fExtendR, fp fExtendU, fp fExtendD, BOOL fFlip,
      BOOL fRemoveCutouts = TRUE);

   // Intersection apis
   BOOL IntersectLine (PCPoint pStart, PCPoint pDirection, PTEXTUREPOINT ptp, BOOL fIncludeCutouts,
      BOOL fPositiveAlpha);
   PCListFixed ArchToPoints (DWORD dwNum, PCPoint paFront, PCPoint paBack);
   BOOL ArchToCutout (DWORD dwNum, PCPoint paFront, PCPoint paBack, PWSTR pszCutout, BOOL fOverlay = FALSE);
   BOOL HVToInfo (fp h, fp v, PCPoint pPoint = NULL, PCPoint pNorm = NULL, PTEXTUREPOINT pText = NULL);
   BOOL HVDrag (PTEXTUREPOINT pTextOld, PCPoint pNew, PCPoint pViewer, PTEXTUREPOINT pTextNew);
   void IntersectLoopWithGrid (PTEXTUREPOINT pLoop, DWORD dwNum, BOOL fLooped, PCListFixed pl, BOOL fReverse);
   PCListFixed IntersectWithSurfaces (fp fExtend, DWORD dwNum, CSplineSurface **papss, PCMatrix pam,
      BOOL fIgnoreCutouts = FALSE);
   BOOL FindLongestSegment (PTEXTUREPOINT pStart, PTEXTUREPOINT pEnd,
      PTEXTUREPOINT pFoundStart, PTEXTUREPOINT pFoundEnd, BOOL fEdgeOnly = TRUE);
   BOOL FindMinMaxHV (PTEXTUREPOINT pMin, PTEXTUREPOINT pMax);
   BOOL IntersectWithSurfacesAndCutout (CSplineSurface **papss,
      PCMatrix paMatrix, DWORD dwNum, PWSTR pszName, BOOL fIgnoreCutouts,
      BOOL fClockwise, DWORD dwMode, BOOL fUseMin, BOOL fFinalClockwise,
      PTEXTUREPOINT pKeep);

   // misc
   BOOL ControlPointsToNormals (DWORD dwX, DWORD dwY, PCPoint pNormal = NULL, PCPoint pH = NULL, PCPoint pV = NULL);

private:
   void MakeSureTexture(void);   // called to make sure textures are built
   void MakeSureCutouts(void);   // called to make sure cutout information is built
   void MakeSureSpline(void);    // make sure the mesh is built
   void IntersectSemiPlaneBorder (PCPoint p1, PCPoint p2, PCPoint p3, PCListFixed pl,
      DWORD dwIntersectFlags = 0x01 | 0x04 | 0x08);
   BOOL IntersectPlaneBorderClosest (PCPoint p1, PCPoint p2, PCPoint p3, PCPoint p4,
                                                  PTEXTUREPOINT ptp);
   BOOL CutoutOfGrid (PTEXTUREPOINT pt, DWORD dwNum, BOOL fClockwise, BOOL fReverse, PCListFixed plMG);

   BOOL        m_fSplineDirty;   // set to TRUE if the spline has changed since last use
   BOOL        m_fCutoutsDirty;  // set to TRUE if the cutout information has changed since last use
   BOOL        m_fTextureDirty;  // set to TRUE if the texture information is dirty

   BOOL        m_fLoopH;         // TRUE if loop along horizontal
   BOOL        m_fLoopV;         // TRUE if loop along vertical
   DWORD       m_dwControlH;     // width (in points) along H
   DWORD       m_dwControlV;     // width (in points) along V
   CMem        m_memControlPoints;  // memory containing control points
   CMem        m_memSegCurveH;    // memory containing segment curve information
   CMem        m_memSegCurveV;   // memory containing segment curve information
   PCPoint     m_paControlPoints;   // [0..m_dwControlV-1][0..m_dwControlH-1] array fo points in m_memControlPoints
   DWORD       *m_padwSegCurveH; // pointing to segment curveh (filled in even if ControlPointsSet() wasnt)
   DWORD       *m_padwSegCurveV; // pointing to segment curvev (filled in even if ControlPoitnsSet() wasnt)
   
   CListVariable m_listCutoutNames; // list of null-termindated strings
   CListVariable m_listCutoutPoints; // list of list of points (TEXTUREPOINT)
   CListFixed    m_listCutoutClockwise;   // BOOL - TRUE if the cutout is clockwise
   CListVariable m_listOverlayNames; // list of null-termindated strings
   CListVariable m_listOverlayPoints; // list of list of points (TEXTUREPOINT)
   CListFixed    m_listOverlayClockwise;  // BOOL - TRUE if overlay is clockwise

   DWORD       m_dwMinDivideH;   // detail information
   DWORD       m_dwMaxDivideH;   // detail information
   DWORD       m_dwMinDivideV;   // detail information
   DWORD       m_dwMaxDivideV;   // detail infomraiton
   fp      m_fDetail;        // detail information
   DWORD       m_dwDivideH;      // calculated number of times to divide
   DWORD       m_dwDivideV;      // calculated number of times to divide

   DWORD       m_dwMeshH;        // size of built mesh
   DWORD       m_dwMeshV;        // size of built mesh
   CMem        m_memMesh;        // memory used for the mesh
   CMem        m_memMeshNormals; // memory used for the mesh normals
   PCPoint     m_paMesh;         // [0..m_dwMeshV-1][0..m_dwMeshH-1]
   PCPoint     m_paMeshNormals;  // [0..m_dwMeshV-1][0..m_dwMeshH-1][0..3], 0..3 are four corners in clockwise direction, all normalized

   fp      m_aTextMatrix[2][2]; // texture matrix as set by TextureInfoSet()
   CMem        m_memTexture;     // memory containing texture map info
   PTEXTPOINT5 m_paTexture;    // [0..m_dwMeshV -1 + m_fLoopV][0..m_dwMeshH - 1 + m_fLoopH], or NULL if no texture

   CListFixed  m_listListMG;     // list of PCListFixed. Those lists are of PCMeshGrid objects

   CListFixed  m_lIntersectLoopWithGridTP;   // reduce number of mallocs in render
};
typedef CSplineSurface *PCSplineSurface;

/*************************************************************************
CSurfaceSchemeSocket - Super-class for surface scheme code so can remap
schemes when they're in CObjectEditor.
*/
class CObjectSurface;
typedef CObjectSurface *PCObjectSurface;
class CWorldSocket;
class CSurfaceSchemeSocket {
public:
   virtual PCMMLNode2 MMLTo (void) = 0;
   virtual BOOL MMLFrom (PCMMLNode2 pNode) = 0;
   // Dont support this: virtual CSurfaceScheme *Clone (void) = 0;
   virtual PCObjectSurface SurfaceGet (PWSTR pszScheme, PCObjectSurface pDefault, BOOL fNoClone) = 0;
   virtual BOOL SurfaceExists (PWSTR pszScheme) = 0;
   virtual BOOL SurfaceSet (PCObjectSurface pSurface) = 0;
   virtual BOOL SurfaceRemove (PWSTR pszScheme) = 0;
   virtual PCObjectSurface SurfaceEnum (DWORD dwIndex) = 0;
   virtual void WorldSet (CWorldSocket *pWorld) = 0;
   virtual void Clear (void) = 0;
};
typedef CSurfaceSchemeSocket *PCSurfaceSchemeSocket;




/*************************************************************************
Textures */

DLLEXPORT BOOL DefaultSurfFromNewTexture (DWORD dwRenderShard, GUID *pgCode, GUID *pgSub, PCObjectSurface pSurf);
DLLEXPORT void HackTextureCreate();
DLLEXPORT void TextureMatrixMultiply (fp pm[2][2], PCMatrix pMatrixXYZ, PTEXTPOINT5 pt);
DLLEXPORT void TextureMatrixMultiply2D (fp pm[2][2], PTEXTUREPOINT pt);
DLLEXPORT void TextureMatrixRotate (fp pm[2][2], fp rot, fp fScaleH = 1, fp fScaleV = 1);
DLLEXPORT BOOL TextureLibraryDialog (DWORD dwRenderShard, HWND hWnd);
class CWorldSocket;
typedef CWorldSocket *PCWorldSocket;
DLLEXPORT PCMMLNode2 TextureCacheUserTextures (DWORD dwRenderShard, PCWorldSocket pWorld);
class CLibrary;
DLLEXPORT BOOL TextureUnCacheUserTextures (DWORD dwRenderShard, PCMMLNode2 pNode, HWND hWnd, CLibrary *pLibrary);



// CSurfaceScheme - This object stores all a color/texture scheme up for a world
class CWorldSocket;
class DLLEXPORT CSurfaceScheme : public CSurfaceSchemeSocket {
public:
   ESCNEWDELETE;

   CSurfaceScheme (void);
   ~CSurfaceScheme (void);

   virtual CSurfaceScheme *Clone (void);

   virtual PCMMLNode2 MMLTo (void);
   virtual BOOL MMLFrom (PCMMLNode2 pNode);
   virtual PCObjectSurface SurfaceGet (PWSTR pszScheme, PCObjectSurface pDefault, BOOL fNoClone);
   virtual BOOL SurfaceExists (PWSTR pszScheme);
   virtual BOOL SurfaceSet (PCObjectSurface pSurface);
   virtual BOOL SurfaceRemove (PWSTR pszScheme);
   virtual PCObjectSurface SurfaceEnum (DWORD dwIndex);
   virtual void WorldSet (CWorldSocket *pWorld);
   virtual void Clear (void);

private:
   DWORD FindIndex (PWSTR pszScheme);
   PCObjectSurface FindSurface (PWSTR pszScheme);

   CListFixed     m_listPCObjectSurface;  // list of surfaces
   BOOL           m_fObjectSurfacesSorted;   // set to TRUE if m_listPCObjectSurface is sorted by string
   CWorldSocket         *m_pWorld;              // world. When surface is changed tells the world of this
};
typedef CSurfaceScheme *PCSurfaceScheme;
extern PWSTR gszObjectSurface;


/**********************************************************************
CRenderSocket - Vritual class called by the objects in space to draw themselves.
*/


// used to desribe the material being renderes.
typedef struct {
   WCHAR             szScheme[64];    // name of the scheme used. Not actually used by the
                                      // renderer, but it is used to trace remapped schemes
                                      // in CObjectEditor, so it good to pass scheme name down

   WORD              wMinorID;            // minor object ID
   WORD              wFill;               // fill - nothing goes here

   CMaterial         Material;            // used to store material information

   BOOL              fUseTextureMap; // if TRUE use the texture map information (most of below)
                                 // else, use cColor
   GUID              gTextureCode;  // GUID that specifies the texture code to use
   GUID              gTextureSub;   // GUID that specifies sub-type of gTextureCode
   TEXTUREMODS       TextureMods;   // modifications applied to the texture
   fp                afTextureMatrix[2][2];// rotation/scaling matrix for texture HV.
                                    // the object that calls the renderer applies this. THe
                                    // renderer can ignore it
   BYTE              abTextureMatrix[sizeof(CMatrix)]; // this is a hak because
                                    // if use mTextureMatrix, get a compile error that
                                    // cant figure out
   //CMatrix           mTextureMatrix;   // rotation/scaling matrix for texture XYZ
} RENDERSURFACE, *PRENDERSURFACE;

// used on POLYDESCRIPT
typedef struct {
   DWORD             dwPoint;             // point index, 0-based index into POLYRENDERINFO.paPoints
   DWORD             dwNormal;            // normal index. 0-based index into POLYRENDERINFO.paNormals
   DWORD             dwColor;             // color index. 0-based index into POLYREDNERINFO.paColors
   DWORD             dwTexture;           // texture index. 0-based index into POLYRENDERINFO.paTextures
} VERTEX, *PVERTEX;

// used in POLYRENDERINFO
typedef struct {
   WORD              wNumVertices;       // number of Vertices in the polygon
   WORD              wFill;               // not used
   DWORD             dwIDPart;            // part ID
   DWORD             dwSurface;           // surface index. 0-based index into POLYRENDERINFO.paSurfaces
   DWORD             fCanBackfaceCull;    // if TRUE it's OK to backface cull this polygon. If FALSE don't
   // array of wNumVertices DWORDS, which are indexes into Vertices
} POLYDESCRIPT, *PPOLYDESCRIPT;

// used by CRenderSocket for specifying a bunch of polygons
typedef struct {
   DWORD             dwNumSurfaces;       // number of surfaces in paSurfaces
   PRENDERSURFACE    paSurfaces;          // pointer to an array of surfaces.
   DWORD             dwNumPoints;         // number of points in paPoints
   PCPoint           paPoints;            // array of points
   DWORD             dwNumNormals;        // number of normals in paNormals
   PCPoint           paNormals;           // arrya of normals
   BOOL              fAlreadyNormalized;  // if TRUE, the normals are already normalized
   DWORD             dwNumTextures;        // number of normals in paNormals
   PTEXTPOINT5       paTextures;           // arrya of textures
   DWORD             dwNumColors;         // number of colors in paColors
   COLORREF          *paColors;           // array of colors
   DWORD             dwNumVertices;      // number of Vertices
   PVERTEX           paVertices;         // pointer to the Vertices
   DWORD             dwNumPolygons;       // number of polygons to draw
   PPOLYDESCRIPT     paPolygons;          // array/list of polygons. size of each member varies because of number of Vertices

   BOOL              fBonesAlreadyApplied;   // set to TRUE if bones have already been applied, so no furhter deformation
                                             // of bones should happen
} POLYRENDERINFO, *PPOLYRENDERINFO;

class CRenderSocket {
public:
   virtual BOOL QueryWantNormals (void) = 0;    // returns TRUE if renderer wants normal vectors
   virtual BOOL QueryWantTextures (void) = 0;   // returns TRUE if the renderer wants textures
   virtual fp QueryDetail (void) = 0;       // returns detail resoltion (in meters) that renderer wants now
                                                // this may change from one object to the next.
   virtual void MatrixSet (PCMatrix pm) = 0;    // sets the current operating matrix. Set NULL implies identity.
                                                // Note: THis matrix will be right-multiplied by any used by renderer for camera
   virtual void PolyRender (PPOLYRENDERINFO pInfo) = 0;
                                                // draws/commits all the polygons. The data in pInfo, and what it points
                                                // to may be modified by this call.

   virtual BOOL QuerySubDetail (PCMatrix pMatrix, PCPoint pBound1, PCPoint pBound2, fp *pfDetail) = 0;
      // an object can call this to see how much detail should be given to a sub-portion
      // of the object and if the sub-portion is visible. This is used by the ground object
      // to optimize drawing. pMatrix is the matrix that transforms the two bounding box corners
      // pBound1 and pBound2 into world space. Returns TRUE if the bounding box is visible, and
      // will fill in pfDetail. if the bounding box are is now obscured by other drawing will return
      // false and WONT bother filling in pfDetail

   virtual BOOL QueryCloneRender (void) = 0;
      // the renderer will return TRUE if it supports the CloneRender() call. If it doesn't
      // the caller will need to call the clone object and have it render.

   virtual BOOL CloneRender (GUID *pgCode, GUID *pgSub, DWORD dwNum, PCMatrix pamMatrix) = 0;
      // an object should only call this if QueryCloneRender() returns TRUE. (CloneRender()
      // will return FALSE if isn't supported). This draws multiple copies (dwNum of them)
      // of a clone. pamMatrix is an array of dwNum matricies that do all the translation,
      // rotation, and scale of the clone's coords to world space. (Any matrix passed into
      // MatrixSet() is ignored). pgCode and pgSub identify the object that is to be used
};
typedef CRenderSocket *PCRenderSocket;


/**************************************************************************************
CObjectSocket - Callback into objects */

class CWorldSocket;

// object render is passed into the CObjectSocket when an object is
// asked to render itself. It contains various parameters importnat
// to rendering
typedef struct {
   DWORD             dwReason;   // reason for rendering. See ORREASON_XXX
   DWORD             dwSpecial;  // special info used when rendering. See ORSPECIAL_XXX
   PCRenderSocket    pRS;        // render socket to render into
   DWORD             dwShow;     // collection of RENDERSHOW_XXX bits saying what should be drawn
                                 // in general, objects ignore this, but if they have parts that
                                 // could be drawn separately (like part roof and part walls)
                                 // then may pay attention to this
   BOOL              fSelected;  // set to TRUE if the object is selected. Some objects will draw
                                 // differently when they're selected
   BOOL              fCameraValid;  // set to TRUE if the camera location is valid
   CPoint            pCamera;    // camera location in world space. Used to optimize drawing order within an object
} OBJECTRENDER, *POBJECTRENDER;

#define  ORREASON_UNKNOWN        0
#define  ORREASON_WORKING        1  // being drawn while working on it, which means draw faster
#define  ORREASON_BOUNDINGBOX    2  // calculating bounding box
#define  ORREASON_FINAL          3  // being drawn for the final render
#define  ORREASON_SHADOWS        4  // calculating shadows for shadows render

#define  ORSPECIAL_NONE          0  // no special info
#define  ORSPECIAL_POLYMESH_EDITPOLY 1 // this is being used to diplay the polymesh editor when editing polygons
#define  ORSPECIAL_POLYMESH_EDITMORPH 2   // being used to display a morph
#define  ORSPECIAL_POLYMESH_EDITBONE 3 // display bone color

typedef struct {
   GUID           gCode; // Class-ID that can find the right piece of code for the object
   GUID           gSub;  // class-ID, which when passed to the class-factor pointed to by
                              // gClassCode, and get the right sub-type (such as style of door)
   DWORD          dwRenderShard; // render shard to use
} OSINFO, *POSINFO;

// OSINTELLIPOS - Information that might help an object place itself
typedef struct {
   CWorldSocket         *pWorld;           // world that the object will be placed in
                                    // this should be used for calculations, NOT the
                                    // object's current world

   BOOL           fMoveOnly;        // if TRUE the object is being pasted and should
                                    // only be moved rotated, not resized

   // where the user clicked
   CPoint         pClickLineStart;  // pClickLineStart and pClickLineVector are
   CPoint         pClickLineVector; // derived from the view where the user was asked to
                                    // click to place the object. This is a line radiating
                                    // from the eye, through the pixel and beyond.
                                    // If the user didn't
                                    // click on anything (except open space) then this
                                    // must be used to guess where the object should be

   BOOL           fClickOnValid;    // TRUE if pClickOn has valid data, FALSE if invalid
   CPoint         pClickOn;         // point in space where the user clicked on

   BOOL           fClickNormalValid;// TRUE if pClickNormal has valid data, FALSE if invalid
   CPoint         pClickNormal;     // normal to surface's plane of the point that was
                                    // clicked on. Points towards the viewer

   // object that clocked on
   BOOL           fClickedOnObject; // if TRUE then the GUID and surface are valid
   GUID           gClickObject;     // object that clicked on
   DWORD          dwClickSurface;   // surface that clicked on. LOWORD(PIMAGEPIXEL.dwID);

   // camera information
   CPoint         pCamera;          // viewer location
   CPoint         pLookAt;          // vector describing what the camera is looking at
   BOOL           fViewFlat;        // if TRUE using flat view (not perspective), FALSE using perspective
   fp         fFOV;             // field of fiew in radians. only valid if !fViewFlat
   fp         fWidth;           // width of display in meters. only valid if fViewFlat
} OSINTELLIPOS, *POSINTELLIPOS;

// OSINTELLIPOSDRAG - Information that might help an object place itself
typedef struct {
   CWorldSocket         *pWorld;           // world that the object will be placed in
                                    // this should be used for calculations, NOT the
                                    // object's current world
   // camera information
   CPoint         pCamera;          // viewer location
   CPoint         pLookAt;          // vector describing what the camera is looking at
   BOOL           fViewFlat;        // if TRUE using flat view (not perspective), FALSE using perspective
   fp         fFOV;             // field of fiew in radians. only valid if !fViewFlat
   fp         fWidth;           // width of display in meters. only valid if fViewFlat

   // what clickede on
   PCPoint        paWorldCoord;     // array of points (in world space) that dragged across,
                                    // fromt he starting click to the ending click. It's
                                    // up to the recipient to make sense of these.
   DWORD          dwNumWorldCoord;  // number of points.
} OSINTELLIPOSDRAG, *POSINTELLIPOSDRAG;

typedef struct {
   BOOL           fEmbedded;        // If TRUE, wants to be embedded, else not embedded
                                    // if want to be embedded, but cant, app figures out location
   
   // embedded info
   GUID           gContainer;       // container wants to be embedded in
   DWORD          dwEmbedSurface;   // surface it to be embedded in
   TEXTUREPOINT   tpEmbed;          // location where wants to be embedded
   fp         fEmbedRotation;   // rotation where wants to be embedded

   // non-embedded info
   CMatrix        mObject;          // what object matrix it wants
} OSINTELLIPOSRESULT, *POSINTELLIPOSRESULT;

// OSCONTROL is information about control points from controlPointQuery
typedef struct {
   DWORD          dwID;             // ID of the control, from 0 to 1023. Will be used as minor
                                    // object ID 0xf000 to 0xffff
   BOOL           fButton;          // if FALSE then drag control point, if TRUE acts like a button and just
                                    // calling ControlPointSet() without a value change even, will
                                    // toggle some action
   BOOL           fPassUp;          // if TRUE, then when CP is in a control in the object editor's world,
                                    // it will be passed up as a control point from the object instances.
                                    // the location of this CP must affect and be affected by one of its attributes
                                    // so that multiple instancing in the OE will work
   WCHAR          szName[64];       // name of the control point
   WCHAR          szMeasurement[64];// measurement to be displayed, such as "48 in.", if this affects
                                    // the width of the object and makes it 48" wide.

   // location and movement freedom
   CPoint         pLocation;        // location in object coordinates
   // not used DWORD          dwFreedom;        // 0 = any direction, 1=plane defined by pV1 and pV2 and pLocation, 2=line defined by pV1 and pLocation
   // not used CPoint         pV1, pV2;         // see dwFreedom. pLocation should be kept on the plane/line defined by these. If not,
                                    // the object will fit the location to the nearest plane/freedom when it's moved anyway

   // pointer display
   DWORD          dwStyle;          // control point style. see CPSTYLE_XXX
   // not used CPoint         pDirection;       // direction vector of point, in control coord.
   fp             fSize;            // width, length, diameter, etc. of the control point
   COLORREF       cColor;           // color of the pointer
} OSCONTROL, *POSCONTROL;
#define CPSTYLE_CUBE          0     // draw a cube. pDirection is ignored
#define CPSTYLE_SPHERE        1     // draw a sphere. pDirection is ignores
#define CPSTYLE_POINTER       2     // draw a pointer in the direction pDirection

// objectsocket messages

#define OSM_SPLINESURFACEGET  100
// Supported by CObjectStructSurface (and others). Pass in a
// spline type and it fills in a list with the spline surfaces (of that type)
// within the object. The spline surfaces should not be midified, although they
// can be used to determine intersections. NOTE: Does NOT clear any other entries in the list already
typedef struct {
   // in
   DWORD          dwType;  // 0 for any and all AB, 1 for ceiling/roof line, 2 for wall line, 3 for floor,
                           // 4 for roof spine (center of roof), 5 for any spline center
   PCListFixed    pListPSS;   // should be initialized to sizeof(PCSplineSurface). Filled in with
                           // list of spline surfaces
   PCListFixed    pListMatrix;   // should be initialized to sizeof(CMatrix). Filled in with
                           // one matrix per spline that transforms the spline from it's own coordinates
                           // into world space

   // out
   // information passed back in pList
} OSMSPLINESURFACEGET, *POSMSPLINESURFACEGET;


#define OSM_IGNOREWORLDBOUNDINGBOXGET  101
// If an object returns true for this, it's ignored by worldboundingboxget when
// fIgnoreGround is passed in as a parameter. Objects that are generally
// not part of the house should return this. If it fills in the fIgnoreComplete
// flag then not used for any bounding box (such as cameras)
typedef struct {
   BOOL           fIgnoreCompletely;   // if set to TRUE then should ignore completely - camera
} OSMIGNOREWORLDBOUNDINGBOXGET, *POSMIGNOREWORLDBOUNDINGBOXGET;


#define OSM_INTERSECTLINE     102
// Intersects a line with the surface. Fills in the point where it intersected.
// Supported by the ground object so a house builing block that is being placed
// knows how far down the ground is and where need to build foundations.
// NOTE: Will return a valid intersection, even though it may intersect before
// or after pStart,pEnd
typedef struct {
   CPoint         pStart;     // start or the line, in WORLD space
   CPoint         pEnd;       // end of the line, in WORLD space
   CPoint         pIntersect; // if intersects, filled with the intersection point in WORLD space
   BOOL           fIntersect; // set to TRUE if there's an intersection
} OSMINTERSECTLINE, *POSMINTERSECTLINE;

#define OSM_QUERYGROUND       103
// Ground objects return TRUE for this. House building blocks use it to know
// which objects need foundations built into them, etc.

#define OSM_GLOBALLEVELCHANGED 104
// Sent to all objects to tell them that the global settings for the definition
// of a level (aka floor) has changed. Generally, this is only paid attention to
// by walls and building block objects.

#define OSM_FINDCLOSESTFLOOR     105
// Sent by doors and doorways into wall objects to find the closest floor to
// a given HV. (This means closest floor level below.) If successful, the doors
// and doorways will base themselves off that floor.
typedef struct {
   DWORD          dwSurface;  // surface
   TEXTUREPOINT   pOrig;      // original value
   TEXTUREPOINT   pClosest;   // if successful, fill this with the closest floor
} OSMFINDCLOSESTFLOOR, *POSMFINDCLOSESTFLOOR;


#define OSM_SETFLOORS         106
// Sent by the CObjectBuildBlock into the walls (CDoubleSurface) it creates. This passes
// in a list of floor levels (not necessarily ordered) as V values.
// The CDoubleSurface object uses this for OSM_FINDCLOSESTFLOOR
typedef struct {
   DWORD          dwSurface;  // surface
   fp         *pafV;      // array of dwords that are v values from 0 to 1
   DWORD          dwNum;      // number of pafV
} OSMSETFLOORS, *POSMSETFLOORS;



#define OSM_BUILDBLOCKQUERY   107
// Sent by one building block to all others in the area. Asks the other building blocks
// what their bounding walls are. (Outside ones face out, inside ones face in.)
// These can be used to clip against. Return TRUE if success.

typedef struct {
   PCListFixed          plBBCLIPVOLUME;   // if successful, PCListFixed is allocated, contains list of
                                          // BBCLIPVOLUME. Caller must free pss in clip volume and plBBCLIPVOLUME
   fp               fArea;            // Filled in area ofthe building block - Smaller building blocks give way to larger
                                          // ones and end up clipping their floors against inside.
                                          // if areas are exactly equal look at guid, below
   GUID                 gID;              // filled with ID guid. If areas exactly equal, lowest guid (by memcmp) gives way
} OSMBUILDBLOCKQUERY, *POSMBUILDBLOCKQUERY;

typedef struct {
   PCSplineSurface      pss;     // spline surface that describes side.
                                 // MUST be freed by caller
   CMatrix              mToWorld;// translates from spline space to world space
   DWORD                dwType;  // 0 for wall, 1 for roof, 2 for floor, 3 for top ceiling, 4 for verandah boundary
   int                  iSide;   // 1 for side A (external of shell), 0 for center, -1 for side B (internal of shell)
                                 // Note that external and internal are really two different entities
} BBCLIPVOLUME, *PBBCLIPVOLUME;



#define OSM_GROUNDCUTOUT   108
// Called by building blocks to every object to say that they have a basement that
// should be cutout of the ground.
typedef struct {
   GUID                 gBuildBlock;   // building block's guid. Can ID the ground if build block changes
   DWORD                dwNum;         // number of points in arch. If 0, should delet the arch
   PCPoint              paTop;         // dwNum points for the top of the arch - (Points in world space)
   PCPoint              paBottom;      // dwNum points for the bottom of the arch - Not necessarily the bottom of the basement (points in world space)
} OSMGROUNDCUTOUT, *POSMGROUNDCUTOUT;


#define OSM_DENYCLIP       109
// If an object returns true for this it won't be clipped out by any of the clipping
// planes. Used by the cameras to make sure they're visible.


#define OSM_WALLMOVED      110
// Send to all walls in the are when another wall is moved. This allows the other walls
// which have a corner where the old moved point was, to move their corner also.
typedef struct {
   CPoint               pOld;          // old corner (bottom) that's moved
   CPoint               pNew;          // new corner (bottom) moved to
} OSMWALLMOVED, *POSMWALLMOVED;



#define OSM_WALLQUERYEDGES 111
// Sent to a wall to query information about it's left and right edges.
// in The structure, [0] is the left, and [1] is the right. This
// information is filled in by the wall.
typedef struct {
   CPoint               apCorner[2];   // bottom corner location, in world space
   CPoint               apACorner[2];  // bottom corner of A side, world space
   CPoint               apBCorner[2];  // bottom corner of B side, world space
   CPoint               apAVector[2];  // direction vector for apACorner, world space
   CPoint               apBVector[2];   // direction vector for apBCorner, world space
} OSMWALLQUERYEDGES, *POSMWALLQUERYEDGES;



#define OSM_NEWLATITUDE     112
// Sent to all objects when the time/date has changed. Trees use it to recognize
// a change in seasons and adjust themselves accordingly.
// No parameters


#define OSM_CLICKONOVERLAY  113
// THis is supported by CDoubleSurface. Call this with a specific surface ID (from
// the loword() of major ID, (the same as when doing a paint message) to see if the
// surface clicked on is the main background, or if it's an overlay. Returns TRUE
// if clicked on an overlay.
typedef struct {
   DWORD    dwID;       // surface ID, such as used for painting
} OSMCLICKONOVERLAY, *POSMCLICKONOVERLAY;



#define OSM_NEWOVERLAYFROMINTER 114
// This is called to create a new overlay in the double surface. The shape of the
// overlay is based on the intersection of the walls (and other cutouts) of the
// area around the point.
typedef struct {
   DWORD    dwIDClick;  // surface ID that clicked on (which is to have the new overlay)
                        // This is NOT an existing overlay, and will either be side A or side B
   TEXTUREPOINT tpClick;   // the HV where clicked on the surface, from ContHVQuery(), passing in dwIDClick
   DWORD    dwIDNew;    // if everything goes ok, the function will return TRUE and will
                        // fill dwIDNew with the new Overlay ID, so this can be sent down
                        // as paint information
} OSMNEWOVERLAYFROMINTER, *POSMNEWOVERLAYFROMINTER;


#define OSM_STRUCTSURFACE     115
// If the object is a CObjectStructSurface, it will return a pointer to itself. Otherwise false
class CObjectStructSurface;
typedef CObjectStructSurface *PCObjectStructSurface;
typedef struct {
   PCObjectStructSurface   poss; // filled in with struct surface
} OSMSTRUCTSURFACE, *POSMSTRUCTSURFACE;


#define OSM_POLYMESH          116
// If the object is a CObjectPolyMesh, it will return a pointer to itself. Otherwise, FALSE;
class CObjectPolyMesh;
typedef CObjectPolyMesh *PCObjectPolyMesh;
typedef struct {
   PCObjectPolyMesh        ppm;  // filled in with a pointer to poly mesh
} OSMPOLYMESH, *POSMPOLYMESH;


#define OSM_BONE              117
// if the object is a CObjectBone, it will return a pointer to itself. Otherwise FALSE:
class CObjectBone;
typedef CObjectBone *PCObjectBone;
typedef struct {
   PCObjectBone            pb;   // filled with a pointer to the bone
} OSMBONE, *POSMBONE;



#define OSM_KEYFRAME          118
// supported by CAnimKeyframe. It returns a pointer to itself.
class CAnimKeyframe;
typedef CAnimKeyframe *PCAnimKeyframe;
typedef struct {
   PCAnimKeyframe          pak;   // filled with a pointer to the keyframe
} OSMKEYFRAME, *POSMKEYFRAME;



#define OSM_BOOKMARK          119
// supported by CAnimBookmark. It returns information about hte bookmark
typedef struct {
   WCHAR          szName[64];    // bookmark name
   fp             fStart;        // start time
   fp             fEnd;          // end time. If == fStart then 0-length bookmark
   GUID           gObjectAnim;   // animation object
   GUID           gObjectWorld;  // world object
} OSMBOOKMARK, *POSMBOOKMARK;


#define OSM_ANIMCAMERA          120
// supported by CObjectAnimCamera. It returns a pointer to itself.
class CObjectAnimCamera;
typedef CObjectAnimCamera *PCObjectAnimCamera;
typedef struct {
   PCObjectAnimCamera          poac;   // filled with a pointer to the camerea
} OSMANIMCAMERA, *POSMANIMCAMERA;

#define OSM_CAVE                121
// supported by CObjectCave. It returns a pointer to itself.
class CObjectCave;
typedef CObjectCave *PCObjectCave;
typedef struct {
   PCObjectCave          pCave;   // filled with a pointer to the camerea
} OSMCAVE, *POSMCAVE;


#define OSM_GROUNDINFOGET        121
// supported by CObjectGround. When called, the values of the structure
// are filled in. THe caller (usually the groundview) can use this for editing
// the ground
#define GWATERNUM                3     // number of levels for ground water
#define GWATEROVERLAP            2     // # of copies of surfaces per level

class CForest;
typedef CForest *PCForest;
typedef struct {
   DWORD             dwWidth;        // # of points across (EW)
   DWORD             dwHeight;       // # of points up/down (NS)
   TEXTUREPOINT      tpElev;         // .h is minimum elevation (at 0), .v is max elevation (at 0xffff)
   WORD              *pawElev;        // pointer to elevation information. [x + m_dwWidth * y]
   fp                fScale;         // number of meters per square. total width = (m_dwWidth-1)*m_fScale. Likewise for height

   // textures
   BYTE              *pabTextSet;   // pointer to texture inforamtion. [x + m_dwWidth*y + dwTextNum*dwWidth*dwHeight]
   DWORD             dwNumText;     // number of textures
   PCObjectSurface   *paTextSurf;   // pointer to an array of PCObjectSurface's. dwNumText entries
   COLORREF          *paTextColor;  // pointer to an array of COLORREF's for how the surfaces are displayed during eiditng. dwNumText entries

   // forest
   BYTE              *pabForestSet; // pointer to texture inforamtion. [x + m_dwWidth*y + dwForetNum*dwWidth*dwHeight]
   DWORD             dwNumForest;   // number of forests - should be at least 1
   PCForest          *paForest;     // pointer to an array of PCForest's. dwNumForest entries

   // water
   BOOL              fWater;         // if TRUE, diplay water
   fp                fWaterElevation;   // water elevation, in meters
   fp                fWaterSize;     // how large to draw water, in meters, if 0 then size of ground
   PCObjectSurface   apWaterSurf[GWATERNUM*GWATEROVERLAP]; // water surface
   fp                afWaterElev[GWATERNUM]; // water elevation at each surface
} OSMGROUNDINFOGET, *POSMGROUNDINFOGET;


#define OSM_GROUNDINFOSET        122
// supported by CObjectGround. When it receives this, it will change the width, height,
// and other settings based on the contents of the structure. NOTE: It will copy
// memory passed in
typedef OSMGROUNDINFOGET OSMGROUNDINFOSET;
typedef POSMGROUNDINFOGET POSMGROUNDINFOSET;


#define OSM_SKYDOME              123
// The skydome object responds to this. It returns TRUE


#define OSM_FLOORLEVEL           124
// Returns the Z level of the floor (and water). This is used to determine where to place
// the camera.
typedef struct {
   CPoint            pTest;         // point in world space, that needs to be translated to object space.
                                    // if this is NOT in the footprint of the object then return FALSE
   int               iFloor;        // floor level looking for. 0 = ground, 1=1st floor, -1 = basement

   // filled in by the function
   fp                fLevel;        // Z-level of the floor. Translated BACK into world space
   BOOL              fIsWater;      // Set to TRUE if there's water
   fp                fWater;        // If there's water (terrain) then water level, translated BACK into world
                                    // space.
   fp                fArea;         // area of the object. Usually, the floor level is affected by the smallest object
} OSMFLOORLEVEL, *POSMFLOORLEVEL;


#define OSSTRING_NAME      1     // Name of the object. Used with StringSet/Get.
#define OSSTRING_GROUP     2     // Group of the object. Used with StringSet/Get
#define OSSTRING_ATTRIBRANK 3    // The attributes listed (separated by back-slashes) have their rank toggled when displayed


typedef struct {
   WCHAR       szName[64];    // attribute name
   fp          fValue;        // value of the attribute
} ATTRIBVAL, *PATTRIBVAL;

#define AIT_NUMBER            0     // this is just a number
#define AIT_DISTANCE          1     // in meters
#define AIT_ANGLE             2     // in radiuans
#define AIT_BOOL              3     // can only be fMin or fMax, on or off
#define AIT_HZ                4     // frequency in hz (only used in TimelineHorzTicks)

typedef struct {
   PCWSTR      pszDescription;   // pointer to a description string. Valid only for so
                                 // long, so dont assume it willalways be around. Can be null
   fp          fMin;          // minimum value
   fp          fMax;          // maximum value
   BOOL        fDefPassUp;    // if TRUE then default to container object inheriting this attribute
   BOOL        fDefLowRank;   // if TRUE, then default to putting at the bottom of the list
   DWORD       dwType;        // one of AIT_XXX
   CPoint      pLoc;          // location of the attribute, in object space
} ATTRIBINFO, *PATTRIBINFO;

typedef struct {     // coords are in object space when struct passed to renderer, world space for light objects
   DWORD          dwForm;     // one of LIFORM_XXX
   BOOL           fNoShadows; // if TRUE then don't cast any shadows
   CPoint         pLoc;    // location of the light... ignored if light is infinitely far away
   CPoint         pDir;    // vector pointint in the direction of the light.
   fp             afLumens[3];   // lumens for the [0]=directional front, [1]=directional back, [2]=omni
   WORD           awColor[3][3];    // color for [0][x]=directional front, [1][x]=directional back, [2][x] =omni, [x] = RGB
   fp             afDirectionality[2][2]; // directionality for [0][y]=front, [1][y] = back,
                                          // [x][0] = maximum angle of illumination, [x][1] = min angle (in radians)
} LIGHTINFO, *PLIGHTINFO;

// CObjectSocket is a pure virtual class supported by all the world objects
// in the system. It's used for communication to the objects.
class CObjectSocket {
public:
   virtual void Delete (void) = 0;
      // tell the object to delete itself

   virtual BOOL StringSet (DWORD dwString, PWSTR psz) = 0;
      // Sets one of the prefedined object strings, such as OSSTRING_NAME.

   virtual PWSTR StringGet (DWORD dwString) = 0;
      // Gets the string. DONT change the memory pointed to by the string

   virtual void ShowSet (BOOL fShow) = 0;
      // If true then show the object. If FALSE then hide the object

   virtual BOOL ShowGet (void) = 0;
      // returns TRUE if the object is visible (not hidden), false if hidden

   virtual void Render (POBJECTRENDER pr, DWORD dwSubObject) = 0;
      // tell the object to render itself. It can set
      // the pr->pRS render matrix to save rotations
      // If dwSubObject != -1 then only a specific sub-object (0..QuerySubObject()-1)
      // of the object is supposed to draw itself

   virtual BOOL LightQuery (PCListFixed pl, DWORD dwShow) = 0;
      // ask the object if it has any lights. If it does, pl is added to (it
      // is already initialized to sizeof LIGHTINFO) with one or more LIGHTINFO
      // structures. Coordinates are in OBJECT SPACE.
      // dwShow is RENDERSHOW_XXX so the light can hide itself if the rest of the
      // object is hidden

   virtual void QueryBoundingBox (PCPoint pCorner1, PCPoint pCorner2, DWORD dwSubObject) = 0;
      // asks the object to fill in a bounding box for itself
      // pTransRot is the rotation matrix it fills in for itself,
      // pCorner1 and pCorner2 define the corners for rectangle for itself
      // if dwSubObject != -1 then asking for the bounding box of a subobject of the object

   virtual DWORD QuerySubObjects (void) = 0;
      // returns the number of sub-objects the object support. Usually this is 0,
      // indicating that no sub-objects are supported.

   virtual void PreRender (void) = 0;
      // This is a semi-hack function to cause the object to render,
      // so that object-editor objects know which colors are used.

   virtual BOOL ObjectMatrixSet (CMatrix *pObject) = 0;
      // sets the translation and rotation (and scaling) matrix for the object in general
      // NOTE: Not supported if object is embedded

   virtual void ObjectMatrixGet (CMatrix *pObject) = 0;
      // gets the object's translation and rotation (and scaling) matrix

   virtual DWORD CategoryQuery (void) = 0;
      // returns the category (a combination of RENDERSHOW_XXX) that this
      // belongs to. Some objects wont be drawn unless their category is selected

   virtual void WorldSet (CWorldSocket *pWorld) = 0;
      // called to tell the object what world it's in so it can interrogate other
      // objects and notify the world that it has changed. NOTE: Object shouldnt
      // do anything until WorldSetFinished() is called - especially if it has
      // embedded object. That makes sure that all the embedded objects are in
      // place and renamed before anything else happens

   virtual void WorldSetFinished (void) = 0;
      // Called after every WorldSet() call. This is called to tell the objects
      // they're in the new world and all their embedded objects are in the new
      // world so they may want to do some processing

   virtual DWORD MoveReferenceCurQuery (void) = 0;
      // returns the index for the current move reference. Initially, the
      // object should have the most likely movement reference selected

   virtual BOOL MoveReferenceCurSet (DWORD dwIndex) = 0;
      // sets the current move reference to a new index. Returns FALSE
      // if the index is invalid

   virtual BOOL MoveReferencePointQuery (DWORD dwIndex, PCPoint pp) = 0;
      // given a move reference index, this fill in pp with the position of
      // the move reference RELATIVE to ObjectMatrixGet. References are numbers
      // from 0+. If the index is more than the number of points then the
      // function returns FALSE

   virtual BOOL MoveReferenceStringQuery (DWORD dwIndex, PWSTR psz, DWORD dwSize, DWORD *pdwNeeded) = 0;
      // given a move reference index (numbered from 0 to the number of references)
      // this fills in a string at psz and dwSize BYTES that names the move reference
      // to the end user. *pdwNeeded is filled with the number of bytes needed for
      // the string. Returns FALSE if dwIndex is too high, or dwSize is too small (although
      // pdwNeeded will be filled in)

   virtual void GUIDSet (GUID *pg) = 0;
      // set the object's GUID

   virtual void GUIDGet (GUID *pg) = 0;
      // get the object's GUID

   virtual BOOL Message (DWORD dwMessage, PVOID pParam) = 0;
      // sends a message to the object. The interpretation of the message depends upon
      // dwMessage, which is OSM_XXX. If the function understands and handles the
      // message it returns TRUE, otherwise FALE.

   virtual CObjectSocket *Clone (void) = 0;
      // clones the current object

   virtual PCMMLNode2 MMLTo (void) = 0;
      // the object should create a new PCMMLNode2 (to be freed by the caller)
      // and fill it in with all the information needed to recreate the object
      // (except for the class ID, which is done elsewhere)

   virtual BOOL MMLFrom (PCMMLNode2 pNode) = 0;
      // the object should read in all the information from PCMMLNode2 and
      // restore itself (as it was in MMLTo)

   virtual void InfoGet (POSINFO pInfo) = 0;
      // the object should fill in the information into POSINFO

   virtual BOOL IntelligentPosition (CWorldSocket * pWorld, POSINTELLIPOS pInfo, POSINTELLIPOSRESULT pResult) = 0;
      // this is called when an object is pasted into a world, or if it's newly
      // created and placed in the world. pInfo conttains data about where the
      // user click, what was being looked at etc. the object can use the opportunity
      // to move, rotate, and reshape (except for paste) itself in a logical
      // location.
      // pWorld is a pointer to the world where it will be placed, but which it isn't
      // in yet.
      // Return TRUE if the object does this, FALSE if it's not handled
      // and the application should place to the best of its ability.
      // Fills in pResult with information about where it wants to be placed.

   virtual BOOL IntelligentPositionDrag (CWorldSocket *pWorld, POSINTELLIPOSDRAG pInfo,  POSINTELLIPOSRESULT pResult) = 0;
      // This is FIRST called when an object is created in purgaturoy and the 
      // application is deciding  if it should click to place the object, or drag to
      // paste the object. The function will be passed all NULL's for parameters.
      // If it supports drag (linear) then returns TRUE. Otherwise returns FALSE.
      //
      // Once drag has taken place, this is called again with the information from
      // dragging

   virtual BOOL IntelligentAdjust (BOOL fAct) = 0;
      // Tells the object to intelligently adjust itself based on nearby objects.
      // For walls, this means triming to the roof line, for floors, different
      // textures, etc. If fAct is FALSE the function is just a query, that returns
      // TRUE if the object cares about adjustment and can try, FALSE if it can't.

   virtual BOOL Deconstruct (BOOL fAct) = 0;
      // Tells the object to deconstruct itself into sub-objects.
      // Basically, new objects will be added that exactly mimic this object,
      // and any embedeeding objects will be moved to the new ones.
      // NOTE: This old one will not be deleted - the called of Deconstruct()
      // will need to call Delete()
      // If fAct is FALSE the function is just a query, that returns
      // TRUE if the object cares about adjustment and can try, FALSE if it can't.

   virtual BOOL ControlPointQuery (DWORD dwID, POSCONTROL pInfo) = 0;
      // application passes in a control point ID (from 0 to 1023) and this will
      // fill in pInfo about the control point. Returns TRUE if successful,
      // FALSE if error (such as invalid dwID)

   virtual BOOL ControlPointSet (DWORD dwID, PCPoint pVal, PCPoint pViewer) = 0;
      // sets the control point to the given point (in object coordinates).
      // the control may fit pVal to a line or plane to make it valid,
      // and may apply min/max values to it, so don't assume that what was
      // set is actually kept. pViewer is a coordinate for the camera (in object
      // space). It's usually not used, but some objects may use the line from
      // pViewer to pVal to intersect the surface. pViewer may be NULL.

   virtual void ControlPointEnum (PCListFixed plDWORD) = 0;
      // the control will fill the list, which is an array of DWORDs, with
      // control IDs. These IDs can then be used for ControlPointQuery()
      // and ControlPointSet(). The function will NOT clear or initialize plDWORD.

   virtual PCObjectSurface SurfaceGet (DWORD dwID) = 0;
      // returns a pointer to the PCObjectSurface color/texture object describing
      // the surface on the object. dwID is from 0 to 1023. Returns NULL if there's
      // no such surface. The object MUST BE FREED by the caller

   virtual BOOL SurfaceSet (PCObjectSurface pos) = 0;
      // changes an object's surface to something new. Returns FALSE if there's
      // no such surface ID

   virtual BOOL DialogQuery (void) = 0;
      // asks an object if it supports its own dialog box for custom
      // settings. Returns TRUE if it does, FALSE if not.

   virtual BOOL DialogShow (DWORD dwSurface, PCEscWindow pWindow, PCPoint pClick) = 0;
      // if DialogQuery() returns TRUE, causes the object to bring up a dialog
      // for editing itself using pWindow. Returns 0 if the user presses close,
      // TRUE if the user presses a "Back" button. If dwSurface is not 0, then
      // it's the surface that the user clicked on. This may be useful for
      // narrowing down the UI choice..
      // pClick is the location (in object space) where user clicked on. Might be NULL.

   virtual BOOL DialogCPQuery (void) = 0;
      // asks an object if it supports its own dialog box for CONTROL POINT SELECTION
      // Returns TRUE if it does, FALSE if not.

   virtual BOOL DialogCPShow (DWORD dwSurface, PCEscWindow pWindow, PCPoint pClick) = 0;
      // if DialogCPQuery() returns TRUE, causes the object to bring up a dialog
      // for selecting CONTROL POINTS using pWindow. Returns 0 if the user presses close,
      // TRUE if the user presses a "Back" button. If dwSurface is not 0, then
      // it's the surface that the user clicked on. This may be useful for
      // narrowing down the UI choice.
      // pClick is the location (in object space) where user clicked on. Might be NULL.

   virtual BOOL EmbedQueryCanEmbed (void) = 0;
      // returns TRUE if the object is capable of being embedded in another
      // object. FALSE if its only standalone. If it's cabaple, it must
      // support the other EmbedXXX functions

   virtual BOOL EmbedContainerGet (GUID *pgCont) = 0;
      // Fills pgCont with the object ID of the container object holding
      // this object. If this object cannot be embedded, or isn't currently
      // embedded, this returns FALSE.

   virtual BOOL EmbedContainerSet (const GUID *pgCont) = 0;
      // Tells this object to use pgCont as the new container object.
      // Generally after this call the object will query the container about
      // it's location, etc., so the container should already be prepared
      // for the calls. If pgCont == NULL then the object has no container
      // NOTE: Generally this function is only called by
      // the container object, and not by the user.

   virtual BOOL EmbedContainerMoved (DWORD dwSurface, const PTEXTUREPOINT pHV, fp fRotation, PCMatrix pm) = 0;
      // Called by the container object into each of its embedded object
      // when the container is moved. Passes in a new translation/rotation
      // matrix from object space into world space (assuming HV of 0,0 is
      // the same as object space 0,0,0). The embedded object just needs to
      // remember this matrix, call pWorld->Objectabouttochange, and changed,
      // and use it for future reference. NOTE: The embedded object is still
      // in the same position relative to the container.
      // Just for extra information, dwSurface is the surface ID, pHV
      // is the texture point, fRotation is rotation.These shouldn't be changed

   virtual BOOL EmbedMovedWithinContainer (DWORD dwSurface, const PTEXTUREPOINT pHV, fp fRotation, PCMatrix pm) = 0;
      // Kind of like EmbedContainerMoved, except this time the object has
      // been moved within the container and may need to readjust its shape/
      // size, etc., or adjust the cutout it has within the container.
      // Surface, pHV, and fRotation are extra infromation not necessarily needed

   virtual BOOL EmbedContainerRenamed (const GUID *pgCont) = 0;
      // Called to tell the embedded object that it's container has a new ID
      // GUID, and that that should be used for future reference.

   virtual BOOL ContQueryCanContain (DWORD dwSurface = -1) = 0;
      // Call this to ask an object if it can contain other objects. If
      // dwSurface == -1, this returns TRUE if (in general) it can, or FALSE
      // if it can't. If dwSurface != -1, then this is asking for the specific
      // surface (gotten from LOWORD(IMAGEPIXEL)). If it can contain embedded
      // objects then this object must support the ContXXX functions

   virtual DWORD ContEmbeddedNum (void) = 0;
      // Returns the number of objects currently embedded in this container.

   virtual BOOL ContEmbeddedEnum (DWORD dwIndex, GUID *pgEmbed) = 0;
      // Given an index from 0 .. ContEmbeddedNum()-1, this filled in pgEmbed with
      // the embedded's guid

   virtual BOOL ContEmbeddedLocationGet (const GUID *pgEmbed, PTEXTUREPOINT pHV, fp *pfRotation, DWORD *pdwSurface) = 0;
      // Given an object GUID (that's embedded), fills in pHV with the location in HV coordinations (0..1,0..1)
      // in the embedded object's surface. pfRotation is filled with clockwise rotation
      // in radios. and pdwSurface with the surface ID, LOWORD(IMAGEPIXEL)

   virtual BOOL ContEmbeddedLocationSet (const GUID *pgEmbed, PTEXTUREPOINT pHV, fp fRotation, DWORD dwSurface) = 0;
      // Given an object GUID (that's already embedded), changes it's location with pHV,
      // rotation with fRotation, or surface with dwSurface.
      // This function must: Make sure object is alreayd embedded
      // that dwSurface and pHV area valid, then call
      // EmbedContainerSet() and EmbedMovedWithinContainer() into the object.

   virtual BOOL ContEmbeddedAdd (const GUID *pgEmbed, PTEXTUREPOINT pHV, fp fRotation, DWORD dwSurface) = 0;
      // Adds an object (which is not embedded) into the list. This function must: Make sure object not
      // already embedded, that it can be embedded, that dwSurface and pHV area valid, then call
      // EmbedContainerSet() and EmbedMovedWithinContainer() into the object.

   virtual BOOL ContMatrixFromHV (DWORD dwSurface, PTEXTUREPOINT pHV, fp fRotation, PCMatrix pm) = 0;
      // Objects can call this to get a rotation matrix that translates from embedded object
      // space (with surface == dwSurface, point on surface = pHV, and rotation = fRotation), into
      // world space. Fills pm with the matrix (if function succedes)

   virtual BOOL ContEmbeddedRemove (const GUID *pgEmbed) = 0;
      // Removed an object (which is embedded) from the list. This function does not
      // delete the object, but insteads throws it off into world space. It must call
      // EmbedContainerSet(), and clean up any internal data structures.

   virtual BOOL ContEmbeddedRenamed (const GUID *pgOld, const GUID *pgNew) = 0;
      // Called to tell the container that one of its embedded objects has been
      // renamed and has a new GUID. Note: pgOld is not guaranteed to be
      // in the list

   virtual BOOL ContHVQuery (PCPoint pEye, PCPoint pClick, DWORD dwSurface, PTEXTUREPOINT pOld, PTEXTUREPOINT pHV) = 0;
      // Called to see the HV location of what clicked on, or in other words, given a line
      // from pEye to pClick (object sapce), where does it intersect surface dwSurface (LOWORD(PIMAGEPIXEL.dwID))?
      // Fills pHV with the point. Returns FALSE if doesn't intersect, or invalid surface, or
      // can't tell with that surface. NOTE: if pOld is not NULL, then assume a drag from
      // pOld (converted to world object space) to pClick. Finds best intersection of the
      // surface then. Useful for dragging embedded objects around surface

   virtual BOOL ContCutout (const GUID *pgEmbed, DWORD dwNum, PCPoint paFront, PCPoint paBack, BOOL fBothSides) = 0;
      // Called by an embeded object to specify an arch cutout within the surface so the
      // object can go through the surface (like a window). The container should check
      // that pgEmbed is a valid object. dwNum is the number of points in the arch,
      // paFront and paBack are the container-object coordinates. (See CSplineSurface::ArchToCutout)
      // If dwNum is 0 then arch is simply removed. If fBothSides is true then both sides
      // of the surface are cleared away, else only the side where the object is embedded
      // is affected.

   virtual BOOL ContCutoutToZipper (const GUID *pgEmbed, PCListFixed plistFrontHVXYZ, PCListFixed plistBackHVXYZ) = 0;
      // Assumeing that ContCutout() has already been called for the pgEmbed, this askes the surfaces
      // for the zippering information (CRenderSufrace::ShapeZipper()) that would perfectly seal up
      // the cutout. plistXXXHVXYZ must be initialized to sizeof(HVXYZ). plisstFrontHVXYZ is filled with
      // the points on the side where the object was embedded, and plistBackHVXYZ are the opposite side.
      // NOTE: These are in the container's object space, NOT the embedded object's object space.

   virtual fp ContThickness (DWORD dwSurface, PTEXTUREPOINT pHV) = 0;
      // returns the thickness of the surface (dwSurface) at pHV. Used by embedded
      // objects like windows to know how deep they should be.

   virtual DWORD ContSideInfo (DWORD dwSurface, BOOL fOtherSide) = 0;
      // returns a DWORD indicating what class of surface it is... right now
      // 0 = internal, 1 = external. dwSurface is the surface. If fOtherSide
      // then want to know what's on the opposite side. Returns 0 if bad dwSurface.

   virtual DWORD AttachNum (void) = 0;
      // Returns the number of objects currently attached to this object.

   virtual BOOL AttachEnum (DWORD dwIndex, GUID *pgAttach) = 0;
      // Given an index from 0..AttachNum()-1, this fills in pgAttach with the attached
      // object's guid. 

   virtual BOOL AttachClosest (PCPoint pLoc, PWSTR pszClosest) = 0;
      // Given a point (pLoc) in the object's coords, returns a name (max 64 characters incl null)
      // and stores it in pszClosest. This name can then be used for adding an attachment
      // In the case of attaching to on object with bones, this will be the bone name

   virtual void AttachPointsEnum (PCListVariable plEnum) = 0;
      // Filled with the names of all the attachment points to the object. lpEnum is
      // filled with a list of NULL-terminated strings. The string can be paseed into
      // AttachAdd(). (AttachClosest() will also return one of these.)
      // This list might be empty if there are no specific points.

   virtual BOOL AttachAdd (const GUID *pgAttach, PWSTR pszAttach, PCMatrix pmMatrix) = 0;
      // Adds an attachment to the object identified by pgAttach (might already be on this
      // list, in which case morethan one copy), to the obhect usning the location pszAttach
      // (gotten from AttachClosest(). pmMatrix is a matrix that converts from the attached object's
      // space into world space. (Hence, defines the translation and rotation
      // needed to keep the attached object a constant position to the object.) The object
      // must also include rotations/translations to accound for pszAttach. NOTE: Assumes
      // attaching in the current skeleton location, so will also need to account for where
      // the object would be in the default skelton location

   virtual BOOL AttachRemove (DWORD dwIndex) = 0;
      // Removes an attachment from the list.

   virtual BOOL AttachRenamed (const GUID *pgOld, const GUID *pgNew) = 0;
      // Called to tell the object that an attachment has been renamed. Note: pgOld
      // is not guaranteed to be in the list. If not, just ignore it

   virtual fp OpenGet (void) = 0;
      // returns how open the object is, from 0 (closed) to 1.0 (open), or
      // < 0 for an object that can't be opened

   virtual BOOL OpenSet (fp fOpen) = 0;
      // opens/closes the object. if fopen==0 it's close, 1.0 = open, and
      // values in between are partially opened closed. Returne TRUE if success

   virtual fp TurnOnGet (void) = 0;
      // returns how "on" (or off) the object is, from 0 (off) to 1.0 (on), or
      // < 0 for an object that can't be opened

   virtual BOOL TurnOnSet (fp fOpen) = 0;
      // turns on/off the object. if fopen==0 it's off, 1.0 = on, and
      // values in between are partially half way. Returne TRUE if success

   virtual BOOL TextureQuery (PCListFixed plText) = 0;
      // asks the object what textures it uses. This allows the save-function
      // to save custom textures into the file. The object just ADDS (doesn't
      // clear or remove) elements, which are two guids in a row: the
      // gCode followed by the gSub of the object. Of course, it may add more
      // than one texture

   virtual BOOL ColorQuery (PCListFixed plColor) = 0;
      // asks the object what colors it uses (exclusive of textures).
      // It adds elements to plColor, which is a list of COLORREF. It may
      // add more than one color

   virtual BOOL ObjectClassQuery (PCListFixed plObj) = 0;
      // asks the curent object what other objects (including itself) it requires
      // so that when a file is saved, all user objects will be saved along with
      // the file, so people on other machines can load them in.
      // The object just ADDS (doesn't clear or remove) elements, which are two
      // guids in a row: gCode followed by gSub of the object. All objects
      // must add at least on (their own). Some, like CObjectEditor, will add
      // all its sub-objects too

   virtual BOOL Merge (GUID *pagWith, DWORD dwNum) = 0;
      // asks the object to merge with the list of objects (identified by GUID) in pagWith.
      // dwNum is the number of objects in the list. The object should see if it can
      // merge with any of the ones in the list (some of which may no longer exist and
      // one of which may be itself). If it does merge with any then it return TRUE.
      // if no merges take place it returns false.

   virtual BOOL AttribGet (PWSTR pszName, fp *pfValue) = 0;
      // given an attribute name, this fills in pfValue with the value of the attribute.
      // Returns FALSE if there's an error and cant fill in attribute

   virtual void AttribGetAll (PCListFixed plATTRIBVAL) = 0;
      // clears the list and fills it in with ATTRIBVAL strucutres describing
      // all the attributes.

   virtual void AttribSetGroup (DWORD dwNum, PATTRIBVAL paAttrib) = 0;
      // passed an array of ATTRIBVALL structures with new values to pass in
      // and set. THose attributes that are not known to the object are ignored.

   virtual BOOL AttribInfo (PWSTR pszName, PATTRIBINFO pInfo) = 0;
      // given an attribute name, this fills in information about the attribute.
      // Returns FALSE if can't find.


   virtual BOOL EditorCreate (BOOL fAct) = 0;
      // If fAct==FALSE, this asks the object if it can show an editor window.
      // This will return TRUE if the editor window was created (or already exists)
      // and FALSE if the object doesn't support an editor window.
      // If fAct==TRUE then this tries to create the editor window, returning
      // TRUE on success, FALSE on failure

   virtual BOOL EditorDestroy (void) = 0;
      // Tells the object to shut down its editor window. Returns TRUE if the object
      // doesn't habe an editor window or destruction is successful. FALSE if the
      // object has a window but it can't be closed now because the data is dirty.

   virtual BOOL EditorShowWindow (BOOL fShow) = 0;
      // if the editor window this does a ShowWindow(hWndEditor, SW_SHOW).
      // if fShow is FALSE if uses SW_HIDE. Called by the animator to make sure
      // that all windows in the system are hidden while animation happens
      // Returns true if there is an editor window open, FALSE if not
};
typedef CObjectSocket * PCObjectSocket;

/**********************************************************************************
CObjectTemplate */
// CObjectTemplate is a template object that does the basics but which can have
// its member variables overridden
class DLLEXPORT CObjectTemplate : public CObjectSocket, private CRenderSocket {
public:
   ESCNEWDELETE;

   CObjectTemplate (void);
   ~CObjectTemplate (void);

   // must be supplied by superclass
   //virtual void Delete (void) = 0;
   //virtual void Render (POBJECTRENDER pr) = 0;
   //virtual CObjectSocket *Clone (void) = 0;
   //virtual PCMMLNode2 MMLTo (void) = 0;
   //virtual BOOL MMLFrom (PCMMLNode2 pNode) = 0;
   
   // often overridden
   virtual BOOL Message (DWORD dwMessage, PVOID pParam);
   virtual BOOL MoveReferencePointQuery (DWORD dwIndex, PCPoint pp);
   virtual BOOL MoveReferenceStringQuery (DWORD dwIndex, PWSTR psz, DWORD dwSize, DWORD *pdwNeeded);
   virtual BOOL IntelligentPosition (CWorldSocket * pWorld, POSINTELLIPOS pInfo, POSINTELLIPOSRESULT pResult);
   virtual BOOL IntelligentPositionDrag (CWorldSocket *pWorld, POSINTELLIPOSDRAG pInfo,  POSINTELLIPOSRESULT pResult);
   virtual BOOL IntelligentAdjust (BOOL fAct);
   virtual BOOL Deconstruct (BOOL fAct);
   virtual BOOL ControlPointQuery (DWORD dwID, POSCONTROL pInfo);
   virtual BOOL ControlPointSet (DWORD dwID, PCPoint pVal, PCPoint pViewer);
   virtual void ControlPointEnum (PCListFixed plDWORD);
   virtual BOOL DialogQuery (void);
   virtual BOOL DialogShow (DWORD dwSurface, PCEscWindow pWindow, PCPoint pClick);
   virtual BOOL DialogCPQuery (void);
   virtual BOOL DialogCPShow (DWORD dwSurface, PCEscWindow pWindow, PCPoint pClick);
   virtual BOOL ContHVQuery (PCPoint pEye, PCPoint pClick, DWORD dwSurface, PTEXTUREPOINT pOld, PTEXTUREPOINT pHV);
   virtual BOOL ContCutout (const GUID *pgEmbed, DWORD dwNum, PCPoint paFront, PCPoint paBack, BOOL fBothSides);
   virtual BOOL ContCutoutToZipper (const GUID *pgEmbed, PCListFixed plistFrontHVXYZ, PCListFixed plistBackHVXYZ);
   virtual BOOL ContMatrixFromHV (DWORD dwSurface, PTEXTUREPOINT pHV, fp fRotation, PCMatrix pm);
   virtual fp ContThickness (DWORD dwSurface, PTEXTUREPOINT pHV);
   virtual DWORD ContSideInfo (DWORD dwSurface, BOOL fOtherSide);
   virtual BOOL EmbedDoCutout (void);
   virtual fp OpenGet (void);
   virtual BOOL OpenSet (fp fOpen);
   virtual BOOL LightQuery (PCListFixed pl, DWORD dwShow);
   virtual fp TurnOnGet (void);
   virtual BOOL TurnOnSet (fp fOpen);
   virtual BOOL TextureQuery (PCListFixed plText);
   virtual BOOL ColorQuery (PCListFixed plColor);
   virtual BOOL ObjectClassQuery (PCListFixed plObj);
   virtual BOOL Merge (GUID *pagWith, DWORD dwNum);
   virtual BOOL AttribGetIntern (PWSTR pszName, fp *pfValue);
   virtual void AttribGetAllIntern (PCListFixed plATTRIBVAL);
   virtual void AttribSetGroupIntern (DWORD dwNum, PATTRIBVAL paAttrib);
   virtual BOOL AttribInfoIntern (PWSTR pszName, PATTRIBINFO pInfo);
   virtual BOOL AttachClosest (PCPoint pLoc, PWSTR pszClosest);
   virtual void AttachPointsEnum (PCListVariable plEnum);
   virtual void InternAttachMatrix (PWSTR pszAttach, PCMatrix pmAttachRot);
   virtual BOOL EditorCreate (BOOL fAct);
   virtual BOOL EditorDestroy (void);
   virtual BOOL EditorShowWindow (BOOL fShow);
   virtual DWORD QuerySubObjects (void);
   virtual void PreRender (void);  // so that object editor will know what colors are available

   // internal functions to be called by the super-classing object
   PCMMLNode2 MMLToTemplate (void);
   BOOL MMLFromTemplate (PCMMLNode2 pNode);
   void CloneTemplate (CObjectTemplate *pCloneTo);
   void ObjectSurfaceAdd (DWORD dwID, COLORREF cColor, DWORD dwMaterialID, PWSTR pszScheme = NULL,
      const GUID *pgTextureCode = NULL, const GUID *pgTextureSub = NULL,
      PTEXTUREMODS pTextureMods = NULL, fp *pafTextureMatrix = NULL);
   void ObjectSurfaceAdd (PCObjectSurface posAdd);
   BOOL ObjectSurfaceRemove (DWORD dwID);
   PCObjectSurface ObjectSurfaceFind (DWORD dwID);
   DWORD ObjectSurfaceFindIndex (DWORD dwID);
   DWORD ObjectSurfaceGetIndex (DWORD dwIndex);
   DWORD ObjectSurfaceNumIndex (void);
   void ContainerSurfaceAdd (DWORD dwID);
   void ContainerSurfaceRemove (DWORD dwID);
   DWORD ContainerSurfaceIndex (DWORD dwID);
   DWORD ContainerSurfaceGetIndex (DWORD dwIndex);
   void TellAttachThatMoved (DWORD dwIndex = -1);

   CMatrix        m_MatrixObject;      // matrix that says where the object is and how it's rotated (and scaled)
   CWorldSocket         *m_pWorld;           // world object
   DWORD          m_dwMoveReferenceCur;// current move reference
   GUID           m_gGUID;             // object's GUID
   OSINFO         m_OSINFO;            // if the object sets this it doesn't need to provide the ::InfoGet call
   CMem           m_memName;           // memory containing the name
   CMem           m_memGroup;          // memory containing the group
   CMem           m_memAttribRank;     // memory containing attribute rankings
   BOOL           m_fObjShow;          // set to TRUE if is visible

   // embedding
   BOOL           m_fCanBeEmbedded;    // set to TRUE during constructor if object can be embedded
   fp             m_fContainerDepth;   // container depth at current location. Automagically filled when EmbedMovedInContainer called
   DWORD          m_dwContSideInfoThis;// ContSideInfo() flags for side embedded on. Filled along with container detph
   DWORD          m_dwContSideInfoOther; // Same as m_dwContSideInfoThis, except other side
   BOOL           m_fEmbedFitToFloor;  // if TRUE, this object will always move itself so that X,Z 0,0 is at a floor level.
                                       // NOTE: m_fEmbedFitToFloor is not saved in MML

   // type
   DWORD          m_dwType;            // HIWORD() is major type, LOWORD() is minor type
                                       // see constructor for info. Super-class can set and get this at will.
                                       // all template does is save the value
   DWORD          m_dwRenderShow;      // Collection of RENDERSHOW_XXX flags. Should be set by object on initiualization


   // usually NOT overridden
   virtual void InfoGet (POSINFO pInfo);
   virtual void QueryBoundingBox (PCPoint pCorner1, PCPoint pCorner2, DWORD dwSubObject);
   virtual BOOL ObjectMatrixSet (CMatrix *pObject);
   virtual void ObjectMatrixGet (CMatrix *pObject);
   virtual DWORD CategoryQuery (void);
   virtual void WorldSet (CWorldSocket *pWorld);
   virtual DWORD MoveReferenceCurQuery (void);
   virtual BOOL MoveReferenceCurSet (DWORD dwIndex);
   virtual void GUIDSet (GUID *pg);
   virtual void GUIDGet (GUID *pg);
   virtual PCObjectSurface SurfaceGet (DWORD dwID);
   virtual BOOL SurfaceSet (PCObjectSurface pos);
   virtual BOOL StringSet (DWORD dwString, PWSTR psz);
   virtual PWSTR StringGet (DWORD dwString);
   virtual void ShowSet (BOOL fShow);
   virtual BOOL ShowGet (void);
   virtual BOOL AttribGet (PWSTR pszName, fp *pfValue);  // do not override - use AttribGetIntern
   virtual void AttribGetAll (PCListFixed plATTRIBVAL); // do not override - use AttribGetAllIntern
   virtual void AttribSetGroup (DWORD dwNum, PATTRIBVAL paAttrib); // do not override - use AttribSetGroupIntern
   virtual BOOL AttribInfo (PWSTR pszName, PATTRIBINFO pInfo); // do not override - use AttribInfoIntern

   virtual BOOL EmbedQueryCanEmbed (void);
   virtual BOOL EmbedContainerGet (GUID *pgCont);
   virtual BOOL EmbedContainerSet (const GUID *pgCont);
   virtual BOOL EmbedContainerMoved (DWORD dwSurface, const PTEXTUREPOINT pHV, fp fRotation, PCMatrix pm) ;
   virtual BOOL EmbedMovedWithinContainer (DWORD dwSurface, const PTEXTUREPOINT pHV, fp fRotation, PCMatrix pm);
   virtual BOOL EmbedContainerRenamed (const GUID *pgCont);
   virtual BOOL ContEmbeddedRenamed (const GUID *pgOld, const GUID *pgNew);
   virtual BOOL ContQueryCanContain (DWORD dwSurface = -1);
   virtual DWORD ContEmbeddedNum (void);
   virtual BOOL ContEmbeddedEnum (DWORD dwIndex, GUID *pgEmbed);
   virtual BOOL ContEmbeddedLocationGet (const GUID *pgEmbed, PTEXTUREPOINT pHV, fp *pfRotation, DWORD *pdwSurface);
   virtual BOOL ContEmbeddedLocationSet (const GUID *pgEmbed, PTEXTUREPOINT pHV, fp fRotation, DWORD dwSurface);
   virtual BOOL ContEmbeddedAdd (const GUID *pgEmbed, PTEXTUREPOINT pHV, fp fRotation, DWORD dwSurface);
   virtual BOOL ContEmbeddedRemove (const GUID *pgEmbed);
   virtual void WorldSetFinished (void);
   virtual DWORD AttachNum (void);
   virtual BOOL AttachEnum (DWORD dwIndex, GUID *pgAttach);
   virtual BOOL AttachAdd (const GUID *pgAttach, PWSTR pszAttach, PCMatrix pmMatrix);
   virtual BOOL AttachRemove (DWORD dwIndex);
   virtual BOOL AttachRenamed (const GUID *pgOld, const GUID *pgNew);

private:
   // CRenderSocket used by QueryBoundingBox and other calls
   virtual BOOL QueryWantNormals (void);
   virtual BOOL QueryWantTextures (void);
   virtual fp QueryDetail (void);
                                                // this may change from one object to the next.
   virtual void MatrixSet (PCMatrix pm);
                                                // Note: THis matrix will be right-multiplied by any used by renderer for camera
   virtual void PolyRender (PPOLYRENDERINFO pInfo);
   virtual BOOL QuerySubDetail (PCMatrix pMatrix, PCPoint pBound1, PCPoint pBound2, fp *pfDetail);
   virtual BOOL QueryCloneRender (void);
   virtual BOOL CloneRender (GUID *pgCode, GUID *pgSub, DWORD dwNum, PCMatrix pamMatrix);


   CMatrix        m_MatrixRS;       // current matrix for the render socket MatrixSet()
   BOOL           m_fPolyAlreadyRendered; // set to TRUE if polyalready rendered called once
   CMatrix        m_MatrixObjectInv; // Set to the inverse of m_MatrixObject when query bounding box.
   CMatrix        m_MatrixRSTimeInv; // m_MatrixObjectInv x m_MatrixRS - to invert points from rendering
   CPoint         m_aPoint[2];      // min (0) and max(1) corners of bounding box
   CListFixed     m_listPCObjectSurface;  // list of object surfaces
   BOOL           m_fObjectSurfacesSorted;   // set to TRUE if m_listPCObjectSurface is sorted by dwID
   BOOL           m_fDeleting;      // set to true if deleting, so don't tell world of change to itself
   PCObjectPolyMesh m_pPolyMesh;    // if deconstructing, this is the pointer to the polymehs, else null

   // embedding
   BOOL           m_fEmbedded;      // set to true if embedded in object
   GUID           m_gContainer;     // set to the container's guid

   // container
   CListFixed     m_listContSurface;   // list of valid container surfaces
   CListFixed     m_listEMBEDINFO;     // list of embedded objects

   // attached
   CListFixed     m_lTATTACH;          // attach info
   BOOL           m_fAttachRecurse;    // flag to make sure dont recurse infinitely
};
typedef CObjectTemplate *PCObjectTemplate;

DLLEXPORT BOOL IsTemplateAttributeRot (PWSTR pszName, DWORD dwLen);
DLLEXPORT BOOL IsTemplateAttributeLoc (PWSTR pszName, DWORD dwLen);
DLLEXPORT BOOL IsTemplateAttribute (PWSTR pszName, BOOL fIncludeOpenOn);
DLLEXPORT PWSTR AttribOpenClose (void);
DLLEXPORT PWSTR AttribOnOff (void);


/*******************************************************************************
CRenderSurface */
DLLEXPORT void ApplyTextureRotation (PTEXTPOINT5 pat, DWORD dwNum, fp m[2][2], PCMatrix pMatrixXYZ);

// HVXYZ - Point with HZ and XYZ in it
typedef struct {
   fp      h,v;     // HV coordinates
   CPoint      p;       // XYZ coordinates
} HVXYZ, *PHVXYZ;


class DLLEXPORT CRenderSurface {
public:
   ESCNEWDELETE;

   CRenderSurface (void);
   ~CRenderSurface (void);

   void ClearAll (void);
   void Init (PCRenderSocket pSocket);    // need to call initialize
   void Commit (void);                    // commit data to m_pRenderMatrix

   void SetDefColor (COLORREF cColor);       // set the current color used by all objects
   void SetDefMaterial (PRENDERSURFACE pMaterial); // set the current material used by all objects
   void SetDefMaterial (DWORD dwRenderShard, PCObjectSurface pos, PCWorldSocket pWorld);      // set material based on object surface
   PCPoint NewPoints (DWORD *pdwIndex, DWORD dwNum = 1); // allocate several new points
   DWORD NewPoint (fp x, fp y, fp z);     // create a new point
   DWORD NewPoint (PCPoint p);
   PCPoint NewNormals (BOOL fNormalized, DWORD *pdwIndex, DWORD dwNum = 1); // allocate several new normals
   DWORD NewNormal (fp x, fp y, fp z, BOOL fNormalized = FALSE);  // creates a new normal and normalizes it
   DWORD NewNormal (PCPoint p1, PCPoint p2, PCPoint p3); // creates a normal to the three points and adds it
   PTEXTPOINT5 NewTextures (DWORD *pdwIndex, DWORD dwNum = 1); // allocate several new texture points
   DWORD NewTexture (fp h, fp v, fp x, fp y, fp z);  // creates a new texture point
   DWORD NewTexture (PCPoint p);          // creates a new texture based on a point
   DWORD DefNormal (DWORD dwDir);         // returns index to a default direction. 0=right,1=left,2=forwards,3=backwards,4=up,5=down
   COLORREF *NewColors (DWORD *pdwIndex, DWORD dwNum = 1);  // allocate several new colors
   DWORD    DefColor (void);              // returns the index to the default color
   COLORREF GetDefColor (void);           // returns the colorref of the default color
   PVERTEX NewVertices (DWORD *pdwIndex, DWORD dwNum = 1);  // allocate several new vertices, already fills def color
   DWORD NewVertex (DWORD dwPoint, DWORD dwNormal, DWORD dwTexture = 0);   // allocate just one vertex
   PPOLYDESCRIPT NewPolygon (DWORD dwNumVertices);  // allocates memory for a new polygon with given vertices, already fills def material
   PPOLYDESCRIPT NewPolygons (DWORD dwMem, DWORD dwNum);
   PPOLYDESCRIPT NewTriangle (DWORD dwV1, DWORD dwV2, DWORD dwV3, BOOL fCanBackfaceCull = TRUE);   // new triangle
   PPOLYDESCRIPT NewQuad (DWORD dwV1, DWORD dwV2, DWORD dwV3, DWORD dwV4, BOOL fCanBackfaceCull = TRUE);   // new quadrilateral
   PRENDERSURFACE NewSurfaces (DWORD *pdwIndex, DWORD dwNum = 1); // allocate several new surfaces
   DWORD    DefSurface (void);            // returns the index to the default surface
   PRENDERSURFACE GetDefSurface (void);
   DWORD NewIDPart (void);                 // increments m_wCurIDPart
   WORD NewIDMinor (void);                // increases minor ID in DefMaterial
   void SetIDPart (DWORD dwVal);           // set the minor ID
   void ApplyTextureRotation (PTEXTPOINT5 pat, DWORD dwNum);
   PCMatrix GetTextureRotation (fp m[2][2]);
   void TempTextureRotation (fp fAngle = 0);

   BOOL     m_fNeedNormals;               // filled by call to init. If FALSE all normal calls fail or area ignored
   BOOL     m_fNeedTextures;              // filled by a call to init. If FALSE, all texture calls fail or are ignroed
   fp       m_fDetail;                    // detail in meters requested. filled in by call to init()
   BOOL     m_fBonesAlreadyApplied;       // set this to TRUE so that bones won't be applied when render

   // shapes
   void ShapeBox (fp x1, fp y1, fp z1, fp x2, fp y2, fp z2);
   void ShapeSurface (DWORD dwType, DWORD dwX, DWORD dwY, PCPoint paPoints,
      DWORD dwPointIndex, PCPoint paNormals = NULL, DWORD dwNormalIndex = 0,
      BOOL fCanBackfaceCull = TRUE,
      PCPoint pNormTop = NULL, PCPoint pNormBottom = NULL, PCPoint pNormLeft = NULL, PCPoint pNormRight = NULL,
      DWORD dwRepeatH = 0, DWORD dwRepeatV = 0);
   void ShapeSurfaceBezier (DWORD dwType, DWORD dwX, DWORD dwY, PCPoint paPoints,
      BOOL fCanBackfaceCull = TRUE);
   void ShapeEllipsoid (fp x, fp y, fp z);
   void ShapeSphere (fp x)
      {
         ShapeEllipsoid (x,x,x);
      };
   void ShapeRotation (DWORD dwNum, PCPoint paPoints, BOOL fBezier = TRUE, BOOL fCanBackfaceCull = TRUE);
   void ShapeFunnel (fp fHeight = 1.0, fp fWidthBase = 1.0, fp fWidthTop = 0.0, BOOL fCanBackfaceCull = TRUE);
   void ShapeQuad (PCPoint pUL, PCPoint pUR, PCPoint pLR, PCPoint pLL,
                                   BOOL fCanBackfaceCull = TRUE);
   void ShapeQuadQuick (PCPoint pUL, PCPoint pUR, PCPoint pLR, PCPoint pLL,
      DWORD dwNorm, int iTextH, int iTextV, BOOL fCanBackfaceCull = TRUE);   // new quadrilateral
   void ShapeTube (fp fHeight = 1.0, fp fWidth = 1.0)
      {
         ShapeFunnel (fHeight, fWidth, fWidth);
      };
   void ShapeTeapot (void);
   void ShapeZipper (PHVXYZ pLine1, DWORD dwNum1, PHVXYZ pLine2, DWORD dwNum2, BOOL fLooped,
      fp fClipVMin = 0, fp fClipVMax = 1, BOOL fClipTriangle = TRUE);

   // from CRenderMatrix - any time matrix function called Commit() automatically called
   void Push (void);
   BOOL Pop (void);
   void Set (const PCMatrix pSet);
   void Get (PCMatrix pGet);
   void Multiply (const PCMatrix pm);
   void Translate (fp x = 0.0, fp y = 0.0, fp z = 0.0);   // translates in x,y,z
   void Rotate (fp fRadians, DWORD dwDim);  // Rotate. dwdim = 1 for x, 2 for y, 3 for z
   void Scale (fp fScale);   // universally scale
   void Scale (fp x, fp y, fp z);   // scale in x, y, z
   void TransRot (const PCPoint p1, const PCPoint p2);
   void Clear (void);

private:
   PCRenderSocket    m_pSocket;           // call into this for rendering functions
   CRenderMatrix     m_RenderMatrix;      // matrix functions
   BOOL              m_fUseTextMatrixTemp;   // if TRUE use the temporary texture matrix
   fp                m_aTextMatrixHVTemp[2][2];   // temporary rotation matrix that's applied

   // memory for arrays
   CMem              m_memPoints;         // memory for points
   CMem              m_memNormals;        // memory for normals
   BOOL              m_fAllNormalsNormalized;   // TRUE if all normals so far have been normalized
   CMem              m_memTextures;       // memory for textures
   CMem              m_memVertices;       // memory for vertices
   CMem              m_memColors;         // memory for colors
   CMem              m_memSurfaces;       // memory for surfaces
   CMem              m_memPolygons;       // memory for polygons
   DWORD             m_dwNumPolygons;     // number of polygons

   // defaults
   DWORD             m_dwIndexDefColor;   // index to the default color, -1 if not set
   DWORD             m_dwIndexDefSurface; // index to the default surface, -1 if not set
   COLORREF          m_cDefColor;         // default color
   RENDERSURFACE     m_DefRenderSurface;  // default rendering surface
   DWORD             m_dwCurIDPart;       // current ID part. Incremented for every hard edge
   DWORD             m_adwIndexDefDirection[6]; // index for defaul directions (see DefNormal()), -1 if not set



};
typedef CRenderSurface *PCRenderSurface;


/**********************************************************************************
CObjectTestBox */
// {8F7525C1-0B82-4799-AB76-8EBA45ED2358}
DEFINE_GUID(CLSID_TESTBOX, 
0x8f7525c1, 0xb82, 0x4799, 0xab, 0x76, 0x8e, 0xba, 0x45, 0xed, 0x23, 0x58);

// CObjectTestBox is a test object to test the system
class DLLEXPORT CObjectTestBox : public CObjectTemplate {
public:
   CObjectTestBox (PVOID pParams, POSINFO pInfo);
   ~CObjectTestBox (void);

   virtual void Delete (void);
   virtual void Render (POBJECTRENDER pr, DWORD dwSubObject);
   virtual CObjectSocket *Clone (void);
   virtual PCMMLNode2 MMLTo (void);
   virtual BOOL MMLFrom (PCMMLNode2 pNode);
   virtual BOOL ControlPointQuery (DWORD dwID, POSCONTROL pInfo);
   virtual BOOL ControlPointSet (DWORD dwID, PCPoint pVal, PCPoint pViewer);
   virtual void ControlPointEnum (PCListFixed plDWORD);
   virtual void QueryBoundingBox (PCPoint pCorner1, PCPoint pCorner2, DWORD dwSubObject);

   CPoint   m_pCorner[2];    // two corners that define the box

   CRenderSurface    m_Renderrs;    // to minimize mallocs
};
typedef CObjectTestBox *PCObjectTestBox;




/**********************************************************************************
CObjectGrass */
// {8F7525C1-0B82-4799-AB76-8EBA45ED2358}
DEFINE_GUID(CLSID_Grass, 
0x9f7525c3, 0xb82, 0x47a9, 0xfb, 0x76, 0x1e, 0xba, 0xc5, 0xed, 0x24, 0x58);

// CObjectGrass is a test object to test the system
class DLLEXPORT CObjectGrass : public CObjectTemplate {
public:
   CObjectGrass (PVOID pParams, POSINFO pInfo);
   ~CObjectGrass (void);

   virtual void Delete (void);
   virtual void Render (POBJECTRENDER pr, DWORD dwSubObject);
   virtual CObjectSocket *Clone (void);
   virtual PCMMLNode2 MMLTo (void);
   virtual BOOL MMLFrom (PCMMLNode2 pNode);
   virtual BOOL ControlPointQuery (DWORD dwID, POSCONTROL pInfo);
   virtual BOOL ControlPointSet (DWORD dwID, PCPoint pVal, PCPoint pViewer);
   virtual void ControlPointEnum (PCListFixed plDWORD);
   virtual void QueryBoundingBox (PCPoint pCorner1, PCPoint pCorner2, DWORD dwSubObject);

   CPoint   m_pCorner;    // corner that defines box

   CRenderSurface    m_Renderrs;       // for malloc optimization
};
typedef CObjectGrass *PCObjectGrass;




/**********************************************************************************
CObjectCave */
// {8F7525C1-0B82-4799-AB76-8EBA45ED2358}
DEFINE_GUID(CLSID_Cave, 
0x2f7f65c8, 0xab42, 0x93a9, 0xfb, 0xc1, 0x1e, 0xba, 0xc5, 0xed, 0x24, 0xa5);

class CMetaball;
typedef CMetaball *PCMetaball;

class CCaveCanopy;
typedef CCaveCanopy *PCCaveCanopy;
#define NUMCAVECANOPY            5     // types of cave canopy
#define CC_GROUND                0     // cave canopy for ground
#define CC_WALL                  1     // on wall
#define CC_CEILING               2     // on ceiling of cave
#define CC_STALAGTITE            3     // stalagtite
#define CC_STALAGMITE            4     // stalagmite


// CObjectCave is a test object to test the system
class DLLEXPORT CObjectCave : public CObjectTemplate {
public:
   CObjectCave (PVOID pParams, POSINFO pInfo);
   ~CObjectCave (void);

   virtual void Delete (void);
   virtual void Render (POBJECTRENDER pr, DWORD dwSubObject);
   virtual CObjectSocket *Clone (void);
   virtual PCMMLNode2 MMLTo (void);
   virtual BOOL MMLFrom (PCMMLNode2 pNode);
   virtual BOOL ControlPointQuery (DWORD dwID, POSCONTROL pInfo);
   virtual BOOL ControlPointSet (DWORD dwID, PCPoint pVal, PCPoint pViewer);
   virtual void ControlPointEnum (PCListFixed plDWORD);
   virtual BOOL DialogQuery (void);
   virtual BOOL DialogShow (DWORD dwSurface, PCEscWindow pWindow, PCPoint pClick);
   virtual void QueryBoundingBox (PCPoint pCorner1, PCPoint pCorner2, DWORD dwSubObject);
   virtual DWORD QuerySubObjects (void);
   virtual BOOL Deconstruct (BOOL fAct);
   virtual BOOL Merge (GUID *pagWith, DWORD dwNum);
   virtual BOOL Message (DWORD dwMessage, PVOID pParam);
   virtual BOOL TextureQuery (PCListFixed plText);
   virtual BOOL ColorQuery (PCListFixed plColor);
   virtual BOOL ObjectClassQuery (PCListFixed plObj);

   void Dirty (short *paiMin, short *paiMax);
   void Dirty (PCMetaball pmb);
   void CalcIfNecessary (void);
   BOOL DeconstructIndividual (PCMetaball pmb);
   BOOL DeleteMetaball (DWORD dwIndex);
   BOOL MergeIndividual (PCObjectCave pMerge);
   void ChangedCanopy (PCCaveCanopy pCanopy);

   CListFixed  m_lPCMetaball;    // list of metaballs in the object
   CListFixed  m_lPCMetaSurface; // surfaces used by the metaball
   CPoint      m_pNoiseStrength; // strength of the noise
   CPoint      m_pNoiseDetail;   // size of the noise, in meters
   DWORD       m_dwMetaSel;      // metaball selected, from 0 .. m_lPCMetaball.Num()-1
   DWORD       m_dwSeed;         // random seed
   BOOL        m_fSeenFromInside;   // checked if generally seen from the inside
   BOOL        m_fBackface;      // set to TRUE if can backface cull
   BOOL        m_fBoxes;         // if TRUE, draw texture objects as boxes

   CRenderSurface    m_Renderrs;    // to minimize mallocs
};
typedef CObjectCave *PCObjectCave;


/**********************************************************************************
CObjectRock */
// {8F7525C1-0B82-4799-AB76-8EBA45ED2358}
DEFINE_GUID(CLSID_Rock, 
0x9f86254a, 0xc192, 0x47a5, 0xfb, 0x76, 0x1e, 0xbf, 0x65, 0xed, 0x24, 0x92);

DEFINE_GUID(GTEXTURECODE_Rock,
0xbfa41866, 0xa33d, 0x4e95, 0x98, 0xcb, 0x65, 0x2e, 0x6, 0x1d, 0xef, 0x1b);
DEFINE_GUID(GTEXTURESUB_Rock,
0x715, 0x18f, 0x11c0, 0xc, 0xc1, 0xc3, 0x88, 0x27, 0x2f, 0xc5, 0x1);

// CObjectRock is a test object to test the system
class DLLEXPORT CObjectRock : public CObjectTemplate {
public:
   CObjectRock (PVOID pParams, POSINFO pInfo);
   ~CObjectRock (void);

   virtual void Delete (void);
   virtual void Render (POBJECTRENDER pr, DWORD dwSubObject);
   virtual CObjectSocket *Clone (void);
   virtual PCMMLNode2 MMLTo (void);
   virtual BOOL MMLFrom (PCMMLNode2 pNode);
   virtual BOOL ControlPointQuery (DWORD dwID, POSCONTROL pInfo);
   virtual BOOL ControlPointSet (DWORD dwID, PCPoint pVal, PCPoint pViewer);
   virtual void ControlPointEnum (PCListFixed plDWORD);
   virtual BOOL MoveReferencePointQuery (DWORD dwIndex, PCPoint pp);
   virtual BOOL MoveReferenceStringQuery (DWORD dwIndex, PWSTR psz, DWORD dwSize, DWORD *pdwNeeded);
   virtual BOOL DialogQuery (void);
   virtual BOOL DialogShow (DWORD dwSurface, PCEscWindow pWindow, PCPoint pClick);
   virtual void QueryBoundingBox (PCPoint pCorner1, PCPoint pCorner2, DWORD dwSubObject);

   void CalcInfoIfNecessary (void);
   void FractalNoise (int iLong, int iLat, DWORD dwSize, BOOL fAbove, BOOL fRight,
                                DWORD dwFactor, fp fNoiseScale, PCPoint paScratch);

   CPoint         m_pCorner;    // corner that defines box
   CPoint         m_pSuperQuad;  // super quadric info. 0 = round, +1 = square
   CPoint         m_apEdge[8];   // low-bit = x, then y, then z. Values range from -1 to 1
   BOOL           m_fStretchToFit;  // if TRUE then stretch 2d texture to fit over entire rock
   DWORD          m_dwSize;      // number of points, best is power of 2 or 3, for fractal bit
   CPoint         m_pNoise;      // noise at various levels of detail, 0, 1, 2, and 3+.
   int            m_iSeed;       // noise seed

   BOOL           m_fDirty;      // set to TRUE if calculated information is dirty;
   CMem           m_memCalc;     // calculated info
   PCPoint        m_paPoint;     // points
   PCPoint        m_paNorm;      // normals
   PTEXTPOINT5    m_patpText;    // textures

   CRenderSurface    m_Renderrs;       // for malloc optimization
};
typedef CObjectRock *PCObjectRock;

/**********************************************************************************
CObjectWaterfall */
// {8F7525C1-0B82-4799-AB76-8EBA45ED2358}
DEFINE_GUID(CLSID_Waterfall, 
0xac7523cb, 0xb82, 0x97aa, 0xfb, 0x76, 0x1e, 0xba, 0xc5, 0xed, 0xf4, 0xb8);

// CObjectWaterfall is a test object to test the system
class DLLEXPORT CObjectWaterfall : public CObjectTemplate {
public:
   CObjectWaterfall (PVOID pParams, POSINFO pInfo);
   ~CObjectWaterfall (void);

   virtual void Delete (void);
   virtual void Render (POBJECTRENDER pr, DWORD dwSubObject);
   virtual CObjectSocket *Clone (void);
   virtual PCMMLNode2 MMLTo (void);
   virtual BOOL MMLFrom (PCMMLNode2 pNode);
   virtual BOOL ControlPointQuery (DWORD dwID, POSCONTROL pInfo);
   virtual BOOL ControlPointSet (DWORD dwID, PCPoint pVal, PCPoint pViewer);
   virtual void ControlPointEnum (PCListFixed plDWORD);
   virtual BOOL AttribGetIntern (PWSTR pszName, fp *pfValue);
   virtual void AttribGetAllIntern (PCListFixed plATTRIBVAL);
   virtual void AttribSetGroupIntern (DWORD dwNum, PATTRIBVAL paAttrib);
   virtual BOOL AttribInfoIntern (PWSTR pszName, PATTRIBINFO pInfo);
   virtual BOOL DialogQuery (void);
   virtual BOOL DialogShow (DWORD dwSurface, PCEscWindow pWindow, PCPoint pClick);
   virtual void QueryBoundingBox (PCPoint pCorner1, PCPoint pCorner2, DWORD dwSubObject);

   int DrawFlow (PCRenderSurface prs, fp fCenter, fp fWidth, fp fDepthTop, fp fDepthBase,
      fp fTimeStart, fp fTimeEnd, fp fPreHeight, fp fPostHeight);
   void CalcIfDirty (void);

   CPoint   m_pCorner;     // corner of lower edge of waterfall. x = positive, y = negative, z = negative
   fp       m_fG;          // gravitational constant to use, usually 9.8 m/s/s
   fp       m_fWidth;      // width of the waterfall at the top, in meters
   int      m_iSeed;       // random seed
   DWORD    m_dwFlows;     // number of flows
   fp       m_fFlowDepthTop;  // depth (in meters) of the waterfall at its crest
   fp       m_fFlowDepthBase; // depth (in meters) of the waterfall at its base
   fp       m_fTimeStart;  // how much of the waterfall is drawn before it crests
   TEXTUREPOINT m_tpFlowAir;  // air between water, .h = min in sec, .v = max in sec
   TEXTUREPOINT m_tpFlowWater; // water, .h = min in sec, .v = max in sec
   fp       m_fTime;       // current time
   fp       m_fFlowWidth;  // flow width, from 0 to 1
   CListFixed m_lFlowObstruct;   // where flow is obstructed, from 0..1, list of fp

   // calculated
   BOOL     m_fDirty;      // set to TRUE if flow calcualtions dirty
   CMem     m_memInfo;     // memory with flow information
   PCPoint  m_paFlows;     // inforamtion about the flows, m_dwFlows x PATTERNREPEAT entries. p[0] = length in sec, p[1] = offset L/R, p[2] = offset F/B
   fp       *m_pafFlowTimeOffset; // flow time offset, m_dwFlows entries
   fp       *m_pafFlowTimeLoop;   // number of seconds for a loop to occur, m_dwFlowsEntries

   CRenderSurface    m_Renderrs;    // to minimize mallocs
};
typedef CObjectWaterfall *PCObjectWaterfall;



#if 0 // DEAD code

/**********************************************************************************
CObjectTree */
// {F356429E-27FA-46b0-835E-115237B4BB05}
DEFINE_GUID(CLSID_Tree, 
0xf356429e, 0x27fa, 0x46b0, 0x83, 0x5e, 0x11, 0x52, 0x37, 0xb4, 0xbb, 0x5);

// {EA3D40EA-AB77-44b8-AE6D-19ABCF719958}
DEFINE_GUID(GUID_TreeConifer1, 
0xea3d40ea, 0xab77, 0x44b8, 0xae, 0x6d, 0x19, 0xab, 0xcf, 0x71, 0x99, 0x01);
DEFINE_GUID(GUID_TreeConifer2, 
0xea3d40ea, 0xab77, 0x44b8, 0xae, 0x6d, 0x19, 0xab, 0xcf, 0x71, 0x99, 0x02);
DEFINE_GUID(GUID_TreeConifer3, 
0xea3d40ea, 0xab77, 0x44b8, 0xae, 0x6d, 0x19, 0xab, 0xcf, 0x71, 0x99, 0x03);
DEFINE_GUID(GUID_TreeConifer4, 
0xea3d40ea, 0xab77, 0x44b8, 0xae, 0x6d, 0x19, 0xab, 0xcf, 0x71, 0x99, 0x04);
DEFINE_GUID(GUID_TreeConifer5, 
0xea3d40ea, 0xab77, 0x44b8, 0xae, 0x6d, 0x19, 0xab, 0xcf, 0x71, 0x99, 0x05);
DEFINE_GUID(GUID_TreeConifer6, 
0xea3d40ea, 0xab77, 0x44b8, 0xae, 0x6d, 0x19, 0xab, 0xcf, 0x71, 0x99, 0x06);
DEFINE_GUID(GUID_TreeConifer7, 
0xea3d40ea, 0xab77, 0x44b8, 0xae, 0x6d, 0x19, 0xab, 0xcf, 0x71, 0x99, 0x07);
DEFINE_GUID(GUID_TreeConifer8, 
0xea3d40ea, 0xab77, 0x44b8, 0xae, 0x6d, 0x19, 0xab, 0xcf, 0x71, 0x99, 0x08);
DEFINE_GUID(GUID_TreeConifer9, 
0xea3d40ea, 0xab77, 0x44b8, 0xae, 0x6d, 0x19, 0xab, 0xcf, 0x71, 0x99, 0x09);

// {242D8F98-9258-4652-BEE9-A4B4FA5A88D6}
DEFINE_GUID(GUID_TreePalm1, 
0x242d8f98, 0x9258, 0x4652, 0xbe, 0xe9, 0xa4, 0xb4, 0xfa, 0x5a, 0x88, 0x01);
DEFINE_GUID(GUID_TreePalm2, 
0x242d8f98, 0x9258, 0x4652, 0xbe, 0xe9, 0xa4, 0xb4, 0xfa, 0x5a, 0x88, 0x02);
DEFINE_GUID(GUID_TreePalm3, 
0x242d8f98, 0x9258, 0x4652, 0xbe, 0xe9, 0xa4, 0xb4, 0xfa, 0x5a, 0x88, 0x03);
DEFINE_GUID(GUID_TreePalm4, 
0x242d8f98, 0x9258, 0x4652, 0xbe, 0xe9, 0xa4, 0xb4, 0xfa, 0x5a, 0x88, 0x04);
DEFINE_GUID(GUID_TreePalm5, 
0x242d8f98, 0x9258, 0x4652, 0xbe, 0xe9, 0xa4, 0xb4, 0xfa, 0x5a, 0x88, 0x05);

// {9D367063-0841-4f96-9899-42D6FD2AB8FD}
DEFINE_GUID(GUID_TreeEverLeaf1, 
0x9d367063, 0x841, 0x4f96, 0x98, 0x99, 0x42, 0xd6, 0xfd, 0x2a, 0xb8, 0x01);
DEFINE_GUID(GUID_TreeEverLeaf2, 
0x9d367063, 0x841, 0x4f96, 0x98, 0x99, 0x42, 0xd6, 0xfd, 0x2a, 0xb8, 0x02);
DEFINE_GUID(GUID_TreeEverLeaf3, 
0x9d367063, 0x841, 0x4f96, 0x98, 0x99, 0x42, 0xd6, 0xfd, 0x2a, 0xb8, 0x03);
DEFINE_GUID(GUID_TreeEverLeaf4, 
0x9d367063, 0x841, 0x4f96, 0x98, 0x99, 0x42, 0xd6, 0xfd, 0x2a, 0xb8, 0x04);
DEFINE_GUID(GUID_TreeEverLeaf5, 
0x9d367063, 0x841, 0x4f96, 0x98, 0x99, 0x42, 0xd6, 0xfd, 0x2a, 0xb8, 0x05);
DEFINE_GUID(GUID_TreeEverLeaf6, 
0x9d367063, 0x841, 0x4f96, 0x98, 0x99, 0x42, 0xd6, 0xfd, 0x2a, 0xb8, 0x06);
DEFINE_GUID(GUID_TreeEverLeaf7, 
0x9d367063, 0x841, 0x4f96, 0x98, 0x99, 0x42, 0xd6, 0xfd, 0x2a, 0xb8, 0x07);
DEFINE_GUID(GUID_TreeEverLeaf8, 
0x9d367063, 0x841, 0x4f96, 0x98, 0x99, 0x42, 0xd6, 0xfd, 0x2a, 0xb8, 0x08);

// {41D2103C-49CF-498f-83B6-C040AD017193}
DEFINE_GUID(GUID_TreeDeciduous1, 
0x41d2103c, 0x49cf, 0x498f, 0x83, 0xb6, 0xc0, 0x40, 0xad, 0x1, 0x71, 0x01);
DEFINE_GUID(GUID_TreeDeciduous2, 
0x41d2103c, 0x49cf, 0x498f, 0x83, 0xb6, 0xc0, 0x40, 0xad, 0x1, 0x71, 0x02);
DEFINE_GUID(GUID_TreeDeciduous3, 
0x41d2103c, 0x49cf, 0x498f, 0x83, 0xb6, 0xc0, 0x40, 0xad, 0x1, 0x71, 0x03);
DEFINE_GUID(GUID_TreeDeciduous4, 
0x41d2103c, 0x49cf, 0x498f, 0x83, 0xb6, 0xc0, 0x40, 0xad, 0x1, 0x71, 0x04);
DEFINE_GUID(GUID_TreeDeciduous5, 
0x41d2103c, 0x49cf, 0x498f, 0x83, 0xb6, 0xc0, 0x40, 0xad, 0x1, 0x71, 0x05);
DEFINE_GUID(GUID_TreeDeciduous6, 
0x41d2103c, 0x49cf, 0x498f, 0x83, 0xb6, 0xc0, 0x40, 0xad, 0x1, 0x71, 0x06);
DEFINE_GUID(GUID_TreeDeciduous7, 
0x41d2103c, 0x49cf, 0x498f, 0x83, 0xb6, 0xc0, 0x40, 0xad, 0x1, 0x71, 0x07);
DEFINE_GUID(GUID_TreeDeciduous8, 
0x41d2103c, 0x49cf, 0x498f, 0x83, 0xb6, 0xc0, 0x40, 0xad, 0x1, 0x71, 0x08);
DEFINE_GUID(GUID_TreeDeciduous9, 
0x41d2103c, 0x49cf, 0x498f, 0x83, 0xb6, 0xc0, 0x40, 0xad, 0x1, 0x71, 0x09);
DEFINE_GUID(GUID_TreeDeciduous10, 
0x41d2103c, 0x49cf, 0x498f, 0x83, 0xb6, 0xc0, 0x40, 0xad, 0x1, 0x71, 0x10);

// {6E98B907-2CE1-42d0-9C77-35F1788AE484}
DEFINE_GUID(GUID_TreeMisc1, 
0x6e98b907, 0x2ce1, 0x42d0, 0x9c, 0x77, 0x35, 0xf1, 0x78, 0x8a, 0xe4, 0x01);
DEFINE_GUID(GUID_TreeMisc2, 
0x6e98b907, 0x2ce1, 0x42d0, 0x9c, 0x77, 0x35, 0xf1, 0x78, 0x8a, 0xe4, 0x02);
DEFINE_GUID(GUID_TreeMisc3, 
0x6e98b907, 0x2ce1, 0x42d0, 0x9c, 0x77, 0x35, 0xf1, 0x78, 0x8a, 0xe4, 0x03);
DEFINE_GUID(GUID_TreeMisc4, 
0x6e98b907, 0x2ce1, 0x42d0, 0x9c, 0x77, 0x35, 0xf1, 0x78, 0x8a, 0xe4, 0x04);

// {B2420FFF-2D9C-4a3e-AEA4-0E5AFEAD14B4}
DEFINE_GUID(GUID_TreeShrub1, 
0xb2420fff, 0x2d9c, 0x4a3e, 0xae, 0xa4, 0xe, 0x5a, 0xfe, 0xad, 0x14, 0x01);
DEFINE_GUID(GUID_TreeShrub2, 
0xb2420fff, 0x2d9c, 0x4a3e, 0xae, 0xa4, 0xe, 0x5a, 0xfe, 0xad, 0x14, 0x02);
DEFINE_GUID(GUID_TreeShrub3, 
0xb2420fff, 0x2d9c, 0x4a3e, 0xae, 0xa4, 0xe, 0x5a, 0xfe, 0xad, 0x14, 0x03);
DEFINE_GUID(GUID_TreeShrub4, 
0xb2420fff, 0x2d9c, 0x4a3e, 0xae, 0xa4, 0xe, 0x5a, 0xfe, 0xad, 0x14, 0x04);
DEFINE_GUID(GUID_TreeShrub5, 
0xb2420fff, 0x2d9c, 0x4a3e, 0xae, 0xa4, 0xe, 0x5a, 0xfe, 0xad, 0x14, 0x05);
DEFINE_GUID(GUID_TreeShrub6, 
0xb2420fff, 0x2d9c, 0x4a3e, 0xae, 0xa4, 0xe, 0x5a, 0xfe, 0xad, 0x14, 0x06);
DEFINE_GUID(GUID_TreeShrub7, 
0xb2420fff, 0x2d9c, 0x4a3e, 0xae, 0xa4, 0xe, 0x5a, 0xfe, 0xad, 0x14, 0x07);
DEFINE_GUID(GUID_TreeShrub8, 
0xb2420fff, 0x2d9c, 0x4a3e, 0xae, 0xa4, 0xe, 0x5a, 0xfe, 0xad, 0x14, 0x08);
DEFINE_GUID(GUID_TreeShrub9, 
0xb2420fff, 0x2d9c, 0x4a3e, 0xae, 0xa4, 0xe, 0x5a, 0xfe, 0xad, 0x14, 0x09);
DEFINE_GUID(GUID_TreeShrub10, 
0xb2420fff, 0x2d9c, 0x4a3e, 0xae, 0xa4, 0xe, 0x5a, 0xfe, 0xad, 0x14, 0x10);
DEFINE_GUID(GUID_TreeShrub11, 
0xb2420fff, 0x2d9c, 0x4a3e, 0xae, 0xa4, 0xe, 0x5a, 0xfe, 0xad, 0x14, 0x11);
DEFINE_GUID(GUID_TreeShrub12, 
0xb2420fff, 0x2d9c, 0x4a3e, 0xae, 0xa4, 0xe, 0x5a, 0xfe, 0xad, 0x14, 0x12);
DEFINE_GUID(GUID_TreeShrub13, 
0xb2420fff, 0x2d9c, 0x4a3e, 0xae, 0xa4, 0xe, 0x5a, 0xfe, 0xad, 0x14, 0x13);

// {4AB17CB5-44BA-4f3e-9BD2-B2FE21EDE450}
DEFINE_GUID(GUID_TreeGround1, 
0x4ab17cb5, 0x44ba, 0x4f3e, 0x9b, 0xd2, 0xb2, 0xfe, 0x21, 0xed, 0xe4, 0x01);
DEFINE_GUID(GUID_TreeGround2, 
0x4ab17cb5, 0x44ba, 0x4f3e, 0x9b, 0xd2, 0xb2, 0xfe, 0x21, 0xed, 0xe4, 0x02);

// {DB6BE67D-93B3-470b-8A0D-0E66A1937CE4}
DEFINE_GUID(GUID_TreePot1, 
0xdb6be67d, 0x93b3, 0x470b, 0x8a, 0xd, 0xe, 0x66, 0xa1, 0x93, 0x7c, 0x01);
DEFINE_GUID(GUID_TreePot2, 
0xdb6be67d, 0x93b3, 0x470b, 0x8a, 0xd, 0xe, 0x66, 0xa1, 0x93, 0x7c, 0x02);
DEFINE_GUID(GUID_TreePot3, 
0xdb6be67d, 0x93b3, 0x470b, 0x8a, 0xd, 0xe, 0x66, 0xa1, 0x93, 0x7c, 0x03);
DEFINE_GUID(GUID_TreePot4, 
0xdb6be67d, 0x93b3, 0x470b, 0x8a, 0xd, 0xe, 0x66, 0xa1, 0x93, 0x7c, 0x04);
DEFINE_GUID(GUID_TreePot5, 
0xdb6be67d, 0x93b3, 0x470b, 0x8a, 0xd, 0xe, 0x66, 0xa1, 0x93, 0x7c, 0x05);
DEFINE_GUID(GUID_TreePot6, 
0xdb6be67d, 0x93b3, 0x470b, 0x8a, 0xd, 0xe, 0x66, 0xa1, 0x93, 0x7c, 0x06);
DEFINE_GUID(GUID_TreePot7, 
0xdb6be67d, 0x93b3, 0x470b, 0x8a, 0xd, 0xe, 0x66, 0xa1, 0x93, 0x7c, 0x07);
DEFINE_GUID(GUID_TreePot8, 
0xdb6be67d, 0x93b3, 0x470b, 0x8a, 0xd, 0xe, 0x66, 0xa1, 0x93, 0x7c, 0x08);
DEFINE_GUID(GUID_TreePot9, 
0xdb6be67d, 0x93b3, 0x470b, 0x8a, 0xd, 0xe, 0x66, 0xa1, 0x93, 0x7c, 0x09);
DEFINE_GUID(GUID_TreePot10, 
0xdb6be67d, 0x93b3, 0x470b, 0x8a, 0xd, 0xe, 0x66, 0xa1, 0x93, 0x7c, 0x10);

#define TREEAGES           3  // information about 3 ages of tree stored, 0 =young, 1=mature, 2=old
#define TREECAN            3  // number of canopy levels, 0 is lower, 1 mid, 2 = high
#define TREESEAS           4  // seasons, 0=spring, 1=summer, 2=fall, 3=winter

#define TS_CUSTOM          0  // custom
#define TS_CONIFERFIR      101
#define TS_CONIFERCEDAR    102
#define TS_CONIFERLEYLANDCYPRESS 103
#define TS_CONIFERITALIANCYPRESS 104
#define TS_CONIFERJUNIPER  105
#define TS_CONIFERSPRUCE   106
#define TS_CONIFERPINE     107
#define TS_CONIFERDOUGLASFIR  108
#define TS_CONIFERSEQUOIA  109

#define TS_EVERLEAFACACIA  201
#define TS_EVERLEAFEUCALYPTREDRIVER 202
#define TS_EVERLEAFEUCALYPTSALMONGUM 203
#define TS_EVERLEAFMELALEUCA 204
#define TS_EVERLEAFBOAB    205
#define TS_EVERLEAFBANYAN  206
#define TS_EVERLEAFCITRUS  207
#define TS_EVERLEAFEUCALYPTSMALL 208

#define TS_PALMCARPENTARIA 301
#define TS_PALMLIVISTONA   302
#define TS_PALMDATE        303
#define TS_PALMCYCAD       304
#define TS_PALMSAND        305

#define TS_DECIDUOUSMAPLESUGAR 401
#define TS_DECIDUOUSMAPLEJAPANESE 402
#define TS_DECIDUOUSMIMOSA 403
#define TS_DECIDUOUSBIRCH  404
#define TS_DECIDUOUSDOGWOOD 405
#define TS_DECIDUOUSCHERRY 406
#define TS_DECIDUOUSOAK    407
#define TS_DECIDUOUSBEECH  408
#define TS_DECIDUOUSMAGNOLIA 409
#define TS_DECIDUOUSWILLOW 410

#define TS_MISCFERN        501
#define TS_MISCBAMBOO      502
#define TS_MISCGRASS1      503
#define TS_MISCGRASS2      504

#define TS_SHRUBBLUEBERRY  601
#define TS_SHRUBBOXWOOD    602
#define TS_SHRUBFLOWERINGQUINCE 603
#define TS_SHRUBJUNIPERFALSE 604
#define TS_SHRUBHIBISCUS   605
#define TS_SHRUBHYDRANGEA  606
#define TS_SHRUBLILAC      607
#define TS_SHRUBROSE       608
#define TS_SHRUBRHODODENDRON 609
#define TS_SHRUBMUGHOPINE  610
#define TS_SHRUBOLEANDER   611
#define TS_SHRUBJUNIPER2   612
#define TS_SHRUBJUNIPER1   613

#define TS_ANNUALSUMBLUE   701
#define TS_ANNUALSPRINGRED 702

#define TS_POTCONIFER      801
#define TS_POTSHAPED1      802
#define TS_POTSHAPED2      803
#define TS_POTLEAFY        804
#define TS_POTCORNPLANT    805
#define TS_POTSUCCULANT    806
#define TS_POTPOINTSETIA   807

// CObjectTree is a test object to test the system
class CObjectTree : public CObjectTemplate {
public:
   CObjectTree (PVOID pParams, POSINFO pInfo);
   ~CObjectTree (void);

   virtual void Delete (void);
   virtual void Render (POBJECTRENDER pr, DWORD dwSubObject);
   virtual CObjectSocket *Clone (void);
   virtual PCMMLNode2 MMLTo (void);
   virtual BOOL MMLFrom (PCMMLNode2 pNode);
   virtual BOOL Message (DWORD dwMessage, PVOID pParam);
   virtual BOOL DialogQuery (void);
   virtual BOOL DialogShow (DWORD dwSurface, PCEscWindow pWindow, PCPoint pClick);
   virtual BOOL MoveReferencePointQuery (DWORD dwIndex, PCPoint pp);
   virtual BOOL MoveReferenceStringQuery (DWORD dwIndex, PWSTR psz, DWORD dwSize, DWORD *pdwNeeded);


   void CalcIfNecessary (void);
   void ParamFromStyle (void);

   // per tree
   DWORD    m_dwSRandExtra;         // add this to SRand, in addition to the object GUID
   DWORD    m_dwBirthday;           // DFDATE when tree was born
   DWORD    m_dwPotShape;           // 0 for no pot, 1 for round, 2 for square
   CPoint   m_pPotSize;             // p[0] = diameter on top, p[1] = diameter on bottom, p[2] = height
   DWORD    m_dwStyle;              // style of tree, TS_XXX

   // define the shape of the tree
   fp   m_fDefaultAge;          // default age that shows tree
   fp   m_afAges[TREEAGES];     // age of the tree at [0]=young, [1]=mature, [2]=old
   fp   m_afHeight[TREEAGES];   // height of the tree
   fp   m_afCanopyZ[TREECAN];   // bottom height of the canopy (0..1)xm_afHeight for the bottom levels
   fp   m_afCanopyW[TREECAN+1]; // width of the canopy (as a multiplier of the m_afHeight) for the
                                    // 0=bottom lower, 1=bottom mid, 2=bottom high, 3=top high
   DWORD    m_dwLGShape;            // shape of leaf group, LGS_XXX
   DWORD    m_adwLGNum[TREEAGES][TREECAN]; // average number of leaf groups appearing by age and cannopy level
   fp   m_fLGRadius;            // Typical XY distance (radius) of the leaf group from the center
                                    // of the tree. 0 => leaf group usually near center, 1.0=> usually on edge of canopy.
                                    // Values can be in between. If >=1, then will max out at 1.0. If <0 will min out at 0
   CPoint   m_pLGSize;              // typical leaf group size. p[0]=length (as radiates out), p[1] = width, [2]=height
   fp   m_afLGSize[TREEAGES][TREECAN+1];  // size multiplier by age and canopy height (0..3=very top)
   COLORREF m_acLGColor[TREESEAS][2];  // color of each leaf group by season, with two alternate colors
   fp   m_afLGDensBySeason[TREESEAS]; // leaf density by season, from 0 (no leaves) to 1 (all)
   fp   m_fLGAngleUpLight;      // typical up angle (in radians) for lightest leaf group of size m_pLGSize and m_afLGSize
   fp   m_fLGAngleUpHeavy;      // how much heaviest one angles up
   fp   m_fHeightVar;           // variability in height, 0 =none, 1 = +/-100%
   fp   m_fLGNumVar;            // variability in number of leaf groups, 0=none, 1=+/-100%
   fp   m_fLGSizeVar;           // variability in size, 0=none, 1=+/-100%
   fp   m_fLGOrientVar;         // variability in rotation of group, 0=none, 1=+/-PI

   fp   m_fTrunkSkewVar;        // variability in main trunk's skew - at an angle. 0=none, 1=can be 90 degrees on side
   fp   m_fTrunkThickness;      // thickness per leaf-node of volume
   fp   m_fTrunkTightness;      // 0 means trunk goes in straight line from branch to ground, 1 from bach horizontal to trunk and then down
   fp   m_fTrunkWobble;         // 0..1, 1 meaing that trunk wabbles around
   fp   m_fTrunkStickiness;     // 0 means lots of individual branches, 1 means all come together into central trunk
   COLORREF m_acTrunkColor[TREESEAS][2];  // trunk color by season, min and max

   fp   m_fFlowersPerLG;        // average number of flowers per leaf group
   fp   m_fFlowerSize;          // flower size, diameter in meters
   fp   m_fFlowerSizeVar;       // variability. 0 = none, 1=+/-100%
   fp   m_fFlowerDist;          // flower distance from leaf group start. 0=always at start, 1=can be far away
   fp   m_fFlowerPeak;          // peak flowering season, 0=mid-spring, 1=mid-summer, 2=mid-fall, 3=mid-winter
   fp   m_fFlowerDuration;      // duration of flower, in years
   DWORD    m_dwFlowerShape;        // shape of the flower
   COLORREF m_cFlower;              // color


   // automagic
   BOOL     m_fDirty;               // set to true if need to recalc stuff
   CListFixed m_lLEAFGROUP;         // list of LEAFGROUP structures
   CListFixed m_lTRUNKNOODLE;       // list of noodles used to draw the trunk
   CListFixed m_lFLOWER;            // list of FLOWER structures
};
typedef CObjectTree *PCObjectTree;

#endif // 0 - DEAD code

/**********************************************************************************
CObjectDoor */
// {8F7525C1-0B82-4799-AB76-8EBA45ED2358}
DEFINE_GUID(CLSID_Door, 
0x8f7525c1, 0xb89, 0x4799, 0xaf, 0x76, 0x8e, 0xba, 0x45, 0xed, 0x23, 0x58);

// entry doors
// {7F11AE71-C40A-4d7c-94F0-AEF60EBA2E11}
DEFINE_GUID(CLSID_DoorEntry1, 
0x7f11ae71, 0xc40a, 0x4d7c, 0x94, 0xf0, 0xae, 0xf6, 0xe, 0xba, 0x2e, 0x1);
DEFINE_GUID(CLSID_DoorEntry2, 
0x7f11ae71, 0xc40a, 0x4d7c, 0x94, 0xf0, 0xae, 0xf6, 0xe, 0xba, 0x2e, 0x2);
DEFINE_GUID(CLSID_DoorEntry3, 
0x7f11ae71, 0xc40a, 0x4d7c, 0x94, 0xf0, 0xae, 0xf6, 0xe, 0xba, 0x2e, 0x3);
DEFINE_GUID(CLSID_DoorEntry4, 
0x7f11ae71, 0xc40a, 0x4d7c, 0x94, 0xf0, 0xae, 0xf6, 0xe, 0xba, 0x2e, 0x4);
DEFINE_GUID(CLSID_DoorEntry5, 
0x7f11ae71, 0xc40a, 0x4d7c, 0x94, 0xf0, 0xae, 0xf6, 0xe, 0xba, 0x2e, 0x5);
DEFINE_GUID(CLSID_DoorEntry6, 
0x7f11ae71, 0xc40a, 0x4d7c, 0x94, 0xf0, 0xae, 0xf6, 0xe, 0xba, 0x2e, 0x6);
DEFINE_GUID(CLSID_DoorEntry7, 
0x7f11ae71, 0xc40a, 0x4d7c, 0x94, 0xf0, 0xae, 0xf6, 0xe, 0xba, 0x2e, 0x7);
DEFINE_GUID(CLSID_DoorEntry8, 
0x7f11ae71, 0xc40a, 0x4d7c, 0x94, 0xf0, 0xae, 0xf6, 0xe, 0xba, 0x2e, 0x8);
DEFINE_GUID(CLSID_DoorEntry9, 
0x7f11ae71, 0xc40a, 0x4d7c, 0x94, 0xf0, 0xae, 0xf6, 0xe, 0xba, 0x2e, 0x9);
DEFINE_GUID(CLSID_DoorEntry10, 
0x7f11ae71, 0xc40a, 0x4d7c, 0x94, 0xf0, 0xae, 0xf6, 0xe, 0xba, 0x2e, 0x10);
DEFINE_GUID(CLSID_DoorEntry11, 
0x7f11ae71, 0xc40a, 0x4d7c, 0x94, 0xf0, 0xae, 0xf6, 0xe, 0xba, 0x2e, 0x11);
DEFINE_GUID(CLSID_DoorEntry12, 
0x7f11ae71, 0xc40a, 0x4d7c, 0x94, 0xf0, 0xae, 0xf6, 0xe, 0xba, 0x2e, 0x12);
DEFINE_GUID(CLSID_DoorEntry13, 
0x7f11ae71, 0xc40a, 0x4d7c, 0x94, 0xf0, 0xae, 0xf6, 0xe, 0xba, 0x2e, 0x13);
DEFINE_GUID(CLSID_DoorEntry14, 
0x7f11ae71, 0xc40a, 0x4d7c, 0x94, 0xf0, 0xae, 0xf6, 0xe, 0xba, 0x2e, 0x14);
DEFINE_GUID(CLSID_DoorEntry15, 
0x7f11ae71, 0xc40a, 0x4d7c, 0x94, 0xf0, 0xae, 0xf6, 0xe, 0xba, 0x2e, 0x15);
DEFINE_GUID(CLSID_DoorEntry16, 
0x7f11ae71, 0xc40a, 0x4d7c, 0x94, 0xf0, 0xae, 0xf6, 0xe, 0xba, 0x2e, 0x16);
DEFINE_GUID(CLSID_DoorEntry17, 
0x7f11ae71, 0xc40a, 0x4d7c, 0x94, 0xf0, 0xae, 0xf6, 0xe, 0xba, 0x2e, 0x17);
DEFINE_GUID(CLSID_DoorEntry18, 
0x7f11ae71, 0xc40a, 0x4d7c, 0x94, 0xf0, 0xae, 0xf6, 0xe, 0xba, 0x2e, 0x18);
DEFINE_GUID(CLSID_DoorEntry19, 
0x7f11ae71, 0xc40a, 0x4d7c, 0x94, 0xf0, 0xae, 0xf6, 0xe, 0xba, 0x2e, 0x19);
DEFINE_GUID(CLSID_DoorEntry20, 
0x7f11ae71, 0xc40a, 0x4d7c, 0x94, 0xf0, 0xae, 0xf6, 0xe, 0xba, 0x2e, 0x20);
DEFINE_GUID(CLSID_DoorEntry21, 
0x7f11ae71, 0xc40a, 0x4d7c, 0x94, 0xf0, 0xae, 0xf6, 0xe, 0xba, 0x2e, 0x21);
DEFINE_GUID(CLSID_DoorEntry22, 
0x7f11ae71, 0xc40a, 0x4d7c, 0x94, 0xf0, 0xae, 0xf6, 0xe, 0xba, 0x2e, 0x22);
DEFINE_GUID(CLSID_DoorEntry23, 
0x7f11ae71, 0xc40a, 0x4d7c, 0x94, 0xf0, 0xae, 0xf6, 0xe, 0xba, 0x2e, 0x23);
DEFINE_GUID(CLSID_DoorEntry24, 
0x7f11ae71, 0xc40a, 0x4d7c, 0x94, 0xf0, 0xae, 0xf6, 0xe, 0xba, 0x2e, 0x24);
DEFINE_GUID(CLSID_DoorEntry25, 
0x7f11ae71, 0xc40a, 0x4d7c, 0x94, 0xf0, 0xae, 0xf6, 0xe, 0xba, 0x2e, 0x25);
DEFINE_GUID(CLSID_DoorEntry26, 
0x7f11ae71, 0xc40a, 0x4d7c, 0x94, 0xf0, 0xae, 0xf6, 0xe, 0xba, 0x2e, 0x26);
DEFINE_GUID(CLSID_DoorEntry27, 
0x7f11ae71, 0xc40a, 0x4d7c, 0x94, 0xf0, 0xae, 0xf6, 0xe, 0xba, 0x2e, 0x27);
DEFINE_GUID(CLSID_DoorEntry28, 
0x7f11ae71, 0xc40a, 0x4d7c, 0x94, 0xf0, 0xae, 0xf6, 0xe, 0xba, 0x2e, 0x28);
DEFINE_GUID(CLSID_DoorEntry29, 
0x7f11ae71, 0xc40a, 0x4d7c, 0x94, 0xf0, 0xae, 0xf6, 0xe, 0xba, 0x2e, 0x29);
DEFINE_GUID(CLSID_DoorEntry30, 
0x7f11ae71, 0xc40a, 0x4d7c, 0x94, 0xf0, 0xae, 0xf6, 0xe, 0xba, 0x2e, 0x30);
DEFINE_GUID(CLSID_DoorEntry31, 
0x7f11ae71, 0xc40a, 0x4d7c, 0x94, 0xf0, 0xae, 0xf6, 0xe, 0xba, 0x2e, 0x31);

// internal
// {C3139A41-9A9B-4285-B862-CA01E48C8C24}
DEFINE_GUID(CLSID_DoorInternal1, 
0xc3139a41, 0x9a9b, 0x4285, 0xb8, 0x62, 0xca, 0x1, 0xe4, 0x8c, 0x8c, 0x1);
DEFINE_GUID(CLSID_DoorInternal2, 
0xc3139a41, 0x9a9b, 0x4285, 0xb8, 0x62, 0xca, 0x1, 0xe4, 0x8c, 0x8c, 0x2);
DEFINE_GUID(CLSID_DoorInternal3, 
0xc3139a41, 0x9a9b, 0x4285, 0xb8, 0x62, 0xca, 0x1, 0xe4, 0x8c, 0x8c, 0x3);
DEFINE_GUID(CLSID_DoorInternal4, 
0xc3139a41, 0x9a9b, 0x4285, 0xb8, 0x62, 0xca, 0x1, 0xe4, 0x8c, 0x8c, 0x4);
DEFINE_GUID(CLSID_DoorInternal5, 
0xc3139a41, 0x9a9b, 0x4285, 0xb8, 0x62, 0xca, 0x1, 0xe4, 0x8c, 0x8c, 0x5);
DEFINE_GUID(CLSID_DoorInternal6, 
0xc3139a41, 0x9a9b, 0x4285, 0xb8, 0x62, 0xca, 0x1, 0xe4, 0x8c, 0x8c, 0x6);
DEFINE_GUID(CLSID_DoorInternal7, 
0xc3139a41, 0x9a9b, 0x4285, 0xb8, 0x62, 0xca, 0x1, 0xe4, 0x8c, 0x8c, 0x7);
DEFINE_GUID(CLSID_DoorInternal8, 
0xc3139a41, 0x9a9b, 0x4285, 0xb8, 0x62, 0xca, 0x1, 0xe4, 0x8c, 0x8c, 0x8);
DEFINE_GUID(CLSID_DoorInternal9, 
0xc3139a41, 0x9a9b, 0x4285, 0xb8, 0x62, 0xca, 0x1, 0xe4, 0x8c, 0x8c, 0x9);
DEFINE_GUID(CLSID_DoorInternal10, 
0xc3139a41, 0x9a9b, 0x4285, 0xb8, 0x62, 0xca, 0x1, 0xe4, 0x8c, 0x8c, 0x10);
DEFINE_GUID(CLSID_DoorInternal11, 
0xc3139a41, 0x9a9b, 0x4285, 0xb8, 0x62, 0xca, 0x1, 0xe4, 0x8c, 0x8c, 0x11);
DEFINE_GUID(CLSID_DoorInternal12, 
0xc3139a41, 0x9a9b, 0x4285, 0xb8, 0x62, 0xca, 0x1, 0xe4, 0x8c, 0x8c, 0x12);

// garage
// {0FF072C5-33B3-4cfa-BF1B-ABB5B80353C1}
DEFINE_GUID(CLSID_DoorGarage1, 
0xff072c5, 0x33b3, 0x4cfa, 0xbf, 0x1b, 0xab, 0xb5, 0xb8, 0x3, 0x53, 0x1);
DEFINE_GUID(CLSID_DoorGarage2, 
0xff072c5, 0x33b3, 0x4cfa, 0xbf, 0x1b, 0xab, 0xb5, 0xb8, 0x3, 0x53, 0x2);
DEFINE_GUID(CLSID_DoorGarage3, 
0xff072c5, 0x33b3, 0x4cfa, 0xbf, 0x1b, 0xab, 0xb5, 0xb8, 0x3, 0x53, 0x3);
DEFINE_GUID(CLSID_DoorGarage4, 
0xff072c5, 0x33b3, 0x4cfa, 0xbf, 0x1b, 0xab, 0xb5, 0xb8, 0x3, 0x53, 0x4);

// unuusal
// {5B99FAE5-7973-4b98-9DB6-8AF1EE119232}
DEFINE_GUID(CLSID_DoorUnusual1, 
0x5b99fae5, 0x7973, 0x4b98, 0x9d, 0xb6, 0x8a, 0xf1, 0xee, 0x11, 0x92, 0x1);
DEFINE_GUID(CLSID_DoorUnusual2, 
0x5b99fae5, 0x7973, 0x4b98, 0x9d, 0xb6, 0x8a, 0xf1, 0xee, 0x11, 0x92, 0x2);
DEFINE_GUID(CLSID_DoorUnusual3, 
0x5b99fae5, 0x7973, 0x4b98, 0x9d, 0xb6, 0x8a, 0xf1, 0xee, 0x11, 0x92, 0x3);
DEFINE_GUID(CLSID_DoorUnusual4, 
0x5b99fae5, 0x7973, 0x4b98, 0x9d, 0xb6, 0x8a, 0xf1, 0xee, 0x11, 0x92, 0x4);
DEFINE_GUID(CLSID_DoorUnusual5, 
0x5b99fae5, 0x7973, 0x4b98, 0x9d, 0xb6, 0x8a, 0xf1, 0xee, 0x11, 0x92, 0x5);
DEFINE_GUID(CLSID_DoorUnusual6, 
0x5b99fae5, 0x7973, 0x4b98, 0x9d, 0xb6, 0x8a, 0xf1, 0xee, 0x11, 0x92, 0x6);
DEFINE_GUID(CLSID_DoorUnusual7, 
0x5b99fae5, 0x7973, 0x4b98, 0x9d, 0xb6, 0x8a, 0xf1, 0xee, 0x11, 0x92, 0x7);
DEFINE_GUID(CLSID_DoorUnusual8, 
0x5b99fae5, 0x7973, 0x4b98, 0x9d, 0xb6, 0x8a, 0xf1, 0xee, 0x11, 0x92, 0x8);

// sliding
// {A7C88893-0351-4817-846B-74B09BB252CA}
DEFINE_GUID(CLSID_DoorSliding1, 
0xa7c88893, 0x351, 0x4817, 0x84, 0x6b, 0x74, 0xb0, 0x9b, 0xb2, 0x52, 0x1);
DEFINE_GUID(CLSID_DoorSliding2, 
0xa7c88893, 0x351, 0x4817, 0x84, 0x6b, 0x74, 0xb0, 0x9b, 0xb2, 0x52, 0x2);
DEFINE_GUID(CLSID_DoorSliding3, 
0xa7c88893, 0x351, 0x4817, 0x84, 0x6b, 0x74, 0xb0, 0x9b, 0xb2, 0x52, 0x3);
DEFINE_GUID(CLSID_DoorSliding4, 
0xa7c88893, 0x351, 0x4817, 0x84, 0x6b, 0x74, 0xb0, 0x9b, 0xb2, 0x52, 0x4);
DEFINE_GUID(CLSID_DoorSliding5, 
0xa7c88893, 0x351, 0x4817, 0x84, 0x6b, 0x74, 0xb0, 0x9b, 0xb2, 0x52, 0x5);
DEFINE_GUID(CLSID_DoorSliding6, 
0xa7c88893, 0x351, 0x4817, 0x84, 0x6b, 0x74, 0xb0, 0x9b, 0xb2, 0x52, 0x6);
DEFINE_GUID(CLSID_DoorSliding7, 
0xa7c88893, 0x351, 0x4817, 0x84, 0x6b, 0x74, 0xb0, 0x9b, 0xb2, 0x52, 0x7);

// {4338A138-3572-4a0b-8B28-492EC33663AF}
DEFINE_GUID(CLSID_DoorPocket1, 
0x4338a138, 0x3572, 0x4a0b, 0x8b, 0x28, 0x49, 0x2e, 0xc3, 0x36, 0x63, 0x1);
DEFINE_GUID(CLSID_DoorPocket2, 
0x4338a138, 0x3572, 0x4a0b, 0x8b, 0x28, 0x49, 0x2e, 0xc3, 0x36, 0x63, 0x2);
DEFINE_GUID(CLSID_DoorPocket3, 
0x4338a138, 0x3572, 0x4a0b, 0x8b, 0x28, 0x49, 0x2e, 0xc3, 0x36, 0x63, 0x3);
DEFINE_GUID(CLSID_DoorPocket4, 
0x4338a138, 0x3572, 0x4a0b, 0x8b, 0x28, 0x49, 0x2e, 0xc3, 0x36, 0x63, 0x4);
DEFINE_GUID(CLSID_DoorPocket5, 
0x4338a138, 0x3572, 0x4a0b, 0x8b, 0x28, 0x49, 0x2e, 0xc3, 0x36, 0x63, 0x5);
DEFINE_GUID(CLSID_DoorPocket6, 
0x4338a138, 0x3572, 0x4a0b, 0x8b, 0x28, 0x49, 0x2e, 0xc3, 0x36, 0x63, 0x6);
DEFINE_GUID(CLSID_DoorPocket7, 
0x4338a138, 0x3572, 0x4a0b, 0x8b, 0x28, 0x49, 0x2e, 0xc3, 0x36, 0x63, 0x7);
DEFINE_GUID(CLSID_DoorPocket8, 
0x4338a138, 0x3572, 0x4a0b, 0x8b, 0x28, 0x49, 0x2e, 0xc3, 0x36, 0x63, 0x8);
DEFINE_GUID(CLSID_DoorPocket9, 
0x4338a138, 0x3572, 0x4a0b, 0x8b, 0x28, 0x49, 0x2e, 0xc3, 0x36, 0x63, 0x9);
DEFINE_GUID(CLSID_DoorPocket10, 
0x4338a138, 0x3572, 0x4a0b, 0x8b, 0x28, 0x49, 0x2e, 0xc3, 0x36, 0x63, 0x10);
DEFINE_GUID(CLSID_DoorPocket11, 
0x4338a138, 0x3572, 0x4a0b, 0x8b, 0x28, 0x49, 0x2e, 0xc3, 0x36, 0x63, 0x11);
DEFINE_GUID(CLSID_DoorPocket12, 
0x4338a138, 0x3572, 0x4a0b, 0x8b, 0x28, 0x49, 0x2e, 0xc3, 0x36, 0x63, 0x12);
DEFINE_GUID(CLSID_DoorPocket13, 
0x4338a138, 0x3572, 0x4a0b, 0x8b, 0x28, 0x49, 0x2e, 0xc3, 0x36, 0x63, 0x13);
DEFINE_GUID(CLSID_DoorPocket14, 
0x4338a138, 0x3572, 0x4a0b, 0x8b, 0x28, 0x49, 0x2e, 0xc3, 0x36, 0x63, 0x14);

// bifold
// {ECE2E7D3-FFDC-476a-8EC0-AFD83EBF3CCE}
DEFINE_GUID(CLSID_DoorBifold1, 
0xece2e7d3, 0xffdc, 0x476a, 0x8e, 0xc0, 0xaf, 0xd8, 0x3e, 0xbf, 0x3c, 0x1);
DEFINE_GUID(CLSID_DoorBifold2, 
0xece2e7d3, 0xffdc, 0x476a, 0x8e, 0xc0, 0xaf, 0xd8, 0x3e, 0xbf, 0x3c, 0x2);
DEFINE_GUID(CLSID_DoorBifold3, 
0xece2e7d3, 0xffdc, 0x476a, 0x8e, 0xc0, 0xaf, 0xd8, 0x3e, 0xbf, 0x3c, 0x3);
DEFINE_GUID(CLSID_DoorBifold4, 
0xece2e7d3, 0xffdc, 0x476a, 0x8e, 0xc0, 0xaf, 0xd8, 0x3e, 0xbf, 0x3c, 0x4);
DEFINE_GUID(CLSID_DoorBifold5, 
0xece2e7d3, 0xffdc, 0x476a, 0x8e, 0xc0, 0xaf, 0xd8, 0x3e, 0xbf, 0x3c, 0x5);
DEFINE_GUID(CLSID_DoorBifold6, 
0xece2e7d3, 0xffdc, 0x476a, 0x8e, 0xc0, 0xaf, 0xd8, 0x3e, 0xbf, 0x3c, 0x6);
DEFINE_GUID(CLSID_DoorBifold7, 
0xece2e7d3, 0xffdc, 0x476a, 0x8e, 0xc0, 0xaf, 0xd8, 0x3e, 0xbf, 0x3c, 0x7);
DEFINE_GUID(CLSID_DoorBifold8, 
0xece2e7d3, 0xffdc, 0x476a, 0x8e, 0xc0, 0xaf, 0xd8, 0x3e, 0xbf, 0x3c, 0x8);
DEFINE_GUID(CLSID_DoorBifold9, 
0xece2e7d3, 0xffdc, 0x476a, 0x8e, 0xc0, 0xaf, 0xd8, 0x3e, 0xbf, 0x3c, 0x9);
DEFINE_GUID(CLSID_DoorBifold10, 
0xece2e7d3, 0xffdc, 0x476a, 0x8e, 0xc0, 0xaf, 0xd8, 0x3e, 0xbf, 0x3c, 0x10);
DEFINE_GUID(CLSID_DoorBifold11, 
0xece2e7d3, 0xffdc, 0x476a, 0x8e, 0xc0, 0xaf, 0xd8, 0x3e, 0xbf, 0x3c, 0x11);
DEFINE_GUID(CLSID_DoorBifold12, 
0xece2e7d3, 0xffdc, 0x476a, 0x8e, 0xc0, 0xaf, 0xd8, 0x3e, 0xbf, 0x3c, 0x12);
DEFINE_GUID(CLSID_DoorBifold13, 
0xece2e7d3, 0xffdc, 0x476a, 0x8e, 0xc0, 0xaf, 0xd8, 0x3e, 0xbf, 0x3c, 0x13);
DEFINE_GUID(CLSID_DoorBifold14, 
0xece2e7d3, 0xffdc, 0x476a, 0x8e, 0xc0, 0xaf, 0xd8, 0x3e, 0xbf, 0x3c, 0x14);
DEFINE_GUID(CLSID_DoorBifold15, 
0xece2e7d3, 0xffdc, 0x476a, 0x8e, 0xc0, 0xaf, 0xd8, 0x3e, 0xbf, 0x3c, 0x15);
DEFINE_GUID(CLSID_DoorBifold16, 
0xece2e7d3, 0xffdc, 0x476a, 0x8e, 0xc0, 0xaf, 0xd8, 0x3e, 0xbf, 0x3c, 0x16);
DEFINE_GUID(CLSID_DoorBifold17, 
0xece2e7d3, 0xffdc, 0x476a, 0x8e, 0xc0, 0xaf, 0xd8, 0x3e, 0xbf, 0x3c, 0x17);
DEFINE_GUID(CLSID_DoorBifold18, 
0xece2e7d3, 0xffdc, 0x476a, 0x8e, 0xc0, 0xaf, 0xd8, 0x3e, 0xbf, 0x3c, 0x18);
DEFINE_GUID(CLSID_DoorBifold19, 
0xece2e7d3, 0xffdc, 0x476a, 0x8e, 0xc0, 0xaf, 0xd8, 0x3e, 0xbf, 0x3c, 0x19);
DEFINE_GUID(CLSID_DoorBifold20, 
0xece2e7d3, 0xffdc, 0x476a, 0x8e, 0xc0, 0xaf, 0xd8, 0x3e, 0xbf, 0x3c, 0x20);
DEFINE_GUID(CLSID_DoorBifold21, 
0xece2e7d3, 0xffdc, 0x476a, 0x8e, 0xc0, 0xaf, 0xd8, 0x3e, 0xbf, 0x3c, 0x21);

// commerical
// {5794207D-3B0F-4ea9-AAC0-A51CD305485E}
DEFINE_GUID(CLSID_DoorCommercial1, 
0x5794207d, 0x3b0f, 0x4ea9, 0xaa, 0xc0, 0xa5, 0x1c, 0xd3, 0x5, 0x48, 0x1);
DEFINE_GUID(CLSID_DoorCommercial2, 
0x5794207d, 0x3b0f, 0x4ea9, 0xaa, 0xc0, 0xa5, 0x1c, 0xd3, 0x5, 0x48, 0x2);
DEFINE_GUID(CLSID_DoorCommercial3, 
0x5794207d, 0x3b0f, 0x4ea9, 0xaa, 0xc0, 0xa5, 0x1c, 0xd3, 0x5, 0x48, 0x3);
DEFINE_GUID(CLSID_DoorCommercial4, 
0x5794207d, 0x3b0f, 0x4ea9, 0xaa, 0xc0, 0xa5, 0x1c, 0xd3, 0x5, 0x48, 0x4);
DEFINE_GUID(CLSID_DoorCommercial5, 
0x5794207d, 0x3b0f, 0x4ea9, 0xaa, 0xc0, 0xa5, 0x1c, 0xd3, 0x5, 0x48, 0x5);
DEFINE_GUID(CLSID_DoorCommercial6, 
0x5794207d, 0x3b0f, 0x4ea9, 0xaa, 0xc0, 0xa5, 0x1c, 0xd3, 0x5, 0x48, 0x6);
DEFINE_GUID(CLSID_DoorCommercial7, 
0x5794207d, 0x3b0f, 0x4ea9, 0xaa, 0xc0, 0xa5, 0x1c, 0xd3, 0x5, 0x48, 0x7);
DEFINE_GUID(CLSID_DoorCommercial8, 
0x5794207d, 0x3b0f, 0x4ea9, 0xaa, 0xc0, 0xa5, 0x1c, 0xd3, 0x5, 0x48, 0x8);
DEFINE_GUID(CLSID_DoorCommercial9, 
0x5794207d, 0x3b0f, 0x4ea9, 0xaa, 0xc0, 0xa5, 0x1c, 0xd3, 0x5, 0x48, 0x9);

// hung windows
// {7CE1D9FD-7DDE-4ba8-8859-0C075536A9BB}
DEFINE_GUID(CLSID_WindowHung1, 
0x7ce1d9fd, 0x7dde, 0x4ba8, 0x88, 0x59, 0xc, 0x7, 0x55, 0x36, 0xa9, 0x1);
DEFINE_GUID(CLSID_WindowHung2, 
0x7ce1d9fd, 0x7dde, 0x4ba8, 0x88, 0x59, 0xc, 0x7, 0x55, 0x36, 0xa9, 0x2);
DEFINE_GUID(CLSID_WindowHung3, 
0x7ce1d9fd, 0x7dde, 0x4ba8, 0x88, 0x59, 0xc, 0x7, 0x55, 0x36, 0xa9, 0x3);
DEFINE_GUID(CLSID_WindowHung4, 
0x7ce1d9fd, 0x7dde, 0x4ba8, 0x88, 0x59, 0xc, 0x7, 0x55, 0x36, 0xa9, 0x4);
DEFINE_GUID(CLSID_WindowHung5, 
0x7ce1d9fd, 0x7dde, 0x4ba8, 0x88, 0x59, 0xc, 0x7, 0x55, 0x36, 0xa9, 0x5);
DEFINE_GUID(CLSID_WindowHung6, 
0x7ce1d9fd, 0x7dde, 0x4ba8, 0x88, 0x59, 0xc, 0x7, 0x55, 0x36, 0xa9, 0x6);

// sliding windows
// {8305F277-19E8-48ad-AC5A-4B1F51AB878A}
DEFINE_GUID(CLSID_WindowSliding1, 
0x8305f277, 0x19e8, 0x48ad, 0xac, 0x5a, 0x4b, 0x1f, 0x51, 0xab, 0x87, 0x1);
DEFINE_GUID(CLSID_WindowSliding2, 
0x8305f277, 0x19e8, 0x48ad, 0xac, 0x5a, 0x4b, 0x1f, 0x51, 0xab, 0x87, 0x2);
DEFINE_GUID(CLSID_WindowSliding3, 
0x8305f277, 0x19e8, 0x48ad, 0xac, 0x5a, 0x4b, 0x1f, 0x51, 0xab, 0x87, 0x3);
DEFINE_GUID(CLSID_WindowSliding4, 
0x8305f277, 0x19e8, 0x48ad, 0xac, 0x5a, 0x4b, 0x1f, 0x51, 0xab, 0x87, 0x4);

// casement windows
// {3971260F-1BC2-4f31-9845-721E846E9F6E}
DEFINE_GUID(CLSID_WindowCasement1, 
0x3971260f, 0x1bc2, 0x4f31, 0x98, 0x45, 0x72, 0x1e, 0x84, 0x6e, 0x9f, 0x1);
DEFINE_GUID(CLSID_WindowCasement2, 
0x3971260f, 0x1bc2, 0x4f31, 0x98, 0x45, 0x72, 0x1e, 0x84, 0x6e, 0x9f, 0x2);
DEFINE_GUID(CLSID_WindowCasement3, 
0x3971260f, 0x1bc2, 0x4f31, 0x98, 0x45, 0x72, 0x1e, 0x84, 0x6e, 0x9f, 0x3);
DEFINE_GUID(CLSID_WindowCasement4, 
0x3971260f, 0x1bc2, 0x4f31, 0x98, 0x45, 0x72, 0x1e, 0x84, 0x6e, 0x9f, 0x4);
DEFINE_GUID(CLSID_WindowCasement5, 
0x3971260f, 0x1bc2, 0x4f31, 0x98, 0x45, 0x72, 0x1e, 0x84, 0x6e, 0x9f, 0x5);
DEFINE_GUID(CLSID_WindowCasement6, 
0x3971260f, 0x1bc2, 0x4f31, 0x98, 0x45, 0x72, 0x1e, 0x84, 0x6e, 0x9f, 0x6);
DEFINE_GUID(CLSID_WindowCasement7, 
0x3971260f, 0x1bc2, 0x4f31, 0x98, 0x45, 0x72, 0x1e, 0x84, 0x6e, 0x9f, 0x7);
DEFINE_GUID(CLSID_WindowCasement8, 
0x3971260f, 0x1bc2, 0x4f31, 0x98, 0x45, 0x72, 0x1e, 0x84, 0x6e, 0x9f, 0x8);
DEFINE_GUID(CLSID_WindowCasement9, 
0x3971260f, 0x1bc2, 0x4f31, 0x98, 0x45, 0x72, 0x1e, 0x84, 0x6e, 0x9f, 0x9);
DEFINE_GUID(CLSID_WindowCasement10, 
0x3971260f, 0x1bc2, 0x4f31, 0x98, 0x45, 0x72, 0x1e, 0x84, 0x6e, 0x9f, 0x10);
DEFINE_GUID(CLSID_WindowCasement11, 
0x3971260f, 0x1bc2, 0x4f31, 0x98, 0x45, 0x72, 0x1e, 0x84, 0x6e, 0x9f, 0x11);
DEFINE_GUID(CLSID_WindowCasement12, 
0x3971260f, 0x1bc2, 0x4f31, 0x98, 0x45, 0x72, 0x1e, 0x84, 0x6e, 0x9f, 0x12);

// unusual
// {35AEE70D-3DF5-4328-B10D-3A4050113845}
DEFINE_GUID(CLSID_WindowUnusual1, 
0x35aee70d, 0x3df5, 0x4328, 0xb1, 0xd, 0x3a, 0x40, 0x50, 0x11, 0x38, 0x1);
DEFINE_GUID(CLSID_WindowUnusual2, 
0x35aee70d, 0x3df5, 0x4328, 0xb1, 0xd, 0x3a, 0x40, 0x50, 0x11, 0x38, 0x2);
DEFINE_GUID(CLSID_WindowUnusual3, 
0x35aee70d, 0x3df5, 0x4328, 0xb1, 0xd, 0x3a, 0x40, 0x50, 0x11, 0x38, 0x3);
DEFINE_GUID(CLSID_WindowUnusual4, 
0x35aee70d, 0x3df5, 0x4328, 0xb1, 0xd, 0x3a, 0x40, 0x50, 0x11, 0x38, 0x4);
DEFINE_GUID(CLSID_WindowUnusual5, 
0x35aee70d, 0x3df5, 0x4328, 0xb1, 0xd, 0x3a, 0x40, 0x50, 0x11, 0x38, 0x5);
DEFINE_GUID(CLSID_WindowUnusual6, 
0x35aee70d, 0x3df5, 0x4328, 0xb1, 0xd, 0x3a, 0x40, 0x50, 0x11, 0x38, 0x6);
DEFINE_GUID(CLSID_WindowUnusual7, 
0x35aee70d, 0x3df5, 0x4328, 0xb1, 0xd, 0x3a, 0x40, 0x50, 0x11, 0x38, 0x7);
DEFINE_GUID(CLSID_WindowUnusual8, 
0x35aee70d, 0x3df5, 0x4328, 0xb1, 0xd, 0x3a, 0x40, 0x50, 0x11, 0x38, 0x8);

// awning windows
// {D0FBF70B-EC89-474b-B828-F5C20517BB06}
DEFINE_GUID(CLSID_WindowAwning1, 
0xd0fbf70b, 0xec89, 0x474b, 0xb8, 0x28, 0xf5, 0xc2, 0x5, 0x17, 0xbb, 0x1);
DEFINE_GUID(CLSID_WindowAwning2, 
0xd0fbf70b, 0xec89, 0x474b, 0xb8, 0x28, 0xf5, 0xc2, 0x5, 0x17, 0xbb, 0x2);
DEFINE_GUID(CLSID_WindowAwning3, 
0xd0fbf70b, 0xec89, 0x474b, 0xb8, 0x28, 0xf5, 0xc2, 0x5, 0x17, 0xbb, 0x3);
DEFINE_GUID(CLSID_WindowAwning4, 
0xd0fbf70b, 0xec89, 0x474b, 0xb8, 0x28, 0xf5, 0xc2, 0x5, 0x17, 0xbb, 0x4);
DEFINE_GUID(CLSID_WindowAwning5, 
0xd0fbf70b, 0xec89, 0x474b, 0xb8, 0x28, 0xf5, 0xc2, 0x5, 0x17, 0xbb, 0x5);
DEFINE_GUID(CLSID_WindowAwning6, 
0xd0fbf70b, 0xec89, 0x474b, 0xb8, 0x28, 0xf5, 0xc2, 0x5, 0x17, 0xbb, 0x6);

// louver windows
// {8632ADE5-45B3-4c9c-8C98-2EF18023306A}
DEFINE_GUID(CLSID_WindowLouver1, 
0x8632ade5, 0x45b3, 0x4c9c, 0x8c, 0x98, 0x2e, 0xf1, 0x80, 0x23, 0x30, 0x1);
DEFINE_GUID(CLSID_WindowLouver2, 
0x8632ade5, 0x45b3, 0x4c9c, 0x8c, 0x98, 0x2e, 0xf1, 0x80, 0x23, 0x30, 0x2);
DEFINE_GUID(CLSID_WindowLouver3, 
0x8632ade5, 0x45b3, 0x4c9c, 0x8c, 0x98, 0x2e, 0xf1, 0x80, 0x23, 0x30, 0x3);

// fixed windows
// {B0578D62-CBC5-49ad-9447-7AC5B1CF055C}
DEFINE_GUID(CLSID_WindowFixed1, 
0xb0578d62, 0xcbc5, 0x49ad, 0x94, 0x47, 0x7a, 0xc5, 0xb1, 0xcf, 0x5, 0x1);
DEFINE_GUID(CLSID_WindowFixed2, 
0xb0578d62, 0xcbc5, 0x49ad, 0x94, 0x47, 0x7a, 0xc5, 0xb1, 0xcf, 0x5, 0x2);
DEFINE_GUID(CLSID_WindowFixed3, 
0xb0578d62, 0xcbc5, 0x49ad, 0x94, 0x47, 0x7a, 0xc5, 0xb1, 0xcf, 0x5, 0x3);
DEFINE_GUID(CLSID_WindowFixed4, 
0xb0578d62, 0xcbc5, 0x49ad, 0x94, 0x47, 0x7a, 0xc5, 0xb1, 0xcf, 0x5, 0x4);
DEFINE_GUID(CLSID_WindowFixed5, 
0xb0578d62, 0xcbc5, 0x49ad, 0x94, 0x47, 0x7a, 0xc5, 0xb1, 0xcf, 0x5, 0x5);
DEFINE_GUID(CLSID_WindowFixed6, 
0xb0578d62, 0xcbc5, 0x49ad, 0x94, 0x47, 0x7a, 0xc5, 0xb1, 0xcf, 0x5, 0x6);
DEFINE_GUID(CLSID_WindowFixed7, 
0xb0578d62, 0xcbc5, 0x49ad, 0x94, 0x47, 0x7a, 0xc5, 0xb1, 0xcf, 0x5, 0x7);
DEFINE_GUID(CLSID_WindowFixed8, 
0xb0578d62, 0xcbc5, 0x49ad, 0x94, 0x47, 0x7a, 0xc5, 0xb1, 0xcf, 0x5, 0x8);
DEFINE_GUID(CLSID_WindowFixed9, 
0xb0578d62, 0xcbc5, 0x49ad, 0x94, 0x47, 0x7a, 0xc5, 0xb1, 0xcf, 0x5, 0x9);
DEFINE_GUID(CLSID_WindowFixed10, 
0xb0578d62, 0xcbc5, 0x49ad, 0x94, 0x47, 0x7a, 0xc5, 0xb1, 0xcf, 0x5, 0x10);
DEFINE_GUID(CLSID_WindowFixed11, 
0xb0578d62, 0xcbc5, 0x49ad, 0x94, 0x47, 0x7a, 0xc5, 0xb1, 0xcf, 0x5, 0x11);
DEFINE_GUID(CLSID_WindowFixed12, 
0xb0578d62, 0xcbc5, 0x49ad, 0x94, 0x47, 0x7a, 0xc5, 0xb1, 0xcf, 0x5, 0x12);
DEFINE_GUID(CLSID_WindowFixed13, 
0xb0578d62, 0xcbc5, 0x49ad, 0x94, 0x47, 0x7a, 0xc5, 0xb1, 0xcf, 0x5, 0x13);
DEFINE_GUID(CLSID_WindowFixed14, 
0xb0578d62, 0xcbc5, 0x49ad, 0x94, 0x47, 0x7a, 0xc5, 0xb1, 0xcf, 0x5, 0x14);
DEFINE_GUID(CLSID_WindowFixed15, 
0xb0578d62, 0xcbc5, 0x49ad, 0x94, 0x47, 0x7a, 0xc5, 0xb1, 0xcf, 0x5, 0x15);

// dooways
// {3A788AA7-5A79-4a21-B9D2-86C25AD79936}
DEFINE_GUID(CLSID_Doorway1, 
0x3a788aa7, 0x5a79, 0x4a21, 0xb9, 0xd2, 0x86, 0xc2, 0x5a, 0xd7, 0x99, 0x1);
DEFINE_GUID(CLSID_Doorway2, 
0x3a788aa7, 0x5a79, 0x4a21, 0xb9, 0xd2, 0x86, 0xc2, 0x5a, 0xd7, 0x99, 0x2);
DEFINE_GUID(CLSID_Doorway3, 
0x3a788aa7, 0x5a79, 0x4a21, 0xb9, 0xd2, 0x86, 0xc2, 0x5a, 0xd7, 0x99, 0x3);
DEFINE_GUID(CLSID_Doorway4, 
0x3a788aa7, 0x5a79, 0x4a21, 0xb9, 0xd2, 0x86, 0xc2, 0x5a, 0xd7, 0x99, 0x4);
DEFINE_GUID(CLSID_Doorway5, 
0x3a788aa7, 0x5a79, 0x4a21, 0xb9, 0xd2, 0x86, 0xc2, 0x5a, 0xd7, 0x99, 0x5);
DEFINE_GUID(CLSID_Doorway6, 
0x3a788aa7, 0x5a79, 0x4a21, 0xb9, 0xd2, 0x86, 0xc2, 0x5a, 0xd7, 0x99, 0x6);
DEFINE_GUID(CLSID_Doorway7, 
0x3a788aa7, 0x5a79, 0x4a21, 0xb9, 0xd2, 0x86, 0xc2, 0x5a, 0xd7, 0x99, 0x7);
DEFINE_GUID(CLSID_Doorway8, 
0x3a788aa7, 0x5a79, 0x4a21, 0xb9, 0xd2, 0x86, 0xc2, 0x5a, 0xd7, 0x99, 0x8);
DEFINE_GUID(CLSID_Doorway9, 
0x3a788aa7, 0x5a79, 0x4a21, 0xb9, 0xd2, 0x86, 0xc2, 0x5a, 0xd7, 0x99, 0x9);
DEFINE_GUID(CLSID_Doorway10, 
0x3a788aa7, 0x5a79, 0x4a21, 0xb9, 0xd2, 0x86, 0xc2, 0x5a, 0xd7, 0x99, 0x10);
DEFINE_GUID(CLSID_Doorway11, 
0x3a788aa7, 0x5a79, 0x4a21, 0xb9, 0xd2, 0x86, 0xc2, 0x5a, 0xd7, 0x99, 0x11);
DEFINE_GUID(CLSID_Doorway12, 
0x3a788aa7, 0x5a79, 0x4a21, 0xb9, 0xd2, 0x86, 0xc2, 0x5a, 0xd7, 0x99, 0x12);
DEFINE_GUID(CLSID_Doorway13, 
0x3a788aa7, 0x5a79, 0x4a21, 0xb9, 0xd2, 0x86, 0xc2, 0x5a, 0xd7, 0x99, 0x13);
DEFINE_GUID(CLSID_Doorway14, 
0x3a788aa7, 0x5a79, 0x4a21, 0xb9, 0xd2, 0x86, 0xc2, 0x5a, 0xd7, 0x99, 0x14);


class CDoorFrame;
class CDoorOpening;
typedef CDoorOpening *PCDoorOpening;


class DLLEXPORT CObjectDoor : public CObjectTemplate {
public:
   CObjectDoor (PVOID pParams, POSINFO pInfo);
   ~CObjectDoor (void);

   virtual void Delete (void);
   virtual void Render (POBJECTRENDER pr, DWORD dwSubObject);
   virtual CObjectSocket *Clone (void);
   virtual PCMMLNode2 MMLTo (void);
   virtual BOOL MMLFrom (PCMMLNode2 pNode);
   virtual BOOL MoveReferencePointQuery (DWORD dwIndex, PCPoint pp);
   virtual BOOL MoveReferenceStringQuery (DWORD dwIndex, PWSTR psz, DWORD dwSize, DWORD *pdwNeeded);
   virtual BOOL EmbedDoCutout (void);
   virtual BOOL DialogQuery (void);
   virtual BOOL DialogShow (DWORD dwSurface, PCEscWindow pWindow, PCPoint pClick);
   virtual fp OpenGet (void);
   virtual BOOL OpenSet (fp fOpen);
   
   void CalcOpeningsIfNecessary (void);

   CDoorFrame  *m_pDoorFrame;
   BOOL        m_fAutoDepth;     // set to TRUE if depth of framing is automatic
   fp      m_fAutoAdjust;    // add (or subtract) from the depth of surface
   DWORD       m_dwBevelSide;     // 0 for outside, 1 for middle, 2 for inside
   CPoint      m_pBevel;         // 0 for LR, 1 for top, 2 for bottom. 0=perp, + = angled out

   CListFixed  m_lOpenings;      // list of PCDoorOpening
   BOOL        m_fDirty;         // if TRUE, m_lOpenings is dirty
   fp      m_fOpened;        // amount its opened. 0=closed, 1.0 = all way


   CRenderSurface    m_Renderrs;       // for malloc optimization
};
typedef CObjectDoor *PCObjectDoor;

/**********************************************************************************
CObjectCamera */
// {8F7525C1-0B82-4799-AB76-8EBA45ED2358}
DEFINE_GUID(CLSID_Camera, 
0x8f7525c1, 0xb86, 0x4729, 0xaf, 0x76, 0x8e, 0xba, 0x45, 0xed, 0x23, 0x58);

class CHouseView;

// CObjectCamera is a test object to test the system
class DLLEXPORT CObjectCamera : public CObjectTemplate {
public:
   CObjectCamera (PVOID pParams, POSINFO pInfo);
   ~CObjectCamera (void);

   virtual void Delete (void);
   virtual void Render (POBJECTRENDER pr, DWORD dwSubObject);
   virtual CObjectSocket *Clone (void);
   virtual PCMMLNode2 MMLTo (void);
   virtual BOOL MMLFrom (PCMMLNode2 pNode);
   virtual BOOL MoveReferencePointQuery (DWORD dwIndex, PCPoint pp);
   virtual BOOL MoveReferenceStringQuery (DWORD dwIndex, PWSTR psz, DWORD dwSize, DWORD *pdwNeeded);
   virtual BOOL ObjectMatrixSet (CMatrix *pObject);
   virtual BOOL Message (DWORD dwMessage, PVOID pParam);
   virtual void QueryBoundingBox (PCPoint pCorner1, PCPoint pCorner2, DWORD dwSubObject);

   CHouseView     *m_pView;       // view that should notify
   BOOL           m_fVisible;    // if TRUE then camerea is visble, false if not


   CRenderSurface    m_Renderrs;    // to minimize mallocs
};
typedef CObjectCamera *PCObjectCamera;



/**********************************************************************************
CObjectAnimCamera */
DEFINE_GUID(CLSID_AnimCamera, 
0x8f7885c1, 0xf286, 0x46c9, 0xaf, 0x1a, 0x8e, 0x0d, 0x45, 0xed, 0x91, 0x58);
class CRevolution;
typedef CRevolution * PCRevolution;


class DLLEXPORT CObjectAnimCamera : public CObjectTemplate {
public:
   CObjectAnimCamera (PVOID pParams, POSINFO pInfo);
   ~CObjectAnimCamera (void);

   virtual void Delete (void);
   virtual void Render (POBJECTRENDER pr, DWORD dwSubObject);
   virtual CObjectSocket *Clone (void);
   virtual PCMMLNode2 MMLTo (void);
   virtual BOOL MMLFrom (PCMMLNode2 pNode);
   virtual BOOL MoveReferencePointQuery (DWORD dwIndex, PCPoint pp);
   virtual BOOL MoveReferenceStringQuery (DWORD dwIndex, PWSTR psz, DWORD dwSize, DWORD *pdwNeeded);
   virtual BOOL Message (DWORD dwMessage, PVOID pParam);
   virtual BOOL DialogQuery (void);
   virtual BOOL DialogShow (DWORD dwSurface, PCEscWindow pWindow, PCPoint pClick);
   virtual BOOL AttribGetIntern (PWSTR pszName, fp *pfValue);
   virtual void AttribGetAllIntern (PCListFixed plATTRIBVAL);
   virtual void AttribSetGroupIntern (DWORD dwNum, PATTRIBVAL paAttrib);
   virtual BOOL AttribInfoIntern (PWSTR pszName, PATTRIBINFO pInfo);
   void QueryBoundingBox (PCPoint pCorner1, PCPoint pCorner2, DWORD dwSubObject);

   // camera settings
   fp                   m_fFOV;        // field of view, in radians
   fp                   m_fExposure;   // in natural log. 1 = best exposure in full sunlight. 2 = higheer, etc.


private:
   PCRevolution         m_apRev[3];    // shapes for drawing

   CRenderSurface    m_Renderrs;    // to minimize mallocs

};
typedef CObjectAnimCamera *PCObjectAnimCamera;

/**********************************************************************************
CObjectUniHole */
// {8F7525C1-0B82-4799-AB76-8EBA45ED2358}
DEFINE_GUID(CLSID_UniHole, 
0x8f7525c1, 0xb89, 0x4799, 0xbf, 0x76, 0x8e, 0xb2, 0x45, 0xed, 0x23, 0xa0);
DEFINE_GUID(CLSID_UniHoleRect, 
0x8f7525c1, 0xb89, 0x4799, 0xaf, 0x76, 0x8e, 0xb2, 0x45, 0xed, 0x23, 0xa0);
DEFINE_GUID(CLSID_UniHoleCirc, 
0x8f7525c1, 0xb89, 0x4799, 0xaf, 0x76, 0x8e, 0xb2, 0x45, 0xed, 0x24, 0xa0);
DEFINE_GUID(CLSID_UniHoleAny, 
0x8f7525c1, 0xb89, 0x4799, 0xaf, 0x76, 0x8e, 0xb2, 0x45, 0xed, 0x25, 0xa0);

// CObjectUniHole is a test object to test the system
class DLLEXPORT CObjectUniHole : public CObjectTemplate {
public:
   CObjectUniHole (PVOID pParams, POSINFO pInfo);
   ~CObjectUniHole (void);

   virtual void Delete (void);
   virtual void Render (POBJECTRENDER pr, DWORD dwSubObject);
   virtual CObjectSocket *Clone (void);
   virtual PCMMLNode2 MMLTo (void);
   virtual BOOL MMLFrom (PCMMLNode2 pNode);
   virtual BOOL ControlPointQuery (DWORD dwID, POSCONTROL pInfo);
   virtual BOOL ControlPointSet (DWORD dwID, PCPoint pVal, PCPoint pViewer);
   virtual void ControlPointEnum (PCListFixed plDWORD);
   virtual BOOL MoveReferencePointQuery (DWORD dwIndex, PCPoint pp);
   virtual BOOL MoveReferenceStringQuery (DWORD dwIndex, PWSTR psz, DWORD dwSize, DWORD *pdwNeeded);
   virtual BOOL EmbedDoCutout (void);
   virtual BOOL DialogQuery (void);
   virtual BOOL DialogShow (DWORD dwSurface, PCEscWindow pWindow, PCPoint pClick);

   CSpline     m_Spline;   // defines the shape


   CRenderSurface    m_Renderrs;    // to minimize mallocs
};
typedef CObjectUniHole *PCObjectUniHole;

/**********************************************************************************
CBalustrade */

// balustrade stypes
#define BS_CUSTOM                0  // custom balistrade
#define BS_BALVERTWOOD           1  // standard 2x4's and 2x2s
#define BS_BALVERTWOOD2          2  // like balvertwood, except posts go above
#define BS_BALVERTWOOD3          3  // like balvertwood, excpets verticals go all the way to the ground
#define BS_BALVERTLOG            4  // log-cabin style
#define BS_BALVERTWROUGHTIRON    5  // wrought iron style
#define BS_BALVERTSTEEL          6  // steel structure and verticals

#define BS_BALHORZPANELS         50 // horizontal with panels
#define BS_BALHORZWOOD           51 // hozizontal with small bits of wood
#define BS_BALHORZWIRE           52 // horizontal with wire
#define BS_BALHORZWIRERAIL       53 // horizontal with wire, with handrail
#define BS_BALHORZPOLE           54 // horzintal 2" diameter poles

#define BS_BALPANEL              100 // suspended panel
#define BS_BALPANELSOLID         101 // solid balustrade

#define BS_BALOPEN               150 // open, one rail on top
#define BS_BALOPENMIDDLE         151 // open with middle rail
#define BS_BALOPENPOLE           152 // open, made from pole

#define BS_BALBRACEX             200 // X brace

#define BS_BALFANCYGREEK         250 // greek columns
#define BS_BALFANCYGREEK2        251 // greek columns, # 2
#define BS_BALFANCYWOOD          252 // fancy version of 2x4 and 2x2 posts

#define BS_FENCEVERTPICKET       500   // standard white picket fence
#define BS_FENCEVERTSTEEL        501   // steel fence
#define BS_FENCEVERTWROUGHTIRON  502   // wrought iron fence
#define BS_FENCEVERTPICKETSMALL  503   // small pickets
#define BS_FENCEVERTPANEL        504   // panels

#define BS_FENCEHORZLOG          550   // horizontal, log
#define BS_FENCEHORZSTICK        551   // lots of sticks
#define BS_FENCEHORZPANELS       552   // horizontal panels
#define BS_FENCEHORZWIRE         553   // wire fence

#define BS_FENCEPANELSOLID       600   // rock wall
#define BS_FENCEPANELPOLE        601   // chain mesh

#define BS_FENCEBRACEX           700   // X brace


// balustrade color indecies - into m_adwBalColor. Used internally
#define BALCOLOR_POSTMAIN        0
#define BALCOLOR_POSTBASE        1
#define BALCOLOR_POSTTOP         2
#define BALCOLOR_POSTBRACE       3
#define BALCOLOR_HANDRAIL        4
#define BALCOLOR_TOPBOTTOMRAIL   5
#define BALCOLOR_AUTOHORZRAIL    6
#define BALCOLOR_AUTOVERTRAIL    7
#define BALCOLOR_PANEL           8
#define BALCOLOR_BRACE           9
#define BALCOLOR_MAX             10 // used for number of balustrade colors.

class CObjectBuildBlock;
class CNoodle;
class DLLEXPORT CBalustrade {
public:
   ESCNEWDELETE;

   CBalustrade (void);
   ~CBalustrade (void);

   BOOL InitButDontCreate (DWORD dwRenderShard, DWORD dwType, PCObjectTemplate pTemplate);
   BOOL Init (DWORD dwRenderShard, DWORD dwType, PCObjectTemplate pTemplate, PCSpline pBottom, PCSpline pTop, BOOL fIndent);
   BOOL NewSplines (PCSpline pBottom, PCSpline pTop);
   PCMMLNode2 MMLTo (void);
   BOOL MMLFrom (PCMMLNode2 pNode);
   CBalustrade *Clone (void);
   void Render (DWORD dwRenderShard, POBJECTRENDER pr, CRenderSurface *prs);
   void QueryBoundingBox (PCPoint pCorner1, PCPoint pCorner2);
   BOOL CutoutBasedOnSurface (PCSplineSurface pss, PCMatrix m);
   void ExtendPostsToRoof (DWORD dwNum, PCSplineSurface *papss, PCMatrix pam);
   void ClaimClear (void);
   void ClaimCloneTo (CBalustrade *pCloneTo, PCObjectTemplate pTemplate);
   BOOL ClaimRemove (DWORD dwID);
   BOOL CloneTo (CBalustrade *pNew, PCObjectTemplate pTemplate);
   BOOL ClaimFindByID (DWORD dwID);
   PWSTR AppearancePage (PCEscWindow pWindow, CObjectBuildBlock *pBB);
   PWSTR OpeningsPage (PCEscWindow pWindow, CObjectBuildBlock *pBB);
   PWSTR DisplayPage (PCEscWindow pWindow);
   PWSTR CornersPage (PCEscWindow pWindow);
   void ControlPointEnum (PCListFixed plDWORD);
   BOOL ControlPointSet (DWORD dwID, PCPoint pVal, PCPoint pViewer,CObjectBuildBlock *pBB);
   BOOL ControlPointQuery (DWORD dwID, POSCONTROL pInfo);

   // control points displayed
   DWORD             m_dwDisplayControl;  // 0 for end points, 1 for openings
   BOOL              m_fForceVert;     // if true for top to be over bottom
   BOOL              m_fForceLevel;    // if true, force bottom to be at 0
   BOOL              m_fHideFirst;     // if TRUE, hide first post
   BOOL              m_fHideLast;      // if TRUE, hide last post
   BOOL              m_fNoColumns;     // if TRUE then all columns converted to posts
   BOOL              m_fFence;         // set to TRUE if it's a fence
   BOOL              m_fSwapSides;     // swap handrails to other side

//private:
   BOOL DeterminePostLocations (void);
   BOOL BuildNoodleLocations (void);
   void PostToCoord (DWORD dwStart, DWORD dwEnd, PCListFixed plBottom, PCListFixed plTop);
   fp LengthFromCoord (PCListFixed pl, DWORD dwStart, DWORD dwEnd);
   CNoodle* NoodleFromCoord (PCListFixed plBottom, PCListFixed plTop,
                                       BOOL fBaseFromTop, fp fOffset,
                                       DWORD dwStart, DWORD dwEnd, BOOL fLooped);
   void DistanceFromPost (PCListFixed pl, DWORD dwPost, fp fRight, PCPoint pVal);
   void ParamFromStyle (DWORD dwStyle);
   DWORD ClaimSurface (DWORD dwColor, COLORREF cColor, DWORD dwMaterialID, PWSTR pszScheme,
      const GUID *pgTextureCode = NULL, const GUID *pgTextureSub = NULL,
      PTEXTUREMODS pTextureMods = NULL, fp *pafTextureMatrix = NULL);
   void ClaimAllNecessaryColors (void);
   BOOL GenerateIntersect (void);

   PCSplineSurface   m_pssControlInter;   // temporary spline surface used for control intersections

   PCObjectTemplate  m_pTemplate;    // object template
   DWORD             m_adwBalColor[BALCOLOR_MAX];  // 0 for no color, else into into m_pTemplate color

   PCSpline          m_psBottom;        // spline at the bottom
   PCSpline          m_psTop;           // spline at the top
   PCSpline          m_psOrigBottom;    // original bottom - used for skirting
   PCSpline          m_psOrigTop;       // original top - used for skirting
   fp            m_fHeight;          // height to draw, starting at bottom
   BOOL              m_fIndent;           // set to true if indenting from loop

   CListFixed        m_lBALCUTOUT;    // list of BALCUTOUT info. where cutouts start and stop

   CListFixed        m_lPosts;         // list of BPOSTINFO structures
   CListFixed        m_lNoodles;       // list of BNOODLEINFO used for handrail
   fp            m_fMaxPostDistanceBig;  // maximum distance between posts, in meters, for ones up to roof
   fp            m_fMaxPostDistanceSmall;   // for small ones just supporting balustrade
   BOOL              m_fPostDivideIntoFull;  // if TRUE, use fMaxPostDistanceBig. FALSE, use only fMaxPostDistanceSmall
   fp            m_fBraceBelow;    // number of meters below roof to put bracing
   DWORD             m_adwPostShape[5];   // shapes for post. 0=bottom,1=top(ifshort),2=top(iflong), 3=brace, 4=main
   DWORD             m_adwPostTaper[3];   // taper for post. 0=bottom,1=top(if short), 3=top(if long)
   CPoint            m_apPostSize[5];     // size for post. 0=bottom,1=top(if short), 3=top(if long),3=brace, 4=main
   BOOL              m_afPostUse[4];      // use this?, 0=bottom,1=top (if short),2=top(if long), 3=brace
   BOOL              m_fPostWantFullHeightCutout;  // set to TRUE if want full height posts for cutout edges, etc. just ballistrade height
   fp            m_fPostHeightAboveBalustrade; // posts go this high above the balustrades, in meters
   BOOL              m_afHorzRailUse[4];       // if TRUE use handrail
                                                // [0] is the handrail, [1] the top rail, [2] the bottom rail, [3] for the mass-produced horizontal members
   DWORD             m_adwHorzRailShape[4];    // handrail shape, NS_XXX.
   CPoint            m_apHorzRailSize[4];      // p[0] = height, p[1] = depth, (looking front on ballistrade)
   CPoint            m_apHorzRailOffset[4];    // p[1] = front/back, p[2] = offset from m_fHeight (for handrail and top rail) and from ground for bottom rail, and not used for auto
   CPoint            m_pHorzRailAuto;  // p[0] = meters above floor where start, p[1]=meters above top-rail where end, p[2] = distance between memeber
   BOOL              m_fVertUse;       // set to true if using verticals
   BOOL              m_fVertUsePoint;  // if TRUE then draw the point
   DWORD             m_dwVertPointTaper;  // taper on the point
   DWORD             m_adwVertShape[2];    // vertical shape, 0 for main, 1 for point
   CPoint            m_apVertSize[2];      // vertical size. .p[0] = width, .p[1] = depth. .p[2] = point height (only for point), [0] for main, [1] for point
   fp            m_fVertOffset;    // offset in depth
   CPoint            m_pVertAuto;      // [0] = meters above floor, [1] = meters above m_fHeight, [2] = distance between
   BOOL              m_fPanelUse;      // set to true if using panel
   CPoint            m_pPanelInfo;     // [0] = offset from floor (postivie), [1] = top (m_fHeight), [2] = distance from right/left
   fp            m_fPanelOffset;   // offset panel in this amount
   fp            m_fPanelThickness;   // how thick panel is
   DWORD             m_dwBrace;        // 0 for none, 1 for x, 2 for alternate slash, 3 for v, 4 for ^
   CPoint            m_pBraceSize;     // [0] = height, [1] is depth
   TEXTUREPOINT      m_pBraceTB;       // .h if offset from bottom, .v is offset from top
   DWORD             m_dwBraceShape;   // brace shape
   fp            m_fBraceOffset;   // indent brace offset
   DWORD             m_dwBalStyle;     // balustrade style
   DWORD             m_dwType;         // dont know what this is for at the moment
};
typedef CBalustrade *PCBalustrade;

DLLEXPORT void RemoveLinear (PCListFixed pl, BOOL fLoop);


/**********************************************************************************
CPiers */
// pier styles
#define PS_CUSTOM                0  // custom pier
#define PS_LOGSTUMP              1  // log stumps
#define PS_STEEL                 2  // 75mm RHS steel
#define PS_WOOD                  3  // 100mm wood
#define PS_CEMENTSQUARE          4  // 300mm cement
#define PS_CEMENTROUND           5  // 300mm round
#define PS_GREEK                 6  // greek column

// pier color indecies - into m_adwBalColor. Used internally
#define PIERCOLOR_POSTMAIN        0
#define PIERCOLOR_POSTBASE        1
#define PIERCOLOR_POSTTOP         2
#define PIERCOLOR_POSTBRACE       3
#define PIERCOLOR_MAX             4 // used for number of balustrade colors.

class CDoubleSurface;

class DLLEXPORT CPiers{
public:
   ESCNEWDELETE;

   CPiers (void);
   ~CPiers (void);

   BOOL InitButDontCreate (DWORD dwRenderShard, DWORD dwType, PCObjectTemplate pTemplate);
   BOOL Init (DWORD dwRenderShard, DWORD dwType, PCObjectTemplate pTemplate, PCSpline pBottom, PCSpline pTop, BOOL fIndent);
   BOOL NewSplines (DWORD dwRenderShard, PCSpline pBottom, PCSpline pTop);
   PCMMLNode2 MMLTo (void);
   BOOL MMLFrom (PCMMLNode2 pNode);
   CPiers *Clone (void);
   void Render (DWORD dwRenderShard, POBJECTRENDER pr, CRenderSurface *prs);
   void QueryBoundingBox (PCPoint pCorner1, PCPoint pCorner2);
   BOOL CutoutBasedOnSurface (DWORD dwRenderShard, PCSplineSurface pss, PCMatrix m);
   void ExtendPostsToGround (DWORD dwRenderShard);
   void ClaimClear (void);
   void ClaimCloneTo (CPiers *pCloneTo, PCObjectTemplate pTemplate);
   BOOL ClaimRemove (DWORD dwID);
   BOOL CloneTo (CPiers *pNew, PCObjectTemplate pTemplate);
   BOOL ClaimFindByID (DWORD dwID);
   PWSTR AppearancePage (DWORD dwRenderShard, PCEscWindow pWindow, CObjectBuildBlock *pBB);
   PWSTR OpeningsPage (DWORD dwRenderShard, PCEscWindow pWindow, CObjectBuildBlock *pBB);
   PWSTR DisplayPage (PCEscWindow pWindow);
   PWSTR CornersPage (DWORD dwRenderShard, PCEscWindow pWindow);
   void ControlPointEnum (PCListFixed plDWORD);
   BOOL ControlPointSet (DWORD dwRenderShard, DWORD dwID, PCPoint pVal, PCPoint pViewer,CObjectBuildBlock *pBB);
   BOOL ControlPointQuery (DWORD dwID, POSCONTROL pInfo);
   BOOL Deconstruct (DWORD dwRenderShard, BOOL fAct);

   // control points displayed
   DWORD             m_dwDisplayControl;  // 0 for end points, 1 for openings
   BOOL              m_fForceVert;     // if true for top to be over bottom
   BOOL              m_fForceLevel;    // if true, force bottom to be at 0
   BOOL              m_fShowSkirting;  // if TRUE wants to show skirting

   DWORD             m_dwRenderShardTemp; // used for some dialogs

//private:
   BOOL DeterminePostLocations (DWORD dwRenderShard);
   void PostToCoord (DWORD dwStart, DWORD dwEnd, PCListFixed plBottom, PCListFixed plTop);
   fp LengthFromCoord (PCListFixed pl, DWORD dwStart, DWORD dwEnd);
   CNoodle* NoodleFromCoord (PCListFixed plBottom, PCListFixed plTop,
                                       BOOL fBaseFromTop, fp fOffset,
                                       DWORD dwStart, DWORD dwEnd, BOOL fLooped);
   void DistanceFromPost (PCListFixed pl, DWORD dwPost, fp fRight, PCPoint pVal);
   void ParamFromStyle (DWORD dwStyle);
   DWORD ClaimSurface (DWORD dwColor, COLORREF cColor, DWORD dwMaterialID, PWSTR pszScheme,
      const GUID *pgTextureCode = NULL, const GUID *pgTextureSub = NULL,
      PTEXTUREMODS pTextureMods = NULL, fp *pafTextureMatrix = NULL);
   void ClaimAllNecessaryColors (void);
   BOOL GenerateIntersect (void);
   void UpdateSkirting (DWORD dwRenderShard, fp fHeight);
   void UpdateSkirtingCutouts (void);

   PCSplineSurface   m_pssControlInter;   // temporary spline surface used for control intersections

   PCObjectTemplate  m_pTemplate;    // object template
   DWORD             m_adwBalColor[PIERCOLOR_MAX];  // 0 for no color, else into into m_pTemplate color

   PCSpline          m_psBottom;        // spline at the bottom
   PCSpline          m_psTop;           // spline at the top
   PCSpline          m_psOrigBottom;    // original bottom - used for skirting
   PCSpline          m_psOrigTop;       // original top - used for skirting
   CDoubleSurface*   m_pdsSkirting;     // skirting fp surface
   fp            m_fHeight;          // height to draw (if no ground)
   fp            m_fDepthBelow;       // how far below ground it goes
   BOOL              m_fIndent;           // set to true if indenting from loop

   CListFixed        m_lCutouts;       // list intialized to sizeof(fp)*2. where cutouts start and stop

   CListFixed        m_lPosts;         // list of BPOSTINFO structures
   fp            m_fMaxPostDistanceBig;  // maximum distance between posts, in meters, for ones up to roof
   fp            m_fBraceBelow;    // number of meters below roof to put bracing
   DWORD             m_adwPostShape[5];   // shapes for post. 0=bottom,1=top(ifshort),2=top(iflong), 3=brace, 4=main
   DWORD             m_adwPostTaper[3];   // taper for post. 0=bottom,1=top(if short), 3=top(if long)
   CPoint            m_apPostSize[5];     // size for post. 0=bottom,1=top(if short), 3=top(if long),3=brace, 4=main
   BOOL              m_afPostUse[4];      // use this?, 0=bottom,1=top (if short),2=top(if long), 3=brace
   DWORD             m_dwBalStyle;     // Piers style
   DWORD             m_dwType;         // dont know what this is for at the moment
};
typedef CPiers *PCPiers;


/********************************************************************************
CDoubleSurface */

#define INTERSECTEXTEND        .1       // amount to extend intersections. BUGFIX - Was .05

typedef struct {
   DWORD       dwID;       // surface ID
   BOOL        fAlsoEmbed; // if TRUE, also claimed for embedding
   DWORD       dwReason;   // 0 for side A primary, 1 for sideB primary, 2 for drawing edge,
                           // 3 for side A overlay, 4 for sideB overlay
                           // 10 for framing 0, 11 for framing 1, 12 for framing 2
} CLAIMSURFACE, *PCLAIMSURFACE;

class DLLEXPORT CDoubleSurface {
public:
   ESCNEWDELETE;

   CDoubleSurface (void);
   ~CDoubleSurface (void);

   BOOL Init (DWORD dwRenderShard, DWORD dwType, PCObjectTemplate pTemplate);
   BOOL InitButDontCreate (DWORD dwType, PCObjectTemplate pTemplate);

   void MatrixSet (PCMatrix pm);
   void MatrixGet (PCMatrix pm);
   BOOL NewSize (fp fWidth, fp fHeight, DWORD dwH = 0, DWORD dwV = 0,
                                   fp *pafHOrig = 0, fp *pafVOrig = 0,
                                   fp *pafHNew = 0, fp *pafVNew = 0);

   void TellEmbeddedThatMoved (void);
   void EmbedIntegrityCheck (void);
   BOOL RecalcSides (void);
   BOOL RecalcEdges (void);
   void ClaimClear (void);
   BOOL ClaimRemove (DWORD dwID);
   DWORD ClaimSurface (DWORD dwReason, BOOL fAlsoEmbed, COLORREF cColor, DWORD dwMaterialID, PWSTR pszScheme = NULL,
      const GUID *pgTextureCode = NULL, const GUID *pgTextureSub = NULL,
      PTEXTUREMODS pTextureMods = NULL, fp *pafTextureMatrix = NULL);
   PCLAIMSURFACE ClaimFindByReason (DWORD dwReason);
   PCLAIMSURFACE ClaimFindByID (DWORD dwID);
   void ClaimCloneTo (CDoubleSurface *pCloneTo, PCObjectTemplate pTemplate);
   void Render (DWORD dwRenderShard, POBJECTRENDER pr, CRenderSurface *prs);
   void QueryBoundingBox (PCPoint pCorner1, PCPoint pCorner2);
   PCSplineSurface CDoubleSurface::OverlayShapeFindByColor (DWORD dwColor, PWSTR *ppszName, PTEXTUREPOINT *pptp,
                                         DWORD *pdwNum, BOOL *pfClockwise);
   void CloneTo (CDoubleSurface *pNew, PCObjectTemplate pTemplate);
   PCMMLNode2 MMLTo (void);
   BOOL MMLFrom (PCMMLNode2 pNode);
   BOOL ControlPointChanged (DWORD dwX, DWORD dwY, int iSide);
   BOOL ControlPointsSet (DWORD dwControlH, DWORD dwControlV,
      PCPoint paPoints, DWORD *padwSegCurveH, DWORD *padwSegCurveV,
      DWORD dwDivide);
   PCSplineSurface SurfaceOvershootForIntersect (BOOL fSideA);
   void EnforceConstraints (void);
   void ChangedHV (DWORD dwH, DWORD dwV,
                                   fp *pafHOrig, fp *pafVOrig,
                                   fp *pafHNew, fp *pafVNew);
   PCListFixed FindIntersections (fp fExtend, int iThis, DWORD dwWithWorld, DWORD dwWithSelf, DWORD dwFlags,
      BOOL fIgnoreCutouts);
   //PCListFixed FindIntersections (fp fExtend, int iThis, DWORD dwWithWorld, DWORD dwWithSelf, DWORD dwFlags = 0x0001,
   //   BOOL fIgnoreCutouts = FALSE);
   BOOL FindIntersectionsWithWorld (DWORD dwWith, PCListFixed plPSS, PCListFixed plMatrix);
   BOOL FindIntersectionsWithSelf (DWORD dwWith, PCListFixed plPSS, PCListFixed plMatrix);
   BOOL IntersectWithOtherSurfaces (DWORD dwFlags, DWORD dwMode, PWSTR pszCutout, BOOL fIgnoreCutouts);
   BOOL CutoutFromSpline (PWSTR pszName, DWORD dwNum, PCPoint paPoints, DWORD *padwSegCurve,
                                       DWORD dwMinDivide, DWORD dwMaxDivide, fp fDetail,
                                       BOOL fClockwise);
   void DrawEdge (PTEXTUREPOINT pCutoutA, DWORD dwNumA, CRenderSurface *prs);
   BOOL ShrinkToCutouts (BOOL fQuery);
   BOOL IntersectWithSurfacesAndCutout (
      CSplineSurface **papssA, PCMatrix paMatrixA, DWORD dwNumA, PTEXTUREPOINT pKeepA,
      CSplineSurface **papssB, PCMatrix paMatrixB, DWORD dwNumB, PTEXTUREPOINT pKeepB,
      PWSTR pszName, BOOL fIgnoreCutouts,
      BOOL fClockwise, DWORD dwMode, BOOL fUseMin, BOOL fFinalClockwise);

   BOOL AddOverlay (BOOL fSideA, PWSTR pszName, PTEXTUREPOINT ptp,
                    DWORD dwNum, BOOL fCLockwise, DWORD *pdwDisplayControl,
                    BOOL fSelect = TRUE);

   DWORD CDoubleSurface::ControlNumGet (BOOL fH);
   BOOL ControlPointGet (int iSide, DWORD dwH, DWORD dwV, PCPoint pVal);
   BOOL HVToInfo (int iSide, fp h, fp v, PCPoint pPoint = NULL, PCPoint pNorm = NULL, PTEXTUREPOINT pText = NULL);
   BOOL ControlPointSet (int iSide, DWORD dwH, DWORD dwV, PCPoint pVal);
   BOOL HVDrag (int iSide, PTEXTUREPOINT pTextOld, PCPoint pNew, PCPoint pViewer, PTEXTUREPOINT pTextNew);
   void DetailGet (DWORD *pdwMinDivideH, DWORD *pdwMaxDivideH, DWORD *pdwMinDivideV, DWORD *pdwMaxDivideV,
      fp *pfDetail);
   BOOL DetailSet (DWORD dwMinDivideH, DWORD dwMaxDivideH, DWORD dwMinDivideV, DWORD dwMaxDivideV,
      fp fDetail);
   DWORD SegCurveGet (BOOL fH, DWORD dwIndex);
   BOOL SegCurveSet (BOOL fH, DWORD dwIndex, DWORD dwValue);
   BOOL IntersectLine (int iSide, PCPoint pStart, PCPoint pDirection, PTEXTUREPOINT ptp,
      BOOL fIncludeCutouts, BOOL fPositiveAlpha);
   BOOL ControlPointSetRotation (DWORD dwX, DWORD dwY, PCPoint pVal, BOOL fSentNotify,
      BOOL fKeepSplineMatrixIdent);
   void RotateSplines (PCPoint p1, PCPoint p2);
   void TellOtherWallsThatMoving (PCPoint pOld, PCPoint pNew);
   void WallIntelligentBevel (void);

   DWORD EdgeOrigNumPointsGet (void);
   BOOL EdgeOrigPointGet (DWORD dwH, PCPoint pP);
   BOOL EdgeOrigSegCurveGet (DWORD dwH, DWORD *pdwSegCurve);
   void EdgeDivideGet (DWORD *pdwMinDivide, DWORD *pdwMaxDivide, fp *pfDetail);
   BOOL EdgeInit (BOOL fLooped, DWORD dwPoints, PCPoint paPoints, PTEXTUREPOINT paTextures,
      DWORD *padwSegCurve, DWORD dwMinDivide, DWORD dwMaxDivide,
      fp fDetail);

   PWSTR SurfaceDetailPage (PCEscWindow pWindow);
   PWSTR SurfaceThicknessPage (PCEscWindow pWindow);
   PWSTR SurfaceFramingPage (PCEscWindow pWindow);
   PWSTR SurfaceBevellingPage (PCEscWindow pWindow);
   PWSTR SurfaceCurvaturePage (PCEscWindow pWindow);
   PWSTR SurfaceShowCladPage (PCEscWindow pWindow);
   PWSTR SurfaceControlPointsPage (PCEscWindow pWindow);
   PWSTR SurfaceEdgeInterPage (PCEscWindow pWindow);
   PWSTR SurfaceEdgePage (PCEscWindow pWindow);
   PWSTR SurfaceOverMainPage (PCEscWindow pWindow, BOOL fSideA, DWORD *pdwDisplayControl);
   PWSTR SurfaceCutoutMainPage (PCEscWindow pWindow, BOOL fSideA);
   PWSTR SurfaceFloorsPage (PCEscWindow pWindow);

   BOOL ControlPointQuery (DWORD dwColor, DWORD dwID, POSCONTROL pInfo);
   BOOL ControlPointSet (DWORD dwColor, DWORD dwID, PCPoint pVal, PCPoint pViewer);
   void ControlPointEnum (DWORD dwColor, PCListFixed plDWORD);
   BOOL ContHVQuery (PCPoint pEye, PCPoint pClick, DWORD dwSurface, PTEXTUREPOINT pOld, PTEXTUREPOINT pHV);
   BOOL ContCutout (const GUID *pgEmbed, DWORD dwNum, PCPoint paFront, PCPoint paBack, BOOL fBothSides);
   BOOL ContCutoutToZipper (const GUID *pgEmbed, PCListFixed plistFrontHVXYZ, PCListFixed plistBackHVXYZ);
   fp ContThickness (DWORD dwSurface, PTEXTUREPOINT pHV);
   DWORD ContSideInfo (DWORD dwSurface, BOOL fOtherSide);
   BOOL ContMatrixFromHV (DWORD dwSurface, PTEXTUREPOINT pHV, fp fRotation, PCMatrix pm);
   BOOL Message (DWORD dwMessage, PVOID pParam);
   BOOL ContEmbeddedRenamed (const GUID *pgOld, const GUID *pgNew);

   CSplineSurface    m_SplineCenter;  // spline in the center
   CSplineSurface    m_SplineA, m_SplineB;  // A and B side splines
   CSpline           m_SplineEdge;     // edge spline

   // bevel
   fp            m_fBevelLeft;  // -PI/2 to PI/2. 0 = perpendicular
   fp            m_fBevelRight; // -PI/2 to PI/2. 0 = perpendicular
   fp            m_fBevelTop;   // -PI/2 to PI/2. 0 = perpendicular
   fp            m_fBevelBottom;   // -PI/2 to PI/2. 0 = perpendicular

   // contraints
   BOOL              m_fConstBottom;   // bottom must be flat
   BOOL              m_fConstAbove;    // all top points must be above bottom points
   BOOL              m_fConstRectangle;   // all 4 corner points must form rectangle

   // thickness
   fp            m_fThickStud;  // thickness of stud
   fp            m_fThickA;     // thickness of cladding on side A
   fp            m_fThickB;     // thickness of cladding on side B

   // hide edges
   BOOL              m_fHideSideA;     // if true don't draw side A
   BOOL              m_fHideSideB;     // if true don't draw side B
   BOOL              m_fHideEdges;     // if true don't draw edges
   DWORD             m_dwExtSideA;     // 0 if internal, 1 if external
   DWORD             m_dwExtSideB;     // 0 if internal, 1 if external

   // where the floors are
   CListFixed        m_listFloorsA;    // list of doubles. Floor heights on side A
   CListFixed        m_listFloorsB;    // list of doubles. Floor heights on side B

   PCObjectTemplate  m_pTemplate;    // object template

   // framing information
   BOOL              m_fFramingDirty;  // set to TRUE if the framing calculations are dirty
   BOOL              m_fFramingShowOnlyWhenVisible;      // if TRUE, only show the framing if sidea A or B hidden, or general structure hidden
   BOOL              m_fFramingRotate; // if TRUE rotate the framing by 90 degrees
   BOOL              m_afFramingShow[3];   // if TRUE then show the framing
   fp                m_afFramingInset[3][2]; // inset for the layers of framing
   fp                m_afFramingExtThick[3][2]; // extra thickness
   BOOL              m_afFramingInsetTL[3];  // TRUE if framing measure by TL, FALSE if BR
   fp                m_afFramingMinDist[3];  // minimum distance between beams
   fp                m_afFramingBevel[3][2]; // bevel angle for framing
   fp                m_afFramingExtend[3][2];   // extend framing this many meters
   DWORD             m_adwFramingShape[3];   // shape of noodle to use
   CPoint            m_apFramingScale[3];    // size of the framing. p[0] = width, p[1] = depth
   CListFixed        m_lFNOODLE;             // list of FNOODLES (which contain PCNoodle) for framing

private:
   BOOL              m_dwType;      // type
   CMatrix           m_MatrixToObject; // translate from fp-sided surface to object space
   CMatrix           m_MatrixFromObject;  // translate from object to fp-sided surface space
   CListFixed        m_listCLAIMSURFACE;   // list of claimed surfaces, for object surface add.

   CListFixed        m_lDrawEdgeHVTempA;  // minimize mallocs during render
   CListFixed        m_lDrawEdgeHVTempB;  // minimize mallocs during render
   CListFixed        m_lDrawEdgeA;        // minimize mallocs during render
   CListFixed        m_lDrawEdgeB;        // minimize mallocs during render
   CListFixed        m_lDrawEdgeABack;    // minimize mallocs during render
   CListFixed        m_lDrawEdgeBBack;    // minimize mallocs during render

   PCSplineSurface Side (int iSide);
   BOOL BevelPoint (DWORD dwX, DWORD dwY, int iSideOrig,
                    PCPoint pNew, int iSideNew, BOOL fOvershoot = FALSE);
   BOOL OverlayForPaint (BOOL fSideA, PTEXTUREPOINT pClick, PCListFixed pl);
   PCListFixed OverlayIntersections (BOOL fSideA);
   BOOL FramingAddSolid (PCSplineSurface pss, PTEXTUREPOINT patp, fp *pafBevel,
                                      fp *pafExtend, DWORD dwShape, PCPoint pScale,
                                      DWORD dwColor, fp fCenter);
   BOOL FramingAddWithBreaks (PCSplineSurface pss, PTEXTUREPOINT patp, fp *pafBevel,
                                      fp *pafExtend, DWORD dwShape, PCPoint pScale,
                                      DWORD dwColor, fp fCenter);
   BOOL FramingAdd (PCSplineSurface pss, 
                                 BOOL fHorzBeams, fp *pafInset, BOOL fMeasureInsetTL, fp fMinDist,
                                 fp *pafBevel, fp *pafExtend, DWORD dwShape, PCPoint pScale,
                                 DWORD dwColor, fp fCenter);
   void FramingClear (void);
   void FramingCalcIfDirty (void);


};

typedef CDoubleSurface *PCDoubleSurface;

DLLEXPORT void BoundingBoxApplyMatrix (PCPoint pCorner1, PCPoint pCorner2, PCMatrix pMatrix);
DLLEXPORT DWORD OverlayShapeNameToColor (PWSTR pszName);
DLLEXPORT BOOL DSGenerateThreeDFromSplineSurface (PWSTR pszControl, PCEscPage pPage, PCSplineSurface pss, DWORD dwUse);
DLLEXPORT BOOL DSGenerateThreeDFromOverlays (PWSTR pszControl, PCEscPage pPage, PCSplineSurface pss,
                                        BOOL fSideA);
DLLEXPORT BOOL DSGenerateThreeDForPosition (PWSTR pszControl, PCEscPage pPage, PCSplineSurface pss,
                                       DWORD dwGridSize, BOOL fSideA);
DLLEXPORT BOOL DSGenerateThreeDForSplitOverlay (PWSTR pszControl, PCEscPage pPage, PCSplineSurface pss,
                                      PWSTR pszOverlay, BOOL fSideA);
DLLEXPORT BOOL DSGenerateThreeDFromCutouts (PWSTR pszControl, PCEscPage pPage, PCSplineSurface pss,
                                        BOOL fSideA);
DLLEXPORT BOOL DSGenerateThreeDFromSpline (PWSTR pszControl, PCEscPage pPage, PCSpline pSpline, PCSplineSurface pss,
                                      DWORD dwUse);
DLLEXPORT PVOID ClipCutout (PTEXTUREPOINT ptp, DWORD dwNum, BOOL fClockwise);

DLLEXPORT BOOL GenerateThreeDFromSegments (PWSTR pszControl, PCEscPage pPage, PCSplineSurface pss,
                                      PCListFixed plSeq, PCListFixed plSelected, DWORD dwSideA);
DLLEXPORT void FitTo (PTEXTUREPOINT pt, DWORD dwFitH, DWORD dwFitV, fp *afHOrig, fp *afVOrig, fp *afHNew, fp *afVNew, fp *afHScale, fp *afVScale,
            BOOL fMinMax = TRUE);

extern PWSTR    gpapszOverlayNames[9];


/********************************************************************************
CSingleSurface */
class DLLEXPORT CSingleSurface {
public:
   ESCNEWDELETE;

   CSingleSurface (void);
   ~CSingleSurface (void);

   BOOL Init (DWORD dwType, PCObjectTemplate pTemplate);
   BOOL InitButDontCreate (DWORD dwType, PCObjectTemplate pTemplate);

   void MatrixSet (PCMatrix pm);
   void MatrixGet (PCMatrix pm);
   void TellEmbeddedThatMoved (void);
   void EmbedIntegrityCheck (void);
   BOOL RecalcSides (void);
   BOOL RecalcEdges (void);
   void ClaimClear (void);
   BOOL ClaimRemove (DWORD dwID);
   DWORD ClaimSurface (DWORD dwReason, BOOL fAlsoEmbed, COLORREF cColor, DWORD dwMaterialID, PWSTR pszScheme = NULL,
      const GUID *pgTextureCode = NULL, const GUID *pgTextureSub = NULL,
      PTEXTUREMODS pTextureMods = NULL, fp *pafTextureMatrix = NULL);
   PCLAIMSURFACE ClaimFindByReason (DWORD dwReason);
   PCLAIMSURFACE ClaimFindByID (DWORD dwID);
   void ClaimCloneTo (CSingleSurface *pCloneTo, PCObjectTemplate pTemplate);
   void Render (DWORD dwRenderShard, POBJECTRENDER pr, CRenderSurface *prs);
   void QueryBoundingBox (PCPoint pCorner1, PCPoint pCorner2);
   PCSplineSurface OverlayShapeFindByColor (DWORD dwColor, PWSTR *ppszName, PTEXTUREPOINT *pptp,
                                         DWORD *pdwNum, BOOL *pfClockwise);
   void CloneTo (CSingleSurface *pNew, PCObjectTemplate pTemplate);
   PCMMLNode2 MMLTo (void);
   BOOL MMLFrom (PCMMLNode2 pNode);
   BOOL ControlPointChanged (DWORD dwX, DWORD dwY);
   BOOL ControlPointsSet (BOOL fLoopH, BOOL fLoopV, DWORD dwControlH, DWORD dwControlV,
      PCPoint paPoints, DWORD *padwSegCurveH, DWORD *padwSegCurveV);
   void ChangedHV (DWORD dwH, DWORD dwV,
                                   fp *pafHOrig, fp *pafVOrig,
                                   fp *pafHNew, fp *pafVNew);
   BOOL CutoutFromSpline (PWSTR pszName, DWORD dwNum, PCPoint paPoints, DWORD *padwSegCurve,
                                       DWORD dwMinDivide, DWORD dwMaxDivide, fp fDetail,
                                       BOOL fClockwise);

   BOOL AddOverlay (PWSTR pszName, PTEXTUREPOINT ptp,
                                       DWORD dwNum, BOOL fCLockwise, DWORD *pdwDisplayControl);

   BOOL LoopSet (BOOL fH, BOOL fLoop);
   BOOL LoopGet (BOOL fH);
   DWORD ControlNumGet (BOOL fH);
   BOOL ControlPointGet (DWORD dwH, DWORD dwV, PCPoint pVal);
   BOOL HVToInfo (fp h, fp v, PCPoint pPoint = NULL, PCPoint pNorm = NULL, PTEXTUREPOINT pText = NULL);
   BOOL ControlPointSet (DWORD dwH, DWORD dwV, PCPoint pVal);
   BOOL HVDrag (PTEXTUREPOINT pTextOld, PCPoint pNew, PCPoint pViewer, PTEXTUREPOINT pTextNew);
   void DetailGet (DWORD *pdwMinDivideH, DWORD *pdwMaxDivideH, DWORD *pdwMinDivideV, DWORD *pdwMaxDivideV,
      fp *pfDetail);
   BOOL DetailSet (DWORD dwMinDivideH, DWORD dwMaxDivideH, DWORD dwMinDivideV, DWORD dwMaxDivideV,
      fp fDetail);
   DWORD SegCurveGet (BOOL fH, DWORD dwIndex);
   BOOL SegCurveSet (BOOL fH, DWORD dwIndex, DWORD dwValue);
   BOOL IntersectLine (PCPoint pStart, PCPoint pDirection, PTEXTUREPOINT ptp,
      BOOL fIncludeCutouts, BOOL fPositiveAlpha);

   DWORD EdgeOrigNumPointsGet (void);
   BOOL EdgeOrigPointGet (DWORD dwH, PCPoint pP);
   BOOL EdgeOrigSegCurveGet (DWORD dwH, DWORD *pdwSegCurve);
   void EdgeDivideGet (DWORD *pdwMinDivide, DWORD *pdwMaxDivide, fp *pfDetail);
   BOOL EdgeInit (BOOL fLooped, DWORD dwPoints, PCPoint paPoints, PTEXTUREPOINT paTextures,
      DWORD *padwSegCurve, DWORD dwMinDivide, DWORD dwMaxDivide,
      fp fDetail);

   PWSTR SurfaceDetailPage (PCEscWindow pWindow);
   PWSTR SurfaceCurvaturePage (PCEscWindow pWindow);
   PWSTR SurfaceControlPointsPage (PCEscWindow pWindow);
   PWSTR SurfaceEdgePage (PCEscWindow pWindow);
   PWSTR SurfaceOverMainPage (PCEscWindow pWindow, DWORD *pdwDisplayControl);

   BOOL ControlPointQuery (DWORD dwColor, DWORD dwID, POSCONTROL pInfo);
   BOOL ControlPointSet (DWORD dwColor, DWORD dwID, PCPoint pVal, PCPoint pViewer);
   void ControlPointEnum (DWORD dwColor, PCListFixed plDWORD);
   BOOL ContHVQuery (PCPoint pEye, PCPoint pClick, DWORD dwSurface, PTEXTUREPOINT pOld, PTEXTUREPOINT pHV);
   BOOL ContCutout (const GUID *pgEmbed, DWORD dwNum, PCPoint paFront, PCPoint paBack, BOOL fBothSides);
   DWORD ContSideInfo (DWORD dwSurface, BOOL fOtherSide);
   BOOL ContMatrixFromHV (DWORD dwSurface, PTEXTUREPOINT pHV, fp fRotation, PCMatrix pm);
   BOOL ContEmbeddedRenamed (const GUID *pgOld, const GUID *pgNew);

   CSplineSurface    m_Spline;  // spline in the center
   CSpline           m_SplineEdge;     // edge spline



   PCObjectTemplate  m_pTemplate;    // object template

private:
   BOOL              m_dwType;      // type
   CMatrix           m_MatrixToObject; // translate from fp-sided surface to object space
   CMatrix           m_MatrixFromObject;  // translate from object to fp-sided surface space
   CListFixed        m_listCLAIMSURFACE;   // list of claimed surfaces, for object surface add.


};

typedef CSingleSurface *PCSingleSurface;

/**********************************************************************************
CObjectStructSurface */
// {8F7525C5-0B82-4799-AB76-8EBA45ED2358}
DEFINE_GUID(CLSID_StructSurface, 
0x8f7525c5, 0xb82, 0x4799, 0xab, 0x76, 0x8e, 0xba, 0x45, 0xed, 0x23, 0x58);

// {C18492E2-0E74-4c95-B56E-1FF4355810F9}
DEFINE_GUID(CLSID_StructSurfaceInternalStudWall, 
0xc18492e2, 0xe74, 0x4c95, 0xb5, 0x6e, 0x1f, 0xf4, 0x35, 0x58, 0x10, 0xf9);
DEFINE_GUID(CLSID_StructSurfaceExternalStudWall, 
0xc18492e3, 0xe74, 0x4c95, 0xb5, 0x6e, 0x1f, 0xf4, 0x35, 0x58, 0x10, 0xf9);
DEFINE_GUID(CLSID_StructSurfaceInternalStudWallCurved, 
0xc18492e4, 0xe74, 0x4c95, 0xb5, 0x6e, 0x1f, 0xf4, 0x35, 0x58, 0x10, 0xf9);
DEFINE_GUID(CLSID_StructSurfaceExternalStudWallCurved, 
0xc18492e5, 0xe74, 0x4c95, 0xb5, 0x6e, 0x1f, 0xf4, 0x35, 0x58, 0x10, 0xf9);
DEFINE_GUID(CLSID_StructSurfaceInternalBlockWall, 
0xc18492e6, 0xe74, 0x4c95, 0xb5, 0x6e, 0x1f, 0xf4, 0x35, 0x58, 0x10, 0xf9);
DEFINE_GUID(CLSID_StructSurfaceExternalBlockWall, 
0xc18492e7, 0xe74, 0x4c95, 0xb5, 0x6e, 0x1f, 0xf4, 0x35, 0x58, 0x10, 0xf9);
DEFINE_GUID(CLSID_StructSurfaceInternalBlockWallCurved, 
0xc18492e8, 0xe74, 0x4c95, 0xb5, 0x6e, 0x1f, 0xf4, 0x35, 0x58, 0x10, 0xf9);
DEFINE_GUID(CLSID_StructSurfaceExternalBlockWallCurved, 
0xc18492e9, 0xe74, 0x4c95, 0xb5, 0x6e, 0x1f, 0xf4, 0x35, 0x58, 0x10, 0xf9);
DEFINE_GUID(CLSID_StructSurfaceExternalWallOption, 
0xc18492e9, 0xe74, 0x4c95, 0xb5, 0x68, 0x1f, 0xf4, 0x35, 0x58, 0x10, 0xf9);
DEFINE_GUID(CLSID_StructSurfaceExternalWallSkirting, 
0xc18492e9, 0xe74, 0x4c95, 0xb5, 0x68, 0x1f, 0xf4, 0x38, 0x58, 0x10, 0xf9);

DEFINE_GUID(CLSID_StructSurfaceRoofFlat, 
0xc18492e7, 0xe74, 0x4c95, 0xb5, 0x6e, 0x1f, 0xf4, 0x35, 0x58, 0x10, 0xfa);
DEFINE_GUID(CLSID_StructSurfaceRoofCurved, 
0xc18492e7, 0xe74, 0x4c95, 0xb5, 0x6f, 0x1f, 0xf4, 0x35, 0x58, 0x10, 0xfa);
DEFINE_GUID(CLSID_StructSurfaceRoofCurvedPartial, 
0xc18492e7, 0xe74, 0x4c95, 0xb5, 0x60, 0x1f, 0xf4, 0x35, 0x58, 0x10, 0xfa);


DEFINE_GUID(CLSID_StructSurfaceFloorPadTiles, 
0xc18492e0, 0xe74, 0x4c95, 0xb5, 0x60, 0x1f, 0xf4, 0x35, 0x58, 0x10, 0xfa);
DEFINE_GUID(CLSID_StructSurfaceFloorFloor, 
0xc18492e1, 0xe74, 0x4c95, 0xb5, 0x60, 0x1f, 0xf4, 0x35, 0x58, 0x10, 0xfa);
DEFINE_GUID(CLSID_StructSurfaceFloorDropCeiling, 
0xc18492e2, 0xe74, 0x4c95, 0xb5, 0x60, 0x1f, 0xf4, 0x35, 0x58, 0x10, 0xfa);
DEFINE_GUID(CLSID_StructSurfaceFloorPadPlain, 
0xc18492e0, 0xe74, 0x4c95, 0xb5, 0x60, 0x1f, 0xf4, 0x35, 0x58, 0x11, 0xfa);
DEFINE_GUID(CLSID_StructSurfaceFloorDeck, 
0xc18492e1, 0xe74, 0x4c95, 0xb5, 0x60, 0x1f, 0xf4, 0x35, 0x58, 0x12, 0xfa);

// CObjectStructSurface is a test object to test the system
class DLLEXPORT CObjectStructSurface : public CObjectTemplate {
public:
   CObjectStructSurface (PVOID pParams, POSINFO pInfo);
   ~CObjectStructSurface (void);

   virtual void Delete (void);
   virtual void Render (POBJECTRENDER pr, DWORD dwSubObject);
   virtual CObjectSocket *Clone (void);
   virtual PCMMLNode2 MMLTo (void);
   virtual BOOL MMLFrom (PCMMLNode2 pNode);
   virtual BOOL ControlPointQuery (DWORD dwID, POSCONTROL pInfo);
   virtual BOOL ControlPointSet (DWORD dwID, PCPoint pVal, PCPoint pViewer);
   virtual void ControlPointEnum (PCListFixed plDWORD);
   virtual BOOL DialogQuery (void);
   virtual BOOL DialogShow (DWORD dwSurface, PCEscWindow pWindow, PCPoint pClick);
   virtual BOOL DialogCPQuery (void);
   virtual BOOL DialogCPShow (DWORD dwSurface, PCEscWindow pWindow, PCPoint pClick);
   virtual BOOL MoveReferencePointQuery (DWORD dwIndex, PCPoint pp);
   virtual BOOL MoveReferenceStringQuery (DWORD dwIndex, PWSTR psz, DWORD dwSize, DWORD *pdwNeeded);
   virtual BOOL ContHVQuery (PCPoint pEye, PCPoint pClick, DWORD dwSurface, PTEXTUREPOINT pOld, PTEXTUREPOINT pHV);
   virtual BOOL ContCutout (const GUID *pgEmbed, DWORD dwNum, PCPoint paFront, PCPoint paBack, BOOL fBothSides);
   virtual BOOL ContCutoutToZipper (const GUID *pgEmbed, PCListFixed plistFrontHVXYZ, PCListFixed plistBackHVXYZ);
   virtual BOOL ContMatrixFromHV (DWORD dwSurface, PTEXTUREPOINT pHV, fp fRotation, PCMatrix pm);
   virtual fp ContThickness (DWORD dwSurface, PTEXTUREPOINT pHV);
   virtual DWORD ContSideInfo (DWORD dwSurface, BOOL fOtherSide);
   virtual BOOL Message (DWORD dwMessage, PVOID pParam);
   virtual BOOL IntelligentAdjust (BOOL fAct);
   virtual BOOL ContEmbeddedRenamed (const GUID *pgOld, const GUID *pgNew);
   virtual BOOL IntelligentPositionDrag (CWorldSocket *pWorld, POSINTELLIPOSDRAG pInfo,  POSINTELLIPOSRESULT pResult);
   virtual BOOL Merge (GUID *pagWith, DWORD dwNum);
   virtual void QueryBoundingBox (PCPoint pCorner1, PCPoint pCorner2, DWORD dwSubObject);


   // private
   BOOL Merge (CObjectStructSurface *poss);
   CDoubleSurface    m_ds;

   // control points displayed
   DWORD             m_dwDisplayControl;  // 0 for curvature, 1 for edge


   CRenderSurface    m_Renderrs;    // to minimize mallocs
};
typedef CObjectStructSurface *PCObjectStructSurface;


/**********************************************************************************
CObjectSingleSurface */
// {41E3B9BE-1099-4cb1-8B61-BF9DB2CBAD3D}
DEFINE_GUID(CLSID_SingleSurface, 
0x41e3b9be, 0x1099, 0x4cb1, 0x8b, 0x61, 0xbf, 0x9d, 0xb2, 0xcb, 0xad, 0x3d);


// CObjectSingleSurface is a test object to test the system
class DLLEXPORT CObjectSingleSurface : public CObjectTemplate {
public:
   CObjectSingleSurface (PVOID pParams, POSINFO pInfo);
   ~CObjectSingleSurface (void);

   virtual void Delete (void);
   virtual void Render (POBJECTRENDER pr, DWORD dwSubObject);
   virtual CObjectSocket *Clone (void);
   virtual PCMMLNode2 MMLTo (void);
   virtual BOOL MMLFrom (PCMMLNode2 pNode);
   virtual BOOL ControlPointQuery (DWORD dwID, POSCONTROL pInfo);
   virtual BOOL ControlPointSet (DWORD dwID, PCPoint pVal, PCPoint pViewer);
   virtual void ControlPointEnum (PCListFixed plDWORD);
   virtual BOOL DialogQuery (void);
   virtual BOOL DialogShow (DWORD dwSurface, PCEscWindow pWindow, PCPoint pClick);
   virtual BOOL DialogCPQuery (void);
   virtual BOOL DialogCPShow (DWORD dwSurface, PCEscWindow pWindow, PCPoint pClick);
   virtual BOOL ContHVQuery (PCPoint pEye, PCPoint pClick, DWORD dwSurface, PTEXTUREPOINT pOld, PTEXTUREPOINT pHV);
   virtual BOOL ContCutout (const GUID *pgEmbed, DWORD dwNum, PCPoint paFront, PCPoint paBack, BOOL fBothSides);
   virtual BOOL ContCutoutToZipper (const GUID *pgEmbed, PCListFixed plistFrontHVXYZ, PCListFixed plistBackHVXYZ);
   virtual BOOL ContMatrixFromHV (DWORD dwSurface, PTEXTUREPOINT pHV, fp fRotation, PCMatrix pm);
   virtual fp ContThickness (DWORD dwSurface, PTEXTUREPOINT pHV);
   virtual DWORD ContSideInfo (DWORD dwSurface, BOOL fOtherSide);
   virtual BOOL ContEmbeddedRenamed (const GUID *pgOld, const GUID *pgNew);
   virtual void QueryBoundingBox (PCPoint pCorner1, PCPoint pCorner2, DWORD dwSubObject);
   

   void CalcScale (void);

   CSingleSurface    m_ds;

   // control points displayed
   DWORD             m_dwDisplayControl;  // 0 for curvature, 1 for edge

   // calculated by CalcScale()
   CPoint            m_pScaleMin;         // minimum XYZ of object
   CPoint            m_pScaleMax;         // maximum XYZ of object
   CPoint            m_pScaleSize;        // scale size, all values > EPSILON


   CRenderSurface    m_Renderrs;    // to minimize mallocs
};
typedef CObjectSingleSurface *PCObjectSingleSurface;

/**********************************************************************************
CObjectBalustrade */
// {740F4D5D-1923-484f-AC5B-369B444BB802}
DEFINE_GUID(CLSID_Balustrade,
0x740f4d5d, 0x1923, 0x484f, 0xac, 0x5b, 0x36, 0x9b, 0x44, 0x4b, 0xb8, 0x2);

DEFINE_GUID(CLSID_BalustradeBalVertWood,
0x740f4d5d, 0x1923, 0x484f, 0xfc, 0x5b, 0x36, 0x9b, 0x44, 0x4b, 0xb8, 0x00);
DEFINE_GUID(CLSID_BalustradeBalVertWood2,
0x740f4d5d, 0x1923, 0x484f, 0xfc, 0x5b, 0x36, 0x9b, 0x44, 0x4b, 0xb8, 0x01);
DEFINE_GUID(CLSID_BalustradeBalVertWood3,
0x740f4d5d, 0x1923, 0x484f, 0xfc, 0x5b, 0x36, 0x9b, 0x44, 0x4b, 0xb8, 0x02);
DEFINE_GUID(CLSID_BalustradeBalVertLog,
0x740f4d5d, 0x1923, 0x484f, 0xfc, 0x5b, 0x36, 0x9b, 0x44, 0x4b, 0xb8, 0x03);
DEFINE_GUID(CLSID_BalustradeBalVertWroughtIron,
0x740f4d5d, 0x1923, 0x484f, 0xfc, 0x5b, 0x36, 0x9b, 0x44, 0x4b, 0xb8, 0x04);
DEFINE_GUID(CLSID_BalustradeBalVertSteel,
0x740f4d5d, 0x1923, 0x484f, 0xfc, 0x5b, 0x36, 0x9b, 0x44, 0x4b, 0xb8, 0x05);
DEFINE_GUID(CLSID_BalustradeBalHorzPanels,
0x740f4d5d, 0x1923, 0x484f, 0xfc, 0x5b, 0x36, 0x9b, 0x44, 0x4b, 0xb8, 0x07);
DEFINE_GUID(CLSID_BalustradeBalHorzWood,
0x740f4d5d, 0x1923, 0x484f, 0xfc, 0x5b, 0x36, 0x9b, 0x44, 0x4b, 0xb8, 0x08);
DEFINE_GUID(CLSID_BalustradeBalHorzWire,
0x740f4d5d, 0x1923, 0x484f, 0xfc, 0x5b, 0x36, 0x9b, 0x44, 0x4b, 0xb8, 0x09);
DEFINE_GUID(CLSID_BalustradeBalHorzWireRail,
0x740f4d5d, 0x1923, 0x484f, 0xfc, 0x5b, 0x36, 0x9b, 0x44, 0x4b, 0xb8, 0x0a);
DEFINE_GUID(CLSID_BalustradeBalHorzPole,
0x740f4d5d, 0x1923, 0x484f, 0xfc, 0x5b, 0x36, 0x9b, 0x44, 0x4b, 0xb8, 0x0b);
DEFINE_GUID(CLSID_BalustradeBalPanel,
0x740f4d5d, 0x1923, 0x484f, 0xfc, 0x5b, 0x36, 0x9b, 0x44, 0x4b, 0xb8, 0x0c);
DEFINE_GUID(CLSID_BalustradeBalPanelSolid,
0x740f4d5d, 0x1923, 0x484f, 0xfc, 0x5b, 0x36, 0x9b, 0x44, 0x4b, 0xb8, 0x0d);
DEFINE_GUID(CLSID_BalustradeBalOpen,
0x740f4d5d, 0x1923, 0x484f, 0xfc, 0x5b, 0x36, 0x9b, 0x44, 0x4b, 0xb8, 0x0e);
DEFINE_GUID(CLSID_BalustradeBalOpenMiddle,
0x740f4d5d, 0x1923, 0x484f, 0xfc, 0x5b, 0x36, 0x9b, 0x44, 0x4b, 0xb8, 0x0f);
DEFINE_GUID(CLSID_BalustradeBalOpenPole,
0x740f4d5d, 0x1923, 0x484f, 0xfc, 0x5b, 0x36, 0x9b, 0x44, 0x4b, 0xb8, 0x10);
DEFINE_GUID(CLSID_BalustradeBalBraceX,
0x740f4d5d, 0x1923, 0x484f, 0xfc, 0x5b, 0x36, 0x9b, 0x44, 0x4b, 0xb8, 0x11);
DEFINE_GUID(CLSID_BalustradeBalFancyGreek,
0x740f4d5d, 0x1923, 0x484f, 0xfc, 0x5b, 0x36, 0x9b, 0x44, 0x4b, 0xb8, 0x12);
DEFINE_GUID(CLSID_BalustradeBalFancyGreek2,
0x740f4d5d, 0x1923, 0x484f, 0xfc, 0x5b, 0x36, 0x9b, 0x44, 0x4b, 0xb8, 0x13);
DEFINE_GUID(CLSID_BalustradeBalFancyWood,
0x740f4d5d, 0x1923, 0x484f, 0xfc, 0x5b, 0x36, 0x9b, 0x44, 0x4b, 0xb8, 0x14);

DEFINE_GUID(CLSID_BalustradeFenceVertPicket,
0x740f4d5d, 0x1923, 0x484f, 0xfc, 0x5b, 0x36, 0x9b, 0x44, 0x4b, 0xa8, 0x00);
DEFINE_GUID(CLSID_BalustradeFenceVertSteel,
0x740f4d5d, 0x1923, 0x484f, 0xfc, 0x5b, 0x36, 0x9b, 0x44, 0x4b, 0xa8, 0x01);
DEFINE_GUID(CLSID_BalustradeFenceVertWroughtIron,
0x740f4d5d, 0x1923, 0x484f, 0xfc, 0x5b, 0x36, 0x9b, 0x44, 0x4b, 0xa8, 0x02);
DEFINE_GUID(CLSID_BalustradeFenceVertPicketSmall,
0x740f4d5d, 0x1923, 0x484f, 0xfc, 0x5b, 0x36, 0x9b, 0x44, 0x4b, 0xa8, 0x03);
DEFINE_GUID(CLSID_BalustradeFenceVertPanel,
0x740f4d5d, 0x1923, 0x484f, 0xfc, 0x5b, 0x36, 0x9b, 0x44, 0x4b, 0xa8, 0x04);
DEFINE_GUID(CLSID_BalustradeFenceHorzLog,
0x740f4d5d, 0x1923, 0x484f, 0xfc, 0x5b, 0x36, 0x9b, 0x44, 0x4b, 0xa8, 0x05);
DEFINE_GUID(CLSID_BalustradeFenceHorzStick,
0x740f4d5d, 0x1923, 0x484f, 0xfc, 0x5b, 0x36, 0x9b, 0x44, 0x4b, 0xa8, 0x06);
DEFINE_GUID(CLSID_BalustradeFenceHorzPanels,
0x740f4d5d, 0x1923, 0x484f, 0xfc, 0x5b, 0x36, 0x9b, 0x44, 0x4b, 0xa8, 0x07);
DEFINE_GUID(CLSID_BalustradeFenceHorzWire,
0x740f4d5d, 0x1923, 0x484f, 0xfc, 0x5b, 0x36, 0x9b, 0x44, 0x4b, 0xa8, 0x08);
DEFINE_GUID(CLSID_BalustradeFencePanelSolid,
0x740f4d5d, 0x1923, 0x484f, 0xfc, 0x5b, 0x36, 0x9b, 0x44, 0x4b, 0xa8, 0x09);
DEFINE_GUID(CLSID_BalustradeFencePanelPole,
0x740f4d5d, 0x1923, 0x484f, 0xfc, 0x5b, 0x36, 0x9b, 0x44, 0x4b, 0xa8, 0x0a);
DEFINE_GUID(CLSID_BalustradeFenceBraceX,
0x740f4d5d, 0x1923, 0x484f, 0xfc, 0x5b, 0x36, 0x9b, 0x44, 0x4b, 0xa8, 0x0b);

// CObjectBalustrade is a test object to test the system
class DLLEXPORT CObjectBalustrade : public CObjectTemplate {
public:
   CObjectBalustrade (PVOID pParams, POSINFO pInfo);
   ~CObjectBalustrade (void);

   virtual void Delete (void);
   virtual void Render (POBJECTRENDER pr, DWORD dwSubObject);
   virtual CObjectSocket *Clone (void);
   virtual PCMMLNode2 MMLTo (void);
   virtual BOOL MMLFrom (PCMMLNode2 pNode);
   virtual BOOL ControlPointQuery (DWORD dwID, POSCONTROL pInfo);
   virtual BOOL ControlPointSet (DWORD dwID, PCPoint pVal, PCPoint pViewer);
   virtual void ControlPointEnum (PCListFixed plDWORD);
   virtual BOOL DialogQuery (void);
   virtual BOOL DialogShow (DWORD dwSurface, PCEscWindow pWindow, PCPoint pClick);
   virtual BOOL DialogCPQuery (void);
   virtual BOOL DialogCPShow (DWORD dwSurface, PCEscWindow pWindow, PCPoint pClick);
   virtual BOOL MoveReferencePointQuery (DWORD dwIndex, PCPoint pp);
   virtual BOOL MoveReferenceStringQuery (DWORD dwIndex, PWSTR psz, DWORD dwSize, DWORD *pdwNeeded);
   virtual BOOL Message (DWORD dwMessage, PVOID pParam);
   virtual BOOL IntelligentAdjust (BOOL fAct);
   virtual BOOL IntelligentPositionDrag (CWorldSocket *pWorld, POSINTELLIPOSDRAG pInfo,  POSINTELLIPOSRESULT pResult);
   virtual void QueryBoundingBox (PCPoint pCorner1, PCPoint pCorner2, DWORD dwSubObject);


   CBalustrade    m_ds;


   CRenderSurface    m_Renderrs;    // to minimize mallocs

};
typedef CObjectBalustrade *PCObjectBalustrade;



/**********************************************************************************
CObjectStairs */
// {2501DD91-A3B1-4afd-B87C-DE101EF700AC}
DEFINE_GUID(CLSID_Stairs, 
0x2501dd91, 0xa3b1, 0x4afd, 0xb8, 0x7c, 0xde, 0x10, 0x1e, 0xf7, 0x0, 0xac);

// {539DE403-39B9-46c7-B0D2-DDA7C6979F43}
DEFINE_GUID(CLSID_StairsStraight, 
0x539de403, 0x39b9, 0x46c7, 0xb0, 0xd2, 0xdd, 0xa7, 0xc6, 0x97, 0x9f, 0x00);
DEFINE_GUID(CLSID_StairsLandingTurn, 
0x539de403, 0x39b9, 0x46c7, 0xb0, 0xd2, 0xdd, 0xa7, 0xc6, 0x97, 0x9f, 0x01);
DEFINE_GUID(CLSID_StairsStairwell, 
0x539de403, 0x39b9, 0x46c7, 0xb0, 0xd2, 0xdd, 0xa7, 0xc6, 0x97, 0x9f, 0x02);
DEFINE_GUID(CLSID_StairsStairwell2, 
0x539de403, 0x39b9, 0x46c7, 0xb0, 0xd2, 0xdd, 0xa7, 0xc6, 0x97, 0x9f, 0x03);
DEFINE_GUID(CLSID_StairsSpiral, 
0x539de403, 0x39b9, 0x46c7, 0xb0, 0xd2, 0xdd, 0xa7, 0xc6, 0x97, 0x9f, 0x04);
DEFINE_GUID(CLSID_StairsWinding, 
0x539de403, 0x39b9, 0x46c7, 0xb0, 0xd2, 0xdd, 0xa7, 0xc6, 0x97, 0x9f, 0x05);
DEFINE_GUID(CLSID_StairsEntry, 
0x539de403, 0x39b9, 0x46c7, 0xb0, 0xd2, 0xdd, 0xa7, 0xc6, 0x97, 0x9f, 0x06);

// path types
#define SPATH_STRAIGHT        0     // straight up
#define SPATH_LANDINGTURN     1     // up, turn right/left, and up some more
#define SPATH_STAIRWELL       2     // switches back and forth
#define SPATH_STAIRWELL2      3     // stairscase inside (or outside) a rectangle
#define SPATH_SPIRAL          4     // spiral staircase
#define SPATH_WINDING         5     // just to show that can do winding
#define SPATH_CUSTOMCENTER    6     // custom path with only the center controled
#define SPATH_CUSTOMLR        7     // custom path with left/right controlled

class CColumn;
// CObjectStairs is a test object to test the system
class DLLEXPORT CObjectStairs : public CObjectTemplate {
public:
   CObjectStairs (PVOID pParams, POSINFO pInfo);
   ~CObjectStairs (void);

   virtual void Delete (void);
   virtual void Render (POBJECTRENDER pr, DWORD dwSubObject);
   virtual CObjectSocket *Clone (void);
   virtual PCMMLNode2 MMLTo (void);
   virtual BOOL MMLFrom (PCMMLNode2 pNode);
   virtual BOOL ControlPointQuery (DWORD dwID, POSCONTROL pInfo);
   virtual BOOL ControlPointSet (DWORD dwID, PCPoint pVal, PCPoint pViewer);
   virtual void ControlPointEnum (PCListFixed plDWORD);
   virtual BOOL DialogQuery (void);
   virtual BOOL DialogShow (DWORD dwSurface, PCEscWindow pWindow, PCPoint pClick);
   virtual BOOL MoveReferencePointQuery (DWORD dwIndex, PCPoint pp);
   virtual BOOL MoveReferenceStringQuery (DWORD dwIndex, PWSTR psz, DWORD dwSize, DWORD *pdwNeeded);
   virtual BOOL Deconstruct (BOOL fAct);
   virtual void QueryBoundingBox (PCPoint pCorner1, PCPoint pCorner2, DWORD dwSubObject);

   void CalcLRC (void);
   void CalcTread (BOOL fRedoAll = TRUE);
   void CalcLRFromPathType (DWORD dwType);
   CObjectStairs *CloneStairs (void);
   void CreateWalls (void);
   void CreateRails (void);

   PCSpline    m_psCenter;  // center spline
   PCSpline    m_psLeft;    // left spline, when walking up
   PCSpline    m_psRight;   // right spline, when walking up
   BOOL        m_fLRAuto;  // if TRUE then the left/right are automatically calculated by center,
                           // else, center is automatically calculated by left and right
   fp      m_fWidth;   // width of the staircase (used if m_fLRAuto)
   DWORD       m_dwPathType;  // type of path to use. See SPATH_XXX
   CListFixed  m_lTREADINFO;  // list of tread info structures
   CPoint      m_apBottomKick[2][2];  // bottom kickboard info
   CColumn     *m_pColumn; // column used for spiral staircase
   fp      m_fTotalRise;  // total rise in the staircase - used by some stairs
   fp      m_fLength;  // lenght of staircase - used by some stairs
   fp      m_fRestLanding;   // length of the rest landing, half way up some stairs
   BOOL        m_fPathClockwise; // if TRUE, path will turn clockwise, FALSE counter. Used by some SPATH_XXX
   fp      m_fStairwellWidth;   // width (X direction) of stairwell
   fp      m_fStairwellLength;  // length (Y direction) of stairwell
   fp      m_fStairwellLanding; // width of landing (on left/right in X direction)
   DWORD       m_dwRiserLevels;     // number of levels to rise
   fp      m_fLevelHeight;      // number of meters in a level
   BOOL        m_fStairwellTwoPerLevel;   // if TRUE, two landings per level in stairwell, else 1
   fp      m_fSpiralRadius;     // external radius (use width to determine internal)
   fp      m_fSpiralRevolutions;   // number of revolutions. 1.0 = 360 degrees
   fp      m_fLandingTop;       // length of the landing at the top
   int         m_iLandingTopDir;    // direction. 0 for straight, -1 for left, 1 for right
   BOOL        m_fSpiralPost;       // post in the center of sprial staircase
   fp      m_fSpiralPostDiameter;  // diameter of the post
   fp      m_fSpiralPostHeight; // height of post above top step

   fp      m_fTreadThickness;   // thickness of tread
   fp      m_fRiser;   // riser height (height from one step to next
   fp      m_fTreadExtendLeft;  // amount to extend to left
   fp      m_fTreadExtendRight; // amount to extend to right
   fp      m_fTreadExtendBack;  // amount to extend back
   fp      m_fTreadMinDepth;    // minimum depth
   fp      m_fKickboardIndent;  // indent kickboard this much on top. (goes to bottom of tread below)
   BOOL        m_fKickboard;        // if TRUE draw kickboard
   BOOL        m_fTreadLip;         // set to TRUE if includes a lip (can be used for sleepers in external walkway)
   fp      m_fTreadLipThickness;   // thickness of tread lip (height)
   fp      m_fTreadLipDepth;    // depth of tread lip
   fp      m_fTreadLipOffset;   // vertical offset compared to top of tread. + mean higher

   CBalustrade *m_apBal[3];         // [0]=left balustrade,[1]=middle balustade,[2]=right balustrade
   BOOL        m_afBalWant[3];      // Set to TRUE if want the balustrades

   CDoubleSurface *m_apWall[3];     // [0]=left wall, [1] = middle, [2] = right
   BOOL        m_afWallWant[3];     // Set to TRUE if want the wals
   fp      m_fWallIndent;       // indent L/R walls by this much
   BOOL        m_fWallToGround;     // set to TRUE if wall should go to ground
   fp      m_fWallBelow;        // amount wall should go below risers

   CNoodle     *m_apRail[3];        // [0]=keft rail, [1]=middle,[2]=right
   BOOL        m_afRailWant[3];     // if want rails
   DWORD       m_adwRailShape[3];   // noodle shape
   TEXTUREPOINT m_pRailSize;        // .h=width and .v=height
   TEXTUREPOINT m_pRailOffset;      // .h=indent, .v=up/down
   BOOL        m_fRailExtend;       // set to true if extend to the ground and top of stairs


   CRenderSurface    m_Renderrs;       // for malloc optimization
};
typedef CObjectStairs *PCObjectStairs;


/**********************************************************************************
CObjectPiers */
// {740F4D5D-1923-484f-AC5B-369B444BB802}
DEFINE_GUID(CLSID_Piers,
0x740f895d, 0x1923, 0x484f, 0xac, 0x5b, 0x36, 0x9b, 0x44, 0x4b, 0xb8, 0x2);

DEFINE_GUID(CLSID_PiersLogStump,
0x740f895d, 0x1921, 0x484f, 0xfc, 0x5b, 0x36, 0x9b, 0x44, 0x4b, 0xb8, 0x00);
DEFINE_GUID(CLSID_PiersSteel,
0x740f895d, 0x1924, 0x484f, 0xfc, 0x5b, 0x36, 0x9b, 0x44, 0x4b, 0xb8, 0x01);
DEFINE_GUID(CLSID_PiersWood,
0x740f895d, 0x1926, 0x484f, 0xfc, 0x5b, 0x36, 0x9b, 0x44, 0x4b, 0xb8, 0x02);
DEFINE_GUID(CLSID_PiersCementSquare,
0x740f895d, 0x1927, 0x484f, 0xfc, 0x5b, 0x36, 0x9b, 0x44, 0x4b, 0xb8, 0x03);
DEFINE_GUID(CLSID_PiersCementRound,
0x740f895d, 0x1922, 0x484f, 0xfc, 0x5b, 0x36, 0x9b, 0x44, 0x4b, 0xb8, 0x04);
DEFINE_GUID(CLSID_PiersGreek,
0x740f895d, 0x1928, 0x484f, 0xfc, 0x5b, 0x36, 0x9b, 0x44, 0x4b, 0xb8, 0x05);

// CObjectPiers is a test object to test the system
class DLLEXPORT CObjectPiers : public CObjectTemplate {
public:
   CObjectPiers (PVOID pParams, POSINFO pInfo);
   ~CObjectPiers (void);

   virtual void Delete (void);
   virtual void Render (POBJECTRENDER pr, DWORD dwSubObject);
   virtual CObjectSocket *Clone (void);
   virtual PCMMLNode2 MMLTo (void);
   virtual BOOL MMLFrom (PCMMLNode2 pNode);
   virtual BOOL ControlPointQuery (DWORD dwID, POSCONTROL pInfo);
   virtual BOOL ControlPointSet (DWORD dwID, PCPoint pVal, PCPoint pViewer);
   virtual void ControlPointEnum (PCListFixed plDWORD);
   virtual BOOL DialogQuery (void);
   virtual BOOL DialogShow (DWORD dwSurface, PCEscWindow pWindow, PCPoint pClick);
   virtual BOOL DialogCPQuery (void);
   virtual BOOL DialogCPShow (DWORD dwSurface, PCEscWindow pWindow, PCPoint pClick);
   virtual BOOL MoveReferencePointQuery (DWORD dwIndex, PCPoint pp);
   virtual BOOL MoveReferenceStringQuery (DWORD dwIndex, PWSTR psz, DWORD dwSize, DWORD *pdwNeeded);
   virtual BOOL Message (DWORD dwMessage, PVOID pParam);
   virtual BOOL IntelligentAdjust (BOOL fAct);
   virtual BOOL Deconstruct (BOOL fAct);
   virtual void QueryBoundingBox (PCPoint pCorner1, PCPoint pCorner2, DWORD dwSubObject);


   CPiers    m_ds;

   CRenderSurface    m_Renderrs;    // to minimize mallocs


};
typedef CObjectPiers *PCObjectPiers;


/**********************************************************************************
CObjectBuildBlock */
// {7CAA2E10-78DB-4b70-9A84-CD384991C5DA}
DEFINE_GUID(CLSID_BuildBlock, 
0x7caa2e10, 0x78db, 0x4b70, 0x9a, 0x84, 0xcd, 0x38, 0x49, 0x91, 0xc5, 0xda);

// {0E73C64E-4BD8-4d45-891B-12ADCDA60045}
DEFINE_GUID(CLSID_BuildBlockBuildHipHalf, 
0xe73c64e, 0x4bd8, 0x4d45, 0x89, 0x1b, 0x12, 0xad, 0xcd, 0xa6, 0x0, 0x00);
DEFINE_GUID(CLSID_BuildBlockBuildHipPeak, 
0xe73c64e, 0x4bd8, 0x4d45, 0x89, 0x1b, 0x12, 0xad, 0xcd, 0xa6, 0x0, 0x01);
DEFINE_GUID(CLSID_BuildBlockBuildHipRidge, 
0xe73c64e, 0x4bd8, 0x4d45, 0x89, 0x1b, 0x12, 0xad, 0xcd, 0xa6, 0x0, 0x02);
DEFINE_GUID(CLSID_BuildBlockBuildOutback1,
0xe73c64e, 0x4bd8, 0x4d45, 0x89, 0x1b, 0x12, 0xad, 0xcd, 0xa6, 0x0, 0x03);
DEFINE_GUID(CLSID_BuildBlockBuildSaltBox, 
0xe73c64e, 0x4bd8, 0x4d45, 0x89, 0x1b, 0x12, 0xad, 0xcd, 0xa6, 0x0, 0x04);
DEFINE_GUID(CLSID_BuildBlockBuildGableThree, 
0xe73c64e, 0x4bd8, 0x4d45, 0x89, 0x1b, 0x12, 0xad, 0xcd, 0xa6, 0x0, 0x05);
DEFINE_GUID(CLSID_BuildBlockBuildShed, 
0xe73c64e, 0x4bd8, 0x4d45, 0x89, 0x1b, 0x12, 0xad, 0xcd, 0xa6, 0x0, 0x06);
DEFINE_GUID(CLSID_BuildBlockBuildGableFour, 
0xe73c64e, 0x4bd8, 0x4d45, 0x89, 0x1b, 0x12, 0xad, 0xcd, 0xa6, 0x0, 0x07);
DEFINE_GUID(CLSID_BuildBlockBuildShedFolded, 
0xe73c64e, 0x4bd8, 0x4d45, 0x89, 0x1b, 0x12, 0xad, 0xcd, 0xa6, 0x0, 0x08);
DEFINE_GUID(CLSID_BuildBlockBuildHipShedPeak, 
0xe73c64e, 0x4bd8, 0x4d45, 0x89, 0x1b, 0x12, 0xad, 0xcd, 0xa6, 0x0, 0x09);
DEFINE_GUID(CLSID_BuildBlockBuildHipShedRidge, 
0xe73c64e, 0x4bd8, 0x4d45, 0x89, 0x1b, 0x12, 0xad, 0xcd, 0xa6, 0x0, 0x10);
DEFINE_GUID(CLSID_BuildBlockBuildMonsard, 
0xe73c64e, 0x4bd8, 0x4d45, 0x89, 0x1b, 0x12, 0xad, 0xcd, 0xa6, 0x0, 0x11);
DEFINE_GUID(CLSID_BuildBlockBuildGambrel, 
0xe73c64e, 0x4bd8, 0x4d45, 0x89, 0x1b, 0x12, 0xad, 0xcd, 0xa6, 0x0, 0x12);
DEFINE_GUID(CLSID_BuildBlockBuildGullwing, 
0xe73c64e, 0x4bd8, 0x4d45, 0x89, 0x1b, 0x12, 0xad, 0xcd, 0xa6, 0x0, 0x13);
DEFINE_GUID(CLSID_BuildBlockBuildOutback2, 
0xe73c64e, 0x4bd8, 0x4d45, 0x89, 0x1b, 0x12, 0xad, 0xcd, 0xa6, 0x0, 0x14);
DEFINE_GUID(CLSID_BuildBlockBuildBalinese, 
0xe73c64e, 0x4bd8, 0x4d45, 0x89, 0x1b, 0x12, 0xad, 0xcd, 0xa6, 0x0, 0x15);
DEFINE_GUID(CLSID_BuildBlockBuildFlat, 
0xe73c64e, 0x4bd8, 0x4d45, 0x89, 0x1b, 0x12, 0xad, 0xcd, 0xa6, 0x0, 0x16);
DEFINE_GUID(CLSID_BuildBlockBuildNone, 
0xe73c64e, 0x4bd8, 0x4d45, 0x89, 0x1b, 0x12, 0xad, 0xcd, 0xa6, 0x0, 0x17);
DEFINE_GUID(CLSID_BuildBlockBuildTrianglePeak, 
0xe73c64e, 0x4bd8, 0x4d45, 0x89, 0x1b, 0x12, 0xad, 0xcd, 0xa6, 0x0, 0x18);
DEFINE_GUID(CLSID_BuildBlockBuildPentagonPeak, 
0xe73c64e, 0x4bd8, 0x4d45, 0x89, 0x1b, 0x12, 0xad, 0xcd, 0xa6, 0x0, 0x19);
DEFINE_GUID(CLSID_BuildBlockBuildHexagonPeak, 
0xe73c64e, 0x4bd8, 0x4d45, 0x89, 0x1b, 0x12, 0xad, 0xcd, 0xa6, 0x0, 0x20);
DEFINE_GUID(CLSID_BuildBlockBuildHalfHipSkew, 
0xe73c64e, 0x4bd8, 0x4d45, 0x89, 0x1b, 0x12, 0xad, 0xcd, 0xa6, 0x0, 0x21);
DEFINE_GUID(CLSID_BuildBlockBuildShedSkew, 
0xe73c64e, 0x4bd8, 0x4d45, 0x89, 0x1b, 0x12, 0xad, 0xcd, 0xa6, 0x0, 0x22);
DEFINE_GUID(CLSID_BuildBlockBuildShedCurved, 
0xe73c64e, 0x4bd8, 0x4d45, 0x89, 0x1b, 0x12, 0xad, 0xcd, 0xa6, 0x0, 0x23);
DEFINE_GUID(CLSID_BuildBlockBuildBalineseCurved, 
0xe73c64e, 0x4bd8, 0x4d45, 0x89, 0x1b, 0x12, 0xad, 0xcd, 0xa6, 0x0, 0x24);
DEFINE_GUID(CLSID_BuildBlockBuildCone, 
0xe73c64e, 0x4bd8, 0x4d45, 0x89, 0x1b, 0x12, 0xad, 0xcd, 0xa6, 0x0, 0x25);
DEFINE_GUID(CLSID_BuildBlockBuildConeCurved, 
0xe73c64e, 0x4bd8, 0x4d45, 0x89, 0x1b, 0x12, 0xad, 0xcd, 0xa6, 0x0, 0x26);
DEFINE_GUID(CLSID_BuildBlockBuildHalfHipLoop, 
0xe73c64e, 0x4bd8, 0x4d45, 0x89, 0x1b, 0x12, 0xad, 0xcd, 0xa6, 0x0, 0x27);
DEFINE_GUID(CLSID_BuildBlockBuildHemisphere, 
0xe73c64e, 0x4bd8, 0x4d45, 0x89, 0x1b, 0x12, 0xad, 0xcd, 0xa6, 0x0, 0x28);
DEFINE_GUID(CLSID_BuildBlockBuildHemisphereHalf, 
0xe73c64e, 0x4bd8, 0x4d45, 0x89, 0x1b, 0x12, 0xad, 0xcd, 0xa6, 0x0, 0x29);
DEFINE_GUID(CLSID_BuildBlockBuildGullWingCurved, 
0xe73c64e, 0x4bd8, 0x4d45, 0x89, 0x1b, 0x12, 0xad, 0xcd, 0xa6, 0x0, 0x30);
DEFINE_GUID(CLSID_BuildBlockBuildHalfHipCurved, 
0xe73c64e, 0x4bd8, 0x4d45, 0x89, 0x1b, 0x12, 0xad, 0xcd, 0xa6, 0x0, 0x31);


// {0E73C64E-4BD8-4d45-891B-12ADCDA60045}
DEFINE_GUID(CLSID_BuildBlockVerandahHipHalf, 
0xe73c64e, 0x4bd8, 0x1234, 0x89, 0x1b, 0x12, 0xad, 0xcd, 0xa6, 0x0, 0x00);
DEFINE_GUID(CLSID_BuildBlockVerandahHipPeak, 
0xe73c64e, 0x4bd8, 0x1234, 0x89, 0x1b, 0x12, 0xad, 0xcd, 0xa6, 0x0, 0x01);
DEFINE_GUID(CLSID_BuildBlockVerandahHipRidge, 
0xe73c64e, 0x4bd8, 0x1234, 0x89, 0x1b, 0x12, 0xad, 0xcd, 0xa6, 0x0, 0x02);
DEFINE_GUID(CLSID_BuildBlockVerandahOutback1,
0xe73c64e, 0x4bd8, 0x1234, 0x89, 0x1b, 0x12, 0xad, 0xcd, 0xa6, 0x0, 0x03);
DEFINE_GUID(CLSID_BuildBlockVerandahSaltBox, 
0xe73c64e, 0x4bd8, 0x1234, 0x89, 0x1b, 0x12, 0xad, 0xcd, 0xa6, 0x0, 0x04);
DEFINE_GUID(CLSID_BuildBlockVerandahGableThree, 
0xe73c64e, 0x4bd8, 0x1234, 0x89, 0x1b, 0x12, 0xad, 0xcd, 0xa6, 0x0, 0x05);
DEFINE_GUID(CLSID_BuildBlockVerandahShed, 
0xe73c64e, 0x4bd8, 0x1234, 0x89, 0x1b, 0x12, 0xad, 0xcd, 0xa6, 0x0, 0x06);
DEFINE_GUID(CLSID_BuildBlockVerandahGableFour, 
0xe73c64e, 0x4bd8, 0x1234, 0x89, 0x1b, 0x12, 0xad, 0xcd, 0xa6, 0x0, 0x07);
DEFINE_GUID(CLSID_BuildBlockVerandahShedFolded, 
0xe73c64e, 0x4bd8, 0x1234, 0x89, 0x1b, 0x12, 0xad, 0xcd, 0xa6, 0x0, 0x08);
DEFINE_GUID(CLSID_BuildBlockVerandahHipShedPeak, 
0xe73c64e, 0x4bd8, 0x1234, 0x89, 0x1b, 0x12, 0xad, 0xcd, 0xa6, 0x0, 0x09);
DEFINE_GUID(CLSID_BuildBlockVerandahHipShedRidge, 
0xe73c64e, 0x4bd8, 0x1234, 0x89, 0x1b, 0x12, 0xad, 0xcd, 0xa6, 0x0, 0x10);
DEFINE_GUID(CLSID_BuildBlockVerandahMonsard, 
0xe73c64e, 0x4bd8, 0x1234, 0x89, 0x1b, 0x12, 0xad, 0xcd, 0xa6, 0x0, 0x11);
DEFINE_GUID(CLSID_BuildBlockVerandahGambrel, 
0xe73c64e, 0x4bd8, 0x1234, 0x89, 0x1b, 0x12, 0xad, 0xcd, 0xa6, 0x0, 0x12);
DEFINE_GUID(CLSID_BuildBlockVerandahGullwing, 
0xe73c64e, 0x4bd8, 0x1234, 0x89, 0x1b, 0x12, 0xad, 0xcd, 0xa6, 0x0, 0x13);
DEFINE_GUID(CLSID_BuildBlockVerandahOutback2, 
0xe73c64e, 0x4bd8, 0x1234, 0x89, 0x1b, 0x12, 0xad, 0xcd, 0xa6, 0x0, 0x14);
DEFINE_GUID(CLSID_BuildBlockVerandahBalinese, 
0xe73c64e, 0x4bd8, 0x1234, 0x89, 0x1b, 0x12, 0xad, 0xcd, 0xa6, 0x0, 0x15);
DEFINE_GUID(CLSID_BuildBlockVerandahFlat, 
0xe73c64e, 0x4bd8, 0x1234, 0x89, 0x1b, 0x12, 0xad, 0xcd, 0xa6, 0x0, 0x16);
DEFINE_GUID(CLSID_BuildBlockVerandahNone, 
0xe73c64e, 0x4bd8, 0x1234, 0x89, 0x1b, 0x12, 0xad, 0xcd, 0xa6, 0x0, 0x17);
DEFINE_GUID(CLSID_BuildBlockVerandahTrianglePeak, 
0xe73c64e, 0x4bd8, 0x1234, 0x89, 0x1b, 0x12, 0xad, 0xcd, 0xa6, 0x0, 0x18);
DEFINE_GUID(CLSID_BuildBlockVerandahPentagonPeak, 
0xe73c64e, 0x4bd8, 0x1234, 0x89, 0x1b, 0x12, 0xad, 0xcd, 0xa6, 0x0, 0x19);
DEFINE_GUID(CLSID_BuildBlockVerandahHexagonPeak, 
0xe73c64e, 0x4bd8, 0x1234, 0x89, 0x1b, 0x12, 0xad, 0xcd, 0xa6, 0x0, 0x20);
DEFINE_GUID(CLSID_BuildBlockVerandahHalfHipSkew, 
0xe73c64e, 0x4bd8, 0x1234, 0x89, 0x1b, 0x12, 0xad, 0xcd, 0xa6, 0x0, 0x21);
DEFINE_GUID(CLSID_BuildBlockVerandahShedSkew, 
0xe73c64e, 0x4bd8, 0x1234, 0x89, 0x1b, 0x12, 0xad, 0xcd, 0xa6, 0x0, 0x22);
DEFINE_GUID(CLSID_BuildBlockVerandahShedCurved, 
0xe73c64e, 0x4bd8, 0x1234, 0x89, 0x1b, 0x12, 0xad, 0xcd, 0xa6, 0x0, 0x23);
DEFINE_GUID(CLSID_BuildBlockVerandahBalineseCurved, 
0xe73c64e, 0x4bd8, 0x1234, 0x89, 0x1b, 0x12, 0xad, 0xcd, 0xa6, 0x0, 0x24);
DEFINE_GUID(CLSID_BuildBlockVerandahCone, 
0xe73c64e, 0x4bd8, 0x1234, 0x89, 0x1b, 0x12, 0xad, 0xcd, 0xa6, 0x0, 0x25);
DEFINE_GUID(CLSID_BuildBlockVerandahConeCurved, 
0xe73c64e, 0x4bd8, 0x1234, 0x89, 0x1b, 0x12, 0xad, 0xcd, 0xa6, 0x0, 0x26);
DEFINE_GUID(CLSID_BuildBlockVerandahHalfHipLoop, 
0xe73c64e, 0x4bd8, 0x1234, 0x89, 0x1b, 0x12, 0xad, 0xcd, 0xa6, 0x0, 0x27);
DEFINE_GUID(CLSID_BuildBlockVerandahHemisphere, 
0xe73c64e, 0x4bd8, 0x1234, 0x89, 0x1b, 0x12, 0xad, 0xcd, 0xa6, 0x0, 0x28);
DEFINE_GUID(CLSID_BuildBlockVerandahHemisphereHalf, 
0xe73c64e, 0x4bd8, 0x1234, 0x89, 0x1b, 0x12, 0xad, 0xcd, 0xa6, 0x0, 0x29);
DEFINE_GUID(CLSID_BuildBlockVerandahGullWingCurved, 
0xe73c64e, 0x4bd8, 0x1234, 0x89, 0x1b, 0x12, 0xad, 0xcd, 0xa6, 0x0, 0x30);
DEFINE_GUID(CLSID_BuildBlockVerandahHalfHipCurved, 
0xe73c64e, 0x4bd8, 0x1234, 0x89, 0x1b, 0x12, 0xad, 0xcd, 0xa6, 0x0, 0x31);

// {EB99428D-9610-4e4b-B5C2-06B594179B57}
DEFINE_GUID(CLSID_BuildBlockDeckRectangle, 
0xeb99428d, 0x9610, 0x4e4b, 0xb5, 0xc2, 0x6, 0xb5, 0x94, 0x17, 0x9b, 0x00);
DEFINE_GUID(CLSID_BuildBlockDeckTriangle, 
0xeb99428d, 0x9610, 0x4e4b, 0xb5, 0xc2, 0x6, 0xb5, 0x94, 0x17, 0x9b, 0x01);
DEFINE_GUID(CLSID_BuildBlockDeckPentagon, 
0xeb99428d, 0x9610, 0x4e4b, 0xb5, 0xc2, 0x6, 0xb5, 0x94, 0x17, 0x9b, 0x02);
DEFINE_GUID(CLSID_BuildBlockDeckHexagon, 
0xeb99428d, 0x9610, 0x4e4b, 0xb5, 0xc2, 0x6, 0xb5, 0x94, 0x17, 0x9b, 0x03);
DEFINE_GUID(CLSID_BuildBlockDeckCircle, 
0xeb99428d, 0x9610, 0x4e4b, 0xb5, 0xc2, 0x6, 0xb5, 0x94, 0x17, 0x9b, 0x04);
DEFINE_GUID(CLSID_BuildBlockDeckCircleHalf, 
0xeb99428d, 0x9610, 0x4e4b, 0xb5, 0xc2, 0x6, 0xb5, 0x94, 0x17, 0x9b, 0x05);

// {E7410AF2-BDE9-485a-BC87-1C813B04D8EF}
DEFINE_GUID(CLSID_BuildBlockWalkwayEnclosedHipHalf, 
0xe7410af2, 0xbde9, 0x485a, 0xbc, 0x87, 0x1c, 0x81, 0x3b, 0x4, 0xd8, 0x00);
DEFINE_GUID(CLSID_BuildBlockWalkwayEnclosedSaltBox, 
0xe7410af2, 0xbde9, 0x485a, 0xbc, 0x87, 0x1c, 0x81, 0x3b, 0x4, 0xd8, 0x01);
DEFINE_GUID(CLSID_BuildBlockWalkwayEnclosedShed, 
0xe7410af2, 0xbde9, 0x485a, 0xbc, 0x87, 0x1c, 0x81, 0x3b, 0x4, 0xd8, 0x02);
DEFINE_GUID(CLSID_BuildBlockWalkwayEnclosedFlat, 
0xe7410af2, 0xbde9, 0x485a, 0xbc, 0x87, 0x1c, 0x81, 0x3b, 0x4, 0xd8, 0x03);
DEFINE_GUID(CLSID_BuildBlockWalkwayEnclosedShedCurved, 
0xe7410af2, 0xbde9, 0x485a, 0xbc, 0x87, 0x1c, 0x81, 0x3b, 0x4, 0xd8, 0x04);
DEFINE_GUID(CLSID_BuildBlockWalkwayEnclosedHalfHipCurved, 
0xe7410af2, 0xbde9, 0x485a, 0xbc, 0x87, 0x1c, 0x81, 0x3b, 0x4, 0xd8, 0x05);

// {E7410AF2-BDE9-485a-BC87-1C813B04D8EF}
DEFINE_GUID(CLSID_BuildBlockWalkwayOpenHipHalf, 
0xe7410af2, 0xbde9, 0x1234, 0xbc, 0x87, 0x1c, 0x81, 0x3b, 0x4, 0xd8, 0x00);
DEFINE_GUID(CLSID_BuildBlockWalkwayOpenSaltBox, 
0xe7410af2, 0xbde9, 0x1234, 0xbc, 0x87, 0x1c, 0x81, 0x3b, 0x4, 0xd8, 0x01);
DEFINE_GUID(CLSID_BuildBlockWalkwayOpenShed, 
0xe7410af2, 0xbde9, 0x1234, 0xbc, 0x87, 0x1c, 0x81, 0x3b, 0x4, 0xd8, 0x02);
DEFINE_GUID(CLSID_BuildBlockWalkwayOpenFlat, 
0xe7410af2, 0xbde9, 0x1234, 0xbc, 0x87, 0x1c, 0x81, 0x3b, 0x4, 0xd8, 0x03);
DEFINE_GUID(CLSID_BuildBlockWalkwayOpenShedCurved, 
0xe7410af2, 0xbde9, 0x1234, 0xbc, 0x87, 0x1c, 0x81, 0x3b, 0x4, 0xd8, 0x04);
DEFINE_GUID(CLSID_BuildBlockWalkwayOpenHalfHipCurved, 
0xe7410af2, 0xbde9, 0x1234, 0xbc, 0x87, 0x1c, 0x81, 0x3b, 0x4, 0xd8, 0x05);
DEFINE_GUID(CLSID_BuildBlockWalkwayOpenNone, 
0xe7410af2, 0xbde9, 0x1234, 0xbc, 0x87, 0x1c, 0x81, 0x3b, 0x4, 0xd8, 0x06);

// {AE27AB2A-443B-4a0e-B1FB-11B07A39E137}
DEFINE_GUID(CLSID_BuildBlockDormerHipHalf, 
0xae27ab2a, 0x443b, 0x4a0e, 0xb1, 0xfb, 0x11, 0xb0, 0x7a, 0x39, 0xe1, 0x00);
DEFINE_GUID(CLSID_BuildBlockDormerShed, 
0xae27ab2a, 0x443b, 0x4a0e, 0xb1, 0xfb, 0x11, 0xb0, 0x7a, 0x39, 0xe1, 0x01);
DEFINE_GUID(CLSID_BuildBlockDormerHalfHipCurved, 
0xae27ab2a, 0x443b, 0x4a0e, 0xb1, 0xfb, 0x11, 0xb0, 0x7a, 0x39, 0xe1, 0x02);
DEFINE_GUID(CLSID_BuildBlockDormerHipShedRidge, 
0xae27ab2a, 0x443b, 0x4a0e, 0xb1, 0xfb, 0x11, 0xb0, 0x7a, 0x39, 0xe1, 0x03);
DEFINE_GUID(CLSID_BuildBlockDormerFlat, 
0xae27ab2a, 0x443b, 0x4a0e, 0xb1, 0xfb, 0x11, 0xb0, 0x7a, 0x39, 0xe1, 0x04);

// {7BC3E464-4B82-47cb-8028-AC1844B8EE56}
DEFINE_GUID(CLSID_BuildBlockBayHipHalf, 
0x7bc3e464, 0x4b82, 0x47cb, 0x80, 0x28, 0xac, 0x18, 0x44, 0xb8, 0xee, 0x00);
DEFINE_GUID(CLSID_BuildBlockBayShed, 
0x7bc3e464, 0x4b82, 0x47cb, 0x80, 0x28, 0xac, 0x18, 0x44, 0xb8, 0xee, 0x01);
DEFINE_GUID(CLSID_BuildBlockBayHalfHipCurved, 
0x7bc3e464, 0x4b82, 0x47cb, 0x80, 0x28, 0xac, 0x18, 0x44, 0xb8, 0xee, 0x02);
DEFINE_GUID(CLSID_BuildBlockBayHipShedRidge, 
0x7bc3e464, 0x4b82, 0x47cb, 0x80, 0x28, 0xac, 0x18, 0x44, 0xb8, 0xee, 0x03);
DEFINE_GUID(CLSID_BuildBlockBayFlat, 
0x7bc3e464, 0x4b82, 0x47cb, 0x80, 0x28, 0xac, 0x18, 0x44, 0xb8, 0xee, 0x04);
DEFINE_GUID(CLSID_BuildBlockBayNone, 
0x7bc3e464, 0x4b82, 0x47cb, 0x80, 0x28, 0xac, 0x18, 0x44, 0xb8, 0xee, 0x05);
DEFINE_GUID(CLSID_BuildBlockBayHexagonPeak, 
0x7bc3e464, 0x4b82, 0x47cb, 0x80, 0x28, 0xac, 0x18, 0x44, 0xb8, 0xee, 0x06);
DEFINE_GUID(CLSID_BuildBlockBayHemisphereHalf, 
0x7bc3e464, 0x4b82, 0x47cb, 0x80, 0x28, 0xac, 0x18, 0x44, 0xb8, 0xee, 0x07);

// {2254F067-34EF-4870-B092-3F4D35A30A2C}
DEFINE_GUID(CLSID_BuildBlockBayWindowHipHalf, 
0x2254f067, 0x34ef, 0x4870, 0xb0, 0x92, 0x3f, 0x4d, 0x35, 0xa3, 0xa, 0x00);
DEFINE_GUID(CLSID_BuildBlockBayWindowShed, 
0x2254f067, 0x34ef, 0x4870, 0xb0, 0x92, 0x3f, 0x4d, 0x35, 0xa3, 0xa, 0x01);
DEFINE_GUID(CLSID_BuildBlockBayWindowHalfHipCurved, 
0x2254f067, 0x34ef, 0x4870, 0xb0, 0x92, 0x3f, 0x4d, 0x35, 0xa3, 0xa, 0x02);
DEFINE_GUID(CLSID_BuildBlockBayWindowHipShedRidge, 
0x2254f067, 0x34ef, 0x4870, 0xb0, 0x92, 0x3f, 0x4d, 0x35, 0xa3, 0xa, 0x03);
DEFINE_GUID(CLSID_BuildBlockBayWindowFlat, 
0x2254f067, 0x34ef, 0x4870, 0xb0, 0x92, 0x3f, 0x4d, 0x35, 0xa3, 0xa, 0x04);
DEFINE_GUID(CLSID_BuildBlockBayWindowHexagonPeak, 
0x2254f067, 0x34ef, 0x4870, 0xb0, 0x92, 0x3f, 0x4d, 0x35, 0xa3, 0xa, 0x05);
DEFINE_GUID(CLSID_BuildBlockBayWindowHemisphereHalf, 
0x2254f067, 0x34ef, 0x4870, 0xb0, 0x92, 0x3f, 0x4d, 0x35, 0xa3, 0xa, 0x06);

extern PWSTR gpszBuildBlockClip;

#define NUMLEVELS    5
typedef struct {
   WCHAR             szName[64]; // name of the surface
   PCDoubleSurface   pSurface;   // surface
   BOOL              fHidden;    // if TRUE, the surface is hidden (completely clipped)
   DWORD             dwMajor;    // major ID: BBSURFMAJOR_XXX
   DWORD             dwMinor;    // minor ID. Wall number, roof number, etc.
   DWORD             dwFlags;    // flags. Combination of BBSRUFFLAG_XXX
   fp            fHeight;    // height of surface, from 0..fHeight
   fp            fWidth;     // width of surface, from -fWidth/2 to fWidth/2
   DWORD             dwType;     // type of the surface - passed into CDoubleSurface::Init()
   CPoint            pTrans;     // translation of the surface
   fp            fRotX;      // radians should be rotated around, in X
   fp            fRotY;      // raiduans, should be rotated around in Y
   fp            fRotZ;      // radiuans should be rotated around, in Z
   fp            fBevelRight;   // bevel on right side
   fp            fBevelLeft; // bevel on left side
   fp            fBevelTop;  // bevel on top
   fp            fBevelBottom;  // bevel on bottom
   fp            fThickStud;  // thickness of the stud 
   fp            fThickSideA;   // thicknes sof side a
   fp            fThickSideB;   // thicknes sof side b
   BOOL              fHideEdges; // set to true if want edges hidden
   BOOL              fChanged;   // temporary variable to note if the object changed in AdjustAllSurfaces()
   DWORD             dwStretchHInfo; // temporary variable for how things get stretched
                                    // 0 => stretch equally
                                    // 1 => stretch/shrink space on left
                                    // 2 => stretch/shrink space on right
   DWORD             dwStretchVInfo; // Like stretchHinfo, except left=top, right=bottom
                                    // 0 => stretch equally
                                    // 1 => stretch/shrink space on top
                                    // 2 => stretch/shrink space on bottom
   BOOL              fDontIncludeInVolume;   // if TRUE, then when returning the volume
                                    // information, don't include this. Used for floors/walls
                                    // that have no effect on the volume
   BOOL              fKeepClip;     // if TRUE, when clipping, keep whatever texture point in pKeepA and pKeepB
                                    // else use normal keep
   TEXTUREPOINT      pKeepA, pKeepB;// used if fKeepClip
   BOOL              fKeepLargest;  // if true, keeps the largest intersection. Ignores fKeepClip then
} BBSURF, *PBBSURF;


/* BBBALUSTRADE - Information about balustrade */
#define BBBALMAJOR_BALUSTRADE       1
#define BBBALMAJOR_PIERS            2
typedef struct {
   DWORD             dwMajor;    // major ID: BBBALMAJOR_XXX
   DWORD             dwMinor;    // minor ID. Wall number, roof number, etc. Corresponds to ID of floor that created
   BOOL              fTouched;   // set to TRUE if was touched this pass of making balustrades
   PCBalustrade      pBal;       // object (if it's a BBBALMAJOR_BALUSTRADE)
   PCPiers           pPiers;     // object (if it's a BBBALMAJOR_PIERS)
} BBBALUSTRADE, *PBBBALUSTRADE;

// CObjectBuildBlock is a test object to test the system
class DLLEXPORT CObjectBuildBlock : public CObjectTemplate {
public:
   CObjectBuildBlock (PVOID pParams, POSINFO pInfo);
   ~CObjectBuildBlock (void);

   virtual void Delete (void);
   virtual void Render (POBJECTRENDER pr, DWORD dwSubObject);
   virtual CObjectSocket *Clone (void);
   virtual PCMMLNode2 MMLTo (void);
   virtual BOOL MMLFrom (PCMMLNode2 pNode);
   virtual BOOL ControlPointQuery (DWORD dwID, POSCONTROL pInfo);
   virtual BOOL ControlPointSet (DWORD dwID, PCPoint pVal, PCPoint pViewer);
   virtual void ControlPointEnum (PCListFixed plDWORD);
   virtual BOOL DialogQuery (void);
   virtual BOOL DialogShow (DWORD dwSurface, PCEscWindow pWindow, PCPoint pClick);
   virtual BOOL DialogCPQuery (void);
   virtual BOOL DialogCPShow (DWORD dwSurface, PCEscWindow pWindow, PCPoint pClick);
   virtual BOOL MoveReferencePointQuery (DWORD dwIndex, PCPoint pp);
   virtual BOOL MoveReferenceStringQuery (DWORD dwIndex, PWSTR psz, DWORD dwSize, DWORD *pdwNeeded);
   virtual BOOL ContHVQuery (PCPoint pEye, PCPoint pClick, DWORD dwSurface, PTEXTUREPOINT pOld, PTEXTUREPOINT pHV);
   virtual BOOL ContCutout (const GUID *pgEmbed, DWORD dwNum, PCPoint paFront, PCPoint paBack, BOOL fBothSides);
   virtual BOOL ContCutoutToZipper (const GUID *pgEmbed, PCListFixed plistFrontHVXYZ, PCListFixed plistBackHVXYZ);
   virtual BOOL ContMatrixFromHV (DWORD dwSurface, PTEXTUREPOINT pHV, fp fRotation, PCMatrix pm);
   virtual fp ContThickness (DWORD dwSurface, PTEXTUREPOINT pHV);
   virtual DWORD ContSideInfo (DWORD dwSurface, BOOL fOtherSide);
   virtual BOOL Message (DWORD dwMessage, PVOID pParam);
   virtual BOOL IntelligentAdjust (BOOL fAct);
   virtual BOOL Deconstruct (BOOL fAct);
   virtual BOOL ContEmbeddedRenamed (const GUID *pgOld, const GUID *pgNew);
   virtual BOOL ObjectMatrixSet (CMatrix *pObject);
   virtual void WorldSetFinished (void);
   virtual DWORD QuerySubObjects (void);
   virtual void QueryBoundingBox (PCPoint pCorner1, PCPoint pCorner2, DWORD dwSubObject);

   BOOL AdjustHeightToFloor (void);
   void AdjustAllSurfaces (void);
   void VerifyObjectMatrix (void);
   BOOL ToClippingVolume (PCListFixed plBBCV);
   void RoofControlFromObject (PCPoint paConvert, DWORD dwNum);
   void RoofControlToObject (PCPoint paConvert, DWORD dwNum);
   void RoofDefaultControl (DWORD dwType);
   void MakeTheRoof (fp *pfRoofHeight, fp *pfCeilingHeight);
   PBBSURF WishListAddCurved (PBBSURF pSurf, DWORD dwH, DWORD dwV,
      PCPoint papControl, DWORD *padwSegCurveH, DWORD *padwSegCurveV, DWORD dwFlags);
   PBBSURF WishListAddFlat (PBBSURF pSurf, PCPoint pBase1, PCPoint pBase2,
                            PCPoint pTop1, PCPoint pTop2, DWORD dwFlags);
   PBBSURF WishListAdd (PBBSURF pSurf);
   BOOL RoofControlPointQuery (DWORD dwID, POSCONTROL pInfo);
   BOOL RoofControlPointSet (DWORD dwID, PCPoint pVal, PCPoint pViewer);

   void BalustradeReset (void);
   BOOL BalustradeAdd (DWORD dwMajor, DWORD dwMinor, fp fHeightTop,
                       fp fHeightBottom, BOOL fWall, PCSplineSurface pss, PCMatrix pm);
   void BalustradeRemoveDead (void);
   void BalustradeExtendToRoof (void);

   void MakeTheWalls (fp fStartAt, fp fRoofHeight, fp fPerimBase);
   void WallDefaultSpline (DWORD dwType, BOOL fWall);
   PCSpline WallSplineAtHeight (fp fHeight, BOOL fWall);
   BOOL WallControlPointQuery (DWORD dwID, POSCONTROL pInfo, BOOL fWall);
   BOOL WallControlPointSet (DWORD dwID, PCPoint pVal, PCPoint pViewer, BOOL fWall);
   BOOL WallCornerAtHeight (DWORD dwCorner, fp fHeight, CPoint *pPoint, BOOL fWall);
   void ConvertRoofPoints (DWORD dwOrig, DWORD dwNew);

   // control points displayed
   DWORD             m_dwDisplayControl;  // 0 for curvature, 1 for edge

   CListFixed        m_listBBSURF;        // list of building block surfaces
   CListFixed        m_listWish;          // wish list for BBSRUF - temporary so dont clone, etc.
   fp            m_fClosestHeight;    // temporary variable that must be initialised before calling wishlisadd

   fp            m_fXMin;             // Center of left X wall
   fp            m_fXMax;             // Center of right X wall
   fp            m_fYMin;             // Center of front Y wall
   fp            m_fYMax;             // Center of back Y wall
   fp            m_fHeight;           // Height of roof above sideA of first floor that
                                          // house starts on (excluding basement)
   fp            m_fRoofOverhang1;    // overhang 1, meaning depends on type of building block
   fp            m_fRoofOverhang2;    // overhang 2, meaning depends on type of building block
   DWORD         m_dwRoofHeight;      // what the roof height is relative to:
                                      // 0 => average of width and height
                                      // 1 = width
                                      // 2 = height
                                      // 3 = constant (in meters)

   fp            m_fOldXMin;          // Old Center of left X wall
   fp            m_fOldXMax;          // Old Center of right X wall
   fp            m_fOldYMin;          // Old Center of front Y wall
   fp            m_fOldYMax;          // Old Center of back Y wall
   fp            m_fOldHeight;        // Old Height of roof above sideA of first floor that

   fp            m_fRoofThickStruct;  // thickness of the structure
   fp            m_fRoofThickSideA;   // thickness of side A
   fp            m_fRoofThickSideB;   // thickness of side B
   fp            m_fWallThickStruct;  // thickness of the structure
   fp            m_fWallThickSideA;   // thickness of side A
   fp            m_fWallThickSideB;   // thickness of side B
   fp            m_fFloorThickStruct;  // thickness of the structure
   fp            m_fFloorThickSideA;   // thickness of side A
   fp            m_fFloorThickSideB;   // thickness of side B
   fp            m_fBasementThickStruct;  // thickness of the structure
   fp            m_fBasementThickSideA;   // thickness of side A
   fp            m_fBasementThickSideB;   // thickness of side B
   fp            m_fPadThickStruct;  // thickness of the structure
   fp            m_fPadThickSideA;   // thickness of side A
   fp            m_fPadThickSideB;   // thickness of side B
   fp            m_fCeilingThickStruct;  // thickness of the structure
   fp            m_fCeilingThickSideA;   // thickness of side A
   fp            m_fCeilingThickSideB;   // thickness of side B

   DWORD             m_dwFoundation;      // foundation. 0 for none, 1 for piers, 2 for pad,
                                          // 3 for perimeter, 4 for basement
   fp            m_fPerimDepth;       // perimeter depth
   BOOL              m_fFoundWall;        // if TRUE, use the normal walls as the foundation
                                          // walls.
   BOOL              m_fCathedral;        // if TRUE, use a cathedral ceiling
   BOOL              m_fFloorsIntoRoof;   // if TRUE, floors can ascend into the roof

   BOOL              m_fKeepFloor;           // if TRUE, this object keeps its floors when intersecting
   BOOL              m_fKeepWalls;           // if TRUE, this object keeps its walls when intersecting
   BOOL              m_fRestrictHeight;      // if TRUE restrict height to levels
   BOOL              m_fRestrictElevation;   // if TRUE, restrict elevation to levels, else can be any, but only one floor
   BOOL              m_fLevelGlobal;      // if TRUE, use global settings for levels
   fp            m_fLevelElevation[NUMLEVELS]; // elevation at each elevation
   DWORD             m_dwGroundFloor;     // what floor index (into m_fLevelElevation) was selected as the ground floor
   fp            m_fLevelHigher;      // at higher levels increment by this much
   fp            m_fLevelMinDist;     // minimum distance between level and ceiling

   CListFixed        m_lRoofControl;      // list of roof control points, initialzed to CPoint.
                                          // meaning depends upon type of roof
   DWORD             m_dwRoofType;        // type of roof
   DWORD             m_dwRoofControlFreedom; // freedom of motion allowed for roof control poonts, 0=limited,1=some,2=max

   DWORD             m_dwWallType;        // shape of wall. WALLSHAPE_XXX
   CSpline           m_sWall;             // wall spline. In coordinates from 0..1, 0..1,0. At height=1/2xm_fHeight
   CListFixed        m_lWallAngle;        // fp. angle of wall. 0 straight up and down. Positive angled out

   DWORD             m_dwVerandahType;        // shape of wall. WALLSHAPE_XXX
   CSpline           m_sVerandah;             // wall spline. In coordinates from 0..1, 0..1,0. At height=1/2xm_fHeight
   CListFixed        m_lVerandahAngle;        // fp. angle of wall. 0 straight up and down. Positive angled out
   BOOL              m_fVerandahEveryFloor;  // if true then verandah on every floor, else only ground floor
   DWORD             m_dwBalEveryFloor;   // if 0 every floor, 1 all except ground, 2 none
   fp            m_fRoofHeight;          // last-used roof height. used to create verandah clipping area

   CListFixed        m_lBBBALUSTRADE;      // list of BBBALUSTRADE
private:
   void IntegrityCheck (void);
   CPoint            m_pOldOMSPoint;      // temporary variables used in objectmatrixset

   CRenderSurface    m_Renderrs;       // for malloc optimization

};
typedef CObjectBuildBlock *PCObjectBuildBlock;



/**********************************************************************************
CObjectGround */
// {986D633D-271D-47b9-AED8-346813F7386B}
DEFINE_GUID(CLSID_Ground, 
0x986d633d, 0x271d, 0x47b9, 0xae, 0xd8, 0x34, 0x68, 0x13, 0xf7, 0x38, 0x6b);

#define GROUNDTHREADS          (MAXRAYTHREAD)          // since changed way code works
// #define  GROUNDTHREADS        (MAXRAYTHREAD*2-1)     // number of threads potentially used to draw ground
                                 // make sure a prime number for randomization purposes
// RENDERSURFACEPLUS - RENDERSURFACE plus actual texture info
typedef struct {
   RENDERSURFACE        rs;               // render surface
   PCTextureMapSocket   pTexture;   // texture (if known)
   QWORD                qwTextureCacheTimeStamp;      // time stamp from texture get
} RENDERSURFACEPLUS, *PRENDERSURFACEPLUS;


// CObjectGround is a test object to test the system
class DLLEXPORT CObjectGround : public CObjectTemplate {
public:
   CObjectGround (PVOID pParams, POSINFO pInfo);
   ~CObjectGround (void);

   virtual void Delete (void);
   virtual void Render (POBJECTRENDER pr, DWORD dwSubObject);
   virtual CObjectSocket *Clone (void);
   virtual PCMMLNode2 MMLTo (void);
   virtual BOOL MMLFrom (PCMMLNode2 pNode);
   virtual BOOL DialogQuery (void);
   virtual BOOL DialogShow (DWORD dwSurface, PCEscWindow pWindow, PCPoint pClick);
   //virtual BOOL ControlPointQuery (DWORD dwID, POSCONTROL pInfo);
   //virtual BOOL ControlPointSet (DWORD dwID, PCPoint pVal, PCPoint pViewer);
   //virtual void ControlPointEnum (PCListFixed plDWORD);
   //virtual BOOL DialogCPQuery (void);
   //virtual BOOL DialogCPShow (DWORD dwSurface, PCEscWindow pWindow);
   virtual BOOL MoveReferencePointQuery (DWORD dwIndex, PCPoint pp);
   virtual BOOL MoveReferenceStringQuery (DWORD dwIndex, PWSTR psz, DWORD dwSize, DWORD *pdwNeeded);
   virtual BOOL ContHVQuery (PCPoint pEye, PCPoint pClick, DWORD dwSurface, PTEXTUREPOINT pOld, PTEXTUREPOINT pHV);
   virtual BOOL ContCutout (const GUID *pgEmbed, DWORD dwNum, PCPoint paFront, PCPoint paBack, BOOL fBothSides);
   virtual BOOL ContCutoutToZipper (const GUID *pgEmbed, PCListFixed plistFrontHVXYZ, PCListFixed plistBackHVXYZ);
   virtual BOOL ContMatrixFromHV (DWORD dwSurface, PTEXTUREPOINT pHV, fp fRotation, PCMatrix pm);
   virtual fp ContThickness (DWORD dwSurface, PTEXTUREPOINT pHV);
   virtual DWORD ContSideInfo (DWORD dwSurface, BOOL fOtherSide);
   virtual BOOL ContEmbeddedRenamed (const GUID *pgOld, const GUID *pgNew);
   virtual BOOL Message (DWORD dwMessage, PVOID pParam);
   virtual BOOL TextureQuery (PCListFixed plText);
   virtual BOOL ColorQuery (PCListFixed plColor);
   virtual BOOL ObjectClassQuery (PCListFixed plObj);
   virtual void QueryBoundingBox (PCPoint pCorner1, PCPoint pCorner2, DWORD dwSubObject);
   virtual DWORD QuerySubObjects (void);
   virtual BOOL EditorCreate (BOOL fAct);
   virtual BOOL EditorDestroy (void);
   virtual BOOL EditorShowWindow (BOOL fShow);
   virtual BOOL Deconstruct (BOOL fAct);



   void CalcNormals (void);
   void CalcBoundingBoxes (void);
   void Elevation (DWORD dwX, DWORD dwY, PCPoint pElev);
   void XYToLoc (fp fX, fp fY, PCPoint pElev);
   BOOL IntersectLine (PCPoint pStart, PCPoint pDir, BOOL fBehind,
      fp *pfAlpha = NULL, PCPoint pInter = NULL, PTEXTUREPOINT pText = NULL);
   void HVToPoint (fp fH, fp fV, PCPoint pLoc);
   void CalcOGGrid (void);
   void FillInRENDERSURFACE (void);
   void RenderTrees (DWORD dwXMin, DWORD dwXMax, DWORD dwYMin, DWORD dwYMax,
                                 POBJECTRENDER pr, fp fDetail, BOOL fForestBoxes, PCPoint pCamera,
                                 DWORD dwSubRendering);
   void RenderSection (DWORD dwXMin, DWORD dwXMax, DWORD dwYMin, DWORD dwYMax,
                       POBJECTRENDER pr, fp fDetail, COLORREF cGround, RENDERSURFACE *prsGround);

   // settings
   DWORD             m_dwWidth;        // # of points across (EW)
   DWORD             m_dwHeight;       // # of points up/down (NS)
   TEXTUREPOINT      m_tpElev;         // .h is minimum elevation (at 0), .v is max elevation (at 0xffff)
   WORD              *m_pawElev;        // pointer to elevation information. [x + m_dwWidth * y]
   CMem              m_memElev;        // memory of m_dwWidth x m_dwHeight x sizeof(WORD)
   fp                m_fScale;         // number of meters per square. total width = (m_dwWidth-1)*m_fScale. Likewise for height
   CListFixed        m_lOGCUTOUT;      // list of cutouts
   BOOL              m_fLessDetail;    // if TRUE then draw gound with less detail when working on it
   BOOL              m_fForestBoxes;   // if TRUE then draw fores trees as boxes
   BOOL              m_fSaveCompressed;   // if TRUE then save compressed (lossy)f
   BOOL              m_fDontDrawNearTrees;   // if TRUE, dont draw forest trees that intersect with camera

   // textures
   CMem              m_memTextSet;     // memory of m_lTextPCObjectSurface.NUm() x m_dwWidth x m_dwHeight x sizeof(BYTE)
   BYTE              *m_pabTextSet;    // pointer to texture index information. [x + m_dwWidth * y + dwTextNum * m_dwWidth * m_dwHeight]
   CListFixed        m_lTextPCObjectSurface; // list of PCObjectSurface's for the texture
   CListFixed        m_lTextCOLORREF;  // list of colors to display the m_lTextPCObjectSurface as when editing

   // forest
   CMem              m_memForestSet;   // memory of m_lPCForest.Num() x m_dwWidth x m_dwHeight x sizeof(BYTE)
   BYTE              *m_pabForestSet;  // pointer to forest index inforamtion. [x + m_dwWidth * y + dwForestNum * m_dwWidth * m_dwHeight]
   CListFixed        m_lPCForest;      // list of PCForest objects. Must always have at least 1

   // water
   BOOL              m_fWater;         // if TRUE, diplay water
   fp                m_fWaterElevation;   // water elevation, in meters
   fp                m_fWaterSize;     // how large to draw water, in meters, if 0 then size of ground
   PCObjectSurface   m_apWaterSurf[GWATERNUM*GWATEROVERLAP]; // water surface
   fp                m_afWaterElev[GWATERNUM]; // water elevation at each surface


   // calculated
   BOOL              m_fNormalsDirty;  // set to true if the normals are dirty and need recalc
   CMem              m_memNormals;      // normals for surface. m_dwWidth x m_dwHeight x sizeof(CPoint).
   BOOL              m_fBBDirty;       // set to true if the bounding boxes are dirty
   CListFixed        m_lOGGRID;        // used to store exact information about cutouts
   CListFixed        m_alRENDERSURFACEPLUS[GROUNDTHREADS]; // one rendersurface per m_lTextPCObjectSurface
   GUID              m_gTextGround;    // guid used for run-time texture
   GUID              m_gTextWater;     // guid used for run-time texture
   RENDERSURFACEPLUS  m_aWaterRS[GROUNDTHREADS][GWATERNUM*GWATEROVERLAP];  // rendersurface for the water
   CListFixed        m_lGROUNDSEC;     // list of ground secionts
   CRITICAL_SECTION  m_aCritSec[GROUNDTHREADS];     // critical section to block access

private:
   BOOL CompressGrid (RECT *pr, PCMem pMem);
   size_t DeCompressGrid (RECT *pr, PVOID pComp, size_t dwSize);
   BOOL CompressElevation (PCMem pComp);
   BOOL DeCompressElevation (PBYTE pComp, size_t dwSize);
   BOOL CompressBytes (PBYTE pComp, size_t dwNum, PCMem pMem);
   BOOL DeCompressBytes (PBYTE pComp, size_t dwSize, PCMem pMem);

   CListFixed        m_lGroundRender;  // optimize mallocs
   CRenderSurface    m_lRenderrs;      // optimize mallocs
   CRenderSurface    m_lRenderSectionrs;      // optimize mallocs
   CRenderSurface    m_lRenderTreesrs;      // optimize mallocs
   CListFixed        m_lRenderTreesCloneMatrix; // optimize mallocs
};
typedef CObjectGround *PCObjectGround;

DLLEXPORT BOOL CutoutBasedOnSpline (PCDoubleSurface pSurf, PCSpline pSpline, BOOL fCutout);

/**********************************************************************************
CObjectTeapot */
// {722D3D0D-8F4B-4a82-B0D3-83D601511755}
DEFINE_GUID(CLSID_TEAPOT, 
0x722d3d0d, 0x8f4b, 0x4a82, 0xb0, 0xd3, 0x83, 0xd6, 0x1, 0x51, 0x17, 0x55);
// CObjectTeapot is a test object to test the system
class DLLEXPORT CObjectTeapot : public CObjectTemplate {
public:
   CObjectTeapot (PVOID pParams, POSINFO pInfo);
   ~CObjectTeapot (void);

   virtual void Delete (void);
   virtual void Render (POBJECTRENDER pr, DWORD dwSubObject);
   virtual CObjectSocket *Clone (void);
   virtual PCMMLNode2 MMLTo (void);
   virtual BOOL MMLFrom (PCMMLNode2 pNode);

   CRenderSurface    m_Renderrs;    // to minimize mallocs
};
typedef CObjectTeapot *PCObjectTeapot;



/**********************************************************************************
CTooth */
class CNoodle;
typedef CNoodle *PCNoodle;

class DLLEXPORT CTooth {
public:
   ESCNEWDELETE;

   CTooth (void);
   ~CTooth (void);

   PCMMLNode2 MMLTo (void);
   BOOL MMLFrom (PCMMLNode2 pNode);
   CTooth *Clone (void);
   BOOL CloneTo (CTooth *pTo);
   BOOL Render (POBJECTRENDER pr, PCRenderSurface prs);
   void QueryBoundingBox (PCPoint pCorner1, PCPoint pCorner2);

   void Mirror (void);
   BOOL Dialog (PCObjectTemplate pTemp, CTooth *pMirror, PCEscWindow pWindow);
   void Scale (PCPoint pScale);
   fp IsMirror (CTooth *pTooth);

   // user set - if any of these are changed then m_fDirty should be set to TRUE
   fp                m_fLength;     // length in meters
   fp                m_fCurvature;  // curvature, in radians
   fp                m_fCurveLinear;   // how linear curve is. 50 = linear, 0 to 100
   fp                m_fProfileLinear; // how linear profile is. 50 = linear, 0 to 100
   TEXTUREPOINT      m_tpRoot;      // width and depth at root
   TEXTUREPOINT      m_tpTip;       // width and depth at tip
   CPoint            m_pProfileRoot;   // profile at root, from 0 to 100, 50= circle
                                    // p[0]=x front, p[1]=xback, p[2]=y right, p[3]=y left
   CPoint            m_pProfileTip; // profile at tip info
   DWORD             m_dwTextWrapProf; // number of times the texture wraps around the profile, or 0 if dosn't wrap
   DWORD             m_dwTextWrapLength;  // number of times the texture repeats along the length, or 0 if doesn't repeat
   DWORD             m_dwUserDetail;   // user detail setting, 0=min, 2 = max

   // calculated
   BOOL              m_fDirty;      // set to TRUE if noodle needs to be reset

private:
   BOOL CalcIfNecessary (DWORD dwWant);

   DWORD             m_dwDetail;    // detail of redering, 0 is minimum, 2 is best
   PCNoodle          m_pNoodle;      // for drawing
};
typedef CTooth *PCTooth;




/**********************************************************************************
CObjectJaw */
#define NUMJAWCP        5     // number of CP in a jaw

typedef struct {
   fp          fLoc;       // location along spline, 0 = back of jaw on left side,
                           // 0.5 is middle of jaw, 1 = back of jaw on right side
   CMatrix     mTransRot;  // additional translation and rotation matrix applied to the tooth
   PCTooth     pTooth;     // tooth
} JAWTOOTH, *PJAWTOOTH;

// {722D3D0D-8F4B-4a82-B0D3-83D601511755}
DEFINE_GUID(CLSID_Jaw, 
0x1f643d0d, 0x8129, 0x164a, 0xb0, 0xd3, 0x83, 0xd6, 0x1, 0x51, 0x17, 0xc4);
// CObjectJaw is a test object to test the system
class DLLEXPORT CObjectJaw : public CObjectTemplate {
public:
   CObjectJaw (PVOID pParams, POSINFO pInfo);
   ~CObjectJaw (void);

   virtual void Delete (void);
   virtual void Render (POBJECTRENDER pr, DWORD dwSubObject);
   virtual CObjectSocket *Clone (void);
   virtual PCMMLNode2 MMLTo (void);
   virtual BOOL MMLFrom (PCMMLNode2 pNode);
   virtual BOOL MoveReferencePointQuery (DWORD dwIndex, PCPoint pp);
   virtual BOOL MoveReferenceStringQuery (DWORD dwIndex, PWSTR psz, DWORD dwSize, DWORD *pdwNeeded);
   virtual BOOL DialogQuery (void);
   virtual BOOL DialogShow (DWORD dwSurface, PCEscWindow pWindow, PCPoint pClick);
   virtual BOOL ControlPointQuery (DWORD dwID, POSCONTROL pInfo);
   virtual BOOL ControlPointSet (DWORD dwID, PCPoint pVal, PCPoint pViewer);
   virtual void ControlPointEnum (PCListFixed plDWORD);
   virtual BOOL AttribGetIntern (PWSTR pszName, fp *pfValue);
   virtual void AttribGetAllIntern (PCListFixed plATTRIBVAL);
   virtual void AttribSetGroupIntern (DWORD dwNum, PATTRIBVAL paAttrib);
   virtual BOOL AttribInfoIntern (PWSTR pszName, PATTRIBINFO pInfo);
   virtual void QueryBoundingBox (PCPoint pCorner1, PCPoint pCorner2, DWORD dwSubObject);

//private:
   BOOL CalcRenderIfNecessary (POBJECTRENDER pr);
   void SplineLocToPoint (fp fLoc, PCPoint pLoc);
   void SplineLocToMatrix (fp fLoc, PCMatrix pMatrix);
   DWORD FindMirror (DWORD dwTooth);

   BOOL           m_fLowerJaw;               // set to TRUE if this is the lower jaw, FALSE if upper.
   BOOL           m_fTeethSymmetry;          // if TRUE then when modify teeth impose symmetry
   DWORD          m_dwTeethMissing;          // if bit is set then tooth is missing.
   CPoint         m_apJawSpline[NUMJAWCP];   // array of points that define 1/2 of jaw line.
                                             // on the left side (potive x) going from the back
                                             // of the jaw, [0] to the front [NUMJAWCP-1]
   CPoint         m_apGumSpline[NUMJAWCP][4]; // array of points that define the position of the gum.
                                             // [][0] is the ouside gum CP, furtherst from jaw CP, [][1] is outside gum CP closest to jaw CP.
                                             // [][2] is inside gum, closest to jaw spline, [][3] is inside gum, furthest from jaw spline
                                             // p[0] is horizontal distance from jaw. p[1] is not used. p[2] is vertical distance
   CPoint         m_apRoof[2];               // points that define roof of mouth. [0] is the roof for the
                                             // back of the mouth, [1] is the roof for the front of the mouth
                                             // p[0] on the roof is always 0.
   fp             m_fTeethDeltaZ;            // teeth are always this many meters below the jaw spline
   CListFixed     m_lJAWTOOTH;               // list of tooth information
   DWORD          m_dwUserDetail;            // user detail level for gums and tongue
   BOOL           m_fTongueShow;             // if TRUE then draw tongue
   CPoint         m_pTongueSize;             // size of tongue. p[0] = width, p[1]=length,p[2]=height,p[3]= taper from 0 to 1.0
   CPoint         m_pTongueLimit;            // p[0] = max height (in meters), p[1]= max back (as percent)
   TEXTUREPOINT   m_tpTongueLoc;             // .h = LipSyncMusc:TongueForward, .v = LipSyncMusc:TongueUp

   BOOL           m_fDirty;                  // set to TRUE if dirty
   BOOL           m_fTongueDirty;            // set tp TRUE if tongue dirty
   PCNoodle       m_pTongue;                 // tongue noodle
   CPoint         m_apBound[2];              // [0] = min, [1] = max of jaw
   DWORD          m_dwDetail;                // detail level that calculated in
   CMem           m_memJawPoint;             // memory with jaw render point location
   CMem           m_memJawNorm;              // memory with jaw render norm location
   CMem           m_memJawText;              // memory with jaw texture location
   CMem           m_memJawVert;              // memory with jaw vertex location
   CMem           m_memJawPoly;              // memory with jaw polygon location. Array of 4 DWORDs of vertex number, last one might be -1

   CRenderSurface    m_Renderrs;    // to minimize mallocs
};
typedef CObjectJaw *PCObjectJaw;




/**********************************************************************************
CObjectTooth */
// {722D3D0D-8F4B-4a82-B0D3-83D601511755}
DEFINE_GUID(CLSID_Tooth, 
0x1f633d0d, 0x8f4b, 0x8942, 0xb0, 0xd3, 0x1f, 0xd6, 0x1, 0x51, 0x6c, 0x55);
// CObjectTooth is a test object to test the system
class DLLEXPORT CObjectTooth : public CObjectTemplate {
public:
   CObjectTooth (PVOID pParams, POSINFO pInfo);
   ~CObjectTooth (void);

   virtual void Delete (void);
   virtual void Render (POBJECTRENDER pr, DWORD dwSubObject);
   virtual CObjectSocket *Clone (void);
   virtual PCMMLNode2 MMLTo (void);
   virtual BOOL MMLFrom (PCMMLNode2 pNode);
   virtual BOOL MoveReferencePointQuery (DWORD dwIndex, PCPoint pp);
   virtual BOOL MoveReferenceStringQuery (DWORD dwIndex, PWSTR psz, DWORD dwSize, DWORD *pdwNeeded);
   virtual BOOL DialogQuery (void);
   virtual BOOL DialogShow (DWORD dwSurface, PCEscWindow pWindow, PCPoint pClick);
   virtual void QueryBoundingBox (PCPoint pCorner1, PCPoint pCorner2, DWORD dwSubObject);


   // public
   CTooth         m_Tooth;       // tooth

   CRenderSurface    m_Renderrs;    // to minimize mallocs
};
typedef CObjectTooth *PCObjectTooth;


/**********************************************************************************
CHairLock */
#define MAXHAIRLAYERS      3        // number of hairs tubes within hair
#define MAXRENDERSUB       4        // maximum number of slices hair can be divided into for ray tacing optimization
class DLLEXPORT CHairLock {
public:
   ESCNEWDELETE;

   CHairLock (void);
   ~CHairLock (void);

   BOOL MMLFrom (PCMMLNode2 pNode);
   PCMMLNode2 MMLTo (void);
   CHairLock *Clone (void);
   BOOL CloneTo (CHairLock *pTo);
   void MMLToBinaryMinMax (PCPoint pMin, PCPoint pMax, fp *pfMin, fp *pfMax);
   BOOL MMLToBinary (PCMem pMem, PCPoint pScale, fp fScale, DWORD dwUserInfo);
   DWORD MMLFromBinary (PBYTE pab, DWORD dwSize, PCPoint pScale, fp fScale, DWORD *pdwUserInfo);

   BOOL Render (POBJECTRENDER pr, PCRenderSurface prs, PCObjectTemplate pTemp, DWORD dwSurfaceBase,
      BOOL fBackfaceCull /*= TRUE*/, PCMatrix pmPointToClipSpace, PCMatrix pmClipSpaceToPoint);
   void QueryBoundingBox (PCPoint pCorner1, PCPoint pCorner2,
                        PCMatrix pmPointToClipSpace, PCMatrix pmClipSpaceToPoint);
   BOOL ControlPointQuery (DWORD dwControlBase, BOOL fRoot, BOOL fCanDelete, 
      DWORD dwID, POSCONTROL pInfo);
   BOOL ControlPointSet (PCObjectTemplate pTemp, DWORD dwControlBase, BOOL fRoot, BOOL fCanDelete, 
      DWORD dwID, PCPoint pVal, PCPoint pViewer,
      PCPoint pEllipseCent = NULL, PCPoint pEllipseRad = NULL,
      DWORD *pdwChanged = NULL, DWORD *pdwNode = NULL, PCPoint pChangedVect = NULL, PCPoint pChangedOrig = NULL,
      fp fMoveFalloffDist = 0.001, fp fEndHairWeight = 0.0);
   void ControlPointEnum (DWORD dwControlBase, BOOL fRoot, BOOL fCanDelete, 
      PCListFixed plDWORD);

   BOOL SplineSet (DWORD dwNum, PCPoint paPoint, fp *pafTwist);
   PCPoint SplineGet (DWORD *pdwNum, fp **ppafTwist);
   BOOL ProfileSet (PTEXTUREPOINT patpProfile);
   PTEXTUREPOINT ProfileGet (void);
   BOOL HairLayersSet (DWORD dwHairLayers, DWORD *padwHairLayerRepeat, fp *pafHairLayerScale);
   DWORD HairLayersGet (DWORD **ppadwHairLayerRepeat, fp **ppafHairlayerScale);
   BOOL DiameterSet (fp fDiameter);
   fp DiameterGet (void);
   BOOL VariationSet (fp fVariation, DWORD dwSeed);
   fp VariationGet (DWORD *pdwSeed);
   BOOL LengthSet (fp fLength);
   fp LengthGet (void);
   BOOL RemainderSet (fp fRemainder);
   fp RemainderGet (void);
   BOOL TotalLengthSet (fp fLength, CHairLock *pTemplate = NULL);
   fp TotalLengthGet (void);

   BOOL IKPull (DWORD dwNumPoint, DWORD *padwSplinePoint, PCPoint paPullDir,
      PCPoint pEllipseCent, PCPoint pEllipseRad, fp fEndHairWeight);
   BOOL IKPull (PCPoint pPullFrom, PCPoint pPullDir, fp fFallOffDist,
      PCPoint pEllipseCent, PCPoint pEllipseRad, fp fEndHairWeight);
   BOOL Blend (PCPoint pSubRoot, PCPoint pRoot, DWORD dwNum, CHairLock **papBlend, fp*pafBlend);
   BOOL GravitySimulate (fp fAmount, PCPoint pEllipseCent, PCPoint pEllipseRad);
   BOOL Mirror (DWORD dwDim);
   BOOL MessUp (fp fAmount, PCPoint pEllipseCent, PCPoint pEllipseRad);
   void Tip (PCPoint pTip);
   BOOL Straighten (void);
   BOOL Twist (BOOL fLength, fp fAngle);

private:
   BOOL CalcRenderIfNecessary (POBJECTRENDER pr, PCMatrix pmPointToClipSpace, PCMatrix pmClipSpaceToPoint);
   void FixLengths (PCPoint pap = NULL);
   BOOL IKJitter (PCPoint papPath, DWORD dwNumPoint, DWORD *padwSplinePoint, PCPoint paPullDir,
      PCPoint pEllipseCent, PCPoint pEllipseRad);
   fp IKScore (PCPoint papPath, DWORD dwNumPoint, DWORD *padwSplinePoint, PCPoint paPullDir, 
      PCPoint pEllipseCent, PCPoint pEllipseRad);

   CListFixed     m_lPathPoint;        // Array of points
                                       // spline for the path
                                       // note point 1 (not 0) is actually fixed point.
                                       // point 0 used to determine how hair attached to root
   CListFixed     m_lPathTwist;        // delta twist of hair in radians (float). One entry per m_lPathPoint
   TEXTUREPOINT   m_atpProfile[5];     // profile. .h=horz, .v=vert radius of circle. 1.0 = default with, 0 = less.
                                       // [0] = at start, [1] = start+1, [2]=mid, [3]=end-1, [4]=at end
   fp             m_fDiameter;         // diameter of hair lock. Multiplied by m_atpProfile.h and .v
   fp             m_fLengthPerPoint;   // length of hair lock per point in spline. Spline points are equidistant
   fp             m_fRemainder;        // 0..1, indicates how much of the last spline point is actually drawn.
                                       // the length in meters if m_fRemainder * m_fLengthPerPoint
   DWORD          m_dwHairLayers;      // number of hair layers. Normally 1, but can go up to MAXHAIRLAYERS
   DWORD          m_adwHairLayerRepeat[MAXHAIRLAYERS];  // number of times to repeat texture around hair (so no seem)
                                       // for each hair layer
   fp             m_afHairLayerScale[MAXHAIRLAYERS]; // amount to scale each hair layer diameter (0..1) based on original
                                       // size (m_fDiamater). In general numbers declining in size
   DWORD          m_dwVariationSeed;   // seed used for random variation in profile along length
   fp             m_fVariation;        // amount of variation in profile along length

   // run-time calculated
   DWORD          m_dwDetailProf;      // detail level of last drawn. 0=minimal, 2=maximum
   DWORD          m_dwDetailLen;       // detal along length, 0..2
   BOOL           m_fDirty;            // set to TRUE if spline calculations dirty
   DWORD          m_dwRenderSub;       // number of sub-renders. Use more than one to make faster for ray trace
   CMem           m_amemRenderPoint[MAXRENDERSUB];    // memory for points render
   CMem           m_amemRenderNorm[MAXRENDERSUB];     // memory for normal render
   CMem           m_amemRenderText[MAXRENDERSUB];     // memory for texture render
   CMem           m_amemRenderVert[MAXRENDERSUB];     // memory for vertex render
   CMem           m_amemRenderPoly[MAXRENDERSUB];     // memory for polygon render
   CListFixed     m_lpTwistVector;     // calculated twist vector for each node of m_splinePath. Normalized
                                       // used by control point set and get. Only valid if !m_fDirty
   CListFixed     m_lpDirVector;       // calculated direction vector for each node of m_splinePath. Used for
                                       // control points set and get. Only valid if !m_fDirty
   CMatrix        m_mCalcPointToClipSpace;   // matrix that was used for point to clip space caclulations
   BOOL           m_fCalcPointToClipSpace;   // set to TRUE if m_mCalcPointToClipSpace was used, or FALSE if was NULL
   BOOL           m_fCalcCompletelyClipped;  // set to TRUE if was completely clipped
};
typedef CHairLock *PCHairLock;



/**********************************************************************************
CObjectHairLock */
// {722D3D0D-8F4B-4a82-B0D3-83D601511755}
DEFINE_GUID(CLSID_HairLock, 
0x1f2d3d94, 0x1c4b, 0xa282, 0x1f, 0x06, 0x83, 0xd6, 0x1, 0x51, 0x17, 0x55);
// CObjectHairLock is a test object to test the system
class DLLEXPORT CObjectHairLock : public CObjectTemplate {
public:
   CObjectHairLock (PVOID pParams, POSINFO pInfo);
   ~CObjectHairLock (void);

   virtual void Delete (void);
   virtual void Render (POBJECTRENDER pr, DWORD dwSubObject);
   virtual CObjectSocket *Clone (void);
   virtual PCMMLNode2 MMLTo (void);
   virtual BOOL MMLFrom (PCMMLNode2 pNode);
   virtual BOOL MoveReferencePointQuery (DWORD dwIndex, PCPoint pp);
   virtual BOOL MoveReferenceStringQuery (DWORD dwIndex, PWSTR psz, DWORD dwSize, DWORD *pdwNeeded);
   virtual BOOL ControlPointQuery (DWORD dwID, POSCONTROL pInfo);
   virtual BOOL ControlPointSet (DWORD dwID, PCPoint pVal, PCPoint pViewer);
   virtual void ControlPointEnum (PCListFixed plDWORD);
   virtual BOOL DialogQuery (void);
   virtual BOOL DialogShow (DWORD dwSurface, PCEscWindow pWindow, PCPoint pClick);
   virtual void QueryBoundingBox (PCPoint pCorner1, PCPoint pCorner2, DWORD dwSubObject);


   // variables
   CHairLock         m_HairLock;    // lock of hair
   BOOL              m_fBackfaceCull;  // if TRUE then can backface cull

   CRenderSurface    m_Renderrs;    // to minimize mallocs
};
typedef CObjectHairLock *PCObjectHairLock;



/**********************************************************************************
CObjectHairHead */
DEFINE_GUID(CLSID_HairHead, 
0x1ca43d0d, 0x4a4b, 0x1d82, 0x23, 0x46, 0x95, 0xd6, 0x1, 0x51, 0x17, 0x55);

typedef struct {
   PCHairLock     pLock;            // lock of hair. Can be NULL.
   CPoint         pLocSphere;       // location on spherical coords, from -1 to 1
   CPoint         pNorm;            // normal (given ellipse shape)
} HAIRHEADROOT, *PHAIRHEADROOT;

// CObjectHairHead is a test object to test the system
class DLLEXPORT CObjectHairHead : public CObjectTemplate {
public:
   CObjectHairHead (PVOID pParams, POSINFO pInfo);
   ~CObjectHairHead (void);

   virtual void Delete (void);
   virtual void Render (POBJECTRENDER pr, DWORD dwSubObject);
   virtual CObjectSocket *Clone (void);
   virtual PCMMLNode2 MMLTo (void);
   virtual BOOL MMLFrom (PCMMLNode2 pNode);
   virtual BOOL MoveReferencePointQuery (DWORD dwIndex, PCPoint pp);
   virtual BOOL MoveReferenceStringQuery (DWORD dwIndex, PWSTR psz, DWORD dwSize, DWORD *pdwNeeded);
   virtual BOOL ControlPointQuery (DWORD dwID, POSCONTROL pInfo);
   virtual BOOL ControlPointSet (DWORD dwID, PCPoint pVal, PCPoint pViewer);
   virtual void ControlPointEnum (PCListFixed plDWORD);
   virtual BOOL DialogQuery (void);
   virtual BOOL DialogShow (DWORD dwSurface, PCEscWindow pWindow, PCPoint pClick);
   virtual BOOL DialogCPQuery (void);
   virtual BOOL DialogCPShow (DWORD dwSurface, PCEscWindow pWindow, PCPoint pClick);

   virtual BOOL AttribGetIntern (PWSTR pszName, fp *pfValue);
   virtual void AttribGetAllIntern (PCListFixed plATTRIBVAL);
   virtual void AttribSetGroupIntern (DWORD dwNum, PATTRIBVAL paAttrib);
   virtual BOOL AttribInfoIntern (PWSTR pszName, PATTRIBINFO pInfo);
   virtual void QueryBoundingBox (PCPoint pCorner1, PCPoint pCorner2, DWORD dwSubObject);

//private:
   void DeleteAllLocks (void);      // deletes all locks and clears m_lHAIRHEADROOT
   void ApplyRadius (void);         // loops through all m_lHAIRHEADROOT and appies the radius
   void NewDensity (void);          // deletes all locks, then intializes using the new density
   void SproutNewHair (DWORD dwRoot, fp fHeight);  // sprouts a new hair at the given root
   void SyncRoot (DWORD dwRoot);    // synchronize one root to the given diameter, length per point, etc.
   void SyncAllRoots (void);        // synchronize all roots to the given diameter, length per point, etc.
   DWORD RootMirror (DWORD dwRoot);
   fp RootWeight (PCPoint pOrig, DWORD dwRoot);

   // variables saved in MML
   fp             m_fEndHairWeight; // 1.0 if end hair moves stiffly, 0.0 if holds to old position in space
   CPoint         m_pHat;           // [0] = z location of hat at center of head (1000 => not using). [1] = rot angle around x, [2] =rot angle around y
   DWORD          m_dwDensity;      // hairs across 90 degrees of arc, 1+
   DWORD          m_dwCurEdit;      // current root being edited
   BOOL           m_fShowEllipse;   // if TRUE draw the ellipse. Always draw if there aren't any hairs
   BOOL           m_fSymmetry;      // if TRUE then symmetry turned on
   BOOL           m_fBackfaceCull;  // if TRUE then can backface cull
   CPoint         m_pRadius;        // radius of head, so elliptical
   CListFixed     m_lHAIRHEADROOT;  // list of hair info
   fp             m_fBrushSize;     // brush size, from 0 .. 1
   fp             m_fDiameter;         // diameter of hair lock. From 0..1, real meaning depends upon radius values
   fp             m_fLengthPerPoint;   // length of hair lock per point in spline. Spline points are equidistant
   DWORD          m_dwHairLayers;      // number of hair layers. Normally 1, but can go up to MAXHAIRLAYERS
   DWORD          m_adwHairLayerRepeat[MAXHAIRLAYERS];  // number of times to repeat texture around hair (so no seem)
                                       // for each hair layer
   fp             m_afHairLayerScale[MAXHAIRLAYERS]; // amount to scale each hair layer diameter (0..1) based on original
                                       // size (m_fDiamater). In general numbers declining in size
   TEXTUREPOINT   m_atpProfile[5];     // profile. .h=horz, .v=vert radius of circle. 1.0 = default with, 0 = less.
                                       // [0] = at start, [1] = start+1, [2]=mid, [3]=end-1, [4]=at end
   fp             m_fVariation;        // amount of variation in profile along length

   CRenderSurface    m_Renderrs;    // to minimize mallocs
};
typedef CObjectHairHead *PCObjectHairHead;



/**********************************************************************************
CObjectBranch */
DEFINE_GUID(CLSID_Branch, 
0x16723d0d, 0x8f8a, 0x4a82, 0x10, 0xd3, 0x47, 0xd6, 0x1, 0x9c, 0x17, 0x8b);


class CBranchNode;
typedef CBranchNode *PCBranchNode;
class CObjectClone;
typedef CObjectClone *PCObjectClone;
typedef struct {
   GUID        gCode;         // object code
   GUID        gSub;          // object sub-code
   fp          fWeight;       // number of points given to it when calculating branch thickness

   // calculated
   PCObjectClone pClone;      // clone of the leaf
   DWORD       dwDim;         // dimension that it's longest along, 0 for x, 1 for y (not Z though)
   BOOL        fNegative;     // if TRUE then longest along negative x
   fp          fDimLength;    // length along the dimension that's longest
} LEAFTYPE, *PLEAFTYPE;

typedef struct {
   PCNoodle    pNoodle;       // noodle to draw
   fp          fLength;       // length of the noodle in meters - so can hide if low detail
} BRANCHNOODLE, *PBRANCHNOODLE;

// AUTOBRANCH - Information for automatic branch generation
typedef struct {
   // branch lenght
   fp          fBranchInitialAngle; // initial angle. 0..PI. 0 = up
   fp          fBranchLength; // typical length, meters
   fp          fBranchLengthVar; // variation in lenght. 0..1
   fp          fBranchShorten;   // amount shortens per generation. 0..1

   // branch extension
   fp          fBranchExtendProb;      // probability that will extend branch, 0..1
   fp          fBranchExtendProbGen;   // change in fBranchExtendProb per generation, 0..1
   fp          fBranchDirVar;          // variation in direction, 0..1

   // branch forking
   fp          fBranchForkProb;        // probability of branch forking, 0..1
   fp          fBranchForkProbGen;     // chance of forking increasing per generation, 0..1
   fp          fBranchForkNum;         // number of forks, 0..1.
   fp          fBranchForkForwards;    // how forwards. 1 = forwards, 0 = backwards
   fp          fBranchForkUpDown;      // how up/down. 1=up, 0=down
   fp          fBranchForkVar;         // variation in forking, 0..1.

   // branch gravity
   fp          fBranchGrav;            // 1=branchs will want to go upwards, 0=branch go downwards
   fp          fBranchGravGen;         // how much fBranchGrav changes each generation. 1 = increase, 0 = decrease

   // leaves along branch
   fp          fLeafExtendProb;        // 0..1, probability of leaf being along extension
   fp          fLeafExtendProbGen;     // 0..1, likelihood of leaves increasing
   fp          fLeafExtendNum;         // 0..1, number of leaves
   fp          fLeafExtendForwards;    // 0..1, 1 = forwards, 0 = backwards
   fp          fLeafExtendVar;         // 0..1, variation in leaf direction
   fp          fLeafExtendScale;       // 0..1, .5 = default scale
   fp          fLeafExtendScaleVar;    // 0..1, amount leaf size varies
   fp          fLeafExtendScaleGen;    // 0..1, how much leaf size changes, .5 = none, 1 = larger

   // leaf at terminal node
   fp          fLeafEndProb;           // 0..1, likelihood of leaf at end
   fp          fLeafEndNum;            // 0..1, number of leaves at the end
   fp          fLeafEndNumVar;         // 0..1, variation in the number of leaves at the end
   fp          fLeafEndSymmetry;       // 0..1, amount of symmetry in end leaves
   fp          fLeafEndVar;            // 0..1, variation in leaf location at end
   fp          fLeafEndScale;          // 0..1, .5 = default scale
   fp          fLeafEndScaleVar;       // 0..1, amount leaf size varies

   // misc
   DWORD       dwSeed;                 // seed to use
   DWORD       dwMaxGen;               // maximum generations
} AUTOBRANCH, *PAUTOBRANCH;

class DLLEXPORT CObjectBranch : public CObjectTemplate {
public:
   CObjectBranch (PVOID pParams, POSINFO pInfo);
   ~CObjectBranch (void);

   virtual void Delete (void);
   virtual void Render (POBJECTRENDER pr, DWORD dwSubObject);
   virtual CObjectSocket *Clone (void);
   virtual PCMMLNode2 MMLTo (void);
   virtual BOOL MMLFrom (PCMMLNode2 pNode);
   virtual BOOL MoveReferencePointQuery (DWORD dwIndex, PCPoint pp);
   virtual BOOL MoveReferenceStringQuery (DWORD dwIndex, PWSTR psz, DWORD dwSize, DWORD *pdwNeeded);
   virtual BOOL ControlPointQuery (DWORD dwID, POSCONTROL pInfo);
   virtual BOOL ControlPointSet (DWORD dwID, PCPoint pVal, PCPoint pViewer);
   virtual void ControlPointEnum (PCListFixed plDWORD);
   virtual BOOL DialogQuery (void);
   virtual BOOL DialogShow (DWORD dwSurface, PCEscWindow pWindow, PCPoint pClick);
   virtual BOOL TextureQuery (PCListFixed plText);
   virtual BOOL ColorQuery (PCListFixed plColor);
   virtual BOOL ObjectClassQuery (PCListFixed plObj);
   virtual void QueryBoundingBox (PCPoint pCorner1, PCPoint pCorner2, DWORD dwSubObject);

//private:
   void CalcBranches (void);
   void AutoBranchFill (void);
   void AutoBranchGen (void);
   PCBranchNode AutoBranchGenIndiv (PCPoint pDir, DWORD dwMaxGen, PAUTOBRANCH pAB);

   PCBranchNode         m_pRoot;       // root branch
   CListFixed           m_lLEAFTYPE;   // list of leaves supported
   fp                   m_fThickScale; // given a thickness of 1 unit, how many meters does this translate into
   DWORD                m_dwDivide;    // number of times to divide the noodle
   BOOL                 m_fRound;      // set to TRUE if use extra smooth rounding
   DWORD                m_dwTextureWrap;  // how to wrap around when creating noodles
   BOOL                 m_fCap;        // set to TRUE to cap the ends
   BOOL                 m_fDispLowDetail;  // set to TRUE if want to temporarily display as low detail
   BOOL                 m_fRoots;      // set to TRUE if have roots
   fp                   m_fBaseThick;  // make thicker at the base
   fp                   m_fRootThick;  // how much to scale root thickness by
   fp                   m_fTapRoot;    // how deep to draw the tap root

   // calculated
   BOOL                 m_fDirty;      // set to true if need to recalc nodes, etc.
   CListFixed           m_lBRANCHNOODLE;  // list of branch noodles
   CListFixed           m_lBRANCHCP;      // CP for branch
   PCBranchNode         m_pCurModify;     // curent one modifying
   BOOL                 m_fCurModifyLeaf; // set to true if modifying a leaf
   PVOID                m_pCurModifyIndex;  // current index of what modifying (if leaf), or pointer to next node if branch
   CListFixed           m_lDialogHBITMAP; // bitmap handles for images
   CListFixed           m_lDialogCOLORREF;   // transparency info fo rimages
   PCRevolution         m_pRevFarAway;     // drawn if object is far away
   fp                   m_fLowDetail;     // size of low detail
   PAUTOBRANCH          m_pAutoBranch;    // in case doing automatic branches

   CRenderSurface       m_Renderrs;       // for malloc optimization
   CListFixed           m_lRenderList;    // for malloc optimization
   CListFixed           m_lRenderListBack;   // backup
};
typedef CObjectBranch *PCObjectBranch;


/**********************************************************************************
CObjectBone */
DEFINE_GUID(CLSID_Bone, 
0x722afd0d, 0x4c4b, 0x4a82, 0x18, 0xd3, 0x83, 0x62, 0x1, 0x51, 0x17, 0x55);


/* CBone - Handle all the main bone code */
class CRenderSurface;
typedef CRenderSurface *PCRenderSurface ;

class CBone {
public:
   ESCNEWDELETE;

   CBone (void);
   ~CBone (void);

   PCMMLNode2 MMLTo (void);
   BOOL MMLFrom (PCMMLNode2 pNode);
   BOOL CloneTo (CBone *pTo, BOOL fIncludeChildren = TRUE);

   void CalcMatrix (void);
   void CalcObjectSpace (PCMatrix pBoneToObject, BOOL fFixed, PCMatrix pmNextInLine = NULL,
      BOOL fForIK = FALSE);
   void Render (POBJECTRENDER pr, PCRenderSurface prs, CBone *pEnvelope, PCListFixed plBoneList);
   void AddToList (PCListFixed pl);
   BOOL Reorient (PCMatrix pm);
   BOOL DeleteChild (CBone *pFind);
   BOOL Split (PWSTR pszSplitName);
   void Scale (PCPoint pScale);
   void FillInParent (CBone *pParent);
   fp CalcMass (BOOL fJustThis = FALSE);
   fp CalcIKMass (void);

//private:
   // written out
   WCHAR             m_szName[64];  // name
   CMem              m_memDesc;     // description
   BOOL              m_fCanBeFixed; // if TRUE can be fixed element in the IK chain
   fp                m_fFixedAmt;   // amount that is fixed. 0 = not fixed. 1 = normal. 2 = high
   CPoint            m_pEnd;        // location of end of bone (in original coords), start is always 0,0,0
   CPoint            m_pUp;         // up vector (in original coords). Always length 1, perp to m_pEnd
   CPoint            m_pMotionMin;  // minimum motion. [0]=hinge (along pUp) rads,[1]=pivot (perp pUp) rads,[2]=twist rads,[3]=lengthen multiplier
   CPoint            m_pMotionMax;  // maximum motion.
   CPoint            m_pMotionDef;  // value of the motion as is appears in the default hape (using m_pEnd, m_pUp)
   CPoint            m_pMotionCur;  // current motion
   BOOL              m_fFlipLR;     // flip the definition of left
   BOOL              m_fUseLimits;  // if TRUE use min/max to limit motion
   BOOL              m_fUseEnvelope;   // must be TRUE to use an envelope
   BOOL              m_fDefLowRank;       // if defaults to low rank
   BOOL              m_fDefPassUp;        // if defaults to passing up
   fp                m_fDrawThick;  // drawing thickness, as a percentage (0..1) of bone length
   CListFixed        m_lPCBone;     // list of children
   CListFixed        m_lObjRigid;   // list of GUIDs for objects that have rigid bodies
   CListFixed        m_lObjIgnore;  // list of GUIDs for objects that are ignored
   CPoint            m_pEnvStart;   // envelope at start. Units are length of bone. p[0]=top, p[1]=right, p[2]=bottom, p[3]=left
   CPoint            m_pEnvEnd;     // envelope at end. See m_pEnvStart
   CPoint            m_pEnvLen;     // envelope at edges (beyond bone). Units are length of bone. p[0]=start, p[1]=end

   // calculated
   CMatrix           m_mChildCur;   // matrix to apply to child rotation (when rendering)
   CMatrix           m_mRender;     // use this when rendering, so goes from 0,0,0 to len,0,0. Up = 0,0,1
   CMatrix           m_mCalcOS;     // matrix applied to the starting point to deform if !fFixed in CalcObjectSpace
   CMatrix           m_mXToEnd;     // used in CObjectEditor when calculating
   CPoint            m_pStartOS;    // start location, in object coords (includes rotation or cur)
   CPoint            m_pEndOS;      // end location, in object coords (includes rotation of cur)
   CPoint            m_pUpOS;       // up vector, in object coords (includes rotation of cur)
   CPoint            m_pStartOF;    // fixed to default settings
   CPoint            m_pEndOF;      // fixes to default settings
   CPoint            m_pUpOF;       // fixed to default settings
   CBone             *m_pParent;    // filled in by FillInParent() call

   // temporary variables used for IK
   fp                m_fIKMass;     // mass of this and children, only calced when doing ik
   BOOL              m_fIKChain;    // set to TRUE if in IK chain, FALSE if not
   int               m_iIKTwiddle;  // twiddle direction... 0 for none. 1..4 for m_pMotionCur.p[0..3], -1..-4 for negative values
   CPoint            m_pIKOrig;     // original m_pMotionCur before IK experiment done
   CPoint            m_pIKWant;     // if m_fFixedAmt this is kept up to date with IK loc that want
};
typedef CBone *PCBone;


// OEBW - Scratch for calculating weight associated with each bone when rendering
typedef struct {
   fp          fWeight;       // amount of weight to apply
   CPoint      pBone;         // location in bone space
   CPoint      pNormBone;     // normal in bone space
} OEBW, *POEBW;

// OEBONE - Used as companion structure to CBone's in CObjectBone::m_lBoneList
typedef struct {
   PCBone      pBone;   // bone this corresponds to
   PCListFixed plChildren; // list of children, POEBONE elements
   PVOID       pParent; // parent. Typecase to POEBONE

   // dealing with pre-bend space
   BOOL        fDeform;          // sets to true if deforms, FALSE if not. If doesnt deform, no transform info used
   CMatrix     mPreToBone;       // converts a point in pre-bend world space to the bone space
   CMatrix     mBoneToPre;       // converts from bone space to pre-bend world space
   CMatrix     mPreNormToBone;   // converts a pre-bend normal (in world space) to the bone space normal
   fp          fPreBoneLen;      // length of the bone before deformation
   CPoint      apBound[2];       // min/max bounding points in world space

   // post bend space (after bone has been bent), only valid if fDeform
   CMatrix     mBoneToPost;      // convert from bone space to post-bend internal world space...
                                 // x=0..fPreBoneLen (will do stretch)
                                 // does NOT do the rotation
   CMatrix     mBoneNormToPost;  // converts a normal in bone coords to post-bend coords
                                 // does NOT do the rotation
} OEBONE, *POEBONE;

// OERIGIDBONE - Keep track of which objects are affecting in rigid way by bone
// so that when calculate CP from them, the CP are treated as rigid
typedef struct {
   GUID        gObject;       // object's guid that is rigid
   PCBone      pBone;         // bone that affects it
} OERIGIDBONE, *POERIGIDBONE;



// CObjectBone is a test object to test the system
class DLLEXPORT CObjectBone : public CObjectTemplate {
public:
   CObjectBone (PVOID pParams, POSINFO pInfo);
   ~CObjectBone (void);

   virtual void Delete (void);
   virtual void Render (POBJECTRENDER pr, DWORD dwSubObject);
   virtual CObjectSocket *Clone (void);
   virtual PCMMLNode2 MMLTo (void);
   virtual BOOL MMLFrom (PCMMLNode2 pNode);
   virtual BOOL Message (DWORD dwMessage, PVOID pParam);
   virtual BOOL Merge (GUID *pagWith, DWORD dwNum);
   virtual BOOL ControlPointQuery (DWORD dwID, POSCONTROL pInfo);
   virtual BOOL ControlPointSet (DWORD dwID, PCPoint pVal, PCPoint pViewer);
   virtual void ControlPointEnum (PCListFixed plDWORD);
   virtual BOOL DialogQuery (void);
   virtual BOOL DialogShow (DWORD dwSurface, PCEscWindow pWindow, PCPoint pClick);
   virtual BOOL AttribGetIntern (PWSTR pszName, fp *pfValue);
   virtual void AttribGetAllIntern (PCListFixed plATTRIBVAL);
   virtual void AttribSetGroupIntern (DWORD dwNum, PATTRIBVAL paAttrib);
   virtual BOOL AttribInfoIntern (PWSTR pszName, PATTRIBINFO pInfo);
   virtual BOOL MoveReferencePointQuery (DWORD dwIndex, PCPoint pp);
   virtual BOOL MoveReferenceStringQuery (DWORD dwIndex, PWSTR psz, DWORD dwSize, DWORD *pdwNeeded);
   virtual BOOL ObjectMatrixSet (CMatrix *pObject);
   virtual BOOL EditorDestroy (void);
   virtual BOOL EditorShowWindow (BOOL fShow);
   virtual BOOL EditorCreate (BOOL fAct);
   virtual BOOL Deconstruct (BOOL fAct);

   DWORD CurBoneGet (void);
   void CurBoneSet (DWORD dwBone);
   BOOL DialogBoneEdit (DWORD dwBone, PCEscWindow pWindow);
   DWORD SymmetryGet (void);
   void SymmetrySet (DWORD dwSymmetry);
   void DefaultPosition (void);
   BOOL SplitBone (DWORD dwBone);
   BOOL DeleteBone (DWORD dwBone);
   BOOL DisconnectBone (DWORD dwBone);
   BOOL ScaleBone (DWORD dwBone, PCPoint pScale);
   BOOL RotateBone (DWORD dwBone, fp fRot);
   BOOL AddBone (DWORD dwAddTo, PCPoint pDir);

//private:
   BOOL Merge (PCObjectBone pWith, PCMatrix mToObject);
   void CalcObjectSpace (void);
   void MakeNameUnique (PCBone pUnique);
   void FindMirrors (PCBone pRoot, PCListFixed plPCBone, PCListFixed plDWORD);
   void CalcAttribList (void);
   BOOL IKDetermineChain (DWORD dwBone, PCPoint pLoc, PCPoint pViewer, PCListFixed plIKCHAIN);
   fp IKEvaluateChain (PCListFixed plIKCHAIN, PCPoint pNewTranslate, PCBone pChanged = NULL);
   int IKTwiddleLink (PCListFixed plIKCHAIN, DWORD dwLink, fp fPreTwiddleError, fp fStrength = 1);
   BOOL IKTwiddleAll (PCListFixed plIKCHAIN, fp fPreTwiddleError);
   BOOL IKApplyTwiddle (PCListFixed plIKCHAIN, fp fStrength, DWORD *padwSpecific = NULL);
   BOOL IKBestStrength (PCListFixed plIKCHAIN, PCPoint pNewTrans);
   BOOL IKSlowConverge (PCListFixed plIKCHAIN, PCPoint pNewTrans, fp fStrength);
   void IKMove (DWORD dwBone, PCPoint pWant, PCPoint pViewer, PCPoint pNewTrans);
   void ResetIKWant (void);

   // Not used anymore - old IK code that only allowed one fixed pt
   // BOOL IKDetermineChain (DWORD dwBone, PCListFixed plIKCHAIN);
   //fp IKEvaluateChain (PCListFixed plIKCHAIN, PCPoint pWant, PCPoint pViewer);
   //int IKTwiddleLink (PCListFixed plIKCHAIN, PCPoint pWant, PCPoint pViewer, DWORD dwLink);
   //BOOL IKTwiddleAll (PCListFixed plIKCHAIN, PCPoint pWant, PCPoint pViewer);
   //BOOL IKApplyTwiddle (PCListFixed plIKCHAIN, fp fStrength);
   //BOOL IKBestStrength (PCListFixed plIKCHAIN, PCPoint pWant, PCPoint pViewer);
   //void IKMove (DWORD dwBone, PCPoint pWant, PCPoint pViewer);


   CListFixed     m_lPCBone;     // list of bones, in tree formation, with sub-bones referenced off main bone
   DWORD          m_dwSymmetry;  // bit field. 0x01 = x symmetry, 0x02=y symmetry, 0x04=z symetry
   DWORD          m_dwCurBone;   // index into m_lBoneList for the current bone that working on (which CP shown)

   // calculated
   CListFixed     m_lBoneList;   // list of PCBone. Just a list of all the bones. NOT sorted. Kept up to date
   CListFixed     m_lOBATTRIB;   // so know list of attributes
   BOOL           m_fBoneDefault;   // if TRUE all the bones are in default positions
   CPoint         m_apBound[2];  // bounding box for bones (in their default state)
   BOOL           m_fSkeltonForOE;  // set to TRUE if this is acting as the skeleton for the OE, will shows different CP
   CPoint         m_pIKTrans;    // if m_fSkeletonForOE, this will be set after a ::ControlPointSet() to the amount that the
                                 // CObjectBone (and hence the entire OE object) should be translated to keep IK in sync

   // bone information translated over from CObjectEditor, but better suited for
   // being in bone. The following functions and variables are used for bone effects
   void ObjEditBonePointConvert (PCPoint pPointWorld, PCPoint pNormWorld,
                              PCPoint pPointDeform, PCPoint pNormDeform,
                              DWORD dwNumExistWeight = 0, PCBone *papExistWeightBone = NULL, fp *pafExistWeight = NULL);
   void ObjEditBoneRender (PPOLYRENDERINFO pInfo, PCRenderSocket pRS,
                        PCMatrix pmToWorld);
   void ObjEditBonesClear (void);
   void ObjEditBonesTestObject (DWORD dwObject, BOOL fAllBones = FALSE);
   void ObjEditBonesCalcPreBend (void);
   void ObjEditBonesCalcPostBend (void);
   BOOL ObjEditBonesRigidCP (PCObjectSocket pos, PCMatrix pmBoneRot);
   BOOL ObjEditBoneSetup (BOOL fForce = FALSE, PCMatrix pmExtraRot = NULL);
   PCListFixed m_plOEBONE;      // pointer to list of OEBONE structure, each OEBONE element corresponds to bone in pObjectBone->m_lBoneList
   PCListFixed m_plOERIGIDBONE; // pointer to list of OERIGIDBONE structures
   PCListFixed m_plBoneMove;    // list of PCBone sorted so the ones closest to the ground are first, allow to set as movmeent point,
                              // so can make walking on ground easier
   PCListFixed m_plBoneAffect;  // list of POEBONE for which bones affect the current object
   PCListFixed m_plOEBW;        // one element per element in plBoneAffect. Weight of each bone. sizeof(OEBW).
   BOOL        m_fBoneAffectUniform;  // if checked, there's only one bone, and it should have a uniform effect
   CMatrix     m_mBoneExtraRot;  // extra rotation

   CRenderSurface    m_Renderrs;    // to minimize mallocs
};
typedef CObjectBone *PCObjectBone;


/**********************************************************************************
CObjectPolyhedron */
// {2D7B1A31-5337-4dda-9477-3C5E760B3DCA}
DEFINE_GUID(CLSID_Polyhedron, 
0x2d7b1a31, 0x5337, 0x4dda, 0x94, 0x77, 0x3c, 0x5e, 0x76, 0xb, 0x3d, 0xca);


// CObjectPolyhedron is a test object to test the system
class DLLEXPORT CObjectPolyhedron : public CObjectTemplate {
public:
   CObjectPolyhedron (PVOID pParams, POSINFO pInfo);
   ~CObjectPolyhedron (void);

   virtual void Delete (void);
   virtual void Render (POBJECTRENDER pr, DWORD dwSubObject);
   virtual CObjectSocket *Clone (void);
   virtual PCMMLNode2 MMLTo (void);
   virtual BOOL MMLFrom (PCMMLNode2 pNode);
   virtual BOOL DialogCPQuery (void);
   virtual BOOL DialogCPShow (DWORD dwSurface, PCEscWindow pWindow, PCPoint pClick);
   virtual BOOL ControlPointQuery (DWORD dwID, POSCONTROL pInfo);
   virtual BOOL ControlPointSet (DWORD dwID, PCPoint pVal, PCPoint pViewer);
   virtual void ControlPointEnum (PCListFixed plDWORD);
   virtual void QueryBoundingBox (PCPoint pCorner1, PCPoint pCorner2, DWORD dwSubObject);

//priviate:
   void     CalcScale (void);
   void     RenderFacet (CRenderSurface *prs,
                                     PCPoint papPoint, DWORD *padwVertex, DWORD dwPointIndex);

   DWORD    m_dwDisplayControl;        // which control points displayed, 0=scale, 1=control points
   DWORD    m_dwDisplayConstraint;     // 0 = full constraint, 1 = partial, 2=none
   DWORD    m_dwShape;                 // shape - 0 = box, etc.

   CListFixed m_lPoints;               // list of vertext points for the object

   // calculated
   CPoint   m_pScaleMax;               // highest points in size
   CPoint   m_pScaleMin;               // smallest points in size
   CPoint   m_pScaleSize;              // max - min, at least size of CLOSE


   CRenderSurface    m_Renderrs;       // for malloc optimization
};
typedef CObjectPolyhedron *PCObjectPolyhedron;


/************************************************************************
CPolyMesh */

class CPolyMesh;
typedef CPolyMesh *PCPolyMesh;

// SIDETEXT - Alternate texture based on a specific side
typedef struct {
   DWORD          dwSide;     // side index
   TEXTUREPOINT   tp;         // alternate texture point
} SIDETEXT, *PSIDETEXT;

// VERTDEFORM - How much to deform a vertex based on deformation
typedef struct {
   DWORD          dwDeform;   // deform index
   CPoint         pDeform;    // amount to deform
} VERTDEFORM, *PVERTDEFORM;

// BONEWEIGHT - How much to deform a vertex based on bone
typedef struct {
   DWORD          dwBone;     // bone index
   fp             fWeight;    // how much bone weighs on vertex, 1 = full value
                              // NOTE: Must keep as fp because is assumed that in BoneApplyToRender
} BONEWEIGHT, *PBONEWEIGHT;

// PMMIRROR - Information about mirror
typedef struct {
   DWORD          dwObject;   // other object that is mirrored (index into list of vertices, sides, etc.)
   DWORD          dwType;     // type of mirror. Bitfield. 0x01 for x, 0x02 for y, 0x04 for z.
} PMMIRROR, *PPMMIRROR;

// CPMVert - Information about a vertex in a polygon mesh
class DLLEXPORT CPMVert {
public:
   ESCNEWDELETE;

   CPMVert (void);
   // no destructor necessary

   // functions to get the 1st element of the mirrors, sides, edges, etc.
   PMMIRROR *MirrorGet (void);
   DWORD *SideGet (void);
   DWORD *EdgeGet (void);
   PSIDETEXT SideTextGet (void);
   PVERTDEFORM VertDeformGet (void);
   PBONEWEIGHT BoneWeightGet (void);
   PVOID MaxPMGet (void);  // limit of memory
   BOOL HasAMirror (DWORD dwSymmetry);

   // functions to add new elements
   BOOL MirrorAdd (PMMIRROR *pMirror, WORD wNum=1);
   BOOL SideAdd (DWORD *padwSide, WORD wNum=1);
   BOOL EdgeAdd (DWORD *padwEdge, WORD wNum=1);
   BOOL SideTextAdd (PSIDETEXT pSIDETEXT, WORD wNum=1);
   BOOL VertDeformAdd (PVERTDEFORM pVERTDEFORM, WORD wNum=1);
   BOOL BoneWeightAdd (PBONEWEIGHT pBONEWEIGHT, WORD wNum=1);

   // functions to remove elements.. deletes anything that matches the given elem, and reduces higher values by 1
   void MirrorRemove (DWORD dwRemove);
   void SideRemove (DWORD dwRemove);
   void EdgeRemove (DWORD dwRemove);
   void SideTextRemove (DWORD dwRemove);
   void VertDeformRemove (DWORD dwRemove);
   void BoneWeightRemove (DWORD dwRemove);

   // functions to clear out elements in list
   void MirrorClear (void);
   void SideClear (void);
   void EdgeClear (void);
   void SideTextClear (void);
   void VertDeformClear (void);
   void BoneWeightClear (void);

   // functions to remove elements... anything with a map of -1 is removed, else renamed
   void MirrorRename (DWORD dwNum, DWORD *padwOrig, DWORD *padwNew);
   void SideRename (DWORD dwNum, DWORD *padwOrig, DWORD *padwNew);
   void EdgeRename (DWORD dwNum, DWORD *padwOrig, DWORD *padwNew);
   void SideTextRename (DWORD dwNum, DWORD *padwOrig, DWORD *padwNew, BOOL fCanCreateDup);
   void VertDeformRename (DWORD dwNum, DWORD *padwOrig, DWORD *padwNew);
   void BoneWeightRename (DWORD dwNum, DWORD *padwOrig, DWORD *padwNew);

   // textures
   PTEXTUREPOINT TextureGet (DWORD dwSide = -1);     // returns texture based on the side
   BOOL TextureSet (DWORD dwSide, PTEXTUREPOINT pt, BOOL fAlwaysAdd = FALSE);     // set the texture based on the side

   // deformations
   BOOL DeformChanged (DWORD dwDeform, PCPoint pNewDelta);

   // apply deformations
   void DeformCalc (DWORD dwNum, DWORD *padwDeform, fp *pafDeform, PCPoint pLoc);
   void CalcNorm (PCPolyMesh pMesh);   // calculate the normal at the point

   // misc
   void Clear (void);
   void Scale (PCMatrix pScale, PCMatrix pScaleInvTrans);   // scales point and normals
   BOOL Average (CPMVert **papVert, fp *pafWeight, DWORD dwNum, DWORD dwSide, BOOL fKeepDeform);   // if TRUE keep deformations
   BOOL CloneTo (CPMVert *pTo, BOOL fKeepDeform = TRUE);
   CPMVert *Clone (BOOL fKeepDeform = TRUE);
   PCMMLNode2 MMLTo (void);
   BOOL MMLFrom (PCMMLNode2 pNode);
   BOOL MMLToBinary (PCMem pMem, PCPoint pScale, PTEXTUREPOINT ptScale, fp fRound, DWORD dwSymmetry);
   void MMLToBinaryCalcMinMax (PCPoint pLocMax, PCPoint pLocMin, PTEXTUREPOINT ptTextMax,
                                     PTEXTUREPOINT ptTextMin);
   DWORD MMLFromBinary (PBYTE pab, DWORD dwSize, PCPoint pScale, PTEXTUREPOINT ptScale);
   void SideTextClean (void);


   // variables
   CPoint            m_pLoc;        // location of the vertex in space
   TEXTUREPOINT      m_tText;       // texture of the vertex
   CPoint            m_pNorm;       // calculated normal
   CPoint            m_pLocSubdivide;  // when the object is rendered, this is filled in with
                                    // the location of the point after all the subdivision is done
   CPoint            m_pNormSubdivide; // like m_pLocSubdivide except contains normal

   // do not change the following
   WORD              m_wNumMirror;  // number of mirrors of the point (due to symmetry)
   WORD              m_wNumSide;    // number of sides this is on
   WORD              m_wNumEdge;    // number of edges this is on
   WORD              m_wNumSideText;   // number of remaps of textures based on side
   WORD              m_wNumVertDeform; // number of deformations specific to vertex
   WORD              m_wNumBoneWeight; // bone weighting for bones

private:

   CMem              m_memMisc;     // memory containing misc information

};
typedef CPMVert *PCPMVert;


// CPMSide - Information about a side in a polygon mesh
class DLLEXPORT CPMSide {
public:
   ESCNEWDELETE;

   // constructor and destructor
   CPMSide (void);
   // destructor not needed

   // misc
   void Clear (void);
   BOOL CloneTo (CPMSide *pTo, BOOL fKeepDeform = TRUE);
   CPMSide *Clone (BOOL fKeepDeform = TRUE);
   PCMMLNode2 MMLTo (void);
   BOOL MMLFrom (PCMMLNode2 pNode);
   BOOL MMLToBinary (PCMem pMem);
   DWORD MMLFromBinary (PBYTE pab, DWORD dwSize);

   // functions to get the 1st element of the mirrors, sides, edges, etc.
   PMMIRROR *MirrorGet (void);
   DWORD *VertGet (void);
   PVOID MaxPMGet (void);  // limit of memory
   BOOL CPMSide::HasAMirror (DWORD dwSymmetry, PCPMVert *ppv);

   // functions to add new elements
   BOOL MirrorAdd (PMMIRROR *pMirror, WORD wNum=1);
   BOOL VertAdd (DWORD *padwVert, WORD wNum=1);

   BOOL VertInsert (DWORD dwVert, DWORD dwIndex);
   DWORD VertFind (DWORD dwVert);

   // functions to remove elements.. deletes anything that matches the given elem, and reduces higher values by 1
   void MirrorRemove (DWORD dwRemove);
   void VertRemove (DWORD dwRemove);
   void VertRemoveByIndex (DWORD dwIndex);

   // functions to clear out elements in list
   void MirrorClear (void);
   void VertClear (void);

   // functions to remove elements... anything with a map of -1 is removed, else renamed
   void MirrorRename (DWORD dwNum, DWORD *padwOrig, DWORD *padwNew);
   void VertRename (DWORD dwNum, DWORD *padwOrig, DWORD *padwNew);

   void CalcNorm (PCPolyMesh pMesh);
   void Reverse (void);
   void RotateVert (BOOL fRight);

   // can change
   DWORD             m_dwSurfaceText;  // texture number (object specific) used for surface
   DWORD             m_dwOrigSide;  // original side - used when subdividing

   // do not change the following
   WORD              m_wNumMirror;  // number of mirrors of the point (due to symmetry)
   WORD              m_wNumVert;    // number of vertices

   // calculated
   CPoint            m_pNorm;       // normal for the fce

private:
   CMem              m_memMisc;     // memory containing misc information
};
typedef CPMSide *PCPMSide;


// CPMEdge - Information about an edge inthe polygon mesh
class CPMEdge;
typedef CPMEdge *PCPMEdge;
class DLLEXPORT CPMEdge {
public:
   ESCNEWDELETE;

   CPMEdge (void);
   // no destructor for now

   // misc
   void Clear (void);
   BOOL CloneTo (CPMEdge *pTo);
   CPMEdge *Clone (void);
   BOOL HasAMirror (DWORD dwSymmetry, PCPMVert *ppv);

#ifdef MIRROREDGE // Dont store mirror info in edges
   // functions to get the 1st element of the mirrors, sides, edges, etc.
   PMMIRROR *MirrorGet (void);
   PVOID MaxPMGet (void);  // limit of memory

   // functions to add new elements
   BOOL MirrorAdd (PMMIRROR *pMirror, WORD wNum=1);

   // functions to remove elements.. deletes anything that matches the given elem, and reduces higher values by 1
   void MirrorRemove (DWORD dwRemove);

   // functions to clear out elements in list
   void MirrorClear (void);

   // functions to remove elements... anything with a map of -1 is removed, else renamed
   void MirrorRename (DWORD dwNum, DWORD *padwOrig, DWORD *padwNew);
#endif

   void VertSet (DWORD dwVert1, DWORD dwVert2);
   void VertRename (DWORD dwNum, DWORD *padwOrig, DWORD *padwNew);   // note - will need to reorder vert


   // variables
   DWORD             m_adwSide[2];  // twice sides that create edge

   // do not change the following
   DWORD             m_adwVert[2];  // two vertices that create edge. m_adwVert[0] < m_adwVert[1]
   PCPMEdge          m_apTree[2];   // for tree sort... [0] is less than, [1] is more than
#ifdef MIRROREDGE // Dont store mirror info in edges
   WORD              m_wNumMirror;  // number of mirrors of the point (due to symmetry)

private:
   CMem              m_memMisc;     // memory containing misc information
#endif
};
typedef CPMEdge *PCPMEdge;


// PMBONEMORPH - Info on how bone causes a morph to activate
typedef struct {
   WCHAR             szName[64];    // name of the morph. 0 string if none
   DWORD             dwDim;         // dimension of the bone to use 0=hinge,1=swing,2=rotate,3=stretch
   TEXTUREPOINT      tpBoneAngle;   // bone angle (or extension) min (.h) and max (.v)
   TEXTUREPOINT      tpMorphValue;  // what the morph value is given the bone angle
} PMBONEMORPH, *PPMBONEMORPH;

// PMBONEINFO - Information that polymesh has about bones
#define NUMBONEMORPH    2           // number of bone morphs per bone
typedef struct {
   WCHAR             szName[64];    // name of the bone
   PCBone            pBone;         // bone to use, must double check that is still valid pointer
   CPoint            pBoneLastRender;  // position of the bone (m_pMotionCur) as was used in the last render, all 4 values used
                                    // this info makes rendering faster because can tell if bone moved in process
   PCBone            apMirror[7];   // mirrors of this bone, [(x + y*2 + z*4) - 1]

   PMBONEMORPH       aBoneMorph[NUMBONEMORPH];   // number of morphs for this bone
} PMBONEINFO, *PPMBONEINFO;


// CPolyMesh - Polygon mesh
class DLLEXPORT CPolyMesh {
public:
   ESCNEWDELETE;

   // constructor and destuctor
   CPolyMesh (void);
   ~CPolyMesh (void);

   // standard
   void Clear (void);
   BOOL CloneTo (CPolyMesh *pTo, BOOL fKeepDeform = TRUE);
   CPolyMesh *Clone (BOOL fKeepDeform = TRUE);
   PCMMLNode2 MMLTo (void);
   BOOL MMLFrom (PCMMLNode2 pNode);

   // symmetry
   void SymmetrySet (DWORD dwSymmetry);   // sets the symmetry that will be using
   DWORD SymmetryGet (void);
   void MirrorVert (DWORD *padw, DWORD dwNum, PCListFixed plMirror);
   void MirrorEdge (DWORD *padw, DWORD dwNum, PCListFixed plMirror, PCListFixed plMirrorOnly = NULL);
   void MirrorSide (DWORD *padw, DWORD dwNum, PCListFixed plMirror);
   void SidesToEdges (DWORD *padw, DWORD dwNum, PCListFixed pl);
   void SidesToVert (DWORD *padw, DWORD dwNum, PCListFixed pl);
   void EdgesToVert (DWORD *padw, DWORD dwNum, PCListFixed pl);
   BOOL SymmetryVertGroups (DWORD dwNum, DWORD *padw, PCListFixed paList);
   BOOL SymmetrySideGroups (DWORD dwNum, DWORD *padw, PCListFixed paList);
   BOOL MirrorToOtherSide (DWORD dwDim, BOOL fKeepPos);

   // groups
   BOOL GroupVertMove (DWORD dwVert, PCPoint pDir, DWORD dwNumGroup, DWORD *padwGroup);
   void GroupCOM (DWORD dwType, DWORD dwNum, DWORD *padw, PCPoint pCenter,
      BOOL fIncludeDeform);
   void GroupNormal (DWORD dwType, DWORD dwNum, DWORD *padw, PCPoint pNorm,
      BOOL fIncludeDeform);
   BOOL GroupMove (DWORD dwMove, PCPoint pMove, DWORD dwType, DWORD dwNum, DWORD *padw);
   BOOL GroupDelete (DWORD dwType, DWORD dwNum, DWORD *padw);
   BOOL GroupBevel (fp fRadius, DWORD dwType, DWORD dwNum, DWORD *padw);
   BOOL OrganicMove (PCPoint pCenter, fp fRadius, fp fBrushPow, fp fBrushStrength,
                             DWORD dwNumSide, DWORD *padwSide, DWORD dwType, PCPoint pMove,
                             PCObjectTemplate posMask, DWORD dwMaskColor, BOOL fMaskInvert);
   BOOL OrganicScale (PCPoint pScale,
                             PCObjectTemplate posMask, DWORD dwMaskColor, BOOL fMaskInvert);
   BOOL Merge (PCPolyMesh pMerge, PCMatrix pmMergeToThis,
      PCObjectTemplate pThisTemp, PCObjectTemplate pMergeTemp);

   // subdivision
   void SubdivideSet (DWORD dwSubDivide); // -1 = faceted, 0=none but smooth, etc.
   DWORD SubdivideGet (void);
   BOOL Subdivide (BOOL fKeepDeform);  // cause the polygon mesh to be subdivided

   // vertex functions
   DWORD VertFind (PCPoint pPoint, PCListFixed plVert = NULL);
   DWORD VertAdd (PCPoint pPoint);
   PCPMVert VertGet (DWORD dwVert);
   void VertToBoundBox (PCPoint pMin, PCPoint pMax, PCPoint pScaleSize);
   void VertToBoundBoxRot (PCPoint pMin, PCPoint pMax, DWORD dwNumVert, DWORD *padwVert,
                                   PCMatrix pmObjectToTexture);
   DWORD VertFindFromClick (PCPoint pLoc, DWORD dwSide);
   BOOL VertLocGet (DWORD dwVert, PCPoint pLoc);
   BOOL VertCollapse (DWORD dwNum, DWORD *padw);
   void VertRenamed (DWORD dwNum, DWORD *padwOrig, DWORD *padwNew);
   void VertSideLinkRebuild (void);
   void VertSideTextClean (void);
   void VertDeleteMany (DWORD dwNum, DWORD *padw);
   void VertDeleteDead (void);
   DWORD VertNum (void);
   void VertOnEdge (DWORD dwNumVert, DWORD *padwVert, DWORD dwNumSide, DWORD *padwSide,
                            PCListFixed plVert);

   // side functions
   PCPMSide SideGet (DWORD dwSide);
   DWORD SideAdd (DWORD dwSurface, DWORD *padwVert, DWORD dwNum);
   DWORD SideAdd (DWORD dwSurface, DWORD dwVert1, DWORD dwVert2, DWORD dwVert3, DWORD dwVert4 = -1);
   BOOL SideCalcMirror (DWORD *padwVert, DWORD dwNum, DWORD dwMirror,
                                PCListFixed pList);
   BOOL SideAreSame (DWORD *padwVert1, DWORD *padwVert2, DWORD dwNum);
   void SideFlipNormal (DWORD dwNum, DWORD *padw);
   void SideRotateVert (DWORD dwNum, DWORD *padw);
   BOOL SideMakePlanar (DWORD dwNum, DWORD *padw);
   void SideRenamed (DWORD dwNum, DWORD *padwOrig, DWORD *padwNew, BOOL fCanCreateDup);
   void SideDeleteMany (DWORD dwNum, DWORD *padw);
   void SideDeleteDead (void);
   BOOL SideMerge (DWORD dwSide1, DWORD dwSide2, DWORD *padwEdge);
   BOOL SideTesselate (DWORD dwNum, DWORD *padw, DWORD dwTess);
   BOOL SideSplit (DWORD dwSide, DWORD *padwSplit1, DWORD *padwSplit2);
   BOOL SideAddChecks (DWORD dwNum, DWORD *padwVert);
   void SideToGroups (DWORD dwNum, DWORD *padwSide, PCListFixed plSideGroup);
   void SideGroupToEdges (DWORD dwNum, DWORD *padwSide, PCListFixed plEdge,
      BOOL fIgnoreSingleEdge = FALSE);
   BOOL SideGroupToPath (DWORD dwNum, DWORD *padwSide, PCListFixed plVert);
   BOOL SideExtrude (DWORD dwNum, DWORD *padwSide, DWORD dwEffect, BOOL fPiecewise, fp fAmt);
   BOOL SideDisconnect (DWORD dwNum, DWORD *padwSide);
   BOOL SideKeep (DWORD dwNum, DWORD *padw);
   DWORD SideNum (void);

   // edge functions
   BOOL EdgeAdd (DWORD dwVert1, DWORD dwVert2, DWORD dwSide);
   DWORD EdgeFind (DWORD dwVert1, DWORD dwVert2);
   PCPMEdge EdgeGet (DWORD dwEdge);
   BOOL EdgeSplit (DWORD dwNum, DWORD *padw, PCListFixed plMirrorOnly = NULL);
   void CalcEdges (void);           // recalculates the edges based on the vertices and sides

   // textures
   void TextureSpherical (DWORD dwNumSide, DWORD *padwSide, PCMatrix pmObjectToTexture,
      BOOL fInMeters);
   void TextureCylindrical (DWORD dwNumSide, DWORD *padwSide, PCMatrix pmObjectToTexture,
      BOOL fInMeters);
   void TextureLinear (DWORD dwNumSide, DWORD *padwSide, PCMatrix pmObjectToTexture,
      BOOL fInMeters);
   void FixWraparoundTextures (DWORD dwNumSide, DWORD *padwSide, PCMatrix pmObjectToTexture);
   void WipeExtraText(DWORD dwNumSide, DWORD *padwSide);
   BOOL SurfacePaint (DWORD dwNumSide, DWORD *padwSide, DWORD dwSurface);
   void SurfaceInMetersSet (DWORD dwSurface, BOOL fInMeters);
   BOOL SurfaceInMetersGet (DWORD dwSurface);
   void SurfaceRename (DWORD dwOrig, DWORD dwNew);
   void SurfaceFilterSides (DWORD dwNumSide, DWORD *padwSide, DWORD dwSurface,
                                    PCListFixed plFilter);
   void SurfaceFilterVert (DWORD dwNumVert, DWORD *padwVert,
                                   DWORD dwNumSide, DWORD *padwSide,
                                    PCListFixed plFilter);
   BOOL TextureMove (DWORD dwNumVert, DWORD *padwVert, DWORD dwNumSide, DWORD *padwSide,
                             DWORD dwSurface, DWORD dwType, PTEXTUREPOINT pMove);
   BOOL TextureDisconnect (DWORD dwNumSide, DWORD *padwSide, DWORD dwSurface);
   BOOL TextureDisconnectVert (DWORD dwNumVert, DWORD *padwVert,
      DWORD dwNumSide, DWORD *padwSide, DWORD dwSurface);
   BOOL TextureCollapse (DWORD dwNumVert, DWORD *padwVert, DWORD dwSurface);
   BOOL TextureMirror (DWORD dwSurface, DWORD dwDim, BOOL fFromPos, PTEXTUREPOINT ptAxis,
                               BOOL fMirrorH, BOOL fMirrorV);

   // automatic shape creation
   void CreateBasedOnType (DWORD dwType, DWORD dwDivisions, PCPoint paParam);
   void CreateSphere (DWORD dwNum, PCPoint pScale);
   void CreateCylinder (DWORD dwNum, PCPoint pScale, PCPoint pScaleNegative, BOOL fCap);
   void CreateCylinderRounded (DWORD dwNum, PCPoint pScale, PCPoint pScaleNegative);
   void CreateTaurus (DWORD dwNum, PCPoint pScale, PCPoint pInnerScale);
   void CreateBox (DWORD dwNum, PCPoint pScale);
   void CreatePlane (DWORD dwNum, PCPoint pScale);
   void CreateCone (DWORD dwNum, PCPoint pScale, BOOL fShowTop, BOOL fShowBottom,
                                  fp fTop, fp fBottom);

   // drawing
   BOOL Render (POBJECTRENDER pr, PCRenderSurface prs, PCObjectTemplate pot);
   void QueryBoundingBox (PCPoint pCorner1, PCPoint pCorner2, PCObjectTemplate pot);

   // selection
   void SelModeSet (DWORD dwMode);
   DWORD SelModeGet (void);
   BOOL SelAdd (DWORD dwIndex,  DWORD dwIndex2 = 0);
   BOOL SelRemove (DWORD dwIndex);
   void SelClear (void);
   void SelAll (BOOL fSelAllAsymmetrical);
   void SelSort (void);
   DWORD *SelEnum (DWORD *pdwNum);
   DWORD SelFind (DWORD dwIndex, DWORD dwIndex2 = 0);
   void SelVertRemoved (DWORD dwVert);
   void SelVertRenamed (DWORD dwNum, DWORD *padwOrig, DWORD *padwNew);
   void SelSideRemoved (DWORD dwSide);
   void SelSideRenamed (DWORD dwNum, DWORD *padwOrig, DWORD *padwNew);

   // attributes
   BOOL AttribInfoIntern (PWSTR pszName, PATTRIBINFO pInfo);
   void AttribSetGroupIntern (DWORD dwNum, PATTRIBVAL paAttrib, PCWorldSocket pWorld, PCObjectSocket pos);
   void AttribGetAllIntern (PCListFixed plATTRIBVAL);
   BOOL AttribGetIntern (PWSTR pszName, fp *pfValue);

   // morphs
   void CalcMorphRemap (void);
   BOOL CanModify (void);
   DWORD MorphStateGet (void);
   void MorphStateSet (DWORD dwState);
   BOOL DialogEditMorph (PCEscWindow pWindow, PCWorldSocket pWorld, PCObjectSocket pObject,
                                 DWORD dwMorph);
   BOOL DialogAddMorph (PCEscWindow pWindow, PCWorldSocket pWorld, PCObjectSocket pObject);
   BOOL MorphPaint (PCPoint pCenter, fp fRadius, fp fBrushPow, fp fBrushStrength,
                             DWORD dwNumSide, DWORD *padwSide, DWORD dwType, fp fScale);
   BOOL MorphScale (PCPoint pScale);

   // bones
   PCObjectBone BoneGet (PCWorldSocket pWorld);
   BOOL BoneSyncNames (PCObjectBone pBone, PCObjectTemplate pObject);
   void BoneSyncNamesOrFindOne (PCObjectTemplate pObject);
   void BoneClear (void);
   void BoneMirrorCalc (PCObjectBone pBone);
   void BoneDisplaySet (DWORD dwBone);
   DWORD BoneDisplayGet (void);
   void CalcBoneColors (PCPolyMesh pUse);
   PPMBONEINFO BoneEnum (DWORD *pdwNum);
   PCObjectBone BoneSeeIfRenderDirty (PCWorldSocket pWorld, BOOL *pfHasBoneMorph);
   void BoneApplyToRender (PCPolyMesh pUse, PCObjectBone pBone, PCMatrix pmObjectToWorld);
   BOOL BonePaint (PCPoint pCenter, fp fRadius, fp fBrushPow, fp fBrushStrength,
                             DWORD dwNumSide, DWORD *padwSide, DWORD dwType, PCWorldSocket pWorld);
   BOOL BoneMirror (DWORD dwDim, BOOL fKeepPos, PCWorldSocket pWorld, BOOL fCopy);
   BOOL DialogEditBone (PCEscWindow pWindow, PCWorldSocket pWorld, PCObjectSocket pObject,
                                 DWORD dwBone);

   // misc
   BOOL PolyRenderToMesh (PPOLYRENDERINFO pInfo, PCMatrix pmObjectToMesh,
                                  PCObjectTemplate pTemplate);

   // variables that can change
   BOOL              m_fCanBackface; // set to TRUE if can backface cull, FALSE if can't
   BOOL              m_fDirtyMorphCOM; // set to TRUE if the calculated morph COM are dirty
   BOOL              m_fDirtyRender; // set to true if something has changed how will render

   // attributes morphs - dont change this
   CListFixed        m_lPCOEAttrib;     // list of pointers to attributes (morphs) supported
                                        // by polygon mesh

private:
   void SymmetryRecalc (void);      // recalculates the symmetry entries depending upon m_dwSymmetry
   void CalcNorm (void);         // calculate the normals for all the points

   BOOL CalcRenderSides (DWORD *padwSides, DWORD dwNum, DWORD dwSurface,
                                 BOOL fSmooth, DWORD dwSpecial, PCPolyMesh pAddTo);
   BOOL CalcRender (BOOL fSmooth, DWORD dwSpecial, PCPolyMesh pAddTo);
   void CalcRender (DWORD dwSpecial, PCObjectBone pBone, PCMatrix pmObjectToWorld);

   BOOL SideExtrudeOneGroup (DWORD dwNum, DWORD *padwSide, DWORD dwNumLoop,
                                     DWORD *padwLoop, PCPoint pNorm, PCPoint pCOM,
                                     DWORD dwEffect, fp fAmt);
   BOOL SideDisconnectOneGroup (DWORD dwNum, DWORD *padwSide, DWORD dwNumLoop,
                                     DWORD *padwLoop);
   DWORD SideCutBevel (DWORD dwSide, DWORD *padwEdgeVert, fp fRadius,
                               PCListFixed plBevelFinished, PCListFixed plBevelQueue,
                               DWORD dwVertAutoAccept, BOOL fAutoAddEdge = FALSE);
   BOOL BevelEdges (fp fRadius, PCListFixed plEdge, PCListFixed plVert, PCListFixed plSideNew,
      PCListFixed plVertToDelete);
   BOOL EdgeDelete (DWORD dwNum, DWORD *padw, PCListFixed plVertDel = NULL);
   fp VertTextureToScale (DWORD dwVert, DWORD dwNumSide, DWORD *padwSide,
                                  PCObjectTemplate pos, DWORD dwColor, BOOL fInvert);
   BOOL MergeNoSymmetry (PCPolyMesh pMerge, PCMatrix pmMergeToThis, BOOL fFlip,
      PWSTR pszAppend, PCObjectTemplate pThisTemp, PCObjectTemplate pMergeTemp);
   void CalcMorphColors (PCPolyMesh pUse);
   void CalcMorphCOM (void);

   // varaibles
   DWORD             m_dwSymmetry;  // bit field. 0x01 for x, 0x02 for y, 0x04 for z
   CListFixed        m_lVert;       // list of vertices (PPMVert)
   CListFixed        m_lSide;       // list of sides (PPMSide)
   CListFixed        m_lEdge;       // list of edges (PPMEdge)
   DWORD             m_dwSubdivide; // number of times to subdivide - for smoothing
   BOOL              m_fDirtyEdge;  // set to TRUE if edge data is dirty
   BOOL              m_fDirtyNorm;  // set to TRUE if the normals data is dirty
   BOOL              m_fDirtyScale; // set to TRUE if m_pScaleSize is dirty
   BOOL              m_fDirtySymmetry; // set to TRUE if symmetry is dirty
   CPoint            m_pBoundMin, m_pBoundMax;  // scale of polygon mesh
   DWORD             m_dwSpecialLast;  // what dwSpecial was last time called render

   // selection
   DWORD             m_dwSelMode;   // selection mode, 0 for point, 1 for edge, 2 for side
   CListFixed        m_lSel;        // selection list. Initialized to sizeof(DWORD) for point or side, 2xsizeof(DWORD) for edge
   BOOL              m_fSelSorted;  // TRUE if the selection list is sorted, FALSE if not

   // rendering
   CListFixed        m_lRenderSurface; // surface number to render with.
   // dont use CListVariable     m_lRenderColor;   // color # to render with, COLORREF
   CListVariable     m_lRenderPoints;  // list of CPoints for rendering, points
   CListVariable     m_lRenderNorms;   // list of CPoints for rendering, normals
   CListVariable     m_lRenderText;    // list of TEXTUREPOINT for rendering, textures
   CListVariable     m_lRenderVertex;  // list of VERTEX
   CListVariable     m_lRenderPoly;    // list of polygons for rendering, same as polydescript info
   CListVariable     m_lRenderColor;   // list of COLORREF for rendering
   BOOL              m_fBonesAlreadyApplied; // set to TRUE if bones already applied

   // attributes/morphs
   CListFixed        m_lMorphRemapID;   // IDs for morph remapping. DWORD values
   CListFixed        m_lMorphRemapValue; // values for morph remapping. fp values
   CListFixed        m_lMorphCOM;      // list of CPoint for COM of each morph

   // misc surface
   CListFixed        m_lSurfaceNotInMeters;  // list of DWORD ids to indicate which surface is treated as being in meters

   // bones
   GUID              m_gBone;          // GUID for the bone's ID, or NULL if no bone
   CListFixed        m_lPMBONEINFO;    // list of bone info associated with vertices
   CMatrix           m_mBoneLocLastRender;   // location of bone in the last render, to tell if bone moved
   BOOL              m_fBoneMirrorValid;  // set to TRUE if the bone mirror info in m_lPMBONEINFO is valid
   DWORD             m_dwBoneDisplay;  // which bone to color. index into m_lPMBONEINFO to display, or out-of-range value for all
   BOOL              m_fBoneChangeShape;  // set to TRUE if the bone morphing has caused a shape change
};
typedef CPolyMesh *PCPolyMesh;



/**********************************************************************************
CObjectPolyMesh */
// {38D4F348-2740-470b-B93C-083B87ABADF4}
DEFINE_GUID(CLSID_PolyMesh,
0x38d4f348, 0x2740, 0x470b, 0xb9, 0x3c, 0x8, 0x3b, 0x87, 0xab, 0xad, 0xf4);

DEFINE_GUID(CLSID_PolyMesh_Cube,
0xa41, 0x5c3f, 0x7e, 0xc0, 0x3a, 0x5a, 0x2d, 0xb1, 0x53, 0xc2, 0x1);

class CPolyMesh;
typedef CPolyMesh *PCPolyMesh;

// CObjectPolyMesh is a test object to test the system
class DLLEXPORT CObjectPolyMesh : public CObjectTemplate {
public:
   CObjectPolyMesh (PVOID pParams, POSINFO pInfo);
   ~CObjectPolyMesh (void);

   virtual void Delete (void);
   virtual void Render (POBJECTRENDER pr, DWORD dwSubObject);
   virtual CObjectSocket *Clone (void);
   virtual PCMMLNode2 MMLTo (void);
   virtual BOOL MMLFrom (PCMMLNode2 pNode);
   virtual BOOL DialogQuery (void);
   virtual BOOL DialogShow (DWORD dwSurface, PCEscWindow pWindow, PCPoint pClick);
   virtual BOOL ControlPointQuery (DWORD dwID, POSCONTROL pInfo);
   virtual BOOL ControlPointSet (DWORD dwID, PCPoint pVal, PCPoint pViewer);
   virtual void ControlPointEnum (PCListFixed plDWORD);
   virtual BOOL EditorCreate (BOOL fAct);
   virtual BOOL EditorDestroy (void);
   virtual BOOL EditorShowWindow (BOOL fShow);
   virtual BOOL Message (DWORD dwMessage, PVOID pParam);
   virtual BOOL Merge (GUID *pagWith, DWORD dwNum);
   virtual BOOL AttribGetIntern (PWSTR pszName, fp *pfValue);
   virtual void AttribGetAllIntern (PCListFixed plATTRIBVAL);
   virtual void AttribSetGroupIntern (DWORD dwNum, PATTRIBVAL paAttrib);
   virtual BOOL AttribInfoIntern (PWSTR pszName, PATTRIBINFO pInfo);
   virtual BOOL Deconstruct (BOOL fAct);
   virtual void QueryBoundingBox (PCPoint pCorner1, PCPoint pCorner2, DWORD dwSubObject);

// private:
   CPolyMesh         m_PolyMesh;    // polymesh object to use
   DWORD             m_dwDivisions; // if 0 then phaseII, else number of divisions fo rphase I
   CPoint            m_apParam[2];  // parameters used for Phase I
   DWORD             m_dwSubdivideWork;   // number of subidivisions for working
   DWORD             m_dwSubdivideFinal;  // number of subdividions for final render

   CRenderSurface    m_Renderrs;       // for malloc optimization
};
typedef CObjectPolyMesh *PCObjectPolyMesh;

DLLEXPORT DWORD DWORDSearch (DWORD dwFind, DWORD dwNum, DWORD *padwElem);

/**********************************************************************************
CObjectEditor */

DLLEXPORT int _cdecl ATTRIBVALCompare (const void *elem1, const void *elem2);

/* COEAttrib - Stores information about how object-editor attribute is stored and derived */
#define COEMAXREMAP        5        // maximum number of remap points
typedef struct {
   GUID        gFromObject;         // GUID for which object it's from
   WCHAR       szName[64];          // attribute name in that object, not used in morphs
   DWORD       dwMorphID;           // used only in morphs - an ID#
   DWORD       dwNumRemap;          // number of remap points
   fp          afpComboValue[COEMAXREMAP];   // what the combo value is (numberically increasing)
   fp          afpObjectValue[COEMAXREMAP];  // what it's remapped to for the object
   DWORD       dwCapEnds;           // flag field. if 0x01 then values less than afpComboValue[0]
                                    // are capped to afpObjectValue[0]. Else, they are linearly
                                    // interpreded down. 0x02 is for afpComboValue[dwNumRemap-1]
} COEATTRIBCOMBO, *PCOEATTRIBCOMBO;
extern PWSTR gpszAttrib;

class DLLEXPORT COEAttrib {
public:
   ESCNEWDELETE;

   COEAttrib (void);
   ~COEAttrib (void);

   BOOL MMLFrom (PCMMLNode2 pNode);
   PCMMLNode2 MMLTo (void);
   void CloneTo (COEAttrib *pTo);


   // generated at runtime
   BOOL     m_fAutomatic;        // if TRUE was automatically generated by exported other attributes
                                 // MMLTo() does not write out if m_fAutomatic = TRUE

   // saved away
   DWORD    m_dwType;            // 0 for an attribute to use, 1 for an un-map so that an
                                 // automatic attribute is NOT exported
                                 // 2 - (Used for morphs only) - Original attribute. m_lCOEATTRIBUTECOMBO ignored
   WCHAR    m_szName[64];        // name of the attribute
   GUID     m_gRemove;           // Only used if m_dwType==1. Will ignore automatically exproted
                                 // attributes of m_szName from m_gRemove
   CListFixed m_lCOEATTRIBCOMBO; // list of which content attributes to combine to make this one
   fp       m_fDefValue;         // default value
   CMem     m_memInfo;           // info string
   fp       m_fMin;              // min value
   fp       m_fMax;              // max value
   DWORD    m_dwInfoType;        // AIT_XXX
   BOOL     m_fDefLowRank;       // if defaults to low rank
   BOOL     m_fDefPassUp;        // if defaults to passing up

   // used for supporting multiple instances
   fp       m_fCurValue;         // current value that has been passed down to all contained objects
};
typedef COEAttrib *PCOEAttrib;

DLLEXPORT void ObjEditClearPCOEAttribList (PCListFixed plPCOE);
DLLEXPORT void ObjEditCreatePCOEAttribList (PCWorldSocket pWorld, PCListFixed plPCOE);
DLLEXPORT BOOL ObjEditWritePCOEAttribList (PCWorldSocket pWorld, PCListFixed plPCOE);

// Template object - holder for creating templates
// {7DD42D20-06F4-400d-A924-0CF5AD1C7285}
DEFINE_GUID(CLSID_ObjTemplate, 
0x7dd42d20, 0x6f4, 0x400d, 0xa9, 0x24, 0xc, 0xf5, 0xad, 0x1c, 0x72, 0x85);

// {1FB57A4D-9D0F-4fbb-B0EA-E34C9049AAEF}
DEFINE_GUID(CLSID_ObjEditor, 
0x1fb57a4d, 0x9d0f, 0x4fbb, 0xb0, 0xea, 0xe3, 0x4c, 0x90, 0x49, 0xaa, 0xef);

BOOL ObjEditReloadCache (DWORD dwRenderShard, GUID *pgCode, GUID *pgSub);

class CObjectEditor;
typedef CObjectEditor *PCObjectEditor;

class DLLEXPORT COERemap : public CRenderSocket {
public:
   ESCNEWDELETE;

   COERemap (void);
   ~COERemap (void);

   // from CRenderSocket
   BOOL QueryWantNormals (void);    // returns TRUE if renderer wants normal vectors
   BOOL QueryWantTextures (void);    // returns TRUE if renderer wants textures
   fp QueryDetail (void);       // returns detail resoltion (in meters) that renderer wants now
                                                // this may change from one object to the next.
   void MatrixSet (PCMatrix pm);    // sets the current operating matrix. Set NULL implies identity.
                                                // Note: THis matrix will be right-multiplied by any used by renderer for camera
   void PolyRender (PPOLYRENDERINFO pInfo);
                                                // draws/commits all the polygons. The data in pInfo, and what it points
   virtual BOOL QuerySubDetail (PCMatrix pMatrix, PCPoint pBound1, PCPoint pBound2, fp *pfDetail);
                                                // to may be modified by this call.
   virtual BOOL QueryCloneRender (void);
   virtual BOOL CloneRender (GUID *pgCode, GUID *pgSub, DWORD dwNum, PCMatrix pamMatrix);

   // must be set
   PCObjectEditor     m_pEdit;
   CMatrix            m_mMatrixSet;    // last matrix set

private:
   DWORD             m_adwRecurse[5];      // keep count of recursions so dont

   CListFixed        m_lCloneRenderm;     // minimize mallocs
};
typedef COERemap *PCOERemap;

// CSSRemap - Remap calls to CSurfaceScheme from the objects in  CObjectEditor, so
// that colors can be changed.
class DLLEXPORT CSSRemap : public CSurfaceSchemeSocket {
public:
   ESCNEWDELETE;

   CSSRemap (void);
   ~CSSRemap (void);

   // from CSurfaceSchemeSocket
   virtual PCMMLNode2 MMLTo (void);
   virtual BOOL MMLFrom (PCMMLNode2 pNode);
   // Dont support this: virtual CSurfaceScheme *Clone (void);
   virtual PCObjectSurface SurfaceGet (PWSTR pszScheme, PCObjectSurface pDefault, BOOL fNoClone);
   virtual BOOL SurfaceExists (PWSTR pszScheme);
   virtual BOOL SurfaceSet (PCObjectSurface pSurface);
   virtual BOOL SurfaceRemove (PWSTR pszScheme);
   virtual PCObjectSurface SurfaceEnum (DWORD dwIndex);
   virtual void WorldSet (CWorldSocket *pWorld);
   virtual void Clear (void);

   // remap functions
   DWORD RemapNum (void);
   PCObjectSurface RemapGetSurface (DWORD dwNum);
   PWSTR RemapGetName (DWORD dwNum);
   DWORD RemapFind (PWSTR psz);
   BOOL RemapRemove (DWORD dwNum);
   BOOL RemapAdd (PWSTR pszName, PCObjectSurface pSurface);
   void RemapClear (void);
   PCMMLNode2 RemapMMLTo (void);
   BOOL RemapMMLFrom (PCMMLNode2 pNode);
   void CloneTo (CSSRemap *pNew);

   // to be set by application
   PCSurfaceSchemeSocket   m_pMaster;     // set this to the master socket to call
   PCSurfaceSchemeSocket   m_pOrig;       // set this to the original schemes, in case the master doesn't know
                                          // generally, this is CWorld.m_pSurfaceSchemeOrig

   // automatically maintained
   CBTree                  m_tSchemesQuerried;  // tree of schemes that were querried for with SurfaceGet().
                              // indexed by scheme name. Use to remap wMinorID in RENDERSURFACE so
                              // can tell which scheme belongs to which minor surface

private:
   CBTree                  m_tRemap;   // names of surface remaped. The content is a PCObjectSurface
   DWORD                   m_dwRecurse;   // to check number of times recursing
};
typedef CSSRemap *PCSSRemap;

class CWorld;
typedef CWorld *PCWorld;
class DLLEXPORT CObjectEditor : public CObjectTemplate {
   friend class COERemap;

public:
   CObjectEditor (PVOID pParams, POSINFO pInfo);
   ~CObjectEditor (void);

   virtual void Delete (void);
   virtual void Render (POBJECTRENDER pr, DWORD dwSubObject);
   virtual CObjectSocket *Clone (void);
   virtual PCMMLNode2 MMLTo (void);
   virtual BOOL MMLFrom (PCMMLNode2 pNode);
   virtual BOOL TextureQuery (PCListFixed plText);
   virtual BOOL ColorQuery (PCListFixed plColor);
   virtual BOOL ObjectClassQuery (PCListFixed plObj);
   virtual PCObjectSurface SurfaceGet (DWORD dwID);
   virtual BOOL SurfaceSet (PCObjectSurface pos);
   virtual BOOL MoveReferencePointQuery (DWORD dwIndex, PCPoint pp);
   virtual BOOL MoveReferenceStringQuery (DWORD dwIndex, PWSTR psz, DWORD dwSize, DWORD *pdwNeeded);
   virtual fp TurnOnGet (void);
   virtual BOOL TurnOnSet (fp fOpen);
   virtual fp OpenGet (void);
   virtual BOOL OpenSet (fp fOpen);
   virtual BOOL LightQuery (PCListFixed pl, DWORD dwShow);
   virtual BOOL EmbedDoCutout (void);
   virtual BOOL AttribGetIntern (PWSTR pszName, fp *pfValue);
   virtual void AttribGetAllIntern (PCListFixed plATTRIBVAL);
   virtual void AttribSetGroupIntern (DWORD dwNum, PATTRIBVAL paAttrib);
   virtual BOOL AttribInfoIntern (PWSTR pszName, PATTRIBINFO pInfo);
   virtual BOOL ControlPointQuery (DWORD dwID, POSCONTROL pInfo);
   virtual BOOL ControlPointSet (DWORD dwID, PCPoint pVal, PCPoint pViewer);
   virtual void ControlPointEnum (PCListFixed plDWORD);
   virtual BOOL AttachClosest (PCPoint pLoc, PWSTR pszClosest);
   virtual void AttachPointsEnum (PCListVariable plEnum);
   virtual void InternAttachMatrix (PWSTR pszAttach, PCMatrix pmAttachRot);
   virtual BOOL EditorCreate (BOOL fAct);
   virtual BOOL EditorDestroy (void);
   virtual BOOL EditorShowWindow (BOOL fShow);
   virtual void QueryBoundingBox (PCPoint pCorner1, PCPoint pCorner2, DWORD dwSubObject);


private:
   PCWorld        m_pObjectWorld;      // where the sub-objects are stored
   COERemap       m_Remap;             // remapping object
   CSSRemap       m_SSRemap;           // remapt the surface socket
   POBJECTRENDER  m_pObjectRender;     // temporary variable used for rendering
   CMatrix        m_mObjectTemp;       // temporary matrix used while rendering
   PVOID          m_pOBJWORLDCACHE;    // points to the cache. Useful at times.
   CListFixed     m_lATTRIBVAL;        // instance specific attributes
   DWORD          m_dwRenderShard;     // render shard

   CMem           m_memObjEditAttribSetGroupSort;  // avoid malloc during render
   CListFixed     m_lObjEditAttribSetGroupChange;  // avoid malloc during render
};


/**************************************************************************************
CViewSocket - Called by the world object to notify application when objects have changed */

#define WORLDC_SELADD         0x0001      // new selection has been added
#define WORLDC_SELREMOVE      0x0002      // selection has been removed
#define WORLDC_OBJECTADD      0x0004      // new object has been added
#define WORLDC_OBJECTREMOVE   0x0008      // object has been removed
#define WORLDC_OBJECTCHANGESEL 0x0010     // a selected object has changed
#define WORLDC_OBJECTCHANGENON 0x0020     // a non-selected object has changed
#define WORLDC_SURFACECHANGED 0x0040      // a default surface has changed
#define WORLDC_LIGHTCHANGED   0x0080      // world lighting changed
#define WORLDC_CAMERAMOVED    0x0100      // if camera moves redraw in general, but not light buffers
#define WORLDC_CAMERACHANGESEL 0x0200     // special case - selected camera has changed
#define WORLDC_CAMERACHANGENON 0x0400     // special case - non selected camrea has changed

#define WORLDC_NEEDTOREDRAW   (WORLDC_OBJECTCHANGESEL | WORLDC_OBJECTCHANGENON)

// CViewSocket is a pure virtual class supported by all the view objects
// in the system. It's used for communication to the view objects from the world object.
class CViewSocket {
public:
   virtual void WorldChanged (DWORD dwChanged, GUID *pgObject) = 0;
      // tell the view that the world has changed. dwChanged is a bit-field
      // of WORLDC_XXX, indicating what has changed since the last call
      // if pgObject is not null then get an indication of a specific object that
      // changed. However, if the lots of objects have changed will send null.
      // if change message for something other than object then pgObject also null

   virtual void WorldUndoChanged (BOOL fUndo, BOOL fRedo) = 0;
      // tell the view that the world's undo/redo state has changed. (or
      // it might not have). fUndo is TRUE if undos are possible.
      // fRedo is TRUE if redos are possible

   virtual void WorldAboutToChange (GUID *pgObject) = 0;
      // tells the view than an object is about to change. Usually this is ignored,
      // except for when syncing up Scene with object. pgObject points to the object
      // about to change
};
typedef CViewSocket * PCViewSocket;


/*************************************************************************
CWorldSocket - Called by objects talking to the world. This virtual class
allows different bits of code to simlate the world - which may be
necessary when objects contain other objects */
class CWorldSocket {
public:
   virtual void NameSet (PWSTR pszName) = 0;
   virtual PWSTR NameGet (void) = 0;
   virtual BOOL DirtyGet (void) = 0;
   virtual void DirtySet (BOOL fDirty) = 0;
   virtual void RenderShardSet (DWORD dwRenderShard) = 0;
   virtual DWORD RenderShardGet (void) = 0;

   virtual BOOL NotifySocketAdd (PCViewSocket pAdd) = 0;
   virtual BOOL NotifySocketRemove (PCViewSocket pRemove) = 0;
   virtual void NotifySockets (DWORD dwChange, GUID *pgObject) = 0;

   virtual PCObjectSocket ObjectGet (DWORD dwObject) = 0; // get object
   virtual DWORD ObjectAdd (PCObjectSocket pObject, BOOL fDontRememberForUndo = FALSE,
      GUID *pgID = NULL) = 0;  // add an object
   virtual DWORD ObjectNum (void) = 0; // returns the number of objects
   virtual BOOL ObjectRemove (DWORD dwObject) = 0;
   virtual DWORD ObjectFind (PCObjectSocket pObject) = 0;
   virtual DWORD ObjectFind (GUID *pg) = 0;

   virtual void Clear (BOOL fOnlyObjects = TRUE) = 0;

   virtual PCMMLNode2 MMLTo (PCProgressSocket pProgress = NULL) = 0;
   virtual BOOL MMLFrom (PCMMLNode2 pNode, BOOL *pfFailedToLoad, PCProgressSocket pProgress = NULL) = 0;

   virtual BOOL SelectionRemove (DWORD dwObject) = 0;
   virtual BOOL SelectionAdd (DWORD dwObject) = 0;
   virtual BOOL SelectionExists (DWORD dwObject) = 0;
   virtual DWORD *SelectionEnum (DWORD *pdwNum) = 0;
   virtual BOOL SelectionClear (void) = 0;

   virtual BOOL BoundingBoxGet (DWORD dwObject, CMatrix *pm, PCPoint pCorner1, PCPoint pCorner2,
      DWORD dwSubObject = (DWORD)-1) = 0; // get the bounding box
   virtual BOOL WorldBoundingBoxGet (PCPoint pCorner1, PCPoint pCorner2, BOOL fIgnoreGround) = 0;

   virtual void ObjectAboutToChange (PCObjectSocket pObject) = 0;
   virtual void ObjectChanged (PCObjectSocket pObject) = 0;
   virtual void SurfaceAboutToChange (PWSTR pszScheme) = 0;
   virtual void SurfaceChanged (PWSTR pszCheme) = 0;
   virtual void ObjectCacheReset (void) = 0;

   virtual void UndoClear (BOOL fUndo, BOOL fRedo) = 0;
   virtual void UndoRemember (void) = 0;
   virtual BOOL UndoQuery (BOOL *pfRedo) = 0;
   virtual BOOL Undo (BOOL fUndoIt = TRUE) = 0;

   virtual PCMMLNode2 AuxGet (void) = 0;

   virtual void UsurpWorld (CWorldSocket *pUsurp, BOOL fSelect) = 0;
   virtual PCObjectSocket UsurpObject (DWORD dwObject) = 0;
   virtual void IntersectBoundingBox (PCMatrix mBoundingBoxToWorld, PCPoint pCorner1,
                                   PCPoint pCorner2, PCListFixed plistIndex) = 0;

   virtual BOOL VariableSet (PWSTR pszName, PWSTR pszData) = 0;
   virtual BOOL VariableRemove (PWSTR pszName) = 0;
   virtual PWSTR VariableGet (PWSTR pszName) = 0;

   virtual void SurfaceSchemeSet (PCSurfaceSchemeSocket pScheme) = 0;
   virtual PCSurfaceSchemeSocket SurfaceSchemeGet (void) = 0;

   virtual BOOL ObjEditorDestroy (void) = 0;
      // Causes all the editors (from custom objects) that are in the world to be
      // destoryed. Returns TRUE if all successful, FALSE if some fail (because
      // they data in them is dirty)

   virtual BOOL ObjEditorShowWindow (BOOL fShow) = 0;
      // All of the editors for all of the objects in the world will be shown
      // or hidden depending upon fShow

};
typedef CWorldSocket * PCWorldSocket;

/*************************************************************************
CWorld */


extern PWSTR gpszWSObjLocation; // 0 for ground (default), 1 for wall, 2 for ceiling
extern PWSTR gpszWSObjShowPlain;   // 0 for no, 1 for yes (default)
extern PWSTR gpszWSObjType;   // object type - RENDERSHOW_XYZ. Default RENDERSHOW_FURNITURE
extern PWSTR gpszWSObjEmbed; // object embedding - 0 for none, 1 for just stick, 2 for one layers, 3 for two layers


// OBJECTINFO is used in CWorld to keep track of each object
typedef struct {
   PCObjectSocket    pObject;    // pointer to the actual object

   BOOL              fIsBoundingBoxValid; // true if the bounding box is valid
   CMatrix           mBoundingMatrix;  // maxtrix that rotates the bounding box
   CPoint            Corner[2];        // two corners (pre m_BoundingMatrix)
   GUID              gGUID;            // GUID for the object
   PCListFixed       plSubObjectCorners;  // if has sub-objects, this is a list of all the corners (2 points)
} OBJECTINFO, *POBJECTINFO;

class CUndoPacket;

// CWorld - Manage all the objects
class DLLEXPORT CWorld : public CWorldSocket {
   //friend class CSurfaceScheme;

public:
   ESCNEWDELETE;

   CWorld (void);
   ~CWorld (void);

   virtual void NameSet (PWSTR pszName);
   virtual PWSTR NameGet (void);
   virtual BOOL DirtyGet (void);
   virtual void DirtySet (BOOL fDirty);
   virtual void RenderShardSet (DWORD dwRenderShard);
   virtual DWORD RenderShardGet (void);

   virtual BOOL NotifySocketAdd (PCViewSocket pAdd);
   virtual BOOL NotifySocketRemove (PCViewSocket pRemove);
   virtual void NotifySockets (DWORD dwChange, GUID *pgObject);

   virtual PCObjectSocket ObjectGet (DWORD dwObject); // get object
   virtual DWORD ObjectAdd (PCObjectSocket pObject, BOOL fDontRememberForUndo = FALSE,
      GUID *pgID = NULL);  // add an object
   virtual DWORD ObjectNum (void); // returns the number of objects
   virtual BOOL ObjectRemove (DWORD dwObject);
   virtual DWORD ObjectFind (PCObjectSocket pObject);
   virtual DWORD ObjectFind (GUID *pg);

   virtual void Clear (BOOL fOnlyObjects = TRUE);

   virtual PCMMLNode2 MMLTo (PCProgressSocket pProgress = NULL);
   virtual BOOL MMLFrom (PCMMLNode2 pNode, BOOL *pfFailedToLoad, PCProgressSocket pProgress = NULL);

   virtual BOOL SelectionRemove (DWORD dwObject);
   virtual BOOL SelectionAdd (DWORD dwObject);
   virtual BOOL SelectionExists (DWORD dwObject);
   virtual DWORD *SelectionEnum (DWORD *pdwNum);
   virtual BOOL SelectionClear (void);

   virtual BOOL BoundingBoxGet (DWORD dwObject, CMatrix *pm, PCPoint pCorner1, PCPoint pCorner2,
      DWORD dwSubObject = (DWORD)-1); // get the bounding box
   virtual BOOL WorldBoundingBoxGet (PCPoint pCorner1, PCPoint pCorner2, BOOL fIgnoreGround);

   virtual void ObjectAboutToChange (PCObjectSocket pObject);
   virtual void ObjectChanged (PCObjectSocket pObject);
   virtual void SurfaceAboutToChange (PWSTR pszScheme);
   virtual void SurfaceChanged (PWSTR pszCheme);
   virtual void ObjectCacheReset (void);

   virtual void UndoClear (BOOL fUndo, BOOL fRedo);
   virtual void UndoRemember (void);
   virtual BOOL UndoQuery (BOOL *pfRedo);
   virtual BOOL Undo (BOOL fUndoIt = TRUE);

   virtual PCMMLNode2 AuxGet (void);

   virtual void UsurpWorld (CWorldSocket *pUsurp, BOOL fSelect);
   virtual PCObjectSocket UsurpObject (DWORD dwObject);

   virtual void IntersectBoundingBox (PCMatrix mBoundingBoxToWorld, PCPoint pCorner1,
                                   PCPoint pCorner2, PCListFixed plistIndex);

   virtual BOOL VariableSet (PWSTR pszName, PWSTR pszData);
   virtual BOOL VariableRemove (PWSTR pszName);
   virtual PWSTR VariableGet (PWSTR pszName);

   virtual void SurfaceSchemeSet (PCSurfaceSchemeSocket pScheme);
   virtual PCSurfaceSchemeSocket SurfaceSchemeGet (void);

   virtual BOOL ObjEditorDestroy (void);
   virtual BOOL ObjEditorShowWindow (BOOL fShow);

   PCSurfaceScheme   m_pOrigSurfaceScheme;      // original surface scheme, used before SurfaceSchemeSet is called, and left around
   PCSurfaceSchemeSocket   m_pSurfaceScheme;     // surface scheme settings, initially set to origsurfacescheme

   BOOL  m_fKeepUndo;      // if TRUE (default), keep track of undo/redo, if FALSE, don't keep track of changes

   BOOL  m_fDirty;      // Keep track of changes since last save. App can set this to true/false.
                        // If any changes that cause undo/redo to happen then will be set to true automatically

private:
   void NotifySocketsUndo (void);

   CListFixed     m_listOBJECTINFO; // list of objects
   CListFixed     m_listSelected;   // list of DWORDs, sorted, of what's selected
   CListFixed     m_listViewSocket;   // list of pointers to world sockets that get notified of change
   CListFixed     m_listPCUndoPacket;  // list of undo packets. Lowest # index are oldest
   CListFixed     m_listPCRedoPacket;  // list of PCUndoPacket. Lowest # index are oldest
   CUndoPacket    *m_pUndoCur;          // current undo that writing into

   CListVariable  m_listVarName;    // list of variable names - wide strings
   CListVariable  m_listVarData;    // list of variable data - wide strings, associated with m_listVarName

   PCMMLNode2      m_pAux;           // auxiliary information
   WCHAR          m_szName[256];    // name of the world - file name, and displayed in views
   DWORD          m_dwLastFind;     // index from the last find - used to speed up ObjectFind(GUID)
   DWORD          m_dwRenderShard;  // render shard to use. Defaults to -1, so MUST call RenderShardSet()
};
typedef CWorld * PCWorld;

DLLEXPORT void GUIDGen (GUID *pg);

DLLEXPORT void GlobalFloorLevelsSet (PCWorldSocket pWorld, PCObjectSocket pIgnore, 
                           fp *pafLevel, fp fHigher);
DLLEXPORT BOOL GlobalFloorLevelsGet (PCWorldSocket pWorld, PCObjectSocket pThis, fp *pafLevel, fp *pfHigher);



/********************************************************************************
CRenderClip */
typedef struct {
   CPoint         Point;      // point
   WORD           wColor[3];  // RGB color
   CPoint         Normal;     // normal
   TEXTPOINT5     Texture;    // texture.
   DWORD          dwClipBits; // which planes it needs to be clipped agains, from CalcClipBits
   DWORD          dwOrigIndex;// original index in list of vertices. If a new point is added
                              // or point was changed, this will be -1
} CLIPPOLYPOINT, *PCLIPPOLYPOINT;

class DLLEXPORT CRenderClip {
public:
   ESCNEWDELETE;

   CRenderClip (void);
   ~CRenderClip (void);

   void Clear (void);
   BOOL CloneTo (CRenderClip *pTo);
   BOOL AddPlane (PCPoint pNormal, PCPoint pPoint);
   void CalcClipBits (DWORD dwClipMask, DWORD dwNum, PCPoint apPoints, DWORD *padwBits);
   void ClipLine (DWORD dwClipPlane,
                            const PCPoint pNC, const PCPoint pNormalNC, const WORD *pwColorNC,
                            const PTEXTPOINT5 pTextureNC,
                            PCPoint pC, PCPoint pNormalC, WORD *pwColorC,
                            PTEXTPOINT5 pTextureC);
   BOOL ClipPolygonOneSide (DWORD dwClipPlane, BOOL fColor, BOOL fNormal,
      BOOL fTexture, DWORD dwNumSrc, PCLIPPOLYPOINT pSrc,
      DWORD *pdwNumDest, PCLIPPOLYPOINT pDest);
   BOOL ClipPolygon (BOOL fColor, BOOL fNormal, BOOL fTexture,
                               DWORD dwNumSrc, PCLIPPOLYPOINT pSrc,
                               DWORD *pdwNumDest, PCLIPPOLYPOINT pDest);
   BOOL ClipPolygons (DWORD dwClipMask, PPOLYRENDERINFO pInfo);
   BOOL TrivialClip (DWORD dwNum, PCPoint paPoints, DWORD *pdwBitPlanes);
   DWORD ClipMask (void);

private:
   CListFixed        m_listCLIPPLANE;     // list of clip planes
   CMem              m_MemRemap;          // scratch memory for remapping points in ClipPolygons()
   CMem              m_MemClipBits;       // strach memory for remembers bits in ClipPolygons
   CMem              m_MemPoints;         // scratch for ClipPolygons
   CMem              m_MemNormals;        // same
   CMem              m_MemTextures;       // same
   CMem              m_MemColors;         // same
   CMem              m_MemPolygons;       // same
   CMem              m_MemVertices;       // same
   CImage            m_TempImage;         // just use to make sure gamma correction done
   CMem              m_memClipPolygonCPP; // to not put heaps of stuff on stack
   CMem              m_memClipPolygonsSrc;   // to not put heaps of stuff on stack
   CMem              m_memClipPolygonsDest;  // to not put heaps of stuff on stack
};
typedef CRenderClip *PCRenderClip;



/**********************************************************************************
CRenderSuper - Super-class that all renderers belong to.
*/

#define  CAMERAMODEL_FLAT              0x00  // isometric(?) view. No perspective.
#define  CAMERAMODEL_PERSPWALKTHROUGH  0x10  // perspective. Treat as a walkthough, as opposed to manupulate object
#define  CAMERAMODEL_PERSPOBJECT       0x11  // perspective. Tread as holding an object in hand and can rotate

// RENDERSHOW_XXX - One or more of the following
#define  RENDERSHOW_ALL                (-1)
#define  RENDERSHOW_WALLS              0x00000001
#define  RENDERSHOW_FLOORS             0x00000002
#define  RENDERSHOW_ROOFS              0x00000004
#define  RENDERSHOW_STAIRS             0x00000008
#define  RENDERSHOW_BALUSTRADES        0x00000010
#define  RENDERSHOW_PIERS              0x00000020
#define  RENDERSHOW_FRAMING            0x00000040
#define  RENDERSHOW_DOORS              0x00000080
#define  RENDERSHOW_WINDOWS            0x00000100
#define  RENDERSHOW_CABINETS           0x00000200
#define  RENDERSHOW_PLUMBING           0x00000400
#define  RENDERSHOW_FURNITURE          0x00000800
#define  RENDERSHOW_LANDSCAPING        0x00001000
#define  RENDERSHOW_GROUND             0x00002000
#define  RENDERSHOW_MISC               0x00004000
#define  RENDERSHOW_ELECTRICAL         0x00008000
#define  RENDERSHOW_VIEWCAMERA         0x00010000     // display where view cameras are
#define  RENDERSHOW_WEATHER            0x00020000     // sun, moon, clouds
#define  RENDERSHOW_APPLIANCE          0x00040000
#define  RENDERSHOW_ANIMCAMERA         0x00080000     // displays where animnation cameras are
#define  RENDERSHOW_PLANTS             0x00100000
#define  RENDERSHOW_CHARACTER          0x00200000

class CRenderSuper {
public:

   // set what image and world to use
   virtual DWORD ImageQueryFloat (void) = 0;
      // Returns 2 if the renderer uses a CFImage (in which case will need to call
      // CFImageSet), 1 if uses both CFImage and CImage,
      // or 0 if the renderer uses a  CImage (in which case call CImageSet)

   virtual BOOL CImageSet (PCImage pImage) = 0;
      // sets the image to use, overwriting old one, and getting new dimensions
      // This only works if ImageQueryFloat() returns FALSE

   virtual BOOL CFImageSet (PCFImage pImage) = 0;
      // sets the image to use, overwriting old one, and getting new dimensions
      // This only works if ImageQueryFloat() returns TRUE

   virtual void CWorldSet (PCWorldSocket pWorld) = 0;
      // specify the world object that the renderer is attached to

   // camera
   virtual void CameraFlat (PCPoint pCenter, fp fLongitude, fp fTiltX, fp fTiltY, fp fScale,
                                     fp fTransX, fp fTransY) = 0;
      // set the camera (and matrix) using using an isometric perspective

   virtual void CameraFlatGet (PCPoint pCenter, fp *pfLongitude, fp *pfTiltX, fp *pfTiltY, fp *pfScale, fp *pfTransX, fp *pfTransY = 0) = 0;
      // gets the isometric information

   virtual void CameraPerspWalkthrough (PCPoint pLookFrom, fp fLongitude = 0, fp fLatitude = 0, fp fTilt = 0, fp fFOV = PI / 4) = 0;
      // set camera to lookin from first person

   virtual void CameraPerspWalkthroughGet (PCPoint pLookFrom, fp *pfLongitude, fp *pfLatitude, fp *pfTilt, fp *pfFOV) = 0;
      // gets the walkthrough perspective info

   virtual void CameraPerspObject (PCPoint pTranslate, PCPoint pCenter, fp fRotateZ, fp fRotateX, fp fRotateY, fp fFOV) = 0; 
      // sets the camera to hold at arms length and rotate

   virtual void CameraPerspObjectGet (PCPoint pTranslate, PCPoint pCenter, fp *pfRotateZ, fp *pfRotateX, fp *pfRotateY, fp *pfFOV) = 0;
      // gets the object perspective info

   virtual DWORD CameraModelGet (void) = 0;
      // returns the current camera model, from CAMERAMODEL_XXX

   // draw
   virtual BOOL Render (DWORD dwWorldChanged, HANDLE hEventSafe, PCProgressSocket pProgress) = 0;
      // render with the current settings.
      // dwWorldChanges is a set of bits from WORLDC_XXX indicating what has changed since the last render
      // pProgress is a progesss object
      //HANDLE   hEventSafe - If not NULL, SetEvent() is called on this event when
      //         it's safe to start up another renderer in a different thread (since
      //         this one is no longer looking at the world data). NOTE: Don't
      //         change the world data until AFTER the function returns.

   // set attributes
   virtual fp ExposureGet (void) = 0;
      // returns the exposure level

   virtual void ExposureSet (fp fExposure) = 0;
      // sets the exposure level

   virtual COLORREF BackgroundGet (void) = 0;
      // returns the background color (where no objects appear)

   virtual void BackgroundSet (COLORREF cBack) = 0;
      // sets the background color

   virtual DWORD AmbientExtraGet (void) = 0;
      // returns a number of extra ambient lighting. 0xffff is full sunlight's worth

   virtual void AmbientExtraSet (DWORD dwAmbient) = 0;
      // sets the level for extra ambient lighting

   virtual DWORD RenderShowGet (void) = 0;
      // returns the flags indicating which objects are to be shown. See RENDERSHOW_XXX

   virtual void RenderShowSet (DWORD dwShow) = 0;
      // sets the flags indicating which objects are to be shown. Comination of RENDERSHOW_XXX

   virtual fp PixelToZ (DWORD dwX, DWORD dwY) = 0;
      // given an x and y, returns the z value for the pixel

   virtual void PixelToViewerSpace (fp dwX, fp dwY, fp fZ, PCPoint p) = 0;
      // given and x and y pixel (fractional), along with a z returend from pixeltoz,
      // this returns the point in viewer space

   virtual void PixelToWorldSpace (fp dwX, fp dwY, fp fZ, PCPoint p) = 0;
      // given and x and y pixel (fractional), along with a z returend from pixeltoz,
      // this returns the point in world space

   virtual BOOL WorldSpaceToPixel (const PCPoint pWorld, fp *pfX, fp *pfY, fp *pfZ = NULL) = 0;
      // given a point in world space, this returns the x and y pixel

   virtual void ShadowsFlagsSet (DWORD dwFlags) = 0;
      // set the m_dwShadowsFlags to dwFlags. Uses SF_XXX from CRenderTraditional

   virtual DWORD ShadowsFlagsGet (void) = 0;
      // set the m_dwShadowsFlags to dwFlags. Uses SF_XXX from CRenderTraditional
};
typedef CRenderSuper *PCRenderSuper;

/******************************************************************************
CRayObject */
class CRayObject;
typedef CRayObject *PCRayObject;
class CRenderRay;
typedef CRenderRay *PCRenderRay;
class CRenderTraditional;
typedef CRenderTraditional *PCRenderTraditional;



// CRTSURF - Surface information for ray tracing
typedef struct {
   WORD              wMinorID;            // minor object ID
   DWORD             wFill;               // so align ok
   PCTextureMapSocket pTexture;           // texture to use. If NULL no texture
   CMaterial         Material;            // used if solid color
} CRTSURF, *PCRTSURF;

// CRTPOINT - Compressed way of storing point for ray tracing
typedef struct {
   float                p[3];                // xyz... NOTE: Using float so wont be too much memory
} CRTPOINT, *PCRTPOINT;

// CRTNORMAL - Compressed way of storing normal for ray tracing
typedef struct {
   DWORD             dw;                  // (short) (WORD) ((dw & 0x3ff) << 6) = n[0]
                                          // (short) (WORD) (((dw >> 10) & 0x3ff) << 6) = n[1]
                                          // (short) (WORD) (((dw >> 20) & 0x3ff) << 6) = n[2]
} CRTNORMAL, *PCRTNORMAL;

// CRTVERTEXWORD and CRTVERTEXDWORD - For storing vertex in ray tracing
typedef struct {
   WORD             dwPoint;             // point index, 0-based index into POLYRENDERINFO.paPoints
   WORD             dwNormal;            // normal index. 0-based index into POLYRENDERINFO.paNormals
   WORD             dwColor;             // color index. 0-based index into POLYREDNERINFO.paColors
   WORD             dwTexture;           // texture index. 0-based index into POLYRENDERINFO.paTextures
} CRTVERTEXWORD, *PCRTVERTEXWORD;

typedef struct {
   DWORD             dwPoint;             // point index, 0-based index into POLYRENDERINFO.paPoints
   DWORD             dwNormal;            // normal index. 0-based index into POLYRENDERINFO.paNormals
   DWORD             dwColor;             // color index. 0-based index into POLYREDNERINFO.paColors
   DWORD             dwTexture;           // texture index. 0-based index into POLYRENDERINFO.paTextures
} CRTVERTEXDWORD, *PCRTVERTEXDWORD; // used if >= 64K items


// CRTTRIWORD and CRTTRIDWORD - For storing triangle information away for ray tracing
typedef struct {
   DWORD             dwIDPart;            // part ID
   WORD              fCanBackfaceCull;    // set to TRUE if can backface cull
   WORD              wFill;               // not used

   WORD              dwSurface;           // surface index. 0-based index into POLYRENDERINFO.paSurfaces
   WORD              adwVertex[3];        // vertex index
   CPoint            pBoundSphere;        // bounding spehere info, [0..2]=xyz, [3]=radius
   CMatrix           mBound;              // use for plane bounding test
#ifdef USEGRID
   DWORD             dwGridInfo;          // info about where it appears on the grid
#endif// USEGRID
} CRTTRIWORD, *PCRTTRIWORD;

typedef struct {
   DWORD             dwIDPart;            // part ID
   WORD              fCanBackfaceCull;    // set to TRUE if can backface cull
   WORD              wFill;               // not used

   DWORD             dwSurface;           // surface index. 0-based index into POLYRENDERINFO.paSurfaces
   DWORD             adwVertex[3];        // vertex index
   CPoint            pBoundSphere;        // bounding spehere info, [0..2]=xyz, [3]=radius
   CMatrix           mBound;              // use for plane bounding test
#ifdef USEGRID
   DWORD             dwGridInfo;          // info about where it appears on the grid
#endif// USEGRID
} CRTTRIDWORD, *PCRTTRIDWORD;



// CRTQUADWORD and CRTQUADDWORD - For storing quadrilateral information away for ray tracing
typedef struct {
   DWORD             dwIDPart;             // part ID
   WORD              fCanBackfaceCull;    // set to TRUE if can backface cull

   WORD              dwSurface;           // surface index. 0-based index into POLYRENDERINFO.paSurfaces
   //WORD              wFill;               // so DWORD aligned
   WORD              adwVertex[4];        // vertex index
   CPoint            pBoundSphere;        // bounding spehere info, [0..2]=xyz, [3]=radius
   CMatrix           amBound[2];          // use for plane bounding test
#ifdef USEGRID
   DWORD             dwGridInfo;          // info about where it appears on the grid
#endif// USEGRID
} CRTQUADWORD, *PCRTQUADWORD;

typedef struct {
   DWORD             dwIDPart;             // part ID
   WORD              fCanBackfaceCull;    // set to TRUE if can backface cull
   WORD              wFill;               // not used

   DWORD             dwSurface;           // surface index. 0-based index into POLYRENDERINFO.paSurfaces
   DWORD             adwVertex[4];        // vertex index
   CPoint            pBoundSphere;        // bounding spehere info, [0..2]=xyz, [3]=radius
   CMatrix           amBound[2];          // use for plane bounding test
#ifdef USEGRID
   DWORD             dwGridInfo;          // info about where it appears on the grid
#endif// USEGRID
} CRTQUADDWORD, *PCRTQUADDWORD;

// CRTSUBOBJECT - Information about a sub-obhect
typedef struct {
   CMatrix           mMatrix;             // apply to all points in sub-object, sub-object to world
   CMatrix           mMatrixInv;          // go from world space to sub-object space
   BOOL              fMatrixIdent;        // set to TRUE if the matrix is identity
   PCRayObject       pObject;             // sub-object
   CPoint            apBound[2];          // bounding box after matrix apply. [0] = min, [1]=max
   CPoint            pBoundSphere;        // spehere bounding box after matrix apply. [0..2] = xyz, [3] =radius
#ifdef USEGRID
   DWORD             dwGridInfo;          // info about where it appears on the grid
#endif// USEGRID
} CRTSUBOBJECT, *PCRTSUBOBJECT;

// CRTCLONE - Information about the clone
typedef struct {
   CPoint            apBound[2];          // bounding box for all clones noted in this structure
   CPoint            pBoundSphere;        // bounding box for all clones noted in this structure
   CMatrix           mMatrix;             // apply to all points of clone to convert from clone space to world space
   CMatrix           mMatrixInv;          // go from world space to sub-object space
#ifdef USEGRID
   DWORD             dwGridInfo;          // info about where it appears on the grid
#endif// USEGRID
} CRTCLONE, *PCRTCLONE;

// RTPF_XXX - When polygon numbers are stored in CRTRAYPATH (and others), will have the
// top 2 bits masked to indicate if mean polygon or quad
#define RTPF_FLAGMASK   0xc0000000        // mask to isolate flags for RTPF_XXX
#define RTPF_NUMMASK    (~RTPF_FLAGMASK) // so can isolate ouyt the numbers
#define RTPF_TRIANGLE   0x00000000        // if this then number in low bits is a triangle
#define RTPF_QUAD1      0x40000000        // low bits are a quad number, 1st triangle of quad intersected
#define RTPF_QUAD2      0x80000000        // low bits are a quad number, 2snd triangle of quad intersected

// CRTRAYREFLECT - Ray used for reflection or transmission
typedef struct {
   WORD              wX, wY;              // x and y pixel
   float             fBeamWidth;          // width of the beam at the start
   fp                afStart[3];          // starting location - make this accurate for intersection reasons
   float             afDir[3];            // direction. Normalized
   float             afColor[3];          // color scale by this much, 0..1 for amount that's reflected
   PCRayObject       pDontIntersect;      // dont test against dwDontIntersect in ray object pDontIntersect
   DWORD             dwDontIntersect;     // triangle/quad number not to intersect with or-ed with RTRF_XXX
} CRTRAYREFLECT, *PCRTRAYREFLECT;

// CRTRAYPATH - Structure used to hold information about a path to test the ray
typedef struct {
   fp                fBeamOrig;           // original beam width at the start of the ray
   fp                fBeamSpread;         // how much the beam spreads for every meter distance travelled
   CPoint            pStart;              // starting location
   CPoint            pDir;                // direction. NORMALIZED
   fp                fAlpha;              // alpha of the closest intersection. If no inter should be large, like 1000000000
   BOOL              fStopAtAnyOpaque;    // if TRUE sending ray to light source; if hit any opaque object then
                                          // might as well stop
   PCRayObject       pDontIntersect;      // dont test against dwDontIntersect in ray object pDontIntersect
   DWORD             dwDontIntersect;     // triangle/quad number not to intersect with or-ed with RTRF_XXX
   CMatrix           mToWorld;            // matrix that converts from coords that pStart and pDir in to world coords

   // information about intersection
   PCRayObject       pInterRayObject;     // ray object that intersected
   DWORD             dwInterPolygon;      // tirangle/quad number, or-ed with RTPF_XXXX
   CPoint            pInterStart;         // start vector that caused intersection
   CPoint            pInterDir;           // direction vector that caused intersection
   BOOL              fInterOpaque;        // if fStopAtAnyOpaque then this is filled with TRUE if intersection
                                          // was opaque, or FALSE if not opaque
   CMatrix           mInterToWorld;       // converts the vectors that pstart and pend to world coords
} CRTRAYPATH, *PCRTRAYPATH;


#define MINOBJECTFORGRID      100    // need at least this many objects before does grid
#define MINOBJECTFORGRIDNOCLONE 500 // need this many objects before grid without clone

#define RTGRIDDIM             8     // when objects divided into grid, this is the dimension for each side
#define RTGRIDBINS            (RTGRIDDIM * RTGRIDDIM * RTGRIDDIM) // number of bins
#define RTGRIDDWORD           ((DWORD) ((RTGRIDBINS+31) / 32)) // number of DWORDs needed to store grid
// CRTGRID - Store information about a grid
typedef struct {
   DWORD             adw[RTGRIDDWORD]; // bitfiels of RTGRIDDIM x RTGRIDDIM x RTGRIDDIM
} CRTGRID, *PCRTGRID;


// CRTSURFACE - Information about the surface where a ray intersects
typedef struct {
   PCRayObject       pInterRayObject;     // ray object that intersected
   DWORD             dwInterPolygon;      // tirangle/quad number, or-ed with RTPF_XXXX
   fp                fBeamWidth;          // how wide the beam is in meters

   DWORD             dwIDMajor;           // major ID
   DWORD             dwIDMinor;           // minor ID
   DWORD             dwIDPart;             // part ID
   //WORD              wFill;               // fill

   CPoint            pLoc;                // location in world space
   CPoint            pNorm;               // normal in world space (NORMALIZED)
   CPoint            pNormText;           // normal jittered by bump map (NORMALIZED)

   WORD              awColor[3];          // surface color
   WORD              wFill2;              // fill
   float             afGlow[3];           // amount to glow. Under full sunlight 0xffff = white

   CMaterial         Material;            // material info

   BOOL              fReflect;            // set to TRUE if there's a reflection beam
   CPoint            pReflectDir;         // reflection direction
   float             afReflectColor[3];   // reflection color, 0..1, as a percentage of what follows

   BOOL              fRefract;            // set to TRUE if there's a refraction beam
   CPoint            pRefractDir;         // refraction direction
   float             afRefractColor[3];   // refraction color, 0..1, as a percentage of what follows
} CRTSURFACE, *PCRTSURFACE;

#define CRTBOUND        3  // number of types of bounding volumes, 0=eye to object, 1=to light, 2=reflect/refrace

class CRayObject {
   friend class CRenderTraditional;

public:
   ESCNEWDELETE;

   CRayObject (DWORD dwRenderShard, PCRenderRay pRenderRay, PCRenderTraditional pRenderTrad, BOOL fIsClone);
   ~CRayObject (void);

   void Clear (BOOL fFreeExist = TRUE);
   BOOL PolygonsSet (PPOLYRENDERINFO pInfo, PCMatrix pMatrix, PCWorldSocket pWorld);
   BOOL SubObjectAdd (PCRayObject pAdd, PCMatrix pMatrix = NULL);
   BOOL ClonesAdd (GUID *pgCode, GUID *pgSub, PCMatrix paMatrix, DWORD dwNum);
   void CalcBoundBox (void);  // note: must be called after polygonset, subobjectadd, or clonesadd
   BOOL RayIntersect (DWORD dwThread, DWORD dwBound, PCRTRAYPATH pRay, BOOL fNoBoundSphere);
   BOOL LightsAdd (PLIGHTINFO pLight, DWORD dwNum);
   PLIGHTINFO LightsGet (DWORD *pdwNum);
   BOOL SurfaceInfoGet (DWORD dwThread, PCRTRAYPATH pRay, PCRTSURFACE pSurf, BOOL fOnlyWantRefract = FALSE);
   BOOL BoundingVolumeInit (DWORD dwThread, DWORD dwBound, PCPoint papStart, PCPoint papEnd,
      PCPoint papNewBound);

   DWORD             m_dwRenderShard;
   DWORD             m_dwID;        // major object ID, so if ray hits is used for rendering
   CPoint            m_apBound[2];  // bounding box. [0] = min points, [1] = max poitns.
                                    // do not change. Kept up to date by object itself
   CPoint            m_pBoundSphere;   // [0..2] are xyz center of sphere, [3] = radius
                                    // do not change. Kept up to date by object itself

   BOOL              m_fDontDelete; // if set, then dont delete sub-objects when clear, special
                                    // function for storing world objects

private:
   BOOL RayIntersectPoly (DWORD dwThread, PCRTGRID pGrid, PCRTRAYPATH pRay, DWORD dwRay, BOOL fNoBoundSphere);
   BOOL RayIntersectSub (DWORD dwThread, DWORD dwBound, PCRTGRID pGrid, PCRTRAYPATH pRay,
      DWORD dwSub, BOOL fNoBoundSphere);
   BOOL RayIntersectClone (DWORD dwThread, PCRTGRID pGrid, PCRTRAYPATH pRay,
      DWORD dwSub, BOOL fNoBoundSphere);
   void CalcPolyBound (BOOL fTriangle, DWORD dwNum);
#ifdef USEGRID
   DWORD GridAddPoly (PCRTPOINT p1, PCRTPOINT p2, PCRTPOINT p3, PCRTPOINT p4,
                               PCMatrix pmWorldToGrid, PCListFixed plCRTGRID);
   DWORD GridAddBox (PCPoint papBound,
                               PCMatrix pmWorldToGrid, PCListFixed plCRTGRID);
   BOOL TestAgainstGrid (PCRTGRID pLine, DWORD dwGridInfo);
#endif // USEGRID

   // polygons
   PCMem             m_pmemPoly;            // memory used for storing the polygons
   DWORD             m_dwPolyDWORD;         // if TRUE the vertex and polygon info is a DWORD, else WORD
   DWORD             m_dwNumSurfaces;       // number of surfaces in paSurfaces
   PCRTSURF          m_pSurfaces;           // pointer to memory, an array of surfaces, PCRTSURF.
   PRENDERSURFACE    m_pRENDERSURFACE;      // pointer to memory, an array of RENDERSURFACE. Only used if pRenderTrad
   DWORD             m_dwNumPoints;         // number of points in paPoints
   PCRTPOINT         m_pPoints;             // pointer to memory, array of points, PCRTPOINT
   DWORD             m_dwNumNormals;        // number of normals in paNormals
   PCRTNORMAL        m_pNormals;            // pointer to memory, arrya of normals, PCRTNORMAL
                                            // NOTE: Already normalized
   DWORD             m_dwNumTextures;       // number of textures in paTextures
   PTEXTPOINT5       m_pTextures;           // pointer to memory, arrya of textures, PTEXTUREPOINT
   DWORD             m_dwNumColors;         // number of colors in paColors
   COLORREF          *m_pColors;            // pointer to memory, array of colors, COLORREF
   DWORD             m_dwNumVertices;       // number of Vertices
   PCRTVERTEXWORD    m_pVertices;           // pointer to memory, array of Vertices, either PCRTVERTEXWORD, or PCRTVERTEXDWORD depending on m_dwPolyDWORD
   DWORD             m_dwNumTri;            // number of triangles to draw
   PCRTTRIWORD       m_pTri;                // pointer to memory, array/list of triangles. PCRTTRIWORD or PCRTTRIDWORD
   DWORD             m_dwNumQuad;           // number of quads to draw
   PCRTQUADWORD      m_pQuad;               // pointer to memory, array/list of quads. PCRTTRIWORD or PCRTTRIDWORD

   PCRenderRay       m_pRenderRay;           // ray trace object. can be NULL if m_pRenderTrad is NOT null.
   PCRenderTraditional m_pRenderTrad;        // traditioanl render. can be NULL if m_pRenderRay is NOT null

   // sub-objects and clones
   CListFixed        m_lCRTSUBOBJECT;        // list of sub-objects
   CListFixed        m_lCRTCLONE;            // list of clones
   CListFixed        m_lLIGHTINFO;           // lights
   PCRayObject       m_pCloneObject;         // object that's the clone
   GUID              m_gCloneCode;           // object ID code
   GUID              m_gCloneSub;            // object ID sub

   // quick test for last intersect
   DWORD             m_adwLastObject[MAXRAYTHREAD][CRTBOUND];
   CListFixed        m_alInBound[MAXRAYTHREAD][CRTBOUND]; // list of XYZ indicating which sub-objects are
                                             // in the bounding volume, sorted by order of occurence
                                             // if polygons XYZ=DWORD, else they're CRTINBOUND structures
#ifdef USEGRID
   CListFixed        m_lCRTGRID;             // list of grids used to speed up intersections
#endif // USEGRID
   CMem              m_memGridSorted;        // memory containing array of (max # objects) x sizeof (DWORD) x 3(xyz),
                                             // for order of object test given ray apporach from x=0,y=0,or z=0
   // misc
   BOOL              m_fIsClone;             // set to TRUE the the object is a clone

   CListFixed        m_lCalcBoundBoxSort;    // to minimize mallocs
   CListFixed        m_alBoundingVolumeInitScratch[MAXRAYTHREAD]; // to minimize mallocs
};



/**********************************************************************************
CRenderRay */
typedef struct {
   GUID              gID;        // GUID identifying object
   PCRayObject       pObject;    // object that makes this one
   PCObjectSocket    pos;        // pointer to object
   BOOL              fDirty;     // set to TRUE if object is dirty and needs reloading
   BOOL              fVisible;   // set to TRUE if is visible
} CRTOBJECT, *PCRTOBJECT;

// CRTRAYTOLIGHTHDR - header for the ray to light
typedef struct {
   DWORD             dwNumLight; // number of lights following this
   WORD              wX, wY;     // x and y pixel that this will affect
   fp                afSurfLoc[3];  // surface xyz, make this fp so that have max accuracy for shadows
   float             fBeamWidth;    // width of the beam at the point going to light
   PCRayObject       pDontIntersect;      // dont test against dwDontIntersect in ray object pDontIntersect
   DWORD             dwDontIntersect;     // triangle/quad number not to intersect with or-ed with RTRF_XXX
} CRTRAYTOLIGHTHDR, *PCRTRAYTOLIGHTHDR;

// CRTRAYTOLIGHT - stores information for a ray to light
typedef struct {
   DWORD             dwLight;       // light number, so can group all rays to one light together
   float             afLightLoc[3]; // xyz light location
   float             afColor[3];    // rgb color (in lumens) if there are no obstructions
} CRTRAYTOLIGHT, *PCRTRAYTOLIGHT;

class CRenderRay;
typedef CRenderRay *PCRenderRay;

typedef struct {
   PCRenderRay          pRay;    // ray object
   DWORD                dwThread;   // thread ID, from 0..MAXRAYTHREAD-1
   HANDLE               hThread; // handle to the thread
   HANDLE               hSignal; // set to cause bucket to process another buffer
   HANDLE               hDone;   // flagged by bucket to indicate that buffer done
   BOOL                 fWantQuit;  // will be set to TRUE if the thread should quit
} CRTBUCKETINFO, *PCRTBUCKETINFO;

class DLLEXPORT CRenderRay : public CRenderSuper, public CRenderSocket,
   public CViewSocket {
public:
   ESCNEWDELETE;

   // from CRenderSuper
   virtual DWORD ImageQueryFloat (void);
   virtual BOOL CImageSet (PCImage pImage);
   virtual BOOL CFImageSet (PCFImage pImage);
   virtual void CWorldSet (PCWorldSocket pWorld);
   virtual void CameraFlat (PCPoint pCenter, fp fLongitude, fp fTiltX, fp fTiltY, fp fScale,
                                     fp fTransX, fp fTransY);
   virtual void CameraFlatGet (PCPoint pCenter, fp *pfLongitude, fp *pfTiltX, fp *pfTiltY, fp *pfScale, fp *pfTransX, fp *pfTransY = 0);
   virtual void CameraPerspWalkthrough (PCPoint pLookFrom, fp fLongitude = 0, fp fLatitude = 0, fp fTilt = 0, fp fFOV = PI / 4);
   virtual void CameraPerspWalkthroughGet (PCPoint pLookFrom, fp *pfLongitude, fp *pfLatitude, fp *pfTilt, fp *pfFOV);
   virtual void CameraPerspObject (PCPoint pTranslate, PCPoint pCenter, fp fRotateZ, fp fRotateX, fp fRotateY, fp fFOV) ;
   virtual void CameraPerspObjectGet (PCPoint pTranslate, PCPoint pCenter, fp *pfRotateZ, fp *pfRotateX, fp *pfRotateY, fp *pfFOV);
   virtual DWORD CameraModelGet (void);
   virtual BOOL Render (DWORD dwWorldChanged, HANDLE hEventSafe, PCProgressSocket pProgress);
   virtual fp PixelToZ (DWORD dwX, DWORD dwY);
   virtual void PixelToViewerSpace (fp dwX, fp dwY, fp fZ, PCPoint p);
   virtual void PixelToWorldSpace (fp dwX, fp dwY, fp fZ, PCPoint p);
   virtual BOOL WorldSpaceToPixel (const PCPoint pWorld, fp *pfX, fp *pfY, fp *pfZ = NULL);
   virtual fp ExposureGet (void);
   virtual void ExposureSet (fp fExposure);
   virtual COLORREF BackgroundGet (void);
   virtual void BackgroundSet (COLORREF cBack);
   virtual DWORD AmbientExtraGet (void);
   virtual void AmbientExtraSet (DWORD dwAmbient);
   virtual DWORD RenderShowGet (void);
   virtual void RenderShowSet (DWORD dwShow);
   virtual void ShadowsFlagsSet (DWORD dwFlags);
   virtual DWORD ShadowsFlagsGet (void);


   // from CRenderSocket
   virtual BOOL QueryWantNormals (void);    // returns TRUE if renderer wants normal vectors
   virtual BOOL QueryWantTextures (void);    // returns TRUE if renderer wants textures
   virtual fp QueryDetail (void);       // returns detail resoltion (in meters) that renderer wants now
   virtual void MatrixSet (PCMatrix pm);    // sets the current operating matrix. Set NULL implies identity.
   virtual void PolyRender (PPOLYRENDERINFO pInfo);
   virtual BOOL QuerySubDetail (PCMatrix pMatrix, PCPoint pBound1, PCPoint pBound2, fp *pfDetail);
   virtual BOOL QueryCloneRender (void);
   virtual BOOL CloneRender (GUID *pgCode, GUID *pgSub, DWORD dwNum, PCMatrix pamMatrix);

   // from CViewSocket
   virtual void WorldChanged (DWORD dwChanged, GUID *pgObject);
   virtual void WorldUndoChanged (BOOL fUndo, BOOL fRedo);
   virtual void WorldAboutToChange (GUID *pgObject);

   // ray tracer specific
   void QuickSetRay (DWORD dwQuality, DWORD dwStrobe, DWORD dwAnti);
   PCRayObject CloneGet (GUID *pgCode, GUID *pgSub, PCPoint papBound);
   BOOL CloneRelease (GUID *pgCode, GUID *pgSub);

   // other
   CRenderRay (DWORD dwRenderShard);
   ~CRenderRay (void);
   void BucketThread (DWORD dwThread); // dont call this extnerally

   DWORD             m_dwRenderShard;
   DWORD             m_dwShadowsFlags;    // flags for shadows mode, SF_XXX
   DWORD             m_dwSpecial;      // passed to object for special rendering flags. See ORSPECIAL_XXX
   BOOL              m_fRandom;        // if TRUE then pixels and bounces are randomized
   DWORD             m_dwSuperSample;  // amount of supersampling to do. 1+, 1 = 1x1, 2=2x2, etc.
   DWORD             m_dwMaxThreads;   // maximum number of threads to use for this instance
   DWORD             m_dwMaxReflect;   // maximum reflection and refraction paths
   BOOL              m_fAntiEdge;      // if TRUE then detects edges and anti-aliases them
   fp                m_fScaleBeam;     // beam widths are scaled by this amount... useful for motion
                                       // blur to ensure textures arent over-antialiased
   fp                m_fMinLight;      // if the amount of light going to a pixel is less than this
                                       // then throw out. Uusally 1.0 / 0xffff.
   int               m_iPriorityIncrease; // how much to increase or decrease the thread priority

private:
   BOOL SyncUpObjects (void);
   BOOL RenderDirtyObjects (void);
   void DirtyAllObjects (void);
   BOOL CalcPixelsForBucket (RECT *pr, WORD *pawScore, WORD wScoreThresh,
                                      fp fColorScale, PCMem pMem, DWORD *pdwRays);
   HANDLE CreateBucketThread (DWORD dwThread);
   void TransformToScreen (DWORD dwNum, PCPoint paPoints);
   BOOL RayIntersect (DWORD dwThread, DWORD dwBound, PCRTRAYPATH pRay);
   BOOL PixelFromSurface (DWORD dwThread, DWORD dwX, DWORD dwY, PCPoint pEye, float *pafScale, PCRTSURFACE pSurf);
   void CalcAmbientLight (void);
   BOOL IlluminationFromLight (const PCRTSURFACE pSurf, const PLIGHTINFO pLight, const PCPoint pEyeNorm,
         float *pafIllum);
   void LightIntensity (const PLIGHTINFO pLight, const PCPoint pLoc, float *pafColor, PCPoint pDir);
   PCRTRAYTOLIGHT QueueRayToLight (DWORD dwThread, DWORD dwX, DWORD dwY, const PCPoint pSurf, DWORD dwNum,
      fp fBeamWidth, const PCRayObject pDontIntersect, DWORD dwDontIntersect);
   BOOL QueueReflect (DWORD dwThread, DWORD dwX, DWORD dwY,  const float *pafColorScale,
                               const PCRTRAYPATH pRay, const PCRTSURFACE pSurf);
   void CalculateRayToLight (DWORD dwThread);
   void CalculateRayToLight (DWORD dwThread, const PCRTRAYTOLIGHTHDR ph, const PCRTRAYTOLIGHT pr);
   void CalculateReflect (DWORD dwThread, BOOL fMoreReflect);
   BOOL RayPassThroughLeaf (const PCRTRAYPATH pRay, const PCRTSURFACE pSurf);

   PCWorldSocket  m_pWorld;         // world using. NULL if none set
   PCFImage       m_pImage;         // image that drawing to. NULL if none set
   DWORD          m_dwCameraModel;  // CAMERAMODEL_XXX.. current camera model
   PCProgressSocket m_pProgress;    // progress bar to use while rendering
   fp             m_fExposure;      // amount to expose image to, in  lumens
   COLORREF       m_cBackground;    // background color
   DWORD          m_dwAmbientExtra; // extra ambient light to add to scene
   DWORD          m_dwRenderShow;      // bits indicating what will show when render
   float          m_afAmbient[3];   // ambient light, in lumens
   CListFixed     m_lLIGHTINFO;     // list of lights

   CRenderMatrix  m_RenderMatrix;         // contains internal rendering matrix stuff
   fp             m_afCenterPixel[2];     // center of image. 1/2 m_pImage->m_dwWidth and m_pImage->m_dwHeight
   fp             m_afScalePixel[2];      // scaling of pixels so -1 to 1 goes to 0 to m_dwImageWidth,height
   fp             m_fCameraFlatLongitude; // longitude for flat camerea
   fp             m_fCameraFlatTiltX;     // tilt X for flat camera
   fp             m_fCameraFlatTiltY;     // tilt Y for flat camera
   fp             m_fCameraFlatScale;     // scale for flat camera
   fp             m_fCameraFlatTransX;    // translate for flat camera
   fp             m_fCameraFlatTransY;    // translate for flat camera
   CPoint         m_CameraFlatCenter;     // center of rotation for flat view
   CPoint         m_CameraObjectCenter;   // center of rotation for object view
   CMatrix        m_CameraMatrixRotAfterTrans;  // filled in by Camera setting functions. This is the rotation matrix and second translation that occurs AFTER
                                       // the camera's translation matrix. Don't modify this because won't do antyhing. But, can
                                       // get the inverse and use for dragging objcts around
   CMatrix        m_CameraMatrixRotOnlyAfterTrans;  // Only the rotation matrix part of m_CamereaMatrixRotAfterTrans
   fp             m_fCameraFOV;           // field of view for perspective models
   CPoint         m_CameraLookFrom;       // look from vector for walkthrough
   fp             m_fCameraLongitude;     // longitude for walkthrough
   fp             m_fCameraLatitude;      // latitude for walkthrough
   fp             m_fCameraTilt;          // tilt for walkthrough
   CPoint         m_CameraObjectLoc;      // for object perspective, location of the object
   fp             m_fCameraObjectRotX;    // rotation around X for object perspective
   fp             m_fCameraObjectRotY;    // rotation around Y for object perspective
   fp             m_fCameraObjectRotZ;    // rotation around Z for object perspective

   // list of objects
   CListFixed     m_lCRTOBJECT;           // list of objects
   PCRayObject    m_pRayObjectWorld;      // list of all objects in the world
   CListFixed     m_lCRTCLONECACHE;       // list of clones stored by ray object
   DWORD          m_dwCloneID;            // so each clone-type has a different ID

   // for rendering
   CMatrix        m_mRenderSocket;        // matrix from render socket
   PCRayObject    m_pRayCur;              // current ray object that adding to
   BOOL           m_fCloneRender;         // set to TRUE if rendering into a clone, FALSE a normal object
   CMem           m_amemCRTCAMERARAY[MAXRAYTHREAD];   // memory for storing the rays for the bucket
   DWORD          m_adwCameraRayNum[MAXRAYTHREAD];    // number of rays in m_amemCRTCAMERARAY
   CRTBUCKETINFO  m_aBucket[MAXRAYTHREAD];   // used to pass information to ray thread
   CMem           m_amemRayToLight[MAXRAYTHREAD];  // stores the rays going to light sources
   fp             m_fBeamSpread;          // for every meter beam travels, radius increases this much (in meters)
   fp             m_fBeamBase;            // beam initially begins at this width (in meters)
   CListFixed     m_alCRTRAYREFLECT[MAXRAYTHREAD][2]; // ping-pong buffer for reflection and refraction rays
   DWORD          m_adwRayReflectBuf[MAXRAYTHREAD];   // 0 or 1, for which buffer currently writing to for ping pong
   fp             m_fMinLightAnt;         // minimum amount of light, taking into account supersampling
   fp             m_fMinLightLumens;      // miniumu light taking into account exposure
   fp             m_fMinLightAntLarge;    // minlightant x 0xffff
};

/**********************************************************************************
CRenderTraditional */

DLLEXPORT void RendCPSizeSet (DWORD dwSize);
DLLEXPORT DWORD RendCPSizeGet (void);

/* CRenderTraditional - traditional renderer */
#define  RENDERMODEL_LINEMONO          0x00  // draw with a monochomatic line, color specified by m_cMonoLine
#define  RENDERMODEL_LINEAMBIENT       0x01  // line rendering. Use color as specified by polygons
#define  RENDERMODEL_LINESHADED        0x02  // line rendering. Use shading as specified by normals, etc.
#define  RENDERMODEL_LINETEXTURE       0x03  // same as RENDERMODEL_LINESHADED
#define  RENDERMODEL_LINESHADOWS       0x04  // know idea what this will do
#define  RENDERMODEL_SURFACEMONO       0x10  // surface rendering. Use m_cMono for the color of the surfaces
#define  RENDERMODEL_SURFACEAMBIENT    0x11  // surface rendering. Ambient light only
#define  RENDERMODEL_SURFACESHADED     0x12  // sufrace rendering. Include shading
#define  RENDERMODEL_SURFACETEXTURE    0x13  // surface rendering. Include textures.
#define  RENDERMODEL_SURFACESHADOWS    0x14  // draw in shadows

#define  RM_MONO                       0x00  // look at low nibble
#define  RM_AMBIENT                    0x01
#define  RM_SHADED                     0x02
#define  RM_TEXTURE                    0x03
#define  RM_SHADOWS                    0x04


// Shadows flags, for m_dwShadowsFlags.
#define  SF_NOSHADOWS                  0x0001   // dont have any objects cast shadows
#define  SF_NOSPECULARITY              0x0002   // dont render any specularity
#define  SF_LOWTRANSPARENCY            0x0004   // reduced transparency
#define  SF_NOBUMP                     0x0008   // dont draw bump maps
#define  SF_LOWDETAIL                  0x0010   // draw objects in 1/4 lower detail
#define  SF_TEXTURESONLY               0x0020   // max out at textures, no shadows mode
#define  SF_TWOPASS360                 0x1000   // used for 360 render object, causing it to do 2 passes, 1st one is very fast
#define  SF_NOSUPERSAMPLE              0x2000   // disabled supersampling and blurring to get rid of jaggies

typedef struct {
   DWORD       dwID;       // unique indentifier
   CPoint      ap[3];      // 3 points. Going in clockwise direction. Right-hand rule points to clipped side
} RTCLIPPLANE, *PRTCLIPPLANE;

typedef struct {
   WORD  wRed;          // 0..65536 red
   WORD  wGreen;
   WORD  wBlue;
   WORD  wIntensity;    // sometimes attach the intensity of the light here for texture maps
} GCOLOR, *PGCOLOR;

// RPPIXEL - Used for painting in 3d
typedef struct {
   float       afLoc[3];   // location of the point in XYZ. World space.
   float       afNorm[3];  // XYZ normal (world space). Already normalized
   float       afHV[2];    // horionztal and vertical texture values
   DWORD       dwTextIndex;   // index into the texture - 1. Or 0 if not textures
} RPPIXEL, *PRPPIXEL;

// RPTEXTINFO - Attached to the RPPIXEL, this is the textures that can be painted on
typedef struct {
   GUID        gCode;      // texture code
   GUID        gSub;       // texture sub-code
} RPTEXTINFO, *PRPTEXTINFO;

// ROINFO is used to keep track of the object when sorting objects by Z, etc.
typedef struct {
   DWORD             dwNumber;   // object number
   PCObjectSocket    pObject; // object
   BOOL              fCanUseZTest;  // set to TRUE if aPoint[2] is valid
   CPoint            aPoint[2];  // min and max bounding box, in screen coords
   DWORD             dwClipPlanes;  // clipping planes that need to check against
   fp                fDetail;    // detail that need for the object
   fp                fZNear;     // closest Z. Set to 0 if !fCanUseZTest
   BOOL              fTrivialClip;   // if TRUE then can trivial clip-out the object by bounding box
   BOOL              fVerySmall; // TRUE if object is smaller than requied detail
   BOOL              fSelected;  // set to TRUE if object is selected, FALSE if not
   DWORD             dwSubObject;   // normally -1, but if object supports sub-objects then multiple
} ROINFO, *PROINFO;


typedef struct {
   DWORD    dwNumObjects;     // number of objects in world
   DWORD    dwRenderObject;   // number of times called render object - think about rendering
   DWORD    dwObjTrivial;     // objects that trivial clip because out of screen space
   DWORD    dwObjCantUseZTest;   // objects that want to use Z test on but which cant
   DWORD    dwObjCovered;     // number of objects completely covered, so dont draw
   DWORD    dwObjNotCovered;  // number of objects not covered, so ended calling object->Render()
   DWORD    dwBoundingBoxGet; // number of times BoundingBoxGet() is forced to ask object
   DWORD    dwPolyBackface;   // number of polygons that backface culled
   BOOL     fRedrawSel;       // set to TRUE if redrew the selection
   BOOL     fRedrawNonSel;    // set to TRUE if redrew non-selection
   BOOL     fRedrawAll;       // set to TRUE if redrew everything together

   // from CImage
   DWORD    dwNumPoly;        // number of polygons drawn (will be subdivided to triangles)
   DWORD    dwNumTri;         // number of triangles drawn (sometimes more than one tirangle per polygon)
   DWORD    dwTriCovered;     // number of triangles eliminated because completely covered
   DWORD    dwNumHLine;       // number of horizontal lines drawn (for filling in triangles)
   DWORD    dwPixelRequest;   // number of pixels requested to be drawn by HorzLine(), but not necessarily calculated
   DWORD    dwPixelTrivial;   // pixels trivially rejected because all pixels in line behind
   DWORD    dwPixelBehind;    // number of pixels rejected because behind
   DWORD    dwPixelTransparent;  // number of pixels rejected because transparent
   DWORD    dwPixelWritten;   // number of pixels actually written
   DWORD    dwPixelOverwritten;  // number of pixels overwritten (was something there already)
   DWORD    dwNumDiagLine;    // number of diagonal lines drawn (for wireframe)
} RENDERSTATS, *PRENDERSTATS;
#ifdef _TIMERS
extern RENDERSTATS gRenderStats;
#endif

// RENDTHREADINFO - Information for a thread in the traditional rendering engine
class CRenderTraditional;
typedef CRenderTraditional *PCRenderTraditional;
typedef struct {
   PCRenderTraditional pRend; // point back to the renderer
   DWORD       dwThread;      // thread number, 0+
   HANDLE      hThread;       // handle for the thread
   HANDLE      hEvent;        // event that should set to wake up
   BOOL        fSuspended;    // set to TRUE if suspended and waiting for hEven to go off

   BOOL        fTempSuspendWant; // set if want to temporarily suspend
   BOOL        fTempSuspendAcknowledge;   // set by thread to acknowledge that temporarily suspended
} RENDTHREADINFO, *PRENDTHREADINFO;


// RENDTHREADBUF - Information on a buffer to be rendered
typedef struct {
   BOOL        fValid;        // set to TRUE if there's data in here
   BOOL        fClaimed;      // set to TRUE if claimed and being worked on
   DWORD       dwAge;         // age of the buffer... used to make sure that old buffers aren't ignored forever
   RECT        rPixels;       // pixels covered by the rectangle
   PBYTE       pMem;          // memory with data
   DWORD       dwUsed;        // size of the data available and filled in
   DWORD       dwMax;         // maximum available in this buffer.
   DWORD       dwPolyArea;    // area of all polygons added
   BOOL        fPoly;         // TRUE if claimed for polygon, FALSE for shadows
} RENDTHREADBUF, *PRENDTHREADBUF;

#define BUFPERTHREAD       4     // number of buffers cached up per thread, BUGFIX - was 3
   // BUGFIX - reduced from 5 to 2 because doubled m_dwThreads
   // BUGFIX - Upped to 4 to see if speed difference

// RENDTHREADPOLY - Information for drawing a polygon multithreaded
typedef struct {
   DWORD          dwAction;      // action... 0 for solid polygon, 1 for lines (only for CImage and CZImage), 13 for RENDERTHREADCLONE
   DWORD          dwNum;         // number of points, and IHLEND structures that follow
   IDMISC         Misc;          // misc info
   BOOL           fSolid;        // set to TRUE if sold
   PCTextureMapSocket pTexture;  // texture to use
   fp             fGlowScale;    // glow scale
   BOOL           fTriangle;     // set to TRUE if force to convert to triangle
} RENDTHREADPOLY, *PRENDTHREADPOLY;

// RENDERTHREADCLONE - Information needed to render a clone multithreaded
typedef struct {
   DWORD          dwAction;      // action... 0 for solid polygon, 1 for lines (only for CImage and CZImage), 13 for RENDERTHREADCLONE
   DWORD          dwNum;         // number of matricies, CMatrix following at the end
   GUID           gCode;         // object code
   GUID           gSub;          // object sub-code
   DWORD          dwWantClipThis;   // clip flags for object
   DWORD          dwObjectID;    // clip flags from object
} RENDERTHREADCLONE, *PRENDERTHREADCLONE;

// RENDTHREADSPOLY - Information for drawing a polygon multithreaded, shader version
typedef struct {
   DWORD          dwAction;      // action... 10 for solid polygon, 11 for lines (only for CFImage)
   DWORD          dwNum;         // number of points, and ISHLEND structures that follow
   ISDMISC        Misc;          // misc info
   BOOL           fTriangle;     // set to TRUE if force to convert to triangle
} RENDTHREADSPOLY, *PRENDTHREADSPOLY;


// SCANLINEINTERP - Optimization for interpolating a point across a scanline
typedef struct {
   BOOL           fUse;                // set to TRUE if use this
   BOOL           fHomogeneous;        // if TRUE then homogeneous coords (4 points), else 3 points
   BOOL           fBaseDeltaIsZero;    // set to TRUE if the base delta is 0, so dont need to interpolate
   double         afBaseLeft[4];       // base value on the left side
   double         afPerMeterLeft[4];   // per-meter value on the left side
   double         afBaseDelta[4];      // how much the base changes with each pixel
   double         afPerMeterDelta[4];  // how much the per-meter changes per pixel
   double         afBaseCur[4];        // current base value, incremented
   double         afPerMeterCur[4];    // current per-meter value, incremeented
} SCANLINEINTERP, *PSCANLINEINTERP;


// SCANLINEOPT - Structure containing scanline optimizations
typedef struct {
   SCANLINEINTERP aSLI[3];             // scanlineinterp for previous ([0]), this ([1]), and next([2]) scanline
   // PCPoint        apaScanLineLoc[3];   // locations for the Z-depth buffer of the scanlines
   CPoint         apBaseLeft[3];       // location of scanline
   CPoint         apBaseRight[3];      // location
   CPoint         apBasePlusMeterLeft[3]; // location
   CPoint         apBasePlusMeterRight[3];   // location

   PCListFixed    plLightIndex;        // list of DWORDs with index into plLightSCANLINEINTERP for each light
   PCListFixed    plLightSCANLINEINTERP;  // list of SCANLINEINTERP with info for each light
} SCANLINEOPT, *PSCANLINEOPT;

void SCANLINEINTERPCalc (BOOL fHomogeneous, DWORD dwPixels, PCPoint pBaseLeft, PCPoint pBaseRight,
                         PCPoint pBasePlusMeterLeft, PCPoint pBasePlusMeterRight,
                         PSCANLINEINTERP pSLI);

void RenderCalcCenterScale (DWORD dwWidth, DWORD dwHeight, fp *pafCenterPixel, fp *pafScalePixel);

/*************************************************************************************
SCANLINEINTERPFromZ - Given a a Z, this fills in a point. This assumes that
cur has been updated to the given pixel.

inputs
   PSCANLINEINTERP   pSLI - Info calcualted by SCANLINEINTERPCalc
   double            fZ - Z value (in meters)
   PCPoint           p - Filled in
returns
   none
*/
__inline void SCANLINEINTERPFromZ (PSCANLINEINTERP pSLI, double fZ, PCPoint p)
{
   p->p[0] = pSLI->afBaseCur[0] + pSLI->afPerMeterCur[0] * fZ;
   p->p[1] = pSLI->afBaseCur[1] + pSLI->afPerMeterCur[1] * fZ;
   p->p[2] = pSLI->afBaseCur[2] + pSLI->afPerMeterCur[2] * fZ;

   if (!pSLI->fHomogeneous)
      p->p[3] = 1.0; // just to make sure
   else
      // homogeneous
      p->p[3] = pSLI->afBaseCur[3] + pSLI->afPerMeterCur[3] * fZ;
}

#define RTTHREADSCALE            1           // create twice as many threads as processors
   // BUGFIX - Create the same number of threads as processors since doing multi-shard rendering
#define RTINTERNALTHREADS        (MAXRAYTHREAD+1)      // [0] is main thread, all the others for subthreads

class CNPREffectOutline;
typedef CNPREffectOutline *PCNPREffectOutline;
class CNPREffectFog;
typedef CNPREffectFog *PCNPREffectFog;

class DLLEXPORT CRenderTraditional : public CRenderSuper, public CRenderSocket {
   friend class CShadowBuf;
   friend class CRayObject;

public:
   ESCNEWDELETE;

   // from CRenderSuper
   virtual DWORD ImageQueryFloat (void);
   virtual BOOL CImageSet (PCImage pImage);
   virtual BOOL CFImageSet (PCFImage pImage);
   virtual void CWorldSet (PCWorldSocket pWorld);
   virtual void CameraFlat (PCPoint pCenter, fp fLongitude, fp fTiltX, fp fTiltY, fp fScale,
                                     fp fTransX, fp fTransY);
   virtual void CameraFlatGet (PCPoint pCenter, fp *pfLongitude, fp *pfTiltX, fp *pfTiltY, fp *pfScale, fp *pfTransX, fp *pfTransY = 0);
   virtual void CameraPerspWalkthrough (PCPoint pLookFrom, fp fLongitude = 0, fp fLatitude = 0, fp fTilt = 0, fp fFOV = PI / 4);
   virtual void CameraPerspWalkthroughGet (PCPoint pLookFrom, fp *pfLongitude, fp *pfLatitude, fp *pfTilt, fp *pfFOV);
   virtual void CameraPerspObject (PCPoint pTranslate, PCPoint pCenter, fp fRotateZ, fp fRotateX, fp fRotateY, fp fFOV); 
   virtual void CameraPerspObjectGet (PCPoint pTranslate, PCPoint pCenter, fp *pfRotateZ, fp *pfRotateX, fp *pfRotateY, fp *pfFOV);
   virtual DWORD CameraModelGet (void);
   virtual BOOL Render (DWORD dwWorldChanged, HANDLE hEventSafe, PCProgressSocket pProgress);
   virtual fp PixelToZ (DWORD dwX, DWORD dwY);
   virtual void PixelToViewerSpace (fp dwX, fp dwY, fp fZ, PCPoint p);
   virtual void PixelToWorldSpace (fp dwX, fp dwY, fp fZ, PCPoint p);
   virtual BOOL WorldSpaceToPixel (const PCPoint pWorld, fp *pfX, fp *pfY, fp *pfZ = NULL);
   virtual fp ExposureGet (void);
   virtual void ExposureSet (fp fExposure);
   virtual COLORREF BackgroundGet (void);
   virtual void BackgroundSet (COLORREF cBack);
   virtual DWORD AmbientExtraGet (void);
   virtual void AmbientExtraSet (DWORD dwAmbient);
   virtual DWORD RenderShowGet (void);
   virtual void RenderShowSet (DWORD dwShow);
   virtual void ShadowsFlagsSet (DWORD dwFlags);
   virtual DWORD ShadowsFlagsGet (void);

   CRenderTraditional (DWORD dwRenderShard);
   ~CRenderTraditional (void);
   void CloneTo (CRenderTraditional *pClone); // copies settings to clone
   void CloneLightsTo (PCRenderTraditional pClone);
   PCMMLNode2 MMLTo (void);
   BOOL MMLFrom (PCMMLNode2 pNode);
   BOOL CZImageSet (PCZImage pImage);

   DWORD          m_dwRenderShard;     // shard to render for
   
   // public variables that expect to change
   fp             m_fShadowsLimit;     // if 0, shadows as far as need. Else, sun shadows limited to this (in meters).
                                       // non-infinite light sources are half or quarter, depending on the strength
   BOOL           m_fShadowsLimitCenteredOnCamera; // if TRUE, the main shadow is centered on the camera, so that
                                       // will be faster for 360 degree render
   DWORD          m_dwShadowsFlags;    // flags for shadows mode, SF_XXX
   DWORD          m_dwSpecial;         // passed to object for special rendering flags. See ORSPECIAL_XXX
   BOOL           m_fBackfaceCull;     // if set to TRUE does backface culling, else doesn't
   BOOL           m_fSelectedBound;    // if true draw bounding box around selected objects
   BOOL           m_fFinalRender;      // if TRUE, drawing for the final render (not saved with MMLTo)
   BOOL           m_fForShadows;       // if TRUE, drawing for purposes of calculating shadows
   DWORD          m_dwRenderModel;     // see RENDERMODEL_XXX
   COLORREF       m_cMono;             // for RENDERMODEL_LINEMONO and RENDERMODEL_SURFACEMONO
   DWORD          m_dwIntensityAmbient1; // Used for RENDERMODEL_XXXAMBIENT. ambient light intensity, 0x10000 = original brightness
   DWORD          m_dwIntensityAmbient2; // Used for RENDERMODEL_XXXSHADED, ambient light intensity, 0x10000 = original brightness
   DWORD          m_dwIntensityLight;   // intensity of light source form light vector, 0x10000 = original brightness
                                        // automagically set when call LightVectorSet(date,time)
   //CPoint         m_pLightSunColor;    // RGB (0..1.0) for the color coming from the sun - used only for shadows
   CPoint         m_pLightAmbientColor;   // RGB (0..1.0) for ambient color - used only for shadows, both sun and moon
   //CPoint         m_pLightMoonColor;   // RGB (0..1.0) for light coming from the moon - used only in shadows
   //CPoint         m_pSunWas;           // location where the sun was an hour before - used to orient it
   //CPoint         m_pMoonWas;          // where moon was
   //CPoint         m_pMoonVector;       // where moon is
   //fp             m_fMoonPhase;        // phase of the moon. 0 is new, .5 full, 1 old

   fp             m_fDetailPixels;     // try to get polygons to m_fDetailPixels across

   // BOOL           m_fRBGlasses;        // if TRUE, draw the image to be used for red/blue 3D glasses
   // BOOL           m_fRBRightEyeDominant; // if TRUE the right eye is dominant, and the object info from that is kep. FALSE if other is dominant
   // fp         m_fRBEyeDistance;    // number of meters between the left and right eyes
   //DFDATE         m_dwSunDate;         // date used to calculate the sun. Call LightVectorSet to change this
   //DFTIME         m_dwSunTime;         // time used to calculate the sun. Call LightVectorSet to change this
   DWORD          m_dwClouds;          // cloud cover, 0 for full sun, 1 for partly, 2 for light clouds, 3 for heavy clouds
   //fp         m_fSunLatitude;      // latitude (in radians) used to calculated the sun. Call LightVectorSet to change this
   DWORD          m_dwDrawPlane;       // if 0 draw x=0 plane, 1 => y=0, 2 => z=0 plane, other values (-1) dont draw
   CMatrix        m_CameraMatrixRotAfterTrans;  // filled in by Camera setting functions. This is the rotation matrix and second translation that occurs AFTER
                                       // the camera's translation matrix. Don't modify this because won't do antyhing. But, can
                                       // get the inverse and use for dragging objcts around
   CMatrix        m_CameraMatrixRotOnlyAfterTrans;  // Only the rotation matrix part of m_CamereaMatrixRotAfterTrans
   CMatrix        m_WorldToView;       // convert from world to view space, used for ::CalcDetail

   BOOL           m_fGridDraw;         // if TRUE, draws a grid on the objects so can identify location in space
   fp             m_fGridMajorSize, m_fGridMinorSize; // distance betweem grids (in meters)
   COLORREF       m_crGridMajor, m_crGridMinor; // color of the major and minor grids
   CMatrix        m_mGridFromWorld;    // converts from world space coords to grid space - allows rotation/translation of grid
   BOOL           m_fGridDrawUD;       // if TRUE, draw UD lines, else only NS and EW
   BOOL           m_fGridDrawDots;     // if TRUE, draw dots instead of lines

   BOOL           m_fDrawControlPointsOnSel; // if set to TRUE then the renderer will draw control
                                       // points on the selected objects

   CPoint         m_lightWorld;           // light vector in world space - NOTE: Made this public for test purposes. dont modify

   DWORD          m_dwShowOnly;        // Only draws this object number (index into m_pWorld). If -1 draws all objects
   DWORD          m_dwSelectOnly;      // if not -1 then always draw this object as selected, else use selection
   BOOL           m_fIgnoreSelection;  // if TRUE then ignore whether or not an object is selected when drawing
   BOOL           m_fBlendSelAbove;    // if TRUE, when blending the selection in, blend it above the existing objects
   int               m_iPriorityIncrease; // how much to increase or decrease the thread priority

   PCNPREffectOutline m_pEffectOutline;   // if not NULL then outlining for render
   PCNPREffectFog m_pEffectFog;   // if not NULL then fog for render

   void CacheClear (void);             // call this if change any of the public member variables


   void LightVectorSet (PCPoint pLight);  // specify the direction of light, in world space
   void LightVectorSetFromSun (void);
   //void LightVectorSet (DFTIME dwTime, DFDATE dwDate, fp fLatitude, fp fTrueNorth);

   BOOL ClipPlaneGet (DWORD dwID, PRTCLIPPLANE pPlane);
   BOOL ClipPlaneSet (DWORD dwID, PRTCLIPPLANE pPlane);
   BOOL ClipPlaneRemove (DWORD dwID);
   void ClipPlaneClear (void);
   void CameraClipNearFar (fp fNear = EPSILON, fp fFar = -1);
   void CameraClipNearFarFlat (fp fNear = -10000, fp fFar = 10000);



   void DrawGrid (COLORREF crMajor, fp fMajorSize, COLORREF crMinor,
      fp fMinorSize, PCMatrix pmWorldToGrid, BOOL fUD, BOOL fDots);
   BOOL RenderForPainting (PCMem pMemRPPIXEL, PCListFixed plRPTEXTINFO,
      PCPoint pViewer, PCMatrix pMatrix, PCProgressSocket pProgress = NULL);



   CRenderTraditional *Clone (void);   // clones this renderer.


   // from CRenderSocket
   BOOL QueryWantNormals (void);    // returns TRUE if renderer wants normal vectors
   BOOL QueryWantTextures (void);    // returns TRUE if renderer wants textures
   fp QueryDetail (void);       // returns detail resoltion (in meters) that renderer wants now
                                                // this may change from one object to the next.
   void MatrixSet (PCMatrix pm);    // sets the current operating matrix. Set NULL implies identity.
                                                // Note: THis matrix will be right-multiplied by any used by renderer for camera
   void PolyRender (PPOLYRENDERINFO pInfo);
                                                // draws/commits all the polygons. The data in pInfo, and what it points
                                                // to may be modified by this call.
   virtual BOOL QuerySubDetail (PCMatrix pMatrix, PCPoint pBound1, PCPoint pBound2, fp *pfDetail);
   virtual BOOL QueryCloneRender (void);
   virtual BOOL CloneRender (GUID *pgCode, GUID *pgSub, DWORD dwNum, PCMatrix pamMatrix);

   void MultiThreadedProc (PRENDTHREADINFO pInfo);

private:
   void FOVSet (fp fFOV);
   BOOL MultiThreadedInit (void);
   void MultiThreadedPolyCalc (void);
   void MultiThreadedEnd (void);
   void MultiThreadedWaitForComplete (BOOL fCancelled);
   DWORD MultiThreadedClaimBuf (BOOL fPoly);
   void MultiThreadedUnClaimBuf (DWORD dwBuf);
   BOOL MultiThreadedDrawClone (GUID *pgCode, GUID *pgSub, DWORD dwNum, PCMatrix pcm);
   BOOL MultiThreadedDrawPolygon (BOOL fFill, DWORD dwNum, const PIHLEND ppEnd[],
      const PIDMISC pm, BOOL fSolid, const PCTextureMapSocket pTexture, fp fGlowScale,
      BOOL fTriangle);
   BOOL MultiThreadedDrawPolygon (BOOL fFill, DWORD dwNum, const PISHLEND ppEnd[],
      const PISDMISC pm, BOOL fTriangle);
   BOOL MultiThreadedShadowsScanline (DWORD dwStart, DWORD dwEnd);
   void ShaderApplyScanLines (DWORD dwStart, DWORD dwEnd, DWORD dwThread,
      PCMem pMemScratch);

   PCRayObject CloneGet (GUID *pgCode, GUID *pgSub, BOOL fCreateIfNotExist,
      PCPoint papBound, PCRayObject *ppRayObject);
   BOOL CloneRelease (GUID *pgCode, GUID *pgSub);
   void CloneFreeAll (void);
   BOOL CloneRenderObject (DWORD dwThread, PCPoint papBound, PCRayObject *papRayObject, PCMatrix pm,
      RECT *prChanged);
   BOOL CloneRenderRayObject (DWORD dwThread, PCRayObject pObject, PCMatrix pm);

   // clones
   CRITICAL_SECTION m_RTCLONECritSec;  // critical section for accessing the clone list
   CListFixed     m_lRTCLONEINFO;      // list if clone info (RTCLONEINFO) for traditional render. NOT sorted at the moment
   CMatrix        m_mRenderSocket;        // matrix from render socket (used for clones)
   PCRayObject    m_pRayCur;              // current ray object that adding to
   BOOL           m_fCloneRender;         // set to TRUE if rendering into a clone, FALSE a normal object
   DWORD          m_dwCloneID;            // so each clone-type has a different ID
   CMem           m_amemCloneScratch[RTINTERNALTHREADS]; // scrach memory
   DWORD          m_dwForceCache;         // forcecache value used for polygons

   // multithreaded
   DWORD          m_dwThreadsPoly;        // number of threads used for polygon rendering
   DWORD          m_dwThreadsShadows;     // number of threads used for shadows rendering
   DWORD          m_dwThreadsMax;         // number of extra threads created, or 0 if all in one thread
   PCTextureMapSocket m_pTextureLast;  // last texture map, optimization
   CMem           m_memThreadBuf;      // buffer to use for the threads
   DWORD          m_dwThreadBufCur;    // current thread buffer being filled, or -1 if none
   CRITICAL_SECTION m_RenderCritSec;   // critical section to enter to access these properties
   RENDTHREADINFO m_aRENDTHREADINFO[MAXRAYTHREAD]; // information on all the threads
   RENDTHREADBUF  m_aRENDTHREADBUF[MAXRAYTHREAD*BUFPERTHREAD]; // buffers that can store stuff in
   CRenderMatrix  m_aRendThreadRenderMatrix[MAXRAYTHREAD*BUFPERTHREAD]; // store away render matrix if doing clone render
   BOOL           m_fExitThreads;      // set to TRUE if all threads should exit
   HANDLE         m_hEventMainSuspended; // event for the main thread being suspended
   BOOL           m_fMainSuspended;    // set to TRUE if the main thread is suspended waiting for a free buffer

   // shader apply
   CMatrix        m_mShaderApplyInv;   // used for shader apply, and useful for multithreaded
   CPoint         m_pShaderApplyEye;   // for shaderapply
   PCRenderTraditional m_pRenderForLight; // render engine for lights, if necessary

   // others
   DWORD          m_dwRenderShow;      // bits indicating what will show when render
   DWORD          m_dwIntensityAmbientExtra; // ambient light added to what's available from the sun and moon
                                       // if call LightVectorSet(date,time) this is automatically calculated (includes intensityambientextra)
   COLORREF       m_cBackground;       // when clear, set the background to this
   fp             m_fExposure;         // amount of exposure so that a white object under m_fExposure lumens light (at 1 m) is RGB=256,256,256
   PCProgressSocket     m_pProgress;            // progress bar that can use to render
   PCImage        m_pImage;               // image to use
   PCZImage       m_pZImage;           // Z image to use
   PCFImage       m_pFImage;           // floating point image to use
   fp         m_afCenterPixel[2];     // center of image. 1/2 m_pImage->m_dwWidth and m_pImage->m_dwHeight
   fp         m_afScalePixel[2];      // scaling of pixels so -1 to 1 goes to 0 to m_dwImageWidth,height
   PRPPIXEL             m_pRPPIXEL;    // scratch used so know if drawing to RPPIXEL
   PCListFixed          m_plRPTEXTINFO;   // scratch used so know if drawing to RPPIXEL
   PCPoint              m_pRPView;     // scratch used to know where the viewer is
   PCMatrix             m_pRPMatrix;   // scratch matrix used to convert light normals from screen to world

   PCWorldSocket        m_pWorld;               // world to draw

   BOOL           m_fWantNormalsForThis;  // TRUE if want normals from this object, FALSE if not
   BOOL           m_fWantTexturesForThis; // TRUE if the renderer wants textures
   DWORD          m_adwWantClipThis[RTINTERNALTHREADS];       // bit-flag for clipping planes that the object crosses and need to clip for
   fp             m_fDetailForThis;       // amount of detail resolution (in meters) that want for this object
   BOOL           m_fPassCameraValid;     // set to TRUE if the camera location is valid for the render pass
   CPoint         m_pPassCamera;          // set to world location of camera for the pass

   DWORD          m_adwObjectID[RTINTERNALTHREADS];           // object ID to use for this. HIWORD is valid, LOWORD is 0's.

   PCImageShader  m_pImageShader;         // image for the shader - used only if have shadows

   CRenderMatrix  m_aRenderMatrix[RTINTERNALTHREADS];         // contains internal rendering matrix stuff, [0] = main, others are per thread
   CPoint         m_lightView;            // light vector relative to viewer - dericed from m_lightWorld

   CMem           m_amemColorModel[RTINTERNALTHREADS];        // memory used for storage in ColorModelApply
   CMem           m_amemColorModelBack[RTINTERNALTHREADS];    // for the back side
   CMem           m_amemColorModelGlow[RTINTERNALTHREADS];    // for the glowing side
   CMem           m_amemColorModelLight[RTINTERNALTHREADS];   // temporary scratch pad for storing light intensity
   CMem           m_amemColorModelLightBack[RTINTERNALTHREADS];  // for the back side
   CMem           m_amemColorModelLightGlow[RTINTERNALTHREADS];  // for the glowing side
   CMem           m_amemColorModelFog[RTINTERNALTHREADS];     // for storing fog intensity
   CMem           m_amemPolyVertex[RTINTERNALTHREADS];        // memory for putting together polygon vertices

   CRenderClip    m_aClipAll[RTINTERNALTHREADS];              // clipping planes, all included, including screen clipping
   CRenderClip    m_aClipOnlyScreen[RTINTERNALTHREADS];       // clipping planes, but only the screen ones. Used for selected objects, etc.

   // camera models
   DWORD          m_dwCameraModel;        // one of the CAMERAMODEL_XXX settings
   fp         m_fCameraFOV;           // field of view for perspective models
   fp             m_fTanHalfCameraFOV;    // precalculate this
   CPoint         m_CameraLookFrom;       // look from vector for walkthrough
   fp         m_fCameraLongitude;     // longitude for walkthrough
   fp         m_fCameraLatitude;      // latitude for walkthrough
   fp         m_fCameraTilt;          // tilt for walkthrough
   CPoint         m_CameraObjectLoc;      // for object perspective, location of the object
   fp         m_fCameraObjectRotX;    // rotation around X for object perspective
   fp         m_fCameraObjectRotY;    // rotation around Y for object perspective
   fp         m_fCameraObjectRotZ;    // rotation around Z for object perspective
   fp         m_fCameraFlatLongitude; // longitude for flat camerea
   fp         m_fCameraFlatTiltX;     // tilt X for flat camera
   fp         m_fCameraFlatTiltY;     // tilt Y for flat camera
   fp         m_fCameraFlatScale;     // scale for flat camera
   fp         m_fCameraFlatTransX;    // translate for flat camera
   fp         m_fCameraFlatTransY;    // translate for flat camera
   fp         m_fClipNear;            // Z-near clipping distance for perspective
   fp         m_fClipFar;             // Z-far clipping distance for perspective
   fp         m_fClipNearFlat;        // Z-near clipping distance for flat model
   fp         m_fClipFarFlat;         // Z-far clipping distance for flat model
   CPoint         m_CameraObjectCenter;   // center of rotation for object view
   CPoint         m_CameraFlatCenter;     // center of rotation for flat view
   CListFixed     m_listRTCLIPPLANE;      // list of clipping planes

   CImage         m_CacheImage[2];        // cache of images, 0=non-selected, 1=selected
   CZImage        m_ZCacheImage[2];       // cache of images, 0=non-selected, 1=selected
   CFImage        m_FCacheImage[2];       // cache of floating point images, 0=non-selected, 1=selected
   DWORD          m_dwCacheImageValid;    // bitfield. bit 0 => non-selected valid, bit 1=>selected valid
   CListFixed     m_lPCLight;             // list of lights
   CListFixed     m_lLIGHTINFO;           // list of lights that want

   // smaller z-buffer
   PCZImage       m_pZImageSmall;         // smaller Z image used for fast Z search, or NULL if not used here

   CMem           m_memRenderPass;        // minimize number of mallocs
   CListFixed     m_alShaderApplyScanLinesLightIndex[MAXRAYTHREAD]; // minimize number of mallocs
   CListFixed     m_alShaderApplyScanLinesLightSCANLINEINTERP[MAXRAYTHREAD]; // minimize number of mallocs
   CMem           m_memShaderApplyScratch[MAXRAYTHREAD]; // scratch memory

   void TransformToScreen (DWORD dwNum, PCPoint paPoints);  // transform clipped coordinates in W space to the screeen, in place
   DWORD ModelInfiniteLight (PCPoint pNormal);          // model light at infinity
   PGCOLOR ColorModelApply (DWORD dwThread, PPOLYRENDERINFO pInfo, DWORD *pdwUsage, BOOL fBack,
      BOOL fGlow);   // applies the color model.
         // fills in pdwUsage for how to interpret PGCOLOR.
         // 0 => all points use PGCOLOR[0],
         // 1 => use based on color index of vertex
         // 2 => use based on vertex index
         // 3 => use based on point
   BOOL ShouldBackfaceCull (const PPOLYDESCRIPT pPoly, const PPOLYRENDERINFO pInfo, BOOL *pfPerpToViewer = NULL);  // returns TRUE if should backface cull the polygon. Coordinates must be in screen sapce
   void RenderObject (PROINFO pObject);       // sets up params, calls into the object's callback, and has it rendered
   void ClipRebuild (void);
   BOOL RenderInternal (BOOL fCanUseCache, HANDLE hEventSafe);  // render with the current settings
   BOOL RenderPass (DWORD dwSelection);  // do one pass of rendering
   void RenderControlPoint (POSCONTROL posc, PCMatrix pmObject);
   void PolyRenderShadows (DWORD dwThread, const PPOLYRENDERINFO pInfo);
   void PolyRenderNoShadows (DWORD dwThread, const PPOLYRENDERINFO pInfo);
   BOOL ShaderApply (HANDLE hEventSafe);
   void ShaderForPainting (void);
   void ShaderLighting (const PCMaterial pm, const PCPoint pWorld, const PCPoint pN, const PCPoint pNText, const PCPoint pEye,
                                         const WORD *pawColor, PSCANLINEOPT pSLO, fp fZScanLine, PCMem pMemScratch,
                                         fp *pafDiffuse, fp *pafSpecular);
   BOOL ShaderPixel (DWORD dwThread, const PISPIXEL pis, /* DWORD dwTransDepth,*/ DWORD x, DWORD y, fp *pafColor, const PCMatrix pmInv, const PCPoint pEye,
      PSCANLINEOPT pSLO, PCMem pMemScratch, BOOL fCanBeTransparent);
   BOOL CalcDetail (DWORD dwThread, const PCMatrix pmObjectToWorld, const PCPoint pBound1,
                                     const PCPoint pBound2, PROINFO pInfo);

   void LightsDirty (void);
   void LightsAddRemove (void);
   void LightsCalc (PCRenderTraditional *ppRender, PCProgressSocket pProgress);
   void LightsSpotlight (PCRenderTraditional *ppRender, PCProgressSocket pProgress);
   //void DrawSunMoon (void);
   //void DrawClouds (void);
   void DrawPlane (DWORD dwPlane);
   DWORD Width (void);
   DWORD Height (void);
   BOOL IsCompletelyCovered (const RECT *pRect, fp fZ);
};
typedef CRenderTraditional *PCRenderTraditional;

DLLEXPORT PVOID MemMalloc (PCMem pMem, size_t dwAlloc);

/***********************************************************************************
CNoodle */

// noddle shapes for ShapeDefault()
#define NS_CUSTOM       0
#define NS_RECTANGLE    1

#define NS_CBEAMFRONT   3     // C-beam, open to the front
#define NS_CBEAMBACK    4     // C-beam, open to the back
#define NS_CBEAMRIGHT   5     // C-beam, open to the right
#define NS_CBEAMLEFT    6     // C-beam, open to the left

#define NS_ZBEAMFRONT   7     // Z-beam, open to the front
#define NS_ZBEAMBACK    8
#define NS_ZBEAMRIGHT   9
#define NS_ZBEAMLEFT    10

#define NS_IBEAMLR      11    // I-beam, open on LR
#define NS_IBEAMFB      12    // I-beam, open on front,back

#define NS_TRIANGLE     13
#define NS_PENTAGON     14
#define NS_HEXAGON      15

#define NS_CIRCLEFAST   2     // fast-to-draw circle
#define NS_CIRCLE       16


class DLLEXPORT CNoodle {
public:
   ESCNEWDELETE;

   CNoodle (void);
   ~CNoodle (void);

   BOOL PathSpline (BOOL fLooped, DWORD dwPoints, PCPoint paPoints,
      DWORD *padwSegCurve, DWORD dwDivide = 2);
   BOOL PathLinear (PCPoint pStart, PCPoint pEnd);
   PCSpline PathSplineGet (void);

   BOOL FrontSpline (DWORD dwPoints, PCPoint paPoints,
      DWORD *padwSegCurve, DWORD dwDivide = 2);
   BOOL FrontVector (PCPoint pFront);
   PCSpline FrontSplineGet (void);
   BOOL FrontVectorGet (PCPoint pFront);

   BOOL ScaleSpline (DWORD dwPoints, PCPoint paPoints,
      DWORD *padwSegCurve, DWORD dwDivide = 2);
   BOOL ScaleVector (PCPoint pScale);
   PCSpline ScaleSplineGet (void);
   BOOL ScaleVectorGet (PCPoint pScale);

   BOOL ShapeDefault (DWORD dwShape);
   BOOL ShapeSpline (BOOL fLooped, DWORD dwPoints, PCPoint paPoints,
      DWORD *padwSegCurve, DWORD dwDivide = 2);
   BOOL ShapeSplineSurface (BOOL fLoopedH, DWORD dwH, DWORD dwV, PCPoint paPoints,
      DWORD *padwSegCurveH, DWORD *padwSegCurveV, DWORD dwDivideH = 2, DWORD dwDivideV = 2);
   BOOL ShapeDefaultGet (DWORD *pdwShape);
   PCSpline ShapeSplineGet (void);
   PCSplineSurface ShapeSplineSurfaceGet (void);

   BOOL OffsetSet (PCPoint pOffset);
   BOOL OffsetGet (PCPoint pOffset);
   BOOL BevelSet (BOOL fStart, DWORD dwMode, PCPoint pBevel);
   BOOL BevelGet (BOOL fStart, DWORD *pdwMode, PCPoint pBevel);
   BOOL DrawEndsSet (BOOL fDraw);
   BOOL DrawEndsGet (void);
   BOOL BackfaceCullSet (BOOL fCull);
   BOOL BackfaceCullGet (void);
   void TextureWrapSet (DWORD dwWrapHorz, DWORD dwWrapVert = 0);
   DWORD TextureWrapGet (BOOL fHorz = TRUE);

   BOOL MatrixSet (PCMatrix pm);
   BOOL MatrixGet (PCMatrix pm);

   BOOL Render (POBJECTRENDER pr, PCRenderSurface prs);
   void QueryBoundingBox (PCPoint pCorner1, PCPoint pCorner2);
   PCMMLNode2 MMLTo (void);
   BOOL MMLFrom (PCMMLNode2 pNode);
   CNoodle *Clone (void);
   BOOL CloneTo (CNoodle *pTo);
   BOOL LocationAndDirection (fp fLoc, PCPoint pLoc, PCPoint pForward = NULL,
                                    PCPoint pFront = NULL, PCPoint pRight = NULL, PCPoint pScale = NULL);

private:
   BOOL CalcPolygonsIfDirty (void);

   PCSpline    m_psPath;      // path spline
   PCSpline    m_psFront;     // spline that points to the front
   CPoint      m_pFront;      // vector for front, used if m_psFront NULL
   PCSpline    m_psScale;     // scale vector. x & y are scale. z from 0..1
   CPoint      m_pScale;      // used if !m_psScale. x and y are scale
   DWORD       m_dwShape;     // used if !m_psShape, and !m_pssShape
   PCSpline    m_psShape;     // shape spline
   PCSplineSurface m_pssShape;   // for a changing shape.
   CPoint      m_pOffset;     // offset, pre-rotation/twist
   CPoint      m_apBevel[2];  // [0] = start, [1] = end, bevel normal at start, pre-rotation/twist
   DWORD       m_adwBevelMode[2]; // 0 = perpendicular to end, ignore m_pBevelStart/End
                              // 1 = m_pBevelXXX is relative to direction of column
                              // 2 = m_pBevelXXX is relative to object (rotated by Matrix Set/Get)
                              // [0] = start, [1] = end
   BOOL        m_fDrawEnds;   // if TRUE draw the ends
   BOOL        m_fBackfaceCull;  // if TRUE then backface cull
   DWORD       m_dwTextureWrap;  // if this is non-zero then the texture is wrapped this
                              // many times around the noodle, exactly, so no seams
   DWORD       m_dwTextureWrapVert; // wrap along length
   CMatrix     m_mMatrix;     // value in matrix set
   
   // calculated values
   BOOL        m_fDirty;      // set to true if calculated values are dirty
   CMem        m_amemPoints[3];  // points used for complex[0] and simplified[1] version, [2] is scratch space
   DWORD       m_adwPoints[2];   // number of points in complex and simplified
   CMem        m_amemNormals[3]; // normals for complex and simplified
   DWORD       m_adwNormals[2];  // number of normals
   CMem        m_amemTextures[3];   // textures for complex and simplified
   DWORD       m_adwTextures[2]; // number of textures
   CMem        m_amemVertices[3];   // vertices for complex and simplifed
   DWORD       m_adwVertices[2]; // number of vertices
   CMem        m_amemPolygons[3];   // polygons for complex and simplified
   DWORD       m_adwPolygons[2]; // number of polygons.
   fp      m_fLength;        // average length of extrusion
   fp      m_fCircum;        // afterage circumfrence of extrusion
};

typedef CNoodle * PCNoodle;


/***********************************************************************************
CRevolution */
#define RPROF_CUSTOM             0  // custom profile
#define RPROF_LINEVERT           11  // vertical line, from 1,0 to 1,1
#define RPROF_LINECONE           12  // line from 0,0 to 1,1
#define RPROF_LINEDIAG2          13  // line from 0,.5 to 1,1
#define RPROF_LINEDIAG23         14  // from from 0,.666 to 1,1
#define RPROF_C                  15 // line from 0,0 to 1,0 to 1,1 to 0,1
#define RPROF_L                  16 // line from 0,0 to 1,0 to 1,1
#define RPROF_LDIAG              17 // line from 0,0 to .66,0 to 1,1

#define RPROF_CIRCLE             21  // circle
#define PRROF_CIRCLEHEMI         22 // hald a circle
#define RPROF_CIRCLEQUARTER      23 // about 1/4 of circle
#define RPROF_CIRCLEOPEN         24 // circle, open at bottom - light glass globes
#define PPROF_CIRCLEQS           25 // bottom is 0,0 to 1,.5 is quarter circle, 1,.5 to 1,1 is linear

#define RPROF_LIGHTGLOBE         31    // light globe shape
#define RPROF_LIGHTSPOT          32    // spotlight shape
#define RPROF_FANCYGLASSSHADE    33 // old fashioned fancy glass light-bulb diffuser

#define RPROF_SHADECURVED        41 // curved lamp shade - old fashioned
#define RPROF_SHADEDIRECTIONAL   42 // 0,0 to .5,0 to .5,.33, to 1,1
#define PRROF_CERAMICLAMPSTAND1  43 // shapde for ceramic lamp
#define PRROF_CERAMICLAMPSTAND2  44 // shapde for ceramic lamp
#define PRROF_CERAMICLAMPSTAND3  45 // shapde for ceramic lamp
#define RPROF_SHADEDIRECTIONAL2  46 // 0,0 to .8,0 to .8,.4, to 1,.6 to 1,1
#define RPROF_HORN               57 // ellipse, 0,0 to 0,1 to 1,1

#define RPROF_RING               60 // ring rom 1,0 to 1.05,0
#define RPROF_RINGWIDE           61 // ring from 1,0 to 1.2,0


#define RREV_CUSTOM              0  // custom revolution
#define RREV_SQUARE              11  // square
#define RREV_TRIANGLE            12 // triangle
#define RREV_PENTAGON            13 // pentagon
#define RREV_HEXAGON             14 // hexagon
#define RREV_OCTAGON             15 // octagon 
#define RREV_CIRCLE              21  // circular
#define RREV_CIRCLEFAN           22 // circular, but with fan bends for strength
#define RREV_CIRCLEHEMI          23 // circular, from 1,0 looping through 0,1, to -1,0
#define RREV_SQUAREHALF          31 // non-looping, from 1,0 to 1,1 to -1,1 to -1,0

class DLLEXPORT CRevolution {
public:
   ESCNEWDELETE;

   CRevolution(void);
   ~CRevolution(void);

   BOOL ProfileSet (PCSpline pSpline, int iNormTipStart = 0, int iNormTipEnd = 0); // set the profile based on a spline
   BOOL ProfileSet (DWORD dwType, DWORD dwDetail = 1);     // set profile based on a type
   BOOL ProfileSet (PCPoint pStart, PCPoint pEnd);
   DWORD ProfileGet (DWORD *pdwDetail = NULL);            // returns current profile tyoe
   PCSpline ProfileGetSpline (void);
   BOOL RevolutionSet (PCSpline pSpline); // set the Revolution based on a spline
   BOOL RevolutionSet (DWORD dwType, DWORD dwDetail = 1);     // set Revolution based on a type
   DWORD RevolutionGet (DWORD *pdwDetail = NULL);            // returns current Revolution tyoe
   PCSpline RevolutionGetSpline (void);
   BOOL ScaleSet (PCPoint pScale);
   BOOL ScaleGet (PCPoint pScale);
   BOOL DirectionSet (PCPoint pBottom, PCPoint pAround, PCPoint pLeft);
   BOOL DirectionGet (PCPoint pBottom, PCPoint pAround, PCPoint pLeft);
   BOOL BackfaceCullSet (BOOL fBackface);
   BOOL BackfaceCullGet (void);
   void TextRepeatSet (DWORD dwRepeatH, DWORD dwRepeatV);
   void TextRepeatGet (DWORD *pdwRepeatH, DWORD *pdwRepeatV);

   BOOL Render (POBJECTRENDER pr, PCRenderSurface prs);
   void QueryBoundingBox (PCPoint pCorner1, PCPoint pCorner2);
   PCMMLNode2 MMLTo (void);
   BOOL MMLFrom (PCMMLNode2 pNode);
   CRevolution *Clone (void);
   BOOL CloneTo (CRevolution *pTo);


private:
   BOOL CalcSurfaceIfDirty (void);

   PCSpline       m_psProfile;      // spline for the profile. X and Y only
   PCSpline       m_psRevolution;   // spline for the revolution, X and Y only
   DWORD          m_dwProfileType;  // See RPROF_XXX
   DWORD          m_dwRevolutionType;// See RREV_XXX
   DWORD          m_dwProfileDetail;   // as passed into splineinit
   DWORD          m_dwRevolutionDetail;   // as passed into spline init
   CPoint         m_pScale;         // scale
   CPoint         m_pBottom;        // where the bottom (0,0,0) of the revoltion is
   CPoint         m_pAroundVec;     // vector that it's rotating around
   CPoint         m_pLeftVec;       // looking in m_pAroundVec direction, this points to the left
   BOOL           m_fBackface;      // Set to true if can backface cull
   int            m_aiNormTip[2];   // if 1 or -1, force the normal at the tip, [0]=start,[1]=end,
                                    // to be either m_pAroundVec (1) or -m_pAroundVec (-1)
   DWORD          m_adwTextRepeat[2];   // [0]=horz,[1]=vert, if the value is > 0 then repeat the
                                    // texture exactly that many times in horz or vertical. if 0 then scale
                                    // in number of meters

   // derived
   BOOL           m_fDirty;         // true if it's dirty
   CMatrix        m_mMatrix;        // matrix to apply to rotations, translations...
                                    // derived from m_pBottom, m_pAroundVec, m_pLeftVec
   CMem           m_memPoints;      // memory for all the points, arranged [0..dwY-1][0..dwX-1]
   DWORD          m_dwX;            // number of X across
   DWORD          m_dwY;            // number of Y across
};
typedef CRevolution * PCRevolution;






/**********************************************************************************
CObjectEye */
// {722D3D0D-8F4B-4a82-B0D3-83D601511755}
DEFINE_GUID(CLSID_Eye, 
0xf4693d18, 0x4c4b, 0x1b82, 0x9a, 0xd3, 0x83, 0xd6, 0x1, 0x51, 0x17, 0x55);
// CObjectEye is a test object to test the system
class DLLEXPORT CObjectEye : public CObjectTemplate {
public:
   CObjectEye (PVOID pParams, POSINFO pInfo);
   ~CObjectEye (void);

   virtual void Delete (void);
   virtual void Render (POBJECTRENDER pr, DWORD dwSubObject);
   virtual CObjectSocket *Clone (void);
   virtual PCMMLNode2 MMLTo (void);
   virtual BOOL MMLFrom (PCMMLNode2 pNode);
   virtual BOOL MoveReferencePointQuery (DWORD dwIndex, PCPoint pp);
   virtual BOOL MoveReferenceStringQuery (DWORD dwIndex, PWSTR psz, DWORD dwSize, DWORD *pdwNeeded);
   virtual BOOL ControlPointQuery (DWORD dwID, POSCONTROL pInfo);
   virtual BOOL ControlPointSet (DWORD dwID, PCPoint pVal, PCPoint pViewer);
   virtual void ControlPointEnum (PCListFixed plDWORD);
   virtual BOOL AttribGetIntern (PWSTR pszName, fp *pfValue);
   virtual void AttribGetAllIntern (PCListFixed plATTRIBVAL);
   virtual void AttribSetGroupIntern (DWORD dwNum, PATTRIBVAL paAttrib);
   virtual BOOL AttribInfoIntern (PWSTR pszName, PATTRIBINFO pInfo);
   virtual BOOL DialogQuery (void);
   virtual BOOL DialogShow (DWORD dwSurface, PCEscWindow pWindow, PCPoint pClick);
   virtual void QueryBoundingBox (PCPoint pCorner1, PCPoint pCorner2, DWORD dwSubObject);

//private:
   void EyeInfoFromPoint (DWORD dwEye, PCPoint pLoc);
   void EyeInfoToPoint (DWORD dwEye, PCPoint pLoc);

   fp             m_fDiameter;     // diameter of eye
   fp             m_fCorneaArc;    // degrees of arc (in radians) that cornea covers eyeball
   fp             m_fCorneaHeight; // heigh of cornea as percent (0..1) of diameter
   fp             m_fApart;        // distance apart that eyes are
   DWORD          m_dwNumEyes;     // number of eyes, 1 or 2
   BOOL           m_fIndepColor;   // set to TRUE if independent coloration for eyese
   BOOL           m_fIndepMove;    // set to TRUE if the eye movement is independent
   CPoint         m_pCatEyes;      // amount that reflects cats eyes. p[0] = 0 (round) to 1 (cat), p[1]=angle of slit (0 = UD)
   DWORD          m_dwRepeatSclera; // number of times texture repeats on sclera
   DWORD          m_dwRepeatIris;   // number of times texture repeats on iris

   // settable by attributes
   CPoint         m_apEyeInfo[2];  // [0]=left,[1]=right... p[0] = rot l/r, p[1] = rot u/d, p[2] = iris open, from 0 (closed) to 1 (open), p[2]=distance in meters

   // for drawing
   BOOL           m_fRevValid;      // set to TRUE when revolutions are properly calculated
   DWORD          m_dwPrevRes;      // previous resoltion that calculated for
   CRevolution    m_revCornea;      // cornea
   CRevolution    m_revSclera;      // for drawing the sclera

   CRenderSurface    m_Renderrs;    // to minimize mallocs
};
typedef CObjectEye *PCObjectEye;


/**********************************************************************************
CObjectLeaf */

DEFINE_GUID(CLSID_Leaf, 
0x887a3d0d, 0x614b, 0xa882, 0x94, 0xd3, 0x83, 0x11, 0x1, 0x51, 0x17, 0x55);

// CObjectLeaf is a test object to test the system
class DLLEXPORT CObjectLeaf : public CObjectTemplate {
public:
   CObjectLeaf (PVOID pParams, POSINFO pInfo);
   ~CObjectLeaf (void);

   virtual void Delete (void);
   virtual void Render (POBJECTRENDER pr, DWORD dwSubObject);
   virtual CObjectSocket *Clone (void);
   virtual PCMMLNode2 MMLTo (void);
   virtual BOOL MMLFrom (PCMMLNode2 pNode);
   virtual BOOL MoveReferencePointQuery (DWORD dwIndex, PCPoint pp);
   virtual BOOL MoveReferenceStringQuery (DWORD dwIndex, PWSTR psz, DWORD dwSize, DWORD *pdwNeeded);
   virtual BOOL ControlPointQuery (DWORD dwID, POSCONTROL pInfo);
   virtual BOOL ControlPointSet (DWORD dwID, PCPoint pVal, PCPoint pViewer);
   virtual void ControlPointEnum (PCListFixed plDWORD);
   virtual BOOL DialogQuery (void);
   virtual BOOL DialogShow (DWORD dwSurface, PCEscWindow pWindow, PCPoint pClick);
   virtual void QueryBoundingBox (PCPoint pCorner1, PCPoint pCorner2, DWORD dwSubObject);

//private:
   void CalcLeaf (void);

   // user set
   CPoint            m_pStem;          // stem goes from 0,0,0 to pStem. If 0-length then dont draw stem
   fp                m_fStemDiam;      // stem diameter
   fp                m_fStemConnect;   // Percent (0..1) up the leaf that the stems connects to it
   TEXTUREPOINT      m_tpSize;         // leaf size. .h = width (perp to stem), .v = length
   DWORD             m_dwDetailH;      // amount of detail along width (perp to stem). 0 = min (3 points)
   DWORD             m_dwDetailV;      // amount of detail along length (perp to stem. 0 = min (3 points)
   TEXTUREPOINT      m_tpPerpShape;    // .h = angle that crease in leaf (running along stem) forms, .v = stiffness along length (0=min, 1=max)
   TEXTUREPOINT      m_tpLenShape;     // .h = angle relative to stem, .v = stiffness (0 = min, 1 = max)
   TEXTUREPOINT      m_tpRipples;      // .h = height of ripple (from 0..1), .v = length of ripple (0..1)
   BOOL              m_fCutoutTop;     // set to TRUE if cutout notch from top
   BOOL              m_fCutoutBottom;  // cutout notch from bottom

   // calculated
   BOOL              m_fDirty;         // set to TRUE if need to recalc info
   CNoodle           m_nStem;          // stem noodle
   fp                m_fStemLen;       // length of the stem - used to detect if should draw
   DWORD             m_dwWidth;        // width of calculated leaf
   DWORD             m_dwHeight;       // height of calculated leaf
   CMem              m_memPoints;      // an array of m_dwWidth x m_dwHeight points
   CMem              m_memNormals;     // an array of m_dwWidth x m_dwHeight normals

   CRenderSurface    m_Renderrs;       // for malloc optimization
   CMem              m_memRenderMap;   // for malloc optimization
};
typedef CObjectLeaf *PCObjectLeaf;


/**********************************************************************************
CObjectBooks */
// {26FA52AA-AEDE-48f3-A290-2B81049288E5}
DEFINE_GUID(CLSID_Books, 
0x26fa52aa, 0xaede, 0x48f3, 0xa2, 0x90, 0x2b, 0x81, 0x4, 0x92, 0x88, 0xe5);

// CObjectBooks is a test object to test the system
class DLLEXPORT CObjectBooks : public CObjectTemplate {
public:
   CObjectBooks (PVOID pParams, POSINFO pInfo);
   ~CObjectBooks (void);

   virtual void Delete (void);
   virtual void Render (POBJECTRENDER pr, DWORD dwSubObject);
   virtual CObjectSocket *Clone (void);
   virtual PCMMLNode2 MMLTo (void);
   virtual BOOL MMLFrom (PCMMLNode2 pNode);
   virtual BOOL ControlPointQuery (DWORD dwID, POSCONTROL pInfo);
   virtual BOOL ControlPointSet (DWORD dwID, PCPoint pVal, PCPoint pViewer);
   virtual void ControlPointEnum (PCListFixed plDWORD);
   virtual BOOL DialogQuery (void);
   virtual BOOL DialogShow (DWORD dwSurface, PCEscWindow pWindow, PCPoint pClick);
   virtual void QueryBoundingBox (PCPoint pCorner1, PCPoint pCorner2, DWORD dwSubObject);

// private:
   void RenderBook (POBJECTRENDER pr, PCRenderSurface prs, PCPoint pSize,
                               fp fBindOut, fp fPageIndent, COLORREF *pcReplace);
   void FillBookInfo (void);

   CPoint      m_pBookSize;   // typical book size. [0] = thick, [1] = length, [2]=height
   CPoint      m_pBookVar;    // book variation, 0..1. [0] = thick, [1] = size, [2] = aspect, [3] indent on shelf in meters
   COLORREF    m_acColor[3];  // 3 color variants that are interpolated
   fp          m_fBindOut;    // how far the binding is out, as a percentage of the book thickness
   fp          m_fPageIndent; // typical page indent in meters
   fp          m_fRowLength;  // length of a row of books, if < booksize.p[0] then only one book

   // automatically generated
   CListFixed  m_lBOOKINFO;   // list of bookinfo
   DWORD       m_dwFillInSeed;   // seed that filled in with

   CRenderSurface    m_Renderrs;    // to minimize mallocs
};
typedef CObjectBooks *PCObjectBooks;




/**********************************************************************************
CObjectRevolution */

// {E522F0A5-8C4C-4f7c-BE95-1B6560F84F52}
DEFINE_GUID(CLSID_Revolution, 
0xe522f0a5, 0x8c4c, 0x4f7c, 0xbe, 0x95, 0x1b, 0x65, 0x60, 0xf8, 0x4f, 0x52);


// CObjectRevolution is a test object to test the system
class DLLEXPORT CObjectRevolution : public CObjectTemplate {
public:
   CObjectRevolution (PVOID pParams, POSINFO pInfo);
   ~CObjectRevolution (void);

   virtual void Delete (void);
   virtual void Render (POBJECTRENDER pr, DWORD dwSubObject);
   virtual CObjectSocket *Clone (void);
   virtual PCMMLNode2 MMLTo (void);
   virtual BOOL MMLFrom (PCMMLNode2 pNode);
   virtual BOOL DialogQuery (void);
   virtual BOOL DialogShow (DWORD dwSurface, PCEscWindow pWindow, PCPoint pClick);
   virtual BOOL DialogCPQuery (void);
   virtual BOOL DialogCPShow (DWORD dwSurface, PCEscWindow pWindow, PCPoint pClick);
   virtual BOOL MoveReferencePointQuery (DWORD dwIndex, PCPoint pp);
   virtual BOOL MoveReferenceStringQuery (DWORD dwIndex, PWSTR psz, DWORD dwSize, DWORD *pdwNeeded);
   virtual BOOL ControlPointQuery (DWORD dwID, POSCONTROL pInfo);
   virtual BOOL ControlPointSet (DWORD dwID, PCPoint pVal, PCPoint pViewer);
   virtual void ControlPointEnum (PCListFixed plDWORD);
   virtual void QueryBoundingBox (PCPoint pCorner1, PCPoint pCorner2, DWORD dwSubObject);

// private:
   DWORD ControlPointNum (void);

   DWORD    m_dwDisplayControl;        // which control points displayed, 0=scale, 1=profile, 2=revolution
   CRevolution    m_Rev;            // revolution object

   CRenderSurface    m_Renderrs;       // for malloc optimization
};
typedef CObjectRevolution *PCObjectRevolution;




/**********************************************************************************
CObjectTableCloth */
// {F02F2323-5ACD-468f-9048-6A71F360CA3E}
DEFINE_GUID(CLSID_TableCloth, 
0xf02f2323, 0x5acd, 0x468f, 0x90, 0x48, 0x6a, 0x71, 0xf3, 0x60, 0xca, 0x3e);

// CObjectTableCloth is a test object to test the system
class DLLEXPORT CObjectTableCloth : public CObjectTemplate {
public:
   CObjectTableCloth (PVOID pParams, POSINFO pInfo);
   ~CObjectTableCloth (void);

   virtual void Delete (void);
   virtual void Render (POBJECTRENDER pr, DWORD dwSubObject);
   virtual CObjectSocket *Clone (void);
   virtual PCMMLNode2 MMLTo (void);
   virtual BOOL MMLFrom (PCMMLNode2 pNode);
   virtual BOOL DialogQuery (void);
   virtual BOOL DialogShow (DWORD dwSurface, PCEscWindow pWindow, PCPoint pClick);
   virtual BOOL MoveReferencePointQuery (DWORD dwIndex, PCPoint pp);
   virtual BOOL MoveReferenceStringQuery (DWORD dwIndex, PWSTR psz, DWORD dwSize, DWORD *pdwNeeded);
   virtual void QueryBoundingBox (PCPoint pCorner1, PCPoint pCorner2, DWORD dwSubObject);


// private:
   void CalcInfo (void);
   void ShapeToSpline (PCSpline ps);

   DWORD       m_dwShape;        // shape of the painting, 0 = rect, 1=oval, 2=rounded
   CPoint      m_pSizeTable;     // size of the table. p[0] = width, p[1] = height
   CPoint      m_pSizeCloth;     // size of the tablecloth. p[0] = width, p[1] = height
   fp          m_fRippleSize;    // size of one ripple in meters - determines how thick fabric looks

   // calculated
   DWORD       m_dwNumPoints;    // number of points
   DWORD       m_dwNumNormals;   // number of normals
   DWORD       m_dwNumText;      // number of texture settings
   DWORD       m_dwNumVertex;    // number of vertices
   DWORD       m_dwNumPoly;      // number of polygons
   CMem        m_memPoints;      // memory containing points
   CMem        m_memNormals;     // memory containing normals
   CMem        m_memText;        // memory containign textures
   CMem        m_memVertex;      // memory containing vertices
   CMem        m_memPoly;        // memory containing polygons - array of 4-DWORs for vertices.
                                 // last DWORD can be -1 indicating just a triangle

   CRenderSurface    m_Renderrs;       // for malloc optimization
};
typedef CObjectTableCloth *PCObjectTableCloth;





/**********************************************************************************
CObjectCushion */
// {8C695D2D-9933-4f2c-A2A9-3E673956EF35}
DEFINE_GUID(CLSID_Cushion, 
0x8c695d2d, 0x9933, 0x4f2c, 0xa2, 0xa9, 0x3e, 0x67, 0x39, 0x56, 0xef, 0x35);

// CObjectCushion is a test object to test the system
class DLLEXPORT CObjectCushion : public CObjectTemplate {
public:
   CObjectCushion (PVOID pParams, POSINFO pInfo);
   ~CObjectCushion (void);

   virtual void Delete (void);
   virtual void Render (POBJECTRENDER pr, DWORD dwSubObject);
   virtual CObjectSocket *Clone (void);
   virtual PCMMLNode2 MMLTo (void);
   virtual BOOL MMLFrom (PCMMLNode2 pNode);
   virtual BOOL DialogQuery (void);
   virtual BOOL DialogShow (DWORD dwSurface, PCEscWindow pWindow, PCPoint pClick);
   virtual BOOL ControlPointQuery (DWORD dwID, POSCONTROL pInfo);
   virtual BOOL ControlPointSet (DWORD dwID, PCPoint pVal, PCPoint pViewer);
   virtual void ControlPointEnum (PCListFixed plDWORD);
   virtual void QueryBoundingBox (PCPoint pCorner1, PCPoint pCorner2, DWORD dwSubObject);


// private:
   void CalcInfo (void);
   void ShapeToSpline (PCSpline ps);
   void RenderCushion (POBJECTRENDER pr, PCRenderSurface prs, BOOL fFlip);
   fp BulgeCurve (fp fDist, DWORD dwSide);

   DWORD       m_dwShape;        // shape of the painting, 0 = rect, 1=oval, 2=rounded
   CPoint      m_pSize;          // size of the cusion. p[0] = width, p[1] = height, p[2] = height of stuffing
   CPoint      m_pCenter;        // center point (only X and Y valid) for the pillow
   fp          m_fDetail;        // keep the triangles about this size
   fp          m_afBulge[2];     // amount that bulges (as percent of width and length, or depth) out at m_fBulgeDist from edge, [0] is on top and bottom, [1] on edge
   fp          m_afBulgeDist[2]; // distance from edge that has full bulge (as percent of width and length, or depth), [0] is on top and bottom, [1] on edge
   CPoint      m_pButtonDist;    // buttons are this distance apart, p[0] = x, p[1] = y;
   DWORD       m_dwButtonNumX;   // number of bottons in X direction
   DWORD       m_dwButtonNumY;   // number of buttons in Y direction
   DWORD       m_dwButtonStagger;   // stagger pattern. 0 = grid, 1=every even one (Y, 2=every odd one (Y)
   CPoint      m_pCutout;        // cutout location
   fp          m_fRounded;       // rounded amount, 0..1
   CSpline     m_Spline;         // path spline - up to date with latest spline, but only saved away if custom shape

   // calculated
   DWORD       m_dwNumPoints;    // number of points
   DWORD       m_dwNumNormals;   // number of normals
   DWORD       m_dwNumText;      // number of texture settings
   DWORD       m_dwNumVertex;    // number of vertices
   DWORD       m_dwNumPoly;      // number of polygons
   CMem        m_memPoints;      // memory containing points
   CMem        m_memNormals;     // memory containing normals
   CMem        m_memText;        // memory containign textures
   CMem        m_memVertex;      // memory containing vertices
   CMem        m_memPoly;        // memory containing polygons - array of 4-DWORs for vertices.
                                 // last DWORD can be -1 indicating just a triangle

   // points for the edge loop
   DWORD       m_dwEdgeNumPoints;    // number of points
   DWORD       m_dwEdgeNumNormals;   // number of normals
   DWORD       m_dwEdgeNumText;      // number of texture settings
   DWORD       m_dwEdgeNumVertex;    // number of vertices
   DWORD       m_dwEdgeNumPoly;      // number of polygons
   CMem        m_memEdgePoints;      // memory containing points
   CMem        m_memEdgeNormals;     // memory containing normals
   CMem        m_memEdgeText;        // memory containign textures
   CMem        m_memEdgeVertex;      // memory containing vertices
   CMem        m_memEdgePoly;        // memory containing polygons - array of 4-DWORs for vertices.
                                 // last DWORD can be -1 indicating just a triangle


   CRenderSurface    m_Renderrs;       // for malloc optimization

};
typedef CObjectCushion *PCObjectCushion;






/**********************************************************************************
CObjectCurtain */

// {BD7A16AA-FF24-4665-9E85-460D01B74C3D}
DEFINE_GUID(CLSID_Curtain, 
0xbd7a16aa, 0xff24, 0x4665, 0x9e, 0x85, 0x46, 0xd, 0x1, 0xb7, 0x4c, 0x3d);

// CObjectCurtain is a test object to test the system
class DLLEXPORT CObjectCurtain : public CObjectTemplate {
public:
   CObjectCurtain (PVOID pParams, POSINFO pInfo);
   ~CObjectCurtain (void);

   virtual void Delete (void);
   virtual void Render (POBJECTRENDER pr, DWORD dwSubObject);
   virtual CObjectSocket *Clone (void);
   virtual PCMMLNode2 MMLTo (void);
   virtual BOOL MMLFrom (PCMMLNode2 pNode);
   virtual BOOL DialogQuery (void);
   virtual BOOL DialogShow (DWORD dwSurface, PCEscWindow pWindow, PCPoint pClick);
   virtual BOOL MoveReferencePointQuery (DWORD dwIndex, PCPoint pp);
   virtual BOOL MoveReferenceStringQuery (DWORD dwIndex, PWSTR psz, DWORD dwSize, DWORD *pdwNeeded);
   virtual fp OpenGet (void);
   virtual BOOL OpenSet (fp fOpen);
   virtual BOOL ControlPointQuery (DWORD dwID, POSCONTROL pInfo);
   virtual BOOL ControlPointSet (DWORD dwID, PCPoint pVal, PCPoint pViewer);
   virtual void ControlPointEnum (PCListFixed plDWORD);
   virtual void QueryBoundingBox (PCPoint pCorner1, PCPoint pCorner2, DWORD dwSubObject);


// private:
   void CalcInfo (void);
   void RenderCurtain (POBJECTRENDER pr, PCRenderSurface prs, BOOL fFlip);
   BOOL GenerateHangingCurtain (fp fOpen, fp fFabricWidth, fp fFabricHeight,
                                             fp fRippleLen, fp fClosedWidth, fp fCompress);
   BOOL GenerateTiedCurtain (fp fOpen, fp fFabricWidth, fp fFabricHeight,
                                             fp fRippleLen, fp fClosedWidth, fp fCompress,
                                             fp fTieV, fp fTieHeight, fp fTieH);
   BOOL GenerateFoldingCurtain (fp fOpen, fp fFabricWidth, fp fFabricHeight,
                                             fp fRippleLen, fp fMinHeight, fp fSag,
                                             DWORD dwTies);
   BOOL GenerateFolding2Curtain (fp fOpen, fp fFabricWidth, fp fFabricHeight,
                                             fp fRippleLen);
   BOOL GenerateLouvers (fp fOpen, fp fWidth, fp fHeight,
                                      fp fLouverWidth, fp fAngle);
   BOOL GenerateBlind (fp fOpen, fp fWidth, fp fHeight, BOOL fFromLeft);

   DWORD       m_dwShape;        // shape of the painting, 0 = rect, 1=oval, 2=rounded
   CPoint      m_pSize;          // size of the curtain. p[0] = width, p[1] = height
   fp          m_fRippleSize;    // size of one ripple in meters - determines how thick fabric looks
   fp          m_fOpen;             // amount it's opened

   fp          m_fFromWall;      // distance that curtain hands from wall
   fp          m_fClothExtra;    // extra cloth in fabric curtains, from 1.0 to ???
   fp          m_fCompressability;  // how much can compress, from .1 to .5 ish
   fp          m_fTieV;          // tie from 0..1 of the height
   fp          m_fTieH;          // tie tightness, from 0..1
   fp          m_fHangSag;       // amount of sag, from 0...3
   DWORD       m_dwHangDivide;   // number of divisions in hanging
   fp          m_fBladeSize;     // side of louver blade
   fp          m_fBladeAngle;    // rotation of louver blade

   // calculated
   DWORD       m_dwNumPoints;    // number of points
   DWORD       m_dwNumNormals;   // number of normals
   DWORD       m_dwNumText;      // number of texture settings
   DWORD       m_dwNumVertex;    // number of vertices
   DWORD       m_dwNumPoly;      // number of polygons
   CMem        m_memPoints;      // memory containing points
   CMem        m_memNormals;     // memory containing normals
   CMem        m_memText;        // memory containign textures
   CMem        m_memVertex;      // memory containing vertices
   CMem        m_memPoly;        // memory containing polygons - array of 4-DWORs for vertices.
                                 // last DWORD can be -1 indicating just a triangle
   CNoodle     m_NoodleCircle;   // circular noodle for blinds
   CNoodle     m_NoodleSquare;   // square noodle for blinds
};
typedef CObjectCurtain *PCObjectCurtain;



/**********************************************************************************
CColumn */
#define COLUMNBRACES    4

// tapering
#define  CT_NONE              0  // no tapering from bottom/top to column
#define  CT_FULLTAPER         1  // linear taper from bottom/top width to column
#define  CT_HALFTAPER         2  // linear taper to half way
#define  CT_UPANDTAPER        3  // move up and then taper
#define  CT_SPHERE            4  // put a sphere on the end
#define  CT_DIAMOND           5  // put a diamond on the end
#define  CT_CAPPED            6  // put a bit of a cap on the end
#define  CT_SPIKE             7  // use for top, will draw a spike on the top
#define  CT_SPIKE2            8  // spikes from original to second shape

typedef struct {
   BOOL           fUse;       // set to TRUE to use one, FALSE to not
   DWORD          dwShape;    // one of NS_XXX, from CNoodle
   DWORD          dwTaper;    // how the shape tapers, CT_XXX
   CPoint         pSize;      // [0] = width, [1] = depth, [2] = height, all in meters

   // Bevelling is used whether or not there's an end base
   DWORD          dwBevelMode;// 0 for perpendicular to direction
                              // 1 for pBevelNorm is relative to direction of column
                              // 2 for pBevleNorm is relative to object coords
   CPoint         pBevelNorm; // normal (not necessarily normalized) for bevel. See dwBevelMode
   BOOL           fCapped;    // set to TRUE if should cap the very ends of column
} CBASEINFO, *PCBASEINFO;

typedef struct {
   BOOL           fUse;       // set to TRUE if use this one, FALSE if not
   DWORD          dwShape;    // one of NS_ZZZ from XNoodle
   CPoint         pSize;      // [0] = width, [1] = depth, [2] = height, all in meters
   CPoint         pEnd;       // where brace ends (intersects with floor/beam above)

   // bevel - for where the brace meets the column above. The bevel for the
   // brace meeting the column is automaically calculated
   DWORD          dwBevelMode;// 0 for perpendicular to direction
                              // 1 for pBevelNorm is relative to direction of arm
                              // 2 for pBevleNorm is relative to object coords
   CPoint         pBevelNorm; // normal (not necessarily normalized) for bevel. See dwBevelMode
} CBRACEINFO, *PCBRACEINFO;

class DLLEXPORT CColumn {
public:
   ESCNEWDELETE;

   CColumn (void);
   ~CColumn (void);

   PCMMLNode2 MMLTo (void);
   BOOL MMLFrom (PCMMLNode2 pNode);
   CColumn *Clone (void);
   BOOL CloneTo (CColumn *pNew);
   BOOL Render (POBJECTRENDER pr, PCRenderSurface prs, DWORD dwElements = (DWORD)-1);
   void QueryBoundingBox (PCPoint pCorner1, PCPoint pCorner2, DWORD dwElements = (DWORD)-1);

   BOOL StartEndFrontSet (PCPoint pStart, PCPoint pEnd, PCPoint pFront = NULL);
   BOOL StartEndFrontGet (PCPoint pStart, PCPoint pEnd, PCPoint pFront = NULL);
   BOOL SizeSet (PCPoint pSize);
   BOOL SizeGet (PCPoint pSize);
   BOOL ShapeSet (DWORD dwShape);
   DWORD ShapeGet (void);
   BOOL BaseInfoSet (BOOL fBottom, PCBASEINFO pInfo);
   BOOL BaseInfoGet (BOOL fBottom, PCBASEINFO pInfo);
   BOOL BraceInfoSet (DWORD dwBrace, PCBRACEINFO pInfo);
   BOOL BraceInfoGet (DWORD dwBrace, PCBRACEINFO pInfo);
   BOOL BraceStartSet (fp fDistance);
   fp BraceStartGet (void);
   BOOL MatrixSet (PCMatrix pm);
   BOOL MatrixGet (PCMatrix pm);

private:
   CPoint         m_pStart;   // starting point
   CPoint         m_pEnd;     // ending point
   CPoint         m_pFront;   // front vector
   CPoint         m_pSize;    // of main columne, [0] = width, [1] = depth
   DWORD          m_dwShape;  // shape of main column, NS_XXX, except no NS_CUSTOM
   CBASEINFO      m_biBottom; // base infor for the bottom
   CBASEINFO      m_biTop;    // base info for the top
   CBRACEINFO     m_biBrace[COLUMNBRACES];   // information for each of the braces
   fp         m_fBraceStart; // distance from top (in meters) where braces start
   CMatrix        m_Matrix;   // rotation matrix

   // derived variables
   BOOL           m_fDirty;   // set to TRUE if the column needs to be recalculated
   PCNoodle       m_pColumn;  // column noodle
   PCNoodle       m_pBottom;  // base noodle
   PCNoodle       m_pTop;     // top noodle
   PCNoodle       m_apBraces[COLUMNBRACES];  // for the braces

   BOOL BuildNoodles (void);
};

typedef CColumn * PCColumn;



/**********************************************************************************
CObjectNoodle */
class CNoodle;
// {722D3D0D-8F4B-4a82-B0D3-83D601511755}
DEFINE_GUID(CLSID_Noodle, 
0x624d3d0d, 0x8f41, 0x4a82, 0xb0, 0xd3, 0x83, 0xd6, 0x1, 0x51, 0x17, 0x55);
// CObjectNoodle is a test object to test the system
class DLLEXPORT CObjectNoodle : public CObjectTemplate {
public:
   CObjectNoodle (PVOID pParams, POSINFO pInfo);
   ~CObjectNoodle (void);

   virtual void Delete (void);
   virtual void Render (POBJECTRENDER pr, DWORD dwSubObject);
   virtual CObjectSocket *Clone (void);
   virtual PCMMLNode2 MMLTo (void);
   virtual BOOL MMLFrom (PCMMLNode2 pNode);
   virtual BOOL DialogQuery (void);
   virtual BOOL DialogShow (DWORD dwSurface, PCEscWindow pWindow, PCPoint pClick);
   virtual BOOL DialogCPQuery (void);
   virtual BOOL DialogCPShow (DWORD dwSurface, PCEscWindow pWindow, PCPoint pClick);
   virtual BOOL ControlPointQuery (DWORD dwID, POSCONTROL pInfo);
   virtual BOOL ControlPointSet (DWORD dwID, PCPoint pVal, PCPoint pViewer);
   virtual void ControlPointEnum (PCListFixed plDWORD);
   virtual BOOL MoveReferencePointQuery (DWORD dwIndex, PCPoint pp);
   virtual BOOL MoveReferenceStringQuery (DWORD dwIndex, PWSTR psz, DWORD dwSize, DWORD *pdwNeeded);
   virtual void QueryBoundingBox (PCPoint pCorner1, PCPoint pCorner2, DWORD dwSubObject);


//private:
   void PathSplineFromParam (void);
   DWORD ControlPointNum (void);

   CNoodle     m_Noodle;
   DWORD       m_dwDisplayControl;     // which control points are displaye,
                                       // 0=path,1=shape,2=scale,3=front,4=bevel
   DWORD       m_dwPath;               // path, 0=custom,1=line,2=circle,3=rectangle,4=coil,5=spring
   CPoint      m_pPathParam;           // paramters for non-custom path.


   CRenderSurface    m_Renderrs;       // for malloc optimization
};
typedef CObjectNoodle *PCObjectNoodle;

BOOL NoodGenerateThreeDFromSpline (PWSTR pszControl, PCEscPage pPage, PCSpline pSpline,
                                      PCSpline pSplineShape, DWORD dwUse, BOOL fRot = TRUE);



/**********************************************************************************
CObjectPainting */
// {BB0A07BD-1D45-4ab0-96EA-AFF89D1565C0}
DEFINE_GUID(CLSID_Painting, 
0xbb0a07bd, 0x1d45, 0x4ab0, 0x96, 0xea, 0xaf, 0xf8, 0x9d, 0x15, 0x65, 0xc0);

// CObjectPainting is a test object to test the system
class DLLEXPORT CObjectPainting : public CObjectTemplate {
public:
   CObjectPainting (PVOID pParams, POSINFO pInfo);
   ~CObjectPainting (void);

   virtual void Delete (void);
   virtual void Render (POBJECTRENDER pr, DWORD dwSubObject);
   virtual CObjectSocket *Clone (void);
   virtual PCMMLNode2 MMLTo (void);
   virtual BOOL MMLFrom (PCMMLNode2 pNode);
   virtual BOOL MoveReferencePointQuery (DWORD dwIndex, PCPoint pp);
   virtual BOOL MoveReferenceStringQuery (DWORD dwIndex, PWSTR psz, DWORD dwSize, DWORD *pdwNeeded);
   virtual BOOL DialogQuery (void);
   virtual BOOL DialogShow (DWORD dwSurface, PCEscWindow pWindow, PCPoint pClick);
   virtual BOOL ControlPointQuery (DWORD dwID, POSCONTROL pInfo);
   virtual BOOL ControlPointSet (DWORD dwID, PCPoint pVal, PCPoint pViewer);
   virtual void ControlPointEnum (PCListFixed plDWORD);
   virtual void QueryBoundingBox (PCPoint pCorner1, PCPoint pCorner2, DWORD dwSubObject);

//private:
   void CreateNoodle (void);
   void RenderCanvas (POBJECTRENDER pr, PCRenderSurface prs, fp fY, BOOL fBorder,
      BOOL fStretch, fp fNewWidth, fp fNewHeight);

   DWORD       m_dwStretch;      // how to stretch. 0= no stretch, 1=stretch, 2=resize
   DWORD       m_dwShape;        // shape of the painting, 0 = rect, 1=oval, 2=rounded
   CPoint      m_pSize;          // size of the painting. p[0] = width, p[1] = height, p[2] = border/mat width, p[3] = border/mat height
   DWORD       m_dwFrameProfile; // frame shape - from NS_XXX. NS_CUSTOM(0) == no frame
   CPoint      m_pFrameSize;     // size of frame - p[0] = frame width, p[1] = frame depth
   fp          m_fCanvasThick;   // canvas thickness
   BOOL        m_fTable;         // if TRUE, sits on the table, so draw at an angle, and add back to it

   // calculated
   CNoodle     m_nFrame;         // frame noodle

   CRenderSurface    m_Renderrs;    // to minimize mallocs
};
typedef CObjectPainting *PCObjectPainting;


/**********************************************************************************
CObjectColumn */
class CColumn;
// {722D3D0D-8F4B-4a82-B0D3-83D601511755}
DEFINE_GUID(CLSID_Column, 
0x624d3d0d, 0x8f41, 0x4a82, 0xb0, 0xd8, 0xa3, 0xd6, 0x1, 0x51, 0x17, 0x55);
// CObjectColumn is a test object to test the system
class DLLEXPORT CObjectColumn : public CObjectTemplate {
public:
   CObjectColumn (PVOID pParams, POSINFO pInfo);
   ~CObjectColumn (void);

   virtual void Delete (void);
   virtual void Render (POBJECTRENDER pr, DWORD dwSubObject);
   virtual CObjectSocket *Clone (void);
   virtual PCMMLNode2 MMLTo (void);
   virtual BOOL MMLFrom (PCMMLNode2 pNode);
   virtual BOOL ControlPointQuery (DWORD dwID, POSCONTROL pInfo);
   virtual BOOL ControlPointSet (DWORD dwID, PCPoint pVal, PCPoint pViewer);
   virtual void ControlPointEnum (PCListFixed plDWORD);
   virtual BOOL DialogQuery (void);
   virtual BOOL DialogShow (DWORD dwSurface, PCEscWindow pWindow, PCPoint pClick);
   virtual void QueryBoundingBox (PCPoint pCorner1, PCPoint pCorner2, DWORD dwSubObject);

   CColumn     m_Column;

   CRenderSurface    m_Renderrs;    // to minimize mallocs
};
typedef CObjectColumn *PCObjectColumn;


/***********************************************************************************
CToolTip */
class DLLEXPORT CToolTip {
   friend BOOL ToolTipPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam);
public:
   ESCNEWDELETE;

   CToolTip(void);
   ~CToolTip(void);

   BOOL Init (char *psz, DWORD dwDir, RECT *pr, HWND hWndParent);

   HWND           m_hWnd;        // window handle
   CMem           m_memTip;      // tip string

private:
   PCEscWindow    m_pWindow;       // window that will display the tip
   DWORD          m_dwDir;       // direction, from dwDir parm
   RECT           m_rBarrier;    // barrier for move around, from pr
};

typedef CToolTip *PCToolTip;


/***********************************************************************************
CIconButton */
#define  IBFLAG_DISABLEDLIGHT    0x0001   // show a disabled light indicating selctable, UNLESS _BLUELIGHT is on
#define  IBFLAG_BLUELIGHT        0x0002   // show a blue light indicating current selection
#define  IBFLAG_DISABLEDARROW    0x0004   // show a disabled arrow UNLESS _REDARROW is set
#define  IBFLAG_REDARROW         0x0008   // show a red arrow indicating that specified meaning of pointer

#define  IBFLAG_SHAPEMASK        0x00f0   // so can isolate the shape
#define  IBFLAG_SHAPE_TOP        0x0010   // shaped so tab goes on top
#define  IBFLAG_SHAPE_RIGHT      0x0020   // shaped so tab goes on right
#define  IBFLAG_SHAPE_BOTTOM     0x0030   // shaped so tab goes on bottom
#define  IBFLAG_SHAPE_LEFT       0x0040   // so tab on left
#define  IBFLAG_SHAPE_LLOOP      0x0050   // left end of a loop
#define  IBFLAG_SHAPE_RLOOP      0x0060   // right end of a loop
#define  IBFLAG_SHAPE_CLOOP      0x0070   // center end of a loop
#define  IBFLAG_SHAPE_TLOOP      0x0080   // top of a loop
#define  IBFLAG_SHAPE_BLOOP      0x0090   // bottom of a loop
#define  IBFLAG_SHAPE_VLOOP      0x00a0   // center of a vertical loop

#define  IBFLAG_ENDPIECE         0x0100   // if it's an end-piece needs to draw finishing line

#define  IBFLAG_MBUTTON          0x1000   // show an M to indicate the use middle button
#define  IBFLAG_RBUTTON          0x2000   // show an R to indicate the use of right button

DLLEXPORT void ButtonBitmapReleaseAll (void);

class DLLEXPORT CIconButton {
   friend LRESULT CALLBACK CIconButtonWndProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
public:
   ESCNEWDELETE;

   CIconButton(void);
   ~CIconButton(void);

   BOOL Init (DWORD dwJPGRes, HINSTANCE hInstance, char *pszAccel, char *pszName, char *pszTip,
      DWORD dwDir, RECT *pr, HWND hWndParent, DWORD dwID, DWORD dwPixels = M3DBUTTONSIZE);
   char *NameGet (void)
      {
         return (char*) m_memName.p;
      };
   BOOL Move (RECT *pr);
   void Enable (BOOL fEnable);
   void FlagsSet (DWORD dwFlags);
   DWORD FlagsGet (void);
   BOOL TestAccelerator (DWORD dwMsg, WPARAM wParam, LPARAM lParam);
   void ColorSet (COLORREF cDark, COLORREF cMed, COLORREF cLight, COLORREF cOut);

   HWND           m_hWnd;        // window handle
   CMem           m_memTip;      // tip string
   CMem           m_memName;     // name string
   HBITMAP        m_hbmpMask;    // mask bitmap
   HBITMAP        m_hbmpColorLight;   // image with color bitmap
   HBITMAP        m_hbmpColorMed;   // image with color bitmap
   HBITMAP        m_hbmpColorDark;   // image with color bitmap
   HBITMAP        m_hbmpBW;      // black-and white disabled image
   HBITMAP        m_hbmpRed;     // redding color so know that can click
   HBITMAP        m_hbmpLowContrast;   // show as low contrast
   DWORD          m_dwID;        // notification command ID if clicked
   DWORD          m_dwDir;       // direction of the tooltip
   PCToolTip      m_pTip;        // tooltip
   BOOL           m_fTimer;      // set to true if the timer is on
   DWORD          m_dwTimerOn;   // mumber of milliseconds timer is on
   int            m_iHeight, m_iWidth; // size of bitmap
   char           m_szAccel[32]; // acclerator
   WORD           m_vkAccel;     // virtual key for accelerator
   DWORD          m_dwAccelMsg;       // message accelerator is looking for, WM_CHAR, WM_SYSCHAR, WM_KEYDOWN
   BOOL           m_fAccelControl;  // if TRUE control down
   BOOL           m_fAccelAlt;      // if TRUE, alt down
   COLORREF       m_cBackDark;   // dark background color (not in tab)
   COLORREF       m_cBackMed;    // medium background color (for tab not selected)
   COLORREF       m_cBackLight;  // light background color (for selected tab)
   COLORREF       m_cBackOutline;   // outline color

private:
   LRESULT WndProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

   DWORD          m_dwFlags;     // current display flags
};

typedef CIconButton *PCIconButton;

/***********************************************************************************
CButtonBar */
class DLLEXPORT CButtonBar {
public:
   ESCNEWDELETE;

   CButtonBar(void);
   ~CButtonBar(void);

   BOOL Init (RECT *pr, HWND hWndParent, DWORD dwDir);
   void Move (RECT *pr);
   void Clear (DWORD dwGroup = 3);
   PCIconButton ButtonAdd (DWORD dwPosn, DWORD dwJPGRes, HINSTANCE hInstance,
      char *pszAccel, char *pszName, char *pszTip, DWORD dwID);
   void AdjustAllButtons (void);
   void FlagsSet (DWORD dwID, DWORD dwFlag, DWORD dwGroup = 3);
   void Show (BOOL fShow);
   BOOL TestAccelerator (DWORD dwMsg, WPARAM wParam, LPARAM lParam);
   void ColorSet (COLORREF cDark, COLORREF cMed, COLORREF cLight, COLORREF cOut);
   PCIconButton ButtonExists (DWORD dwID);

   HWND           m_hWndParent;  // parent window handle
   DWORD          m_dwDir;       // direction of the tooltip
   RECT           m_rect;        // rectangle where this button bar is located
   CListFixed     m_alPCIconButton[3]; // array of lists. Each list contains icon buttons
                                 // 0 is the top/left, 1 is centered, 2 is bottom/right
   BOOL           m_fShow;       // if showing buttons then TRUE
   COLORREF       m_cBackDark;   // dark background color (not in tab)
   COLORREF       m_cBackMed;    // medium background color (for tab not selected)
   COLORREF       m_cBackLight;  // light background color (for selected tab)
   COLORREF       m_cBackOutline;   // outline color


private:
   void RecalcEndPiece (void);

};
typedef CButtonBar *PCButtonBar;


/***********************************************************************************
CHouseView */

/* scroll action */
#define SA_NONE            0        // doesnt do anything
#define SA_XABSOLUTE       10       // move camrera X in absolute location
#define SA_YABSOLUTE       11       // move camera Y in avsoulut location
#define SA_ZABSOLUTE       12       // move camera Z in absolute location
#define SA_XROTABSOLUTE    20       // rotation around X in absolute tersm - radians
#define SA_YROTABSOLUTE    21       // rotation around Y in absolute tersm - radians
#define SA_ZROTABSOLUTE    22       // rotation around Z in absolute tersm - radians
#define SA_LRRELATIVE      30       // move camera LR relative to looking at
#define SA_FBRELATIVE      31       // move camera FB relative to looking at (positive numbers are forward)
#define SA_UDRELATIVE      32       // move camera UD relative to looking at
#define SA_LRROTRELATIVE   40       // rotate around LR relative to looking at
#define SA_FBROTRELATIVE   41       // rotate around FB relative to looking at (positive numbers are forward)
#define SA_UDROTRELATIVE   42       // rotate around UD relative to looking at
#define SA_FLATZOOMABSOLUTE    50       // absoluate zoom, with length being # meters visible across
#define SA_FLATZOOMRELATIVE    51       // relative zoom, with numbers being multiplier
#define SA_FLATLRABSOLUTE  52       // scroll LR, avsolute
#define SA_FLATLUDABSOLUTE 53       // scroll UD, absolute
#define SA_PERSPFOV        60       // field of view
#define SA_WALKZRELATIVE   70       // up/down in Z

typedef struct {
   DWORD             dwAction;   // action to take, SA_XXX
   fp                fMin;    // value when scroll bar is all the way to left/top
   fp                fMax;    // value when scroll bar is all the way to right/bottom
   BOOL              fLog;    // set tot TRUE if logarithmic
   BOOL              fRelative;  // changes are relative
} SCROLLACTION, *PSCROLLACTION;


typedef struct {
   HMONITOR    hMonitor;      // monitor
   HDC         hDCMonitor;    // device context for monitor
   RECT        rMonitor;      // location of monitor in virtual space
   RECT        rWork;         // work area
   BOOL        fPrimary;      // set to true if primary monitor
   DWORD       dwUsed;        // count of number of other CHouseView windows using this monitor
} XMONITORINFO, *PXMONITORINFO;

typedef struct {
   DWORD       dwIndex;       // filled in later with index (not written to MML)
   WCHAR       szName[64];    // name of this view
   DWORD       dwCamera;      // camera model, CAMERAMODEL_XXX
   CPoint      pCenter;       // used as center
   CPoint      pTrans;        // translation or look from
   CPoint      pRot;          // used for XYZ rotation
   fp          fFOV;          // field of view
} FAVORITEVIEW, *PFAVORITEVIEW;

extern CListFixed    gListPCHouseView;

#define VIEWTHUMB       5     // 5 thumbnails, 1 main + 4 small

#define VIEWWHAT_WORLD        0  // looking at the house
#define VIEWWHAT_OBJECT       1  // looking at an object
#define VIEWWHAT_POLYMESH     2  // modifying a polymesh object
#define VIEWWHAT_BONE         3  // modifying the bone object

// used for viewwhat_polymesh for m_dwViewSub
#define VSPOLYMODE_POLYEDIT   0  // edit poltying 
#define VSPOLYMODE_ORGANIC    1  // organic modelling
#define VSPOLYMODE_MORPH      2  // morph modelling
#define VSPOLYMODE_BONE       3  // bone modelling
#define VSPOLYMODE_TEXTURE    4  // texture modelling
#define VSPOLYMODE_TAILOR     5  // tailor modelling

class CSceneSet;
typedef CSceneSet *PCSceneSet;

class DLLEXPORT CHouseView : public CViewSocket {
   friend LRESULT CALLBACK CHouseViewWndProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
public:
   ESCNEWDELETE;

   CHouseView(PCWorldSocket pWorld, PCSceneSet pSceneSet, DWORD dwViewWhat,
      GUID *pgCode = NULL, GUID *pgSub = NULL, PCWorld pWorldObj = NULL, PCWorldSocket pWorldPolyMesh = NULL);
   ~CHouseView(void);

   BOOL Init (DWORD dwMonitor = (DWORD)-1);
   CHouseView *Clone (DWORD dwMonitor = (DWORD)-1);
   BOOL MMLFrom (PCMMLNode2 pNode);
   PCMMLNode2 MMLTo (void);
   void UpdateScrollBars (void);
   void UpdateAllButtons (void);
   void UpdateCameraButtons (void);
   void UpdatePolyMeshButton (void);
   DWORD ButtonExists (DWORD dwID, DWORD dwEnabledFor);
   void SetPointerMode (DWORD dwMode, DWORD dwButton);
   void SetProperCursor (int iX, int iY);
   BOOL PointInImage (int iX, int iY, DWORD *pdwImageX = NULL, DWORD *pdwImageY = NULL);
   void RenderUpdate (DWORD dwReason, BOOL fForce = FALSE);
   void InvalidateDisplay (void);
   void ClipPointUI (DWORD dwX, DWORD dwY);
   void GridFromPointUI (DWORD dwX, DWORD dwY);
   void NextPointerMode (DWORD dwMode);
   void ForcePaint (void);

   BOOL FileSave (BOOL fForceAsk);
   BOOL ObjectSave (void);
   BOOL FileOpen (void);
   BOOL FileNew (void);

   void CameraDeleted (void);
   void CameraMoved (PCMatrix pm);
   void CameraPosition (void);
   void CameraFromObject (GUID *pgObject);
   BOOL CameraToObject (GUID *pgObject);
   BOOL LookAtArea (PCPoint pMin, PCPoint pMax, DWORD dwFrom);

   // for move and rotate - either call into singly selected object, or group
   DWORD MoveReferenceCurQuery (void);
   BOOL MoveReferenceCurSet (DWORD dwIndex);
   BOOL MoveReferencePointQuery (DWORD dwIndex, PCPoint pp);
   BOOL MoveReferenceStringQuery (DWORD dwIndex, PWSTR psz, DWORD dwSize, DWORD *pdwNeeded);
   void MoveReferenceMatrixGet (PCMatrix pm);
   void MoveReferenceMatrixSet (PCMatrix pm, PCMatrix pmWas);

   void UpdateToolTip (char *psz, BOOL fForce = FALSE);
   void UpdateToolTipByPointerMode (void);

   BOOL ObjectMerge (DWORD dwMergeTo);
   BOOL ObjectAttach (DWORD dwAttachTo, DWORD dwX, DWORD dwY);
   BOOL ClipboardCopy (void);
   BOOL ClipboardCut (void);
   BOOL ClipboardDelete (BOOL fUndoSnapshot);
   BOOL ClipboardPaste (void);
   void ClipboardUpdatePasteButton (void);
   void MoveOutOfPurgatory (DWORD dwX, DWORD dwY, BOOL fCanEmbed, PCPoint papDrag = NULL, DWORD dwDrag = 0);
   void SnapEmbedToGrid (PCObjectSocket pCont, DWORD dwSurf, PTEXTUREPOINT pHV);
   void ObjectCreateMenu (void);
   void QualityApply (void);
   void QualitySync (void);
   void CreateApplicationMenu (void);
   void GridMove (PCPoint p, DWORD dwChangeBits = 0x07);

   void ScrollActionDefault (DWORD dwModel, DWORD dwScroll, DWORD dwAction);
   void ScrollActionInit (void);
   BOOL ScrollActionFromScrollbar (int nPos, DWORD dwScroll, PCRenderTraditional pRender);
   void ScrollActionUpdateScrollbar (HWND hWnd, DWORD dwScroll, PCRenderTraditional pRender);

   BOOL FavoriteViewRemove (DWORD dwIndex);
   BOOL FavoriteViewAdd (PFAVORITEVIEW pfv);
   BOOL FavoriteViewGet (DWORD dwIndex, PFAVORITEVIEW pfv);
   DWORD FavoriteViewNum (void);
   void FavoriteViewGen (PWSTR pszName, PFAVORITEVIEW pfv);
   void FavoriteViewApply (PFAVORITEVIEW pfv);

   BOOL RotationRemember (PCPoint pCenter, PCPoint pCenterOrig, PCPoint pVector, PCMatrix pmOrig,
      DWORD dwX, DWORD dwY, BOOL fCamera = FALSE);
   BOOL RotationDrag (DWORD dwX, DWORD dwY, PCMatrix pNew, BOOL fIncludeOrig = TRUE,
      BOOL fSkipSphere = FALSE,BOOL fCamera = FALSE);
   void ArrowKeys (int iX, int iY);

   // CViewSocket
   virtual void WorldChanged (DWORD dwChanged, GUID *pgObject);
   virtual void WorldUndoChanged (BOOL fUndo, BOOL fRedo);
   virtual void WorldAboutToChange (GUID *pgObject);

   // bone
   PCObjectBone BoneObject2 (DWORD *pdwFind = NULL);
   void BoneSelIndividual (DWORD dwX, DWORD dwY);
   void BoneEdit (DWORD dwX, DWORD dwY);
   BOOL BoneObjectMerge (DWORD dwMerge);
   BOOL BonePrepForDrag (DWORD dwX, DWORD dwY, BOOL fInOut, BOOL fFailIfCantChange);
   void BoneAddRect (PCObjectBone pb, DWORD dwStartBone, RECT *pr);
   BOOL BoneAdd (DWORD dwBone, DWORD dwX, DWORD dwY);

   // polymesh
   PCObjectPolyMesh PolyMeshObject2 (DWORD *pdwFind = NULL);

   DWORD          m_dwViewWhat;  // values from VIEWWHAT_XXX
   DWORD          m_dwViewSub;   // sub-view, depends upon m_dwViewWhat
                                 // for VIEWHAT_POLYMODE, VSPOLYMOD_XXX
   COLORREF       m_cBackDark;   // dark background color (not in tab)
   COLORREF       m_cBackMed;    // medium background color (for tab not selected)
   COLORREF       m_cBackLight;  // light background color (for selected tab)
   COLORREF       m_cBackOutline;   // outline color

   PCObjectCamera m_pCamera;     // camera object
   HWND           m_hWnd;        // window handle
   BOOL           m_fSmallWindow;   // if TRUE, draw as a small window
   BOOL           m_fHideButtons;   // Valid if m_fSmallWindow set. If TRUE, window so small that hide buttons, FALSE show but smaller
   BOOL           m_fWorldChanged;  // bits to indicate what part of the world has changed since last render
   HWND           m_ahWndScroll[4]; // scrollbars for top=0, right=1, bottom=2, left=3
   BOOL           m_fDisableChangeScroll; // doesn't allow scroll-bar to be automatically modified during a scroll
   BOOL           m_fScrollRelCache;   // set to TRUE if have cached the relative scroll information for this go around
   fp             m_fScrollCacheFOV;   // for relative scroll
   fp             m_fScrollCacheScale; // for relative scroll
   CPoint         m_pScrollCacheCenterOfRot; // for relative scroll
   CPoint         m_pScrollCacheRot;   // for relative scroll
   CPoint         m_pScrollCacheCamera;   // for relative scroll
   PCButtonBar    m_pbarGeneral;  // button bar for general buttons
   PCButtonBar    m_pbarView;     // bar specific to the way the data is viewed
   PCButtonBar    m_pbarObject;   // bar speciifc to the object that's selected
   PCButtonBar    m_pbarMisc;     // miscellaneous
   PCIconButton   m_pUndo, m_pRedo; // undo and redo buttons
   PCIconButton   m_pPaste;         // paste button
   PCIconButton   m_pObjControlNSEW;    // control drag button
   PCIconButton   m_pObjControlNSEWUD;    // control drag button
   PCIconButton   m_pObjControlUD;    // control drag button
   PCIconButton   m_pObjControlInOut;    // control drag button
   CListFixed     m_lTraceInfo;     // tracing information
   CListFixed     m_listNeedSelect; // list of buttons that need selection before they're enabled
   CListFixed     m_listNeedSelectNoEmbed;   // enable if selected, but not if any objects are embedded
   CListFixed     m_listNeedSelectOnlyOneEmbed; // enable if selected, but if any objects are embedded than can be the only one
   CListFixed     m_listNeedSelectOneEmbed;  // only enable if only one object, and that's embedded
   PCToolTip      m_pToolTip;    // tool tip for information to user
   //CMem           m_memToolTip;  // what's displayed in the tooltip
   DWORD          m_dwToolTip;   // tooltip corner. 0 = UL, 1= UR, 2=LR, 3=LL
   DWORD          m_dwClipFormat;   // clipboard format for ASP
   HWND           m_hWndClipNext;   // clibboard viewer next window
   DWORD          m_fTimerWaitToDraw;  // if this is true, there's a timer set that causes the window to draw later
   DWORD          m_fTimerWaitToTip;   // if TRUE, there's a timer on that waits for a tooltip
   CMem           m_memTip;         // contain string of the tooltip
   BOOL           m_fWaitToDrawForce;  // if TRUE, forces not to wait to draw

   // thumbnails
   DWORD          m_dwThumbnailShow;   // 0 for no thumbnails, 1 for to the right, 2 for bottom
   DWORD          m_dwThumbnailCur;    // current thumbnail that's enlarged from 1..VIEWTHUMB-1
   RECT           m_arThumbnailLoc[VIEWTHUMB];  // pixel coordinates of thumbnails
   BOOL           m_afThumbnailVis[VIEWTHUMB];  // set to true if the thumbnail is visible

   // grid
   BOOL           m_fGridDisplay;      // if TRUE, display the 3D grid
   BOOL           m_fGridDisplay2D;    // if TRUE, display the 2D grid in flat mode
   fp         m_fGrid;             // size of grid, in meters. 0 for none
   fp         m_fGridAngle;        // angular-grid, in radiuans. 0 for none
   CPoint         m_pGridCenter;       // center of grid.
   CPoint         m_pGridRotation;     // rotate the grid by this
   CMatrix        m_mGridToWorld;      // convert from grid coords to world coords
   CMatrix        m_mWorldToGrid;      // convert from world coords to grid coords
   PCListFixed    m_plZGrid;           // temporary list of doubles for Z - hold down shift and move up/down
   CPoint         m_pRotCenter;        // temporary used for rotation
   CPoint         m_pRotCenterOrig;        // temporary used for rotation
   CPoint         m_pRotVector;        // temporary used for rotation
   CPoint         m_pRotClickOrig;     // temporary used for rotation
   CMatrix        m_mRotOrig;          // temporary used for rotation
   CMatrix        m_mRotInv;           // temporary used for rotation, invert to simulate original click at P=1,0,0
   BOOL           m_fRotClickFront;    // set to TRUE if clicked on the front part of the rotation, FALSE if back
   fp             m_fRotRadius;        // temporary used for rotation

   CImage         m_aImage[VIEWTHUMB];       // image object of what's to be drawn
   PCWorldSocket        m_pWorld;      // world
   PCSceneSet     m_pSceneSet;   // animation scene set. Can be null.
   CWorld         m_Purgatory;   // used to store objects about to be pasted or added
   PCRenderTraditional m_apRender[VIEWTHUMB];  // rendering object
   DWORD          m_dwImageScale;   // amount to scale the image's pixels by, IMAGESCALE = max res, 2xIMAGESCALE=1/2 res, etc.
   DWORD          m_dwQuality;      // quality type to use
   DWORD          m_dwCameraButtons;   // Camera model buttons shown by m_pbarView. CAMERAMODEL_XXX. If not one of those then set for nothing
   DWORD          m_adwPointerMode[3];  // what the meaning of the pointer is. IDC_XXX. [0]=left,[1]=middle,[2]=right
   DWORD          m_dwPointerModeLast; // last pointer mode used, so can swap back and forth
   POINT          m_pntButtonDown;  // point on the screen where the button was pressed down
   BOOL           m_dwButtonDown;    // 0 if no button is down, 1 for left, 2 for middle, 3 for right
   BOOL           m_fCaptured;      // set to true if mouse captured
   POINT          m_pntMouseLast;   // if the button is down, this reflects where the button last was
   //fp         m_fScaleScroll;   // scaling factor for scrolling. meaning changes depnding on pointer mode
   CPoint         m_pScaleScrollX;  // change translation if move cursor to right
   CPoint         m_pScaleScrollY;  // change translation if move cursor down
   DWORD          m_dwMoveTimerInterval;  // number of millisecond between timer clicks for mouse-move, click operation
   DWORD          m_dwMoveTimerID;  // timer ID. 0 if not on
   CPoint         m_pMoveRotReferenceInObj;  // when moving/rotating, the reference point in object space
   CPoint         m_pMoveRotReferenceInWorld;   // when moving/rotating, the reference point in world space
   CMatrix        m_pMoveRotReferenceMatrix;  // initial matrix for the object being moved/rotated
   CMatrix        m_pMoveRotReferenceWas;    // what the matrix was after last setting
   CPoint         m_pMoveRotReferenceLLT;    // latitude=x, longitude=z, tilt=y, in that order
   DWORD          m_dwObjControlObject;   // object that dragging
   DWORD          m_dwObjControlID;       // control ID
   CPoint         m_pObjControlClick;     // location (in object space) where clicked
   CPoint         m_pObjControlOrig;      // location (in object space) where control point first was - can use to ensuring draggin accurate
   fp         m_fObjControlOrigZ;     // original Z depth (relaive to user) where clicked
   WCHAR          m_szObjName[128];       // object name being inserted
   BOOL           m_fPastingEmbedded;  // if TRUE, the object that adding is embedded

   fp             m_fViewScale;        // largest size model is expected to get - used for scroll bars and zoom
   fp             m_fVWWalkSpeed;   // walking speed in meters per second
   fp             m_fVWalkAngle;     // angle grid for walking
   fp             m_fClipTempAngle; // temporary angle used for clip plane by line
   BOOL           m_fClipAngleValid;   // set to true if the clipping angle is valid
   CPoint         m_pClipPlaneClick;   // where clicked on
   CPoint         m_pClipPlaneSurfaceH;   // surface H vector
   CPoint         m_pClipPlaneSurfaceV;   // surface V vector
   BOOL           m_fClipPlaneSurfaceValid;  // TRUE if surface vectors valid
   CPoint         m_pClipPlaneCameraH;    // camera H vector
   CPoint         m_pClipPlaneCameraV;    // camera V vector
   fp         m_fClipPlaneOffset;     // offset the clipping plane this much perpendicular to point
   BOOL           m_fSelRegionWhollyIn;   // option - if TRUE any selection by region must have objects wholly in
   BOOL           m_fSelRegionVisible;    // option - Only select region if object is visible
   CPoint         m_pObjectLocationGrid;  // scaling for the object location grid
   CPoint         m_pObjectControlGrid;   // grid for snapping control points
   CPoint         m_pObjectOrientGrid;    // scaling for object orientation grid. 2 = long, 1=tilt, 0=lat
   DWORD          m_dwMoveReferenceCur;   // current move reference for a selection of objects
   fp         m_fOpenStart;           // starting value for open/close when drag

   BOOL           m_fMoveEmbedded;        // when moving/rotating, set to true if doing so to embedded object
   TEXTUREPOINT   m_MoveEmbedHV;          // original location of moved embedded object
   fp         m_fMoveEmbedRotation;   // original rotation of moved embedded object
   DWORD          m_dwMoveEmbedSurface;   // surface that embedded object is on
   TEXTUREPOINT   m_MoveEmbedClick;       // HV where clicked
   TEXTUREPOINT   m_MoveEmbedOld;         // location of mouse (in HV) last time got data

   // polymesh editing
   GUID           m_gObject;              // object editing
   CPoint         m_pCenterWorld;         // center of the world. Normally 0,0,0, but center of polymesh object if have one
   BOOL           m_fShowOnlyPolyMesh;    // if TRUE then when render show only the polymesh
   DWORD          m_dwPolyMeshSelMode;    // last polymesh selection mode
   PCPolyMesh     m_pPolyMeshOrig;        // original polymesh before started changing
   DWORD          m_adwPolyMeshMove[2];   // used to indicate object being moved, or -1 if moving selection
   DWORD          m_dwPolyMeshMoveSide;   // side where polymesh move came from, -1 if dont know
   DWORD          m_adwPolyMeshSplit[2][2];  // [x][y], where x = 0 or 1 for start/end, y=0 or 1 for 1st vert or 2nd vert
   RECT           m_rPolyMeshLastSplitLine;  // location where last split occurred, for invalidate
   CListFixed     m_lPolyMeshVert;           // list of DWORDs indicated what vertices selected for creating new polygon
   DWORD          m_fAirbrushTimer;       // airbrush timer
   DWORD          m_dwPolyMeshBrushPoint; // amount of pointiness in brush, 0 ..3
   DWORD          m_dwPolyMeshBrushStrength; // brush strength, from 1..10
   DWORD          m_dwPolyMeshBrushAffectOrganic;   // same as pased to CPolyMesh::OrganicMove
   DWORD          m_dwPolyMeshBrushAffectTailor;   // painting mode for tailor
   DWORD          m_dwPolyMeshBrushAffectMorph;   // same as pased to CPolyMesh::MorphMove
   DWORD          m_dwPolyMeshBrushAffectBone;  // how brush affects the bone
   DWORD          m_dwPolyMeshMaskColor;  // color used for mask. See OrganicMove()
   BOOL           m_fPolyMeshMaskInvert;  // TRUE if invert meaning of colors
   DWORD          m_dwPolyMeshMagPoint;   // amount of pointiness for magnet, 0..3
   DWORD          m_dwPolyMeshMagSize;    // magnet size, 0..4
   CPoint         m_pPolyMeshClick;       // point in polymesh space where clicked
   HWND           m_hWndPolyMeshList;     // list box for polygon mesh
   RECT           m_rPolyMeshList;        // location where polygon mesh should go
   DWORD          m_dwPolyMeshCurMorph;   // currently selected item, set to -2 to indicate shouldnt update at all
   HFONT          m_hPolyMeshFont;        // used for lsit boxes
   DWORD          m_dwPolyMeshCurText;    // current texture index being used
   BOOL           m_fPolyMeshTextDisp;    // set to TRUE if displaying texture, FALSE if not
   TEXTUREPOINT   m_tpPolyMeshTextDispHV; // HV location of texture being displayed
   POINT          m_pPolyMeshTextDispPoint;  // xy point where m_tpPolyMeshTextDispHV should be displayed
   fp             m_fPolyMeshTextDispScale;  // one unit of HV is this many pixels
   DWORD          m_dwPolyMeshTextDispLastVert; // last vertex hovered over for texture display, or -1 if none

private:
   LRESULT WndProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
   LRESULT ButtonDown (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, DWORD dwButton);
   LRESULT ButtonUp (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, DWORD dwButton);
   LRESULT MouseMove (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
   LRESULT Paint (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
   void PaintFlatGrid (DWORD dwThumb, HDC hDC);
   PCListFixed IncludeEmbeddedObjects (DWORD *padw, DWORD dwNum);
   void LookAtPointFromDistance (PCPoint pAt, PCPoint pFrom, double fNewScale = 1);
   BOOL NormalFromPoint (DWORD dwX, DWORD dwY, PCPoint pClick, PCPoint pN, PCPoint pH, PCPoint pV);
   void CalcThumbnailLoc (void);
   DWORD NearSelect (DWORD dwX, DWORD dwY);
   DWORD NearestCP (DWORD dwX, DWORD dwY, DWORD dwID);
   BOOL MakeSureAllWindowsClosed (BOOL fCloseAll);

   PCObjectSocket PolyMeshObject (DWORD *pdwFind = NULL);
   BOOL PolyMeshBoundBox (PCPoint pab);
   void PolyMeshSettings (void);
   DWORD PolyMeshVertFromPixel (DWORD dwX, DWORD dwY);
   void PolyMeshSelIndividual (DWORD dwX, DWORD dwY, BOOL fControl);
   void PolyMeshSelRenderVert (PCRenderTraditional pRender, PCImage pImage, PCPoint pWorld);
   void PolyMeshSelRender (PCObjectPolyMesh pm, DWORD dwObjectID,
                                        PCRenderTraditional pRender, PCImage pImage);
   BOOL PolyMeshEdgeFromPixel (DWORD dwX, DWORD dwY, DWORD *padwEdge);
   void PolyMeshEdgeFromSide (PCObjectPolyMesh pm, DWORD dwSide1, DWORD dwSide2,
                                       PCListFixed plEdge);
   DWORD PolyMeshSideFromPixel (DWORD dwX, DWORD dwY);
   void PolyMeshSelRegion (RECT *pr, CPoint apVolume[2][2][2]);
   BOOL PolyMeshPrepForDrag (DWORD dwX, DWORD dwY, BOOL fInOut, BOOL fFailIfCantChange);
   BOOL PolyMeshObjectFromPixel (DWORD dwX, DWORD dwY, BOOL fPrefSel = TRUE);
   BOOL PolyMeshValidEdgeFromSide (PCObjectPolyMesh pm, DWORD dwSide, PCListFixed plValidEdge);
   DWORD PolyMeshSideSplit (PCObjectPolyMesh pm);
   BOOL PolyMeshSideSplitRect (PCObjectPolyMesh pm, RECT *pr);
   BOOL PolyMeshVertToPixel (PCObjectPolyMesh pm, POINT pCursor, PCListFixed plPoint);
   void PolyMeshVertToRect (PCObjectPolyMesh pm, POINT pCursor, RECT *pr);
   void PolyMeshBrushApply (DWORD dwPointer, POINT *pNew);
   BOOL PolyMeshObjectMerge (DWORD dwMerge);
   void PolyMeshCreateAndFillList (void);
   LRESULT PolyMeshListBox (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
   void PolyMeshPaintTexture (HDC hDC);
   void PolyMeshTextureMouseMove (DWORD dwX, DWORD dwY);

   void RectImageToScreen (RECT *pr, BOOL fExtra = FALSE);
   DWORD IsEmbeddedObjectSelected (void);
   DWORD NumEmbeddedObjectsSelected (void);
   CPoint         m_pVW;            // temp for mouse scrolling
   fp         m_fVWLong, m_fVWLat, m_fVWTilt, m_fVWFOV;
   CPoint         m_pV3D;           // temp for mouse scrolling
   fp         m_fV3DRotZ, m_fV3DRotX, m_fV3DRotY, m_fV3DFOV;
   fp         m_fCFLong, m_fCFTilt, m_fCFTiltY, m_fCFScale, m_fCFTransX, m_fCFTransY; // temp for mouse viewing
   CPoint         m_pCameraCenter;  // camera centering used for flat and object perspective
   SCROLLACTION   m_aScrollAction[3][4];     // scrollvar action, [0=flat,1=model,2=walk][0=top,1=right,2=bottom,3=left]
};

typedef CHouseView *PCHouseView;

// VIEWOBJ - List of CHouseView looking at objects
typedef struct {
   PCHouseView       pView;      // view that's being used
   PCWorld           pWorld;     // world involved - when gListVIEWOBJ item is deleted, must delete pWorld
   GUID              gCode;      // object code
   GUID              gSub;       // object sub
   WCHAR             szMajor[128], szMinor[128], szName[128];  // object name
} VIEWOBJ, *PVIEWOBJ;

// VIEWOBJECT - List of CHouseView used for looking at polymesh
typedef struct {
   PCHouseView       pView;      // view being used
   GUID              gObject;    // object identifier
   PCWorldSocket     pWorld;     // world it's in
} VIEWOBJECT, *PVIEWOBJECT;

extern CListFixed    gListVIEWOBJ;      // list of views looking at objects
extern DWORD         gdwMouseWheel;      // for mouse wheel

DLLEXPORT PCHouseView FindViewForWorld (PCWorldSocket pWorld);
DLLEXPORT BOOL IsAttribNameUnique (PWSTR pszName, PCListFixed plCOEAttrib,
                         DWORD dwExclude = -1);
DLLEXPORT void MakeAttribNameUnique (PWSTR pszName, PCListFixed plCOEAttrib, DWORD dwExclude = -1);
DLLEXPORT void QualityApply (DWORD dwQuality, PCRenderTraditional pRender);
DLLEXPORT PCListFixed ListVIEWOBJ (void);
DLLEXPORT PCListFixed ListVIEWPOLYMESH (void);
DLLEXPORT PCListFixed ListPCHouseView (void);
DLLEXPORT BOOL ObjectViewShowHide (PCWorldSocket pWorld, GUID *pgObject, BOOL fShow);
DLLEXPORT BOOL ObjectViewNew (PCWorldSocket pWorld, GUID *pgObject, DWORD dwViewWhat);
DLLEXPORT BOOL ObjectViewDestroy (PCWorldSocket pWorld, GUID *pgObject);

/**********************************************************************************
Main */
class CSceneSet;

extern HINSTANCE   ghInstance;
// extern CWorld      pgWorld;              // world
extern CSceneSet   pgSceneSet;           // scene set attached to world
extern char        gszAppPath[256];     // application path
extern char        gszAppDir[256];      // application directory
extern CMem        gMemTemp;            // temporary memory
extern char gszFileWantToOpen[256];   // want to open this file
extern DWORD       gdwCPSize;        // control point size
extern BOOL        gfCanModifyLibrary; // set to true if can modify library
DLLEXPORT void MakeWindowText (DWORD dwRenderShard, char *psz);
DLLEXPORT void SetViewTitles (DWORD dwRenderShard);
DLLEXPORT void CloseAllViewsDown (void);
DLLEXPORT BOOL FileSave (DWORD dwRenderShard, char *pszFile);
DLLEXPORT void MRUListEnum (PCListVariable pMRU);
DLLEXPORT void MRUListAdd (char *pszFile);
DLLEXPORT int M3DMainLoop (LPSTR lpCmdLine, int nShowCmd);
DLLEXPORT BOOL CanModifyLibrary (void);
DLLEXPORT char *MainFileWantToOpen (void);

DLLEXPORT BOOL M3DInit (DWORD dwRenderShard, BOOL fLibraryAppDir, PCProgressSocket pProgress);
DLLEXPORT BOOL M3DEnd (DWORD dwRenderShard, PCProgressSocket pProgress);
DLLEXPORT BOOL M3DFileOpen (DWORD dwRenderShard, PWSTR pszFile, BOOL *pfFailedToLoad, PCProgressSocket pProgress,
                            BOOL fViewLoad /*= TRUE*/, PCRenderTraditional *ppRender /*= NULL*/);
class CLibrary;
DLLEXPORT BOOL M3DFileOpenRemoveUser (DWORD dwRenderShard, PWSTR pszFileSource, PCMegaFile pmfDest,
                            PWSTR pszFileDest, CLibrary * pLibraryUserClone);
DLLEXPORT void M3DFileNew (DWORD dwRenderShard, BOOL fCreateGroundSky /*= TRUE*/);
DLLEXPORT PCWorld M3DWorldSwapOut (DWORD dwRenderShard, PCSceneSet* ppScene);
DLLEXPORT void M3DWorldSwapIn (PCWorld pWorld, PCSceneSet pScene);


/*******************************************************************************
Sun */
DLLEXPORT void SunVector (DFTIME dwTime, DFDATE dwDate, fp fLatitude, PCPoint pSun);
DLLEXPORT fp MoonVector (DFTIME dwTime, DFDATE dwDate, fp fLatitude, PCPoint pMoon);
DLLEXPORT fp DFTIMEToTimeInDay (DFTIME dwTime);
DLLEXPORT fp DFDATEToTimeInYear (DFDATE dwDate);
DLLEXPORT void TimeInYearDayToDFDATETIME (fp fTimeInDay, fp fTimeInYear, DWORD dwYear,
                                DFTIME *pdwTime, DFDATE *pdwDate);
DLLEXPORT void SunVector (DWORD dwYear, fp fTimeInYear, fp fTimeInDay, fp fLatitude, PCPoint pSun);
DLLEXPORT fp MoonVector (DWORD dwYear, fp fTimeInYear, fp fTimeInDay, fp fLatitude, PCPoint pMoon);


/*******************************************************************************
Measure */
#define  INCHESPERMETER             39.37007874 // from MS Excel
#define  INCHESPERCENTIMETER        (INCHESPERMETER / 100.0)
#define  INCHESPERMILLIMETER        (INCHESPERMETER / 1000.0)
#define  INCHESPERFOOT              12
#define  METERSPERINCH              (1.0 / INCHESPERMETER)
#define  METERSPERFOOT              (METERSPERINCH * INCHESPERFOOT)
#define  FEETPERMETER               (1.0 / METERSPERFOOT)
#define  CENTIMETERSPERINCH         (1.0 / INCHESPERCENTIMETER)
#define  MILLIMETERSPERINCH         (1.0 / INCHESPERMILLIMETER)
#define  FEETPERMILE                5280.0
#define  METERSPERMILE              (METERSPERFOOT * FEETPERMILE)


#define  FLOORHEIGHT_TEMPERATE      2.4
#define  FLOORHEIGHT_TROPICAL       2.9

#define  MUNIT_METRIC               0x00
#define  MUNIT_METRIC_METERS        0x00
#define  MUNIT_METRIC_CENTIMETERS   0x01
#define  MUNIT_METRIC_MILLIMETERS   0x02
#define  MUNIT_METRIC_KM            0x03

#define  MUNIT_ENGLISH              0x10
#define  MUNIT_ENGLISH_FEET         0x10     // show as N'
#define  MUNIT_ENGLISH_INCHES       0x11     // show as M"
#define  MUNIT_ENGLISH_FEETINCHES   0x12     // show as N' M"
#define  MUNIT_ENGLISH_MILES        0x13     // show as 5.34 mi.

DLLEXPORT void MeasureDefaultUnitsSet (DWORD dwUnits);
DLLEXPORT DWORD MeasureDefaultUnits (void);
DLLEXPORT BOOL MeasureParseString (PCEscPage pPage, WCHAR *pszControl, fp *pf);
DLLEXPORT BOOL MeasureParseString (WCHAR *psz, fp *pf);
DLLEXPORT BOOL MeasureParseString (char *psz, fp *pf);
DLLEXPORT fp ApplyGrid (fp fValue, fp fGrid);
DLLEXPORT void MeasureToString (fp fValue, char *psz, BOOL fAccurate = FALSE);
DLLEXPORT void MeasureToString (PCEscPage pPage, WCHAR *pszControl, fp fValue, BOOL fAccurate = FALSE);
DLLEXPORT void MeasureToString (fp fValue, WCHAR *psz, BOOL fAccurate = FALSE);
DLLEXPORT fp MeasureFindScale (fp fScale);


/********************************************************************************
ResLeak */
DLLEXPORT void ResLeakEnd (void);
DLLEXPORT BOOL ResLeakDeleteObject (HGDIOBJ hObject, const char *pszFile, int iLine);
DLLEXPORT BOOL ResLeakDeleteDC(HDC hObject, const char *pszFile, int iLine);
DLLEXPORT int ResLeakReleaseDC(HWND hWnd, HDC hObject, const char *pszFile, int iLine);
DLLEXPORT BOOL ResLeakDestroyCursor(HCURSOR hObject, const char *pszFile, int iLine);
DLLEXPORT BOOL ResLeakDestroyIcon(HICON hObject, const char *pszFile, int iLine);
DLLEXPORT HDC ResLeakCreateCompatibleDC(HDC hdc, const char *pszFile, int iLine);
DLLEXPORT HDC ResLeakCreateDC(LPCTSTR lpszDriver,LPCTSTR lpszDevice, LPCTSTR lpszOutput, CONST DEVMODE *lpInitData,
   const char *pszFile, int iLine);
DLLEXPORT HDC ResLeakGetDC(HWND hWnd,
   const char *pszFile, int iLine);
DLLEXPORT HPEN ResLeakCreatePen(int fnPenStyle, int nWidth,COLORREF crColor,
   const char *pszFile, int iLine);
DLLEXPORT HPEN ResLeakCreatePenIndirect(CONST LOGPEN *lplgpn,
   const char *pszFile, int iLine);
DLLEXPORT HBRUSH ResLeakCreateBrushIndirect(CONST LOGBRUSH *lplb,
   const char *pszFile, int iLine);
DLLEXPORT HBRUSH ResLeakCreateSolidBrush(COLORREF crColor,
   const char *pszFile, int iLine);
DLLEXPORT HBITMAP ResLeakCreateBitmap(
  int nWidth,         // bitmap width, in pixels
  int nHeight,        // bitmap height, in pixels
  UINT cPlanes,       // number of color planes used by device
  UINT cBitsPerPel,   // number of bits required to identify a color
  CONST VOID *lpvBits, // pointer to array containing color data
   const char *pszFile, int iLine);
DLLEXPORT HBITMAP ResLeakCreateBitmapIndirect(
  CONST BITMAP *lpbm,    // pointer to the bitmap data
   const char *pszFile, int iLine);
DLLEXPORT HBITMAP ResLeakCreateCompatibleBitmap(
  HDC hdc,        // handle to device context
  int nWidth,     // width of bitmap, in pixels
  int nHeight,     // height of bitmap, in pixels
   const char *pszFile, int iLine);
DLLEXPORT HBITMAP ResLeakCreateDIBitmap(
  HDC hdc,                  // handle to device context
  CONST BITMAPINFOHEADER *lpbmih,  // pointer to bitmap size and
                                   // format data
  DWORD fdwInit,            // initialization flag
  CONST VOID *lpbInit,      // pointer to initialization data
  CONST BITMAPINFO *lpbmi,  // pointer to bitmap color-format data
  UINT fuUsage,              // color-data usage
   const char *pszFile, int iLine);
DLLEXPORT HBITMAP ResLeakCreateDIBSection(
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
DLLEXPORT HFONT ResLeakCreateFont(
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
DLLEXPORT HFONT  ResLeakCreateFontIndirect(
  CONST LOGFONT *lplf,   // pointer to logical font structure
   const char *pszFile, int iLine);
DLLEXPORT HRGN ResLeakCreateRectRgn(
  int nLeftRect,   // x-coordinate of region's upper-left corner
  int nTopRect,    // y-coordinate of region's upper-left corner
  int nRightRect,  // x-coordinate of region's lower-right corner
  int nBottomRect,  // y-coordinate of region's lower-right corner
   const char *pszFile, int iLine);
DLLEXPORT HANDLE ResLeakLoadImage(
  HINSTANCE hinst,   // handle of the instance containing the image
  LPCTSTR lpszName,  // name or identifier of image
  UINT uType,        // type of image
  int cxDesired,     // desired width
  int cyDesired,     // desired height
  UINT fuLoad,        // load flags
   const char *pszFile, int iLine);


/*******************************************************************************
Util */
extern PWSTR gszBack;
extern PWSTR gszChecked;
extern PWSTR gszText;
extern PWSTR gszPos;
extern PWSTR gszCurSel;
extern PWSTR gpszASP;
extern PWSTR gszColor;
extern DWORD gdwToday;

DLLEXPORT PWSTR CatMinor (void);
DLLEXPORT PWSTR CatMajor (void);
DLLEXPORT PWSTR CatName (void);
DLLEXPORT PWSTR CatGUIDCode (void);
DLLEXPORT PWSTR CatGUIDSub (void);
DLLEXPORT PWSTR WSObjEmbed (void);
DLLEXPORT PWSTR WSObjLocation (void);
DLLEXPORT PWSTR WSObjShowPlain (void);
DLLEXPORT PWSTR WSObjType (void);
DLLEXPORT PWSTR Attrib (void);
DLLEXPORT PWSTR Pos (void);
DLLEXPORT PWSTR Text (void);
DLLEXPORT PWSTR Back (void);
DLLEXPORT PWSTR Checked (void);
DLLEXPORT PWSTR CurSel (void);
DLLEXPORT PWSTR ASPString (void);
DLLEXPORT PWSTR WSFoundation (void);
DLLEXPORT PWSTR WSClimate (void);
DLLEXPORT PWSTR WSTime (void);
DLLEXPORT PWSTR WSLatitude (void);
DLLEXPORT PWSTR WSTrueNorth (void);
DLLEXPORT PWSTR WSDate (void);
DLLEXPORT DWORD TodayFast (void);
DLLEXPORT PCListFixed ReturnXMONITORINFO (void);
DLLEXPORT PCWorldSocket WorldGet (DWORD dwRenderShard, PCSceneSet *ppScene /*= NULL*/);
DLLEXPORT COLORREF MapColorPicker (DWORD dwIndex);
DLLEXPORT void M3DLibraryDir (char *psz, DWORD dwLen);
DLLEXPORT void WorldSet (DWORD dwRenderShard, PCWorldSocket pWorld, PCSceneSet pScene);
DLLEXPORT void BeepWindowInit (void);
DLLEXPORT void BeepWindowEnd (void);
DLLEXPORT void BeepWindowBeep (DWORD dwBeep);
DLLEXPORT void DefaultBuildingSettings (DFDATE *pdwDate = NULL, DFTIME *pdwTime = NULL, fp *pfLatitude = NULL,
                              DWORD *pdwClimate = NULL, DWORD *pdwRoofing = NULL,
                              DWORD *pdwExternalWalls = NULL, DWORD *pdwFoundation = NULL);
DLLEXPORT BOOL M3DDynamicGet (void);
DLLEXPORT void M3DDynamicSet (BOOL fDynamic);
DLLEXPORT BOOL ComboBoxSet (PCEscPage pPage, PWSTR psz, DWORD dwVal);
DLLEXPORT BOOL ListBoxSet (PCEscPage pPage, PWSTR psz, DWORD dwVal);
DLLEXPORT BOOL IsCutoutEntireSurface (DWORD dwCutoutNum, PTEXTUREPOINT ptp);
DLLEXPORT DWORD FillXMONITORINFO (void);
DLLEXPORT PWSTR ObjectSurface (void);
DLLEXPORT PSTR RegBase (void);
DLLEXPORT PCWSTR RegBaseW (void);
DLLEXPORT PWSTR WSRoofing (void);
DLLEXPORT PWSTR WSExternalWalls (void);
DLLEXPORT PWSTR RedoSamePage (void);
DLLEXPORT int VistaThreadPriorityHack (int iPriority, int iIncrease = 0);
DLLEXPORT void VistaThreadPriorityHackSet (BOOL fHack);

DLLEXPORT void ToHLS (WORD r, WORD g, WORD b, WORD *ph, WORD *pl, WORD *ps);
DLLEXPORT void FromHLS (WORD h, WORD l, WORD s, WORD *pr, WORD *pg, WORD *pb);
DLLEXPORT COLORREF FromHLS256 (float fH, float fL, float fS);
DLLEXPORT void FromHLS256 (float fH, float fL, float fS,
            float* pfRed, float* pfGreen, float* pfBlue);
DLLEXPORT void ToHLS256 (float fRed, float fGreen, float fBlue,
            float* pfH, float* pfL, float* pfS);
DLLEXPORT float GammaFloat256 (float fColor);
DLLEXPORT void GammaFloat256 (float *pafColor);
DLLEXPORT float UnGammaFloat256 (float fColor);
DLLEXPORT void UnGammaFloat256 (float *pafColor);


DLLEXPORT DWORD GenerateAlphaInPerspective (fp x1, fp x2, fp w1, fp w2,
                                  int iStart, int iCount, PCMem pMem, BOOL fOneExtra = FALSE);

DLLEXPORT void KeySet (char *pszKey, DWORD dwValue);
DLLEXPORT DWORD KeyGet (char *pszKey, DWORD dwDefault);
DLLEXPORT void GetLastDirectory (char *psz, DWORD dwSize);
DLLEXPORT void GetLastDirectory (WCHAR *psz, DWORD dwSize);
DLLEXPORT void SetLastDirectory (char *psz);
DLLEXPORT void SetLastDirectory (WCHAR *psz);
DLLEXPORT fp MyRand (fp fMin, fp fMax);
DLLEXPORT void KeySet (char *pszKey, GUID *pgValue);
DLLEXPORT void KeyGet (char *pszKey, GUID *pgValue);

DLLEXPORT DFDATE Today (void);
DLLEXPORT DFTIME Now (void);
DLLEXPORT DFDATE MinutesToDFDATE (__int64 iMinutes);
DLLEXPORT __int64 DFDATEToMinutes (DFDATE date);
DLLEXPORT void DialogBoxLocation (HWND hWnd, RECT *pr);
DLLEXPORT void DialogBoxLocation2 (HWND hWnd, RECT *pr);
DLLEXPORT void DialogBoxLocation3 (HWND hWnd, RECT *pr, BOOL fLeft);
DLLEXPORT BOOL DefPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam);
DLLEXPORT BOOL FillStatusColor (PCEscPage pPage, PWSTR pszControl, COLORREF cr);
DLLEXPORT COLORREF AskColor (HWND hWnd, COLORREF cr,
                             PCEscPage pPage = NULL, PWSTR pszControl = NULL,
                             BOOL *pfCancel = NULL);

DLLEXPORT BOOL DateControlSet (PCEscPage pPage, PWSTR pszControl, DFDATE date);
DLLEXPORT DFDATE DateControlGet (PCEscPage pPage, PWSTR pszControl);
DLLEXPORT BOOL TimeControlSet (PCEscPage pPage, PWSTR pszControl, DFTIME time);
DLLEXPORT DFTIME TimeControlGet (PCEscPage pPage, PWSTR pszControl);
DLLEXPORT BOOL AngleToControl (PCEscPage pPage, PWSTR pszControl, fp fAngle, BOOL fNegative = FALSE);
DLLEXPORT fp AngleFromControl (PCEscPage pPage, PWSTR pszControl);
DLLEXPORT fp DoubleFromControl (PCEscPage pPage, PWSTR pszControl);
DLLEXPORT BOOL DoubleToControl (PCEscPage pPage, PWSTR pszControl, fp fDouble);
DLLEXPORT BOOL AngleToString (char *psz, fp fAngle, BOOL fNegative = FALSE);

DLLEXPORT void MemZero (PCMem pMem);
DLLEXPORT void MemCatSanitize (PCMem pMem, PWSTR psz);
DLLEXPORT void MemCat (PCMem pMem, int iNum);
DLLEXPORT void MemCat (PCMem pMem, PWSTR psz);


/************************************************************************************
VolumeSort */
DLLEXPORT PCListFixed ObjectsInVolume (PCWorldSocket pWorld, CPoint apVolume[2][2][2],
                             BOOL fMustBeWhollyIn, BOOL fZValues = FALSE,
                             DWORD *padwIgnore = NULL, DWORD dwIgnoreNum = 0);



/**************************************************************************************
MML */
DLLEXPORT BOOL MMLToMem (PCMMLNode2 pNode, PCMem pMem, BOOL fSkipTag = FALSE, DWORD dwIndent = 0, BOOL fIndent=FALSE);
DLLEXPORT PCMMLNode2 MMLFromMem (PCWSTR psz);
DLLEXPORT BOOL MMLValueSet (PCMMLNode2 pNode, PWSTR pszID, PWSTR pszValue);
DLLEXPORT BOOL MMLValueSet (PCMMLNode2 pNode, PWSTR pszID, PBYTE pb, size_t dwSize);
DLLEXPORT BOOL MMLValueSet (PCMMLNode2 pNode, PWSTR pszID, int iValue);
DLLEXPORT BOOL MMLValueSet (PCMMLNode2 pNode, PWSTR pszID, fp fValue);
DLLEXPORT BOOL MMLValueSet (PCMMLNode2 pNode, PWSTR pszID, PCPoint p);
DLLEXPORT BOOL MMLValueSet (PCMMLNode2 pNode, PWSTR pszID, PCMatrix p);
DLLEXPORT PWSTR MMLValueGet (PCMMLNode2 pNode, PWSTR pszID);
DLLEXPORT int MMLValueGetInt (PCMMLNode2 pNode, PWSTR pszID, int iDefault);
DLLEXPORT size_t MMLValueGetBinary (PCMMLNode2 pNode, PWSTR pszID, PBYTE pb, size_t dwSize);
DLLEXPORT size_t MMLValueGetBinary (PCMMLNode2 pNode, PWSTR pszID, PCMem pMem);
DLLEXPORT fp MMLValueGetDouble (PCMMLNode2 pNode, PWSTR pszID, fp fDefault);
DLLEXPORT BOOL MMLValueGetPoint (PCMMLNode2 pNode, PWSTR pszID, PCPoint p, PCPoint pDefault = NULL);
DLLEXPORT BOOL MMLValueGetMatrix (PCMMLNode2 pNode, PWSTR pszID, PCMatrix p, PCMatrix pDefault = NULL);
DLLEXPORT BOOL MMLValueSet (PCMMLNode2 pNode, PWSTR pszID, PTEXTUREPOINT p);
DLLEXPORT BOOL MMLValueGetTEXTUREPOINT (PCMMLNode2 pNode, PWSTR pszID, PTEXTUREPOINT p, PTEXTUREPOINT pDefault = NULL);

DLLEXPORT BOOL MMLToObjects (PCWorldSocket pWorld, PCMMLNode2 pNode, BOOL fSelect, BOOL fRename = TRUE, PCProgressSocket pProgress = NULL);
DLLEXPORT PCMMLNode2 MMLFromObjects (PCWorldSocket pWorld, DWORD *padwIndex, DWORD dwSize, PCProgressSocket pProgress = NULL);
DLLEXPORT PCMMLNode2 MMLFromObject (PCObjectSocket pObject);
DLLEXPORT PCObjectSocket MMLToObject (DWORD dwRenderShard, PCMMLNode2 pNode);



/*************************************************************************************
Textures */
#define  TEXTURETHUMBNAIL         100

class CTextureMap;
typedef CTextureMap *PCTextureMap;
DLLEXPORT void ObjPaintDialog (DWORD dwRenderShard, DWORD dwID, PCObjectSocket pos, PCHouseView pView, BOOL fNoUI);
DLLEXPORT BOOL TextureCacheRelease (DWORD dwRenderShard, PCTextureMapSocket pTexture);
DLLEXPORT void TextureCacheDelete (DWORD dwRenderShard, const GUID *pgCode, const GUID *pgSub, BOOL fReallyDelete = TRUE);
DLLEXPORT PCTextureMapSocket TextureCacheGet (DWORD dwRenderShard, PRENDERSURFACE pRS, QWORD *pqwTextureCacheTimeStamp, PCTextureMapSocket pTextureMapLast);
DLLEXPORT PCTextureMapSocket TextureCacheGetDynamic (DWORD dwRenderShard, GUID *pgCode, GUID *pgSub, QWORD *pqwTextureCacheTimeStamp, PCTextureMapSocket pTextureMapLast);
DLLEXPORT void TextureCacheAddDynamic (DWORD dwRenderShard, PCTextureMapSocket pAdd);
DLLEXPORT void TextureCacheClear (DWORD dwRenderShard, DWORD dwMax);
DLLEXPORT DWORD TextureCacheNum (DWORD dwRenderShard);
DLLEXPORT void TextureCacheEnd (DWORD dwRenderShard, PCProgressSocket pProgress);
DLLEXPORT void TextureCacheInit (DWORD dwRenderShard, PCProgressSocket pProgress);
DLLEXPORT PCTextureMapSocket TextureCreate (DWORD dwRenderShard, const GUID *pCode, const GUID *pSub);
DLLEXPORT BOOL TextureNameFromGUIDs (DWORD dwRenderShard, GUID *pgCode, GUID *pgSub,PWSTR pszMajor, PWSTR pszMinor, PWSTR pszName);
DLLEXPORT BOOL TextureGUIDsFromName (DWORD dwRenderShard, PWSTR pszMajor, PWSTR pszMinor, PWSTR pszName, GUID *pgCode, GUID *pgSub);
DLLEXPORT void TextureEnumItems (DWORD dwRenderShard, PCListFixed pl, PWSTR pszMajor, PWSTR pszMinor);
DLLEXPORT void TextureEnumMinor (DWORD dwRenderShard, PCListFixed pl, PWSTR pszMajor);
DLLEXPORT void TextureEnumMajor (DWORD dwRenderShard, PCListFixed pl);
DLLEXPORT BOOL TextureSelDialog (DWORD dwRenderShard, HWND hWnd, PCObjectSurface pSurf, PCWorldSocket pWorld);
DLLEXPORT void TextureRebuildTextureColorList (DWORD dwRenderShard);
DLLEXPORT void AttachDateTimeToMML (PCMMLNode2 pNode);
DLLEXPORT void GetDateAndTimeFromMML (PCMMLNode2 pNode, DFDATE *pDate, DFTIME *pTime);
DLLEXPORT HBITMAP TextureGetThumbnail (DWORD dwRenderShard, GUID *pgCode, GUID *pgSub, HWND hWnd, COLORREF *pcTransparent);
DLLEXPORT BOOL TextureDetailSet (DWORD dwRenderShard, int iDetail);
DLLEXPORT int TextureDetailGet (DWORD dwRenderShard);

typedef struct {
   float       fPixelLen;     // number of meters each pixel is in width and height
   float       fGlowScale;    // only used of have glow map, all glow amounts are scaled by this
      // BUGFIX - Was fp, but since saving to disk needed to make float
   BOOL        fFloor;        // set to TRUE if expected to go on the floor, affects how shading will be done
   DWORD       dwMap;         // Bitfield indicating what kind of texture maps there are.
                              // - Color texture map assumed
                              // 0x01 - Specularity map
                              // 0x02 - Bump map
                              // 0x04 - Transparency map
                              // 0x08 - Glow map
} TEXTINFO, *PTEXTINFO;

BOOL TextureAlgoUnCache (DWORD dwRenderShard, const GUID *pgCode, const GUID *pgSub,
                         DWORD *pdwX, DWORD *pdwY, PCMaterial pMat = NULL, PTEXTINFO pInfo = NULL,
                         PBYTE *ppbCombined = NULL, PBYTE *ppbColorOnly = NULL, PBYTE *ppbSpec = NULL,
                         PSHORT *ppsBump = NULL, PBYTE *ppbTrans = NULL, PBYTE *ppbGlow = NULL);
BOOL TextureAlgoCache (DWORD dwRenderShard, const GUID *pgCode, const GUID *pgSub, PCImage pImage, PCImage pImage2,
                       PCMaterial pMaterial, PTEXTINFO pti);
BOOL TextureAlgoTextureQuery (DWORD dwRenderShard, PCListFixed plText, PCBTree pTree, GUID *pagThis);
BOOL TextureAlgoSubTextureNoRecurse (DWORD dwRenderShard, PCListFixed plText, GUID *pagThis);

#define TMFP_TRANSPARENCY        0x0001            // want transparency information
#define TMFP_SPECULARITY         0x0002            // want specularity information
#define TMFP_OTHERMATINFO        0x0004            // want other material info
#define TMFP_ALL                 (DWORD)-1

// Used for ForceCache()
#define FORCECACHE_ALL           ((DWORD)-1)       // cache all the texture features
#define FORCECACHE_COMBINED      0x0001            // cache the combination render of color, bump used for non-shadows render
#define FORCECACHE_COLORONLY     0x0002            // cache the color-only data
#define FORCECACHE_GLOW          0x0004            // cache the glow
#define FORCECACHE_BUMP          0x0008            // cache bump map
#define FORCECACHE_TRANS         0x0010            // cache transparency
#define FORCECACHE_SPEC          0x0020            // cache specularity

// CTextureMapSocket - virtual class used to create other texture maps.
// Do this so can have replacable texture maps
class DLLEXPORT CTextureMapSocket {
public:
   virtual void TextureModsSet (PTEXTUREMODS pt) = 0;
   virtual void TextureModsGet (PTEXTUREMODS pt) = 0;
   virtual void GUIDsGet (GUID *pCode, GUID *pSub) = 0;
   virtual void GUIDsSet (const GUID *pCode, const GUID *pSub) = 0;
   virtual void DefScaleGet (fp *pfDefScaleH, fp *pfDefScaleV) = 0;
   virtual void MaterialGet (DWORD dwThread, PCMaterial pMat) = 0;
   virtual DWORD MightBeTransparent (DWORD dwThread) = 0;
   virtual DWORD DimensionalityQuery (DWORD dwThread) = 0;
   virtual COLORREF AverageColorGet (DWORD dwThread, BOOL fGlow) = 0;
   virtual void Delete (void) = 0;
   virtual void ForceCache (DWORD dwForceCache) = 0;

   virtual void FillPixel (DWORD dwThread, DWORD dwFlags, WORD *pawColor, const PTEXTPOINT5 pText, const PTEXTPOINT5 pMax,
      PCMaterial pMat, float *pafGlow, BOOL fHighQuality) = 0;
   virtual BOOL PixelBump (DWORD dwThread, const PTEXTPOINT5 pText, const PTEXTPOINT5 pRight,
                             const PTEXTPOINT5 pDown, const PTEXTUREPOINT pSlope, fp *pfHeight = NULL, BOOL fHighQuality = FALSE) = 0;
   virtual BOOL QueryTextureBlurring (DWORD dwThread) = 0;
   virtual BOOL QueryBump (DWORD dwThread) = 0;
   virtual void FillLine (DWORD dwThread, PGCOLOR pac, DWORD dwNum, const PTEXTPOINT5 pLeft, const PTEXTPOINT5 pRight,
      float *pafGlow, BOOL *pfGlow, WORD *pawTrans, BOOL *pfTrans, fp *pafAlpha /*= NULL*/) = 0;


   // specifically not an =0 function
   virtual void FillImageFlat (PCImage pImage, PTEXTPOINT5 pUL, PTEXTPOINT5 pUR,
      PTEXTPOINT5 pLL, PTEXTPOINT5 pLR);
   virtual void FillImage (PCImage pImage, BOOL fEncourageSphere, BOOL fStretchToFit,
                  fp afTextureMatrix[2][2], PCMatrix pmTextureMatrix,
                  PCMaterial pMaterial);

};

#define  NUMTMIMAGE        4     // number of image types, 0 = combined, 1 = color only, 2 = bump + transparency + 2xspecularity,
                                 // 3 = glow

#define  TI_COMBINED       0     // combined image (color + some lighting). Doesnt include glow
#define  TI_COLORONLY      1     // color only image
#define  TI_BUMP           2     // bump + transparency + 2xspecularity
#define  TI_GLOW           3     // self illumination

class DLLEXPORT CTextureMap : public CTextureMapSocket, public CEscMultiThreaded {
public:
   ESCNEWDELETE;

   CTextureMap (void);
   ~CTextureMap (void);

   // initialize from various sources
   BOOL Init (const GUID *pCode, const GUID *pSub,
      DWORD dwX, DWORD dwY, PCMaterial pMat, PTEXTINFO pTextInfo,
      PBYTE pabCombined, PBYTE pabColorOnly, BYTE *pabSpec, short *pasBump, PBYTE pabTrans,
      float *pafGlow);

   virtual void TextureModsSet (PTEXTUREMODS pt);
   virtual void TextureModsGet (PTEXTUREMODS pt);
   virtual void GUIDsGet (GUID *pCode, GUID *pSub);
   virtual void GUIDsSet (const GUID *pCode, const GUID *pSub);
   virtual void DefScaleGet (fp *pfDefScaleH, fp *pfDefScaleV);
   virtual void ForceCache (DWORD dwForceCache);

   virtual void FillPixel (DWORD dwThread, DWORD dwFlags, WORD *pawColor, const PTEXTPOINT5 pText, const PTEXTPOINT5 pMax,
      PCMaterial pMat, float *pafGlow, BOOL fHighQuality);
   virtual BOOL PixelBump (DWORD dwThread, const PTEXTPOINT5 pText, const PTEXTPOINT5 pRight,
                             const PTEXTPOINT5 pDown, const PTEXTUREPOINT pSlope, fp *pfHeight = NULL, BOOL fHighQuality = FALSE);
   virtual BOOL QueryTextureBlurring (DWORD dwThread);
   virtual BOOL QueryBump (DWORD dwThread);

   virtual void MaterialGet (DWORD dwThread, PCMaterial pMat);
   virtual DWORD MightBeTransparent (DWORD dwThread);
   virtual DWORD DimensionalityQuery (DWORD dwThread);

   void ProduceDownsamples (DWORD dwVer);  // produce downsamples if not already there
   virtual COLORREF AverageColorGet (DWORD dwThread, BOOL fGlow);
   virtual void Delete (void);

   void FillLine (DWORD dwThread, PGCOLOR pac, DWORD dwNum, const PTEXTPOINT5 pLeft, const PTEXTPOINT5 pRight,
      float *pafGlow, BOOL *pfGlow, WORD *pawTrans, BOOL *pfTrans, fp *pafAlpha /*= NULL*/);

   // CEscMultiThreaded
   virtual void EscMultiThreadedCallback (PVOID pParams, DWORD dwParamSize,
      DWORD dwThread);

   WORD  m_awColorAverage[NUMTMIMAGE][3]; // calculated when downsample. This is the average color of the texture
                           // map. Can be used for colors when drawing textured object but
                           // in non-textured rendering mode
   COLORREF m_acColorAverage[NUMTMIMAGE];  // BYTE version of 
   BOOL     m_fDontDownsample;   // if TRUE, then dont downsample, always run off existing image.
                                 // used for sky bitmap so doesn't end up producing smaller and smalelr versions

private:
   BOOL WantToProduceDownsamples (DWORD dwVer);
   PTMIMAGE TMImageAlloc (DWORD dwX, DWORD dwY, DWORD dwType);
   void TMImageFree (PTMIMAGE pImage);
   void TMImagePixelInterpTMCOLOR (PTMIMAGE pImage, fp fX, fp fY, PGCOLOR pc, BOOL fHighQuality);
   void TMImagePixelInterpTMTRANS (PTMIMAGE pImage, fp fX, fp fY, PGCOLOR pc, DWORD dwWant, BOOL fHighQuality);
   void TMImagePixelInterpTMGLOW (PTMIMAGE pImage, fp fX, fp fY, PTMGLOW pc, BOOL fHighQuality);
   __inline PTMCOLOR TMImagePixelTMCOLOR (PTMIMAGE pImage, int iX, int iY)
      {
         PTMCOLOR pc = (PTMCOLOR) (pImage + 1);

         // fix problems with mod of negative numbers
         if (iX < 0)
            iX += ((-iX) / (int) pImage->dwX + 1) * pImage->dwX;
         if (iY < 0)
            iY += ((-iY) / (int) pImage->dwY + 1) * pImage->dwY;

         pc = pc + ((iX % (int) pImage->dwX) + (iY % (int) pImage->dwY) * (int) pImage->dwX);
         return pc;
      };
   __inline PTMTRANS TMImagePixelTMTRANS (PTMIMAGE pImage, int iX, int iY)
      {
         PTMTRANS pc = (PTMTRANS) (pImage + 1);

         // fix problems with mod of negative numbers
         if (iX < 0)
            iX += ((-iX) / (int) pImage->dwX + 1) * pImage->dwX;
         if (iY < 0)
            iY += ((-iY) / (int) pImage->dwY + 1) * pImage->dwY;

         pc = pc + ((iX % (int) pImage->dwX) + (iY % (int) pImage->dwY) * (int) pImage->dwX);
         return pc;
      };
   __inline PTMGLOW TMImagePixelTMGLOW (PTMIMAGE pImage, int iX, int iY)
      {
         PTMGLOW pc = (PTMGLOW) (pImage + 1);

         // fix problems with mod of negative numbers
         if (iX < 0)
            iX += ((-iX) / (int) pImage->dwX + 1) * pImage->dwX;
         if (iY < 0)
            iY += ((-iY) / (int) pImage->dwY + 1) * pImage->dwY;

         pc = pc + ((iX % (int) pImage->dwX) + (iY % (int) pImage->dwY) * (int) pImage->dwX);
         return pc;
      };

   void FreeAllImages (BOOL fLeaveOrig);
   void FilteredPixelFromOrig (DWORD dwVer, DWORD dwTakeFrom, int iX, int iY, DWORD dwWidth, PTMCOLOR pc);
   void ProduceColorized(DWORD dwVer);     // produce colorized image if not already there

   PTMIMAGE       m_apImageOrig[NUMTMIMAGE];  // original image, see sef of NUMTIMIMAGE
   PTMIMAGE       m_apImageColorized[NUMTMIMAGE]; // colorized image, see def of NUMMIMAGE
   CListFixed     m_alistImageDown[NUMTMIMAGE];  // downsampled images, see def of NUMTMIMAGE
   GUID           m_gCode;          // code ID
   GUID           m_gSub;           // sub-code ID
   fp             m_fDefH, m_fDefV; // default width in H and V
   TEXTUREMODS    m_TextureMods;    // texture modifications to apply
   CMem           m_amemPosn[MAXRAYTHREAD];        // temporary
   CMaterial      m_Material;       // default materials
   TEXTINFO       m_TI;             // texture information
   DWORD          m_dwX;            // width
   DWORD          m_dwY;            // height
   DWORD          m_dwForceCache;   // set to bits for what forcecache has been called
};

DLLEXPORT void ApplyTextureMods (PTEXTUREMODS pMods, float *pafColor);
DLLEXPORT void ApplyTextureMods (PTEXTUREMODS pMods, WORD *pawColor);

DEFINE_GUID(GTEXTURECODE_VolNoise, 
0xacf2ae4, 0x18a1, 0x4919, 0xa9, 0x6c, 0xbb, 0xfb, 0xcf, 0x2a, 0xd0, 0x3e);
DEFINE_GUID(GTEXTURECODE_VolWood, 
0x1ff2ac1, 0xc9a1, 0x2a19, 0xa9, 0x6c, 0xbb, 0xfb, 0xcf, 0x2a, 0xd0, 0x3e);
DEFINE_GUID(GTEXTURECODE_VolMarble, 
0xac1434c1, 0x21c9, 0x2a19, 0xa9, 0x6c, 0xc1, 0xfb, 0xcf, 0x2a, 0xd0, 0x3e);
DEFINE_GUID(GTEXTURECODE_VolSandstone, 
0xacf21e4, 0x18a1, 0x4f29, 0xa9, 0x6c, 0x1a, 0xfb, 0xa4, 0x2a, 0x2b, 0x3e);

DEFINE_GUID(GTEXTURECODE_Waterfall,
0xbfa41866, 0xa33d, 0x4e95, 0x98, 0xcb, 0x65, 0x2e, 0x6, 0x1d, 0xef, 0x1b);
DEFINE_GUID(GTEXTURESUB_Waterfall,
0x81a, 0x8821, 0x504d, 0x2c, 0xf0, 0xec, 0x4, 0xd6, 0x2c, 0xc5, 0x1);

// {34DE5E7E-0248-4a0f-B0B4-1D7F4925FDEC}
// BUGFIX - Update when change format for algo data
DEFINE_GUID(GUID_AlgoFileHeader, 
0x3cde5e7e, 0x248, 0x4aa8, 0xb9, 0xb4, 0x1d, 0x7f, 0x49, 0x25, 0x14, 0xec);

// {01523AE4-08A1-4619-A9DB-BB2BCF10D03E}
DEFINE_GUID(GTEXTURECODE_Algorithmic, 
0x1523ae4, 0x8a1, 0x4619, 0xa9, 0xdb, 0xbb, 0x2b, 0xcf, 0x10, 0xd0, 0x3e);

// {1A4B3F62-C781-417e-AA70-D2BC7E161E96}
DEFINE_GUID(GTEXTURECODE_Parquet, 
0x1a4b3f62, 0xc781, 0x417e, 0xaa, 0x70, 0xd2, 0xbc, 0x7e, 0x16, 0x1e, 0x96);

// {9146DE9B-7450-4a5f-94EF-0EA1D5910FB5}
DEFINE_GUID(GTEXTURECODE_StonesStacked, 
0x9146de9b, 0x7450, 0x4a5f, 0x94, 0xef, 0xe, 0xa1, 0xd5, 0x91, 0xf, 0xb5);
DEFINE_GUID(GTEXTURECODE_StonesRandom, 
0x3146de9b, 0x7450, 0x4a5f, 0x98, 0xef, 0xe1, 0xa1, 0xd5, 0x91, 0xf, 0xb5);
DEFINE_GUID(GTEXTURECODE_Pavers, 
0x6146de9a, 0x7450, 0x4a5f, 0x98, 0x1f, 0xe1, 0xa1, 0xc5, 0x91, 0xf, 0xb5);

// {8892E71E-8497-483a-9897-60E4B9E17489}
DEFINE_GUID(GTEXTURECODE_Bitmap, 
0x8892e71e, 0x8497, 0x483a, 0x98, 0x97, 0x60, 0xe4, 0xb9, 0xe1, 0x74, 0x89);
DEFINE_GUID(GTEXTURECODE_JPEG, 
0x8892e71e, 0x8497, 0x4832, 0x98, 0x97, 0x65, 0xe4, 0xb9, 0xe1, 0x74, 0x89);
DEFINE_GUID(GTEXTURECODE_ImageFile, 
0x8892a71e, 0x8491, 0x483a, 0x98, 0x97, 0x60, 0xe4, 0xb9, 0xe1, 0x74, 0x89);


// {92367EC5-6FEE-465f-BFAA-F8C02C3178F3}
DEFINE_GUID(GTEXTURECODE_Tiles, 
0x92367ec5, 0x6fee, 0x465f, 0xbf, 0xaa, 0xf8, 0xc0, 0x2c, 0x31, 0x78, 0xf3);

// {BFA41866-A33D-4e95-98CB-652E061DEF1B}
DEFINE_GUID(GTEXTURECODE_TextureAlgoNoise, 
0xbfa41866, 0xa33d, 0x4e95, 0x98, 0xcb, 0x65, 0x2e, 0x6, 0x1d, 0xef, 0x1b);

// {E5919B6D-8853-4678-9162-8289339A269B}
DEFINE_GUID(GTEXTURECODE_Marble, 
0xe5919b6d, 0x8853, 0x4678, 0x91, 0x62, 0x82, 0x89, 0x33, 0x9a, 0x26, 0x9b);


// {1CEE2D37-7C2D-4e22-9EB4-55DF1AA02686}
DEFINE_GUID(GTEXTURECODE_WoodPlanks, 
0x1cee2d37, 0x7c2d, 0x4e22, 0x9e, 0xb4, 0x55, 0xdf, 0x1a, 0xa0, 0x26, 0x86);

// {F7443042-718E-46f1-8198-3257D165229E}
DEFINE_GUID(GTEXTURECODE_Shingles, 
0xf7443042, 0x718e, 0x46f1, 0x81, 0x98, 0x32, 0x57, 0xd1, 0x65, 0x22, 0x9e);

// {054B8075-2A9A-4364-AAA8-EBE3442BD3FD}
DEFINE_GUID(GTEXTURECODE_Clapboards, 
0x54b8075, 0x2a9a, 0x4364, 0xaa, 0xa8, 0xeb, 0xe3, 0x44, 0x2b, 0xd3, 0xfd);

// {0858FBCE-E059-4da2-81D3-89E1252139B8}
DEFINE_GUID(GTEXTURECODE_Fabric, 
0x858fbce, 0xe059, 0x4da2, 0x81, 0xd3, 0x89, 0xe1, 0x25, 0x21, 0x39, 0xb8);

DEFINE_GUID(GTEXTURECODE_GrassTussock, 
0x358fb9e, 0x2059, 0x4da2, 0x84, 0xd3, 0x89, 0x41, 0x25, 0x21, 0x99, 0xa2);

DEFINE_GUID(GTEXTURECODE_Faceomatic, 
0x4c8fb9e, 0x2059, 0x4da2, 0x19, 0xd3, 0x89, 0x41, 0x38, 0x1a, 0x99, 0xf4);

DEFINE_GUID(GTEXTURECODE_Mix, 
0x4adfb91, 0x2929, 0x4d64, 0x19, 0xd3, 0x38, 0x41, 0x38, 0x21, 0x99, 0xfb);

DEFINE_GUID(GTEXTURECODE_LeafLitter, 
0xcadfb91, 0x2129, 0x4d64, 0x99, 0xd3, 0x48, 0x41, 0x36, 0x21, 0x97, 0xf2);

DEFINE_GUID(GTEXTURECODE_Text, 
0xc4dfb91, 0xa929, 0x4d64, 0x19, 0xd3, 0x38, 0xc6, 0x38, 0x3b, 0x99, 0x2c);

DEFINE_GUID(GTEXTURECODE_TreeBark, 
0x1cff92e, 0x2a49, 0x4da2, 0x19, 0x16, 0x89, 0x41, 0xb2, 0x1a, 0x99, 0xf4);

DEFINE_GUID(GTEXTURECODE_Branch, 
0x553fbfe, 0x1059, 0x4da2, 0x84, 0xd9, 0x82, 0x41, 0x25, 0x81, 0x99, 0xa3);

DEFINE_GUID(GTEXTURECODE_Iris, 
0x1c9abce, 0xa459, 0x60a2, 0x1b, 0xd3, 0x89, 0xe1, 0x25, 0x21, 0x39, 0xb8);

DEFINE_GUID(GTEXTURESUB_IrisEye,
0xc14, 0x2485, 0x2, 0xfc, 0x62, 0x5f, 0x55, 0x6b, 0xb1, 0xc3, 0x1);

DEFINE_GUID(GTEXTURECODE_BloodVessels, 
0xa59abce, 0x1859, 0x60a2, 0x1b, 0xd3, 0x89, 0xe1, 0x25, 0x21, 0x39, 0x2c);

DEFINE_GUID(GTEXTURESUB_BloodVesselsEye,
0xc11, 0x2485, 0x2, 0xfc, 0x62, 0x5f, 0x55, 0x6b, 0xb1, 0xc3, 0x1);

DEFINE_GUID(GTEXTURECODE_Hair, 
0x1f2cabce, 0xa443, 0x162a, 0x1b, 0xd3, 0x15, 0xe1, 0x25, 0x21, 0x39, 0x2c);

DEFINE_GUID(GTEXTURESUB_HairDefault,
0x815, 0x1a76, 0x40, 0x66, 0x54, 0xb4, 0x2c, 0xef, 0xb9, 0xc3, 0x1);

// {874FB072-B389-4b8a-A25D-06B0D28D8757}
DEFINE_GUID(GTEXTURECODE_Lattice, 
0x874fb072, 0xb389, 0x4b8a, 0xa2, 0x5d, 0x6, 0xb0, 0xd2, 0x8d, 0x87, 0x57);

// {24EE09D8-0694-4a95-B8FE-98E306B60204}
DEFINE_GUID(GTEXTURECODE_Wicker, 
0x24ee09d8, 0x694, 0x4a95, 0xb8, 0xfe, 0x98, 0xe3, 0x6, 0xb6, 0x2, 0x4);

DEFINE_GUID(GTEXTURECODE_Chainmail, 
0x54ef19d8, 0xa494, 0x4245, 0x18, 0xfe, 0x98, 0x9c, 0x6, 0xb6, 0x2, 0x4);

DEFINE_GUID(GTEXTURECODE_TestPattern, 
0x4a9c09d8, 0x1494, 0x4625, 0xb8, 0xa1, 0x98, 0xe3, 0x6, 0xb6, 0x2, 0x4);

// {65E8E044-B55F-4818-BACF-EEE463552804}
DEFINE_GUID(GTEXTURECODE_BoardBatten, 
0x65e8e044, 0xb55f, 0x4818, 0xba, 0xcf, 0xee, 0xe4, 0x63, 0x55, 0x28, 0x4);

// {9302423E-E333-4375-B80D-ECB8F40DD047}
DEFINE_GUID(GTEXTURECODE_Corrogated, 
0x9302423e, 0xe333, 0x4375, 0xb8, 0xd, 0xec, 0xb8, 0xf4, 0xd, 0xd0, 0x47);

DEFINE_GUID(GTEXTURECODE_HearthBricks,
0x92367ec5, 0x6fee, 0x465f, 0xbf, 0xaa, 0xf8, 0xc0, 0x2c, 0x31, 0x78, 0xf3);
DEFINE_GUID(GTEXTURESUB_HearthBricks,
0xf1e, 0x6fe7, 0x545, 0x60, 0xdb, 0xea, 0xac, 0x68, 0x1e, 0xc2, 0x1);

// used textures
DEFINE_GUID(GTEXTURECODE_SidingStucco,
0xbfa41866, 0xa33d, 0x4e95, 0x98, 0xcb, 0x65, 0x2e, 0x6, 0x1d, 0xef, 0x1b);
DEFINE_GUID(GTEXTURESUB_SidingStucco,
0x32b, 0xabb, 0x485, 0x40, 0x7b, 0xbb, 0x94, 0x98, 0x1f, 0xc2, 0x1);

DEFINE_GUID(GTEXTURECODE_SidingClapboards,
0x54b8075, 0x2a9a, 0x4364, 0xaa, 0xa8, 0xeb, 0xe3, 0x44, 0x2b, 0xd3, 0xfd);
DEFINE_GUID(GTEXTURESUB_SidingClapboards,
0x113d, 0xc198, 0x26c, 0xc0, 0x71, 0x89, 0xc0, 0x46, 0x1f, 0xc2, 0x1);

DEFINE_GUID(GTEXTURECODE_SidingBricks,
0x92367ec5, 0x6fee, 0x465f, 0xbf, 0xaa, 0xf8, 0xc0, 0x2c, 0x31, 0x78, 0xf3);
DEFINE_GUID(GTEXTURESUB_SidingBricks,
0xc91548f0, 0xf20, 0x4290, 0x96, 0xca, 0xd9, 0x37, 0xad, 0x84, 0x46, 0xb9);

DEFINE_GUID(GTEXTURECODE_SidingCorrugated,
0x9302423e, 0xe333, 0x4375, 0xb8, 0xd, 0xec, 0xb8, 0xf4, 0xd, 0xd0, 0x47);
DEFINE_GUID(GTEXTURESUB_SidingCorrugated,
0xf6d4f824, 0x3ded, 0x4002, 0x8d, 0x4b, 0xfa, 0x79, 0x23, 0x50, 0x15, 0x8f);

DEFINE_GUID(GTEXTURECODE_SidingStone,
0x9146de9b, 0x7450, 0x4a5f, 0x94, 0xef, 0xe, 0xa1, 0xd5, 0x91, 0xf, 0xb5);
DEFINE_GUID(GTEXTURESUB_SidingStone,
0x1232, 0xaf2b, 0x299, 0x60, 0xb8, 0x4d, 0x9b, 0x4d, 0x1f, 0xc2, 0x1);

DEFINE_GUID(GTEXTURECODE_SidingBlock,
0x92367ec5, 0x6fee, 0x465f, 0xbf, 0xaa, 0xf8, 0xc0, 0x2c, 0x31, 0x78, 0xf3);
DEFINE_GUID(GTEXTURESUB_SidingBlock,
0xc91548f2, 0xf20, 0x4290, 0x96, 0xca, 0xd9, 0x37, 0xad, 0x84, 0x46, 0xb9);

DEFINE_GUID(GTEXTURECODE_SidingLogs,
0x54b8075, 0x2a9a, 0x4364, 0xaa, 0xa8, 0xeb, 0xe3, 0x44, 0x2b, 0xd3, 0xfd);
DEFINE_GUID(GTEXTURESUB_SidingLogs,
0x120c, 0xd61f, 0x279, 0xc0, 0x93, 0x6b, 0xbf, 0x48, 0x1f, 0xc2, 0x1);

DEFINE_GUID(GTEXTURECODE_RoofingCorrugated,
0x9302423e, 0xe333, 0x4375, 0xb8, 0xd, 0xec, 0xb8, 0xf4, 0xd, 0xd0, 0x47);
DEFINE_GUID(GTEXTURESUB_RoofingCorrugated,
0x32ad470b, 0x3022, 0x46ae, 0xba, 0xd1, 0xb5, 0x37, 0x79, 0x7e, 0x87, 0x72);

DEFINE_GUID(GTEXTURECODE_FlooringTiles,
0x92367ec5, 0x6fee, 0x465f, 0xbf, 0xaa, 0xf8, 0xc0, 0x2c, 0x31, 0x78, 0xf3);
DEFINE_GUID(GTEXTURESUB_FlooringTiles,
0xd73558b6, 0x7bab, 0x479c, 0xa4, 0xa, 0x2c, 0x9c, 0xcd, 0x2e, 0x2c, 0xec);

DEFINE_GUID(GTEXTURECODE_FlooringDecking,
0x1cee2d37, 0x7c2d, 0x4e22, 0x9e, 0xb4, 0x55, 0xdf, 0x1a, 0xa0, 0x26, 0x86);
DEFINE_GUID(GTEXTURESUB_FlooringDecking,
0xcab80752, 0xde3d, 0x4090, 0x80, 0x98, 0x15, 0xc7, 0xa1, 0x56, 0xef, 0xd);

DEFINE_GUID(GTEXTURECODE_FlooringHardwood,
0x1cee2d37, 0x7c2d, 0x4e22, 0x9e, 0xb4, 0x55, 0xdf, 0x1a, 0xa0, 0x26, 0x86);
DEFINE_GUID(GTEXTURESUB_FlooringHardwood,
0xcab80750, 0xde3d, 0x4090, 0x80, 0x98, 0x15, 0xc7, 0xa1, 0x56, 0xef, 0xd);

DEFINE_GUID(GTEXTURECODE_WoodTrim,
0x1cee2d37, 0x7c2d, 0x4e22, 0x9e, 0xb4, 0x55, 0xdf, 0x1a, 0xa0, 0x26, 0x86);
DEFINE_GUID(GTEXTURESUB_WoodTrim,
0x733, 0x923, 0x568, 0xe0, 0xc6, 0xe7, 0x36, 0xbb, 0x1f, 0xc2, 0x1);

// BUGFIX - Using new default grass, based on hair instead of oise
//DEFINE_GUID(GTEXTURECODE_Grass,
//0xbfa41866, 0xa33d, 0x4e95, 0x98, 0xcb, 0x65, 0x2e, 0x6, 0x1d, 0xef, 0x1b);
//DEFINE_GUID(GTEXTURESUB_Grass,
//0x209f0f83, 0xd434, 0x4846, 0x95, 0x22, 0x3b, 0x92, 0x88, 0x64, 0x97, 0x7d);
DEFINE_GUID(GTEXTURECODE_Grass,
0x1f2cabce, 0xa443, 0x162a, 0x1b, 0xd3, 0x15, 0xe1, 0x25, 0x21, 0x39, 0x2c);
DEFINE_GUID(GTEXTURESUB_Grass,
0x811, 0xd0f3, 0xb344, 0x36, 0x41, 0xbd, 0xcd, 0x49, 0x28, 0xc6, 0x1);

DEFINE_GUID(GTEXTURECODE_Skirting,
0x874fb072, 0xb389, 0x4b8a, 0xa2, 0x5d, 0x6, 0xb0, 0xd2, 0x8d, 0x87, 0x57);
DEFINE_GUID(GTEXTURESUB_Skirting,
0xf1e, 0xc9f4, 0x709, 0x60, 0xa9, 0x88, 0xf3, 0xfa, 0x1f, 0xc2, 0x1);

DEFINE_GUID(GTEXTURECODE_RoofingWoodShingles,
0xf7443042, 0x718e, 0x46f1, 0x81, 0x98, 0x32, 0x57, 0xd1, 0x65, 0x22, 0x9e);
DEFINE_GUID(GTEXTURESUB_RoofingWoodShingles,
0xd3a, 0xa92b, 0x18f, 0x40, 0xbd, 0xb4, 0x4, 0x25, 0x1f, 0xc2, 0x1);

DEFINE_GUID(GTEXTURECODE_RoofingTiles,
0xf7443042, 0x718e, 0x46f1, 0x81, 0x98, 0x32, 0x57, 0xd1, 0x65, 0x22, 0x9e);
DEFINE_GUID(GTEXTURESUB_RoofingTiles,
0xe13, 0x4249, 0x1a0, 0xc0, 0x6e, 0xfd, 0x8c, 0x27, 0x1f, 0xc2, 0x1);

DEFINE_GUID(GTEXTURECODE_RoofingAsphalt,
0xf7443042, 0x718e, 0x46f1, 0x81, 0x98, 0x32, 0x57, 0xd1, 0x65, 0x22, 0x9e);
DEFINE_GUID(GTEXTURESUB_RoofingAsphalt,
0xe0e, 0x4249, 0x1a0, 0xc0, 0x6e, 0xfd, 0x8c, 0x27, 0x1f, 0xc2, 0x1);

DEFINE_GUID(GTEXTURECODE_WindowShutter,
0x54b8075, 0x2a9a, 0x4364, 0xaa, 0xa8, 0xeb, 0xe3, 0x44, 0x2b, 0xd3, 0xfd);
DEFINE_GUID(GTEXTURESUB_WindowShutter,
0xf3e, 0x81ca, 0x723, 0xa0, 0xd3, 0x0, 0xe0, 0xfe, 0x1f, 0xc2, 0x1);

DEFINE_GUID(GTEXTURECODE_ShallowWater,
0xbfa41866, 0xa33d, 0x4e95, 0x98, 0xcb, 0x65, 0x2e, 0x6, 0x1d, 0xef, 0x1b);
DEFINE_GUID(GTEXTURESUB_ShallowWater,
0xc0a, 0x1e1e, 0x652, 0x20, 0x23, 0x5d, 0xed, 0xde, 0x1f, 0xc2, 0x1);

DEFINE_GUID(GTEXTURECODE_PoolWater,
0xbfa41866, 0xa33d, 0x4e95, 0x98, 0xcb, 0x65, 0x2e, 0x6, 0x1d, 0xef, 0x1b);
DEFINE_GUID(GTEXTURESUB_PoolWater,
0xc09, 0x1e1e, 0x652, 0x20, 0x23, 0x5d, 0xed, 0xde, 0x1f, 0xc2, 0x1);

DEFINE_GUID(GTEXTURECODE_LakeWater,
0xbfa41866, 0xa33d, 0x4e95, 0x98, 0xcb, 0x65, 0x2e, 0x6, 0x1d, 0xef, 0x1b);
DEFINE_GUID(GTEXTURESUB_LakeWater,
0xc08, 0x1e1e, 0x652, 0x20, 0x23, 0x5d, 0xed, 0xde, 0x1f, 0xc2, 0x1);

DEFINE_GUID(GTEXTURECODE_LightShade,
0x858fbce, 0xe059, 0x4da2, 0x81, 0xd3, 0x89, 0xe1, 0x25, 0x21, 0x39, 0xb8);
DEFINE_GUID(GTEXTURESUB_LightShade,
0x1111, 0xfc96, 0x76d, 0x80, 0x23, 0xeb, 0x3c, 0xa, 0x20, 0xc2, 0x1);

#if 0
// {AC4D2D8E-AADF-4f8a-9B65-3AD23A3135B1}
DEFINE_GUID(GTEXTURESUB_Checkerboard, 
0xac4d2d8e, 0xaadf, 0x4f8a, 0x9b, 0x65, 0x3a, 0xd2, 0x3a, 0x31, 0x35, 0xb1);

// {D73558B6-7BAB-479c-A40A-2C9CCD2E2CEC}
DEFINE_GUID(GTEXTURESUB_TilesWhite, 
0xd73558b6, 0x7bab, 0x479c, 0xa4, 0xa, 0x2c, 0x9c, 0xcd, 0x2e, 0x2c, 0xec);
DEFINE_GUID(GTEXTURESUB_TilesWhiteSmooth, 
0xd73558c0, 0x7bab, 0x479c, 0xa4, 0xa, 0x2c, 0x9c, 0xcd, 0x2e, 0x2c, 0xec);
DEFINE_GUID(GTEXTURESUB_TilesRed, 
0xd73558c1, 0x7bab, 0x479c, 0xa4, 0xa, 0x2c, 0x9c, 0xcd, 0x2e, 0x2c, 0xec);
DEFINE_GUID(GTEXTURESUB_TilesGreen, 
0xd73558c2, 0x7bab, 0x479c, 0xa4, 0xa, 0x2c, 0x9c, 0xcd, 0x2e, 0x2c, 0xec);
DEFINE_GUID(GTEXTURESUB_TilesBlue, 
0xd73558c3, 0x7bab, 0x479c, 0xa4, 0xa, 0x2c, 0x9c, 0xcd, 0x2e, 0x2c, 0xec);
DEFINE_GUID(GTEXTURESUB_TilesCheckerboard, 
0xd73558c4, 0x7bab, 0x479c, 0xa4, 0xa, 0x2c, 0x9c, 0xcd, 0x2e, 0x2c, 0xec);
DEFINE_GUID(GTEXTURESUB_TilesCheckerboardCyan, 
0xd73558c5, 0x7bab, 0x479c, 0xa4, 0xa, 0x2c, 0x9c, 0xcd, 0x2e, 0x2c, 0xec);
DEFINE_GUID(GTEXTURESUB_TilesSlate, 
0xd73558c6, 0x7bab, 0x479c, 0xa4, 0xa, 0x2c, 0x9c, 0xcd, 0x2e, 0x2c, 0xec);
DEFINE_GUID(GTEXTURESUB_TilesTeracotta, 
0xd73558c7, 0x7bab, 0x479c, 0xa4, 0xa, 0x2c, 0x9c, 0xcd, 0x2e, 0x2c, 0xec);


// {C91548FF-0F20-4290-96CA-D937AD8446B9}
DEFINE_GUID(GTEXTURESUB_Brick, 
0xc91548f0, 0xf20, 0x4290, 0x96, 0xca, 0xd9, 0x37, 0xad, 0x84, 0x46, 0xb9);
DEFINE_GUID(GTEXTURESUB_BrickGrey, 
0xc91548f1, 0xf20, 0x4290, 0x96, 0xca, 0xd9, 0x37, 0xad, 0x84, 0x46, 0xb9);
DEFINE_GUID(GTEXTURESUB_CementBlock, 
0xc91548f2, 0xf20, 0x4290, 0x96, 0xca, 0xd9, 0x37, 0xad, 0x84, 0x46, 0xb9);
DEFINE_GUID(GTEXTURESUB_BrickPainted, 
0xc91548f3, 0xf20, 0x4290, 0x96, 0xca, 0xd9, 0x37, 0xad, 0x84, 0x46, 0xb9);
DEFINE_GUID(GTEXTURESUB_CutStone, 
0xc91548f4, 0xf20, 0x4290, 0x96, 0xca, 0xd9, 0x37, 0xad, 0x84, 0x46, 0xb9);

// {CAB80759-DE3D-4090-8098-15C7A156EF0D}
DEFINE_GUID(GTEXTURESUB_HardwoodDark, 
0xcab80750, 0xde3d, 0x4090, 0x80, 0x98, 0x15, 0xc7, 0xa1, 0x56, 0xef, 0xd);
DEFINE_GUID(GTEXTURESUB_HardwoodLight, 
0xcab80751, 0xde3d, 0x4090, 0x80, 0x98, 0x15, 0xc7, 0xa1, 0x56, 0xef, 0xd);
DEFINE_GUID(GTEXTURESUB_Decking, 
0xcab80752, 0xde3d, 0x4090, 0x80, 0x98, 0x15, 0xc7, 0xa1, 0x56, 0xef, 0xd);
DEFINE_GUID(GTEXTURESUB_Plywood, 
0xcab80753, 0xde3d, 0x4090, 0x80, 0x98, 0x15, 0xc7, 0xa1, 0x56, 0xef, 0xd);
DEFINE_GUID(GTEXTURESUB_PlanksPainted, 
0xcab80754, 0xde3d, 0x4090, 0x80, 0x98, 0x15, 0xc7, 0xa1, 0x56, 0xef, 0xd);

// {209F0F82-D434-4846-9522-3B928864977D}
DEFINE_GUID(GTEXTURESUB_Stucco, 
0x209f0f80, 0xd434, 0x4846, 0x95, 0x22, 0x3b, 0x92, 0x88, 0x64, 0x97, 0x7d);
DEFINE_GUID(GTEXTURESUB_RenderedCement, 
0x209f0f81, 0xd434, 0x4846, 0x95, 0x22, 0x3b, 0x92, 0x88, 0x64, 0x97, 0x7d);
DEFINE_GUID(GTEXTURESUB_SpeckledCeiling, 
0x209f0f82, 0xd434, 0x4846, 0x95, 0x22, 0x3b, 0x92, 0x88, 0x64, 0x97, 0x7d);
DEFINE_GUID(GTEXTURESUB_Grass, 
0x209f0f83, 0xd434, 0x4846, 0x95, 0x22, 0x3b, 0x92, 0x88, 0x64, 0x97, 0x7d);
DEFINE_GUID(GTEXTURESUB_CarpetBeige, 
0x209f0f84, 0xd434, 0x4846, 0x95, 0x22, 0x3b, 0x92, 0x88, 0x64, 0x97, 0x7d);



// {32AD470B-3022-46ae-BAD1-B537797E8772}
DEFINE_GUID(GTEXTURESUB_CustomOrbZinc, 
0x32ad470b, 0x3022, 0x46ae, 0xba, 0xd1, 0xb5, 0x37, 0x79, 0x7e, 0x87, 0x72);
// {F6D4F822-3DED-4002-8D4B-FA792350158F}
DEFINE_GUID(GTEXTURESUB_CustomOrbRed, 
0xf6d4f822, 0x3ded, 0x4002, 0x8d, 0x4b, 0xfa, 0x79, 0x23, 0x50, 0x15, 0x8f);
DEFINE_GUID(GTEXTURESUB_CustomOrbGreen, 
0xf6d4f823, 0x3ded, 0x4002, 0x8d, 0x4b, 0xfa, 0x79, 0x23, 0x50, 0x15, 0x8f);
DEFINE_GUID(GTEXTURESUB_CustomOrbBlue, 
0xf6d4f824, 0x3ded, 0x4002, 0x8d, 0x4b, 0xfa, 0x79, 0x23, 0x50, 0x15, 0x8f);
DEFINE_GUID(GTEXTURESUB_CustomOrbCreme, 
0xf6d4f825, 0x3ded, 0x4002, 0x8d, 0x4b, 0xfa, 0x79, 0x23, 0x50, 0x15, 0x8f);
DEFINE_GUID(GTEXTURESUB_CustomOrbCyan, 
0xf6d4f826, 0x3ded, 0x4002, 0x8d, 0x4b, 0xfa, 0x79, 0x23, 0x50, 0x15, 0x8f);
DEFINE_GUID(GTEXTURESUB_CustomOrbBlack, 
0xf6d4f827, 0x3ded, 0x4002, 0x8d, 0x4b, 0xfa, 0x79, 0x23, 0x50, 0x15, 0x8f);
DEFINE_GUID(GTEXTURESUB_CustomOrbWhite, 
0xf6d4f828, 0x3ded, 0x4002, 0x8d, 0x4b, 0xfa, 0x79, 0x23, 0x50, 0x15, 0x8f);
DEFINE_GUID(GTEXTURESUB_CustomOrbGrey, 
0xf6d4f829, 0x3ded, 0x4002, 0x8d, 0x4b, 0xfa, 0x79, 0x23, 0x50, 0x15, 0x8f);
// {F6D4F822-3DED-4002-8D4B-FA792350158F}
DEFINE_GUID(GTEXTURESUB_SquareOrbZinc, 
0xf7d4f821, 0x3ded, 0x4002, 0x8d, 0x4b, 0xfa, 0x79, 0x23, 0x50, 0x15, 0x8f);
DEFINE_GUID(GTEXTURESUB_SquareOrbRed, 
0xf7d4f822, 0x3ded, 0x4002, 0x8d, 0x4b, 0xfa, 0x79, 0x23, 0x50, 0x15, 0x8f);
DEFINE_GUID(GTEXTURESUB_SquareOrbGreen, 
0xf7d4f823, 0x3ded, 0x4002, 0x8d, 0x4b, 0xfa, 0x79, 0x23, 0x50, 0x15, 0x8f);
DEFINE_GUID(GTEXTURESUB_SquareOrbBlue, 
0xf7d4f824, 0x3ded, 0x4002, 0x8d, 0x4b, 0xfa, 0x79, 0x23, 0x50, 0x15, 0x8f);
DEFINE_GUID(GTEXTURESUB_SquareOrbCreme, 
0xf7d4f825, 0x3ded, 0x4002, 0x8d, 0x4b, 0xfa, 0x79, 0x23, 0x50, 0x15, 0x8f);
DEFINE_GUID(GTEXTURESUB_SquareOrbCyan, 
0xf7d4f826, 0x3ded, 0x4002, 0x8d, 0x4b, 0xfa, 0x79, 0x23, 0x50, 0x15, 0x8f);
DEFINE_GUID(GTEXTURESUB_SquareOrbBlack, 
0xf7d4f827, 0x3ded, 0x4002, 0x8d, 0x4b, 0xfa, 0x79, 0x23, 0x50, 0x15, 0x8f);
DEFINE_GUID(GTEXTURESUB_SquareOrbWhite, 
0xf7d4f828, 0x3ded, 0x4002, 0x8d, 0x4b, 0xfa, 0x79, 0x23, 0x50, 0x15, 0x8f);
DEFINE_GUID(GTEXTURESUB_SquareOrbGrey, 
0xf7d4f829, 0x3ded, 0x4002, 0x8d, 0x4b, 0xfa, 0x79, 0x23, 0x50, 0x15, 0x8f);

// {FD8E10DA-484B-4223-A542-E083B7C4C150}
DEFINE_GUID(GTEXTURESUB_WoodPlanks, 
0xfd8e10da, 0x484b, 0x4223, 0xa5, 0x42, 0xe0, 0x83, 0xb7, 0xc4, 0xc1, 0x50);

// {32916B47-6234-4659-BAD8-4B7594604F8F}
DEFINE_GUID(GTEXTURESUB_Printer, 
0x32916b47, 0x6234, 0x4659, 0xba, 0xd8, 0x4b, 0x75, 0x94, 0x60, 0x4f, 0x8f);

// {9108D3F6-C962-4765-955A-783EC9FDA85D}
DEFINE_GUID(GTEXTURESUB_Help, 
0x9108d3f6, 0xc962, 0x4765, 0x95, 0x5a, 0x78, 0x3e, 0xc9, 0xfd, 0xa8, 0x5d);

// {E1512B21-7BE4-475d-806C-7A29A53D3EB4}
DEFINE_GUID(GTEXTURESUB_DialogBox, 
0xe1512b21, 0x7be4, 0x475d, 0x80, 0x6c, 0x7a, 0x29, 0xa5, 0x3d, 0x3e, 0xb4);

// {F485C309-0DC7-4180-AA60-03746D281517}
DEFINE_GUID(GTEXTURESUB_New, 
0xf485c309, 0xdc7, 0x4180, 0xaa, 0x60, 0x3, 0x74, 0x6d, 0x28, 0x15, 0x17);


// {B2947DE8-4205-4351-8647-9AEA1A43F21B}
DEFINE_GUID(GTEXTURESUB_PhotoLetchworth, 
0xb2947de8, 0x4205, 0x4351, 0x86, 0x47, 0x9a, 0xea, 0x1a, 0x43, 0xf2, 0x1b);
DEFINE_GUID(GTEXTURESUB_PaintingMitch, 
0xb2947de9, 0x4205, 0x4351, 0x86, 0x47, 0x9a, 0xea, 0x1a, 0x43, 0xf2, 0x1b);
#endif 0

/************************************************************************
AlgoText */

//CTextCreatorSocket - Super class. Texture creators should all be sub-classed from
//this and support the virtual functions.

class DLLEXPORT CTextCreatorSocket {
public:
   virtual void Delete (void) = 0;
      // tell the object to delete itself

   virtual BOOL MMLFrom (PCMMLNode2 pNode) = 0;
      // object reads in information about itself from MML.

   virtual PCMMLNode2 MMLTo (void) = 0;
      // object writes information about itself to MML

   virtual BOOL Render (PCImage pImage, PCImage pImage2, PCMaterial pMaterial, PTEXTINFO pTextInfo) = 0;
      // tells object to render itself. It will initialize pImage to the appropriate
      // size. The image is then filled in with RGB for the color (not affected by
      // shading), iZ is the depth (0x10000 == 1 pixel in height), LOWORD(dwID) / 100 is
      // the power component for specularity, HIWORD(dwID) is brightness (0 to 0xffff) of
      // specularity. dwIDPart has the following set.... LOBYTE(dwIDPart) is the transparency
      // of the surface at the point.

      // if pTextInfo.dwMap & 0x08 then pImage2's RGB contains the amount of glow.
      // other wise it's ignored

      // Also pMaterial is filled in with default material information for the surface
      // (although if pImage has changing values for the particular component this
      // is overridden), and pTextInfo is filled in with misc info.

      // If the createor socket doesn't support this then returns FALSE.

   virtual BOOL FillImageWithTexture (PCImage pImage, BOOL fEncourageSphere, BOOL fStretchToFit);
      // Sometimes overridden, this fills the image (using the image's already-set size)
      // to the texutre. Used to display the texture while editing
      // If fEncourageSphere is TRUE then tends to draw object as sphere more often
      // If fStretchToFit then stretch image to fit entire area

   virtual BOOL Dialog (PCEscWindow pWindow) = 0;
      // tells the object to display a page allowing the user to edit the information.
      // Should return TRUE if the user pressed Back, FALSE if they closed the window

   virtual BOOL TextureQuery (PCListFixed plText, PCBTree pTree, GUID *pagThis);
      // asks the object what textures it uses. This allows the save-function
      // to save custom textures into the file. The object just ADDS (doesn't
      // clear or remove) elements, which are two guids in a row: the
      // gCode followed by the gSub of the object. Of course, it may add more
      // than one texture. NOTE: If the texture if already on the list it wont add itself
      // pTree is 2-guids as string, and used for checking existence. It's also added to.

   virtual BOOL SubTextureNoRecurse (PCListFixed plText, GUID *pagThis);
      // called to verify that there's no recursion. If the texture finds itself
      // on the list already, if fails. Otherwise, it temporarily adds itself
      // to the list and calls into sub-textures, which do the same.
      // plText is a list of 2 guids per element, gCode and gSub. pagThis likewise
      // points to 2 guids
};
typedef CTextCreatorSocket *PCTextCreatorSocket;

#define TIFCACHE     6     // number of caches that can have for images

class CTextCreatorImageFile : public CTextCreatorSocket {
public:
   CTextCreatorImageFile (DWORD dwRenderShard, DWORD dwType);
   virtual void Delete (void);
   virtual BOOL MMLFrom (PCMMLNode2 pNode);
   virtual PCMMLNode2 MMLTo (void);
   virtual BOOL Render (PCImage pImage, PCImage pImage2, PCMaterial pMaterial, PTEXTINFO pTextInfo);
   virtual BOOL Dialog (PCEscWindow pWindow);

   BOOL LoadImageFromDisk (void);
   BOOL CacheToImage (PCImage pImage, PCMem pCache);
   BOOL ImageToCache (PCMem pCache, PCImage pImage, DWORD dwType);
   BOOL CacheToFile (PWSTR pszFile, PCMem pCache);
   BOOL FileToCache (PCMem pCache, PWSTR pszFile);

   DWORD             m_dwRenderShard;
   CMaterial        m_Material;  // material to use
   BOOL             m_fCached;   // if TRUE, cache the image
   CMem             m_amemCache[TIFCACHE];  // cached image (See XXXToCache), or m_dwCurPosn == 0 => not there
   fp               m_fWidth;    // width of image in meters
   DWORD            m_dwX;       // width in pixels
   DWORD            m_dwY;       // height in pixels

   COLORREF          m_cDefColor;   // default color if no file
   WCHAR             m_szFile[256];  // name of the file
   WCHAR             m_szTransFile[256];  // transparency - name of the file
   WCHAR             m_szLucenFile[256];  // translucency - name of the file
   WCHAR             m_szGlossFile[256];  // glossiness - name of the file
   WCHAR             m_szBumpFile[256];  // bump-map - name of the file
   WCHAR             m_szGlowFile[256];  // glow - name of the file
   DWORD             m_dwTransRGB;  // transparency channel
   DWORD             m_dwLucenRGB;  // translucency channel
   DWORD             m_dwGlossRGB;  // gloss channel
   DWORD             m_dwBumpRGB;  // bump-map channel
   fp                m_fBumpHeight; // bump-map height

   COLORREF         m_cTransColor;  // transparent color
   DWORD            m_dwTransDist;  // acceptable transparent distance
   BOOL             m_fTransUse;    // set to TRUE if use transparency
};
typedef CTextCreatorImageFile *PCTextCreatorImageFile;


PCTextCreatorSocket CreateTextureCreator (DWORD dwRenderShard, const GUID *pgMajor, DWORD dwType, PCMMLNode2 pNode,
                                          BOOL fCanCreateVol);

/********************************************************************************
VolText */
class DLLEXPORT CTextureVolSocket : public CTextureMapSocket, public CTextCreatorSocket {
public:
   ESCNEWDELETE;

   CTextureVolSocket (void);
   ~CTextureVolSocket (void);

   // ALWAYS OVERRIDDENT - SPECIFIC TO THIS
   virtual BOOL Init (DWORD dwRenderShard, const GUID *pCode, const GUID *pSub, PCMMLNode2 pNode);
      // dwRenderShard is the render shard this will belong to

   // ALWAYS OVERRIDDEN - from CTextureMapSocket
   virtual void Delete (void) = 0;
   virtual void FillPixel (DWORD dwThread, DWORD dwFlags, WORD *pawColor, const PTEXTPOINT5 pText, const PTEXTPOINT5 pMax,
      PCMaterial pMat, float *pafGlow, BOOL fHighQuality) = 0;

   // ALWAYS OVERRIDDEN - from CTextCreatorSocket
   virtual BOOL MMLFrom (PCMMLNode2 pNode) = 0;
   virtual PCMMLNode2 MMLTo (void) = 0;
   virtual BOOL Dialog (PCEscWindow pWindow) = 0;

   // SOMETIMES OVERRIDDEN - from CTextureMapSocket
   virtual void ForceCache (DWORD dwForceCache);
   virtual DWORD MightBeTransparent (DWORD dwThread);
   virtual DWORD DimensionalityQuery (DWORD dwThread);
   virtual BOOL BumpAtLoc (PTEXTPOINT5 pText, fp *pfHeight);
   virtual BOOL QueryBump (DWORD dwThread);

   // NOT OVERRIDDEN - from CTextureMapSocket
   virtual BOOL QueryTextureBlurring (DWORD dwThread);
   virtual BOOL Render (PCImage pImage, PCImage pImage2, PCMaterial pMaterial, PTEXTINFO pTextInfo);
   virtual BOOL FillImageWithTexture (PCImage pImage, BOOL fEncourageSphere, BOOL fStretchToFit);
   virtual void TextureModsSet (PTEXTUREMODS pt);
   virtual void TextureModsGet (PTEXTUREMODS pt);
   virtual void GUIDsGet (GUID *pCode, GUID *pSub);
   virtual void GUIDsSet (const GUID *pCode, const GUID *pSub);
   virtual void DefScaleGet (fp *pfDefScaleH, fp *pfDefScaleV);
   virtual BOOL PixelBump (DWORD dwThread, const PTEXTPOINT5 pText, const PTEXTPOINT5 pRight,
                             const PTEXTPOINT5 pDown, const PTEXTUREPOINT pSlope, fp *pfHeight = NULL, BOOL fHighQuality = FALSE);
   virtual void MaterialGet (DWORD dwThread, PCMaterial pMat);
   virtual COLORREF AverageColorGet (DWORD dwThread, BOOL fGlow);
   virtual void FillLine (DWORD dwThread, PGCOLOR pac, DWORD dwNum, const PTEXTPOINT5 pLeft, const PTEXTPOINT5 pRight,
      float *pafGlow, BOOL *pfGlow, WORD *pawTrans, BOOL *pfTrans, fp *pafAlpha /*= NULL*/);

// private:
   // filled in by init
   GUID           m_gCode;          // code ID
   GUID           m_gSub;           // sub-code ID
   fp             m_fDefH, m_fDefV; // default width in H and V
   TEXTUREMODS    m_TextureMods;    // texture modifications to apply
   CMaterial      m_Material;       // default material

   // calculated by AverageColorGet - and also cleared by init
   COLORREF       m_cColorAverage;  // average color, use -1 if not known
   COLORREF       m_cColorAverageGlow; // average glow color, use -1 if not known
   BOOL           m_fMightBeTransparent;  // set to TRUE if texture might have transparency
   DWORD          m_dwRenderShard;  // render shard

private:
   // scrathc
   CMem           m_amemPosn[MAXRAYTHREAD];        // just used for fill line
};
typedef CTextureVolSocket *PCTextureVolSocket;

DLLEXPORT PCTextureMapSocket TextureCreateVol (DWORD dwRenderShard, const GUID *pCode, const GUID *pSub);
DLLEXPORT PCTextureVolSocket TextureCreateVol (const GUID *pCode);

/********************************************************************************
CTextureImage */

class DLLEXPORT CTextureImage {
public:
   ESCNEWDELETE;

   CTextureImage (void);
   ~CTextureImage (void);

   BOOL FromTexture (DWORD dwRenderShard, const GUID *pgCode, const GUID *pgSub);
   BOOL ToTexture (DWORD dwRenderShard);
   CTextureImage *Clone (void);

   // member variables
   GUID        m_gCode;          // ID for the texture
   GUID        m_gSub;           // ID for the texture
   fp          m_fGlowScale;     // amount to scale glow
   fp          m_fPixelLen;      // width (or height) of a single pixel, in meters
   fp          m_fBumpHeight;    // how many meters the bump spans
   CMaterial   m_Material;       // material for object
   COLORREF    m_cDefColor;      // default color if no file
   DWORD       m_dwWidth;        // width. Do not modify
   DWORD       m_dwHeight;       // height. Do not modify
   COLORREF    m_cTransColor;    // transparent color
   DWORD       m_dwTransDist;    // acceptable transparent distance
   BOOL        m_fTransUse;      // set to TRUE if use transparency
   BOOL        m_fCached;        // if TRUE, cache the image

   // pointers to memory. Do not modify pointers, memory can be modified
   PBYTE       m_pbRGB;          // pointer to m_memRGB.p, NULL if no RGB info
   PBYTE       m_pbSpec;         // pointer to m_memSpec.p. NULL if no spec info
   PBYTE       m_pbGlow;         // pointer to m_memGlow.p. NULL if no RGB info
   PBYTE       m_pbBump;         // pointer to m_memBump.p. NULL if no bump info
   PBYTE       m_pbTrans;        // pointer to m_memTrans.p. NULL if no transparency info

//private:
   // storage
   CMem        m_memRGB;         // colors, wxhx3 bytes
   CMem        m_memSpec;        // specularity, wxhx2 bytes. byte0=power,byte1=intenisty
   CMem        m_memGlow;        // glow colors, wxhx3 bytes
   CMem        m_memBump;        // bump, wxgx1 byte
   CMem        m_memTrans;       // transparency, wxhx1 byte
};
typedef CTextureImage *PCTextureImage;


/*************************************************************************************
ObjectCF */
#define  OBJECTTHUMBNAIL         100

DLLEXPORT void ObjectCFEnd (void);
DLLEXPORT void ObjectCFInit (void);
DLLEXPORT BOOL ObjectCFNameFromGUIDs (DWORD dwRenderShard, GUID *pgCode, GUID *pgSub,PWSTR pszMajor, PWSTR pszMinor, PWSTR pszName);
DLLEXPORT BOOL ObjectCFGUIDsFromName (DWORD dwRenderShard, PWSTR pszMajor, PWSTR pszMinor, PWSTR pszName, GUID *pgCode, GUID *pgSub);
DLLEXPORT DWORD ObjectCFIndexFromName (PWSTR pszMajor, PWSTR pszMinor, PWSTR pszName);
DLLEXPORT BOOL ObjectCFNameFromIndex (DWORD dwIndex, GUID *pgCode, GUID *pgSub,PWSTR *ppszMajor, PWSTR *ppszMinor, PWSTR *ppszName);
DLLEXPORT void ObjectCFEnumItems (DWORD dwRenderShard, PCListFixed pl, PWSTR pszMajor, PWSTR pszMinor);
DLLEXPORT void ObjectCFEnumMinor (DWORD dwRenderShard, PCListFixed pl, PWSTR pszMajor);
DLLEXPORT void ObjectCFEnumMajor (DWORD dwRenderShard, PCListFixed pl);
DLLEXPORT PCObjectSocket ObjectCFCreate (DWORD dwRenderShard, const GUID *pCode, const GUID *pSub);
DLLEXPORT BOOL ObjectCFNewDialog (DWORD dwRenderShard, HWND hWnd, GUID *pgCode, GUID *pgSub);
DLLEXPORT void HackObjectsCreate (void);
DLLEXPORT BOOL ObjectLibraryDialog (DWORD dwRenderShard, HWND hWnd);
DLLEXPORT BOOL ObjectEditorSave (PVIEWOBJ pvo);
DLLEXPORT PCMMLNode2 ObjectCacheUserObjects (PCWorldSocket pWorld);
DLLEXPORT BOOL ObjectUnCacheUserObjects (DWORD dwRenderShard, PCMMLNode2 pNode, HWND hWnd, CLibrary *pLibrary);
DLLEXPORT HBITMAP Thumbnail (DWORD dwRenderShard, GUID *pgMajor, GUID *pgMinor, HWND hWnd, COLORREF *pcrBackground);
BOOL ObjectEditorShowWindow (DWORD dwRenderShard, GUID *pgCode, GUID *pgSub);
DLLEXPORT BOOL ObjectCreateThumbnail (GUID *pgMajor, GUID *pgMinor, PCRenderTraditional pRender,
                            PCWorldSocket pWorld, PCImage pOrig,
                            GUID *pgEffectCode = NULL, GUID *pgEffectSub = NULL);

/************************************************************************************
CObjectStructSurface */
DLLEXPORT DWORD OverlayShapeNameToColor (PWSTR pszName);
DWORD OverlayShapeNameToID (PWSTR pszName);
BOOL OverlayShapeFromPoints (DWORD dwID, PTEXTUREPOINT ptp, DWORD dwNum,
                             PCPoint pCenter, fp *pfWidth, fp *pfHeight);
PCListFixed OverlayShapeToPoints (DWORD dwID, PCPoint pCent, fp fWidth,
                                  fp fHeight);
PCSplineSurface OverlayShapeFindByColor (CObjectStructSurface *pObj, DWORD dwColor, PWSTR *ppszName, PTEXTUREPOINT *pptp,
                                         DWORD *pdwNum, BOOL *pfClockwise);


/**************************************************************************************
CObjectBuildBlock */
extern PWSTR gpszDisplayControl;


/*************************************************************************************
CDoorFrame */

typedef struct {
   int         iBase;         // 0 for no base frame, 1 for base frame, 2 for must have bottom frame
   PCSpline    pSpline;       // spline fot he inside of the opening. Automatically generated
} OPENINGINFO, *POPENINGINFO;

// rectagnles
#define DFS_RECTANGLE            0     // simple rectangle
#define DFS_RECTANGLEVENT        1     // rectangle with a vent above
#define DFS_RECTANGLELRLITES     2     // rectangle with openings on left/right
#define DFS_RECTANGLELITEVENT    3     // rentagnge with one lite and a vent above
#define DFS_RECTANGLELRLITEVENT  4     // rangle with LR lites and a vent above
#define DFS_REPEATNUMTOP         5     // repeating doors/windows with a fixed number and top
#define DFS_REPEATNUM            6     // repeating doors/windows, but no top
#define DFS_REPEATMIN            7     // repeating doors/windows, with minimum division
#define DFS_REPEATMINTOP         8     // repeating doors/windows, with min division and top

// archres
#define DFS_ARCH                 100   // curved top, open inside
#define DFS_ARCHSPLIT            101   // curved top, split at the curve
#define DFS_ARCHLRLITES          102   // arch with left/right lites
#define DFS_ARCHLRLITES2         103   // arch goes over both the lites
#define DFS_ARCHLRLITES3         104   // arch with curved lites
#define DFS_ARCHHALF             105   // half an arch
#define DFS_ARCHHALF2            106   // quarter of a sphere
#define DFS_ARCH2                107   // half circle
#define DFS_CIRCLE               108   // circle
#define DFS_ARCHCIRCLE           109   // arch which is mostly circles
#define DFS_ARCHPEAK             110   // arch with a peak
#define DFS_ARCHPARTCIRCLE       111   // arch with part circle on top

// unusual
#define DFS_RECTANGLEPEAK        200   // rectangle, but has a peak on left.
#define DFS_TRAPEZOID1           201   // trapezoid with two lites on left and right
#define DFS_TRAPEZOID2           202   // trapezoid, but only one lite on left
#define DFS_ARCHTRIANGLE         203   // arch that's a triangle on top
#define DFS_TRIANGLERIGHT        204   // right triangle
#define DFS_TRIANGLEEQUI         205   // equilateral triangle
#define DFS_PENTAGON             206   // pentagon
#define DFS_HEXAGON              207   // hexagon
#define DFS_OCTAGON              208   // octagon

#define DFD_WINDOW               0     // used in m_dwDoor
#define DFD_DOOR                 1     // used in m_dwDoor
#define DFD_CABINET              2     // used in m_dwDoor

class DLLEXPORT CDoorFrame {
public:
   ESCNEWDELETE;

   CDoorFrame (void);
   ~CDoorFrame (void);

   PCMMLNode2 MMLTo (void);
   BOOL MMLFrom (PCMMLNode2 pNode);
   BOOL CloneTo (CDoorFrame *pNew);
   CDoorFrame *Clone (void);
   BOOL Init (DWORD dwShape, DWORD dwDoor, DWORD dwSetStyle);
   BOOL FrameSizeSet (fp fWidth, fp fDepth);
   BOOL FrameSizeGet (fp *pfWidth, fp *pfDepth);
   BOOL SubFrameSizeSet (fp fWidth, fp fDepth);
   BOOL SubFrameSizeGet (fp *pfWidth, fp *pfDepth);
   DWORD OpeningsNum (void);
   int OpeningsBaseGet (DWORD dwOpening);
   BOOL OpeningsBaseSet (DWORD dwOpening, int iBase);
   PCSpline OpeningsSpline (DWORD dwOpening);
   DWORD MeasNum (void);
   PCWSTR MeasGet (DWORD dwNum, fp *pfVal, BOOL *pfDistance);
   BOOL MeasSet (DWORD dwNum, fp fVal);
   void Render (POBJECTRENDER pr, PCRenderSurface prs, DWORD dwSide);
   PCSpline OutsideGet (void);
   int DialogShape (PCEscWindow pWindow);

//private:
   void CalcIfNecessary (void);
   void CheckOpenings (void);
   BOOL SplineInit (fp fBottom, PCSpline pSpline, BOOL fLooped, DWORD dwPoints,
                             PCPoint paPoints, DWORD *padwSegCurve, DWORD dwDivide,
                             fp fExpand = 0);
   BOOL SplineInit (fp fBottom, PCSpline pSpline,
                             fp ixl, fp izt, fp ixr, fp izb, BOOL fBase = TRUE);
   BOOL NoodleLinear (fp fBottom, BOOL fSubFrame,
                               fp fx1, fp fz1, fp fx2, fp fz2);
   void SplineMassage (fp fBottom, BOOL fLooped, DWORD dwPoints,
                             PCPoint paPoints, DWORD *padwSegCurve, DWORD dwDivide);
   BOOL NoodlePath (fp fBottom, BOOL fSubFrame, BOOL fLooped, DWORD dwPoints,
                             PCPoint paPoints, DWORD *padwSegCurve, DWORD dwDivide,
                             fp fExpand = 0);
   BOOL NoodlePath (fp fBottom, BOOL fSubFrame,
                             fp ixl, fp izt, fp ixr, fp izb, BOOL fBase = TRUE);

   DWORD       m_dwShape;     // shaoe of the frame, from DFS_XXXX, 0x10000 added to it mirrors it
   BOOL        m_dwDoor;      // One of DFD_XXX
   BOOL        m_fDirty;      // if TRUE its dirty and stuff needs to be recalculated
   CPoint      m_pFrameSize;  // p[0] is the width, p[1] is the depth.
   CPoint      m_pSubFrameSize;  // [0] is witdth, p[1] is depth
   CListFixed  m_lOPENINGINFO;   // list of info about the openings
   CListFixed  m_lMeasurement;   // list of doubles that are measurements specific to the shape

   // calculated from information at hand
   CListFixed  m_alNoodles[2];    // list of noodles used to draw the frame, [0] are front ones, [1] are back
   CSpline     m_lOutside;    // outside spline, going around clockwise
   // m_lOPENINGINFO.pSpline is automatically generated
};
typedef CDoorFrame *PCDoorFrame;

DLLEXPORT BOOL GenerateThreeDFromDoorFrame (PWSTR pszControl, PCEscPage pPage, PCDoorFrame pDoor);

/**********************************************************************************
CDoor */
#define  DS_CUSTOM            0     // custom style
#define  DS_DOORSOLID         1     // solid door
#define  DS_DOORSCREEN        2     // screen door
#define  DS_DOORGLASS1        3
#define  DS_DOORGLASS2        4
#define  DS_DOORGLASS3        5
#define  DS_DOORGLASS4        6
#define  DS_DOORGLASS5        7
#define  DS_DOORGLASS6        8
#define  DS_DOORGLASS7        9
#define  DS_DOORGLASS8        10
#define  DS_DOORGLASS9        11
#define  DS_DOORGLASS10       12
#define  DS_DOORGLASS11       13
#define  DS_DOORGLASS12       14
#define  DS_DOORGLASS13       15
#define  DS_DOORPANEL1        21
#define  DS_DOORPANEL2        22
#define  DS_DOORPANEL3        23
#define  DS_DOORPANEL4        24
#define  DS_DOORPANEL5        25
#define  DS_DOORPANEL6        26
#define  DS_DOORPANEL7        27
#define  DS_DOORLOUVER1       31
#define  DS_DOORLOUVER2       32
#define  DS_DOORLOUVER3       33
#define  DS_DOORPG1           41
#define  DS_DOORPG2           42
#define  DS_DOORPG3           43
#define  DS_DOORPG4           44
#define  DS_DOORPG5           45
#define  DS_DOORBRACE1        51
#define  DS_DOORBRACE2        52
#define  DS_DOORGARAGE1       61
#define  DS_DOORGARAGE2       62
#define  DS_DOORGARAGE3       63
#define  DS_DOORGATE1         71
#define  DS_WINDOWPLAIN1      101
#define  DS_WINDOWPLAIN2      102
#define  DS_WINDOWPLAIN3      103
#define  DS_WINDOWPLAIN4      104
#define  DS_WINDOWLITE1       111
#define  DS_WINDOWLITE2       112
#define  DS_WINDOWLITE3       113
#define  DS_WINDOWLITE4       114
#define  DS_WINDOWLITE5       115
#define  DS_WINDOWLITE6       116
#define  DS_WINDOWLITE7       117
#define  DS_WINDOWLITE8       118
#define  DS_WINDOWSHUTTER1    121
#define  DS_WINDOWSHUTTER2    122
#define  DS_WINDOWSCREEN      131
#define  DS_WINDOWLOUVER1     141
#define  DS_WINDOWLOUVER2     142
#define  DS_WINDOWBARS1       151
#define  DS_WINDOWBARS2       152
#define  DS_WINDOWBARS3       153
#define  DS_WINDOWBARS4       154
#define  DS_WINDOWBARS5       155

#define  DS_CABINETSOLID      200
#define  DS_CABINETPANEL      201
#define  DS_CABINETLITES      202
#define  DS_CABINETGLASS      203
#define  DS_CABINETLOUVER     204

#define  DHS_NONE          0     // no door handle

#define  DSURF_GLASS       0x0001   // bitfield indicating what kind of surface
#define  DSURF_EXTFRAME    0x0002   // exterior framing color
#define  DSURF_INTFRAME    0x0004   // interior framing color
#define  DSURF_EXTPANEL    0x0008   // exterior panel
#define  DSURF_INTPANEL    0x0010   // interior panel
#define  DSURF_SHUTTER     0x0020   // shutter color
#define  DSURF_FLYSCREEN   0x0040   // flyscreen
#define  DSURF_LOUVERS     0x0080   // small louvers done with texture map
#define  DSURF_DOORKNOB    0x0100   // doorknob
#define  DSURF_BRACING     0x0200   // bracing color
#define  DSURF_FROSTED     0x0400   // frosted glass
#define  DSURF_ALFRAME     0x0800   // aluminum frame
#define  DSURF_COLORSMAX   0x0fff   // max colors

#define DKS_NONE           0        // no knob
#define DKS_DOORKNOB       1        // basic doorknob that twist
#define DKS_DOORKNOBLOCK   2        // doorknob with deadbolt lock above it
#define DKS_LEVER          3        // doorknob that has a lever
#define DKS_LEVERLOCK      4        // lever with a deadbolt lock above it
#define DKS_POCKET         5        // small plate with handle used in internal pocket doors
#define DKS_SLIDER         6        // small handle used with sliding doors
#define DKS_DOORPULL1      11       // old-style door handle
#define DKS_DOORPULL2      12       // door handle attached to large plate - industrial
#define DKS_DOORPULL3      13       // piece of bent flat metal that's door handle
#define DKS_DOORPULL4      14       // bar bent in square config to pull
#define DKS_DOORPULL5      15       // bar bent in loop to pull
#define DKS_DOORPULL6      16       // long horizontal across door
#define DKS_DOORPULL7      17       // long handle up and down door
#define DKS_DOORPULL8      18       // diagonal across door
#define DKS_DOORPUSH1      21       // plate opposite hinged side of door that piush on
#define DKS_DOORPUSH2      22       // horizontal push-bar across door
#define DKS_CABINET1       30       // small D handle 1
#define DKS_CABINET2       31       // larger D handle
#define DKS_CABINET3       32       // small F hangle
#define DKS_CABINET4       33       // larger F handle
#define DKS_CABINET5       34       // knob
#define DKS_CABINET6       35       // loop that put fingers in
#define DKS_CABINET7       36       // classic handle

typedef struct {
   DWORD          adwStyle[2];   // style of the door knob, DKS_XXX, 0 for no door knob, [0] is front (y==-1), [1] is back
   CPoint         pKnob;         // point where the knob goes. only X and Z matter
   CPoint         pOpposite;      // opposite side of the door as the knob - used for knobs that run across
   BOOL           afKick[2];     // if TRUE kickplate, [0] for front, [1] for back
   fp             fKickIndent;   // indent kickplace on left, right, and bottom
   fp             fKickHeight;   // height of kick plate
   fp             fKnobRotate;   // how much to rotate the knob
} DOORKNOB, *PDOORKNOB;

class CDoorDiv;
class CDoorSet;
typedef CDoorDiv * PCDoorDiv;

class DLLEXPORT CDoor {
   friend class CDoorDiv;

public:
   ESCNEWDELETE;

   CDoor (void);
   ~CDoor (void);
   BOOL ShapeSet (DWORD dwNum, PCPoint pPoints, fp fThickness);
   PCListFixed ShapeGet (fp *pfThickness);
   BOOL StyleSet (DWORD dwStyle);
   BOOL StyleHandleSet (DOORKNOB *pKnob);
   CDoor * Clone (void);
   PCMMLNode2 MMLTo (void);
   BOOL MMLFrom (PCMMLNode2 pNode);
   DWORD SurfaceQuery (void);
   void Render (POBJECTRENDER pr, CRenderSurface *prs, DWORD dwSurface);
   void CalcIfNecessary (void);
   BOOL CustomDialog (PCEscWindow pWindow, PCWorldSocket pWorld, PCObjectSocket pThis,
      CDoorSet *pDoorSet, DWORD dwDoorNum, PCObjectDoor pObjectDoor);


//private:
   void DoorDivMML (PCMem pm, PCDoorDiv pDiv, PWSTR pszID);
   void DoorDivToControls (PCDoorDiv pDiv, PWSTR pszID, PCEscPage pPage);
   void CalcDoorKnob (void);
   BOOL CommitRect (PCPoint pMin, PCPoint pMax, fp fY, DWORD dwColor, DWORD dwFlags,
      PCMatrix pMatrix = NULL);

   CListFixed     m_lShape;      // list of CPoint that define the shape
   fp         m_fThickness;  // depth of the door
   DWORD          m_dwStyle;     // stle, from DS_XXX.
   DWORD          m_dwHandleStyle;  // doorknob style, DHS_XXX
   CPoint         m_pHandleLoc;  // doorknob location, exact meaning depends upon dwhandlepos
   DWORD          m_dwHandlePos; // specifies how to interperet handle loc.
   DOORKNOB       m_DoorKnob;    // doorknob information

   // automacially calculated
   BOOL           m_fDirty;      // TRUE if the door drawing information is dirty
   BOOL           m_dwSurface;   // bitfield indicating what surfaces are needed., from DSURF_XXX 
   CListFixed     m_lRENDELEM;   // list of rendering elements
   PCDoorDiv      m_pDoorDiv;    // to draw the door. May or may not be automaticaly calculated

   CListFixed     m_lRenderFront;   // minimize mallocs
   CListFixed     m_lRenderBack;    // minimize mallocs
};
typedef CDoor * PCDoor;


/**********************************************************************************
CDoorSet */

#define  DSORIENT_LEFT        0     // door oepns to the left
#define  DSORIENT_RIGHT       1     // door opens to the right
#define  DSORIENT_LEFTRIGHT   2     // door opens to both the left and right
#define  DSORIENT_UP          3     // door opens up
#define  DSORIENT_DOWN        4     // door opens down
#define  DSORIENT_UPDOWN      5     // door opens both up and down

#define  DSMOVE_HINGED        0     // door is hinged on DSORIENT_ side
#define  DSMOVE_PIVOT         1     // door pivots around the center and turns towards DSORIENT
#define  DSMOVE_SLIDE1        2     // door slides towards DSORIENT. Last one in list is fixed
#define  DSMOVE_SLIDE2        3     // door slides towards DSORIENT and into wall
#define  DSMOVE_BIFOLD        4     // bifold doors
#define  DSMOVE_GARAGE        5     // like garage door that is rolled parallel to ceiling
#define  DSMOVE_ROLLER        6     // rolls up on the side
#define  DSMOVE_FIXED         7     // no movement


#define  DSSTYLE_BITS         0x3f00  // use these to isolate the style, except for window flag
#define  DSSTYLE_WINDOW       0x8000  // or this into DSSYTLE_XXX, exclusive with DSSTYLE_CABINET
#define  DSSTYLE_CABINET      0x4000   // or this into DSSTYLE_XXX, exclusive with DSSTYLE_WINDOW
#define  DSSTYLE_CUSTOM       0x0000     // custom door style
#define  DSSTYLE_FIXED        0x0100   // fixed
#define  DSSTYLE_HINGEL       0x0200   // hinged, left, 1 division
#define  DSSTYLE_HINGER       0x0300   // hinged, right, 1 division
#define  DSSTYLE_HINGELR      0x0400   // hinged, left and right, 2 divisions
#define  DSSTYLE_HINGEU       0x0500   // hinged, up, 1 dicision
#define  DSSTYLE_BIL          0x0600   // bifold, left 4 divisions
#define  DSSTYLE_BILR         0x0700   // bifold, left/right 4 divisions
#define  DSSTYLE_SLIDEL       0x0800   // slide, left, 2 divisions
#define  DSSTYLE_SLIDEU       0x0900   // slide, up, 2 divisions
#define  DSSTYLE_POCKL        0x0a00   // pocket, left, 1 division
#define  DSSTYLE_POCKLR       0x0b00   // pocket, left/right, 2 divisions
#define  DSSTYLE_POCKU        0x0c00   // pocket up, 1 division
#define  DSSTYLE_GARAGEU      0x0d00   // garage, up, 4 divisions
#define  DSSTYLE_GARAGEU2     0x0e00   // garage up, 1 division
#define  DSSTYLE_ROLLERU      0x0f00   // roller up, 8 divisions
#define  DSSTYLE_LOUVER       0x1000   // pivot, up(?), division for louver height
#define  DSSTYLE_HINGELR2     0x1100   // hinged, left/right, 2 divisions, custom shape - saloon door
#define  DSSTYLE_HINGEL2      0x1200   // hinged, left, 1 division, FOR EXTERIOR DOORS
#define  DSSTYLE_POCKLR2      0x1300   // pocket, LR, 2 divisions, on outside
#define  DSSTYLE_HINGELO      0x1400   // hinged, left, 1 division, on the OUTSIDE
#define  DSSTYLE_HINGERO      0x1500   // hinged, right, 1 division, on the OUTSIDE
#define  DSSTYLE_HINGELRO     0x1600   // hinged, left and right, 2 divisions, on the OUTSIDE

#define  DSCS_FULL            0     // door occupies full space
#define  DSCS_RECTANGLE       1     // door is a rectangle in the space
#define  DSCS_SALOON          2     // saloon style doors

class DLLEXPORT CDoorSet {
public:
   ESCNEWDELETE;

   CDoorSet (void);
   ~CDoorSet (void);
   BOOL ShapeSet (PCSpline pShape);
   BOOL DivisionsSet (BOOL fFixed, fp fDivisions, DWORD dwOrient, DWORD dwMovement);
   void DivisionsGet (BOOL *pfFixed, fp *pfDivisions, DWORD *pdwOrient, DWORD *pdwMovement);
   BOOL ClipPolygons (PCListFixed pPoints, DWORD dwPlane, fp fOffset, BOOL fKeepLess);
   void OpenSet (fp fMin, fp fMax);
   void OpenGet (fp *pfMin, fp *pfMax);
   BOOL CustomShapeSet (DWORD dwShape, fp fBottom, fp fTop, BOOL fAlternate);
   void CustomShapeGet (DWORD *pdwShape, fp *pfBottom, fp *pfTop, BOOL *pfAlternate);
   CDoorSet * Clone (void);
   PCMMLNode2 MMLTo (void);
   BOOL MMLFrom (PCMMLNode2 pNode);
   DWORD SurfaceQuery (void);
   void Render (POBJECTRENDER pr, CRenderSurface *prs, DWORD dwSurface, fp fOpen);
   BOOL StyleSet (DWORD dwStyle);
   DWORD StyleGet (void);
   DWORD DoorNum (void);
   CDoor *DoorGet (DWORD dwID);
   BOOL DoorSet (DWORD dwID, CDoor *pDoor);
   fp ThicknessGet (void);
   BOOL ThicknessSet (fp fThick);
   BOOL KnobDialog (PCEscWindow pWindow, PCWorldSocket pWorld, PCObjectSocket pThis,
      PCObjectDoor pObjectDoor);

//private:
   void CalcIfNecessary (void);

   // user specified
   CListFixed     m_lShape;      // list of CPoints that defines the door shape.
   DWORD          m_dwStyle;     // style for the door set
   fp         m_fThickness;  // how thick the door set is
   BOOL           m_fFixed;      // if FALSE then fDivisions is max size of doors, TRUE is number of door divisions
   fp         m_fDivisions;  // see m_fFixed
   DWORD          m_dwOrient;    // Orientation of doors. See DSORIENT_XXX
   DWORD          m_dwMove;      // how the door moves. See DSMOVE_XXX
   fp         m_fOpenMin;    // minimum amount that will draw opened, from 0..1
   fp         m_fOpenMax;    // maximum amount that will draw opened, from 0..1
   BOOL           m_fCustomAlternate;  // if TRUE, when draw custom shape will alternate, flipping
   DWORD          m_dwCustomShape;  // shape number if custom. 0 if not on list
   TEXTUREPOINT   m_tCustomMinMax;  // height for custom shape, from 0..1, .h is bottom, .v is top
   DWORD          m_adwDKStyle[2];  // door knob style, DKS_XXX, [1] = push side, [0] = pull side
   TEXTUREPOINT   m_tDKLoc;      // location of doorknob. .h is distance from edge, .v is distance from bottom
   BOOL           m_afKick[2];   // set to TRUE if kickplace. [1] = push side, [0] = pull side
   TEXTUREPOINT   m_tKickLoc;    // .h is offset from left, right, and bottom of door. .v is kickplate height
   fp             m_fKnobRotate; // rotate the knob this amount

   // automatically entered
   BOOL           m_fDirty;      // set to TRUE if this is dirty
   CListFixed     m_lDoors;      // list of PCDoors for all the doors
   DWORD          m_dwDoors;     // number of doors actually using
   CPoint         m_pShapeMin;   // min coordinates of m_lShape
   CPoint         m_pShapeMax;   // mxa coordinates of m_lShape
};
typedef CDoorSet *PCDoorSet;

DLLEXPORT BOOL ClipPolygons (PCListFixed pPoints, DWORD dwPlane, fp fOffset, BOOL fKeepLess);

/**********************************************************************************
CDoorOpening */

class DLLEXPORT CDoorOpening {
public:
   ESCNEWDELETE;

   CDoorOpening (void);
   ~CDoorOpening (void);
   BOOL ShapeSet (PCSpline psShape, BOOL fOutside,
                                fp fYWallOutside, fp fYWallInside,
                                fp fYFrameOutside, fp fYFrameInside);
   PCSpline ShapeGet (BOOL *pfOutside,
                                fp *pfYWallOutside, fp *pfYWallInside,
                                fp *pfYFrameOutside, fp *pfYFrameInside);
   BOOL StyleInit (DWORD dwStyle);
   void ExtendSet (BOOL fOutside, fp fLeft, fp fRight, fp fTop, fp fBottom);
   void ExtendGet (BOOL fOutside, fp *pfLeft, fp *pfRight, fp *pfTop, fp *pfBottom);
   CDoorOpening * Clone (void);
   PCMMLNode2 MMLTo (void);
   BOOL MMLFrom (PCMMLNode2 pNode);
   DWORD SurfaceQuery (void);
   void Render (POBJECTRENDER pr, CRenderSurface *prs, DWORD dwSurface, fp fOpen);
   void CalcIfNecessary (void);
   PCDoorSet DoorSetGet (DWORD dwPosn);
   BOOL DoorSetRemove (DWORD dwPosn);
   BOOL DoorSetAdd (DWORD dwPosn, DWORD dwStyle);
   BOOL DoorSetFlip (DWORD dwPosn);

//private:
   // set by user
   CSpline        m_sShape;      // shape spline
   BOOL           m_fOutside;    // TRUE if lookin at this from outside (clickwise)
   CPoint         m_pYWallFrame; // [0] = wallout, [1]=wallin, [2]=frameout, [3]=framein
   CPoint         m_apExtend[2]; // For out-of-frame door-sets, extend edges a bit...
                                 // [0] = most external, [1] = most internal
                                 // .p[0] = left, .p[1]=right, .p[2]=up, .p[3]=down

   // auto
   BOOL           m_fDirty;      // set to TRUE if dirty
   PCDoorSet      m_apDoorSet[6];   // [0] is most external, [5] is most internal
};

/***************************************************************************************
Help */
DLLEXPORT void ASPHelp (DWORD dwRes = 239);
DLLEXPORT void ASPHelpEnd (void);
DLLEXPORT void ASPHelpCheckPage (void);
DLLEXPORT void ASPHelpInit (void);
DLLEXPORT BOOL HelpDefPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam);

/*****************************************************************************
CShadowBuf */

#define FLOATSHADOWPIXELS     // for more accuracy. see if fixes problem

class DLLEXPORT CShadowBuf {
public:
   ESCNEWDELETE;

   CShadowBuf (void);
   ~CShadowBuf (void);

   CShadowBuf *Clone (void);

   BOOL Init (PCRenderTraditional pRender, PCPoint pEye, PCPoint pDirection, PCPoint pUp, PCWorldSocket pWorld,
      fp fIsotripicScale, DWORD dwShowFlags, DWORD dwPixels, BOOL fFinalRender,
      DWORD dwShowOnly, PCPoint papClipPlane, fp fLightMaximum, PCProgressSocket pProgress);
   BOOL ShadowTest (const PCPoint pTest, BOOL fIsTransformed, fp *pfDist, fp *pfX, fp *pfpY);
   void ShadowTransform (const PCPoint pWorld, PCPoint pShadow);
   void TestToImage (PCImage pFill);
   fp PixelInterp (fp fX, fp fY, BOOL fFast);
   DWORD HasShadowBuf (void);



   __inline DWORD Size (void) {return m_dwXY;};
   __inline fp Pixel (DWORD dwX, DWORD dwY)
      {
#ifdef FLOATSHADOWPIXELS
         return ((float*)m_memPixels.p)[dwX + dwY * m_dwXY];
#else
         WORD w = ((WORD*)m_memPixels.p)[dwX + dwY * m_dwXY];
         if (w == 0xffff)
            return 1000000;
         return (fp) (fp)w * m_fScale + m_fMin;
#endif
      };
   __inline void BoundaryGet (PCPoint pMin, PCPoint pMax)
      {
         pMin->Copy (&m_apBoundary[0]);
         pMax->Copy (&m_apBoundary[1]);
      };

private:
   CPoint      m_pDirection;     // normalized direction vector that looking at
   CPoint      m_pEye;           // point looking from
   CRenderMatrix m_RM;           // render information from CRenderTraditional
   DWORD       m_dwXY;           // width and height of m_memPixels
   CMem        m_memPixels;      // memory of x*y with WORD (or float if  FLOATSHADOWPIXELS) values, from 0=m_fMin, to 0xfffe=m_fMax, 0xffff=infinite
#ifdef FLOATSHADOWPIXELS
   fp          m_fPush;          // extra push value to account for roundoff error
#endif
   fp          m_fMin;           // minimum value in m_mempixels
   fp          m_fMax;           // maximum value in m_memPixels
   fp          m_fScale;         // for determining pixel
   CPoint      m_apBoundary[2];  // 2 points that form min and max boundary points
   BOOL        m_fIsotropic;     // set to TRUE if isotropic, FALSE if perspective
   fp          m_afCenterPixel[2];  // from Render
   fp          m_afScalePixel[2];   // from Render

};
typedef CShadowBuf *PCShadowBuf;


/*****************************************************************************
CLight */
#define LIFORM_POINT          0     // point light source
#define LIFORM_INFINITE       1     // infinite light source
#define LIFORM_AMBIENT        2     // ambient light surce - use lumens[2]


class DLLEXPORT CLight {
public:
   ESCNEWDELETE;

   CLight (DWORD dwRenderShard, int iPriorityIncrease);
   ~CLight (void);

   CLight *Clone (void);

   void WorldSet (PCWorldSocket pWorld, DWORD dwShowFlags, BOOL fFinalRender, DWORD dwShowOnly);
   BOOL LightInfoSet (PLIGHTINFO pInfo, fp fLightMaximum);
   BOOL LightInfoGet (PLIGHTINFO pInfo, fp *pfLightMaximum);
   void DirtySet (void);
   BOOL DirtyGet (void);
   BOOL RecalcIfDirty (PCRenderTraditional *ppRender, PCProgressSocket pProgress);    // so can do this in advance
   BOOL CalcIntensity (PCRenderTraditional *ppRender, const PCPoint pLoc, PSCANLINEINTERP paSLI, fp fZScanLineInterp,
      fp *pafColor, fp fMinIntensity);
   void CalcSCANLINEINTERP (PCRenderTraditional *ppRender, DWORD dwPixels, PCPoint pBaseLeft, PCPoint pBaseRight,
                         PCPoint pBasePlusMeterLeft, PCPoint pBasePlusMeterRight,
                         PCListFixed plSCANLINEINTERP);
   void TestImage (PCImage pImage, DWORD dwImage);
   void Spotlight (PCRenderTraditional *ppRender, BOOL fLargeSpot, PCPoint pCenter, fp fDiameter,
      PCPoint papClipPlane, PCProgressSocket pProgress);
   DWORD HasShadowBuf (void);

   DWORD          m_dwRenderShard;  // render shard for the light
   int            m_iPriorityIncrease; // how much to increase/decrease the thread priority

private:
   LIGHTINFO      m_li;          // information abou the light. Coords are in world space
   BOOL           m_fDirty;      // set to TRUE if light is dirty, FALSE if all info is good
   CPoint         m_apBoundary[2];  // min/max bounding box. If outside this then light in shadow
   PCWorldSocket        m_pWorld;      // world that using
   DWORD          m_dwShowFlags; // which elements of the world are being shown
   BOOL           m_fFinalRender;   // set to TRUE if dinal render
   DWORD          m_dwShowOnly;  // -1 for all objects casting shadows, or object number
   PCShadowBuf    m_apShadowBuf[6]; // array of shadow buffers
   CPoint         m_pDirNorm;    // normalized direction
   fp             m_afDirectCos[2][2]; // contain the cos of directionality info, for faster compares
   fp             m_fLightMaximum;  // maximum light distance, or 0 if none

   CPoint         m_pSpotCenter;    // last large spotlite center
   fp             m_fSpotDiameter;  // last large spotlite diameter
};
typedef CLight *PCLight;



/**********************************************************************************
CObjectSkydome */
DEFINE_GUID(CLSID_Skydome, 
0x157d040d, 0x971a, 0x63f2, 0xf2, 0xd3, 0x83, 0xd6, 0x1, 0x51, 0x17, 0x89);

DEFINE_GUID(CLSID_SkydomeSub,
0xc29, 0xbcb3, 0xb, 0x88, 0x1a, 0x7f, 0xf5, 0x55, 0xce, 0xc2, 0x1);

DEFINE_GUID(CLSID_ImageMoonCode,
0x8892e71e, 0x8497, 0x4832, 0x98, 0x97, 0x65, 0xe4, 0xb9, 0xe1, 0x74, 0x89);
DEFINE_GUID(CLSID_ImageMoonSub,
0x724, 0x5a10, 0x36, 0x50, 0x85, 0x41, 0xc7, 0x74, 0xd4, 0xc2, 0x1);

typedef struct {
   WCHAR          szName[64];    // name to display
   BOOL           fVisible;      // set to TRUE if it's vibile.
   CPoint         pDir;          // vector pointint in the direction of the light. Normalized
   fp             fLumens;       // lumens for the light
   WORD           awColor[3];    // color for the light
   fp             fAmbientLumens;   // brightness of ambient light (in lumens)
   WORD           awAmbientColor[3];   // color of ambient light
   fp             fSolarLumens;  // lumens of light when reflecting onto planets
   WORD           awSolarColor[3];  // color of light when reflecting onto planets
   fp             fSizeRad;      // number of radians across this is. Sun and moon are about 5 degrees
   fp             fDist;         // distance (in meters)
   BOOL           fEmitLight;    // set to TRUE if this is light emitting
   PCObjectSurface pSurface;     // describing the image. NULL if none
   fp             fBrightness;   // how bright to display surface image, # lumens where will be accurate match

   // calculated
   CPoint         pLoc;          // location in space
   fp             fRadius;       // radius, in meters
   TEXTUREPOINT   tpAltAz;       // altitude and azimuth
   TEXTUREPOINT   tpPixel;       // pixel location
   fp             fIntensity;    // intensity in terms of x basic color of light
} SKYDOMELIGHT, *PSKYDOMELIGHT;

typedef struct {
   float       fRed;
   float       fGreen;
   float       fBlue;
   float       fDensity;         // density of atmophere at pixel
   float       afOrig[3];        // original atmophere colors
   float       fCloudZ;          // Z value for cloud, ZINFINITE if no cloud
   float       fCloudDensity;    // thickness in meters of cloud
   float       afCloudPixel[3];  // location in space where visible portion of cloud is
   WORD        awCloudPixel[2];  // original pixel location where came from - used for blending
} SKYPIXEL, *PSKYPIXEL;


#define CLOUDLAYERS        2
// CObjectSkydome is a test object to test the system
class DLLEXPORT CObjectSkydome : public CEscMultiThreaded, public CObjectTemplate {
public:
   CObjectSkydome (PVOID pParams, POSINFO pInfo);
   ~CObjectSkydome (void);

   virtual void Delete (void);
   virtual void Render (POBJECTRENDER pr, DWORD dwSubObject);
   virtual CObjectSocket *Clone (void);
   virtual PCMMLNode2 MMLTo (void);
   virtual BOOL MMLFrom (PCMMLNode2 pNode);
   virtual BOOL MoveReferencePointQuery (DWORD dwIndex, PCPoint pp);
   virtual BOOL MoveReferenceStringQuery (DWORD dwIndex, PWSTR psz, DWORD dwSize, DWORD *pdwNeeded);
   virtual BOOL LightQuery (PCListFixed pl, DWORD dwShow);
   virtual BOOL Message (DWORD dwMessage, PVOID pParam);
   virtual BOOL DialogQuery (void);
   virtual BOOL DialogShow (DWORD dwSurface, PCEscWindow pWindow, PCPoint pClick);
   virtual BOOL TextureQuery (PCListFixed plText);
   virtual BOOL AttribGetIntern (PWSTR pszName, fp *pfValue);
   virtual void AttribGetAllIntern (PCListFixed plATTRIBVAL);
   virtual void AttribSetGroupIntern (DWORD dwNum, PATTRIBVAL paAttrib);
   virtual BOOL AttribInfoIntern (PWSTR pszName, PATTRIBINFO pInfo);
   virtual BOOL Deconstruct (BOOL fAct);
   virtual void QueryBoundingBox (PCPoint pCorner1, PCPoint pCorner2, DWORD dwSubObject);

   void CalcAll (void);

//private:
   void CalcSunAndMoon (void);
   void CreateDome (void);
   DWORD PointExistInDome (PCPoint pWant);
   void MakeSureTextureExists (void);
   double CalcTextureHash (void);
   void CalcTextureGUID (GUID *pgID);
   void FillAtmosphere (PSKYPIXEL ps);
   void DirectionToAltAz (PCPoint pDir, PTEXTUREPOINT ptAltAz);
   void AltAzToDirection (PTEXTUREPOINT ptAltAz, PCPoint pDir);
   void AltAzToPixel (PTEXTUREPOINT ptAltAz, PTEXTUREPOINT ptPixel, BOOL fLimit = FALSE);
   void PixelToAltAz (PTEXTUREPOINT ptPixel, PTEXTUREPOINT ptAltAz);
   void PixelToDirection (PTEXTUREPOINT ptPixel, PCPoint pDir);
   void DirectionToPixel (PCPoint pDir, PTEXTUREPOINT ptPixel);
   void ApplyAtmophereToPixel (int iX, int iY, float *pafPixel,PSKYPIXEL ps);
   void DrawSunOrPlanet (PSKYDOMELIGHT pSun, PSKYPIXEL ps,
      PSKYDOMELIGHT pSunList, DWORD dwNumSun);
   void CalcSunOrPlanet (PSKYDOMELIGHT pSun);
   void HazeAroundSunOrPlanet (PSKYDOMELIGHT pSun, PSKYPIXEL ps, fp fBrightest);
   void DrawStars (PSKYPIXEL ps);
   void CloudClearArea (PSKYPIXEL ps, DWORD dwXMin = 0, DWORD dwYMin = 0,
                                     DWORD dwXMax = 100000, DWORD dwYMax = 100000);
   void CloudCalcLoc (PSKYPIXEL ps, DWORD dwXMin = 0, DWORD dwYMin = 0,
                                     DWORD dwXMax = 100000, DWORD dwYMax = 100000);
   void CloudRender (COLORREF cColor, PSKYPIXEL ps,
      PSKYDOMELIGHT pSunList, DWORD dwNumSun,fp fDensityScale,
      DWORD dwXMin = 0, DWORD dwYMin = 0,
      DWORD dwXMax = 100000, DWORD dwYMax = 100000);
   void CloudSmooth (PSKYPIXEL ps, DWORD dwSize);
   void CloudCalcCirrus (DWORD dwLayer, PSKYPIXEL ps);
   void DrawCirrus (DWORD dwLayer, PSKYPIXEL ps, PSKYDOMELIGHT pSunList, DWORD dwNumSun);
   void CloudCalcCumulus (PSKYPIXEL ps);
   void DrawCumulus (PSKYPIXEL ps, PSKYDOMELIGHT pSunList, DWORD dwNumSun);
   void SRand (DWORD dwSeed);
   void DrawSphere (PCPoint pLoc, fp fRadPix, PTEXTUREPOINT ptpPixelOrig, PSKYPIXEL ps);
   void CalcAttribList (void);

   // CEscMultiThreaded
   virtual void EscMultiThreadedCallback (PVOID pParams, DWORD dwParamSize,
      DWORD dwThread);

   DWORD SkyRes (void);

   CRenderSurface m_Renderrs;    // optimization

   fp             m_fYear;       // current year. Only used as int
   fp             m_fTimeInYear; // from 0..11.999, 0=Jan1, 11.9999=Dec31
   fp             m_fTimeInDay;  // from 0..23.9999, 0 = 12 midnight, 23.999=11:59 PM

   fp             m_fMoonSize;   // make moon and sun this much larger
   BOOL           m_fShowSun;    // set to TRUE if should draw the sun
   BOOL           m_fShowMoon;   // set to TRUE if should draw the moon
   BOOL           m_fDimClouds;  // set to TRUE to automatically dim sun and moon based on clouds

   DWORD          m_dwResolution;   // 0 = 1000x1000, 1=2000x2000, 2=4000x4000

   fp             m_fHaze;       // number from 0 to 10. 1 is default.
   fp             m_fDensityZenith; // density at the zenith
   fp             m_fSkydomeSize;   // number of meters in radius for skydome
   COLORREF       m_cAtmosphereGround; // color of atmosphere at ground
   COLORREF       m_cAtmosphereMid; // color of atmophere at midpoint
   COLORREF       m_cAtmosphereZenith; // color of atmosphere at zenith
   BOOL           m_fAutoColors; // if TRUE automatically generate atmophere colors

   DWORD          m_dwStarsNum;  // number of stars

   BOOL           m_afCirrusDraw[CLOUDLAYERS];    // set to true if using cirrus clouds
   COLORREF       m_acCirrusColor[CLOUDLAYERS];   // color of cirrus clouds
   fp             m_afCirrusScale[CLOUDLAYERS];   // scale of fundamental frequency, in m
   fp             m_afCirrusDetail[CLOUDLAYERS];  // 0..1, amount of detail in clouds, 1 is more detail
   fp             m_afCirrusCover[CLOUDLAYERS];   // 0..1, the higher the number the more area is covered
   fp             m_afCirrusThickness[CLOUDLAYERS];  // how thick the clouds get, 0..1
   CPoint         m_apCirrusLoc[CLOUDLAYERS];     // x,y= offset for animation, z = height
   DWORD          m_adwCirrusSeed[CLOUDLAYERS];   // random number seed

   BOOL           m_fCumulusDraw;    // set to true if using Cumulus clouds
   COLORREF       m_cCumulusColor;   // color of Cumulus clouds
   fp             m_fCumulusScale;   // scale of fundamental frequency, in m
   fp             m_fCumulusDetail;  // 0..1, amount of detail in clouds, 1 is more detail
   fp             m_fCumulusCover;   // 0..1, the higher the number the more area is covered
   fp             m_fCumulusThickness;  // how thick the clouds get, 0..1
   fp             m_fCumulusSheer;   // 0..1 - amount of sheer applied to tops
   fp             m_fCumulusSheerAngle;   // radians. angle of sheer
   CPoint         m_pCumulusLoc;     // x,y= offset for animation, z = height
   DWORD          m_dwCumulusSeed;  // random number seed

   CListFixed     m_lSuns;       // list of sunds



   // calculated
   CListFixed     m_lAttrib;     // list of attributes supported. ATTRIBVAL - the value is used to indicate which attribute
   BOOL           m_fSunMoonDirty;  // if true then the sun and moon info are dirty
   SKYDOMELIGHT   m_lSun;        // sun info
   SKYDOMELIGHT   m_lMoon;       // moon info
   CMem           m_memPoints;   // memory containing the dome points
   CMem           m_memNormals;  // memory containing the dome normals
   CMem           m_memText;     // memory containing the dome's textures
   CMem           m_memVertices; // memory containing the dome vertices
   CMem           m_memPoly;     // memory containing the dome's polygons
   DWORD          m_dwNumPoints; // number of points in m_memPoints
   DWORD          m_dwNumNormals;   // number of normals
   DWORD          m_dwNumText;   // number of textures
   DWORD          m_dwNumVertices;  // number of vertices
   DWORD          m_dwNumPoly;   // number of polygons; they're all triangles, so know size too
   // GUID           m_gTexture;    // GUID for the automagically produced texture
};
typedef CObjectSkydome *PCObjectSkydome;



/**********************************************************************************
CObjectLight */

/*********** LIGHT SHADES *********/
// the following shades open out up
#define LST_ROUNDDIAGCLOTH2         10       // round, linediag2, cloth, incandescent
#define LST_ROUNDDIAGCLOTH23        11       // round, linediag23, cloth, incandecsent
#define LST_ROUNDDIAGSTRAIGHT       12       // round, line, cloth, incandecsent
#define LST_SQUAREDIAGCLOTH2        13       // square, linediag2, cloth, incandescent
#define LST_SQUAREDIAGCLOTH23       14       // square, linediag23, cloth, incandecsent
#define LST_SQUAREDIAGSTRAIGHT      15       // square, line, cloth, incandecsent
#define LST_FANDIAGCLOTH2           16       // round fan, linediag2, cloth, incandescent
#define LST_FANDIAGCLOTH23          17       // round fan, linediag23, cloth, incandecsent
#define LST_FANDIAGSTRAIGHT         18       // round fan, line, cloth, incandecsent
#define LST_ROUNDOLDFASHCLOTH       19       // round, old fashioned cloth

// the following shades open out down
#define LST_ROUNDDIAGCLOTH2DN       20       // round, linediag2, cloth, incandescent
#define LST_ROUNDDIAGCLOTH23DN      21       // round, linediag23, cloth, incandecsent
#define LST_ROUNDDIAGSTRAIGHTDN     22       // round, line, cloth, incandecsent
#define LST_SQUAREDIAGCLOTH2DN      23       // square, linediag2, cloth, incandescent
#define LST_SQUAREDIAGCLOTH23DN     24       // square, linediag23, cloth, incandecsent
#define LST_SQUAREDIAGSTRAIGHTDN    25       // square, line, cloth, incandecsent
#define LST_FANDIAGCLOTH2DN         26       // round fan, linediag2, cloth, incandescent
#define LST_FANDIAGCLOTH23DN        27       // round fan, linediag23, cloth, incandecsent
#define LST_FANDIAGSTRAIGHTDN       28       // round fan, line, cloth, incandecsent
#define LST_ROUNDOLDFASHCLOTHDN     29       // round, old fashioned cloth

// following are glass shades
#define LST_GLASSINCANROUNDCONEDN   30
#define LST_GLASSINCANSQUARECONEDN  31
#define LST_GLASSINCANHEXCONEDN     32
#define LST_GLASSINCANROUNDHEMIDN   33       // hemisphere
#define LST_GLASSINCANHEXHEMIDN     34       // hexagon around, hemisphere shape
#define LST_GLASSINCANOCTHEMIDN     35
#define LST_GLASSINCANSQUAREOPENDN  36       // square with open on top and bottom
#define LST_GLASSINCANROUNDEDDN     37       // rounded on one end

#define LST_GLASSINCANROUNDCONE     40
#define LST_GLASSINCANSQUARECONE    41
#define LST_GLASSINCANHEXCONE       42
#define LST_GLASSINCANROUNDHEMI     43       // hemisphere
#define LST_GLASSINCANHEXHEMI       44       // hexagon around, hemisphere shape
#define LST_GLASSINCANOCTHEMI       45
#define LST_GLASSINCANSQUAREOPEN    46
#define LST_GLASSINCANROUNDED       47       // rounded on one end

// glass covers
#define LST_GLASSINCANOLDFASHIONED  50       // old fashioned incandescent cover
#define LST_GLASSINCANBULB          51       // bulb around the light
#define LST_GLASSINCANBOX           52       // box around the light
#define LST_GLASSINCANCYLINDER      53       // cylinder around the light
#define LST_GLASSINCANBULBLID       54       // bulb with lid
#define LST_GLASSINCANBOXLID        55       // incandensent box with a lid
#define LST_GLASSINCANCYLINDERLID   56       // cylinder with lid
#define LST_GLASSINCANBOXROOF       57       // rectangular with roof
#define LST_GLASSINCANCYLINDERROOF  58       // cylinder with roof
#define LST_GLASSINCANBULBBBASE     59       // round bulb, but with a drawn base

// halogen varieties
#define LST_HALOGENBOWL             60       // on tall halogeen light stands
#define LST_HALOGEN8BULB            61       // small incandesncent cylinder around 8w halogen
#define LST_HALOGEN8BOX             62       // cylinder with a lid
#define LST_HALOGEN8CYLINDER        63       // sphere with a lid
#define LST_HALOGEN8BULBLID         64       // bulb with lid
#define LST_HALOGEN8BOXLID          65       // incandensent box with a lid
#define LST_HALOGEN8CYLINDERLID     66       // cylinder with lid
#define LST_HALOGEN8BOXROOF         67       // box with roof
#define LST_HALOGEN8CYLINDERROOF    68       // cylinder with roof

// incandescnet spotlight
#define LST_INCANSPOT1              70       // style 1 - flared
#define LST_INCANSPOT2              71       // style 2- angled
#define LST_INCANSPOT3              72       // style 3 - larger at one end
#define LST_INCANSPOT4              73       // cylinder
#define LST_INCANSPOT5              74       // hemi-sphere
#define LST_INCANSPOT6              75       // half cylinder
#define LST_LAVALAMP                76       // top of lava lamp

// flourescent sptlight
#define LST_FLOUROSPOT1             80       // half cylinder, attached in long end
#define LST_FLOUROSPOT2             81       // half cylinder, attached in middle
#define LST_FLOUROSPOT2LID          82       // flourospot 1, but also has a lid

// sulphure bulb
#define LST_SODIUMSPOT1             90       // hemi-sphere
#define LST_SODIUMSPOT2             91       // half cylinder

// flouro shades
#define LST_FLOURO8W1               100      // 1 8w flouro
#define LST_FLOURO18W1              101      // 1 18w flouro
#define LST_FLOURO18W2              102      // 2 18w flouro
#define LST_FLOURO36W1              103      // 1 36w flouro
#define LST_FLOURO36W2              104      // 2 36w flouro
#define LST_FLOURO36W4              105      // 4 36w flouro in drop-down commerical ceiling
#define LST_FLOUROROUND             106      // round flouro light in hemi-sphere

// halogen spotlights
#define LST_HALOGENSPOT1            110       // style 1 - flared
#define LST_HALOGENSPOT2            111       // style 2- angled
#define LST_HALOGENSPOT3            112       // style 3 - larger at one end
#define LST_HALOGENSPOT4            113       // cylinder
#define LST_HALOGENCEILSPOT         114     // embedded in ceiling as spotlight
#define LST_HALOGENTUBESPOT         115      // spotlight using halogen tube
#define LST_INCANPOOLTABLE          116      // for a pool table

// incandescent ceiling mounted shades
#define LST_INCANCEILSQUARE         120      // square profile, on the ceiling
#define LST_INCANCEILHEMISPHERE     121      // hemisphere profile, on the ceiling
#define LST_INCANCEILCONE           122      // cone, on the ceiling
#define LST_INCANCEILROUND          123      // roun, on ceiling
#define LST_INCANCEILSPOT           124      // embedded in ceiling as spotlight

// other
#define LST_INCANBULBSOCKET         130      // socket alone
#define LST_SKYLIGHTDIFFUSERROUND   131      // round skylight diffuser
#define LST_SKYLIGHTDIFFUSERSQUARE  132      // square skylight diffuser
#define LST_HALOGENFAKECANDLE       133      // draw a fake candle
#define LST_INCANSPOTSOCKET         134      // socket alone

// curved flouro shades
#define LST_FLOURO8W1CURVED         140      // 1 8w flouro
#define LST_FLOURO18W1CURVED        141      // 1 18w flouro
#define LST_FLOURO18W2CURVED        142      // 2 18w flouro
#define LST_FLOURO36W1CURVED        143      // 1 36w flouro
#define LST_FLOURO36W2CURVED        144      // 2 36w flouro

// wall sconces
#define LST_INCANSCONCECONE         150      // cone with light going up
#define LST_INCANSCONCEHEMISPHERE   151      // hemisphere with light goingup
#define LST_INCANSCONELOOP          152      // loop with light going up and down

// fire bits
#define LST_FIRECANDLE1             160      // candle with holder
#define LST_FIRECANDLE2             161      // candle without holder, but thicker
#define LST_FIRETORCH               162      // torch
#define LST_FIREBOWL                163      // bowl with fire in it
#define LST_FIRELANTERNHUNG         164      // hanging lantern
#define LST_FLAMECANDLE             165      // candle flame by itself
#define LST_FLAMETORCH              166      // torch flame by itself
#define LST_FLAMEFIRE               167      // fire flame by itself

//light bulbs
#define LST_BULBINCANGLOBE          170
#define LST_BULBINCANSPOT           171
#define LST_BULBHALOGENSPOT         172
#define LST_BULBHALOGENBULB         173
#define LST_BULBHALOGENTUBE         174
#define LST_BULBFLOURO8             175
#define LST_BULBFLOURO18            176
#define LST_BULBFLOURO36            177
#define LST_BULBFLOUROROUND         178
#define LST_BULBFLOUROGLOBE         179
#define LST_BULBSODIUMGLOBE         180


/********* LIGHT STANDS *********/
#define LSTANDT_OUTDOORPATHWAY         10       // outdoor, pathway light
#define LSTANDT_OUTDOORLAMP2M          11       // 2M lamp post
#define LSTANDT_OUTDOORLAMP3M          12       // 3M lamp post
#define LSTANDT_OUTDOORTULIP           13       // tulip stand
#define LSTANDT_OUTDOORLAMPTRIPPLE     14       // hold up three lamps
#define LSTANDT_OUTDOOROVERHANG        15       // large overhang outdoor
#define LSTANDT_OUTDOOROVERHANG2       16       // overhang, with two lights
#define LSTANDT_OUTDOOROVERHANG3       17       // overhand with three lights

#define LSTANDT_FLOORLAMPSIMPLE        30       // straight up
#define LSTANDT_FLOORLAMPMULTISTALK    31       // multiple stalks for spot lights
#define LSTANDT_FLOORLAMPTULIP         32       // bend on top
#define LSTANDT_FLOORLAMPARMUP         33       // arm, light facing up
#define LSTANDT_FLOORLAMPARMANY        34       // arm, light any direction
#define LSTANDT_FLOORLAMPSIMPLETALL    35       // simple but tall

#define LSTANDT_DESKLAMPARM            50       // desk lamp with simple arm
#define LSTANDT_DESKLAMPARMSMALL       51       // small arm
#define LSTANDT_DESKLAMPFLEXI          52       // flexible
#define LSTANDT_DESKLAMPSIMPLE         53       // simple post up
#define LSTANDT_DESKLAMPCURVEDOWN      54       // tulip bend, ends up curcing down
#define LSTANDT_DESKLAMPCURVEFRONT     55       // half bend, ends up going only half of curve
#define LSTANDT_DESKLAMPLAVA           56       // lava lamp base
#define LSTANDT_DESKLAMPCERAMIC1       57
#define LSTANDT_DESKLAMPCERAMIC2       58
#define LSTANDT_DESKLAMPCERAMIC3       59

#define LSTANDT_TABLELAMPSIMPLE1       70       // just vertical out of round stand
#define LSTANDT_TABLELAMPSIMPLE2       71       // square vertical out of sqaure stand
#define LSTANDT_TABLELAMPCERAMIC1      72       // shape 1
#define LSTANDT_TABLELAMPCERAMIC2      73
#define LSTANDT_TABLELAMPCERAMIC3      74
#define LSTANDT_TABLELAMPCERAMIC4      75
#define LSTANDT_TABLELAMPCERAMIC5      76
#define LSTANDT_TABLELAMPCERAMIC6      77
#define LSTANDT_TABLELAMPSQUAREBASE    78
#define LSTANDT_TABLELAMPCYLINDERBASE  79
#define LSTANDT_TABLELAMPCURVEDBASE    80
#define LSTANDT_TABLELAMPHEXBASE       81
#define LSTANDT_TABLELAMPHORNBASE      82

#define LSTANDT_NONEMOUNTED            90       // no stand, but ceiling/wall mounted
#define LSTANDT_NONESTANDING           91       // no stand, self standing

#define LSTANDT_WALLSTALKOUT           100      // wall mounted, straight out
#define LSTANDT_WALLSTALKCURVEDDOWN    101      // wall mounted, curve down
#define LSTANDT_WALLSTALKCURVEUP       102      // wall mounted curve up
#define LSTANDT_WALLSTALK2             103      // two stalks
#define LSTANDT_WALLSTALK3             104      // three stalks

#define LSTANDT_CEILINGSTALK1          110      // one stalk from ceiling
#define LSTANDT_CEILINGSTALK2          111      // two stalk from ceiling
#define LSTANDT_CEILINGSTALK3          112      // three stalks from ceiling
#define LSTANDT_CEILINGFIXED1          113      // fixed directly to base
#define LSTANDT_CEILINGFIXED2          114      // fixed directly to base
#define LSTANDT_CEILINGFIXED3          115      // fixed directly to base

#define LSTANDT_CEILINGCHANDELIER3     120
#define LSTANDT_CEILINGCHANDELIER4     121
#define LSTANDT_CEILINGCHANDELIER5     122
#define LSTANDT_CEILINGCHANDELIER6     123
#define LSTANDT_CEILINGCHANDELIER3UP   124
#define LSTANDT_CEILINGCHANDELIER4UP   125
#define LSTANDT_CEILINGCHANDELIER5UP   126
#define LSTANDT_CEILINGCHANDELIER6UP   127

#define LSTANDT_TRACK2                 130
#define LSTANDT_TRACK3                 131
#define LSTANDT_TRACK4                 132
#define LSTANDT_TRACK5                 133
#define LSTANDT_TRACKGLOBES2           134
#define LSTANDT_TRACKGLOBES3           135
#define LSTANDT_TRACKGLOBES4           136
#define LSTANDT_TRACKGLOBES5           137

#define LSTANDT_FLOURO8W1              140      // 1 8w flouro
#define LSTANDT_FLOURO18W1             141      // 1 18w flouro
#define LSTANDT_FLOURO18W2             142      // 2 18w flouro
#define LSTANDT_FLOURO36W1             143      // 1 36w flouro
#define LSTANDT_FLOURO36W2             144      // 2 36w flouro
#define LSTANDT_FLOURO36W4             145      // 4 36w flouro in drop-down commerical ceiling
#define LSTANDT_FLOUROROUND            146      // round flouro light in hemi-sphere

#define LSTANDT_FIRETORCHHOLDER        150      // holds torch at an angle
#define LSTANDT_FIRELOG                151      // log for the fireplace
#define LSTANDT_FIRECANDELABRA         152      // candelabra

#define LSCALE_ORIG        0x100000
#define LSCALE_THIRD       0x060000
#define LSCALE_HALF        0x080000
#define LSCALE_23          0x0c0000
#define LSCALE_LARGER      0x180000

#define  LIGHTTYPE(stand,shade,scale) (PVOID)((DWORD)(stand) | ((DWORD)(shade) << 8) | (scale))

// {988D241C-CF42-4b25-9EB2-69BE70BCDD4D}
DEFINE_GUID(CLSID_Light, 
0x988d241c, 0xcf42, 0x4b25, 0x9e, 0xb2, 0x69, 0xbe, 0x70, 0xbc, 0xdd, 0x4d);

// {2CCCF176-F479-4293-8950-2E684715B5B9}
DEFINE_GUID(CLSID_LightBulb1, 
0x2cccf176, 0xf479, 0x4293, 0x89, 0x50, 0x2e, 0x68, 0x47, 0x15, 0xb5, 0x1);
DEFINE_GUID(CLSID_LightBulb2, 
0x2cccf176, 0xf479, 0x4293, 0x89, 0x50, 0x2e, 0x68, 0x47, 0x15, 0xb5, 0x2);
DEFINE_GUID(CLSID_LightBulb3, 
0x2cccf176, 0xf479, 0x4293, 0x89, 0x50, 0x2e, 0x68, 0x47, 0x15, 0xb5, 0x3);
DEFINE_GUID(CLSID_LightBulb4, 
0x2cccf176, 0xf479, 0x4293, 0x89, 0x50, 0x2e, 0x68, 0x47, 0x15, 0xb5, 0x4);
DEFINE_GUID(CLSID_LightBulb5, 
0x2cccf176, 0xf479, 0x4293, 0x89, 0x50, 0x2e, 0x68, 0x47, 0x15, 0xb5, 0x5);
DEFINE_GUID(CLSID_LightBulb6, 
0x2cccf176, 0xf479, 0x4293, 0x89, 0x50, 0x2e, 0x68, 0x47, 0x15, 0xb5, 0x6);
DEFINE_GUID(CLSID_LightBulb7, 
0x2cccf176, 0xf479, 0x4293, 0x89, 0x50, 0x2e, 0x68, 0x47, 0x15, 0xb5, 0x7);
DEFINE_GUID(CLSID_LightBulb8, 
0x2cccf176, 0xf479, 0x4293, 0x89, 0x50, 0x2e, 0x68, 0x47, 0x15, 0xb5, 0x8);
DEFINE_GUID(CLSID_LightBulb9, 
0x2cccf176, 0xf479, 0x4293, 0x89, 0x50, 0x2e, 0x68, 0x47, 0x15, 0xb5, 0x9);
DEFINE_GUID(CLSID_LightBulb10, 
0x2cccf176, 0xf479, 0x4293, 0x89, 0x50, 0x2e, 0x68, 0x47, 0x15, 0xb5, 0x10);
DEFINE_GUID(CLSID_LightBulb11, 
0x2cccf176, 0xf479, 0x4293, 0x89, 0x50, 0x2e, 0x68, 0x47, 0x15, 0xb5, 0x11);
DEFINE_GUID(CLSID_LightBulb12, 
0x2cccf176, 0xf479, 0x4293, 0x89, 0x50, 0x2e, 0x68, 0x47, 0x15, 0xb5, 0x12);
DEFINE_GUID(CLSID_LightBulb13, 
0x2cccf176, 0xf479, 0x4293, 0x89, 0x50, 0x2e, 0x68, 0x47, 0x15, 0xb5, 0x13);
DEFINE_GUID(CLSID_LightBulb14, 
0x2cccf176, 0xf479, 0x4293, 0x89, 0x50, 0x2e, 0x68, 0x47, 0x15, 0xb5, 0x14);
DEFINE_GUID(CLSID_LightBulb15, 
0x2cccf176, 0xf479, 0x4293, 0x89, 0x50, 0x2e, 0x68, 0x47, 0x15, 0xb5, 0x15);
DEFINE_GUID(CLSID_LightBulb16, 
0x2cccf176, 0xf479, 0x4293, 0x89, 0x50, 0x2e, 0x68, 0x47, 0x15, 0xb5, 0x16);
DEFINE_GUID(CLSID_LightBulb17, 
0x2cccf176, 0xf479, 0x4293, 0x89, 0x50, 0x2e, 0x68, 0x47, 0x15, 0xb5, 0x17);
DEFINE_GUID(CLSID_LightBulb18, 
0x2cccf176, 0xf479, 0x4293, 0x89, 0x50, 0x2e, 0x68, 0x47, 0x15, 0xb5, 0x18);
DEFINE_GUID(CLSID_LightBulb19, 
0x2cccf176, 0xf479, 0x4293, 0x89, 0x50, 0x2e, 0x68, 0x47, 0x15, 0xb5, 0x19);
DEFINE_GUID(CLSID_LightBulb20, 
0x2cccf176, 0xf479, 0x4293, 0x89, 0x50, 0x2e, 0x68, 0x47, 0x15, 0xb5, 0x20);

// {64C18F32-642E-420e-8AC6-047A226C7005}
DEFINE_GUID(CLSID_LightFire1, 
0x64c18f32, 0x642e, 0x420e, 0x8a, 0xc6, 0x4, 0x7a, 0x22, 0x6c, 0x70, 0x1);
DEFINE_GUID(CLSID_LightFire2, 
0x64c18f32, 0x642e, 0x420e, 0x8a, 0xc6, 0x4, 0x7a, 0x22, 0x6c, 0x70, 0x2);
DEFINE_GUID(CLSID_LightFire3, 
0x64c18f32, 0x642e, 0x420e, 0x8a, 0xc6, 0x4, 0x7a, 0x22, 0x6c, 0x70, 0x3);
DEFINE_GUID(CLSID_LightFire4, 
0x64c18f32, 0x642e, 0x420e, 0x8a, 0xc6, 0x4, 0x7a, 0x22, 0x6c, 0x70, 0x4);
DEFINE_GUID(CLSID_LightFire5, 
0x64c18f32, 0x642e, 0x420e, 0x8a, 0xc6, 0x4, 0x7a, 0x22, 0x6c, 0x70, 0x5);
DEFINE_GUID(CLSID_LightFire6, 
0x64c18f32, 0x642e, 0x420e, 0x8a, 0xc6, 0x4, 0x7a, 0x22, 0x6c, 0x70, 0x6);
DEFINE_GUID(CLSID_LightFire7, 
0x64c18f32, 0x642e, 0x420e, 0x8a, 0xc6, 0x4, 0x7a, 0x22, 0x6c, 0x70, 0x7);
DEFINE_GUID(CLSID_LightFire8, 
0x64c18f32, 0x642e, 0x420e, 0x8a, 0xc6, 0x4, 0x7a, 0x22, 0x6c, 0x70, 0x8);
DEFINE_GUID(CLSID_LightFire9, 
0x64c18f32, 0x642e, 0x420e, 0x8a, 0xc6, 0x4, 0x7a, 0x22, 0x6c, 0x70, 0x9);
DEFINE_GUID(CLSID_LightFire10, 
0x64c18f32, 0x642e, 0x420e, 0x8a, 0xc6, 0x4, 0x7a, 0x22, 0x6c, 0x70, 0x10);
DEFINE_GUID(CLSID_LightFire11, 
0x64c18f32, 0x642e, 0x420e, 0x8a, 0xc6, 0x4, 0x7a, 0x22, 0x6c, 0x70, 0x11);
DEFINE_GUID(CLSID_LightFire12, 
0x64c18f32, 0x642e, 0x420e, 0x8a, 0xc6, 0x4, 0x7a, 0x22, 0x6c, 0x70, 0x12);
DEFINE_GUID(CLSID_LightFire13, 
0x64c18f32, 0x642e, 0x420e, 0x8a, 0xc6, 0x4, 0x7a, 0x22, 0x6c, 0x70, 0x13);
DEFINE_GUID(CLSID_LightFire14, 
0x64c18f32, 0x642e, 0x420e, 0x8a, 0xc6, 0x4, 0x7a, 0x22, 0x6c, 0x70, 0x14);
DEFINE_GUID(CLSID_LightFire15, 
0x64c18f32, 0x642e, 0x420e, 0x8a, 0xc6, 0x4, 0x7a, 0x22, 0x6c, 0x70, 0x15);
DEFINE_GUID(CLSID_LightFire16, 
0x64c18f32, 0x642e, 0x420e, 0x8a, 0xc6, 0x4, 0x7a, 0x22, 0x6c, 0x70, 0x16);
DEFINE_GUID(CLSID_LightFire17, 
0x64c18f32, 0x642e, 0x420e, 0x8a, 0xc6, 0x4, 0x7a, 0x22, 0x6c, 0x70, 0x17);
DEFINE_GUID(CLSID_LightFire18, 
0x64c18f32, 0x642e, 0x420e, 0x8a, 0xc6, 0x4, 0x7a, 0x22, 0x6c, 0x70, 0x18);
DEFINE_GUID(CLSID_LightFire19, 
0x64c18f32, 0x642e, 0x420e, 0x8a, 0xc6, 0x4, 0x7a, 0x22, 0x6c, 0x70, 0x19);
DEFINE_GUID(CLSID_LightFire20, 
0x64c18f32, 0x642e, 0x420e, 0x8a, 0xc6, 0x4, 0x7a, 0x22, 0x6c, 0x70, 0x20);

// {918D30B2-B219-43cb-98E3-6EAB85CA5817}
DEFINE_GUID(CLSID_LightHanging1, 
0x918d30b2, 0xb219, 0x43cb, 0x98, 0xe3, 0x6e, 0xab, 0x85, 0xca, 0x58, 0x1);
DEFINE_GUID(CLSID_LightHanging2, 
0x918d30b2, 0xb219, 0x43cb, 0x98, 0xe3, 0x6e, 0xab, 0x85, 0xca, 0x58, 0x2);
DEFINE_GUID(CLSID_LightHanging3, 
0x918d30b2, 0xb219, 0x43cb, 0x98, 0xe3, 0x6e, 0xab, 0x85, 0xca, 0x58, 0x3);
DEFINE_GUID(CLSID_LightHanging4, 
0x918d30b2, 0xb219, 0x43cb, 0x98, 0xe3, 0x6e, 0xab, 0x85, 0xca, 0x58, 0x4);
DEFINE_GUID(CLSID_LightHanging5, 
0x918d30b2, 0xb219, 0x43cb, 0x98, 0xe3, 0x6e, 0xab, 0x85, 0xca, 0x58, 0x5);
DEFINE_GUID(CLSID_LightHanging6, 
0x918d30b2, 0xb219, 0x43cb, 0x98, 0xe3, 0x6e, 0xab, 0x85, 0xca, 0x58, 0x6);
DEFINE_GUID(CLSID_LightHanging7, 
0x918d30b2, 0xb219, 0x43cb, 0x98, 0xe3, 0x6e, 0xab, 0x85, 0xca, 0x58, 0x7);
DEFINE_GUID(CLSID_LightHanging8, 
0x918d30b2, 0xb219, 0x43cb, 0x98, 0xe3, 0x6e, 0xab, 0x85, 0xca, 0x58, 0x8);
DEFINE_GUID(CLSID_LightHanging9, 
0x918d30b2, 0xb219, 0x43cb, 0x98, 0xe3, 0x6e, 0xab, 0x85, 0xca, 0x58, 0x9);
DEFINE_GUID(CLSID_LightHanging10, 
0x918d30b2, 0xb219, 0x43cb, 0x98, 0xe3, 0x6e, 0xab, 0x85, 0xca, 0x58, 0x10);
DEFINE_GUID(CLSID_LightHanging11, 
0x918d30b2, 0xb219, 0x43cb, 0x98, 0xe3, 0x6e, 0xab, 0x85, 0xca, 0x58, 0x11);
DEFINE_GUID(CLSID_LightHanging12, 
0x918d30b2, 0xb219, 0x43cb, 0x98, 0xe3, 0x6e, 0xab, 0x85, 0xca, 0x58, 0x12);
DEFINE_GUID(CLSID_LightHanging13, 
0x918d30b2, 0xb219, 0x43cb, 0x98, 0xe3, 0x6e, 0xab, 0x85, 0xca, 0x58, 0x13);
DEFINE_GUID(CLSID_LightHanging14, 
0x918d30b2, 0xb219, 0x43cb, 0x98, 0xe3, 0x6e, 0xab, 0x85, 0xca, 0x58, 0x14);
DEFINE_GUID(CLSID_LightHanging15, 
0x918d30b2, 0xb219, 0x43cb, 0x98, 0xe3, 0x6e, 0xab, 0x85, 0xca, 0x58, 0x15);
DEFINE_GUID(CLSID_LightHanging16, 
0x918d30b2, 0xb219, 0x43cb, 0x98, 0xe3, 0x6e, 0xab, 0x85, 0xca, 0x58, 0x16);
DEFINE_GUID(CLSID_LightHanging17, 
0x918d30b2, 0xb219, 0x43cb, 0x98, 0xe3, 0x6e, 0xab, 0x85, 0xca, 0x58, 0x17);
DEFINE_GUID(CLSID_LightHanging18, 
0x918d30b2, 0xb219, 0x43cb, 0x98, 0xe3, 0x6e, 0xab, 0x85, 0xca, 0x58, 0x18);
DEFINE_GUID(CLSID_LightHanging19, 
0x918d30b2, 0xb219, 0x43cb, 0x98, 0xe3, 0x6e, 0xab, 0x85, 0xca, 0x58, 0x19);
DEFINE_GUID(CLSID_LightHanging20, 
0x918d30b2, 0xb219, 0x43cb, 0x98, 0xe3, 0x6e, 0xab, 0x85, 0xca, 0x58, 0x20);
DEFINE_GUID(CLSID_LightHanging21, 
0x918d30b2, 0xb219, 0x43cb, 0x98, 0xe3, 0x6e, 0xab, 0x85, 0xca, 0x58, 0x21);
DEFINE_GUID(CLSID_LightHanging22, 
0x918d30b2, 0xb219, 0x43cb, 0x98, 0xe3, 0x6e, 0xab, 0x85, 0xca, 0x58, 0x22);
DEFINE_GUID(CLSID_LightHanging23, 
0x918d30b2, 0xb219, 0x43cb, 0x98, 0xe3, 0x6e, 0xab, 0x85, 0xca, 0x58, 0x23);
DEFINE_GUID(CLSID_LightHanging24, 
0x918d30b2, 0xb219, 0x43cb, 0x98, 0xe3, 0x6e, 0xab, 0x85, 0xca, 0x58, 0x24);
DEFINE_GUID(CLSID_LightHanging25, 
0x918d30b2, 0xb219, 0x43cb, 0x98, 0xe3, 0x6e, 0xab, 0x85, 0xca, 0x58, 0x25);
DEFINE_GUID(CLSID_LightHanging26, 
0x918d30b2, 0xb219, 0x43cb, 0x98, 0xe3, 0x6e, 0xab, 0x85, 0xca, 0x58, 0x26);
DEFINE_GUID(CLSID_LightHanging27, 
0x918d30b2, 0xb219, 0x43cb, 0x98, 0xe3, 0x6e, 0xab, 0x85, 0xca, 0x58, 0x27);
DEFINE_GUID(CLSID_LightHanging28, 
0x918d30b2, 0xb219, 0x43cb, 0x98, 0xe3, 0x6e, 0xab, 0x85, 0xca, 0x58, 0x28);
DEFINE_GUID(CLSID_LightHanging29, 
0x918d30b2, 0xb219, 0x43cb, 0x98, 0xe3, 0x6e, 0xab, 0x85, 0xca, 0x58, 0x29);
DEFINE_GUID(CLSID_LightHanging30, 
0x918d30b2, 0xb219, 0x43cb, 0x98, 0xe3, 0x6e, 0xab, 0x85, 0xca, 0x58, 0x30);

// {4D1A73D3-4F2E-46e0-B915-628D12F7AA5B}
DEFINE_GUID(CLSID_LightCeiling1, 
0x4d1a73d3, 0x4f2e, 0x46e0, 0xb9, 0x15, 0x62, 0x8d, 0x12, 0xf7, 0xaa, 0x1);
DEFINE_GUID(CLSID_LightCeiling2, 
0x4d1a73d3, 0x4f2e, 0x46e0, 0xb9, 0x15, 0x62, 0x8d, 0x12, 0xf7, 0xaa, 0x2);
DEFINE_GUID(CLSID_LightCeiling3, 
0x4d1a73d3, 0x4f2e, 0x46e0, 0xb9, 0x15, 0x62, 0x8d, 0x12, 0xf7, 0xaa, 0x3);
DEFINE_GUID(CLSID_LightCeiling4, 
0x4d1a73d3, 0x4f2e, 0x46e0, 0xb9, 0x15, 0x62, 0x8d, 0x12, 0xf7, 0xaa, 0x4);
DEFINE_GUID(CLSID_LightCeiling5, 
0x4d1a73d3, 0x4f2e, 0x46e0, 0xb9, 0x15, 0x62, 0x8d, 0x12, 0xf7, 0xaa, 0x5);
DEFINE_GUID(CLSID_LightCeiling6, 
0x4d1a73d3, 0x4f2e, 0x46e0, 0xb9, 0x15, 0x62, 0x8d, 0x12, 0xf7, 0xaa, 0x6);
DEFINE_GUID(CLSID_LightCeiling7, 
0x4d1a73d3, 0x4f2e, 0x46e0, 0xb9, 0x15, 0x62, 0x8d, 0x12, 0xf7, 0xaa, 0x7);
DEFINE_GUID(CLSID_LightCeiling8, 
0x4d1a73d3, 0x4f2e, 0x46e0, 0xb9, 0x15, 0x62, 0x8d, 0x12, 0xf7, 0xaa, 0x8);
DEFINE_GUID(CLSID_LightCeiling9, 
0x4d1a73d3, 0x4f2e, 0x46e0, 0xb9, 0x15, 0x62, 0x8d, 0x12, 0xf7, 0xaa, 0x9);
DEFINE_GUID(CLSID_LightCeiling10, 
0x4d1a73d3, 0x4f2e, 0x46e0, 0xb9, 0x15, 0x62, 0x8d, 0x12, 0xf7, 0xaa, 0x10);
DEFINE_GUID(CLSID_LightCeiling11, 
0x4d1a73d3, 0x4f2e, 0x46e0, 0xb9, 0x15, 0x62, 0x8d, 0x12, 0xf7, 0xaa, 0x11);
DEFINE_GUID(CLSID_LightCeiling12, 
0x4d1a73d3, 0x4f2e, 0x46e0, 0xb9, 0x15, 0x62, 0x8d, 0x12, 0xf7, 0xaa, 0x12);
DEFINE_GUID(CLSID_LightCeiling13, 
0x4d1a73d3, 0x4f2e, 0x46e0, 0xb9, 0x15, 0x62, 0x8d, 0x12, 0xf7, 0xaa, 0x13);
DEFINE_GUID(CLSID_LightCeiling14, 
0x4d1a73d3, 0x4f2e, 0x46e0, 0xb9, 0x15, 0x62, 0x8d, 0x12, 0xf7, 0xaa, 0x14);
DEFINE_GUID(CLSID_LightCeiling15, 
0x4d1a73d3, 0x4f2e, 0x46e0, 0xb9, 0x15, 0x62, 0x8d, 0x12, 0xf7, 0xaa, 0x15);
DEFINE_GUID(CLSID_LightCeiling16, 
0x4d1a73d3, 0x4f2e, 0x46e0, 0xb9, 0x15, 0x62, 0x8d, 0x12, 0xf7, 0xaa, 0x16);
DEFINE_GUID(CLSID_LightCeiling17, 
0x4d1a73d3, 0x4f2e, 0x46e0, 0xb9, 0x15, 0x62, 0x8d, 0x12, 0xf7, 0xaa, 0x17);
DEFINE_GUID(CLSID_LightCeiling18, 
0x4d1a73d3, 0x4f2e, 0x46e0, 0xb9, 0x15, 0x62, 0x8d, 0x12, 0xf7, 0xaa, 0x18);
DEFINE_GUID(CLSID_LightCeiling19, 
0x4d1a73d3, 0x4f2e, 0x46e0, 0xb9, 0x15, 0x62, 0x8d, 0x12, 0xf7, 0xaa, 0x19);
DEFINE_GUID(CLSID_LightCeiling20, 
0x4d1a73d3, 0x4f2e, 0x46e0, 0xb9, 0x15, 0x62, 0x8d, 0x12, 0xf7, 0xaa, 0x20);
DEFINE_GUID(CLSID_LightCeiling21, 
0x4d1a73d3, 0x4f2e, 0x46e0, 0xb9, 0x15, 0x62, 0x8d, 0x12, 0xf7, 0xaa, 0x21);
DEFINE_GUID(CLSID_LightCeiling22, 
0x4d1a73d3, 0x4f2e, 0x46e0, 0xb9, 0x15, 0x62, 0x8d, 0x12, 0xf7, 0xaa, 0x22);
DEFINE_GUID(CLSID_LightCeiling23, 
0x4d1a73d3, 0x4f2e, 0x46e0, 0xb9, 0x15, 0x62, 0x8d, 0x12, 0xf7, 0xaa, 0x23);
DEFINE_GUID(CLSID_LightCeiling24, 
0x4d1a73d3, 0x4f2e, 0x46e0, 0xb9, 0x15, 0x62, 0x8d, 0x12, 0xf7, 0xaa, 0x24);
DEFINE_GUID(CLSID_LightCeiling25, 
0x4d1a73d3, 0x4f2e, 0x46e0, 0xb9, 0x15, 0x62, 0x8d, 0x12, 0xf7, 0xaa, 0x25);
DEFINE_GUID(CLSID_LightCeiling26, 
0x4d1a73d3, 0x4f2e, 0x46e0, 0xb9, 0x15, 0x62, 0x8d, 0x12, 0xf7, 0xaa, 0x26);
DEFINE_GUID(CLSID_LightCeiling27, 
0x4d1a73d3, 0x4f2e, 0x46e0, 0xb9, 0x15, 0x62, 0x8d, 0x12, 0xf7, 0xaa, 0x27);
DEFINE_GUID(CLSID_LightCeiling28, 
0x4d1a73d3, 0x4f2e, 0x46e0, 0xb9, 0x15, 0x62, 0x8d, 0x12, 0xf7, 0xaa, 0x28);
DEFINE_GUID(CLSID_LightCeiling29, 
0x4d1a73d3, 0x4f2e, 0x46e0, 0xb9, 0x15, 0x62, 0x8d, 0x12, 0xf7, 0xaa, 0x29);
DEFINE_GUID(CLSID_LightCeiling30, 
0x4d1a73d3, 0x4f2e, 0x46e0, 0xb9, 0x15, 0x62, 0x8d, 0x12, 0xf7, 0xaa, 0x30);
DEFINE_GUID(CLSID_LightCeiling31, 
0x4d1a73d3, 0x4f2e, 0x46e0, 0xb9, 0x15, 0x62, 0x8d, 0x12, 0xf7, 0xaa, 0x31);
DEFINE_GUID(CLSID_LightCeiling32, 
0x4d1a73d3, 0x4f2e, 0x46e0, 0xb9, 0x15, 0x62, 0x8d, 0x12, 0xf7, 0xaa, 0x32);
DEFINE_GUID(CLSID_LightCeiling33, 
0x4d1a73d3, 0x4f2e, 0x46e0, 0xb9, 0x15, 0x62, 0x8d, 0x12, 0xf7, 0xaa, 0x33);
DEFINE_GUID(CLSID_LightCeiling34, 
0x4d1a73d3, 0x4f2e, 0x46e0, 0xb9, 0x15, 0x62, 0x8d, 0x12, 0xf7, 0xaa, 0x34);
DEFINE_GUID(CLSID_LightCeiling35, 
0x4d1a73d3, 0x4f2e, 0x46e0, 0xb9, 0x15, 0x62, 0x8d, 0x12, 0xf7, 0xaa, 0x35);
DEFINE_GUID(CLSID_LightCeiling36, 
0x4d1a73d3, 0x4f2e, 0x46e0, 0xb9, 0x15, 0x62, 0x8d, 0x12, 0xf7, 0xaa, 0x36);
DEFINE_GUID(CLSID_LightCeiling37, 
0x4d1a73d3, 0x4f2e, 0x46e0, 0xb9, 0x15, 0x62, 0x8d, 0x12, 0xf7, 0xaa, 0x37);
DEFINE_GUID(CLSID_LightCeiling38, 
0x4d1a73d3, 0x4f2e, 0x46e0, 0xb9, 0x15, 0x62, 0x8d, 0x12, 0xf7, 0xaa, 0x38);
DEFINE_GUID(CLSID_LightCeiling39, 
0x4d1a73d3, 0x4f2e, 0x46e0, 0xb9, 0x15, 0x62, 0x8d, 0x12, 0xf7, 0xaa, 0x39);
DEFINE_GUID(CLSID_LightCeiling40, 
0x4d1a73d3, 0x4f2e, 0x46e0, 0xb9, 0x15, 0x62, 0x8d, 0x12, 0xf7, 0xaa, 0x40);

// {B3B80015-A5EE-47e4-A1DC-BA59CEEA81D7}
DEFINE_GUID(CLSID_LightWall1, 
0xb3b80015, 0xa5ee, 0x47e4, 0xa1, 0xdc, 0xba, 0x59, 0xce, 0xea, 0x81, 0x1);
DEFINE_GUID(CLSID_LightWall2, 
0xb3b80015, 0xa5ee, 0x47e4, 0xa1, 0xdc, 0xba, 0x59, 0xce, 0xea, 0x81, 0x2);
DEFINE_GUID(CLSID_LightWall3, 
0xb3b80015, 0xa5ee, 0x47e4, 0xa1, 0xdc, 0xba, 0x59, 0xce, 0xea, 0x81, 0x3);
DEFINE_GUID(CLSID_LightWall4, 
0xb3b80015, 0xa5ee, 0x47e4, 0xa1, 0xdc, 0xba, 0x59, 0xce, 0xea, 0x81, 0x4);
DEFINE_GUID(CLSID_LightWall5, 
0xb3b80015, 0xa5ee, 0x47e4, 0xa1, 0xdc, 0xba, 0x59, 0xce, 0xea, 0x81, 0x5);
DEFINE_GUID(CLSID_LightWall6, 
0xb3b80015, 0xa5ee, 0x47e4, 0xa1, 0xdc, 0xba, 0x59, 0xce, 0xea, 0x81, 0x6);
DEFINE_GUID(CLSID_LightWall7, 
0xb3b80015, 0xa5ee, 0x47e4, 0xa1, 0xdc, 0xba, 0x59, 0xce, 0xea, 0x81, 0x7);
DEFINE_GUID(CLSID_LightWall8, 
0xb3b80015, 0xa5ee, 0x47e4, 0xa1, 0xdc, 0xba, 0x59, 0xce, 0xea, 0x81, 0x8);
DEFINE_GUID(CLSID_LightWall9, 
0xb3b80015, 0xa5ee, 0x47e4, 0xa1, 0xdc, 0xba, 0x59, 0xce, 0xea, 0x81, 0x9);
DEFINE_GUID(CLSID_LightWall10, 
0xb3b80015, 0xa5ee, 0x47e4, 0xa1, 0xdc, 0xba, 0x59, 0xce, 0xea, 0x81, 0x10);
DEFINE_GUID(CLSID_LightWall11, 
0xb3b80015, 0xa5ee, 0x47e4, 0xa1, 0xdc, 0xba, 0x59, 0xce, 0xea, 0x81, 0x11);
DEFINE_GUID(CLSID_LightWall12, 
0xb3b80015, 0xa5ee, 0x47e4, 0xa1, 0xdc, 0xba, 0x59, 0xce, 0xea, 0x81, 0x12);
DEFINE_GUID(CLSID_LightWall13, 
0xb3b80015, 0xa5ee, 0x47e4, 0xa1, 0xdc, 0xba, 0x59, 0xce, 0xea, 0x81, 0x13);
DEFINE_GUID(CLSID_LightWall14, 
0xb3b80015, 0xa5ee, 0x47e4, 0xa1, 0xdc, 0xba, 0x59, 0xce, 0xea, 0x81, 0x14);
DEFINE_GUID(CLSID_LightWall15, 
0xb3b80015, 0xa5ee, 0x47e4, 0xa1, 0xdc, 0xba, 0x59, 0xce, 0xea, 0x81, 0x15);
DEFINE_GUID(CLSID_LightWall16, 
0xb3b80015, 0xa5ee, 0x47e4, 0xa1, 0xdc, 0xba, 0x59, 0xce, 0xea, 0x81, 0x16);
DEFINE_GUID(CLSID_LightWall17, 
0xb3b80015, 0xa5ee, 0x47e4, 0xa1, 0xdc, 0xba, 0x59, 0xce, 0xea, 0x81, 0x17);
DEFINE_GUID(CLSID_LightWall18, 
0xb3b80015, 0xa5ee, 0x47e4, 0xa1, 0xdc, 0xba, 0x59, 0xce, 0xea, 0x81, 0x18);
DEFINE_GUID(CLSID_LightWall19, 
0xb3b80015, 0xa5ee, 0x47e4, 0xa1, 0xdc, 0xba, 0x59, 0xce, 0xea, 0x81, 0x19);
DEFINE_GUID(CLSID_LightWall20, 
0xb3b80015, 0xa5ee, 0x47e4, 0xa1, 0xdc, 0xba, 0x59, 0xce, 0xea, 0x81, 0x20);
DEFINE_GUID(CLSID_LightWall21, 
0xb3b80015, 0xa5ee, 0x47e4, 0xa1, 0xdc, 0xba, 0x59, 0xce, 0xea, 0x81, 0x21);
DEFINE_GUID(CLSID_LightWall22, 
0xb3b80015, 0xa5ee, 0x47e4, 0xa1, 0xdc, 0xba, 0x59, 0xce, 0xea, 0x81, 0x22);
DEFINE_GUID(CLSID_LightWall23, 
0xb3b80015, 0xa5ee, 0x47e4, 0xa1, 0xdc, 0xba, 0x59, 0xce, 0xea, 0x81, 0x23);
DEFINE_GUID(CLSID_LightWall24, 
0xb3b80015, 0xa5ee, 0x47e4, 0xa1, 0xdc, 0xba, 0x59, 0xce, 0xea, 0x81, 0x24);
DEFINE_GUID(CLSID_LightWall25, 
0xb3b80015, 0xa5ee, 0x47e4, 0xa1, 0xdc, 0xba, 0x59, 0xce, 0xea, 0x81, 0x25);
DEFINE_GUID(CLSID_LightWall26, 
0xb3b80015, 0xa5ee, 0x47e4, 0xa1, 0xdc, 0xba, 0x59, 0xce, 0xea, 0x81, 0x26);
DEFINE_GUID(CLSID_LightWall27, 
0xb3b80015, 0xa5ee, 0x47e4, 0xa1, 0xdc, 0xba, 0x59, 0xce, 0xea, 0x81, 0x27);
DEFINE_GUID(CLSID_LightWall28, 
0xb3b80015, 0xa5ee, 0x47e4, 0xa1, 0xdc, 0xba, 0x59, 0xce, 0xea, 0x81, 0x28);
DEFINE_GUID(CLSID_LightWall29, 
0xb3b80015, 0xa5ee, 0x47e4, 0xa1, 0xdc, 0xba, 0x59, 0xce, 0xea, 0x81, 0x29);
DEFINE_GUID(CLSID_LightWall30, 
0xb3b80015, 0xa5ee, 0x47e4, 0xa1, 0xdc, 0xba, 0x59, 0xce, 0xea, 0x81, 0x30);
DEFINE_GUID(CLSID_LightWall31, 
0xb3b80015, 0xa5ee, 0x47e4, 0xa1, 0xdc, 0xba, 0x59, 0xce, 0xea, 0x81, 0x31);
DEFINE_GUID(CLSID_LightWall32, 
0xb3b80015, 0xa5ee, 0x47e4, 0xa1, 0xdc, 0xba, 0x59, 0xce, 0xea, 0x81, 0x32);
DEFINE_GUID(CLSID_LightWall33, 
0xb3b80015, 0xa5ee, 0x47e4, 0xa1, 0xdc, 0xba, 0x59, 0xce, 0xea, 0x81, 0x33);
DEFINE_GUID(CLSID_LightWall34, 
0xb3b80015, 0xa5ee, 0x47e4, 0xa1, 0xdc, 0xba, 0x59, 0xce, 0xea, 0x81, 0x34);
DEFINE_GUID(CLSID_LightWall35, 
0xb3b80015, 0xa5ee, 0x47e4, 0xa1, 0xdc, 0xba, 0x59, 0xce, 0xea, 0x81, 0x35);

// {9E3CC902-CE67-4686-9E21-E2BB1B4EBBF9}
DEFINE_GUID(CLSID_LightTable1, 
0x9e3cc902, 0xce67, 0x4686, 0x9e, 0x21, 0xe2, 0xbb, 0x1b, 0x4e, 0xbb, 0x1);
DEFINE_GUID(CLSID_LightTable2, 
0x9e3cc902, 0xce67, 0x4686, 0x9e, 0x21, 0xe2, 0xbb, 0x1b, 0x4e, 0xbb, 0x2);
DEFINE_GUID(CLSID_LightTable3, 
0x9e3cc902, 0xce67, 0x4686, 0x9e, 0x21, 0xe2, 0xbb, 0x1b, 0x4e, 0xbb, 0x3);
DEFINE_GUID(CLSID_LightTable4, 
0x9e3cc902, 0xce67, 0x4686, 0x9e, 0x21, 0xe2, 0xbb, 0x1b, 0x4e, 0xbb, 0x4);
DEFINE_GUID(CLSID_LightTable5, 
0x9e3cc902, 0xce67, 0x4686, 0x9e, 0x21, 0xe2, 0xbb, 0x1b, 0x4e, 0xbb, 0x5);
DEFINE_GUID(CLSID_LightTable6, 
0x9e3cc902, 0xce67, 0x4686, 0x9e, 0x21, 0xe2, 0xbb, 0x1b, 0x4e, 0xbb, 0x6);
DEFINE_GUID(CLSID_LightTable7, 
0x9e3cc902, 0xce67, 0x4686, 0x9e, 0x21, 0xe2, 0xbb, 0x1b, 0x4e, 0xbb, 0x7);
DEFINE_GUID(CLSID_LightTable8, 
0x9e3cc902, 0xce67, 0x4686, 0x9e, 0x21, 0xe2, 0xbb, 0x1b, 0x4e, 0xbb, 0x8);
DEFINE_GUID(CLSID_LightTable9, 
0x9e3cc902, 0xce67, 0x4686, 0x9e, 0x21, 0xe2, 0xbb, 0x1b, 0x4e, 0xbb, 0x9);
DEFINE_GUID(CLSID_LightTable10, 
0x9e3cc902, 0xce67, 0x4686, 0x9e, 0x21, 0xe2, 0xbb, 0x1b, 0x4e, 0xbb, 0x10);
DEFINE_GUID(CLSID_LightTable11, 
0x9e3cc902, 0xce67, 0x4686, 0x9e, 0x21, 0xe2, 0xbb, 0x1b, 0x4e, 0xbb, 0x11);
DEFINE_GUID(CLSID_LightTable12, 
0x9e3cc902, 0xce67, 0x4686, 0x9e, 0x21, 0xe2, 0xbb, 0x1b, 0x4e, 0xbb, 0x12);
DEFINE_GUID(CLSID_LightTable13, 
0x9e3cc902, 0xce67, 0x4686, 0x9e, 0x21, 0xe2, 0xbb, 0x1b, 0x4e, 0xbb, 0x13);
DEFINE_GUID(CLSID_LightTable14, 
0x9e3cc902, 0xce67, 0x4686, 0x9e, 0x21, 0xe2, 0xbb, 0x1b, 0x4e, 0xbb, 0x14);
DEFINE_GUID(CLSID_LightTable15, 
0x9e3cc902, 0xce67, 0x4686, 0x9e, 0x21, 0xe2, 0xbb, 0x1b, 0x4e, 0xbb, 0x15);
DEFINE_GUID(CLSID_LightTable16, 
0x9e3cc902, 0xce67, 0x4686, 0x9e, 0x21, 0xe2, 0xbb, 0x1b, 0x4e, 0xbb, 0x16);
DEFINE_GUID(CLSID_LightTable17, 
0x9e3cc902, 0xce67, 0x4686, 0x9e, 0x21, 0xe2, 0xbb, 0x1b, 0x4e, 0xbb, 0x17);
DEFINE_GUID(CLSID_LightTable18, 
0x9e3cc902, 0xce67, 0x4686, 0x9e, 0x21, 0xe2, 0xbb, 0x1b, 0x4e, 0xbb, 0x18);
DEFINE_GUID(CLSID_LightTable19, 
0x9e3cc902, 0xce67, 0x4686, 0x9e, 0x21, 0xe2, 0xbb, 0x1b, 0x4e, 0xbb, 0x19);
DEFINE_GUID(CLSID_LightTable20, 
0x9e3cc902, 0xce67, 0x4686, 0x9e, 0x21, 0xe2, 0xbb, 0x1b, 0x4e, 0xbb, 0x20);
DEFINE_GUID(CLSID_LightTable21, 
0x9e3cc902, 0xce67, 0x4686, 0x9e, 0x21, 0xe2, 0xbb, 0x1b, 0x4e, 0xbb, 0x21);
DEFINE_GUID(CLSID_LightTable22, 
0x9e3cc902, 0xce67, 0x4686, 0x9e, 0x21, 0xe2, 0xbb, 0x1b, 0x4e, 0xbb, 0x22);
DEFINE_GUID(CLSID_LightTable23, 
0x9e3cc902, 0xce67, 0x4686, 0x9e, 0x21, 0xe2, 0xbb, 0x1b, 0x4e, 0xbb, 0x23);
DEFINE_GUID(CLSID_LightTable24, 
0x9e3cc902, 0xce67, 0x4686, 0x9e, 0x21, 0xe2, 0xbb, 0x1b, 0x4e, 0xbb, 0x24);
DEFINE_GUID(CLSID_LightTable25, 
0x9e3cc902, 0xce67, 0x4686, 0x9e, 0x21, 0xe2, 0xbb, 0x1b, 0x4e, 0xbb, 0x25);
DEFINE_GUID(CLSID_LightTable26, 
0x9e3cc902, 0xce67, 0x4686, 0x9e, 0x21, 0xe2, 0xbb, 0x1b, 0x4e, 0xbb, 0x26);
DEFINE_GUID(CLSID_LightTable27, 
0x9e3cc902, 0xce67, 0x4686, 0x9e, 0x21, 0xe2, 0xbb, 0x1b, 0x4e, 0xbb, 0x27);
DEFINE_GUID(CLSID_LightTable28, 
0x9e3cc902, 0xce67, 0x4686, 0x9e, 0x21, 0xe2, 0xbb, 0x1b, 0x4e, 0xbb, 0x28);
DEFINE_GUID(CLSID_LightTable29, 
0x9e3cc902, 0xce67, 0x4686, 0x9e, 0x21, 0xe2, 0xbb, 0x1b, 0x4e, 0xbb, 0x29);
DEFINE_GUID(CLSID_LightTable30, 
0x9e3cc902, 0xce67, 0x4686, 0x9e, 0x21, 0xe2, 0xbb, 0x1b, 0x4e, 0xbb, 0x30);

// {610B0E27-5DA1-48fe-84A6-402D32299DD0}
DEFINE_GUID(CLSID_LightFloor1, 
0x610b0e27, 0x5da1, 0x48fe, 0x84, 0xa6, 0x40, 0x2d, 0x32, 0x29, 0x9d, 0x1);
DEFINE_GUID(CLSID_LightFloor2, 
0x610b0e27, 0x5da1, 0x48fe, 0x84, 0xa6, 0x40, 0x2d, 0x32, 0x29, 0x9d, 0x2);
DEFINE_GUID(CLSID_LightFloor3, 
0x610b0e27, 0x5da1, 0x48fe, 0x84, 0xa6, 0x40, 0x2d, 0x32, 0x29, 0x9d, 0x3);
DEFINE_GUID(CLSID_LightFloor4, 
0x610b0e27, 0x5da1, 0x48fe, 0x84, 0xa6, 0x40, 0x2d, 0x32, 0x29, 0x9d, 0x4);
DEFINE_GUID(CLSID_LightFloor5, 
0x610b0e27, 0x5da1, 0x48fe, 0x84, 0xa6, 0x40, 0x2d, 0x32, 0x29, 0x9d, 0x5);
DEFINE_GUID(CLSID_LightFloor6, 
0x610b0e27, 0x5da1, 0x48fe, 0x84, 0xa6, 0x40, 0x2d, 0x32, 0x29, 0x9d, 0x6);
DEFINE_GUID(CLSID_LightFloor7, 
0x610b0e27, 0x5da1, 0x48fe, 0x84, 0xa6, 0x40, 0x2d, 0x32, 0x29, 0x9d, 0x7);
DEFINE_GUID(CLSID_LightFloor8, 
0x610b0e27, 0x5da1, 0x48fe, 0x84, 0xa6, 0x40, 0x2d, 0x32, 0x29, 0x9d, 0x8);
DEFINE_GUID(CLSID_LightFloor9, 
0x610b0e27, 0x5da1, 0x48fe, 0x84, 0xa6, 0x40, 0x2d, 0x32, 0x29, 0x9d, 0x9);
DEFINE_GUID(CLSID_LightFloor10, 
0x610b0e27, 0x5da1, 0x48fe, 0x84, 0xa6, 0x40, 0x2d, 0x32, 0x29, 0x9d, 0x10);
DEFINE_GUID(CLSID_LightFloor11, 
0x610b0e27, 0x5da1, 0x48fe, 0x84, 0xa6, 0x40, 0x2d, 0x32, 0x29, 0x9d, 0x11);
DEFINE_GUID(CLSID_LightFloor12, 
0x610b0e27, 0x5da1, 0x48fe, 0x84, 0xa6, 0x40, 0x2d, 0x32, 0x29, 0x9d, 0x12);
DEFINE_GUID(CLSID_LightFloor13, 
0x610b0e27, 0x5da1, 0x48fe, 0x84, 0xa6, 0x40, 0x2d, 0x32, 0x29, 0x9d, 0x13);
DEFINE_GUID(CLSID_LightFloor14, 
0x610b0e27, 0x5da1, 0x48fe, 0x84, 0xa6, 0x40, 0x2d, 0x32, 0x29, 0x9d, 0x14);
DEFINE_GUID(CLSID_LightFloor15, 
0x610b0e27, 0x5da1, 0x48fe, 0x84, 0xa6, 0x40, 0x2d, 0x32, 0x29, 0x9d, 0x15);
DEFINE_GUID(CLSID_LightFloor16, 
0x610b0e27, 0x5da1, 0x48fe, 0x84, 0xa6, 0x40, 0x2d, 0x32, 0x29, 0x9d, 0x16);
DEFINE_GUID(CLSID_LightFloor17, 
0x610b0e27, 0x5da1, 0x48fe, 0x84, 0xa6, 0x40, 0x2d, 0x32, 0x29, 0x9d, 0x17);
DEFINE_GUID(CLSID_LightFloor18, 
0x610b0e27, 0x5da1, 0x48fe, 0x84, 0xa6, 0x40, 0x2d, 0x32, 0x29, 0x9d, 0x18);
DEFINE_GUID(CLSID_LightFloor19, 
0x610b0e27, 0x5da1, 0x48fe, 0x84, 0xa6, 0x40, 0x2d, 0x32, 0x29, 0x9d, 0x19);
DEFINE_GUID(CLSID_LightFloor20, 
0x610b0e27, 0x5da1, 0x48fe, 0x84, 0xa6, 0x40, 0x2d, 0x32, 0x29, 0x9d, 0x20);
DEFINE_GUID(CLSID_LightFloor21, 
0x610b0e27, 0x5da1, 0x48fe, 0x84, 0xa6, 0x40, 0x2d, 0x32, 0x29, 0x9d, 0x21);
DEFINE_GUID(CLSID_LightFloor22, 
0x610b0e27, 0x5da1, 0x48fe, 0x84, 0xa6, 0x40, 0x2d, 0x32, 0x29, 0x9d, 0x22);
DEFINE_GUID(CLSID_LightFloor23, 
0x610b0e27, 0x5da1, 0x48fe, 0x84, 0xa6, 0x40, 0x2d, 0x32, 0x29, 0x9d, 0x23);
DEFINE_GUID(CLSID_LightFloor24, 
0x610b0e27, 0x5da1, 0x48fe, 0x84, 0xa6, 0x40, 0x2d, 0x32, 0x29, 0x9d, 0x24);
DEFINE_GUID(CLSID_LightFloor25, 
0x610b0e27, 0x5da1, 0x48fe, 0x84, 0xa6, 0x40, 0x2d, 0x32, 0x29, 0x9d, 0x25);
DEFINE_GUID(CLSID_LightFloor26, 
0x610b0e27, 0x5da1, 0x48fe, 0x84, 0xa6, 0x40, 0x2d, 0x32, 0x29, 0x9d, 0x26);
DEFINE_GUID(CLSID_LightFloor27, 
0x610b0e27, 0x5da1, 0x48fe, 0x84, 0xa6, 0x40, 0x2d, 0x32, 0x29, 0x9d, 0x27);
DEFINE_GUID(CLSID_LightFloor28, 
0x610b0e27, 0x5da1, 0x48fe, 0x84, 0xa6, 0x40, 0x2d, 0x32, 0x29, 0x9d, 0x28);
DEFINE_GUID(CLSID_LightFloor29, 
0x610b0e27, 0x5da1, 0x48fe, 0x84, 0xa6, 0x40, 0x2d, 0x32, 0x29, 0x9d, 0x29);
DEFINE_GUID(CLSID_LightFloor30, 
0x610b0e27, 0x5da1, 0x48fe, 0x84, 0xa6, 0x40, 0x2d, 0x32, 0x29, 0x9d, 0x30);

// {90211A40-B618-4c6d-9D90-1B6AC8BAAEE0}
DEFINE_GUID(CLSID_LightDesk1, 
0x90211a40, 0xb618, 0x4c6d, 0x9d, 0x90, 0x1b, 0x6a, 0xc8, 0xba, 0xae, 0x1);
DEFINE_GUID(CLSID_LightDesk2, 
0x90211a40, 0xb618, 0x4c6d, 0x9d, 0x90, 0x1b, 0x6a, 0xc8, 0xba, 0xae, 0x2);
DEFINE_GUID(CLSID_LightDesk3, 
0x90211a40, 0xb618, 0x4c6d, 0x9d, 0x90, 0x1b, 0x6a, 0xc8, 0xba, 0xae, 0x3);
DEFINE_GUID(CLSID_LightDesk4, 
0x90211a40, 0xb618, 0x4c6d, 0x9d, 0x90, 0x1b, 0x6a, 0xc8, 0xba, 0xae, 0x4);
DEFINE_GUID(CLSID_LightDesk5, 
0x90211a40, 0xb618, 0x4c6d, 0x9d, 0x90, 0x1b, 0x6a, 0xc8, 0xba, 0xae, 0x5);
DEFINE_GUID(CLSID_LightDesk6, 
0x90211a40, 0xb618, 0x4c6d, 0x9d, 0x90, 0x1b, 0x6a, 0xc8, 0xba, 0xae, 0x6);
DEFINE_GUID(CLSID_LightDesk7, 
0x90211a40, 0xb618, 0x4c6d, 0x9d, 0x90, 0x1b, 0x6a, 0xc8, 0xba, 0xae, 0x7);
DEFINE_GUID(CLSID_LightDesk8, 
0x90211a40, 0xb618, 0x4c6d, 0x9d, 0x90, 0x1b, 0x6a, 0xc8, 0xba, 0xae, 0x8);
DEFINE_GUID(CLSID_LightDesk9, 
0x90211a40, 0xb618, 0x4c6d, 0x9d, 0x90, 0x1b, 0x6a, 0xc8, 0xba, 0xae, 0x9);
DEFINE_GUID(CLSID_LightDesk10, 
0x90211a40, 0xb618, 0x4c6d, 0x9d, 0x90, 0x1b, 0x6a, 0xc8, 0xba, 0xae, 0x10);
DEFINE_GUID(CLSID_LightDesk11, 
0x90211a40, 0xb618, 0x4c6d, 0x9d, 0x90, 0x1b, 0x6a, 0xc8, 0xba, 0xae, 0x11);
DEFINE_GUID(CLSID_LightDesk12, 
0x90211a40, 0xb618, 0x4c6d, 0x9d, 0x90, 0x1b, 0x6a, 0xc8, 0xba, 0xae, 0x12);
DEFINE_GUID(CLSID_LightDesk13, 
0x90211a40, 0xb618, 0x4c6d, 0x9d, 0x90, 0x1b, 0x6a, 0xc8, 0xba, 0xae, 0x13);
DEFINE_GUID(CLSID_LightDesk14, 
0x90211a40, 0xb618, 0x4c6d, 0x9d, 0x90, 0x1b, 0x6a, 0xc8, 0xba, 0xae, 0x14);
DEFINE_GUID(CLSID_LightDesk15, 
0x90211a40, 0xb618, 0x4c6d, 0x9d, 0x90, 0x1b, 0x6a, 0xc8, 0xba, 0xae, 0x15);
DEFINE_GUID(CLSID_LightDesk16, 
0x90211a40, 0xb618, 0x4c6d, 0x9d, 0x90, 0x1b, 0x6a, 0xc8, 0xba, 0xae, 0x16);
DEFINE_GUID(CLSID_LightDesk17, 
0x90211a40, 0xb618, 0x4c6d, 0x9d, 0x90, 0x1b, 0x6a, 0xc8, 0xba, 0xae, 0x17);
DEFINE_GUID(CLSID_LightDesk18, 
0x90211a40, 0xb618, 0x4c6d, 0x9d, 0x90, 0x1b, 0x6a, 0xc8, 0xba, 0xae, 0x18);
DEFINE_GUID(CLSID_LightDesk19, 
0x90211a40, 0xb618, 0x4c6d, 0x9d, 0x90, 0x1b, 0x6a, 0xc8, 0xba, 0xae, 0x19);
DEFINE_GUID(CLSID_LightDesk20, 
0x90211a40, 0xb618, 0x4c6d, 0x9d, 0x90, 0x1b, 0x6a, 0xc8, 0xba, 0xae, 0x20);
DEFINE_GUID(CLSID_LightDesk21, 
0x90211a40, 0xb618, 0x4c6d, 0x9d, 0x90, 0x1b, 0x6a, 0xc8, 0xba, 0xae, 0x21);
DEFINE_GUID(CLSID_LightDesk22, 
0x90211a40, 0xb618, 0x4c6d, 0x9d, 0x90, 0x1b, 0x6a, 0xc8, 0xba, 0xae, 0x22);
DEFINE_GUID(CLSID_LightDesk23, 
0x90211a40, 0xb618, 0x4c6d, 0x9d, 0x90, 0x1b, 0x6a, 0xc8, 0xba, 0xae, 0x23);
DEFINE_GUID(CLSID_LightDesk24, 
0x90211a40, 0xb618, 0x4c6d, 0x9d, 0x90, 0x1b, 0x6a, 0xc8, 0xba, 0xae, 0x24);
DEFINE_GUID(CLSID_LightDesk25, 
0x90211a40, 0xb618, 0x4c6d, 0x9d, 0x90, 0x1b, 0x6a, 0xc8, 0xba, 0xae, 0x25);
DEFINE_GUID(CLSID_LightDesk26, 
0x90211a40, 0xb618, 0x4c6d, 0x9d, 0x90, 0x1b, 0x6a, 0xc8, 0xba, 0xae, 0x26);
DEFINE_GUID(CLSID_LightDesk27, 
0x90211a40, 0xb618, 0x4c6d, 0x9d, 0x90, 0x1b, 0x6a, 0xc8, 0xba, 0xae, 0x27);
DEFINE_GUID(CLSID_LightDesk28, 
0x90211a40, 0xb618, 0x4c6d, 0x9d, 0x90, 0x1b, 0x6a, 0xc8, 0xba, 0xae, 0x28);
DEFINE_GUID(CLSID_LightDesk29, 
0x90211a40, 0xb618, 0x4c6d, 0x9d, 0x90, 0x1b, 0x6a, 0xc8, 0xba, 0xae, 0x29);
DEFINE_GUID(CLSID_LightDesk30, 
0x90211a40, 0xb618, 0x4c6d, 0x9d, 0x90, 0x1b, 0x6a, 0xc8, 0xba, 0xae, 0x30);

// {B212330C-D895-464c-B2FF-C645B14464C2}
DEFINE_GUID(CLSID_LightOutdoor1,
0xb212330c, 0xd895, 0x464c, 0xb2, 0xff, 0xc6, 0x45, 0xb1, 0x44, 0x64, 0x1);
DEFINE_GUID(CLSID_LightOutdoor2,
0xb212330c, 0xd895, 0x464c, 0xb2, 0xff, 0xc6, 0x45, 0xb1, 0x44, 0x64, 0x2);
DEFINE_GUID(CLSID_LightOutdoor3,
0xb212330c, 0xd895, 0x464c, 0xb2, 0xff, 0xc6, 0x45, 0xb1, 0x44, 0x64, 0x3);
DEFINE_GUID(CLSID_LightOutdoor4,
0xb212330c, 0xd895, 0x464c, 0xb2, 0xff, 0xc6, 0x45, 0xb1, 0x44, 0x64, 0x4);
DEFINE_GUID(CLSID_LightOutdoor5,
0xb212330c, 0xd895, 0x464c, 0xb2, 0xff, 0xc6, 0x45, 0xb1, 0x44, 0x64, 0x5);
DEFINE_GUID(CLSID_LightOutdoor6,
0xb212330c, 0xd895, 0x464c, 0xb2, 0xff, 0xc6, 0x45, 0xb1, 0x44, 0x64, 0x6);
DEFINE_GUID(CLSID_LightOutdoor7,
0xb212330c, 0xd895, 0x464c, 0xb2, 0xff, 0xc6, 0x45, 0xb1, 0x44, 0x64, 0x7);
DEFINE_GUID(CLSID_LightOutdoor8,
0xb212330c, 0xd895, 0x464c, 0xb2, 0xff, 0xc6, 0x45, 0xb1, 0x44, 0x64, 0x8);
DEFINE_GUID(CLSID_LightOutdoor9,
0xb212330c, 0xd895, 0x464c, 0xb2, 0xff, 0xc6, 0x45, 0xb1, 0x44, 0x64, 0x9);
DEFINE_GUID(CLSID_LightOutdoor10,
0xb212330c, 0xd895, 0x464c, 0xb2, 0xff, 0xc6, 0x45, 0xb1, 0x44, 0x64, 0x10);
DEFINE_GUID(CLSID_LightOutdoor11,
0xb212330c, 0xd895, 0x464c, 0xb2, 0xff, 0xc6, 0x45, 0xb1, 0x44, 0x64, 0x11);
DEFINE_GUID(CLSID_LightOutdoor12,
0xb212330c, 0xd895, 0x464c, 0xb2, 0xff, 0xc6, 0x45, 0xb1, 0x44, 0x64, 0x12);
DEFINE_GUID(CLSID_LightOutdoor13,
0xb212330c, 0xd895, 0x464c, 0xb2, 0xff, 0xc6, 0x45, 0xb1, 0x44, 0x64, 0x13);
DEFINE_GUID(CLSID_LightOutdoor14,
0xb212330c, 0xd895, 0x464c, 0xb2, 0xff, 0xc6, 0x45, 0xb1, 0x44, 0x64, 0x14);
DEFINE_GUID(CLSID_LightOutdoor15,
0xb212330c, 0xd895, 0x464c, 0xb2, 0xff, 0xc6, 0x45, 0xb1, 0x44, 0x64, 0x15);
DEFINE_GUID(CLSID_LightOutdoor16,
0xb212330c, 0xd895, 0x464c, 0xb2, 0xff, 0xc6, 0x45, 0xb1, 0x44, 0x64, 0x16);
DEFINE_GUID(CLSID_LightOutdoor17,
0xb212330c, 0xd895, 0x464c, 0xb2, 0xff, 0xc6, 0x45, 0xb1, 0x44, 0x64, 0x17);
DEFINE_GUID(CLSID_LightOutdoor18,
0xb212330c, 0xd895, 0x464c, 0xb2, 0xff, 0xc6, 0x45, 0xb1, 0x44, 0x64, 0x18);
DEFINE_GUID(CLSID_LightOutdoor19,
0xb212330c, 0xd895, 0x464c, 0xb2, 0xff, 0xc6, 0x45, 0xb1, 0x44, 0x64, 0x19);
DEFINE_GUID(CLSID_LightOutdoor20,
0xb212330c, 0xd895, 0x464c, 0xb2, 0xff, 0xc6, 0x45, 0xb1, 0x44, 0x64, 0x20);
DEFINE_GUID(CLSID_LightOutdoor21,
0xb212330c, 0xd895, 0x464c, 0xb2, 0xff, 0xc6, 0x45, 0xb1, 0x44, 0x64, 0x21);
DEFINE_GUID(CLSID_LightOutdoor22,
0xb212330c, 0xd895, 0x464c, 0xb2, 0xff, 0xc6, 0x45, 0xb1, 0x44, 0x64, 0x22);
DEFINE_GUID(CLSID_LightOutdoor23,
0xb212330c, 0xd895, 0x464c, 0xb2, 0xff, 0xc6, 0x45, 0xb1, 0x44, 0x64, 0x23);
DEFINE_GUID(CLSID_LightOutdoor24,
0xb212330c, 0xd895, 0x464c, 0xb2, 0xff, 0xc6, 0x45, 0xb1, 0x44, 0x64, 0x24);
DEFINE_GUID(CLSID_LightOutdoor25,
0xb212330c, 0xd895, 0x464c, 0xb2, 0xff, 0xc6, 0x45, 0xb1, 0x44, 0x64, 0x25);

class CLightShade;
typedef CLightShade *PCLightShade;
class CLightStand;
typedef CLightStand *PCLightStand;

class DLLEXPORT CObjectLight : public CObjectTemplate {
public:
   CObjectLight (PVOID pParams, POSINFO pInfo);
   ~CObjectLight (void);

   virtual void Delete (void);
   virtual void Render (POBJECTRENDER pr, DWORD dwSubObject);
   virtual CObjectSocket *Clone (void);
   virtual PCMMLNode2 MMLTo (void);
   virtual BOOL MMLFrom (PCMMLNode2 pNode);
   virtual BOOL MoveReferencePointQuery (DWORD dwIndex, PCPoint pp);
   virtual BOOL MoveReferenceStringQuery (DWORD dwIndex, PWSTR psz, DWORD dwSize, DWORD *pdwNeeded);
   virtual BOOL LightQuery (PCListFixed pl, DWORD dwShow);
   virtual fp TurnOnGet (void);
   virtual BOOL TurnOnSet (fp fOpen);
   virtual BOOL ControlPointQuery (DWORD dwID, POSCONTROL pInfo);
   virtual BOOL ControlPointSet (DWORD dwID, PCPoint pVal, PCPoint pViewer);
   virtual void ControlPointEnum (PCListFixed plDWORD);
   virtual BOOL EmbedDoCutout (void);
   virtual BOOL ObjectMatrixSet (CMatrix *pObject);
   virtual BOOL DialogQuery (void);
   virtual BOOL DialogShow (DWORD dwSurface, PCEscWindow pWindow, PCPoint pClick);

//private:
   void CreateSurfaces (void);

   fp          m_fLightOn;       // amount that light is on, 0 = off, 1.0 = on all the way
   PCLightShade m_pLightShade;   // light shade
   PCLightStand m_pLightStand;   // light post

   CRenderSurface    m_Renderrs;       // for malloc optimization
};
typedef CObjectLight *PCObjectLight;


/************************************************************************************
CObjectLightGeneric */

DEFINE_GUID(CLSID_LightGeneric, 
0x1f8d241c, 0xcf42, 0x9c25, 0x9e, 0xb2, 0x69, 0xbe, 0x70, 0xbc, 0xdd, 0x1a);
DEFINE_GUID(CLSID_LightGenericSub,
0xd20, 0xc0da, 0x285b, 0x74, 0x7a, 0x80, 0xbe, 0xd3, 0xb, 0xc4, 0x1);

class DLLEXPORT CObjectLightGeneric : public CObjectTemplate {
public:
   CObjectLightGeneric (PVOID pParams, POSINFO pInfo);
   ~CObjectLightGeneric (void);

   virtual void Delete (void);
   virtual void Render (POBJECTRENDER pr, DWORD dwSubObject);
   virtual CObjectSocket *Clone (void);
   virtual PCMMLNode2 MMLTo (void);
   virtual BOOL MMLFrom (PCMMLNode2 pNode);
   virtual BOOL LightQuery (PCListFixed pl, DWORD dwShow);
   virtual fp TurnOnGet (void);
   virtual BOOL TurnOnSet (fp fOpen);
   virtual BOOL DialogQuery (void);
   virtual BOOL DialogShow (DWORD dwSurface, PCEscWindow pWindow, PCPoint pClick);
   virtual BOOL MoveReferencePointQuery (DWORD dwIndex, PCPoint pp);
   virtual BOOL MoveReferenceStringQuery (DWORD dwIndex, PWSTR psz, DWORD dwSize, DWORD *pdwNeeded);
   virtual void QueryBoundingBox (PCPoint pCorner1, PCPoint pCorner2, DWORD dwSubObject);

//private:

   fp          m_fLightOn;       // amount that light is on, 0 = off, 1.0 = on all the way
   LIGHTINFO   m_li;             // light infro. pLoc and pDir ignored

   CRevolution m_Revolution;     // for bulb

   CRenderSurface    m_Renderrs;    // to minimize mallocs
};
typedef CObjectLightGeneric *PCObjectLightGeneric;



/************************************************************************************
CLibrary */
// {17D70BF5-B6D0-4ba5-AE41-125DAC9D6FDF}
DEFINE_GUID(GUID_LibraryHeaderOld, 
0x17d70bf5, 0xb6d0, 0x4ba5, 0xae, 0x41, 0x12, 0x5d, 0xac, 0x9d, 0x6f, 0xdf);

// {10913B9B-9F4B-4889-A5DA-48A715D68E47}
DEFINE_GUID(GUID_LibraryHeaderNew, 
0x10913b9b, 0x9f4b, 0x4889, 0xa5, 0xda, 0x48, 0xa7, 0x15, 0xd6, 0x8e, 0x47);

// {10913B9B-9F4B-4889-A5DA-48A715D68E47}
DEFINE_GUID(GUID_LibraryHeaderNewer, 
0x10913b9c, 0x9f4b, 0x4889, 0xa5, 0xda, 0x48, 0xa7, 0x15, 0xd6, 0x8e, 0x47);

class CLibrary;
class CCatItem;
typedef CCatItem *PCCatItem;

DLLEXPORT PWSTR MMLCompressed (void);

class DLLEXPORT CLibraryCategory {
   friend class CLibrary;

public:
   ESCNEWDELETE;

   CLibraryCategory(CLibrary *pLibrary);
   ~CLibraryCategory();

   BOOL ItemAdd (PWSTR pszMajor, PWSTR pszMinor, PWSTR pszName, const GUID *pgCode, const GUID *pgSub, PCMMLNode2 pNode);
   PCMMLNode2 ItemGet (PWSTR pszMajor, PWSTR pszMinor, PWSTR pszName);
   PCMMLNode2 ItemGet (const GUID *pgCode, const GUID *pgSub);
   BOOL ItemExists (PWSTR pszMajor, PWSTR pszMinor, PWSTR pszName);
   void ItemDirty (PWSTR pszMajor, PWSTR pszMinor, PWSTR pszName, BOOL fDirty = TRUE);
   void ItemDirty (const GUID *pgCode, const GUID *pgSub, BOOL fDirty = TRUE);
   BOOL ItemRename (PWSTR pszOrigMajor, PWSTR pszOrigMinor, PWSTR pszOrigName, PWSTR pszMajor, PWSTR pszMinor, PWSTR pszName);
   BOOL ItemRemove (PWSTR pszMajor, PWSTR pszMinor, PWSTR pszName);
   BOOL ItemNameFromGUID (const GUID *pgCode, const GUID *pgSub, PWSTR pszMajor, PWSTR pszMinor, PWSTR pszName);
   BOOL ItemGUIDFromName (PWSTR pszMajor, PWSTR pszMinor, PWSTR pszName, GUID *pgCode, GUID *pgSub);
   void EnumMajor (PCListFixed pl);
   void EnumMinor (PCListFixed pl, PWSTR pszMajor);
   void EnumItems (PCListFixed pl, PWSTR pszMajor, PWSTR pszMinor);
   BOOL Commit (void);
   CLibraryCategory *Clone (CLibrary *pLibrary);

private:
   PCMMLNode2 MMLTo (void);    // writes entire category to MML
   BOOL MMLFrom (PCMMLNode2 pNode, DWORD *pdwNum = NULL);  // reads entire category from MML. NOTE - Deletes pNode itself
   PCBTree FindMajor (PWSTR pszMajor, BOOL fCreateIfNotExist = FALSE);  // fiven major cateogory, finds treee that contains the minor
   PCBTree FindMinor (PWSTR pszMajor, PWSTR pszMinor, BOOL fCreateIfNotExist = FALSE); // find tree of sub-category names
   BOOL IntItemRemoveButDontDelete (PWSTR pszMajor, PWSTR pszMinor, PWSTR pszName, BOOL fDelete);
   PCCatItem IntItemGet (PWSTR pszMajor, PWSTR pszMinor, PWSTR pszName);

   CLibrary*   m_pLibrary;    // library to which this belongs - sets dirty flag
   CBTree      m_treeItems;   // tree of main-category names, data is PCBTree of sub-category names,
                              // that tree in turn contains another PCBTree of items names with contents of
                              // CATITEM
};
typedef CLibraryCategory *PCLibraryCategory;

class DLLEXPORT CLibrary {
   friend class CLibraryCategory;
public:
   ESCNEWDELETE;

   CLibrary (void);
   ~CLibrary (void);

   BOOL Init (PWSTR pszFile, PWSTR pszIndex, PCProgressSocket pProgress, DWORD *pdwNum = NULL);
   void DirtySet (BOOL fDirty = TRUE);
   BOOL DirtyGet (void);
   BOOL Commit (PCProgressSocket pProgress, BOOL fDebug=FALSE);
   CLibrary *Clone (PWSTR pszFile, PWSTR *papszInclude, DWORD dwIncludeNum);

   PCLibraryCategory CategoryGet (PWSTR pszCategory, BOOL fCreateIfNotExist = TRUE);

   WCHAR       m_szFile[256]; // library file - dont change this, but can read it
   WCHAR       m_szIndex[256];   // name of the index

private:
   BOOL MMLFrom (PCMMLNode2 pNode, DWORD *pdwNum);

   BOOL        m_fDirty;      // set to TRUE if library dirty and needs to be saved
   CListFixed  m_lCATINFO;    // list of category info
   PCMegaFile  m_pmf;         // megafile to read/write to
};
typedef CLibrary *PCLibrary;

extern CLibrary gaLibraryInstalled[MAXRENDERSHARDS];    // installed library
extern CLibrary gaLibraryUser[MAXRENDERSHARDS];         // user library
extern PCLibraryCategory gapLCTexturesInstalled[MAXRENDERSHARDS];  // custom textures
extern PCLibraryCategory gapLCTexturesUser[MAXRENDERSHARDS];
extern PCLibraryCategory gapLCObjectsInstalled[MAXRENDERSHARDS];   // custom objects
extern PCLibraryCategory gapLCObjectsUser[MAXRENDERSHARDS];

extern PWSTR gpszCatMajor;
extern PWSTR gpszCatMinor;
extern PWSTR gpszCatName;
extern PWSTR gpszCatGUIDCode;
extern PWSTR gpszCatGUIDSub;

DLLEXPORT void LibraryOpenFiles (DWORD dwRenderShard, BOOL fAppDir, PCProgressSocket pProgress);
DLLEXPORT void LibrarySaveFiles (DWORD dwRenderShard, BOOL fAppDir, PCProgressSocket pProgress, BOOL fSkipInstalled = FALSE);
DLLEXPORT void LibraryCombineLists (PCListFixed lA, PCListFixed lB);
DLLEXPORT PCLibraryCategory LibraryTextures (DWORD dwRenderShard, BOOL fUser);
DLLEXPORT PCLibraryCategory LibraryObjects (DWORD dwRenderShard, BOOL fUser);
DLLEXPORT PCLibraryCategory LibraryEffects (DWORD dwRenderShard, BOOL fUser);
DLLEXPORT PCLibrary LibraryUserClone (DWORD dwRenderShard, PWSTR pszFile);


/*****************************************************************************************
CThumbnail */
// {B7217153-B26D-46d8-8686-8B6DB0A27324}
DEFINE_GUID(GUID_ThumbnailHeader, 
0xb7217153, 0xb26d, 0x46d8, 0x86, 0x86, 0x8b, 0x6d, 0xb0, 0xa2, 0x73, 0x24);

class CThumbnail;
typedef CThumbnail *PCThumbnail;

class DLLEXPORT CThumbnailCache {
public:
   ESCNEWDELETE;

   CThumbnailCache (void);
   ~CThumbnailCache (void);

   BOOL Init (WCHAR *pszFile, PCProgressSocket pProgress);
   BOOL Commit (PCProgressSocket pProgress);

   HBITMAP ThumbnailToBitmap (GUID *pgMajor, GUID *pgMinor, HWND hWnd, COLORREF *pcTransparent);
   BOOL ThumbnailSize (GUID *pgMajor, GUID *pgMinor, DWORD *pdwWidth, DWORD *pdwHeight);
   BOOL ThumbnailRemove (GUID *pgMajor, GUID *pgMinor);
   BOOL ThumbnailAdd (GUID *pgMajor, GUID *pgMinor, PCImage pImage, COLORREF cTransparent = (DWORD)-1);
   BOOL ThumbnailClearAll (BOOL fMemoryOnly);

private:
   BOOL HouseCleaning (void); // remove some thumbnails if too many

   WCHAR    m_szFile[256];    // file name
   CMegaFile m_mfFile;        // megafile
   CListFixed m_lPCThumbnail; // list of pointers to thumbnails
};
typedef CThumbnailCache *PCThumbnailCache;

extern CThumbnailCache      gThumbnail;    // main thumbnail cache

DLLEXPORT void ThumbnailOpenFile (PCProgressSocket pProgress);
DLLEXPORT void ThumbnailSaveFile(PCProgressSocket pProgress);
DLLEXPORT PCThumbnailCache ThumbnailGet (void);


/****************************************************************************************
VolumeIntersect */
DLLEXPORT PCListFixed OutlineFromPoints (PCWorldSocket pWorld, DWORD dwPlane, BOOL fFlip);

/**********************************************************************************
CObjectCeilingFan */
// {454D9732-9EC8-43c2-8D9C-B049FE82A2B1}
DEFINE_GUID(CLSID_CeilingFan, 
0x454d9732, 0x9ec8, 0x43c2, 0x8d, 0x9c, 0xb0, 0x49, 0xfe, 0x82, 0xa2, 0xb1);


// CObjectCeilingFan is a test object to test the system
class DLLEXPORT CObjectCeilingFan : public CObjectTemplate {
public:
   CObjectCeilingFan (PVOID pParams, POSINFO pInfo);
   ~CObjectCeilingFan (void);

   virtual void Delete (void);
   virtual void Render (POBJECTRENDER pr, DWORD dwSubObject);
   virtual CObjectSocket *Clone (void);
   virtual PCMMLNode2 MMLTo (void);
   virtual BOOL MMLFrom (PCMMLNode2 pNode);
   virtual BOOL MoveReferencePointQuery (DWORD dwIndex, PCPoint pp);
   virtual BOOL MoveReferenceStringQuery (DWORD dwIndex, PWSTR psz, DWORD dwSize, DWORD *pdwNeeded);
   virtual BOOL ObjectMatrixSet (CMatrix *pObject);
   virtual BOOL EmbedDoCutout (void);
   virtual BOOL ControlPointQuery (DWORD dwID, POSCONTROL pInfo);
   virtual BOOL ControlPointSet (DWORD dwID, PCPoint pVal, PCPoint pViewer);
   virtual void ControlPointEnum (PCListFixed plDWORD);
   virtual void QueryBoundingBox (PCPoint pCorner1, PCPoint pCorner2, DWORD dwSubObject);

//private:
   void CalcInfo (void);

   DWORD    m_dwBlades;          // number of blades
   fp       m_fStemLen;          // length of the stem
   fp       m_fBladeLen;         // length of the blades

   // calculated
   CRevolution    m_aRev[3];     // revolutions used to draw the fan
   CNoodle        m_Noodle;      // one noodle for the stem
   CMatrix        m_mHang;       // hanging matrix

   CRenderSurface    m_Renderrs;    // to minimize mallocs
};
typedef CObjectCeilingFan *PCObjectCeilingFan;





/**********************************************************************************
CObjectCabinet */
// {806C78CE-AB95-4356-91D3-7B26034AC8B4}
DEFINE_GUID(CLSID_Cabinet, 
0x806c78ce, 0xab95, 0x4356, 0x91, 0xd3, 0x7b, 0x26, 0x3, 0x4a, 0xc8, 0xb4);


// CObjectCabinet is a test object to test the system
class DLLEXPORT CObjectCabinet : public CObjectTemplate {
public:
   CObjectCabinet (PVOID pParams, POSINFO pInfo);
   ~CObjectCabinet (void);

   virtual void Delete (void);
   virtual void Render (POBJECTRENDER pr, DWORD dwSubObject);
   virtual CObjectSocket *Clone (void);
   virtual PCMMLNode2 MMLTo (void);
   virtual BOOL MMLFrom (PCMMLNode2 pNode);
   virtual BOOL ControlPointQuery (DWORD dwID, POSCONTROL pInfo);
   virtual BOOL ControlPointSet (DWORD dwID, PCPoint pVal, PCPoint pViewer);
   virtual void ControlPointEnum (PCListFixed plDWORD);
   virtual BOOL DialogQuery (void);
   virtual BOOL DialogShow (DWORD dwSurface, PCEscWindow pWindow, PCPoint pClick);
   virtual BOOL ContHVQuery (PCPoint pEye, PCPoint pClick, DWORD dwSurface, PTEXTUREPOINT pOld, PTEXTUREPOINT pHV);
   virtual BOOL ContCutout (const GUID *pgEmbed, DWORD dwNum, PCPoint paFront, PCPoint paBack, BOOL fBothSides);
   virtual BOOL ContCutoutToZipper (const GUID *pgEmbed, PCListFixed plistFrontHVXYZ, PCListFixed plistBackHVXYZ);
   virtual BOOL ContMatrixFromHV (DWORD dwSurface, PTEXTUREPOINT pHV, fp fRotation, PCMatrix pm);
   virtual fp ContThickness (DWORD dwSurface, PTEXTUREPOINT pHV);
   virtual DWORD ContSideInfo (DWORD dwSurface, BOOL fOtherSide);
   virtual BOOL Deconstruct (BOOL fAct);
   virtual BOOL ContEmbeddedRenamed (const GUID *pgOld, const GUID *pgNew);
   virtual void QueryBoundingBox (PCPoint pCorner1, PCPoint pCorner2, DWORD dwSubObject);

   void AdjustAllSurfaces (void);


   PBBSURF WishListAddCurved (PBBSURF pSurf, DWORD dwH, DWORD dwV,
      PCPoint papControl, DWORD *padwSegCurveH, DWORD *padwSegCurveV, DWORD dwFlags);
   PBBSURF WishListAddCurved (PBBSURF pSurf, PCSpline pSpline,
                                           fp fBottom, fp fTop, DWORD dwFlags);
   PBBSURF WishListAddFlat (PBBSURF pSurf, PCPoint pBase1, PCPoint pBase2,
                            PCPoint pTop1, PCPoint pTop2, DWORD dwFlags);
   PBBSURF WishListAdd (PBBSURF pSurf);

   void MakeTheWalls (fp fStartAt, fp fRoofHeight, BOOL fBase);
   BOOL MakeShelfOrCounter (DWORD dwID, fp fHeight, fp fThick, BOOL fCounter,
                                         PCPoint pExtend, fp fRound, BOOL *pafCircular = NULL);


   CListFixed        m_listBBSURF;        // list of building block surfaces
   CListFixed        m_listWish;          // wish list for BBSRUF - temporary so dont clone, etc.
   fp            m_fHeight;           // Height of roof above sideA of first floor that
                                          // house starts on (excluding basement)
   fp            m_fCounterThick;  // thickness of the structure
   fp            m_fWallThick;  // thickness of the structure
   fp            m_fShelfThick;  // thickness of the structure
   fp            m_fLevelElevation[NUMLEVELS]; // elevation at each elevation
   fp            m_fLevelHigher;      // at higher levels increment by this much
   fp            m_fLevelMinDist;     // minimum distance between level and ceiling
   CSpline           m_sPath;             // path that the cabinets take. Front is on the right

   BOOL           m_fDrawCounter;      // if TRUE the top is a counter, FALSE it's a shelf
   BOOL           m_fDrawOnlyBottom;   // if TRUE, draw only the boottom shelf. Else, draw all shelves
   DWORD          m_dwRoundEnd;        // if bit 0 then round left end, 1 then right end
   DWORD          m_dwShowWall;        // bit 0=show back, 1=show front, 2=showleft, 3=showright
   fp             m_fBaseHeight;       // base height for cabinet
   fp             m_fCabinetDepth;     // how deep the cabinet is, defaults to 600 mm
   fp             m_fCounterRounded;   // amount that is rounded, in meters.
   CPoint         m_pCounterOverhang;  // coverhang from cabinet, [0] for back, [1] for front, [2] left, [3] right
   CPoint         m_pBaseIndent;       // amount base is indented, [0]=back, [1]=front, [2]=left, [3]=right

private:
   void IntegrityCheck (void);


   CRenderSurface    m_Renderrs;       // for malloc optimization
};
typedef CObjectCabinet *PCObjectCabinet;


/**********************************************************************************
CObjectDrawer */

// {6D40D1DE-BE85-41cf-A056-708551901FAD}
DEFINE_GUID(CLSID_Drawer, 
0x6d40d1de, 0xbe85, 0x41cf, 0xa0, 0x56, 0x70, 0x85, 0x51, 0x90, 0x1f, 0xad);

// CObjectDrawer is a test object to test the system
class DLLEXPORT CObjectDrawer : public CObjectTemplate {
public:
   CObjectDrawer (PVOID pParams, POSINFO pInfo);
   ~CObjectDrawer (void);

   virtual void Delete (void);
   virtual void Render (POBJECTRENDER pr, DWORD dwSubObject);
   virtual CObjectSocket *Clone (void);
   virtual PCMMLNode2 MMLTo (void);
   virtual BOOL MMLFrom (PCMMLNode2 pNode);
   virtual fp OpenGet (void);
   virtual BOOL OpenSet (fp fOpen);
   virtual BOOL MoveReferencePointQuery (DWORD dwIndex, PCPoint pp);
   virtual BOOL MoveReferenceStringQuery (DWORD dwIndex, PWSTR psz, DWORD dwSize, DWORD *pdwNeeded);
   virtual BOOL EmbedDoCutout (void);
   virtual BOOL DialogQuery (void);
   virtual BOOL DialogShow (DWORD dwSurface, PCEscWindow pWindow, PCPoint pClick);

//private:
   void CalcDoorKnob (void);
   BOOL CommitRect (PCPoint ppMin, PCPoint ppMax, fp fY, DWORD dwColor, DWORD dwFlags, PCMatrix pMatrix = NULL);

   fp          m_fOpened;       // amount that opened, from 0..1
   CPoint      m_pOpening;    // opening size. [0]=LR,[1]=UD,[2]=depth,[3]=height of drawer wall as % of UD
   CPoint      m_pExtend;     // extend drawer beyond opening size, [0]=left,[1]=right,[2]=top,[3]=bottom
   CPoint      m_pThick;      // thickness, [0] = of front panel, [1] = of other boards
   DWORD       m_dwKnobStyle; // doorknob style
   fp          m_fKnobFromTop;    // distance from the top
   fp          m_fKnobRotate; // rotate knob

   CListFixed  m_lDRENDELEM;  // render elements

   CRenderSurface    m_Renderrs;       // for malloc optimization
   CListFixed        m_RenderL1;       // for malloc optimization
   CListFixed        m_RenderL2;       // for malloc optimization
};
typedef CObjectDrawer *PCObjectDrawer;

/**********************************************************************************
CObjectPathway */
// {15B10A1B-5EC3-4b78-9E43-636071F25A91}
DEFINE_GUID(CLSID_Pathway, 
0x15b10a1b, 0x5ec3, 0x4b78, 0x9e, 0x43, 0x63, 0x60, 0x71, 0xf2, 0x5a, 0x91);

// CObjectPathway is a test object to test the system
class DLLEXPORT CObjectPathway : public CObjectTemplate {
public:
   CObjectPathway (PVOID pParams, POSINFO pInfo);
   ~CObjectPathway (void);

   virtual void Delete (void);
   virtual void Render (POBJECTRENDER pr, DWORD dwSubObject);
   virtual CObjectSocket *Clone (void);
   virtual PCMMLNode2 MMLTo (void);
   virtual BOOL MMLFrom (PCMMLNode2 pNode);
   virtual BOOL ControlPointQuery (DWORD dwID, POSCONTROL pInfo);
   virtual BOOL ControlPointSet (DWORD dwID, PCPoint pVal, PCPoint pViewer);
   virtual void ControlPointEnum (PCListFixed plDWORD);
   virtual BOOL DialogQuery (void);
   virtual BOOL DialogShow (DWORD dwSurface, PCEscWindow pWindow, PCPoint pClick);
   virtual BOOL MoveReferencePointQuery (DWORD dwIndex, PCPoint pp);
   virtual BOOL MoveReferenceStringQuery (DWORD dwIndex, PWSTR psz, DWORD dwSize, DWORD *pdwNeeded);
   virtual BOOL IntelligentAdjust (BOOL fAct);
   virtual BOOL IntelligentPositionDrag (CWorldSocket *pWorld, POSINTELLIPOSDRAG pInfo,  POSINTELLIPOSRESULT pResult);
   virtual void QueryBoundingBox (PCPoint pCorner1, PCPoint pCorner2, DWORD dwSubObject);

//private:
   BOOL CalcPath (void);


   CSpline     m_sPath;       // path that's followed
   CPoint      m_pWidth;      // p[1] = height, p[2] = roundness (from 0..1), p[3] = roundness of ends (from 0..1)
   BOOL        m_fKeepSameWidth; // if TRUE, always keep at the same width
   BOOL        m_fKeepLevel;  // keep the pathway level

   // calculated
   DWORD       m_dwNumPoints;    // number of points
   DWORD       m_dwNumNormals;   // number of normals
   DWORD       m_dwNumText;      // number of texture settings
   DWORD       m_dwNumVertex;    // number of vertices
   DWORD       m_dwNumPoly;      // number of polygons
   CMem        m_memPoints;      // memory containing points
   CMem        m_memNormals;     // memory containing normals
   CMem        m_memText;        // memory containign textures
   CMem        m_memVertex;      // memory containing vertices
   CMem        m_memPoly;        // memory containing polygons - array of 4-DWORs for vertices.
                                 // last DWORD can be -1 indicating just a triangle

   CRenderSurface    m_Renderrs;    // to minimize mallocs
};
typedef CObjectPathway *PCObjectPathway;


/**********************************************************************************
CObjectTarp */

// {FC919F97-BB7C-4d71-998D-5E2DB6B991AC}
DEFINE_GUID(CLSID_Tarp, 
0xfc919f97, 0xbb7c, 0x4d71, 0x99, 0x8d, 0x5e, 0x2d, 0xb6, 0xb9, 0x91, 0xac);


// CObjectTarp is a test object to test the system
class DLLEXPORT CObjectTarp : public CObjectTemplate {
public:
   CObjectTarp (PVOID pParams, POSINFO pInfo);
   ~CObjectTarp (void);

   virtual void Delete (void);
   virtual void Render (POBJECTRENDER pr, DWORD dwSubObject);
   virtual CObjectSocket *Clone (void);
   virtual PCMMLNode2 MMLTo (void);
   virtual BOOL MMLFrom (PCMMLNode2 pNode);
   virtual BOOL MoveReferencePointQuery (DWORD dwIndex, PCPoint pp);
   virtual BOOL MoveReferenceStringQuery (DWORD dwIndex, PWSTR psz, DWORD dwSize, DWORD *pdwNeeded);
   virtual BOOL ControlPointQuery (DWORD dwID, POSCONTROL pInfo);
   virtual BOOL ControlPointSet (DWORD dwID, PCPoint pVal, PCPoint pViewer);
   virtual void ControlPointEnum (PCListFixed plDWORD);
   virtual void QueryBoundingBox (PCPoint pCorner1, PCPoint pCorner2, DWORD dwSubObject);

//private:
   BOOL CalcTarp (void);

   DWORD       m_dwNumCorner;    // number of corners, either 3 or 4
   CPoint      m_apPoint[8];     // [0] = LR, [2] = LL, [4] = UL, [6] = UR. In-between points are sag info

   // calculated
   DWORD       m_dwNumPoints;    // number of points
   DWORD       m_dwNumNormals;   // number of normals
   DWORD       m_dwNumText;      // number of texture settings
   DWORD       m_dwNumVertex;    // number of vertices
   DWORD       m_dwNumPoly;      // number of polygons
   CMem        m_memPoints;      // memory containing points
   CMem        m_memNormals;     // memory containing normals
   CMem        m_memText;        // memory containign textures
   CMem        m_memVertex;      // memory containing vertices
   CMem        m_memPoly;        // memory containing polygons - array of 4-DWORs for vertices.
                                 // last DWORD can be -1 indicating just a triangle

   CRenderSurface    m_Renderrs;    // to minimize mallocs
};
typedef CObjectTarp *PCObjectTarp;


/**********************************************************************************
CObjectPool */

// {CB4BD7DE-6783-445a-8692-FCF48C067589}
DEFINE_GUID(CLSID_Pool, 
0xcb4bd7de, 0x6783, 0x445a, 0x86, 0x92, 0xfc, 0xf4, 0x8c, 0x6, 0x75, 0x89);


// CObjectPool is a test object to test the system
class DLLEXPORT CObjectPool : public CObjectTemplate {
public:
   CObjectPool (PVOID pParams, POSINFO pInfo);
   ~CObjectPool (void);

   virtual void Delete (void);
   virtual void Render (POBJECTRENDER pr, DWORD dwSubObject);
   virtual CObjectSocket *Clone (void);
   virtual PCMMLNode2 MMLTo (void);
   virtual BOOL MMLFrom (PCMMLNode2 pNode);
   virtual BOOL MoveReferencePointQuery (DWORD dwIndex, PCPoint pp);
   virtual BOOL MoveReferenceStringQuery (DWORD dwIndex, PWSTR psz, DWORD dwSize, DWORD *pdwNeeded);
   virtual BOOL ControlPointQuery (DWORD dwID, POSCONTROL pInfo);
   virtual BOOL ControlPointSet (DWORD dwID, PCPoint pVal, PCPoint pViewer);
   virtual void ControlPointEnum (PCListFixed plDWORD);
   virtual fp TurnOnGet (void);
   virtual BOOL TurnOnSet (fp fOpen);
   virtual BOOL DialogQuery (void);
   virtual BOOL DialogShow (DWORD dwSurface, PCEscWindow pWindow, PCPoint pClick);
   virtual BOOL EmbedDoCutout (void);

//private:
   BOOL CalcPool (void);
   void ShapeToSpline (PCSpline ps);
   BOOL MyEmbedDoCutout (void);

   DWORD       m_dwShape;        // 0 for custom, 1 for rect, 2 for rounded rect, 3 for oval, 4 for rounded
   CPoint      m_pShapeSize;     // If if not custom. p[0] = width, p[1] = length, p[2] = rounded amount (in meters) for rounded rect
   CSpline     m_sShape;         // pool shape. [0] and [1] are valid. [2] is 0.
   CPoint      m_pLip;           // lip information. p[0] = lip width, p[1] = lip height
   CPoint      m_pCenter;        // center point. Use p[2] as depth of bottom at center location
   CPoint      m_pBottomNorm;    // normal of the bottom - use to angle the base
   BOOL        m_fFitToEmbed;    // if TRUE then dont draw lip, but fit directly to embedded object. Good for ponds
   BOOL        m_fRectangular;   // if TRUE, draw lip to a rectangle surronding pool, else lip shaped with pool
   fp          m_fWallSlope;     // 0..1 - amount that walls slope in, 0=ud, 1=all the want to center
   fp          m_fWallRounded;   // 0..1 - indicates how rounded walls are. 0=straight, 1=sin
   fp          m_fFilled;        // 0..1 - amount that is filled with water. if 0 then draw no water
   fp          m_fHoleSize;      // diameter of hole at center point. Can be 0

   // calculated
   CListFixed  m_lLipOutside;    // list of points around the lip
   CListFixed  m_lPoolEdge;      // list of points for the edge of the pool's wall (furthest out)
   CListFixed  m_lPoolIn;        // list of points for the inside of the pool's wall (closest)
   CListFixed  m_lPoolHole;      // list of points surrounding the hole of the pool
   CListFixed  m_lWater;         // water boundary

   DWORD       m_dwNumPoints;    // number of points
   DWORD       m_dwNumNormals;   // number of normals
   DWORD       m_dwNumText;      // number of texture settings
   DWORD       m_dwNumVertex;    // number of vertices
   DWORD       m_dwNumPoly;      // number of polygons
   CMem        m_memPoints;      // memory containing points
   CMem        m_memNormals;     // memory containing normals
   CMem        m_memText;        // memory containign textures
   CMem        m_memVertex;      // memory containing vertices
   CMem        m_memPoly;        // memory containing polygons - array of 4-DWORs for vertices.
                                 // last DWORD can be -1 indicating just a triangle

   CPoint      m_pScaleMin;      // smallest corner
   CPoint      m_pScaleMax;      // furthest corner
   BOOL        m_fNeedRecalcEmbed;    // set initially so embedded object will be recalced on rendering
   

   CRenderSurface    m_Renderrs;    // to minimize mallocs
};
typedef CObjectPool *PCObjectPool;



/**********************************************************************************
CObjectFireplace */

// {D8434F81-A694-4d96-9040-BDB0443AAA08}
DEFINE_GUID(CLSID_Fireplace, 
0xd8434f81, 0xa694, 0x4d96, 0x90, 0x40, 0xbd, 0xb0, 0x44, 0x3a, 0xaa, 0x8);

// CObjectFireplace is a test object to test the system
class DLLEXPORT CObjectFireplace : public CObjectTemplate {
public:
   CObjectFireplace (PVOID pParams, POSINFO pInfo);
   ~CObjectFireplace (void);

   virtual void Delete (void);
   virtual void Render (POBJECTRENDER pr, DWORD dwSubObject);
   virtual CObjectSocket *Clone (void);
   virtual PCMMLNode2 MMLTo (void);
   virtual BOOL MMLFrom (PCMMLNode2 pNode);
   virtual BOOL MoveReferencePointQuery (DWORD dwIndex, PCPoint pp);
   virtual BOOL MoveReferenceStringQuery (DWORD dwIndex, PWSTR psz, DWORD dwSize, DWORD *pdwNeeded);
   virtual BOOL EmbedDoCutout (void);
   virtual BOOL ControlPointQuery (DWORD dwID, POSCONTROL pInfo);
   virtual BOOL ControlPointSet (DWORD dwID, PCPoint pVal, PCPoint pViewer);
   virtual void ControlPointEnum (PCListFixed plDWORD);
   
//private:
   CPoint      m_pChimney;    // p[0] = chimney width (at the top), p[1] = depth (at top), p[2] = height (above floor),
                              // p[3] = height that chimney starts at (above ground)
   CPoint      m_pChimneyCap; // 0 = cap width, 1=cap depth, 2 = cap height (cap below chimney top)
   CPoint      m_pFireplace;  // 1 = Y offset for front of fireplace - if Negative then can see flue fireplace flue in the room
                              // 2 = fireplace height (less than m_pChimney.p[3]),
                              // 3 = fireplace height at base (since goes below the ground) - Z value
                              // fireplace width = m_pfirebox.p[2]
                              // fireplace depth is m_pChimney.p[1], or firebox depth if below firebox piece
   CPoint      m_pFireBox;    // 0 = firebox width, 1 = firebox depth, 2=firebox height (from floor level)
                              // 3 = firebox curve (add this to firebox height to make 3 points for an arch
   CPoint      m_pBrickFloor; // brick floor in front of or below firebox,
                              // 0 = width, 1 = depth, 2 = height (form floor), 3 = offset from front of mantel
   CPoint      m_pHearth;     // 0 = hearth width, 1= hearth height (above floor), 2=hearth depth,
                              // 3 = hearth curve (add this to hearth height to create 3 points for an arch)
   CPoint      m_pMantel;     // 0 = width, 1 = (negative) amount sticks out beyond firebox's front, 2=height, 3=amount that sticks in beyond firebox's back

   // calculated
   BOOL        m_fNoodleDirty;   // set to true if the noodles are dirty
   CNoodle     m_MantelNood;  // mantel noodle
   CNoodle     m_CapNood;     // cap noodle


   CRenderSurface    m_Renderrs;       // for malloc optimization

};
typedef CObjectFireplace *PCObjectFireplace;


/**********************************************************************************
CObjectTruss */
// {B5AD8312-E519-4c0c-BDBE-EA717EC8255D}
DEFINE_GUID(CLSID_Truss, 
0xb5ad8312, 0xe519, 0x4c0c, 0xbd, 0xbe, 0xea, 0x71, 0x7e, 0xc8, 0x25, 0x5d);

// CObjectTruss is a test object to test the system
class DLLEXPORT CObjectTruss : public CObjectTemplate {
public:
   CObjectTruss (PVOID pParams, POSINFO pInfo);
   ~CObjectTruss (void);

   virtual void Delete (void);
   virtual void Render (POBJECTRENDER pr, DWORD dwSubObject);
   virtual CObjectSocket *Clone (void);
   virtual PCMMLNode2 MMLTo (void);
   virtual BOOL MMLFrom (PCMMLNode2 pNode);
   virtual BOOL MoveReferencePointQuery (DWORD dwIndex, PCPoint pp);
   virtual BOOL MoveReferenceStringQuery (DWORD dwIndex, PWSTR psz, DWORD dwSize, DWORD *pdwNeeded);
   virtual BOOL ControlPointQuery (DWORD dwID, POSCONTROL pInfo);
   virtual BOOL ControlPointSet (DWORD dwID, PCPoint pVal, PCPoint pViewer);
   virtual void ControlPointEnum (PCListFixed plDWORD);
   virtual BOOL DialogQuery (void);
   virtual BOOL DialogShow (DWORD dwSurface, PCEscWindow pWindow, PCPoint pClick);
   virtual void QueryBoundingBox (PCPoint pCorner1, PCPoint pCorner2, DWORD dwSubObject);

//private:
   void CalcInfo (void);

   CSpline     m_sTruss;         // main spline for the truss beam. If two beams then this is the top
   CSpline     m_sTrussBottom;   // bottom truss beam
   DWORD       m_dwShape;        // 2 = 2 dimensional, 3 = triangle, 4 = rectangular
   DWORD       m_dwMembersStyle; // style of the dialgonnal members, 0 = x, 1=triangle odd, 2=triangle even, 1=saw off, 2=saw even, 3 = none
   DWORD       m_dwAltVerticals; // 0 = vertical every one, 1=every odd one, 2=every even one
   DWORD       m_dwMembers;      // number of members
   BOOL        m_fMirrorStyle;   // if TRUE, mirror the styles half way through
   DWORD       m_dwShowBeam;     // bit-field flag to show the major beams
   DWORD       m_adwBeamStyle[2];   // style of the [0]=major and [1]=minor beams
   CPoint      m_apBeamSize[2];  // width and height of the [0]=major and [1]=minor beams
   fp          m_fMultiSize;     // if 3 or 4 dimensional, this is the distance between major beams

   // calculated
   CListFixed  m_lPCNoodleMajor; // list of noodle pointers for major beams
   CListFixed  m_lPCNoodleMinor; // list of noodle points for minor beams
   CListFixed  m_lMember;        // list of TEXTUREPOINT where member instersects 0..1, .h = top, .v = bottom

   CRenderSurface    m_Renderrs;    // to minimize mallocs
};
typedef CObjectTruss *PCObjectTruss;


/****************************************************************************************
MMLCompress */
DLLEXPORT BOOL MMLCompressMaxGet (void);
DLLEXPORT void MMLCompressMaxSet (BOOL fCompress);
DLLEXPORT DWORD MMLDecompressFromANSI (PVOID pMem, size_t dwSize, PCProgressSocket pProgress, PCMMLNode2 *ppNode);
DLLEXPORT DWORD MMLCompressToANSI (PCMMLNode2 pNode, PCProgressSocket pProgress, PCMem pMem);
DLLEXPORT DWORD MMLDecompressFromToken (PVOID pMem, size_t dwSize, PCProgressSocket pProgress, PCMMLNode2 *ppNode, BOOL fDontEncodeBinary);
DLLEXPORT DWORD MMLCompressToToken (PCMMLNode2 pNode, PCProgressSocket pProgress, PCMem pMem, BOOL fDontEncodeBinary);
DLLEXPORT DWORD RLEDecode (PBYTE pRLE, size_t dwSize, size_t dwElemSize,
                     PCMem pMem, size_t *pdwUsed);
DLLEXPORT DWORD RLEEncode (PBYTE pOrigMem, size_t dwNum, size_t dwElemSize, PCMem pMem);
DLLEXPORT DWORD PatternDecode (PBYTE pEnc, size_t dwSize, DWORD dwElemSize,
                     PCMem pMem, size_t *pdwUsed);
DLLEXPORT DWORD PatternEncode (PBYTE pOrigMem, size_t dwNum, size_t dwElemSize, PCMem pMem, PCProgressSocket pProgress);
DLLEXPORT BOOL MMLFileSave (WCHAR *pszFile, const GUID *pgID, PCMMLNode2 pNode, DWORD dwFormat = 1,
                            PCProgressSocket pProgress = NULL, BOOL fBypassGlobalMegafile = FALSE);
DLLEXPORT BOOL MMLFileSave (PCMem pMem, const GUID *pgID, PCMMLNode2 pNode, DWORD dwFormat = 1, PCProgressSocket pProgress = NULL);
DLLEXPORT BOOL MMLFileSave (PCMegaFile pMegaFile, WCHAR *pszFile, const GUID *pgID, PCMMLNode2 pNode, DWORD dwFormat = 1,
                            PCProgressSocket pProgress = NULL,
                            FILETIME *pftCreate = NULL, FILETIME *pftModify = NULL, FILETIME *pftAccess = NULL);
DLLEXPORT PCMMLNode2 MMLFileOpen (WCHAR *pszFile, const GUID *pgID, PCProgressSocket pProgress = NULL,
                                 BOOL fIgnoreDir = FALSE, BOOL fBypassGlobalMegafile = FALSE);
DLLEXPORT PCMMLNode2 MMLFileOpen (PCMegaFile pMegaFile, WCHAR *pszFile, const GUID *pgID, PCProgressSocket pProgress = NULL,
                       BOOL fIgnoreDir = FALSE);
DLLEXPORT PCMMLNode2 MMLFileOpen (HINSTANCE hInstance, DWORD dwID, PSTR pszResType, const GUID *pgID, PCProgressSocket pProgress = NULL);
DLLEXPORT PCMMLNode2 MMLFileOpen (PBYTE pMem, size_t dwSize, const GUID *pgID, PCProgressSocket pProgress = NULL);

DEFINE_GUID(GUID_FileHeader, 
0x1265d433, 0xcd44, 0xaf61, 0xae, 0x8a, 0x12, 0xb2, 0x8f, 0xa8, 0xfb, 0xb8);

DEFINE_GUID(GUID_FileHeaderComp, 
0xf665d213, 0xa912, 0x6c61, 0xae, 0x8a, 0x4a, 0xb2, 0x8f, 0x61, 0xfb, 0xb8);


/******************************************************************************************
Tracing */
// TRACEINFO - Describes tracing paper page
typedef struct {
   WCHAR       szFile[256];       // file name
   CPoint      pCenter;       // center in world coordinates
   CPoint      pAngle;        // angle, LLT, that appears on
   fp          fWidth;        // width of the image in meters
   BOOL        fShow;         // set to TRUE if showing
   DWORD       dwTransModel;  // transparency model. 0 for black lines on white, 1 for white lines on black, 2 for color
   fp          fTransparency; // transparency. 0 = opaque, 1=transparent
} TRACEINFO, *PTRACEINFO;

class CGroundView;
typedef CGroundView *PCGroundView;
DLLEXPORT BOOL TraceDialog (PCHouseView pv, PCGroundView pg = NULL);
DLLEXPORT void TraceInfoFromWorld (PCWorldSocket pWorld, PCListFixed pList);
DLLEXPORT void TraceCacheClear (void);
DLLEXPORT BOOL TraceApply (PCListFixed plTRACEINFO, PCRenderTraditional pRender, PCImage pImage);
DLLEXPORT BOOL TraceApply (PCListFixed plTRACEINFO, PCGroundView pg, PCImage pImage);


/*******************************************************************************************
Quote */
DLLEXPORT PWSTR DeepThoughtGenerate (void);


/*****************************************************************************************
Register */
BOOL RegisterPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam);
DLLEXPORT BOOL RegisterIsRegistered (void);


/*******************************************************************************************
CListView */
DLLEXPORT void ListViewCheckPage (void);
DLLEXPORT BOOL ListViewRemove (PCWorldSocket pWorld);
DLLEXPORT BOOL ListViewNew (PCWorldSocket pWorld, HWND hWndCreator);
DLLEXPORT void ListViewShowAll (BOOL fShow);

DLLEXPORT BOOL OLGetNamePage (PCEscPage pPage, DWORD dwMessage, PVOID pParam);

/************************************************************************
CAnimSocket - Is a pure virtual object supported by all the animation
objects. An "animation object" is one on the animation timeline that
informs the app what animation to do
*/
class CScene;
typedef CScene *PCScene;

class CSceneObj;
typedef CSceneObj *PCSceneObj;

class CAnimSocket {
public:
   virtual void Delete (void) = 0;
      // tell the object to delete itself

   virtual void WorldSet (PCWorldSocket pWorld, PCScene pScene, GUID *pgObject) = 0;
      // tells the animation object where it belongs; this information isn't saved in
      // MML but should be remembered and may be used by the object from time to time
      // pWorld is the world it's in, pScene is the scene it's in, pgObject is the object
      // (PCObjectSocket) it affects.

   virtual void WorldGet (PCWorldSocket *ppWorld, PCScene *ppScene, GUID *pgObject) = 0;
      // gets the current world, scene, and object. If any ppXXX attributes are NULL
      // then they're not filled in

   virtual void GUIDSet (GUID *pgObject) = 0;
      // tells the object what GUID it should use to identify itself. This is saved
      // in MML

   virtual void GUIDGet (GUID *pgObject) = 0;
      // returns the GUID that this uses to identify itself

   virtual void ClassGet (GUID *pgClass) = 0;
      // the object should fill in pgClass with a class identifier for the
      // object so it can be reloaded

   virtual int PriorityGet (void) = 0;
      // when objects are told to apply themselves to the attribute spline
      // they are sorted by priority (lowest values first) and called in that order.
      // So, objects that want to be first in line (such as keyframe) should use 0,
      // increasing to 50 for pose animations, and 100 for keyframe deltas.

   virtual void QueryMinDisplay (int *piX, int *piY) = 0;
      // the object fills in the minimum width and height (in pixels) that it
      // wants to display itself.

   virtual void TimeGet (fp *pfStart, fp *pfEnd) = 0;
      // the object fills pfStart and pfEnd with the start and end time of
      // the object

   virtual void TimeSet (fp fStart, fp fEnd) = 0;
      // sets the object's starting and ending time. It may choose to ignore
      // the fEnd (or adjust it) to stay within the object's limits

   virtual fp QueryTimeMax (void) = 0;
      // returns the maxium duration of time that the object will cover

   virtual void VertSet (fp fVert) = 0;
      // sets a vertical position for the object - any fp value is valid

   virtual fp VertGet (void) = 0;
      // returns the vertical position for the object, frm vert set

   virtual void Draw (HDC hDC, RECT *prHDC, fp fLeft, fp fRight) = 0;
      // has the object draw itself onto the given HDC. rHDC is the portion of the HDC
      // it should draw onto - although it can draw beyond the edges since clipping
      // will be enfored. fLeft is the time (in sec) at rHDC.left and fRight is the
      // time at rHDC.right. NOTE: If the object requested (in QueryMinDisplay) to
      // be larger (in x) than it technically should be, the time display may end
      // up being stretched out. rHDC.bottom - rHDC.top is >= QueryMinDisplay's Y value.

   virtual void AnimObjSplineApply (PCSceneObj pSpline) = 0;
      // tells the template to draw itself to the animation spline. Basically, it
      // can look through the splines of of the object, and get individual PCAnimAttribSplines
      // which it can then examine and/or modify as is appropriate

   virtual BOOL AnimObjSplineSecondPass (void) = 0;
      // the object returns TRUE if it wants AnimObjSplineApply() to be called a second time,
      // after all the other animation objects have changed the data. This is used by
      // keyframe objects to make sure that their their attribute settings override
      // everything else

   virtual BOOL Message (DWORD dwMessage, PVOID pParam) = 0;
      // sends a message to the object. The interpretation of the message depends upon
      // dwMessage, which is OSM_XXX. If the function understands and handles the
      // message it returns TRUE, otherwise FALE.

   virtual CAnimSocket *Clone (void) = 0;
      // clones the current object

   virtual PCMMLNode2 MMLTo (void) = 0;
      // the object should create a new PCMMLNode2 (to be freed by the caller)
      // and fill it in with all the information needed to recreate the object
      // (except for the class ID, which is done elsewhere)

   virtual BOOL MMLFrom (PCMMLNode2 pNode) = 0;
      // the object should read in all the information from PCMMLNode2 and
      // restore itself (as it was in MMLTo)

   virtual BOOL DialogQuery (void) = 0;
      // asks an object if it supports its own dialog box for custom
      // settings. Returns TRUE if it does, FALSE if not.

   virtual BOOL DialogShow (PCEscWindow pWindow) = 0;
      // if DialogQuery() returns TRUE, causes the object to bring up a dialog
      // for editing itself using pWindow. Returns 0 if the user presses close,
      // TRUE if the user presses a "Back" button.

   virtual BOOL Deconstruct (BOOL fAct) = 0;
      // Tells the object to deconstruct itself into sub-objects.
      // Basically, new objects will be added that exactly mimic this object,
      // and any embedeeding objects will be moved to the new ones.
      // NOTE: This old one will not be deleted - the called of Deconstruct()
      // will need to call Delete()
      // If fAct is FALSE the function is just a query, that returns
      // TRUE if the object cares about adjustment and can try, FALSE if it can't.

   virtual BOOL Merge (GUID *pagWith, DWORD dwNum) = 0;
      // asks the object to merge with the list of objects (identified by GUID) in pagWith.
      // dwNum is the number of objects in the list. The object should see if it can
      // merge with any of the ones in the list (some of which may no longer exist and
      // one of which may be itself). If it does merge with any then it return TRUE.
      // if no merges take place it returns false.

   virtual BOOL WaveFormatGet (DWORD *pdwSamplesPerSec, DWORD *pdwChannels) = 0;
      // If the object supports audio output, this fills in pdwSamplesPerSec and pdwChannels
      // with the sampling rate and channels of the audio. If it doesn't support audio
      // then returns FALSE

   virtual BOOL WaveGet (DWORD dwSamplesPerSec, DWORD dwChannels, int iTimeSec, int iTimeFrac,
                         DWORD dwSamples, short *pasSamp, fp *pfVolume) = 0;
      // Gets the digital audio from the object. The required sampling rate is dwSAmplesPerSec,
      // required channels dwCahnnels. Number of samples to get (in the new sampling rate) adw dwSamples,
      // and they are to be copied to pasSamp. iTimeSec is the starting time of the wave to get, in
      // whole seconds. iTimeFrac is in 1/dwSAmplesPerSec. Doing this instead of using a combined fp
      // value to make sure wave getting is 100% accurate. *pfVolume should be filled in with a
      // volume scale for mixing; 1.0 = no change. Returns FALSE if no data to get or it's
      // all 0's anyway so no point getting.
   
};
typedef CAnimSocket *PCAnimSocket;



/************************************************************************
CAnimTemplate - Template object that other CAnimXXX can be based on
*/

class DLLEXPORT CAnimTemplate : public CAnimSocket {
public:
   ESCNEWDELETE;

   CAnimTemplate (void);
   ~CAnimTemplate (void);

   // must be overridden
   //virtual void Delete (void) = 0;
   //virtual void AnimObjSplineApply (PCSceneObj pSpline) = 0;
   //virtual CAnimSocket *Clone (void) = 0; // must call into CloneTemplate()
   //virtual PCMMLNode2 MMLTo (void) = 0; // must call into MMLToTemplate()
   //virtual BOOL MMLFrom (PCMMLNode2 pNode) = 0; // must call into MMLFromTemplate()

   // usually overridden
   virtual void Draw (HDC hDC, RECT *prHDC, fp fLeft, fp fRight);
   virtual BOOL DialogQuery (void);
   virtual BOOL DialogShow (PCEscWindow pWindow);

   // might be overridden
   virtual void TimeSet (fp fStart, fp fEnd);
   virtual BOOL Message (DWORD dwMessage, PVOID pParam);
   virtual BOOL Deconstruct (BOOL fAct);
   virtual BOOL Merge (GUID *pagWith, DWORD dwNum);
   virtual BOOL WaveFormatGet (DWORD *pdwSamplesPerSec, DWORD *pdwChannels);
   virtual BOOL WaveGet (DWORD dwSamplesPerSec, DWORD dwChannels, int iTimeSec, int iTimeFrac,
                         DWORD dwSamples, short *pasSamp, fp *pfVolume);

   // not usually overridden
   virtual void WorldSet (PCWorldSocket pWorld, PCScene pScene, GUID *pgObject);
   virtual void WorldGet (PCWorldSocket *ppWorld, PCScene *ppScene, GUID *pgObject);
   virtual void GUIDSet (GUID *pgObject);
   virtual void GUIDGet (GUID *pgObject);
   virtual void ClassGet (GUID *pgClass);
   virtual int PriorityGet (void);
   virtual void QueryMinDisplay (int *piX, int *piY);
   virtual void TimeGet (fp *pfStart, fp *pfEnd);
   virtual fp QueryTimeMax (void);
   virtual void VertSet (fp fVert);
   virtual fp VertGet (void);
   virtual BOOL AnimObjSplineSecondPass (void);

   PCMMLNode2 MMLToTemplate (void);  // must be called to keep template info written away
   BOOL MMLFromTemplate (PCMMLNode2 pNode);   // must be called to get template info from mml
   void CloneTemplate (CAnimTemplate *pCloneTo); // must be called to clone MML info

   // member variables that should be set by the constructor of super-class
   GUID                 m_gClass;      // class identifier for ClassGet()
   int                  m_iPriority;   // priority for PriorityGet()
   int                  m_iMinDisplayX; // minimum X display, for QueryMinDisplay
   int                  m_iMinDisplayY; // mininum Y display, for QueryMinDisplay
   fp                   m_fTimeMax;    // maximum allowed time. For QueryTimeMax
   BOOL                 m_fAnimObjSplineSecondPass;   // return value for AnimObjSplineSecondPass(). Defaults to FALSE

   // member variables automagically stored
   PCWorldSocket        m_pWorld;      // from WorldSet()
   PCScene              m_pScene;      // from WorldSet()
   GUID                 m_gWorldObject; // from WorldSet
   GUID                 m_gAnim;       // animation object, from GUIDSet. Saved in MML
   CPoint               m_pLoc;        // p[0] = start time, p[1] = end time. p[2] = vertical location. Saved in MML

private:
};
typedef CAnimTemplate *PCAnimTemplate;

/************************************************************************
CAnimKeyframe */

DEFINE_GUID(CLSID_AnimKeyframe, 
0x9865ddf3, 0xcd44, 0xaf2c, 0xae, 0x85, 0x12, 0xb2, 0xfc, 0xa8, 0xfb, 0xb8);

class DLLEXPORT CAnimKeyframe : public CAnimTemplate {
public:
   CAnimKeyframe (DWORD dwType);
   ~CAnimKeyframe (void);

   virtual void Delete (void);
   virtual void AnimObjSplineApply (PCSceneObj pSpline);
   virtual CAnimSocket *Clone (void);
   virtual PCMMLNode2 MMLTo (void);
   virtual BOOL MMLFrom (PCMMLNode2 pNode);

   virtual void Draw (HDC hDC, RECT *prHDC, fp fLeft, fp fRight);
   virtual BOOL DialogQuery (void);
   virtual BOOL DialogShow (PCEscWindow pWindow);
   virtual BOOL Message (DWORD dwMessage, PVOID pParam);
   virtual BOOL Deconstruct (BOOL fAct);
   virtual BOOL Merge (GUID *pagWith, DWORD dwNum);

   void SyncFromWorld (void);
   BOOL SupportAttribute (PWSTR pszName, DWORD dwLen);
   BOOL AttributeSet (PWSTR pszName, fp fValue, BOOL fCreateIfNotExist);
   BOOL AttribRemove (PWSTR pszName);

   CListFixed     m_lATTRIBVAL;    // list of attributes supported
   DWORD          m_dwType;        // 0 for keyframe excluding motion, 1 for translation, 2 for rotation
   DWORD          m_dwLinear;      // 0 for all points produce constant interp, 1 for linear, 2 for spline
private:
};
typedef CAnimKeyframe *PCAnimKeyframe;



/************************************************************************
CAnimBookmark */

DEFINE_GUID(CLSID_AnimBookmark, 
0x981cdd5f, 0x8a44, 0x622c, 0xae, 0x85, 0x09, 0xb2, 0xfc, 0x54, 0xfb, 0x43);

class DLLEXPORT CAnimBookmark : public CAnimTemplate {
public:
   CAnimBookmark (DWORD dwType);
   ~CAnimBookmark (void);

   virtual void Delete (void);
   virtual void AnimObjSplineApply (PCSceneObj pSpline);
   virtual CAnimSocket *Clone (void);
   virtual PCMMLNode2 MMLTo (void);
   virtual BOOL MMLFrom (PCMMLNode2 pNode);

   virtual void Draw (HDC hDC, RECT *prHDC, fp fLeft, fp fRight);
   virtual BOOL DialogQuery (void);
   virtual BOOL DialogShow (PCEscWindow pWindow);
   virtual BOOL Message (DWORD dwMessage, PVOID pParam);
   virtual BOOL Deconstruct (BOOL fAct);

   DWORD          m_dwType;        // 0 for Bookmark of 0 time, 1 for bookmark of any time
   WCHAR          m_szName[64];     // bookmark name
private:
};
typedef CAnimBookmark *PCAnimBookmark;


/************************************************************************
CAnimPath */

DEFINE_GUID(CLSID_AnimPath, 
0xa192dd21, 0xcdc4, 0xa861, 0xae, 0x8a, 0x12, 0x10, 0x8f, 0x9b, 0xfb, 0xa6);

class DLLEXPORT CAnimPath : public CAnimTemplate {
public:
   CAnimPath (void);
   ~CAnimPath (void);

   virtual void Delete (void);
   virtual void AnimObjSplineApply (PCSceneObj pSpline);
   virtual CAnimSocket *Clone (void);
   virtual PCMMLNode2 MMLTo (void);
   virtual BOOL MMLFrom (PCMMLNode2 pNode);

   virtual void Draw (HDC hDC, RECT *prHDC, fp fLeft, fp fRight);
   virtual BOOL DialogQuery (void);
   virtual BOOL DialogShow (PCEscWindow pWindow);

private:
};
typedef CAnimPath *PCAnimPath;


/************************************************************************
CAnimWave */

DEFINE_GUID(CLSID_AnimWave, 
0x9865ddf3, 0xcd44, 0xaf61, 0xae, 0x8a, 0x12, 0xb2, 0x8f, 0xa8, 0xfb, 0xb8);

class CM3DWave;
typedef CM3DWave *PCM3DWave;

class DLLEXPORT CAnimWave : public CAnimTemplate {
public:
   CAnimWave (void);
   ~CAnimWave (void);

   virtual void Delete (void);
   virtual void AnimObjSplineApply (PCSceneObj pSpline);
   virtual CAnimSocket *Clone (void);
   virtual PCMMLNode2 MMLTo (void);
   virtual BOOL MMLFrom (PCMMLNode2 pNode);

   virtual void Draw (HDC hDC, RECT *prHDC, fp fLeft, fp fRight);
   virtual BOOL DialogQuery (void);
   virtual BOOL DialogShow (PCEscWindow pWindow);
   virtual BOOL Deconstruct (BOOL fAct);
   virtual BOOL WaveFormatGet (DWORD *pdwSamplesPerSec, DWORD *pdwChannels);
   virtual BOOL WaveGet (DWORD dwSamplesPerSec, DWORD dwChannels, int iTimeSec, int iTimeFrac,
                         DWORD dwSamples, short *pasSamp, fp *pfVolume);

//private:
   CAnimWave *CAnimWave::InternalClone (void);

   DWORD          m_dwPreferredView;      // what preferred view is: 0 for file name, 1 for text,
                                          // 2 for phonemes, 3 for wave, 4 for FFT
   fp             m_fMixVol;              // mix volume, 1.0 = normal, 2.0 = 2x as loud, etc.
   PCM3DWave      m_pWave;                // cached wave file
   BOOL           m_fLoop;                // if TRUE then loop the audio for the length
   BOOL           m_fCutShort;            // if TRUE will cut short the audio if wave shorter
   DWORD          m_dwLipSync;            // 0 for off, 1 for open/close attrib, 2 for automatic
   WCHAR          m_szLipSyncOnOff[64];   // name of attribute to use if it's on/off
   fp             m_fLipSyncScale;        // amount to scale lip sync attribs, 1 = default
};
typedef CAnimWave *PCAnimWave;

/**************************************************************************************
CUndoScenePacket - Handles undo/redo storage */
class CSceneSet;
typedef CSceneSet *PCSceneSet;
typedef struct {
   DWORD             dwObject;            // Type of object.    see below
   DWORD             dwChange;            // chage. 0=changed,1=added,2=removed

   // dwObject == 0, world object
   PCAnimSocket      pObject;             // object changed. If Changed/removed this is
                                          // the contents right before changing/removing.
                                          // If Added, this is final result. Flip for Redo buffer
   GUID              gScene;              // scene's GUID
   GUID              gObjectWorld;        // GUID of the world object that's associated with
   GUID              gObjectAnim;         // GUID of the anim object
} UNDOSCENECHANGE, *PUNDOSCENECHANGE;

// SCENESEL - Structures to indicate what's selected for undo
typedef struct {
   GUID              gScene;              // scene's GUID
   GUID              gObjectWorld;        // GUID of the world object that's associated with
   GUID              gObjectAnim;         // GUID of the anim object
} SCENESEL, *PSCENESEL;

class DLLEXPORT CUndoScenePacket {
public:
   ESCNEWDELETE;

   CUndoScenePacket (void);
   ~CUndoScenePacket (void);
   void Change (PCSceneSet pSceneSet, PCAnimSocket pObject, BOOL fBefore, DWORD dwChange);
   void FillSelectionList (PCSceneSet pSceneSet);

   CListFixed        m_lUNDOSCENECHANGE;  // list of UNDOSCENECHANGE structures
   CListFixed        m_lSCENESEL;     // list of GUIDs indicating which objects are selected

};
typedef CUndoScenePacket *PCUndoScenePacket;



/**************************************************************************************
CViewSceneSocket - Socket that's called into by the SceneSet to inform all objects
on the norification list that an object or selection has changed
*/
class CViewSceneSocket {
public:
   virtual void SceneChanged (DWORD dwChanged, GUID *pgScene, GUID *pgObjectWorld, GUID *pgObjectAnim) = 0;
      // tell the view that the world has changed. dwChanged is a bit-field
      // of WORLDC_XXX, indicating what has changed since the last call
      // if the scene that changed is known, pgScene will point go a scene guid
      // if the CObjectSocket is know pgObjectWorld will point to an object guid
      // if the CAnimSocket is know, pgObjectAnim will point to an animation object guid

   virtual void SceneUndoChanged (BOOL fUndo, BOOL fRedo) = 0;
      // tell the view that the world's undo/redo state has changed. (or
      // it might not have). fUndo is TRUE if undos are possible.
      // fRedo is TRUE if redos are possible

   virtual void SceneStateChanged (BOOL fScene, BOOL fTime) = 0;
      // indicates that the scene's state has changed. If fScene is TRUE then
      // the scene itself changed. if fTime is TRUE then the time changed
};
typedef CViewSceneSocket * PCViewSceneSocket;


// SSDATTRIB - remember default attributes
typedef struct {
   GUID           gObjectWorld;     // CObjectSocket object
   PCListFixed    plATTRIBVAL;      // pointer to list of ATTRIBVAL. Sorted alphabetically
} SSDATTRIB, *PSSDATTRIB;

/***************************************************************************************
CSceneSet - Manages a collection of scenes, their undo buffers, and their link
to the world.
*/
class DLLEXPORT CSceneSet : public CViewSocket {
public:
   ESCNEWDELETE;

   CSceneSet (void);
   ~CSceneSet (void);

   void WorldSet (PCWorld pWorld);
   PCWorld WorldGet (void);

   BOOL DirtyGet (void);
   void DirtySet (BOOL fDirty);

   BOOL NotifySocketAdd (PCViewSceneSocket pAdd);
   BOOL NotifySocketRemove (PCViewSceneSocket pRemove);
   void NotifySockets (DWORD dwChange, GUID *pgScene, GUID *pgObjectWorld, GUID *pgObjectAnim);

   DWORD SceneNum (void);  // number of scenes
   DWORD SceneNew (void);  // creates a new scene. Returns index, -1 if error
   BOOL SceneRemove (DWORD dwIndex); // removes scene
   PCScene SceneGet (DWORD dwIndex);   // get scene
   DWORD SceneClone (DWORD dwIndex);   // clones scene. Returns index to new one. -1 if error
   DWORD SceneFind (PWSTR pszName); // find a scene by its name. Returns index. -1 if error
   DWORD SceneFind (GUID *pgScene); // finds a scene by its guid. Returns index. -1 if error

   void Clear (void);   // clears everything
   void CameraSet (GUID *pgCamera);
   BOOL CameraGet (GUID *pgCamera);

   PCMMLNode2 MMLTo (PCProgressSocket pProgress = NULL);
   BOOL MMLFrom (PCMMLNode2 pNode, PCProgressSocket pProgress = NULL);

   BOOL SelectionRemove (GUID *pgScene, GUID *pgObjectWorld, GUID *pgObjectAnim);
   BOOL SelectionAdd (GUID *pgScene, GUID *pgObjectWorld, GUID *pgObjectAnim);
   BOOL SelectionExists (GUID *pgScene, GUID *pgObjectWorld, GUID *pgObjectAnim);
   PSCENESEL SelectionEnum (DWORD *pdwNum);
   BOOL SelectionClear (void);

   void ObjectAboutToChange (PCAnimSocket pObject);
   void ObjectChanged (PCAnimSocket pObject);
   void ObjectRemoved (PCAnimSocket pObject);   // updated undo with this (so dont delete pObject), and sends out notification
   void ObjectAdded (PCAnimSocket pObject);  // updates undo with this, and sends out notification

   void UndoClear (BOOL fUndo, BOOL fRedo);
   void UndoRemember (void);
   BOOL UndoQuery (BOOL *pfRedo);
   BOOL Undo (BOOL fUndoIt = TRUE);

   void StateGet (PCScene *ppScene, fp *pfTime);   // gets the current scene and time
   BOOL StateSet (PCScene pScene, fp fTime);        // sets the current scene and time. complex interaction w/world
   void SyncFromWorld (void); // if any object in world have changed since last time was called will sync them up - changes the scene
   void SyncToWorld (void);


   // connected to world so know what changes in it and keep record
   virtual void WorldChanged (DWORD dwChanged, GUID *pgObject);
   virtual void WorldUndoChanged (BOOL fUndo, BOOL fRedo);
   virtual void WorldAboutToChange (GUID *pgObject);

   // default attributes for all world objects
   BOOL DefaultAttribExist (GUID *pgObjectWorld);  // returns true if any default attributes exist for the object
   BOOL DefaultAttribGetAll (GUID *pgObjectWorld, PCListFixed plATTRIBVAL);   // gets all of the defaults remembered for the object
   BOOL DefaultAttribGet (GUID *pgObjectWorld, PWSTR pszAttrib, fp *pfValue, BOOL fAutoRemember);
      // if fAutoRemember then if there is no default remembered, it will automatically
      // remember this, assuming that it's about to change
   BOOL DefaultAttribSet (GUID *pgObjectWorld, PWSTR pszAttrib, fp fValue);
   BOOL DefaultAttribForget (GUID *pgObjectWorld, PWSTR pszAttrib); // forget what remembered about default



   BOOL  m_fKeepUndo;      // if TRUE (default), keep track of undo/redo, if FALSE, don't keep track of changes
   BOOL  m_fFastRefresh;   // if TRUE when SyncToWorld() is called, all the views will be called and told to refresh immediately
                           // if FALSE, they will refresh when they wish

private:
   void NotifySocketsUndo (void);
   DWORD DefaultAttribIndex (GUID *pgObjectWorld, BOOL fExact = TRUE);

   PCWorld        m_pWorld;         // current world
   PCScene        m_pCurScene;      // current scene that working on
   fp             m_fCurTime;       // current time in the current scene
   BOOL           m_fIgnoreWorldNotify;   // ignore world notifications
   BOOL           m_fNeedToSyncToWorld;   // if TRUE needs to call SyncToWorld() when can
   DWORD          m_dwLastDefault;  // last default index - used for faster attribute gets

   CListFixed     m_lPCScene;       // list of scenes
   CUndoScenePacket    *m_pUndoCur;          // current undo that writing into
   CListFixed     m_listSCENESEL;   // list of SCENESELs, NOT sorted, of what's selected
   CListFixed     m_listViewSceneSocket;   // list of pointers to world sockets that get notified of change
   CListFixed     m_listPCUndoScenePacket;  // list of undo packets. Lowest # index are oldest
   CListFixed     m_listPCRedoScenePacket;  // list of PCUndoPacket. Lowest # index are oldest

   BOOL           m_fWorldChangedCompletely; // sent if the world has changed completely and need to resync all objects
   CListFixed     m_lWorldObjectGUID;  // list of world object GUIDs that have changed since last time called SyncWithWorld

   CListFixed     m_lSSDATTRIB; // list of SSDATTRIB, sorted by GUID gObjectWorld

   // camera
   BOOL           m_fCamera;        // set to TRUE if a camera (other than view) has been set
   GUID           m_gCamera;        // camera object. Only valid if m_fCamera
};
typedef CSceneSet * PCSceneSet;



/***************************************************************************************
CScene - Handles collections of changes needed for scene
*/
DLLEXPORT fp AngleClosestModulo (fp fA, fp fB);
DLLEXPORT BOOL CameraEnum (PCWorldSocket pWorld, PCListFixed pl);

class CSceneObj;
typedef CSceneObj *PCSceneObj;

class DLLEXPORT CScene {
public:
   ESCNEWDELETE;

   CScene (void);
   ~CScene (void);

   void SceneSetSet (PCSceneSet pSet); // so scene knows what's using
   PCSceneSet SceneSetGet (void);
   PCWorld WorldGet (void);
   void GUIDSet (GUID *pgScene); // changes the scene's guid
   void GUIDGet (GUID *pgScene); // get the scene's guid
   void NameSet (PWSTR pszName); // set the scene's name
   PWSTR NameGet (void);   // get the scene's name. DONT change the string
   fp DurationGet (void);
   void DurationSet (fp fDuration);
   DWORD FPSGet (void);
   void FPSSet (DWORD dwFPS);
   fp ApplyGrid (fp fValue);

   CScene *Clone (void);   // clone this and all objects in it
   PCMMLNode2 MMLTo (void); // write out MML for this
   BOOL MMLFrom (PCMMLNode2 pNode);  // read from MML
   void Clear (void);   // clears out contents of scene

   DWORD ObjectNum (void); // returns the number of objects in the scene
   PCSceneObj ObjectGet (DWORD dwIndex);
   BOOL ObjectRemove (DWORD dwIndex);
   DWORD ObjectAdd (GUID *pgObjectWorld);   // add an object to watch
   DWORD ObjectFind (GUID *pgObjectWorld);   // find the object index
   PCSceneObj ObjectGet (GUID *pgObjectWorld, BOOL fCreateIfNotExist = FALSE);

   void SyncFromWorld (DWORD dwNum, GUID *pagObjects); // sync the following object IDs up. ig pagObjects == NULL then sync all objects in world (not just in scene)
   void SyncToWorld (DWORD dwNum, GUID *pagObjects); // causes the given objects to set their attributes to the world objects, if pagObjects==NULL then all objects stored

   BOOL BookmarkEnum (PCListFixed pl, BOOL fInstant = TRUE, BOOL fDuration = TRUE);
   BOOL CameraEnum (PCListFixed pl);
   BOOL WaveFormatGet (DWORD *pdwSamplesPerSec, DWORD *pdwChannels);

private:
   PCSceneSet     m_pSceneSet;   // scene set the scene is in
   
   // saved to MML
   GUID           m_gGUID;       // ID for the scene
   CMem           m_memName;     // name of the scene
   CListFixed     m_lPCSceneObj;  // listof pointers to scene objectes. SORTED by object's GUID
   fp             m_fDuration;   // duration of the scene
   DWORD          m_dwFPS;       // frames per second. if 0 then none
};
typedef CScene *PCScene;


/***************************************************************************************
CSceneObj - Stores information about an object appearing in a scene.
*/
class CAnimAttrib;
typedef CAnimAttrib *PCAnimAttrib;

class DLLEXPORT CSceneObj {
public:
   ESCNEWDELETE;

   CSceneObj (void);
   ~CSceneObj (void);

   void SceneSet (PCScene pScene);     // tells the scene object what to use
   PCScene SceneGet (void);
   PCSceneSet SceneSetGet (void);
   PCWorld WorldGet (void);
   void GUIDSet (GUID *pgObjectWorld); // sets the object that this affects
   void GUIDGet (GUID *pgObjectWorld); // gets the object that this affects

   CSceneObj *Clone (PCScene pScene);   // clone this and all objects in it
   PCMMLNode2 MMLTo (void); // write out MML for this
   BOOL MMLFrom (PCMMLNode2 pNode);  // read from MML

   DWORD ObjectNum (void);    // returns number of animation objects
   PCAnimSocket ObjectGet (DWORD dwIndex);   // returns animation socket
   DWORD ObjectFind (GUID *pgObjectAnim); // finds by ID for object
   BOOL ObjectRemove (DWORD dwIndex, BOOL fDontRememberForUndo = FALSE);  // remove the object, also notifies CSceneSet of change
   DWORD ObjectAdd (PCAnimSocket pObject, BOOL fDontRememberForUndo = FALSE,
      GUID *pgID = NULL);  // add an object
   PCAnimSocket ObjectSwapForUndo (DWORD dwIndex, PCAnimSocket pSwap);
   BOOL WaveFormatGet (DWORD *pdwSamplesPerSec, DWORD *pdwChannels);

   void SyncFromWorld (void);
   void SyncToWorld (void);   // note - Will also need to check for attributes
      // with defaults but which are not represented in CAnimAttribSpline list

   void AnimAttribDirty (void);   // invalidate all the animattrib - called by CSceneSet when
      // an object in world sends change notification
   BOOL AnimAttribGenerate (void);  // generates the animation attributes if they're not already valid
   void AnimAttribGenerateWithout (PCAnimSocket pasIgnore);
   BOOL GraphMovePoint (PWSTR pszAttrib, PTEXTUREPOINT ptOld,
                                 PTEXTUREPOINT ptNew, DWORD dwLinear);

   PCAnimAttrib AnimAttribGet (DWORD dwIndex); // get by index
   PCAnimAttrib AnimAttribGet (PWSTR pszName, BOOL fCreateIfNotExist = FALSE);  // get an attribute based on name
   DWORD AnimAttribNum (void);   // returns number of animattibs

private:
   PCScene        m_pScene;

   // stored in mml
   CListFixed     m_lPCAnimSocket;  // list of animation objects within this, NOT sorted
   GUID           m_gObjectWorld;   // world object used

   // calculated
   BOOL           m_fAnimAttribValid;  // true if the items in the list valid, false if invalid
   CListFixed     m_lPCAnimAttrib;  // list of current animation attributes, sorted by attribute name
   PCAnimSocket   m_pAnimAttribWithout;   // one to ignore when generating attributes
};
typedef CSceneObj *PCSceneObj;


/***************************************************************************************
CAnimAttrib - Object that stores the animatable attribute information that has
been generated
*/
class DLLEXPORT CAnimAttrib {
   friend class CSceneObj;

public:
   ESCNEWDELETE;

   CAnimAttrib (void);
   ~CAnimAttrib (void);

   void Clear (void);   // clear out the contents

   CAnimAttrib *Clone (void);

   DWORD PointNum (void);
   BOOL PointGet (DWORD dwIndex, PTEXTUREPOINT pt, DWORD *pdwLinear);   // gets point
   BOOL PointSet (DWORD dwIndex, PTEXTUREPOINT pt, DWORD dwLinear);   // set the point. if pt.h would mess up sort then resorts
   BOOL PointRemove (DWORD dwIndex);
   DWORD PointClosest (fp fTime, int iDir);  // gets closest point. iDir=-1 to left, iDir=0 any dir, iDir=1 to right
   void PointAdd (PTEXTUREPOINT pt, DWORD dwLinear);   // add a point. If one exists at the same time-frame then overwrite

   fp ValueGet (fp fTime);    // gets a value at a specified time. Does spline calculations

   // should be set by creator
   fp             m_fMin;        // minimum value according to ATTRIBINFO
   fp             m_fMax;        // maximum value according to ATTRIBINFO
   DWORD          m_dwType;      // one of AIT_XXX. Interoplation is affected by the type
   WCHAR          m_szName[64];    // name. Should only be changed by CSceneObj

private:
   CListFixed     m_lAAPOINT;      // list of points for information. sorted by .h (time)
};
typedef CAnimAttrib *PCAnimAttrib;


/***************************************************************************************
CTimeline */
DLLEXPORT void TimelineTicks (HDC hDC, RECT *pr, PCMatrix pmPixelToTime, PCMatrix pmTimeToPixel,
                    COLORREF crMinor, COLORREF crMajor, DWORD dwFramesPerSec);
DLLEXPORT void TimelineHorzTicks (HDC hDC, RECT *pr, PCMatrix pmPixelToTime, PCMatrix pmTimeToPixel,
                    COLORREF crMinor, COLORREF crMajor, DWORD dwType);


class DLLEXPORT CTimeline {
   friend LRESULT CALLBACK CTimelineWndProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
public:
   ESCNEWDELETE;

   CTimeline (void);
   ~CTimeline (void);

   BOOL Init (HWND hWndParent, COLORREF cBack, COLORREF cBackShadow,
      COLORREF cLight, COLORREF cShadow, COLORREF cOutline, DWORD dwMsg);
   void Move (RECT *pr);

   BOOL LimitsSet (fp fLeft, fp fRight, fp fScene);
   void LimitsGet (fp *pfLeft, fp *pfRight, fp *pfScene);
   BOOL PointerSet (fp fVal);
   fp PointerGet (void);
   BOOL ScaleSet (fp fLeft, fp fRight);

private:
   LRESULT WndProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

   HWND           m_hWnd;        // window that drawing in
   COLORREF       m_cBack;       // background color
   COLORREF       m_cBackShadow; // shadow of background
   COLORREF       m_cLight;      // light color
   COLORREF       m_cMed;        // medium color
   COLORREF       m_cShadow;     // shadow color
   COLORREF       m_cOutline;    // outline color
   DWORD          m_dwMsg;       // message to send when user has moved slider
   DWORD          m_dwCapture;   // 0 = if mouse not captured, 1 for move main ptr,
                                 // 2 for move left limit, 3 for right limit, 4 fpr move limit scene

   fp             m_fLimitLeft;  // lowest value that playback will have
   fp             m_fLimitRight; // highest value that playbak will have
   fp             m_fLimitScene; // limit of the scene
   fp             m_fPointer;    // pointer location
   fp             m_fScaleLeft;  // value of pointer/limit if all the way on left display
   fp             m_fScaleRight; // value of pointer/limit if all the way on the right display

};
typedef CTimeline *PCTimeline;

/******************************************************************************************
CSceneView */
DLLEXPORT void SceneViewShutDown (void);
DLLEXPORT void SceneViewCreate (PCWorldSocket pWorld, PCSceneSet pSceneSet);
DLLEXPORT PCListFixed ListPCSceneView (void);

extern CListFixed gListPCSceneView;

// SVBAY - structure saying what's in each of the scene view bays (rows)
class CSceneView;
typedef CSceneView *PCSceneView;
typedef struct {
   DWORD          dwType;        // type of information displayed. 0 means none, 1 means world object
   GUID           gWorldObject;  // object that's being shown
   fp             fVPosMin;      // vertical position at the top of the window
   fp             fVPosMax;      // vertical position at the bottom of the window
   
   // extents that can scroll
   fp             fScrollMin;    // smallest amount can scroll to, ATTRIBINFO.iMin
   fp             fScrollMax;    // largest amount can scroll to, ATTRIBINFO.iMax
   BOOL           fScrollFlip;   // if TRUE flip the direction of scrolling

   CMatrix        mPixelToTime;  // converts a pixel to time, [0] = h-pixel1, [1]=h-pixel2, [2]=v-pixel
   CMatrix        mTimeToPixel;   // converts from time into a pixel, [0] = time1, [1]=time2, [2]=vpos

   HWND           hWndInfo;      // info window
   HWND           hWndData;      // data window
   HWND           hWndList;      // list box
   PCListFixed    plATTRIBVAL;   // list of attributes. fValue==0 => not shown, else shown
   PCSceneView    pSceneView;    // just point back so know

   BOOL           fSelected;     // temporary flag used to determine if on m_pWorld selection list
   DWORD          dwOrigPosn;    // temporary flag used for rearranging items
   DWORD          dwAge;         // getTickCount() - When first appeared on list
} SVBAY, *PSVBAY;

#define WVPLAYBUF          8  // number of play buffers

class DLLEXPORT CSceneView : public CViewSocket, public CViewSceneSocket {
   friend LRESULT CALLBACK CSceneViewWndProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
   friend LRESULT CALLBACK CSceneViewBayInfoWndProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
   friend LRESULT CALLBACK CSceneViewBayDataWndProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

public:
   ESCNEWDELETE;

   CSceneView (void);
   ~CSceneView (void);

   BOOL SceneSet (PCWorldSocket pWorld, PCSceneSet pSceneSet);

   // CViewSocket
   virtual void WorldChanged (DWORD dwChanged, GUID *pgObject);
   virtual void WorldUndoChanged (BOOL fUndo, BOOL fRedo);
   virtual void WorldAboutToChange (GUID *pgObject);

   // CViewSceneSocket
   virtual void SceneChanged (DWORD dwChanged, GUID *pgScene, GUID *pgObjectWorld, GUID *pgObjectAnim);
   virtual void SceneUndoChanged (BOOL fUndo, BOOL fRedo);
   virtual void SceneStateChanged (BOOL fScene, BOOL fTime);

//private:
   LRESULT WndProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
   LRESULT BayInfoWndProc (PSVBAY pb, HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
   LRESULT BayDataWndProc (PSVBAY pb, HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
   LRESULT ButtonDown (PSVBAY pb, HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, DWORD dwButton);
   LRESULT ButtonUp (PSVBAY pb, HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, DWORD dwButton);
   LRESULT MouseMove (PSVBAY pb, HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
   LRESULT Paint (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
   LRESULT TestForAccel (PSVBAY pb, HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
   void UpdateNumBays (DWORD dwWant, BOOL fUpdateDisplay);
   void SetTitle (void);
   void ClipboardUpdatePasteButton (void);
   void SetProperCursor (PSVBAY pBay, int iX, int iY);
   void SetPointerMode (DWORD dwMode, DWORD dwButton);
   void ModifyBaysFromSelection (void);
   void CalcBayMatrix (PSVBAY pBay);
   void PaintAnimObject (HDC hDC, RECT *prBayData, PSVBAY pb, PCAnimSocket pas);
   BOOL MoveOutOfPurgatory (PSVBAY pb, fp fTime, fp fVertOrig, GUID *pgObjectAnim = NULL);
   PCAnimSocket MouseToObject (PSVBAY pb, int iX, int iY, int *piOver = NULL);
   void InvalidateBayData (void);
   BOOL ClipboardDelete (BOOL fUndoSnapshot);
   BOOL ClipboardPaste (void);
   BOOL ClipboardCut (void);
   BOOL ClipboardCopy (void);
   void NewScrollLoc (void);
   void NewScrollLoc (PSVBAY pb);
   BOOL IsAttribInteresting (PCSceneObj pSceneObj, PWSTR pszAttrib);
   BOOL BaySelectAttrib (PSVBAY pb, PWSTR pszAttrib);
   void BaySelectDefault (PSVBAY pb);
   void BaySelectToMinMax (PSVBAY pb);
   void BayPaintGraph (PSVBAY pb, HDC hDC, RECT *pr);
   PCAnimAttrib GraphClosest (PSVBAY pb, int iX, int iY, fp *pfTime, fp *pfValue,
      DWORD *pdwLinear, BOOL fApplyGrid);
   BOOL GraphMovePoint (PSVBAY pb, PWSTR pszAttrib, PTEXTUREPOINT ptOld,
                                 PTEXTUREPOINT ptNew, DWORD dwLinear);
   BOOL PlayAddBuffer (PWAVEHDR pwh);

   HWND           m_hWnd;  // main window handle
   HWND           m_hWndScroll; // horizontal time scroll
   PCTimeline     m_pTimeline;
   HWND           m_hWndClipNext; // clipboard viewer
   HFONT          m_hFont;    // small font to display with
   CListFixed     m_lSVBAY;   // list of SVBAY structures
   PCButtonBar    m_pbarTop;  // top button bar
   PCButtonBar    m_pbarBottom;     // bottom button bar
   COLORREF       m_cBackDark;   // dark background color (not in tab)
   COLORREF       m_cBackMed;    // medium background color (for tab not selected)
   COLORREF       m_cBackLight;  // light background color (for selected tab)
   COLORREF       m_cBackOutline;   // outline color
   PCIconButton   m_pUndo, m_pRedo; // undo and redo buttons
   PCIconButton   m_pPaste;         // paste button
   PCIconButton   m_pPlay, m_pPlayLoop, m_pStop; // play and stop
   BOOL           m_fSmallWindow;   // TRUE if using small buttons
   DWORD          m_dwClipFormat;   // clipboard format for scene information
   DWORD          m_dwButtonDown;   // which button is down, 0 for none
   DWORD          m_adwPointerMode[3];  // what the meaning of the pointer is. IDC_XXX. [0]=left,[1]=middle,[2]=right
   BOOL           m_fCaptured;      // true if mouse is captured
   POINT          m_pntButtonDown;  // where the button went down
   POINT          m_pntMouseLast;   // last mouse location
   CPoint         m_pButtonDown;    // time (.p[0]) and location (.p[2]) where mouse when down
   CPoint         m_pMouseLast;     // time (.p[0]) and location (.p[2]) where mouse was last
   CPoint         m_pZoomOrig;      // original zoom when clicked down at m_pntButtonDown
   PSVBAY         m_pTimerAutoScroll; // set to the bay that can autoscroll if timer is on. NULL if timer is off
   PCAnimSocket   m_pPasteDrag;     // object being paste-dragged or resized
   int            m_iResizeSide;    // used when resizing. 0 for center, -1 for left, 1 for right
   fp             m_fResizeDelta;   // delta to take into account not clicking exactly on edge

   PCWorldSocket  m_pWorld;      // world
   PCSceneSet     m_pSceneSet;   // scene
   CListFixed     m_listNeedSelect; // list of buttons that are enabled or disabled based on selection
   PCScene        m_pPurgatory;  // store objects-to-be inserted and pasted

   // playing
   DWORD          m_dwPlaying;      // 0 if stopped, 1 if play straight, 2 if play loop
   DWORD          m_dwPlayTime;     // GetTickCount() at last time that set frame
   fp             m_fPlayLastFrame; // frame that was SUPPOSED to set at m_dwPlayTime, may
                                    // have been slightly different because of the grid

   // timeline info
   fp             m_fViewTimeStart; // left edge of window is this time
   fp             m_fViewTimeEnd;   // right edge of window is this time

   // selection draw info
   BOOL           m_fSelDraw;       // TRUE if should draw selection
   PSVBAY         m_pSelBay;        // selection bay, or NULL if all bays
   fp             m_fSelStart;      // selection start, in sec
   fp             m_fSelEnd;        // selection end, in sec. Could be < m_fSelStart or >.
   COLORREF       m_cSelColor;      // color of the selection rectangle

   // moving graph CP
   BOOL           m_fMCP;           // set to TRUE if moving a graph control point
   WCHAR          m_szMCPAttrib[64]; // attribute
   TEXTUREPOINT   m_tpMCPDelta;     // add this to current mouse location to account for click offset
   TEXTUREPOINT   m_tpMCPOld;       // old location of the point
   DWORD          m_dwMCPLinear;    // if linear or not

   // playing audio
   HWAVEOUT       m_hWaveOut;       // handle to wave out
   PCAnimSocket   m_pAnimWave;      // if not NULL then get animation info from this
   PCSceneObj     m_pObjWave;       // if not NULL then get audio from this
   DWORD          m_dwWaveSamplesPerSec;  // sampling rate using
   DWORD          m_dwWaveChannels; // number of channels using
   WAVEHDR        m_aPlayWaveHdr[WVPLAYBUF]; // headers for playback
   CMem           m_memPlay;           // memory for playback
   DWORD          m_dwPlayBufSize;     // playback buffer size, in bytes
   float          *m_pafPlayTemp;      // temporary buffer for summing
   double         m_fPlayCur;       // current play position, in seconds
};
typedef CSceneView *PCSceneView;




/*************************************************************************
Animation */

// ANIMINFO - Information necessary to do an animation
typedef struct {
   // must be filled in by caller
   PCWorldSocket        pWorld;     // world animating with. Must be filled in by caller.
   PCSceneSet           pSceneSet;  // scene set using. Must be filled in by caller. If NULL then no animation is done
   PCRenderTraditional  pRender;    // renderer to base off of.
   PCMem                pmemWFEX;   // pointer to a CMem that will contain the WAVEFORMATEX structure for the
                                    // digital audio to use. The caller into AnimInfoDialog() can initialize this
                                    // filling in the memory and setting m_dwCurPosn to the size of the WAVEFORMATEX structure.
                                    // If initialized, the UI will default to that wave format. If not, a good format is
                                    // automatically chosen.
                                    // On existing AnimInfoDialog () this will either have a valid WAVEFORMATEX and m_dwCurPosn.

   // filled in by caller or AnimInfoDialog()
   PCScene              pScene;     // scene within pSceneSet. If NULL then no animtion is done
   GUID                 gCamera;    // camera object to use, or GUID_NULL if default camera from renderer
   fp                   fTimeStart; // start time
   fp                   fTimeEnd;   // end time. if == fTimeStart then just one snapshot
   DWORD                dwPixelsX;  // resoltion
   DWORD                dwPixelsY;  // resolution
   DWORD                dwQuality;  // quality setting, from CHouseView quality numbers,
                                    // higher numbers also include ray tracing levels, 6..8 are ray tracing qualities
   DWORD                dwAnti;     // 1 = none, 2=2pixel, 3=3pixel, 4=4pixel
   DWORD                dwFPS;      // frames per second
   fp                   fExposureTime; // if override, the new exposure time
   DWORD                dwExposureStrobe; // if override, the number of strobes
   WCHAR                szFile[256];   // file to save to. If .avi will save as .avi, can also
                                    // be .jpg or .bmp, in which case if it's a single image will
                                    // use name exactly. If more than one will create numbered frames.
                                    // will be ignored if fToBitmap is set
   BOOL                 fToBitmap;  // if TRUE (and should only be true for single frame) then
                                    // draws to a bitmap (hToBitmap). Caller must delete this
   DWORD                dwAVICompressor;  // compressor to use. 0 if none
   BOOL                 fIncludeAudio; // if TRUE then include audio in the AVI
   GUID                 gEffectCode;   // major effect code
   GUID                 gEffectSub;    // minor effect code

   // return info
   HBITMAP              hToBitmap;  // if fToBitmap is set, this will be filled with a handle
                                    // to the bitmap

   // internally used
   HBITMAP              hBitEffect; // bitmap showing the effect
} ANIMINFO, *PANIMINFO;


DLLEXPORT BOOL AnimInfoDialog (HWND hWnd, PCWorldSocket pWorld, PCSceneSet pSceneSet, PCRenderTraditional pRender,
                     PCMem pmemWFEX, PANIMINFO pAI);
DLLEXPORT BOOL AnimAnimate (DWORD dwRenderShard, PANIMINFO pai);
DLLEXPORT BOOL AnimAnimateWait (DWORD dwRenderShard, PANIMINFO pai);




/********************************************************************************
CNoise2D - 2d noise object */
class DLLEXPORT CNoise2D {
public:
   ESCNEWDELETE;

   CNoise2D (void);
   ~CNoise2D (void);
   BOOL Init (DWORD dwGridX, DWORD dwGridY);
   fp Value (fp h, fp v);

private:
   DWORD       m_dwGridX;        // number across in x
   DWORD       m_dwGridY;        // number across in y
   CMem        m_memData;        // memory containing values
   //PCPoint     m_pRand;          // random data, [m_dwGridY][m_dwGridX]
   float       *m_pRand;         // random data,  [m_dwGridY][m_dwGridX]
};
typedef CNoise2D *PCNoise2D;



/********************************************************************************
CNoise3D - 3d noise object */
class DLLEXPORT CNoise3D {
public:
   ESCNEWDELETE;

   CNoise3D (void);
   ~CNoise3D (void);
   BOOL Init (DWORD dwGridX, DWORD dwGridY, DWORD dwGridZ);
   fp Value (fp x, fp y, fp z);

private:
   DWORD       m_dwGridZ;        // number in Z
   CListFixed  m_lPCNoise2D;     // list of m_dwGridZ 2d noise entries
};
typedef CNoise3D *PCNoise3D;


/***********************************************************************************
CGroundView */

// GVSTATE - Stores information about the current state. Can be duplicated for undo and redo
typedef struct {
   DWORD             dwWidth;    // width
   DWORD             dwHeight;   // height
   TEXTUREPOINT      tpElev;         // .h is minimum elevation (at 0), .v is max elevation (at 0xffff)
   fp                fScale;         // number of meters per square. total width = (m_dwWidth-1)*m_fScale. Likewise for height
   WORD              *pawElev;        // pointer to elevation information. [x + m_dwWidth * y]
   PCMem             pmemElev;      // pointer to memory containing the elevation info

   // textures
   BYTE              *pabTextSet;   // pointer to texture inforamtion. [x + m_dwWidth*y + dwTextNum * dwWidth * dwHeight]
   PCMem             pmemTextSet;   // pointer to memory containing the texture information
   PCListFixed       plTextSurf;    // pointer to list containing PCObjectSurface for texture
   PCListFixed       plTextColor;   // pointer to list containing COLORREF for texutre. same number elem as plTextSurf

   // forests
   BYTE              *pabForestSet; // pointer to forest inforamtion. [x + m_dwWidth*y + dwForestNum * dwWIdth * dwHeight]
   PCMem             pmemForestSet; // pointer to memory containing the forest information
   PCListFixed       plPCForest;    // pointer to list containing PCForest for texture

   // water
   BOOL              fWater;         // if TRUE, diplay water
   fp                fWaterElevation;   // water elevation, in meters
   fp                fWaterSize;     // how large to draw water, in meters, if 0 then size of ground
   PCObjectSurface   apWaterSurf[GWATERNUM*GWATEROVERLAP]; // water surface
   fp                afWaterElev[GWATERNUM]; // water elevation at each surface
} GVSTATE, *PGVSTATE;

#define GVCOLORSCHEME      5        // # of points in the groundview color scheme (for topography)

class DLLEXPORT CGroundView {
   friend LRESULT CALLBACK CGroundViewWndProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
   friend LRESULT CALLBACK CGroundViewMapWndProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
   friend LRESULT CALLBACK CGroundViewKeyWndProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
public:
   ESCNEWDELETE;

   CGroundView(PCWorldSocket pWorld, GUID *pgObject);
   ~CGroundView(void);

   BOOL Init (DWORD dwMonitor = (DWORD)-1);
   CGroundView *Clone (DWORD dwMonitor = (DWORD)-1);
   void UpdateAllButtons (void);
   BOOL ButtonExists (DWORD dwID);
   void SetPointerMode (DWORD dwMode, DWORD dwButton, BOOL fInvalidate = TRUE);
   void SetProperCursor (int iX, int iY);
   BOOL Save (HWND hWndParent);

   void NewView (DWORD dwView);
   void InvalidateDisplay (void);
   BOOL PointInImage (int iX, int iY, fp *pfImageX = NULL, fp *pfImageY = NULL);
   void Elevation (DWORD dwX, DWORD dwY, PCPoint pElev);
   void Nornal (DWORD dwX, DWORD dwY, PCPoint pNorm);
   BOOL HVToElev (fp fH, fp fV, WORD *pwElev);
   void TicksHorizontal (HDC hDC, RECT *prText, RECT *pr, COLORREF crMinor, COLORREF crMajor, BOOL fKey = FALSE);
   void TicksVertical (HDC hDC, RECT *prText, RECT *pr, COLORREF crMinor, COLORREF crMajor);
   void MapScrollUpdate (void);
   void CalcWaterColorScheme (void);
   void UpdateTextList (void);
   void UpdateForestList (void);
   
   void UndoUpdateButtons (void);
   void UndoAboutToChange (void);
   void UndoCache (void);
   BOOL Undo (BOOL fUndo);

   void BrushApply (DWORD dwPointer, POINT *pNew, POINT *pPrev);
   void GroundGenMtn (BOOL fDetailed, DWORD dwType, DWORD dwWidth,  DWORD dwHeight,
      POINT *pCenter, DWORD dwRadius, float *pafHeight);
   void GroundGenPlateau (BOOL fDetailed, DWORD dwType, DWORD dwWidth,  DWORD dwHeight,
      POINT *pCenter, DWORD dwRadius, float *pafHeight);
   void GroundGenRaise (BOOL fDetailed, DWORD dwType, DWORD dwWidth,  DWORD dwHeight,
      POINT *pCenter, DWORD dwRadius, float *pafHeight);
   void GroundGen (BOOL fDetailed);
   void ApplyRain (void);
   BOOL UpDownSample (fp fScale);
   BOOL ChangeSize (DWORD dwWidth, DWORD dwHeight);

   void TracePixelToWorldSpace (fp dwX, fp dwY, PCPoint p);
   void TraceWorldSpaceToPixel (PCPoint pWorld, fp *pfX, fp *pfY);
   fp IsPixelPainted (DWORD dwX, DWORD dwY);

   PCWorldSocket  m_pWorld;      // world that viewing
   GUID           m_gObject;     // object that viewing
   GVSTATE        m_gState;      // current state
   BOOL           m_fDirty;      // set to TRUE if state has changed since last save
   BOOL           m_fUndoDirty;  // set to TRUE if state has changed since was last copied to undo buffer
   CListFixed     m_lGVSTATEUndo;   // undo
   CListFixed     m_lGVSTATERedo;   // redo
   BOOL           m_fWorkingWithUndo;  // if TRUE, then any new changes will be ignored because
                                       // already have an undo in m_lGVSTATEUndo
   DWORD          m_dwView;      // 0 = topo, 1 = text, 2 =forest
   DWORD          m_dwCurText;   // current texture being used. index into m_gState.plTextSurf
   DWORD          m_dwCurForest;  // current forest being used.

   DWORD          m_dwBrushAir;  // 0 for pen, 1 for brush, 2 for airbrush
   DWORD          m_dwBrushPoint;   // 0 for flat, 1 for rounded, 2 for pointy, 3 for very pointy
   DWORD          m_dwBrushStrength;   // 1..10
   DWORD          m_dwBrushEffect;  // 0 for raise, 1 for lower, 2 raise to max, 3 lower to max,
                                    // 4 blur, 5 sharpen, 6 noise
   BOOL           m_fAirbrushTimer; // set to TRUE if airbrush timer is on

   COLORREF       m_cBackDark;   // dark background color (not in tab)
   COLORREF       m_cBackMed;    // medium background color (for tab not selected)
   COLORREF       m_cBackLight;  // light background color (for selected tab)
   COLORREF       m_cBackOutline;   // outline color
   COLORREF       m_acColorScheme[GVCOLORSCHEME];  // color scheme for translation of elevation to color
   COLORREF       m_acColorSchemeWater[GVCOLORSCHEME];   // color scheme for water
   WORD           m_wWaterMark;  // where the water begins, from 0 to 0xffff

   fp             m_fGridUDMinor;      // size of minor UD grid
   fp             m_fGridUDMajor;      // size of major UD grid
   CPoint         m_pLight;         // normalized vector pointing towards the light

   CListFixed     m_lTraceInfo;     // tracing paper being used
   POINT          m_pTraceOffset;   // how much the UL of the image is offset from 0,0 of the screen

   TEXTUREPOINT   m_tpViewUL;       // upper-left corner coords of view, in ground pixels, m_dwWidth x m_dwHeight
   fp             m_fViewScale;     // number of screen pixels per ground pixel.

   HWND           m_hWnd;           // window handle
   HWND           m_hWndMap;        // map window handle
   HWND           m_hWndElevKey;        // key window handle
   HWND           m_hWndTextList;   // list of textures
   HWND           m_hWndForestList; //  list of forests
   HFONT          m_hFont;          // used to dipplay list box
   BOOL           m_fSmallWindow;   // if TRUE, draw as a small window
   PCButtonBar    m_pbarGeneral;  // button bar for general buttons
   PCButtonBar    m_pbarView;     // bar specific to the way the data is viewed
   PCButtonBar    m_pbarObject;   // bar speciifc to the object that's selected
   PCButtonBar    m_pbarMisc;     // miscellaneous
   PCIconButton   m_pUndo, m_pRedo; // undo and redo buttons

   DWORD          m_adwPointerMode[3];  // what the meaning of the pointer is. IDC_XXX. [0]=left,[1]=middle,[2]=right
   DWORD          m_dwPointerModeLast; // last pointer mode used, so can swap back and forth
   POINT          m_pntButtonDown;  // point on the screen where the button was pressed down
   BOOL           m_dwButtonDown;    // 0 if no button is down, 1 for left, 2 for middle, 3 for right
   BOOL           m_fCaptured;      // set to true if mouse captured
   POINT          m_pntMouseLast;   // if the button is down, this reflects where the button last was
   CImage         m_ImageTemp;      // used for painting
   CMem           m_memPaintBuf;    // temporary memory used for painting
   CMem           m_memGroundGenTemp;  // temporary memory for ground gen
   DWORD          m_dwGroundGenSeed;   // seed for ground gen
   RECT           m_rGroundRangeLast;  // range (in ground pixels) the last time was called. all zero's if not called
   BOOL           m_fPaintOn;       // set to true if painting based on selection and elevation are on
   TEXTUREPOINT   m_tpPaintElev;    // .h = ideal elevation from 0..1, .v = range from 0..1
   TEXTUREPOINT   m_tpPaintSlope;   // .h = ideal slope from 0..1, .v = range from 0..0
   fp             m_fPaintAmount;   // strength from 0 .. 1

private:
   LRESULT WndProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
   LRESULT MapWndProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
   LRESULT KeyWndProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
   LRESULT ButtonDown (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, DWORD dwButton);
   LRESULT ButtonUp (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, DWORD dwButton);
   LRESULT MouseMove (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
   LRESULT Paint (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
   LRESULT PaintMap (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
   LRESULT PaintKey (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

   BOOL StateClone (PGVSTATE pNew, PGVSTATE pOrig);
   void StateFree (PGVSTATE pState);
};

typedef CGroundView *PCGroundView;

DLLEXPORT BOOL GroundViewModifying (PCWorldSocket pWorld);
DLLEXPORT BOOL GroundViewShowHide (PCWorldSocket pWorld, GUID *pgObject, BOOL fShow);
DLLEXPORT BOOL GroundViewNew (PCWorldSocket pWorld, GUID *pgObject);
DLLEXPORT BOOL GroundViewDestroy (PCWorldSocket pWorld, GUID *pgObject);
DLLEXPORT PBYTE BrushFillAffected (DWORD dwBrushSize, DWORD dwBrushPoint, DWORD dwBrushAir,
                         POINT *pPixelPrev, POINT *pPixelNew, PCMem pMem, RECT *pr);




/***********************************************************************************
CPaintView */


class DLLEXPORT CPaintView {
   friend LRESULT CALLBACK CPaintViewWndProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
   friend LRESULT CALLBACK CPaintViewMapWndProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
public:
   ESCNEWDELETE;

   CPaintView(void);
   ~CPaintView(void);

   BOOL Init (DWORD dwMonitor = (DWORD)-1);
   BOOL NewScene (PCWorldSocket pWorld, PCRenderTraditional pRender);

   //CPaintView *Clone (DWORD dwMonitor = (DWORD)-1);
   void UpdateAllButtons (void);
   BOOL ButtonExists (DWORD dwID);
   void SetPointerMode (DWORD dwMode, DWORD dwButton, BOOL fInvalidate = TRUE);
   void SetProperCursor (int iX, int iY);
   BOOL Save (HWND hWndParent);

   void NewView (DWORD dwView);
   void InvalidateDisplay (void);
   BOOL PointInImage (int iX, int iY, fp *pfImageX = NULL, fp *pfImageY = NULL);
   void UpdateTextList (void);
   
   void UndoUpdateButtons (void);
   void UndoAboutToChange (PCTextureImage pImage);
   void UndoCache (void);
   BOOL Undo (BOOL fUndo);
   void FillTemplate (void);
   void FillImage (RECT *prFill);
   BOOL ChangeResolution (PCTextureImage pti, DWORD dwNewWidth, DWORD dwNewHeight);

   PCWorldSocket  m_pWorld;      // world that viewing

   PCRenderTraditional m_pRender;  // render engine to use
   CImage         m_Image;       // image to paint on for the master background
   CImage         m_ImagePaint;  // scratch for painting so can create bitmap
   CImage         m_ImageTemplate;  // image of the current template
   CMem           m_memRPPIXEL;  // memory containing the image
   CListFixed     m_lRPTEXTINFO; // list of textures in the image
   CPoint         m_pViewer;     // viewer location in world space
   CMatrix        m_mLightMatrix;   // matrix for converting from view space to  world space vectors
   PRPPIXEL       m_pPixel;      // pointer to pixels
   CListFixed     m_lPCTextureImage;   // list of texture images that have memorized for editing
   CListFixed     m_lTextureImageDirty;   // list of BOOLs indicating which of m_lPCTextureImage are dirty
   CListFixed     m_lPVTEMPLATE; // list of templates that are used for painting

   DWORD          m_dwView;      // 0 = topo, 1 = text, 2 =forest
   DWORD          m_dwBlendTemplate; // 0 to 256 - amount to blend the template in. 256 = max
   DWORD          m_dwCurTemplate;   // current texture being used. index into m_gState.plTextSurf
   CPoint         m_pLight;      // location of light, in viewer space
   CPoint         m_pLightWorld; // light vector, in world space. Calculated from m_pLight when draw

   DWORD          m_dwBrushAir;  // 0 for pen, 1 for brush, 2 for airbrush
   DWORD          m_dwBrushPoint;   // 0 for flat, 1 for rounded, 2 for pointy, 3 for very pointy
   DWORD          m_dwBrushStrength;   // 1..10
   DWORD          m_dwBrushEffect;  // 0 for raise, 1 for lower, 2 raise to max, 3 lower to max,
                                    // 4 blur, 5 sharpen, 6 noise
   BOOL           m_fAirbrushTimer; // set to TRUE if airbrush timer is on

   COLORREF       m_cBackDark;   // dark backPaint color (not in tab)
   COLORREF       m_cBackMed;    // medium backPaint color (for tab not selected)
   COLORREF       m_cBackLight;  // light backPaint color (for selected tab)
   COLORREF       m_cBackOutline;   // outline color

   HWND           m_hWnd;           // window handle
   HWND           m_hWndMap;        // map window handle
   HWND           m_hWndTextList;   // list of textures
   HWND           m_hWndBlend;      // window for blending
   HFONT          m_hFont;          // used to dipplay list box
   BOOL           m_fSmallWindow;   // if TRUE, draw as a small window
   PCButtonBar    m_pbarGeneral;  // button bar for general buttons
   PCButtonBar    m_pbarView;     // bar specific to the way the data is viewed
   PCButtonBar    m_pbarObject;   // bar speciifc to the object that's selected
   PCButtonBar    m_pbarMisc;     // miscellaneous
   PCIconButton   m_pUndo, m_pRedo; // undo and redo buttons

   CListFixed     m_lUndo;          // list of PCListFixed. Each CListFixed contains a list of PCTextureImage for undo
   CListFixed     m_lRedo;          // same as m_lUndo
   PCListFixed    m_plCurUndo;      // current (cached) undo. List contains PCTextureImage

   DWORD          m_adwPointerMode[3];  // what the meaning of the pointer is. IDC_XXX. [0]=left,[1]=middle,[2]=right
   DWORD          m_dwPointerModeLast; // last pointer mode used, so can swap back and forth
   POINT          m_pntButtonDown;  // point on the screen where the button was pressed down
   BOOL           m_dwButtonDown;    // 0 if no button is down, 1 for left, 2 for middle, 3 for right
   BOOL           m_fCaptured;      // set to true if mouse captured
   POINT          m_pntMouseLast;   // if the button is down, this reflects where the button last was
   CMatrix        m_mTemplateOrig;  // original template
   CMem           m_memPaintBuf;    // scratch memory
   CListFixed     m_lBLURPIXEL;     // information about pixel to be blurred or sharpened
   PCTextureImage m_pPaintOnly;     // if not NULL then paint only this texture image
   PCTextureImage m_pTextureTemp;   // use this to pass information to PaintDialog

private:
   LRESULT WndProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
   LRESULT MapWndProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
   LRESULT ButtonDown (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, DWORD dwButton);
   LRESULT ButtonUp (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, DWORD dwButton);
   LRESULT MouseMove (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
   LRESULT Paint (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
   LRESULT PaintMap (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
   void ShaderPixel (PIMAGEPIXEL pip, PCPoint pView, PCPoint pLoc, PCPoint pNorm,
                              PCPoint pLight, PTEXTUREPOINT ptp, PCTextureImage pInfo,
                              DWORD x, DWORD y);
   BOOL SameSurface (PRPPIXEL pCompare, DWORD x, DWORD y, PTEXTUREPOINT pTP);
   BOOL PixelBump (DWORD dwThread, PCTextureImage pInfo, const PTEXTUREPOINT pText, const PTEXTUREPOINT pRight,
                             const PTEXTUREPOINT pDown, const PTEXTUREPOINT pSlope, fp *pfHeight = NULL, BOOL fHighQuality = FALSE);
   HBITMAP BitmapFromItem (HDC hDC, DWORD dwIndex);
   void TextureImageInterp (PCTextureImage pImage, PTEXTUREPOINT ptp,
                                     WORD *pwRGB, float *pfGlow, WORD *pwTrans, fp *pfBump,
                                     WORD *pwSpecReflect, WORD *pwSpecPower);
   BOOL CopyOfRegion (RECT *pr, DWORD dwScale);
   void PaintTriRegion (PTEXTUREPOINT patpDest, PCTextureImage pDest,
                                 PTEXTUREPOINT patpSrc, PCTextureImage pSrc,
                                 PBYTE pabWeight, BOOL fNoColor, BOOL fPaintIgnoreTrans, BOOL fBumpAdds);
   void PaintPoint (int ix, int iy, PCTextureImage pDest,
                             PTEXTUREPOINT pt, PCTextureImage pSrc, fp fWeight,
                             BOOL fNoColor, BOOL fPaintIgnoreTrans, BOOL fBumpAdds);
   void PaintRegion (RECT *pr, PBYTE pbPaint);
   void BrushApply (DWORD dwPointer, POINT *pNew, POINT *pPrev);
   void PaintBlur (void);
};

typedef CPaintView *PCPaintView;

DLLEXPORT BOOL PaintViewModifying (PCWorldSocket pWorld);
DLLEXPORT void PaintViewShowHide (BOOL fShow);
DLLEXPORT void PaintViewNew (PCWorldSocket pWorld, PCRenderTraditional pRender);



/******************************************************************************
CObjectClone */
class DLLEXPORT CObjectClone : private CRenderSocket {
public:
   ESCNEWDELETE;

   CObjectClone (void);
   ~CObjectClone (void);         // note: dont call this. call release

   BOOL Init (DWORD dwRenderShard, GUID *pgCode, GUID *pgSub);
   DWORD AddRef (void);
   DWORD Release (void);
   void Render (POBJECTRENDER pr, DWORD dwID, PCMatrix pmApply = NULL, fp fDetail = -1); 
   BOOL UpdateToNewObject (BOOL fReloadObject);
   BOOL BoundingBoxGet (CMatrix *pm, PCPoint pCorner1, PCPoint pCorner2); // get the bounding box
   BOOL TextureQuery (PCListFixed plText);
   BOOL ColorQuery (PCListFixed plColor);
   BOOL ObjectClassQuery (PCListFixed plObj);
   fp MaxSize (void);


   // can read but dont change
   GUID           m_gCode;       // code-guid for the object
   GUID           m_gSub;        // sub-guid for the object
   DWORD          m_dwRenderShard;  // render shard

private:
   DWORD          m_dwRefCount;  // after init is 1. If reaches 0 (with release) then will delete itself
   CWorld         m_World;       // contains the object
   fp             m_fDetail;     // return this value when QueryDetail() called
   PCMatrix       m_pmApply;     // matrix to apply to all matrix set calls
   PCRenderSocket m_pRender;     // pass render into this
   BOOL           m_fInLoop;     // see if in infinite loop. Fail if in it.
   fp             m_fMaxSize;    // maximum distance from 0,0,0 for any point
   DWORD          m_dwID;        // ID to use

   // from render socket
   virtual BOOL QueryWantNormals (void);    // returns TRUE if renderer wants normal vectors
   virtual BOOL QueryWantTextures (void);   // returns TRUE if the renderer wants textures
   virtual fp QueryDetail (void);       // returns detail resoltion (in meters) that renderer wants now
   virtual void MatrixSet (PCMatrix pm);    // sets the current operating matrix. Set NULL implies identity.
   virtual void PolyRender (PPOLYRENDERINFO pInfo);
   virtual BOOL QuerySubDetail (PCMatrix pMatrix, PCPoint pBound1, PCPoint pBound2, fp *pfDetail);
   virtual BOOL QueryCloneRender (void);
   virtual BOOL CloneRender (GUID *pgCode, GUID *pgSub, DWORD dwNum, PCMatrix pamMatrix);

};
typedef CObjectClone *PCObjectClone;

DLLEXPORT PCObjectClone ObjectCloneGet (DWORD dwRenderShard, GUID *pgCode, GUID *pgSub, BOOL fCreateIfNotExist = TRUE);


/*************************************************************************
CForest */
typedef struct {
   GUID        gCode;      // GUID describing what obejct to use
   GUID        gSub;       // GUID describing what object to use
   DWORD       dwWeight;   // when figuring out chance of this occurring, this probability
                           // is dwWeight / sum-of-weights. Hence, must be 1+
} CANOPYTREE, *PCANOPYTREE;

class DLLEXPORT CForestCanopy {
public:
   ESCNEWDELETE;

   CForestCanopy();
   ~CForestCanopy();

   PCMMLNode2 MMLTo (void);
   BOOL MMLFrom (PCMMLNode2 pNode);
   CForestCanopy *Clone (void);
   void DirtySet (void);

   BOOL Dialog (DWORD dwRenderShard, PCEscWindow pWindow);
   fp MaxTreeSize (DWORD dwRenderShard);
   BOOL EnumTree (DWORD dwRenderShard, int x, int y, DWORD dwType, PCMatrix pMatrix, PCPoint pLoc, BYTE *pScore,
      BOOL *pfIfDistantEliminate);
   void TreesInRange (DWORD dwRenderShard, PCPoint pCorner1, PCPoint pCorner2, RECT *pr);
   PCObjectClone TreeClone (DWORD dwRenderShard, DWORD dwType);
   BOOL TextureQuery (DWORD dwRenderShard, PCListFixed plText);
   BOOL ColorQuery (DWORD dwRenderShard, PCListFixed plColor);
   BOOL ObjectClassQuery (DWORD dwRenderShard, PCListFixed plObj);

   // set by user - note: After changing any of these DirtySet() must be called
   CListFixed        m_lCANOPYTREE;       // list of CANOPYTREE to use
   TEXTUREPOINT      m_tpSeparation;      // typical X and Y separation of trees in this canopy
   TEXTUREPOINT      m_tpSeparationVar;   // variation in separation. From 0 (exactly on grid) to 1 (full variation)
   CPoint            m_pRotationVar;      // variation in rotation. From 0 (no variation) to 1 (maximum variation)
   CPoint            m_pScaleVar;         // variation in scale. From 0 (no variation) to 1 (max variation).
                                          // note: p[4] is used for scaling xyz with one value so keep same proportions
   DWORD             m_dwRepeatX;         // number of trees in X before pattern repeats. 1+
   DWORD             m_dwRepeatY;         // number of trees in Y before pattern repeats. 1+
   DWORD             m_dwSeed;            // random seed used to generate canopy
   BOOL              m_fNoShadows;        // if TRUE then dont draw shadows
   BOOL              m_fLowDensity;       // if checked then cause distant areas to be drawn with lower density

   // scratch used to display bitmaps
   CListFixed        m_lDialogHBITMAP;    // list of bitmaps for thumbnails
   CListFixed        m_lDialogCOLORREF;   // list colorrefs for transparency
   DWORD             m_dwRenderShardTemp;


private:
   BOOL CalcTrees (DWORD dwRenderShard);

   // calculated
   BOOL              m_fDirty;            // set to TRUE if data has changed since last calc
   CListFixed        m_lPCObjectClone;    // list of clone objects, corresponding to m_lCANOPYTREE
   CMem              m_memTREEINFO;       // location of trees and matrices
   fp                m_fMaxSize;          // maximum size of a tree - used as buffer to make sure all trees in bounding box
};
typedef CForestCanopy *PCForestCanopy;


#define NUMFORESTCANOPIES        4        // each forest has 4 canopies

class DLLEXPORT CForest  {
public:
   ESCNEWDELETE;

   CForest (void);
   ~CForest (void);

   PCMMLNode2 MMLTo (void);
   BOOL MMLFrom (PCMMLNode2 pNode);
   CForest *Clone (void);
   BOOL TextureQuery (DWORD dwRenderShard, PCListFixed plText);
   BOOL ColorQuery (DWORD dwRenderShard, PCListFixed plColor);
   BOOL ObjectClassQuery (DWORD dwRenderShard, PCListFixed plObj);

   // variables that user can change
   WCHAR             m_szName[64];        // name of the type of forest
   COLORREF          m_cColor;            // color of the forest - shown on the map
   PCForestCanopy    m_apCanopy[NUMFORESTCANOPIES];   // list of canopies. [0] is largest, getting smaller
                           // note: These cannot be deleted.

private:
};
typedef CForest *PCForest;




/*********************************************************************************
Synthesize.cpp */
class CM3DWave;
typedef CM3DWave *PCM3DWave;
class CMTTS;
typedef CMTTS *PCMTTS;

#define SRFEATUREINCLUDEPCM         // if defined then SRFEATURE includes PCM

#ifdef SRFEATUREINCLUDEPCM
#define SRFEATUREINCLUDEPCM_SHORT   // instead of byte, include a short
#endif

#define SRACCURACY_PSYCHOWEIGHTS_DELTA_000

// SRFEATURE - Store SR feature information in it
#define SRSAMPLESPERSEC    200      // number of samples to take per second when generateing SR
//#define SRSAMPLESPERSEC    100     OLD  // number of samples to take per second when generateing SR
#define VOICECHATSAMPLESPERSEC 100  // number of samples per second to use for voicechat
#define SRBASEPITCH        100      // 200 hz, BUGFIX down to 100 hz from 200
#define SRPOINTSPEROCTAVE  24        // keep 4 points per octave, BUGFIX - changed to 6, then back to 12 cause not enough detail
   // BUGFIX - Was 12, upped to 24
#define SROCTAVE           7        // z octaves, goes to 12.8 kHz, BUGFIX went from 6 to 7 octaves
#define SRDATAPOINTS       (SROCTAVE * SRPOINTSPEROCTAVE)   // number of data points to get


#define SRACCURACY_SRSMALL_6
//#define SRACCURACY_SRSMALL_4
//#define SRACCURACY_SRSMALL_3
//#define SRACCURACY_SRSMALL_2
   // BUGBUG - better accuracy with SRACCURACY_MAXPHONESAMPLESFIXED_512 | SRACCURACY_TOPPERCENTUNIT_32 | SRACCURACY_SRSMALL_3, but
   // BUGBUG - May be better with SRACCURACY_TOPPERCENTUNIT_64
   // takes too much memory


#ifdef SRACCURACY_SRSMALL_6
#define SRSMALL            6        // size of the small SRFEATURE relative to the large one
#endif
#ifdef SRACCURACY_SRSMALL_4
#define SRSMALL            4        // size of the small SRFEATURE relative to the large one
#endif
#ifdef SRACCURACY_SRSMALL_3
#define SRSMALL            3        // size of the small SRFEATURE relative to the large one
#endif
#ifdef SRACCURACY_SRSMALL_2
#define SRSMALL            2        // size of the small SRFEATURE relative to the large one
#endif

#define SRDATAPOINTSSMALL  (SRDATAPOINTS / SRSMALL)
#define SRPHASENUM         64        // store phase on this many harmonics
#define SRFEATUREPCM       256      // number of PCM points in the SRFEATURE
#define SRFEATUREPCM_HARMSTART   20 // start keeping PCM at 20th harmonic
#define SRFEATUREPCM_HARMFULL    40 // everything is kept as PCM by 40th harmonic
typedef struct {
   // same as SRFEATUREOLD
   char           acVoiceEnergy[SRDATAPOINTS]; // number of db
   char           acNoiseEnergy[SRDATAPOINTS];  // number of db
   BYTE           abPhase[SRPHASENUM];    //0..255 representing phase of harmonic relative to first harmonic
                                          // [0] is always 0 since 1st harminc - not anymore, can be non-zero

#ifdef SRFEATUREINCLUDEPCM
   // new for PCM
   BYTE           bPCMHarmFadeStart;         // harmonic number where the fading starts, 0 = DC, 1= fund, 2 = 1st harm, etc.
   BYTE           bPCMHarmFadeFull;          // harmonic number where fading is full
   BYTE           bPCMHarmNyquist;           // harmonic number where the nyquist limit kicks in. If 0, then no data in features
   BYTE           bPCMFill;                  // nothing
   float          fPCMScale;                 // how much to scale the PCM values by

#ifdef SRFEATUREINCLUDEPCM_SHORT
   short          asPCM[SRFEATUREPCM];       // points to store
#else
   char           acPCM[SRFEATUREPCM];       // points to store
#endif

#endif
} SRFEATURE, *PSRFEATURE;


// SRFEATUREOLD - for backward compatability with loading
typedef struct {
   char           acVoiceEnergy[SRDATAPOINTS]; // number of db
   char           acNoiseEnergy[SRDATAPOINTS];  // number of db
   BYTE           abPhase[SRPHASENUM];    //0..255 representing phase of harmonic relative to first harmonic, [0] is always 0 since 1st harminc
} SRFEATUREOLD, *PSRFEATUREOLD;

// SRFEATUREFLOAT - Flating point representation of SRFEATURE
typedef struct {
   float          afVoiceEnergy[SRDATAPOINTS];
   float          afNoiseEnergy[SRDATAPOINTS];
   float          afPhase[SRPHASENUM];

#ifdef SRFEATUREINCLUDEPCM
   // new for PCM
   BYTE           bPCMHarmFadeStart;         // harmonic number where the fading starts
   BYTE           bPCMHarmFadeFull;          // harmonic number where fading is full
   BYTE           bPCMHarmNyquist;           // harmonic number where the nyquist limit kicks in
   BYTE           bPCMFill;                  // nothing
   float          afPCM[SRFEATUREPCM];       // points to store
#endif
} SRFEATUREFLOAT, *PSRFEATUREFLOAT;

// SRFEATURESMALL
typedef struct {
   char           acVoiceEnergyMain[SRDATAPOINTSSMALL]; // number of db
   char           acNoiseEnergyMain[SRDATAPOINTSSMALL];  // number of db
#ifndef SRACCURACY_PSYCHOWEIGHTS_DELTA_000
   char           acVoiceEnergyDelta[SRDATAPOINTSSMALL]; // number of db, delta from previous
   char           acNoiseEnergyDelta[SRDATAPOINTSSMALL];  // number of db, delta from previous
#endif
} SRFEATURESMALL, *PSRFEATURESMALL;


// SRDETAILEDPHASE - For storing phase in that allows for better inerpolation
#define PHASEDETAILED      4        // when storing phase for voiced, extra detail for higher frequenceies
#define SRDATAPOINTSDETAILED (SRDATAPOINTS * PHASEDETAILED) // more detailed version

typedef struct {
   float          afHarmPhase[SRPHASENUM][2];      // [x][0] = sin * amp, [x][1] = cos * amp, of phase, per harmonic
   float          afVoicedPhase[SRDATAPOINTSDETAILED][2]; // [x][0] = sin * amp, [x][1] = cos * amp, of phase, per voiced SRDATAPOINT element
} SRDETAILEDPHASE, *PSRDETAILEDPHASE;

// WAVESEGMENTFLOAT - Wave segment as float
typedef struct {
   float          afPCM[SRFEATUREPCM];
} WAVESEGMENTFLOAT, *PWAVESEGMENTFLOAT;


// TTSFEATURECOMPEXTRA - Extra info for TTS compress, per SRFEATURE
typedef struct {
   float                fPitchF0All;        // frequency in Hz of original
   float                fPitchF0Fuji;        // frequency in Hz of fuji pitch
} TTSFEATURECOMPEXTRA, *PTTSFEATURECOMPEXTRA;

// PSOLASTRUCT - For storing info necessary to synthesized PSOLA
typedef struct {
   // source audio
   DWORD          dwNumSRFEATURE;         // number of SRFEATURE's worth of audio
   DWORD          dwSamplesPerSec;        // sampling rate for the wave
   DWORD          dwSRSkip;               // saples per SRFEATURE
   short          *pasWave;               // waveform. dwNumSRFEATURE * dwSRSkip samples
   PTTSFEATURECOMPEXTRA paTFCE;           // array of dwNumSRFEATURE entries for pitch

   // stretch amount
   DWORD          dwFeatureStartSrc;      // feature number, offset from start of pasWave and paTFCE, where to get audio from
   DWORD          dwFeatureEndSrc;        // feature number to get up to. Can't be more than dwNumSRFEATURE
   int            iFeatureStartDest;      // feature number to write to in the wave. Output PCM will start writing assuming the pitch at this point
   int            iFeatureEndDest;        // ending destination number

   // used by PSOLASTRUCTContiguousExtra() and PSOLASTRUCTContiguous() and PSOLAGeneratePitchPointsWithEpochs()
   BOOL           fAppended;              // appended to make an extra smooth connection
} PSOLASTRUCT, *PPSOLASTRUCT;

// PSOLAPOINT - For storing a pitch point in PSOLA
typedef struct {
   double             fSample;                // sample location
   double             fSRFEATURE;             // location in SRFEATUREs
   double             fPSOLASTRUCT;           // index into the PSOLASTRUCT. 0.0 = right at beginning of first PSOLASTRUCT.
                                          // 0.9999 right at end of first. 1.0 = beginning of second. Can be negative,
                                          // or more than number of PSOLASTRUCT

   // used by PSOLASTRUCTContiguousExtra() and PSOLASTRUCTContiguous()
   BOOL           fAppended;              // if TRUE then was appended for purposes of smooth edges
   DWORD          dwTimesUsed;            // number of times this was used to generate PSOLA. Should ideally
                                          // be used only twice, once as a left and once as a right
} PSOLAPOINT, *PPSOLAPOINT;

// CProgressWaveSAmple - Used to provide accurate progress for waves
class CProgressWaveSample {
public:
   virtual BOOL Update (PCM3DWave pWave, DWORD dwSampleValid, DWORD dwSampleExpect) = 0;
   //   // pWave is the wave that's called. dwSampleValid is the sample up to which
   //   // is valid. dwSampleExpect is filled with the expected number of samples.
   //   // returns should be TRUE if continue, FALSE if want to quit
};
typedef CProgressWaveSample *PCProgressWaveSample;


// CProgressWaveSAmple - Used to provide accurate progress for waves
class CProgressWaveTTS {
public:
   virtual BOOL TTSWaveData (PCM3DWave pWave) = 0;
      // pWave is new wave data that can be copied/played. It might also be NULL just to check
      // to see if want to quit. Return TRUE to continue synthesizing, FALSE to stop

   virtual BOOL TTSSpeedVolume (fp *pafSpeed, fp *pafVolume) = 0;
      // called occasionally to see if the default speed and volume have changed.
      // These values are default filled in with 1.0. You can change them.
      // Return TRUE to continue speaking, FALSE if error and wish to stop
};
typedef CProgressWaveTTS *PCProgressWaveTTS;

class CSinLUT;
typedef CSinLUT *PCSinLUT;


class DLLEXPORT CVoiceSynthesize {
public:
   ESCNEWDELETE;

   CVoiceSynthesize (void);
   ~CVoiceSynthesize (void);

   BOOL CloneTo (CVoiceSynthesize *pTo);
   CVoiceSynthesize *Clone (void);
   BOOL SynthesizeFromSRFeature (int iTTSQuality, PCM3DWave pWave, PPSOLASTRUCT paPS, DWORD dwNumPS, fp fFormantShift,
      PCListFixed plWAVESEGMENTFLOAT,
      BOOL fEnablePCM, PCProgressSocket pProgress,
                                 PCM3DWave pWaveBase = NULL,
                                 BOOL fClearSRFEATURE = TRUE,
                                 PCProgressWaveSample pProgressWave = NULL);

//private:
   void GenerateOctaveToWavelength (DWORD dwSamplesPerSec, float *pafWavelength);
   BOOL CalcVolumeAndPitch (PCM3DWave pWave, DWORD dwSRSample,
      PSRFEATURE pSRFeature, DWORD dwHarmonics, fp fHarmonicDelta,BOOL fNoiseOnly,
       BOOL fNoiseForFFT, BOOL fEnablePCM,
      float *pafWavelength, DWORD *padwHarmAngleDelta, DWORD *padwHarmVolume, float *pfPCMEnergy);
   void InverseFFTSpectrum (PSRFEATURE pSRF, DWORD dwHarmonics, DWORD *padwHarmVolume,
                                           DWORD dwSamplesPerSec, fp fPitch, BOOL fNoiseForFFT,
                                           PWAVESEGMENTFLOAT pWSF, float *pafWave,
                                           PCSinLUT pSinLUT, PCMem pMemFFTScratch);
   BOOL SynthesizeVoiced (PCM3DWave pWave, BOOL fNoiseOnly, PCListFixed plWAVESEGMENTFLOAT, BOOL fEnablePCM, PCProgressSocket pProgress,
      PCProgressWaveSample pProgressWave);
   BOOL SynthesizeFromWave (PCM3DWave pWave, PCM3DWave pSourceWave, PCProgressSocket pProgress);
   fp GetFundamental (PCM3DWave pWave, DWORD dwSRSample);
   void PSOLAFillInSilence (PCM3DWave pWave, DWORD dwSampleStart, DWORD dwSampleEnd);
   void PSOLAVolumeAdjust (PCM3DWave pWave, fp *pafSRFEnergy, short *pasPCM, DWORD dwSamples, int iSampleStart,
      BOOL fSilenceToLeft, BOOL fSilenceToRight);
   BOOL PSOLAEnergyInOrig (PCM3DWave pWave, PCMem pMem);
   double PSOLABlendQuality (PCM3DWave pWave, short *pasPCM, DWORD dwSamples, int iCenterWave, int iCenterPCM);
   int PSOLAAutoCorrelate (PCM3DWave pWave, short *pasPCM, DWORD dwSamples, int iCenterWave,
                                          int iCenterPCM, DWORD dwFractionOfWavelength, double *pfScore);
   BOOL PSOLASTRUCTNonContiguous (PPSOLASTRUCT paPS, DWORD dwNum, fp fFormantShift, PCM3DWave pWave,
      int iTTSQuality);
   BOOL PSOLASTRUCTContiguousExtra (PPSOLASTRUCT paPS, DWORD dwNum, fp fFormantShift,
                                             PCM3DWave pWave, fp fPhaseAngle,
                                             PCListFixed plPSOLAPOINTSrc, int iTTSQuality, PCMem pMem,
                                             DWORD *pdwSampleStart, DWORD *pdwSampleEnd,
                                             BOOL *pfSilenceToLeft, BOOL *pfSilenceToRight,
                                             DWORD *padwError);
   void PSOLAFillInPCM (PCM3DWave pWave, short *pasFrom, DWORD dwSampleNum, DWORD dwSampleStart,
                                       DWORD dwSampleEnd, int iSampleStartWave, int *piSampleLastWrite, int *piSampleLastWriteUpTo);

   // fp PitchAtSRFEATURE (double fSRFEATURE, BOOL fFromWave, PCM3DWave pWave,
   //                                    PTTSFEATURECOMPEXTRA pTFCE, DWORD dwNumTFCE);
   //BOOL PSOLAGeneratePitchPoints (BOOL fSrc, PPSOLASTRUCT paPS, DWORD dwNum,
   //                                              PCM3DWave pWave, fp fPhaseAngle,
   //                                              BOOL fSilenceToLeft, BOOL fSilenceToRight, 
   //                                              PCListFixed plPSOLAPOINT);
   DWORD PSOLAPOINTClosest (double fPSOLASTRUCT, PPSOLAPOINT paPSOLAPOINT, DWORD dwNum);


   // harmonics
   fp                m_fHarmonicSpacing;        // 1.0 = normal spacing, other values create higher and lower spacing

   // source wave
   fp                m_fWavePitch;              // wave pitch, if 0 then dont do pitch bend
};
typedef CVoiceSynthesize *PCVoiceSynthesize;

fp PitchAtSRFEATURE (double fSRFEATURE, BOOL fFromWave, PCM3DWave pWave,
                                       PTTSFEATURECOMPEXTRA pTFCE, DWORD dwNumTFCE);
BOOL PSOLAGeneratePitchPoints (BOOL fSrc, PPSOLASTRUCT paPS, DWORD dwNum,
                                                 PCM3DWave pWave, fp fPhaseAngle,
                                                 BOOL fSilenceToLeft, BOOL fSilenceToRight, PCListFixed plPSOLAPOINT);
BOOL PSOLAGeneratePitchPointsWithEpochs (BOOL fSrc, PPSOLASTRUCT paPS, DWORD dwNum,
                                                 PCM3DWave pWave,
                                                 BOOL fSilenceToLeft, BOOL fSilenceToRight,
                                                 int iTTSQuality, PCListFixed plPSOLAPOINT);
BOOL PSOLASTRUCTContiguous (PPSOLASTRUCT paPS, DWORD dwNum, fp fFormantShift,
                                             PCM3DWave pWave, fp fPhaseAngle, BOOL fSilenceToLeft, BOOL fSilenceToRight,
                                             PCListFixed plPSOLAPOINTSrc, int iTTSQuality, PCMem pMem,
                                             DWORD *pdwError);

/*************************************************************************************
CVoiceDisguise.cpp */

// DISGUISEINFO - Information about the current disguise
typedef struct {
   // BUGFIX - change from fp to float so can save as binary
   float             afOctaveBands[2][SROCTAVE];   // scaling of octave bands. Each is relative weight for size
                        // [0][x] = voiced, [1][x] = unvoiced
   float             afEmphasizePeaks[2][SROCTAVE]; // how much to emphasize peaks and valleys. 1.0 = normal, 0 = smooth, 2 = max
                        // [0][x] = voiced, [1][x] = unvoiced
   char              abVolume[2][SROCTAVE];   // how much the energy is adjusted up/down for
                                          // each octave, in dB. [0][x] = voiced, [1][x] = unvoiced
} DISGUISEINFO, *PDISGUISEINFO;

// DISGUISEINFOOLD - Old version
typedef struct {
   // BUGFIX - change from fp to float so can save as binary
   float             afOctaveBands[SROCTAVE];   // scaling of octave bands. Each is relative weight for size
   float             afEmphasizePeaks[SROCTAVE]; // how much to emphasize peaks and valleys. 1.0 = normal, 0 = smooth, 2 = max
   char              abVolume[2][SROCTAVE];   // how much the energy is adjusted up/down for
                                          // each octave, in dB. [0][x] = voiced, [1][x] = unvoiced
} DISGUISEINFOOLD, *PDISGUISEINFOOLD;

// PHONEMEDISGUISE - Disguise information about a specific phoneme
typedef struct {
   WCHAR             szName[16];             // name of the phoneme
   SRFEATURE         srfOrig;                // original SR feature
   SRFEATURE         srfMimic;               // SR feature to mimic
   BOOL              fMimic;                 // set to TRUE if the mimic srfeature is valid
   float             fPitchOrig;             // pitch for original
   float             fPitchMimic;            // pitch for mimic
      // BUGFIX - Changed from fp to float so could save as binary
   DISGUISEINFO      DI;                     // disguise information
   SRFEATURESMALL    srfSmall;               // small SRfeature, calculated from srfOrig, normalized to 10,000
   // fp                fSmallEnergy;           // small's energy before it was normalized
} PHONEMEDISGUISE, *PPHONEMEDISGUISE;

// WAVEBASECHOICE - Choise for a wavebase from the disguise wizard
typedef struct {
   WCHAR             szName[64];             // name displayed
   WCHAR             szFile[256];            // file name
   float             fPitch;                 // pitch of wave, or 0 if none
      // BUGFIX - Made float just in case saving to binary
} WAVEBASECHOICE, *PWAVEBASECHOICE;

// WVPITCH - Extracted pitch information
typedef struct {
   // BUGFIX - Changed from fp to float so could save to binary
   float       fFreq;         // frequency in Hz
   float       fStrength;     // score. Units are appox in 32676 to 0. So pure sine should get around 32000
} WVPITCH, *PWVPITCH;


// WVPHONEME - Phoneme information stores in wave
typedef struct {
   DWORD       dwSample;      // sample that the phoneme starts at
   WCHAR       awcNameLong[4+4];     // phoneme name, followed by zero's, such as "ae\0\0"
   DWORD       dwEnglishPhone; // english phone - used for mouth shape. only use bottom byte, but DWORD align
} WVPHONEME, *PWVPHONEME;

// WVPHONEMEOLD - Old - Phoneme information stores in wave
typedef struct {
   DWORD       dwSample;      // sample that the phoneme starts at
   WCHAR       awcName[4];     // phoneme name, followed by zero's, such as "ae\0\0"
   DWORD       dwEnglishPhone; // english phone - used for mouth shape. only use bottom byte, but DWORD align
} WVPHONEMEOLD2, *PWVPHONEMEOLD2;


class DLLEXPORT CVoiceDisguise {
public:
   ESCNEWDELETE;

   CVoiceDisguise (void);
   ~CVoiceDisguise (void);
   BOOL Dialog (PCEscWindow pWindow, PCMTTS pTTS,
      DWORD dwNumWAVEBASECHOICE, PWAVEBASECHOICE paWAVEBASECHOICE,
      DWORD dwFlags);
   DWORD DialogModifyWave (PCEscWindow pWindow, PCM3DWave pWave);
   void Clear (void);
   BOOL CloneTo (CVoiceDisguise *pTo);
   BOOL ChangedQuery (void);
   CVoiceDisguise *Clone (void);
   PCMMLNode2 MMLTo (void);
   BOOL MMLFrom (PCMMLNode2 pNode);
   BOOL Load (HWND hWnd);
   BOOL Save (HWND hWnd);
   BOOL MixTogether (DWORD dwNum, CVoiceDisguise **papVD, fp *pafWeight);

   fp ModifySRFEATURE (PSRFEATURE pOrig, PSRDETAILEDPHASE pSDPOrig,
                                    PSRFEATURE pMod, PSRDETAILEDPHASE pSDPMod, fp fPitchOrig,
                                    PDISGUISEINFO pDI, BOOL fIncludeExtra);
   BOOL ModifySRFEATUREInWave (PCM3DWave pWave, PSRDETAILEDPHASE paSRDP, fp *pfFormantShift,
      DWORD dwStartFeature = 0, DWORD dwEndFeature = (DWORD)-1);
   fp CalcPitchVariation (PCM3DWave pWave, fp fCenterPitch);
   BOOL DetermineDISGUISEINFOForWave (DWORD dwNum, PCM3DWave pWave, PSRFEATURE psrf,
      PWVPHONEME paWVPHONEME, DWORD dwNumWVPHONEME,
      DWORD dwFeatureSize, DWORD dwPhonemeOffset, PCListFixed plDISGUISEINFO);
   BOOL SynthesizeFromSRFeature (int iTTSQuality, PCM3DWave pWave, PPSOLASTRUCT paPS, DWORD dwNumPS, fp fFormantShift,
      PCListFixed plWAVESEGMENTFLOAT,
      PSRDETAILEDPHASE paSRDP, BOOL fEnablePCM, PCProgressSocket pProgress,
      BOOL fClearSRFEATURE = TRUE, BOOL fSkipModify = FALSE, PCProgressWaveSample pProgressWave = NULL);

   // should set this
   CMem              m_memSourceTTSFile;  // if using a TTS engine, helps to find the wave file for TTS
                                          // this is the TTS file name. NULL-terminated string

   // adjustments
   BOOL              m_fAgreeNoOffensive; // set this if player agreed to no offensive langauge
   fp                m_fPitchOrig;        // pitch of the original, needed for scaling
   fp                m_fPitchScale;       // how much to scale the pitch from original to new
   fp                m_fPitchVariationScale; // how much to scale the pitch variation
   BOOL              m_fPitchMusical;     // if TRUE then snap pitch to nearest musical value
   DISGUISEINFO      m_DisguiseInfo;      // global disguise info
   CListFixed        m_lPHONEMEDISGUISE;  // list of phoneme disguises
   float             m_afVoicedToUnvoiced[SROCTAVE];  // convert voiced to unvoiced, 0..1.0
      // BUGFIX - Was fp, but made it float so that works with doubles saved to binary
   char              m_abOverallVolume[2];   // overall volume, [0]=unvoiced, [1]=voiced
   fp                m_fNonIntHarmonics;  // usually 1.0, but other values for non-integer harmonics
   CMem              m_memWaveBase;       // wave file base, or "" if no wave file
   fp                m_fWaveBasePitch;    // pitch of wavefile base, or 0 if none

   CListFixed        m_lMixPCVoiceDisguise;  // list of voice disguises to mix together
   CListFixed        m_lMixWeight;           // weights of all the voice disguises. Number of elements =
                                             // 1 (for this one) + m_lMixPCVoiceDisguise.Num()

   // for the dialog
   BOOL DialogPlaySample (HWND hWnd, BOOL fIncludeTemp);
   BOOL DialogPlaySample (PSRFEATURE pSRFEATURE, fp fPitch);
   //BOOL DialogPlaySample (DWORD dwSample, BOOL fOrig, BOOL fApplyDisguise,
   //                                    DWORD dwSamplePitch = (DWORD)-1);
   BOOL DialogPlaySample (PPHONEMEDISGUISE pPD, BOOL fOrig, BOOL fApplyDisguise);
   BOOL DialogPlaySampleSnip (DWORD dwSample, BOOL fOrig);
   void DialogSRFEATUREViewUpdate (PCEscPage pPage, DWORD dwNum = (DWORD)-1);
   BOOL DialogSRFEATUREGet (DWORD dwSample, BOOL fOrig, BOOL fApplyDisguise,
                                         fp *pfPitch, PSRFEATURE pSRFEATURE);

   PCMTTS            m_pTTS;              // TTS to convert. If NULL then disguising recorded voice.
   DWORD             m_dwNumWAVEBASECHOICE; // number of choices the user has for wavees to base off of
   PWAVEBASECHOICE   m_paWAVEBASECHOICE;  // list of choices to base off of
   BOOL              m_dwFlags;           // if have flags
   PCM3DWave         m_pWaveScratch;      // scratch wave used in the dialog code
   PCM3DWave         m_pWaveMimic;        // voice to mimic
   PCM3DWave         m_pWaveOrig;         // original recording from the user
   CMem              m_memSpoken;         // spoekn in m_pWaveMimic
   CListFixed        m_lWAVEVIEWMARKERMimic; // markers for mimic
   CListFixed        m_lWAVEVIEWMARKEROrig; // markers for orig
   CListFixed        m_lPHONEMEDISGUISETemp; // list of temporary phoneme disguises that working on
   DWORD             m_dwFineTunePhoneme; // phoneme number to fine tune, or -1 if modifying all phonemes
   PCM3DWave         m_pWaveTest;               // copy of wave to test
   PCM3DWave         m_pWaveTestOriginal;       // original wave used for testing

   PCM3DWave         m_pWaveCache;        // cached wave, for fast access
private:
   void CreateNoiseAndGarbage (DWORD dwNum, PPHONEMEDISGUISE paPD,
                                            PPHONEMEDISGUISE pPDNoise, PPHONEMEDISGUISE pPDGarbage);
   void ApplyDISGUISEINFOToWave (DWORD dwNum, PCM3DWave pWave,
      PSRFEATURE pasrf, PSRDETAILEDPHASE paSRDP, PWVPITCH paPitch, PDISGUISEINFO paDI,
      PDISGUISEINFO pDISum);

   CListFixed        m_lPageHistory;      // list of page IDs in the history. DWORds
   PCEscWindow       m_pWindow;           // window used to display dialog
};
typedef CVoiceDisguise *PCVoiceDisguise;


/****************************************************************************
CM3DWave */


// SRFPHASEEXTRACT - Internal structure for extracting phase information
#define SRFFPHASEEXTRACT_NUM        8     // number of phases stored
typedef struct {
   fp       afEnergy[SRFFPHASEEXTRACT_NUM];  // energy for lower harmonics
   fp       afPhase[SRFFPHASEEXTRACT_NUM];   // phase, 0 to 2pi (or maybe -pi to pi) for phases
   fp       fCenter;          // center sample
   fp       fWavelength;      // wavelength
} SRFPHASEEXTRACT, *PSRFPHASEEXTRACT;

// FXECHOPARAM - Parameters for the echo fx
typedef struct {
   DWORD       dwSurfaces;    // number of surfaces to echo off of, 1+
   fp          fDelay;        // average echo delay, in seconds
   fp          fDelayVar;     // variation in delay, from 0..1 (1 being max variation)
   fp          fDecay;        // amount echo decays with each bounce, 0..1, 1 being max
   //fp          fSharpness;    // how sharp the echo is, 0..1, 1 being very sharp
   fp          fTime;         // number of seconds of echo to produce
   DWORD       dwSeed;        // random seed
} FXECHOPARAM, *PFXECHOPARAM;

// WVWORD - Word information stored in wave
typedef struct {
   DWORD       dwSample;      // sample that the word starts
   // NOTE: After this structure comes the unicode, null-terminated string for the word
} WVWORD, *PWVWORD;

// WVEPOCH - Epoch information
typedef struct {
   double      fSample;       // sample that epoch is at
} WVEPOCH, *PWVEPOCH;

// HARMINFO - For storing voiced, unvoiced, and phase of harmonics
typedef struct {
   float       fVoiced;       // voiced energy
   float       fUnvoiced;     // unvoiced energy
   float       fPhase;        // phase, 2PI, not necessarily fmoded
   float       afVoicedSinCos[2];   // sin/cos versions of voiced, from FFT. Phase = atan2(afVoicedSinCos[0], afVoicedSinCos[1])
} HARMINFO, *PHARMINFO;

#define NUMWVMATCH      3        // number of matches that store
      // BUGFIX - Upped from 2 to 3 so that get more accurate with blizzard 2007 voice
typedef struct {
   float       afStrength[NUMWVMATCH]; // strength for best and second best
   float       afFreq[NUMWVMATCH];    // pitches, in sample terms, for best and second best matches
   WORD        awMaximaStart[NUMWVMATCH]; // local minima just below adwFreq[x]
   WORD        awMaximaEnd[NUMWVMATCH]; // local minima just above adwFreq[x]
   BOOL        fUsed;      // if TRUE already used in a segment
} WVPITCHEXTRA, *PWVPITCHEXTRA;

class CVoiceFile;
typedef CVoiceFile *PCVoiceFile;

class CMLexicon;
typedef CMLexicon *PCMLexicon;

class CSinLUT;
typedef CSinLUT *PCSinLUT;

#define SRFEATCACHE     10    // cache no more than 10 chunks of SR features

#define PITCH_NUM                   2  // two different pitch values store
#define PITCH_F0                    0  // F0 pitch
#define PITCH_SUB                   1  // F0/sub pitch

// for CalcSRFeature(), CalcXXX, etc.
#define WAVECALC_TTS_PARTIALPCM     0  // calcualting for purposes of text-to-speech, only keep PCM for higher frequencies
#define WAVECALC_TTS_FULLPCM        1  // for TTS, but keep FULL PCM around for highest quality
#define WAVECALC_SEGMENT            2  // calculating for purposes of ordinary wave segmentation
#define WAVECALC_TRANSPROS          3  // calculating for purposes of transplanted prosody
#define WAVECALC_VOICECHAT          4  // caclulating for voicechat

class DLLEXPORT CM3DWave {
public:
   ESCNEWDELETE;

   CM3DWave (void);
   ~CM3DWave (void);

   BOOL New (DWORD dwSamplesPerSec, DWORD dwChannels, DWORD dwSamples = 0);
   BOOL New (PWAVEFORMATEX pwfex = NULL, DWORD dwSamples = 0);
   BOOL Open (PCProgressSocket pProgress, char* szFile, BOOL fLoadWave = TRUE, HMMIO hmmio = NULL,
      DWORD dwForceSamplesPerSec = 0, PCProgressWaveSample pProgressWave = NULL);
   BOOL Save (BOOL fIncludeSRFeature, PCProgressSocket pProgress, HMMIO hmmio = NULL); // saves the wave using m_szFile
   PCM3DWave Clone (void);
   PCM3DWave Copy (DWORD dwStart, DWORD dwEnd);
   BOOL RequireWave (PCProgressSocket pProgress);
   BOOL ReleaseWave (void);
   BOOL ReleaseSRFeatures (void);
   BOOL Allocate (DWORD dwSamples);
   BOOL BlankWaveToSize (DWORD dwSamples, BOOL fBlankPitch);
   BOOL AppendPCMAudio (PVOID pPCM, DWORD dwSamples, BOOL fAlsoSRPitch = FALSE);
   BOOL AppendWave (PCM3DWave pWave);

   void ReleaseCalc (void);
   BOOL CalcEnergy (PCProgressSocket pProgress);
   WORD CalcSilenceEnergy (void);
   BOOL CalcPitch (DWORD dwCalcFor, PCProgressSocket pProgress);
   void PitchLowPass (DWORD dwPitchSub);
   void CalcPitchIfNeeded (DWORD dwCalcFor, HWND hWnd);
   fp CalcEnergyRange (DWORD dwStart, DWORD dwEnd);
   fp CalcSREnergyRange (DWORD dwStart, DWORD dwEnd, BOOL fPsycho);

   fp CalcWavelengthEnergy (int iCenter, int iSamples);
   fp PitchAtSample (DWORD dwPitchSub, fp fSample, DWORD dwChannel);
   fp PitchOverRange (DWORD dwPitchSub, DWORD dwStartSample, DWORD dwEndSample, DWORD dwChannel,
      fp *pfAvgStrength /*= NULL*/, fp *pfPitchDelta /*= NULL*/, fp *pfPitchBulge /*= NULL*/,
      BOOL fIgnoreStrength /*= FALSE*/);
   void PitchSubClear (void);

   void DefaultWFEXGet (PWAVEFORMATEX pwfex, DWORD dwSize);
   void DefaultWFEXSet (PWAVEFORMATEX pwfex);
   void DefaultSpeakerSet (PCMem pMem);
   void DefaultSpeakerGet (PCMem pMem);

   BOOL ADPCMHackCompress (void);
   BOOL ADPCMHackDecompress (void);

   void PaintWave (HDC hDC, RECT *pr, double fLeft, double fRight,
                          double fTop, double fBottom, DWORD dwChannel,
                          BOOL fEverySample = FALSE);
   void PaintFFT (HDC hDC, RECT *pr, double fLeft, double fRight,
                          double fTop, double fBottom, DWORD dwChannel, HWND hWnd,
                          DWORD dwPaintWhat = 0,fp *pfFreqRange = NULL);
   void PaintEnergy (HDC hDC, RECT *pr, double fLeft, double fRight,
                          double fTop, double fBottom, DWORD dwChannel);
   void PaintPitch (HDC hDC, RECT *pr, double fLeft, double fRight,
                          double fTop, double fBottom, DWORD dwChannel, HWND hWnd);
   void PaintPhonemes (HDC hDC, RECT *pr, double fLeft, double fRight,
      PCMLexicon pLex);
   void PaintWords (HDC hDC, RECT *pr, double fLeft, double fRight);

   DWORD WindowSize (void);
   BOOL ReplaceSection (DWORD dwStart, DWORD dwEnd, PCM3DWave pWave);
   void MakePCM (void);
   BOOL ConvertWFEX (PWAVEFORMATEX pwfex, PCProgressSocket pProgress);
   BOOL ConvertChannels (DWORD dwChannels, PCProgressSocket pProgress);
   BOOL ConvertSamplesPerSec (DWORD dwSamplesPerSec, PCProgressSocket pProgress);
   BOOL Decimate (DWORD dwOrig, DWORD dwNew, BOOL fInterp, PCProgressSocket pProgress);
   BOOL ConvertSamplesAndChannels (DWORD dwSamplesPerSec, DWORD dwChannels, PCProgressSocket pProgress);
   BOOL Filter (DWORD dwFreqBands, fp *pafFreqBands, DWORD dwTimeBands, DWORD *padwTimeBands,
                       fp *pafAmplify, PCProgressSocket pProgress, BOOL fReleaseCalc = TRUE);

   PCM3DWave Monotone (DWORD dwCalcFor, fp fPitch, DWORD dwStartSample, DWORD dwEndSample,
                              PCListFixed plMap, PCProgressSocket pProgress);
   double MonotoneFindOrigSample (double fSample, PCListFixed plMap,
                                         double *pfPitch);

   short FindMax (void);
   BOOL FXVolume (fp fVolStart, fp fVolEnd, PCProgressSocket pProgress);
   BOOL FXReverse (PCProgressSocket pProgress);
   BOOL FXRemoveDCOffset (BOOL fAgressive, PCProgressSocket pProgress);
   BOOL FXSwapChannels (PCProgressSocket pProgress);
   BOOL FXNoiseReduce (PCProgressSocket pProgress, BOOL fStrongNoiseReduce);
   BOOL FXTrimSilence (PCProgressSocket pProgress);
   BOOL FXFrequency (DWORD dwEffect, PCProgressSocket pProgress);
   BOOL FXBlend (fp fLoop, PCProgressSocket pProgress);
   BOOL FXAcousticCompress (fp fPower, PCProgressSocket pProgress);
   BOOL FXGenerateEcho (PFXECHOPARAM pEcho);
   BOOL FXConvolve (PCM3DWave pWith, PCProgressSocket pProgress);
   BOOL FXSine (fp fStart, fp fEnd, DWORD dwShape, DWORD dwChannel, PCProgressSocket pProgress);
   BOOL FXTimeStretch (fp fStretch, PCProgressSocket pProgress);
   BOOL FXSRFEATUREStretch (fp fStretch);
   void FXSRFEATUREExtend (void);
   BOOL FXPSOLAStretch (fp fPitchScale, fp fDurationScale, fp fFormantShift, PCProgressSocket pProgress);

   BOOL WordDelete (DWORD dwStart, DWORD dwEnd);
   BOOL WordInsert (DWORD dwInsert, DWORD dwTime, DWORD dwFrom, PCListVariable plWVWORDFrom);
   void WordStretch (double fStretch);
   void WordRemoveDup (void);

   void PhonemeRemoveDup (BOOL fRemoveOnlySilence = FALSE);
   BOOL PhonemeDelete (DWORD dwStart, DWORD dwEnd);
   BOOL PhonemeInsert (DWORD dwInsert, DWORD dwTime, DWORD dwFrom, PCListFixed plWVPHONEMEFrom);
   void PhonemeStretch (double fStretch);
   void PhonemeGetRidOfShortSilence (void);
   BOOL PhonemeAtTime (DWORD dwSample, PWSTR psz);
   BOOL PhonemeAtTime (DWORD dwSample, PWSTR *ppszLeft = NULL, PWSTR *ppszRight = NULL, fp *pfAlpha = NULL,
                              fp *pfLateralTension = NULL, fp *pfVerticalOpen = NULL,
                              fp *pfTeethTop = NULL, fp* pfTeethBottom = NULL,
                              fp *pfTongueForward = NULL, fp *pfTongueUp = NULL);

   void CalcSRFeaturesIfNeeded (DWORD dwCalcFor, HWND hWnd, PCProgressSocket pProgress = NULL);
   BOOL CalcSRFeatures (DWORD dwCalcFor, PCProgressSocket pProgress);
   PSRFEATURE CacheSRFeatures (DWORD dwStart, DWORD dwEnd);
   BOOL BlankSRFeatures (void);
   BOOL SRFEATUREAlignPCM (BOOL fFullPCM, PCMLexicon pLex, PCListFixed plSRDETAILEDPHASE);

   BOOL WaveBufferGet (DWORD dwSamplesPerSec, DWORD dwChannels, int iTimeSec, int iTimeFrac,
                              BOOL fLoop, DWORD dwMaxSamples, DWORD dwSamples, short *pasSamp);
   void FFT (DWORD dwWindowSize, int iCenter, DWORD dwChannel,
                    float *pafWindow, float *pasfFFT, PCSinLUT pLUT, PCMem pMemFFTScratch,
                    double *pfEnergy, DWORD dwFilterHalf);

   void QuickPlayStop (void);
   BOOL QuickPlay (DWORD dwStart = 0, DWORD dwEnd = -1);
   int QuickPlayQuery (void);
   void PlayCallback (HWAVEOUT hwo,UINT uMsg,         
      DWORD_PTR dwParam1, DWORD_PTR dwParam2); // really internal function

   void CalcEpochsIfNecessary (void);

   // record
   PCM3DWave Record (HWND hWnd, BOOL fSimple, DWORD *pdwReplace);
   PCM3DWave RecordAdvanced (PCEscWindow pWindow, DWORD dwResource, HINSTANCE hInstance,
                                    BOOL fSimple, DWORD *pdwReplace, PWSTR pszLink);
   void RecordCallback (PVOID  priVoid, HWAVEIN hwi,UINT uMsg,         
      DWORD_PTR dwParam1, DWORD_PTR dwParam2); // actually private

   // you may wish to change this
   DWORD                m_dwSRSAMPLESPERSEC; // when get a change, the default samples per sec to use
                                             // if you change this to VOICECHATSAMPLESPERSEC before do
                                             // any wave analysis then OK for voicechat

   // user data 
   PVOID                m_pUserData;         // can set oneself

   // variables
   char                 m_szFile[256];       // file that's loaded. null-string if no file
   BOOL                 m_fDirty;            // dirty flag. If TRUE wave has changed since last save
   DWORD                m_dwSamplesPerSec;   // number of samples per second
   DWORD                m_dwSamples;         // number of samples, can read but dont modify
   DWORD                m_dwChannels;        // number of chanels, can read but dont modify
   short                *m_psWave;           // pointer to memory containing wave data. if NULL then no wave data loaded
   CMem                 m_memWFEX;           // memory for the wave format. dont change this. OK to read. Contains entire WFEX
   CListFixed           m_lWVPHONEME;        // list of phonemes. the dwSample's are sorted in order
   CListVariable        m_lWVWORD;           // list of WVWORD structures for what words. dwSamples are sorted in order
   CListFixed           m_lBookmarkIndex;    // list of DWORD (samples) where bookmarks are set, NOT saved in file
   CListVariable        m_lBookmarkString;   // bookmark strings, NOT saved in file

   CListFixed           m_lWVEPOCH;          // list of epochs, in order

   CMem                 m_memSpoken;         // words spoken in the wave file
   CMem                 m_memSpeaker;        // name of person speaking

   // energy info
   DWORD                m_dwEnergySamples;   // number of samples calculated for energy. if 0 need to recalc energy
   DWORD                m_dwEnergySkip;      // energy reading taken this many samples
   WORD                 *m_pwEnergy;         // pointer to an array of m_dwEnergySamples x m_dwChannels entries for amount of energy, 0..65535

   // pitch info
   DWORD                m_adwPitchSamples[PITCH_NUM];    // number of samples calculated for pitch. if 0 none calculated
   DWORD                m_adwPitchSkip[PITCH_NUM];       // pitch sample taken this many samples
   PWVPITCH             m_apPitch[PITCH_NUM];            // pointer to an array of m_dwPitchSample WVPITCH structures
   fp                   m_afPitchMaxStrength[PITCH_NUM]; // maximum strength in the current pitch, at least epsilon
   CMem                 m_amemPitch[PITCH_NUM];          // memory containing the pitch info

   // SR features
   DWORD                m_dwSRSamples;       // number of samples for SR. 0 if none
   DWORD                m_dwSRSkip;          // number of samples between features
   PSRFEATURE           m_paSRFeature;       // pointer to m_dwSRSamples x SRFEATURE

   // quick play - really internal, but called from external timer proc
   BOOL                 m_fQuickPlayDone;    // set to TRUE if quickplay is done

private:
   PCMem                m_pmemWave;          // pointer to memory containing wave data. if NULL then no wave data loaded
   CMem                 m_memEnergy;         // memory containing the energy info
   PCMem                m_pmemSR;             // memory for SR features

   CM3DWave*            m_pAnimWave;         // wave with correct sampling rate and channels used for animation
   DWORD                m_dwSRFeatChunkStart;   // where the SR feature chunk started
   DWORD                m_dwSRFeatChunkSize;    // size of the SR feature chunk
   PCMem                m_apmemSRFeatTemp[SRFEATCACHE];    // temporarily load in feature chunk
   DWORD                m_adwSRFeatTemp[SRFEATCACHE];      // feature number temporarily loaded in, m_memSRFeatTemp.m_dwCurPosn indicates size

   // used for playing a small snippet of sound
   HWAVEOUT             m_hWaveOut;          // waveout playing through
   WAVEHDR              m_whWaveOut;         // wave header for the one buffer
   PCMem                m_pmemWaveOut;       // memory for the buffer used to play in wave out
   DWORD                m_dwQuickPlayTimer;  // to close off wave file

   void CalcEpochsDetermineBreaks (PCListFixed plBreak);
   BOOL CalcEpochs (void);
   BOOL CalcEpochsForRange (DWORD dwSampleStart, DWORD dwSampleEnd);
   HBITMAP RecordBitmap (short *pszWave, DWORD dwSamples, DWORD dwChannels, HDC hDC, short *psCache,
                                 PVOID priVoid);
   void ReNormalizeToWave ();
   BOOL ReNormalizeToWaveSRFEATURE (PCM3DWave pClone, DWORD dwFeature,
                                           PCSinLUT pLUT, PCMem pMemScratch, PCMem pMemFFTScratch, PCMem pMemWindow,
                                           DWORD *pdwWindowSize);
   void StretchFFT (DWORD dwWindowSize, int iCenter, DWORD dwChannel, float *pafFFTVoiced,
                           float *pafFFTNoise, float *pafFFTPhase, float fZeroOctave, DWORD dwDivsPerOctave, DWORD dwBlur,
                           PCSinLUT pLUT, PCMem pMemFFTScratch);
   void StretchFFTFloat (fp fCenter, DWORD dwChannel, DWORD dwFFTSize, float *pafFFT,
      fp fScaleLeft, fp fScaleRight, PCSinLUT pLUT, PCMem pMemFFTScratch,
      PCListFixed plfCache, PCListVariable plvCache);
   fp StretchFFTSlide (fp fCenter, DWORD dwChannel);
   BOOL MagicFFT (fp fSRSample, float *pafFFTVoiced, float *pafFFTNoise, float *pafFFTPhase,
      PCSinLUT pLUT, PCMem pMemFFTScratch,
      PCListFixed plfCache, PCListVariable plvCache,
      BOOL fCanSlide, PSRFPHASEEXTRACT pSRFPhase);
   BOOL SuperMagicFFT (fp fSRSample, float *pafFFTVoiced, float *pafFFTNoise, float *pafFFTPhase,
      float *pafFFTPhaseLast,
      PCSinLUT pLUT, PCMem pMemFFTScratch,
      PCListFixed plfCache, PCListVariable plvCache, PSRFPHASEEXTRACT pSRFPhase);
   BOOL FixHighFrequencyBlurriness (fp fSRSample, float *pafFFTVoiced, float *pafFFTNoise,
      PCSinLUT pLUT, PCMem pMemFFTScratch);
   BOOL FixedFFTToSRDATAPOINT (fp fSRSample, float *pafFFTVoiced,
                                           PCSinLUT pLUT, PCMem pMemFFTScratch);
   void CalcSRFeaturesStack (fp fPitch, float *pafFFTVoiced, float *pafFFTUnvoiced, float *pafFFTPhase,
                                    PCMem pMemStack, int *piStack, PCSinLUT pSinLUT, PCMem pMemFFTScratch);
   void RescaleSRDATAPOINTBySmallFFT (fp fSRSample, float *pafFFTVoiced,
                                             float *pafFFTUnvoiced, float fWeight, PCSinLUT pLUT, PCMem pMemFFTScratch);
   void RemoveNoisySRFEATURE (float *pafCenterVoiced, float *pafCenterUnvoiced, float *pafCenterEnergy, float *pafCenterPercent,
                                     float *pafLeftVoiced, float *pafLeftUnvoiced, float *pafLeftEnergy, float *pafLeftPercent,
                                     float *pafRightVoiced, float *pafRightUnvoiced, float *pafRightEnergy, float *pafRightPercent,
                                     float *pafNewVoiced, float *pafNewUnvoiced);
   fp FineTunePitch (int iCenter, DWORD dwChannel, DWORD dwMinWaveLen, DWORD dwMaxWaveLen,
      fp *pfVoiced, fp*pfNoise, PCSinLUT pLUT, PCMem pMemFFTScratch);
   void EchoBounce (DWORD dwCount, DWORD dwChan,
      DWORD dwSample, fp fScale, fp fSpreadOut, PFXECHOPARAM pEcho);
   BOOL AnalyzePitchSpectrum (int iSample, DWORD dwChannel, DWORD dwWindowSize,
                                     float *pafWindow, DWORD dwResultSize, float fWavelenInc, float *pafResult,
                                     PCMem pMem, PCSinLUT pLUT, PCMem pMemFFTScratch,
                                     PCListFixed plPITCHDETECTVALUE);
   BOOL AnalyzePitchSpectrumBlock (DWORD dwSamples, double fSampleStart, double fSampleDelta,
                                           DWORD dwChannel,
                                           PCMem pMemBlock, DWORD *pdwResultSize, DWORD *pdwWindowSize,
                                           PCProgressSocket pProgress);
   BOOL CalcPitchInternalA (BOOL fFastPitch, PCProgressSocket pProgress);
   BOOL CalcPitchInternalB (BOOL fFastPitch, PCProgressSocket pProgress);
   BYTE CalcPitchExpandHyp (PCListFixed plOrig, PCListFixed plNew,
                                   PWVPITCHEXTRA pExtra, fp fNothingScore, fp fChangePenalty,
                                   fp fPitchChangePenalty);

   float FFTToHarmInfo (DWORD dwNum, PHARMINFO pHI, DWORD dwWindowSize, float *pafFFT);
   void HarmInfoToSRFEATUREFLOAT (DWORD dwNum, PHARMINFO pHI, fp fPitch, PSRFEATUREFLOAT pSRF);
   float HarmInfoFromStretch (DWORD dwNum, short *pasWave, DWORD dwCenter, double fWavelength,
                                    DWORD dwNumHI, PHARMINFO pHI,
                                    PCSinLUT pLUT, PCMem pMemFFTScratch,
                                    DWORD dwCalcSRFeatPCM);
   void PCMToSRFEATUREFLOAT (DWORD dwCalcFor, int iCenter, fp fPitch, DWORD dwSamplesPerSecOrig, PSRFEATUREFLOAT pSRF, PHARMINFO paHI,
                                    float *pafEnergyOctave,
      PCSinLUT pLUT, PCMem pMemFFTScratch, DWORD dwCalcSRFeatPCM,
      fp *pfPitchFineTune);
   fp PCMToSRFEATUREFocusInOnPitch (fp fOctaveMin, fp fOctaveMax, fp fNoiseMin, fp fNoiseCenter, fp fNoiseMax,
                                           fp fMinDelta, DWORD dwNum, short *pasWave, DWORD dwCenter,
                                           PCSinLUT pLUT, PCMem pMemFFTScratch,
                                           DWORD dwCalcSRFeatPCM);
   void HARMINFOAverageAll (DWORD dwSamples, double *pafPitch, PHARMINFO paHI, PSRFEATUREFLOAT paSRF,
      DWORD dwCalcSRFeatPCM);
   void BlurSRDETAILEDPHASE (void);
};


/************************************************************************************8
WaveOpen */
DLLEXPORT BOOL WaveFileOpen (HWND hWnd, BOOL fSave, PWSTR pszFile, PCListVariable plMultiSel = NULL);
DEFINE_GUID(GUID_WaveCache, 
0x3e16ddf3, 0x9444, 0x4617, 0xae, 0x85, 0x2f, 0xb2, 0xfc, 0x91, 0xfb, 0xb8);

DLLEXPORT PCWSTR MyStrIStr (PCWSTR pszLookIn, PCWSTR pszLookFor);

/************************************************************************************
VoiceChat */
DLLEXPORT DWORD VoiceChatDeCompress (PBYTE pbData, DWORD dwSize, PCM3DWave pWave, PCVoiceDisguise pVoiceDisguise,
                                     PCListVariable plValidWaves = NULL, PCProgressWaveSample pProgressWave = NULL);
DLLEXPORT BOOL VoiceChatCompress (DWORD dwQuality, PCM3DWave pWave, PCVoiceDisguise pVoiceDisguise, PCMem pMem);
DLLEXPORT PCM3DWave VoiceChatStream (PCM3DWave pWave, PCListFixed plEnergy, BOOL fHavePreviouslySent);
DLLEXPORT BOOL VoiceChatWhisperify (PBYTE pbData, DWORD dwSize);
DLLEXPORT BOOL VoiceChatRandomize (PBYTE pbData, DWORD dwSize);
DLLEXPORT BOOL VoiceChatValidateMaxLength (PBYTE pbData, DWORD dwSize, DWORD dwMax);

#define VCH_ID_VERYBEST    1     // very best quality, using SIZEOFVOICECHATBLOCK3
#define VCH_ID_BEST        2     // quality with every SR feature, and using SIZEOFVOICECHATBLOCK1
#define VCH_ID_MED         3     // quality with every other SR feature, and using SIZEOFVOICECHATBLOCK1
#define VCH_ID_LOW         4     // quality with every other SR feature, and using SIZEOFVOICECHATBLOCK2
#define VCH_ID_VERYLOW     5     // quality with every other SR feature, and using SIZEOFVOICECHATBLOCK2


/************************************************************************************8
CWaveView */

#define WP_WAVE            0  // displays a wave
#define WP_FFT             1  // displays a FFT
#define WP_ENERGY          2  // displays the energy
#define WP_FFTFREQ         3  // use FFT to analyze frequency
#define WP_PITCH           4  // display pitch info
#define WP_FFTSTRETCH      5  // stretched FFT based on pitch
#define WP_PHONEMES        6  // display phonemes
#define WP_FFTSTRETCHEXP   7  // stretched FFT bsed on pitch, shown in octaves
#define WP_WORD            8  // display words
#define WP_SRFEATURES      9  // speech recognition features
#define WP_SRFEATURESVOICED 10   // voiced-only features
#define WP_SRFEATURESNOISE 11 // noise-only features
#define WP_SRFEATURESPHASE 12 // phase-only features
#define WP_SRFEATURESPHASEPITCH 13 // phase-only features, pitch shifted
#define WP_SRFEATURESPCM   14 // PCM in feature


class CWaveView;
typedef struct {
   DWORD                dwType;              // one of WP_XXX
   DWORD                dwSubType;           // channel number, -1 for all channels
   HWND                 hWnd;                // window for the pane
   fp                   fTop;                // top displays, from 0..1
   fp                   fBottom;             // bottom displays, from 0..1
   int                  iHeight;             // if positive is a fixed height (in pixels). If negative relative weight
   CWaveView            *pWaveView;           // wave view that's associated with
} WAVEPANE, *PWAVEPANE;

// FXPITCHSPEED - Affect pitch and speed at the same time
typedef struct {
   fp                fPitch;        // 1.0 = no change, 2.0 = 2x pitch and speed
} FXPITCHSPEED, *PFXPITCHSPEED;


// FXPSOLA - PSOLA effects
typedef struct {
   fp                fPitch;        // 1.0 = no change, 2.0 = 2x pitch and speed
   fp                fDuration;
   fp                fFormants;
} FXPSOLA, *PFXPSOLA;

// FXVOLUME - Store information about volume effect
typedef struct {
   fp                fStart;        // amplify at the start
   fp                fEnd;          // amplify at the end
} FXVOLUME, *PFXVOLUME;

// FXSINE - Store information about sine effect
typedef struct {
   fp                fStart;        // pitch at the start
   fp                fEnd;          // pitch at the end
   DWORD             dwChannel;     // 0 for all, 1 for left, 2 right, etc.
   DWORD             dwShape;       // 0 for sine, 1 for triangle, 2 for sawtooth, 3 for square
} FXSINE, *PFXSINE;


// FXMONOTONE - For monotone effect
typedef struct {
   fp                fPitch;        // pitch to use
} FXMONOTONE, *PFXMONOTONE;

// FXFILTER - Store information away about filter effect
typedef struct {
   fp                afFilter[2][10];  // filter values
   BOOL              fIgnoreEnd;    // if TRUE ignore then ending filter
} FXFILTER, *PFXFILTER;

// FXBLEND - Store information about blen effect
typedef struct {
   fp                fLoop;         // percentage of wave to loop
} FXBLEND, *PFXBLEND;


// FXACCOMPRESS - Store information for acoustic compression
typedef struct {
   fp                fCompress;         // range from 0 to .999
} FXACCOMPRESS, *PFXACCOMPRESS;

// FXECHO - Store information about echo
typedef struct {
   FXECHOPARAM       Echo;       // echo information
   HBITMAP           hBit;       // where the bitmap is stored
   PCM3DWave         pWave;      // where the echo wave is stored
} FXECHO, *PFXECHO;

class DLLEXPORT CWaveView {
   friend LRESULT CALLBACK CWaveViewWndProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
   friend LRESULT CALLBACK CWaveViewDataWndProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
   friend LRESULT CALLBACK CWaveViewMouthWndProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
public:
   ESCNEWDELETE;

   CWaveView(void);
   ~CWaveView(void);

   BOOL Init (PCM3DWave pWave);
   void UpdateAllButtons (void);
   BOOL ButtonExists (DWORD dwID);
   void SetPointerMode (DWORD dwMode, DWORD dwButton, BOOL fInvalidate = TRUE);
   void SetProperCursor (PWAVEPANE pwp, int iX, int iY);
   BOOL Save (HWND hWndParent, BOOL fForceNewName = FALSE);
   BOOL Open (HWND hWndParent);

   void InvalidateDisplay (void);
   BOOL PointInImage (PWAVEPANE pwp, int iX, int iY, double *pfImageX = NULL, double *pfImageY = NULL);
   
   void UndoUpdateButtons (void);
   void UndoAboutToChange (void);
   void UndoCache (void);
   BOOL Undo (BOOL fUndo);
   void UndoClear (BOOL fRefreshButtons = TRUE);

   void PlayCallback (HWAVEOUT hwo,UINT uMsg, DWORD_PTR dwParam1, DWORD_PTR dwParam2);
   void UpdateScroll (void);
   void RebuildPanes (void);
   BOOL FXPaste (PCM3DWave pWave);
   void EnableButtonsThatNeedSel (void);

   PCM3DWave      m_pWave;          // wave object to use
   BOOL           m_fUndoDirty;  // set to TRUE if state has changed since was last copied to undo buffer
   BOOL           m_fWorkingWithUndo;  // if TRUE, then any new changes will be ignored because
                                       // already have an undo in m_lGVSTATEUndo
   CListFixed     m_lWVSTATEUndo;      // list of undo
   CListFixed     m_lWVSTATERedo;      // list of redo

   DWORD          m_dwBrushAir;  // 0 for pen, 1 for brush, 2 for airbrush
   DWORD          m_dwBrushPoint;   // 0 for flat, 1 for rounded, 2 for pointy, 3 for very pointy
   DWORD          m_dwBrushStrength;   // 1..10
   DWORD          m_dwBrushEffect;  // 0 for raise, 1 for lower, 2 raise to max, 3 lower to max,
                                    // 4 blur, 5 sharpen, 6 noise
   BOOL           m_fAirbrushTimer; // set to TRUE if airbrush timer is on

   COLORREF       m_cBackDark;   // dark background color (not in tab)
   COLORREF       m_cBackMed;    // medium background color (for tab not selected)
   COLORREF       m_cBackLight;  // light background color (for selected tab)
   COLORREF       m_cBackOutline;   // outline color

   HWND           m_hWnd;           // window handle
   HWND           m_hWndScroll; // horizontal time scroll
   HWND           m_hWndMouth;      // where draws mouth
   PCTimeline     m_pTimeline;
   BOOL           m_fSmallWindow;   // if TRUE, draw as a small window
   PCButtonBar    m_pbarGeneral;  // button bar for general buttons
   PCButtonBar    m_pbarView;     // bar specific to the way the data is viewed
   PCButtonBar    m_pbarObject;   // bar speciifc to the object that's selected
   PCButtonBar    m_pbarMisc;     // miscellaneous
   PCIconButton   m_pUndo, m_pRedo; // undo and redo buttons
   PCIconButton   m_pPaste, m_pPasteMix;      // pase button
   PCIconButton   m_pStop, m_pPlay, m_pPlayLoop, m_pPlaySel, m_pRecord;   // stop and play buttons

   DWORD          m_adwPointerMode[3];  // what the meaning of the pointer is. IDC_XXX. [0]=left,[1]=middle,[2]=right
   DWORD          m_dwPointerModeLast; // last pointer mode used, so can swap back and forth
   POINT          m_pntButtonDown;  // point on the screen where the button was pressed down
   BOOL           m_dwButtonDown;    // 0 if no button is down, 1 for left, 2 for middle, 3 for right
   BOOL           m_fCaptured;      // set to true if mouse captured
   POINT          m_pntMouseLast;   // if the button is down, this reflects where the button last was
   CListFixed     m_listNeedSelect; // list of buttons that are enabled or disabled based on selection
   CListVariable  m_lWAVEPANE;      // list of panes

   int            m_iViewSampleLeft;   // sample viewed on left side
   int            m_iViewSampleRight;  // sample viewend on right side
   int            m_iSelStart;         // selection start
   int            m_iSelEnd;           // selection end

   // playback
   WAVEHDR        m_aPlayWaveHdr[WVPLAYBUF]; // headers for playback
   CMem           m_memPlay;           // memory for playback
   DWORD          m_dwPlayBufSize;     // playback buffer size, in bytes
   HWAVEOUT       m_hPlayWaveOut;      // wave out. Use this to indicate if playing or stopped
   BOOL           m_fPlayLooped;       // if TRUE then playback is looped
   BOOL           m_fPlayStopping;     // set to TRUE if stopping
   DWORD          m_dwPlayBufOut;      // number of playback buffers out to wave device
   DWORD          m_dwPlayTimer;       // if non-NULL, timer ID used to update playback posn
   CListFixed     m_lPlaySync;         // list of 2 DWORDs... [0] is time in waveOutGetPosition(), [1] is sample within playback buffer
   DWORD          m_dwPlaySyncCount;   // what waveOutGetPosition should be returning
   int            m_iPlayLimitMin;     // minimum sample where playback starts
   int            m_iPlayLimitMax;     // maximum sample where playback starts
   int            m_iPlayCur;          // current playback sample (to be added at next available buffer)

   // effects
   FXVOLUME       m_FXVolume;          // volume effect
   FXPITCHSPEED   m_FXPitchSpeed;      // pitch and speed
   FXPSOLA        m_FXPSOLA;           // PSOLA
   FXSINE         m_FXSine;            // sine effect
   FXMONOTONE     m_FXMonotone;        // make monotone
   FXFILTER       m_FXFilter;          // filter effect
   FXBLEND        m_FXBlend;           // blend effect
   FXACCOMPRESS   m_FXAcCompress;      // acoustic compression effect
   FXECHO         m_FXEcho;            // echo infomration
   CVoiceDisguise m_VoiceDisguise; // for voice synthesis
   int            m_iTTSQuality;       // TTS quality to use
   BOOL           m_fDisablePCM;       // disable PCM in TTS

   // phonemes
   WCHAR          m_szLexicon[256];    // default lexicon file
   PCMLexicon     m_pLex;              // lexicon to use
   //WCHAR          m_szTrainFile[256];  // current training file
   //WCHAR          m_szTTS[256];        // text-to-speech to use

private:
   LRESULT WndProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
   LRESULT DataWndProc (PWAVEPANE pwp, HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
   LRESULT ButtonDown (PWAVEPANE pwp, HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, DWORD dwButton);
   LRESULT ButtonUp (PWAVEPANE pwp, HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, DWORD dwButton);
   LRESULT MouseMove (PWAVEPANE pwp, HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
   LRESULT Paint (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

   void SetTitle (void);
   void WavePaneClear (BOOL fRefresh);
   void WavePaneRemove (DWORD dwItem, BOOL fRefresh);
   void WavePaneAdd (DWORD dwType, DWORD dwSubType, BOOL fRefresh);
   void WavePaneUpdateScroll (PWAVEPANE pwp);
   void TimelineUpdate (void);
   void Record (HWND hWnd);
   BOOL Play (HWND hWnd, BOOL fLooped);
   void Stop (void);
   BOOL PlayAddBuffer (PWAVEHDR pwh, BOOL fInCritSec);
   BOOL ReplaceRegion (DWORD dwStart, DWORD dwEnd, PCM3DWave pWave, BOOL fSelectReplace);
   BOOL ReplaceRegion (PCM3DWave pWave);
   void Copy (void);
   BOOL Paste (void);
   BOOL PasteMix (void);
   PCM3DWave WaveFromClipboard (void);
   void ClipboardUpdatePasteButton (void);
   BOOL Settings (HWND hWnd);
   BOOL SpeechRecog (HWND hWnd);
   BOOL FXTTS (HWND hWnd);
   BOOL FXTTSBatch (HWND hWnd);
   PCM3DWave FXCopy (void);
   DWORD IsOverPhonemeDrag (PWAVEPANE pwp, fp fSample);
   DWORD IsOverWordDrag (PWAVEPANE pwp, fp fSample);
   void WordPhonemeAttach (DWORD dwIndex, BOOL fWord, DWORD dwNewSample);
   LRESULT MouthWndProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
   BOOL DrawMouth (HDC hDCDraw, RECT *pr);

   int            m_iZoomLeft;         // store zoom info
   int            m_iZoomRight;
   fp             m_fZoomTop;
   fp             m_fZoomBottom;
   HWND           m_hWndClipNext;      // next clipboad
   DWORD          m_dwPhonemeDrag;     // phoneme that's being dragged

   CRITICAL_SECTION m_csWaveOut;       // to prevent hangs on Vista
   MMTIME         m_mtLast;            // do this in callback since can't on vista
};
typedef CWaveView *PCWaveView;


/***********************************************************************************
Phonemes */

#if 0 // dead code
// PHONEMEINFO - Information about phoneme
typedef struct {
   DWORD          dwCategory;       // combination of PIC_XXX
   WCHAR          szPhone[4];        // pointer to 4-character name, pluss fill
   WCHAR          szSample[12];     // sample word
   DWORD          dwShape;          // combination of PIS_XXX
} PHONEMEINFO, *PPHONEMEINFO;

DLLEXPORT WCHAR *PhonemeSilence (void);
DLLEXPORT DWORD PhonemeNum (void);
DLLEXPORT PPHONEMEINFO PhonemeGet (DWORD dwIndex);
DLLEXPORT PPHONEMEINFO PhonemeGet (WCHAR *pszName);
DLLEXPORT DWORD PhonemeFind (WCHAR *pszName);
#endif 0



/*********************************************************************************
CWaveCache.cpp */
DLLEXPORT BOOL WaveCacheRelease (PCM3DWave pWave);
DLLEXPORT PCM3DWave WaveCacheOpen (char *pszFile);
DLLEXPORT PCM3DWave WaveCacheOpen (PWSTR pszFile);





/***************************************************************************
TextParse.cpp */
class CMLexicon;
typedef CMLexicon *PCMLexicon;
class DLLEXPORT CTextParse {
public:
   ESCNEWDELETE;

   CTextParse (void);
   ~CTextParse (void);
   CTextParse *Clone (void);

   BOOL Init (LANGID LangID, PCMLexicon pLexicon);

   BOOL ParseFromMML (PCMMLNode2 pNode, BOOL fIncludePron, BOOL fRandomPron);
   PCMMLNode2 MMLToPreParse (PWSTR pszText, BOOL fTagged);
   PCMMLNode2 ParseFromMMLText (PWSTR pszText, BOOL fIncludePron, BOOL fRandomPron);
   PCMMLNode2 ParseFromText (PWSTR pszText, BOOL fIncludePron, BOOL fRandomPron);
   BOOL ParseToText (PCMMLNode2 pNode, PCMem pMemText);
   BOOL ParseAddPronunciation (PCMMLNode2 pNode, BOOL fRandomPron);

   PWSTR Word (void);
   PWSTR Text (void);
   PWSTR Punctuation (void);
   PWSTR Number (void);
   PWSTR Context (void);
   PWSTR Pronunciation (void);
   PWSTR POS (void);
   PWSTR RuleDepthLowDetail (void);
   PWSTR ParseRuleDepth (void);

private:
   BOOL ParseIndividualText (PWSTR psz, PCMMLNode2 pNode, DWORD dwNode);
   BOOL ParseNumberToWords (PWSTR psz, PCMMLNode2 pNode, DWORD dwNode);
   BOOL ParseContainNumber (PWSTR psz, PCMMLNode2 pNode, DWORD dwNode);
   BOOL ParseNaturalToWords (int iVal, PWSTR pszContext, PCMMLNode2 pNode, DWORD dwNode);
   BOOL ParseTwoDigitsToWords (int iVal, BOOL fOrdinal, BOOL fInsertOh,
      PCMMLNode2 pNode, DWORD dwNode);
   BOOL ParseThreeDigitsToWords (int iVal, BOOL fOrdinal,
                                        PCMMLNode2 pNode, DWORD dwNode);
   BOOL ParseSixDigitsToWords (int iVal, BOOL fOrdinal,
                                        PCMMLNode2 pNode, DWORD dwNode);
   BOOL ParseNineDigitsToWords (int iVal, BOOL fOrdinal,
                                        PCMMLNode2 pNode, DWORD dwNode);
   BOOL ParseTwelveDigitsToWords (int iVal, BOOL fOrdinal,
                                        PCMMLNode2 pNode, DWORD dwNode);

   BOOL ChineseUse (void);
   DWORD ChineseFindPinyinWord (PWSTR pszText, PCMem pMemWord);

   WCHAR IsPunct (WCHAR cTest);
   BOOL IsWordSplitPunct (WCHAR cTest);
   DWORD NaturalParse (PWSTR psz, BOOL fCanHaveComma, int *piVal);
   DWORD IntegerParse (PWSTR psz, int *piVal);
   DWORD FloatParse (PWSTR psz, fp *pfVal, DWORD *pdwDecStart);
   DWORD TimeParse (PWSTR psz, int *paiHour, int *paiMinute);
   DWORD DateParse (PWSTR psz, int *paiDay, int *paiMonth, int *paiYear);
   DWORD CurrencyParse (PWSTR psz, WCHAR *pcSymbol, int *paiWhole, int *paiCents);
   BOOL NumberInsert (PWSTR pszText, DWORD dwChars, PWSTR pszContext,
                               PCMMLNode2 pNode, DWORD dwInsert);
   BOOL WordInsert (PWSTR pszText, DWORD dwChars, PCMMLNode2 pNode, DWORD dwInsert);
   BOOL PunctuationInsert (WCHAR cPunct, PCMMLNode2 pNode, DWORD dwInsert);
   PWSTR Digit (DWORD dwValue, BOOL fFormal, BOOL fOrdinal);


   PCMLexicon        m_pLexicon;    // lexicon to use for word ambiguities.
   LANGID            m_LangID;      // langauge ID
};
typedef CTextParse *PCTextParse;

DWORD ChineseFindPinyinSyllableLength (PCMLexicon pLexicon, PWSTR pszText, BOOL fCompareAgainstLex);


/********************************************************************************
CMLexicon */

// Used for the phoneme information in LEXENGLISHPHONE
#define PIC_MAJORTYPE                  0xc0000000        // whether constant, vowel, etc.
#define PIC_CONSONANT                  0x00000000        // consonant
#define PIC_VOWEL                      0x40000000        // vowel
#define PIC_MISC                       0x80000000        // silence

#define PIC_VOICED                     0x00000001        // set if voiced, not set if unvoiced
#define PIC_PLOSIVE                    0x00000002        // set of plosive, not set if held phoneme

#define PIS_LATTEN_MASK                0x0000000f        // mask so can pull out lateral tension
#define PIS_LATTEN_PUCKER              0x00000000        // negative lateral tension, puckering
#define PIS_LATTEN_REST                0x00000001        // laterally lips at rest
#define PIS_LATTEN_SLIGHT              0x00000002        // laterally lips under some tension
#define PIS_LATTEN_MAX                 0x00000003        // laterally, lips under maximum tension

#define PIS_VERTOPN_MASK               0x000000f0        // mask to get vertical open
#define PIS_VERTOPN_CLOSED             0x00000000        // lips closed
#define PIS_VERTOPN_SLIGHT             0x00000010        // lips are slightly open
#define PIS_VERTOPN_MID                0x00000020        // lips are mid-way open
#define PIS_VERTOPN_MAX                0x00000040        // lips are opened up maximum

#define PIS_TEETHTOP_MASK              0x00000f00        // mask for top teeth
#define PIS_TEETHTOP_MID               0x00000100        // set when upper teeth visible
#define PIS_TEETHTOP_FULL              0x00000200        // set when upper teeth visible

#define PIS_TEETHBOT_MASK              0x0000f000        // mask
#define PIS_TEETHBOT_MID               0x00001000        // set when lower teeth visible
#define PIS_TEETHBOT_FULL              0x00002000        // set when lower teeth visible

#define PIS_TONGUETOP_MASK             0x000f0000        // mask for where the tongue is vertically
#define PIS_TONGUETOP_ROOF             0x00000000        // touching the roof of the mouth
#define PIS_TONGUETOP_TEETH            0x00010000        // at top-teeth level
#define PIS_TONGUETOP_BOTTOM           0x00020000        // at the bottom of the mouth

#define PIS_TONGUEFRONT_MASK           0x00f00000        // location of the tongue from dront to back
#define PIS_TONGUEFRONT_TEETH          0x00000000        // far enough forward to touch the teeth
#define PIS_TONGUEFRONT_BEHINDTEETH    0x00100000        // far enough forward to tucch the gums behind the teeth
#define PIS_TONGUEFRONT_PALATE         0x00200000        // far enough back to touch the palate

#define PIS_PHONEGROUPNUM              17                // number of phoneme groups
#define PHONEGROUPSQUARE         (PIS_PHONEGROUPNUM*PIS_PHONEGROUPNUM)
#define PIS_PHONEGROUP_MASK            0x1f000000        // so know what phoneme groups things are in
#define PIS_TOPHONEGROUP(x)            ((DWORD)(x) << 24)   // take a number and convert to a phone group
#define PIS_FROMPHONEGROUP(x)          (((x) & PIS_PHONEGROUP_MASK) >> 24) // take phoneme info and convert back
#define PIS_KEEPSHUT                   0x80000000        // if TRUE then keep this phoneme's mouth shape constant for duration

#define PIS_PHONEMEGAGROUPNUM          5                 // number of phonemes in the megagroup
#define PHONEMEGAGROUPSQUARE           (PIS_PHONEMEGAGROUPNUM*PIS_PHONEMEGAGROUPNUM)

DLLEXPORT DWORD LexPhoneGroupToMega (DWORD dwGroup);

// part of speech defines
#define POS_MAJOR_EXTRACT(x)           ((x) >> 4)        // extract major POS from POS
#define POS_MAJOR_ISOLATE(x)           ((x) & 0xf0)      // solates major POS from POS
#define POS_MAJOR_MAKE(x)              ((BYTE)(x) << 4)   // turn into POS

#define POS_MAJOR_UNKNOWN              0x00              // unknown
#define POS_MAJOR_NOUN                 0x10              // noun
#define POS_MAJOR_PRONOUN              0x20              // pronoun
#define POS_MAJOR_ADJECTIVE            0x30
#define POS_MAJOR_PREPOSITION          0x40
#define POS_MAJOR_ARTICLE              0x50              // the, a
#define POS_MAJOR_VERB                 0x60
#define POS_MAJOR_ADVERB               0x70
#define POS_MAJOR_AUXVERB              0x80              // have, can, will
#define POS_MAJOR_CONJUNCTION          0x90
#define POS_MAJOR_INTERJECTION         0xa0
#define POS_MAJOR_NUM                  0x0b              // number of major POS
#define POS_MAJOR_PUNCTUATION          POS_MAJOR_MAKE(POS_MAJOR_NUM)   // used in some functions for punctaution placeholder
#define POS_MAJOR_NUMPLUSONE           (POS_MAJOR_NUM+1)

#define MAXSTRESSES                    8                 // can't have a language with any more than this number of stresses

#define LEXPOSNGRAM                    4                 // number of word-POS in an N-gram
   // BUGFIX - Was 3, but upped to 4 for more accuracy

// LEXPHONE - Stores information about phoneme specific to lexicon
typedef struct {
   WCHAR          szSampleWord[16];    // sample word to display so user understands phoneme
   WCHAR          szPhoneLong[4+4+1];          // phoneme representation
   BYTE           bEnglishPhone;       // similar to this english phone
   BYTE           bStress;             // 0 if no stress at all, 1 if primary, 2 if secondar stress, 3, 4, 5 also acceptable
   WORD           wPhoneOtherStress;   // if phone has a primary or secondary stress (or 3+ stress), points to the no-stress version, unsorted #
} LEXPHONE, *PLEXPHONE;

// LEXENGLISHPHONE - Information about english
typedef struct {
   DWORD          dwCategory;       // combination of PIC_XXX
   WCHAR          szPhoneLong[4+4];        // pointer to 4-character name, pluss fill
   WCHAR          szSample[12];     // sample word
   DWORD          dwShape;          // combination of PIS_XXX
   DWORD          dwSAPIPhone;      // sapi phone number
   DWORD          dwSAPIViseme;     // sapi viseme number
} LEXENGLISHPHONE, *PLEXENGLISHPHONE;

// PARSERULEDEPTH - Structure for storing rule depth from parsing
typedef struct {
   BYTE              bBefore;        // depth before word starts
   BYTE              bDuring;       // depth at the word
   BYTE              bAfter;        // depth after the word
   BYTE              bFill;         // filler
} PARSERULEDEPTH, *PPARSERULEDEPTH;

// LEXPOSGUESS - Structure used when guessing POS
typedef struct {
   PWSTR          pszWord;    // pointer to the word string
   WORD           wPOSBitField;  // bit field of all possible parts of speech
   BYTE           bPOS;       // will be filled in with POS when done
   BYTE           bRuleDepthLowDetail; // Bits 0..4 are two sets of rule-depths. Filled in when fill in POS guess
   PARSERULEDEPTH ParseRuleDepth;   // full details on rule depth
   PVOID          pvUserData; // user data, not used by the lex functions
   PCListVariable plForm;     // used interally to temporarily store pointer, wont be valid or uable by caller
} LEXPOSGUESS, *PLEXPOSGUESS;


// LTSHYP - Hypthesis for letter to sound
typedef struct {
   WORD     awShard[50];      // shard storage
   DWORD    dwCurShard;       // current shard writing into
   PWSTR    pszCur;           // current text to analyze
   fp       fScore;           // score up to now
} LTSHYP, *PLTSHYP;


#define MAXNGRAMHISTORY    6           // maximum history for the NGram

// CNGramDatabase - Stores Ngrams used for lexicon
class DLLEXPORT CNGramDatabase {
public:
   ESCNEWDELETE;

   CNGramDatabase (void);
   ~CNGramDatabase (void);
   BOOL Init (DWORD dwValues, DWORD dwHistory, BYTE bEOD);
   CNGramDatabase * Clone (void);
   BOOL CloneTo (CNGramDatabase *pTo);
   PCMMLNode2 MMLTo (void);
   BOOL MMLFrom (PCMMLNode2 pNode);

   void TrainStreamLEXPOSGUESS (DWORD dwNum, PLEXPOSGUESS paStream, double fCount);
   void TrainStream (DWORD dwNum, PBYTE pabStream, double fCount);
   void TrainingApply (void);
   double ProbStream (DWORD dwNum, PBYTE pabStream);
   BOOL ProbStreamBitField (DWORD dwNum, WORD *pawStream, PCListFixed plPCLexPOSHyp);

   double               m_fTrainingCount;   // how much data went into training

private:
   DWORD IndexNGram (PBYTE pabHistory);
   DWORD IndexBackoff (PBYTE pabHistory);
   void TrainFixedBlock (PBYTE pabHistory, double fCount);
   void TrainStreamIndex (DWORD dwNum, PBYTE pabStream, int iIndex, double fCount);
   void TrainFixedBlockBitField (WORD *pawHistory, double fCount);
   void TrainStreamIndexBitField (DWORD dwNum, WORD *pawStream, int iIndex, double fCount);
   void TrainStreamBitField (DWORD dwNum, WORD *pawStream, double fCount);
   void TrainingApplySpecific (PBYTE pabHistory);
   double ProbFixedBlock (PBYTE pabHistory);
   double ProbStreamIndex (DWORD dwNum, PBYTE pabStream, int iIndex);
   BOOL HypCleanup (PCListFixed plHyp);
   BOOL HypExpand (PCListFixed plFrom, PCListFixed plTo,
                              WORD *pawPOSBitField, DWORD dwNum, DWORD dwCur);

   // user supplied
   DWORD                m_dwValues;       // number of values, from 0..m_dwValues-1 that are possible
   DWORD                m_dwHistory;      // number of dimensions in the NGram. Max MAXNGRAMHISTORY
   BYTE                 m_bEOD;           // value used if N-gram data take before/after stream, such as punctuation define

   // calculated
   DWORD                m_dwValuesPlusOne;   // m_dwValues+1
   DWORD                m_adwHistoryScale[MAXNGRAMHISTORY+1];   // scale each history element by this much
   CMem                 m_memTraining;    // training data (if used)
   CMem                 m_memNGram;       // Ngram data, including backoff
   PBYTE                m_pabNGram;       // Ngram data
   PBYTE                m_pabBackoff;     // backoff data
};
typedef CNGramDatabase *PCNGramDatabase;



/*************************************************************************************
CLexParseGroup - Object used to define a grouping of words.
*/
class DLLEXPORT CLexParseGroup {
public:
   ESCNEWDELETE;

   CLexParseGroup (void);
   ~CLexParseGroup (void);

   DWORD WordPartOfGroup (PWSTR psz, DWORD dwPOSFlags);
   BOOL Dialog (PCEscWindow pWindow, BOOL *pfWantToDelete);
   PCMMLNode2 MMLTo (void);
   BOOL MMLFrom (PCMMLNode2 pNode);
   BOOL CloneTo (CLexParseGroup *pTo);
   CLexParseGroup *Clone (void);


   // properties
   CMem                 m_memName;        // name of the group
   CMem                 m_memDesc;        // description of the group
   CListVariable        m_lGroupString;   // group comparison string
   CListFixed           m_ldwGroupCompare;// list of DWORDs for eaach lGroupString. 0 for an
                                          // exact match, 1 for match at start, 2 for match at end
   DWORD                m_dwPOS;          // part of speech limiter, 0..15. Or -1 if any POS is possible

   DWORD                m_dwIndexBeforeSort; // used for sorting. This is their index before

private:
};
typedef CLexParseGroup *PCLexParseGroup;


/*************************************************************************************
CLexParseRule - Object used to store information about a rule.
*/

// LEXPARSERULE - Rule information
typedef struct {
   WORD           wType;      // 0 for a POS, 1 for a group, 2 for a rule
   WORD           wNumber;    // POS number, group number, or rule number
   DWORD          dwFlags;    // flags, LEXPARSERULEFLAGS_XXX
} LEXPARSERULE, *PLEXPARSERULE;

#define LEXPARSERULEFLAGS_REPEAT       0x0001      // rule can be repeated 1 or more times
#define LEXPARSERULEFLAGS_OPTIONAL     0x0002      // rule is optional and can be skipped

class CLexParse;
typedef CLexParse *PCLexParse;
class DLLEXPORT CLexParseRule {
public:
   ESCNEWDELETE;

   CLexParseRule (void);
   ~CLexParseRule (void);

   void Sorted (DWORD *padwGroup, DWORD *padwRule);
   void GroupDeleted (DWORD dwGroup);
   void RuleDeleted (DWORD dwRule);
   BOOL Dialog (PCEscWindow pWindow, BOOL *pfWantToDelete, PCLexParse pLexParse);
   PCMMLNode2 MMLTo (void);
   BOOL MMLFrom (PCMMLNode2 pNode);
   BOOL CloneTo (CLexParseRule *pTo);
   CLexParseRule *Clone (void);

   // properties
   CMem                 m_memName;        // name of the group
   CMem                 m_memDesc;        // description of the group
   fp                   m_fScore;         // add/remove this to the score
   CListVariable        m_lRules;         // list of rules. Each rule is an array of LEXPARSERULE

   // used for dialog
   PCLexParse           m_pLexParse;     // main parser
   DWORD                m_dwIndexBeforeSort; // used for sorting. This is their index before

private:
};
typedef CLexParseRule *PCLexParseRule;




// LEXPARSEWORDSCRATCHRESULT - Array of results of the parse
typedef struct {
   WORD              wType;         // 0 for POS, 1 for group, 2 for rule
   WORD              wNumber;       // if group or rule, this is the rule number
   WORD              wPOS;          // if POS or group, this is the POS number (0..15) that's used
                                    // not used for rules as POS, but store as index into list
   WORD              wCase;         // used for rules, indicating the case number for the parse.
} LEXPARSEWORDSCRATCHRESULT, *PLEXPARSEWORDSCRATCHRESULT;

// LEXPARSEWORDSCRATCH - At the top of m_hRulePCListVariable for a parse
typedef struct {
   LEXPARSEWORDSCRATCHRESULT lpwsrThis;   // information about this particular entry, and where it came from
   DWORD             dwWords;       // number of words eaten up by this rule
   fp                fScore;        // score of this rule and all sub-rules
   // followed by an array of LEXPARSEWORDSCRATCHRESULT
} LEXPARSEWORDSCRATCH, *PLEXPARSEWORDSCRATCH;

// CLexParseWordScratch - Object for storing scratch parse information
class DLLEXPORT CLexParseWordScratch {
public:
   ESCNEWDELETE;

   CLexParseWordScratch (void);
   ~CLexParseWordScratch (void);

   BOOL Init (PWSTR pszWord, DWORD dwPOSFlags, DWORD dwPOSPreferred,
      PCLexParse pLexParse, PCMLexicon pLex);
   BOOL RuleAlreadyParsed (DWORD dwRule, PCListVariable *ppList);
   void RuleParsed (DWORD dwRule, PLEXPARSEWORDSCRATCH pInfo, DWORD dwNumResult);

   PWSTR             m_pszWord;     // word string
   DWORD             m_dwPOSFlags;  // bit set for each POS that word can be
   DWORD             m_dwPOSPreferred; // preferred POS, 0..16, or -1 if dont have one
   DWORD             *m_padwGroup;  // bit for each group to indicate what group it's in
   DWORD             *m_padwRule;   // bit for each rule to indicate what rule has been looked at
   CListVariable     m_alvWord[16]; // LEXPARSEWORDSCRATCH and follow on stuff for appropraite POS.
   CHashDWORD        m_hRulePCListVariable; // hash of rules that were successfully parsed, and their
                                    // PCListVariable containing the parse information. The information
                                    // is a LEXPARSEWORDSCRATCH structure and follow-on stuff
   CHashDWORD        m_hGroupPCListVariable; // like the m_hRulePCListVariable, but for group numbers
                                    // that are successful

   // set by ParseResultToString
   PARSERULEDEPTH    m_ParseRuleDepth; // rule depth info
   BYTE              m_bFinalPOS;   // final POS that's decided upon, from 0..16
   BYTE              m_bFiller;     // filler. does nothing

private:
   PCLexParse        m_pLexParse;   // parse lexicon
   PCMLexicon        m_pLex;        // lexicon
   CMem              m_mem;         // memory to store flags
};
typedef CLexParseWordScratch *PCLexParseWordScratch;



/*************************************************************************************
CLexParse - Object that handles the parsing.
*/

class DLLEXPORT CLexParse : public CEscMultiThreaded {
public:
   ESCNEWDELETE;

   CLexParse (void);
   ~CLexParse (void);

   void CreateNGramIfNecessary (void);
   void GroupDelete (DWORD dwGroup);
   void RuleDelete (DWORD dwRule);
   BOOL Dialog (PCEscWindow pWindow, PCMLexicon pLex);
   BOOL DialogGrammarProb (PCEscWindow pWindow, PCMLexicon pLex);
   void POSGuess (PLEXPOSGUESS paLPG, DWORD dwNum, PCMem pMemText, fp *pfScore, PCMLexicon pLex);
   PCMMLNode2 MMLTo (void);
   BOOL MMLFrom (PCMMLNode2 pNode, BOOL fIncludeNGrams);
   BOOL CloneTo (CLexParse *pTo);
   CLexParse *Clone (void);
   void Clear (void);
   void Sort (void);

   BOOL ParseScratch (PCLexParseWordScratch *pplpws, DWORD dwNumWord, PCMem pMemBest);
   BOOL ParseResultToString (PCLexParseWordScratch *pplpws, DWORD dwNumWord,
                                     PLEXPARSEWORDSCRATCHRESULT pResult, DWORD dwNumResult,
                                     PCMem pMemMML);
   // void Test (PCMLexicon pLex);


   // from EscMultiThreaded
   virtual void EscMultiThreadedCallback (PVOID pParams, DWORD dwParamSize, DWORD dwThread);

   // properties
   PCMLexicon           m_pLexDialog;        // lexicon used for the dialog
   CListFixed           m_lPCLexParseGroup;  // list of groups. If delete, call GroupDelete()
   CListFixed           m_lPCLexParseRule;   // list of ruels. If delete, call RuleDelete()

   // for N-gram rules
   // CMem                 m_memNGram;       // memory containing the N-gram. pow(POS_MAJOR_NUM+1,LEXPOSNGRAM) DWORDs
   PCNGramDatabase       m_pNGramSingle;    // NGram for single POS
   PCNGramDatabase       m_pNGramRules;     // NGram using the POS rules
   CListFixed           m_lGTRULE;    // list of GTRULE for the rules to reduce a sentence
private:
   PCListVariable ParseLEXPARSERULESingle (PLEXPARSERULE pRule, PCLexParseWordScratch *pplpws, DWORD dwNumWord,
      PCMem pMemScratch);
   PCListVariable ParseLEXPARSERULEMultiple (PLEXPARSERULE pRule, DWORD dwNumRule, DWORD dwFlagsOverride,
                                                     PCLexParseWordScratch *pplpws, DWORD dwNumWord,
                                                     PCMem pMemScratch);
   BOOL ParseScratchRecurse (PCLexParseWordScratch *pplpws, DWORD dwNumWord,
                                     /*PCMem pMemCur,*/ PCMem paMemScratch, PCMem pMemBest);
   void POSGuessProb (PLEXPOSGUESS paLPG, DWORD dwNum, PCMLexicon pLex);

   CRITICAL_SECTION  m_csMultiThreaded;       // for multithreaded beam search
};
typedef CLexParse *PCLexParse;

#define NUMLEXENGLISHPHONE          41    // number of english phonemes

// CMLexicon - lexicon class
class DLLEXPORT CMLexicon {
public:
   ESCNEWDELETE;

   CMLexicon (void);
   ~CMLexicon (void);

   CMLexicon *Clone (void);
   void Clear(void);
   BOOL MMLFrom (PCMMLNode2 pNode, PWSTR pszSrcFile, BOOL fIncludeNGrams);
   PCMMLNode2 MMLTo (void);
   BOOL Save (void);
   BOOL Load (void);
   DWORD MemoryTouch (void);

   PWSTR MasterLexGet (void);
   BOOL MasterLexSet (PWSTR pszFile);
   BOOL MasterLexExists (void);

   LANGID LangIDGet (void);
   void LangIDSet (LANGID LangID);
   GUID *GUIDGet (void);

   BOOL DialogMain (PCEscWindow pWindow);
   BOOL DialogLangID (PCEscWindow pWindow);
   BOOL DialogPhone (PCEscWindow pWindow);
   BOOL DialogPronMain (PCEscWindow pWindow);
   BOOL DialogPronEdit (PCEscWindow pWindow, DWORD dwWord, PWSTR pszWord,
                                DWORD dwWordSize, PCListVariable plForms);
   DWORD DialogAddWord (PCEscWindow pWindow);
   BOOL DialogPronOOV (PCEscWindow pWindow, PCBTree pTreeReview);
   BOOL DialogPronReview (PCEscWindow pWindow, PCBTree pTreeReview, BOOL fTempLex,
      PWSTR pszFind, int iFindLoc);
   BOOL DialogPOSReview (PCEscWindow pWindow, PCBTree pTreeReview, BOOL fTempLex,
                                  PWSTR pszFind, int iFindLoc);
   BOOL DialogGrammar (PCEscWindow pWindow);
   BOOL DialogLTS (PCEscWindow pWindow);
   BOOL DialogScanUnisyn (PCEscWindow pWindow);
   BOOL DialogScanMandarin (PCEscWindow pWindow);
   BOOL DialogScanMandarin2 (PCEscWindow pWindow);

   BOOL LexImportMandarin (PCEscPage pPage);
   BOOL LexImportMandarin2 (PCEscPage pPage);
   BOOL LexImportMandarin2Parse (PCEscPage pPage, PWSTR pszLine, PCMLexicon pLexEnglish, PCTextParse pTextParseEnglish);
   BOOL LexImportMandarin2ParsePOS (PCEscPage pPage, PWSTR pszLine, PWSTR pszDef, DWORD *padwPOS, PCMLexicon pLexEnglish, PCTextParse pTextParseEnglish);
   BOOL LexImportMandarinLblFile (PCEscPage pPage, PWSTR pszFileNameOnly,
                                          PWSTR pszFile, FILE *pfData);
   BOOL LexImportUnisyn (PCEscPage pPage);
   BOOL LexImportUnisynParse (PCEscPage pPage, PWSTR psz, PCListFixed plLUPT,
      BOOL fStressRemap, BOOL fPhoneSpaces, BOOL fStressSyllable);
   BOOL LexImportUnisynParsePOS (PCEscPage pPage, PWSTR psz, PCListFixed plLUPT,
                                      DWORD *pdwPOS, PWSTR pszWord);
   BOOL LexImportUnisynParsePron (PCEscPage pPage, PWSTR psz, PCListFixed plLUPT,
                                      PBYTE pszPron, DWORD dwLen, PWSTR pszWord,
                                      BOOL fStressSyllable);
   PCWSTR PhonemeSilence (void);
   DWORD PhonemeNum (void);
   PLEXPHONE PhonemeGet (PCWSTR pszName);
   PLEXPHONE PhonemeGet (DWORD dwIndex);
   DWORD PhonemeFind (PCWSTR pszName);
   DWORD PhonemeFindUnsort (PCWSTR pszName);
   PLEXPHONE PhonemeGetUnsort (DWORD dwIndex);
   void PhonemeSort (void);   // generally private
   void PhonemeAddBlank (PWSTR pszName = NULL);
   DWORD PhonemeToEnglishPhone (DWORD dwIndex, DWORD *pdwStress);
   DWORD PhonemeToGroup (DWORD dwIndex);
   DWORD PhonemeToMegaGroup (DWORD dwIndex);
   DWORD PhonemeToUnstressed (DWORD dwIndex);
   DWORD PhonemeGroupToPhone (DWORD dwGroup, BOOL fIncludeStressed, PCListFixed plRet);
   void PhonemeGroupToPhoneString (DWORD dwGroup, PSTR psz, DWORD dwBytes);
   void PhonemeGroupToPhoneString (DWORD dwGroup, PWSTR psz, DWORD dwBytes);

   DWORD Stresses (void);

   BOOL PronunciationToText (PBYTE pbPron, PWSTR pszText, DWORD dwChars);
   BOOL PronunciationFromText (PWSTR pszText, PBYTE pbPron, DWORD dwChars, DWORD *padwBadPhone);

   PCMMLNode2 TextScan (char *pszText, HWND hWnd, PCBTree pTree);
   DWORD BulkConvert (PWSTR pszText, PBYTE pbPhone, BOOL fEnd,
                             PBYTE pbTo, PCProgressSocket pProgress);
   DWORD POSBulkConvert (PWSTR pszText, BOOL fEnd, BOOL fMustBeUnknown,
                             BYTE bPOS, PCProgressSocket pProgress);

   DWORD WordNum (void);
   DWORD WordFind (PWSTR pszWord, BOOL fInsert = FALSE);
   BOOL WordGet (DWORD dwIndex, PWSTR pszWord, DWORD dwWordSize, PCListVariable plForm);
   BOOL WordGet (PWSTR pszWord, PCListVariable plForm);
   BOOL WordSet (PWSTR pszWord, PCListVariable plForm);
   BOOL WordRemove (DWORD dwIndex);
   void WordClearAll (void);
   BOOL WordExists (PWSTR pszWord);
   BOOL WordPronunciation (PWSTR pszWord, PCListVariable plForm, BOOL fUseLTS,
      PCMLexicon pMainLex, PCListVariable plDontRecurse);
   BOOL WordsFromFile (char *pszFile, PCEscPage);
   void EnumEntireLexicon (PCListFixed pListPWSTR, BOOL fRandomize = TRUE);
   BOOL WordSyllables (PBYTE pbPron, PWSTR pszWord, PCListFixed plBoundary);

   void POSGuess (PLEXPOSGUESS paLPG, DWORD dwNum);
   BOOL POSMMLParseToLEXPOSGUESS (PCMMLNode2 pNode, PCListFixed plLEXPOSGUESS,
                                          BOOL fFillPOS, PCTextParse pParse,
                                          BOOL *pfBackedOff = NULL);
   BOOL POSWaveToLEXPOSGUESS (PCM3DWave pWave, PCListFixed plLEXPOSGUESS,
                                          PCTextParse pParse,
                                          BOOL *pfBackedOff = NULL);

   // internal ui functions
   DWORD GeneratePrefix (PCProgressSocket pProgress);
   PCWSTR POSToString (BYTE bPOS, BOOL fShort = TRUE);
   BOOL POSToStatus (PCEscPage pPage, PWSTR pszStatus, PWSTR pszScroll, BYTE bPOS);
   void LexUIShowTTS (PCEscPage pPage);
   void LexUIChangeTTS (PCEscPage pPage);
   BOOL LexUIPronFromEdit (PCEscPage pPage, PWSTR pszEdit, BOOL fRewrite,
                                   PBYTE pbPron, DWORD dwPronSize,
                                   PWSTR pszWord, DWORD dwWordForm,
                                   PWSTR pszWordToSpeak, BOOL fSpeak);
   CMLexicon *ReviewListGet (void);
   BOOL ReviewProduceNext (PCBTree pTreeWords, DWORD dwMode, DWORD dwNumMax,
                                   PCMLexicon pLexTemp, PWSTR pszFind, int iFindLoc, PCListVariable plReview);
   BOOL POSReviewProduceNext (PCBTree pTreeWords, DWORD dwNumMax,
                                   PCMLexicon pLexTemp, PWSTR pszFind, int iFindLoc, PCListVariable plReview);

   void ShardParseWord (PWSTR pszWord, PBYTE pbMustMatch, WORD *pawScratch, DWORD dwScratchNum,
                                PCListVariable plShard, DWORD *padwSkipped,
                                DWORD dwScratchOffset = 0, DWORD dwSkipCur = 0);
   BOOL ShardAdd (PWSTR pszWord, PBYTE pabPron);
   BOOL ShardSomeTraining (void);
   void ShardClear (void);
   BOOL ShardLTS (PWSTR pszWord, PCListVariable plForm);
   WORD WordToPOSBitField (PWSTR psz);
   BOOL ChineseLTS (PWSTR pszWord, PCListVariable plForm, PCListVariable plDontRecurse);
   BOOL ChineseUse (void);

   // public vars that can modify
   WCHAR                m_szFile[256];    // filenaem
   //WCHAR                m_szTTSSpeak[256];   // use this TTS engine to speak samples
   DWORD                m_dwRefCount;     // reference count used by lexicon cache
   BOOL                 m_fDirty;         // set to TRUE if dirty and need to save

   CLexParse            m_LexParse;       // parse rules

private:
   // fp POSGuessInternal (PLEXPOSGUESS paLPG, DWORD dwNum, DWORD dwCur, BOOL *pfBackedOff);
   BOOL ShardAdvanceHyp (PLTSHYP pHyp, PCListFixed plAddTo);
   fp ShardCalcScore (WORD *pawShard, DWORD dwNum);
   fp ShardCalcScoreOnNGram (WORD *pawShard, DWORD dwNum, DWORD dwIndex);

   void LexUnisynParseToToken (PWSTR psz, DWORD dwMajorID, PCListFixed plLUPT, BOOL fStressRemap = FALSE, DWORD dwNumTones = 0);

   BOOL                 m_fExceptions;    // if TRUE this is an exceptions lexicon
   WCHAR                m_szMaster[256];  // master lexicon filename
   CMLexicon            *m_pMasterLex;    // pointer to the master lexicon

   LANGID               m_LangID;         // language ID
   GUID                 m_gID;            // guid for the lexicon
   CListFixed           m_lLEXPHONE;      // list of lexicon phones
   CListFixed           m_lPLEXPHONESort; // list of PLEXPHONE that convert from sorted phoneme index to unsorted, -1 index points to silence
   CMem                 m_memWords;       // memory containing words. m_dwCurPosn is limit of words
                                          // For each word, contains WCHAR string, BYTE indicating how many forms, forms (see below), and WORD align
                                          // For each form contains BYTE for part-of-speech and null-terinated string for phonemes
   CListFixed           m_lWords;         // list of words. Array of DWORDs that are byte-offset into m_memWords

   CListFixed           m_lLEXSHARDNGRAM; // list of Ngrams for the LTS shards
   CListFixed           m_lPCLexShard;    // list of shard objects
   DWORD                m_dwStresses;     // number of stresses calculated
};
typedef CMLexicon *PCMLexicon;

DLLEXPORT PCMLexicon MLexiconCacheOpen (PWSTR pszFile, BOOL fCreateIfNotExist);
void MLexiconCacheAddRef (PCMLexicon pLex);
DLLEXPORT BOOL MLexiconCacheClose (PCMLexicon pLex);
DLLEXPORT void MLexiconCacheShutDown (BOOL fForce);
DLLEXPORT DWORD MLexiconEnglishPhoneNum (void);
DLLEXPORT PCWSTR MLexiconEnglishPhoneSilence (void);
DLLEXPORT PLEXENGLISHPHONE MLexiconEnglishPhoneGet (WCHAR *pszName);
DLLEXPORT PLEXENGLISHPHONE MLexiconEnglishPhoneGet (DWORD dwIndex);
DLLEXPORT DWORD MLexiconEnglishPhoneFind (WCHAR *pszName);
DLLEXPORT DWORD MLexiconEnglishGroupEvenSmaller (DWORD dwGroup);


DEFINE_GUID(GUID_MLexicon, 
0xa418d433, 0x2944, 0xc661, 0x1c, 0x8a, 0x12, 0xb2, 0x8f, 0xa8, 0xfb, 0xb8);

DLLEXPORT BOOL MLexiconOpenDialog (HWND hWnd, PWSTR pszFile, DWORD dwChars, BOOL fSave);



/***********************************************************************************
SpeechRecog */
class CPhoneme;
typedef CPhoneme* PCPhoneme;

class CWaveDirInfo;
typedef CWaveDirInfo *PCWaveDirInfo;

class CWaveToDo;
typedef CWaveToDo *PCWaveToDo;

DWORD FuncWordGroup (DWORD dwIndex);
fp FuncWordWeight (DWORD dwGroup);

// CVoiceFile - Store information about someone's voice
class DLLEXPORT CVoiceFile {
public:
   ESCNEWDELETE;

   CVoiceFile (void);
   ~CVoiceFile (void);
   PCMMLNode2 MMLTo (void);
   BOOL MMLFrom (PCMMLNode2 pNode, PWSTR pszSrcFile);
   CVoiceFile *Clone (void);
   BOOL Save (WCHAR *szFile);
   BOOL Open (WCHAR *szFile = NULL);

   PCPhoneme PhonemeGet (WCHAR *pszName, BOOL fCreateIfNotExist = FALSE);
   BOOL PhonemeRemove (WCHAR *pszName);
   DWORD PhonemeNum (void);
   PCPhoneme PhonemeGet (DWORD dwIndex);

   void PrepForMultiThreaded (void);

   BOOL LexiconSet (PWSTR pszLexicon);
   PWSTR LexiconGet (void);
   BOOL LexiconRequired (void);
   PCMLexicon Lexicon (void);

   BOOL TrainRecordings (HWND hWnd, BOOL fLessPickey, BOOL fSimple);
   BOOL TrainSingleWave (PCM3DWave pWave, HWND hWnd, PCProgress pProgress, BOOL fLessPickey, BOOL fSimple,
      PCHashString phFunctionWords, BOOL fFillFunctionWords);
   void TrainClear (void);
   BOOL Recognize (PWSTR pszText, PCM3DWave pWave, BOOL fHalfExamples, PCProgressSocket pProgress);
   BOOL FillWaveWithTraining (PCM3DWave pWave);
   DWORD TrainVerifyAllTrained (PCMem pMem);

   DWORD Recommend (HWND hWnd);

   BOOL DialogMain (PCEscWindow pWindow);

   // can modify
   WCHAR             m_szFile[256];    // file for the voice file
   WCHAR             m_szDefTraining[256];   // SRtrain file to use for default training
   BOOL              m_fForTTS;        // if TRUE training for TTS, so require more training data
   BOOL              m_fCDPhone;       // if TRUE using context dependent phones

   // can look at but dont delete
   PCWaveDirInfo     m_pWaveDir;       // list of wave files
   PCWaveToDo        m_pWaveToDo;      // to do list

private:
   void HypAdvanceAll (DWORD dwTime, PCListFixed plPCSRHyp);
   void HypRemoveDuplicates (PCListFixed plPCSRHyp);
   void HypPrune (PCListFixed plPCSRHyp, PCM3DWave pWave, BOOL fHalfExamples);
   BOOL HypRecognize (PWSTR pszText, PCM3DWave pWave, BOOL fHalfExamples, PCProgressSocket pProgress);

   CHashString       m_hPCPhoneme;     // list of phonemes
   PCTextParse       m_pTextParse;     // parses the text
   WCHAR             m_szLexicon[256];      // lexicon to use
   PCMLexicon        m_pLex;           // lexicon used, can read but dont modify
   DWORD             m_dwTooShortWarnings;   // number of times that has been warned about being too short
};


DLLEXPORT BOOL VoiceFileOpenDialog (HWND hWnd, PWSTR pszFile, DWORD dwChars, BOOL fSave);
DLLEXPORT void VoiceFileDefaultGet (PWSTR pszFile, DWORD dwChars);
DLLEXPORT void VoiceFileDefaultGet (PCEscPage pPage, PWSTR pszControl);
DLLEXPORT void VoiceFileDefaultSet (PWSTR pszFile);
DLLEXPORT BOOL VoiceFileDefaultUI (HWND hWnd);




/*************************************************************************************
CMTTS */

#define TTSDEMIPHONES               2        // each phone is split into this many emiphones

#define TRIPHONESPECIFICMISMATCH    16  // number of triphone mismatches to store
#define TRIPHONEGROUPMISMATCH       (TRIPHONESPECIFICMISMATCH/2) // for megaphone results
#define TRIPHONEMEGAGROUPMISMATCH   (TRIPHONEGROUPMISMATCH/2) // for megaphone results

// MISMATCHINFO - Used to store mismatch unit scores
typedef struct {
   BYTE           bLeft;         // left context. Meaning depends on dwMismatchAccuracy
   BYTE           bRight;        // right context. Meaning depends on dwMismatchAccuracy
   BYTE           abRank[TTSDEMIPHONES];  // rank to use. Set to 0 if invalid
} MISMATCHINFO, *PMISMATCHINFO;

#ifndef SRACCURACY_PSYCHOWEIGHTS_DELTA_000
fp SubSRFEATURECompare (DWORD dwNumDataPoints,
                        fp *pafVoicedAMain, fp *pafNoiseAMain, fp *pafVoicedADelta, fp *pafNoiseADelta, int iStartA, int iEndA,
                        fp *pafVoicedBMain, fp *pafNoiseBMain, fp *pafVoicedBDelta, fp *pafNoiseBDelta, int iStartB, int iEndB,
                        fp *pfEnergyA, fp *pfEnergyB);
#else
fp SubSRFEATURECompare (DWORD dwNumDataPoints,
                        fp *pafVoicedAMain, fp *pafNoiseAMain, int iStartA, int iEndA,
                        fp *pafVoicedBMain, fp *pafNoiseBMain, int iStartB, int iEndB,
                        fp *pfEnergyA, fp *pfEnergyB);
#endif

void DetermineBlend (BOOL fFullSRFEATURE, PSRFEATURE psrfA, PSRFEATURE psrfB, fp fScaleA, fp fScaleB,
                            BOOL fHalfWay, fp *pafOctaveBend, fp *pafOctaveScale);
void SRFEATUREBendAndScale (BOOL fFullSRFEATURE, PSRFEATURE psrfOrig, PSRDETAILEDPHASE pSDPOrig,
                            fp *pafOctaveBend, fp *pafOctaveScale, PSRFEATURE psrfMod, PSRDETAILEDPHASE pSDPMod);

class CMTTSTriPhoneAudio;
typedef CMTTSTriPhoneAudio *PCMTTSTriPhoneAudio;
class CMTTSTriPhonePros;
typedef CMTTSTriPhonePros *PCMTTSTriPhonePros;
class CTTSPunctPros;
typedef CTTSPunctPros *PCTTSPunctPros;
class CSentenceSyllable;
typedef CSentenceSyllable *PCSentenceSyllable;


#define NUMLEXWORDEMPH     5        // number of word groups to store word emphasis for, so get top 4096 words
#define NUMFUNCWORDS       50       // number of function words that affect prosody to L/R
#define TTSPROSNGRAM       2        // total number of N-grams = (TTSPROSNGRAMBIT+TTSPROSNGRAM)*2+1, TTSPROSNGRAM = l/r amount + 1 center
#define TTSPROSNGRAMBIT    1        // extra tail and header to n-gram that only cares about punctuation POS
#define NUMPROSWORDLENGTH  20       // number of phoneme lengths to compare speed with
#define NUMPHONEEMPH       2        // depth into the word to see how much phonemes near POS are emphasized (used for left/right emphasis)
#define PHONEPOSBIN        16       // number of bis for POS. Use the high nibble of POS

#define NUMPROSODYMODELCOMMON                   4     // number of bins for common prosody model words
#define FUNCWORDGROUPPERPROSODYMODELCOMMON      2     // number of function word groups per prosody model common
#define NUMFUNCWORDGROUP      ((NUMPROSODYMODELCOMMON-1)*FUNCWORDGROUPPERPROSODYMODELCOMMON)  // number of function word groups, for frequency calculations

// TTSNGRAM - for storing ngram information for stress
typedef struct {
   BYTE           bPitch;     // pitch scale, 100 = 1.0, linear.
                              //If pitch == 0 then ngram not valid because not enough points
   BYTE           bVol;       // volume calse, 100 = 1.0, linear
   BYTE           bDur;       // duration scale, 100 = 1.0, linear
   BYTE           bFlags;     // 0x01 = pause to left of center word because of ngram
} TTSNGRAM, *PTTSNGRAM;


// TTSGLOBALSTATE - Information about the current speaking rate, etc.
typedef struct {
   // set by prosody
   fp             fProsodyAvgPitch;        // average pitch in hz
   fp             fProsodyWPM;             // words per minute
   fp             fProsodyVol;             // volume scale, 1.0 = no change
   fp             fProsodyPitchExpress;    // pitch expressiveness, 1.0 = no change, 2.0 = 2x as expressive, etc.
   //fp             fProsodyAvgSyllableDur;  // average syllable duration, in seconds. (This will need the prosody WPM applied.)

   // emtions, effect applied somewhat later
   fp             fEmotionWhisper;  // 0..100
   fp             fEmotionShout;
   fp             fEmotionQuiet;
   fp             fEmotionHappy;
   fp             fEmotionSad;
   fp             fEmotionAfraid;
   fp             fEmotionDrunk;

   // derived values
   fp             fDerAvgPitch;        // average pitch in hz
   fp             fDerWPM;             // words per minute
   fp             fDerVol;             // volume scale, 1.0 = no change
   fp             fDerPitchExpress;    // pitch expressiveness, 1.0 = no change, 2.0 = 2x as expressive, etc.
   fp             fDerDurationExpress; // how expressive the voice is about duration
   fp             fDerVolumeExpress;   // how expressive the voice is about volume
   fp             fDerShout;           // 0 if none, 1 if full shout, -1 if speak quietly
   fp             fDerWhisper;         // 0 if none, 1 if full whisper
   fp             fDerMicropauses;     // micropause value, 0.5 normal, 1.0 => pause rarely, 0.0 => pause always
   //fp             fDerAvgSyllableDur;  // average syllable duration, in seconds. (This will need the prosody WPM applied.)
} TTSGLOBALSTATE, *PTTSGLOBALSTATE;


// TTSACCENTRULE - Rule for chaning from one phoneme to another
typedef struct {
   BYTE           abOrig[8];        // original phonemes
   BYTE           abNew[8];         // new phonemes
   DWORD          dwLoc;            // 0 if anywhere in word, 1 if at start, 2 if at end
} TTSACCENTRULE, *PTTSACCENTRULE;

class CMTTSSubVoice;
typedef CMTTSSubVoice *PCMTTSSubVoice;
class CTTSProsody;
typedef CTTSProsody *PCTTSProsody;

// TTSVOICEMOD - Voice modification information
#define TTSVOICEMODMAXPROSODY    10    // maximum prosody modules to incorporate
typedef struct {
   PCMTTSSubVoice          pSubVoice;        // sub-voice to use
   PCMLexicon              pLex;          // lexicon to use. If NULL then just use master voice's lexicon
   PCTTSProsody            apTTSProsody[TTSVOICEMODMAXPROSODY];   // prosody to use
} TTSVOICEMOD, *PTTSVOICEMOD;


#define NUMTRIPHONEGROUP      3        // number of ways to group triphones
#define NUMPROSODYTTS      4        // max number of imported prosody models



// CProgressWaveTTSToWave - Class that appends to existing wave
class CProgressWaveTTSToWave : public CProgressWaveTTS {
public:
   ESCNEWDELETE;

   virtual BOOL TTSWaveData (PCM3DWave pWave);
   virtual BOOL TTSSpeedVolume (fp *pafSpeed, fp *pafVolume);

   // should fill these in
   PCM3DWave         m_pWave;          // wave to fill in
};

typedef CProgressWaveTTSToWave *PCProgressWaveTTSToWave;

// CMTTSSubVoice - Information about a sub-voice
class DLLEXPORT CMTTSSubVoice {
public:
   ESCNEWDELETE;

   CMTTSSubVoice (void);
   ~CMTTSSubVoice (void);

   CMTTSSubVoice *Clone (void);
   BOOL CloneTo (CMTTSSubVoice *pTo);
   PCMMLNode2 MMLTo (void);
   BOOL MMLFrom (PCMMLNode2 pNode, PWSTR pszSrcFile);

   BOOL Dialog (PCEscWindow pWindow, PCMTTS pTTS);
   BOOL MixTogether (DWORD dwNumVoice, CMTTSSubVoice **papSVVoice, fp *pafWeightVoice,
                                 DWORD dwNumPros, CMTTSSubVoice **papSVPros, fp *pafWeightPros,
                                 DWORD dwNumPron, CMTTSSubVoice **papSVPron, fp *pafWeightPron);

   PCMLexicon Lexicon (BOOL fBackoffToMaster, PCMTTS pMaster);
   PWSTR LexiconGet (void);
   BOOL LexiconSet (PWSTR pszLexicon);
   BOOL LexiconRequired (void);

   CMem              m_memName;        // name string
   CVoiceDisguise    m_VoiceDisguise;        // modifies voice synthesis
   CListFixed        m_lTTSACCENTRULE;    // list of accent rules
   fp                m_fBlurPitch;        // blur pitch over this many seconds
   fp                m_fBlurVolume;       // blur volume over this many seconds
   fp                m_fPhoneRiseAccentuate; // amount to scale phone rise by
   fp                m_fPhonePitchAccentuate;   // amount to accentuate phone's pitch by
   CPoint            m_pPOSAccentuate;    // accentaute POS results by this, [0]=pitch, [1]=vol, [2]=dur
   // CPoint            m_pPunctAccentuate;  // accentaute POS results by this, [0]=pitch, [1]=vol, [2]=dur
   fp                m_fDurWordStart;     // duration scale for phoneme at start of word
   fp                m_fDurWordEnd;       // duration scale for phoneme at end of word
   fp                m_fMicroPauseThreshhold;   // normally 0.5, but if 1 then only if absolutely sure, 0 always
                                          // pause between words
   CListFixed        m_lDurPerPhone;      // duration scale per phoneme, array of floats
   fp                m_fAvgPitch;         // average pitch
   //fp                m_fAvgSyllableDur;   // average duration of a syllable, in seconds
   DWORD             m_dwWordsPerMinute;  // average words per minute
   WCHAR             m_szLexicon[256];    // lexicon filename

   WCHAR             m_aszProsodyTTS[NUMPROSODYTTS][256]; // TTS file to use for prosody, for derived tts
   PCTTSProsody      m_pCTTSProsody;   // prosody for sub-voice

private:
   // created at runtime
   PCMLexicon        m_pLexMain;       // from the lexicon cache
};
typedef CMTTSSubVoice *PCMTTSSubVoice;


// SRANALBLOCK - Block of analysis information needed for speech recognition
typedef struct {
   SRFEATURESMALL    sr;            // features, normalized to an energy of PHONESAMPLENORMALIZED
   fp                fEnergy;       // energy of sr, from SRFEATUREEnergyCalc(), before normalization
   fp                fEnergyAfterNormal;  // energy after noramlized, should be PHONESAMPLENORMALIZED, but never exactly right
   PCListFixed       plCache;       // pointer to a list of cache inforamtion,
                                    // initialized to sizeof SRANALCACHE
} SRANALBLOCK, *PSRANALBLOCK;

// THREADWAVECALCINFO - Info for calculating thread when first start up
typedef struct {
   PCMTTS            pThis;         // so can find self
   HANDLE            hThread;       // thread
   DWORD             dwInitial;     // initial value
   DWORD             dwModulo;      // modulo value
   DWORD             dwThreadID;    // thread iD
} THREADWAVECALCINFO, *PTHREADWAVECALCINFO;


// SYNTHUNITJOININFO - Information about the join
typedef struct {
   DWORD          dwBestA;          // best offset for center unit's right side, -1 if no valid info
   DWORD          dwBestB;          // best offset for right unit's left side, -1 if no valid info
   fp             fJoinScore;       // how good the join is
} SYNTHUNITJOININFO, *PSYNTHUNITJOININFO;


#define MAXSYNGENFEATURESCANDIDATES       10    // maxmimum number of candidates

// SYNGENFEATURECANDIDATE - Candidate for synthesis. Several candidates are passed in and
// the candidate with the best acoustic score is synthesized
typedef struct {
   DWORD       *padwPhone; // - Pointer to an array of phonemes. Each phoneme is the
               //unsorted phone number. The high byte (<< 24) contains 0 if the phone
               //is in the middle of a word, 1 on left side, 2 on right side, 3 word by itself
   PWSTR       *papszWord; // - Words assocaited with each phoneme. If the same word is assocaited
               // with several phonemes all the phonemes point to the same string. This
               // can be null, but if it is then the words won't be properly marked
               // in the wave.
   DWORD       *padwDur; // - Duration for each phone in SRFEATURE time-units (1/100th sec)
   fp          *pafVol; // - Array of volumes, one per phoneme. See fVolAbsolute
   fp          *pafAccent; // array of accent (-1.0 to 1.0), one per phoneme.
   BOOL        fVolAbsolute; // - If TRUE pafVol is the absolute volume to use (so can get this
               // from transplanted prosody); in which case it's the CalcSREnergyRange() of
               // the phoneme. If FALSE, relative volume.
   BOOL           fAbsPitch; //  - If TRUE, synthesizeing with aboslute pitch. If FALSE,
                  // then relative pitch, so can approximate unite
   PCListVariable plPitchF0Extra; // - This list has one element per phoneme to indicate the
               //fundamental pitch of the voice (in Hz divided by fuji, so 1.0 = use fuji pitch). Each element is N * sizeof(fp),
               //where N is from 0 to whatever. It indicates the number of pitch points
               //stored per phoneme.
   PCListVariable plPitchF0Fuji; // like plPitchF0Extra, but fujisaki pitch in Hz
   PCListFixed    plBookmarkIndex;  // list of DWORDs for index of bookmark
   PCListVariable plBookmarkString; // one bookmark string per index
   DWORD       dwNum; // - Number of phonemes

   // used only by SynthGenFeatures()
   DWORD       *padwWord; // - Word (from m_pLexWords) associated with each phoneme, or -1 if no
               //known word is associated

   // used only by SynthGenWaveInt()
   PTTSGLOBALSTATE paTTSGS; //  - Global state per phoneme. Can be NULL, but no emotion then.

} SYNGENFEATURESCANDIDATE, *PSYNGENFEATURESCANDIDATE;

// position of phoneme in word
#define WORDPOS_WORDSTART     0x01     // phoneme is at the beginning of a word
#define WORDPOS_WORDEND       0x02     // phoneme is at the end of a word
#define WORDPOS_SYLSTART      0x04     // phoneme is at the start of a syllable
#define WORDPOS_SYLEND        0x08     // phoneme is at the end of a syllable
#define WORDPOS_WORDONLY(x)   ((x) & (WORDPOS_WORDSTART | WORDPOS_WORDEND))   // keep only the word parts
#define WORDPOS_MAX           ((WORDPOS_WORDSTART | WORDPOS_WORDEND | WORDPOS_SYLSTART | WORDPOS_SYLEND) + 1)
                                    // maximum value needed to store all word pos's
#define WORDPOS_MAXONLY(x)    ((x) % WORDPOS_MAX) // make sure it's within max

#define WORDPOS_EXTRA_PLOSIVE 0x10     // used as scratch space in GenerateUNITOPTION and a few others
#define WORDPOS_EXTRA_VOICED  0x20     // used as scratch space in GenerateUNITOPTION and a few others


// TTSTARGETCOSTS - Structure that contain target costs calculated for a specific voice
typedef struct {
   float          afUnitScorePitchPerPhone[NUMLEXENGLISHPHONE][MAXSTRESSES][2];
   float          afUnitScorePCMPitchPhone[NUMLEXENGLISHPHONE][MAXSTRESSES][2];
   float          afUnitScorePCMPitchPSOLAPhone[NUMLEXENGLISHPHONE][MAXSTRESSES][2];
   float          afUnitScoreEnergyPerPhone[NUMLEXENGLISHPHONE][MAXSTRESSES][2];
   float          afUnitScoreDurationPerPhone[NUMLEXENGLISHPHONE][MAXSTRESSES][2];
   float          afUnitScoreFuncPerPhone[NUMLEXENGLISHPHONE][MAXSTRESSES];
   float          afUnitScoreMismatchedWordSylPos[WORDPOS_MAX];
   float          afUnitScoreNonContiguousMid[PIS_PHONEGROUPNUM];
   float          afUnitScoreNonContiguous[PIS_PHONEGROUPNUM][PIS_PHONEGROUPNUM];
   float          afUnitScoreLRMismatch[PIS_PHONEGROUPNUM][5][2][6];
   float          afUnitScorePitchPSOLAPitchCertaintyPerPhone[NUMLEXENGLISHPHONE][MAXSTRESSES][2];
   float          afUnitScoreDurationPSOLAPitchCertaintyPerPhone[NUMLEXENGLISHPHONE][MAXSTRESSES][2];
   float          afUnitScoreAccentPerPhone[NUMLEXENGLISHPHONE][MAXSTRESSES][2];
} TTSTARGETCOSTS, *PTTSTARGETCOSTS;


// COMPARESYLINFO - Penalties used for comparing syllables
typedef struct {
   // objectively derived
   fp                fDuration;        // how much duration affects, per doubling
   fp                fEnergy;          // how much energy effects, per doubling
   fp                fAccent;          // how much accent effects, per point
   fp                fPitchF0Extra;           // how much pitch affects, per octave
   fp                fPitchF0Fuji;           // how much pitch affects, per octave
   fp                fFuncWord;        // funtion word mismatch
   fp                fWordPosMismatchStart;  // penalty if start of syllable mismatch
   fp                fWordPosMismatchEnd; // penalty if end of syllable mismatch

   // derived from objectively calculated valyes
   fp                fDurationSkew;      // how much duration skew affects, per doubling
   fp                fWordMismatchPenalty;   // penalty to score if mismatched word
   fp                fPunctMismatchPenalty;  // penalty to score if mismatched penalty
   fp                fVeryBadScore;    // definition of a very bad score
   fp                fRandomize;       // default randomization
   fp                fCrossSentencePenalty;  // penalty if cross sentence
} COMPARESYLINFO, *PCOMPARESYLINFO;

#define SSPITCHPOINTS            5        // 5 pitch points per syllable


// WRDEMPH - Info on pitch, energy, and duration
typedef struct {
   fp             afPitchF0Extra[SSPITCHPOINTS]; // on top of fuji, LINEAR!!!
   fp             fPitchF0Fuji;
   fp             fEnergyAvg;
   fp             fDur;
} WRDEMPH, *PWRDEMPH;

class CTTSWave;
typedef CTTSWave *PCTTSWave;

class CPhaseModel;
typedef CPhaseModel *PCPhaseModel;

class DLLEXPORT CMTTS : public CEscMultiThreaded {
   friend class CMTTSSubVoice;

public:
   ESCNEWDELETE;

   CMTTS (void);
   ~CMTTS (void);

   CMTTS *Clone (void);
   PCMMLNode2 MMLTo (void);
   BOOL MMLFrom (PCMMLNode2 pNode, PWSTR pszSrcFile, BOOL fThreadSafe);
   BOOL Save (PWSTR pszFile);
   BOOL Open (PWSTR pszFile, BOOL fThreadSafe);
   DWORD MemoryTouch (void);
   void MemoryCacheFree (fp fPercentJoin, fp fPercentWaves);
   void FillInTriPhoneIndex (void);

   PCPhaseModel PhaseModelGet (DWORD dwPhoneme, BOOL fCreateIfNotExist);

   // speak
   BOOL SynthGenWave (PCM3DWave pWave, DWORD dwSamplesPerSec, PWSTR pszText, BOOL fTagged, int iTTSQuality, BOOL fDisablePCM, PCProgressSocket pProgress,
      PTTSVOICEMOD pVoiceMod = NULL, PCProgressWaveTTS pProgressWave = NULL);
   BOOL SynthGenWaveInt (DWORD dwCandidates, PSYNGENFEATURESCANDIDATE paCandidates,
                             DWORD dwSamplesPerSec,
                             int iTTSQuality, BOOL fDisablePCM, PCProgressSocket pProgress,
                              PTTSVOICEMOD pVoiceMod, PCProgressWaveTTS pProgressWave);
   BOOL SynthGenWaveIntNewWave (DWORD dwCandidates, PSYNGENFEATURESCANDIDATE paCandidates,
                             DWORD dwSamplesPerSec,
                             int iTTSQuality, BOOL fDisablePCM, PCProgressSocket pProgress,
                              PTTSVOICEMOD pVoiceMod, PCProgressWaveTTS pProgressWave);
   BOOL PhaseDetermine (PCM3DWave pWave, fp fPitchScaleToCounteractVoiceMod, PCListFixed plWSF);
   BOOL PhaseWriteToWave (PCM3DWave pWave, PCListFixed plWSF);


   // analysis
   BOOL SynthDetermineTriPhoneAudio (fp fAvgPitchF0All, fp fAvgPitchF0Fuji, DWORD *padwPhone, DWORD *padwWord,
                                         DWORD *padwDur, fp *pafVol, BOOL fVolAbsolute,
                                         fp *pafAccent,
                                         PCListVariable plPitchF0Extra, PCListVariable plPitchF0Fuji,
                                         DWORD dwNum,
                                    int iTTSQuality, BOOL fDisablePCM,
                                    PCListFixed plPCMTTSTriPhoneAudio, fp *pfBestScore,
                                    PCProgressSocket pProgress);
   BOOL SynthDetermineTriPhonePros (DWORD *padwPhone, DWORD dwNum,
                                    PCListFixed plPCMTTSTriPhonePros);
   PCMTTSTriPhonePros SynthDetermineTriPhonePros (BYTE bPhone, WORD wWordSylPos,
                                                      BYTE bLeft, BYTE bRight);
   BOOL TriPhoneAudioSet (DWORD dwOrigWave, DWORD dwOrigPhone, DWORD dwPhone, DWORD dwWord,
                         WORD wWordSylPos, BYTE bPhoneLeft, BYTE bPhoneRight,
                         DWORD dwFuncWordGroup,
                         BYTE *pabRank, PMISMATCHINFO paMMI, DWORD dwMismatchAccuracy,
                         // WORD wFlags,
                         float fOrigPitchF0Extra, float fOrigPitchF0Fuji, float fPitchDelta, float fPitchBulge,
                         float fCenterEnergy, float fLeftPitchF0Extra, float fLeftPitchF0Fuji, float fLeftEnergy, DWORD dwLeftDuration,
                         DWORD dwFeatureStart, DWORD dwFeatureEnd,
//                         PSRFEATURE pSRF, float *pafPitch, DWORD dwNum,
//                         BOOL fCanUseLeft, BOOL fCanUseRight,
                         DWORD dwTrimLeft, DWORD dwTrimRight,
                         fp fMaxEnergyInWave,
                         fp fMaxEnergyInWaveMod,
                         PBYTE pabPhoneContigous,
                         fp *pafPitchConfidence,
                         fp fAccent,
                         BOOL fCheckForExist);
   BOOL TriPhoneProsSet (DWORD dwPhone, WORD wWordSylPos, BYTE bPhoneLeft, BYTE bPhoneRight,
                         WORD wAvgDuration, short iPitchF0Extra, short iPitchF0Fuji, short iPitchDelta, short iPitchBulge,
                         fp fEnergyAvg,
                         BOOL fCheckForExist);
   void TriPhoneClearWord (DWORD dwWord);
   //PCMTTSTriPhone TriPhoneMatch (BYTE bCenter, BYTE bWordPos, BYTE bLeft,
   //                                  BYTE bRight, BYTE bFarLeft, BYTE bFarRight, DWORD dwWord);
   fp TriPhoneEnergy (PCMTTSTriPhoneAudio pti,
      DWORD *pdwCount = NULL);
   DWORD PhonemeBackoff (DWORD bPhone, PCMLexicon pLex, DWORD bSilence);

   BOOL DialogDerivedMain (PCEscWindow pWindow);

   // lexicon
   BOOL LexiconSet (PWSTR pszFile);
   PCMLexicon Lexicon (BOOL fBackoffToMaster = TRUE);

   DWORD WordRank (PWSTR pszWord, BOOL fUseHistory);
   void WordRankHistory (PWSTR pszWord);

   void TTSWaveCompress (void);
   void TTSWaveCalcSRANALBLOCK (DWORD dwInitial, DWORD dwModulo);
   PCTTSWave TTSFindWave (DWORD dwSentenceNum);
   BOOL TTSWaveAdd (WORD wFlags, PCM3DWave pWave, DWORD dwSentenceNum, DWORD dwFeatureStart, DWORD dwFeatureEnd, PSRFEATURE pSRFToUse);
   void TTSWaveCalcSRANALBLOCKCreateThreads (void);

   // access variables that might be remapped
   BOOL TriPhoneGroupGet (void);
   void TriPhoneGroupSet (DWORD dwTriPhoneGroup);
   BOOL KeepLogGet (void);
   void KeepLogSet (BOOL fKeepLog);
   void TTSTARGETCOSTSSet (PTTSTARGETCOSTS pTarget);
   BOOL FullPCMGet (void);
   void FullPCMSet (BOOL fFullPCM);
   fp AvgPitchGet (BOOL fFuji);
   void AvgPitchSet (fp fAvgPitch, fp fAvgPitchF0Fuji,
      fp fFujiRangeLow, fp fFujiRangeMed, fp fFujiRangeHigh);
   void LexFuncWordsSet (PCMLexicon pLex, DWORD dwBin);
   void EnergyPerPitchSet (char *pacEnergyPerPitch);
   char *EnergyPerPitchGet (fp fPitch);
   void EnergyPerVolumeSet (char *pacEnergyPerVolume);
   char *EnergyPerVolumeGet (fp fEnergyRatio);
   void AvgSyllableDurSet (fp fSyllableDur);
   fp AvgSyllableDurGet (void);
   DWORD WordsPerMinuteGet (void);
   void WordsPerMinuteSet (DWORD dwWordsPerMinute);
   PCMLexicon LexTrainingWordsGet (void);
   PCMLexicon LexWordsGet (void);
   BOOL LexWordsSet (PCMLexicon pNew);
   // BOOL LexWordEmphSet (PCMLexicon paLexWordEmph[NUMLEXWORDEMPH]);
   // BOOL ProsWordEmphFromWordLength (CPoint paWordEmphFromWordLength[NUMPROSWORDLENGTH]);
   BOOL ProsPhoneEmph (WRDEMPH paPhoneEmph[NUMPHONEEMPH*2][PHONEPOSBIN]);
   // BOOL LexFuncWordsSet (PCMLexicon pLexFuncWords);
   // BOOL LexWordEmphScaleSet (CPoint paLexWordEmphScale[NUMLEXWORDEMPH+1]);
   // PCMem MemMicroPauseGet (void);
   // PCMem MemNGramGet (void);
   PCTTSProsody TTSProsodyGet (void);
   // BOOL PunctProsAdd (PCTTSPunctPros pAdd);
   BOOL IsDerivedGet (void);
   void IsDerivedSet (BOOL fDerived);
   PWSTR TTSMasterGet (void);
   CMTTS * TTSMasterGet2 (void);
   BOOL TTSMasterSet (PWSTR pszMaster);
   void FillInVOICEMOD (PTTSVOICEMOD pVoiceMod, PCMTTSSubVoice pSubVoice, PCTTSProsody *papCTTSProsody);
   BOOL SAPIRegister (void);

   // from EscMultiThreaded
   virtual void EscMultiThreadedCallback (PVOID pParams, DWORD dwParamSize, DWORD dwThread);

   // variables
   WCHAR             m_szFile[256];       // filename
   DWORD             m_dwRefCount;        // used for TTS cache
   BOOL              m_fThreadSafe;       // set to TRUE if opened under thread safe conditions

   DWORD             m_dwUnits;           // number of units actually in the voice
   BOOL              m_fPauseLessOften;   // if true, don't pause as much voice training indicates,
                                          // because voice was recorded with too many pauses

   CMTTSSubVoice     m_SubVoiceGeneric;   // not saved to MML. Used as generic sub-voice placeholder
   CListFixed        m_lPCMTTSSubVoice;   // list of sub-voices for the tts
   WCHAR             m_aszProsodyTTS[NUMPROSODYTTS][256]; // TTS file to use for prosody, for derived tts
   CMem              m_memTTSTARGETCOSTS; // memory that contains target costs. Only valid if m_dwCurPosn == sizeof(TTSTARGETCOSTS)

   // for SAPI
   LANGID            m_LangID;            // language ID
   CMem              m_memSAPIToken;      // token name (to identify)
   CMem              m_memSAPIName;       // voice name
   CMem              m_memSAPIVendor;     // vendor name
   DWORD             m_dwAge;             // 0 for child, 1 for teenager, 2 for adult, 3 for senior
   DWORD             m_dwGender;          // 0 for male, 1 for female
   DWORD             m_dwVersionMajor;     // major version number
   DWORD             m_dwVersionMinor;    // minor version number
   DWORD             m_dwVersionBuild;    // build number

   // multithreaded
   CRITICAL_SECTION  m_csTTSWaveDecompress;                 // so don't decompress when not supposed to
   CRITICAL_SECTION  m_acsTTSWave[MAXRAYTHREAD];            // so dont stamp over memory
   BOOL              m_fTTSWaveCalcSRANALBLOCKWantToQuit;   // set to TRUE when want to quit
   BOOL              m_fTTSWaveDisableCompress; // set to TRUE when synhtesizing so disable compress
   DWORD             m_dwTTSWaveAnalyzing;                  // counter of number of threads analyzing
   THREADWAVECALCINFO m_aThreadWaveCalcInfo[MAXRAYTHREAD];  // to calculate info when first start up

private:
   PWSTR LexiconGet (void);
   BOOL LexiconRequired (void);
   PCMTTSSubVoice MakeSubVoice (PCMMLNode2 pNode, BOOL *pfMustDelete, PCTTSProsody *papCTTSProsody);
   PCMMLNode2 FindSubVoice (PCMMLNode2 pNode);
   BOOL SynthGenWaveNode (DWORD dwSamplesPerSec, PCMMLNode2 pNode, int iTTSQuality, BOOL fDisablePCM, PCProgressSocket pProgress,
      PTTSVOICEMOD pVoiceMod, PCProgressWaveTTS pProgressWave);
   BOOL SynthGenWaveWithProgress (DWORD dwSamplesPerSec, PCMMLNode2 pNode, int iTTSQuality, BOOL fDisablePCM, PCProgressSocket pProgress,
      PTTSVOICEMOD pVoiceMod, PCProgressWaveTTS pProgressWave);
   BOOL SpeakNodeSubDivide (PCMMLNode2 pNode, PCMMLNode2 pAppendTo,
                                PCMMLNode2 *ppClone, BOOL *pfFoundContent);
   DWORD SpeakNodeSubDivideText (PWSTR pszText, BOOL *pfFoundSentenceEnd);

   void HypExpand (DWORD dwTime, PCMem pmemFrom, PCMem pmemTo, DWORD dwPhoneValid, BYTE *pbPhone,
                       BYTE *pbWordSylPos, BYTE *pbTPhoneGroup, BYTE *pbTPhoneNoStress,
                       DWORD *pdwWord,
                       int* paiPitchF0Extra, int *paiPitchF0Fuji,
                       /*DWORD dwDurationPrev,*/ DWORD dwDuration, fp fEnergy, fp fEnergyPrev,
                       fp fAccent, BYTE bSilence,
                       DWORD dwFuncWordGroup,
                       fp fEnergyAverage, int iTTSQuality, BOOL fDisablePCM,
                       PCListFixed plUNITOPTIONLast, PCListFixed plUNITOPTION);
   PCMTTSTriPhoneAudio HypKeepBestScore (PCMem pMem, fp fEnergyAverage);
   void HypEliminateLowScores (PCMem pMem, DWORD dwMaxNum);
   BOOL SynthGenFeatures (DWORD dwCandidates, PSYNGENFEATURESCANDIDATE paCandidates,
                              PCM3DWave pWave, PCListFixed plSRDETAILEDPHASE,
                              PTTSVOICEMOD pVoiceMod, int iTTSQuality, BOOL fDisablePCM, PCListFixed plPSOLASTRUCT,
                              DWORD *pdwBestCandidate, PCProgressSocket pProgress);
   BOOL SynthArtificialProsody (DWORD *padwPhone, PWSTR *paszWord, PCMMLNode2 *papMMLWord,
                              PCPoint paPhoneMod,
                              PTTSGLOBALSTATE paTTSGLOBALSTATE, DWORD dwNum,
                              PCListFixed plDur, PCListVariable plPitchF0Extra, PCListVariable plPitchF0Fuji, PCListFixed plVol,
                              PCListFixed plAccent,
                              PTTSVOICEMOD pVoiceMod, BOOL fDisablePCM, BOOL *pfAbsPitch);
   BOOL SynthWordMMLToPhones (PCMMLNode2 pNode, DWORD dwNodeStart, PCListFixed plPhone, PCListFixed plWord,
                                  PCListFixed plMMLWord, PCListFixed plBookmarkIndex, PCListVariable plBookmarkString,
                                  PCListFixed plPhoneMod, PCListVariable plString,
                                  PTTSGLOBALSTATE pState, PCListFixed plTTSGLOBALSTATE,
                                  PTTSVOICEMOD pVoiceMod, int iTTSQuality, BOOL fTransPros, DWORD dwMultiPass,
                                  DWORD *pdwNodeEnd, fp fTempVolume, fp fTempSpeed);
   PCMMLNode2 SynthTextToWordMML (PWSTR pszText, BOOL fTagged, PCMLexicon pLex);
   PCMMLNode2 SynthMMLToWordMML (PCMMLNode2 pNode, PCMLexicon pLex);
   void GlottalPulseChange (PSRFEATURE psr, fp fOrigPitch, fp fNewPitch, BOOL fIsVoiced);
   BOOL ApplyAccent (PCListFixed plPhone, PCListFixed plWord, PCListFixed plMMLWord,
                         PCListFixed plPhoneMod, PCListFixed plTTSGLOBALSTATE, PTTSVOICEMOD pVoiceMod);

   DWORD CalcWordsPerMinute (void);

   DWORD SynthUnitCanSkip (DWORD dwDemiPhone,
                               PCMTTSTriPhoneAudio pCenter,
                               PCMTTSTriPhoneAudio pLeft, PCMTTSTriPhoneAudio pLeftDemi,
                               PCMTTSTriPhoneAudio pRight,
                               BYTE bPhoneCenter, BYTE bPhoneLeft, BYTE bPhoneRight);
   BOOL SynthUnit (fp fModDuration,
      DWORD dwDemiPhone, PCM3DWave pWave, PCListFixed plSRDETAILEDPHASE,
      PCListFixed plPitch, // fp fSnapToPitch,
      PCMTTSTriPhoneAudio pCenter, PCMTTSTriPhonePros pCenterPros,
                       DWORD dwStart, DWORD dwEnd,
                       PCMTTSTriPhoneAudio pLeft, PCMTTSTriPhonePros pLeftPros,
                       PCMTTSTriPhoneAudio pRight, PCMTTSTriPhonePros pRightPros,
                       PTTSVOICEMOD pVoiceMod,
                       DWORD dwSkip, DWORD dwSkipLeft, DWORD dwSkipRight,
                       PCMTTSTriPhoneAudio *ppLastTPAA, PCMTTSTriPhoneAudio *ppLastTPAB,
                       DWORD *pdwLastFeatureA, DWORD *pdwLastFeatureB, DWORD *pdwLastTime,
                       int iTTSQuality, PSYNTHUNITJOININFO pSUJI, PCListFixed plPSOLASTRUCT,
                       int *piDurationMod);
   BOOL SynthUnitCalcFeatureJoins (DWORD dwDemiPhone, PCMTTSTriPhoneAudio pTPA, DWORD dwSkip,
                                       DWORD *pdwLeft, DWORD *pdwRight,
                                       DWORD *pdwLeftIdeal, DWORD *pdwRightIdeal);
   BOOL SynthAdjustVolumeRelative (PCM3DWave pWave, DWORD *padwDur, fp *pafVol,
                                       DWORD dwNum);
   BOOL SynthAdjustEnergyPerVolume (PCM3DWave pWave, DWORD *padwPhone, DWORD *padwDur,
                                       DWORD dwNum);
   PCSentenceSyllable SynthProsodyCreateSyllables (PCMMLNode2 pNode, DWORD dwNodeStart, PCTextParse pParse,
                                                       PTTSVOICEMOD pVoiceMod, int iTTSQuality, BOOL fTransPros,
                                                       DWORD dwMultiPass, PCListFixed plNodeAssociate, DWORD *pdwNodeEnd);
   PCSentenceSyllable SynthProsodyCreateSyllablesInt (PCMMLNode2 pNode, DWORD dwNodeStart, DWORD dwNodeEnd, PCTextParse pParse,
                                                       PTTSVOICEMOD pVoiceMod, int iTTSQuality, BOOL fTransPros, 
                                                       DWORD dwMultiPass, PCListFixed plNodeAssociate);
   void FillInCOMPARESYLINFO (PCOMPARESYLINFO pInfo);
   BOOL SynthProsodyTagsApply (PCMMLNode2 pNode, DWORD dwNodeStart, PCTextParse pParse, PTTSVOICEMOD pVoiceMod, int iTTSQuality,
                                   BOOL fTransPros, DWORD dwMultiPass, DWORD *pdwNodeEnd);
   BOOL SynthTextInterpretTags (PCMMLNode2 pNode, PTTSGLOBALSTATE pState, PTTSVOICEMOD pVoiceMod,
      BOOL *pfTransPros);
   void ProsodyToGlobalState (PCMMLNode2 pNode, PTTSGLOBALSTATE pState, PTTSVOICEMOD pVoiceMod);
   void EmotionToGlobalState (PCMMLNode2 pNode, PTTSGLOBALSTATE pState, PTTSVOICEMOD pVoiceMod);
   BOOL LogSpoken (PCM3DWave pWave);
   void GenerateUNITOPTION (DWORD dwTime, PCMem pmemFrom, DWORD dwPhoneValid, BYTE *pbPhone,
                       BYTE *pbWordSylPos, BYTE *pbTPhoneGroup, BYTE *pbTPhoneNoStress,
                       DWORD *pdwWord,
                       int* paiPitchF0Extra, int *paiPitchF0Fuji,
                       /*DWORD dwDurationPrev, */DWORD dwDuration, fp fEnergy, fp fEnergyPrev,
                       fp fAccent, BYTE bSilence,
                       DWORD dwFuncWordGroup,
                       fp fEnergyAverage,
                       PCListFixed plUNITOPTION, PCListFixed plUNITOPTIONLast,
                       PCMem pMemInter, int iTTSQuality, BOOL fDisablePCM);
   void UnitCacheSize (int iTTSQuality, DWORD *pdwFirstPass, DWORD *pdwSecondPass);

   void FreeUnits (void);
   BOOL MakeSureEnoughPhone (DWORD dwNum = (DWORD)-1);
   // PTTSNGRAM NGramFind (PBYTE pabPOS);
   // void NGramFindBackoff (PBYTE pabPOS, PCPoint pPros, BOOL *pfPauseLeft);
   // void NGramFindBackoff (PCTextParse pParse, PCMMLNode2 pNode,
   //    DWORD dwIndex, PCPoint pPros, BOOL *pfPauseLeft, BYTE *pabPOS);
   BOOL FindSurroundingPOS (PCTextParse pParse, PCMMLNode2 pNode, DWORD dwNodeStart, DWORD dwNodeEnd,
                              DWORD dwIndex, BYTE *pabPOS);

   __inline fp ScoreCalcSelfA (DWORD dwTime, PCMTTSTriPhoneAudio ptNew, PCMTTSTriPhoneAudio ptPrev, BOOL fUsePitch,
                               int *paiPitchF0Extra, int *paiPitchF0Fuji,
                               DWORD dwDuration, fp fEnergy, fp fAccent,
                               DWORD dwFuncWordGroup,
                               DWORD *pdwWord, PBYTE pbWordSylPos,
                               PBYTE pbPhone, PBYTE pbTPhoneNoStress, PBYTE pbTPhoneGroup, BYTE bSilence,
                               DWORD *pdwPrevious, PVOID pahFrom,
                               BOOL fDisablePCM);
   __inline fp ScoreCalcSelfB (DWORD dwTime, PCMTTSTriPhoneAudio ptNew, PCMTTSTriPhoneAudio ptPrev, BOOL fUsePitch,
                               int *paiPitchF0Extra, int *paiPitchF0Fuji,
                               DWORD dwDuration, fp fEnergy, fp fAccent,
                               DWORD dwFuncWordGroup,
                               DWORD *pdwWord, PBYTE pbWordSylPos,
                               PBYTE pbPhone, PBYTE pbTPhoneNoStress, PBYTE pbTPhoneGroup, BYTE bSilence,
                               DWORD *pdwPrevious, PVOID pahFrom,
                               BOOL fDisablePCM);
   __inline fp ScoreCalcBoundary (DWORD dwTime, PCMTTSTriPhoneAudio ptNew, DWORD *pdwWord, PBYTE pbWordPos,
                                         PBYTE pbPhone, PBYTE pbTPhoneNoStress, PBYTE pbTPhoneGroup, BYTE bSilence,
                                         DWORD *pdwPrevious,
                                         PVOID pahFrom, PCMTTSTriPhoneAudio *paPCMTTSTriPhoneAudio);
   // __inline fp ScoreCalcContiguous (PCMTTSTriPhoneAudio ptNew, PBYTE pbPhone, DWORD dwPhoneValid);

   fp JoinCompare (PSRANALBLOCK pAComp, PSRANALBLOCK pBComp,
                       DWORD dwCompressA, DWORD dwCompressB, DWORD dwNumAComp, DWORD dwNumBComp,
                       DWORD dwCenterAUncomp, DWORD dwCenterBUncomp, DWORD dwHalfWindow,
                       int iJoinCompareQuality);
   fp JoinFindBest (CRITICAL_SECTION *pCSA, CRITICAL_SECTION *pCSB, DWORD dwSentenceNumA, DWORD dwSentenceNumB,
                        DWORD dwFeatureCenterA, DWORD dwFeatureCenterB,
                        DWORD dwRangeA, DWORD dwRangeB, DWORD dwIdealA, DWORD dwIdealB, DWORD dwHalfWindow,
                        int iBestCompareQuality, int iJoinCompareQuality,
                        DWORD *pdwBestA, DWORD *pdwBestB);


   PCMLexicon        m_apLexFuncWord[NUMFUNCWORDGROUP];  // function words
   DWORD             m_dwTriPhoneGroup;   // 0 for 16 groups, 1 for ignore stress, 2 for max
                                          // 0..NUMTRIPHONEGROUP-1
   BOOL              m_fKeepLog;          // if trye will keep log of what's spoken
   BOOL              m_fFullPCM;          // keep full PCM, not just high frequencies
   fp                m_fAvgPitch;         // average pitch
   fp                m_fAvgPitchF0Fuji;   // average fuji pitch
   fp                m_fAvgSyllableDur;   // average duration of a syllable, in seconds
   fp                m_fFujiRangeLow;     // low-end of range, in octaves
   fp                m_fFujiRangeMed;     // typical range, in octaves
   fp                m_fFujiRangeHigh;    // high range of fuji, in octaves
   DWORD             m_dwWordsPerMinute;  // average words per minute
   WCHAR             m_szLexicon[256];    // lexicon filename
   PCListFixed       *m_palPCMTTSTriPhoneAudio; // array for each phoneme (by unsort number), of a list
                                          // each list contains list of PCMTTSTriPhoneAudio
                                          // uses m_memTriPhoneAudio.
   PCListFixed       *m_palPCMTTSTriPhonePros;  // like Audio above, but for prosody info
//   PCMem             *m_pamemTriPhoneAudio;  // for m_memTriPhoneAudioMem
   PCMTTSTriPhoneAudio *m_papObjectCachePCMTTSTriPhoneAudio;   // cache in m_memObjectCachePCMTTSTriPhoneAudio
   PCMTTSTriPhonePros *m_papObjectCachePCMTTSTriPhonePros;   // cache in m_memObjectCachePCMTTSTriPhonePros
   DWORD             m_dwNumPhone;        // number of phonemes allocated for m_memTriPhone and m_palPCMTTSTriPhoneAudio
   CMem              m_memTriPhoneAudio;  // memory where store triphone lists
   // CMem              m_memTriPhoneAudioMem;  // memory with list of pointers to other memory so will load/free faster
   CMem              m_memTriPhonePros;   // memory where store triphone lists
   CMem              m_memObjectCachePCMTTSTriPhonePros; // list of m_dwPhonemes of cached objects allocated
   CMem              m_memObjectCachePCMTTSTriPhoneAudio; // list of m_dwPhonemes of cached objects allocated
   CListFixed        m_lPCTTSWave;        // list of CTTSWaves, indexed by wave number
   CListFixed        m_lPCPhaseModel;     // list of phase model objects, one for each phoneme

   // can look at but dont delete
   PCMLexicon        m_pLexTrainingWords; // lexicon of the words that trained TTS with... used for prosody
   PCMLexicon        m_pLexWords;         // lexicon of the most common words to keep around

   // prosody info
   // PCMLexicon        m_apLexWordEmph[NUMLEXWORDEMPH]; // array of lists to keep track
                                       // of the emphasis of the common words.. so know very
                                       // common words vs. less common
   // CPoint            m_apLexWordEmphScale[NUMLEXWORDEMPH+1];  // amount to scale emphasis of
                                       // words that appear in m_apLexWordEmph. [0]=pitch, [1]=vol, [2]=dur
   // CPoint            m_apProsWordEmphFromWordLength[NUMPROSWORDLENGTH]; // amount to scale based on # of phonemes
                                       // in word, [0]=pitch, [1]=vol, [2]=dur
   CPoint            m_apPhoneEmph[NUMPHONEEMPH*2][PHONEPOSBIN]; // how much the phonemes are emphasized based on neighboring POS
                                             // first entries are left part of word, last entries are right part
                                             // the [16] is for the high nibble of the POS
   // PCMLexicon        m_pLexFuncWords;  // list of function words (NUMFUNCWORDS entries) that
                                       // are the most common words and which should affect prosody
   // CListFixed        m_lPCTTSPunctPros;   // list of punctuation prosody information, including for func words
   //CMem              m_memMicroPause;  // Contains an array of NxN bits, where N = m_pLexicon->PhonemeNum().
                                       // bit is set to TRUE if micropause occurs between the two phonemes
                                       // m_dwCurPosn set to # of bytes needed
   // CMem              m_memNGram;       // array of pow(POS_MAJOR_NUM+1, (TTSPROSNGRAMBIT+TTSPROSNGRAM)*2+1) TTSNGRAM
   // CListFixed        m_lPCSentenceSyllable;  // list of sentence syllables used to generate prosody.
   PCTTSProsody      m_pCTTSProsody;   // main prosody for the tts
   CMem              m_memEnergyPerPitch; // memory for the energy-per-pitch info
   CMem              m_memEnergyPerVolume;   // memory fo the energy-per-volume info

   // created at runtime
   PCMLexicon        m_pLexMain;       // from the lexicon cache
   CListVariable     m_lWordRankHistory;  // history of recent non-function words spoken

   // derived tts
   BOOL              m_fIsDerived;     // set to TRUE if TTS is derived
   WCHAR             m_szTTSMaster[256];  // master tts voice
   CMTTS             *m_pTTSMaster;     // pointer to master TTS. this is opened with the cache

   CRITICAL_SECTION  m_csSynthBeamSearch;       // for multithreaded beam search
};
typedef CMTTS *PCMTTS;


DLLEXPORT BOOL TTSWaveToTransPros (PCM3DWave pWave, PCMLexicon pLex, DWORD dwPitchSamples,
                         PCListFixed plPhone, PCListFixed plWord, PCListFixed plDur,
                         PCListVariable plPitchF0Extra, PCListVariable plPitchF0Fuji, PCListFixed plVol,
                         PCListFixed plAccent,
                         PCListFixed plPCMTTSTriPhoneAudio, PCMTTS pTTS,
                         BOOL fIncludesFuji);
DLLEXPORT BOOL TTSFileOpenDialog (HWND hWnd, PWSTR pszFile, DWORD dwChars, BOOL fSave);
DLLEXPORT BOOL TTSProsodyFileOpenDialog (HWND hWnd, PWSTR pszFile, DWORD dwChars, BOOL fSave, BOOL fIncludeTTS);
DLLEXPORT PCMTTS TTSCacheOpen (PWSTR pszFile, BOOL fLoadIfNotExist /* was =TRUE */, BOOL fThreadSafe);
DLLEXPORT BOOL TTSCacheClose (PCMTTS pTTS);
DLLEXPORT void TTSCacheMemoryTouch (void);
DLLEXPORT void TTSCacheMemoryFree (fp fPercentJoin, fp fPercentWaves);
DLLEXPORT void TTSCacheShutDown (void);
DLLEXPORT void TTSCacheDefaultGet (PWSTR pszFile, DWORD dwChars);
DLLEXPORT void TTSCacheDefaultGet (PCEscPage pPage, PWSTR pszControl);
DLLEXPORT void TTSCacheDefaultSet (PWSTR pszFile);
DLLEXPORT BOOL TTSCacheDefaultUI (HWND hWnd);
DLLEXPORT BOOL TTSCacheSpeak (PWSTR pszSpeak, BOOL fTagged, BOOL fDisablePCM, HWND hWndProgress);
DLLEXPORT void TTSCacheErrorDisplay (HWND hWnd);
DLLEXPORT BOOL TTSCacheSpellCheck (PWSTR pszSpeak, HWND hWndProgress);


/*************************************************************************************
RecBatch */
DLLEXPORT DWORD RecBatch (DWORD dwCalcFor, PCEscWindow pWindow, PCWSTR pszVoiceFile, BOOL fRequireVoiceFile,
                BOOL fUIToReview, BOOL fSimple, PCWaveToDo pWaveToDo, PCWaveDirInfo pWaveDirInfo,
                PCWSTR pszMasterFile, PCWSTR pszStartName, HWND hWnd);
DLLEXPORT DWORD RecBatch2 (DWORD dwCalcFor, HWND hWnd, PCWSTR pszVoiceFile, BOOL fRequireVoiceFile,
                BOOL fSimple, PCListVariable plWaveToDo, PWSTR pszDir,
                PCWSTR pszStartName);

/*************************************************************************************
CTTSWork */
DLLEXPORT BOOL ResolvePathIfNotExist (PWSTR pszLexicon, PWSTR pszSrc);
DLLEXPORT PWSTR FindFileNameMinusPath (PWSTR pszFile);
DLLEXPORT DWORD TTSWorkEnoughMemory (DWORD dwSentences);
DLLEXPORT BOOL WaveEditorRun (PCWSTR pszDir, PCWSTR pszFile, HWND hWnd);
DLLEXPORT void TTSWorkLangEnum (PCListFixed plLANGID, PCListVariable plLangName);
DLLEXPORT BOOL TTSWorkDefaultFileName (LANGID lid, PWSTR pszLangName, DWORD dwFile, PWSTR pszFile);

#define TWDF_LEXICON       0     // lexicon file, for TTSWorkDefaultFileName()
#define TWDF_SRMALE        1     // sr male training
#define TWDF_SRFEMALE      2     // sr female training
#define TWDF_PROMPTS       3     // prompt sentences
#define TWDF_COMMONWORDS   4     // text file for common words


#define ENERGYPERPITCHBASE       75.0       // start pitch track at 75 hz
#define ENERGYPERPITCHOCTAVES    3        // keep track of pitch over three octaves
#define ENERGYPERPITCHPOINTSPEROCTAVE  6  // sample pitch at every other point
#define ENERGYPERPITCHNUM (ENERGYPERPITCHOCTAVES * ENERGYPERPITCHPOINTSPEROCTAVE)   // number of energy-per-pitch values to kepp

#define ENERGYPERVOLUMEPOINTSPEROCTAVE 6     // 6 data points per doubling of energy, 1 per dB
#define ENERGYPERVOLUMECENTER    (3 * ENERGYPERVOLUMEPOINTSPEROCTAVE)   // 1/8th the average energy below
#define ENERGYPERVOLUMENUM       (ENERGYPERVOLUMECENTER + 3 * ENERGYPERVOLUMEPOINTSPEROCTAVE)   // to 8x the average energy above

// TTSWORKNGRAM - Use this to store counts and scores for a particular ngram
typedef struct {
   CPoint      pProsody;      // .p[0] = pitch scale, .p[1] = volume scale,
                              // and .p[2] = duration scale. p[3] = total count
   DWORD       dwPauseLeft;   // count for number of occurences that pause to left
   DWORD       dwPauseTotal;  // total count for those that do and dont pause
} TTSWORKNGRAM, *PTTSWORKNGRAM;


// TTSANAL - Analysis information used for tts
typedef struct {
   double         afPhonemePitchSum[256];    // pitch sum per phoneme, combined extra and fuji
   double         afPhonemeDurationSum[256]; // duration sum per phoneme
   double         afPhonemeEnergySum[256];   // energy sum per phoneme
   DWORD          adwPhonemeCount[256];      // count for afPhonemePitchSum, afPhonemeDuationSum, afPhonemeEnergySum

   PCMTTS         apTTS[NUMPROSODYTTS];      // tto to get prosody from. might be NULL
   PCTTSProsody   apTTSProsody[NUMPROSODYTTS];// prosody model to get prosody from
   PCListFixed    paplTriPhone[WORDPOS_MAX][PHONEGROUPSQUARE];      // triphone information
   PCListFixed    *paplWords;                // pointer to an array fo m_pWordLex->WordNum() words
   //PCListFixed    plWave;                    // list of WAVEANINFO from the waves
   PCListFixed    plPCWaveAn;                // list of PCWaveAn from the waves
   PCListFixed    *paplWEMPH;                // pointer to a list of plWave->Num() PCListFixed. Each list
                                             // contains an array of WEMPH structures for word emphasis info for each wave
   PCListFixed    plTRIPHONETRAIN;           // triphone info, referred to in PHONEAN
   PVOID          paGroupTRIPHONETRAIN;           // TRIPHONETRAIN structures
   PVOID          paSpecificTRIPHONETRAIN;           // TRIPHONETRAIN structures
   PVOID          paPHONETRAIN;              // phoneme training, but TRIPHONETRAIN structures
   PVOID          paMegaPHONETRAIN;       // TRIPHONE training, but independent of word position, and using mega phone groups
   double         *pafEnergyPerPitch;        // array of (SRDATAPOINTS + 1 (count)) doubles x ENERGYPERPITCHNUM, for keeping track of how
                                             // pitch changes energy
   char           *pacEnergyPerPitch;        // filled in with SRDATAPOINTS * ENERGYPERPITCHNUM characters indicating how many
                                             // dB above normal the pitch is
   double         *pafEnergyPerVolume;       // array of (SRDATAPOINTS + 1 (count)) doubles x ENERGYPERVOLUMENUM, for keeping track of how
                                             // volume changes energy
   char           *pacEnergyPerVolume;       // filled in with SRDATAPOINTS * ENERGYPERVOLUMENUM characters indicating how many
                                             // dB above normal the volume is
   PCVoiceFile    pOrigSR;                   // original SR for phoneme
   DWORD          dwPhonemes;                // number of phonemes to use
   fp             fAvgPitchF0All;                 // average pitch, both extra and fuji
   fp             fAvgPitchF0Fuji;           // average pitch, just fuji
   fp             fAvgSyllableDur;           // average duration of a syllable, in seconds
   fp             fFujiRangeLow;             // fuji range over a sentence, low, medium, high, in octaves
   fp             fFujiRangeMed;             // fuji range over a sentence, low, medium, high, in octaves
   fp             fFujiRangeHigh;            // fuji range over a sentence, low, medium, high, in octaves
   // CPoint         apLexWordEmphScale[NUMLEXWORDEMPH+1];  // amount to scale emphasis of
                                       // words that appear in m_apLexWordEmph. [0]=pitch, [1]=vol, [2]=dur
   // CPoint         apWordEmphFromWordLength[NUMPROSWORDLENGTH+1]; // amount to scale emphasis depending on word length
                                             // [0]=pitch, [1]=vol, [2]=dur
   WRDEMPH         apPhoneEmph[NUMPHONEEMPH*2][PHONEPOSBIN]; // how much the phonemes are emphasized based on neighboring POS
                                             // first entries are left part of word, last entries are right part
                                             // the [16] is for the high nibble of the POS
   //DWORD          *padwMicroPause;           // keeps tracks of the probability of short silence ocurring between 2 phones
                                             // table of NxNx2 DWORDs. N=# of phonemes in pLexicon->PhonemesNum(),
                                             // 2 = total[0], followed by # that have pause[1]
   // PTTSWORKNGRAM      paTTSNGram;                // pointer to an array of pow(POS_MAJOR_NUM+1, (TTSPROSNGRAMBIT+TTSPROSNGRAM)*2+1) elements
} TTSANAL, *PTTSANAL;

// JCPHONETRAIN - Information for trainig JoinCosts()
typedef struct {
   double            fValueSum;     // depends on the exact meaning of JCPHONETRAIN, but might be sum of log of duration, or log energy
   DWORD             dwCount;       // number of phonemes trained
   PCPhoneme         pPhoneme;   // phoneme that trained with
} JCPHONETRAIN, *PJCPHONETRAIN;

// JCLINEARFITPOINT - For storing linear fit info
typedef struct {
   fp                fX;            // X = octave, volume, etc.
   fp                fY;            // Y = SR score
} JCLINEARFITPOINT, *PJCLINEARFITPOINT;

// JCLINEARFIT - Information for storing linear fit
typedef struct {
   PCListFixed       plJCLINEARFITPOINT;  // list of points for standard deviation
} JCLINEARFIT, *PJCLINEARFIT;

// JCWORDPOS - Word position information
typedef struct {
   double            afScore[WORDPOS_MAX];          // score for the 4 different word positions. 0 = exact match,
                                          // 1 = mismatch in left, 2 = mismatch in right, 3 = mismatch in both
   DWORD             adwCount[WORDPOS_MAX];         // count for number of entries in each
} JCWORDPOS, *PJCWORDPOS;

// JCPITCHPCM - Pitch PCM
typedef struct {
   double            afScore[PIS_PHONEGROUPNUM];       // score sum, [voiced][plosive]
   DWORD             adwCount[PIS_PHONEGROUPNUM];      // count
} JCPITCHPCM, *PJCPITCHPCM;


// JCPITCHPCMPHONE - Pitch PCM per phoneme
typedef struct {
   double            afScore[NUMLEXENGLISHPHONE][MAXSTRESSES];       // score sum
   DWORD             adwCount[NUMLEXENGLISHPHONE][MAXSTRESSES];      // count
} JCPITCHPCMPHONE, *PJCPITCHPCMPHONE;

// JCCONNECT - Non-contiguous connection information
typedef struct {
   double            fScore;              // sum of SR scores
   DWORD             dwCount;             // count
} JCCONNECT, *PJCCONNECT;

// JCCONTEXT - Context information
typedef struct {
   double            afScoreCon[2][6];       // score sum, [0 = left, 1=right][0=exact, 1=same triphone, 2=stressed phoneme,3=unstressed phoneme, 4=group, 5=megagroup, 4=not in megagroup]
   DWORD             adwCountCon[2][6];      // count. as per afScore
} JCCONTEXT, *PJCCONTEXT;

// TTSJOINCOSTS - For passing join costs info around
typedef struct {
   FILE           *pfJoinCosts;              // text file to write output
   DWORD          dwPhonemes;                // phonemes in lexicon
   PCListFixed    plJCPHONEMEINFO;           // phoneme info for join costs, one per phoneme
   PTTSTARGETCOSTS   pTarget;                // where to write the target costs to
   CRITICAL_SECTION cs;                  // critical section used to modify params when in multithreaded

   DWORD          dwContextIndexMegaStart;   // where mega-phone starts in the context index
   DWORD          dwContextIndexGroupStart;  // where group-phone starts in the context index
   //DWORD          dwContextIndexUnstressedStart;   // where unstressed version starts on context index
   DWORD          dwContextIndexStressedStart;  // where stressed version starts in context index
   DWORD          dwContextIndexNum;         // number in the context index

   JCPITCHPCM     JCPITCHPCMHigher;          // for higher pitch
   JCPITCHPCM     JCPITCHPCMLower;           // for lower pitch
   JCPITCHPCM     JCPITCHPCMPSOLAHigher;     // for higher pitch, PSOLA
   JCPITCHPCM     JCPITCHPCMPSOLALower;      // for lower pitch, PSOLA
   JCPITCHPCMPHONE     JCPITCHPCMHigherPhone;          // for higher pitch, per phoneme
   JCPITCHPCMPHONE     JCPITCHPCMLowerPhone;           // for lower pitch, per phoneme
   JCPITCHPCMPHONE     JCPITCHPCMPSOLAHigherPhone;     // for higher pitch, PSOLA, per phoneme
   JCPITCHPCMPHONE     JCPITCHPCMPSOLALowerPhone;      // for lower pitch, PSOLA, per phoneme

   PJCPHONETRAIN  paJCPHONETRAINPitch;       // train for much much error for pitch. pJC->dwPhonemes * PHONEGROUPSQUARE elem
   PJCLINEARFIT   paJCLINEARFITPitch;        // linear fit info for pitch. pJC->dwPhonemes * PHONEGROUPSQUARE elem
   PJCPHONETRAIN  paJCPHONETRAINDuration;    // train for much much error for duration. pJC->dwPhonemes * PHONEGROUPSQUARE elem
   PJCLINEARFIT   paJCLINEARFITDuration;     // linear fit info for duration. pJC->dwPhonemes * PHONEGROUPSQUARE elem
   PJCPHONETRAIN  paJCPHONETRAINEnergy;      // train for much much error for energy. pJC->dwPhonemes * PHONEGROUPSQUARE elem
   PJCLINEARFIT   paJCLINEARFITEnergy;       // linear fit info for energy. pJC->dwPhonemes * PHONEGROUPSQUARE elem
   PJCPHONETRAIN  paJCPHONETRAINAccent;      // train for much much error for accent. pJC->dwPhonemes * PHONEGROUPSQUARE elem
   PJCLINEARFIT   paJCLINEARFITAccent;       // linear fit info for accent. pJC->dwPhonemes * PHONEGROUPSQUARE elem
   PJCLINEARFIT   paJCLINEARFITPSOLAPitch;   // linear fit for how PSOLA affects pitch. NUMLEXENGLISHPHONE * MAXSTRESSES
   PJCLINEARFIT   paJCLINEARFITPSOLADuration;   // linear fit for how PSOLA affect duration. NUMLEXENGLISHPHONE * MAXSTRESSES
   PJCPHONETRAIN  paJCPHONETRAINWordPos;     // training for position of phoneme in word. 4 x pJC->dwPhonemes x PHONEGROUPSQUARE
   PJCWORDPOS     paJCWORDPOSWordPos;        // word position information. pJC->dwPhonemes * PHONEMEGROUPSQUARE elem
   PJCPHONETRAIN  paJCPHONETRAINConnect;     // training for connection edges of one phoneme to another. pJC->dwPhonemes * pJC->dwPhonemes
   PJCPHONETRAIN  paJCPHONETRAINConnectMid;  // training for connection mid of a phoneme. pJC->dwPhonemes
   PJCCONNECT     paJCCONNECTConnect;        // scores for connection to right context. pJC->dwPhonemes x pJC->dwPhonemes
   PJCCONNECT     paJCCONNECTConnectMid;     // scores for connection in center of phoneme. pJC->dwPhonemes
   PJCPHONETRAIN  paJCPHONETRAINContext;     // training for context. pJC->dwPhonemes * pJC->dwContextIndexNum * pJC->dwContextIndexNum
   PJCCONTEXT     paJCCONTEXTContext;        // recognition results for context. pJC->dwPhonemes * PHONEMEGROUPSQUARE elem
   PJCPHONETRAIN  paJCPHONETRAINFunc;        // train for much much error for function words. pJC->dwPhonemes * PHONEGROUPSQUARE elem
   PJCLINEARFIT   paJCLINEARFITFunc;         // linear fit info for function words. pJC->dwPhonemes * PHONEGROUPSQUARE elem


} TTSJOINCOSTS, *PTTSJOINCOSTS;


// SENTSYLEMPH - Floating point emphasis information for a sentence syllable
typedef struct {
   fp             afPitchF0Extra[SSPITCHPOINTS];        // relative pitch above fuji, linear
   fp             fPitchF0Fuji;  // relative pitch, fuji, linear
   fp             fPitchF0FujiDelta;   // how much pitch changes, as per cPitchF0FujiDelta but / 100
   // fp             fPitchSweep;   // pitch change over the syllable, in octaves, positive or negative
   // fp             fPitchBulge;   // bulge in pitch in the center of syllable. In octaves, positive or negative
   fp             fVolume;       // relative volume, linear
   fp             fAccent;       // accent, from -1 to 1
   fp             fDurPhone;     // duration relative to what expected for given phonemes, linear
   fp             fDurSyl;       // duration given average syllable, linear
   fp             fDurSkew;      // as per cDurSkew, but not * 100.
} SENTSYLEMPH, *PSENTSYLEMPH;

// SYLEMPH - syllable emphasis information... part of WEMPH
typedef struct {
   SENTSYLEMPH    Emph;          // emphasis info
   fp             fMultiMisc;    // syllable number (index into word) + 256 * stress value if stressed
   DWORD          dwNumPhones;   // number of phonemes. Used for analysis of wave to generate tts
} SYLEMPH, *PSYLEMPH;


// TYPICALSYLINFO - For storing typical syllable info
// NOTE: ALL of these MUST be double or some code won't work
// NOTE: MUST MATCH PROSODYNGRAMINFO exactly except double vs. float
typedef struct {
   double               fCount;     // count for all the values
   double               afPitchF0ExtraSum[SSPITCHPOINTS]; // pitch stored as log(x) x count
   double               fPitchF0FujiSum; // pitch stores as log(x) x count
   double               fPitchF0FujiDeltaSum;   // delta between this and last pitch, stored as log(x) x count
   double               fVolumeSum; // bulge stored as log x count
   double               fAccentSum; // sum of accents, individual accents from -1 to 1
   double               fDurPhoneSum; // bulge stored as log x count
   double               fDurSylSum; // bulge stored as log x count
   double               fDurSkewSum;   // sum stores as log x count
} TYPICALSYLINFO, *PTYPICALSYLINFO;

#define PROSODYNGRAMNUM    1           // number of points in NGRAM, to either side of center.
                                       // thus, if this is 2 then really 5 dimensions in N-gram

// PROSODYNGRAMINFO - Ngram information for prosody
// NOTE: ALL of these MUST be floats or some code won't work
// NOTE: MUST MATCH TYPICALSYLINFO exactly except double vs. float
typedef struct {
   float               fCount;     // count for all the values
   float               fPitchSum;  // pitch stored as log() x count
   float               fPitchSweepSum; // sweep stored as log x count
   float               fPitchBulgeSum; // bulge stored as log x count
   float               fVolumeSum; // bulge stored as log x count
   float               fDurPhoneSum; // bulge stored as log x count
   float               fDurSylSum; // bulge stored as log x count
   float               fDurSkewSum;   // sum stores as log x count
} PROSODYNGRAMINFO, *PPROSODYNGRAMINFO;

class CWaveAn;
typedef CWaveAn *PCWaveAn;


class DLLEXPORT CTTSWork : public CEscMultiThreaded {
public:
   ESCNEWDELETE;

   CTTSWork (void);
   ~CTTSWork (void);

   PCMMLNode2 MMLTo (void);
   BOOL MMLFrom (PCMMLNode2 pNode, PWSTR pszSrcFile);
   CTTSWork *Clone (void);
   BOOL Save (PWSTR pszFile);
   BOOL Open (PWSTR pszFile);

   BOOL LexiconSet (PWSTR pszFile);
   PWSTR LexiconGet (void);
   PCMLexicon Lexicon (void);
   BOOL LexiconRequired (void);

   BOOL ScanCommonWords (HWND hWnd, PWSTR pszFile, BOOL fSimple);
   DWORD Recommend (HWND hWnd);
   BOOL Analyze (HWND hWnd, PCEscWindow pWindow, BOOL fSimple, DWORD dwFileSize);
   BOOL JoinCosts (HWND hWnd, PCEscWindow pWindow);
   BOOL Import2008 (PCEscPage pPage);
   BOOL Import2009Mandarin (PCEscPage pPage);
   BOOL SaveRecTranscript (PCEscPage pPage);
   BOOL CreateCustomSR (HWND hWnd, PWSTR pszDefaultSR, BOOL fSimple);

   BOOL ToDoReadIn (char *pszText, HWND hWnd, PCListVariable plToAdd,
                           PCListVariable plAlreadyExistInToDo, PCListVariable plAlreadyExistInWave,
                           PCListVariable plHaveMisspelled, PCListVariable plHaveUnknown,
                           PCMLexicon plLexUnknown);


   void BlacklistAdd (WORD wWave, BOOL fIsPhone, WORD wUnit);
   BOOL BlacklistExist (WORD wWave, BOOL fIsPhone, WORD wUnit);
   BOOL BlacklistRemove (WORD wWave, BOOL fIsPhone, WORD wUnit);
   void BlacklistClearTriPhone (PTTSANAL pAnal, WORD wPhone, WORD wWordSylPos, WORD wTriPhone);
   void BlacklistClearWord (PTTSANAL pAnal, DWORD dwWord);
   DWORD BlacklistNumTriPhone (PTTSANAL pAnal, WORD wPhone, WORD wWordSylPos,
                                      BYTE bPhoneLeft, BYTE bPhoneRight,
                                      DWORD *pdwCount);
   DWORD BlacklistNumWord (PTTSANAL pAnal, DWORD dwWord, DWORD *pdwCount);

#if 0 // not used anymore
   BOOL AnalysisWord (PTTSANAL pAnal, PCMTTS PTTS, DWORD dwWord, BOOL fRemoveExisting,
      BOOL fNow);
#endif
   BOOL AnalysisTriPhone (PTTSANAL pAnal, PCMTTS pTTS, DWORD dwPhone, WORD wWordSylPos,
                                 WORD wTriPhone, BOOL fRemoveExisting, BOOL fNow);

   BOOL DialogMain (PCEscWindow pWindow, BOOL fSimple);
   BOOL ResegmentWaves (PCEscPage pPage, HWND hWnd, DWORD dwMode, BOOL fKeepExisting, BOOL fJustFeatures /* = FALSE*/,
      BOOL fSimple, PCVoiceFile pVF, DWORD *padwTopNCount);
   BOOL RedoVowels (PCEscPage pPage);
   BOOL SRAccuracy (PCEscPage pPage);
   fp FuncWordWeight (PWSTR psz, DWORD *pdwGroup);
   BOOL BlizzardSmallVoice (HWND hWnd);

   // from EscMultiThreaded
   virtual void EscMultiThreadedCallback (PVOID pParams, DWORD dwParamSize, DWORD dwThread);

   // variables
   WCHAR             m_szFile[256];       // filename
   WCHAR             m_szSRFile[256];     // speech recognition to use
   WCHAR             m_aszProsodyTTS[NUMPROSODYTTS][256]; // TTS file to use for prosody
   DWORD             m_dwMinInstance;     // minimum instances, 1..10 of each n-gram
   DWORD             m_dwWordCache;       // keep this many words around
   DWORD             m_dwMinExamples;     // minimu number of examples of a phoneme or word
   DWORD             m_dwTriPhoneGroup;   // 0 for 16 groups, 1 for ignore stress, 2 for max
   //DWORD             m_dwTriPhonePitch;   // 0 for no extra copies of pitch, .. 3 for maximum
   BOOL              m_fKeepLog;          // if trye will keep log of what's spoken
   BOOL              m_fPauseLessOften;   // if TRUE, micropauses in TTS voice will be less often than training
   BOOL              m_fFullPCM;          // keep full PCM, not just high frequencies
   DWORD             m_dwTimeCompress;    // how much to time compress, from 0 to 3.
   DWORD             m_dwFreqCompress;     // 0 for none (include PCM), 1 for no PCM, 2 for max
   DWORD             m_dwPCMCompress;     // how much PCM compression to use. 0 for none, 1 for ADPCM, 2 for FUll PCM
   BOOL              m_fWordStartEndCombine; // if TRUE combine phonemes for word start/end with phonemes in center
   double            m_fWordEnergyAvg;    // average word energy
   DWORD             m_dwWordCount;       // number of words to calculate m_fWordEnergyAvg;
   DWORD             m_dwTotalUnits;      // total number units in the voice, from 3K - 30K
   CMem              m_memTTSTARGETCOSTS; // memory that contains target costs. Only valid if m_dwCurPosn == sizeof(TTSTARGETCOSTS)
   //DWORD             m_dwMultiUnit;       // number of multiple units
   // DWORD             m_dwMultiSyllableUnit;       // number of syllable units
   //DWORD             m_dwConnectUnits;       // units to connect

   DWORD             m_dwUnitsAdded;      // number of units added
   DWORD             m_dwLastSRRetrain;   // last time retrained SR, in number of sentences

   // for SAPI
   LANGID            m_LangID;            // language ID
   CMem              m_memSAPIToken;      // token name (to identify)
   CMem              m_memSAPIName;       // voice name
   CMem              m_memSAPIVendor;     // vendor name
   DWORD             m_dwAge;             // 0 for child, 1 for teenager, 2 for adult, 3 for senior
   DWORD             m_dwGender;          // 0 for male, 1 for female
   DWORD             m_dwVersionMajor;     // major version number
   DWORD             m_dwVersionMinor;    // minor version number
   DWORD             m_dwVersionBuild;    // build number

   // can look at but dont delete
   PCWaveDirInfo     m_pWaveDir;       // list of wave files
   PCWaveDirInfo     m_pWaveDirEx;     // list of wave files excluded from the prosody model
   PCWaveToDo        m_pWaveToDo;      // to do list
   PCMLexicon        m_pLexWords;      // lexicon of the most common words to keep around
   PCMLexicon        m_apLexFuncWord[NUMFUNCWORDGROUP];  // function words
   // PCMLexicon        m_apLexWordEmph[NUMLEXWORDEMPH]; // array of lists to keep track
                                       // of the emphasis of the common words.. so know very
                                       // common words vs. less common
   // PCMLexicon        m_pLexFuncWords;  // list of function words (NUMFUNCWORDS entries) that
                                       // are the most common words and which should affect prosody

   PCMLexicon        m_pLexMisspelled; // list of misspelled words

private:
   fp AnalysisUnitPenalty (PTTSANAL pAnal, PCMTTS pTTS, PVOID ppa);
   BOOL AnalysisSingleUnits (PTTSANAL pAnal, PCMTTS pTTS, PCListFixed plUNITGROUPCOUNT);
   BOOL AnalysisMultiUnits (PTTSANAL pAnal, PCMTTS pTTS, PCListFixed plUNITGROUPCOUNT,
                                   BYTE bStartPhone);
   BOOL AnalysisSelectFromUNITGROUPCOUNT (PTTSANAL pAnal, PCMTTS pTTS, PCListFixed plUNITGROUPCOUNT);
   BOOL AnalysisWriteTriPhones (PTTSANAL pAnal, PCMTTS pTTS, PCProgressSocket pProgress);
   BOOL TTSWaveAddPhone (PTTSANAL pAnal, DWORD dwWaveNum, DWORD dwPhoneIndex, PCMTTS pTTS,
      PCM3DWave pWavePCM);
   BOOL TTSWaveAddPhoneSingle (PTTSANAL pAnal, DWORD dwWaveNum, DWORD dwPhoneIndex, BOOL fRightHalf, PCMTTS pTTS,
      PCM3DWave pWavePCM);
   BOOL UsePHONEAN (PTTSANAL pAnal, DWORD dwWaveNum, DWORD dwPhoneIndex, PVOID pPHONEAN, PVOID pPHONEANPrev, PCMTTS pTTS,
      BOOL fNow, DWORD dwLexWord, BOOL fRemoveExisting, PCM3DWave pWavePCM);
   BOOL AnalysisGroupTRIPHONETRAINMultiPass (PCProgressSocket pProgress, PTTSANAL pAnal, PCMTTS pTTS);
   BOOL AnalysisSpecificTRIPHONETRAINMultiPass (PCProgressSocket pProgress, PTTSANAL pAnal, PCMTTS pTTS);
   BOOL AnalysisGroupTRIPHONETRAIN (PCProgressSocket pProgress, PTTSANAL pAnal, PCMTTS pTTS,
      DWORD dwStartPhone, DWORD dwEndPhone);
   BOOL AnalysisSpecificTRIPHONETRAIN (PCProgressSocket pProgress, PTTSANAL pAnal, PCMTTS pTTS,
      DWORD dwStartPhone, DWORD dwEndPhone);
   void AnalysisGroupTRIPHONETRAINSub (PTTSANAL pAnal, PCMTTS pTTS, DWORD dwWave, DWORD dwThread,
      DWORD dwStartPhone, DWORD dwEndPhone);
   void AnalysisSpecificTRIPHONETRAINSub (PTTSANAL pAnal, PCMTTS pTTS, DWORD dwWave, DWORD dwThread,
      DWORD dwStartPhone, DWORD dwEndPhone);
   BOOL AnalysisPHONETRAIN (PCProgressSocket pProgress, PTTSANAL pAnal, PCMTTS pTTS);
   void AnalysisPHONETRAINSub (PTTSANAL pAnal, PCMTTS pTTS, DWORD dwWave, DWORD dwThread);
   BOOL AnalysisMegaPHONETRAIN (PCProgressSocket pProgress, PTTSANAL pAnal, PCMTTS pTTS);
   void AnalysisMegaPHONETRAINSub (PTTSANAL pAnal, PCMTTS pTTS, DWORD dwWave, DWORD dwThread);

#if 0 // not used anymore
   BOOL AnalysisAllWords (PTTSANAL pAnal, PCMTTS pTTS, PCProgressSocket pProgress);
   BOOL AnalysisMultiUnit (PTTSANAL pAnal, PCMTTS pTTS);
   BOOL AnalysisWordSyllableUnit (PTTSANAL pAnal, PCMTTS pTTS);
   BOOL AnalysisSyllableUnit (PTTSANAL pAnal, PCMTTS pTTS);
   BOOL AnalysisConnectUnits (PTTSANAL pAnal, PCMTTS pTTS);
   BOOL AnalysisDiphones (PTTSANAL pAnal, PCMTTS pTTS);
#endif // 0

   BOOL JoinCostsInit (FILE *pf, PCMem pmemJoinCosts, PTTSJOINCOSTS pJC);
   void JoinCostsFree (DWORD dwPass, PTTSJOINCOSTS pJC);
   BOOL JoinCostsTrain (DWORD dwPass, PCProgressSocket pProgress, PTTSANAL pAnal, PTTSJOINCOSTS pJC, PCMTTS pTTS);
   void AnalysisJoinCostsSub (DWORD dwPass, PTTSANAL pAnal, PTTSJOINCOSTS pJC, PCMTTS pTTS, DWORD dwWave, DWORD dwThread);
   BOOL JoinCostsWrite (DWORD dwPass, PTTSANAL pAnal, PTTSJOINCOSTS pJC, PCMTTS pTTS);
   void JoinCostsContext (DWORD dwIndex, PCMLexicon pLex, char *psz, DWORD dwBytes);
   void JoinSimulatePSOLA (PSRFEATURE pSRF, fp fPitchOrig, int iRaise);
   BOOL JoinSimulatePSOLAInWave (PCM3DWave pWave, PSRFEATURE paSRF, DWORD dwNum, PCMem pMem, int iRaise);
   BOOL JoinCalcPSOLASpread (PCMem pMem, fp fPitchAdjust);

   BOOL AnalysisTriPhoneGroup (PTTSANAL pAnal, PCMTTS pTTS);
   BOOL AnalysisAllTriPhones (PTTSANAL pAnal, PCMTTS pTTS, PCProgressSocket pProgress);
   BOOL AnalysisInit (PCProgressSocket pProgress, PCMem pmemAnal, PTTSANAL pAnal);
   void AnalysisFree (PTTSANAL pAnal);
   //BOOL AnalyzeWave (PCM3DWave pWave,  DWORD dwWaveNum,
   //                         PCListFixed paplTriPhone[][256], PCListFixed paplWord[]);
   PCListFixed AnalyzeAllWaves (PCProgressSocket pProgress,
                            PCListFixed paplTriPhone[][PHONEGROUPSQUARE], PCListFixed paplWord[],
                            fp *pfAveragePitchF0All, fp *pfAveragePitchF0Fuji, fp *pfAvgSyllableDur ,PCVoiceFile pVF,
                            double *pafEnergyPerPitch, char *pacEnergyPerPitch,
                            double *pafEnergyPerVolume, char *pacEnergyPerVolume,
                            double *pafPhonemePitchSum,
                            double *pafPhonemeDurationSum, double *pafPhonemeEnergySum, DWORD *padwPhonemeCount,
                            fp *pfFujiRangeLow, fp *pfFujiRangeMed, fp *pfFujiRangeHigh);
   BOOL FindNewTriPhoneOrWord (DWORD *padwPhone, DWORD dwNum, PCMMLNode2 pWords,
                                      PCListFixed palTriPhone[][PHONEGROUPSQUARE], DWORD *padwWordCount);
   BOOL RecommendFromText (PCMMLNode2 pSent, PCTextParse pTextParse, PCMem pMemString,
                                  PCListFixed palTriPhone[][PHONEGROUPSQUARE], DWORD *padwWordCount);
   BOOL RecommendFromLex (DWORD *pdwCurWord, PWSTR *papszWord, DWORD dwNum,
                                 PCTextParse pTextParse, PCMem pMemString,
                                 PCListFixed palTriPhone[][PHONEGROUPSQUARE], DWORD *padwWordCount);

   PWSTR WordEmphPhone (PCWaveAn pWaveAn, PCMTTS pTTS, DWORD dwWord,
      PWRDEMPH pEmph,
      DWORD *pdwPhoneStart, DWORD *pdwPhoneEnd,
      DWORD *pdwNumSyl, PSYLEMPH pSylEmph);
   void WordEmphPhone2 (PCWaveAn pWaveAn, PCMTTS pTTS, DWORD dwWord, DWORD dwNumSyl, PSYLEMPH pSylEmph);
   void WordEmphTrainSylPrep (PCWaveAn pWaveAn, PCMTTS pTTS, DWORD dwWord,
      PCMTTSTriPhonePros *pptp);
   PCListFixed WordEmphExtractFromWave (PCWaveAn pWaveAn, PCMTTS pTTS);
   void WordEmphExtractFromWave2 (PCWaveAn pWaveAn, PCMTTS pTTS, PCListFixed plWEMPH);
   void WordEmphTrainFromWavePrep (PCWaveAn pWaveAn, PCMTTS pTTS);
   void WordEmphTrainFromWave (PCWaveAn pWaveAn, PCMTTS pTTS);
   void WordEmphProduceSentSyl (PTTSANAL pAnal, PCMTTS pTTS);
   void WordEmphExtractAllWaves (PTTSANAL pAnal, PCMTTS pTTS, PCProgressSocket pProgress);
   void AdjustSYLANPitch (PTTSANAL pAnal, PCMTTS pTTS);
   DWORD GeneratePOSList (PTTSANAL pAnal, PCWaveAn pwa, DWORD dwWaveIndex, DWORD dwSylIndex, PCListFixed plPOS);
   // void WordEmphFromCommon (PTTSANAL pAnal);
   // void WordEmphFromWordLength (PTTSANAL pAnal, PCMTTS pTTS);
   void PhoneEmph (PTTSANAL pAnal, PCMTTS pTTS);
   // void WordEmphFromFuncWord (PTTSANAL pAnal, PCMTTS pTTS);
   // void WordEmphFromPunct (PTTSANAL pAnal, PCMTTS pTTS);
   // void WordEmphFromNGram (PTTSANAL pAnal, PCMTTS pTTS);
   BOOL CleanSRFEATURE (PSRFEATURE pasrf, DWORD dwNum, BOOL fVoiced, PCMem pMem);
   PSRFEATURE CacheSRFeaturesWithAdjust (PCM3DWave pWave, DWORD dwTimeStart, DWORD dwTimeEnd, BOOL fIsVoiced,
               fp fAvgEnergyForVoiced, PCMTTS pTTS, DWORD dwThread);

   WCHAR             m_szLexicon[256];    // lexicon filename
   CListFixed        m_lPHONEBLACK;       // blacklist

   // created at runtime
   PCMLexicon        m_pLex;              // from the lexicon cache

   // temp
   CMem              m_amemCacheConvert[MAXRAYTHREAD];   // temporary memory used for adding EnergyPerPitchGet() to cached waves
   CMem              m_memJoinCalcPSOLASpread;        // where the PSOLA spread calcs go for join costs
};
typedef CTTSWork *PCTTSWork;


DLLEXPORT BOOL TTSWorkFileOpenDialog (HWND hWnd, PWSTR pszFile, DWORD dwChars, BOOL fSave);


/*************************************************************************************
CTTSTransPros */
#define NUMTPQUAL       5        // how many different qualities are stored

class DLLEXPORT CTTSTransPros {
public:
   ESCNEWDELETE;

   CTTSTransPros (void);
   ~CTTSTransPros (void);

   PCMMLNode2 MMLTo (void);
   BOOL MMLFrom (PCMMLNode2 pNode);
   CTTSTransPros *Clone (void);

   DWORD Dialog (PCEscWindow pWindow, PCM3DWave pWave, PWSTR pszText, PWSTR pszSpeaker, PCMem pMemText);
   DWORD DialogQuick (PCEscWindow pWindow, PWSTR pszText, PWSTR pszPreModText, PCMem pMemText);
   BOOL WaveToPerWordTP (PWSTR pszOrigText, PCM3DWave pWave, PWSTR pszTTS, PCMem pMem);
   BOOL WaveToPerPhoneTP (PWSTR pszOrigText, PCM3DWave pWave, DWORD dwPitch,
      PCMLexicon pLex, PWSTR pszTTS, PCMem pMem);

   // member variables that might want to modify. Saved and loaded to MML
   //WCHAR          m_szTTS[256];        // TTS voice to use
   //WCHAR          m_szTTSWork[256];    // TTS project to use, will add recording to this tts voice
   //WCHAR          m_szVoiceFile[256];  // SR training data to use
   fp             m_fAvgPitch;         // speakers average pitch, if 0 will be calculated
   DWORD          m_dwQuality;         // quality. 0 = all synthesized, 1=per-word, 2=per phoneme, 3 = 2-per phone, 4=3-per phoneme
   DWORD          m_dwPitchType;       // 0 = no pitch, 1 = relative, 2=absoluate
   DWORD          m_dwVolType;         // 0 = no vol, 1 = relative, 2=absolute
   DWORD          m_dwDurType;         // 0 = no dur, 1 = relative, 2= absolute
   fp             m_fPitchAdjust;      // 1.0 = no change, 2.0 = 2x the pitch, etc.
   fp             m_fPitchExpress;     // 1.0 = no change, 2.0 = 2x the pitch, etc.
   fp             m_fVolAdjust;        // 1.0 = no change, 2.0 = 2x as loud, etc.
   fp             m_fDurAdjust;        // 1.0 = no change, 2.0 = 2x as long, etc.

   CMem           m_memPreModText;       // original text, BEFORE user removed %2 and (am/are/is)

   // used internally so dont call
   PCM3DWave      m_pWave;             // wave to modify
   PCM3DWave      m_pWaveTP;           // scratch wave for the transplanted prosody
   DWORD          m_dwQualityWaveTP;   // quality (m_dwQuality) currently stored in m_pWaveTP, or -1
   CMem           m_aMemQuality[NUMTPQUAL];  // memory containing versions of all the qualities
   PCMem          m_pMemText;          // filled in with TP text
   BOOL           m_fKeepRec;          // TRUE if should keep rec
   BOOL           m_fKeepSeg;          // TRUE if should keep seg
   BOOL           m_fModOrig;          // if TRUE modify original recording
   PCVoiceFile    m_pSR;               // speech recognition to use
   PCMTTS         m_pTTS;              // text-to-speech to use

private:
   BOOL WaveDeterminePhone (PCM3DWave pWave, PCMLexicon pLex, PCMTTS pTTS,
                                        PCListFixed plPCMTTSTriPhonePros);
};
typedef CTTSTransPros *PCTTSTransPros;

/*********************************************************************************
CNPREffect */

// NPRQI - Information about NPR engine
typedef struct {
   PCWSTR         pszID;         // name of the effect stored in MML so can recreate
   PCWSTR         pszName;       // name of the effect (shown to users)
   PCWSTR         pszDesc;       // description of the effect
   BOOL           fInPlace;      // if TRUE then modified the image in-place, else needs
                                 // to modify to a destination image
} NPRQI, *PNPRQI;

class CNPREffectsList;
typedef CNPREffectsList *PCNPREffectsList;

class CNPREffect {
public:
   virtual void Delete (void) = 0;
      // Has the effect delete itself. In general, this should be called
      // instead of the "delete" call

   virtual void QueryInfo (PNPRQI pqi) = 0;
      // Has the effect fill in a  NPRQI structure so the application can get
      // the name, etc.

   virtual PCMMLNode2 MMLTo (void) = 0;
      // Standard MMLTo

   virtual BOOL MMLFrom (PCMMLNode2 pNode) = 0;
      // Stnadard MMLFrom

   virtual CNPREffect *Clone (void) = 0;
      // Clones the object

   virtual BOOL Render (PCImage pOrig, PCImage pDest, PCRenderSuper pRender,
                        PCWorldSocket pWorld, BOOL fFinal, PCProgressSocket pProgress) = 0;
      // Has the effect apply itself to the image pOrig. pDest is the image that
      // the changes will be written to. If NPRQI.fInPlace is TRUE then pDest MUST
      // be pOrig. If it's FALSE then pDest MUST be different than pOrig.
      // pDest must already be filled with a copy of pOrig.
      // pRender is the rendering engine used to produce the image, or NULL if unavailable
      // pWorld is the world used to produce the image, or NULLif unavailable
      // fFinal is TRUE if this is the final-quality image
      // pProgress - is the progress bar, or NULL if none

   virtual BOOL Render (PCFImage pOrig, PCFImage pDest, PCRenderSuper pRender,
                        PCWorldSocket pWorld, BOOL fFinal, PCProgressSocket pProgress) = 0;
      // Like the other render() except using a PCFImage

   virtual BOOL Dialog (PCEscWindow pWindow, PCNPREffectsList pAllEffects, PCImage pTest, 
      PCRenderSuper pRender, PCWorldSocket pWorld) = 0;
      // Brings up UI to edit the settings. pWindow is the  window to display on.
      // pTest is the test image (which is NOT to be modified) to base effect on.
      // pAllEffects is the PCNPRAllEffects (bugbug) object that does all the
      // effects, so a user can see how the final image looks with all the effects.
      // pRender is the render engine that created the pTest image
      // pWorld is the world that created the test image.

   virtual BOOL IsPainterly (void) = 0;
      // return TRUE if this is a painterly effect, which means that it doesn't
      // need as much resolution

};
typedef CNPREffect *PCNPREffect;


/*********************************************************************************
CNPREffectOutline */

class DLLEXPORT CNPREffectOutline : public CNPREffect {
public:
   ESCNEWDELETE;

   CNPREffectOutline (DWORD dwRenderShard);
   ~CNPREffectOutline (void);

   virtual void Delete (void);
   virtual void QueryInfo (PNPRQI pqi);
   virtual PCMMLNode2 MMLTo (void);
   virtual BOOL MMLFrom (PCMMLNode2 pNode);
   virtual CNPREffect *Clone (void);
   virtual BOOL Render (PCImage pOrig, PCImage pDest, PCRenderSuper pRender,
                        PCWorldSocket pWorld, BOOL fFinal, PCProgressSocket pProgress);
   virtual BOOL Render (PCFImage pOrig, PCFImage pDest, PCRenderSuper pRender,
                        PCWorldSocket pWorld, BOOL fFinal, PCProgressSocket pProgress);
   virtual BOOL Dialog (PCEscWindow pWindow, PCNPREffectsList pAllEffects, PCImage pTest,
      PCRenderSuper pRender, PCWorldSocket pWorld);
   virtual BOOL IsPainterly (void);

   CNPREffectOutline * CloneEffect (void);

   // variables
   DWORD          m_dwRenderShard;  // where rendering to
   DWORD          m_dwLevel;     // outline level, 0 for none, 1 for 
                                 // 1 - Fill in primary object borders, but secondary are 100% transparent
                                 // 2 - Fill in primary object borders, secondary are 50% transparent
                                 // 3 - Fill in pimary object border, secondary are 50%, tertiary are 75% trans
   COLORREF       m_cOutline;    // color to use

   BOOL           m_fShadeSelected; // if TRUE shade selected with the selected color (not used in final render)
   COLORREF       m_cOutlineSelected; // color to use for selected objects

   BOOL           m_fSetAllObjectsToColor; // If this is true, if the ID != 0 then
                                 // change the object color to cSetObjectsTo. Use this to just
                                 // show an outline of the objects while keeping the objects opaque.
   COLORREF       m_cSetObjectsTo;  // What color to make objects.

   CPoint         m_pPenWidth;   // width of pen, [0] = major, [1] = minor, [2]= tertiary
   fp             m_fNoiseAmount;   // amount that noise jiggles, 0+
   DWORD          m_dwNoiseDetail;  // frequency of noise jiggles, 5+

   // BUGBUG - will need more options for outline
private:
   CMem           m_memOutline;  // temporary memory for outline
   BOOL RenderFinal (PCImage pImage, PCFImage pFImage);

};
typedef CNPREffectOutline *PCNPREffectOutline;

extern PWSTR gpszEffectOutline;


/*********************************************************************************
CNPREffectFog */

class DLLEXPORT CNPREffectFog : public CNPREffect, public CEscMultiThreaded {
public:
   ESCNEWDELETE;

   CNPREffectFog (DWORD dwRenderShard);
   ~CNPREffectFog (void);

   virtual void Delete (void);
   virtual void QueryInfo (PNPRQI pqi);
   virtual PCMMLNode2 MMLTo (void);
   virtual BOOL MMLFrom (PCMMLNode2 pNode);
   virtual CNPREffect *Clone (void);
   virtual BOOL Render (PCImage pOrig, PCImage pDest, PCRenderSuper pRender,
                        PCWorldSocket pWorld, BOOL fFinal, PCProgressSocket pProgress);
   virtual BOOL Render (PCFImage pOrig, PCFImage pDest, PCRenderSuper pRender,
                        PCWorldSocket pWorld, BOOL fFinal, PCProgressSocket pProgress);
   virtual BOOL Dialog (PCEscWindow pWindow, PCNPREffectsList pAllEffects, PCImage pTest, 
      PCRenderSuper pRender, PCWorldSocket pWorld);
   virtual BOOL IsPainterly (void);

   // CEscMultiThreaded
   virtual void EscMultiThreadedCallback (PVOID pParams, DWORD dwParamSize,
      DWORD dwThread);

   CNPREffectFog * CloneEffect (void);

   // variables
   DWORD          m_dwRenderShard;  // where rendering to
   BOOL           m_fFog;        // if TRUE then show
   COLORREF       m_cFogColor;   // fog color
   CPoint         m_pDist;       // distance to 1/2 attenuation
   BOOL           m_fSkydomeColor;  // if TRUE use the skydome color
   BOOL           m_fIgnoreSkydome; // if TRUE don't draw on skydome
   BOOL           m_fIgnoreBackground; // if TRUE don't draw on background

private:

};
typedef CNPREffectFog *PCNPREffectFog;

extern PWSTR gpszEffectFog;
DLLEXPORT DWORD FindSkydome (PCWorldSocket pWorld);



/*********************************************************************************
CNPREffectFogHLS */

class DLLEXPORT CNPREffectFogHLS : public CNPREffect, public CEscMultiThreaded {
public:
   ESCNEWDELETE;

   CNPREffectFogHLS (DWORD dwRenderShard);
   ~CNPREffectFogHLS (void);

   virtual void Delete (void);
   virtual void QueryInfo (PNPRQI pqi);
   virtual PCMMLNode2 MMLTo (void);
   virtual BOOL MMLFrom (PCMMLNode2 pNode);
   virtual CNPREffect *Clone (void);
   virtual BOOL Render (PCImage pOrig, PCImage pDest, PCRenderSuper pRender,
                        PCWorldSocket pWorld, BOOL fFinal, PCProgressSocket pProgress);
   virtual BOOL Render (PCFImage pOrig, PCFImage pDest, PCRenderSuper pRender,
                        PCWorldSocket pWorld, BOOL fFinal, PCProgressSocket pProgress);
   virtual BOOL Dialog (PCEscWindow pWindow, PCNPREffectsList pAllEffects, PCImage pTest, 
      PCRenderSuper pRender, PCWorldSocket pWorld);
   virtual BOOL IsPainterly (void);


   // CEscMultiThreaded
   virtual void EscMultiThreadedCallback (PVOID pParams, DWORD dwParamSize,
      DWORD dwThread);

   CNPREffectFogHLS * CloneEffect (void);

   // variables
   DWORD          m_dwRenderShard;  // where rendering to
   COLORREF       m_cFogHLSColor;   // FogHLS color
   CPoint         m_pDist;       // distance to 1/2 attenuation
   BOOL           m_fIgnoreSkydome; // if TRUE don't draw on skydome
   BOOL           m_fIgnoreBackground; // if TRUE don't draw on background

private:
   BOOL RenderAny (PCWorldSocket pWorld, PCImage pImage, PCFImage pFImage);

};
typedef CNPREffectFogHLS *PCNPREffectFogHLS;

extern PWSTR gpszEffectFogHLS;

/*********************************************************************************
CNPREffectColor */

class DLLEXPORT CNPREffectColor : public CNPREffect, public CEscMultiThreaded {
public:
   ESCNEWDELETE;

   CNPREffectColor (DWORD dwRenderShard);
   ~CNPREffectColor (void);

   virtual void Delete (void);
   virtual void QueryInfo (PNPRQI pqi);
   virtual PCMMLNode2 MMLTo (void);
   virtual BOOL MMLFrom (PCMMLNode2 pNode);
   virtual CNPREffect *Clone (void);
   virtual BOOL Render (PCImage pOrig, PCImage pDest, PCRenderSuper pRender,
                        PCWorldSocket pWorld, BOOL fFinal, PCProgressSocket pProgress);
   virtual BOOL Render (PCFImage pOrig, PCFImage pDest, PCRenderSuper pRender,
                        PCWorldSocket pWorld, BOOL fFinal, PCProgressSocket pProgress);
   virtual BOOL Dialog (PCEscWindow pWindow, PCNPREffectsList pAllEffects, PCImage pTest, 
      PCRenderSuper pRender, PCWorldSocket pWorld);
   virtual BOOL IsPainterly (void);


   // CEscMultiThreaded
   virtual void EscMultiThreadedCallback (PVOID pParams, DWORD dwParamSize,
      DWORD dwThread);

   CNPREffectColor * CloneEffect (void);

   // variables
   DWORD          m_dwRenderShard;  // where rendering to
   BOOL           m_fUseHLS;        // if true use HLS
   CPoint         m_pHLS;           // HLS values
   BOOL           m_fUseCB;         // if TRUE use contrast and brightness
   CPoint         m_pCB;            // contrast and brighness
   BOOL           m_fUseRGB;        // if TRUE use RGB
   CPoint         m_pRGB;           // RGB mods
   BOOL           m_afRGBInvert[3]; // if set to TRUE then invert RGB
   BOOL           m_fIgnoreBackground; // if TRUE don't draw on background

private:
   BOOL RenderAny (PCImage pImage, PCFImage pFImage);

};
typedef CNPREffectColor *PCNPREffectColor;

extern PWSTR gpszEffectColor;




/*********************************************************************************
CNPREffectAutoExposure */

class DLLEXPORT CNPREffectAutoExposure : public CNPREffect, public CEscMultiThreaded {
public:
   ESCNEWDELETE;

   CNPREffectAutoExposure (DWORD dwRenderShard);
   ~CNPREffectAutoExposure (void);

   virtual void Delete (void);
   virtual void QueryInfo (PNPRQI pqi);
   virtual PCMMLNode2 MMLTo (void);
   virtual BOOL MMLFrom (PCMMLNode2 pNode);
   virtual CNPREffect *Clone (void);
   virtual BOOL Render (PCImage pOrig, PCImage pDest, PCRenderSuper pRender,
                        PCWorldSocket pWorld, BOOL fFinal, PCProgressSocket pProgress);
   virtual BOOL Render (PCFImage pOrig, PCFImage pDest, PCRenderSuper pRender,
                        PCWorldSocket pWorld, BOOL fFinal, PCProgressSocket pProgress);
   virtual BOOL Dialog (PCEscWindow pWindow, PCNPREffectsList pAllEffects, PCImage pTest, 
      PCRenderSuper pRender, PCWorldSocket pWorld);
   virtual BOOL IsPainterly (void);

   // CEscMultiThreaded
   virtual void EscMultiThreadedCallback (PVOID pParams, DWORD dwParamSize,
      DWORD dwThread);

   CNPREffectAutoExposure * CloneEffect (void);

   // variables
   DWORD          m_dwRenderShard;  // where rendering to
   TEXTUREPOINT   m_tpExposureMinMax;  // minumum (.h) and maximum (.v) exposure
   TEXTUREPOINT   m_tpExposureRealLight;   // maps to the given real light levels
   fp             m_fGrayAt;        // stary greying image at this exposure (min/max), down to minimum
   fp             m_fNoiseAt;       // noise level at this amount (in terms of min/max)

private:
   BOOL RenderAny (PCRenderSuper pRender, PCImage pImage, PCFImage pFImage);

};
typedef CNPREffectAutoExposure *PCNPREffectAutoExposure;

extern PWSTR gpszEffectAutoExposure;


/*********************************************************************************
CNPREffectColorBlind */

class DLLEXPORT CNPREffectColorBlind : public CNPREffect {
public:
   ESCNEWDELETE;

   CNPREffectColorBlind (DWORD dwRenderShard);
   ~CNPREffectColorBlind (void);

   virtual void Delete (void);
   virtual void QueryInfo (PNPRQI pqi);
   virtual PCMMLNode2 MMLTo (void);
   virtual BOOL MMLFrom (PCMMLNode2 pNode);
   virtual CNPREffect *Clone (void);
   virtual BOOL Render (PCImage pOrig, PCImage pDest, PCRenderSuper pRender,
                        PCWorldSocket pWorld, BOOL fFinal, PCProgressSocket pProgress);
   virtual BOOL Render (PCFImage pOrig, PCFImage pDest, PCRenderSuper pRender,
                        PCWorldSocket pWorld, BOOL fFinal, PCProgressSocket pProgress);
   virtual BOOL Dialog (PCEscWindow pWindow, PCNPREffectsList pAllEffects, PCImage pTest, 
      PCRenderSuper pRender, PCWorldSocket pWorld);
   virtual BOOL IsPainterly (void);

   CNPREffectColorBlind * CloneEffect (void);

   // variables
   DWORD          m_dwRenderShard;  // where rendering to
   DWORD          m_dwRed;          // red chomas, 0x01=red, 0x02=green, 0x04=blue
   DWORD          m_dwGreen;        // green chromas
   DWORD          m_dwBlue;         // blue chromas

private:
   BOOL RenderAny (PCImage pImage, PCFImage pFImage);

};
typedef CNPREffectColorBlind *PCNPREffectColorBlind;

extern PWSTR gpszEffectColorBlind;

/*********************************************************************************
CNPREffectPosterize */

class DLLEXPORT CNPREffectPosterize : public CNPREffect, public CEscMultiThreaded {
public:
   ESCNEWDELETE;

   CNPREffectPosterize (DWORD dwRenderShard);
   ~CNPREffectPosterize (void);

   virtual void Delete (void);
   virtual void QueryInfo (PNPRQI pqi);
   virtual PCMMLNode2 MMLTo (void);
   virtual BOOL MMLFrom (PCMMLNode2 pNode);
   virtual CNPREffect *Clone (void);
   virtual BOOL Render (PCImage pOrig, PCImage pDest, PCRenderSuper pRender,
                        PCWorldSocket pWorld, BOOL fFinal, PCProgressSocket pProgress);
   virtual BOOL Render (PCFImage pOrig, PCFImage pDest, PCRenderSuper pRender,
                        PCWorldSocket pWorld, BOOL fFinal, PCProgressSocket pProgress);
   virtual BOOL Dialog (PCEscWindow pWindow, PCNPREffectsList pAllEffects, PCImage pTest, 
      PCRenderSuper pRender, PCWorldSocket pWorld);
   virtual BOOL IsPainterly (void);

   // CEscMultiThreaded
   virtual void EscMultiThreadedCallback (PVOID pParams, DWORD dwParamSize,
      DWORD dwThread);

   CNPREffectPosterize * CloneEffect (void);

   // variables
   DWORD          m_dwRenderShard;  // where rendering to
   CPoint         m_pPalette;       // # colors in pallette
   fp             m_fHueShift;      // how much to shift colors
   BOOL           m_fIgnoreBackground; // if TRUE don't draw on background

private:
   BOOL RenderAny (PCImage pImage, PCFImage pFImage);

};
typedef CNPREffectPosterize *PCNPREffectPosterize;

extern PWSTR gpszEffectPosterize;




/*********************************************************************************
CNPREffectHalftone */

class DLLEXPORT CNPREffectHalftone : public CNPREffect, public CEscMultiThreaded {
public:
   ESCNEWDELETE;

   CNPREffectHalftone (DWORD dwRenderShard);
   ~CNPREffectHalftone (void);

   virtual void Delete (void);
   virtual void QueryInfo (PNPRQI pqi);
   virtual PCMMLNode2 MMLTo (void);
   virtual BOOL MMLFrom (PCMMLNode2 pNode);
   virtual CNPREffect *Clone (void);
   virtual BOOL Render (PCImage pOrig, PCImage pDest, PCRenderSuper pRender,
                        PCWorldSocket pWorld, BOOL fFinal, PCProgressSocket pProgress);
   virtual BOOL Render (PCFImage pOrig, PCFImage pDest, PCRenderSuper pRender,
                        PCWorldSocket pWorld, BOOL fFinal, PCProgressSocket pProgress);
   virtual BOOL Dialog (PCEscWindow pWindow, PCNPREffectsList pAllEffects, PCImage pTest, 
      PCRenderSuper pRender, PCWorldSocket pWorld);
   virtual BOOL IsPainterly (void);

   // CEscMultiThreaded
   virtual void EscMultiThreadedCallback (PVOID pParams, DWORD dwParamSize,
      DWORD dwThread);

   CNPREffectHalftone * CloneEffect (void);

   // variables
   DWORD          m_dwRenderShard;  // where rendering to
   BOOL           m_fColor;         // if set the generate color image
   DWORD          m_dwPattern;      // halftoning pattern
   fp             m_fSize;          // size (in percent, 1 = 1%) of width
   BOOL           m_fIgnoreBackground; // if TRUE don't draw on background

private:
   BOOL RenderAny (PCImage pImage, PCFImage pFImage);

};
typedef CNPREffectHalftone *PCNPREffectHalftone;

extern PWSTR gpszEffectHalftone;




/*********************************************************************************
CNPREffectFilmGrain */

class DLLEXPORT CNPREffectFilmGrain : public CNPREffect, public CEscMultiThreaded {
public:
   ESCNEWDELETE;

   CNPREffectFilmGrain (DWORD dwRenderShard);
   ~CNPREffectFilmGrain (void);

   virtual void Delete (void);
   virtual void QueryInfo (PNPRQI pqi);
   virtual PCMMLNode2 MMLTo (void);
   virtual BOOL MMLFrom (PCMMLNode2 pNode);
   virtual CNPREffect *Clone (void);
   virtual BOOL Render (PCImage pOrig, PCImage pDest, PCRenderSuper pRender,
                        PCWorldSocket pWorld, BOOL fFinal, PCProgressSocket pProgress);
   virtual BOOL Render (PCFImage pOrig, PCFImage pDest, PCRenderSuper pRender,
                        PCWorldSocket pWorld, BOOL fFinal, PCProgressSocket pProgress);
   virtual BOOL Dialog (PCEscWindow pWindow, PCNPREffectsList pAllEffects, PCImage pTest, 
      PCRenderSuper pRender, PCWorldSocket pWorld);
   virtual BOOL IsPainterly (void);

   // CEscMultiThreaded
   virtual void EscMultiThreadedCallback (PVOID pParams, DWORD dwParamSize,
      DWORD dwThread);

   CNPREffectFilmGrain * CloneEffect (void);

   // variables
   DWORD          m_dwRenderShard;  // where rendering to
   fp             m_fNoise;         // amount of noise in the film grain
   BOOL           m_fIgnoreBackground; // checked if ignore the background

private:
   BOOL RenderAny (PCImage pImage, PCFImage pFImage);

};
typedef CNPREffectFilmGrain *PCNPREffectFilmGrain;

extern PWSTR gpszEffectFilmGrain;




/*********************************************************************************
CNPREffectWatercolor */

class DLLEXPORT CNPREffectWatercolor : public CNPREffect {
public:
   ESCNEWDELETE;

   CNPREffectWatercolor (DWORD dwRenderShard);
   ~CNPREffectWatercolor (void);

   virtual void Delete (void);
   virtual void QueryInfo (PNPRQI pqi);
   virtual PCMMLNode2 MMLTo (void);
   virtual BOOL MMLFrom (PCMMLNode2 pNode);
   virtual CNPREffect *Clone (void);
   virtual BOOL Render (PCImage pOrig, PCImage pDest, PCRenderSuper pRender,
                        PCWorldSocket pWorld, BOOL fFinal, PCProgressSocket pProgress);
   virtual BOOL Render (PCFImage pOrig, PCFImage pDest, PCRenderSuper pRender,
                        PCWorldSocket pWorld, BOOL fFinal, PCProgressSocket pProgress);
   virtual BOOL Dialog (PCEscWindow pWindow, PCNPREffectsList pAllEffects, PCImage pTest, 
      PCRenderSuper pRender, PCWorldSocket pWorld);
   virtual BOOL IsPainterly (void);

   CNPREffectWatercolor * CloneEffect (void);

   // variables
   DWORD          m_dwRenderShard;  // where rendering to
   fp             m_fNoise;         // amount of noise on borders
   BOOL           m_fIgnoreBackground; // checked if ignore the background

   fp             m_fMaskSize;         // as a percent of screen width
   fp             m_fNoiseSize;        // as a percent of screen width
   BOOL           m_fStainedGlass;     // if stained glass then new ID for each
   fp             m_fColorError;       // amount of color error acceptable, from 0.. 1
   fp             m_fDarkenAmount;     // amount to darken around edges
   fp             m_fEdgeSize;         // how much paint groups up against edge (in percent of screen width)
   fp             m_fNoiseDarken ;     // amount that noise darkens
   CPoint         m_pColorVar;         // color varion. [0] = h, [1] = l, [2] = s. From 0..1
   CPoint         m_pColorPalette;     // # colors in pallette
   fp             m_fColorHueShift;    // how much to shift color hue
   BOOL           m_fColorUseFixed;    // if TRUE used the fixed palette
   COLORREF       m_acColorFixed[5];   // colors in fixed palette

private:
   BOOL RenderAny (PCImage pImage, PCFImage pFImage, PCProgressSocket pProgress);
   void FillMask (int *paiBuf, DWORD dwSize, PCNoise2D pNoise);
   void MaskEval (int *paiMask, DWORD dwSize, DWORD dwWidth,
                                     DWORD dwHeight, BOOL f360, PCImage pImage,
                                     PCFImage pFImage, DWORD dwX, DWORD dwY,
                                     WORD *pawColor, BYTE *pabCovered, int *paiCoverage,
                                     BOOL fMaskOver);
   DWORD MaskPaint (DWORD dwSize, DWORD dwWidth,
                                     DWORD dwHeight, BOOL f360, PCImage pImage,
                                     PCFImage pFImage, DWORD dwX, DWORD dwY,
                                     WORD *pawColor, int *paiCoverage, PCNoise2D pNoise, DWORD dwID);
   void Color (WORD *pawColor);

   CMem           m_memMask;        // memory to store masks in
};
typedef CNPREffectWatercolor *PCNPREffectWatercolor;

extern PWSTR gpszEffectWatercolor;



/*********************************************************************************
CNPREffectTV */

class DLLEXPORT CNPREffectTV : public CNPREffect {
public:
   ESCNEWDELETE;

   CNPREffectTV (DWORD dwRenderShard);
   ~CNPREffectTV (void);

   virtual void Delete (void);
   virtual void QueryInfo (PNPRQI pqi);
   virtual PCMMLNode2 MMLTo (void);
   virtual BOOL MMLFrom (PCMMLNode2 pNode);
   virtual CNPREffect *Clone (void);
   virtual BOOL Render (PCImage pOrig, PCImage pDest, PCRenderSuper pRender,
                        PCWorldSocket pWorld, BOOL fFinal, PCProgressSocket pProgress);
   virtual BOOL Render (PCFImage pOrig, PCFImage pDest, PCRenderSuper pRender,
                        PCWorldSocket pWorld, BOOL fFinal, PCProgressSocket pProgress);
   virtual BOOL Dialog (PCEscWindow pWindow, PCNPREffectsList pAllEffects, PCImage pTest, 
      PCRenderSuper pRender, PCWorldSocket pWorld);
   virtual BOOL IsPainterly (void);

   CNPREffectTV * CloneEffect (void);

   // variables
   DWORD          m_dwRenderShard;  // where rendering to
   fp             m_fNoise;         // amount of noise in the film grain
   BOOL           m_fIgnoreBackground; // checked if ignore the background

private:
   BOOL RenderAny (PCImage pImage, PCFImage pFImage);

};
typedef CNPREffectTV *PCNPREffectTV;

extern PWSTR gpszEffectTV;


/*********************************************************************************
CNPREffectBlur */

class DLLEXPORT CNPREffectBlur : public CNPREffect, public CEscMultiThreaded {
public:
   ESCNEWDELETE;

   CNPREffectBlur (DWORD dwRenderShard);
   ~CNPREffectBlur (void);

   virtual void Delete (void);
   virtual void QueryInfo (PNPRQI pqi);
   virtual PCMMLNode2 MMLTo (void);
   virtual BOOL MMLFrom (PCMMLNode2 pNode);
   virtual CNPREffect *Clone (void);
   virtual BOOL Render (PCImage pOrig, PCImage pDest, PCRenderSuper pRender,
                        PCWorldSocket pWorld, BOOL fFinal, PCProgressSocket pProgress);
   virtual BOOL Render (PCFImage pOrig, PCFImage pDest, PCRenderSuper pRender,
                        PCWorldSocket pWorld, BOOL fFinal, PCProgressSocket pProgress);
   virtual BOOL Dialog (PCEscWindow pWindow, PCNPREffectsList pAllEffects, PCImage pTest, 
      PCRenderSuper pRender, PCWorldSocket pWorld);
   virtual BOOL IsPainterly (void);


   // CEscMultiThreaded
   virtual void EscMultiThreadedCallback (PVOID pParams, DWORD dwParamSize,
      DWORD dwThread);

   CNPREffectBlur * CloneEffect (void);

   // variables
   DWORD          m_dwRenderShard;  // where rendering to
   fp             m_fBlurWidth;     // width (as percent of screen) to blur
   DWORD          m_dwBlurSize;     // accuracy of blur
   fp             m_fScaleSaturated;   // how much to scale saturated
   BOOL           m_fIgnoreBackground; // if TRUE don't draw on background
   BOOL           m_fBlurRange;     // if TRUE then blur a range
   TEXTUREPOINT   m_tpBlurMinMax;    // blur min and max

private:
   BOOL RenderAny (PCImage pImageSrc, PCFImage pFImageSrc,
                   PCImage pImageDest, PCFImage pFImageDest,
                   PCProgressSocket pProgress);

};
typedef CNPREffectBlur *PCNPREffectBlur;

extern PWSTR gpszEffectBlur;


/*********************************************************************************
CNPREffectDOF */

class DLLEXPORT CNPREffectDOF : public CNPREffect, public CEscMultiThreaded {
public:
   ESCNEWDELETE;

   CNPREffectDOF (DWORD dwRenderShard);
   ~CNPREffectDOF (void);

   virtual void Delete (void);
   virtual void QueryInfo (PNPRQI pqi);
   virtual PCMMLNode2 MMLTo (void);
   virtual BOOL MMLFrom (PCMMLNode2 pNode);
   virtual CNPREffect *Clone (void);
   virtual BOOL Render (PCImage pOrig, PCImage pDest, PCRenderSuper pRender,
                        PCWorldSocket pWorld, BOOL fFinal, PCProgressSocket pProgress);
   virtual BOOL Render (PCFImage pOrig, PCFImage pDest, PCRenderSuper pRender,
                        PCWorldSocket pWorld, BOOL fFinal, PCProgressSocket pProgress);
   virtual BOOL Dialog (PCEscWindow pWindow, PCNPREffectsList pAllEffects, PCImage pTest, 
      PCRenderSuper pRender, PCWorldSocket pWorld);
   virtual BOOL IsPainterly (void);

   // CEscMultiThreaded
   virtual void EscMultiThreadedCallback (PVOID pParams, DWORD dwParamSize,
      DWORD dwThread);

   CNPREffectDOF * CloneEffect (void);

   // variables
   DWORD          m_dwRenderShard;  // where rendering to
   fp             m_fBlurWidth;     // width (as percent of screen) to blur
   DWORD          m_dwBlurSize;     // accuracy of blur
   TEXTUREPOINT   m_tpBlurDistMinMax;    // blur min and max. starts blurring at min (h), completely blurred at max (v)
   TEXTUREPOINT   m_tpBlurNearMinMax;    // blur min and max. starts un-blurring at min (h), completely un-blurred at max (v)

private:
   BOOL RenderAny (PCImage pImageSrc, PCFImage pFImageSrc,
                   PCImage pImageDest, PCFImage pFImageDest,
                   PCProgressSocket pProgress);

};
typedef CNPREffectDOF *PCNPREffectDOF;

extern PWSTR gpszEffectDOF;

/*********************************************************************************
CBlurBuf - Calculates blur info and stores it in a buffer
*/
class CBlurBuf {
   friend class CNPREffectDOF;

public:
   CBlurBuf (void);
   ~CBlurBuf (void);

   BOOL Init (float fLenStandard, DWORD dwSize);

   void Blur (PCImage pImage, int iX, int iY,
      BOOL fIgnoreBackground, PTEXTUREPOINT ptMinMax,
      WORD wScaleSaturdated, WORD *pawColor);
   void Blur (PCFImage pImage, int iX, int iY,
      BOOL fIgnoreBackground, PTEXTUREPOINT ptMinMax,
      float *pafColor);

private:
   BOOL InitToBlank (DWORD dwWidth, DWORD dwHeight);
   void ZeroEdges (void);
   void MirrorFromULCorner (void);
   void Normalize (void);
   void FloatToWord (void);

   CMem        m_memFloat; // memory for floating-point representation
   CMem        m_memWORD;  // memory for word-representation of blur
   DWORD       m_dwWidth;  // width (in pixels)
   DWORD       m_dwHeight; // height (in pixels)
   WORD        m_wScaleCenter;   // how much to scale the central point by
   WORD        m_wScaleNonCenter;   // amount to scale non-center pixels by (useful for glow)
   DWORD       m_dwSumScaleNonCenter;  // sum of non-center scales
};
typedef CBlurBuf *PCBlurBuf;





/*********************************************************************************
CNPREffectGlow */

class DLLEXPORT CNPREffectGlow : public CNPREffect, public CEscMultiThreaded {
public:
   ESCNEWDELETE;

   CNPREffectGlow (DWORD dwRenderShard);
   ~CNPREffectGlow (void);

   virtual void Delete (void);
   virtual void QueryInfo (PNPRQI pqi);
   virtual PCMMLNode2 MMLTo (void);
   virtual BOOL MMLFrom (PCMMLNode2 pNode);
   virtual CNPREffect *Clone (void);
   virtual BOOL Render (PCImage pOrig, PCImage pDest, PCRenderSuper pRender,
                        PCWorldSocket pWorld, BOOL fFinal, PCProgressSocket pProgress);
   virtual BOOL Render (PCFImage pOrig, PCFImage pDest, PCRenderSuper pRender,
                        PCWorldSocket pWorld, BOOL fFinal, PCProgressSocket pProgress);
   virtual BOOL Dialog (PCEscWindow pWindow, PCNPREffectsList pAllEffects, PCImage pTest, 
      PCRenderSuper pRender, PCWorldSocket pWorld);
   virtual BOOL IsPainterly (void);

   // CEscMultiThreaded
   virtual void EscMultiThreadedCallback (PVOID pParams, DWORD dwParamSize,
      DWORD dwThread);

   CNPREffectGlow * CloneEffect (void);

   // variables
   DWORD          m_dwRenderShard;  // where rendering to
   fp             m_fGlowWidth;     // width (as percent of screen) to blur
   DWORD          m_dwMode;         // mode to use
   BOOL           m_fIgnoreBackground; // if TRUE don't draw on background

private:
   BOOL RenderAny (PCImage pImageSrc, PCFImage pFImageSrc,
                   PCImage pImageDest, PCFImage pFImageDest,
                   PCProgressSocket pProgress);

};
typedef CNPREffectGlow *PCNPREffectGlow;

extern PWSTR gpszEffectGlow;



/*********************************************************************************
CNPREffectSpyglass */

class DLLEXPORT CNPREffectSpyglass : public CNPREffect {
public:
   ESCNEWDELETE;

   CNPREffectSpyglass (DWORD dwRenderShard);
   ~CNPREffectSpyglass (void);

   virtual void Delete (void);
   virtual void QueryInfo (PNPRQI pqi);
   virtual PCMMLNode2 MMLTo (void);
   virtual BOOL MMLFrom (PCMMLNode2 pNode);
   virtual CNPREffect *Clone (void);
   virtual BOOL Render (PCImage pOrig, PCImage pDest, PCRenderSuper pRender,
                        PCWorldSocket pWorld, BOOL fFinal, PCProgressSocket pProgress);
   virtual BOOL Render (PCFImage pOrig, PCFImage pDest, PCRenderSuper pRender,
                        PCWorldSocket pWorld, BOOL fFinal, PCProgressSocket pProgress);
   virtual BOOL Dialog (PCEscWindow pWindow, PCNPREffectsList pAllEffects, PCImage pTest, 
      PCRenderSuper pRender, PCWorldSocket pWorld);
   virtual BOOL IsPainterly (void);

   CNPREffectSpyglass * CloneEffect (void);

   // variables
   DWORD          m_dwRenderShard;  // where rendering to
   fp             m_fScale;         // 1.0 = use as much space as possible
   fp             m_fBlur;          // 1.0 = very blurry, 0 = not
   fp             m_fStickOut;      // objects closer than this stick out
   DWORD          m_dwShape;        // shape of spyglass
   COLORREF       m_cSpyglassColor;   // Spyglass color

private:
   BOOL RenderAny (PCImage pImage, PCFImage pFImage);

};
typedef CNPREffectSpyglass *PCNPREffectSpyglass;

extern PWSTR gpszEffectSpyglass;





/*********************************************************************************
CNPREffectRelief */

class DLLEXPORT CNPREffectRelief : public CNPREffect, public CEscMultiThreaded {
public:
   ESCNEWDELETE;

   CNPREffectRelief (DWORD dwRenderShard);
   ~CNPREffectRelief (void);

   virtual void Delete (void);
   virtual void QueryInfo (PNPRQI pqi);
   virtual PCMMLNode2 MMLTo (void);
   virtual BOOL MMLFrom (PCMMLNode2 pNode);
   virtual CNPREffect *Clone (void);
   virtual BOOL Render (PCImage pOrig, PCImage pDest, PCRenderSuper pRender,
                        PCWorldSocket pWorld, BOOL fFinal, PCProgressSocket pProgress);
   virtual BOOL Render (PCFImage pOrig, PCFImage pDest, PCRenderSuper pRender,
                        PCWorldSocket pWorld, BOOL fFinal, PCProgressSocket pProgress);
   virtual BOOL Dialog (PCEscWindow pWindow, PCNPREffectsList pAllEffects, PCImage pTest, 
      PCRenderSuper pRender, PCWorldSocket pWorld);
   virtual BOOL IsPainterly (void);


   // CEscMultiThreaded
   virtual void EscMultiThreadedCallback (PVOID pParams, DWORD dwParamSize,
      DWORD dwThread);

   CNPREffectRelief * CloneEffect (void);

   // variables
   DWORD          m_dwRenderShard;  // where rendering to
   fp             m_fWidth;            // width in meters of image
   fp             m_fDirection;        // direction of light
   fp             m_fIncidence;        // light angle of incidence
   fp             m_fAmbient;          // amount of ambient light
   fp             m_fSpec;             // specular brightness, 0 to 100
   fp             m_fTransDist;        // transparency distance
   BOOL           m_fTransparent;      // set to TRUE if acts transparent
   PCObjectSurface m_pSurf;              // surface
   BOOL           m_fIgnoreBackground; // checked if ignore the background

private:
   BOOL RenderAny (PCImage pImageSrc, PCFImage pFImageSrc,
                                  PCImage pImageDest, PCFImage pFImageDest,
                                  PCProgressSocket pProgress);

};
typedef CNPREffectRelief *PCNPREffectRelief;

extern PWSTR gpszEffectRelief;




/*********************************************************************************
CNPREffectPainting */
class CFieldDetector;
typedef CFieldDetector *PCFieldDetector;

class CPaintingBrush;
typedef CPaintingBrush *PCPaintingBrush;
class CPaintingStroke;
typedef CPaintingStroke *PCPaintingStroke;


/* CPaintingStroke - C++ object for drawing a stroke, and maintaining brushes, etc. */
class CPaintingStroke {
public:
   CPaintingStroke (void);
   ~CPaintingStroke (void);

   BOOL Init (DWORD dwType, DWORD dwNoiseRes, PCPoint pParam);

   BOOL Paint (POINT *paSeg, DWORD *padwSize, DWORD dwNum,
                              DWORD dwCurve, PCImage pImage, PCFImage pFImage,
                              WORD *pawColor);

private:
   BOOL InitStrokeMem (DWORD dwWidth, DWORD dwHeight);   // initializes the memory for storing the stroke
   void PaintSegment (POINT *pStart, POINT *pEnd, DWORD dwSizeStart, DWORD dwSizeEnd, BOOL fIncludeLast);
   PCPaintingBrush GetBrush (DWORD dwSize);
   BOOL PaintSegments (POINT *paSeg, DWORD *padwSize, DWORD dwNum, POINT *pOffset);
   BOOL PaintCurvedSegments (POINT *paSeg, DWORD *padwSize, DWORD dwNum,
                                           DWORD dwCurve, POINT *pOffset);

   DWORD             m_dwType;            // type of brush
   CNoise2D          m_Noise;             // noise
   CPoint            m_pParam;            // passed into the brush creation
   CListFixed        m_lPCPaintingBrush;  // list of paint brushes at each size

   CMem              m_memStroke;         // memory to store the stroke
   DWORD             m_dwWidth;           // stroke width
   DWORD             m_dwHeight;          // stroke height
   DWORD             *m_padwStrokeMinMax;  // stroke min/max mem
   PBYTE             m_pbStroke;          // stroke memory

   CMem              m_memCurve;          // temporary memory for curve
};
typedef CPaintingStroke *PCPaintingStroke;



class DLLEXPORT CNPREffectPainting : public CNPREffect, public CEscMultiThreaded {
public:
   ESCNEWDELETE;

   CNPREffectPainting (DWORD dwRenderShard);
   ~CNPREffectPainting (void);

   virtual void Delete (void);
   virtual void QueryInfo (PNPRQI pqi);
   virtual PCMMLNode2 MMLTo (void);
   virtual BOOL MMLFrom (PCMMLNode2 pNode);
   virtual CNPREffect *Clone (void);
   virtual BOOL Render (PCImage pOrig, PCImage pDest, PCRenderSuper pRender,
                        PCWorldSocket pWorld, BOOL fFinal, PCProgressSocket pProgress);
   virtual BOOL Render (PCFImage pOrig, PCFImage pDest, PCRenderSuper pRender,
                        PCWorldSocket pWorld, BOOL fFinal, PCProgressSocket pProgress);
   virtual BOOL Dialog (PCEscWindow pWindow, PCNPREffectsList pAllEffects, PCImage pTest, 
      PCRenderSuper pRender, PCWorldSocket pWorld);
   virtual BOOL IsPainterly (void);

   // CEscMultiThreaded
   virtual void EscMultiThreadedCallback (PVOID pParams, DWORD dwParamSize,
      DWORD dwThread);

   CNPREffectPainting * CloneEffect (void);

   // variables
   DWORD          m_dwRenderShard;  // where rendering to
   BOOL           m_fIgnoreBackground; // checked if ignore the background

   // background
   DWORD          m_dwTextBack;        // background mode, 0 for none, 1 for solid,
                                       // 2 for top to bottom, 3 left to right, 4 texture
   COLORREF       m_acTextColor[2];    // texture color for top/left and bottom/right
   fp             m_fTextWidth;        // width of texture in meters
   PCObjectSurface m_pTextSurf;        // texture surface

   // strokes
   DWORD          m_dwStrokesNum;      // number of strokes
   fp             m_fStrokeMomentum;   // momentum scale, 0..1
   fp             m_fStrokeRandom;     // random amount, 0..1
   fp             m_fStrokeZWeight;    // weighting for Z, 0..1
   fp             m_fStrokeColorWeight;// weighting for color, 0..1
   fp             m_fStrokeObjectWeight;// weighting for object, 0..1
   fp             m_fStrokePrefWeight; // preferred angle weight, 0..1
   fp             m_fStrokePrefAngle;  // preferred angle, radians
   fp             m_fStrokeZBlur;      // blur size, in percent width
   fp             m_fStrokeColorBlur;  // blur size, in percent width
   fp             m_fStrokeObjectBlur; // blur size, in percent width
   BOOL           m_fStrokeZPerp;      // if TRUE go perpendicular to gradient
   BOOL           m_fStrokeColorPerp;  // if TRUE go perpendicular to gradient
   BOOL           m_fStrokeObjectPerp; // if TRUE go perpendicular to gradient
   BOOL           m_fStrokePrefEither; // check to indicate preferred angle can go at angle or +180 degrees to
   BOOL           m_fStrokeCrosshatch; // half strokes perp to normal
   fp             m_fStrokeLen;        // typical stroke length as a percent of width
   fp             m_fStrokeStep;       // distance of a stroke step, as a percent of width
   fp             m_fStrokeAnchor;     // where stroke picks up anchor (0 = at start, 1=at end)
   fp             m_fStrokeLenVar;     // variability of stroke length (0 = none, 1= max)
   fp             m_fStrokePenColor;   // penalty for change in color (0 = none, 1=max)
   fp             m_fStrokePenObject;  // penalty for crossing over object (0 = none, 1=max)
   CPoint         m_pStrokeWidth;      // width of stroke (as percent of screen) at start=[0],
                                       // anchor [1], and end[2]. [3] is variation from 0 to 1.
   CPoint         m_pColorVar;         // color varion. [0] = h, [1] = l, [2] = s. From 0..1
   CPoint         m_pColorPalette;     // # colors in pallette
   fp             m_fColorHueShift;    // how much to shift color hue
   BOOL           m_fColorUseFixed;    // if TRUE used the fixed palette
   COLORREF       m_acColorFixed[5];   // colors in fixed palette
   DWORD          m_dwBrushShape;      // 0 for circular, 1 for horizonal, 2 for vertical,
                                       // 3 diagonal 1, 4 diagonal 2
   CPoint         m_pBrushParam;       // paramters passed to brush init. 0 = low-freq noise,
                                       // 1 = high freq noise, 2 = contrast, 3 = scale
   fp             m_fDeltaFilterWidth; // width of filter (in percent of width) for stroke elimination by deltas
   fp             m_fBackMatch;        // 0..1, amount of diffence between background and color allowed
   COLORREF       m_cBackColor;        // background color for eliminating storkes
   fp             m_fSpyglassScale;         // 1.0 = use as much space as possible
   fp             m_fSpyglassBlur;          // 1.0 = very blurry, 0 = not
   fp             m_fSpyglassStickOut;      // objects closer than this stick out
   DWORD          m_dwSpyglassShape;        // shape of spyglass, 0 for off

private:
   BOOL RenderAny (PCImage pImageSrc, PCFImage pFImageSrc,
                                  PCImage pImageDest, PCFImage pFImageDest,
                                  PCProgressSocket pProgress);
   BOOL ClearBackground (PCImage pImageDest, PCFImage pFImageDest);
   BOOL PathCalc (DWORD dwX, DWORD dwY, PCImage pImage,
                  PCFImage pFImage, PCListFixed plPath, DWORD *pdwAnchor);
   void DirectionCalc (DWORD dwX, DWORD dwY, PCImage pImage,
                                        PCFImage pFImage, BOOL fBackwards,
                                        BOOL fCrosshatch, PTEXTUREPOINT ptpDir);
   BOOL Stroke (DWORD dwThread, DWORD dwX, DWORD dwY, PCImage pImageSrc, PCFImage pFImageSrc,
                                 PCImage pImageDest, PCFImage pFImageDest,
                                 PCPaintingStroke pStroke);
   void Color (WORD *pawColor);

   CMem           m_memStrokes;        // memory to store strokes

   // used for paths
   PCFieldDetector m_pFieldColor;      // field used for color
   PCFieldDetector m_pFieldZ;          // field used for Z
   PCFieldDetector m_pFieldObject;     // field used for objects
   TEXTUREPOINT   m_tpPrefAngle;       // preferred angle converted to sin and cos
   CListFixed     m_alPathPOINT[MAXRAYTHREAD];        // list of POINT for path
   CListFixed     m_alPathDWORD[MAXRAYTHREAD];        // list of DWORD for path size
};
typedef CNPREffectPainting *PCNPREffectPainting;

extern PWSTR gpszEffectPainting;


/*********************************************************************************
CNPREffectOutlineSketch */
class DLLEXPORT CNPREffectOutlineSketch : public CNPREffect, public CEscMultiThreaded {
public:
   ESCNEWDELETE;

   CNPREffectOutlineSketch (DWORD dwRenderShard);
   ~CNPREffectOutlineSketch (void);

   virtual void Delete (void);
   virtual void QueryInfo (PNPRQI pqi);
   virtual PCMMLNode2 MMLTo (void);
   virtual BOOL MMLFrom (PCMMLNode2 pNode);
   virtual CNPREffect *Clone (void);
   virtual BOOL Render (PCImage pOrig, PCImage pDest, PCRenderSuper pRender,
                        PCWorldSocket pWorld, BOOL fFinal, PCProgressSocket pProgress);
   virtual BOOL Render (PCFImage pOrig, PCFImage pDest, PCRenderSuper pRender,
                        PCWorldSocket pWorld, BOOL fFinal, PCProgressSocket pProgress);
   virtual BOOL Dialog (PCEscWindow pWindow, PCNPREffectsList pAllEffects, PCImage pTest, 
      PCRenderSuper pRender, PCWorldSocket pWorld);
   virtual BOOL IsPainterly (void);

   // CEscMultiThreaded
   virtual void EscMultiThreadedCallback (PVOID pParams, DWORD dwParamSize,
      DWORD dwThread);

   CNPREffectOutlineSketch * CloneEffect (void);

   // strokes
   DWORD          m_dwRenderShard;  // where rendering to
   fp             m_fStrokeRandom;     // random amount, 0..1
   fp             m_fStrokeLen;        // typical stroke length as a percent of width
   fp             m_fStrokeStep;       // distance of a stroke step, as a percent of width
   fp             m_fStrokeAnchor;     // where stroke picks up anchor (0 = at start, 1=at end)
   fp             m_fStrokeLenVar;     // variability of stroke length (0 = none, 1= max)
   fp             m_fStrokePenColor;   // penalty for change in color (0 = none, 1=max)
   fp             m_fStrokePenObject;  // penalty for crossing over object (0 = none, 1=max)
   CPoint         m_pStrokeWidth;      // width of stroke (as percent of screen) at start=[0],
                                       // anchor [1], and end[2]. [3] is variation from 0 to 1.
   CPoint         m_pColorVar;         // color varion. [0] = h, [1] = l, [2] = s. From 0..1
   CPoint         m_pColorPalette;     // # colors in pallette
   fp             m_fColorHueShift;    // how much to shift color hue
   BOOL           m_fColorUseFixed;    // if TRUE used the fixed palette
   COLORREF       m_acColorFixed[5];   // colors in fixed palette
   DWORD          m_dwBrushShape;      // 0 for circular, 1 for horizonal, 2 for vertical,
                                       // 3 diagonal 1, 4 diagonal 2
   CPoint         m_pBrushParam;       // paramters passed to brush init. 0 = low-freq noise,
                                       // 1 = high freq noise, 2 = contrast, 3 = scale
   fp             m_fSpyglassScale;         // 1.0 = use as much space as possible
   fp             m_fSpyglassBlur;          // 1.0 = very blurry, 0 = not
   fp             m_fSpyglassStickOut;      // objects closer than this stick out
   DWORD          m_dwSpyglassShape;        // shape of spyglass, 0 for off

   DWORD          m_dwLevel;           // whether to show primary, secondary, or tertiary lines
   fp             m_fStrokeLenMin;     // minimum stroke length as a percent of width
   fp             m_fOvershoot;        // overshoot as a value from 0..1
   fp             m_fMaxAngle;         // maximum angle within a line, from 0.. 1
   fp             m_fStrokePrefAngle;  // preferred angle, radians

   COLORREF       m_cOutlineColor;     // outline color
   fp             m_fOutlineBlend;     // how much to blend in object color, 0..1
private:
   BOOL RenderAny (PCImage pImageSrc, PCFImage pFImageSrc,
                                  PCImage pImageDest, PCFImage pFImageDest,
                                  PCProgressSocket pProgress);
   BOOL Stroke (DWORD dwThread, DWORD dwX, DWORD dwY, PCImage pImageSrc, PCFImage pFImageSrc,
                                 PCImage pImageDest, PCFImage pFImageDest,
                                 PBYTE pbOutline, PCPaintingStroke pStroke);
   void Color (WORD *pawColor);
   BOOL TableOutlines (DWORD dwLevel, PCImage pImage, PCFImage pFImage,
                                             PCMem pMem, PCListFixed plPrimary,
                                             PCListFixed plSecondary, PCListFixed plTertiary);
   PCListFixed GeneratePaths (PBYTE pbOutline, DWORD dwWidth, DWORD dwHeight,
                               BOOL f360, BYTE bLevel, DWORD dwX, DWORD dwY);
   PCListFixed GenerateBestPathSeg (PBYTE pbOutline, DWORD dwWidth, DWORD dwHeight,
                               BOOL f360, BYTE bLevel, DWORD dwX, DWORD dwY,
                               fp fAngleLast, fp *pfAngle);
   PCListFixed GenerateBestPath (PBYTE pbOutline, DWORD dwWidth, DWORD dwHeight,
                               BOOL f360, BYTE bLevel, DWORD dwX, DWORD dwY,
                               DWORD dwLen);

   // CMem           m_memStrokes;        // memory to store strokes

   // used for paths
   // CListFixed     m_alPathPOINT[MAXRAYTHREAD];        // list of POINT for path
   CListFixed     m_alPathDWORD[MAXRAYTHREAD];        // list of DWORD for path size
};
typedef CNPREffectOutlineSketch *PCNPREffectOutlineSketch;

extern PWSTR gpszEffectOutlineSketch;


/*********************************************************************************
CNPREffectsList */
class DLLEXPORT CNPREffectsList {
public:
   ESCNEWDELETE;

   CNPREffectsList (DWORD dwRenderShard);
   ~CNPREffectsList (void);

   CNPREffectsList *Clone (void);
   PCMMLNode2 MMLTo (void);
   BOOL MMLFrom (PCMMLNode2 pNode);

   BOOL Render (PCImage pImage, PCRenderSuper pRender,
               PCWorldSocket pWorld, BOOL fFinal, PCProgressSocket pProgress);
   BOOL Render (PCFImage pImage, PCRenderSuper pRender,
               PCWorldSocket pWorld, BOOL fFinal, PCProgressSocket pProgress);
   BOOL Dialog (PCEscWindow pWindow, PCImage pTest,
      PCRenderSuper pRender, PCWorldSocket pWorld);
   BOOL IsPainterly (void);

   // look but dont touch
   DWORD             m_dwRenderShard;     // render shard to use
   CListFixed        m_lPCNPREffect;      // list of PCNPREffect, item 0 is first effect run
private:
};
typedef CNPREffectsList *PCNPREffectsList;

HBITMAP EffectImageToBitmap (PCImage pImage, PCNPREffectsList pEffectsList,
                             PCNPREffect pEffect, PCRenderSuper pRender,
                             PCWorldSocket pWorld);

/***********************************************************************************
Effects */
DLLEXPORT void EffectEnumMajor (DWORD dwRenderShard, PCListFixed pl);
DLLEXPORT void EffectEnumMinor (DWORD dwRenderShard, PCListFixed pl, PWSTR pszMajor);
DLLEXPORT void EffectEnumItems (DWORD dwRenderShard, PCListFixed pl, PWSTR pszMajor, PWSTR pszMinor);
DLLEXPORT BOOL EffectGUIDsFromName (DWORD dwRenderShard, PWSTR pszMajor, PWSTR pszMinor, PWSTR pszName, GUID *pgCode, GUID *pgSub);
DLLEXPORT BOOL EffectNameFromGUIDs (DWORD dwRenderShard, GUID *pgCode, GUID *pgSub,PWSTR pszMajor, PWSTR pszMinor, PWSTR pszName);
DLLEXPORT BOOL EffectLibraryDialog (DWORD dwRenderShard, HWND hWnd, PCImage pImage, PCRenderSuper pRender,
                          PCWorldSocket pWorld);
DLLEXPORT PCNPREffectsList EffectCreate (DWORD dwRenderShard, const GUID *pCode, const GUID *pSub);
DLLEXPORT BOOL EffectSelDialog (DWORD dwRenderShard, HWND hWnd, GUID *pgEffectCode, GUID *pgEffectSub);
DLLEXPORT HBITMAP EffectGetThumbnail (DWORD dwRenderShard, GUID *pgCode, GUID *pgSub, HWND hWnd, COLORREF *pcTransparent = NULL,
                            BOOL fBlankIfFail = FALSE);



#endif // _M3D_H_
