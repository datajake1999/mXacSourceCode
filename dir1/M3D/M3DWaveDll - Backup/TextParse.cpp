/***************************************************************************
TextParse.cpp - Code to do text parsing for text-to-speech.

begun 3/9/2003
Copyright 2003 Mike Rozak. All rights reserved
*/

#include <windows.h>
#include "escarpment.h"
#include "..\M3D.h"
#include "m3dwave.h"
#include "resource.h"


/* globals */
static PWSTR gpszYear = L"Year";
static PWSTR gpszOrdinal = L"Ordinal";
static PWSTR gpszMinute = L"Minute";

/***************************************************************************
CTextParse::Constuctor and destructor
*/
CTextParse::CTextParse (void)
{
   m_pLexicon = NULL;
   m_LangID = 0;
}

CTextParse::~CTextParse (void)
{
   // do nothing for now
   // NOTE: Specifically not releasing the lexicon
}


/***************************************************************************
CTextParse::Clone - Standard functionality
*/
CTextParse *CTextParse::Clone (void)
{
   PCTextParse pNew = new CTextParse;
   if (!pNew)
      return NULL;
   if (!pNew->Init (m_LangID, m_pLexicon)) {
      delete pNew;
      return NULL;
   }
   return pNew;
}

/***************************************************************************
TextParse::Init - Initalizes the text parser to work.

inputs
   LANGID            LangID - Language ID to parse for. If the parser doens't
                        support the given langauge ID then returns error
   PCMLexicon             pLexicon - Lexicon to use for parse disambiguation.
returns
   BOOL - TRUE if success, FALSE if fail
*/
BOOL CTextParse::Init (LANGID LangID, PCMLexicon pLexicon)
{
   // BUGFIX - Ignore if call a second time
   //if (m_LangID || m_pLexicon)
   //   return FALSE;

#if 0 // BUGFIX - Don't do this so can do chinese and other languages
   switch (PRIMARYLANGID(LangID)) {
   case LANG_ENGLISH:
      // can handle this
      break;
   default:
      return FALSE;  // cant handle
   }
#endif // 0
   m_LangID = LangID;
   // NOTE: specifically not addreffing the lexicon
   m_pLexicon = pLexicon;
   return TRUE;
}


/***************************************************************************
TextParse::MMLToPreParse - Takes atext with MML tags in it and
returns a PCMMLNode2 that can be passed to ParseFromMML(). This code
is sliced out of ParseFromMMLText().

inputs
   PWSTR          pszText - Text to parse
   BOOL           fTagged - Set to TRUE if is tagged, FALSE if just text.
returns
   PCMMLNode2 - Can be passed to ParseFromMML()
*/
PCMMLNode2 CTextParse::MMLToPreParse (PWSTR pszText, BOOL fTagged)
{
   if (!fTagged) {
      if (pszText[0] == 0)
         return NULL;

      PCMMLNode2 pNew;
      pNew = new CMMLNode2;
      pNew->ContentAdd (pszText);
      return pNew;
   }

   CEscError Error;
   PCMMLNode pNode;
   PCMMLNode2 pNode2;
   CMem memTemp;
   if (!memTemp.Required ((wcslen(pszText)+20)*sizeof(WCHAR)))
      return NULL;
   PWSTR pszMem = (PWSTR)memTemp.p;
   wcscpy (pszMem, L"<s>");
   wcscat (pszMem, pszText);
   wcscat (pszMem, L"</s>");

   pNode = ParseMML (pszMem, ghInstance, NULL, NULL, &Error, FALSE);
   if (!pNode)
      return NULL;
   pNode2 = pNode->CloneAsCMMLNode2();
   delete pNode;
   if (!pNode2)
      return NULL;

   // pull out sub node
   PCMMLNode2 pSub = NULL;
   PWSTR psz;
   pNode2->ContentEnum (0, &psz, &pSub);
   if (!pSub) {
      delete pNode2;
      return NULL;
   }
   pNode2->ContentRemove (0, FALSE);
   delete pNode2;
   pNode2 = pSub;

   return pNode2;
}

/***************************************************************************
TextParse::ParseFromMMLText - This takes a text string that contains MML,
such as "Hello <sound name="c:\test.wave"/> there.". It then parses it
into MML and converts all the words into <word text=hello/> nodes, etc.

It returns the PCMMLNode2 to the root, all the the words hanging sequentially
off the root. Returns NULL if there's an error in parsing

inputs
   PWSTR          pszText - Text to parse
   BOOL           fIncludePron - If TRUE the include the pronunciation of each
                  word or element in the Pron=xxx tag, which uses the UNSORTED phoneme
                  ID. (Silence will end up being LEXPHONE_SILENCE)
   BOOL              fRandomPron - if TRUE, then select a random pronunciation, so
                     long as it's acceptable. If FALSE, then choose the first one
returns
   PCMMLNode2 - Node. Must be freed by caller
*/
PCMMLNode2 CTextParse::ParseFromMMLText (PWSTR pszText, BOOL fIncludePron, BOOL fRandomPron)
{
   PCMMLNode2 pNode2 = MMLToPreParse (pszText, TRUE);
   if (!pNode2)
      return NULL;

   // call main parser
   if (!ParseFromMML (pNode2, fIncludePron, fRandomPron)) {
      delete pNode2;
      return NULL;
   }

   // done
   return pNode2;
}

/***************************************************************************
TextParse::ParseFromText - This takes a text string (that doesn't contain
any tags) and produces a MMLNode list of words from it.

inputs
   PWSTR                pszText - Text without tages
   BOOL           fIncludePron - If TRUE the include the pronunciation of each
                  word or element in the Pron=xxx tag, which uses the UNSORTED phoneme
                  ID. (Silence will end up being LEXPHONE_SILENCE)
   BOOL              fRandomPron - if TRUE, then select a random pronunciation, so
                     long as it's acceptable. If FALSE, then choose the first one
returns
   PCMMLNode2 - Node, same as from ParseFromMML. Must be freed by caller.
               Returns NULL if error
*/
PCMMLNode2 CTextParse::ParseFromText (PWSTR pszText, BOOL fIncludePron, BOOL fRandomPron)
{
   PCMMLNode2 pNew = MMLToPreParse (pszText, FALSE);
   if (!pNew)
      return NULL;

   // call main parser
   if (!ParseFromMML (pNew, fIncludePron, fRandomPron)) {
      delete pNew;
      return NULL;
   }

   // done
   return pNew;
}


/***************************************************************************
TextParse::
   Word() - Returns the string to be used to idenitfy a word node.
      Text() - Attribute within Word node that contains the string
   Punctuation() - Returns string for puntuation
      Text() - Also supported by Punctiation nodes
   Number() - Number. (Actually, this is internal, and ultimately converted to word)
      Text() - Text of the number
      Context() - Context of the number
*/
PWSTR CTextParse::Word (void)
{
   return L"Word";
}
PWSTR CTextParse::Text (void)
{
   return L"Text";
}
PWSTR CTextParse::Punctuation (void)
{
   return L"Punct";
}
PWSTR CTextParse::Pronunciation (void)
{
   return L"Pron";
}
PWSTR CTextParse::POS (void)
{
   return L"POS";
}
PWSTR CTextParse::RuleDepthLowDetail (void)
{
   return L"RuleDepthLowDetail";
}
PWSTR CTextParse::ParseRuleDepth (void)
{
   return L"ParseRuleDepth";
}
PWSTR CTextParse::Number (void)
{
   return L"Number";
}
PWSTR CTextParse::Context (void)
{
   return L"Context";
}

/***************************************************************************
TextParse::ParseAddPronunciation - This loops through the parsed text and
adds a pronunciation to each word and punctuation element.

The pronunciations are filled with their UNSORTED phoneme numbers.
This also adds part of speech using the word's POS

NOTE: You don't need to call this if you pass the needpron flag into the
parse functions.

inputs
   PCMMLNode2         pNode - Node from ParseFromMML(), ParseFromText(),
                     or ParseFromMMLText()
   BOOL              fRandomPron - if TRUE, then select a random pronunciation, so
                     long as it's acceptable. If FALSE, then choose the first one
returns
   BOOL - TRUE if success, FALSE if error (because no lexicon)
*/
BOOL CTextParse::ParseAddPronunciation (PCMMLNode2 pNode, BOOL fRandomPron)
{
   if (!m_pLexicon)
      return FALSE;

   // get the lexicon to fill in POS
   CListFixed lLEXPOSGUESS;
   m_pLexicon->POSMMLParseToLEXPOSGUESS (pNode, &lLEXPOSGUESS, TRUE, this);

   DWORD dwSilence = m_pLexicon->PhonemeFindUnsort (m_pLexicon->PhonemeSilence());
   BYTE bSilence = (BYTE)dwSilence;
   WCHAR szBinary[256];

   DWORD i;
   PWSTR psz;
   PCMMLNode2 pSub;
   CListVariable lForm, lDontRecurse;
   CListFixed lThe;
   CListFixed lPronsOK, lPronsNotOK;
   lPronsOK.Init (sizeof(DWORD));
   lPronsNotOK.Init (sizeof(DWORD));
   lThe.Init (sizeof(DWORD));
   BOOL fEnglish = PRIMARYLANGID(m_pLexicon->LangIDGet()) == LANG_ENGLISH;
   for (i = 0; i < pNode->ContentNum(); i++) {
      pSub = NULL;
      pNode->ContentEnum (i, &psz, &pSub);
      if (!pSub)
         continue;
      psz = pSub->NameGet();
      if (!psz)
         continue;

      // if already has a pronunciation then ignore
      BOOL fHavePron = FALSE;
      if (pSub->AttribGetString(Pronunciation()))
         fHavePron = TRUE;

      // else, if word/sentence add pronunciation
      if (!_wcsicmp(psz, Word())) {
         // NOTE: At some future point may want to apply some intelligence to
         // the form of the word that select, such as "the" as "thuh" or "thee"

         // get the word
         psz = pSub->AttribGetString(Text());
         if (!psz)
            continue;   // shouldnt happen

         // if have the add it to list of words that need to check on later
         if (!fHavePron && !_wcsicmp(psz, L"the") && fEnglish)
            lThe.Add (&i);

         PWSTR pszPOS = pSub->AttribGetString(POS());
         BYTE bPOSWant = pszPOS ? (BYTE)_wtoi(pszPOS) : 0;

         // get the pronunciation
         lForm.Clear();
         lDontRecurse.Clear();
         m_pLexicon->WordPronunciation (psz, &lForm, TRUE, NULL, &lDontRecurse);
         if (!lForm.Num())
            continue;   // nothing

         // get 1st pronunciation that matches POS
         PBYTE pb;
         DWORD j;
         if (fRandomPron) {
            lPronsOK.Clear();
            lPronsNotOK.Clear();
         }
         for (j = 0; j < lForm.Num();  j++) {
            pb = (PBYTE)lForm.Get(j) + 1;
            if (pb[-1] == bPOSWant) {
               if (fRandomPron)
                  lPronsOK.Add (&j);
               else
                  // take the first one
                  break;
            }
            else if (fRandomPron)
               lPronsNotOK.Add (&j);
         }

         // if doing random pronunciations, and they exist, then choose one
         if (fRandomPron) {
            PCListFixed plPronsToUse = lPronsOK.Num() ? &lPronsOK : &lPronsNotOK;

            if (plPronsToUse->Num() >= 2 ) {
               j = *((DWORD*)plPronsToUse->Get((DWORD)rand() % plPronsToUse->Num()));
               pb =  (PBYTE)lForm.Get(j) + 1;
            }
         }

         if (j >= lForm.Num())
            pb =  (PBYTE)lForm.Get(0) + 1;   // just choose first one
         DWORD dwLen = (DWORD)strlen((char*)pb);
         if (dwLen+1 >= sizeof(szBinary)/sizeof(WCHAR)/2)
            continue;   // too long

         // subtract 1
         for (j = 0; j < dwLen; j++)
            pb[j]--;

         // add
         if (!fHavePron) {
            MMLBinaryToString (pb, dwLen, szBinary);
            pSub->AttribSetString (Pronunciation(), szBinary);
         }

         // set POS
         _itow(pb[-1], szBinary, 10);
         pSub->AttribSetString (POS(), szBinary);
         continue;
      }
      else if (!_wcsicmp(psz, Punctuation())) {
         // add a silence.
         if (!fHavePron) {
            MMLBinaryToString (&bSilence, sizeof(bSilence), szBinary);
            pSub->AttribSetString (Pronunciation(), szBinary);
         }
         continue;
      }
   }// i

   // fix "the" to "thuh" or "thee" for english
   DWORD *padw = (DWORD*)lThe.Get(0);
   for (i = 0; i < lThe.Num(); i++) {
      pSub = NULL;
      pNode->ContentEnum (padw[i], &psz, &pSub);
      if (!pSub)
         continue;

      // look at the next word
      PCMMLNode2 pNext = NULL;
      pNode->ContentEnum (padw[i]+1, &psz, &pNext);
      if (!pNext)
         continue;   // nothing there

      // get the pronunciation
      BYTE abPron[128];
      psz = pNext->AttribGetString (Pronunciation());
      if (!psz)
         continue;
      if (MMLBinaryFromString (psz, abPron, sizeof(abPron)) < 1)
         continue;   // too short

      // get the phoneme
      PLEXPHONE plp = m_pLexicon->PhonemeGetUnsort(abPron[0]);
      if (!plp)
         continue;
      PLEXENGLISHPHONE ple = MLexiconEnglishPhoneGet(plp->bEnglishPhone);
      if (!ple)
         continue;

      // what form?
      DWORD dwForm = ((ple->dwCategory & PIC_MAJORTYPE) == PIC_VOWEL) ? 1 : 0;

      // get the word string
      psz = pSub->AttribGetString(Text());
      if (!psz)
         continue;   // shouldnt happen

      // get the pronunciation
      lForm.Clear();
      lDontRecurse.Clear();
      m_pLexicon->WordPronunciation (psz, &lForm, TRUE, NULL, &lDontRecurse);
      if (!lForm.Num())
         continue;   // nothing
      dwForm = min(dwForm, lForm.Num()-1);

      // get 1st pronunciation that matches POS
      PBYTE pb;
      pb = (PBYTE)lForm.Get(dwForm) + 1;
      DWORD dwLen = (DWORD)strlen((char*)pb);
      if (dwLen+1 >= sizeof(szBinary)/sizeof(WCHAR)/2)
         continue;   // too long

      // subtract 1
      DWORD j;
      for (j = 0; j < dwLen; j++)
         pb[j]--;

      // add
      MMLBinaryToString (pb, dwLen, szBinary);
      pSub->AttribSetString (Pronunciation(), szBinary);
   } // i

   return TRUE;
}


/***************************************************************************
TextParse::ParseToText - This takes MML from either ParseFromMML(),
ParseFromText(), or ParseFromMMLText() and outputs a text string of words.

inputs
   PCMMLNode2         pNode - node
   PCMem             pMemText - Memory block that is filled with the string
returns
   BOOL - TRUE if success
*/
BOOL CTextParse::ParseToText (PCMMLNode2 pNode, PCMem pMemText)
{
   MemZero (pMemText);

   DWORD i;
   PWSTR psz;
   PCMMLNode2 pSub;
   for (i = 0; i < pNode->ContentNum(); i++) {
      if (!pNode->ContentEnum (i, &psz, &pSub))
         continue;

      // shouldnt have any text by itself, only words
      if (!pSub)
         continue;
      psz = pSub->NameGet();
      if (!psz)
         continue;

      // if word
      if (!_wcsicmp(psz, Word())) {
         psz = pSub->AttribGetString (Text());
         if (!psz)
            return FALSE;

         if (((PWSTR)pMemText->p)[0])
            MemCat (pMemText, L" ");   // separating speace
         MemCat (pMemText, psz);
      }
      else if (!_wcsicmp(psz, Punctuation())) {
         psz = pSub->AttribGetString (Text());
         if (!psz)
            return FALSE;

         MemCat (pMemText, psz);
      }
   } // i
   
   return TRUE;
}

/***************************************************************************
ChineseIsAlphaPinyin - Checks to the if the character is acceptable for pinyin.

inputs
   WCHAR          cChar - Character to test
returns
   BOOL - TRUE if it's acceptable for pinyin.
*/
BOOL ChineseIsAlphaPinyin (WCHAR cChar)
{
   if ((cChar >= L'A') && (cChar <= L'Z'))
      return TRUE;
   if ((cChar >= L'a') && (cChar <= L'z'))
      return TRUE;

   // Oomlaut is changed to v
   // if (cChar == L'ü')
   //   return TRUE;

   return FALSE;
}


#define CHINESEPINYIN_MAXALPHA      8     // pinyin can't be any more than 8 alpha chars
#define CHINESEPINYIN_MAXFORM       5     // up to form 5
#define CHINESEPINYIN_MAXSYLLABLES  8     // maximum syllables in a word
#define CHINESEPINYIN_MAXSYLLABLESGUESS   3  // if guessing at the existence of a word (because individual syllables don't make sense) then maxes out at 3 syllables

/***************************************************************************
TextParse::ChineseFindPinyinSyllableLength - This takes a string and tests to see if
it's Pinyin. It returns the length of the pinyin syllable in characters.

inputs
   PCMLexicon     pLexicon - Lexicon
   PWSTR          pszText - Text
   BOOL           fCompareAgainstLex - if TRUE, compare this againt the lexicon
                  if it looks like pinyin. If it doesn't compare then error
returns
   DWORD - Number of characters of pinyin, or 0 if it isn't pinyin
*/
DWORD ChineseFindPinyinSyllableLength (PCMLexicon pLexicon, PWSTR pszText, BOOL fCompareAgainstLex)
{
   // see how many alpha chars in a row
   DWORD i;
   for (i = 0; i < CHINESEPINYIN_MAXALPHA; i++)
      if (!ChineseIsAlphaPinyin(pszText[i]))
         break;
   if (!i)
      return 0;   // nothing

   // need a number
   if ((pszText[i] < L'1') || (pszText[i] > L'0' + CHINESEPINYIN_MAXFORM))
      return 0;   // nothing

   // else, found match
   i++;  // to include number

   // test againt lexicon
   if (fCompareAgainstLex && pLexicon) {
      WCHAR cTemp = pszText[i];
      pszText[i] = 0;

      BOOL fRet = pLexicon->WordExists (pszText);

#ifdef _DEBUG
      if (!fRet) {
         OutputDebugStringW (L"\r\nChineseFindPinyinSyllableLength can't verify pinyin: ");
         OutputDebugStringW (pszText);
      }
#endif

      pszText[i] = cTemp;

      if (!fRet)
         return 0;
   }

   return i;
}


/***************************************************************************
CTextParse::ChineseIsSyllableAWord - Sees if a syllable is actually a word.

inputs
   PCMLexicon           pLexicon - Lexicon to use
   PWSTR                pszText - For the syllable
returns
   BOOL - TRUE if its a word (because it has a part-of-speech)
*/
BOOL ChineseIsSyllableAWord (PCMLexicon pLexicon, PWSTR pszText)
{
   CListVariable lForms;
   DWORD i;

   if (!pLexicon || !pLexicon->WordGet (pszText, &lForms))
      return FALSE;  // shouldnt happen
   for (i = 0; i < lForms.Num(); i++) {
      PBYTE pbPron = (PBYTE)lForms.Get(i);
      if (pbPron[0])
         break;
   }
   if (i < lForms.Num())
      return TRUE; // this works as a stand-alone word

   // else, just a syllable
   return FALSE;
}

/***************************************************************************
CTextParse::ChineseFindPinyinLongest - Find the longest sequence of syllables
that makes up a word.

inputs
   PCMLexicon           pLexicon - Lexicon to use
   PCListVariable       plLookAhead - List of future words
   DWORD                dwStart - Starting index (in plLookAhead) to look at
   PCMem                pMemWord - Scratch memory where the longest word is written
   BOOL                 *pfIsWord - Filled with TRUE if it's definitely a word, FALSE if it's just a syllable
returns
   DWORD - Longest number of syllables that makes a word.
*/
DWORD ChineseFindPinyinLongest (PCMLexicon pLexicon, PCListVariable plLookAhead, DWORD dwStart, PCMem pMemWord, BOOL *pfIsWord)
{
   DWORD i;
   DWORD dwSearch;
   for (dwSearch = plLookAhead->Num(); dwSearch <= plLookAhead->Num(); dwSearch--) {
      if (dwSearch <= dwStart)
         break;

      // create the string
      MemZero (pMemWord);
      for (i = dwStart; i < dwSearch; i++) {
         if (i > dwStart)
            MemCat (pMemWord, L"-");

         MemCat (pMemWord, (PWSTR)plLookAhead->Get(i));
      } // i

      // does it exist
      if (pLexicon && pLexicon->WordExists ((PWSTR)pMemWord->p)) {
         if (dwSearch - dwStart >= 2)
            *pfIsWord = TRUE; // if two syllables always a word
         else
            *pfIsWord = ChineseIsSyllableAWord (pLexicon, (PWSTR)pMemWord->p);

         return dwSearch - dwStart;
      }
   } // dwSearch

   // doesn't make a word at all
   *pfIsWord = FALSE;
   return 0;
}

/***************************************************************************
CTextParse::ChineseFindPinyinWord - Finds the longest pinyin word in the string.

inputs
   PWSTR          pszText - Text string to look into
   PCMem          pMemWord - If this finds a word, this memory will be filled with
                     the word string.
returns
   DWORD - Number of characters used to create the pinyin word. If this is 0 then
            no acceptable pinyin word was found.
*/
DWORD CTextParse::ChineseFindPinyinWord (PWSTR pszText, PCMem pMemWord)
{
   // look ahead and see how many words can find
   CListVariable lLookAhead;  // list of strings for each of the words
   WCHAR aszSeparator[CHINESEPINYIN_MAXSYLLABLES]; // separator found after the word
   DWORD adwTotalLength[CHINESEPINYIN_MAXSYLLABLES]; // total length
   DWORD i;
   DWORD dwRet;
   PWSTR pszCur = pszText;
   for (i = 0; i < CHINESEPINYIN_MAXSYLLABLES; i++) {
      dwRet = ChineseFindPinyinSyllableLength (m_pLexicon, pszCur, TRUE);
      if (!dwRet)
         break;

      // add this tot he look-ahead list
      WCHAR cTemp = aszSeparator[i] = pszCur[dwRet];

      if (i && (aszSeparator[i-1] != aszSeparator[0]))
         break;   // separators changed

      pszCur[dwRet] = 0;
      lLookAhead.Add (pszCur, (dwRet + 1) * sizeof(WCHAR));
      pszCur[dwRet] = cTemp;
      adwTotalLength[i] = (DWORD)((PBYTE)pszCur - (PBYTE)pszText) / sizeof(WCHAR) + dwRet;

      // can only advance further if encountered a hypen or a space
      if ((cTemp != L'-') && (cTemp != L' '))
         break;

      // skip space
      pszCur += dwRet;
      if (pszCur[0])
         pszCur++;   // skip the hyphen or space
      while (pszCur[0] == L' ')
         pszCur++;
   } // i

   // if didn't find anything at all then exit now
   if (!lLookAhead.Num())
      return 0;

   // if have a hyphen after the first syllable then combine together and test
   // that it's a whole word
   if (aszSeparator[0] == L'-') {   // know all the other separators will also be hyphens
      MemZero (pMemWord);
      for (i = 0; i < lLookAhead.Num(); i++) {
         if (i)
            MemCat (pMemWord, L"-");

         MemCat (pMemWord, (PWSTR)lLookAhead.Get(i));
      } // i

      // BUGFIX - If had the hyphens, then assume that wanted to be a whole word,
      // so just return it as that
      return adwTotalLength[lLookAhead.Num()-1];

      // see if this is a word
      //if (m_pLexicon && m_pLexicon->WordExists ((PWSTR)pMemWord->p))
         // found an entire word with hyphens
      //   return adwTotalLength[lLookAhead.Num()-1];

      // else, wipe all the hyphens because not understood
      // note: dont bother wish this since won't look at this variable again
      // for (i = 0; (aszSeparator[i] == L'-') && (i < lLookAhead.Num()); i++)
      //    aszSeparator[i] = L' ';
      //    // this should set all separators to spaces

      //MemZero (pMemWord);
   } // if hyphen
   
   // see what the longest word is
   BOOL fIsWord;
   DWORD dwLongest = ChineseFindPinyinLongest (m_pLexicon, &lLookAhead, 0, pMemWord, &fIsWord);
   if (!dwLongest)
      return 0;

   // if this is definitely a word, then return it
   if (fIsWord || (dwLongest >= lLookAhead.Num()) )
      return adwTotalLength[dwLongest-1];

   // if get here, only one syllable long, and it doesn't seem to be a word
   // therefore, extend see if can find any whole words after
   // BUGFIX - Not too many syllables long as guestimated word
   DWORD dwStart;
   for (dwStart = 1; (dwStart < CHINESEPINYIN_MAXSYLLABLESGUESS) && (dwStart < lLookAhead.Num()); dwStart++) {
      dwLongest = ChineseFindPinyinLongest (m_pLexicon, &lLookAhead, dwStart, pMemWord, &fIsWord);

      // stop if find a word (or an error)
      if (!dwLongest || fIsWord)
         break;
   } // dwStart

   // use dwStart as the length
   dwLongest = dwStart;
   MemZero (pMemWord);
   for (i = 0; i < dwLongest; i++) {
      if (i)
         MemCat (pMemWord, L"-");

      MemCat (pMemWord, (PWSTR)lLookAhead.Get(i));
   } // i
   return adwTotalLength[dwLongest-1];
}


/***************************************************************************
CTextParse::ChineseUse - Returns true if should use the chinese parsing rules
for this lexicon
*/
BOOL CTextParse::ChineseUse (void)
{
   if (PRIMARYLANGID(m_LangID) == 0x04)
      return TRUE;
   else
      return FALSE;
}

/***************************************************************************
TextParse::ParseFromMML - This that a MML node whose children make up a list
of text to be parsed. The elements are either in the form of text or as
other MML nodes which will be treated as tags within the text. (The other MML
nodes might be something like <audio file=c:\test.wav/> to play audio.)

This looks at the text chunks and then parses them into smaller pieces of
either <word text="hello"/> or <punct text="."/>.

It will also parse integers, floats, time, currency, and dates. It uses
the lexicon to disambiguation abbreviations "Mr." to <word text="mr."/> instead of
<word text=Mr/><punct text=.>

inputs
   PCMMLNode2         pNode - Node to parse. This is modified in the process
   BOOL           fIncludePron - If TRUE the include the pronunciation of each
                  word or element in the Pron=xxx tag, which uses the UNSORTED phoneme
                  ID. (Silence will end up being LEXPHONE_SILENCE)
   BOOL              fRandomPron - if TRUE, then select a random pronunciation, so
                     long as it's acceptable. If FALSE, then choose the first one
returns
   BOOL - TRUE if success. FALSE if had problems
*/
BOOL CTextParse::ParseFromMML (PCMMLNode2 pNode, BOOL fIncludePron, BOOL fRandomPron)
{
   // must have initalized
   if (!m_LangID)
      return FALSE;

   // set the overall name to make sure ti has one
   pNode->NameSet (L"Parse");

   // split up by spaces. This is universal amongst languages, even japanese,
   // when it has spaces
   DWORD dwNode;
   PCMMLNode2 pSub;
   CMem memTemp;  // BUGFIX - was using gMemTEmp, which isnt' thread safe
   PWSTR psz;

   // loop through the nodes and split up lines with \r\n
   for (dwNode = 0; dwNode < pNode->ContentNum(); dwNode++) {
      if (!pNode->ContentEnum (dwNode, &psz, &pSub))
         continue;
      if (!psz)
         continue;

      // copy into memTemp
      DWORD dwLen = (DWORD)wcslen(psz);
      if (!memTemp.Required ((dwLen+1) * sizeof(WCHAR)))
         return FALSE;
      memcpy (memTemp.p, psz, (dwLen+1)*sizeof(WCHAR));
      psz = (PWSTR) memTemp.p;

      // start at the beginning and work forwards until find a whitespace
      DWORD dwInserted = 0;
      while (psz[0]) {
         // skip all whitesapce at the beginning
         while (iswspace(psz[0]))
            psz++;

         if (!psz[0])
            break;

         // loop until whitespace (but not space), or end of string
         PWSTR pszEnd;
         for (pszEnd = psz+1; pszEnd[0] && !(iswspace(pszEnd[0]) && (pszEnd[0] != L' ')); pszEnd++);
         
         // isolate
         WCHAR cEnd = pszEnd[0];
         pszEnd[0] = 0;

         // only insert if not blank
         if (psz[0]) {
            dwInserted++;
            pNode->ContentInsert (dwNode+dwInserted, psz);
         }

         pszEnd[0] = cEnd;
         psz = pszEnd;
      } // while psz[0];
      
      // if get here delete the current node
      pNode->ContentRemove (dwNode);
      dwNode--;   // since just deleted it
      dwNode += dwInserted;
   } // dwNode


   // split up into invidual words
   CMem memWord;  // used for chinese
   for (dwNode = 0; dwNode < pNode->ContentNum(); dwNode++) {
      if (!pNode->ContentEnum (dwNode, &psz, &pSub))
         continue;
      if (!psz)
         continue;

      // copy into memTemp
      DWORD dwLen = (DWORD)wcslen(psz);
      if (!memTemp.Required ((dwLen+1) * sizeof(WCHAR)))
         return FALSE;
      memcpy (memTemp.p, psz, (dwLen+1)*sizeof(WCHAR));
      psz = (PWSTR) memTemp.p;

      // start at the beginning and work forwards until find a whitespace
      DWORD dwInserted = 0;
      while (psz[0]) {
         // skip whitepace
         while (iswspace(psz[0]))
            psz++;

         if (!psz[0])
            break;

         if (ChineseUse()) {
            DWORD dwUsed = ChineseFindPinyinWord (psz, &memWord);
            if (dwUsed) {
               dwInserted++;

               // whatever is left must be a word, so add it as such
               pSub = new CMMLNode2;
               if (pSub) {
                  pSub->NameSet (Word());
                  pSub->AttribSetString (Text(), (PWSTR)memWord.p);
                  pNode->ContentInsert (dwNode+dwInserted, pSub);

#ifdef _DEBUG
                  OutputDebugStringW (L"\r\nParseFromMML Chinese word: ");
                  OutputDebugStringW ((PWSTR)memWord.p);
#endif
               }
               psz += dwUsed;

               continue;
            }
         }


         // loop until whitespace, or end of string
         PWSTR pszEnd;
         for (pszEnd = psz+1; pszEnd[0] && !iswspace(pszEnd[0]); pszEnd++);
         
         // isolate
         WCHAR cEnd = pszEnd[0];
         pszEnd[0] = 0;

         // only insert if not blank
         if (psz[0]) {
            dwInserted++;
            pNode->ContentInsert (dwNode+dwInserted, psz);
         }

         pszEnd[0] = cEnd;
         psz = pszEnd;
      } // while psz[0];
      
      // if get here delete the current node
      pNode->ContentRemove (dwNode);
      dwNode--;   // since just deleted it
      dwNode += dwInserted;
   } // dwNode

   // go back through the list and sub-parse all the individual text items.
   for (dwNode = 0; dwNode < pNode->ContentNum(); dwNode++) {
      if (!pNode->ContentEnum (dwNode, &psz, &pSub))
         continue;
      if (!psz)
         continue;

      // BUGFIX - Copy so that wont crash due to the fact that MMLNode2 moves
      // memory around
      // copy into memTemp
      DWORD dwLen = (DWORD)wcslen(psz);
      if (!memTemp.Required ((dwLen+1) * sizeof(WCHAR)))
         return FALSE;
      memcpy (memTemp.p, psz, (dwLen+1)*sizeof(WCHAR));
      psz = (PWSTR) memTemp.p;

      // parse subitem
      if (!ParseIndividualText (psz, pNode, dwNode))
         return FALSE;
      pNode->ContentRemove (dwNode);
      dwNode--;   // since just deleted, to counteract the dwNode++
   } // dwNode

   // go back through the nodes and parse the numbers...
   for (dwNode = 0; dwNode < pNode->ContentNum(); dwNode++) {
      if (!pNode->ContentEnum (dwNode, &psz, &pSub))
         continue;
      if (!pSub)
         continue;
      psz = pSub->NameGet();
      if (!psz)
         continue;
      if (_wcsicmp(psz, Number()))
         continue;
      psz = pSub->AttribGetString (Text());
      if (!psz)
         continue;

      // parse subitem
      if (!ParseNumberToWords (psz, pNode, dwNode))
         return FALSE;
      pNode->ContentRemove (dwNode);
      dwNode--;   // since just deleted, to counteract the dwNode++
   } // dwNode

   // produce pronunciations?
   if (fIncludePron)
      if (!ParseAddPronunciation (pNode, fRandomPron))
         return FALSE;

   return TRUE;
}

/***************************************************************************
CTextParse::IsPunct - Returns TRUE if the given character is punctuation
that should be converted to <punct text="."/>.

inputs
   WCHAR          cTest - Test this
returns
   WCHAR - Punctuation to use, or 0 if not puncuation. For example, if
      have a back-quite character will return a single quote.
*/
WCHAR CTextParse::IsPunct (WCHAR cTest)
{
   switch (cTest) {
   case L'.':
   case L',':
   case L';':
   case L':':
   case L'"':
   case L'!':
   case L'?':
   case L'-':
   case L'(':  // BUGFIX - Add brackets
   case L')':
   case L'[':
   case L']':
   case L'{':
   case L'}':
   case L'<':
   case L'>':
   case L'*':
   case L'/':
   case L'%':
   case L'+':
   case L'|':
   case L'=':
   case L'\\':
   case L'_':
   case L'@':
   case L'#':
   case L'&':
      return cTest;

   case '`':   // BUGFIX - convert this
      return L'\'';

   case L'“':
   case L'”':
      return L'"';

   default:
      return 0;
   }
}


/***************************************************************************
CTextParse::IsWordSplitPunct - Returns TRUE if the given character is punctuation
that can split a word, such as -, /, or \.

inputs
   WCHAR          cTest - Test this
returns
   BOOL - TRUE if splitter
*/
BOOL CTextParse::IsWordSplitPunct (WCHAR cTest)
{
   WCHAR c = IsPunct(cTest);
   if (!c)
      return FALSE;

   switch (c) {
   case L'\'': // dont ever split word because of this
      return FALSE;
   default: // all other punctuation
      return TRUE;
   }
}

/***************************************************************************
CTextParse::ParseIndividualText - Internal function that parses individual
text items such as "hello,". This will
   1) Pull out punctiation such as ".", ",", etc.
   2) Parse numbers, time, currency, dates, etc.
   3) Produce individual words in the end

inputs
   PWSTR          psz - String to analyze
   PCMMLNode2      pNode - Parse node of the string
   DWORD          dwNode - Node within pNode, which is the string. If any new
                  strings are to be added then insert them AFTER dwNode so
                  they get parsed as a result.
returns
   BOOL - TRUE if OK, FALSE if error
*/
BOOL CTextParse::ParseIndividualText (PWSTR psz, PCMMLNode2 pNode, DWORD dwNode)
{
   // remove punctuation from start ... unless is a period followed by
   // a number
   PCMMLNode2 pSub;
   WCHAR cPunct;
   WCHAR szTemp[6], szAcronym[3];
   DWORD dwLen = (DWORD)wcslen(psz);
   DWORD i;

   // do acronyms
   if ((dwLen >= 2) && (dwLen <= 5)) {
      for (i = 0; i < dwLen; i++)
         if (!iswalpha(psz[i]) || !iswupper(psz[i]))
            break;

      // if there's already a pronunciation then ignore
      if ((i >= dwLen) && m_pLexicon->WordExists(psz))
         i = 0;

      if (i >= dwLen) {
         // have an acronym that doesn't exist
         for (i = i - 1; i < dwLen; i--) {
            szAcronym[0] = psz[i];
            szAcronym[1] = L'.';
            szAcronym[2] = 0;

            if (i)
               pNode->ContentInsert (dwNode+1, szAcronym);
         }

         // continue parsing with only this
         dwLen = 2;
         psz = szAcronym;
      }
   } // potential acronym

   cPunct = IsPunct (psz[0]);

   DWORD dwUsed;
   switch (cPunct) {
   case L'.':
   case L'-':
      // test that it's not a FP number or integer
      // if so then set cPunct to 0
      dwUsed = FloatParse (psz, NULL, NULL);
      if (dwUsed) {
         // if there's stuff after the number then deal with
         if (dwUsed < dwLen)
            pNode->ContentInsert (dwNode+1, psz+dwUsed);

         // insert the number
         if (!NumberInsert (psz, dwUsed, NULL, pNode, dwNode+1))
            return FALSE;
         return TRUE;
      }
      break;

   }
   if (cPunct) {
insertpunctbefore:
      // add the text that follows this
      if (psz[1])
         pNode->ContentInsert (dwNode+1, psz + 1);

      // put in the punctuation
      pSub = new CMMLNode2;
      if (!pSub)
         return FALSE;
      pSub->NameSet (Punctuation());
      szTemp[0] = cPunct;
      szTemp[1] = 0;
      pSub->AttribSetString (Text(), szTemp);
      pNode->ContentInsert (dwNode+1, pSub);
      return TRUE;
   }

   // remove punciation from end... unless is a period that makes up
   // abbreviation
   cPunct = (dwLen ? IsPunct(psz[dwLen-1]) : 0);
   switch (cPunct) {
   case L'.':
      // test the lexicon to see if it's an abbreviation
      if (m_pLexicon && m_pLexicon->WordExists(psz))
         cPunct = 0;
      // if so then set cPunct to 0
      break;
   }
   if (cPunct) {
insertpunctafter:
      // put in the punctuation
      pSub = new CMMLNode2;
      if (!pSub)
         return FALSE;
      pSub->NameSet (Punctuation());
      szTemp[0] = cPunct;
      szTemp[1] = 0;
      pSub->AttribSetString (Text(), szTemp);
      pNode->ContentInsert (dwNode+1, pSub);

      // add the text that follows this
      psz[dwLen-1] = 0; // technically shouldnt do this, but OK since will be deleting it
      if (psz[0])
         pNode->ContentInsert (dwNode+1, psz);

      return TRUE;
   }

   // Should see if the entire thing matches a lex, in which case
   // dont split. Do this to handle a "word" like "MI5".
   if (m_pLexicon && m_pLexicon->WordExists (psz))
      goto wholeword;

   if (ParseContainNumber (psz, pNode, dwNode))
      return TRUE;

   // see if this can be split up smaller
   for (i = 0; psz[i]; i++)
      if (IsWordSplitPunct(psz[i]))
         break;
   if (psz[i] && psz[i+1]) {
      // if get here then found a punctuation that should split at, so split here
      pNode->ContentInsert (dwNode+1, psz + (i+1));

      // add the text that follows this
      psz[i] = 0; // technically shouldnt do this, but OK since will be deleting it
      // NOTE - Zeroing out over the punctuation point since don't want this to
      // cause a pause in the speaking if it's a hyphen
      // continue on to adding whole word
   }

   // BUGFIX - try removing ' from beginning if word doesn't exist
   dwLen = (DWORD)wcslen(psz); // since may have changed
   if ((dwLen >= 2) && m_pLexicon && (psz[0] == L'\'') ) {
      if (m_pLexicon->WordExists(psz+1)) {
         cPunct = psz[0];
         goto insertpunctbefore;
      }

   } // if start with '

   // remove ' from the end since not part of word
   if ((dwLen >= 2) && (psz[dwLen-1] == L'\'')) {
      cPunct = psz[dwLen-1];
      goto insertpunctafter;
   }

wholeword:
   // whatever is left must be a word, so add it as such
   pSub = new CMMLNode2;
   if (!pSub)
      return FALSE;
   pSub->NameSet (Word());
   pSub->AttribSetString (Text(), psz);
   pNode->ContentInsert (dwNode+1, pSub);

   return TRUE;
}


/***************************************************************************
CTextParse::NaturalParse - Parses a string (as far as possible) assuming
it's a natural number.

inputs
   PWSTR          psz - String, NULL terminated
   int            *piVal - Filled with the value
   BOOL           fCanHaveComma - set to TRUE if this can have commas in the number
                  (such as 30,000) or FALSE if can't
returns
   DWORD - Number of characters used
*/
DWORD CTextParse::NaturalParse (PWSTR psz, BOOL fCanHaveComma, int *piVal)
{
   if (piVal)
      *piVal = 0;

   // look for a comma
   DWORD dwComma = -1;
   if (fCanHaveComma)
      for (dwComma = 0; psz[dwComma]; dwComma++)
         if (psz[dwComma] == L',')
            break;

   DWORD dwUsed;
   for (dwUsed = 0; psz[dwUsed]; dwUsed++) {
      // if it's a comman and expecting one then can skip
      if ((psz[dwUsed] == L',') && (dwComma != -1) && (dwUsed >= dwComma) && (((dwUsed - dwComma)%4) == 0))
         continue;

      if ((psz[dwUsed] < L'0') || (psz[dwUsed] > L'9'))
         break;

      if (piVal)
         *piVal = (*piVal * 10) + (int)psz[dwUsed] - (int)L'0';
   }

   return dwUsed;
}


/***************************************************************************
CTextParse::IntegerParse - Parses a string (as far as possible) assuming
it's a integer (signed) number.

inputs
   PWSTR          psz - String, NULL terminated
   int            *piVal - Filled with the value
returns
   DWORD - Number of characters used
*/
DWORD CTextParse::IntegerParse (PWSTR psz, int *piVal)
{
   if (psz[0] == L'-') {
      DWORD dwRet = NaturalParse (psz+1, TRUE, piVal);
      if (!dwRet)
         return 0;   // minus, but no number

      // else, negative
      if (piVal)
         *piVal = *piVal * -1;
      return dwRet+1;
   }
   else
      return NaturalParse (psz, TRUE, piVal);
}


/***************************************************************************
CTextParse::FloatParse - Parses a string (as far as possible) assuming
it's a floatig point number.

inputs
   PWSTR          psz - String, NULL terminated
   fp             *pfVal - Filled with the value
   DWORD          *pdwDecStart - Filled in the the index (from psz) where the
                  1st digit after the decimal point starts
returns
   DWORD - Number of characters used
*/
DWORD CTextParse::FloatParse (PWSTR psz, fp *pfVal, DWORD *pdwDecStart)
{
   DWORD dwRet;
   int iVal;

   if (psz[0] == L'.') {
      // might be ".23"... see
      dwRet = NaturalParse (psz+1, FALSE, &iVal);
      if (!dwRet)
         return 0;   // nope, just a period

      // else, float
      if (pfVal)
         *pfVal = pow ((fp).1, (fp)dwRet) * (fp)iVal;
      if (pdwDecStart)
         *pdwDecStart = 1;
      return dwRet+1;
   }

   // first, must parse integer
   dwRet = IntegerParse (psz, &iVal);
   if (!dwRet)
      return 0;   // nope
   if (pfVal)
      *pfVal = (fp)iVal;
   if (pdwDecStart)
      *pdwDecStart = 0;

   // see if there's a period
   if (psz[dwRet] == L'.') {
      DWORD dwRet2;
      // might be ".23"... see
      dwRet2 = NaturalParse (psz+(dwRet+1), FALSE, &iVal);
      if (!dwRet2)
         return dwRet;   // nope, just a period

      if (pdwDecStart)
         *pdwDecStart = dwRet + 1;

      // else, float
      if (pfVal) {
         *pfVal = fabs(*pfVal) + pow ((fp).1, (fp)dwRet2) * (fp)iVal;
         if (psz[0] == L'-')
            *pfVal *= -1;
      }
      return dwRet+dwRet2+1;
   }

   return dwRet;
}


/***************************************************************************
CTextParse::TimeParse - Parses a string (as far as possible) assuming
it's a floatig point number.

inputs
   PWSTR          psz - String, NULL terminated
   int            *paiHour - Filled in with the hour
   int            *paiMinute - Filled with the minute
returns
   DWORD - Number of characters used
*/
DWORD CTextParse::TimeParse (PWSTR psz, int *paiHour, int *paiMinute)
{
   int iHour;
   DWORD dwHour = NaturalParse (psz, FALSE, &iHour);
   if (iHour > 24)
      return 0;   // cant be
   if (paiHour)
      *paiHour = iHour;

   // must be color
   if (psz[dwHour] != L':')
      return 0;

   int iMinute;
   DWORD dwMinute = NaturalParse (psz + (dwHour+1), FALSE, &iMinute);
   if (dwMinute != 2)
      return 0;
   if (iMinute >= 60)
      return 0;
   if (paiMinute)
      *paiMinute = iMinute;
   
   return dwHour + dwMinute + 1;
}

/***************************************************************************
CTextParse::DateParse - Parses a string (as far as possible) assuming
it's a date.

inputs
   PWSTR          psz - String, NULL terminated
   int            *paiDay - Day of month. Filled to -1 if not specified
   int            *paiMonth - Month
   int            *paiYear - Year
returns
   DWORD - Number of characters used
*/
DWORD CTextParse::DateParse (PWSTR psz, int *paiDay, int *paiMonth, int *paiYear)
{
   // look for DMY or MDY or MY
   int aiVal[3];
   DWORD adwNum[3];
   memset (aiVal, 0, sizeof(aiVal));
   memset (adwNum, 0, sizeof(adwNum));

   // furst entry
   adwNum[0] = NaturalParse (psz, FALSE, &aiVal[0]);
   if (!adwNum[0])
      return 0;
   if ((psz[adwNum[0]] != L'/') && (psz[adwNum[0]] != L'-'))   // separator
      return 0;
   WCHAR cSep = psz[adwNum[0]];

   // second entry
   adwNum[1] = NaturalParse (psz + (adwNum[0]+1), FALSE, &aiVal[1]);
   if (!adwNum[1])
      return 0;

   // third?
   DWORD dwFields = 2;
   if (psz[adwNum[0]+adwNum[1]+1] == cSep) {
      adwNum[2] = NaturalParse (psz + (adwNum[0]+adwNum[1]+2), FALSE, &aiVal[2]);
      if (!adwNum[2])
         return 0;   // looked like another date but not
      dwFields = 3;
   }

   // which field is which
   DWORD dwMonth, dwDay, dwYear;
   if (dwFields == 2) {
      dwMonth = 0;
      dwYear = 1;
      dwDay = -1;
   }
   else {
      // order of dates
      switch (m_LangID) {
      case MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US):
         dwMonth = 0;
         dwDay = 1;
         dwYear = 2;
         break;
      default:
         dwMonth = 1;
         dwDay = 0;
         dwYear = 2;
         break;
      }
   }

   // limits checks
   if ((aiVal[dwMonth] < 1) || (aiVal[dwMonth] > 12))
      return 0;
   if ((dwDay != -1) && ((aiVal[dwDay]<1) || (aiVal[dwDay]>31)))
      return 0;
   if ((aiVal[dwYear] < 100) && (adwNum[dwYear] == 2))
      aiVal[dwYear] += 2000;  // round up to current century

   // fill in info
   if (paiDay)
      *paiDay = (dwDay != -1) ? aiVal[dwDay] : -1;
   if (paiMonth)
      *paiMonth = aiVal[dwMonth];
   if (paiYear)
      *paiYear = aiVal[dwYear];

   return adwNum[0] + adwNum[1] + adwNum[2] + dwFields - 1;
}


/***************************************************************************
CTextParse::CurrencyParse - Parses a string (as far as possible) assuming
it's a date.

inputs
   PWSTR          psz - String, NULL terminated
   WCAHR           *pcSymbol - Currency symbol, such as '$' or 'e'
   int            *paiWhole - Whole number value
   int            *paiCents - Filled with the number of cents, or -1 if no cents
returns
   DWORD - Number of characters used
*/
DWORD CTextParse::CurrencyParse (PWSTR psz, WCHAR *pcSymbol, int *paiWhole, int *paiCents)
{
   // is symbol at beginning?
   WCHAR cSymbol = 0;
   switch (psz[0]) {
   // NOTE: Eventually want euro symbol, etc.
   case L'$':
      cSymbol = psz[0];
      break;
   }

   // look for integer
   DWORD dwWhole;
   int iWhole;
   dwWhole = IntegerParse (psz + (cSymbol ? 1 : 0), &iWhole);
   if (!dwWhole)
      return 0;

   // look for cents
   int iCents = -1;
   DWORD dwCents = 0;
   if (psz[dwWhole + (cSymbol ? 1 : 0)] == L'.') {
      dwCents = NaturalParse (psz + (dwWhole + (cSymbol ? 1 : 0) + 1), FALSE, &iCents);
      if (dwCents != 2)
         return 0;   // error
      // if require 2 digits ok then
   } // cents

   // if no symbol then look for one at end
   if (!cSymbol)
      switch (psz[dwWhole + dwCents + (dwCents ? 1 : 0)]) {
      // NOTE: Eventually want euro symbol, etc.
      case L'$':
         cSymbol = psz[dwWhole + dwCents + (dwCents ? 1 : 0)];
         break;
      }

   // must have some symbol
   if (!cSymbol)
      return 0;

   // else found
   if (pcSymbol)
      *pcSymbol = cSymbol;
   if (paiWhole)
      *paiWhole = iWhole;
   if (paiCents)
      *paiCents = iCents;

   return dwWhole + dwCents + (dwCents ? 1 : 0) + 1 /*symbol*/;
}


/***************************************************************************
CTextParse::NumberInsert - This internal function insers a numnber MML,
<number text="123" context="year"/> into the mml.

inputs
   PWSTR             pszText - Text to insert. NOT necessarily null terminated
   DWORD             dwChars - Number of characters (excluding NULL).
                                 if 0 then do wcslen
   PWSTR             pszContext - Use for the context string
   PCMMLNode2         pNode - Node to insert into
   DWORD             dwInsert - Insert before this one
returns
   BOOL - TRUE if succes, FALSE if error
*/
BOOL CTextParse::NumberInsert (PWSTR pszText, DWORD dwChars, PWSTR pszContext,
                               PCMMLNode2 pNode, DWORD dwInsert)
{
   WCHAR szTemp[64];
   if (!dwChars)
      dwChars = (DWORD)wcslen(pszText);
   dwChars = min(dwChars, sizeof(szTemp)/sizeof(WCHAR)-1);
   memcpy (szTemp, pszText, dwChars * sizeof(WCHAR));
   szTemp[dwChars] = 0;

   PCMMLNode2 pSub;
   pSub = new CMMLNode2;
   if (!pSub)
      return FALSE;
   pSub->NameSet (Number());
   pSub->AttribSetString (Text(), szTemp);
   if (pszContext)
      pSub->AttribSetString (Context(), pszContext);
   pNode->ContentInsert (dwInsert, pSub);

   return TRUE;
}

/***************************************************************************
CTextParse::WordInsert - This internal function insers a numnber MML,
<word text="hello"/> into the mml.

inputs
   PWSTR             pszText - Text to insert. NOT necessarily null terminated
   DWORD             dwChars - Number of characters (excluding NULL).
                                 if 0 then do wcslen
   PCMMLNode2         pNode - Node to insert into
   DWORD             dwInsert - Insert before this one
returns
   BOOL - TRUE if succes, FALSE if error
*/
BOOL CTextParse::WordInsert (PWSTR pszText, DWORD dwChars, PCMMLNode2 pNode, DWORD dwInsert)
{
   WCHAR szTemp[64];
   if (!dwChars)
      dwChars = (DWORD)wcslen(pszText);
   dwChars = min(dwChars, sizeof(szTemp)/sizeof(WCHAR)-1);
   memcpy (szTemp, pszText, dwChars * sizeof(WCHAR));
   szTemp[dwChars] = 0;

   PCMMLNode2 pSub;
   pSub = new CMMLNode2;
   if (!pSub)
      return FALSE;
   pSub->NameSet (Word());
   pSub->AttribSetString (Text(), szTemp);
   pNode->ContentInsert (dwInsert, pSub);

   return TRUE;
}



/***************************************************************************
CTextParse::PunctuationInsert - This internal function insers a numnber MML,
<punt text=","/> into the mml.

inputs
   WCAHR             cPunct - Punctuation
   PCMMLNode2         pNode - Node to insert into
   DWORD             dwInsert - Insert before this one
returns
   BOOL - TRUE if succes, FALSE if error
*/
BOOL CTextParse::PunctuationInsert (WCHAR cPunct, PCMMLNode2 pNode, DWORD dwInsert)
{
   WCHAR szTemp[2];
   szTemp[0] = cPunct;
   szTemp[1] = 0;

   PCMMLNode2 pSub;
   pSub = new CMMLNode2;
   if (!pSub)
      return FALSE;
   pSub->NameSet (Punctuation());
   pSub->AttribSetString (Text(), szTemp);
   pNode->ContentInsert (dwInsert, pSub);

   return TRUE;
}

/***************************************************************************
CTextParse::ParseContainNumber - This checks to see if the parse contains a
digit of any sort. If it does then this is split into multiple parses.

inputs
   PWSTR          psz - String to analyze
   PCMMLNode2      pNode - Parse node of the string
   DWORD          dwNode - Node within pNode, which is the string. If any new
                  strings are to be added then insert them AFTER dwNode so
                  they get parsed as a result.
returns
   BOOL - TRUE if had number and was split up, FALSE if no number and should continue
*/
BOOL CTextParse::ParseContainNumber (PWSTR psz, PCMMLNode2 pNode, DWORD dwNode)
{
   // see if there's a digit
   DWORD i;
   for (i = 0; psz[i]; i++)
      if ((psz[i] >= L'0') && (psz[i] <= L'9'))
         break;
   if (!psz[i])
      return FALSE;  // nothing

   // if get here, contains a digit

   // see which one works best...
   DWORD dwNumFloat, dwNumInteger, dwNumCurrency, dwNumDate, dwNumTime;
   DWORD dwMax;
   fp fFloat;
   WCHAR cSymbol;
   int iInt, iDay, iMonth, iYear, iWhole, iCents, iHour, iMinute;
   dwNumFloat = FloatParse (psz, &fFloat, NULL);
   dwNumInteger = IntegerParse (psz, &iInt);
   dwNumCurrency = CurrencyParse (psz, &cSymbol, &iWhole, &iCents);
   dwNumDate = DateParse (psz, &iDay, &iMonth, &iYear);
   dwNumTime = TimeParse (psz, &iHour, &iMinute);
   dwMax = max(max(max(max(dwNumFloat, dwNumInteger), dwNumCurrency), dwNumDate), dwNumTime);

   // if cant parse anything then split up after the first character and repeat
   // until eventually get something that parses
   if (!dwMax) {
      if (psz[1])
         pNode->ContentInsert (dwNode+1, psz+1);
      psz[1] = 0; // technically shouldnt do, but OK since will delete next
      pNode->ContentInsert (dwNode+1, psz);
      return TRUE;
   }

   // append whatever doesn't get parsed
   if (psz[dwMax])
      pNode->ContentInsert (dwNode+1, psz + dwMax);

   WCHAR szTemp[32];
   if (dwNumDate == dwMax) {
      // write out the date
      // put in the year
      _itow (iYear, szTemp, 10);
      NumberInsert (szTemp, 0, gpszYear, pNode, dwNode+1);

      // put in the date
      if (iDay != -1) {
         // insert a comma
         PunctuationInsert (L',', pNode, dwNode+1);

         // insert the date
         _itow (iDay, szTemp, 10);
         NumberInsert (szTemp, 0, gpszOrdinal, pNode, dwNode+1);
      }

      // insert the month
      PWSTR aszMonthEnglish[12] = {
         L"January", L"February", L"March", L"April",
         L"May", L"June", L"July", L"August",
         L"September", L"October", L"November", L"December"};
      pNode->ContentInsert (dwNode+1, aszMonthEnglish[iMonth-1]);
   }
   else if (dwNumCurrency == dwMax) {
      // if cents then add
      if ((iCents != -1) && iCents) {
         // BUGFIX - If iCents==0 then dont speak
         pNode->ContentInsert (dwNode+1, (iCents == 1) ? L"cent" : L"cents");
         _itow (iCents, szTemp, 10);
         NumberInsert (szTemp, 0, NULL, pNode, dwNode+1);
         pNode->ContentInsert (dwNode+1, L"and");
      }

      // currency
      switch (cSymbol) {
      case L'$':
         pNode->ContentInsert (dwNode+1, (iWhole == 1) ? L"dollar" : L"dollars");
         break;
      default:
         // dont know
         break;
      }

      // whole value
      _itow (iWhole, szTemp, 10);
      NumberInsert (szTemp, 0, NULL, pNode, dwNode+1);
   }
   else if (dwNumTime == dwMax) {
      if (iMinute) {
         _itow (iMinute, szTemp, 10);
         NumberInsert (szTemp, 0, gpszMinute, pNode, dwNode+1);

         PunctuationInsert (L',', pNode, dwNode+1);
      }
      else
         pNode->ContentInsert (dwNode+1, L"o'clock");
      
      // time
      _itow (iHour, szTemp, 10);
      NumberInsert (szTemp, 0, NULL, pNode, dwNode+1);
   }
   else if ((dwNumInteger == dwMax) || (dwNumFloat == dwMax)) {
      // put the number there...
      NumberInsert (psz, dwMax, NULL, pNode, dwNode+1);
   }

   // done
   return TRUE;
}



/***************************************************************************
CTextParse::ParseNumberToWords - Internal function that takes a <number text=123/>
field and converts this to words.

inputs
   PWSTR          psz - String to analyze
   PCMMLNode2      pNode - Parse node of the string
   DWORD          dwNode - Node within pNode, which is the string. If any new
                  strings are to be added then insert them AFTER dwNode so
                  they get parsed as a result.
returns
   BOOL - TRUE if OK, FALSE if error
*/
BOOL CTextParse::ParseNumberToWords (PWSTR psz, PCMMLNode2 pNode, DWORD dwNode)
{
   // figure out the context
   PCMMLNode2 pSub;
   PWSTR pszContext;
   pSub = NULL;
   pszContext = NULL;
   pNode->ContentEnum (dwNode, &pszContext, &pSub);
   if (!pSub)
      return FALSE;
   pszContext = pSub->AttribGetString (Context());

   // convert this to a float/int
   DWORD dwNumFloat, dwNumInt, dwDecStart;
   BOOL fFloat;
   fp fVal;
   int iVal;
   dwNumFloat = FloatParse (psz, &fVal, &dwDecStart);
   dwNumInt = IntegerParse (psz, &iVal);
   if (!dwNumFloat && !dwNumInt)
      return FALSE;  // shouldnt happen
   if (dwNumInt >= dwNumFloat)
      fFloat = FALSE;
   else
      fFloat = TRUE;

   // negative?
   BOOL fNegative = FALSE;
   if (fFloat && (fVal < 0)) {
      fNegative = TRUE;
      fVal *= -1;
   }
   else if (!fFloat && (iVal < 0)) {
      fNegative = TRUE;
      iVal *= -1;
   }

   // float vs. int
   if (fFloat) {
      // all the digits
      DWORD dwLen = (DWORD)wcslen(psz);
      DWORD i;
      for (i = dwLen-1; (i < dwLen) && (i >= dwDecStart); i--)
         WordInsert (Digit (psz[i] - L'0', TRUE, FALSE), NULL, pNode, dwNode+1);

      // point
      WordInsert (L"point", 0, pNode, dwNode+1);

      // add in all the fractional bits...
      ParseNaturalToWords ((int)fVal, NULL, pNode, dwNode);
   }
   else
      ParseNaturalToWords (iVal, pszContext, pNode, dwNode);


   // negative
   if (fNegative)
      WordInsert (L"negative", 0, pNode, dwNode+1);

   return TRUE;
}


/***************************************************************************
CTextParse::Digit - Returns the string used for a number between 0 and 9.

inputs
   DWORD       dwValue
   BOOL        fFormal - If TRUE use formal "zero" else informal "oh"
   BOOL        fOrdinal - If TRUE then use ordinal
returns
   PWSTR - String
*/
PWSTR CTextParse::Digit (DWORD dwValue, BOOL fFormal, BOOL fOrdinal)
{
   PWSTR apszEnglishFormal[10] = {
      L"zero", L"one", L"two", L"three", L"four",
      L"five", L"six", L"seven", L"eight", L"nine"};
   PWSTR apszEnglishInformal[10] = {
      L"oh", L"one", L"two", L"three", L"four",
      L"five", L"six", L"seven", L"eight", L"nine"};
   PWSTR apszEnglishOrdinal[10] = {
      L"zeroeth", L"first", L"second", L"third", L"fourth",
      L"fifth", L"sixth", L"seventh", L"eighth", L"ninth"};

   if (fOrdinal)
      return apszEnglishOrdinal[dwValue];

   return fFormal ? apszEnglishFormal[dwValue] : apszEnglishInformal[dwValue];
}


/***************************************************************************
CTextParse::ParseTwoDigitsToWords - Fills in the string for numbers between 0 and 99.

inputs
   int            iVal - Value, from 0 to 99
   BOOL           fOrdinal - If true then ordinal number
   BOOL           fInsertOh - If TRUE and the number is < 10, then will insert
                  and "Oh" before the digit.
   PCMMLNode2      pNode - Parse node of the string
   DWORD          dwNode - Node within pNode, which is the string. If any new
                  strings are to be added then insert them AFTER dwNode so
                  they get parsed as a result.
returns
   BOOL - TRUE if OK, FALSE if error
*/
BOOL CTextParse::ParseTwoDigitsToWords (int iVal, BOOL fOrdinal, BOOL fInsertOh,
                                        PCMMLNode2 pNode, DWORD dwNode)
{
   // if only on digit easy
   if (iVal < 10) {
      if (fInsertOh) {
         WordInsert (Digit(0, FALSE, FALSE), 0, pNode, dwNode+1);
         dwNode++;
      }

      WordInsert (Digit ((DWORD)iVal, TRUE, fOrdinal), 0, pNode, dwNode+1);
      return TRUE;
   }

   // if it's a teen then special case
   if (iVal < 20) {
      PWSTR apszEnglishTeen[10] = {
         L"ten", L"eleven", L"twelve", L"thirteen", L"fourteen",
         L"fifteen", L"sixteen", L"seventeen", L"eighteen", L"nineteen"};
      PWSTR apszEnglishTeenOrdinal[10] = {
         L"tenth", L"eleventh", L"twelfth", L"thirteenth", L"fourteenth",
         L"fifteenth", L"sixteenth", L"seventeenth", L"eighteenth", L"nineteenth"};

      WordInsert (fOrdinal ? apszEnglishTeenOrdinal[iVal-10] : apszEnglishTeen[iVal-10],
         0, pNode, dwNode+1);
      return TRUE;
   }

   // digits
   PWSTR apszEnglishTens[9] = {
      L"ten", L"twenty", L"thirty", L"forty", L"fifty",
      L"sixty", L"seventy", L"eighty", L"ninety"};
   PWSTR apszEnglishTensOrdinal[9] = {
      L"tenth", L"twentieth", L"thirtieth", L"fortieth", L"fiftieth",
      L"sixtieth", L"seventieth", L"eightieth", L"ninetieth"};

   // single digit
   if (iVal % 10)
      WordInsert (Digit ((DWORD)iVal%10, FALSE, fOrdinal), 0, pNode, dwNode+1);

   // tens digit
   WordInsert ((fOrdinal && ((iVal%10) == 0)) ?
         apszEnglishTensOrdinal[iVal/10 - 1] :
         apszEnglishTens[iVal/10-1],
      0, pNode, dwNode+1);

   return TRUE;
}


/***************************************************************************
CTextParse::ParseThreeDigitsToWords - Fills in the string for numbers between 0 and 999.

inputs
   int            iVal - Value, from 0 to 999
   BOOL           fOrdinal - If true then ordinal number
   PCMMLNode2      pNode - Parse node of the string
   DWORD          dwNode - Node within pNode, which is the string. If any new
                  strings are to be added then insert them AFTER dwNode so
                  they get parsed as a result.
returns
   BOOL - TRUE if OK, FALSE if error
*/
BOOL CTextParse::ParseThreeDigitsToWords (int iVal, BOOL fOrdinal,
                                        PCMMLNode2 pNode, DWORD dwNode)
{
   if (iVal < 100)
      return ParseTwoDigitsToWords (iVal, fOrdinal, FALSE, pNode, dwNode);

   // put in the last digits
   if (iVal % 100) {
      ParseTwoDigitsToWords (iVal % 100, fOrdinal, FALSE, pNode, dwNode);
      fOrdinal = FALSE;
   }

   // hundred...
   WordInsert (fOrdinal ? L"hundreth" : L"hundred", 0, pNode, dwNode+1);

   // number in front
   WordInsert (Digit ((DWORD)iVal/100, TRUE, FALSE), 0, pNode, dwNode+1);

   return TRUE;
}


/***************************************************************************
CTextParse::ParseSixDigitsToWords - Fills in the string for numbers between 0 and 999,999.

inputs
   int            iVal - Value, from 0 to 999,999
   BOOL           fOrdinal - If true then ordinal number
   PCMMLNode2      pNode - Parse node of the string
   DWORD          dwNode - Node within pNode, which is the string. If any new
                  strings are to be added then insert them AFTER dwNode so
                  they get parsed as a result.
returns
   BOOL - TRUE if OK, FALSE if error
*/
BOOL CTextParse::ParseSixDigitsToWords (int iVal, BOOL fOrdinal,
                                        PCMMLNode2 pNode, DWORD dwNode)
{
   if (iVal < 1000)
      return ParseThreeDigitsToWords (iVal, fOrdinal, pNode, dwNode);

   // put in the last digits
   if (iVal % 1000) {
      ParseThreeDigitsToWords (iVal % 1000, fOrdinal, pNode, dwNode);
      fOrdinal = FALSE;

      PunctuationInsert (L',', pNode, dwNode+1);
   }

   // thousand...
   WordInsert (fOrdinal ? L"thousandth" : L"thousand", 0, pNode, dwNode+1);

   return ParseThreeDigitsToWords (iVal / 1000, FALSE, pNode, dwNode);
}


/***************************************************************************
CTextParse::ParseNineDigitsToWords - Fills in the string for numbers between 0 and 999,999,999.

inputs
   int            iVal - Value, from 0 to 999,999,999
   BOOL           fOrdinal - If true then ordinal number
   PCMMLNode2      pNode - Parse node of the string
   DWORD          dwNode - Node within pNode, which is the string. If any new
                  strings are to be added then insert them AFTER dwNode so
                  they get parsed as a result.
returns
   BOOL - TRUE if OK, FALSE if error
*/
BOOL CTextParse::ParseNineDigitsToWords (int iVal, BOOL fOrdinal,
                                        PCMMLNode2 pNode, DWORD dwNode)
{
   if (iVal < 1000000)
      return ParseSixDigitsToWords (iVal, fOrdinal, pNode, dwNode);

   // put in the last digits
   if (iVal % 1000000) {
      ParseSixDigitsToWords (iVal % 1000000, fOrdinal, pNode, dwNode);
      fOrdinal = FALSE;

      PunctuationInsert (L',', pNode, dwNode+1);
   }

   // thousand...
   WordInsert (fOrdinal ? L"millionth" : L"million", 0, pNode, dwNode+1);

   return ParseThreeDigitsToWords (iVal / 1000000, FALSE, pNode, dwNode);
}


/***************************************************************************
CTextParse::ParseTwelveDigitsToWords - Fills in the string for numbers between 0 and 999,999,999,999.

inputs
   int            iVal - Value, from 0 to 999,999,999,999
   BOOL           fOrdinal - If true then ordinal number
   PCMMLNode2      pNode - Parse node of the string
   DWORD          dwNode - Node within pNode, which is the string. If any new
                  strings are to be added then insert them AFTER dwNode so
                  they get parsed as a result.
returns
   BOOL - TRUE if OK, FALSE if error
*/
BOOL CTextParse::ParseTwelveDigitsToWords (int iVal, BOOL fOrdinal,
                                        PCMMLNode2 pNode, DWORD dwNode)
{
   if (iVal < 1000000000)
      return ParseNineDigitsToWords (iVal, fOrdinal, pNode, dwNode);

   // put in the last digits
   if (iVal % 1000000000) {
      ParseNineDigitsToWords (iVal % 1000000000, fOrdinal, pNode, dwNode);
      fOrdinal = FALSE;

      PunctuationInsert (L',', pNode, dwNode+1);
   }

   // thousand...
   WordInsert (fOrdinal ? L"billionth" : L"billion", 0, pNode, dwNode+1);

   return ParseThreeDigitsToWords ((iVal / 1000000000)%1000, FALSE, pNode, dwNode);
}


/***************************************************************************
CTextParse::ParseNaturalToWords - Internal function that takes a 0+ integer
and writes out the words for it.

inputs
   int            iVal - Value
   PWSTR          pszContext - Context. Might be null
   PCMMLNode2      pNode - Parse node of the string
   DWORD          dwNode - Node within pNode, which is the string. If any new
                  strings are to be added then insert them AFTER dwNode so
                  they get parsed as a result.
returns
   BOOL - TRUE if OK, FALSE if error
*/
BOOL CTextParse::ParseNaturalToWords (int iVal, PWSTR pszContext, PCMMLNode2 pNode, DWORD dwNode)
{
   // special for minutes
   if (pszContext && !_wcsicmp(pszContext, gpszMinute))
      return ParseTwoDigitsToWords (iVal % 100, FALSE, TRUE, pNode, dwNode);

   // special for years, 1904 -> "nineteen oh four"
   if (pszContext && !_wcsicmp(pszContext, gpszYear) && (iVal >= 100) && (iVal <= 9999) &&
      (iVal % 1000)) {
      // lower two digits
      if (iVal % 100)
         ParseTwoDigitsToWords (iVal % 100, FALSE, TRUE, pNode, dwNode);
      else
         WordInsert (L"hundred", 0, pNode, dwNode+1);

      // upper two digits
      ParseTwoDigitsToWords (iVal / 100, FALSE, TRUE, pNode, dwNode);
      return TRUE;
   }

   // if ordinal?
   BOOL fOrdinal = FALSE;
   if (pszContext && !_wcsicmp(pszContext, gpszOrdinal))
      fOrdinal = TRUE;

   // else, if get here, normal parse
   return ParseTwelveDigitsToWords (iVal, fOrdinal, pNode, dwNode);
}
