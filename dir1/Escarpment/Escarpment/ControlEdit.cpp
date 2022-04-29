/***********************************************************************
ControlEdit.cpp - Code for a control

begun 4/14/2000 by Mike Rozak
Copyright 2000 mike Rozak. All rights reserved
*/

#include <windows.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include "escarpment.h"
#include "resleak.h"

#define ESCM_RECALC           (ESCM_USER+1)
#define ESCM_NEWSCROLL        (ESCM_USER+2)
#define ESCM_RIGHTARROW       (ESCM_USER+3)
#define ESCM_LEFTARROW        (ESCM_USER+4)
#define ESCM_LINEUP           (ESCM_USER+5)
#define ESCM_LINEDOWN         (ESCM_USER+6)
#define ESCM_HOME             (ESCM_USER+7)
#define ESCM_END              (ESCM_USER+8)
#define ESCM_CTRLHOME         (ESCM_USER+9)
#define ESCM_CTRLEND          (ESCM_USER+10)
#define ESCM_PAGEUP           (ESCM_USER+11)
#define ESCM_PAGEDOWN         (ESCM_USER+12)
#define ESCM_SHIFTRIGHTARROW  (ESCM_USER+13)
#define ESCM_SHIFTLEFTARROW   (ESCM_USER+14)
#define ESCM_SHIFTLINEUP      (ESCM_USER+15)
#define ESCM_SHIFTLINEDOWN    (ESCM_USER+16)
#define ESCM_CTRLSHIFTRIGHTARROW  (ESCM_USER+17)
#define ESCM_CTRLSHIFTLEFTARROW   (ESCM_USER+18)
#define ESCM_SHIFTHOME        (ESCM_USER+19)
#define ESCM_SHIFTEND         (ESCM_USER+20)
#define ESCM_SHIFTCTRLHOME    (ESCM_USER+21)
#define ESCM_SHIFTCTRLEND     (ESCM_USER+22)
#define ESCM_SHIFTPAGEUP      (ESCM_USER+23)
#define ESCM_SHIFTPAGEDOWN    (ESCM_USER+24)
#define ESCM_BACKSPACE        (ESCM_USER+25)
#define ESCM_DEL              (ESCM_USER+26)
#define ESCM_SELECTALL        (ESCM_USER+27)
#define ESCM_CTRLRIGHTARROW   (ESCM_USER+28)
#define ESCM_CTRLLEFTARROW    (ESCM_USER+29)

typedef struct {
   PCMem          pMem;       // memory that contains the text
   int            iLRMargin;  // left right margin
   int            iTBMargin;  // top bottom margin
   int            iTopLine;   // top line
   int            iScrollX;   // amount of scrolling horizontal. 0 = no. positive => scrolled
   int            iSelStart;  // selection start (character based). No sel if iSelStart = iSelEnd
   int            iSelEnd;    // seleection end (charcter based), also caret position.
   int            iBorderSize;    // border width
   COLORREF       cBackground;   // background color
   COLORREF       cBorder;    // border color, default blue
   PWSTR          pszHScroll;  // scroll bar name
   PWSTR          pszVScroll; // vertical scroll bar name
   COLORREF       cSelection; // color of the selection
   COLORREF       cSelectionBorder; // color of the selection border
   COLORREF       cCaret;     // color of the caret
   COLORREF       cCaretBlink;   // color of the caret when it's blinked
   int            iMaxChars;  // maximum chars allowed to be typed
   BOOL           fMultiLine; // if TRUE capture enter and be multiple line
   BOOL           fWordWrap;  // if TRUE word wrap, else keep scrolling
   BOOL           fPassword;  // if TRUE display "*" for password
   BOOL           fReadOnly;  // if TRUE read only
   BOOL           fSelAll;    // if TRUE the select the entire text when get focus
   BOOL           fPreLineIndent;   // keep the previous line's indent
   BOOL           fCaptureTab;   // if TRUE captures tab. FALSE lets them pass through
   BOOL           fDirty;     // if TRUE the data has been changed. Apps should set the dirty
                              // flag to 0 so they know if things have changed

   BOOL           fReCalc;    // if set to TRUE then need to recalc

   PCListVariable plistUndo;  // undo list. 0 is most recent
   PCListVariable plistRedo;  // redo list, 0 is most recent

   DWORD          dwFocusSet; // last time when focus set
   BOOL           fIgnoreNextButton;   // if TRUE ignore next button down because user just clicked on a selall

   int            iNumLines;  // number of lines
   int            iMaxWidth;  // max width in pixels of longest line
   PCListFixed    plistLine;  // indicates what character # line starts at. LINEINFO
   int            iLineInvalidAfter;   // plistLine is invalid after this line number, very large # if all valid
   DWORD          dwLen;      // number of characters in text
   DWORD          dwSubTimer; // so only change blink 1/2 as often

   CEscControl    *pHScroll;      // horizontal scroll control
   CEscControl    *pVScroll;      // vertical scroll control
   BOOL           fCaretBlink;   // if true caret is visible from blink, false it's not.
   POINT          pLastMouse;    // last recorded mouse position

   HFONT          hFont;      // font to use, from m_fi
   TWFONTINFO     *pfi;       // for height/width/line spacing, color, etc.
} EDIT, *PEDIT;

typedef struct {
   DWORD       dwCount;    // character that starts at
   int         iWidth;     // width
} LINEINFO, *PLINEINFO;

typedef struct {
   int         iOldSelStart, iOldSelEnd;  // set the selection to this (and this is where text came from)
   DWORD       dwStartReplace;      // start character to be replaced
   DWORD       dwLenReplaced;      // length of the string that replace. should be selstart-selend
   DWORD       dwLenReplaceWith;    // length of the string that overwrote iSelStart to iSelEnd
   // followed by NULL-terminated unicode string with undo data
} UNDOINFO, *PUNDOINFO;

static void CalcLines (PCEscControl pControl, PEDIT pc, BOOL fForce = FALSE);

/***********************************************************************
PaintLine - Paints a line of text.

inputs
   PCEscControl      pControl
   PEDIT             pc - info
   DWORD    dwLine - line number, starting at 0, to paint
   POINT    *pPoint - UL corner in HDC
   HDC      hDC - To paint into
   RECT     *prHDC - HDC rect for control
returns
   none
*/
static void PaintLine (PCEscControl pControl, PEDIT pc, DWORD dwLine, POINT *pPoint,
                       HDC hDC, RECT *prHDC)
{
   CMem  ansi;
   int   iSLen;
   HFONT hOld;

   // call calclnies just to make sure
   CalcLines (pControl, pc);

   SetBkMode (hDC, TRANSPARENT);
   SetTextAlign (hDC, TA_BASELINE | TA_LEFT);
   SetTextColor (hDC, pc->pfi->cText);
   hOld = (HFONT) SelectObject (hDC, pc->hFont);

   // charcter for the line
   DWORD dwStart, dwEnd;
   PWSTR psz;
   psz = (PWSTR) pc->pMem->p;
   if (dwLine >= (DWORD) pc->iNumLines) {
      SelectObject (hDC, hOld);
      return;   // not line
   }
   LINEINFO *pli;
   pli = (LINEINFO*) pc->plistLine->Get(dwLine);
   dwStart = pli->dwCount;
   if (dwLine+1 >= (DWORD) pc->iNumLines)
      dwEnd = pc->dwLen;
   else {
      pli = (LINEINFO*) pc->plistLine->Get(dwLine+1);
      dwEnd = pli->dwCount;
   }

   // eliminate and CRs at the end
   while ((dwEnd > dwStart) && ((psz[dwEnd-1] == L'\n') || (psz[dwEnd-1] == L'\r')) )
      dwEnd--;

   // start drawing the text
   int   iX;
   RECT  r;
   iX = 0;
   while (dwStart < dwEnd) {
      // find how far until get to tab
      DWORD dwEndCur;
      for (dwEndCur = dwStart; dwEndCur < dwEnd; dwEndCur++)
         if (psz[dwEndCur] == L'\t')
            break;

      if ((dwEndCur == dwStart) && (dwEndCur < dwEnd)) {
         // tab
         iX = (iX + pc->pfi->iTab) / pc->pfi->iTab * pc->pfi->iTab;
         dwStart = dwEndCur+1;
         continue;
      }

      // else it's text
      ansi.Required ((dwEndCur - dwStart + 1) * 2);
      iSLen = (int) WideCharToMultiByte (CP_ACP, 0, psz + dwStart, dwEndCur - dwStart,
         (char*) ansi.p, (int)ansi.m_dwAllocated, 0, 0);

      // if password fill with *
      if (pc->fPassword)
         _strnset ((char*) ansi.p, '*', iSLen);

      r.left = pPoint->x + iX;
      r.top = pPoint->y;
      r.bottom = 10000;
      r.right = 10000;
      // BUGFIX - Put in NOPREFIX so ampersand would be properly drawn/calculated
      DrawText (hDC, (char*) ansi.p, iSLen, &r, DT_NOPREFIX | DT_LEFT | DT_TOP | DT_SINGLELINE | DT_CALCRECT);
      iX = r.right - pPoint->x;
      ExtTextOut (hDC, r.left, r.top + pc->pfi->iAbove, ETO_CLIPPED, prHDC, (char*) ansi.p, iSLen, NULL);
      // DrawText (hDC, (char*) ansi.p, iSLen, &r, DT_NOPREFIX | DT_LEFT | DT_TOP | DT_SINGLELINE);

      // advance
      dwStart = dwEndCur;
   }

   SelectObject (hDC, hOld);
}

/***********************************************************************
LineExtent - Figures out where all the characters are located in the line

inputs
   PCEscControl      pControl
   PEDIT             pc - info
   DWORD             dwLine - line number, starting at 0, to paint
   HDC               hDC - To paint into
   int               paiRight - Array (number = # characters between this line & the next)
                        filled with pixel (from left edge) that is right edge of character
   DWORD             *pdwMax - maximum character filled in
returns
   BOOL - TRUE if successful
*/
static BOOL LineExtent (PCEscControl pControl, PEDIT pc, DWORD dwLine,
                       HDC hDC, int *paiRight, DWORD *pdwMax)
{
   // BUGFIX - Crashing problem where if nothing then could crash
   if (!paiRight)
      return FALSE;

   CMem  ansi;
   HFONT hOld;
   int   iLog = EscGetDeviceCaps(hDC, LOGPIXELSX);
   *pdwMax = 0;

   // call calclnies just to make sure
   CalcLines (pControl, pc);

   SetBkMode (hDC, TRANSPARENT);
   SetTextAlign (hDC, TA_BASELINE | TA_LEFT);
   SetTextColor (hDC, pc->pfi->cText);
   hOld = (HFONT) SelectObject (hDC, pc->hFont);

   // charcter for the line
   DWORD dwStart, dwEnd;
   PWSTR psz;
   psz = (PWSTR) pc->pMem->p;
   if (dwLine >= (DWORD) pc->iNumLines) {
      SelectObject (hDC, hOld);
      return FALSE;   // not line
   }
   LINEINFO *pli;
   pli = (LINEINFO*) pc->plistLine->Get(dwLine);
   dwStart = pli->dwCount;
   // BUGFIX - Potentially fix crash
   if (dwStart > pc->dwLen)
      dwStart = pc->dwLen;
   if (dwLine+1 >= (DWORD) pc->iNumLines)
      dwEnd = pc->dwLen;
   else {
      pli = (LINEINFO*) pc->plistLine->Get(dwLine+1);
      dwEnd = pli->dwCount;
   }

   // eliminate and CRs at the end
   while ((dwEnd > dwStart) && ((psz[dwEnd-1] == L'\n') || (psz[dwEnd-1] == L'\r')) )
      dwEnd--;

   // start drawing the text
   int   iX;
   iX = 0;
   DWORD dwRealStart;
   dwRealStart = dwStart;
   while (dwStart < dwEnd) {
      // find how far until get to tab
      DWORD dwEndCur;
      for (dwEndCur = dwStart; dwEndCur < dwEnd; dwEndCur++)
         if (psz[dwEndCur] == L'\t')
            break;

      if ((dwEndCur == dwStart) && (dwEndCur < dwEnd)) {
         // tab
         iX = (iX + pc->pfi->iTab) / pc->pfi->iTab * pc->pfi->iTab;
         paiRight[dwStart - dwRealStart] = iX;
         *pdwMax = dwStart - dwRealStart + 1;
         dwStart = dwEndCur+1;
         continue;
      }


      int   iMax, iSLen;
      SIZE  s;
      // see if this fits
      ansi.Required ((dwEndCur - dwStart + 1) * 2);
      iSLen = (int) WideCharToMultiByte (CP_ACP, 0, psz + dwStart, dwEndCur - dwStart,
         (char*) ansi.p, (int)ansi.m_dwAllocated, 0, 0);

      // if password fill with *
      if (pc->fPassword)
         _strnset ((char*) ansi.p, '*', iSLen);

      if (!GetTextExtentExPoint (hDC, (char*) ansi.p, iSLen, 1000000, &iMax, paiRight + (dwStart-dwRealStart), &s)) {
         SelectObject (hDC, hOld);
         return FALSE;
      }

#ifdef _DEBUG
      RECT  r;
      r.left = r.top = 0;
      r.bottom = 10000;
      r.right = 10000;
      // BUGFIX - Put in NOPREFIX so ampersand would be properly drawn/calculated
      DrawText (hDC, (char*) ansi.p, iSLen, &r, DT_NOPREFIX | DT_CALCRECT);
#endif

      // convert units
      DWORD i;
      for (i = dwStart; i < dwEndCur; i++) {
         int   iVal;
//         iVal = (i+1 < dwEndCur) ? paiRight[i+1] : s.cx;
         iVal = paiRight[i-dwRealStart];  // BUGFIX - It crashed right here. Think paiRight was too small
         // iVal *= 1;
         iVal += iX;
         paiRight[i-dwRealStart] = iVal;
         *pdwMax = i - dwRealStart + 1;
      }

      if (dwEndCur > dwStart)
         iX = s.cx + iX;  //paiRight[dwEndCur-1];

      // advance
      dwStart = dwEndCur;
   }

   SelectObject (hDC, hOld);
   return TRUE;
}

/***********************************************************************
PointToLine - Given a Y position, returns the line number. -1 if above first
line, pc->iNumLines if below last one

inputs
   PCEscControl      pControl
   PEDIT             pc - info
   int               iY - Y value. In page coords.
returns
   int - line #
*/
static int PointToLine (PCEscControl pControl, PEDIT pc, int iY)
{
   // make iY relative to top of control display
   iY -= (pControl->m_rPosn.top + pc->iBorderSize + pc->iTBMargin);

   // make iY relative to top of text
   iY += pc->iTopLine * (pc->pfi->iAbove + pc->pfi->iBelow + pc->pfi->iLineSpaceWrap);

   // if < 0 then negative
   if (iY < 0)
      return -1;

   // divide by line height
   iY /= (pc->pfi->iAbove + pc->pfi->iBelow + pc->pfi->iLineSpaceWrap);

   if (iY >= pc->iNumLines)
      return pc->iNumLines;

   return iY;
}


/***********************************************************************
LineLen - Returns the line length (in characters
inputs
   PCEscControl      pControl
   PEDIT             pc - info
   DWORD             dwLine - line
   DWORD             *pdwStart - if not NULL, filled in the the starting character
   BOOL              fExcludeCR - If TRUE, excluding the CR or ending space from the len
returns
   DWORD - number of characters in the line, including terminating CR
*/
static DWORD LineLen (PCEscControl pControl, PEDIT pc, DWORD dwLine, DWORD *pdwStart = NULL,
                      BOOL fExcludeCR = FALSE)
{
   LINEINFO *pli;
   DWORD    dwStart;

   pli = (LINEINFO*) pc->plistLine->Get(dwLine);
   if (!pli) {
      if (pdwStart)
         *pdwStart  =0;
      return 0;
   }
   dwStart = pli->dwCount;
   // BUGFIX - Potentially fix crash in edit
   if (dwStart > pc->dwLen)
      dwStart = pc->dwLen;
   if (pdwStart)
      *pdwStart = dwStart;

   pli = (LINEINFO*) pc->plistLine->Get(dwLine+1);
   DWORD dwLen;
   if (pli)
      dwLen = pli->dwCount - dwStart;
   else
      dwLen = pc->dwLen - dwStart;

   // exclude trailing cr/space
   if (fExcludeCR && dwLen) {
      PWSTR psz = (PWSTR)pc->pMem->p;
      if (psz[dwStart + dwLen - 1] == L' ')
         dwLen--;
      else if ((dwLen >= 2) && (psz[dwStart+dwLen-1] == L'\n') && (psz[dwStart+dwLen-2] == L'\r'))
         dwLen-=2;
      else if (psz[dwStart+dwLen-1] == L'\n')
         dwLen--;
   }

   return dwLen;
}


/***********************************************************************
PointToChar - Converts a point to a character. If no line is specified then
it calculates the line.

inputs
   PCEscControl      pControl
   PEDIT             pc - info
   POINT             *pPoint - point
   int               iLine -line. If not specified calculate
   BOOL              fInclusive - If set, then if its not over a character
                        then return the nearest character
   BOOL              fClick - if fClick then clicking for caret, so find closet insertion
returns
   DWORD - character #. -1 if it's not over any character
*/
static DWORD PointToChar (PCEscControl pControl, PEDIT pc, POINT *pPoint, int iLine = -1,
                          BOOL fInclusive = FALSE, BOOL fClick = FALSE)
{
   if (iLine == -1)
      iLine = PointToLine (pControl, pc, pPoint->y);

   // if no line then
   if (iLine < 0)
      return fInclusive ? 0 : (DWORD)-1;
   if (iLine >= pc->iNumLines)
      return fInclusive ? pc->dwLen : (DWORD)-1;

   // given the line, see which character it's over

   // offset into the line
   DWORD dwLineLen, dwLineStart;
   dwLineLen = LineLen (pControl, pc, (DWORD) iLine, &dwLineStart, TRUE);

   // allocate that many integers
   CMem  pMem;
   pMem.Required ((dwLineLen+4) * sizeof(int) * 2);
      // BUGFIX - Added 4 so always some amount
      // BUGFIX - Did x2 because one unicode char could go to two ansi below
      // may fix crash
   int   *pi;
   pi = (int*) pMem.p;

   DWORD dwMax;
   HDC   hDC;
   hDC = pControl->m_pParentPage->m_pWindow->DCGet();
   if (!LineExtent (pControl, pc, (DWORD)iLine, hDC, pi, &dwMax)) {
      pControl->m_pParentPage->m_pWindow->DCRelease();
      return (DWORD)-1; // error
   }
   pControl->m_pParentPage->m_pWindow->DCRelease();

   // offset the X coord
   int   iX;
   iX = pPoint->x - (pControl->m_rPosn.left + pc->iBorderSize + pc->iLRMargin);
   iX += pc->iScrollX;

   if (iX < 0)
      return fInclusive ? dwLineStart : (DWORD) -1;

   if (dwMax && (iX > pi[dwMax-1])) {
      // if last line and click at end, then last character
      if (iLine == (pc->iNumLines-1))
         return fInclusive ? (DWORD)pc->dwLen : (DWORD)-1;

      return fInclusive ? (dwLineStart + dwLineLen /* BUGFIX LineLen - 1*/) : (DWORD)-1;
   }

   // loop
   DWORD i;
   for (i = 0; i < dwMax; i++) {
      if (fClick) {
         if (iX <= (pi[i] + (i ? pi[i-1] : 0))/2)
            return dwLineStart + i;
      }
      else { // want match
         if (iX <= pi[i])
            return dwLineStart + i;
      }
   }

   // else start
   return dwLineStart + dwLineLen; // BUGFIX linelen - 1;
}
   
/***********************************************************************
CharLine - Given a character, return the line

inputs
   PCEscControl      pControl
   PEDIT             pc - info
   DWORD             dwChar - cahracter
returns
   DWORD dwLine - -1 if error
*/
static DWORD CharLine (PCEscControl pControl, PEDIT pc, DWORD dwChar)
{
   if (!pc->dwLen && (dwChar == pc->dwLen))
      return 0;   // line 0

   if (dwChar >= pc->dwLen)
      return (DWORD)-1;

   DWORD i, dwNum;
   LINEINFO *pli;
   dwNum = pc->plistLine->Num();
   for (i = 0; i < dwNum; i++) {
      pli = (PLINEINFO) pc->plistLine->Get(i);
      if (dwChar < pli->dwCount)
         return i - 1;
   }

   // else, max lines
   return dwNum-1;
}



/***********************************************************************
CharPosn - Given a character, find the position

inputs
   PCEscControl      pControl
   PEDIT             pc - info
   HDC               hDC - To paint into
   DWORD             dwChar - cahracter
   RECT              *prChar - Character bounding box
returns
   BOOL - FALSE if beyond edge
*/
static BOOL CharPosn (PCEscControl pControl, PEDIT pc, HDC hDC,
                       DWORD dwChar, RECT *prCaret)
{
   // determine the line
   DWORD dwLine = CharLine (pControl, pc, dwChar);
   if (dwLine == (DWORD)-1)
      return FALSE;

   // offset into the line
   DWORD dwLineLen, dwLineStart;
   dwLineLen = LineLen (pControl, pc, dwLine, &dwLineStart, FALSE);

   // allocate that many integers
   CMem  pMem;
   pMem.Required ((dwLineLen+4) * sizeof(int) * 2);   // BUGFIX - May prevent crash when down-line on last line
      // BUGFIX - Because of unicode conversion may get 2x the characters
   int   *pi;
   pi = (int*) pMem.p;

   DWORD dwMax, dwPos;
   if (!LineExtent (pControl, pc, dwLine, hDC, pi, &dwMax)) {
      if ((dwChar == 0) && (pc->dwLen == 0)) {
         dwMax = 0;
      }
      else  // cand help this
         return FALSE;
   }
   dwPos = dwChar - dwLineStart;

   // fill it in
   prCaret->top = ((int) dwLine - pc->iTopLine) *
      (pc->pfi->iAbove + pc->pfi->iBelow + pc->pfi->iLineSpaceWrap) + pControl->m_rPosn.top +
      pc->iBorderSize + pc->iTBMargin;
   prCaret->bottom = prCaret->top + pc->pfi->iAbove + pc->pfi->iBelow + pc->pfi->iLineSpaceWrap;
   if (dwPos >= dwMax) {
      // at the end of the line
      prCaret->left = prCaret->right = dwMax ? pi[dwMax-1] : 0;
   }
   else {
      prCaret->left = dwPos ? pi[dwPos-1] : 0;
      prCaret->right = pi[dwPos];
   }
   prCaret->left += pControl->m_rPosn.left + pc->iBorderSize + pc->iLRMargin;
   prCaret->right += pControl->m_rPosn.left + pc->iBorderSize + pc->iLRMargin;
   prCaret->left -= pc->iScrollX;
   prCaret->right -= pc->iScrollX;

   return TRUE;
}

/***********************************************************************
SelPosn - Given a character that the selection is before (or pc->dwLen)
   this returns a rectangle of width 0 indicating where the selection goes.

inputs
   PCEscControl      pControl
   PEDIT             pc - info
   HDC               hDC - To paint into
   int               iSel
   RECT              *prCaret - Caret
returns
   BOOL - TRUE if succeded
*/
static BOOL SelPosn (PCEscControl pControl, PEDIT pc, HDC hDC, int iSel, RECT *prCaret)
{
   // find the cahracter position
   RECT  r;
   BOOL  fBeyond = iSel >= (int) pc->dwLen;
   // secial case. If the caret is at the end of the document, AND the last line
   // has length 0 (user just pressed enter) then draw it there
   if ((iSel >= (int) pc->dwLen) && !LineLen (pControl, pc, (DWORD) pc->iNumLines-1, NULL, FALSE)) {
      DWORD dwLine;
      dwLine = (DWORD) max(pc->iNumLines - 1, 0);
      r.top = ((int) dwLine - pc->iTopLine) *
         (pc->pfi->iAbove + pc->pfi->iBelow + pc->pfi->iLineSpaceWrap) + pControl->m_rPosn.top +
         pc->iBorderSize + pc->iTBMargin;
      r.bottom = r.top + pc->pfi->iAbove + pc->pfi->iBelow + pc->pfi->iLineSpaceWrap;
      r.left = r.right = 0;

      r.left += pControl->m_rPosn.left + pc->iBorderSize + pc->iLRMargin;
      r.right += pControl->m_rPosn.left + pc->iBorderSize + pc->iLRMargin;
      r.left -= pc->iScrollX;
      r.right -= pc->iScrollX;

   }
   else if (!CharPosn (pControl, pc, hDC, fBeyond ? (pc->dwLen ? (pc->dwLen -1) : 0) : (DWORD) iSel, &r))
      return FALSE;

   // rectangle
   *prCaret =r;
   if (fBeyond) {
      prCaret->left = prCaret->right;
   }
   else {
      prCaret->right = prCaret->left;
   }
//   prCaret->right++;
//   prCaret->left--;
   prCaret->top -= 2;

   return TRUE;
}


/***********************************************************************
CaretPosn - Returns a rectangle surrounding the carent position in page coords.

inputs
   PCEscControl      pControl
   PEDIT             pc - info
   HDC               hDC - To paint into
   RECT              *prCaret - Caret
returns
   BOOL - TRUE if succeded
*/
static BOOL CaretPosn (PCEscControl pControl, PEDIT pc, HDC hDC, RECT *prCaret)
{
   if (!SelPosn (pControl, pc, hDC, pc->iSelEnd, prCaret))
      return FALSE;

   // caret is wider
   prCaret->right++;
   prCaret->left--;

   return TRUE;
}


/***********************************************************************
PaintSel - Paints the selection.

inputs
   PCEscControl      pControl
   PEDIT             pc - info
   HDC               hDC - To paint into
   POINT             *pOffset - Add to page to get to HDC coords
returns
   none
*/
static void PaintSel (PCEscControl pControl, PEDIT pc, HDC hDC, POINT *pOffset)
{
   // if no selection dont paint
   if (pc->iSelEnd == pc->iSelStart)
      return;

   // find min & max
   int   iMin, iMax;
   iMin = min(pc->iSelEnd, pc->iSelStart);
   iMax = max(pc->iSelEnd, pc->iSelStart);

   // get rectangle
   RECT  rMin, rMax, rControl;
   if (!SelPosn (pControl, pc, hDC, iMin, &rMin))
      return;
   if (!SelPosn (pControl, pc, hDC, iMax, &rMax))
      return;
   rControl = pControl->m_rPosn;
   rControl.left += pc->iBorderSize + 1;
   rControl.right -= (pc->iBorderSize + 2);

   // convert to HDC coords
   OffsetRect (&rMin, pOffset->x, pOffset->y);
   OffsetRect (&rMax, pOffset->x, pOffset->y);
   OffsetRect (&rControl, pOffset->x, pOffset->y);

   // BUGFIX - Limit the min and max to be within the control
   rMin.left = max(rMin.left, rControl.left);
   rMin.top = max(rMin.top, rControl.top);
   rMin.top = min(rMin.top, rControl.bottom);
   rMin.right = min(rMin.right, rControl.right);
   rMin.bottom = min(rMin.bottom, rControl.bottom);
   rMin.bottom = max(rMin.bottom, rControl.top);

   rMax.left = max(rMax.left, rControl.left);
   rMax.top = max(rMax.top, rControl.top);
   rMax.top = min(rMax.top, rControl.bottom);
   rMax.right = min(rMax.right, rControl.right);
   rMax.bottom = min(rMax.bottom, rControl.bottom);
   rMax.bottom = max(rMax.bottom, rControl.top);

   // several cases
   HBRUSH   hbr;
   HPEN     hOld, hPen;
   RECT  r;
   hbr = CreateSolidBrush (pc->cSelection);
   hPen = CreatePen (PS_SOLID, 0, pc->cSelectionBorder);
   hOld = (HPEN) SelectObject (hDC, hPen);

   if (rMin.top == rMax.top) {
      // on the same line. only one box to draw
      r.left = rMin.left - 1;
      r.right = rMax.right + 1;
      r.top = rMin.top;
      r.bottom = rMin.bottom;
      FillRect (hDC, &r, hbr);

      // outline
      MoveToEx (hDC, r.left, r.top, NULL);
      LineTo (hDC, r.right, r.top);
      LineTo (hDC, r.right, r.bottom);
      LineTo (hDC, r.left, r.bottom);
      LineTo (hDC, r.left, r.top);
   }
   else if ((rMin.bottom >= rMax.top) && (rMin.left >= rMax.left)) {
      // on adjacent lines

      // top box
      r = rMin;
      r.left--;;
      r.right = rControl.right;
      FillRect (hDC, &r, hbr);

      // outline
      MoveToEx (hDC, r.left, r.top, NULL);
      LineTo (hDC, r.right, r.top);
      LineTo (hDC, r.right, r.bottom);
      LineTo (hDC, r.left, r.bottom);
      LineTo (hDC, r.left, r.top);

      // bottom box
      r = rMax;
      r.right++;
      r.left = rControl.left;
      FillRect (hDC, &r, hbr);

      // outline
      MoveToEx (hDC, r.left, r.top, NULL);
      LineTo (hDC, r.right, r.top);
      LineTo (hDC, r.right, r.bottom);
      LineTo (hDC, r.left, r.bottom);
      LineTo (hDC, r.left, r.top);
   }
   else {
      // multiple lines in-between
      RECT  rTop, rCenter, rBottom;
      // top box
      rTop = rMin;
      rTop.left--;;
      rTop.right = rControl.right;
      FillRect (hDC, &rTop, hbr);

      // bottom box
      rBottom = rMax;
      rBottom.right++;
      rBottom.left = rControl.left;
      FillRect (hDC, &rBottom, hbr);

      // in-between
      rCenter = rControl;
      rCenter.top = min(rMin.bottom - 1, rMax.top);
      rCenter.bottom = max(rMax.top + 1, rMin.bottom);
      FillRect (hDC, &rCenter, hbr);

      // outline
      MoveToEx (hDC, rTop.left, rCenter.top, NULL);
      LineTo (hDC, rTop.left, rTop.top);
      LineTo (hDC, rTop.right, rTop.top);
      LineTo (hDC, rCenter.right, rCenter.bottom);
      LineTo (hDC, rBottom.right, rCenter.bottom);
      LineTo (hDC, rBottom.right, rBottom.bottom);
      LineTo (hDC, rBottom.left, rBottom.bottom);
      LineTo (hDC, rCenter.left, rCenter.top);
      LineTo (hDC, rTop.left, rCenter.top);

   }
   DeleteObject (hbr);
   SelectObject (hDC, hOld);
   DeleteObject (hPen);
}

/***********************************************************************
ParseNoCRTab - Parses the text range and determines the longest string
   that will fit in. There's no CR or tab.

inputs
   PCEscControl      pControl
   PEDIT             pc - info
   PWSTR             psz - string to parse
   DWORD             dwStart - start location
   DWORD             dwEnd - location of the NULL. dwStart + dwNumChars
   HDC               hDC - DC
   int               iMax - maximum number of pixels usable
   int               *piUsed - amount of space used
   BOOL              fStartLine - if TRUE a the start of a line, else part way through
reutnrs
   DWORD - charcters used
*/
static DWORD ParseNoCRTab (PCEscControl pControl, PEDIT pc, PWSTR psz,
                           DWORD dwStart, DWORD dwEnd, HDC hDC,
                           int iMax, int *piUsed, BOOL fStartLine)
{
   CMem  ansi;
   int   iSLen;
   RECT  r;
   *piUsed = 0;
   if (dwStart == dwEnd)
      return 0;

   // optimize this so calculate the lengths of the whole string and separating
   // points for individual characters, and then break. Askign for line length one
   // word at a time is slow.
   // see if this fits
   ansi.Required ((dwEnd - dwStart + 1) * 2);
   iSLen = (int) WideCharToMultiByte (CP_ACP, 0, psz + dwStart, dwEnd - dwStart,
      (char*) ansi.p, (int)ansi.m_dwAllocated, 0, 0);
   CMem  memInt;
   if (!memInt.Required (iSLen * sizeof(int)))
      return dwEnd - dwStart; // error

   // if password fill with *
   if (pc->fPassword)
      _strnset ((char*) ansi.p, '*', iSLen);

   SetTextAlign (hDC, TA_BASELINE | TA_LEFT);
   SetTextColor (hDC, pc->pfi->cText);
   HFONT hOld;
   hOld = (HFONT) SelectObject (hDC, pc->hFont);
   int   iFit;
   SIZE  s;
   iFit = 0;
   if (!GetTextExtentExPoint (hDC, (char*) ansi.p, iSLen, iMax, &iFit, (int*) memInt.p, &s)) {
      SelectObject (hDC, hOld);
      return dwEnd - dwStart; // error
   }
   SelectObject (hDC, hOld);


   // see how far can go and still fit
   DWORD    dwFitCount = 0;
   int      iFitSize = 0;
   DWORD dwCountTotal, dwCountWord;
   dwCountTotal = dwStart;
   while (TRUE) {
      // BUGFIX - optimize if there's no word wrap
      if (pc->fWordWrap) {
         // see how long the next word is
         for (dwCountWord = dwCountTotal; dwCountWord < dwEnd; dwCountWord++) {
            if ((psz[dwCountWord] == L' ') || (psz[dwCountWord] == L'-')) {
               dwCountWord++;
               break;
            }
         }
      }
      else {
         // no work wrap so just set word count to the end
         dwCountWord = dwEnd;
      }

      // if nothing then done
      if (dwCountWord == dwCountTotal)
         break;

      // see if this fits
      ansi.Required ((dwCountWord - dwStart + 1) * 2);
      iSLen = (int) WideCharToMultiByte (CP_ACP, 0, psz + dwStart, dwCountWord - dwStart,
         (char*) ansi.p, (int)ansi.m_dwAllocated, 0, 0);
      // just use to calculate the length needed

      // just look this up
      if (iSLen > iFit)
         break;   // too large

      // else remember
      dwCountTotal = dwCountWord;
      dwFitCount = dwCountTotal - dwStart;
      iFitSize = ((int*) (memInt.p))[iSLen-1];
   }

   // if didn't fit anything then do 1 character at a time
   if (!dwFitCount && fStartLine) {
      dwCountTotal = dwStart;
      while (TRUE) {
         dwCountTotal++;
         if (dwCountTotal > dwEnd)
            break;

         // see if this fits
         ansi.Required ((dwCountTotal - dwStart + 1) * 2);
         iSLen = (int) WideCharToMultiByte (CP_ACP, 0, psz + dwStart, dwCountTotal - dwStart,
            (char*) ansi.p, (int)ansi.m_dwAllocated, 0, 0);

         // if password fill with *
         if (pc->fPassword)
            _strnset ((char*) ansi.p, '*', iSLen);

         // use drawtext here because it'sa wierd circumstance
         r.left = r.top = 0;
         r.bottom = 10000;
         r.right = 10000;
         hOld = (HFONT) SelectObject (hDC, pc->hFont);
         // BUGFIX - Put in NOPREFIX so ampersand would be properly drawn/calculated
         DrawText (hDC, (char*) ansi.p, iSLen, &r, DT_NOPREFIX | DT_CALCRECT);
         SelectObject (hDC, hOld);

         // if this is the first character then remember
         // if too large then just stop
         if ((r.right >= iMax) && (dwCountTotal > dwStart+1))
            break;

         // else remember
         dwFitCount = dwCountTotal - dwStart;
         iFitSize = r.right;
      }
   }

   // found the fit
   *piUsed = iFitSize;
   return dwFitCount;
}

/***********************************************************************
ParseNoCR - Parses the text range looking to realign the text. There's
no CR in the text, but there may be tabs.

inputs
   PCEscControl      pControl
   PEDIT             pc - info
   PWSTR             psz - string to parse
   DWORD             dwStart - start location
   DWORD             dwEnd - location of the NULL. dwStart + dwNumChars
   HDC               hDC - DC
reutnrs
   DWORD - charcters used
*/
static DWORD ParseNoCR (PCEscControl pControl, PEDIT pc, PWSTR psz, DWORD dwStart, DWORD dwEnd, HDC hDC)
{
   // figure out how much space have to draw. start curX at 0
   int   iMaxX = pControl->m_rPosn.right - pControl->m_rPosn.left - 2 * pc->iBorderSize - 2*pc->iLRMargin;


   // if no word wrap the iMaxX is very large
   if (!pc->fWordWrap)
      iMaxX = 1000000;

   int   iCurX = 0;
   DWORD dwCurLine = dwStart, dwCur;
   DWORD dwUsed;
   int   iUsed;
   BOOL  fWrap;

   // parse the whole text
   while (dwCurLine <= dwEnd) {
      // parse out a line
      dwCur = dwCurLine;
      iCurX = 0;
      fWrap = FALSE;
      while (dwCur < dwEnd) {
         // find the next string that can process as a whole. That means look for a tab
         DWORD i;

         for (i = dwCur; i < dwEnd; i++)
            if (psz[i] == L'\t')
               break;
         if ((i == dwCur) && (i < dwEnd)) {
            // found a tab
            dwUsed = 1;
            iUsed = ((iCurX + pc->pfi->iTab) / pc->pfi->iTab * pc->pfi->iTab) - iCurX;

            if (iUsed > iMaxX - iCurX) {
               dwUsed = 0;
               iUsed = 0;
               fWrap = TRUE;
            }
         }
         else {   // not to end. Found a tab after this
            // first, text in-between
            dwUsed = ParseNoCRTab (pControl, pc, psz, dwCur, i, hDC, iMaxX - iCurX, &iUsed, dwCur == dwCurLine);
            if (dwUsed != (i - dwCur))
               fWrap = TRUE;
         }

         // advance
         dwCur += dwUsed;
         iCurX += iUsed;

         // if wrapped break
         if (fWrap)
            break;
      }
      
      // when get here at end of a line, either because of wrap or out of data
      LINEINFO li;
      li.dwCount = dwCurLine;
      li.iWidth = iCurX;
      pc->plistLine->Add (&li);
      pc->iLineInvalidAfter = (int) pc->plistLine->Num();

      dwCurLine = dwCur;
      if (dwCurLine >= dwEnd)
         break;
   }

   return dwEnd - dwStart;
}

/***********************************************************************
ParseWithCR - Parses the text range looking to realign the text. Handles CR
in the text. Calls ParseNoCR, ultimately modifying pc->plistLine

inputs
   PCEscControl      pControl
   PEDIT             pc - info
   PWSTR             psz - string to parse
   DWORD             dwStart - start location
   DWORD             dwEnd - location of the NULL. dwStart + dwNumChars
reutnrs
   DWORD - charcters used
*/
static DWORD ParseWithCR (PCEscControl pControl, PEDIT pc, PWSTR psz, DWORD dwStart, DWORD dwEnd)
{
   // get the DC
   HDC   hDC = pControl->m_pParentPage->m_pWindow->DCGet();
   HFONT hOld;
   hOld = (HFONT) SelectObject (hDC, pc->hFont);

   DWORD dwRealStart = dwStart;
   DWORD i;
   BOOL  fEndWithCR;
   fEndWithCR = FALSE;
   while (dwStart < dwEnd) {
      for (i = dwStart; i < dwEnd; i++) {
         if ((i+1 < dwEnd) && (psz[i] == L'\r') && (psz[i++] == L'\n')) {
            // \r\n combo
            i++;
            break;
         }

         if (psz[i] == L'\n')
            break;

         // else continue
      }
      if (i < dwEnd) {
         ParseNoCR (pControl, pc, psz, dwStart, i, hDC);
         dwStart = i+1;
         fEndWithCR = TRUE;
      }
      else {   // got to the end and no CR
         ParseNoCR (pControl, pc, psz, dwStart, i, hDC);
         fEndWithCR = FALSE;
         break;
      }
   }

   // if end with a CR, add another line
   if (fEndWithCR) {
      // when get here at end of a line, either because of wrap or out of data
      LINEINFO li;
      li.dwCount = dwEnd;
      li.iWidth = 0;
      pc->plistLine->Add (&li);
      pc->iLineInvalidAfter = (int) pc->plistLine->Num();
   }

   // relase DC
   SelectObject (hDC, hOld);
   pControl->m_pParentPage->m_pWindow->DCRelease();

   return dwEnd - dwRealStart;
}

/***********************************************************************
LinesVisible - Calculates and returns the number of lines visible.

inputs
   PCEscControl      pControl
   PEDIT             pc - info
   BOOL              fFully - if TRUE then only returns the number of lines fully
                     visible, excluding partial lines
returns
   int- total
*/
static int LinesVisible (PCEscControl pControl, PEDIT pc, BOOL fFully = FALSE)
{
   // calclines just ot make sure
   CalcLines (pControl, pc);

   // size
   int   iHeight, iHeightText, iHeightText2;
   iHeight = pControl->m_rPosn.bottom - pControl->m_rPosn.top - 2*pc->iTBMargin - 2*pc->iBorderSize;
   iHeightText = pc->pfi->iAbove + pc->pfi->iBelow;
   iHeightText2 = iHeightText + pc->pfi->iLineSpaceWrap;

   // lines
   int   iLines;
   if (iHeight < iHeightText)
      iLines = fFully ? 0 : 1;
   else if (iHeight == iHeightText)
      iLines = 1;
   else {
      iHeight -= iHeightText;
      iLines = 1 + (iHeight / iHeightText2);

      if (!fFully && (iHeight % iHeightText))
         iLines++;
   }

   return iLines;
}


/***********************************************************************
MiscUpdate - Does miscellaenous update
   - Make sure selection is within range

inputs
   PCEscControl      pControl
   PEDIT             pc - info
*/
static void MiscUpdate (PCEscControl pControl, PEDIT pc)
{
   // do checks to make sure selection is OK & stuff like that
   pc->iTopLine = min(pc->iTopLine, pc->iNumLines - LinesVisible(pControl, pc, TRUE));
   pc->iTopLine = max(0, pc->iTopLine);
   pc->iScrollX = min(pc->iScrollX, pc->iMaxWidth - (pControl->m_rPosn.right - pControl->m_rPosn.left - 2*pc->iBorderSize - 2*pc->iLRMargin));
   pc->iScrollX = max(0, pc->iScrollX);

   pc->iSelEnd = max(0, pc->iSelEnd);
   pc->iSelEnd = min((int)pc->dwLen, pc->iSelEnd);
   pc->iSelStart = max(0, pc->iSelStart);
   pc->iSelStart = min((int)pc->dwLen, pc->iSelStart);
}

/***********************************************************************
UpdateScroll - Updates teh scroll bars to the new info

inputs
   PCEscControl      pControl
   PEDIT             pc - info
*/
static void UpdateScroll (PCEscControl pControl, PEDIT pc)
{
   // find hscroll & vscroll
   if (pc->pszHScroll && !pc->pHScroll) {
      pc->pHScroll = pControl->m_pParentPage->ControlFind (pc->pszHScroll);
      if (pc->pHScroll) {
         pc->pHScroll->AttribSet (L"min", L"0");
         pc->pHScroll->m_pParentControl = pControl;
      }
   }
   if (pc->pszVScroll && !pc->pVScroll) {
      pc->pVScroll = pControl->m_pParentPage->ControlFind (pc->pszVScroll);
      if (pc->pVScroll) {
         pc->pVScroll->AttribSet (L"min", L"0");
         pc->pVScroll->m_pParentControl = pControl;
      }
   }

   // set their new values
   WCHAR szTemp[64];
   if (pc->pHScroll) {
      swprintf (szTemp, L"%d", pc->iMaxWidth);
      pc->pHScroll->AttribSet (L"max", szTemp);
      swprintf (szTemp, L"%d", pControl->m_rPosn.right - pControl->m_rPosn.left -
         2*pc->iBorderSize - 2*pc->iLRMargin);
      pc->pHScroll->AttribSet (L"page", szTemp);
      swprintf (szTemp, L"%d", pc->iScrollX);
      pc->pHScroll->AttribSet (L"pos", szTemp);
   }
   if (pc->pVScroll) {
      swprintf (szTemp, L"%d", LinesVisible (pControl, pc, TRUE));
      pc->pVScroll->AttribSet (L"page", szTemp);
      swprintf (szTemp, L"%d", pc->iNumLines);
      pc->pVScroll->AttribSet (L"max", szTemp);
      swprintf (szTemp, L"%d", pc->iTopLine);
      pc->pVScroll->AttribSet (L"pos", szTemp);
   }

}

/***********************************************************************
CalcLines - The recalculates the line starts from pLineInvalidAfter on.
   It fills in plistLine with the starting character for each line, taking
   into account fWordWrap, fMultiLine, and fPassword. It fills in iNumLines, iMaxWidth,
   plistline, iLineInvalidAfter. It also resets pHScroll and pVScroll.
   Also calls MiscUpdate

  Only works if fReCalc is true

inputs
   PCEscControl      pControl
   PEDIT             pc - info
   BOOL              fForce - if TRUE, force even if pc->fRecalc is FALSE
*/
static void CalcLines (PCEscControl pControl, PEDIT pc, BOOL fForce)
{
   if (!fForce && !pc->fReCalc)
      return;
   if (pc->fReCalc) {
      // all lines invalid
      pc->iLineInvalidAfter = 0;
   }
   pc->fReCalc = FALSE;

   // find out the character number for that line
   DWORD dwChar;
   LINEINFO *pli;
   pli = (PLINEINFO) pc->plistLine->Get((DWORD)pc->iLineInvalidAfter);
   if (pli)
      dwChar = pli->dwCount;
   else {
      pc->plistLine->Truncate (0);
      dwChar = 0;
   }

   // truncate the list to only those valid
   pc->plistLine->Truncate ((DWORD) pc->iLineInvalidAfter);

   // how big is the text
   DWORD   dwLen;
   dwLen = pc->pMem->p ? (DWORD) wcslen((WCHAR*) pc->pMem->p) : NULL;
   pc->dwLen = dwLen;

   // if more text then parse
   if (dwChar < dwLen)
      ParseWithCR (pControl, pc, (WCHAR*) pc->pMem->p, dwChar, dwLen);

   pc->iNumLines = (int) pc->plistLine->Num();
   pc->iLineInvalidAfter = pc->iNumLines;

   // calculate imaxwidth
   pc->iMaxWidth = 0;
   DWORD i;
   for (i = 0; i < (DWORD) pc->iNumLines; i++) {
      pli = (PLINEINFO) pc->plistLine->Get(i);
      pc->iMaxWidth = max(pc->iMaxWidth, pli->iWidth);
   }

   // update scroll bars
   MiscUpdate (pControl, pc);
   UpdateScroll (pControl, pc);
}


/***********************************************************************
ScrollToLine - Scrolls so line N is at the top, and we're horizontally
scrolled by M.

inputs
   PCEscControl
   PEDIT
   int               iY - Line number that's on top
   int               iX - X scroll position
*/
static void ScrollToLine (PCEscControl pControl, PEDIT pc, int iX, int iY)
{
   iY = min(iY, pc->iNumLines - LinesVisible(pControl, pc, TRUE));
   iY = max(0, iY);
   iX = min(iX, pc->iMaxWidth - (pControl->m_rPosn.right - pControl->m_rPosn.left - 2*pc->iBorderSize - 2*pc->iLRMargin));
   iX = max(0, iX);

   // change
   pc->iTopLine = iY;
   pc->iScrollX = iX;
   UpdateScroll (pControl, pc);
   pControl->Invalidate ();
}

/***********************************************************************
CaretVisible - Scrolls the edit box such that the caret is visible. iSelEnd.
*/
static void CaretVisible (PCEscControl pControl, PEDIT pc)
{
   HDC   hDC = pControl->m_pParentPage->m_pWindow->DCGet();
   // make sure have correc tlines
   CalcLines (pControl, pc);

   // get the caret line and X index
   int   iLine;
   if (pc->iSelEnd < (int) pc->dwLen)
      iLine = CharLine (pControl, pc, pc->iSelEnd);
   else
      iLine = pc->iNumLines - 1;

   RECT  rc;
   CaretPosn (pControl, pc, hDC, &rc);

   pControl->m_pParentPage->m_pWindow->DCRelease();

   // if this line is visible then do nothing
   int   iBottom;
   iBottom = pc->iTopLine + LinesVisible (pControl, pc, TRUE);
   if ((iLine >= pc->iTopLine) && (iLine < iBottom) && (rc.left >= pControl->m_rPosn.left) && (rc.right <= pControl->m_rPosn.right))
      return;

   // else need to scroll to a page
   int   iScrollToY, iScrollToX;
   iScrollToY = pc->iTopLine;
   iScrollToX = pc->iScrollX;
   if (iLine < pc->iTopLine)
      iScrollToY = iLine;
   if (iLine >= iBottom)
      iScrollToY = iLine - (LinesVisible (pControl, pc, TRUE) - 1);
   int   iLeft, iRight;
   iLeft = pControl->m_rPosn.left + pc->iBorderSize + pc->iLRMargin;
   iRight = pControl->m_rPosn.right - pc->iBorderSize - pc->iLRMargin;
   if (rc.left < iLeft)
      iScrollToX -= (iLeft - rc.left);
   if (rc.right > iRight)
      iScrollToX += (rc.right - iRight);
   if (iScrollToX < 0)
      iScrollToX = 0;

   // scroll
   ScrollToLine (pControl, pc, iScrollToX, iScrollToY);

}


/***********************************************************************
UndoRedoAdd - Adds a node to the beginning of undo/redo.

inputs
   PCListVariable plist - undo or redo list
   int   iSelStart, iSelEnd - selection info at the time
   DWORD    dwStartReplace - start of the replacement
   DWORD    dwLenReplaced - length replaced
   WCHAR *psz - string - that's replaced
   DWORD dwLenReplaceWith - length of the string that are replaceing with
returns
   BOOL - success
*/
static BOOL UndoRedoAdd (PCListVariable plist, int iSelStart, int iSelEnd,
                         DWORD dwStartReplace, DWORD dwLenReplaced,
                         WCHAR *psz, DWORD dwLenReplaceWith)
{
   DWORD dwLen = dwLenReplaced;
   CMem  mem;
   DWORD dwSize;
   if (!mem.Required (dwSize = sizeof(UNDOINFO) + dwLen*2))
      return FALSE;

   // copy
   PUNDOINFO   pi;
   pi = (PUNDOINFO) mem.p;
   pi->iOldSelStart= iSelStart;
   pi->iOldSelEnd = iSelEnd;
   pi->dwStartReplace = dwStartReplace;
   pi->dwLenReplaced = dwLen;
   pi->dwLenReplaceWith = dwLenReplaceWith;
   memcpy ((WCHAR*) (pi+1), psz, dwLen*2);

   BOOL fRet;
   fRet = plist->Insert (0, pi, dwSize);
   if (!fRet)
      return fRet;

   // if too many elemenets delete last one
   if (plist->Num() > 256)
      plist->Remove (plist->Num()-1);

   // done
   return TRUE;
}

/***********************************************************************
ReplaceSel - Replace the selection with new text

inputs
   PCEscControl      pControl
   PEDIT             pc
   PWSTR             psz - Replace with
   DWORD             dwLen - # of characters
   BOOL              fSaveForUndo - if TRUE save for Undo. Else don't touch undo/redo
reutnrs
   none
*/
static void ReplaceSel (PCEscControl pControl, PEDIT pc, PWSTR psz, DWORD dwLen,
                        BOOL fSaveForUndo = TRUE)
{
   // if it's read only then fail
   if (pc->fReadOnly) {
      pControl->m_pParentPage->m_pWindow->Beep (ESCBEEP_DONTCLICK);
      return;
   }

   CalcLines (pControl, pc);

   // remember how many lines
   int   iOldLines;
   iOldLines = (int) pc->plistLine->Num();

   // BUGFIX - So don't crash when there's an really huge selection
   pc->iSelStart = min(pc->iSelStart, (int)pc->dwLen);
   pc->iSelEnd = min(pc->iSelEnd, (int)pc->dwLen);

   // find min & max
   int   iMin, iMax;
   iMin = min(pc->iSelStart, pc->iSelEnd);
   iMax = max(pc->iSelStart, pc->iSelEnd);
   iMin = max(0,iMin);
   iMax = max(0,iMax);
   iMin = min(iMin, (int)pc->dwLen);
   iMax = min(iMax, (int)pc->dwLen);
   iMax = max(iMin, iMax);

   // how much space is needed
   int   iNeed;
   iNeed = (int) pc->dwLen - (iMax - iMin) + (int) dwLen + 1;
   if (iNeed > pc->iMaxChars) {
      // won't fit in
      pControl->m_pParentPage->m_pWindow->Beep (ESCBEEP_DONTCLICK);
      return;
   }

   // add this to the undo list
   if (fSaveForUndo && (dwLen || (iMin != iMax)) ) {
      // save into undo
      // BUGFIX - so doesn't crash on paste replace all
      pc->iSelEnd = min(pc->iSelEnd, (int) pc->dwLen);
      UndoRedoAdd (pc->plistUndo, pc->iSelStart, pc->iSelEnd,
         (DWORD) min(pc->iSelStart, pc->iSelEnd),
         (DWORD) abs(pc->iSelStart - pc->iSelEnd),
         ((WCHAR*) pc->pMem->p) + iMin, dwLen);

      // clear redo
      pc->plistRedo->Clear();
   }

   // remember what lines
   DWORD dwLine;
   dwLine = CharLine(pControl, pc, (DWORD)iMin);
   if (dwLine == (DWORD)-1)
      dwLine = (DWORD)pc->iNumLines-1;
   // take off one more since changing this can sometimes affect word wrap above
   if (dwLine)
      dwLine--;

   // how much is required
   if (!pc->pMem->Required(iNeed * 2 + 4))   // BUGFIX - A bit extra just in case
      return;
   PWSTR p;
   p = (PWSTR) pc->pMem->p;
   p[pc->dwLen] = 0;  // null terminate

   // delete the old stuff
   if (iMax > iMin)
      memmove ( p + iMin, p+iMax, ((int) pc->dwLen - iMax + 1) * 2);

   // insert the new
   if (dwLen) {
      memmove (p + (iMin + (int) dwLen), p + iMin, ((int) pc->dwLen - iMax + 1) * 2);
      memcpy (p + iMin, psz, dwLen * 2);
   }

   // new selection
   pc->iSelStart = pc->iSelEnd = iMin + (int) dwLen;

   // everything after this is invalid
   RECT  rInvalid;
   rInvalid = pControl->m_rPosn;
   rInvalid.top = ((int) dwLine - pc->iTopLine) *
      (pc->pfi->iAbove + pc->pfi->iBelow + pc->pfi->iLineSpaceWrap) + pControl->m_rPosn.top;
   if (rInvalid.top < rInvalid.bottom)
      pControl->Invalidate(&rInvalid);
   pc->iLineInvalidAfter = (int) dwLine;
   CalcLines (pControl, pc, TRUE);

   // make sure the caret's visible
   CaretVisible (pControl, pc);

   // set the dirty flag
   pc->fDirty = TRUE;

   // BUGFIX - if there are fewer lines than before then invalidate everything
   // otherwise if deleted at end had drawing problems
   if (iOldLines > (int) pc->plistLine->Num()) {
      pControl->Invalidate();
   }

   // send change notification
   ESCNEDITCHANGE ec;
   ec.pControl = pControl;
   pControl->MessageToParent (ESCN_EDITCHANGE, &ec);
}




/***********************************************************************
StoreUndo - Copies the current text into the undo buffer. This should
be called periodically so user can undo. Leaves at least 10(?) seconds
of changes in the undo buffer.
*/

/***********************************************************************
SendChange - Sends an ESCN_EDITCHANGE notification
*/

/***********************************************************************
Control callback
*/
BOOL ControlEdit (PCEscControl pControl, DWORD dwMessage, PVOID pParam)
{
   EDIT  *pc = (EDIT*) pControl->m_mem.p;
   switch (dwMessage) {
   case ESCM_CONSTRUCTOR:
      {
         pControl->m_mem.Required (sizeof(EDIT));
         pc = (EDIT*) pControl->m_mem.p;
         memset (pc, 0, sizeof(EDIT));
         pc->pMem = new CMem;
         pc->plistLine = new CListFixed;
         if (pc->plistLine) {
            pc->plistLine->Init (sizeof(LINEINFO));

            // the first line always starts at 0
            LINEINFO li;
            memset (&li, 0, sizeof(li));
            pc->plistLine->Add (&li);
         }
         pc->plistUndo = new CListVariable;
         pc->plistRedo = new CListVariable;
         pc->iLRMargin = pc->iTBMargin = 4;
         pc->iBorderSize = 2;
         pc->cBackground = RGB(0xe0, 0xe0, 0xff); // (DWORD)-1;  // transparent
         pc->cBorder = RGB(0, 0, 0xff);
         pc->cSelection = RGB(0xc0, 0xc0, 0xc0);
         pc->cSelectionBorder = RGB(0,0,0);
         pc->cCaret= RGB(0xff, 0, 0);
         pc->cCaretBlink = RGB(0xc0, 0xc0, 0xc0);
         pc->fMultiLine = FALSE;
         pc->fWordWrap = FALSE;
         pc->fReadOnly = FALSE;
         pc->fSelAll = FALSE;
         pc->fPreLineIndent = FALSE;
         pc->fCaptureTab = FALSE;
         pc->fReCalc = TRUE;
         pc->dwSubTimer = 0;
         pc->iMaxChars = 1000000;
         pc->fDirty = FALSE;
         pc->dwFocusSet = 0;
         pc->fIgnoreNextButton = FALSE;

         // all the attributes
         pControl->AttribListAddCMem (L"text", pc->pMem, &pc->fReCalc, TRUE);
         pControl->AttribListAddDecimal (L"LRMargin", &pc->iLRMargin, &pc->fReCalc, TRUE);
         pControl->AttribListAddDecimal (L"TBMargin", &pc->iTBMargin, &pc->fReCalc, TRUE);
         pControl->AttribListAddDecimal (L"border", &pc->iBorderSize, &pc->fReCalc, TRUE);
         pControl->AttribListAddColor (L"bordercolor", &pc->cBorder, NULL, TRUE);
         pControl->AttribListAddColor (L"color", &pc->cBackground, NULL, TRUE);
         pControl->AttribListAddDecimal (L"topline", &pc->iTopLine, NULL, TRUE, ESCM_NEWSCROLL);
         pControl->AttribListAddDecimal (L"scrollx", &pc->iScrollX, NULL, TRUE, ESCM_NEWSCROLL);
         pControl->AttribListAddDecimal (L"selstart", &pc->iSelStart, NULL, TRUE);
         pControl->AttribListAddDecimal (L"selend", &pc->iSelEnd, NULL, TRUE);
         pControl->AttribListAddString (L"hscroll", &pc->pszHScroll, NULL, TRUE);
         pControl->AttribListAddString (L"vscroll", &pc->pszVScroll, NULL, TRUE);
         pControl->AttribListAddColor (L"selcolor", &pc->cSelection, NULL, TRUE);
         pControl->AttribListAddColor (L"selbordercolor", &pc->cSelectionBorder, NULL, TRUE);
         pControl->AttribListAddColor (L"caretcolor", &pc->cCaret, NULL, TRUE);
         pControl->AttribListAddColor (L"caretblinkcolor", &pc->cCaretBlink, NULL, TRUE);
         pControl->AttribListAddDecimal (L"maxchars", &pc->iMaxChars);
         pControl->AttribListAddBOOL (L"multiline", &pc->fMultiLine, &pc->fReCalc, TRUE);
         pControl->AttribListAddBOOL (L"wordwrap", &pc->fWordWrap, &pc->fReCalc, TRUE);
         pControl->AttribListAddBOOL (L"password", &pc->fPassword, &pc->fReCalc, TRUE);
         pControl->AttribListAddBOOL (L"prelineindent", &pc->fPreLineIndent, &pc->fReCalc, TRUE);
         pControl->AttribListAddBOOL (L"capturetab", &pc->fCaptureTab, NULL, FALSE);
         pControl->AttribListAddBOOL (L"dirty", &pc->fDirty, NULL, FALSE);
         pControl->AttribListAddBOOL (L"readonly", &pc->fReadOnly, NULL, FALSE);
         pControl->AttribListAddBOOL (L"selall", &pc->fSelAll, NULL, FALSE);

      }
      return TRUE;

   case ESCM_DESTRUCTOR:
      if (pc->pMem)
         delete pc->pMem;
      if (pc->plistLine)
         delete pc->plistLine;
      if (pc->plistUndo)
         delete pc->plistUndo;
      if (pc->plistRedo)
         delete pc->plistRedo;
      return TRUE;

   case ESCM_INITCONTROL:
      {
         HDC   hDC = pControl->m_pParentPage->m_pWindow->DCGet();

         // create hfont to draw with
         pc->pfi = pControl->m_FontCache.Need (hDC, &pControl->m_fi.fi,
            pControl->m_fi.iPointSize, pControl->m_fi.dwFlags,
            pControl->m_fi.szFont);
         pc->hFont = pc->pfi ? pc->pfi->hFont : NULL;
         if (pc->pfi)
            pc->pfi->iTab = max(pc->pfi->iTab, 1);

         // release DC
         pControl->m_pParentPage->m_pWindow->DCRelease();

         // note that want keyboard
         pControl->m_fWantMouse = TRUE;
         pControl->m_dwWantFocus = 2;

         // add acceleartors
         ESCACCELERATOR a;
         memset (&a, 0, sizeof(a));
         a.c = VK_LEFT;
         a.dwMessage = ESCM_LEFTARROW;
         pControl->m_listAccelFocus.Add (&a);

         a.dwMessage = ESCM_SHIFTLEFTARROW;
         a.fShift = TRUE;
         pControl->m_listAccelFocus.Add (&a);
         a.fShift = FALSE;

         a.dwMessage = ESCM_CTRLLEFTARROW;
         a.fControl = TRUE;
         pControl->m_listAccelFocus.Add (&a);
         a.fControl = FALSE;

         a.dwMessage = ESCM_CTRLSHIFTLEFTARROW;
         a.fShift = TRUE;
         a.fControl = TRUE;
         pControl->m_listAccelFocus.Add (&a);
         a.fShift = FALSE;
         a.fControl = FALSE;

         a.c = VK_RIGHT;
         a.dwMessage = ESCM_RIGHTARROW;
         pControl->m_listAccelFocus.Add (&a);

         a.dwMessage = ESCM_SHIFTRIGHTARROW;
         a.fShift = TRUE;
         pControl->m_listAccelFocus.Add (&a);
         a.fShift = FALSE;

         a.dwMessage = ESCM_CTRLRIGHTARROW;
         a.fControl = TRUE;
         pControl->m_listAccelFocus.Add (&a);
         a.fControl = FALSE;

         a.dwMessage = ESCM_CTRLSHIFTRIGHTARROW;
         a.fShift = TRUE;
         a.fControl = TRUE;
         pControl->m_listAccelFocus.Add (&a);
         a.fShift = FALSE;
         a.fControl = FALSE;

         a.c = VK_HOME;
         a.dwMessage = ESCM_HOME;
         pControl->m_listAccelFocus.Add (&a);

         a.dwMessage = ESCM_SHIFTHOME;
         a.fShift = TRUE;
         pControl->m_listAccelFocus.Add (&a);
         a.fShift = FALSE;

         a.c = VK_END;
         a.dwMessage = ESCM_END;
         pControl->m_listAccelFocus.Add (&a);

         a.dwMessage = ESCM_SHIFTEND;
         a.fShift = TRUE;
         pControl->m_listAccelFocus.Add (&a);
         a.fShift = FALSE;

         a.c = VK_HOME;
         a.fControl = TRUE;
         a.dwMessage = ESCM_CTRLHOME;
         pControl->m_listAccelFocus.Add (&a);

         a.dwMessage = ESCM_SHIFTCTRLHOME;
         a.fShift = TRUE;
         pControl->m_listAccelFocus.Add (&a);
         a.fShift = FALSE;
         a.fControl = FALSE;

         a.c = VK_END;
         a.fControl = TRUE;
         a.dwMessage = ESCM_CTRLEND;
         pControl->m_listAccelFocus.Add (&a);

         a.dwMessage = ESCM_SHIFTCTRLEND;
         a.fShift = TRUE;
         pControl->m_listAccelFocus.Add (&a);
         a.fShift = FALSE;
         a.fControl = FALSE;

         if (pc->fMultiLine) {
            a.c = VK_UP;
            a.dwMessage = ESCM_LINEUP;
            pControl->m_listAccelFocus.Add (&a);

            a.dwMessage = ESCM_SHIFTLINEUP;
            a.fShift = TRUE;
            pControl->m_listAccelFocus.Add (&a);
            a.fShift = FALSE;

            a.c = VK_DOWN;
            a.dwMessage = ESCM_LINEDOWN;
            pControl->m_listAccelFocus.Add (&a);

            a.dwMessage = ESCM_SHIFTLINEDOWN;
            a.fShift = TRUE;
            pControl->m_listAccelFocus.Add (&a);
            a.fShift = FALSE;

            a.c = VK_PRIOR;
            a.dwMessage = ESCM_PAGEUP;
            pControl->m_listAccelFocus.Add (&a);

            a.dwMessage = ESCM_SHIFTPAGEUP;
            a.fShift = TRUE;
            pControl->m_listAccelFocus.Add (&a);
            a.fShift = FALSE;

            a.c = VK_NEXT;
            a.dwMessage = ESCM_PAGEDOWN;
            pControl->m_listAccelFocus.Add (&a);

            a.dwMessage = ESCM_SHIFTPAGEDOWN;
            a.fShift = TRUE;
            pControl->m_listAccelFocus.Add (&a);
            a.fShift = FALSE;

         }

         // accelerators for backspace and delete
         a.dwMessage = ESCM_BACKSPACE;
         a.c = 8;
         pControl->m_listAccelFocus.Add (&a);
         a.dwMessage = ESCM_DEL;
         a.c = VK_DELETE;
         pControl->m_listAccelFocus.Add (&a);

         // cut, copy, paste
         a.dwMessage = ESCM_EDITCUT;
         a.c = 24; //L'X';
         a.fControl = TRUE;
         pControl->m_listAccelFocus.Add (&a);
         a.dwMessage = ESCM_EDITCOPY;
         a.c = 3; //L'C';
         pControl->m_listAccelFocus.Add (&a);
         a.dwMessage = ESCM_EDITPASTE;
         a.c = 22; //L'V';
         pControl->m_listAccelFocus.Add (&a);
         a.fControl = FALSE;

         // select all
         a.dwMessage = ESCM_SELECTALL;
         a.c = 1; //L'A';
         a.fControl = TRUE;
         pControl->m_listAccelFocus.Add (&a);
         a.fControl = FALSE;


         // accelerators for undo, redo
         a.dwMessage = ESCM_EDITUNDO;
         a.c = 26; //L'z';
         a.fControl = TRUE;
         pControl->m_listAccelFocus.Add (&a);
         a.dwMessage = ESCM_EDITREDO;
         a.c = 25; //L'y';
         pControl->m_listAccelFocus.Add (&a);
         a.fControl = FALSE;
      }
      return TRUE;

   case ESCM_QUERYSIZE:
      {
         PESCMQUERYSIZE p = (PESCMQUERYSIZE) pParam;

         // if had ruthers...
         p->iHeight = pc->iBorderSize * 2 + pc->iTBMargin * 2 + pc->pfi->iAbove +
            pc->pfi->iBelow;  // one line tall
         // if multiline request something 3 lines tall?
         if (pc->fMultiLine)
            p->iHeight += (pc->pfi->iAbove + pc->pfi->iBelow + pc->pfi->iLineSpaceWrap)*2;
         p->iWidth = p->iDisplayWidth / 3; // 1/3 of screen
      }
      return FALSE;  // pass onto default handler

   case ESCM_SIZE:
      // recalc
      pc->fReCalc = TRUE;
      return FALSE;  // default handler

   case ESCM_FOCUS:
      // BUGFIX - Don't create the timer until get focus. Kill it when lose focus
      if (pControl->m_fFocus)
         // set timer for caret
         pControl->TimerSet (GetCaretBlinkTime() / 2);
      else
         pControl->TimerKill();


      // BUGFIX - If SelAll= set to true then select the entire text string
      if (pc->fSelAll && pControl->m_fFocus) {
#ifdef _DEBUG
         OutputDebugString ("ESCM_FOCUS:SelAll\r\n");
#endif
         pControl->Message (ESCM_SELECTALL);
         pc->dwFocusSet = GetTickCount();
         pc->fIgnoreNextButton = FALSE;
      }
      break;   // fall on through to default handler

   case ESCM_CONTROLTIMER:
      {
         // dont bother if doesn't have focus because won't blink
         if (!pControl->m_fFocus)
            return TRUE;

         // if mouse outside rect then scroll
         if (!pControl->m_fMouseOver && pControl->m_fLButtonDown) {
            int   iX, iY;
            iX = pc->iScrollX;
            iY = pc->iTopLine;

            if (pc->pLastMouse.x < pControl->m_rPosn.left)
               iX -= (pControl->m_rPosn.right - pControl->m_rPosn.left) / 16;
            if (pc->pLastMouse.x > pControl->m_rPosn.right)
               iX += (pControl->m_rPosn.right - pControl->m_rPosn.left) / 16;
            if (pc->pLastMouse.y < pControl->m_rPosn.top)
               iY -= 1;
            if (pc->pLastMouse.y > pControl->m_rPosn.bottom)
               iY += 1;

            ScrollToLine (pControl, pc, iX, iY);
         }

         pc->dwSubTimer = pc->dwSubTimer + 1;

         // only blink every other subtimer
         if (!(pc->dwSubTimer % 2))
            return TRUE;

         // blink the caret
         pc->fCaretBlink = !pc->fCaretBlink;

         // invalidate this
         RECT  r;
         HDC   hDC;
         hDC = pControl->m_pParentPage->m_pWindow->DCGet();
         if (CaretPosn (pControl, pc, hDC, &r))
            pControl->Invalidate (&r);
         pControl->m_pParentPage->m_pWindow->DCRelease();
      }
      return TRUE;

   case ESCM_PAINT:
      {
         PESCMPAINT p = (PESCMPAINT) pParam;

         // figure out invalid
         RECT  rInvalid;
         if (!IntersectRect (&rInvalid, &p->rControlHDC, &p->rInvalidHDC))
            return TRUE;  // nothing invalid

         // recalc if neede
         CalcLines (pControl, pc);

         // paint the background
         if (pc->cBackground != (DWORD)-1) {
            HBRUSH hbr;
            hbr = CreateSolidBrush (pc->cBackground);
            FillRect (p->hDC, &p->rControlHDC, hbr);
            DeleteObject (hbr);
         }


         // paint selection
         POINT pOffset;
         pOffset.x = p->rControlHDC.left - p->rControlPage.left;
         pOffset.y = p->rControlHDC.top - p->rControlPage.top;
         PaintSel (pControl, pc, p->hDC, &pOffset);

         // paint text
         int   iLines;
         POINT point;
         iLines = LinesVisible (pControl, pc);
         point.x = p->rControlHDC.left + pc->iLRMargin + pc->iBorderSize - pc->iScrollX;
         point.y = p->rControlHDC.top + pc->iTBMargin + pc->iBorderSize;
         int   i;
         int   iNext;
         for (i = 0; i < iLines; i++, point.y = iNext) {
            iNext = point.y + pc->pfi->iAbove + pc->pfi->iBelow + pc->pfi->iLineSpaceWrap;

            if ((point.y > rInvalid.bottom) || (iNext < rInvalid.top))
               continue;

            PaintLine (pControl, pc, (DWORD) (i + pc->iTopLine), &point, p->hDC, &p->rControlHDC);
         }


         // paint caret
         RECT  r;
         COLORREF cr;
         // change caret color depending on blink state & focus
         cr = (pControl->m_fFocus && !pc->fCaretBlink) ? pc->cCaret : pc->cCaretBlink;
         if (CaretPosn (pControl, pc, p->hDC, &r)) {
            RECT  r2;
            if (IntersectRect (&r2, &r, &p->rControlPage)) {
               HBRUSH hbr;
               OffsetRect (&r2, p->rControlHDC.left - p->rControlPage.left,
                  p->rControlHDC.top - p->rControlPage.top);
               hbr = CreateSolidBrush (cr);
               FillRect (p->hDC, &r2, hbr);
               DeleteObject (hbr);
            }
         }

         // draw border
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

   case ESCM_CHAR:
      {
         PESCMCHAR p = (PESCMCHAR) pParam;

         // BUGFIX - if alt and control are pressed down then keep for hungarian AltGr
         BOOL fAlt, fControl;
         fAlt = (p->lKeyData & (1<<29)) ? TRUE : FALSE;
         fControl = (GetKeyState (VK_CONTROL) < 0);
         if (fAlt && fControl) {
            // this space intentioanlly left blank
         }
         else {
            // if alt or control then ignore
            if (fAlt || fControl)
               return FALSE;
         }

         // BUGFIX - Escape
         if (p->wCharCode == VK_ESCAPE)
            return FALSE;  // dont eat

         // only capture tabs if have flag set
         if (!pc->fCaptureTab && (p->wCharCode == L'\t'))
            return FALSE;

         // if backspace ignore
         if (p->wCharCode == 8) {
            p->fEaten = TRUE;
            return TRUE;
         }

         // if the cusor is over the edit control then hide
         if (pControl->m_fMouseOver)
            while (ShowCursor (FALSE) >= 0);

         WCHAR c;
         c = p->wCharCode;
         if (c == 13) {
            // if this is not multiline then ignore
            if (!pc->fMultiLine)
               return FALSE;

            // look on the current line and see how many spaces/tabs
            if (pc->fPreLineIndent) {
               CMem  pCR;
               DWORD dwSpace = 0;

               int   iMin;
               DWORD dwLine;
               iMin = min(pc->iSelStart, pc->iSelEnd);
               dwLine = CharLine (pControl, pc, (DWORD) iMin);
               if (dwLine == (DWORD)-1)
                  dwLine = (DWORD) pc->iNumLines - 1;

               // start of line
               DWORD dwStart, dwLen, dwMax;
               dwLen = LineLen (pControl, pc, dwLine, &dwStart, TRUE);
               dwMax = min(pc->dwLen, dwStart + dwLen);
               dwMax = min(dwMax, (DWORD) iMin);

               // how long for space
               PWSTR psz;
               psz = (WCHAR*) pc->pMem->p;
               for (dwSpace = 0; (dwSpace + dwStart) < dwMax; dwSpace++)
                  if ((psz[dwSpace+dwStart] != L'\t') && (psz[dwSpace+dwStart] != L' '))
                     break;

               // duplicate this
               pCR.Required ((dwSpace+2)*2);
               PWSTR p2;
               p2 = (PWSTR) pCR.p;
               p2[0] = L'\r';
               p2[1] = L'\n';
               memcpy (p2+2, psz + dwStart, dwSpace * 2);
               ReplaceSel (pControl, pc, p2, dwSpace+2);
               p->fEaten = TRUE;
               return TRUE;
            }
            // else, just replace with CR

            ReplaceSel (pControl, pc, L"\r\n", 2);
            p->fEaten = TRUE;
            return TRUE;
         }

         ReplaceSel (pControl, pc, &c, 1);

         p->fEaten = TRUE;
      }
      return TRUE;

   case ESCM_NEWSCROLL:
      // sent when app changes the scroll position
      CalcLines (pControl, pc);
      ScrollToLine (pControl, pc, pc->iScrollX, pc->iTopLine);
      return TRUE;

   case ESCM_MOUSEWHEEL:
      {
         PESCMMOUSEWHEEL p = (PESCMMOUSEWHEEL) pParam;

         // BUGFIX - If edit has focus then scroll

         // ignore if not multiline
         if (!pc->fMultiLine)
            return FALSE;

         // BUGFIX - Double the speed, from 1 to 2
         if (p->zDelta < 0)
            ScrollToLine (pControl, pc, pc->iScrollX, pc->iTopLine + 1*2);
         else
            ScrollToLine (pControl, pc, pc->iScrollX, pc->iTopLine - 1*2);

      }
      return TRUE;

   case ESCN_SCROLL:
   case ESCN_SCROLLING:
      {
         PESCNSCROLL p = (PESCNSCROLL) pParam;

         if (p->pControl == pc->pHScroll) {
            pc->iScrollX = p->iPos;
            pc->fReCalc = TRUE;
            pControl->Invalidate();
         }
         else if (p->pControl == pc->pVScroll) {
            pc->iTopLine = p->iPos;
            pc->fReCalc = TRUE;
            pControl->Invalidate();
         }
      }
      return TRUE;

   case ESCM_RBUTTONDOWN:
      {
         // BUGFIX - Menu if right button down

         PESCMLBUTTONDOWN p = (PESCMLBUTTONDOWN) pParam;
         // release the button
         pControl->m_pParentPage->MouseCaptureRelease (pControl);

         // figure out what can and can't do
         // calclines just in case
         CalcLines (pControl, pc);

         BOOL fCanUndo = pc->plistUndo->Num() ? TRUE : FALSE;
         BOOL fCutCopy = !((pc->iSelStart == pc->iSelEnd) || (pc->fPassword));
         BOOL fSelectAll = (pc->iSelStart != 0) || (pc->iSelEnd < (int)pc->dwLen);


         // create the menu
         HMENU hMenu;
         hMenu = CreatePopupMenu ();
         AppendMenu (hMenu, MF_STRING | (fCanUndo ? MF_ENABLED : (MF_DISABLED | MF_GRAYED)),
            ESCM_EDITUNDO, "&Undo");
         AppendMenu (hMenu, MF_SEPARATOR,0,0);
         AppendMenu (hMenu, MF_STRING | (fCutCopy ? MF_ENABLED : (MF_DISABLED | MF_GRAYED)),
            ESCM_EDITCUT, "Cu&t");
         AppendMenu (hMenu, MF_STRING | (fCutCopy ? MF_ENABLED : (MF_DISABLED | MF_GRAYED)),
            ESCM_EDITCOPY, "&Copy");
         AppendMenu (hMenu, MF_STRING | MF_ENABLED,
            ESCM_EDITPASTE, "&Paste");
         AppendMenu (hMenu, MF_STRING | (fCutCopy ? MF_ENABLED : (MF_DISABLED | MF_GRAYED)),
            ESCM_DEL, "&Delete");
         AppendMenu (hMenu, MF_SEPARATOR,0,0);
         AppendMenu (hMenu, MF_STRING | (fSelectAll ? MF_ENABLED : (MF_DISABLED | MF_GRAYED)),
            ESCM_SELECTALL, "Select &All");

         POINT screen;
         pControl->CoordPageToScreen (&p->pPosn, &screen);
         DWORD dwCmd;
         dwCmd = TrackPopupMenu (hMenu, TPM_LEFTALIGN | TPM_TOPALIGN | TPM_RETURNCMD,
            screen.x, screen.y, 0, pControl->m_pParentPage->m_pWindow->m_hWnd, NULL);

         switch (dwCmd) {
         case 0:
            break;   // do nothing
         case 27: // select all
            pControl->Message (ESCM_SELECTALL);
            break;
         case 26:
            pControl->Message (ESCM_DEL);
            break;
         default:
            pControl->Message (dwCmd);
            break;
         }


         DestroyMenu (hMenu);
      }
      return TRUE;

   case ESCM_LBUTTONDOWN:
      {
         PESCMLBUTTONDOWN p = (PESCMLBUTTONDOWN) pParam;
         pc->pLastMouse = p->pPosn;

         // if fselall and just clicked then ignore this
         if (pc->fSelAll && pc->dwFocusSet && (GetTickCount() < pc->dwFocusSet+250)) {
            pc->dwFocusSet = 0;  // reset
            pc->fIgnoreNextButton = TRUE; // ignore this
            return TRUE;
         }
         pc->dwFocusSet = 0;  // reset just in case

         DWORD dwChar;
         dwChar = PointToChar (pControl, pc, &p->pPosn, -1, TRUE, TRUE);
         if (dwChar == (DWORD)-1)
            return FALSE;  // shouldn't happen

         pc->iSelEnd = (int) dwChar;

         // if shift down then extend selection
         if (GetKeyState (VK_SHIFT) >= 0)
            pc->iSelStart = pc->iSelEnd;

         pControl->Invalidate ();

         pControl->m_pParentPage->m_pWindow->Beep (ESCBEEP_BUTTONDOWN);
      }
      return TRUE;

   case ESCM_LBUTTONUP:
      // if ignoring because of selall then do so
      if (pc->fIgnoreNextButton) {
         pc->fIgnoreNextButton = FALSE;
         return TRUE;
      }

      if (pc->iSelStart != pc->iSelEnd)
         pControl->m_pParentPage->m_pWindow->Beep(ESCBEEP_SCROLLDRAGSTOP);

      return true;

   case ESCM_MOUSEMOVE:
      {
         PESCMMOUSEMOVE p = (PESCMMOUSEMOVE) pParam;
         pc->pLastMouse = p->pPosn;

         // only care if left button down
         if (!pControl->m_fLButtonDown || pc->fIgnoreNextButton)
            return TRUE;

         // if mouse dragged off screen region then scroll

         DWORD dwChar;
         dwChar = PointToChar (pControl, pc, &p->pPosn, -1, TRUE, TRUE);
         if (dwChar == (DWORD)-1)
            return FALSE;  // shouldn't happen

         // make a sound if just starting to drag
         if ((pc->iSelStart == pc->iSelEnd) && (pc->iSelEnd != (int) dwChar))
         pControl->m_pParentPage->m_pWindow->Beep(ESCBEEP_SCROLLDRAGSTART);

         pc->iSelEnd = (int) dwChar;

         pControl->Invalidate ();
      }
      return TRUE;

   case ESCM_SHIFTRIGHTARROW:
   case ESCM_SHIFTLEFTARROW:
   case ESCM_SHIFTLINEUP:
   case ESCM_SHIFTLINEDOWN:
   case ESCM_SHIFTHOME:
   case ESCM_SHIFTEND:
   case ESCM_SHIFTCTRLHOME:
   case ESCM_SHIFTCTRLEND:
   case ESCM_SHIFTPAGEUP:
   case ESCM_SHIFTPAGEDOWN:
   case ESCM_RIGHTARROW:
   case ESCM_LEFTARROW:
   case ESCM_CTRLRIGHTARROW:
   case ESCM_CTRLLEFTARROW:
   case ESCM_CTRLSHIFTRIGHTARROW:
   case ESCM_CTRLSHIFTLEFTARROW:
   case ESCM_LINEUP:
   case ESCM_LINEDOWN:
   case ESCM_HOME:
   case ESCM_END:
   case ESCM_CTRLHOME:
   case ESCM_CTRLEND:
   case ESCM_PAGEUP:
   case ESCM_PAGEDOWN:
      {
         // get DC
         HDC   hDC = pControl->m_pParentPage->m_pWindow->DCGet();

         // if start != end then invalidate whole thing
         if (pc->iSelEnd != pc->iSelStart)
            pControl->Invalidate();

         // invalidate old caret
         RECT  rc;
         POINT pt;
         int   iLine;
         if (CaretPosn (pControl, pc, hDC, &rc))
            pControl->Invalidate (&rc);
         pt.x = rc.left;
         pt.y = rc.top;
         if (pc->iSelEnd < (int) pc->dwLen)
            iLine = CharLine (pControl, pc, pc->iSelEnd);
         else
            iLine = pc->iNumLines - 1;
         DWORD dwLineStart, dwLineLen;

         switch (dwMessage) {
         case ESCM_LEFTARROW:
         case ESCM_SHIFTLEFTARROW:
            // if we're at the start of a line go to the end of the next line
            dwLineLen = LineLen (pControl, pc, (DWORD)iLine, &dwLineStart, TRUE);
            if (iLine && (pc->iSelEnd <= (int) dwLineStart)) {
               dwLineLen = LineLen (pControl, pc, (DWORD)iLine-1, &dwLineStart, TRUE);
               pc->iSelEnd = dwLineStart + dwLineLen;
            }
            else  // just decrease
               pc->iSelEnd--;
            break;

         case ESCM_RIGHTARROW:
         case ESCM_SHIFTRIGHTARROW:
            // if we're at the end of the line go to the beginning of the next line
            dwLineLen = LineLen (pControl, pc, (DWORD)iLine, &dwLineStart, TRUE);
            if ((iLine+1 < pc->iNumLines) && (pc->iSelEnd >= (int) dwLineStart + (int)dwLineLen)) {
               dwLineLen = LineLen (pControl, pc, (DWORD)iLine+1, &dwLineStart, TRUE);
               pc->iSelEnd = dwLineStart;
            }
            else  // just advance
               pc->iSelEnd++;
            break;

         // BUGFIX - Do control-(shift)-left/right arrow
         case ESCM_CTRLLEFTARROW:
         case ESCM_CTRLSHIFTLEFTARROW:
            {
               pc->iSelEnd--;
               WCHAR *c;
               c = (WCHAR*) pc->pMem->p;
               while (pc->iSelEnd >= 1) {
                  if (pc->iSelEnd >= (int)pc->dwLen)
                     break;   // can't test beyond end

                  // if get to an alpha on the right, and non-alpha on the left then is a word
                  if (!iswspace(c[pc->iSelEnd]) && iswspace(c[pc->iSelEnd-1]))
                     break;
                  pc->iSelEnd--;
               }
            }
            break;
         case ESCM_CTRLRIGHTARROW:
         case ESCM_CTRLSHIFTRIGHTARROW:
            {
               pc->iSelEnd++;
               WCHAR *c;
               c = (WCHAR*) pc->pMem->p;
               while (pc->iSelEnd < (int)pc->dwLen) {
                  if (pc->iSelEnd < 1)
                     break;   // can't test before beginning

                  // if get to an alpha on the left, and non-alpha on the right then is a word
                  if (!iswspace(c[pc->iSelEnd-1]) && iswspace(c[pc->iSelEnd]))
                     break;
                  pc->iSelEnd++;
               }
            }
            break;

         case ESCM_HOME:
         case ESCM_SHIFTHOME:
            dwLineLen = LineLen (pControl, pc, (DWORD)iLine, &dwLineStart, TRUE);
            pc->iSelEnd = (int) dwLineStart;
            break;

         case ESCM_END:
         case ESCM_SHIFTEND:
            dwLineLen = LineLen (pControl, pc, (DWORD)iLine, &dwLineStart, TRUE);
            if (dwLineStart + dwLineLen >= pc->dwLen)
               pc->iSelEnd = (int) pc->dwLen;
            else
               pc->iSelEnd = (int) (dwLineStart + dwLineLen /*BUGFIX linelen - 1*/);
            break;

         case ESCM_CTRLHOME:
         case ESCM_SHIFTCTRLHOME:
            pc->iSelEnd = 0;
            break;

         case ESCM_CTRLEND:
         case ESCM_SHIFTCTRLEND:
            pc->iSelEnd = (int) pc->dwLen;
            break;

         case ESCM_PAGEUP:
         case ESCM_SHIFTPAGEUP:
            pc->iSelEnd = (int) PointToChar (pControl, pc, &pt, iLine-LinesVisible(pControl, pc, TRUE), TRUE, TRUE);
            break;

         case ESCM_PAGEDOWN:
         case ESCM_SHIFTPAGEDOWN:
            pc->iSelEnd = (int) PointToChar (pControl, pc, &pt, iLine+LinesVisible(pControl, pc, TRUE), TRUE, TRUE);
            break;

         case ESCM_LINEUP:
         case ESCM_SHIFTLINEUP:
            pc->iSelEnd = (int) PointToChar (pControl, pc, &pt, iLine-1, TRUE, TRUE);
            break;

         case ESCM_LINEDOWN:
         case ESCM_SHIFTLINEDOWN:
            pc->iSelEnd = (int) PointToChar (pControl, pc, &pt, iLine+1, TRUE, TRUE);
            break;
         }
         // if shift is down keep old selstart & end
         if ((dwMessage < ESCM_SHIFTRIGHTARROW) || (dwMessage >ESCM_SHIFTPAGEDOWN))
            pc->iSelStart = pc->iSelEnd;
         pc->iSelEnd = max(0, pc->iSelEnd);
         pc->iSelEnd = min((int)pc->dwLen, pc->iSelEnd);
         pc->iSelStart = max(0, pc->iSelStart);
         pc->iSelStart = min((int)pc->dwLen, pc->iSelStart);

         // set the caret to red so that can see when scroll
         pc->fCaretBlink = FALSE;

         // if start != end then invalidate whole thing
         if (pc->iSelEnd != pc->iSelStart)
            pControl->Invalidate();

         // invalidate new caret position
         if (CaretPosn (pControl, pc, hDC, &rc))
            pControl->Invalidate (&rc);

         // scroll so selection is visible
         CaretVisible(pControl, pc);

         // release DC
         pControl->m_pParentPage->m_pWindow->DCRelease();

      }
      return TRUE;

   case ESCM_EDITCUT:
      pControl->Message (ESCM_EDITCOPY);

      // delete the text
      ReplaceSel (pControl, pc, NULL, 0);
      return TRUE;

   case ESCM_EDITCOPY:
      {
         // calclines just in case
         CalcLines (pControl, pc);

         // beep if nothing to copy
         // BUGFIX - Don't allow user to copy if it's a password control
         if ((pc->iSelStart == pc->iSelEnd) || (pc->fPassword)){
            pControl->m_pParentPage->m_pWindow->Beep (ESCBEEP_DONTCLICK);
            return TRUE;
         }

         // get the text and put in \r\n
         CMem  memcr;
         if (!memcr.Required ((abs(pc->iSelStart - pc->iSelEnd) + 1) * 4))
            return TRUE;
         int   iFrom, iTo, iMax;
         iFrom = min(pc->iSelStart, pc->iSelEnd);
         iMax = max(pc->iSelStart, pc->iSelEnd);
         // BUGFIX - Crash in edit when copy all
         iMax = min(iMax, (int) pc->dwLen);
         iTo = 0;
         WCHAR c;
         for (; iFrom < iMax; iFrom++) {
            c = ((WCHAR*) pc->pMem->p)[iFrom];
            if (c == L'\r')
               continue;

            // if it's \n add 2 characters
            if (c == L'\n') {
               ((WCHAR*) memcr.p)[iTo++] = L'\r';
               ((WCHAR*) memcr.p)[iTo++] = L'\n';
               continue;
            }

            // text
            ((WCHAR*) memcr.p)[iTo++] = c;
         }
         ((WCHAR*) memcr.p)[iTo++] = 0;

         // convert from unicode to ANSI
         CMem  mem;
         int   iLen;
         if (!mem.Required ((wcslen((WCHAR*) memcr.p) + 1) * 2))
            return TRUE;
         iLen = (int) WideCharToMultiByte (CP_ACP, 0,
            (WCHAR*) memcr.p, -1,
            (char*) mem.p, (int)mem.m_dwAllocated, 0, 0);
         ((char*) mem.p)[iLen] = 0; // null terminate

         // fill hmem
         HANDLE   hMem;
         hMem = GlobalAlloc (GMEM_MOVEABLE | GMEM_DDESHARE, iLen+1);
         if (!hMem)
            return TRUE;
         strcpy ((char*) GlobalLock(hMem), (char*) mem.p);
         GlobalUnlock (hMem);

         OpenClipboard (pControl->m_pParentPage->m_pWindow->m_hWnd);
         EmptyClipboard ();
         SetClipboardData (CF_TEXT, hMem);
         CloseClipboard ();
      }
      return TRUE;

   case ESCM_EDITPASTE:
      {
         // calclines just in case
         CalcLines (pControl, pc);

         CMem  mem;

         // get the clipboard
         if (!OpenClipboard (pControl->m_pParentPage->m_pWindow->m_hWnd)) {
            pControl->m_pParentPage->m_pWindow->Beep (ESCBEEP_DONTCLICK);
            return TRUE;
         }

         HANDLE   h;

         h = GetClipboardData (CF_TEXT);
         if (h) {
            char  *p;
            p = (char*) GlobalLock(h);
            mem.Required ((strlen(p)+1)*2);
            MultiByteToWideChar (CP_ACP, 0, p, -1, (WCHAR*)mem.p, (int)mem.m_dwAllocated/2);
            GlobalUnlock (h);
         }
         else {
            pControl->m_pParentPage->m_pWindow->Beep (ESCBEEP_DONTCLICK);
            // follow through so the clipboard is closed
         }

         CloseClipboard ();

         // if there's memory paste it in and massage it so that there's no cr/lf
         if (mem.p) {
            WCHAR *pszFrom, *pszTo;
            for (pszFrom = pszTo = (WCHAR*) mem.p; *pszFrom; pszFrom++) {
               // get rid of /r
               // BUGFIX - keep /r if (*pszFrom == L'\r')
               //   continue;

               // maybe get rid of /n
               if (!pc->fMultiLine && ((*pszFrom == L'\n') || (*pszFrom == L'\r')))
                  continue;

               // copy
               *(pszTo++) = *pszFrom;
            }
            *pszTo = NULL;

            // paste
            ReplaceSel (pControl, pc, (WCHAR*) mem.p, (DWORD)wcslen((WCHAR*) mem.p));
         }
      }
      return TRUE;

   case ESCM_BACKSPACE:
      {
         // if selend == selstart then move selstart
         if (pc->iSelEnd == pc->iSelStart) {
            if (pc->iSelStart <= 0) {
               pc->iSelStart = 0;
               pControl->m_pParentPage->m_pWindow->Beep (ESCBEEP_DONTCLICK);
               return TRUE;
            }

            // simulate a shift left-arrow. This way take care of delete both \n and \r
            pControl->Message (ESCM_SHIFTLEFTARROW);

            // flip end & start so end is higher
            int   iTemp;
            iTemp = pc->iSelEnd;
            pc->iSelEnd = pc->iSelStart;
            pc->iSelStart = iTemp;
         }

         // replace
         ReplaceSel (pControl, pc, NULL, 0);
      }
      return TRUE;

   case ESCM_DEL:
      {
         // calclines just in case
         CalcLines (pControl, pc);

         // if selend == selstart then move selstart
         if (pc->iSelEnd == pc->iSelStart) {
            if (pc->iSelStart >= (int) pc->dwLen) {
               pc->iSelStart = (int) pc->dwLen;
               pControl->m_pParentPage->m_pWindow->Beep (ESCBEEP_DONTCLICK);
               return TRUE;
            }

            // simulate a shift right-arrow. This way take care of delete both \n and \r
            pControl->Message (ESCM_SHIFTRIGHTARROW);

            // flip end & start sostart is higher
            int   iTemp;
            iTemp = pc->iSelEnd;
            pc->iSelEnd = pc->iSelStart;
            pc->iSelStart = iTemp;
         }

         // replace
         ReplaceSel (pControl, pc, NULL, 0);
      }
      return TRUE;

   case ESCM_SELECTALL:
      {
         // calclines just in case
         CalcLines (pControl, pc);

         pc->iSelStart = 0;
         pc->iSelEnd = (int) pc->dwLen;

         // if start != end then invalidate whole thing
         pControl->Invalidate();

         // scroll so selection is visible
         CaretVisible(pControl, pc);

      }
      return TRUE;

   case ESCM_EDITUNDO:
      {
         // calclines just in case
         CalcLines (pControl, pc);

         if (!pc->plistUndo->Num()) {
            pControl->m_pParentPage->m_pWindow->Beep (ESCBEEP_DONTCLICK);
            return TRUE;
         }

         // what info
         PUNDOINFO   pi;
         pi = (PUNDOINFO) pc->plistUndo->Get(0);
         if (!pi)
            return FALSE;

         // store the current away for redo
         UndoRedoAdd (pc->plistRedo, pc->iSelStart, pc->iSelEnd,
            pi->dwStartReplace, pi->dwLenReplaceWith,
            (WCHAR*) (pc->pMem->p) + pi->dwStartReplace, pi->dwLenReplaced);
            //abs(pi->iOldSelStart-pi->iOldSelEnd));

         // replace
         pc->iSelStart = (int) pi->dwStartReplace; // min(pi->iOldSelStart, pi->iOldSelEnd);
         pc->iSelEnd = pc->iSelStart + (int) pi->dwLenReplaceWith;

         ReplaceSel (pControl, pc, (PWSTR)(pi+1), pi->dwLenReplaced, FALSE);

         // set the new selection
         pc->iSelStart = pc->iSelEnd = max(pi->iOldSelEnd, pi->iOldSelStart);
         // did keep the old sel start & end before but caused problems with backspace

         // make sure the caret's visible
         CaretVisible (pControl, pc);

         // delete the undo record
         pc->plistUndo->Remove(0);

         // invalidate everything to ensure it all redraws
         pControl->Invalidate();

      }
      return TRUE;

   case ESCM_EDITREDO:
      {
         // calclines just in case
         CalcLines (pControl, pc);

         if (!pc->plistRedo->Num()) {
            pControl->m_pParentPage->m_pWindow->Beep (ESCBEEP_DONTCLICK);
            return TRUE;
         }

         // what info
         PUNDOINFO   pi;
         pi = (PUNDOINFO) pc->plistRedo->Get(0);
         if (!pi)
            return FALSE;

         // store the current away for undo
         UndoRedoAdd (pc->plistUndo, pc->iSelStart, pc->iSelEnd,
            pi->dwStartReplace, pi->dwLenReplaceWith,
            (WCHAR*) (pc->pMem->p) + pi->dwStartReplace, pi->dwLenReplaced);
//         int   iUndoStart, iUndoLen;
//         iUndoStart = min(pi->iOldSelStart, pi->iOldSelEnd);
//         iUndoLen = (int) pi->dwLenReplaceWith;
//         UndoRedoAdd (pc->plistUndo, iUndoStart, iUndoStart + iUndoLen,
//            (DWORD) iUndoStart, (DWORD) iUndoLen,
//            (WCHAR*) (pc->pMem->p) + iUndoStart, pi->dwLenReplaced);
#if 0
      UndoRedoAdd (pc->plistUndo, pc->iSelStart, pc->iSelEnd,
         (DWORD) min(pc->iSelStart, pc->iSelEnd),
         (DWORD) abs(pc->iSelStart - pc->iSelEnd),
         ((WCHAR*) pc->pMem->p) + iMin, dwLen);
#endif //0

         // replace
         pc->iSelStart = pi->dwStartReplace;
         pc->iSelEnd = pc->iSelStart + (int) pi->dwLenReplaceWith;

         ReplaceSel (pControl, pc, (PWSTR)(pi+1), pi->dwLenReplaced, FALSE);

         // set the new selection
         pc->iSelStart = pc->iSelEnd = max(pi->iOldSelEnd, pi->iOldSelStart);
         // did keep the old sel start & end before but caused problems with backspace

         // make sure the caret's visible
         CaretVisible (pControl, pc);

         // delete the rdo record
         pc->plistRedo->Remove(0);

         // invalidate everything to ensure it all redraws
         pControl->Invalidate();

      }
      return TRUE;

   case ESCM_EDITCANUNDOREDO:
      {
         PESCMEDITCANUNDOREDO p = (PESCMEDITCANUNDOREDO) pParam;
         CalcLines (pControl, pc);

         p->fRedo = pc->plistRedo->Num() ? TRUE : FALSE;
         p->fUndo = pc->plistUndo->Num() ? TRUE : FALSE;
      }
      return TRUE;

   case ESCM_EDITCHARFROMPOS:
      {
         PESCMEDITCHARFROMPOS p = (PESCMEDITCHARFROMPOS) pParam;
         CalcLines (pControl, pc);

         p->dwChar = PointToChar (pControl, pc, &p->p);

         return TRUE;
      }
      return TRUE;

   case ESCM_EDITEMPTYUNDO:
      {
         pc->plistRedo->Clear();
         pc->plistUndo->Clear();
      }
      return TRUE;

   case ESCM_EDITGETLINE:
      {
         PESCMEDITGETLINE p = (PESCMEDITGETLINE) pParam;
         CalcLines (pControl, pc);

         // if beyond edge
         if (p->dwLine >= pc->plistLine->Num()) {
            p->dwNeeded = 0;
            p->dwSize = 0;
            p->fFilledIn = FALSE;
            return TRUE;
         }

         // find the line info
         DWORD dwLen, dwIndex;
         dwLen = LineLen (pControl, pc, p->dwLine, &dwIndex);
         p->dwNeeded = (dwLen+1)*2;
         p->fFilledIn = p->dwNeeded <= p->dwSize;
         if (p->fFilledIn) {
            memcpy (p->psz, (WCHAR*) (pc->pMem->p) + dwIndex, dwLen * 2);
            (p->psz)[dwLen] = 0; // null terminate
         }

         return TRUE;
      }
      return TRUE;

   case ESCM_EDITLINEFROMCHAR:
      {
         PESCMEDITLINEFROMCHAR p = (PESCMEDITLINEFROMCHAR) pParam;
         CalcLines (pControl, pc);

         p->dwLine = CharLine (pControl, pc, p->dwChar);
      }
      return TRUE;


   case ESCM_EDITLINEINDEX:
      {
         PESCMEDITLINEINDEX p = (PESCMEDITLINEINDEX) pParam;
         CalcLines (pControl, pc);

         // if beyond edge
         if (p->dwLine >= pc->plistLine->Num()) {
            p->dwChar = (DWORD)-1;
            p->dwLen = (DWORD)-1;
            return TRUE;
         }

         // find the line info
         DWORD dwLen, dwIndex;
         dwLen = LineLen (pControl, pc, p->dwLine, &dwIndex);
         p->dwLen = dwLen;
         p->dwChar = dwIndex;

         return TRUE;
      }
      return TRUE;

   case ESCM_EDITPOSFROMCHAR:
      {
         PESCMEDITPOSFROMCHAR p = (PESCMEDITPOSFROMCHAR) pParam;
         CalcLines (pControl, pc);

         HDC   hDC;
         BOOL  fRet;
         hDC = pControl->m_pParentPage->m_pWindow->DCGet();
         fRet = CharPosn (pControl, pc, hDC, p->dwChar, &p->r);
         pControl->m_pParentPage->m_pWindow->DCRelease();
         if (!fRet)
            memset (&p->r, 0, sizeof(p->r));

         return TRUE;
      }
      return TRUE;


   case ESCM_EDITREPLACESEL:
      {
         PESCMEDITREPLACESEL p = (PESCMEDITREPLACESEL) pParam;
         CalcLines (pControl, pc);

         ReplaceSel (pControl, pc, p->psz, p->dwLen);
         return TRUE;
      }
      return TRUE;


   case ESCM_EDITSCROLLCARET:
      CalcLines (pControl, pc);
      CaretVisible (pControl, pc);
      return TRUE;

   case ESCM_EDITFINDTEXT:
      {
         PESCMEDITFINDTEXT p = (PESCMEDITFINDTEXT) pParam;
         CalcLines (pControl, pc);

         // assume don't find anything
         p->dwFoundStart = p->dwFoundEnd = (DWORD) -1;

         // length of string
         DWORD dwLen;
         dwLen = (DWORD)wcslen (p->pszFind);
         if (!dwLen) return TRUE;   // cant find this

         // lowercase first char
         WCHAR cLower;
         cLower = towlower (p->pszFind[0]);

         // make sure start & end within range
         DWORD dwStart, dwEnd;
         dwStart = min(p->dwStart, pc->dwLen);
         dwEnd = min(p->dwEnd, pc->dwLen);
         if (dwEnd >= (dwLen-1))
            dwEnd -= (dwLen-1);  // so dont find string beyond end of range
         else
            dwEnd = 0;

         // loop
         DWORD i;
         PWSTR psz;
         psz = (PWSTR) pc->pMem->p;
         for (i = dwStart; i < dwEnd; i++) {
            // match first cahracter?
            if (towlower(psz[i]) != cLower)
               continue;

            // match whole string only
            if (p->dwFlags & FR_MATCHCASE) {
               if (wcsncmp (psz + i, p->pszFind, dwLen))
                  continue;
            }
            else {
               if (_wcsnicmp (psz + i, p->pszFind, dwLen))
                  continue;
            }

            // if it's supposed to be a whole word then preceding and
            // following character must be non-alpha
            if (p->dwFlags & FR_WHOLEWORD) {
               if (i && iswalpha(psz[i-1]))
                  continue;
               if ( ((i+dwLen) < pc->dwLen) && iswalpha(psz[i+dwLen]))
                  continue;
            }

            // else, have a match
            p->dwFoundStart = i;
            p->dwFoundEnd = i+dwLen;
            return TRUE;
         }

         // if got here didn't find
         return TRUE;
      }
      return TRUE;
   }
   return FALSE;
}




