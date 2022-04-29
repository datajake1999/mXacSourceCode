/*******************************************************************************
CRenderSurface.cpp - Code for the CRenderSurface object that generates
all the surfaces and calls into CRenderSocket. Basically, every object (whenever
its called to render) should create this and use it, destroying it when rendering
of the object is finished.

begun 7/9/2001 by Mike Rozak
Copyright 2001 by Mike Rozak. All rights reserved
*/

#include <windows.h>
#include <math.h>
#include "escarpment.h"
#include "..\M3D.h"

#define  MESH(x,y) ((x) + (y) * dwAcross)
#define  MESH2(x,y) ((x) + (y) * dwXT)


/*********************************************************************************
MemMalloc - Allocte memory for a CMem object.

inputs
   PCMem    pMem - CMem object
   DWORD    dwAlloc - Amount to allocate
returns
   PVOID - Memory, or NULL if error
*/
PVOID MemMalloc (PCMem pMem, size_t dwAlloc)
{
   size_t    dwIndex;
   
   // fp in size every time because if have large image is
   // constantly reallocated
   if (pMem->m_dwCurPosn + dwAlloc >= pMem->m_dwAllocated) {
      pMem->Required ((pMem->m_dwCurPosn + dwAlloc + 32) * 2);
   }

   dwIndex = pMem->Malloc(dwAlloc);
   if (dwIndex == (DWORD)-1)
      return NULL;
   return (PVOID) ((PBYTE)pMem->p + dwIndex);
}

/**********************************************************************************
CRenderSurface::Constructor and destructor
*/
CRenderSurface::CRenderSurface (void)
{
#ifdef MALLOCOPT
   m_memPoints.Required (1);  // so can detect mallocs for rendersurface
#endif

   ClearAll();
}

CRenderSurface::~CRenderSurface (void)
{
   Commit();
}


/**********************************************************************************
CRenderSurface::ClearAll - Clears out the contents
*/
void CRenderSurface::ClearAll (void)
{
   m_RenderMatrix.ClearAll();

   m_fNeedNormals = TRUE;
   m_fNeedTextures = TRUE;
   m_fBonesAlreadyApplied = FALSE;
   m_fDetail = .1;
   m_pSocket = NULL;

   m_aTextMatrixHVTemp[0][0] = m_aTextMatrixHVTemp[1][1] = 1;
   m_aTextMatrixHVTemp[0][1] = m_aTextMatrixHVTemp[1][0] = 0;
   m_fUseTextMatrixTemp = FALSE;

   m_cDefColor = RGB(0x80,0x80,0x80);
   memset (&m_DefRenderSurface, 0, sizeof(m_DefRenderSurface));
   m_DefRenderSurface.afTextureMatrix[0][0] = m_DefRenderSurface.afTextureMatrix[1][1] = 1;
   m_DefRenderSurface.afTextureMatrix[0][1] = m_DefRenderSurface.afTextureMatrix[1][0] = 0;
   m_DefRenderSurface.wMinorID = 1;
   m_dwCurIDPart = 0;

   CMatrix mIdent;
   mIdent.Identity();
   memcpy (m_DefRenderSurface.abTextureMatrix, &mIdent, sizeof(mIdent));

   Commit();
}

/**********************************************************************************
CRenderSurface::Init - Call this before calling any other function. It initializes
the object by remembering the render-socket, and querying it for some values.

inputs
   PCRenderSocket    pSocket - Socket to use.
returns
   none
*/
void CRenderSurface::Init (PCRenderSocket pSocket)
{
   // commit just in case changing objects
   Commit ();

   m_pSocket = pSocket;
   m_fNeedNormals = pSocket->QueryWantNormals();
   m_fNeedTextures = pSocket->QueryWantTextures();
   m_fDetail = pSocket->QueryDetail();
}


/**********************************************************************************
CRenderSurface::Commit - If there are polygons (or light sources, or whatver)
queued up, these are passed to the renderer. Once that's done everything is cleared.
*/
void CRenderSurface::Commit (void)
{
   // if there's no socket then can't do anything
   if (!m_pSocket)
      goto clear;

   // if there are polygons then draw them
   if (m_dwNumPolygons) {
      // set the rotatiion matrix
      CMatrix m;
      m_RenderMatrix.Get(&m);
      m_pSocket->MatrixSet (&m);

      POLYRENDERINFO pr;
      pr.dwNumColors = (DWORD)m_memColors.m_dwCurPosn / sizeof(COLORREF);
      pr.paColors = (COLORREF*) m_memColors.p;
      pr.dwNumNormals = (DWORD)m_memNormals.m_dwCurPosn / sizeof(CPoint);
      pr.paNormals = (PCPoint) m_memNormals.p;
      pr.paTextures = (PTEXTPOINT5) m_memTextures.p;
      pr.dwNumTextures = (DWORD)m_memTextures.m_dwCurPosn / sizeof(TEXTPOINT5);
      pr.dwNumPolygons = m_dwNumPolygons;
      pr.paPolygons = (PPOLYDESCRIPT)m_memPolygons.p;
      pr.dwNumPoints = (DWORD)m_memPoints.m_dwCurPosn / sizeof(CPoint);
      pr.paPoints = (PCPoint) m_memPoints.p;
      pr.dwNumSurfaces = (DWORD)m_memSurfaces.m_dwCurPosn / sizeof(RENDERSURFACE);
      pr.paSurfaces = (PRENDERSURFACE) m_memSurfaces.p;
      pr.dwNumVertices = (DWORD)m_memVertices.m_dwCurPosn / sizeof(VERTEX);
      pr.paVertices = (PVERTEX) m_memVertices.p;
      pr.fAlreadyNormalized = m_fAllNormalsNormalized;
      pr.fBonesAlreadyApplied = m_fBonesAlreadyApplied;

      // draw
      m_pSocket->PolyRender (&pr);
   }

clear:
   // wipe it all out
   m_memPoints.m_dwCurPosn = 0;
   m_memNormals.m_dwCurPosn = 0;
   m_memTextures.m_dwCurPosn = 0;
   m_fAllNormalsNormalized = TRUE;
   m_memVertices.m_dwCurPosn = 0;
   m_memColors.m_dwCurPosn = 0;
   m_memSurfaces.m_dwCurPosn = 0;
   m_memPolygons.m_dwCurPosn = 0;
   m_dwNumPolygons = 0;
   m_dwIndexDefColor = -1;
   m_dwIndexDefSurface = -1;
   DWORD i;
   for (i = 0; i < 6; i++)
      m_adwIndexDefDirection[i] = -1;
}



/**********************************************************************************
CRenderSurface::SetDefColor - Sets the new default color to be used by all the shapes
from here-on.

inputs
   COLORREF    cColor - New color
returns
   none
*/
void CRenderSurface::SetDefColor (COLORREF cColor)
{
   if (m_cDefColor != cColor) {
      m_cDefColor = cColor;
      m_dwIndexDefColor = -1; // so next time it's requested gets added
   }
}

/**********************************************************************************
CRenderSurface::SetDefMaterial - Sets the new default material. All surfaces creates
with this material from here-on will use this color.

NOTE: Suggest using the SetDefMaterial (PCObjectSurface) instead since it will
talk to the CObjectTemplate nicely.

inputs
   PRENDERSURFACE    pMaterial - material
returns
   none
*/
void CRenderSurface::SetDefMaterial (PRENDERSURFACE pMaterial)
{
   m_DefRenderSurface = *pMaterial;
   m_dwIndexDefSurface = -1;
}


/**********************************************************************************
CRenderSurface::SetDefMaterial - Sets the new default material. All surfaces creates
with this material from here-on will use this color.

NOTE: If there's a texture, this automagically adjusts the base color to the average
of the texture map.

inputs
   PCObjectSurface      pos - Object surface to use. (If NULL then default surface is black).
                           If the object surface refers to a scheme then that color/texture
                           is used in preference.
   PCWorldSocket              pWorld - If not NULL, this will get the surface information from it
                           the world's CSurfaceScheme.
returns
   none
*/
void CRenderSurface::SetDefMaterial (DWORD dwRenderShard, PCObjectSurface pos, PCWorldSocket pWorld)
{
   // create new material
   RENDERSURFACE rs;
   CMatrix mIdent;
   PCObjectSurface ps = NULL;
   memset (&rs, 0, sizeof(rs));
   rs.afTextureMatrix[0][0] = rs.afTextureMatrix[1][1] = 1;
   rs.afTextureMatrix[0][1] = rs.afTextureMatrix[1][0] = 0;
   mIdent.Identity();
   memcpy (rs.abTextureMatrix, &mIdent, sizeof(mIdent));

   // BUGFIX - Keep track of the scheme so know what to remap when
   // have CObjectEditor remappig schemes
   if (pos)
      wcscpy (rs.szScheme, pos->m_szScheme);

   if (!pos) {
      SetDefMaterial (&rs);
      SetDefColor (RGB(0,0,0));
      return;
   }

   rs.wMinorID = (WORD) pos->m_dwID;

   // Handle getting information from schemes
   if (pos->m_szScheme[0] && pWorld) {
      ps = (pWorld->SurfaceSchemeGet())->SurfaceGet (pos->m_szScheme, pos, TRUE);

      // if found a match then take over and use that
      if (ps)
         pos = ps;
   }

   // set transparency
   memcpy (&rs.Material, &pos->m_Material, sizeof(pos->m_Material));
   //rs.wTransparency = pos->m_wTransparency;

   COLORREF cColor;
   cColor = pos->m_cColor;

   // texture info transferred, assuming the renderer wants the texture info
   if (pos->m_fUseTextureMap) {
      // BUGFIX - Took out  (&& m_fNeedTextures) so that would still use texture map
      // for color even if drawing solid
      rs.fUseTextureMap = TRUE;
      rs.gTextureCode = pos->m_gTextureCode;
      rs.gTextureSub = pos->m_gTextureSub;
      rs.TextureMods = pos->m_TextureMods;
      memcpy (&rs.afTextureMatrix, &pos->m_afTextureMatrix, sizeof(pos->m_afTextureMatrix));
      memcpy (&rs.abTextureMatrix, &pos->m_mTextureMatrix, sizeof(pos->m_mTextureMatrix));

      PCTextureMapSocket pMap;
      // if there is a texture map then get it's average color
      if (pMap = TextureCacheGet (dwRenderShard, &rs, NULL, NULL)) {
         cColor = pMap->AverageColorGet(0, FALSE); // NOTE: Not including glow color here
         TextureCacheRelease (dwRenderShard, pMap);
      }
      
   }

   // set both the material and color
   SetDefMaterial (&rs);
   SetDefColor (cColor);

   // if got the information from the surfacescheme then delte it
   // because noclone if (ps)
   // because noclone    delete ps;
}


/**********************************************************************************
CRenderSurface::NewPoints - Allocates the space for dwNumPoints. It's up to
the caller to fill them in.

inputs
   DWORD       *pdwIndex - Filled with the index (into the points list) of the first
                           point. The others follow sequentially. Use this number
                           in the VERTEX structure form NewVertices(). Of course,
                           this is invalid once Commit() is called, or any matrix operations
                           are done. All points will automatically be modified by the current
                           rotation/translation matrix ONCE Commit() is called.
   DWORD       dwNum - Number of points to create
returns
   PCPoint - Pointer to the first point. This pointer is only valid until the next
   call to NewPoints(), so fill it quickly. Use *pdwIndex to remember values.
*/
PCPoint CRenderSurface::NewPoints (DWORD *pdwIndex, DWORD dwNum)
{
   *pdwIndex = (DWORD)m_memPoints.m_dwCurPosn / sizeof(CPoint);
	MALLOCOPT_INIT;
	MALLOCOPT_OKTOMALLOC;
   PCPoint pRet = (PCPoint) MemMalloc (&m_memPoints, dwNum * sizeof(CPoint));
	MALLOCOPT_RESTORE;
   return pRet;
}


/**********************************************************************************
CRenderSurface::NewPoint - Allocate a new point.

inputs
   fp      x,y,z - Coordinates. These are autaomtically modified by the current
                        rotation/translation matrix ONCE Commit() is called.
returns
   DWORD - Index for the point. Use this for VERTEX call and others. Valid until Commit() called.
*/
DWORD CRenderSurface::NewPoint (fp x, fp y, fp z)
{
   DWORD dwIndex;
   PCPoint p;
   p = NewPoints (&dwIndex);
   if (!p)
      return 0;
   p->p[0] = x;
   p->p[1] = y;
   p->p[2] = z;
   return dwIndex;
}


/**********************************************************************************
CRenderSurface::NewPoint - Allocate a new point.

inputs
   PCPoint p - Coordinates. These are autaomtically modified by the current
                        rotation/translation matrix ONCE Commit() is called.
returns
   DWORD - Index for the point. Use this for VERTEX call and others. Valid until Commit() called.
*/
DWORD CRenderSurface::NewPoint (PCPoint p)
{
   return NewPoint (p->p[0], p->p[1], p->p[2]);
}

/**********************************************************************************
CRenderSurface::NewTextures - Allocates the space for dwNumTextures. It's up to
the caller to fill them in.

inputs
   DWORD       *pdwIndex - Filled with the index (into the Textures list) of the first
                           Texture. The others follow sequentially. Use this number
                           in the VERTEX structure form NewVertices(). Of course,
                           this is invalid once Commit() is called, or any matrix operations
                           are done. All Textures will automatically be modified by the current
                           rotation/translation matrix ONCE Commit() is called.
   DWORD       dwNum - Number of Textures to create
returns
   PCPoint - Pointer to the first Texture. This pointer is only valid until the next
   call to NewTextures(), so fill it quickly. Use *pdwIndex to remember values.
*/
PTEXTPOINT5 CRenderSurface::NewTextures (DWORD *pdwIndex, DWORD dwNum)
{
   // if don't need Textures then exit
   if (!m_fNeedTextures) {
      *pdwIndex = NULL;
      return NULL;
   }

   *pdwIndex = (DWORD)m_memTextures.m_dwCurPosn / sizeof(TEXTPOINT5);
	MALLOCOPT_INIT;
	MALLOCOPT_OKTOMALLOC;
   PTEXTPOINT5 pt = (PTEXTPOINT5) MemMalloc (&m_memTextures, dwNum * sizeof(TEXTPOINT5));
	MALLOCOPT_RESTORE;
   return pt;
}

/**********************************************************************************
CRenderSurface::NewTexture - Allocate a new Texture.

inputs
   fp      h,v - Coordinates.
returns
   DWORD - Index for the Texture. Use this for VERTEX call and others. Valid until Commit() called.
*/
DWORD CRenderSurface::NewTexture (fp h, fp v, fp x, fp y, fp z)
{
   DWORD dwIndex;
   PTEXTPOINT5 p;
   p = NewTextures (&dwIndex);
   if (!p)
      return 0;
   p->hv[0] = h;
   p->hv[1] = v;
   p->xyz[0] = x;
   p->xyz[1] = y;
   p->xyz[2] = z;

   return dwIndex;
}


/**********************************************************************************
CRenderSurface::NewTexture - Creates a new texture based on a point. The
X and Z values of the point are used directly for the texture's h and v.
(Except that Z is reversed so that positive numbers go down.)

inputs
   PCPoint     p - point where [0] and [2] are used for HV, all uysed for XYZ
returns
   DWORD - Index for the Texture. Use this for VERTEX call and others. Valid until Commit() called.
*/
DWORD CRenderSurface::NewTexture (PCPoint p)
{
   return NewTexture (p->p[0], -p->p[2], p->p[0], p->p[1], p->p[2]);
}



/**********************************************************************************
CRenderSurface::NewNormals - Allocates the space for dwNumNormals. It's up to
the caller to fill them in.

inputs
   BOOL        fNormalized - Use TRUE if the when the caller will set the points they
                           will all be normalized. Use FALSE if they might not be and
                           the rendering system will need to normalize.
   DWORD       *pdwIndex - Filled with the index (into the Normals list) of the first
                           Normal. The others follow sequentially. Use this number
                           in the VERTEX structure form NewVertices(). Of course,
                           this is invalid once Commit() is called, or any matrix operations
                           are done. All Normals will automatically be modified by the current
                           rotation/translation matrix ONCE Commit() is called.
   DWORD       dwNum - Number of Normals to create
returns
   PCPoint - Pointer to the first Normal. This pointer is only valid until the next
   call to NewNormals(), so fill it quickly. Use *pdwIndex to remember values.
*/
PCPoint CRenderSurface::NewNormals (BOOL fNormalized, DWORD *pdwIndex, DWORD dwNum)
{
   // if don't need normals then exit
   if (!m_fNeedNormals) {
      *pdwIndex = NULL;
      return NULL;
   }

   // keep track to make sure all normals are normalized
   m_fAllNormalsNormalized &= fNormalized;

   *pdwIndex = (DWORD)m_memNormals.m_dwCurPosn / sizeof(CPoint);
	MALLOCOPT_INIT;
	MALLOCOPT_OKTOMALLOC;
   PCPoint p = (PCPoint) MemMalloc (&m_memNormals, dwNum * sizeof(CPoint));
	MALLOCOPT_RESTORE;
   return p;
}

/**********************************************************************************
CRenderSurface::NewNormal - Allocate a new Normal.

inputs
   fp      x,y,z - Coordinates. These are autaomtically modified by the current
                        rotation/translation matrix ONCE Commit() is called.
   BOOL        fNormalized - Use TRUE if the length of xyz is 1.0. Else, set this to
                        FALSE and NewNormal() will automatically normalize it.
returns
   DWORD - Index for the Normal. Use this for VERTEX call and others. Valid until Commit() called.
*/
DWORD CRenderSurface::NewNormal (fp x, fp y, fp z, BOOL fNormalized)
{
   DWORD dwIndex;
   PCPoint p;
   p = NewNormals (TRUE, &dwIndex);
   if (!p)
      return 0;
   p->p[0] = x;
   p->p[1] = y;
   p->p[2] = z;

   if (!fNormalized)
      p->Normalize();

   return dwIndex;
}

/**********************************************************************************
CRenderSurface::NewNormal - Allocate a new Normal.

inputs
   PCPoint     p1,p2,p3 - Three points in clockwise direction that are used to
               calculate the normal. The new normal is automagically normalized.
returns
   DWORD - Index for the Normal. Use this for VERTEX call and others. Valid until Commit() called.
*/
DWORD CRenderSurface::NewNormal (PCPoint p1, PCPoint p2, PCPoint p3)
{
   DWORD dwIndex;
   PCPoint p;
   p = NewNormals (TRUE, &dwIndex);
   if (!p)
      return 0;
   p->MakeANormal (p1, p2, p3, TRUE);

   return dwIndex;
}

/**********************************************************************************
CRenderSurface::DefNormal - Returns the index into one of the 6 default normals
   (right, left, forwards, backwards, up, down). This makes drawing boxes and other
   square bits (which are common in houses) as fast as possible.

inputs
   DWORD       dwDir - 0=right, 1=left, 2=forwards (away from viewer), 3=backwards(towards viewer),
                        4=up, 5=down
returns
   DWORD - Index for the normal. Only valid until Commit() is called. If normals
            are not needed by the renderer this returns 0.
*/
DWORD CRenderSurface::DefNormal (DWORD dwDir)
{
   if (!m_fNeedNormals)
      return 0;

   if (m_adwIndexDefDirection[dwDir] != -1)
      return m_adwIndexDefDirection[dwDir];

   // else, create
   DWORD dwRet;
   switch (dwDir) {
   default:
   case 0:  // right
      dwRet = NewNormal (1,0,0,TRUE);
      break;
   case 1:  // left
      dwRet = NewNormal (-1,0,0,TRUE);
      break;
   case 2:  // forwards
      dwRet = NewNormal (0,1,0,TRUE);
      break;
   case 3:  // backwards
      dwRet = NewNormal (0,-1,0,TRUE);
      break;
   case 4:  // up
      dwRet = NewNormal (0,0,1,TRUE);
      break;
   case 5:  // down
      dwRet = NewNormal (0,0,-1,TRUE);
      break;
   }
   m_adwIndexDefDirection[dwDir] = dwRet;
   return m_adwIndexDefDirection[dwDir];
}

/**********************************************************************************
CRenderSurface::NewColors - Allocates the space for dwNumColors. It's up to
the caller to fill them in.

inputs
   DWORD       *pdwIndex - Filled with the index (into the Colors list) of the first
                           Color. The others follow sequentially. Use this number
                           in the VERTEX structure form NewVertices(). Of course,
                           this is invalid once Commit() is called, or any matrix operations
                           are done.
   DWORD       dwNum - Number of Colors to create
returns
   COLORREF * - Pointer to the first Color. This pointer is only valid until the next
   call to NewColors(), so fill it quickly. Use *pdwIndex to remember values.
*/
COLORREF *CRenderSurface::NewColors (DWORD *pdwIndex, DWORD dwNum)
{
   *pdwIndex = (DWORD)m_memColors.m_dwCurPosn / sizeof(COLORREF);
   return (COLORREF*) MemMalloc (&m_memColors, dwNum * sizeof(COLORREF));
}

/**********************************************************************************
CRenderSurface::DefColor - Returns the index to the default color.

inputs
   none
retursn
   DWORD - Index used in VERTEX. This is only valid until the next Commit()
*/
DWORD CRenderSurface::DefColor (void)
{
   if (m_dwIndexDefColor != -1)
      return m_dwIndexDefColor;

   COLORREF *p;
   DWORD dw;
   p = NewColors (&dw);
   if (!p)
      return 0;
   m_dwIndexDefColor = dw;
   *p = m_cDefColor;
   return m_dwIndexDefColor;

}


/**********************************************************************************
CRenderSurface::GetDefColor - Returns the color ref of default color

inputs
   none
retursn
   COLORREF - color
*/
COLORREF CRenderSurface::GetDefColor (void)
{
   return m_cDefColor;
}

/**********************************************************************************
CRenderSurface::NewVertices - Allocates the space for dwNumVertices. It's up to
the caller to fill them in.

NOTE: The vertices will all have DefColor() filled in for the color. Normals will
be set to 0 just in case no normals are needed. Just a time
saver for the programmer.

inputs
   DWORD       *pdwIndex - Filled with the index (into the Vertices list) of the first
                           point. The others follow sequentially. Use this number
                           for polygons creation functions. Of course,
                           this is invalid once Commit() is called, or any matrix operations
                           are done.
   DWORD       dwNum - Number of Vertices to create
returns
   PVERTEX - Pointer to the first vertex. This pointer is only valid until the next
   call to NewVertices(), so fill it quickly. Use *pdwIndex to remember values.
*/
PVERTEX CRenderSurface::NewVertices (DWORD *pdwIndex, DWORD dwNum)
{
   PVERTEX p;

   *pdwIndex = (DWORD)m_memVertices.m_dwCurPosn / sizeof(VERTEX);
	MALLOCOPT_INIT;
	MALLOCOPT_OKTOMALLOC;
   p = (PVERTEX) MemMalloc (&m_memVertices, dwNum * sizeof(VERTEX));
	MALLOCOPT_RESTORE;
   if (!p)
      return NULL;

   DWORD i;
   DWORD dwDef;
   dwDef = DefColor();
   for (i = 0; i < dwNum; i++) {
      p[i].dwColor = dwDef;
      p[i].dwNormal = 0;
      p[i].dwTexture = 0;
   }

   return p;
}

/**********************************************************************************
CRenderSurface::NewVertex - Creates a new vertex from a point, and normal
   index. The color used is the default color.

inputs
   DWORD       dwPoint - Point index, from NewPoint()
   DWORD       dwNormal - Noraml index, from NewNormal()
   DWORD       dwTexture - Texture index, from NewTexture()
returns
   DWORD - Index into the vertex
*/
DWORD CRenderSurface::NewVertex (DWORD dwPoint, DWORD dwNormal, DWORD dwTexture)
{
   DWORD dwIndex;
   PVERTEX p = NewVertices (&dwIndex);
   if (!p)
      return 0;
   p->dwPoint = dwPoint;
   p->dwNormal = dwNormal;
   p->dwTexture = dwTexture;

   return dwIndex;
}

/**********************************************************************************
CRenderSurface::NewPolygon - Allocates the space for a new polygon.

NOTE: To make development easier... POLYDESCRIPT.dwSurface is set to the default
surface. fCanBackfaceCull is set to TRUE. wNumVertices is set to dwNumVertices.
dwIDPart is set to m_dwCurIDPart. Just the vertex numbers need to be set.

inputs
   DWORD       dwNumVertices - Number of vertices it will have. The space is allocated
returns
   PPOLYDESCRIPT - Pointer to the polygon. This pointer is only valid until the next
                  call to NewPolygon() or Commit().
*/
PPOLYDESCRIPT CRenderSurface::NewPolygon (DWORD dwNumVertices)
{
   PPOLYDESCRIPT p;
	MALLOCOPT_INIT;
	MALLOCOPT_OKTOMALLOC;
   p = (PPOLYDESCRIPT) MemMalloc (&m_memPolygons, sizeof(POLYDESCRIPT) + dwNumVertices * sizeof(DWORD));
	MALLOCOPT_RESTORE;
   if (!p)
      return NULL;
   p->dwSurface = DefSurface();
   p->fCanBackfaceCull = TRUE;
   p->wNumVertices = (WORD) dwNumVertices;
   p->dwIDPart = m_dwCurIDPart;

   m_dwNumPolygons++;
   
   return p;
}

/**********************************************************************************
CRenderSurface::NewPolygons - Allocates the space for a new polygons. usually
not called unless doing bulk polygons. Try NewPolygon() instead.

NOTE: This doesnt set the memory

inputs
   DWORD       dwMem - Memory to be used by the new polygons
   DWORD       dwNum - Number of polygons created
returns
   PPOLYDESCRIPT - Pointer to the polygon. This pointer is only valid until the next
                  call to NewPolygon() or Commit().
*/
PPOLYDESCRIPT CRenderSurface::NewPolygons (DWORD dwMem, DWORD dwNum)
{
   PPOLYDESCRIPT p;
   p = (PPOLYDESCRIPT) MemMalloc (&m_memPolygons, dwMem);
   if (!p)
      return NULL;

   m_dwNumPolygons += dwNum;
   
   return p;
}

/**********************************************************************************
CRenderSurface::NewTriangle - Creates a new triangle and fills in the vertices as
specified.

inputs
   DWORD       dwV1, dwV2, dwV3 - Vertices in a clockwise direction
   BOOL        fCanBackfaceCull - Set to TRUE if it's OK to backface cull this, FALSE if not
returns
   PPOLYDESCRIPT - Pointer to the polygon. This pointer is only valid until the next
                  call to NewPolygon() or Commit().
*/
PPOLYDESCRIPT CRenderSurface::NewTriangle (DWORD dwV1, DWORD dwV2, DWORD dwV3, BOOL fCanBackfaceCull)
{
   PPOLYDESCRIPT p;
   p = NewPolygon (3);
   if (!p)
      return NULL;
   p->fCanBackfaceCull = fCanBackfaceCull;
   DWORD *pdw;
   pdw = (DWORD*) (p+1);
   pdw[0] = dwV1;
   pdw[1] = dwV2;
   pdw[2] = dwV3;

   return p;
}

/**********************************************************************************
CRenderSurface::NewQuad - Creates a new quadrilateral and fills in the vertices as
specified.

inputs
   DWORD       dwV1, dwV2, dwV3, dwV4 - Vertices in a clockwise direction
   BOOL        fCanBackfaceCull - Set to TRUE if it's OK to backface cull this, FALSE if not
returns
   PPOLYDESCRIPT - Pointer to the polygon. This pointer is only valid until the next
                  call to NewPolygon() or Commit().
*/
PPOLYDESCRIPT CRenderSurface::NewQuad (DWORD dwV1, DWORD dwV2, DWORD dwV3, DWORD dwV4, BOOL fCanBackfaceCull)
{
   PPOLYDESCRIPT p;
   p = NewPolygon (4);
   if (!p)
      return NULL;
   p->fCanBackfaceCull = fCanBackfaceCull;
   DWORD *pdw;
   pdw = (DWORD*) (p+1);
   pdw[0] = dwV1;
   pdw[1] = dwV2;
   pdw[2] = dwV3;
   pdw[3] = dwV4;

   return p;
}

/**********************************************************************************
CRenderSurface::NewSurfaces - Allocates the space for dwNumSurfaces. It's up to
the caller to fill them in. (Although they are set to 0 by the initialization.)

inputs
   DWORD       *pdwIndex - Filled with the index (into the Surfaces list) of the first
                           point. The others follow sequentially. Use this number
                           for polygons creation functions. Of course,
                           this is invalid once Commit() is called, or any matrix operations
                           are done.
   DWORD       dwNum - Number of Surfaces to create
returns
   PRENDERSURFACE - Pointer to the first surface. This pointer is only valid until the next
   call to NewSurfaces(), so fill it quickly. Use *pdwIndex to remember values.
*/
PRENDERSURFACE CRenderSurface::NewSurfaces (DWORD *pdwIndex, DWORD dwNum)
{
   PRENDERSURFACE p;

   *pdwIndex = (DWORD)m_memSurfaces.m_dwCurPosn / sizeof(RENDERSURFACE);
   p = (PRENDERSURFACE) MemMalloc (&m_memSurfaces, dwNum * sizeof(RENDERSURFACE));
   if (!p)
      return NULL;
   memset (p, 0, sizeof(RENDERSURFACE) * dwNum);

   return p;
}

/**********************************************************************************
CRenderSurface::DefSurface - Returns the index to the default surface.

inputs
   none
retursn
   DWORD - Index into the default surface. This is only valid until the next
         time Commit() is called.
*/
DWORD CRenderSurface::DefSurface (void)
{
   if (m_dwIndexDefSurface != -1)
      return m_dwIndexDefSurface;

   DWORD dw;
   PRENDERSURFACE p;
   p = NewSurfaces (&dw);
   if (!p)
      return 0;
   *p = m_DefRenderSurface;
   m_dwIndexDefSurface = dw;
   return m_dwIndexDefSurface;
}

/**********************************************************************************
CRenderSurface::GetDefSurface - Returns the default surface as a pointer to a RENDERSURFACE.

inputs
   none
returns
   PRENDERSURFACE - Default surface. Only valid until another surface is added or. Do not modify
   object drawn.
*/
PRENDERSURFACE CRenderSurface::GetDefSurface (void)
{
   return &m_DefRenderSurface;
}


/**********************************************************************************
CRenderSurface::NewIDPart - Increments the current part ID - teritiary object iD.
Basically, the only use for this is to ensure that the OutlineObjects() call
in CImage() draws a line at the edge. To emphasize a hard edge, such as on a box, call
this before drawing each side. Conversely, if you're drawing a sphere or something rounded,
DON'T call this.

inputs
   none
returns
   DWORD - New part iD
*/
DWORD CRenderSurface::NewIDPart (void)
{
   m_dwCurIDPart = (m_dwCurIDPart + 1) & IDPARTBITS_MASK;
   return m_dwCurIDPart;
}

/**********************************************************************************
CRenderSurface::SetIDPart - Set the part ID to a specific value.
*/
void CRenderSurface::SetIDPart (DWORD dwVal)
{
   m_dwCurIDPart = dwVal & IDPARTBITS_MASK;
}


/**********************************************************************************
CRenderSurface::NewIDMinor - Increments the current minor ID - secondary object iD.
It keeps the same RENDERSURFACE data except increases this value.

inputs
   none
returns
   WORD - New part iD
*/
WORD CRenderSurface::NewIDMinor (void)
{
   m_DefRenderSurface.wMinorID++;
   m_dwIndexDefSurface = -1;

   return m_DefRenderSurface.wMinorID;
}

/**********************************************************************************
CRenderSurface::Matrix functions - The following matrix functions just call into
m_RenderMatrix with the same paremeters. THe only difference is thsat they all
(except for Push() and Get()) call Commit() first to make sure any points generated with the
previous rotation/translation were drawn.

The matrix operations are applied to all points and normals passed into
CRenderSurface.
*/

void CRenderSurface::Push (void)
{
   m_RenderMatrix.Push();
}

BOOL CRenderSurface::Pop (void)
{
   Commit();
   return m_RenderMatrix.Pop();
}

void CRenderSurface::Set (const PCMatrix pSet)
{
   Commit();
   m_RenderMatrix.Set(pSet);
}

void CRenderSurface::Get (PCMatrix pGet)
{
   m_RenderMatrix.Get(pGet);
}

void CRenderSurface::Multiply (const PCMatrix pm)
{
   Commit();
   m_RenderMatrix.Multiply(pm);
}

void CRenderSurface::Translate (fp x, fp y, fp z)
{
   Commit();
   m_RenderMatrix.Translate(x,y,z);
}

void CRenderSurface::Rotate (fp fRadians, DWORD dwDim)
{
   Commit();
   m_RenderMatrix.Rotate(fRadians,dwDim);
}

void CRenderSurface::Scale (fp fScale)
{
   Commit();
   m_RenderMatrix.Scale(fScale);
}

void CRenderSurface::Scale (fp x, fp y, fp z)
{
   Commit();
   m_RenderMatrix.Scale(x,y,z);
}

void CRenderSurface::TransRot (const PCPoint p1, const PCPoint p2)
{
   Commit();
   m_RenderMatrix.TransRot(p1,p2);
}

void CRenderSurface::Clear (void)
{
   Commit();
   m_RenderMatrix.Clear();
}



/**********************************************************************************
CRenderSurface::ShapeBox - Draws a box that's aligned to the xyz grid.

inputs
   fp      x1,y1,z2 - Coordinates of one corner
   fp      x2,y2,z2 - Coordinates of the other corner
returns
   none
*/
void CRenderSurface::ShapeBox (fp x1, fp y1, fp z1, fp x2, fp y2, fp z2)
{
   // calculate all the points
   DWORD dwIndex, dwTextureIndex;
   dwTextureIndex = 0;  // BUGFIX - So initialized
   PCPoint pp = NewPoints (&dwIndex, 8);
   PTEXTPOINT5 pt = m_DefRenderSurface.fUseTextureMap ? NewTextures(&dwTextureIndex, 24) : NULL;
   if (!pp)
      return;

#define  XLEFT    0x01
#define  XRIGHT   0
#define  YFRONT   0x02
#define  YBACK   0
#define  ZBOTTOM  0x04
#define  ZTOP     0

   int   i;
   for (i = 0; i < 8; i++) {
      // values
      pp[i].p[0] = ((i & XLEFT) ? min(x1,x2) : max(x1,x2));
      pp[i].p[1] = ((i & YFRONT) ? min(y1,y2) : max(y1,y2));
      pp[i].p[2] = ((i & ZBOTTOM) ? min(z1,z2) : max(z1,z2));
   }

   DWORD dwRemap[24] = {   // remaps from TP number 0..17 to TP number 0..23
      XLEFT|YFRONT|ZBOTTOM, XLEFT|YBACK|ZBOTTOM, XLEFT|YBACK|ZTOP, XLEFT|YFRONT|ZTOP,
      XRIGHT|YFRONT|ZTOP, XRIGHT|YBACK|ZTOP, XRIGHT|YBACK|ZBOTTOM, XRIGHT|YFRONT|ZBOTTOM,
      16, 17, XRIGHT|YBACK|ZBOTTOM, XRIGHT|YBACK|ZTOP,
      XLEFT|YFRONT|ZTOP, XRIGHT|YFRONT|ZTOP, XRIGHT|YFRONT|ZBOTTOM, XLEFT|YFRONT|ZBOTTOM,
      11, 8, 9, 10,
      14, 15, 12, 13
   };
   DWORD dwPointRemap[24] = { // so know what points are associated with what vertices
      XLEFT|YFRONT|ZBOTTOM, XLEFT|YBACK|ZBOTTOM, XLEFT|YBACK|ZTOP, XLEFT|YFRONT|ZTOP,
      XRIGHT|YFRONT|ZTOP, XRIGHT|YBACK|ZTOP, XRIGHT|YBACK|ZBOTTOM, XRIGHT|YFRONT|ZBOTTOM,
      XLEFT|YBACK|ZTOP, XLEFT|YBACK|ZBOTTOM, XRIGHT|YBACK|ZBOTTOM, XRIGHT|YBACK|ZTOP,
      XLEFT|YFRONT|ZTOP, XRIGHT|YFRONT|ZTOP, XRIGHT|YFRONT|ZBOTTOM, XLEFT|YFRONT|ZBOTTOM,
      XLEFT|YFRONT|ZTOP, XLEFT|YBACK|ZTOP, XRIGHT|YBACK|ZTOP, XRIGHT|YFRONT|ZTOP,
      XLEFT|YFRONT|ZBOTTOM, XRIGHT|YFRONT|ZBOTTOM, XRIGHT|YBACK|ZBOTTOM, XLEFT|YBACK|ZBOTTOM
   };

   if (pt) {
      TEXTPOINT5 pTemp[18];

      // go around the top
      pTemp[XLEFT|YBACK|ZTOP].hv[0] = 0;
      pTemp[XLEFT|YBACK|ZTOP].hv[1] = 0;

      pTemp[XLEFT|YFRONT|ZTOP].hv[0] = fabs(y1-y2);
      pTemp[XLEFT|YFRONT|ZTOP].hv[1] = 0;

      pTemp[XRIGHT|YFRONT|ZTOP].hv[0] = fabs(y1-y2) + fabs(x1-x2);
      pTemp[XRIGHT|YFRONT|ZTOP].hv[1] = 0;

      pTemp[XRIGHT|YBACK|ZTOP].hv[0] = 2 * fabs(y1-y2) + fabs(x1-x2);
      pTemp[XRIGHT|YBACK|ZTOP].hv[1] = 0;

      // and the bottom
      for (i = 0; i < 8; i++) {
         if (!(i & ZBOTTOM))
            continue;

         pTemp[i].hv[0] = pTemp[i & ~ZBOTTOM].hv[0];
         pTemp[i].hv[1] = fabs(z1-z2);
      }

      // back
      pTemp[16] = pTemp[XRIGHT|YBACK|ZTOP];
      pTemp[16].hv[0] += fabs(x1-x2);
      pTemp[17] = pTemp[XRIGHT|YBACK|ZBOTTOM];
      pTemp[17].hv[0] += fabs(x1-x2);

      //top
      pTemp[10] = pTemp[XRIGHT|YFRONT|ZTOP];
      pTemp[11] = pTemp[XLEFT|YFRONT|ZTOP];
      pTemp[8] = pTemp[11];
      pTemp[8].hv[1] -= fabs(y1-y2);
      pTemp[9] = pTemp[10];
      pTemp[9].hv[1] -= fabs(y1-y2);


      //bottom
      pTemp[14] = pTemp[XLEFT|YFRONT|ZBOTTOM];
      pTemp[15] = pTemp[XRIGHT|YFRONT|ZBOTTOM];
      pTemp[13].hv[0] = pTemp[14].hv[0];
      pTemp[13].hv[1] = pTemp[14].hv[1] + fabs(y1-y2);
      pTemp[12].hv[0] = pTemp[15].hv[0];
      pTemp[12].hv[1] = pTemp[15].hv[1] + fabs(y1-y2);

      // copy over and include xyz
      for (i = 0; i < 24; i++) {
         pt[i] = pTemp[dwRemap[i]];
         pt[i].xyz[0] = pp[dwPointRemap[i]].p[0];
         pt[i].xyz[1] = pp[dwPointRemap[i]].p[1];
         pt[i].xyz[2] = pp[dwPointRemap[i]].p[2];
      }

      // rotate
      ApplyTextureRotation (pt, 24);
   } // if texture

   DWORD dwNorm;
   // left side
   NewIDPart();
   dwNorm = DefNormal (1);
   NewQuad (
      NewVertex(dwIndex + dwPointRemap[0], dwNorm, pt ? (dwTextureIndex + 0) : 0),
      NewVertex(dwIndex + dwPointRemap[1], dwNorm, pt ? (dwTextureIndex + 1) : 0),
      NewVertex(dwIndex + dwPointRemap[2],dwNorm, pt ? (dwTextureIndex + 2) : 0),
      NewVertex(dwIndex + dwPointRemap[3],dwNorm, pt ? (dwTextureIndex + 3) : 0)
      );

   // right side
   NewIDPart();
   dwNorm = DefNormal (0);
   NewQuad (
      NewVertex(dwIndex + dwPointRemap[4],dwNorm, pt ? (dwTextureIndex + 4) : 0),
      NewVertex(dwIndex + dwPointRemap[5],dwNorm, pt ? (dwTextureIndex + 5) : 0),
      NewVertex(dwIndex + dwPointRemap[6], dwNorm, pt ? (dwTextureIndex + 6) : 0),
      NewVertex(dwIndex + dwPointRemap[7], dwNorm, pt ? (dwTextureIndex + 7) : 0)
      );
   
   // back side
   NewIDPart();
   dwNorm = DefNormal (2);
   NewQuad (
      NewVertex(dwIndex + dwPointRemap[8], dwNorm, pt ? (dwTextureIndex + 8) : 0),
      NewVertex(dwIndex + dwPointRemap[9], dwNorm, pt ? (dwTextureIndex + 9) : 0),
      NewVertex(dwIndex + dwPointRemap[10], dwNorm, pt ? (dwTextureIndex + 10) : 0),
      NewVertex(dwIndex + dwPointRemap[11], dwNorm, pt ? (dwTextureIndex + 11) : 0)
      );

   // font side
   NewIDPart();
   dwNorm = DefNormal (3);
   NewQuad (
      NewVertex(dwIndex + dwPointRemap[12], dwNorm, pt ? (dwTextureIndex + 12) : 0),
      NewVertex(dwIndex + dwPointRemap[13],dwNorm, pt ? (dwTextureIndex + 13) : 0),
      NewVertex(dwIndex + dwPointRemap[14],dwNorm, pt ? (dwTextureIndex + 14) : 0),
      NewVertex(dwIndex + dwPointRemap[15], dwNorm, pt ? (dwTextureIndex + 15) : 0)
      );

   // top side. Different order of texture map
   NewIDPart();
   dwNorm = DefNormal (4);
   NewQuad (
      NewVertex(dwIndex + dwPointRemap[16], dwNorm, pt ? (dwTextureIndex + 16) : 0),
      NewVertex(dwIndex + dwPointRemap[17],dwNorm, pt ? (dwTextureIndex + 17) : 0),
      NewVertex(dwIndex + dwPointRemap[18],dwNorm, pt ? (dwTextureIndex + 18) : 0),
      NewVertex(dwIndex + dwPointRemap[19], dwNorm, pt ? (dwTextureIndex + 19) : 0)
      );

   // bottom side
   NewIDPart();
   dwNorm = DefNormal (5);
   NewQuad (
      NewVertex(dwIndex + dwPointRemap[20], dwNorm, pt ? (dwTextureIndex + 20) : 0),
      NewVertex(dwIndex + dwPointRemap[21], dwNorm, pt ? (dwTextureIndex + 21) : 0),
      NewVertex(dwIndex + dwPointRemap[22],dwNorm, pt ? (dwTextureIndex + 22) : 0),
      NewVertex(dwIndex + dwPointRemap[23],dwNorm, pt ? (dwTextureIndex + 23) : 0)
      );

   // make sure temporary rotation off
   m_fUseTextMatrixTemp = FALSE;
}


/**********************************************************************************
CRenderSurface::ShapeSurface - Draws a mesh, which is a two dimensional array of
points, connected by surfaces. Or, like a piece of cloth over an area.

NOTE: Never have two points the same or the normal calculations may not work.

inputs
   DWORD    dwType -
      0 - ends are unconnected to one another
      1 - ends on left and right (across axis) are connected
      2 - ends on top and bottom (down axis) are connected
      3 - all ends connected
   DWORD    dwX, dwY - width and height
   PCPoint  paPoints - points. [dwY][dwX]. This is a pointer into the points
               list returned by NewPoints().
   DWORD    dwPointIndex - Index of points into local list
   PCPoint  paNormals - Same style as points. If this is NULL the normals will
      automatically be calculated by doing a cross-product. Unless, normals
      aren't needed, in which case they aren't calculataed This is a pointer
      into the normals list returned by NewNormals()
   DWORD    dwNormalIndex - Index of normals into local list.
   BOOL     fCanBackfaceCull - If set to FALSE, overriide backface culling
   PCPoint  pNormTop, pNormBottom, pNormLeft, pNormRight - If either of these aren't
            NULL then the normal on the top, bottom, left, or right edge is forced
            to this value. (Only checked if paNormals==NULL). Use this to ensure
            good normals for the top of the sphere. ASSUMED to be normalized already
   DWORD    dwRepeatH, dwRepeatV - If these are 0 then H and V are calculated by meters.
            However, if positive number then the H and/or V texture is repeated EXACTLY
            this many times across the surface. This is done so that a pattern will
            match up exactly with itself
returns
   none
*/
void CRenderSurface::ShapeSurface (DWORD dwType, DWORD dwX, DWORD dwY, PCPoint paPoints, DWORD dwPointIndex, PCPoint paNormals, DWORD dwNormalIndex,
                                   BOOL fCanBackfaceCull,
                                   PCPoint pNormTop, PCPoint pNormBottom, PCPoint pNormLeft, PCPoint pNormRight,
                                   DWORD dwRepeatH, DWORD dwRepeatV)
{
   DWORD dwXT, dwYT;

   // textures are one wider
   dwXT = dwX + 1;
   dwYT = dwY + 1;

   // up the minor item number
   NewIDPart();

   // calculate normals?
   DWORD x,y, dwAcross;
   DWORD x1, x2, y1, y2;   // points in mesh that will use
   dwAcross = dwX;
   dwNormalIndex = 0;
   if (m_fNeedNormals && !paNormals) {
      paNormals = NewNormals (TRUE, &dwNormalIndex, dwX * dwY);
      if (!paNormals)
         return;  // error

      // calculate normals with cross produce
      for (x = 0; x < dwX; x++) for (y = 0; y < dwY; y++) {
         // BUGFIX - if force normals apply
         if (pNormTop && (y == 0)) {
            paNormals[MESH(x,y)].Copy(pNormTop);
            continue;
         }
         if (pNormBottom && (y+1 == dwY)) {
            paNormals[MESH(x,y)].Copy(pNormBottom);
            continue;
         }
         if (pNormLeft && (x == 0)) {
            paNormals[MESH(x,y)].Copy(pNormLeft);
            continue;
         }
         if (pNormRight && (x+1 == dwX)) {
            paNormals[MESH(x,y)].Copy(pNormRight);
            continue;
         }

         // left/right
         // take entriest to either side
         x1 = (x + dwX - 1) % dwX;
         x2 = (x + 1) % dwX;
         if (!(dwType & 0x01)) {
            // not, connected on left right, so min/max with edge
            x1 = min(x, x1);
            x2 = max(x, x2);
         }

         // top bottom
         y1 = (y + dwY - 1) % dwY;
         y2 = (y + 1) % dwY;
         if (!(dwType & 0x02)) {
            // not, connected on left right, so min/max with edge
            y1 = min(y, y1);
            y2 = max(y, y2);
         }

         CPoint pEast, pNorth;
         pEast.Subtract (paPoints + MESH(x2,y), paPoints + MESH(x1, y));
         pNorth.Subtract (paPoints + MESH(x,y1), paPoints + MESH(x,y2));

         // cross-product
         paNormals[MESH(x,y)].CrossProd (&pEast,&pNorth);
         paNormals[MESH(x,y)].Normalize();
      }

   }

   // Add all the vertices
   PVERTEX pNewVertex;
   DWORD dwVertexIndex;
   pNewVertex = NewVertices (&dwVertexIndex, dwXT * dwYT);
   if (!pNewVertex)
      return;
   for (y = 0; y < dwYT; y++) for (x = 0; x < dwXT; x++) {
      pNewVertex[MESH2(x,y)].dwPoint = dwPointIndex + MESH(x%dwX,y%dwY);
   }

   if (m_fNeedNormals)
      for (y = 0; y < dwYT; y++) for (x = 0; x < dwXT; x++) {
         pNewVertex[MESH2(x,y)].dwNormal = dwNormalIndex + MESH(x%dwX,y%dwY);
      }

   // textures. Right now just using X and Z values
   if (m_fNeedTextures && m_DefRenderSurface.fUseTextureMap) {
      DWORD dwTextureIndex;

      PTEXTPOINT5 pt = NewTextures (&dwTextureIndex, dwXT * dwYT);
      if (!pt)
         goto skiptexture;

      // fill in the h component first
      fp fTotal, fInc;
      fInc = 0;
      for (x = 0; x < dwXT; x++) {
         // find the average distance between the left and rigth points,
         // exept the first row, which is 0
         fTotal = 0;
         if (x) for (y = 0; y < dwYT; y++)
            fTotal += DistancePointToPoint (&paPoints[MESH(x-1,y%dwY)],
               &paPoints[MESH(x%dwX,y%dwY)]);
         fTotal /= dwYT;
         fInc += fTotal;

         // write the total in
         for (y = 0; y < dwYT; y++)
            pt[MESH2(x,y)].hv[0] = fInc;

      }

      // fill in the v component
      fInc = 0;
      for (y = 0; y < dwYT; y++) {
         // find the average distance between the top and bottom points,
         // exept the first row, which is 0
         fTotal = 0;
         if (y) for (x = 0; x < dwXT; x++)
            fTotal += DistancePointToPoint (&paPoints[MESH(x%dwX,y-1)],
               &paPoints[MESH(x%dwX,y%dwY)]);
         fTotal /= dwXT;
         fInc += fTotal;

         // write the total in
         for (x = 0; x < dwXT; x++) {
            pt[MESH2(x,y)].hv[1] = fInc;

            // and fill in xyz while at it
            pt[MESH2(x,y)].xyz[0] = paPoints[MESH(x%dwX,y%dwY)].p[0];
            pt[MESH2(x,y)].xyz[1] = paPoints[MESH(x%dwX,y%dwY)].p[1];
            pt[MESH2(x,y)].xyz[2] = paPoints[MESH(x%dwX,y%dwY)].p[2];
         }

      }

      ApplyTextureRotation (pt, dwXT * dwYT);

      // potentially go through all the texutres and apply repeating pattern
      if (dwRepeatH || dwRepeatV)
         for (y = 0; y < dwYT; y++) for (x = 0; x < dwXT; x++) {
            if (dwRepeatH)
               pt[MESH2(x,y)].hv[0] = (fp)x / (fp)(dwXT-1) * (fp)dwRepeatH;
            if (dwRepeatV)
               pt[MESH2(x,y)].hv[1] = (fp)y / (fp)(dwYT-1) * (fp)dwRepeatV;
         } // x,y

      // write it into the vectors
      for (y = 0; y < dwYT; y++) for (x = 0; x < dwXT; x++)
         pNewVertex[MESH2(x,y)].dwTexture = dwTextureIndex + MESH2(x,y);
   }
skiptexture:

   // make sure temporary rotation off
   m_fUseTextMatrixTemp = FALSE;

   // create all the polygons
   for (x = 0; x < dwX; x++) for (y = 0; y < dwY; y++) {
      // if we're at the edge, but we're not connected, then don't do
      if ((x == (dwX-1)) && !(dwType & 0x01))
         continue;
      if ((y == (dwY-1)) && !(dwType & 0x02))
         continue;

      // next over
      DWORD x2, y2;
      x2 = (x + 1);// % dwX;
      y2 = (y + 1);// % dwY;

      // BUGFIX - Look for degenerate points
      DWORD adwVert[4];
      DWORD dwNum = 4;
      adwVert[0] = MESH2(x,y);
      adwVert[1] = MESH2(x2,y);
      adwVert[2] = MESH2(x2,y2);
      adwVert[3] = MESH2(x,y2);
      DWORD i;
      // BUGFIX - Was adding dwVertIndex above, moved to below
      for (i = 0; i < dwNum; i++) {
         PCPoint p1 = &paPoints[pNewVertex[adwVert[i]].dwPoint - dwPointIndex];
         PCPoint p2 = &paPoints[pNewVertex[adwVert[(i+1)%dwNum]].dwPoint - dwPointIndex];
         if (p1->AreClose(p2)) {
            // found degenerate point
            memmove (adwVert + i, adwVert + (i+1), sizeof(DWORD)*(dwNum-i-1));
            dwNum--;
            i--;
            continue;
         }
      } // i

      if (dwNum == 4)
         NewQuad (adwVert[0] + dwVertexIndex,
            adwVert[1] + dwVertexIndex,
            adwVert[2] + dwVertexIndex,
            adwVert[3] + dwVertexIndex
            , fCanBackfaceCull);
      else if (dwNum == 3)
         NewTriangle (adwVert[0] + dwVertexIndex,
            adwVert[1] + dwVertexIndex,
            adwVert[2] + dwVertexIndex,
            fCanBackfaceCull);
   }

   // done
}

/**********************************************************************************
CRenderSurface::ShapeEllipsoid - Draws a sphere of the given radius, centered on 0,0,0.

inputs
   fp      x, y, z - xyz size
returns
   none
*/
void CRenderSurface::ShapeEllipsoid (fp x, fp y, fp z)
{
   Push();

   // automatically scale, since this solves some normal-generation problems
   BOOL  m_fScale;
   m_fScale = ((x != y) || (x != z));
   if (m_fScale) {
      Scale (1.0, y / x, z / x);
   }
   // rotate to that epsilon-point is on top, not right in front
   Rotate (-PI/2, 1);

   // calculate the number of facets we'll need, but don't put in too many
   long facets = 4;
   fp fMax;
   fMax = max(x,y);
   fMax = max(fMax, z);
   while ((facets < 8) && (fMax / facets > this->m_fDetail))
      facets *= 2;
   // BUGFIX - Was facets < 64 - but had m_fDetail=0, so too many facets

   long  i,j;
   PCPoint  p, p2, pn;

   long  width, height;
   width = facets * 2;
   height = (width / 2) + 1;

   // allocate for the points and the normals
   PCPoint paPoints, paNormals;
   DWORD    dwPointIndex, dwNormalIndex;
   paPoints = NewPoints (&dwPointIndex, width * height);
   if (!paPoints)
      return;
   dwNormalIndex = 0;
   paNormals = m_fNeedNormals ? NewNormals(TRUE, &dwNormalIndex, width * height) : NULL;

   DWORD dwAcross;   // for macro
   dwAcross = (DWORD) width;

   // if there's symmedtry can optimize

   if (height & 0x01) {
      // odd number of values high. This is good
      for (i = 0; i < width; i++) for (j = 0; j <= (height/2); j++) {
         p = paPoints + MESH(i, j);
         fp   fRadK, fRadI;
         fRadK = PI/2 * (1.0-EPSILON) - j / (fp) (height-1) * PI * (1.0-EPSILON);
         fRadI = i / (fp) width * 2.0 * PI;
         p->p[0] = cos(fRadI) * cos(fRadK) * x;
         p->p[1] = sin(fRadK) * x;
         p->p[2] = -sin(fRadI) * cos(fRadK) * x;

         if (paNormals) {
            pn = paNormals + MESH(i,j);
            pn->p[0] = p->p[0] / x;
            pn->p[1] = p->p[1] / x;
            pn->p[2] = p->p[2] / x;
         }
      }

      for (i = 0; i < width; i++) for (j = 0; j < (height/2); j++) {
         p = paPoints + MESH(i, j);
         p2 = paPoints + MESH(i, height - 1 - j);
         p2->p[0] = p->p[0];
         p2->p[1] = -p->p[1];
         p2->p[2] = p->p[2];

         if (paNormals) {
            p = paNormals + MESH(i, j);
            p2 = paNormals + MESH(i, height - 1 - j);
            p2->p[0] = p->p[0];
            p2->p[1] = -p->p[1];
            p2->p[2] = p->p[2];
         }
      }
   }
   else {
      // even number. do the entire loop
      for (i = 0; i < width; i++) for (j = 0; j < height; j++) {
         p = paPoints + MESH(i, j);
         fp   fRadK, fRadI;
         fRadK = PI/2 * (1.0-EPSILON) - j / (fp) (height-1) * PI * (1.0-EPSILON);
         fRadI = i / (fp) width * 2.0 * PI;
         p->p[0] = cos(fRadI) * cos(fRadK) * x;
         p->p[1] = sin(fRadK) * x;
         p->p[2] = -sin(fRadI) * cos(fRadK) * x;

         if (paNormals) {
            pn = paNormals + MESH(i,j);
            pn->p[0] = p->p[0] / x;
            pn->p[1] = p->p[1] / x;
            pn->p[2] = p->p[2] / x;
         }
      }
   }

   ShapeSurface(1, width, height, paPoints, dwPointIndex, paNormals, dwNormalIndex);

   Pop ();
}


/********************************************************************
Bezier - does a bezier sub-division.

inputs
   PCPoint     paPointsIn - input points. [4]
   PCPoint     paPointsOut - filled with output [7]
   DWORD       dwDepth - depth. if 0, only 3 points filld in, 1 then 7, 2 then 15, etc.
returns
   DWORD - total number of points filled in
*/
DWORD Bezier (PCPoint paPointsIn, PCPoint paPointsOut, DWORD dwDepth)
{
   CPoint   pMid;
   CPoint   aNew[4];
   DWORD i;
   DWORD dwTotal = 0;

   if (!dwDepth) {
      memcpy (paPointsOut, paPointsIn, 3 * sizeof(CPoint));
      return 3;
   }

   // find mid-point
   for (i = 0; i < 4; i++)
      pMid.p[i] = (paPointsIn[1].p[i] + paPointsIn[2].p[i]) / 2;

   // start filling in
   aNew[0].Copy (paPointsIn + 0);
   for (i = 0; i < 4; i++)
      aNew[1].p[i] = (paPointsIn[0].p[i] + paPointsIn[1].p[i]) / 2;
   for (i = 0; i < 4; i++)
      aNew[2].p[i] = (paPointsIn[1].p[i] + pMid.p[i]) / 2;
   aNew[3].Copy (&pMid);
   dwTotal += Bezier (aNew, paPointsOut, dwDepth-1);

   // next batch
   aNew[0].Copy (&pMid);
   for (i = 0; i < 4; i++)
      aNew[1].p[i] = (pMid.p[i] + paPointsIn[2].p[i]) / 2;
   for (i = 0; i < 4; i++)
      aNew[2].p[i] = (paPointsIn[2].p[i] + paPointsIn[3].p[i]) / 2;
   aNew[3].Copy (paPointsIn + 3);
   dwTotal += Bezier (aNew, paPointsOut + dwTotal, dwDepth-1);

   return dwTotal;

}

/********************************************************************
BezierArray - Given a set of points, this generates a bezier curve (basically
   a larger set of points.)

inputs
   DWORD    dwNumIn - number of incoming points. if fLoop is FALSE,
               must be 4, 7, 10, 13, etc. If fLoop is TRUE, must be a multiple of 3.
   PCPoint  paPointsIn - incoming points. [dwNumIn]
   BOOL     fLoop - if TRUE the points represent a continuous loop, else
               a line segment
   DWORD    dwNumOut - number of points that want out. If fLoop is FALSE,
               this must be (n x (dwNumIn-1)) + 1, where n is an integer.
               Otherwise, NULL will be returned from the functon.
               If fLoop is TRUE, this must be (n x dwNumIn).
returns
   PCPoint  - new points, dwNumOut. THis must be freed with ESCFREE(). It can
            be NULL if the wrong number of bezier points are given
*/
PCPoint BezierArray (DWORD dwNumIn, PCPoint paPointsIn, BOOL fLoop, DWORD dwNumOut)
{
   PCPoint   paRet = NULL;

   // step one, if fLoop is TRUE, then call back into self so floop is not true
   if (fLoop) {
      // basically allocate temporary memory and add the starting bezier onto the end
      PCPoint paTemp;
      paTemp = (PCPoint) ESCMALLOC ((dwNumIn+1) * sizeof(CPoint));
      if (!paTemp) return NULL;
      memcpy (paTemp, paPointsIn, dwNumIn * sizeof(CPoint));
      paTemp[dwNumIn].Copy (paPointsIn);

      paRet = BezierArray (dwNumIn+1, paTemp, FALSE, dwNumOut+1);
      ESCFREE (paTemp);

      return paRet;
   }


   // make sure right number of points
   DWORD n, m;
   // make sure dwnumin right
   n = dwNumIn ? (dwNumIn - 1) / 3 : 0;
   if (!n)
      return NULL;
   if ((n*3)+1 != dwNumIn)
      return NULL;

   // make sure out it good
   m = dwNumOut ? (dwNumOut - 1) / 3 : 0;
   if (!m)
      return NULL;
   if ( (m*3 + 1) != dwNumOut)
      return NULL;

   // make sure out m, is a multiple of n
   DWORD s;
   s = m / n;
   if (s * n != m)
      return NULL;
   if (s & (s-1)) // should be a power of 2
      return NULL;

   // divide and conquer
   paRet = (PCPoint) ESCMALLOC(dwNumOut * sizeof(CPoint));
   if (!paRet)
      return NULL;
   DWORD i, dwTotal;

   // loop through this and split it into smaller chunks
   DWORD dwDepth;
   for (dwDepth = 0; s > 1; s /= 2, dwDepth++);
   for (i = dwTotal = 0; i < n; i++) {
      dwTotal += Bezier (paPointsIn + (i * 3), paRet + dwTotal, dwDepth);
   }

   // finish off with the last point
#ifdef _DEBUG
   if ((i*3) != dwNumIn-1)
      i = 1;
   if (dwTotal != dwNumOut-1)
      i = 1;
#endif
   paRet[dwTotal].Copy (paPointsIn + (i*3));

   // done
   return paRet;
}



/**********************************************************************************
CRenderSurface::ShapeSurfaceBezier - Does a surface using bezier patches.

inputs
   DWORD    dwType - ShapeSurface
   DWORD    dwX, dwY - width and height in points
               if (dwType & 0x01) then it's connected on the right side
                  and dwX = n*3. Else dwX = n*3+1
                  Where n is an integer
               if (dwType & 0x02) then it's connected on the top and bottom
                  dwY = n*3. Else dwY = n*3+1
   PCPoint  paPoints - Points. [dwY][dwX]. These must be in non-rotated space!
   BOOL     fCanBackfaceCull - If set to FALSE, overriide backface culling
*/
void CRenderSurface::ShapeSurfaceBezier (DWORD dwType, DWORD dwX, DWORD dwY, PCPoint paPoints,
                                         BOOL fCanBackfaceCull)
{
   long facets;
   long  i,j;
   PCPoint*ppaPoint = NULL;
   PCPoint paTemp = NULL;
   PCPoint ps;
   ps =NULL;

   // find the min and max on XYZ so that know how many facets we need
   CPoint pMin, pMax;
   PCPoint pTemp;
   fp fMax;
   pMin.Copy (paPoints);
   pMax.Copy (paPoints);
   for (i = 0; i < (int) (dwX * dwY); i++) {
      pTemp = paPoints + i;
      pMin.p[0] = min(pMin.p[0], pTemp->p[0]);
      pMin.p[1] = min(pMin.p[0], pTemp->p[0]);
      pMin.p[2] = min(pMin.p[0], pTemp->p[0]);
      pMax.p[0] = max(pMax.p[0], pTemp->p[0]);
      pMax.p[1] = max(pMax.p[0], pTemp->p[0]);
      pMax.p[2] = max(pMax.p[0], pTemp->p[0]);
   }
   fMax = max(pMax.p[0] - pMin.p[0], pMax.p[1] - pMin.p[1]);
   fMax = max(fMax, pMax.p[2] - pMin.p[2]);
   facets = (int)( fMax / m_fDetail);
   facets = min(64, facets);

   long  width, height;
   width = dwX;
   height = dwY;

   // because beziers must be a fixed size, keep on increasing until match
   DWORD dwBezX, dwBezY;
   for (dwBezX = dwX; dwBezX < max((DWORD)facets, (DWORD) width); ) {
      if (dwType & 0x01)
         dwBezX *= 2;
      else
         dwBezX = (dwBezX-1)*2+1;
   }
   for (dwBezY = dwY; dwBezY < max((DWORD)facets, (DWORD) height); ) {
      if (dwType & 0x02)
         dwBezY *= 2;
      else
         dwBezY = (dwBezY-1)*2+1;
   }

   // expand all the bezier curves along the X axis
   PCPoint pF;
   ppaPoint = (PCPoint*) ESCMALLOC(dwY * sizeof(PCPoint));
   memset (ppaPoint, 0, dwY * sizeof(PCPoint));
   if (!ppaPoint)
      return;
   for (i = 0; i < (long) dwY; i++) {
      pF = BezierArray (dwX, paPoints + (i * dwX), dwType & 0x01, dwBezX);
      if (!pF)
         goto done;
      ppaPoint[i] = pF;
   }

   // now, expand all these along the Y axis
   DWORD dwPointIndex;
   ps = (PCPoint) NewPoints (&dwPointIndex, dwBezX * dwBezY);
   if (!ps)
      goto done;
   paTemp = (PCPoint) ESCMALLOC (dwY * sizeof(CPoint));
   for (i = 0; i < (long) dwBezX; i++) {
      // fill pafTemp with the Y bezier points
      for (j = 0; j < (long) dwY; j++)
         paTemp[j].Copy (ppaPoint[j] + i);

      // expand
      PCPoint pBezRet;
      pBezRet = BezierArray (dwY, paTemp, dwType & 0x02, dwBezY);
      if (!pBezRet)
         goto done;  // failed for some reason

      // while we're here, copy to s
      for (j = 0; j < (long) dwBezY; j++)
         ps[i+j*dwBezX].Copy (pBezRet + j);

      // free  it up
      ESCFREE (pBezRet);
   }

   ShapeSurface (dwType, dwBezX, dwBezY, ps, dwPointIndex, NULL, 0, fCanBackfaceCull);

done:
   // fre memory
   if (paTemp)
      ESCFREE (paTemp);

   if (ppaPoint) {
      // BUGFIX - memory leak
      for (i = 0; i < (long) dwY; i++) {
         if (ppaPoint[i])
            ESCFREE (ppaPoint[i]);
      }
      ESCFREE (ppaPoint);
   }
}

/*******************************************************************
ShapeTeapot - Draws a teapot. The teapot uses a mesh, and if there's
   a bump map or color map will use that. The teapot is centered around
   the Y axis. It's base is at 0.
*/
void CRenderSurface::ShapeTeapot (void)
{

   DWORD x, y;
   // main part
   CPoint   apMain[13] = { 1.4000, 2.25000, 0, 1,
                        1.3375, 2.38125, 0, 1,
                        1.4375, 2.38125, 0, 1,
                        1.5000, 2.25000, 0, 1,
                        1.7500, 1.72500, 0, 1,
                        2.0000, 1.20000, 0, 1,
                        2.0000, 0.75000, 0, 1,
                        2.0000, 0.30000, 0, 1,
                        1.5000, 0.07500, 0, 1,
                        1.5000, 0.00000, 0, 1,
                        1.0000, 0.00000, 0, 1,
                        0.5000, 0.00000, 0, 1,
                        0.0000, 0.00000, 0, 1
                        };

   ShapeRotation (13, &apMain[0]);

   CPoint   apLid[7] = {   0.0, 3.00, 0, 1,
                        0.8, 3.00, 0, 1,
                        0.0, 2.70, 0, 1,
                        0.2, 2.55, 0, 1,
                        0.4, 2.40, 0, 1,
                        1.3, 2.40, 0, 1,
                        1.3, 2.25, 0, 1
                        };
   ShapeRotation (7, &apLid[0]);

   // handle
   CPoint   apHandleIn[7]= {  -1.60, 1.8750, 0, 1,
                           -2.30, 1.8750, 0, 1,
                           -2.70, 1.8750, 0, 1,
                           -2.70, 1.6500, 0, 1,
                           -2.70, 1.4250, 0, 1,
                           -2.50, 0.9750, 0, 1,
                           -2.00, 0.7500, 0, 1
                        };
   CPoint   apHandleOut[7]= { -1.50, 2.1000, 0, 1,
                           -2.50, 2.1000, 0, 1,
                           -3.00, 2.1000, 0, 1,
                           -3.00, 1.6500, 0, 1,
                           -3.00, 1.2000, 0, 1,
                           -2.65, 0.7875, 0, 1,
                           -1.90, 0.4500, 0, 1
                          }; 

   CPoint   apHandle[6][7];
   for (x = 0; x < 6; x++) {
      fp   *p;

      switch (x) {
      case 0:
      case 1:
      case 5:
         p = (fp*) apHandleIn;
         break;
      case 2:
      case 3:
      case 4:
         p = (fp*) apHandleOut;
      }

      memcpy (apHandle[x], p, sizeof(apHandleIn));

      // set Z
      fp   z;
      switch (x) {
      case 0:
      case 3:
         z = 0;
         break;
      case 1:
      case 2:
         z = 0.3;
         break;
      case 4:
      case 5:
         z = -0.3;
         break;
      }
      for (y = 0; y < 7; y++)
         apHandle[x][y].p[2] = z;
   }

   // draw it
   ShapeSurfaceBezier (2, 7, 6, &apHandle[0][0]);

   // Spout
   CPoint   apSpoutIn[7]=  {  1.700, 1.27500, 0, 1,
                           2.600, 1.27500, 0, 1,
                           2.300, 1.95000, 0, 1,
                           2.700, 2.25000, 0, 1,
                           2.800, 2.32500, 0, 1,
                           2.900, 2.32500, 0, 1,
                           2.800, 2.25000, 0, 1
                        };
   CPoint   apSpoutOut[7]=  { 1.700, 0.45000, 0, 1,
                           3.100, 0.67500, 0, 1,
                           2.400, 1.87500, 0, 1,
                           3.300, 2.25000, 0, 1,
                           3.525, 2.34375, 0, 1,
                           3.450, 2.36250, 0, 1,
                           3.200, 2.25000, 0, 1
                          }; 

   CPoint   apSpout[6][7];
   for (x = 0; x < 6; x++) {
      fp   *p;

      switch (x) {
      case 0:
      case 1:
      case 5:
         p = (fp*) apSpoutIn;
         break;
      case 2:
      case 3:
      case 4:
         p = (fp*) apSpoutOut;
      }

      memcpy (apSpout[x], p, sizeof(apSpoutIn));

      // set Z
      fp   z;
      switch (x) {
      case 0:
      case 3:
         z = 0;
         break;
      case 1:
      case 2:
         z = 0.3;
         break;
      case 4:
      case 5:
         z = -0.3;
         break;
      }
      for (y = 0; y < 7; y++)
         apSpout[x][y].p[2] = z;
   }

   // draw it
   ShapeSurfaceBezier (2, 7, 6, &apSpout[0][0]);
}


/*******************************************************************
ShapeRotation - Draws an rotation of a line (or bezier curve) around the Y-axis.

inputs
   DWORD    dwNum - number of points
   PCPoint  paPoints - Array of points, specified by dwNum. As a general
               rule, all X values should be >= 0. This code will automatically
               adjust any X values = 0 to EPSLION so that normal detection works
               z values are ignored. The points should also be from top-down
               so that culling dowesn't eliminate them.
   BOOL     fBezier - if TRUE, apply bezier curve to paPoints. To do a bezier
               dwNum must be a multiple of 3 + 1, ie 4, 7, 10, 13, etc.
   BOOL     fCanBackfaceCull  - If set to TRUE canbackface cull
*/
void CRenderSurface::ShapeRotation (DWORD dwNum, PCPoint paPoints, BOOL fBezier,
                                    BOOL fCanBackfaceCull)
{
   long facets = 8;
   long  i,j;

   // may already have done so
   // should see if bounding box will rule out immediately
   fp   fMaxY, fMinY, fMaxX;
   fMaxY = fMinY = paPoints[0].p[1];
   if (paPoints[0].p[0] == 0)
      paPoints[0].p[0] = EPSILON;
   fMaxX = paPoints[0].p[0];
   for (i = 1; i < (int) dwNum; i++) {
      fMaxY = max(fMaxY, paPoints[i].p[1]);
      fMinY = min(fMinY, paPoints[i].p[1]);
      if (paPoints[i].p[0] == 0)
         paPoints[i].p[0] = EPSILON;
      fMaxX = max(fMaxX, paPoints[i].p[0]);
   }

   facets = 4;
   while ((facets < 8) && (fMaxX / facets > m_fDetail))
      facets *= 2;
   // BUGFIX - Was facets < 64 - but had m_fDetail=0, so too many facets

   PCPoint s, p, paBez;

   long  width, height;
   width = facets * 2;
   height = dwNum;
   paBez = NULL;

   // if want a bezier curve
   if (fBezier) {
      if (m_fDetail)
         facets = (int) (fMaxY / m_fDetail);
      else
         facets = 16;
      facets = min(16, facets);
      // BUGFIX - Changed to 16 facets from 64

      // keep on increasing the size we want until it fits
      DWORD dwWant;
      for (dwWant = dwNum; dwWant < max((DWORD) facets, (DWORD) height); dwWant = (dwWant - 1)*2+1);
      paBez = BezierArray (dwNum, paPoints, FALSE, dwWant);

      // if we actually get a match then use this
      if (paBez) {
         height = dwNum = dwWant;
         paPoints = paBez;
      }
   }
   
   DWORD dwPointIndex;
   s = NewPoints (&dwPointIndex, width * height);
   if (!s)
      return;
   DWORD dwAcross;   // for macro
   dwAcross = (DWORD) width;

   // if there's symmedtry can optimize

   // even number. do the entire loop
   for (i = 0; i < width; i++) for (j = 0; j < height; j++) {
      fp   fx, fy;
      fx = paPoints[j].p[0];
      fy = paPoints[j].p[1];

      p = s + MESH(i, j);
      fp   fRad;
      fRad = i / (fp) width * 2.0 * PI;
      p->p[0] = sin(fRad) * fx;
      p->p[1] = fy;
      p->p[2] = cos(fRad) * fx;
   }

   ShapeSurface (1, width, height, s, dwPointIndex, NULL, 0, fCanBackfaceCull);

   // fre memory
   if (paBez)
      ESCFREE (paBez);

}


/*******************************************************************
CRenderSufrace::ShapeFunnel - Draws a Funnel, standing up and centered on the Y axis

inputs
   fp   fHeight - height. It's base is at 0.
   fp   fWidthBase - width at the base. Should be >= 0
   fp   fWidthTop - width at the top. SHould be >= 0
   BOOL     fCanBackfaceCull
*/
void CRenderSurface::ShapeFunnel (fp fHeight, fp fWidthBase, fp fWidthTop,
                                  BOOL fCanBackfaceCull)
{
   CPoint   p[2];
   p[0].p[0] = fWidthTop;
   p[0].p[1] = fHeight;
   p[1].p[0] = fWidthBase;
   p[1].p[1] = 0;

   ShapeRotation (2, &p[0], TRUE, fCanBackfaceCull);
}


/*******************************************************************
CRenderSufrace::ShapeQuad - Draws a quadrilateral.

inputs
   PCPoint  pUL, pUR, pLR, pLL - Points in the quad
   BOOL     fCanBackfaceCull - set to true if can backface cull
returns
   none
*/
void CRenderSurface::ShapeQuad (PCPoint pUL, PCPoint pUR, PCPoint pLR, PCPoint pLL,
                                BOOL fCanBackfaceCull)
{
   DWORD dwIndex;
   PCPoint pn = NewPoints (&dwIndex, 4);
   if (pn) {
      pn[0*2+0].Copy (pUL);
      pn[0*2+1].Copy (pUR);
      pn[1*2+1].Copy (pLR);
      pn[1*2+0].Copy (pLL);
      ShapeSurface (0, 2, 2, pn, dwIndex, NULL, 0, fCanBackfaceCull);
   }
}

/*******************************************************************
CRenderSufrace::ShapeQuad - Draws a quadrilateral.

inputs
   PCPoint  pUL, pUR, pLR, pLL - Points in the quad
   DWORD    dwNorm - Normal index. This must be valid if NewNormal() returns non-0
   int      iTextH, iTextV - Which dimensions 1..3 are used for H and V. If -1 to -3 then
               scale texture by -1
   BOOL     fCanBackfaceCull - set to true if can backface cull
returns
   none
*/
void CRenderSurface::ShapeQuadQuick (PCPoint pUL, PCPoint pUR, PCPoint pLR, PCPoint pLL,
      DWORD dwNorm, int iTextH, int iTextV, BOOL fCanBackfaceCull)
{
   PTEXTPOINT5 pText;
   PCPoint pPoint;
   DWORD dwTextIndex, dwPointIndex;
   CPoint apTemp[4]; // copy to temp so that when have NewPoint() dont muck up what pUL, etc. was pointing to
   apTemp[0].Copy (pUL);
   apTemp[1].Copy (pUR);
   apTemp[2].Copy (pLR);
   apTemp[3].Copy (pLL);
   pPoint = NewPoints (&dwPointIndex, 4);
   if (!pPoint)
      return;
   memcpy (pPoint, apTemp, sizeof(apTemp));

   pText = NewTextures (&dwTextIndex, 4);
   DWORD i;
   if (pText) {
      for (i = 0; i < 4; i++) {
         pText[i].hv[0] = pPoint[i].p[abs(iTextH)-1] * ((iTextH < 0) ? -1 : 1);
         pText[i].hv[1] = pPoint[i].p[abs(iTextV)-1] * ((iTextV < 0) ? -1 : 1);
         pText[i].xyz[0] = apTemp[i].p[0];
         pText[i].xyz[1] = apTemp[i].p[1];
         pText[i].xyz[2] = apTemp[i].p[2];
      }
      ApplyTextureRotation (pText, 4);
   }

   NewQuad (
      NewVertex (dwPointIndex + 0, dwNorm, pText ? (dwTextIndex+0) : 0),
      NewVertex (dwPointIndex + 1, dwNorm, pText ? (dwTextIndex+1) : 0),
      NewVertex (dwPointIndex + 2, dwNorm, pText ? (dwTextIndex+2) : 0),
      NewVertex (dwPointIndex + 3, dwNorm, pText ? (dwTextIndex+3) : 0),
      fCanBackfaceCull);
}


/**********************************************************************
CRenderSurface::GetTextureRotation - Fills in the matrix with the
proposed texture rotation.

inputs
   fp      m[2][2] - filled in with the texture rotation
returns
   PCMatrix - Returns a pointer to the matrix to be used for the xyz rotation. You
      can use this, but DONT change it
*/
PCMatrix CRenderSurface::GetTextureRotation (fp m[2][2])
{
   DWORD i, j;
   // copy it over
   if (m_fUseTextMatrixTemp) {
      m_fUseTextMatrixTemp = FALSE;
      for (i = 0; i < 2; i++)
         for (j = 0; j < 2; j++)
            m[j][i] = m_DefRenderSurface.afTextureMatrix[0][i] * m_aTextMatrixHVTemp[j][0] +
                        m_DefRenderSurface.afTextureMatrix[1][i] * m_aTextMatrixHVTemp[j][1];
            // BUGFIX - Below is the old code. I think was wrong order
            //m[j][i] = m_aTextMatrixHVTemp[0][i] * m_DefRenderSurface.afTextureMatrix[j][0] +
            //            m_aTextMatrixHVTemp[1][i] * m_DefRenderSurface.afTextureMatrix[j][1];
   }
   else
      for (i = 0; i < 2; i++)
         for (j = 0; j < 2; j++)
            m[i][j] = m_DefRenderSurface.afTextureMatrix[i][j];

   return (PCMatrix) (&m_DefRenderSurface.abTextureMatrix[0]);
}

/**********************************************************************
ApplyTextureRotation - Applies a rotation matrix to all the
points.

inputs
   PTEXTPOINT5        pat - Pointer to array
   DWORD                dwNum - Number
   fp               m[2][2] - matrix rotation for HV
   PCMatrix          pMatrixXYZ - Matrix for XYZ portion
returns
   none
*/
void ApplyTextureRotation (PTEXTPOINT5 pat, DWORD dwNum, fp m[2][2], PCMatrix pMatrixXYZ)
{
   // multiply by all the points
   TEXTUREPOINT tp;
   CPoint pTemp;
   DWORD i;
   for (i = 0; i < dwNum; i++) {
      tp.h = pat[i].hv[0];
      tp.v = pat[i].hv[1];
      pat[i].hv[0] = m[0][0] * tp.h + m[1][0] * tp.v;
      pat[i].hv[1] = m[0][1] * tp.h + m[1][1] * tp.v;

      if (pMatrixXYZ) {
         // and point
         pTemp.p[0] = pat[i].xyz[0];
         pTemp.p[1] = pat[i].xyz[1];
         pTemp.p[2] = pat[i].xyz[2];
         pTemp.p[3] = 1;
         pTemp.MultiplyLeft (pMatrixXYZ);
         pat[i].xyz[0] = pTemp.p[0];
         pat[i].xyz[1] = pTemp.p[1];
         pat[i].xyz[2] = pTemp.p[2];
      }
   }
}


/**********************************************************************
CRenderSurface::TempTextureRotation - Provide a temporary texture rotation
that affectgs the next objects drawn. It only affects the next shape drawn.

inputs
   fp      fAngle - Angle, in radians. Clockwise
returns
   none
*/
void CRenderSurface::TempTextureRotation (fp fAngle)
{
   if (!fAngle) {
      m_fUseTextMatrixTemp = FALSE;
      return;
   }

   CMatrix m;
   m.RotationZ (-fAngle);

   DWORD x,y;
   for (x = 0; x < 2; x++) for (y = 0; y < 2; y++)
      m_aTextMatrixHVTemp[x][y] = m.p[x][y];

   m_fUseTextMatrixTemp = TRUE;
}

/**********************************************************************
CRenderSurface::ApplyTextureRotation - Applies the texture rotation. This
is an internal function that is called by the shapes as they add
TEXTPOINT5s. It uses the rotation from the currently set matrix
in DefMaterial().

If m_fUseTextMatrixTemp, then it also multiplies in m_aTextMatrixHVTemp before clearing
the m_fUseTextMatrixTemp flag;

Of course, it can also be use outside CRenderSurface.

inputs
   PTEXTPOINT5        pat - Pointer to array
   DWORD                dwNum - Number
returns
   none
*/
void CRenderSurface::ApplyTextureRotation (PTEXTPOINT5 pat, DWORD dwNum)
{
   fp   m[2][2];
   PCMatrix pMatrixXYZ;

   pMatrixXYZ = GetTextureRotation (m);

   ::ApplyTextureRotation (pat, dwNum, m, pMatrixXYZ);

}


/**********************************************************************
CRenderSurface::ShapeZipper - Given two curves, pLine1 and pLine2, this
draws a flat surface between the two, using a zippering function to
draw triangles between the points.

pLine1 and pLine2 both run clockwise. pLine1 is in front (towards user). If
get the ordering wrong then triangles will end up being back-face culled.

inputs
   PHVXYZ         pLine1 - Pointer to list of HVXYZ structures. h & v values are ignored.
   DWORD          dwNum1 - Number of points in pLine1
   PHVXYZ         pLine2 - Pointer ot list of HVXYZ structures. h & v values are ignored.
   DWORD          dwNum2 - Number of points in pLine2. Not necessarily the same number as pLine1
   BOOL           fLooped - If TRUE, will connect the entire thing together.
   fp         fClipVMin, fClipVMax - If a triangle's pLineX.v < cClipVMin or > fClipVMax
                        then it's clipped and not drawn. If the triangle is within then
                        it's drawn. If fClipVMin = 0, and vClipVMax = 1 then don't bother testing.
   BOOL           fClipTriangle - if fClipTriangle (and clipping is requested) then the
                        individual triangles making up the zipper will be cut up into bits
                        to make the clipping right on the mark. Otherwise, entire triangle
                        is drawn.
retursn
   noen
*/
void CRenderSurface::ShapeZipper (PHVXYZ pLine1, DWORD dwNum1, PHVXYZ pLine2, DWORD dwNum2, BOOL fLooped,
                                  fp fClipVMin, fp fClipVMax, BOOL fClipTriangle)
{
   // dont bother with clip tiragle if min and max are beyond possible ranges
   BOOL fClipping = FALSE;
   if ((fClipVMin <= 0) && (fClipVMax >= 1))
      fClipTriangle = FALSE;
   else
      fClipping = TRUE;

   if (fClipping && fClipTriangle) {
      // redo the two lines, doing clipping
      DWORD i,j;
      CListFixed pNew1, pNew2;
      for (j = 0; j < 2; j++) {
         PHVXYZ pOrig;
         DWORD dwNum;
         PCListFixed pNew;
         pOrig = j ? pLine2 : pLine1;
         dwNum = j ? dwNum2 : dwNum1;
         pNew = j ? &pNew2 : &pNew1;
         pNew->Init (sizeof(HVXYZ));

         pNew->Required (dwNum);

         for (i = 0; i < dwNum; i++) {
            // get the current and next point
            PHVXYZ p1, p2;
            p1 = pOrig + (i % dwNum);
            p2 = pOrig + ((i+1) % dwNum);

            // always add the first point
            pNew->Add (p1);

            if (!fLooped && (i == dwNum-1))
               break;

            if (p1->v == p2->v)
               continue;   // they're the same, wont be any cut here

            // see if it crosses
            fp fAlphaOn, fAlphaOff;
            fAlphaOn = (fClipVMin - p1->v) / (p2->v - p1->v);
            fAlphaOff = (fClipVMax - p1->v) / (p2->v - p1->v);

            // which points
            fp afAlpha[2];
            afAlpha[0] = min(fAlphaOn, fAlphaOff);
            afAlpha[1] = max(fAlphaOn, fAlphaOff);
            DWORD k;
            for (k = 0; k < 2; k++) {
               if ((afAlpha[k] <= 0) || (afAlpha[k] >= 1))
                  continue;   // out of range

               HVXYZ hv;

               hv.h = afAlpha[k] * (p2->h - p1->h) + p1->h;
               hv.v = afAlpha[k] * (p2->v - p1->v) + p1->v;
               hv.p.p[0] = afAlpha[k] * (p2->p.p[0] - p1->p.p[0]) + p1->p.p[0];
               hv.p.p[1] = afAlpha[k] * (p2->p.p[1] - p1->p.p[1]) + p1->p.p[1];
               hv.p.p[2] = afAlpha[k] * (p2->p.p[2] - p1->p.p[2]) + p1->p.p[2];
               hv.p.p[3] = 1;

               pNew->Add (&hv);
            }
         }
      }

      ShapeZipper ((PHVXYZ)pNew1.Get(0), pNew1.Num(), (PHVXYZ)pNew2.Get(0), pNew2.Num(),
         fLooped, fClipVMin, fClipVMax, FALSE);
      return;
   }

   // new minor ID
   NewIDPart ();

   // how many points really?
   DWORD dw1, dw2;
   dw1 = dwNum1;
   dw2 = dwNum2;
   if (fLooped) {
      dw1++;
      dw2++;
   }

   // find the distance between two adjacent points so know H distance
   CPoint pt;
   fp fDistH;
   pt.Subtract (&pLine1[0].p, &pLine2[0].p);
   fDistH = pt.Length();

   // find the length of the right and left side, and then average for V
   fp fLen1, fLen2, fAverage;
   fLen1 = fLen2 = 0;


   // start at first point on each
   DWORD dwCur1, dwCur2;
   fp   fTextV1, fTextV2, fTextV1a, fTextV2b;
   dwCur1 = 0;
   dwCur2 = 0;
   fTextV1 = fTextV2 = 0;
   while (TRUE) {
      if ((dwCur1 >= dw1) && (dwCur2 >= dw2))
         break;   // done

      PHVXYZ p1, p2, p1a, p2b;
      p1 = pLine1 + (dwCur1 % dwNum1);
      p2 = pLine2 + (dwCur2 % dwNum2);
      p1a = pLine1 + ((dwCur1+1) % dwNum1);
      p2b = pLine2 + ((dwCur2+1) % dwNum2);

      // figure out which triangle to use, one of two choices.
      // both triangles use two points from dwCur1 to dwCur2
      // however, third point can eitehr be (dwCur1+1) (tri A) or (dwCur2+1) (triB).
      // use whichever one has the shortest length
      BOOL  fAOK, fBOK;
      fp fADist, fBDist, fLen;
      fAOK = fBOK = FALSE;
      //pt.Subtract (&p1->p, &p2->p);
      fADist = fBDist = 0; // should include pt.Length(), but makes no different to final comparison
      if (dwCur1+1 < dw1) {
         fAOK = TRUE;
         pt.Subtract (&p1a->p, &p2->p);
         fADist += pt.Length();
         pt.Subtract (&p1a->p, &p1->p);
         fLen = pt.Length();
         // fADist += fLen; BUGFIX - Really want the shortest distance between new end point
         fTextV1a = fTextV1 + fLen;
      }
      if (dwCur2+1 < dw2) {
         fBOK = TRUE;
         pt.Subtract (&p2b->p, &p1->p);
         fBDist += pt.Length();
         pt.Subtract (&p2b->p, &p2->p);
         fLen = pt.Length();
         // fBDist += fLen; BUGFIX - Really want the shortest distance between new end point
         fTextV2b = fTextV2 + fLen;
      }

      // take whichever triangle has the smallest perimiter
      BOOL fTakeA;
      if (fAOK && fBOK)
         fTakeA = (fADist < fBDist);
      else if (!fAOK && !fBOK)
         return;  // done
      else
         fTakeA = fAOK;

      // add the triangle
      PCPoint     pv1, pv2, pv3;
      pv1 = fTakeA ? &p1a->p : &p2b->p;
      pv2 = &p1->p;
      pv3 = &p2->p;

      // consider clipping this
      if (fClipping) {
         PHVXYZ px = fTakeA ? p1a : p2b;

         if ((p1->v+EPSILON < fClipVMin) || (p1->v-EPSILON > fClipVMax) ||
            (p2->v+EPSILON < fClipVMin) || (p2->v-EPSILON > fClipVMax) ||
            (px->v+EPSILON < fClipVMin) || (px->v-EPSILON > fClipVMax))
            goto nexttriangle;

      }

      DWORD dwNormal;
      dwNormal = NewNormal (pv1, pv2, pv3);
#if 0
      pv1->p[3] = pv2->p[3] = pv3->p[3] = 1;
      PCPoint pNormal;
      pNormal = NewNormals (TRUE, &dwNormal);
      if (pNormal) {
         pNormal->MakeANormal (pv1, pv2, pv3, TRUE);
      }
      else
         dwNormal = 0;
#endif // 0

      // rotate
      DWORD dwTextures;
      PTEXTPOINT5 ptp;
      dwTextures = 0;   // BUGFIX - So dont get undefined error
      // BUGFIX - If dont use texture map then dont create textures
      ptp =  m_DefRenderSurface.fUseTextureMap ? NewTextures (&dwTextures, 3) : NULL;
      if (ptp) {
         if (fLen1 == 0) {
            // need to calculate the lengths
            DWORD i;
            for (i = 0; i < dw1; i++) {
               pt.Subtract (&pLine1[i%dwNum1].p, &pLine1[(i+1)%dwNum1].p);
                  // BUGFIX - Was i without %dwNum - caused crash
               fLen1 += pt.Length();
            }
            for (i = 0; i < dw2; i++) {
               pt.Subtract (&pLine2[i%dwNum2].p, &pLine2[(i+1)%dwNum2].p);
                  // BUGFIX - Was i without %dwNum - caused crash
               fLen2 += pt.Length();
            }
            fLen1 = max(.001, fLen1);
            fLen2 = max(.001, fLen2);
            fAverage = (fLen1 + fLen2) / 2;
            fLen1 = fAverage / fLen1;
            fLen2 = fAverage / fLen2;
         }

         // BUGFIX - Put negatives in front of .h values since was reversed before
         ptp[0].hv[0] = fTakeA ? -fDistH : 0;
         ptp[0].hv[1] = fTakeA ? (fTextV1a * fLen1) : (fTextV2b * fLen2);
         ptp[1].hv[0] = -fDistH;
         ptp[1].hv[1] = fTextV1 * fLen1;
         ptp[2].hv[0] = 0;
         ptp[2].hv[1] = fTextV2 * fLen2;

         // and xyz texture
         DWORD j;
         for (j = 0; j < 3; j++) {
            ptp[0].xyz[j] = pv1->p[j];
            ptp[1].xyz[j] = pv2->p[j];
            ptp[2].xyz[j] = pv3->p[j];
         }

         ApplyTextureRotation (ptp, 3);
      }

      NewTriangle (
         NewVertex (NewPoint(pv1->p[0], pv1->p[1], pv1->p[2]), dwNormal, ptp ? (dwTextures+0) : 0),
         NewVertex (NewPoint(pv2->p[0], pv2->p[1], pv2->p[2]), dwNormal, ptp ? (dwTextures+1) : 0),
         NewVertex (NewPoint(pv3->p[0], pv3->p[1], pv3->p[2]), dwNormal, ptp ? (dwTextures+2) : 0),
         TRUE
         );
      // know that cant ever have both being false because tested that earlier

nexttriangle:
      if (fTakeA) {
         dwCur1++;   // advance
         fTextV1 = fTextV1a;
      }
      else {
         dwCur2++;   // advance
         fTextV2 = fTextV2b;
      }
   }
}




// BUGBUG - When calling shapesurface and pass in a cone (with width .01 of the max at one end),
// the normal calculation seems busted. This happens because subdivide quad into two
// triangles - one ends up being very thin, the other thicker. Causes problems with
// normal interpolations
