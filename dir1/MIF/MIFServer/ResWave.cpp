/*************************************************************************************
ResWave.cpp - Code for the Wave resource.

begun 24/3/04 by Mike Rozak.
Copyright 2004 by Mike Rozak. All rights reserved
*/

#include <windows.h>
#include <objbase.h>
#include "escarpment.h"
#include "..\..\m3d\M3D.h"
#include "..\..\m3d\mifl.h"
#include "..\mif.h"
#include "..\buildnum.h"
#include "mifserver.h"
#include "resource.h"

// OWD - Information for OpenWaveDialog
typedef struct {
   WCHAR       szFile[256];     // Wave file
   fp          fVolL;            // left vol
   fp          fVolR;            // right vol
   BOOL        fReadOnly;        // set to TRUE if is read only file
   BOOL        fChanged;         // set to TRUE if changed
} OWD, *POWD;




/*************************************************************************
ResWavePage
*/
BOOL ResWavePage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   POWD powd = (POWD)pPage->m_pUserData;   // node to modify

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         PCEscControl pControl;

         // disable?
         if (powd->fReadOnly) {
            if (pControl = pPage->ControlFind (L"open"))
               pControl->Enable (FALSE);
            if (pControl = pPage->ControlFind (L"voll"))
               pControl->Enable (FALSE);
            if (pControl = pPage->ControlFind (L"volr"))
               pControl->Enable (FALSE);
         }

         pControl = pPage->ControlFind (L"file");
         if (pControl)
            pControl->AttribSet (Text(), powd->szFile);

         DoubleToControl (pPage, L"voll", powd->fVolL);
         DoubleToControl (pPage, L"volr", powd->fVolR);
      }
      break;

   case ESCN_EDITCHANGE:
      {
         PESCNEDITCHANGE p = (PESCNEDITCHANGE)pParam;
         PWSTR psz = p->pControl->m_pszName;
         if (!psz)
            break;

         // just get all

         // BUGFIX - get file name too
         PCEscControl pControl = pPage->ControlFind (L"file");
         DWORD dwNeed;
         if (pControl)
            pControl->AttribGet (Text(), powd->szFile, sizeof(powd->szFile), &dwNeed);

         powd->fVolL = DoubleFromControl (pPage, L"voll");
         powd->fVolR = DoubleFromControl (pPage, L"volr");
         powd->fChanged = TRUE;
         return TRUE;
      }
      break;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;
         PWSTR psz;
         psz = p->pControl->m_pszName;

         if (!_wcsicmp(psz, L"open")) {
            if (!WaveFileOpen (pPage->m_pWindow->m_hWnd, FALSE, powd->szFile))
               return TRUE;

            powd->fChanged = TRUE;
            PCEscControl pControl = pPage->ControlFind (L"file");
            if (pControl)
               pControl->AttribSet (Text(), powd->szFile);
            return TRUE;
         }
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Wave resource";
            return TRUE;
         }
      }
      break;

   }; // dwMessage

   return FALSE;
}


static PWSTR gpszFile = L"file";
static PWSTR gpszVolL = L"voll";
static PWSTR gpszVolR = L"volr";

/*************************************************************************
ResWaveEdit - Modify a resource Wave. Uses standard API from mifl.h, ResourceEdit.
*/
PCMMLNode2 ResWaveEdit (HWND hWnd, LANGID lid, PCMMLNode2 pIn, BOOL fReadOnly)
{
   PCMMLNode2 pRet = NULL;
   OWD owd;
   memset (&owd, 0, sizeof(owd));
   owd.fReadOnly = fReadOnly;

   PWSTR psz;
   psz = pIn ? MMLValueGet (pIn, gpszFile) : NULL;
   if (psz)
      wcscpy (owd.szFile, psz);
   if (pIn) {
      owd.fVolL = MMLValueGetDouble (pIn, gpszVolL, 1);
      owd.fVolR = MMLValueGetDouble (pIn, gpszVolR, 1);
   }
   else
      owd.fVolL = owd.fVolR = 1;

   // create the window
   RECT r;
   DialogBoxLocation3 (GetDesktopWindow(), &r, TRUE);
   CEscWindow Window;
   Window.Init (ghInstance, hWnd, 0, &r);
redo:
   psz = Window.PageDialog (ghInstance, IDR_MMLRESWAVE, ResWavePage, &owd);
   if (psz && !_wcsicmp(psz, RedoSamePage()))
      goto redo;

   if (!owd.fChanged)
      goto done;

   // create new MML
   PCMMLNode2 pNode = new CMMLNode2;
   if (!pNode)
      goto done;
   pNode->NameSet (CircumrealityWave());
   if (owd.szFile[0])
      MMLValueSet (pNode, gpszFile, owd.szFile);
   if (owd.fVolL != 1)
      MMLValueSet (pNode, gpszVolL, owd.fVolL);
   if (owd.fVolR != 1)
      MMLValueSet (pNode, gpszVolR, owd.fVolR);

   pRet = pNode;

done:
   return pRet;
}

