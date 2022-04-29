/****************************************************************************
MMLInterpret.cpp - Code to "interpret" the MMLNode objects from MMLParse,
   and produce all the necessary data structures to view a page.

begun 3/21/2000 by Mike Rozak
Copyright 2000 mike Rozak. All rights reserved
*/

#include "mymalloc.h"
#include <windows.h>
#include <stdlib.h>
#include <math.h>
#include <stdio.h>
#include "tools.h"
#include "fontcache.h"
#include "mmlparse.h"
#include "textwrap.h"
#include "mmlinterpret.h"
#include "jpeg.h"
#include "escarpment.h"
#include "paint.h"
#include "resleak.h"
#include <crtdbg.h>


// controls from various files
BOOL ControlColorBlend (PCEscControl pControl, DWORD dwMessage, PVOID pParam);
BOOL ControlImage (PCEscControl pControl, DWORD dwMessage, PVOID pParam);
BOOL ControlLink (PCEscControl pControl, DWORD dwMessage, PVOID pParam);
BOOL ControlHorizontalLine (PCEscControl pControl, DWORD dwMessage, PVOID pParam);
BOOL ControlButton (PCEscControl pControl, DWORD dwMessage, PVOID pParam);
BOOL ControlScrollBar (PCEscControl pControl, DWORD dwMessage, PVOID pParam);
BOOL ControlStatus (PCEscControl pControl, DWORD dwMessage, PVOID pParam);
BOOL ControlThreeD (PCEscControl pControl, DWORD dwMessage, PVOID pParam);
BOOL ControlChart (PCEscControl pControl, DWORD dwMessage, PVOID pParam);
BOOL ControlProgressBar (PCEscControl pControl, DWORD dwMessage, PVOID pParam);
BOOL ControlEdit (PCEscControl pControl, DWORD dwMessage, PVOID pParam);
BOOL ControlListBox (PCEscControl pControl, DWORD dwMessage, PVOID pParam);
BOOL ControlMenu (PCEscControl pControl, DWORD dwMessage, PVOID pParam);
BOOL ControlComboBox (PCEscControl pControl, DWORD dwMessage, PVOID pParam);
BOOL ControlTime (PCEscControl pControl, DWORD dwMessage, PVOID pParam);
BOOL ControlDate (PCEscControl pControl, DWORD dwMessage, PVOID pParam);
BOOL ControlFilteredList (PCEscControl pControl, DWORD dwMessage, PVOID pParam);




#define BIGSCALE  1.259921049895    // how much text is bigger every time "big" is called

/* globals */
static PWSTR   gaszErrorText[] = {
   L"",   // error 0
   L"Out of memory.", // error 1
   L"Can't generate the necessary font.", // error 2
   L"The text layout/wrapping call failed.", // error 3
   L"File not found.", // error 4
   L"" // empty
};


/******************************************************************************
EscTextFromNode - Given a node, this looks throught he node and all its content
nodes for text. The text is appeneded onto the CMem object with strcat. (No
NULL is appended though)

inputs
   PCMMLNode      pNode - node to look through
   PCMem          pMem - memory to append to
returns
   none
*/
void EscTextFromNode (PCMMLNode pNode, PCMem pMem)
{
   DWORD i;
   PWSTR psz;
   PCMMLNode   pSub;
   for (i = 0; i < pNode->ContentNum(); i++) {
      if (!pNode->ContentEnum (i, &psz, &pSub))
         continue;

      if (psz)
         pMem->StrCat (psz);
      else if (pSub)
         EscTextFromNode (pSub, pMem);
   }
}

/***********************************************************************************
FindPageInfo - Searches through the MML tree looking for page info.

inputs
   PCMMLNode      pNode - node to search through
returns
   PCMMLNode - PageInfo node, or NULL if can't find
*/
PCMMLNode FindPageInfo (PCMMLNode pNode)
{
   // is this it?
   PWSTR pszName;
   pszName = pNode->NameGet();
   if (pszName && !_wcsicmp(pszName, L"PageInfo"))
      return pNode;

   // else children
   DWORD i;
   PWSTR psz;
   PCMMLNode   pSub;
   for (i = 0; i < pNode->ContentNum(); i++) {
      pSub = NULL;
      pNode->ContentEnum(i, &psz, &pSub);
      if (!pSub) continue;

      // must be right type
      if (pSub->m_dwType != MMLCLASS_ELEMENT)
         continue;

      // if it's a control then skip because controls might
      // contain pageinfo
      psz = pSub->NameGet();
      if (psz && EscControlGet (psz))
         continue;

      PCMMLNode   pRet;
      pRet = FindPageInfo (pSub);
      if (pRet)
         return pRet;

      // else continue
   }

   return NULL;   // cant find
}


/******************************************************************************
CEscTextBlock::Constructor & destructor
*/
CEscTextBlock::CEscTextBlock (void)
{
   m_pPage = NULL;
   m_pControl = NULL;
   m_pError = NULL;
   m_pNode = NULL;
   m_listText.Init (sizeof(WCHAR));
   m_listPTWFONTINFO.Init (sizeof(PTWFONTINFO));
   m_listTWOBJECTINFO.Init (sizeof(TWOBJECTINFO));
   m_plistTWTEXTELEM = NULL;
   m_plistTWOBJECTPOSN = NULL;
   m_pFontCache = NULL;
   m_fDeleteCMMLNode = FALSE;
   m_fRootNodeNULL = FALSE;
   m_pNodePageInfo = NULL;
   m_fFoundStretch = FALSE;

   m_fUseOtherError = m_fUseOtherFont = m_fUseOtherControlCache = FALSE;

   // set up defaults for the font
   memset (&m_fi, 0, sizeof(m_fi));
   m_fi.iPointSize = 12;
   wcscpy (m_fi.szFont, L"Arial");
   m_fi.fi.cBack = (DWORD)-1;
   m_fi.fi.iLineSpacePar = -100;
   m_fi.fi.iTab = 96 / 2;  // for now. Really set during Interpret

   // if there aren't any controls then add them
   if (!EscControlGet (L"ColorBlend")) {
      EscControlAdd (L"Image", ControlImage);
      EscControlAdd (L"ColorBlend", ControlColorBlend);
      EscControlAdd (L"HR", ControlHorizontalLine);
      EscControlAdd (L"Button", ControlButton);
      EscControlAdd (L"ScrollBar", ControlScrollBar);
      EscControlAdd (L"Status", ControlStatus);
      EscControlAdd (L"ThreeD", ControlThreeD);
      EscControlAdd (L"Chart", ControlChart);
      EscControlAdd (L"ProgressBar", ControlProgressBar);
      EscControlAdd (L"Edit", ControlEdit);
      EscControlAdd (L"ListBox", ControlListBox);
      EscControlAdd (L"Menu", ControlMenu);
      EscControlAdd (L"ComboBox", ControlComboBox);
      EscControlAdd (L"Time", ControlTime);
      EscControlAdd (L"Date", ControlDate);
      EscControlAdd (L"FilteredList", ControlFilteredList);
      // NOTE: Do not add ControlLink to this since it's special
   }
}

CEscTextBlock::~CEscTextBlock (void)
{
   if (!m_fUseOtherError && m_pError)
      delete m_pError;
   if (m_plistTWTEXTELEM)
      delete m_plistTWTEXTELEM;
   if (m_plistTWOBJECTPOSN) {
      // before delet object position must make sure not referencing any objects
      // that need to be freed/deleted
      FreeObjects ();
      delete m_plistTWOBJECTPOSN;
   }
   if (!m_fUseOtherControlCache && m_plistPCESCCONTROL) {
      FreeControls();
      delete m_plistPCESCCONTROL;
   }
   if (!m_fUseOtherFont && m_pFontCache)
      delete m_pFontCache;

   // delete the page
   if (m_fDeleteCMMLNode && m_pNode)
      delete m_pNode;
}



/******************************************************************************
CEscTextBlock::Init - Initializes the object with its own variables.

*/
BOOL CEscTextBlock::Init (void)
{
   m_pError = new CEscError;
   m_plistTWTEXTELEM = new CListVariable;
   m_plistTWOBJECTPOSN = new CListFixed;
   m_pFontCache = new CFontCache;
   m_plistPCESCCONTROL = new CListFixed;

   if (!m_pError || !m_plistTWTEXTELEM || !m_plistTWOBJECTPOSN || !m_pFontCache || !m_plistPCESCCONTROL)
      return FALSE;

   m_plistTWOBJECTPOSN->Init (sizeof(TWOBJECTPOSN));
   m_listText.Init (sizeof(WCHAR));
   m_listPTWFONTINFO.Init (sizeof(PTWFONTINFO));
   m_listTWOBJECTINFO.Init (sizeof(TWOBJECTINFO));
   m_plistPCESCCONTROL->Init (sizeof(CEscControl*));

   return TRUE;
}


/******************************************************************************
CEscTextBlock::OtherError - Use a different error object to report into. Don't
   delete the error object when the interpet object is deleted.

inputs
   PCEscError  pError - new error
returns
   none
*/
void CEscTextBlock::OtherError (PCEscError pError)
{
   if (!m_fUseOtherError && m_pError)
      delete m_pError;
   m_pError = pError;
   m_fUseOtherError = TRUE;
}


/******************************************************************************
CEscTextBlock::OtherControlCache - Use a different control cache to pull old
   controls from. Don't delete the control cache object when the interpet object is deleted.

inputs
   PCListFixed pList - list of PCESCCONTROL
returns
   none
*/
void CEscTextBlock::OtherControlCache (PCListFixed pList)
{
   if (!m_fUseOtherControlCache && m_plistPCESCCONTROL) {
      FreeControls ();
      delete m_plistPCESCCONTROL;
   }
   m_plistPCESCCONTROL = pList;
   m_fUseOtherControlCache = TRUE;
}


/******************************************************************************
CEscTextBlock::OtherFontCache - Use a different font cache to add to. Don't
   delete the font cache when the interpret object is deleted..

inputs
   PCFontCache pFontCache - new font cache
returns
   none
*/
void CEscTextBlock::OtherFontCache (PCFontCache pFontCache)
{
   if (!m_fUseOtherFont && m_pFontCache)
      delete m_pFontCache;

   m_pFontCache = pFontCache;
   m_fUseOtherFont = TRUE;
}


/******************************************************************************
CEscTextBlock::AddChar - Adds a character to the m_listText and m_listTWFONTINFO
items, so they can be processed by Text wrap.

inputs
   WCHAR       c - character
   IFONTINFO   *pIFontInfo - font info structure - If NULL use m_fi.
returns
   BOOL - TRUE if OK
*/
BOOL CEscTextBlock::AddChar (WCHAR c, IFONTINFO *pIFontInfo)
{
   WCHAR    sz[2];
   sz[0] = c;
   sz[1] = 0;

   return AddString (sz, pIFontInfo);
}

/******************************************************************************
CEscTextBlock::AddString - Adds a string all using the same font.

inputs
   PWSTR       psz - string
   IFONTINFO   *pIFontInfo - font info. - If NULL use m_fi
returns
   BOOL - TRUE if OK
*/
BOOL CEscTextBlock::AddString (PWSTR psz, IFONTINFO *pIFontInfo)
{
   if (!pIFontInfo)
      pIFontInfo = &m_fi;

   DWORD i, dwLen;
   dwLen = (DWORD) wcslen (psz);

   m_listText.Required (dwLen);
   for (i = 0; i < dwLen; i++)
      m_listText.Add (&psz[i]);

   // fint the font in the font cache
   TWFONTINFO  *pfi;
   pfi = m_pFontCache->Need (m_hDC, &pIFontInfo->fi, pIFontInfo->iPointSize,
      pIFontInfo->dwFlags, pIFontInfo->szFont);
   if (!pfi) {
      m_pError->Set (2, gaszErrorText[2]);
      return FALSE;
   }

   m_listPTWFONTINFO.Required (dwLen);
   for (i = 0; i < dwLen; i++)
      m_listPTWFONTINFO.Add (&pfi);

   // done
   return TRUE;
}

/******************************************************************************
CEscTextBlock::AddObject - Adds an object to the current location.

inputs
   QWORD       qwID - object ID
   int         iWidth, iHeight - WIdth in height
   DWORD       dwText - From TWTEXT_XXXX
   PTWFONTINFO pfi - Font info to USE. NULL if doesnt affect text justification
returns
   BOOL - TRUE if succede
*/
BOOL CEscTextBlock::AddObject (QWORD qwID, int iWidth, int iHeight, DWORD dwText, PIFONTINFO pfi)
{
   DWORD dwChar;
   dwChar = m_listText.Num();

   TWOBJECTINFO   oi;
   memset (&oi, 0, sizeof(oi));
   oi.dwPosn = dwChar;
   oi.dwText = dwText;
   oi.iHeight = iHeight;
   oi.iWidth = iWidth;
   oi.qwID = qwID;
   if (pfi) {
      TWFONTINFO *pfi2;
      pfi2 = m_pFontCache->Need (m_hDC, &pfi->fi, pfi->iPointSize,
         pfi->dwFlags, pfi->szFont);

      if (pfi2)
         oi.fi = *pfi2;
      else
         oi.fi.dwJust = (DWORD)-1;
   }
   else
      oi.fi.dwJust = (DWORD)-1;

   m_listTWOBJECTINFO.Add (&oi);
   return TRUE;
}

/******************************************************************************
CEscTextBlock::Tag - Deal with a tag. Determine what type of tag it is and
create new objects, add text, or whateber. This function is basically a big
switch statemtn that calls other TagXXXX functions.

inputs
   PCMMLNode      pNode - Node to parse
returns
   BOOL - TRUE if succede
*/
BOOL CEscTextBlock::Tag (PCMMLNode pNode)
{
   BOOL  fTagKnown;
   // if this is the frist node, and we're supposed to interpret it at
   // NULL, then call tag simple text
   if (m_fRootNodeNULL && (m_pNode == pNode))
      return TagSimpleText (pNode, &fTagKnown, TRUE);

   // ignore it unless its an element
   if (pNode->m_dwType != MMLCLASS_ELEMENT)
      return TRUE;

   // get the node text
   WCHAR *pName;
   pName = pNode->NameGet();
   if (!pName)
      return FALSE;

   // see if there are any controls named after this
   PESCCONTROLCALLBACK pCallback;
   if (pCallback = EscControlGet (pName))
      return TagControl (pNode, (PVOID) pCallback);

   // see if it's simple text
   if (TagSimpleText (pNode, &fTagKnown))
      return TRUE;   // it was handled
   // else it wasnt handled by TagSimpleText
   if (fTagKnown)
      return FALSE;  // it wan't handled byt TagSimpleText knew the tag. => error

   // see if it's a table
   if (!_wcsicmp(pName, L"Table"))
      return TagTable (pNode);

   // bullets
   if (!_wcsicmp(pName, L"ol"))
      return TagList (pNode, TRUE);
   if (!_wcsicmp(pName, L"ul"))
      return TagList (pNode, FALSE);

   // if comment ignore
   if (!_wcsicmp(pName, L"comment"))
      return TRUE;

   // ignore hoverhelp
   if (!_wcsicmp(pName, L"hoverhelp"))
      return TRUE;

   // see if it's a section
   if (!_wcsicmp(pName, L"Section")) {
      return TagSection (pNode);
   }

   // see if it's the starting <main> tag
   if (!_wcsicmp(pName, L"<Main>"))
      return TagMain (pNode);

   // if it's a <pageinfo> node then just remember
   if (!_wcsicmp(pName, L"PageInfo")) {
      m_pNodePageInfo = pNode;
      return TRUE;
   }

   // if it's a stretch marker
   if (!_wcsicmp(pName, L"StretchStart"))
      return TagStretch (TRUE);
   else if (!_wcsicmp(pName, L"StretchStop"))
      return TagStretch (FALSE);

   // as a last resort, use the unknown tag handler
   return TagUnknown (pNode);
}


/******************************************************************************
CEscTextBlock::TagStretch - The stretch tage indicates where to vertically start
stretching/shrinking the display from for windows that stretch.

inputs
   BOOL     fStart - if TRUE, indicates the start, FALSE indicates the end
returns
   BOOl - TRUE if successful
*/
BOOL CEscTextBlock::TagStretch (BOOL fStart)
{
   // add table object
   STRETCHOBJECT  *pco;
   pco = (STRETCHOBJECT*) ESCMALLOC(sizeof(STRETCHOBJECT));
   if (!pco) {
      m_pError->Set (1, gaszErrorText[1]);
      return FALSE;
   }
   memset (pco, 0, sizeof(*pco));
   pco->dwID = MMLOBJECT_STRETCH;
   pco->fStart = fStart;

   // remember that found stretch indicators
   m_fFoundStretch = TRUE;

   return AddObject ((QWORD) pco, 0, 0, TWTEXT_BEHIND | TWTEXT_INLINE_CENTER);
}

/******************************************************************************
CEscTextBlock::TagSection - Creates a section object within the text so know
where a section begins. Use this to scroll within the document.

inputs
   PCMMLNode      pNode - Node to look through
returns
   BOOl - TRUE if successful
*/
BOOL CEscTextBlock::TagSection (PCMMLNode pNode)
{
   // first of all, find the name
   CMem  mem;
   PWSTR psz;
   psz = pNode->AttribGet(L"name");
   if (psz)
      mem.StrCat (psz);
   else
      EscTextFromNode (pNode, &mem);
   mem.CharCat (0);

   // add table object
   SECTIONOBJECT  *pco;
   pco = (SECTIONOBJECT*) ESCMALLOC(sizeof(SECTIONOBJECT));
   if (!pco) {
      m_pError->Set (1, gaszErrorText[1]);
      return FALSE;
   }
   memset (pco, 0, sizeof(*pco));
   pco->dwID = MMLOBJECT_SECTION;
   pco->pszName = (WCHAR*) mem.p;
   mem.p = NULL;

   if (!AddObject ((QWORD) pco, 0, 0, TWTEXT_BEHIND | TWTEXT_INLINE_TOP))
      return FALSE;

   // pass it onto simple text and pretend that it's known
   BOOL fTagKnown;
   return TagSimpleText (pNode, &fTagKnown, TRUE);
}

/******************************************************************************
CEscTextBlock::TagSimpleText - Handles a tag that somehow simply modifies the
font and then adds the internal text onto the queue. This includes <p>, <font> <big>,
etc. Called by ::Tag.

inputs
   PCMMLNode      pNode - Node to parse
   BOOL           *pfTagKnown - Set to TRUE if the tag is known. FALSE if the
                  tag from pNode wasn't one from here. So, if the function returns
                  FALSE and pfTagKnown is set to FALSE, then see if it's a different
                  tag.
   BOOL           fPretend - If it was unknown, pretend it's just text.
returns
   BOOL - TRUE if succede. FALSE if fails
*/
BOOL CEscTextBlock::TagSimpleText (PCMMLNode pNode, BOOL *pfTagKnown, BOOL fPretend)
{
   *pfTagKnown = TRUE;

   BOOL  fAppendCR = FALSE;
   BOOL  fAppendWrapCR = FALSE;

   // store away the font info to return to normal later
   IFONTINFO   fi;
   fi = m_fi;

   PWSTR pName;
   pName = pNode->NameGet();

   // look for a match
   if (!_wcsicmp (pName, L"p") || !_wcsicmp (pName, L"align")) {
      // paragraph
      // since paragraph's only change is that it appends a CR when done, set the
      // append CR flag and do that later
      if (!_wcsicmp (pName, L"p"))
         fAppendCR = TRUE;

      // <p> suppports align=left,right,or center
      WCHAR *psz;
      psz = pNode->AttribGet (L"align");
      if (psz) {
         if (!_wcsicmp(psz, L"right"))
            m_fi.fi.dwJust = 2;
         else if (!_wcsicmp(psz, L"center"))
            m_fi.fi.dwJust = 1;
         else
            m_fi.fi.dwJust = 0;
      }

      // tabs - number of pixels
      int   i;
      if (AttribToDecimal (pNode->AttribGet (L"tab"), &i))
         m_fi.fi.iTab = i;

      // indents - number of pixels
      if (AttribToDecimal (pNode->AttribGet (L"parindent"), &i))
         m_fi.fi.iIndentLeftFirst += i;
      if (AttribToDecimal (pNode->AttribGet (L"wrapindent"), &i))
         m_fi.fi.iIndentLeftWrap += i;
      if (AttribToDecimal (pNode->AttribGet (L"rightindent"), &i))
         m_fi.fi.iIndentRight += i;

      // line spacing - takes a % of charcter height, or size in pixels
      BOOL  fPercent;
      if (AttribToDecimalOrPercent (pNode->AttribGet(L"parlinespacing"), &fPercent, &i))
         m_fi.fi.iLineSpacePar = fPercent ? -i : i;
      if (AttribToDecimalOrPercent (pNode->AttribGet(L"wraplinespacing"), &fPercent, &i))
         m_fi.fi.iLineSpacePar = fPercent ? -i : i;

   }

   else if (!_wcsicmp (pName, L"a")) {  // link
      // Underline & blue
      // BUGFIX - Links can be bold
      if (pNode->AttribGet(L"href"))
         m_fi.dwFlags |= FCFLAG_UNDERLINE;
      m_fi.fi.cText = RGB(0,0,0xff);
      m_fi.fi.qwLinkID = (QWORD) pNode;   // set this for link ID so know which node
                                          // to use later to turn into control

      // if other color then use that
      COLORREF cr;
      if (AttribToColor (pNode->AttribGet(L"color"), &cr))
         m_fi.fi.cText = cr;
   }

   else if (!_wcsicmp (pName, L"br")) {
      fAppendWrapCR = TRUE;
   }

   else if (!_wcsicmp (pName, L"font")) {
      WCHAR *psz;

      // font face
      psz = pNode->AttribGet (L"face");
      if (psz) {
         wcsncpy (m_fi.szFont, psz, sizeof(m_fi.szFont)/2 - 1);
      }

      // color
      COLORREF cr;
      if (AttribToColor (pNode->AttribGet (L"color"), &cr))
         m_fi.fi.cText = cr;

      // size
      int   iSize;
      if (AttribToDecimal (pNode->AttribGet(L"size"), &iSize))
         m_fi.iPointSize = iSize;
   }

   else if (!_wcsicmp(pName, L"big")) {
      m_fi.iPointSize = (int) (m_fi.iPointSize * BIGSCALE);   /* 2.0 ^ 1/3 */
   }

   else if (!_wcsicmp(pName, L"small")) {
      m_fi.iPointSize = (int) (m_fi.iPointSize / BIGSCALE);
   }

   else if (!_wcsicmp(pName, L"sup")) {
      m_fi.fi.iSuper += m_fi.iPointSize / 2;
      m_fi.iPointSize = (int) (m_fi.iPointSize / BIGSCALE);
   }

   else if (!_wcsicmp(pName, L"sub")) {
      m_fi.fi.iSuper -= m_fi.iPointSize / 2;
      m_fi.iPointSize = (int) (m_fi.iPointSize / BIGSCALE);
   }

   else if (!_wcsicmp(pName, L"strong") || !_wcsicmp(pName, L"bold")) {
      m_fi.dwFlags |= FCFLAG_BOLD;
   }

   else if (!_wcsicmp(pName, L"em") || !_wcsicmp(pName, L"italic")) {
      m_fi.dwFlags |= FCFLAG_ITALIC;
   }

   else if (!_wcsicmp(pName, L"u") || !_wcsicmp(pName, L"underline")) {
      m_fi.dwFlags |= FCFLAG_UNDERLINE;
   }

   else if (!_wcsicmp(pName, L"strike") || !_wcsicmp(pName, L"strikethrough")) {
      m_fi.dwFlags |= FCFLAG_STRIKEOUT;
   }

   else if (!_wcsicmp(pName, L"blockquote")) {
      m_fi.fi.iIndentLeftFirst += m_fi.fi.iTab;
      m_fi.fi.iIndentLeftWrap += m_fi.fi.iTab;
      m_fi.fi.iIndentRight += m_fi.fi.iTab;
   }

   else if (!_wcsicmp(pName, L"td") || !_wcsicmp(pName, L"th")) {  // table data
      // just act as ordinary text, since called by table functions
   }

   else if (!_wcsicmp(pName, L"highlight")) {
      COLORREF cr;
      m_fi.fi.cBack = RGB(0xff, 0xff, 0x00);
      if (AttribToColor (pNode->AttribGet(L"color"), &cr))
         m_fi.fi.cBack = cr;
   }

   else if (!_wcsicmp(pName, L"li")) {   // list item
      fAppendCR = TRUE;
   }
   else if (!_wcsicmp(pName, L"keyword")) {
      // do nothing
   }
   else if (!_wcsicmp(pName, L"null")) {   // null operation
      // do nothing
   }

   else {
      // unknown
      *pfTagKnown = FALSE;
      if (!fPretend)
         return FALSE;
   }

   // loop through the contents
   DWORD i;
   PWSTR psz;
   PCMMLNode   pSub;
   for (i = 0; i < pNode->ContentNum(); i++) {
      pNode->ContentEnum (i, &psz, &pSub);

      if (psz)
         AddString (psz);
      else if (pSub) {
         if (!Tag (pSub)) {
            m_fi = fi;
            return FALSE;
         }
      }

      // ignoring text in the top of the main node
   }


   if (fAppendCR)
      AddChar (L'\n');
   if (fAppendWrapCR)
      AddChar (1 /* wrap newline*/);

   //restore font info
   m_fi = fi;

   return TRUE;
}


/******************************************************************************
CEscTextBlock::TagMain - Handle the main (1st) element.

inputs
   PCMMLNode      pNode - Node to parse
retrns
   BOOL - TRUE if succed. FALSE if fails
*/
BOOL CEscTextBlock::TagMain (PCMMLNode pNode)
{
   // find all the sub nodes and do this
   DWORD i;
   PWSTR psz;
   PCMMLNode   pSub;
   for (i = 0; i < pNode->ContentNum(); i++) {
      pNode->ContentEnum (i, &psz, &pSub);

      if (pSub) {
         if (!Tag (pSub))
            return FALSE;
      }

      // ignoring text in the top of the main node
   }

   return TRUE;
}

/******************************************************************************
CEscTextBlock::TagList - Handle either numbered list or bullet list.

inputs
   PCMMLNode      pNode - Node to parse
   BOOL           fNumber - if TRUE, its a numbered list
retrns
   BOOL - TRUE if succed. FALSE if fails
*/
BOOL CEscTextBlock::TagList (PCMMLNode pNode, BOOL fNumber)
{
   DWORD dwLineNum = 1;
   IFONTINFO   iOld, iBulletLine, iNonBulletLine, iSymbol;
   iOld = m_fi;

   // bullet line is indented
   iBulletLine = m_fi;
   iBulletLine.fi.iIndentLeftWrap += iBulletLine.fi.iTab;

   // non-bullet line starts where the other one wraps
   iNonBulletLine = iBulletLine;
   iNonBulletLine.fi.iIndentLeftFirst = iBulletLine.fi.iIndentLeftWrap;

   // symbol for the dot
   WCHAR cBullet;
   iSymbol = iBulletLine;

   // color for bullet?
   COLORREF cr;
   if (AttribToColor (pNode->AttribGet(L"color"), &cr))
      iSymbol.fi.cText = cr;

   // font to use for dot
   WCHAR *pszPre, *pszAfter;
   pszPre = pszAfter = NULL;
   DWORD dwNumType = 0; //0 = number, 1 = letter, 2 = roman
   BOOL  fLower = FALSE;
   if (fNumber) {
      pszPre = pNode->AttribGet (L"textprior");
      pszAfter = pNode->AttribGet (L"textafter");
      if (!pszPre)
         pszPre = L"";
      if (!pszAfter)
         pszAfter = L".";


      PWSTR psz = pNode->AttribGet(L"type");
      if (psz && !wcscmp(psz, L"A"))
         dwNumType = 1;
      else if (psz && !wcscmp(psz, L"a")) {
         dwNumType = 1;
         fLower = TRUE;
      }
      else if (psz && !wcscmp(psz, L"I"))
         dwNumType = 2;
      else if (psz && !wcscmp(psz, L"i")) {
         dwNumType = 2;
         fLower = TRUE;
      }
   }
   else {   // bullet
      WCHAR szSymbol[] = L"Symbol";
      WCHAR szWingdings2[] =L"Wingdings 2";
      WCHAR szWingdings[] =L"Wingdings";

      PWSTR psz = pNode->AttribGet(L"type");
      if (psz && !_wcsicmp(psz, L"circle")) { // open circle
         wcscpy (iSymbol.szFont, szWingdings2);
         cBullet = 0x0081; // L'A' + 28 * 3 + 4;
      }
      else if (psz && !_wcsicmp(psz, L"diamond")) {
         wcscpy (iSymbol.szFont, szWingdings2);
         cBullet = 0xae;
      }
      else if (psz && !_wcsicmp(psz, L"checkbox")) {
         wcscpy (iSymbol.szFont, szWingdings2);
         cBullet = L'R';
      }
      else if (psz && !_wcsicmp(psz, L"check")) {
         wcscpy (iSymbol.szFont, szWingdings2);
         cBullet = L'P';
      }
      else if (psz && !_wcsicmp(psz, L"square")) {
         wcscpy (iSymbol.szFont, szWingdings2);
         cBullet = 0xa1; //L'A' + 28 * 3 + 12;
      }
      else if (psz && !_wcsicmp(psz, L"star")) {
         wcscpy (iSymbol.szFont, szWingdings2);
         cBullet = 0xd8; //L'A' + 28 * 5 + 10;
      }
      else if (psz && !_wcsicmp(psz, L"star2")) {
         wcscpy (iSymbol.szFont, szWingdings2);
         cBullet = 0xde; //L'A' + 28 * 5 + 22;
      }
      else if (psz && !_wcsicmp(psz, L"arrow")) {
         wcscpy (iSymbol.szFont, szWingdings);
         cBullet = L'A' + 28 * 6 - 1;
      }
      else if (psz && !_wcsicmp(psz, L"pointer")) {
         wcscpy (iSymbol.szFont, szWingdings);
         cBullet = L'A' + 28 * 5 + 11;
      }
      else {
         wcscpy (iSymbol.szFont, szSymbol);
         cBullet = L'A' + 28 * 4 + 6;
      }
   }
   // BUGBUG - 2.0 - at some point handle image bullets.

   // find all the sub nodes and do this
   DWORD i;
   PWSTR psz;
   PCMMLNode   pSub;
   for (i = 0; i < pNode->ContentNum(); i++) {
      pNode->ContentEnum (i, &psz, &pSub);

      if (!pSub)
         continue;

      if (!_wcsicmp(pSub->NameGet(), L"li")) {
         // it's a numbered/bulleted line
         if (fNumber) {
            WCHAR szTemp[128];

            if (dwNumType == 0)  // decimal number
               swprintf (szTemp, L"%s%d%s", pszPre, dwLineNum, pszAfter);

            else if (dwNumType == 1) { // letter
               wcscpy (szTemp, pszPre);
               int   iLen;
               iLen = (int) wcslen(szTemp);
               DWORD i;
               for (i = 0; i < (dwLineNum + 25) / 26; i++)
                  szTemp[iLen++] = (fLower ? L'a' : L'A') + (WCHAR) ((dwLineNum-1) % 26);
               szTemp[iLen] = 0;
               wcscat (szTemp, pszAfter);
            }

            else { // roman - not exactly correct for high numbers
               wcscpy (szTemp, pszPre);
               int   iLen;
               iLen = (int) wcslen(szTemp);
               DWORD i = dwLineNum;
               while (i >= 100) {
                  szTemp[iLen++] = fLower ? L'c' : L'C';
                  i -= 100;
               }
               if (i >= 90) {
                  szTemp[iLen++] = fLower ? L'x' : L'X';
                  szTemp[iLen++] = fLower ? L'c' : L'C';
                  i -= 90;
               }
               if (i >= 50) {
                  szTemp[iLen++] = fLower ? L'l' : L'L';
                  i -= 50;
               }
               if (i >= 40) {
                  szTemp[iLen++] = fLower ? L'x' : L'X';
                  szTemp[iLen++] = fLower ? L'l' : L'L';
                  i -= 40;
               }
               while (i >= 10) {
                  szTemp[iLen++] = fLower ? L'x' : L'X';
                  i -= 10;
               }
               szTemp[iLen] = 0;
               switch (i) { // < 10
               case 1:
                  wcscat (szTemp, fLower ? L"i" : L"I");
                  break;
               case 2:
                  wcscat (szTemp, fLower ? L"ii" : L"II");
                  break;
               case 3:
                  wcscat (szTemp, fLower ? L"iii" : L"III");
                  break;
               case 4:
                  wcscat (szTemp, fLower ? L"iv" : L"IV");
                  break;
               case 5:
                  wcscat (szTemp, fLower ? L"v" : L"V");
                  break;
               case 6:
                  wcscat (szTemp, fLower ? L"vi" : L"VI");
                  break;
               case 7:
                  wcscat (szTemp, fLower ? L"vii" : L"VII");
                  break;
               case 8:
                  wcscat (szTemp, fLower ? L"viii" : L"VIII");
                  break;
               case 9:
                  wcscat (szTemp, fLower ? L"ix" : L"IX");
                  break;
               }
               wcscat (szTemp, pszAfter);
            }

            dwLineNum++;

            m_fi = iSymbol;
            AddString (szTemp);
         }
         else {
            m_fi = iSymbol;
            AddChar (cBullet);  // bullet
         }

         m_fi = iBulletLine;
         AddChar ('\t');

         // treat li as a <p> and do normal processing
         BOOL  f;
         if (!TagSimpleText (pSub, &f, TRUE)) {
            m_fi = iOld;
            return FALSE;
         }
      }
      else {
         m_fi = iNonBulletLine;

         // ordinaty line, such as <p>
         if (!Tag (pSub)) {
            m_fi = iOld;
            return FALSE;
         }
      }
   }

   m_fi = iOld;
   return TRUE;
}

/******************************************************************************
CEscTextBlock::TagUnknown - Deal with a completely unknown tag.

inputs
   PCMMLNode      pNode - Node to parse
retrns
   BOOL - TRUE if succed. FALSE if fails
*/
BOOL CEscTextBlock::TagUnknown (PCMMLNode pNode)
{
   // for now, (and maybe forever), just print out "Unknown tag, XXX" - and process it
   // like a <p> or whatever

   AddString (L"Unknown tag:");
   AddString (pNode->NameGet());
   AddChar (L'\n');

   BOOL f;
   TagSimpleText (pNode, &f, TRUE);

   return TRUE;
}

/******************************************************************************
CEscTextBlock::FillInTABLEINFO - Asks the current node's attributes for TABLEINFO info.

inputs
   PCMMLNode      pNode - Node to parse
   TABLEINFO      *pti - filled in
   int            iPercent - If height/width a pecent, than use this to ber percent of
retrns
   none
*/
void CEscTextBlock::FillInTABLEINFO (PCMMLNode pNode, TABLEINFO *pti, int iPercent)
{
   // get the variables
   COLORREF cr;
   if (AttribToColor (pNode->AttribGet (L"bgcolor"), &cr))
      pti->cBack = cr;
   if (AttribToColor (pNode->AttribGet (L"bordercolor"), &cr))
      pti->cEdge = cr;
   PWSTR psz;
   psz = pNode->AttribGet(L"align");
   if (psz && !_wcsicmp(psz, L"left"))
      pti->dwAlignHorz = 0;
   else if (psz && !_wcsicmp(psz, L"center"))
      pti->dwAlignHorz = 1;
   else if (psz && !_wcsicmp(psz, L"right"))
      pti->dwAlignHorz = 2;
   psz = pNode->AttribGet(L"valign");
   if (psz && !_wcsicmp(psz, L"top"))
      pti->dwAlignVert = 0;
   else if (psz && (!_wcsicmp(psz, L"middle") || !_wcsicmp(psz, L"center")))
      pti->dwAlignVert = 1;
   else if (psz && !_wcsicmp(psz, L"bottom"))
      pti->dwAlignVert = 2;
   int   i;
   if (AttribToDecimal (pNode->AttribGet (L"border"), &i))
      pti->iEdgeOuter = i;
   if (AttribToDecimal (pNode->AttribGet (L"innerlines"), &i))
      pti->iEdgeInner = i;
   if (AttribToDecimal (pNode->AttribGet (L"lrmargin"), &i))
      pti->iMarginLeftRight = i;
   if (AttribToDecimal (pNode->AttribGet (L"tbmargin"), &i))
      pti->iMarginTopBottom = i;
   BOOL  fPercent;
   if (AttribToDecimalOrPercent (pNode->AttribGet (L"width"), &fPercent, &i)) {
      if (fPercent)
         i = i * iPercent / 100;
      pti->iWidth = i;
   }
   if (AttribToDecimalOrPercent (pNode->AttribGet (L"height"), &fPercent, &i)) {
      if (fPercent)
         i = i * iPercent / 100;
      pti->iHeight = i;
   }

}

/******************************************************************************
CEscTextBlock::TagTable - Handle a table, defined by <table>

inputs
   PCMMLNode      pNode - Node to parse
retrns
   BOOL - TRUE if succed. FALSE if fails
*/
BOOL CEscTextBlock::TagTable (PCMMLNode pNode)
{
   TABLEINFO   ti;
   memset (&ti, 0, sizeof(ti));
   ti.cBack = (DWORD)-1;
   ti.cEdge = RGB(0,0,0);
   ti.dwAlignHorz = ti.dwAlignVert = 0;
   ti.iEdgeInner = 1;
   ti.iEdgeOuter = 2;
   ti.iHeight = 0;
   ti.iWidth = m_iWidth * 8 / 10;
   ti.iMarginLeftRight = 4;
   ti.iMarginTopBottom = 4;

   FillInTABLEINFO (pNode, &ti, m_iWidth);

   // also get information about the  positioning of the table within the text
   DWORD dwPositioning;
   if (!AttribToPositioning (pNode->AttribGet(L"posn"), &dwPositioning))
      dwPositioning = TWTEXT_INLINE_CENTER;

   // loop through the contents and find the rows. For each row, call in TagRow.
   DWORD i;
   PWSTR psz;
   PCMMLNode   pSub;

   // first, count the number of rows
   DWORD dwRows;
   dwRows = 0;
   for (i = 0; i < pNode->ContentNum(); i++) {
      pNode->ContentEnum (i, &psz, &pSub);

      if (!pSub)
         continue;
      if (_wcsicmp(pSub->NameGet(), L"tr"))
         continue;   // ignore non-rows
      dwRows++;
   }
   // if no rows then no table, so quit
   if (!dwRows)
      return TRUE;

   // place to store the results from the rows
   CListVariable  *plistTWTEXTELEM;
   CListFixed     *plistTWOBJECTPOSN;
   CListVariable  listTWTEXTELEM2;
   CListFixed     listTWOBJECTPOSN2;
   plistTWTEXTELEM = new CListVariable;
   plistTWOBJECTPOSN = new CListFixed;
   if (!plistTWTEXTELEM || !plistTWOBJECTPOSN) {
      if (plistTWTEXTELEM)
         delete plistTWTEXTELEM;
      if (plistTWOBJECTPOSN)
         delete plistTWOBJECTPOSN;
         m_pError->Set (1, gaszErrorText[1]);
         return FALSE;
   }
   plistTWOBJECTPOSN->Init (sizeof(TWOBJECTPOSN));
   listTWOBJECTPOSN2.Init (sizeof(TWOBJECTPOSN));

   DWORD dwCurRow;
   int   iCurHeight;
   dwCurRow = 0;
   iCurHeight = 0;
   for (i = 0; i < pNode->ContentNum(); i++) {
      pNode->ContentEnum (i, &psz, &pSub);

      if (!pSub)
         continue;

      // if it's a backround object then process it and merge it in
      DWORD dwPositioning;
      if (AttribToPositioning (pSub->AttribGet(L"posn"), &dwPositioning) && (dwPositioning == TWTEXT_BACKGROUND)) {
         CEscTextBlock interp;

         // it's a special background object, so add it

         // set up some variables so it uses the font cache and stuff
         interp.Init();
         interp.OtherError (m_pError);
         interp.OtherControlCache (m_plistPCESCCONTROL);
         interp.OtherFontCache (m_pFontCache);
         interp.m_fi = m_fi; // keep the same font into

         if (!interp.Interpret (m_pPage, m_pControl, pSub, m_hDC, m_hInstance, ti.iWidth)) {
            delete plistTWTEXTELEM;
            delete plistTWOBJECTPOSN;
            return FALSE;
         }

         // merge in
         plistTWTEXTELEM->Merge (interp.m_plistTWTEXTELEM);
         plistTWOBJECTPOSN->Merge (interp.m_plistTWOBJECTPOSN);
         if (!m_pNodePageInfo)
            m_pNodePageInfo = interp.m_pNodePageInfo;
         continue;
      }

      if (_wcsicmp(pSub->NameGet(), L"tr"))
         continue;   // ignore non-rows

      TABLEINFO   ti2;
      int   iHeight;
      ti2 = ti;
      if (!dwCurRow)
         ti2.iHeight = ti2.iHeight - (ti2.iHeight / dwRows) * (dwRows-1);
      else
         ti2.iHeight /= dwRows;

      listTWTEXTELEM2.Clear();
      listTWOBJECTPOSN2.Clear();
      if (!TagRow (pSub, &ti2, &iHeight, &listTWTEXTELEM2, &listTWOBJECTPOSN2, !dwCurRow, dwCurRow == (dwRows-1))) {
         delete plistTWTEXTELEM;
         delete plistTWOBJECTPOSN;
         return FALSE;
      }

      // move all the elements vertically and combine
      if (!TextMove (0, iCurHeight, &listTWTEXTELEM2, &listTWOBJECTPOSN2)) {
         delete plistTWTEXTELEM;
         delete plistTWOBJECTPOSN;
         m_pError->Set (1, gaszErrorText[1]);
         return FALSE;
      }

      // merge in
      plistTWTEXTELEM->Merge (&listTWTEXTELEM2);
      plistTWOBJECTPOSN->Merge (&listTWOBJECTPOSN2);

      iCurHeight += iHeight;
      dwCurRow++;
   }

   // need to wrap this all into an object and add it using the positioning
   TABLEOBJECT *pto;
   pto = (TABLEOBJECT*) ESCMALLOC (sizeof(TABLEOBJECT));
   if (!pto) {
      delete plistTWTEXTELEM;
      delete plistTWOBJECTPOSN;
      return FALSE;
   }
   pto->dwID = MMLOBJECT_TABLE;
   pto->plistTWOBJECTPOSN = plistTWOBJECTPOSN;
   pto->plistTWTEXTELEM = plistTWTEXTELEM;

   // add it
   AddObject ((QWORD) pto, ti.iWidth, iCurHeight, dwPositioning);

   // dont delete the plistXXX because they're now in the object
   return TRUE;
}


/******************************************************************************
CEscTextBlock::TagRow - Given a row tag (which has been verified, this
   gets the attributes nad looks cell tags. These are then calculated and
   combined together.

inputs
   PMMLNode       pNode - node
   TABLEINFO      *ptai - table into
   int            *piHeight - filled with the height of the rows
   CVariableList  *plte - Pointer to cleared list of TWTEXTELEM to fill in
   CFixedList     *plop - Pointer to cleared list of TWOBJECTPOSN to fill in
   BOOL           fTopEdge - If TRUE, the top is the edge of the table
   BOOL           fBottomEdge - If TRUE, the bottom is the edge of the table
returns
   BOOL - TRUE if OK
*/
typedef struct {
   PCMMLNode      pNode;   // node for the column
   TABLEINFO      ti;
   int            iHeight; // height that it ends up being
   CEscTextBlock   *pInterp;   // interpret object
} TABLECELL, *PTABLECELL;

BOOL CEscTextBlock::TagRow (PCMMLNode pNode, TABLEINFO *ptai, int *piHeight,
                            CListVariable *plte, CListFixed *plop,
                            BOOL fTopEdge, BOOL fBottomEdge)
{
   BOOL  fError = TRUE;
   CListFixed  lc;
   lc.Init (sizeof(TABLECELL));

   // default to left alignment in table row
   IFONTINFO fi;
   fi = m_fi;
   m_fi.fi.dwJust = 0;

   // step one, figure out how many columns there are
   DWORD i;
   WCHAR *psz;
   PCMMLNode   pSub;
   lc.Required (pNode->ContentNum());
   for (i = 0; i < pNode->ContentNum(); i++) {
      pNode->ContentEnum (i, &psz, &pSub);

      if (!pSub)
         continue;
      if (_wcsicmp(pSub->NameGet(), L"td") && _wcsicmp(pSub->NameGet(), L"th"))
         continue;   // ignore non-rows
      
      // fill it in
      TABLECELL  tc;
      memset (&tc, 0, sizeof(tc));
      tc.pNode = pSub;
      tc.ti = *ptai;

      lc.Add (&tc);
   }
   // if no cells then no table, so quit
   if (!lc.Num())
      return TRUE;


   // else, loop through each cell and figure out how large it thinks it should be
   TABLECELL   *ptc;
   int   iTotalWidth;
   iTotalWidth = 0;
   for (i = 0; i < lc.Num(); i++) {
      ptc = (TABLECELL*) lc.Get (i);
      
      // adjust the width to 1/N
      ptc->ti.iWidth /= lc.Num();

      // however, get the attributes and ask it
      FillInTABLEINFO (ptc->pNode, &ptc->ti, ptai->iWidth);

      // keep track of the new width
      iTotalWidth += ptc->ti.iWidth;
   }
   if (!iTotalWidth)
      iTotalWidth = 1;

   // go back and adjust the widths again because it might be too large/small
   int   iTotalWidth2;
   iTotalWidth2 = 0;
   for (i = 0; i < lc.Num(); i++) {
      ptc = (TABLECELL*) lc.Get (i);

      // scale so they'll all fit
      ptc->ti.iWidth = (int) ((double) ptc->ti.iWidth * (double) ptai->iWidth /
         (double)iTotalWidth);

      // since round off error
      iTotalWidth2 += ptc->ti.iWidth;

      // if its the last one the add the roundoff
      if (i == (lc.Num()-1))
         ptc->ti.iWidth += (ptai->iWidth - iTotalWidth2);
   }

   // now we know the width and other attributes, do MML -> text for each cell.
   int   iMaxHeight;
   iMaxHeight = 0;
   for (i = 0; i < lc.Num(); i++) {
      ptc = (TABLECELL*) lc.Get (i);

      // create the interpret object
      ptc->pInterp = new CEscTextBlock();
      if (!ptc->pInterp) {
         m_pError->Set (1, gaszErrorText[1]);
         fError = FALSE;
         goto cleanup;
      }

      // init it
      if (!ptc->pInterp->Init ()) {
         m_pError->Set (1, gaszErrorText[1]);
         fError = FALSE;
         goto cleanup;
      }

      // figure out size of the right/left and top/bottom borders/margins
      BOOL  fRightEdge, fLeftEdge;
      int   iExtraWidth, iExtraHeight;
      fRightEdge = (i == (lc.Num()-1));
      fLeftEdge = !i;
      iExtraWidth = 2 * ptc->ti.iMarginLeftRight +
         (fLeftEdge ? ptc->ti.iEdgeOuter : (ptc->ti.iEdgeInner / 2)) +
         (fRightEdge ? ptc->ti.iEdgeOuter : ((ptc->ti.iEdgeInner+1) / 2)); // +1 for roundoff
      iExtraHeight = 2 * ptc->ti.iMarginTopBottom +
         (fTopEdge ? ptc->ti.iEdgeOuter : (ptc->ti.iEdgeInner / 2)) +
         (fBottomEdge ? ptc->ti.iEdgeOuter : ((ptc->ti.iEdgeInner+1) /2)); // +1 for roundoff

      // set up some variables so it uses the font cache and stuff
      ptc->pInterp->OtherError (m_pError);
      ptc->pInterp->OtherControlCache (m_plistPCESCCONTROL);
      ptc->pInterp->OtherFontCache (m_pFontCache);
      ptc->pInterp->m_fi = m_fi; // keep the same font into

      if (!ptc->pInterp->Interpret (m_pPage, m_pControl, ptc->pNode, m_hDC, m_hInstance, ptc->ti.iWidth - iExtraWidth, TRUE)) {
         fError = FALSE;
         goto cleanup;
      }

      if (!m_pNodePageInfo)
         m_pNodePageInfo = ptc->pInterp->m_pNodePageInfo;

      // remember the maximum height
      iMaxHeight = max (iMaxHeight, ptc->pInterp->m_iCalcHeight + iExtraHeight);
      iMaxHeight = max (iMaxHeight, ptc->ti.iHeight);
   }

   // go though all the cells again, centering them and adding them onto plte and plop
   int   iCurWidth;
   iCurWidth = 0;
   for (i = 0; i < lc.Num(); i++) {
      ptc = (TABLECELL*) lc.Get (i);

      // figure out size of the right/left and top/bottom borders/margins
      BOOL  fRightEdge, fLeftEdge;
      int   iExtraWidth, iExtraHeight;
      fRightEdge = (i == (lc.Num()-1));
      fLeftEdge = !i;
      iExtraWidth = 2 * ptc->ti.iMarginLeftRight +
         (fLeftEdge ? ptc->ti.iEdgeOuter : (ptc->ti.iEdgeInner / 2)) +
         (fRightEdge ? ptc->ti.iEdgeOuter : ((ptc->ti.iEdgeInner+1) / 2)); // +1 for roundoff
      iExtraHeight = 2 * ptc->ti.iMarginTopBottom +
         (fTopEdge ? ptc->ti.iEdgeOuter : (ptc->ti.iEdgeInner / 2)) +
         (fBottomEdge ? ptc->ti.iEdgeOuter : ((ptc->ti.iEdgeInner+1) /2)); // +1 for roundoff

      int   iDeltaX, iDeltaY;
      if (ptc->ti.dwAlignHorz == 0) {  // left
         iDeltaX = ptc->ti.iMarginLeftRight +
            (fLeftEdge ? ptc->ti.iEdgeOuter : (ptc->ti.iEdgeInner / 2));
      }
      else if (ptc->ti.dwAlignHorz == 1) { // center
         iDeltaX = (ptc->ti.iWidth - ptc->pInterp->m_iCalcWidth) / 2;
      }
      else {   // right
         iDeltaX = ptc->ti.iWidth - ptc->ti.iMarginLeftRight -
            (fRightEdge ? ptc->ti.iEdgeOuter : ((ptc->ti.iEdgeInner+1) / 2)) -
            ptc->pInterp->m_iCalcWidth;
      }
      iDeltaX += iCurWidth;

      if (ptc->ti.dwAlignVert == 0) { // top
         iDeltaY = ptc->ti.iMarginTopBottom +
            (fTopEdge ? ptc->ti.iEdgeOuter : (ptc->ti.iEdgeInner / 2));
      }
      else if (ptc->ti.dwAlignVert == 1) { // center
         iDeltaY = (iMaxHeight - ptc->pInterp->m_iCalcHeight) / 2;
      }
      else { // bottom
         iDeltaY = iMaxHeight - ptc->ti.iMarginTopBottom -
            (fBottomEdge ? ptc->ti.iEdgeOuter : ((ptc->ti.iEdgeInner+1) /2)) -
            ptc->pInterp->m_iCalcHeight;
      }

      // figure out were the background images go
      RECT  rBack;
      rBack.left = iCurWidth + (fLeftEdge ? ptc->ti.iEdgeOuter : (ptc->ti.iEdgeInner / 2));
      rBack.right = iCurWidth + ptc->ti.iWidth - (fRightEdge ? ptc->ti.iEdgeOuter : ((ptc->ti.iEdgeInner+1) / 2));
      rBack.top = (fTopEdge ? ptc->ti.iEdgeOuter : (ptc->ti.iEdgeInner / 2));
      rBack.bottom = iMaxHeight - (fBottomEdge ? ptc->ti.iEdgeOuter : ((ptc->ti.iEdgeInner+1) /2));

      // move them
      //TextMove (iDeltaX, iDeltaY, ptc->pInterp->m_plistTWTEXTELEM,
      //   ptc->pInterp->m_plistTWOBJECTPOSN);
      ptc->pInterp->PostInterpret (iDeltaX, iDeltaY, &rBack);

      // add table object
      CELLOBJECT  *pco;
      pco = (CELLOBJECT*) ESCMALLOC(sizeof(CELLOBJECT));
      if (!pco) {
         m_pError->Set (1, gaszErrorText[1]);
         fError = FALSE;
         goto cleanup;
      }
      memset (pco, 0, sizeof(*pco));
      pco->cBack = ptc->ti.cBack;
      pco->cEdge = ptc->ti.cEdge;
      pco->dwID = MMLOBJECT_CELL;
      pco->iBottom = fBottomEdge ? ptc->ti.iEdgeOuter : ((ptc->ti.iEdgeInner+1) /2);
      pco->iTop =fTopEdge ? ptc->ti.iEdgeOuter : (ptc->ti.iEdgeInner / 2);
      pco->iLeft = fLeftEdge ? ptc->ti.iEdgeOuter : (ptc->ti.iEdgeInner / 2);
      pco->iRight = fRightEdge ? ptc->ti.iEdgeOuter : ((ptc->ti.iEdgeInner+1) / 2);
      TWOBJECTPOSN   op;
      memset (&op, 0, sizeof(op));
      op.qwID = (QWORD) pco;
      op.dwText = TWTEXT_BEHIND;
      op.dwBehind = 1;
      op.r.left = iCurWidth;
      op.r.right = iCurWidth + ptc->ti.iWidth ;
      op.r.top = 0;
      op.r.bottom = iMaxHeight;
      ptc->pInterp->m_plistTWOBJECTPOSN->Insert (0, &op);   // insert at beginning

      // merge the textelem and objectposn in with the master list
      plte->Merge (ptc->pInterp->m_plistTWTEXTELEM);
      plop->Merge (ptc->pInterp->m_plistTWOBJECTPOSN);

      iCurWidth += ptc->ti.iWidth;
   }

   *piHeight = iMaxHeight;

cleanup:
   for (i = 0; i < lc.Num(); i++) {
      ptc = (TABLECELL*) lc.Get (i);

      if (ptc->pInterp)
         delete ptc->pInterp;
   }

   // restore the font
   m_fi = fi;

   return fError;
}


/******************************************************************************
CEscTextBlock::TagControl - Handle a control tag.

inputs
   PCMMLNode      pNode - node
   PVOID          pCallback - pESCCONTROLCALLBACK
returns
   BOOL - TRUE if OK
*/
BOOL CEscTextBlock::TagControl (PCMMLNode pNode, PVOID pCallback)
{
   CONTROLOBJECT *pio = NULL;
   PCEscControl pControl = NULL;

   // object info
   pio = (CONTROLOBJECT*) ESCMALLOC (sizeof(CONTROLOBJECT));
   if (!pio) {
      m_pError->Set (1, gaszErrorText[1]);
      goto error;
   }
   memset (pio, 0, sizeof(*pio));
   pio->dwID = MMLOBJECT_CONTROL;

   // see if can find another control that matches in the control cache. This
   // way we reuse control objects when the window is resized
   DWORD i;
   pControl = NULL;
   for (i = 0; i < m_plistPCESCCONTROL->Num(); i++) {
      PCEscControl   pTemp;
      pTemp = *((PCEscControl*) m_plistPCESCCONTROL->Get(i));
      if (pTemp->m_pNode == pNode) {
         // found it
         pControl = pTemp;
         pio->pControl = pControl;
         m_plistPCESCCONTROL->Remove (i);
         break;
      }
   }

   if (!pControl) {
      // create the new control
      pControl = new CEscControl;
      if (!pControl) {
         m_pError->Set (1, gaszErrorText[1]);
         goto error;
      }
      pio->pControl = (PVOID) pControl;

      // init the control
      if (!pControl->Init (pNode, &m_fi, m_hInstance, m_pPage, m_pControl, (PESCCONTROLCALLBACK)pCallback)) {
         m_pError->Set (1, gaszErrorText[1]);
         goto error;
      }
   }

   // size?
   ESCMQUERYSIZE  qs;
   memset (&qs, 0, sizeof(qs));
   qs.iDisplayWidth = m_iWidth;
   qs.iHeight = qs.iWidth = 10;
   qs.hDC = m_hDC;
   pControl->Message (ESCM_QUERYSIZE, (PVOID) &qs);

   // position?
   DWORD dwPositioning, dwNeeded;
   WCHAR szPos[64];
   szPos[0] = 0;
   pControl->AttribGet (L"posn", szPos, sizeof(szPos), &dwNeeded);
   if (!AttribToPositioning (szPos, &dwPositioning))
      dwPositioning = TWTEXT_INLINE_CENTER;

   // fill in objectinfo
   if (!AddObject ((QWORD) pio, qs.iWidth, qs.iHeight, dwPositioning, &m_fi))
      goto error;


   return TRUE;

error:
   if (pio)
      ESCFREE (pio);
   if (pControl)
      delete pControl;
   return FALSE;
}



/******************************************************************************
CEscTextBlock::ConvertLinksToControls - Go through all the TWOBJECTPOSN and
look for objects with qwLinkID != 0. If find them, create a CEscControl (of hidden type
"Link") over it so can handle tabbing and clicking.

inputs
   none
returns
   none
*/
BOOL CEscTextBlock::ConvertLinksToControls (void)
{
   DWORD i, dwNum;
   dwNum = m_plistTWOBJECTPOSN->Num();
   CONTROLOBJECT *pio = NULL;
   PCEscControl pControl = NULL;

   TWOBJECTPOSN  *pop;
   for (i = 0; i < dwNum; i++) {
      pop = (TWOBJECTPOSN*) m_plistTWOBJECTPOSN->Get(i);
      if (!pop)
         break;

      if (!pop->qwLink)
         continue;   // no pointer


      // object info
      pio = NULL;
      pControl = NULL;
      pio = (CONTROLOBJECT*) ESCMALLOC (sizeof(CONTROLOBJECT));
      if (!pio) {
         m_pError->Set (1, gaszErrorText[1]);
         goto error;
      }
      memset (pio, 0, sizeof(*pio));
      pio->dwID = MMLOBJECT_CONTROL;

      // see if can find another control that matches in the control cache. This
      // way we reuse control objects when the window is resized
      DWORD i;
      pControl = NULL;
      for (i = 0; i < m_plistPCESCCONTROL->Num(); i++) {
         PCEscControl   pTemp;
         pTemp = *((PCEscControl*) m_plistPCESCCONTROL->Get(i));
         if (pTemp->m_pNode == (PCMMLNode) pop->qwLink) {
            // found it
            pControl = pTemp;
            pio->pControl = pControl;
            m_plistPCESCCONTROL->Remove (i);
            break;
         }
      }

      if (!pControl) {
         // create the new control
         pControl = new CEscControl;
         if (!pControl) {
            m_pError->Set (1, gaszErrorText[1]);
            goto error;
         }
         pio->pControl = (PVOID) pControl;

         // init the control
         if (!pControl->Init ((PCMMLNode)pop->qwLink, &m_fi, m_hInstance, m_pPage, m_pControl, ControlLink)) {
            m_pError->Set (1, gaszErrorText[1]);
            goto error;
         }
      }

      // reset values for pop
      pop->qwID = (QWORD) pio;
      pop->qwLink = 0;

   }

   // done
   return TRUE;

   error:
      if (pio)
         ESCFREE (pio);
      if (pControl)
         delete pControl;
      return FALSE;
}


/******************************************************************************
CEscTextBlock::PullOutTables - Go through the OBJECTPOSN structures and find out
where all the tables were placed. (MMOBJECT_TABLE) If find them, yank them out of
the list, and merge their respective objects and text elemens into the greater list.

inputs
   none
returns
   none
*/
void CEscTextBlock::PullOutTables (void)
{
   DWORD i, dwNum;
   dwNum = m_plistTWOBJECTPOSN->Num();

   TWOBJECTPOSN  *pop;
   for (i = 0; i < dwNum; i++) {
      pop = (TWOBJECTPOSN*) m_plistTWOBJECTPOSN->Get(i);
      if (!pop)
         break;

      if (!pop->qwID)
         continue;   // no pointer

      TABLEOBJECT *pto;
      pto = (PTABLEOBJECT) pop->qwID;
      if (pto->dwID != MMLOBJECT_TABLE)
         continue;

      // else, remember it and delete the entry
      TWOBJECTPOSN po;
      po = *pop;
      pop = NULL;
      m_plistTWOBJECTPOSN->Remove (i);
      i--;  // since i++ will offset
      dwNum--; // since deleted

      // center the elements
      TextMove (po.r.left, po.r.top, pto->plistTWTEXTELEM, pto->plistTWOBJECTPOSN);

      // resize the background
      TextBackground (&po.r, pto->plistTWOBJECTPOSN);

      // merge them in
      m_plistTWTEXTELEM->Merge(pto->plistTWTEXTELEM);
      m_plistTWOBJECTPOSN->Merge(pto->plistTWOBJECTPOSN);

      // free stuff up
      delete pto->plistTWOBJECTPOSN;
      delete pto->plistTWTEXTELEM;
      ESCFREE (pto);
   }

   // done
}

/******************************************************************************
CEscTextBlock::FreeControls - If there are any controls on the m_plistPCESCCONTROL
that were stored away because they're to be reused (when ReInterpreting) then
delete them.

inputs
   none
returns
   none
*/
void CEscTextBlock::FreeControls (void)
{
   DWORD i;
   PCEscControl   pControl;
   for (i = 0; i < m_plistPCESCCONTROL->Num(); i++) {
      pControl = *((PCEscControl*) m_plistPCESCCONTROL->Get(i));
      if (pControl)
         delete pControl;
   }
   m_plistPCESCCONTROL->Clear();
}

/******************************************************************************
CEscTextBlock::FreeObjects - Look through the m_listTWOBJECTPOSN for objects
(qwID == pointer) and free any up.

inputs
   none
returns
   none
*/
void CEscTextBlock::FreeObjects (void)
{
   DWORD i;

   TWOBJECTPOSN *pop;
   for (i = 0; i < m_plistTWOBJECTPOSN->Num(); i++) {
      pop = (TWOBJECTPOSN*) m_plistTWOBJECTPOSN->Get(i);
      if (!pop || !pop->qwID)
         continue;   // not here

      // else see what it is
      DWORD *pdw;
      pdw = (DWORD*) pop->qwID;
      switch (*pdw) {
      case MMLOBJECT_TABLE:
         {
            // else, remember it and delete the entry
            TABLEOBJECT *pto;
            pto = (TABLEOBJECT*) pdw;

            // free stuff up
            if (pto->plistTWOBJECTPOSN)
               delete pto->plistTWOBJECTPOSN;
            if (pto->plistTWTEXTELEM)
               delete pto->plistTWTEXTELEM;
            ESCFREE (pto);
         }
         break;

      case MMLOBJECT_CONTROL:
         {
            CONTROLOBJECT *pto;
            pto = (CONTROLOBJECT*) pdw;

            // free stuff up
            if (pto->pControl)
               delete ((PCEscControl) pto->pControl);
            ESCFREE (pto);
         }
         break;


      case MMLOBJECT_SECTION:
         {
            SECTIONOBJECT *pto;
            pto = (SECTIONOBJECT*) pdw;

            // free stuff up
            if (pto->pszName)
               ESCFREE (pto->pszName);
            ESCFREE (pto);
         }
         break;


      case MMLOBJECT_CELL:
      case MMLOBJECT_STRETCH:
         {
            // contains nothing
            ESCFREE (pdw);
   #ifdef _DEBUG
            //_CrtCheckMemory ();
   #endif // DEBUG
         }
         break;

      }

      // BUGFIX - Set to 0. See if prevents crash
      // Disabled because didn't see, to make a difference
      // pop->qwID = 0;
   }
}


/******************************************************************************
CEscTextBlock::Interpret - Takes a CMMLNode and interprets it, filling in all
the relevent info such as font cache, text locations, objects, object locations, etc.

   Fills in:
      m_error
      m_listTWTEXTELEM
      m_listTWOBJECTPOSN
      m_FontCache
      m_iCalcWidth
      m_iCalcHeight

  This basically does:
   1) Set up some "global" object variables.
   2) Call "Tag" for the root and work on down the list.
   3) Call the TextWrap code to figure out where the text goes
   creates all objects

inputs
   CEscPage    pPage - needed for the controls
   CEscControl pControl - control for parent
   CMMLNode    *pNode - node
   HDC         hDC - HDC to use
   HINSTANCE   hInstance - HINSTANCE for resources
   int         iWidth - Width to generate
   BOOL        fDontClearFont - if set to true, doens't clear the font settings
   BOOL        fRootNodeNULL - Usually set to FALSE, If TRUE, it ignores the node
               type of the root node and just treats it as text. Controls such as
               <button>Press me!</button> use this so they can send their own
               pNode into <button> and get "Press me" as text.
returns
   BOOL - TRUE if OK, FALSE if error. If error, see m_error
*/
BOOL CEscTextBlock::Interpret (CEscPage *pPage, CEscControl *pControl, CMMLNode *pNode,
                               HDC hDC, HINSTANCE hInstance, int iWidth, BOOL fDontClearFont,
                               BOOL fRootNodeNULL)
{
   m_fRootNodeNULL = fRootNodeNULL;
   m_pControl = pControl;
   m_pPage = pPage;
   m_hDC = hDC;
   m_hInstance = hInstance;
   m_iWidth = iWidth;
   if (m_fDeleteCMMLNode && m_pNode)
      delete m_pNode;
   m_pNode = pNode;
   m_pNodePageInfo = NULL;
   m_fFoundStretch = FALSE;

   // clear out some stuff
   if (!m_fUseOtherControlCache)
      FreeControls();
   m_listText.Clear();
   m_listPTWFONTINFO.Clear();
   m_listTWOBJECTINFO.Clear();
   if (!m_fUseOtherError)
      m_pError->Set (0);
   m_plistTWTEXTELEM->Clear();
   // free objects up before clearing objectposn
   FreeObjects ();
   m_plistTWOBJECTPOSN->Clear();
   if (!m_fUseOtherFont)
      m_pFontCache->Clear();
   m_iCalcWidth = m_iCalcHeight = 0;

   if (!fDontClearFont) {
      // set up defaults for the font
      memset (&m_fi, 0, sizeof(m_fi));
      m_fi.iPointSize = 12;
      wcscpy (m_fi.szFont, L"Arial");
      m_fi.fi.cBack = (DWORD)-1;
      m_fi.fi.iLineSpacePar = -100;
      m_fi.fi.iTab = EscGetDeviceCaps(hDC, LOGPIXELSX) / 2; // 1/2" tabs
   }

   // handle the main tag
   if (!Tag (pNode))
      return FALSE;

   // add a null to the end of the text
   WCHAR cNull;
   cNull = 0;
   m_listText.Add (&cNull);

   // text wrap
   HRESULT  hRes;
   hRes = TextWrap (m_hDC, m_iWidth, (PWSTR) m_listText.Get(0),
      (PTWFONTINFO*) m_listPTWFONTINFO.Get(0),
      (PTWOBJECTINFO) m_listTWOBJECTINFO.Get(0), m_listTWOBJECTINFO.Num(),
      &m_iCalcWidth, &m_iCalcHeight,
      m_plistTWTEXTELEM, m_plistTWOBJECTPOSN);
   if (hRes) {
      m_pError->Set (3, gaszErrorText[3]);
      return FALSE;
   }

   // go through all objectposn and find out where tables are.
   // Then, add the tables back in
   PullOutTables ();

   // convert links to controls
   ConvertLinksToControls ();

   // all succede
   return TRUE;
}

/******************************************************************************
CMMLTextBlock::SectionFind - Searches through the data for a <section>
with the specified text. If it finds it, it returns the Y pixel at which the
section starts.

inputs
   PWSTR       psz - String to search for
returns
   int - Pixel at which section starts. -1 if can't find
*/
int CEscTextBlock::SectionFind (PWSTR psz)
{
   DWORD i;
   for (i = 0; i < m_plistTWOBJECTPOSN->Num(); i++) {
      PTWOBJECTPOSN  pop;
      pop = (PTWOBJECTPOSN) m_plistTWOBJECTPOSN->Get(i);
      if (!pop)
         continue;
      if (!pop->qwID)
         continue;

      PSECTIONOBJECT ps;
      ps = (PSECTIONOBJECT) pop->qwID;
      if (ps->dwID != MMLOBJECT_SECTION)
         continue;

      // else found it
      if (!_wcsicmp(psz, ps->pszName))
         return pop->r.top;
   }

   // if get here can't find
   return -1;
}


/******************************************************************************
CMMLTextBlock::SectionFromY - Given a Y value, returns the section that it's
in.

inputs
   int      iY - Y value, in page coordinates
   BOOL     fReturnFirst - If no section has yet been declared, and this is TRUE
               then returns the first section it finds. if FALSE, it reutrns NULL.
returns
   PWSTR - section string. Valid until page is resized or deleted.
*/
PWSTR CEscTextBlock::SectionFromY (int iY, BOOL fReturnFirst)
{
   DWORD i;
   PWSTR pRet = NULL;
   for (i = 0; i < m_plistTWOBJECTPOSN->Num(); i++) {
      PTWOBJECTPOSN  pop;
      pop = (PTWOBJECTPOSN) m_plistTWOBJECTPOSN->Get(i);
      if (!pop)
         continue;
      if (!pop->qwID)
         continue;

      PSECTIONOBJECT ps;
      ps = (PSECTIONOBJECT) pop->qwID;
      if (ps->dwID != MMLOBJECT_SECTION)
         continue;

      // else found a section
      if ((pop->r.top > iY) && (pRet || !fReturnFirst)) {
         // section is lower, so break
         return pRet;
      }

      pRet = ps->pszName;
   }

   // if get here can't find
   return pRet;
}


/******************************************************************************
CMMLTextBlock::Stretch - Called by CEscPage when the window sytle is set to stretching.
This:
   1) Finds the markers for stretchstart & stretchend
   2) Figure out how much need to stretch/shrink the region
   3) For all objects after stretchend, move them down
   4) For all objects within and straddling strechstart/stop, stretch them.
   4) Do the same for text, except text is only moved

Call this before PostInterpret.

inputs
   int      iStretchTo - Height to stretch to. The height used to stretch from is m_iCalcHeight
returns
   BOOL - TRUE if OK
*/
BOOL CEscTextBlock::Stretch (int iStretchTo)
{
   // find the stretch.
   int   iTop = -1, iBottom = -1;
   DWORD i;
   for (i = 0; i < m_plistTWOBJECTPOSN->Num(); i++) {
      PTWOBJECTPOSN  pop;
      pop = (PTWOBJECTPOSN) m_plistTWOBJECTPOSN->Get(i);
      if (!pop)
         continue;
      if (!pop->qwID)
         continue;

      PSTRETCHOBJECT ps;
      ps = (PSTRETCHOBJECT) pop->qwID;
      if (ps->dwID != MMLOBJECT_STRETCH)
         continue;

      // else found it
      if (ps->fStart)
         iTop = pop->r.top;
      else
         iBottom = pop->r.top;
   }

   // if bottom <= top then error
   if (iBottom <= iTop)
      return FALSE;

   // how much would have to stretch
   int   iOrigHeight, iNewHeight, iMoveDown;
   iOrigHeight = iBottom - iTop;
   iNewHeight = max (iOrigHeight + (iStretchTo - m_iCalcHeight), 1);
      // always leave at least one pixel
   iMoveDown = iNewHeight - iOrigHeight;

   // move/stretch objects
   for (i = 0; i < m_plistTWOBJECTPOSN->Num(); i++) {
      PTWOBJECTPOSN  pop;
      pop = (PTWOBJECTPOSN) m_plistTWOBJECTPOSN->Get(i);
      if (!pop)
         continue;

      // luckily, don't need to send move/size messages to controls, since postinterpret does this

      // top
      if (pop->r.top <= iTop) {
         // it's above the stretch region. do nothing
      }
      else if (pop->r.top < iBottom) {
         // it's within the stretch region, so stretch
         pop->r.top = iTop + (int) ((pop->r.top - iTop) / (double) iOrigHeight * (double) iNewHeight);
      }
      else {
         // it's below, move down
         pop->r.top += iMoveDown;
      }

      // bottom
      if (pop->r.bottom <= iTop) {
         // it's above the stretch region. do nothing
      }
      else if (pop->r.bottom < iBottom) {
         // it's within the stretch region, so stretch
         pop->r.bottom = iTop + (int) ((pop->r.bottom - iTop) / (double) iOrigHeight * (double) iNewHeight);
      }
      else {
         // it's below, move down
         pop->r.bottom += iMoveDown;
      }
   }

   // move (but dont stretch) the text
   for (i = 0; i < m_plistTWTEXTELEM->Num(); i++) {
      PTWTEXTELEM pte;
      pte = (PTWTEXTELEM) m_plistTWTEXTELEM->Get(i);
      if (!pte) continue;

      // what's the offset
      int   iNewBaseline;
      if (pte->iBaseline <= iTop) {
         // it's above the stretch region. do nothing
         continue;
      }
      else if (pte->iBaseline < iBottom) {
         // it's within the stretch region, so stretch
         iNewBaseline = iTop + (int) ((pte->iBaseline - iTop) / (double) iOrigHeight * (double) iNewHeight);
      }
      else {
         // it's below, move down
         iNewBaseline = pte->iBaseline + iMoveDown;
      }

      // moove it down
      iNewBaseline -= pte->iBaseline;
      pte->iBaseline += iNewBaseline;
      OffsetRect(&pte->r, 0, iNewBaseline);
      OffsetRect (&pte->rBack, 0, iNewBaseline);
   }

   // all moved, reset the calc height
   m_iCalcHeight += iMoveDown;
   return TRUE;
}

/******************************************************************************
CMMLReInterpret::ReInterpret - ReInterprets the same nodes as has been used
in the last Interpret, just in case the width has changed.

   Fills in:
      m_error
      m_listTWTEXTELEM
      m_listTWOBJECTPOSN
      m_FontCache
      m_iCalcWidth
      m_iCalcHeight
      also controls/objects

  This basically does:
   1) Set up some "global" object variables.
   2) Call "Tag" for the root and work on down the list.
   3) Call the TextWrap code to figure out where the text goes
   reuses objects if their pNodes match

inputs
   HDC         hDC - HDC to use
   int         iWidth - Width to generate
   BOOL        fDontClearFont - if set to true, doens't clear the font settings
returns
   BOOL - TRUE if OK, FALSE if error. If error, see m_error
*/
BOOL CEscTextBlock::ReInterpret (HDC hDC, int iWidth, BOOL fDontClearFont)
{
   m_hDC = hDC;
   m_iWidth = iWidth;
   m_pNodePageInfo = NULL;
   m_fFoundStretch = FALSE;

   // look through all the objects for controls and cpy them to m_plistPCESCCONTROL
   // so we can reuse them
   FreeControls();
   DWORD i;
   TWOBJECTPOSN *pop;
   m_plistPCESCCONTROL->Required (m_plistTWOBJECTPOSN->Num());
   for (i = 0; i < m_plistTWOBJECTPOSN->Num(); i++) {
      pop = (TWOBJECTPOSN*) m_plistTWOBJECTPOSN->Get(i);
      if (!pop || !pop->qwID)
         continue;   // not here

      // else see what it is
      CONTROLOBJECT *pto;
      pto = (CONTROLOBJECT*) pop->qwID;
      if (pto->dwID != MMLOBJECT_CONTROL)
         continue;

      m_plistPCESCCONTROL->Add (&pto->pControl);
      pto->pControl = NULL;
   }

   // clear out some stuff
   m_listText.Clear();
   m_listPTWFONTINFO.Clear();
   m_listTWOBJECTINFO.Clear();
   if (!m_fUseOtherError)
      m_pError->Set (0);
   m_plistTWTEXTELEM->Clear();
   // free objects up before clearing objectposn
   if (!m_fUseOtherControlCache)
      FreeObjects ();
   m_plistTWOBJECTPOSN->Clear();
   if (!m_fUseOtherFont)
      m_pFontCache->Clear();
   m_iCalcWidth = m_iCalcHeight = 0;

   if (!fDontClearFont) {
      // set up defaults for the font
      memset (&m_fi, 0, sizeof(m_fi));
      m_fi.iPointSize = 12;
      wcscpy (m_fi.szFont, L"Arial");
      m_fi.fi.cBack = (DWORD)-1;
      m_fi.fi.iLineSpacePar = -100;
      m_fi.fi.iTab = EscGetDeviceCaps(hDC, LOGPIXELSX) / 2; // 1/2" tabs
   }

   // handle the main tag
   if (!Tag (m_pNode))
      return FALSE;

   // add a null to the end of the text
   WCHAR cNull;
   cNull = 0;
   m_listText.Add (&cNull);

   // text wrap
   HRESULT  hRes;
   hRes = TextWrap (m_hDC, m_iWidth, (PWSTR) m_listText.Get(0),
      (PTWFONTINFO*) m_listPTWFONTINFO.Get(0),
      (PTWOBJECTINFO) m_listTWOBJECTINFO.Get(0), m_listTWOBJECTINFO.Num(),
      &m_iCalcWidth, &m_iCalcHeight,
      m_plistTWTEXTELEM, m_plistTWOBJECTPOSN);
   if (hRes) {
      m_pError->Set (3, gaszErrorText[3]);
      return FALSE;
   }

   // go through all objectposn and find out where tables are.
   // Then, add the tables back in
   PullOutTables ();

   // convert links to controls
   ConvertLinksToControls ();

   // free any controls left over - there shouldn't be any
   FreeControls();

   // all succede
   return TRUE;
}

/*********************************************************************************
CEscTextBlock::Paint - Paints the text block.

inputs
   HDC         hDC - DC to paint into
   POINT       *pOffset - XY offset of text data
   RECT        *prClip - Clip rectangle for HDC.
                  Can be NULL
   RECT        *prScreen - Location of prClip in screen coordinates.
                  Can be NULL
   RECT        *prTotalScreen - Describes total screen size. Some controls use
                  this for drawing.
returns
   BOOL - TRUE if OK
*/
BOOL CEscTextBlock::Paint (HDC hDC, POINT *pOffset, RECT *prClip, RECT *prScreen,
                           RECT *prTotalScreen)
{
   TextPaint (hDC, pOffset, m_plistTWTEXTELEM, m_plistTWOBJECTPOSN, prClip, prScreen,
      prTotalScreen);

   return TRUE;
}


/******************************************************************************
CEscTextBlock::PostInterpret - Should be called after Interpret() and after
the caller has decided how to center/offset the text.
   1) moves all the text
   2) Adjusts all the BACKGROUND objects to full size

inputs
   int   iDeltaX, iDeltaY - DeltaX & Y for current text/objects
   RECT  *prFull - Full page/cell rectangle. background objects have their size set to this
returns
   BOOL - error
*/
BOOL CEscTextBlock::PostInterpret (int iDeltaX, int iDeltaY, RECT *prFull)
{
   // move them
   TextMove (iDeltaX, iDeltaY, m_plistTWTEXTELEM, m_plistTWOBJECTPOSN);

   // adjust background
   TextBackground (prFull, m_plistTWOBJECTPOSN);

   // go through all the controls and tell them they've moved
   DWORD i;
   TWOBJECTPOSN *pop;
   for (i = 0; i < m_plistTWOBJECTPOSN->Num(); i++) {
      pop = (TWOBJECTPOSN*) m_plistTWOBJECTPOSN->Get(i);
      if (!pop || !pop->qwID)
         continue;   // not here

      // else see what it is
      CONTROLOBJECT *pto;
      pto = (CONTROLOBJECT*) pop->qwID;
      if (pto->dwID != MMLOBJECT_CONTROL)
         continue;

      // it's a control, so tell it
      PCEscControl   pControl;
      pControl = (PCEscControl) pto->pControl;
      pControl->m_rPosn = pop->r;
      pControl->Message (ESCM_MOVE);
      pControl->Message (ESCM_SIZE);
   }

   return TRUE;
}

/******************************************************************************
CEscTextBlock::FindBreak - This finds a good breaking point between pages,
one that minimizes the number of objects broken in half. It finds the greatest
Y value, within the iYMin and iYMax range that breaks the least number
of objects in half.

inputs
   int      iYMin - Minimum Y value acceptable for a break
   int      iYMax - Maximum Y value acceptable for a break
returns
   int - Breaking point

*/
typedef struct {
   int      iY;      // break point
   DWORD    dwBroken;   // number of objects split at this point
} BREAKINFO, *PBREAKINFO;
int CEscTextBlock::FindBreak (int iYMin, int iYMax)
{
   CListFixed  l;
   l.Init (sizeof(BREAKINFO));
   l.Required (m_plistTWTEXTELEM->Num()+m_plistTWOBJECTPOSN->Num()+2);

   // trivial case
   if (iYMax <= iYMin)
      return iYMax;

   // first, all possible breaks
   BREAKINFO bi, *pbi;
   memset (&bi, 0, sizeof(bi));
   bi.iY = iYMin;
   l.Add (&bi);
   bi.iY = iYMax;
   l.Add (&bi);

   DWORD i, j;
   // text elements
   TWTEXTELEM *pte;
   for (i = 0; i < m_plistTWTEXTELEM->Num(); i++) {
      pte = (TWTEXTELEM*) m_plistTWTEXTELEM->Get(i);
      if (!pte)
         continue;

      if ((pte->r.top >= iYMax) || (pte->r.top <= iYMin))
         continue;   // not in range

      // see if should add
      for (j = 0; j < l.Num(); j++) {
         pbi = (PBREAKINFO) l.Get(j);
         if (pbi->iY == pte->r.top)
            break;   // found match already
      }
      if (j < l.Num())
         continue;   // found match

      // else add
      bi.iY = pte->r.top;
      l.Add (&bi);
   }

   // other objects
   TWOBJECTPOSN *pto;
   for (i = 0; i < m_plistTWOBJECTPOSN->Num(); i++) {
      pto = (TWOBJECTPOSN*) m_plistTWOBJECTPOSN->Get(i);
      if (!pto)
         continue;

      if ((pto->r.top >= iYMax) || (pto->r.top <= iYMin))
         continue;   // not in range

      // see if should add
      for (j = 0; j < l.Num(); j++) {
         pbi = (PBREAKINFO) l.Get(j);
         if (pbi->iY == pto->r.top)
            break;   // found match already
      }
      if (j < l.Num())
         continue;   // found match

      // else add
      bi.iY = pto->r.top;
      l.Add (&bi);
   }


   // we now have a list of potential splits
   // go through the list again, this time counting which potential splits
   // cut objects
   for (i = 0; i < m_plistTWTEXTELEM->Num(); i++) {
      pte = (TWTEXTELEM*) m_plistTWTEXTELEM->Get(i);
      if (!pte)
         continue;

      if ((pte->r.bottom <= iYMin) || (pte->r.top >= iYMax))
         continue;   // not in range

      // see if this breaks any of the lines
      for (j = 0; j < l.Num(); j++) {
         pbi = (PBREAKINFO) l.Get(j);

         if ((pbi->iY > pte->r.top) && (pbi->iY < pte->r.bottom))
            pbi->dwBroken++;
      }
   }
   for (i = 0; i < m_plistTWOBJECTPOSN->Num(); i++) {
      pto = (TWOBJECTPOSN*) m_plistTWOBJECTPOSN->Get(i);
      if (!pto)
         continue;

      if ((pto->r.bottom <= iYMin) || (pto->r.top >= iYMax))
         continue;   // not in range

      // see if this breaks any of the lines
      for (j = 0; j < l.Num(); j++) {
         pbi = (PBREAKINFO) l.Get(j);

         if ((pbi->iY > pto->r.top) && (pbi->iY < pto->r.bottom))
            pbi->dwBroken++;
      }
   }


   // look through the breaks and see which one has the least errors
   PBREAKINFO  pBest;
   pBest = NULL;
   for (j = 0; j < l.Num(); j++) {
      pbi = (PBREAKINFO) l.Get(j);

      // nothing in then use first one
      if (!pBest) {
         pBest = pbi;
         continue;
      }

      // if this has a higher error than best then skip
      if (pbi->dwBroken > pBest->dwBroken)
         continue;

      // if it has a lower error then take hands down
      if (pbi->dwBroken < pBest->dwBroken) {
         pBest = pbi;
         break;
      }

      // else, the errors are the same. only take if futher down
      if (pbi->iY > pBest->iY)
         pBest = pbi;
   }

   // found it
   return pBest->iY;
}


/*********************************************************************************
AttribToHex - Converts an attribute string into hex.

inputs
   WCHAR       *psz - string
   DWORD       *pRet - hex
returns
   BOOL - TRUE if OK, FALSE if error
*/
BOOL AttribToHex (WCHAR *psz, size_t *pRet)
{
   if (!psz) {
      *pRet = 0;
      return FALSE;
   }

   DWORD dw = 0, i;

   for (i = 0; psz[i]; i++) {
      dw *= 16;

      if ((psz[i] >= L'0') && (psz[i] <= L'9'))
         dw += (psz[i] - L'0');
      else if ((psz[i] >= L'a') && (psz[i] <= L'f')) {
         dw += (psz[i] - L'a' + 10);
      }
      else if ((psz[i] >= L'A') && (psz[i] <= L'F')) {
         dw += (psz[i] - L'A' + 10);
      }
      else {
         *pRet = 0;
         return FALSE;
      }
   }

   *pRet = dw;
   return TRUE;
}

/*********************************************************************************
AttribToDecimal - Converts an attribute into decimal

inputs
   WCHAR       *psz - string
   int         *pRet - return
returns
   BOOL - TRUE if OK, FALSE if couldnt convret all
*/
BOOL AttribToDecimal (WCHAR *psz, int *pRet)
{
   if (!psz) {
      *pRet = 0;
      return FALSE;
   }

   WCHAR *pEnd;

   *pRet = wcstol (psz, &pEnd, 10);
   return !pEnd || (pEnd >= (psz + wcslen(psz)));
}

/*********************************************************************************
AttribToDouble - Converts an attribute into double

inputs
   WCHAR       *psz - string
   doubke      *pRet - return
returns
   BOOL - TRUE if OK, FALSE if couldnt convret all
*/
BOOL AttribToDouble (WCHAR *psz, double *pRet)
{
   if (!psz) {
      *pRet = 0;
      return FALSE;
   }

   char  szTemp[64];
   WideCharToMultiByte (CP_ACP, 0, psz, -1, szTemp, sizeof(szTemp), 0, 0);

   *pRet = atof (szTemp);
   return TRUE;
}

/*********************************************************************************
AttribTo3DPoint - Converts an attribute into 3D Point - Fiils in pRet[0], pRet[1], pRet[2]

inputs
   WCHAR       *psz - string
   doubke      *pRet - return. 4 doubles
returns
   BOOL - TRUE if OK, FALSE if couldnt convret all
*/
BOOL AttribTo3DPoint (WCHAR *psz, double *pRet)
{
   pRet[0] = pRet[1] =pRet[2] = pRet[3] = 0;
   if (!psz) {
      return FALSE;
   }

   char  szTemp[128];
   WideCharToMultiByte (CP_ACP, 0, psz, -1, szTemp, sizeof(szTemp), 0, 0);

   CHAR     *pNext, *pCur;
   DWORD i;
   pCur = szTemp;
   for (i = 0; (i < 4) && pCur; i++, pCur = pNext) {
      pNext = strchr (pCur, ',');
      if (pNext) {
         *pNext = 0;
         pNext++;
      }
      pRet[i] = atof (pCur);
   }

   return TRUE;
}

/*********************************************************************************
AttribToYesNo - Converts an attribute a BOOL for a YES/NO answer. Their yes/no
   or TRUE/FALSE

inputs
   WCHAR       *psz - string
   BOOL        *pRet - return
returns
   BOOL - TRUE if OK, FALSE if couldnt convret all
*/
BOOL AttribToYesNo (WCHAR *psz, BOOL *pRet)
{
   if (!psz) {
      *pRet = 0;
      return FALSE;
   }

   if (!_wcsicmp(psz, L"yes") || !_wcsicmp(psz, L"true")) {
      *pRet = TRUE;
      return TRUE;
   }
   else if (!_wcsicmp(psz, L"no") || !_wcsicmp(psz, L"false")) {
      *pRet = FALSE;
      return TRUE;
   }

   // else unknown
   return FALSE;
}

/*********************************************************************************
AttribToColor - Converts an attribute string to a RGB color.
   Right now the form is "#RRGGBB", where RGB in hex.

inputs
   WCHAR       *psz - string
   COLORREF    *pRet - Filled with the return
returns
   BOOL - TRUE if successful convert, FALSE if not
*/
BOOL AttribToColor (WCHAR *psz, COLORREF *pRet)
{
   if (!psz) {
      *pRet = 0;
      return FALSE;
   }

   if (psz[0] != L'#') {
      if (!_wcsicmp(psz, L"transparent") || !_wcsicmp(psz, L"clear")) {
         *pRet = (DWORD)-1;
         return TRUE;
      }

      *pRet = 0;
      return FALSE;
   }

   BOOL  fRet;
   size_t dw;
   fRet = AttribToHex (psz+1, &dw);
   *pRet = (DWORD)dw;
   *pRet = RGB(*pRet >> 16, (*pRet >> 8) & 0xff, *pRet & 0xff);
   return fRet;
}

/*********************************************************************************
AttribToDecimalOrPercent - Converts an attribute string to a decimal or percent. This means
the attribute must end in a '%' sign for the percent.

inputs
   WCHAR          *psz - string
   BOOL           *pfPercent - TRUE if its a percent. Else decimal
   int            *pRef - filled with return
returns
   BOOL - TRUE if succesful convet
*/
BOOL AttribToDecimalOrPercent (WCHAR *psz, BOOL *pfPercent, int *pRet)
{
   *pfPercent = FALSE;
   if (!psz) {
      *pRet = 0;
      return FALSE;
   }

   int   iLen;
   iLen = (int) wcslen(psz);
   if (iLen && (psz[iLen-1] == L'%'))
      *pfPercent = TRUE;

   WCHAR c;
   BOOL  fRet;
   if (*pfPercent) {
      c = psz[iLen-1];
      psz[iLen-1] = 0;
   }
   fRet = AttribToDecimal (psz, pRet);
   if (*pfPercent)
      psz[iLen-1] = c;

   return TRUE;

}


/*********************************************************************************
AttribToAccelerator - Parse an attribute and convert it to an accelerator.

inputs
   WCHAR          *psz -s tring
   PESCACCELERATOR pRet - Filled in
returns
   BOOL - TRUE if succesful convert
*/
BOOL AttribToAccelerator (WCHAR *psz, PESCACCELERATOR pRet)
{
   memset (pRet, 0, sizeof(*pRet));
   if (!psz)
      return FALSE;

   WCHAR szAlt[] = L"alt-";
   WCHAR szShift[] = L"shift-";
   WCHAR szCtl[] = L"ctl-";
   WCHAR szCtrl[] = L"ctrl-";
   WCHAR szControl[] = L"control-";

   // repeat
   while (TRUE) {
      if (!_wcsnicmp (psz, szAlt, wcslen(szAlt))) {
         pRet->fAlt = TRUE;
         psz += wcslen(szAlt);
         continue;
      }

      if (!_wcsnicmp (psz, szShift, wcslen(szShift))) {
         pRet->fShift = TRUE;
         psz += wcslen(szShift);
         continue;
      }

      if (!_wcsnicmp (psz, szCtl, wcslen(szCtl))) {
         pRet->fControl = TRUE;
         psz += wcslen(szCtl);
         continue;
      }

      if (!_wcsnicmp (psz, szCtrl, wcslen(szCtrl))) {
         pRet->fControl = TRUE;
         psz += wcslen(szCtrl);
         continue;
      }

      if (!_wcsnicmp (psz, szControl, wcslen(szControl))) {
         pRet->fControl = TRUE;
         psz += wcslen(szControl);
         continue;
      }

      if (!psz[0])
         return FALSE;

      // enter, escape
      WCHAR szEnter[] = L"enter";
      WCHAR szEscape[] = L"escape";
      if (!_wcsicmp (psz, szEnter)) {
         pRet->c = VK_RETURN;
         break;
      }
      if (!_wcsicmp (psz, szEscape)) {
         pRet->c = VK_ESCAPE;
         break;
      }

      // should have a letter
      if (psz[1])
         return FALSE;

      pRet->c = (WCHAR) towupper (psz[0]);
      break;
   }

   return TRUE;
}

/*********************************************************************************
AttribToPercent - Converts an attribute string to a percent. This means
the attribute must end in a '%' sign for the percent.

inputs
   WCHAR          *psz - string
   int            *pRef - filled with return
returns
   BOOL - TRUE if succesful convet
*/
BOOL AttribToPercent (WCHAR *psz, int *pRet)
{
   BOOL  fPercent;

   if (!AttribToDecimalOrPercent (psz, &fPercent, pRet))
      return FALSE;
   if (!fPercent)
      return FALSE;

   return TRUE;
}
/*********************************************************************************
AttribToPositioning - Converts the attribute into positiong information for objects. The
following strings are valid: Use the "posn" variable.
   inlinetop, inlinecenter, inlinebottom - it's in the line & vertical position
   edgeleft, edgeright - on the edge & left/right side
   if last character contains "-" then it's located behind ordinary text/objects


#define TWTEXT_BEHIND         0x00000001     // sits behidn the text. If not set then text wraps around
#define TWTEXT_INLINE         0x00000100     // it inline with the text. If not, then is to the right/left of the section
#define TWTEXT_INLINE_TOP     0x00010100     // inline & the top of object is aligned with top of text
#define TWTEXT_INLINE_CENTER  0x00020100     // inline & the object is centered with the text
#define TWTEXT_INLINE_BOTTOM  0x00030100     // inline & the bottom of the object is aligned with the baseline of the text.
#define TWTEXT_INLINE_MASK    0x00ff0100
#define TWTEXT_EDGE_LEFT      0x00010000     // not inline & the object is on the left side
#define TWTEXT_EDGE_RIGHT     0x00020000     // not inline & the object is on the right side
#define TWTEXT_EDGE_MASK      0x00ff0000
#define TWTEXT_BACKGROUND

inputs
   WCHAR       *psz - string
   DWORD       *pRet - return
returns
   BOOL - TRUE if OK, FALSE if couldnt convret all
*/
BOOL AttribToPositioning (WCHAR *psz, DWORD *pRet)
{
   *pRet = 0;

   if (!psz) {
      return FALSE;
   }

   int   iLen;
   iLen = (int) wcslen(psz);
   BOOL  fMinus;
   fMinus = FALSE;
   if (iLen && (psz[iLen-1] == L'-')) {
      *pRet |= TWTEXT_BEHIND;
      psz[iLen-1] = 0;
      fMinus = TRUE;
   }

   if (!_wcsicmp (psz, L"inlinetop"))
      *pRet |= TWTEXT_INLINE_TOP;
   else if (!_wcsicmp (psz, L"inlinecenter"))
      *pRet |= TWTEXT_INLINE_CENTER;
   else if (!_wcsicmp (psz, L"inlinemiddle"))
      *pRet |= TWTEXT_INLINE_CENTER;
   else if (!_wcsicmp (psz, L"inlinebottom"))
      *pRet |= TWTEXT_INLINE_BOTTOM;
   else if (!_wcsicmp (psz, L"edgeleft"))
      *pRet |= TWTEXT_EDGE_LEFT;
   else if (!_wcsicmp (psz, L"edgeright"))
      *pRet |= TWTEXT_EDGE_RIGHT;
   else if (!_wcsicmp (psz, L"background"))
      *pRet |= TWTEXT_BACKGROUND;
   else
      *pRet |= TWTEXT_INLINE_CENTER;   // default



   if (fMinus)
      psz[iLen-1] = L'-';  // restore -
   return TRUE;
}


/*********************************************************************************
ColorToAttrib - Fills in a string based on the colorred

inputs
   COLORREF    cr - colorref
   PWSTR       psz - Filled in
*/
void ColorToAttrib (PWSTR psz, COLORREF cr)
{
   if (cr == (DWORD)-1) {
      wcscpy (psz, L"transparent");
      return;
   }

   // else #
   // note that flipping the order
   swprintf (psz, L"#%x", RGB(GetBValue(cr), GetGValue(cr), GetRValue(cr)));

}


// FUTURE - handle a language setting so tags/text not corresponding to the correct
//    language are eliminated. Can handle several languages on one page.

// BUGBUG - 2.0 - need a way can set emboss text, etc.

// BUGBUG - 2.0 - Way can tell a table to shrink to the size of its largest element.

// BUGBUG - 2.0 - tables don't let buttons take up 100% vertical. See month.mml


