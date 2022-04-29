/*************************************************************
FontCache.cpp - Caches fonts & font formatting information.

begun 3/18/00 by Mike Rozak
Copyright 2000 Mike Rozak. All rights reserved
*/

#include "mymalloc.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include "tools.h"
#include "textwrap.h"
#include "fontcache.h"
#include "resleak.h"

// structures & defines
typedef struct {
   LOGFONT        lf;
   HFONT          hFont;
   DWORD          dwCount; // use count
} GFONTCACHE, *PGFONTCACHE;

static WORD gwScale = 0x100;
CListFixed gListFontCache;   // list of font GFONTCACHE
static BOOL gfFontCacheInit = FALSE;   // TRUE if initialed


/************************************************************************
MyCreateFontIndirect - goes through gfListFontCache.

inputs
   LOGFONT     *pl
returns
   HFONT
*/
// BUGFIX - Was creating way too many fonts
HFONT MyCreateFontIndirect (LOGFONT *pl)
{
   // initialize if not
   if (!gfFontCacheInit) {
      gListFontCache.Init (sizeof(GFONTCACHE));
      gfFontCacheInit = TRUE;
   }

   // find
   DWORD i;
   GFONTCACHE *pTest;
   for (i = 0; i < gListFontCache.Num(); i++) {
      pTest = (GFONTCACHE*) gListFontCache.Get(i);
      if (!memcmp (&(pTest->lf), pl, sizeof(LOGFONT))) {
         pTest->dwCount++;
         return pTest->hFont;
      }
   }

   // if get here couldn't find, so make
   GFONTCACHE n;
   memset (&n, 0, sizeof(n));
   n.hFont = CreateFontIndirect (pl);
#ifdef _DEBUG
      OutputDebugString ("CreateFontIndirect - FontCache\r\n");
#endif
   if (!n.hFont)
      return NULL;
   memcpy (&n.lf, pl, sizeof(LOGFONT));
   n.dwCount = 1;

   gListFontCache.Add (&n);

   return n.hFont;
}


/************************************************************************
MyDeleteObject - For deleting hFonts creating this way

inputs
   HFONT    hFont
returns
*/
void MyDeleteObject (HFONT hFont)
{
   // initialize if not
   if (!gfFontCacheInit)
      return;  // nothing to delete

   // find
   DWORD i;
   GFONTCACHE *pTest;
   for (i = 0; i < gListFontCache.Num(); i++) {
      pTest = (GFONTCACHE*) gListFontCache.Get(i);
      if (pTest->hFont != hFont)
         continue;

      // decrease the counter
      pTest->dwCount--;

      // if no count delete
      if (!pTest->dwCount) {
         // BUGFIX - Forgot to actually delete the font that was cached
         DeleteObject (pTest->hFont);

         gListFontCache.Remove (i);
      }
      return;  // all done
   }

#ifdef _DEBUG
   OutputDebugString ("MyDeleteObject - Err\r\n");
#endif
}

/************************************************************************
Constructor & destructor
*/
CFontCache::CFontCache (void)
{
   m_listHFONT.Init (sizeof(HFONT));
   m_listLOGFONT.Init (sizeof(LOGFONT));
}


CFontCache::~CFontCache (void)
{
   // clear
   Clear ();
}



/************************************************************************
Clear - Clears the font cache of all objects. This means pointers previously
returned are invalid, and thant HFONTs are invalid
*/
void CFontCache::Clear (void)
{
   DWORD i;
   HFONT *phFont;

   for (i = 0; i < m_listHFONT.Num(); i++) {
      phFont = (HFONT*) m_listHFONT.Get (i);

      MyDeleteObject (*phFont);
   }

   // wipe out lists
   m_listHFONT.Clear ();
   m_listLOGFONT.Clear ();
   m_listTWFONTINFO.Clear();
}


/****************************************************************************
Need - Call this whenever a new font is needed. It looks through a cache and sees
   if it's already created. If it is, it doesn't bother recreating it.

inputs
   HDC         hDC - HDC
   TWFONTINFO  *pfi - Fill in everything except hFont, iAbove, and iBelow
   int         iPointSize - Point size font looking for
   DWORD       dwFlags - One or more ofFCFLAG_XXX
   PCSTR       pszFace - Unicode string for font face
returns
   TWFONTINFO * - Pointer to a permenantly available strucutre containaing the font
         info. The structure is available until CFontCache::Clear is called. Evertyhing
         will be identical to pfi, except hFont, iAbove, and iBelow are filled in

NOTE: If iLineSpacePar or iLineSpaceWrap are negative, they're assumed to be a percentage
of the total font height. So -40 for a 13-pixel high font = 40 * 13 / 100 pixels high
*/
TWFONTINFO *CFontCache::Need (HDC hDC, TWFONTINFO *pfi, int iPointSize, DWORD dwFlags, PCWSTR pszFace)
{
   // create the logfont
   LOGFONT  lf;
   memset (&lf, 0, sizeof(lf));
   lf.lfHeight = -MulDiv(iPointSize, EscGetDeviceCaps(hDC, LOGPIXELSY), 72); 
   lf.lfHeight = lf.lfHeight * (int) gwScale / 0x100; // scale according to global setting
   lf.lfCharSet = DEFAULT_CHARSET;
   lf.lfWeight = FW_NORMAL;   // BUGFIX - Adjust the weight of all fonts to normal
   lf.lfPitchAndFamily = FF_SWISS | VARIABLE_PITCH;
   WideCharToMultiByte (CP_ACP, 0, pszFace, -1, lf.lfFaceName, sizeof(lf.lfFaceName), 0, 0);
   if (dwFlags & FCFLAG_BOLD)
      lf.lfWeight = FW_BOLD;
   if (dwFlags & FCFLAG_ITALIC)
      lf.lfItalic = TRUE;
   if (dwFlags & FCFLAG_UNDERLINE)
      lf.lfUnderline = TRUE;
   if (dwFlags & FCFLAG_STRIKEOUT)
      lf.lfStrikeOut = TRUE;

   // see if there's a font that matches
   DWORD i;
   LOGFONT  *plf;
   for (i = 0; i < m_listLOGFONT.Num(); i++) {
      plf = (LOGFONT*) m_listLOGFONT.Get(i);
      if (!memcmp (plf, &lf, sizeof(lf)))
         break;
   }
   if (i >= m_listLOGFONT.Num()) {
      // couldn't fint
      HFONT hf;
      hf = MyCreateFontIndirect (&lf);
      if (!hf)
         return NULL;
      m_listLOGFONT.Add (&lf);
      m_listHFONT.Add (&hf);

      i = m_listLOGFONT.Num() - 1;
   }

   // fill in TWFNOTINFO
   TWFONTINFO fi;
   fi = *pfi;
   fi.hFont = *((HFONT*) m_listHFONT.Get(i));

   // see if can find a TWFONTINFO to match
   HFONT hOld;
   hOld = (HFONT) SelectObject (hDC, fi.hFont);
   TEXTMETRIC  tm;
   int   iCap;
   iCap = EscGetDeviceCaps (hDC, LOGPIXELSY);
   GetTextMetrics (hDC, &tm);
   SelectObject (hDC, hOld);
   // BUGFIX - so it's centered better
   fi.iAbove = tm.tmAscent + tm.tmExternalLeading - tm.tmInternalLeading;
   fi.iBelow = tm.tmDescent + tm.tmInternalLeading;
//   fi.iAbove = tm.tmAscent + tm.tmExternalLeading;
//   fi.iBelow = tm.tmDescent; // + tm.tmInternalLeading;
   //fi.iAbove = (fi.iAbove * 72) / iCap;
   //fi.iBelow = (fi.iBelow * 72) / iCap;
   int   iTotal;
   iTotal = fi.iAbove + fi.iBelow;
   if (fi.iLineSpacePar < 0)
      fi.iLineSpacePar = -fi.iLineSpacePar * iTotal / 100;
   if (fi.iLineSpaceWrap < 0)
      fi.iLineSpaceWrap = -fi.iLineSpaceWrap * iTotal / 100;

   // see if this matches
   TWFONTINFO* p;
   for (i = 0; i < m_listTWFONTINFO.Num(); i++) {
      p = (TWFONTINFO*) m_listTWFONTINFO.Get(i);

      if (!memcmp(p, &fi, sizeof(fi))) {
         return p;
      }
   }

   // else, cant find so add
   m_listTWFONTINFO.Add (&fi, sizeof(fi));
   return (TWFONTINFO*) m_listTWFONTINFO.Get(m_listTWFONTINFO.Num()-1);
}


/****************************************************************************
EscFontScaleSet - Sets the font scaling for ALL fonts created by the system.
   Use this to globally scale fonts for the user. Make sure to call this
   before creating the page.

inputs
   WORD     wScale - 0x100 for 1x normal. 0x200 for 2x normal. 0x80 for 1/2x normal, etc.
returns
   none
*/
void EscFontScaleSet (WORD wScale)
{
   gwScale = wScale;
}

/****************************************************************************
EscFontScaleGet - Gets the font scaling. See EscFontScaleSet

inputs
   none
retursn
   WORD - scale
*/
WORD EscFontScaleGet (void)
{
   return gwScale;
}