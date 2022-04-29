/**********************************************************************
Favorites.cpp - Code that handles the calendar UI.

begun 2-Feb-2001 by Mike Rozak
Copyright 2001 Mike Rozak. All rights reserved
*/

#include <windows.h>
#include <tapi.h>
#include "escarpment.h"
#include "dragonfly.h"
#include "resource.h"


typedef struct {
   WCHAR    szName[64];       // name of the favorite
   WCHAR    szDescription[128];  // short description of the favorite
   WCHAR    szAccel[2];       // accelerator, like "A" for alt-A. Or 0 if none
   WCHAR    szLink[128];      // what link is used
   COLORREF color;            // RGB color to display
} FAVORITE, *PFAVORITE;

/* globals */
static BOOL gfFavoritesInit = FALSE;
static CListFixed glistFavorites;      // list of FAVORITE structures

static WCHAR gszFavoritesNode[] = L"FavoritesNode";
static WCHAR gszFavorite[] = L"Favorite";
static WCHAR gszAccelerator[] = L"Accelerator";
static WCHAR gszLink[] = L"Link";
static WCHAR gszColor[] = L"Color";


/**********************************************************************
FavoritesDefault - Set the favorites list back to their defaults.
*/
void FavoritesDefault (void)
{
   HANGFUNCIN;
   FavoritesInit();
   glistFavorites.Clear();

   // add it in
   FAVORITE f;
   memset (&f, 0, sizeof(f));

   wcscpy (f.szName, L"Planner");
   wcscpy (f.szAccel, L"p");
   wcscpy (f.szDescription, L"Lets you plan your day down to the minute.");
   wcscpy (f.szLink, L"r:288");
   f.color = RGB(0x80, 0x80, 0xff);
   glistFavorites.Add (&f);

   wcscpy (f.szName, L"Combo view");
   wcscpy (f.szAccel, L"c");
   wcscpy (f.szDescription, L"A monthly calendar.");
   wcscpy (f.szLink, L"r:258");
   f.color = RGB(0x80, 0x80, 0xff);
   glistFavorites.Add (&f);

   wcscpy (f.szName, L"List");
   wcscpy (f.szAccel, L"l");
   wcscpy (f.szDescription, L"A list of meetings, tasks, and reminders over the next week or month.");
   wcscpy (f.szLink, L"r:239");
   f.color = RGB(0x80, 0x80, 0xff);
   glistFavorites.Add (&f);

   wcscpy (f.szName, L"Today");
   wcscpy (f.szAccel, L"t");
   wcscpy (f.szDescription, L"What you have scheduled for today.");
   wcscpy (f.szLink, L"r:109");
   f.color = RGB(0x80, 0x80, 0xff);
   glistFavorites.Add (&f);

   wcscpy (f.szName, L"");
   wcscpy (f.szAccel, L"");
   wcscpy (f.szDescription, L"");
   wcscpy (f.szLink, L"");
   f.color = RGB(0x80, 0x80, 0xff);
   glistFavorites.Add (&f);


   wcscpy (f.szName, L"Meetings");
   wcscpy (f.szAccel, L"m");
   wcscpy (f.szDescription, L"Schedule meetings.");
   wcscpy (f.szLink, L"r:113");
   f.color = RGB(0x80, 0x80, 0xff);
   glistFavorites.Add (&f);

   wcscpy (f.szName, L"Reminders");
   wcscpy (f.szAccel, L"r");
   wcscpy (f.szDescription, L"See and set reminders.");
   wcscpy (f.szLink, L"r:114");
   f.color = RGB(0x80, 0x80, 0xff);
   glistFavorites.Add (&f);

   wcscpy (f.szName, L"Tasks");
   wcscpy (f.szAccel, L"k");
   wcscpy (f.szDescription, L"View and edit your task list.");
   wcscpy (f.szLink, L"r:115");
   f.color = RGB(0x80, 0x80, 0xff);
   glistFavorites.Add (&f);

   wcscpy (f.szName, L"Projects");
   wcscpy (f.szAccel, L"e");
   wcscpy (f.szDescription, L"Manage your projects.");
   wcscpy (f.szLink, L"r:145");
   f.color = RGB(0x80, 0x80, 0xff);
   glistFavorites.Add (&f);

   wcscpy (f.szName, L"");
   wcscpy (f.szAccel, L"");
   wcscpy (f.szDescription, L"");
   wcscpy (f.szLink, L"");
   f.color = RGB(0x80, 0x80, 0xff);
   glistFavorites.Add (&f);



   wcscpy (f.szName, L"Address book");
   wcscpy (f.szAccel, L"a");
   wcscpy (f.szDescription, L"Your address book.");
   wcscpy (f.szLink, L"r:142");
   f.color = RGB(0x80, 0x80, 0xff);
   glistFavorites.Add (&f);

   wcscpy (f.szName, L"People");
   wcscpy (f.szAccel, L"");
   wcscpy (f.szDescription, L"Add and remove people.");
   wcscpy (f.szLink, L"r:118");
   f.color = RGB(0x80, 0x80, 0xff);
   glistFavorites.Add (&f);

   wcscpy (f.szName, L"Phone");
   wcscpy (f.szAccel, L"h");
   wcscpy (f.szDescription, L"Make phone calls.");
   wcscpy (f.szLink, L"r:117");
   f.color = RGB(0x80, 0x80, 0xff);
   glistFavorites.Add (&f);

   wcscpy (f.szName, L"Incoming call");
   wcscpy (f.szAccel, L"i");
   wcscpy (f.szDescription, L"Click this if someone just rang and you want to take notes on the call.");
   wcscpy (f.szLink, L"takenotes");
   f.color = RGB(0xb0, 0x80, 0xff);
   glistFavorites.Add (&f);

   wcscpy (f.szName, L"");
   wcscpy (f.szAccel, L"");
   wcscpy (f.szDescription, L"");
   wcscpy (f.szLink, L"");
   f.color = RGB(0x80, 0x80, 0xff);
   glistFavorites.Add (&f);



   wcscpy (f.szName, L"Notes");
   wcscpy (f.szAccel, L"n");
   wcscpy (f.szDescription, L"Take a quick note.");
   wcscpy (f.szLink, L"r:116");
   f.color = RGB(0x80, 0x80, 0xff);
   glistFavorites.Add (&f);

   wcscpy (f.szName, L"Journal");
   wcscpy (f.szAccel, L"j");
   wcscpy (f.szDescription, L"Record an idea, information, or work done.");
   wcscpy (f.szLink, L"r:121");
   f.color = RGB(0x80, 0x80, 0xff);
   glistFavorites.Add (&f);

   wcscpy (f.szName, L"Archive");
   wcscpy (f.szAccel, L"");
   wcscpy (f.szDescription, L"Remember the text from web pages you visit.");
   wcscpy (f.szLink, L"r:124");
   f.color = RGB(0x80, 0x80, 0xff);
   glistFavorites.Add (&f);

   wcscpy (f.szName, L"");
   wcscpy (f.szAccel, L"");
   wcscpy (f.szDescription, L"");
   wcscpy (f.szLink, L"");
   f.color = RGB(0x80, 0x80, 0xff);
   glistFavorites.Add (&f);


   wcscpy (f.szName, L"Daily wrap-up");
   wcscpy (f.szAccel, L"");
   wcscpy (f.szDescription, L"Re-examine what you accomplished today.");
   wcscpy (f.szLink, L"r:220");
   f.color = RGB(0x80, 0x80, 0xff);
   glistFavorites.Add (&f);

   wcscpy (f.szName, L"Memory lane");
   wcscpy (f.szAccel, L"");
   wcscpy (f.szDescription, L"Remember your past.");
   wcscpy (f.szLink, L"r:122");
   f.color = RGB(0x80, 0x80, 0xff);
   glistFavorites.Add (&f);

   wcscpy (f.szName, L"");
   wcscpy (f.szAccel, L"");
   wcscpy (f.szDescription, L"");
   wcscpy (f.szLink, L"");
   f.color = RGB(0x80, 0x80, 0xff);
   glistFavorites.Add (&f);

   wcscpy (f.szName, L"Search");
   wcscpy (f.szAccel, L"s");
   wcscpy (f.szDescription, L"Search through your records or through the documentation.");
   wcscpy (f.szLink, L"r:128");
   f.color = RGB(0x80, 0x80, 0xff);
   glistFavorites.Add (&f);

   wcscpy (f.szName, L"Documentation");
   wcscpy (f.szAccel, L"");
   wcscpy (f.szDescription, L"Documentation and miscellaneous settings.");
   wcscpy (f.szLink, L"r:129");
   f.color = RGB(0x80, 0x80, 0xff);
   glistFavorites.Add (&f);

   wcscpy (f.szName, L"Table of contents");
   wcscpy (f.szAccel, L"");
   wcscpy (f.szDescription, L"Use this to access all of Dragonfly features.");
   wcscpy (f.szLink, L"r:252");
   f.color = RGB(0x80, 0x80, 0xff);
   glistFavorites.Add (&f);

   wcscpy (f.szName, L"");
   wcscpy (f.szAccel, L"");
   wcscpy (f.szDescription, L"");
   wcscpy (f.szLink, L"");
   f.color = RGB(0x80, 0x80, 0xff);
   glistFavorites.Add (&f);

   // save it
   FavoritesSave();
}


/**********************************************************************
FavoritesInit - Load in the favorites list from the database if it
isn't already.
*/
void FavoritesInit (void)
{
   HANGFUNCIN;
   if (gfFavoritesInit)
      return;
   gfFavoritesInit = TRUE;
   glistFavorites.Init (sizeof(FAVORITE));

   // else get it
   PCMMLNode   pNode;
   DWORD dwNum;
   pNode = FindMajorSection (gszFavoritesNode, &dwNum);
   if (!pNode)
      return;   // error. This shouldn't happen

   // read in other tasks
   DWORD i;
   PCMMLNode pSub;
   PWSTR psz;
   dwNum = pNode->ContentNum();
   for (i = 0; i < dwNum; i++) {
      pSub = NULL;
      pNode->ContentEnum(i, &psz, &pSub);
      if (!pSub)
         continue;
      if (_wcsicmp(pSub->NameGet(), gszFavorite))
         continue;   // not looking for this

      // have a favorites node, so get the values
      FAVORITE f;
      memset (&f, 0, sizeof(f));
      psz = NodeValueGet (pSub, gszName);
      if (psz)
         wcscpy (f.szName, psz);
      psz = NodeValueGet (pSub, gszDescription);
      if (psz)
         wcscpy (f.szDescription, psz);
      psz = NodeValueGet (pSub, gszAccelerator);
      if (psz)
         wcscpy (f.szAccel, psz);
      psz = NodeValueGet (pSub, gszLink);
      if (psz)
         wcscpy (f.szLink, psz);
      f.color = NodeValueGetInt (pSub, gszColor, 0);

      glistFavorites.Add (&f);
   }

   gpData->Flush();
   gpData->NodeRelease(pNode);

   // if nothing there then default
   if (!glistFavorites.Num())
      FavoritesDefault();

}

/**********************************************************************
FavoritesSave - Save everything in the favorites list to disk.
*/
void FavoritesSave (void)
{
   HANGFUNCIN;
   if (!gfFavoritesInit)
      return;

   // else get it
   PCMMLNode   pNode;
   DWORD dwNum;
   pNode = FindMajorSection (gszFavoritesNode, &dwNum);
   if (!pNode)
      return;   // error. This shouldn't happen

   // remove existing favorites
   DWORD i;
   PCMMLNode   pSub;
   PWSTR psz;
   dwNum = pNode->ContentNum();
   for (i = dwNum - 1; i < dwNum; i--) {
      pSub = NULL;
      pNode->ContentEnum(i, &psz, &pSub);
      if (!pSub)
         continue;
      if (_wcsicmp(pSub->NameGet(), gszTask) && _wcsicmp(pSub->NameGet(), gszFavorite))
         continue;

      // else, old task. delete
      pNode->ContentRemove (i);
   }

   // write it out
   for (i = 0; i < glistFavorites.Num(); i++) {
      PFAVORITE pf = (PFAVORITE) glistFavorites.Get(i);
      if (!pf)
         continue;

      pSub = pNode->ContentAddNewNode ();
      if (!pSub)
         continue;
      pSub->NameSet (gszFavorite);

      // write it in
      NodeValueSet (pSub, gszName, pf->szName);
      NodeValueSet (pSub, gszDescription, pf->szDescription);
      NodeValueSet (pSub, gszAccelerator, pf->szAccel);
      NodeValueSet (pSub, gszLink, pf->szLink);
      NodeValueSet (pSub, gszColor, pf->color);
   }

   gpData->Flush();
   gpData->NodeRelease(pNode);


}




#if 0
/***************************************************************************************8
FavoritesMakeSureTOC - Make sure that at least one of the menu items can get to the TOC.
If not, then add it on.
*/
void FavoritesMakeSureTOC (void)
{
   HANGFUNCIN;
   FAVORITE f;
   PFAVORITE pf;

   memset (&f, 0, sizeof(f));
   wcscpy (f.szName, L"TOC");
   wcscpy (f.szAccel, L"");
   wcscpy (f.szDescription, L"Table of contents.");
   wcscpy (f.szLink, L"r:252");
   f.color = RGB(0x80, 0x80, 0xff);

   // loop
   DWORD i;
   for (i = 0; i < glistFavorites.Num(); i++) {
      pf = (PFAVORITE) glistFavorites.Get (i);
      if (!_wcsicmp(pf->szLink, f.szLink) && pf->szName[0])
         return;  // found
   }

   // if get here didn't find
   glistFavorites.Add (&f);

   // save it
   FavoritesSave();
}
#endif // 0

/***************************************************************************************8
FavoritesMenuSub - Create the substitution for the main menu based on favorites.

inputs
   BOOL     fRight - If TRUE, is assumed to be on the right, so align up/down
returns
   PWSTR - Substitution string. Pointer to gMemTemp.p
*/
PWSTR FavoritesMenuSub (BOOL fRight)
{
   HANGFUNCIN;
   MemZero (&gMemTemp);

   FavoritesInit();

   // FavoritesMakeSureTOC ();

   DWORD dwRows, dwColumns, dwNum, x, y;
   dwNum = glistFavorites.Num();
   dwColumns = fRight ? 1 : 5;
   dwRows = (dwNum + dwColumns - 1) / dwColumns;

   if (!fRight)
      MemCat (&gMemTemp, L"<tr>");
   for (x = 0; x < dwColumns; x++) {
      if (!fRight)
         MemCat (&gMemTemp, L"<td>");

      for (y = 0; y < dwRows; y++) {
         PFAVORITE pf = (PFAVORITE) glistFavorites.Get(x + y * dwColumns);
         if (!pf)
            continue;

         // if it's empty then insert a blank line
         if (!pf->szName[0] || !pf->szLink[0])
            goto blankline;

         WCHAR szTemp[128];
         // link
         MemCat (&gMemTemp, L"<a href=\"");
         MemCatSanitize (&gMemTemp, pf->szLink);
         MemCat (&gMemTemp, L"\" color=");
         ColorToAttrib (szTemp, pf->color ? pf->color : RGB(0x80,0x80,0xff));
         MemCat (&gMemTemp, szTemp);
         if (pf->szAccel[0]) {
            MemCat (&gMemTemp, L" accel=alt-");
            MemCatSanitize (&gMemTemp, pf->szAccel);
         }
         MemCat (&gMemTemp, L">");

         // text
         DWORD dwCur, dwFound;
         dwFound = (DWORD)-1;
         WCHAR c;
         c = towlower (pf->szAccel[0]);
         if (c) for (dwCur = 0; pf->szName[dwCur]; dwCur++) {
            if (towlower(pf->szName[dwCur]) == c) {
               dwFound = dwCur;
               break;
            }
         }
         if (dwFound == (DWORD)-1) {
            // accelerator not listed, so don't need to bold
            MemCatSanitize (&gMemTemp, pf->szName);
         }
         else {
            // put bold around accelerator
            wcscpy (szTemp, pf->szName);
            szTemp[dwFound] = 0;
            MemCatSanitize (&gMemTemp, szTemp);
            MemCat (&gMemTemp, L"<bold>");
            szTemp[0] = pf->szName[dwFound];
            szTemp[1] = 0;
            MemCatSanitize (&gMemTemp, szTemp);
            MemCat (&gMemTemp, L"</bold>");
            wcscpy (szTemp, pf->szName + (dwFound + 1));
            MemCatSanitize (&gMemTemp, szTemp);
         }

         // is the hover help long?
         DWORD dwHover;
         dwHover = 0;
         if (pf->szAccel[0])
            dwHover++;
         if (pf->szDescription[0])
            dwHover++;
         if (wcslen(pf->szDescription) > 50)
            dwHover++;
         if (dwHover) {
            MemCat (&gMemTemp, (dwHover > 1) ? L"<xhoverhelp>" : L"<xhoverhelpshort>");

            // put in description
            if (pf->szDescription)
               MemCatSanitize (&gMemTemp, pf->szDescription);

            // and tip for accleratro
            if (pf->szAccel[0]) {
               if (pf->szDescription)
                  MemCat (&gMemTemp, L" ");
               MemCat (&gMemTemp, L"You can also type Alt+");
               MemCatSanitize (&gMemTemp, pf->szAccel);
               MemCat (&gMemTemp, L" to get to this page.");
            }

              
            MemCat (&gMemTemp, (dwHover > 1) ? L"</xhoverhelp>" : L"</xhoverhelpshort>");
         }

         // end link
         MemCat (&gMemTemp, L"</a>");

blankline:
         MemCat (&gMemTemp, L"<br/>");
      }

      if (!fRight)
         MemCat (&gMemTemp, L"</td>");
   }
   if (!fRight)
      MemCat (&gMemTemp, L"</tr>");

   return (PWSTR) gMemTemp.p;
}


/*****************************************************************************
FavoritesAddPage - Override page callback.
*/
BOOL FavoritesAddPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   HANGFUNCIN;
   PFAVORITE pf = (PFAVORITE) pPage->m_pUserData;

   switch (dwMessage) {

   case ESCM_INITPAGE:
      {
         PCEscControl pControl;
         pControl = pPage->ControlFind (gszName);
         if (pControl)
            pControl->AttribSet (gszText, pf->szName);
         pControl = pPage->ControlFind (gszDescription);
         if (pControl)
            pControl->AttribSet (gszText, pf->szDescription);
         pControl = pPage->ControlFind (gszAccelerator);
         if (pControl)
            pControl->AttribSet (gszText, pf->szAccel);
      }
      break;   // default

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;

         // only handle button press of "addproject"
         if (!p->pControl || !p->pControl->m_pszName)
            break;

         if (!_wcsicmp(p->pControl->m_pszName, L"add")) {
            // pressed add. Do some verification
            PCEscControl pControl;
            DWORD dwNeeded;

            // name
            WCHAR szTemp[64];
            pControl = pPage->ControlFind(gszName);
            if (pControl)
               pControl->AttribGet (gszText, szTemp, sizeof(szTemp),&dwNeeded);
            if (!szTemp) {
               pPage->MBWarning (L"You must type in a name for the menu item.");
               return TRUE;
            }
            wcscpy (pf->szName, szTemp);

            pControl = pPage->ControlFind(gszDescription);
            if (pControl)
               pControl->AttribGet (gszText, pf->szDescription, sizeof(pf->szDescription),&dwNeeded);
            pControl = pPage->ControlFind(gszAccelerator);
            if (pControl)
               pControl->AttribGet (gszText, pf->szAccel, sizeof(pf->szAccel),&dwNeeded);

            // exit this
            pPage->Exit (gszOK);
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}

/**************************************************************************************
FavoritesMenuAdd - Add the current page to the favorites list.

inputs
   PCEscPage      pPage - page
returns
   BOOL - TRUE if it was added, FALSE if not
*/
BOOL FavoritesMenuAdd (PCEscPage pPage)
{
   HANGFUNCIN;
   CEscWindow  cWindow;
   RECT  r;

   // make sure it's all initialized
   FavoritesInit ();
   FAVORITE f;
   memset (&f, 0, sizeof(f));
   char szTemp[64];
   szTemp[0] = 0;
   GetWindowText (pPage->m_pWindow->m_hWnd, szTemp, sizeof(szTemp));
   MultiByteToWideChar (CP_ACP, 0, szTemp, -1, f.szName, sizeof(f.szName)/2);
   // figure out accel string
   wcscpy (f.szLink, gCurHistory.szLink);
   f.color = RGB(0x80, 0x80, 0xff);


   DialogRect (&r);
   cWindow.Init (ghInstance, pPage->m_pWindow->m_hWnd,
      EWS_TITLE | EWS_FIXEDSIZE | EWS_VSCROLL | EWS_AUTOHEIGHT | EWS_FIXEDWIDTH, &r);
   PWSTR pszRet;
   pszRet = cWindow.PageDialog (ghInstance, IDR_MMLFAVORITESADD, FavoritesAddPage, &f);

   // add this
   if (pszRet && !_wcsicmp(pszRet, gszOK)) {
      // add it
      glistFavorites.Add (&f);
      FavoritesSave ();

      return TRUE;
   }
   else
      return FALSE;
}


/*****************************************************************************
FavoritesPage - Override page callback.
*/
BOOL FavoritesPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   HANGFUNCIN;
   switch (dwMessage) {
   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         // only go beyond this point if have task list
         if (!_wcsicmp(p->pszSubName, L"MENULIST")) {
            // if there aren't any tasks then done
            if (!glistFavorites.Num()) {
               p->pszSubString = L"";
               return TRUE;
            }

            // else there are tasks, so display the info
            MemZero (&gMemTemp);
            MemCat (&gMemTemp, L"<xtablecenter innerlines=0 width=60%%>");
            MemCat (&gMemTemp, L"<xtrheader>Menu items</xtrheader>");

            // all the menu items
            DWORD i;
            PFAVORITE pf;
            DWORD dwNum;
            BOOL fFoundTOC;
            fFoundTOC = FALSE;
            dwNum = glistFavorites.Num();
            for (i = 0; i < dwNum; i++) {
               pf = (PFAVORITE) glistFavorites.Get(i);

               MemCat (&gMemTemp, L"<tr><td width=20%% align=center>");

               // arrows
               if (i) {
                  // up arrow
                  MemCat (&gMemTemp, L"<xUpArrow href=u:");
                  MemCat (&gMemTemp, (int)i);
                  MemCat (&gMemTemp, L"/>");
               }
               else {
                  MemCat (&gMemTemp, L"<xSpaceArrow/>");
               }
               if ((i+1) < dwNum) {
                  // down arrow
                  MemCat (&gMemTemp, L"<xDownArrow href=d:");
                  MemCat (&gMemTemp, (int)i);
                  MemCat (&gMemTemp, L"/>");
               }
               else {
                  MemCat (&gMemTemp, L"<xSpaceArrow/>");
               }

               // X to delete. Can't delete TOC
               MemCat (&gMemTemp, L"  ");
               if (!_wcsicmp (pf->szLink, L"r:252") && !fFoundTOC) {
                  fFoundTOC = TRUE;
                  MemCat (&gMemTemp, L"<xSpaceArrow/>");
               }
               else {
                  // X bitmap
                  MemCat (&gMemTemp, L"<xXbitmap href=x:");
                  MemCat (&gMemTemp, (int)i);
                  MemCat (&gMemTemp, L"/>");
               }

               // BUGFIX - change the color
               MemCat (&gMemTemp, L"<colorblend width=16 height=16 color=");
               WCHAR szTemp[64];
               ColorToAttrib (szTemp, pf->color);
               MemCat (&gMemTemp, szTemp);
               MemCat (&gMemTemp, L" href=q:");
               MemCat (&gMemTemp, (int)i);
               MemCat (&gMemTemp, L"><xhoverhelpshort>Change the color.</xhoverhelpshort></colorblend>");

               MemCat (&gMemTemp, L"</td><td width=80%%>");

               // link
               if (pf->szName[0]) {
                  MemCat (&gMemTemp, L"<a href=!");
                  MemCat (&gMemTemp, (int) i);
                  MemCat (&gMemTemp, L">");
               }

               // task name
               if (pf->szName[0])
                  MemCatSanitize (&gMemTemp, pf->szName);
               else
                  MemCat (&gMemTemp, L"<italic>Blank</italic>");

               if (pf->szName[0]) {
                  MemCat (&gMemTemp, L"</a>");
               }
               
               // description
               if (pf->szDescription[0]) {
                  MemCat (&gMemTemp, L"<small> - ");
                  MemCatSanitize (&gMemTemp, pf->szDescription);
                  MemCat (&gMemTemp, L"</small>");
               }

               if (!_wcsicmp(FavoritesGetStartup (), pf->szLink)) {
                  MemCat (&gMemTemp, L"<bold><small> (Dragonfly will show this page when it starts up.)</small></bold>");
               }

               // finish off task
               MemCat (&gMemTemp, L"</td></tr>");

            }

            // finnish up the table
            MemCat (&gMemTemp, L"</xtablecenter>");
            MemCat (&gMemTemp, L"<xbr/>");
            p->pszSubString = (PWSTR) gMemTemp.p;
            return TRUE;
         }
      }
      break;   // default behavior

   case ESCM_LINK:
      {
         PESCMLINK p = (PESCMLINK) pParam;

         if (!p->psz)
            break;

         // if it's a specialy link for editing
         if (p->psz[0] == L'!') {
            DWORD dwIndex;
            dwIndex = 0;
            AttribToDecimal (p->psz + 1, (int*) &dwIndex);

            CEscWindow  cWindow;
            RECT  r;

            PFAVORITE pf;
            pf = (PFAVORITE) glistFavorites.Get(dwIndex);
            if (!pf)
               return TRUE;

            DialogRect (&r);
            cWindow.Init (ghInstance, pPage->m_pWindow->m_hWnd,
               EWS_TITLE | EWS_FIXEDSIZE | EWS_VSCROLL | EWS_AUTOHEIGHT | EWS_FIXEDWIDTH, &r);
            PWSTR pszRet;
            pszRet = cWindow.PageDialog (ghInstance, IDR_MMLFAVORITESEDIT, FavoritesAddPage, pf);

            // add this
            if (pszRet && !_wcsicmp(pszRet, gszOK)) {
               // add it
               FavoritesSave ();
               pPage->Exit (gszRedoSamePage); // was successfully edited, so refresh
            };

            return TRUE;
         }
         else if ( ((p->psz[0] == L'u') || (p->psz[0] == L'd')) && (p->psz[1] == L':')) {
            // up/down arrow
            int iIndex, iMoveTo;
            iIndex = 0;
            AttribToDecimal (p->psz + 2, &iIndex);
            iMoveTo = iIndex + ((p->psz[0] == L'u') ? -1 : 1);

            // swap
            PFAVORITE pA, pB;
            pA = (PFAVORITE) glistFavorites.Get(iIndex);
            pB = (PFAVORITE) glistFavorites.Get(iMoveTo);

            FAVORITE f;
            f = *pA;
            *pA = *pB;
            *pB = f;

            FavoritesSave();
            pPage->Exit (gszRedoSamePage); // was successfully edited, so refresh

            return TRUE;
         }
         else if ( (p->psz[0] == L'x') && (p->psz[1] == L':')) {
            // up/down arrow
            int iIndex;
            iIndex = 0;
            AttribToDecimal (p->psz + 2, &iIndex);

            if (pPage->MBYesNo (L"Are you sure you wish to delete this item?") != IDYES)
               return TRUE;

            glistFavorites.Remove (iIndex);
            // FavoritesMakeSureTOC();

            // inform user that removed
            EscChime (ESCCHIME_INFORMATION);
            EscSpeak (L"Menu item removed.");

            FavoritesSave();
            pPage->Exit (gszRedoSamePage); // was successfully edited, so refresh

            return TRUE;
         }
         else if ( (p->psz[0] == L'q') && (p->psz[1] == L':')) {
            // BUGFIX - Allow to change color
            int iIndex;
            iIndex = 0;
            AttribToDecimal (p->psz + 2, &iIndex);
            PFAVORITE pf;
            pf = (PFAVORITE) glistFavorites.Get(iIndex);
            if (!pf)
               return TRUE;

            CHOOSECOLOR cc;
            memset (&cc, 0, sizeof(cc));
            cc.lStructSize = sizeof(cc);
            cc.hwndOwner = pPage->m_pWindow->m_hWnd;
            cc.rgbResult = pf->color;
            cc.Flags = CC_FULLOPEN | CC_RGBINIT | CC_SOLIDCOLOR;
            DWORD adwCust[16];
            DWORD j;
            for (j = 0; j < 16; j++)
               adwCust[j] = RGB(j * 16, j*16, j*16);
            cc.lpCustColors = adwCust;
            ChooseColor (&cc);

            // if changed then set status
            if (cc.rgbResult != pf->color) {
               pf->color = cc.rgbResult;
               FavoritesSave();
               pPage->Exit (gszRedoSamePage); // was successfully edited, so refresh
            }

            return TRUE;
         }
      }
      break;   // default behavior

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;

         // only handle button press of "remove"
         if (!p->pControl || !p->pControl->m_pszName)
            break;
         
         if (!_wcsicmp(p->pControl->m_pszName, L"addnewmeeting")) {
            FAVORITE f;
            memset (&f, 0, sizeof(f));
            f.color = RGB(0xb0, 0x80, 0xff);
            wcscpy (f.szName, L"Add meeting");
            wcscpy (f.szDescription, L"Quickly add a meeting.");
            wcscpy (f.szLink, L"qzaddmeeting");
            glistFavorites.Add (&f);

            // inform user that removed
            EscChime (ESCCHIME_INFORMATION);
            EscSpeak (L"Menu item added.");

            FavoritesSave();
            pPage->Exit (gszRedoSamePage); // was successfully edited, so refresh
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"addnewcall")) {
            FAVORITE f;
            memset (&f, 0, sizeof(f));
            f.color = RGB(0xb0, 0x80, 0xff);
            wcscpy (f.szName, L"Schedule call");
            wcscpy (f.szDescription, L"Schedule a phone call.");
            wcscpy (f.szLink, L"qzaddcall");
            glistFavorites.Add (&f);

            // inform user that removed
            EscChime (ESCCHIME_INFORMATION);
            EscSpeak (L"Menu item added.");

            FavoritesSave();
            pPage->Exit (gszRedoSamePage); // was successfully edited, so refresh
            return TRUE;
         }

         else if (!_wcsicmp(p->pControl->m_pszName, L"addnewnote")) {
            FAVORITE f;
            memset (&f, 0, sizeof(f));
            f.color = RGB(0xb0, 0x80, 0xff);
            wcscpy (f.szName, L"Add note");
            wcscpy (f.szDescription, L"Quickly add a note.");
            wcscpy (f.szLink, L"qzaddnote");
            glistFavorites.Add (&f);

            // inform user that removed
            EscChime (ESCCHIME_INFORMATION);
            EscSpeak (L"Menu item added.");

            FavoritesSave();
            pPage->Exit (gszRedoSamePage); // was successfully edited, so refresh
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"addnewperson")) {
            FAVORITE f;
            memset (&f, 0, sizeof(f));
            f.color = RGB(0xb0, 0x80, 0xff);
            wcscpy (f.szName, L"Add person");
            wcscpy (f.szDescription, L"Quickly add a person or business.");
            wcscpy (f.szLink, L"qzaddpersonbusiness");
            glistFavorites.Add (&f);

            // inform user that removed
            EscChime (ESCCHIME_INFORMATION);
            EscSpeak (L"Menu item added.");

            FavoritesSave();
            pPage->Exit (gszRedoSamePage); // was successfully edited, so refresh
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"addnewreminder")) {
            FAVORITE f;
            memset (&f, 0, sizeof(f));
            f.color = RGB(0xb0, 0x80, 0xff);
            wcscpy (f.szName, L"Add reminder");
            wcscpy (f.szDescription, L"Quickly add a reminder.");
            wcscpy (f.szLink, L"qzaddreminder");
            glistFavorites.Add (&f);

            // inform user that removed
            EscChime (ESCCHIME_INFORMATION);
            EscSpeak (L"Menu item added.");

            FavoritesSave();
            pPage->Exit (gszRedoSamePage); // was successfully edited, so refresh
            return TRUE;
         }

         else if (!_wcsicmp(p->pControl->m_pszName, L"addnewtask")) {
            FAVORITE f;
            memset (&f, 0, sizeof(f));
            f.color = RGB(0xb0, 0x80, 0xff);
            wcscpy (f.szName, L"Add task");
            wcscpy (f.szDescription, L"Quickly add a task.");
            wcscpy (f.szLink, L"qzaddtask");
            glistFavorites.Add (&f);

            // inform user that removed
            EscChime (ESCCHIME_INFORMATION);
            EscSpeak (L"Menu item added.");

            FavoritesSave();
            pPage->Exit (gszRedoSamePage); // was successfully edited, so refresh
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"addblank")) {
            FAVORITE f;
            memset (&f, 0, sizeof(f));
            glistFavorites.Add (&f);

            // inform user that removed
            EscChime (ESCCHIME_INFORMATION);
            EscSpeak (L"Menu item added.");

            FavoritesSave();
            pPage->Exit (gszRedoSamePage); // was successfully edited, so refresh
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"addincomingcall")) {
            FAVORITE f;
            memset (&f, 0, sizeof(f));
            wcscpy (f.szName, L"Incoming call");
            wcscpy (f.szAccel, L"i");
            wcscpy (f.szDescription, L"Click this if someone just rang and you want to take notes on the call.");
            wcscpy (f.szLink, L"takenotes");
            f.color = RGB(0xb0, 0x80, 0xff);
            glistFavorites.Add (&f);

            // inform user that removed
            EscChime (ESCCHIME_INFORMATION);
            EscSpeak (L"Menu item added.");

            FavoritesSave();
            pPage->Exit (gszRedoSamePage); // was successfully edited, so refresh
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"defaultmenu")) {
            if (pPage->MBYesNo (L"Are you sure you want to restore the default menu?",
               L"You will lose any changes you have made so far.") != IDYES)
               return TRUE; 

            // inform user that removed
            EscChime (ESCCHIME_INFORMATION);
            EscSpeak (L"Menu restored.");

            // restore
            FavoritesDefault();
            pPage->Exit (gszRedoSamePage); // was successfully edited, so refresh
            return TRUE;
         }



      }
      break;   // default behavior
   };


   return DefPage (pPage, dwMessage, pParam);
}


/****************************************************************************
FavoritesGetStartup - Returns the link to the first menu item so that it can
also be used for the startup page. If that doesn't have a colon in it (which means
it's an action like incoming phone call), then it goes to the next one, etc.

inputs
   none
returns
   PWSTR - name. Don't change this
*/
PWSTR FavoritesGetStartup (void)
{
   HANGFUNCIN;
   FavoritesInit ();

   DWORD i;
   for (i = 0; i < glistFavorites.Num(); i++) {
      PFAVORITE pf = (PFAVORITE) glistFavorites.Get(i);
      if (!pf)
         continue;
      if (wcschr(pf->szLink, L':'))
         return pf->szLink;

      // else continue because couldnt find colon
   }

   // if get here return today
   return gszStartingURL;
}
