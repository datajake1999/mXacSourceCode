/***************************************************************8
TextWrap.cpp - Text wrapping functins. For text & graphics/objects.

begun 3/16/2000 by Mike Rozak
Copyright 2000 Mike Rozak. All rights reserved
*/


#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include "textwrap.h"
#include "resleak.h"

/* internal defines and structures */
typedef struct {
   RECT     r;          // top Y range
                        // bottom Y range. r.bottom = next->r.top;
                        // left/right range
} CLIPINFO, *PCLIPINFO;

typedef struct {
   PTWOBJECTINFO  pObject; // to be added tot he queue at the next chance
} CLIPQUEUE, *PCLIPQUEUE;

typedef struct {
   DWORD       dwID;    // always set to 1 to indicate it's a line text elem
   TWTEXTELEM  te;      // text elem structure, excluding the string
   DWORD       dwIndexStart;  // character # for the start of the string
   DWORD       dwIndexCount;  // number of characters in the strin
   int         iWidth;  // width
   int         iAbove;  // above baseline
   int         iBelow;  // below baseline
} LINETEXTELEM, *PLINETEXTELEM;

typedef struct {
   DWORD       dwID;    // always set to 2, to identify LINEOBJECTPOSN
   TWOBJECTPOSN   op;   // object position
} LINEOBJECTPOSN, *PLINEOBJECTPOSN;

typedef struct {
   DWORD       dwID;    // always set to 3
   int         iWidth;  // width in pixels
} LINETAB, *PLINETAB;

class CTextLine{
private:
   CListVariable  m_lElem;   // either LINETEXTELEM or LINEOBJECTPOSN or LINETAB
   int      m_iHeightCenter; // maximum height of centered object
   int      m_iHeightTop;    // maximum height of top-aligned object
   int      m_iHeightBottom; // maximum height of bottom-aligned object
   int      m_iHeightAbove;  // maximum height of text above baseline
   int      m_iHeightBelow;  // maximum height to text below baseline
   int      m_iWidth;        // total width (excluding backgroun object)
   int      m_iWidthBack;     // total width (including the background object)
   DWORD    m_dwAlign;       // -1 for unkown, 0 for left, 1 for center, 2 for right
   DWORD    m_dwLastAlign;    // last alignment used. Just in case no alignment specified
   BOOL     m_fHasCR;         // has an ending CR

public:
   ESCNEWDELETE;

   CTextLine (void);
   ~CTextLine (void);

   int      m_iLineSpacePar;  // line spacing for a paragraph
   int      m_iLineSpaceWrap; // line spacing for a word wrap
   int      m_iIndentLeftFirst;  // left indent on first line
   int      m_iIndentLeftSecond; // left indent on 2nd+ line
   int      m_iIndentRight;   // indent on the right

   int      Width(void);   // returns width
   int      Height(void);  // returns the height
   int      HypWidth (int iWidthExtra);   // hypothesize a width based on extra text
   int      HypWidthTab (int iLeft, TWFONTINFO *pfi);     // hypothesize a tab
   int      HypHeight (TWFONTINFO *pfi);  // hypothesize a new height based on the font info
   int      HypHeight (TWOBJECTINFO *poi);   // hypothesize a new height based on the object info

   BOOL     AddText (TWFONTINFO *pfi, int iWidthPixel, DWORD dwIndexStart, DWORD dwCount);  // add text to the list
   BOOL     AddObject (TWOBJECTINFO *poi);   // add an object to the list
   BOOL     AddTab (int iLeft, TWFONTINFO *pfi);  // adds a tab. iLeft is left side of text - needed for proper tab
   BOOL     AddCR (TWFONTINFO *pfi);
   void     Clear (void);     // clear out line info
   BOOL     Render (PWSTR pszText, RECT rPosn, CListVariable *plTextElem, CListFixed *plObjectPosn,
      int *piBottomY);
               // writes the line info in. r.bottom ignored.
   BOOL     HasData (void);
};

typedef CTextLine * PCTextLine;

/* globals */
typedef struct {
   //given by caller
   HDC            hDC;     // DC to work on
   int            iWidth;  // width of region
   PWSTR          pszText; // text
   DWORD          dwTextLen;  // number of characters
   PTWFONTINFO*   ppfi;    // Pointer to an array of PTWFONTINFO
   PTWOBJECTINFO  poi;     // array of TWOBJECTINFO
   DWORD          dwNumOI; // number of object info
   CListVariable  *pListTextElem;   // filled in with TWTEXTELEM
   CListFixed     *pListObjectPosn; // filled in with TWOBJECTPOSN

   // databases
   CTextLine      TextLine;   // text line object
   int            iCurY;      // current top Y position
   CListFixed     ClipInfo;   // CLIPINFO list
   CListFixed     ClipQueue;  // CLIPQUEUE list
   DWORD          dwNextChar;  // next character (index) to look at
   DWORD          dwNextObject;// next object (index) to look at
   BOOL           fNewLine;    // if TRUE then we're working on a newline
} TEXTWRAP, *PTEXTWRAP;



BOOL RenderObject (TEXTWRAP *ptw, PTWOBJECTINFO poi);

/************************************************************
ClipWidth - Given the height of an object, and it's top, this provides
   the left/right boundaries, and the width possible.

inputs
   TEXTWRAP *ptw - text wrap
   int      iHeight - height of object
   int      iTopY - Y value of top line
   int      *piLeft - filled with left boundary. NULL if dont care
   int      *piRight - filled with right boundary. NULL if dont care
returns
   int - *piRight - *piLeft
*/
int ClipWidth (TEXTWRAP *ptw, int iHeight, int iTopY, int *piLeft, int *piRight)
{
   int   iLeft, iRight;
   iLeft = 0;
   iRight = 1000000;

   // loop
   DWORD i;
   CLIPINFO *pci;
   for (i = 0; i < ptw->ClipInfo.Num(); i++) {
      pci = (PCLIPINFO) ptw->ClipInfo.Get(i);

      // if iTopY is below the bottom of the clip info then continue
      // since it doesn't affect
      if (iTopY >= pci->r.bottom)
         continue;

      // if iTopY + iHeight < the top then stop, since clipping is sorted, and doesn't affect
      if ((iTopY + iHeight) < pci->r.top)
         break;

      // else, it's affected
      iLeft = max(iLeft, pci->r.left);
      iRight = min(iRight, pci->r.right);
   }

   // done
   if (piLeft)
      *piLeft = iLeft;
   if (piRight)
      *piRight = iRight;
   return iRight - iLeft;
}

/************************************************************
ClipText - Given the height of text, and it's top, this provides
   the left/right boundaries, and the width possible.

inputs
   TEXTWRAP *ptw - text wrap
   int      iHeight - height of object
   int      iTopY - Y value of top line
   int      iIndentLeft - left indent of text in pixels
   int      iIndentRight - right indent of text in pixels
   int      *piLeft - filled with left boundary. NULL if dont care
   int      *piRight - filled with right boundary. NULL if dont care
returns
   int - *piRight - *piLeft
*/
int ClipText (TEXTWRAP *ptw, int iHeight, int iTopY,
               int iIndentLeft, int iIndentRight, int *piLeft, int *piRight)
{
   int   iLeft, iRight;

   ClipWidth (ptw, iHeight, iTopY, &iLeft, &iRight);
   iLeft = max(iLeft, iIndentLeft);
   iRight = min(iRight, ptw->iWidth - iIndentRight);


   // done
   if (piLeft)
      *piLeft = iLeft;
   if (piRight)
      *piRight = iRight;
   return iRight - iLeft;
}


/************************************************************
ClipPrune - Prune the clip tree based on the top-line. IE: Eliminate
   all clipping regions above the top line since we never have to
   worry about them again.
inputs
   TEXTWRAP    *ptw - text wrap
*/
void ClipPrune (TEXTWRAP *ptw)
{
   CLIPINFO *pci;

   while (TRUE) {
      pci = (CLIPINFO*) ptw->ClipInfo.Get(0);
      if (!pci || (pci->r.bottom > ptw->iCurY))
         break;

      // delete
      ptw->ClipInfo.Remove(0);
   }
}


/************************************************************
ClipSplit - Split a clipping section if it its bisected by iY.

inputs
   TEXTWRAP    *ptw - info
   int         iY - to split
*/
void ClipSplit (TEXTWRAP *ptw, int iY)
{
   DWORD i;
   CLIPINFO *pci;
   for (i = 0; i < ptw->ClipInfo.Num(); i++) {
      pci = (PCLIPINFO) ptw->ClipInfo.Get(i);

      // continue if not far enough
      if (iY >= pci->r.bottom)
         continue;

      // stop if gone too far
      if (iY <= pci->r.top)
         break;

      // this is the candidate for splitting. It's within the two borders
      CLIPINFO ci;
      ci = *pci;
      pci->r.top = iY;
      ci.r.bottom = iY;

      ptw->ClipInfo.Insert (i, &ci);
      break;
   }
}


/************************************************************
ClipAddRegion - Adds a region to clip, either to the left or right.
   Note: This does not actually check to see if it's fit!.

inputs
   TEXTWRAP    *ptw - info
   int         iWidth - width of region
   int         iHeight - height of region
   int         iTopY - top Y
   BOOL        fLeft - if TRUE stick on the left, else right
   RECT        *pr - Filled in with the new location
returns
   none
*/
void ClipAddRegion (TEXTWRAP *ptw, int iWidth, int iHeight, int iTopY,
                    BOOL fLeft, RECT *pr)
{
   // figure out where it will be positioned
   int   iPosLeft, iPosRight, iEdgeLeft, iEdgeRight;
   ClipWidth (ptw, iHeight, iTopY, &iPosLeft, &iPosRight);
   if (fLeft) {
      iEdgeLeft = iPosLeft;
      iEdgeRight = iEdgeLeft + iWidth;
   }
   else {
      iEdgeRight = iPosRight;
      iEdgeLeft = iEdgeRight - iWidth;
   }

   // split the datavase
   int   iBottom = iTopY + iHeight;
   ClipSplit (ptw, iTopY);
   ClipSplit (ptw, iBottom);

   // find all the chunks that fit within the region
   DWORD i;
   CLIPINFO *pci;
   for (i = 0; i < ptw->ClipInfo.Num(); i++) {
      pci = (PCLIPINFO) ptw->ClipInfo.Get(i);

      // if the bottom of region is less than top the continue
      if (pci->r.bottom <= iTopY)
         continue;

      // if gone past then stop
      if (pci->r.top >= iBottom)
         break;

      // else it's within the clip area
      if (fLeft)
         pci->r.left = max(pci->r.left, iEdgeRight);
      else
         pci->r.right = min(pci->r.right, iEdgeLeft);
   }

   // new location
   pr->top = iTopY;
   pr->bottom = pr->top + iHeight;
   pr->left = iEdgeLeft;
   pr->right = iEdgeRight;

   // done
}


/************************************************************
CTextLine::CTextLine - constructor & destructpr
*/
CTextLine::CTextLine (void)
{
   m_dwLastAlign = 0;
   Clear ();
}

CTextLine::~CTextLine (void)
{
   // left blank
}


/**************************************************************
CTextLine::Clear - Clears out the line data to begin a new line.
*/
void CTextLine::Clear (void)
{
   m_lElem.Clear ();
   m_iHeightCenter = m_iHeightTop = m_iHeightBottom = 0;
   m_iHeightAbove = m_iHeightBelow = 0;
   m_iWidth = m_iWidthBack = 0;
   m_dwAlign = (DWORD)-1;
   m_fHasCR = FALSE;

   m_iLineSpacePar = m_iLineSpaceWrap = 0;
   m_iIndentLeftFirst = m_iIndentLeftSecond = m_iIndentRight = 0;
}



/**************************************************************
CTextLine::Width - Returns the width
*/
int CTextLine::Width (void)
{
   return m_iWidth;
}

/**************************************************************
CTextLine::Height - Returns the height
*/
int CTextLine::Height (void)
{
   int   iAbove, iBelow;

   // text
   iAbove = m_iHeightAbove;
   iBelow = m_iHeightBelow;

   // acount for objects above/below
   iBelow = max (iBelow, m_iHeightTop - m_iHeightAbove);
   iAbove = max (iAbove, m_iHeightBottom);

   // center - with text, ignoring decenders
   iAbove = max(iAbove, m_iHeightCenter);

   // total
   int   iTotal;
   iTotal = iAbove + iBelow;

   return iTotal;
}

/**************************************************************
CTextLine::HypWidth - Returns the width + a hypothesized extra
*/
int CTextLine::HypWidth (int iExtra)
{
   return Width() + iExtra;
}

/**************************************************************
CTextLine::HypWidthTab - Hypothesize adding a tab on.

inputs  
   int      iLeft - Left hand side (pixels) where plan to start the text. Affects tabs
   TWFONTINFO *pfi - Font info
returns
   int - New width
*/
int      CTextLine::HypWidthTab (int iLeft, TWFONTINFO *pfi)
{
   DWORD dwAlign;
   dwAlign = m_dwAlign;
   if (dwAlign == (DWORD)-1)
      dwAlign = m_dwLastAlign = pfi->dwJust;

   if (dwAlign == 0) {  // left justified
      // calculate new end
      int   iEnd;
      iEnd = iLeft + Width() + pfi->iTab;
      iEnd -= (iEnd % pfi->iTab);

      return iEnd - iLeft;
   }
   else  // center or right justify, so tabs are full width
      return Width() + pfi->iTab;
}

/**************************************************************
CTextLine::HypHeight - Hypothesize assuming a new font is added or a new object is added
*/
int CTextLine::HypHeight (TWFONTINFO *pfi)
{
   int   iTempAbove, iTempBelow, iHyp;

   // remember the alignment & stuff if there's nothing ste
   if (m_dwAlign == (DWORD)-1) {
      m_dwAlign = m_dwLastAlign = pfi->dwJust;
      m_iLineSpacePar = pfi->iLineSpacePar;
      m_iLineSpaceWrap = pfi->iLineSpaceWrap;
      m_iIndentLeftFirst = pfi->iIndentLeftFirst;
      m_iIndentLeftSecond = pfi->iIndentLeftWrap;
      m_iIndentRight = pfi->iIndentRight;
   }

   iTempAbove = m_iHeightAbove;
   iTempBelow = m_iHeightBelow;
   m_iHeightAbove = max(m_iHeightAbove, pfi->iAbove + pfi->iSuper);
   m_iHeightBelow = max(m_iHeightBelow, pfi->iBelow - pfi->iSuper);
      // note: including superscript in calculations

   iHyp = Height();

   m_iHeightAbove = iTempAbove;
   m_iHeightBelow = iTempBelow;

   return iHyp;
}

int CTextLine::HypHeight (TWOBJECTINFO *poi)
{
   if (!(poi->dwText & TWTEXT_INLINE))
      return 1000000;   // to make sure dont send this

   int   iTemp, *pi;
   switch (poi->dwText & TWTEXT_INLINE_MASK) {
      case TWTEXT_INLINE_TOP:
         pi = &m_iHeightTop;
         break;
      case TWTEXT_INLINE_BOTTOM:
         pi = &m_iHeightBottom;
         break;
      case TWTEXT_INLINE_CENTER:
      default:
         pi = &m_iHeightCenter;
         break;
   }

   iTemp = *pi;
   *pi = max (poi->iHeight, *pi);

   int   iHyp;
   iHyp = Height ();

   *pi = iTemp;

   return iHyp;
}



/*************************************************************************
CTextLine::AddTab - Adds a tab to the line. NOTE: Tab characters must NOT
   be added with AddText. Only printable characters and spaces are allowed.

inputs
   int   iLeft - Left side where line will start.
               NOTE - this is assume left justified. Tabs not so happy with right justification
   TWFONTINFO *pfi - font into for the tab
returns
   BOOL - TRUE if success
*/
BOOL CTextLine::AddTab (int iLeft, TWFONTINFO *pfi)
{
   int   iWidth, iTabWidth;

   // adjust alignment
   if (m_dwAlign == (DWORD)-1) {
      m_dwAlign = m_dwLastAlign = pfi->dwJust;
      m_iLineSpacePar = pfi->iLineSpacePar;
      m_iLineSpaceWrap = pfi->iLineSpaceWrap;
      m_iIndentLeftFirst = pfi->iIndentLeftFirst;
      m_iIndentLeftSecond = pfi->iIndentLeftWrap;
      m_iIndentRight = pfi->iIndentRight;
   }

   // hypothesize the width with a tab.
   iWidth = HypWidthTab (iLeft, pfi);
   iTabWidth = iWidth - Width();
   m_iWidth = iWidth;
   m_iWidthBack = max(m_iWidth, m_iWidthBack);

   // include the tab font's height
   m_iHeightAbove = max(m_iHeightAbove, pfi->iAbove + pfi->iSuper);
   m_iHeightBelow = max(m_iHeightBelow, pfi->iBelow - pfi->iSuper);
      // note: including superscript in calculations

   // add as an element
   LINETAB lt;
   lt.dwID = 3;
   lt.iWidth = iTabWidth;

   return m_lElem.Add (&lt, sizeof(lt));
}


/*************************************************************************
CTextLine::AddText - Adds a text string (that does not include non-printable
   characters, except space) to the line.

inputs
   TWFONTINFO  *pfi - font info for text
   int         iWidthPixel - Number of pixels wide it ended up being
   DWORD       dwIndexStart - starting character, from input string
   DWORD       dwCount - number of characters
returns
   BOOL - TRUE if succede
*/
BOOL CTextLine::AddText (TWFONTINFO *pfi, int iWidthPixel, DWORD dwIndexStart, DWORD dwCount)
{
   // adjust alignment
   if (m_dwAlign == (DWORD)-1) {
      m_dwAlign = m_dwLastAlign = pfi->dwJust;
      m_iLineSpacePar = pfi->iLineSpacePar;
      m_iLineSpaceWrap = pfi->iLineSpaceWrap;
      m_iIndentLeftFirst = pfi->iIndentLeftFirst;
      m_iIndentLeftSecond = pfi->iIndentLeftWrap;
      m_iIndentRight = pfi->iIndentRight;
   }

   // adjust the width
   m_iWidth += iWidthPixel;
   m_iWidthBack = max(m_iWidthBack, m_iWidth);

   // include the tab font's height
   m_iHeightAbove = max(m_iHeightAbove, pfi->iAbove + pfi->iSuper);
   m_iHeightBelow = max(m_iHeightBelow, pfi->iBelow - pfi->iSuper);
      // note: including superscript in calculations

   // store away
   LINETEXTELEM   te;
   te.dwID = 1;
   te.dwIndexCount = dwCount;
   te.dwIndexStart = dwIndexStart;
   te.iAbove = pfi->iAbove + pfi->iSuper;
   te.iBelow = pfi->iBelow - pfi->iSuper;
   te.te.pfi = pfi;
   te.iWidth = iWidthPixel;
   // not yet defined - te.te.r
   memset (&te.te.r, 0, sizeof(te.te.r));

   return m_lElem.Add (&te, sizeof(te)) != (DWORD)-1;
}

/*************************************************************************
CTextLine::AddCR - Adds a CR to the line. While it doesn't actually change the
   width, it makes sure the height/justifcation aer fixed.

inputs
   TWFONTINFO  *pfi - font info for text
returns
   BOOL - TRUE if succede
*/
BOOL CTextLine::AddCR (TWFONTINFO *pfi)
{
   // adjust alignment
   if (m_dwAlign == (DWORD)-1) {
      m_dwAlign = m_dwLastAlign = pfi->dwJust;
      m_iLineSpacePar = pfi->iLineSpacePar;
      m_iLineSpaceWrap = pfi->iLineSpaceWrap;
      m_iIndentLeftFirst = pfi->iIndentLeftFirst;
      m_iIndentLeftSecond = pfi->iIndentLeftWrap;
      m_iIndentRight = pfi->iIndentRight;
   }

   // include the tab font's height
   m_iHeightAbove = max(m_iHeightAbove, pfi->iAbove + pfi->iSuper);
   m_iHeightBelow = max(m_iHeightBelow, pfi->iBelow - pfi->iSuper);
      // note: including superscript in calculations

   // remember that added cr
   m_fHasCR = TRUE;

   return TRUE;
}

/*************************************************************************
CTextLine::AddObject - Adds an object to the line.

inputs
   TWOBJECTINFO   *poi - object
returns
   BOOL - TRUE if successful, FALSE if not
*/
BOOL CTextLine::AddObject (TWOBJECTINFO *poi)
{
   // make sure it's inline
   if (!(poi->dwText & TWTEXT_INLINE))
      return 1000000;   // to make sure dont send this

   // adjust alignment
   if ((m_dwAlign == (DWORD)-1) && (poi->fi.dwJust != (DWORD)-1)) {
      TWFONTINFO  *pfi = &poi->fi;
      m_dwAlign = m_dwLastAlign = pfi->dwJust;
      m_iLineSpacePar = pfi->iLineSpacePar;
      m_iLineSpaceWrap = pfi->iLineSpaceWrap;
      m_iIndentLeftFirst = pfi->iIndentLeftFirst;
      m_iIndentLeftSecond = pfi->iIndentLeftWrap;
      m_iIndentRight = pfi->iIndentRight;
   }

   // adjust the height
   switch (poi->dwText & TWTEXT_INLINE_MASK) {
      case TWTEXT_INLINE_TOP:
         m_iHeightTop = max(m_iHeightTop, poi->iHeight);
         break;
      case TWTEXT_INLINE_BOTTOM:
         m_iHeightBottom= max(m_iHeightBottom, poi->iHeight);
         break;
      case TWTEXT_INLINE_CENTER:
      default:
         m_iHeightCenter = max(m_iHeightCenter, poi->iHeight);
         break;
   }

   // and the width
   if (!(poi->dwText & TWTEXT_BEHIND)) {
      m_iWidth += poi->iWidth;
      m_iWidthBack = max(m_iWidthBack, m_iWidth);
   }
   else {
      m_iWidthBack = max(m_iWidthBack, m_iWidth + poi->iWidth);
   }

   // add it to the list
   LINEOBJECTPOSN op;
   memset (&op, 0, sizeof(op));
   op.dwID = 2;
   op.op.dwText = poi->dwText;
   op.op.dwBehind = (poi->dwText & TWTEXT_BEHIND) ? 1 : 0;
   if (poi->dwText == TWTEXT_BACKGROUND)
      op.op.dwBehind = 2;  // special background mode
   op.op.qwID = poi->qwID;
   op.op.r.left = op.op.r.top = 0;  // storing just width and height in rect for now
   op.op.r.right = poi->iWidth;
   op.op.r.bottom = poi->iHeight;

   return m_lElem.Add (&op, sizeof(op));
}


/************************************************************************************8
CTextLine::Render - Renders the line into plTextElem and plObjectPosn using the rPosn
   rectangle.

inputs
   PWSTR    pszText - Text that added text elements from
   RECT     rPosn - Rectangle to render into. r.bottom is ignored/
   CListVariable  *plTextElem - Text elements
   CListVariable  *plObjectPosn - Objects
   int      *piBottomY - Filled with the bottom Y coordinate
returns
   BOOL - TRUE if successful.
*/
BOOL CTextLine::Render (PWSTR pszText,
                        RECT rPosn, CListVariable *plTextElem, CListFixed *plObjectPosn,
                        int *piBottomY)
{
   CMem  mem;

   // force a justification
   if (m_dwAlign == (DWORD)-1)
      m_dwAlign = m_dwLastAlign;

   // figure out left
   int   iLeft;
   switch (m_dwAlign) {
   case 1: // center
      iLeft = (rPosn.right + rPosn.left) / 2 - m_iWidthBack / 2;
      break;

   case 2: // right
      iLeft = rPosn.right - m_iWidthBack;
      break;

   default:
   case 0:  // left
      iLeft = rPosn.left;
      break;
   }
   
   // figure out top text, baseline, total height
   int   iTopText, iBaseline, iBottomText, iBottomLine;
   iTopText = 0;
   iBaseline = m_iHeightAbove + iTopText;
   iBottomText = iBaseline + m_iHeightBelow;
   iBottomLine = iBottomText;

   // account for objects
   if (m_iHeightBottom > m_iHeightAbove) { // object off whose bottom flush with baseline
      iTopText += (m_iHeightBottom - m_iHeightAbove);
      iBaseline += (m_iHeightBottom - m_iHeightAbove);
      iBottomText += (m_iHeightBottom - m_iHeightAbove);
      iBottomLine += (m_iHeightBottom - m_iHeightAbove);
   }
   if (m_iHeightTop > (iBottomLine - iTopText)) {  // object whose top is flush with top of text
      iBottomLine += (m_iHeightTop - iBottomLine + iTopText);
   }
   int   iTextHeight, iOverflowTop, iOverflowBottom;
   iTextHeight = iBaseline - iTopText;
   iOverflowTop = iOverflowBottom = 0;
   if (m_iHeightCenter > iTextHeight) {
      int   iOverflow;
      iOverflow = m_iHeightCenter - iTextHeight;
      iOverflowBottom = iOverflow / 2;
      iOverflowTop = iOverflow - iOverflowBottom;
   }
   if (iOverflowTop > iTopText) {
      int   iDelta;
      iDelta = iOverflowTop;
      iTopText += iDelta;
      iBaseline += iDelta;
      iBottomText += iDelta;
      iBottomLine += iDelta;
   }
   if (iOverflowBottom > (iBottomLine - iBaseline)) {
      int   iDelta;
      iDelta = iOverflowBottom - iBaseline + iBottomLine;
      iBottomLine += iDelta;
   }

   // start at the top of the rectangle
   iTopText += rPosn.top;
   iBaseline += rPosn.top;
   iBottomText += rPosn.top;
   iBottomLine += rPosn.top;

   // we know piBottomY
   *piBottomY = iBottomLine;

   // go through each of the objects
   DWORD i;
   LINETEXTELEM   *pte;
   LINEOBJECTPOSN *pop;
   LINETAB *pt;
   TWTEXTELEM  *ptwte;
   WCHAR *psz;
   DWORD dwRequired;
   int   iOffset;
   int   iTemp;

   for (i = 0; i < m_lElem.Num(); i++) {
      DWORD *pdw;
      pdw = (DWORD*) m_lElem.Get(i);

      switch (*pdw) {
      case 1: // text
         // figure out the text location for drawing
         pte = (LINETEXTELEM*) pdw;
         pte->te.r.left = iLeft;
         pte->te.r.right = iLeft + pte->iWidth;
         pte->te.r.top = iBaseline - pte->iAbove - pte->te.pfi->iSuper;
         pte->te.r.bottom = iBaseline + pte->iBelow - pte->te.pfi->iSuper;
         pte->te.iBaseline = iBaseline - pte->te.pfi->iSuper;

         pte->te.rBack = pte->te.r;
         pte->te.rBack.top = iTopText;
         pte->te.rBack.bottom = iBottomText;
         iTemp = (iBottomText - iTopText) / 5;
         pte->te.rBack.left -= iTemp;
         pte->te.rBack.right += iTemp;
         pte->te.rBack.top -= iTemp;
         pte->te.rBack.bottom += iTemp;

         // copy it all into a structure and append the string
         dwRequired = sizeof(TWTEXTELEM) + (pte->dwIndexCount + 1) * sizeof(WCHAR);
         if (!mem.Required(dwRequired))
            return FALSE;
         ptwte = (TWTEXTELEM*) mem.p;
         *ptwte = pte->te;
         psz = (WCHAR*) (ptwte+1);
         memcpy (psz, pszText + pte->dwIndexStart, pte->dwIndexCount * sizeof(WCHAR));
         psz[pte->dwIndexCount] = 0;

         // add this
         if (plTextElem->Add (mem.p, dwRequired) == (DWORD)-1)
            return FALSE;

         // A Special hack for links. If this is a link then add an object covering
         // the same area
         if (pte->te.pfi->qwLinkID) {
            TWOBJECTPOSN   op;
            memset (&op, 0, sizeof(op));
            op.dwBehind = 1;
            op.dwText = TWTEXT_BEHIND;   // really doesn't matter
            op.qwLink = pte->te.pfi->qwLinkID;
            op.r = ptwte->rBack;
            plObjectPosn->Add (&op);
         }

         // advance
         iLeft = pte->te.r.right;
         break;

      case 2: // object
         pop = (LINEOBJECTPOSN*) pdw;

         // fill in the position
         pop->op.r.left += iLeft;
         pop->op.r.right += iLeft;
         switch (pop->op.dwText & TWTEXT_INLINE_MASK) {
            case TWTEXT_INLINE_TOP:
               iOffset = iTopText;
               break;
            case TWTEXT_INLINE_BOTTOM:
               iOffset = iBaseline - pop->op.r.bottom;
               break;
            case TWTEXT_INLINE_CENTER:
            default:
               iOffset = (iBaseline + iTopText) / 2 - pop->op.r.bottom / 2;
               break;
         }
         pop->op.r.top += iOffset;
         pop->op.r.bottom += iOffset;

         // add it
         plObjectPosn->Add (&pop->op);

         // advance if it's not background
         if (!pop->op.dwBehind)
            iLeft = pop->op.r.right;
         break;

      case 3: // tab
         pt = (LINETAB*) pdw;

         // basically, just advance
         iLeft += pt->iWidth;
         break;
      }

   }  // end loop through all elems


   //done
   return TRUE;
}

/********************************************************************
HasData - Returns TRUE if there's some data (text, object, tab) in the
   current line.
*/
BOOL CTextLine::HasData (void)
{
   if (m_fHasCR)
      return TRUE;

   if (m_lElem.Num())
      return TRUE;
   else
      return FALSE;
}



/********************************************************************
FinishOffLine - Finish off the current line. This:
   0) Do nothing is CTextLine::HasData returns FALSE
   1) Determines the bounding rectangle for the line by looking
      at the clipping region, left indent, and right indent.
   2) Call CTextLine.Render
   3) CAll CTextLine.Clear
   4) Increate the Y offset by the line's vertical spacing, plus
         any extra accorinding to the font's line spacing.
   5) Set the NewLine flag.
   6) If there's anything on the clip queue then deal with that by
      calling renderobject.

inputs
   TEXTWRAP *     ptw - text wrap info
   BOOL           fWrap - if TRUE, doing a newline because of awrap. FALSE
                     because of a CR.
returns
   BOOl - TRUE if OK, FALSE if not
*/
BOOL FinishOffLine (TEXTWRAP *ptw, BOOL fWrap)
{
   // if no data then do nothing
   if (!ptw->TextLine.HasData())
      return TRUE;


   // look at the clipping region
   int   iWidth, iRight, iLeft;
   iWidth = ClipText (ptw, ptw->TextLine.Height(), ptw->iCurY,
      ptw->fNewLine ? ptw->TextLine.m_iIndentLeftFirst : ptw->TextLine.m_iIndentLeftSecond,
      ptw->TextLine.m_iIndentRight, &iLeft, &iRight);

   // render
   int   iY;
   RECT  rPosn;
   rPosn.left = iLeft;
   rPosn.right = iRight;
   rPosn.top = ptw->iCurY;
   rPosn.bottom = 1000000;
   ptw->TextLine.Render (ptw->pszText, rPosn, ptw->pListTextElem, ptw->pListObjectPosn, &iY);

   // increase cur posn
   iY += fWrap ? ptw->TextLine.m_iLineSpaceWrap : ptw->TextLine.m_iLineSpacePar;
   ptw->iCurY = iY;

   // clear the old line
   ptw->TextLine.Clear ();

   ptw->fNewLine = !fWrap;

   // go through the clipping queue and add those
   while (ptw->ClipQueue.Num()) {
      CLIPQUEUE   *pc;
      pc = (CLIPQUEUE*) ptw->ClipQueue.Get(0);

      RenderObject (ptw, pc->pObject);

      // delete element
      ptw->ClipQueue.Remove(0);
   }

   // pune
   ClipPrune (ptw);

   return TRUE;
}



/********************************************************************
DoesLRObjectFit - Given an object that's going to be put on the left/right
of the display area, figure out if it fits below a specific Y height.

inputs
   TEXTWRAP *        ptw - text wrap info
   PTWOBJECTINFO     *poi - object info
   int               iY - current Y
   int               *piLeft, *piRight - left & right
returns
   BOOL - TRUE if fits
*/
BOOL DoesLRObjectFit (TEXTWRAP *ptw, PTWOBJECTINFO poi, int iY, int*piLeft, int*piRight)
{
   // figure out if what it'll do to the left/right edge
   int   iWidth;
   iWidth = ClipWidth (ptw, poi->iHeight, iY, piLeft, piRight);

   return (poi->iWidth <= iWidth);
}


/********************************************************************
FindWhereLRObjectFits - Given an object that's attached to the left/right
of the display area, figure out the highest Y height where it fits. If
this gets tot he end and it still doesn't fit, it pretends it fits.

inputs
   TEXTWRAP *        ptw - text wrap info
   PTWOBJECTINFO     *poi - object info
   int               iY - current Y
   int               *piLeft, *piRight - left & right
returns
   int - Y at which it fits
*/
int FindWhereLRObjectFits (TEXTWRAP *ptw, PTWOBJECTINFO poi, int iY, int*piLeft, int*piRight)
{
   // see if it fits at the current line
   if (DoesLRObjectFit (ptw, poi, iY, piLeft, piRight))
      return iY;

   // start looking
   DWORD i;
   CLIPINFO *ci;
   for (i = 0; i < ptw->ClipInfo.Num(); i++) {
      ci = (CLIPINFO*) ptw->ClipInfo.Get(i);

      if (ci->r.top <= iY)
         continue;   // above what we just looked at

      // else, a new clipping area. see ifit works
      if (DoesLRObjectFit (ptw, poi, ci->r.top, piLeft, piRight))
         return ci->r.top;
   }

   // else, got to the end, so use the last clip area and say yet
   ci = (CLIPINFO*) ptw->ClipInfo.Get(ptw->ClipInfo.Num()-1);
   return max(ci->r.top, iY);
}


/********************************************************************
RenderObject - Given a PTWOBJECTINFO, this deals with it.

   If it's a left/right edge thing then it:
      1) Sees if it'll fit in the current line.
      Yes?
         Add it to pListObjectPosn
         Adjust the clipping rect.
      No? (Note: If the current is empty, !CTextLine:HasData, then fit it in somehow.)
         Add it to the clip queue

   If it's inline
      1) See if it'll fit within the current line
      Yes?
         CTextLine::AddObject
      No?
         FinishOffLine
         CTextLine::addObject

inputs
   TEXTWRAP *        ptw - text wrap info
   PTWOBJECTINFO     *poi - object info
returns
   BOOL - TRUE/FALSE
*/
BOOL RenderObject (TEXTWRAP *ptw, PTWOBJECTINFO poi)
{
   DWORD  dwBehind = (poi->dwText & TWTEXT_BEHIND) ? 1 : 0;
   if (poi->dwText == TWTEXT_BACKGROUND)
      dwBehind = 2;  // special background mode

   if (poi->dwText & TWTEXT_INLINE) {
      // it's an inline object

findlongest:
      // NOTE: Hypothesizing even if it's behind, just to entire that
      // it will fit in the display

      // see if it fits
      int   iHypHeight, iHypWidth;
      iHypHeight = ptw->TextLine.HypHeight (poi);
      iHypWidth = ptw->TextLine.HypWidth (poi->iWidth);

      int   iWidth, iRight, iLeft;
      iWidth = ClipText (ptw, iHypHeight, ptw->iCurY,
         ptw->fNewLine ? ptw->TextLine.m_iIndentLeftFirst : ptw->TextLine.m_iIndentLeftSecond,
         ptw->TextLine.m_iIndentRight, &iLeft, &iRight);

      // if it doesn't fit & there's no line then keep moving down until no more clipping
      if ((iWidth < iHypWidth) && !ptw->TextLine.HasData()) {
         DWORD i;
         CLIPINFO *pci;
         for (i = 0; i < ptw->ClipInfo.Num(); i++) {
            // loop until find a clip slice that is entirely below the current top
            pci = (CLIPINFO*) ptw->ClipInfo.Get(i);
            if (ptw->iCurY >= pci->r.top)
               continue;

            // else found it
            ptw->iCurY = pci->r.top;
            goto findlongest;
         }

      }

      // if it wont fit as is, then finish off the line
      if (iWidth < iHypWidth)
         FinishOffLine (ptw, TRUE);

      // add it, whether it fits now or not.
      ptw->TextLine.AddObject (poi);
   }
   else if (dwBehind == 2) {
      // it's a special background object
      TWOBJECTPOSN   op;
      memset (&op, 0, sizeof(op));
      op.dwText = poi->dwText;
      op.dwBehind = dwBehind;
      op.qwID = poi->qwID;
      memset (&op.r, 0, sizeof(op.r));
      ptw->pListObjectPosn->Add (&op);
   }
   else {
      // it's on the left/right

      // figure out first place where can get the object (by itself) to fit
      int   iYFit, iLeft, iRight;
      iYFit = FindWhereLRObjectFits (ptw, poi, ptw->iCurY, &iLeft, &iRight);

      // is there any data in the line
      if (ptw->TextLine.HasData()) {
         // see if the object by itself will fit on the current line
         if (iYFit != ptw->iCurY) {
            // no, the object by iself doesn't fit, so add it to the queue
            CLIPQUEUE   cq;
            cq.pObject = poi;
            ptw->ClipQueue.Add (&cq);

            return TRUE;
         }

         int   iWidthT, iRightT, iLeftT;
         int   iHypHeight, iHypWidth;
         iHypHeight = ptw->TextLine.Height ();
         iHypWidth = ptw->TextLine.HypWidth (poi->iWidth);
         iWidthT = ClipText (ptw, iHypHeight, ptw->iCurY,
            ptw->fNewLine ? ptw->TextLine.m_iIndentLeftFirst : ptw->TextLine.m_iIndentLeftSecond,
            ptw->TextLine.m_iIndentRight, &iLeftT, &iRightT);
         // NOTE: If it's behind the text then don't worry about it fitting in with
         // the text
         // BUGFIX - when put spitting teapot in overview.mml, it seems to be
         // placed on line below where it should be
//         if (!dwBehind && (iWidthT > (iRight - iLeft - poi->iWidth))) {
         if (!dwBehind && (iHypWidth > (iRight - iLeft))) {
            // the combined text and L/R object won't fit, so add the L/R object to the queue

            CLIPQUEUE   cq;
            cq.pObject = poi;
            ptw->ClipQueue.Add (&cq);

            return TRUE;
         }
      }

      // else, there's either no data in the TextLine, or the bject will fit, so add it.
      RECT  r;
      if (!dwBehind)
         ClipAddRegion (ptw, poi->iWidth, poi->iHeight, iYFit,
            (poi->dwText & TWTEXT_EDGE_LEFT) ? TRUE : FALSE, &r);
      else {
         r.top = iYFit;
         r.bottom = r.top + poi->iHeight;
         if (poi->dwText & TWTEXT_EDGE_LEFT) {
            r.left = iLeft;
            r.right = r.left + poi->iWidth;
         }
         else {
            r.right = iRight;
            r.left = r.right - poi->iHeight;
         }
      }

      TWOBJECTPOSN   op;
      memset (&op, 0, sizeof(op));
      op.dwText = poi->dwText;
      op.dwBehind = dwBehind;
      op.qwID = poi->qwID;
      op.r = r;
      ptw->pListObjectPosn->Add (&op);
   }

   return TRUE;
}

/********************************************************************
ParseNoCR - Called by ParseWithCR. Parses a string sequence that has
   no CR/Tab. Which means it needs to be broken up into words.

   1) Identify line-break character transitions
      - space to letter. Inbetween
      - such as after hyphen.
   2) Starting with the smallest string, see if it'll fit in the line.
         If it does, repeat until find the longest string that will.
   3) If even the first string doesn't appear, do FinishOffLine.
      a) Retry to fit.
      b) If it still doesn't fit do it character by character. At least one character.

IMPORTANT: What about spaces at the end?

inputs
   TEXTWRAP *        ptw - text wrap info
   DWORD             dwStart - start character
   DWORD             dwCount - number of characters
returns
   BOOL - TRUE/FALSE
*/
BOOL ParseNoCR (TEXTWRAP *ptw, DWORD dwStart, DWORD dwCount)
{
   // mem is used to remember if there's a word break AFTER the current character.
   // There's always a word break after the last one.

moretext:
   CMem  mem;
   mem.Required (dwCount);
   PBYTE  p;
   p = (PBYTE) mem.p;

   // make sure there's some text
   if (!dwCount)
      return TRUE;

   // idenfity word-break's
   DWORD i, dwIndex;
   for (i = 0, dwIndex = dwStart; i < dwCount; i++, dwIndex++) {
      // always a word break on the last character
      if (i+1 == dwCount) {
         p[i] = TRUE;
         break;
      }

      // if this character is non-space and the next is space
      if ((ptw->pszText[dwIndex] != L' ') && (ptw->pszText[dwIndex+1] == L' ')) {
         p[i] = TRUE;
         continue;
      }
      
      // if this is a hyphen
      if (ptw->pszText[dwIndex] == L'-') {
         p[i] = TRUE;
         continue;
      }

      // else not
      p[i] = FALSE;
   }

   // find the first
   DWORD dwFirst;
   for (i = 0; i < dwCount; i++)
      if (p[i])
         break;
   dwFirst = i;

   // see if it the text fits
   CMem  ansi;
   RECT  r;
   DWORD dwCurCount, dwLastFit;
   HFONT hOld;
   hOld = (HFONT) SelectObject (ptw->hDC, (ptw->ppfi[dwStart])->hFont);
   int   iSLen;

   // optimize this so calculate the lengths of the whole string and separating
   // points for individual characters, and then break. Askign for line length one
   // word at a time is slow.
   // see if this fits
   ansi.Required ((dwCount + 1) * 2);
   iSLen = WideCharToMultiByte (CP_ACP, 0, ptw->pszText + dwStart, dwCount,
      (char*) ansi.p, (int)ansi.m_dwAllocated, 0, 0);
   CMem  memInt;
   if (!memInt.Required (iSLen * sizeof(int)))
      return FALSE; // error

   SetTextAlign (ptw->hDC, TA_BASELINE | TA_LEFT);
   int   iFit;
   SIZE  s;
   iFit = 0;
   if (!GetTextExtentExPoint (ptw->hDC, (char*) ansi.p, iSLen, ptw->iWidth, &iFit, (int*) memInt.p, &s)) {
      SelectObject (ptw->hDC, hOld);
      return FALSE; // error
   }

findlongest:
   dwLastFit = (DWORD)-1; // last length that fit
   dwCurCount = dwFirst;
   while (TRUE) {
      // increase dwCurCount by one so includes all the numbers
      dwCurCount++;

      ansi.Required ((dwCurCount+1) * 2);
      iSLen = WideCharToMultiByte (CP_ACP, 0, ptw->pszText + dwStart, dwCurCount,
         (char*) ansi.p, (dwCurCount+1)*2, 0, 0);
      // calc iSLen so that know how many ansi chars in

#if 0
      r.left = r.top = 0;
      r.bottom = 10000;
      r.right = 10000;

      // BUGFIX - Put in NOPREFIX so ampersand would be properly drawn/calculated
      DrawText (ptw->hDC, (char*) ansi.p, iSLen, &r, DT_NOPREFIX | DT_CALCRECT);
#else

      r.left = r.top = 0;
      if (iSLen > iFit)
         r.right = 100000;   // it doesn't fit
      else
         r.right = iSLen ? ((int*) memInt.p)[iSLen-1] : 0;
      r.bottom = s.cy;
#endif // 0

      // see if this fits
      // see if it fits
      int   iHypHeight, iHypWidth;
      iHypHeight = ptw->TextLine.HypHeight (ptw->ppfi[dwStart]);
      iHypWidth = ptw->TextLine.HypWidth (r.right - r.left);

      int   iWidth, iRight, iLeft;
      iWidth = ClipText (ptw, iHypHeight, ptw->iCurY,
         ptw->fNewLine ? ptw->TextLine.m_iIndentLeftFirst : ptw->TextLine.m_iIndentLeftSecond,
         ptw->TextLine.m_iIndentRight, &iLeft, &iRight);

      // if it wont fit as is, then finish off the line
      if (iWidth >= iHypWidth)
         dwLastFit = dwCurCount;
      else {
         // it doesn't fit

         // if this is the first word, which doesn't fit, then see if can move the
         // line down until it does. This keeps on trying the next lowest clip
         // region until something fits
         if ((dwLastFit == (DWORD)-1) && !ptw->TextLine.HasData()) {
            DWORD i;
            CLIPINFO *pci;
            for (i = 0; i < ptw->ClipInfo.Num(); i++) {
               // loop until find a clip slice that is entirely below the current top
               pci = (CLIPINFO*) ptw->ClipInfo.Get(i);
               if (ptw->iCurY >= pci->r.top)
                  continue;

               // else found it
               ptw->iCurY = pci->r.top;
               goto findlongest;
            }

         }

         break;
      }

      // go onto the next break
      for (; dwCurCount < dwCount; dwCurCount++)
         if (p[dwCurCount])
            break;
      if (dwCurCount >= dwCount)
         break;   // no more
   }

   // if it doesn't fit anything and there's data in the line then flush the line and retry
   if ((dwLastFit == (DWORD)-1) && ptw->TextLine.HasData()) {
      FinishOffLine (ptw, TRUE);
      goto findlongest;
   }

   if (dwLastFit == (DWORD)-1) {
      // if it doesn't fit anything and there isn't any data, then do character by character
      // at the very least, assume that one character first
      dwLastFit = 1;
      dwCurCount = 2;
      while (TRUE) {
         ansi.Required ((dwCurCount+1) * 2);
         iSLen = WideCharToMultiByte (CP_ACP, 0, ptw->pszText + dwStart, dwCurCount,
            (char*) ansi.p, (dwCurCount+1)*2, 0, 0);
         r.left = r.top = 0;
         r.bottom = 10000;
         r.right = 10000;

         // BUGFIX - Put in NOPREFIX so ampersand would be properly drawn/calculated
         DrawText (ptw->hDC, (char*) ansi.p, iSLen, &r, DT_NOPREFIX | DT_CALCRECT);

         // see if this fits
         // see if it fits
         int   iHypHeight, iHypWidth;
         iHypHeight = ptw->TextLine.HypHeight (ptw->ppfi[dwStart]);
         iHypWidth = ptw->TextLine.HypWidth (r.right - r.left);

         int   iWidth, iRight, iLeft;
         iWidth = ClipText (ptw, iHypHeight, ptw->iCurY,
            ptw->fNewLine ? ptw->TextLine.m_iIndentLeftFirst : ptw->TextLine.m_iIndentLeftSecond,
            ptw->TextLine.m_iIndentRight, &iLeft, &iRight);

         // if it wont fit as is, then finish off the line
         if (iWidth >= iHypWidth)
            dwLastFit = dwCurCount;
         else
            break;

         // go onto the next character
         dwCurCount++;
         if (dwCurCount >= dwCount)
            break;   // no more
      }
   }

   // we have something by now. Figure out its size
   dwCurCount = dwLastFit;
   ansi.Required ((dwCurCount+1) * 2);
   iSLen = WideCharToMultiByte (CP_ACP, 0, ptw->pszText + dwStart, dwCurCount,
      (char*) ansi.p, (dwCurCount+1)*2, 0, 0);
   r.left = r.top = 0;
   r.bottom = 10000;
   r.right = 10000;
   // BUGFIX - Put in NOPREFIX so ampersand would be properly drawn/calculated
   DrawText (ptw->hDC, (char*) ansi.p, iSLen, &r, DT_NOPREFIX | DT_CALCRECT);

   // add it
   ptw->TextLine.AddText (ptw->ppfi[dwStart], r.right - r.left, dwStart, dwCurCount);

   SelectObject (ptw->hDC, hOld);

   // if this ended with a space then remove spaces until there's more text
   // however, if the entire string ended with spaces then must leave them
   // in because they might be necessary to separate an aobject
   DWORD dwNoSpace;
   dwNoSpace = dwCurCount;
   while ((dwNoSpace < dwCount) && (ptw->pszText[dwStart + dwNoSpace] == L' '))
      dwNoSpace++;
   if (dwNoSpace < dwCount)
      dwCurCount = dwNoSpace;

   // are we done
   if (dwCurCount >= dwCount)
      return TRUE;

   // if we're not done then we know we word wrapped, so flush now
   FinishOffLine (ptw, TRUE);

   // else repeat
   dwStart += dwCurCount;
   dwCount -= dwCurCount;

   goto moretext;
}


/********************************************************************
ParseWithCR - Called by ParseLargestTextSequence. This parses a string
   that has a unified font and no objects. It finds the tabs and CR,
   and splits the string into bits. In turn, it calls ParseNoCR for
   a sequence without a CR/Tab, or it handles the CR/Tab.

   Handle the tab by:
      1) Seeing if it fits. If No thenFinishOffLine.
      2) CTextLine::AddTab

   Handle CR by:
      1) CTextLine::AddCR
      2) FinishOffLine

inputs
   TEXTWRAP *        ptw - text wrap info
   DWORD             dwStart - start character
   DWORD             dwCount - number of characters
returns
   BOOL - TRUE/FALSE
*/
BOOL ParseWithCR (TEXTWRAP *ptw, DWORD dwStart, DWORD dwCount)
{
   while (dwCount) {
      switch (ptw->pszText[dwStart]) {
      case L'\t':
         {
            // detect if image is inserted to the left and do this
            int   iWidth, iRight, iLeft;
            int   iHypHeight, iHypWidth;
            iHypHeight = ptw->TextLine.HypHeight (ptw->ppfi[dwStart]);
            iWidth = ClipText (ptw, iHypHeight, ptw->iCurY,
               ptw->fNewLine ? ptw->TextLine.m_iIndentLeftFirst : ptw->TextLine.m_iIndentLeftSecond,
               ptw->TextLine.m_iIndentRight, &iLeft, &iRight);


            // it's a tab. Hypthesize adding it
            iHypWidth = ptw->TextLine.HypWidthTab (
               iLeft,
               ptw->ppfi[dwStart]);


            // if it's too large then stick in a word wrap
            if (iWidth < iHypWidth)
               FinishOffLine (ptw, TRUE);

            // put the tab in
            ptw->TextLine.AddTab (
               iLeft,
               ptw->ppfi[dwStart]);

            dwStart++;
            dwCount--;
           
         }
         break;

      case L'\n': // non-wrap newline
      case 1: // wrap newline
         {
            // cr. Just put it in

            ptw->TextLine.AddCR (ptw->ppfi[dwStart]);

            // and finish off the line with a FALSE for wrap
            FinishOffLine (ptw, ptw->pszText[dwStart] == 1 /* wrap newline*/);

            dwStart++;
            dwCount--;
           
         }
         break;

      case L'\r':
         // ignore this character
         dwStart++;
         dwCount--;
         break;

      default:
         {
            // it's text. See how far can go until  run into a newline, tab
            DWORD i;
            for (i = 0; i < dwCount; i++)
               if ((ptw->pszText[dwStart+i] == L'\t') || (ptw->pszText[dwStart+i] == L'\n') ||
                   (ptw->pszText[dwStart+i] == 1 /* wrap newline*/) ||(ptw->pszText[dwStart+i] == L'\r'))
                  break;

            // i is not the length that can go before /r or /n
            ParseNoCR (ptw, dwStart, i);

            // move on
            dwStart += i;
            dwCount -= i;
         }
         break;
      }
   }

   // done
   return TRUE;
}



/********************************************************************
ParseLargestTextSequence - Finds the largest text sequence that can
reasonably be parsed. This is:
   1) Not interrupted by objects.
   2) Same font & other details.

It then calls the ParseWithCR for the segment. This does the rest of the parsing.

inputs
   TEXTWRAP *        ptw - text wrap info
returns
   BOOL - TRUE/FALSE
*/
BOOL ParseLargestTextSequence (TEXTWRAP * ptw)
{
   DWORD dwMax;

   // find the next object
   dwMax = ptw->dwTextLen;
   if (ptw->dwNextObject < ptw->dwNumOI)
      dwMax = min(dwMax, ptw->poi[ptw->dwNextObject].dwPosn);

   DWORD dwStart;
   dwStart = ptw->dwNextChar;

   for (; ptw->dwNextChar < dwMax; ptw->dwNextChar++) {
      // make sure same font
      if (ptw->ppfi[dwStart] != ptw->ppfi[ptw->dwNextChar])
         break;   // change in font display
   }

   // do it
   ParseWithCR (ptw, dwStart, ptw->dwNextChar - dwStart);

   return TRUE;
}



/********************************************************************
TextWrap - Figures out how the text wraps around the given objets.

inputs
   HDC            hDC - To render to.
   int            iWidth - Width in pixels of the display area
   PWSTR          pszText - NULL-terminated unicode text string
   PTWFONTINFO*   ppfi - Pointer to an array of PTWFONTINFO. One element
                  per character in pszText (excluding NULL). Each TWFONTINFO
                  describes the text formatting for the character. If one or
                  more characters in the string have the same formatting then
                  they should point to the same TWFONTINFO structure.
   PTWOBJECTINFO  poi - Array of TWOBJECTINFO structures describing what
                  objects are associated with the text. The objects should be
                  sorted according to their character position, so that objects
                  linked to earlier characters appear at the top of the list.
   DWORD          dwNumOI - Number of poi.

outputs
   int            *piWidth - Width used
   int            *piHeight - Height used
   CListVariable  *pListTextElem - Filled in with all of the text elements (TWTEXTELEM)
                  structures/strings describing what is displayed where. Not sorted.
   CListFixed     *pListObjectPosn - Filled in with all of the object positions, Not sorted.

returns
   HRESULT - error
*/
HRESULT TextWrap (HDC hDC, int iWidth, PWSTR pszText, PTWFONTINFO* ppfi,
                  PTWOBJECTINFO poi, DWORD dwNumOI,
                  int *piWidth, int *piHeight,
                  CListVariable *pListTextElem, CListFixed *pListObjectPosn)
{
   TEXTWRAP tw;

   // copy over input data
   tw.hDC = hDC;
   tw.iWidth = iWidth;
   tw.pszText = pszText;
   tw.dwTextLen = (DWORD)wcslen(pszText);
   tw.ppfi = ppfi;
   tw.poi = poi;
   tw.dwNumOI = dwNumOI;
   tw.pListTextElem = pListTextElem;
   tw.pListObjectPosn = pListObjectPosn;

   // set up some databases
   tw.iCurY = 0;
   tw.ClipInfo.Init (sizeof(CLIPINFO));
   CLIPINFO ci;
   ci.r.top = 0;
   ci.r.left = 0;
   ci.r.right = iWidth;
   ci.r.bottom = 10000000;
   tw.ClipInfo.Add (&ci);
   tw.ClipQueue.Init (sizeof(CLIPQUEUE));
   tw.dwNextChar = 0;
   tw.dwNextObject = 0;
   tw.fNewLine = TRUE;

   // repeat while have objects and characters left
   while (tw.dwNextChar < tw.dwTextLen) {
      // repeat while there are objects before the current text
      while ((tw.dwNextObject < tw.dwNumOI) && (tw.poi[tw.dwNextObject].dwPosn <= tw.dwNextChar)) {
         RenderObject (&tw, poi + tw.dwNextObject);
         tw.dwNextObject++;
      }

      // pull out the largest section of text and do that
      ParseLargestTextSequence (&tw);
   }

   // if there are objects after the next hen render them
   while ((tw.dwNextObject < tw.dwNumOI)) {
      RenderObject (&tw, poi + tw.dwNextObject);
      tw.dwNextObject++;
   }

   // finish off line, just in case. This will also empty all the queued objects
   FinishOffLine (&tw, TRUE);

   // fill in width & height
   DWORD i;
   int iLeft, iRight;
   iLeft = 1000000;
   iRight = 0;
   TWTEXTELEM  *pte;
   BOOL  fAnyElem;
   fAnyElem = FALSE;
   for (i = 0; i < pListTextElem->Num(); i++) {
      pte = (TWTEXTELEM*) pListTextElem->Get(i);
      iRight = max (iRight, pte->r.right);
      iLeft = min (iLeft, pte->r.left);
      fAnyElem = TRUE;

      // BUGFIX - if use posnedgeright and the very end of a document then m_iCalcHeight
      // does not include that object. The maximum calculated height should include up to
      // the edge of the text-wrap/clipping region
      tw.iCurY = max(tw.iCurY, pte->r.bottom);
   }
   TWOBJECTPOSN   *pop;
   for (i = 0; i < pListObjectPosn->Num(); i++) {
      pop = (TWOBJECTPOSN*) pListObjectPosn->Get(i);
      iRight = max (iRight, pop->r.right);
      iLeft = min (iLeft, pop->r.left);
      fAnyElem = TRUE;

      // BUGFIX - if use posnedgeright and the very end of a document then m_iCalcHeight
      // does not include that object. The maximum calculated height should include up to
      // the edge of the text-wrap/clipping region
      tw.iCurY = max(tw.iCurY, pop->r.bottom);
   }
   if (!fAnyElem)
      iLeft = 0;
   *piWidth = iRight - iLeft;
   *piHeight = tw.iCurY;

   return NOERROR;
}


/********************************************************************
TextMove - Move every element in the text section in X/Y. Do this so
   can center text within a table cell.

inputs
   int            iDeltaX, iDeltaY
   CListVariable  *pListTextElem - Filled in with all of the text elements (TWTEXTELEM)
                  structures/strings describing what is displayed where. Not sorted.
   CListFixed     *pListObjectPosn - Filled in with all of the object positions, Not sorted.
returns
   BOOL - TRUE if OK

BUGBUG - 2.0 - at some point may want to do clipping here so text doesnt go out of
bounds of tables
*/
BOOL TextMove (int iDeltaX, int iDeltaY, CListVariable *plte, CListFixed *plop)
{
   DWORD i;
   TWTEXTELEM *pte;
   TWOBJECTPOSN *pop;
   for (i = 0; i < plte->Num(); i++) {
      pte = (TWTEXTELEM*) plte->Get(i);
      if (!pte)
         return FALSE;
      pte->iBaseline += iDeltaY;
      pte->r.left += iDeltaX;
      pte->r.right += iDeltaX;
      pte->r.top += iDeltaY;
      pte->r.bottom += iDeltaY;
      pte->rBack.left += iDeltaX;
      pte->rBack.right += iDeltaX;
      pte->rBack.top += iDeltaY;
      pte->rBack.bottom += iDeltaY;
   }

   for (i = 0; i < plop->Num(); i++) {
      pop = (TWOBJECTPOSN*) plop->Get(i);
      if (!pop)
         return FALSE;

      pop->r.left += iDeltaX;
      pop->r.right += iDeltaX;
      pop->r.top += iDeltaY;
      pop->r.bottom += iDeltaY;
   }

   return TRUE;

}


/********************************************************************
TextBackground - Adjust special background objects so they take up the entire
space alloted. This way can have an object that draws the background of
the cell/page, instead of a hacked in color/jpg.

inputs
   RECT           rNew - new rectangle to use for them
   CListFixed     *pListObjectPosn - Filled in with all of the object positions, Not sorted.
returns
   none
*/
void TextBackground (RECT *prNew, CListFixed *plop)
{
   DWORD i;
   TWOBJECTPOSN *pop;

   for (i = 0; i < plop->Num(); i++) {
      pop = (TWOBJECTPOSN*) plop->Get(i);
      if (!pop)
         continue;
      if (pop->dwBehind != 2)
         continue;

      // turn off this setting
      pop->dwBehind = 1;
      pop->r = *prNew;
   }

}


// BUGBUG - 2.0 - if have a change of font within a word then word-wrap will split
// the word at the change of font. Ideally, never split words


