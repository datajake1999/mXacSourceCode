/***************************************************************8
TextWrap.cpp - Text wrapping functins. For text & graphics/objects.

begun 3/16/2000 by Mike Rozak
Copyright 2000 Mike Rozak. All rights reserved
*/


#ifndef _TEXTWRAP_H_
#define _TEXTWRAP_H_

#include "escarpment.h"


/* TWOBJECTINFO - Describes the input object info for text wrap */
typedef struct {
   QWORD    qwID;       // Unique object identifier
   DWORD    dwPosn;     // character position, offset from the beginning fo the string.
                        // hence, 0 = immediately before the 1st character
   int      iWidth;     // width
   int      iHeight;    // height
   DWORD    dwText;     // how it interacts with the text, one or more of TWTEXT_XXX
   TWFONTINFO    fi;    // howit's aligned
} TWOBJECTINFO, *PTWOBJECTINFO;

#define TWTEXT_BEHIND         0x00000001     // sits behidn the text. If not set then text wraps around
#define TWTEXT_INLINE         0x00000100     // it inline with the text. If not, then is to the right/left of the section
#define TWTEXT_INLINE_TOP     0x00010100     // inline & the top of object is aligned with top of text
#define TWTEXT_INLINE_CENTER  0x00020100     // inline & the object is centered with the text
#define TWTEXT_INLINE_BOTTOM  0x00030100     // inline & the bottom of the object is aligned with the baseline of the text.
#define TWTEXT_INLINE_MASK    0x00ff0100
#define TWTEXT_EDGE_LEFT      0x00010000     // not inline & the object is on the left side
#define TWTEXT_EDGE_RIGHT     0x00020000     // not inline & the object is on the right side
#define TWTEXT_EDGE_MASK      0x00ff0000
#define TWTEXT_BACKGROUND     0x01000001     // background image. Will be stretched to full size of background

/* TWTEXTELEM -  Says where the text is placed */
typedef struct {
   TWFONTINFO  *pfi;       // font info used
   RECT        r;          // rectangle for positioning
   RECT        rBack;      // background rectangle for background fill
   int         iBaseline;  // baseline
   // followed by NULL-terminated Unicode string
} TWTEXTELEM, *PTWTEXTELEM;


/* TWOBJECTPOSN - Describes where the object is placed */
typedef struct {
   QWORD       qwID;       // object ID
   QWORD       qwLink;     // special hack for links. This will be set to pcmmlnode, and qwID not set
   DWORD       dwBehind;   // 0 = with text, 1 = behind, 2 = background object
   RECT        r;          // rectangle for the position
   DWORD       dwText;     // TWTEXT_XXX flags for the object
} TWOBJECTPOSN, *PTWOBJECTPOSN;



// functions
HRESULT TextWrap (HDC hDC, int iWidth, PWSTR pszText, PTWFONTINFO* ppfi,
                  PTWOBJECTINFO poi, DWORD dwNumOI,
                  int *piWidth, int *piHeight,
                  CListVariable *pListTextElem, CListFixed *pListObjectPosn);
BOOL TextMove (int iDeltaX, int iDeltaY, CListVariable *plte, CListFixed *plop);
void TextBackground (RECT *prNew, CListFixed *plop);



#endif _TEXTWRAP_H_
