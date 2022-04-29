/***********************************************************************
ControlThreeD.cpp - Code for a control

begun 4/8/2000 by Mike Rozak
Copyright 2000 mike Rozak. All rights reserved
*/


#include <windows.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include "escarpment.h"
#include "resleak.h"

typedef struct {
   PWSTR       pszHRef;
   PWSTR       pszScrollRot[3];    // control to rotate about XYZ
   PWSTR       pszScrollDistance;
   double      fMinDistance;     // positive
   double      fMaxDistance;     // positive
   int         iTimerInterval;   // timer interval
   int         iTime;            // current time
   COLORREF       cBorder;
   int            iBorderSize;
   PCListFixed    plLinkID;      // DWORDs associated with link IDs
   PCListVariable plLinkHRef;    // HRef when click on ID

   PCEscControl   pScroll[3], pScrollDistance;
   BOOL        fRecalc;
   int         iDistance;
   int         iRot[3];
   DWORD       dwRotating;       // current dimension rotating
   Matrix      mRot;             // curent rotation matrix
   POINT       pOffset;          // add to page coords to get HDC coords

   CRender     *pRender;
   DWORD          dwDefPoints;    // number of def points allocated for
   DWORD          dwDefColors;    // number of def colors allocated for
   double*        pafDefPoints;   // array of points. Size = dwDefPoints * 4 * sizeof(double)
   double*        pafDefColors;   // array of the default color values
   char           aszAxis[3][256];// axis names
   HDC            hDC;           // used during rendering
   PCMMLNode      pReplaceNode;  // if not NULL, use this as replacement node for rendering
} THREED, *PTHREED;

#define  NULLOBJECT        0x53481243  // a random number
#define  MAXPOLYVERT       40       // maximum vertices

void ScanForSubNodes (PCEscControl pControl, THREED *pc, PCMMLNode pNode);

/***********************************************************************
ParseInstruction - Given a pNode with an instruction parses it and
acts upon it.

inputs
   PCEscControl      pControl - control
   THREED            *pc - control info
   PCMMLNode         pNode - node to look for
returns
   none
*/
void ParseInstruction (PCEscControl pControl, THREED *pc, PCMMLNode pNode)
{
   PWSTR psz;
   psz = pNode->NameGet();
   if (!psz)
      return;  // no name

   // pull out some common values
   PWSTR pszVal, pszName, pszColor, pszXCount, pszYCount, pszStart, pszFile, pszAxis, pszNum, pszPoint;
   double   fVal;
   COLORREF cColor;
   int   iXCount, iYCount, iStart, iAxis, iNum;
   pnt   pPoint;
   AttribToDouble (pszVal = pNode->AttribGet(L"val"), &fVal);
   pszName = pNode->AttribGet(L"name");
   AttribToColor (pszColor = pNode->AttribGet(L"color"), &cColor);
   AttribToDecimal (pszXCount = pNode->AttribGet(L"xcount"), &iXCount);
   AttribToDecimal (pszYCount = pNode->AttribGet(L"ycount"), &iYCount);
   AttribToDecimal (pszStart = pNode->AttribGet(L"start"), &iStart);
   AttribToDecimal (pszAxis = pNode->AttribGet(L"axis"), &iAxis);
   AttribToDecimal (pszNum = pNode->AttribGet(L"num"), &iNum);
   AttribTo3DPoint (pszPoint = pNode->AttribGet(L"point"), pPoint);
   pszFile = pNode->AttribGet(L"file");


   // as a speed up, do a case statement on the first character before moving on
   switch (towlower (psz[0])) {
   case L'a':
      if (!_wcsicmp (psz, L"AxisX") || !_wcsicmp(psz, L"AxisY") || !_wcsicmp(psz, L"AxisZ")) {
         DWORD i;
         i = (DWORD) towlower(psz[4]) - L'x';

         if (pszName)
            WideCharToMultiByte (CP_ACP, 0, pszName, -1, pc->aszAxis[i], sizeof(pc->aszAxis[i]), 0, 0);
         else
            pc->aszAxis[i][0] = 0;
      }
      else if (!_wcsicmp (psz, L"AxisMaxLines"))
         pc->pRender->m_dwMaxAxisLines = (DWORD) fVal;
      break;

   case L'b':
      if (!_wcsicmp (psz, L"BackCullOn"))
         pc->pRender->m_fBackCull = TRUE;
      else if (!_wcsicmp (psz, L"BackCullOff"))
         pc->pRender->m_fBackCull = FALSE;
      else if (!_wcsicmp (psz, L"BumpMapApply"))
            pc->pRender->BumpMapApply();
      else if (!_wcsicmp (psz, L"BumpMap")) {
         DWORD x, y, i;
         x = (DWORD) iXCount;
         y = (DWORD) iYCount;
         i = (DWORD) iStart;

         // make sure doesn't exceed point limit
         if (((i + x * y) > pc->dwDefPoints) || (x*y == 0))
            return;

         double   *pf;
         pf = (double*) ESCMALLOC (x * y * sizeof(double));
         if (!pf)
            return;
         DWORD l;
         for (l = 0; l < x * y; l++)
            pf[l] = pc->pafDefPoints[(i+l) * 4];

         // do it
         pc->pRender->BumpMap (x, y, pf);

         ESCFREE (pf);
      }
      else if (!_wcsicmp (psz, L"BumpMapFree")) {
         pc->pRender->BumpMapFree();
      }
      else if (!_wcsicmp (psz, L"BumpMapScale")) {
         pc->pRender->m_fBumpScale = fVal;
      }
      else if (!_wcsicmp (psz, L"BumpMapFromBitmap")) {
         DWORD dwBmpResource = 0, dwJpgResource;
         AttribToDecimal (psz = (WCHAR*) pNode->AttribGet (L"bmpresource"), (int*) &dwBmpResource);
         AttribToDecimal (psz = (WCHAR*) pNode->AttribGet (L"jpgresource"), (int*) &dwJpgResource);
         if (!pszFile && !dwBmpResource && !dwJpgResource)
            return;

         DWORD dwColor = (DWORD) iAxis;
         if (dwColor < 1)
            dwColor = 2;
         if (dwColor > 3)
            return;

         HBITMAP hBit;
         PCBitmapCache pCache;
         pCache = EscBitmapCache();
         DWORD dwID;
         // BUGFIX - Allow bitmap to be cahced
         if (dwBmpResource)
            dwID = pCache->CacheResourceBMP (dwBmpResource, pControl->m_hInstance, &hBit);
         else if (dwJpgResource)
            dwID = pCache->CacheResourceJPG (dwJpgResource, pControl->m_hInstance, &hBit);
         else
            dwID = pCache->CacheFile (pszFile, &hBit);
         // BUGBUG - 2.0 - allow to use resource
         if (!dwID || !hBit)
            return;

         pc->pRender->BumpMapFromBitmap (hBit, dwColor-1);

         pCache->CacheRelease(dwID);
      }
      break;


   case L'c':
      if (!_wcsicmp (psz, L"ColorMap")) {
         DWORD x, y, i;
         x = (DWORD) iXCount;
         y = (DWORD) iYCount;
         i = (DWORD) iStart;

         // make sure doesn't exceed coplor limit
         if (((i + x * y) > pc->dwDefColors) || (x*y == 0))
            return;

         DWORD   *pf;
         pf = (DWORD*) ESCMALLOC (x * y * sizeof(DWORD));
         if (!pf) return;
         DWORD l;
         for (l = 0; l < x * y; l++) {
            double *p;
            p = pc->pafDefColors + ((i+l) * 4);
            pf[l] = RGB(p[0], p[1], p[2]);
         }

         // do it
         pc->pRender->ColorMap (x, y, pf);

         ESCFREE (pf);
      }
      else if (!_wcsicmp (psz, L"ColorMapFree")) {
         pc->pRender->ColorMapFree();
      }
      else if (!_wcsicmp (psz, L"ColorMapFromBitmap")) {
         DWORD dwBmpResource = 0, dwJpgResource;
         AttribToDecimal (psz = (WCHAR*) pNode->AttribGet (L"bmpresource"), (int*) &dwBmpResource);
         AttribToDecimal (psz = (WCHAR*) pNode->AttribGet (L"jpgresource"), (int*) &dwJpgResource);
         if (!pszFile && !dwBmpResource && !dwJpgResource)
            return;

         HBITMAP hBit;
         PCBitmapCache pCache;
         pCache = EscBitmapCache();
         DWORD dwID;
         // BUGFIX - Allow bitmap to be cahced
         if (dwBmpResource)
            dwID = pCache->CacheResourceBMP (dwBmpResource, pControl->m_hInstance, &hBit);
         else if (dwJpgResource)
            dwID = pCache->CacheResourceJPG (dwJpgResource, pControl->m_hInstance, &hBit);
         else
            dwID = pCache->CacheFile (pszFile, &hBit);
         // BUGBUG - 2.0 - allow to use resource
         if (!dwID || !hBit)
            return;

         pc->pRender->ColorMapFromBitmap (hBit);

         pCache->CacheRelease(dwID);
      }
      else if (!_wcsicmp (psz, L"ColorDefault")) {
         pc->pRender->m_DefColor[0] = GetRValue(cColor);
         pc->pRender->m_DefColor[1] = GetGValue(cColor);
         pc->pRender->m_DefColor[2] = GetBValue(cColor);
      }
#if 0 // clear doenst really make sence
      else if (!_wcsicmp (psz, L"Clear")) {
         // always use black background for transparency

         pc->pRender->Clear();
      }
#endif
      break;

   case L'd':
      if (!_wcsicmp (psz, L"DefPoint")) {
         // get the point number and three values
         DWORD dwNum;
         dwNum = (DWORD) iNum;

         // allocate enough memory
         if (!pc->pafDefPoints) {
            pc->dwDefPoints = dwNum + 128;
            pc->pafDefPoints = (double*) ESCMALLOC (pc->dwDefPoints * 4 * sizeof(double));
         }
         if ((dwNum >= pc->dwDefPoints) && pc->pafDefPoints) {
            pc->dwDefPoints = dwNum + 128;
            pc->pafDefPoints = (double*) ESCREALLOC (pc->pafDefPoints, pc->dwDefPoints * 4 * sizeof(double));
         }
         if (!pc->pafDefPoints) return;

         // copy it over
         pc->pafDefPoints[dwNum*4 + 0] = pPoint[0];
         pc->pafDefPoints[dwNum*4 + 1] = pPoint[1];
         pc->pafDefPoints[dwNum*4 + 2] = pPoint[2];
         pc->pafDefPoints[dwNum*4 + 3] = 1;
      }
      else if (!_wcsicmp (psz, L"DefColor")) {
         // get the point number and three values
         DWORD dwNum;
         dwNum = (DWORD) iNum;


         // allocate enough memory
         if (!pc->pafDefColors) {
            pc->dwDefColors = dwNum + 128;
            pc->pafDefColors = (double*) ESCMALLOC (pc->dwDefColors * 4 * sizeof(double));
         }
         if ((dwNum >= pc->dwDefColors) && pc->pafDefColors) {
            pc->dwDefColors = dwNum + 128;
            pc->pafDefColors = (double*) ESCREALLOC (pc->pafDefColors, pc->dwDefColors * 4 * sizeof(double));
         }
         if (!pc->pafDefColors) return;

         // copy it over
         double *p;
         p = pc->pafDefColors + (dwNum*4);

         p[0] = GetRValue(cColor);
         p[1] = GetGValue(cColor);
         p[2] = GetBValue(cColor);
         p[3] = 1;
      }
      else if (!_wcsicmp (psz, L"DrawNormalsOn")) {
         pc->pRender->m_fDrawNormals = TRUE;
      }
      else if (!_wcsicmp (psz, L"DrawNormalsOff")) {
         pc->pRender->m_fDrawNormals = FALSE;
      }
      else if (!_wcsicmp (psz, L"DrawFacetsOn")) {
         pc->pRender->m_fDrawFacets = TRUE;
      }
      else if (!_wcsicmp (psz, L"DrawFacetsOff")) {
         pc->pRender->m_fDrawFacets = FALSE;
      }
      break;

   case L'f':
      if (!_wcsicmp (psz, L"FogOn")) {
         pc->pRender->m_fFogOn = TRUE;
      }
      if (!_wcsicmp (psz, L"FogOff")) {
         pc->pRender->m_fFogOn = FALSE;
      }
      else if (!_wcsicmp (psz, L"FogRange")) {
         double   zStart, zEnd;
         AttribToDouble(pNode->AttribGet(L"start"), &zStart);
         AttribToDouble(pNode->AttribGet(L"end"), &zEnd);
         if (zStart <= zEnd) return;

         pc->pRender->FogRange (zStart, zEnd);
      }
      else if (!_wcsicmp (psz, L"FacetsMax")) {
         pc->pRender->m_dwMaxFacets = max ((DWORD) fVal, 2);
      }
      else if (!_wcsicmp (psz, L"FacetsPixelsPer")) {
         pc->pRender->m_fPixelsPerFacet = fVal;
      }
      break;

   case L'i':
      if (!_wcsicmp (psz, L"IfTime")) {
         DWORD dwDim;
         dwDim = (DWORD) towlower(psz[6]) - L'x' + 1;

         int      fFrom, fTo;
         int      iElapse;
         BOOL     fBack;
         if (!AttribToDecimal (pNode->AttribGet (L"from"), &fFrom))
            fFrom = 0;
         if (!AttribToDecimal (pNode->AttribGet (L"to"), &fTo))
            fTo = 0;
         if (!AttribToDecimal (pNode->AttribGet (L"interval"), &iElapse))
            iElapse = 36;
         if (iElapse <= 1)
            iElapse = 2;
         if (!AttribToYesNo (pNode->AttribGet (L"back"), &fBack))
            fBack = FALSE;

         // real time
         int      iTime;
         iTime = pc->iTime % iElapse;
         if (fBack && ((pc->iTime / iElapse) % 2))
            iTime = iElapse - iTime;   // since go backwards

         if ((iTime >= fFrom) && (iTime <= fTo))
            ScanForSubNodes (pControl, pc, pNode);
      }
      else if (!_wcsicmp(psz, L"ID")) {
         pc->pRender->m_dwMajorObjectID = (DWORD) fVal;
         pControl->m_fWantMouse = TRUE;

         // BUGFIX - Add href for specific slot
         psz = pNode->AttribGet (L"href");
         if (psz && pc->plLinkID && pc->plLinkHRef) {
            DWORD i;
            DWORD *pdw = (DWORD*)pc->plLinkID->Get(0);
            for (i = 0; i < pc->plLinkID->Num(); i++, pdw++)
               if (*pdw == pc->pRender->m_dwMajorObjectID)
                  break;
            if (i < pc->plLinkID->Num()) {
               // already exsits, so replace
               pc->plLinkHRef->Set (i, psz, (wcslen(psz)+1)*sizeof(WCHAR));
            }
            else {
               // add
               pc->plLinkID->Add (&pc->pRender->m_dwMajorObjectID);
               pc->plLinkHRef->Add (psz, (wcslen(psz)+1)*sizeof(WCHAR));
            }
         } // if href
      }
      break;


   case L'l':
      if (!_wcsicmp (psz, L"LightAmbient")) {
         pc->pRender->m_fLightBackground = fVal;
      }
      else if (!_wcsicmp (psz, L"LightIntensity")) {
         pc->pRender->m_fLightIntensity = fVal;
      }
      else if (!_wcsicmp (psz, L"LightVector")) {
         pc->pRender->LightVector (pPoint);
      }
      break;



   case L'm':
      if (!_wcsicmp (psz, L"MeshSphere")) {
         AttribToDouble (pNode->AttribGet(L"radius"), &fVal);
         pc->pRender->MeshSphere ((fVal != 0.0) ? fVal : 1.0);
      }
      else if (!_wcsicmp (psz, L"MeshEllipsoid")) {
         pnt   p;
         AttribToDouble (pNode->AttribGet(L"x"), &p[0]);
         AttribToDouble (pNode->AttribGet(L"y"), &p[1]);
         AttribToDouble (pNode->AttribGet(L"z"), &p[2]);
         pc->pRender->MeshEllipsoid (p[0], p[1], p[2]);
      }
      else if (!_wcsicmp (psz, L"MeshFunnel")) {
         pnt   p;
         if (!AttribToDouble (pNode->AttribGet(L"height"), &p[0]))
            p[0] = 1;
         if (!AttribToDouble (pNode->AttribGet(L"base"), &p[1]))
            p[1] = 1;
         if (!AttribToDouble (pNode->AttribGet(L"top"), &p[2]))
            p[2] = 0;

         pc->pRender->MeshFunnel (p[0], p[1], p[2]);
      }
      else if (!_wcsicmp (psz, L"MeshPlane")) {
         pnt   p;
         if (!AttribToDouble (pNode->AttribGet(L"x"), &p[0]))
            p[0] = 1;
         if (!AttribToDouble (pNode->AttribGet(L"y"), &p[1]))
            p[1] = 1;

         pc->pRender->MeshPlane (p[0], p[1]);
      }
      else if (!_wcsicmp (psz, L"MeshTube")) {
         pnt   p;
         if (!AttribToDouble (pNode->AttribGet(L"height"), &p[0]))
            p[0] = 1;
         if (!AttribToDouble (pNode->AttribGet(L"radius"), &p[1]))
            p[1] = 1;

         pc->pRender->MeshTube (p[0], p[1]);
      }
      else if (!_wcsicmp (psz, L"MatrixPush")) {
         pc->pRender->MatrixPush ();
         ScanForSubNodes (pControl, pc, pNode);
         pc->pRender->MatrixPop ();

      }
      else if (!_wcsicmp (psz, L"MatrixSet")) {
         Matrix   m;
         AttribTo3DPoint (pNode->AttribGet(L"column1"), m[0]);
         AttribTo3DPoint (pNode->AttribGet(L"column2"), m[1]);
         AttribTo3DPoint (pNode->AttribGet(L"column3"), m[2]);
         AttribTo3DPoint (pNode->AttribGet(L"column4"), m[3]);
         pc->pRender->MatrixSet (m);
      }
      else if (!_wcsicmp (psz, L"MeshRotation")) {
         WCHAR szTemp[64];
         pnt   ap[MAXPOLYVERT];
         DWORD i;
         for (i = 0; i < MAXPOLYVERT; i++) {
            swprintf (szTemp, L"p%d", i+1);
            if (!AttribTo3DPoint (pNode->AttribGet(szTemp), ap[i]))
               break;
         }
         if (i < 2)
            return;  // error


         BOOL  fBezier;
         AttribToYesNo (pNode->AttribGet(L"bezier"), &fBezier);
         if (fBezier && ((i-1)%3))
            fBezier = FALSE;

         // draw the polygon
         pc->pRender->MeshRotation (i, (double*) ap, fBezier);
      }
      else if (!_wcsicmp (psz, L"MeshBezier")) {
         DWORD x, y, i, dwFlags;
         x = (DWORD) iXCount;
         y = (DWORD) iYCount;
         i = (DWORD) iStart;
         dwFlags = 0;
         AttribToDecimal (pNode->AttribGet(L"flags"), (int*) &dwFlags);

         // make sure doesn't exceed point limit
         if (((i + x * y) > pc->dwDefPoints) || (x*y == 0))
            return;

         // make sure have rigth number of points
         if ((x - ((dwFlags & 0x01) ? 0 : 1)) % 3) return;
         if ((y - ((dwFlags & 0x02) ? 0 : 1)) % 3) return;

         // do it
         pc->pRender->MeshBezier (dwFlags, x, y, pc->pafDefPoints + (i * 4));
      }
      else if (!_wcsicmp (psz, L"MeshFromPoints")) {
         DWORD x, y, i, dwFlags;
         x = (DWORD) iXCount;
         y = (DWORD) iYCount;
         i = (DWORD) iStart;
         dwFlags = 0;
         AttribToDecimal (pNode->AttribGet(L"flags"), (int*) &dwFlags);

         // make sure doesn't exceed point limit
         if (((i + x * y) > pc->dwDefPoints) || (x*y == 0))
            return;

         // do it
         pc->pRender->MeshFromPoints (dwFlags, x, y, pc->pafDefPoints + (i * 4));
      }
      break;

   case L'r':
      if (!_wcsicmp (psz, L"RotateX") || !_wcsicmp (psz, L"RotateY") || !_wcsicmp (psz, L"RotateZ") ) {
         DWORD dwDim;
         dwDim = (DWORD) towlower(psz[6]) - L'x' + 1;

         pc->pRender->Rotate (fVal / 360.0 * 2.0 * PI, dwDim);
      }
      else if (!_wcsicmp (psz, L"RotateXTime") || !_wcsicmp (psz, L"RotateYTime") || !_wcsicmp (psz, L"RotateZTime") ) {
         DWORD dwDim;
         dwDim = (DWORD) towlower(psz[6]) - L'x' + 1;

         double   fFrom, fTo;
         int      iElapse;
         BOOL     fBack;
         if (!AttribToDouble (pNode->AttribGet (L"from"), &fFrom))
            fFrom = 0;
         if (!AttribToDouble (pNode->AttribGet (L"to"), &fTo))
            fTo = 360;
         if (!AttribToDecimal (pNode->AttribGet (L"interval"), &iElapse))
            iElapse = 36;
         if (iElapse <= 1)
            iElapse = 2;
         if (!AttribToYesNo (pNode->AttribGet (L"back"), &fBack))
            fBack = FALSE;

         // real time
         int      iTime;
         double   fDelta;
         fDelta = (fTo - fFrom) / (double) (iElapse + (fBack?0:0));
         iTime = pc->iTime % iElapse;
         if (fBack && ((pc->iTime / iElapse) % 2))
            iTime = iElapse - iTime;   // since go backwards

         // rotate
         pc->pRender->Rotate ((fFrom + fDelta * iTime) / 180.0 * PI, dwDim);
      }
      break;

   
   case L's':
      if (!_wcsicmp (psz, L"ShapeFlatPyramid")) {
         double   fBaseX, fBaseY, fTopX, fTopY, fZ;
         if (!AttribToDouble (pNode->AttribGet(L"basex"), &fBaseX))
            fBaseX = 1;
         if (!AttribToDouble (pNode->AttribGet(L"basey"), &fBaseY))
            fBaseY = 1;
         if (!AttribToDouble (pNode->AttribGet(L"topx"), &fTopX))
            fTopX = .5;
         if (!AttribToDouble (pNode->AttribGet(L"topy"), &fTopY))
            fTopY = .5;
         if (!AttribToDouble (pNode->AttribGet(L"z"), &fZ))
            fZ = 1;

         pc->pRender->ShapeFlatPyramid (fBaseX, fBaseY, fTopX, fTopY, fZ);
      }
      else if (!_wcsicmp (psz, L"ShapeDeepTriangle")) {
         double   fBaseX, fBaseY, fTopX, fTopY, fZ;
         if (!AttribToDouble (pNode->AttribGet(L"basex"), &fBaseX))
            fBaseX = 1;
         if (!AttribToDouble (pNode->AttribGet(L"basey"), &fBaseY))
            fBaseY = 1;
         if (!AttribToDouble (pNode->AttribGet(L"topx"), &fTopX))
            fTopX = .5;
         if (!AttribToDouble (pNode->AttribGet(L"topy"), &fTopY))
            fTopY = .5;
         if (!AttribToDouble (pNode->AttribGet(L"z"), &fZ))
            fZ = 1;

         pc->pRender->ShapeDeepTriangle (fBaseX, fBaseY, fTopX, fTopY, fZ);
      }
      else if (!_wcsicmp (psz, L"ShapeDeepArrow")) {
         double   fX, fY, fZ;
         if (!AttribToDouble (pNode->AttribGet(L"x"), &fX))
            fX = 1;
         if (!AttribToDouble (pNode->AttribGet(L"y"), &fY))
            fY = 1;
         if (!AttribToDouble (pNode->AttribGet(L"z"), &fZ))
            fZ = 1;

         pc->pRender->ShapeDeepArrow (fX, fY, fZ);
      }
      else if (!_wcsicmp (psz, L"ShapeDeepFrame")) {
         double   fX, fY, fZ, fFrameBase, fFrameTop;
         if (!AttribToDouble (pNode->AttribGet(L"x"), &fX))
            fX = 1;
         if (!AttribToDouble (pNode->AttribGet(L"y"), &fY))
            fY = 1;
         if (!AttribToDouble (pNode->AttribGet(L"z"), &fZ))
            fZ = 1;
         if (!AttribToDouble (pNode->AttribGet(L"FrameBase"), &fFrameBase))
            fFrameBase = .2;
         if (!AttribToDouble (pNode->AttribGet(L"FrameTop"), &fFrameTop))
            fFrameTop = .1;

         pc->pRender->ShapeDeepFrame (fX, fY, fZ, fFrameBase, fFrameTop);
      }
      else if (!_wcsicmp (psz, L"ShapePolygon")) {
         WCHAR szTemp[64];
         pnt   ap[MAXPOLYVERT];
         pnt   aco[MAXPOLYVERT];
         DWORD i;
         for (i = 0; i < MAXPOLYVERT; i++) {
            swprintf (szTemp, L"p%d", i+1);
            if (!AttribTo3DPoint (pNode->AttribGet(szTemp), ap[i]))
               break;

            swprintf (szTemp, L"c%d", i+1);
            COLORREF cr;
            if (AttribToColor (pNode->AttribGet(szTemp), &cr)) {
               aco[i][0] = GetRValue(cr);
               aco[i][1] = GetGValue(cr);
               aco[i][2] = GetBValue(cr);
            }
            else {
               CopyPnt (pc->pRender->m_DefColor, aco[i]);
            }
         }
         if (i < 2)
            return;  // error


         // draw the polygon
         pc->pRender->ShapePolygon (i, ap, aco);
      }
      else if (!_wcsicmp (psz, L"ShapeMeshSurface")) {
         pc->pRender->ShapeMeshSurface();
      }
      else if (!_wcsicmp (psz, L"ShapeAxis")) {
         pnt   llb, urf, unitllb, uniturf;

         AttribTo3DPoint (pNode->AttribGet(L"llb"), llb);
         AttribTo3DPoint (pNode->AttribGet(L"urf"), urf);
         AttribTo3DPoint (pNode->AttribGet(L"unitllb"), unitllb);
         AttribTo3DPoint (pNode->AttribGet(L"uniturf"), uniturf);

         DWORD dwFlags;
         dwFlags = 0;
         AttribToDecimal (pNode->AttribGet(L"flags"), (int*) &dwFlags);

         PSTR  papszAxis[3];
         papszAxis[0] = pc->aszAxis[0];
         papszAxis[1] = pc->aszAxis[1];
         papszAxis[2] = pc->aszAxis[2];

         pc->pRender->ShapeAxis (llb, urf, unitllb, uniturf, papszAxis, dwFlags);
      }
      else if (!_wcsicmp (psz, L"Scale") ) {
         pnt   p;
         if (!AttribToDouble (pNode->AttribGet(L"x"), &p[0]))
            p[0] = fVal ? fVal : 1.0;
         if (!AttribToDouble (pNode->AttribGet(L"y"), &p[1]))
            p[1] = p[0];
         if (!AttribToDouble (pNode->AttribGet(L"z"), &p[2]))
            p[2] = p[1];

         pc->pRender->Scale (p[0], p[1], p[2]);
      }
      else if (!_wcsicmp (psz, L"ScaleTime")) {
         pnt      pFrom, pTo;
         int      iElapse;
         BOOL     fBack;
         if (!AttribTo3DPoint (pNode->AttribGet (L"from"), pFrom))
            pFrom[0] = pFrom[1] = pFrom[2] = 1;
         if (!AttribTo3DPoint (pNode->AttribGet (L"to"), pTo))
            pTo[0] = pTo[1] = pTo[2] = 1;
         if (!AttribToDecimal (pNode->AttribGet (L"interval"), &iElapse))
            iElapse = 36;
         if (iElapse <= 1)
            iElapse = 2;
         if (!AttribToYesNo (pNode->AttribGet (L"back"), &fBack))
            fBack = FALSE;

         // real time
         int      iTime;
         double   fAlpha;
         fAlpha = 1.0 / (double) (iElapse + (fBack?0:0));
         iTime = pc->iTime % iElapse;
         if (fBack && ((pc->iTime / iElapse) % 2))
            iTime = iElapse - iTime;   // since go backwards
         fAlpha *= iTime;

         // rotate
         pnt   n;
         n[0] = pFrom[0] + (pTo[0] - pFrom[0]) * fAlpha;
         n[1] = pFrom[1] + (pTo[1] - pFrom[1]) * fAlpha;
         n[2] = pFrom[2] + (pTo[2] - pFrom[2]) * fAlpha;
         pc->pRender->Scale (n[0], n[1], n[2]);
      }
      else if (!_wcsicmp (psz, L"ShapeLine")) {
         WCHAR szTemp[64];
         pnt   ap[MAXPOLYVERT];
         pnt   aco[MAXPOLYVERT];
         DWORD i;
         for (i = 0; i < MAXPOLYVERT; i++) {
            swprintf (szTemp, L"p%d", i+1);
            if (!AttribTo3DPoint (pNode->AttribGet(szTemp), ap[i]))
               break;

            swprintf (szTemp, L"c%d", i+1);
            COLORREF cr;
            if (AttribToColor (pNode->AttribGet(szTemp), &cr)) {
               aco[i][0] = GetRValue(cr);
               aco[i][1] = GetGValue(cr);
               aco[i][2] = GetBValue(cr);
            }
            else {
               CopyPnt (pc->pRender->m_DefColor, aco[i]);
            }
         }
         if (i < 2)
            return;  // error

         BOOL  fArrow;
         AttribToYesNo (pNode->AttribGet (L"arrow"), &fArrow);

         // draw the polygon
         pc->pRender->ShapeLine (i, (double*) ap, (double*) aco, fArrow);
      }
      else if (!_wcsicmp (psz, L"ShapeArrow")) {
         WCHAR szTemp[64];
         pnt   ap[MAXPOLYVERT];
         DWORD i;
         for (i = 0; i < MAXPOLYVERT; i++) {
            swprintf (szTemp, L"p%d", i+1);
            if (!AttribTo3DPoint (pNode->AttribGet(szTemp), ap[i]))
               break;
         }
         if (i < 2)
            return;  // error

         double   fWidth, fWidth2;
         BOOL fTip;
         if (!AttribToDouble (pNode->AttribGet(L"width"), &fWidth))
            fWidth = 0.1;
         if (!AttribToDouble (pNode->AttribGet(L"width2"), &fWidth2))
            fWidth2 = fWidth;
         if (!AttribToYesNo (pNode->AttribGet(L"tip"), &fTip))
            fTip = TRUE;
         // draw the polygon
         pc->pRender->ShapeArrow (i, (double*) ap, fWidth, fWidth2, fTip);
      }
      else if (!_wcsicmp (psz, L"ShapeBox") ) {
         pnt   p;
         if (!AttribToDouble (pNode->AttribGet(L"x"), &p[0]))
            p[0] = fVal ? fVal : 1.0;
         if (!AttribToDouble (pNode->AttribGet(L"y"), &p[1]))
            p[1] = p[0];
         if (!AttribToDouble (pNode->AttribGet(L"z"), &p[2]))
            p[2] = p[1];

         pc->pRender->ShapeBox (p[0], p[1], p[2]);
      }
      else if (!_wcsicmp (psz, L"ShapeDot")) {
         // do it
         pc->pRender->ShapeDot (pPoint);
      }
      else if (!_wcsicmp (psz, L"ShapeMeshVectors")) {
         DWORD x, y, i;
         x = (DWORD) iXCount;
         y = (DWORD) iYCount;
         i = (DWORD) iStart;

         // make sure doesn't exceed point limit
         if (((i + x * y) > pc->dwDefPoints) || (x*y == 0)) return;

         // do it
         pc->pRender->ShapeMeshVectors (x, y, pc->pafDefPoints + (i * 4));
      }
      else if (!_wcsicmp (psz, L"ShapeMeshVectorsFromBitmap")) {
         DWORD dwBmpResource = 0, dwJpgResource;
         AttribToDecimal (psz = (WCHAR*) pNode->AttribGet (L"bmpresource"), (int*) &dwBmpResource);
         AttribToDecimal (psz = (WCHAR*) pNode->AttribGet (L"jpgresource"), (int*) &dwJpgResource);
         if (!pszFile && !dwBmpResource && !dwJpgResource)
            return;

         HBITMAP hBit;
         PCBitmapCache pCache;
         pCache = EscBitmapCache();
         DWORD dwID;
         // BUGFIX - Allow bitmap to be cahced
         if (dwBmpResource)
            dwID = pCache->CacheResourceBMP (dwBmpResource, pControl->m_hInstance, &hBit);
         else if (dwJpgResource)
            dwID = pCache->CacheResourceJPG (dwJpgResource, pControl->m_hInstance, &hBit);
         else
            dwID = pCache->CacheFile (pszFile, &hBit);
         // BUGBUG - 2.0 - allow to use resource
         if (!dwID || !hBit)
            return;

         pc->pRender->ShapeMeshVectorsFromBitmap (hBit);

         pCache->CacheRelease(dwID);
      }
      else if (!_wcsicmp (psz, L"ShapeTeapot")) {
         pc->pRender->ShapeTeapot ();
      }
      break;

   case L't':
      if (!_wcsicmp (psz, L"Text")) {
         if (!pszName)
            return;

         pnt   pLeft, pRight;
         AttribTo3DPoint (pNode->AttribGet(L"left"), pLeft);
         AttribTo3DPoint (pNode->AttribGet(L"right"), pRight);

         WCHAR *pszVal;
         int   iHorz, iVert;
         pszVal = pNode->AttribGet(L"align");
         if (pszVal && !_wcsicmp(pszVal, L"left"))
            iHorz = -1;
         else if (pszVal && !_wcsicmp(pszVal, L"right"))
            iHorz = 1;
         else
            iHorz = 0;
         pszVal = pNode->AttribGet(L"valign");
         if (pszVal && !_wcsicmp(pszVal, L"top"))
            iVert = -1;
         else if (pszVal && !_wcsicmp(pszVal, L"bottom"))
            iVert = 1;
         else
            iVert = 0;

         char  szTemp[512];
         WideCharToMultiByte (CP_ACP, 0, pszName, -1, szTemp, sizeof(szTemp), 0, 0);

         pc->pRender->Text (szTemp, pLeft, pRight, iHorz, iVert);
      }
      else if (!_wcsicmp (psz, L"TextSizePixels") || !_wcsicmp (psz, L"TextSize3D")) {
         // create the logfont
         LOGFONT  lf;
         memset (&lf, 0, sizeof(lf));
         lf.lfHeight = -MulDiv(pControl->m_fi.iPointSize, EscGetDeviceCaps(pc->hDC, LOGPIXELSY), 72); 
         lf.lfCharSet = DEFAULT_CHARSET;
         lf.lfPitchAndFamily = FF_SWISS | VARIABLE_PITCH;
         lf.lfWeight = FW_NORMAL;   // BUGFIX - Adjust the weight of all fonts to normal
         WideCharToMultiByte (CP_ACP, 0, pControl->m_fi.szFont, -1, lf.lfFaceName, sizeof(lf.lfFaceName), 0, 0);
         if (pControl->m_fi.dwFlags & FCFLAG_BOLD)
            lf.lfWeight = FW_BOLD;
         if (pControl->m_fi.dwFlags & FCFLAG_ITALIC)
            lf.lfItalic = TRUE;
         if (pControl->m_fi.dwFlags & FCFLAG_UNDERLINE)
            lf.lfUnderline = TRUE;
         if (pControl->m_fi.dwFlags & FCFLAG_STRIKEOUT)
            lf.lfStrikeOut = TRUE;

         pc->pRender->TextFontSet (&lf, fVal, _wcsicmp (psz, L"TextSize3D") ? TRUE : FALSE);
      }
      else if (!_wcsicmp (psz, L"Translate") ) {
         pc->pRender->Translate (pPoint[0], pPoint[1], pPoint[2]);
      }
      else if (!_wcsicmp (psz, L"TranslateTime")) {
         pnt      pFrom, pTo;
         int      iElapse;
         BOOL     fBack;
         AttribTo3DPoint (pNode->AttribGet (L"from"), pFrom);
         AttribTo3DPoint (pNode->AttribGet (L"to"), pTo);
         if (!AttribToDecimal (pNode->AttribGet (L"interval"), &iElapse))
            iElapse = 36;
         if (iElapse <= 1)
            iElapse = 2;
         if (!AttribToYesNo (pNode->AttribGet (L"back"), &fBack))
            fBack = FALSE;

         // real time
         int      iTime;
         double   fAlpha;
         fAlpha = 1.0 / (double) (iElapse + (fBack?0:0));
         iTime = pc->iTime % iElapse;
         if (fBack && ((pc->iTime / iElapse) % 2))
            iTime = iElapse - iTime;   // since go backwards
         fAlpha *= iTime;

         // rotate
         pnt   n;
         n[0] = pFrom[0] + (pTo[0] - pFrom[0]) * fAlpha;
         n[1] = pFrom[1] + (pTo[1] - pFrom[1]) * fAlpha;
         n[2] = pFrom[2] + (pTo[2] - pFrom[2]) * fAlpha;
         pc->pRender->Translate (n[0], n[1], n[2]);
      }
      break;

   case L'v':
      if (!_wcsicmp (psz, L"VectorArrowsOn")) {
         pc->pRender->m_fVectorArrows = TRUE;
      }
      if (!_wcsicmp (psz, L"VectorArrowsOff")) {
         pc->pRender->m_fVectorArrows = FALSE;
      }
      else if (!_wcsicmp (psz, L"VectorInterpOn")) {
         pc->pRender->m_fDontInterpVectors = FALSE;
      }
      else if (!_wcsicmp (psz, L"VectorInterpOff")) {
         pc->pRender->m_fDontInterpVectors = TRUE;
      }
      else if (!_wcsicmp (psz, L"VectorScale")) {
         pc->pRender->m_fVectorScale = fVal;
      }
      break;

   case L'w':
      if (!_wcsicmp (psz, L"WireFrameOn")) {
         pc->pRender->m_fWireFrame = TRUE;
      }
      if (!_wcsicmp (psz, L"WireFrameOff")) {
         pc->pRender->m_fWireFrame = FALSE;
      }
      break;

   }


}



/***********************************************************************
ScanForSubNodes - Scan the contents of the node for sub-nodes and run those

inputs
   PCEscControl      pControl - control
   THREED            *pc - control info
   PCMMLNode         pNode - node to look for
returns
   none
*/
void ScanForSubNodes (PCEscControl pControl, THREED *pc, PCMMLNode pNode)
{
   DWORD i;
   for (i = 0; i < pNode->ContentNum(); i++) {
      WCHAR    *psz;
      PCMMLNode   pSubNode;
      pNode->ContentEnum (i, &psz, &pSubNode);

      // ignore strings

      if (pSubNode)
         ParseInstruction (pControl, pc, pSubNode);
   }
}

/********************************************************************************
Rotated - This should be called if any of the x,y,z rotation scroll bars
   are changed

inputs
   int    x,y,z - new rotation values. 0 = no change
   BOOL     fForce - if TRUE, force to recaululate
*/
void Rotated (PCEscControl pControl, PTHREED pc, int x, int y, int z, BOOL fForce = FALSE)
{
   DWORD dwLast;
   dwLast = pc->dwRotating;

   // figure which has changed
   DWORD dwChanged;
   int dwVal;
   if (x != pc->iRot[0]) {
      dwChanged = 0;
      dwVal = x;
   }
   else if (y != pc->iRot[1]) {
      dwChanged = 1;
      dwVal = y;
   }
   else if (z != pc->iRot[2]) {
      dwChanged = 2;
      dwVal = z;
   }
   else {
      dwChanged = (pc->dwRotating + 1) % 3;
      if (!fForce)
         return;     // nothing has changed
   }

   // has the dimension changed?
   if (dwChanged != pc->dwRotating) {
      // pre-rotate by old m_adwCurRot[m_dwRotating] value
      Matrix   r, r2;
      DWORD x, y;
      for (x = 0; x < 4; x++) for (y = 0; y < 4; y++)
         r[x][y] = (x == y) ? 1.0 : 0.0;
      double   fAngle;
      fAngle = (double) pc->iRot[pc->dwRotating] / 180.0 * PI;
      RotationMatrix (r, (pc->dwRotating == 0) ? fAngle : 0,
         (pc->dwRotating == 1) ? fAngle : 0, (pc->dwRotating == 2) ? fAngle : 0);
      MultiplyMatrices (r, pc->mRot, r2);

      // should make sure matrix is still 1.0 and square
      MakeMatrixSquare (r2);

      // store away
      memcpy (pc->mRot, r2, sizeof(pc->mRot));

      // update the sliders. Old one to 0. New one to new value
      if (pc->pScroll[dwLast])
         pc->pScroll[dwLast]->AttribSet (L"pos", L"0");
      // don't need to update the one just changed since it's obviously been moved
   }

   // set everything to 180 except the new changed
   pc->iRot[0] = pc->iRot[1] = pc->iRot[2] = 0;

   if (fForce)
      return;  // don't actually want to change anything

   pc->dwRotating = dwChanged;
   pc->iRot[dwChanged] = dwVal;

}



/***********************************************************************
Control callback
*/
BOOL ControlThreeD (PCEscControl pControl, DWORD dwMessage, PVOID pParam)
{
   THREED  *pc = (THREED*) pControl->m_mem.p;
   switch (dwMessage) {
   case ESCM_CONSTRUCTOR:
      {
         pControl->m_mem.Required (sizeof(THREED));
         pc = (THREED*) pControl->m_mem.p;
         memset (pc, 0, sizeof(THREED));
         pc->pszHRef = NULL;
         pc->pRender = new CRender;
         pc->fMinDistance = 5;
         pc->fMaxDistance = 50;
         pc->iDistance = 50;
         pc->cBorder = 0;
         pc->iBorderSize = 2;
         pc->plLinkHRef = new CListVariable;
         pc->plLinkID = new CListFixed;
         if (pc->plLinkID)
            pc->plLinkID->Init (sizeof(DWORD));
         DWORD x, y;
         for (x = 0; x < 4; x++) for (y = 0; y < 4; y++)
            pc->mRot[x][y] = (x == y) ? 1.0 : 0.0;

         pControl->AttribListAddColor (L"bordercolor", &pc->cBorder, FALSE, TRUE);
         pControl->AttribListAddDecimal (L"border", &pc->iBorderSize, FALSE, TRUE);
         pControl->AttribListAddString (L"href", &pc->pszHRef);
         pControl->AttribListAddString (L"ScrollRotX", &pc->pszScrollRot[0], &pc->fRecalc, TRUE);
         pControl->AttribListAddString (L"ScrollRotY", &pc->pszScrollRot[1], &pc->fRecalc, TRUE);
         pControl->AttribListAddString (L"ScrollRotZ", &pc->pszScrollRot[2], &pc->fRecalc, TRUE);
         pControl->AttribListAddString (L"ScrollDistance", &pc->pszScrollDistance, &pc->fRecalc, TRUE);
         pControl->AttribListAddDouble (L"mindistance", &pc->fMinDistance, NULL, TRUE);
         pControl->AttribListAddDouble (L"mindistance", &pc->fMaxDistance, NULL, TRUE);
         pControl->AttribListAddDecimal (L"timerinterval", &pc->iTimerInterval, NULL, TRUE);
         pControl->AttribListAddDecimal (L"time", &pc->iTime, NULL, TRUE);
      }
      return TRUE;


   case ESCM_MOUSEMOVE:
      {
         // override if have specific link numbers
         if (!pc->plLinkID->Num() || !pc->pRender)
            break;

         ESCMMOUSEMOVE *p = (ESCMMOUSEMOVE*) pParam;

         DWORD dwMajor, dwMinor;
         dwMajor = 0;
         pc->pRender->ObjectGet (p->pPosn.x - pControl->m_rPosn.left,
            p->pPosn.y - pControl->m_rPosn.top, &dwMajor, &dwMinor);

         // if hit
         BOOL fFound = FALSE;
         if (dwMajor && (dwMajor != NULLOBJECT)) {
            // see if there is a match to the link
            DWORD i;
            DWORD *padw = (DWORD*)pc->plLinkID->Get(0);
            for (i = 0; i < pc->plLinkID->Num(); i++,padw++)
               if (*padw == dwMajor) {
                  fFound = TRUE;
                  break;
               }
         }

         // if hit
         pControl->m_pParentPage->SetCursor (fFound ? IDC_HANDCURSOR : IDC_NOCURSOR);
      }
      return TRUE;

   case ESCM_DESTRUCTOR:
      // BUGFIX - Wsan't freeing colors and points
      if (pc->pafDefColors)
         ESCFREE (pc->pafDefColors);
      if (pc->pafDefPoints)
         ESCFREE (pc->pafDefPoints);

      if (pc->plLinkHRef)
         delete pc->plLinkHRef;
      if (pc->plLinkID)
         delete pc->plLinkID;

      if (pc->pRender)
         delete pc->pRender;
      if (pc->pReplaceNode)
         delete pc->pReplaceNode;
      return TRUE;

   case ESCM_THREEDCHANGE:
      {
         PESCMTHREEDCHANGE p = (PESCMTHREEDCHANGE) pParam;

         // delete the old one
         if (pc->pReplaceNode)
            delete pc->pReplaceNode;
         pc->pReplaceNode = NULL;

         if (p->pNode) {
            pc->pReplaceNode = p->pNode->Clone();
         }
         else if (p->pszMML) {
            CEscError   err;
            pc->pReplaceNode = ParseMML (p->pszMML, pControl->m_hInstance, NULL, NULL, &err);
         }

         // redraw
         pControl->Invalidate();


      }
      return TRUE;

   case ESCM_INITCONTROL:
      {
         // if this has a href then want mouse
         pControl->m_fWantMouse = pc->pszHRef ? TRUE : FALSE;
         if (pControl->m_fWantMouse)
            pControl->m_dwWantFocus = 1;

         // secify that accept space or enter
         if (pControl->m_dwWantFocus) {
            ESCACCELERATOR a;
            memset (&a, 0, sizeof(a));
            a.c = L' ';
            a.dwMessage = ESCM_SWITCHACCEL;
            pControl->m_listAccelFocus.Add (&a);
            a.c = L'\n';
            pControl->m_listAccelFocus.Add (&a);
         }

         if (pc->iTimerInterval)
            pControl->TimerSet ((DWORD) pc->iTimerInterval);
      }
      return TRUE;

   case ESCM_CONTROLTIMER:
      // timer goes off so redraw
      pc->iTime++;
      pControl->Invalidate();
      return TRUE;

   case ESCM_LBUTTONDOWN:
      {
         ESCMLBUTTONDOWN *p = (ESCMLBUTTONDOWN*) pParam;

         // BUGFIX - Proeblem when click on 3d object
         // must release capture or bad things happen
         pControl->m_pParentPage->MouseCaptureRelease(pControl);

         if (pc->pszHRef) {
            pControl->m_pParentPage->m_pWindow->Beep (ESCBEEP_LINKCLICK);

            pControl->m_pParentPage->Link (pc->pszHRef);
            return TRUE;
         }

         // if click on render object
         if (pc->pRender) {
            DWORD dwMajor, dwMinor;
            dwMajor = 0;
            pc->pRender->ObjectGet (p->pPosn.x - pControl->m_rPosn.left,
               p->pPosn.y - pControl->m_rPosn.top, &dwMajor, &dwMinor);

            // if hit
            if (dwMajor && (dwMajor != NULLOBJECT)) {
               // see if there is a match to the link
               DWORD i;
               DWORD *padw = (DWORD*)pc->plLinkID->Get(0);
               for (i = 0; i < pc->plLinkID->Num(); i++,padw++)
                  if (*padw == dwMajor)
                     break;
               if (i < pc->plLinkID->Num()) {
                  pControl->m_pParentPage->m_pWindow->Beep (ESCBEEP_LINKCLICK);

                  pControl->m_pParentPage->Link ((PWSTR)pc->plLinkHRef->Get(i));
                  return TRUE;
               }

               // if there are any links at all then beep
               if (pc->plLinkID->Num()) {
                  pControl->m_pParentPage->m_pWindow->Beep (ESCBEEP_DONTCLICK);
                  return TRUE;
               }

               ESCNTHREEDCLICK c;
               c.pControl = pControl;
               c.dwMajor = dwMajor;
               c.dwMinor = dwMinor;
               pControl->m_pParentPage->m_pWindow->Beep (ESCBEEP_LINKCLICK);
               pControl->MessageToParent (ESCN_THREEDCLICK, &c);
               return TRUE;
            }

            // else beep at user
            pControl->m_pParentPage->m_pWindow->Beep (ESCBEEP_DONTCLICK);
         }
      }
      return TRUE;

   case ESCM_PAINT:
      {
         PESCMPAINT p = (PESCMPAINT) pParam;
         if (!pc->pRender)
            return FALSE;

         pc->pOffset.x = p->rControlHDC.left - p->rControlPage.left;
         pc->pOffset.y = p->rControlHDC.top - p->rControlPage.top;

         // if we have attributes that point to the scroll bar but dont
         // actually have the control then find
         DWORD i;
         for (i = 0; i < 3; i++)
            if (pc->pszScrollRot[i] && !pc->pScroll[i]) {
               pc->pScroll[i] = pControl->m_pParentPage->ControlFind (pc->pszScrollRot[i]);
               if (pc->pScroll[i]) {
                  WCHAR szTemp[64];
                  pc->pScroll[i]->AttribSet (L"min", L"-180");
                  pc->pScroll[i]->AttribSet (L"max", L"180");
                  pc->pScroll[i]->AttribSet (L"page", L"10");
                  swprintf (szTemp, L"%d", pc->iRot[i]);
                  pc->pScroll[i]->AttribSet (L"pos", szTemp);
                  pc->pScroll[i]->m_pParentControl = pControl;
               }
            }
         if (pc->pszScrollDistance && !pc->pScrollDistance) {
            pc->pScrollDistance = pControl->m_pParentPage->ControlFind (pc->pszScrollDistance);
            if (pc->pScrollDistance) {
               WCHAR szTemp[64];
               pc->pScrollDistance->AttribSet (L"min", L"0");
               pc->pScrollDistance->AttribSet (L"max", L"100");
               pc->pScrollDistance->AttribSet (L"page", L"5");
               swprintf (szTemp, L"%d", pc->iDistance);
               pc->pScrollDistance->AttribSet (L"pos", szTemp);
               pc->pScrollDistance->m_pParentControl = pControl;
            }
         }

         // init
         pc->hDC = p->hDC;
         pc->pRender->Init (p->hDC, &p->rControlHDC);
         pc->pRender->m_fPixelsPerFacet /= 2.0;
         pc->pRender->m_dwMaxFacets /= 2;
         pc->pRender->m_dwMajorObjectID = NULLOBJECT;

         // convert distance number from 0 to 100, to a real distance
         double   fVal;
         double   fPerUnit;
         if (pc->fMaxDistance <= pc->fMinDistance)
            pc->fMaxDistance = fabs(pc->fMinDistance) + 50;
         if (pc->fMinDistance <= 0.0)
            pc->fMinDistance = pc->fMaxDistance / 10;
         fPerUnit = pow (pc->fMaxDistance / pc->fMinDistance, 1.0/100.0);
         fVal = pc->fMinDistance * pow(fPerUnit, pc->iDistance);
         pc->pRender->Translate (0, 0, -fVal);

         // rotate by the current rotation component
         pc->pRender->Rotate (pc->iRot[pc->dwRotating] / 180.0 * PI, pc->dwRotating+1);

         // multiply by our rotation matrix
         pc->pRender->MatrixMultiply (pc->mRot);

         // do it
         ScanForSubNodes (pControl, pc, pc->pReplaceNode ? pc->pReplaceNode : pControl->m_pNode);

         // draw
         pc->pRender->Commit (p->hDC, &p->rControlHDC);


         if (pc->iBorderSize) {
            HBRUSH hbr;
            int   iBorder = (int) pc->iBorderSize;
            hbr = CreateSolidBrush (pc->cBorder);

            // left
            RECT r;
            r = p->rControlHDC;
            r.right = r.left + iBorder;
            FillRect (p->hDC, &r, hbr);

            // right
            r = p->rControlHDC;
            r.left = r.right - iBorder;
            FillRect (p->hDC, &r, hbr);

            // top
            r = p->rControlHDC;
            r.bottom = r.top + iBorder;
            FillRect (p->hDC, &r, hbr);

            // bottom
            r = p->rControlHDC;
            r.top = r.bottom - iBorder;
            FillRect (p->hDC, &r, hbr);

            DeleteObject (hbr);
         }
      }
      return TRUE;


   case ESCN_SCROLL:
   case ESCN_SCROLLING:
      {
         ESCNSCROLL  *p = (ESCNSCROLL*) pParam;
         DWORD i;
         for (i = 0; i < 3; i++)
            if (p->pControl == pc->pScroll[i]) {
               int   r;
               r = p->iPos;
               if (i == 2)
                  r = -r;  // flip z

               // found it. it's 3D
               Rotated (pControl, pc,
                  (i == 0) ? r : pc->iRot[0],
                  (i == 1) ? r : pc->iRot[1],
                  (i == 2) ? r : pc->iRot[2]);

               // pc->iRot[i] = p->iPos;
               pControl->Invalidate ();
               return TRUE;
            }
         if (p->pControl == pc->pScrollDistance) {
            // the distance has changed
            pc->iDistance = p->iPos;
            pControl->Invalidate();
            return TRUE;
         }
      }
      return TRUE;
   }

   return FALSE;
}

