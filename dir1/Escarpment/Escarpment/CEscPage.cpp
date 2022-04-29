/***********************************************************************
CEscPage - Code for CEscPage

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
#include "resleak.h"

/***********************************************************************
Constructor & destructor
*/
CEscPage::CEscPage (void)
{
   m_pControlFocus = NULL;
   m_pCallback = NULL;
   m_pWindow = NULL;
   memset (&m_rTotal, 0, sizeof(m_rTotal));
   m_rVisible = m_rTotal;
   m_listESCACCELERATOR.Init (sizeof(ESCACCELERATOR));
   m_pNode = NULL;
   m_pControlMouse = m_pControlCapture = NULL;
   m_iLastWidth = -1;
   m_iLastHeight = -1;
   m_pUserData = 0;
   memset (&m_fi, 0, sizeof(m_fi));

   // set up default margins based upon scroll bar
   m_rMargin.left = m_rMargin.right = GetSystemMetrics (SM_CXVSCROLL) / 2;
   m_rMargin.top = m_rMargin.bottom = GetSystemMetrics (SM_CYHSCROLL) / 2;
}

CEscPage::~CEscPage (void)
{
   Message (ESCM_DESTRUCTOR);

   // release the mouse just in case have it
   if (m_pWindow)
      m_pWindow->MouseCaptureRelease(); 

   if (m_pWindow && (m_pWindow->m_pPage == this))
      m_pWindow->m_pPage = NULL;
   if (m_pNode)
      delete m_pNode;
}


/***********************************************************************
MassageControls - Called during Init or ReInterpret. This:
   1) Does postinterpreting
   2) Tells all the controls they've moved.
   3) Some other stuff

inputs
   none
returns
   BOOL - TRUE if successful
*/
BOOL CEscPage::MassageControls (void)
{
   // post interpret
   m_rTotal.left = m_rTotal.top = 0;
   m_rTotal.right = m_rVisible.right;

   // if the window style uses stretching then try to stretch
   if (m_TextBlock.m_fFoundStretch) {
      m_TextBlock.Stretch (m_pWindow->m_rClient.bottom - m_pWindow->m_rClient.top - m_rMargin.top - m_rMargin.bottom);
   }

   // if the window uses automatically adjusted height then limit our text bottom to
   // m_iCalcHeight + margin.
   if (m_pWindow->m_dwStyle & EWS_AUTOHEIGHT) {
      m_rTotal.bottom = m_TextBlock.m_iCalcHeight + m_rMargin.top + m_rMargin.bottom;
      m_rVisible.top = 0;
      m_rVisible.bottom = m_rTotal.bottom;
   }
   else  // else, limit to max of what's visible
      m_rTotal.bottom = max(m_TextBlock.m_iCalcHeight + m_rMargin.top + m_rMargin.bottom, m_rVisible.bottom - m_rVisible.top);

   // if autoswidth then determine new width
   if (m_pWindow->m_dwStyle & EWS_AUTOWIDTH) {
      m_rTotal.right = m_TextBlock.m_iCalcWidth + m_rMargin.left + m_rMargin.right;
   }

   m_TextBlock.PostInterpret (m_rMargin.left, m_rMargin.top, &m_rTotal);

   // need to readjust window size if it's size is to be
   // limited by the data height
   if (m_pWindow->m_hWnd && (m_pWindow->m_dwStyle & (EWS_AUTOHEIGHT | EWS_AUTOWIDTH))) {
      RECT  rWindow, rClient, rOld;
      GetWindowRect (m_pWindow->m_hWnd, &rWindow);
      rOld = rWindow;
      GetClientRect (m_pWindow->m_hWnd, &rClient);
      if ((rClient.bottom != m_rTotal.bottom) || (rClient.right != m_rTotal.right)) {
         if (m_pWindow->m_dwStyle & EWS_AUTOHEIGHT)
            rWindow.bottom += (m_rTotal.bottom - rClient.bottom);
         if (m_pWindow->m_dwStyle & EWS_AUTOWIDTH)
            rWindow.right += (m_rTotal.right - rClient.right);

         // only move if change
         if ((rOld.bottom != rWindow.bottom) || (rOld.right != rWindow.right))
            MoveWindow (m_pWindow->m_hWnd, rWindow.left, rWindow.top,
               rWindow.right - rWindow.left, rWindow.bottom - rWindow.top, TRUE);
      }
   }

   // set up the list of control names -> PCEscControl
   DWORD i;
   TWOBJECTPOSN *pop;
   m_treeControls.Clear();
   // go through all the controls and tell them they've moved
   for (i = 0; i < m_TextBlock.m_plistTWOBJECTPOSN->Num(); i++) {
      pop = (TWOBJECTPOSN*) m_TextBlock.m_plistTWOBJECTPOSN->Get(i);
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
      if (pControl->m_pszName)
         m_treeControls.Add (pControl->m_pszName, &pControl, sizeof(pControl));
   }

   // BUGFIX - moved code that tells all controls they moved into PostInterpret

   // invalidate everything
   Invalidate ();

   // note the potential scroll so the parent window knows
   // call vscroll since also checks boundary conditions
   VScroll (m_rVisible.top);
   // m_pWindow->ScrollBar (m_rVisible.top, m_rVisible.bottom - m_rVisible.top, m_rTotal.bottom);

   return TRUE;
}

/***********************************************************************
Init(File) - Initalizes the page. This causes the text to be parsed and then
interpreted. Also calls ESCM_INITPAGE.

inputs
   PWSTR    pszPageText - Unciode string page text
      or
   PSTR     pszPageText - ANSI string page text
      or
   DWORD    dwResource - "TEXT" resource containing ANSI or UNICODE.
      or
   PCMMLNoed pNode - Already parsed PCMMLNode. Once passed, the caller loses
               control of this.
      or
   PWSTR    pszFile - file
   PESCPAGECALLBACK pCallback - callback to use
   CEscWindow * - Window to create in
   PVOID    pUserData - user data
   IFONTINFO   *pfi - Font to use. If NULL then dont set
returns
   BOOL - TRUE if succesful. If error, may be info in m_TextBlock.CError
*/
BOOL CEscPage::Init (PWSTR pszPageText, PESCPAGECALLBACK pCallback, CEscWindow *pWindow, PVOID pUserData, IFONTINFO *pfi)
{
   // fail if already intialized
   if (m_pWindow)
      return FALSE;

   // remember some stuff
   m_pCallback = pCallback;
   m_pWindow = pWindow;
   m_pUserData = pUserData;
   m_fi = *pfi;

   Message (ESCM_CONSTRUCTOR);

   // initalize the interp
   if (!m_TextBlock.Init ())
      return FALSE;

   // parse it
   m_pNode = ParseMML (pszPageText, pWindow->m_hInstancePage, pCallback, this, m_TextBlock.m_pError);
   if (!m_pNode) {
      // call the error notification
      ESCMINTERPRETERROR ie;
      ie.pError = m_TextBlock.m_pError;
      Message (ESCM_INTERPRETERROR, &ie);

      return FALSE;
   }

   return Init2 (FALSE, m_pNode, pCallback, pWindow, pUserData, pfi);
      // Init2 will delete m_pNode
}

BOOL CEscPage::Init (PSTR pszPageText, PESCPAGECALLBACK pCallback, CEscWindow *pWindow, PVOID pUserData, IFONTINFO *pfi)
{
   PWSTR pszUnicode;

   pszUnicode = DataToUnicode (pszPageText, (DWORD)strlen(pszPageText));
   if (!pszUnicode)
      return FALSE;

   BOOL  fRet;
   fRet = Init (pszUnicode, pCallback, pWindow, pUserData, pfi);
   ESCFREE (pszUnicode);

   return fRet;
}

BOOL CEscPage::Init (DWORD dwResource, PESCPAGECALLBACK pCallback, CEscWindow *pWindow, PVOID pUserData, IFONTINFO *pfi)
{
   PWSTR pszUnicode;

   pszUnicode = ResourceToUnicode (pWindow->m_hInstancePage, dwResource);
   if (!pszUnicode)
      return FALSE;

   BOOL  fRet;
   fRet = Init (pszUnicode, pCallback, pWindow, pUserData, pfi);
   ESCFREE (pszUnicode);

   return fRet;
}

BOOL CEscPage::Init (PCMMLNode pNode, PESCPAGECALLBACK pCallback, CEscWindow *pWindow, PVOID pUserData, IFONTINFO *pfi)
{
   PCMMLNode   pClone;
   pClone = pNode->Clone();
   return Init2 (TRUE, pClone, pCallback, pWindow, pUserData, pfi);
      // Init2 will delete the clone
}

/*********************************************************************************************
Init2 - Internal function that only calls textblock.init if flag set

  NOTE - This WILL delete pNode.
*/
BOOL CEscPage::Init2 (BOOL fCall, PCMMLNode pNode, PESCPAGECALLBACK pCallback, CEscWindow *pWindow, PVOID pUserData, IFONTINFO *pfi)
{
   if (fCall) {
      // fail if already intialized
      if (m_pWindow)
         return FALSE;
      if (!pNode)
         return FALSE;

      // remember some stuff
      m_pNode = pNode;
      m_pCallback = pCallback;
      m_pWindow = pWindow;
      m_pUserData = pUserData;
      m_fi = *pfi;

      Message (ESCM_CONSTRUCTOR);

      // initalize the interp
      if (!m_TextBlock.Init ())
         return FALSE;
   }

   // determine viewing area
   m_rVisible.left = m_rVisible.top = 0;
   m_iLastWidth = m_rVisible.right = pWindow->m_rClient.right - pWindow->m_rClient.left;
   m_iLastHeight = pWindow->m_rClient.bottom - pWindow->m_rClient.top;
   m_rVisible.bottom = pWindow->m_rClient.bottom - pWindow->m_rClient.top;

   // BUGFIX - Look at the margin info first
   PCMMLNode pSub;
   PWSTR psz;
   pSub = NULL;
   if (m_pNode)
      m_pNode->ContentEnum (m_pNode->ContentFind (L"pageinfo"), &psz, &pSub);
   if (pSub) {
      // margins
      int   iVal;
      if (AttribToDecimal(pSub->AttribGet(L"LRMargin"), &iVal))
         m_rMargin.left = m_rMargin.right = iVal;
      if (AttribToDecimal(pSub->AttribGet(L"TBMargin"), &iVal))
         m_rMargin.top = m_rMargin.bottom = iVal;
      if (AttribToDecimal(pSub->AttribGet(L"leftMargin"), &iVal))
         m_rMargin.left = iVal;
      if (AttribToDecimal(pSub->AttribGet(L"rightMargin"), &iVal))
         m_rMargin.right = iVal;
      if (AttribToDecimal(pSub->AttribGet(L"topMargin"), &iVal))
         m_rMargin.top = iVal;
      if (AttribToDecimal(pSub->AttribGet(L"bottomMargin"), &iVal))
         m_rMargin.bottom = iVal;

   }

   // if we're automatically adjusting right size, then set to width of screen
   int   iWidth;
   iWidth = m_rVisible.right - (m_rMargin.left + m_rMargin.right);
   if (m_pWindow->m_dwStyle & EWS_AUTOWIDTH)
      iWidth = GetSystemMetrics (SM_CXSCREEN);

   // interpret
   HDC   hDC;
   hDC = pWindow->DCGet();
   m_TextBlock.m_fi = m_fi;
   if (!m_TextBlock.Interpret (this, NULL, m_pNode, hDC, pWindow->m_hInstancePage, iWidth, TRUE)) {
      pWindow->DCRelease();

      // call the error notification
      ESCMINTERPRETERROR ie;
      ie.pError = m_TextBlock.m_pError;
      Message (ESCM_INTERPRETERROR, &ie);

      return FALSE;
   }
   pWindow->DCRelease();

   // look in the pageinfo for window text, etc.
   if (m_TextBlock.m_pNodePageInfo) {
      // BUGFIX - Moved above
      // margins
      //int   iVal;
      //if (AttribToDecimal(m_TextBlock.m_pNodePageInfo->AttribGet(L"LRMargin"), &iVal))
      //   m_rMargin.left = m_rMargin.right = iVal;
      //if (AttribToDecimal(m_TextBlock.m_pNodePageInfo->AttribGet(L"TBMargin"), &iVal))
      //   m_rMargin.top = m_rMargin.bottom = iVal;
      //if (AttribToDecimal(m_TextBlock.m_pNodePageInfo->AttribGet(L"leftMargin"), &iVal))
      //   m_rMargin.left = iVal;
      //if (AttribToDecimal(m_TextBlock.m_pNodePageInfo->AttribGet(L"rightMargin"), &iVal))
      //   m_rMargin.right = iVal;
      //if (AttribToDecimal(m_TextBlock.m_pNodePageInfo->AttribGet(L"topMargin"), &iVal))
      //   m_rMargin.top = iVal;
      //if (AttribToDecimal(m_TextBlock.m_pNodePageInfo->AttribGet(L"bottomMargin"), &iVal))
      //   m_rMargin.bottom = iVal;

      // title
      WCHAR *psz;
      psz = m_TextBlock.m_pNodePageInfo->AttribGet (L"Title");
      if (psz)
         m_pWindow->TitleSet (psz);
   }

   if (!MassageControls ())
      return FALSE;

   // make sure the scrollbar is set up
   m_pWindow->ScrollBar (m_rVisible.top, m_rVisible.bottom - m_rVisible.top, m_rTotal.bottom);

   // set focus to a control
   // Dont do this because if do, then may scroll to middle/bottom of page
   // in order to ensure control with focus is visibleFocusToNextControl ();

   // add alt-f4 - close
   ESCACCELERATOR a;
   memset (&a, 0, sizeof(a));
   a.c = VK_F4;
   a.fAlt = TRUE;
   a.dwMessage = ESCM_CLOSE;
   m_listESCACCELERATOR.Add (&a);

   // if one of the controls has "defcontrol" then set focus
   DWORD i;
   for (i = 0; i < m_treeControls.Num(); i++) {
      PWSTR psz;
      psz = m_treeControls.Enum(i);
      if (!psz)
         continue;

      PCEscControl p;
      p = ControlFind (psz);
      if (!p) continue;

      // check the attributes
      if (p->m_fDefControl) {
         FocusSet (p);
         break;
      }
   }

   // call the page init
   Message (ESCM_INITPAGE);

   return TRUE;
}

BOOL CEscPage::InitFile (PWSTR pszFile, PESCPAGECALLBACK pCallback, CEscWindow *pWindow, PVOID pUserData, IFONTINFO *pfi)
{
   PWSTR pszUnicode;

   pszUnicode = FileToUnicode (pszFile);
   if (!pszUnicode)
      return FALSE;

   BOOL  fRet;
   fRet = Init (pszUnicode, pCallback, pWindow, pUserData, pfi);
   ESCFREE (pszUnicode);

   return fRet;
}


/***********************************************************************
Exit - Cause the page to close. This will:
   1) Post the exit code onto the window.
   2) Destroy this page object

inputs
   PWSTR    pszExitCode - Exit code. Cant be NULL
returns
   BOOL - TRUE if OK
*/
#define EXITCODEMAX     256
BOOL CEscPage::Exit (PWSTR pszExitCode)
{
   // BUGFIX - If exit code then fail to exit. Put this in because got a crash on a page close
   // somehow the page's message loop was still running after the page was closed.

   // BUGFIX - Another fix to make sure can change exit code on the fly without
   // failing

   DWORD dwLen = (DWORD)wcslen(pszExitCode);
   if (m_pWindow->m_pszExitCode && (dwLen >= EXITCODEMAX))
      return FALSE;

   if (m_pWindow->m_pszExitCode && (dwLen >= EXITCODEMAX)) {
      ESCFREE (m_pWindow->m_pszExitCode);
      m_pWindow->m_pszExitCode = NULL;
   }
   if (!m_pWindow->m_pszExitCode)
      m_pWindow->m_pszExitCode = (WCHAR*) ESCMALLOC ((max(dwLen,EXITCODEMAX) + 1) * 2);
   wcscpy (m_pWindow->m_pszExitCode, pszExitCode);

   // store away the current scroll so when go back will be at right scroll posn
   m_pWindow->m_iExitVScroll = m_rVisible.top;

   return TRUE;
}


/***********************************************************************
Link - Does the following:
   1) Callsd Message (ESCM_LINK). If doesn't handle then the default handler will
      a) If it's http: https: mailto:, call up the Internet
      b) else, do Exit()

inputs
   PWSTR pszLink - link
returns
   BOOL - TRUE if succesful
*/
BOOL CEscPage::Link (PWSTR pszLink)
{
   ESCMLINK l;
   l.psz = pszLink;


   // must release capture or bad things happen
   if (m_pControlCapture)
      MouseCaptureRelease(m_pControlCapture);

   return Message (ESCM_LINK, &l);
}

/***********************************************************************
ReInterpret - Cause the page to be reinterpreted - such as if the size of
   the window has changed and text needs to be rewrapped. Calls m_Interpet,
   and then invalidates window

BUGGBUG - when resize IDEALLY should keep the top-left corner on the same
text as it was before. Right now, when resize only remembers the Y offset and
keeps that, which isn't correct

inputs
   none
returns
   BOOL - TRUE if OK
*/
BOOL CEscPage::ReInterpret (void)
{
   // if we're automatically adjusting right size, then set to width of screen
   int   iWidth;
   iWidth = m_pWindow->m_rClient.right - m_pWindow->m_rClient.left - (m_rMargin.left + m_rMargin.right);
   // BUGFIX - was this before: iWidth = m_iLastWidth - (m_rMargin.left + m_rMargin.right);
   if (m_pWindow->m_dwStyle & EWS_AUTOWIDTH)
      iWidth = GetSystemMetrics (SM_CXSCREEN);

   // interpret
   HDC   hDC;
   hDC = m_pWindow->DCGet();
   m_iLastWidth = m_pWindow->m_rClient.right - m_pWindow->m_rClient.left;
   m_iLastHeight = m_pWindow->m_rClient.bottom - m_pWindow->m_rClient.top;
   m_TextBlock.m_fi = m_fi;
   if (!m_TextBlock.ReInterpret (hDC, iWidth, TRUE)) {
      m_pWindow->DCRelease();

      // call the error notification
      ESCMINTERPRETERROR ie;
      ie.pError = m_TextBlock.m_pError;
      Message (ESCM_INTERPRETERROR, &ie);

      return FALSE;
   }
   m_pWindow->DCRelease();

   if (!MassageControls ())
      return FALSE;

   // make sure m_pCOntrolCapture, m_pControlMouse, and m_pControLFocus
   // are still valid. There's a chance they're not anymore
   if (!IsControlValid (m_pControlCapture))
      m_pControlCapture = NULL;
   if (!IsControlValid (m_pControlMouse))
      m_pControlMouse = NULL;
   if (!IsControlValid (m_pControlFocus))
      m_pControlFocus = NULL;

   return TRUE;
}

/***********************************************************************
FocusSet - Sets the focus to a specific control. This:
   1) Makes sure the control actually wants focus. Return NULL if not.
   2) Tells the old control its no longer in focus, setting its m_fFocus and ESCM_FOCUS.
   3) Tells the new control its in focus, setting its m_fFocus and ESCM_FOCUS
   4) Remembers this

inputs
   PCEscControl *pControl
returns
   BOOL - TRUE if OK
*/
BOOL CEscPage::FocusSet (PCEscControl pControl)
{
   if (pControl == m_pControlFocus)
      return TRUE;

   if (pControl && !pControl->m_dwWantFocus)
      return FALSE;

   // unfocus the old one
   if (m_pControlFocus) {
      m_pControlFocus->m_fFocus = FALSE;
      m_pControlFocus->Message (ESCM_FOCUS);
   }

   m_pControlFocus = pControl;

   // focus the new one
   if (m_pControlFocus) {
      m_pControlFocus->m_fFocus = TRUE;
      m_pControlFocus->Message (ESCM_FOCUS);

      // make sure it's visible
      if (m_pControlFocus->m_rPosn.bottom <= m_rVisible.top)
         VScroll (m_pControlFocus->m_rPosn.top);
      else if (m_pControlFocus->m_rPosn.top >= m_rVisible.bottom)
         VScroll (m_pControlFocus->m_rPosn.bottom - (m_rVisible.bottom - m_rVisible.top));
   }

   return TRUE;
}



/***********************************************************************
FocusGet - Returns which control ha focus.

returns
   PEscControl* - The one with focus. NULL if nothing has focus
*/
PCEscControl CEscPage::FocusGet (void)
{
   return m_pControlFocus;
}



/**********************************************************************
VScrollToSection - Finds the <section> with the specified name (no
# before the name) and vertically scrolls the page so the section
title is at the top of the page.

inputs
   PWSTR    psz - section string
returns
   BOOL - TRUE if success. FALSE if error
*/
BOOL CEscPage::VScrollToSection (PWSTR psz)
{
   // find it in the text block
   int   i;
   i = m_TextBlock.SectionFind (psz);
   if (i == -1)
      return FALSE;  // cant find

   // else scroll
   i = max(i-2, 0); // scroll up a bit
   return VScroll (i);
}

/***********************************************************************
VScroll - Causes the page to vertical scroll so pixel (page coord) iY is
at the top of the windows client area.

Basiucally:
   1) Change the param (after validating it)
   2) ESCM_SCROLL
   3) Notifies the parent window of the scroll

inputs
   int   iY
retursn
   BOOL - TRUE if OK
*/
BOOL CEscPage::VScroll (int iY)
{
#if 0
   char  szTemp[256];
   wsprintf (szTemp, "Scroll = %d\r\n", iY);
   OutputDebugString (szTemp);
#endif

   if (iY + (m_rVisible.bottom - m_rVisible.top) > m_rTotal.bottom)
      iY = m_rTotal.bottom - (m_rVisible.bottom - m_rVisible.top);
   if (iY < 0)
      iY = 0;
   int   iDelta;
   iDelta = iY - m_rVisible.top;
   if (iDelta) {
      m_rVisible.top += iDelta;
      m_rVisible.bottom += iDelta;

      // tell the window control that scrolled, so it should scroll the bitmap, etc.
      m_pWindow->ScrollMe ();
      Message (ESCM_SCROLL);
   }

   m_pWindow->ScrollBar (m_rVisible.top, m_rVisible.bottom - m_rVisible.top, m_rTotal.bottom);

   return TRUE;
}

/***********************************************************************
SetCursor - Sets the cursor that the page uses if it's asked.

inputs
   HCURSOR  hCursor - cursor.
      or
   DWORD dwID - Cursor ID from LoadCursor (NULL, XXX)
returns
   BOOL - TRUE if OK
*/
BOOL CEscPage::SetCursor (HCURSOR hCursor)
{
   return m_pWindow->SetCursor (hCursor);
}

BOOL CEscPage::SetCursor (DWORD dwID)
{
   return m_pWindow->SetCursor (dwID);
}

/***********************************************************************
Invalidate - Invalidates a section of the page so we know it need spainting.
   Basically does some coordinate translations and calls the Windows API
   InvalidateRect.

inptus
   RECT     *pRetct - Rectangle. If NULL invalidates entire page
returns
   BOOL - TRUE if OK
*/
BOOL CEscPage::Invalidate (RECT *pRect)
{
   // if null invalidate entire area
   if (!pRect)
      pRect = &m_rTotal;

   // convert to window coords
   RECT  cw;
   CoordPageToWindow (pRect, &cw);

   // only invalidate if its within the visible region
   RECT  rInvalid;
   if (!IntersectRect (&rInvalid, pRect, &m_rVisible))
      return TRUE;

retry:
   // see if this intersects with any currently invalid
   DWORD i;
   for (i = 0; i < m_pWindow->m_listInvalid.Num(); i++) {
      RECT  *pr, rInt;
      pr = (RECT*) m_pWindow->m_listInvalid.Get(i);

      // we want to combine contiguuous chunks togehter too, so because
      // intersect rect doesn't tell us if they're contiguous, add one
      // to borders
      RECT  r1, r2;
      r1 = rInvalid;
      r2 = *pr;
      r1.left -= 1;
      r1.right += 1;
      r1.top -= 1;
      r1.bottom += 1;
      r2.left -= 1;
      r2.right += 1;
      r2.top -= 1;
      r2.bottom += 1;
      if (IntersectRect (&rInt, &r1, &r2)) {
         // the two overlap, so to make refresh simpler, just union them
         UnionRect (&rInt, &rInvalid, pr);
         rInvalid = rInt;

         // delete this
         m_pWindow->m_listInvalid.Remove (i);

         // go back and try this all again since new, larger
         // invalid region may intersect with others
         goto retry;
      }
   }

   // if we get here it doesn't intersect withanything, so add it
   m_pWindow->m_listInvalid.Add (&rInvalid);

   // finally, invalidate
   if (m_pWindow->m_hWnd)
      InvalidateRect (m_pWindow->m_hWnd, &cw, FALSE);
   return TRUE;
}

/***********************************************************************
Update - Forces an update of any invalidated areas. Basically calls UpdateWindow

return
   BOOL - TRUE if OK
*/
BOOL CEscPage::Update (void)
{
   if (m_pWindow->m_hWnd)
      UpdateWindow (m_pWindow->m_hWnd);
   return TRUE;
}

/***********************************************************************
CoordXXXToXXX - Converts coordinates from page to window (client) to
screen, and back. Either converts points or rects.
*/
void CEscPage::CoordPageToWindow (POINT *pPage, POINT *pWindow)
{
   pWindow->x = pPage->x - m_rVisible.left;
   pWindow->y = pPage->y - m_rVisible.top;
}

void CEscPage::CoordPageToWindow (RECT *pPage, RECT *pWindow)
{
   POINT *p1 = (POINT*) pPage;
   POINT *p2 = (POINT*) pWindow;

   CoordPageToWindow (p1, p2);
   p1++;
   p2++;
   CoordPageToWindow (p1, p2);
}

void CEscPage::CoordWindowToPage (POINT *pWindow, POINT *pPage)
{
   pPage->x = pWindow->x + m_rVisible.left;
   pPage->y = pWindow->y + m_rVisible.top;
}

void CEscPage::CoordWindowToPage (RECT *pWindow, RECT *pPage)
{
   POINT *p1 = (POINT*) pWindow;
   POINT *p2 = (POINT*) pPage;

   CoordWindowToPage (p1, p2);
   p1++;
   p2++;
   CoordWindowToPage (p1, p2);
}

void CEscPage::CoordPageToScreen (POINT *pPage, POINT *pScreen)
{
   pScreen->x = pPage->x - m_rVisible.left + m_pWindow->m_rClient.left;
   pScreen->y = pPage->y - m_rVisible.top + m_pWindow->m_rClient.top;
}

void CEscPage::CoordPageToScreen (RECT *pPage, RECT *pScreen)
{
   POINT *p1 = (POINT*) pPage;
   POINT *p2 = (POINT*) pScreen;

   CoordPageToScreen (p1, p2);
   p1++;
   p2++;
   CoordPageToScreen (p1, p2);
}

void CEscPage::CoordScreenToPage (POINT *pScreen, POINT *pPage)
{
   pPage->x = pScreen->x - m_pWindow->m_rClient.left + m_rVisible.left;
   pPage->y = pScreen->y - m_pWindow->m_rClient.top + m_rVisible.top;
}

void CEscPage::CoordScreenToPage (RECT *pScreen, RECT *pPage)
{
   POINT *p1 = (POINT*) pScreen;
   POINT *p2 = (POINT*) pPage;

   CoordScreenToPage (p1, p2);
   p1++;
   p2++;
   CoordScreenToPage (p1, p2);
}


/***********************************************************************
BOOL CEscPage::Handle3DControls - Tell 3D controls that they have moved
so they can redraw.
*/
BOOL CEscPage::Handle3DControls (void)
{
   // look through all the controls and see if they want to be invalidated on move
   DWORD j, dwNum;
   TWOBJECTPOSN *pop;
   dwNum = m_TextBlock.m_plistTWOBJECTPOSN->Num();
   if (!dwNum)
      return FALSE;  // nothing to set focus to
   for (j = 0; j < dwNum ; j++) {
      pop = (TWOBJECTPOSN*) m_TextBlock.m_plistTWOBJECTPOSN->Get(j);
      if (!pop || !pop->qwID)
         continue;   // not here

      // else see what it is
      CONTROLOBJECT *pto;
      pto = (CONTROLOBJECT*) pop->qwID;
      if ((pto->dwID != MMLOBJECT_CONTROL) || !pto->pControl)
         continue;
      PCEscControl   pControl;
      pControl = (PCEscControl) pto->pControl;

      if (pControl->m_fRedrawOnMove) {
         RECT  rInt;
         if (IntersectRect (&rInt, &m_rVisible, &pControl->m_rPosn)) {
#ifdef _DEBUG
            char  szTemp[256];
            char  szName[256];
            szName[0] = 0;
            if (pControl->m_pszName)
               WideCharToMultiByte (CP_ACP, 0, pControl->m_pszName, -1, szName, sizeof(szName), 0, 0);
            wsprintf (szTemp, "fRedrawOnMove %s (%d,%d) to (%d,%d)\r\n",
               szName,
               pControl->m_rPosn.left, pControl->m_rPosn.top,
               pControl->m_rPosn.right, pControl->m_rPosn.bottom);
            OutputDebugString (szTemp);
#endif
            Invalidate (&rInt);
         }
      }
   }
   
   return TRUE;
}


/***********************************************************************
Message - Sends a message to the page. This trys sending it to m_pCallback.
   If there isn't one, or it returns FALSE, then this acts as the default
   message handler.

inputs
   DWORD    dwMessage
   PVOID    pParam - depends upon the message
returns
   BOOL - TRUE if handled
*/
BOOL CEscPage::Message (DWORD dwMessage, PVOID pParam)
{
   // try sending to callback
   if (m_pCallback && m_pCallback(this, dwMessage, pParam))
      return TRUE;

   switch (dwMessage) {
   case ESCM_CLOSE:
      Exit (L"[Close]");
      return TRUE;


   case ESCM_SIZE:
      if ( (m_iLastWidth != (m_pWindow->m_rClient.right - m_pWindow->m_rClient.left)) ||
         ( m_TextBlock.m_fFoundStretch && (m_iLastHeight != (m_pWindow->m_rClient.bottom - m_pWindow->m_rClient.top))) )
         ReInterpret ();

      // note the potential scroll so the parent window knows
      // Call VScroll because checks boundary conditison
      VScroll (m_rVisible.top);
      // m_pWindow->ScrollBar (m_rVisible.top, m_rVisible.bottom - m_rVisible.top, m_rTotal.bottom);

      return TRUE;

   case ESCM_MOVE:
      Handle3DControls();
      return TRUE;

   case ESCM_SCROLL:
      Handle3DControls();
      return TRUE;

   case ESCM_MOUSEHOVER:
      {
         // if it's over a control pass it on. else ignore.
         if (m_pControlMouse)
            m_pControlMouse->Message (dwMessage, pParam);
      }
      return TRUE;

   case ESCM_MOUSEMOVE:
      {
         ESCMMOUSEMOVE *p = (ESCMMOUSEMOVE*) pParam;

         // if we have a capture send down to that and done
         if (m_pControlCapture) {
            m_pControlCapture->m_fMouseOver = PtInRect (&m_pControlCapture->m_rPosn, p->pPosn);
            m_pControlCapture->Message (ESCM_MOUSEMOVE, pParam);
            return TRUE;
         }

         // else, see what it's over. It's not over anything if
         // it's off the visible display area
         PCEscControl   pOver;
         pOver = m_pWindow->m_fMouseOver ? ControlFromPoint(&p->pPosn) : NULL;

         // BUGFIX - if not enabled then don't click over
         if (pOver && (pOver != m_pControlMouse)) {
            if (!pOver->m_fEnabled)
               pOver = NULL;
         }

         // if it's over the same thing as last time no big deal, else we have
         // to call leave and enter
         if (pOver != m_pControlMouse)  {
            if (m_pControlMouse) {
               m_pControlMouse->m_fMouseOver = FALSE;
               m_pControlMouse->Message (ESCM_MOUSELEAVE);
            }
            if (pOver) {
               pOver->m_fMouseOver = TRUE;
               pOver->Message (ESCM_MOUSEENTER);
            }
            else
               // if it's not over anything then set the cursor to can't click
               SetCursor ((DWORD)IDC_NOCURSOR);
         }

         // tell the new one about the move
         m_pControlMouse = pOver;
         if (m_pControlMouse)
            m_pControlMouse->Message (ESCM_MOUSEMOVE, pParam);
         else
            SetCursor ((DWORD)IDC_NOCURSOR);

      }
      return TRUE;

   case ESCM_MOUSELEAVE:
      {
         // not over anything
         PCEscControl   pOver;
         pOver = NULL;

         // if it's over the same thing as last time no big deal, else we have
         // to call leave and enter
         if (m_pControlMouse)  {
            m_pControlMouse->m_fMouseOver = FALSE;
            m_pControlMouse->Message (ESCM_MOUSELEAVE);
         }

         // tell the new one about the move
         m_pControlMouse = pOver;
         SetCursor ((DWORD)IDC_NOCURSOR);

      }
      return TRUE;

   case ESCM_LBUTTONDOWN:
   case ESCM_MBUTTONDOWN:
   case ESCM_RBUTTONDOWN:
      {
         // call mouse move first just to get everything squared away
         Message (ESCM_MOUSEMOVE, pParam);

         // now, if there's nothing captured then capture the current mouse
         if (!m_pControlCapture) {
            m_pControlCapture = m_pControlMouse;
            if (!m_pControlCapture) {
               // might want to beep when click on a non-control
               m_pWindow->Beep (ESCBEEP_DONTCLICK);
               return TRUE;   // clicked on a non-control
            }

            // set focus to the control
            FocusSet (m_pControlCapture);
         }

         // CEscWindow has already captured the mouse for us, so do nothing
         // except set some flags
         m_pControlCapture->m_fCapture = m_pWindow->m_fCapture;
         m_pControlCapture->m_fLButtonDown = m_pWindow->m_fLButtonDown;
         m_pControlCapture->m_fMButtonDown = m_pWindow->m_fMButtonDown;
         m_pControlCapture->m_fRButtonDown = m_pWindow->m_fRButtonDown;

         // pass on the mesage
         m_pControlCapture->Message (dwMessage, pParam);

         // BUGBUG - 2.0 - On IE pressing MButton down and moving mouse causes an easy
         // scroll. DO that?
      }
      return TRUE;

   case ESCM_LBUTTONUP:
   case ESCM_MBUTTONUP:
   case ESCM_RBUTTONUP:
      {
         // BUGFIX: if no control has captured this then don't send message
         if (!m_pControlCapture)
            return TRUE;

         // call mouse move first just to get everything squared away
         Message (ESCM_MOUSEMOVE, pParam);

         PCEscControl   pControl;
         pControl = m_pControlCapture ? m_pControlCapture : m_pControlMouse;

         // if a control is marked as captured but capture is now off
         // then mark it as uncaptured
         if (m_pControlCapture && !m_pWindow->m_fCapture)
            m_pControlCapture = NULL;

         // CEscWindow has already captured the mouse for us, so do nothing
         // except set some flags
         if (pControl) {
            pControl->m_fCapture = m_pWindow->m_fCapture;
            pControl->m_fLButtonDown = m_pWindow->m_fLButtonDown;
            pControl->m_fMButtonDown = m_pWindow->m_fMButtonDown;
            pControl->m_fRButtonDown = m_pWindow->m_fRButtonDown;

            // pass on the mesage
            pControl->Message (dwMessage, pParam);
         }
      }
      return TRUE;


   case ESCM_LINK:
      {
         ESCMLINK *p = (ESCMLINK*) pParam;

         // see if it matches a bookmark in the page. If so, scroll there
         if (p->psz[0] == L'#') {
            VScrollToSection (p->psz + 1);
            return TRUE;
         }

         // convert to ANSI for test purposes & see if its an internet link
         char  szTemp[512];
         WideCharToMultiByte (CP_ACP, 0, p->psz, -1, szTemp, sizeof(szTemp), 0, 0);
         char  szHTTP[] = "http:";
         char  szHTTPS[] = "https:";
         char  szMAILTO[] = "mailto:";
         if (!_strnicmp (szTemp, szHTTP, strlen(szHTTP)) ||
            !_strnicmp (szTemp, szHTTPS, strlen(szHTTPS)) ||
            !_strnicmp (szTemp, szMAILTO, strlen(szMAILTO))) {

            // it's internet, winexec
            ShellExecute (m_pWindow->m_hWnd,
               NULL, szTemp, NULL, NULL, SW_SHOW);
            return TRUE;
         }

         // else, exit
         Exit (p->psz);
      }
      return TRUE;


   case ESCM_CHAR:
   case ESCM_SYSCHAR:
      {
         ESCMCHAR *p = (ESCMCHAR*) pParam;
         DWORD i;
         WCHAR c = (WCHAR) towupper (p->wCharCode);

         // see if control wants to hog everything
         if (m_pControlFocus && (m_pControlFocus->m_dwWantFocus == 2)) {
            m_pControlFocus->Message (dwMessage, pParam);
            if (p->fEaten)
               return TRUE;   // hogged
         }

         // first off, always eat tabs and shift tabs so that a control cant monopolize
         if (p->wCharCode == L'\t') {
            FocusToNextControl (GetKeyState (VK_SHIFT) >= 0);
            return TRUE;
         }

         // pass down to control
         if (m_pControlFocus) {
            m_pControlFocus->Message (dwMessage, pParam);
            if (p->fEaten)
               return TRUE;
         }

         BOOL  fChar;
         switch (dwMessage) {
            case ESCM_CHAR:
            case ESCM_SYSCHAR:
               fChar = TRUE;
               break;
            default:
               fChar = FALSE; // dealing with keydown
         }

         // look through all the page's acceleartors for a match
         for (i = 0; i < m_listESCACCELERATOR.Num(); i++) {
            ESCACCELERATOR *pa = (ESCACCELERATOR*) m_listESCACCELERATOR.Get(i);
            if (pa->c != c)
               continue;   // no match

            BOOL  fCanBeChar;
            fCanBeChar = (c == L' ') || (c == VK_ESCAPE) || (c > VK_HELP) || (c == 3) || ((c >= 24) && (c <= 26)) || (c == 22) || (c == 1);

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

         // look through all the controls and see if they have global accelerators
         DWORD j, dwNum;
         TWOBJECTPOSN *pop;
         dwNum = m_TextBlock.m_plistTWOBJECTPOSN->Num();
         if (!dwNum)
            return FALSE;  // nothing to set focus to
         for (j = 0; j < dwNum ; j++) {
            pop = (TWOBJECTPOSN*) m_TextBlock.m_plistTWOBJECTPOSN->Get(j);
            if (!pop || !pop->qwID)
               continue;   // not here

            // else see what it is
            CONTROLOBJECT *pto;
            pto = (CONTROLOBJECT*) pop->qwID;
            if ((pto->dwID != MMLOBJECT_CONTROL) || !pto->pControl)
               continue;
            PCEscControl   pControl;
            pControl = (PCEscControl) pto->pControl;

            // loop through accel
            for (i = 0; i <= pControl->m_listAccelNoFocus.Num(); i++) {
               ESCACCELERATOR *pa = (ESCACCELERATOR*) pControl->m_listAccelNoFocus.Get(i);
               if (!pa)
                  pa = &pControl->m_AccelSwitch;

               if (pa->c != c) {
                  // BUGFIX - Allow ctrl-P to work as an accelerator
                  if (pa->fControl && (c >= 1) && (c <= 26)) {
                     if ( ((c + L'a' - 1) == pa->c) || ((c + L'A' - 1) == pa->c))
                        goto skipforcontrol;
                  }

                  continue;   // no match
               }

skipforcontrol:
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
               pControl->Message (pa->dwMessage);
               return TRUE;
            }
         }
         
         // should beep since dont understand?
         m_pWindow->Beep (ESCBEEP_DONTCLICK);
      }
      return TRUE;

   case ESCM_KEYDOWN:
   case ESCM_SYSKEYDOWN:
      {
         ESCMKEYDOWN *p = (ESCMKEYDOWN*) pParam;

         // pass it onto the control with focus
         if (m_pControlFocus) {
            m_pControlFocus->Message (dwMessage, pParam);
            if (p->fEaten)
               return TRUE;
         }
         // arrow keys and page/up down, and page end/begin

         BOOL  fChar;
         switch (dwMessage) {
            case ESCM_CHAR:
            case ESCM_SYSCHAR:
               fChar = TRUE;
               break;
            default:
               fChar = FALSE; // dealing with keydown
         }

         // look through all the page's acceleartors for a match
         DWORD i;
         WCHAR c;
         c = p->nVirtKey;
         // BUGFIX - dont do below so can handle Alt-F4. VK_F4 seems to be in the
         // lower-case region
         //if ((c >= L'a') && (c <= L'z'))
         //   c = (WCHAR) towupper (c);
         for (i = 0; i < m_listESCACCELERATOR.Num(); i++) {
            ESCACCELERATOR *pa = (ESCACCELERATOR*) m_listESCACCELERATOR.Get(i);
            if (pa->c != c)
               continue;   // no match

            BOOL  fCanBeChar;
            fCanBeChar = (c == L' ') || (c == VK_ESCAPE) || (c > VK_HELP) || (c == 3) || ((c >= 24) && (c <= 26)) || (c == 22) || (c == 1);
            if ((c >= VK_F1) && (c <= VK_F12))
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

         // BUGFIX - If alt is preseed for any of these then ignore because it's
         // eating some keys for the hold alt down why typing 0140.
         if (p->lKeyData & (1<<29))
            return TRUE;

         // BUGFIX - On compaq presario portable scroll buttons move twice as much
         if (p->nVirtKey == VK_UP) {
            VScroll (m_rVisible.top - 32);
            // IMPORTANT - using 32 for line up/down. Really should look at font
         }
         else if (p->nVirtKey == VK_DOWN) {
            VScroll (m_rVisible.top + 32);
         }
         else if (p->nVirtKey == VK_NEXT) {
            VScroll (m_rVisible.bottom);
         }
         else if (p->nVirtKey == VK_PRIOR) {
            VScroll (m_rVisible.top - (m_rVisible.bottom - m_rVisible.top));
         }
         else if (p->nVirtKey == VK_HOME) {
            VScroll (0);
         }
         else if (p->nVirtKey == VK_END) {
            VScroll (m_rTotal.bottom);
         }

      }
      return TRUE;

   case ESCM_KEYUP:
   case ESCM_SYSKEYUP:
      // pass it onto the control with focus
      if (m_pControlFocus)
         m_pControlFocus->Message (dwMessage, pParam);
      return TRUE;

   case ESCM_ENDSESSION:
      Exit(L"[EndSession]");
      return TRUE;

   case ESCM_QUERYENDSESSION:
      Message (ESCM_CLOSE);
      return TRUE;

   case ESCM_MOUSEWHEEL:
      {
         PESCMMOUSEWHEEL p = (PESCMMOUSEWHEEL) pParam;

         // BUGFIX - If there's a control with focus send whe mousewheel to it
         // If it doesn't like it then scroll the window
         if (m_pControlFocus) {
            BOOL  fRet;
            fRet = m_pControlFocus->Message (dwMessage, pParam);
            if (fRet)
               return fRet;
         }

         // BUGFIX - On compaq presario portable scroll buttons move twice as much
         // BUGFIX - Mousewheel going in the wrong direction, so flip + and -
         // BUGFIX - Double the speed, from 32 to 64
         if (p->zDelta < 0)
            VScroll (m_rVisible.top + 32*2);
         else
            VScroll (m_rVisible.top - 32*2);

      }
      return TRUE;
   }

   // The following messages have no default behavior
   // ESCM_CONSTRUCTOR
   // ESCM_DESTRUCTOR
   // ESCM_TIMER
   // ESCM_INITPAGE
   // ESCm_INTERPERROR
   // ESCM_ACTIVATE
   // ESCM_ACTIVATEAPP
   // ESCM_SUBSTITUTION
   // ESCM_MOUSEENTER
   // ESCM_MOUSELEAVE
   // Messages from controls

   return FALSE;
}

/***********************************************************************
ControlFromPoint - Given a point (in page coord) return a pointer to the
   control that's there. Good for hit testing.

IMPORTANT - this is being done the real slow way. Need to use optimized version later

inputs
   POINT    *pPage - in page coord
returns
   PCEscControl - control
*/
PCEscControl CEscPage::ControlFromPoint (POINT *pPage)
{
   // go through all the controls and tell them they've moved
   DWORD i, dwNum;
   TWOBJECTPOSN *pop;
   dwNum = m_TextBlock.m_plistTWOBJECTPOSN->Num();
   for (i = dwNum - 1; i < dwNum ; i--) { // go backwards
      pop = (TWOBJECTPOSN*) m_TextBlock.m_plistTWOBJECTPOSN->Get(i);
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

      // if the control doesn't want mouse input then ignore. That
      // way can have mouse-transparent controls
      if (!pControl->m_fWantMouse)
         continue;

      if (PtInRect (&pControl->m_rPosn, *pPage))
         return pControl;
   }

   return NULL;
}


/***********************************************************************
Paint - Causes the page to paint to a HDC. This goes through all the
   controls and text and gets them to draw.

inputs
   RECT     *pPageCoord - Rectangle (in page coordinates) that needs drawing.
   RECT     *pHDCCoord - Rectangle (in HDC coordinates) where to draw to. Has
               the same dimensions as pPAgeCoord.
   RECT     *pScreenCoord - Where this will appear on the screen. Some controls
               will use this for perspective. Same dimensions as pPageCoord.
   HDC      hDC - DC
returns
   BOOL - TRUE if OK
*/

#ifndef SM_CXVIRTUALSCREEN
#define SM_CXVIRTUALSCREEN      78
#define SM_CYVIRTUALSCREEN      79
#endif // SM_CXVIRTUALSCREEN

BOOL CEscPage::Paint (RECT *pPageCoord, RECT *pHDCCoord, RECT *pScreenCoord,
                      HDC hDC)
{
   if (!m_TextBlock.m_plistTWTEXTELEM || !m_TextBlock.m_plistTWOBJECTPOSN)
      return FALSE;

   // total screen coordinates
   RECT  r;
   r.left = r.top = 0;
   r.right = GetSystemMetrics (SM_CXVIRTUALSCREEN);
   r.bottom = GetSystemMetrics (SM_CYVIRTUALSCREEN);
   // BUGFIX - OnWin98, if have two screens then total screen may be larger
   if (!r.right || !r.bottom) {
      r.right = GetSystemMetrics (SM_CXSCREEN);
      r.bottom = GetSystemMetrics (SM_CYSCREEN);
   }

   // figure out the offset
   POINT pOffset;
   pOffset.x = pHDCCoord->left - pPageCoord->left;
   pOffset.y = pHDCCoord->top - pPageCoord->top;
   m_TextBlock.Paint (hDC, &pOffset, pHDCCoord, pScreenCoord, &r);

   return TRUE;
}

/***************************************************************************
MouseCaptureRelease - A control calls this to release its capture on the mouse.

inputs
   PCEscControl   pControl - The control releasing
returns
   BOOL - TRUE if successful
*/
BOOL CEscPage::MouseCaptureRelease (PCEscControl pControl)
{
   if (pControl != m_pControlCapture)
      return FALSE;

   // else, go for it
   m_pControlCapture = NULL;
   pControl->m_fCapture = FALSE;
   pControl->m_fLButtonDown = pControl->m_fMButtonDown = pControl->m_fRButtonDown = FALSE;
      // since no longer know if button is down
   m_pWindow->MouseCaptureRelease ();
   return TRUE;
}


/***************************************************************************
FocusToNextControl - Does the effect of a tab (or shift tab) and moved the focus
forward or backward.

inputs
   BOOL     fForward - If true move forward, FALSE backward
returns
   BOOL - error
*/
BOOL CEscPage::FocusToNextControl (BOOL fForward)
{
   // figure which control currently has focus
   DWORD i, dwNum;
   TWOBJECTPOSN *pop;
   dwNum = m_TextBlock.m_plistTWOBJECTPOSN->Num();
   if (!dwNum)
      return FALSE;  // nothing to set focus to
   for (i = 0; i < dwNum ; i++) {
      pop = (TWOBJECTPOSN*) m_TextBlock.m_plistTWOBJECTPOSN->Get(i);
      if (!pop || !pop->qwID)
         continue;   // not here

      // else see what it is
      CONTROLOBJECT *pto;
      pto = (CONTROLOBJECT*) pop->qwID;
      if (pto->dwID != MMLOBJECT_CONTROL)
         continue;

      if (pto->pControl == m_pControlFocus)
         break;
   }

   // if nothing has focus, setting depends upon forward/back
   DWORD dwStart;
   if (i >= dwNum)
      dwStart = fForward ? 0 : dwNum-1;
   else
      dwStart = fForward ? ((i + 1) % dwNum) : ((i+dwNum-1) % dwNum);

   // loop lookin for a control that takes focus
   DWORD dwIndex;
   for (i = 0; i <= dwNum; i++) {
      if (fForward)
         dwIndex = (dwStart + i) % dwNum;
      else
         dwIndex = (dwStart + dwNum - i) % dwNum;

      // get ath object
      pop = (TWOBJECTPOSN*) m_TextBlock.m_plistTWOBJECTPOSN->Get(dwIndex);
      if (!pop || !pop->qwID)
         continue;   // not here

      // else see what it is
      CONTROLOBJECT *pto;
      pto = (CONTROLOBJECT*) pop->qwID;
      if ((pto->dwID != MMLOBJECT_CONTROL) || !pto->pControl)
         continue;

      PCEscControl pControl;
      pControl = (PCEscControl) pto->pControl;
      if (!pControl->m_dwWantFocus)
         continue;

      // special hack for radio buttons - send ESCM_RADIOWANTFOCUS
      ESCMRADIOWANTFOCUS wf;
      wf.fAcceptFocus = TRUE;
      pControl->Message (ESCM_RADIOWANTFOCUS, &wf);
      if (!wf.fAcceptFocus)
         continue;

      // finally, set the focus
      return FocusSet (pControl);
   }

   // cant find
   return FALSE;
}

/********************************************************************************
IsControlValid - Called after a page-resize to see if the control is valid

inputs
   PCEscControl   pControl
returns
   BOOL - TRUE if VALID
*/
BOOL CEscPage::IsControlValid (PCEscControl pControl)
{
   if (!pControl)
      return NULL;

   // figure which control currently has focus
   DWORD i, dwNum;
   TWOBJECTPOSN *pop;
   dwNum = m_TextBlock.m_plistTWOBJECTPOSN->Num();
   if (!dwNum)
      return FALSE;  // nothing to set focus to
   for (i = 0; i < dwNum ; i++) {
      pop = (TWOBJECTPOSN*) m_TextBlock.m_plistTWOBJECTPOSN->Get(i);
      if (!pop || !pop->qwID)
         continue;   // not here

      // else see what it is
      CONTROLOBJECT *pto;
      pto = (CONTROLOBJECT*) pop->qwID;
      if (pto->dwID != MMLOBJECT_CONTROL)
         continue;

      if (pto->pControl == pControl)
         return TRUE;
   }

   // else not
   return FALSE;
}


/********************************************************************************
ControlFind - Given a control's name, finds it in the page.

inputs
   WCHAR    *pszName - name
returns
   PCEscControl - Control pointer. NULL if none
*/
PCEscControl CEscPage::ControlFind (WCHAR *pszName)
{
   PCEscControl   *pp;
   pp = (PCEscControl*) m_treeControls.Find (pszName);
   if (!pp)
      return NULL;
   return *pp;
}




/********************************************************************************
MessageBox - Displays a mesaage box. See EscMessageBox for details on parameters
*/
DWORD CEscPage::MessageBox (PWSTR pszTitle, PWSTR pszSummary, PWSTR pszFinePrint, DWORD dwType)
{
   return EscMessageBox (m_pWindow->m_hWnd, pszTitle, pszSummary, pszFinePrint, dwType);
}

/********************************************************************************
MBInformation - Displays a message box containing information that the user should
know, but which isn't absoluately required.

inputs
   PWSTR    pszSummary - 1 sentence summary
   PWSTR    pszFinePrint - Detailed information
   BOOL     fCancel - if TRUE, cancel button available in addition to OK
returns
   DWORD - IDOK or IDCANCEL
*/
DWORD CEscPage::MBInformation (PWSTR pszSummary, PWSTR pszFinePrint, BOOL fCancel)
{
   PWSTR psz;
   PWSTR pszInfo = L"Information";
   psz = pszInfo;
   if (m_TextBlock.m_pNodePageInfo)
      psz = m_TextBlock.m_pNodePageInfo->AttribGet (L"title");
   if (!psz)
      psz = pszInfo;

   return MessageBox (psz, pszSummary, pszFinePrint,
      MB_ICONINFORMATION | (fCancel ? MB_OKCANCEL : MB_OK));
}

/********************************************************************************
MBSpeakInformation - If TTS is installed and turned on, this speaks
the message instead of bringing up a message box. It if's not
installed it displays a message box containing information that the user should
know, but which isn't absoluately required.

inputs
   PWSTR    pszSummary - 1 sentence summary
   PWSTR    pszFinePrint - Detailed information
returns
   DWORD - IDOK or IDCANCEL
*/
DWORD CEscPage::MBSpeakInformation (PWSTR pszSummary, PWSTR pszFinePrint)
{
   if (EscSpeakTTS()) {
      // chime
      EscChime (ESCCHIME_INFORMATION);

      if (EscSpeak (pszSummary))
         return IDOK;
   }

   // else, normal info message box
   return MBInformation (pszSummary, pszFinePrint);
}


/********************************************************************************
MBSpeakWarning - If TTS is installed and turned on, this speaks
the message instead of bringing up a message box. It if's not
installed it displays a message box containing information that the user should
know, but which isn't absoluately required.

inputs
   PWSTR    pszSummary - 1 sentence summary
   PWSTR    pszFinePrint - Detailed information
returns
   DWORD - IDOK or IDCANCEL
*/
DWORD CEscPage::MBSpeakWarning (PWSTR pszSummary, PWSTR pszFinePrint)
{
   if (EscSpeakTTS()) {
      // chime
      EscChime (ESCCHIME_WARNING);

      if (EscSpeak (pszSummary))
         return IDOK;
   }

   // else, normal info message box
   return MBWarning (pszSummary, pszFinePrint);
}

/********************************************************************************
MBYesNo - Displays a message box containing a yes/no qestion.

inputs
   PWSTR    pszSummary - 1 sentence summary
   PWSTR    pszFinePrint - Detailed information
   BOOL     fCancel - if TRUE, cancel button available in addition to OK
returns
   DWORD - IDYES, IDNO or IDCANCEL
*/
DWORD CEscPage::MBYesNo (PWSTR pszSummary, PWSTR pszFinePrint, BOOL fCancel)
{
   PWSTR psz;
   PWSTR pszInfo = L"Question";
   psz = pszInfo;
   if (m_TextBlock.m_pNodePageInfo)
      psz = m_TextBlock.m_pNodePageInfo->AttribGet (L"title");
   if (!psz)
      psz = pszInfo;

   return MessageBox (psz, pszSummary, pszFinePrint,
      MB_ICONQUESTION | (fCancel ? MB_YESNOCANCEL : MB_YESNO));
}

/********************************************************************************
MBWarning - Displays a message box containing information that the user should
know about.

inputs
   PWSTR    pszSummary - 1 sentence summary
   PWSTR    pszFinePrint - Detailed information
   BOOL     fCancel - if TRUE, cancel button available in addition to OK
returns
   DWORD - IDOK or IDCANCEL
*/
DWORD CEscPage::MBWarning (PWSTR pszSummary, PWSTR pszFinePrint, BOOL fCancel)
{
   PWSTR psz;
   PWSTR pszInfo = L"Warning";
   psz = pszInfo;
   if (m_TextBlock.m_pNodePageInfo)
      psz = m_TextBlock.m_pNodePageInfo->AttribGet (L"title");
   if (!psz)
      psz = pszInfo;

   return MessageBox (psz, pszSummary, pszFinePrint,
      MB_ICONWARNING | (fCancel ? MB_OKCANCEL : MB_OK));
}

/********************************************************************************
MBError - Displays a message box containing an error.

inputs
   PWSTR    pszSummary - 1 sentence summary
   PWSTR    pszFinePrint - Detailed information
   BOOL     fCancel - if TRUE, cancel button available in addition to OK
returns
   DWORD - IDOK or IDCANCEL
*/
DWORD CEscPage::MBError (PWSTR pszSummary, PWSTR pszFinePrint, BOOL fCancel)
{
   PWSTR psz;
   PWSTR pszInfo = L"Error";
   psz = pszInfo;
   if (m_TextBlock.m_pNodePageInfo)
      psz = m_TextBlock.m_pNodePageInfo->AttribGet (L"title");
   if (!psz)
      psz = pszInfo;

   return MessageBox (psz, pszSummary, pszFinePrint,
      MB_ICONERROR | (fCancel ? MB_OKCANCEL : MB_OK));
}



// BUGBUG - 2.0 - if set PageInfo LRMargin=50, when the window first pops
// up the left margin is good, but the text goes off the right hand size. WHen
// resize then LRMargin is OK.

// BUGBUG - 2.0 - Use DefControl=true on add project task,
// edit project task, etc. The problem is that it seems to scroll the window so
// that the control is on top the screen. It should only scroll if it
// doesn't have focus NOTE: I think this is untrue, and what was seeing was
// really another problem that occured in Dragonfly


// BUGBUG - 2.0 - When add task (or modify so that ordering somehow
// changed) then refresh tends to get broken near the bottom of the screen and get
// duplication of pixel chunks. This bug also noted by forced Refresh()

// BUGBUG - 2.0 - in project view, tab-order is out of whack. I think this has to do with
// the tables within tables.

