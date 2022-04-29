/***********************************************************************
CEscControl - Code for CEscControl

begun 3/29/2000 by Mike Rozak
Copyright 2000 mike Rozak. All rights reserved
*/

#include "mymalloc.h"
#include <windows.h>
#include <stdlib.h>
#include <stdio.h>
#include "escarpment.h"
#include "tools.h"
#include "fontcache.h"
#include "paint.h"
#include "textwrap.h"
#include "mmlparse.h"
#include "mmlinterpret.h"
#include "escarpment.h"
#include "resleak.h"


/* defined */
#define  ATTRIB_HEX        1
#define  ATTRIB_DECIMAL    2
#define  ATTRIB_COLOR      3
#define  ATTRIB_DECIMALORPERCENT 4
#define  ATTRIB_BOOL       5
#define  ATTRIB_PERCENT    6
#define  ATTRIB_ACCELERATOR 7
#define  ATTRIB_STRING1    8
#define  ATTRIB_STRING2    9
#define  ATTRIB_DOUBLE     10
#define  ATTRIB_3DPOINT    11
#define  ATTRIB_CMEM       12

#define  ESCM_KEYHELP      (ESCM_USER+8345)

typedef struct {
   DWORD    dwType;  // ATTRIBX_
   PVOID    p;       // main pointer
   PVOID    p2;      // secondary pointer. used for DecimalOrPercent
   BOOL     *pfDirty;   // dirty flag
   BOOL     fRepaint;   // repaint if TRUE
   DWORD    dwMessage;  // message sent when changed
} ATTRIBCONVERT, *PATTRIBCONVERT;

/* globals */
static CBTree  gtreeControlCallback;

/***********************************************************************
Constructor & destructor
*/
CEscControl::CEscControl (void)
{
   m_pNode = NULL;
   memset (&m_fi, 0, sizeof(m_fi));
   m_hInstance = NULL;
   m_pParentPage = NULL;
   m_pParentControl = NULL;
   m_pCallback = NULL;
   memset (&m_rPosn, 0, sizeof(m_rPosn));
   m_fEnabled = TRUE;
   m_fDefControl = FALSE;
   m_fFocus = FALSE;
   m_fMouseOver = FALSE;
   m_fLButtonDown = FALSE;
   m_fMButtonDown = FALSE;
   m_fRButtonDown = FALSE;
   m_fCapture = FALSE;
   m_pszName = NULL;
   m_dwWantFocus = 0;
   m_fWantMouse = FALSE;
   m_listAccelFocus.Init (sizeof(ESCACCELERATOR));
   m_listAccelNoFocus.Init (sizeof(ESCACCELERATOR));
   memset(&m_AccelSwitch, 0, sizeof(m_AccelSwitch));
   m_dwTimerID = 0;
   m_fRedrawOnMove = FALSE;
}

CEscControl::~CEscControl (void)
{
   Message (ESCM_DESTRUCTOR);

   if (m_dwTimerID)
      m_pParentPage->m_pWindow->TimerKill (m_dwTimerID);
}


void* CEscControl::operator new (size_t s)
{
   return MYMALLOC(s);
}

void  CEscControl::operator delete (void* p)
{
   MYFREE (p);
}


/***********************************************************************
Init - Initializes the control. this does:
   1) Set a bunch of variables.
   2) Call ESCM_INITCONTROL

inputs
   PCMMLNODE      pNode - Node to use. This pointer is used, but the data it in it not touched
   IFONTINFO      *pfi - FOnt info to use
   HINSTANCE      hInstance - hInstance
   CEscPage       *pParentPage - Page which is ultimately parent
   CEscControl    *pParentControl - Parent control if this has one. Else NULL
   PESCCONTROLCALLBACK pCallback - Callback for control messages
returns
   BOOL - TRUE if success
*/
BOOL CEscControl::Init (PCMMLNode pNode, IFONTINFO *pfi, HINSTANCE hInstance,
   CEscPage *pParentPage, CEscControl *pParentControl, PESCCONTROLCALLBACK pCallback)
{
   // make sure not already initialized
   if (m_pNode)
      return FALSE;
   
   // copy variables
   m_pNode = pNode;
   m_fi = *pfi;
   m_fi.fi.dwAutoNumber = FALSE;
   m_fi.fi.iIndentLeftFirst = 0;
   m_fi.fi.iIndentLeftWrap = 0;
   m_fi.fi.iIndentRight = 0;
   m_fi.fi.iSuper = 0;
   m_fi.fi.qwLinkID = 0;
   m_fi.fi.dwJust = 0;
   m_hInstance = hInstance;
   m_pParentPage = pParentPage;
   m_pParentControl = pParentControl;
   m_pCallback = pCallback;

   // add the name to the list
   AttribListAddString (L"name", &m_pszName);

   // add the accelerator to the list
   m_AccelSwitch.dwMessage = ESCM_SWITCHACCEL;
   AttribListAddAccelerator (L"accel", &m_AccelSwitch);
   if (m_pNode->ContentFind(L"HoverHelp") != (DWORD)-1) {
      ESCACCELERATOR a;
      memset (&a, 0, sizeof(a));
      a.c = VK_F1;
      a.dwMessage = ESCM_KEYHELP;
      m_listAccelFocus.Add (&a);
   }


   // add enabled
   AttribListAddBOOL (L"enabled", &m_fEnabled);
   AttribListAddBOOL (L"defcontrol", &m_fDefControl);

   Message (ESCM_CONSTRUCTOR);

   // set up some some stuff...

   // attributes
   DWORD i;
   PWSTR pszAttrib, pszValue;
   for (i = 0; i < m_pNode->AttribNum(); i++) {
      if (!m_pNode->AttribEnum (i, &pszAttrib, &pszValue))
         continue;
      AttribListSet (pszAttrib, pszValue);
   }

   Message (ESCM_INITCONTROL);

   return TRUE;
}


/***********************************************************************
CoordXXXToXXX - Converts coordinates from page to window (client) to
screen, and back. Either converts points or rects.
*/
void CEscControl::CoordPageToWindow (POINT *pPage, POINT *pWindow)
{
   m_pParentPage->CoordPageToWindow (pPage, pWindow);
}

void CEscControl::CoordPageToWindow (RECT *pPage, RECT *pWindow)
{
   m_pParentPage->CoordPageToWindow (pPage, pWindow);
}

void CEscControl::CoordWindowToPage (POINT *pWindow, POINT *pPage)
{
   m_pParentPage->CoordWindowToPage (pWindow, pPage);
}

void CEscControl::CoordWindowToPage (RECT *pWindow, RECT *pPage)
{
   m_pParentPage->CoordWindowToPage (pWindow, pPage);
}

void CEscControl::CoordPageToScreen (POINT *pPage, POINT *pScreen)
{
   m_pParentPage->CoordPageToScreen (pPage, pScreen);
}

void CEscControl::CoordPageToScreen (RECT *pPage, RECT *pScreen)
{
   m_pParentPage->CoordPageToScreen (pPage, pScreen);
}

void CEscControl::CoordScreenToPage (POINT *pScreen, POINT *pPage)
{
   m_pParentPage->CoordScreenToPage (pScreen, pPage);
}

void CEscControl::CoordScreenToPage (RECT *pScreen, RECT *pPage)
{
   m_pParentPage->CoordScreenToPage (pScreen, pPage);
}

/***********************************************************************
IsVisible - Returns TRUE if the control is visible on the PC screen.
   Basically converts coordinates to window and sees if it's in the viisble
   client.

inptus
   none
retusn
   BOOL - TRUE if is visible
*/
BOOL CEscControl::IsVisible (void)
{
   // what if main window iconized?
   if (IsIconic (m_pParentPage->m_pWindow->m_hWnd))
      return FALSE;

   RECT  rScreen, rDest;
   CoordPageToScreen (&m_rPosn, &rScreen);

   return IntersectRect (&rDest, &rScreen, &m_pParentPage->m_pWindow->m_rClient);
}

/***********************************************************************
Enable - Enables/disables the controls.
   1) Sets the flag.
   2) CAlls ESCM_ENABLE
inputs
   BOOL  fEnable
returns
   BOOL - TRUE if OK
*/
BOOL CEscControl::Enable (BOOL fEnable)
{
   m_fEnabled = fEnable;
   Message (ESCM_ENABLE);

   return TRUE;
}

/***********************************************************************
TimerSet - Uses the controls single easy-to-user timer and turns it on (or
resets it if its already in use).

inputs
   DWORD    dwTime - millisecond
BOOL
   TRUE - if successful
*/
BOOL CEscControl::TimerSet (DWORD dwTime)
{
   if (m_dwTimerID)
      m_pParentPage->m_pWindow->TimerKill (m_dwTimerID);
   m_dwTimerID = m_pParentPage->m_pWindow->TimerSet (dwTime, this, ESCM_CONTROLTIMER);
   return m_dwTimerID ? TRUE : FALSE;
}

/***********************************************************************
TimerKill - Kills the single easy-to-use timer
*/
BOOL CEscControl::TimerKill (void)
{
   if (m_dwTimerID)
      m_pParentPage->m_pWindow->TimerKill (m_dwTimerID);
   m_dwTimerID = 0;
   return TRUE;
}

/***********************************************************************
Invalidate - Invalidates a region of the page. Use NULL to invalidate the
entire control

inputs
   RECT     *pPage - page coordinates.If NULL invalidates entire control
returns
   BOOL - TRUE if OK
*/
BOOL CEscControl::Invalidate (RECT *pPage)
{
   if (!pPage)
      pPage = &m_rPosn;

   // intersec twith control
   RECT  r;
   if (!IntersectRect (&r, pPage, &m_rPosn))
      return TRUE;   // not in control

   return m_pParentPage->Invalidate (&r);
}

/***********************************************************************
Message - Sends a message to the control. THis:
   1) Passes it to m_pCallback if there is one. If this returns FALSE then
   2) Default message handler
inputs
   DWORD dwMessage - message
   PVOID pParam -d epends on the message
returns
   TRUE if was handled
*/
BOOL CEscControl::Message (DWORD dwMessage, PVOID pParam)
{
   // send to callback
   if (m_pCallback && m_pCallback(this, dwMessage, pParam))
      return TRUE;

   switch (dwMessage) {
   case ESCM_QUERYSIZE:
      {
         ESCMQUERYSIZE  *p = (ESCMQUERYSIZE*) pParam;
         int iFinalWidth, iFinalHeight;
         BOOL  fPercent;
         if (AttribToDecimalOrPercent (m_pNode->AttribGet(L"width"), &fPercent, &iFinalWidth)) {
            if (fPercent)
               p->iWidth = iFinalWidth * p->iDisplayWidth / 100;
            else
               p->iWidth = iFinalWidth;
         };

         if (AttribToDecimalOrPercent (m_pNode->AttribGet(L"height"), &fPercent, &iFinalHeight)) {
            if (fPercent)
               p->iHeight = iFinalHeight * p->iDisplayWidth / 100;
            else
               p->iHeight = iFinalHeight;
         };
         
      }
      return TRUE;


   case ESCM_PAINT:
      {
         ESCMPAINT *p = (ESCMPAINT*) pParam;

         // didn't specify a paint routine so paint blue
         HBRUSH   hbr;
         hbr = CreateSolidBrush (RGB(0x80, 0x80, 0xff));
         FillRect (p->hDC, &p->rControlHDC, hbr);
         DeleteObject (hbr);
      }
      return TRUE;



#define  MOUSEOVERSIZE  2

   case ESCM_PAINTMOUSEOVER:
      {
         ESCMPAINT *p = (ESCMPAINT*) pParam;

         HPEN hPen, hPenOld;
         HBRUSH hBrush;
         hPen = CreatePen (PS_SOLID, MOUSEOVERSIZE, RGB(255, 0, 0));
         hPenOld = (HPEN) SelectObject (p->hDC, hPen);
         hBrush = (HBRUSH) SelectObject (p->hDC, GetStockObject(NULL_BRUSH));
         RoundRect (p->hDC,
            p->rControlHDC.left+1, p->rControlHDC.top+1 ,
            p->rControlHDC.right, p->rControlHDC.bottom,
            MOUSEOVERSIZE * 4, MOUSEOVERSIZE * 4);
         SelectObject (p->hDC, hPenOld);
         SelectObject (p->hDC, hBrush);
         DeleteObject (hPen);
      }
      return TRUE;



   case ESCM_PAINTFOCUS:
      {
         ESCMPAINT *p = (ESCMPAINT*) pParam;

         SetBkMode (p->hDC, OPAQUE);
         SetBkColor (p->hDC, RGB(0xff,0xff,0xff));

         HPEN hPen, hPenOld;
         HBRUSH hBrush;
         hPen = CreatePen (PS_DOT, 1, RGB(0, 0, 0));
         hPenOld = (HPEN) SelectObject (p->hDC, hPen);
         hBrush = (HBRUSH) SelectObject (p->hDC, GetStockObject(NULL_BRUSH));
         RoundRect (p->hDC,
            p->rControlHDC.left, p->rControlHDC.top ,
            p->rControlHDC.right, p->rControlHDC.bottom,
            MOUSEOVERSIZE * 4, MOUSEOVERSIZE * 4);
         SelectObject (p->hDC, hPenOld);
         SelectObject (p->hDC, hBrush);
         DeleteObject (hPen);
      }
      return TRUE;



   case ESCM_ATTRIBGET:
      {
         ESCMATTRIBGET *p = (ESCMATTRIBGET*) pParam;
         
         p->fFilledIn = AttribListGet (p->pszAttrib, p->pszValue, p->dwSize, &p->dwNeeded);
      }
      return TRUE;


   case ESCM_KEYHELP:
   case ESCM_MOUSEHOVER:
      {
         PESCMMOUSEHOVER p = (PESCMMOUSEHOVER) pParam;

         // if a button is down dont do
         if (m_fLButtonDown || m_fMButtonDown || m_fRButtonDown)
            return TRUE;

         // hover help
         DWORD dwIndex;
         dwIndex = m_pNode->ContentFind (L"HoverHelp");
         if (dwIndex == (DWORD)-1)
            return TRUE;   // no hover help

         PWSTR psz;
         PCMMLNode   pSub;
         m_pNode->ContentEnum(dwIndex, &psz, &pSub);
         POINT pt, pt2;
         if (p) {
            // mouse
            pt2 = p->pPosn;
            pt2.y += 16;   // so it's below the icon
         }
         else {
            // use bottom of control
            pt2.x = m_rPosn.left;
            pt2.y = m_rPosn.bottom;
         }
         CoordPageToScreen (&pt2, &pt);

         // else
         m_pParentPage->m_pWindow->HoverHelp (pSub, dwMessage == ESCM_KEYHELP, &pt);
      }
      return TRUE;

   case ESCM_MOUSEENTER:
      if (m_fWantMouse && m_fEnabled) {
         Invalidate ();
         m_pParentPage->SetCursor (IDC_HANDCURSOR/*(DWORD)IDC_CROSS*/);
      }
      return TRUE;

   case ESCM_FOCUS:
      Invalidate();
      return TRUE;

   case ESCM_ENABLE:
      Invalidate();
      return TRUE;

   case ESCM_MOUSELEAVE:
      if (m_fWantMouse && m_fEnabled)
         Invalidate ();
      return TRUE;

      
   case ESCM_CHAR:
   case ESCM_SYSCHAR:
   case ESCM_KEYDOWN:
   case ESCM_SYSKEYDOWN:
      {
         ESCMCHAR* p = (ESCMCHAR*) pParam;

         BOOL  fChar;
         switch (dwMessage) {
            case ESCM_CHAR:
            case ESCM_SYSCHAR:
               fChar = TRUE;
               break;
            default:
               fChar = FALSE; // dealing with keydown
         }

         // look through all the acceleartors for a match
         DWORD i;
         WCHAR c;
         if (fChar)
            c = (WCHAR) towupper (p->wCharCode);
         else
            c = p->wCharCode;
         for (i = 0; i < m_listAccelFocus.Num(); i++) {
            ESCACCELERATOR *pa = (ESCACCELERATOR*) m_listAccelFocus.Get(i);
            if (pa->c != c)
               continue;   // no match

            BOOL  fCanBeChar;
            fCanBeChar = (c == L' ') || (c == VK_ESCAPE) || (c > VK_HELP) || (c == 3) || ((c >= 24) && (c <= 26)) || (c == 22) || (c == 1);
            if (!fChar && (c >= VK_F1) && (c <= VK_F12))
               fCanBeChar = FALSE;

            // if it's a char message and get a value < space ignore
            if (fChar && !fCanBeChar)
               continue;
            // if it's a keydown message and get a value >= space ignore
            if (!fChar && fCanBeChar)
               continue;

            if ((pa->fAlt && !(p->lKeyData & (1<<29))) ||
               (!pa->fAlt && !(!(p->lKeyData & (1<<29)))) )
               continue;
            if ((pa->fShift && (GetKeyState (VK_SHIFT) >= 0)) ||
               (!pa->fShift && (GetKeyState (VK_SHIFT) < 0)) )
               continue;
            if ((pa->fControl && (GetKeyState (VK_CONTROL) >= 0)) ||
               (!pa->fControl && (GetKeyState (VK_CONTROL) < 0)) )
               continue;

            // else match
            p->fEaten = TRUE;
            Message (pa->dwMessage);
            return TRUE;
         }
      }

      return TRUE;

   case ESCM_SWITCHACCEL:
      {
         // set focus to this
         m_pParentPage->FocusSet (this);

         WCHAR szAccel[512];
         DWORD dwNeeded;
         if (AttribListGet (L"href", szAccel, sizeof(szAccel), &dwNeeded))
            m_pParentPage->Link (szAccel);
      }
      return TRUE;

   case ESCM_MOVE:
      if (m_fRedrawOnMove)
         Invalidate ();
      return TRUE;

   case ESCM_SIZE:
      Invalidate();
      return TRUE;

   case ESCM_ATTRIBSET:
      {
         ESCMATTRIBSET *p  =(ESCMATTRIBSET*) pParam;
         return AttribListSet (p->pszAttrib, p->pszValue);
      }
      return TRUE;

   case ESCM_ATTRIBENUM:
      {
         ESCMATTRIBENUM *p = (ESCMATTRIBENUM*) pParam;

         PWSTR psz;
         psz = m_treeAttrib.Enum(p->dwIndex);
         if (!psz) {
            // cant find
            p->dwNeeded = 0;
            p->fFilledIn = 0;
            return TRUE;
         }

         // else figure out how much is needed
         p->dwNeeded = (wcslen(psz)+1)*2;
         p->fFilledIn = p->dwNeeded <= p->dwSize;
         if (p->fFilledIn)
            wcscpy (p->pszAttrib, psz);
         return TRUE;
      }
      return TRUE;
   }


   // No default handler for
   // ESCM_CONSTRUCTOR
   // ESCM_DESCTRUCTOR
   // ESCM_TIMER
   // ESCM_MOUSEMOVE
   // ESCM_XBUTTONDOWN
   // ESCM_XBUTTONUP
   // ESCM_INITCONTROL
   // ESCM_CONTROLTIMER
   // ESCM_(SYS)KEYDOWN/UP
   // messages from child controls

   return FALSE;
}

/***********************************************************************
MessageToParent - Sends a message to the control's parent (either
   another contorl or a page.

inputs
   DWORD    dwMesage
   PVOID    pParam - Parameter
returns
   BOOL - TRUE if it was handled
*/
BOOL CEscControl::MessageToParent (DWORD dwMessage, PVOID pParam)
{
   if (m_pParentControl)
      return m_pParentControl->Message (dwMessage, pParam);
   else
      return m_pParentPage->Message (dwMessage, pParam);
}

/***********************************************************************
AttribGet - Gets an attribute from the control. (Basically calls ESCM_ATTRIBGET).

inputs
   PWSTR    pszAttrib - attribute
   PWSTR    pszValue - value - to be filled in
   DWORD    dwSize - bytes for value
   DWORD    dwNeeded - filled in with number of bytes needd for string
returns
   BOOL - TRUE if got, FALSE if not
*/
BOOL CEscControl::AttribGet (PWSTR pszAttrib, PWSTR pszValue, DWORD dwSize, DWORD *pdwNeeded)
{
   ESCMATTRIBGET  a;

   a.dwNeeded = 0;
   a.dwSize = dwSize;
   a.fFilledIn = FALSE;
   a.pszAttrib = pszAttrib;
   a.pszValue = pszValue;

   Message (ESCM_ATTRIBGET, &a);

   *pdwNeeded = a.dwNeeded;
   return a.fFilledIn;
}

/***********************************************************************
AttribGetBOOL - Gets a boolean attribute
inputs
   PWSTR    pszAttrib - attribute
returns
   BOOL - TRUE if got, FALSE if not. FALSE if fail
*/
BOOL CEscControl::AttribGetBOOL (PWSTR pszAttrib)
{
   WCHAR sz[16];
   BOOL  fRet;
   DWORD dwNeeded;
   fRet = AttribGet (pszAttrib, sz, sizeof(sz), &dwNeeded);
   if (!fRet) return FALSE;
   if (AttribToYesNo (sz, &fRet))
      return fRet;
   return FALSE;
}


/***********************************************************************
AttribGetInt - Gets a boolean attribute
inputs
   PWSTR    pszAttrib - attribute
returns
   int - value. 0 if cant parse attribute
*/
int CEscControl::AttribGetInt (PWSTR pszAttrib)
{
   WCHAR sz[16];
   BOOL  fRet;
   DWORD dwNeeded;
   fRet = AttribGet (pszAttrib, sz, sizeof(sz), &dwNeeded);
   if (!fRet) return 0;
   int iVal;
   if (AttribToDecimal (sz, &iVal))
      return iVal;
   return 0;
}


/***********************************************************************
AttribSet - Sets the attribute for a control. (Basically calls ESCM_ATTRIBSET)

inputs
   PWSTR    pszAttrib - attribute
   PWSTR    pszValue - string value
returns
   BOOL - TRUE if successful
*/
BOOL CEscControl::AttribSet (PWSTR pszAttrib, PWSTR pszValue)
{
   ESCMATTRIBSET a;

   a.pszAttrib = pszAttrib;
   a.pszValue = pszValue;

   return Message (ESCM_ATTRIBSET, &a);
}

/***********************************************************************
AttribSetBOOL - Sets the attribute for a control to a BOOL.

inputs
   PWSTR    pszAttrib - attribute
   BOOL     fVal - Value
returns
   BOOL - TRUE if successful
*/
BOOL CEscControl::AttribSetBOOL (PWSTR pszAttrib, BOOL fVal)
{
   return AttribSet (pszAttrib, fVal ? L"yes" : L"no");
}

/***********************************************************************
AttribSetInt - Sets the attribute for a control to a int.

inputs
   PWSTR    pszAttrib - attribute
   int     iVal - Value
returns
   BOOL - TRUE if successful
*/
BOOL CEscControl::AttribSetInt (PWSTR pszAttrib, int iVal)
{
   WCHAR sz[16];
   swprintf (sz, L"%d", iVal);
   return AttribSet (pszAttrib, sz);
}

/***********************************************************************
AttribEnum - Enumerates an attribute of a control. Basicall calls
ESCM_ATTRIBENUM.

inputs
   DWORD    dwNum - Index number starting at 0. If it's too high will fail
   PWSTR    pszAttrib - Attribute name to be filled in
   DWORD    dwSize - Number of bytes of pszAttrib
returns
   BOOL - TRUE if OK
*/
BOOL CEscControl::AttribEnum (DWORD dwNum, PWSTR pszAttrib, DWORD dwSize, DWORD *pdwNeeded)
{
   ESCMATTRIBENUM a;
   a.dwIndex = dwNum;
   a.dwNeeded = 0;
   a.dwSize = dwSize;
   a.fFilledIn = FALSE;
   a.pszAttrib = pszAttrib;

   Message (ESCM_ATTRIBENUM, &a);
   *pdwNeeded = a.dwNeeded;

   return a.fFilledIn;
}

/***********************************************************************
Paint - Causes the control to paint to a HDC.

inputs
   RECT     *pPageCoord - Rectangle (in page coordinates) that needs drawing.
   RECT     *pHDCCoord - Rectangle (in HDC coordinates) where to draw to. Has
               the same dimensions as pPAgeCoord.
   RECT     *pScreenCoord - Where this will appear on the screen. Some controls
               will use this for perspective. Same dimensions as pPageCoord.
   RECT     *pTotalScreen - Rectangle covering the total screen
   HDC      hDC - DC
returns
   BOOL - TRUE if OK
*/
BOOL CEscControl::Paint (RECT *prPage, RECT *prDC, RECT *prScreen, RECT *pTotalScreen, HDC hDC)
{
   ESCMPAINT   p;
   p.hDC = hDC;
   p.rInvalidHDC = *prDC;
   p.rInvalidPage = *prPage;
   p.rInvalidScreen = *prScreen;
   p.rTotalScreen = *pTotalScreen;

   p.rControlPage = m_rPosn;
   p.rControlHDC = m_rPosn;
   p.rControlScreen = m_rPosn;
   OffsetRect (&p.rControlHDC, prDC->left - prPage->left, prDC->top - prPage->top);
   OffsetRect (&p.rControlScreen, prScreen->left - prPage->left, prScreen->top - prPage->top);

   // paint the main thing
   Message (ESCM_PAINT, &p);

   // tab
   if (m_fEnabled & m_fFocus)
      Message (ESCM_PAINTFOCUS, &p);

   // mouse over rect
   if (m_fEnabled & m_fMouseOver & m_fWantMouse)
      Message (ESCM_PAINTMOUSEOVER, &p);

   return TRUE;
}

/******************************************************************************
EscControlAdd - Adds a control to the list of ones being interpreted

inputs
   PWSTR    psz - Control name. Used in the parsing/interpretation part for
                  the tag name. If one of same name already exists this overwrites
   PESCCONTROLCALLBACK pCallback - Callback. If NULL then deletes the control.
returns  
   BOOL - TRUE if OK.
*/
BOOL EscControlAdd (PWSTR psz, PESCCONTROLCALLBACK pCallback)
{
   if (!pCallback)
      return gtreeControlCallback.Remove (psz);

   // else set
   return gtreeControlCallback.Add (psz, &pCallback, sizeof(pCallback));
}


/******************************************************************************
EscControlGet - Returns the pointer to the control callback.

inputs
   PWSTR    psz - string
returns
   PESCCONTROLCALLBACK - Pointer to callback. NULL if cant find
*/
PESCCONTROLCALLBACK EscControlGet (PWSTR psz)
{
   PESCCONTROLCALLBACK *pp;
   DWORD dwSize;

   pp = (PESCCONTROLCALLBACK*) gtreeControlCallback.Find (psz, &dwSize);
   if (!pp)
      return NULL;
   return *pp;
}


/***************************************************************************
CEscControl::AttribListAddXXX - Call these during the ESCM_CONSTRUCTOR to
tell the default handler what types the attributes are. That way, byt he time
ESCM_INITCONTROL, all the attributes (numberic values) called will have been
filled in if they were specified in the object's attributes session. Furthermore,
wheenver ESCM_ATTRIBGET or ESCM_ATTRIBSET are called they'll appear there.

inputs
   WCHAR       *psz - attribute name
   Pointer     - pointer to the integer (or whatever) value that is filled in
               when ESCM_ATTRIBSET is called, or used if ESCM_ATTRIBGET is called
   BOOL        *pfDirty - Optional. Set to TRUE if the attribute is changed, so the
               user code can quickly know if something was changed. For example have
               a BOOL m_fNeedRepaint flag set to TRUE.
   BOOL        fRepaint - If TRUE, invalidates the control rectangle when the attribute
               is changd.
   DWORD       dwMessage - message to send when attirbute changes
returns
   BOOL - TRUE if succss
*/
BOOL CEscControl::AttribListAddHex (WCHAR *psz, DWORD *pdwAttrib, BOOL *pfDirty, BOOL fRepaint, DWORD dwMessage)
{
   ATTRIBCONVERT  ac;
   memset (&ac, 0, sizeof(ac));
   ac.pfDirty = pfDirty;
   ac.fRepaint = fRepaint;
   ac.dwMessage = dwMessage;

   ac.dwType = ATTRIB_HEX;
   ac.p = (PVOID) pdwAttrib;

   // add this attribute to the tree with a dummy value so enumeration works
   m_treeAttrib.Add (psz, L"", 2);

   return m_treeAttribList.Add(psz, &ac, sizeof(ac));
}

BOOL CEscControl::AttribListAddDecimal (WCHAR *psz, int *piAttrib, BOOL *pfDirty, BOOL fRepaint, DWORD dwMessage)
{
   ATTRIBCONVERT  ac;
   memset (&ac, 0, sizeof(ac));
   ac.pfDirty = pfDirty;
   ac.fRepaint = fRepaint;
   ac.dwMessage = dwMessage;


   ac.dwType = ATTRIB_DECIMAL;
   ac.p = (PVOID) piAttrib;

   // add this attribute to the tree with a dummy value so enumeration works
   m_treeAttrib.Add (psz, L"", 2);

   return m_treeAttribList.Add(psz, &ac, sizeof(ac));
}

BOOL CEscControl::AttribListAddDouble (WCHAR *psz, double *piAttrib, BOOL *pfDirty, BOOL fRepaint, DWORD dwMessage)
{
   ATTRIBCONVERT  ac;
   memset (&ac, 0, sizeof(ac));
   ac.pfDirty = pfDirty;
   ac.fRepaint = fRepaint;
   ac.dwMessage = dwMessage;


   ac.dwType = ATTRIB_DOUBLE;
   ac.p = (PVOID) piAttrib;

   // add this attribute to the tree with a dummy value so enumeration works
   m_treeAttrib.Add (psz, L"", 2);

   return m_treeAttribList.Add(psz, &ac, sizeof(ac));
}

BOOL CEscControl::AttribListAdd3DPoint (WCHAR *psz, double *piAttrib, BOOL *pfDirty, BOOL fRepaint, DWORD dwMessage)
{
   ATTRIBCONVERT  ac;
   memset (&ac, 0, sizeof(ac));
   ac.pfDirty = pfDirty;
   ac.fRepaint = fRepaint;
   ac.dwMessage = dwMessage;


   ac.dwType = ATTRIB_3DPOINT;
   ac.p = (PVOID) piAttrib;

   // add this attribute to the tree with a dummy value so enumeration works
   m_treeAttrib.Add (psz, L"", 2);

   return m_treeAttribList.Add(psz, &ac, sizeof(ac));
}

BOOL CEscControl::AttribListAddColor (WCHAR *psz, COLORREF *pcr, BOOL *pfDirty, BOOL fRepaint, DWORD dwMessage)
{
   ATTRIBCONVERT  ac;
   memset (&ac, 0, sizeof(ac));
   ac.pfDirty = pfDirty;
   ac.fRepaint = fRepaint;
   ac.dwMessage = dwMessage;


   ac.dwType = ATTRIB_COLOR;
   ac.p = (PVOID) pcr;

   // add this attribute to the tree with a dummy value so enumeration works
   m_treeAttrib.Add (psz, L"", 2);

   return m_treeAttribList.Add(psz, &ac, sizeof(ac));
}

BOOL CEscControl::AttribListAddDecimalOrPercent (WCHAR *psz, int *piAttrib, BOOL *pfPercent, BOOL *pfDirty, BOOL fRepaint, DWORD dwMessage)
{
   ATTRIBCONVERT  ac;
   memset (&ac, 0, sizeof(ac));
   ac.pfDirty = pfDirty;
   ac.fRepaint = fRepaint;
   ac.dwMessage = dwMessage;


   ac.dwType = ATTRIB_DECIMALORPERCENT;
   ac.p = (PVOID) piAttrib;
   ac.p2 = (PVOID) pfPercent;

   // add this attribute to the tree with a dummy value so enumeration works
   m_treeAttrib.Add (psz, L"", 2);

   return m_treeAttribList.Add(psz, &ac, sizeof(ac));
}

BOOL CEscControl::AttribListAddBOOL (WCHAR *psz, BOOL *pfAttrib, BOOL *pfDirty, BOOL fRepaint, DWORD dwMessage)
{
   ATTRIBCONVERT  ac;
   memset (&ac, 0, sizeof(ac));
   ac.pfDirty = pfDirty;
   ac.fRepaint = fRepaint;
   ac.dwMessage = dwMessage;


   ac.dwType = ATTRIB_BOOL;
   ac.p = (PVOID) pfAttrib;

   // add this attribute to the tree with a dummy value so enumeration works
   m_treeAttrib.Add (psz, L"", 2);

   return m_treeAttribList.Add(psz, &ac, sizeof(ac));
}

BOOL CEscControl::AttribListAddPercent (WCHAR *psz, int *piAttrib, BOOL *pfDirty, BOOL fRepaint, DWORD dwMessage)
{
   ATTRIBCONVERT  ac;
   memset (&ac, 0, sizeof(ac));
   ac.pfDirty = pfDirty;
   ac.fRepaint = fRepaint;
   ac.dwMessage = dwMessage;


   ac.dwType = ATTRIB_PERCENT;
   ac.p = (PVOID) piAttrib;

   // add this attribute to the tree with a dummy value so enumeration works
   m_treeAttrib.Add (psz, L"", 2);

   return m_treeAttribList.Add(psz, &ac, sizeof(ac));
}

BOOL CEscControl::AttribListAddAccelerator (WCHAR *psz, PESCACCELERATOR pAttrib, BOOL *pfDirty, BOOL fRepaint, DWORD dwMessage)
{
   ATTRIBCONVERT  ac;
   memset (&ac, 0, sizeof(ac));
   ac.pfDirty = pfDirty;
   ac.fRepaint = fRepaint;
   ac.dwMessage = dwMessage;


   ac.dwType = ATTRIB_ACCELERATOR;
   ac.p = (PVOID) pAttrib;

   // add this attribute to the tree with a dummy value so enumeration works
   m_treeAttrib.Add (psz, L"", 2);

   return m_treeAttribList.Add(psz, &ac, sizeof(ac));
}

BOOL CEscControl::AttribListAddString (WCHAR *psz, PWSTR *ppszAttrib, BOOL *pfDirty, BOOL fRepaint, DWORD dwMessage)
{
   ATTRIBCONVERT  ac;
   memset (&ac, 0, sizeof(ac));
   ac.pfDirty = pfDirty;
   ac.fRepaint = fRepaint;
   ac.dwMessage = dwMessage;


   ac.dwType = ATTRIB_STRING1;
   ac.p = (PVOID) ppszAttrib;

   // add this attribute to the tree with a dummy value so enumeration works
   m_treeAttrib.Add (psz, L"", 2);

   return m_treeAttribList.Add(psz, &ac, sizeof(ac));
}

BOOL CEscControl::AttribListAddString (WCHAR *psz, PWSTR pszAttrib, DWORD dwSize, BOOL *pfDirty, BOOL fRepaint, DWORD dwMessage)
{
   ATTRIBCONVERT  ac;
   memset (&ac, 0, sizeof(ac));
   ac.pfDirty = pfDirty;
   ac.fRepaint = fRepaint;
   ac.dwMessage = dwMessage;


   ac.dwType = ATTRIB_STRING2;
   ac.p = (PVOID) pszAttrib;
   ac.p2 = (PVOID) dwSize;

   // add this attribute to the tree with a dummy value so enumeration works
   m_treeAttrib.Add (psz, L"", 2);

   return m_treeAttribList.Add(psz, &ac, sizeof(ac));
}

BOOL CEscControl::AttribListAddCMem (WCHAR *psz, PCMem pMem, BOOL *pfDirty, BOOL fRepaint, DWORD dwMessage)
{
   ATTRIBCONVERT  ac;
   memset (&ac, 0, sizeof(ac));
   ac.pfDirty = pfDirty;
   ac.fRepaint = fRepaint;
   ac.dwMessage = dwMessage;


   ac.dwType = ATTRIB_CMEM;
   ac.p = (PVOID) pMem;

   // add this attribute to the tree with a dummy value so enumeration works
   m_treeAttrib.Add (psz, L"", 2);

   return m_treeAttribList.Add(psz, &ac, sizeof(ac));
}


/*************************************************************************
CEscControl::AttribListSet- Sets an attribute, using m_treeAttribList to set it.
   It also updates m_treeAttrib.

inputs
   WCHAR    *psz - string
   WCHAR    *pszValue - value to write
returns
   BOOL - true if OK
*/
BOOL CEscControl::AttribListSet (WCHAR *psz, WCHAR *pszValue)
{
   // see if it's in the attriblist
   PATTRIBCONVERT pc;
   pc = (PATTRIBCONVERT) m_treeAttribList.Find (psz);
   if (!pc) {
      // write it directly to the attribute list
      return m_treeAttrib.Add (psz, pszValue, (wcslen(pszValue)+1)*2);
   }

   // else switch
   switch (pc->dwType) {
   case ATTRIB_HEX:
      {
         DWORD dw;
         if (AttribToHex (pszValue, &dw)) {
            // if no change then just return
            if (*((DWORD*) pc->p) == dw)
               return TRUE;

            *((DWORD*) pc->p) = dw;
            goto returntrue;
         }
      }
      return FALSE;

   case ATTRIB_DECIMAL:
      {
         int dw;
         if (AttribToDecimal (pszValue, &dw)) {
            // if no change then just return
            if (*((int*) pc->p) == dw)
               return TRUE;

            *((int*) pc->p) = dw;
            goto returntrue;
         }
      }
      return FALSE;

   case ATTRIB_DOUBLE:
      {
         double dw;
         if (AttribToDouble (pszValue, &dw)) {
            // if no change then just return
            if (*((double*) pc->p) == dw)
               return TRUE;

            *((double*) pc->p) = dw;
            goto returntrue;
         }
      }
      return FALSE;

   case ATTRIB_3DPOINT:
      {
         double dw[4];
         if (AttribTo3DPoint (pszValue, dw)) {
            // if no change then just return
            if ( (((double*) pc->p)[0] == dw[0]) && (((double*) pc->p)[1] == dw[1]) &&
               (((double*) pc->p)[2] == dw[2]) )
               return TRUE;

            ((double*) pc->p)[0] = dw[0];
            ((double*) pc->p)[1] = dw[1];
            ((double*) pc->p)[2] = dw[2];
            goto returntrue;
         }
      }
      return FALSE;

   case ATTRIB_COLOR:
      {
         COLORREF dw;
         if (AttribToColor (pszValue, &dw)) {
            // if no change then just return
            if (*((COLORREF*) pc->p) == dw)
               return TRUE;

            *((COLORREF*) pc->p) = dw;
            goto returntrue;
         }
      }
      return FALSE;

   case ATTRIB_DECIMALORPERCENT:
      {
         int dw;
         BOOL  fPercent;
         if (AttribToDecimalOrPercent (pszValue, &fPercent, &dw)) {
            // if no change then just return
            if ( (*((int*) pc->p) == dw) && (*((BOOL*) pc->p2) == fPercent))
               return TRUE;

            *((int*) pc->p) = dw;
            *((BOOL*) pc->p2) = fPercent;
            goto returntrue;
         }
      }
      return FALSE;

   case ATTRIB_BOOL:
      {
         BOOL dw;
         if (AttribToYesNo (pszValue, &dw)) {
            // if no change then just return
            if (*((BOOL*) pc->p) == dw)
               return TRUE;

            *((BOOL*) pc->p) = dw;
            goto returntrue;
         }
      }
      return FALSE;

   case ATTRIB_PERCENT:
      {
         int dw;
         if (AttribToPercent (pszValue, &dw)) {
            // if no change then just return
            if (*((int*) pc->p) == dw)
               return TRUE;

            *((int*) pc->p) = dw;
            goto returntrue;
         }
      }
      return FALSE;

   case ATTRIB_ACCELERATOR:
      {
         ESCACCELERATOR dw;
         if (AttribToAccelerator (pszValue, &dw)) {
            ((ESCACCELERATOR*) pc->p)->c = dw.c;
            ((ESCACCELERATOR*) pc->p)->fAlt = dw.fAlt;
            ((ESCACCELERATOR*) pc->p)->fControl = dw.fControl;
            ((ESCACCELERATOR*) pc->p)->fShift = dw.fShift;
            goto returntrue;
         }
      }
      return FALSE;

   case ATTRIB_STRING1:
      if (pszValue) {
         m_treeAttrib.Add (psz, pszValue, (wcslen(pszValue)+1)*2);
         *((PWSTR*) pc->p) = (PWSTR) m_treeAttrib.Find (psz);;
      }
      else {
         *((PWSTR*) pc->p) = NULL;
      }
      goto returntrue;

   case ATTRIB_STRING2:
      if (pszValue) {
         int   iLen;
         iLen = (wcslen(pszValue) + 1) *2;
         if (iLen <= (int) (DWORD)pc->p2)
            memcpy (pc->p, pszValue, iLen);
         else {
            iLen = min((int) (DWORD)pc->p2 - 2, iLen);
            memcpy (pc->p, pszValue, iLen);
            ((WCHAR*) pc->p)[iLen / 2] = 0;
         }
      }
      else {   // pszvalue
         ((WCHAR*) pc->p)[0] = 0;   // null string
      }
      goto returntrue;

   case ATTRIB_CMEM:
      if (pszValue) {
         int   iLen;
         iLen = (wcslen(pszValue) + 1) *2;
         PCMem pMem;
         pMem = (PCMem) pc->p;
         if (pMem->Required(iLen))
            memcpy (pMem->p, pszValue, iLen);
      }
      else {   // pszvalue
         PCMem pMem;
         pMem = (PCMem) pc->p;
         if (pMem->Required(2))
            wcscpy ((PWSTR) pMem->p, L"");   // null string
      }
      goto returntrue;

   }

   return TRUE;

returntrue:
   if (pc->pfDirty)
      *(pc->pfDirty) = TRUE;
   if (pc->fRepaint)
      Invalidate();
   if (pc->dwMessage)
      Message (pc->dwMessage);

   return TRUE;
}


/*************************************************************************
CEscControl::AttribListGet- Gets an attribute, using m_treeAttribList to get it.

inputs
   WCHAR    *psz - string
   WCHAR    *pszValue - where to write to.
   DWORD    dwSize - size of pszValue in bytes
   DWORD    *pdwNeeded - Filled in with the amount needed
returns
   BOOL - true if OK, FALSE if didn't fill in
*/
BOOL CEscControl::AttribListGet (WCHAR *psz, WCHAR *pszValue, DWORD dwSize, DWORD *pdwNeeded)
{
   // defaults
   WCHAR    szTemp[128];   // temporary
   PWSTR    pszFrom = szTemp;    // string to copy from

   // see if it's in the attriblist
   PATTRIBCONVERT pc;
   pc = (PATTRIBCONVERT) m_treeAttribList.Find (psz);
   if (pc) {
      // else switch
      switch (pc->dwType) {
      case ATTRIB_HEX:
         swprintf (szTemp, L"%x", *((DWORD*)pc->p));
         break;

      case ATTRIB_DECIMAL:
         swprintf (szTemp, L"%d", *((int*)pc->p));
         break;

      case ATTRIB_DOUBLE:
         swprintf (szTemp, L"%g", *((double*)pc->p));
         break;

      case ATTRIB_3DPOINT:
         swprintf (szTemp, L"%g,%g,%g", ((double*)pc->p)[0],((double*)pc->p)[1],((double*)pc->p)[2]);
         break;

      case ATTRIB_COLOR:
         // swprintf (szTemp, L"#%x", *((COLORREF*)pc->p));
         ColorToAttrib (szTemp, *((COLORREF*)pc->p));
         break;

      case ATTRIB_DECIMALORPERCENT:
         if (*((BOOL*)pc->p2))
            swprintf (szTemp, L"%d%%", *((int*)pc->p));
         else
            swprintf (szTemp, L"%d", *((int*)pc->p));
         break;

      case ATTRIB_BOOL:
         if (*((BOOL*)pc->p))
            pszFrom = L"yes";
         else
            pszFrom = L"no";
         break;

      case ATTRIB_PERCENT:
         swprintf (szTemp, L"%d%%", *((int*)pc->p));
         break;

      case ATTRIB_ACCELERATOR:
         {
            ESCACCELERATOR *p;
            p = ((ESCACCELERATOR*) pc->p);
            szTemp[0] = 0;
            if (p->fControl)
               wcscat (szTemp, L"ctl-");
            if (p->fShift)
               wcscat (szTemp, L"shift-");
            if (p->fAlt)
               wcscat (szTemp, L"alt-");
            int   iLen;
            iLen = wcslen(szTemp);
            szTemp[iLen++] = p->c;
            szTemp[iLen] = 0;
         }
         break;

      case ATTRIB_STRING1:
         pszFrom = *((PWSTR*) pc->p);
         break;

      case ATTRIB_STRING2:
         pszFrom = (PWSTR) pc->p;
         break;

      case ATTRIB_CMEM:
         pszFrom = (PWSTR) ((PCMem)pc->p)->p;
         break;

      }
   }
   else {
      // write it directly to the attribute list
      pszFrom = (WCHAR*) m_treeAttrib.Find (psz);
   }

   if (!pszFrom) {
      *pdwNeeded = 0;
      return FALSE;
   }

   // see if it fits
   int   iLen;
   iLen = (wcslen(pszFrom)+1)*2;
   *pdwNeeded = (DWORD) iLen;
   if (iLen > (int) dwSize) {
      return FALSE;
   }

   // copy
   memcpy (pszValue, pszFrom, iLen);

   return TRUE;
}


/*************************************************************************
CEscControl::TextBlock - Creates a new text block. This:
   1) Creates the CEscTextBlock object
   2) Initializes it
   3) Calls Interpret

It does not:
   4) Call PostInterpret - This must be done after the control decides how
   it wants stuff centered.

inputs
   HDC         hDC - HDC used to calculate
   CMMLNode    *pNode - node to create.
   int         iWidth - maximum width
   BOOL        fDeleteNode - If TRUE, delete the node when text block destroyed.
                  If FALSE, don't. NOTE: Set to FALSE when using m_pNode or children.
   BOOL        fRootNodeNULL - If TRUE, don't use the name of the root node, but treat
                  is as text. If FALSE, look at name. For eaxmple, if a
                  <button>Press me!</button> control, then would pass m_pNode for
                  pNode, and FALSE for fRootNodeNULL because dont want to create
                  a button within a button.
returns
   CEscTextBlock* - New block, or NULL if cant create
*/
CEscTextBlock *CEscControl::TextBlock (HDC hDC, CMMLNode *pNode, int iWidth, BOOL fDeleteNode, BOOL fRootNodeNULL)
{
   CEscTextBlock*p;
   p = new CEscTextBlock();
   if (!p)
      return NULL;

   if (!p->Init ()) {
      delete p;
      return NULL;
   }

   // flags to delete
   p->m_fDeleteCMMLNode = fDeleteNode;

   // use control's font cache
   p->OtherFontCache (&m_FontCache);

   // use old font except no indent
   p->m_fi = m_fi;

   if (!p->Interpret (m_pParentPage, this, pNode, hDC, m_hInstance, iWidth, TRUE, fRootNodeNULL)) {
      delete p;
      return NULL;
   }

   return p;
}




