/*************************************************************************************
ResSpeakScript.cpp - Code for the SpeakScript resource.

begun 29/6/06 by Mike Rozak.
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

// RSSD - Information for OpenSpeakScriptDialog
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
} RSSD, *PRSSD;


static PWSTR gapszActionType[] = {L"Speak", L"Narration", L"Sound", L"Pause", L"Emote"};

// speak and narration params
static PWSTR gpszText = L"Text";
static PWSTR gpszTransPros = L"TransPros";
static PWSTR gpszSpeakStyle = L"SpeakStyle"; // speaking style
static PWSTR gpszKnowledge = L"Knowledge";
static PWSTR gpszNPCRelA = L"NPCRelA";
static PWSTR gpszNPCRelB = L"NPCRelB";
static PWSTR gpszNPCRelKnownByName = L"NPCRelKnownByName";

// Emote
static PWSTR gpszEmote = L"Emote";  // emote to display

// Pause
static PWSTR gpszTime = L"Time";    // puase time, in seconds

// sound
static PWSTR gpszWaitSound = L"WaitSound";    // if set to 1 then wait for sound to continue
static PWSTR gpszResource = L"Resource";      // resource to display


/*************************************************************************
ResSpeakScriptPage
*/
BOOL ResSpeakScriptPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PRSSD pRSSD = (PRSSD)pPage->m_pUserData;   // node to modify

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         TTSCacheDefaultGet (pPage, L"ttsfile");

         // scroll to right position
         if (pRSSD->iVScroll > 0) {
            pPage->VScroll (pRSSD->iVScroll);

            // when bring up pop-up dialog often they're scrolled wrong because
            // iVScoll was left as valeu, and they use defpage
            pRSSD->iVScroll = 0;

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

         PWSTR pszText = L"text:",
            pszEmote = L"emote:", pszTime = L"time:";
         DWORD dwTextLen = (DWORD)wcslen(pszText),
            dwEmoteLen = (DWORD)wcslen(pszEmote), dwTimeLen = (DWORD)wcslen(pszTime);
         DWORD dwIndex;

         WCHAR szTemp[1024];
         DWORD dwNeed;
         if (!wcsncmp(psz, pszText, dwTextLen)) {
            dwIndex = (DWORD)_wtoi(psz + dwTextLen);
            PCMMLNode2 pSub;
            pRSSD->pNode->ContentEnum (dwIndex, &psz, &pSub);
            if (!pSub)
               return TRUE;   // shouldnt happen

            szTemp[0] = 0;
            p->pControl->AttribGet (Text(), szTemp, sizeof(szTemp), &dwNeed);

            if (szTemp[0])
               pSub->AttribSetString (gpszText, szTemp);
            else
               pSub->AttribDelete (gpszText);

            pRSSD->fChanged = TRUE;
            //pPage->Exit (RedoSamePage());
            return TRUE;
         }
         else if (!wcsncmp(psz, pszEmote, dwEmoteLen)) {
            dwIndex = (DWORD)_wtoi(psz + dwEmoteLen);
            PCMMLNode2 pSub;
            pRSSD->pNode->ContentEnum (dwIndex, &psz, &pSub);
            if (!pSub)
               return TRUE;   // shouldnt happen

            szTemp[0] = 0;
            p->pControl->AttribGet (Text(), szTemp, sizeof(szTemp), &dwNeed);

            if (szTemp[0])
               pSub->AttribSetString (gpszEmote, szTemp);
            else
               pSub->AttribDelete (gpszEmote);

            pRSSD->fChanged = TRUE;
            //pPage->Exit (RedoSamePage());
            return TRUE;
         }
         else if (!wcsncmp(psz, pszTime, dwTimeLen)) {
            dwIndex = (DWORD)_wtoi(psz + dwTimeLen);
            PCMMLNode2 pSub;
            pRSSD->pNode->ContentEnum (dwIndex, &psz, &pSub);
            if (!pSub)
               return TRUE;   // shouldnt happen

            szTemp[0] = 0;
            p->pControl->AttribGet (Text(), szTemp, sizeof(szTemp), &dwNeed);

            if (szTemp[0])
               pSub->AttribSetString (gpszTime, szTemp);
            else
               pSub->AttribDelete (gpszTime);

            pRSSD->fChanged = TRUE;
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
            pszWaitSound = L"waitsound:", pszNPCRelKnownByName = L"npcrelknownbyname:";
         DWORD dwMoveUpLen = (DWORD)wcslen(pszMoveUp), dwDelLen = (DWORD)wcslen(pszDel), dwMoveDownLen = (DWORD)wcslen(pszMoveDown),
            dwWaitSoundLen = (DWORD)wcslen(pszWaitSound), dwNPCRelKnownByNameLen= (DWORD)wcslen(pszNPCRelKnownByName);

         DWORD dwIndex;
         if (!wcsncmp(psz, pszMoveUp, dwMoveUpLen)) {
            dwIndex = (DWORD)_wtoi(psz + dwMoveUpLen);
            PCMMLNode2 pSub;
            pRSSD->pNode->ContentEnum (dwIndex, &psz, &pSub);
            if (!pSub)
               return TRUE;   // shouldnt happen
            pRSSD->pNode->ContentRemove (dwIndex, FALSE);

            // insert higher
            pRSSD->pNode->ContentInsert (dwIndex-1, pSub);

            pRSSD->fChanged = TRUE;
            pPage->Exit (RedoSamePage());
            return TRUE;
         }
         else if (!wcsncmp(psz, pszMoveDown, dwMoveDownLen)) {
            dwIndex = (DWORD)_wtoi(psz + dwMoveDownLen);
            PCMMLNode2 pSub;
            pRSSD->pNode->ContentEnum (dwIndex, &psz, &pSub);
            if (!pSub)
               return TRUE;   // shouldnt happen
            pRSSD->pNode->ContentRemove (dwIndex, FALSE);

            // insert higher
            pRSSD->pNode->ContentInsert (dwIndex+1, pSub);

            pRSSD->fChanged = TRUE;
            pPage->Exit (RedoSamePage());
            return TRUE;
         }
         else if (!wcsncmp(psz, pszDel, dwDelLen)) {
            dwIndex = (DWORD)_wtoi(psz + dwDelLen);

            if (IDYES != pPage->MBYesNo (L"Are you sure you want to delete this action?"))
               return TRUE;

            pRSSD->pNode->ContentRemove (dwIndex);

            pRSSD->fChanged = TRUE;
            pPage->Exit (RedoSamePage());
            return TRUE;
         }
         else if (!wcsncmp(psz, pszWaitSound, dwWaitSoundLen)) {
            dwIndex = (DWORD)_wtoi(psz + dwWaitSoundLen);
            PCMMLNode2 pSub;
            pRSSD->pNode->ContentEnum (dwIndex, &psz, &pSub);
            if (!pSub)
               return TRUE;   // shouldnt happen

            if (p->pControl->AttribGetBOOL(Checked()))
               pSub->AttribSetString (gpszWaitSound, L"1");
            else
               pSub->AttribDelete (gpszWaitSound);

            pRSSD->fChanged = TRUE;
            // pPage->Exit (RedoSamePage());
            return TRUE;
         }
         else if (!wcsncmp(psz, pszNPCRelKnownByName, dwNPCRelKnownByNameLen)) {
            dwIndex = (DWORD)_wtoi(psz + dwNPCRelKnownByNameLen);
            PCMMLNode2 pSub;
            pRSSD->pNode->ContentEnum (dwIndex, &psz, &pSub);
            if (!pSub)
               return TRUE;   // shouldnt happen

            if (p->pControl->AttribGetBOOL(Checked()))
               pSub->AttribSetString (gpszNPCRelKnownByName, L"1");
            else
               pSub->AttribDelete (gpszNPCRelKnownByName);

            pRSSD->fChanged = TRUE;
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
            pRSSD->pNode->ContentEnum (dwIndex, &psz, &pSub);
            if (!pSub)
               return TRUE;   // shouldnt happen
            PWSTR pszSpeak = pSub->AttribGetString (gpszText);
            TTSCacheSpellCheck (pszSpeak, pPage->m_pWindow->m_hWnd);
            return TRUE;
         }
         if (!wcsncmp (psz, pszTTSSP, dwTTSSPLen)) {
            DWORD dwIndex = (DWORD)_wtoi(psz + dwTTSSPLen);
            PCMMLNode2 pSub;
            pRSSD->pNode->ContentEnum (dwIndex, &psz, &pSub);
            if (!pSub)
               return TRUE;   // shouldnt happen
            PWSTR pszSpeak = pSub->AttribGetString (gpszText);
            TTSCacheSpeak (pszSpeak, FALSE, FALSE, pPage->m_pWindow->m_hWnd);
            return TRUE;
         }
         if (!wcsncmp (psz, pszTTSTP, dwTTSTPLen)) {
            DWORD dwIndex = (DWORD)_wtoi(psz + dwTTSTPLen);
            PCMMLNode2 pSub;
            pRSSD->pNode->ContentEnum (dwIndex, &psz, &pSub);
            if (!pSub)
               return TRUE;   // shouldnt happen
            PWSTR pszSpeak = pSub->AttribGetString (gpszText);

            if (2 == pRSSD->pLib->TransProsQuickDialog (NULL, pPage->m_pWindow->m_hWnd, pRSSD->lid,
               (PWSTR)pRSSD->pRes->m_memName.p, pszSpeak)) {

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
            pRSSD->pNode->ContentEnum (dwIndex, &psz, &pSub);
            if (!pSub)
               return TRUE;   // shouldnt happen
            PWSTR pszSpeak = pSub->AttribGetString (gpszText);
            if (pRSSD->pLib->TransProsQuickDeleteUI (pPage, pszSpeak, pRSSD->lid)) {
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

         PWSTR pszActionType = L"at:", pszTortp = L"tortp:", pszSpeakStyle = L"speakstyle:";
         DWORD dwActionTypeLen = (DWORD)wcslen(pszActionType), dwTortpLen = (DWORD)wcslen(pszTortp),
            dwSpeakStyleLen = (DWORD)wcslen(pszSpeakStyle);
         DWORD dwIndex;

         if (!wcsncmp (psz, pszActionType, dwActionTypeLen)) {
            dwIndex = (DWORD)_wtoi(psz + dwActionTypeLen);
            PCMMLNode2 pSub = NULL;
            pRSSD->pNode->ContentEnum (dwIndex, &psz, &pSub);
            if (!pSub)
               return TRUE;   // shouldnt happen

            if (!_wcsicmp(gapszActionType[p->dwCurSel], pSub->NameGet()))
               return TRUE;   // same, so no change

            // else, change
            pRSSD->pNode->ContentRemove (dwIndex);
            pSub = new CMMLNode2;
            if (pSub) {
               pSub->NameSet (gapszActionType[p->dwCurSel]);
               pRSSD->pNode->ContentInsert (dwIndex, pSub);
            }

            pRSSD->fChanged = TRUE;
            pPage->Exit (RedoSamePage());
            return TRUE;
         }
         else if (!wcsncmp (psz, pszSpeakStyle, dwSpeakStyleLen)) {
            dwIndex = (DWORD)_wtoi(psz + dwSpeakStyleLen);
            PCMMLNode2 pSub = NULL;
            pRSSD->pNode->ContentEnum (dwIndex, &psz, &pSub);
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

            pRSSD->fChanged = TRUE;
            // pPage->Exit (RedoSamePage());
            return TRUE;
         }
         else if (!wcsncmp (psz, pszTortp, dwTortpLen)) {
            dwIndex = (DWORD)_wtoi(psz + dwTortpLen);
            PCMMLNode2 pSub = NULL;
            pRSSD->pNode->ContentEnum (dwIndex, &psz, &pSub);
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
                  pRSSD->plTransPros->Num() ? (PWSTR)pRSSD->plTransPros->Get(0) : L"Unknown");
            }
            else {
               pSub->AttribDelete (gpszTransPros);
               // no need to add text since will assume that
            }

            pRSSD->fChanged = TRUE;
            pPage->Exit (RedoSamePage());
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
            p->pList = pRSSD->plTransPros;
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"sounds")) {
            p->pList = pRSSD->plSounds;
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"knowledge")) {
            p->pList = pRSSD->plKnowledge;
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"NPCRel")) {
            p->pList = pRSSD->plNPCRel;
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
            pRSSD->pNode->ContentEnum (dwIndex, &psz, &pSub);
            if (!pSub)
               return TRUE;   // shouldnt happen

            // make sure they're not the same
            psz = pSub->AttribGetString (gpszTransPros);
            int iNewSel = p->pControl->AttribGetInt (CurSel());
            PWSTR pszNew = (iNewSel >= 0) ? (PWSTR)pRSSD->plTransPros->Get((DWORD)iNewSel) : NULL;
            if (!psz || !pszNew) {
               if (psz == pszNew)
                  return TRUE;   // no change
            }
            else {
               if (!_wcsicmp(psz, pszNew))
                  return TRUE;   // no change
            }
            
            pSub->AttribSetString (gpszTransPros, pszNew ? pszNew : L"Unknown");

            pRSSD->fChanged = TRUE;
            //pPage->Exit (RedoSamePage());
            return TRUE;
         }
         else if (!wcsncmp(psz, pszSound, dwSoundLen)) {
            dwIndex = (DWORD)_wtoi(psz + dwSoundLen);
            PCMMLNode2 pSub = NULL;
            pRSSD->pNode->ContentEnum (dwIndex, &psz, &pSub);
            if (!pSub)
               return TRUE;   // shouldnt happen

            // make sure they're not the same
            psz = pSub->AttribGetString (gpszResource);
            int iNewSel = p->pControl->AttribGetInt (CurSel());
            PWSTR pszNew = (iNewSel >= 0) ? (PWSTR)pRSSD->plSounds->Get((DWORD)iNewSel) : NULL;
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

            pRSSD->fChanged = TRUE;
            //pPage->Exit (RedoSamePage());
            return TRUE;
         }
         else if (!wcsncmp(psz, pszKnowledge, dwKnowledgeLen)) {
            dwIndex = (DWORD)_wtoi(psz + dwKnowledgeLen);
            PCMMLNode2 pSub = NULL;
            pRSSD->pNode->ContentEnum (dwIndex, &psz, &pSub);
            if (!pSub)
               return TRUE;   // shouldnt happen

            // make sure they're not the same
            psz = pSub->AttribGetString (gpszKnowledge);
            int iNewSel = p->pControl->AttribGetInt (CurSel());
            PWSTR pszNew = (iNewSel >= 0) ? (PWSTR)pRSSD->plKnowledge->Get((DWORD)iNewSel) : NULL;
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

            pRSSD->fChanged = TRUE;
            //pPage->Exit (RedoSamePage());
            return TRUE;
         }
         else if (!wcsncmp(psz, pszNPCRelA, dwNPCRelALen)) {
            dwIndex = (DWORD)_wtoi(psz + dwNPCRelALen);
            PCMMLNode2 pSub = NULL;
            pRSSD->pNode->ContentEnum (dwIndex, &psz, &pSub);
            if (!pSub)
               return TRUE;   // shouldnt happen

            // make sure they're not the same
            psz = pSub->AttribGetString (gpszNPCRelA);
            int iNewSel = p->pControl->AttribGetInt (CurSel());
            PWSTR pszNew = (iNewSel >= 0) ? (PWSTR)pRSSD->plNPCRel->Get((DWORD)iNewSel) : NULL;
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

            pRSSD->fChanged = TRUE;
            //pPage->Exit (RedoSamePage());
            return TRUE;
         }
         else if (!wcsncmp(psz, pszNPCRelB, dwNPCRelBLen)) {
            dwIndex = (DWORD)_wtoi(psz + dwNPCRelBLen);
            PCMMLNode2 pSub = NULL;
            pRSSD->pNode->ContentEnum (dwIndex, &psz, &pSub);
            if (!pSub)
               return TRUE;   // shouldnt happen

            // make sure they're not the same
            psz = pSub->AttribGetString (gpszNPCRelB);
            int iNewSel = p->pControl->AttribGetInt (CurSel());
            PWSTR pszNew = (iNewSel >= 0) ? (PWSTR)pRSSD->plNPCRel->Get((DWORD)iNewSel) : NULL;
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

            pRSSD->fChanged = TRUE;
            //pPage->Exit (RedoSamePage());
            return TRUE;
         }
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"SpeakScript resource";
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
            DWORD dwNum = pRSSD->pNode->ContentNum();
            PWSTR papszColor[sizeof(gapszActionType)/sizeof(PWSTR)] = {
               L" bgcolor=#c0ff00", L" bgcolor=#80c080", L" bgcolor=#c0c0ff", L" bgcolor=#c0c0c0",
               L" bgcolor=#ffc0c0"};

            for (i = 0; i < dwNum; i++) {
               PCMMLNode2 pSub = NULL;
               PWSTR psz = NULL;
               pRSSD->pNode->ContentEnum (i, &psz, &pSub);
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
               if (pRSSD->fReadOnly)
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
               case 1:  // narration
                  // text or transplanted prosody
                  psz = pSub->AttribGetString (gpszTransPros);
                  fTransPros = psz ? TRUE : FALSE;

                  MemCat (&gMemTemp,
                     L"<xComboTextOrTransPros width=20% ");
                  if (pRSSD->fReadOnly)
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
                     if (pRSSD->fReadOnly)
                        MemCat (&gMemTemp,
                        L"enabled=false ");
                     MemCat (&gMemTemp,
                        L"cursel=");
                     psz = pSub->AttribGetString (gpszTransPros);
                     pdwIndex = psz ? (DWORD*) pRSSD->phTransPros->Find (psz) : NULL;
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

                     if (pRSSD->fReadOnly)
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

                     if (!pRSSD->fReadOnly) {
                        MemCat (&gMemTemp,
                           L"<button width=25% name=ttstp:");
                        MemCat (&gMemTemp, (int)i);
                        MemCat (&gMemTemp,
                           L">Record transplanted prosody</button>");

                        MemCat (&gMemTemp,
                           L"<button width=25% name=ttsdt:");
                        MemCat (&gMemTemp, (int)i);
                        PWSTR pszText = psz;
                        DWORD dwID = pRSSD->pLib->TransProsQuickFind (pszText, pRSSD->lid);
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
                     if (pRSSD->fReadOnly)
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
                  } // if speak

                  // knowledge
                  MemCat (&gMemTemp,
                     L"<br/>Knowledge: "
                     L"<bold><filteredlist width=50% additem=\"\" sort=false listname=knowledge ");
                  if (pRSSD->fReadOnly)
                     MemCat (&gMemTemp,
                     L"enabled=false ");
                  MemCat (&gMemTemp,
                     L"cursel=");
                  psz = pSub->AttribGetString (gpszKnowledge);
                  pdwIndex = psz ? (DWORD*) pRSSD->phKnowledge->Find (psz) : NULL;
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
                  if (pRSSD->fReadOnly)
                     MemCat (&gMemTemp,
                     L"enabled=false ");
                  MemCat (&gMemTemp,
                     L"cursel=");
                  psz = pSub->AttribGetString (gpszNPCRelA);
                  pdwIndex = psz ? (DWORD*) pRSSD->phNPCRel->Find (psz) : NULL;
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
                  if (pRSSD->fReadOnly)
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
                  if (pRSSD->fReadOnly)
                     MemCat (&gMemTemp,
                     L"enabled=false ");
                  MemCat (&gMemTemp,
                     L"cursel=");
                  psz = pSub->AttribGetString (gpszNPCRelB);
                  pdwIndex = psz ? (DWORD*) pRSSD->phNPCRel->Find (psz) : NULL;
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
                  if (pRSSD->fReadOnly)
                     MemCat (&gMemTemp,
                     L"enabled=false ");
                  MemCat (&gMemTemp,
                     L"cursel=");
                  psz = pSub->AttribGetString (gpszResource);
                  pdwIndex = psz ? (DWORD*) pRSSD->phSounds->Find (psz) : NULL;
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
                  if (pRSSD->fReadOnly)
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

               case 3:  // Pause
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
                  if (pRSSD->fReadOnly)
                     MemCat (&gMemTemp, L" readonly=true");
                  MemCat (&gMemTemp,
                     L"></edit></bold>");
                  break;


               case 4:  // Emote
                  // emote
                  MemCat (&gMemTemp,
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
                  if (pRSSD->fReadOnly)
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
               if (pRSSD->fReadOnly || !i)
                  MemCat (&gMemTemp,
                     L"enabled=FALSE ");
               MemCat (&gMemTemp,
                  L"name=moveup:");
               MemCat (&gMemTemp,
                  (int)i);
               MemCat (&gMemTemp,
                  L"/>"
                  L"<button style=box color=#ff0000 ");
               if (pRSSD->fReadOnly)
                  MemCat (&gMemTemp,
                     L"enabled=FALSE ");
               MemCat (&gMemTemp,
                  L"name=del:");
               MemCat (&gMemTemp,
                  (int)i);
               MemCat (&gMemTemp,
                  L"/>"
                  L"<button style=downtriangle ");
               if (pRSSD->fReadOnly || (i+1 >= dwNum))
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
TransProsQuickFillResourceFromNode - Passed in a SpeakScript, CutScene,
or ConvScript resources, and extracts the strings, looking for quick
transplanted prosody IDs.

inputs
   PCMIFLLib            pLib - Library it's in
   PCMMLNode2           pNode - Node
   LANGID               lid - Language ID
   PCListFixed          plResID - Initialized to sizeof(DWORD) and filled in,
                        as per TransProsQuickEnum
returns
   none
*/
void TransProsQuickFillResourceFromNode (PCMIFLLib pLib, PCMMLNode2 pNode, LANGID lid,
                                         PCListFixed plResID)
{
   CListVariable l;
   PWSTR psz;
   PCMMLNode2 pSub;
   DWORD i;
   if (pNode) for (i = 0; i < pNode->ContentNum(); i++) {
      pSub = NULL;
      pNode->ContentEnum (i, &psz, &pSub);
      if (!pSub)
         continue;

      psz = pSub->AttribGetString (gpszText);
      if (!psz)
         continue;

      l.Add (psz, (wcslen(psz)+1)*sizeof(WCHAR));
   }

   pLib->TransProsQuickEnum (&l, NULL, lid, plResID);
}

/*************************************************************************
ResSpeakScriptEdit - Modify a resource SpeakScript. Uses standard API from mifl.h, ResourceEdit.
*/
PCMMLNode2 ResSpeakScriptEdit (HWND hWnd, LANGID lid, PCMMLNode2 pIn, BOOL fReadOnly,
                            PCMIFLProj pProj, PCMIFLLib pLib, PCMIFLResource pRes)
{
   PCMMLNode2 pRet = NULL;
   RSSD rssd;
   CListVariable lTransPros, lSounds, lKnowledge, lNPCRel;
   CHashString hTransPros, hSounds, hKnowledge, hNPCRel;
   memset (&rssd, 0, sizeof(rssd));
   rssd.fReadOnly = fReadOnly;
   rssd.pNode = pIn->Clone();
   rssd.plTransPros = &lTransPros;
   rssd.phTransPros = &hTransPros;
   rssd.plSounds = &lSounds;
   rssd.phSounds = &hSounds;
   rssd.plKnowledge = &lKnowledge;
   rssd.phKnowledge = &hKnowledge;
   rssd.plNPCRel = &lNPCRel;
   rssd.phNPCRel = &hNPCRel;
   PWSTR apsz[5], apszClass[5];
   apsz[0] = CircumrealityTransPros();
   FillListWithResources (apsz, 1, NULL, 0, pProj, rssd.plTransPros, rssd.phTransPros);
   apsz[0] = CircumrealityWave();
   apsz[1] = CircumrealityMusic();
   FillListWithResources (apsz, 2, NULL, 0, pProj, rssd.plSounds, rssd.phSounds);
   apszClass[0] = L"cKnowledge";
   FillListWithResources (NULL, 0, apszClass, 1, pProj, rssd.plKnowledge, rssd.phKnowledge);
   apszClass[0] = L"cCharacter";
   apszClass[1] = L"cFaction";
   FillListWithResources (NULL, 0, apszClass, 2, pProj, rssd.plNPCRel, rssd.phNPCRel);
   PrependOptionToList (L"ThisNPC", rssd.plNPCRel, rssd.phNPCRel);  // so can select this NPC as relationship
   rssd.lid = lid;
   rssd.pLib = pLib;
   rssd.pRes = pRes;

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
   psz = Window.PageDialog (ghInstance, IDR_MMLRESSPEAKSCRIPT, ResSpeakScriptPage, &rssd);
   if (psz && !_wcsicmp(psz, RedoSamePage())) {
      rssd.iVScroll = Window.m_iExitVScroll;
      goto redo;
   }
   else if (psz && !_wcsicmp(psz, L"add")) {
      rssd.iVScroll = Window.m_iExitVScroll;
      PCMMLNode2 pSub = rssd.pNode->ContentAddNewNode ();
      pSub->NameSet (gapszActionType[0]);
      rssd.fChanged = TRUE;
      goto redo;
   }

   if (!rssd.fChanged) {
      delete rssd.pNode;
      goto done;
   }

   // create new MML
   pRet = rssd.pNode;   // so dont delete

   // delete transpros resources
   TransProsQuickFillResourceFromNode (pLib, pRet, lid, &lResIDNew);
   pLib->TransProsQuickDelete (&lResIDOrig, &lResIDNew);

done:
   return pRet;
}

