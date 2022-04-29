/***********************************************************************
ControlLink.cpp - Code for a a link

begun 3/31/2000 by Mike Rozak
Copyright 2000 mike Rozak. All rights reserved
*/

#include <windows.h>
#include <stdlib.h>
#include "escarpment.h"
#include "resleak.h"

/***********************************************************************
Control callback
*/
BOOL ControlLink (PCEscControl pControl, DWORD dwMessage, PVOID pParam)
{
   switch (dwMessage) {
   case ESCM_INITCONTROL:
      {
         // if this has a href then want mouse
         // always want mouse because might be hover help
         //pControl->m_fWantMouse = pControl->m_pNode->AttribGet(L"href") ? TRUE : FALSE;
         //if (pControl->m_fWantMouse)
         //   pControl->m_dwWantFocus = 1;
         pControl->m_fWantMouse = TRUE;
         pControl->m_dwWantFocus = 1;

         // secify that accept space or enter
         if (pControl->m_dwWantFocus) {
            ESCACCELERATOR a;
            memset (&a, 0, sizeof(a));
            a.c = L' ';
            a.dwMessage = ESCM_SWITCHACCEL;
            pControl->m_listAccelFocus.Add (&a);
            a.c = L'\n';
            pControl->m_listAccelFocus.Add (&a);
         }
      }
      return TRUE;


   case ESCM_LBUTTONDOWN:
      {
         WCHAR *psz;
         psz = (WCHAR*) pControl->m_treeAttrib.Find (L"href");
         if (psz) {
            pControl->m_pParentPage->m_pWindow->Beep(ESCBEEP_LINKCLICK);

            // must release capture or bad things happen
            pControl->m_pParentPage->MouseCaptureRelease(pControl);

            pControl->m_pParentPage->Link (psz);
         }
         else
            return FALSE;  // let it beep
      }
      return TRUE;

   case ESCM_PAINT:
      // purposely draw nothing so it doesn't overlap the text
      return TRUE;

   }

   return FALSE;
}

