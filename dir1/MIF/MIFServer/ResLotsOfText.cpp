/*************************************************************************************
ResLotsOfText.cpp - Code for the MML text resource.

begun 18/6/09 by Mike Rozak.
Copyright 2009 by Mike Rozak. All rights reserved
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

// OTD - Information for OpenTextDialog
typedef struct {
   PCMem       pText;            // memory with the text
   BOOL        fReadOnly;        // set to TRUE if is read only file
   BOOL        fChanged;         // set to TRUE if changed
} OTD, *POTD;

// globals used by test page
static DWORD gdwErrorNum = 0;
static DWORD gdwErrorSurroundChar = 0;
static CMem gMemErrorString;
static CMem gMemErrorSurround;
static CMem gMemErrorSurroundChar;

/*************************************************************************
ResLotsOfTextPage
*/
BOOL ResLotsOfTextPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   POTD potd = (POTD)pPage->m_pUserData;   // node to modify

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         PCEscControl pControl;

         pControl = pPage->ControlFind (L"desclong");
         if (pControl) {
            pControl->AttribSet (Text(), (PWSTR)potd->pText->p);
            if (potd->fReadOnly)
               pControl->AttribSetBOOL (L"readonly", TRUE);
         }

         // disable?
         if (potd->fReadOnly) {
            if (pControl = pPage->ControlFind (L"testmml"))
               pControl->Enable (FALSE);
         }
      }
      break;

   case ESCN_EDITCHANGE:
      {
         PESCNEDITCHANGE p = (PESCNEDITCHANGE)pParam;
         PWSTR psz = p->pControl->m_pszName;
         if (!psz)
            break;

         // how big is this?
         DWORD dwNeed = 0;
         p->pControl->AttribGet (Text(), NULL, 0, &dwNeed);

         PCMem pMem = NULL;

         if (!_wcsicmp(psz, L"desclong"))
            pMem = potd->pText;

         if (pMem) {
            if (!pMem->Required (dwNeed))
               return FALSE;

            p->pControl->AttribGet (Text(), (PWSTR)pMem->p, (DWORD)pMem->m_dwAllocated, &dwNeed);
            potd->fChanged = TRUE;

            return TRUE;
         }
      }
      break;


   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Lots-of-Text resource";
            return TRUE;
         }
      }
      break;

   }; // dwMessage

   return FALSE;
}


// static PWSTR gpszOrigText = L"OrigText";
// static PWSTR gpszMML = L"MML";
// static PWSTR gpszLangID = L"LangID";
static PWSTR gpszTextMML = L"Text";

/*************************************************************************
ResLotsOfTextEdit - Modify a resource LotsOfText. Uses standard API from mifl.h, ResourceEdit.
*/
PCMMLNode2 ResLotsOfTextEdit (HWND hWnd, LANGID lid, PCMMLNode2 pIn, BOOL fReadOnly)
{
   CEscError err;
   PCMMLNode2 pRet = NULL;
   CMem memText;
   OTD otd;
   memset (&otd, 0, sizeof(otd));
   otd.fReadOnly = fReadOnly;
   otd.pText = &memText;

   // get the original text...
   PWSTR psz;
   MemZero (&memText);
   PCMMLNode2 pSub;
   if (pIn && (pIn->ContentNum() == 1)) {
      pIn->ContentEnum (0, &psz, &pSub);
      if (psz) {
         MemZero (&memText);
         MemCat (&memText, psz);
      }
   }

   // create the window
   RECT r;
   DialogBoxLocation3 (GetDesktopWindow(), &r, TRUE);
   CEscWindow Window;
   Window.Init (ghInstance, hWnd, 0, &r);
redo:
   psz = Window.PageDialog (ghInstance, IDR_MMLRESLOTSOFTEXT, ResLotsOfTextPage, &otd);
   if (psz && !_wcsicmp(psz, RedoSamePage()))
      goto redo;

   if (!otd.fChanged)
      goto done;

   // create new MML
   PCMMLNode2 pNode = new CMMLNode2;
   if (!pNode)
      goto done;
   pNode->NameSet (CircumrealityLotsOfText());
   if (((PWSTR)memText.p)[0])  // BUGFIX - So dont send empty string
      pNode->ContentAdd ((PWSTR) memText.p);

   pRet = pNode;

done:
   return pRet;
}

