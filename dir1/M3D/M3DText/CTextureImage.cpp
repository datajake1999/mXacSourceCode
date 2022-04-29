/********************************************************************************
CTextureImage.cpp - Code for storing a texture image in an uncompressed format
that can be useful for painting and undo.

begun 4/4/03 by Mike Rozak
Copyright 2003 Mike Rozak. All rights reserved
*/

#include <windows.h>
#include <math.h>
#include "resource.h"
#include "escarpment.h"
#include "..\M3D.h"



/********************************************************************************
CTextureImage::Constructor and destructor
*/
CTextureImage::CTextureImage (void)
{
   memset (&m_gCode, 0, sizeof(m_gCode));
   memset (&m_gSub, 0, sizeof(m_gSub));
   m_fGlowScale = 1;
   m_fPixelLen = .001;
   m_fBumpHeight = .001;
   m_Material.InitFromID (MATERIAL_FLAT);
   m_cDefColor = RGB(0x80,0x80,0x80);
   m_dwWidth = m_dwHeight = 100;
   m_cTransColor = RGB(0xff,0xff,0xff);
   m_fCached = FALSE;
   m_dwTransDist = 25;
   m_fTransUse = FALSE;

   m_pbRGB = m_pbSpec = m_pbGlow = m_pbBump = m_pbTrans = NULL;

}

CTextureImage::~CTextureImage (void)
{
   // nothing for now
}

/********************************************************************************
CTextureImage::Clone - Creates a copy of this

returns
   PCTextureImage - clone. Must be freed by caller
*/
CTextureImage *CTextureImage::Clone (void)
{
   PCTextureImage pNew = new CTextureImage;
   if (!pNew)
      return NULL;

   pNew->m_gCode = m_gCode;
   pNew->m_gSub = m_gSub;
   pNew->m_fGlowScale = m_fGlowScale;
   pNew->m_fPixelLen = m_fPixelLen;
   pNew->m_fBumpHeight = m_fBumpHeight;
   memcpy (&pNew->m_Material, &m_Material, sizeof(m_Material));
   pNew->m_cDefColor = m_cDefColor;
   pNew->m_dwWidth = m_dwWidth;
   pNew->m_dwHeight = m_dwHeight;
   pNew->m_cTransColor = m_cTransColor;
   pNew->m_fCached = m_fCached;
   pNew->m_dwTransDist = m_dwTransDist;
   pNew->m_fTransUse = m_fTransUse;

   pNew->m_pbBump = pNew->m_pbGlow = pNew->m_pbRGB = pNew->m_pbSpec = pNew->m_pbTrans = NULL;

   DWORD dw;
   dw = pNew->m_dwWidth * pNew->m_dwHeight;

   if (m_pbRGB) {
      if (!pNew->m_memRGB.Required(dw * 3)) {
         delete pNew;
         return FALSE;
      }
      pNew->m_pbRGB = (PBYTE) pNew->m_memRGB.p;
      memcpy (pNew->m_pbRGB, m_pbRGB, dw * 3);
   }

   if (m_pbGlow) {
      if (!pNew->m_memGlow.Required(dw * 3)) {
         delete pNew;
         return FALSE;
      }
      pNew->m_pbGlow = (PBYTE) pNew->m_memGlow.p;
      memcpy (pNew->m_pbGlow, m_pbGlow, dw * 3);
   }

   if (m_pbSpec) {
      if (!pNew->m_memSpec.Required(dw * 2)) {
         delete pNew;
         return FALSE;
      }
      pNew->m_pbSpec = (PBYTE) pNew->m_memSpec.p;
      memcpy (pNew->m_pbSpec, m_pbSpec, dw * 2);
   }

   if (m_pbBump) {
      if (!pNew->m_memBump.Required(dw)) {
         delete pNew;
         return FALSE;
      }
      pNew->m_pbBump = (PBYTE) pNew->m_memBump.p;
      memcpy (pNew->m_pbBump, m_pbBump, dw);
   }

   if (m_pbTrans) {
      if (!pNew->m_memTrans.Required(dw)) {
         delete pNew;
         return FALSE;
      }
      pNew->m_pbTrans = (PBYTE) pNew->m_memTrans.p;
      memcpy (pNew->m_pbTrans, m_pbTrans, dw);
   }

   return pNew;
}

/********************************************************************************
CTextureImage::FromTexture - Give 2 guids representing a texture, this extracts
all the information from the texture into this. It gets as much as possible from
the cache - since this is unadulterated and not comrpressed to JPEG. The rest is
from the texture itself, by calling Render() and/or, if it's an image texture,
going directly in.

inputs
   GUID        *pgCode, *pgSub - GUIDs
returns
   BOOL - TRUE if succeded
*/
BOOL CTextureImage::FromTexture (DWORD dwRenderShard, const GUID *pgCode, const GUID *pgSub)
{
   TEXTINFO ti;
   PBYTE pComplete, pColorOnly, pbSpecMap, pTrans, pGlow;
   PSHORT pBumpMap;
   DWORD i;
   if (!TextureAlgoUnCache (dwRenderShard, pgCode, pgSub, &m_dwWidth, &m_dwHeight, &m_Material,
      &ti, &pComplete,
      &pColorOnly, &pbSpecMap, &pBumpMap, &pTrans, &pGlow))
      return FALSE;

   // BUGFIX - if no coloronly but there's a complete then use complete
   if (!pColorOnly && pComplete && !pbSpecMap && !pBumpMap && !pTrans && !pGlow)
      pColorOnly = pComplete;

   m_gCode = *pgCode;
   m_gSub = *pgSub;
   m_fGlowScale = ti.fGlowScale;
   m_fPixelLen = ti.fPixelLen;
   m_pbRGB = m_pbSpec = m_pbGlow = m_pbBump = m_pbTrans = NULL;
   m_fBumpHeight = .001;   // for now
   m_cDefColor = RGB(0x80,0x80,0x80);  // for now
   m_cTransColor = RGB(0x80, 0x80, 0x80); // for now
   m_fCached = FALSE;
   m_dwTransDist = 25;  // for now
   m_fTransUse = FALSE; // for now

   if (!m_dwWidth || !m_dwHeight)
      return FALSE;
   DWORD dw;
   dw = m_dwWidth * m_dwHeight;

   // try to load in the texture creator
   PCTextCreatorImageFile pCreator;
   pCreator = NULL;
   if (IsEqualGUID(*pgCode, GTEXTURECODE_ImageFile)) {
      // find it
      PCMMLNode2 pNode;
      WCHAR szMajor[128], szMinor[128], szName[128];
      if (!LibraryTextures(dwRenderShard, FALSE)->ItemNameFromGUID (pgCode, pgSub, szMajor, szMinor, szName)) {
         if (!LibraryTextures(dwRenderShard, TRUE)->ItemNameFromGUID (pgCode, pgSub, szMajor, szMinor, szName))
            goto definfo;  // cant find
      }
      pNode = LibraryTextures(dwRenderShard, FALSE)->ItemGet (szMajor, szMinor, szName);
      if (!pNode)
         pNode = LibraryTextures(dwRenderShard, TRUE)->ItemGet (szMajor, szMinor, szName);
      if (!pNode)
         goto definfo;

      pCreator = new CTextCreatorImageFile (dwRenderShard, 0);
      if (!pCreator)
         goto definfo;
      pCreator->MMLFrom (pNode);
      // NOTE: Not deleting pNode on purpose

      m_fBumpHeight = pCreator->m_fBumpHeight;
      m_cDefColor = pCreator->m_cDefColor;
      m_cTransColor = pCreator->m_cTransColor;
      m_dwTransDist = pCreator->m_dwTransDist;
      m_fTransUse = pCreator->m_fTransUse;
      m_fCached = pCreator->m_fCached;
   }

definfo:
   // is there rgb?
   if (pColorOnly) {
      if (!m_memRGB.Required (dw * 3))
         goto error;
      m_pbRGB = (PBYTE) m_memRGB.p;
      memcpy (m_pbRGB, pColorOnly, dw * 3);
   }

   if (pGlow) {
      if (!m_memGlow.Required (dw * 3))
         goto error;
      m_pbGlow = (PBYTE) m_memGlow.p;
      memcpy (m_pbGlow, pGlow, dw * 3);
   }

   // is there a specularity map?
   if (pbSpecMap) {
      if (!m_memSpec.Required (dw * 2))
         goto error;
      m_pbSpec = (PBYTE) m_memSpec.p;
      memcpy (m_pbSpec, pbSpecMap, dw * 2);
   }

   // is there a transparency map?
   if (pTrans) {
      if (!m_memTrans.Required (dw))
         goto error;
      m_pbTrans = (PBYTE) m_memTrans.p;
      memcpy (m_pbTrans, pTrans, dw);
   }

   // is there a bump map?
   if (pBumpMap) {
      if (!m_memBump.Required (dw))
         goto error;
      m_pbBump = (PBYTE) m_memBump.p;

      if (pCreator) {
         fp fScale = m_fBumpHeight / m_fPixelLen / 256.0;
         fp f;

         for (i = 0; i < dw; i++) {
            f = (fp) pBumpMap[i] / 256.0;
            f = f / fScale + (fp) 0x80;
            f = min(255,f);
            f = max(0,f);
            m_pbBump[i] = (BYTE) f;
         }
      }
      else {
         // find the min and max for the bump
         DWORD i;
         short iMin, iMax;
         iMin = iMax = pBumpMap[0];
         for (i = 1; i < dw; i++) {
            iMin = min(iMin, pBumpMap[i]);
            iMax = max(iMax, pBumpMap[i]);
         }

         int iDelta;
         iDelta = iMax - iMin;
         iDelta = max(iDelta, 1);
         m_fBumpHeight = (fp)iDelta / (fp)256.0 * m_fPixelLen;

         for (i = 0; i < dw; i++)
            m_pbBump[i] = (BYTE) (((fp) pBumpMap[i] - (fp) iMin) / (fp) iDelta * 255.0);

         i = 0;   // BUGFIX - To fix crash because of compiler error?
      }
   }


   if (pCreator)
      pCreator->Delete();
   return TRUE;

error:
   if (pCreator)
      pCreator->Delete();
   return FALSE;
}

/********************************************************************************
CTextureImage::ToTexture - Writes the texture out to the texture. This only works
if the texture is an image texture. In the processs of writing, this will also overwrite
any files used. It will even create new files if they dont exist
*/
BOOL CTextureImage::ToTexture (DWORD dwRenderShard)
{
   // make sure it's a texture image
   if (!IsEqualGUID(m_gCode, GTEXTURECODE_ImageFile))
      return FALSE;


   // save to image cache so dont lose data
   CImage Image, Image2;
   TEXTINFO ti;

   // fill in textinfo
   memset (&ti, 0, sizeof(ti));
   ti.dwMap = (m_pbSpec ? 0x01 : 0) | (m_pbBump ? 0x02 : 0) |
      (m_pbTrans ? 0x04 : 0) | (m_pbGlow ? 0x08 : 0);
   ti.fFloor = FALSE;
   ti.fGlowScale = m_fGlowScale;
   ti.fPixelLen = m_fPixelLen;

   // images
   PIMAGEPIXEL pip, pip2;
   Image.Init (m_dwWidth, m_dwHeight, m_cDefColor);
   pip = Image.Pixel(0,0);
   pip2 = pip;
   if (m_pbGlow) {
      Image2.Init (m_dwWidth, m_dwHeight);
      pip2 = Image2.Pixel(0,0);
   }
   fp fScale;
   fScale = m_fBumpHeight / m_fPixelLen / 256.0;
   DWORD i, dw;
   dw = m_dwWidth * m_dwHeight;
   for (i = 0; i < dw; i++, pip++, pip2++) {
      if (m_pbRGB) {
         pip->wRed = Gamma(m_pbRGB[i*3 + 0]);
         pip->wGreen = Gamma(m_pbRGB[i*3 + 1]);
         pip->wBlue = Gamma(m_pbRGB[i*3 + 2]);
      }

      if (m_pbGlow) {
         pip2->wRed = Gamma(m_pbGlow[i*3 + 0]);
         pip2->wGreen = Gamma(m_pbGlow[i*3 + 1]);
         pip2->wBlue = Gamma(m_pbGlow[i*3 + 2]);
      }

      if (m_pbSpec) {
         pip->dwID = m_Material.m_wSpecExponent // since no map for this
            | ((DWORD) Gamma(m_pbSpec[i*2 + 1]) << 16);
      }

      if (m_pbTrans) {
         pip->dwIDPart = (pip->dwIDPart & ~(0xff)) | m_pbTrans[i];
      }

      if (m_pbBump) {
         pip->fZ = ((fp) m_pbBump[i] - (fp)0x80) * fScale;
      }
   }

   if (!TextureAlgoCache (dwRenderShard, &m_gCode, &m_gSub, &Image, &Image2, &m_Material, &ti))
      return FALSE;


   // load in the MML version
   // find it
   PCMMLNode2 pNode;
   BOOL fUser;
   WCHAR szMajor[128], szMinor[128], szName[128];
   if (!LibraryTextures(dwRenderShard, FALSE)->ItemNameFromGUID (&m_gCode, &m_gSub, szMajor, szMinor, szName)) {
      if (!LibraryTextures(dwRenderShard, TRUE)->ItemNameFromGUID (&m_gCode, &m_gSub, szMajor, szMinor, szName))
         return FALSE;  // cant find
   }
   pNode = LibraryTextures(dwRenderShard, fUser = FALSE)->ItemGet (szMajor, szMinor, szName);
   if (!pNode)
      pNode = LibraryTextures(dwRenderShard, fUser = TRUE)->ItemGet (szMajor, szMinor, szName);
   if (!pNode)
      return FALSE;

   PCTextCreatorImageFile pCreator;
   pCreator = new CTextCreatorImageFile (dwRenderShard, 0);
   if (!pCreator)
      return FALSE;
   pCreator->MMLFrom (pNode);
   // NOTE: Not deleting pNode on purpose

   // save settings away
   pCreator->m_cDefColor = m_cDefColor;
   pCreator->m_cTransColor = m_cTransColor;
   pCreator->m_dwTransDist = m_dwTransDist;
   pCreator->m_fBumpHeight = m_fBumpHeight;
   pCreator->m_fTransUse = m_fTransUse;
   pCreator->m_fWidth = (fp)m_dwWidth * m_fPixelLen;
   pCreator->m_fCached = m_fCached;

   // figure out what files used
   PWSTR apszFile[TIFCACHE];
   memset (apszFile, 0 ,sizeof(apszFile));
   apszFile[0] = pCreator->m_szFile;
   apszFile[1] = pCreator->m_szTransFile;
   apszFile[2] = pCreator->m_szLucenFile;
   apszFile[3] = pCreator->m_szGlossFile;
   apszFile[4] = pCreator->m_szBumpFile;
   apszFile[5] = pCreator->m_szGlowFile;

   // if there are any duplicates then eliminate the duplicates
   DWORD j;
   for (i = 0; i < TIFCACHE; i++) {
      if (!(apszFile[i])[0])
         continue;

      for (j = 0; j < i; j++)
         if (!_wcsicmp(apszFile[i], apszFile[j]))
            break;
      if (j < i)
         (apszFile[i])[0] = 0;   // so dont get duplicate
   }

   // figure out the default choice for directory and type
   WCHAR szDir[256];
   DWORD adwType[TIFCACHE];  // 1 for bitmap, 2 for jpeg
   DWORD dwLen, dwDirType;
   szDir[0] = 0;
   memset (adwType, 0, sizeof(adwType));
   dwDirType = 0;
   for (i = 0; i < TIFCACHE; i++) {
      if (!(apszFile[i])[0])
         continue;

      // if not jpg or bmp then ignore
      dwLen = (DWORD)wcslen(apszFile[i]);
      if ((dwLen > 4) && !_wcsnicmp(apszFile[i] + (dwLen - 4), L".bmp", 4))
         adwType[i] = 1;
      else if ((dwLen > 4) && !_wcsnicmp(apszFile[i] + (dwLen - 4), L".jpg", 4))
         adwType[i] = 2;
      else {
         // not valid file
         (apszFile[i])[0] = 0;
         continue;
      }

      // found one
      if (!szDir[0]) {
         wcscpy (szDir, apszFile[i]);
         dwDirType = adwType[i]; // so remember a type
      }
      break;
   }
   if (!dwDirType)
      dwDirType = 2;
   if (!szDir[0])
      MultiByteToWideChar (CP_ACP, 0, gszAppDir, -1, szDir, sizeof(szDir)/2);
   else {
      // go back to a backslash
      dwLen = (DWORD)wcslen(szDir);
      for (i = dwLen - 1; i < dwLen; i--)
         if (szDir[i] == L'\\') {
            szDir[i+1] = 0;
            break;
         }
   }

   // GUID for this
   WCHAR szGUID[64];
   MMLBinaryToString ((PBYTE) &m_gSub, sizeof(m_gSub), szGUID);

   // write all the files out
   Image.Init (m_dwWidth, m_dwHeight);
   dw = m_dwWidth * m_dwHeight;
   for (i = 0; i < TIFCACHE; i++) {
      // start
      PIMAGEPIXEL pip = Image.Pixel(0,0);

      // do we need this one?
      switch (i) {
         case 0:  // rgb
            if (!m_pbRGB) {
               (apszFile[i])[0] = 0;   // since dont need
               continue;
            }
            for (j = 0; j < dw; j++, pip++) {
               pip->wRed = Gamma(m_pbRGB[j*3+0]);
               pip->wGreen = Gamma(m_pbRGB[j*3+1]);
               pip->wBlue = Gamma(m_pbRGB[j*3+2]);
            }
            break;

         case 1:  // transparency
            if (!m_pbTrans) {
               (apszFile[i])[0] = 0;   // since dont need
               continue;
            }
            for (j = 0; j < dw; j++, pip++)
               pip->wRed = pip->wGreen = pip->wBlue = Gamma(m_pbTrans[j]);
            break;

         case 2:  // translucency
            // dont support this right now
            (apszFile[i])[0] = 0;   // since dont need
            continue;
            break;

         case 3:  // glossiness
            if (!m_pbSpec) {
               (apszFile[i])[0] = 0;   // since dont need
               continue;
            }
            for (j = 0; j < dw; j++, pip++)
               pip->wRed = pip->wGreen = pip->wBlue = Gamma(m_pbSpec[j*2+1]);
            break;

         case 4:  // bump map
            if (!m_pbBump) {
               (apszFile[i])[0] = 0;   // since dont need
               continue;
            }
            for (j = 0; j < dw; j++, pip++)
               pip->wRed = pip->wGreen = pip->wBlue = Gamma(m_pbBump[j]);
            break;

         case 5:  // glow
            if (!m_pbGlow) {
               (apszFile[i])[0] = 0;   // since dont need
               continue;
            }
            for (j = 0; j < dw; j++, pip++) {
               pip->wRed = Gamma(m_pbGlow[j*3+0]);
               pip->wGreen = Gamma(m_pbGlow[j*3+1]);
               pip->wBlue = Gamma(m_pbGlow[j*3+2]);
            }
            break;
      }

      // create a name
      if (!(apszFile[i])[0]) {
         adwType[i] = dwDirType;

         wcscpy (apszFile[i], szDir);
         wcscat (apszFile[i], szGUID);
         WCHAR szChar[2];
         szChar[0] = L'a' + i;
         szChar[1] = 0;
         wcscat (apszFile[i], szChar);
         wcscat (apszFile[i], (adwType[i] == 1) ? L".bmp" : L".jpg");
      }

      // create an hbitmap from this
      HBITMAP hBit;
      HDC hDC;
      hDC = GetDC (GetDesktopWindow ());
      hBit = Image.ToBitmap (hDC);
      ReleaseDC (GetDesktopWindow(), hDC);

      char szTemp[256];
      WideCharToMultiByte (CP_ACP, 0, apszFile[i], -1, szTemp, sizeof(szTemp), 0,0);
      if (adwType[i] == 1)
         BitmapSave (hBit, szTemp);
      else
         BitmapToJPegNoMegaFile (hBit, szTemp);
      if (hBit)
         DeleteObject (hBit);
   }

   // tell to reload
   pCreator->LoadImageFromDisk();

   // save...
   pNode = pCreator->MMLTo();
   pCreator->Delete();
   if (pNode)
      AttachDateTimeToMML(pNode);

   LibraryTextures(dwRenderShard, fUser)->ItemRemove (szMajor, szMinor, szName);
   LibraryTextures(dwRenderShard, fUser)->ItemAdd (szMajor, szMinor, szName,
      &m_gCode, &m_gSub, pNode);
   // NOTE: Specifically NOT deleting pNode

   // clean out the thumbnail just in case
   ThumbnailGet()->ThumbnailRemove (&m_gCode, &m_gSub);

   // note: Dont bother removing from algocache here because did so
   //TextureAlgoCacheRemove (&gCode, &gSub);
   TextureCacheDelete (dwRenderShard, &m_gCode, &m_gSub);

   // refresh the world in case suing
   PCWorldSocket pWorld;
   pWorld = WorldGet(dwRenderShard, NULL);
   if (pWorld)
      pWorld->NotifySockets (WORLDC_SURFACECHANGED, NULL);

   return TRUE;
}



// BUGBUG - may eventually want the thumbnail cache to be a megafile

// BUGBUG - may eventually want the texture cache to be a megafile
