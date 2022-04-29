/*************************************************************************************
CMIFNLP.cpp - Code for natural language processing in Circumreality

begun 29/6/04 by Mike Rozak.
Copyright 2004 by Mike Rozak. All rights reserved
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





// NMD - Information for dialog
typedef struct {
   PCCircumrealityNLPParser pParser;       // parser to use
   PCCircumrealityNLPRuleSet pRuleSet;     // rule set to use
   DWORD       dwTab;            // tab being displayed
   LANGID      lid;              // language to send the commands as
   DWORD       dwSortBy;         // what to sort the list by, passed into RuleSort()

   BOOL        fReadOnly;        // set to TRUE if is read only file
   BOOL        fChanged;         // set to TRUE if changed
   int         iVScroll;         // amount to scroll
} NMD, *PNMD;

static PWSTR gpszEditedName = L"!EDITED!";

/*************************************************************************************
CCircumrealityNLPHyp::Constructor and destructor
*/
CCircumrealityNLPHyp::CCircumrealityNLPHyp (void)
{
   m_fProb = 1;
   m_dwRuleGroup = m_dwRuleElem = 0;
   m_fStable = FALSE;

   m_pdwIDCount = NULL;
   m_dwID = NULL;
   m_lParents.Init (sizeof(DWORD));
}

CCircumrealityNLPHyp::~CCircumrealityNLPHyp (void)
{
   // do nothing for now
}


/*************************************************************************************
CCircumrealityNLPHyp::CloneTo - Standard API
*/
BOOL CCircumrealityNLPHyp::CloneTo (CCircumrealityNLPHyp *pTo)
{
   if (!pTo->m_memTokens.Required (m_memTokens.m_dwCurPosn))
      return FALSE;
   memcpy (pTo->m_memTokens.p, m_memTokens.p, m_memTokens.m_dwCurPosn);
   pTo->m_memTokens.m_dwCurPosn = m_memTokens.m_dwCurPosn;

   pTo->m_fProb = m_fProb;
   pTo->m_dwRuleGroup = m_dwRuleGroup;
   pTo->m_dwRuleElem = m_dwRuleElem;
   pTo->m_fStable = m_fStable;

   pTo->m_dwID = m_dwID;
   pTo->m_pdwIDCount = m_pdwIDCount;
   pTo->m_lParents.Init (sizeof(DWORD), m_lParents.Get(0), m_lParents.Num());
   return TRUE;
}

/*************************************************************************************
CCircumrealityNLPHyp::Clone - Standard API
*/
CCircumrealityNLPHyp *CCircumrealityNLPHyp::Clone (void)
{
   CCircumrealityNLPHyp *pNew = new CCircumrealityNLPHyp;
   if (!pNew)
      return NULL;
   if (!CloneTo (pNew)) {
      delete pNew;
      return NULL;
   }
   return pNew;
}


/*************************************************************************************
CCircumrealityNLPHyp::NewID - Creates a new ID for the hypothesis
*/
void CCircumrealityNLPHyp::NewID (void)
{
   m_dwID = *m_pdwIDCount;
   *m_pdwIDCount = *m_pdwIDCount + 1;
}



/*************************************************************************************
CFCircumrealityCFG::Constructor and destructor
*/
CCircumrealityCFG::CCircumrealityCFG (void)
{
   m_lCFGNODE.Init (sizeof(CFGNODE));
   m_lStartToken.Init (sizeof(DWORD));
   m_dwNodeStart = 0;
   m_phTokens = NULL;
}

CCircumrealityCFG::~CCircumrealityCFG (void)
{
   // nothing for now
}



/*************************************************************************************
CCircumrealityCFG::Init - Initialize the CFG object to use the right tokens.

inputs
   PCHashString      phTokens - These are the tokens that will be used by the
                     CFG when if extracts the sub-string.
returns
   BOOL - TRUE if succes
*/
BOOL CCircumrealityCFG::Init (PCHashString phTokens)
{
   m_lCFGNODE.Clear();
   m_lStartToken.Clear();
   m_phTokens = phTokens;

   return TRUE;
}

/*************************************************************************************
CCircumrealityCFG::ExtractCFGFromSubString - This takes a string with CFG notation, such as
"[the] [bright] lantern of %1" and produces the CFG information from it.

inputs
   PWSTR          psz - String to parse, starting at the beginning
   DWORD          dwCount - Number of characters to parse, this can be called recursively.
   DWORD          dwEndIndex - All nodes will use this as the termination index
   DWORD          dwParseMode - If 0 allow everything: [], (), |, %n, and *n.
                              If 0x01 disallow wildcards %n and *n, allow the rest.
                              if 0x02 disallow alternatives, [], (), and |, allow the rest.
                              if 0x03 disallow everything
returns
   DWORD - Index into m_lCFGNODE where the first node was added, or -1 if error
*/
DWORD CCircumrealityCFG::ExtractCFGFromSubString (PWSTR psz, DWORD dwCount, DWORD dwEndIndex, DWORD dwParseMode)
{
   DWORD dwIndexAdded = -1;   // in case need to return
   CMem  mem;     // temporary memory
   CFGNODE node;

   DWORD dwCurSeqStart = -1;  // start current sequence, where do alternatives from
   DWORD dwCurSeqEnd = -1;    // end index of current sequence, where append to
   DWORD dwLastSeqStart = -1; // where start of the last sequence was
   DWORD i;

   while (dwCount) {
      // skip whitespace
      if (iswspace(psz[0])) {
         psz++;
         dwCount--;
         continue;
      }
      if (!dwCount)
         break;

      // see what character have
      switch (psz[0]) {
      default: // word
dodefault:
         {
            PWSTR pszStart = psz;
            psz++;
            dwCount--;  // BUGFIX - wasn't reducing count

            // repeat until get to end of word
            while (dwCount) {
               BOOL fWantBreak = FALSE;

               if (iswspace(psz[0]))
                  break;   // stop at whitespace

               switch (psz[0]) {
               case L'%':
               case L'*':
                  fWantBreak = (dwParseMode & 0x01) ? FALSE : TRUE;
                  break;
               case L'[':     // word breaks on these characters
               case L']':
               case L'(':
               case L')':
               case L'|':
                  fWantBreak = (dwParseMode & 0x02) ? FALSE : TRUE;
                  break;
               } // switch
               if (fWantBreak)
                  break;

               // advacne
               dwCount--;
               psz++;
            } // while (dwCount)

            // add the string to the hash table if it isn't alread
            DWORD dwLen = ((DWORD)((PBYTE)psz - (PBYTE)pszStart)) / sizeof(WCHAR);
            if (!mem.Required ((dwLen+1)*sizeof(WCHAR)))
               return -1;  // error
            memcpy (mem.p, pszStart, dwLen*sizeof(WCHAR));
            ((PWSTR)mem.p)[dwLen] = 0;
            _wcslwr ((PWSTR)mem.p);

            // add the string if it's not there
            DWORD dwToken = m_phTokens->FindIndex ((PWSTR)mem.p, FALSE);
            if (dwToken == -1) {
               m_phTokens->Add ((PWSTR)mem.p, NULL, FALSE);
               dwToken = m_phTokens->FindIndex ((PWSTR)mem.p, FALSE);
            }

            // add to list
            if (dwCurSeqStart == (DWORD)-1) {
               // entirely new sequence
               node.dwAlt = -1;  // no alternative as of yet
               node.dwNext = dwEndIndex;  // always point to end index
               node.dwToken = dwToken; // use this token
               dwCurSeqStart = dwCurSeqEnd = m_lCFGNODE.Add (&node);

               // alternative of previous?
               if (dwLastSeqStart != -1) {
                  PCFGNODE pn = (PCFGNODE)m_lCFGNODE.Get(dwLastSeqStart);
                  pn->dwAlt = dwCurSeqStart;
               }

               // update more
               dwLastSeqStart = dwCurSeqStart;
               if (dwIndexAdded == -1)
                  dwIndexAdded = dwCurSeqStart;
            }
            else {
               // appending onto existing sequence
               node.dwAlt = -1;  // no alternative as of yet
               node.dwNext = dwEndIndex;  // always point to end index
               node.dwToken = dwToken; // use this token
               DWORD dwNewNode = m_lCFGNODE.Add (&node);

               // make the last end point to the new start
               PCFGNODE pn = (PCFGNODE)m_lCFGNODE.Get(dwCurSeqEnd);
               pn->dwNext = dwNewNode;

               // update the current end
               dwCurSeqEnd = dwNewNode;
            }
         }
         break;

      case L'|':     // alternative
         if (dwParseMode & 0x02)
            goto dodefault;  // not allowing alternatives

         // BUGFIX - If followed by something that looks like a guid then do default
         for (i = 1; i <= sizeof(GUID)*2; i++)
            if (!( ((psz[i] >= L'0') && (psz[i] <= L'9')) ||
               ((psz[i] >= L'A') && (psz[i] <= L'F')) ||
               ((psz[i] >= L'a') && (psz[i] <= L'f')) ))
                  break;
         if (i > sizeof(GUID)*2)
            goto dodefault;


         // NOTE: If have | at the beginning of a statement will end up being ignored

         // updat curseq start and end so know is alternative
         dwCurSeqStart = dwCurSeqEnd = -1;

         // eat it up
         psz++;
         dwCount--;
         break;

      case L'%':     // wildcard, 1 word
      case L'*':     // wildcard, 1+ words
         {
            if (dwParseMode & 0x01)
               goto dodefault;  // not allowing wildcards

            // BUGFIX - If the next character isn't a digit then
            // don't error out, but add the symbol by itself
            if ((dwCount < 2) || (psz[1] < L'0') || (psz[1] > L'9'))
               goto dodefault;   // allow by itself
               //return -1;  // shouldnt get one at the end, or without a digit following
            DWORD dwToken = ((psz[0] == L'%') ? CFGNODETOK_ONEWORD : CFGNODETOK_MANYWORDS) +
               (DWORD)(psz[1] - L'0'); // to store the bin
            dwCount -= 2;
            psz += 2;

            // the following code is duplicated from adding new word
            // add to list
            if (dwCurSeqStart == (DWORD)-1) {
               // entirely new sequence
               node.dwAlt = -1;  // no alternative as of yet
               node.dwNext = dwEndIndex;  // always point to end index
               node.dwToken = dwToken; // use this token
               dwCurSeqStart = dwCurSeqEnd = m_lCFGNODE.Add (&node);

               // alternative of previous?
               if (dwLastSeqStart != -1) {
                  PCFGNODE pn = (PCFGNODE)m_lCFGNODE.Get(dwLastSeqStart);
                  pn->dwAlt = dwCurSeqStart;
               }

               // update more
               dwLastSeqStart = dwCurSeqStart;
               if (dwIndexAdded == -1)
                  dwIndexAdded = dwCurSeqStart;
            }
            else {
               // appending onto existing sequence
               node.dwAlt = -1;  // no alternative as of yet
               node.dwNext = dwEndIndex;  // always point to end index
               node.dwToken = dwToken; // use this token
               DWORD dwNewNode = m_lCFGNODE.Add (&node);

               // make the last end point to the new start
               PCFGNODE pn = (PCFGNODE)m_lCFGNODE.Get(dwCurSeqEnd);
               pn->dwNext = dwNewNode;

               // update the current end
               dwCurSeqEnd = dwNewNode;
            }
         }
         break;

      case L'[':     // optional
      case L'(':     // paren so can include sub-elements
         {
            if (dwParseMode & 0x02)
               goto dodefault;  // not allowing alternatives

            // loop until no longer have bracket
            int iBracket = 0, iParen = 0;
            BOOL fBracket = (psz[0] == L'[');
            if (fBracket)
               iBracket++;
            else
               iParen++;

            psz++;
            dwCount--;
            PWSTR pszStart = psz;

            // repeat
            while (dwCount && (iBracket || iParen)) {
               switch (psz[0]) {
               case L'[':
                  iBracket++;
                  break;
               case L']':
                  iBracket--;
                  break;
               case L'(':
                  iParen++;
                  break;
               case L')':
                  iParen--;
                  break;
               }
               psz++;
               dwCount--;
            } // while dwCount && iBracket || iParen

            PWSTR pszEnd = psz;

            // skip whitespace
            while (dwCount && iswspace(psz[0])) {
               psz++;
               dwCount--;
            }

            // make a NULL node for this to terminate?
            DWORD dwTerminate = dwEndIndex;
            if (dwCount) { // still stuff left, so need terminator with NOP
               node.dwAlt = -1;  // no alternative as of yet
               node.dwNext = dwEndIndex;  // always point to end index
               node.dwToken = CFGNODETOK_NOP; // NO-OP
               dwTerminate = m_lCFGNODE.Add (&node);
            }

            // parse what's within
            int iNewCount = ((int)((PBYTE)pszEnd - (PBYTE)pszStart)) / sizeof(WCHAR);
            if (!(iBracket || iParen))
               iNewCount--;   // discard last char
            DWORD dwNextInSeq = (DWORD)-1;
            if (iNewCount)
               dwNextInSeq = ExtractCFGFromSubString (pszStart, (DWORD)iNewCount, dwTerminate, dwParseMode);
            if (dwNextInSeq == -1)
               return -1;  // error, nothing inside

            // if this has an alternative for the first element then insert a NO-OP before this
            // do this as protection against multiply stacked alts
            PCFGNODE pn = (PCFGNODE)m_lCFGNODE.Get(dwNextInSeq);
            if (pn->dwAlt != -1) {
               node.dwAlt = -1;  // no alternative as of yet
               node.dwNext = dwNextInSeq;  // always point to end index
               node.dwToken = CFGNODETOK_NOP; // NO-OP
               dwNextInSeq = m_lCFGNODE.Add (&node);
            }

            // if it's a bracket then add an alternate that skips right to the end
            if (fBracket) {
               // I know than pn->dwAlt = -1 because previous statement ensures this
               pn = (PCFGNODE)m_lCFGNODE.Get(dwNextInSeq);
               pn->dwAlt = dwTerminate;
            }

            // put into datastructure
            // the following code is duplicated from adding new word
            // add to list, with some mods
            if (dwCurSeqStart == (DWORD)-1) {
               // entirely new sequence
               node.dwAlt = -1;  // no alternative as of yet
               node.dwNext = dwNextInSeq;  // always point to end index
               node.dwToken = CFGNODETOK_NOP; // use this token
               dwCurSeqStart = dwNextInSeq;
               dwCurSeqEnd = dwTerminate;

               // alternative of previous?
               if (dwLastSeqStart != -1) {
                  PCFGNODE pn = (PCFGNODE)m_lCFGNODE.Get(dwLastSeqStart);
                  pn->dwAlt = dwCurSeqStart;
               }

               // update more
               dwLastSeqStart = dwCurSeqStart;
               if (dwIndexAdded == -1)
                  dwIndexAdded = dwCurSeqStart;
            }
            else {
               // appending onto existing sequence
               // make the last end point to the new start
               PCFGNODE pn = (PCFGNODE)m_lCFGNODE.Get(dwCurSeqEnd);
               pn->dwNext = dwNextInSeq;

               // update the current end
               dwCurSeqEnd = dwTerminate;
            }
         } // [] or ()
         break;

      }
   }  // while (dwCount) - until no more count

   // return what added
   return dwIndexAdded;
}



/*************************************************************************************
CCircumrealityCFG::CalcStartTokens - Given a filled m_lCFGNODE list, this calculate what the
possible starting tokens are (ignoring wildcards).

This assumes m_lStartToken was cleared.

inputs
   DWORD          dwIndex - Index to look into first
*/
void CCircumrealityCFG::CalcStartTokens (DWORD dwIndex)
{
   PCFGNODE pn = (PCFGNODE)m_lCFGNODE.Get(dwIndex);
   if (!pn)
      return;  // don't bother adding

   // do recursion and try alternative
   if (pn->dwAlt != -1)
      CalcStartTokens (pn->dwAlt);

   // is this a valid starting token?
   if (pn->dwToken < CFGNODETOK_MANYWORDS) {
      DWORD i;
      DWORD *padw = (DWORD*)m_lStartToken.Get(0);
      for (i = 0; i < m_lStartToken.Num(); i++, padw++)
         if (padw[0] == pn->dwToken)
            return;   // already there
      
      // else, add
      m_lStartToken.Add (&pn->dwToken);
      return;
   }
   
   // else, if get here, not a valid token, so try further down the chain
   CalcStartTokens (pn->dwNext);
}



/*************************************************************************************
CCircumrealityCFG::ExtractCFG - This takes a string with CFG notation, such as
"[the] [bright] lantern of %1" and produces the CFG information from it.

inputs
   PWSTR          psz - String to parse, starting at the beginning
   DWORD          dwParseMode - If 0 allow everything: [], (), |, %n, and *n.
                              If 0x01 disallow wildcards %n and *n, allow the rest.
                              if 0x02 disallow alternatives, [], (), and |, allow the rest.
                              if 0x03 disallow everything
returns
   BOOL - TRUE if success
*/
BOOL CCircumrealityCFG::ExtractCFG (PWSTR psz, DWORD dwParseMode)
{
   // clear to a blank terminating onde
   CFGNODE node;
   node.dwAlt = node.dwNext = -1;
   node.dwToken = CFGNODETOK_NOP;
   m_lCFGNODE.Init (sizeof(node), &node, 1);

   // parse it
   m_dwNodeStart = ExtractCFGFromSubString (psz, (DWORD)wcslen(psz), 0, dwParseMode);
   if (m_dwNodeStart == -1) {
      if (psz[0] || !(dwParseMode & 0x02))
         return FALSE;  // error
      else
         m_dwNodeStart = 0;   // since was a blank line and in the "to" category
   }

   // pull out starting words
   m_lStartToken.Clear();
   CalcStartTokens (m_dwNodeStart);

   return TRUE;
}



/*************************************************************************************
CCircumrealityCFG::ParseInternal - This parses the given series of tokens (using strings from m_phTokens),
and adds all successful parses to the list.

inputs
   DWORD          *padwTokens - Pointer to an array of tokens from m_phTokens (and maybe others)
   DWORD          dwNum - Number of tokens in the list
   BOOL           fMustUseAll - If TRUE then must use all the the tokens to have a complete
                     parse
   CCircumrealityCFG        **ppCFGTemplate - When the CFG is parsed, the elements in pBin will
                     be filled in based on wildcards, %n and *n. When the parse is finished,
                     the CFG from the template, pCFGTemplate[n], will be used to generate
                     the new list of tokens, using info from pBin to fill in the blanks.
                     NOTE: ppCFGTemplate[n] must have been compiled DISABLING branching, such
                     as [], (), and |.
   DWORD          dwNumTemplate - Number of templated pointed to by ppCFGTemplate. Normally there's
                     only 1, but if there's more then all the template results will be appended
                     together, using a -1 token to divide them (good for parse resulting in multiple sentences)
   PCListVariable plParsed - Filled with the successful parses of the CFG, each parse to
                     an item. The parses are DWORD tokens. NOTE: The 1st DWORD in the list
                     is the number of tokens used from padwTokens. This way a synonym will
                     know how many words were substituted. This should have been cleard
                     before the first call.
   PCMem          pMem - Scratch memory where can
   PCFGPARSETEMP  pBin - Initialized to -1 for all, used to keep track of which token the
                     bins start and end. Scratch
   DWORD          dwIndex - Current parse index, started at m_dwNodeStart
   DWORD          dwCurToken - Current token that on, in padwToken list
returns
   none
*/
void CCircumrealityCFG::ParseInternal (DWORD *padwTokens, DWORD dwNum, BOOL fMustUseAll, CCircumrealityCFG **ppCFGTemplate, DWORD dwNumTemplate,
                             PCListVariable plParsed, PCMem pMem, PCFGPARSETEMP pBin, DWORD dwIndex, DWORD dwCurToken)
{
   PCFGNODE pn = (PCFGNODE)m_lCFGNODE.Get (dwIndex);
   if (!pn) {
      // if got here made it to the end of a parse

      // if have to use all the tokens but didn't then just exit, since incomplete
      if (fMustUseAll && (dwCurToken < dwNum))
         return;

      // else, successful parse
      if (dwNumTemplate == 1) {
         ppCFGTemplate[0]->UseAsTemplate (pMem, pBin, padwTokens, dwNum);
         if (pMem->m_dwCurPosn >= sizeof(DWORD)) {
            // indicate the number of elmeents used
            ((DWORD*)pMem->p)[0] = dwCurToken;
            plParsed->Add (pMem->p, pMem->m_dwCurPosn);
         }
      }
      else {
         // multiple templates
         CMem memTemp;
         DWORD i;
         pMem->m_dwCurPosn = 0;  // to clear
         for (i = 0; i < dwNumTemplate; i++) {
            memTemp.m_dwCurPosn = 0;
            ppCFGTemplate[i]->UseAsTemplate (&memTemp, pBin, padwTokens, dwNum);
            if (memTemp.m_dwCurPosn < sizeof(DWORD))
               continue;

            // append
            if (!pMem->Required (pMem->m_dwCurPosn + memTemp.m_dwCurPosn))
               return;  // error
            memcpy ((PBYTE)pMem->p + pMem->m_dwCurPosn, memTemp.p, memTemp.m_dwCurPosn);
            ((DWORD*)pMem->p)[pMem->m_dwCurPosn / sizeof(DWORD)] = (DWORD)-1; // so sentence splitter
            pMem->m_dwCurPosn += memTemp.m_dwCurPosn; // BUGFIX - forgot this
         } // i

         // indicate the number of elmeents used
         ((DWORD*)pMem->p)[0] = dwCurToken;
         plParsed->Add (pMem->p, pMem->m_dwCurPosn);
      } // if multiple sentences
      return;  // done
   }

   // if there's an alternative then go down that direction
   if (pn->dwAlt != -1) {
      ParseInternal (padwTokens, dwNum, fMustUseAll, ppCFGTemplate, dwNumTemplate,plParsed, pMem,
         pBin, pn->dwAlt, dwCurToken);
   }

   // see if this node says about the parse
   if (pn->dwToken == CFGNODETOK_NOP) {
      // no nothing
   }
   else if (pn->dwToken >= CFGNODETOK_ONEWORD) {
      // pull out one word

      // if dont have a word then no match
      if (dwCurToken >= dwNum)
         return;

      // BUGFIX - store away bin now, and then restore
      DWORD dwBin = pn->dwToken - CFGNODETOK_ONEWORD;
      WORD awStart = pBin->awBinStart[dwBin];
      WORD awEnd = pBin->awBinEnd[dwBin];

      // remember where it starts/stops
      pBin->awBinStart[dwBin] = pBin->awBinEnd[dwBin] = (WORD)dwCurToken;
     
      // eat it up and go on
      dwCurToken++;

      // if get here, go onto the next node
      ParseInternal (padwTokens, dwNum, fMustUseAll, ppCFGTemplate, dwNumTemplate, plParsed, pMem, pBin, pn->dwNext,
         dwCurToken);

      // restore
      pBin->awBinStart[dwBin] = awStart;
      pBin->awBinEnd[dwBin] = awEnd;

      return;

   }
   else if (pn->dwToken >= CFGNODETOK_MANYWORDS) {
      // BUGFIX - store away bin now, and then restore
      DWORD dwBin = pn->dwToken - CFGNODETOK_MANYWORDS;
      WORD awStart = pBin->awBinStart[dwBin];
      WORD awEnd = pBin->awBinEnd[dwBin];

      // pull out one or more words
      DWORD dwLastToken;
      pBin->awBinStart[dwBin] = (WORD)dwCurToken;

      // NOTE - intentionally use <= here
      for (dwLastToken = dwCurToken+1; dwLastToken <= dwNum; dwLastToken++) {
         pBin->awBinEnd[dwBin] = (WORD)dwLastToken-1;

         ParseInternal (padwTokens, dwNum, fMustUseAll, ppCFGTemplate, dwNumTemplate,plParsed, pMem,
            pBin, pn->dwNext, dwLastToken);
      } // dwLastToken

      // restore
      pBin->awBinStart[dwBin] = awStart;
      pBin->awBinEnd[dwBin] = awEnd;

      return;  // dont use default handler
   }
   else {
      // else, must be exact match
      // BUGFIX - Was padwTokens[0], but should be padwTokens[dwCurToken]
      if ((dwCurToken >= dwNum) || (padwTokens[dwCurToken] != pn->dwToken))
         return;  // no match

      // eat it up
      dwCurToken++;
   }

   // if get here, go onto the next node
   ParseInternal (padwTokens, dwNum, fMustUseAll, ppCFGTemplate, dwNumTemplate, plParsed, pMem, pBin, pn->dwNext,
      dwCurToken);
}



/*************************************************************************************
CCircumrealityCFG::UseAsTemplate - Uses the CFG's m_lCFGNODE as a template for converting
from one CFG format to another. This fills in memory (pMem) with an array of DWORDs.
The first DWORD is 0, since it will be filled in by the caller. The next DWORDs are filled
in with token numbers, either from within m_lCFGNODE, or from pBin that's passed in.

NOTE: To call this the CFG must have been Extracted using the ignore-branching option.

Thus, if the CFG is "He said *1 to *2", and pBin indicates *1 is "hi there" and *2 is "mary",
the output memory will be 0, followed by the tokens for "he said hi there to mary".

inputs
   PCMem          pMem - Will be filled in with DWORD tokens. The m_dwCurPosn will
                     be set to the last token
   PCFGPARSETEMP  pBin - The meanings of wildcards, filled in by ParseInternal.
   DWORD          *padwTokens - Pointer to an array of tokens from m_phTokens (and maybe others),
                     related to pBin
   DWORD          dwNum - Number of tokens in the list, related to pBin
returns
   none
*/
void CCircumrealityCFG::UseAsTemplate (PCMem pMem, PCFGPARSETEMP pBin, DWORD *padwTokens, DWORD dwNum)
{
   // fill 0 in header
   pMem->m_dwCurPosn = 0;
   if (!pMem->Required (pMem->m_dwCurPosn + sizeof(DWORD)))
      return;  // error
   ((DWORD*)pMem->p)[pMem->m_dwCurPosn/sizeof(DWORD)] = 0;  // for now
   pMem->m_dwCurPosn += sizeof(DWORD);

   // other bits
   DWORD dwIndex;
   PCFGNODE pn = (PCFGNODE) m_lCFGNODE.Get(0);
   for (dwIndex = m_dwNodeStart; dwIndex < m_lCFGNODE.Num(); dwIndex = pn[dwIndex].dwNext) {
      DWORD dwBin;

      if (pn[dwIndex].dwToken == CFGNODETOK_NOP)
         continue;   // nothing to do
      else if (pn[dwIndex].dwToken >= CFGNODETOK_ONEWORD)
         dwBin = pn[dwIndex].dwToken - CFGNODETOK_ONEWORD;
      else if (pn[dwIndex].dwToken >= CFGNODETOK_MANYWORDS)
         dwBin = pn[dwIndex].dwToken - CFGNODETOK_MANYWORDS;
      else {
         // else, one word from template gets transferredover
         if (!pMem->Required (pMem->m_dwCurPosn + sizeof(DWORD)))
            return;  // error
         ((DWORD*)pMem->p)[pMem->m_dwCurPosn/sizeof(DWORD)] = pn[dwIndex].dwToken;
         pMem->m_dwCurPosn += sizeof(DWORD);
         continue;
      }

      // if bin is empty do nothing
      if ((pBin->awBinEnd[dwBin] >= (WORD)dwNum) || (pBin->awBinStart[dwBin] >= (WORD)dwNum))
         continue;

      // if strange length then do nthing
      if (pBin->awBinEnd[dwBin] < pBin->awBinStart[dwBin])
         continue;

      // how much
      DWORD dwSize = (DWORD)(pBin->awBinEnd[dwBin] + 1 - pBin->awBinStart[dwBin]) * sizeof(DWORD);
         // NOT - Just added +1 because of way storing binend. Think it's right

      // copy over
      if (!pMem->Required (pMem->m_dwCurPosn + dwSize))
         return;  // error
      memcpy ((PBYTE)pMem->p + pMem->m_dwCurPosn, padwTokens + pBin->awBinStart[dwBin], dwSize);
      pMem->m_dwCurPosn += dwSize;
   } // dwIndex

   // done
}

/*************************************************************************************
CCircumrealityCFG::Parse - This parses the given series of tokens (using strings from m_phTokens),
and adds all successful parses to the list.

inputs
   DWORD          *padwTokens - Pointer to an array of tokens from m_phTokens (and maybe others)
   DWORD          dwNum - Number of tokens in the list
   BOOL           fMustUseAll - If TRUE then must use all the the tokens to have a complete
                     parse
   CCircumrealityCFG        *pCFGTemplate - When the CFG is parsed, the elements in pBin will
                     be filled in based on wildcards, %n and *n. When the parse is finished,
                     the CFG from the template, pCFGTemplate, will be used to generate
                     the new list of tokens, using info from pBin to fill in the blanks.
                     NOTE: pCFGTemplate must have been compiled DISABLING branching, such
                     as [], (), and |.
   DWORD          dwNumTemplate - Number of templated pointed to by ppCFGTemplate. Normally there's
                     only 1, but if there's more then all the template results will be appended
                     together, using a -1 token to divide them (good for parse resulting in multiple sentences)
   PCListVariable plParsed - Filled with the successful parses of the CFG, each parse to
                     an item. The parses are DWORD tokens. NOTE: The 1st DWORD in the list
                     is the number of tokens used from padwTokens. This way a synonym will
                     know how many words were substituted. This WILL BE cleared at the
                     start of the call
returns
   none
*/
void CCircumrealityCFG::Parse (DWORD *padwTokens, DWORD dwNum, BOOL fMustUseAll, CCircumrealityCFG **ppCFGTemplate, DWORD dwNumTemplate,
                             PCListVariable plParsed)
{
   CMem mem;
   CFGPARSETEMP pt;
   DWORD i;
   for (i = 0; i < 10; i++)
      pt.awBinStart[i] = pt.awBinEnd[i] = -1;

   // clear it
   plParsed->Clear();

   // parse
   ParseInternal (padwTokens, dwNum, fMustUseAll, ppCFGTemplate, dwNumTemplate, plParsed, &mem, &pt,
      m_dwNodeStart, 0);
}



/*************************************************************************************
CCircumrealityCFG::IsStartingToken - Returns TRUE if the given token is one of the potential
tokens that starts the CFG.

inputs
   DWORD          dwToken - From m_phTokens
returns
   BOOL - TRUE if token could start the CFG, FALSE if not
*/
BOOL CCircumrealityCFG::IsStartingToken (DWORD dwToken)
{
   DWORD *padwToken = (DWORD*) m_lStartToken.Get(0);
   DWORD i;
   for (i = 0; i < m_lStartToken.Num(); i++, padwToken++)
      if (padwToken[0] == dwToken)
         return TRUE;

   return FALSE;
}



/*************************************************************************************
CCircumrealityCFG::HasStartingToken - Returns TRUE if the phrase to be parsed has a token
that starts this.

inputs
   DWORD          *padwTokens - Pointer to an array of tokens from m_phTokens (and maybe others)
   DWORD          dwNum - Number of tokens in the list
returns
   BOOL - TRUE if has tokens
*/
BOOL CCircumrealityCFG::HasStartingToken (DWORD *padwTokens, DWORD dwNum)
{
   DWORD *padwToken = (DWORD*) m_lStartToken.Get(0);
   DWORD i, j;
   for (i = 0; i < m_lStartToken.Num(); i++, padwToken++) {
      DWORD *padwCurToken = padwTokens;
      for (j = 0; j < dwNum; j++, padwCurToken++)
         if (padwCurToken[0] == padwToken[0])
            return TRUE;
   } // i

   return FALSE;
}


/*************************************************************************************
CCircumrealityCFG::CloneTo - Standard API
*/
BOOL CCircumrealityCFG::CloneTo (CCircumrealityCFG *pTo)
{
   pTo->m_lCFGNODE.Init (sizeof(CFGNODE), m_lCFGNODE.Get(0), m_lCFGNODE.Num());
   pTo->m_lStartToken.Init (sizeof(DWORD), m_lStartToken.Get(0), m_lStartToken.Num());
   pTo->m_dwNodeStart = m_dwNodeStart;
   pTo->m_phTokens = m_phTokens;

   return TRUE;
}

/*************************************************************************************
CCircumrealityCFG::Clone - Standard API
*/
CCircumrealityCFG *CCircumrealityCFG::Clone (void)
{
   PCCircumrealityCFG pNew = new CCircumrealityCFG;
   if (!pNew)
      return NULL;
   if (!CloneTo (pNew)) {
      delete pNew;
      return NULL;
   }
   return pNew;
}



/*************************************************************************************
CCircumrealityNLPRule::Constructor and destructor
*/
CCircumrealityNLPRule::CCircumrealityNLPRule (void)
{
   m_dwType = 0;
   m_fProb = 0.5;
   m_phTokens = NULL;
   m_lPCCircumrealityCFG.Init (sizeof(PCCircumrealityCFG));
}

CCircumrealityNLPRule::~CCircumrealityNLPRule (void)
{
   DWORD i;
   PCCircumrealityCFG *ppc = (PCCircumrealityCFG*)m_lPCCircumrealityCFG.Get(0);
   for (i = 0; i < m_lPCCircumrealityCFG.Num(); i++)
      delete ppc[i];
   m_lPCCircumrealityCFG.Clear();
}

/*************************************************************************************
CCircumrealityCFG::Init - Initialize the object to use the right tokens.

inputs
   PCHashString      phTokens - These are the tokens that will be used by the
                     CFG when if extracts the sub-string.
returns
   BOOL - TRUE if succes
*/
BOOL CCircumrealityNLPRule::Init (PCHashString phTokens)
{
   if (m_phTokens)
      return FALSE;
   m_phTokens = phTokens;

   return TRUE;
}


static PWSTR gpszFrom = L"From";
static PWSTR gpszProb = L"Prob";
static PWSTR gpszReword = L"Reword";
static PWSTR gpszSplit = L"Split";
static PWSTR gpszSyn = L"Syn";
static PWSTR gpszTo = L"To";

/*************************************************************************************
CCircumrealityCFG::MMLTo - Creates a sub-node in pNode and fills in the information.
The sub-node will be of type "Syn" for synonym, "Reword" for 1-to-1 rewording,
or "Split" to split into multiple sentences.

inputs
   PCMMLNode2         pNode - Node to add to
returns
   BOOL - TRUE if success
*/
BOOL CCircumrealityNLPRule::MMLTo (PCMMLNode2 pNode)
{
   if (m_dwType >= 3)
      return FALSE;  // dont know this type
   PCMMLNode2 pSub = pNode->ContentAddNewNode ();
   if (!pSub)
      return FALSE;

   PWSTR psz;
   DWORD i;
   WCHAR szTemp[32];

   switch (m_dwType) {
   case 0:  // synonym
   case 1:  // reword
      pSub->NameSet (m_dwType ? gpszReword : gpszSyn);

      // to attribute
      psz = (PWSTR) m_lCFGString.Get(1);
      if (psz && psz[0])
         pSub->AttribSetString (gpszTo, psz);
      break;


   case 2:  // split sentence
      pSub->NameSet (gpszSplit);

      // several to attributes
      for (i = 1; i < m_lCFGString.Num(); i++) {
         psz = (PWSTR) m_lCFGString.Get(i);
         if (!psz || !psz[0])
            continue;

         swprintf (szTemp, L"To%d", (int)i);
         pSub->AttribSetString (szTemp, psz);
      }
      break;
   } // switch

   // set from
   psz = (PWSTR) m_lCFGString.Get(0);
   if (psz && psz[0])
      pSub->AttribSetString (gpszFrom, psz);
   
   // probability
   if (m_fProb != 0.5) {
      swprintf (szTemp, L"%g", (double)m_fProb * 100.0);
      pSub->AttribSetString (gpszProb, szTemp);
   }

   // done
   return TRUE;
}


/*************************************************************************************
CCircumrealityCFG::MMLFrom - This reads the rule from the current node. If the node's name
does NOT match a proper rule it returns FALSE.

inputs
   PCMMLNode2            pNode - Should have name of "Syn", "Reword", or "Split"
returns
   BOOL - TRUE if read in, FALSE if error (such as bad name)
*/
BOOL CCircumrealityNLPRule::MMLFrom (PCMMLNode2 pNode)
{
   PWSTR psz = pNode->NameGet();
   if (!psz)
      return FALSE;
   if (!_wcsicmp (psz, gpszSyn))
      m_dwType = 0;
   else if (!_wcsicmp (psz, gpszReword))
      m_dwType = 1;
   else if (!_wcsicmp (psz, gpszSplit))
      m_dwType = 2;
   else
      return FALSE;

   // try to read in the probability
   psz = pNode->AttribGetString (gpszProb);
   m_fProb = 0.5; // default
   if (psz) {
      char sza[64];
      DWORD dwNeeded;
      dwNeeded = (DWORD)wcslen(psz);
      if (dwNeeded < sizeof(sza)/2) {
         WideCharToMultiByte (CP_ACP,0,psz,-1,sza,sizeof(sza),0,0);
         m_fProb = atof (sza) / 100.0; // BUGFIX - Divide by 100.0
         m_fProb = max(m_fProb, 0.00001); // small prob
         m_fProb = min(m_fProb, 0.99); // never too high
      }
   } // if prob string

   // get from
   m_lCFGString.Clear();
   psz = pNode->AttribGetString (gpszFrom);
   if (!psz)
      psz = L"";
   m_lCFGString.Add (psz, (wcslen(psz)+1)*sizeof(WCHAR));

   // get to
   DWORD i;
   WCHAR szTemp[32];
   switch (m_dwType) {
   case 0:  // syn
   case 1:  // reword
      psz = pNode->AttribGetString (gpszTo);
      if (!psz)
         psz = L"";
      m_lCFGString.Add (psz, (wcslen(psz)+1)*sizeof(WCHAR));
      break;

   case 2:  // split
      for (i = 1; ; i++) {
         swprintf (szTemp, L"To%d", (int)i);
         psz = pNode->AttribGetString (szTemp);
         if (!psz)
            break;
         m_lCFGString.Add (psz, (wcslen(psz)+1)*sizeof(WCHAR));
      } // i
      break;
   } // switch

   // done
   CalcCFG (); // ignore errors from this
   return TRUE;
}

/*************************************************************************************
CCircumrealityNLPRule::CloneTo - Standard API
*/
BOOL CCircumrealityNLPRule::CloneTo (CCircumrealityNLPRule *pTo)
{
   pTo->m_dwType = m_dwType;
   pTo->m_fProb = m_fProb;
   pTo->m_phTokens = m_phTokens;

   // stinrgs
   DWORD i;
   pTo->m_lCFGString.Clear();
   for (i = 0; i < m_lCFGString.Num(); i++)
      pTo->m_lCFGString.Add (m_lCFGString.Get(i), m_lCFGString.Size(i));

   // CFGs
   PCCircumrealityCFG *ppc = (PCCircumrealityCFG*)pTo->m_lPCCircumrealityCFG.Get(0);
   for (i = 0; i < pTo->m_lPCCircumrealityCFG.Num(); i++)
      delete ppc[i];
   pTo->m_lPCCircumrealityCFG.Init (sizeof(PCCircumrealityCFG), m_lPCCircumrealityCFG.Get(0), m_lPCCircumrealityCFG.Num());
   ppc = (PCCircumrealityCFG*)pTo->m_lPCCircumrealityCFG.Get(0);
   for (i = 0; i < pTo->m_lPCCircumrealityCFG.Num(); i++)
      ppc[i] = ppc[i]->Clone();

   return TRUE;
}

/*************************************************************************************
CCircumrealityNLPRule::Clone - Standard API
*/
CCircumrealityNLPRule *CCircumrealityNLPRule::Clone (void)
{
   PCCircumrealityNLPRule pNew = new CCircumrealityNLPRule;
   if (!pNew)
      return NULL;
   if (!CloneTo (pNew)) {
      delete pNew;
      return NULL;
   }
   return pNew;
}



/*************************************************************************************
CCircumrealityNLPRule::CalcCFG - CALL THIS whenever m_lCFGString or m_dwType are changed.
This recalculates the CFGs necessary for parsing.

returns
   BOOL - TRUE if succcess
*/
BOOL CCircumrealityNLPRule::CalcCFG (void)
{
   // free existing CFGs
   DWORD i;
   PCCircumrealityCFG *ppc = (PCCircumrealityCFG*)m_lPCCircumrealityCFG.Get(0);
   for (i = 0; i < m_lPCCircumrealityCFG.Num(); i++)
      delete ppc[i];
   m_lPCCircumrealityCFG.Clear();

   // re-create
   for (i = 0; i < m_lCFGString.Num(); i++) {
      // create and init
      PCCircumrealityCFG pc = new CCircumrealityCFG;
      if (!pc)
         return FALSE;
      if (!pc->Init (m_phTokens)) {
         delete pc;
         return FALSE;
      }

      // try to compile cfg
      if (!pc->ExtractCFG ((PWSTR)m_lCFGString.Get(i), i ? 0x02 : 0)) {
         delete pc;
         return FALSE;
      }

      // add it
      m_lPCCircumrealityCFG.Add (&pc);
   } // i

   if (m_lCFGString.Num() < 2)
      return FALSE;

   // done
   return TRUE;
}



/*************************************************************************************
CCircumrealityNLPRule::ParseSentence - Parses a series of tokens looking for matches. If they're
found then the matches are added to the hypothesis list.

inputs
   PCCircumrealityNLPHyp    pHypBase - Hypothesis to base this off of
   DWORD          *padwTokens - List of tokens to parse. These must NOT contain -1 to indicate
                        sentence breaks
   DWORD          dwNum - Number of tokens
   DWORD          *padwTokPre - Insert these tokens ahead (so can parse sentence)
   DWORD          dwNumPre - Number in padwTokPre
   DWORD          *padwTokPost - Add these tokens at the end (so can parse sentence)
   DWORD          dwNumPost - Number of padwTokPost
   DWORD          dwRuleGroup - Rule-group value for new hypothesis
   DWORD          dwRuleELem - Rule-element value for new hypothesis
   PCListFixed    plPCCircumrealityNLPHyp - List of hypothesis to add new hypothesis to
   PCMem          pMem - Scratch memory
returns
   BOOL - TRUE if added and hypthesis
*/
BOOL CCircumrealityNLPRule::ParseSentence (PCCircumrealityNLPHyp pHypBase, DWORD *padwTokens, DWORD dwNum,
                                 DWORD *padwTokPre, DWORD dwNumPre, DWORD *padwTokPost, DWORD dwNumPost,
                                 DWORD dwRuleGroup, DWORD dwRuleElem,
                                 PCListFixed plPCCircumrealityNLPHyp, PCMem pMem)
{
   BOOL fRet = FALSE;

   // do a quick test to see if anything matches
   PCCircumrealityCFG *pcc = (PCCircumrealityCFG*)m_lPCCircumrealityCFG.Get(0);
   DWORD dwNumCFG = m_lPCCircumrealityCFG.Num();
   if (dwNumCFG < 2)
      return fRet;   // cant parse this because nothing for sub
   PCCircumrealityCFG pFrom = pcc[0];
   if (!pFrom->HasStartingToken (padwTokens, dwNum))
      return FALSE;  // not going to match at all

   // different types of parse
   DWORD i, j;
   CListVariable lParsed;
   if (m_dwType == 0) { // synonym
      // loop through all the words and see if synonym starts
      for (i = 0; i < dwNum; i++) {
         if (!pFrom->IsStartingToken (padwTokens[i]))
            continue;   // not a starting token so skip

         // else, starting token, so see if can parse
         pFrom->Parse (padwTokens + i, dwNum - i, FALSE, pcc+1, 1, &lParsed);
         
         // loop through all the parses and do the substitutions
         for (j = 0; j < lParsed.Num(); j++) {
            // find the info
            DWORD dwLen = (DWORD)lParsed.Size(j) / sizeof(DWORD) - 1;
            DWORD *padw = (DWORD*)lParsed.Get(j);
            DWORD dwReplaced = padw[0];

            // how many elems are needed?
            DWORD dwNeed = dwNumPre + i + dwLen + (dwNum - i - dwReplaced) + dwNumPost;

            // create a new hypothesis
            PCCircumrealityNLPHyp pHyp = pHypBase->Clone();
            if (!pHyp)
               continue;

            // BUGFIX - Keep history of hypothesis so can be more accurate with probability
            pHyp->NewID();
            pHyp->m_lParents.Add (&pHypBase->m_dwID);

            pHyp->m_dwRuleElem = dwRuleElem;
            pHyp->m_dwRuleGroup = dwRuleGroup;
            pHyp->m_fProb = pHypBase->m_fProb * m_fProb;
            pHyp->m_fStable = FALSE;
            if (!pHyp->m_memTokens.Required (dwNeed * sizeof(DWORD))) {
               delete pHyp;
               continue;
            }

            // copy over the memory
            DWORD dwCur = 0;
            if (dwNumPre) {
               memcpy ((DWORD*)pHyp->m_memTokens.p + dwCur, padwTokPre, dwNumPre * sizeof(DWORD));
               dwCur += dwNumPre;
            }
            memcpy ((DWORD*)pHyp->m_memTokens.p + dwCur, padwTokens, i * sizeof(DWORD));
            dwCur += i;
            memcpy ((DWORD*)pHyp->m_memTokens.p + dwCur, padw+1, dwLen * sizeof(DWORD));
            dwCur += dwLen;
            memcpy ((DWORD*)pHyp->m_memTokens.p + dwCur, padwTokens + (i + dwReplaced), (dwNum - i - dwReplaced) * sizeof(DWORD));
            dwCur += (dwNum - i - dwReplaced);
            if (dwNumPost) {
               memcpy ((DWORD*)pHyp->m_memTokens.p + dwCur, padwTokPost, dwNumPost * sizeof(DWORD));
               dwCur += dwNumPost;
            }
            pHyp->m_memTokens.m_dwCurPosn = dwCur * sizeof(DWORD);

            plPCCircumrealityNLPHyp->Add (&pHyp);
            fRet = TRUE;   // since created a parse
         } // j, over all parses
      } // i, over all words
   }
   else {   // else reword or split
      // parse the entire block
      pFrom->Parse (padwTokens, dwNum, TRUE, pcc+1, (m_dwType == 2) ? (dwNumCFG - 1) : 1, &lParsed);

      // loop through all the parses and do the substitutions
      for (j = 0; j < lParsed.Num(); j++) {
         // find the info
         DWORD dwLen = (DWORD)lParsed.Size(j) / sizeof(DWORD) - 1;
         DWORD *padw = (DWORD*)lParsed.Get(j);
         // dont actually care about padw[0] == dwReplaced, since should be the entire lot

         // how many elems are needed?
         DWORD dwNeed = dwNumPre + dwLen + dwNumPost;

         // create a new hypothesis
         PCCircumrealityNLPHyp pHyp = pHypBase->Clone();
         if (!pHyp)
            continue;

         // BUGFIX - Keep history of hypothesis so can be more accurate with probability
         pHyp->NewID();
         pHyp->m_lParents.Add (&pHypBase->m_dwID);

         pHyp->m_dwRuleElem = dwRuleElem;
         pHyp->m_dwRuleGroup = dwRuleGroup;
         pHyp->m_fProb = pHypBase->m_fProb * m_fProb;
         pHyp->m_fStable = FALSE;
         if (!pHyp->m_memTokens.Required (dwNeed * sizeof(DWORD))) {
            delete pHyp;
            continue;
         }

         // copy over the memory
         DWORD dwCur = 0;
         if (dwNumPre) {
            memcpy ((DWORD*)pHyp->m_memTokens.p + dwCur, padwTokPre, dwNumPre * sizeof(DWORD));
            dwCur += dwNumPre;
         }
         memcpy ((DWORD*)pHyp->m_memTokens.p + dwCur, padw+1, dwLen * sizeof(DWORD));
         dwCur += dwLen;
         if (dwNumPost) {
            memcpy ((DWORD*)pHyp->m_memTokens.p + dwCur, padwTokPost, dwNumPost * sizeof(DWORD));
            dwCur += dwNumPost;
         }
         pHyp->m_memTokens.m_dwCurPosn = dwCur * sizeof(DWORD);

         plPCCircumrealityNLPHyp->Add (&pHyp);
         fRet = TRUE;   // since created a parse
      } // j, over all parses
   } // if reword or split


   return fRet;
}




/*************************************************************************************
CCircumrealityNLPRule::Parse - Parses a series of tokens looking for matches. If they're
found then the matches are added to the hypothesis list.

NOTE: CalcCFG must have been called before doing this

inputs
   PCCircumrealityNLPHyp    pHypBase - Hypothesis to base this off of
   DWORD          *padwTokens - List of tokens to parse. These tokens can be separated
                     by -1 to indicate a sentence break.
   DWORD          dwNum - Number of tokens
   BOOL           fMultipleSent - Set to TRUE if any element in padwTokens[0..dwNum-1] == -1,
                  indicating an end of sentence
   DWORD          dwRuleGroup - Rule-group value for new hypothesis
   DWORD          dwRuleELem - Rule-element value for new hypothesis
   PCListFixed    plPCCircumrealityNLPHyp - List of hypothesis to add new hypothesis to
   PCMem          pMem - Scratch memory
returns
   BOOL - TRUE if added and hypthesis
*/
BOOL CCircumrealityNLPRule::Parse (PCCircumrealityNLPHyp pHypBase, DWORD *padwTokens, DWORD dwNum, BOOL fMultipleSent,
                         DWORD dwRuleGroup, DWORD dwRuleElem,
                         PCListFixed plPCCircumrealityNLPHyp, PCMem pMem)
{
   // for faster processing, if know it's only one sentence then don't bother
   // determining where the sentences begin and end
   if (!fMultipleSent)
      return ParseSentence (pHypBase, padwTokens, dwNum, NULL, 0, NULL, 0,
         dwRuleGroup, dwRuleElem, plPCCircumrealityNLPHyp, pMem);

   // loop through all the sentences
   DWORD dwSentStart, dwSentEnd;
   BOOL fRet = FALSE;

   for (dwSentStart = 0; dwSentStart < dwNum; dwSentStart = dwSentEnd + 1) {
      // find the sentence end
      for (dwSentEnd = dwSentStart; dwSentEnd < dwNum; dwSentEnd++)
         if (padwTokens[dwSentEnd] == (DWORD)-1)
            break;

      // parse this sentence
      fRet |= ParseSentence (pHypBase, padwTokens + dwSentStart, dwSentEnd - dwSentStart,
         padwTokens, dwSentStart, padwTokens + dwSentEnd, dwNum - dwSentEnd,
         dwRuleGroup, dwRuleElem, plPCCircumrealityNLPHyp, pMem);
   } // dwSentStart

   return fRet;
}



/*************************************************************************************
CCircumrealityNLPRuleSet::Constructor and destructor
*/
CCircumrealityNLPRuleSet::CCircumrealityNLPRuleSet (void)
{
   m_lCIRCUMREALITYRSINFO.Init (sizeof(CIRCUMREALITYRSINFO));
   m_pCur = NULL;
   m_dwCurIndex = -1;
   m_phTokens = NULL;
   MemZero (&m_memName);
   m_fEnabled = TRUE;
}

CCircumrealityNLPRuleSet::~CCircumrealityNLPRuleSet (void)
{
   // clear out
   while (m_lCIRCUMREALITYRSINFO.Num())
      CIRCUMREALITYRSINFOClear (m_lCIRCUMREALITYRSINFO.Num()-1); // BUGFIX - Delete from the end
}



/*************************************************************************************
CCircumrealityNLPRuleSet::CIRCUMREALITYRSINFOClear - Deletes the CIRCUMREALITYRSINFO at the given index.

inputs
   DWORD          dwIndex - From 0 to m_lCIRCUMREALITYRSINFO.Num()-1
returns
   BOOL - TRUE if cleared out
*/
BOOL CCircumrealityNLPRuleSet::CIRCUMREALITYRSINFOClear (DWORD dwIndex)
{
   PCIRCUMREALITYRSINFO pi = (PCIRCUMREALITYRSINFO) m_lCIRCUMREALITYRSINFO.Get(dwIndex);
   if (!pi)
      return FALSE;

   // delete
   DWORD i;
   if (pi->plPCCircumrealityNLPRule) {
      PCCircumrealityNLPRule *ppr = (PCCircumrealityNLPRule*)pi->plPCCircumrealityNLPRule->Get(0);
      for (i = 0; i < pi->plPCCircumrealityNLPRule->Num(); i++)
         delete ppr[i];
      delete pi->plPCCircumrealityNLPRule;
   }

   m_lCIRCUMREALITYRSINFO.Remove (dwIndex);
   m_pCur = NULL;
   m_dwCurIndex = (DWORD)-1;
   return TRUE;
}


/*************************************************************************************
CCircumrealityNLPRuleSet::Init - Initializes the rule set so it uses the given tokens.

inputs
   PCHashString         phTokens - Tokens
returns
   BOOL - TRUE if success
*/
BOOL CCircumrealityNLPRuleSet::Init (PCHashString phTokens)
{
   if (m_phTokens)
      return FALSE;
   m_phTokens = phTokens;
   return TRUE;
}



//static PWSTR gpszNLPRuleSet = L"NLPRuleSet";
   // NOTE - This definition should be put into CircumrealityDLL, and should be documented there

/*************************************************************************************
CCircumrealityNLPRuleSet:MMLTo - This writes the current rules to MML.

NOTE: Is uses the CURRENT language for writing
*/
PCMMLNode2 CCircumrealityNLPRuleSet::MMLTo (void)
{
   if (!m_pCur)
      return NULL;
   PCMMLNode2 pNode = new CMMLNode2;
   if (!pNode)
      return NULL;
   pNode->NameSet (CircumrealityNLPRuleSet());

   DWORD i;
   PCCircumrealityNLPRule *ppr = (PCCircumrealityNLPRule*)m_pCur->plPCCircumrealityNLPRule->Get(0);
   for (i = 0; i < m_pCur->plPCCircumrealityNLPRule->Num(); i++) {
      ppr[i]->MMLTo (pNode);
   } // i

   return pNode;
}


/*************************************************************************************
CCircumrealityNLPRuleSet::MMLFrom - This reads in the given language, overwriting any information
that's there already.

inputs
   PCMMLNode2         pNode - Node to read from
   LANGID            lid - Language to overwrite. If this is -1 then it's an internal
                        flag to do a recusive MMLFrom, in case the MML node
                        has a <NLPRuleSet> node within itself.
returns
   BOOL - TRUE if success
*/
BOOL CCircumrealityNLPRuleSet::MMLFrom (PCMMLNode2 pNode, LANGID lid)
{
   if (lid != (LANGID)-1) {
      LanguageSet (lid, 2);   // look for exact match
      if (m_pCur && (m_pCur->lid == lid))
         CIRCUMREALITYRSINFOClear (m_dwCurIndex);   // wipe out current

      // create a new one
      CIRCUMREALITYRSINFO in;
      memset (&in, 0, sizeof(in));
      in.lid = lid;
      in.plPCCircumrealityNLPRule = new CListFixed;
      if (!in.plPCCircumrealityNLPRule)
         return FALSE;
      in.plPCCircumrealityNLPRule->Init (sizeof(PCCircumrealityNLPRule));

      // add it
      m_dwCurIndex = m_lCIRCUMREALITYRSINFO.Num();
      m_lCIRCUMREALITYRSINFO.Add (&in);
      m_pCur =(PCIRCUMREALITYRSINFO) m_lCIRCUMREALITYRSINFO.Get(m_dwCurIndex);
   }

   // prepapre rule to put in
   PCCircumrealityNLPRule pNew = new CCircumrealityNLPRule;
   if (!pNew)
      return FALSE;  // error
   pNew->Init (m_phTokens);

   // loop
   DWORD i;
   PWSTR psz;
   PCMMLNode2 pSub;
   for (i = 0; i < pNode->ContentNum (); i++) {
      pSub = NULL;
      pNode->ContentEnum (i, &psz, &pSub);
      if (!pSub)
         continue;

      // see if found one
      if (pNew->MMLFrom (pSub)) {
         // add to the list
         m_pCur->plPCCircumrealityNLPRule->Add (&pNew);

         // create a new storage element
         pNew = new CCircumrealityNLPRule;
         if (!pNew)
            return FALSE;  // error
         pNew->Init (m_phTokens);

         continue;
      }

      // else, not a match. May be another rule set
      psz = pSub->NameGet();
      if (psz && !_wcsicmp(psz, CircumrealityNLPRuleSet()))
         MMLFrom (pSub, (LANGID)-1);   // recurse
   } // i

   // delete prepared rule
   delete pNew;

   return TRUE;   // done
}

/*************************************************************************************
CCircumrealityNLPRuleSet::CloneTo - Clones this rule set on top of another one
*/
BOOL CCircumrealityNLPRuleSet::CloneTo (CCircumrealityNLPRuleSet *pTo)
{
   // clear out pTo
   while (pTo->m_lCIRCUMREALITYRSINFO.Num())
      pTo->CIRCUMREALITYRSINFOClear (0);

   // clone
   pTo->m_lCIRCUMREALITYRSINFO.Init (sizeof(CIRCUMREALITYRSINFO), m_lCIRCUMREALITYRSINFO.Get(0), m_lCIRCUMREALITYRSINFO.Num());
   PCIRCUMREALITYRSINFO pi = (PCIRCUMREALITYRSINFO)pTo->m_lCIRCUMREALITYRSINFO.Get(0);
   DWORD i, j;
   for (i = 0; i < pTo->m_lCIRCUMREALITYRSINFO.Num(); i++) {
      PCListFixed pOld = pi[i].plPCCircumrealityNLPRule;
      pi[i].plPCCircumrealityNLPRule = new CListFixed;
      pi[i].plPCCircumrealityNLPRule->Init (sizeof(PCCircumrealityNLPRule), pOld->Get(0), pOld->Num());

      // clone sub-elems
      PCCircumrealityNLPRule *ppr = (PCCircumrealityNLPRule*)pi[i].plPCCircumrealityNLPRule->Get(0);
      for (j = 0; j < pi[i].plPCCircumrealityNLPRule->Num(); j++)
         ppr[j] = ppr[j]->Clone();
   }

   pTo->m_phTokens = m_phTokens;
   pTo->m_dwCurIndex = m_dwCurIndex;
   pTo->m_pCur = (PCIRCUMREALITYRSINFO) pTo->m_lCIRCUMREALITYRSINFO.Get(pTo->m_dwCurIndex);
   MemZero (&pTo->m_memName);
   MemCat (&pTo->m_memName, (PWSTR)m_memName.p);
   pTo->m_fEnabled = m_fEnabled;

   return TRUE;
}



/*************************************************************************************
CCircumrealityNLPRuleSet::Clone - Standard API
*/
CCircumrealityNLPRuleSet *CCircumrealityNLPRuleSet::Clone (void)
{
   CCircumrealityNLPRuleSet *pNew = new CCircumrealityNLPRuleSet;
   if (!pNew)
      return NULL;
   if (!CloneTo (pNew)) {
      delete pNew;
      return NULL;
   }
   return pNew;
}



/*************************************************************************************
CCircumrealityNLPRuleSet::LanguageSet - Sets the language that's to be used for the rule-set.
You need to do this because each rule set can contain rules for different languages.

inputs
   LANGID            lid - Language to look for
   DWORD             dwExact - The search always tries to find the closest match.
                           0 => will take the best it can get, 1=>fails if can't find
                           a match for the major language group, 2=>fails if can't find exact match
returns
   BOOL - TRUE if usccess
*/
BOOL CCircumrealityNLPRuleSet::LanguageSet (LANGID lid, DWORD dwExact)
{
   if (m_pCur && (m_pCur->lid == lid))
      return TRUE;   // nothing to do

   PCIRCUMREALITYRSINFO pr = (PCIRCUMREALITYRSINFO)m_lCIRCUMREALITYRSINFO.Get(0);
   DWORD dwBest = -1, dwClose = -1;
   WORD wPrimary = PRIMARYLANGID(lid);
   DWORD i;
   for (i = 0; i < m_lCIRCUMREALITYRSINFO.Num(); i++) {
      if (pr->lid == lid)
         dwBest = i;
      else if (PRIMARYLANGID(pr->lid) == wPrimary)
         dwClose = i;
   } // i

   if (dwBest != (DWORD)-1) {
      m_dwCurIndex = dwBest;
      m_pCur = pr + m_dwCurIndex;
      return TRUE;
   }
   else if ((dwClose != (DWORD) -1) && (dwExact < 2)) {
      m_dwCurIndex = dwClose;
      m_pCur = pr + m_dwCurIndex;
      return TRUE;
   }
   else if (m_lCIRCUMREALITYRSINFO.Num() && (dwExact < 1)) {
      // pick one
      m_dwCurIndex = 0;
      m_pCur = pr + m_dwCurIndex;
      return TRUE;
   }
   else {
      m_dwCurIndex = -1;
      m_pCur = NULL;
      return FALSE;
   }
}



/*************************************************************************************
CCircumrealityNLPRuleSet::LanguageGet - Returns the language ID of the currently selected
language.

returns
   LANGID - Language, or 0 if no language selected
*/
LANGID CCircumrealityNLPRuleSet::LanguageGet (void)
{
   return m_pCur ? m_pCur->lid : 0;
}



/*************************************************************************************
CCircumrealityNLPRuleSet::RuleNum - Returns the number of rules in the set (using the currently
selected language)
*/
DWORD CCircumrealityNLPRuleSet::RuleNum (void)
{
   if (!m_pCur || !m_pCur->plPCCircumrealityNLPRule)
      return NULL;

   return m_pCur->plPCCircumrealityNLPRule->Num();
}



/*************************************************************************************
CCircumrealityNLPRuleSet::RuleGet - Gets the rule (using the currently selected language)

inputs
   DWORD          dwIndex - 0 .. RuleNum()-1
returns
   PCCircumrealityNLPRule - Rule object. This can be changed but DON'T delete it
*/
PCCircumrealityNLPRule CCircumrealityNLPRuleSet::RuleGet (DWORD dwIndex)
{
   if (!m_pCur || !m_pCur->plPCCircumrealityNLPRule)
      return NULL;

   PCCircumrealityNLPRule *ppr = (PCCircumrealityNLPRule*) m_pCur->plPCCircumrealityNLPRule->Get(dwIndex);
   if (!ppr)
      return NULL;
   return *ppr;
}


/*************************************************************************************
CCircumrealityNLPRuleSet::RuleRemove - Deletes the rule (using the currently selected language)

inputs
   DWORD          dwIndex - 0 .. RuleNum()-1
returns
   BOOL - TRUE if found and deleted it
*/
BOOL CCircumrealityNLPRuleSet::RuleRemove (DWORD dwIndex)
{
   if (!m_pCur || !m_pCur->plPCCircumrealityNLPRule)
      return FALSE;

   PCCircumrealityNLPRule *ppr = (PCCircumrealityNLPRule*) m_pCur->plPCCircumrealityNLPRule->Get(dwIndex);
   if (!ppr)
      return FALSE;

   delete ppr[0];
   m_pCur->plPCCircumrealityNLPRule->Remove (dwIndex);
   return TRUE;

}


/*************************************************************************************
CCircumrealityNLPRuleSet::RuleAdd - Adds a blank rule to the rule set.

returns
   DWORD - New rule's index number, or -1 if couldn't add
*/
DWORD CCircumrealityNLPRuleSet::RuleAdd (void)
{
   if (!m_pCur || !m_pCur->plPCCircumrealityNLPRule)
      return (DWORD)-1;

   PCCircumrealityNLPRule pNew = new CCircumrealityNLPRule;
   if (!pNew)
      return (DWORD)-1;
   pNew->Init (m_phTokens);

   return m_pCur->plPCCircumrealityNLPRule->Add (&pNew);
}


/***********************************************************************
CCircumrealityNLPHypSortCallback - Callback to sort the list.
*/
static int __cdecl CCircumrealityNLPRuleSortCallback0 (const void *elem1, const void *elem2 )
{
   PCCircumrealityNLPRule *p1, *p2;
   p1 = (PCCircumrealityNLPRule *) elem1;
   p2 = (PCCircumrealityNLPRule *) elem2;

   PWSTR psz1 = (PWSTR) p1[0]->m_lCFGString.Get(0);
   PWSTR psz2 = (PWSTR) p2[0]->m_lCFGString.Get(0);

   if (!psz1)
      psz1 = L"";
   if (!psz2)
      psz2 = L"";

   return _wcsicmp(psz1, psz2);
}

static int __cdecl CCircumrealityNLPRuleSortCallback1 (const void *elem1, const void *elem2 )
{
   PCCircumrealityNLPRule *p1, *p2;
   p1 = (PCCircumrealityNLPRule *) elem1;
   p2 = (PCCircumrealityNLPRule *) elem2;

   PWSTR psz1 = (PWSTR) p1[0]->m_lCFGString.Get(1);
   PWSTR psz2 = (PWSTR) p2[0]->m_lCFGString.Get(1);

   if (!psz1)
      psz1 = L"";
   if (!psz2)
      psz2 = L"";

   return _wcsicmp(psz1, psz2);
}

static int __cdecl CCircumrealityNLPRuleSortCallback2 (const void *elem1, const void *elem2 )
{
   PCCircumrealityNLPRule *p1, *p2;
   p1 = (PCCircumrealityNLPRule *) elem1;
   p2 = (PCCircumrealityNLPRule *) elem2;

   if (p1[0]->m_fProb < p2[0]->m_fProb)
      return 1;
   else if (p1[0]->m_fProb > p2[0]->m_fProb)
      return -1;
   else
      return 0;
}



/*************************************************************************************
CCircumrealityNLPRuleSet::RuleSort - Sort the rules.

inputs
   DWORD          dwSort - If 0 then sort by from, 1 then first to, 2 then score
returns
   none
*/
void CCircumrealityNLPRuleSet::RuleSort (DWORD dwSort)
{
   if (!m_pCur || !m_pCur->plPCCircumrealityNLPRule)
      return;

   qsort (m_pCur->plPCCircumrealityNLPRule->Get(0), m_pCur->plPCCircumrealityNLPRule->Num(),
      sizeof(PCCircumrealityNLPRule),
      dwSort ? ((dwSort==1) ? CCircumrealityNLPRuleSortCallback1 : CCircumrealityNLPRuleSortCallback2) : CCircumrealityNLPRuleSortCallback0);
}



/*************************************************************************************
CCircumrealityNLPParser::Constructor and destructor
*/
CCircumrealityNLPParser::CCircumrealityNLPParser (void)
{
   MemZero (&m_memName);
   m_lPCCircumrealityNLPRuleSet.Init (sizeof(PCCircumrealityNLPRuleSet));
   m_lPCCircumrealityNLPHyp.Init (sizeof(PCCircumrealityNLPHyp));
   m_phTokens = NULL;
   m_hTokensTemp.Init (0, 100);
   m_lid = 0;
   m_dwExact = 0;
}

CCircumrealityNLPParser::~CCircumrealityNLPParser (void)
{
   HypClear();

   // free up the rule sets
   DWORD i;
   PCCircumrealityNLPRuleSet *ppr = (PCCircumrealityNLPRuleSet*) m_lPCCircumrealityNLPRuleSet.Get(0);
   for (i = 0; i < m_lPCCircumrealityNLPRuleSet.Num(); i++)
      delete ppr[i];
   m_lPCCircumrealityNLPRuleSet.Clear();
}

/*************************************************************************************
CCircumrealityNLPParser::Init - Initializes the rule set so it uses the given tokens.

inputs
   PCHashString         phTokens - Tokens
returns
   BOOL - TRUE if success
*/
BOOL CCircumrealityNLPParser::Init (PCHashString phTokens)
{
   if (m_phTokens)
      return FALSE;
   m_phTokens = phTokens;
   return TRUE;
}


/*************************************************************************************
CCircumrealityNLPParser::CloneTo - Copies settings to the parser.
*/
BOOL CCircumrealityNLPParser::CloneTo (CCircumrealityNLPParser *pTo)
{
   MemZero (&pTo->m_memName);
   MemCat (&pTo->m_memName, (PWSTR)m_memName.p);
   pTo->m_phTokens = m_phTokens;
   pTo->m_lid = m_lid;
   pTo->m_dwExact = m_dwExact;

   // free up the rule sets
   DWORD i;
   PCCircumrealityNLPRuleSet *ppr = (PCCircumrealityNLPRuleSet*) pTo->m_lPCCircumrealityNLPRuleSet.Get(0);
   for (i = 0; i < pTo->m_lPCCircumrealityNLPRuleSet.Num(); i++)
      delete ppr[i];
   pTo->m_lPCCircumrealityNLPRuleSet.Clear();

   // copy over
   pTo->m_lPCCircumrealityNLPRuleSet.Init (sizeof(PCCircumrealityNLPRuleSet), m_lPCCircumrealityNLPRuleSet.Get(0), m_lPCCircumrealityNLPRuleSet.Num());
   ppr = (PCCircumrealityNLPRuleSet*) pTo->m_lPCCircumrealityNLPRuleSet.Get(0);
   for (i = 0; i < pTo->m_lPCCircumrealityNLPRuleSet.Num(); i++)
      ppr[i] = ppr[i]->Clone();

   // hypothesis
   pTo->HypClear();
   pTo->m_lPCCircumrealityNLPHyp.Init (sizeof(PCCircumrealityNLPHyp), m_lPCCircumrealityNLPHyp.Get(0), m_lPCCircumrealityNLPHyp.Num());
   PCCircumrealityNLPHyp *pph = (PCCircumrealityNLPHyp*) pTo->m_lPCCircumrealityNLPHyp.Get(0);
   for (i = 0; i < pTo->m_lPCCircumrealityNLPHyp.Num(); i++)
      pph[i] = pph[i]->Clone();

   return TRUE;
}

/*************************************************************************************
CCircumrealityNLPParser::Clone - Standard API
*/
CCircumrealityNLPParser *CCircumrealityNLPParser::Clone (void)
{
   CCircumrealityNLPParser *pNew = new CCircumrealityNLPParser;
   if (!pNew)
      return NULL;
   if (!CloneTo (pNew)) {
      delete pNew;
      return NULL;
   }
   return pNew;
}

/*************************************************************************************
CCircumrealityNLPParser::LanguageSet - Sets the language that's to be used for the rule-set.
You need to do this because each rule set can contain rules for different languages.

inputs
   LANGID            lid - Language to look for
   DWORD             dwExact - The search always tries to find the closest match.
                           0 => will take the best it can get, 1=>fails if can't find
                           a match for the major language group, 2=>fails if can't find exact match
returns
   BOOL - TRUE if usccess. This will always be TRUE, although some rules may be deactivated
            as a result of the dwExact requirement.
*/
BOOL CCircumrealityNLPParser::LanguageSet (LANGID lid, DWORD dwExact)
{
   m_lid = lid;
   m_dwExact = dwExact;

   DWORD i;
   PCCircumrealityNLPRuleSet *ppr = (PCCircumrealityNLPRuleSet*) m_lPCCircumrealityNLPRuleSet.Get(0);
   for (i = 0; i < m_lPCCircumrealityNLPRuleSet.Num(); i++)
      ppr[i]->LanguageSet (m_lid, m_dwExact);

   return TRUE;
}


/*************************************************************************************
CCircumrealityNLPParser::LanguageGet - Returns the language passed in by language set
*/
LANGID CCircumrealityNLPParser::LanguageGet (void)
{
   return m_lid;
}



/*************************************************************************************
CCircumrealityNLPParser::RuleSetNum - Returns the number of rule sets
*/
DWORD CCircumrealityNLPParser::RuleSetNum (void)
{
   return m_lPCCircumrealityNLPRuleSet.Num();
}




/*************************************************************************************
CCircumrealityNLPParser::RuleSetGet - Gets a rule set object based on the index.
Do NOT delete it though.

inputs
   DWORD          dwIndex - From 0 to RuleSetNum()-1
returns
   PCCircumrealityNLPRuleSet - Rule set, or NULL if error
*/
PCCircumrealityNLPRuleSet CCircumrealityNLPParser::RuleSetGet (DWORD dwIndex)
{
   PCCircumrealityNLPRuleSet *ppr = (PCCircumrealityNLPRuleSet*) m_lPCCircumrealityNLPRuleSet.Get(dwIndex);
   if (!ppr)
      return NULL;
   return ppr[0];
}




/*************************************************************************************
CCircumrealityNLPParser::RuleSetRemove - Deletes the rule set at the given index.

inputs
   DWORD          dwIndex - From 0 to RuleSetNum()-1
returns
   BOOL - TRUE if deleted
*/
BOOL CCircumrealityNLPParser::RuleSetRemove (DWORD dwIndex)
{
   PCCircumrealityNLPRuleSet *ppr = (PCCircumrealityNLPRuleSet*) m_lPCCircumrealityNLPRuleSet.Get(dwIndex);
   if (!ppr)
      return FALSE;
   delete ppr[0];
   m_lPCCircumrealityNLPRuleSet.Remove (dwIndex);
   return TRUE;
}



/*************************************************************************************
CCircumrealityNLPParser::RuleSetFind - Finds a rule set based on the name.

NOTE: This is a slow find, so hopefully there aren't too many rule sets.

inputs
   PWSTR          pszName - Name of the rule (case insensative)
returns
   DWORD - Index, or -1 if can't find
*/
DWORD CCircumrealityNLPParser::RuleSetFind (PWSTR pszName)
{
   DWORD i;
   PCCircumrealityNLPRuleSet *ppr = (PCCircumrealityNLPRuleSet*) m_lPCCircumrealityNLPRuleSet.Get(0);
   for (i = 0; i < m_lPCCircumrealityNLPRuleSet.Num(); i++)
      if (!_wcsicmp((PWSTR)ppr[i]->m_memName.p, pszName))
         return i;

   return (DWORD)-1;
}



/*************************************************************************************
CCircumrealityNLPParser::RuleSetAdd - Adds a blank rule set to the list

inputs
   PWSTR          pszName - Name of the ruleset. (The ruleset object will have this
                  filled in). NOTE: This does NOT test to make sure the name is unique.
returns
   DWORD - Index of the new rule, or -1 if error
*/
DWORD CCircumrealityNLPParser::RuleSetAdd (PWSTR pszName)
{
   PCCircumrealityNLPRuleSet pNew = new CCircumrealityNLPRuleSet;
   if (!pNew)
      return (DWORD)-1;
   if (!pNew->Init (m_phTokens)) {
      delete pNew;
      return (DWORD)-1;
   }
   MemZero (&pNew->m_memName);
   MemCat (&pNew->m_memName, pszName);

   return m_lPCCircumrealityNLPRuleSet.Add (&pNew);
}



/*************************************************************************************
CCircumrealityNLPParser::HypClear - Clears the hypothesis list
*/
void CCircumrealityNLPParser::HypClear (void)
{
   DWORD i;
   PCCircumrealityNLPHyp *pph = (PCCircumrealityNLPHyp*)m_lPCCircumrealityNLPHyp.Get(0);
   for (i = 0; i < m_lPCCircumrealityNLPHyp.Num(); i++)
      delete pph[i];
   m_lPCCircumrealityNLPHyp.Clear();

   // while at it, clear out the temporary tokens
   m_hTokensTemp.Clear();
}

/*************************************************************************************
CCircumrealityNLPParser::WordBreak - This takes a sentence (in the current language) and
breaks it into individual tokens. It prefers using tokens from m_phTokens, but if
new tokens are needed this will add to m_hTokensTemp.

NOTE: The resulting parses will by added to the hypothesis list. Therefore, the
caller of this function should probably call HypClear() first.

This translates parts in quotes as entire tokens, converting &lt; and other
MML tokens to <, etc.

inputs
   PWSTR             pszText - Text to break into words, using the current LanguageSet() value
returns
   BOOL - TRUE if success
*/
BOOL CCircumrealityNLPParser::WordBreak (PWSTR pszText)
{
   // create the hypothesis
   PCCircumrealityNLPHyp pHyp = new CCircumrealityNLPHyp;
   if (!pHyp)
      return FALSE;

   // BUGFIX - keep track of hypothesis IDs
   m_dwIDHyp = 0;
   pHyp->m_lParents.Add (&m_dwIDHyp);
   pHyp->m_pdwIDCount = &m_dwIDHyp;
   pHyp->NewID();

   pHyp->m_dwRuleElem = pHyp->m_dwRuleGroup = 0;
   pHyp->m_fProb = 1;
   pHyp->m_fStable = FALSE;
   pHyp->m_memTokens.m_dwCurPosn = 0;


   CMem mem;

   // pull out the quotes...
   while (TRUE) {
      DWORD i, j;
      for (i = 0; pszText[i]; i++)
         if (pszText[i] == L'"')
            break;
      if (!pszText[i]) {
         // no quotes
         if (i)
            WordBreakNoQuotes (pszText, pHyp);
         break;
      }

      // find the end of the quotes
      for (j = i+1; pszText[j]; j++)
         if (pszText[j] == L'"')
            break;

      // put in NULL chars so can parse easier
      WCHAR cAtI, cAtJ;
      cAtI = pszText[i];
      pszText[i] = 0;
      cAtJ = pszText[j];
      pszText[j] = 0;

      // words before quotes
      if (i)
         WordBreakNoQuotes (pszText, pHyp);

      // convert quotes and add...
      // convert &lt; to <, &gt; to >, etc. This way can parse commands with
      // entire E-mail messages
      if (MMLStringToString (pszText + (i+1), FALSE, NULL, &mem)) {
         WordBreakAddToken ((PWSTR)mem.p, pHyp, TRUE);   // NOTE: Force to be a temp token
      } // add

      // restore
      pszText[i] = cAtI;
      pszText[j] = cAtJ;
      if (!cAtJ)
         break;   // was at the end anyway
      pszText += (j+1); // so move on
   } // while (TRUE), pulling out quotes


   // add the hypothesis
   m_lPCCircumrealityNLPHyp.Add (&pHyp);
   return TRUE;
}

/*************************************************************************************
CCircumrealityNLPParser::WordBreakAddToken - Adds a token to the hypothesis.

inputs
   PWSTR          pszToken - Token name
   PCCircumrealityNLPHyp    pHyp - Hypothesis
   BOOL           fForceTempToken - If TRUE then the token is forced into
                     being a temp token, so it isn't matched against the
                     usual comparisons. Use this for items in quotes
returns
   BOOL - TRUE if success
*/
BOOL CCircumrealityNLPParser::WordBreakAddToken (PWSTR pszToken, PCCircumrealityNLPHyp pHyp,
                                       BOOL fForceTempToken)
{
   // find the token in the main list
   DWORD dwIndex = fForceTempToken ? (DWORD)-1 : m_phTokens->FindIndex (pszToken, TRUE);
   if (dwIndex == -1) {
      // see if can find it in temporary tokens
      dwIndex = m_hTokensTemp.FindIndex (pszToken, FALSE);  // NOTE: Use tokenstemp as case sensative
      if (dwIndex != -1)
         dwIndex += m_phTokens->Num(); // so offset
   }
   if (dwIndex == -1) {
      // add it
      m_hTokensTemp.Add (pszToken, NULL, FALSE);   // case sensative in tokens temp
      dwIndex = m_hTokensTemp.FindIndex (pszToken, FALSE);  // NOTE: Use tokenstemp as case sensative
      if (dwIndex != -1)
         dwIndex += m_phTokens->Num(); // so offset
   }
   if (dwIndex == -1)
      return FALSE;  // error, cant seem to add

   // append to hyp memory
   if (!pHyp->m_memTokens.Required (pHyp->m_memTokens.m_dwCurPosn + sizeof(dwIndex)))
      return FALSE;
   memcpy ((PBYTE)pHyp->m_memTokens.p + pHyp->m_memTokens.m_dwCurPosn, &dwIndex, sizeof(dwIndex));
   pHyp->m_memTokens.m_dwCurPosn += sizeof(dwIndex);

   return TRUE;
}




/*************************************************************************************
CCircumrealityNLPParser::WordBreakNoQuotes - Do word breaking, knowing that there aren't any quotes.

This one pulls out guids, where have "|<guid>" like "|afe034a4034"...
inputs
   PWSTR          pszText - String to break up
   PCCircumrealityNLPHyp    pHyp - Hypothesis to add to
returns
   BOOL - TRUE if success
*/
BOOL CCircumrealityNLPParser::WordBreakNoQuotes (PWSTR pszText, PCCircumrealityNLPHyp pHyp)
{
   // loop, pulling out numbers
   while (TRUE) {
      DWORD i, j;
      for (i = 0; pszText[i]; i++) {
         if (pszText[i] != L'|')
            continue;   // only care about vertical bars

         // make sure next digits are guid
         BOOL fNotGUID = FALSE;
         for (j = i + 1; j < i+1+sizeof(GUID)*2; j++)
            if (!( ((pszText[j] >= L'0') && (pszText[j] <= L'9')) ||
               ((pszText[j] >= L'A') && (pszText[j] <= L'F')) ||
               ((pszText[j] >= L'a') && (pszText[j] <= L'f')) )) {
                  fNotGUID = TRUE;
                  break;
               }
         if (!fNotGUID)
            break;   // found limits
      } // i

      if (!pszText[i]) {
         // no digits found, so go on
         if (i)
            WordBreakNoGUIDs (pszText, pHyp);
         return TRUE;
      }

      // should have isolated the number
      // put in NULL chars so can parse easier
      WCHAR cAtI, cAtJ;
      cAtI = pszText[i];
      pszText[i] = 0;
      cAtJ = pszText[j];
      pszText[j] = 0;

      // words before quotes
      if (i)
         WordBreakNoGUIDs (pszText, pHyp);

      // add digits
      pszText[i] = cAtI;
      WordBreakAddToken (pszText + i, pHyp);

      // restore
      pszText[j] = cAtJ;
      pszText += j; // so move on
   } // while (TRUE)

   // done
   return TRUE;
}




/*************************************************************************************
CCircumrealityNLPParser::WordBreakNoGUIDs - Do word breaking, knowing that there aren't any quotes or guids.

This one pulls out numbers...

inputs
   PWSTR          pszText - String to break up
   PCCircumrealityNLPHyp    pHyp - Hypothesis to add to
returns
   BOOL - TRUE if success
*/
BOOL CCircumrealityNLPParser::WordBreakNoGUIDs (PWSTR pszText, PCCircumrealityNLPHyp pHyp)
{
   // loop, pulling out numbers
   while (TRUE) {
      DWORD i, j;
      for (i = 0; pszText[i]; i++)
         if (iswdigit(pszText[i]))
            break;

      if (!pszText[i]) {
         // no digits found, so go on
         if (i)
            WordBreakNoNumbers (pszText, pHyp);
         return TRUE;
      }

      // NOTE: Specifically NOT isolating $, or euro-sign, or colon for time, etc.
      // just want number

      // go back if have negatives or decimals in front
      DWORD dwDigit = i;
      if (i && (pszText[i-1] == L'.'))
         i--;
      // NOTE: Defintiion of what's a '.' is language dependent, since some languages use other symbols

      if (i && (pszText[i-1] == L'-'))
         i--;

      // loop until not digit after where initally found one
      for (j = dwDigit+1; iswdigit(pszText[j]); j++);

      // if it's a period then ok so long as followed by digit
      if ((pszText[j] == L'.') && (iswdigit(pszText[j+1])))
         for (j = j+1; iswdigit(pszText[j]); j++);

      // should have isolated the number
      // put in NULL chars so can parse easier
      WCHAR cAtI, cAtJ;
      cAtI = pszText[i];
      pszText[i] = 0;
      cAtJ = pszText[j];
      pszText[j] = 0;

      // words before quotes
      if (i)
         WordBreakNoNumbers (pszText, pHyp);

      // add digits
      pszText[i] = cAtI;
      WordBreakAddToken (pszText + i, pHyp);

      // restore
      pszText[j] = cAtJ;
      pszText += j; // so move on
   } // while (TRUE)

   // done
   return TRUE;
}



/*************************************************************************************
IsOKPunct - Test for OK punctuation. Basically, all punctuation except '-' and '''
since used in words.
*/
BOOL IsOKPunct (WCHAR c)
{
   if ((c == L'-') || (c == L'\'') || (c == L'`'))
      return FALSE;
   return iswpunct (c);
}


/*************************************************************************************
CCircumrealityNLPParser::WordBreakNoNumbers - Do word breaking, knowing that there aren't any quotes
or numbers.

This one pulls out punctuation...

inputs
   PWSTR          pszText - String to break up
   PCCircumrealityNLPHyp    pHyp - Hypothesis to add to
returns
   BOOL - TRUE if success
*/
BOOL CCircumrealityNLPParser::WordBreakNoNumbers (PWSTR pszText, PCCircumrealityNLPHyp pHyp)
{
   // loop, pulling out numbers
   while (TRUE) {
      DWORD i;
      for (i = 0; pszText[i]; i++)
         if (IsOKPunct(pszText[i])) // BUGFIX - Was iswpunct()
            break;

      if (!pszText[i]) {
         // no digits found, so go on
         if (i)
            WordBreakNoPunct (pszText, pHyp);
         return TRUE;
      }

      // isolate the punctuation
      WCHAR szPunct[2];
      szPunct[0] = pszText[i];
      szPunct[1] = 0;
      pszText[i] = 0;

      // words before quotes
      if (i)
         WordBreakNoPunct (pszText, pHyp);

      // add digits
      WordBreakAddToken (szPunct, pHyp);

      // restore
      pszText[i] = szPunct[0];
      pszText += (i+1); // so move on
   } // while (TRUE)

   // done
   return TRUE;
}



/*************************************************************************************
CCircumrealityNLPParser::WordBreakNoPunct - Do word breaking, knowing that there aren't any quotes
or numbers, or punctuation.

This one pulls out words...

inputs
   PWSTR          pszText - String to break up
   PCCircumrealityNLPHyp    pHyp - Hypothesis to add to
returns
   BOOL - TRUE if success
*/
BOOL CCircumrealityNLPParser::WordBreakNoPunct (PWSTR pszText, PCCircumrealityNLPHyp pHyp)
{
   // NOTE: This will change for languages such as japanese, where will need
   // to do work breaking

   // loop, pulling out numbers
   while (TRUE) {
      // skip whitespace
      while (iswspace(pszText[0]))
         pszText++;

      DWORD i;
      for (i = 0; pszText[i]; i++)
         if (iswspace (pszText[i]))
            break;

      if (!pszText[i]) {
         // no digits found, so go on
         if (i)
            WordBreakAddToken (pszText, pHyp);
         return TRUE;
      }

      // isolate the whitespace
      WCHAR cAtI = pszText[i];
      pszText[i] = 0;

      // words before quotes
      if (i)
         WordBreakNoPunct (pszText, pHyp);

      // restore
      pszText[i] = cAtI;
      pszText += (i+1); // so move on
   } // while (TRUE)

   // done
   return TRUE;
}




/*************************************************************************************
CCircumrealityNLPParser::HypExpand - This looks at a hypothesis m_dwRuleGroup && m_dwRuleElem
and keeps on expanding the hypothesis until there are not more rules that can
be applied. It starts expanding at the rule defined by m_dwRuleGroup and m_dwRuleElem...

inputs
   PCCircumrealityNLPHyp       pHyp - Hypothesis to expand
returns
   BOOL - TRUE if success
*/
BOOL CCircumrealityNLPParser::HypExpand (PCCircumrealityNLPHyp pHyp)
{
   // if it's frozen then don't bother
   if (pHyp->m_fStable)
      return TRUE;

   CMem memScratch;  // used in hypothesis expansion

   // determine if this has multiple sentences
   DWORD *padwTokens = (DWORD*)pHyp->m_memTokens.p;
   DWORD dwNum = (DWORD)pHyp->m_memTokens.m_dwCurPosn / sizeof(DWORD);
   DWORD i;
   for (i = 0; i < dwNum; i++)
      if (padwTokens[i] == (DWORD)-1)
         break;   // has sentence break
   BOOL fMultipleSent = (i < dwNum);

   while (pHyp->m_dwRuleGroup < RuleSetNum()) {
      PCCircumrealityNLPRuleSet pSet = RuleSetGet (pHyp->m_dwRuleGroup);

      // if the rule set is disabled then skip
      if (!pSet->m_fEnabled) {
         pHyp->m_dwRuleGroup++;
         pHyp->m_dwRuleElem = 0;
         continue;
      }

      // loop through all the individual rules
      while (pHyp->m_dwRuleElem < pSet->RuleNum()) {
         PCCircumrealityNLPRule pRule = pSet->RuleGet(pHyp->m_dwRuleElem);
         pHyp->m_dwRuleElem++;   // so next time will parse others

         pRule->Parse (pHyp, padwTokens, dwNum,
            fMultipleSent, pHyp->m_dwRuleGroup, pHyp->m_dwRuleElem,
            &m_lPCCircumrealityNLPHyp, &memScratch);
      }

      // go to next rule set
      pHyp->m_dwRuleGroup++;
      pHyp->m_dwRuleElem = 0;
   } // while rule group

   // done
   return TRUE;
}


/*************************************************************************************
CCircumrealityNLPParser::HypNum - Returns the number of hypothesis
*/
DWORD CCircumrealityNLPParser::HypNum (void)
{
   return m_lPCCircumrealityNLPHyp.Num();
}

/*************************************************************************************
CCircumrealityNLPParser::HypGet - Gets a hypothesis object based on the index.
Do NOT delete it though.

inputs
   DWORD          dwIndex - From 0 to HypNum()-1
returns
   PCCircumrealityNLPHyp - Hypothesis, or NULL if error
*/
PCCircumrealityNLPHyp CCircumrealityNLPParser::HypGet (DWORD dwIndex)
{
   PCCircumrealityNLPHyp *ppr = (PCCircumrealityNLPHyp*) m_lPCCircumrealityNLPHyp.Get(dwIndex);
   if (!ppr)
      return NULL;
   return ppr[0];
}


/***********************************************************************
CCircumrealityNLPHypSortCallback - Callback to sort the list.
*/
static int __cdecl CCircumrealityNLPHypSortCallback (const void *elem1, const void *elem2 )
{
   PCCircumrealityNLPHyp *p1, *p2;
   p1 = (PCCircumrealityNLPHyp *) elem1;
   p2 = (PCCircumrealityNLPHyp *) elem2;

   PCMem pm1 = &(*p1)->m_memTokens;
   PCMem pm2 = &(*p2)->m_memTokens;

   // first compare size
   if (pm1->m_dwCurPosn < pm2->m_dwCurPosn)
      return -1;
   else if (pm1->m_dwCurPosn > pm2->m_dwCurPosn)
      return 1;

   // then memcmp
   return memcmp (pm1->p, pm2->p, pm1->m_dwCurPosn);
}

static int __cdecl CCircumrealityNLPHypSortCallback2 (const void *elem1, const void *elem2 )
{
   PCCircumrealityNLPHyp *p1, *p2;
   p1 = (PCCircumrealityNLPHyp *) elem1;
   p2 = (PCCircumrealityNLPHyp *) elem2;

   if (p1[0]->m_fProb < p2[0]->m_fProb)
      return 1;
   else if (p1[0]->m_fProb > p2[0]->m_fProb)
      return -1;
   else
      return 0;
}


/*************************************************************************************
CCircumrealityNLPParser::HypSort - Sort the hypothesis.

inputs
   DWORD          dwSort - If 0 then sort by tokens (so similar tokens together),
                           if 1 then sort by score (so high score at the top)
returns
   none
*/
void CCircumrealityNLPParser::HypSort (DWORD dwSort)
{
   qsort (m_lPCCircumrealityNLPHyp.Get(0), m_lPCCircumrealityNLPHyp.Num(),
      sizeof(PCCircumrealityNLPHyp), dwSort ? CCircumrealityNLPHypSortCallback2 : CCircumrealityNLPHypSortCallback);
}


/*************************************************************************************
CCircumrealityNLPParser::HypExpandAll -
   1) This loops through all the hypothesis and expands them all by one level.
   2) Any pre-existing hypothesis are marked as m_fStable so they won't be expanded
      a second time.
   3) The list is sorted and duplicates hypothesis are removed.
   4) If there are no un-stable hypothesis left then returns FALSE to indicate
      that did full expansion.

returns
   BOOL - TRUE if expanded but more can be done, FALSE if expanded as much as possible
*/
BOOL CCircumrealityNLPParser::HypExpandAll (void)
{
   // reset all the starting positions to 0...
   DWORD i, j, k;
   PCCircumrealityNLPHyp *pph = (PCCircumrealityNLPHyp*)m_lPCCircumrealityNLPHyp.Get(0);
   for (i = 0; i < m_lPCCircumrealityNLPHyp.Num(); i++)
      pph[i]->m_dwRuleElem = pph[i]->m_dwRuleGroup = 0;

   // expand them
   DWORD dwHypAtStart = HypNum();
   for (i = 0; i < HypNum(); i++) {
      PCCircumrealityNLPHyp pHyp = HypGet(i);
      if (!pHyp)
         return FALSE;  // error

      // epand this
      HypExpand (pHyp);

      // mark it as stable
      if (i < dwHypAtStart)
         pHyp->m_fStable = TRUE;
   } // i

   // sort so similar items are near one another
   HypSort (0);

   // loop backwards and remove dups
   pph = (PCCircumrealityNLPHyp*)m_lPCCircumrealityNLPHyp.Get(0);
   for (i = m_lPCCircumrealityNLPHyp.Num()-1; i; i--) {
      // only care if they're the same size
      if (pph[i]->m_memTokens.m_dwCurPosn != pph[i-1]->m_memTokens.m_dwCurPosn)
         continue;

      // only care if they're the same
      if (memcmp (pph[i]->m_memTokens.p, pph[i-1]->m_memTokens.p, pph[i]->m_memTokens.m_dwCurPosn))
         continue;

      // they're identical

      // move the highest probability up in the list
      if (pph[i-1]->m_fProb < pph[i]->m_fProb) {
         PCCircumrealityNLPHyp pTemp = pph[i-1];
         pph[i-1] = pph[i];
         pph[i] = pTemp;
      }

      // keep the stability factor or-ed from both of them
      pph[i-1]->m_fStable |= pph[i]->m_fStable;

      // BUGFIX - if the two probabilities are not the same then find all hypothesis derived
      // from the lower probability, and increase their probability, since they
      // could have gotten there by this other rout
      if (pph[i-1]->m_fProb != pph[i]->m_fProb) {
         fp fScale = pph[i-1]->m_fProb / pph[i]->m_fProb;
         DWORD dwLookFor = pph[i]->m_dwID;
         for (j = 0; j < m_lPCCircumrealityNLPHyp.Num(); j++) {
            DWORD *padwParents = (DWORD*) pph[j]->m_lParents.Get(0);
            DWORD dwNum = pph[j]->m_lParents.Num();
            for (k = 0; k < dwNum; k++)
               if (padwParents[k] == dwLookFor) {
                  pph[j]->m_fProb *= fScale;
                  break;   // cant only be parent once
               }
         } // j
      } // if change probability of others

      // remove the last one
      delete pph[i];
      m_lPCCircumrealityNLPHyp.Remove (i);
      pph = (PCCircumrealityNLPHyp*)m_lPCCircumrealityNLPHyp.Get(0);  // reload in case memory moved
   }

   // remove any hypothesis if > 1000
   for (i = m_lPCCircumrealityNLPHyp.Num()-1; i > 1000; i--) {
      // remove one
      delete pph[i];
      m_lPCCircumrealityNLPHyp.Remove (i);
      pph = (PCCircumrealityNLPHyp*)m_lPCCircumrealityNLPHyp.Get(0);  // reload in case memory moved
   }

   // look for non-stable bits
   // will be valid so dont need to do: pph = (PCCircumrealityNLPHyp*)m_lPCCircumrealityNLPHyp.Get(0);
   for (i = 0; i < m_lPCCircumrealityNLPHyp.Num(); i++)
      if (!pph[i]->m_fStable)
         return TRUE;
   return FALSE;  // all stable
}



/*************************************************************************************
CCircumrealityNLPParser::HypTokenGet - Gets the string value of the token in a hypothesis.

inputs
   DWORD          dwToken - From one of they hypothesis
returns
   PWSTR - String. Returns NULL if it's a word break (or somehow unknown, which shouldnt happen)
*/
PWSTR CCircumrealityNLPParser::HypTokenGet (DWORD dwToken)
{
   if (dwToken == (DWORD)-1)
      return NULL;   // word break
   else if (dwToken < m_phTokens->Num())
      return m_phTokens->GetString (dwToken);
   else
      return m_hTokensTemp.GetString (dwToken - m_phTokens->Num());
}



/*************************************************************************************
CCircumrealityNLPParser::Parse - Parses the current string (using the language in LanguageSet()).
All the parses will be stored in the hypothesis; use HypGet() and HypTokenGet() to
get the parse tokens.

inputs
   PWSTR          pszText - Text string
returns
   BOOL - TRUE if success.
*/
BOOL CCircumrealityNLPParser::Parse (PWSTR pszText)
{
   // clear hypothesis
   HypClear();

   // set the language just in case
   LanguageSet (m_lid, m_dwExact);

   // word break
   if (!WordBreak (pszText))
      return FALSE;

   // loop up to 25 times converting tokens
   DWORD i;
   for (i = 0; i < 25; i++)
      if (!HypExpandAll ())
         break;

   // done
   return TRUE;
}




/*************************************************************************************
CCircumrealityNLP::Constructor and destrcutor
*/
CCircumrealityNLP::CCircumrealityNLP (void)
{
   m_hTokens.Init (0, 10000); // start out with a large table
   m_lPCCircumrealityNLPParser.Init (sizeof(PCCircumrealityNLPParser));
   m_dwLastParser = 0;
}

CCircumrealityNLP::~CCircumrealityNLP (void)
{
   // free up all the parsers
   DWORD i;
   PCCircumrealityNLPParser *ppp = (PCCircumrealityNLPParser*)m_lPCCircumrealityNLPParser.Get(0);
   for (i = 0; i < m_lPCCircumrealityNLPParser.Num(); i++)
      delete ppp[i];
   m_lPCCircumrealityNLPParser.Clear();
}



/*************************************************************************************
CCircumrealityNLP::ParserNum - Returns the number of parsers
*/
DWORD CCircumrealityNLP::ParserNum (void)
{
   return m_lPCCircumrealityNLPParser.Num();
}



/*************************************************************************************
CCircumrealityNLP::ParserGet - Gets a parser based on its index. Do NOT delete it from here

inputs
   DWORD       dwIndex - Index
returns
   PCCircumrealityNLPParser - Parser
*/
PCCircumrealityNLPParser CCircumrealityNLP::ParserGet (DWORD dwIndex)
{
   PCCircumrealityNLPParser *ppp = (PCCircumrealityNLPParser*)m_lPCCircumrealityNLPParser.Get(dwIndex);
   return ppp ? ppp[0] : NULL;
}


/*************************************************************************************
CCircumrealityNLP::ParserRemove - Deletes a parser based on its index.

inputs
   DWORD       dwIndex - Index
returns
   BOOL - TRUE if success
*/
BOOL CCircumrealityNLP::ParserRemove (DWORD dwIndex)
{
   PCCircumrealityNLPParser *ppp = (PCCircumrealityNLPParser*)m_lPCCircumrealityNLPParser.Get(dwIndex);
   if (!ppp)
      return FALSE;
   delete ppp[0];
   m_lPCCircumrealityNLPParser.Remove (dwIndex);
   return TRUE;
}


/*************************************************************************************
CCircumrealityNLP::ParserFind - Finds a perser based on its name.

inputs
   PWSTR          pszName - Name. Cases insesative
returns
   DWORD - Index, or -1 if cant find
*/
DWORD CCircumrealityNLP::ParserFind (PWSTR pszName)
{
   PCCircumrealityNLPParser *ppp = (PCCircumrealityNLPParser*)m_lPCCircumrealityNLPParser.Get(m_dwLastParser);
   if (ppp && !_wcsicmp((PWSTR) ppp[0]->m_memName.p, pszName))
      return m_dwLastParser;  // quick match

   // else, slow search
   ppp = (PCCircumrealityNLPParser*)m_lPCCircumrealityNLPParser.Get(0);
   DWORD i;
   for (i = 0; i < m_lPCCircumrealityNLPParser.Num(); i++)
      if (!_wcsicmp((PWSTR) ppp[i]->m_memName.p, pszName)) {
         m_dwLastParser = i;
         return i;
      }

   // else, cant find
   return -1;
}


/*************************************************************************************
CCircumrealityNLP::ParserAdd - Adds a new parser.

inputs
   PWSTR          pszName - Name to  use. NOTE: This does NOT check to see ifthe name
                  is unique
returns
   DWORD - Index to the parser, or -1 if error
*/
DWORD CCircumrealityNLP::ParserAdd (PWSTR pszName)
{
   PCCircumrealityNLPParser pNew = new CCircumrealityNLPParser;
   if (!pNew)
      return (DWORD)-1;
   if (!pNew->Init (&m_hTokens)) {
      delete pNew;
      return (DWORD)-1;
   }
   MemZero (&pNew->m_memName);
   MemCat (&pNew->m_memName, pszName);

   return m_lPCCircumrealityNLPParser.Add (&pNew);
}



/*************************************************************************************
CCircumrealityNLP::ParserClone - Clones the parser at the given index and then adds it.

inputs
   DWORD          dwIndex - Index of parser
returns
   DOWRD - Index of added one, or -1 if error
*/
DWORD CCircumrealityNLP::ParserClone (DWORD dwIndex)
{
   PCCircumrealityNLPParser pNew = ParserGet (dwIndex);
   if (!pNew)
      return -1;
   pNew = pNew->Clone();
   if (!pNew)
      return -1;

   return m_lPCCircumrealityNLPParser.Add (&pNew);
}





/*************************************************************************
CircumrealityNLPPage
*/
BOOL CircumrealityNLPPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PNMD pnmd = (PNMD)pPage->m_pUserData;
   PCCircumrealityNLPParser pParser = pnmd->pParser;
   PCCircumrealityNLPRuleSet pRuleSet = pnmd->pRuleSet;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         // scroll to right position
         if (pnmd->iVScroll > 0) {
            pPage->VScroll (pnmd->iVScroll);

            // when bring up pop-up dialog often they're scrolled wrong because
            // iVScoll was left as valeu, and they use defpage
            pnmd->iVScroll = 0;

            // BUGFIX - putting this invalidate in to hopefully fix a refresh
            // problem when add or move a task in the ProjectView page
            pPage->Invalidate();
         }

         // set a probability
         DoubleToControl (pPage, L"prob", 50);

         // short sort by
         ComboBoxSet (pPage, L"sortby", pnmd->dwSortBy);

         // check the boxes
         DWORD i;
         PCEscControl pControl;
         WCHAR szTemp[64];
         for (i = 0; i < pParser->RuleSetNum(); i++) {
            PCCircumrealityNLPRuleSet pSet = pParser->RuleSetGet (i);
            if (!pSet)
               continue;

            swprintf (szTemp, L"turnon%d", (int)i);
            pControl = pPage->ControlFind (szTemp);
            if (pControl)
               pControl->AttribSetBOOL (Checked(), pSet->m_fEnabled);
         } // i
            

         // refresh the display
         pPage->Message (ESCM_USER+102);  // refresh image
      }
      break;

   case ESCN_COMBOBOXSELCHANGE:
      {
         PESCNCOMBOBOXSELCHANGE p = (PESCNCOMBOBOXSELCHANGE) pParam;
         PWSTR psz = p->pControl->m_pszName;
         if (!psz)
            break;

         else if (!_wcsicmp(psz, L"sortby")) {
            DWORD dwVal = p->pszName ? _wtoi(p->pszName) : 0;
            if (dwVal == pnmd->dwSortBy)
               return TRUE;

            pnmd->dwSortBy = dwVal;

            // refresh
            pPage->Message (ESCM_USER+102);
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

         PWSTR pszTurnOn = L"turnon";
         DWORD dwTurnOnLen = (DWORD)wcslen(pszTurnOn);

         if (!wcsncmp (psz, pszTurnOn, dwTurnOnLen)) {
            DWORD dwNum = _wtoi(psz + dwTurnOnLen);
            PCCircumrealityNLPRuleSet pSet = pParser->RuleSetGet (dwNum);
            if (pSet)
               pSet->m_fEnabled = TRUE;
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"test")) {
            PCEscControl pControl = pPage->ControlFind (L"testtext");
            if (!pControl)
               return TRUE;
            WCHAR szTemp[512];
            DWORD dwNeed;
            szTemp[0] = 0;
            pControl->AttribGet (Text(), szTemp, sizeof(szTemp), &dwNeed);

            pParser->Parse (szTemp);

            // play sound so knows that worked
            EscChime (ESCCHIME_INFORMATION);
            pPage->Message (ESCM_USER+102);  // redraw
            return TRUE;
         }
         else if (!_wcsicmp (psz, L"delrule")) {
            PCEscControl pControl = pPage->ControlFind (L"rulelist");
            if (!pControl)
               return TRUE;
            int iSel = pControl->AttribGetInt (CurSel());

            ESCMLISTBOXGETITEM lbi;
            memset (&lbi, 0, sizeof(lbi));
            lbi.dwIndex = (DWORD)iSel;
            pControl->Message (ESCM_LISTBOXGETITEM, &lbi);
            if (!lbi.pszName)
               return TRUE;   // shouldnt happen

            DWORD dwIndex = _wtoi(lbi.pszName);

            if (IDYES != pPage->MBYesNo (L"Are you sure you wish to delete the selected rule?"))
               return TRUE;

            pRuleSet->RuleRemove (dwIndex);

            // play sound so knows that worked
            pnmd->fChanged = TRUE;
            EscChime (ESCCHIME_INFORMATION);
            pPage->Message (ESCM_USER+102);  // redraw
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"addrule")) {
            // get all the strings
#define NUMSTRINGS      5
            DWORD i;
            PCEscControl pControl;
            WCHAR szTemp[64];
            WCHAR aszString[NUMSTRINGS][256];
            DWORD dwNeed;
            for (i = 0; i < NUMSTRINGS; i++) {
               aszString[i][0] = 0;
               if (i >= 1)
                  swprintf (szTemp, L"to%d", (int)i-1);
               else
                  wcscpy (szTemp, L"from");

               pControl = pPage->ControlFind (szTemp);
               if (!pControl)
                  continue;
               pControl->AttribGet (Text(), aszString[i], sizeof(aszString[i]), &dwNeed);
            } // i

            // if nothing in the from line then error
            if (!aszString[0][0]) {
               pPage->MBWarning (L"You must type something into the \"from\" line.");
               return TRUE;
            }
            
            // the other strings can be blank

            // probability
            fp fProb = DoubleFromControl (pPage, L"prob");
            fProb /= 100.0;
            if ((fProb < 0.00001) || (fProb > 0.991)) {
               pPage->MBWarning (L"The probability must be between 0.01 and 99 (percent).");
               return TRUE;
            }

            // create a new one
            DWORD dwRule = pRuleSet->RuleAdd ();
            PCCircumrealityNLPRule pRule = pRuleSet->RuleGet(dwRule);
            if (!pRule)
               return TRUE;   // error
            pRule->m_dwType = pnmd->dwTab;   // 0..2
            pRule->m_fProb = fProb;
            pRule->m_lCFGString.Clear();
            for (i = 0; i < NUMSTRINGS; i++)
               if (aszString[i][0] || (i < 1))
                  pRule->m_lCFGString.Add (aszString[i], (wcslen(aszString[i])+1)*sizeof(WCHAR));
            if (pRule->m_lCFGString.Num() < 2)  // add a blank string if none
               pRule->m_lCFGString.Add (aszString[1], (wcslen(aszString[1])+1)*sizeof(WCHAR));

            if (!pRule->CalcCFG()) {
               // error
               pRuleSet->RuleRemove (dwRule);

               pPage->MBWarning (L"The rules have an error in them.",
                  L"You may have mismatched parenthsis, brackets, or other errors in the grammar. See the documentation below.");

               return TRUE;
            }

            // wipe out the old rules
            for (i = 0; i < NUMSTRINGS; i++) {
               aszString[i][0] = 0;
               if (i >= 1)
                  swprintf (szTemp, L"to%d", (int)i-1);
               else
                  wcscpy (szTemp, L"from");
               pControl = pPage->ControlFind (szTemp);
               if (pControl)
                  pControl->AttribSet (Text(), L"");
            }

            // play sound so knows that worked
            pnmd->fChanged = TRUE;
            EscChime (ESCCHIME_INFORMATION);
            pPage->Message (ESCM_USER+102);  // redraw
            return TRUE;
         }
      }
      break;

   case ESCM_USER+102:  // called to indicate that should redraw
      {
         PCEscControl pControl = pPage->ControlFind ((pnmd->dwTab < 3) ? L"rulelist" : L"testresults");
         if (!pControl)
            return TRUE;

         // wipe out
         pControl->Message (ESCM_LISTBOXRESETCONTENT);

         // elements
         CMem mem;
         MemZero (&mem);
         DWORD i, j, dwNum;
         PWSTR psz;
         if (pnmd->dwTab < 3) {  // rule set
            // sort, just in case
            pRuleSet->RuleSort (pnmd->dwSortBy);

            for (i = 0; i < pRuleSet->RuleNum(); i++) {
               PCCircumrealityNLPRule pRule = pRuleSet->RuleGet (i);
               if (!pRule)
                  continue;
               if (pRule->m_dwType != pnmd->dwTab)
                  continue;   // only care about ones of a specific type

               MemCat (&mem, L"<elem name=");
               MemCat (&mem, (int)i);
               MemCat (&mem, L"><align wrapindent=32>");

               psz = (PWSTR) pRule->m_lCFGString.Get(0);
               if (!psz)
                  psz = L"Unknown";

               MemCat (&mem, L"<bold><font color=#800000>");
               MemCatSanitize (&mem, psz);
               MemCat (&mem, L"</font></bold> to ");

               dwNum = pRule->m_lCFGString.Num();
               if (pnmd->dwTab != 2) // only allow multiple if split
                  dwNum = min(dwNum, 2);
               for (j = 1; j < dwNum; j++) {
                  psz = (PWSTR) pRule->m_lCFGString.Get(j);
                  if (!psz)
                     continue;

                  if (j > 1)
                     MemCat (&mem, L", ");

                  MemCat (&mem, L"<bold><font color=#008000>");
                  MemCatSanitize (&mem, psz);
                  MemCat (&mem, L"</font></bold>");
               } // j

               // show probability
               WCHAR szTemp[32];
               swprintf (szTemp, L" (%g%%)", (double)pRule->m_fProb * 100.0);
               MemCat (&mem, szTemp);

               MemCat (&mem, L"</align></elem>");
            } // i
         }
         else {   // test results
            pParser->HypSort (1);

            for (i = 0; i < pParser->HypNum(); i++) {
               PCCircumrealityNLPHyp pHyp = pParser->HypGet (i);
               if (!pHyp)
                  continue;

               DWORD *padw = (DWORD*)pHyp->m_memTokens.p;
               DWORD dwNum = (DWORD)pHyp->m_memTokens.m_dwCurPosn / sizeof(DWORD);
               MemCat (&mem, L"<elem name=");
               MemCat (&mem, (int)i);
               MemCat (&mem, L"><align wrapindent=32>");

               MemCat (&mem, L"<bold>");
               for (j = 0; j < dwNum; j++) {
                  PWSTR psz = pParser->HypTokenGet (padw[j]);
                  if (!psz)
                     psz = L". ";
                  else if (j)
                     MemCat (&mem, L" ");
                  MemCatSanitize (&mem, psz);
               } // j
               MemCat (&mem, L"</bold>");

               // show probability
               WCHAR szTemp[32];
               swprintf (szTemp, L" (%g%%)", (double)pHyp->m_fProb * 100.0);
               MemCat (&mem, szTemp);

               MemCat (&mem, L"</align></elem>");
            } // i
         }

         // write
         ESCMLISTBOXADD ca;
         memset (&ca, 0, sizeof(ca));
         ca.dwInsertBefore = -1;
         ca.pszMML = (PWSTR)mem.p;
         pControl->Message (ESCM_LISTBOXADD, &ca);

         // select
         pControl->AttribSetInt (CurSel(), 0);
      }
      return TRUE;

   case ESCM_LINK:
      {
         PESCMLINK p = (PESCMLINK) pParam;
         if (!p->psz)
            break;

         PWSTR pszTab = L"tabpress:";
         DWORD dwLen = (DWORD)wcslen(pszTab);

         if (!wcsncmp(p->psz, pszTab, dwLen)) {
            pnmd->dwTab = (DWORD)_wtoi(p->psz + dwLen);
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
            if (dwNum == pnmd->dwTab)
               p->pszSubString = L"";
            else
               p->pszSubString = L"<comment>";
            return TRUE;
         }
         else if (!wcsncmp (p->pszSubName, pszEndIfTab, dwEndIfTabLen)) {
            DWORD dwNum = _wtoi(p->pszSubName + dwEndIfTabLen);
            if (dwNum == pnmd->dwTab)
               p->pszSubString = L"";
            else
               p->pszSubString = L"</comment>";
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"RSTABS")) {
            PWSTR apsz[] = {
               L"Synonym",
               L"Reword",
               L"Split",
               L"Test",
            };
            PWSTR apszHelp[] = {
               L"Add or remove synonyms from the rules.",
               L"Add or remove sentence rewording rules.",
               L"Add or remove sentence splitting rules.",
               L"Test your rules",
            };
            DWORD adwID[] = {
               0, // syn
               1, // reword
               2, // split
               3, // test
            };

            CListFixed lSkip;
            lSkip.Init (sizeof(DWORD));

            p->pszSubString = RenderSceneTabs (pnmd->dwTab, sizeof(apsz)/sizeof(PWSTR), apsz,
               apszHelp, adwID, lSkip.Num(), (DWORD*)lSkip.Get(0));
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"NLP rule set";
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"LIBENABLE")) {
            if (pnmd && pnmd->fReadOnly)
               p->pszSubString = L"enabled=false";
            else
               p->pszSubString = L"";
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"LIBREADONLY")) {
            if (pnmd && pnmd->fReadOnly)
               p->pszSubString = L"readonly=true";
            else
               p->pszSubString = L"";
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"RULESONOFF")) {
            MemZero (&gMemTemp);

            DWORD i;
            for (i = 0; i < pParser->RuleSetNum(); i++) {
               PCCircumrealityNLPRuleSet pSet = pParser->RuleSetGet (i);
               if (!pSet || !_wcsicmp((PWSTR) pSet->m_memName.p, gpszEditedName))
                  continue;

               // add option
               MemCat (&gMemTemp, L"<xChoiceButton valign=center style=x checkbox=true name=turnon");
               MemCat (&gMemTemp, (int)i);
               MemCat (&gMemTemp, L"><bold>");
               MemCatSanitize (&gMemTemp, (PWSTR)pSet->m_memName.p);
               MemCat (&gMemTemp, L"</bold></xChoiceButton>");
            } // i
            
            p->pszSubString = (PWSTR)gMemTemp.p;
            return TRUE;
         }
      }
      break;

   }; // dwMessage

   return FALSE;
}


/*************************************************************************
NLPRuleSetEdit - Modify a NLPRuleSet info. Uses standard API from mifl.h, ResourceEdit.
*/
PCMMLNode2 NLPRuleSetEdit (HWND hWnd, LANGID lid, PCMMLNode2 pIn, BOOL fReadOnly, PCMIFLProj pProj)
{
   // load in the rule set
   CCircumrealityNLP nlp;
   DWORD dwParser = nlp.ParserAdd (gpszEditedName);
   PCCircumrealityNLPParser pParser = nlp.ParserGet (dwParser);
   if (!pParser)
      return NULL;
   pParser->LanguageSet (lid, 0);   // set the language
   DWORD dwRuleSet = pParser->RuleSetAdd (gpszEditedName);
   PCCircumrealityNLPRuleSet pRuleSet = pParser->RuleSetGet (dwRuleSet);
   if (!pRuleSet)
      return NULL;
   if (!pRuleSet->MMLFrom (pIn, lid))
      return NULL;

   // load in all rule sets in the project so that
   // provide user the choice of which ones to activate
   DWORD i, j;
   if (pProj) for (i = 0; i < pProj->LibraryNum(); i++) {
      PCMIFLLib pLib = pProj->LibraryGet(i);
      if (!pLib)
         continue;

      for (j = 0; j < pLib->ResourceNum(); j++) {
         PCMIFLResource pRes = pLib->ResourceGet (j);
         if (!pRes)
            continue;
         if (_wcsicmp((PWSTR)pRes->m_memType.p, CircumrealityNLPRuleSet()))
            continue;   // no match

         // else found, so try to find closest language...
         PCMMLNode2 pNode = pRes->Get (lid);
         if (!pNode)
            continue;

         // see if already exists
         if (-1 != pParser->RuleSetFind((PWSTR)pRes->m_memName.p))
            continue;   // exists, but overridden

         // add
         DWORD dwIndex = pParser->RuleSetAdd ((PWSTR)pRes->m_memName.p);
         PCCircumrealityNLPRuleSet pSet = pParser->RuleSetGet (dwIndex);
         if (!pSet)
            continue;
         pSet->MMLFrom (pNode, lid);
         pSet->LanguageSet (lid, 0);   // just in case
         pSet->m_fEnabled = FALSE;  // start up disabled so can turn on
      } // j
   } // i, proj

   PCMMLNode2 pRet = NULL;
   NMD nmd;
   memset (&nmd, 0, sizeof(nmd));
   nmd.fReadOnly = fReadOnly;
   nmd.lid = lid;
   nmd.pParser = pParser;
   nmd.pRuleSet = pRuleSet;

   // create the window
   RECT r;
   PWSTR psz;
   DialogBoxLocation3 (GetDesktopWindow(), &r, TRUE);
   CEscWindow Window;
   Window.Init (ghInstance, hWnd, 0, &r);
redo:
   psz = Window.PageDialog (ghInstance, IDR_MMLRESNLPRULESET, CircumrealityNLPPage, &nmd);
   nmd.iVScroll = Window.m_iExitVScroll;
   if (psz && !_wcsicmp(psz, RedoSamePage()))
      goto redo;

   if (!nmd.fChanged)
      goto done;

   // save it out
   PCMMLNode2 pNode = pRuleSet->MMLTo ();
   if (!pNode)
      goto done;
   // don't need this, since already done with RuleSet::MMLTo(), pNode->NameSet (CircumrealityNLPRuleSet());

   pRet = pNode;

done:
   return pRet;
}


/*************************************************************************************
NLPNounCaseParseToken - Given a token, figures out the meaning.

inputs
   PWSTR          psz - Token string
   DWORD          dwLen - Number of characters to look at. 1 or more
   DWORD          *pdwID1 - Filled with the main meaning, a NLP_XXX_YYY, where XXX matches the return
   DWORD          *pdwID2 - In some cases, a second valid ID. If not, 0
returns
   DWORD - Type of info, a NLPNC_XXX_MASK setting, or 0 if none
*/
DWORD NLPNounCaseParseToken (PWSTR psz, DWORD dwLen, DWORD *pdwID1, DWORD *pdwID2)
{
   PWSTR pszCheck = NULL;
   DWORD dwRet;
   *pdwID2 = 0;   // just in case

   switch (towlower (psz[0])) {
   case L'a':
      if (dwLen >= 2) switch (towlower (psz[1])) {
      case L'b':
         if (dwLen >= 3) switch (tolower(psz[2])) {
         case L'e':
            // abessive
            pszCheck = L"abessive";
            *pdwID1 = NLPNC_CASE_ABESSIVE;
            dwRet = NLPNC_CASE_MASK;
            break;
         case L'l':
            // ablative
            pszCheck = L"ablative";
            *pdwID1 = NLPNC_CASE_ABLATIVE;
            dwRet = NLPNC_CASE_MASK;
            break;
         case L's':
            // absolutive
            pszCheck = L"absolutive";
            *pdwID1 = NLPNC_CASE_ABSOLUTIVE;
            dwRet = NLPNC_CASE_MASK;
            break;
         } // switch
         break;

      case L'c':
         // accurative
         pszCheck = L"accurative";
         *pdwID1 = NLPNC_CASE_ACCUSATIVE;
         dwRet = NLPNC_CASE_MASK;
         break;

      case L'd':
         // adessive
         pszCheck = L"adessive";
         *pdwID1 = NLPNC_CASE_ADESSIVE;
         dwRet = NLPNC_CASE_MASK;
         break;

      case L'l':
         // allative
         pszCheck = L"allative";
         *pdwID1 = NLPNC_CASE_ALLATIVE;
         dwRet = NLPNC_CASE_MASK;
         break;

      case L'n':
         // animate
         pszCheck = L"animate";
         *pdwID1 = NLPNC_ANIM_ANIMATE;
         dwRet = NLPNC_ANIM_MASK;
         break;
      } // switch
      break;

   case L'c':
      // comitative
      pszCheck = L"comitative";
      *pdwID1 = NLPNC_CASE_COMITATIVE;
      dwRet = NLPNC_CASE_MASK;
      break;

   case L'd':
      if (dwLen >= 2) switch (towlower(psz[1])) {
      case L'a':
         // dative
         pszCheck = L"dative";
         *pdwID1 = NLPNC_CASE_DATIVE;
         dwRet = NLPNC_CASE_MASK;
         break;
      case L'e':
         if (dwLen >= 3) switch (towlower(psz[2])) {
         case L'd':
            // dedative
            pszCheck = L"dedative";
            *pdwID1 = NLPNC_CASE_DEDATIVE;
            dwRet = NLPNC_CASE_MASK;
            break;
         case L'f':
            // definitearticle
            pszCheck = L"definitearticle";
            *pdwID1 = NLPNC_ART_DEFINITE;
            dwRet = NLPNC_ART_MASK;
            break;
         }
         break;
      }
      break;

   case L'e':
      if (dwLen >= 2) switch (towlower(psz[1])) {
      case L'l':
         // elative
         pszCheck = L"elative";
         *pdwID1 = NLPNC_CASE_ELATIVE;
         dwRet = NLPNC_CASE_MASK;
         break;
      case L'r':
         // ergative
         pszCheck = L"ergative";
         *pdwID1 = NLPNC_CASE_ERGATIVE;
         dwRet = NLPNC_CASE_MASK;
         break;
      case L's':
         // essive
         pszCheck = L"essive";
         *pdwID1 = NLPNC_CASE_ESSIVE;
         dwRet = NLPNC_CASE_MASK;
         break;
      }
      break;

   case L'f':
      if (dwLen >= 2) switch (towlower(psz[1])) {
      case L'a':
         // familiar
         pszCheck = L"familiar";
         *pdwID1 = NLPNC_FAM_FAMILIAR;
         dwRet = NLPNC_FAM_MASK;
         break;
      case L'e':
         if (dwLen >= 3) switch (towlower(psz[2])) {
         case L'w':
            // few
            pszCheck = L"few";
            *pdwID1 = NLPNC_COUNT_FEW;
            dwRet = NLPNC_COUNT_MASK;
            break;
         case L'm':
            // female
            pszCheck = L"female";
            *pdwID1 = NLPNC_GENDER_FEMALE;
            dwRet = NLPNC_GENDER_MASK;
            break;
         }
         break;
      case L'i':
         // first
         pszCheck = L"first";
         *pdwID1 = NLPNC_PERSON_FIRST;
         dwRet = NLPNC_PERSON_MASK;
         break;
      case L'o':
         // formal
         pszCheck = L"formal";
         *pdwID1 = NLPNC_FAM_FORMAL;
         dwRet = NLPNC_FAM_MASK;
         break;
      } // switch
      break;

   case L'g':
      // genitive
      pszCheck = L"genitive";
      *pdwID1 = NLPNC_CASE_GENITIVE;
      dwRet = NLPNC_CASE_MASK;
      break;

   case L'i':
      if (dwLen >= 2) switch (towlower(psz[1])) {
      case L'l':
         // illative
         pszCheck = L"illative";
         *pdwID1 = NLPNC_CASE_ILLATIVE;
         dwRet = NLPNC_CASE_MASK;
         break;
      case L'n':
         if (dwLen >= 3) switch (towlower(psz[2])) {
         case L'a':
            // inanimate
            pszCheck = L"inanimate";
            *pdwID1 = NLPNC_ANIM_INANIMATE;
            dwRet = NLPNC_ANIM_MASK;
            break;
         case L'd':
            // indefinitearticle
            pszCheck = L"indefinitearticle";
            *pdwID1 = NLPNC_ART_INDEFINITE;
            dwRet = NLPNC_ART_MASK;
            break;
         case L'e':
            // inessive
            pszCheck = L"inessive";
            *pdwID1 = NLPNC_CASE_INESSIVE;
            dwRet = NLPNC_CASE_MASK;
            break;
         case L's':
            // instructive
            // instrumental
            pszCheck = L"instrumental"; // note: dont worry about instuctive, since same root for 5 chars
            *pdwID1 = NLPNC_CASE_INSTRUMENTAL;
            dwRet = NLPNC_CASE_MASK;
            break;
         } // switch
         break;
      } // switch
      break;

   case L'l':
      if (dwLen >= 2) switch (towlower(psz[1])) {
      case L'o':
         if (dwLen >= 3) switch (towlower(psz[2])) {
         case L'c':
            // locative
            pszCheck = L"locative";
            *pdwID1 = NLPNC_CASE_LOCATIVE;
            dwRet = NLPNC_CASE_MASK;
            break;
         case L'o':
            // long
            pszCheck = L"long";
            *pdwID1 = NLPNC_VERBOSE_LONG;
            dwRet = NLPNC_VERBOSE_MASK;
            break;
         case L'w':
            // lower
            pszCheck = L"lower";
            *pdwID1 = NLPNC_CAPS_LOWER;
            dwRet = NLPNC_CAPS_MASK;
            break;
         }
         break;
      }
      break;

   case L'm':
      if (dwLen >= 2) switch (towlower(psz[1])) {
      case L'a':
         if (dwLen >= 3) switch (towlower(psz[2])) {
         case L'n':
            // many
            pszCheck = L"many";
            *pdwID1 = NLPNC_COUNT_MANY;
            dwRet = NLPNC_COUNT_MASK;
            break;
         case L'l':
            // male
            pszCheck = L"male";
            *pdwID1 = NLPNC_GENDER_MALE;
            dwRet = NLPNC_GENDER_MASK;
            break;
         }
         break;
      }
      break;

   case L'n':
      if (dwLen >= 2) switch (towlower(psz[1])) {
      case L'e':
         // male
         pszCheck = L"neuter";
         *pdwID1 = NLPNC_GENDER_NEUTER;
         dwRet = NLPNC_GENDER_MASK;
         break;
      case L'o':
         if (dwLen >= 3) switch (towlower(psz[2])) {
         case L'a':
            // noarticle
            pszCheck = L"noarticle";
            *pdwID1 = NLPNC_ART_NONE;
            dwRet = NLPNC_ART_MASK;
            break;
         case L'm':
            // nominative
            pszCheck = L"nominative";
            *pdwID1 = NLPNC_CASE_NOMINATIVE;
            dwRet = NLPNC_CASE_MASK;
            break;
         case L'q':
            // noquantity
            pszCheck = L"noquantity";
            *pdwID1 = NLPNC_QUANTITY_NOQUANTITY;
            dwRet = NLPNC_QUANTITY_MASK;
            break;
         }
         break;
      }
      break;

   case L'o':
      if (dwLen >= 2) switch (towlower(psz[1])) {
      case L'b':
         if (dwLen >= 3) switch (towlower(psz[2])) {
         case L'j':
            // objective
            pszCheck = L"objective";
            *pdwID1 = NLPNC_CASE_ACCUSATIVE;
            *pdwID2 = NLPNC_CASE_DATIVE;
            dwRet = NLPNC_CASE_MASK;
            break;
         case L'l':
            // oblique
            pszCheck = L"oblique";
            *pdwID1 = NLPNC_CASE_OBLIQUE;
            dwRet = NLPNC_CASE_MASK;
            break;
         }
         break;
      }
      break;

   case L'p':
      if (dwLen >= 2) switch (towlower(psz[1])) {
      case L'a':
         // partitive
         pszCheck = L"partitive";
         *pdwID1 = NLPNC_CASE_PARTITIVE;
         dwRet = NLPNC_CASE_MASK;
         break;
      case L'l':
         // plural
         pszCheck = L"plural";
         *pdwID1 = NLPNC_COUNT_FEW;
         *pdwID2 = NLPNC_COUNT_MANY;
         dwRet = NLPNC_COUNT_MASK;
         break;
      case L'o':
         if (dwLen >= 3) switch (towlower(psz[2])) {
         case L's':
            if (dwLen >= 4) switch (towlower(psz[3])) {
            case L's':
               // possessive
               pszCheck = L"possessive";
               *pdwID1 = NLPNC_CASE_POSSESSIVE;
               dwRet = NLPNC_CASE_MASK;
               break;
            case L't':
               // postpositional
               pszCheck = L"postpositional";
               *pdwID1 = NLPNC_CASE_POSTPOSITIONAL;
               dwRet = NLPNC_CASE_MASK;
               break;
            }
            break;
         }
         break;
      case L'r':
         if (dwLen >= 3) switch (towlower(psz[2])) {
         case L'e':
            // prepositional
            pszCheck = L"prepositional";
            *pdwID1 = NLPNC_CASE_PREPOSITIONAL;
            dwRet = NLPNC_CASE_MASK;
            break;
         case L'o':
            // prolative
            pszCheck = L"prolative";
            *pdwID1 = NLPNC_CASE_PROLATIVE;
            dwRet = NLPNC_CASE_MASK;
            break;
         }
         break;
      }
      break;

   case L'q':
      // quantity
      pszCheck = L"quantity";
      *pdwID1 = NLPNC_QUANTITY_QUANTITY;
      dwRet = NLPNC_QUANTITY_MASK;
      break;

   case L's':
      if (dwLen >= 2) switch (towlower(psz[1])) {
      case L'e':
         // first
         pszCheck = L"second";
         *pdwID1 = NLPNC_PERSON_SECOND;
         dwRet = NLPNC_PERSON_MASK;
         break;
      case L'h':
         // short
         pszCheck = L"short";
         *pdwID1 = NLPNC_VERBOSE_SHORT;
         dwRet = NLPNC_VERBOSE_MASK;
         break;
      case L'i':
         // single
         pszCheck = L"single";
         *pdwID1 = NLPNC_COUNT_SINGLE;
         dwRet = NLPNC_COUNT_MASK;
         break;
      case L'u':
         // subjective
         pszCheck = L"subjective";
         *pdwID1 = NLPNC_CASE_NOMINATIVE; // on purpose, since english has subjective
         dwRet = NLPNC_CASE_MASK;
         break;
      }
      break;

   case L't':
      if (dwLen >= 2) switch (towlower(psz[1])) {
      case L'e':
         // terminative
         pszCheck = L"terminative";
         *pdwID1 = NLPNC_CASE_TERMINATIVE;
         dwRet = NLPNC_CASE_MASK;
         break;
      case L'h':
         // third
         pszCheck = L"third";
         *pdwID1 = NLPNC_PERSON_THIRD;
         dwRet = NLPNC_PERSON_MASK;
         break;
      case L'r':
         // translative
         pszCheck = L"translative";
         *pdwID1 = NLPNC_CASE_TRANSLATIVE;
         dwRet = NLPNC_CASE_MASK;
         break;
      }
      break;

   case L'u':
      // upper
      pszCheck = L"upper";
      *pdwID1 = NLPNC_CAPS_UPPER;
      dwRet = NLPNC_CAPS_MASK;
      break;

   case L'v':
      // vocative
      pszCheck = L"vocative";
      *pdwID1 = NLPNC_CASE_VOCATIVE;
      dwRet = NLPNC_CASE_MASK;
      break;

   } // swtich

   // double-check an exact match
   if (pszCheck && !_wcsnicmp(psz, pszCheck, dwLen))
      return dwRet;

   // else
   return 0;   // cant parse
}



/*************************************************************************************
NLPNounCaseSkipSection - Skips, counting the number of quotes.

inputs
   PWSTR          pszParse - Where to start from
   BOOL           fStopAtColon - If TRUE then stops at a colon
returns
   PWSTR - New location
*/
PWSTR NLPNounCaseSkipSection (PWSTR pszParse, BOOL fStopAtColon)
{
   DWORD dwParen = 0;
   for (; *pszParse; pszParse++) {
      if (pszParse[0] == L'(') {
         dwParen++;
         continue;
      }
      else if (pszParse[0] == L')') {
         // if no paren left than done
         if (!dwParen)
            return pszParse;
         dwParen--;
         continue;
      }
      else if ((pszParse[0] == L':') && !dwParen && fStopAtColon)
         // stop at a colon only if not in parent, and have flag set
         return pszParse;
   } // while


   return pszParse;
}


/*************************************************************************************
NLPNounCaseInternal - This parses a noun-case string, identifying noun-case tests
(with "X?Y:Z" and parenthesis), and fills in the parse string based upon the
given dwNounCase settings.

inputs
   PWSTR          pszParse - String to parse. This will parse until an ')' or EOL is reached.
   PCMem          pmParsed - This has the parsed characters added to it using pmParse->CharCat()
   DWORD          dwNounCase - Flags specifying noun-case information. See NLPNC_XXX
   BOOL           fAllowTest - If TRUE then allows a test '?'.
   BOOL           fAllowColon - If TRUE then allows a colon ':'
returns
   PWSTR - New location for pszParse
*/
PWSTR NLPNounCaseInternal (PWSTR pszParse, PCMem pmParsed, DWORD dwNounCase, BOOL fAllowTest,
                           BOOL fAllowColon)
{
thebeginning:
   // loop ahead until find a character that don't like
   PWSTR pszCur;
   for (pszCur = pszParse; *pszCur; pszCur++)
      if ((fAllowTest && (*pszCur == L'?')) || (fAllowColon && (*pszCur == L':')) ||
         (*pszCur == L'(') || (*pszCur == L')'))
         break;
   DWORD dwLen = (DWORD)((PBYTE)pszCur - (PBYTE)pszParse)/sizeof(WCHAR);

   // depending upon what's next...
   switch (*pszCur) {
   case 0:
      // came to an end of sting without special symbols, so must be all
      if (dwLen)
         pmParsed->StrCat (pszParse, dwLen);
      return pszCur;

   case L'(':
      // found quote, so cant be a test... just parse in quote and continue
      if (dwLen)
         pmParsed->StrCat (pszParse, dwLen); // add stuff before quote

      // parse through quote
      pszCur = NLPNounCaseInternal (pszCur+1, pmParsed, dwNounCase, TRUE, FALSE);
      if (*pszCur == L')')
         pszCur++;   // go past last quote
     
      // repeat
      pszParse = pszCur;
      goto thebeginning;

   case L')':
   case L':':
      // got to the end..
      if (dwLen)
         pmParsed->StrCat (pszParse, dwLen); // add stuff before quote
      return pszCur;
   }

   // else, got a '?', which means a test
   DWORD dwMask, dwTest1, dwTest2;
   BOOL fFirst = FALSE, fSecond = FALSE;
   dwMask = NLPNounCaseParseToken (pszParse, dwLen, &dwTest1, &dwTest2);
   if (dwMask) {
      DWORD dwVal = dwNounCase & dwMask;
      if ( (dwTest1 == dwVal) || (dwTest2 && (dwTest2 == dwVal)) )
         fFirst = TRUE;
      else
         fSecond = TRUE;
   }

   // go past the '?'
   pszCur++;

   // get or skip the first statment
   if (fFirst)
      pszCur = NLPNounCaseInternal (pszCur, pmParsed, dwNounCase, FALSE, TRUE);
   else
      pszCur = NLPNounCaseSkipSection(pszCur, TRUE);
   if (*pszCur == L':') {
      pszCur++;
      if (fSecond)
         pszCur = NLPNounCaseInternal (pszCur, pmParsed, dwNounCase, FALSE, FALSE);
      else
         pszCur = NLPNounCaseSkipSection (pszCur, FALSE);
   }

   // either got to end of the paren, end of line, or a colon
   return pszCur;
}


/*************************************************************************************
NLPNounCase - This parses a noun-case string, identifying noun-case tests
(with "X?Y:Z" and parenthesis), and fills in the parse string based upon the
given dwNounCase settings.

NOTE: The strings are the shortest unique length of the noun differentiator,
like "plu" for "plural".

NOTE: This looks at the caps setting and capitalizes the string automatically.

inputs
   PWSTR          pszParse - String to parse.
   PCMem          pmParsed - This is filled with the parsed string, NULL-terminated
   DWORD          dwNounCase - Flags specifying noun-case information. See NLPNC_XXX
                     NOTE: This is modified so there's always a setting for the case
                     sub-types
   BOOL           fAppendNounCase - If TRUE then append a '{' followed by itow(dwNounCase),
                     followed by '}', so this is easily passed into NLPVerbForm().
returns
   PWSTR - pmParsed->p
*/
PWSTR NLPNounCase (PWSTR pszParse, PCMem pmParsed, DWORD dwNounCase, BOOL fAppendNounCase)
{
   // make sure there is a setting
   if (!(dwNounCase & NLPNC_ART_MASK))
      dwNounCase |= NLPNC_ART_NONE;
   if (!(dwNounCase & NLPNC_COUNT_MASK))
      dwNounCase |= NLPNC_COUNT_SINGLE;
   if (!(dwNounCase & NLPNC_ANIM_MASK))
      dwNounCase |= NLPNC_ANIM_INANIMATE;
   if (!(dwNounCase & NLPNC_FAM_MASK))
      dwNounCase |= NLPNC_FAM_FAMILIAR;
   if (!(dwNounCase & NLPNC_VERBOSE_MASK))
      dwNounCase |= NLPNC_VERBOSE_LONG;
   if (!(dwNounCase & NLPNC_CASE_MASK))
      dwNounCase |= NLPNC_CASE_NOMINATIVE;
   if (!(dwNounCase & NLPNC_CAPS_MASK))
      dwNounCase |= NLPNC_CAPS_LOWER;
   if (!(dwNounCase & NLPNC_GENDER_MASK))
      dwNounCase |= NLPNC_GENDER_NEUTER;
   if (!(dwNounCase & NLPNC_PERSON_MASK))
      dwNounCase |= NLPNC_PERSON_THIRD;
   if (!(dwNounCase & NLPNC_QUANTITY_MASK))
      dwNounCase |= NLPNC_QUANTITY_NOQUANTITY;

   // make sure there's enough memory
   DWORD dwLen = (DWORD)wcslen(pszParse);
   if (!pmParsed->Required ((dwLen+1)*sizeof(WCHAR)))
      return NULL;
   pmParsed->m_dwCurPosn = 0;

   // parse
   NLPNounCaseInternal (pszParse, pmParsed, dwNounCase, TRUE, FALSE);

   // append noun-case info?
   if (fAppendNounCase) {
      WCHAR szTemp[32];
      swprintf (szTemp, L"{%d}", (int)dwNounCase);
      pmParsed->StrCat (szTemp);
   }

   pmParsed->CharCat (0);
   PWSTR psz = (PWSTR)pmParsed->p;

   // capitalize the first letter if the flag is set
   if ((dwNounCase & NLPNC_CAPS_MASK) == NLPNC_CAPS_UPPER)
      psz[0] = towupper (psz[0]);

   return psz;
}

/*************************************************************************************
NLPVerbForm - This conjgates the verbs in a sentence. The NLPNC_XXX flags next
to each noun help decide what form of the verb to take.

inputs
   PWSTR          psz - Original string. Ther verb forms are surround by parens and
                  divided with slashes. Each noun (that could affect the verb)
                  should have a number next to it (combination of NLPNC_XXX flags)
                  surrounded by curly braces. For example: "XXX{5435434} (am/are/is/are) here."
   PCMIFLVM       pVM - Virtual machine used for the callback, which will be
                  to "NLPVerbFormCallback"
   PCMem          pMem - Filled in with the new string.
returns
   BOOL - TRUE if success
*/
// NVCINFO - Where the verbs and nouns are
typedef struct {
   DWORD       dwStart;       // Start character, where { or ( is
   DWORD       dwEnd;         // End character, one pas } or )
   BOOL        fVerb;         // if TRUE then it's a verb conjugation
   DWORD       dwNounCase;    // noun case flags
} NVCINFO, *PNVCINFO;

BOOL NLPVerbForm (PWSTR psz, PCMIFLVM pVM, PCMem pMem)
{
   // the string will be less that the lenght of the original
   DWORD dwLen = (DWORD)wcslen(psz);
   if (!pMem->Required ((dwLen+1)*sizeof(WCHAR)))
      return FALSE;
   pMem->m_dwCurPosn = 0;

   // find out where all the verbs and nouns are, adding them to a list.
   CListFixed lNVCINFO;
   lNVCINFO.Init (sizeof(NVCINFO));
   NVCINFO info;
   memset (&info, 0, sizeof(info));
   DWORD i, j;
   for (i = 0; i < dwLen; i++) {
      if (psz[i] == L'(') {
         // found start of a set of verb conjugations
         BOOL fFoundSlash = FALSE;
         for (j = i + 1; (j < dwLen) && (psz[j] != L')'); j++)
            if (psz[j] == L'/')
               fFoundSlash = TRUE;
         if ((psz[j] == L')') && fFoundSlash) {
            // valid verb conjugation list
            info.dwStart = i;
            info.dwEnd = j+1;
            info.fVerb = TRUE;
            lNVCINFO.Add (&info);
            i = j;   // so continue on
         }
      }
      else if (psz[i] == L'{') {
         // found noun form info in brackets
         DWORD dwValue = 0;
         for (j = i + 1; (j < dwLen); j++) {
            if ((psz[j] < L'0') || (psz[j] > L'9'))
               break;
            dwValue = (dwValue * 10) + (DWORD)(psz[j] - L'0');
         } // j
         if ((psz[j] == L'}') && (j > i+1)) {
            // found valid noun form
            info.dwStart = i;
            info.dwEnd = j+1;
            info.dwNounCase = dwValue;
            info.fVerb = FALSE;
            lNVCINFO.Add (&info);
            i = j;   // so continue on
         }
      };
   } // i

   // if there isnt anything to conjugate then easy
   if (!lNVCINFO.Num()) {
      memcpy (pMem->p, psz, (dwLen+1)*sizeof(WCHAR));
      return TRUE;
   }

   // create a list to pass into callback
   // This will be a list of noun numbers, with 0's for in-between verbs.
   // The callback will convert 0's to negative numbers to indicate the conjugation index
   PCMIFLVarList pl = new CMIFLVarList;
   if (!pl)
      return FALSE;
   CMIFLVarLValue var;
   PNVCINFO pvc = (PNVCINFO)lNVCINFO.Get(0);
   for (i = 0; i < lNVCINFO.Num(); i++) {
      if (pvc[i].fVerb)
         var.m_Var.SetDouble(0);
      else
         var.m_Var.SetDouble (pvc[i].dwNounCase);
      pl->Add (&var.m_Var, TRUE);
   } // i

   // call into the callback
   DWORD dwFunc = pVM->FunctionNameToID (L"nlpverbformcallback");

   // call it
   PCMIFLVarList plArg = new CMIFLVarList;
   if (!plArg) {
      pl->Release();
      return FALSE;
   }
   var.m_Var.SetList (pl);
   plArg->Add (&var.m_Var, TRUE);

   // NOTE: purposesly not surrounding with CPUMonitor() calls
   DWORD dwRet = pVM->FunctionCall (dwFunc, plArg, &var);
      // note: Ignoring dwRet

   // go through the list and fill in verbs
   for (i = 0; i < lNVCINFO.Num(); i++) {
      if (!pvc[i].fVerb)
         continue;

      PCMIFLVar pItem = pl->Get(i);
      if (!pItem)
         pvc[i].dwNounCase = 0;
      else
         pvc[i].dwNounCase = (DWORD)(-pItem->GetDouble(pVM));
      if (pvc[i].dwNounCase)
         pvc[i].dwNounCase -= 1; // so 0-indexed
   } // i

   // free the list
   plArg->Release();
   pl->Release();

   // rebuild the sentences...
   for (i = 0; i < lNVCINFO.Num(); i++) {
      // add what came between this and the last
      if (i)
         pMem->StrCat (psz + pvc[i-1].dwEnd, pvc[i].dwStart - pvc[i-1].dwEnd);
      else
         pMem->StrCat (psz, pvc[i].dwStart);

      // if it's not a verb, just skip over
      if (!pvc[i].fVerb)
         continue;

      // else, find the right slash
      DWORD dwLeft = pvc[i].dwNounCase;
      DWORD dwLast = pvc[i].dwStart+1;
      DWORD j;
      for (j = dwLast; (j < pvc[i].dwEnd) && dwLeft; j++)
         if (psz[j] == L'/') {
            // found another slash
            dwLast = j+1;
            dwLeft--;
         };

      // no matter what, take the last one
      // now try to find the end of the string
      for (j = dwLast; (j < pvc[i].dwEnd) && (psz[j] != L')') && (psz[j] != L'/'); j++);
      
      // append
      pMem->StrCat (psz + dwLast, j - dwLast);
   } // i

   // add the last
   pMem->StrCat (psz + (i ? pvc[i-1].dwEnd : 0));
   pMem->CharCat (0);
   
   // done
   return TRUE;
}

