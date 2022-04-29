/***********************************************************************
Main.cpp - Main routines for OilPain.cpp. Code to help people make
oil paintings

  Begun 9/29/99
  Copyright 1999-2000 Mike Rozak. All rights reserved
*/

#include <windows.h>
#include <objbase.h>
#include <initguid.h>
#include <commctrl.h>
#include <stdio.h>
#include <math.h>
#include "escarpment.h"
#include "resource.h"
#include "oilpaint.h"
#include "buildnum.h"
#include "register.h"
#include "buildnum.h"
#include <crtdbg.h>

#define DEBUGPRINTING      // BUGBUG


typedef struct {
   char     *pszColor;  // color name
   COLORREF rgb;        // rgb color
} CATCOLOR, *PCATCOLOR;

typedef struct {
   char     szName[256];
   WORD     r, g, b;       // red green blue
   int     x, y, z;       // xyz color space
} PCOLOR, *PPCOLOR;

typedef struct {
   DWORD       dwMagicNumber;
   DWORD       dwState;
   char        szFileName[256];  // file
   BOOL        fInches;    // if true measurement in inches, else CM
   double      fWidth;    // width in inches
   double      fHeight;   // height in inches
   double      fContrast; // contrast
   double      fBrightness;  // brightness
   double      fColorized;   // colorized
   DWORD       dwMix;     // 0 => no mixing, 1 => mix colors in halves, 2 => mix colors with black & white, 
   double      fGrid;    // grid size, in inches/cm. Or if negative 1/(-iGrid) of height/width
   int         iViewCanvasLeft, iViewCanvasTop;   // left/top of canvas, in pixels on original image
   int         iViewCanvasWidth;   // width of canvas in image pixels
   COLORREF    cBaseBlack;    // definition of black
   DWORD       dwBaseDetail;  // amount of detail
   double      fOutlineStrength; // outline strength
} IMAGEDATA, *PIMAGEDATA;

typedef struct {
   char        szName[64]; // color name
   BOOL        fShow;      // if TRUE, show it. FALSE, don't show
   COLORREF    rgb;        // RGB color
   WORD        awRGB[3];   // gamma-corrcted, word values
   int         aiXYZ[3];   // XYZ location
   double      afXYZ[3];   // XYZ location as double
} SPACEPAINT, *PSPACEPAINT;

#define MAGICNUMBER     348356
#define  ESCM_STOREAWAY    (ESCM_USER+86)
#define  ESCM_ADJUSTHEIGHT (ESCM_USER+99)


void ColorsToImage (float fOutlineStrength);

/******************************************************************************
globals
*/
static CListFixed gListSPACEPAINT;
HINSTANCE   ghInstance;
HWND        ghWndMain = NULL;
char        gszAppDir[256];   // application directory
char        gszAppPath[256];  // application name
//char        gszHelp[256];     // help file
char        *gszCmdLine;      // command line

char        gszMainClass[] = "OilPaintMain";
char        gszSample[] = "***Sample";
char        gszRegKey[] = "Software\\mXac\\OilPaint";
char        gszRegPaintKey[] = "Software\\mXac\\OilPaint\\Paints";
char        gszVersion[] = "Version";

// for the color space analysis
DWORD       gadwColorCount[16][8][4];  // count of colors from image
BOOL        gfSpaceShowPaints = TRUE;
BOOL        gfSpaceShowPaintsSolid = FALSE;
BOOL        gfSpaceShowPaintsShading = FALSE;
BOOL        gfSpaceShowCoverage = FALSE;
BOOL        gfSpaceShowCoverageSolid = FALSE;
BOOL        gfSpaceShowCoverageShading = FALSE;
BOOL        gfSpaceShowCoverageThird = FALSE;
BOOL        gfSpaceShowImage = FALSE;
BOOL        gfSpaceShowImageSolid = FALSE;
BOOL        gfSpaceShowImageShading = FALSE;

BOOL        gfKnowGridArt = FALSE;     // set to TRUE if know its gridart


CMem        gMemTemp;

PCOLOR      *gpPColor = NULL; // pallete colors
DWORD       gdwPColorSize;    // number of elements
DWORD       gdwRegCount;      // current registration count
PCEscPage   gpPagePaints;     // to communicate with paint adding dialog
DWORD       gdwDisplayScale = 100;  // for displaying sketch and paint base
DWORD       gdwSketchPageExit;   // page to claim on when exist sketchpage

// image data
DWORD       gdwState;          // one of the view mode settings. 0 if shouldn't skip
char        gszFileName[256] = "";  // file
BOOL        gfInches = TRUE;  // if TRUE measurement in inches, else CM
double      gfWidth = 20;    // width in inches
double      gfHeight = 20;   // height in inches
double      gfContrast = 1.0; // contrast
double      gfOutlineStrength = 0.25; // outline strength
double      gfBrightness = 1.0;  // brightness
double      gfColorized = 1.0;   // colorized
DWORD       gdwMix = 2;     // 0 => no mixing, 1 => mix colors in halves, 2 => mix colors with black & white, 3 => bypass, 4=>mix 2 colors w bnw, 5=>mix 3 colors
double      gfGrid = 2;    // grid size, in inches
COLORREF    gcBaseBlack = 0;    // definition of black
DWORD       gdwBaseDetail = 3;  // amount of detail
int         giViewCanvasLeft, giViewCanvasTop;   // left/top of canvas, in pixels on original image
int         giViewCanvasWidth;   // width of canvas in image pixels
CImage      gImageMain;       // main image
CImage      gImageCutout;     // cutout to fit in the canvas
CImage      gImageColors;     // image made out of base colors
WORD        *gawImageColor = NULL;   // pointer to a 2-d array (size of gImageCutOut) with color numbers for closest fit

static double afContrast[5] = {0.5, 1.0 / 1.5, 1.0, 1.5, 2.0};
static double afColorize[5] = {0.0, 0.5, 1.0, 1.5, 2.0};

// globals used to interact with view window
HBITMAP     ghBitView = NULL; // view bitmap
HBITMAP     ghBitWhitened = NULL;   // whitened bitmap
DWORD       gdwBitViewX, gdwBitViewY;
DWORD       gdwViewMode;
#define  VIEWMODE_INTRO                   1
#define  VIEWMODE_CANVASSIZE              2
#define  VIEWMODE_CANVASLOCATION          3
#define  VIEWMODE_COLORIZE                4
#define  VIEWMODE_PAINTS                  5
#define  VIEWMODE_GRID                    6
#define  VIEWMODE_PRINT                   7
#define  VIEWMODE_SKETCH                  8
#define  VIEWMODE_PAINTBASE               9
#define  VIEWMODE_PAINTDETAILS            10
#define  VIEWMODE_QUIT                    11

PCEscWindow gpWindow;            // main window object

// globals used within the view window
double      gfScale = 1.0;
int         giLeftX, giTopY;  // location of upper/left corner of bitmap relative to client area
                              // scale already taken into account
int         giViewRet = 0;    // return info
DWORD       gdwHitTest = 0;   // hit test
POINT       gpDelta;          // delta for dragging
int         giOrigLeft, giOrigTop, giOrigWidth;    // of selection

WCHAR gszNext[] = L"Next";
WCHAR gszBack[] = L"Back";
WCHAR gszSamePage[] = L"SamePage";
WCHAR gszExit[] = L"[Close]";
WCHAR gszHome[] = L"Home";
WCHAR gszChecked[] = L"Checked";
WCHAR gszCurSel[] = L"CurSel";
WCHAR gszPallette[] = L"pallette";

CATCOLOR    gaCatColor[] = {
   "Alizarin crimson", RGB(104,15,21),
   "Archival crimson", RGB(231,11,1),
   "Arylamide yellow", RGB(255,203,0),
   "Arylamide yellow deep", RGB(251,145,0),
   "Aureolin", RGB(254,200,0),
   "Australian green", RGB(180,184,0),
   "Australian leaf green", RGB(64,163,124),
   "Australian grey", RGB(248,229,203),
   "Australian red gold", RGB(187,9,4),
   "Black", RGB(0x0,0x0,0x0),
   "Brilliant blue", RGB(0,60,180),
   "Brilliant magenta", RGB(211,53,136),
   "Brilliant orange", RGB(241,111,0),
   "Brilliant violet", RGB(115,104,166),
   "Brown earth", RGB(99,52,49),
   "Brown pink", RGB(98,1,19),
   "Buff titanium", RGB(231,215,170),
   "Burnt orange quinacridone", RGB(189,6,0),
   "Burnt sienna", RGB(121,16,34),
   "Burt umber", RGB(63,4,33),
   "Cadmium green", RGB(111,182,30),
   "Cadmium maroon", RGB(110,20,40),
   "Cadmium orange", RGB(233,47,0),
   "Cadmium red", RGB(217,13,1),
   "Cadmium red deep", RGB(178,9,16),
   "Cadmium red light", RGB(230,48,4),
   "Cadmium red medium", RGB(151,13,17),
   "Cadmium red mid", RGB(214,1,0),
   "Cadmium scarlet", RGB(223,15,1),
   "Cadmium yellow", RGB(255,212,0),
   "Cadmium yellow deep", RGB(249,146,0),
   "Cadmium yellow light", RGB(255, 238, 0),
   "Cadmium yellow medium", RGB(255,187,0),
   "Cadmium yellow mid", RGB(255,197,0),
   "Cadmium yellow pale", RGB(247,235,9),
   "Carmine", RGB(219,0,73),
   "Cerulean Blue",0x704f01,
   //scanned - "Cerulean Blue",RGB(0,106,179),
   "Chrome lemon", RGB(246,224,0),
   "Chrome orange", RGB(230,178,0),
   "Chrome orange deep", RGB(233,73,0),
   "Chromium green oxide", RGB(83,129,81),
   "Cobalt blue", RGB(0, 3, 138),
   "Cobalt blue deep", RGB(19,0,119),
   "Cobalt green", RGB(91,186,163),
   "Cobalt turquoise", RGB(55,158,165),
   "Cobalt violet", RGB(53,0,102),
   "Cobalt violet dark", RGB(59,2,108),
   "Coeruleum", RGB(0,150,212),
   "Coral", RGB(220, 11, 0),
   "Crimson alizarin", RGB(218,6,41),
   "Dioxazine purple", RGB(59,56,104),
   "Dioxazine violet", RGB(55,22,103),
   "Emerald green", RGB(33,145,101),
   "Flesh tint", RGB(246,201,160),
   "Flesh tint deep", RGB(222, 170, 160),
   "Flinders blue violet", RGB(29,0,90),
   "Flinders red violet", RGB(92,11,41),
   "Forest green", RGB(33,50,49),
   "French ultramarine", RGB(0,0,123),
   "Gold", RGB(219,65,127),
   "Gold ochre", RGB(158,56,19),
   "Gold oxide", RGB(174,118,27),
   "Golden yellow", RGB(254,181,0),
   "Hooker's green", RGB(40,53,46),
   "Indian red", RGB(135,8,31),
   "Indian red oxide", RGB(135,64,43),
   "Indian yellow", RGB(255,95,0),
   "Indigo blue", RGB(21,16,42),
   "Italian pink", RGB(195,45,22),
   "Jaune brilliant", RGB(251,205,75),
   "Lemon Yellow", 0x11dbff,
   // scanned - "Lemon yellow", RGB(255,242, 0),
   "Light red", RGB(150,25,22),
   "Light red oxide", RGB(152,89,63),
   "Lilac", RGB(158,135,200),
   "Magnesium blue hue", RGB(0, 91, 164),
   "Mars violet", RGB(88,20,44),
   "Mica gold", RGB(172,138,38),
   "Mineral violet", RGB(59,2,92),
   "Naples yellow", RGB(251, 233,131),
   "Naples yellow reddish", RGB(248,156,86),
   "Napthol crimson", RGB(220,0,53),
   "Napthol scarlet", RGB(197,50,17),
   "Olive green", RGB(19,10,22),
   "Oxide of chromium", RGB(76,129,59),
   "Pacific blue", RGB(95,119,188),
   "Paint-by-Grid's Burnt Sienna", RGB(208,83,53),
   "Paint-by-Grid's Cobalt Blue", RGB(43,83, 179),
   "Paint-by-Grid's Crimson Red", RGB(136,9,28),
   "Paint-by-Grid's Lamp Black", RGB(0x0,0x0,0x0),
   "Paint-by-Grid's Lemon Yellow", RGB(253,230,102),
   "Paint-by-Grid's Light Green", RGB(64, 155, 27),
   "Paint-by-Grid's Phthalocyanine Blue", RGB(36,44,109),
   "Paint-by-Grid's Raw Umber", RGB(135,82,50),
   "Paint-by-Grid's Titanium White", RGB(255,255,255),
   "Paint-by-Grid's Vermillion", RGB(238,49,47),
   "Paint-by-Grid's Viridian", RGB(24,111,128),
   "Paint-by-Grid's Yellow Ochre", RGB(249,167,32),
   "Payne's grey", RGB(23,21,44),
   "Pearl white", RGB(255,253,217),
   "Permanent alizarine", RGB(169,67,73),
   "Permanent brown madder", RGB(152,58,46),
   "Permanent crimson", RGB(155,21,16),
   "Permanent blue", RGB(0,131,200),
   "Permanent geranium", RGB(221,14,66),
   "Permanent green light", RGB(117,182,83),
   "Permanent light green", RGB(156,187,59),
   "Permanent mauve", RGB(11,0,92),
   "Permanent magenta", RGB(103,1,17),
   "Permanent rose", RGB(167,0,17),
   "Permanent sap green", RGB(44,75,55),
   "Permanent vandyke brown", RGB(34,8,34),
   "Permanent viridian hue", RGB(49,91,66),
   "Pilbara red", RGB(140,15,28),
   "Phthalo blue", RGB(0, 0, 117),
   "Phthalo green", RGB(40,156,116),
   "Phthalo green yellow shade", RGB(40,164,41),
   "Phthalo turquoise", RGB(29,126,143),
   "Prussian blue", RGB(9,0,45),
   "Prussian green", RGB(38,156,124),
   "Purple madder", RGB(118,52,46),
   "Quinacridone red violet", RGB(105,56,52),
   "Quinacridone magenta", RGB(140,57,83),
   "Raw sienna", RGB(178,74,21),
   "Raw umber", RGB(49,25,48),
   "Red gold", RGB(199,104,26),
   "Rose dore", RGB(212,38,20),
   "Rose madder", RGB(180,1,3),
   "Royal blue", RGB(19,51,121),
   "Sap green", RGB(4,35,39),
   "Sapphire", RGB(12,32,67),
   "Scarlet lake", RGB(225,9,49),
   "Silver", RGB(223,220,206),
   "Sky blue", RGB(0,124,184),
   "Spectrum cerulean", RGB(0,94,179),
   "Spectrum crimson", RGB(142,3,12),
   "Spectrum blue", RGB(0,0,137),
   "Spectrum green light", RGB(189,207,0),
   "Spectrum red", 0x0b00ce,
   // scanned "Spectrum red", RGB(210,0,0),
   "Spectrum red deep", RGB(185,0,4),
   "Spectrum orange", RGB(239,70,0),
   "Spectrum vermilion", RGB(218,1,0),
   "Spectrum violet", RGB(69,0,65),
   "Spectrum yellow", RGB(255,227,0),
   "Superchrome scarlet", RGB(229,103,20),
   "Superchrome yellow", RGB(255,203,0),
   "Superchrome yellow light", RGB(251,179,0),
   "Superchrome yellow medium", RGB(254,161,0),
   "Superchrome yellow mid", RGB(245,134,0),
   "Tasman blue", RGB(1,111,184),
   "Terre verte", 0x546d10,
   "Terre verte traditional", RGB(74,112,52),
   "Transparent red oxide", RGB(229,88,0),
   "Transparent gold oxide", RGB(171,54,7),
   "Turquoise", 0x898303,
   // scanned - "Turquoise", RGB(12,161,146),
   "Ultramarine blue", RGB(1,0,127),
   "Vandyke Brown", 0x073d61,
   // scanned - "Vandyke brown", RGB(22,2,3),
   "Venetian red", RGB(177,72,25),
   "Vermilion", RGB(234,44,0),
   "Viridian", RGB(27,149,123),
   "Warm grey", RGB(121,133,128),
   "White", RGB(255,255,255),
   "Yellow green", RGB(224,219,1),
   "Yellow light hansa", RGB(250,244,40),
   "Yellow light", RGB(247,201,0),
   "Yellow mid", RGB(223,164,0),
   "Yellow ochre", RGB(210,141,27),
   "Yellow oxide", RGB(193,165,40),
};

BOOL LoadBitmapUI (PCEscPage pPage);
PWSTR ColorSpaceSubst (void);
void ColorSpaceImage (void);
void ToXYZ (WORD r, WORD g, WORD b, int *px, int *py, int *pz);
BOOL ControlImage (PCEscControl pControl, DWORD dwMessage, PVOID pParam);


/*****************************************************************************
MemCatColor - Concatenate a color as in #ff0034

inputs
   PCMem       pMem - mem to add to
   COLORREF    cr - color
*/
void MemCatColor (PCMem pMem, COLORREF cr)
{
   WCHAR szTemp[64];
   ColorToAttrib (szTemp, cr);
   MemCat (pMem, szTemp);
}

/*****************************************************************************
MemCatPoint - Concatenate a point, such as 54.0,342,34.3

inputs
   PCMem       pMem - mem to add to
   double      *p - Pointer to an array of 3 points
*/
void MemCatPoint (PCMem pMem, double *p)
{
   WCHAR szTemp[128];
   swprintf (szTemp, L"%.5g,%.5g,%.5g", p[0], p[1], p[2]);
   MemCat (pMem, szTemp);
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
   RegOpenKeyEx (HKEY_CURRENT_USER, gszRegKey, 0, KEY_READ, &hKey);
   DWORD dw,dwType;
   if (hKey) {
      dw = dwSize;
      RegQueryValueEx (hKey, "Directory", 0, &dwType, (BYTE*) psz, &dw);
      RegCloseKey (hKey);
   }
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
   RegCreateKeyEx (HKEY_CURRENT_USER, gszRegKey, 0, 0, REG_OPTION_NON_VOLATILE,
      KEY_READ | KEY_WRITE, NULL, &hKey, &dwDisp);
   if (hKey) {
      RegSetValueEx (hKey, "Directory", 0, REG_SZ, (BYTE*) psz, strlen(psz)+1);

      RegCloseKey (hKey);
   }
}


/*****************************************************************************
PalletteLoad - Reads in a pallette from a file

inputs
   PCEscPage pPage - bring up error message
returns
   BOOL - TRUE if loaded
*/
BOOL PalletteLoad (PCEscPage pPage)
{
   OPENFILENAME   ofn;
   char  szTemp[256];
   szTemp[0] = 0;
   memset (&ofn, 0, sizeof(ofn));
   
   // BUGFIX - Set directory
   char szInitial[256];
   GetLastDirectory(szInitial, sizeof(szInitial));
   ofn.lpstrInitialDir = szInitial;

   ofn.lStructSize = sizeof(ofn);
   ofn.hwndOwner = pPage->m_pWindow->m_hWnd;
   ofn.hInstance = ghInstance;
   ofn.lpstrFilter = "Oil Painting Pallette (*.opp)\0*.opp\0\0\0";
   ofn.lpstrFile = szTemp;
   ofn.nMaxFile = sizeof(szTemp);
   ofn.lpstrTitle = "Load a pallette";
   ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
   ofn.lpstrDefExt = "opp";
   // nFileExtension 

   if (!GetOpenFileName(&ofn))
      return TRUE;

   // BUGFIX - Save diretory
   strcpy (szInitial, ofn.lpstrFile);
   szInitial[ofn.nFileOffset] = 0;
   SetLastDirectory(szInitial);

   FILE *f;
   f = fopen (szTemp, "rt");

   if (!f) {
      pPage->MBWarning (L"The file can't be opened.");
      return FALSE;
   }

   if (IDYES != pPage->MBYesNo (L"This will throw out your current list of paints. Are you sure you want to load a new palette?")) {
      fclose (f);
      return FALSE;
   }

   // wipe out the registry entries
   HKEY  hKey = NULL;
   RegOpenKeyEx (HKEY_CURRENT_USER, gszRegKey, 0, KEY_READ, &hKey);
   DWORD dw;
   if (hKey) {
      RegDeleteKey (hKey, "Paints");
      RegCloseKey (hKey);
   }

   DWORD dwDisp;
   RegCreateKeyEx (HKEY_CURRENT_USER, gszRegPaintKey, 0, 0, REG_OPTION_NON_VOLATILE,
      KEY_READ | KEY_WRITE, NULL, &hKey, &dwDisp);
   if (!hKey) {
      // unexpected error
      fclose (f);
      return TRUE;
   }
   // read in the file and overwrite
   while (fgets(szTemp, sizeof(szTemp), f)) {
      char *pszTab;
      pszTab = strchr(szTemp, '\t');
      if (!pszTab)
         continue;

      *pszTab = 0;
      pszTab++;

      if (!szTemp[0])
         continue;

      dw = (DWORD) atoi(pszTab);
      RegSetValueEx (hKey, szTemp, 0, REG_DWORD, (BYTE*) &dw, sizeof(dw));
   }


   // close
   RegCloseKey (hKey);
   fclose (f);
   return TRUE;
}


/*****************************************************************************
PalletteSave - Reads all the colors from the registry and saves them to a file.

inputs
   PCEscPage pPage - bring up error message
returns
   BOOL - TRUE if saved
*/
BOOL PalletteSave (PCEscPage pPage)
{
   OPENFILENAME   ofn;
   char  szTemp[256];
   szTemp[0] = 0;
   memset (&ofn, 0, sizeof(ofn));

   // BUGFIX - Set directory
   char szInitial[256];
   GetLastDirectory(szInitial, sizeof(szInitial));

   ofn.lpstrInitialDir = szInitial;
   ofn.lStructSize = sizeof(ofn);
   ofn.hwndOwner = pPage->m_pWindow->m_hWnd;
   ofn.hInstance = ghInstance;
   ofn.lpstrFilter = "Oil Painting Pallette (*.opp)\0*.opp\0\0\0";
   ofn.lpstrFile = szTemp;
   ofn.nMaxFile = sizeof(szTemp);
   ofn.lpstrTitle = "Save the pallette";
   ofn.Flags = OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
   ofn.lpstrDefExt = "opp";
   // nFileExtension 
   if (!GetSaveFileName(&ofn))
      return FALSE;

   // BUGFIX - Save diretory
   strcpy (szInitial, ofn.lpstrFile);
   szInitial[ofn.nFileOffset] = 0;
   SetLastDirectory(szInitial);

   FILE *f;
   f = fopen(szTemp, "wt");
   if (!f) {
      pPage->MBWarning (L"The file could not be saved.", L"The file may be write protected.");
      return FALSE;
   }
   
   // enum and write
   HKEY  hKey = NULL;
   RegOpenKeyEx (HKEY_CURRENT_USER, gszRegPaintKey, 0, KEY_READ, &hKey);
   if (hKey) {
      // enumerate
      COLORREF dwRGB;
      DWORD dwNameSize, dwType, i, dwSize;
      char szName[64];
      for (i = 0; ; i++) {
         dwNameSize = sizeof(szName);
         dwSize = sizeof(dwRGB);
         if (RegEnumValue (hKey, i, szName, &dwNameSize, NULL, &dwType, (PBYTE) &dwRGB, &dwSize))
            break;


         fprintf (f, "%s\t%d\n", szName, (int) dwRGB);
      }

      RegCloseKey (hKey);
   }

   fclose (f);

   return TRUE;
}

/*****************************************************************************
DefPage - Default page callback. It handles standard operations
*/
BOOL DefPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   static BOOL fFirstTime = TRUE;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
      // speak the first time
         // play a chime
#define  NOTEBASE       62
#define  VOLUME         64
      if (fFirstTime) {

         // load tts in befor chime
         EscSpeakTTS ();

         ESCMIDIEVENT aChime[] = {
            {0, MIDIINSTRUMENT (0, 72+3)}, // flute
            {0, MIDINOTEON (0, NOTEBASE+0,VOLUME)},
            {300, MIDINOTEOFF (0, NOTEBASE+0)},
            {0, MIDINOTEON (0, NOTEBASE-1,VOLUME)},
            {300, MIDINOTEOFF (0, NOTEBASE-1)},
            {0, MIDINOTEON (0, NOTEBASE+0,VOLUME)},
            {300, MIDINOTEOFF (0, NOTEBASE+0)},
            {100, MIDINOTEON (0, NOTEBASE+6,VOLUME)},
            {200, MIDINOTEOFF (0, NOTEBASE+6)},
            {100, MIDINOTEON (0, NOTEBASE-6,VOLUME)},
            {750, MIDINOTEOFF (0, NOTEBASE-6)}
         };
         EscChime (aChime, sizeof(aChime) / sizeof(ESCMIDIEVENT));

         // speak
         EscSpeak (L"Welcome to Oil Painting Assistant.");
         fFirstTime = FALSE;

      }


      }
      return TRUE;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"OPTemplateEnd")) {
            p->pszSubString = L"<?Template resource=402?>";
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"OPTemplateNext")) {
            p->pszSubString = L"<?Template resource=401?>";
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"INCHESCM")) {
            p->pszSubString = gfInches ? L"inches" : L"centimeters";
            return TRUE;
         }
      }
      return FALSE;


   };


   return FALSE;
}


/*****************************************************************************
FirstPage
*/
BOOL FirstPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   static BOOL fFirstTime = TRUE;

   switch (dwMessage) {
   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS)pParam;

         if (!p->pControl || !p->pControl->m_pszName)
            break;

         if (!_wcsicmp(p->pControl->m_pszName, L"session")) {
            // bring up help file saying that only accept sessions
//            WinHelp (NULL, gszHelp, HELP_CONTEXT, IDH_LOADSESSION);
            OPENFILENAME   ofn;
            char  szTemp[256];
            szTemp[0] = 0;
            memset (&ofn, 0, sizeof(ofn));
            
            // BUGFIX - Set directory
            char szInitial[256];
            GetLastDirectory(szInitial, sizeof(szInitial));
            ofn.lpstrInitialDir = szInitial;

            ofn.lStructSize = sizeof(ofn);
            ofn.hwndOwner = pPage->m_pWindow->m_hWnd;
            ofn.hInstance = ghInstance;
            ofn.lpstrFilter = "Oil Painting Session (*.opa)\0*.opa\0\0\0";
            ofn.lpstrFile = szTemp;
            ofn.nMaxFile = sizeof(szTemp);
            ofn.lpstrTitle = "Open Oil Painting Session";
            ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
            ofn.lpstrDefExt = "opa";
            // nFileExtension 

            if (!GetOpenFileName(&ofn))
               return TRUE;

            // BUGFIX - Save diretory
            strcpy (szInitial, ofn.lpstrFile);
            szInitial[ofn.nFileOffset] = 0;
            SetLastDirectory(szInitial);

            // open it
            FILE  *f;
            IMAGEDATA   id;
            f = fopen (szTemp, "rb");
            if (!f) return TRUE;
            fread (&id, 1, sizeof(id), f);
            fclose (f);
            if (id.dwMagicNumber != MAGICNUMBER)
               return TRUE;

            // else have it
            gdwState = id.dwState;
            strcpy (gszFileName, id.szFileName);
            gfInches = id.fInches;
            gfWidth = id.fWidth;
            gfHeight = id.fHeight;
            gfContrast = id.fContrast;
            gfOutlineStrength = id.fOutlineStrength;
            gfBrightness = id.fBrightness;
            gfColorized = id.fColorized;
            gdwMix = id.dwMix;
            gfGrid = id.fGrid;
            gcBaseBlack = id.cBaseBlack;
            gdwBaseDetail = id.dwBaseDetail;
            giViewCanvasLeft = id.iViewCanvasLeft;
            giViewCanvasTop = id.iViewCanvasTop;
            giViewCanvasWidth = id.iViewCanvasWidth;

            if (gImageMain.FromBitmapFile (gszFileName))
               return TRUE;

            pPage->Exit (gszNext);
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"image")) {
            if (!LoadBitmapUI (pPage))
               return TRUE;
            gdwState = 0;
            pPage->Exit (gszNext);
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"sample")) {
            strcpy (gszFileName, gszAppDir);
            strcat (gszFileName, "\\sample.jpg");
            // load bitmap
            if (gImageMain.FromBitmapFile (gszFileName))
               return TRUE;
            gdwState = 0;
            pPage->Exit (gszNext);
         }
         return TRUE;

      }
      break;

   case ESCM_INITPAGE:
      {
         // if regcount > 50 then disable buttons
         if ((gdwRegCount > 50) && !RegisterIsRegistered()) {
            PCEscControl pControl;
            pControl = pPage->ControlFind (L"image");
            if (pControl)
               pControl->Enable(FALSE);
            pControl = pPage->ControlFind (L"session");
            if (pControl)
               pControl->Enable(FALSE);
            pControl = pPage->ControlFind (L"sample");
            if (pControl)
               pControl->Enable(FALSE);
         }
      }
      break;   // default

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"VERSION")) {
            MemZero (&gMemTemp);
            WCHAR szTemp[32];
            MultiByteToWideChar (CP_ACP, 0, BUILD_NUMBER, -1, szTemp, sizeof(szTemp)/2);
            MemCat (&gMemTemp, szTemp);

            p->pszSubString = (PWSTR) gMemTemp.p;
            return TRUE;
         }
      }
      return FALSE;


   };


   return DefPage (pPage, dwMessage, pParam);
}

/*****************************************************************************
PageDialog - Displays a page.

inputs
   DWORD             dwID - Resource ID
   PESCPAGECALLBACK  pCallback - Callback
   PVOID             pUserData - User data to pass to the page
returns
   PWSTR - Return string. This either points to a string in the window object,
   or it will be gszNext, gszBack, gszExit, gszHome.
*/
PWSTR PageDialog (DWORD dwID, PESCPAGECALLBACK pCallback = NULL,
                   PVOID pUserData = NULL)
{
   // display the window
   PWSTR pRet;

   pRet = gpWindow->PageDialog (ghInstance, dwID, pCallback ? pCallback : DefPage, pUserData);
   if (!pRet)
      return gszBack;   // error showing back so go back

   // special checks
   if (!_wcsicmp(pRet, gszExit))
      return gszExit;
   if (!_wcsicmp(pRet, gszBack))
      return gszBack;
   if (!_wcsicmp(pRet, gszNext))
      return gszNext;
   if (!_wcsicmp(pRet, gszHome))
      return gszHome;

   // if the return
   return pRet;
}


/****************************************************************************
AskSave - This is called if the user indicates quit. It asks them if they
   want to save their session, yes/no/cancel.

inputs
   HWND     hWnd- to pull message from
   DWORD    dwState - state. one of the viewmode defines
   BOOL     fSkipAsk - If TRUE, skip verifying
returns
   BOOL - TRUE if user wants to quit (data may be saved), FALSE if user pressed cancel
*/
BOOL AskSave (HWND hWnd, DWORD dwState, BOOL fSkipAsk = TRUE)
{
   int   iRet;
   if (fSkipAsk) {
      iRet = MessageBox (hWnd, "Do you want to save you current settings?", "Exit",
         MB_YESNOCANCEL);
      if (iRet == IDNO)
         return TRUE;
      if (iRet != IDYES)
         return FALSE;  // some sort of cancel
   }

   // save UI
   OPENFILENAME   ofn;
   char  szTemp[256];
   szTemp[0] = 0;
   memset (&ofn, 0, sizeof(ofn));

   // BUGFIX - Set directory
   char szInitial[256];
   GetLastDirectory(szInitial, sizeof(szInitial));

   ofn.lpstrInitialDir = szInitial;
   ofn.lStructSize = sizeof(ofn);
   ofn.hwndOwner = hWnd;
   ofn.hInstance = ghInstance;
   ofn.lpstrFilter = "Oil Painting Session (*.opa)\0*.opa\0\0\0";
   ofn.lpstrFile = szTemp;
   ofn.nMaxFile = sizeof(szTemp);
   ofn.lpstrTitle = "Save session";
   ofn.Flags = OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
   ofn.lpstrDefExt = "opa";
   // nFileExtension 
   if (!GetSaveFileName(&ofn))
      return FALSE;

   // BUGFIX - Save diretory
   strcpy (szInitial, ofn.lpstrFile);
   szInitial[ofn.nFileOffset] = 0;
   SetLastDirectory(szInitial);

   // save it
   IMAGEDATA   id;
   id.dwMagicNumber = MAGICNUMBER;
   id.dwState = dwState;
   strcpy (id.szFileName, gszFileName);
   id.fInches = gfInches;
   id.fWidth = gfWidth;
   id.fHeight = gfHeight;
   id.fContrast = gfContrast;
   id.fOutlineStrength = gfOutlineStrength;
   id.fBrightness = gfBrightness;
   id.fColorized = gfColorized;
   id.dwMix = gdwMix;
   id.fGrid = gfGrid;
   id.cBaseBlack = gcBaseBlack;
   id.dwBaseDetail = gdwBaseDetail;
   id.iViewCanvasLeft = giViewCanvasLeft;
   id.iViewCanvasTop = giViewCanvasTop;
   id.iViewCanvasWidth = giViewCanvasWidth;

   FILE  * f;
   f = fopen (szTemp, "wb");
   if (!f) {
      MessageBox (hWnd, "Can't save the file. It may be write protected.", NULL, MB_OK);
      return FALSE;
   }

   fwrite (&id, 1, sizeof(id), f);
   fclose (f);

   return TRUE;
}

/****************************************************************************
AskSave - This is called if the user indicates quit. It asks them if they
   want to save their session, yes/no/cancel.

inputs
   PCEscPage   pPage - page
   DWORD    dwState - state. one of the viewmode defines
returns
   BOOL - TRUE if user wants to quit (data may be saved), FALSE if user pressed cancel
*/
BOOL AskSave (PCEscPage pPage, DWORD dwState, BOOL fSkipAsk = TRUE)
{
   int   iRet;
   if (fSkipAsk) {
      iRet = pPage->MBYesNo (L"Do you want to save you current settings?",
         L"If you save your settings you'll be able to run Oil Painting Assistant "
         L"later and continue from this point.", TRUE);
      if (iRet == IDNO)
         return TRUE;
      if (iRet != IDYES)
         return FALSE;  // some sort of cancel
   }

   // save UI
   OPENFILENAME   ofn;
   char  szTemp[256];
   szTemp[0] = 0;
   memset (&ofn, 0, sizeof(ofn));

   // BUGFIX - Set directory
   char szInitial[256];
   GetLastDirectory(szInitial, sizeof(szInitial));
   ofn.lpstrInitialDir = szInitial;

   ofn.lStructSize = sizeof(ofn);
   ofn.hwndOwner = pPage->m_pWindow->m_hWnd;
   ofn.hInstance = ghInstance;
   ofn.lpstrFilter = "Oil Painting Session (*.opa)\0*.opa\0\0\0";
   ofn.lpstrFile = szTemp;
   ofn.nMaxFile = sizeof(szTemp);
   ofn.lpstrTitle = "Save session";
   ofn.Flags = OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
   ofn.lpstrDefExt = "opa";
   // nFileExtension 
   if (!GetSaveFileName(&ofn))
      return FALSE;

   // BUGFIX - Save diretory
   strcpy (szInitial, ofn.lpstrFile);
   szInitial[ofn.nFileOffset] = 0;
   SetLastDirectory(szInitial);

   // save it
   IMAGEDATA   id;
   id.dwMagicNumber = MAGICNUMBER;
   id.dwState = dwState;
   strcpy (id.szFileName, gszFileName);
   id.fInches = gfInches;
   id.fWidth = gfWidth;
   id.fHeight = gfHeight;
   id.fContrast = gfContrast;
   id.fOutlineStrength = gfOutlineStrength;
   id.fBrightness = gfBrightness;
   id.fColorized = gfColorized;
   id.dwMix = gdwMix;
   id.fGrid = gfGrid;
   id.cBaseBlack = gcBaseBlack;
   id.dwBaseDetail = gdwBaseDetail;
   id.iViewCanvasLeft = giViewCanvasLeft;
   id.iViewCanvasTop = giViewCanvasTop;
   id.iViewCanvasWidth = giViewCanvasWidth;

   FILE  * f;
   f = fopen (szTemp, "wb");
   if (!f) {
      pPage->MBWarning (L"Can't save the file. It may be write protected.");
      return FALSE;
   }

   fwrite (&id, 1, sizeof(id), f);
   fclose (f);

   return TRUE;
}

/****************************************************************************
AboutDialogProc - dialog procedure for the intro
*/
BOOL CALLBACK AboutDlgProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
   switch (uMsg) {
   case WM_INITDIALOG:
      {
         SetDlgItemText (hWnd, IDC_VERSION, BUILD_NUMBER);

      }
      return TRUE;

   case WM_COMMAND:
      switch (LOWORD(wParam)) {
      case IDOK:  // IDOK shouldn't happen but...
      case IDCANCEL:
         EndDialog (hWnd, IDCANCEL);
         return TRUE;

      }
      break;
   }
   return FALSE;
}



/********************************************************************************
Colorize - Cuts out a section of the main bitmap and colorizes it in the process.
*/
void Colorize (void)
{
   DWORD x, y, xMax, yMax, xMax2, yMax2;
   WORD  r, g, b;

   int   iHeight;
   iHeight = (int) (giViewCanvasWidth * gfHeight / gfWidth);
   gImageCutout.Clear ((DWORD) giViewCanvasWidth, (DWORD) iHeight);
   xMax = gImageCutout.DimX();
   yMax = gImageCutout.DimY();
   xMax2 = gImageMain.DimX();
   yMax2 = gImageMain.DimY();
   for (y = 0; y < yMax; y++) for (x = 0; x < xMax; x++) {
      int   ix, iy;
      ix = giViewCanvasLeft + (int) x;
      iy = giViewCanvasTop + (int) y;

      if ((ix >= 0) && (ix < (int) xMax2) && (iy > 0) && (iy < (int) yMax2))
         gImageMain.PixelGet ((DWORD) ix, (DWORD) iy, &r, &g, &b);
      else
         r = g = b = 65535;   // white, off the picture

      double   fr, fg, fb;
      fr = r / 65535.0;
      fg = g / 65535.0;
      fb = b / 65535.0;

      // contrast
      if (gfContrast != 1.0) {
         fr = pow (fr, gfContrast);
         fg = pow (fg, gfContrast);
         fb = pow (fb, gfContrast);
      }

      // brightness
      if (gfBrightness != 1.0) {
         fr *= gfBrightness;
         fg *= gfBrightness;
         fb *= gfBrightness;
      }

      // colorize
      if (gfColorized != 1.0) {
         double   f;
         f = (fr + fg + fb) / 3.0;
         fr = f + (fr - f) * gfColorized;
         fg = f + (fg - f) * gfColorized;
         fb = f + (fb - f) * gfColorized;
      }

      fr = min(fr, 1.0);
      fg = min(fg, 1.0);
      fb = min(fb, 1.0);
      fr = max(fr, 0.0);
      fg = max(fg, 0.0);
      fb = max(fb, 0.0);

      gImageCutout.PixelSet (x, y, (WORD) (fr * 63335), (WORD) (fg * 65535), (WORD) (fb * 65535));
   }
}

/********************************************************************************
CenterDialog - Centers the dialog.

inputs
   HWND hWnd - window
*/
void CenterDialog (HWND hWnd)
{
   RECT r, rWork;

   GetWindowRect (hWnd, &r);
   SystemParametersInfo (SPI_GETWORKAREA, 0, &rWork, 0);

   int   cx, cy;
   cx = (rWork.right - rWork.left) / 15 + rWork.left;
   cy = (rWork.bottom - rWork.top) / 2 + rWork.top;
   MoveWindow (hWnd, cx, cy - (r.bottom-r.top)/2, r.right-r.left, r.bottom-r.top, TRUE);
}


/********************************************************************************8
LoadBitmapUI - Brings up user interface to ask for bitmap name and loads it in.
Returns TRUE if all is loaded, FALSE if failed ot load

inputs
   HWND     hWnd - window to create on top of
*/
BOOL LoadBitmapUI (PCEscPage pPage)
{
   HWND hWnd = pPage->m_pWindow->m_hWnd;
   // bring up help file saying that only accept bitmaps
//   WinHelp (NULL, gszHelp, HELP_CONTEXT, IDH_LOADBITMAP);

   OPENFILENAME   ofn;
   char  szTemp[256];
   szTemp[0] = 0;
   memset (&ofn, 0, sizeof(ofn));

   // BUGFIX - Set directory
   char szInitial[256];
   GetLastDirectory(szInitial, sizeof(szInitial));
   ofn.lpstrInitialDir = szInitial;
   
   ofn.lStructSize = sizeof(ofn);
   ofn.hwndOwner = hWnd;
   ofn.hInstance = ghInstance;
   ofn.lpstrFilter = "All (*.bmp,*.jpg)\0*.bmp;*.jpg;*.jpeg\0JPEG (*.jpg)\0*.jpg;*.jpeg\0Bitmap (*.bmp)\0*.bmp\0\0\0";
   ofn.lpstrFile = szTemp;
   ofn.nMaxFile = sizeof(szTemp);
   ofn.lpstrTitle = "Open Image";
   ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
   ofn.lpstrDefExt = "jpg";
   // nFileExtension 

   if (!GetOpenFileName(&ofn))
      return FALSE;

#ifdef TESTMSG
   MessageBox (hWnd, "Test version - LoadBitmapUI", NULL, MB_OK);
#endif


   // BUGFIX - Save diretory
   strcpy (szInitial, ofn.lpstrFile);
   szInitial[ofn.nFileOffset] = 0;
   SetLastDirectory(szInitial);

   strcpy (gszFileName, ofn.lpstrFile);

   // load bitmap
   if (gImageMain.FromBitmapFile (ofn.lpstrFile)) {
      pPage->MBWarning (L"The file couldn't be opened.",
         L"It may not be a proper JPEG or bitmap file, or your hard disk may "
         L"almost be full.");
      return FALSE;
   }

#ifdef TESTMSG
   MessageBox (hWnd, "Test version - Finished OK", NULL, MB_OK);
#endif
   return TRUE;
}


/*****************************************************************************
CanvasSizePage
*/
BOOL CanvasSizePage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   static BOOL gfKeepAspect = FALSE;

   switch (dwMessage) {

   case ESCM_INITPAGE:
      {
         PCEscControl pControl;
         pControl = pPage->ControlFind (L"width");
         WCHAR szTemp[32];
         swprintf (szTemp, L"%g", gfWidth);
         if (pControl)
            pControl->AttribSet (gszText, szTemp);
         pControl = pPage->ControlFind (L"height");
         swprintf (szTemp, L"%g", gfHeight);
         if (pControl)
            pControl->AttribSet (gszText, szTemp);

         pControl = pPage->ControlFind (gfInches ? L"inches" : L"cm");
         if (pControl)
            pControl->AttribSetBOOL (gszChecked, TRUE);

         gfKeepAspect = FALSE;
      }
      return TRUE;

   case ESCM_ADJUSTHEIGHT: // adjust edit box height
      {
         if (!gfKeepAspect)
            return TRUE;
         PCEscControl pControl;
         WCHAR szTemp[32];
         DWORD dwNeeded;
         double   fWidth;

         // what's the current width
         pControl = pPage->ControlFind (L"width");
         if (!pControl)
            return TRUE;
         dwNeeded = sizeof(szTemp);
         szTemp[0] = 0;
         pControl->AttribGet (gszText, szTemp, sizeof(szTemp), &dwNeeded);
         fWidth = 0;
         AttribToDouble (szTemp, &fWidth);

         // apply aspect ration to calculate height
         double fHeight;
         fHeight = fWidth * gdwBitViewY / gdwBitViewX;

         // set height
         pControl = pPage->ControlFind (L"height");
         if (!pControl)
            return TRUE;
         swprintf (szTemp, L"%g", fHeight);
         pControl->AttribSet (gszText, szTemp);
      }
      return TRUE;

   case ESCN_EDITCHANGE:
      {
         PESCNEDITCHANGE p = (PESCNEDITCHANGE)pParam;

         // adjust height if type in width
         if (gfKeepAspect && p->pControl && p->pControl->m_pszName && !_wcsicmp(p->pControl->m_pszName, L"width"))
            pPage->Message (ESCM_ADJUSTHEIGHT);
      }
      break;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;

         if (p->pControl && p->pControl->m_pszName && !_wcsicmp(p->pControl->m_pszName, L"keepaspect")) {
            gfKeepAspect = p->pControl->AttribGetBOOL (gszChecked);

            PCEscControl pControl;
            pControl = pPage->ControlFind (L"height");
            if (pControl) {
               pControl->Enable (!gfKeepAspect);
            }

            pPage->Message (ESCM_ADJUSTHEIGHT);

            break;   // default
         }

      }
      break;

   case ESCM_STOREAWAY:
      {
         PCEscControl pControl;
         WCHAR szTemp[32];
         DWORD dwNeeded;
         pControl = pPage->ControlFind (L"width");
         if (pControl) {
            dwNeeded = sizeof(szTemp);
            szTemp[0] = 0;
            pControl->AttribGet (gszText, szTemp, sizeof(szTemp), &dwNeeded);
            gfWidth = 0;
            AttribToDouble (szTemp, &gfWidth);
            gfWidth = max(.1,gfWidth);
         }
         pControl = pPage->ControlFind (L"height");
         if (pControl) {
            dwNeeded = sizeof(szTemp);
            szTemp[0] = 0;
            pControl->AttribGet (gszText, szTemp, sizeof(szTemp), &dwNeeded);
            gfHeight = 0;
            AttribToDouble (szTemp, &gfHeight);
            gfHeight = max(.1,gfHeight);
         }

         pControl = pPage->ControlFind (L"inches");
         gfInches = (pControl && pControl->AttribGetBOOL (gszChecked));
      }
      return TRUE;
   case ESCM_LINK:
      {
         PESCMLINK p = (PESCMLINK) pParam;

         pPage->Message (ESCM_STOREAWAY);
         gfKnowGridArt = FALSE;  // asume its not gridart

         // if it's one of the grid-art sizes then deal with that
         if (!p->psz)
            break;
         if (!_wcsicmp (p->psz, L"4x5")) {
            gfKnowGridArt = TRUE;
            gfWidth = 16;
            gfHeight = 20;
            gfInches = TRUE;
            gfGrid = 4.0;
            pPage->Exit (gszNext);
            return TRUE;
         }
         else if (!_wcsicmp (p->psz, L"5x4")) {
            gfKnowGridArt = TRUE;
            gfWidth = 20;
            gfHeight = 16;
            gfInches = TRUE;
            gfGrid = 4.0;
            pPage->Exit (gszNext);
            return TRUE;
         }
         else if (!_wcsicmp (p->psz, L"3x4")) {
            gfKnowGridArt = TRUE;
            gfWidth = 12;
            gfHeight = 16;
            gfInches = TRUE;
            gfGrid = 12.0 / 4.0;  // bugfix - multiply for partial grid
            pPage->Exit (gszNext);
            return TRUE;
         }
         else if (!_wcsicmp (p->psz, L"4x3")) {
            gfKnowGridArt = TRUE;
            gfWidth = 16;
            gfHeight = 12;
            gfInches = TRUE;
            gfGrid = 12.0 / 4.0;  // bugfix - multiply for partial grid
            pPage->Exit (gszNext);
            return TRUE;
         }
         else if (!_wcsicmp (p->psz, L"5x6")) {
            gfKnowGridArt = TRUE;
            gfWidth = 20;
            gfHeight = 24;
            gfInches = TRUE;
            gfGrid = 20.0 / 4.0;  // bugfix - multiply for partial grid
            pPage->Exit (gszNext);
            return TRUE;
         }
         else if (!_wcsicmp (p->psz, L"6x5")) {
            gfKnowGridArt = TRUE;
            gfWidth = 24;
            gfHeight = 20;
            gfInches = TRUE;
            gfGrid = 20.0 / 4.0;
            pPage->Exit (gszNext);
            return TRUE;
         }
      }
      break;   // default behavour
   case ESCM_CLOSE:
      pPage->Message (ESCM_STOREAWAY);
      if (!AskSave (pPage, VIEWMODE_CANVASSIZE))
         return TRUE;   // abort cancel
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"IMAGE")) {
            gMemTemp.Required (512);
            MultiByteToWideChar (CP_ACP, 0, gszFileName, -1, (PWSTR)gMemTemp.p, 512);
            p->pszSubString = (PWSTR)gMemTemp.p;
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}



/*****************************************************************************
ColorizePage
*/
BOOL ColorizePage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   switch (dwMessage) {

   case ESCM_INITPAGE:
      {
         DWORD dwColor;
         PCEscControl pControl;

         if (gfContrast >= 2.0)
            dwColor = 4;
         else if (gfContrast >= 1.5)
            dwColor = 3;
         else if (gfContrast >= 1.0)
            dwColor = 2;
         else if (gfContrast >= (1.0 / 1.5))
            dwColor = 1;
         else
            dwColor = 0;
         pControl = pPage->ControlFind (L"contrast");
         if (pControl)
            pControl->AttribSetInt (gszCurSel, (int) dwColor);

         if (gfBrightness >= 2.0)
            dwColor = 4;
         else if (gfBrightness >= 1.5)
            dwColor = 3;
         else if (gfBrightness >= 1.0)
            dwColor = 2;
         else if (gfBrightness >= (1.0 / 1.5))
            dwColor = 1;
         else
            dwColor = 0;
         pControl = pPage->ControlFind (L"brightness");
         if (pControl)
            pControl->AttribSetInt (gszCurSel, (int) dwColor);

         if (gfColorized >= 2.0)
            dwColor = 4;
         else if (gfColorized >= 1.5)
            dwColor = 3;
         else if (gfColorized >= 1.0)
            dwColor = 2;
         else if (gfColorized >= 0.5)
            dwColor = 1;
         else
            dwColor = 0;
         pControl = pPage->ControlFind (L"colorize");
         if (pControl)
            pControl->AttribSetInt (gszCurSel, (int) dwColor);
      }
      return TRUE;

   case ESCM_CLOSE:
      if (!AskSave (pPage, VIEWMODE_COLORIZE))
         return TRUE;   // abort cancel
      break;

   case ESCN_COMBOBOXSELCHANGE:
      {
         PESCNCOMBOBOXSELCHANGE p = (PESCNCOMBOBOXSELCHANGE)pParam;
         double fVal;
         BOOL fChanged = FALSE;

         if (!p->pControl || !p->pControl->m_pszName)
            break;
         if (!_wcsicmp(p->pControl->m_pszName, L"contrast")) {
            fVal = afContrast[p->dwCurSel];
            fChanged = (gfContrast != fVal);
            gfContrast = fVal;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"brightness")) {
            fVal = afContrast[p->dwCurSel];
            fChanged = (gfBrightness != fVal);
            gfBrightness = fVal;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"colorize")) {
            fVal = afColorize[p->dwCurSel];
            fChanged = (gfColorized != fVal);
            gfColorized = fVal;
         }

         // invalidate the bitmap
         if (fChanged) {
            PCEscControl pControl = pPage->ControlFind (L"image");
            Colorize ();

            // set new bitmap
            if (ghBitView)
               DeleteObject (ghBitView);
            ghBitView = gImageCutout.ToBitmap ();
            WCHAR szTemp[32];
            swprintf (szTemp, L"%x", (int) ghBitView);
            pControl->AttribSet (L"hbitmap", szTemp);
         }
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"HBITMAP")) {
            gMemTemp.Required (32);
            swprintf ((PWSTR) gMemTemp.p, L"%x", (int) ghBitView);
            p->pszSubString = (PWSTR)gMemTemp.p;
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}

/******************************************************************************
CalcGrid - Calculate the grid in relation to pixels in the cutout.

inputs
   double      *pfX, *pfY - filled with X and Y pixels. 0 for no grid
returns
   none
*/
void CalcGrid (double *pfX, double *pfY)
{
   *pfX = *pfY = 0;
   if (!gfGrid)
      return;

   double   fDistX, fDistY;
   if (gfGrid < 0) {
      fDistX = (double) gdwBitViewX / (-gfGrid);
      fDistY = (double) gdwBitViewY / (-gfGrid);
   }
   else
      fDistX = fDistY = (double) gdwBitViewX * gfGrid / gfWidth;

   *pfX = fDistX;
   *pfY = fDistY;
}

/******************************************************************************
SendGridToImage - Looks at the global for the current grid settings,
calculated how large it should be, and sends it to the "image" control.
*/
void SendGridToImage (PCEscPage pPage)
{
   double fx, fy;
   CalcGrid (&fx, &fy);
   PCEscControl pControl;
   pControl = pPage->ControlFind (L"image");
   if (pControl) {
      WCHAR szTemp[32];
      swprintf (szTemp, L"%g", fx);
      pControl->AttribSet (L"gridx", szTemp);

      swprintf (szTemp, L"%g", fy);
      pControl->AttribSet (L"gridy", szTemp);
   }
}

/*****************************************************************************
MoveHover - Handle mouse hover over the image.

inputs
   PCEscPage      pPage - Page
   PVOID          pParam - hover parameters
returns
   BOOL - What should return
*/
// #define FINDWHYNOTOOLTIP

BOOL MouseHover (PCEscPage pPage, PVOID pParam)
{
  PESCMMOUSEHOVER p = (PESCMMOUSEHOVER) pParam;
#ifdef FINDWHYNOTOOLTIP
  EscOutputDebugString (L"\r\nMouseHover in OilPaint");
#endif

   // BUGFIX - if there's no coor info then do nothing
   if (!gpPColor) {
#ifdef FINDWHYNOTOOLTIP
      EscOutputDebugString (L"\r\nError: !gpPColor");
#endif
      return FALSE;
   }

   // if it's over the image control then change stats
   PCEscControl pControl;
   pControl = pPage->ControlFind (L"image");
   if (!pControl) {
#ifdef FINDWHYNOTOOLTIP
      EscOutputDebugString (L"\r\nError: !pControl");
#endif
      return FALSE;
   }
   if (!PtInRect (&pControl->m_rPosn, p->pPosn)) {
#ifdef FINDWHYNOTOOLTIP
      EscOutputDebugString (L"\r\nWarning: !PtInRect m_rPosn");
#endif
      return FALSE;
   }

   // take border into account
   int   iBorder;
   iBorder = pControl->AttribGetInt (L"border");
   RECT r;
   r = pControl->m_rPosn;
   r.left += iBorder;
   r.top += iBorder;
   r.right -= iBorder;
   r.bottom -= iBorder;
   if (!PtInRect (&r, p->pPosn)) {
#ifdef FINDWHYNOTOOLTIP
      EscOutputDebugString (L"\r\nWarning: !PtInRect p");
#endif
      return FALSE;
   }

   // convert this to pixel X and Y
   int   ix, iy;
   ix = (int) ((double) (p->pPosn.x - r.left) / (r.right - r.left) * gImageCutout.DimX());
   iy = (int) ((double) (p->pPosn.y - r.top) / (r.bottom - r.top) * gImageCutout.DimY());
   if ((ix >= 0) && (ix < (int) gImageCutout.DimX()) &&
      (iy >= 0) && (iy < (int) gImageCutout.DimY())) {
      PCOLOR   *pc;
      pc = gpPColor + gawImageColor[iy * gImageCutout.DimX() + ix];
      WCHAR szTemp[256];

      // convert it to hoverhelp
      MemZero (&gMemTemp);
      MemCat (&gMemTemp, L"<small>");

      // color
      szTemp[0] = 0; // BUGFIX - Just in case name too large
      MultiByteToWideChar (CP_ACP, 0, pc->szName, -1, szTemp, sizeof(szTemp)/2);
      MemCatSanitize (&gMemTemp, szTemp);
      MemCat (&gMemTemp, L"<br/>");

      // location
      double x, y;
      x = (double) ix / gImageCutout.DimX() * gfWidth;
      y = (double) iy / gImageCutout.DimY() * gfHeight;
      swprintf (szTemp,
         gfInches ? L"%.1f inches from the left, %.1f inches from the top" :
            L"%.1f cm from the left, %.1f cm from the top",
         x, y);
#ifdef FINDWHYNOTOOLTIP
      EscOutputDebugString (L"\r\n");
      EscOutputDebugString (szTemp);
#endif
      MemCatSanitize (&gMemTemp, szTemp);

      MemCat (&gMemTemp, L"</small>");

      PCMMLNode pNode;
      CEscError err;
      pNode = ParseMML((PWSTR)gMemTemp.p, ghInstance, NULL,NULL, &err);
      if (pNode) {
         POINT p2, p1;
         p2 = p->pPosn;
         p2.x += 16;
         p2.y += 16;
         pPage->CoordPageToScreen (&p2, &p1);
         pNode->AttribSet (L"hresize", L"true");
#ifdef FINDWHYNOTOOLTIP
         EscOutputDebugString (L"\r\nAbout to display tooltip");
#endif
         BOOL fRet = pPage->m_pWindow->HoverHelp(ghInstance, pNode, FALSE, &p1);
#ifdef FINDWHYNOTOOLTIP
         if (fRet)
            EscOutputDebugString (L"\r\nDispplayed tooltip");
         else
            EscOutputDebugString (L"\r\nError: Hoverhelp failed");
#endif
         delete pNode;
      }
#ifdef FINDWHYNOTOOLTIP
      else
         EscOutputDebugString (L"\r\nError: !pNode");
#endif

      //pPage->m_pWindow->TitleSet (szTemp);
      return TRUE;
   }
#ifdef FINDWHYNOTOOLTIP
   else
      EscOutputDebugString (L"\r\nWarning: !In image");
#endif

   return FALSE;
}

/*****************************************************************************
GridPage
*/
BOOL GridPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   switch (dwMessage) {

   case ESCM_INITPAGE:
      {
         PCEscControl pControl;
         WCHAR szTemp[16];
         swprintf (szTemp, L"%g", (gfGrid > 0) ? gfGrid : (-gfGrid));

         if (gfGrid == 0) {
            if (pControl = pPage->ControlFind (L"none"))
               pControl->AttribSetBOOL (gszChecked, TRUE);
         }
         else if (gfGrid > 0) {
            if (pControl = pPage->ControlFind (L"fixed"))
               pControl->AttribSetBOOL (gszChecked, TRUE);
            if (pControl = pPage->ControlFind (L"fixedval"))
               pControl->AttribSet (gszText, szTemp);
         }
         else {   // fgrid < 0, fraction
            if (pControl = pPage->ControlFind (L"fraction"))
               pControl->AttribSetBOOL (gszChecked, TRUE);
            if (pControl = pPage->ControlFind (L"fractionval"))
               pControl->AttribSet (gszText, szTemp);
         }

         if (pControl = pPage->ControlFind (L"outline"))
            pControl->AttribSetInt (L"pos", (int)(gfOutlineStrength*100.0));

         // send the new grid to the image
         SendGridToImage(pPage);
      }
      return TRUE;

   case ESCM_CLOSE:
      if (!AskSave (pPage, VIEWMODE_GRID))
         return TRUE;   // abort cancel
      break;

   case ESCN_SCROLL:
   //case ESCN_SCROLLING:
      {
         PESCNSCROLL p = (PESCNSCROLL)pParam;
         if (!p->pControl->m_pszName || _wcsicmp(p->pControl->m_pszName, L"outline"))
            break;

         gfOutlineStrength = (double)p->iPos / 100.0;

         PCEscControl pControl;
         pControl = pPage->ControlFind (L"image");

         // rebuild the image
         ColorsToImage ((float)gfOutlineStrength);
         if (ghBitView)
            DeleteObject (ghBitView);
         ghBitView = gImageColors.ToBitmap (NULL);

         WCHAR szTemp[32];
         swprintf (szTemp, L"%x", (int) ghBitView);
         if (pControl)
            pControl->AttribSet (L"hbitmap", szTemp);
         
      }
      break;

   case ESCM_ADJUSTHEIGHT:
      {
         // find which is checked
         PCEscControl pControl;
         BOOL fFixed = FALSE;
         pControl = pPage->ControlFind (L"none");
         if (pControl && pControl->AttribGetBOOL(gszChecked)) {
            // no grid
            gfGrid = 0;

            // set the grid
            SendGridToImage (pPage);
            return TRUE;
         }

         pControl = pPage->ControlFind (L"fixed");
         if (pControl)
            fFixed = pControl->AttribGetBOOL (gszChecked);


         // get the value
         pControl = pPage->ControlFind (fFixed ? L"fixedval" : L"fractionval");
         if  (!pControl)
            return TRUE;
         WCHAR szTemp[32];
         DWORD dwNeeded;
         double   fWidth;
         dwNeeded = sizeof(szTemp);
         szTemp[0] = 0;
         pControl->AttribGet (gszText, szTemp, sizeof(szTemp), &dwNeeded);
         fWidth = 0;
         AttribToDouble (szTemp, &fWidth);
         if (fWidth <= 0.0)
            return TRUE;   // wrong value

         // remember this
         gfGrid = fFixed ? fWidth : (-fWidth);

         // set the grid
         SendGridToImage (pPage);
      }
      return TRUE;

   case ESCN_EDITCHANGE:
      {
         PESCNEDITCHANGE p = (PESCNEDITCHANGE)pParam;

         if (!p->pControl || !p->pControl->m_pszName)
            break;

         // adjust height if type in width
         if (!_wcsicmp(p->pControl->m_pszName, L"fixedval") || !_wcsicmp(p->pControl->m_pszName, L"fractionval")) {
            pPage->Message (ESCM_ADJUSTHEIGHT);
         }
      }
      break;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS)pParam;

         if (!p->pControl || !p->pControl->m_pszName)
            break;

         if (!_wcsicmp(p->pControl->m_pszName, L"fixed") || !_wcsicmp(p->pControl->m_pszName, L"fraction") || !_wcsicmp(p->pControl->m_pszName, L"none")) {
            pPage->Message (ESCM_ADJUSTHEIGHT);
         }

         if (!_wcsicmp(p->pControl->m_pszName, L"savebase")) {
            gImageColors.ToJPEGFile (pPage->m_pWindow->m_hWnd, NULL);
            return TRUE;
         }
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"HBITMAP")) {
            gMemTemp.Required (32);
            swprintf ((PWSTR) gMemTemp.p, L"%x", (int) ghBitView);
            p->pszSubString = (PWSTR)gMemTemp.p;
            return TRUE;
         }
      }
      break;

   case ESCM_MOUSEHOVER:
      return MouseHover (pPage, pParam);
   };


   return DefPage (pPage, dwMessage, pParam);
}

/*****************************************************************************
SketchPage
*/
BOOL SketchPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   switch (dwMessage) {

   case ESCM_CLOSE:
      if (!AskSave (pPage, gdwSketchPageExit))
         return TRUE;   // abort cancel
      break;

   
   case ESCM_INITPAGE:
      {
         PCEscControl pControl;
         WCHAR szTemp[16];
         swprintf (szTemp, L"%d", (int) gdwDisplayScale);

         ESCMCOMBOBOXSELECTSTRING ss;
         memset (&ss, 0, sizeof(ss));
         ss.fExact = TRUE;
         ss.iStart = -1;
         ss.psz = szTemp;
         pControl = pPage->ControlFind (L"scale");
         if (pControl)
            pControl->Message (ESCM_COMBOBOXSELECTSTRING, &ss);

         // send the new grid to the image
         SendGridToImage(pPage);
      }
      return TRUE;

   case ESCN_COMBOBOXSELCHANGE:
      {
         PESCNCOMBOBOXSELCHANGE p = (PESCNCOMBOBOXSELCHANGE)pParam;
         DWORD iVal;
         BOOL fChanged = FALSE;

         if (!p->pControl || !p->pControl->m_pszName)
            break;
         if (p->pszName && !_wcsicmp(p->pControl->m_pszName, L"scale")) {
            iVal = (DWORD) _wtoi(p->pszName);
            fChanged = (gdwDisplayScale != iVal);
            gdwDisplayScale = iVal;
         }

         // invalidate the bitmap
         if (fChanged) {
            pPage->Exit (gszSamePage);
            return TRUE;
         }
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"HBITMAP")) {
            gMemTemp.Required (32);
            swprintf ((PWSTR) gMemTemp.p, L"%x", (int) ghBitView);
            p->pszSubString = (PWSTR)gMemTemp.p;
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"SCALE")) {
            MemZero (&gMemTemp);
            MemCat (&gMemTemp, (int)gdwDisplayScale);
            p->pszSubString = (PWSTR)gMemTemp.p;
            return TRUE;
         }
      }
      break;

   case ESCM_MOUSEHOVER:
      return MouseHover (pPage, pParam);
   };


   return DefPage (pPage, dwMessage, pParam);
}

/*****************************************************************************
SignPage
*/
BOOL SignPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   switch (dwMessage) {

   case ESCM_LINK:
      {
         PESCMLINK p = (PESCMLINK) pParam;

         if (p->psz && !_wcsicmp(p->psz, L"save")) {
            if (AskSave (pPage, VIEWMODE_SKETCH, FALSE))
               pPage->MBSpeakInformation (L"File saved.");
            return TRUE;   // dont exit
         }
      }
      // BUGFIX - Was return TRUE but then "back" button wouldn't work
      break;

   };


   return DefPage (pPage, dwMessage, pParam);
}
/*****************************************************************************
BitmapToDIB - Necessary to print in color?

iunputs
   HBITMAP        hBitmap - Bitmap to use
   HDC            hDCUse - DC to base off of, or NULL if use main window
*/
typedef HANDLE HDIB;
HDIB BitmapToDIB(HBITMAP hBitmap, HDC hDCUse) 
{ 
    BITMAP              bm;         // bitmap structure 
    BITMAPINFOHEADER    bi;         // bitmap header 
    LPBITMAPINFOHEADER  lpbi;       // pointer to BITMAPINFOHEADER 
    DWORD               dwLen;      // size of memory block 
    HANDLE              hDIB, h;    // handle to DIB, temp handle 
    HDC                 hDC;        // handle to DC 
    WORD                biBits;     // bits per pixel 
//    HPALLETE            hPal = NULL;
 
    // check if bitmap handle is valid 
 
    if (!hBitmap) 
        return NULL; 
 
    // fill in BITMAP structure, return NULL if it didn't work 
 
    if (!GetObject(hBitmap, sizeof(bm), (LPSTR)&bm)) 
        return NULL; 
 
    // if no palette is specified, use default palette 
 
//    if (hPal == NULL) 
//        hPal = GetStockObject(DEFAULT_PALETTE); 
 
    // calculate bits per pixel 
 
    biBits = bm.bmPlanes * bm.bmBitsPixel; 
 
    // make sure bits per pixel is valid 
 
    if (biBits <= 1) 
        biBits = 1; 
    else if (biBits <= 4) 
        biBits = 4; 
    else if (biBits <= 8) 
        biBits = 8; 
    // BUGFIX - Take out setting bibits to 24
    // BUGFIX - Try putting back in because not printing now
    else // if greater than 8-bit, force to 24-bit 
        biBits = 24; 
 
    // initialize BITMAPINFOHEADER 
 
    bi.biSize = sizeof(BITMAPINFOHEADER); 
    bi.biWidth = bm.bmWidth; 
    bi.biHeight = bm.bmHeight; 
    bi.biPlanes = 1; 
    bi.biBitCount = biBits; 
    bi.biCompression = BI_RGB; 
    bi.biSizeImage = 0; 
    bi.biXPelsPerMeter = 0; 
    bi.biYPelsPerMeter = 0; 
    bi.biClrUsed = 0; 
    bi.biClrImportant = 0; 
 
    // calculate size of memory block required to store BITMAPINFO 
 
    dwLen = bi.biSize; // + PaletteSize((LPSTR)&bi); 
 
    // get a DC 
 
    HWND hWndDesk = GetDesktopWindow();   // BUGFIX - Try to get desktop
    if (hDCUse)
       hDC = hDCUse;
    else
      hDC = GetDC(hWndDesk); 
    if (!hDC)
       return NULL;
 
    // select and realize our palette 
 
//    hPal = SelectPalette(hDC, hPal, FALSE); 
//    RealizePalette(hDC); 
 
    // alloc memory block to store our bitmap 
 
    hDIB = GlobalAlloc(GHND, dwLen); 
 
    // if we couldn't get memory block 
 
    if (!hDIB) 
    { 
      // clean up and return NULL 
 
//      SelectPalette(hDC, hPal, TRUE); 
//      RealizePalette(hDC); 
      if (!hDCUse)
         ReleaseDC(hWndDesk, hDC); 
      return NULL; 
    } 
 
    // lock memory and get pointer to it 
 
    lpbi = (LPBITMAPINFOHEADER)GlobalLock(hDIB); 
 
    /// use our bitmap info. to fill BITMAPINFOHEADER 
 
    *lpbi = bi; 
 
    // call GetDIBits with a NULL lpBits param, so it will calculate the 
    // biSizeImage field for us     
 
   DWORD dwErr;
    if (!GetDIBits(hDC, hBitmap, 0, (UINT)bi.biHeight, NULL, (LPBITMAPINFO)lpbi, 
       DIB_RGB_COLORS)) {
          // BUGFIX - Put in error here
          dwErr = GetLastError ();
          GlobalUnlock (hDIB);
         GlobalFree(hDIB); 
         hDIB = NULL; 
         if (!hDCUse)
               ReleaseDC(hWndDesk, hDC); 
         return NULL; 
       }
 
    // get the info. returned by GetDIBits and unlock memory block 
 
    bi = *lpbi; 
    GlobalUnlock(hDIB); 

 #define WIDTHBYTES(bits)    (((bits) + 31) / 32 * 4) 

    // if the driver did not fill in the biSizeImage field, make one up  
    if (bi.biSizeImage == 0) 
        lpbi->biSizeImage = bi.biSizeImage = WIDTHBYTES((DWORD)bm.bmWidth * biBits) * bm.bmHeight; 
    // BUGFIX - Set lpbi->biSizeImage also
 
    // realloc the buffer big enough to hold all the bits 
 
    dwLen = bi.biSize + /* PaletteSize((LPSTR)&bi) +*/ bi.biSizeImage; 
 
    if (h = GlobalReAlloc(hDIB, dwLen, 0)) 
        hDIB = h; 
    else 
    { 
        // clean up and return NULL 
 
        GlobalFree(hDIB); 
        hDIB = NULL; 
//        SelectPalette(hDC, hPal, TRUE); 
//        RealizePalette(hDC); 
        if (!hDCUse)
            ReleaseDC(hWndDesk, hDC); 
        return NULL; 
    } 
 
    // lock memory block and get pointer to it */ 
 
    lpbi = (LPBITMAPINFOHEADER)GlobalLock(hDIB); 
 
    // call GetDIBits with a NON-NULL lpBits param, and actualy get the 
    // bits this time 
 
    if (GetDIBits(hDC, hBitmap, 0, (UINT)bi.biHeight, (LPSTR)lpbi + 
            (WORD)lpbi->biSize /*+ PaletteSize((LPSTR)lpbi)*/, (LPBITMAPINFO)lpbi, 
            DIB_RGB_COLORS) == 0) 
    { 
        // clean up and return NULL 
         dwErr = GetLastError ();

        GlobalUnlock(hDIB); 
        hDIB = NULL; 
//        SelectPalette(hDC, hPal, TRUE); 
//        RealizePalette(hDC); 
        if (!hDCUse)
            ReleaseDC(hWndDesk, hDC); 
        return NULL; 
    } 
 
    bi = *lpbi; 
 
    // clean up  
    GlobalUnlock(hDIB); 
//    SelectPalette(hDC, hPal, TRUE); 
//    RealizePalette(hDC); 
    if (!hDCUse)
      ReleaseDC(hWndDesk, hDC); 
 
    // return handle to the DIB 
    return hDIB; 
} 
 

/**************************************************************************************
PrintImages - This prints the selected images.

inputs
   HWND     hWnd - dialog window calling this
   BOOL     fPrintOrig - print original image
   BOOL     fPrintCutout - print cutout
   BOOL     fCutoutGrid - print grid on cutour
   BOOL     fCutoutBig - Print cutout full scale
   BOOL     fPrintBase - print base
   BOOL     fBaseGrid - put a grid on the base
   BOOL     fBaseBig - Paint base full scale
   BOLL     fBaseKey - print the key
   BOOL     fInvert - If TRUE then print inverted images
returns
   BOOL - TRUE if OK, FALSE is cancel pressed
*/
BOOL PrintImages (HWND hWnd, BOOL fPrintOrig,
                  BOOL fPrintCutout, BOOL fCutoutGrid, BOOL fCutoutBig,
                  BOOL fPrintBase, BOOL fBaseGrid, BOOL fBaseBig,
                  BOOL fBaseKey, BOOL fInvert)
{
   BOOL  fRet = TRUE;
   HDC   hDCMem = NULL;
   HDC   hDCPrint = NULL;
   HBITMAP  hBitOld = NULL, hBit = NULL; 
   HPEN  hPen = NULL, hPenOld = NULL;
   HFONT hFont = NULL, hFontOld = NULL;

#ifdef DEBUGPRINTING
   EscOutputDebugString (L"\r\nPrintimages");
#endif

   // print dialog box
   PRINTDLG pd;
   memset (&pd, 0, sizeof(pd));
   pd.lStructSize = sizeof(pd);
   pd.Flags = PD_RETURNDC | PD_NOPAGENUMS | PD_NOSELECTION | PD_USEDEVMODECOPIESANDCOLLATE;
   pd.hwndOwner = hWnd;
   pd.nFromPage = 1;
   pd.nToPage = (fPrintOrig ? 1 : 0) + (fPrintCutout ? 1 : 0) + (fPrintBase ? 1 : 0) +
      (fBaseKey ? 1 : 0);
   if (!pd.nToPage)
      return TRUE;   // nothing to print
   pd.nCopies = 1;

   if (!PrintDlg (&pd))
      return FALSE;  // cancelled or error

   hDCPrint = pd.hDC;

   if (!(GetDeviceCaps(pd.hDC, RASTERCAPS) & RC_BITBLT)) {
      MessageBox(hWnd, 
        "Printer cannot display bitmaps.", NULL, 
        MB_OK);
      fRet = FALSE;
      goto done;
   }  

   // start the document
   DOCINFO  di;
   memset (&di, 0, sizeof(di));
   di.cbSize = sizeof(di);
   di.lpszDocName = "Oil Painting Assistant"; 
#ifdef DEBUGPRINTING
   EscOutputDebugString (L"\r\nStartDoc");
#endif
   if (StartDoc(hDCPrint, &di) <= 0) {
      MessageBox(hWnd, 
        "StartDoc() call failed.", NULL, 
        MB_OK);
      fRet = FALSE;
      fRet = FALSE;
      goto done;
   }

   // create the font and stuff
   hPen = CreatePen (PS_DASHDOT, 1, RGB(0,0,0));
   // BUGFIX - Try taking this out because users talking about different printer color
   // problems. I put in because seemed to need for HP, but may not need now:
   SetICMMode (hDCPrint, ICM_ON);   // BUGFIX - Put back in because user complains that no image printed
   // SetICMMode (hDCMem, ICM_ON);
   hDCMem = CreateCompatibleDC(hDCPrint);
   if (!hDCMem) {
      MessageBox(hWnd, 
        "CreateCompatibleDC() failed.", NULL, 
        MB_OK);
      fRet = FALSE;
      goto done;
   }

   // figure out some scaling
   double fPixelsPerInchX, fPixelsPerInchY;
   int   iPixX, iPixY;     // pixels across paper
   int   iXOrigin, iYOrigin;
   fPixelsPerInchX = (double) GetDeviceCaps(hDCPrint, LOGPIXELSX);
   fPixelsPerInchY = (double) GetDeviceCaps(hDCPrint, LOGPIXELSY);
   iPixX = GetDeviceCaps (hDCPrint, HORZRES);
   iPixY = GetDeviceCaps (hDCPrint, VERTRES);
   // Use the printable region of the printer to determine 
   // the margins. 
   iXOrigin = GetDeviceCaps(hDCPrint, PHYSICALOFFSETX); 
   iYOrigin = GetDeviceCaps(hDCPrint, PHYSICALOFFSETY); 

#ifdef DEBUGPRINTING
   WCHAR szwTemp[256];
   swprintf (szwTemp, L"\r\nPixelsPerInch=%g,%g, Res=%d,%d, Offset=%d,%d",
      (double)fPixelsPerInchX, (double)fPixelsPerInchY, (int)iPixX, (int)iPixY,
      (int)iXOrigin, (int)iYOrigin);
   EscOutputDebugString (szwTemp);
#endif
   if (!iPixX || !iPixY || !iXOrigin || !iYOrigin) {
      MessageBox(hWnd, 
        "GetDeviceCaps() failed.", NULL, 
        MB_OK);
   }

   // BUGFIX - Moved the font
   // do the font
   LOGFONT  lf;
   memset (&lf, 0, sizeof(lf));
   // BUGFIX - So doesn't print really small fonts
   //lf.lfHeight = 16 * 3 / 2;
   lf.lfHeight = -MulDiv(16, GetDeviceCaps(hDCPrint, LOGPIXELSY), 72); 
   lf.lfCharSet = DEFAULT_CHARSET;
   lf.lfPitchAndFamily = VARIABLE_PITCH | FF_MODERN;
   strcpy (lf.lfFaceName, "Arial");
   hFont = CreateFontIndirect (&lf);

#ifdef DEBUGPRINTING
   swprintf (szwTemp, L"\r\nFont height = %d", (int)lf.lfHeight);
   EscOutputDebugString (szwTemp);
#endif

   DWORD dwImage;
   for (dwImage = 0; dwImage < 3; dwImage++) {
      CImage *pImage;
      BOOL  fGrid;
      BOOL  fBig;
      fBig = FALSE;

      switch (dwImage) {
      case 0:  // original
         if (!fPrintOrig)
            continue;   // don't print this
         pImage = &gImageMain;
         fGrid = FALSE;
         break;
      case 1:  // cutout
         if (!fPrintCutout)
            continue;   // don't print this
         pImage = &gImageCutout;
         fGrid = fCutoutGrid;
         fBig = fCutoutBig;
         break;
      case 2:  // base
         if (!fPrintBase)
            continue;   // don't print this
         pImage = &gImageColors;
         fGrid = fBaseGrid;
         fBig = fBaseBig;
         break;
      }

#ifdef DEBUGPRINTING
      swprintf (szwTemp, L"\r\nImage %d", (int)dwImage);
      EscOutputDebugString (szwTemp);
#endif

      HDIB  hDib = NULL;



      // create the bitmap & select it
      DWORD dwX, dwY;
      hBit = pImage->ToBitmap (NULL, fInvert);
      if (!hBit) {
         MessageBox (hWnd, "pImage->ToBitmap() returns NULL.", "Error", MB_OK);
         fRet = FALSE;
         goto done;
      }
      dwX = pImage->DimX();
      dwY = pImage->DimY();


#ifdef DEBUGPRINTING
      swprintf (szwTemp, L"\r\n\tDim = %d, %d", (int)dwX, (int)dwY);
      EscOutputDebugString (szwTemp);
#endif

      double   fScale;
      if (fBig) {
         // BUGFIX - if it's big then calculate the scale
         // first assume measurement in inches
         fScale = gfWidth / dwX;

         if (!gfInches)
            fScale /= 2.54;
#ifdef DEBUGPRINTING
         swprintf (szwTemp, L"\r\n\tBig fScale = %g / %d = %g", (double)gfWidth, (int)dwX, (double)fScale);
         EscOutputDebugString (szwTemp);
#endif

      }
      else {
         // figure out a scaling such that the image won't go beyond either edge
         // look at aspect ratio
         if (((double) dwY / (double) dwX) > ((iPixY - 2*iYOrigin) / fPixelsPerInchY * fPixelsPerInchX / (iPixX - 2*iXOrigin))) {
            // image is taller than the paper
            fScale = (iPixY - 2*iYOrigin) / fPixelsPerInchY / dwY;   // so takes all of Y
         }
         else {
            // image is wider than the paper
            fScale = (iPixX - 2*iXOrigin) / fPixelsPerInchX / dwX;   // so takes all of X
         }
#ifdef DEBUGPRINTING
         swprintf (szwTemp, L"\r\n\tSmall fScale = %g", (double)fScale);
         EscOutputDebugString (szwTemp);
#endif
      }

      // 
#ifdef _DEBUG
      // fScale /= 4.0;
#endif
      int   iLeft, iTop, iWidth, iHeight, iWidthIndent, iHeightIndent; // in printer coordinates
      iWidth = (int) (dwX * fScale * fPixelsPerInchX);
      iHeight = (int) (dwY * fScale * fPixelsPerInchY);

      // figure out where to start
      if (fBig) {
         iLeft = 0;
         iTop = 0;
      }
      else {
         iLeft = iPixX / 2 - iWidth / 2;
         iTop = iPixY / 2 - iHeight / 2;
      }
      iWidthIndent = iHeightIndent = 0;

#ifdef DEBUGPRINTING
      swprintf (szwTemp, L"\r\n\tiLeft = %d, iTop = %d", (int)iLeft, (int)iTop);
      EscOutputDebugString (szwTemp);
#endif
      for (; iWidthIndent < iWidth; iWidthIndent += iPixX) {
         iHeightIndent = 0;
         for (; iHeightIndent < iHeight; iHeightIndent += iPixY) {
            if (StartPage(hDCPrint) <= 0) {
               MessageBox(hWnd, 
                 "StartPage() failed.", NULL, 
                 MB_OK);
               fRet = FALSE;
               goto done;
            }

#ifdef DEBUGPRINTING
            swprintf (szwTemp, L"\r\n\tStart page %d, %d", (int)iWidthIndent, (int)iHeightIndent);
            EscOutputDebugString (szwTemp);
#endif
            // BUGFIX - at the bottom of every page print the file and the date
            hFontOld = (HFONT) SelectObject (hDCPrint, hFont);
            char  szTemp[256];
            SIZE sFooter;
            SYSTEMTIME st;
            memset (&sFooter, 0, sizeof(sFooter));
            GetLocalTime (&st);
            sprintf (szTemp, "%s - %d/%d/%d", gszFileName, (int) st.wDay, (int)st.wMonth, (int) st.wYear);
            // BUGFIX - On one printer it's an all black backgroun with white text
            SetTextColor (hDCPrint, RGB(0,0,0));
            SetBkColor (hDCPrint, RGB(255,255,255));
            GetTextExtentPoint32(hDCPrint, szTemp, strlen(szTemp), &sFooter);
            TextOut(hDCPrint, ((int)iPixX - sFooter.cx) / 2, iPixY - iYOrigin - sFooter.cy,
               szTemp, strlen(szTemp));
      
            SelectObject (hDCPrint, hFontOld);
            //StretchBlt(hDCPrint, iLeft, iTop, iWidth, iHeight,
            //  hDCMem, 0, 0, dwX, dwY, SRCCOPY);

            // offset
            int   iXOff, iYOff;
            iXOff = iLeft-iWidthIndent;
            iYOff = iTop-iHeightIndent;

            // BUGFIX - Move bitmap to dib code later on
            // try the dib approach
            // BUGFIX - Pass hDCPrint in when create device independent bitmap. Maybe this
            // will fix the inverse color bug

            if (hDib)
               GlobalFree (hDib);
            hDib = NULL;

            hDib = BitmapToDIB(hBit, NULL);
            // BUGFIX - Maybe fix crash
            if (!hDib) {
               MessageBox (hWnd, "BitmapToDIB() returns NULL.", "Error", MB_OK);
               fRet = FALSE;
               goto done;
            }

            //      hBitOld = (HBITMAP) SelectObject (hDCMem, hBit);
            // try dib
            BITMAPINFOHEADER  *pi;
            pi = (BITMAPINFOHEADER*) GlobalLock (hDib);
            if (!pi) {
               MessageBox (hWnd, "GlobalLock (hd) returns NULL.", "Error", MB_OK);
               fRet = FALSE;
               goto done;
            }
#ifdef DEBUGPRINTING
            swprintf (szwTemp, L"\r\n\tStretchDIBits %d, %d, %d, %d, %d, %d",
               (int)iXOff, (int)iYOff, (int)iWidth, (int)iHeight, (int)dwX, (int) dwY);
            EscOutputDebugString (szwTemp);
#endif
            if (GDI_ERROR == StretchDIBits(  hDCPrint,                // handle to device context
               iXOff, iYOff, iWidth, iHeight,
               0, 0, dwX, dwY,
               (LPSTR)pi +(WORD)pi->biSize,
               (BITMAPINFO*) pi,
               DIB_RGB_COLORS, 
               SRCCOPY)) {
                  MessageBox(hWnd, 
                    "StretchDIBits() failed.", NULL, 
                    MB_OK);
            }

            // draw an outline around the image
            if (fBig) {
               hPenOld = (HPEN) SelectObject (hDCPrint, GetStockObject (BLACK_PEN));

               int   iBottom, iRight;
               iRight = min(iPixX - 1, iWidth+iXOff);
               iBottom = min(iPixY - 1, iHeight+iYOff);

               MoveToEx (hDCPrint, 1, 1, NULL);
               LineTo (hDCPrint, iRight, 1);
               LineTo (hDCPrint, iRight, iBottom);
               LineTo (hDCPrint, 1, iBottom);
               LineTo (hDCPrint, 1, 1);

               SelectObject (hDCPrint, hPenOld);
            }

            // draw the grid?
            if (fGrid && (gfGrid != 0)) {
               double   fDeltaX, fDeltaY;
               if (gfGrid > 0) {
                  fDeltaX = (double) iWidth / (double) gfWidth * gfGrid;
                  fDeltaY = (double) iHeight / (double) gfHeight * gfGrid;
               }
               else {
                  fDeltaX = (double) iWidth / (-gfGrid);
                  fDeltaY = (double) iHeight / (-gfGrid);
               }

               // BUGFIX - Offset
               double fOffsetX, fOffsetY;
               fOffsetX = fOffsetY = 0;
               if (gfKnowGridArt && (gfGrid > 0)) {
                  double f;
                  f = fmod(gfWidth, gfGrid);
                  if ((f > .01) && (f < gfGrid-.01))
                     fOffsetX = -(fDeltaX - f / 2.0);
                  f = fmod(gfHeight, gfGrid);
                  if ((f > .01) && (f < gfGrid-.01))
                     fOffsetY = -(fDeltaY - f / 2.0);
               }

               // BUGFIX - cant see lines on black image
               double   fPos;
               SetBkColor (hDCPrint, RGB(255,255,255));
               int   iOldMode;
               iOldMode = SetBkMode (hDCPrint, OPAQUE);

               // BUGFIX - Create some thick pens
               int iThick;
               iThick = min(iPixX, iPixY) / 512;
               iThick = max(iThick, 2);
               HPEN hBlack, hWhite;
               hBlack = CreatePen (PS_SOLID, iThick, 0);
               hWhite = CreatePen (PS_SOLID, iThick, RGB(0xff,0xff,0xff));

               for (fPos = fDeltaX + fOffsetX; fPos < iWidth; fPos += fDeltaX) {
                  // black pen
                  hPenOld = (HPEN) SelectObject (hDCPrint, hBlack);
                  MoveToEx (hDCPrint, (int) fPos + iXOff - iThick/2, iYOff, NULL);
                  LineTo (hDCPrint, (int) fPos + iXOff - iThick/2, iYOff + iHeight);

                  // white pen next to it
                  SelectObject (hDCPrint, hWhite);
                  MoveToEx (hDCPrint, (int) fPos + iXOff + iThick/2, iYOff, NULL);
                  LineTo (hDCPrint, (int) fPos + iXOff + iThick/2, iYOff + iHeight);

                  SelectObject (hDCPrint, hPenOld);
               }
               for (fPos = fDeltaY + fOffsetY; fPos < iHeight; fPos += fDeltaY) {
                  // black pen
                  hPenOld = (HPEN) SelectObject (hDCPrint, hBlack);
                  MoveToEx (hDCPrint, iXOff, (int) fPos + iYOff - iThick/2, NULL);
                  LineTo (hDCPrint, iXOff + iWidth, (int) fPos + iYOff - iThick/2);

                  // white pen
                  SelectObject (hDCPrint, hWhite);
                  MoveToEx (hDCPrint, iXOff, (int) fPos + iYOff + iThick/2, NULL);
                  LineTo (hDCPrint, iXOff + iWidth, (int) fPos + iYOff + iThick/2);

                  SelectObject (hDCPrint, hPenOld);
               }
               DeleteObject (hBlack);
               DeleteObject (hWhite);
               SetBkMode (hDCPrint, iOldMode);
            }

            // unselect bitmap and end the page
      //      SelectObject (hDCMem, hBitOld);
            if (EndPage(hDCPrint) <= 0) {
               MessageBox(hWnd, 
                 "EndPage() failed.", NULL, 
                 MB_OK);
               fRet = FALSE;
               goto done;
            }
            if (hDib)
               GlobalUnlock (hDib);
         }
      }  // end loop over pages

      DeleteObject (hBit);
      if (hDib)
         GlobalFree (hDib);
      hDib = NULL;

   }

#ifdef DEBUGPRINTING
   EscOutputDebugString (L"\r\nDonePrint");
#endif

   // color key
   if (fBaseKey) {
      // loop through and find which colors are used how many times
      DWORD adwCount[4000];   // large #
      DWORD xMax, yMax, x, y;
      int iOldMode;
      iOldMode = SetBkMode (hDCPrint, TRANSPARENT);
      xMax = gImageCutout.DimX();
      yMax = gImageCutout.DimY();

      memset (adwCount, 0, sizeof(adwCount));
      for (y = 0; y < yMax; y++) for (x = 0; x < xMax; x++)
         if (gawImageColor[y * xMax + x] < sizeof(adwCount)/sizeof(DWORD)) // BUGFIX - Just in case overwriting
            adwCount[gawImageColor[y * xMax + x]]++;

      // estimate size of text
      char  szString[] = "XXXX";
      SIZE  s;
      // BUGFIX - Select font around text params because of bug report with wierd printer
      hFontOld = (HFONT) SelectObject (hDCPrint, hFont);
      GetTextExtentPoint32(hDCPrint, szString, strlen(szString), &s);
      SelectObject (hDCPrint, hFontOld);

      // print it out
      BOOL  fHavePage = FALSE;
      int   iX;      // current x location
      BOOL  fColumn; // if FALSE in left column, true in right
      PCOLOR   *p;

      // BUGFIX - put in min(gdwPColorSide, sizeof(adwCount)/sizeof(DWORD)) - just to make sure
      for (x = 0; x < min(gdwPColorSize,sizeof(adwCount)/sizeof(DWORD)); x++) {
         // if no colors of that type used then skip
         if (!adwCount[x])
            continue;

         // if this info would go beyond the edge of the page then finished
         if (fHavePage && ((iX + 2 * s.cy) > (iPixY - iYOrigin - 2*s.cy))) {

            if (fColumn) {
               // end of the second column
               EndPage (hDCPrint);
               fHavePage = FALSE;
            }
            else {
               // end of the first column. Start at top
               fColumn = TRUE;
               iX = iXOrigin;
            }
         }

         // if no page then create one
         if (!fHavePage) {
            StartPage (hDCPrint);
            iX = iXOrigin;
            fColumn = 0;
            fHavePage = TRUE;
            // BUGFIX - Select font around text params because of bug report with wierd printer
            hFontOld = (HFONT) SelectObject (hDCPrint, hFont);
            GetTextExtentPoint32(hDCPrint, szString, strlen(szString), &s);
            SelectObject (hDCPrint, hFontOld);

            // BUGFIX - On one printer it's showing an all-black background, so fill white
            RECT rAll;
            rAll.left = rAll.top = 0;
            rAll.right = iPixX;
            rAll.bottom = iPixY;
            HBRUSH hWhite;
            hWhite = CreateSolidBrush (RGB(0xff, 0xff, 0xff));
            FillRect (hDCPrint, &rAll, hWhite);
            DeleteObject (hWhite);

            // BUGFIX - at the bottom of every page print the file and the date
            hFontOld = (HFONT) SelectObject (hDCPrint, hFont);
            char  szTemp[256];
            SIZE sFooter;
            SYSTEMTIME st;
            memset (&sFooter, 0, sizeof(sFooter));
            GetLocalTime (&st);
            sprintf (szTemp, "%s - %d/%d/%d", gszFileName, (int) st.wDay, (int)st.wMonth, (int) st.wYear);
            // BUGFIX - On one printer it's an all black backgroun with white text
            SetTextColor (hDCPrint, RGB(0,0,0));
            SetBkColor (hDCPrint, RGB(255,255,255));
            GetTextExtentPoint32(hDCPrint, szTemp, strlen(szTemp), &sFooter);
            TextOut(hDCPrint, ((int)iPixX - sFooter.cx) / 2, iPixY - iYOrigin - sFooter.cy,
               szTemp, strlen(szTemp));
      
            SelectObject (hDCPrint, hFontOld);
         }

         // else used, so print
         p = gpPColor + x;

         // colorregion
         RECT  r;
         r.left = fColumn ? (iPixX / 2) : iXOrigin;
         r.right = r.left + s.cx;
         r.top = iX;
         r.bottom = r.top + s.cy;
         HBRUSH   hBrush;
         hBrush = CreateSolidBrush (gImageColors.UnGamma (p->r, p->g, p->b));
         FillRect (hDCPrint, &r, hBrush);
         DeleteObject (hBrush);

         // BUGFIX - On one printer it's an all black backgroun with white text
         SetTextColor (hDCPrint, RGB(0,0,0));
         SetBkColor (hDCPrint, RGB(255,255,255));

         // BUGFIX - Take care of extra long names and do word wrap
         RECT rTest;
         rTest.left = r.right + s.cx / 4;
         rTest.right = r.left + (iPixX / 2 - iXOrigin) / 10 * 9;
         rTest.top = r.top;
         rTest.bottom = rTest.top + iPixX;   // a large number
         int iHeight;

         // label
         // BUGFIX - Select font around text params because of bug report with wierd printer
         hFontOld = (HFONT) SelectObject (hDCPrint, hFont);
         //TextOut(hDCPrint, r.right + s.cx / 4, r.top,
         //   p->szName, strlen(p->szName));
         iHeight = DrawText (hDCPrint, p->szName, -1, &rTest, DT_LEFT | DT_NOPREFIX | DT_WORDBREAK);
         r.bottom = max (r.bottom, rTest.top + iHeight);
         SelectObject (hDCPrint, hFontOld);

         // move on
         iX = r.bottom + s.cy / 2;  // some spacing


      }

      SetBkMode (hDCPrint, iOldMode);

      if (fHavePage)
         EndPage (hDCPrint);
   }

   // end the document
   EndDoc (hDCPrint);

done:
   if (hPen)
      DeleteObject (hPen);
//   if (hBitOld)
//      SelectObject (hDCMem, hBitOld);
   if (hDCMem)
      DeleteDC (hDCMem);
   if (hDCPrint)
      DeleteDC (hDCPrint);
   if (hFont) {
      DeleteObject (hFont);
      hFont = NULL;
   }

   return fRet;
}

/*****************************************************************************
PrintPage
*/
BOOL PrintPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   static DWORD gdwTimerID = 0;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         PCEscControl pControl;

         // if base colors were skipped then dont show base
         BOOL  fSkip = (gdwMix == 3);

         gdwTimerID = 0;

         if (pControl = pPage->ControlFind (L"entire"))
            pControl->AttribSetBOOL (gszChecked, FALSE);
         if (pControl = pPage->ControlFind (L"canvas"))
            pControl->AttribSetBOOL (gszChecked, TRUE);
         if (pControl = pPage->ControlFind (L"canvasgrid"))
            pControl->AttribSetBOOL (gszChecked, TRUE);
         if (pControl = pPage->ControlFind (L"base")) {
            pControl->AttribSetBOOL (gszChecked, !fSkip);
            if (fSkip)
               pControl->Enable(FALSE);
         }
         if (pControl = pPage->ControlFind (L"basegrid")) {
            pControl->AttribSetBOOL (gszChecked, !fSkip);
            if (fSkip)
               pControl->Enable(FALSE);
         }
         if (pControl = pPage->ControlFind (L"key")) {
            pControl->AttribSetBOOL (gszChecked, !fSkip);
            if (fSkip)
               pControl->Enable(FALSE);
         }

      }
      break;   // default

   case ESCM_CLOSE:
      if (!AskSave (pPage, VIEWMODE_PRINT))
         return TRUE;   // abort cancel
      break;   // default

#if 0 // BUGFIX - taje out because added "printing completed" mb
   case ESCM_TIMER:
      {
         PESCMTIMER p = (PESCMTIMER) pParam;
         if (p->dwID != gdwTimerID)
            break;   // not this timer

         pPage->m_pWindow->TimerKill (gdwTimerID);
         gdwTimerID = NULL;
         pPage->Exit (gszNext);
      }
      break;
#endif

   case ESCM_LINK:
      {
         PESCMLINK p = (PESCMLINK) pParam;
         if (p->psz && !_wcsicmp(p->psz, gszNext)) {
            PCEscControl pControl;
            BOOL fEntire = FALSE, fCanvas = FALSE, fCanvasGrid = FALSE,
               fBase = FALSE, fBaseGrid = FALSE, fKey = FALSE,
               fCanvasBig = FALSE, fBaseBig = FALSE, fInvert = FALSE;

            if (pControl = pPage->ControlFind (L"entire"))
               fEntire = pControl->AttribGetBOOL (gszChecked);
            if (pControl = pPage->ControlFind (L"canvas"))
               fCanvas = pControl->AttribGetBOOL (gszChecked);
            if (pControl = pPage->ControlFind (L"canvasgrid"))
               fCanvasGrid = pControl->AttribGetBOOL (gszChecked);
            if (pControl = pPage->ControlFind (L"canvasbig"))
               fCanvasBig = pControl->AttribGetBOOL (gszChecked);
            if (pControl = pPage->ControlFind (L"base"))
               fBase = pControl->AttribGetBOOL (gszChecked);
            if (pControl = pPage->ControlFind (L"basegrid"))
               fBaseGrid = pControl->AttribGetBOOL (gszChecked);
            if (pControl = pPage->ControlFind (L"basebig"))
               fBaseBig = pControl->AttribGetBOOL (gszChecked);
            if (pControl = pPage->ControlFind (L"key"))
               fKey = pControl->AttribGetBOOL (gszChecked);
            if (pControl = pPage->ControlFind (L"invert"))
               fInvert = pControl->AttribGetBOOL (gszChecked);

#ifndef DEBUGPRINTING   // so can test
            if ((gdwRegCount > 35) && !RegisterIsRegistered()) {
               pPage->MBWarning (L"Because you have not registered your copy of Oil Painting Assistant "
                  L"yet (after running it 35 times) it will not print your images.");
               fEntire = fCanvas = fCanvasGrid = fBase = fBaseGrid = FALSE;
            }
#endif

            if (!PrintImages (pPage->m_pWindow->m_hWnd, fEntire, fCanvas, fCanvasGrid,
               fCanvasBig, fBase, fBaseGrid, fBaseBig, fKey, fInvert))
               return TRUE;   // printed cancel

            if (fEntire | fCanvas | fBase | fKey) {
               // BUGFIX - User reporting a crash after print. so, try to put a pause in the
               // page and not end it right away

               // BUGFIX - Take out because added printing finished message
               //gdwTimerID = pPage->m_pWindow->TimerSet (100, pPage);

               pPage->MBInformation (L"Printing finished.");

               // BUGFIX - Take out because added printing finished message
               //return TRUE;   // cancel link
            }
         }
      }
      break;

   };


   return DefPage (pPage, dwMessage, pParam);
}



/***********************************************************************
UserScrolls - called when the user scrolls the horizontal or vertical
scroll bars

inputs
   BOOL        fHorz - true if horizontal
   int         nScrollCode - scroll code
   short       nPos - position
   HWND        hWnd - window of scroll
returns
   none
*/
void UserScrolls (BOOL fHorz, int nScrollCode, short nPos, HWND hWnd)
{
   // get the current position
   SCROLLINFO  si;
   memset (&si, 0, sizeof(si));
   si.cbSize = sizeof(si);
   si.fMask = SIF_ALL;
   GetScrollInfo (hWnd, fHorz ? SB_HORZ : SB_VERT, &si);

   // switch nScrollCode
   switch (nScrollCode) {
   case SB_BOTTOM:
      si.nPos = si.nMax;
      break;
   case SB_ENDSCROLL:
      // ignore end-scroll since really doesn't
      // make a differnet
      return;
   case SB_LINEDOWN:
      si.nPos += max(1, si.nPage / 16);
      break;
   case SB_LINEUP:
      si.nPos -= max(1, si.nPage / 16);
      break;
   case SB_PAGEDOWN:
      si.nPos += (si.nPage ? si.nPage : ((si.nMax - si.nMin)/16));
      break;
   case SB_PAGEUP:
      si.nPos -= (si.nPage ? si.nPage : ((si.nMax - si.nMin)/16));
      break;
   case SB_THUMBPOSITION:
   case SB_THUMBTRACK:
      si.nPos = si.nTrackPos;
      break;

   case SB_TOP:
   //case SB_LEFT:
   //case SB_RIGHT:
   //case SB_LINELEFT:
   //case SB_LINERIGHT:
   //case SB_PAGELEFT:
   //case SB_PAGERIGHT:
      break;
   }

   si.nPos = max(si.nMin, si.nPos);
   si.nPos = min(si.nPos, si.nMax - max((int)si.nPage-1,0));

   // set it
   SetScrollPos(hWnd, fHorz ? SB_HORZ : SB_VERT, si.nPos, TRUE);

   // move image
   if (fHorz)
      giLeftX = -si.nPos;
   else
      giTopY = -si.nPos;
   InvalidateRect (hWnd, NULL, FALSE);
   return;
}


/******************************************************************************
SelectionHitTest - Given mouse pointer in the client region when the selection
is visible, this returns if the selection is over one of the drag areas and
an offset from the "base" of the drag section.

inputs
   POINT    p - mouse position
   POINT    &pDelta - Filled with the delta
returns
   DWORD    - one of the following
*/
#define  SHT_NOTHING    0
#define  SHT_LEFTEDGE   1
#define  SHT_RIGHTEDGE  2
#define  SHT_TOPEDGE    3
#define  SHT_BOTTOMEDGE 4
#define  SHT_MIDDLE     5

#define  EDGE           5

DWORD SelectionHitTest (POINT p, POINT *pDelta)
{
   pDelta->x = pDelta->y = 0;

   // figure out height
   int   giViewCanvasHeight, giViewCanvasRight, giViewCanvasBottom;
   giViewCanvasHeight = (int) (giViewCanvasWidth * gfHeight / gfWidth);
   giViewCanvasRight = giViewCanvasLeft + giViewCanvasWidth;
   giViewCanvasBottom = giViewCanvasTop + giViewCanvasHeight;

   // location on the screen
   RECT rs;
   rs.left = giLeftX + (int) (giViewCanvasLeft * gfScale);
   rs.right = giLeftX + (int) ((giViewCanvasLeft + giViewCanvasWidth) * gfScale);
   rs.top = giTopY + (int) (giViewCanvasTop * gfScale);
   rs.bottom = giTopY + (int) ((giViewCanvasTop + giViewCanvasHeight) * gfScale);

   // outside?
   if ( ((p.x < rs.left - EDGE) || (p.x > rs.right + EDGE)) ||
      ((p.y < rs.top - EDGE) || (p.y > rs.bottom + EDGE)) )
      return SHT_NOTHING;

   pDelta->x = p.x;
   pDelta->y = p.y;

   // left?
   if (p.x < rs.left + EDGE) {
      return SHT_LEFTEDGE;
   }
   if (p.x > rs.right - EDGE) {
      return SHT_RIGHTEDGE;
   }

   // top/bottom
   if (p.y < rs.top + EDGE) {
      return SHT_TOPEDGE;
   }
   if (p.y > rs.bottom - EDGE) {
      return SHT_BOTTOMEDGE;
   }

   // else in center
   return SHT_MIDDLE;
}

/******************************************************************************
MainWndProc
*/
long CALLBACK MainWndProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{

   switch (uMsg) {
   case WM_CREATE:
      gfScale = 1.0;
      giLeftX = giTopY = 0;
      gdwHitTest = SHT_NOTHING;


      RECT  r;
      GetClientRect (hWnd, &r);
      // BUGFIX - Make sure fits entire image
      while (gfScale * gdwBitViewX * sqrt(2.0) < r.right)
         gfScale *= sqrt(2.0);
      while ((gfScale * gdwBitViewX > r.right) || (gfScale * gdwBitViewY > r.bottom))
         gfScale /= sqrt(2.0);

      // adjust the scroll bars
      SendMessage (hWnd, WM_USER+182, 0, 0);

      return 0;

   case WM_USER+182: // recalc scroll bars
      {
         // find the min/max that have to be allowed to scroll
         RECT  r;
         GetClientRect (hWnd, &r);

         SCROLLINFO si;
         memset (&si, 0, sizeof(si));
         si.cbSize = sizeof(si);
         si.fMask = SIF_ALL | SIF_DISABLENOSCROLL;
         si.nMin = 0;
         si.nMax = (int) (gdwBitViewX * gfScale);
#if 0
         si.nMin = -(int) (gdwBitViewX * gfScale);
         si.nMax = (int) (gdwBitViewX * gfScale) * 2 + r.right;
#endif
         si.nPage = r.right;
         si.nPos = -giLeftX;
         SetScrollInfo (hWnd, SB_HORZ, &si, TRUE);

         si.nMax = (int) (gdwBitViewY * gfScale);
#if 0
         si.nMin = -(int) (gdwBitViewY * gfScale);
         si.nMax = (int) (gdwBitViewY * gfScale) * 2 + r.bottom;
#endif
         si.nPage = r.bottom;
         si.nPos = -giTopY;
         SetScrollInfo (hWnd, SB_VERT, &si, TRUE);
      }
      return 0;

   case WM_HSCROLL:
   case WM_VSCROLL:
      UserScrolls ((uMsg == WM_HSCROLL), (int) LOWORD(wParam), (short) HIWORD(wParam), hWnd);
      return 0;

   case WM_SIZE:
      // adjust the scroll bars
      SendMessage (hWnd, WM_USER+182, 0, 0);
      break;

   case WM_CLOSE:
      if (!AskSave (hWnd, gdwViewMode))
         return 0;
      giViewRet = 0;
      break;   // default actions

   case WM_MOUSEMOVE:
   case WM_LBUTTONDOWN:
   case WM_LBUTTONUP:
      {
         // get location
         POINTS ps;
         POINT p;
         ps = MAKEPOINTS (lParam);
         p.x = ps.x;
         p.y = ps.y;

         // hittest
         DWORD dwHit;
         POINT pDelta;
         dwHit = SelectionHitTest (p, &pDelta);

         // set caret
         switch (dwHit) {
         case SHT_LEFTEDGE:
            SetCursor (LoadCursor (NULL, IDC_SIZEWE));
            break;
         case SHT_RIGHTEDGE:
            SetCursor (LoadCursor (NULL, IDC_SIZEWE));
            break;
         case SHT_TOPEDGE:
            SetCursor (LoadCursor (NULL, IDC_SIZENS));
            break;
         case SHT_BOTTOMEDGE:
            SetCursor (LoadCursor (NULL, IDC_SIZENS));
            break;
         case SHT_MIDDLE:
            SetCursor (LoadCursor (ghInstance, MAKEINTRESOURCE(IDC_MYHAND)));
            break;
         default:
            SetCursor (LoadCursor (NULL, IDC_NO));
            break;
         }

         // if button down then capture
         if (uMsg == WM_LBUTTONDOWN) {
            if (dwHit == SHT_NOTHING)
               return 0;
            SetCapture (hWnd);

            gdwHitTest = dwHit;
            gpDelta = pDelta;
            giOrigLeft = giViewCanvasLeft;
            giOrigTop = giViewCanvasTop;
            giOrigWidth = giViewCanvasWidth;
            return 0;
         }

         // if we're not captured then return
         if (gdwHitTest == SHT_NOTHING)
            return 0;

         pDelta.x = p.x - gpDelta.x;
         pDelta.y = p.y - gpDelta.y;

         // move the selection
         switch (gdwHitTest) {
         case SHT_LEFTEDGE:
            giViewCanvasLeft = giOrigLeft + (int) (pDelta.x / gfScale);
            giViewCanvasWidth = giOrigWidth - (int) (pDelta.x / gfScale);
            if (giViewCanvasWidth < 1)
               giViewCanvasWidth = 1;
            break;
         case SHT_RIGHTEDGE:
            giViewCanvasWidth = giOrigWidth + (int) (pDelta.x / gfScale);
            if (giViewCanvasWidth < 1)
               giViewCanvasWidth = 1;
            break;
         case SHT_TOPEDGE:
            giViewCanvasTop = giOrigTop + (int) (pDelta.y / gfScale);
            giViewCanvasWidth = giOrigWidth - (int) (pDelta.y / gfScale / (double) gfHeight * (double) gfWidth);
            if (giViewCanvasWidth < 1)
               giViewCanvasWidth = 1;
            break;
         case SHT_BOTTOMEDGE:
            giViewCanvasWidth = giOrigWidth + (int) (pDelta.y / gfScale / (double) gfHeight * (double) gfWidth);
            if (giViewCanvasWidth < 1)
               giViewCanvasWidth = 1;
            break;
         case SHT_MIDDLE:
            giViewCanvasTop = giOrigTop + (int) (pDelta.y / gfScale);
            giViewCanvasLeft = giOrigLeft + (int) (pDelta.x / gfScale);
            break;
         }
         InvalidateRect (hWnd, NULL, FALSE);

         if (uMsg == WM_LBUTTONUP) {
            // done
            ReleaseCapture ();
            gdwHitTest = SHT_NOTHING;
            return 0;
         }
      }
      return 0;

   case WM_COMMAND:
      WORD  wVal;
      wVal = 0;

      switch (LOWORD(wParam)) {

      case ID_NEXT:
         giViewRet = 1;
         DestroyWindow (hWnd);
         return 0;

      case ID_BACK:
         giViewRet = -1;
         DestroyWindow (hWnd);
         return 0;

      case ID_ZOOM_IN:
      case ID_ZOOM_OUT:

         // remember which pixel is drawn in the center of the client
         int   iCenterX, iCenterY;
         RECT  r;
         GetClientRect (hWnd, &r);
         iCenterX = (int) (((r.right / 2) - giLeftX) / gfScale);
         iCenterY = (int) (((r.bottom / 2) - giTopY) / gfScale);

         // change scale
         if (LOWORD(wParam) == ID_ZOOM_IN)
            gfScale *= sqrt(2.0);
         else
            gfScale /= sqrt(2.0);
         if (gfScale > 4.0)
            gfScale = 4.0;
         if (gfScale < (1.0 / 4.0))
            gfScale = 1.0 / 4.0;

         // deal with offset
         giLeftX = (int) (r.right / 2 - iCenterX * gfScale);
         giTopY = (int) (r.bottom / 2 - iCenterY * gfScale);
         giLeftX = max (giLeftX, r.right - (int) (gdwBitViewX * gfScale));
         giLeftX = min (giLeftX, 0);
         giTopY = max (giTopY, r.bottom - (int) (gdwBitViewY * gfScale));
         giTopY = min (giTopY, 0);
#if 0
         giLeftX = min(giLeftX, r.right);
         giLeftX = max(giLeftX, -(int) (gdwBitViewX * gfScale));
         giTopY = max(giTopY, - (int) (gdwBitViewY * gfScale));
         giTopY = min(giTopY, r.bottom);
#endif

         // adjust the scroll bars
         SendMessage (hWnd, WM_USER+182, 0, 0);

         InvalidateRect (hWnd, NULL, FALSE);

         return 0;
      }
      break;

   case WM_PAINT:
      {
         PAINTSTRUCT ps;
         HDC   hDCWnd;
         hDCWnd = BeginPaint (hWnd, &ps);

         HDC hDC;
         hDC = CreateCompatibleDC (hDCWnd);
         HBITMAP hBitOld;
         hBitOld = (HBITMAP) SelectObject (hDC, ghBitView);

         // paint boxes around image
         RECT  r, rBox;
         GetClientRect (hWnd, &r);
         rBox = r;
         rBox.right = giLeftX;
         FillRect (hDCWnd, &rBox, (HBRUSH) (COLOR_BTNFACE+1));
         rBox = r;
         rBox.left = giLeftX + (int) (gdwBitViewX * gfScale);
         FillRect (hDCWnd, &rBox, (HBRUSH) (COLOR_BTNFACE+1));
         rBox = r;
         rBox.bottom = giTopY;
         FillRect (hDCWnd, &rBox, (HBRUSH) (COLOR_BTNFACE+1));
         rBox = r;
         rBox.top = giTopY + (int) (gdwBitViewY * gfScale);
         FillRect (hDCWnd, &rBox, (HBRUSH) (COLOR_BTNFACE+1));

         if (ghBitWhitened && gdwViewMode == VIEWMODE_CANVASLOCATION) {
            // selecting canvas, so need to draw out of two bitmaps
            HDC hDCFade;
            hDCFade = CreateCompatibleDC (hDCWnd);
            HBITMAP hBitOldFade;
            hBitOldFade = (HBITMAP) SelectObject (hDCFade, ghBitWhitened);

            // figure out height
            int   giViewCanvasHeight, giViewCanvasRight, giViewCanvasBottom;
            giViewCanvasHeight = (int) (giViewCanvasWidth * gfHeight / gfWidth);
            giViewCanvasRight = giViewCanvasLeft + giViewCanvasWidth;
            giViewCanvasBottom = giViewCanvasTop + giViewCanvasHeight;

            // location on the screen
            RECT rs;
            rs.left = giLeftX + (int) (giViewCanvasLeft * gfScale);
            rs.right = giLeftX + (int) ((giViewCanvasLeft + giViewCanvasWidth) * gfScale);
            rs.top = giTopY + (int) (giViewCanvasTop * gfScale);
            rs.bottom = giTopY + (int) ((giViewCanvasTop + giViewCanvasHeight) * gfScale);

            // draw final image. clip so don't do entire thing
            HRGN hRgnOld;
            hRgnOld = CreateRectRgn (-10000, -10000, 10000, 10000);
            GetClipRgn(hDCWnd, hRgnOld);
            IntersectClipRect (hDCWnd, rs.left, rs.top, rs.right, rs.bottom);
            SetStretchBltMode (hDCWnd, COLORONCOLOR);
            StretchBlt (hDCWnd,
               giLeftX, giTopY, (int) (gdwBitViewX * gfScale), (int) (gdwBitViewY * gfScale),
               hDC,
               0, 0, gdwBitViewX, gdwBitViewY,
               SRCCOPY);
            // undo the clipping
            SelectClipRgn (hDCWnd, hRgnOld);
            DeleteObject (hRgnOld);

            hRgnOld = CreateRectRgn (-10000, -10000, 10000, 10000);
            GetClipRgn(hDCWnd, hRgnOld);
            ExcludeClipRect (hDCWnd, rs.left, rs.top, rs.right, rs.bottom);
            SetStretchBltMode (hDCWnd, COLORONCOLOR);
            StretchBlt (hDCWnd,
               giLeftX, giTopY, (int) (gdwBitViewX * gfScale), (int) (gdwBitViewY * gfScale),
               hDCFade,
               0, 0, gdwBitViewX, gdwBitViewY,
               SRCCOPY);
            // undo the clipping
            SelectClipRgn (hDCWnd, hRgnOld);
            DeleteObject (hRgnOld);
#if 0
            SetStretchBltMode (hDCWnd, COLORONCOLOR);
            // draw a left side if there is one
            if (giViewCanvasLeft > 0)
               StretchBlt (hDCWnd,
                  giLeftX, giTopY, (int) (giViewCanvasLeft * gfScale), (int) (gdwBitViewY * gfScale),
                  hDCFade,
                  0, 0, giViewCanvasLeft, gdwBitViewY,
                  SRCCOPY);
            // right side
            if (giViewCanvasRight < (int) gdwBitViewX)
               StretchBlt (hDCWnd,
                  giLeftX + (int) (giViewCanvasRight * gfScale), giTopY, (int) (((int)gdwBitViewX - giViewCanvasRight) * gfScale), (int) (gdwBitViewY * gfScale),
                  hDCFade,
                  giViewCanvasRight, 0, (int) gdwBitViewX - giViewCanvasRight, gdwBitViewY,
                  SRCCOPY);
            // top
            if (giViewCanvasTop > 0)
               StretchBlt (hDCWnd,
                  giLeftX, giTopY, (int) (gdwBitViewX * gfScale), (int) (giViewCanvasTop * gfScale),
                  hDCFade,
                  0, 0, gdwBitViewX, giViewCanvasTop,
                  SRCCOPY);
            // bottom
            if (giViewCanvasBottom < (int) gdwBitViewY)
               StretchBlt (hDCWnd,
                  giLeftX, giTopY + (int) (giViewCanvasBottom * gfScale), (int) (gdwBitViewX * gfScale), (int) (((int)gdwBitViewY - giViewCanvasBottom) * gfScale),
                  hDCFade,
                  0, giViewCanvasBottom, gdwBitViewX, (int) gdwBitViewY - giViewCanvasBottom,
                  SRCCOPY);
#endif // 0

            // draw lines around it
            HPEN  hPen, hOld;
            hPen = CreatePen (PS_DOT, 1, RGB(0,0,0));
            hOld = (HPEN) SelectObject (hDCWnd, hPen);
            MoveToEx (hDCWnd, rs.left, rs.top, NULL);
            LineTo (hDCWnd, rs.right, rs.top);
            LineTo (hDCWnd, rs.right, rs.bottom);
            LineTo (hDCWnd, rs.left, rs.bottom);
            LineTo (hDCWnd, rs.left, rs.top);
            SelectObject (hDCWnd, hOld);
            DeleteObject (hPen);

            // free up
            SelectObject (hDCFade, hBitOldFade);
            DeleteDC(hDCFade);
         }
         else {
            SetStretchBltMode (hDCWnd, COLORONCOLOR);
            StretchBlt (hDCWnd,
               giLeftX, giTopY, (int) (gdwBitViewX * gfScale), (int) (gdwBitViewY * gfScale),
               hDC,
               0, 0, gdwBitViewX, gdwBitViewY,
               SRCCOPY);
         }

         // free up
         SelectObject (hDC, hBitOld);
         DeleteDC(hDC);

         EndPaint (hWnd, &ps);
      }
      return 0;

   case WM_DESTROY:
      {
         // restore
         WINDOWPLACEMENT wp;
         memset (&wp, 0, sizeof(wp));
         wp.length = sizeof(wp);
         GetWindowPlacement (hWnd, &wp);
         SetWindowPlacement (gpWindow->m_hWnd, &wp);
      }
      // that's all
      return 0;
   }

   // else
   return DefWindowProc (hWnd, uMsg, wParam, lParam);
}




void RegisterWindow (void)
{
   WNDCLASS wc;

   memset (&wc, 0, sizeof(wc));
   wc.lpfnWndProc = MainWndProc;
   wc.style = CS_HREDRAW | CS_VREDRAW;
   wc.hInstance = ghInstance;
   wc.hIcon = LoadIcon (ghInstance, MAKEINTRESOURCE(IDI_APPICON));
   wc.hCursor = NULL;
   wc.hbrBackground = NULL;
   wc.lpszMenuName = MAKEINTRESOURCE (IDR_MAINMENU);
   wc.lpszClassName = gszMainClass;
   RegisterClass (&wc);
}




/*****************************************************************************
PaintNamePage
*/
BOOL PaintNamePage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PWSTR psz = (PWSTR) pPage->m_pUserData;

   switch (dwMessage) {
   case ESCM_LINK:
      {
         PESCMLINK p = (PESCMLINK) pParam;

         if (p->psz && !_wcsicmp(p->psz, L"ok")) {
            // save the text away
            DWORD dwNeeded;
            PCEscControl pControl = pPage->ControlFind (gszText);
            if (pControl)
               pControl->AttribGet (gszText, psz, 128, &dwNeeded);

            if (!iswalpha(psz[0])) {
               pPage->MBWarning (L"You must start the name with a letter.");
               return TRUE;
            }
        }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}
/*****************************************************************************
PaintMML - Appends some MML onto gMemTemp given the paint color and name.
This is used for the list box.

inputs
   char     *psz - string for paint name
   COLORREF cr - color
*/
void PaintMML (char *psz, COLORREF cr)
{
   WCHAR szName[256], szColor[32];
   MultiByteToWideChar (CP_ACP, 0, psz, -1, szName, sizeof(szName)/2);
   ColorToAttrib (szColor, cr);
   MemCat (&gMemTemp, L"<elem name=\"");
   MemCatSanitize (&gMemTemp, szName);
   MemCat (&gMemTemp, L"\"><colorblend color=");
   MemCat (&gMemTemp, szColor);
   MemCat (&gMemTemp, L" width=32 height=32/> ");
   MemCatSanitize (&gMemTemp, szName);
   MemCat (&gMemTemp, L"</elem>");
}

/*****************************************************************************
CatalogPaintPage
*/
BOOL CatalogPaintPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   switch (dwMessage) {
   case ESCM_LINK:
      {
         PESCMLINK p = (PESCMLINK)pParam;

         if (!p->psz)
            break;

         if (!_wcsicmp(p->psz, L"ok")) {
            // get the text of the selected color
            PCEscControl pControl;
            pControl = pPage->ControlFind (gszPallette);
            int iSel;
            iSel = pControl->AttribGetInt (gszCurSel);
            ESCMLISTBOXGETITEM gi;
            memset (&gi, 0, sizeof(gi));
            gi.dwIndex = (DWORD) iSel;
            pControl->Message (ESCM_LISTBOXGETITEM, &gi);
            char  szTemp[256];
            szTemp[0] = 0;
            if (gi.pszName)
               WideCharToMultiByte (CP_ACP, 0, gi.pszName, -1, szTemp, sizeof(szTemp), 0,0);

            // see what it matches
            DWORD i;
            for (i = 0; i < (sizeof(gaCatColor) / sizeof(CATCOLOR)); i++)
               if (!_stricmp(gaCatColor[i].pszColor, szTemp))
                  break;
            if (i >=(sizeof(gaCatColor) / sizeof(CATCOLOR)))
               return TRUE;

            // add to registry
            HKEY  hKey = NULL;
            DWORD dwDisp;
            RegCreateKeyEx (HKEY_CURRENT_USER, gszRegPaintKey, 0, 0, REG_OPTION_NON_VOLATILE,
               KEY_READ | KEY_WRITE, NULL, &hKey, &dwDisp);
            DWORD dw;
            if (hKey) {
               dw = gaCatColor[i].rgb;
               RegSetValueEx (hKey, szTemp, 0, REG_DWORD, (BYTE*) &dw, sizeof(dw));

               RegCloseKey (hKey);
            }

            // add to combobox
            pControl = gpPagePaints->ControlFind (gszPallette);
            MemZero (&gMemTemp);
            PaintMML (szTemp, dw);
            ESCMLISTBOXADD add;
            memset (&add, 0, sizeof(add));
            add.dwInsertBefore = (DWORD)-1;
            add.pszMML = (PWSTR) gMemTemp.p;
            pControl->Message (ESCM_LISTBOXADD, &add);

            // select it
            WCHAR szName[256];
            MultiByteToWideChar (CP_ACP, 0, szTemp, -1, szName, sizeof(szName)/2);
            ESCMLISTBOXSELECTSTRING ss;
            memset (&ss, 0, sizeof(ss));
            ss.iStart = -1;
            ss.fExact = TRUE;
            ss.psz = szName;
            pControl->Message (ESCM_LISTBOXSELECTSTRING, &ss);

            break;   // exit
         }
      }
      break;

   case ESCM_INITPAGE:
      {
         PCEscControl pControl;
         pControl = pPage->ControlFind (gszPallette);
         MemZero (&gMemTemp);

         // enum all the paints
         DWORD i;
         for (i = 0; i < (sizeof(gaCatColor) / sizeof(CATCOLOR)); i++) {
            PaintMML (gaCatColor[i].pszColor, gaCatColor[i].rgb);
         }

         // set list box
         if (pControl) {
            ESCMLISTBOXADD add;
            memset (&add, 0, sizeof(add));
            add.dwInsertBefore = (DWORD)-1;
            add.pszMML = (PWSTR) gMemTemp.p;
            pControl->Message (ESCM_LISTBOXADD, &add);
            pControl->AttribSetInt (gszCurSel, 0);
         }

      }
      break;   // default


   };


   return DefPage (pPage, dwMessage, pParam);
}


/*****************************************************************************
EnumeratePaints - Load in all the paints used (from the registry) into
a global so that the ColorSpaceSubst() can use it.
*/

void EnumeratePaints (void)
{

   SPACEPAINT sp;

   gListSPACEPAINT.Init (sizeof(SPACEPAINT));
   gListSPACEPAINT.Clear ();

   // enumerate the registry
   HKEY  hKey = NULL;
   RegOpenKeyEx (HKEY_CURRENT_USER, gszRegPaintKey, 0, KEY_READ, &hKey);
   DWORD j;
   if (hKey) {
      // enumerate
      COLORREF dwRGB;
      DWORD dwNameSize, dwType, i, dwSize;
      for (i = 0; ; i++) {
         dwNameSize = sizeof(sp.szName);
         dwSize = sizeof(dwRGB);
         if (RegEnumValue (hKey, i, sp.szName, &dwNameSize, NULL, &dwType, (PBYTE) &dwRGB, &dwSize))
            break;

         sp.rgb = dwRGB;
         sp.awRGB[0] = gImageMain.Gamma(GetRValue(dwRGB));
         sp.awRGB[1] = gImageMain.Gamma(GetGValue(dwRGB));
         sp.awRGB[2] = gImageMain.Gamma(GetBValue(dwRGB));
         sp.fShow = TRUE;


         ToXYZ (sp.awRGB[0], sp.awRGB[1], sp.awRGB[2], &sp.aiXYZ[0], &sp.aiXYZ[1], &sp.aiXYZ[2]);

         // and to doubles
         for (j = 0; j < 3; j++)
            sp.afXYZ[j] = sp.aiXYZ[j];

         gListSPACEPAINT.Add(&sp);
      }

      RegCloseKey (hKey);
   }

}

/*****************************************************************************
ColorSpacePage
*/
BOOL ColorSpacePage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         PCEscControl pControl;

         if (pControl = pPage->ControlFind (L"showpaints"))
            pControl->AttribSetBOOL (gszChecked, gfSpaceShowPaints);
         if (pControl = pPage->ControlFind (L"showpaintssolid"))
            pControl->AttribSetBOOL (gszChecked, gfSpaceShowPaintsSolid);
         if (pControl = pPage->ControlFind (L"showpaintsshading"))
            pControl->AttribSetBOOL (gszChecked, gfSpaceShowPaintsShading);
         if (pControl = pPage->ControlFind (L"showCoverage"))
            pControl->AttribSetBOOL (gszChecked, gfSpaceShowCoverage);
         if (pControl = pPage->ControlFind (L"showCoveragesolid"))
            pControl->AttribSetBOOL (gszChecked, gfSpaceShowCoverageSolid);
         if (pControl = pPage->ControlFind (L"showCoverageshading"))
            pControl->AttribSetBOOL (gszChecked, gfSpaceShowCoverageShading);
         if (pControl = pPage->ControlFind (L"showCoveragethird"))
            pControl->AttribSetBOOL (gszChecked, gfSpaceShowCoverageThird);
         if (pControl = pPage->ControlFind (L"showImage"))
            pControl->AttribSetBOOL (gszChecked, gfSpaceShowImage);
         if (pControl = pPage->ControlFind (L"showImagesolid"))
            pControl->AttribSetBOOL (gszChecked, gfSpaceShowImageSolid);
         if (pControl = pPage->ControlFind (L"showImageshading"))
            pControl->AttribSetBOOL (gszChecked, gfSpaceShowImageShading);

         DWORD i;
         WCHAR szTemp[16];
         for (i = 0; i < gListSPACEPAINT.Num(); i++) {
            PSPACEPAINT p = (PSPACEPAINT) gListSPACEPAINT.Get(i);

            swprintf (szTemp, L"cb:%d", (int) i);
            pControl = pPage->ControlFind (szTemp);
            if (pControl)
               pControl->AttribSetBOOL (gszChecked, p->fShow);
         }
      }
      break;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS)pParam;

         if (!p->pControl || !p->pControl->m_pszName)
            break;

         BOOL  *pf;
         pf = NULL;

         if (!wcsncmp(p->pControl->m_pszName, L"cb:", 3)) {
            PSPACEPAINT sp = (PSPACEPAINT) gListSPACEPAINT.Get(_wtoi(p->pControl->m_pszName+3));
            pf = &sp->fShow;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"showpaints"))
            pf = &gfSpaceShowPaints;
         else if (!_wcsicmp(p->pControl->m_pszName, L"showpaintssolid"))
            pf = &gfSpaceShowPaintsSolid;
         else if (!_wcsicmp(p->pControl->m_pszName, L"showpaintsshading"))
            pf = &gfSpaceShowPaintsShading;
         else if (!_wcsicmp(p->pControl->m_pszName, L"showCoverage"))
            pf = &gfSpaceShowCoverage;
         else if (!_wcsicmp(p->pControl->m_pszName, L"showCoveragesolid"))
            pf = &gfSpaceShowCoverageSolid;
         else if (!_wcsicmp(p->pControl->m_pszName, L"showCoverageshading"))
            pf = &gfSpaceShowCoverageShading;
         else if (!_wcsicmp(p->pControl->m_pszName, L"showCoveragethird"))
            pf = &gfSpaceShowCoverageThird;
         else if (!_wcsicmp(p->pControl->m_pszName, L"showImage"))
            pf = &gfSpaceShowImage;
         else if (!_wcsicmp(p->pControl->m_pszName, L"showImagesolid"))
            pf = &gfSpaceShowImageSolid;
         else if (!_wcsicmp(p->pControl->m_pszName, L"showImageshading"))
            pf = &gfSpaceShowImageShading;
         if (!pf)
            break;   // nothing


         BOOL  fCheck;
         fCheck = p->pControl->AttribGetBOOL (gszChecked);
         if (fCheck == *pf)
            return TRUE;   // nothing changed
         
         // else, redraw
         *pf = fCheck;
         ESCMTHREEDCHANGE ch;
         memset (&ch, 0, sizeof(ch));
         ch.pszMML = ColorSpaceSubst();
         PCEscControl pControl;
         pControl = pPage->ControlFind (L"space3d");
         if (pControl)
            pControl->Message (ESCM_THREEDCHANGE, &ch);
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"COLORSPACE")) {
            p->pszSubString = ColorSpaceSubst();
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"PAINTSWITCH")) {
            MemZero (&gMemTemp);

            DWORD i;
            for (i = 0; i < gListSPACEPAINT.Num(); i++) {
               PSPACEPAINT p = (PSPACEPAINT) gListSPACEPAINT.Get(i);

               // if just starting row indicate so
               if ((i % 4) == 0)
                  MemCat (&gMemTemp, L"<tr>");

               MemCat (&gMemTemp, L"<td><button style=x checkbox=true name=cb:");
               MemCat (&gMemTemp, (int) i);
               MemCat (&gMemTemp, L">");
               WCHAR szTemp[256];
               szTemp[0] = 0; // BUGFIX - Just in case name too large
               MultiByteToWideChar (CP_ACP, 0, p->szName, -1, szTemp, sizeof(szTemp)/2);
               MemCatSanitize (&gMemTemp, szTemp);
               MemCat (&gMemTemp, L"</button></td>");

               // if ending treminate
               if ((i % 4) == 3)
                  MemCat (&gMemTemp, L"</tr>");
            }

            // blanks for the last one
            for (; i%4; i++) {
               MemCat (&gMemTemp, L"<td/>");

               // if ending treminate
               if ((i % 4) == 3)
                  MemCat (&gMemTemp, L"</tr>");
            }

            p->pszSubString = (PWSTR) gMemTemp.p;
            return TRUE;
         }
      }
      break;

   };


   return DefPage (pPage, dwMessage, pParam);
}

/*****************************************************************************
PaintsPage
*/
BOOL PaintsPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   static BOOL gfClickOnBlack = FALSE;

   switch (dwMessage) {
   case ESCN_LISTBOXSELCHANGE:
      {
         PESCNLISTBOXSELCHANGE p = (PESCNLISTBOXSELCHANGE) pParam;

         if (gfClickOnBlack && p->pControl && p->pszName && p->pControl->m_pszName &&
            !_wcsicmp(p->pControl->m_pszName, L"pallette")) {

            // clicked on pallette
            gfClickOnBlack = FALSE;

            char szTemp[64];
            WideCharToMultiByte (CP_ACP, 0, p->pszName, -1, szTemp, sizeof(szTemp), 0,0);
            HKEY  hKey = NULL;
            DWORD dwDisp;
            RegCreateKeyEx (HKEY_CURRENT_USER, gszRegPaintKey, 0, 0, REG_OPTION_NON_VOLATILE,
               KEY_READ | KEY_WRITE, NULL, &hKey, &dwDisp);
            DWORD dwColor;
            dwColor = 0;
            if (hKey) {
               DWORD dwSize;
               DWORD dwType;
               dwSize = sizeof(dwColor);
               RegQueryValueEx (hKey, szTemp, 0, &dwType, (BYTE*) &dwColor, &dwSize);

               RegCloseKey (hKey);
            }

            gcBaseBlack = dwColor;

            // set the black color
            PCEscControl pControl;
            pControl = pPage->ControlFind (L"currentblack");
            if (pControl) {
               MemZero (&gMemTemp);
               MemCat (&gMemTemp, L"<colorblend posn=background color=");
               MemCatColor (&gMemTemp, gcBaseBlack);
               MemCat (&gMemTemp, L"/>");
               ESCMSTATUSTEXT st;
               memset (&st, 0, sizeof(st));
               st.pszMML = (PWSTR) gMemTemp.p;
               pControl->Message (ESCM_STATUSTEXT, &st);
            }

            // inform user
            pPage->MBInformation (L"The definition of \"black\" has been changed.");

            break;   // default
         }
      }
      break;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS)pParam;

         if (!p->pControl || !p->pControl->m_pszName)
            break;

         if (!_wcsicmp(p->pControl->m_pszName, L"remove")) {
            if (IDYES != pPage->MBYesNo (L"Are you sure you want to remove this paint?"))
               return TRUE;

            // remove from combo box
            char  szTemp[256];
            int iSel;
            PCEscControl pControl;
            pControl = pPage->ControlFind (gszPallette);
            iSel = pControl->AttribGetInt(gszCurSel);

            // get the name
            ESCMLISTBOXGETITEM gi;
            memset (&gi, 0, sizeof(gi));
            gi.dwIndex = (DWORD) iSel;
            pControl->Message (ESCM_LISTBOXGETITEM, &gi);
            WideCharToMultiByte (CP_ACP, 0, gi.pszName ? gi.pszName : L"", -1,
               szTemp, sizeof(szTemp), 0, 0);

            ESCMLISTBOXDELETE lbd;
            lbd.dwIndex = (DWORD)iSel;
            pControl->Message (ESCM_LISTBOXDELETE, &lbd);
            pControl->AttribSetInt (gszCurSel, 0);

            // remove from registry
            HKEY  hKey = NULL;
            // BUGFIX - Didnt include HKEY_WRITE, so didn't work on NT
            RegOpenKeyEx (HKEY_CURRENT_USER, gszRegPaintKey, 0, KEY_READ | KEY_WRITE, &hKey);
            RegDeleteValue (hKey, szTemp);
            RegCloseKey (hKey);
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"colorspace")) {
            RECT  rt;
            SystemParametersInfo (SPI_GETWORKAREA, 0, &rt, 0);
            CEscWindow  cWindow;

            rt.left += 50;
            rt.right -= 50;
            rt.top += 50;
            rt.bottom -= 50;

            cWindow.Init (ghInstance, gpWindow->m_hWnd, EWS_FIXEDSIZE, &rt);
            cWindow.IconSet (LoadIcon(ghInstance, MAKEINTRESOURCE(IDI_APPICON)));

            PWSTR pRet;
            EnumeratePaints();
            ColorSpaceImage ();
redocolorspace:
            pRet = cWindow.PageDialog (ghInstance, IDR_MMLCOLORSPACE, ColorSpacePage);
            if (pRet && !_wcsicmp(pRet, gszSamePage))
               goto redocolorspace;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"pallettesave")) {
            PalletteSave (pPage);
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"palletteload")) {
            if (PalletteLoad (pPage))
               pPage->Exit (gszSamePage);
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"changeblack")) {
            pPage->MBInformation (L"Click on the paint in the list that you want to use as the base's \"black\" color.");
            gfClickOnBlack = TRUE;
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"edit")) {
            // get text
            char  szTemp[256];
            int iSel;
            WCHAR szName[256];
            PCEscControl pControl;
            pControl = pPage->ControlFind (gszPallette);
            iSel = pControl->AttribGetInt (gszCurSel);
            ESCMLISTBOXGETITEM gi;
            memset (&gi, 0, sizeof(gi));
            gi.dwIndex = (DWORD) iSel;
            pControl->Message (ESCM_LISTBOXGETITEM, &gi);
            szTemp[0] = 0;
            szName[0] = 0;
            if (gi.pszName) {
               WideCharToMultiByte (CP_ACP, 0, gi.pszName, -1, szTemp, sizeof(szTemp), 0,0);
               wcscpy (szName, gi.pszName);
            }

            // read the color
            HKEY  hKey = NULL;
            DWORD dwDisp;
            RegCreateKeyEx (HKEY_CURRENT_USER, gszRegPaintKey, 0, 0, REG_OPTION_NON_VOLATILE,
               KEY_READ | KEY_WRITE, NULL, &hKey, &dwDisp);
            DWORD dwColor;
            if (hKey) {
               DWORD dwSize;
               DWORD dwType;
               dwSize = sizeof(dwColor);
               RegQueryValueEx (hKey, szTemp, 0, &dwType, (BYTE*) &dwColor, &dwSize);

               // color
               // pull up help topic describing color choosing
//               WinHelp (NULL, gszHelp, HELP_CONTEXT, IDH_PAINTCOLOR);
               CHOOSECOLOR cc;
               memset (&cc, 0, sizeof(cc));
               cc.lStructSize = sizeof(cc);
               cc.hwndOwner = pPage->m_pWindow->m_hWnd;
               cc.rgbResult = dwColor;
               cc.Flags = CC_ANYCOLOR | CC_FULLOPEN | CC_RGBINIT | CC_SOLIDCOLOR;
               DWORD adwCust[16];
               DWORD j;
               for (j = 0; j < 16; j++)
                  adwCust[j] = RGB(j * 16, j*16, j*16);
               cc.lpCustColors = adwCust;
               BOOL fRet;
               fRet = ChooseColor (&cc);
               dwColor = cc.rgbResult;

               // write key
               if (fRet)
                  RegSetValueEx (hKey, szTemp, 0, REG_DWORD, (BYTE*) &dwColor, sizeof(dwColor));

               RegCloseKey (hKey);
            }

            // because the color is shown ,change the entry
            ESCMLISTBOXDELETE lbd;
            lbd.dwIndex = (DWORD)iSel;
            pControl->Message (ESCM_LISTBOXDELETE, &lbd);

            MemZero (&gMemTemp);
            PaintMML (szTemp, dwColor);
            ESCMLISTBOXADD add;
            memset (&add, 0, sizeof(add));
            add.dwInsertBefore = (DWORD)-1;
            add.pszMML = (PWSTR) gMemTemp.p;
            pControl->Message (ESCM_LISTBOXADD, &add);

            ESCMLISTBOXSELECTSTRING ss;
            memset (&ss, 0, sizeof(ss));
            ss.iStart = -1;
            ss.fExact = TRUE;
            ss.psz = szName;
            pControl->Message (ESCM_LISTBOXSELECTSTRING, &ss);
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"add")) {
            gpPagePaints = pPage;

            RECT  rt, r;
            SystemParametersInfo (SPI_GETWORKAREA, 0, &rt, 0);
            BOOL fCustomColor;
            fCustomColor = FALSE;
            {
               CEscWindow  cWindow;

               r.left = (rt.left*3 + rt.right) / 4;
               r.right = (rt.left + rt.right*3) / 4;
               r.top = rt.top + 50;
               r.bottom = rt.bottom - 50;

               cWindow.Init (ghInstance, gpWindow->m_hWnd, EWS_FIXEDSIZE, &r);
               cWindow.IconSet (LoadIcon(ghInstance, MAKEINTRESOURCE(IDI_APPICON)));

               PWSTR pRet;
               pRet = cWindow.PageDialog (ghInstance, IDR_MMLCATALOGPAINT, CatalogPaintPage);
               if (pRet && !_wcsicmp(pRet, L"custom"))
                  fCustomColor = TRUE;
            }

            // if requested a custom color, ask for a name and color
            if (fCustomColor) {
               WCHAR szName[64];
               {
                  CEscWindow  cWindow;

                  r.left = (rt.left*3 + rt.right) / 4;
                  r.right = (rt.left + rt.right*3) / 4;
                  r.top = rt.top + 50;
                  r.bottom = rt.bottom - 50;

                  cWindow.Init (ghInstance, gpWindow->m_hWnd, EWS_FIXEDSIZE | EWS_AUTOHEIGHT, &r);
                  cWindow.IconSet (LoadIcon(ghInstance, MAKEINTRESOURCE(IDI_APPICON)));

                  PWSTR pRet;
                  szName[0] = 0;
                  pRet = cWindow.PageDialog (ghInstance, IDR_MMLPAINTNAME, PaintNamePage, &szName);
                  if (!pRet || _wcsicmp(pRet, L"ok"))
                     return TRUE;   // didn't press OK
               }

               // get the color
               char szTemp[128];
               WideCharToMultiByte (CP_ACP, 0, szName, -1, szTemp, sizeof(szTemp), 0,0);
               CHOOSECOLOR cc;
               memset (&cc, 0, sizeof(cc));
               cc.lStructSize = sizeof(cc);
               cc.hwndOwner = pPage->m_pWindow->m_hWnd;
               cc.rgbResult = RGB(0x255, 0x255, 0x255);
               cc.Flags = CC_ANYCOLOR | CC_FULLOPEN | CC_RGBINIT | CC_SOLIDCOLOR;
               DWORD adwCust[16];
               DWORD j;
               for (j = 0; j < 16; j++)
                  adwCust[j] = RGB(j * 16, j*16, j*16);
               cc.lpCustColors = adwCust;
               if (!ChooseColor (&cc))
                  return TRUE;

               // add to registry
               HKEY  hKey = NULL;
               DWORD dwDisp;
               RegCreateKeyEx (HKEY_CURRENT_USER, gszRegPaintKey, 0, 0, REG_OPTION_NON_VOLATILE,
                  KEY_READ | KEY_WRITE, NULL, &hKey, &dwDisp);
               DWORD dw;
               if (hKey) {
                  dw = cc.rgbResult;
                  RegSetValueEx (hKey, szTemp, 0, REG_DWORD, (BYTE*) &dw, sizeof(dw));

                  RegCloseKey (hKey);
               }

               // add to combobox
               PCEscControl pControl;
               pControl = pPage->ControlFind (gszPallette);
               MemZero (&gMemTemp);
               PaintMML (szTemp, dw);
               ESCMLISTBOXADD add;
               memset (&add, 0, sizeof(add));
               add.dwInsertBefore = (DWORD)-1;
               add.pszMML = (PWSTR) gMemTemp.p;
               pControl->Message (ESCM_LISTBOXADD, &add);

               // select it
               ESCMLISTBOXSELECTSTRING ss;
               memset (&ss, 0, sizeof(ss));
               ss.iStart = -1;
               ss.fExact = TRUE;
               ss.psz = szName;
               pControl->Message (ESCM_LISTBOXSELECTSTRING, &ss);
            }

            return TRUE;
         }
      }
      break;

   case ESCM_INITPAGE:
      {
         PCEscControl pControl;

         // radio button
         pControl = pPage->ControlFind (L"existing");
         if (pControl)
            pControl->AttribSetBOOL (gszChecked, gdwMix == 0);
         pControl = pPage->ControlFind (L"mix");
         if (pControl)
            pControl->AttribSetBOOL (gszChecked, gdwMix == 1);
         pControl = pPage->ControlFind (L"threemix");
         if (pControl)
            pControl->AttribSetBOOL (gszChecked, gdwMix == 5);
         pControl = pPage->ControlFind (L"blackwhite");
         if (pControl)
            pControl->AttribSetBOOL (gszChecked, gdwMix == 2);
         pControl = pPage->ControlFind (L"bypass");
         if (pControl)
            pControl->AttribSetBOOL (gszChecked, gdwMix == 3);
         pControl = pPage->ControlFind (L"mixblackwhite");
         if (pControl)
            pControl->AttribSetBOOL (gszChecked, gdwMix == 4);
            // BUGFIX - some people don't need

         // enumerate the paint names in the registry
tryagain:
         HKEY  hKey = NULL;
         RegOpenKeyEx (HKEY_CURRENT_USER, gszRegPaintKey, 0, KEY_READ, &hKey);
         if (!hKey) {
            // no paints, so add
            HKEY  hKey = NULL;
            DWORD dwDisp;
            RegCreateKeyEx (HKEY_CURRENT_USER, gszRegPaintKey, 0, 0, REG_OPTION_NON_VOLATILE,
               KEY_READ | KEY_WRITE, NULL, &hKey, &dwDisp);
            if (hKey) {
               DWORD dw;

               dw = RGB(255,255,255);
               RegSetValueEx (hKey, "White", 0, REG_DWORD, (BYTE*) &dw, sizeof(dw));

               dw = RGB(0x0,0x0,0x0);
               RegSetValueEx (hKey, "Black", 0, REG_DWORD, (BYTE*) &dw, sizeof(dw));

               dw = 0x704f01;
               RegSetValueEx (hKey, "Cerulean Blue", 0, REG_DWORD, (BYTE*) &dw, sizeof(dw));

               dw = 0x11dbff;
               RegSetValueEx (hKey, "Lemon Yellow", 0, REG_DWORD, (BYTE*) &dw, sizeof(dw));

               dw = 0x0b00ce;
               RegSetValueEx (hKey, "Spectrum Red", 0, REG_DWORD, (BYTE*) &dw, sizeof(dw));

               dw = 0x546d10;
               RegSetValueEx (hKey, "Terre Verte", 0, REG_DWORD, (BYTE*) &dw, sizeof(dw));

               dw = 0x898303;
               RegSetValueEx (hKey, "Turquoise", 0, REG_DWORD, (BYTE*) &dw, sizeof(dw));

               dw = 0x073d61;
               RegSetValueEx (hKey, "Van Dyke Brown", 0, REG_DWORD, (BYTE*) &dw, sizeof(dw));

               RegCloseKey (hKey);
            }

            goto tryagain;
         }

         // enumerate
         MemZero (&gMemTemp);
         DWORD dwNameSize, dwType, i, dwVal, dwNeeded;
         char  szName[256];
         for (i = 0; ; i++) {
            dwNameSize = sizeof(szName);
            dwVal = 0;
            dwNeeded = sizeof(dwVal);
            if (RegEnumValue (hKey, i, szName, &dwNameSize, NULL, &dwType, (LPBYTE) &dwVal, &dwNeeded))
               break;

            // add to the list box
            PaintMML (szName, (COLORREF)dwVal);
         }

         RegCloseKey (hKey);

         pControl = pPage->ControlFind (gszPallette);
         if (pControl) {
            ESCMLISTBOXADD add;
            memset (&add, 0, sizeof(add));
            add.dwInsertBefore = (DWORD)-1;
            add.pszMML = (PWSTR) gMemTemp.p;
            pControl->Message (ESCM_LISTBOXADD, &add);
            pControl->AttribSetInt (gszCurSel, 0);
         }

         // set the detail setting
         pControl = pPage->ControlFind (L"detail");
         if (pControl)
            pControl->AttribSetInt(gszCurSel, (int) gdwBaseDetail - 1);

         // set the black color
         pControl = pPage->ControlFind (L"currentblack");
         if (pControl) {
            MemZero (&gMemTemp);
            MemCat (&gMemTemp, L"<colorblend posn=background color=");
            MemCatColor (&gMemTemp, gcBaseBlack);
            MemCat (&gMemTemp, L"/>");
            ESCMSTATUSTEXT st;
            memset (&st, 0, sizeof(st));
            st.pszMML = (PWSTR) gMemTemp.p;
            pControl->Message (ESCM_STATUSTEXT, &st);
         }
      }
      break;   // default

   case ESCN_COMBOBOXSELCHANGE:
      {
         PESCNCOMBOBOXSELCHANGE p = (PESCNCOMBOBOXSELCHANGE) pParam;

         if (p->pControl && p->pControl->m_pszName && !_wcsicmp(p->pControl->m_pszName, L"detail")) {
            gdwBaseDetail = p->dwCurSel+1;
            return TRUE;
         }
      }
      break;

   case ESCM_CLOSE:
      pPage->Message (ESCM_STOREAWAY);
      if (!AskSave (pPage, VIEWMODE_PAINTS))
         return TRUE;   // abort cancel
      break;   // default

   case ESCM_LINK:
      pPage->Message (ESCM_STOREAWAY);
      break;   // default behavour
   case ESCM_STOREAWAY:
      {
         PCEscControl pControl;
         gdwMix = 3;
         pControl = pPage->ControlFind (L"existing");
         if (pControl && pControl->AttribGetBOOL (gszChecked))
            gdwMix = 0;
         pControl = pPage->ControlFind (L"mix");
         if (pControl && pControl->AttribGetBOOL (gszChecked))
            gdwMix = 1;
         pControl = pPage->ControlFind (L"threemix");
         if (pControl && pControl->AttribGetBOOL (gszChecked))
            gdwMix = 5;
         pControl = pPage->ControlFind (L"blackwhite");
         if (pControl && pControl->AttribGetBOOL (gszChecked))
            gdwMix = 2;
         pControl = pPage->ControlFind (L"mixblackwhite");
         if (pControl && pControl->AttribGetBOOL (gszChecked))
            gdwMix = 4;

      }
      return TRUE;

   };


   return DefPage (pPage, dwMessage, pParam);
}

/****************************************************************************
ToHLS - Converts from RGB to HLS

inputs
   double   fRed, fGreen, fBlue - rgb
   double   *pfH, *pfL, *pfS
returns
   none
*/
void ToHLS256 (double fRed, double fGreen, double fBlue,
            double* pfH, double* pfL, double* pfS)
{
   double   fMax, fMin;
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
   double fDelta;
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
FromHLS - Converts from HLS to RGB HLS

inputs
   double   fRed, fGreen, fBlue - rgb
   double   *pfH, *pfL, *pfS
returns
   none
*/
void FromHLS256 (double fH, double fL, double fS,
            double* pfRed, double* pfGreen, double* pfBlue)
{
   // combination of lightness and saturation tells us min and max
   double fMin, fMax, k;
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
   double fDelta;
   fDelta = fMax - fMin;

   double r,g,b;
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
   double   fr, fg, fb, fh, fl, fs;

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
   double   fr, fg, fb, fh, fl, fs;

   fh = h / 256.0;
   fl = l / 256.0;
   fs = s / 256.0;

   FromHLS256 (fh, fl, fs, &fr, &fg, &fb);
   *pb = (WORD) (fr * 255);
   *pg = (WORD) (fg * 255);
   *pb = (WORD) (fb * 255);
}

void ToXYZFromHLS (WORD h, WORD l, WORD s, int *px, int *py, int *pz)
{
   WORD  i;

   // make sine/cos table
   static BOOL fSin = FALSE;
   static int  aiSin[256], aiCos[256];
   if (!fSin) {
      fSin = TRUE;
      for (i = 0; i < 256; i++) {
         aiSin[i] = (int) (sin(i / 256.0 * 2 * 3.1415) * 256.0);
         aiCos[i] = (int) (cos(i / 256.0 * 2 * 3.1415) * 256.0);
      }
   }

   int   ix1, iy1, iz1;

   *py = (iy1 = (int) l / 128 - 256); // / 16; // -256 to + 256
   *px = ix1 = (int) aiSin[h / 256] * (s / 256) * (256 - max(iy1,-iy1)) / 256 / 256;
   *pz = iz1 = (int) aiCos[h / 256] * (s / 256) * (256 - max(iy1,-iy1)) / 256 / 256;
}

void ToXYZ (WORD r, WORD g, WORD b, int *px, int *py, int *pz)
{
   WORD  h, l, s;

   // figure where the comparison point is in 3d HLS space
   ToHLS ((WORD) gImageMain.UnGamma(r) * 256,
      (WORD) gImageMain.UnGamma(g) * 256,
      (WORD) gImageMain.UnGamma(b) * 256,
      &h, &l, &s);
   ToXYZFromHLS (h, l, s, px, py, pz);
}

/*****************************************************************************
SubtractiveMix - Mix two subtractive colors.

inputs
   WORD     r1, g1, b1 - red, green, blue
   WORD     r2, g2, b2 - second color
   WORD     *pr, *pg, *pb - mixed color
   doublr   fAmt1 - If .5 (default) then 1/2 is color 1, and 1/2 is color 2.
                     If .7 then 70% color 1, 30% color 2, etc.
returns
   none
*/
void SubtractiveMix (WORD r1, WORD g1, WORD b1,
                     WORD r2, WORD g2, WORD b2,
                     WORD *pr, WORD *pg, WORD *pb,
                     double fAmt1 = 0.5)
{
   BYTE  br1, br2, bg1, bg2, bb1, bb2;

   // convert to gamma, invert (255-c), and then ungamma
   br1 = gImageMain.UnGamma(r1);
   bg1 = gImageMain.UnGamma(g1);
   bb1 = gImageMain.UnGamma(b1);
   br2 = gImageMain.UnGamma(r2);
   bg2 = gImageMain.UnGamma(g2);
   bb2 = gImageMain.UnGamma(b2);
   r1 = gImageMain.Gamma (255 - br1);
   g1 = gImageMain.Gamma (255 - bg1);
   b1 = gImageMain.Gamma (255 - bb1);
   r2 = gImageMain.Gamma (255 - br2);
   g2 = gImageMain.Gamma (255 - bg2);
   b2 = gImageMain.Gamma (255 - bb2);

   // average these
   WORD  r, g, b;
   if (fAmt1 == 0.5) {
      r = r1 / 2 + r2 / 2;
      g = g1 / 2 + g2 / 2;
      b = b1 / 2 + b2 / 2;
   }
   else {
      double fAmt2 = 1.0 - fAmt1;

      r = (WORD) (r1 * fAmt1 + r2 * fAmt2);
      g = (WORD) (g1 * fAmt1 + g2 * fAmt2);
      b = (WORD) (b1 * fAmt1 + b2 * fAmt2);
   }

   // convert to gamm, invert (255-c), and then ungamma
   br1 = gImageMain.UnGamma (r);
   bg1 = gImageMain.UnGamma (g);
   bb1 = gImageMain.UnGamma (b);
   *pr = gImageMain.Gamma (255 - br1);
   *pg = gImageMain.Gamma (255 - bg1);
   *pb = gImageMain.Gamma (255 - bb1);

   // done
}

/*****************************************************************************
GeneratePalette - Generates the palette to use. If the check is set to allow
mixing then this mixes colors
*/
void GeneratePalette (void)
{
   if (gpPColor)
      free (gpPColor);
   gpPColor = NULL;
   size_t dwSize;

   PCOLOR   aTemp[100]; // temporary storage for 100 colors
   DWORD    dwBase = 0; // number of base colors found

   // enumerate the registry
   HKEY  hKey = NULL;
   RegOpenKeyEx (HKEY_CURRENT_USER, gszRegPaintKey, 0, KEY_READ, &hKey);
   if (hKey) {
      // enumerate
      DWORD dwNameSize, dwType, i, dwRGB, dwSize;
      // BUGFIX - Make i < 100 check just to make sure don't overrun
      for (i = 0; i < 100; i++) {
         dwNameSize = sizeof(aTemp[i].szName);
         dwSize = sizeof(dwRGB);
         if (RegEnumValue (hKey, i, aTemp[i].szName, &dwNameSize, NULL, &dwType, (PBYTE) &dwRGB, &dwSize))
            break;

         aTemp[i].r = gImageMain.Gamma(GetRValue(dwRGB));
         aTemp[i].g = gImageMain.Gamma(GetGValue(dwRGB));
         aTemp[i].b = gImageMain.Gamma(GetBValue(dwRGB));

         ToXYZ (aTemp[i].r, aTemp[i].g, aTemp[i].b, &aTemp[i].x, &aTemp[i].y, &aTemp[i].z);
      }
      dwBase = i;

      RegCloseKey (hKey);
   }

   // if not mix then easier
   if (gdwMix == 0) {
      gdwPColorSize = dwBase;
      gpPColor = (PCOLOR*) malloc(sizeof(PCOLOR) * gdwPColorSize);
      if (!gpPColor)
         return;
      memcpy (gpPColor, aTemp, sizeof(PCOLOR) * gdwPColorSize);
      return;
   }

   // if mix with black and white
   if ((gdwMix == 2) || (gdwMix == 4)) {
      gdwPColorSize = 0;
      // BUGFIX - Just using x2 just in case miscalculated for memory overrun
      dwSize = sizeof(PCOLOR) * 2 *
         ((dwBase+1) * 6 + (gdwMix == 4) ? (dwBase * (dwBase+1) * 6) : 0);
      gpPColor = (PCOLOR*) malloc (dwSize);
            // BUGFIX - Changed two entries from dwBase to dwBase+1
      if (!gpPColor)
         return;
      DWORD x, y;
      for (x = 0; x < dwBase; x++) {
         // whole color
         memcpy (gpPColor + gdwPColorSize, aTemp + x, sizeof(PCOLOR));
         gdwPColorSize++;

         // if this is white then continue
         if ((aTemp[x].r > 60000) && (aTemp[x].g > 60000) && (aTemp[x].b > 60000))
            continue;

         // mix with white
         sprintf (gpPColor[gdwPColorSize].szName, "%s + White", aTemp[x].szName);
         SubtractiveMix (aTemp[x].r, aTemp[x].g, aTemp[x].b,
            0xffff, 0xffff, 0xffff,
            &gpPColor[gdwPColorSize].r, &gpPColor[gdwPColorSize].g, &gpPColor[gdwPColorSize].b);
         ToXYZ (gpPColor[gdwPColorSize].r, gpPColor[gdwPColorSize].g, gpPColor[gdwPColorSize].b, &gpPColor[gdwPColorSize].x, &gpPColor[gdwPColorSize].y, &gpPColor[gdwPColorSize].z);
         PCOLOR   *p;
         p = gpPColor + gdwPColorSize;
         gdwPColorSize++;

         // mix with white again
         sprintf (gpPColor[gdwPColorSize].szName, "%s + 3 parts White", aTemp[x].szName);
         SubtractiveMix (p->r, p->g, p->b,
            0xffff, 0xffff, 0xffff,
            &gpPColor[gdwPColorSize].r, &gpPColor[gdwPColorSize].g, &gpPColor[gdwPColorSize].b);
         ToXYZ (gpPColor[gdwPColorSize].r, gpPColor[gdwPColorSize].g, gpPColor[gdwPColorSize].b, &gpPColor[gdwPColorSize].x, &gpPColor[gdwPColorSize].y, &gpPColor[gdwPColorSize].z);
         p = gpPColor + gdwPColorSize;
         gdwPColorSize++;

         // mix with white again
         sprintf (gpPColor[gdwPColorSize].szName, "%s + 7 parts White", aTemp[x].szName);
         SubtractiveMix (p->r, p->g, p->b,
            0xffff, 0xffff, 0xffff,
            &gpPColor[gdwPColorSize].r, &gpPColor[gdwPColorSize].g, &gpPColor[gdwPColorSize].b);
         ToXYZ (gpPColor[gdwPColorSize].r, gpPColor[gdwPColorSize].g, gpPColor[gdwPColorSize].b, &gpPColor[gdwPColorSize].x, &gpPColor[gdwPColorSize].y, &gpPColor[gdwPColorSize].z);
         p = gpPColor + gdwPColorSize;
         gdwPColorSize++;

         // if this is not black then mix with black
         if ((aTemp[x].r < 100) && (aTemp[x].g < 100) && (aTemp[x].b < 100))
            continue;

         // mix with 1/2 part black
         sprintf (gpPColor[gdwPColorSize].szName, "%s + 1/2 part Black", aTemp[x].szName);
         SubtractiveMix (aTemp[x].r, aTemp[x].g, aTemp[x].b,
            GetRValue(gcBaseBlack), GetGValue(gcBaseBlack), GetBValue(gcBaseBlack),
            &gpPColor[gdwPColorSize].r, &gpPColor[gdwPColorSize].g, &gpPColor[gdwPColorSize].b,
            1.0 / 1.5);
         ToXYZ (gpPColor[gdwPColorSize].r, gpPColor[gdwPColorSize].g, gpPColor[gdwPColorSize].b, &gpPColor[gdwPColorSize].x, &gpPColor[gdwPColorSize].y, &gpPColor[gdwPColorSize].z);
         gdwPColorSize++;

         // mix with black
         sprintf (gpPColor[gdwPColorSize].szName, "%s + Black", aTemp[x].szName);
         SubtractiveMix (aTemp[x].r, aTemp[x].g, aTemp[x].b,
            GetRValue(gcBaseBlack), GetGValue(gcBaseBlack), GetBValue(gcBaseBlack),
            &gpPColor[gdwPColorSize].r, &gpPColor[gdwPColorSize].g, &gpPColor[gdwPColorSize].b);
         ToXYZ (gpPColor[gdwPColorSize].r, gpPColor[gdwPColorSize].g, gpPColor[gdwPColorSize].b, &gpPColor[gdwPColorSize].x, &gpPColor[gdwPColorSize].y, &gpPColor[gdwPColorSize].z);
         gdwPColorSize++;
      }

      // if it's type 4 (mix two colors), then do that
      if (gdwMix == 4) for (x = 0; x < dwBase; x++) for (y = 0; y < dwBase; y++) {
         // don't mix with same color
         if (x == y)
            continue;

         // if this is white then continue
         if ((aTemp[x].r > 60000) && (aTemp[x].g > 60000) && (aTemp[x].b > 60000))
            continue;
         // if this is white then continue
         if ((aTemp[y].r > 60000) && (aTemp[y].g > 60000) && (aTemp[y].b > 60000))
            continue;
         // if this is not black then mix with black
         if ((aTemp[x].r < 100) && (aTemp[x].g < 100) && (aTemp[x].b < 100))
            continue;
         // if this is not black then mix with black
         if ((aTemp[y].r < 100) && (aTemp[y].g < 100) && (aTemp[y].b < 100))
            continue;

         // else, half and half
         WORD  r, g, b;
         sprintf (gpPColor[gdwPColorSize].szName, "%s + %s", aTemp[x].szName, aTemp[y].szName);
         SubtractiveMix (aTemp[x].r, aTemp[x].g, aTemp[x].b,
            aTemp[y].r, aTemp[y].g, aTemp[y].b,
            &gpPColor[gdwPColorSize].r, &gpPColor[gdwPColorSize].g, &gpPColor[gdwPColorSize].b);
         r = gpPColor[gdwPColorSize].r;
         g = gpPColor[gdwPColorSize].g;
         b = gpPColor[gdwPColorSize].b;
         char *pszMix;
         pszMix = gpPColor[gdwPColorSize].szName;
         ToXYZ (gpPColor[gdwPColorSize].r, gpPColor[gdwPColorSize].g, gpPColor[gdwPColorSize].b, &gpPColor[gdwPColorSize].x, &gpPColor[gdwPColorSize].y, &gpPColor[gdwPColorSize].z);
         gdwPColorSize++;


         // mix with white
         sprintf (gpPColor[gdwPColorSize].szName, "%s + White", pszMix);
         SubtractiveMix (r, g, b,
            0xffff, 0xffff, 0xffff,
            &gpPColor[gdwPColorSize].r, &gpPColor[gdwPColorSize].g, &gpPColor[gdwPColorSize].b,
            2.0 / 3.0);
         ToXYZ (gpPColor[gdwPColorSize].r, gpPColor[gdwPColorSize].g, gpPColor[gdwPColorSize].b, &gpPColor[gdwPColorSize].x, &gpPColor[gdwPColorSize].y, &gpPColor[gdwPColorSize].z);
         gdwPColorSize++;

         // mix with white again
         sprintf (gpPColor[gdwPColorSize].szName, "%s + 3 parts White", pszMix);
         SubtractiveMix (r, g, b,
            0xffff, 0xffff, 0xffff,
            &gpPColor[gdwPColorSize].r, &gpPColor[gdwPColorSize].g, &gpPColor[gdwPColorSize].b,
            2.0 / 5.0);
         ToXYZ (gpPColor[gdwPColorSize].r, gpPColor[gdwPColorSize].g, gpPColor[gdwPColorSize].b, &gpPColor[gdwPColorSize].x, &gpPColor[gdwPColorSize].y, &gpPColor[gdwPColorSize].z);
         gdwPColorSize++;

         // mix with white again
         sprintf (gpPColor[gdwPColorSize].szName, "%s + 7 parts White", pszMix);
         SubtractiveMix (r, g, b,
            0xffff, 0xffff, 0xffff,
            &gpPColor[gdwPColorSize].r, &gpPColor[gdwPColorSize].g, &gpPColor[gdwPColorSize].b,
            2.0 / 9.0);
         ToXYZ (gpPColor[gdwPColorSize].r, gpPColor[gdwPColorSize].g, gpPColor[gdwPColorSize].b, &gpPColor[gdwPColorSize].x, &gpPColor[gdwPColorSize].y, &gpPColor[gdwPColorSize].z);
         gdwPColorSize++;

         // mix with 1/2 part black
         sprintf (gpPColor[gdwPColorSize].szName, "%s + 1/2 part Black", pszMix);
         SubtractiveMix (r, g, b,
            GetRValue(gcBaseBlack), GetGValue(gcBaseBlack), GetBValue(gcBaseBlack),
            &gpPColor[gdwPColorSize].r, &gpPColor[gdwPColorSize].g, &gpPColor[gdwPColorSize].b,
            2.0 / 2.5);
         ToXYZ (gpPColor[gdwPColorSize].r, gpPColor[gdwPColorSize].g, gpPColor[gdwPColorSize].b, &gpPColor[gdwPColorSize].x, &gpPColor[gdwPColorSize].y, &gpPColor[gdwPColorSize].z);
         gdwPColorSize++;

         // mix with black
         sprintf (gpPColor[gdwPColorSize].szName, "%s + Black", pszMix);
         SubtractiveMix (r, g, b,
            GetRValue(gcBaseBlack), GetGValue(gcBaseBlack), GetBValue(gcBaseBlack),
            &gpPColor[gdwPColorSize].r, &gpPColor[gdwPColorSize].g, &gpPColor[gdwPColorSize].b,
            2.0 / 3.0);
         ToXYZ (gpPColor[gdwPColorSize].r, gpPColor[gdwPColorSize].g, gpPColor[gdwPColorSize].b, &gpPColor[gdwPColorSize].x, &gpPColor[gdwPColorSize].y, &gpPColor[gdwPColorSize].z);
         gdwPColorSize++;
      }


      return;
   }

   // else mix = 1 or 5, mix them all together
   DWORD x,y,z;
   gdwPColorSize = 0;
   // BUGFIX - Just using x2 just in case miscalculated for memory overrun
   dwSize =  sizeof(PCOLOR) * 2 *
      (dwBase * (dwBase+1) / 2 + dwBase + 1 + ((gdwMix == 5) ? (dwBase * dwBase * (dwBase+1) / 2): 0) );
   gpPColor = (PCOLOR*) malloc (dwSize);
         // BUGFIX - Changed two entries from dwBase to dwBase+1
   if (!gpPColor)
      return;
   for (x = 0; x < dwBase; x++) for (y = 0; y <= x; y++) {
      // if even column then whole color
      if (x == y) {
         memcpy (gpPColor + gdwPColorSize, aTemp + x, sizeof(PCOLOR));
         gdwPColorSize++;
         continue;
      }

      // else, half and half
      sprintf (gpPColor[gdwPColorSize].szName, "%s + %s", aTemp[x].szName, aTemp[y].szName);
      SubtractiveMix (aTemp[x].r, aTemp[x].g, aTemp[x].b,
         aTemp[y].r, aTemp[y].g, aTemp[y].b,
         &gpPColor[gdwPColorSize].r, &gpPColor[gdwPColorSize].g, &gpPColor[gdwPColorSize].b);
#if 0
      gpPColor[gdwPColorSize].r = aTemp[x].r / 2 + aTemp[y].r / 2;
      gpPColor[gdwPColorSize].g = aTemp[x].g / 2 + aTemp[y].g / 2;
      gpPColor[gdwPColorSize].b = aTemp[x].b / 2 + aTemp[y].b / 2;
#endif // 0

      ToXYZ (gpPColor[gdwPColorSize].r, gpPColor[gdwPColorSize].g, gpPColor[gdwPColorSize].b, &gpPColor[gdwPColorSize].x, &gpPColor[gdwPColorSize].y, &gpPColor[gdwPColorSize].z);
      gdwPColorSize++;
   }

   if (gdwMix == 5) {
      for (x = 0; x < dwBase; x++) for (y = x+1; y < dwBase; y++) for (z=y+1; z < dwBase; z++) {
         // else, half and half
         sprintf (gpPColor[gdwPColorSize].szName, "%s + %s + %s",
            aTemp[x].szName, aTemp[y].szName, aTemp[z].szName);
         WORD r,g,b;
         SubtractiveMix (aTemp[x].r, aTemp[x].g, aTemp[x].b,
            aTemp[y].r, aTemp[y].g, aTemp[y].b,
            &r, &g, &b);
         SubtractiveMix (r, g, b,
            aTemp[z].r, aTemp[z].g, aTemp[z].b,
            &gpPColor[gdwPColorSize].r, &gpPColor[gdwPColorSize].g, &gpPColor[gdwPColorSize].b,
            .666);

         ToXYZ (gpPColor[gdwPColorSize].r, gpPColor[gdwPColorSize].g, gpPColor[gdwPColorSize].b, &gpPColor[gdwPColorSize].x, &gpPColor[gdwPColorSize].y, &gpPColor[gdwPColorSize].z);
         gdwPColorSize++;
      }
   }
   // done
}

/*****************************************************************************
ClosestColor - Looks through the palette and finds the closest color.

inputs
   WORD     r,g,b - gamma corrected
   WORD     *paCount - optional. Pointer to an array of counts. A color is only
               valid if count[color#] > 0
returns
   WORD - color number
*/
WORD ClosestColor (WORD r, WORD g, WORD b, WORD *paCount = NULL)
{
   DWORD dwClosestDist = 0xffffffff;
   WORD  wClosest = 0;
   WORD  i;

   int   ix1, iy1, iz1;

   // figure where the comparison point is in 3d HLS space
   ToXYZ (r, g, b, &ix1, &iy1, &iz1);


   for (i = 0; i < (WORD) gdwPColorSize; i++) {
      // if color not in the list then skip it
      if (paCount && !paCount[i])
         continue;

      DWORD dwDist;
      PCOLOR   *p;
      p = gpPColor + i;
      dwDist = 0;

      // distance in these coords
      dwDist = (ix1 - p->x) * (ix1 - p->x) + (iy1 - p->y) * (iy1 - p->y) + (iz1 - p->z) * (iz1 - p->z);

#if 0
      // RGB distance
      if (p->r > r)
         dw = (p->r - r);
      else
         dw = (r - p->r);
      dwDist += dw * dw / 256;

      if (p->g > g)
         dw = (p->g - g);
      else
         dw = (g - p->g);
      dwDist += dw * dw / 256;

      if (p->b > b)
         dw = (p->b - b);
      else
         dw = (b - p->b);
      dwDist += dw * dw / 256;
#endif

      if (dwDist < dwClosestDist) {
         dwClosestDist = dwDist;
         wClosest = i;
      }
   }


   return wClosest;
}


/*****************************************************************************
ConvertToColors - Convers the cutout image to an array of closest colors.
It does this by:
1.1) Treat a color as being in a block of 3x3 (or whatever) pixels.
1.2) Average all the colors in the block.
1.3) Find the closest color.
1.4) Use that.
2.1) Then, go over the "mega-block", which is a block of 3x3 3x3 pixels (or whatever)
2.2) Look through all those colors and use the most common, converting everything.
3.1) Repeatedly pass over and run smoothing operation. For each pixel which has neighbors
      of different colors, see if it's color is closer to a neighbor. If so, jump to that.

BUGFIX - This is a different version than original for speed and better quality.

inputs
   DWORD    dwBlockWidth - Number of pixels across/down a block. 1+
   DWORD    dwMegaBlockWidth - Number of blocks acrss/down a megablock. 1+
*/
typedef struct {
   WORD     wColor;     // color number
   DWORD    dwCount;    // count
} MBINFO, *PMBINFO;
void ConvertToColors (DWORD dwBlockWidth, DWORD dwMegaBlockWidth = 3)
{
   dwBlockWidth = max(1,dwBlockWidth);
   dwMegaBlockWidth = max(1, dwMegaBlockWidth);

   if (gawImageColor)
      free (gawImageColor);
   DWORD xMax, yMax, x, y;
   xMax = gImageCutout.DimX();
   yMax = gImageCutout.DimY();
   gawImageColor = (WORD*) malloc (xMax * yMax * sizeof(WORD));
   if (!gawImageColor)
      return;

   // loop, averaging pixels to a block and then finding closest color
   WORD  r,g,b;
   DWORD y2, x2;
   for (y = 0; y < yMax; y += dwBlockWidth) for (x = 0; x < xMax; x += dwBlockWidth) {
      DWORD  dwR, dwG, dwB,dwCount;
      dwR = dwG = dwB = dwCount = 0;

      // average them
      for (y2 = y; (y2 < y+dwBlockWidth) && (y2 < yMax); y2++)
         for (x2 = x; (x2 < x+dwBlockWidth) && (x2 < xMax); x2++) {
            gImageCutout.PixelGet (x2, y2, &r, &g, &b);
            dwCount++;
            dwR += r;
            dwG += g;
            dwB += b;
         }
      dwR /= dwCount;
      dwG /= dwCount;
      dwB /= dwCount;

      // find nearest color
      WORD  wNearest;
      wNearest = ClosestColor ((WORD)dwR, (WORD)dwG, (WORD)dwB);

      // fill the image color in
      for (y2 = y; (y2 < y+dwBlockWidth) && (y2 < yMax); y2++)
         for (x2 = x; (x2 < x+dwBlockWidth) && (x2 < xMax); x2++)
            gawImageColor[y2 * xMax + x2] = wNearest;
   }

   // Megablocks
   MBINFO mb, *pmb;
   CListFixed cl;
   DWORD i;
   cl.Init (sizeof(MBINFO));
   DWORD dwMega;
   dwMega = dwBlockWidth * dwMegaBlockWidth;
   for (y = 0; y < yMax; y += dwMega) for (x = 0; x < xMax; x += dwMega) {
      cl.Clear();

      // count which colors appear how often
      for (y2 = y; (y2 < y+dwMega) && (y2 < yMax); y2++)
         for (x2 = x; (x2 < x+dwMega) && (x2 < xMax); x2++) {
            mb.wColor = gawImageColor[y2 * xMax + x2];
            mb.dwCount = 1;

            for (i = 0; i < cl.Num(); i++) {
               pmb = (PMBINFO) cl.Get(i);
               if (pmb->wColor == mb.wColor)
                  break;
            }
            if (i < cl.Num())
               pmb->dwCount++;
            else
               cl.Add (&mb);
         }

      // find the most common
      PMBINFO pMax;
      pMax = (PMBINFO) cl.Get(0);
      for (i = 1; i < cl.Num(); i++) {
         pmb = (PMBINFO) cl.Get(i);
         if (pmb->dwCount > pMax->dwCount)
            pMax = pmb;
      }

      // write them in
      WORD wMax;
      wMax = pMax->wColor;
      for (y2 = y; (y2 < y+dwMega) && (y2 < yMax); y2++)
         for (x2 = x; (x2 < x+dwMega) && (x2 < xMax); x2++)
            gawImageColor[y2 * xMax + x2] = wMax;

   }

   // smoothing
   DWORD dwPass;
   WORD     awCount[50000]; // shouldn't be any more than this # of combinations
      // BUGFIX - increased from 10000 to 100000 because of crash
      // BUGFIX - decreased from 100000 to 50000 to reduce stack overflow chance
   DWORD    dwChanged;
   WORD     wClosest;

   // BUGFIX - Just check for overlow
   gdwPColorSize = min(gdwPColorSize, sizeof(awCount)/sizeof(WORD));

   for (dwPass = 0; dwPass < dwMega/2; dwPass++) {
      dwChanged = 0;

      for (y = 0; y < yMax; y++) for (x = 0; x < xMax; x++) {
         memset (awCount, 0, sizeof(WORD) * gdwPColorSize);

         // look at surrounding colors and see what of each exists
         for (y2 = y ? (y-1) : 0; (y2 < yMax) && (y2 <= y+1); y2++)
            for (x2 = x ? (x-1) : 0; (x2 < xMax) && (x2 <= x+1); x2++)
               if ((y2 != y) || (x2 != x))
                  if (gawImageColor[y2 * xMax + x2] < gdwPColorSize) // BUGFIX - extra check
                     awCount[gawImageColor[y2 * xMax + x2]]++;

         // take the cloest
         gImageCutout.PixelGet (x, y, &r, &g, &b);
         wClosest = ClosestColor (r, g, b, awCount);
         if (wClosest != gawImageColor[y * xMax + x])
            dwChanged++;
         gawImageColor[y * xMax + x] = wClosest;
      }

#ifdef _DEBUG
      char szTemp[256];
      sprintf (szTemp, "Pass %d: Changed %d\r\n", (int) dwPass, (int) dwChanged);
      OutputDebugString(szTemp);
#endif

   }
}


/*****************************************************************************
ColorsToImage - Converts all the colors in gawImageColor to the image gImageColors.

inputs
   float       fOutlineStrength - How strong the outline is, from 0 to 1. 0.5 is outline + color
*/
void ColorsToImage (float fOutlineStrength)
{
   DWORD xMax, yMax, x, y;
   xMax = gImageCutout.DimX();
   yMax = gImageCutout.DimY();

   gImageColors.Clear (xMax, yMax);

   // color strength
   DWORD dwColorStrength, dwWhiteStrength, dwWhite;
   if (fOutlineStrength < 0.5)
      dwColorStrength = 0x100;
   else
      dwColorStrength = (DWORD)((1.0 - fOutlineStrength) * (float)0x200);
   dwWhiteStrength = 0x100 - dwColorStrength;
   dwWhite = dwWhiteStrength * 0xffff;

   // border
   DWORD dwBorderColorStrength, dwBlackStrength, dwBlack;
   if (fOutlineStrength < 0.5)
      dwBorderColorStrength = (DWORD)((0.5 - fOutlineStrength) * (float)0x200);
   else
      dwBorderColorStrength = 0;
   dwBlackStrength = 0x100 - dwBorderColorStrength;
   dwBlack = dwWhiteStrength * 0;   // so it's 0

   // loop
   for (y = 0; y < yMax; y++) for (x = 0; x < xMax; x++) {
      // see if this if is an outline
      int xx, yy;
      BOOL fBorder = FALSE;
      WORD wThis = gawImageColor[y * xMax + x];
      for (yy = (int)y-1; yy <= (int)y+1; yy++) {
         if ((yy < 0) || (yy >= (int)yMax))
            continue;

         for (xx = (int)x-((yy==(int)y) ? 1 : 0); xx <= (int)x+((yy==(int)y) ? 1 : 0); xx++) {
            if ((xx < 0) || (xx >= (int)xMax))
               continue;

            if (gawImageColor[yy * (int)xMax + xx] != wThis) {
               fBorder = TRUE;
               break;
            }
         }

         if (fBorder)
            break;
      } // yy

      PCOLOR   *p;
      p = gpPColor + gawImageColor[y * xMax + x];

      DWORD dwRed, dwGreen, dwBlue;
      dwRed = ((DWORD)p->r * dwColorStrength + dwWhite) / 0x100;
      dwGreen = ((DWORD)p->g * dwColorStrength + dwWhite) / 0x100;
      dwBlue = ((DWORD)p->b * dwColorStrength + dwWhite) / 0x100;

      if (fBorder && dwBlackStrength) {
         dwRed = (dwRed * dwBorderColorStrength + dwBlack) / 0x100;
         dwGreen = (dwGreen * dwBorderColorStrength + dwBlack) / 0x100;
         dwBlue = (dwBlue * dwBorderColorStrength + dwBlack) / 0x100;
      }

      gImageColors.PixelSet (x, y,
         (WORD)dwRed,
         (WORD)dwGreen,
         (WORD)dwBlue);
   }

}




/*****************************************************************************
DoViewWindow - pulls up the view window. Returns 1 if next, 0 if quit, -1 if
   back.

inputs
   char     *pszTitle - title
returns
   int - see above
*/
int DoViewWindow (char *pszTitle)
{
   giViewRet = -1;

   // create the window
   // make sure the window is created in specific location
   RECT  r;
   SystemParametersInfo (SPI_GETWORKAREA, 0, (PVOID) &r, 0);

   // based on window's rect
   WINDOWPLACEMENT   wp;
   memset (&wp, 0, sizeof(wp));
   wp.length = sizeof(wp);
   GetWindowPlacement (gpWindow->m_hWnd, &wp);
   ShowWindow (gpWindow->m_hWnd, SW_HIDE);

   ghWndMain = CreateWindowEx (WS_EX_CLIENTEDGE,
      gszMainClass, pszTitle,
      WS_OVERLAPPEDWINDOW | WS_HSCROLL | WS_VSCROLL,
      r.left, r.top, (r.right - r.left), r.bottom - r.top, 
      NULL, NULL,
      ghInstance, NULL);
   SetWindowPlacement (ghWndMain, &wp);


   // Message loop
   MSG msg;
   while (IsWindow(ghWndMain) && GetMessage (&msg, NULL, 0, 0)) {
      TranslateMessage (&msg);
      DispatchMessage (&msg);
   }


   return giViewRet;
}



/*******************************************************************************
ResourceToFile - Loads a resource number and writes a file
*/
BOOL ResourceToFile (DWORD dwRes, char *pszFile)
{
   FILE *f;
   f = fopen(pszFile, "wb");
   if (!f)
      return FALSE;

   HRSRC    hr;
   hr = FindResource (ghInstance, MAKEINTRESOURCE (dwRes), "bin");
   if (!hr) {
      fclose (f);
      return FALSE;
   }

   HGLOBAL  hg;
   hg = LoadResource (ghInstance, hr);
   if (!hg) {
      fclose (f);
      return FALSE;
   }

   PVOID pMem;
   pMem = LockResource (hg);
   if (!pMem) {
      fclose (f);
      return FALSE;
   }

   DWORD dwSize;
   dwSize = SizeofResource (ghInstance, hr);

   // save
   if (dwSize != fwrite (pMem, 1, dwSize, f)) {
      fclose (f);
      return FALSE;
   }

   fclose (f);

   // IMPORTANT - There does not seem to be anyway of freeing pMem, so don't
   return TRUE;
}



int WINAPI WinMain (HINSTANCE hInstance, HINSTANCE hPrevInstance,
                    LPSTR lpCmdLine, int nShowCmd)
{
#ifdef TESTMSG
   MessageBox (NULL, "Special test version", NULL, MB_OK);
#endif

#if 0
   // code to verify inverse hls
   DWORD dwR, dwG, dwB;
   for (dwR = 0; dwR < 256; dwR += 32)
      for (dwG = 0; dwG < 256; dwG += 32)
      for (dwB = 0; dwB < 256; dwB += 32) {
         double   fh, fl, fs, fr, fg, fb;
         ToHLS256 (dwR, dwG, dwB, &fh, &fl, &fs);
         FromHLS256 (fh, fl, fs, &fr, &fg, &fb);

         char szTemp[256];
         sprintf (szTemp, "RGB %d,%d,%d to HLS %d,%d,%d to RGB %d,%d,%d\r\n",
            (int) dwR, (int) dwG, (int)dwB,
            (int) fh, (int) fl, (int) fs,
            (int) fr, (int) fg, (int)fb);
         if ((fabs(fr-dwR) > 5) || (fabs(fg-dwG) > 5) || (fabs(fb-dwB) > 5))
            OutputDebugString (szTemp);
      }
#endif

#ifdef _DEBUG
   // Get current flag
   int tmpFlag = _CrtSetDbgFlag( _CRTDBG_REPORT_FLAG );

   // Turn on leak-checking bit
   tmpFlag |= _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF | _CRTDBG_CHECK_ALWAYS_DF;
   tmpFlag = LOWORD(tmpFlag) | _CRTDBG_CHECK_EVERY_16_DF;
   //tmpFlag |= _CRTDBG_CHECK_ALWAYS_DF | _CRTDBG_DELAY_FREE_MEM_DF;

   // Set flag to the new value
   _CrtSetDbgFlag( tmpFlag );

   // test
   //char *p;
   //p = (char*)malloc (42);
   //p[43] = 0;
#endif // _DEBUG

   // the the locale for the metrics
   int iInches;
   iInches = 0;
   GetLocaleInfo (LOCALE_USER_DEFAULT, LOCALE_IMEASURE, (char*) &iInches, sizeof(iInches));
   gfInches = iInches ? TRUE : FALSE;

   gszCmdLine = lpCmdLine;

   // store the directory away
   GetModuleFileName (hInstance, gszAppPath, sizeof(gszAppPath));
   strcpy (gszAppDir, gszAppPath);
   char  *pCur;
   for (pCur = gszAppDir + strlen(gszAppDir); pCur >= gszAppDir; pCur--)
      if (*pCur == '\\') {
         pCur[1] = 0;
         break;
      }

   EscInitialize (L"mikerozak@bigpond.com", 2511603559, 0);
   EscSoundsSet (0); // BUGBUG - Disable sounds as a test to see if works

   // add an imagegrid control that shows grid
   EscControlAdd (L"ImageGrid", ControlImage);

   // help file
//   strcpy (gszHelp, gszAppDir);
//   strcat (gszHelp, "oilpaint.hlp>Small");

   // registartion
   // RegisterCount (hInstance, NULL);
   gdwRegCount = GetAndIncreaseUsage();

   CoInitialize (NULL);
   InitCommonControls ();

   ghInstance = hInstance;

   // register class
   RegisterWindow ();

   RECT  rt;
   SystemParametersInfo (SPI_GETWORKAREA, 0, &rt, 0);
   CEscWindow  cWindow;

   cWindow.Init (hInstance, NULL, 0, &rt);
   cWindow.IconSet (LoadIcon(ghInstance, MAKEINTRESOURCE(IDI_APPICON)));

   gpWindow = &cWindow;


intro:
   PWSTR psz;
   psz = PageDialog (IDR_MMLFIRSTPAGE, FirstPage);
   if (!_wcsicmp(psz, L"history")) {
      psz = PageDialog (IDR_MMLHISTORY);
      if (psz == gszBack)
         goto intro;
      else
         goto quit;
   }
   else if (!_wcsicmp(psz, L"register")) {
      psz = PageDialog (IDR_MMLREGISTER, RegisterPage);
      if (psz == gszBack)
         goto intro;
      else
         goto quit;
   }
   else if (psz != gszNext)
      goto quit;

   gdwBitViewX = gImageMain.DimX();
   gdwBitViewY = gImageMain.DimY();
   if (!gdwState) {
      giViewCanvasLeft = (int) gdwBitViewX / 4;
      giViewCanvasTop = (int) gdwBitViewY / 4;
      giViewCanvasWidth = (int) gdwBitViewX / 2;
      gfContrast = 1.0; // contrast
      gfOutlineStrength = 0.25;
      gfBrightness = 1.0;  // brightness
      gfColorized = 1.0;   // colorized
      gfGrid = 2;
   }

canvassize:
   // so can reload
   if (gdwState) {
      if (gdwState != VIEWMODE_CANVASSIZE)
         goto movecanvas;
      else
         gdwState = 0;
   }

   psz = PageDialog (IDR_MMLCANVASSIZE, CanvasSizePage);
   if (psz == gszBack)
      goto intro;
   else if (psz != gszNext)
      goto quit;

   // BUGFIX: use the canvas size to adjust the index into the image...
   double fAspectCanvas = gfHeight / gfWidth;
   double fAspectImage = (double)gdwBitViewY / (double)gdwBitViewX;
   double fImageScale;
   if (fAspectCanvas < fAspectImage)
      // canvas is wider, image is taller
      fImageScale = (double)gdwBitViewX / gfWidth;
   else
      // canvas is taller, image is wider
      fImageScale = (double)gdwBitViewY / gfHeight;

   giViewCanvasWidth = (int)(gfWidth * fImageScale);
   giViewCanvasLeft = (int) ((double)gdwBitViewX/2 - gfWidth*fImageScale/2);
   giViewCanvasTop = (int) ((double)gdwBitViewY/2 - gfHeight*fImageScale/2);


movecanvas:
   // view to select where canvas shows
   if (ghBitView)
      DeleteObject (ghBitView);
   if (ghBitWhitened)
      DeleteObject (ghBitWhitened);
   ghBitWhitened = NULL;
   ghBitView = gImageMain.ToBitmap ();
   gdwBitViewX = gImageMain.DimX();
   gdwBitViewY = gImageMain.DimY();
   gdwViewMode = VIEWMODE_CANVASLOCATION;

   // copy and created whitened version
   CImage *pNew;
   DWORD x, y, xMax, yMax;
   WORD r,g,b;
   pNew = gImageMain.Clone ();
   if (pNew) {
      xMax = pNew->DimX();
      yMax = pNew->DimY();
      for (y = 0; y < yMax; y++) for (x = 0; x < xMax; x++) {
         pNew->PixelGet (x, y, &r, &g, &b);
         r = r / 2 + 32767;
         g = g / 2 + 32767;
         b = b / 2 + 32767;
         pNew->PixelSet (x, y, r, g, b);
      }
      ghBitWhitened = pNew->ToBitmap();
      delete pNew;
   }

   // so can reload
   if (gdwState) {
      if (gdwState != VIEWMODE_CANVASLOCATION)
         goto colorize;
      else
         gdwState = 0;
   }

   switch (DoViewWindow("Select the portion of the image you wish to paint and press 'Next'.")) {
   case 1:
      break;
   case -1:
      goto canvassize;
   default:
      goto quit;
   }

colorize:
   // once have canvas size, make cuttout and colorize
   Colorize ();
   // view to select where canvas shows
   if (ghBitView)
      DeleteObject (ghBitView);
   ghBitView = gImageCutout.ToBitmap ();
   gdwBitViewX = gImageCutout.DimX();
   gdwBitViewY = gImageCutout.DimY();
   gdwViewMode = VIEWMODE_COLORIZE;

   // so can reload
   if (gdwState) {
      if (gdwState != VIEWMODE_COLORIZE)
         goto paints;
      else
         gdwState = 0;
   }

   psz = PageDialog (IDR_MMLCOLORIZE, ColorizePage);
   if (psz == gszBack)
      goto movecanvas;
   else if (psz != gszNext)
      goto quit;


paints:
   // so can reload
   if (gdwState) {
      if (gdwState != VIEWMODE_PAINTS)
         goto basecolor;
      else
         gdwState = 0;
   }

redopaints:
   psz = PageDialog (IDR_MMLPAINTS, PaintsPage);
   if (psz == gszBack)
      goto colorize;
   else if (!_wcsicmp(psz, gszSamePage))
      goto redopaints;
   else if (psz != gszNext)
      goto quit;

basecolor:
   if (gdwMix == 3) {   // bypass
      if (ghBitView)
         DeleteObject (ghBitView);
      if (gpPColor)
         free (gpPColor);
      gpPColor = NULL;
      gdwPColorSize = 0;

      ghBitView = gImageCutout.ToBitmap ();
      gdwBitViewX = gImageCutout.DimX();
      gdwBitViewY = gImageCutout.DimY();
   }
   else {
      GeneratePalette ();
      ConvertToColors (gdwBaseDetail,gdwBaseDetail);
      // Clump();
      ColorsToImage ((float)gfOutlineStrength);
      if (ghBitView)
         DeleteObject (ghBitView);
      gdwBitViewX = gImageColors.DimX();
      gdwBitViewY = gImageColors.DimY();
      double fGridX, fGridY;
      CalcGrid (&fGridX, &fGridY);
      ghBitView = gImageColors.ToBitmap (NULL);
   }
   gdwViewMode = VIEWMODE_GRID;

   // so can reload
   if (gdwState) {
      if (gdwState != VIEWMODE_GRID)
         goto print;
      else
         gdwState = 0;
   }

   psz = PageDialog (IDR_MMLGRID, GridPage);
   if (psz == gszBack)
      goto paints;
   else if (psz != gszNext)
      goto quit;
   // grid help

print:
   // so can reload
   if (gdwState) {
      if (gdwState != VIEWMODE_PRINT)
         goto sketch;
      else
         gdwState = 0;
   }

   psz = PageDialog (IDR_MMLPRINT, PrintPage);
   if (psz == gszBack)
      goto basecolor;
   else if (psz != gszNext)
      goto quit;

sketch:
   if (ghBitView)
      DeleteObject (ghBitView);
   gdwBitViewX = gImageCutout.DimX();
   gdwBitViewY = gImageCutout.DimY();
   double fGridX, fGridY;
   CalcGrid (&fGridX, &fGridY);
   ghBitView = gImageCutout.ToBitmap (NULL);
   gdwViewMode = VIEWMODE_SKETCH;

   // so can reload
   if (gdwState) {
      if (gdwState != VIEWMODE_SKETCH)
         goto paintbase;
      else
         gdwState = 0;
   }

   // sketch help
redosketch:
   gdwSketchPageExit = VIEWMODE_SKETCH;
   psz = PageDialog (IDR_MMLSKETCH, SketchPage);
   if (psz == gszBack)
      goto print;
   else if (psz && !_wcsicmp(psz,gszSamePage))
      goto redosketch;
   else if (psz != gszNext)
      goto quit;

paintbase:
   if (ghBitView)
      DeleteObject (ghBitView);
   if (gdwMix == 3) {   // bypass base colors
      ghBitView = gImageCutout.ToBitmap ();
      gdwBitViewX = gImageCutout.DimX();
      gdwBitViewY = gImageCutout.DimY();
   }
   else {
      gdwBitViewX = gImageColors.DimX();
      gdwBitViewY = gImageColors.DimY();
      double fGridX, fGridY;
      CalcGrid (&fGridX, &fGridY);
      ghBitView = gImageColors.ToBitmap (NULL);
   }
   gdwViewMode = VIEWMODE_PAINTBASE;

   // so can reload
   if (gdwState) {
      if (gdwState != VIEWMODE_PAINTBASE)
         goto paintdetails;
      else
         gdwState = 0;
   }

   // paint base
//   WinHelp (NULL, gszHelp, HELP_CONTEXT, IDH_PAINTBASE);
redobase:
   gdwSketchPageExit = VIEWMODE_PAINTBASE;
   psz = PageDialog (IDR_MMLPAINTBASE, SketchPage);
   if (psz == gszBack)
      goto sketch;
   else if (psz && !_wcsicmp(psz,gszSamePage))
      goto redobase;
   else if (psz != gszNext)
      goto quit;

paintdetails:
   if (ghBitView)
      DeleteObject (ghBitView);
   ghBitView = gImageCutout.ToBitmap ();
   gdwBitViewX = gImageCutout.DimX();
   gdwBitViewY = gImageCutout.DimY();
   gdwViewMode = VIEWMODE_PAINTDETAILS;

   // so can reload
   if (gdwState) {
      if (gdwState != VIEWMODE_PAINTDETAILS)
         goto quit;
      else
         gdwState = 0;
   }

redodetails:
   gdwSketchPageExit = VIEWMODE_PAINTDETAILS;
   psz = PageDialog (IDR_MMLPAINTDETAILS, SketchPage);
   if (psz == gszBack)
      goto paintbase;
   else if (psz && !_wcsicmp(psz,gszSamePage))
      goto redodetails;
   else if (psz != gszNext)
      goto quit;

// sign:
   gdwState = 0;
   psz = PageDialog (IDR_MMLSIGN, SignPage);
   if (psz == gszBack)
      goto paintdetails;
   // else, continue onto quit

quit:
   gdwState = 0;
   // free up stuff
   if (ghBitView)
      DeleteObject (ghBitView);
   if (ghBitWhitened)
      DeleteObject (ghBitWhitened);
   if (gpPColor)
      free (gpPColor);
   if (gawImageColor)
      free (gawImageColor);

   CoUninitialize ();

   // kill help
//   WinHelp (NULL, gszHelp, HELP_QUIT, 0);
   EscUninitialize();


   return 0;
}



/*****************************************************************************
ColorSpaceImage - Looks at the image and filles in the gadwColorCount array
with the number of pixels of that color.
*/
void ColorSpaceImage (void)
{
   // clear it out
   memset (gadwColorCount, 0, sizeof(gadwColorCount));

   // loop
   DWORD x,y, x2, y2, dwR, dwG, dwB, dwCount;
   WORD wr, wg, wb;
   for (x = 0; x < gImageCutout.DimX(); x += 3) for (y = 0; y < gImageCutout.DimY(); y += 3) {
      dwCount = 0;
      dwR = dwG = dwB = 0;

      for (x2 = x; (x2 < x+3) && (x2 < gImageCutout.DimX()); x2++)
         for (y2 = y; (y2 < y+3) && (y2 < gImageCutout.DimY()); y2++) {
            gImageCutout.PixelGet (x2, y2, &wr, &wg, &wb);
            dwR += wr;
            dwG += wg;
            dwB += wb;
            dwCount++;
         }

      // normalize
      dwR /= dwCount;
      dwG /= dwCount;
      dwB /= dwCount;

      // convert to HLS
      WORD h, l, s;
      ToHLS ((WORD) gImageMain.UnGamma((WORD)dwR) * 256,
         (WORD) gImageMain.UnGamma((WORD)dwG) * 256,
         (WORD) gImageMain.UnGamma((WORD)dwB) * 256,
         &h, &l, &s);

      // count in global
      h = h >> 12;
      l = l >> 13;
      s = s >> 14;
      gadwColorCount[h][l][s]++;
   }

}

/*****************************************************************************
ColorSpaceSubst - Fills gmemtemp with substitution for the color space

returns
   PWSTR - gmemtemp.p
*/
PWSTR ColorSpaceSubst (void)
{
   DWORD    i,j, k;
   PWSTR    pszShading = L"<Lightambient val=.4/><lightintensity val=.6/>";
   PWSTR    pszNoShading = L"<Lightambient val=1/><lightintensity val=0/>";

   MemZero (&gMemTemp);

   MemCat (&gMemTemp, L"<scale x=.023/>");

   // MemCat (&gMemTemp, L"<FogRange start=2 end=-2/><FogOn/>");
   MemCat (&gMemTemp, pszNoShading);

   // ********** do the color loop
   // figure out 16 points around
#define  FULLSPACE   16
   double aPntFullSpace[FULLSPACE][3];
   COLORREF acFullSpace[FULLSPACE];
   double aPntPoints[3][3];
   COLORREF acPoints[3];

   double fRed, fGreen, fBlue;
   int x,y,z;
   WORD h, l, s;
   for (i = 0; i < FULLSPACE; i++) {
      h = (WORD) i * 16;
      l = 128;
      s = 255;
      FromHLS256(h, l, s, &fRed, &fGreen, &fBlue);
      acFullSpace[i] = RGB((BYTE)fRed, (BYTE)fGreen, (BYTE) fBlue);
      ToXYZFromHLS (h * 255, l * 255, s * 255, &x, &y, &z);

      aPntFullSpace[i][0] = x;
      aPntFullSpace[i][1] = y;
      aPntFullSpace[i][2] = z;
   }
   // white point
   h = l = s = 255;
   acPoints[0] = RGB(0xff, 0xff, 0xff);
   ToXYZFromHLS (h * 255, l * 255, s * 255, &x, &y, &z);
   aPntPoints[0][0] = x;
   aPntPoints[0][1] = y;
   aPntPoints[0][2] = z;

   // black point
   h = l = s = 0;
   acPoints[1] = RGB(0,0,0);
   ToXYZFromHLS (h * 255, l * 255, s * 255, &x, &y, &z);
   aPntPoints[1][0] = x;
   aPntPoints[1][1] = y;
   aPntPoints[1][2] = z;

   // center point
   h = s = 0;
   l = 128;
   acPoints[1] = RGB(0x80,0x80,0x80);
   ToXYZFromHLS (h * 255, l * 255, s * 255, &x, &y, &z);
   aPntPoints[2][0] = x;
   aPntPoints[2][1] = y;
   aPntPoints[2][2] = z;

   // ring
   MemCat (&gMemTemp, L"<shapeline arrow=false");
   for (i = 0; i <= FULLSPACE; i++) {
      MemCat (&gMemTemp, L" c");
      MemCat (&gMemTemp, (int) i+1);
      MemCat (&gMemTemp, L"=");
      MemCatColor (&gMemTemp, acFullSpace[i % FULLSPACE]);

      MemCat (&gMemTemp, L" p");
      MemCat (&gMemTemp, (int) i+1);
      MemCat (&gMemTemp, L"=");
      MemCatPoint (&gMemTemp, aPntFullSpace[i % FULLSPACE]);

   }
   MemCat (&gMemTemp, L"/>");

   // do lines up and in
   for (i = 0; i <= FULLSPACE; i++) for (j=0; j < 3; j++) {
      MemCat (&gMemTemp, L"<shapeline arrow=false");

      // color point
      MemCat (&gMemTemp, L" c1=");
      MemCatColor (&gMemTemp, acFullSpace[i % FULLSPACE]);
      MemCat (&gMemTemp, L" p1=");
      MemCatPoint (&gMemTemp, aPntFullSpace[i % FULLSPACE]);

      // non-color point
      MemCat (&gMemTemp, L" c2=");
      MemCatColor (&gMemTemp, acPoints[j]);
      MemCat (&gMemTemp, L" p2=");
      MemCatPoint (&gMemTemp, aPntPoints[j]);

      MemCat (&gMemTemp, L"/>");
   }

   // **** Display the paints
   if (gfSpaceShowPaints) {
      MemCat (&gMemTemp, gfSpaceShowPaintsShading ? pszShading : pszNoShading);
      for (i = 0; i < gListSPACEPAINT.Num(); i++) {
         PSPACEPAINT ps = (PSPACEPAINT) gListSPACEPAINT.Get(i);

         // if not supposed to show then dont
         if (!ps->fShow)
            continue;

         MemCat (&gMemTemp, L"<matrixpush><translate point=");
         MemCatPoint (&gMemTemp, ps->afXYZ);
         MemCat (&gMemTemp, L"/><colordefault color=");
         MemCatColor (&gMemTemp, gfSpaceShowPaintsSolid ? RGB(0x40,0x40,0xc0) : ps->rgb);
         MemCat (&gMemTemp, L"/><MeshEllipsoid x=20 y=20 z=20/>");
         MemCat (&gMemTemp, L"<shapemeshsurface/>");
         MemCat (&gMemTemp, L"</matrixpush>");
      }
   }

   // ****** Display a mix of paints
   if (gfSpaceShowCoverage) {
      MemCat (&gMemTemp, gfSpaceShowCoverageShading ? pszShading : pszNoShading);
      MemCat (&gMemTemp, L"<colordefault color=#40c040/>");

      for (i = 0; i < gListSPACEPAINT.Num(); i++) {
         PSPACEPAINT ps = (PSPACEPAINT) gListSPACEPAINT.Get(i);

         // if not supposed to show then dont
         if (!ps->fShow)
            continue;

         // show what happens if mix with other colors
         DWORD dwMix;
   #define  MAXMIXES    4
   #define  MAXWHITE    3
         COLORREF    aMixColor[MAXMIXES+1][MAXWHITE+1];
         WORD        awMixRGB[MAXMIXES+1][MAXWHITE+1][3];
         double      afMixXYZ[MAXMIXES+1][MAXWHITE+1][3];
         for (j = i + 1; j < gListSPACEPAINT.Num(); j++) {
            PSPACEPAINT ps2 = (PSPACEPAINT) gListSPACEPAINT.Get(j);

            // if not supposed to show then dont
            if (!ps2->fShow)
               continue;

            // set end points
            aMixColor[0][0] = ps->rgb;
            memcpy (awMixRGB[0][0], ps->awRGB, sizeof(ps->awRGB));
            memcpy (afMixXYZ[0][0], ps->afXYZ, sizeof(ps->afXYZ));
            aMixColor[MAXMIXES][0] = ps2->rgb;
            memcpy (afMixXYZ[MAXMIXES][0], ps2->afXYZ, sizeof(ps2->afXYZ));
            memcpy (awMixRGB[MAXMIXES][0], ps2->awRGB, sizeof(ps2->awRGB));

            for (dwMix = 1; dwMix < MAXMIXES; dwMix++) {
               SubtractiveMix (awMixRGB[0][0][0], awMixRGB[0][0][1], awMixRGB[0][0][2], 
                  awMixRGB[MAXMIXES][0][0], awMixRGB[MAXMIXES][0][1], awMixRGB[MAXMIXES][0][2],
                  &awMixRGB[dwMix][0][0], &awMixRGB[dwMix][0][1], &awMixRGB[dwMix][0][2],
                  (double) (MAXMIXES-dwMix) / MAXMIXES);

               // ungamma and convert to RGB
               aMixColor[dwMix][0] = gImageColors.UnGamma(awMixRGB[dwMix][0][0], awMixRGB[dwMix][0][1], awMixRGB[dwMix][0][2]);

               // convert to XYZ
               int iMix[3];
               ToXYZ (awMixRGB[dwMix][0][0], awMixRGB[dwMix][0][1], awMixRGB[dwMix][0][2], &iMix[0], &iMix[1], &iMix[2]);

               // and to doubles
               for (k = 0; k < 3; k++)
                  afMixXYZ[dwMix][0][k] = iMix[k];
            }

            // mix with black or white
            DWORD dwBlack, x, y;
            WORD dwCR, dwCG, dwCB;;

            // if we're doing the normal black and white then go from 0..1,
            // else if we're mixing three non-black and white then j+1..# colors
            DWORD dwStart, dwMax;
            dwStart = gfSpaceShowCoverageThird ? (j + 1) : 0;
            dwMax = gfSpaceShowCoverageThird ? gListSPACEPAINT.Num() : 2;
            for (dwBlack = dwStart; dwBlack < dwMax; dwBlack++) {

               // what colors are we mixing with
               if (gfSpaceShowCoverageThird) {
                  PSPACEPAINT ps3 = (PSPACEPAINT) gListSPACEPAINT.Get(dwBlack);

                  // if it's disabled skip
                  if (!ps3->fShow)
                     continue;

                  // if any of the colors is black or white then skip
                  if ( (ps->rgb == RGB(0,0,0)) || (ps2->rgb == RGB(0,0,0)) || (ps3->rgb == RGB(0,0,0)) ||
                     (ps->rgb == RGB(255,255,255)) || (ps2->rgb == RGB(255,255,255)) || (ps3->rgb == RGB(255,255,255)) )
                     continue;

                  // remember the rgb
                  dwCR = ps3->awRGB[0];
                  dwCG = ps3->awRGB[1];
                  dwCB = ps3->awRGB[2];
               }
               else {
                  // black and white
                  if (dwBlack) {
                     dwCR = gImageMain.Gamma (GetRValue(gcBaseBlack));
                     dwCG = gImageMain.Gamma (GetGValue(gcBaseBlack));
                     dwCB = gImageMain.Gamma (GetBValue(gcBaseBlack));
                  }
                  else {
                     dwCR = dwCG = dwCB = 0xffff;
                  }
               }
               for (x = 0; x < MAXMIXES+1; x++) for (y = 1; y < MAXWHITE+1; y++) {
                  SubtractiveMix (awMixRGB[x][0][0], awMixRGB[x][0][1], awMixRGB[x][0][2], 
                     dwCR, dwCG, dwCB,
                     &awMixRGB[x][y][0], &awMixRGB[x][y][1], &awMixRGB[x][y][2],
                     (double) (MAXWHITE-y) / MAXWHITE + .02);
                  // + .02 is so that not entirely white or black

                  // ungamma and convert to RGB
                  aMixColor[x][y] = gImageColors.UnGamma(awMixRGB[x][y][0], awMixRGB[x][y][1], awMixRGB[x][y][2]);

                  // convert to XYZ
                  int iMix[3];
                  ToXYZ (awMixRGB[x][y][0], awMixRGB[x][y][1], awMixRGB[x][y][2], &iMix[0], &iMix[1], &iMix[2]);

                  // and to doubles
                  for (k = 0; k < 3; k++)
                     afMixXYZ[x][y][k] = iMix[k];
               }

               // define the points
               for (x = 0; x < MAXMIXES+1; x++) for (y = 0; y < MAXWHITE+1; y++) {
                  DWORD dwIndex;
                  if (dwBlack)
                     dwIndex = (MAXMIXES-x) + y * (MAXMIXES+1)+1;
                  else
                     dwIndex = x + y * (MAXMIXES+1)+1;

                  if (!gfSpaceShowCoverageSolid) {
                     MemCat (&gMemTemp, L"<defcolor num=");
                     MemCat (&gMemTemp, (int)dwIndex);
                     MemCat (&gMemTemp, L" color=");
                     MemCatColor (&gMemTemp, aMixColor[x][y]);
                     MemCat (&gMemTemp, L"/>");
                  }

                  MemCat (&gMemTemp, L"<defpoint num=");
                  MemCat (&gMemTemp, (int)dwIndex);
                  MemCat (&gMemTemp, L" point=");
                  MemCatPoint (&gMemTemp, afMixXYZ[x][y]);
                  MemCat (&gMemTemp, L"/>");
               }

               MemCat (&gMemTemp, L"<backculloff/>");

               // color map
               if (!gfSpaceShowCoverageSolid) {
                  MemCat (&gMemTemp, L"<colormap start=1 xcount=");
                  MemCat (&gMemTemp, (int) MAXMIXES+1);
                  MemCat (&gMemTemp, L" ycount=");
                  MemCat (&gMemTemp, (int) MAXWHITE+1);
                  MemCat (&gMemTemp, L"/>");
               }

               // do the shape
               MemCat (&gMemTemp, L"<meshfrompoints start=1 xcount=");
               MemCat (&gMemTemp, (int) MAXMIXES+1);
               MemCat (&gMemTemp, L" ycount=");
               MemCat (&gMemTemp, (int) MAXWHITE+1);
               MemCat (&gMemTemp, L"/>");

               // draw
               MemCat (&gMemTemp, L"<shapemeshsurface/>");
               MemCat (&gMemTemp, L"<backcullon/>");
            }

         }
      }
   }

   // **** Show what colors needed for the image
   //DWORD h, l, s;
   if (gfSpaceShowImage) {
      MemCat (&gMemTemp, gfSpaceShowImageShading ? pszShading : pszNoShading);
      MemCat (&gMemTemp, L"<colordefault color=#c04040/>");

      for (h = 0; h < 16; h++) for (l = 0; l < 8; l++) for (s = 0; s < 4; s++) {
         // if it's empty then continue
         if (!gadwColorCount[h][l][s])
            continue;

         // look at up, down, left, right, front, back
         BOOL  afEmpty[6];
         int   ih, il, is;
         DWORD dwEmptyCount;
         dwEmptyCount = 0;
         for (i = 0; i < 6; i++) {
            ih = (int)h + ((i == 0) ? 1 : 0) - ((i == 1) ? 1 : 0);
            il = (int)l + ((i == 2) ? 1 : 0) - ((i == 3) ? 1 : 0);
            is = (int)s + ((i == 4) ? 1 : 0) - ((i == 5) ? 1 : 0);

            // if on border then empty, so draw wall, except is <= 0, which doesn't matter
            if ((ih < 0) || (ih >= 16) || (il < 0) || (il >= 8) || (is >= 4)) {
               afEmpty[i] = TRUE;
               dwEmptyCount++;
               continue;
            }

            // check it
            afEmpty[i] = !gadwColorCount[ih][il][is];
            if (afEmpty[i])
               dwEmptyCount++;
         }
         // if all full then continue
         if (!dwEmptyCount)
            continue;

         // calculate XYZ for edges
         double   afEdge[2][2][2][3];
         COLORREF acEdge[2][2][2];
         DWORD h2,l2,s2;
         for (h2 = 0; h2 < 2; h2++) for (l2 = 0; l2 < 2; l2++) for (s2 = 0; s2 < 2; s2++) {
            int x, y, z;
            WORD h3, l3, s3;
            h3 = (h << 12) + (h2 ? 0x1000 : 0);
            l3 = (l << 13) + (l2 ? ((l < 7) ? 0x2000 : 0x1fff) : 0);
            s3 = (s << 14) + (s2 ? ((s < 3) ? 0x4000 : 0x3fff) : 0);

            // XYZ location
            ToXYZFromHLS (h3,l3,s3, &x, &y, &z);
            afEdge[h2][l2][s2][0] = x;
            afEdge[h2][l2][s2][1] = y;
            afEdge[h2][l2][s2][2] = z;

            // color
            double fr, fg, fb;
            FromHLS256 (h3 / 256, l3 / 256, s3 / 256, &fr, &fg, &fb);
            acEdge[h2][l2][s2] = RGB((BYTE)fr, (BYTE)fg, (BYTE)fb);

         }

         // draw the edges
         for (i = 0; i < 6; i++) {
            if (!afEmpty[i])
               continue;

            double   *(paEdge4[4]);
            COLORREF acEdge4[4];

            switch (i) {
            case 2:  // top
               paEdge4[0] = afEdge[0][1][0];
               acEdge4[0] = acEdge[0][1][0];
               paEdge4[1] = afEdge[1][1][0];
               acEdge4[1] = acEdge[1][1][0];
               paEdge4[2] = afEdge[1][1][1];
               acEdge4[2] = acEdge[1][1][1];
               paEdge4[3] = afEdge[0][1][1];
               acEdge4[3] = acEdge[0][1][1];
               break;
            case 3:  // bottom
               paEdge4[3] = afEdge[0][0][0];
               acEdge4[3] = acEdge[0][0][0];
               paEdge4[2] = afEdge[1][0][0];
               acEdge4[2] = acEdge[1][0][0];
               paEdge4[1] = afEdge[1][0][1];
               acEdge4[1] = acEdge[1][0][1];
               paEdge4[0] = afEdge[0][0][1];
               acEdge4[0] = acEdge[0][0][1];
               break;
            case 0:  // right
               paEdge4[3] = afEdge[1][1][0];
               acEdge4[3] = acEdge[1][1][0];
               paEdge4[2] = afEdge[1][1][1];
               acEdge4[2] = acEdge[1][1][1];
               paEdge4[1] = afEdge[1][0][1];
               acEdge4[1] = acEdge[1][0][1];
               paEdge4[0] = afEdge[1][0][0];
               acEdge4[0] = acEdge[1][0][0];
               break;
            case 1:  // left
               paEdge4[0] = afEdge[0][1][0];
               acEdge4[0] = acEdge[0][1][0];
               paEdge4[1] = afEdge[0][1][1];
               acEdge4[1] = acEdge[0][1][1];
               paEdge4[2] = afEdge[0][0][1];
               acEdge4[2] = acEdge[0][0][1];
               paEdge4[3] = afEdge[0][0][0];
               acEdge4[3] = acEdge[0][0][0];
               break;
            case 5:  // front
               paEdge4[3] = afEdge[0][1][0];
               acEdge4[3] = acEdge[0][1][0];
               paEdge4[2] = afEdge[1][1][0];
               acEdge4[2] = acEdge[1][1][0];
               paEdge4[1] = afEdge[1][0][0];
               acEdge4[1] = acEdge[1][0][0];
               paEdge4[0] = afEdge[0][0][0];
               acEdge4[0] = acEdge[0][0][0];
               break;
            case 4:  // back
               paEdge4[0] = afEdge[0][1][1];
               acEdge4[0] = acEdge[0][1][1];
               paEdge4[1] = afEdge[1][1][1];
               acEdge4[1] = acEdge[1][1][1];
               paEdge4[2] = afEdge[1][0][1];
               acEdge4[2] = acEdge[1][0][1];
               paEdge4[3] = afEdge[0][0][1];
               acEdge4[3] = acEdge[0][0][1];
               break;
            }

            // draw the polygon
            MemCat (&gMemTemp, L"<shapepolygon");
            for (j = 0; j < 4; j++) {
               if (!gfSpaceShowImageSolid) {
                  MemCat (&gMemTemp, L" c");
                  MemCat (&gMemTemp, (int) j+1);
                  MemCat (&gMemTemp, L"=");
                  MemCatColor (&gMemTemp, acEdge4[j]);
               }

               MemCat (&gMemTemp, L" p");
               MemCat (&gMemTemp, (int) j+1);
               MemCat (&gMemTemp, L"=");
               MemCatPoint (&gMemTemp, paEdge4[j]);

            }
            MemCat (&gMemTemp, L"/>");

         }
      }
   }

   return (PWSTR) gMemTemp.p;
}



/*
I was thinking about printing zoomed images but thought the UI might be a bit complex for most people and didn't think it'd get too many users. But, since you've requested it I'll reconsider it.

Doesn't printing each base-color separately result in heaps of paper? I understand why you're doing it. Right now I'm trying to think of an alternative that might work just as well but not use as much paper.

Close .opa. I should look into this. Do you keep the orignal .bmp in the same place on disk?

-----Original Message-----
From: Paul Breeden <mikko21@earthlink.net>
To: MikeRozak@BigPond.Com.Au <MikeRozak@BigPond.Com.Au>
Date: Saturday, November 06, 1999 12:36 AM
Subject: Oil Painting Assistant


>Great program Mike !!!!Just the grid feature alone is worth it.. I don't
>know how many times I've drawn grids on photos then tried to
>mattematicly tried to convert the size to fit my canvas. One possible
>suggestion would be to enable printing of zoomed images. I presently do
>a screen capture and transfer that to Photoshop then print it. It works
>pretty good.You can also select each base coat color seperately Tranfer
>that to a blank background and print it with the grid. Also helpful if
>you don't want to sit in front of computer while painting. Can be time
>consuming if you work with a lot of colors tho. I've also found that if
>I try to work from a saved .opa the program just closes??? All in all
>tho it's an awesome program, wish i had thought of it !!! Do you have
>any samples of your work online ? I'd like to see them...I'll send you a
>photo of the one I'm working on now when it's done.. Thanx again for a
>great idea..
>
*/



// BUGBUG - Be able to do paint by numbers - which means outline and number.
//          Why? Can only print to paper, and no one will trace out it all to a
//          canvas.



// BUGBUG - Warn people that if have internet popup killer may prevent tooltips
// from appearing. Do so 1st time run and when run tutotial


// From: Dennis O'Connor 
// Suggestions: 1. How about a 'Help'page?
//                     2. When you get to the page showing the section of the image you want. There is no help letting you know
//                         that you can resize the 'window' of selection.
//                     3. You can only resize the selection window (when you figure it out) horizontally or vertically. Why not
//                        diagonally?
//                     4. What about a 'Watercolour' version?


// From zon: Recommends that printing boxes not be checked by default

// From DDD:
// Still using your indispensable Oil Painting Assistant for gridding my paintings.
// Would really like to save the finished (gridded or non-gridded, but at any
// rate cropped) photo file as a JPEG, so I can plug it into these on-line
// framing services and visualize the "painting" in a frame, so I can order
// a frame before the painting is finished.  I also need it in JPEG OPA is
// where I crop & size the photo to the canvas dimensions--need to have that
// in a JPEG format.
//    Also, would be great if there were some print preview options displayed,
// especially for "full size, multiple sheets" so one could decide whether to
// change the printout to landscape to save paper.
//    Please let me know if you decide to implement any of these; maybe I
// could send another bit of money.