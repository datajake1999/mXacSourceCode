/*************************************************************************************
ResCutScene.cpp - Code for the CutScene resource.

begun 28/6/06 by Mike Rozak.
Copyright 2006 by Mike Rozak. All rights reserved
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

// RCSD - Information for OpenCutSceneDialog
typedef struct {
   PCMMLNode2  pNode;

   PCListVariable plTransPros;   // list of transplanted prosody resoruce
   PCHashString   phTransPros;   // hash for transplanted prosody resoruce
   PCListVariable plVoice;       // voice to use
   PCHashString   phVoice;       // voice to use
   PCListVariable plObjects;     // list of objects, as well as NPCAsNarrator
   PCHashString   phObjects;     // like plObjects
   PCListVariable plImages;      // list of image resources
   PCHashString   phImages;      // like plObjects
   PCListVariable plRooms;       // list of room objects
   PCHashString   phRooms;       // like plRooms
   PCListVariable plSounds;      // list of sound resources
   PCHashString   phSounds;      // like plObjects
   PCListVariable plAmbient;      // list of sound resources
   PCHashString   phAmbient;      // like plObjects
   LANGID         lid;           // language
   PCMIFLLib      pLib;          // library it's in
   PCMIFLResource pRes;          // resource it's in

   int         iVScroll;         // where to scroll to
   BOOL        fReadOnly;        // set to TRUE if is read only file
   BOOL        fChanged;         // set to TRUE if changed
} RCSD, *PRCSD;


static PWSTR gapszActionType[] = {L"Speak", L"Sound", L"Pause", L"Ambient", L"NPCEmote", L"Image",
   L"Room"};
PWSTR gapszSpeakStyle[10] = {L"afraid", L"drunk", L"halfwhisper", L"happy",
   L"loud", L"normal", L"quiet", L"sad", L"whisper", L"yell"};

// speak params
static PWSTR gpszText = L"Text";
static PWSTR gpszTransPros = L"TransPros";
static PWSTR gpszVoice = L"Voice";  // voice to use
static PWSTR gpszSpeakStyle = L"SpeakStyle"; // speaking style
static PWSTR gpszLang = L"Lang"; // fictional language to use
static PWSTR gpszNoLipSync = L"NoLipSync"; // to to TRUE if lip sync turned off

// NPCEmote
static PWSTR gpszObject = L"Object";   // object to display
static PWSTR gpszEmote = L"Emote";  // emote to display
static PWSTR gpszFade = L"Fade";    // fade time, in seconds
static PWSTR gpszLookDir = L"LookDir"; // look direction, -1 for left, 0 for center, 1 for right

// rooms
static PWSTR gpszRoom = L"Room";  // Room to display
static PWSTR gpszDateTime = L"DateTime";  // DateTime to display
static PWSTR gpszHeight = L"Height";  // Height to display
static PWSTR gpszLRAngle = L"LRAngle";  // LRAngle to display
static PWSTR gpszUDAngle = L"UDAngle";  // UDAngle to display
static PWSTR gpszFOV = L"FOV";  // FOV to display

// Pause
static PWSTR gpszTime = L"Time";    // puase time, in seconds

// Image
static PWSTR gpszResource = L"Resource";    // resource to display
static PWSTR gpszDisableNPR = L"DisableNPR";    // if set to 1 then NPR disabled
//static PWSTR gpszFade = L"Fade";    // fade time, in seconds

// sound
static PWSTR gpszWaitSound = L"WaitSound";    // if set to 1 then wait for sound to continue
//static PWSTR gpszResource = L"Resource";    // resource to display

// ambient
// static PWSTR gpszResource = L"Resource";    // resource to display

/************************************************************************************************
IsObjectDerivedFromClass - Checks to see if an object is ultimately derived from a given class.

inputs
   PCMIFLProj        pProj - Project
   PWSTR             pszObject - Object that looking for
   PWSTR             pszClass - Class that seeing if derived from
   DWORD             dwRecurse - Recurse count. Start at 0. If get to 10 then stops.
returns
   BOOL - TRUE if it is.
*/
BOOL IsObjectDerivedFromClass (PCMIFLProj pProj, PWSTR pszObject, PWSTR pszClass, DWORD dwRecurse)
{
   // if pszObject and pszClass are the same then success
   if (!_wcsicmp(pszObject, pszClass))
      return TRUE;

   // make sure don't recurse
   if (dwRecurse >= 10)
      return FALSE;

   // loop through all the libraries
   DWORD dwLib;
   for (dwLib = 0; dwLib < pProj->LibraryNum(); dwLib++) {
      PCMIFLLib pLib = pProj->LibraryGet(dwLib);

      // try to find the object
      DWORD dwIndex = pLib->ObjectFind (pszObject, (DWORD)-1);
      if (dwIndex == (DWORD)-1)
         continue;   // not in this library
      PCMIFLObject pObj = pLib->ObjectGet (dwIndex);

      DWORD dwClass;
      for (dwClass = 0; dwClass < pObj->m_lClassSuper.Num(); dwClass++)
         if (IsObjectDerivedFromClass (pProj, (PWSTR)pObj->m_lClassSuper.Get(dwClass), pszClass, dwRecurse+1))
            return TRUE;
   } // dwLib

   return FALSE;
}



/*************************************************************************
PrependOptionToList - Prepends an options to a list generated by
FillListWithResources()

inputs
   PWSTR          pszOption - Option
   PCListVariable pl - To be inserted before
   PCHashString   ph - To be inserted before
returns
   none
*/
void PrependOptionToList (PWSTR pszOption, PCListVariable pl, PCHashString ph)
{
   if (ph->Find (pszOption))
      return;  // already there

   MemZero (&gMemTemp);
   MemCat (&gMemTemp, pszOption);
   gMemTemp.CharCat (0);   // extra null termination
   gMemTemp.CharCat (0);   // extra null termination

   // insert at start of list
   pl->Insert (0, gMemTemp.p, gMemTemp.m_dwCurPosn);

   // move all hash pointers down
   DWORD *pdw;
   DWORD i;
   for (i = 0; i < ph->Num(); i++) {
      pdw = (DWORD*)ph->Get(i);
      *pdw = *pdw + 1;
   } // i

   // add new item
   i = 0;
   ph->Add (pszOption, &i);
}

/*************************************************************************
FillListWithResources - Fills a list with resoruces of the given type.

inputs
   PWSTR*         papszResourceType - Type of resorce to look for, such as "TransPros". Can be NULL
   DWORD          dwNumResourceType - Number of resource types to look for
   PWSTR          *papszClass - Objects of this class are included, list of them. Can be NULL.
   DWORD          dwNumClass - Number of classes
   PCMIFLProj     pProj - Project
   PCListVariable pl - To be filled
   PCHashString   ph - Hash, indexed from string to DWORD index number
returns
   none
*/
typedef struct {
   PWSTR          psz;     // to sort by
   DWORD          dwType;  // 0 for resource, 1 for class
   PVOID          pData;   // data for the string
} FLWR, *PFLWR;


static int __cdecl FLWRSortCallback (const void *elem1, const void *elem2 )
{
   FLWR *p1, *p2;
   p1 = (PFLWR) elem1;
   p2 = (PFLWR) elem2;

   return _wcsicmp(p1->psz, p2->psz);
}


void FillListWithResources (PWSTR *papszResourceType, DWORD dwNumResourceType,
                            PWSTR *papszClass, DWORD dwNumClass, PCMIFLProj pProj, PCListVariable pl, PCHashString ph)
{
   CListFixed lFLWR;
   lFLWR.Init (sizeof(FLWR));
   pl->Clear();
   ph->Init (sizeof(DWORD));

   // loop, filling in at first
   DWORD i, j, k;
   FLWR flwr;
   memset (&flwr, 0, sizeof(flwr));

   // resources
   PWSTR psz;
   PCMIFLLib pLib;
   if (papszResourceType) for (i = 0; i < pProj->LibraryNum(); i++) {
      pLib = pProj->LibraryGet (i);
      for (j = 0; j < pLib->ResourceNum(); j++) {
         PCMIFLResource pRes = pLib->ResourceGet (j);

         // get the type
         psz = (PWSTR) pRes->m_memType.p;
         for (k = 0; k < dwNumResourceType; k++)
            if (!_wcsicmp(psz, papszResourceType[k]))
               break;   // wrong type
         if (k >= dwNumResourceType)
            continue;   // none of the resources match

         // else, found right type, make sure the specific name doesn't arleady exist
         psz = (PWSTR)pRes->m_memName.p;
         if (ph->Find(psz))
            continue;   // exits

         // else, add
         ph->Add (psz, &i);  // to has so dont add multiple times
         flwr.pData = pRes;
         flwr.psz = psz;
         flwr.dwType = 0;
         lFLWR.Add (&flwr);
      } // j
   } // i

   // classes
   if (papszClass && dwNumClass) for (i = 0; i < pProj->LibraryNum(); i++) {
      pLib = pProj->LibraryGet (i);
      for (j = 0; j < pLib->ObjectNum(); j++) {
         PCMIFLObject pObj = pLib->ObjectGet(j);

         // make sure instantiated
         if (!pObj->m_fAutoCreate)
            continue;

         // if already on the list then dont bother
         psz = (PWSTR)pObj->m_memName.p;
         if (ph->Find(psz))
            continue;   // exits

         // make sure it's derived from the given class
         for (k = 0; k < dwNumClass; k++)
            if (IsObjectDerivedFromClass (pProj, psz, papszClass[k]))
               break;
         if (k >= dwNumClass)
            continue;

         // else, add
         ph->Add (psz, &i);  // to has so dont add multiple times
         flwr.pData = pObj;
         flwr.psz = psz;
         flwr.dwType = 1;
         lFLWR.Add (&flwr);
      } // j
   } // i

   // sort alphabetically
   qsort (lFLWR.Get(0), lFLWR.Num(), sizeof(FLWR), FLWRSortCallback);

   // rearrange
   PFLWR pflwr = (PFLWR)lFLWR.Get(0);
   for (i = 0; i < lFLWR.Num(); i++, pflwr++) {
      DWORD *pdw = (DWORD*)ph->Find (pflwr->psz);
      if (pdw)
         *pdw = i;   // new location

      // add to the list
      size_t dwCur;
      switch (pflwr->dwType) {
      case 0:  // resource
         {
            PCMIFLResource pRes = (PCMIFLResource) pflwr->pData;
            MemZero (&gMemTemp);
            MemCat (&gMemTemp, (PWSTR)pRes->m_memName.p);
            dwCur = gMemTemp.m_dwCurPosn;
            MemCat (&gMemTemp, L"a");  // so have something to change
            MemCat (&gMemTemp, (PWSTR)pRes->m_memDescShort.p);
            ((PWSTR)gMemTemp.p)[dwCur/sizeof(WCHAR)] = 0;   // double null terminate
            pl->Add (gMemTemp.p, gMemTemp.m_dwCurPosn);
         }
         break;

      case 1:  // object/class
         {
            PCMIFLObject pObj = (PCMIFLObject) pflwr->pData;
            MemZero (&gMemTemp);
            MemCat (&gMemTemp, (PWSTR)pObj->m_memName.p);
            dwCur = gMemTemp.m_dwCurPosn;
            MemCat (&gMemTemp, L"a");  // so have something to change
            MemCat (&gMemTemp, (PWSTR)pObj->m_memDescShort.p);
            ((PWSTR)gMemTemp.p)[dwCur/sizeof(WCHAR)] = 0;   // double null terminate
            pl->Add (gMemTemp.p, gMemTemp.m_dwCurPosn);
         }
         break;
      } // switch
   } // i
}


/*************************************************************************
ResCutScenePage
*/
BOOL ResCutScenePage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PRCSD pRCSD = (PRCSD)pPage->m_pUserData;   // node to modify

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         TTSCacheDefaultGet (pPage, L"ttsfile");

         // scroll to right position
         if (pRCSD->iVScroll > 0) {
            pPage->VScroll (pRCSD->iVScroll);

            // when bring up pop-up dialog often they're scrolled wrong because
            // iVScoll was left as valeu, and they use defpage
            pRCSD->iVScroll = 0;

            // BUGFIX - putting this invalidate in to hopefully fix a refresh
            // problem when add or move a task in the ProjectView page
            pPage->Invalidate();
         }

      }
      break;

   case ESCN_EDITCHANGE:
      {
         PESCNEDITCHANGE p = (PESCNEDITCHANGE)pParam;
         PWSTR psz = p->pControl->m_pszName;
         if (!psz)
            break;

         PWSTR pszText = L"text:", pszLang = L"lang:",
            pszEmote = L"emote:", pszFade = L"fade:", pszTime = L"time:",
            pszDateTime = L"datetime:", pszHeight = L"height:",
            pszLRAngle = L"lrangle:", pszUDAngle = L"udangle:", pszFOV = L"fov:";
         DWORD dwTextLen = (DWORD)wcslen(pszText), dwLangLen = (DWORD)wcslen(pszLang),
            dwEmoteLen = (DWORD)wcslen(pszEmote), dwFadeLen = (DWORD)wcslen(pszFade), dwTimeLen = (DWORD)wcslen(pszTime),
            dwDateTimeLen = (WORD)wcslen(pszDateTime), dwHeightLen = (DWORD)wcslen(pszHeight),
            dwLRAngleLen = (DWORD)wcslen(pszLRAngle), dwUDAngleLen = (DWORD)wcslen(pszUDAngle),
            dwFOVLen = (DWORD)wcslen(pszFOV);
         DWORD dwIndex;

         WCHAR szTemp[1024];
         DWORD dwNeed;
         if (!wcsncmp(psz, pszText, dwTextLen)) {
            dwIndex = (DWORD)_wtoi(psz + dwTextLen);
            PCMMLNode2 pSub;
            pRCSD->pNode->ContentEnum (dwIndex, &psz, &pSub);
            if (!pSub)
               return TRUE;   // shouldnt happen

            szTemp[0] = 0;
            p->pControl->AttribGet (Text(), szTemp, sizeof(szTemp), &dwNeed);

            if (szTemp[0])
               pSub->AttribSetString (gpszText, szTemp);
            else
               pSub->AttribDelete (gpszText);

            pRCSD->fChanged = TRUE;
            //pPage->Exit (RedoSamePage());
            return TRUE;
         }
         else if (!wcsncmp(psz, pszLang, dwLangLen)) {
            dwIndex = (DWORD)_wtoi(psz + dwLangLen);
            PCMMLNode2 pSub;
            pRCSD->pNode->ContentEnum (dwIndex, &psz, &pSub);
            if (!pSub)
               return TRUE;   // shouldnt happen

            szTemp[0] = 0;
            p->pControl->AttribGet (Text(), szTemp, sizeof(szTemp), &dwNeed);

            if (szTemp[0])
               pSub->AttribSetString (gpszLang, szTemp);
            else
               pSub->AttribDelete (gpszLang);

            pRCSD->fChanged = TRUE;
            //pPage->Exit (RedoSamePage());
            return TRUE;
         }
         else if (!wcsncmp(psz, pszEmote, dwEmoteLen)) {
            dwIndex = (DWORD)_wtoi(psz + dwEmoteLen);
            PCMMLNode2 pSub;
            pRCSD->pNode->ContentEnum (dwIndex, &psz, &pSub);
            if (!pSub)
               return TRUE;   // shouldnt happen

            szTemp[0] = 0;
            p->pControl->AttribGet (Text(), szTemp, sizeof(szTemp), &dwNeed);

            if (szTemp[0])
               pSub->AttribSetString (gpszEmote, szTemp);
            else
               pSub->AttribDelete (gpszEmote);

            pRCSD->fChanged = TRUE;
            //pPage->Exit (RedoSamePage());
            return TRUE;
         }
         else if (!wcsncmp(psz, pszDateTime, dwDateTimeLen)) {
            dwIndex = (DWORD)_wtoi(psz + dwDateTimeLen);
            PCMMLNode2 pSub;
            pRCSD->pNode->ContentEnum (dwIndex, &psz, &pSub);
            if (!pSub)
               return TRUE;   // shouldnt happen

            szTemp[0] = 0;
            p->pControl->AttribGet (Text(), szTemp, sizeof(szTemp), &dwNeed);

            if (szTemp[0])
               pSub->AttribSetString (gpszDateTime, szTemp);
            else
               pSub->AttribDelete (gpszDateTime);

            pRCSD->fChanged = TRUE;
            //pPage->Exit (RedoSamePage());
            return TRUE;
         }
         else if (!wcsncmp(psz, pszHeight, dwHeightLen)) {
            dwIndex = (DWORD)_wtoi(psz + dwHeightLen);
            PCMMLNode2 pSub;
            pRCSD->pNode->ContentEnum (dwIndex, &psz, &pSub);
            if (!pSub)
               return TRUE;   // shouldnt happen

            szTemp[0] = 0;
            p->pControl->AttribGet (Text(), szTemp, sizeof(szTemp), &dwNeed);

            if (szTemp[0])
               pSub->AttribSetString (gpszHeight, szTemp);
            else
               pSub->AttribDelete (gpszHeight);

            pRCSD->fChanged = TRUE;
            //pPage->Exit (RedoSamePage());
            return TRUE;
         }
         else if (!wcsncmp(psz, pszLRAngle, dwLRAngleLen)) {
            dwIndex = (DWORD)_wtoi(psz + dwLRAngleLen);
            PCMMLNode2 pSub;
            pRCSD->pNode->ContentEnum (dwIndex, &psz, &pSub);
            if (!pSub)
               return TRUE;   // shouldnt happen

            szTemp[0] = 0;
            p->pControl->AttribGet (Text(), szTemp, sizeof(szTemp), &dwNeed);

            if (szTemp[0])
               pSub->AttribSetString (gpszLRAngle, szTemp);
            else
               pSub->AttribDelete (gpszLRAngle);

            pRCSD->fChanged = TRUE;
            //pPage->Exit (RedoSamePage());
            return TRUE;
         }
         else if (!wcsncmp(psz, pszUDAngle, dwUDAngleLen)) {
            dwIndex = (DWORD)_wtoi(psz + dwUDAngleLen);
            PCMMLNode2 pSub;
            pRCSD->pNode->ContentEnum (dwIndex, &psz, &pSub);
            if (!pSub)
               return TRUE;   // shouldnt happen

            szTemp[0] = 0;
            p->pControl->AttribGet (Text(), szTemp, sizeof(szTemp), &dwNeed);

            if (szTemp[0])
               pSub->AttribSetString (gpszUDAngle, szTemp);
            else
               pSub->AttribDelete (gpszUDAngle);

            pRCSD->fChanged = TRUE;
            //pPage->Exit (RedoSamePage());
            return TRUE;
         }
         else if (!wcsncmp(psz, pszFOV, dwFOVLen)) {
            dwIndex = (DWORD)_wtoi(psz + dwFOVLen);
            PCMMLNode2 pSub;
            pRCSD->pNode->ContentEnum (dwIndex, &psz, &pSub);
            if (!pSub)
               return TRUE;   // shouldnt happen

            szTemp[0] = 0;
            p->pControl->AttribGet (Text(), szTemp, sizeof(szTemp), &dwNeed);

            if (szTemp[0])
               pSub->AttribSetString (gpszFOV, szTemp);
            else
               pSub->AttribDelete (gpszFOV);

            pRCSD->fChanged = TRUE;
            //pPage->Exit (RedoSamePage());
            return TRUE;
         }
         else if (!wcsncmp(psz, pszFade, dwFadeLen)) {
            dwIndex = (DWORD)_wtoi(psz + dwFadeLen);
            PCMMLNode2 pSub;
            pRCSD->pNode->ContentEnum (dwIndex, &psz, &pSub);
            if (!pSub)
               return TRUE;   // shouldnt happen

            szTemp[0] = 0;
            p->pControl->AttribGet (Text(), szTemp, sizeof(szTemp), &dwNeed);

            if (szTemp[0])
               pSub->AttribSetString (gpszFade, szTemp);
            else
               pSub->AttribDelete (gpszFade);

            pRCSD->fChanged = TRUE;
            //pPage->Exit (RedoSamePage());
            return TRUE;
         }
         else if (!wcsncmp(psz, pszTime, dwTimeLen)) {
            dwIndex = (DWORD)_wtoi(psz + dwTimeLen);
            PCMMLNode2 pSub;
            pRCSD->pNode->ContentEnum (dwIndex, &psz, &pSub);
            if (!pSub)
               return TRUE;   // shouldnt happen

            szTemp[0] = 0;
            p->pControl->AttribGet (Text(), szTemp, sizeof(szTemp), &dwNeed);

            if (szTemp[0])
               pSub->AttribSetString (gpszTime, szTemp);
            else
               pSub->AttribDelete (gpszTime);

            pRCSD->fChanged = TRUE;
            //pPage->Exit (RedoSamePage());
            return TRUE;
         }

         return TRUE;
      }
      break;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;
         PWSTR psz;
         psz = p->pControl->m_pszName;

         PWSTR pszMoveUp = L"moveup:", pszDel = L"del:", pszMoveDown = L"movedown:",
            pszWaitSound = L"waitsound:", pszNoLipSync = L"nolipsync:",
            pszDisableNPR = L"disablenpr:";
         DWORD dwMoveUpLen = (DWORD)wcslen(pszMoveUp), dwDelLen = (DWORD)wcslen(pszDel), dwMoveDownLen = (DWORD)wcslen(pszMoveDown),
            dwWaitSoundLen = (DWORD)wcslen(pszWaitSound), dwNoLipSyncLen = (DWORD)wcslen(pszNoLipSync),
            dwDisableNPRLen = (DWORD)wcslen(pszDisableNPR);

         DWORD dwIndex;
         if (!wcsncmp(psz, pszMoveUp, dwMoveUpLen)) {
            dwIndex = (DWORD)_wtoi(psz + dwMoveUpLen);
            PCMMLNode2 pSub;
            pRCSD->pNode->ContentEnum (dwIndex, &psz, &pSub);
            if (!pSub)
               return TRUE;   // shouldnt happen
            pRCSD->pNode->ContentRemove (dwIndex, FALSE);

            // insert higher
            pRCSD->pNode->ContentInsert (dwIndex-1, pSub);

            pRCSD->fChanged = TRUE;
            pPage->Exit (RedoSamePage());
            return TRUE;
         }
         else if (!wcsncmp(psz, pszMoveDown, dwMoveDownLen)) {
            dwIndex = (DWORD)_wtoi(psz + dwMoveDownLen);
            PCMMLNode2 pSub;
            pRCSD->pNode->ContentEnum (dwIndex, &psz, &pSub);
            if (!pSub)
               return TRUE;   // shouldnt happen
            pRCSD->pNode->ContentRemove (dwIndex, FALSE);

            // insert higher
            pRCSD->pNode->ContentInsert (dwIndex+1, pSub);

            pRCSD->fChanged = TRUE;
            pPage->Exit (RedoSamePage());
            return TRUE;
         }
         else if (!wcsncmp(psz, pszDel, dwDelLen)) {
            dwIndex = (DWORD)_wtoi(psz + dwDelLen);

            if (IDYES != pPage->MBYesNo (L"Are you sure you want to delete this action?"))
               return TRUE;

            pRCSD->pNode->ContentRemove (dwIndex);

            pRCSD->fChanged = TRUE;
            pPage->Exit (RedoSamePage());
            return TRUE;
         }
         else if (!wcsncmp(psz, pszWaitSound, dwWaitSoundLen)) {
            dwIndex = (DWORD)_wtoi(psz + dwWaitSoundLen);
            PCMMLNode2 pSub;
            pRCSD->pNode->ContentEnum (dwIndex, &psz, &pSub);
            if (!pSub)
               return TRUE;   // shouldnt happen

            if (p->pControl->AttribGetBOOL(Checked()))
               pSub->AttribSetString (gpszWaitSound, L"1");
            else
               pSub->AttribDelete (gpszWaitSound);

            pRCSD->fChanged = TRUE;
            // pPage->Exit (RedoSamePage());
            return TRUE;
         }
         else if (!wcsncmp(psz, pszDisableNPR, dwDisableNPRLen)) {
            dwIndex = (DWORD)_wtoi(psz + dwDisableNPRLen);
            PCMMLNode2 pSub;
            pRCSD->pNode->ContentEnum (dwIndex, &psz, &pSub);
            if (!pSub)
               return TRUE;   // shouldnt happen

            if (p->pControl->AttribGetBOOL(Checked()))
               pSub->AttribSetString (gpszDisableNPR, L"1");
            else
               pSub->AttribDelete (gpszDisableNPR);

            pRCSD->fChanged = TRUE;
            // pPage->Exit (RedoSamePage());
            return TRUE;
         }
         else if (!wcsncmp(psz, pszNoLipSync, dwNoLipSyncLen)) {
            dwIndex = (DWORD)_wtoi(psz + dwNoLipSyncLen);
            PCMMLNode2 pSub;
            pRCSD->pNode->ContentEnum (dwIndex, &psz, &pSub);
            if (!pSub)
               return TRUE;   // shouldnt happen

            if (p->pControl->AttribGetBOOL(Checked()))
               pSub->AttribSetString (gpszNoLipSync, L"1");
            else
               pSub->AttribDelete (gpszNoLipSync);

            pRCSD->fChanged = TRUE;
            // pPage->Exit (RedoSamePage());
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"newttsfile")) {
            if (TTSCacheDefaultUI (pPage->m_pWindow->m_hWnd))
               TTSCacheDefaultGet (pPage, L"ttsfile");
            return TRUE;
         }

         // transplanted prosody options
         PWSTR pszTTSSC = L"ttssc:", pszTTSSP = L"ttssp:", pszTTSTP = L"ttstp:", pszTTSDT = L"ttsdt:";
         DWORD dwTTSSCLen = (DWORD)wcslen(pszTTSSC), dwTTSSPLen = (DWORD)wcslen(pszTTSSP),
            dwTTSTPLen = (DWORD)wcslen(pszTTSTP), dwTTSDTLen = (DWORD)wcslen(pszTTSDT);
         if (!wcsncmp (psz, pszTTSSC, dwTTSSCLen)) {
            DWORD dwIndex = (DWORD)_wtoi(psz + dwTTSSCLen);
            PCMMLNode2 pSub;
            pRCSD->pNode->ContentEnum (dwIndex, &psz, &pSub);
            if (!pSub)
               return TRUE;   // shouldnt happen
            PWSTR pszSpeak = pSub->AttribGetString (gpszText);
            TTSCacheSpellCheck (pszSpeak, pPage->m_pWindow->m_hWnd);
            return TRUE;
         }
         if (!wcsncmp (psz, pszTTSSP, dwTTSSPLen)) {
            DWORD dwIndex = (DWORD)_wtoi(psz + dwTTSSPLen);
            PCMMLNode2 pSub;
            pRCSD->pNode->ContentEnum (dwIndex, &psz, &pSub);
            if (!pSub)
               return TRUE;   // shouldnt happen
            PWSTR pszSpeak = pSub->AttribGetString (gpszText);
            TTSCacheSpeak (pszSpeak, FALSE, FALSE, pPage->m_pWindow->m_hWnd);
            return TRUE;
         }
         if (!wcsncmp (psz, pszTTSTP, dwTTSTPLen)) {
            DWORD dwIndex = (DWORD)_wtoi(psz + dwTTSTPLen);
            PCMMLNode2 pSub;
            pRCSD->pNode->ContentEnum (dwIndex, &psz, &pSub);
            if (!pSub)
               return TRUE;   // shouldnt happen
            PWSTR pszSpeak = pSub->AttribGetString (gpszText);

            if (2 == pRCSD->pLib->TransProsQuickDialog (NULL, pPage->m_pWindow->m_hWnd, pRCSD->lid,
               (PWSTR)pRCSD->pRes->m_memName.p, pszSpeak)) {

                  // enable the delete control
                  WCHAR szTemp[64];
                  swprintf (szTemp, L"ttsdt:%d", (int)dwIndex);
                  PCEscControl pControl = pPage->ControlFind (szTemp);
                  if (pControl)
                     pControl->Enable (TRUE);
            }

            return TRUE;
         }
         if (!wcsncmp (psz, pszTTSDT, dwTTSDTLen)) {
            DWORD dwIndex = (DWORD)_wtoi(psz + dwTTSDTLen);
            PCMMLNode2 pSub;
            pRCSD->pNode->ContentEnum (dwIndex, &psz, &pSub);
            if (!pSub)
               return TRUE;   // shouldnt happen
            PWSTR pszSpeak = pSub->AttribGetString (gpszText);
            if (pRCSD->pLib->TransProsQuickDeleteUI (pPage, pszSpeak, pRCSD->lid)) {
                  // disable the delete control
                  WCHAR szTemp[64];
                  swprintf (szTemp, L"ttsdt:%d", (int)dwIndex);
                  PCEscControl pControl = pPage->ControlFind (szTemp);
                  if (pControl)
                     pControl->Enable (FALSE);
            }
            return TRUE;
         }
      }
      break;

   case ESCN_COMBOBOXSELCHANGE:
      {
         PESCNCOMBOBOXSELCHANGE p = (PESCNCOMBOBOXSELCHANGE) pParam;
         PWSTR psz = p->pControl->m_pszName;
         if (!psz)
            break;

         PWSTR pszActionType = L"at:", pszTortp = L"tortp:", pszSpeakStyle = L"speakstyle:",
            pszLookDir = L"lookdir:";
         DWORD dwActionTypeLen = (DWORD)wcslen(pszActionType), dwTortpLen = (DWORD)wcslen(pszTortp),
            dwSpeakStyleLen = (DWORD)wcslen(pszSpeakStyle), dwLookDirLen = (DWORD)wcslen(pszLookDir);
         DWORD dwIndex;

         if (!wcsncmp (psz, pszActionType, dwActionTypeLen)) {
            dwIndex = (DWORD)_wtoi(psz + dwActionTypeLen);
            PCMMLNode2 pSub = NULL;
            pRCSD->pNode->ContentEnum (dwIndex, &psz, &pSub);
            if (!pSub)
               return TRUE;   // shouldnt happen

            if (!_wcsicmp(gapszActionType[p->dwCurSel], pSub->NameGet()))
               return TRUE;   // same, so no change

            // else, change
            pRCSD->pNode->ContentRemove (dwIndex);
            pSub = new CMMLNode2;
            if (pSub) {
               pSub->NameSet (gapszActionType[p->dwCurSel]);
               pRCSD->pNode->ContentInsert (dwIndex, pSub);
            }

            pRCSD->fChanged = TRUE;
            pPage->Exit (RedoSamePage());
            return TRUE;
         }
         else if (!wcsncmp (psz, pszSpeakStyle, dwSpeakStyleLen)) {
            dwIndex = (DWORD)_wtoi(psz + dwSpeakStyleLen);
            PCMMLNode2 pSub = NULL;
            pRCSD->pNode->ContentEnum (dwIndex, &psz, &pSub);
            if (!pSub)
               return TRUE;   // shouldnt happen

            psz = pSub->AttribGetString (gpszSpeakStyle);
            if (!psz)
               psz = gapszSpeakStyle[SPEAKSTYLEDEFAULT];
            if (!_wcsicmp(gapszSpeakStyle[p->dwCurSel], psz))
               return TRUE;   // same, so no change

            // else, change
            if (p->dwCurSel != SPEAKSTYLEDEFAULT)
               pSub->AttribSetString (gpszSpeakStyle, gapszSpeakStyle[p->dwCurSel]);
            else
               pSub->AttribDelete (gpszSpeakStyle);

            pRCSD->fChanged = TRUE;
            // pPage->Exit (RedoSamePage());
            return TRUE;
         }
         else if (!wcsncmp (psz, pszTortp, dwTortpLen)) {
            dwIndex = (DWORD)_wtoi(psz + dwTortpLen);
            PCMMLNode2 pSub = NULL;
            pRCSD->pNode->ContentEnum (dwIndex, &psz, &pSub);
            if (!pSub)
               return TRUE;   // shouldnt happen

            psz = pSub->AttribGetString (gpszTransPros);
            BOOL fTransPros = psz ? TRUE : FALSE;
            BOOL fWantTransPros = p->dwCurSel ? TRUE : FALSE;

            if (fTransPros == fWantTransPros)
               return TRUE;   // no change

            // else, changed
            if (fWantTransPros) {
               pSub->AttribDelete (gpszText);
               pSub->AttribSetString (gpszTransPros,
                  pRCSD->plTransPros->Num() ? (PWSTR)pRCSD->plTransPros->Get(0) : L"Unknown");
            }
            else {
               pSub->AttribDelete (gpszTransPros);
               // no need to add text since will assume that
            }

            pRCSD->fChanged = TRUE;
            pPage->Exit (RedoSamePage());
            return TRUE;
         }
         else if (!wcsncmp (psz, pszLookDir, dwLookDirLen)) {
            dwIndex = (DWORD)_wtoi(psz + dwLookDirLen);
            PCMMLNode2 pSub = NULL;
            pRCSD->pNode->ContentEnum (dwIndex, &psz, &pSub);
            if (!pSub)
               return TRUE;   // shouldnt happen

            int iDir = 0;
            if (!pSub->AttribGetInt (gpszLookDir, &iDir))
               iDir = 0;
            int iWantDir = (int)p->dwCurSel - 1;

            if (iDir == iWantDir)
               return TRUE;   // no change

            // else, changed
            if (iWantDir)
               pSub->AttribSetInt (gpszLookDir, iWantDir);
            else
               pSub->AttribDelete (gpszLookDir);
               // no need to add text since will assume that

            pRCSD->fChanged = TRUE;
            pPage->Exit (RedoSamePage());
            return TRUE;
         }      }
      break;

   case ESCN_FILTEREDLISTQUERY:
      {
         PESCNFILTEREDLISTQUERY p = (PESCNFILTEREDLISTQUERY) pParam;
         PWSTR psz = p->pszListName;
         if (!psz)
            break;

         if (!_wcsicmp(psz, L"transpros")) {
            p->pList = pRCSD->plTransPros;
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"voice")) {
            p->pList = pRCSD->plVoice;
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"objects")) {
            p->pList = pRCSD->plObjects;
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"images")) {
            p->pList = pRCSD->plImages;
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"rooms")) {
            p->pList = pRCSD->plRooms;
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"sounds")) {
            p->pList = pRCSD->plSounds;
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"ambient")) {
            p->pList = pRCSD->plAmbient;
            return TRUE;
         }
      }
      break;

   case ESCN_FILTEREDLISTCHANGE:
      {
         PESCNFILTEREDLISTCHANGE p = (PESCNFILTEREDLISTCHANGE) pParam;
         PWSTR psz = p->pControl->m_pszName;
         if (!psz)
            break;

         PWSTR pszTransPros = L"transpros:", pszVoice = L"voice:",
            pszObject = L"object:", pszResource = L"resource:",
            pszSound = L"sound:", pszAmbient = L"ambient:", pszRoom = L"room:";
         DWORD dwTransProsLen = (DWORD)wcslen(pszTransPros), dwVoiceLen = (DWORD)wcslen(pszVoice),
            dwObjectLen = (DWORD)wcslen(pszObject), dwResourceLen = (DWORD)wcslen(pszResource),
            dwSoundLen = (DWORD)wcslen(pszSound), dwAmbientLen = (DWORD)wcslen(pszAmbient),
            dwRoomLen = (DWORD)wcslen(pszRoom);
         DWORD dwIndex;

         if (!wcsncmp(psz, pszTransPros, dwTransProsLen)) {
            dwIndex = (DWORD)_wtoi(psz + dwTransProsLen);
            PCMMLNode2 pSub = NULL;
            pRCSD->pNode->ContentEnum (dwIndex, &psz, &pSub);
            if (!pSub)
               return TRUE;   // shouldnt happen

            // make sure they're not the same
            psz = pSub->AttribGetString (gpszTransPros);
            int iNewSel = p->pControl->AttribGetInt (CurSel());
            PWSTR pszNew = (iNewSel >= 0) ? (PWSTR)pRCSD->plTransPros->Get((DWORD)iNewSel) : NULL;
            if (!psz || !pszNew) {
               if (psz == pszNew)
                  return TRUE;   // no change
            }
            else {
               if (!_wcsicmp(psz, pszNew))
                  return TRUE;   // no change
            }
            
            pSub->AttribSetString (gpszTransPros, pszNew ? pszNew : L"Unknown");

            pRCSD->fChanged = TRUE;
            //pPage->Exit (RedoSamePage());
            return TRUE;
         }
         else if (!wcsncmp(psz, pszVoice, dwVoiceLen)) {
            dwIndex = (DWORD)_wtoi(psz + dwVoiceLen);
            PCMMLNode2 pSub = NULL;
            pRCSD->pNode->ContentEnum (dwIndex, &psz, &pSub);
            if (!pSub)
               return TRUE;   // shouldnt happen

            // make sure they're not the same
            psz = pSub->AttribGetString (gpszVoice);
            int iNewSel = p->pControl->AttribGetInt (CurSel());
            PWSTR pszNew = (iNewSel >= 0) ? (PWSTR)pRCSD->plVoice->Get((DWORD)iNewSel) : NULL;
            if (!psz || !pszNew) {
               if (psz == pszNew)
                  return TRUE;   // no change
            }
            else {
               if (!_wcsicmp(psz, pszNew))
                  return TRUE;   // no change
            }
            
            pSub->AttribSetString (gpszVoice, pszNew ? pszNew : (PWSTR) pRCSD->plVoice->Get(0));

            pRCSD->fChanged = TRUE;
            //pPage->Exit (RedoSamePage());
            return TRUE;
         }
         else if (!wcsncmp(psz, pszAmbient, dwAmbientLen)) {
            dwIndex = (DWORD)_wtoi(psz + dwAmbientLen);
            PCMMLNode2 pSub = NULL;
            pRCSD->pNode->ContentEnum (dwIndex, &psz, &pSub);
            if (!pSub)
               return TRUE;   // shouldnt happen

            // make sure they're not the same
            psz = pSub->AttribGetString (gpszResource);
            int iNewSel = p->pControl->AttribGetInt (CurSel());
            PWSTR pszNew = (iNewSel >= 0) ? (PWSTR)pRCSD->plAmbient->Get((DWORD)iNewSel) : NULL;
            if (!psz || !pszNew) {
               if (psz == pszNew)
                  return TRUE;   // no change
            }
            else {
               if (!_wcsicmp(psz, pszNew))
                  return TRUE;   // no change
            }
            
            pSub->AttribSetString (gpszResource, pszNew ? pszNew : (PWSTR) pRCSD->plAmbient->Get(0));

            pRCSD->fChanged = TRUE;
            //pPage->Exit (RedoSamePage());
            return TRUE;
         }
         else if (!wcsncmp(psz, pszObject, dwObjectLen)) {
            dwIndex = (DWORD)_wtoi(psz + dwObjectLen);
            PCMMLNode2 pSub = NULL;
            pRCSD->pNode->ContentEnum (dwIndex, &psz, &pSub);
            if (!pSub)
               return TRUE;   // shouldnt happen

            // make sure they're not the same
            psz = pSub->AttribGetString (gpszObject);
            int iNewSel = p->pControl->AttribGetInt (CurSel());
            PWSTR pszNew = (iNewSel >= 0) ? (PWSTR)pRCSD->plObjects->Get((DWORD)iNewSel) : NULL;
            if (!psz || !pszNew) {
               if (psz == pszNew)
                  return TRUE;   // no change
            }
            else {
               if (!_wcsicmp(psz, pszNew))
                  return TRUE;   // no change
            }
            
            pSub->AttribSetString (gpszObject, pszNew ? pszNew : (PWSTR) pRCSD->plObjects->Get(0));

            pRCSD->fChanged = TRUE;
            //pPage->Exit (RedoSamePage());
            return TRUE;
         }
         else if (!wcsncmp(psz, pszResource, dwResourceLen)) {
            dwIndex = (DWORD)_wtoi(psz + dwResourceLen);
            PCMMLNode2 pSub = NULL;
            pRCSD->pNode->ContentEnum (dwIndex, &psz, &pSub);
            if (!pSub)
               return TRUE;   // shouldnt happen

            // make sure they're not the same
            psz = pSub->AttribGetString (gpszResource);
            int iNewSel = p->pControl->AttribGetInt (CurSel());
            PWSTR pszNew = (iNewSel >= 0) ? (PWSTR)pRCSD->plImages->Get((DWORD)iNewSel) : NULL;
            if (!psz || !pszNew) {
               if (psz == pszNew)
                  return TRUE;   // no change
            }
            else {
               if (!_wcsicmp(psz, pszNew))
                  return TRUE;   // no change
            }
            
            if (pszNew)
               pSub->AttribSetString (gpszResource, pszNew);
            else
               pSub->AttribDelete (gpszResource);

            pRCSD->fChanged = TRUE;
            //pPage->Exit (RedoSamePage());
            return TRUE;
         }
         else if (!wcsncmp(psz, pszRoom, dwRoomLen)) {
            dwIndex = (DWORD)_wtoi(psz + dwRoomLen);
            PCMMLNode2 pSub = NULL;
            pRCSD->pNode->ContentEnum (dwIndex, &psz, &pSub);
            if (!pSub)
               return TRUE;   // shouldnt happen

            // make sure they're not the same
            psz = pSub->AttribGetString (gpszRoom);
            int iNewSel = p->pControl->AttribGetInt (CurSel());
            PWSTR pszNew = (iNewSel >= 0) ? (PWSTR)pRCSD->plRooms->Get((DWORD)iNewSel) : NULL;
            if (!psz || !pszNew) {
               if (psz == pszNew)
                  return TRUE;   // no change
            }
            else {
               if (!_wcsicmp(psz, pszNew))
                  return TRUE;   // no change
            }
            
            pSub->AttribSetString (gpszRoom, pszNew ? pszNew : (PWSTR) pRCSD->plRooms->Get(0));

            pRCSD->fChanged = TRUE;
            //pPage->Exit (RedoSamePage());
            return TRUE;
         }
         else if (!wcsncmp(psz, pszSound, dwSoundLen)) {
            dwIndex = (DWORD)_wtoi(psz + dwSoundLen);
            PCMMLNode2 pSub = NULL;
            pRCSD->pNode->ContentEnum (dwIndex, &psz, &pSub);
            if (!pSub)
               return TRUE;   // shouldnt happen

            // make sure they're not the same
            psz = pSub->AttribGetString (gpszResource);
            int iNewSel = p->pControl->AttribGetInt (CurSel());
            PWSTR pszNew = (iNewSel >= 0) ? (PWSTR)pRCSD->plSounds->Get((DWORD)iNewSel) : NULL;
            if (!psz || !pszNew) {
               if (psz == pszNew)
                  return TRUE;   // no change
            }
            else {
               if (!_wcsicmp(psz, pszNew))
                  return TRUE;   // no change
            }
            
            if (pszNew)
               pSub->AttribSetString (gpszResource, pszNew);
            else
               pSub->AttribDelete (gpszResource);

            pRCSD->fChanged = TRUE;
            //pPage->Exit (RedoSamePage());
            return TRUE;
         }
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"CutScene resource";
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"COMBODEFINE")) {
            MemZero (&gMemTemp);

            // actiontype
            MemCat (&gMemTemp,
                  L"<!xComboActionType>"
                  L"<bold><combobox macroattribute=1 cbheight=150>");

            DWORD i;
            for (i = 0; i < sizeof(gapszActionType) / sizeof(PWSTR); i++) {
               MemCat (&gMemTemp,
                  L"<elem name=");
               MemCat (&gMemTemp, (int)i);
               MemCat (&gMemTemp,
                  L">");
               MemCatSanitize (&gMemTemp, gapszActionType[i]);
               MemCat (&gMemTemp,
                  L"</elem>");
            } // i

            MemCat (&gMemTemp, L"</combobox></bold></xComboActionType>");


            // speak style
            MemCat (&gMemTemp,
                  L"<!xComboSpeakStyle>"
                  L"<bold><combobox macroattribute=1 cbheight=150>");
            for (i = 0; i < sizeof(gapszSpeakStyle) / sizeof(PWSTR); i++) {
               MemCat (&gMemTemp,
                  L"<elem name=");
               MemCat (&gMemTemp, (int)i);
               MemCat (&gMemTemp,
                  L">");
               MemCatSanitize (&gMemTemp, gapszSpeakStyle[i]);
               MemCat (&gMemTemp,
                  L"</elem>");
            } // i
            MemCat (&gMemTemp, L"</combobox></bold></xComboSpeakStyle>");


            p->pszSubString = (PWSTR) gMemTemp.p;
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"ACTIONS")) {
            MemZero (&gMemTemp);

            DWORD i, j;
            DWORD dwNum = pRCSD->pNode->ContentNum();
            PWSTR papszColor[sizeof(gapszActionType)/sizeof(PWSTR)] = {L" bgcolor=#c0ff00", L" bgcolor=#c0c0ff", L" bgcolor=#c0c0c0",
               L" bgcolor=#80c080", L" bgcolor=#ffc0c0", L" bgcolor=#ffff80",
               L" bgcolor=#8080ff"};
            int iDir;

            for (i = 0; i < dwNum; i++) {
               PCMMLNode2 pSub = NULL;
               PWSTR psz = NULL;
               pRCSD->pNode->ContentEnum (i, &psz, &pSub);
               if (!pSub)
                  continue;

               MemCat (&gMemTemp,
                  L"<tr>");

               // get the type
               psz = pSub->NameGet();
               for (j = 0; j < sizeof(gapszActionType)/sizeof(PWSTR); j++)
                  if (!_wcsicmp(psz, gapszActionType[j]))
                     break;
               if (j >= sizeof(gapszActionType)/sizeof(PWSTR))
                  j = 0;   // so have something
               PWSTR pszColor = papszColor[j];

               // type
               MemCat (&gMemTemp,
                  L"<td width=20%");
               MemCat (&gMemTemp,
                  pszColor);
               MemCat (&gMemTemp,
                  L"><xComboActionType width=100% ");
               if (pRCSD->fReadOnly)
                  MemCat (&gMemTemp, L"enabled=false ");
               MemCat (&gMemTemp,
                  L"cursel=");
               MemCat (&gMemTemp,
                  (int) j);
               MemCat (&gMemTemp,
                  L" name=at:");
               MemCat (&gMemTemp,
                  (int) i);
               MemCat (&gMemTemp,
                  L"/></td>");

               // details
               MemCat (&gMemTemp,
                  L"<td width=65%");
               MemCat (&gMemTemp,
                  pszColor);
               MemCat (&gMemTemp,
                  L">");

               DWORD *pdwIndex;
               BOOL fTransPros;
               switch (j) {
               case 0:  // Speak
                  // text or transplanted prosody
                  psz = pSub->AttribGetString (gpszTransPros);
                  fTransPros = psz ? TRUE : FALSE;

                  MemCat (&gMemTemp,
                     L"<xComboTextOrTransPros width=20% ");
                  if (pRCSD->fReadOnly)
                     MemCat (&gMemTemp, L"enabled=false ");
                  MemCat (&gMemTemp,
                     L"cursel=");
                  MemCat (&gMemTemp, fTransPros ? 1 : 0);
                  MemCat (&gMemTemp,
                     L" name=tortp:");
                  MemCat (&gMemTemp,
                     (int) i);
                  MemCat (&gMemTemp,
                     L"/>");

                  if (fTransPros) {
                     // transplanted prosody selection
                     MemCat (&gMemTemp,
                        L"<bold><filteredlist width=80% additem=\"\" sort=false listname=transpros ");
                     if (pRCSD->fReadOnly)
                        MemCat (&gMemTemp,
                        L"enabled=false ");
                     MemCat (&gMemTemp,
                        L"cursel=");
                     psz = pSub->AttribGetString (gpszTransPros);
                     pdwIndex = psz ? (DWORD*) pRCSD->phTransPros->Find (psz) : NULL;
                     MemCat (&gMemTemp,
                        pdwIndex ? (int)pdwIndex[0] : -1);
                     MemCat (&gMemTemp,
                        L" name=transpros:");
                     MemCat (&gMemTemp,
                        (int) i);
                     MemCat (&gMemTemp,
                        L"/></bold>");
                  }
                  else {
                     // text
                     MemCat (&gMemTemp,
                        L"<bold><edit maxchars=1000 width=80% name=text:");
                     MemCat (&gMemTemp,
                        (int)i);

                     psz = pSub->AttribGetString (gpszText);
                     if (psz && psz[0]) {
                        MemCat (&gMemTemp,
                           L" text=\"");
                        MemCatSanitize (&gMemTemp,
                           psz);
                        MemCat (&gMemTemp,
                           L"\"");
                     }

                     if (pRCSD->fReadOnly)
                        MemCat (&gMemTemp, L" readonly=true");

                     MemCat (&gMemTemp,
                        L"></edit></bold>");


                     // quick transpranted prosody options
                     MemCat (&gMemTemp,
                        L"<br/>"
                        L"<button width=25% name=ttssc:");
                     MemCat (&gMemTemp, (int)i);
                     MemCat (&gMemTemp,
                        L">Spell check</button>"
                        L"<button width=25% name=ttssp:");
                     MemCat (&gMemTemp, (int)i);
                     MemCat (&gMemTemp,
                        L">Speak</button>");

                     if (!pRCSD->fReadOnly) {
                        MemCat (&gMemTemp,
                           L"<button width=25% name=ttstp:");
                        MemCat (&gMemTemp, (int)i);
                        MemCat (&gMemTemp,
                           L">Record transplanted prosody</button>");

                        MemCat (&gMemTemp,
                           L"<button width=25% name=ttsdt:");
                        MemCat (&gMemTemp, (int)i);
                        PWSTR pszText = psz;
                        DWORD dwID = pRCSD->pLib->TransProsQuickFind (pszText, pRCSD->lid);
                        if (dwID == (DWORD)-1)
                           MemCat (&gMemTemp, L" enabled=false");
                        MemCat (&gMemTemp,
                           L">");
                        MemCat (&gMemTemp,
                           L"Delete trans. pros.</button>");
                     }

                  }

                  MemCat (&gMemTemp,
                     L"<br/>");

                  // voice
                  MemCat (&gMemTemp,
                     L"<bold><filteredlist width=40% additem=\"\" sort=false listname=voice ");
                  if (pRCSD->fReadOnly)
                     MemCat (&gMemTemp,
                     L"enabled=false ");
                  MemCat (&gMemTemp,
                     L"cursel=");
                  psz = pSub->AttribGetString (gpszVoice);
                  if (!psz)
                     psz = (PWSTR)pRCSD->plVoice->Get(0);   // NOTE: If nothing default to narrator
                  pdwIndex = psz ? (DWORD*) pRCSD->phVoice->Find (psz) : NULL;
                  MemCat (&gMemTemp,
                     pdwIndex ? (int)pdwIndex[0] : -1);
                  MemCat (&gMemTemp,
                     L" name=voice:");
                  MemCat (&gMemTemp,
                     (int) i);
                  MemCat (&gMemTemp,
                     L"/></bold>");

                  // speaking style
                  MemCat (&gMemTemp,
                     L"<xComboSpeakStyle width=20% ");
                  if (pRCSD->fReadOnly)
                     MemCat (&gMemTemp, L"enabled=false ");
                  MemCat (&gMemTemp,
                     L"cursel=");
                  psz = pSub->AttribGetString (gpszSpeakStyle);
                  if (!psz)
                     psz = gapszSpeakStyle[SPEAKSTYLEDEFAULT];
                  for (j = 0; j < sizeof(gapszSpeakStyle)/sizeof(PWSTR); j++)
                     if (!_wcsicmp(psz, gapszSpeakStyle[j]))
                        break;
                  if (j >= sizeof(gapszSpeakStyle)/sizeof(PWSTR))
                     j = 0;   // so have something
                  MemCat (&gMemTemp,
                     (int) j);
                  MemCat (&gMemTemp,
                     L" name=speakstyle:");
                  MemCat (&gMemTemp,
                     (int) i);
                  MemCat (&gMemTemp,
                     L"/>");



                  // waitsound
                  MemCat (&gMemTemp,
                     L"<button width=20% style=x checkbox=true ");
                  if (pRCSD->fReadOnly)
                     MemCat (&gMemTemp, L"enabled=false ");
                  psz = pSub->AttribGetString (gpszNoLipSync);
                  if (psz && (psz[0] != L'0'))  // BUGFIX - Was psz[1]
                     MemCat (&gMemTemp, L"checked=true ");
                  MemCat (&gMemTemp,
                     L"name=nolipsync:");
                  MemCat (&gMemTemp,
                     (int)i);
                  MemCat (&gMemTemp,
                     L"><bold>No lip sync</bold></button>");

                  // language
                  MemCat (&gMemTemp,
                     L"<bold>  Lang: <edit maxchars=32 width=10% name=lang:");
                  MemCat (&gMemTemp,
                     (int)i);

                  psz = pSub->AttribGetString (gpszLang);
                  if (psz && psz[0]) {
                     MemCat (&gMemTemp,
                        L" text=\"");
                     MemCatSanitize (&gMemTemp,
                        psz);
                     MemCat (&gMemTemp,
                        L"\"");
                  }

                  if (pRCSD->fReadOnly)
                     MemCat (&gMemTemp, L" readonly=true");

                  MemCat (&gMemTemp,
                     L"></edit></bold>");

                  break;   // end of speak section

               case 1:  // sound
                  // sound
                  MemCat (&gMemTemp,
                     L"<bold><filteredlist width=50% additem=\"\" sort=false listname=sounds ");
                  if (pRCSD->fReadOnly)
                     MemCat (&gMemTemp,
                     L"enabled=false ");
                  MemCat (&gMemTemp,
                     L"cursel=");
                  psz = pSub->AttribGetString (gpszResource);
                  pdwIndex = psz ? (DWORD*) pRCSD->phSounds->Find (psz) : NULL;
                  MemCat (&gMemTemp,
                     pdwIndex ? (int)pdwIndex[0] : -1);
                  MemCat (&gMemTemp,
                     L" name=sound:");
                  MemCat (&gMemTemp,
                     (int) i);
                  MemCat (&gMemTemp,
                     L"/></bold>");

                  // waitsound
                  MemCat (&gMemTemp,
                     L"<br/>"
                     L"<button style=x checkbox=true ");
                  if (pRCSD->fReadOnly)
                     MemCat (&gMemTemp, L"enabled=false ");
                  psz = pSub->AttribGetString (gpszWaitSound);
                  if (psz && (psz[0] != L'0'))  // BUGFIX - Was psz[1]
                     MemCat (&gMemTemp, L"checked=true ");
                  MemCat (&gMemTemp,
                     L"name=waitsound:");
                  MemCat (&gMemTemp,
                     (int)i);
                  MemCat (&gMemTemp,
                     L"><bold>Wait until sound completed</bold></button>");

                  break;

               case 2:  // Pause
                  // time
                  MemCat (&gMemTemp,
                     L"<bold>Time: <edit maxchars=32 width=25% name=time:");
                  MemCat (&gMemTemp,
                     (int)i);
                  psz = pSub->AttribGetString (gpszTime);
                  if (psz && psz[0]) {
                     MemCat (&gMemTemp,
                        L" text=\"");
                     MemCatSanitize (&gMemTemp,
                        psz);
                     MemCat (&gMemTemp,
                        L"\"");
                  }
                  if (pRCSD->fReadOnly)
                     MemCat (&gMemTemp, L" readonly=true");
                  MemCat (&gMemTemp,
                     L"></edit></bold>");
                  break;


               case 3:  // ambient
                  // ambient
                  MemCat (&gMemTemp,
                     L"<bold><filteredlist width=50% additem=\"\" sort=false listname=ambient ");
                  if (pRCSD->fReadOnly)
                     MemCat (&gMemTemp,
                     L"enabled=false ");
                  MemCat (&gMemTemp,
                     L"cursel=");
                  psz = pSub->AttribGetString (gpszResource);
                  if (!psz)
                     psz = (PWSTR) pRCSD->plAmbient->Get(0);
                  pdwIndex = psz ? (DWORD*) pRCSD->phAmbient->Find (psz) : NULL;
                  MemCat (&gMemTemp,
                     pdwIndex ? (int)pdwIndex[0] : -1);
                  MemCat (&gMemTemp,
                     L" name=ambient:");
                  MemCat (&gMemTemp,
                     (int) i);
                  MemCat (&gMemTemp,
                     L"/></bold>");
                  break;

               case 4:  // NPCEmote
                  // object
                  MemCat (&gMemTemp,
                     L"<bold><filteredlist width=50% additem=\"\" sort=false listname=objects ");
                  if (pRCSD->fReadOnly)
                     MemCat (&gMemTemp,
                     L"enabled=false ");
                  MemCat (&gMemTemp,
                     L"cursel=");
                  psz = pSub->AttribGetString (gpszObject);
                  if (!psz)
                     psz = (PWSTR)pRCSD->plObjects->Get(0);   // NOTE: If nothing default to narrator
                  pdwIndex = psz ? (DWORD*) pRCSD->phObjects->Find (psz) : NULL;
                  MemCat (&gMemTemp,
                     pdwIndex ? (int)pdwIndex[0] : -1);
                  MemCat (&gMemTemp,
                     L" name=object:");
                  MemCat (&gMemTemp,
                     (int) i);
                  MemCat (&gMemTemp,
                     L"/></bold>");

                  // emote
                  MemCat (&gMemTemp,
                     L"<br/>"
                     L"<bold>Emote: <edit maxchars=32 width=25% name=emote:");
                  MemCat (&gMemTemp,
                     (int)i);
                  psz = pSub->AttribGetString (gpszEmote);
                  if (psz && psz[0]) {
                     MemCat (&gMemTemp,
                        L" text=\"");
                     MemCatSanitize (&gMemTemp,
                        psz);
                     MemCat (&gMemTemp,
                        L"\"");
                  }
                  if (pRCSD->fReadOnly)
                     MemCat (&gMemTemp, L" readonly=true");
                  MemCat (&gMemTemp,
                     L"></edit></bold>");

                  // look dir
                  iDir = 0;
                  if (!pSub->AttribGetInt (gpszLookDir, &iDir))
                     iDir = 0;
                  MemCat (&gMemTemp,
                     L" <xComboLookDir width=25% ");
                  if (pRCSD->fReadOnly)
                     MemCat (&gMemTemp, L"enabled=false ");
                  MemCat (&gMemTemp,
                     L"cursel=");
                  MemCat (&gMemTemp, (DWORD)max(iDir+1,0));
                  MemCat (&gMemTemp,
                     L" name=lookdir:");
                  MemCat (&gMemTemp,
                     (int) i);
                  MemCat (&gMemTemp,
                     L"/>");

                  // fade
                  MemCat (&gMemTemp,
                     L"      <bold>Fade: <edit maxchars=32 width=25% name=fade:");
                  MemCat (&gMemTemp,
                     (int)i);
                  psz = pSub->AttribGetString (gpszFade);
                  if (psz && psz[0]) {
                     MemCat (&gMemTemp,
                        L" text=\"");
                     MemCatSanitize (&gMemTemp,
                        psz);
                     MemCat (&gMemTemp,
                        L"\"");
                  }
                  if (pRCSD->fReadOnly)
                     MemCat (&gMemTemp, L" readonly=true");
                  MemCat (&gMemTemp,
                     L"></edit></bold>");

                  break;

               case 5:  // image
                  // object
                  MemCat (&gMemTemp,
                     L"<bold><filteredlist width=50% additem=\"\" sort=false listname=images ");
                  if (pRCSD->fReadOnly)
                     MemCat (&gMemTemp,
                     L"enabled=false ");
                  MemCat (&gMemTemp,
                     L"cursel=");
                  psz = pSub->AttribGetString (gpszResource);
                  pdwIndex = psz ? (DWORD*) pRCSD->phImages->Find (psz) : NULL;
                  MemCat (&gMemTemp,
                     pdwIndex ? (int)pdwIndex[0] : -1);
                  MemCat (&gMemTemp,
                     L" name=resource:");
                  MemCat (&gMemTemp,
                     (int) i);
                  MemCat (&gMemTemp,
                     L"/></bold>");

                  MemCat (&gMemTemp,
                     L"<br/>");

                  // disable NPR
                  MemCat (&gMemTemp,
                     L"<button style=x checkbox=true ");
                  if (pRCSD->fReadOnly)
                     MemCat (&gMemTemp, L"enabled=false ");
                  psz = pSub->AttribGetString (gpszDisableNPR);
                  if (psz && (psz[0] != L'0'))  // BUGFIX - Was psz[1]
                     MemCat (&gMemTemp, L"checked=true ");
                  MemCat (&gMemTemp,
                     L"name=disablenpr:");
                  MemCat (&gMemTemp,
                     (int)i);
                  MemCat (&gMemTemp,
                     L"><bold>Disable NPR</bold></button>");

                  // fade
                  MemCat (&gMemTemp,
                     L"<bold>      Fade: <edit maxchars=32 width=25% name=fade:");
                  MemCat (&gMemTemp,
                     (int)i);
                  psz = pSub->AttribGetString (gpszFade);
                  if (psz && psz[0]) {
                     MemCat (&gMemTemp,
                        L" text=\"");
                     MemCatSanitize (&gMemTemp,
                        psz);
                     MemCat (&gMemTemp,
                        L"\"");
                  }
                  if (pRCSD->fReadOnly)
                     MemCat (&gMemTemp, L" readonly=true");
                  MemCat (&gMemTemp,
                     L"></edit></bold>");

                  break;

               case 6:  // Room
                  // riin
                  MemCat (&gMemTemp,
                     L"<bold><filteredlist width=50% additem=\"\" sort=false listname=rooms ");
                  if (pRCSD->fReadOnly)
                     MemCat (&gMemTemp,
                     L"enabled=false ");
                  MemCat (&gMemTemp,
                     L"cursel=");
                  psz = pSub->AttribGetString (gpszRoom);
                  if (!psz)
                     psz = (PWSTR)pRCSD->plRooms->Get(0);   // NOTE: If nothing default to narrator
                  pdwIndex = psz ? (DWORD*) pRCSD->phRooms->Find (psz) : NULL;
                  MemCat (&gMemTemp,
                     pdwIndex ? (int)pdwIndex[0] : -1);
                  MemCat (&gMemTemp,
                     L" name=room:");
                  MemCat (&gMemTemp,
                     (int) i);
                  MemCat (&gMemTemp,
                     L"/></bold>");

                  // date/time
                  MemCat (&gMemTemp,
                     L"<br/>"
                     L"<bold>Date/time: <edit maxchars=32 width=25% name=datetime:");
                  MemCat (&gMemTemp,
                     (int)i);
                  psz = pSub->AttribGetString (gpszDateTime);
                  if (psz && psz[0]) {
                     MemCat (&gMemTemp,
                        L" text=\"");
                     MemCatSanitize (&gMemTemp,
                        psz);
                     MemCat (&gMemTemp,
                        L"\"");
                  }
                  if (pRCSD->fReadOnly)
                     MemCat (&gMemTemp, L" readonly=true");
                  MemCat (&gMemTemp,
                     L"></edit></bold>");

                  // height
                  MemCat (&gMemTemp,
                     L"      <bold>Viewer height: <edit maxchars=32 width=25% name=height:");
                  MemCat (&gMemTemp,
                     (int)i);
                  psz = pSub->AttribGetString (gpszHeight);
                  if (psz && psz[0]) {
                     MemCat (&gMemTemp,
                        L" text=\"");
                     MemCatSanitize (&gMemTemp,
                        psz);
                     MemCat (&gMemTemp,
                        L"\"");
                  }
                  if (pRCSD->fReadOnly)
                     MemCat (&gMemTemp, L" readonly=true");
                  MemCat (&gMemTemp,
                     L"></edit></bold>");

                  // LR Angle
                  MemCat (&gMemTemp,
                     L"<br/><bold>LR Angle: <edit maxchars=32 width=25% name=lrangle:");
                  MemCat (&gMemTemp,
                     (int)i);
                  psz = pSub->AttribGetString (gpszLRAngle);
                  if (psz && psz[0]) {
                     MemCat (&gMemTemp,
                        L" text=\"");
                     MemCatSanitize (&gMemTemp,
                        psz);
                     MemCat (&gMemTemp,
                        L"\"");
                  }
                  if (pRCSD->fReadOnly)
                     MemCat (&gMemTemp, L" readonly=true");
                  MemCat (&gMemTemp,
                     L"></edit></bold>");

                  // UD Angle
                  MemCat (&gMemTemp,
                     L"      <bold>UD Angle: <edit maxchars=32 width=25% name=udangle:");
                  MemCat (&gMemTemp,
                     (int)i);
                  psz = pSub->AttribGetString (gpszUDAngle);
                  if (psz && psz[0]) {
                     MemCat (&gMemTemp,
                        L" text=\"");
                     MemCatSanitize (&gMemTemp,
                        psz);
                     MemCat (&gMemTemp,
                        L"\"");
                  }
                  if (pRCSD->fReadOnly)
                     MemCat (&gMemTemp, L" readonly=true");
                  MemCat (&gMemTemp,
                     L"></edit></bold>");

                  // FOV
                  MemCat (&gMemTemp,
                     L"<br/><bold>FOV: <edit maxchars=32 width=25% name=fov:");
                  MemCat (&gMemTemp,
                     (int)i);
                  psz = pSub->AttribGetString (gpszFOV);
                  if (psz && psz[0]) {
                     MemCat (&gMemTemp,
                        L" text=\"");
                     MemCatSanitize (&gMemTemp,
                        psz);
                     MemCat (&gMemTemp,
                        L"\"");
                  }
                  if (pRCSD->fReadOnly)
                     MemCat (&gMemTemp, L" readonly=true");
                  MemCat (&gMemTemp,
                     L"></edit></bold>");


                  break; // room

               } // switch
               MemCat (&gMemTemp,
                  L"</td>");

               // move up, down, or release
               MemCat (&gMemTemp,
                  L"<td width=15% align=center");
               MemCat (&gMemTemp,
                  pszColor);
               MemCat (&gMemTemp,
                  L">"
                  L"<button style=uptriangle ");
               if (pRCSD->fReadOnly || !i)
                  MemCat (&gMemTemp,
                     L"enabled=FALSE ");
               MemCat (&gMemTemp,
                  L"name=moveup:");
               MemCat (&gMemTemp,
                  (int)i);
               MemCat (&gMemTemp,
                  L"/>"
                  L"<button style=box color=#ff0000 ");
               if (pRCSD->fReadOnly)
                  MemCat (&gMemTemp,
                     L"enabled=FALSE ");
               MemCat (&gMemTemp,
                  L"name=del:");
               MemCat (&gMemTemp,
                  (int)i);
               MemCat (&gMemTemp,
                  L"/>"
                  L"<button style=downtriangle ");
               if (pRCSD->fReadOnly || (i+1 >= dwNum))
                  MemCat (&gMemTemp,
                     L"enabled=FALSE ");
               MemCat (&gMemTemp,
                  L"name=movedown:");
               MemCat (&gMemTemp,
                  (int)i);
               MemCat (&gMemTemp,
                  L"/>"
                  L"</td>");

               MemCat (&gMemTemp,
                  L"</tr>");
            } // i

            p->pszSubString = (PWSTR) gMemTemp.p;
            return TRUE;
         }

      }
      break;

   }; // dwMessage

   return FALSE;
}


/*************************************************************************
ResCutSceneEdit - Modify a resource CutScene. Uses standard API from mifl.h, ResourceEdit.
*/
PCMMLNode2 ResCutSceneEdit (HWND hWnd, LANGID lid, PCMMLNode2 pIn, BOOL fReadOnly,
                            PCMIFLProj pProj, PCMIFLLib pLib, PCMIFLResource pRes)
{
   PCMMLNode2 pRet = NULL;
   RCSD rcsd;
   CListVariable lTransPros, lVoice, lObjects, lImages, lSounds, lAmbient, lRooms;
   CHashString hTransPros, hVoice, hObjects, hImages, hSounds, hAmbient, hRooms;
   memset (&rcsd, 0, sizeof(rcsd));
   rcsd.fReadOnly = fReadOnly;
   rcsd.pNode = pIn->Clone();
   rcsd.plTransPros = &lTransPros;
   rcsd.phTransPros = &hTransPros;
   rcsd.plVoice = &lVoice;
   rcsd.phVoice = &hVoice;
   rcsd.plObjects = &lObjects;
   rcsd.phObjects = &hObjects;
   rcsd.plImages = &lImages;
   rcsd.phImages = &hImages;
   rcsd.plRooms = &lRooms;
   rcsd.phRooms = &hRooms;
   rcsd.plSounds = &lSounds;
   rcsd.phSounds = &hSounds;
   rcsd.plAmbient = &lAmbient;
   rcsd.phAmbient = &hAmbient;
   PWSTR apsz[5], apszClass[5];
   apsz[0] = CircumrealityTransPros();
   FillListWithResources (apsz, 1, NULL, 0, pProj, rcsd.plTransPros, rcsd.phTransPros);
   apsz[0] = CircumrealityVoice();
   apszClass[0] = L"cCharacter"; // BUGFIX - Was just using apsz
   FillListWithResources (apsz, 1, apszClass, 1, pProj,
      rcsd.plVoice, rcsd.phVoice);
   PrependOptionToList (L"Narrator", rcsd.plVoice, rcsd.phVoice);  // option
   PrependOptionToList (L"NPCAsNarrator", rcsd.plVoice, rcsd.phVoice);  // option
   apszClass[0] = L"cObject"; // BUGFIX - Was just using apsz
   FillListWithResources (NULL, 0, apszClass, 1, pProj, rcsd.plObjects, rcsd.phObjects);
   PrependOptionToList (L"NPCAsNarrator", rcsd.plObjects, rcsd.phObjects);  // option

   apszClass[0] = L"cRoom";
   FillListWithResources (NULL, 0, apszClass, 1, pProj, rcsd.plRooms, rcsd.phRooms);

   apsz[0] = CircumrealityImage();
   apsz[1] = CircumrealityText();
   apsz[2] = Circumreality3DScene();
   apsz[3] = Circumreality3DObjects();
   apsz[4] = CircumrealityTitle();
   FillListWithResources (apsz, 5, NULL, 0, pProj, rcsd.plImages, rcsd.phImages);
   apsz[0] = CircumrealityWave();
   apsz[1] = CircumrealityMusic();
   FillListWithResources (apsz, 2, NULL, 0,  pProj, rcsd.plSounds, rcsd.phSounds);
   apsz[0] = CircumrealityAmbient();
   FillListWithResources (apsz, 1, NULL, 0, pProj, rcsd.plAmbient, rcsd.phAmbient);
   PrependOptionToList (L"None", rcsd.plAmbient, rcsd.phAmbient);  // option
   rcsd.lid = lid;
   rcsd.pLib = pLib;
   rcsd.pRes = pRes;

   // keep track of transpros changes
   CListFixed lResIDOrig, lResIDNew;
   TransProsQuickFillResourceFromNode (pLib, pIn, lid, &lResIDOrig);

   PWSTR psz;
   // create the window
   RECT r;
   DialogBoxLocation3 (GetDesktopWindow(), &r, TRUE);
   CEscWindow Window;
   Window.Init (ghInstance, hWnd, 0, &r);
redo:
   psz = Window.PageDialog (ghInstance, IDR_MMLRESCUTSCENE, ResCutScenePage, &rcsd);
   if (psz && !_wcsicmp(psz, RedoSamePage())) {
      rcsd.iVScroll = Window.m_iExitVScroll;
      goto redo;
   }
   else if (psz && !_wcsicmp(psz, L"add")) {
      rcsd.iVScroll = Window.m_iExitVScroll;
      PCMMLNode2 pSub = rcsd.pNode->ContentAddNewNode ();
      pSub->NameSet (gapszActionType[0]);
      rcsd.fChanged = TRUE;
      goto redo;
   }

   if (!rcsd.fChanged) {
      delete rcsd.pNode;
      goto done;
   }

   // create new MML
   pRet = rcsd.pNode;   // so dont delete

   // delete transpros resources
   TransProsQuickFillResourceFromNode (pLib, pRet, lid, &lResIDNew);
   pLib->TransProsQuickDelete (&lResIDOrig, &lResIDNew);

done:
   return pRet;
}

