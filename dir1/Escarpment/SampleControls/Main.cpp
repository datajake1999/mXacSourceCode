/********************************************************************
Main.cpp - Sample that shows you how to write your own custom controls.
It includes the source code for the controls from Escarpment.dll.

begun 5/11/2000 by Mike Rozak
Copyrigh 2000 by Mike Rozak. All rights reserved
*/
#include <windows.h>
#include <escarpment.h>
#include "resource.h"

HINSTANCE   ghInstance;

// controls from various files
BOOL ControlColorBlend (PCEscControl pControl, DWORD dwMessage, PVOID pParam);
BOOL ControlImage (PCEscControl pControl, DWORD dwMessage, PVOID pParam);
BOOL ControlLink (PCEscControl pControl, DWORD dwMessage, PVOID pParam);
BOOL ControlHorizontalLine (PCEscControl pControl, DWORD dwMessage, PVOID pParam);
BOOL ControlButton (PCEscControl pControl, DWORD dwMessage, PVOID pParam);
BOOL ControlScrollBar (PCEscControl pControl, DWORD dwMessage, PVOID pParam);
BOOL ControlStatus (PCEscControl pControl, DWORD dwMessage, PVOID pParam);
BOOL ControlThreeD (PCEscControl pControl, DWORD dwMessage, PVOID pParam);
BOOL ControlProgressBar (PCEscControl pControl, DWORD dwMessage, PVOID pParam);
BOOL ControlEdit (PCEscControl pControl, DWORD dwMessage, PVOID pParam);
BOOL ControlListBox (PCEscControl pControl, DWORD dwMessage, PVOID pParam);
BOOL ControlMenu (PCEscControl pControl, DWORD dwMessage, PVOID pParam);
BOOL ControlComboBox (PCEscControl pControl, DWORD dwMessage, PVOID pParam);




int WINAPI WinMain (HINSTANCE hInstance, HINSTANCE hPrevInstance,
                    LPSTR lpCmdLine, int nShowCmd)
{
   ghInstance = hInstance;

   // Note: you would replace "sample" and "42" with your E-mail and the
   // registration key sent to you
   EscInitialize(L"sample", 42, 0);


   // tell escarpment about the controls
   EscControlAdd (L"MyImage", ControlImage);
   EscControlAdd (L"MyColorBlend", ControlColorBlend);
   EscControlAdd (L"MyHR", ControlHorizontalLine);
   EscControlAdd (L"MyButton", ControlButton);
   EscControlAdd (L"MyScrollBar", ControlScrollBar);
   EscControlAdd (L"MyStatus", ControlStatus);
   EscControlAdd (L"MyThreeD", ControlThreeD);
   EscControlAdd (L"MyProgressBar", ControlProgressBar);
   EscControlAdd (L"MyEdit", ControlEdit);
   EscControlAdd (L"MyListBox", ControlListBox);
   EscControlAdd (L"MyMenu", ControlMenu);
   EscControlAdd (L"MyComboBox", ControlComboBox);


   CEscWindow  cWindow;
   
   WCHAR *psz;
   cWindow.Init (hInstance);
   psz = cWindow.PageDialog (IDR_MAIN, NULL);

   EscUninitialize();

   // done
   return 0;
}

