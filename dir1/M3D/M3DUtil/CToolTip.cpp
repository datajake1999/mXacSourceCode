/***************************************************************************
CToolTip - C++ object for viewing the tooltip.

begun 31/8/2001
Copyright 2001 Mike Rozak. All rights reserved
*/

#include <windows.h>
#include "escarpment.h"
#include "..\M3D.h"
#include "resource.h"

#define VISBUF     8

/****************************************************************************
Constructor and destructor
*/
CToolTip::CToolTip (void)
{
   m_hWnd = NULL;
   m_pWindow = NULL;
   m_dwDir = 0;
   memset (&m_rBarrier, 0, sizeof(m_rBarrier));
}


CToolTip::~CToolTip (void)
{
   if (m_pWindow)
      delete m_pWindow;
   m_pWindow = NULL;
}

/******************************************************************************
ToolTipPage - Handle the tool tip.
*/
BOOL ToolTipPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCToolTip pTip = (PCToolTip) pPage->m_pUserData;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         // move the window into the right position
         RECT  r;
         HWND hWnd = pPage->m_pWindow->m_hWnd;
         GetWindowRect (hWnd, &r);
         int   iWidth, iHeight, iLeft, iTop;
         iWidth = r.right - r.left;
         iHeight = r.bottom - r.top;
         switch (pTip->m_dwDir) {
         case 0:  // above
            iLeft = pTip->m_rBarrier.left;
            iTop = pTip->m_rBarrier.top - iHeight - VISBUF;
            break;
         case 1:  // right
            iLeft = pTip->m_rBarrier.right + VISBUF;
            iTop = pTip->m_rBarrier.top;
            break;
         case 2:  // below
            iLeft = pTip->m_rBarrier.left;
            iTop = pTip->m_rBarrier.bottom + VISBUF;
            break;
         case 4:  // above and left
            iLeft = pTip->m_rBarrier.left - iWidth - VISBUF;
            iTop = pTip->m_rBarrier.top - iHeight - VISBUF;
            break;
         case 3:  // left
         default:
            iLeft = pTip->m_rBarrier.left - iWidth - VISBUF;
            iTop = pTip->m_rBarrier.top;
            break;
         }

         // edge of screen
         RECT rw;
         rw.left = rw.top = 0;
         rw.right = GetSystemMetrics (SM_CXVIRTUALSCREEN);
         rw.bottom = GetSystemMetrics (SM_CYVIRTUALSCREEN);
         if (rw.right < 1)
            rw.right = GetSystemMetrics (SM_CXSCREEN);
         if (rw.bottom < 1)
            rw.bottom = GetSystemMetrics (SM_CYSCREEN);
         //GetWindowRect (hWndParent, &rw);
         iLeft = max(iLeft, rw.left);
         iTop = max(iTop, rw.top);
         if (iLeft + iWidth > rw.right)
            iLeft = rw.right - iWidth;
         if (iTop + iHeight > rw.bottom)
            iTop = rw.bottom - iHeight;

         MoveWindow (hWnd, iLeft, iTop, iWidth, iHeight, TRUE);

         ShowWindow (hWnd, SW_SHOWNOACTIVATE);
      }
      break;
   }

   return FALSE;
}

   

/****************************************************************************8
CToolTip::Init - Creates a tooltip window so that it doesn't overlap the given
rectangle.

inputs
   char     *psz - tooltip string. Note: This is MML
   DWORD    dwDir - 0 is above, 1 is to the right, 2 is below, 3 is to the left, 4=above and left
   RECT     *pr - rectangle
   HWND     hWndParent - parent of this window since will be popup
returns
   HWND - tooltip
*/
BOOL CToolTip::Init (char *psz, DWORD dwDir, RECT *pr, HWND hWndParent)
{
   if (m_pWindow)
      return FALSE;

   // copy over params
   m_dwDir = dwDir;
   m_rBarrier = *pr;
   if (!m_memTip.Required (strlen(psz)+1))
      return FALSE;
   strcpy ((char*)m_memTip.p, psz);


   // create the window
   m_pWindow = new CEscWindow;
   if (!m_pWindow)
      return FALSE;
   RECT r;
   r.top = r.left = 0;
   r.right = GetSystemMetrics (SM_CYSCREEN) / 6;   // BUGFIX - Was / 4
   r.bottom = 1000;
   if (!m_pWindow->Init (ghInstance, hWndParent,
      EWS_NOTITLE | EWS_FIXEDSIZE | EWS_NOVSCROLL | EWS_AUTOHEIGHT | EWS_HIDE, &r))
      return FALSE;
   
   // create the string
   MemZero (&gMemTemp);
   // BUGFIX - Take out one of the smalls
   MemCat (&gMemTemp, L"<pageinfo lrmargin=4 tbmargin=4/><colorblend posn=background tcolor=#e0ffe0 bcolor=#c0e0c0/><small>");
   CMem memANSI;
   if (!memANSI.Required (strlen(psz)*2 + 2))
      return FALSE;
   MultiByteToWideChar (CP_ACP, 0, psz, -1, (PWSTR) memANSI.p, (DWORD)memANSI.m_dwAllocated);
   MemCat (&gMemTemp, (PWSTR) memANSI.p); // NOTE: Not sanitizing
   MemCat (&gMemTemp, L"</small>");

   // create the page
   if (!m_pWindow->PageDisplay (ghInstance, (PWSTR) gMemTemp.p, ToolTipPage, this))
      return FALSE;

   m_hWnd = m_pWindow->m_hWnd;

   return TRUE;
}

