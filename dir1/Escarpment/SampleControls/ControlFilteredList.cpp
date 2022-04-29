/***********************************************************************
ControlFilteredList.cpp - Code to get the FilteredList from a user.

begun 5/30/2000 by Mike Rozak
Copyright 2000 mike Rozak. All rights reserved
*/

#include <windows.h>
#include <stdlib.h>
#include <stdio.h>
#include "escarpment.h"
#include "control.h"
#include "resleak.h"



#define  ESCM_UPDATELISTBOX      (ESCM_USER+343)
#define  ESCM_OK                 (ESCM_USER+344)


typedef struct {
   DROPDOWN       dd;
   WCHAR          szBlank[128];   // name used if no name is specified
   PWSTR          pszListName;   // name of the list
   PWSTR          pszAddItem;    // text to display for adding item
   int            iCurSel;    // currently selected item. -1 for none
   BOOL           fSort;         // if TRUE then sort the list

   WCHAR          szCurDisplay[256]; // current FilteredList displayed
   PCEscControl   pControl;    // this control
} FILTEREDLIST, *PFILTEREDLIST;

static WCHAR gszChecked[] = L"checked";



/***********************************************************************
GetList - Returns the pointer to the list from the application.

inputs
   PFILTEREDLIST pc
returns
   PCListVariable - list
*/
static PCListVariable GetList (PFILTEREDLIST pc)
{
   ESCNFILTEREDLISTQUERY q;
   memset (&q, 0, sizeof(q));
   q.pControl = pc->pControl;
   q.pszListName = pc->pszListName;
   pc->pControl->MessageToParent (ESCN_FILTEREDLISTQUERY, &q);

#ifdef _DEBUG
   // for test purposes
   if (!q.pList) {
      static BOOL fListFilled = FALSE;
      static CListVariable list;

      if (!fListFilled) {
         fListFilled = TRUE;

         WCHAR szFillList[] =
            L"Jenny Abernacky\0\0"
            L"Bill Bradburry\0Billy\0"
            L"Jim Briggs\0\0"
            L"Michael Rozak\0Mike\0"
            L"Fred Smith\0Freddie\0"
            L"John Smith\0\0"
            L"Samantha Walters\0\0"
            L"Harry Weatherspoon\0\0"
            L"Carry Wellsford\0\0"
            L"Jone Zeck\0\0"
            L"\0\0";

         PWSTR pCur;
         for (pCur = szFillList; pCur[0]; ) {
            DWORD dwLen;
            dwLen = wcslen(pCur)+1;
            dwLen += wcslen(pCur+dwLen)+1;

            list.Add (pCur, dwLen*2);

            pCur += dwLen;
         }
      }

      q.pList = &list;
   }
#endif
   return q.pList;
}


/***********************************************************************
stristr - Finds an occrance of the string within the other

inputs
   PWSTR    psz - string to search in
   PWSTR    pszFind - string to findr
returns
   PWSTR - NULL if error, else where found
*/
PWSTR stristr (PWSTR psz, PWSTR pszFind)
{
   // lowercase first char
   WCHAR cLower;
   DWORD dwLen;
   cLower = towlower (pszFind[0]);
   dwLen = wcslen(pszFind);

   PWSTR pCur;
   for (pCur = psz; *pCur; pCur++) {
      if (towlower(*pCur) != cLower)
         continue;

      // string compare
      if (wcsnicmp (pCur, pszFind, dwLen))
         continue;

      // else found
      return pCur;
   }

   return NULL;
}


/***********************************************************************
Page callback
*/
BOOL FilteredListCallback (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PFILTEREDLIST pc = (PFILTEREDLIST) pPage->m_pUserData;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         ESCACCELERATOR a;
         memset (&a, 0, sizeof(a));
         a.c = VK_ESCAPE;
         a.dwMessage = ESCM_CLOSE;
         pPage->m_listESCACCELERATOR.Add (&a);
         a.c = L' ';
         a.dwMessage = ESCM_OK;
         pPage->m_listESCACCELERATOR.Add (&a);
         a.c = VK_RETURN;
         pPage->m_listESCACCELERATOR.Add (&a);

         DropDownFitToScreen(pPage);

         // update the list box
         pPage->Message (ESCM_UPDATELISTBOX);
         return TRUE;
      }

   case ESCN_EDITCHANGE:
      // update the list box
      pPage->Message (ESCM_UPDATELISTBOX);
      return TRUE;

   case ESCM_OK:
      {
         PCEscControl pControl;
         pControl = pPage->ControlFind (L"list");
         if (!pControl)
            return FALSE;

         // get the current selection and return that
         ESCMLISTBOXGETITEM i;
         memset (&i, 0, sizeof(i));
         i.dwIndex = (DWORD) pControl->AttribGetInt (L"cursel");
         pControl->Message (ESCM_LISTBOXGETITEM, &i);

         if (i.pszData)
            AttribToDecimal (i.pszData, &pc->iCurSel);

         pPage->Exit (L"selected");
      }
      return TRUE;

   case ESCM_UPDATELISTBOX:
      {
         // get the edit box text
         PCEscControl pControl;
         pControl = pPage->ControlFind (L"edit");
         if (!pControl)
            return FALSE;

         WCHAR szText[128];
         szText[0] = 0;
         DWORD dwNeeded;
         pControl->AttribGet (L"text", szText, sizeof(szText), &dwNeeded);

         // loop through this and find the beginning of each word
         CListFixed lf;
         lf.Init (sizeof(PWSTR));
         PWSTR pCur;
         pCur = szText;
         while (TRUE) {
            while (*pCur == L' ')
               pCur++;

            if (!*pCur)
               break;

            // found the beginning
            lf.Add (&pCur);

            // find the end
            while (*pCur && (*pCur != L' '))
               pCur++;

            // if it's a NULL then break
            if (!*pCur)
               break;

            // else zeroe and move on
            *pCur = 0;
            pCur++;
         }

         // get the list object to look through
         PCListVariable pl;
         pl = GetList (pc);

         // clear the list box
         pControl = pPage->ControlFind (L"list");
         if (!pControl)
            return TRUE;   // cant really do anything

         // stop the list box sorting for now
         pControl->AttribSetBOOL (L"sort", FALSE);

         DWORD dwCurItem;
         int   iSetSel;
         pControl->Message (ESCM_LISTBOXRESETCONTENT);
         dwCurItem = 0;
         iSetSel = -1;

         // add all the items
         DWORD i, dwSize;
         for (i = 0; pl && (i < pl->Num()); i++) {
            pCur = (PWSTR) pl->Get(i);
            dwSize = pl->Size(i);

            // second string
            PWSTR pCur2;
            pCur2 = pCur + (wcslen(pCur)+1);
            if ((DWORD)((PBYTE)pCur2 - (PBYTE)pCur) >= dwSize)
               pCur2 = NULL;

            // filter according to text
            DWORD j;
            BOOL  fFilterOut;
            fFilterOut = FALSE;
            PWSTR pFilter;
            for (j = 0; j < lf.Num(); j++) {
               pFilter = *((PWSTR*) lf.Get(j));

               // find it in the first one
               if (stristr (pCur, pFilter))
                  continue;   // found it, go to the next word

               // else, didn't find it in the first. try in the 2nd
               if (pCur2 && stristr (pCur2, pFilter))
                  continue;   // found it

               // else didn't find
               fFilterOut = TRUE;
               break;
            }

            // if filter out is set then eliminate
            if (fFilterOut)
               continue;

            // will this be the selection?
            if ((pc->iCurSel >= 0) && ((DWORD)pc->iCurSel == i))
               iSetSel = (int) dwCurItem;

            // if it gets here add the item
            WCHAR szHuge[5000];  // only on the stack temporarily
            swprintf (szHuge, L"<elem data=%d name=\"", (int) i);
            DWORD dwNeeded;
            StringToMMLString (pCur, szHuge + wcslen(szHuge), sizeof(szHuge) - wcslen(szHuge)*2 - 2, &dwNeeded);
            wcscat (szHuge, L"\">");
            StringToMMLString (pCur, szHuge + wcslen(szHuge), sizeof(szHuge) - wcslen(szHuge)*2 - 2, &dwNeeded);
            wcscat (szHuge, L"</elem>");
            ESCMLISTBOXADD add;
            memset (&add, 0, sizeof(add));
            add.dwInsertBefore = -1;
            add.pszMML = szHuge;
            pControl->Message (ESCM_LISTBOXADD, &add);
            dwCurItem++;

         }

         // if the flag is set, add a "Add item" at the end of the list
         if (pc->pszAddItem && pc->pszAddItem[0]) {
            WCHAR szHuge[5000];  // only on the stack temporarily
            wcscpy (szHuge, L"<elem data=-2 name=zzzz><bold>");
            DWORD dwNeeded;
            StringToMMLString (pc->pszAddItem, szHuge + wcslen(szHuge), sizeof(szHuge) - wcslen(szHuge)*2 - 2, &dwNeeded);
            wcscat (szHuge, L"</bold></elem>");
            ESCMLISTBOXADD add;
            memset (&add, 0, sizeof(add));
            add.dwInsertBefore = -1;
            add.pszMML = szHuge;
            pControl->Message (ESCM_LISTBOXADD, &add);
            dwCurItem++;
         }

         // if there's an entry for nothing entered then use it
         if (pc->szBlank[0]) {
            // will this be the selection?
            if (pc->iCurSel < 0)
               iSetSel = (int) dwCurItem;

            WCHAR szHuge[5000];  // only on the stack temporarily
            wcscpy (szHuge, L"<elem data=-1 name=zzzzz><italic>");
            DWORD dwNeeded;
            StringToMMLString (pc->szBlank, szHuge + wcslen(szHuge), sizeof(szHuge) - wcslen(szHuge)*2 - 2, &dwNeeded);
            wcscat (szHuge, L"</italic></elem>");
            ESCMLISTBOXADD add;
            memset (&add, 0, sizeof(add));
            add.dwInsertBefore = -1;
            add.pszMML = szHuge;
            pControl->Message (ESCM_LISTBOXADD, &add);
            dwCurItem++;
         }

         // if there's nothing set to be selected, then select the first item in the list
         if (iSetSel < 0)
            iSetSel = 0;

         // set the selection to the current list item
         pControl->AttribSetInt (L"cursel", iSetSel);

         // restart the sorting if requested
         if (pc->fSort)
            pControl->AttribSetBOOL (L"sort", TRUE);

      }
      return TRUE;

   case ESCN_LISTBOXSELCHANGE:
      {
         ESCNLISTBOXSELCHANGE *p = (ESCNLISTBOXSELCHANGE*) pParam;

         // if its a mouse click then exit. Elase ignore
         if (p->dwReason != 1)
            return TRUE;

         // remember the new selection
         if (p->pszData)
            AttribToDecimal (p->pszData, &pc->iCurSel);
         else
            pc->iCurSel = -1;

         pPage->Exit (L"found");
      }
      return TRUE;

   }

   return FALSE;
}


/***********************************************************************
Control callback
*/
BOOL ControlFilteredList (PCEscControl pControl, DWORD dwMessage, PVOID pParam)
{
   FILTEREDLIST *pc = (FILTEREDLIST*) pControl->m_mem.p;

   switch (dwMessage) {
   case ESCM_CONSTRUCTOR:
      {
         pControl->m_mem.Required (sizeof(FILTEREDLIST));
         pc = (FILTEREDLIST*) pControl->m_mem.p;
         memset (pc, 0, sizeof(FILTEREDLIST));

         wcscpy (pc->szBlank, L"Nothing entered");
         pc->pControl = pControl;
         pc->pszListName = NULL;
         pc->pszAddItem = L"Add a new item";
         pc->iCurSel = -1;
         pc->fSort = FALSE;

         // attributes
         pControl->AttribListAddString (L"blank", pc->szBlank, sizeof(pc->szBlank), FALSE, TRUE);
         pControl->AttribListAddString (L"listname", &pc->pszListName, FALSE, TRUE);
         pControl->AttribListAddString (L"additem", &pc->pszAddItem, FALSE, TRUE);
         pControl->AttribListAddDecimal (L"cursel", &pc->iCurSel, NULL, TRUE);
         pControl->AttribListAddBOOL (L"sort", &pc->fSort, FALSE, TRUE);

         // constructor for dropdown
         DropDownMessageHandler (pControl, dwMessage, pParam, &pc->dd);
      }
      return TRUE;

   case ESCM_DROPDOWNOPENED:
      {
         PESCMDROPDOWNOPENED p = (PESCMDROPDOWNOPENED)pParam;

         // put together the MML text to be used
         CMem  mem;

         mem.m_dwCurPosn = 0;

         // background color
         WCHAR szTemp[512];
         WCHAR szColor[16], szColor2[16];
         ColorToAttrib (szColor, pc->dd.cTBackground);
         ColorToAttrib (szColor2, pc->dd.cBBackground);
         swprintf (szTemp, L"<colorblend posn=background tcolor=%s bcolor=%s/>",
            szColor, szColor2);
         mem.StrCat (szTemp);

         mem.StrCat (
            L"<small>"
            );

         // edit control
         swprintf (szTemp, L"<Edit name=edit width=%d maxchars=80 defcontrol=true/>",
            p->iWidth);
         mem.StrCat (szTemp);

         // break
         mem.StrCat (L"<br/>");

         // listbox
         swprintf (szTemp, L"<listbox name=list vscroll=vscroll height=%d width=%d/>",
            p->iHeight, p->iWidth);
         mem.StrCat (szTemp);

         // scrollbar
         swprintf (szTemp, L"<scrollbar name=vscroll orient=vert height=%d/>",
            p->iHeight);
         mem.StrCat (szTemp);

         // end small
         mem.StrCat (
            L"</small>"
            );

         // done. Append the NULL
         mem.CharCat (0);

         // bring up the window
         WCHAR    *psz;
         int      iOldSel;
         iOldSel = pc->iCurSel;

         psz = p->pWindow->PageDialog ((PWSTR)mem.p, FilteredListCallback, (PVOID) pc);

         // hide the window so it's not visible is a call is made from this
         p->pWindow->ShowWindow (SW_HIDE);

         // invalidate the control since its contents have changed
         pControl->Invalidate();

         // alert the app
         ESCNFILTEREDLISTCHANGE tc;
         memset (&tc, 0, sizeof(tc));
         tc.iCurSel = pc->iCurSel;
         tc.pControl = pControl;
         if (iOldSel != pc->iCurSel)
            pControl->MessageToParent (ESCN_FILTEREDLISTCHANGE, &tc);
      }
      return TRUE;

   case ESCM_DROPDOWNTEXTBLOCK:
      {
         PESCMDROPDOWNTEXTBLOCK p = (PESCMDROPDOWNTEXTBLOCK) pParam;

         // determine what we should say
         PWSTR pCur;
         if (pc->iCurSel >= 0)
            pCur = (PWSTR) GetList(pc)->Get((DWORD) pc->iCurSel);
         else
            pCur = NULL;
         if (!pCur)
            pCur = pc->szBlank;

         // if this isn't forced and this string is identical to the old
         // one then just accept it
         if (!p->fMustSet && !wcsicmp(pCur, pc->szCurDisplay))
            return TRUE;

         // else set it
         wcscpy (pc->szCurDisplay, pCur);
         p->pszText = pc->szCurDisplay;
      }
      return TRUE;

   }

   return DropDownMessageHandler (pControl, dwMessage, pParam, pc ? &pc->dd : NULL);
}





