/*************************************************************************************
CResTitle.cpp - Code for handling the resource title.

begun 2/9/04 by Mike Rozak.
Copyright 2004 by Mike Rozak. All rights reserved
*/

#include <windows.h>
#include <objbase.h>
#include "escarpment.h"
#include "..\..\m3d\M3D.h"
#include "..\..\m3d\mifl.h"
#include "..\mif.h"
#include "..\buildnum.h"
#include "resource.h"




/****************************************************************************
PCResTitleInfoSetFromFile - Reads in the info from the file.

inputs
   PWSTR          pszFile - File to read in
   BOOL           *pfCircumreality - Will set this to TRUE if it's a CRF file, FALSE if
                     it's a CRK file.
returns
   PCResTitleInfoSet - Info that must be freed by caller, or NULL if error
*/
PCResTitleInfoSet PCResTitleInfoSetFromFile (PWSTR pszFile, BOOL *pfCircumreality)
{
   // what type of megafile
   DWORD dwLen = (DWORD)wcslen(pszFile);
   if (dwLen < 4)
      return NULL;
   BOOL fCircumreality = !_wcsicmp(pszFile + (dwLen-4), L".crf");

   CMegaFile Mega;
   if (!Mega.Init (pszFile, fCircumreality ? &GUID_MegaCircumreality : &GUID_MegaCircumrealityLink, FALSE))
      return NULL;

   PCMMLNode2 pNode = MMLFileOpen (&Mega, CircumrealityTitleInfo(), &CLSID_MegaTitleInfo);
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

   if (pfCircumreality)
      *pfCircumreality = fCircumreality;

   delete pNode;
   return pNew;
}

/*************************************************************************************
CResTitleInfoShard::Constructor and destructor
*/
CResTitleInfoShard::CResTitleInfoShard (void)
{
   m_dwPort = 4000;
   MemZero (&m_memName);
   MemZero (&m_memDescShort);
   MemZero (&m_memAddr);
   MemZero (&m_memParam);
}

CResTitleInfoShard::~CResTitleInfoShard (void)
{
   // do nothing for now
}



/*************************************************************************************
CResTitleInfoShard::Clone
*/
CResTitleInfoShard *CResTitleInfoShard::Clone (void)
{
   PCResTitleInfoShard pNew = new CResTitleInfoShard;
   if (!pNew)
      return NULL;

   pNew->m_dwPort = m_dwPort;
   MemZero (&pNew->m_memDescShort);
   MemCat (&pNew->m_memDescShort, (PWSTR)m_memDescShort.p);
   MemZero (&pNew->m_memName);
   MemCat (&pNew->m_memName, (PWSTR)m_memName.p);
   MemZero (&pNew->m_memAddr);
   MemCat (&pNew->m_memAddr, (PWSTR)m_memAddr.p);
   MemZero (&pNew->m_memParam);
   MemCat (&pNew->m_memParam, (PWSTR)m_memParam.p);

   return pNew;

}


static PWSTR gpszAddr = L"Addr";
static PWSTR gpszDescShort = L"DescShort";
static PWSTR gpszName = L"Name";
static PWSTR gpszParam = L"Param";
static PWSTR gpszPort = L"Port";
static PWSTR gpszShard = L"Shard";
static PWSTR gpszJPEGBack = L"JPEGBack";

/*************************************************************************************
CResTitleInfoShard::MMLTo - Standard API
*/
PCMMLNode2 CResTitleInfoShard::MMLTo (void)
{
   PCMMLNode2 pNode = new CMMLNode2;
   if (!pNode)
      return NULL;
   pNode->NameSet (gpszShard);

   MMLValueSet (pNode, gpszPort, (int)m_dwPort);

   PWSTR psz;

   psz = (PWSTR)m_memName.p;
   if (psz && psz[0])
      MMLValueSet (pNode, gpszName, psz);

   psz = (PWSTR)m_memDescShort.p;
   if (psz && psz[0])
      MMLValueSet (pNode, gpszDescShort, psz);

   psz = (PWSTR)m_memAddr.p;
   if (psz && psz[0])
      MMLValueSet (pNode, gpszAddr, psz);

   psz = (PWSTR)m_memParam.p;
   if (psz && psz[0])
      MMLValueSet (pNode, gpszParam, psz);

   return pNode;
}



/*************************************************************************************
CResTitleInfoShard::MMLFrom - Standard API
*/
BOOL CResTitleInfoShard::MMLFrom (PCMMLNode2 pNode)
{
   // clear out
   MemZero (&m_memName);
   MemZero (&m_memDescShort);
   MemZero (&m_memAddr);
   MemZero (&m_memParam);

   m_dwPort = (int)MMLValueGetInt (pNode, gpszPort, (int)4000);

   PWSTR psz;

   psz = MMLValueGet (pNode, gpszName);
   if (psz)
      MemCat (&m_memName, psz);

   psz = MMLValueGet (pNode, gpszDescShort);
   if (psz)
      MemCat (&m_memDescShort, psz);

   psz = MMLValueGet (pNode, gpszAddr);
   if (psz)
      MemCat (&m_memAddr, psz);

   psz = MMLValueGet (pNode, gpszParam);
   if (psz)
      MemCat (&m_memParam, psz);

   return TRUE;
}


/*************************************************************************************
DFDATEToday - Returns today's date (UTC) as DFDATE.

inputs
   BOOL        fLocalTime - If TRUE use local time. FALSE then system time
returns
   DFDATE - Time
*/
DFDATE DFDATEToday (BOOL fLocalTime)
{
   SYSTEMTIME st;
   memset (&st, 0, sizeof(st));
   if (fLocalTime)
      GetLocalTime (&st);
   else
      GetSystemTime (&st);

   return TODFDATE (st.wDay, st.wMonth, st.wYear);
}

/*************************************************************************************
CResTitleInfo::Constructor and destructor
*/
CResTitleInfo::CResTitleInfo (void)
{
   m_lid = 1033;
   m_fPlayOffline = TRUE;
   m_dwDelUnusedUsers = 14;
   m_dwDateModified = DFDATEToday(FALSE);

   MemZero (&m_memName);
   MemZero (&m_memFileName);
   MemZero (&m_memDescShort);
   MemZero (&m_memDescLong);
   // MemZero (&m_memEULA);
   MemZero (&m_memWebSite);

   m_memJPEGBack.m_dwCurPosn = 0;

   m_lPCResTitleInfoShard.Init (sizeof(PCResTitleInfoShard));
}

CResTitleInfo::~CResTitleInfo (void)
{
   DWORD i;
   PCResTitleInfoShard *pps = (PCResTitleInfoShard*)m_lPCResTitleInfoShard.Get(0);
   for (i = 0; i < m_lPCResTitleInfoShard.Num(); i++)
      delete pps[i];
   m_lPCResTitleInfoShard.Clear();
}


/*************************************************************************************
CResTitleInfo::CloneTo - Standard API
*/
BOOL CResTitleInfo::CloneTo (CResTitleInfo *pTo)
{
   DWORD i;
   PCResTitleInfoShard *pps = (PCResTitleInfoShard*)pTo->m_lPCResTitleInfoShard.Get(0);
   for (i = 0; i < pTo->m_lPCResTitleInfoShard.Num(); i++)
      delete pps[i];
   pTo->m_lPCResTitleInfoShard.Clear();


   pTo->m_lid = m_lid;
   pTo->m_fPlayOffline = m_fPlayOffline;
   pTo->m_dwDelUnusedUsers = m_dwDelUnusedUsers;
   pTo->m_dwDateModified = m_dwDateModified;
   MemZero (&pTo->m_memDescLong);
   MemCat (&pTo->m_memDescLong, (PWSTR)m_memDescLong.p);
   MemZero (&pTo->m_memDescShort);
   MemCat (&pTo->m_memDescShort, (PWSTR)m_memDescShort.p);
   MemZero (&pTo->m_memName);
   MemCat (&pTo->m_memName, (PWSTR)m_memName.p);
   MemZero (&pTo->m_memFileName);
   MemCat (&pTo->m_memFileName, (PWSTR)m_memFileName.p);
   // MemZero (&pTo->m_memEULA);
   // MemCat (&pTo->m_memEULA, (PWSTR)m_memEULA.p);
   MemZero (&pTo->m_memWebSite);
   MemCat (&pTo->m_memWebSite, (PWSTR)m_memWebSite.p);

   if (!pTo->m_memJPEGBack.Required (m_memJPEGBack.m_dwCurPosn))
      return FALSE;
   memcpy (pTo->m_memJPEGBack.p, m_memJPEGBack.p, m_memJPEGBack.m_dwCurPosn);
   pTo->m_memJPEGBack.m_dwCurPosn = m_memJPEGBack.m_dwCurPosn;

   // pTo->m_lPCResTitleInfoShard will be empty
   pTo->m_lPCResTitleInfoShard.Init (sizeof(PCResTitleInfoShard), m_lPCResTitleInfoShard.Get(0), m_lPCResTitleInfoShard.Num());
   PCResTitleInfoShard *ppTIS = (PCResTitleInfoShard*)pTo->m_lPCResTitleInfoShard.Get(0);
   for (i = 0; i < pTo->m_lPCResTitleInfoShard.Num(); i++)
      ppTIS[i] = ppTIS[i]->Clone();

   return TRUE;
}

/*************************************************************************************
CResTitleInfo::Clone
*/
CResTitleInfo *CResTitleInfo::Clone (void)
{
   PCResTitleInfo pNew = new CResTitleInfo;
   if (!pNew)
      return NULL;

   if (!CloneTo (pNew)) {
      delete pNew;
      return NULL;
   }


   return pNew;

}


static PWSTR gpszDescLong = L"DescLong";
static PWSTR gpszLangID = L"LangID";
static PWSTR gpszPlayOffline = L"PlayOffline";
static PWSTR gpszWebSite = L"WebSite";
static PWSTR gpszEULA = L"EULA";
static PWSTR gpszFileName = L"FileName";
static PWSTR gpszDelUnusedUsers = L"DelUnusedUsers";
static PWSTR gpszDateModified = L"DateModified";

/*************************************************************************************
CResTitleInfo::MMLTo - Standard API
*/
PCMMLNode2 CResTitleInfo::MMLTo (void)
{
   PCMMLNode2 pNode = new CMMLNode2;
   if (!pNode)
      return NULL;
   pNode->NameSet (CircumrealityTitleInfo());

   MMLValueSet (pNode, gpszLangID, (int)m_lid);
   MMLValueSet (pNode, gpszPlayOffline, (int)m_fPlayOffline);
   MMLValueSet (pNode, gpszDelUnusedUsers, (int)m_dwDelUnusedUsers);
   MMLValueSet (pNode, gpszDateModified, (int)m_dwDateModified);

   if (m_memJPEGBack.m_dwCurPosn)
      MMLValueSet (pNode, gpszJPEGBack, (PBYTE)m_memJPEGBack.p, m_memJPEGBack.m_dwCurPosn);

   PWSTR psz;

   psz = (PWSTR)m_memName.p;
   if (psz && psz[0])
      MMLValueSet (pNode, gpszName, psz);

   psz = (PWSTR)m_memFileName.p;
   if (psz && psz[0])
      MMLValueSet (pNode, gpszFileName, psz);

   psz = (PWSTR)m_memDescShort.p;
   if (psz && psz[0])
      MMLValueSet (pNode, gpszDescShort, psz);

   psz = (PWSTR)m_memDescLong.p;
   if (psz && psz[0])
      MMLValueSet (pNode, gpszDescLong, psz);

   // psz = (PWSTR)m_memEULA.p;
   // if (psz && psz[0])
   //    MMLValueSet (pNode, gpszEULA, psz);

   psz = (PWSTR)m_memWebSite.p;
   if (psz && psz[0])
      MMLValueSet (pNode, gpszWebSite, psz);

   DWORD i;
   PCMMLNode2 pSub;
   PCResTitleInfoShard *pps = (PCResTitleInfoShard*)m_lPCResTitleInfoShard.Get(0);
   for (i = 0; i < m_lPCResTitleInfoShard.Num(); i++) {
      pSub = pps[i]->MMLTo ();
      if (pSub)
         pNode->ContentAdd (pSub);
   } // i

   return pNode;
}


/*************************************************************************************
CResTitleInfoShard::MMLFrom - Standard API
*/
BOOL CResTitleInfo::MMLFrom (PCMMLNode2 pNode)
{
   // clear out what have
   MemZero (&m_memName);
   MemZero (&m_memFileName);
   MemZero (&m_memDescShort);
   MemZero (&m_memDescLong);
   // MemZero (&m_memEULA);
   MemZero (&m_memWebSite);
   m_memJPEGBack.m_dwCurPosn = 0;

   DWORD i;
   PCResTitleInfoShard *pps = (PCResTitleInfoShard*)m_lPCResTitleInfoShard.Get(0);
   for (i = 0; i < m_lPCResTitleInfoShard.Num(); i++)
      delete pps[i];
   m_lPCResTitleInfoShard.Clear();

   // get
   m_lid = (LANGID) MMLValueGetInt (pNode, gpszLangID, (int)1033);
   m_fPlayOffline = (BOOL) MMLValueGetInt (pNode, gpszPlayOffline, (int)TRUE);
   m_dwDelUnusedUsers = (DWORD) MMLValueGetInt (pNode, gpszDelUnusedUsers, 14);
   m_dwDateModified = (DWORD) MMLValueGetInt (pNode, gpszDateModified, DFDATEToday(FALSE));

   m_memJPEGBack.m_dwCurPosn = MMLValueGetBinary (pNode, gpszJPEGBack, &m_memJPEGBack);


   PWSTR psz;

   psz = MMLValueGet (pNode, gpszName);
   if (psz)
      MemCat (&m_memName, psz);

   psz = MMLValueGet (pNode, gpszFileName);
   if (psz)
      MemCat (&m_memFileName, psz);

   psz = MMLValueGet (pNode, gpszDescShort);
   if (psz)
      MemCat (&m_memDescShort, psz);

   psz = MMLValueGet (pNode, gpszDescLong);
   if (psz)
      MemCat (&m_memDescLong, psz);

   // psz = MMLValueGet (pNode, gpszEULA);
   // if (psz)
   //   MemCat (&m_memEULA, psz);

   psz = MMLValueGet (pNode, gpszWebSite);
   if (psz)
      MemCat (&m_memWebSite, psz);


   PCMMLNode2 pSub;
   for (i = 0; i < pNode->ContentNum(); i++) {
      pSub = NULL;
      pNode->ContentEnum (i, &psz, &pSub);
      if (!pSub)
         continue;
      psz = pSub->NameGet();
      if (!psz)
         continue;

      if (!_wcsicmp(psz, gpszShard)) {
         PCResTitleInfoShard pNew = new CResTitleInfoShard;
         if (!pNew)
            continue;
         pNew->MMLFrom (pSub);
         m_lPCResTitleInfoShard.Add (&pNew);
      }
   } // i

   return TRUE;
}



/*************************************************************************
TitleInfoPage
*/
BOOL TitleInfoPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCResTitleInfo prs = (PCResTitleInfo)pPage->m_pUserData;   // node to modify

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         // scroll to right position
         if (prs->m_iVScroll > 0) {
            pPage->VScroll (prs->m_iVScroll);

            // when bring up pop-up dialog often they're scrolled wrong because
            // iVScoll was left as valeu, and they use defpage
            prs->m_iVScroll = 0;

            // BUGFIX - putting this invalidate in to hopefully fix a refresh
            // problem when add or move a task in the ProjectView page
            pPage->Invalidate();
         }

         PCEscControl pControl;
         DWORD i;
         PCResTitleInfoShard *pps = (PCResTitleInfoShard*)prs->m_lPCResTitleInfoShard.Get(0);
         switch (prs->m_dwTab) {
         case 0: // name and desc
            pControl = pPage->ControlFind(L"name");
            if (pControl)
               pControl->AttribSet (Text(), (PWSTR)prs->m_memName.p);

            pControl = pPage->ControlFind(L"descshort");
            if (pControl)
               pControl->AttribSet (Text(), (PWSTR)prs->m_memDescShort.p);

            pControl = pPage->ControlFind(L"desclong");
            if (pControl)
               pControl->AttribSet (Text(), (PWSTR)prs->m_memDescLong.p);

            pControl = pPage->ControlFind(L"website");
            if (pControl)
               pControl->AttribSet (Text(), (PWSTR)prs->m_memWebSite.p);
            break;

         //case 4: // eula
         //   pControl = pPage->ControlFind(L"eula");
         //   if (pControl)
         //      pControl->AttribSet (Text(), (PWSTR)prs->m_memEULA.p);
         //   break;

         case 1: // offline
            pControl = pPage->ControlFind (L"playoffline");
            if (pControl)
               pControl->AttribSetBOOL (Checked(), prs->m_fPlayOffline);
            break;

         case 2: // internet
            for (i = 0; i < prs->m_lPCResTitleInfoShard.Num(); i++) {
               PCResTitleInfoShard ps = pps[i];
               WCHAR szTemp[64];

               swprintf (szTemp, L"shardname%d", (int)i);
               pControl = pPage->ControlFind (szTemp);
               if (pControl)
                  pControl->AttribSet (Text(), (PWSTR)ps->m_memName.p);

               swprintf (szTemp, L"sharddesc%d", (int)i);
               pControl = pPage->ControlFind (szTemp);
               if (pControl)
                  pControl->AttribSet (Text(), (PWSTR)ps->m_memDescShort.p);

               swprintf (szTemp, L"shardaddr%d", (int)i);
               pControl = pPage->ControlFind (szTemp);
               if (pControl)
                  pControl->AttribSet (Text(), (PWSTR)ps->m_memAddr.p);

               swprintf (szTemp, L"shardparam%d", (int)i);
               pControl = pPage->ControlFind (szTemp);
               if (pControl)
                  pControl->AttribSet (Text(), (PWSTR)ps->m_memParam.p);

               swprintf (szTemp, L"shardport%d", (int)i);
               DoubleToControl (pPage, szTemp, ps->m_dwPort);

            } // i
            break;

         case 3:  // misc
            if (pControl = pPage->ControlFind (L"filename"))
               pControl->AttribSet (Text(), (PWSTR)prs->m_memFileName.p);

            DoubleToControl (pPage, L"delunusedusers", prs->m_dwDelUnusedUsers);

            break;

         } // switch
      }
      break;

   case ESCN_EDITCHANGE:
      {
         PESCNEDITCHANGE p = (PESCNEDITCHANGE)pParam;
         PWSTR psz = p->pControl->m_pszName;
         if (!psz)
            break;

         (*prs->m_pfChanged) = TRUE;
         prs->m_dwDateModified = DFDATEToday(FALSE);

         // get the length
         DWORD dwNeed = 0;
         p->pControl->AttribGet (Text(), NULL, 0, &dwNeed);

         PCResTitleInfoShard *pps = (PCResTitleInfoShard*)prs->m_lPCResTitleInfoShard.Get(0);
         PWSTR pszShardName = L"shardname", pszShardDesc = L"sharddesc", pszShardAddr = L"shardaddr",
            pszShardParam = L"shardparam", pszShardPort = L"shardport";
         DWORD dwShardNameLen = (DWORD)wcslen(pszShardName), dwShardDescLen = (DWORD)wcslen(pszShardDesc),
            dwShardAddrLen = (DWORD)wcslen(pszShardAddr), dwShardParamLen = (DWORD)wcslen(pszShardParam),
            dwShardPortLen = (DWORD)wcslen(pszShardPort);

         PCMem pMem = NULL;
         if (!_wcsicmp(psz, L"name"))
            pMem = &prs->m_memName;
         else if (!_wcsicmp(psz, L"filename"))
            pMem = &prs->m_memFileName;
         else if (!_wcsicmp(psz, L"descshort"))
            pMem = &prs->m_memDescShort;
         else if (!_wcsicmp(psz, L"desclong"))
            pMem = &prs->m_memDescLong;
         else if (!_wcsicmp(psz, L"website"))
            pMem = &prs->m_memWebSite;
         // else if (!_wcsicmp(psz, L"eula"))
         //   pMem = &prs->m_memEULA;
         else if (!wcsncmp(psz, pszShardName, dwShardNameLen))
            pMem = &pps[_wtoi(psz + dwShardNameLen)]->m_memName;
         else if (!wcsncmp(psz, pszShardDesc, dwShardDescLen))
            pMem = &pps[_wtoi(psz + dwShardDescLen)]->m_memDescShort;
         else if (!wcsncmp(psz, pszShardAddr, dwShardAddrLen))
            pMem = &pps[_wtoi(psz + dwShardAddrLen)]->m_memAddr;
         else if (!wcsncmp(psz, pszShardParam, dwShardParamLen))
            pMem = &pps[_wtoi(psz + dwShardParamLen)]->m_memParam;
         else if (!wcsncmp(psz, pszShardPort, dwShardPortLen)) {
            pps[_wtoi(psz + dwShardNameLen)]->m_dwPort = (DWORD)DoubleFromControl (pPage, psz);
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"delunusedusers")) {
            prs->m_dwDelUnusedUsers = (DWORD) DoubleFromControl (pPage, L"delunusedusers");
            return TRUE;
         }

         if (pMem) {
            if (!pMem->Required (dwNeed))
               return TRUE;
            p->pControl->AttribGet (Text(), (PWSTR)pMem->p, (DWORD)pMem->m_dwAllocated, &dwNeed);
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

         PWSTR pszShardDel = L"sharddel";
         DWORD dwShardDelLen = (DWORD)wcslen(pszShardDel);

         if (!_wcsicmp(psz, L"playoffline")) {
            (*prs->m_pfChanged) = TRUE;
            prs->m_dwDateModified = DFDATEToday(FALSE);
            prs->m_fPlayOffline = p->pControl->AttribGetBOOL(Checked());
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"loadjpegback")) {
            // open file
            OPENFILENAME   ofn;
            char  szTemp[256];
            szTemp[0] = 0;
            memset (&ofn, 0, sizeof(ofn));
            
            // BUGFIX - Set directory
            char szInitial[256];
            strcpy (szInitial, gszAppDir);
            GetLastDirectory(szInitial, sizeof(szInitial)); // BUGFIX - get last dir
            ofn.lpstrInitialDir = szInitial;

            ofn.lStructSize = sizeof(ofn);
            ofn.hwndOwner = pPage->m_pWindow->m_hWnd;
            ofn.hInstance = ghInstance;
            ofn.lpstrFilter = "Image files (*.jpg;*.bmp)\0*.jpg;*.bmp\0\0\0";
            ofn.lpstrFile = szTemp;
            ofn.nMaxFile = sizeof(szTemp);
            ofn.lpstrTitle = "Open image file";
            ofn.Flags = (OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY);
            ofn.lpstrDefExt = ".jpg";
            // nFileExtension 

            if (!GetOpenFileName(&ofn)) {
               if (prs->m_memJPEGBack.m_dwCurPosn) {
                  if (IDYES == pPage->MBYesNo (L"You already have a .jpg image saved. Do you want to delete it?")) {
                     (*prs->m_pfChanged) = TRUE;
                     prs->m_memJPEGBack.m_dwCurPosn = 0;
                  }
               }
               return TRUE;
            }

            // load it
            HBITMAP hBmp = JPegOrBitmapLoad (ofn.lpstrFile,TRUE);
            if (!hBmp) {
               pPage->MBWarning (L"The file couldn't be opened.");
               return TRUE;
            }

            // save as jpeg
            if (!BitmapToJPeg (hBmp, &prs->m_memJPEGBack))
               prs->m_memJPEGBack.m_dwCurPosn = 0;
            DeleteObject (hBmp);


            (*prs->m_pfChanged) = TRUE;
            // NOTE - Not changing this since take date from file: prs->m_dwDateModified = DFDATEToday();

            return TRUE;
         }
         else if (!_wcsicmp(psz, L"loadfromfile")) {
            // open file
            OPENFILENAME   ofn;
            char  szTemp[256];
            szTemp[0] = 0;
            memset (&ofn, 0, sizeof(ofn));
            
            // BUGFIX - Set directory
            char szInitial[256];
            strcpy (szInitial, gszAppDir);
            GetLastDirectory(szInitial, sizeof(szInitial)); // BUGFIX - get last dir
            ofn.lpstrInitialDir = szInitial;

            ofn.lStructSize = sizeof(ofn);
            ofn.hwndOwner = pPage->m_pWindow->m_hWnd;
            ofn.hInstance = ghInstance;
            ofn.lpstrFilter = "Circumreality file (*.crk;*.crf))\0*.crk;*.crf\0\0\0";
            ofn.lpstrFile = szTemp;
            ofn.nMaxFile = sizeof(szTemp);
            ofn.lpstrTitle = "Load information from Circumreality file";
            ofn.Flags = (OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY);
            ofn.lpstrDefExt = "cfk";
            // nFileExtension 

            if (!GetOpenFileName(&ofn))
               return TRUE;

            // BUGFIX - Save diretory
            strcpy (szInitial, ofn.lpstrFile);
            szInitial[ofn.nFileOffset] = 0;
            SetLastDirectory(szInitial);

            // copy over
            WCHAR szFile[256];
            MultiByteToWideChar (CP_ACP, 0, ofn.lpstrFile, -1, szFile, sizeof(szFile)/sizeof(WCHAR));

            PCResTitleInfoSet pRTIS = PCResTitleInfoSetFromFile (szFile);
            if (!pRTIS) {
               pPage->MBWarning (L"The file didn't contain a <TitleInfo> resource.");
               return TRUE;
            }

            PCResTitleInfo pRTI = pRTIS->Find (prs->m_lid);
            if (!pRTI) {
               delete pRTIS;
               pPage->MBWarning (L"The file didn't contain the appropriate language resource.");
               return TRUE;
            }

            pRTI->CloneTo (prs);

            delete pRTIS;

            (*prs->m_pfChanged) = TRUE;
            // NOTE - Not changing this since take date from file: prs->m_dwDateModified = DFDATEToday();

            pPage->Exit (RedoSamePage());
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"newshard")) {
            (*prs->m_pfChanged) = TRUE;
            prs->m_dwDateModified = DFDATEToday(FALSE);

            PCResTitleInfoShard pNew = new CResTitleInfoShard;
            if (!pNew)
               return TRUE;
            prs->m_lPCResTitleInfoShard.Add (&pNew);

            pPage->MBInformation (L"Shard added. You should fill in the name and server.");
            pPage->Exit (RedoSamePage());
            return TRUE;
         }
         else if (!wcsncmp(psz, pszShardDel, dwShardDelLen)) {
            DWORD dwNum = _wtoi(psz + dwShardDelLen);
            if (dwNum >= prs->m_lPCResTitleInfoShard.Num())
               return TRUE;

            if (IDYES != pPage->MBYesNo (L"Are you sure you wish to delete this shard?"))
               return TRUE;

            (*prs->m_pfChanged) = TRUE;
            prs->m_dwDateModified = DFDATEToday(FALSE);

            PCResTitleInfoShard *pps = (PCResTitleInfoShard*)prs->m_lPCResTitleInfoShard.Get(0);
            delete pps[dwNum];
            prs->m_lPCResTitleInfoShard.Remove (dwNum);

            pPage->Exit (RedoSamePage());

            return TRUE;
         }
      }
      break;

   case ESCM_LINK:
      {
         PESCMLINK p = (PESCMLINK) pParam;
         if (!p->psz)
            break;

         PWSTR pszTab = L"tabpress:";
         DWORD dwLen = (DWORD)wcslen(pszTab);

         if (!wcsncmp(p->psz, pszTab, dwLen)) {
            prs->m_dwTab = (DWORD)_wtoi(p->psz + dwLen);
            pPage->Exit (RedoSamePage());
            return TRUE;
         }

      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         PWSTR pszIfTab = L"IFTAB", pszEndIfTab = L"ENDIFTAB";
         DWORD dwIfTabLen = (DWORD)wcslen(pszIfTab), dwEndIfTabLen = (DWORD)wcslen(pszEndIfTab);

         if (!wcsncmp (p->pszSubName, pszIfTab, dwIfTabLen)) {
            DWORD dwNum = _wtoi(p->pszSubName + dwIfTabLen);
            if (dwNum == prs->m_dwTab)
               p->pszSubString = L"";
            else
               p->pszSubString = L"<comment>";
            return TRUE;
         }
         else if (!wcsncmp (p->pszSubName, pszEndIfTab, dwEndIfTabLen)) {
            DWORD dwNum = _wtoi(p->pszSubName + dwEndIfTabLen);
            if (dwNum == prs->m_dwTab)
               p->pszSubString = L"";
            else
               p->pszSubString = L"</comment>";
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"RSTABS")) {
            PWSTR apsz[] = {
               L"Name",
               L"Offline",
               L"Internet",
               L"Misc"
               // L"EULA",
            };
            PWSTR apszHelp[] = {
               L"Change the name and description of the Circumreality title.",
               L"Allow the player to play offline.",
               L"Specify the internet connection.",
               L"Miscellaneous settings."
               // L"End-user license agreement.",
            };
            DWORD adwID[] = {
               0,
               1,
               2,
               3
               // 4
            };

            CListFixed lSkip;
            lSkip.Init (sizeof(DWORD));

            p->pszSubString = RenderSceneTabs (prs->m_dwTab, sizeof(apsz)/sizeof(PWSTR), apsz,
               apszHelp, adwID, lSkip.Num(), (DWORD*)lSkip.Get(0));
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Title information";
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"LIBENABLE")) {
            if (prs && prs->m_fReadOnly)
               p->pszSubString = L"enabled=false";
            else
               p->pszSubString = L"";
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"LIBREADONLY")) {
            if (prs && prs->m_fReadOnly)
               p->pszSubString = L"readonly=true";
            else
               p->pszSubString = L"";
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"SHARDLIST")) {
            MemZero (&gMemTemp);

            DWORD i;
            for (i = 0; i < prs->m_lPCResTitleInfoShard.Num(); i++) {
               MemCat (&gMemTemp,
                  L"<xtrheader>Shard ");
               MemCat (&gMemTemp, (int)i+1);
               MemCat (&gMemTemp,
                  L"</xtrheader>"
                  L"<tr><td>"
                  L"<bold>Name</bold> - Name of the shard."
                  L"</td>"
                  L"<td><bold><edit width=100% maxchars=64 ");
               if (prs->m_fReadOnly)
                  MemCat (&gMemTemp, L"readonly=true");
               MemCat (&gMemTemp,
                  L"name=shardname");
               MemCat (&gMemTemp, (int)i);
               MemCat (&gMemTemp,
                  L"/></bold></td>"
                  L"</tr>"
                  L"<tr>"
                  L"<td>"
                  L"<bold>Short description</bold> - Informs the user why this shard is different"
                  L"than any others."
                  L"</td>"
                  L"<td><bold><edit width=100% maxchars=256 multiline=true wordwrap=true ");
               if (prs->m_fReadOnly)
                  MemCat (&gMemTemp, L"readonly=true");
               MemCat (&gMemTemp,
                  L" name=sharddesc");
               MemCat (&gMemTemp, (int)i);
               MemCat (&gMemTemp,
                  L"/></bold></td>"
                  L"</tr>"
                  L"<tr>"
                  L"<td>"
                  L"<bold>Server</bold> - This is the server's domain name"
                  L"(\"<bold>www.mXac.com.au</bold>\"), an IP address (\"<bold>12.34.56.78</bold>\"),"
                  L"or a web page where the user can look to find the"
                  L"IP address (\"<bold>http://www.mXac.com.au/CurrentServer.htm</bold>\")"
                  L"</td>"
                  L"<td><bold><edit width=100% maxchars=64 ");
               if (prs->m_fReadOnly)
                  MemCat (&gMemTemp, L"readonly=true");
               MemCat (&gMemTemp,
                  L" name=shardaddr");
               MemCat (&gMemTemp, (int)i);
               MemCat (&gMemTemp,
                  L"/></bold></td>"
                  L"</tr>"
                  L"<tr>"
                  L"<td>"
                  L"<bold>Port</bold> - TCP port number. If you don't know what this means,"
                  L"use a number in the 4000's."
                  L"</td>"
                  L"<td><bold><edit width=100% maxchars=64 ");
               if (prs->m_fReadOnly)
                  MemCat (&gMemTemp, L"readonly=true");
               MemCat (&gMemTemp,
                  L" name=shardport");
               MemCat (&gMemTemp, (int)i);
               MemCat (&gMemTemp,
                  L"/></bold></td>"
                  L"</tr>"
                  L"<tr>"
                  L"<td>"
                  L"<bold>Parameter</bold> - Parameter passed to the server code so it knows what"
                  L"shard it's running. The parameter is accessible through ShardParam()."
                  L"</td>"
                  L"<td><bold><edit width=100% maxchars=64 ");
               if (prs->m_fReadOnly)
                  MemCat (&gMemTemp, L"readonly=true");
               MemCat (&gMemTemp,
                  L" name=shardparam");
               MemCat (&gMemTemp, (int) i);
               MemCat (&gMemTemp,
                  L"/></bold></td>"
                  L"</tr>"
                  L"<tr><td>"
                  L"<xChoiceButton style=righttriangle ");
               if (prs->m_fReadOnly)
                  MemCat (&gMemTemp, L"enabled=false");
               MemCat (&gMemTemp,
                  L" name=sharddel");
               MemCat (&gMemTemp, (int)i);
               MemCat (&gMemTemp,
                  L">"
                  L"<bold>Delete this shard</bold><br/>"
                  L"Pressing this deletes this shard from the list."
                  L"</xChoiceButton>"
                  L"</td></tr>");
            } // i

            p->pszSubString = (PWSTR)gMemTemp.p;
            return TRUE;
         }
      }
      break;

   }; // dwMessage

   return FALSE;
}


/*************************************************************************************
CResTitleInfo::Edit - This brings up a dialog box for editing the object.

inputs
   HWND           hWnd - Window to bring dialog up from
   LANGID         lid - Language ID to use as default
   BOOL           fReadOnly - If TRUE then data is read only and cant be changed
returns
   BOOL - TRUE if changed, FALSE if didnt
*/
BOOL CResTitleInfo::Edit (HWND hWnd, LANGID lid, BOOL fReadOnly)
{
   m_lid = lid;   // to to remember

   BOOL fChanged = FALSE;
   m_pfChanged = &fChanged;
   m_fReadOnly  = fReadOnly;
   m_lid = lid;
   m_iVScroll = 0;
   m_dwTab = 0;
   CEscWindow Window;

   // create the window
   RECT r;
   PWSTR psz;
   DialogBoxLocation3 (GetDesktopWindow(), &r, TRUE);
   Window.Init (ghInstance, hWnd, 0, &r);
redo:
   psz = Window.PageDialog (ghInstance, IDR_MMLRESTITLEINFO, TitleInfoPage, this);
   m_iVScroll = Window.m_iExitVScroll;
   if (psz && !_wcsicmp(psz, RedoSamePage()))
      goto redo;


   return fChanged;
}





/*************************************************************************************
CResTitleInfoSet::Constructor and destructor
*/
CResTitleInfoSet::CResTitleInfoSet (void)
{
   m_lPCResTitleInfo.Init (sizeof(PCResTitleInfo));
}

CResTitleInfoSet::~CResTitleInfoSet (void)
{
   DWORD i;
   PCResTitleInfo *ppr = (PCResTitleInfo*)m_lPCResTitleInfo.Get(0);
   for (i = 0; i < m_lPCResTitleInfo.Num(); i++)
      delete ppr[i];
   m_lPCResTitleInfo.Clear();
}



/*************************************************************************************
CResTitleInfoSet::Clone
*/
CResTitleInfoSet *CResTitleInfoSet::Clone (void)
{
   PCResTitleInfoSet pNew = new CResTitleInfoSet;
   if (!pNew)
      return NULL;

   // pNew->m_lPCResTitleInfoShard will be empty
   pNew->m_lPCResTitleInfo.Init (sizeof(PCResTitleInfo), m_lPCResTitleInfo.Get(0), m_lPCResTitleInfo.Num());
   PCResTitleInfo *ppTIS = (PCResTitleInfo*)pNew->m_lPCResTitleInfo.Get(0);
   DWORD i;
   for (i = 0; i < pNew->m_lPCResTitleInfo.Num(); i++)
      ppTIS[i] = ppTIS[i]->Clone();

   return pNew;

}
static PWSTR gpszTitleInfoSet = L"TitleInfoSet";

/*************************************************************************************
CResTitleInfoSet::MMLTo - Standard API
*/
PCMMLNode2 CResTitleInfoSet::MMLTo (void)
{
   PCMMLNode2 pNode = new CMMLNode2;
   if (!pNode)
      return NULL;
   pNode->NameSet (gpszTitleInfoSet);

   PCMMLNode2 pSub;
   DWORD i;
   PCResTitleInfo *ppr = (PCResTitleInfo*)m_lPCResTitleInfo.Get(0);
   for (i = 0; i < m_lPCResTitleInfo.Num(); i++) {
      pSub = ppr[i]->MMLTo();
      if (pSub)
         pNode->ContentAdd (pSub);
   } // i

   return pNode;
}


/*************************************************************************************
CResTitleInfoSet::MMLFrom - Standard API
*/
BOOL CResTitleInfoSet::MMLFrom (PCMMLNode2 pNode)
{
   // clear out existing
   DWORD i;
   PCResTitleInfo *ppr = (PCResTitleInfo*)m_lPCResTitleInfo.Get(0);
   for (i = 0; i < m_lPCResTitleInfo.Num(); i++)
      delete ppr[i];
   m_lPCResTitleInfo.Clear();

   PWSTR psz;
   PCMMLNode2 pSub;
   for (i = 0; i < pNode->ContentNum(); i++) {
      pSub = NULL;
      pNode->ContentEnum (i, &psz, &pSub);
      if (!pSub)
         continue;
      psz = pSub->NameGet();

      if (psz && !_wcsicmp(psz, CircumrealityTitleInfo())) {
         PCResTitleInfo pNew = new CResTitleInfo;
         if (!pNew)
            continue;
         pNew->MMLFrom (pSub);
         m_lPCResTitleInfo.Add (&pNew);
      }
   } // i

   return TRUE;
}


/*************************************************************************************
CResTitleInfoSet::FromResource - Given a MIFL resource with the TitleInfo resource,
this extracts the inforamtion and stores it in this object, including all the
various languages.

inputs
   PCMIFLResource       pRes - Resource that's of type CircumrealityTitleInfo()
returns
   BOOL - TRUE if success
*/
BOOL CResTitleInfoSet::FromResource (PCMIFLResource pRes)
{
   // clear out existing
   DWORD i;
   PCResTitleInfo *ppr = (PCResTitleInfo*)m_lPCResTitleInfo.Get(0);
   for (i = 0; i < m_lPCResTitleInfo.Num(); i++)
      delete ppr[i];
   m_lPCResTitleInfo.Clear();

   // add
   PCMMLNode2 *ppn = (PCMMLNode2*)pRes->m_lPCMMLNode2.Get(0);
   LANGID *plid = (LANGID*)pRes->m_lLANGID.Get(0);
   for (i = 0; i < pRes->m_lPCMMLNode2.Num(); i++) {
      PCResTitleInfo pNew = new CResTitleInfo;
      if (!pNew)
         return FALSE;
      pNew->MMLFrom (ppn[i]);

      pNew->m_lid = plid[i]; // should already be set, but just in case

      m_lPCResTitleInfo.Add (&pNew);
   } // i

   return TRUE;
}


/*************************************************************************************
CResTitleInfoSet::Find - Finds the restitle that most closesly matches the supplied
language.

inputs
   LANGID         lid - Language desired
*/
PCResTitleInfo CResTitleInfoSet::Find (LANGID lid)
{
   PCResTitleInfo *ppr = (PCResTitleInfo*)m_lPCResTitleInfo.Get(0);
   DWORD dwNum = m_lPCResTitleInfo.Num();
   if (dwNum == 0)
      return NULL;
   else if (dwNum == 1)
      return ppr[0];   // no choice

   DWORD i, dwScore;
   DWORD dwBest = -1;
   DWORD dwBestScore = 0;

   WORD wPrim = PRIMARYLANGID(lid);

   for (i = 0; i < dwNum; i++) {
      if (lid == ppr[i]->m_lid) {
         return ppr[i];   // since this is always the best
      }
      else if (wPrim == PRIMARYLANGID(ppr[i]->m_lid))
         dwScore = 3;
      else if (i == 0)
         dwScore = 2;
      else
         dwScore = 0;

      if ((dwBest == -1) || (dwScore > dwBestScore)) {
         dwBest  = i;
         dwBestScore = dwScore;
      }
   } // i

   if (dwBest == -1)
      return NULL;   // shouldnt happen
   else
      return ppr[dwBest];
}

