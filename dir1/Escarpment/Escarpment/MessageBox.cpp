/***********************************************************************
MessageBox.cpp - Does the mesagebox code.

begun 4/19/2000 by Mike Rozak
Copyright 2000 mike Rozak. All rights reserved
*/

#include "mymalloc.h"
#include <windows.h>
#include <stdlib.h>
#include <stdio.h>
#include "escarpment.h"
#include "mmlparse.h"
#include "resource.h"
#include "resleak.h"

extern HINSTANCE ghInstance;

typedef struct {  // used for substitution callback
   PWSTR       pszTitle;   // title string
   PWSTR       pszTop;      // top color string
   PWSTR       pszBottom;   // bottom color string
   PWSTR       pszSummary; // summary string. No MML processing on it
   PWSTR       pszMMLFinePrint;// MML for fine print
   PWSTR       pszMMLButtons; // MML forbuttons
} MBP, *PMBP;

/***********************************************************************
Page callback
*/
BOOL MessageBoxPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         // add the acclerators
         ESCACCELERATOR a;
         memset (&a, 0, sizeof(a));
         a.c = VK_ESCAPE;
         a.dwMessage = ESCM_CLOSE;
         pPage->m_listESCACCELERATOR.Add (&a);
      }
      return TRUE;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;
         PMBP  pm = (PMBP) pPage->m_pUserData;

         if (!_wcsicmp(p->pszSubName, L"Title"))
            p->pszSubString = pm->pszTitle;
         else if (!_wcsicmp(p->pszSubName, L"Top"))
            p->pszSubString = pm->pszTop;
         else if (!_wcsicmp(p->pszSubName, L"bottom"))
            p->pszSubString = pm->pszBottom;
         else if (!_wcsicmp(p->pszSubName, L"summary"))
            p->pszSubString = pm->pszSummary;
         else if (!_wcsicmp(p->pszSubName, L"mmlfineprint"))
            p->pszSubString = pm->pszMMLFinePrint;
         else if (!_wcsicmp(p->pszSubName, L"mmlbuttons"))
            p->pszSubString = pm->pszMMLButtons;

         p->fMustFree = FALSE;
      }
      return TRUE;
   }
   return FALSE;
}

/***********************************************************************
EscMessageBox - Displays a message using escarpment. You can use this one,
   or can use the ones built into CEscPage.

inputs
   HWND        hWnd - Window to bring on top of
   PWSTR       pszTitle - title for the message box
   PWSTR       pszSummary - 1 sentence summary of message.
   PWSTR       pszFinePrint - Fine print in the message. May be NULL.
   DWORD       dwType - Type. One or more of the following flags:
         : Flag Meaning 
         MB_ABORTRETRYIGNORE The message box contains three push buttons: Abort, Retry, and Ignore. 
         MB_OK The message box contains one push button: OK. This is the default. 
         MB_OKCANCEL The message box contains two push buttons: OK and Cancel. 
         MB_RETRYCANCEL The message box contains two push buttons: Retry and Cancel. 
         MB_YESNO The message box contains two push buttons: Yes and No. 
         MB_YESNOCANCEL The message box contains three push buttons: Yes, No, and Cancel. 

         MB_ICONEXCLAMATION, 
         MB_ICONWARNING
          An exclamation-point icon appears in the message box. 
         MB_ICONINFORMATION, MB_ICONASTERISK
          An icon consisting of a lowercase letter i in a circle appears in the message box. 
         MB_ICONQUESTION A question-mark icon appears in the message box. 
         MB_ICONSTOP, 
         MB_ICONERROR, 
         MB_ICONHAND 
returns
   DWORD - One of the following - Will always return IDCANCEL if user presses ESCAPE key.
      IDABORT Abort button was selected. 
      IDCANCEL Cancel button was selected. 
      IDIGNORE Ignore button was selected. 
      IDNO No button was selected. 
      IDOK OK button was selected. 
      IDRETRY Retry button was selected. 
      IDYES Yes button was selected. 
*/
DWORD EscMessageBox (HWND hWnd, PWSTR pszTitle, PWSTR pszSummary, PWSTR pszFinePrint, DWORD dwType)
{
#if 0
   WCHAR pszMain[] =
      L"<pageinfo title=\"<<<Title>>>\"/>"
      L"<colorblend lcolor=#808090 rcolor=#c0c0d0 posn=background/>"
      L"<table width=100% border=0 innerlines = 0>"
      L"<tr>"
         L"<td width=70% border=2 innerlines=4>"  // need 2x as much for innerlines
            L"<colorblend tcolor=<<<$Top>>> bcolor=<<<$Bottom>>> posn=background/>"
            L"<<<Summary>>>"
            L"<<<$MMLFinePrint>>>"
         L"</td>"
         L"<td width=30%>"
            L"<<<$MMLButtons>>>"
         L"</td>"
      L"</tr>"
      L"</table>";
#endif //0
   WCHAR szMMLFinePrint[] =
      L"<p/>"
      L"<small>%s</small>";

   // need to convert fine print & text into MML compatible
   PWSTR  pszMMLFinePrint;
   pszMMLFinePrint = pszFinePrint ? StringToMMLString(pszFinePrint) : NULL;

   // determine coloration
   COLORREF crTop, crBottom;
   WCHAR szTop[16], szBottom[16];
   DWORD dwChime;
   switch (dwType & 0xf0) {
   case MB_ICONHAND: // error
      crTop = RGB (0xff, 0x80, 0x80);
      crBottom = RGB(0xc0, 0x40, 0x40);
      dwChime = ESCCHIME_ERROR;
      break;
   case MB_ICONQUESTION:
      crTop = RGB (0xf0, 0xf0, 0xf0);
      crBottom = RGB(0xc0, 0xc0, 0xc0);
      dwChime = ESCCHIME_QUESTION;
      break;
   case MB_ICONEXCLAMATION:   // warning
      crTop = RGB (0xc0, 0xff, 0x80);
      crBottom = RGB(0xc0, 0xc0, 0x40);
      dwChime = ESCCHIME_WARNING;
      break;
   default:
   case MB_ICONASTERISK:   // info
      crTop = RGB (0xc0, 0xff, 0xc0);
      crBottom = RGB(0x90, 0xd0, 0x90);
      dwChime = ESCCHIME_INFORMATION;
      break;
   }
   ColorToAttrib (szTop, crTop);
   ColorToAttrib (szBottom, crBottom);


   // fill in based on info
   CMem  memFinePrint, memButtons;
   if (pszMMLFinePrint) {
      if (!memFinePrint.Required ((wcslen(pszMMLFinePrint) + 128)*2)) {
         if (pszMMLFinePrint)
            ESCFREE (pszMMLFinePrint);
         return IDCANCEL;
      }
      swprintf ((WCHAR*) memFinePrint.p, szMMLFinePrint, pszMMLFinePrint);
      ESCFREE (pszMMLFinePrint);
   }

   // buttons
   if (!memButtons.Required (512 * 2)) {
      return IDCANCEL;
   }
   DWORD dw;
   PWSTR pszButtons;
   pszButtons = (WCHAR*) memButtons.p;
   pszButtons[0] = 0;
   dw = dwType & 0x0f;
   if ((dw == MB_OK) || (dw == MB_OKCANCEL))
      wcscat (pszButtons, L"<button href=ok accel=enter>OK</button><br/>");
   if (dw == MB_ABORTRETRYIGNORE)
      wcscat (pszButtons, L"<button href=abort accel=A><u>A</u>bort</button><br/>");
   if ((dw == MB_ABORTRETRYIGNORE) || (dw == MB_RETRYCANCEL))
      wcscat (pszButtons, L"<button href=retry accel=R><u>R</u>etry</button><br/>");
   if ((dw == MB_YESNO) || (dw == MB_YESNOCANCEL)) {
      wcscat (pszButtons, L"<button href=yes accel=Y><u>Y</u>es</button><br/>");
      wcscat (pszButtons, L"<button href=no accel=N><u>N</u>o</button><br/>");
   }
   if ((dw == MB_OKCANCEL) || (dw == MB_YESNOCANCEL) || (dw == MB_RETRYCANCEL))
      wcscat (pszButtons, L"<button href=cancel accel=escape>Cancel</button><br/>");

   // pack this into structure for callback
   MBP   m;
   memset (&m, 0, sizeof(m));
   m.pszBottom = szBottom;
   m.pszMMLButtons = pszButtons;
   m.pszMMLFinePrint = memFinePrint.p ? (WCHAR*) memFinePrint.p : 0;
   m.pszSummary = pszSummary;
   m.pszTitle = pszTitle;
   m.pszTop = szTop;

   // center the message box
   RECT  rPosn;
   int   iX, iY;
   iX = GetSystemMetrics (SM_CXSCREEN);
   iY = GetSystemMetrics (SM_CYSCREEN);
   rPosn.left = iX/ 4;
   rPosn.right = iX - (iX / 4);
   rPosn.top = iY / 3;
   rPosn.bottom = rPosn.top + 1; // iY - (iY / 3);

   // BUGBUG - 2.0 - may want more sophisticated sizing at some point in case theres'
   // a lot of text

   // preload tts before chimes
   EscSpeakTTS ();

   // chimes
   EscChime (dwChime);


   // speak the intro
   if (pszSummary)
      EscSpeak (pszSummary);

   // bring up the window
   CEscWindow  cWindow;
   WCHAR *psz;
   cWindow.Init (ghInstance, hWnd, EWS_NOTITLE | EWS_NOVSCROLL | EWS_FIXEDSIZE | EWS_AUTOHEIGHT, &rPosn);
   psz = cWindow.PageDialog (ghInstance, IDR_MBDEFAULT, MessageBoxPage, &m);
   // IMPORTANT - taking out EWS_TITLE to see how it looks

   if (!psz)
      return IDCANCEL;
   if (!_wcsicmp(psz, L"ok"))
      return IDOK;
   else if (!_wcsicmp(psz, L"cancel"))
      return IDCANCEL;
   else if (!_wcsicmp(psz, L"abort"))
      return IDABORT;
   else if (!_wcsicmp(psz, L"retry"))
      return IDRETRY;
   else if (!_wcsicmp(psz, L"ignore"))
      return IDIGNORE;
   else if (!_wcsicmp(psz, L"yes"))
      return IDYES;
   else if (!_wcsicmp(psz, L"no"))
      return IDNO;
   else
      return IDCANCEL;
}


// BUGBUG - 2.0 - at some point allow people to customize message box

// BUGBUG - 2.0 - if people can customize then dont use ghInstance, since that affects where
// the JPEG images come from

// BUGBUG - 2.0 - at some point have differnet styles of message boxes
