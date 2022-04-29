/************************************************************************
CButtonBar - Class which draws a bunch of buttons across the screen
and manages them.

begun 31/8/01 by Mike Rozak
Copyright 2001 Mike Rozak. All rights reserved
*/

#include <windows.h>
#include "escarpment.h"
#include "..\M3D.h"
#include "resource.h"


/****************************************************************************
Constructor and destructor
*/
CButtonBar::CButtonBar (void)
{
   m_cBackDark = RGB (0x20,0x20,0x20);
   m_cBackMed = RGB (0x40, 0x40, 0x40);
   m_cBackLight = RGB (0x80, 0x80, 0x80);
   m_cBackOutline = RGB (0, 0, 0);
   m_hWndParent = NULL;
   m_dwDir = 0;
   memset (&m_rect, 0, sizeof(m_rect));
   m_fShow = TRUE;

   DWORD i;
   for (i =0; i < 3; i++)
      m_alPCIconButton[i].Init (sizeof(PCIconButton*));
}

CButtonBar::~CButtonBar (void)
{
   Clear();
}


/****************************************************************************
CButtonBar::ColorSet - Sets the color of the icon
inputs
   COLORREF    cDark, cMed, cLight, cOut - Colors
returns
   none
*/
void CButtonBar::ColorSet (COLORREF cDark, COLORREF cMed, COLORREF cLight, COLORREF cOut)
{
   m_cBackDark = cDark;
   m_cBackMed = cMed;
   m_cBackLight = cLight;
   m_cBackOutline = cOut;

   // show the buttons
   DWORD i, j;
   for (i =0; i < 3; i++) {
      for (j = 0; j < m_alPCIconButton[i].Num(); j++) {
         PCIconButton p = *((PCIconButton*)m_alPCIconButton[i].Get(j));

         p->ColorSet (cDark, cMed, cLight, cOut);
      }
   };
}


/****************************************************************************
Move - Moves the button bar to a new location, which causes all the buttons
within the button bar also to be moved.

inputs
   RECT     *pr - Rectangle to move them to
returns
   none
*/
void CButtonBar::Move (RECT *pr)
{
   // invalidate the old
   InvalidateRect (m_hWndParent, &m_rect, FALSE);

   // store this
   m_rect = *pr;

   // width, height, orientation, unit size
   int   iWidth, iHeight, iSize;
   BOOL  fVert;
   iWidth = pr->right - pr->left;
   iHeight = pr->bottom - pr->top;
   iWidth = max(iWidth, 1);
   iHeight = max(iHeight, 1);
   fVert = (iWidth < iHeight);
   iSize = min(iWidth, iHeight);

   // move the buttons on the top/left
   DWORD i, j;
   for (i =0; i < 3; i++) {
      // XY starting position based on this
      RECT  rButton;
      int   iXInc, iYInc;
      iXInc = iYInc = 0;

      switch (i) {
      case 0:  // left/top
         rButton.left = pr->left;
         rButton.top = pr->top;
         if (fVert)
            iYInc = iSize;
         else
            iXInc = iSize;
         break;

      case 1:  // center
         if (fVert) {
            rButton.left = pr->left;
            rButton.top = (pr->bottom + pr->top) / 2 - m_alPCIconButton[i].Num() * iSize / 2;
            iYInc = iSize;
         }
         else {
            rButton.left = (pr->right + pr->left) / 2 - m_alPCIconButton[i].Num() * iSize / 2;
            rButton.top = pr->top;
            iXInc = iSize;
         }
         break;

      case 2:  // right/bottom
         if (fVert) {
            rButton.left = pr->left;
            rButton.top = pr->bottom - iSize;
            iYInc = -iSize;
         }
         else {
            rButton.left = pr->right - iSize;
            rButton.top = pr->top;
            iXInc = -iSize;
         }
         break;
      }

      rButton.right = rButton.left + iSize;
      rButton.bottom = rButton.top + iSize;

      for (j = 0; j < m_alPCIconButton[i].Num(); j++) {
         PCIconButton p = *((PCIconButton*)m_alPCIconButton[i].Get(j));

         p->Move (&rButton);
         rButton.left += iXInc;
         rButton.right += iXInc;
         rButton.top += iYInc;
         rButton.bottom += iYInc;
      }
   };

   // invalidate the new location
   InvalidateRect (m_hWndParent, &m_rect, FALSE);
}


/****************************************************************************
Show - Shows/hide all the buttons.

inputs
   BOOL     fShow - If TRUE, show them all, else hide
returns
   none
*/
void CButtonBar::Show (BOOL fShow)
{
   m_fShow = fShow;

   // show the buttons
   DWORD i, j;
   for (i =0; i < 3; i++) {
      for (j = 0; j < m_alPCIconButton[i].Num(); j++) {
         PCIconButton p = *((PCIconButton*)m_alPCIconButton[i].Get(j));

         ShowWindow (p->m_hWnd, m_fShow ? SW_SHOWNA : SW_HIDE);
      }
   };
}
/**********************************************************************************
CButtonBar::Init - initializes the button bar

inputs
   RECT     *pr - Rectangle where the bar goes
   HWND     hWndParent - Parent that create the buttons under
   DWORD    dwDir - Direction where tooltips go. 0 = top, 1 = right, 2=down, 3=left
returns
   BOOL - TRUE if successful
*/
BOOL CButtonBar::Init (RECT *pr, HWND hWndParent, DWORD dwDir)
{
   if (m_hWndParent)
      return FALSE;

   m_hWndParent = hWndParent;
   m_rect = *pr;
   m_dwDir = dwDir;

   return TRUE;
}


/**********************************************************************************
CButtonBar::RecalcEndPiece - Recals where the end piece (of the tabs) goes, so the
final line can be drawn.
*/
void CButtonBar::RecalcEndPiece (void)
{
   DWORD i, j;
   for (i = 0; i < 3; i++) {
      DWORD dwNum;
      dwNum = m_alPCIconButton[i].Num();
      for (j = 0; j < dwNum; j++) {
         BOOL fDir = (i < 2) ? TRUE : FALSE;
         if ((m_rect.right - m_rect.left) < (m_rect.bottom - m_rect.top)) {
            // vertical
            fDir = !fDir;
            if (m_rect.left)
               fDir = !fDir;
         }
         else {
            // horizontal
            if (m_rect.top)
               fDir = !fDir;
         }
         PCIconButton p = *((PCIconButton*)m_alPCIconButton[i].Get(
            fDir ? j : (dwNum - j - 1) ));
         PCIconButton pNext = (j+1 < dwNum) ?
            *((PCIconButton*)m_alPCIconButton[i].Get(
            fDir ? (j+1) : (dwNum - (j+1) - 1) )) :
            NULL;

         // is this an end piece
         BOOL fEnd;
         DWORD dwBits;
         dwBits = p->FlagsGet ();
         fEnd = (dwBits & (IBFLAG_DISABLEDARROW | IBFLAG_REDARROW)) ? TRUE : FALSE;
         if (pNext && fEnd) {
            DWORD dwBitsNext = pNext->FlagsGet ();;
            if (dwBitsNext & (IBFLAG_DISABLEDARROW | IBFLAG_REDARROW))
               fEnd = FALSE;
         }

         // does it think it's an end
         BOOL fThink;
         fThink = (dwBits & IBFLAG_ENDPIECE) ? TRUE : FALSE;
         if (fThink != fEnd) {
            if (fEnd)
               dwBits |= IBFLAG_ENDPIECE;
            else
               dwBits &= ~IBFLAG_ENDPIECE;
            p->FlagsSet (dwBits);
         }
      }
   }
}

/**********************************************************************************
CButtonBar::Clear - Erases all the buttons in the button bar

inputs
   DWORD dwGroup - Group to clear, 0..2. Or use 3 to clear them all
*/
void CButtonBar::Clear (DWORD dwGroup)
{
   // delete all the icon buttons
   DWORD i, j;
   for (i = (dwGroup > 2) ? 0 : dwGroup; i < ((dwGroup > 2) ? 3 : (dwGroup+1)); i++) {
      for (j = 0; j < m_alPCIconButton[i].Num(); j++) {
         PCIconButton p = *((PCIconButton*)m_alPCIconButton[i].Get(j));
         delete p;
      };

      m_alPCIconButton[i].Clear();
   }

   // invalidate where the buttons were
   if (m_hWndParent)
      InvalidateRect (m_hWndParent, &m_rect, FALSE);
}


/**********************************************************************************
CButtonBar::AdjustAllButtons - Call this after adding one or more buttons.
It moved all the buttons tot heir right position.
*/
void CButtonBar::AdjustAllButtons (void)
{
   Move (&m_rect);

   RecalcEndPiece();
}

/**********************************************************************************
CButtonBar::ButtonAdd - Adds a button to the button bar.

inputs
   DWORD    dwPosn - 0 is at the top/left, 1 is center, 2 is bottom/right
   DWORD    dwBmpRes - Resource for the image
   HINSTANCE hInstance - Instance to get icon from
   char     *pszName - Name
   char     *pszTip - Tooltip
   DWORD    dwID - button ID
returns
   PCIconButton - button that's added
*/
PCIconButton CButtonBar::ButtonAdd (DWORD dwPosn, DWORD dwBmpRes, HINSTANCE hInstance,
                                    char *pszAccel, char *pszName, char *pszTip, DWORD dwID)
{
   // if out of range exit
   if (dwPosn > 2)
      return NULL;

   PCIconButton pb;
   pb = new CIconButton;
   if (!pb)
      return FALSE;
   RECT r;
   r = m_rect; // temporary
   pb->ColorSet (m_cBackDark, m_cBackMed, m_cBackLight, m_cBackOutline);
   if (!pb->Init (dwBmpRes, hInstance, pszAccel, pszName, pszTip, m_dwDir, &r, m_hWndParent, dwID)) {
      delete pb;
      return FALSE;
   }

   // stick it in the list
   m_alPCIconButton[dwPosn].Add (&pb);

   // if not showing then hide it
   if (!m_fShow)
      ShowWindow (pb->m_hWnd, SW_HIDE);

   // redraw it all
   // BUGFIX - Dont do this now because too slow: Move (&m_rect);

   return pb;
}


/**********************************************************************************
CButtonBar::FlagsSet - Set one flag for a group of buttons within the button
bar, or the entire button bar.

inputs
   DWORD       dwID - Button ID. If a button has this ID it as dwFlag or-ed in
               with its other flags. If the button does not have this ID it has
               dwFlag's bits turned to 0. Use this to set IBFLAG_BLUELIGHT and
               IBFLAG_REDARROW.
   DWORD       dwFlag - Flag to set
   DWORD       dwGroup - Group 0..2 (top, middle, bottom), or 3 for all groups
returns
   none
*/
void CButtonBar::FlagsSet (DWORD dwID, DWORD dwFlag, DWORD dwGroup)
{
   DWORD i,j;
   for (i = (dwGroup > 2) ? 0 : dwGroup; i < ((dwGroup > 2) ? 3 : (dwGroup+1)); i++) {
      for (j = 0; j < m_alPCIconButton[i].Num(); j++) {
         PCIconButton p = *((PCIconButton*)m_alPCIconButton[i].Get(j));

         DWORD dwf;
         dwf = p->FlagsGet();

         if (dwID == p->m_dwID)
            dwf |= dwFlag;
         else
            dwf &= (~dwFlag);

         p->FlagsSet (dwf);
      }
   }
}


/**********************************************************************************
CButtonBar::ButtonExists - Returns true if a button with the given ID exists

inputs
   DWORD       dwID - Button ID.
returns
   BOOL - TRUE if button exists
*/
PCIconButton CButtonBar::ButtonExists (DWORD dwID)
{
   DWORD i,j;
   for (i = 0; i < 3; i++) {
      for (j = 0; j < m_alPCIconButton[i].Num(); j++) {
         PCIconButton p = *((PCIconButton*)m_alPCIconButton[i].Get(j));

         if (dwID != p->m_dwID)
            continue;
         if (!IsWindowEnabled(p->m_hWnd))
            continue;   // dont say yes if disabled
         return p;
      }
   }

   return NULL;
}

/****************************************************************************8
CButtonBar::TestAccelerator - See if this button wants the accelerator

inputs
   DWORD    dwMsg - Message
   WPARAM   wParam
   LPARAM   lParam
returns
   BOOL - TRUE if the button accepts the acclerator and acts on it, false if
         it doesnt want it
*/
BOOL CButtonBar::TestAccelerator (DWORD dwMsg, WPARAM wParam, LPARAM lParam)
{
   DWORD i,j;
   for (i = 0; i < 3; i++) for (j = 0; j < m_alPCIconButton[i].Num(); j++) {
      PCIconButton pb = *((PCIconButton*) m_alPCIconButton[i].Get(j));
      if (pb->TestAccelerator (dwMsg, wParam, lParam))
         return TRUE;
   }
   return FALSE;
}
