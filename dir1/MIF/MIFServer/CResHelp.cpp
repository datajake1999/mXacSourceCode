/*************************************************************************************
CResHelp.cpp - Code for the MML help resource.

begun 24/11/04 by Mike Rozak.
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




/*************************************************************************************
CResHelp::Constructor and destructor
*/
CResHelp::CResHelp (void)
{
   MemZero (&m_memName);
   MemZero (&m_memDescShort);
   MemZero (&m_memDescLong);
   MemZero (&m_memFunction);
   MemZero (&m_memFunctionParam);
   MemZero (&m_memBook);
   MemZero (&m_memAsResource);

   m_lid = 1033;  // default

   DWORD i;
   for (i = 0; i < 2; i++)
      MemZero (&m_aMemHelp[i]);
}

CResHelp::~CResHelp (void)
{
   // do nothing for now
}


static PWSTR gpszOrigText = L"OrigText";
static PWSTR gpszMML = L"MML";
static PWSTR gpszLangID = L"LangID";
static PWSTR gpszDescShort = L"DescShort";
static PWSTR gpszFunction = L"Function";
static PWSTR gpszFunctionParam = L"FunctionParam";
static PWSTR gpszName = L"Name";
static PWSTR gpszBook = L"Book";

/*************************************************************************************
CResHelp::MMLTo - Standard API
*/
PCMMLNode2 CResHelp::MMLTo (void)
{
   CEscError err;
   PCMMLNode2 pNode = new CMMLNode2;
   if (!pNode)
      return NULL;
   pNode->NameSet (CircumrealityHelp());

   if (((PWSTR)m_memDescLong.p)[0])
      MMLValueSet (pNode, gpszOrigText, (PWSTR) m_memDescLong.p);
   MMLValueSet (pNode, gpszLangID, (int)m_lid);

   if (((PWSTR)m_memName.p)[0])
      MMLValueSet (pNode, gpszName, (PWSTR) m_memName.p);
   if (((PWSTR)m_memDescShort.p)[0])
      MMLValueSet (pNode, gpszDescShort, (PWSTR) m_memDescShort.p);
   if (((PWSTR)m_memFunction.p)[0])
      MMLValueSet (pNode, gpszFunction, (PWSTR) m_memFunction.p);
   if (((PWSTR)m_memFunctionParam.p)[0])
      MMLValueSet (pNode, gpszFunctionParam, (PWSTR) m_memFunctionParam.p);
   if (((PWSTR)m_memBook.p)[0])
      MMLValueSet (pNode, gpszBook, (PWSTR) m_memBook.p);

   DWORD i;
   WCHAR szTemp[64];
   for (i = 0; i < 2; i++) {
      swprintf (szTemp, L"Help%d", (int)i);
      if (((PWSTR)m_aMemHelp[i].p)[0])
         MMLValueSet (pNode, szTemp, (PWSTR) m_aMemHelp[i].p);
   } // i

   PCMMLNode pSub1 = ParseMML ((PWSTR)m_memDescLong.p, ghInstance, NULL, NULL, &err, FALSE);
   PCMMLNode2 pSub = pSub1 ? pSub1->CloneAsCMMLNode2() : NULL;
   if (pSub1)
      delete pSub1;
   if (pSub) {
      pSub->NameSet (gpszMML);
      pNode->ContentAdd (pSub);
   }

   return pNode;
}

/*************************************************************************************
CResHelp::MMLFrom - Standard API

inputs
   LANGID         lid - Language to use if none is found
*/
BOOL CResHelp::MMLFrom (PCMMLNode2 pNode, LANGID lid)
{
   // wipe out what already have
   MemZero (&m_memName);
   MemZero (&m_memDescShort);
   MemZero (&m_memDescLong);
   MemZero (&m_memFunction);
   MemZero (&m_memFunctionParam);
   MemZero (&m_memBook);
   MemZero (&m_memAsResource);

   m_lid = lid;  // default

   DWORD i;
   for (i = 0; i < 2; i++)
      MemZero (&m_aMemHelp[i]);


   // get values
   PWSTR psz;

   psz = MMLValueGet (pNode, gpszOrigText);
   if (psz)
      MemCat (&m_memDescLong, psz);

   psz = MMLValueGet (pNode, gpszName);
   if (psz)
      MemCat (&m_memName, psz);

   psz = MMLValueGet (pNode, gpszDescShort);
   if (psz)
      MemCat (&m_memDescShort, psz);

   psz = MMLValueGet (pNode, gpszFunction);
   if (psz)
      MemCat (&m_memFunction, psz);

   psz = MMLValueGet (pNode, gpszFunctionParam);
   if (psz)
      MemCat (&m_memFunctionParam, psz);

   psz = MMLValueGet (pNode, gpszBook);
   if (psz)
      MemCat (&m_memBook, psz);

   WCHAR szTemp[64];
   for (i = 0; i < 2; i++) {
      swprintf (szTemp, L"Help%d", (int)i);
      psz = MMLValueGet (pNode, szTemp);
      if (psz)
         MemCat (&m_aMemHelp[i], psz);
   } // i

   m_lid = (LANGID)MMLValueGetInt (pNode, gpszLangID, (int)lid);

   return TRUE;
}






// OHD - Information for OpenTextDialog
typedef struct {
   PCResHelp   pResHelp;         // help information
   BOOL        fReadOnly;        // set to TRUE if is read only file
   BOOL        fChanged;         // set to TRUE if changed
} OHD, *POHD;

// globals used by test page
static DWORD gdwErrorNum = 0;
static DWORD gdwErrorSurroundChar = 0;
static CMem gMemErrorString;
static CMem gMemErrorSurround;
static CMem gMemErrorSurroundChar;

/***********************************************************************
TestPage callback - All it really does is store the error info away
*/
static BOOL TestPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
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
ResHelpPage
*/
BOOL ResHelpPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   POHD pohd = (POHD)pPage->m_pUserData;   // node to modify

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         PCEscControl pControl;

         pControl = pPage->ControlFind (L"desclong");
         if (pControl) {
            pControl->AttribSet (Text(), (PWSTR)pohd->pResHelp->m_memDescLong.p);
            if (pohd->fReadOnly)
               pControl->AttribSetBOOL (L"readonly", TRUE);
         }

         pControl = pPage->ControlFind (L"descshort");
         if (pControl) {
            pControl->AttribSet (Text(), (PWSTR)pohd->pResHelp->m_memDescShort.p);
            if (pohd->fReadOnly)
               pControl->AttribSetBOOL (L"readonly", TRUE);
         }

         pControl = pPage->ControlFind (L"name");
         if (pControl) {
            pControl->AttribSet (Text(), (PWSTR)pohd->pResHelp->m_memName.p);
            if (pohd->fReadOnly)
               pControl->AttribSetBOOL (L"readonly", TRUE);
         }

         pControl = pPage->ControlFind (L"function");
         if (pControl) {
            pControl->AttribSet (Text(), (PWSTR)pohd->pResHelp->m_memFunction.p);
            if (pohd->fReadOnly)
               pControl->AttribSetBOOL (L"readonly", TRUE);
         }

         pControl = pPage->ControlFind (L"functionparam");
         if (pControl) {
            pControl->AttribSet (Text(), (PWSTR)pohd->pResHelp->m_memFunctionParam.p);
            if (pohd->fReadOnly)
               pControl->AttribSetBOOL (L"readonly", TRUE);
         }

         pControl = pPage->ControlFind (L"book");
         if (pControl) {
            pControl->AttribSet (Text(), (PWSTR)pohd->pResHelp->m_memBook.p);
            if (pohd->fReadOnly)
               pControl->AttribSetBOOL (L"readonly", TRUE);
         }

         pControl = pPage->ControlFind (L"helpcat0");
         if (pControl) {
            pControl->AttribSet (Text(), (PWSTR)pohd->pResHelp->m_aMemHelp[0].p);
            if (pohd->fReadOnly)
               pControl->AttribSetBOOL (L"readonly", TRUE);
         }

         pControl = pPage->ControlFind (L"helpcat1");
         if (pControl) {
            pControl->AttribSet (Text(), (PWSTR)pohd->pResHelp->m_aMemHelp[1].p);
            if (pohd->fReadOnly)
               pControl->AttribSetBOOL (L"readonly", TRUE);
         }

         // disable?
         // BUGFIX - Dont do this
         //if (pohd->fReadOnly) {
         //   if (pControl = pPage->ControlFind (L"testmml"))
         //      pControl->Enable (FALSE);
         //}
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
            pMem = &pohd->pResHelp->m_memDescLong;
         else if (!_wcsicmp(psz, L"descshort"))
            pMem = &pohd->pResHelp->m_memDescShort;
         else if (!_wcsicmp(psz, L"name"))
            pMem = &pohd->pResHelp->m_memName;
         else if (!_wcsicmp(psz, L"helpcat0"))
            pMem = &pohd->pResHelp->m_aMemHelp[0];
         else if (!_wcsicmp(psz, L"helpcat1"))
            pMem = &pohd->pResHelp->m_aMemHelp[1];
         else if (!_wcsicmp(psz, L"function"))
            pMem = &pohd->pResHelp->m_memFunction;
         else if (!_wcsicmp(psz, L"functionparam"))
            pMem = &pohd->pResHelp->m_memFunctionParam;
         else if (!_wcsicmp(psz, L"book"))
               pMem = &pohd->pResHelp->m_memBook;

         if (pMem) {
            if (!pMem->Required (dwNeed))
               return FALSE;

            p->pControl->AttribGet (Text(), (PWSTR)pMem->p, (DWORD)pMem->m_dwAllocated, &dwNeed);
            pohd->fChanged = TRUE;

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

            // will need to prefix with right bits so black background
            CMem memAll;
            MemZero (&memAll);
            MemCat (&memAll, ResHelpStringPrefix(TRUE));
            MemCat (&memAll, (PWSTR) pohd->pResHelp->m_memDescLong.p);
            MemCat (&memAll, ResHelpStringSuffix());

            // try to compile this
            WCHAR *psz;
            CMem memRet;
            {
               CEscWindow  cWindow;

               cWindow.Init (ghInstance, pPage->m_pWindow->m_hWnd, 0);
               psz = cWindow.PageDialog (ghInstance, (PWSTR) memAll.p, TestPage);
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
                  PWSTR pszSrc = (WCHAR*) memAll.p;
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
                     swprintf (szTemp, L"%d", ((PBYTE) pszFind - (PBYTE) pszSrc)/2 -
                        wcslen(ResHelpStringPrefix(TRUE)) + gdwErrorSurroundChar);
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
            p->pszSubString = L"Help resource";
            return TRUE;
         }
      }
      break;

   }; // dwMessage

   return FALSE;
}



/*************************************************************************
ResHelpEdit - Modify a resource Text. Uses standard API from mifl.h, ResourceEdit.
*/
PCMMLNode2 ResHelpEdit (HWND hWnd, LANGID lid, PCMMLNode2 pIn, BOOL fReadOnly)
{
   CResHelp rh;
   rh.m_lid = lid;
   if (pIn && !rh.MMLFrom (pIn, lid))
      return NULL;

   OHD ohd;
   memset (&ohd, 0, sizeof(ohd));
   ohd.fReadOnly = fReadOnly;
   ohd.pResHelp = &rh;

   // create the window
   RECT r;
   PWSTR psz;
   DialogBoxLocation3 (GetDesktopWindow(), &r, TRUE);
   CEscWindow Window;
   Window.Init (ghInstance, hWnd, 0, &r);
redo:
   psz = Window.PageDialog (ghInstance, IDR_MMLRESHELP, ResHelpPage, &ohd);
   if (psz && !_wcsicmp(psz, RedoSamePage()))
      goto redo;

   if (!ohd.fChanged)
      return NULL;

   // create new MML
   PCMMLNode2 pNode = rh.MMLTo ();
   if (!pNode)
      return NULL;
   return pNode;
}


/*************************************************************************************
CResHelp::CheckSum - Returns a checsum so can tell if help changed
*/
DWORD CResHelp::CheckSum (void)
{
   DWORD dwCount = 0, dwSum =0;
   WCHAR wXOR = 0;

   DWORD i;
   for (i = 0; i < 6; i++) {
      PWSTR psz = NULL;
      switch (i) {
      case 0:
         psz = (PWSTR)m_memName.p;
         break;
      case 1:
         psz = (PWSTR)m_memDescShort.p;
         break;
      case 2:
         psz = (PWSTR)m_memDescLong.p;
         break;
      case 3:
         psz = (PWSTR)m_aMemHelp[0].p;
         break;
      case 4:
         psz = (PWSTR)m_aMemHelp[1].p;
         break;
      case 5:
         psz = (PWSTR)m_memBook.p;
         break;
      } // switch

      for (; *psz; psz++, dwCount++) {
         dwSum += (DWORD)(*psz);
         wXOR ^= *psz;
      }
   } // i

   // combine together
   return (LOWORD(dwSum) + ((DWORD)wXOR << 24)) ^ (dwCount << 16);
}


/*************************************************************************************
CResHelp::ResourceGet - Returns a string which is the MML resource to be passed to
any MIFL function getting the help's resource.

returns
   PWSTR - String. NULL if error
*/
PWSTR CResHelp::ResourceGet (void)
{
   // if already calculated then easy
   PWSTR psz = (PWSTR)m_memAsResource.p;
   if (psz[0])
      return psz;

   // else, generate
   PCMMLNode2 pNode = MMLTo ();
   if (!pNode)
      return NULL;

   // remove the original text since ends up being worthless
   DWORD dwIndex = pNode->ContentFind (gpszOrigText);
   if (dwIndex != -1)
      pNode->ContentRemove (dwIndex);

   // convert to MML
   m_memAsResource.m_dwCurPosn = 0;
   if (!MMLToMem (pNode, &m_memAsResource, FALSE, 0, FALSE)) {
      delete pNode;
      return NULL;
   }
   m_memAsResource.CharCat (0);

   // finally
   delete pNode;
   return (PWSTR)m_memAsResource.p;
}
