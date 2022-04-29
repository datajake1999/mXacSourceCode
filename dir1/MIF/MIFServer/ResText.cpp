/*************************************************************************************
ResText.cpp - Code for the MML text resource.

begun 22/5/04 by Mike Rozak.
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

/***********************************************************************
TestPage callback - All it really does is store the error info away
*/
BOOL TestPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   switch (dwMessage) {
   case ESCM_INITPAGE:
      // add am accelerator for escape just in case there's no title bar
      // as set by preferences
      ESCACCELERATOR a;
      memset (&a, 0, sizeof(a));
      a.c = VK_ESCAPE;
      a.dwMessage = ESCM_CLOSE;
      pPage->m_listESCACCELERATOR.Add (&a);
      return TRUE;

   case ESCM_INTERPRETERROR:
      {
         PESCMINTERPRETERROR p = (PESCMINTERPRETERROR) pParam;

         gdwErrorNum = p->pError->m_dwNum;
         gMemErrorString.Required ((wcslen(p->pError->m_pszDesc)+1)*2);
         wcscpy ((PWSTR)gMemErrorString.p, p->pError->m_pszDesc);

         if (p->pError->m_pszSurround) {
            gMemErrorSurround.Required ((wcslen(p->pError->m_pszSurround)+1)*2);
            wcscpy ((PWSTR) gMemErrorSurround.p, p->pError->m_pszSurround);
            gdwErrorSurroundChar = p->pError->m_dwSurroundChar;
         }
         else {
            gdwErrorSurroundChar = (DWORD)-1;
         }
      }
      return TRUE;
   }
   return FALSE;
}



/*************************************************************************
ResTextPage
*/
BOOL ResTextPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
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

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;
         PWSTR psz;
         psz = p->pControl->m_pszName;

         if (!_wcsicmp(psz, L"testmml")) {
            MemZero (&gMemErrorString);
            MemZero (&gMemErrorSurround);
            MemZero (&gMemErrorSurroundChar);

            // try to compile this
            WCHAR *psz;
            CMem memRet;
            {
               CEscWindow  cWindow;

               cWindow.Init (ghInstance, pPage->m_pWindow->m_hWnd, 0);
               psz = cWindow.PageDialog (ghInstance, (PWSTR) potd->pText->p, TestPage);
               if (psz) {
                  MemZero (&memRet);
                  MemCat (&memRet, psz);
                  psz = (PWSTR)memRet.p;
               }
            }
            
            // if return string then report that
            if (psz) {
               CMem memTemp;
               MemZero (&memTemp);
               MemCat (&memTemp, L"The page returned, ");
               MemCat (&memTemp, psz);
               pPage->MBSpeakInformation ((PWSTR)memTemp.p);
            }
            else {
               // error
               pPage->MBError (L"The compile failed.",
                  gMemErrorString.p ? (PWSTR) gMemErrorString.p : L"No reason given.");

               // see if can find the location of the error and set the caret there
               PWSTR pszErr;
               pszErr = (PWSTR) gMemErrorSurround.p;
               if ((gdwErrorSurroundChar != (DWORD)-1) && pszErr) {
                  PWSTR pszFind, pszFind2;
                  PWSTR pszSrc = (WCHAR*) potd->pText->p;
                  pszFind = wcsstr (pszSrc, pszErr);

                  // keep on looking for the last occurance
                  pszFind2 = pszFind;
                  while (pszFind2) {
                     pszFind2 = wcsstr (pszFind+1, pszErr);
                     if (pszFind2)
                        pszFind = pszFind2;
                  }

                  if (pszFind) {
                     PCEscControl pc = pPage->ControlFind (L"desclong");

                     // found it, set the attribute
                     WCHAR szTemp[16];
                     swprintf (szTemp, L"%d", ((PBYTE) pszFind - (PBYTE) pszSrc)/2 + gdwErrorSurroundChar);
                     pc->AttribSet (L"selstart", szTemp);
                     pc->AttribSet (L"selend", szTemp);
                     pc->Message (ESCM_EDITSCROLLCARET);
                     pPage->FocusSet (pc);   // BUGFIX - set focus
                  }
               }
            }

            return TRUE;
         }
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Text resource";
            return TRUE;
         }
      }
      break;

   }; // dwMessage

   return FALSE;
}


static PWSTR gpszOrigText = L"OrigText";
static PWSTR gpszMML = L"MML";
static PWSTR gpszLangID = L"LangID";

/*************************************************************************
ResTextEdit - Modify a resource Text. Uses standard API from mifl.h, ResourceEdit.
*/
PCMMLNode2 ResTextEdit (HWND hWnd, LANGID lid, PCMMLNode2 pIn, BOOL fReadOnly)
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
   psz = pIn ? MMLValueGet (pIn, gpszOrigText) : NULL;
   if (psz)
      MemCat (&memText, psz);

   // create the window
   RECT r;
   DialogBoxLocation3 (GetDesktopWindow(), &r, TRUE);
   CEscWindow Window;
   Window.Init (ghInstance, hWnd, 0, &r);
redo:
   psz = Window.PageDialog (ghInstance, IDR_MMLRESTEXT, ResTextPage, &otd);
   if (psz && !_wcsicmp(psz, RedoSamePage()))
      goto redo;

   if (!otd.fChanged)
      goto done;

   // create new MML
   PCMMLNode2 pNode = new CMMLNode2;
   if (!pNode)
      goto done;
   pNode->NameSet (CircumrealityText());
   if (((PWSTR)memText.p)[0])  // BUGFIX - So dont send empty string
      MMLValueSet (pNode, gpszOrigText, (PWSTR) memText.p);
   MMLValueSet (pNode, gpszLangID, (int)lid);

   PCMMLNode pSub1 = ParseMML ((PWSTR)memText.p, ghInstance, NULL, NULL, &err, FALSE);
   PCMMLNode2 pSub = pSub1 ? pSub1->CloneAsCMMLNode2() : NULL;
   if (pSub1)
      delete pSub1;
   if (pSub) {
      pSub->NameSet (gpszMML);
      pNode->ContentAdd (pSub);
   }

   pRet = pNode;

done:
   return pRet;
}

