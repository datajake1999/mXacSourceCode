/************************************************************************
CObjectPainting.cpp - Draws a Painting.

begun 5/9/02 by Mike Rozak
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

static SPLINEMOVEP   gaPaintingMove[] = {
   L"Base", 0, 0
};

/**********************************************************************************
CObjectPainting::Constructor and destructor */
CObjectPainting::CObjectPainting (PVOID pParams, POSINFO pInfo)
{
   m_dwRenderShow = RENDERSHOW_FURNITURE;
   m_OSINFO = *pInfo;

   m_dwStretch = 2;  // scale to fit
   m_dwShape = 0;
   m_pSize.p[0] = .5;
   m_pSize.p[1] = 1;
   m_pSize.p[2] = .05;
   m_pSize.p[3] = .05;
   m_dwFrameProfile = NS_RECTANGLE;
   m_fCanvasThick = .015;
   m_pFrameSize.Zero();
   m_pFrameSize.p[0] = .05;
   m_pFrameSize.p[1] = .025;
   m_fTable = FALSE;

   // Fill noodle based on paramters
   CreateNoodle ();

   // set embedding based on thable
   m_fCanBeEmbedded = !m_fTable;

   // colors
   ObjectSurfaceAdd (1, RGB(0xff,0xff,0xe0), MATERIAL_PAINTGLOSS, L"Artwork frame",
                  &GTEXTURECODE_WoodTrim, &GTEXTURESUB_WoodTrim);
   ObjectSurfaceAdd (2, RGB(0,0,0xff), MATERIAL_PAINTGLOSS);   // paitning
   ObjectSurfaceAdd (3, RGB(0xff,0xff,0xff), MATERIAL_PAINTGLOSS);   // mat
   ObjectSurfaceAdd (4, RGB(0x20,0x20,0x20), MATERIAL_PAINTMATTE);   // back
}


CObjectPainting::~CObjectPainting (void)
{
   // do nothing for now
}


/**********************************************************************************
CObjectPainting::Delete - Called to delete this object
*/
void CObjectPainting::Delete (void)
{
   delete this;
}

/**********************************************************************************
CObjectPainting::Render - Draws a test box.

inputs
   POBJECTRENDER     pr - Render information
*/
void CObjectPainting::Render (POBJECTRENDER pr, DWORD dwSubObject)
{
   // create the surface render object and draw
   // CRenderSurface rs;
   m_Renderrs.ClearAll();

   CMatrix mObject;
   m_Renderrs.Init (pr->pRS);
   ObjectMatrixGet (&mObject);
   m_Renderrs.Multiply (&mObject);

   // Take into account if on table
   if (m_fTable) {
      m_Renderrs.Push();

      m_Renderrs.Rotate (-PI/8, 1);
      fp fTrans;
      fTrans = m_pSize.p[1]/2;
      if (m_dwFrameProfile)
         fTrans += m_pFrameSize.p[0];
      m_Renderrs.Translate (0, 0, fTrans);
   }

   // draw the frame
   if (m_dwFrameProfile) {
      m_Renderrs.SetDefMaterial (m_OSINFO.dwRenderShard, ObjectSurfaceFind (1), m_pWorld);
      m_nFrame.Render (pr, &m_Renderrs);
   }

   // draw the painting
   fp fNewWidth, fNewHeight;
   fNewWidth = m_pSize.p[0] - 2 * m_pSize.p[2];
   fNewHeight = m_pSize.p[1] - 2 * m_pSize.p[3];
   m_Renderrs.SetDefMaterial (m_OSINFO.dwRenderShard, ObjectSurfaceFind (2), m_pWorld);
   if ((m_dwStretch == 2) && (fNewWidth > CLOSE) && (fNewHeight > CLOSE)) {
      PRENDERSURFACE ps;
      ps = m_Renderrs.GetDefSurface ();
      PCTextureMapSocket pMap;
      pMap = NULL;
      if (ps->fUseTextureMap)
         pMap = TextureCacheGet (m_OSINFO.dwRenderShard, ps, NULL, NULL);

      // if there is a texture map then get it's average color
      if (pMap) {
         fp fDefWidth, fDefHeight;
         pMap->DefScaleGet (&fDefWidth, &fDefHeight);
         TextureCacheRelease (m_OSINFO.dwRenderShard, pMap);

         fDefWidth = max(CLOSE, fDefWidth);
         fDefHeight = max(CLOSE, fDefHeight);
         
         // aspect ration
         fp fAspectText, fAspectPaint;
         fAspectText = fDefWidth / fDefHeight;
         fAspectPaint = fNewWidth / fNewHeight;

         if (fAspectText > fAspectPaint + CLOSE)
            fNewHeight /= (fAspectText / fAspectPaint);
         else if (fAspectPaint > fAspectText + CLOSE)
            fNewWidth /= (fAspectPaint / fAspectText);
      }

   }
   if ((fNewWidth > CLOSE) && (fNewHeight > CLOSE)) {
      RenderCanvas (pr, &m_Renderrs, -m_fCanvasThick - 0.001, FALSE, m_dwStretch != 0, fNewWidth, fNewHeight);
   }

   // draw the mat
   m_Renderrs.SetDefMaterial (m_OSINFO.dwRenderShard, ObjectSurfaceFind (3), m_pWorld);
   RenderCanvas (pr, &m_Renderrs, -m_fCanvasThick, !m_dwFrameProfile, FALSE, m_pSize.p[0], m_pSize.p[1]);

   // draw the back face
   m_Renderrs.SetDefMaterial (m_OSINFO.dwRenderShard, ObjectSurfaceFind (4), m_pWorld);
   m_Renderrs.Push();
   m_Renderrs.Rotate (PI,3);
   RenderCanvas (pr, &m_Renderrs, 0, FALSE, FALSE, m_pSize.p[0], m_pSize.p[1]);
   m_Renderrs.Pop();

   if (m_fTable) {
      m_Renderrs.Pop();

      CPoint pUL, pUR, pLR, pLL;
      pUL.Zero();
      pUL.p[1] = m_pSize.p[1] / 2 * sin(PI/8);
      pUL.p[2] = m_pSize.p[1] / 2 * cos(PI/8);
      pUR.Copy (&pUL);
      pUR.p[1] += m_pSize.p[1] / 10;
      pLL.Zero();
      pLR.Zero();
      pLR.p[1] = pUR.p[1] + m_pSize.p[1] / 2 * sin(PI/8);
      pLR.p[2] = 0;
      m_Renderrs.ShapeQuad (&pUL, &pUR, &pLR, &pLL, FALSE);
   }

   m_Renderrs.Commit();
}





/**********************************************************************************
CObjectPainting::QueryBoundingBox - Standard API
*/
void CObjectPainting::QueryBoundingBox (PCPoint pCorner1, PCPoint pCorner2, DWORD dwSubObject)
{
   pCorner2->Zero();
   pCorner2->p[0] = m_pSize.p[0] / 2;
   pCorner2->p[2] = m_pSize.p[1] / 2;
   pCorner2->p[0] += m_pFrameSize.p[0];
   pCorner2->p[2] += m_pFrameSize.p[0];
   pCorner2->p[1] += m_pFrameSize.p[1];
   pCorner1->Copy (pCorner2);
   pCorner1->Scale (-1);

   // Take into account if on table
   if (m_fTable) {
      CMatrix m, mTemp;

      m.RotationX (-PI/8);
      fp fTrans;
      fTrans = m_pSize.p[1]/2;
      if (m_dwFrameProfile)
         fTrans += m_pFrameSize.p[0];
      mTemp.Translation (0, 0, fTrans);
      m.MultiplyLeft (&mTemp);

      BoundingBoxApplyMatrix (pCorner1, pCorner2, &m);

      CPoint pUL, pUR, pLR, pLL;
      pUL.Zero();
      pUL.p[1] = m_pSize.p[1] / 2 * sin(PI/8);
      pUL.p[2] = m_pSize.p[1] / 2 * cos(PI/8);
      pUR.Copy (&pUL);
      pUR.p[1] += m_pSize.p[1] / 10;
      pLL.Zero();
      pLR.Zero();
      pLR.p[1] = pUR.p[1] + m_pSize.p[1] / 2 * sin(PI/8);
      pLR.p[2] = 0;

      pCorner1->Min (&pUL);
      pCorner1->Min (&pUR);
      pCorner1->Min (&pLL);
      pCorner1->Min (&pLR);
      pCorner2->Max (&pUL);
      pCorner2->Max (&pUR);
      pCorner2->Max (&pLL);
      pCorner2->Max (&pLR);
   }


#ifdef _DEBUG
   // test, make sure bounding box not too small
   CPoint p1,p2;
   DWORD i;
   CObjectTemplate::QueryBoundingBox (&p1, &p2, dwSubObject);
   for (i = 0; i < 3; i++)
      if ((p1.p[i] < pCorner1->p[i] - CLOSE*10) || (p2.p[i] > pCorner2->p[i] + CLOSE*10))
         break;
   if (i < 3)
      OutputDebugString ("\r\nCObjectPainting::QueryBoundingBox too small.");
#endif
}


/**********************************************************************************
CObjectPainting::Clone - Clones this

inputs
   none
returns
   PCObjectSocket - Clone
*/
PCObjectSocket CObjectPainting::Clone (void)
{
   PCObjectPainting pNew;

   pNew = new CObjectPainting(NULL, &m_OSINFO);

   // clone template info
   CloneTemplate(pNew);

   pNew->m_dwStretch = m_dwStretch;
   pNew->m_dwShape = m_dwShape;
   pNew->m_pSize.Copy (&m_pSize);
   pNew->m_dwFrameProfile = m_dwFrameProfile;
   pNew->m_pFrameSize.Copy (&m_pFrameSize);
   pNew->m_fCanvasThick = m_fCanvasThick;
   pNew->m_fTable = m_fTable;

   m_nFrame.CloneTo (&pNew->m_nFrame);

   return pNew;
}

static PWSTR gpszShape = L"Shape";
static PWSTR gpszSize = L"Size";
static PWSTR gpszFrameProfile = L"FrameProfile";
static PWSTR gpszFrameSize = L"FrameSize";
static PWSTR gpszCanvasThick = L"CanvasThick";
static PWSTR gpszTable = L"Table";
static PWSTR gpszStretch = L"Stretch";

PCMMLNode2 CObjectPainting::MMLTo (void)
{
   // call into the template first
   PCMMLNode2 pNode = MMLToTemplate();
   if (!pNode)
      return NULL;

   MMLValueSet (pNode, gpszStretch, (int) m_dwStretch);
   MMLValueSet (pNode, gpszShape, (int) m_dwShape);
   MMLValueSet (pNode, gpszSize, &m_pSize);
   MMLValueSet (pNode, gpszFrameProfile, (int) m_dwFrameProfile);
   MMLValueSet (pNode, gpszFrameSize, &m_pFrameSize);
   MMLValueSet (pNode, gpszCanvasThick, m_fCanvasThick);
   MMLValueSet (pNode, gpszTable, (int) m_fTable);

   return pNode;
}

BOOL CObjectPainting::MMLFrom (PCMMLNode2 pNode)
{
   // call into the template first
   if (!MMLFromTemplate (pNode))
      return FALSE;

   m_dwStretch = (DWORD) MMLValueGetInt (pNode, gpszStretch, 0);
   m_dwShape = (DWORD) MMLValueGetInt (pNode, gpszShape, (int) 0);
   MMLValueGetPoint (pNode, gpszSize, &m_pSize);
   m_dwFrameProfile = (DWORD) MMLValueGetInt (pNode, gpszFrameProfile, (int) 0);
   MMLValueGetPoint (pNode, gpszFrameSize, &m_pFrameSize);
   m_fCanvasThick = MMLValueGetDouble (pNode, gpszCanvasThick, 0);
   m_fTable = (BOOL) MMLValueGetInt (pNode, gpszTable, (int) 0);


   // can embed?
   m_fCanBeEmbedded = !m_fTable;

   // genreate the noodle from this
   CreateNoodle ();

   return TRUE;
}


/*********************************************************************************
CObjectPainting::CreateNoodle - Looks at the shape and other member variables
and creates the noodle object.
*/
void CObjectPainting::CreateNoodle (void)
{
   // figure out the path
   CPoint apLoc[8];
   DWORD dwSegCurve[8];
   DWORD dwNum, dwDivide;
   dwNum = 0;
   dwDivide = 3;
   memset (apLoc, 0, sizeof(apLoc));
   memset (dwSegCurve, 0, sizeof(dwSegCurve));

   fp fWidth, fHeight;
   fWidth = m_pSize.p[0];
   fHeight = m_pSize.p[1];

   DWORD i;

   switch (m_dwShape) {
   case 0:  // rectangle
   default:
      apLoc[0].p[0] = -fWidth/2;
      apLoc[0].p[2] = -fHeight/2;

      apLoc[1].Copy (&apLoc[0]);
      apLoc[1].p[2] = fHeight/2;

      apLoc[2].Copy (&apLoc[1]);
      apLoc[2].p[0] = fWidth/2;

      apLoc[3].Copy (&apLoc[2]);
      apLoc[3].p[2] = -fHeight/2;

      dwNum = 4;
      dwDivide = 0;
      break;

   case 1:  // oval
      apLoc[0].p[0] = -fWidth/2;
      apLoc[0].p[2] = 0;
      dwSegCurve[0] = SEGCURVE_ELLIPSENEXT;

      apLoc[1].Copy (&apLoc[0]);
      apLoc[1].p[2] = fHeight/2;
      dwSegCurve[1] = SEGCURVE_ELLIPSEPREV;

      apLoc[2].Copy (&apLoc[1]);
      apLoc[2].p[0] = 0;
      dwSegCurve[2] = SEGCURVE_ELLIPSENEXT;

      apLoc[3].Copy (&apLoc[2]);
      apLoc[3].p[0] = fWidth/2;
      dwSegCurve[3] = SEGCURVE_ELLIPSEPREV;

      for (i = 4; i < 8; i++) {
         apLoc[i].Copy (&apLoc[i-4]);
         apLoc[i].Scale (-1);
         dwSegCurve[i] = dwSegCurve[i-4];
      }

      dwNum = 8;

      break;

   case 2:  // rounded
      if (fWidth > fHeight+CLOSE) {
         // curved on right and left
         apLoc[0].p[0] = -fWidth/2 + fHeight/2;
         apLoc[0].p[2] = -fHeight/2;
         dwSegCurve[0] = SEGCURVE_CIRCLENEXT;

         apLoc[1].p[0] = -fWidth/2;
         apLoc[1].p[2] = 0;
         dwSegCurve[1] = SEGCURVE_CIRCLEPREV;

         apLoc[2].p[0] = apLoc[0].p[0];
         apLoc[2].p[2] = -apLoc[0].p[2];
         dwSegCurve[2] = SEGCURVE_LINEAR;

         dwNum = 3;
      }
      else if (fHeight > fWidth+CLOSE) {
         // curved on top and bottom
         apLoc[0].p[2] = fHeight/2 - fWidth/2;
         apLoc[0].p[0] = -fWidth/2;
         dwSegCurve[0] = SEGCURVE_CIRCLENEXT;

         apLoc[1].p[2] = fHeight/2;
         apLoc[1].p[0] = 0;
         dwSegCurve[1] = SEGCURVE_CIRCLEPREV;

         apLoc[2].p[2] = apLoc[0].p[2];
         apLoc[2].p[0] = -apLoc[0].p[0];
         dwSegCurve[2] = SEGCURVE_LINEAR;

         dwNum = 3;
      }
      else {
         // completely round
         apLoc[0].p[0] = -fWidth/2;
         apLoc[0].p[2] = 0;
         dwSegCurve[0] = SEGCURVE_CIRCLENEXT;

         apLoc[1].p[2] = fHeight/2;
         apLoc[1].p[0] = 0;
         dwSegCurve[1] = SEGCURVE_CIRCLEPREV;

         dwNum = 2;
      }

      for (i = dwNum; i < dwNum*2; i++) {
         apLoc[i].Copy (&apLoc[i-dwNum]);
         apLoc[i].Scale (-1);
         dwSegCurve[i] = dwSegCurve[i-dwNum];
      }
      dwNum *= 2;
      break;
   }


   // set the curve
   m_nFrame.PathSpline (TRUE, dwNum, apLoc, dwSegCurve, dwDivide);

   // other info
   CPoint pFront, pOffset, pScale;
   pFront.Zero();
   pFront.p[1] = -1;
   m_nFrame.FrontVector (&pFront);
   m_nFrame.BackfaceCullSet (FALSE);
   pOffset.Zero();
   pOffset.p[0] = -m_pFrameSize.p[0]/2;
   pOffset.p[1] = -m_pFrameSize.p[1] / 2;
   m_nFrame.OffsetSet (&pOffset);
   pScale.Zero();
   pScale.p[0] = m_pFrameSize.p[0];
   pScale.p[1] = m_pFrameSize.p[1];
   m_nFrame.ScaleVector (&pScale);
   m_nFrame.ShapeDefault (m_dwFrameProfile ? m_dwFrameProfile : NS_RECTANGLE);

   // done
}


/**********************************************************************************
CObjectPainting::RenderCanvas - Draws the canvas, or the mat, or the backing.

NOTE: The texture should already have been set

inputs
   POBJECTRENDER     pr - Render information
   PCRenderSurface   prs - Render surface
   fp                fY - Y value to use. Should be NEGATIVE to be in front
   BOOL              fBorder - If TRUE, draws a border from fY to 0 (asumes fY is negative)
   BOOL              fStrech - If TRUE, stretch one width of the texture out to the entire canvas
   fp                fNewWidth, fNewHeight - New width and height to use - if scaled down
*/
void CObjectPainting::RenderCanvas (POBJECTRENDER pr, PCRenderSurface prs, fp fY, BOOL fBorder,
                                    BOOL fStretch, fp fNewWidth, fp fNewHeight)
{
   // get the path noodle
   PCSpline ps = m_nFrame.PathSplineGet ();
   DWORD dwNodes = ps->QueryNodes();

   fp fWidth, fHeight;
   fWidth = m_pSize.p[0];
   fHeight = m_pSize.p[1];

   // create all the points for the path plus a center point PLUS the border points
   DWORD dwNum, dwOffsetBorder, dwCenter;
   dwNum = dwNodes;
   if (fBorder) {
      dwOffsetBorder = dwNum;
      dwNum *= 2;
   }
   else
      dwOffsetBorder = 0;
   dwCenter = dwNum;
   dwNum++; // for center point

   PCPoint paPoints, paNormals;
   PTEXTPOINT5 paText;
   DWORD i;
   DWORD dwPointIndex, dwNormalIndex, dwTextIndex;
   
   // do all the points
   paPoints = prs->NewPoints (&dwPointIndex, dwNum);
   if (!paPoints)
      return;
   for (i = 0; i < dwNodes; i++) {
      paPoints[i].Copy (ps->LocationGet (i));
      paPoints[i].p[0] *= fNewWidth / fWidth;
      paPoints[i].p[2] *= fNewHeight / fHeight;
      paPoints[i].p[1] = fY;

      if (dwOffsetBorder) {
         paPoints[i+dwOffsetBorder].Copy (&paPoints[i]);
         paPoints[i+dwOffsetBorder].p[1] = 0;
      }
   }
   paPoints[dwCenter].Zero();
   paPoints[dwCenter].p[1] = fY;

   // normals
   paNormals = prs->NewNormals (TRUE, &dwNormalIndex, dwOffsetBorder*2+1);
   if (paNormals) {
      // perpendicular to tangents
      CPoint pUp;
      PCPoint pTan;
      pUp.Zero();
      pUp.p[1] = -1;
      for (i = 0; i < dwOffsetBorder; i++) {
         pTan = ps->TangentGet (i, FALSE);
         paNormals[i*2+0].CrossProd (&pUp, pTan);
         paNormals[i*2+0].p[1] = 0;
         paNormals[i*2+0].Normalize();

         pTan = ps->TangentGet (i, TRUE);
         paNormals[i*2+1].CrossProd (&pUp, pTan);
         paNormals[i*2+1].p[1] = 0;
         paNormals[i*2+1].Normalize();
      }

      // normal at dwOffsetBorder*2 is always -1
      paNormals[dwOffsetBorder*2].Copy (&pUp);
   }

   // textures
   paText = prs->NewTextures (&dwTextIndex, dwNodes+1);
   if (paText) {
      for (i = 0; i < dwNodes+1; i++) {
         PCPoint p;
         p = (i < dwNodes) ? &paPoints[i] : &paPoints[dwCenter];

         paText[i].hv[0] = p->p[0] + fNewWidth/2;   // so starts on left
         paText[i].hv[1] = fNewHeight/2-p->p[2];
         paText[i].xyz[0] = p->p[0];
         paText[i].xyz[1] = p->p[1];
         paText[i].xyz[2] = p->p[2];

         // different scaling depeing upon stretch
         if (fStretch) {
            paText[i].hv[0] /= fNewWidth;
            paText[i].hv[1] /= fNewHeight;
         }
      }

      if (!fStretch)
         prs->ApplyTextureRotation (paText, dwNodes+1);
   }

   // verticies
   PVERTEX pVert;
   DWORD dwVertIndex;
   pVert = prs->NewVertices (&dwVertIndex, dwNodes + dwOffsetBorder*4 + 1);
   if (!pVert)
      return;
   for (i = 0; i < dwNodes+dwOffsetBorder*4+1; i++) {
      // 0..dwNodes-1 = top points
      // At dwNodes - dwOffsetBorder*4 = edge points alternating between left/front, left/wall, right/front, right/wall
      // at dwNodes+dwOffsetBorder*4 = center verted
      if (paNormals) {
         if ((i < dwNodes) || (i >= dwNodes+dwOffsetBorder*4))
            pVert[i].dwNormal = dwNormalIndex + dwOffsetBorder*2;
         else
            pVert[i].dwNormal = dwNormalIndex + (i-dwNodes)/2;
      }
      else
         pVert[i].dwNormal = 0;

      if (i < dwNodes)
         pVert[i].dwPoint = dwPointIndex + i;
      else if (i >= dwNodes + dwOffsetBorder*4)
         pVert[i].dwPoint = dwPointIndex + dwCenter;
      else
         pVert[i].dwPoint = dwPointIndex + 
            ( ((i - dwNodes)%2) ? ((i-dwNodes)/4) : ((i-dwNodes)/4 + dwNodes) );

      if (paText) {
         if (i < dwNodes)
            pVert[i].dwTexture = dwTextIndex + i;
         else if (i >= dwNodes + dwOffsetBorder*4)
            pVert[i].dwTexture = dwTextIndex + dwNodes;
         else
            pVert[i].dwTexture = dwTextIndex + (i-dwNodes) / 4;
      }
      else
         pVert[i].dwTexture = 0;
   }

   // polygons on the front
   for (i = 0; i < dwNodes; i++)
      prs->NewTriangle (
         dwVertIndex + dwNodes + dwOffsetBorder*4,
         dwVertIndex + i,
         dwVertIndex + (i+1)%dwNodes);
   
   // and the sides
   if (fBorder)
      for (i = 0; i < dwNodes; i++)
         prs->NewQuad (
            dwVertIndex + dwNodes + ((i+1)%dwNodes)*4 + 0,
            dwVertIndex + dwNodes + ((i+1)%dwNodes)*4 + 1,
            dwVertIndex + dwNodes + i*4 + 3,
            dwVertIndex + dwNodes + i*4 + 2
            );

   // done
}



/**************************************************************************************
CObjectPainting::MoveReferencePointQuery - 
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
BOOL CObjectPainting::MoveReferencePointQuery (DWORD dwIndex, PCPoint pp)
{
   PSPLINEMOVEP ps;
   DWORD dwDataSize;
   ps = gaPaintingMove;
   dwDataSize = sizeof(gaPaintingMove);

   if (dwIndex >= dwDataSize / sizeof(SPLINEMOVEP))
      return FALSE;

   // always at 0,0 in Paintings
   pp->Zero();
   return TRUE;
}

/**************************************************************************************
CObjectPainting::MoveReferenceStringQuery -
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
BOOL CObjectPainting::MoveReferenceStringQuery (DWORD dwIndex, PWSTR psz, DWORD dwSize, DWORD *pdwNeeded)
{
   PSPLINEMOVEP ps;
   DWORD dwDataSize;
   ps = gaPaintingMove;
   dwDataSize = sizeof(gaPaintingMove);
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


/* PaintingDialogPage
*/
BOOL PaintingDialogPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCObjectPainting pv = (PCObjectPainting) pPage->m_pUserData;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         PCEscControl pControl;

         ComboBoxSet (pPage, L"shape", pv->m_dwShape);
         ComboBoxSet (pPage, L"profile", pv->m_dwFrameProfile ? pv->m_dwFrameProfile : NS_RECTANGLE);

         MeasureToString (pPage, L"matwidth", pv->m_pSize.p[2]);
         MeasureToString (pPage, L"matheight", pv->m_pSize.p[3]);
         MeasureToString (pPage, L"canvasthick", pv->m_fCanvasThick);
         MeasureToString (pPage, L"framewidth", pv->m_pFrameSize.p[0]);
         MeasureToString (pPage, L"frameheight", pv->m_pFrameSize.p[1]);

         PWSTR psz;
         switch (pv->m_dwStretch) {
         case 0:
         default:
            psz = L"nostretch";
            break;
         case 1:
            psz = L"stretch";
            break;
         case 2:
            psz = L"resize";
            break;
         }
         pControl = pPage->ControlFind (psz);
         if (pControl)
            pControl->AttribSetBOOL (Checked(), TRUE);

         pControl = pPage->ControlFind (L"table");
         if (pControl)
            pControl->AttribSetBOOL (Checked(), pv->m_fTable);
         pControl = pPage->ControlFind (L"showframe");
         if (pControl)
            pControl->AttribSetBOOL (Checked(), pv->m_dwFrameProfile ? TRUE : FALSE);

      }
      break;

   case ESCN_COMBOBOXSELCHANGE:
      {
         PESCNCOMBOBOXSELCHANGE p = (PESCNCOMBOBOXSELCHANGE) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;
         PWSTR psz;
         psz = p->pControl->m_pszName;
         DWORD dwVal;
         dwVal = p->pszName ? (DWORD) _wtoi(p->pszName) : 0;

         if (!_wcsicmp(psz, L"shape")) {
            if (dwVal == pv->m_dwShape)
               break;

            pv->m_pWorld->ObjectAboutToChange (pv);
            pv->m_dwShape = dwVal;
            pv->CreateNoodle ();
            pv->m_pWorld->ObjectChanged (pv);
            return TRUE;
         }
         else if (!_wcsicmp (psz, L"profile")) {
            if ((dwVal == pv->m_dwFrameProfile) || !pv->m_dwFrameProfile)
               break;   // no point worrying about this

            pv->m_pWorld->ObjectAboutToChange (pv);
            pv->m_dwFrameProfile = dwVal;
            pv->CreateNoodle ();
            pv->m_pWorld->ObjectChanged (pv);
            return TRUE;
         }
      }
      break;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;
         PWSTR psz;
         psz = p->pControl->m_pszName;

         if (!_wcsicmp(psz, L"showframe")) {
            BOOL fCheck = p->pControl->AttribGetBOOL (Checked());
            
            pv->m_pWorld->ObjectAboutToChange (pv);
            if (fCheck) {
               PCEscControl pControl;
               pControl = pPage->ControlFind (L"profile");
               if (!pControl)
                  break;
               ESCMCOMBOBOXGETITEM gi;
               memset (&gi, 0, sizeof(gi));
               gi.dwIndex = pControl->AttribGetInt (CurSel());
               pControl->Message (ESCM_COMBOBOXGETITEM, &gi);
               if (gi.pszName)
                  pv->m_dwFrameProfile = _wtoi(gi.pszName);
            }
            else
               pv->m_dwFrameProfile = 0;  // none
            pv->CreateNoodle ();
            pv->m_pWorld->ObjectChanged (pv);

            break;
         }
         else if (!_wcsicmp(psz, L"table")) {
            pv->m_pWorld->ObjectAboutToChange (pv);
            pv->m_fTable = p->pControl->AttribGetBOOL (Checked());
            pv->m_fCanBeEmbedded = !pv->m_fTable;
            pv->m_pWorld->ObjectChanged (pv);
            break;
         }
         else if (!_wcsicmp(psz, L"nostretch")) {
            pv->m_pWorld->ObjectAboutToChange (pv);
            pv->m_dwStretch = 0;
            pv->m_pWorld->ObjectChanged (pv);
            break;
         }
         else if (!_wcsicmp(psz, L"stretch")) {
            pv->m_pWorld->ObjectAboutToChange (pv);
            pv->m_dwStretch = 1;
            pv->m_pWorld->ObjectChanged (pv);
            break;
         }
         else if (!_wcsicmp(psz, L"resize")) {
            pv->m_pWorld->ObjectAboutToChange (pv);
            pv->m_dwStretch = 2;
            pv->m_pWorld->ObjectChanged (pv);
            break;
         }

      }
      break;

   case ESCN_EDITCHANGE:
      {
         PESCNEDITCHANGE p = (PESCNEDITCHANGE) pParam;

         if (!p->pControl || !p->pControl->m_pszName)
            break;
         PWSTR psz;
         psz = p->pControl->m_pszName;

         // since any edit change will result in redraw, get them all
         pv->m_pWorld->ObjectAboutToChange (pv);
         MeasureParseString (pPage, L"matwidth", &pv->m_pSize.p[2]);
         pv->m_pSize.p[2] = max(0,pv->m_pSize.p[2]);
         MeasureParseString (pPage, L"matheight", &pv->m_pSize.p[3]);
         pv->m_pSize.p[3] = max(0, pv->m_pSize.p[3]);
         MeasureParseString (pPage, L"canvasthick", &pv->m_fCanvasThick);
         pv->m_fCanvasThick = max(0,pv->m_fCanvasThick);
         MeasureParseString (pPage, L"framewidth", &pv->m_pFrameSize.p[0]);
         pv->m_pFrameSize.p[0] = max(.001,pv->m_pFrameSize.p[0]);
         MeasureParseString (pPage, L"frameheight", &pv->m_pFrameSize.p[1]);
         pv->m_pFrameSize.p[1] = max(.001,pv->m_pFrameSize.p[1]);
         pv->CreateNoodle ();
         pv->m_pWorld->ObjectChanged (pv);


         break;
      }

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Painting/rug settings";
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}


/**********************************************************************************
CObjectPainting::DialogShow - Causes the object to show a dialog box that allows
different attributes to be changed.

inputs
   DWORD             dwSuface - Surface that was clicked on. 0 if unknown.
   PCEscWindow       pWindow - Escarpment window to draw it in
returns
   BOOL - TRUE if user pressed "Back", FALSE if user pressed "Close"
*/
BOOL CObjectPainting::DialogShow (DWORD dwSurface, PCEscWindow pWindow, PCPoint pClick)
{
   PWSTR pszRet;
   pszRet = pWindow->PageDialog (ghInstance, IDR_MMLPAINTINGDIALOG, PaintingDialogPage, this);
   if (!pszRet)
      return FALSE;

   return !_wcsicmp(pszRet, Back());
}


/**********************************************************************************
CObjectPainting::DialogQuery - Returns TRUE if the object supports a custom
dialog box, FALSE if it doesn't. The default object doesn't
*/
BOOL CObjectPainting::DialogQuery (void)
{
   return TRUE;
}


/*************************************************************************************
CObjectPainting::ControlPointQuery - Called to query the information about a control
point identified by dwID.

inputs
   DWORD       dwID - ID
   POSCONTROL  pInfo - Filled in with the infomration
returns
   BOOL - TRUE if successful
*/
BOOL CObjectPainting::ControlPointQuery (DWORD dwID, POSCONTROL pInfo)
{
   fp fKnobSize = max(m_pFrameSize.p[0], m_pFrameSize.p[1]);
   fKnobSize = max(max(m_pSize.p[0],m_pSize.p[1])/20, fKnobSize);
   fKnobSize  = max(m_fCanvasThick, fKnobSize);
   fKnobSize *= 2;

   if (dwID >= 2)
      return FALSE;

   memset (pInfo,0, sizeof(*pInfo));

   pInfo->dwID = dwID;
   //pInfo->dwFreedom = 0;   // any direction
   pInfo->dwStyle = CPSTYLE_CUBE;
   pInfo->fSize = fKnobSize;
   pInfo->cColor = RGB(0,0xff,0xff);
   switch (dwID) {
   case 0:
   default:
      wcscpy (pInfo->szName, L"Width");
      break;
   case 1:
      wcscpy (pInfo->szName, L"Height");
      break;
   }
   MeasureToString (m_pSize.p[dwID], pInfo->szMeasurement);

   pInfo->pLocation.Zero();
   pInfo->pLocation.p[dwID ? 2 : 0] = m_pSize.p[dwID] / 2.0;

   // rotate?
   if (m_fTable) {
      pInfo->pLocation.p[2] += m_pSize.p[1] / 2.0;
      pInfo->pLocation.p[1] += pInfo->pLocation.p[2] * sin(PI/8);
      if (m_dwFrameProfile)
         pInfo->pLocation.p[2] += m_pFrameSize.p[0];
      pInfo->pLocation.p[2] *= cos(PI/8);
   }

   return TRUE;
}

/*************************************************************************************
CObjectPainting::ControlPointSet - Called to change the valud of a control point.

inputs
   DWORD       dwID - ID
   PCPoint     pVal - Contains the new location, in object coordinates
returns
   BOOL - TRUE if successful
*/
BOOL CObjectPainting::ControlPointSet (DWORD dwID, PCPoint pVal, PCPoint pViewer)
{
   if (dwID >= 2)
      return FALSE;

   // potentially untorate this
   CPoint pNew;
   pNew.Copy (pVal);
   if (m_fTable) {
      pNew.p[2] /= cos(PI/8);
      pNew.p[2] -= m_pSize.p[2] / 2;
      if (m_dwFrameProfile)
         pNew.p[2] -= m_pFrameSize.p[0];
      pNew.p[2] /= 2.0; // since will double later
      // no point worrying about the others
   }

   pNew.p[0] = max(CLOSE, pNew.p[0]);
   pNew.p[2] = max(CLOSE, pNew.p[2]);
   // tell the world we're about to change
   if (m_pWorld)
      m_pWorld->ObjectAboutToChange (this);

   if (dwID)
      m_pSize.p[1] = pNew.p[2]*2;
   else
      m_pSize.p[0] = pNew.p[0]*2;

   CreateNoodle ();

   // tell the world we've changed
   if (m_pWorld)
      m_pWorld->ObjectChanged (this);

   return TRUE;
}

/*************************************************************************************
CObjectPainting::ControlPointEnum - Called to enumerate a list of control point IDs
into the list.

inputs
   PCListFixed       plDWORD - Should be filled in with control points
returns
   none
*/
void CObjectPainting::ControlPointEnum (PCListFixed plDWORD)
{
   DWORD i;

   DWORD dwNum = 2;
   for (i = 0; i < dwNum; i++)
      plDWORD->Add (&i);
}

