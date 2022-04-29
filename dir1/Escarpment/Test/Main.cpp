/**************************************************************************
Main.cpp - Used for test purposes to test out librayr.
*/

#include <windows.h>
#include <stdlib.h>
#include <stdio.h>
#include <escarpment.h>
#include "resource.h"

/* globals */
HINSTANCE      ghInstance;


/***********************************************************************
Page callback
*/
BOOL MyPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   switch (dwMessage) {
   case ESCN_SCROLL:
   case ESCN_SCROLLING:
      {
         ESCNSCROLL *p = (ESCNSCROLL*) pParam;
         WCHAR  szTemp[256];
         swprintf (szTemp, L"<null>Posn = <big>%d</big></null>", p->iPos);

         PCEscControl   pc;
         pc = pPage->ControlFind (L"status");
         if (pc) {
            ESCMSTATUSTEXT st;
            memset (&st, 0, sizeof(st));
            st.pszMML = szTemp;
            pc->Message (ESCM_STATUSTEXT, &st);
         }

         pc = pPage->ControlFind (L"p1");
         swprintf (szTemp, L"%d", p->iPos);
         if (pc)
            pc->AttribSet (L"pos", szTemp);
         pc = pPage->ControlFind (L"p2");
         swprintf (szTemp, L"%d", p->iPos);
         if (pc)
            pc->AttribSet (L"pos", szTemp);
      }
      return TRUE;

   case ESCN_COMBOBOXSELCHANGE:
      {
         PESCNCOMBOBOXSELCHANGE p = (PESCNCOMBOBOXSELCHANGE) pParam;
         WCHAR szShort[] = L"This  is a short message for you to check out to see what you want.";
         WCHAR szLong[] = L" All work and no play makes jack a dull boy. All work and no play makes jack a dull boy. All work and no play makes jack a dull boy.\nAll work and no play makes jack a dull boy. All work and no play makes jack a dull boy.";
         
         // else just display
         switch (p->dwCurSel % 4) {
         case 0:
            pPage->MBInformation (szShort);
            break;
         case 1:
            pPage->MBYesNo (szShort, szLong, TRUE);
            break;
         case 2:
            pPage->MBWarning (szShort, szLong, TRUE);
            break;
         case 3:
            pPage->MBError (szShort, szLong, TRUE);
            break;
         }

      }
      return TRUE;

   }
   return FALSE;
}

int WINAPI WinMain (HINSTANCE hInstance, HINSTANCE hPrevInstance,
                    LPSTR lpCmdLine, int nShowCmd)
{
   ghInstance = hInstance;

   CEscWindow  cWindow;
   
   WCHAR *psz;
   cWindow.Init (hInstance);
   psz = cWindow.PageDialog (IDR_MMLTEST, MyPage);

   // done
   return 0;
}