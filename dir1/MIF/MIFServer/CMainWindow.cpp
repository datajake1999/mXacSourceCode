/*************************************************************************************
CMainWindow.cpp - Code for displaying the main server window

begun 29/2/04 by Mike Rozak.
Copyright 2004 by Mike Rozak. All rights reserved
*/

// #define USEIPV6      // BUGFIX - Need this to include winsock2

#ifdef USEIPV6
#include <winsock2.h>
note - IPV6 not fully debugged yet
#endif

#include <windows.h>
#include <objbase.h>
#include <commctrl.h>
#include "escarpment.h"
#include "..\..\m3d\M3D.h"
#include "..\..\m3d\mifl.h"
#include "..\mif.h"
#include "..\buildnum.h"
#include "mifserver.h"
#include "resource.h"

#ifdef USEIPV6
#include <Ws2tcpip.h>
#endif


#define TIMER_ALREADYINFUNC         343      // timer ID so can callback when not in function
#define TIMER_CONNECTLIST           344      // time to update connection list

#define IDC_CONNECTLIST             1054     // connection list

static PWSTR FindIgnoreDir (WCHAR *pszFind, PCBTree pt);

// globals
PCMainWindow gpMainWindow = NULL;
PSTR gpszCircumRealityServerMainWindow = "CircumrealityServerMainWindow";

/************************************************************************************
BinaryToStringPlusOne - Conerts binary to a unicode string, where every character
in the string is a +1 version of the binary byte.

inputs
   PBYTE          pbData - Data
   DWORD          dwSize - Size in bytes
   PCMem          pMem - Filled in
returns
   BOOL - TRUE if success
*/
BOOL BinaryToStringPlusOne (PBYTE pbData, DWORD dwSize, PCMem pMem)
{
   // make a string out of this
   if (!pMem->Required ((dwSize+1)*sizeof(WCHAR)))
      return FALSE;

   PWSTR psz = (PWSTR)pMem->p;
   DWORD i;
   for (i = 0; i < dwSize; i++, psz++, pbData++)
      *psz = (WCHAR)*pbData + 1;
   *psz = 0;   // NULL terminate

   return TRUE;
}


/************************************************************************************
StringPlusOneToBinary - Converts a unicode string where every character
in the string is a +1 version of the binary byte, to binary

inputs
   PWSTR          psz - String
   PCMem          pMem - Filled in. m_dwCurPosn filled in with the size
returns
   BOOL - TRUE if success
*/
BOOL StringPlusOneToBinary (PWSTR psz, PCMem pMem)
{
   DWORD dwLen = (DWORD)wcslen(psz);
   pMem->m_dwCurPosn = dwLen;

   // make a string out of this
   if (!pMem->Required (dwLen))
      return FALSE;

   PBYTE pbData = (PBYTE)pMem->p;
   DWORD i;
   for (i = 0; i < dwLen; i++, psz++, pbData++)
      *pbData = (BYTE)(psz[0] - 1);

   return TRUE;
}

/************************************************************************************
MainWindowWndProc - internal windows callback for socket simulator
*/
LRESULT CALLBACK MainWindowWndProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
#ifdef _WIN64
   PCMainWindow p = (PCMainWindow) GetWindowLongPtr (hWnd, GWLP_USERDATA);
#else
   PCMainWindow p = (PCMainWindow) (LONG_PTR) GetWindowLongPtr (hWnd, GWLP_USERDATA);
#endif

   switch (uMsg) {
   case WM_CREATE: 
      {
         // store away the user data
         LPCREATESTRUCT lpcs = (LPCREATESTRUCT) lParam;
         SetWindowLongPtr (hWnd, GWLP_USERDATA, (LONG_PTR) lpcs->lpCreateParams);
         p = (PCMainWindow) lpcs->lpCreateParams;
      }
      break;
   };

   if (p)
      return p->WndProc (hWnd, uMsg, wParam, lParam);
   else
      return DefWindowProc (hWnd, uMsg, wParam, lParam);
}



/*************************************************************************************
CMainWindow::Constructor and destructor
*/
CMainWindow::CMainWindow (void)
{
   // remember main window
   gpMainWindow = this;
   m_fPostQuitMessage = FALSE;   // BUGFIX - Default to not quitting when shut down

   m_hWnd = NULL;
   m_pIT = NULL;
   m_pIDT = NULL;
   m_pEmailThread = NULL;
   m_pTextLog = NULL;
   m_pVM = NULL;
   m_fTimerInFunc = FALSE;
   m_hWndList = NULL;
   m_szDatabaseDir[0] = 0;
   m_pDatabase = NULL;
   m_pInstance = NULL;
   m_pmfBinaryData = NULL;
   m_dwDatabaseCount = 0;

   m_szConnectFile[0] = 0;
   m_fConnectRemote = FALSE;
   m_dwConnectPort = 0;

   m_fShutDownImmediately = FALSE;

   // CPU monitoring
   LARGE_INTEGER iCur;
   QueryPerformanceFrequency (&iCur);
   m_iPerCountFreq = *((__int64*)&iCur);
   m_iPerCount60Sec = m_iPerCountFreq * 60;  // 60 seconds of records
   m_iPerCountLast = 0;
   m_iCPUThreadTimeStart = 0;
   m_gCPUObject = GUID_NULL;
   m_fCPUObjectNULL = TRUE;
   m_lCPUMONITOR.Init (sizeof(CPUMONITOR));

   m_lPCOnlineHelp.Init (sizeof(PCOnlineHelp));

   MemZero (&m_memShardParam);
}

CMainWindow::~CMainWindow (void)
{
   // stop the thread first
   if (m_pIDT)
      m_pIDT->ThreadStop();

   if (m_pIT)
      delete m_pIT;
   m_pIT = NULL;

   // delete database thread
   if (m_pIDT)
      delete m_pIDT;
   m_pIDT = NULL;

   if (m_pEmailThread)
      delete m_pEmailThread;
   m_pEmailThread = NULL;

   if (m_pTextLog)
      delete m_pTextLog;
   m_pTextLog = NULL;

   if (m_hWnd)
      DestroyWindow (m_hWnd);
   m_hWnd = NULL;

   // note that new main window
   gpMainWindow = NULL;

   // free up the cached info
   DWORD i;
   for (i = 0; i < m_tMFCACHE.Num(); i++) {
      PMFCACHE pmc = (PMFCACHE)m_tMFCACHE.GetNum(i);
      MegaFileFree (pmc->pbData);
   } // i

   // free up database
   if (m_pDatabase)
      delete m_pDatabase;
   if (m_pInstance)
      delete m_pInstance;
   if (m_pmfBinaryData)
      delete m_pmfBinaryData;

   // free up help
   PCOnlineHelp *pph = (PCOnlineHelp*)m_lPCOnlineHelp.Get(0);
   for (i = 0; i < m_lPCOnlineHelp.Num(); i++)
      delete pph[i];
   m_lPCOnlineHelp.Clear();
}



/****************************************************************************
PCResTitleInfoSetFromFile - Reads in the info from the file.

inputs
   PCMegaFile     pmf - Mega file
returns
   PCResTitleInfoSet - Info that must be freed by caller, or NULL if error
*/
PCResTitleInfoSet PCResTitleInfoSetFromFile (PCMegaFile pmf)
{
   PCMMLNode2 pNode = MMLFileOpen (pmf, CircumrealityTitleInfo(), &CLSID_MegaTitleInfo);
   if (!pNode)
      return NULL;   // no node

   PCResTitleInfoSet pNew = new CResTitleInfoSet;
   if (!pNew) {
      delete pNode;
      return NULL;
   }
   if (!pNew->MMLFrom (pNode)) {
      delete pNode;
      delete pNew;
      return NULL;
   }

   delete pNode;
   return pNew;
}


/*************************************************************************************
CMainWindow::Init - Initalizes the main window.

inputs
   PWSTR             pszMegaFile - Megafile to read the information from.
returns
   BOOL - TRUE if success
*/
BOOL CMainWindow::Init (PWSTR pszMegaFile)
{
   CMegaFile MegaFile;
   if (!MegaFile.Init (pszMegaFile, &GUID_MegaCircumreality, FALSE))
      return FALSE;

   // remeber what file using
   wcscpy (m_szConnectFile, pszMegaFile);
   m_fConnectRemote = FALSE;
   m_dwConnectPort = 0;
   MemZero (&m_memShardParam);
   MemCat (&m_memShardParam, L"offline");
   if (gdwCmdLineParam) {
      PCResTitleInfoSet pSet = PCResTitleInfoSetFromFile (&MegaFile);
      PCResTitleInfo pInfo = (pSet && pSet->m_lPCResTitleInfo.Num()) ? *((PCResTitleInfo*)pSet->m_lPCResTitleInfo.Get(0)) : NULL;
         // always get the first one
      DWORD dwNum = pInfo ? pInfo->m_lPCResTitleInfoShard.Num() : 0;
      DWORD dwIndex = dwNum ? min(dwNum-1, gdwCmdLineParam-1) : 0;
      PCResTitleInfoShard pShard = dwNum ? *((PCResTitleInfoShard*)pInfo->m_lPCResTitleInfoShard.Get(dwIndex)) : NULL;

      if (pShard) {
         m_fConnectRemote = TRUE;
         m_dwConnectPort = pShard->m_dwPort;
         MemZero (&m_memShardParam);
         MemCat (&m_memShardParam, (PWSTR)pShard->m_memParam.p);
      }

      if (pSet)
         delete pSet;
   }
   BOOL fOffline = !_wcsicmp((PWSTR) m_memShardParam.p, L"offline");

   // fill in the database directory
   wcscpy (m_szDatabaseDir, pszMegaFile);
   PWSTR pszCur, pszDot = NULL;
   for (pszCur = m_szDatabaseDir; pszCur[0]; pszCur++)
      if (pszCur[0] == L'.')
         pszDot = pszCur;
   if (pszDot) {
      pszDot[0] = L'\\'; // wipe out last dot
      pszDot[1] = 0;
   }

   // enumerate all the files in the megafile
   CListVariable lEnumName;
   CListFixed lEnumInfo;
   MFCACHE mc;
   DWORD i;
   memset (&mc, 0, sizeof(mc));
   MegaFile.Enum (&lEnumName, &lEnumInfo);
   PMFFILEINFO pmi = (PMFFILEINFO) lEnumInfo.Get(0);
   for (i = 0; i < lEnumName.Num(); i++, pmi++) {
      PWSTR pszName = (PWSTR)lEnumName.Get(i);
      mc.pbData = MegaFile.Load (pszName, &mc.iSize);
      mc.ftModify = pmi->iTimeModify;

      m_tMFCACHE.Add (pszName, &mc, sizeof(mc));
   } // i

   WNDCLASS wc;
   memset (&wc, 0, sizeof(wc));
   wc.hInstance = ghInstance;
   wc.lpfnWndProc = MainWindowWndProc;
   wc.lpszClassName = gpszCircumRealityServerMainWindow;
   wc.style = CS_HREDRAW | CS_VREDRAW;
   wc.hIcon = LoadIcon (ghInstance, MAKEINTRESOURCE(IDI_APPICON));
   wc.hCursor = LoadCursor (NULL, IDC_ARROW);
   wc.hbrBackground = (HBRUSH)(COLOR_WINDOW+1);
   RegisterClass (&wc);

   m_hWnd = CreateWindow (
      wc.lpszClassName, "Circumreality World Simulator",
      WS_OVERLAPPEDWINDOW,
      CW_USEDEFAULT ,CW_USEDEFAULT ,CW_USEDEFAULT ,CW_USEDEFAULT , NULL,
      LoadMenu(ghInstance, MAKEINTRESOURCE(IDR_MENUMAINWINDOW)), ghInstance, (PVOID) this);
   if (!m_hWnd)
      return FALSE;  // error. shouldt happen

   // if offline then disable shutdown menus
   if (fOffline) {
      HMENU hMenu = GetMenu (m_hWnd);
      EnableMenuItem (hMenu, 0, MF_BYPOSITION | MF_DISABLED | MF_GRAYED);
   }

   // BUGFIX - Keep server window in background when create it
   //ShowWindow (m_hWnd, SW_SHOWNOACTIVATE);
   RECT r;
   GetWindowRect (m_hWnd, &r);
   SetWindowPos (m_hWnd, (HWND)HWND_BOTTOM, r.left,r.top,
      r.right - r.left, r.bottom - r.top, SWP_SHOWWINDOW | SWP_NOACTIVATE);

   // update the help
   HelpConstruct ();

   return TRUE;
}


/*************************************************************************************
CMainWindow::WndProc - Manages the window calls for the main window
*/
LRESULT CMainWindow::WndProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
   switch (uMsg) {
   case WM_CREATE:
      {
         m_hWnd = hWnd; // just in case

         // create the server list
         m_hWndList = CreateWindowEx (WS_EX_CLIENTEDGE, WC_LISTVIEW, "",
            WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SORTASCENDING | LVS_SINGLESEL | LVS_SHOWSELALWAYS,
            CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
            hWnd, (HMENU) IDC_CONNECTLIST, ghInstance, NULL);

         // get the size of the window
         RECT r;
         GetClientRect (hWnd, &r);

         // add columns to the database
         SetTimer (hWnd, TIMER_CONNECTLIST, 1000, NULL); // timer to update list
         LVCOLUMN lvc; 
         lvc.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM; 
         lvc.fmt = LVCFMT_LEFT;
         lvc.cx = r.right / 8;
         lvc.pszText = "Address";  
         ListView_InsertColumn (m_hWndList, lvc.iSubItem = 0, &lvc);
         lvc.pszText = "UniqueID";  
         ListView_InsertColumn (m_hWndList, lvc.iSubItem = 1, &lvc);
         lvc.pszText = "Status";  
         ListView_InsertColumn (m_hWndList, lvc.iSubItem = 2, &lvc);
         lvc.pszText = "User";  
         ListView_InsertColumn (m_hWndList, lvc.iSubItem = 3, &lvc);
         lvc.pszText = "Character";  
         ListView_InsertColumn (m_hWndList, lvc.iSubItem = 4, &lvc);
         lvc.pszText = "Sent KB (Compress)";  
         ListView_InsertColumn (m_hWndList, lvc.iSubItem = 5, &lvc);
         lvc.pszText = "Received KB (Compress)";  
         ListView_InsertColumn (m_hWndList, lvc.iSubItem = 6, &lvc);
         lvc.pszText = "Connect time";  
         ListView_InsertColumn (m_hWndList, lvc.iSubItem = 7, &lvc);

         MoveWindow (m_hWndList, r.left, r.top, r.right - r.left,
            r.bottom - r.top, TRUE);
      }
      break;

   case WM_SIZE:
      {
         RECT r;
         
         GetClientRect (hWnd, &r);

         // move the list
         MoveWindow (m_hWndList, r.left, r.top, r.right - r.left,
            r.bottom - r.top, TRUE);
      }
      break;

   case WM_COMMAND:
      switch (LOWORD(wParam)) {
      case ID_SHUTDOWN_IMMEDIATELY:
      case ID_SHUTDOWN_IN30SECONDS:
      case ID_SHUTDOWN_IN60SECONDS:
      case ID_SHUTDOWN_IN2MINUTES:
      case ID_SHUTDOWN_IN5MINUTES:
      case ID_SHUTDOWN_ABOUTSHUTDOWN:
         {
            double fTime;
            switch (LOWORD(wParam)) {
            case ID_SHUTDOWN_IMMEDIATELY:
               fTime = 1;
               break;
            case ID_SHUTDOWN_IN30SECONDS:
               fTime = 30;
               break;
            case ID_SHUTDOWN_IN60SECONDS:
               fTime = 60;
               break;
            case ID_SHUTDOWN_IN2MINUTES:
               fTime = 120;
               break;
            case ID_SHUTDOWN_IN5MINUTES:
               fTime = 300;
               break;
            case ID_SHUTDOWN_ABOUTSHUTDOWN:
            default:
               fTime = 0;
               break;
            } // switch

            // find the function
            DWORD dwShutDownTimed = m_pVM ? m_pVM->FunctionNameToID (L"shutdowntimed") : -1;
            if (dwShutDownTimed == (DWORD)-1)
               return 0;   // error, cant find

            // call it
            PCMIFLVarList pl = new CMIFLVarList;
            if (!pl)
               return 0;

            CMIFLVarLValue var;
            var.m_Var.SetBOOL (FALSE); // so dont restart
            pl->Add (&var.m_Var, TRUE);

            var.m_Var.SetDouble (fTime);  // number of seconds
            pl->Add (&var.m_Var, TRUE);

            DWORD dwRet;
            CPUMonitor(NULL); // for accurate CPU monitoring
            dwRet = m_pVM->FunctionCall (dwShutDownTimed, pl, &var);
               // note: Ignoring dwRet
            CPUMonitor(NULL); // for accurate CPU monitoring
            pl->Release();
         }
         return 0;
      } // switch
      break;

   case WM_SEARCHCALLBACK: // search callback
      if (m_pTextLog)
         m_pTextLog->OnMessage ((PTEXTLOGSEARCH)lParam, FALSE);
      // else, will leak memory, but only in extreme cases
      return 0;

   case WM_USER+124: // new data came in
      {
         if (!m_pIT)
            return 0;   // shouldnt happen
         
         // if we're already in a function then set a timer off so can check later...
         if (!m_pVM || m_pVM->AlreadyInFunction()) {
            // only set the timer if we don't already have one set
            if (!m_fTimerInFunc) {
               SetTimer (hWnd, TIMER_ALREADYINFUNC, 50, NULL);
               m_fTimerInFunc = TRUE;
            }
            return 0;
         }

         // if timer was running then kill it
         if (m_fTimerInFunc) {
            KillTimer (hWnd, TIMER_ALREADYINFUNC);
            m_fTimerInFunc = FALSE;
         }

         // see what new connections have
         DWORD dwID, dwRet;
         while (-1 != (dwID = m_pIT->ConnectionCreatedGet ())) {
            // DOCUMENT: In tutorial will need to document how connection createdget is called

            // if there's no ConnectionNew() function then ignore
            if (m_dwConnectionNew == -1)
               continue;

            // call it
            PCMIFLVarList pl = new CMIFLVarList;
            if (!pl) {
               // shouldnt happen
               m_pIT->ConnectionRemove (dwID);
               continue;
            }
            CMIFLVarLValue var;
            var.m_Var.SetDouble (dwID);
            pl->Add (&var.m_Var, TRUE);
            CPUMonitor(NULL); // for accurate CPU monitoring
            dwRet = m_pVM->FunctionCall (m_dwConnectionNew, pl, &var);
               // note: Ignoring dwRet
            CPUMonitor(NULL); // for accurate CPU monitoring
            pl->Release();

            if (var.m_Var.TypeGet() != MV_OBJECT) {
               // automatically delete
               m_pIT->ConnectionRemove (dwID);
               continue;
            }

            // else, it's an object...
            PCCircumrealityPacket pp = m_pIT->ConnectionGet (dwID);
            if (!pp) {
               // shopuldnt hapopen
               m_pIT->ConnectionRemove (dwID);
               continue;
            }
            CIRCUMREALITYPACKETINFO Info;
            pp->InfoGet (&Info);
            Info.gObject = var.m_Var.GetGUID();
            wcscpy (Info.szStatus, L"Connecting");
            pp->InfoSet (&Info);
            m_pIT->ConnectionGetRelease();   // so can resume deleting
         } // while connection new

         // what new messages have
         while (-1 != (dwID = m_pIT->ConnectionReceivedGet())) {
            // DOCUMENT: In tutorial will need to document how messages are received from client...

            // loop through all the messages...
            PCCircumrealityPacket pp = m_pIT->ConnectionGet (dwID);
            if (!pp)
               continue;   // shouldnt of had this

            DWORD dwType;
            while (-1 != (dwType = pp->PacketGetType(0))) {
               PCMem pMem;
               PCMMLNode2 pNode = NULL;
               if (dwType == CIRCUMREALITYPACKET_VOICECHAT) {
                  // expecting MML and memory
                  pMem = new CMem;
                  pNode = pp->PacketGetMML (0, pMem);
                  if (!pNode || !pMem) {
                     if (pNode)
                        delete pNode;
                     if (pMem)
                        delete pMem;
                     continue;
                  }
               }
               else if (dwType == CIRCUMREALITYPACKET_UNIQUEID) {
                  // expecting MML and memory
                  pMem = pp->PacketGetMem (0);
                  if (!pMem)
                     continue;
               }
               else if (dwType == CIRCUMREALITYPACKET_INFOFORSERVER) {
                  // expecting MML
                  pMem = NULL;
                  pNode = pp->PacketGetMML (0);
                  if (!pNode)
                     continue;
               }
               else {
                  // get the raw memory
                  pMem = pp->PacketGetMem(0);
                  if (!pMem)
                     continue;
               }

               // based on the type of packet...
               switch (dwType) {
               case CIRCUMREALITYPACKET_FILEREQUEST:
               case CIRCUMREALITYPACKET_FILEREQUESTIGNOREDIR:
                  PacketFileRequest (pp, pMem, (dwType == CIRCUMREALITYPACKET_FILEREQUESTIGNOREDIR));
                  break;
               case CIRCUMREALITYPACKET_FILEDATEQUERY:
                  PacketFileDateQuery (pp, pMem);
                  break;
               case CIRCUMREALITYPACKET_TEXTCOMMAND:
// #ifdef _DEBUG
//          OutputDebugStringW (L"\r\nCommand:");
//          OutputDebugStringW ((PWSTR)pMem->p + 1);
//          OutputDebugStringW (L"\r\n");
// #endif

                  PacketTextCommand (pp, pMem);
                  break;
               case CIRCUMREALITYPACKET_INFOFORSERVER:
                  PacketInfoForServer (pp, pNode);
                  break;
               case CIRCUMREALITYPACKET_UPLOADIMAGE:
                  PacketUploadImage (pp, pMem);
                  break;
               case CIRCUMREALITYPACKET_UNIQUEID:
                  {
                     CIRCUMREALITYPACKETINFO info;
                     if (pMem->m_dwCurPosn < sizeof(info.szUniqueID)-sizeof(WCHAR)) {
                        pp->InfoGet (&info);
                        memset (info.szUniqueID, 0, sizeof(info.szUniqueID));
                        memcpy (info.szUniqueID, pMem->p, pMem->m_dwCurPosn);
                        pp->InfoSet (&info);
                     }
                  }
                  break;

               case CIRCUMREALITYPACKET_VOICECHAT:
                  {
                     // skip the high level element
                     PCMMLNode2 pSub = NULL;
                     PWSTR psz;
                     pNode->ContentEnum (0, &psz, &pSub);

                     PacketVoiceChat (pp, pSub, pMem);
                  }
                  break;
               case CIRCUMREALITYPACKET_SERVERQUERYWANTTTS:
               case CIRCUMREALITYPACKET_SERVERQUERYWANTRENDER:
                  {
                     PacketServerQueryWant (dwID, dwType == CIRCUMREALITYPACKET_SERVERQUERYWANTRENDER, 
                        pp, pMem);
                  }
                  break;

               case CIRCUMREALITYPACKET_SERVERREQUESTTTS:
               case CIRCUMREALITYPACKET_SERVERREQUESTRENDER:
                  {
                     PacketServerRequest (dwID, dwType == CIRCUMREALITYPACKET_SERVERREQUESTRENDER, 
                        pp, pMem);
                  }
                  break;

               case CIRCUMREALITYPACKET_SERVERCACHETTS:
               case CIRCUMREALITYPACKET_SERVERCACHERENDER:
                  {
                     PacketServerCache (dwID, dwType == CIRCUMREALITYPACKET_SERVERCACHERENDER, 
                        pp, pMem);
                  }
                  break;
               case CIRCUMREALITYPACKET_CLIENTLOGOFF:
                  PacketLogOff (pp, pMem);
                  break;
               default:
                  // only care about text commands. These are the only ones that
                  // should reach this far anyway
                  break;
               }
               if (pNode)
                  delete pNode;
               if (pMem)
                  delete pMem;
            } // while packet-gettyope
            m_pIT->ConnectionGetRelease();   // so can resume deleting
         } // while connection received

         // see what errors have
         int iErr;
         WCHAR szErr[256];
         while (-1 != (dwID = m_pIT->ConnectionErrGet (&iErr, szErr))) {
            // DOCUMENT: In tutorial will need to document how connectionerror is called

            // if there's no ConnectionNew() function then ignore
            if (m_dwConnectionError == -1) {
               // remove it
               m_pIT->ConnectionRemove (dwID);
               continue;
            }

            // find the object...
            PCCircumrealityPacket pp = m_pIT->ConnectionGet (dwID);
            if (!pp) {
               // remove it
               m_pIT->ConnectionRemove (dwID);
               continue;
            }
            CIRCUMREALITYPACKETINFO info;
            pp->InfoGet (&info);
            m_pIT->ConnectionGetRelease();   // so can resume deleting

            if (!m_pVM || !m_pVM->ObjectFind (&info.gObject)) {
               // remove it
               m_pIT->ConnectionRemove (dwID);
               continue;
            }

            // call it
            PCMIFLVarList pl = new CMIFLVarList;
            if (!pl) {
               // shouldnt happen
               m_pIT->ConnectionRemove (dwID);
               continue;
            }
            CMIFLVar vTemp;
            vTemp.SetDouble (iErr);
            pl->Add (&vTemp, FALSE);
            vTemp.SetString (szErr);
            pl->Add (&vTemp, FALSE);

            CMIFLVarLValue var;
            CPUMonitor(NULL); // for accurate CPU monitoring
            dwRet = m_pVM->MethodCall (&info.gObject, m_dwConnectionError, pl, 0, 0, &var);
               // note: Ignoring dwRet
            CPUMonitor(NULL); // for accurate CPU monitoring
            pl->Release();
         } // while connection new

         // save the database bit by bit
         if (m_pDatabase)
            m_pDatabase->SavePartial (m_dwDatabaseCount++);
      }
      return 0;


   case WM_TIMER:
      if (wParam == TIMER_ALREADYINFUNC) {
         return SendMessage (hWnd, WM_USER+124, 0, 0);
      }
      else if (wParam == TIMER_CONNECTLIST) {
         // potentially immediate shutdown
         if (m_fShutDownImmediately && (!m_pVM || !m_pVM->AlreadyInFunction())) {
            DestroyWindow (hWnd);
            return 0;
         }

         // save the database bit by bit
         if (m_pDatabase)
            m_pDatabase->SavePartial (m_dwDatabaseCount++);

         // update the connection list
         CListFixed lConnect;
         if (m_pIT)
            m_pIT->ConnectionEnum (&lConnect);
         DWORD dwNum;
         DWORD *padwConnect = (DWORD*)lConnect.Get(0);

         // get the time, in millseconds
         LARGE_INTEGER     liPerCountFreq, liCount;
         __int64  iTime;
         QueryPerformanceFrequency (&liPerCountFreq);
         QueryPerformanceCounter (&liCount);
         iTime = *((__int64*)&liCount) / ( *((__int64*)&liPerCountFreq) / 1000);

         // remember current selection
         // clears the list databases and refreshes it with an up-to-date list of databases
         DWORD i;
         LVITEM   pItem;

         // remember the current selection, as a pointer to the database object
         // so can re-point there
         dwNum = ListView_GetItemCount (m_hWndList);
         for (i = 0; i < dwNum; i++)
            if (ListView_GetItemState (m_hWndList, i,  LVIS_SELECTED))
               break;
         DWORD dwSelected = -1;
         if (i < dwNum) {
            memset (&pItem, 0, sizeof(pItem));
            pItem.iItem = i;
            pItem.mask = LVIF_PARAM;
            ListView_GetItem (m_hWndList, &pItem);
            dwSelected = (DWORD) pItem.lParam;
         }



         // wipe the list
         ListView_DeleteAllItems (m_hWndList);

         // repopulate it by enumeting through gListDatabases, asking each database for its name
         CIRCUMREALITYPACKETINFO info;
         char  szTemp[sizeof(info.szIP)*2+2];
         PCCircumrealityPacket pp;
         BOOL  fFound = FALSE;
         dwNum = lConnect.Num();
         for (i = 0; i < dwNum; i++) {
            pp = m_pIT->ConnectionGet(padwConnect[i]);
            if (!pp)
               continue;

            pp->InfoGet (&info);
            m_pIT->ConnectionGetRelease();   // so can resume deleting

            // IP
            WideCharToMultiByte (CP_ACP, 0, info.szIP, -1, szTemp, sizeof(szTemp), 0,0);
            memset (&pItem, 0, sizeof(pItem));
            pItem.mask = LVIF_TEXT | LVIF_PARAM | LVIF_STATE;
            pItem.pszText = szTemp;
            pItem.lParam = padwConnect[i];
            pItem.stateMask = LVIS_SELECTED;

            // reset the selection if find it
            if (padwConnect[i] == dwSelected) {
               pItem.state = LVIS_SELECTED;
               fFound = TRUE;
            }
            int   iIndex;
            iIndex = ListView_InsertItem (m_hWndList, &pItem);

            // set the text for the subitems
            pItem.mask = LVIF_TEXT;
            pItem.iItem = iIndex;
            pItem.pszText = szTemp;

            // unique id
            pItem.iSubItem = 1;
            WideCharToMultiByte (CP_ACP, 0, info.szUniqueID, -1, szTemp, sizeof(szTemp), 0,0);
            ListView_SetItem (m_hWndList, &pItem);

            // status
            pItem.iSubItem = 2;
            WideCharToMultiByte (CP_ACP, 0, info.szStatus, -1, szTemp, sizeof(szTemp), 0,0);
            ListView_SetItem (m_hWndList, &pItem);

            // user
            pItem.iSubItem = 3;
            WideCharToMultiByte (CP_ACP, 0, info.szUser, -1, szTemp, sizeof(szTemp), 0,0);
            ListView_SetItem (m_hWndList, &pItem);

            // character
            pItem.iSubItem = 4;
            WideCharToMultiByte (CP_ACP, 0, info.szCharacter, -1, szTemp, sizeof(szTemp), 0,0);
            ListView_SetItem (m_hWndList, &pItem);


            // sent
            pItem.iSubItem = 5;
            sprintf (szTemp, "%.2f (%d%%)", (double)info.iSendBytesComp / 1000.0,
               info.iSendBytes ? (int)((info.iSendBytes - info.iSendBytesComp) * 100 / info.iSendBytes) : (int)0);
            ListView_SetItem (m_hWndList, &pItem);

            // received
            pItem.iSubItem = 6;
            sprintf (szTemp, "%.2f (%d%%)", (double)info.iReceiveBytesComp / 1000.0,
               info.iReceiveBytes ? (int)((info.iReceiveBytes - info.iReceiveBytesComp) * 100 / info.iReceiveBytes) : (int)0);
            ListView_SetItem (m_hWndList, &pItem);

            // on since
            pItem.iSubItem = 7;
            double fTimeOn = (double)(iTime - info.iConnectTime) / 1000.0;
            if (fTimeOn < 60)
               sprintf (szTemp, "%.2f sec", (double)fTimeOn);
            else if (fTimeOn < 60 * 60)
               sprintf (szTemp, "%.2f min", (double)fTimeOn / 60.0);
            else
               sprintf (szTemp, "%.2f hr", (double)fTimeOn / 60.0 / 60.0);
            ListView_SetItem (m_hWndList, &pItem);
         }

         // reset the selection if there's no selection
         if (!fFound)
            ListView_SetItemState(m_hWndList, 0, LVIS_SELECTED, LVIS_SELECTED);


         // redraw
         InvalidateRect (m_hWndList, NULL, FALSE);
      } // if timer is connectionlist
      break;

   // BUGFIX - Handle WM_QUERYENDSESSION and save all the data
   case WM_QUERYENDSESSION:
      // NOTE: Hopefully this will give enough time for everything to shut down. Old
      // versions of windows would shut down whether or not everything was closed up.
      // I think XP will wait a bit
      SendMessage (hWnd, WM_COMMAND, ID_SHUTDOWN_IMMEDIATELY, 0);
      return TRUE;

   // BUGFIX - Trap WM_CLOSE and recommend closing nicely
   case WM_CLOSE:
      {
         int iRet = EscMessageBox (m_hWnd, L"Circumreality World Simulator", L"Do you want to close nicely?",
            L"If you close nicely, all the connected players will receive a shutdown message "
            L"and all the data in the databases will be saved. If you press No then players "
            L"won't be immediately aware of the shutdown and data may not be saved.", MB_YESNOCANCEL);
         if (iRet == IDYES) {
            SendMessage (hWnd, WM_COMMAND, ID_SHUTDOWN_IMMEDIATELY, 0);
            return 0;
         }
         if (iRet != IDNO)
            return 0;   // cancelled
         
         // else, default handler
      }
      // fall through to default handler
      break;

   case WM_DESTROY:
      if (m_fTimerInFunc) {
         KillTimer (hWnd, TIMER_ALREADYINFUNC);
         m_fTimerInFunc = FALSE;
      }

      KillTimer (hWnd, TIMER_CONNECTLIST);

      if (m_fPostQuitMessage)
         PostQuitMessage (0);
      m_hWnd = NULL;
      break;

   } // switch uMsg


   // else
   return DefWindowProc (hWnd, uMsg, wParam, lParam);
}

/*************************************************************************************
CMainWindow::PacketFileDateQuery - Called when client wants to know if files are out
of date.

inputs
   PCCircumrealityPacket pp - Packet it came from
   PCMem       pMem - Memory contianing the data, with m_pdwCurPosn being the size
returns
   none
*/
void CMainWindow::PacketFileDateQuery (PCCircumrealityPacket pp, PCMem pMem)
{
   // create memory to store return info
   CMem mem;
   
   // loop...
   PBYTE pbCur = (PBYTE)pMem->p;
   DWORD dwLeft = (DWORD)pMem->m_dwCurPosn;

   PCMegaFile pmf = BinaryDataMega();

   while (dwLeft >= sizeof(FILETIME)+sizeof(WCHAR)) {
      PFILETIME pft = (PFILETIME)pbCur;
      pbCur += sizeof(FILETIME);
      dwLeft -= sizeof(FILETIME);

      // string...
      PWSTR psz = (PWSTR)pbCur;
      PWSTR pszCur = psz;
      for (; (dwLeft >= sizeof(WCHAR)) && pszCur[0]; dwLeft -= sizeof(WCHAR), pbCur += sizeof(WCHAR), pszCur++);
      if (dwLeft < sizeof(WCHAR))
         break;      // shouldnt happen. Implies hackers at work

      // advance beyond null
      dwLeft -= sizeof(WCHAR);
      pbCur += sizeof(WCHAR);
   
      // BUGFIX - If the file in question ends with ".me3" assume it's a library
      // that can be any location. therefore, search for any directory
      // BUGFIX - Same for mlx and tts
      DWORD dwLen = (DWORD)wcslen(psz);
      PWSTR pszLookFor = psz;
      if ((dwLen >= 4) && (
         !_wcsicmp(psz + (dwLen-4), L".me3") ||
         !_wcsicmp(psz + (dwLen-4), L".tts") ||
         !_wcsicmp(psz + (dwLen-4), L".mlx"))) {
         pszLookFor = FindIgnoreDir (psz,&m_tMFCACHE);
         if (!pszLookFor)
            pszLookFor = psz; // at least something
      }

      // find this file
      PMFCACHE pmc = (PMFCACHE) m_tMFCACHE.Find (pszLookFor);

      if (pmc && !memcmp(&pmc->ftModify, pft, sizeof(pmc->ftModify)))
         continue;   // file exists and is the right time, so dont bother

      // if cant find, try in datacahe. If find then continue and don't append
      if (!pmc && pmf->Exists (pszLookFor))
         continue;

      // else, file doesn't exist anymore, or is the wrong time, so need to append
      mem.StrCat (psz, (DWORD)wcslen(psz)+1);
   }

   // send the delete message
   pp->PacketSend (CIRCUMREALITYPACKET_FILEDELETE, mem.p, (DWORD)mem.m_dwCurPosn);
}



/*************************************************************************************
FindIgnoreDir - Finds a file if the directories are to be ignored.
This only does name-for-name comparison.

inputs
   WCHAR          pszFind - To find
   PCBTree        pt - Tree witth filenames
returns
   PWSTR - Name of the file actually found (stored in the tree), so dont change this
*/
static PWSTR FindIgnoreDir (WCHAR *pszFind, PCBTree pt)
{
   PWSTR pszCur;
   DWORD dwLenFind;
   while (pszCur = wcschr(pszFind, L'\\'))
      pszFind = pszCur+1;
   dwLenFind = (DWORD)wcslen(pszFind);

   DWORD i;
   for (i = 0; i < pt->Num(); i++) {
      PWSTR psz = pt->Enum (i);
      DWORD dwLen = (DWORD)wcslen(psz);
      if (dwLen < dwLenFind)
         continue;   // too short

      // if not backslash at start then bad
      if ((dwLen > dwLenFind) && (psz[dwLen - dwLenFind - 1] != L'\\'))
         continue;   // no backslash

      // compare
      if (!_wcsicmp(psz + (dwLen - dwLenFind), pszFind))
         return psz; // found it
   } // i

   // else not
   return NULL;
}


/*************************************************************************************
CMainWindow::PacketFileRequest - Called when the client requests a file.

inputs
   PCCircumrealityPacket pp - Packet it came from
   PCMem       pMem - Memory contianing the data, with m_pdwCurPosn being the size
   BOOL        fIgnoreDir - If TRUE then ignore the directory
returns
   none
*/
void CMainWindow::PacketFileRequest (PCCircumrealityPacket pp, PCMem pMem, BOOL fIgnoreDir)
{
   // create a string with this
   DWORD dwLen = (DWORD)pMem->m_dwCurPosn / sizeof(WCHAR);
   if (!dwLen)
      return;  // error, shouldnt happen. If does implies hacker

   PWSTR psz = (PWSTR)pMem->p;
   if (psz[dwLen-1])
      return;  // error, shouldnt happend. If does implies hacker

   // find the file
   PWSTR pszLookFor = psz;
   if (fIgnoreDir) {
      pszLookFor = FindIgnoreDir (psz, &m_tMFCACHE);
      if (!pszLookFor)
         pszLookFor = psz;
   }
   PMFCACHE pmc = (PMFCACHE) m_tMFCACHE.Find (pszLookFor);

   // if this is a project or library file then DONT send, so that hackers cannot
   // access the data
   dwLen = (DWORD)wcslen(psz);
   if (dwLen >= 4) {
      if (!_wcsicmp(psz + (dwLen-4), L".mfl") || !_wcsicmp(psz + (dwLen-4), L".mfp"))
         pmc = NULL;
   }

   // if still cant find pmc then see about using the binary database
   __int64 iLoad = 0;
   PVOID pLoad = NULL;
   PCMegaFile pmf = NULL;
   MFFILEINFO info;
   if (!pmc && (pmf = BinaryDataMega()) && pmf->Exists (pszLookFor, &info))
      pLoad = pmf->Load (pszLookFor, &iLoad);

   CMem mem;
   DWORD dwNeed = (dwLen+1)*sizeof(WCHAR) + sizeof(FILETIME) + (DWORD)iLoad;
   if (!mem.Required (dwNeed)) {
      if (pLoad)
         MegaFileFree (pLoad);
      return;  // error, out of memory
   }
   FILETIME *pft = (FILETIME*)mem.p;
   if (pmc)
      *pft = pmc->ftModify;
   else if (pLoad)
      *pft = info.iTimeModify;
   else  // dont know, so send 0
      memset (pft, 0, sizeof(FILETIME));
   memcpy (pft+1, psz, (dwLen+1)*sizeof(WCHAR));
   if (pLoad) {
      memcpy ((PBYTE) pft + (dwNeed - (DWORD)iLoad), pLoad, (DWORD)iLoad);
      MegaFileFree (pLoad);
   }

   // send back response...
   pp->PacketSend (CIRCUMREALITYPACKET_FILEDATA, mem.p, dwNeed,
      pmc ? pmc->pbData : NULL, (pmc && pmc->pbData) ? (DWORD) pmc->iSize : 0);

#ifdef _DEBUG
   OutputDebugString ("\r\nSending ");
   OutputDebugStringW (psz);
#endif
}

/*************************************************************************************
CMainWindow::PacketTextCommand - Called when a text command packet comes in

inputs
   PCCircumrealityPacket pp - Packet it came from
   PCMem       pMem - Memory contianing the data, with m_pdwCurPosn being the size
returns
   none
*/
void CMainWindow::PacketTextCommand (PCCircumrealityPacket pp, PCMem pMem)
{
   // create a string with this
   DWORD dwLen = (DWORD)pMem->m_dwCurPosn;
   if ((dwLen < sizeof(LANGID) + sizeof(WCHAR)) || (((PWSTR)pMem->p + (dwLen/2-1))[0]))
      // shouldnt happen... implies someone trying an exploit
      return;

   // verify that can send message
   if (m_dwConnectionMessage == -1)
      return;

   CIRCUMREALITYPACKETINFO info;
   pp->InfoGet (&info);

   if (!m_pVM || !m_pVM->ObjectFind (&info.gObject))
      return;

   // call it
   PCMIFLVarList pl = new CMIFLVarList;
   if (!pl)
      return;

   CMIFLVarLValue var;
   var.m_Var.SetDouble (*((LANGID*)pMem->p));   // first param is langid
   pl->Add (&var.m_Var, TRUE);
   var.m_Var.SetString ((PWSTR)((PBYTE)pMem->p + sizeof(LANGID)));   // second param is string
   pl->Add (&var.m_Var, TRUE);
   var.m_Var.SetUndefined();
   CPUMonitor(NULL); // for accurate CPU monitoring
   m_pVM->MethodCall (&info.gObject, m_dwConnectionMessage, pl, 0, 0, &var);
   CPUMonitor(NULL); // for accurate CPU monitoring
      // note: Ignoring dwRet
   pl->Release();
}



/*************************************************************************************
CMainWindow::PacketInfoForServer - Called when information for the server comes in

inputs
   PCCircumrealityPacket pp - Packet it came from
   PCMMLNode2      pNode - Node information
returns
   none
*/
void CMainWindow::PacketInfoForServer (PCCircumrealityPacket pp, PCMMLNode2 pNode)
{
   PWSTR psz;
   PCMMLNode2 pSub;
   DWORD i;
   for (i = 0; i < pNode->ContentNum(); i++) {
      pSub = NULL;
      pNode->ContentEnum (i, &psz, &pSub);
      if (!pSub)
         continue;

      // get the string for the name and the value
      PWSTR pszName = pSub->NameGet();
      if (!pszName)
         continue;
      PWSTR pszValue = pSub->AttribGetString (L"v");
      if (!pszValue)
         continue;

      // verify that can send message
      if (m_dwConnectionInfoForServer == -1)
         continue;

      CIRCUMREALITYPACKETINFO info;
      pp->InfoGet (&info);

      if (!m_pVM || !m_pVM->ObjectFind (&info.gObject))
         continue;

      // call it
      PCMIFLVarList pl = new CMIFLVarList;
      if (!pl)
         continue;

      CMIFLVarLValue var;
      var.m_Var.SetString (pszName);
      pl->Add (&var.m_Var, TRUE);
      var.m_Var.SetString (pszValue);
      pl->Add (&var.m_Var, TRUE);
      var.m_Var.SetUndefined();
      CPUMonitor(NULL); // for accurate CPU monitoring
      m_pVM->MethodCall (&info.gObject, m_dwConnectionInfoForServer, pl, 0, 0, &var);
      CPUMonitor(NULL); // for accurate CPU monitoring
         // note: Ignoring dwRet
      pl->Release();
   } // i
}

/*************************************************************************************
CMainWindow::PacketUploadImage - Called when an uploade image packet comes in

inputs
   PCCircumrealityPacket pp - Packet it came from
   PCMem       pMem - Memory contianing the data, with m_pdwCurPosn being the size
returns
   none
*/
void CMainWindow::PacketUploadImage (PCCircumrealityPacket pp, PCMem pMem)
{
   // verify that can send message
   if (m_dwConnectionUploadImage == -1)
      return;

   // determine the size
   DWORD dwSize = (DWORD)pMem->m_dwCurPosn;
   if (dwSize < 3 * sizeof(DWORD))
      return;  // error in packet

   // params
   DWORD *pdw = (DWORD*) pMem->p;
   DWORD dwNum = pdw[0];
   DWORD dwWidth = pdw[1];
   DWORD dwHeight = pdw[2];
   if (!dwNum || !dwWidth || !dwHeight)
      return;  // error in packeet

   // limit this to 3 MB, about 1000 x 1000 pixels
   if (dwSize > 3000000)
      return;

   // if expected is not the size then error
   DWORD dwExpect = sizeof(DWORD)*3 + 3 * dwWidth * dwHeight;
   if (dwExpect != dwSize)
      return;


   // convert this to an image
   CImage Image;
   if (!Image.Init (dwWidth, dwHeight))
      return;
   PIMAGEPIXEL pip = Image.Pixel(0,0);
   DWORD i;
   PBYTE pb = (PBYTE) (pdw + 3);
   for (i = 0; i < dwWidth * dwHeight; i++, pip++, pb += 3) {
      pip->wRed = Gamma (pb[0]);
      pip->wGreen = Gamma (pb[1]);
      pip->wBlue = Gamma (pb[2]);
   } // i

   // convert to jpeg
   HWND hWnd = GetDesktopWindow ();
   HDC hDC = GetDC (hWnd);
   if (!hDC)
      return;
   HBITMAP hBitmap = Image.ToBitmap (hDC);
   ReleaseDC (hWnd, hDC);

   CMem mem;
   BOOL fRet = BitmapToJPeg (hBitmap, &mem);
   DeleteObject (hBitmap);
   if (!fRet)
      return;  // error, shouldnt happen

   // make a string out of this
   CMem memString;
   if (!BinaryToStringPlusOne ((PBYTE)mem.p, (DWORD)mem.m_dwCurPosn, &memString))
      return;


   // call into callback
   CIRCUMREALITYPACKETINFO info;
   pp->InfoGet (&info);

   if (!m_pVM || !m_pVM->ObjectFind (&info.gObject))
      return;

   // call it
   PCMIFLVarList pl = new CMIFLVarList;
   if (!pl)
      return;

   CMIFLVarLValue var;
   var.m_Var.SetDouble (dwNum);   // first param is picture ID
   pl->Add (&var.m_Var, TRUE);
   var.m_Var.SetDouble (dwWidth);   // second is width
   pl->Add (&var.m_Var, TRUE);
   var.m_Var.SetDouble (dwHeight);   // third is height
   pl->Add (&var.m_Var, TRUE);
   var.m_Var.SetString ((PWSTR)memString.p);   // second param is string
   pl->Add (&var.m_Var, TRUE);
   var.m_Var.SetUndefined();
   CPUMonitor(NULL); // for accurate CPU monitoring
   m_pVM->MethodCall (&info.gObject, m_dwConnectionUploadImage, pl, 0, 0, &var);
   CPUMonitor(NULL); // for accurate CPU monitoring
      // note: Ignoring dwRet
   pl->Release();
}



/*************************************************************************************
CMainWindow::PacketVoiceChat - Called when an uploade image packet comes in

inputs
   PCCircumrealityPacket pp - Packet it came from
   PCMMLNode2  pNode - Node with information
   PCMem       pMem - Memory contianing the data, with m_pdwCurPosn being the size
returns
   none
*/
#define MAXVOICECHATSEGMENT         5     // cant send any more than 5 seconds in a segment
void CMainWindow::PacketVoiceChat (PCCircumrealityPacket pp, PCMMLNode2 pNode, PCMem pMem)
{
   if (!pMem || !pNode)
      return;  // error

   // verify that it's a valid packet... if not, ignore it
   if (!VoiceChatValidateMaxLength ((PBYTE)pMem->p, (DWORD)pMem->m_dwCurPosn, MAXVOICECHATSEGMENT))
      return;

   // test that is valid wave file for voice chat
   if (!VoiceChatDeCompress ((PBYTE)pMem->p, (DWORD)pMem->m_dwCurPosn, NULL, NULL, &m_lWaveBaseValid))
      return;

   // verify that can send message
   if (m_dwConnectionVoiceChat == -1)
      return;

   // make a string out of this
   CMem memString;
   if (!BinaryToStringPlusOne ((PBYTE)pMem->p, (DWORD)pMem->m_dwCurPosn, &memString))
      return;

   // call into callback
   CIRCUMREALITYPACKETINFO info;
   pp->InfoGet (&info);

   if (!m_pVM || !m_pVM->ObjectFind (&info.gObject))
      return;

   // call it
   PCMIFLVarList pl = new CMIFLVarList;
   if (!pl)
      return;

   CMIFLVarLValue var;

   // speak to
   GUID gID = GUID_NULL;
   if (sizeof(GUID) != MMLValueGetBinary (pNode, L"ID", (PBYTE)&gID, sizeof(gID)))
      gID = GUID_NULL;
   if (IsEqualGUID(gID, GUID_NULL))
      var.m_Var.SetNULL();
   else
      var.m_Var.SetObject(&gID);
   pl->Add (&var.m_Var, TRUE);

   // style
   PWSTR psz;
   psz = MMLValueGet (pNode, L"style");
   if (psz)
      var.m_Var.SetString (psz);
   else
      var.m_Var.SetNULL();
   pl->Add (&var.m_Var, TRUE);

   // language
   psz = MMLValueGet (pNode, L"language");
   if (psz)
      var.m_Var.SetString (psz);
   else
      var.m_Var.SetNULL();
   pl->Add (&var.m_Var, TRUE);

   // binary
   var.m_Var.SetString ((PWSTR)memString.p);   // second param is string
   pl->Add (&var.m_Var, TRUE);

   var.m_Var.SetUndefined();
   CPUMonitor(NULL); // for accurate CPU monitoring
   m_pVM->MethodCall (&info.gObject, m_dwConnectionVoiceChat, pl, 0, 0, &var);
      // note: Ignoring dwRet
   CPUMonitor(NULL); // for accurate CPU monitoring
   pl->Release();
}

/*************************************************************************************
MainWindowTitle - Creates the main window title's root from the filename.

inputs
   PCWSTR         pszFile - File name, like "c:\world\myworld.crf"
   PSTR           pszTitle - Filled in with the title
   DWORD          dwTitleSize - Size of the title, in chars
returns
   none
*/
void MainWindowTitle (PCWSTR pszFile, PSTR pszTitle, DWORD dwTitleSize)
{
   strcpy (pszTitle, "Circumreality World Simulator - ");
   WideCharToMultiByte (CP_ACP, 0, pszFile, -1, pszTitle+strlen(pszTitle),
      dwTitleSize-(DWORD)strlen(pszTitle), 0, 0);
}


/*************************************************************************************
CMainWindow::PacketServerQueryWant - Sees if the server wants to receive the given
data.

inputs
   DWORD       dwConnectionID - Connection ID
   BOOL        fRender - If TRUE this is a render packet, FALSE its TTS
   PCCircumrealityPacket pp - Packet it came from
   PCMem       pMem - Memory contianing the data, with m_pdwCurPosn being the size
returns
   none
*/
void CMainWindow::PacketServerQueryWant (DWORD dwConnectionID, BOOL fRender, PCCircumrealityPacket pp, PCMem pMem)
{
   PCIRCUMREALITYPACKETSEVERQUERYWANT pw;
   if (pMem->m_dwCurPosn < sizeof(*pw))
      return;

   // make sure size ok
   pw = (PCIRCUMREALITYPACKETSEVERQUERYWANT) pMem->p;
   if ((pw->dwStringSize % sizeof(WCHAR)) || !pw->dwStringSize || (sizeof(*pw) + pw->dwStringSize > pMem->m_dwCurPosn))
      return;

   // make sure string ends in NULL
   PWSTR pszString = (PWSTR)(pw + 1);
   if (pszString[pw->dwStringSize/sizeof(WCHAR)-1])
      return;

   // see if wants
   CMem memReply;
   PCIRCUMREALITYPACKETCLIENTREPLYWANT prw;
   size_t iNeed = sizeof(*prw) + pw->dwStringSize;
   if (!memReply.Required (iNeed))
      return;  // error
   memReply.m_dwCurPosn = iNeed;
   prw = (PCIRCUMREALITYPACKETCLIENTREPLYWANT) memReply.p;
   memset (prw, 0, sizeof(*prw));
   prw->dwQuality = pw->dwQuality;
   prw->dwStringSize = pw->dwStringSize;
   memcpy (prw + 1, pszString, pw->dwStringSize);
   prw->fWant = (m_pIDT && m_pIDT->EntryAddAccept (FALSE, pszString, pw->dwQuality,
      fRender ? IMAGEDATABASETYPE_IMAGE : IMAGEDATABASETYPE_WAVE,
      dwConnectionID,
      NULL, 0));

   // send this
   pp->PacketSend (
      fRender ? CIRCUMREALITYPACKET_CLIENTREPLYWANTRENDER : CIRCUMREALITYPACKET_CLIENTREPLYWANTTTS,
      memReply.p, (DWORD)memReply.m_dwCurPosn);
   
   // done
}


/*************************************************************************************
CMainWindow::PacketServerCache - Provides data for the server to cache

inputs
   DWORD       dwConnectionID - Connection ID
   BOOL        fRender - If TRUE this is a render packet, FALSE its TTS
   PCCircumrealityPacket pp - Packet it came from
   PCMem       pMem - Memory contianing the data, with m_pdwCurPosn being the size
returns
   none
*/
void CMainWindow::PacketServerCache (DWORD dwConnectionID, BOOL fRender, PCCircumrealityPacket pp, PCMem pMem)
{
   PCIRCUMREALITYPACKETSEVERCACHE pw;
   if (pMem->m_dwCurPosn < sizeof(*pw))
      return;

   // make sure size ok
   pw = (PCIRCUMREALITYPACKETSEVERCACHE) pMem->p;
   if ((pw->dwStringSize % sizeof(WCHAR)) || !pw->dwStringSize || !pw->dwDataSize ||
      (sizeof(*pw) + pw->dwStringSize + pw->dwDataSize > pMem->m_dwCurPosn))
      return;

   // make sure string ends in NULL
   PWSTR pszString = (PWSTR)(pw + 1);
   if (pszString[pw->dwStringSize/sizeof(WCHAR)-1])
      return;

   // send this on
   if (m_pIDT)
      m_pIDT->EntryAdd (FALSE, 0, pszString, pw->dwQuality, 0,
         fRender ? IMAGEDATABASETYPE_IMAGE : IMAGEDATABASETYPE_WAVE,
         dwConnectionID,
         (PBYTE)(pw+1) + pw->dwStringSize,
         pw->dwDataSize);
   
   // done
}


/*************************************************************************************
CMainWindow::PacketServerRequest - Player's computer requests the download of data.

inputs
   DWORD       dwConnectionID - Connection ID
   BOOL        fRender - If TRUE this is a render packet, FALSE its TTS
   PCCircumrealityPacket pp - Packet it came from
   PCMem       pMem - Memory contianing the data, with m_pdwCurPosn being the size
returns
   none
*/
void CMainWindow::PacketServerRequest (DWORD dwConnectionID, BOOL fRender, PCCircumrealityPacket pp, PCMem pMem)
{
   PCIRCUMREALITYPACKETSEVERREQUEST pw;
   if (pMem->m_dwCurPosn < sizeof(*pw))
      return;

   // make sure size ok
   pw = (PCIRCUMREALITYPACKETSEVERREQUEST) pMem->p;
   if ((pw->dwStringSize % sizeof(WCHAR)) || !pw->dwStringSize ||
      (sizeof(*pw) + pw->dwStringSize > pMem->m_dwCurPosn))
      return;

   // make sure string ends in NULL
   PWSTR pszString = (PWSTR)(pw + 1);
   if (pszString[pw->dwStringSize/sizeof(WCHAR)-1])
      return;

   // send this on
   if (m_pIDT)
      m_pIDT->EntryAdd (FALSE, 1, pszString, pw->dwQualityMin, pw->dwQualityMax,
         fRender ? IMAGEDATABASETYPE_IMAGE : IMAGEDATABASETYPE_WAVE,
         dwConnectionID,
         NULL, 0);
   
   // done
}

/*************************************************************************************
CMainWindow::PacketLogOff - Called when a logoff command packet comes in

inputs
   PCCircumrealityPacket pp - Packet it came from
   PCMem       pMem - Memory contianing the data, with m_pdwCurPosn being the size
returns
   none
*/
void CMainWindow::PacketLogOff (PCCircumrealityPacket pp, PCMem pMem)
{
   // verify that can send message
   if (m_dwConnectionLogOff == -1)
      return;

   CIRCUMREALITYPACKETINFO info;
   pp->InfoGet (&info);

   if (!m_pVM || !m_pVM->ObjectFind (&info.gObject))
      return;

   // call it
   PCMIFLVarList pl = new CMIFLVarList;
   if (!pl)
      return;

   // pass in one parameter, which is undefined
   CMIFLVarLValue var;
   var.m_Var.SetUndefined ();
   pl->Add (&var.m_Var, TRUE);

   // call
   CPUMonitor(NULL); // for accurate CPU monitoring
   m_pVM->MethodCall (&info.gObject, m_dwConnectionLogOff, pl, 0, 0, &var);
      // note: Ignoring dwRet
   CPUMonitor(NULL); // for accurate CPU monitoring
   pl->Release();
}


/*************************************************************************************
CMainWindow::VMGet - Gets the VM currently being used by the window
*/
PCMIFLVM CMainWindow::VMGet (void)
{
   return m_pVM;
}


/*************************************************************************************
CMainWindow::VMSet - Tells the main window which VM to use when making function
calls. While the VM is remembered, it is NOT deleted when then main window shuts
down.

NOTE: For a server to list it will need to set a VM.

inputs
   PCMIFLVM       pVM - VM to use. This can be NULL

returns
   BOOL - TRUE if success, FALSE if error (with creating server window)
*/
BOOL CMainWindow::VMSet (PCMIFLVM pVM)
{
   // if there's an existing VM then shut down the connections for the server

   // stop the thread first
   if (m_pIDT)
      m_pIDT->ThreadStop ();

   if (m_pIT) {
      if (m_pIT)
         delete m_pIT;
      m_pIT = NULL;
   }

   if (m_pIDT) {
      delete m_pIDT;
      m_pIDT = NULL;
   }

   if (m_pEmailThread) {
      delete m_pEmailThread;
      m_pEmailThread = NULL;
   }
   if (m_pTextLog) {
      delete m_pTextLog;
      m_pTextLog = NULL;
   }
   if (m_pDatabase) {
      delete m_pDatabase;
      m_pDatabase = NULL;
   }
   if (m_pInstance) {
      delete m_pInstance;
      m_pInstance = NULL;
   }
   if (m_pmfBinaryData) {
      delete m_pmfBinaryData;
      m_pmfBinaryData = NULL;
   }


   // NOTE: Dont need to free up help since doesnt change with each VM

   m_pVM = pVM;

   if (m_pVM) {
      // reload in the binary data
      BinaryDataMega ();

      // create the internet data thread
      m_pIDT = new CImageDatabaseThread;
      if (!m_pIDT) {
         m_pVM = NULL;
         return FALSE;
      }

      // try creating new connection
      m_pIT = new CInternetThread;
      if (!m_pIT) {
         delete m_pIDT;
         m_pVM = NULL;
         return FALSE;
      }

      // start up the image database thread
      WCHAR szImageDatabaseDir[512];
      wcscpy (szImageDatabaseDir, m_szDatabaseDir);
      if (szImageDatabaseDir[wcslen(szImageDatabaseDir)-1] == L'\\')
         szImageDatabaseDir[wcslen(szImageDatabaseDir)-1] = 0; // remove backslasyh
      CreateDirectoryW (szImageDatabaseDir, NULL); // to make sure it exists
      wcscat (szImageDatabaseDir, L"\\RenderCache");

      FILETIME ftCreate, ftWrite, ftRead;
      HANDLE hFile;
      memset (&ftWrite, 0, sizeof(ftWrite));
      hFile = CreateFileW (m_szConnectFile, FILE_READ_ATTRIBUTES,
         FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING,
         0, NULL);
      if (hFile != INVALID_HANDLE_VALUE) {
         GetFileTime (hFile, &ftCreate, &ftRead, &ftWrite);
         CloseHandle (hFile);
      }
      __int64 iFileTime = *((__int64*)&ftWrite);

#if 0 // hack so that can test the cache even though rebuilding all the time
      iFileTime = 1;
#endif

      if (!m_pIDT->ThreadStart (szImageDatabaseDir, iFileTime, m_pIT)) {
         delete m_pIT;
         delete m_pIDT;
         m_pIT = NULL;
         m_pVM = NULL;
         return FALSE;
      }

#if 0 // hack to test
      srand(GetTickCount());
      PWSTR apszFile[] = {L"FileA", L"RandomFileB", L"CFile", L"TheDFile"};
      PWSTR apszData[] = {L"Some data from A", L"B data", L"Random C data", L"D data"};
      DWORD i;
      for (i = 0; i < 10; i++) {
         PWSTR pszFile = apszFile[rand() % (sizeof(apszFile) / sizeof(apszFile[0]))];
         PWSTR pszData = apszData[rand() % (sizeof(apszData) / sizeof(apszData[0]))];
         m_pIDT->EntryAdd (FALSE, pszFile, rand() % 10, 2, 111, pszData, (wcslen(pszData)+1)*sizeof(WCHAR));
      } // i
#endif

      int iWinSockErr = 0;
      if (!m_pIT->Connect (m_szConnectFile, m_fConnectRemote, m_dwConnectPort, m_hWnd, WM_USER+124, &iWinSockErr)) {
         delete m_pIDT;
         delete m_pIT;
         m_pIDT = NULL;
         m_pIT = NULL;
         m_pVM = NULL;

         WCHAR szTemp[512];
         WCHAR szName[256];
         MultiByteToWideChar (CP_ACP, 0, gszAppPath, -1, szName, sizeof(szName)/sizeof(WCHAR));
         swprintf (szTemp,
            L"You may not have connected to the Internet, or your firewall might be "
            L"blocking; make sure that the application, %s, is not blocked.", szName);

         EscMessageBox (m_hWnd, L"Circumreality World Simulator", L"The Internet connection failed.", szTemp, MB_OK);

         return FALSE;
      }

      // set the text to show off the internet address
      char szTemp[512];
      MainWindowTitle (m_szConnectFile, szTemp, sizeof(szTemp));
      if (m_fConnectRemote) {
         strcat (szTemp, " (IP addr:");
#ifdef USEIPV6
         sockaddr_in6 si;
         memset (&si, 0, sizeof(si));
         memcpy (&si.sin6_addr, &m_pIT->m_qwConnectIP, sizeof(si.sin6_addr));
         char szHostIP[256];
         szHostIP[0] = 0;
         char *pszAddr = szHostIP;
         si.sin6_family = AF_INET6;
         getnameinfo ((const struct sockaddr FAR*) &si, sizeof(si), szHostIP, sizeof(szHostIP) / sizeof(szHostIP[0]), NULL, 0, NI_NUMERICHOST);
            // do this to store away IP address
         int iRet = WSAGetLastError ();

         if (pszAddr)
            strcat (szTemp, pszAddr);

         // if it's a questionable address then alter user
         IP6ADDRHACK aZero;
         memset (&aZero, 0, sizeof(aZero));
         if (!memcmp(&aZero, &m_pIT->m_qwConnectIP, sizeof(aZero)) || !memcmp(&m_pIT->m_qwConnectIP, &in6addr_loopback, sizeof(in6addr_loopback)) )
            strcat (szTemp, " BAD IP: Is your Internet on?");
                  // BUGFIX - Don't hard-code
#else
         in_addr ia;
         ia.S_un.S_addr = m_pIT->m_qwConnectIP;
         char *pszAddr = inet_ntoa (ia);

         if (pszAddr)
            strcat (szTemp, pszAddr);

         // if it's a questionable address then alter user
         if (!m_pIT->m_qwConnectIP || (m_pIT->m_qwConnectIP == INADDR_LOOPBACK /* inet_addr("127.0.0.1") */ ))
            strcat (szTemp, " BAD IP: Is your Internet on?");
                  // BUGFIX - Don't hard-code
#endif
         strcat (szTemp, ")");
      }
      SetWindowText (m_hWnd, szTemp);

      // Get functions and method names for things to
      // call when:
      m_dwConnectionNew = m_pVM->FunctionNameToID (L"connectionnew");
      m_dwConnectionMessage = m_pVM->MethodNameToID (L"connectionmessage");
      m_dwConnectionInfoForServer = m_pVM->MethodNameToID (L"connectioninfoforserver");
      m_dwConnectionUploadImage = m_pVM->MethodNameToID (L"connectionuploadimage");
      m_dwConnectionVoiceChat = m_pVM->MethodNameToID (L"connectionvoicechat");
      m_dwConnectionError = m_pVM->MethodNameToID (L"connectionerror");
      m_dwConnectionLogOff = m_pVM->MethodNameToID (L"connectionlogoff");

      // log
      m_pTextLog = new CTextLog;
      if (m_pTextLog) {
         if (!m_pTextLog->Init (m_szDatabaseDir)) {
            delete m_pTextLog;
            m_pTextLog = NULL;
         }
      }
      // create the database
      m_pDatabase = new CDatabase;
      if (m_pDatabase) {
         if (!m_pDatabase->Init (m_szDatabaseDir, m_pVM)) {
            delete m_pDatabase;
            m_pDatabase =NULL;
         }
      }

      // create the instance
      m_pInstance = new CInstance;
      if (m_pInstance) {
         if (!m_pInstance->Init (m_szDatabaseDir)) {
            delete m_pInstance;
            m_pInstance = NULL;
         }
      }

      // email thread
      m_pEmailThread = new CEmailThread;


   }

   // clear out the parser
   while (m_NLP.ParserNum())
      m_NLP.ParserRemove (0);

   // update help... this will really only be called the first time a VM is set
   HelpConstruct ();


   return TRUE;
}



/*************************************************************************************
CMainWindow::BinaryDataMega - Gets the binary data megafile
*/
PCMegaFile CMainWindow::BinaryDataMega (void)
{
   if (m_pmfBinaryData)
      return m_pmfBinaryData;

   // create the directory
   // create the directory if it doesn't already exist
   WCHAR szwTemp[512];
   wcscpy (szwTemp, m_szDatabaseDir);
   DWORD dwLen = (DWORD)wcslen(szwTemp);
   if (dwLen && (szwTemp[dwLen-1] == '\\'))
      szwTemp[dwLen-1] = 0;
   CreateDirectoryW (szwTemp, NULL);

   // append the name
   wcscat (szwTemp, L"\\BinaryData.msg");

   // create the megafile
   m_pmfBinaryData = new CMegaFile;
   if (!m_pmfBinaryData)
      return NULL;
   if (!m_pmfBinaryData->Init (szwTemp, &GUID_BinaryData)) {
      delete m_pmfBinaryData;
      m_pmfBinaryData = NULL;
      return NULL;
   }
   m_pmfBinaryData->m_fDontUpdateLastAccess = TRUE; // faster access
   m_pmfBinaryData->m_fAssumeNotShared = TRUE;  // faster access
   return m_pmfBinaryData;
}

/*************************************************************************************
CMainWindow::HelpConstruct - Constructs all the online help bits if they're not
already.
*/
BOOL CMainWindow::HelpConstruct (void)
{
   // if already constructed dont bother
   if (m_lPCOnlineHelp.Num() || !m_pVM)
      return TRUE;

   // create the directory if it doesn't already exist
   WCHAR szwTemp[512];
   wcscpy (szwTemp, m_szDatabaseDir);
   DWORD dwLen = (DWORD)wcslen(szwTemp);
   if (dwLen && (szwTemp[dwLen-1] == '\\'))
      szwTemp[dwLen-1] = 0;
   CreateDirectoryW (szwTemp, NULL);

   // find all the languages
   DWORD i;
   LANGID *plid = (LANGID*)gpMIFLProj->m_lLANGID.Get(0);
   for (i = 0; i < gpMIFLProj->m_lLANGID.Num(); i++) {
      PCOnlineHelp pHelp = new COnlineHelp;
      if (!pHelp)
         return FALSE;

      if (!pHelp->Init (m_pVM, plid[i], m_szDatabaseDir, m_hWnd)) {
         delete pHelp;
         return FALSE;
      }

      m_lPCOnlineHelp.Add (&pHelp);
   }

   return TRUE;
}


/*************************************************************************************
CMainWindow::HelpGetPCOnlineHelp - Gets the appropriate help for the current language.
*/
PCOnlineHelp CMainWindow::HelpGetPCOnlineHelp (void)
{
   if (!m_pVM)
      return NULL;
   HelpConstruct();  // just incase

   DWORD dwIndex = MIFLLangMatch (&gpMIFLProj->m_lLANGID, m_pVM->m_LangID, FALSE);
   if (dwIndex >= m_lPCOnlineHelp.Num())
      return NULL;   // shouldnt happen
   
   return *((PCOnlineHelp*)m_lPCOnlineHelp.Get(dwIndex));
}


/*************************************************************************************
CMainWindow::HelpGetArticle - Gets a help article based on an exact copy of the
article's name.

inputs
   PWSTR          pszName - Name looking for
   PCListVariable plBooks - List of names for acceptable books.
   PCResHelp      *ppHelp - Filled in the with help pointer
   PCMIFLVar      pActor - Actor to test against
   PCMIFLVM       pVM - Virtual machine to use
returns
   PWSTR - Text version of MML to send. This pointer will be valid for awhile,
            but dont modify contents or delete
*/
PWSTR CMainWindow::HelpGetArticle (PWSTR pszName, PCListVariable plBooks, PCResHelp *ppHelp,
                                   PCMIFLVar pActor, PCMIFLVM pVM)
{
   *ppHelp = NULL;

   PCOnlineHelp pHelp = HelpGetPCOnlineHelp();
   if (!pHelp)
      return NULL;

   // find the topic
   PCResHelp* pph = (PCResHelp*)pHelp->m_tTitles.Find (pszName);
   if (!pph)
      return NULL;   // cant find

   // make sure it's from an acceptable book
   DWORD i;
   for (i = 0; i < plBooks->Num(); i++)
      if (!_wcsicmp((PWSTR)pph[0]->m_memBook.p, (PWSTR)plBooks->Get(i)))
         break;
   if (i >= plBooks->Num())
      return NULL;

   // will need to call the function to see if visible
   if (!HelpCanUserSee (pActor, pVM, pph[0]))
      return NULL;

   // else, get from resoruce
   *ppHelp = pph[0];
   return pph[0]->ResourceGet();
}


/*************************************************************************************
CMainWindow::HelpVerifyCatHasVisibleBooks - This verifies that contents (or sub-contents)
of the directory have visible books. Returns TRUE if even one of them does, FALSE
if none of them do.

inputs
   POHCATEGORY          pCat - Category to check
   PCListVariable plBooks - List of names for acceptable books.
returns
   BOOL - TRUE if found any
*/
BOOL CMainWindow::HelpVerifyCatHasVisibleBooks (POHCATEGORY pCat, PCListVariable plBooks)
{
   // go through the books in this level
   DWORD i, j;
   if (pCat->plPCResHelp) {
      PCResHelp *pph = (PCResHelp*)pCat->plPCResHelp->Get(0);
      for (i = 0; i < pCat->plPCResHelp->Num(); i++) {
         PCResHelp ph = pph[i];

         for (j = 0; j < plBooks->Num(); j++)
            if (!_wcsicmp((PWSTR)ph->m_memBook.p, (PWSTR)plBooks->Get(j)))
               break;
         if (j < plBooks->Num())
            return TRUE;
      } // i
   } // if help

   // else, if get here must look through sub-trees
   if (pCat->ptOHCATEGORY) for (i = 0; i < pCat->ptOHCATEGORY->Num(); i++) {
      POHCATEGORY pSub = (POHCATEGORY)pCat->ptOHCATEGORY->GetNum(i);

      if (HelpVerifyCatHasVisibleBooks (pSub, plBooks))
         return TRUE;
   }

   // NOTE; Intentionally NOT calling the function to see if its visible
   // since would slow down the system too much

   // else, no match
   return FALSE;
}


/*************************************************************************************
CMainWindow::HelpContents - Given a contents path like "main/sub item/below", this
enumerates all the contents of the path.

inputs
   PWSTR          pszPath - Path to look at, like "main/sub item/below". This
                  can be an empty string to view the root path
   PCListVariable plBooks - List of names for acceptable books.
   PCListFixed    plPCResHelp - Filled with a list of PCResHelp info containing
                  books that match the plBooks critera
   PCListVariable plSubDir - Filled with a list of sub-directory names (excluding
                  the parents) for directories that match then search critera.
   PCMIFLVar      pActor - Actor to test against
   PCMIFLVM       pVM - Virtual machine to use
returns
   BOOL - TRUE if success
*/
BOOL CMainWindow::HelpContents (PWSTR pszPath, PCListVariable plBooks,
                                PCListFixed plPCResHelp, PCListVariable plSubDir,
                                PCMIFLVar pActor, PCMIFLVM pVM)
{
   // wipe out lists
   plPCResHelp->Init (sizeof(PCResHelp));
   plSubDir->Clear();

   PCOnlineHelp pHelp = HelpGetPCOnlineHelp();
   if (!pHelp)
      return FALSE;

   // walk down the path
   POHCATEGORY pCat = &pHelp->m_OHCATEGORY;
   while (pszPath[0]) {
      PWSTR pszSlash = wcschr(pszPath, L'/');
      if (!pszSlash)
         pszSlash = pszPath + wcslen(pszPath);
      WCHAR cCur = pszSlash[0];
      pszSlash[0] = 0;

      // find the sub-category
      if (pCat->ptOHCATEGORY)
         pCat = (POHCATEGORY)pCat->ptOHCATEGORY->Find (pszPath);
      else
         pCat = NULL;

      pszSlash[0] = cCur;  // to restore
      pszPath = cCur ? (pszSlash+1) : pszSlash;

      if (!pCat)
         return FALSE;  // cant find
   } // while pszPath

   // look through all the articles and make sure can see
   DWORD i, j;
   if (pCat->plPCResHelp) {
      PCResHelp *pph = (PCResHelp*)pCat->plPCResHelp->Get(0);
      for (i = 0; i < pCat->plPCResHelp->Num(); i++) {
         PCResHelp ph = pph[i];

         for (j = 0; j < plBooks->Num(); j++)
            if (!_wcsicmp((PWSTR)ph->m_memBook.p, (PWSTR)plBooks->Get(j)))
               break;
         if (j >= plBooks->Num())
            continue; // not acceptable

         // need to verify that can call this one
         if (!HelpCanUserSee (pActor, pVM, ph))
            continue;

         // else, add
         plPCResHelp->Add (&ph);
      } // i
   } // if help

   // go through all the subcategories
   if (pCat->ptOHCATEGORY) for (i = 0; i < pCat->ptOHCATEGORY->Num(); i++) {
      POHCATEGORY pSub = (POHCATEGORY)pCat->ptOHCATEGORY->GetNum(i);

      if (!HelpVerifyCatHasVisibleBooks (pSub, plBooks))
         continue;   // nothing in here works

      // else, add
      PWSTR psz = pCat->ptOHCATEGORY->Enum (i);
      plSubDir->Add (psz, (wcslen(psz)+1)*sizeof(WCHAR));
   }
   
   // return TRUE if any of the items are set
   return (plPCResHelp->Num() || plSubDir->Num());
}


/*************************************************************************************
CMainWindow::HelpSearch - Search for a number of keywords.

inputs
   PWSTR          pszKeywords - List of keywords separated by spaced
   PCListVariable plBooks - List of names for acceptable books.
   PCListFixed    plPCResHelp - Filled with the help pages that results from the search
   PCListFixed    plScore - Filled with a DWORD score for each search result
   PCMIFLVar      pActor - Actor to test against
   PCMIFLVM       pVM - Virtual machine to use
returns
   BOOL - TRUE if any search results were found, FALSE if error
*/
BOOL CMainWindow::HelpSearch (PWSTR pszKeywords, PCListVariable plBooks,
                              PCListFixed plPCResHelp, PCListFixed plScore,
                              PCMIFLVar pActor, PCMIFLVM pVM)
{
   // clear out results
   plPCResHelp->Init (sizeof(PCResHelp));
   plScore->Init (sizeof(DWORD));

   // get the help
   PCOnlineHelp pHelp = HelpGetPCOnlineHelp();
   if (!pHelp)
      return FALSE;

   // search it
   pHelp->m_Search.Search (pszKeywords);

   DWORD i, j;
   PWSTR psz;
   for (i = 0; i < pHelp->m_Search.m_listFound.Num(); i++) {
      psz = (PWSTR) pHelp->m_Search.m_listFound.Get(i);
      if (!psz)
         continue;

      // the search info is packed, so unpack and convert so
      // none of the characters interfere with MML

      // scorew
      DWORD dwScore;
      dwScore = *((DWORD*) psz);
      psz += (sizeof(DWORD)/sizeof(WCHAR));

      // document title, skipped
      psz += (wcslen(psz)+1);

      // section title, skipped
      psz += (wcslen(psz)+1);

      // link
      if ((psz[0] != L's') || (psz[1] != L':'))
         continue;   // shouldnt happen
      DWORD dwIndex = _wtoi(psz + 2);
      PCResHelp *pph = (PCResHelp*)pHelp->m_tTitles.GetNum(dwIndex);
      if (!pph)
         continue;   // shouldnt happen
      PCResHelp ph = pph[0];

      // verify that right book
      for (j = 0; j < plBooks->Num(); j++)
         if (!_wcsicmp((PWSTR)ph->m_memBook.p, (PWSTR)plBooks->Get(j)))
            break;
      if (j >= plBooks->Num())
         continue; // not acceptable

      // need to verify that viewer can see
      if (!HelpCanUserSee (pActor, pVM, ph))
         continue;

      // if get here, add
      plPCResHelp->Add (&ph);
      plScore->Add (&dwScore);
   } // i

   // done
   return (plPCResHelp->Num() ? TRUE : FALSE);
}


/*************************************************************************************
CMainWindow::HelpCanUserSee - Function call to see if the user can see the help.

inputs
   PCMIFLVar         pActor - Actor variable
   PCMIFLVM          pVM - Virtual machine
   PCResHelp         pHelp - Help to test
returns
   BOOL - TRUE if it can be seen
*/
BOOL CMainWindow::HelpCanUserSee (PCMIFLVar pActor, PCMIFLVM pVM, PCResHelp pHelp)
{
   PWSTR pszFunction = (PWSTR)pHelp->m_memFunction.p;
   if (!pszFunction[0] || !pVM)
      return TRUE;   // no function, so dont bother

   // else, try to find
   DWORD dwID = pVM->FunctionNameToID (pszFunction);
   if (dwID == -1)
      return FALSE;   // function not found, should really cause an error

   // call it
   PCMIFLVarList pl = new CMIFLVarList;
   if (!pl)
      return FALSE;

   pl->Add (pActor, TRUE);

   CMIFLVarLValue var;
   var.m_Var.SetString ((PWSTR)pHelp->m_memFunctionParam.p);
   pl->Add (&var.m_Var, TRUE);

   DWORD dwRet;
   CPUMonitor(NULL); // for accurate CPU monitoring
   dwRet = m_pVM->FunctionCall (dwID, pl, &var);
      // note: Ignoring dwRet
   CPUMonitor(NULL); // for accurate CPU monitoring
   pl->Release();

   return var.m_Var.GetBOOL(pVM);
}


/*************************************************************************************
CMainWindow::CPUMonitorTrim - This method clears out entries in the CPU monitor
that don't occur within 60 seconds of the current time. It assumes that
newer entries are added to the END of the list, so they're sorted (increasing) by
time.

inputs
   __int64        iTime - Current time
*/
void CMainWindow::CPUMonitorTrim (__int64 iTime)
{
   __int64 iOld = iTime - m_iPerCount60Sec;

   PCPUMONITOR pm = (PCPUMONITOR) m_lCPUMONITOR.Get(0);
   while (m_lCPUMONITOR.Num() && (pm->iTimeStart < iOld)) {
      m_lCPUMONITOR.Remove (0);
      pm = (PCPUMONITOR) m_lCPUMONITOR.Get(0);
   }

   // if there are any that are too new then remove those too
   DWORD dwNum;
   while ((dwNum = m_lCPUMONITOR.Num()) && (pm[dwNum-1].iTimeStart > iTime)) {
      // shouldnt actually happen
      m_lCPUMONITOR.Remove (dwNum-1);
      pm = (PCPUMONITOR) m_lCPUMONITOR.Get(0);
   }
}


/*************************************************************************************
CMainWindow::CPUMonitor - Call this to start monitoring the CPU used by
a specific objects

inputs
   GUID        *pgObject - Object that's started. Use NULL if no longer monitoring
               an object.
returns
   none
*/
void CMainWindow::CPUMonitor (GUID *pgObject)
{
   // if this is a NULL object, and already on a NULL object then return
   // likewise, if it's the same object then do nothing
   if (pgObject) {
      // if same object then do nothing
      if (!m_fCPUObjectNULL && IsEqualGUID(*pgObject, m_gCPUObject))
         return;
   }
   else {
      // NULL object
      if (m_fCPUObjectNULL)
         return;
   }

   // get the current time
   LARGE_INTEGER iCur;
   __int64 iTime;
   QueryPerformanceCounter (&iCur);
   iTime = *((__int64*)&iCur);

   // get the CPU
   FILETIME ftCreate, ftExit, ftKernel, ftUser;
   GetThreadTimes (GetCurrentThread(), &ftCreate, &ftExit, &ftKernel, &ftUser);
   __int64 iTimeThread = *((__int64*)&ftKernel) + *((__int64*)&ftUser);

   // trim the list down
   CPUMonitorTrim (iTime);

   // potentially add the old one to the list
   if (!m_fCPUObjectNULL) {
      CPUMONITOR cm;
      cm.gObject = m_gCPUObject;
      cm.iTimeStart = m_iPerCountLast;
      cm.iTimeElapsed = (iTime - cm.iTimeStart) / (m_iPerCountFreq / 1000);
         // in milliseconds
      cm.iTimeThread = iTimeThread - m_iCPUThreadTimeStart;
      m_lCPUMONITOR.Add (&cm);
   }

   // set this one
   if (pgObject) {
      m_fCPUObjectNULL = FALSE;
      m_iPerCountLast = iTime;
      m_iCPUThreadTimeStart = iTimeThread;
      m_gCPUObject = *pgObject;
   }
   else {
      // just remember that NULL object
      m_fCPUObjectNULL = TRUE;
      m_iPerCountLast = 0; // just to clear
   }
}


/*************************************************************************************
CMainWindow::CPUMonitorUsed - Returns the percentage of the main thread that was
used.

inputs
   GUID        *pgObject - Object that looking for
returns
   double - Time used, in percent, 0..1
*/
double CMainWindow::CPUMonitorUsed (GUID *pgObject)
{
   // trim down
   LARGE_INTEGER iCur;
   __int64 iTime;
   QueryPerformanceCounter (&iCur);
   iTime = *((__int64*)&iCur);
   CPUMonitorTrim(iTime);

   __int64 iTotal = 0;
   PCPUMONITOR pcm = (PCPUMONITOR)m_lCPUMONITOR.Get(0);
   DWORD i;
   for (i = 0; i < m_lCPUMONITOR.Num(); i++, pcm++) {
      if (IsEqualGUID(pcm->gObject, *pgObject))
         iTotal += pcm->iTimeThread;
   } // i

   // divide this by 60 seconds
   iTotal /= (m_iPerCount60Sec / m_iPerCountFreq);

   // therefore, have percent * 10million, since filetime = 100 nanosec
   return (double)iTotal / 10000000.0;
}


// BUGBUG - eventually show graph of number of users on (and by paying customers)
// BUGBUG - eventually show graph of data IO of app, and total data IO
// BUGBUG - eventually show whether user is paying customer or not
// BUGBUG - eventually keep log of crashes
// BUGBUG - eventually have a "ShutDownAndRestart" command that can be used
// to restart the service once in awhile
// BUGBUG - eventually keep track of swear words appearing in chat
// BUGBUG - eventually keep log of new user names or other entities looking for offensive langauge
// BUGBUG - eventually have a blacklist of IP addresses? user names? etc.
// BUGBUG - eventually historgram of connection speeds used by users
// BUGBUG - eventually want to click on database item and delete it or something else
// BUGBUG - eventually may limit number of users and kick off (or refuse to log on) non-paying users

// BUGBUG - eventually graph number of threads
// BUGBUG - eventually graph number of handles

// BUGBUG - may eventually want UI in the project editor for a "always make sure transmitted"
// list that will force the files to be transmitted to the host, even if not immediately
// used

// BUGBUG - At one point got a leak of what looked like CNLPParse() stuff with
// `out and `north and `in in lost memory strings. Cant repro

// BUGBUG - this will need to be modified so can run from a command line on a
// remote server => no message boxes


// BUGBUG - When demoing encountered a problem where created a new project, and a new
// library within the project, and new object within the library. However, couldnt
// actually switch to displaying the library's object, for some reason. I can't seem
// to reproduce
