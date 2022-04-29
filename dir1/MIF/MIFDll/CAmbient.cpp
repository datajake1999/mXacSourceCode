/*************************************************************************************
CAmbient.cpp - Code for UI to modify the ambient sounds.

begun 18/8/04 by Mike Rozak.
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




/*************************************************************************************
OpenMusicDialog - Dialog box for opening a .jpg or .bmp

inputs
   HWND           hWnd - To display dialog off of
   PWSTR          pszFile - Pointer to a file name. Must be filled with initial file
   DWORD          dwChars - Number of characters in the file
   BOOL           fSave - If TRUE then saving instead of openeing. if fSave then
                     pszFile contains an initial file name, or empty string
returns
   BOOL - TRUE if pszFile filled in, FALSE if nothing opened
*/
BOOL OpenMusicDialog (HWND hWnd, PWSTR pszFile, DWORD dwChars, BOOL fSave)
{
   OPENFILENAME   ofn;
   char  szTemp[256];
   szTemp[0] = 0;
   WideCharToMultiByte (CP_ACP, 0, pszFile, -1, szTemp, sizeof(szTemp), 0, 0);
   memset (&ofn, 0, sizeof(ofn));
   
   // BUGFIX - Set directory
   char szInitial[256];
   strcpy (szInitial, gszAppDir);
   GetLastDirectory(szInitial, sizeof(szInitial)); // BUGFIX - get last dir
   ofn.lpstrInitialDir = szInitial;

   ofn.lStructSize = sizeof(ofn);
   ofn.hwndOwner = hWnd;
   ofn.hInstance = ghInstance;
   ofn.lpstrFilter = "MIDI file (*.mid)\0*.mid\0\0\0";
   ofn.lpstrFile = szTemp;
   ofn.nMaxFile = sizeof(szTemp);
   ofn.lpstrTitle = fSave ? "Save music" :
      "Open an music file";
   ofn.Flags = fSave ? (OFN_PATHMUSTEXIST | OFN_HIDEREADONLY) :
      (OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY);
   ofn.lpstrDefExt = "mid";
   // nFileExtension 

   if (fSave) {
      if (!GetSaveFileName(&ofn))
         return FALSE;
   }
   else {
      if (!GetOpenFileName(&ofn))
         return FALSE;
   }

   // BUGFIX - Save diretory
   strcpy (szInitial, ofn.lpstrFile);
   szInitial[ofn.nFileOffset] = 0;
   SetLastDirectory(szInitial);

   // copy over
   MultiByteToWideChar (CP_ACP, 0, ofn.lpstrFile, -1, pszFile, dwChars);
   return TRUE;
}







/*************************************************************************************
CAmbient::Constructor and destructor
*/
CAmbient::CAmbient (void)
{
   MemZero (&m_memName);
   m_gID = GUID_NULL;
   m_lPCAmbientRandom.Init (sizeof(PCAmbientRandom));
   m_lPCAmbientLoop.Init (sizeof(PCAmbientLoop));
   m_pOffset.Zero();
}

CAmbient::~CAmbient (void)
{
   DWORD i;

   // clear random
   PCAmbientRandom *ppar = (PCAmbientRandom*)m_lPCAmbientRandom.Get(0);
   for (i = 0; i < m_lPCAmbientRandom.Num(); i++)
      delete ppar[i];
   m_lPCAmbientRandom.Clear();

   // clear Loop
   PCAmbientLoop *ppal = (PCAmbientLoop*)m_lPCAmbientLoop.Get(0);
   for (i = 0; i < m_lPCAmbientLoop.Num(); i++)
      delete ppal[i];
   m_lPCAmbientLoop.Clear();
}


static PWSTR gpszName = L"Name";
static PWSTR gpszID = L"ID";
static PWSTR gpszRandom = L"Random";
static PWSTR gpszLoop = L"Loop";
static PWSTR gpszMin = L"Min";
static PWSTR gpszMax = L"Max";
static PWSTR gpszVol = L"Vol";
static PWSTR gpszTime = L"Time";
static PWSTR gpszV = L"v";
static PWSTR gpszMusic = L"Music";
static PWSTR gpszWave = L"Wave";
static PWSTR gpszMinDist = L"MinDist";
static PWSTR gpszOffset = L"Offset";

/*************************************************************************************
CAmbient::MMLTo - Standard API
*/
PCMMLNode2 CAmbient::MMLTo (void)
{
   PCMMLNode2 pNode = new CMMLNode2;
   if (!pNode)
      return NULL;
   pNode->NameSet (CircumrealityAmbient());

   PWSTR psz = (PWSTR)m_memName.p;
   if (psz && psz[0])
      MMLValueSet (pNode, gpszName, psz);
   if (!IsEqualGUID(m_gID, GUID_NULL))
      MMLValueSet (pNode, gpszID, (PBYTE)&m_gID, sizeof(m_gID));
   if (m_pOffset.p[0] || m_pOffset.p[1] || m_pOffset.p[2])
      MMLValueSet (pNode, gpszOffset, &m_pOffset);

   // all the random ones
   DWORD i;
   PCMMLNode2 pSub;
   PCAmbientRandom *ppar = (PCAmbientRandom*)m_lPCAmbientRandom.Get(0);
   for (i = 0; i < m_lPCAmbientRandom.Num(); i++) {
      pSub = ppar[i]->MMLTo();
      if (pSub)
         pNode->ContentAdd (pSub);
   }

   // clear Loop
   PCAmbientLoop *ppal = (PCAmbientLoop*)m_lPCAmbientLoop.Get(0);
   for (i = 0; i < m_lPCAmbientLoop.Num(); i++) {
      pSub = ppal[i]->MMLTo();
      if (pSub)
         pNode->ContentAdd (pSub);
   }

   return pNode;
}


/*************************************************************************************
CAmbient::MMLFrom - Standard API
*/
BOOL CAmbient::MMLFrom (PCMMLNode2 pNode)
{
   DWORD i;

   // clear random
   PCAmbientRandom *ppar = (PCAmbientRandom*)m_lPCAmbientRandom.Get(0);
   for (i = 0; i < m_lPCAmbientRandom.Num(); i++)
      delete ppar[i];
   m_lPCAmbientRandom.Clear();

   // clear Loop
   PCAmbientLoop *ppal = (PCAmbientLoop*)m_lPCAmbientLoop.Get(0);
   for (i = 0; i < m_lPCAmbientLoop.Num(); i++)
      delete ppal[i];
   m_lPCAmbientLoop.Clear();

   // clear other info
   MemZero (&m_memName);
   m_gID = GUID_NULL;
   m_pOffset.Zero();

   PWSTR psz;
   psz = MMLValueGet (pNode, gpszName);
   if (psz)
      MemCat (&m_memName, psz);
   MMLValueGetPoint (pNode, gpszOffset, &m_pOffset);
   MMLValueGetBinary (pNode, gpszID, (PBYTE) &m_gID, sizeof(m_gID));

   PCMMLNode2 pSub;
   for (i = 0; i < pNode->ContentNum(); i++) {
      pSub = NULL;
      pNode->ContentEnum (i, &psz, &pSub);
      if (!pSub)
         continue;
      psz = pSub->NameGet();
      if (!psz)
         continue;

      if (!_wcsicmp(psz, gpszRandom)) {
         PCAmbientRandom par = new CAmbientRandom;
         if (!par)
            continue;
         if (!par->MMLFrom (pSub)) {
            delete par;
            continue;
         }
         m_lPCAmbientRandom.Add (&par);
         continue;
      }
      else if (!_wcsicmp(psz, gpszLoop)) {
         PCAmbientLoop par = new CAmbientLoop;
         if (!par)
            continue;
         if (!par->MMLFrom (pSub)) {
            delete par;
            continue;
         }
         m_lPCAmbientLoop.Add (&par);
         continue;
      }
   } // i

   return TRUE;
}





/*************************************************************************
AmbientPage
*/
BOOL AmbientPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCAmbient pa = (PCAmbient)pPage->m_pUserData;   // node to modify

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         PCEscControl pControl;

         // set the name
         pControl = pPage->ControlFind (L"name");
         if (pControl)
            pControl->AttribSet (Text(), (PWSTR)pa->m_memName.p);


         // display list box entries
         ESCMLISTBOXADD lba;
         DWORD i;
         WCHAR szTemp[64];
         pControl = pPage->ControlFind (L"randomlist");
         if (pControl) for (i = 0; i < pa->m_lPCAmbientRandom.Num(); i++) {
            swprintf (szTemp, L"Random set #%d", (int)i+1);
            memset (&lba, 0, sizeof(lba));
            lba.dwInsertBefore = -1;
            lba.pszText = szTemp;
            pControl->Message (ESCM_LISTBOXADD, &lba);
         } // i
         pControl = pPage->ControlFind (L"looplist");
         if (pControl) for (i = 0; i < pa->m_lPCAmbientLoop.Num(); i++) {
            swprintf (szTemp, L"Looped set #%d", (int)i+1);
            memset (&lba, 0, sizeof(lba));
            lba.dwInsertBefore = -1;
            lba.pszText = szTemp;
            pControl->Message (ESCM_LISTBOXADD, &lba);
         } // i

         // ambient
         for (i = 0; i < 3; i++) {
            swprintf (szTemp, L"offset%d", (int)i);
            MeasureToString (pPage, szTemp, pa->m_pOffset.p[i]);

            if (pa->m_fReadOnly && (pControl = pPage->ControlFind (szTemp)))
               pControl->Enable (FALSE);
         } // i

         // disable controls
         if (pa->m_fReadOnly) {
            if (pControl = pPage->ControlFind (L"name"))
               pControl->Enable(FALSE);
            if (pControl = pPage->ControlFind (L"newrandom"))
               pControl->Enable(FALSE);
            if (pControl = pPage->ControlFind (L"editrandom"))
               pControl->Enable(FALSE);
            if (pControl = pPage->ControlFind (L"removerandom"))
               pControl->Enable(FALSE);
            if (pControl = pPage->ControlFind (L"newloop"))
               pControl->Enable(FALSE);
            if (pControl = pPage->ControlFind (L"copyloop"))
               pControl->Enable(FALSE);
            if (pControl = pPage->ControlFind (L"editloop"))
               pControl->Enable(FALSE);
            if (pControl = pPage->ControlFind (L"removeloop"))
               pControl->Enable(FALSE);
         }
      }
      break;


   case ESCN_EDITCHANGE:
      {
         PESCNEDITCHANGE p = (PESCNEDITCHANGE)pParam;
         PWSTR psz = p->pControl->m_pszName;
         if (!psz)
            break;

         PWSTR pszOffset = L"offset";
         DWORD dwOffsetLen = (DWORD)wcslen(pszOffset);

         if (!_wcsicmp(psz, L"name")) {
            DWORD dwNeed = 0;
            p->pControl->AttribGet (Text(), NULL, 0, &dwNeed);
            if (!pa->m_memName.Required (dwNeed))
               return TRUE;
            p->pControl->AttribGet (Text(), (PWSTR) pa->m_memName.p, (DWORD)pa->m_memName.m_dwAllocated, &dwNeed);
            pa->m_fChanged = TRUE;
            return TRUE;
         }
         else if (!wcsncmp (psz, pszOffset, dwOffsetLen)) {
            DWORD dwIndex = _wtoi(psz + dwOffsetLen);
            MeasureParseString (pPage, psz, &pa->m_pOffset.p[dwIndex]);
            pa->m_fChanged = TRUE;
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

         if (!_wcsicmp(psz, L"newrandom")) {
            PCAmbientRandom par = new CAmbientRandom;
            if (!par)
               return TRUE;
            pa->m_lPCAmbientRandom.Add (&par);
            pa->m_fChanged = TRUE;

            // edit this
            WCHAR szTemp[64];
            swprintf (szTemp, L"random:%d", (int)pa->m_lPCAmbientRandom.Num()-1);
            pPage->Exit (szTemp);
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"editrandom")) {
            DWORD dwSel = 0;
            PCEscControl pControl = pPage->ControlFind (L"randomlist");
            if (pControl)
               dwSel = (DWORD)pControl->AttribGetInt (CurSel());
            if (dwSel >= pa->m_lPCAmbientRandom.Num()) {
               pPage->MBWarning (L"You must select a set first.");
               return TRUE;
            }

            // edit this
            WCHAR szTemp[64];
            swprintf (szTemp, L"random:%d", (int)dwSel);
            pPage->Exit (szTemp);
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"removerandom")) {
            DWORD dwSel = 0;
            PCEscControl pControl = pPage->ControlFind (L"randomlist");
            if (pControl)
               dwSel = (DWORD)pControl->AttribGetInt (CurSel());
            if (dwSel >= pa->m_lPCAmbientRandom.Num()) {
               pPage->MBWarning (L"You must select a set first.");
               return TRUE;
            }

            if (IDYES != pPage->MBYesNo (L"Are you sure you wish to delete the set?"))
               return TRUE;

            // delete
            PCAmbientRandom *ppar = (PCAmbientRandom*) pa->m_lPCAmbientRandom.Get(0);
            delete ppar[dwSel];
            pa->m_lPCAmbientRandom.Remove (dwSel);
            pa->m_fChanged = TRUE;

            ESCMLISTBOXDELETE del;
            memset (&del, 0, sizeof(del));
            del.dwIndex = dwSel;
            pControl->Message (ESCM_LISTBOXDELETE, &del);

            return TRUE;
         }
         else if (!_wcsicmp(psz, L"newloop")) {
            PCAmbientLoop par = new CAmbientLoop;
            if (!par)
               return TRUE;
            pa->m_lPCAmbientLoop.Add (&par);
            pa->m_fChanged = TRUE;

            // edit this
            WCHAR szTemp[64];
            swprintf (szTemp, L"loop:%d", (int)pa->m_lPCAmbientLoop.Num()-1);
            pPage->Exit (szTemp);
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"copyloop")) {
            DWORD dwSel = 0;
            PCEscControl pControl = pPage->ControlFind (L"looplist");
            if (pControl)
               dwSel = (DWORD)pControl->AttribGetInt (CurSel());
            if (dwSel >= pa->m_lPCAmbientLoop.Num()) {
               pPage->MBWarning (L"You must select a set first.");
               return TRUE;
            }

            // copy it
            PCAmbientLoop *ppal = (PCAmbientLoop*)pa->m_lPCAmbientLoop.Get(0);
            PCMMLNode2 pNode = ppal[dwSel]->MMLTo ();
            PCAmbientLoop par = new CAmbientLoop;
            if (!par) {
               delete pNode;
               return TRUE;
            }
            par->MMLFrom (pNode);
            delete pNode;
            pa->m_lPCAmbientLoop.Add (&par);
            pa->m_fChanged = TRUE;

            // edit this
            WCHAR szTemp[64];
            swprintf (szTemp, L"loop:%d", (int)pa->m_lPCAmbientLoop.Num()-1);
            pPage->Exit (szTemp);
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"editloop")) {
            DWORD dwSel = 0;
            PCEscControl pControl = pPage->ControlFind (L"looplist");
            if (pControl)
               dwSel = (DWORD)pControl->AttribGetInt (CurSel());
            if (dwSel >= pa->m_lPCAmbientLoop.Num()) {
               pPage->MBWarning (L"You must select a set first.");
               return TRUE;
            }

            // edit this
            WCHAR szTemp[64];
            swprintf (szTemp, L"loop:%d", (int)dwSel);
            pPage->Exit (szTemp);
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"removeloop")) {
            DWORD dwSel = 0;
            PCEscControl pControl = pPage->ControlFind (L"looplist");
            if (pControl)
               dwSel = (DWORD)pControl->AttribGetInt (CurSel());
            if (dwSel >= pa->m_lPCAmbientLoop.Num()) {
               pPage->MBWarning (L"You must select a set first.");
               return TRUE;
            }

            if (IDYES != pPage->MBYesNo (L"Are you sure you wish to delete the set?"))
               return TRUE;

            // delete
            PCAmbientLoop *ppar = (PCAmbientLoop*) pa->m_lPCAmbientLoop.Get(0);
            delete ppar[dwSel];
            pa->m_lPCAmbientLoop.Remove (dwSel);
            pa->m_fChanged = TRUE;

            ESCMLISTBOXDELETE del;
            memset (&del, 0, sizeof(del));
            del.dwIndex = dwSel;
            pControl->Message (ESCM_LISTBOXDELETE, &del);

            return TRUE;
         }
      }
      break;


   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Ambient sounds resource";
            return TRUE;
         }
      }
      break;

   }; // dwMessage

   return FALSE;
}


/*************************************************************************************
CAmbient::Edit - This brings up a dialog box for editing the object.

inputs
   HWND           hWnd - Window to bring dialog up from
   BOOL           fReadOnly - If TRUE then data is read only and cant be changed
returns
   BOOL - TRUE if changed, FALSE if didnt
*/
BOOL CAmbient::Edit (HWND hWnd, BOOL fReadOnly)
{
   m_fChanged = FALSE;
   m_fReadOnly  = fReadOnly;

   CEscWindow Window;

   PWSTR pszRandom = L"random:", pszLoop = L"loop:";
   DWORD dwRandomLen = (DWORD)wcslen(pszRandom), dwLoopLen = (DWORD)wcslen(pszLoop);

   // create the window
   RECT r;
   PWSTR psz;
   DialogBoxLocation3 (GetDesktopWindow(), &r, TRUE);
   Window.Init (ghInstance, hWnd, 0, &r);
redo:
   psz = Window.PageDialog (ghInstance, IDR_MMLAMBIENT, AmbientPage, this);
   if (!psz)
      goto done;
   if (!_wcsicmp(psz, RedoSamePage()))
      goto redo;
   else if (!wcsncmp (psz, pszRandom, dwRandomLen)) {
      DWORD dwNum = _wtoi(psz + dwRandomLen);
      PCAmbientRandom *ppar = (PCAmbientRandom*)m_lPCAmbientRandom.Get(0);
      PCAmbientRandom par = ppar[dwNum];
      BOOL fChanged, fRet;

      fRet = par->Edit (&Window, fReadOnly, &fChanged);
      if (fChanged)
         m_fChanged = TRUE;
      if (fRet)
         goto redo;
      else
         goto done;
   }
   else if (!wcsncmp (psz, pszLoop, dwLoopLen)) {
      DWORD dwNum = _wtoi(psz + dwLoopLen);
      PCAmbientLoop *ppar = (PCAmbientLoop*)m_lPCAmbientLoop.Get(0);
      PCAmbientLoop par = ppar[dwNum];
      BOOL fChanged, fRet;

      fRet = par->Edit (&Window, fReadOnly, &fChanged);
      if (fChanged)
         m_fChanged = TRUE;
      if (fRet)
         goto redo;
      else
         goto done;
   }

done:
   return m_fChanged;
}



/*************************************************************************************
CAmbient::TimeInit - Initializes all the time information so that the ambient
object can be used in the audio thread.
*/
void CAmbient::TimeInit (void)
{
   DWORD i;
   PCAmbientRandom *ppar = (PCAmbientRandom*)m_lPCAmbientRandom.Get(0);
   for (i = 0; i < m_lPCAmbientRandom.Num(); i++)
      ppar[i]->TimeUpdate();  // to clear

   // will need to init for loop
   PCAmbientLoop *ppal = (PCAmbientLoop*)m_lPCAmbientLoop.Get(0);
   for (i = 0; i < m_lPCAmbientLoop.Num(); i++)
      ppal[i]->m_dwLastState = -1;
}


/*************************************************************************************
CAmbient::TimeElapsed - Call this when time has elapsed. This will subtract
the time from the m_fTimerToXXX info and fill the list with any events that need to
be added.

inputs
   double            fDelta - Time elapsed since the last call
   PCListFixed       plEvents - Appends events (PCMMLNode2) that need to be
                        handles, such as <Wave> or <Music>. This should have
                        already been initialized
returns
   none
*/
void CAmbient::TimeElapsed (double fDelta, PCListFixed plEvents)
{
   DWORD i;
   PCAmbientRandom *ppar = (PCAmbientRandom*)m_lPCAmbientRandom.Get(0);
   GUID *pgID = IsEqualGUID(m_gID, GUID_NULL) ? NULL : &m_gID;
   for (i = 0; i < m_lPCAmbientRandom.Num(); i++)
      ppar[i]->TimeElapsed(this, fDelta, pgID, plEvents);
}



/*************************************************************************************
CAmbient::LoopNum - Returns the number of loop objects
*/
DWORD CAmbient::LoopNum (void)
{
   return m_lPCAmbientLoop.Num();
}


/*************************************************************************************
CAmbient::LoopWhatsNext - Given the current loop, advances forward and finds out
what the next audio events are. These are filled into plEvents

inputs
   DWORD             dwNum - Loop number
   PCBTree           ptAmbientLoopVar - Tree indexed by a var name, with doubles as
                     the values. Used for testing divergence based on values.
   PCListFixed       plEvents - Appends events (PCMMLNode2) that need to be
                        handles, such as <Wave> or <Music>. This should have
                        already been initialized
returns
   BOOL - TRUE if success, FALSE if cant find the loop
*/
BOOL CAmbient::LoopWhatsNext (DWORD dwNum, PCBTree ptAmbientLoopVar, PCListFixed plEvents)
{
   PCAmbientLoop *ppal = (PCAmbientLoop*)m_lPCAmbientLoop.Get(dwNum);
   if (!ppal || !ppal[0])
      return FALSE;

   GUID *pgID = IsEqualGUID(m_gID, GUID_NULL) ? NULL : &m_gID;
   return ppal[0]->LoopWhatsNext (this, pgID, ptAmbientLoopVar, plEvents);
}




/*************************************************************************************
CAmbientRandom::Constructor and destructor
*/
CAmbientRandom::CAmbientRandom (void)
{
   m_f3D = TRUE;

   m_pMin.p[0] = m_pMin.p[1] = -50;
   m_pMin.p[2] = 2;
   m_pMin.p[3] = 40; // dB

   m_pMax.p[0] = m_pMax.p[1] = 50;
   m_pMax.p[2] = 4;
   m_pMax.p[3] = 60; // dB

   m_fMinDist = 1;

   m_pTime.p[0] = 1;
   m_pTime.p[1] = 5;
   m_pTime.p[2] = 2;
   m_pTime.p[3] = 1; // meaningless value

   m_pVol.p[0] = m_pVol.p[2] = 0.8;
   m_pVol.p[1] = m_pVol.p[3] = 1.2;

   m_fTimeToNextSound = m_fTimeToNextTimer = 0;
}

CAmbientRandom::~CAmbientRandom (void)
{
   // do nothing, for now
}



/*************************************************************************************
CAmbientRandom::MMLTo - Standard API
*/
PCMMLNode2 CAmbientRandom::MMLTo (void)
{
   PCMMLNode2 pNode = new CMMLNode2;
   if (!pNode)
      return NULL;
   pNode->NameSet (gpszRandom);

   if (m_f3D) {
      MMLValueSet (pNode, gpszMin, &m_pMin);
      MMLValueSet (pNode, gpszMax, &m_pMax);
      if (m_fMinDist != 1)
         MMLValueSet (pNode, gpszMinDist, m_fMinDist);
   }
   else
      MMLValueSet (pNode, gpszVol, &m_pVol);
   MMLValueSet (pNode, gpszTime, &m_pTime);

   DWORD i;
   PWSTR psz;
   PCMMLNode2 pSub;
   for (i = 0; i < m_lWave.Num(); i++) {
      psz = (PWSTR)m_lWave.Get(i);
      if (psz && psz[0]) {
         pSub = pNode->ContentAddNewNode ();
         if (!pSub)
            continue;
         pSub->NameSet (gpszWave);
         pSub->AttribSetString (gpszV, psz);
      }
   } // i
   for (i = 0; i < m_lMusic.Num(); i++) {
      psz = (PWSTR)m_lMusic.Get(i);
      if (psz && psz[0]) {
         pSub = pNode->ContentAddNewNode ();
         if (!pSub)
            continue;
         pSub->NameSet (gpszMusic);
         pSub->AttribSetString (gpszV, psz);
      }
   } // i

   return pNode;
}



/*************************************************************************************
CAmbientRandom::MMLFrom - Standard API
*/
BOOL CAmbientRandom::MMLFrom (PCMMLNode2 pNode)
{
   // clear out what have
   m_pMin.p[0] = m_pMin.p[1] = -50;
   m_pMin.p[2] = 2;
   m_pMin.p[3] = 40; // dB

   m_pMax.p[0] = m_pMax.p[1] = 50;
   m_pMax.p[2] = 4;
   m_pMax.p[3] = 0; // dB... make this 0 so if not set won't use 3d sound

   m_fMinDist = 1;

   m_pTime.p[0] = 1;
   m_pTime.p[1] = 5;
   m_pTime.p[2] = 2;
   m_pTime.p[3] = 1; // meaningless value

   m_pVol.p[0] = m_pVol.p[2] = 0.8;
   m_pVol.p[1] = m_pVol.p[3] = 1.2;

   m_lWave.Clear();
   m_lMusic.Clear();

   // get points
   MMLValueGetPoint (pNode, gpszMin, &m_pMin);
   MMLValueGetPoint (pNode, gpszMax, &m_pMax);
   m_fMinDist = MMLValueGetDouble (pNode, gpszMinDist, 1);
   MMLValueGetPoint (pNode, gpszTime, &m_pTime);
   MMLValueGetPoint (pNode, gpszVol, &m_pVol);

   if (m_pMax.p[3])
      m_f3D = TRUE;
   else {
      m_f3D = FALSE;
      m_pMax.p[3] = 60; // just so have value
   }

   // get contents
   DWORD i;
   PWSTR psz;
   PCMMLNode2 pSub;
   for (i = 0; i < pNode->ContentNum(); i++) {
      pSub = NULL;
      pNode->ContentEnum (i, &psz, &pSub);
      if (!pSub)
         continue;
      psz = pSub->NameGet();
      if (!psz)
         continue;

      if (!_wcsicmp(psz, gpszWave)) {
         psz = pSub->AttribGetString (gpszV);
         if (psz)
            m_lWave.Add (psz, (wcslen(psz)+1)*sizeof(WCHAR));
         continue;
      }
      else if (!_wcsicmp(psz, gpszMusic)) {
         psz = pSub->AttribGetString (gpszV);
         if (psz)
            m_lMusic.Add (psz, (wcslen(psz)+1)*sizeof(WCHAR));
         continue;
      }
   } // i

   return TRUE;
}



/*************************************************************************
AmbientRandomPage
*/
BOOL AmbientRandomPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCAmbientRandom pa = (PCAmbientRandom)pPage->m_pUserData;   // node to modify

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         PCEscControl pControl;

         // set the name
         pControl = pPage->ControlFind (L"use3d");
         if (pControl) {
            pControl->AttribSetBOOL (Checked(), pa->m_f3D);
            if (pa->m_fReadOnly)
               pControl->Enable (FALSE);
         }

         // fill in values
         DWORD i;
         WCHAR szTemp[64];
         for (i = 0; i < 4; i++) {
            // min
            swprintf (szTemp, L"min%d", (int)i);
            if (i == 3)
               DoubleToControl (pPage, szTemp, pa->m_pMin.p[i]);
            else
               MeasureToString (pPage, szTemp, pa->m_pMin.p[i]);
            if (pa->m_fReadOnly && (pControl = pPage->ControlFind(szTemp)))
               pControl->Enable(FALSE);

            // max
            swprintf (szTemp, L"max%d", (int)i);
            if (i == 3)
               DoubleToControl (pPage, szTemp, pa->m_pMax.p[i]);
            else
               MeasureToString (pPage, szTemp, pa->m_pMax.p[i]);
            if (pa->m_fReadOnly && (pControl = pPage->ControlFind(szTemp)))
               pControl->Enable(FALSE);

            // volume
            swprintf (szTemp, L"vol%d", (int)i);
            DoubleToControl (pPage, szTemp, pa->m_pVol.p[i]);
            if (pa->m_fReadOnly && (pControl = pPage->ControlFind(szTemp)))
               pControl->Enable(FALSE);

            // time
            if (i < 3) {
               swprintf (szTemp, L"time%d", (int)i);
               DoubleToControl (pPage, szTemp, pa->m_pTime.p[i]);
               if (pa->m_fReadOnly && (pControl = pPage->ControlFind(szTemp)))
                  pControl->Enable(FALSE);
            }
         }
         MeasureToString (pPage, L"MinDist", pa->m_fMinDist);
         if (pa->m_fReadOnly && (pControl = pPage->ControlFind(L"MinDist")))
            pControl->Enable(FALSE);


         pPage->Message (ESCM_USER+104);

         // disable controls
         if (pa->m_fReadOnly) {
            if (pControl = pPage->ControlFind (L"newwave"))
               pControl->Enable(FALSE);
            if (pControl = pPage->ControlFind (L"newmusic"))
               pControl->Enable(FALSE);
            if (pControl = pPage->ControlFind (L"remove"))
               pControl->Enable(FALSE);
         }
      }
      break;

   case ESCM_USER+104:  // update the listbox
      {
         // display list box entries
         ESCMLISTBOXADD lba;
         DWORD i;
         PCEscControl pControl;
         pControl = pPage->ControlFind (L"randomlist");
         if (!pControl)
            return TRUE;

         pControl->Message (ESCM_LISTBOXRESETCONTENT);

         for (i = 0; i < pa->m_lWave.Num(); i++) {
            memset (&lba, 0, sizeof(lba));
            lba.dwInsertBefore = -1;
            lba.pszText = (PWSTR)pa->m_lWave.Get(i);
            pControl->Message (ESCM_LISTBOXADD, &lba);
         } // i
         // append MIDI too
         for (i = 0; i < pa->m_lMusic.Num(); i++) {
            memset (&lba, 0, sizeof(lba));
            lba.dwInsertBefore = -1;
            lba.pszText = (PWSTR)pa->m_lMusic.Get(i);
            pControl->Message (ESCM_LISTBOXADD, &lba);
         } // i
      }
      return TRUE;


   case ESCN_EDITCHANGE:
      {
         PESCNEDITCHANGE p = (PESCNEDITCHANGE)pParam;
         PWSTR psz = p->pControl->m_pszName;
         if (!psz)
            break;

         // just get all value
         *(pa->m_pfChanged) = TRUE;

         // fill in values
         DWORD i;
         WCHAR szTemp[64];
         for (i = 0; i < 4; i++) {
            // min
            swprintf (szTemp, L"min%d", (int)i);
            if (i == 3)
               pa->m_pMin.p[i] = DoubleFromControl (pPage, szTemp);
            else
               MeasureParseString (pPage, szTemp, &pa->m_pMin.p[i]);

            // max
            swprintf (szTemp, L"max%d", (int)i);
            if (i == 3)
               pa->m_pMax.p[i] = DoubleFromControl (pPage, szTemp);
            else
               MeasureParseString (pPage, szTemp, &pa->m_pMax.p[i]);

            // volume
            swprintf (szTemp, L"vol%d", (int)i);
            pa->m_pVol.p[i] = DoubleFromControl (pPage, szTemp);

            // time
            if (i < 3) {
               swprintf (szTemp, L"time%d", (int)i);
               pa->m_pTime.p[i] = DoubleFromControl (pPage, szTemp);
            }
         }

         MeasureParseString (pPage, L"MinDist", &pa->m_fMinDist);

      }
      break;


   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;
         PWSTR psz;
         psz = p->pControl->m_pszName;

         if (!_wcsicmp(psz, L"use3d")) {
            pa->m_f3D = p->pControl->AttribGetBOOL (Checked());
            (*pa->m_pfChanged) = TRUE;

            return TRUE;
         }
         else if (!_wcsicmp(psz, L"newwave")) {
            WCHAR szTemp[256];
            szTemp[0] = 0;

            // get the filename
            PCEscControl pControl = pPage->ControlFind (L"newwavefile");
            DWORD dwNeed;
            if (pControl)
               pControl->AttribGet (Text(), szTemp, sizeof(szTemp), &dwNeed);
            if (!szTemp[0])   // get wave
               if (!WaveFileOpen (pPage->m_pWindow->m_hWnd, FALSE, szTemp))
                  return TRUE;

            // add it
            pa->m_lWave.Add (szTemp, (wcslen(szTemp)+1)*sizeof(WCHAR));
            (*pa->m_pfChanged) = TRUE;

            // redraw list
            pPage->Message (ESCM_USER+104);
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"newmusic")) {
            WCHAR szTemp[256];
            szTemp[0] = 0;

            if (!OpenMusicDialog (pPage->m_pWindow->m_hWnd, szTemp, sizeof(szTemp)/sizeof(WCHAR), FALSE))
               return TRUE;

            // add it
            pa->m_lMusic.Add (szTemp, (wcslen(szTemp)+1)*sizeof(WCHAR));
            (*pa->m_pfChanged) = TRUE;

            // redraw list
            pPage->Message (ESCM_USER+104);
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"remove")) {
            DWORD dwSel = 0, dwOrigSel = 0;
            BOOL fMusic = FALSE;
            PCEscControl pControl = pPage->ControlFind (L"randomlist");
            if (pControl)
               dwOrigSel = dwSel = (DWORD)pControl->AttribGetInt (CurSel());
            if (dwSel >= pa->m_lWave.Num()) {
               dwSel -= pa->m_lWave.Num();
               fMusic = TRUE;

               if (dwSel >= pa->m_lMusic.Num()) {
                  pPage->MBWarning (L"You must first select a sound.");
                  return TRUE;
               }
            }

            if (IDYES != pPage->MBYesNo (L"Are you sure you wish to remove the sound from the list?"))
               return TRUE;

            // delete
            if (fMusic)
               pa->m_lMusic.Remove (dwSel);
            else
               pa->m_lWave.Remove (dwSel);
            (*pa->m_pfChanged) = TRUE;

            ESCMLISTBOXDELETE del;
            memset (&del, 0, sizeof(del));
            del.dwIndex = dwOrigSel;
            pControl->Message (ESCM_LISTBOXDELETE, &del);

            return TRUE;
         }
      }
      break;


   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Random sounds";
            return TRUE;
         }
      }
      break;

   }; // dwMessage

   return FALSE;
}


/*************************************************************************************
CAmbientRandom::Edit - This brings up a dialog box for editing the object.

inputs
   PCEscWindow    pWindow - Window to display in
   BOOL           fReadOnly - If TRUE then data is read only and cant be changed
   BOOL           *pfChanged - FIlled with TRUE if the data was changed, FALSE if not
returns
   BOOL - TRUE if user pressed back, FALSE if closed
*/
BOOL CAmbientRandom::Edit (PCEscWindow pWindow, BOOL fReadOnly, BOOL *pfChanged)
{
   *pfChanged = FALSE;
   m_fReadOnly = fReadOnly;
   m_pfChanged = pfChanged;

   PWSTR psz;

redo:
   psz = pWindow->PageDialog (ghInstance, IDR_MMLAMBIENTRANDOM, AmbientRandomPage, this);
   if (!psz)
      goto done;
   if (!_wcsicmp(psz, RedoSamePage()))
      goto redo;

done:
   return (psz && !_wcsicmp(psz, Back()));
}



/*************************************************************************************
CAmbientRandom::TimeUpdate - Updates the time to the next timer and the time
to the next sample. Called from the thread proc.
*/
void CAmbientRandom::TimeUpdate (void)
{
   // increase the time to the next event
   m_fTimeToNextTimer += randf (max(m_pTime.p[0],CLOSE), max(m_pTime.p[1],CLOSE));

   // figure out when this happens including jitter
   m_fTimeToNextSound = m_fTimeToNextTimer + randf(-m_pTime.p[2], m_pTime.p[2]);
}


static PWSTR gpszVolL = L"voll";
static PWSTR gpszVolR = L"volr";
static PWSTR gpszOverlap = L"overlap";
static PWSTR gpszFadeIn = L"fadein";
static PWSTR gpszFadeOut = L"fadeout";

/*************************************************************************************
CAmbientRandom::TimeElapsed - Call this when time has elapsed. This will subtract
the time from the m_fTimerToXXX info and fill the list with any events that need to
be added.

inputs
   PCAmbient         pAmbient - Ambient object
   double            fDelta - Time elapsed since the last call
   GUID              *pgID - If an object is associated with this, then this is the object.
   PCListFixed       plEvents - Appends events (PCMMLNode2) that need to be
                        handles, such as <Wave> or <Music>. This should have
                        already been initialized
returns
   none
*/
static PWSTR gpszFile = L"File";

void CAmbientRandom::TimeElapsed (PCAmbient pAmbient, double fDelta, GUID *pgID, PCListFixed plEvents)
{
   m_fTimeToNextTimer -= fDelta;
   m_fTimeToNextSound -= fDelta;

   PCMMLNode2 pNode;

   while (m_fTimeToNextSound <= 0) {
      // play a sound

      DWORD dwNumWave = m_lWave.Num();
      DWORD dwNumMusic = m_lMusic.Num();
      DWORD dwTotal = dwNumWave + dwNumMusic;
      if (!dwTotal)
         goto donewithsound;
      dwTotal = (DWORD)rand() % dwTotal;

      // create the node
      pNode = new CMMLNode2;
      if (!pNode)
         goto donewithsound;

      if (dwTotal < dwNumWave) { // play wave
         pNode->NameSet (CircumrealityWave());
         MMLValueSet (pNode, gpszFile, (PWSTR)m_lWave.Get(dwTotal));
      }
      else {   // play music
         dwTotal -= dwNumWave;
         pNode->NameSet (CircumrealityMusic());
         MMLValueSet (pNode, gpszFile, (PWSTR)m_lMusic.Get(dwTotal));
      }

      // calculate volume or spatial information
      if (m_f3D) {
         // come up with location in 3d space
         CPoint pLoc;
         fp fLen;
         DWORD i;
         for (i = 0; i < 4; i++)
            pLoc.p[i] = randf (m_pMin.p[i], m_pMax.p[i]);
         fLen = pLoc.Length();
         m_fMinDist = max(m_fMinDist, CLOSE);
         if (fLen < m_fMinDist) {
            if (fLen < EPSILON)
               pLoc.p[1] = m_fMinDist; // to cant ever be 0
            else
               pLoc.Scale (m_fMinDist / fLen);
         }

         // add the offset
         if (pAmbient->m_pOffset.p[0] || pAmbient->m_pOffset.p[1] || pAmbient->m_pOffset.p[2])
            pLoc.Add (&pAmbient->m_pOffset);

         // set this
         MMLValueSet (pNode, L"vol3D", &pLoc);
      }
      else {
         // come up with random volume
         fp fLeft = randf (m_pVol.p[0], m_pVol.p[1]);
         fp fRight = randf (m_pVol.p[2], m_pVol.p[3]);
         if (fLeft != 1)
            MMLValueSet (pNode, gpszVolL, fLeft);
         if (fRight != 1)
            MMLValueSet (pNode, gpszVolR, fRight);
      }

      // may need to add object ID to this so know what object assocaited with
      if (pgID)
         MMLValueSet (pNode, gpszID, (PBYTE)pgID, sizeof(*pgID));

      // add to list
      plEvents->Add (&pNode);

donewithsound:
      // if got here just played a sound, so calculate the next one
      TimeUpdate();
   } // while have sounds that can play
}




/*************************************************************************************
CAmbientLoop::Constructor and destructor
*/
CAmbientLoop::CAmbientLoop (void)
{
   m_f3D = FALSE;
   m_fVolL = m_fVolR = 1;
   m_fOverlap = 2;
   m_pVol3D.Zero4();
   m_pVol3D.p[1] = 1;
   m_pVol3D.p[3] = 60;  // db

   m_dwLastState = -1;

   m_lPCAmbientLoopState.Init (sizeof(PCAmbientLoopState));
}

CAmbientLoop::~CAmbientLoop (void)
{
   DWORD i;
   PCAmbientLoopState *ppa = (PCAmbientLoopState*) m_lPCAmbientLoopState.Get(0);
   for (i = 0; i < m_lPCAmbientLoopState.Num(); i++)
      delete ppa[i];
   m_lPCAmbientLoopState.Clear();
}


static PWSTR gpszVol3D = L"vol3d";
static PWSTR gpszState = L"State";

/*************************************************************************************
CAmbientLoop::MMLTo - Standard API
*/
PCMMLNode2 CAmbientLoop::MMLTo (void)
{
   PCMMLNode2 pNode = new CMMLNode2;
   if (!pNode)
      return NULL;
   pNode->NameSet (gpszLoop);

   if (m_f3D)
      MMLValueSet (pNode, gpszVol3D, &m_pVol3D);
   else {
      if (m_fVolL != 1)
         MMLValueSet (pNode, gpszVolL, m_fVolL);
      if (m_fVolR != 1)
         MMLValueSet (pNode, gpszVolR, m_fVolR);
   }

   if (m_fOverlap)
      MMLValueSet (pNode, gpszOverlap, m_fOverlap);

   // all the states
   DWORD i;
   PCMMLNode2 pSub;
   PCAmbientLoopState *ppa = (PCAmbientLoopState*) m_lPCAmbientLoopState.Get(0);
   for (i = 0; i < m_lPCAmbientLoopState.Num(); i++) {
      pSub = ppa[i]->MMLTo();
      if (pSub)
         pNode->ContentAdd (pSub);
   }

   return pNode;
}

/*************************************************************************************
CAmbientLoop::MMLFrom - Standard API
*/
BOOL CAmbientLoop::MMLFrom (PCMMLNode2 pNode)
{
   // clear out existing
   m_f3D = FALSE;
   m_fVolL = m_fVolR = 1;
   m_pVol3D.Zero4();
   m_pVol3D.p[1] = 1;
   m_pVol3D.p[3] = 0;  // db

   DWORD i;
   PCAmbientLoopState *ppa = (PCAmbientLoopState*) m_lPCAmbientLoopState.Get(0);
   for (i = 0; i < m_lPCAmbientLoopState.Num(); i++)
      delete ppa[i];
   m_lPCAmbientLoopState.Clear();

   // get
   MMLValueGetPoint (pNode, gpszVol3D, &m_pVol3D);
   m_fVolL = MMLValueGetDouble (pNode, gpszVolL, 1);
   m_fVolR = MMLValueGetDouble (pNode, gpszVolR, 1);
   if (m_pVol3D.p[3])
      m_f3D = TRUE;
   else
      m_pVol3D.p[3] = 60;  // so have reasonable default

   m_fOverlap = MMLValueGetDouble (pNode, gpszOverlap, 0);

   PWSTR psz;
   PCMMLNode2 pSub;
   for (i = 0; i < pNode->ContentNum(); i++) {
      pSub = NULL;
      pNode->ContentEnum (i, &psz, &pSub);
      if (!pSub)
         continue;
      psz = pSub->NameGet();
      if (!psz)
         continue;
      if (!_wcsicmp(psz, gpszState)) {
         PCAmbientLoopState pNew = new CAmbientLoopState;
         if (!pNew)
            return FALSE;
         if (!pNew->MMLFrom (pSub)) {
            delete pNew;
            return FALSE;
         }
         m_lPCAmbientLoopState.Add (&pNew);
      }
   } // i

   return TRUE;
}


/*************************************************************************
AmbientLoopPage
*/
BOOL AmbientLoopPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCAmbientLoop pa = (PCAmbientLoop)pPage->m_pUserData;   // node to modify

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         // scroll to right position
         if (pa->m_iVScroll > 0) {
            pPage->VScroll (pa->m_iVScroll);

            // when bring up pop-up dialog often they're scrolled wrong because
            // iVScoll was left as valeu, and they use defpage
            pa->m_iVScroll = 0;

            // BUGFIX - putting this invalidate in to hopefully fix a refresh
            // problem when add or move a task in the ProjectView page
            pPage->Invalidate();
         }

         PCEscControl pControl;

         // set the name
         pControl = pPage->ControlFind (L"use3d");
         if (pControl) {
            pControl->AttribSetBOOL (Checked(), pa->m_f3D);
            if (pa->m_fReadOnly)
               pControl->Enable (FALSE);
         }

         // fill in values
         DWORD i;
         WCHAR szTemp[64];
         for (i = 0; i < 4; i++) {
            // min
            swprintf (szTemp, L"vol3d%d", (int)i);
            if (i == 3)
               DoubleToControl (pPage, szTemp, pa->m_pVol3D.p[i]);
            else
               MeasureToString (pPage, szTemp, pa->m_pVol3D.p[i]);
            if (pa->m_fReadOnly && (pControl = pPage->ControlFind(szTemp)))
               pControl->Enable(FALSE);
         }
         DoubleToControl (pPage, L"voll", pa->m_fVolL);
         if (pa->m_fReadOnly && (pControl = pPage->ControlFind(L"voll")))
            pControl->Enable(FALSE);
         DoubleToControl (pPage, L"volr", pa->m_fVolR);
         if (pa->m_fReadOnly && (pControl = pPage->ControlFind(L"volr")))
            pControl->Enable(FALSE);
         DoubleToControl (pPage, L"overlap", pa->m_fOverlap);
         if (pa->m_fReadOnly && (pControl = pPage->ControlFind(L"overlap")))
            pControl->Enable(FALSE);

         // show all the states
         for (i = 0; i < pa->m_lPCAmbientLoopState.Num(); i++) {
            pPage->Message (ESCM_USER+104, &i);
            pPage->Message (ESCM_USER+105, &i);
         }

         // disble
         if (pa->m_fReadOnly) {
            if (pControl = pPage->ControlFind(L"newstate"))
               pControl->Enable(FALSE);
         }
      }
      break;


   case ESCM_USER+104:  // update the listbox for audio
      {
         DWORD dwState = *((DWORD*)pParam);
         PCAmbientLoopState *ppa = (PCAmbientLoopState*)pa->m_lPCAmbientLoopState.Get (dwState);
         if (!ppa)
            return TRUE;
         PCAmbientLoopState ps = *ppa;

         // display list box entries
         ESCMLISTBOXADD lba;
         DWORD i;
         PCEscControl pControl;
         WCHAR szTemp[64];
         swprintf (szTemp, L"soundlist%d", (int)dwState);
         pControl = pPage->ControlFind (szTemp);
         if (!pControl)
            return TRUE;

         pControl->Message (ESCM_LISTBOXRESETCONTENT);

         for (i = 0; i < ps->m_lWaveMusic.Num(); i++) {
            memset (&lba, 0, sizeof(lba));
            lba.dwInsertBefore = -1;
            lba.pszText = (PWSTR)ps->m_lWaveMusic.Get(i);
            pControl->Message (ESCM_LISTBOXADD, &lba);
         } // i
      }
      return TRUE;

   case ESCM_USER+105:  // update the listbox for state branch
      {
         DWORD dwState = *((DWORD*)pParam);
         PCAmbientLoopState *ppa = (PCAmbientLoopState*)pa->m_lPCAmbientLoopState.Get (dwState);
         if (!ppa)
            return TRUE;
         PCAmbientLoopState ps = *ppa;

         // display list box entries
         ESCMLISTBOXADD lba;
         DWORD i;
         PCEscControl pControl;
         WCHAR szTemp[256];
         swprintf (szTemp, L"branchlist%d", (int)dwState);
         pControl = pPage->ControlFind (szTemp);
         if (!pControl)
            return TRUE;

         pControl->Message (ESCM_LISTBOXRESETCONTENT);

         PALSBRANCH pb = (PALSBRANCH)ps->m_lALSBRANCH.Get(0);
         for (i = 0; i < ps->m_lALSBRANCH.Num(); i++, pb++) {
            swprintf (szTemp, L"State %d", (int)pb->dwState+1);
            if (pb->szVar[0]) {
               PWSTR pszComp;
               switch (pb->iCompare) {
               case -2:
                  pszComp = L"<";
                  break;
               case -1:
                  pszComp = L"<=";
                  break;
               case 1:
                  pszComp = L">=";
                  break;
               case 2:
                  pszComp = L">";
                  break;
               default:
                  pszComp = L"==";
                  break;
               }
               swprintf (szTemp + wcslen(szTemp),
                  L" (%s %s %g)", pb->szVar, pszComp, (double)pb->fValue);
            }
            memset (&lba, 0, sizeof(lba));
            lba.dwInsertBefore = -1;
            lba.pszText = szTemp;
            pControl->Message (ESCM_LISTBOXADD, &lba);
         } // i
      }
      return TRUE;


   case ESCN_EDITCHANGE:
      {
         PESCNEDITCHANGE p = (PESCNEDITCHANGE)pParam;
         PWSTR psz = p->pControl->m_pszName;
         if (!psz)
            break;

         // just get all value
         *(pa->m_pfChanged) = TRUE;

         PWSTR pszNewWaveEdit = L"newwaveedit";
         DWORD dwNewWaveEditLen = (DWORD)wcslen(pszNewWaveEdit);
         if (!wcsncmp (psz, pszNewWaveEdit, dwNewWaveEditLen))
            return TRUE;   // ignore this for now

         // fill in values
         DWORD i;
         WCHAR szTemp[64];
         for (i = 0; i < 4; i++) {
            // min
            swprintf (szTemp, L"vol3d%d", (int)i);
            if (i == 3)
               pa->m_pVol3D.p[i] = DoubleFromControl (pPage, szTemp);
            else
               MeasureParseString (pPage, szTemp, &pa->m_pVol3D.p[i]);
         }
         pa->m_fVolL = DoubleFromControl (pPage, L"voll");
         pa->m_fVolR = DoubleFromControl (pPage, L"volr");
         pa->m_fOverlap = DoubleFromControl (pPage, L"overlap");

      }
      break;


   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;
         PWSTR psz;
         psz = p->pControl->m_pszName;

         PWSTR pszNewWave = L"newwave", pszNewMusic = L"newmusic", pszMoveUp = L"moveup",
            pszMoveDown = L"movedown", pszRemove = L"remove",
            pszNewBranch = L"newbranch", pszRemBranch = L"rembranch",
            pszFirstState = L"firststate", pszRemState = L"remstate";
         DWORD dwNewWaveLen = (DWORD)wcslen(pszNewWave),
            dwNewMusicLen = (DWORD)wcslen(pszNewMusic),
            dwMoveUpLen = (DWORD)wcslen(pszMoveUp),
            dwMoveDownLen = (DWORD)wcslen(pszMoveDown),
            dwRemoveLen = (DWORD)wcslen(pszRemove),
            dwNewBranchLen = (DWORD)wcslen(pszNewBranch),
            dwRemBranchLen = (DWORD)wcslen(pszRemBranch),
            dwFirstStateLen = (DWORD)wcslen(pszFirstState),
            dwRemStateLen = (DWORD)wcslen(pszRemState)
            ;

         if (!_wcsnicmp(psz, pszNewBranch, dwNewBranchLen)) {
            DWORD dwState = _wtoi(psz + dwNewBranchLen);
            PCAmbientLoopState ps  = *((PCAmbientLoopState*)pa->m_lPCAmbientLoopState.Get(dwState));

            ALSBRANCH ab;
            memset (&ab, 0, sizeof(ab));

            // get the sate
            WCHAR szTemp[64];
            swprintf (szTemp, L"branchstate%d", (int)dwState);
            ab.dwState = (DWORD) DoubleFromControl (pPage, szTemp) - 1;
            if (ab.dwState >= pa->m_lPCAmbientLoopState.Num()) {
               pPage->MBWarning (L"You must type in a state number.");
               return TRUE;
            }

            // variable
            swprintf (szTemp, L"branchvar%d", (int)dwState);
            PCEscControl pControl = pPage->ControlFind (szTemp);
            DWORD dwNeed;
            if (pControl)
               pControl->AttribGet (Text(), ab.szVar, sizeof(ab.szVar), &dwNeed);

            // combobox value
            swprintf (szTemp, L"branchcomp%d", (int)dwState);
            pControl = pPage->ControlFind (szTemp);
            if (pControl)
               ab.iCompare = pControl->AttribGetInt (CurSel()) - 2;

            // value
            swprintf (szTemp, L"branchval%d", (int)dwState);
            ab.fValue = DoubleFromControl (pPage, szTemp);

            // add it
            ps->m_lALSBRANCH.Add (&ab);
            (*pa->m_pfChanged) = TRUE;

            // redraw list
            pPage->Message (ESCM_USER+105, &dwState);
            return TRUE;
         }
         else if (!_wcsnicmp(psz, pszRemBranch, dwRemBranchLen)) {
            DWORD dwState = _wtoi(psz + dwRemBranchLen);
            PCAmbientLoopState ps  = *((PCAmbientLoopState*)pa->m_lPCAmbientLoopState.Get(dwState));

            DWORD dwSel = 0;
            WCHAR szTemp[64];
            swprintf (szTemp, L"branchlist%d", (int)dwState);
            PCEscControl pControl = pPage->ControlFind (szTemp);
            if (pControl)
               dwSel = (DWORD)pControl->AttribGetInt (CurSel());
            if (dwSel >= ps->m_lALSBRANCH.Num()) {
               pPage->MBWarning (L"You must first select a branch.");
               return TRUE;
            }

            if (IDYES != pPage->MBYesNo (L"Are you sure you wish to remove the branch from the list?"))
               return TRUE;

            // delete
            ps->m_lALSBRANCH.Remove (dwSel);
            (*pa->m_pfChanged) = TRUE;

            ESCMLISTBOXDELETE del;
            memset (&del, 0, sizeof(del));
            del.dwIndex = dwSel;
            pControl->Message (ESCM_LISTBOXDELETE, &del);

            return TRUE;
         }
         else if (!_wcsnicmp(psz, pszRemState, dwRemStateLen)) {
            DWORD dwState = _wtoi(psz + dwRemStateLen);
            PCAmbientLoopState *pps  = (PCAmbientLoopState*)pa->m_lPCAmbientLoopState.Get(0);

            if (IDYES != pPage->MBYesNo (L"Are you sure you wish to remove this state?"))
               return TRUE;

            // delete
            DWORD i;
            for (i = 0; i < pa->m_lPCAmbientLoopState.Num(); i++)
               pps[i]->StateRemove (dwState);

            delete pps[dwState];
            pa->m_lPCAmbientLoopState.Remove (dwState);
            (*pa->m_pfChanged) = TRUE;

            pPage->Exit (RedoSamePage());
            return TRUE;
         }
         else if (!_wcsnicmp(psz, pszFirstState, dwFirstStateLen)) {
            DWORD dwState = _wtoi(psz + dwFirstStateLen);
            PCAmbientLoopState *pps  = (PCAmbientLoopState*)pa->m_lPCAmbientLoopState.Get(0);

            PCAmbientLoopState pTemp = pps[0];
            pps[0] = pps[dwState];
            pps[dwState] = pTemp;
            (*pa->m_pfChanged) = TRUE;

            DWORD i;
            for (i = 0; i < pa->m_lPCAmbientLoopState.Num(); i++)
               pps[i]->StateSwap (0, dwState);

            pPage->Exit (RedoSamePage());
            return TRUE;
         }
         else if (!_wcsnicmp(psz, pszNewWave, dwNewWaveLen)) {
            DWORD dwState = _wtoi(psz + dwNewWaveLen);
            PCAmbientLoopState ps  = *((PCAmbientLoopState*)pa->m_lPCAmbientLoopState.Get(dwState));

            WCHAR szTemp[256];
            szTemp[0] = 0;

            // see if there's text
            WCHAR szEdit[64];
            swprintf (szEdit, L"newwaveedit%d", (int)dwState);
            PCEscControl pControl = pPage->ControlFind (szEdit);
            DWORD dwNeed;
            if (pControl)
               pControl->AttribGet (Text(), szTemp, sizeof(szTemp), &dwNeed);

            if (!szTemp[0])
               if (!WaveFileOpen (pPage->m_pWindow->m_hWnd, FALSE, szTemp))
                  return TRUE;

            // add it
            ps->m_lWaveMusic.Add (szTemp, (wcslen(szTemp)+1)*sizeof(WCHAR));
            (*pa->m_pfChanged) = TRUE;

            // redraw list
            pPage->Message (ESCM_USER+104, &dwState);
            return TRUE;
         }
         else if (!_wcsnicmp(psz, pszNewMusic, dwNewMusicLen)) {
            DWORD dwState = _wtoi(psz + dwNewMusicLen);
            PCAmbientLoopState ps  = *((PCAmbientLoopState*)pa->m_lPCAmbientLoopState.Get(dwState));

            WCHAR szTemp[256];
            szTemp[0] = 0;

            if (!OpenMusicDialog (pPage->m_pWindow->m_hWnd, szTemp, sizeof(szTemp)/sizeof(WCHAR), FALSE))
               return TRUE;

            // add it
            ps->m_lWaveMusic.Add (szTemp, (wcslen(szTemp)+1)*sizeof(WCHAR));
            (*pa->m_pfChanged) = TRUE;

            // redraw list
            pPage->Message (ESCM_USER+104, &dwState);
            return TRUE;
         }
         else if (!_wcsnicmp(psz, pszRemove, dwRemoveLen)) {
            DWORD dwState = _wtoi(psz + dwRemoveLen);
            PCAmbientLoopState ps  = *((PCAmbientLoopState*)pa->m_lPCAmbientLoopState.Get(dwState));

            DWORD dwSel = 0;
            WCHAR szTemp[64];
            swprintf (szTemp, L"soundlist%d", (int)dwState);
            PCEscControl pControl = pPage->ControlFind (szTemp);
            if (pControl)
               dwSel = (DWORD)pControl->AttribGetInt (CurSel());
            if (dwSel >= ps->m_lWaveMusic.Num()) {
               pPage->MBWarning (L"You must first select a sound.");
               return TRUE;
            }

            if (IDYES != pPage->MBYesNo (L"Are you sure you wish to remove the sound from the list?"))
               return TRUE;

            // delete
            ps->m_lWaveMusic.Remove (dwSel);
            (*pa->m_pfChanged) = TRUE;

            ESCMLISTBOXDELETE del;
            memset (&del, 0, sizeof(del));
            del.dwIndex = dwSel;
            pControl->Message (ESCM_LISTBOXDELETE, &del);

            return TRUE;
         }
         else if (!_wcsnicmp(psz, pszMoveUp, dwMoveUpLen)) {
            DWORD dwState = _wtoi(psz + dwMoveUpLen);
            PCAmbientLoopState ps  = *((PCAmbientLoopState*)pa->m_lPCAmbientLoopState.Get(dwState));

            DWORD dwSel = 0;
            WCHAR szTemp[64];
            swprintf (szTemp, L"soundlist%d", (int)dwState);
            PCEscControl pControl = pPage->ControlFind (szTemp);
            if (pControl)
               dwSel = (DWORD)pControl->AttribGetInt (CurSel());
            if (dwSel >= ps->m_lWaveMusic.Num()) {
               pPage->MBWarning (L"You must first select a sound.");
               return TRUE;
            }
            if (!dwSel) {
               pPage->MBWarning (L"You can't move the top item up.");
               return TRUE;
            }

            // move
            PWSTR psz = (PWSTR)ps->m_lWaveMusic.Get(dwSel);
            ps->m_lWaveMusic.Insert (dwSel-1, psz, (wcslen(psz)+1)*sizeof(WCHAR));
            ps->m_lWaveMusic.Remove (dwSel+1);
            (*pa->m_pfChanged) = TRUE;

            // redraw list
            pPage->Message (ESCM_USER+104, &dwState);

            return TRUE;
         }
         else if (!_wcsnicmp(psz, pszMoveDown, dwMoveDownLen)) {
            DWORD dwState = _wtoi(psz + dwMoveDownLen);
            PCAmbientLoopState ps  = *((PCAmbientLoopState*)pa->m_lPCAmbientLoopState.Get(dwState));

            DWORD dwSel = 0;
            WCHAR szTemp[64];
            swprintf (szTemp, L"soundlist%d", (int)dwState);
            PCEscControl pControl = pPage->ControlFind (szTemp);
            if (pControl)
               dwSel = (DWORD)pControl->AttribGetInt (CurSel());
            if (dwSel >= ps->m_lWaveMusic.Num()) {
               pPage->MBWarning (L"You must first select a sound.");
               return TRUE;
            }
            if (dwSel + 1 >= ps->m_lWaveMusic.Num()) {
               pPage->MBWarning (L"You can't move the bottom item down.");
               return TRUE;
            }

            // move
            PWSTR psz = (PWSTR)ps->m_lWaveMusic.Get(dwSel);
            ps->m_lWaveMusic.Insert (dwSel+2, psz, (wcslen(psz)+1)*sizeof(WCHAR));
            ps->m_lWaveMusic.Remove (dwSel);
            (*pa->m_pfChanged) = TRUE;

            // redraw list
            pPage->Message (ESCM_USER+104, &dwState);

            return TRUE;
         }
         else if (!_wcsicmp(psz, L"use3d")) {
            pa->m_f3D = p->pControl->AttribGetBOOL (Checked());
            (*pa->m_pfChanged) = TRUE;

            return TRUE;
         }
         else if (!_wcsicmp(psz, L"newstate")) {
            (*pa->m_pfChanged) = TRUE;

            PCAmbientLoopState pNew = new CAmbientLoopState;
            if (pNew)
               pa->m_lPCAmbientLoopState.Add (&pNew);

            pPage->Exit (RedoSamePage());
            return TRUE;
         }
      }
      break;


   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Looped sounds";
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"STATESUB")) {
            MemZero (&gMemTemp);

            DWORD i;
            for (i = 0; i < pa->m_lPCAmbientLoopState.Num(); i++) {
               MemCat (&gMemTemp, L"<xtablecenter width=100%>"
                  L"<xtrheader>State #");
               MemCat (&gMemTemp, (int)i+1);
               if (i == 0)
                  MemCat (&gMemTemp, L" (Starting state)");
               MemCat (&gMemTemp,
                  L"</xtrheader>"
                  L"<xtrheader>Sounds played</xtrheader>"
                  L"<tr><td>"
                  L"<p align=center>"
                  L"<listbox width=80% height=20% vscroll=soundlistscroll");
               MemCat (&gMemTemp, (int)i);
               MemCat (&gMemTemp,
                  L" name=soundlist");
               MemCat (&gMemTemp, (int)i);
               MemCat (&gMemTemp,
                  L"/>"
                  L"<scrollbar orient=vert height=20% name=soundlistscroll");
               MemCat (&gMemTemp, (int)i);
               MemCat (&gMemTemp,
                  L"/></p>");
               if (!pa->m_fReadOnly) {
                  MemCat (&gMemTemp,
                     L"<table width=100% border=0 innerlines=0>");

                  MemCat (&gMemTemp,
                     L"<tr>");

                  MemCat (&gMemTemp,
                     L"<td><xChoiceButton name=newwave");
                  MemCat (&gMemTemp, (int)i);
                  MemCat (&gMemTemp,
                     L">"
                     L"<bold>Add a new sound (.wav file)</bold><br/>"
                     L"This adds a new sound to be played. A file select dialog box will "
                     L"be shown unless you type in a file name below."
                     L"</xChoiceButton>"
                     L"<align align=right><edit width=66% maxchars=250 name=newwaveedit");
                  MemCat (&gMemTemp, (int)i);
                  MemCat (&gMemTemp,
                     L"/></align>"
                     L"</td>");

                  MemCat (&gMemTemp,
                     L"<td><xChoiceButton style=uptriangle name=moveup");
                  MemCat (&gMemTemp, (int) i);
                  MemCat (&gMemTemp,
                     L">"
                     L"<bold>Move sound up</bold><br/>"
                     L"Moves the sound up in the list."
                     L"</xChoiceButton></td>");

                  MemCat (&gMemTemp,
                     L"</tr>"
                     L"<tr>");

                  MemCat (&gMemTemp,
                     L"<td><xChoiceButton name=newmusic");
                  MemCat (&gMemTemp, (int)i);
                  MemCat (&gMemTemp,
                     L">"
                     L"<bold>Add a new sound (.mid file)</bold><br/>"
                     L"This adds a new sound to be played."
                     L"</xChoiceButton></td>");


                  MemCat (&gMemTemp,
                     L"<td><xChoiceButton style=downtriangle name=movedown");
                  MemCat (&gMemTemp, (int)i);
                  MemCat (&gMemTemp,
                     L">"
                     L"<bold>Move sound down</bold><br/>"
                     L"Moves the sound down in the list."
                     L"</xChoiceButton></td>");

                  MemCat (&gMemTemp,
                     L"</tr>"
                     L"<tr>");

                  MemCat (&gMemTemp,
                     L"<td><xChoiceButton name=remove");
                  MemCat (&gMemTemp, (int) i);
                  MemCat (&gMemTemp,
                     L">"
                     L"<bold>Delete the selected sound</bold><br/>"
                     L"This deletes the selected sound from the list."
                     L"</xChoiceButton></td>"
                     );

                  MemCat (&gMemTemp,
                     L"</tr>"
                     L"</table>");
               }

               MemCat (&gMemTemp,
                  L"</td></tr>"
                  L"<xtrheader>Branches</xtrheader>"
                  L"<tr><td>"
                  L"<p align=center>"
                  L"<listbox width=80% height=20% vscroll=branchlistscroll");
               MemCat (&gMemTemp, (int)i);
               MemCat (&gMemTemp,
                  L" name=branchlist");
               MemCat (&gMemTemp, (int)i);
               MemCat (&gMemTemp,
                  L"/>"
                  L"<scrollbar orient=vert height=20% name=branchlistscroll");
               MemCat (&gMemTemp, (int)i);
               MemCat (&gMemTemp,
                  L"/>"
                  L"</p>");

               if (!pa->m_fReadOnly) {
                  MemCat (&gMemTemp,
                     L"<xChoiceButton name=newbranch");
                  MemCat (&gMemTemp, (int)i);
                  MemCat (&gMemTemp,
                     L">"
                     L"<bold>Add a new branch</bold><br/>"
                     L"This adds a new branch to the state. Make sure to fill in the information below:"
                     L"</xChoiceButton>"
                     L"<p align=right>"
                     L"<bold>State number to branch to: <edit width=33% maxchars=32 name=branchstate");
                  MemCat (&gMemTemp, (int)i);
                  MemCat (&gMemTemp,
                     L"/></bold>"
                     L"<br/>"
                     L"(Optional) Only if <bold>variable: <edit width=33% maxchars=60 name=branchvar");
                  MemCat (&gMemTemp, (int)i);
                  MemCat (&gMemTemp,
                     L"/></bold>"
                     L"<br/>"
                     L"(Optional) <bold>is: <xComboCompare name=branchcomp");
                  MemCat (&gMemTemp, (int)i);
                  MemCat (&gMemTemp,
                     L"/></bold>"
                     L"<br/>"
                     L"(Optional) <bold>value: <edit width=33% maxchars=64 name=branchval");
                  MemCat (&gMemTemp, (int)i);
                  MemCat (&gMemTemp,
                     L"/></bold>"
                     L"</p>"
                     L"<xChoiceButton name=rembranch");
                  MemCat (&gMemTemp, (int)i);
                  MemCat (&gMemTemp,
                     L">"
                     L"<bold>Delete the selected branch</bold><br/>"
                     L"This deletes the selected branch from the list."
                     L"</xChoiceButton>");
               }

               MemCat (&gMemTemp,
                  L"</td></tr>");
	
               if (!pa->m_fReadOnly) {
                  MemCat (&gMemTemp,
                     L"<xtrheader>Miscellaneous</xtrheader>"
                     L"<tr><td>");

                  if (i) {
                     MemCat (&gMemTemp,
                        L"<xChoiceButton name=firststate");
                     MemCat (&gMemTemp, (int)i);
                     MemCat (&gMemTemp,
                        L">"
                        L"<bold>Make this the starting state</bold><br/>"
                        L"Moves this to the top of the state list so it's the starting state."
                        L"</xChoiceButton>");
                  }

                  MemCat (&gMemTemp,
                     L"<xChoiceButton name=remstate");
                  MemCat (&gMemTemp, (int)i);
                  MemCat (&gMemTemp,
                     L">"
                     L"<bold>Delete this state</bold><br/>"
                     L"Deletes the current state from loop."
                     L"</xChoiceButton>"
                     L"</td></tr>");
               }

               MemCat (&gMemTemp,
                  L"</xtablecenter>");
            } // i


            p->pszSubString = (PWSTR) gMemTemp.p;
            return TRUE;
         }
      }
      break;

   }; // dwMessage

   return FALSE;
}



/*************************************************************************************
CAmbientLoop::Edit - This brings up a dialog box for editing the object.

inputs
   PCEscWindow    pWindow - Window to display in
   BOOL           fReadOnly - If TRUE then data is read only and cant be changed
   BOOL           *pfChanged - FIlled with TRUE if the data was changed, FALSE if not
returns
   BOOL - TRUE if user pressed back, FALSE if closed
*/
BOOL CAmbientLoop::Edit (PCEscWindow pWindow, BOOL fReadOnly, BOOL *pfChanged)
{
   *pfChanged = FALSE;
   m_fReadOnly = fReadOnly;
   m_pfChanged = pfChanged;
   m_iVScroll = 0;

   PWSTR psz;

redo:
   psz = pWindow->PageDialog (ghInstance, IDR_MMLAMBIENTLOOP, AmbientLoopPage, this);
   m_iVScroll = pWindow->m_iExitVScroll;
   if (!psz)
      goto done;
   if (!_wcsicmp(psz, RedoSamePage()))
      goto redo;

done:
   return (psz && !_wcsicmp(psz, Back()));
}




/*************************************************************************************
CAmbientLoop::LoopWhatsNext - Given the current loop, advances forward and finds out
what the next audio events are. These are filled into plEvents

inputs
   PCAmbient         pAmbient - Ambient object
   GUID              *pgID - GUID to associate with the sound, or NULL if none
   PCBTree           ptAmbientLoopVar - Tree indexed by a var name, with doubles as
                     the values. Used for testing divergence based on values.
   PCListFixed       plEvents - Appends events (PCMMLNode2) that need to be
                        handles, such as <Wave> or <Music>. This should have
                        already been initialized
returns
   BOOL - TRUE if success, FALSE if cant find the loop
*/
BOOL CAmbientLoop::LoopWhatsNext (PCAmbient pAmbient, GUID *pgID, PCBTree ptAmbientLoopVar, PCListFixed plEvents)
{
   PCAmbientLoopState *ppa = (PCAmbientLoopState*) m_lPCAmbientLoopState.Get(0);
   PCAmbientLoopState pa;

   // find the next state
   DWORD dwTries;
   CListFixed lMatch;
   lMatch.Init (sizeof(DWORD));
   DWORD i;
   for (dwTries = 0; dwTries < 10; dwTries++) {
      if (m_dwLastState == (DWORD)-1) {
         m_dwLastState = 0;
         break;
      }

      // if out of range error
      if (m_dwLastState >= m_lPCAmbientLoopState.Num())
         return FALSE;

      pa = ppa[m_dwLastState];

      // figure out which paths can take
      lMatch.Clear();
      PALSBRANCH pab = (PALSBRANCH)pa->m_lALSBRANCH.Get(0);
      for (i = 0; i < pa->m_lALSBRANCH.Num(); i++, pab++) {
         if (pab->szVar[0]) {
            // do test to see if its variable matches
            double *pf = (double*) ptAmbientLoopVar->Find (pab->szVar);
            double f = pf ? *pf : 0;

            BOOL fPass;
            switch (pab->iCompare) {
            case -2:
               fPass = (f < pab->fValue);
               break;
            case -1:
               fPass = (f <= pab->fValue);
               break;
            case 1:
               fPass = (f >= pab->fValue);
               break;
            case 2:
               fPass = (f >= pab->fValue);
               break;
            default:
               fPass = (f == pab->fValue);
               break;
            }

            if (!fPass)
               continue;   // no match
         }

         // if get here then add
         lMatch.Add (&pab->dwState);
      } // i

      if (!lMatch.Num()) {
         // got to the end
         m_dwLastState = -2;
         return FALSE;
      }

      // else random
      DWORD *pdw = (DWORD*)lMatch.Get((DWORD)rand() % lMatch.Num());
      m_dwLastState = *pdw;

      // if there is audio then break, else loop again
      if (m_dwLastState >= m_lPCAmbientLoopState.Num())
         return FALSE;
      pa = ppa[m_dwLastState];
      if (pa->m_lWaveMusic.Num())
         break;
   } // for
   if (dwTries >= 10) {
      // got into some sort of infinite loop, so exit
      m_dwLastState = -2;
      return FALSE;
   }


   // if get here then have a match and audio
   if (m_dwLastState >= m_lPCAmbientLoopState.Num())
      return FALSE;
   pa = ppa[m_dwLastState];
   PCMMLNode2 pNode;
   PWSTR pszWave = L".wav";
   DWORD dwWaveLen = (DWORD)wcslen(pszWave);
   DWORD dwNum = pa->m_lWaveMusic.Num();
   for (i = 0; i < dwNum; i++) {
      // create the node
      pNode = new CMMLNode2;
      if (!pNode)
         return FALSE;

      PWSTR psz = (PWSTR)pa->m_lWaveMusic.Get(i);
      if (!psz || !psz[0])
         continue;
      DWORD dwLen = (DWORD)wcslen(psz);
      if ((dwLen >= dwWaveLen) && !_wcsicmp(pszWave, psz + (dwLen-dwWaveLen)))
         pNode->NameSet (CircumrealityWave());
      else
         pNode->NameSet (CircumrealityMusic());

      MMLValueSet (pNode, gpszFile, psz);

      // calculate volume or spatial information
      if (m_f3D) {
         CPoint p;
         p.Copy (&m_pVol3D);
         if (pAmbient->m_pOffset.p[0] || pAmbient->m_pOffset.p[1] || pAmbient->m_pOffset.p[2])
            p.Add (&pAmbient->m_pOffset);

         MMLValueSet (pNode, L"vol3D", &p);
      }
      else {
         // come up with volume
         if (m_fVolL != 1)
            MMLValueSet (pNode, gpszVolL, m_fVolL);
         if (m_fVolR != 1)
            MMLValueSet (pNode, gpszVolR, m_fVolR);
      }
      
      // write the fade in and out values, but only at start and end
      if (!i && (m_fOverlap > 0))
         MMLValueSet (pNode, gpszFadeIn, m_fOverlap);
      if ((i+1 >= dwNum) && (m_fOverlap > 0))
         MMLValueSet (pNode, gpszFadeOut, m_fOverlap);

      // may need to add object ID to this so know what object assocaited with
      if (pgID)
         MMLValueSet (pNode, gpszID, (PBYTE)pgID, sizeof(*pgID));

      // add to list
      plEvents->Add (&pNode);
   } // i


   return TRUE;
}






/*************************************************************************************
CAmbientLoopState::Constructor and destructor
*/
CAmbientLoopState::CAmbientLoopState (void)
{
   m_lALSBRANCH.Init (sizeof(ALSBRANCH));
}

CAmbientLoopState::~CAmbientLoopState (void)
{
   // do nothing for now
}




/*************************************************************************************
CAmbientLoopState::StateRemove - Causes an references to dwState to be removed.
Any refrences to states higher than dwState will have one subtracted from them.

inputs
   DWORD             dwState - State that's removed
*/
void CAmbientLoopState::StateRemove (DWORD dwState)
{
   DWORD i;
   PALSBRANCH pa = (PALSBRANCH)m_lALSBRANCH.Get(0);
   for (i = m_lALSBRANCH.Num()-1; i < m_lALSBRANCH.Num(); i--) {
      if (pa[i].dwState < dwState)
         continue;
      else if (pa[i].dwState > dwState) {
         pa[i].dwState--;
         continue;
      }

      // else, delete
      m_lALSBRANCH.Remove (i);
      pa = (PALSBRANCH)m_lALSBRANCH.Get(0);
   } // i
}


/*************************************************************************************
CAmbientLoopState::StateSwap - Swaps dwStateA with dwStateB

inputs
   DWORD          dwStateA - A state number
   DWORD          dwStateB - A state number
*/
void CAmbientLoopState::StateSwap (DWORD dwStateA, DWORD dwStateB)
{
   DWORD i;
   PALSBRANCH pa = (PALSBRANCH)m_lALSBRANCH.Get(0);
   for (i = 0; i < m_lALSBRANCH.Num(); i++) {
      if (pa[i].dwState == dwStateA)
         pa[i].dwState = dwStateB;
      else if (pa[i].dwState == dwStateB)
         pa[i].dwState = dwStateA;
   } // i
}


static PWSTR gpszBranch = L"Branch";
static PWSTR gpszOper = L"oper";
static PWSTR gpszVal = L"val";
static PWSTR gpszVar = L"var";

/*************************************************************************************
CAmbientLoopState::MMLTo - Standard API
*/
PCMMLNode2 CAmbientLoopState::MMLTo (void)
{
   PCMMLNode2 pNode = new CMMLNode2;
   if (!pNode)
      return NULL;
   pNode->NameSet (gpszState);

   // write out the files
   PWSTR pszWave = L".wav";
   DWORD dwWaveLen = (DWORD)wcslen(pszWave);
   DWORD i, dwLen;
   PWSTR psz;
   PCMMLNode2 pSub;
   for (i = 0; i < m_lWaveMusic.Num(); i++) {
      psz = (PWSTR)m_lWaveMusic.Get(i);
      if (!psz || !psz[0])
         continue;
      dwLen = (DWORD)wcslen(psz);

      pSub = pNode->ContentAddNewNode ();
      if (!pSub)
         continue;
      if ((dwLen >= dwWaveLen) && !_wcsicmp(pszWave, psz + (dwLen-dwWaveLen)))
         pSub->NameSet (gpszWave);
      else
         pSub->NameSet (gpszMusic);

      pSub->AttribSetString (gpszV, psz);
   } // i

   // write out the branches
   PALSBRANCH pa = (PALSBRANCH)m_lALSBRANCH.Get(0);
   for (i = 0; i < m_lALSBRANCH.Num(); i++, pa++) {
      pSub = pNode->ContentAddNewNode ();
      if (!pSub)
         continue;

      pSub->NameSet (gpszBranch);
      pSub->AttribSetInt (gpszV, (int)pa->dwState);
      
      // compare?
      if (pa->szVar[0]) {
         pSub->AttribSetString (gpszVar, pa->szVar);
         pSub->AttribSetInt (gpszOper, (int)pa->iCompare);
         pSub->AttribSetDouble (gpszVal, pa->fValue);
      }
   } // i


   return pNode;
}

/*************************************************************************************
CAmbientLoopState::MMLFrom - Standard API
*/
BOOL CAmbientLoopState::MMLFrom (PCMMLNode2 pNode)
{
   // clear out
   m_lWaveMusic.Clear();
   m_lALSBRANCH.Clear();

   PWSTR psz;
   DWORD i;
   PCMMLNode2 pSub;
   for (i = 0; i < pNode->ContentNum(); i++) {
      pSub = NULL;
      pNode->ContentEnum (i, &psz, &pSub);
      if (!pSub)
         continue;
      psz = pSub->NameGet();
      if (!psz)
         continue;

      if (!_wcsicmp(psz, gpszWave) || !_wcsicmp(psz, gpszMusic)) {
         psz = pSub->AttribGetString (gpszV);
         if (!psz)
            continue;
         m_lWaveMusic.Add (psz, (wcslen(psz)+1)*sizeof(WCHAR));
      }
      else if (!_wcsicmp(psz, gpszBranch)) {
         ALSBRANCH b;
         int iVal;
         memset (&b, 0, sizeof(b));
         if (!pSub->AttribGetInt (gpszV, &iVal))
            continue;
         b.dwState = (DWORD)iVal;

         psz = pSub->AttribGetString (gpszVar);
         if (psz && psz[0] && (wcslen(psz)+1 < sizeof(b.szVar)/sizeof(WCHAR))) {
            wcscpy (b.szVar, psz);
            if (!pSub->AttribGetInt (gpszOper, &b.iCompare))
               b.iCompare = 0;
            if (!pSub->AttribGetDouble (gpszVal, &b.fValue))
               b.fValue = 0;
         }
         
         m_lALSBRANCH.Add (&b);
      }
   } // i

   return TRUE;
}

