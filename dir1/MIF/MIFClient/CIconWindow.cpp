/*************************************************************************************
CIconWindow.cpp - Code for displaying the icon client window.

begun 25/5/04 by Mike Rozak.
Copyright 2004 by Mike Rozak. All rights reserved
*/

#include <windows.h>
#include <objbase.h>
#include <dsound.h>
#include <zmouse.h>
#include "escarpment.h"
#include "..\..\m3d\M3D.h"
#include "..\..\m3d\mifl.h"
#include "..\mif.h"
#include "..\buildnum.h"
#include "mifclient.h"
#include "resource.h"


// #define ENABLECHATVERB           // if set, then chat window has verbs in it
   // BUGFIX - don't have verbs in chat window to simplify UI

static PWSTR gpszIconWindowPrefix = L"IconWindow:";
static PWSTR gpszIconWindow = L"IconWindow";
// static PWSTR gpszF7 = L" (F7)";
static WNDPROC gpEditWndProc = NULL;

#define IDC_SCROLLBAR      2153        // scrollbar control
#define IDC_EDITCOMMAND    2154        // edit control

#define CHATVERBSIZE       24 // chat buttons are this large
#ifdef UNIFIEDTRANSCRIPT
   //#define CHATWINDOWBOTTOM   CHATVERBSIZE // how much extra is added to chat window on bottom
   #define ICONCORNER         CHATVERBSIZE
#else
   #define CHATEDITSIZE       (2*m_iTextHeight)
   #define CHATWINDOWBOTTOM   (CHATVERBSIZE + CHATEDITSIZE) // how much extra is added to chat window on bottom
   #define ICONCORNER         CHATEDITSIZE
#endif

#define CHECKTOOLIPTIMER   3534        // timer number that checks that the tooltip should be erased

#define TIMER_SCROLLICON          2068     // scroll
#define TIMERSCROLLICON_INTERVAL  100      // 100 milliseconds
#define TIMERSCROLLICON_DELAY     (500 / TIMERSCROLLICON_INTERVAL)     // time before actually scroll
#define TIMERSCROLLICON_RATE      1000      // scroll all the way across in this time
         // BUGFIX - Was 2000, but too slow, so change to 1000 so faster

/*************************************************************************************
CIWGroup::Constructor and destructor
*/
CIWGroup::CIWGroup (void)
{
   m_pIW = NULL;
   m_fCanChatTo = FALSE;
   MemZero (&m_memName);
   m_lPCVisImage.Init (sizeof(PCVisImage));
   m_lVIRect.Init (sizeof(RECT));
}

CIWGroup::~CIWGroup (void)
{
   DWORD i;
   PCVisImage *ppv = (PCVisImage*) m_lPCVisImage.Get(0);
   for (i = 0; i < m_lPCVisImage.Num(); i++) {
      // NOTE: This lets the main window delete the vis image, since it
      // maintains the master list
      DWORD dwIndex = -1;
      if (m_pIW && m_pIW->m_pMain)
         dwIndex = m_pIW->m_pMain->VisImageFindPtr (ppv[i]);
      if (dwIndex != -1)
         m_pIW->m_pMain->VisImageDelete (dwIndex);
      else
         delete ppv[i]; // shouldnt happen
   }
   m_lPCVisImage.Clear();
}


/*************************************************************************************
CIWGroup::Init - Initializes the group to use the given iconwindow

inputs
   PCIconWindow         pIW - Icon window
   PWSTR                pszName - Name of the group, displayed to the user
   BOOL                 fCanChatTo - Set to TRUE if can chat to
*/
BOOL CIWGroup::Init (PCIconWindow pIW, PWSTR pszName, BOOL fCanChatTo)
{
   m_pIW = pIW;
   m_fCanChatTo = fCanChatTo;
   MemZero (&m_memName);
   MemCat (&m_memName, pszName);

   return TRUE;
}




/*************************************************************************************
CIWGroup::Add - Adds an image to the group
*/
void CIWGroup::Add (PCVisImage pAdd)
{
   RECT r;
   memset (&r, 0, sizeof(r));
   m_lPCVisImage.Add (&pAdd);
   m_lVIRect.Add (&r);
}

/*************************************************************************************
CIconWindow::Constructor and destructor
*/
CIconWindow::CIconWindow (void)
{
   m_pMain = NULL;
   m_hWnd = NULL;
   MemZero (&m_memID);
   MemZero (&m_memName);
   m_lPCIWGroup.Init (sizeof(PCIWGroup));
   m_iTextHeight = 10;
   m_lRECTBack.Init (sizeof(RECT));
   m_hWndHScroll = m_hWndVScroll = NULL;
   m_fScrollAuto = FALSE;
   m_fChatWindow = FALSE;
   m_fTextUnderImages = !m_fChatWindow;
   m_lPCIconButton.Init (sizeof(PCIconButton));
   m_lAudioGUID.Init (sizeof(GUID));
   m_gSpeakWith = GUID_NULL;
   m_iSpeakMode = 0;
#ifndef UNIFIEDTRANSCRIPT
   m_hWndEdit = NULL;
#endif
   m_dwLanguage = (DWORD)-1;
   m_pToolTip = NULL;
   MemZero (&m_memToolTipCur);
   memset (&m_rToolTipCur, 0, sizeof(m_rToolTipCur));
   memset (&m_siHorz, 0, sizeof(m_siHorz));
   memset (&m_siVert, 0, sizeof(m_siVert));
   m_fScrollTimer = FALSE;
   m_dwScrollDelay = TIMERSCROLLICON_DELAY;
}

CIconWindow::~CIconWindow (void)
{
   if (m_fScrollTimer && m_hWnd) {
      KillTimer (m_hWnd, TIMER_SCROLLICON);
      m_fScrollTimer = FALSE;
      m_dwScrollDelay = TIMERSCROLLICON_DELAY;
   }

   if (m_pToolTip) {
      delete m_pToolTip;
      if (IsWindow(m_hWnd))
         KillTimer (m_hWnd, CHECKTOOLIPTIMER);
   }

#if 0 // no longer used
   // save location in registry or user file
   PWSTR pszName = (PWSTR)m_memName.p;
   if (m_pMain && m_hWnd && pszName && pszName[0]) {
      CMem mem;
      MemZero (&mem);
      MemCat (&mem, gpszIconWindowPrefix);
      MemCat (&mem, pszName);

      CMMLNode2 node;
      node.NameSet (gpszIconWindow);
      // m_pMain->ChildLocSave (m_hWnd, &node, !IsWindowVisible(m_hWnd));

      // save it
      m_pMain->UserSave (TRUE, (PWSTR) mem.p, &node);
   }
#endif // 0

   // free up all the groups
   PCIWGroup *ppi = (PCIWGroup*) m_lPCIWGroup.Get(0);
   DWORD i;
   for (i = 0; i < m_lPCIWGroup.Num(); i++)
      delete ppi[i];
   m_lPCIWGroup.Clear();

   // delete the icon buttons
   PCIconButton *ppib = (PCIconButton*) m_lPCIconButton.Get(0);
   for (i = 0; i < m_lPCIconButton.Num(); i++)
      delete ppib[i];
   m_lPCIconButton.Clear();

   // delete the window
   if (m_hWnd)
      DestroyWindow (m_hWnd);
}


/************************************************************************************
IconWindowWndProc - internal windows callback for socket simulator
*/
LRESULT CALLBACK IconWindowWndProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
#ifdef _WIN64
   PCIconWindow p = (PCIconWindow) GetWindowLongPtr (hWnd, GWLP_USERDATA);
#else
   PCIconWindow p = (PCIconWindow) (LONG_PTR)GetWindowLongPtr (hWnd, GWLP_USERDATA);
#endif

   switch (uMsg) {
   case WM_CREATE: 
      {
         // store away the user data
         LPCREATESTRUCT lpcs = (LPCREATESTRUCT) lParam;
         SetWindowLongPtr (hWnd, GWLP_USERDATA, (LONG_PTR) lpcs->lpCreateParams);
         p = (PCIconWindow) lpcs->lpCreateParams;
      }
      break;
   };

   if (p)
      return p->WndProc (hWnd, uMsg, wParam, lParam);
   else
      return DefWindowProc (hWnd, uMsg, wParam, lParam);
}



/*************************************************************************************
CIconWindow::Init - Initializes the icon window.

inputs
   PCMainWindow         pMain - Main window
   PCMMLNode2            pNode - Node that's of type "IconWindow".
                        NOTE: It's the responsibility of CIconWindow to delete this node
   __int64              iTime - Time when this info came in, so can keep menu up to date
returns
   BOOL - TRUE if success, FALSE if error
*/
BOOL CIconWindow::Init (PCMainWindow pMain, PCMMLNode2 pNode, __int64 iTime)
{
   if (m_hWnd || m_pMain) {
      delete pNode;
      return FALSE;  // just to check
   }
   m_pMain = pMain;

   // create room
   WNDCLASS wc;
   memset (&wc, 0, sizeof(wc));
   wc.hInstance = ghInstance;
   wc.lpfnWndProc = IconWindowWndProc;
   wc.lpszClassName = "CircumrealityIconWindow";
   wc.style = CS_HREDRAW | CS_VREDRAW;
   wc.hIcon = NULL;
   wc.hCursor = NULL;
   wc.hbrBackground = NULL;
   RegisterClass (&wc);

   // get the ID
   PWSTR psz;
   CMem mem;
   psz = MMLValueGet (pNode, L"ID");
   if (!psz || !psz[0]) {
      delete pNode;
      return FALSE;
   }
   MemZero (&m_memID);
   MemCat (&m_memID, psz);

   psz = MMLValueGet (pNode, L"Name");
   if (!psz)
      psz = L"";
   MemZero (&m_memName);
   MemCat (&m_memName, psz);
   if (!mem.Required ((wcslen(psz) /*+wcslen(gpszF7)*/ +1)*sizeof(WCHAR))) {
      delete pNode;
      return FALSE;
   }
   WideCharToMultiByte (CP_ACP, 0, psz, -1, (char*)mem.p, (DWORD)mem.m_dwAllocated, 0,0);
   //if (m_fChatWindow)
   //   WideCharToMultiByte (CP_ACP, 0, gpszF7, -1,
   //      (char*)mem.p + strlen((char*)mem.p), (DWORD)(mem.m_dwAllocated - strlen((char*)mem.p)), 0,0);

   // get chat window so will be created properly
   m_fChatWindow = MMLValueGetInt (pNode, L"ChatWindow", 0);
   m_fTextUnderImages = !m_fChatWindow;
   m_fCanChatTo = MMLValueGetInt (pNode, L"CanChatTo", 0);

   // default location
   RECT r;
   POINT pCenter;
   BOOL fHidden = FALSE;
   GetClientRect (m_pMain->m_hWndPrimary, &r);  // assume primary for now
   pCenter.x = (r.left + r.right)/2;
   pCenter.y = (r.top + r.bottom)/2;
   r.left = (r.left + pCenter.x) / 2;
   r.right = (r.right + pCenter.x) / 2;
   r.top = (r.top + pCenter.y) / 2;
   r.bottom = (r.bottom + pCenter.y) / 2;

   // try getting the location from the node
   BOOL fTitle = TRUE;
   DWORD dwMonitor;
   m_pMain->ChildLocGet (TW_ICONWINDOW, (PWSTR)m_memID.p, &dwMonitor, &r, &fHidden, &fTitle);

#if 0 // no longer used
   // try loading the location from the user file
   CMem memFile;
   MemZero (&memFile);
   MemCat (&memFile, gpszIconWindowPrefix);
   MemCat (&memFile, (PWSTR)m_memName.p);
   PCMMLNode2 pUser = m_pMain->UserLoad (TRUE, (PWSTR)memFile.p);
   if (pUser) {
      // m_pMain->ChildLocGet (pUser, &r, &fHidden);
      delete pUser;
   }
#endif // 0

   // BUGFIX - temporarily turn off saving so doesnt mess up settings
   gfChildLocIgnoreSave = TRUE;

   m_pMain->ChildShowTitleIfOverlap (NULL, &r, dwMonitor, fHidden, &fTitle);
   m_hWnd = CreateWindowEx (
      (fTitle ? WS_EX_IFTITLE : WS_EX_IFNOTITLE) | WS_EX_ALWAYS,
      wc.lpszClassName, (char*)mem.p,
      WS_ALWAYS | (fTitle ? WS_IFTITLECLOSE : 0) | WS_CLIPCHILDREN | WS_CLIPSIBLINGS |
      (fHidden ? 0 : WS_VISIBLE),
      r.left , r.top , r.right - r.left , r.bottom - r.top ,
      dwMonitor ? pMain->m_hWndSecond : pMain->m_hWndPrimary,
      NULL, ghInstance, (PVOID) this);

   // BUGFIX - temporarily turn off saving so doesnt mess up settings
   gfChildLocIgnoreSave = FALSE;

   if (!m_hWnd) {
      delete pNode;
      return FALSE;
   }

   //m_pMain->ChildLocSave (m_hWnd, TW_ICONWINDOW, (PWSTR)m_memID.p, NULL);

   // bring to foreground
   SetWindowPos (m_hWnd, (HWND)HWND_TOP, 0,0,0,0,
      SWP_NOMOVE | SWP_NOSIZE);
   SetActiveWindow (m_hWnd);


   // update the contents
   return Update (pNode, iTime);
}


/*************************************************************************************
CIconWindow::Update - Updates the icon view based on the given MML node, which
is a CircumrealityIconWindow() node.

inputs
   PCMMLNOde            pNode - Node
                        NOTE: It's the responsibility of CIconWindow to delete this node
   __int64              iTime - Time when this message came in so can keep menu up to date
returns
   BOOL - TRUE if success
*/
BOOL CIconWindow::Update (PCMMLNode2 pNode, __int64 iTime)
{
   BOOL fAppend = MMLValueGetInt (pNode, L"Append", 0);
   BOOL fAutoShow = MMLValueGetInt (pNode, L"AutoShow", 0);
   m_fChatWindow = MMLValueGetInt (pNode, L"ChatWindow", 0);
   m_fTextUnderImages = !m_fChatWindow;
   m_fCanChatTo = MMLValueGetInt (pNode, L"CanChatTo", 0);
   m_lLanguage.Clear();

   // get the text for the window and set, just in case hase changed
   PWSTR psz;
   CMem mem;
   psz = MMLValueGet (pNode, L"Name");
   if (!psz)
      psz = L"";
   if (!mem.Required ((wcslen(psz) /*+wcslen(gpszF7)*/ +1)*sizeof(WCHAR))) {
      delete pNode;
      return FALSE;
   }
   WideCharToMultiByte (CP_ACP, 0, psz, -1, (char*)mem.p, (DWORD)mem.m_dwAllocated, 0,0);
   //if (m_fChatWindow)
   //   WideCharToMultiByte (CP_ACP, 0, gpszF7, -1,
   //      (char*)mem.p + strlen((char*)mem.p), (DWORD)(mem.m_dwAllocated - strlen((char*)mem.p)), 0,0);
   SetWindowText (m_hWnd, (char*)mem.p);

   // clone the list of groups and clear what have
   CListFixed lGroupOld;
   if (!fAppend) {
      lGroupOld.Init (sizeof (PCIWGroup), m_lPCIWGroup.Get(0), m_lPCIWGroup.Num());
      m_lPCIWGroup.Clear();
   }
   PCIWGroup *ppOld = (PCIWGroup*)lGroupOld.Get(0);

#ifdef _DEBUG
   OutputDebugString ("\r\n Group:");
#endif

   // go through all the groups in the list
   //CBTree tGroups;
   DWORD i, j;
   PCMMLNode2 pSub;
   for (i = 0; i < pNode->ContentNum(); i++) {
      pSub = NULL;
      pNode->ContentEnum (i, &psz, &pSub);
      if (!pSub)
         continue;
      psz = pSub->NameGet();
      if (!psz)
         continue;
      
      if (m_fChatWindow && !_wcsicmp(psz, L"Language")) {
         psz = pSub->AttribGetString (L"v");
         if (!psz)
            continue;
         m_lLanguage.Add (psz, (wcslen(psz)+1)*sizeof(WCHAR));
         continue;
      }
      else if (!_wcsicmp(psz, L"Group")) {
         // get the group name
         psz = MMLValueGet (pSub, L"Name");
         if (!psz)
            psz = L"";  // must have a name

         BOOL fCanChatTo = MMLValueGetInt (pSub, L"CanChatTo", 0);

         // remember that have this group
         // tGroups.Add (psz, &i, 0);

         // see if the group exists already
         PCIWGroup *ppg = fAppend ? (PCIWGroup*)m_lPCIWGroup.Get(0) : ppOld;
         DWORD dwNum = fAppend ? m_lPCIWGroup.Num() : lGroupOld.Num();
         for (j = 0; j < dwNum; j++) {
            if (!ppg[j])
               continue;   // no longer there
            if (!_wcsicmp((PWSTR)ppg[j]->m_memName.p, psz))
               break;
         }
         if (j < dwNum) {
            PCIWGroup pFind = ppg[j];

            // potentially add this onto existing list
            if (!fAppend) {
               ppg[j] = NULL;
               m_lPCIWGroup.Add (&pFind);
            }

            UpdateGroup (pFind, pSub, fAppend, iTime);
#ifdef _DEBUG
         OutputDebugStringW (psz);
         OutputDebugString (" (old), ");
#endif
            continue;
         }

         // else, add group
         PCIWGroup pNew = new CIWGroup;
         if (!pNew)
            continue;
         if (!pNew->Init (this, psz, fCanChatTo)) {
            delete pNew;
            continue;
         }
         m_lPCIWGroup.Add (&pNew);
         UpdateGroup (pNew, pSub, fAppend, iTime);
#ifdef _DEBUG
         OutputDebugStringW (psz);
         OutputDebugString (" (new), ");
#endif
      } // if group
   } // i

#ifdef _DEBUG
   OutputDebugString ("\r\n");
#endif
   // if there are any groups not listed on the update then delete them
   for (i = 0; i < lGroupOld.Num(); i++)
      if (ppOld[i])
         delete ppOld[i];
   //if (!fAppend) for (i = m_lPCIWGroup.Num()-1; i < m_lPCIWGroup.Num(); i--) {
   //   PCIWGroup pg = *((PCIWGroup*)m_lPCIWGroup.Get(i));
   //   if (!tGroups.Find((PWSTR)pg->m_memName.p)) {
   //      // delete this group because no longer in list
   //      delete pg;
   //      m_lPCIWGroup.Remove (i);
   //   }
   //} // i


   // finally
   if (fAutoShow)
      m_pMain->ChildShowWindow (m_hWnd, TW_ICONWINDOW, (PWSTR)m_memID.p, SW_SHOW);
   PositionIcons (TRUE, FALSE);

   // Change in m_fChatWindow causes some controls to be shown/hidden
   VerbButtonsArrange ();

   delete pNode;

   // if this is a chat window then go through the objects and keep their
   // position information around so that can use it to split the speakers amongst
   // the left and right channels
   if (m_fChatWindow) {
      CListFixed lID;
      lID.Init (sizeof(GUID));

      DWORD i, j;
      PCIWGroup *ppg = (PCIWGroup*)m_lPCIWGroup.Get(0);
      for (i = 0; i < m_lPCIWGroup.Num(); i++) {
         PCIWGroup pg = ppg[i];
         RECT *pr = (RECT*) pg->m_lVIRect.Get(0);

         for (j = 0; j < pg->m_lPCVisImage.Num(); j++, pr++) {
            PCVisImage pvi = *((PCVisImage*) pg->m_lPCVisImage.Get(j));
            if (IsEqualGUID (pvi->m_gID, GUID_NULL))
               continue;

            lID.Add (&pvi->m_gID);
         } // j
      } // i

      // remove the last element since know it's the current speaker
      // BUGFIX: Changed from first to last
      if (lID.Num())
         lID.Remove (lID.Num()-1);

      m_pMain->m_pAT->TTSSurroundSet ((GUID*)lID.Get(0), lID.Num());
   } // if chat window

   return TRUE;
}




/*************************************************************************************
CIconWindow::UpdateGroup - Updates the icon view based on the given MML node, which
is a CircumrealityIconWindow() node.

inputs
   PCIWGroup            pg - Group being updated
   PCMMLNOde            pNode - Node.
   BOOL                 fAppend - If TRUE then append, leaving existing objects (although
                           exact matches are replaced). If FALSE then clear out objects
                           that aren't referenced in pNode
   __int64              iTime - Time when this message came in so can keep menu with right time
returns
   BOOL - TRUE if success
*/
BOOL CIconWindow::UpdateGroup (PCIWGroup pg, PCMMLNode2 pNode, BOOL fAppend, __int64 iTime)
{
   // go through and find all objectdisplays used
   DWORD i, j;
   PCMMLNode2 pSub;
   PWSTR psz;
   CHashGUID hID;
   hID.Init (sizeof(DWORD));
   for (i = 0; i < pNode->ContentNum(); i++) {
      pSub = NULL;
      pNode->ContentEnum (i, &psz, &pSub);
      if (!pSub)
         continue;
      psz = pSub->NameGet();
      if (!psz || _wcsicmp(psz, CircumrealityObjectDisplay()))
         continue;   // only care about object display

      // if get here have an object display... see if it already exists...
      GUID gID;
      memset (&gID, 0, sizeof(gID));
      MMLValueGetBinary (pSub, L"ID", (PBYTE)&gID, sizeof(gID));

      // remember that found this
      hID.Add (&gID, &i);

      // see if have match
      PCVisImage *ppv = (PCVisImage*) pg->m_lPCVisImage.Get(0);
      for (j = 0; j < pg->m_lPCVisImage.Num(); j++)
         if (IsEqualGUID (ppv[j]->m_gID, gID))
            break;

      // call object display
      m_pMain->ObjectDisplay (
         pSub,
         (j < pg->m_lPCVisImage.Num()) ? NULL : pg,
         NULL,
         TRUE,
         FALSE,
         NULL,
         (j < pg->m_lPCVisImage.Num()) ? ppv[j]->m_fCanChatTo : FALSE,
         iTime);
   } // i

   // remove any GUIDs that are no longer listed
   if (!fAppend) for (i = pg->m_lPCVisImage.Num()-1; i < pg->m_lPCVisImage.Num(); i--) {
      PCVisImage pvi = *((PCVisImage*)pg->m_lPCVisImage.Get(i));
      if (hID.FindIndex (&pvi->m_gID) == -1) {
         // NOTE: This lets the main window delete the vis image, since it
         // maintains the master list
         DWORD dwIndex = -1;
         if (pg->m_pIW && pg->m_pIW->m_pMain)
            dwIndex = pg->m_pIW->m_pMain->VisImageFindPtr (pvi);
         if (dwIndex != -1)
            pg->m_pIW->m_pMain->VisImageDelete (dwIndex);
         else
            delete pvi; // shouldnt happen
         pg->m_lPCVisImage.Remove (i);
         pg->m_lVIRect.Remove (i);
      }
   } // i

   return TRUE;
}

/*************************************************************************************
CIconWindow::MouseOver - Returns the PCVisImage the mouse is over.

inputs
   int         x - client
   int         y - client
   BOOL        *pfOverMenu - If TRUE then it's over a menu-button location
returns
   PCVisImage - What image the mouse is over, or NULL if none
*/
PCVisImage CIconWindow::MouseOver (int x, int y, BOOL *pfOverMenu)
{
   PCVisImage pOverImage = NULL, pOverMenu = NULL;

   // make sure in the client
   RECT rClient;
   GetClientRect (m_hWnd, &rClient);
   POINT p;
   p.x = x;
   p.y = y;
   if (!PtInRect (&rClient, p))
      return NULL;

   DWORD i, j;
   PCIWGroup *ppg = (PCIWGroup*)m_lPCIWGroup.Get(0);
   for (i = 0; i < m_lPCIWGroup.Num(); i++) {
      PCIWGroup pg = ppg[i];
      PCVisImage *ppv = (PCVisImage*) pg->m_lPCVisImage.Get(0);
      for (j = 0; j < pg->m_lPCVisImage.Num(); j++) {
         RECT r;
         ppv[j]->RectGet (&r);
         if ((x >= r.left) && (x <= r.right) && (y >= r.top) && (y <= r.bottom))
            pOverImage = ppv[j];
         // BUGFIX - Allow menu hotspot to be entire title
         //else if ((x >= r.left) && (x <= r.left+MENUICONSIZE) &&
         //   (y >= r.bottom) && (y <= r.bottom+MENUICONSIZE) && ppv[j]->MenuGet())
         //   pOverMenu = ppv[j];
         else if ((x >= r.left) && (x <= r.right) &&
            (y >= r.bottom) && (y <= r.bottom+m_iTextHeight*2) && ppv[j]->MenuGet())
            pOverMenu = ppv[j];
      } // j
   } // i

   if (pOverImage) {
      *pfOverMenu =FALSE;
      return pOverImage;
   }
   if (pOverMenu) {
      *pfOverMenu = TRUE;
      return pOverMenu;
   }

   return NULL;
}

/*************************************************************************************
CIconWindow::WndProc - Hand the windows messages
*/
LRESULT CIconWindow::WndProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
   switch (uMsg) {
   case WM_CREATE:
      {
         m_hWnd = hWnd;

         // BUGFIX - Moved this test to window creation
         // font height
         HDC hDC = GetDC (m_hWnd);
         HFONT hOld = (HFONT) SelectObject (hDC, m_pMain->m_hFont);
         TEXTMETRIC tm;
         memset (&tm, 0, sizeof(tm));
         GetTextMetrics (hDC, &tm);
         SelectObject (hDC, hOld);
         ReleaseDC (m_hWnd, hDC);
         m_iTextHeight = tm.tmHeight;
      }

      break;

   case WM_PAINT:
      {
         PAINTSTRUCT ps;
         HDC hDC = BeginPaint (hWnd, &ps);
         Paint (hWnd, hDC, &ps.rcPaint);
         EndPaint (hWnd, &ps);
      }
      return 0;

   case WM_MOVING:
      if (m_pMain->TrapWM_MOVING (hWnd, lParam, FALSE))
         return TRUE;
      break;
   case WM_SIZING:
      if (m_pMain->TrapWM_MOVING (hWnd, lParam, TRUE))
         return TRUE;
      break;

   case WM_SIZE:
      {
         m_pMain->ChildLocSave (hWnd, TW_ICONWINDOW, (PWSTR)m_memID.p, NULL);

         // move the scrollbars
         RECT rClient;
         GetClientRect (m_hWnd, &rClient);
#undef GetSystemMetrics
         int iHScroll = GetSystemMetrics (SM_CYHSCROLL);
         int iVScroll = GetSystemMetrics (SM_CXVSCROLL);
         if (m_hWndHScroll)
            rClient.bottom -= iHScroll;   // since would be visible
         if (m_hWndVScroll)
            rClient.right -= iVScroll;

         // reduce client by chat window
         ClientLoc (&rClient, TRUE);

         if (m_hWndHScroll)
            MoveWindow (m_hWndHScroll, rClient.left, rClient.bottom, rClient.right - rClient.left, iHScroll, TRUE);
         if (m_hWndVScroll)
            MoveWindow (m_hWndVScroll, rClient.right, 0, iVScroll, rClient.bottom - rClient.top, TRUE);


         PositionIcons (TRUE, FALSE);
         VerbButtonsArrange ();
      }
      break;

   case WM_COMMAND:
      if (m_pMain->m_pResVerbChat && (wParam >= 1000) && (wParam < 1000 + m_pMain->m_pResVerbChat->m_lPCResVerbIcon.Num())) {
         DWORD i = wParam;
         PCResVerbIcon pvi = *((PCResVerbIcon*)m_pMain->m_pResVerbChat->m_lPCResVerbIcon.Get(i-1000));
         PCIconButton *ppib = (PCIconButton*)m_lPCIconButton.Get(i-1000);

         if (!pvi->m_fHasClick) {
            // deselect the old one
            m_pMain->VerbDeselect ();

            if (m_pMain->m_fMessageDiabled || m_pMain->m_fMenuExclusive) {
               BeepWindowBeep (ESCBEEP_DONTCLICK);
               return 0;
            }


            // no object, so simple click
            m_pMain->SendTextCommand (pvi->m_lid, (PWSTR)pvi->m_memDo.p, NULL, NULL, NULL, TRUE, TRUE, TRUE);
            m_pMain->HotSpotDisable();
            BeepWindowBeep (ESCBEEP_LINKCLICK);
            return 0;
         }

         // if it's already selected then deselect
         if ((m_pMain->m_dwVerbSelected == i-1000) && m_pMain->m_fVerbSelectedIcon) {
            // deselect this
            m_pMain->VerbDeselect();
         }
         else {
            // select
            m_pMain->VerbSelect (NULL, NULL, NULL, i-1000, TRUE);

            // select this
            if (ppib && ppib[0])
               ppib[0]->FlagsSet (ppib[0]->FlagsGet() | IBFLAG_REDARROW);
         }
         BeepWindowBeep (ESCBEEP_MENUOPEN);  // note: have different beep
         m_pMain->VerbTooltipUpdate ();
         m_pMain->HotSpotTooltipUpdate ();
         return 0;
      }
      break;

   case WM_MOVE:
      m_pMain->ChildLocSave (hWnd, TW_ICONWINDOW, (PWSTR)m_memID.p, NULL);

      PositionIcons (TRUE, TRUE);
      break;

   case WM_CHAR:
      {
#ifdef UNIFIEDTRANSCRIPT
      return SendMessage (GetParent (hWnd), uMsg, wParam, lParam);
#else
         if (!m_fChatWindow)
            // send this up to parent... doesn't seem to get called, so no worry
            return SendMessage (GetParent (hWnd), uMsg, wParam, lParam);

         // else, it's a chat window
         if (m_hWndEdit && (wParam == L'\r') && IsWindowVisible (m_hWnd)) {
            CMem memANSI, memWide;
            if (!memANSI.Required (GetWindowTextLength (m_hWndEdit)+1))
               return 0;
            GetWindowText (m_hWndEdit, (char*)memANSI.p, memANSI.m_dwAllocated);
            DWORD dwLen = strlen((char*)memANSI.p);
            SetWindowText (m_hWndEdit, "");

            if (!memWide.Required ((dwLen+1)*sizeof(WCHAR)))
               return 0;
            MultiByteToWideChar (CP_ACP, 0, (char*)memANSI.p, -1, (PWSTR) memWide.p, memWide.m_dwAllocated);

            if (!Speak ((PWSTR)memWide.p))
               EscChime (ESCCHIME_WARNING);
         } // if edit and pres senter
#endif // UNIFIEDTRANSCRIPT
      }
      break;

#ifndef UNIFIEDTRANSCRIPT
   case WM_MOUSEACTIVATE:
      if (m_hWndEdit && (GetFocus() != m_hWndEdit)) {
         SetFocus (m_hWndEdit);
      }
      break;

   case WM_SETFOCUS:
      if (m_hWndEdit && (GetFocus() != m_hWndEdit))
         SetFocus (m_hWndEdit);
      break;

   case WM_SHOWWINDOW:
      // if showing the window set focus to the edit control
      if (wParam && m_hWndEdit && (GetFocus() != m_hWndEdit))
         SetFocus (m_hWndEdit);
      break;
#endif // !UNIFIEDTRANSCRIPT

   case WM_CLOSE:
      // just hide this
      m_pMain->ChildShowWindow (m_hWnd, TW_ICONWINDOW, (PWSTR)this->m_memID.p, SW_HIDE);

      // show bottom pane so player knows is closed
      if (m_pMain->m_pSlideBottom)  // BUGFIX - In case doesn't exist
         m_pMain->m_pSlideBottom->SlideDownTimed (1000);

      return 0;

   case WM_TIMER:
      if (wParam == CHECKTOOLIPTIMER) {
         POINT p;
         GetCursorPos (&p);
         ScreenToClient (m_hWnd, &p);
         TooltipUpdate (p);
         return 0;
      }
      else if (wParam == TIMER_SCROLLICON) {
         POINT p;
         fp fRight, fDown;
         GetCursorPos (&p);
         ScreenToClient (m_hWnd, &p);
         CursorToScroll (p, &fRight, &fDown);

         // if 0 then done
         if (!fRight && !fDown) {
            if (m_fScrollTimer && m_hWnd) {
               KillTimer (m_hWnd, TIMER_SCROLLICON);
               m_fScrollTimer = FALSE;
               m_dwScrollDelay = TIMERSCROLLICON_DELAY;
            }
            return 0;
         }

         // if haven't waited long enough then done
         if (m_dwScrollDelay) {
            m_dwScrollDelay--;
            return 0;
         }

         // else, scroll... know that no scrollbars
         m_siHorz.nPos += (int)((fp)m_siHorz.nPage * fRight * TIMERSCROLLICON_INTERVAL / TIMERSCROLLICON_RATE);
         m_siHorz.nPos = min((int)(m_siHorz.nMax - m_siHorz.nPage), (int)m_siHorz.nPos);
         m_siHorz.nPos = max(m_siHorz.nPos, m_siHorz.nMin);

         m_siVert.nPos += (int)((fp)m_siVert.nPage * fDown * TIMERSCROLLICON_INTERVAL / TIMERSCROLLICON_RATE);
         m_siVert.nPos = min((int)(m_siVert.nMax - m_siVert.nPage), (int)m_siVert.nPos);
         m_siVert.nPos = max(m_siVert.nPos, m_siVert.nMin);


         // set scorllbars just in case... shouldnt happen
         m_siHorz.fMask = m_siVert.fMask = SIF_ALL;
         if (m_hWndHScroll)
            SetScrollInfo (m_hWndHScroll, SB_CTL, &m_siHorz, TRUE);
         if (m_hWndVScroll)
            SetScrollInfo (m_hWndVScroll, SB_CTL, &m_siVert, TRUE);

         // update everything's location
         PositionIcons (FALSE, FALSE);

         return 0;
      }
      break;

   case WM_MOUSEMOVE:
      {
         // update the tooltip
         POINT pt;
         pt.x = (short)LOWORD(lParam);
         pt.y = (short)HIWORD(lParam);
         TooltipUpdate (pt);

         // pan cursor
         fp fRight, fDown;
         CursorToScroll (pt, &fRight, &fDown);
         if (fRight || fDown) {
            DWORD dwCursor;
            if (fabs(fRight) > fabs(fDown))
               dwCursor = (fRight > 0) ? IDC_CURSORPOINTRIGHT : IDC_CURSORPOINTLEFT;
            else
               dwCursor = (fDown > 0) ? IDC_CURSORROTDOWN : IDC_CURSORROTUP;
            SetCursor (LoadCursor (ghInstance, MAKEINTRESOURCE(dwCursor)));

            // set the timer
            if (!m_fScrollTimer) {
               m_fScrollTimer = TRUE;
               m_dwScrollDelay = TIMERSCROLLICON_DELAY;
               SetTimer (m_hWnd, TIMER_SCROLLICON, TIMERSCROLLICON_INTERVAL, 0);
            }

            return 0;
         }

         HCURSOR hc;

         // if it's over the button area then show cursor

         BOOL fOverMenu = FALSE;
         PCVisImage pView = NULL;
         RECT r;
         int iLeft, iBottom;
         ChatWindowToolbarLoc (&iLeft, &iBottom);
         GetClientRect (m_hWnd, &r);
         if (iBottom)
            r.top = r.bottom - iBottom;
         else
            r.right = r.left + iLeft;
#ifdef ENABLECHATVERB
         if (m_fChatWindow && PtInRect (&r, pt))
            hc = m_pMain->m_hCursorMenu;
         else 
#endif
            {
            pView = MouseOver (pt.x, pt.y, &fOverMenu);

            if (pView) {
               if (fOverMenu)
                  hc = m_pMain->m_hCursorMenu;
               else
                  hc = ((PWSTR)m_pMain->m_memVerbShow.p)[0] ? m_pMain->m_hCursorHand : m_pMain->m_hCursorZoom;
                        // BUGFIX - Was m_hCursorMenu instead of m_hCursorZoom
            }
            else
               hc = m_pMain->m_hCursorNo;
         }

         SetCursor (hc);

         if (pView && !fOverMenu && !IsEqualGUID(pView->m_gID, GUID_NULL)) {
            POINT p;
            p.x = (short)LOWORD(lParam);
            p.y = (short)HIWORD(lParam);
            ClientToScreen (m_hWnd, &p);
            m_pMain->VerbTooltipUpdate (m_pMain->ChildOnMonitor(hWnd), TRUE, p.x, p.y);
         }
         else
            m_pMain->VerbTooltipUpdate ();   // to clear
         m_pMain->HotSpotTooltipUpdate ();
      }
      break;


   case WM_LBUTTONDOWN:
      {
         // set this as the foreground window
         SetWindowPos (hWnd, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);

         BOOL fOverMenu;
         PCVisImage pView;
         POINT pt;
         pt.x = (short)LOWORD(lParam);
         pt.y = (short)HIWORD(lParam);

#ifdef ENABLECHATVERB
         // if chat window, can bring up menu to control what buttons
         if (m_fChatWindow) {
#ifndef UNIFIEDTRANSCRIPT
            // will need to set keyboard focus to edit
            if (m_hWndEdit && (GetFocus() != m_hWndEdit))
               SetFocus (m_hWndEdit);
#endif

            RECT r;
            int iLeft, iBottom;
            ChatWindowToolbarLoc (&iLeft, &iBottom);
            GetClientRect (m_hWnd, &r);
            if (iBottom)
               r.top = r.bottom - iBottom;
            else
               r.right = r.left + iLeft;

            if (PtInRect (&r, pt)) {
               BeepWindowBeep (ESCBEEP_MENUOPEN);

               HMENU hMenu = CreatePopupMenu ();

#ifndef UNIFIEDTRANSCRIPT
               // append the languages
               if (m_lLanguage.Num() >= 2) {
                  DWORD i;
                  CMem mem;

                  // most appropriate language
                  AppendMenu (hMenu,
                     ((m_dwLanguage == (DWORD)-1) ? MF_CHECKED : 0) | MF_STRING | MF_ENABLED,
                     2000, (char*)"Most appropriate language");

                  for (i = 0; i < m_lLanguage.Num(); i++) {
                     PWSTR psz = (PWSTR)m_lLanguage.Get(i);
                     if (!mem.Required ((wcslen(psz)+1)*sizeof(WCHAR)))
                        continue;
                     WideCharToMultiByte (CP_ACP, 0, psz, -1, (char*)mem.p, mem.m_dwAllocated, 0,0);

                     AppendMenu (hMenu,
                        ((m_dwLanguage == i) ? MF_CHECKED : 0) | MF_STRING | MF_ENABLED,
                        i + 2001, (char*)mem.p);
                  } // i
                  AppendMenu (hMenu, MF_SEPARATOR, 0, NULL);
               }
#endif // !UNIFIEDTRANSCRIPT

               AppendMenu (hMenu, MF_STRING | MF_ENABLED, ID_VERBWINDOW_CUSTOMIZETOOLBAR, "Customize toolbar...");

               ClientToScreen (m_hWnd, &pt);
               int iRet;
               iRet = TrackPopupMenu (hMenu, TPM_LEFTALIGN | TPM_TOPALIGN | TPM_LEFTBUTTON | TPM_NONOTIFY | TPM_RETURNCMD, 
                  pt.x, pt.y, 0, m_hWnd, NULL);

               DestroyMenu (hMenu);
               if (!iRet)
                  return 0;

               // if select language act on it
               if ((iRet >= 2000) && (iRet <= 2000 + (int) m_lLanguage.Num())) {
                  if (iRet == 2000)
                     m_dwLanguage = (DWORD)-1;
                  else
                     m_dwLanguage = (DWORD)iRet - 2001;
                  return 0;
               }

               switch (iRet) {
               case ID_VERBWINDOW_CUSTOMIZETOOLBAR:
                  {
                     if (!m_pMain->m_pResVerbChat)
                        m_pMain->m_pResVerbChat = new CResVerb;
                     if (!m_pMain->m_pResVerbChat)
                        break;

                     // come up with a default language
                     LANGID lid = DEFLANGID;
                     PCResVerbIcon *ppr = (PCResVerbIcon*)m_pMain->m_pResVerbChat->m_lPCResVerbIcon.Get(0);
                     if (ppr)
                        lid = ppr[0]->m_lid;

                     m_pMain->m_pResVerbChat->Edit (m_pMain->m_hWndPrimary, lid, FALSE, NULL, TRUE, m_pMain->m_pResVerbChatSent);

                     // will need to alert chat window that have new chat verbs
                     PCIconWindow *ppi = (PCIconWindow*)m_pMain->m_lPCIconWindow.Get(0);
                     DWORD i;
                     for (i = 0; i < m_pMain->m_lPCIconWindow.Num(); i++)
                        ppi[i]->VerbChatNew ();
                  }
                  break;
               } // switch
               return 0;
            }  // if point in menu rect
         } // if chat window
#endif // ENABLECHATVERB

         pView = MouseOver (pt.x, pt.y, &fOverMenu);
         if (!pView) {
            SetWindowPos (hWnd, (HWND)HWND_TOP, 0,0,0,0,
               SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE);

            BeepWindowBeep (ESCBEEP_DONTCLICK);

            return 0;
         }

         if (!fOverMenu) {
            // BUGFIX - Click on menu
            if (m_pMain->VerbClickOnObject (&pView->m_gID))
               return 0;

            // BUGFIX - enlarge this
            m_pMain->SetMainWindow (pView);
         }
         else
            m_pMain->ContextMenuDisplay (hWnd, pView);

         return 0;
      }
      break;

   case WM_HSCROLL:
   case WM_VSCROLL:
      {
         HWND hWndScroll = (HWND) lParam;
         BOOL fHorz = (hWndScroll == m_hWndHScroll);

         // get the scrollbar info
         SCROLLINFO si;
         memset (&si, 0, sizeof(si));
         si.cbSize = sizeof(si);
         si.fMask = SIF_ALL;
         GetScrollInfo (hWndScroll, SB_CTL, &si);

         // what's the new position?
         switch (LOWORD(wParam)) {
         default:
            return 0;
         case SB_ENDSCROLL:
            return 0;
         case SB_LINEUP:
         //case SB_LINELEFT:
            si.nPos  -= max(si.nPage / 8, 1);
            break;

         case SB_LINEDOWN:
         //case SB_LINERIGHT:
            si.nPos  += max(si.nPage / 8, 1);
            break;

         case SB_PAGELEFT:
         //case SB_PAGEUP:
            si.nPos  -= si.nPage;
            break;

         case SB_PAGERIGHT:
         //case SB_PAGEDOWN:
            si.nPos  += si.nPage;
            break;

         case SB_THUMBPOSITION:
            si.nPos = si.nTrackPos;
            break;
         case SB_THUMBTRACK:
            si.nPos = si.nTrackPos;
            break;
         }

         // don't go beyond min and max
         si.nPos = min((int)(si.nMax - si.nPage), (int)si.nPos);
         si.nPos = max(si.nPos,si.nMin);

         si.fMask = SIF_ALL;
         SetScrollInfo (hWndScroll, SB_CTL, &si, TRUE);

         // store this away
         if (fHorz)
            m_siHorz = si;
         else
            m_siVert = si;

         // update everything's location
         PositionIcons (FALSE, FALSE);

         return 0;
      }
      break;



   } // switch uMsg


   // else
   return DefWindowProc (hWnd, uMsg, wParam, lParam);
}


/*************************************************************************************
CIconWindow::Speak - Call this to have the chat window speak a line

inputs
   PWSTR       pszText - Text to speak
returns
   BOOL - TRUE if success
*/
BOOL CIconWindow::Speak (PWSTR pszText)
{
   if (m_pMain->m_fMessageDiabled || m_pMain->m_fMenuExclusive)
      return FALSE;

   if (!wcslen(pszText))
      return FALSE;

// NOTE: Disabling enter if it's been too soon since last command
#ifdef _DEBUG
   OutputDebugString ("\r\nEnter pressed in chat");
#endif

   // find the object
   PCVisImage pvTo = NULL;
   DWORD i, j;
   PCIWGroup *ppg = (PCIWGroup*)m_lPCIWGroup.Get(0);
   BOOL fFoundOld = FALSE;
   PCIWGroup pg;
   PCVisImage pvi;
   for (i = 0; i < m_lPCIWGroup.Num(); i++) {
      pg = ppg[i];
      for (j = 0; j < pg->m_lPCVisImage.Num(); j++) {
         pvi = *((PCVisImage*) pg->m_lPCVisImage.Get(j));
         if (IsEqualGUID(pvi->m_gID, m_gSpeakWith)) {
            pvTo = pvi;
            break;
         }
      } // j

      if (pvTo)
         break;
   } // i
   if (!pvTo && (m_iSpeakMode > 0))
      m_iSpeakMode = 0; // cant really whisper to nothing

   // BUGFIX - If not speaking to anything specifically, then see what the main window has
   if (!pvTo) {
      PCVisImage pviMain = m_pMain->FindMainVisImage ();
      ppg = (PCIWGroup*)m_lPCIWGroup.Get(0);
      if (pviMain && !IsEqualGUID(pviMain->m_gID, GUID_NULL)) for (i = 0; i < m_lPCIWGroup.Num(); i++) {
         pg = ppg[i];
         if (!pg->m_fCanChatTo)
            continue;   // can't chat to

         for (j = 0; j < pg->m_lPCVisImage.Num(); j++) {
            pvi = *((PCVisImage*) pg->m_lPCVisImage.Get(j));
            if (IsEqualGUID(pvi->m_gID, pviMain->m_gID)) {
               pvTo = pvi;
               break;
            }
         } // j

         if (pvTo)
            break;
      } // i
   } // if !pvTo


   // use ansi again, but concatenate
   CMem memANSI;
   MemZero (&memANSI);
   if (pvTo)
      MemCat (&memANSI, (m_iSpeakMode == 1) ? L"Whisper" : L"Say");
   else
      MemCat (&memANSI, (m_iSpeakMode == -1) ? L"Yell" : L"Say");
   MemCat (&memANSI, L" \"");
   MemCatSanitize (&memANSI, pszText);
   MemCat (&memANSI, L"\"");
   if (pvTo)
      MemCat (&memANSI, L" to <Object>");

   // may need to append language if more than two
   // BUGFIX - Check for "most appropriate"
   if ((m_dwLanguage != (DWORD)-1) && (m_lLanguage.Num() >= 2)) {
      if (m_dwLanguage >= m_lLanguage.Num())
         m_dwLanguage = 0;

      PWSTR psz = (PWSTR)m_lLanguage.Get(m_dwLanguage);

      // BUGFIX - if find spaces or non-standard characters then in quotes
      CMem mem;
      BOOL fNeedQuotes = FALSE;
      PWSTR pCur;
      MemZero (&mem);
      MemCatSanitize (&mem, psz);
      if (_wcsicmp((PWSTR)mem.p, psz))
         fNeedQuotes = TRUE;
      for (pCur = psz; *pCur; pCur++)
         if (!iswalpha(*pCur))
            fNeedQuotes = TRUE;

      MemCat (&memANSI, L" in ");
      if (fNeedQuotes)
         MemCat (&memANSI, L"\"");
      MemCat (&memANSI, (PWSTR)mem.p);
      if (fNeedQuotes)
         MemCat (&memANSI, L"\"");
   }

   m_pMain->SendTextCommand (1033, (PWSTR)memANSI.p, NULL, pvTo ? &pvTo->m_gID : NULL, NULL, TRUE, TRUE, TRUE);
      // NOTE: Specifically used english because of commands
   m_pMain->HotSpotDisable();
   
   return TRUE;
}


/*************************************************************************************
CIconWindow::ClientLoc - Figure out the client location, using ChatWindowToolbarLoc

inputs
   RECT        *pr - Filled with the rect
   BOOL        fUseExisting - If TRUE, then assume pr is filled with valid info
*/
void CIconWindow::ClientLoc (RECT *pr, BOOL fUseExisting)
{
   if (!fUseExisting)
      GetClientRect (m_hWnd, pr);

#ifdef ENABLECHATVERB
   if (m_fChatWindow) {
      int iLeft, iBottom;
      ChatWindowToolbarLoc (&iLeft, &iBottom);
      pr->left += iLeft;
      pr->bottom -= iBottom;
   }
#endif // ENABLECHATVERB
}

/*************************************************************************************
CIconWindow::ChatWindowToolbarLoc - Fills in the size (horizontal and vertical) of chat
window location. One of these will be set to zero

int
   int      *piLeft - How much the chat window occupies on the left side
   int      *piBottom - How much the chat window occupies on the bottom

returns
   not
*/
void CIconWindow::ChatWindowToolbarLoc (int *piLeft, int *piBottom)
{
   *piLeft = *piBottom = 0;

#ifdef ENABLECHATVERB
   RECT rClient;
   GetClientRect (m_hWnd, &rClient);
   int iWidth = max((rClient.right - rClient.left) / CHATVERBSIZE, 1);
   int iHeight = max((rClient.bottom - rClient.top) / CHATVERBSIZE, 1);

   DWORD dwNumVerb = m_pMain->m_pResVerbChat->m_lPCResVerbIcon.Num();
   dwNumVerb++;   // to include space for menu

   // figure out if it's wider or taller
   // BUGFIX - Allow chat windows to be vertical
   BOOL fWide = (iWidth >= iHeight);
   //BOOL fWide = m_fChatWindow ? TRUE : ((rClient.right - rClient.left) >= (rClient.bottom - rClient.top));

   if (iWidth >= iHeight)
      *piLeft = ((int)dwNumVerb + iHeight-1) / iHeight * CHATVERBSIZE;
   else
      *piBottom = ((int)dwNumVerb + iWidth-1) / iWidth * CHATVERBSIZE;
#endif // ENABLECHATVERB

}

/*************************************************************************************
CIconWindow::Paint - Handles painting of window

inputs
   HWND        hWnd - Window
   HDC         hDC - DC
   RECT        *prClip - Part that needs painting
*/
void CIconWindow::Paint (HWND hWnd, HDC hDC, RECT *prClip)
{
   RECT rClient, rClientFull;
   GetClientRect (hWnd, &rClient);
   rClientFull = rClient;
   ClientLoc (&rClient, FALSE);

   // clear background... use intelligent strips
   if (!m_lRECTBack.Num())
      m_lRECTBack.Add (&rClientFull);   // just in case
   DWORD i,j;
   RECT *pr = (RECT*) m_lRECTBack.Get(0);

#ifdef MIFTRANSPARENTWINDOWS // dont do for transparent windows
   BOOL fTransparent = !m_pMain->ChildHasTitle (m_hWnd);
#else
   BOOL fTransparent = FALSE;
#endif
   RECT rDest;
   for (i = 0; i < m_lRECTBack.Num(); i++, pr++)
      if (IntersectRect (&rDest, pr, prClip))
         m_pMain->BackgroundStretchViaBitmap (m_fTextUnderImages ? BACKGROUND_TEXT : BACKGROUND_IMAGES, fTransparent,
            m_fTextUnderImages ? WANTDARK_DARK : WANTDARK_NORMAL,
            pr, hWnd, GetParent(hWnd), hDC);

   // draw all the text
   CMem mem;
   PCIWGroup *ppg = (PCIWGroup*)m_lPCIWGroup.Get(0);
   HFONT hFontOld = (HFONT) SelectObject (hDC, m_pMain->m_hFont);
   HPEN hPen = CreatePen (PS_SOLID, 0, m_pMain->m_cTextDim);
   HPEN hPenOld = (HPEN) SelectObject (hDC, hPen);
   int iOldMode = SetBkMode (hDC, TRANSPARENT);
   HICON hIconMenu = LoadIcon (ghInstance, MAKEINTRESOURCE(IDI_MINIMENUICON));
   HICON hIconSpeaker = NULL;
   if (m_fTextUnderImages)
      hIconSpeaker = LoadIcon (ghInstance, MAKEINTRESOURCE(IDI_SPEAKERICON));

   BOOL fFoundChat = FALSE;
   RECT *prSpeakTo = NULL;
   DWORD k;
   for (i = 0; i < m_lPCIWGroup.Num(); i++) {
      PCIWGroup pg = ppg[i];

      // title
      if ((pg->m_rTitle.left != pg->m_rTitle.right) && (pg->m_rTitle.top != pg->m_rTitle.bottom)) {
         if (!mem.Required((wcslen((PWSTR)pg->m_memName.p)+1)*sizeof(WCHAR)))
            continue;
         WideCharToMultiByte (CP_ACP, 0, (PWSTR)pg->m_memName.p, -1, (char*)mem.p, (DWORD)mem.m_dwAllocated, 0, 0);
         SetTextColor (hDC, m_pMain->m_cTextDim);
         DrawText (hDC, (char*)mem.p, -1, &pg->m_rTitle, DT_SINGLELINE | DT_LEFT | DT_VCENTER);
      }

      // cound the number of images to acutally draw, so don't draw separating line if empty
      DWORD dwNumMod = 0;
      PCVisImage *ppv;
      // see how many to the right
      for (k = i+1; k < m_lPCIWGroup.Num(); k++) {
         ppv = (PCVisImage*)ppg[k]->m_lPCVisImage.Get(0);
         for (j = 0; j < ppg[k]->m_lPCVisImage.Num(); j++)
            if (ppv[j]->m_fVisibleAsSmall) {
               dwNumMod++;
               break;
            }
      } // k

      // separating line
      if (dwNumMod && ((pg->m_rGroup.left != pg->m_rGroup.right) || (pg->m_rGroup.top != pg->m_rGroup.bottom)) ) {
         MoveToEx (hDC, pg->m_rGroup.left, min(pg->m_rGroup.top, rClient.bottom), NULL);
         LineTo (hDC, pg->m_rGroup.right, min(pg->m_rGroup.bottom, rClient.bottom));
      }

      // loop through all sub-objects
      ppv = (PCVisImage*)pg->m_lPCVisImage.Get(0);
      RECT *prAll = (RECT*)pg->m_lVIRect.Get(0);
      for (j = 0; j < pg->m_lPCVisImage.Num(); j++, prAll++) {
         PCVisImage pv = ppv[j];

         if (IsEqualGUID (pv->m_gID, m_gSpeakWith)) {
            fFoundChat = TRUE;
            prSpeakTo = prAll;

            // if this is hidden because of main display then don't show
            if (!pv->m_fVisibleAsSmall) {
               prSpeakTo = NULL;
               continue;
            }

            // draw a round rectangle
            HPEN hPenRound = CreatePen (PS_SOLID, 3,
               (m_iSpeakMode == 1) ?
                  (m_pMain->m_fLightBackground ? RGB(0x40,0x40,0xff) : RGB(0, 0, 0xff)) :
                  (m_pMain->m_fLightBackground ? RGB(0x40, 0xff, 0x40) : RGB(0, 0xff, 0) ) );
            HPEN hPenOld = (HPEN)SelectObject (hDC, hPenRound);
            HBRUSH hBrushOld = (HBRUSH)SelectObject (hDC, GetStockObject(HOLLOW_BRUSH));
            RoundRect (hDC, prAll->left+2, prAll->top+2, prAll->right-2, prAll->bottom-2,
               m_iTextHeight, m_iTextHeight);
            SelectObject (hDC, hPenOld);
            SelectObject (hDC, hBrushOld);
            DeleteObject (hPenRound);
         }

         if (m_fTextUnderImages) {
            RECT r;
            pv->RectGet(&r);

            // draw the icon?
            // BUGFIX - DON'T drawn menu icon since clutters up screen
            //if (pv->MenuGet())
            //   DrawIcon (hDC,
            //      r.left + (MENUICONSIZE - MINIICONSIZE)/2,
            //      r.bottom + (MENUICONSIZE - MINIICONSIZE)/2,
            //      hIconMenu);

            // see if it's one of the speaking icons
            DWORD k;
            GUID *pgAudio = (GUID*)m_lAudioGUID.Get(0);
            for (k = 0; k < m_lAudioGUID.Num(); k++, pgAudio++)
               if (IsEqualGUID (*pgAudio, pv->m_gID)) {
                  DrawIcon (hDC, r.right - 32, r.bottom, hIconSpeaker);
                  break;
               }

            // title
            r.top = r.bottom;
            r.bottom += m_iTextHeight;
            if (!mem.Required((wcslen((PWSTR)pv->m_memName.p)+1)*sizeof(WCHAR)))
               continue;
            WideCharToMultiByte (CP_ACP, 0, (PWSTR)pv->m_memName.p, -1, (char*)mem.p, (DWORD)mem.m_dwAllocated, 0, 0);
            SetTextColor (hDC, m_pMain->m_cText);
            DrawText (hDC, (char*)mem.p, -1, &r, DT_SINGLELINE | DT_CENTER | DT_VCENTER);

            // other
            if (((WCHAR*)pv->m_memOther.p)[0]) {
               r.top = r.bottom;
               r.bottom += m_iTextHeight;
               if (!mem.Required((wcslen((PWSTR)pv->m_memOther.p)+1)*sizeof(WCHAR)))
                  continue;
               WideCharToMultiByte (CP_ACP, 0, (PWSTR)pv->m_memOther.p, -1, (char*)mem.p, (DWORD)mem.m_dwAllocated, 0, 0);
               SetTextColor (hDC, m_pMain->m_cTextDim);
               DrawText (hDC, (char*)mem.p, -1, &r, DT_SINGLELINE | DT_CENTER | DT_VCENTER);
            }

            // NOTE: NOT painting m_memDesciption
         } // if m_fTextUnderImages
      } // j
   } // i

   SelectObject (hDC, hFontOld);
   SelectObject (hDC, hPenOld);
   DeleteObject (hPen);
   SetBkMode (hDC, iOldMode);


   // draw items
   if (m_pMain)
      m_pMain->VisImagePaintAll (hWnd, hDC, prClip);

#ifdef ENABLECHATVERB
   // paint the menu icon in the lower-right corner
   if (m_fChatWindow) {
      int iLeft, iBottom;
      ChatWindowToolbarLoc (&iLeft, &iBottom);

      // clear the background
      RECT rClear;
      rClear = rClientFull;
      if (iBottom)
         rClear.top = rClear.bottom - iBottom;
      else
         rClear.right = rClear.left + iLeft;

      if (IntersectRect (&rDest, &rClear, prClip))
         m_pMain->BackgroundStretchViaBitmap (fTransparent, &rClear, hWnd, GetParent(hWnd), hDC);

      if (iLeft)
         DrawIcon (hDC,
            (rClientFull.left + iLeft) - ICONCORNER + (ICONCORNER - MENUICONSIZE)/2 + (MENUICONSIZE - MINIICONSIZE)/2,
            rClientFull.bottom - MENUICONSIZE,
            hIconMenu);
      else
         DrawIcon (hDC,
            rClientFull.right - ICONCORNER + (ICONCORNER - MENUICONSIZE)/2 + (MENUICONSIZE - MINIICONSIZE)/2,
            rClientFull.bottom - iBottom + (iBottom - MENUICONSIZE)/2 + (MENUICONSIZE - MINIICONSIZE)/2,
            hIconMenu);
   }
#endif // ENABLECHATVERB


   // if not found chat BUT expected one then set to default mode
   if (!fFoundChat && !IsEqualGUID(m_gSpeakWith, GUID_NULL)) {
      m_gSpeakWith = GUID_NULL;
      m_iSpeakMode = 0;
      prSpeakTo = NULL;
   }

   // draw an icon based on speaking
   if (prSpeakTo) {
      HICON hIcon = LoadIcon (ghInstance,
         MAKEINTRESOURCE((m_iSpeakMode == 1) ? IDI_ICONWHISPERTO : IDI_ICONTALKTO));
      DrawIcon (hDC, prSpeakTo->left, prSpeakTo->top, hIcon);
   }
   else if (m_iSpeakMode == -1) {
      HICON hIcon = LoadIcon (ghInstance, MAKEINTRESOURCE(IDI_ICONYELL));
      DrawIcon (hDC, rClientFull.left + m_iTextHeight, rClientFull.top + m_iTextHeight, hIcon);
   }

}


/*************************************************************************************
CIconWindow::FindLastIcon - Find the last icon displayed, assuming it's
an image of the room.

returns
   PCVisImage     - Icon image, or NULL if error
*/
PCVisImage CIconWindow::FindLastIcon (void)
{
   PCVisImage pRet = NULL;

   DWORD i;
   PCIWGroup *ppi = (PCIWGroup*) m_lPCIWGroup.Get(0);
   for (i = 0; i < m_lPCIWGroup.Num(); i++) {
      PCIWGroup pi = ppi[i];

      // how many elements?
      DWORD dwNum = pi->m_lPCVisImage.Num();
      PCVisImage *ppv = (PCVisImage*) pi->m_lPCVisImage.Get(0);

      if (dwNum && ppv[dwNum-1])
         pRet = ppv[dwNum-1];
   } // i

   return pRet;
}

/*************************************************************************************
CIconWindow::PositionIcons - This is called when there's the icon locations have
changed, or there's been a change to the size

inputs
   BOOL           fChangeScroll - If TRUE can change the scrollbar
   BOOL           fMove - Set to TRUE if because just moved the window. Need to special
                  case this so escarpment window is notified of move
*/
// BUGFIX - Make this larger, based on screen size
// #define MINICONSIZE     150         // icons should try to be this large
#define MINICONSIZE        (GetSystemMetrics(SM_CYSCREEN) / (m_fChatWindow ? 3 : 6))    // icons should try to be this large
BOOL CIconWindow::PositionIcons (BOOL fChangeScroll, BOOL fMove)
{
   // scrollbar size
   SCROLLINFO siHorz, siVert;
   int iHScroll = GetSystemMetrics (SM_CYHSCROLL);
   int iVScroll = GetSystemMetrics (SM_CXVSCROLL);
   RECT rClient, r;
   GetClientRect (m_hWnd, &rClient);
   BOOL fHScrollVis = (m_hWndHScroll ? TRUE : FALSE);
   BOOL fVScrollVis = (m_hWndVScroll ? TRUE : FALSE);

   m_fScrollAuto = !m_pMain->ChildHasTitle (m_hWnd);

   // get the scrollbar info
   siHorz.cbSize = siVert.cbSize = sizeof(SCROLLINFO);
   siHorz.fMask = siVert.fMask = SIF_ALL;
   if (m_hWndHScroll)
      GetScrollInfo (m_hWndHScroll, SB_CTL, &siHorz);
   else
      // BUGFIX - Used stored memset (&siHorz, 0, sizeof(siHorz));
      siHorz = m_siHorz;   // use stored value
   if (m_hWndVScroll)
      GetScrollInfo (m_hWndVScroll, SB_CTL, &siVert);
   else
      // BUGFIX - Use stored memset (&siVert, 0, sizeof(siVert));
      siVert = m_siVert;

   // figure out if it's wider or taller
   // BUGFIX - Allow chat windows to be vertical
   BOOL fWide = ((rClient.right - rClient.left) >= (rClient.bottom - rClient.top));
   //BOOL fWide = m_fChatWindow ? TRUE : ((rClient.right - rClient.left) >= (rClient.bottom - rClient.top));

   // reduce client by chat window
   ClientLoc (&rClient, TRUE);
   int iLeft, iBottom;
   ChatWindowToolbarLoc (&iLeft, &iBottom);

   // want the scrollbar visible?
   BOOL fWantHScrollVis = fChangeScroll ? fWide : fHScrollVis;
   BOOL fWantVScrollVis = fChangeScroll ? !fWide : fVScrollVis;

   // no scrollbars in title
   if (m_fScrollAuto)
      fWantHScrollVis = fWantVScrollVis = FALSE;

   if (fWide /*fWantHScrollVis*/ && !m_fScrollAuto)   // BUGFIX - Look at fWide so create scrollbars properly
      rClient.bottom -= iHScroll;   // since would be visible
   if (!fWide /*fWantVScrollVis*/ && !m_fScrollAuto)
      rClient.right -= iVScroll; // since would be visible
   r = rClient;

   m_lRECTBack.Clear();

   // determine the width and height of one box
   int iImageWidth, iImageHeight, iAbreast;
   iImageWidth = (fWide ? (r.bottom - r.top - 2*m_iTextHeight) : (r.right - r.left));
      // BUGFIX - Was -m_iTextHeight
   iImageWidth = max(iImageWidth, 1);
   iAbreast = iImageWidth / MINICONSIZE;
   iAbreast = max(iAbreast, 1);
   iImageWidth /= iAbreast;
   iImageWidth = iImageHeight = max(iImageWidth, 1);

   // determine the width and height, including spacing and text below
   int iWidth, iHeight;
   if (fWide) {
      iHeight = iImageHeight;
      iImageHeight -= m_iTextHeight; // 3 * m_iTextHeight;  // 2 text and buffer on TB

      if (m_fTextUnderImages)
         iImageHeight -= 2 * m_iTextHeight;

      iImageHeight = max(iImageHeight, 1);
      iImageWidth = iImageHeight * 3 / 2; // aspect ratio
      iWidth = iImageWidth + m_iTextHeight;
   }
   else {
      iWidth = iImageWidth;
      iImageWidth -= m_iTextHeight;   // buffer on LR
      iImageWidth = max(iImageWidth, 2);
      iImageHeight = iImageWidth * 2 / 3;
      iHeight = iImageHeight + m_iTextHeight; // + 3 * m_iTextHeight;  // 2 text and buffer on TB
      
      if (m_fTextUnderImages)
         iHeight += 2 * m_iTextHeight;
   }

   // starting point for the first group
   RECT rAll;
   memset (&rAll, 0, sizeof(rAll));

   // list of horizontal and vertical regions to leave empty
   CListFixed lEmptyVert, lEmptyHorz;
   RECT *prEmptyVert = NULL, *prEmptyHorz = NULL;
   lEmptyVert.Init (sizeof(RECT));
   lEmptyHorz.Init (sizeof(RECT));

   PCVisImage pviMain = m_pMain->FindMainVisImage ();

   // loop through all the groups
   DWORD i, j;
   PCIWGroup *ppi = (PCIWGroup*) m_lPCIWGroup.Get(0);
   for (i = 0; i < m_lPCIWGroup.Num(); i++) {
      PCIWGroup pi = ppi[i];
      memset (&pi->m_rGroup, 0, sizeof(pi->m_rGroup));
      memset (&pi->m_rTitle, 0, sizeof(pi->m_rTitle));

      // how many elements?
      DWORD dwNum = pi->m_lPCVisImage.Num();
      DWORD dwNumMod = dwNum;
      PCVisImage *ppv = (PCVisImage*) pi->m_lPCVisImage.Get(0);

      for (j = 0; j < dwNum; j++) {
         ppv[j]->m_fVisibleAsSmall = TRUE;

         // if this is in the main view then mark as invisible
         // BUGFIX - Only hide icons for chat window
         if (m_fChatWindow && pviMain && IsEqualGUID(pviMain->m_gID, ppv[j]->m_gID) && !IsEqualGUID(ppv[j]->m_gID, GUID_NULL)) {
            ppv[j]->m_fVisibleAsSmall = FALSE;
            dwNumMod--;
         }
      }

      // if there arent any elements then discard group
      if (!dwNumMod)
         continue;

      // number of rows and columns
      DWORD dwRows, dwColumns;
      if (fWide) {
         dwColumns = (dwNumMod + (DWORD)iAbreast - 1) / (DWORD)iAbreast;
         dwRows = (DWORD)iAbreast;
      }
      else {
         dwColumns = (DWORD)iAbreast;
         dwRows = (dwNumMod + (DWORD)iAbreast - 1) / (DWORD)iAbreast;
      }

      // rectangle for entire group
      RECT rGroup;
      rGroup.left = fWide ? rAll.right : rAll.left;
      rGroup.top = fWide ? rAll.top : rAll.bottom;
      rGroup.right = rGroup.left + (int)dwColumns * iWidth;
      rGroup.bottom = rGroup.top + m_iTextHeight + (int)dwRows * iHeight;
      pi->m_rTitle = rGroup;
      pi->m_rTitle.bottom = pi->m_rTitle.top + m_iTextHeight;
      if (i+1 < m_lPCIWGroup.Num()) {   // need border
         if (fWide) {
            rGroup.right += m_iTextHeight;
            pi->m_rGroup = rGroup;
            pi->m_rGroup.left = pi->m_rGroup.right = rGroup.right - m_iTextHeight/2;
            pi->m_rGroup.top -= 100;   // extra
            pi->m_rGroup.bottom += 100; // extra
         }
         else {   // vertical
            rGroup.bottom += m_iTextHeight;
            pi->m_rGroup = rGroup;
            pi->m_rGroup.top = pi->m_rGroup.bottom = rGroup.bottom - m_iTextHeight/2;
            pi->m_rGroup.left -= 100;   // extra
            pi->m_rGroup.right += 100; // extra
         }
      } // if need border line

      // figure out rectangles for each element
      pi->m_lVIRect.Clear();
      RECT r;
      DWORD dwMax = dwColumns * dwRows;
      for (j = 0; j < dwMax; j++) {
         r.left = rGroup.left + (int)(j % dwColumns) * iWidth + m_iTextHeight/2 /*buf*/;
         r.top = rGroup.top + (int)(j / dwColumns) * iHeight + m_iTextHeight + m_iTextHeight/2 /*buf*/;
         r.right = r.left + iImageWidth;
         r.bottom = r.top + iImageHeight;

         if (j < dwNumMod)
            pi->m_lVIRect.Add (&r);
         else
            m_lRECTBack.Add (&r);   // so know need to blank this out

         // note that this is to be left open vertically
         if (!prEmptyVert || (r.left > prEmptyVert->right)) {
            lEmptyVert.Add (&r);
            prEmptyVert = (RECT*)lEmptyVert.Get(lEmptyVert.Num()-1);
         }
         // same for horizontally
         if (!prEmptyHorz || (r.top > prEmptyHorz->bottom)) {
            lEmptyHorz.Add (&r);
            prEmptyHorz = (RECT*)lEmptyHorz.Get(lEmptyHorz.Num()-1);
         }
      } // j

      // store away the next one
      rAll.right = max(rAll.right, rGroup.right);
      rAll.bottom = max(rAll.bottom, rGroup.bottom);
   } // i

   // extend rAll a bit just to make sure
   RECT rCover;
   int iExtra = max(rClient.right - rClient.left, rClient.bottom - rClient.top);
   iExtra = max(iExtra, 0);
   iExtra = max(iExtra, 100) + 100;
   rCover = rAll;
   rCover.left -= iExtra;
   rCover.right += iExtra;
   rCover.top -= iExtra;
   rCover.bottom += iExtra;

   // add empty vertical lines in for blanking
   RECT rBlank;
   prEmptyVert = (RECT*)lEmptyVert.Get(0);
   m_lRECTBack.Required (m_lRECTBack.Num() + lEmptyVert.Num()+1);
   for (i = 0; i < lEmptyVert.Num()+1; i++, prEmptyVert++) {
      rBlank = rCover;
      if (i)
         rBlank.left = prEmptyVert[-1].right;
      if (i < lEmptyVert.Num())
         rBlank.right = prEmptyVert->left;
      m_lRECTBack.Add (&rBlank);
   } // i

   // and empty horizontal lines
   prEmptyHorz = (RECT*)lEmptyHorz.Get(0);
   m_lRECTBack.Required (m_lRECTBack.Num() + lEmptyHorz.Num()+1);
   for (i = 0; i < lEmptyHorz.Num()+1; i++, prEmptyHorz++) {
      rBlank = rCover;
      if (i)
         rBlank.top = prEmptyHorz[-1].bottom;
      if (i < lEmptyHorz.Num())
         rBlank.bottom = prEmptyHorz->top;
      m_lRECTBack.Add (&rBlank);
   } // i

   // set the scrollbars
   if (fChangeScroll) {
      // store away
      siHorz.fMask = SIF_ALL | SIF_DISABLENOSCROLL;
      siHorz.cbSize = sizeof(siHorz);
      siHorz.nPage = rClient.right - rClient.left;
      siHorz.nMax = (int)rAll.right;
      siHorz.nMin = 0;
      siHorz.nPos = min(siHorz.nPos, siHorz.nMax - (int)siHorz.nPage);
      siHorz.nPos = max(0, siHorz.nPos);

      // store away
      siVert.fMask = SIF_ALL | SIF_DISABLENOSCROLL;
      siVert.cbSize = sizeof(siVert);
      siVert.nPage = rClient.bottom - rClient.top;
      siVert.nMax = (int)rAll.bottom;
      siVert.nMin = 0;
      siVert.nPos = min(siVert.nPos, siVert.nMax - (int)siVert.nPage);
      siVert.nPos = max(0, siVert.nPos);


      // store these away
      m_siHorz = siHorz;
      m_siVert = siVert;

      if (fWantHScrollVis) {
         if (!m_hWndHScroll)
            m_hWndHScroll = CreateWindow ("SCROLLBAR", NULL,
               WS_VISIBLE | WS_CHILD | SBS_HORZ,
               rClient.left, rClient.bottom, rClient.right - rClient.left, iHScroll,
               m_hWnd, (HMENU)IDC_SCROLLBAR, ghInstance, NULL);

         SetScrollInfo (m_hWndHScroll, SB_CTL, &siHorz, TRUE);
         // ShowScrollBar (m_hWndHScroll, SB_CTL, TRUE);
      }
      else if (m_hWndHScroll) {
         DestroyWindow (m_hWndHScroll);
         m_hWndHScroll = NULL;
      }

      if (fWantVScrollVis) {
         if (!m_hWndVScroll)
            m_hWndVScroll = CreateWindow ("SCROLLBAR", NULL,
               WS_VISIBLE | WS_CHILD | SBS_VERT,
               rClient.right, 0, iVScroll, rClient.bottom - rClient.top,
               m_hWnd, (HMENU)IDC_SCROLLBAR, ghInstance, NULL);

         SetScrollInfo (m_hWndVScroll, SB_CTL, &siVert, TRUE);
         // ShowScrollBar (m_hWndVScroll, SB_CTL, TRUE);
      }
      else if (m_hWndVScroll) {
         DestroyWindow (m_hWndVScroll);
         m_hWndVScroll = NULL;
      }
   }

   // go through everything and reposition
   for (i = 0; i < m_lPCIWGroup.Num(); i++) {
      PCIWGroup pi = ppi[i];
      DWORD dwNum = pi->m_lPCVisImage.Num();
      DWORD dwNumMod = dwNum;
      PCVisImage *ppv = (PCVisImage*) pi->m_lPCVisImage.Get(0);
      for (j = 0; j < dwNum; j++)
         if (!ppv[j]->m_fVisibleAsSmall)
            dwNumMod--;

      // offset the group
      OffsetRect (&pi->m_rGroup, -(int)siHorz.nPos + iLeft, -(int)siVert.nPos);
      OffsetRect (&pi->m_rTitle, -(int)siHorz.nPos + iLeft, -(int)siVert.nPos);

      RECT *pr = (RECT*) pi->m_lVIRect.Get(0);
      DWORD dwJMod;
      for (j = dwJMod = 0; j < dwNum; j++, dwJMod++) {
         // if not visible then offset
         if (!ppv[j]->m_fVisibleAsSmall) {
            RECT rNew;
            rNew.left = rNew.top = 0;
            rNew.right = iImageWidth;
            rNew.bottom = iImageHeight;

            OffsetRect (&rNew, 10000, 10000);   // way off screen

            dwJMod--;   // to counterage increase
            ppv[j]->RectSet (&rNew, fMove);
            continue;
         }

         // else, just modify
         OffsetRect (&pr[dwJMod], -(int)siHorz.nPos + iLeft, -(int)siVert.nPos);
         ppv[j]->RectSet (&pr[dwJMod], fMove);

         // include some border around the image for the rect
         pr[dwJMod].left -= m_iTextHeight/2;
         pr[dwJMod].top -= m_iTextHeight/2;
         pr[dwJMod].right += m_iTextHeight/2;
         pr[dwJMod].bottom += max(28, m_iTextHeight * 2);
      } // j
   } // i

   // will need to reposition m_lRECTBack
   RECT *pr;
   pr = (RECT*)m_lRECTBack.Get(0);
   for (i = 0; i < m_lRECTBack.Num(); i++, pr++)
      OffsetRect (pr, -(int)siHorz.nPos + iLeft, -(int)siVert.nPos);

   // finally, redraw it all
   InvalidateRect (m_hWnd, NULL, FALSE);
   return TRUE;
}


/*************************************************************************************
CIconWindow::ObjectDelete - Deletes an object that appears in the display

inputs
   GUID           *pgID - ID to delete
returns
   none
*/
void CIconWindow::ObjectDelete (GUID *pgID)
{
   BOOL fChanged = FALSE;

   // loop
   DWORD i, j;

   PCIWGroup *ppg = (PCIWGroup*)m_lPCIWGroup.Get(0);
   for (i = 0; i < m_lPCIWGroup.Num(); i++) {
      PCIWGroup pg = ppg[i];
      for (j = pg->m_lPCVisImage.Num()-1; j < pg->m_lPCVisImage.Num(); j--) {
         PCVisImage pvi = *((PCVisImage*) pg->m_lPCVisImage.Get(j));

         if (IsEqualGUID(pvi->m_gID, *pgID)) {
            // NOTE: This lets the main window delete the vis image, since it
            // maintains the master list
            DWORD dwIndex = -1;
            if (pg->m_pIW && pg->m_pIW->m_pMain)
               dwIndex = pg->m_pIW->m_pMain->VisImageFindPtr (pvi);
            if (dwIndex != -1)
               pg->m_pIW->m_pMain->VisImageDelete (dwIndex);
            else
               delete pvi; // shouldnt happen
            pg->m_lPCVisImage.Remove (j);
            pg->m_lVIRect.Remove (j);

            fChanged = TRUE;
         }

      } // j
   } // i

   // finally repaint
   if (fChanged)
      PositionIcons (TRUE, FALSE);

}




/*************************************************************************************
CIconWindow::AudioStart - Notify an icon window that an object is speaking.
This causes the region of the screen where the speaker icon is
showing to be invalidated.

inputs
   GUID        *pgID - GUID of the speaker.
returns
   none
*/
void CIconWindow::AudioStart (GUID *pgID)
{
   // add this to the list
   m_lAudioGUID.Add (pgID);

   // see if can find this one
   DWORD i, j;
   PCIWGroup *ppg = (PCIWGroup*)m_lPCIWGroup.Get(0);
   for (i = 0; i < m_lPCIWGroup.Num(); i++) {
      PCIWGroup pg = ppg[i];
      for (j = 0; j < pg->m_lPCVisImage.Num(); j++) {
         PCVisImage pvi = *((PCVisImage*) pg->m_lPCVisImage.Get(j));

         if (!IsEqualGUID(pvi->m_gID, *pgID))
            continue;   // not this one
         
         pvi->m_fSpeaking = TRUE;

         // else found
         RECT *pr = (RECT*)pg->m_lVIRect.Get(j);
         InvalidateRect (m_hWnd, pr, FALSE);

         // potentially scroll, but only for chat window
         // BUGFIX - Only if visible as small and if cna talk to
         if (m_fChatWindow && pvi->m_fVisibleAsSmall && pvi->m_fCanChatTo) {
            RECT rClient, rImage;
            ClientLoc (&rClient, FALSE);

            pvi->RectGet (&rImage);
            rImage.bottom += MENUICONSIZE; // large enough for speaker
            if ((m_hWndHScroll || m_fScrollAuto) && ((rImage.left < rClient.left) || (rImage.right > rClient.right))) {
               SCROLLINFO si;
               memset (&si, 0, sizeof(si));
               si.cbSize = sizeof(si);
               si.fMask = SIF_ALL;
               if (m_hWndHScroll)
                  GetScrollInfo (m_hWndHScroll, SB_CTL, &si);
               else
                  si = m_siHorz;

               // don't go beyond min and max
               if (rImage.left < rClient.left)
                  si.nPos -= (rClient.left- rImage.left);
               else if (rImage.right > rClient.right)
                  si.nPos += (rImage.right - rClient.right);

               si.fMask = SIF_ALL;
               if (m_hWndHScroll)
                  SetScrollInfo (m_hWndHScroll, SB_CTL, &si, TRUE);
               m_siHorz = si;

               // update everything's location
               PositionIcons (FALSE, FALSE);
            };
            if ((m_hWndVScroll || m_fScrollAuto) && ((rImage.top < rClient.top) || (rImage.bottom - m_iTextHeight > rClient.bottom))) {
               SCROLLINFO si;
               memset (&si, 0, sizeof(si));
               si.cbSize = sizeof(si);
               si.fMask = SIF_ALL;
               if (m_hWndVScroll)
                  GetScrollInfo (m_hWndVScroll, SB_CTL, &si);
               else
                  si = m_siVert;

               // don't go beyond min and max
               if (rImage.top < rClient.top)
                  si.nPos -= (rClient.top- rImage.top);
               else if (rImage.bottom - m_iTextHeight > rClient.bottom)
                  si.nPos += (rImage.bottom - m_iTextHeight - rClient.bottom);

               si.fMask = SIF_ALL;
               if (m_hWndVScroll)
                  SetScrollInfo (m_hWndVScroll, SB_CTL, &si, TRUE);
               m_siVert = si;

               // update everything's location
               PositionIcons (FALSE, FALSE);
            };
         } // if wasset
         return;  // done
      } // j
   } // i

}



/*************************************************************************************
CIconWindow::AudioStop - Notify an icon window that an object has stopped speaking.
This causes the region of the screen where the speaker icon is
showing to be invalidated.

inputs
   GUID        *pgID - GUID of the speaker.
returns
   none
*/
void CIconWindow::AudioStop (GUID *pgID)
{
   // remove from the list, but only one
   DWORD k;
   GUID *pgAudio = (GUID*)m_lAudioGUID.Get(0);
   for (k = 0; k < m_lAudioGUID.Num(); k++, pgAudio++)
      if (IsEqualGUID (*pgAudio, *pgID)) {
         m_lAudioGUID.Remove (k);
         break;
      }

   // see if can find this one
   DWORD i, j;
   PCIWGroup *ppg = (PCIWGroup*)m_lPCIWGroup.Get(0);
   for (i = 0; i < m_lPCIWGroup.Num(); i++) {
      PCIWGroup pg = ppg[i];
      for (j = 0; j < pg->m_lPCVisImage.Num(); j++) {
         PCVisImage pvi = *((PCVisImage*) pg->m_lPCVisImage.Get(j));

         if (!IsEqualGUID(pvi->m_gID, *pgID))
            continue;

         pvi->m_fSpeaking = FALSE;

         // else, found
         RECT *pr = (RECT*)pg->m_lVIRect.Get(j);
         InvalidateRect (m_hWnd, pr, FALSE);
         return;  // done
      } // j
   } // i

}



/*************************************************************************************
CIconWindow::VerbChatNew - Called when there are new chat verbs to display.
*/
void CIconWindow::VerbChatNew (void)
{
#ifdef ENABLECHATVERB
   if (!m_fChatWindow)
      return;

   // delete the existing icon buttons
   DWORD i;
   PCIconButton *ppib = (PCIconButton*) m_lPCIconButton.Get(0);
   for (i = 0; i < m_lPCIconButton.Num(); i++)
      delete ppib[i];
   m_lPCIconButton.Clear();

   // refresh
   VerbButtonsArrange();
#endif
}
/************************************************************************************
EditSubclassWndProc - to subclass edit control so sends enter up.
*/
static LRESULT CALLBACK EditSubclassWndProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
   switch (uMsg) {
   case WM_CHAR:
      if (wParam == L'\r') {
         SendMessage (GetParent(hWnd), uMsg, wParam, lParam);
         return 0;
      }
      break;

   case WM_MOUSEWHEEL:
      // pass up
      return SendMessage (GetParent(hWnd), uMsg, wParam, lParam);

   }

   // else
   if (gpEditWndProc)
      return CallWindowProc (gpEditWndProc, hWnd, uMsg, wParam, lParam);
   else
      return DefWindowProc (hWnd, uMsg, wParam, lParam);
}



/*************************************************************************************
CIconWindow::VerbButtonsArrange - Arrange and show verb buttons.
*/
void CIconWindow::VerbButtonsArrange (void)
{
#ifdef ENABLECHATVERB
   if (!m_fChatWindow)
      return;

   // either move or add it
   RECT r, rButtonArea;
#ifndef UNIFIEDTRANSCRIPT
   GetClientRect (m_hWnd, &r);
   r.top = r.bottom - CHATEDITSIZE;
   r.right -= CHATEDITSIZE;
   r.right = max(r.right, r.left+1);

   if (m_hWndEdit) {
      MoveWindow (m_hWndEdit, r.left, r.top, r.right - r.left, r.bottom - r.top, TRUE);
   }
   else {
      // create window for edit
      m_hWndEdit = CreateWindowEx (WS_EX_NOPARENTNOTIFY | WS_EX_CLIENTEDGE, "Edit", "",
         WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | ES_MULTILINE | ES_AUTOVSCROLL | ES_NOHIDESEL,
         r.left, r.top, r.right-r.left, r.bottom-r.top,
         m_hWnd, (HMENU)IDC_EDITCOMMAND, ghInstance, NULL);
      SendMessage (m_hWndEdit, EM_LIMITTEXT, 256, 0);

      // NOTE: Because edit window is a child of another child window, selection
      // doesnt work properly. This is a bug/feature in windows
      gpEditWndProc = (WNDPROC)GetWindowLongPtr (m_hWndEdit, GWLP_WNDPROC);
      SetWindowLongPtr (m_hWndEdit, GWLP_WNDPROC, (LONG) EditSubclassWndProc);
      if (IsWindowVisible (m_hWnd))
         SetFocus (m_hWndEdit);

      // set some instructional text
      SetWindowText (m_hWndEdit, "To speak, type here, and press enter.");
      SendMessage (m_hWndEdit, EM_SETSEL, 0, -1);
   }
#endif // !UNIFIEDTRANSCRIPT

   if (!m_pMain->m_pResVerbChat)
      return;  // nothing to do

   // rows and columns
   POINT pOffset;
   GetClientRect (m_hWnd, &r);
   int iLeft, iBottom;
   ChatWindowToolbarLoc (&iLeft, &iBottom);
   if (iLeft)
      r.right = r.left + iLeft;
   else
      r.top = r.bottom - iBottom;

   // r.bottom = r.top + CHATVERBSIZE;
#ifdef UNIFIEDTRANSCRIPT
   if (iBottom)
      r.right -= ICONCORNER;
#endif
   rButtonArea = r;
   pOffset.x = r.left;
   pOffset.y = r.top;

   DWORD dwRows = (DWORD) max((r.bottom - r.top) / CHATVERBSIZE, 1);
   DWORD dwColumns = (DWORD)max((r.right - r.left) / CHATVERBSIZE, 1);

   #ifdef MIFTRANSPARENTWINDOWS // dont do for transparent windows
      BOOL fTransparent = !m_pMain->ChildHasTitle (m_hWnd);
   #else
      BOOL fTransparent = FALSE;
   #endif

   // loop through all the icon buttons
   DWORD i;
   PCResVerbIcon *ppr = (PCResVerbIcon*)m_pMain->m_pResVerbChat->m_lPCResVerbIcon.Get(0);
   PCIconButton *ppib = (PCIconButton*)m_lPCIconButton.Get(0);
   CMem memName, memSan;
   PCIconButton pb;
   for (i = 0; i < m_pMain->m_pResVerbChat->m_lPCResVerbIcon.Num(); i++) {
      PCResVerbIcon pr = ppr[i];
      if (i >= m_lPCIconButton.Num()) {
         // add a button
         pb = NULL;
         m_lPCIconButton.Add (&pb);
         ppib = (PCIconButton*)m_lPCIconButton.Get(0);
      }
      pb = ppib[i];

      // row and columns
      DWORD dwRow = i / dwColumns;
      if (dwRow >= dwRows)
         dwRow = dwRows - 1;  // so dont go too far
      DWORD dwColumn = i - dwRow * dwColumns;

      // figure out rect where it goes
      r.left = pOffset.x + (int)dwColumn * (int)CHATVERBSIZE;
      r.right = r.left + (int)CHATVERBSIZE;
      r.top = pOffset.y + (int)dwRow * (int)CHATVERBSIZE;
      r.bottom = r.top + (int)CHATVERBSIZE;

      // move or create it
      if (pb) {
         pb->Move (&r);
         continue;
      }
      
      // convert to ANSI
      PWSTR psz = (PWSTR)pr->m_memShow.p;
      if (!psz || !psz[0])
         psz = (PWSTR)pr->m_memDo.p;
      MemZero (&memSan);   // sanitize
      MemCatSanitize (&memSan, psz);
      psz = (PWSTR)memSan.p;
      if (!memName.Required ((wcslen(psz)+1)*sizeof(WCHAR)))
         continue;
      WideCharToMultiByte (CP_ACP, 0, psz, -1, (char*)memName.p, (DWORD)memName.m_dwAllocated, 0,0);

      // else, need to create
      pb = ppib[i] = new CIconButton;
      if (!pb)
         continue;

      // set the color and info
      if (pr->m_fHasClick)
         pb->ColorSet (m_pMain->m_cVerbDim, m_pMain->m_cTextDim, m_pMain->m_cTextDim, m_pMain->m_cText);
      else {

         pb->ColorSet (fTransparent ? m_pMain->m_crJPEGBackDarkAll : m_pMain->m_cBackground, m_pMain->m_cVerbDim, m_pMain->m_cTextDim, m_pMain->m_cText);
      }

      if (!pb->Init (pr->IconResourceID(), pr->IconResourceInstance(), NULL,
         (char*)memName.p, "", 1, &r, m_hWnd, 1000 + i)) {
            delete pb;
            ppib[i] = NULL;
            continue;
         }
   } // i
#endif // ENABLECHATVERB
}



/*************************************************************************************
CIconWindow::VerbDeselect - Deselects any selected verbs. This also calls into
the icon windows.
*/
void CIconWindow::VerbDeselect (DWORD dwSelected)
{
   // if it's this one then deselect
   PCIconButton *ppib = (PCIconButton*)m_lPCIconButton.Get(dwSelected);
   if (ppib && ppib[0])
      ppib[0]->FlagsSet (ppib[0]->FlagsGet() & ~(IBFLAG_REDARROW));
}


/*************************************************************************************
CIconWindow::ChatToSet - Sets the current object being chatted to.

inputs
   int         iSpeakMode - 1 for whisper, 0 for normal speech, -1 for ywll
   GUID        *pgID - ID to chat to, or NULL if none
*/
void CIconWindow::ChatToSet (int iSpeakMode, GUID *pgID)
{
   // invalidate the old one
   DWORD i, j;
   PCIWGroup *ppg = (PCIWGroup*)m_lPCIWGroup.Get(0);
   BOOL fFoundOld = FALSE;
   for (i = 0; i < m_lPCIWGroup.Num(); i++) {
      PCIWGroup pg = ppg[i];
      RECT *pr = (RECT*) pg->m_lVIRect.Get(0);

      for (j = 0; j < pg->m_lPCVisImage.Num(); j++, pr++) {
         PCVisImage pvi = *((PCVisImage*) pg->m_lPCVisImage.Get(j));

         if (IsEqualGUID(pvi->m_gID, m_gSpeakWith)) {
            InvalidateRect (m_hWnd, pr, FALSE);
            fFoundOld = TRUE;
         }
         if (pgID && IsEqualGUID(pvi->m_gID, *pgID))
            InvalidateRect (m_hWnd, pr, FALSE);
      } // j
   } // i

   // if it was yelling then invalidate entire rect
   if (m_iSpeakMode == -1)
      InvalidateRect (m_hWnd, NULL, FALSE);

   // if didnt find the old one then it's null
   if (!fFoundOld && !IsEqualGUID(m_gSpeakWith, GUID_NULL)) {
      m_gSpeakWith = GUID_NULL;
      m_iSpeakMode = 0;
   }

   // if nothing to chat to then clear
   if (!pgID) {
      m_iSpeakMode = iSpeakMode;
      m_gSpeakWith = GUID_NULL;
      
      // if speak mode is yell then invalidate all
      if (m_iSpeakMode == -1)
         InvalidateRect (m_hWnd, NULL, FALSE);
      return;
   }

   // if it's the same as the last then change the flag
   m_iSpeakMode = iSpeakMode;
   m_gSpeakWith = *pgID;
}



/*************************************************************************************
CIconWindow::ChatWhoTalkingTo - Returns information about who talking to.

inputs
   GUID        *pgID - Filled with the GUID of who talking to, or GUID_NULL
               if no one in specific.
   PWSTR       *ppszLang - Filled with the language using, or NULL if dont know
   PWSTR       *ppszStyle - Filled with the speaking style string: "speak", "yell", or "whisper"
returnw
   BOOL - TRUE if this is a chat window and info filled in, FALSE if not
*/
BOOL CIconWindow::ChatWhoTalkingTo (GUID *pgID, PWSTR *ppszLang, PWSTR *ppszStyle)
{
   if (!m_fChatWindow)
      return FALSE;

   // else, get info

   // find the object
   PCVisImage pvTo = NULL;
   DWORD i, j;
   PCIWGroup *ppg = (PCIWGroup*)m_lPCIWGroup.Get(0);
   BOOL fFoundOld = FALSE;
   for (i = 0; i < m_lPCIWGroup.Num(); i++) {
      PCIWGroup pg = ppg[i];
      for (j = 0; j < pg->m_lPCVisImage.Num(); j++) {
         PCVisImage pvi = *((PCVisImage*) pg->m_lPCVisImage.Get(j));
         if (IsEqualGUID(pvi->m_gID, m_gSpeakWith)) {
            pvTo = pvi;
            break;
         }
      } // j
   } // i
   if (!pvTo && (m_iSpeakMode > 0))
      m_iSpeakMode = 0; // cant really whisper to nothing
   if (pvTo && (m_iSpeakMode <= 0))
      *pgID = m_gSpeakWith;
   else
      *pgID = GUID_NULL;


   switch (m_iSpeakMode) {
   case 1:
      *ppszStyle = L"whisper";
      break;
   case -1:
      *ppszStyle = L"yell";
      break;
   case 0:
   default:
      *ppszStyle = L"speak";
      break;
   } // switch

   // language
   if (m_dwLanguage == (DWORD)-1)
      *ppszLang = NULL;
   else {
      if (m_dwLanguage >= m_lLanguage.Num())
         m_dwLanguage = 0;
      *ppszLang = (PWSTR)m_lLanguage.Get(m_dwLanguage);
   }

   return TRUE;
}


/*************************************************************************************
CIconWindow::TooltipUpdate - Updates the tooltip text.

inputs
   POINT       pt - Point in client coords
returns
   BOOL - TRUE if udpated
*/
BOOL CIconWindow::TooltipUpdate (POINT pt)
{
   CMem memWant;
   RECT rWant;
   PWSTR psz;
   MemZero (&memWant);
   BOOL fOverMenu;
   PCVisImage pView = MouseOver (pt.x, pt.y, &fOverMenu);
   CListFixed lOBJECTSLIDERS;
   DWORD i;
   if (pView && !fOverMenu) { // BUGFIX - cant be over the menu
      psz = (PWSTR)pView->m_memName.p;
      MemCat (&memWant, L"<bold>");
      if (psz && psz[0])
         MemCat (&memWant, psz);
      MemCat (&memWant, L"</bold>");

      // other
      psz = (PWSTR)pView->m_memOther.p;
      if (psz && psz[0]) {
         MemCat (&memWant, L"<p/><italic>");
         MemCat (&memWant, psz);
         MemCat (&memWant, L"</italic>");
      }

      // BUGFIX - Don't display this because causes tooltip to (a) be large, and (b) obscure the image
      // description
      // psz = (PWSTR)pView->m_memDescription.p;
      // if (psz && psz[0]) {
      //    MemCat (&memWant, L"<p/>");
      //    MemCat (&memWant, psz);
      // }

      // also include sliders
      pView->SlidersGetInterp (&lOBJECTSLIDERS);
      if (lOBJECTSLIDERS.Num()) {
         // see if have left/right
         BOOL fLeft = FALSE, fRight = FALSE;
         POBJECTSLIDERS pos = (POBJECTSLIDERS)lOBJECTSLIDERS.Get(0);
         for (i = 0; i < lOBJECTSLIDERS.Num(); i++, pos++) {
            if (pos->szLeft[0])
               fLeft = TRUE;
            if (pos->szRight[0])
               fRight = TRUE;
         } // i

         MemCat (&memWant, L"<p/><small><table width=100% border=0 innerlines=0 lrmargin=2 tbmargin=2 valign=center>");
         pos = (POBJECTSLIDERS)lOBJECTSLIDERS.Get(0);
         for (i = 0; i < lOBJECTSLIDERS.Num(); i++, pos++) {
            MemCat (&memWant, L"<tr>");
            WCHAR szColor[16], szFontTag[32];
            ColorToAttrib (szColor, pos->cColorTip);
            swprintf (szFontTag, L"<font color=%s>", szColor);
            if (fLeft) {
               MemCat (&memWant, L"<td align=right>");
               MemCat (&memWant, szFontTag);
               MemCatSanitize (&memWant, pos->szLeft);
               MemCat (&memWant, L"</font></td>");
            }
            MemCat (&memWant, L"<td><status width=100% vaign=center align=left height=12 border=1 lrmargin=2 tbmargin=0 bordercolor=");
            MemCat (&memWant, szColor);
            MemCat (&memWant, L"><colorblend height=8 width=");
            MemCat (&memWant, max(1,(int)((pos->fValue + 1.0) * 50.0 + 0.5)) );   // so -1 to 1 goes to 0 to 100  
               // BUGFIX - So always at least one pixel
            MemCat (&memWant, L"% color=");
            MemCat (&memWant, szColor);
            MemCat (&memWant, L"/></status></td>");
            if (fLeft) {
               MemCat (&memWant, L"<td align=left>");
               MemCat (&memWant, szFontTag);
               MemCatSanitize (&memWant, pos->szRight);
               MemCat (&memWant, L"</font></td>");
            }
            MemCat (&memWant, L"</tr>");
         } // i
         MemCat (&memWant, L"</table></small>");
      }

      pView->RectGet (&rWant);
   }
   else
      memset (&rWant, 0, sizeof(rWant));

   // delete tooltip is changed
   psz = (PWSTR)memWant.p;

   // if want a blank tooltip then done
   if (!psz[0]) {
      if (m_pToolTip) {
         delete m_pToolTip;
         m_pToolTip = NULL;
         KillTimer (m_hWnd, CHECKTOOLIPTIMER);
      }
      return TRUE;
   }

   BOOL fWantNew = (memcmp(&rWant, &m_rToolTipCur, sizeof(rWant)) || wcscmp(psz, (PWSTR)m_memToolTipCur.p) || !m_pToolTip);
   if (!fWantNew)
      return FALSE;

   // else, delete
   if (m_pToolTip) {
      delete m_pToolTip;
      m_pToolTip = NULL;
      KillTimer (m_hWnd, CHECKTOOLIPTIMER);
   }

   // else, create
   MemZero (&m_memToolTipCur);
   MemCat (&m_memToolTipCur, (PWSTR)memWant.p);
   m_rToolTipCur = rWant;

   WideCharToMultiByte (CP_ACP, 0, (PWSTR)m_memToolTipCur.p, -1, (char*)memWant.p, (DWORD)memWant.m_dwAllocated, 0,0);
      // Know that there's enough memory in memWant

   // create rect to screen
   ClientToScreen (m_hWnd, (POINT*)&rWant + 0);
   ClientToScreen (m_hWnd, (POINT*)&rWant + 1);
   m_pToolTip = new CToolTip;
   if (!m_pToolTip)
      return FALSE;
   if (!m_pToolTip->Init ((char*)memWant.p, 0, &rWant, m_hWnd)) {
      delete m_pToolTip;
      m_pToolTip = NULL;
      return FALSE;
   }

   // create the timer to close the tooltip
   SetTimer (m_hWnd, CHECKTOOLIPTIMER, 500, NULL);

   // else, done
   return TRUE;
}


/*************************************************************************************
CIconWindow::ContainsPCVisImage - Returns TRUE if the icon window contains PCVisImage

inputs
   PCVisImage     pView - To test
returns
   BOOL - TRUE if it contains it
*/
BOOL CIconWindow::ContainsPCVisImage (PCVisImage pView)
{
   DWORD i, j;
   PCIWGroup *ppi = (PCIWGroup*) m_lPCIWGroup.Get(0);
   for (i = 0; i < m_lPCIWGroup.Num(); i++) {
      PCIWGroup pg = ppi[i];
      PCVisImage *ppv = (PCVisImage*) pg->m_lPCVisImage.Get(0);
      for (j = 0; j < pg->m_lPCVisImage.Num(); j++)
         if (ppv[j] == pView)
            return TRUE;
   } // i

   return FALSE;
}

/*************************************************************************************
CIconWindow::CanChatTo - Returns TRUE if the icon window contains images that can chat to

inputs
   none
returns
   BOOL - TRUE if contains objects that can chat to
*/
BOOL CIconWindow::CanChatTo (void)
{
   return m_fCanChatTo;

#if 0 // cant do because not enough contrl from app
   DWORD i, j;
   PCIWGroup *ppi = (PCIWGroup*) m_lPCIWGroup.Get(0);
   for (i = 0; i < m_lPCIWGroup.Num(); i++) {
      PCIWGroup pg = ppi[i];
      PCVisImage *ppv = (PCVisImage*) pg->m_lPCVisImage.Get(0);
      for (j = 0; j < pg->m_lPCVisImage.Num(); j++)
         if (ppv[j]->m_fCanChatTo)
            return TRUE;
   } // i
#endif // 0

   return FALSE;
}


/*************************************************************************************
CIconWindow::CursorToScroll - Cursor location in the client rect to the
amount to scroll left/right.

inputs
   POINT          pCursor - Cursor location in client rect
   fp             *pfRight - Filled with amount to scroll right (or neg=left), from 0..1. 0 = none
   fp             *pfDown - Filled with amount to scroll down (or neg=up), from 0..1. 0 = none
returns
   none
*/
#define MAPBORDERSIZE      6       // 1/10th
#define MAPBORDERSIZEINV   (1.0 / (fp)MAPBORDERSIZE)
void CIconWindow::CursorToScroll (POINT pCursor, fp *pfRight, fp *pfDown)
{
   // if not autoscroll then skip
   if (!m_fScrollAuto) {
      *pfRight = *pfDown = 0;
      return;
   }

   RECT r;
   GetClientRect (m_hWnd, &r);

   // move the scrollbars
#undef GetSystemMetrics
   int iHScroll = GetSystemMetrics (SM_CYHSCROLL);
   int iVScroll = GetSystemMetrics (SM_CXVSCROLL);
   if (m_hWndHScroll)
      r.bottom -= iHScroll;   // since would be visible
   if (m_hWndVScroll)
      r.right -= iVScroll;

   // reduce client by chat window
   ClientLoc (&r, TRUE);


   // if not in rect then done
   if (!PtInRect (&r, pCursor)) {
      *pfRight = 0;
      *pfDown = 0;
      return;
   }

   // else, how much
   int iX = max(r.right - r.left, 1);
   int iY = max(r.bottom - r.top, 1);

   fp fRight = (fp)(pCursor.x - r.left) / (fp)iX;
   fp fDown = (fp)(pCursor.y - r.top) / (fp)iY;
   if (fRight < MAPBORDERSIZEINV)
      fRight = fRight * MAPBORDERSIZE - 1.0;
   else if (fRight > 1.0 - MAPBORDERSIZEINV)
      fRight = (fRight - (1.0 - MAPBORDERSIZEINV)) * MAPBORDERSIZE;
   else
      fRight = 0;

   if (fDown < MAPBORDERSIZEINV)
      fDown = fDown * MAPBORDERSIZE - 1.0;
   else if (fDown > 1.0 - MAPBORDERSIZEINV)
      fDown = (fDown - (1.0 - MAPBORDERSIZEINV)) * MAPBORDERSIZE;
   else
      fDown = 0;

   // limits
   if ((fRight < 0) && (m_siHorz.nPos <= m_siHorz.nMin))
      fRight = 0;
   if ((fRight > 0) && ((int)m_siHorz.nPos + (int)m_siHorz.nPage >= (int)m_siHorz.nMax))
      fRight = 0;
   if ((fDown > 0) && ((int)m_siVert.nPos + (int)m_siVert.nPage >= (int)m_siVert.nMax))
      fDown = 0;
   if ((fDown < 0) && (m_siVert.nPos <= m_siVert.nMin))
      fDown = 0;

   *pfRight = fRight;
   *pfDown = fDown;
}

