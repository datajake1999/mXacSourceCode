/*********************************************************************************
CNPREffectEffects.cpp - Code for effect

begun 7/4/2004
Copyright 2004 by Mike Rozak
All rights reserved
*/

#include <windows.h>
#include "escarpment.h"
#include "..\M3D.h"
#include "resource.h"


#ifdef _DEBUG
#define WORKONEFFECTS // BUGBUG
#endif


// FXPage - Used for the effects
typedef struct {
   PCObjectSurface pSurf;  // object surface
   WCHAR       szMajor[128];   // major category name
   WCHAR       szMinor[128];   // minor category name
   WCHAR       szName[128];    // name
   PCNPREffectsList pEffects;  // effects currently using
   PCImage     pImage;     // image usaing as test
   PCRenderSuper pRender;     // rendering engine that generated
   PCWorldSocket pWorld;      // world that generated
   HBITMAP     hBit;       // bitmap of the image - 200 x 200
   WCHAR       szCurMajorCategory[128]; // current major category shown
   WCHAR       szCurMinorCategory[128]; // current minor cateogyr shown
   BOOL        fPressedOK; // set to TRUE if press OK
   GUID        gEffectCode;   // major GUID for effect
   GUID        gEffectSub;    // minor GUID for effect
   DWORD       dwRenderShard; // render shard to use

   // used for EffectsSelPage
   int         iVScroll;      // where to scroll to
   GUID        gCode;      // id GUID
   GUID        gSub;       // id GUID
   PCListFixed pThumbInfo;    // for whichimages there
   DWORD       dwTimerID;     // for page
   BOOL        fChanged;      // set to TRUE if anything was changed that requires refresh
   COLORREF    cTransColor;   // used for TextBlankPage and TextFromFilePage
} FXPAGE, *PFXPAGE;

typedef struct {
   HBITMAP     hBitmap;    // bitmap used
   BOOL        fEmpty;     // if fEmpty is set then the bitmap used is a placeholder
   PWSTR       pszName;    // name of the Effect - do not modify
   COLORREF    cTransparent;  // if not -1 then this is the transparent color
} THUMBINFO, *PTHUMBINFO;






/****************************************************************************
UniqueEffectName - Given a name, comes up with a unique one that wont
conflict with other Effects.

inputs
   PWSTR       pszMajor, pszMinor - Categories
   PWSTR       pszName - Initially filled with name, and then modified
returns
   none
*/
void UniqueEffectName (DWORD dwRenderShard, PWSTR pszMajor, PWSTR pszMinor, PWSTR pszName)
{
   DWORD i;
   for (i = 0; ; i++) {
      if (i) {
         PWSTR pszCur;
         // remove numbers from end
         for (pszCur = pszName  + (wcslen(pszName)-1); (pszCur >= pszName) && (*pszCur >= L'0') && (*pszCur <= L'9'); pszCur--)
            pszCur[0] = 0;

         // remove spaces
         for (pszCur = pszName  + (wcslen(pszName)-1); (pszCur >= pszName) && (*pszCur == L' '); pszCur--)
            pszCur[0] = 0;

         // append space and number
         swprintf (pszName + wcslen(pszName), L" %d", (int) i+1);
      }

      // if it matches try again
      if (LibraryEffects(dwRenderShard, FALSE)->ItemGet(pszMajor, pszMinor, pszName))
         continue;
      if (LibraryEffects(dwRenderShard, TRUE)->ItemGet(pszMajor, pszMinor, pszName))
         continue;

      // no match
      return;
   }
}


/***********************************************************************************
EffectCreate - Creates a new Effect object based on the GUIDs.

inputs
   GUID     *pCode - Code guid
   GUID     *pSub - sub guid
returns
   PCEffectsList - New object. NULL if error
*/
PCNPREffectsList EffectCreate (DWORD dwRenderShard, const GUID *pCode, const GUID *pSub)
{
   WCHAR szMajor[128], szMinor[128], szName[128];
   if (!EffectNameFromGUIDs (dwRenderShard, (GUID*) pCode, (GUID*) pSub, szMajor, szMinor, szName))
      return NULL;

   PCMMLNode2 pNode;
   BOOL fUser;
   fUser = TRUE;
   pNode = LibraryEffects(dwRenderShard, TRUE)->ItemGet (szMajor, szMinor, szName);
   if (!pNode) {
      pNode = LibraryEffects(dwRenderShard, FALSE)->ItemGet (szMajor, szMinor, szName);
      fUser = FALSE;
   }
   if (!pNode)
      return NULL;

   PCNPREffectsList pe = new CNPREffectsList(dwRenderShard);
   if (!pe)
      return NULL;
   if (!pe->MMLFrom (pNode)) {
      delete pe;
      return NULL;
   }
   return pe;
}


/***********************************************************************************
EffectEnumMajor - Enumerates the major category names for Effects.

inputs
   PCListFixed    pl- List to be filled with, List of PWSTR that point to the category
      names. DO NOT change the strings. This IS sorted.
returns
   none
*/
void EffectEnumMajor (DWORD dwRenderShard, PCListFixed pl)
{
   CListFixed l2;
   LibraryEffects(dwRenderShard, FALSE)->EnumMajor (pl);
   LibraryEffects(dwRenderShard, TRUE)->EnumMajor(&l2);
   LibraryCombineLists (pl, &l2);
}


/***********************************************************************************
EffectEnumMinor - Enumerates the minor category names for Effects.

inputs
   PCListFixed    pl- List to be filled with, List of PWSTR that point to the category
      names. DO NOT change the strings. This IS sorted.
   PWSTR          pszMajor - Major category name
returns
   none
*/
void EffectEnumMinor (DWORD dwRenderShard, PCListFixed pl, PWSTR pszMajor)
{
   CListFixed l2;
   LibraryEffects(dwRenderShard, FALSE)->EnumMinor (pl, pszMajor);
   LibraryEffects(dwRenderShard, TRUE)->EnumMinor (&l2, pszMajor);
   LibraryCombineLists (pl, &l2);
}

/***********************************************************************************
EffectEnumItems - Enumerates the items of a minor category in Effects.

inputs
   PCListFixed    pl- List to be filled with, List of PWSTR that point to the category
      names. DO NOT change the strings. This IS sorted.
   PWSTR          pszMajor - Major category name
   PWSTR          pszMinor - Minor category name
returns
   none
*/
void EffectEnumItems (DWORD dwRenderShard, PCListFixed pl, PWSTR pszMajor, PWSTR pszMinor)
{
   CListFixed l2;
   LibraryEffects(dwRenderShard, FALSE)->EnumItems (pl, pszMajor, pszMinor);
   LibraryEffects(dwRenderShard, TRUE)->EnumItems (&l2, pszMajor, pszMinor);
   LibraryCombineLists (pl, &l2);
}

/***********************************************************************************
EffectGUIDsFromName - Given a Effect name, retuns the GUIDs

inputs
   PWSTR          pszMajor - Major category name
   PWSTR          pszMinor - Minor category name
   PWSTR          pszName - Name of the Effect
   GUID           *pgCode - Filled with the code GUID if successful
   GUID           *pgSub - Filled with the sub GUID if successful
returns
   BOOL - TRUE if find
*/
BOOL EffectGUIDsFromName (DWORD dwRenderShard, PWSTR pszMajor, PWSTR pszMinor, PWSTR pszName, GUID *pgCode, GUID *pgSub)
{
   if (LibraryEffects(dwRenderShard, FALSE)->ItemGUIDFromName (pszMajor, pszMinor, pszName, pgCode, pgSub))
      return TRUE;
   if (LibraryEffects(dwRenderShard, TRUE)->ItemGUIDFromName (pszMajor, pszMinor, pszName, pgCode, pgSub))
      return TRUE;
   return FALSE;
}

/***********************************************************************************
EffectNameFromGUIDs - Given a Effects GUIDs, this fills in a buffer with
its name strings.

inputs
   GUID           *pgCode - Code guid
   GUID           *pgSub - Sub guid
   PWSTR          pszMajor - Filled with Major category name
   PWSTR          pszMinor - Filled with Minor category name
   PWSTR          pszName - Filled with Name of the Effect
returns
   BOOL - TRUE if find
*/
BOOL EffectNameFromGUIDs (DWORD dwRenderShard, GUID *pgCode, GUID *pgSub,PWSTR pszMajor, PWSTR pszMinor, PWSTR pszName)
{
   if (LibraryEffects(dwRenderShard, FALSE)->ItemNameFromGUID (pgCode, pgSub, pszMajor, pszMinor, pszName))
      return TRUE;
   if (LibraryEffects(dwRenderShard, TRUE)->ItemNameFromGUID (pgCode, pgSub, pszMajor, pszMinor, pszName))
      return TRUE;
   return FALSE;
}




/**************************************************************************
EffectRenamePage
*/
BOOL EffectRenamePage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PFXPAGE pt = (PFXPAGE) pPage->m_pUserData;
   DWORD dwRenderShard = pt->dwRenderShard;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         PCEscControl pControl;

         pControl = pPage->ControlFind (L"name");
         if (pControl)
            pControl->AttribSet (Text(), pt->szName);
         pControl = pPage->ControlFind (L"category");
         if (pControl)
            pControl->AttribSet (Text(), pt->szMajor);
         pControl = pPage->ControlFind (L"subcategory");
         if (pControl)
            pControl->AttribSet (Text(), pt->szMinor);
      }
      break;

   case ESCM_LINK:
      {
         PESCMLINK p = (PESCMLINK) pParam;
         if (!p->psz)
            break;
         if (!_wcsicmp(p->psz, L"ok")) {
            WCHAR szMajor[128], szMinor[128], szName[128];

            PCEscControl pControl;

            szMajor[0] = szMinor[0] = szName[0] = 0;
            DWORD dwNeeded;
            pControl = pPage->ControlFind (L"name");
            if (pControl)
               pControl->AttribGet (Text(), szName, sizeof(szName), &dwNeeded);
            pControl = pPage->ControlFind (L"category");
            if (pControl)
               pControl->AttribGet (Text(), szMajor, sizeof(szName), &dwNeeded);
            pControl = pPage->ControlFind (L"subcategory");
            if (pControl)
               pControl->AttribGet (Text(), szMinor, sizeof(szName), &dwNeeded);

            if (!szMajor[0] || !szMinor[0] || !szName[0]) {
               pPage->MBWarning (L"You cannot leave the category, sub-category, or name blank.");
               return TRUE;
            }

            GUID gCode, gSub;
            if (EffectGUIDsFromName(dwRenderShard, szMajor, szMinor, szName, &gCode, &gSub)) {
               pPage->MBWarning (L"That name already exists.",
                  L"Your effect must have a unique name.");
               return TRUE;
            }

            // ok, rename it
            LibraryEffects(dwRenderShard, FALSE)->ItemRename (pt->szMajor, pt->szMinor, pt->szName,
               szMajor, szMinor, szName);
            LibraryEffects(dwRenderShard, TRUE)->ItemRename (pt->szMajor, pt->szMinor, pt->szName,
               szMajor, szMinor, szName);

            wcscpy (pt->szMajor, szMajor);
            wcscpy (pt->szMinor, szMinor);
            wcscpy (pt->szName, szName);

            break;
         }
      }
      break;
   }

   return DefPage (pPage, dwMessage, pParam);
}



/****************************************************************************
EffectLibraryPage
*/
BOOL EffectLibraryPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   static WCHAR sEffectTemp[16];

   PFXPAGE pt = (PFXPAGE) pPage->m_pUserData;
   DWORD dwRenderShard = pt->dwRenderShard;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         // fill in the list of categories
         pPage->Message (ESCM_USER+91);

         // new Effect so set all the parameters
         pPage->Message (ESCM_USER+84);
      }
      break;

   case ESCM_USER+91: // update the major categoriy list
      {
         CListFixed list;
         DWORD i;
         CMem mem;
         PWSTR psz;
         PCEscControl pControl;
         EffectEnumMajor (dwRenderShard, &list);
         MemZero (&mem);
         for (i = 0; i < list.Num(); i++) {
            psz = *((PWSTR*) list.Get(i));
            MemCat (&mem, L"<elem name=\"");
            MemCatSanitize (&mem, psz);
            MemCat (&mem, L"\">");
            MemCatSanitize (&mem, psz);
            MemCat (&mem, L"</elem>");
         }
         pControl = pPage->ControlFind (L"major");
         if (pControl) {
            pControl->Message (ESCM_COMBOBOXRESETCONTENT);

            ESCMCOMBOBOXADD add;
            memset (&add, 0, sizeof(add));
            add.dwInsertBefore = -1;
            add.pszMML = (PWSTR) mem.p;
            pControl->Message (ESCM_COMBOBOXADD, &add);

            ESCMCOMBOBOXSELECTSTRING sel;
            memset (&sel, 0, sizeof(sel));
            sel.fExact = TRUE;
            sel.psz = pt->szMajor;
            pControl->Message (ESCM_COMBOBOXSELECTSTRING, &sel);
         }
      }
      return TRUE;

   case ESCM_USER+82: // set the minor category box
      {
         // if we're showing the right one then don't care
         if (!_wcsicmp(pt->szCurMajorCategory, pt->szMajor))
            return TRUE;

         // if changed, then names will be changed to
         pt->szCurMinorCategory[0] = 0;
         wcscpy (pt->szCurMajorCategory, pt->szMajor);

         // fill in the list of categories
         CListFixed list;
         DWORD i;
         CMem mem;
         PWSTR psz;
         PCEscControl pControl;
         EffectEnumMinor (dwRenderShard, &list, pt->szMajor);
         MemZero (&mem);
         for (i = 0; i < list.Num(); i++) {
            psz = *((PWSTR*) list.Get(i));
            MemCat (&mem, L"<elem name=\"");
            MemCatSanitize (&mem, psz);
            MemCat (&mem, L"\">");
            MemCatSanitize (&mem, psz);
            MemCat (&mem, L"</elem>");
         }
         pControl = pPage->ControlFind (L"minor");
         if (pControl) {
            pControl->Message (ESCM_COMBOBOXRESETCONTENT);

            ESCMCOMBOBOXADD add;
            memset (&add, 0, sizeof(add));
            add.dwInsertBefore = -1;
            add.pszMML = (PWSTR) mem.p;
            pControl->Message (ESCM_COMBOBOXADD, &add);

            ESCMCOMBOBOXSELECTSTRING sel;
            memset (&sel, 0, sizeof(sel));
            sel.fExact = TRUE;
            sel.psz = pt->szMinor;
            pControl->Message (ESCM_COMBOBOXSELECTSTRING, &sel);
         }
      }
      break;


   case ESCM_USER+83: // set the names list box
      {
         // if we're showing the right one then don't care
         if (!_wcsicmp(pt->szCurMinorCategory, pt->szMinor))
            return TRUE;

         // if changed, then names will be changed to
         wcscpy (pt->szCurMinorCategory, pt->szMinor);

         // fill in the list of names
         CListFixed list;
         DWORD i;
         CMem mem;
         PWSTR psz;
         PCEscControl pControl;
         EffectEnumItems (dwRenderShard, &list, pt->szMajor, pt->szMinor);
         MemZero (&mem);
         BOOL fBold;
         for (i = 0; i < list.Num(); i++) {
            psz = *((PWSTR*) list.Get(i));
            MemCat (&mem, L"<elem name=\"");
            MemCatSanitize (&mem, psz);
            MemCat (&mem, L"\"><small>");

            // if it's a custom Effect then bold it
            if (LibraryEffects(dwRenderShard, TRUE)->ItemGet (pt->szMajor, pt->szMinor, psz))
               fBold = TRUE;
            else
               fBold = FALSE;
            if (fBold)
               MemCat (&mem, L"<font color=#008000>");
            MemCatSanitize (&mem, psz);
            if (fBold)
               MemCat (&mem, L"</font>");
            MemCat (&mem, L"</small></elem>");
         }
         pControl = pPage->ControlFind (L"name");
         if (pControl) {
            pControl->Message (ESCM_LISTBOXRESETCONTENT);

            ESCMLISTBOXADD add;
            memset (&add, 0, sizeof(add));
            add.dwInsertBefore = -1;
            add.pszMML = (PWSTR) mem.p;
            pControl->Message (ESCM_LISTBOXADD, &add);

            ESCMLISTBOXSELECTSTRING sel;
            memset (&sel, 0, sizeof(sel));
            sel.fExact = TRUE;
            sel.psz = pt->szName;
            pControl->Message (ESCM_LISTBOXSELECTSTRING, &sel);
         }
      }
      break;

   case ESCM_USER+84:  // we have a new Effect so set the parameters
      {
         // set the minor category box if necessary
         pPage->Message (ESCM_USER+82);

         // set the name box if necessary
         pPage->Message (ESCM_USER+83);

         // set it in the comboboxes and list boxes
         PCEscControl pControl;
         pControl = pPage->ControlFind (L"major");
         if (pControl) {
            ESCMCOMBOBOXSELECTSTRING sel;
            memset (&sel, 0, sizeof(sel));
            sel.fExact = TRUE;
            sel.psz = pt->szMinor;
            pControl->Message (ESCM_COMBOBOXSELECTSTRING, &sel);
         }
         pControl = pPage->ControlFind (L"minor");
         if (pControl) {
            ESCMCOMBOBOXSELECTSTRING sel;
            memset (&sel, 0, sizeof(sel));
            sel.fExact = TRUE;
            sel.psz = pt->szMinor;
            pControl->Message (ESCM_COMBOBOXSELECTSTRING, &sel);
         }
         pControl = pPage->ControlFind (L"name");
         if (pControl) {
            ESCMLISTBOXSELECTSTRING sel;
            memset (&sel, 0, sizeof(sel));
            sel.fExact = TRUE;
            sel.psz = pt->szName;
            pControl->Message (ESCM_LISTBOXSELECTSTRING, &sel);
         }

         // redo the bitmap
         pPage->Message (ESCM_USER+85);
      }
      return TRUE;

   case ESCM_USER+85:   // redo the bitmap
      {
         // update the buttons for editing if custom
         BOOL  fBuiltIn;
         PCEscControl pControl;
         if (LibraryEffects(dwRenderShard, FALSE)->ItemGet (pt->szMajor, pt->szMinor, pt->szName))
            fBuiltIn = TRUE;
         else
            fBuiltIn = FALSE;
#ifdef WORKONEFFECTS
         fBuiltIn = FALSE; // allow to modify all
#endif
         pControl = pPage->ControlFind (L"edit");
         if (pControl)
            pControl->Enable (!fBuiltIn);
         pControl = pPage->ControlFind (L"rename");
         if (pControl)
            pControl->Enable (!fBuiltIn);
         pControl = pPage->ControlFind (L"remove");
         if (pControl)
            pControl->Enable (!fBuiltIn);


         // redo the bitmap
         PCImage pTemp = pt->pImage->Clone();
         if (!pTemp)
            return TRUE;
         if (pt->pEffects) {
            CProgress Progress;
            Progress.Start (pPage->m_pWindow->m_hWnd, "Drawing...", TRUE);
            pt->pEffects->Render (pTemp, pt->pRender, pt->pWorld, TRUE, &Progress);
         }
         HDC hDC;
         hDC = GetDC (pPage->m_pWindow->m_hWnd);
         if (pt->hBit)
            DeleteObject (pt->hBit);
         pt->hBit = pTemp->ToBitmap (hDC);
         ReleaseDC (pPage->m_pWindow->m_hWnd, hDC);
         delete pTemp;

         pControl = pPage->ControlFind (L"image");

         WCHAR szTemp[32];
         swprintf (szTemp, L"%lx", (__int64) pt->hBit);
         pControl->AttribSet (L"hbitmap", szTemp);

      }
      return TRUE;

   case ESCM_USER + 86: // create a new Effect using the new major, minor, and name
      {
         // get the guids
         if (!EffectGUIDsFromName (dwRenderShard, pt->szMajor, pt->szMinor, pt->szName, &pt->gEffectCode, &pt->gEffectSub))
            return FALSE;  // error

         // get the teture
         if (pt->pEffects)
            delete pt->pEffects;
         pt->pEffects = EffectCreate (dwRenderShard, &pt->gEffectCode, &pt->gEffectSub);
         //if (!pt->pEffects)
         //   return FALSE;

         // and make sure to refresh
         pPage->Message (ESCM_USER+84);
      }
      return TRUE;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;

         if (!p->pControl || !p->pControl->m_pszName)
            break;

         if (!_wcsicmp(p->pControl->m_pszName, L"copyedit")) {
            // get it
            GUID gCode, gSub;
            PCMMLNode2 pNode;
            if (!EffectGUIDsFromName (dwRenderShard, pt->szMajor, pt->szMinor, pt->szName, &gCode, &gSub))
               return TRUE;

            pNode = LibraryEffects(dwRenderShard, FALSE)->ItemGet (pt->szMajor, pt->szMinor, pt->szName);
            if (!pNode)
               pNode = LibraryEffects(dwRenderShard, TRUE)->ItemGet  (pt->szMajor, pt->szMinor, pt->szName);
            if (!pNode)
               return TRUE;

            // clone
            pNode = pNode->Clone();
            if (!pNode)
               return TRUE;

            // find a new name
            UniqueEffectName (dwRenderShard, pt->szMajor, pt->szMinor, pt->szName);

            GUIDGen(&gSub);

            //add it
#ifdef WORKONEFFECTS
            LibraryEffects(dwRenderShard, FALSE)->ItemAdd (pt->szMajor, pt->szMinor, pt->szName, &gCode, &gSub, pNode);
#else
            LibraryEffects(dwRenderShard, TRUE)->ItemAdd (pt->szMajor, pt->szMinor, pt->szName, &gCode, &gSub, pNode);
#endif

            // clean out the thumbnail just in case
            ThumbnailGet()->ThumbnailRemove (&gCode, &gSub);

            EffectGUIDsFromName (dwRenderShard, pt->szMajor, pt->szMinor, pt->szName,
               &pt->gEffectCode, &pt->gEffectSub);

            // update lists and selection
            pt->szCurMinorCategory[0] = 0;   // so will refresh
            pt->szCurMajorCategory[0] = 0;   // so refreshes

            pPage->MBSpeakInformation (L"Effect copied.");

            pPage->Exit (L"edit");
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"rename")) {
            // get the angle
            CEscWindow cWindow;
            RECT r;
            DialogBoxLocation2 (pPage->m_pWindow->m_hWnd, &r);
            cWindow.Init (ghInstance, pPage->m_pWindow->m_hWnd, EWS_FIXEDSIZE | EWS_AUTOHEIGHT | EWS_NOTITLE, &r);
            PWSTR pszRet;
            pszRet = cWindow.PageDialog (ghInstance, IDR_MMLEFFECTRENAME, EffectRenamePage, pt);

            //if (!_wcsicmp(pszRet, L"ok")) { // BUGFIX - Because if hit enter returns [close]
               pt->szCurMajorCategory[0] = 0;
               pt->szCurMinorCategory[0] = 0;
               pPage->Message (ESCM_USER+91);   // new major categories
               pPage->Message (ESCM_USER+86);   // other stuff may have changed
               // BUGFIX - Was +84, but try 86 to see if fixes occasional bug
            //}

            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"remove")) {
            if (IDYES != pPage->MBYesNo (L"Are you sure you wish to permenantly delete this effect?"))
               return TRUE;

            // clear the cache
            GUID gCode, gSub;
            EffectGUIDsFromName (dwRenderShard, pt->szMajor, pt->szMinor, pt->szName, &gCode, &gSub);
            

            // remove it from either one
            LibraryEffects(dwRenderShard, FALSE)->ItemRemove (pt->szMajor, pt->szMinor, pt->szName);
            LibraryEffects(dwRenderShard, TRUE)->ItemRemove (pt->szMajor, pt->szMinor, pt->szName);

            // clean out the thumbnail just in case
            ThumbnailGet()->ThumbnailRemove (&gCode, &gSub);


            // new Effect
            CListFixed list;
            PWSTR psz;
            EffectEnumItems (dwRenderShard, &list, pt->szMajor, pt->szMinor);
            if (!list.Num()) {
               // minor no longer exists
               EffectEnumMinor (dwRenderShard, &list, pt->szMajor);
               if (!list.Num()) {
                  // minor no longer exists
                  EffectEnumMajor (dwRenderShard, &list);
                  psz = *((PWSTR*) list.Get(0));
                  wcscpy (pt->szMajor, psz);

                  EffectEnumMinor (dwRenderShard, &list, pt->szMajor);
                  // will have something there
               }

               psz = *((PWSTR*) list.Get(0));
               wcscpy (pt->szMinor, psz);
               EffectEnumItems (dwRenderShard, &list, pt->szMajor, pt->szMinor);
               // will have something there
            }
            psz = *((PWSTR*) list.Get(0));
            wcscpy(pt->szName, psz);
            pt->szCurMajorCategory[0] = 0;
            pt->szCurMinorCategory[0] = 0;
            EffectGUIDsFromName (dwRenderShard, pt->szMajor, pt->szMinor, pt->szName,
               &pt->gEffectCode, &pt->gEffectSub);
            pPage->Message (ESCM_USER+91);   // new major categories
            pPage->Message (ESCM_USER+86);   // other stuff may have changed
               // BUGFIX - Was +84 but changed to +86

            pPage->MBSpeakInformation (L"Effect removed.");
            return TRUE;
         }
      }
      break;

   case ESCN_COMBOBOXSELCHANGE:
      {
         PESCNCOMBOBOXSELCHANGE p = (PESCNCOMBOBOXSELCHANGE) pParam;
         if (!p->pControl->m_pszName)
            break;

         // if the major ID changed then pick the first minor and Effect that
         // comes to mind
         if (!_wcsicmp(p->pControl->m_pszName, L"major")) {
            // if it hasn't reall change then ignore
            if (!_wcsicmp(pt->szMajor, p->pszName))
               return TRUE;

            wcscpy (pt->szMajor, p->pszName);

            CListFixed list;
            PWSTR psz;

            // first minor we find
            EffectEnumMinor (dwRenderShard, &list, pt->szMajor);
            psz = *((PWSTR*) list.Get(0));
            wcscpy (pt->szMinor, psz);

            // first Effect we find
            EffectEnumItems (dwRenderShard, &list, pt->szMajor, pt->szMinor);
            psz = *((PWSTR*) list.Get(0));
            wcscpy(pt->szName, psz);

            pPage->Message (ESCM_USER + 86);
            return TRUE;
         }
         if (!_wcsicmp(p->pControl->m_pszName, L"minor")) {
            // if it hasn't reall change then ignore
            if (!_wcsicmp(pt->szMinor, p->pszName))
               return TRUE;

            wcscpy (pt->szMinor, p->pszName);

            CListFixed list;
            PWSTR psz;

            // first Effect we find
            EffectEnumItems (dwRenderShard, &list, pt->szMajor, pt->szMinor);
            if (list.Num()) {
               psz = *((PWSTR*) list.Get(0));
               wcscpy(pt->szName, psz);
            }
            else
               pt->szName[0] = 0;

            pPage->Message (ESCM_USER + 86);
            return TRUE;
         }
      }
      break;

   case ESCN_LISTBOXSELCHANGE:
      {
         PESCNLISTBOXSELCHANGE p = (PESCNLISTBOXSELCHANGE) pParam;
         if (!p->pControl->m_pszName)
            break;

         // if the major ID changed then pick the first minor and Effect that
         // comes to mind
         if (!_wcsicmp(p->pControl->m_pszName, L"name")) {
            // if it hasn't reall change then ignore
            if (!_wcsicmp(pt->szName, p->pszName))
               return TRUE;

            wcscpy (pt->szName, p->pszName);

            pPage->Message (ESCM_USER + 86);
            return TRUE;
         }
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"HBITMAP")) {
            swprintf (sEffectTemp, L"%lx", (__int64) pt->hBit);
            p->pszSubString = sEffectTemp;
            return TRUE;
         }
         //else if (!_wcsicmp(p->pszSubName, L"SCALE")) {
         //   DWORD dwScale = 200 * 100 / pt->pImage->Width();
         //   dwScale = max(dwScale, 1);
         //   swprintf (sEffectTemp, L"%d", (int) dwScale);
         //   p->pszSubString = sEffectTemp;
         //   return TRUE;
         //}
         else if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Effects library";
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}



/****************************************************************************
EffectLibraryDialog - This brings up the UI for changing the library

inputs
   HWND           hWnd - Window to create this over
   PCImage        pImage - Image for sample
   PCRenderSuper  pRender - Render engine that generated - can be NULL
   PCWorldSocket  pWorld - World that drawn - can be NULL
returns
   BOOL - TRUE if the user presses OK, FALSE if not
*/
BOOL EffectLibraryDialog (DWORD dwRenderShard, HWND hWnd, PCImage pImage, PCRenderSuper pRender,
                          PCWorldSocket pWorld)
{
   CEscWindow cWindow;
   RECT r;
   DialogBoxLocation (hWnd, &r);

   cWindow.Init (ghInstance, hWnd, EWS_FIXEDSIZE, &r);
   PWSTR pszRet;

   // set up the info
   FXPAGE t;
   memset (&t, 0, sizeof(t));
   t.pRender = pRender;
   t.pWorld = pWorld;
   t.pImage = pImage;
   t.hBit = NULL;
   t.dwRenderShard = dwRenderShard;
   
   // couldn't find, so start out with something
   CListFixed list;
   PWSTR psz;

   // first major we find
   EffectEnumMajor (dwRenderShard, &list);
   psz = list.Num() ? *((PWSTR*) list.Get(0)) : L"";
   wcscpy (t.szMajor, psz);

   // first minor we find
   EffectEnumMinor (dwRenderShard, &list, t.szMajor);
   psz = list.Num() ? *((PWSTR*) list.Get(0)) : L"";
   wcscpy (t.szMinor, psz);

   // first Effect we find
   EffectEnumItems (dwRenderShard, &list, t.szMajor, t.szMinor);
   psz = list.Num() ? *((PWSTR*) list.Get(0)) : L"";
   wcscpy(t.szName, psz);

newtext:
   // get the guids
   if (t.pEffects)
      delete t.pEffects;
   if (EffectGUIDsFromName (dwRenderShard, t.szMajor, t.szMinor, t.szName, &t.gEffectCode, &t.gEffectSub))
      t.pEffects = EffectCreate (dwRenderShard, &t.gEffectCode, &t.gEffectSub);
   else
      t.pEffects = NULL;

   if (t.hBit)
      DeleteObject (t.hBit);
   t.hBit = NULL;

   // redo the bitmap
   PCImage pTemp = t.pImage->Clone();
   if (!pTemp)
      return TRUE;
   // BUGFIX - Dont modify here because will draw as soon as enters page
   //if (t.pEffects) {
   //   CProgress Progress;
   //   Progress.Start (cWindow.m_hWnd, "Drawing...", TRUE);
   //   t.pEffects->Render (pTemp, t.pRender, t.pWorld, TRUE, &Progress);
   //}
   HDC hDC;
   hDC = GetDC (hWnd);
   if (t.hBit)
      DeleteObject (t.hBit);
   t.hBit = pTemp->ToBitmap (hDC);
   ReleaseDC (hWnd, hDC);
   delete pTemp;

   // start with the first page
mainpage:
   // BUGFIX - Set curmajor and minor to 0 so refreshes
   t.szCurMajorCategory[0] = 0;
   t.szCurMinorCategory[0] = 0;
   pszRet = cWindow.PageDialog (ghInstance, IDR_MMLEFFECTLIBRARY, EffectLibraryPage, &t);

   if (pszRet && !_wcsicmp(pszRet, L"newtext")) {
      // create it
      PCNPREffectsList pel = new CNPREffectsList(dwRenderShard);
      if (!pel)
         goto mainpage;

      PCMMLNode2 pNode;
      pNode = pel->MMLTo ();
      if (!pNode) {
         delete pel;
         goto mainpage;
      }

      AttachDateTimeToMML (pNode);

      GUID gSub, gCode;
      GUIDGen (&gSub);
      GUIDGen (&gCode);

      // make sure have name
      if (!t.szMajor[0])
         wcscpy (t.szMajor, L"Unknown");
      if (!t.szMinor[0])
         wcscpy (t.szMinor, L"Unknown");

      // get a unique name
      wcscpy (t.szName, L"New effect");
      UniqueEffectName (dwRenderShard, t.szMajor, t.szMinor, t.szName);
      t.szCurMajorCategory[0] = t.szCurMinorCategory[0] = 0;

#ifdef WORKONEFFECTS
      LibraryEffects(dwRenderShard, FALSE)->ItemAdd (t.szMajor, t.szMinor, t.szName,
         &gCode, &gSub, pNode);
#else
      LibraryEffects(dwRenderShard, TRUE)->ItemAdd (t.szMajor, t.szMinor, t.szName,
         &gCode, &gSub, pNode);
#endif

      // clean out the thumbnail just in case
      ThumbnailGet()->ThumbnailRemove (&gCode, &gSub);

      delete pel;
      goto edit;
   }
   else if (pszRet && !_wcsicmp(pszRet, L"edit")) {
edit:
      GUID gCode, gSub;
      if (!EffectGUIDsFromName(dwRenderShard, t.szMajor, t.szMinor, t.szName, &gCode, &gSub))
         goto mainpage; // error

      // get the MML
      PCMMLNode2 pNode;
      BOOL fUser;
      fUser = TRUE;
      pNode = LibraryEffects(dwRenderShard, TRUE)->ItemGet (t.szMajor, t.szMinor, t.szName);
      if (!pNode) {
         pNode = LibraryEffects(dwRenderShard, FALSE)->ItemGet (t.szMajor, t.szMinor, t.szName);
         fUser = FALSE;
      }
      if (!pNode)
         goto mainpage; // error

      // create
      PCNPREffectsList pel;
      pel = EffectCreate (dwRenderShard, &gCode, &gSub);

      // UI for this
      BOOL fRet;
      pszRet = NULL; // BUGFIX so doesnt crash
      fRet = pel->Dialog (&cWindow, t.pImage, pRender, pWorld);
      int iRet;
      iRet = EscMessageBox (cWindow.m_hWnd, ASPString(),
         L"Do you want to save the changes to your effect?",
         NULL,
         MB_ICONQUESTION | MB_YESNO);
      if (iRet == IDYES) {
         PCMMLNode2 pNode;
         pNode = pel->MMLTo();
         if (pNode)
            AttachDateTimeToMML(pNode);

         if (fUser) {
            LibraryEffects(dwRenderShard, TRUE)->ItemRemove (t.szMajor, t.szMinor, t.szName);
            LibraryEffects(dwRenderShard, TRUE)->ItemAdd (t.szMajor, t.szMinor, t.szName,
               &gCode, &gSub, pNode);
         }
         else {
            LibraryEffects(dwRenderShard, FALSE)->ItemRemove (t.szMajor, t.szMinor, t.szName);
            LibraryEffects(dwRenderShard, FALSE)->ItemAdd (t.szMajor, t.szMinor, t.szName,
               &gCode, &gSub, pNode);
         }
         // clean out the thumbnail just in case
         ThumbnailGet()->ThumbnailRemove (&gCode, &gSub);
      }

      delete pel;

      t.szCurMinorCategory[0] = 0;   // so will refresh
      if (fRet)
         goto newtext; // done
      // else fall through
   }

   // free the Effect map and bitmap
   if (t.pEffects)
      delete t.pEffects;
   if (t.hBit)
      DeleteObject (t.hBit);

   if (pszRet && !_wcsicmp(pszRet, L"ok"))
      return TRUE;
   else
      return FALSE;
}






/****************************************************************************
EffectCreateThumbnail - Create a thumbnail from an object. This is added to
the thumbnail list.

inputs
   GUID        *pgMajor - Major GUID
   GUID        *pgMinor - Minor GUID
returns
   BOOL - TRUE if success
*/
BOOL EffectCreateThumbnail (DWORD dwRenderShard, GUID *pgCode, GUID *pgSub)
{
   if (IsEqualGUID(*pgCode, GUID_NULL))
      return FALSE;

   CImage   Image;
   PCRenderTraditional pRender = new CRenderTraditional (dwRenderShard);
   if (!pRender)
      return FALSE;
   CWorld   World;
   World.RenderShardSet (dwRenderShard);
   Image.Init (TEXTURETHUMBNAIL * 4, TEXTURETHUMBNAIL * 4, RGB(0xff,0xff,0xff));
   pRender->CImageSet (&Image);
   pRender->CWorldSet (&World);

   BOOL fRet = ObjectCreateThumbnail ((GUID*) &CLSID_BuildBlock, (GUID*) &CLSID_BuildBlockBuildHipHalf,
      pRender, &World, &Image, pgCode, pgSub);
   delete pRender;
   return fRet;
}

/****************************************************************************
EffectGetThumbnail - Returns the thumbnail for a Effect. If the thumbnail
doesn't exist then it's created.

inputs
   GUID        *pgMajor - Major GUID
   GUID        *pgMinor - Minor GUID
   HWND        hWnd - To get HDC from
   COLORREF    *pcTransparent - Filled with the transparent color
   BOOL        fBlankIfFail - If fails then returns a blank image

returns
   HBITMAP - Bitmap.Must have DestroyObject() called with it
*/
HBITMAP EffectGetThumbnail (DWORD dwRenderShard, GUID *pgCode, GUID *pgSub, HWND hWnd, COLORREF *pcTransparent,
                            BOOL fBlankIfFail)
{
   HBITMAP hBit;
   hBit = ThumbnailGet()->ThumbnailToBitmap (pgCode, pgSub, hWnd, pcTransparent);
   if (hBit)
      return hBit;


   // create it
   EffectCreateThumbnail (dwRenderShard, pgCode, pgSub);

   // try again
   hBit = ThumbnailGet()->ThumbnailToBitmap (pgCode, pgSub, hWnd, pcTransparent);
   if (hBit)
      return hBit;

   if (!fBlankIfFail)
      return NULL;

   // create a bitmap for error
   HBITMAP hBlank;
   HDC hDCBlank, hDCWnd;
   hDCWnd = GetDC (hWnd);
   hDCBlank = CreateCompatibleDC (hDCWnd);
   hBlank = CreateCompatibleBitmap (hDCWnd, TEXTURETHUMBNAIL, TEXTURETHUMBNAIL);
   SelectObject (hDCBlank, hBlank);
   ReleaseDC (hWnd, hDCWnd);
   RECT rBlank;
   rBlank.left = rBlank.top = 0;
   rBlank.right = rBlank.bottom = TEXTURETHUMBNAIL;
   FillRect (hDCBlank, &rBlank, (HBRUSH) GetStockObject (BLACK_BRUSH));
   // draw the text
   HFONT hFont;
   LOGFONT  lf;
   memset (&lf, 0, sizeof(lf));
   lf.lfHeight = -MulDiv(12, GetDeviceCaps(hDCBlank, LOGPIXELSY), 72); 
   lf.lfCharSet = DEFAULT_CHARSET;
   lf.lfPitchAndFamily = FF_SWISS | VARIABLE_PITCH;
   strcpy (lf.lfFaceName, "Arial");
   hFont = CreateFontIndirect (&lf);
   SelectObject (hDCBlank, hFont);
   SetTextColor (hDCBlank, RGB(0xff,0xff,0xff));
   SetBkMode (hDCBlank, TRANSPARENT);
   DrawText(hDCBlank, "No effect", -1, &rBlank, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_SINGLELINE);
   DeleteObject (hFont);
   DeleteDC (hDCBlank);

   return hBlank;
}





/************************************************************************8
EffectSelPage
*/

BOOL EffectSelPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PFXPAGE pt = (PFXPAGE) pPage->m_pUserData;
   DWORD dwRenderShard = pt->dwRenderShard;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         // add enter - close
         ESCACCELERATOR a;
         memset (&a, 0, sizeof(a));
         a.c = VK_ESCAPE;
         a.fAlt = FALSE;
         a.dwMessage = ESCM_CLOSE;
         pPage->m_listESCACCELERATOR.Add (&a);

         // Handle scroll on redosamepage
         if (pt->iVScroll >= 0) {
            pPage->VScroll (pt->iVScroll);

            // when bring up pop-up dialog often they're scrolled wrong because
            // iVScoll was left as valeu, and they use defpage
            pt->iVScroll = 0;

            // BUGFIX - putting this invalidate in to hopefully fix a refresh
            // problem when add or move a task in the ProjectView page
            pPage->Invalidate();
         }

         // set off time if any images are unfinished
         DWORD i;
         PTHUMBINFO pti;
         pt->dwTimerID = 0;
         for (i = 0; i < pt->pThumbInfo->Num(); i++) {
            pti = (PTHUMBINFO) pt->pThumbInfo->Get(i);
            if (pti->fEmpty && !pti->hBitmap)
               break;
         }
         if (i < pt->pThumbInfo->Num())
            pt->dwTimerID = pPage->m_pWindow->TimerSet (100, pPage); // first one right away
      }
      break;

   case ESCM_DESTRUCTOR:
      // kill the timer
      if (pt->dwTimerID)
         pPage->m_pWindow->TimerKill (pt->dwTimerID);
      pt->dwTimerID = 0;
      break;

   case ESCM_TIMER:
      {
         pPage->m_pWindow->TimerKill (pt->dwTimerID);
         pt->dwTimerID = 0;

         DWORD i;
         PTHUMBINFO pti;
         for (i = 0; i < pt->pThumbInfo->Num(); i++) {
            pti = (PTHUMBINFO) pt->pThumbInfo->Get(i);
            if (pti->fEmpty && !pti->hBitmap)
               break;
         }
         if (i >= pt->pThumbInfo->Num())
            return TRUE;

         // create
         pPage->m_pWindow->SetCursor (IDC_NOCURSOR);
         GUID gCode, gSub;
         EffectGUIDsFromName (dwRenderShard, pt->szMajor, pt->szMinor, pti->pszName, &gCode, &gSub);
         EffectCreateThumbnail (dwRenderShard, &gCode, &gSub);
         COLORREF cTransparent;
         pti->hBitmap = ThumbnailGet()->ThumbnailToBitmap (&gCode, &gSub, pPage->m_pWindow->m_hWnd,
            &cTransparent);
         if (pti->hBitmap) {
            pti->fEmpty = FALSE;

            WCHAR szTemp[32];
            swprintf (szTemp, L"bitmap%d", (int) i);
            PCEscControl pControl;
            pControl = pPage->ControlFind (szTemp);
            swprintf (szTemp, L"%lx", (__int64) pti->hBitmap);
            if (pControl)
               pControl->AttribSet (L"hbitmap", szTemp);
         }
         pPage->m_pWindow->SetCursor (IDC_HANDCURSOR);

         // set another timer
         for (i = 0; i < pt->pThumbInfo->Num(); i++) {
            pti = (PTHUMBINFO) pt->pThumbInfo->Get(i);
            if (pti->fEmpty && !pti->hBitmap)
               break;
         }
         if (i < pt->pThumbInfo->Num())
            pt->dwTimerID = pPage->m_pWindow->TimerSet (250, pPage);
      }
      return TRUE;

   }

   return DefPage (pPage, dwMessage, pParam);
}


/****************************************************************************
EffectSelDialog - This brings up the UI for changing the effect. Retrusn TRUE
if the user presses OK, FALSE if they press cancel. If the user presses OK
the object surface should probably be set.

inputs
   HWND           hWnd - Window to create this over
   GUID           *pgEffectCode - Main code for effect (GUID_NULL if none).
                     Will be changed if returns TRUE
   GUID           *pgEffectSub - Main code for effect (GUID_NULL if none)
                     Will be changed if returns TRUE
returns
   BOOL - TRUE if the user presses OK, FALSE if not
*/
BOOL EffectSelDialog (DWORD dwRenderShard, HWND hWnd, GUID *pgEffectCode, GUID *pgEffectSub)
{
   PWSTR pszMajorCat = L"mcat:", pszMinorCat = L"icat:", pszNameCat = L"obj:", pszColorCat = L"col:";
   DWORD dwMajorCat = (DWORD)wcslen(pszMajorCat), dwMinorCat = (DWORD)wcslen(pszMinorCat), dwNameCat = (DWORD)wcslen(pszNameCat),
      dwColorCat = (DWORD)wcslen(pszColorCat);

   CEscWindow cWindow;
   RECT r;
   DialogBoxLocation2 (hWnd, &r);

   // how many columns
   DWORD dwColumns;
   dwColumns = (DWORD)max(r.right - r.left,0) / (TEXTURETHUMBNAIL * 5 / 4);
   dwColumns = max(dwColumns, 3);

   cWindow.Init (ghInstance, hWnd, EWS_FIXEDSIZE, &r);
   PWSTR pszRet;

   // set up the info
   FXPAGE t;
   memset (&t, 0, sizeof(t));
   t.dwRenderShard = dwRenderShard;

   //HDC hDC;
   //hDC = GetDC (hWnd);
   //if (t.hBit)
   //   DeleteObject (t.hBit);
   //t.hBit = t.pImage->ToBitmap (hDC);
   //ReleaseDC (hWnd, hDC);

   CListFixed lThumbInfo;
   lThumbInfo.Init (sizeof(THUMBINFO));
   t.pThumbInfo = &lThumbInfo;
   t.gEffectCode = *pgEffectCode;
   t.gEffectSub = *pgEffectSub;

redoscheme:
   // get the name of the object
   WCHAR szMajor[128], szMinor[128], szName[128];
   if (!EffectNameFromGUIDs(dwRenderShard, &t.gEffectCode, &t.gEffectSub, szMajor, szMinor, szName)) {
      szMajor[0] = szMinor[0] = szName[0] = 0;
      t.gEffectCode = t.gEffectSub = GUID_NULL;
   }
   wcscpy (t.szMajor, szMajor);
   wcscpy (t.szMinor, szMinor);
   wcscpy (t.szName, szName);

redopage:
   // create a bitmap for error
   HBITMAP hBlank;
   HDC hDCBlank, hDCWnd;
   hDCWnd = GetDC (hWnd);
   hDCBlank = CreateCompatibleDC (hDCWnd);
   hBlank = CreateCompatibleBitmap (hDCWnd, TEXTURETHUMBNAIL, TEXTURETHUMBNAIL);
   SelectObject (hDCBlank, hBlank);
   ReleaseDC (hWnd, hDCWnd);
   RECT rBlank;
   rBlank.left = rBlank.top = 0;
   rBlank.right = rBlank.bottom = TEXTURETHUMBNAIL;
   FillRect (hDCBlank, &rBlank, (HBRUSH) GetStockObject (BLACK_BRUSH));
   // draw the text
   HFONT hFont;
   LOGFONT  lf;
   memset (&lf, 0, sizeof(lf));
   lf.lfHeight = -MulDiv(12, GetDeviceCaps(hDCBlank, LOGPIXELSY), 72); 
   lf.lfCharSet = DEFAULT_CHARSET;
   lf.lfPitchAndFamily = FF_SWISS | VARIABLE_PITCH;
   strcpy (lf.lfFaceName, "Arial");
   hFont = CreateFontIndirect (&lf);
   SelectObject (hDCBlank, hFont);
   SetTextColor (hDCBlank, RGB(0xff,0xff,0xff));
   SetBkMode (hDCBlank, TRANSPARENT);
   DrawText(hDCBlank, "Drawing...", -1, &rBlank, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_SINGLELINE);
   DeleteObject (hFont);
   DeleteDC (hDCBlank);

   // create the page
   CListFixed  lNames;
   DWORD i;
   PWSTR psz;
   BOOL fBold;
   MemZero (&gMemTemp);
   MemCat (&gMemTemp, L"<?Include resource=500?>"
            L"<PageInfo index=false title=\"Change the effect\"/>"
            L"<colorblend tcolor=#000040 bcolor=#000080 posn=background/>"
            L"<font color=#ffffff><table width=100%% innerlines=0 bordercolor=#c0c0c0 valign=top><tr>");

   // major category
   MemCat (&gMemTemp, L"<td bgcolor=#000000 lrmargin=0 tbmargin=0 width=");
   MemCat (&gMemTemp, (int) 100 / (int) dwColumns);
   MemCat (&gMemTemp, L"%%>");
   MemCat (&gMemTemp, L"<table width=100%% border=0 innerlines=0>");
   EffectEnumMajor (dwRenderShard, &lNames);
   if (t.szMajor[0]) {  // BUGFIX - was !IsEqualGUID (t.gEffectCode, GUID_NULL)
      // make sure major is valid
      for (i = 0; i < lNames.Num(); i++) {
         psz = *((PWSTR*) lNames.Get(i));
         if (!_wcsicmp(psz, t.szMajor))
            break;
      }
      if (i >= lNames.Num())
         wcscpy (t.szMajor, *((PWSTR*) lNames.Get(0)));
   }

   // add the solid color
   MemCat (&gMemTemp, L"<tr><td");
   if (!t.szMajor[0]) { // BUGFIX - Was IsEqualGUID (t.gEffectCode, GUID_NULL)
      MemCat (&gMemTemp, L" bgcolor=#101020");
      fBold = TRUE;
   }
   else
      fBold = FALSE;

   MemCat (&gMemTemp, L">");
   MemCat (&gMemTemp, L"<a href=\"mcat:\">");
   if (fBold)
      MemCat (&gMemTemp, L"<font color=#ffffff>");
   MemCat (&gMemTemp, L"<italic>No effect</italic>");
   if (fBold)
      MemCat (&gMemTemp, L"</font>");
   MemCat (&gMemTemp, L"</a>");
   MemCat (&gMemTemp, L"</td></tr>");

   // add Effects
   for (i = 0; i < lNames.Num(); i++) {
      psz = *((PWSTR*) lNames.Get(i));

      MemCat (&gMemTemp, L"<tr><td");
      if (!_wcsicmp(psz, t.szMajor)) {
            // BUGFIX - Take out !IsEqualGUID (t.gEffectCode, GUID_NULL) && 
         MemCat (&gMemTemp, L" bgcolor=#101020");
         fBold = TRUE;
      }
      else
         fBold = FALSE;

      MemCat (&gMemTemp, L">");
      MemCat (&gMemTemp, L"<a href=\"mcat:");
      MemCatSanitize (&gMemTemp, psz);
      MemCat (&gMemTemp, L"\">");
      if (fBold)
         MemCat (&gMemTemp, L"<font color=#ffffff>");
      MemCatSanitize (&gMemTemp, psz);
      if (fBold)
         MemCat (&gMemTemp, L"</font>");
      MemCat (&gMemTemp, L"</a>");
      MemCat (&gMemTemp, L"</td></tr>");
   }
   MemCat (&gMemTemp, L"</table>");
   MemCat (&gMemTemp, L"</td>");

   if (!t.szMajor[0]) { // BUGFIX - Was IsEqualGUID (t.gEffectCode, GUID_NULL)
      MemCat (&gMemTemp, L"<td bgcolor=#101020 width=");
      MemCat (&gMemTemp, (int) 100 * (int)(dwColumns - 1) / (int) dwColumns);
      MemCat (&gMemTemp, L"%%>");

      MemCat (&gMemTemp, L"<a href=noeffect>No effect is used.</a>");

      MemCat (&gMemTemp, L"</td>");
   }
   else {  // suing Effect map
      // minor categories
      MemCat (&gMemTemp, L"<td lrmargin=0 bgcolor=#101020 tbmargin=0 width=");
      MemCat (&gMemTemp, (int) 100 / (int) dwColumns);
      MemCat (&gMemTemp, L"%%>");
      MemCat (&gMemTemp, L"<table width=100%% border=0 innerlines=0>");
      EffectEnumMinor (dwRenderShard, &lNames, t.szMajor);
      // make sure minor is valid
      for (i = 0; i < lNames.Num(); i++) {
         psz = *((PWSTR*) lNames.Get(i));
         if (!_wcsicmp(psz, t.szMinor))
            break;
      }
      if ((i >= lNames.Num()) && lNames.Num())
         wcscpy (t.szMinor, *((PWSTR*) lNames.Get(0)));
      for (i = 0; i < lNames.Num(); i++) {
         psz = *((PWSTR*) lNames.Get(i));

         MemCat (&gMemTemp, L"<tr><td");
         if (!_wcsicmp(psz, t.szMinor)) {
            MemCat (&gMemTemp, L" bgcolor=#202040");
            fBold = TRUE;
         }
         else
            fBold = FALSE;

         MemCat (&gMemTemp, L">");
         MemCat (&gMemTemp, L"<a href=\"icat:");
         MemCatSanitize (&gMemTemp, psz);
         MemCat (&gMemTemp, L"\">");
         if (fBold)
            MemCat (&gMemTemp, L"<font color=#ffffff>");
         MemCatSanitize (&gMemTemp, psz);
         if (fBold)
            MemCat (&gMemTemp, L"</font>");
         MemCat (&gMemTemp, L"</a>");
         MemCat (&gMemTemp, L"</td></tr>");
      }
      MemCat (&gMemTemp, L"</table>");
      MemCat (&gMemTemp, L"</td>");

      // Effects within major and minor
      MemCat (&gMemTemp, L"<td lrmargin=0 tbmargin=0 bgcolor=#202040 width=");
      MemCat (&gMemTemp, (int) 100 * (int)(dwColumns - 2) / (int) dwColumns);
      MemCat (&gMemTemp, L"%%>");
      MemCat (&gMemTemp, L"<table width=100%% border=0 innerlines=0>");
      EffectEnumItems (dwRenderShard, &lNames, t.szMajor, t.szMinor);
      // make sure minor is valid
      for (i = 0; i < lNames.Num(); i++) {
         psz = *((PWSTR*) lNames.Get(i));
         if (!_wcsicmp(psz, t.szName))
            break;
      }
      if ((i >= lNames.Num()) && lNames.Num())
         wcscpy (t.szName, *((PWSTR*) lNames.Get(0)));
      // fill in the list of bitmaps
      lThumbInfo.Clear();
      DWORD j;
      for (i = 0; i < lNames.Num(); ) {
         MemCat (&gMemTemp, L"<tr>");
         for (j = 0; j < dwColumns-2; j++) {
            MemCat (&gMemTemp, L"<td width=");
            MemCat (&gMemTemp, (int) 100 / (int)(dwColumns - 2));
            MemCat (&gMemTemp, L"%%>");

            if (i + j < lNames.Num()) {
               psz = *((PWSTR*) lNames.Get(i+j));
               MemCat (&gMemTemp, L"<p align=center>");

               THUMBINFO ti;
               ti.fEmpty = TRUE;
               EffectGUIDsFromName (dwRenderShard, t.szMajor, t.szMinor, psz, &t.gCode, &t.gSub);
               ti.hBitmap = ThumbnailGet()->ThumbnailToBitmap (&t.gCode, &t.gSub, cWindow.m_hWnd, &ti.cTransparent);
               ti.pszName = *((PWSTR*) lNames.Get(i+j));
               if (!ti.hBitmap)
                  ti.cTransparent = 0;
               lThumbInfo.Add (&ti);

               // bitmap
               MemCat (&gMemTemp, L"<image hbitmap=");
               WCHAR szTemp[32];
               swprintf (szTemp, L"%lx", ti.hBitmap ? (__int64) ti.hBitmap : (__int64)hBlank);
               MemCat (&gMemTemp, szTemp);
               if (ti.cTransparent != -1) {
                  MemCat (&gMemTemp, L" transparent=true transparentdistance=0 transparentcolor=");
                  ColorToAttrib (szTemp, ti.cTransparent);
                  MemCat (&gMemTemp, szTemp);
               }

               MemCat (&gMemTemp, L" name=bitmap");
               MemCat (&gMemTemp, (int) i+j);
               // Show if it the current Effect
               if (!_wcsicmp(szName, psz) && !_wcsicmp(szMajor, t.szMajor) && !_wcsicmp(szMinor, t.szMinor))
                  MemCat (&gMemTemp, L" border=4 bordercolor=#ff0000");
               else
                  MemCat (&gMemTemp, L" border=2 bordercolor=#000000");
               MemCat (&gMemTemp, L" href=\"obj:");
               MemCatSanitize (&gMemTemp, psz);
               MemCat (&gMemTemp, L"\"/>");

               MemCat (&gMemTemp, L"<br/>");
               MemCat (&gMemTemp, L"<small><a color=#ffffff href=\"obj:");
               MemCatSanitize (&gMemTemp, psz);
               MemCat (&gMemTemp, L"\">");
               MemCatSanitize (&gMemTemp, psz);
               MemCat (&gMemTemp, L"</a></small>");
               MemCat (&gMemTemp, L"</p>");
            }
         
            MemCat (&gMemTemp, L"</td>");
         }
         MemCat (&gMemTemp, L"</tr>");
         i += dwColumns-2;
      }
      MemCat (&gMemTemp, L"</table>");
      MemCat (&gMemTemp, L"</td>");
      }  // if pSurf->m_fEffectMap

   // finish off the row
   MemCat (&gMemTemp, L"</tr>");
   MemCat (&gMemTemp, L"</table>");

   // add button for clearing
   MemCat (&gMemTemp, L"<p/><xbr/>"
      L"<xchoicebutton style=righttriangle href=clearcache>"
      L"<bold>Refresh thumbnails</bold><br/>"
      L"The thumbnails you see are generated the first time they're needed and then "
      L"saved to disk. From then on the saved images are used. If you install a new "
      L"version of " APPLONGNAMEW L", some effects may have changed; you can "
      L"press this button to refresh all your thumbnails. Otherise, ignore it."
      L"</xchoicebutton>");
   MemCat (&gMemTemp, L"</font>");

   // start with the first page
   pszRet = cWindow.PageDialog (ghInstance, (PWSTR) gMemTemp.p, EffectSelPage, &t);
   t.iVScroll = cWindow.m_iExitVScroll;

   // free bitmaps
   if (hBlank)
      DeleteObject (hBlank);
   for (i = 0; i < lThumbInfo.Num(); i++) {
      PTHUMBINFO pti = (PTHUMBINFO) lThumbInfo.Get(i);
      if (pti->hBitmap)
         DeleteObject (pti->hBitmap);
   }


   if (!pszRet)
      goto alldone;
   if (!_wcsicmp(pszRet, RedoSamePage()))
      goto redoscheme;
   else if (!_wcsicmp(pszRet, L"clearcache")) {
      ThumbnailGet()->ThumbnailClearAll (FALSE);
      goto redopage;
   }
   else if (!wcsncmp(pszRet, pszMajorCat, dwMajorCat)) {
      wcscpy (t.szMajor, pszRet + dwMajorCat);

      // handle switching between Effect map and non-Effect map
      if (!t.szMajor[0]) {
         t.szMinor[0] = 0;

         if (!IsEqualGUID (t.gEffectCode, GUID_NULL))
            t.fChanged = TRUE;
         t.gEffectCode = t.gEffectSub = GUID_NULL;
      }
      else {
         t.szMinor[0] = 0; // so will find minor category

         if (!IsEqualGUID (t.gEffectCode, GUID_NULL))
            t.fChanged = TRUE;
      }

      goto redopage;
   }
   else if (!wcsncmp(pszRet, pszMinorCat, dwMinorCat)) {
      wcscpy (t.szMinor, pszRet + dwMinorCat);
      goto redopage;
   }
   else if (!wcsncmp(pszRet, pszNameCat, dwNameCat)) {
      BOOL fControl = (GetKeyState (VK_CONTROL) < 0);
      wcscpy (t.szName, pszRet + dwNameCat);
      // rmember the names in case changes
      wcscpy (szMajor, t.szMajor);
      wcscpy (szMinor, t.szMinor);
      wcscpy (szName, t.szName);
      t.fChanged = TRUE;

      EffectGUIDsFromName (dwRenderShard, szMajor, szMinor, szName, &t.gEffectCode, &t.gEffectSub);

      if (fControl)  // select, but stay in same page
         goto redopage;
      else
         goto alldone;
   }
   
alldone:
   if (t.fChanged) {
      *pgEffectCode = t.gEffectCode;
      *pgEffectSub = t.gEffectSub;
   }
   return t.fChanged;
}


// BUGBUG - will need a way to transfer effects between computers. Do this
// when do transferring of poses
