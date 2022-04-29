/***************************************************************************
CObjListView - C++ object for viewing the list of objects.

begun 18/12/2002
Copyright 2002 Mike Rozak. All rights reserved
*/

#include <windows.h>
#include <math.h>
#include "escarpment.h"
#include "..\M3D.h"
#include "resource.h"

// OLVENTRY - Keep list of objects
typedef struct {
   PWSTR          pszName;    // name
   PWSTR          pszGroup;   // group
   GUID           gObject;    // object GUID
   BOOL           fSelected;  // set to true if selected
   BOOL           fShow;      // set to TRUE if visible
} OLVENTRY, *POLVENTRY;

class CObjListView : public CViewSocket {
   friend BOOL CObjListViewPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam);
public:
   ESCNEWDELETE;

   CObjListView (void);
   ~CObjListView (void);

   BOOL Init (PCWorldSocket pWorld, HWND hWndCreator);
   void CheckForClose (void); // call every message

   // CViewSocket
   virtual void WorldAboutToChange (GUID *pgObject);
   virtual void WorldChanged (DWORD dwChanged, GUID *pgObject);
   virtual void WorldUndoChanged (BOOL fUndo, BOOL fRedo);


//private:
   void ObjectsGet (void);
   void NewPage (void);

   PCWorldSocket  m_pWorld;         // world using
   PCEscWindow    m_pWindow;        // window to display from
   DWORD          m_dwRefreshTimer; // timer set to go off to refresh the page, 0 if no timer
   CListFixed     m_lOLVENTRY;      // list of objects
   int            m_iVScroll;       // where to scroll to
   CBTree         m_tGroupsVisible; // which groups are visible
   GUID           m_gMove;          // object to move
   CMem           m_memMove;        // string of group moving
   DWORD          m_dwMove;         // 0 for no move, 1 for moving individual object, 2 for group
};
typedef CObjListView *PCObjListView;


static CListFixed glPCObjListView;   // list of list views


/**************************************************************************
FindLastSlash - Returns a pointer ot the last slash in the string
*/
WCHAR *FindLastSlash (PWSTR psz)
{
   DWORD i, dwLast;
   dwLast = -1;
   for (i = 0; psz[i]; i++)
      if (psz[i] == L'\\')
         dwLast = i;
   if (dwLast != -1)
      return psz + dwLast;
   else
      return NULL;
}


/**************************************************************************
OLVEntryCompare - For qsort */
static int _cdecl OLVEntryCompare (const void *elem1, const void *elem2)
{
   OLVENTRY *pdw1, *pdw2;
   pdw1 = (OLVENTRY*) elem1;
   pdw2 = (OLVENTRY*) elem2;

   int iRet;
   iRet = _wcsicmp (pdw1->pszGroup, pdw2->pszGroup);
   if (iRet)
      return iRet;

   iRet = _wcsicmp (pdw1->pszName, pdw2->pszName);
   if (iRet)
      return iRet;

   return memcmp (&pdw1->gObject, &pdw2->gObject, sizeof(pdw1->gObject));
}
/*************************************************************************************
CObjListView::Constructor and destructor
*/
CObjListView::CObjListView (void)
{
   m_pWorld = NULL;
   m_pWindow = NULL;
   m_dwRefreshTimer = 0;
   m_iVScroll = 0;
   m_dwMove = 0;
   m_lOLVENTRY.Init (sizeof(OLVENTRY));

   // add this to the list of list views
   if (!glPCObjListView.Num())
      glPCObjListView.Init (sizeof(PCObjListView));
   PCObjListView pv;
   pv = this;
   glPCObjListView.Add (&pv);
}

CObjListView::~CObjListView (void)
{
   // Remove from list of objlistviews
   DWORD i;
   for (i = 0; i < glPCObjListView.Num(); i++) {
      PCObjListView pv = *((PCObjListView*) glPCObjListView.Get(i));
      if (pv == this) {
         glPCObjListView.Remove (i);
         break;
      }
   }

   if (m_pWorld)
      m_pWorld->NotifySocketRemove (this);
   if (m_pWindow) {
      if (m_dwRefreshTimer)
         m_pWindow->TimerKill (m_dwRefreshTimer);
      m_dwRefreshTimer = 0;

      m_pWindow->PageClose();
      delete m_pWindow;
      m_pWindow = NULL;
   }
}

/*************************************************************************************
CObjListView::Init - Initializes the list view object to use the given world. This
must be called before other functions are used.

inputs
   PCWorldSocket     pWorld - World to use
   HWDN              hWndCreator - Window creating it. May be used to determine what
                     page the list will apepar on.
returns
   BOOL - TRUE if success
*/
BOOL CObjListView::Init (PCWorldSocket pWorld, HWND hWndCreator)
{
   // register with the world
   if (m_pWorld)
      return FALSE;
   m_pWorld = pWorld;
   m_pWorld->NotifySocketAdd (this);

   // create the window
   m_pWindow = new CEscWindow;
   if (!m_pWindow)
      return FALSE;

   // figure out where should appear
   RECT r;
   int iX, iY;
   iX = GetSystemMetrics (SM_CXSCREEN);
   iY = GetSystemMetrics (SM_CYSCREEN);
   r.left = iX / 8;
   r.top = iY / 8;
   r.right = r.left + iX / 3;
   r.bottom = r.top + iY / 4 * 3;

   if (!m_pWindow->Init (ghInstance, NULL, 0, &r)) {
      delete m_pWindow;
      m_pWindow =NULL;
      return FALSE;
   }

   // topmost
   SetWindowPos (m_pWindow->m_hWnd, (HWND)HWND_TOPMOST, 0, 0,
      0, 0, SWP_NOMOVE | SWP_NOSIZE);

   // show the page
   NewPage ();

   return TRUE;
}

/*************************************************************************************
CObjListView::CheckForClose - Call this every windows message. It looks at the
return parameter for the page to determine if the page was closed. If so, it
goes on to the new page.
*/
void CObjListView::CheckForClose (void)
{
   if (!m_pWindow->m_pszExitCode)
      return;

   // else, and exit code
   PWSTR psz = m_pWindow->m_pszExitCode;

   // get rid of the page if its still there
   m_pWindow->PageClose ();

   // if it has an angle bracket the user pressed close, so delete.
   // otherwise, revamp the page
   if (!psz || (psz[0] == L'['))
      delete this;
   else
      NewPage ();
}


/******************************************************************************
CObjListView::WorldAboutToChange - Callback from world
*/
void CObjListView::WorldAboutToChange (GUID *pgObject)
{
   // do nothing
}

/*************************************************************************************
CObjListView::WorldChanged - Called whenever the world changes. This will set a timer to
cause the page to close.
*/
void CObjListView::WorldChanged (DWORD dwChanged, GUID *pgObject)
{
   // only care about certain changes
   if (!(dwChanged & (WORLDC_SELADD | WORLDC_SELREMOVE | WORLDC_OBJECTADD | WORLDC_OBJECTREMOVE)))
      return;

   if (m_pWindow && m_pWindow->m_pPage) {
      if (m_dwRefreshTimer)
         m_pWindow->TimerKill (m_dwRefreshTimer);
      DWORD dwDelay;
      if (GetForegroundWindow () == m_pWindow->m_hWnd)
         dwDelay = 50;  // dont delay if this one has the focus
      else
         dwDelay = 1000;
      m_dwRefreshTimer = m_pWindow->TimerSet (dwDelay, m_pWindow->m_pPage);
   }
}

/*************************************************************************************
CObjListView::WorldUndoChanged - Called when the undo state has changed. THis
is ignored
*/
void CObjListView::WorldUndoChanged (BOOL fUndo, BOOL fRedo)
{
   return;
}



/*************************************************************************************
CObjListView::ObjectsGet - Fills in all the object info from the world
*/
void CObjListView::ObjectsGet (void)
{
   // get all the objects
   m_lOLVENTRY.Clear();
   OLVENTRY olv;
   memset(&olv, 0, sizeof(olv));

   // get the list of selected
   DWORD dwNumSel;
   DWORD *padwSel;
   padwSel = m_pWorld->SelectionEnum (&dwNumSel);

   DWORD i;
   PCObjectSocket pos;
   for (i = 0; i < m_pWorld->ObjectNum(); i++) {
      pos = m_pWorld->ObjectGet (i);
      if (!pos)
         continue;

      olv.pszName = pos->StringGet (OSSTRING_NAME);
      if (!olv.pszName)
         olv.pszName = L"";
      olv.pszGroup = pos->StringGet (OSSTRING_GROUP);
      if (!olv.pszGroup)
         olv.pszGroup = L"";
      pos->GUIDGet (&olv.gObject);

      olv.fSelected = FALSE;
      if (DWORDSearch (i, dwNumSel, padwSel) != -1)
         olv.fSelected = TRUE;

      olv.fShow = pos->ShowGet();

      m_lOLVENTRY.Add (&olv);
   }

   // Sort
   qsort (m_lOLVENTRY.Get(0), m_lOLVENTRY.Num(), sizeof(OLVENTRY), OLVEntryCompare);
}

/*************************************************************************************
CObjListView::NewPage - Creates the MML for a new page and pulls up that page.
*/
void CObjListView::NewPage (void)
{
   if (m_dwRefreshTimer)
      m_pWindow->TimerKill (m_dwRefreshTimer);
   m_dwRefreshTimer = 0;

   // remember the old scroll
   m_iVScroll = m_pWindow->m_iExitVScroll;

   // get the objects
   ObjectsGet ();

   CMem  pMem;
   MemZero (&pMem);
   // Show the objects
   DWORD i;
   POLVENTRY polv;
   WCHAR szTemp[64];
   polv = (POLVENTRY) m_lOLVENTRY.Get(0);
   MemCat (&pMem, L"<pageinfo bottommargin=4 topmargin=4 lrmargin=8/>");
   MemCat (&pMem, L"<colorblend posn=background tcolor=#ffff80 bcolor=#e0e070/>");
   MemCat (&pMem, L"<small>");
   //MemCat (&pMem, L"<table width=100%% innerlines=0 border=0 tbmargin=8><tr><td>");

   CMem memG;

#define INDENTAMT    16
   // assume visible at first
   BOOL fVisibleGroup, fVisibleItem, fFirstTime;
   PWSTR pszGroup;
   fVisibleGroup = fVisibleItem = TRUE;
   for (i = 0; i < m_lOLVENTRY.Num(); i++) {
      // is this a new group?
      BOOL fNewGroup;
      fNewGroup = (!i || _wcsicmp(polv[i].pszGroup, polv[i-1].pszGroup));

      // find the number of backslashes
      DWORD dwCount, dwLast, j;
      dwCount = dwLast = 0;
      for (j = 0; (polv[i].pszGroup)[j]; j++)
         if ((polv[i].pszGroup)[j] == L'\\') {
            dwCount++;
            dwLast = j+1;
         }

      // if it's a new group then redetermine what's visible
      if (fNewGroup) {
         // this is visible if all up the chain is visible
         fVisibleGroup = fVisibleItem = TRUE;
         fFirstTime = TRUE;
         if (!memG.Required ((wcslen(polv[i].pszGroup)+1)*2))
            continue;
         pszGroup = (PWSTR) memG.p;
         wcscpy (pszGroup, polv[i].pszGroup);
         while (pszGroup[0] && fVisibleGroup) {
            if (!m_tGroupsVisible.Find (pszGroup)) {
               fVisibleItem = FALSE;
               if (!fFirstTime)
                  fVisibleGroup = FALSE;
            }

            // continue
            PWSTR pc;
            pc = FindLastSlash (pszGroup);
            if (pc)
               *pc = 0;
            else
               pszGroup[0] = 0;
            fFirstTime = FALSE;
         }
      }


      // find out where the groups diverge
      DWORD dwDiverge, dwCurDiverge, dwCur;
      if (fNewGroup) {
         dwDiverge = -1;
         for (j = 0; i && (polv[i].pszGroup)[j] && (polv[i-1].pszGroup)[j]; j++) {
            if (((polv[i].pszGroup)[j] == L'\\') && ((polv[i-1].pszGroup)[j] == L'\\'))
               dwDiverge = j; // matched up to here

            if (towlower ((polv[i].pszGroup)[j]) != towlower ((polv[i-1].pszGroup)[j]))
               break;   // different
         }

         // if end of the previous one, but more in first, then diverge is j
         if (i && !((polv[i-1].pszGroup)[j]) && ((polv[i].pszGroup)[j] == L'\\'))   // BUGFIX - Added check for i
            dwDiverge = j;
      }

      // group header
      if (fNewGroup && (polv[i].pszGroup)[0]) {
         dwCurDiverge = -1;
         while (TRUE) {
            PWSTR pszLast;
            // copy over the group
            MemZero (&memG);
            MemCat (&memG, polv[i].pszGroup);
            pszGroup = (PWSTR) memG.p;

            // is this shown?
            dwCur = dwCurDiverge;
            if (dwCurDiverge != -1)
               pszGroup[dwCurDiverge] = 0;
            if ((dwCurDiverge != -1) && pszGroup[0] && !m_tGroupsVisible.Find (pszGroup))
               break;
            if (dwCurDiverge != -1)
               pszGroup[dwCurDiverge] = L'\\';

            pszLast = wcschr (pszGroup + ((dwCurDiverge == -1) ? 0 : (dwCurDiverge + 1)), L'\\');
            if (pszLast) {
               pszLast[0] = 0;

               // remember next step
               dwCurDiverge = (DWORD)((size_t)pszLast - (size_t) pszGroup) / sizeof(WCHAR);
            }
            else
               dwCurDiverge = -1;   // no more

            pszLast = FindLastSlash (pszGroup);


            if (pszGroup[0] && ((DWORD)(dwCur+1) >= (DWORD)(dwDiverge+1))) {
               DWORD dwCount;
               dwCount = 0;
               for (j = 0; pszGroup[j]; j++)
                  if (pszGroup[j] == L'\\')
                     dwCount++;

               MemCat (&pMem, L"<align parindent=");
               MemCat (&pMem, (int) INDENTAMT * (int) dwCount);
               MemCat (&pMem, L" wrapindent=");
               MemCat (&pMem, (int) INDENTAMT * (int) dwCount);
               MemCat (&pMem, L">");
               MemCat (&pMem, L"<image transparent=true bmpresource=");
               MemCat (&pMem, (int) (fVisibleItem ? IDB_ARROWDOWN : IDB_ARROWRIGHT));
               MemCat (&pMem, L" href=\"x:");
               MemCatSanitize (&pMem, pszGroup);
               MemCat (&pMem, L"\"/>");
               MemCat (&pMem, L"<bold><a color=#000000 href=\"g:");
               MemCatSanitize (&pMem, pszGroup);
               MemCat (&pMem, L"\">");
               MemCatSanitize (&pMem, pszLast ? (pszLast+1) : pszGroup);
               MemCat (&pMem, L"</a></bold><br/></align>");
            }

            // move on to next one
            if (dwCurDiverge == -1)
               break;   // did it all
         }  // while divergence
      }

      // don't show the object?
      if (!fVisibleItem)
         continue;

      // show the object
      if ((polv[i].pszGroup)[0])
         dwCount++;     // indent if it's not on the roof
      MemCat (&pMem, L"<align parindent=");
      MemCat (&pMem, (int) INDENTAMT * (int) dwCount);
      MemCat (&pMem, L" wrapindent=");
      MemCat (&pMem, (int) INDENTAMT * (int) dwCount);
      MemCat (&pMem, L">");
      MemCat (&pMem, L"<image transparent=true bmpresource=");
      MemCat (&pMem, (int) IDB_ARROWNONE);
      MemCat (&pMem, L"/>");
      MemCat (&pMem, L"<a color=#");
      if (polv[i].fSelected)
         MemCat (&pMem, L"ff0000");
      else if (!polv[i].fShow)
         MemCat (&pMem, L"00ff00");
      else
         MemCat (&pMem, L"0000ff");
      MemCat (&pMem, L" href=\"o:");
      MMLBinaryToString ((PBYTE) &polv[i].gObject, sizeof(polv[i].gObject), szTemp);
      MemCatSanitize (&pMem, szTemp);
      MemCat (&pMem, L"\">");
      MemCatSanitize (&pMem, polv[i].pszName);
      if (polv[i].fSelected)
         MemCat (&pMem, L" <italic>(Selected)</italic>");
      if (!polv[i].fShow)
         MemCat (&pMem, L" <italic>(Hidden)</italic>");
      MemCat (&pMem, L"</a><br/></align>");
   }
   //MemCat (&pMem, L"</td></tr></table>");
   MemCat (&pMem, L"</small>");

   // create the page
   m_pWindow->PageDisplay (ghInstance, (PWSTR)pMem.p, CObjListViewPage, this);
}



/**************************************************************************
OLGetNamePage
*/
BOOL OLGetNamePage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PWSTR pszName = (PWSTR) pPage->m_pUserData;
   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         PCEscControl pControl;

         pControl = pPage->ControlFind (L"name");
         if (pControl)
            pControl->AttribSet (Text(), pszName);
      }
      break;

   case ESCM_LINK:
      {
         PESCMLINK p = (PESCMLINK) pParam;
         if (!p->psz)
            break;
         if (!_wcsicmp(p->psz, L"ok")) {
            PCEscControl pControl;
            pszName[0] = 0;
            DWORD dwNeeded;
            pControl = pPage->ControlFind (L"name");
            if (pControl)
               pControl->AttribGet (Text(), pszName, 256, &dwNeeded);

            if (!pszName[0]) {
               pPage->MBWarning (L"You cannot leave the name blank.");
               return TRUE;
            }
            break;
         }
      }
      break;
   }

   return DefPage (pPage, dwMessage, pParam);
}


/*************************************************************************************
CObjListViewPage - Handles the page
*/
BOOL CObjListViewPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCObjListView pv = (PCObjListView) pPage->m_pUserData;

   switch (dwMessage) {
      case ESCM_INITPAGE:
         {
            // set the page title based on the world
            PWSTR pszWorld = pv->m_pWorld->NameGet ();
            WCHAR szTemp[512];
            swprintf (szTemp, L"Object list - %s", (pszWorld && pszWorld[0]) ? pszWorld : L"Unknown");
            pv->m_pWindow->IconSet (LoadIcon(ghInstance, MAKEINTRESOURCE(IDI_APPICON)));
            pv->m_pWindow->TitleSet (szTemp);

            // if we have somplace to scroll to then do so
            if (pv->m_iVScroll >= 0) {
               pPage->VScroll (pv->m_iVScroll);

               // when bring up pop-up dialog often they're scrolled wrong because
               // iVScoll was left as valeu, and they use defpage
               pv->m_iVScroll = 0;

               // BUGFIX - putting this invalidate in to hopefully fix a refresh
               // problem when add or move a task in the ProjectView page
               pPage->Invalidate();
            }
         }
         break;

      case ESCM_TIMER:
         {
            PESCMTIMER p = (PESCMTIMER) pParam;
            if (pv->m_dwRefreshTimer && (p->dwID == pv->m_dwRefreshTimer)) {
               pv->m_pWindow->TimerKill (p->dwID);
               pv->m_dwRefreshTimer = 0;
               pPage->Exit (RedoSamePage());
            }
            break;
         }

      case ESCM_LINK:
         {
            PESCMLINK p = (PESCMLINK) pParam;
            PWSTR psz = p->psz;
            if (!p->psz)
               break;

            // if it's an "x:" then show/hide the group
            if ((psz[0] == L'x') && (psz[1] == L':')) {
               DWORD dw = 0;

               // if find then remove
               if (pv->m_tGroupsVisible.Find (psz+2))
                  pv->m_tGroupsVisible.Remove (psz+2);
               else
                  pv->m_tGroupsVisible.Add (psz+2, &dw, sizeof(dw));
               // follow through link so refreshes
               break;
            }
            else if ((psz[0] == L'g') && (psz[1] == L':') && pv->m_dwMove) {
               if (pv->m_dwMove == 1) {
                  pv->m_dwMove = 0; // so dont try to move again

                  // get he object
                  DWORD dwIndex;
                  PCObjectSocket pos;
                  dwIndex = pv->m_pWorld->ObjectFind (&pv->m_gMove);
                  if (dwIndex == -1)
                     return TRUE;
                  pos = pv->m_pWorld->ObjectGet (dwIndex);
                  if (!pos)
                     return TRUE;

                  // set the object
                  pos->StringSet (OSSTRING_GROUP, psz + 2);

                  break;
               }
               else {   // dwMove == 2;
                  pv->m_dwMove = 0; // so dont try to move again

                  // the exact match is in pv->m_memMove
                  PWSTR pszExact;
                  DWORD dwExactLen;
                  pszExact = (PWSTR)pv->m_memMove.p;
                  dwExactLen = (DWORD)wcslen(pszExact);

                  // find the last slash
                  PWSTR pszLast;
                  pszLast = FindLastSlash (pszExact);
                  if (pszLast)
                     pszLast++;
                  else
                     pszLast = pszExact;

                  // find out what this gets moved to
                  CMem memTo;
                  PWSTR pszTo;
                  pszTo = psz + 2;

                  // make sure this is opened
                  DWORD i;
                  i = 0;
                  pv->m_tGroupsVisible.Add (pszTo, &i, sizeof(i));

                  // scractch
                  CMem memScratch;

                  // loop through all the objects changing
                  PCObjectSocket pos;
                  PWSTR pszGroup, pszAfter;
                  for (i = 0; i < pv->m_pWorld->ObjectNum(); i++) {
                     pos = pv->m_pWorld->ObjectGet(i);
                     if (!pos)
                        continue;
                     pszGroup = pos->StringGet (OSSTRING_GROUP);
                     if (!pszGroup)
                        continue;

                     if (!_wcsicmp(pszGroup, pszExact))
                        pszAfter = NULL;
                     else if (!_wcsnicmp (pszGroup, pszExact, dwExactLen) && (pszGroup[dwExactLen] == L'\\'))
                        pszAfter = pszGroup + dwExactLen;
                     else
                        continue;   // no match

                     // create new name
                     MemZero (&memScratch);
                     MemCat (&memScratch, pszTo);
                     MemCat (&memScratch, L"\\");
                     MemCat (&memScratch, pszLast);
                     if (pszAfter)
                        MemCat (&memScratch, pszAfter);

                     // set it
                     pos->StringSet (OSSTRING_GROUP, (PWSTR) memScratch.p);
                  }
                  break;   // so refresh
               }
            }
            else if ((psz[0] == L'g') && (psz[1] == L':')) {
               HMENU hMenu = LoadMenu (ghInstance, MAKEINTRESOURCE(IDR_MENUOLGROUP));
               HMENU hSub = GetSubMenu(hMenu,0);
               BOOL fControl = (GetKeyState (VK_CONTROL) < 0);
               BOOL fShift = (GetKeyState (VK_CONTROL) < 0);

               POINT p;
               GetCursorPos (&p);
               int iRet;
               iRet = TrackPopupMenu (hSub, TPM_CENTERALIGN | TPM_TOPALIGN | TPM_LEFTBUTTON | TPM_NONOTIFY | TPM_RETURNCMD, 
                  p.x, p.y, 0, pPage->m_pWindow->m_hWnd, NULL);
               DestroyMenu (hMenu);
               if (!iRet)
                  return 0;

               WCHAR szTemp[256];
               szTemp[0] = 0;
               if (iRet == ID_GROUP_MOVETOANEXISTINGGROUP) {
                  // store away move info
                  MemZero (&pv->m_memMove);
                  MemCat (&pv->m_memMove, psz + 2);
                  pv->m_dwMove = 2;

                  // show help
                  POINT p;
                  GetCursorPos (&p);
                  PCMMLNode pNode;
                  CEscError pErr;
                  CMem memParse;
                  MemZero (&memParse);
                  MemCat (&memParse,
                     L"<small>"
                     L"Click on the group you wish to move the group to."
                     L"</small>"
                     );
                  pNode = ParseMML ((PWSTR)memParse.p,
                     ghInstance, NULL,
                     NULL, &pErr);
                  if (pNode) {
                     pPage->m_pWindow->HoverHelp (ghInstance, pNode, FALSE, &p);
                     delete pNode;
                  }

                  // dont refresh since still need to move
                  return TRUE;
               }

               // get the object string
               PWSTR pszExact, pszContain;
               DWORD dwContain;
               CMem memApprox;
               pszExact = psz + 2;
               MemZero (&memApprox);
               MemCat (&memApprox, pszExact);
               MemCat (&memApprox, L"\\");
               pszContain = (PWSTR) memApprox.p;
               dwContain = (DWORD)wcslen (pszContain);

               // shift state
               fControl |= (GetKeyState (VK_CONTROL) < 0);
               fShift |= (GetKeyState (VK_CONTROL) < 0);

               // get the list of selected
               DWORD dwNumSel;
               DWORD *padwSel;
               padwSel = pv->m_pWorld->SelectionEnum (&dwNumSel);

               // last slash
               CMem memScratch;
               PWSTR pszLast;
               DWORD dwLast, dwLen;
               pszLast = FindLastSlash (pszExact);
               if (pszLast)
                  dwLast = (DWORD)((size_t) pszLast - (size_t) pszExact) / sizeof(WCHAR);
               else
                  dwLast = -1;

               if (iRet == ID_GROUP_MOVETOANEWGROUP) {
                  // get the name
                  CEscWindow cWindow;
                  RECT r;
                  DialogBoxLocation2 (pPage->m_pWindow->m_hWnd, &r);
                  cWindow.Init (ghInstance, pPage->m_pWindow->m_hWnd, EWS_FIXEDSIZE | EWS_AUTOHEIGHT | EWS_NOTITLE, &r);
                  PWSTR pszRet;
                  pszRet = cWindow.PageDialog (ghInstance, IDR_MMLOLNEWGROUP, OLGetNamePage, szTemp);

                  // set the object
                  if (!pszRet || _wcsicmp(pszRet, L"ok"))
                     return TRUE;

                  // now, append old group (last part) to this
                  wcscat (szTemp, L"\\");
                  if (pszLast)
                     wcscat (szTemp, pszLast+1);
                  else
                     wcscat (szTemp, pszExact);
               }
               else if (iRet == ID_GROUP_RENAMEGROUP) {
                  // get the name
                  CEscWindow cWindow;
                  RECT r;
                  DialogBoxLocation2 (pPage->m_pWindow->m_hWnd, &r);
                  cWindow.Init (ghInstance, pPage->m_pWindow->m_hWnd, EWS_FIXEDSIZE | EWS_AUTOHEIGHT | EWS_NOTITLE, &r);
                  PWSTR pszRet;
                  if (pszLast)
                     wcscpy (szTemp, pszLast+1);
                  else
                     wcscpy (szTemp, pszExact);
                  pszRet = cWindow.PageDialog (ghInstance, IDR_MMLOLRENAMEOBJECT, OLGetNamePage, szTemp);
                  if (!pszRet || _wcsicmp(pszRet, L"ok"))
                     return TRUE;
               }

               // bounding box
               CPoint apBound[2];
               CPoint apWorld[2];
               CMatrix mMatrix;
               BOOL fWorldValid;
               fWorldValid = FALSE;

               // change all the values
               DWORD i;
               PCObjectSocket pos;
               PWSTR pszGroup;
               BOOL fFirstTime, fFlag, fWant;
               fFirstTime = TRUE;
               for (i = 0; i < pv->m_pWorld->ObjectNum(); i++) {
                  pos = pv->m_pWorld->ObjectGet(i);
                  if (!pos)
                     continue;
                  pszGroup = pos->StringGet (OSSTRING_GROUP);
                  if (!pszGroup)
                     continue;

                  if (_wcsicmp(pszGroup, pszExact) && _wcsnicmp(pszGroup, pszContain, dwContain))
                     continue;   // doesn't match the group or subgroup

                  // else, do action
                  switch (iRet) {
                  case ID_GROUP_REPOSITIONCAMERATOVIEWTHIS:
                     if (!pv->m_pWorld->BoundingBoxGet (i, &mMatrix, &apBound[0], &apBound[1]))
                        continue;

                     DWORD x,y,z;
                     CPoint p;
                     for (x = 0; x < 2; x++) for (y = 0; y < 2; y++) for (z = 0; z < 2; z++) {
                        p.p[0] = apBound[x].p[0];
                        p.p[1] = apBound[y].p[1];
                        p.p[2] = apBound[z].p[2];
                        p.p[3] = 1;

                        p.MultiplyLeft (&mMatrix);
                        if (!fWorldValid) {
                           apWorld[0].Copy (&p);
                           apWorld[1].Copy (&p);
                           fWorldValid = TRUE;
                        }
                        else {
                           apWorld[0].Min (&p);
                           apWorld[1].Max (&p);
                        }
                     }
                     break;

                  case ID_GROUP_RENAMEGROUP:
                  case ID_GROUP_MOVETOANEWGROUP:
                     MemZero (&memScratch);
                     if (dwLast != -1) {
                        MemCat (&memScratch, pszGroup);
                        ((PWSTR)memScratch.p)[dwLast+1] = 0;
                        memScratch.m_dwCurPosn = (dwLast+1)*sizeof(WCHAR);
                     }
                     MemCat (&memScratch, szTemp); // add rename

                     // add rest
                     dwLen = (DWORD)wcslen(pszGroup);
                     if (dwLen > dwContain)
                        MemCat (&memScratch, pszGroup + (dwContain-1)); // to include the backslash

                     pos->StringSet (OSSTRING_GROUP, (PWSTR) memScratch.p);
                     break;

                  case ID_GROUP_MOVEALLOBJECTSOUTOFTHISGROUP:
                     // copy group
                     MemZero (&memScratch);
                     MemCat (&memScratch, pszGroup);
                     dwLen = (DWORD)wcslen((PWSTR)memScratch.p);
                     if (dwLen > dwContain)
                        memmove ((PWSTR)memScratch.p + (DWORD)(dwLast+1),
                           (PWSTR)memScratch.p + dwContain, (dwLen - dwContain + 1) * sizeof(WCHAR));
                     else
                        ((PWSTR)memScratch.p)[(dwLast == -1) ? 0 : dwLast] = 0;

                     pos->StringSet (OSSTRING_GROUP, (PWSTR) memScratch.p);
                     break;

                  case ID_GROUP_SELECT:
                     fFlag = (DWORDSearch (i, dwNumSel, padwSel) != -1);
                     if (fFirstTime) {
                        fWant = !fFlag;
                        fFirstTime = FALSE;
                     }
                     if (fWant && !fFlag) {
                        pv->m_pWorld->SelectionAdd (i);
                        padwSel = pv->m_pWorld->SelectionEnum (&dwNumSel); // get this again
                     }
                     else if (!fWant && fFlag) {
                        pv->m_pWorld->SelectionRemove (i);
                        padwSel = pv->m_pWorld->SelectionEnum (&dwNumSel); // get this again
                     }
                     break;

                  case ID_GROUP_SHOW:
                     fFlag = pos->ShowGet();
                     if (fFirstTime) {
                        fWant = !fFlag;
                        fFirstTime = FALSE;
                     }
                     if (fWant != fFlag)
                        pos->ShowSet (fWant);
                     break;
                  } // switch

                  if ((iRet == ID_GROUP_REPOSITIONCAMERATOVIEWTHIS) && fWorldValid) {
                     // found a bounding box so view it
                     PCHouseView phv;
                     phv = FindViewForWorld (pv->m_pWorld);
                     if (phv)
                        phv->LookAtArea (&apWorld[0], &apWorld[1], ID_TOP);
                  }

               } // loop over objects, i
            }
            else if ((psz[0] == L'o') && (psz[1] == L':')) {
               HMENU hMenu = LoadMenu (ghInstance, MAKEINTRESOURCE(IDR_MENUOLOBJECT));
               HMENU hSub = GetSubMenu(hMenu,0);
               BOOL fControl = (GetKeyState (VK_CONTROL) < 0);
               BOOL fShift = (GetKeyState (VK_CONTROL) < 0);

               // reset the move flag
               pv->m_dwMove = 0;

               POINT p;
               GetCursorPos (&p);
               int iRet;
               iRet = TrackPopupMenu (hSub, TPM_CENTERALIGN | TPM_TOPALIGN | TPM_LEFTBUTTON | TPM_NONOTIFY | TPM_RETURNCMD, 
                  p.x, p.y, 0, pPage->m_pWindow->m_hWnd, NULL);
               DestroyMenu (hMenu);
               if (!iRet)
                  return 0;

               // get the object
               GUID gVal;
               MMLBinaryFromString (psz + 2, (PBYTE) &gVal, sizeof(gVal));
               PCObjectSocket pos;
               DWORD dwIndex;
               dwIndex = pv->m_pWorld->ObjectFind (&gVal);
               if (dwIndex == -1)
                  return TRUE;
               pos = pv->m_pWorld->ObjectGet (dwIndex);
               if (!pos)
                  return TRUE;

               fControl |= (GetKeyState (VK_CONTROL) < 0);
               fShift |= (GetKeyState (VK_CONTROL) < 0);
               if (iRet == ID_OLOBJECT_SELECTORDE) {
                  // is it selected?
                  // get the list of selected
                  DWORD dwNumSel;
                  DWORD *padwSel;
                  padwSel = pv->m_pWorld->SelectionEnum (&dwNumSel);
                  if (DWORDSearch (dwIndex, dwNumSel, padwSel) == -1) {
                     if (!fControl && !fShift)
                        pv->m_pWorld->SelectionClear ();
                     pv->m_pWorld->SelectionAdd (dwIndex);
                  }
                  else
                     pv->m_pWorld->SelectionRemove (dwIndex);
               }
               else if (iRet == ID_OLOBJECT_REPOSITIONCAMERATOVIEWTHIS) {
                  // figure out the bounding box
                  CPoint apBound[2];
                  CPoint apWorld[2];
                  CMatrix mMatrix;
                  if (!pv->m_pWorld->BoundingBoxGet (dwIndex, &mMatrix, &apBound[0], &apBound[1]))
                     return TRUE;

                  DWORD x,y,z;
                  CPoint p;
                  for (x = 0; x < 2; x++) for (y = 0; y < 2; y++) for (z = 0; z < 2; z++) {
                     p.p[0] = apBound[x].p[0];
                     p.p[1] = apBound[y].p[1];
                     p.p[2] = apBound[z].p[2];
                     p.p[3] = 1;

                     p.MultiplyLeft (&mMatrix);
                     if (!x && !y && !z) {
                        apWorld[0].Copy (&p);
                        apWorld[1].Copy (&p);
                     }
                     else {
                        apWorld[0].Min (&p);
                        apWorld[1].Max (&p);
                     }
                  }

                  // send it to a view
                  PCHouseView phv;
                  phv = FindViewForWorld (pv->m_pWorld);
                  if (phv)
                     phv->LookAtArea (&apWorld[0], &apWorld[1], ID_TOP);

                  return TRUE;   // no point falling through since nothing will change
               }
               else if (iRet == ID_OLOBJECT_SHOW) {
                  pos->ShowSet (!pos->ShowGet());
                  break;
               }
               else if (iRet == ID_OLOBJECT_MOVETOANEWGROUP) {
                  // store away move info
                  MMLBinaryFromString (psz + 2, (PBYTE) &pv->m_gMove, sizeof(pv->m_gMove));

                  // get he object
                  DWORD dwIndex;
                  PCObjectSocket pos;
                  dwIndex = pv->m_pWorld->ObjectFind (&pv->m_gMove);
                  if (dwIndex == -1)
                     return TRUE;
                  pos = pv->m_pWorld->ObjectGet (dwIndex);
                  if (!pos)
                     return TRUE;

                  CMem memG;
                  PWSTR psz;
                  MemZero (&memG);
                  psz = pos->StringGet(OSSTRING_GROUP);
                  MemCat (&memG, psz ? psz : L"");

                  // get the name
                  CEscWindow cWindow;
                  RECT r;
                  DialogBoxLocation2 (pPage->m_pWindow->m_hWnd, &r);
                  cWindow.Init (ghInstance, pPage->m_pWindow->m_hWnd, EWS_FIXEDSIZE | EWS_AUTOHEIGHT | EWS_NOTITLE, &r);
                  PWSTR pszRet;
                  WCHAR szTemp[256];
                  szTemp[0] = 0;
                  pszRet = cWindow.PageDialog (ghInstance, IDR_MMLOLNEWGROUP, OLGetNamePage, szTemp);

                  // set the object
                  if (pszRet && !_wcsicmp(pszRet, L"ok")) {
                     DWORD dw = 0;
                     if (((PWSTR) memG.p)[0])
                        MemCat (&memG, L"\\");
                     MemCat (&memG, szTemp);
                     pv->m_tGroupsVisible.Add ((PWSTR)memG.p, &dw, sizeof(dw));
                     pos->StringSet (OSSTRING_GROUP, (PWSTR)memG.p);
                  }
                  break;
               }
               else if (iRet == ID_OLOBJECT_MOVEOUTOFTHISGROUP) {
                  // store away move info
                  MMLBinaryFromString (psz + 2, (PBYTE) &pv->m_gMove, sizeof(pv->m_gMove));

                  // get he object
                  DWORD dwIndex;
                  PCObjectSocket pos;
                  dwIndex = pv->m_pWorld->ObjectFind (&pv->m_gMove);
                  if (dwIndex == -1)
                     return TRUE;
                  pos = pv->m_pWorld->ObjectGet (dwIndex);
                  if (!pos)
                     return TRUE;

                  CMem memG;
                  PWSTR psz;
                  MemZero (&memG);
                  psz = pos->StringGet(OSSTRING_GROUP);
                  MemCat (&memG, psz ? psz : L"");
                  psz = FindLastSlash ((PWSTR)memG.p);
                  if (psz)
                     psz[0] = 0;
                  else
                     MemZero (&memG);  // move to top

                  // set the object
                  pos->StringSet (OSSTRING_GROUP, (PWSTR)memG.p);
                  break;
               }
               else if (iRet == ID_OLOBJECT_MOVETOANEXISTINGGROUP) {
                  // store away move info
                  MMLBinaryFromString (psz + 2, (PBYTE) &pv->m_gMove, sizeof(pv->m_gMove));
                  pv->m_dwMove = 1;

                  // show help
                  POINT p;
                  GetCursorPos (&p);
                  PCMMLNode pNode;
                  CEscError pErr;
                  CMem memParse;
                  MemZero (&memParse);
                  MemCat (&memParse,
                     L"<small>"
                     L"Click on the group you wish to move the object to."
                     L"</small>"
                     );
                  pNode = ParseMML ((PWSTR)memParse.p,
                     ghInstance, NULL,
                     NULL, &pErr);
                  if (pNode) {
                     pPage->m_pWindow->HoverHelp (ghInstance, pNode, FALSE, &p);
                     delete pNode;
                  }

                  // dont refresh since still need to move
                  return TRUE;
               }
               else if (iRet == ID_OLOBJECT_RENAME) {
                  // store away move info
                  MMLBinaryFromString (psz + 2, (PBYTE) &pv->m_gMove, sizeof(pv->m_gMove));

                  // get he object
                  DWORD dwIndex;
                  PCObjectSocket pos;
                  dwIndex = pv->m_pWorld->ObjectFind (&pv->m_gMove);
                  if (dwIndex == -1)
                     return TRUE;
                  pos = pv->m_pWorld->ObjectGet (dwIndex);
                  if (!pos)
                     return TRUE;

                  PWSTR psz;
                  psz = pos->StringGet(OSSTRING_NAME);
                     // BUGFIX - Was OSSTRING_GROUP

                  // get the name
                  CEscWindow cWindow;
                  RECT r;
                  DialogBoxLocation2 (pPage->m_pWindow->m_hWnd, &r);
                  cWindow.Init (ghInstance, pPage->m_pWindow->m_hWnd, EWS_FIXEDSIZE | EWS_AUTOHEIGHT | EWS_NOTITLE, &r);
                  PWSTR pszRet;
                  WCHAR szTemp[256];
                  szTemp[0] = 0;
                  if (psz)
                     wcscpy (szTemp, psz);
                  pszRet = cWindow.PageDialog (ghInstance, IDR_MMLOLRENAMEOBJECT, OLGetNamePage, szTemp);

                  // set the object
                  if (pszRet && !_wcsicmp(pszRet, L"ok")) {
                     pos->StringSet (OSSTRING_NAME, szTemp);
                  }
                  break;
               }


               break;   // so redraws
            }

         }
         break;

   }
   return FALSE;
}

/*************************************************************************************
ListViewNew - Create a new list view for the given world. If it already exists then
it's merely made visible (if not already)

inputs
   PCWorldSocket     pWorld - World to view
   HWND              hWndCreator - Maybe be used to determine where list shows up
returns
   BOOL - TRUE if success
*/
BOOL ListViewNew (PCWorldSocket pWorld, HWND hWndCreator)
{
   // see if already exists
   DWORD i;
   PCObjListView pv;
   for (i = 0; i < glPCObjListView.Num(); i++) {
      pv = *((PCObjListView*) glPCObjListView.Get(i));
      if (pv->m_pWorld == pWorld) {
         pv->m_pWindow->ShowWindow (SW_RESTORE);
         return TRUE;
      }
   }

   // else, create
   pv = new CObjListView;
   if (!pv)
      return FALSE;
   if (!pv->Init (pWorld, hWndCreator)) {
      delete pv;
      return FALSE;
   }

   // done
   return TRUE;

}


/*************************************************************************************
ListViewRemove - Removes a list view that matches the given world.

inputs
   PCWorldSocket     pWorld - World to view
returns
   BOOL - TRUE if success
*/
BOOL ListViewRemove (PCWorldSocket pWorld)
{
   // see if already exists
   DWORD i;
   PCObjListView pv;
   for (i = 0; i < glPCObjListView.Num(); i++) {
      pv = *((PCObjListView*) glPCObjListView.Get(i));
      if (pv->m_pWorld == pWorld) {
         delete pv;
         return TRUE;
      }
   }

   return FALSE;
}



/*************************************************************************************
ListViewCheckPage - Call this for every windwos message to handle the close of list views.
*/
void ListViewCheckPage (void)
{
   // see if already exists
   DWORD i;
   PCObjListView pv;
   for (i = glPCObjListView.Num()-1; i < glPCObjListView.Num(); i--) {
      pv = *((PCObjListView*) glPCObjListView.Get(i));
      pv->CheckForClose ();
   }
}




/*************************************************************************************
ListViewShowALl - Shows or hides all list view windows.

inputs
   BOOL     fShow - if true then show
*/
void ListViewShowAll (BOOL fShow)
{
   // see if already exists
   DWORD i;
   PCObjListView pv;
   for (i = 0; i < glPCObjListView.Num(); i++) {
      pv = *((PCObjListView*) glPCObjListView.Get(i));
      pv->m_pWindow->ShowWindow (fShow ? SW_SHOW : SW_HIDE);
   }
}

