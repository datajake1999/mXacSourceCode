/*************************************************************************************
ResConvScript.cpp - Code for the ConvScript resource.

begun 5/7/06 by Mike Rozak.
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

// RCOSD - Information for OpenConvScriptDialog
typedef struct {
   PCMMLNode2  pNode;

   PCListVariable plTransPros;   // list of transplanted prosody resoruce
   PCHashString   phTransPros;   // hash for transplanted prosody resoruce
   PCListVariable plSounds;      // list of sound resources
   PCHashString   phSounds;      // like plObjects
   PCListVariable plKnowledge;   // knowledge objects
   PCHashString   phKnowledge;   // knowledge objects
   PCListVariable plNPCRel;      // NPC can have relationship with
   PCHashString   phNPCRel;      // NPC can have relationship with
   LANGID         lid;           // language
   PCMIFLLib      pLib;          // library it's in
   PCMIFLResource pRes;          // resource it's in

   int         iVScroll;         // where to scroll to
   BOOL        fReadOnly;        // set to TRUE if is read only file
   BOOL        fChanged;         // set to TRUE if changed
} RCOSD, *PRCOSD;


static PWSTR gapszActionType[] = {L"Speak", L"Narration", L"Sound", L"Rate", L"Emote"};
static PWSTR gapszNPC[] = {L"1", L"2", L"3", L"4"};
static PWSTR gapszToWhom[] = {L"All", L"1", L"2", L"3", L"4", L"PC"};

// speak and narration params
static PWSTR gpszToWhom = L"ToWhom";
static PWSTR gpszNPC = L"NPC";
static PWSTR gpszText = L"Text";
static PWSTR gpszTransPros = L"TransPros";
static PWSTR gpszSpeakStyle = L"SpeakStyle"; // speaking style
static PWSTR gpszKnowledge = L"Knowledge";
static PWSTR gpszNPCRelA = L"NPCRelA";
static PWSTR gpszNPCRelB = L"NPCRelB";
static PWSTR gpszNPCRelKnownByName = L"NPCRelKnownByName";
static PWSTR gpszLang = L"Lang";

// Emote
//static PWSTR gpszNPC = L"NPC";
static PWSTR gpszEmote = L"Emote";  // emote to display
//static PWSTR gpszToWhom = L"ToWhom";

// Pause
static PWSTR gpszTime = L"Time";    // puase time, in seconds

// sound
//static PWSTR gpszNPC = L"NPC";
static PWSTR gpszWaitSound = L"WaitSound";    // if set to 1 then wait for sound to continue
static PWSTR gpszResource = L"Resource";      // resource to display


/*************************************************************************
ResConvScriptPage
*/
BOOL ResConvScriptPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PRCOSD pRCOSD = (PRCOSD)pPage->m_pUserData;   // node to modify

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         TTSCacheDefaultGet (pPage, L"ttsfile");

         // scroll to right position
         if (pRCOSD->iVScroll > 0) {
            pPage->VScroll (pRCOSD->iVScroll);

            // when bring up pop-up dialog often they're scrolled wrong because
            // iVScoll was left as valeu, and they use defpage
            pRCOSD->iVScroll = 0;

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
            pszEmote = L"emote:", pszTime = L"time:";
         DWORD dwTextLen = (DWORD)wcslen(pszText), dwLangLen = (DWORD)wcslen(pszLang),
            dwEmoteLen = (DWORD)wcslen(pszEmote), dwTimeLen = (DWORD)wcslen(pszTime);
         DWORD dwIndex;

         WCHAR szTemp[1024];
         DWORD dwNeed;
         if (!wcsncmp(psz, pszText, dwTextLen)) {
            dwIndex = (DWORD)_wtoi(psz + dwTextLen);
            PCMMLNode2 pSub;
            pRCOSD->pNode->ContentEnum (dwIndex, &psz, &pSub);
            if (!pSub)
               return TRUE;   // shouldnt happen

            szTemp[0] = 0;
            p->pControl->AttribGet (Text(), szTemp, sizeof(szTemp), &dwNeed);

            if (szTemp[0])
               pSub->AttribSetString (gpszText, szTemp);
            else
               pSub->AttribDelete (gpszText);

            pRCOSD->fChanged = TRUE;
            //pPage->Exit (RedoSamePage());
            return TRUE;
         }
         else if (!wcsncmp(psz, pszLang, dwLangLen)) {
            dwIndex = (DWORD)_wtoi(psz + dwLangLen);
            PCMMLNode2 pSub;
            pRCOSD->pNode->ContentEnum (dwIndex, &psz, &pSub);
            if (!pSub)
               return TRUE;   // shouldnt happen

            szTemp[0] = 0;
            p->pControl->AttribGet (Text(), szTemp, sizeof(szTemp), &dwNeed);

            if (szTemp[0])
               pSub->AttribSetString (gpszLang, szTemp);
            else
               pSub->AttribDelete (gpszLang);

            pRCOSD->fChanged = TRUE;
            //pPage->Exit (RedoSamePage());
            return TRUE;
         }
         else if (!wcsncmp(psz, pszEmote, dwEmoteLen)) {
            dwIndex = (DWORD)_wtoi(psz + dwEmoteLen);
            PCMMLNode2 pSub;
            pRCOSD->pNode->ContentEnum (dwIndex, &psz, &pSub);
            if (!pSub)
               return TRUE;   // shouldnt happen

            szTemp[0] = 0;
            p->pControl->AttribGet (Text(), szTemp, sizeof(szTemp), &dwNeed);

            if (szTemp[0])
               pSub->AttribSetString (gpszEmote, szTemp);
            else
               pSub->AttribDelete (gpszEmote);

            pRCOSD->fChanged = TRUE;
            //pPage->Exit (RedoSamePage());
            return TRUE;
         }
         else if (!wcsncmp(psz, pszTime, dwTimeLen)) {
            dwIndex = (DWORD)_wtoi(psz + dwTimeLen);
            PCMMLNode2 pSub;
            pRCOSD->pNode->ContentEnum (dwIndex, &psz, &pSub);
            if (!pSub)
               return TRUE;   // shouldnt happen

            szTemp[0] = 0;
            p->pControl->AttribGet (Text(), szTemp, sizeof(szTemp), &dwNeed);

            if (szTemp[0])
               pSub->AttribSetString (gpszTime, szTemp);
            else
               pSub->AttribDelete (gpszTime);

            pRCOSD->fChanged = TRUE;
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
            pszWaitSound = L"waitsound:", pszNPCRelKnownByName = L"npcrelknownbyname:";;
         DWORD dwMoveUpLen = (DWORD)wcslen(pszMoveUp), dwDelLen = (DWORD)wcslen(pszDel), dwMoveDownLen = (DWORD)wcslen(pszMoveDown),
            dwWaitSoundLen = (DWORD)wcslen(pszWaitSound), dwNPCRelKnownByNameLen= (DWORD)wcslen(pszNPCRelKnownByName);

         DWORD dwIndex;
         if (!wcsncmp(psz, pszMoveUp, dwMoveUpLen)) {
            dwIndex = (DWORD)_wtoi(psz + dwMoveUpLen);
            PCMMLNode2 pSub;
            pRCOSD->pNode->ContentEnum (dwIndex, &psz, &pSub);
            if (!pSub)
               return TRUE;   // shouldnt happen
            pRCOSD->pNode->ContentRemove (dwIndex, FALSE);

            // insert higher
            pRCOSD->pNode->ContentInsert (dwIndex-1, pSub);

            pRCOSD->fChanged = TRUE;
            pPage->Exit (RedoSamePage());
            return TRUE;
         }
         else if (!wcsncmp(psz, pszMoveDown, dwMoveDownLen)) {
            dwIndex = (DWORD)_wtoi(psz + dwMoveDownLen);
            PCMMLNode2 pSub;
            pRCOSD->pNode->ContentEnum (dwIndex, &psz, &pSub);
            if (!pSub)
               return TRUE;   // shouldnt happen
            pRCOSD->pNode->ContentRemove (dwIndex, FALSE);

            // insert higher
            pRCOSD->pNode->ContentInsert (dwIndex+1, pSub);

            pRCOSD->fChanged = TRUE;
            pPage->Exit (RedoSamePage());
            return TRUE;
         }
         else if (!wcsncmp(psz, pszDel, dwDelLen)) {
            dwIndex = (DWORD)_wtoi(psz + dwDelLen);

            if (IDYES != pPage->MBYesNo (L"Are you sure you want to delete this action?"))
               return TRUE;

            pRCOSD->pNode->ContentRemove (dwIndex);

            pRCOSD->fChanged = TRUE;
            pPage->Exit (RedoSamePage());
            return TRUE;
         }
         else if (!wcsncmp(psz, pszWaitSound, dwWaitSoundLen)) {
            dwIndex = (DWORD)_wtoi(psz + dwWaitSoundLen);
            PCMMLNode2 pSub;
            pRCOSD->pNode->ContentEnum (dwIndex, &psz, &pSub);
            if (!pSub)
               return TRUE;   // shouldnt happen

            if (p->pControl->AttribGetBOOL(Checked()))
               pSub->AttribSetString (gpszWaitSound, L"1");
            else
               pSub->AttribDelete (gpszWaitSound);

            pRCOSD->fChanged = TRUE;
            // pPage->Exit (RedoSamePage());
            return TRUE;
         }
         else if (!wcsncmp(psz, pszNPCRelKnownByName, dwNPCRelKnownByNameLen)) {
            dwIndex = (DWORD)_wtoi(psz + dwNPCRelKnownByNameLen);
            PCMMLNode2 pSub;
            pRCOSD->pNode->ContentEnum (dwIndex, &psz, &pSub);
            if (!pSub)
               return TRUE;   // shouldnt happen

            if (p->pControl->AttribGetBOOL(Checked()))
               pSub->AttribSetString (gpszNPCRelKnownByName, L"1");
            else
               pSub->AttribDelete (gpszNPCRelKnownByName);

            pRCOSD->fChanged = TRUE;
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
            pRCOSD->pNode->ContentEnum (dwIndex, &psz, &pSub);
            if (!pSub)
               return TRUE;   // shouldnt happen
            PWSTR pszSpeak = pSub->AttribGetString (gpszText);
            TTSCacheSpellCheck (pszSpeak, pPage->m_pWindow->m_hWnd);
            return TRUE;
         }
         if (!wcsncmp (psz, pszTTSSP, dwTTSSPLen)) {
            DWORD dwIndex = (DWORD)_wtoi(psz + dwTTSSPLen);
            PCMMLNode2 pSub;
            pRCOSD->pNode->ContentEnum (dwIndex, &psz, &pSub);
            if (!pSub)
               return TRUE;   // shouldnt happen
            PWSTR pszSpeak = pSub->AttribGetString (gpszText);
            TTSCacheSpeak (pszSpeak, FALSE, FALSE, pPage->m_pWindow->m_hWnd);
            return TRUE;
         }
         if (!wcsncmp (psz, pszTTSTP, dwTTSTPLen)) {
            DWORD dwIndex = (DWORD)_wtoi(psz + dwTTSTPLen);
            PCMMLNode2 pSub;
            pRCOSD->pNode->ContentEnum (dwIndex, &psz, &pSub);
            if (!pSub)
               return TRUE;   // shouldnt happen
            PWSTR pszSpeak = pSub->AttribGetString (gpszText);

            if (2 == pRCOSD->pLib->TransProsQuickDialog (NULL, pPage->m_pWindow->m_hWnd, pRCOSD->lid,
               (PWSTR)pRCOSD->pRes->m_memName.p, pszSpeak)) {

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
            pRCOSD->pNode->ContentEnum (dwIndex, &psz, &pSub);
            if (!pSub)
               return TRUE;   // shouldnt happen
            PWSTR pszSpeak = pSub->AttribGetString (gpszText);
            if (pRCOSD->pLib->TransProsQuickDeleteUI (pPage, pszSpeak, pRCOSD->lid)) {
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
            pszNPC = L"npc:", pszToWhom = L"towhom:";
         DWORD dwActionTypeLen = (DWORD)wcslen(pszActionType), dwTortpLen = (DWORD)wcslen(pszTortp),
            dwSpeakStyleLen = (DWORD)wcslen(pszSpeakStyle),
            dwNPCLen = (DWORD)wcslen(pszNPC), dwToWhomLen = (DWORD)wcslen(pszToWhom);
         DWORD dwIndex;

         if (!wcsncmp (psz, pszActionType, dwActionTypeLen)) {
            dwIndex = (DWORD)_wtoi(psz + dwActionTypeLen);
            PCMMLNode2 pSub = NULL;
            pRCOSD->pNode->ContentEnum (dwIndex, &psz, &pSub);
            if (!pSub)
               return TRUE;   // shouldnt happen

            if (!_wcsicmp(gapszActionType[p->dwCurSel], pSub->NameGet()))
               return TRUE;   // same, so no change

            // else, change
            pRCOSD->pNode->ContentRemove (dwIndex);
            pSub = new CMMLNode2;
            if (pSub) {
               pSub->NameSet (gapszActionType[p->dwCurSel]);
               pRCOSD->pNode->ContentInsert (dwIndex, pSub);
            }

            pRCOSD->fChanged = TRUE;
            pPage->Exit (RedoSamePage());
            return TRUE;
         }
         else if (!wcsncmp (psz, pszSpeakStyle, dwSpeakStyleLen)) {
            dwIndex = (DWORD)_wtoi(psz + dwSpeakStyleLen);
            PCMMLNode2 pSub = NULL;
            pRCOSD->pNode->ContentEnum (dwIndex, &psz, &pSub);
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

            pRCOSD->fChanged = TRUE;
            // pPage->Exit (RedoSamePage());
            return TRUE;
         }
         else if (!wcsncmp (psz, pszTortp, dwTortpLen)) {
            dwIndex = (DWORD)_wtoi(psz + dwTortpLen);
            PCMMLNode2 pSub = NULL;
            pRCOSD->pNode->ContentEnum (dwIndex, &psz, &pSub);
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
                  pRCOSD->plTransPros->Num() ? (PWSTR)pRCOSD->plTransPros->Get(0) : L"Unknown");
            }
            else {
               pSub->AttribDelete (gpszTransPros);
               // no need to add text since will assume that
            }

            pRCOSD->fChanged = TRUE;
            pPage->Exit (RedoSamePage());
            return TRUE;
         }
         else if (!wcsncmp (psz, pszNPC, dwNPCLen)) {
            dwIndex = (DWORD)_wtoi(psz + dwNPCLen);
            PCMMLNode2 pSub = NULL;
            pRCOSD->pNode->ContentEnum (dwIndex, &psz, &pSub);
            if (!pSub)
               return TRUE;   // shouldnt happen

            psz = pSub->AttribGetString (gpszNPC);
            if (!psz)
               psz = gapszNPC[0];
            if (!_wcsicmp(gapszNPC[p->dwCurSel], psz))
               return TRUE;   // same, so no change

            // else, change
            pSub->AttribSetString (gpszNPC, gapszNPC[p->dwCurSel]);

            pRCOSD->fChanged = TRUE;
            // pPage->Exit (RedoSamePage());
            return TRUE;
         }
         else if (!wcsncmp (psz, pszToWhom, dwToWhomLen)) {
            dwIndex = (DWORD)_wtoi(psz + dwToWhomLen);
            PCMMLNode2 pSub = NULL;
            pRCOSD->pNode->ContentEnum (dwIndex, &psz, &pSub);
            if (!pSub)
               return TRUE;   // shouldnt happen

            psz = pSub->AttribGetString (gpszToWhom);
            if (!psz)
               psz = gapszToWhom[0];
            if (!_wcsicmp(gapszToWhom[p->dwCurSel], psz))
               return TRUE;   // same, so no change

            // else, change
            pSub->AttribSetString (gpszToWhom, gapszToWhom[p->dwCurSel]);

            pRCOSD->fChanged = TRUE;
            // pPage->Exit (RedoSamePage());
            return TRUE;
         }
      }
      break;

   case ESCN_FILTEREDLISTQUERY:
      {
         PESCNFILTEREDLISTQUERY p = (PESCNFILTEREDLISTQUERY) pParam;
         PWSTR psz = p->pszListName;
         if (!psz)
            break;

         if (!_wcsicmp(psz, L"transpros")) {
            p->pList = pRCOSD->plTransPros;
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"sounds")) {
            p->pList = pRCOSD->plSounds;
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"knowledge")) {
            p->pList = pRCOSD->plKnowledge;
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"NPCRel")) {
            p->pList = pRCOSD->plNPCRel;
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

         PWSTR pszTransPros = L"transpros:", pszSound = L"sound:", pszKnowledge = L"knowledge:",
            pszNPCRelA = L"npcrela:", pszNPCRelB = L"npcrelb:";
         DWORD dwTransProsLen = (DWORD)wcslen(pszTransPros), dwSoundLen = (DWORD)wcslen(pszSound), dwKnowledgeLen = (DWORD)wcslen(pszKnowledge),
            dwNPCRelALen = (DWORD)wcslen(pszNPCRelA), dwNPCRelBLen = (DWORD)wcslen(pszNPCRelB);
         DWORD dwIndex;

         if (!wcsncmp(psz, pszTransPros, dwTransProsLen)) {
            dwIndex = (DWORD)_wtoi(psz + dwTransProsLen);
            PCMMLNode2 pSub = NULL;
            pRCOSD->pNode->ContentEnum (dwIndex, &psz, &pSub);
            if (!pSub)
               return TRUE;   // shouldnt happen

            // make sure they're not the same
            psz = pSub->AttribGetString (gpszTransPros);
            int iNewSel = p->pControl->AttribGetInt (CurSel());
            PWSTR pszNew = (iNewSel >= 0) ? (PWSTR)pRCOSD->plTransPros->Get((DWORD)iNewSel) : NULL;
            if (!psz || !pszNew) {
               if (psz == pszNew)
                  return TRUE;   // no change
            }
            else {
               if (!_wcsicmp(psz, pszNew))
                  return TRUE;   // no change
            }
            
            pSub->AttribSetString (gpszTransPros, pszNew ? pszNew : L"Unknown");

            pRCOSD->fChanged = TRUE;
            //pPage->Exit (RedoSamePage());
            return TRUE;
         }
         else if (!wcsncmp(psz, pszSound, dwSoundLen)) {
            dwIndex = (DWORD)_wtoi(psz + dwSoundLen);
            PCMMLNode2 pSub = NULL;
            pRCOSD->pNode->ContentEnum (dwIndex, &psz, &pSub);
            if (!pSub)
               return TRUE;   // shouldnt happen

            // make sure they're not the same
            psz = pSub->AttribGetString (gpszResource);
            int iNewSel = p->pControl->AttribGetInt (CurSel());
            PWSTR pszNew = (iNewSel >= 0) ? (PWSTR)pRCOSD->plSounds->Get((DWORD)iNewSel) : NULL;
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

            pRCOSD->fChanged = TRUE;
            //pPage->Exit (RedoSamePage());
            return TRUE;
         }
         else if (!wcsncmp(psz, pszKnowledge, dwKnowledgeLen)) {
            dwIndex = (DWORD)_wtoi(psz + dwKnowledgeLen);
            PCMMLNode2 pSub = NULL;
            pRCOSD->pNode->ContentEnum (dwIndex, &psz, &pSub);
            if (!pSub)
               return TRUE;   // shouldnt happen

            // make sure they're not the same
            psz = pSub->AttribGetString (gpszKnowledge);
            int iNewSel = p->pControl->AttribGetInt (CurSel());
            PWSTR pszNew = (iNewSel >= 0) ? (PWSTR)pRCOSD->plKnowledge->Get((DWORD)iNewSel) : NULL;
            if (!psz || !pszNew) {
               if (psz == pszNew)
                  return TRUE;   // no change
            }
            else {
               if (!_wcsicmp(psz, pszNew))
                  return TRUE;   // no change
            }
            
            if (pszNew)
               pSub->AttribSetString (gpszKnowledge, pszNew);
            else
               pSub->AttribDelete (gpszKnowledge);

            pRCOSD->fChanged = TRUE;
            //pPage->Exit (RedoSamePage());
            return TRUE;
         }
         else if (!wcsncmp(psz, pszNPCRelA, dwNPCRelALen)) {
            dwIndex = (DWORD)_wtoi(psz + dwNPCRelALen);
            PCMMLNode2 pSub = NULL;
            pRCOSD->pNode->ContentEnum (dwIndex, &psz, &pSub);
            if (!pSub)
               return TRUE;   // shouldnt happen

            // make sure they're not the same
            psz = pSub->AttribGetString (gpszNPCRelA);
            int iNewSel = p->pControl->AttribGetInt (CurSel());
            PWSTR pszNew = (iNewSel >= 0) ? (PWSTR)pRCOSD->plNPCRel->Get((DWORD)iNewSel) : NULL;
            if (!psz || !pszNew) {
               if (psz == pszNew)
                  return TRUE;   // no change
            }
            else {
               if (!_wcsicmp(psz, pszNew))
                  return TRUE;   // no change
            }
            
            if (pszNew)
               pSub->AttribSetString (gpszNPCRelA, pszNew);
            else
               pSub->AttribDelete (gpszNPCRelA);

            pRCOSD->fChanged = TRUE;
            //pPage->Exit (RedoSamePage());
            return TRUE;
         }
         else if (!wcsncmp(psz, pszNPCRelB, dwNPCRelBLen)) {
            dwIndex = (DWORD)_wtoi(psz + dwNPCRelBLen);
            PCMMLNode2 pSub = NULL;
            pRCOSD->pNode->ContentEnum (dwIndex, &psz, &pSub);
            if (!pSub)
               return TRUE;   // shouldnt happen

            // make sure they're not the same
            psz = pSub->AttribGetString (gpszNPCRelB);
            int iNewSel = p->pControl->AttribGetInt (CurSel());
            PWSTR pszNew = (iNewSel >= 0) ? (PWSTR)pRCOSD->plNPCRel->Get((DWORD)iNewSel) : NULL;
            if (!psz || !pszNew) {
               if (psz == pszNew)
                  return TRUE;   // no change
            }
            else {
               if (!_wcsicmp(psz, pszNew))
                  return TRUE;   // no change
            }
            
            if (pszNew)
               pSub->AttribSetString (gpszNPCRelB, pszNew);
            else
               pSub->AttribDelete (gpszNPCRelB);

            pRCOSD->fChanged = TRUE;
            //pPage->Exit (RedoSamePage());
            return TRUE;
         }     
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"ConvScript resource";
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

            // NPC
            MemCat (&gMemTemp,
                  L"<!xComboNPC>"
                  L"<bold><combobox macroattribute=1 cbheight=150>");
            for (i = 0; i < sizeof(gapszNPC) / sizeof(PWSTR); i++) {
               MemCat (&gMemTemp,
                  L"<elem name=");
               MemCat (&gMemTemp, (int)i);
               MemCat (&gMemTemp,
                  L">");
               // for digit-only prefix the NPC#
               if (iswdigit (gapszNPC[i][0]))
                  MemCatSanitize (&gMemTemp, L"NPC #");
               MemCatSanitize (&gMemTemp, gapszNPC[i]);
               MemCat (&gMemTemp,
                  L"</elem>");
            } // i
            MemCat (&gMemTemp, L"</combobox></bold></xComboNPC>");

            // toWhom
            MemCat (&gMemTemp,
                  L"<!xComboToWhom>"
                  L"<bold><combobox macroattribute=1 cbheight=150>");
            for (i = 0; i < sizeof(gapszToWhom) / sizeof(PWSTR); i++) {
               MemCat (&gMemTemp,
                  L"<elem name=");
               MemCat (&gMemTemp, (int)i);
               MemCat (&gMemTemp,
                  L">");
               // for digit-only prefix the NPC#
               if (iswdigit (gapszToWhom[i][0]))
                  MemCatSanitize (&gMemTemp, L"NPC #");
               MemCatSanitize (&gMemTemp, gapszToWhom[i]);
               MemCat (&gMemTemp,
                  L"</elem>");
            } // i
            MemCat (&gMemTemp, L"</combobox></bold></xComboToWhom>");

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

            DWORD i, j, k;
            DWORD dwNum = pRCOSD->pNode->ContentNum();
            PWSTR papszColor[sizeof(gapszActionType)/sizeof(PWSTR)] = {
               L" bgcolor=#c0ff00", L" bgcolor=#80c080", L" bgcolor=#c0c0ff", L" bgcolor=#c0c0c0",
               L" bgcolor=#ffc0c0"};

            for (i = 0; i < dwNum; i++) {
               PCMMLNode2 pSub = NULL;
               PWSTR psz = NULL;
               pRCOSD->pNode->ContentEnum (i, &psz, &pSub);
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
               if (pRCOSD->fReadOnly)
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

               // NPC
               MemCat (&gMemTemp,
                  L"<td width=20%");
               MemCat (&gMemTemp,
                  pszColor);
               MemCat (&gMemTemp,
                  L">");
               switch (j) {
               case 0:  // Speak
               case 2:  // sound
               case 4:  // Emote
                  psz = pSub->AttribGetString (gpszNPC);
                  if (!psz)
                     psz = gapszNPC[0];   // default
                  for (k = 0; k < sizeof(gapszNPC)/sizeof(PWSTR); k++)
                     if (!_wcsicmp(psz, gapszNPC[k]))
                        break;
                  if (k >= sizeof(gapszNPC)/sizeof(PWSTR))
                     k = 0;   // default

                  MemCat (&gMemTemp,
                     L"<xComboNPC width=100% ");
                  if (pRCOSD->fReadOnly)
                     MemCat (&gMemTemp, L"enabled=false ");
                  MemCat (&gMemTemp,
                     L"cursel=");
                  MemCat (&gMemTemp,
                     (int) k);
                  MemCat (&gMemTemp,
                     L" name=npc:");
                  MemCat (&gMemTemp,
                     (int) i);
                  MemCat (&gMemTemp,
                     L"/>");
                  break;

               default:
               case 1:  // narration
               case 3:  // Rate
                  // put nothing in
                  break;
               } // switch j, actinon type
               MemCat (&gMemTemp,
                  L"</td>");

               // details
               MemCat (&gMemTemp,
                  L"<td width=45%");
               MemCat (&gMemTemp,
                  pszColor);
               MemCat (&gMemTemp,
                  L">");

               // To whom
               switch (j) {
               case 0:  // Speak
               case 4:  // Emote
                  psz = pSub->AttribGetString (gpszToWhom);
                  if (!psz)
                     psz = gapszToWhom[0];   // default
                  for (k = 0; k < sizeof(gapszToWhom)/sizeof(PWSTR); k++)
                     if (!_wcsicmp(psz, gapszToWhom[k]))
                        break;
                  if (k >= sizeof(gapszToWhom)/sizeof(PWSTR))
                     k = 0;   // default

                  MemCat (&gMemTemp,
                     (j == 0) ? L"Speak to: " : L"Emote to: ");
                  MemCat (&gMemTemp,
                     L"<xComboToWhom width=50% ");
                  if (pRCOSD->fReadOnly)
                     MemCat (&gMemTemp, L"enabled=false ");
                  MemCat (&gMemTemp,
                     L"cursel=");
                  MemCat (&gMemTemp,
                     (int) k);
                  MemCat (&gMemTemp,
                     L" name=towhom:");
                  MemCat (&gMemTemp,
                     (int) i);
                  MemCat (&gMemTemp,
                     L"/><br/>");
                  break;
               } // j

               DWORD *pdwIndex;
               BOOL fTransPros;
               switch (j) {
               case 0:  // Speak
               case 1:  // narration
                  // text or transplanted prosody
                  psz = pSub->AttribGetString (gpszTransPros);
                  fTransPros = psz ? TRUE : FALSE;

                  MemCat (&gMemTemp,
                     L"<xComboTextOrTransPros width=20% ");
                  if (pRCOSD->fReadOnly)
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
                     if (pRCOSD->fReadOnly)
                        MemCat (&gMemTemp,
                        L"enabled=false ");
                     MemCat (&gMemTemp,
                        L"cursel=");
                     psz = pSub->AttribGetString (gpszTransPros);
                     pdwIndex = psz ? (DWORD*) pRCOSD->phTransPros->Find (psz) : NULL;
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

                     if (pRCOSD->fReadOnly)
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

                     if (!pRCOSD->fReadOnly) {
                        MemCat (&gMemTemp,
                           L"<button width=25% name=ttstp:");
                        MemCat (&gMemTemp, (int)i);
                        MemCat (&gMemTemp,
                           L">Record transplanted prosody</button>");

                        MemCat (&gMemTemp,
                           L"<button width=25% name=ttsdt:");
                        MemCat (&gMemTemp, (int)i);
                        PWSTR pszText = psz;
                        DWORD dwID = pRCOSD->pLib->TransProsQuickFind (pszText, pRCOSD->lid);
                        if (dwID == (DWORD)-1)
                           MemCat (&gMemTemp, L" enabled=false");
                        MemCat (&gMemTemp,
                           L">");
                        MemCat (&gMemTemp,
                           L"Delete trans. pros.</button>");
                     }

                  }

                  if (j == 0) {  // speak
                     MemCat (&gMemTemp,
                        L"<br/>");

                     // speaking style
                     MemCat (&gMemTemp,
                        L"<xComboSpeakStyle width=25% ");
                     if (pRCOSD->fReadOnly)
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


                  // language
                  MemCat (&gMemTemp,
                     L"    Lang: <bold><edit maxchars=32 width=50% name=lang:");
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

                  if (pRCOSD->fReadOnly)
                     MemCat (&gMemTemp, L" readonly=true");

                  MemCat (&gMemTemp,
                     L"></edit></bold>");

                  } // if speak

                  // knowledge
                  MemCat (&gMemTemp,
                     L"<br/>Knowledge: "
                     L"<bold><filteredlist width=50% additem=\"\" sort=false listname=knowledge ");
                  if (pRCOSD->fReadOnly)
                     MemCat (&gMemTemp,
                     L"enabled=false ");
                  MemCat (&gMemTemp,
                     L"cursel=");
                  psz = pSub->AttribGetString (gpszKnowledge);
                  pdwIndex = psz ? (DWORD*) pRCOSD->phKnowledge->Find (psz) : NULL;
                  MemCat (&gMemTemp,
                     pdwIndex ? (int)pdwIndex[0] : -1);
                  MemCat (&gMemTemp,
                     L" name=knowledge:");
                  MemCat (&gMemTemp,
                     (int) i);
                  MemCat (&gMemTemp,
                     L"/></bold>");

                  // relationship
                  MemCat (&gMemTemp,
                     L"<br/>Relationship NPC-A: "
                     L"<bold><filteredlist width=25% additem=\"\" sort=false listname=NPCrel ");
                  if (pRCOSD->fReadOnly)
                     MemCat (&gMemTemp,
                     L"enabled=false ");
                  MemCat (&gMemTemp,
                     L"cursel=");
                  psz = pSub->AttribGetString (gpszNPCRelA);
                  pdwIndex = psz ? (DWORD*) pRCOSD->phNPCRel->Find (psz) : NULL;
                  MemCat (&gMemTemp,
                     pdwIndex ? (int)pdwIndex[0] : -1);
                  MemCat (&gMemTemp,
                     L" name=npcrela:");
                  MemCat (&gMemTemp,
                     (int) i);
                  MemCat (&gMemTemp,
                     L"/></bold>");

                  MemCat (&gMemTemp,
                     L"     <button style=x checkbox=true ");
                  if (pRCOSD->fReadOnly)
                     MemCat (&gMemTemp, L"enabled=false ");
                  psz = pSub->AttribGetString (gpszNPCRelKnownByName);
                  if (psz && (psz[0] != L'0'))  // BUGFIX - Was psz[1]
                     MemCat (&gMemTemp, L"checked=true ");
                  MemCat (&gMemTemp,
                     L"name=npcrelknownbyname:");
                  MemCat (&gMemTemp,
                     (int)i);
                  MemCat (&gMemTemp,
                     L"><bold>KnownByName</bold></button>");

                  MemCat (&gMemTemp,
                     L"     NPC-B: "
                     L"<bold><filteredlist width=25% additem=\"\" sort=false listname=NPCrel ");
                  if (pRCOSD->fReadOnly)
                     MemCat (&gMemTemp,
                     L"enabled=false ");
                  MemCat (&gMemTemp,
                     L"cursel=");
                  psz = pSub->AttribGetString (gpszNPCRelB);
                  pdwIndex = psz ? (DWORD*) pRCOSD->phNPCRel->Find (psz) : NULL;
                  MemCat (&gMemTemp,
                     pdwIndex ? (int)pdwIndex[0] : -1);
                  MemCat (&gMemTemp,
                     L" name=npcrelb:");
                  MemCat (&gMemTemp,
                     (int) i);
                  MemCat (&gMemTemp,
                     L"/></bold>");

                  break;   // end of speak section

               case 2:  // sound
                  // sound
                  MemCat (&gMemTemp,
                     L"<bold><filteredlist width=50% additem=\"\" sort=false listname=sounds ");
                  if (pRCOSD->fReadOnly)
                     MemCat (&gMemTemp,
                     L"enabled=false ");
                  MemCat (&gMemTemp,
                     L"cursel=");
                  psz = pSub->AttribGetString (gpszResource);
                  pdwIndex = psz ? (DWORD*) pRCOSD->phSounds->Find (psz) : NULL;
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
                  if (pRCOSD->fReadOnly)
                     MemCat (&gMemTemp, L"enabled=false ");
                  psz = pSub->AttribGetString (gpszWaitSound);
                  if (psz && (psz[0] != L'0'))  // BUGFIX - was psz[1]
                     MemCat (&gMemTemp, L"checked=true ");
                  MemCat (&gMemTemp,
                     L"name=waitsound:");
                  MemCat (&gMemTemp,
                     (int)i);
                  MemCat (&gMemTemp,
                     L"><bold>Wait until sound completed</bold></button>");

                  break;

               case 3:  // Rate
                  // time
                  MemCat (&gMemTemp,
                     L"Time: <bold><edit maxchars=32 width=25% name=time:");
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
                  if (pRCOSD->fReadOnly)
                     MemCat (&gMemTemp, L" readonly=true");
                  MemCat (&gMemTemp,
                     L"></edit></bold>");
                  break;


               case 4:  // Emote
                  // emote
                  MemCat (&gMemTemp,
                     L"Emote: <bold><edit maxchars=32 width=25% name=emote:");
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
                  if (pRCOSD->fReadOnly)
                     MemCat (&gMemTemp, L" readonly=true");
                  MemCat (&gMemTemp,
                     L"></edit></bold>");

                  break;


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
               if (pRCOSD->fReadOnly || !i)
                  MemCat (&gMemTemp,
                     L"enabled=FALSE ");
               MemCat (&gMemTemp,
                  L"name=moveup:");
               MemCat (&gMemTemp,
                  (int)i);
               MemCat (&gMemTemp,
                  L"/>"
                  L"<button style=box color=#ff0000 ");
               if (pRCOSD->fReadOnly)
                  MemCat (&gMemTemp,
                     L"enabled=FALSE ");
               MemCat (&gMemTemp,
                  L"name=del:");
               MemCat (&gMemTemp,
                  (int)i);
               MemCat (&gMemTemp,
                  L"/>"
                  L"<button style=downtriangle ");
               if (pRCOSD->fReadOnly || (i+1 >= dwNum))
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
ResConvScriptEdit - Modify a resource ConvScript. Uses standard API from mifl.h, ResourceEdit.
*/
PCMMLNode2 ResConvScriptEdit (HWND hWnd, LANGID lid, PCMMLNode2 pIn, BOOL fReadOnly,
                            PCMIFLProj pProj, PCMIFLLib pLib, PCMIFLResource pRes)
{
   PCMMLNode2 pRet = NULL;
   RCOSD rcosd;
   CListVariable lTransPros, lSounds, lKnowledge, lNPCRel;
   CHashString hTransPros, hSounds, hKnowledge, hNPCRel;
   memset (&rcosd, 0, sizeof(rcosd));
   rcosd.fReadOnly = fReadOnly;
   rcosd.pNode = pIn->Clone();
   rcosd.plTransPros = &lTransPros;
   rcosd.phTransPros = &hTransPros;
   rcosd.plSounds = &lSounds;
   rcosd.phSounds = &hSounds;
   rcosd.plKnowledge = &lKnowledge;
   rcosd.phKnowledge = &hKnowledge;
   rcosd.plNPCRel = &lNPCRel;
   rcosd.phNPCRel = &hNPCRel;
   PWSTR apsz[5], apszClass[5];
   apsz[0] = CircumrealityTransPros();
   FillListWithResources (apsz, 1, NULL, 0, pProj, rcosd.plTransPros, rcosd.phTransPros);
   apsz[0] = CircumrealityWave();
   apsz[1] = CircumrealityMusic();
   FillListWithResources (apsz, 2, NULL, 0, pProj, rcosd.plSounds, rcosd.phSounds);
   apszClass[0] = L"cKnowledge";
   FillListWithResources (NULL, 0, apszClass, 1, pProj, rcosd.plKnowledge, rcosd.phKnowledge);
   apszClass[0] = L"cCharacter";
   apszClass[1] = L"cFaction";
   FillListWithResources (NULL, 0, apszClass, 2, pProj, rcosd.plNPCRel, rcosd.phNPCRel);
   PrependOptionToList (L"NPC4", rcosd.plNPCRel, rcosd.phNPCRel);
   PrependOptionToList (L"NPC3", rcosd.plNPCRel, rcosd.phNPCRel);
   PrependOptionToList (L"NPC2", rcosd.plNPCRel, rcosd.phNPCRel);
   PrependOptionToList (L"NPC1", rcosd.plNPCRel, rcosd.phNPCRel);

   rcosd.lid = lid;
   rcosd.pLib = pLib;
   rcosd.pRes = pRes;

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
   psz = Window.PageDialog (ghInstance, IDR_MMLRESCONVSCRIPT, ResConvScriptPage, &rcosd);
   if (psz && !_wcsicmp(psz, RedoSamePage())) {
      rcosd.iVScroll = Window.m_iExitVScroll;
      goto redo;
   }
   else if (psz && !_wcsicmp(psz, L"add")) {
      rcosd.iVScroll = Window.m_iExitVScroll;
      PCMMLNode2 pSub = rcosd.pNode->ContentAddNewNode ();
      pSub->NameSet (gapszActionType[0]);
      rcosd.fChanged = TRUE;
      goto redo;
   }

   if (!rcosd.fChanged) {
      delete rcosd.pNode;
      goto done;
   }

   // create new MML
   pRet = rcosd.pNode;   // so dont delete

   // delete transpros resources
   TransProsQuickFillResourceFromNode (pLib, pRet, lid, &lResIDNew);
   pLib->TransProsQuickDelete (&lResIDOrig, &lResIDNew);

done:
   return pRet;
}

